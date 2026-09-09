#pragma once
#include <stdint.h>

// The platform must consume/copy buffers before returning; they remain core-owned.
// Palette entries are 0x00RRGGBB. The core paces PAL/NTSC; do not delay here.
void msxPresent(const uint8_t* pixels, int width, int height, const uint32_t* palette);
void msxPollKeyboard(uint8_t matrix[16]);
// Active-high U,D,L,R,A,B in bits 0..5 and 8..13 for MSX ports 1 and 2.
uint16_t msxPollJoysticks();
bool msxShouldExit();
// Signed mono PCM, 22050 Hz.
void msxSubmitAudio(const int16_t* samples, unsigned count);
void msxReportError(const char* text);
