#include "usb_audio.h"

#include "hs_link.h"
#include "link_master.h"
#include "tusb.h"
#include "usb_descriptors.h"

#include "esp_log.h"
#include "esp_private/usb_phy.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

static const char *TAG = "hs_usb";

#define SPK_CAP 4096
#define MIC_CAP 4096

static int16_t spk_ring[SPK_CAP];
static int16_t mic_ring[MIC_CAP];
static volatile size_t spk_r;
static volatile size_t spk_w;
static volatile size_t mic_r;
static volatile size_t mic_w;
static portMUX_TYPE audio_mux = portMUX_INITIALIZER_UNLOCKED;

static bool speaker_mute;
static bool tone_on;
static bool mounted;
static uint32_t sample_rate = HS_SAMPLE_RATE;
static int16_t volume_db[3];
static uint8_t mute_ch[3];

static const int16_t k_tone[48] = {
    0, 2144, 4250, 6284, 8211, 9997, 11612, 13028, 14222, 15172, 15862, 16282,
    16422, 16282, 15862, 15172, 14222, 13028, 11612, 9997, 8211, 6284, 4250, 2144,
    0, -2144, -4250, -6284, -8211, -9997, -11612, -13028, -14222, -15172, -15862, -16282,
    -16422, -16282, -15862, -15172, -14222, -13028, -11612, -9997, -8211, -6284, -4250, -2144
};

static size_t ring_used(size_t write, size_t read, size_t cap) {
    return (write + cap - read) % cap;
}

static void ring_write(int16_t *ring, volatile size_t *write, volatile size_t *read,
                       size_t cap, const int16_t *src, size_t count) {
    portENTER_CRITICAL(&audio_mux);
    for (size_t i = 0; i < count; i++) {
        size_t next = (*write + 1) % cap;
        if (next == *read) {
            *read = (*read + 1) % cap;
        }
        ring[*write] = src[i];
        *write = next;
    }
    portEXIT_CRITICAL(&audio_mux);
}

static size_t ring_read(int16_t *ring, volatile size_t *write, volatile size_t *read,
                        size_t cap, int16_t *dst, size_t count) {
    portENTER_CRITICAL(&audio_mux);
    size_t have = ring_used(*write, *read, cap);
    size_t take = have < count ? have : count;
    for (size_t i = 0; i < take; i++) {
        dst[i] = ring[*read];
        *read = (*read + 1) % cap;
    }
    portEXIT_CRITICAL(&audio_mux);
    return take;
}

static int16_t next_tone(void) {
    static size_t phase;
    int16_t sample = k_tone[phase];
    phase = (phase + 1) % 48;
    return sample;
}

void usb_audio_pull_speaker(int16_t *interleaved, size_t frames) {
    size_t need = frames * 2;
    size_t got = 0;
    if (!tone_on) {
        got = ring_read(spk_ring, &spk_w, &spk_r, SPK_CAP, interleaved, need);
    }
    for (size_t i = got; i < need; i += 2) {
        int16_t sample = tone_on ? next_tone() : 0;
        interleaved[i] = sample;
        if (i + 1 < need) {
            interleaved[i + 1] = sample;
        }
    }
    int db = (int)volume_db[0];
    if (speaker_mute || mute_ch[0]) {
        memset(interleaved, 0, need * sizeof(int16_t));
        return;
    }
    hs_apply_gain(interleaved, need, db);
}

void usb_audio_push_mic(const int16_t *mono, size_t samples) {
    int16_t up[HS_MIC_SAMPLES * 3];
    if (samples > HS_MIC_SAMPLES) {
        samples = HS_MIC_SAMPLES;
    }
    hs_upsample_by_3(mono, up, samples);
    ring_write(mic_ring, &mic_w, &mic_r, MIC_CAP, up, samples * 3);
}

int usb_audio_speaker_db(void) {
    return volume_db[0];
}

bool usb_audio_speaker_muted(void) {
    return speaker_mute || mute_ch[0];
}

void usb_audio_set_tone(bool enabled) {
    tone_on = enabled;
}

bool usb_audio_mounted(void) {
    return mounted;
}

static bool clock_get(uint8_t rhport, audio_control_request_t const *request) {
    if (request->bEntityID != UAC2_ENTITY_CLOCK) {
        return false;
    }
    if (request->bControlSelector == AUDIO_CS_CTRL_SAM_FREQ && request->bRequest == AUDIO_CS_REQ_CUR) {
        audio_control_cur_4_t curf = {.bCur = (int32_t)tu_htole32(sample_rate)};
        return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *)request, &curf, sizeof(curf));
    }
    if (request->bControlSelector == AUDIO_CS_CTRL_SAM_FREQ && request->bRequest == AUDIO_CS_REQ_RANGE) {
        audio_control_range_4_n_t(1) rangef = {.wNumSubRanges = tu_htole16(1)};
        rangef.subrange[0].bMin = 48000;
        rangef.subrange[0].bMax = 48000;
        rangef.subrange[0].bRes = 0;
        return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *)request, &rangef, sizeof(rangef));
    }
    if (request->bControlSelector == AUDIO_CS_CTRL_CLK_VALID && request->bRequest == AUDIO_CS_REQ_CUR) {
        audio_control_cur_1_t valid = {.bCur = 1};
        return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *)request, &valid, sizeof(valid));
    }
    return false;
}

static bool clock_set(audio_control_request_t const *request, uint8_t const *buffer) {
    if (request->bEntityID != UAC2_ENTITY_CLOCK || request->bRequest != AUDIO_CS_REQ_CUR) {
        return false;
    }
    if (request->bControlSelector != AUDIO_CS_CTRL_SAM_FREQ || request->wLength != sizeof(audio_control_cur_4_t)) {
        return false;
    }
    sample_rate = (uint32_t)((audio_control_cur_4_t const *)buffer)->bCur;
    return true;
}

static bool feature_get(uint8_t rhport, audio_control_request_t const *request) {
    if (request->bEntityID != UAC2_ENTITY_SPK_FEATURE_UNIT || request->bChannelNumber > 2) {
        return false;
    }
    uint8_t channel = request->bChannelNumber;
    if (request->bControlSelector == AUDIO_FU_CTRL_MUTE && request->bRequest == AUDIO_CS_REQ_CUR) {
        audio_control_cur_1_t mute1 = {.bCur = mute_ch[channel]};
        return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *)request, &mute1, sizeof(mute1));
    }
    if (request->bControlSelector != AUDIO_FU_CTRL_VOLUME) {
        return false;
    }
    if (request->bRequest == AUDIO_CS_REQ_RANGE) {
        audio_control_range_2_n_t(1) range_vol = {
            .wNumSubRanges = tu_htole16(1),
            .subrange[0] = {.bMin = tu_htole16(-60 * 256), .bMax = tu_htole16(0), .bRes = tu_htole16(256)},
        };
        return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *)request, &range_vol, sizeof(range_vol));
    }
    if (request->bRequest == AUDIO_CS_REQ_CUR) {
        audio_control_cur_2_t cur_vol = {.bCur = tu_htole16((int16_t)(volume_db[channel] * 256))};
        return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *)request, &cur_vol, sizeof(cur_vol));
    }
    return false;
}

static bool feature_set(audio_control_request_t const *request, uint8_t const *buffer) {
    if (request->bEntityID != UAC2_ENTITY_SPK_FEATURE_UNIT || request->bRequest != AUDIO_CS_REQ_CUR) {
        return false;
    }
    if (request->bChannelNumber > 2) {
        return false;
    }
    uint8_t channel = request->bChannelNumber;
    if (request->bControlSelector == AUDIO_FU_CTRL_MUTE && request->wLength == sizeof(audio_control_cur_1_t)) {
        mute_ch[channel] = ((audio_control_cur_1_t const *)buffer)->bCur;
        return true;
    }
    if (request->bControlSelector == AUDIO_FU_CTRL_VOLUME && request->wLength == sizeof(audio_control_cur_2_t)) {
        int16_t raw = ((audio_control_cur_2_t const *)buffer)->bCur;
        volume_db[channel] = (int16_t)(raw / 256);
        return true;
    }
    return false;
}

bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const *request) {
    audio_control_request_t const *audio = (audio_control_request_t const *)request;
    if (audio->bEntityID == UAC2_ENTITY_CLOCK) {
        return clock_get(rhport, audio);
    }
    if (audio->bEntityID == UAC2_ENTITY_SPK_FEATURE_UNIT) {
        return feature_get(rhport, audio);
    }
    return false;
}

bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const *request, uint8_t *buffer) {
    (void)rhport;
    audio_control_request_t const *audio = (audio_control_request_t const *)request;
    if (audio->bEntityID == UAC2_ENTITY_CLOCK) {
        return clock_set(audio, buffer);
    }
    if (audio->bEntityID == UAC2_ENTITY_SPK_FEATURE_UNIT) {
        return feature_set(audio, buffer);
    }
    return false;
}

void tud_mount_cb(void) {
    mounted = true;
}

void tud_umount_cb(void) {
    mounted = false;
}

void tud_suspend_cb(bool remote_wakeup_en) {
    (void)remote_wakeup_en;
    mounted = false;
}

void tud_resume_cb(void) {
    mounted = tud_mounted();
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                               uint8_t *buffer, uint16_t reqlen) {
    (void)instance;
    (void)report_type;
    if (report_id == HS_HID_REPORT_CONFIG && reqlen >= sizeof(hs_hid_config_t)) {
        link_master_get_config((hs_hid_config_t *)buffer);
        return sizeof(hs_hid_config_t);
    }
    if (report_id == HS_HID_REPORT_STATUS && reqlen >= sizeof(hs_hid_status_t)) {
        link_master_fill_status((hs_hid_status_t *)buffer);
        return sizeof(hs_hid_status_t);
    }
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                           uint8_t const *buffer, uint16_t bufsize) {
    (void)instance;
    (void)report_type;
    if (report_id == HS_HID_REPORT_CONFIG && bufsize >= sizeof(hs_hid_config_t)) {
        hs_hid_config_t config;
        memcpy(&config, buffer, sizeof(config));
        link_master_set_config(&config);
    }
}

static void drain_speaker(void) {
    int16_t chunk[192];
    while (tud_audio_available() >= sizeof(chunk)) {
        uint16_t got = tud_audio_read(chunk, sizeof(chunk));
        if (got < 4) {
            break;
        }
        ring_write(spk_ring, &spk_w, &spk_r, SPK_CAP, chunk, got / sizeof(int16_t));
    }
}

static void feed_microphone(void) {
    int16_t chunk[96];
    size_t have = ring_used(mic_w, mic_r, MIC_CAP);
    if (have < 48) {
        return;
    }
    size_t got = ring_read(mic_ring, &mic_w, &mic_r, MIC_CAP, chunk, 48);
    if (got > 0) {
        tud_audio_write(chunk, (uint16_t)(got * sizeof(int16_t)));
    }
}

static void usb_task(void *arg) {
    (void)arg;
    while (true) {
        tud_task();
        drain_speaker();
        feed_microphone();
        link_master_send_hid();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void usb_audio_start(void) {
    usb_phy_handle_t phy = NULL;
    usb_phy_config_t phy_conf = {
        .controller = USB_PHY_CTRL_OTG,
        .target = USB_PHY_TARGET_INT,
        .otg_mode = USB_OTG_MODE_DEVICE,
        .otg_speed = USB_PHY_SPEED_FULL,
    };
    ESP_ERROR_CHECK(usb_new_phy(&phy_conf, &phy));

    tusb_rhport_init_t dev_init = {
        .role = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_FULL,
    };
    if (!tusb_init(0, &dev_init)) {
        ESP_LOGE(TAG, "TinyUSB nao iniciou");
        return;
    }
    xTaskCreatePinnedToCore(usb_task, "hs_usb", 8192, NULL, 8, NULL, 1);
}
