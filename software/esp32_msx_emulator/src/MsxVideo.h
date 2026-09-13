#pragma once
#include <stdint.h>

// Low-bandwidth 320x240 framebuffer with hardware line doubling.
constexpr int MsxVideoWidth = 320;
constexpr int MsxVideoHeight = 240;
constexpr int MsxVideoLineRepeat = 2;
constexpr int MsxMenuPixelScaleX = 1;

inline void MsxScaleVideoRow(uint8_t *destination, const uint8_t *source,
                            const uint8_t colors[256])
{
    static_assert(MsxVideoWidth == 320, "This scaler expands four source pixels into five.");
    for (int x = 0, out = 0; x < 256; x += 4, out += 5)
    {
        const uint8_t first = colors[source[x]];
        destination[out] = first;
        destination[out + 1] = first;
        destination[out + 2] = colors[source[x + 1]];
        destination[out + 3] = colors[source[x + 2]];
        destination[out + 4] = colors[source[x + 3]];
    }
}
