#pragma once
#include "TkCore.h"
#include "TkInput.h"
#include <string.h>

constexpr unsigned TkPathSize = 240;
struct TkSettings
{
    uint8_t version = 2;
    TkModel model = TkModel::TK95;
    TkTiming timing = TkTiming::Hz60;
    uint8_t sound = 1, volume = 70, autoBoot = 1;
    TkJoystick joystick[2] = {TkJoystick::Kempston, TkJoystick::Sinclair2};
    uint8_t frameSkip = 0, portuguese = 1;
    char rom[2][TkPathSize] = {"/tk/bios/tk95.rom", "/tk/bios/tk90.rom"};
    char tape[TkPathSize] = {};
};

inline bool TkValidPath(const char *path, bool empty = false)
{
    if (!path) return false;
    size_t length = 0;
    while (length < TkPathSize && path[length]) ++length;
    if (length == TkPathSize) return false;
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
            const size_t length = p - part;
            if (!length || (length == 1 && part[0] == '.') ||
                (length == 2 && part[0] == '.' && part[1] == '.')) return false;
            part = p + 1;
        }
    }
    return true;
}

inline bool TkValidSettings(const TkSettings &value)
{
    return value.version == 2 && value.model <= TkModel::TK90X && value.timing <= TkTiming::Hz50 &&
           value.sound <= 1 && value.volume <= 100 && value.autoBoot <= 1 &&
           value.joystick[0] <= TkJoystick::Cursor && value.joystick[1] <= TkJoystick::Cursor &&
           value.frameSkip <= 2 && value.portuguese <= 1 && TkValidPath(value.rom[0]) && TkValidPath(value.rom[1]) &&
           TkValidPath(value.tape, true);
}

inline bool TkDecodeSettings(const void *data, size_t size, TkSettings &value)
{
    if (!data || size != sizeof(TkSettings)) return false;
    TkSettings candidate;
    memcpy(&candidate, data, size);
    const bool migrate = candidate.version == 1;
    if (migrate) candidate.version = 2;
    if (!TkValidSettings(candidate)) return false;
    if (migrate)
    {
        const char *legacy[] = {"/bios/tk95.rom", "/bios/tk90.rom"};
        const TkSettings defaults;
        for (unsigned model = 0; model < 2; ++model)
            if (!strcmp(candidate.rom[model], legacy[model]))
                memcpy(candidate.rom[model], defaults.rom[model], TkPathSize);
    }
    value = candidate;
    return true;
}
