#pragma once
#include <stdint.h>

inline void MacRenderLine(const uint8_t *framebuffer, unsigned y, uint8_t line[640])
{
    for (unsigned x = 0; x < 640; ++x) line[x] = 0;
    if (y < 69 || y >= 411) return;
    const uint8_t *source = framebuffer + (y - 69) * 64;
    for (unsigned x = 0; x < 512; ++x)
        line[x + 64] = source[x / 8] & (0x80 >> (x & 7)) ? 0 : 255;
}
