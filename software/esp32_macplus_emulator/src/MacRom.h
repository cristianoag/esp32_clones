#pragma once
#include <stddef.h>
#include <stdint.h>

inline bool MacValidRom(const uint8_t *rom, size_t size)
{
    if (!rom || size != 128 * 1024) return false;
    const uint32_t expected = (uint32_t(rom[0]) << 24) | (uint32_t(rom[1]) << 16) |
                              (uint32_t(rom[2]) << 8) | rom[3];
    uint32_t sum = 0;
    for (size_t i = 4; i < size; i += 2) sum += (uint32_t(rom[i]) << 8) | rom[i + 1];
    return expected == sum && (rom[4] == 0 && rom[5] == 0x40);
}
