#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HS_LINK_VERSION "1.0.0"

#define HS_USB_VID 0x303A
#define HS_USB_PID 0x4453

#define HS_TYPE_DOWN 1
#define HS_TYPE_UP 2
#define HS_TYPE_CFG 3
#define HS_TYPE_ACK 4

#define HS_FLAG_MUTE 0x01
#define HS_FLAG_TONE 0x02
#define HS_FLAG_SAVE 0x04

#define HS_PROFILE_GAME 0
#define HS_PROFILE_BALANCE 1
#define HS_PROFILE_STABLE 2
#define HS_PROFILE_CUSTOM 3

#define HS_SAMPLE_RATE 48000
#define HS_CHUNK_FRAMES 240
#define HS_MIC_RATE 16000
#define HS_MIC_SAMPLES 80
#define HS_ADPCM_BYTES 240

#define HS_HID_REPORT_STATUS 1
#define HS_HID_REPORT_CONFIG 2

#if defined(_MSC_VER)
#pragma pack(push, 1)
#define HS_PACKED
#else
#define HS_PACKED __attribute__((packed))
#endif

typedef struct HS_PACKED {
    uint16_t seq;
    uint8_t type;
    uint8_t flags;
    int16_t pred_l;
    int16_t pred_r;
    uint8_t index_l;
    uint8_t index_r;
    uint8_t audio[HS_ADPCM_BYTES];
} hs_packet_down_t;

typedef struct HS_PACKED {
    uint16_t seq;
    uint8_t type;
    uint8_t flags;
    uint8_t battery;
    uint8_t reserved;
    int16_t mic[HS_MIC_SAMPLES];
} hs_packet_up_t;

typedef struct HS_PACKED {
    uint16_t seq;
    uint8_t type;
    uint8_t flags;
    uint8_t frame_ms;
    uint8_t jitter_frames;
    uint8_t channel;
    uint8_t tx_power_qdbm;
    int8_t speaker_db;
    int8_t mic_db;
    int8_t sidetone_db;
    uint8_t mic_boost_db;
    uint8_t apply_id;
    uint8_t profile;
} hs_packet_cfg_t;

typedef struct HS_PACKED {
    uint16_t seq;
    uint8_t type;
    uint8_t apply_id;
    uint8_t frame_ms;
    uint8_t jitter_frames;
    uint8_t profile;
    uint8_t channel;
} hs_packet_ack_t;

/* Relatórios HID sem o byte de Report ID. O USB acrescenta o ID. */
typedef struct HS_PACKED {
    uint8_t linked;
    uint8_t battery;
    uint8_t flags;
    int8_t rssi;
    uint16_t loss_permille;
    uint16_t rtt_ms;
    uint8_t frame_ms;
    uint8_t jitter_frames;
    uint8_t channel;
    uint8_t tx_power_qdbm;
    int8_t speaker_db;
    int8_t mic_db;
    int8_t sidetone_db;
    uint8_t mic_boost_db;
    uint8_t profile;
    uint8_t apply_id;
    uint8_t peer_mac[6];
    char version[8];
} hs_hid_status_t;

typedef struct HS_PACKED {
    uint8_t frame_ms;
    uint8_t jitter_frames;
    uint8_t channel;
    uint8_t tx_power_qdbm;
    int8_t speaker_db;
    int8_t mic_db;
    int8_t sidetone_db;
    uint8_t mic_boost_db;
    uint8_t flags;
    uint8_t peer_mac[6];
    uint8_t profile;
} hs_hid_config_t;

#if defined(_MSC_VER)
#pragma pack(pop)
#endif

typedef struct {
    int16_t predictor;
    int8_t index;
} hs_adpcm_state_t;

_Static_assert(sizeof(hs_packet_down_t) == 250, "pacote de descida");
_Static_assert(sizeof(hs_packet_up_t) == 166, "pacote de subida");
_Static_assert(sizeof(hs_hid_status_t) == 32, "relatorio de estado");
_Static_assert(sizeof(hs_hid_config_t) == 16, "relatorio de configuracao");

void hs_adpcm_encode_stereo(hs_adpcm_state_t *left, hs_adpcm_state_t *right,
                            const int16_t *interleaved, uint8_t *out, size_t frames);

void hs_adpcm_decode_stereo(hs_adpcm_state_t *left, hs_adpcm_state_t *right,
                            const uint8_t *in, int16_t *interleaved, size_t frames);

void hs_apply_gain(int16_t *samples, size_t count, int db);
int8_t hs_clamp_db(int db, int lo, int hi);

void hs_decimate_by_3(const int16_t *in, int16_t *out, size_t groups);
void hs_upsample_by_3(const int16_t *in, int16_t *out, size_t samples);

uint8_t hs_battery_percent(int millivolts);
uint16_t hs_estimate_one_way_ms(uint8_t frame_ms, uint8_t jitter_frames);
uint16_t hs_estimate_round_trip_ms(uint8_t frame_ms, uint8_t jitter_frames);

bool hs_transport_valid(uint8_t frame_ms, uint8_t jitter_frames, uint8_t channel);
void hs_profile_apply(uint8_t profile, uint8_t *frame_ms, uint8_t *jitter_frames);
uint8_t hs_jitter_chunks(uint8_t frame_ms, uint8_t jitter_frames);

#ifdef __cplusplus
}
#endif
