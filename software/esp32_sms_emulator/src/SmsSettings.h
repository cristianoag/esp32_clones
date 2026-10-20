#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

constexpr unsigned SmsPathSize = 240;
enum class SmsMapper : uint8_t { Auto, Sega, Codemasters, Korean };
struct SmsSettings
{
    uint8_t version = 1;
    uint8_t pal = 0, japanese = 0;
    uint8_t sound = 1, volume = 70, autoBoot = 1, frameSkip = 0;
    uint8_t joystick[2] = {1, 1};
    SmsMapper mapper = SmsMapper::Auto;
    char rom[SmsPathSize] = {};
};

inline bool SmsValidPath(const char *path, bool empty = false)
{
    if (!path) return false;
    size_t length = 0;
    while (length < SmsPathSize && path[length]) ++length;
    if (length == SmsPathSize) return false;
    if (!length) return empty;
    if (*path != '/' || length == 1) return false;
    const char *part = path + 1, *end = path + length;
    for (const char *p = part; p <= end; ++p)
    {
        const unsigned char c = *p;
        if (c && (c < 32 || c == 127 || c == '\\' || c == ':')) return false;
        if (!c || c == '/')
        {
            const size_t count = p - part;
            if (!count || (count == 1 && part[0] == '.') ||
                (count == 2 && part[0] == '.' && part[1] == '.')) return false;
            part = p + 1;
        }
    }
    return true;
}

inline bool SmsValidSettings(const SmsSettings &s)
{
    return s.version == 1 && s.pal <= 1 && s.japanese <= 1 && s.sound <= 1 &&
           s.volume <= 100 && s.autoBoot <= 1 && s.frameSkip <= 2 &&
           s.joystick[0] <= 1 && s.joystick[1] <= 1 && s.mapper <= SmsMapper::Korean &&
           SmsValidPath(s.rom, true);
}

inline bool SmsDecodeSettings(const void *data, size_t size, SmsSettings &result)
{
    if (!data || size != sizeof(SmsSettings)) return false;
    SmsSettings candidate;
    memcpy(&candidate, data, size);
    if (!SmsValidSettings(candidate)) return false;
    result = candidate;
    return true;
}
