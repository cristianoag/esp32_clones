#pragma once
#include <stdint.h>
#include <string.h>

constexpr uint8_t C64Rgb(unsigned r, unsigned g, unsigned b)
{
    return (r >> 5) | ((g >> 5) << 3) | ((b >> 6) << 6);
}

inline void C64VideoLine(const uint8_t *pixels, uint8_t border, unsigned y, uint8_t (&line)[320])
{
    static const uint8_t palette[16] = {
        C64Rgb(0,0,0), C64Rgb(255,255,255), C64Rgb(136,0,0), C64Rgb(170,255,238),
        C64Rgb(204,68,204), C64Rgb(0,204,85), C64Rgb(0,0,170), C64Rgb(238,238,119),
        C64Rgb(221,136,85), C64Rgb(102,68,0), C64Rgb(255,119,119), C64Rgb(51,51,51),
        C64Rgb(119,119,119), C64Rgb(170,255,102), C64Rgb(0,136,255), C64Rgb(187,187,187)
    };
    if (y < 20 || y >= 220) memset(line, palette[border & 15], sizeof(line));
    else
        for (unsigned x = 0; x < 320; ++x) line[x] = palette[pixels[(y - 20) * 320 + x] & 15];
}
