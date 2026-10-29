#pragma once
#include <stdint.h>

namespace AppleDiskActivity
{
constexpr unsigned X = 600, Y = 222, Width = 32, Height = 12;

inline uint8_t pixel(unsigned x, unsigned y, uint8_t drives, bool bright)
{
    const unsigned drive = x / 16;
    x %= 16;
    if (drive >= 2 || y >= Height || x >= 12 || !(drives & (1u << drive))) return 0;
    if (y == 0 && x > 9) return 0;
    if (!x || x == 11 || !y || y == 11 || (y <= 3 && x >= 3 && x <= 8)) return 255;
    static const uint8_t digits[2][5] = {{2,6,2,2,7}, {6,1,2,4,7}};
    if (y >= 5 && y <= 9 && x >= 5 && x <= 7 && (digits[drive][y - 5] & (4u >> (x - 5)))) return 255;
    return bright ? 0x38 : 0x18;
}
}
