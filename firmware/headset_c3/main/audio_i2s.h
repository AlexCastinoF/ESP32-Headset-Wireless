#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool audio_i2s_start(void);
bool audio_i2s_read_left(int16_t *left, size_t frames);
void audio_i2s_write_stereo(const int16_t *interleaved, size_t frames);
