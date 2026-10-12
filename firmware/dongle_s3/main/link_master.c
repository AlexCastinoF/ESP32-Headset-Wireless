#include "link_master.h"

#include "hs_link.h"
#include "usb_audio.h"

#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "tusb.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

#include <string.h>

static const char *TAG = "hs_link";

#define CHUNK_SLOTS 8

typedef struct {
    uint16_t seq;
    int16_t samples[HS_MIC_SAMPLES];
    bool valid;
} mic_slot_t;

static hs_hid_config_t desired;
static hs_hid_config_t active;
static uint8_t acked_id;
static uint8_t desired_apply_id;
static bool ack_ok = true;
static uint8_t peer[6];
static bool peer_ready;
static int8_t last_rssi = -100;
static uint8_t battery;
static uint8_t remote_flags;
static uint32_t tx_chunks;
static uint32_t lost_chunks;
static uint16_t loss_permille;
static uint16_t expect_up = 1;
static bool have_expect;
static int64_t last_up_us;
static portMUX_TYPE state_mux = portMUX_INITIALIZER_UNLOCKED;

static QueueHandle_t up_queue;
static QueueHandle_t ack_queue;
static uint8_t pending_peer[6];
static volatile bool pending_peer_valid;
static mic_slot_t mic_slots[CHUNK_SLOTS];
static uint16_t play_seq;
static bool play_started;

static bool mac_ready(const uint8_t mac[6]) {
    bool nonzero = false;
    bool not_broadcast = false;
    for (int i = 0; i < 6; i++) {
        if (mac[i] != 0) {
            nonzero = true;
        }
        if (mac[i] != 0xFF) {
            not_broadcast = true;
        }
    }
    return nonzero && not_broadcast;
}

static void remember_peer(const uint8_t mac[6], bool persist) {
    if (!mac_ready(mac)) {
        return;
    }
    if (peer_ready && memcmp(peer, mac, 6) == 0) {
        return;
    }
    if (peer_ready) {
        esp_now_del_peer(peer);
    }
    memcpy(peer, mac, 6);
    esp_now_peer_info_t info = {0};
    memcpy(info.peer_addr, peer, 6);
    info.channel = active.channel;
    info.ifidx = WIFI_IF_STA;
    info.encrypt = false;
    if (esp_now_add_peer(&info) == ESP_OK) {
        peer_ready = true;
    }
    if (persist) {
        nvs_handle_t nvs;
        if (nvs_open("hs", NVS_READWRITE, &nvs) == ESP_OK) {
            nvs_set_blob(nvs, "peer", peer, 6);
            nvs_commit(nvs);
            nvs_close(nvs);
        }
    }
}

static void load_nvs(void) {
    desired.frame_ms = 5;
    desired.jitter_frames = 2;
    desired.channel = 6;
    desired.tx_power_qdbm = 60;
    desired.speaker_db = 0;
    desired.mic_db = 0;
    desired.sidetone_db = -60;
    desired.mic_boost_db = 0;
    desired.profile = HS_PROFILE_BALANCE;
    active = desired;
    nvs_handle_t nvs;
    if (nvs_open("hs", NVS_READONLY, &nvs) != ESP_OK) {
        return;
    }
    size_t len = 6;
    nvs_get_blob(nvs, "peer", peer, &len);
    nvs_get_u8(nvs, "frame", &desired.frame_ms);
    nvs_get_u8(nvs, "jitter", &desired.jitter_frames);
    nvs_get_u8(nvs, "channel", &desired.channel);
    nvs_get_u8(nvs, "power", &desired.tx_power_qdbm);
    nvs_get_i8(nvs, "spk", &desired.speaker_db);
    nvs_get_i8(nvs, "mic", &desired.mic_db);
    nvs_get_i8(nvs, "side", &desired.sidetone_db);
    nvs_get_u8(nvs, "boost", &desired.mic_boost_db);
    nvs_get_u8(nvs, "profile", &desired.profile);
    nvs_close(nvs);
    hs_profile_apply(desired.profile, &desired.frame_ms, &desired.jitter_frames);
    if (!hs_transport_valid(desired.frame_ms, desired.jitter_frames, desired.channel)) {
        desired.frame_ms = 5;
        desired.jitter_frames = 2;
        desired.channel = 6;
        desired.profile = HS_PROFILE_BALANCE;
    }
    desired.speaker_db = hs_clamp_db(desired.speaker_db, -60, 0);
    desired.mic_db = hs_clamp_db(desired.mic_db, -60, 0);
    desired.sidetone_db = hs_clamp_db(desired.sidetone_db, -60, 0);
    if (desired.mic_boost_db > 24) {
        desired.mic_boost_db = 24;
    }
    if (desired.tx_power_qdbm < 8 || desired.tx_power_qdbm > 80) {
        desired.tx_power_qdbm = 60;
    }
    active = desired;
    peer_ready = false;
}

static void save_active(void) {
    nvs_handle_t nvs;
    if (nvs_open("hs", NVS_READWRITE, &nvs) != ESP_OK) {
        return;
    }
    nvs_set_u8(nvs, "frame", active.frame_ms);
    nvs_set_u8(nvs, "jitter", active.jitter_frames);
    nvs_set_u8(nvs, "channel", active.channel);
    nvs_set_u8(nvs, "power", active.tx_power_qdbm);
    nvs_set_i8(nvs, "spk", active.speaker_db);
    nvs_set_i8(nvs, "mic", active.mic_db);
    nvs_set_i8(nvs, "side", active.sidetone_db);
    nvs_set_u8(nvs, "boost", active.mic_boost_db);
    nvs_set_u8(nvs, "profile", active.profile);
    if (peer_ready) {
        nvs_set_blob(nvs, "peer", peer, 6);
    }
    nvs_commit(nvs);
    nvs_close(nvs);
}

static void sanitize(hs_hid_config_t *config) {
    if (config->profile > HS_PROFILE_CUSTOM) {
        config->profile = HS_PROFILE_BALANCE;
    }
    hs_profile_apply(config->profile, &config->frame_ms, &config->jitter_frames);
    if (!hs_transport_valid(config->frame_ms, config->jitter_frames, config->channel)) {
        config->frame_ms = active.frame_ms;
        config->jitter_frames = active.jitter_frames;
        config->channel = active.channel;
    }
    config->speaker_db = hs_clamp_db(config->speaker_db, -60, 0);
    config->mic_db = hs_clamp_db(config->mic_db, -60, 0);
    config->sidetone_db = hs_clamp_db(config->sidetone_db, -60, 0);
    if (config->mic_boost_db > 24) {
        config->mic_boost_db = 24;
    }
    if (config->tx_power_qdbm < 8 || config->tx_power_qdbm > 80) {
        config->tx_power_qdbm = 60;
    }
}

void link_master_set_config(const hs_hid_config_t *config) {
    hs_hid_config_t next = *config;
    sanitize(&next);
    portENTER_CRITICAL(&state_mux);
    bool save = (next.flags & HS_FLAG_SAVE) != 0;
    uint8_t flags = next.flags;
    next.flags = (uint8_t)(flags & (HS_FLAG_MUTE | HS_FLAG_TONE));
    desired = next;
    ack_ok = false;
    acked_id = (uint8_t)(acked_id + 1);
    desired_apply_id = acked_id;
    portEXIT_CRITICAL(&state_mux);
    usb_audio_set_tone((flags & HS_FLAG_TONE) != 0);
    if (mac_ready(next.peer_mac)) {
        remember_peer(next.peer_mac, save);
    }
    if (save) {
        active.speaker_db = next.speaker_db;
        active.mic_db = next.mic_db;
        active.sidetone_db = next.sidetone_db;
        active.mic_boost_db = next.mic_boost_db;
        active.tx_power_qdbm = next.tx_power_qdbm;
        esp_wifi_set_max_tx_power(active.tx_power_qdbm);
        save_active();
    }
}

void link_master_get_config(hs_hid_config_t *config) {
    portENTER_CRITICAL(&state_mux);
    *config = desired;
    memcpy(config->peer_mac, peer, 6);
    portEXIT_CRITICAL(&state_mux);
}

void link_master_fill_status(hs_hid_status_t *status) {
    memset(status, 0, sizeof(*status));
    portENTER_CRITICAL(&state_mux);
    hs_hid_config_t shown = ack_ok ? active : desired;
    bool linked = (esp_timer_get_time() - last_up_us) < 300000;
    status->linked = linked ? 1 : 0;
    status->battery = battery;
    status->flags = remote_flags;
    if ((desired.flags & HS_FLAG_TONE) != 0) {
        status->flags = (uint8_t)(status->flags | HS_FLAG_TONE);
    }
    status->rssi = last_rssi;
    status->loss_permille = loss_permille;
    status->rtt_ms = hs_estimate_round_trip_ms(shown.frame_ms, shown.jitter_frames);
    status->frame_ms = shown.frame_ms;
    status->jitter_frames = shown.jitter_frames;
    status->channel = shown.channel;
    status->tx_power_qdbm = shown.tx_power_qdbm;
    status->speaker_db = shown.speaker_db;
    status->mic_db = shown.mic_db;
    status->sidetone_db = shown.sidetone_db;
    status->mic_boost_db = shown.mic_boost_db;
    status->profile = shown.profile;
    status->apply_id = desired_apply_id;
    memcpy(status->peer_mac, peer, 6);
    portEXIT_CRITICAL(&state_mux);
    strncpy(status->version, HS_LINK_VERSION, sizeof(status->version) - 1);
}

void link_master_send_hid(void) {
    static int64_t last_us;
    int64_t now = esp_timer_get_time();
    if (now - last_us < 100000 || !tud_mounted() || !tud_hid_ready()) {
        return;
    }
    last_us = now;
    hs_hid_status_t status;
    link_master_fill_status(&status);
    tud_hid_report(HS_HID_REPORT_STATUS, &status, sizeof(status));
}

static void apply_acked(const hs_packet_ack_t *ack) {
    portENTER_CRITICAL(&state_mux);
    if (ack->apply_id != desired_apply_id) {
        portEXIT_CRITICAL(&state_mux);
        return;
    }
    bool channel_changed = active.channel != ack->channel && ack->channel == desired.channel;
    active.frame_ms = desired.frame_ms;
    active.jitter_frames = desired.jitter_frames;
    active.profile = desired.profile;
    active.channel = desired.channel;
    ack_ok = true;
    uint8_t channel = active.channel;
    portEXIT_CRITICAL(&state_mux);
    if (channel_changed) {
        vTaskDelay(pdMS_TO_TICKS(20));
        esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    }
    save_active();
}

static void on_uplink(const hs_packet_up_t *packet) {
    uint16_t seq = packet->seq;
    if (have_expect) {
        uint16_t gap = (uint16_t)(seq - expect_up);
        if (gap > 0 && gap < 50) {
            lost_chunks += gap;
        }
    }
    expect_up = (uint16_t)(seq + 1);
    have_expect = true;
    tx_chunks++;
    mic_slot_t *slot = &mic_slots[seq % CHUNK_SLOTS];
    slot->seq = seq;
    memcpy(slot->samples, packet->mic, sizeof(slot->samples));
    slot->valid = true;
    last_up_us = esp_timer_get_time();
    battery = packet->battery;
    remote_flags = packet->flags;
}

static void recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (info && info->src_addr && !peer_ready) {
        memcpy(pending_peer, info->src_addr, 6);
        pending_peer_valid = true;
    }
    if (info && info->rx_ctrl) {
        last_rssi = info->rx_ctrl->rssi;
    }
    if (len == (int)sizeof(hs_packet_up_t) && data[2] == HS_TYPE_UP && up_queue) {
        hs_packet_up_t packet;
        memcpy(&packet, data, sizeof(packet));
        xQueueSend(up_queue, &packet, 0);
        return;
    }
    if (len == (int)sizeof(hs_packet_ack_t) && data[2] == HS_TYPE_ACK && ack_queue) {
        hs_packet_ack_t ack;
        memcpy(&ack, data, sizeof(ack));
        xQueueSend(ack_queue, &ack, 0);
    }
}

static void send_config(uint16_t seq) {
    if (!peer_ready) {
        return;
    }
    hs_packet_cfg_t packet = {0};
    portENTER_CRITICAL(&state_mux);
    packet.seq = seq;
    packet.type = HS_TYPE_CFG;
    packet.flags = desired.flags;
    packet.frame_ms = desired.frame_ms;
    packet.jitter_frames = desired.jitter_frames;
    packet.channel = desired.channel;
    packet.tx_power_qdbm = desired.tx_power_qdbm;
    packet.speaker_db = desired.speaker_db;
    packet.mic_db = desired.mic_db;
    packet.sidetone_db = desired.sidetone_db;
    packet.mic_boost_db = desired.mic_boost_db;
    packet.apply_id = desired_apply_id;
    packet.profile = desired.profile;
    bool need = !ack_ok;
    portEXIT_CRITICAL(&state_mux);
    if (need) {
        esp_now_send(peer, (const uint8_t *)&packet, sizeof(packet));
    }
}

static void release_mic(void) {
    uint8_t need = hs_jitter_chunks(active.frame_ms, active.jitter_frames);
    uint8_t have = 0;
    for (int i = 0; i < CHUNK_SLOTS; i++) {
        if (mic_slots[i].valid) {
            have++;
        }
    }
    if (!play_started) {
        if (have < need) {
            return;
        }
        play_started = true;
        bool found = false;
        for (int i = 0; i < CHUNK_SLOTS; i++) {
            if (!mic_slots[i].valid) {
                continue;
            }
            if (!found || (int16_t)(mic_slots[i].seq - play_seq) < 0) {
                play_seq = mic_slots[i].seq;
                found = true;
            }
        }
    }
    mic_slot_t *slot = &mic_slots[play_seq % CHUNK_SLOTS];
    int16_t samples[HS_MIC_SAMPLES];
    if (slot->valid && slot->seq == play_seq) {
        memcpy(samples, slot->samples, sizeof(samples));
        slot->valid = false;
    } else {
        memset(samples, 0, sizeof(samples));
    }
    usb_audio_push_mic(samples, HS_MIC_SAMPLES);
    play_seq++;
}

static void radio_task(void *arg) {
    (void)arg;
    uint16_t seq = 0;
    hs_adpcm_state_t left = {0, 0};
    hs_adpcm_state_t right = {0, 0};
    TickType_t next = xTaskGetTickCount();
    int64_t last_cfg = 0;
    int64_t last_loss = esp_timer_get_time();
    while (true) {
        uint8_t frame_ms = active.frame_ms;
        vTaskDelayUntil(&next, pdMS_TO_TICKS(frame_ms));
        int chunks = frame_ms / 5;
        if (chunks < 1) {
            chunks = 1;
        }
        for (int i = 0; i < chunks; i++) {
            int16_t pcm[HS_CHUNK_FRAMES * 2];
            usb_audio_pull_speaker(pcm, HS_CHUNK_FRAMES);
            if (active.speaker_db != 0) {
                hs_apply_gain(pcm, HS_CHUNK_FRAMES * 2, active.speaker_db);
            }
            hs_packet_down_t down = {0};
            down.seq = seq;
            down.type = HS_TYPE_DOWN;
            down.pred_l = left.predictor;
            down.pred_r = right.predictor;
            down.index_l = (uint8_t)left.index;
            down.index_r = (uint8_t)right.index;
            hs_adpcm_encode_stereo(&left, &right, pcm, down.audio, HS_CHUNK_FRAMES);
            if (peer_ready) {
                esp_now_send(peer, (const uint8_t *)&down, sizeof(down));
            }
            seq++;
            release_mic();
        }
        if (pending_peer_valid) {
            uint8_t mac[6];
            memcpy(mac, pending_peer, 6);
            pending_peer_valid = false;
            remember_peer(mac, true);
        }
        hs_packet_up_t up;
        while (xQueueReceive(up_queue, &up, 0) == pdTRUE) {
            on_uplink(&up);
        }
        hs_packet_ack_t ack;
        while (xQueueReceive(ack_queue, &ack, 0) == pdTRUE) {
            apply_acked(&ack);
        }
        int64_t now = esp_timer_get_time();
        if (now - last_cfg > 200000) {
            last_cfg = now;
            send_config(seq);
        }
        if (now - last_loss > 1000000) {
            uint32_t sent = tx_chunks;
            uint32_t total = sent + lost_chunks;
            loss_permille = total == 0 ? 0 : (uint16_t)((lost_chunks * 1000u) / total);
            if (loss_permille > 1000) {
                loss_permille = 1000;
            }
            tx_chunks = 0;
            lost_chunks = 0;
            last_loss = now;
        }
    }
}

static void wifi_start(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N));
    ESP_ERROR_CHECK(esp_wifi_set_channel(active.channel, WIFI_SECOND_CHAN_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(active.tx_power_qdbm));
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(recv_cb));
    if (mac_ready(peer)) {
        remember_peer(peer, false);
    }
    ESP_LOGI(TAG, "dongle no canal %u, quadro %u ms, buffer %u", active.channel, active.frame_ms, active.jitter_frames);
}

void link_master_start(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    load_nvs();
    up_queue = xQueueCreate(12, sizeof(hs_packet_up_t));
    ack_queue = xQueueCreate(4, sizeof(hs_packet_ack_t));
    wifi_start();
    xTaskCreatePinnedToCore(radio_task, "hs_radio", 8192, NULL, 7, NULL, 0);
}
