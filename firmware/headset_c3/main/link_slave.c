#include "link_slave.h"

#include "audio_i2s.h"
#include "hs_link.h"

#include "driver/gpio.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

#include <string.h>

#ifndef ADC_ATTEN_DB_12
#define ADC_ATTEN_DB_12 ADC_ATTEN_DB_11
#endif

static const char *TAG = "hs_c3";

#define PLAY_SLOTS 12

typedef struct {
    uint16_t seq;
    int16_t pcm[HS_CHUNK_FRAMES * 2];
    bool valid;
} play_slot_t;

static hs_hid_config_t cfg;
static uint8_t peer[6];
static bool peer_ready;
static uint8_t pending_peer[6];
static volatile bool pending_peer_valid;
static QueueHandle_t down_queue;
static uint8_t channel_before_switch;
static bool channel_waiting;
static int64_t channel_switch_us;
static play_slot_t slots[PLAY_SLOTS];
static uint16_t play_seq;
static bool play_started;
static int misses;
static uint8_t battery = 100;
static bool mic_mute;
static int mute_stable;
static adc_oneshot_unit_handle_t adc;
static adc_cali_handle_t cali;
static int64_t last_down_us;

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
    info.channel = cfg.channel;
    info.ifidx = WIFI_IF_STA;
    if (esp_now_add_peer(&info) == ESP_OK) {
        peer_ready = true;
    }
    if (!persist) {
        return;
    }
    nvs_handle_t nvs;
    if (nvs_open("hs", NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_blob(nvs, "peer", peer, 6);
        nvs_commit(nvs);
        nvs_close(nvs);
    }
}

static void save_cfg(void) {
    nvs_handle_t nvs;
    if (nvs_open("hs", NVS_READWRITE, &nvs) != ESP_OK) {
        return;
    }
    nvs_set_u8(nvs, "frame", cfg.frame_ms);
    nvs_set_u8(nvs, "jitter", cfg.jitter_frames);
    nvs_set_u8(nvs, "channel", cfg.channel);
    nvs_set_u8(nvs, "power", cfg.tx_power_qdbm);
    nvs_set_i8(nvs, "spk", cfg.speaker_db);
    nvs_set_i8(nvs, "mic", cfg.mic_db);
    nvs_set_i8(nvs, "side", cfg.sidetone_db);
    nvs_set_u8(nvs, "boost", cfg.mic_boost_db);
    nvs_set_u8(nvs, "profile", cfg.profile);
    nvs_commit(nvs);
    nvs_close(nvs);
}

static void load_cfg(void) {
    cfg.frame_ms = 5;
    cfg.jitter_frames = 2;
    cfg.channel = 6;
    cfg.tx_power_qdbm = 60;
    cfg.speaker_db = 0;
    cfg.mic_db = 0;
    cfg.sidetone_db = -60;
    cfg.mic_boost_db = 0;
    cfg.profile = HS_PROFILE_BALANCE;
    nvs_handle_t nvs;
    if (nvs_open("hs", NVS_READONLY, &nvs) != ESP_OK) {
        return;
    }
    size_t len = 6;
    nvs_get_blob(nvs, "peer", peer, &len);
    nvs_get_u8(nvs, "frame", &cfg.frame_ms);
    nvs_get_u8(nvs, "jitter", &cfg.jitter_frames);
    nvs_get_u8(nvs, "channel", &cfg.channel);
    nvs_get_u8(nvs, "power", &cfg.tx_power_qdbm);
    nvs_get_i8(nvs, "spk", &cfg.speaker_db);
    nvs_get_i8(nvs, "mic", &cfg.mic_db);
    nvs_get_i8(nvs, "side", &cfg.sidetone_db);
    nvs_get_u8(nvs, "boost", &cfg.mic_boost_db);
    nvs_get_u8(nvs, "profile", &cfg.profile);
    nvs_close(nvs);
    hs_profile_apply(cfg.profile, &cfg.frame_ms, &cfg.jitter_frames);
    if (!hs_transport_valid(cfg.frame_ms, cfg.jitter_frames, cfg.channel)) {
        cfg.frame_ms = 5;
        cfg.jitter_frames = 2;
        cfg.channel = 6;
    }
}

static void apply_config(const hs_packet_cfg_t *packet) {
    if (!hs_transport_valid(packet->frame_ms, packet->jitter_frames, packet->channel)) {
        return;
    }
    bool channel_changed = cfg.channel != packet->channel;
    uint8_t old_channel = cfg.channel;
    cfg.frame_ms = packet->frame_ms;
    cfg.jitter_frames = packet->jitter_frames;
    cfg.channel = packet->channel;
    cfg.tx_power_qdbm = packet->tx_power_qdbm;
    cfg.speaker_db = hs_clamp_db(packet->speaker_db, -60, 0);
    cfg.mic_db = hs_clamp_db(packet->mic_db, -60, 0);
    cfg.sidetone_db = hs_clamp_db(packet->sidetone_db, -60, 0);
    cfg.mic_boost_db = packet->mic_boost_db > 24 ? 24 : packet->mic_boost_db;
    cfg.profile = packet->profile;
    cfg.flags = packet->flags;
    play_started = false;
    misses = 0;
    esp_wifi_set_max_tx_power(cfg.tx_power_qdbm);
    save_cfg();
    if (peer_ready) {
        hs_packet_ack_t ack = {
            .seq = packet->seq,
            .type = HS_TYPE_ACK,
            .apply_id = packet->apply_id,
            .frame_ms = cfg.frame_ms,
            .jitter_frames = cfg.jitter_frames,
            .profile = cfg.profile,
            .channel = cfg.channel,
        };
        esp_now_send(peer, (const uint8_t *)&ack, sizeof(ack));
    }
    if (channel_changed) {
        vTaskDelay(pdMS_TO_TICKS(20));
        esp_wifi_set_channel(cfg.channel, WIFI_SECOND_CHAN_NONE);
        channel_before_switch = old_channel;
        channel_waiting = true;
        channel_switch_us = esp_timer_get_time();
    }
}

static void confirm_channel(void) {
    channel_waiting = false;
}

static void revert_channel_if_silent(void) {
    if (!channel_waiting) {
        return;
    }
    if (esp_timer_get_time() - channel_switch_us < 1500000) {
        return;
    }
    cfg.channel = channel_before_switch;
    channel_waiting = false;
    esp_wifi_set_channel(cfg.channel, WIFI_SECOND_CHAN_NONE);
    save_cfg();
    ESP_LOGW(TAG, "canal sem resposta, voltou para %u", cfg.channel);
}

static void recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (info && info->src_addr && !peer_ready) {
        memcpy(pending_peer, info->src_addr, 6);
        pending_peer_valid = true;
    }
    if (!down_queue || len <= 0) {
        return;
    }
    if (data[2] != HS_TYPE_DOWN && data[2] != HS_TYPE_CFG) {
        return;
    }
    if (data[2] == HS_TYPE_DOWN && len != (int)sizeof(hs_packet_down_t)) {
        return;
    }
    if (data[2] == HS_TYPE_CFG && len != (int)sizeof(hs_packet_cfg_t)) {
        return;
    }
    hs_packet_down_t packet;
    memset(&packet, 0, sizeof(packet));
    memcpy(&packet, data, (size_t)len);
    xQueueSend(down_queue, &packet, 0);
}

static void decode_down(const hs_packet_down_t *packet) {
    hs_adpcm_state_t left = {.predictor = packet->pred_l, .index = (int8_t)packet->index_l};
    hs_adpcm_state_t right = {.predictor = packet->pred_r, .index = (int8_t)packet->index_r};
    play_slot_t *slot = &slots[packet->seq % PLAY_SLOTS];
    slot->seq = packet->seq;
    hs_adpcm_decode_stereo(&left, &right, packet->audio, slot->pcm, HS_CHUNK_FRAMES);
    slot->valid = true;
    last_down_us = esp_timer_get_time();
}

static void mix_sidetone(int16_t *stereo, const int16_t *left) {
    if (cfg.sidetone_db <= -60) {
        return;
    }
    for (size_t i = 0; i < HS_CHUNK_FRAMES; i++) {
        int16_t side[2] = {left[i], left[i]};
        hs_apply_gain(side, 2, cfg.sidetone_db);
        int32_t l = (int32_t)stereo[i * 2] + side[0];
        int32_t r = (int32_t)stereo[i * 2 + 1] + side[1];
        if (l > 32767) l = 32767;
        if (l < -32768) l = -32768;
        if (r > 32767) r = 32767;
        if (r < -32768) r = -32768;
        stereo[i * 2] = (int16_t)l;
        stereo[i * 2 + 1] = (int16_t)r;
    }
}

static int16_t last_pcm[HS_CHUNK_FRAMES * 2];

static void play_chunk(const int16_t *mic_left) {
    uint8_t need = hs_jitter_chunks(cfg.frame_ms, cfg.jitter_frames);
    uint8_t have = 0;
    for (int i = 0; i < PLAY_SLOTS; i++) {
        if (slots[i].valid) {
            have++;
        }
    }
    int16_t stereo[HS_CHUNK_FRAMES * 2];
    memset(stereo, 0, sizeof(stereo));
    if (!play_started) {
        if (have >= need) {
            play_started = true;
            bool found = false;
            for (int i = 0; i < PLAY_SLOTS; i++) {
                if (!slots[i].valid) {
                    continue;
                }
                if (!found || (int16_t)(slots[i].seq - play_seq) < 0) {
                    play_seq = slots[i].seq;
                    found = true;
                }
            }
        }
    }
    if (play_started) {
        play_slot_t *slot = &slots[play_seq % PLAY_SLOTS];
        if (slot->valid && slot->seq == play_seq) {
            memcpy(stereo, slot->pcm, sizeof(stereo));
            memcpy(last_pcm, stereo, sizeof(stereo));
            slot->valid = false;
            misses = 0;
        } else if (misses < 3) {
            misses++;
            memcpy(stereo, last_pcm, sizeof(stereo));
            hs_apply_gain(stereo, HS_CHUNK_FRAMES * 2, -3);
            memcpy(last_pcm, stereo, sizeof(stereo));
        } else {
            play_started = false;
            memset(last_pcm, 0, sizeof(last_pcm));
        }
        play_seq++;
    }
    mix_sidetone(stereo, mic_left);
    audio_i2s_write_stereo(stereo, HS_CHUNK_FRAMES);
}

static void update_led(void) {
    bool linked = (esp_timer_get_time() - last_down_us) < 300000;
    static int phase;
    phase++;
    int on = 0;
    if (battery < 15) {
        on = (phase / 4) % 2;
    } else if (!linked) {
        on = (phase / 40) % 2;
    } else {
        on = 1;
    }
    gpio_set_level(GPIO_NUM_10, on);
}

static void read_battery(void) {
    if (!adc) {
        return;
    }
    int raw = 0;
    int sum = 0;
    int count = 0;
    for (int i = 0; i < 16; i++) {
        if (adc_oneshot_read(adc, ADC_CHANNEL_0, &raw) == ESP_OK) {
            int mv = 0;
            if (cali && adc_cali_raw_to_voltage(cali, raw, &mv) == ESP_OK) {
                sum += mv;
            } else {
                sum += raw * 3300 / 4095;
            }
            count++;
        }
    }
    if (count > 0) {
        battery = hs_battery_percent((sum / count) * 2);
    }
}

static void read_mute(void) {
    int low = gpio_get_level(GPIO_NUM_6) == 0;
    if (low == mic_mute) {
        mute_stable = 0;
        return;
    }
    if (++mute_stable >= 3) {
        mic_mute = low;
        mute_stable = 0;
    }
}

static void send_uplink(uint16_t seq, const int16_t *mic) {
    if (!peer_ready) {
        return;
    }
    hs_packet_up_t packet = {0};
    packet.seq = seq;
    packet.type = HS_TYPE_UP;
    packet.battery = battery;
    if (mic_mute || (cfg.flags & HS_FLAG_MUTE)) {
        packet.flags = HS_FLAG_MUTE;
    } else {
        memcpy(packet.mic, mic, sizeof(packet.mic));
        int gain = (int)cfg.mic_db + (int)cfg.mic_boost_db;
        if (gain != 0) {
            hs_apply_gain(packet.mic, HS_MIC_SAMPLES, gain);
        }
    }
    esp_now_send(peer, (const uint8_t *)&packet, sizeof(packet));
}

static void audio_task(void *arg) {
    (void)arg;
    int loops = 0;
    while (true) {
        if (pending_peer_valid) {
            uint8_t mac[6];
            memcpy(mac, pending_peer, 6);
            pending_peer_valid = false;
            remember_peer(mac, true);
        }
        int16_t left[HS_CHUNK_FRAMES];
        if (!audio_i2s_read_left(left, HS_CHUNK_FRAMES)) {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        int16_t mic[HS_MIC_SAMPLES];
        hs_decimate_by_3(left, mic, HS_MIC_SAMPLES);
        read_mute();
        hs_packet_down_t down;
        while (xQueueReceive(down_queue, &down, 0) == pdTRUE) {
            if (down.type == HS_TYPE_CFG) {
                hs_packet_cfg_t incoming;
                memcpy(&incoming, &down, sizeof(incoming));
                apply_config(&incoming);
            } else if (down.type == HS_TYPE_DOWN) {
                decode_down(&down);
                confirm_channel();
                send_uplink(down.seq, mic);
            }
        }
        play_chunk(left);
        revert_channel_if_silent();
        if (++loops >= 200) {
            loops = 0;
            read_battery();
        }
        update_led();
    }
}

static void start_adc(void) {
    adc_oneshot_unit_init_cfg_t unit = {.unit_id = ADC_UNIT_1};
    if (adc_oneshot_new_unit(&unit, &adc) != ESP_OK) {
        adc = NULL;
        return;
    }
    adc_oneshot_chan_cfg_t chan = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    adc_oneshot_config_channel(adc, ADC_CHANNEL_0, &chan);
    adc_cali_curve_fitting_config_t curve = {
        .unit_id = ADC_UNIT_1,
        .chan = ADC_CHANNEL_0,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_curve_fitting(&curve, &cali) != ESP_OK) {
        cali = NULL;
    }
}

static void wifi_start(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N));
    ESP_ERROR_CHECK(esp_wifi_set_channel(cfg.channel, WIFI_SECOND_CHAN_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(cfg.tx_power_qdbm));
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(recv_cb));
    if (mac_ready(peer)) {
        remember_peer(peer, false);
    }
}

void link_slave_start(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    load_cfg();
    gpio_config_t led = {
        .pin_bit_mask = 1ULL << GPIO_NUM_10,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&led);
    gpio_config_t mute = {
        .pin_bit_mask = 1ULL << GPIO_NUM_6,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&mute);
    start_adc();
    audio_i2s_start();
    down_queue = xQueueCreate(8, sizeof(hs_packet_down_t));
    wifi_start();
    ESP_LOGI(TAG, "fone no canal %u", cfg.channel);
    xTaskCreatePinnedToCore(audio_task, "hs_audio", 12288, NULL, 8, NULL, 0);
}
