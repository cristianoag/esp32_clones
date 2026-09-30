#pragma once
#include "AppleCore.h"
#include <string.h>

constexpr unsigned ApplePathSize = 240;
struct AppleSettings
{
    uint8_t version = 1;
    AppleModel model = AppleModel::IIe;
    uint8_t sound = 1, volume = 70, autoBoot = 1, frameSkip = 0, monochrome = 0;
    uint8_t joystick[2] = {1,1};
    char rom[2][ApplePathSize] = {"/apple2/bios/apple2plus.rom", "/apple2/bios/apple2e.rom"};
    char diskRom[ApplePathSize] = "/apple2/bios/disk2.rom";
    char disk[2][ApplePathSize] = {};
};

inline bool AppleValidPath(const char *path, bool empty = false)
{
    if (!path) return false;
    size_t length = 0;
    while (length < ApplePathSize && path[length]) ++length;
    if (length == ApplePathSize) return false;
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

inline bool AppleValidSettings(const AppleSettings &value)
{
    return value.version == 1 && value.model <= AppleModel::IIe &&
           value.sound <= 1 && value.volume <= 100 && value.autoBoot <= 1 &&
           value.frameSkip <= 2 && value.monochrome <= 1 &&
           value.joystick[0] <= 1 && value.joystick[1] <= 1 &&
           AppleValidPath(value.rom[0]) && AppleValidPath(value.rom[1]) &&
           AppleValidPath(value.diskRom) && AppleValidPath(value.disk[0], true) &&
           AppleValidPath(value.disk[1], true);
}

inline bool AppleDecodeSettings(const void *data, size_t size, AppleSettings &value)
{
    if (!data || size != sizeof(AppleSettings)) return false;
    AppleSettings candidate;
    memcpy(&candidate, data, size);
    if (!AppleValidSettings(candidate)) return false;
    value = candidate;
    return true;
}
