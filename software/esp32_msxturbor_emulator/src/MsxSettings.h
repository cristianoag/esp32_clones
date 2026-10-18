#pragma once
#include <stddef.h>
#include <stdint.h>
#include "MsxMappers.h"
#include "MsxAudioProfiles.h"

constexpr size_t MsxSdPathCapacity = 240;

struct MsxBootSettingsV1
{
    uint32_t version;
    char profile[33];
    uint8_t ramPages;
    uint8_t sound;
    uint8_t autoBoot;
};

struct MsxBootSettingsV2
{
    MsxBootSettingsV1 machine;
    char cartridges[2][MsxSdPathCapacity];
};

struct MsxBootSettingsV3
{
    MsxBootSettingsV1 machine;
    char cartridges[2][MsxSdPathCapacity];
    char disks[2][MsxSdPathCapacity];
    char tape[MsxSdPathCapacity];
};

struct MsxBootSettings
{
    MsxBootSettingsV1 machine;
    char cartridges[2][MsxSdPathCapacity];
    char disks[2][MsxSdPathCapacity];
    char tape[MsxSdPathCapacity];
    uint8_t mappers[2];
    uint8_t audioProfile;
    uint8_t reserved;
};

bool MsxDecodeSettings(const void *data, size_t size, MsxBootSettings &settings);
bool MsxValidSdPath(const char *path, bool allowEmpty = false);
bool MsxJoinSdPath(const char *directory, const char *name, char *output, size_t capacity);
void MsxParentSdPath(char *path);
bool MsxHasExtension(const char *name, const char *extension);
