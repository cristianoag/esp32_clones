#ifndef FMSX_ESP32_PORT_H
#define FMSX_ESP32_PORT_H
#include <stdio.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
FILE *fmsxOpen(const char *name, const char *mode);
void *fmsxAllocate(size_t size);
void *fmsxAllocateCpu(size_t size);
/* Called once per emulated frame, even when the renderer skips that frame. */
void fmsxFrame(void);
void ResetVDP(void);
unsigned int InitAudio(unsigned int rate, unsigned int latency);
void TrashAudio(void);
#ifdef __cplusplus
}
#endif
#endif
