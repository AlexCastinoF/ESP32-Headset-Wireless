#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void usb_audio_start(void);
void usb_audio_pull_speaker(int16_t *interleaved, size_t frames);
void usb_audio_push_mic(const int16_t *mono, size_t samples);
int usb_audio_speaker_db(void);
bool usb_audio_speaker_muted(void);
void usb_audio_set_tone(bool enabled);
bool usb_audio_mounted(void);
