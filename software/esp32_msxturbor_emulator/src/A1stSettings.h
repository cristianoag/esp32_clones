#pragma once
#include "MsxSettings.h"
#include <string.h>

struct A1stSettingsV1
{
    uint32_t version = 1;
    char disk[MsxSdPathCapacity] = {};
    uint8_t sound = 1;
    uint8_t autoBoot = 1;
    uint8_t firmware = 0;
    uint8_t drawEvery = 2;
};

struct A1stSettings
{
    uint32_t version = 2;
    char disks[2][MsxSdPathCapacity] = {};
    char cartridges[2][MsxSdPathCapacity] = {};
    uint8_t sound = 1;
    uint8_t autoBoot = 1;
    uint8_t firmware = 0;
    uint8_t drawEvery = 2;
    uint8_t mappers[2] = {MsxMapperAuto, MsxMapperAuto};
    uint8_t vgaFit = 90;
    uint8_t reserved = 0;
};

static_assert(sizeof(A1stSettingsV1) == 248, "Preserve the original A1ST NVS layout.");
static_assert(sizeof(A1stSettings) == 972, "Keep A1ST NVS settings stable.");

inline bool A1stDecodeSettings(const void* data, size_t size, A1stSettings& result)
{
    if (!data) return false;
    A1stSettings candidate;
    if (size == sizeof(A1stSettingsV1)) {
        A1stSettingsV1 old;
        memcpy(&old, data, size);
        if (old.version != 1) return false;
        memcpy(candidate.disks[0], old.disk, sizeof(old.disk));
        candidate.sound = old.sound;
        candidate.autoBoot = old.autoBoot;
        candidate.firmware = old.firmware;
        candidate.drawEvery = old.drawEvery;
    }
    else if (size == sizeof(A1stSettings)) memcpy(&candidate, data, size);
    else return false;
    if (candidate.version != 2 || candidate.sound > 1 || candidate.autoBoot > 1 ||
        candidate.firmware > 1 || candidate.drawEvery < 1 || candidate.drawEvery > 6 ||
        candidate.vgaFit < 80 || candidate.vgaFit > 100 || candidate.vgaFit % 5 ||
        candidate.reserved || !MsxValidMapper(candidate.mappers[0]) ||
        !MsxValidMapper(candidate.mappers[1])) return false;
    for (const auto& disk : candidate.disks)
        if (!MsxValidSdPath(disk, true) || (disk[0] && !MsxHasExtension(disk, ".dsk"))) return false;
    for (const auto& cartridge : candidate.cartridges)
        if (!MsxValidSdPath(cartridge, true) ||
            (cartridge[0] && !MsxHasExtension(cartridge, ".rom"))) return false;
    result = candidate;
    return true;
}
