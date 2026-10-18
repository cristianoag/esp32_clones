#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
void tmeInit(uint8_t *rom, uint8_t *ram);
void tmeRunFrame(void);
const uint8_t *tmeFramebuffer(void);
void tmeAudio(int16_t samples[370], unsigned volume);
#ifdef __cplusplus
}
#endif
