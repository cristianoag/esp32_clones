#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

constexpr unsigned C64PathSize = 240;
struct C64Settings
{
    uint8_t version = 1;
    uint8_t sound = 1, volume = 70, autoBoot = 1, frameSkip = 0, swapPorts = 0;
    uint8_t joystick[2] = {1,1};
    char rom[3][C64PathSize] = {"/c64/bios/basic.rom", "/c64/bios/kernal.rom", "/c64/bios/chargen.rom"};
};

inline bool C64ValidPath(const char *path, bool empty = false)
{
    if (!path) return false;
    size_t length = 0;
    while (length < C64PathSize && path[length]) ++length;
    if (length == C64PathSize) return false;
    const char *end = path + length;
    if (end == path) return empty;
    if (*path != '/' || end == path + 1) return false;
    const char *part = path + 1;
    for (const char *p = part; p <= end; ++p)
    {
        const unsigned char c = *p;
        if (c && (c < 32 || c == 127 || c == '\\' || c == ':')) return false;
        if (!c || c == '/')
        {
            const size_t size = p - part;
            if (!size || (size == 1 && part[0] == '.') ||
                (size == 2 && part[0] == '.' && part[1] == '.')) return false;
            part = p + 1;
        }
    }
    return true;
}

inline bool C64ValidSettings(const C64Settings &value)
{
    return value.version == 1 &&
           value.sound <= 1 && value.volume <= 100 && value.autoBoot <= 1 &&
           value.frameSkip <= 2 && value.swapPorts <= 1 &&
           value.joystick[0] <= 1 && value.joystick[1] <= 1 &&
           C64ValidPath(value.rom[0]) && C64ValidPath(value.rom[1]) &&
           C64ValidPath(value.rom[2]);
}

inline bool C64DecodeSettings(const void *data, size_t size, C64Settings &value)
{
    if (!data || size != sizeof(C64Settings)) return false;
    C64Settings candidate;
    memcpy(&candidate, data, size);
    if (!C64ValidSettings(candidate)) return false;
    value = candidate;
    return true;
}
