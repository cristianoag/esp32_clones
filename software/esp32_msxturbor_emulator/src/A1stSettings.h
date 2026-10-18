#pragma once
#include "MsxSettings.h"
#include <string.h>

struct A1stSettings
{
    uint32_t version = 1;
    char disk[MsxSdPathCapacity] = {};
    uint8_t sound = 1;
    uint8_t autoBoot = 1;
    uint8_t firmware = 0;
    uint8_t drawEvery = 2;
};

inline bool A1stDecodeSettings(const void* data, size_t size, A1stSettings& result)
{
    if (!data || size != sizeof(A1stSettings)) return false;
    A1stSettings candidate;
    memcpy(&candidate, data, size);
    if (candidate.version != 1 || candidate.sound > 1 || candidate.autoBoot > 1 ||
        candidate.firmware > 1 || candidate.drawEvery < 1 || candidate.drawEvery > 6 ||
        !MsxValidSdPath(candidate.disk, true) ||
        (candidate.disk[0] && !MsxHasExtension(candidate.disk, ".dsk"))) return false;
    result = candidate;
    return true;
}
