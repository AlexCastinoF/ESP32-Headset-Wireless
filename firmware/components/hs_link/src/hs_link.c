#include "hs_link.h"

#include <string.h>

static const int16_t k_index_table[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
};

static const int16_t k_step_table[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230,
    253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
    1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327,
    3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487,
    12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

/* Ganho Q15 com 32768 = 0 dB. Índice 0 é -60 dB, índice 60 é 0 dB, índice 84 é +24 dB. */
static const int32_t k_gain_q15[85] = {
    33, 37, 41, 46, 52, 58, 65, 73,
    82, 92, 104, 116, 130, 146, 164, 184,
    207, 232, 260, 292, 328, 368, 413, 463,
    519, 583, 654, 734, 823, 924, 1036, 1163,
    1305, 1464, 1642, 1843, 2068, 2320, 2603, 2920,
    3277, 3677, 4125, 4629, 5193, 5827, 6538, 7336,
    8231, 9235, 10362, 11627, 13045, 14637, 16423, 18427,
    20675, 23198, 26029, 29205, 32768, 36766, 41252, 46286,
    51934, 58271, 65381, 73358, 82309, 92353, 103622, 116265,
    130452, 146369, 164229, 184268, 206752, 231980, 260285, 292045,
    327680, 367663, 412525, 462860, 519338
};

static int16_t clamp16(int32_t value) {
    if (value > 32767) {
        return 32767;
    }
    if (value < -32768) {
        return -32768;
    }
    return (int16_t)value;
}

static int clamp_index(int index) {
    if (index < 0) {
        return 0;
    }
    if (index > 88) {
        return 88;
    }
    return index;
}

static int16_t decode_nibble(hs_adpcm_state_t *state, uint8_t nibble) {
    int index = clamp_index(state->index);
    int step = k_step_table[index];
    int delta = step >> 3;
    if (nibble & 4) {
        delta += step;
    }
    if (nibble & 2) {
        delta += step >> 1;
    }
    if (nibble & 1) {
        delta += step >> 2;
    }
    int32_t predictor = state->predictor;
    if (nibble & 8) {
        predictor -= delta;
    } else {
        predictor += delta;
    }
    state->predictor = clamp16(predictor);
    state->index = (int8_t)clamp_index(index + k_index_table[nibble & 0x0F]);
    return state->predictor;
}

static uint8_t encode_sample(hs_adpcm_state_t *state, int16_t sample) {
    int index = clamp_index(state->index);
    int step = k_step_table[index];
    int32_t diff = (int32_t)sample - state->predictor;
    uint8_t nibble = 0;
    if (diff < 0) {
        nibble = 8;
        diff = -diff;
    }
    int delta = step >> 3;
    if (diff >= step) {
        nibble |= 4;
        diff -= step;
        delta += step;
    }
    step >>= 1;
    if (diff >= step) {
        nibble |= 2;
        diff -= step;
        delta += step;
    }
    step >>= 1;
    if (diff >= step) {
        nibble |= 1;
        delta += step;
    }
    decode_nibble(state, nibble);
    return nibble;
}

void hs_adpcm_encode_stereo(hs_adpcm_state_t *left, hs_adpcm_state_t *right,
                            const int16_t *interleaved, uint8_t *out, size_t frames) {
    for (size_t i = 0; i < frames; i++) {
        uint8_t lo = encode_sample(left, interleaved[i * 2]);
        uint8_t hi = encode_sample(right, interleaved[i * 2 + 1]);
        out[i] = (uint8_t)((hi << 4) | (lo & 0x0F));
    }
}

void hs_adpcm_decode_stereo(hs_adpcm_state_t *left, hs_adpcm_state_t *right,
                            const uint8_t *in, int16_t *interleaved, size_t frames) {
    for (size_t i = 0; i < frames; i++) {
        uint8_t byte = in[i];
        interleaved[i * 2] = decode_nibble(left, byte & 0x0F);
        interleaved[i * 2 + 1] = decode_nibble(right, byte >> 4);
    }
}

int8_t hs_clamp_db(int db, int lo, int hi) {
    if (db < lo) {
        return (int8_t)lo;
    }
    if (db > hi) {
        return (int8_t)hi;
    }
    return (int8_t)db;
}

void hs_apply_gain(int16_t *samples, size_t count, int db) {
    int clamped = hs_clamp_db(db, -60, 24);
    int32_t gain = k_gain_q15[clamped + 60];
    for (size_t i = 0; i < count; i++) {
        samples[i] = clamp16(((int32_t)samples[i] * gain) >> 15);
    }
}

void hs_decimate_by_3(const int16_t *in, int16_t *out, size_t groups) {
    for (size_t i = 0; i < groups; i++) {
        int32_t sum = (int32_t)in[i * 3] + in[i * 3 + 1] + in[i * 3 + 2];
        out[i] = (int16_t)(sum / 3);
    }
}

void hs_upsample_by_3(const int16_t *in, int16_t *out, size_t samples) {
    if (samples == 0) {
        return;
    }
    for (size_t i = 0; i < samples; i++) {
        int32_t a = in[i];
        int32_t b = (i + 1 < samples) ? in[i + 1] : in[i];
        out[i * 3] = (int16_t)a;
        out[i * 3 + 1] = (int16_t)((2 * a + b) / 3);
        out[i * 3 + 2] = (int16_t)((a + 2 * b) / 3);
    }
}

uint8_t hs_battery_percent(int millivolts) {
    static const int mv[] = {3300, 3400, 3550, 3700, 3800, 3900, 4000, 4150};
    static const int pct[] = {0, 8, 20, 40, 55, 70, 85, 100};
    if (millivolts <= mv[0]) {
        return 0;
    }
    if (millivolts >= mv[7]) {
        return 100;
    }
    for (int i = 0; i < 7; i++) {
        if (millivolts <= mv[i + 1]) {
            int span = mv[i + 1] - mv[i];
            int rise = pct[i + 1] - pct[i];
            return (uint8_t)(pct[i] + (millivolts - mv[i]) * rise / span);
        }
    }
    return 100;
}

uint16_t hs_estimate_one_way_ms(uint8_t frame_ms, uint8_t jitter_frames) {
    return (uint16_t)(frame_ms + 1u + (uint16_t)jitter_frames * frame_ms + 11u);
}

uint16_t hs_estimate_round_trip_ms(uint8_t frame_ms, uint8_t jitter_frames) {
    return (uint16_t)(2u * hs_estimate_one_way_ms(frame_ms, jitter_frames) + 8u);
}

bool hs_transport_valid(uint8_t frame_ms, uint8_t jitter_frames, uint8_t channel) {
    if (frame_ms != 5 && frame_ms != 10) {
        return false;
    }
    if (jitter_frames < 1 || jitter_frames > 3) {
        return false;
    }
    if (channel < 1 || channel > 13) {
        return false;
    }
    return true;
}

void hs_profile_apply(uint8_t profile, uint8_t *frame_ms, uint8_t *jitter_frames) {
    if (profile == HS_PROFILE_GAME) {
        *frame_ms = 5;
        *jitter_frames = 1;
    } else if (profile == HS_PROFILE_STABLE) {
        *frame_ms = 10;
        *jitter_frames = 2;
    } else if (profile == HS_PROFILE_BALANCE) {
        *frame_ms = 5;
        *jitter_frames = 2;
    }
}

uint8_t hs_jitter_chunks(uint8_t frame_ms, uint8_t jitter_frames) {
    uint8_t chunks = (uint8_t)(frame_ms / 5u);
    if (chunks == 0) {
        chunks = 1;
    }
    return (uint8_t)(chunks * jitter_frames);
}
