#include "AppleCore.h"
#include <string.h>
#include "../lib/Adafruit-GFX-Library-master/glcdfont.c"

namespace
{
uint8_t rgb(unsigned r, unsigned g, unsigned b)
{
    return (r >> 5) | ((g >> 5) << 3) | ((b >> 6) << 6);
}

const uint8_t palette[16] = {
    rgb(0,0,0), rgb(221,0,51), rgb(0,0,153), rgb(221,34,221),
    rgb(0,119,34), rgb(85,85,85), rgb(34,34,255), rgb(102,170,255),
    rgb(136,85,0), rgb(255,102,0), rgb(170,170,170), rgb(255,153,136),
    rgb(17,221,0), rgb(255,255,0), rgb(68,255,153), rgb(255,255,255)
};

unsigned textOffset(unsigned row)
{
    return (row & 7) * 128 + (row / 8) * 40;
}
}

void AppleCore::render(uint8_t *pixels, bool monochrome) const
{
    memset(pixels, 0, Width * Height);
    const bool iie = model == AppleModel::IIe;
    const unsigned textBase = page2 && !store80 ? 0x800 : 0x400;
    const unsigned hiresBase = page2 && !store80 ? 0x4000 : 0x2000;
    for (unsigned y = 0; y < 192; ++y)
    {
        uint8_t *line = pixels + (y + 24) * Width + 40;
        if (text || (mixed && y >= 160))
        {
            const bool wide = iie && col80;
            const unsigned columns = wide ? 80 : 40, scale = wide ? 1 : 2;
            for (unsigned column = 0; column < columns; ++column)
            {
                const unsigned bank = wide && !(column & 1) ? 1 : 0;
                const uint8_t code = ram[bank][textBase + textOffset(y / 8) + (wide ? column / 2 : column)];
                bool inverse = code < 0x40;
                unsigned character = code & 127;
                if (code < 0x80)
                {
                    if (iie && altCharset) inverse = true;
                    else
                    {
                        character &= 63;
                        if (code >= 0x40) inverse = (frames / 16) & 1;
                    }
                    if (character < 32) character += 64;
                }
                else if (!iie)
                {
                    character &= 63;
                    if (character < 32) character += 64;
                }
                if (character < 32) character += 64;
                for (unsigned x = 0; x < 7; ++x)
                {
                    const bool dot = x >= 1 && x <= 5 && ((font[character * 5 + x - 1] >> (y & 7)) & 1);
                    const uint8_t color = dot != inverse ? (monochrome ? rgb(0,255,0) : 255) : 0;
                    for (unsigned s = 0; s < scale; ++s) line[(column * 7 + x) * scale + s] = color;
                }
            }
        }
        else if (!hires)
        {
            const bool wide = iie && col80 && doubleHires;
            for (unsigned column = 0; column < (wide ? 80u : 40u); ++column)
            {
                const unsigned bank = wide && !(column & 1) ? 1 : 0;
                const uint8_t value = ram[bank][textBase + textOffset(y / 8) + (wide ? column / 2 : column)];
                unsigned color = (value >> (y & 4 ? 4 : 0)) & 15;
                if (bank) color = ((color << 1) | (color >> 3)) & 15;
                const unsigned width = wide ? 7 : 14;
                memset(line + column * width, monochrome ? rgb(0, color * 17, 0) : palette[color], width);
            }
        }
        else
        {
            const unsigned address = hiresBase + textOffset(y / 8) + (y & 7) * 1024;
            if (iie && col80 && doubleHires)
            {
                uint8_t bits[560];
                for (unsigned column = 0; column < 80; ++column)
                {
                    const uint8_t value = ram[(column & 1) ? 0 : 1][address + column / 2];
                    for (unsigned bit = 0; bit < 7; ++bit) bits[column * 7 + bit] = (value >> bit) & 1;
                }
                for (unsigned x = 0; x < 560; ++x)
                {
                    const unsigned group = x & ~3u;
                    unsigned color = 0;
                    for (unsigned bit = 0; bit < 4; ++bit) color |= bits[group + bit] << bit;
                    // DHR serial bits are in the reverse phase order of the lo-res palette.
                    color = ((color & 1) << 3) | ((color & 14) >> 1);
                    line[x] = monochrome ? (bits[x] ? rgb(0,255,0) : 0) : palette[color];
                }
            }
            else
            {
                bool bits[280];
                for (unsigned x = 0; x < 280; ++x)
                    bits[x] = (ram[0][address + x / 7] >> (x % 7)) & 1;
                for (unsigned x = 0; x < 280; ++x)
                {
                    unsigned color = 0;
                    if (bits[x])
                    {
                        if (monochrome || (x && bits[x - 1]) || (x < 279 && bits[x + 1])) color = 15;
                        else
                        {
                            const bool shift = ram[0][address + x / 7] & 128;
                            color = shift ? (x & 1 ? 9 : 6) : (x & 1 ? 12 : 3);
                        }
                    }
                    line[x * 2] = line[x * 2 + 1] = monochrome && color ? rgb(0,255,0) : palette[color];
                }
            }
        }
    }
}
