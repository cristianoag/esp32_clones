#pragma once
#include <stdint.h>
#include <string.h>

constexpr unsigned A1stVideoWidth = 320;
constexpr unsigned A1stVideoHeight = 240;
constexpr unsigned A1stVideoLineRepeat = 2;

struct A1stViewport
{
    unsigned width, height, left, top;
    explicit A1stViewport(unsigned percent)
        : width(A1stVideoWidth * percent / 100),
          height(A1stVideoHeight * percent / 100),
          left((A1stVideoWidth - width) / 2), top((A1stVideoHeight - height) / 2) {}
    bool containsRow(unsigned y) const { return y >= top && y < top + height; }
    unsigned sourceRow(unsigned y) const
    {
        return (y - top) * (A1stVideoHeight - 1) / (height - 1);
    }
};

inline void A1stFitMenuRow(uint8_t destination[320], const uint8_t source[320],
                           const A1stViewport& viewport)
{
    memset(destination, 0, A1stVideoWidth);
    for (unsigned x = 0; x < viewport.width; ++x)
        destination[viewport.left + x] = source[x * (A1stVideoWidth - 1) / (viewport.width - 1)];
}

inline uint8_t A1stVgaColor(uint16_t rgb565)
{
    return static_cast<uint8_t>(((rgb565 >> 13) & 7) |
                                ((rgb565 >> 5) & 0x38) |
                                ((rgb565 << 3) & 0xc0));
}

inline void A1stScaleVideoRow(uint8_t destination[320], const uint16_t* source,
                              unsigned width, unsigned fit = 100)
{
    const A1stViewport viewport(fit);
    const unsigned scale = width == 544 ? 2 : 1;
    memset(destination, 0, A1stVideoWidth);
    source += 8 * scale;
    const unsigned denominator = viewport.width - 1;
    const unsigned step = (256 * scale - 1) / denominator;
    const unsigned increment = (256 * scale - 1) % denominator;
    unsigned sourceX = 0, fraction = 0;
    for (unsigned x = 0; x < viewport.width; ++x) {
        destination[viewport.left + x] = A1stVgaColor(source[sourceX]);
        sourceX += step;
        fraction += increment;
        if (fraction >= denominator) { fraction -= denominator; ++sourceX; }
    }
}
