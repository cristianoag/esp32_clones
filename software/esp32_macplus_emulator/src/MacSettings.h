#pragma once
#include "MacFlh.h"
#include <string.h>

constexpr size_t MacPathSize = 192;
struct MacSettings
{
    uint32_t schema = 1;
    char rom[MacPathSize] = "/macplus/bios/macplus.rom";
    char disk[MacPathSize] = "/macplus/disks/hd.img";
    uint8_t readOnly = 0;
    uint8_t sound = 1;
    uint8_t volume = 70;
    uint8_t autoBoot = 1;
    uint8_t frameSkip = 1;
};

inline bool MacValidPath(const char *path)
{
    if (!path || path[0] != '/') return false;
    const size_t length = strnlen(path, MacPathSize);
    if (!length || length == MacPathSize) return false;
    const char *component = path + 1;
    for (const char *p = component;; ++p) {
        const unsigned char c = *p;
        if (c && (c < 32 || c == 127 || c == '\\' || c == ':')) return false;
        if (!c || c == '/') {
            const size_t count = p - component;
            if (!count || (count == 1 && component[0] == '.') ||
                (count == 2 && component[0] == '.' && component[1] == '.')) return false;
            if (!c) return true;
            component = p + 1;
        }
    }
}

inline bool MacDecodeSettings(const void *bytes, size_t size, MacSettings &result)
{
    if (!bytes || size != sizeof(MacSettings)) return false;
    MacSettings candidate;
    memcpy(&candidate, bytes, size);
    if (candidate.schema != 1 || !MacValidPath(candidate.rom) ||
        (candidate.disk[0] && !MacValidPath(candidate.disk)) || candidate.readOnly > 1 ||
        candidate.sound > 1 || candidate.volume > 100 || candidate.autoBoot > 1 ||
        candidate.frameSkip > 3) return false;
    result = candidate;
    return true;
}
