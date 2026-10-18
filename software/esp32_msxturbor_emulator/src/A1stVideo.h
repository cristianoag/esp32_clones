#pragma once
#include <stdint.h>

inline uint8_t A1stVgaColor(uint16_t rgb565)
{
    return static_cast<uint8_t>(((rgb565 >> 13) & 7) |
                                ((rgb565 >> 5) & 0x38) |
                                ((rgb565 << 3) & 0xc0));
}

inline void A1stScaleVideoRow(uint8_t destination[320], const uint16_t* source,
                              unsigned width)
{
    const unsigned scale = width == 544 ? 2 : 1;
    source += 8 * scale;
    for (unsigned x = 0; x < 320; ++x)
        destination[x] = A1stVgaColor(source[x * 256 * scale / 320]);
}
