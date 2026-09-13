#pragma once
#include <stdint.h>

// Low-bandwidth 320x240 framebuffer with hardware line doubling.
constexpr int MsxVideoWidth = 320;
constexpr int MsxVideoHeight = 240;
constexpr int MsxVideoLineRepeat = 2;
constexpr int MsxMenuPixelScaleX = 1;

inline void MsxScaleVideoRow(uint8_t *destination, const uint8_t *source,
                            const uint8_t colors[256], const uint16_t sourceX[MsxVideoWidth])
{
    for (int x = 0; x < MsxVideoWidth; ++x) destination[x] = colors[source[sourceX[x]]];
}

inline void MsxVideoColumns(int sourceWidth, uint16_t sourceX[MsxVideoWidth])
{
    for (int x = 0; x < MsxVideoWidth; ++x) sourceX[x] = x * sourceWidth / MsxVideoWidth;
}
