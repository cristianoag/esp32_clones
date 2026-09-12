#include "MsxSettings.h"
#include <stdio.h>
#include <string.h>

static_assert(sizeof(MsxBootSettingsV1) == 40, "Keep compatibility with existing NVS settings.");
static_assert(sizeof(MsxBootSettingsV2) == 520, "Keep compatibility with cartridge-only settings.");
static_assert(offsetof(MsxBootSettings, disks) == sizeof(MsxBootSettingsV2), "Old settings must remain a prefix.");
static_assert(sizeof(MsxBootSettingsV3) == 1240 && sizeof(MsxBootSettings) == 1244, "Keep the saved media layout stable.");
static_assert(offsetof(MsxBootSettings, mappers) == sizeof(MsxBootSettingsV3), "v3 must remain a prefix.");

bool MsxHasExtension(const char *name, const char *extension)
{
    const size_t nameLength = strlen(name), extensionLength = strlen(extension);
    if (nameLength < extensionLength) return false;
    name += nameLength - extensionLength;
    for (size_t i = 0; i < extensionLength; ++i)
    {
        const char a = name[i] >= 'A' && name[i] <= 'Z' ? name[i] + ('a' - 'A') : name[i];
        const char b = extension[i] >= 'A' && extension[i] <= 'Z' ? extension[i] + ('a' - 'A') : extension[i];
        if (a != b) return false;
    }
    return true;
}

bool MsxValidSdPath(const char *path, bool allowEmpty)
{
    if (!path || strnlen(path, MsxSdPathCapacity) == MsxSdPathCapacity) return false;
    if (!*path) return allowEmpty;
    if (*path != '/' || path[1] == '\0') return false;
    const char *component = path + 1;
    for (const char *p = component;; ++p)
    {
        const unsigned char c = *p;
        if (c && (c < 32 || c == 127 || c == '\\' || c == ':')) return false;
        if (!c || c == '/')
        {
            const size_t length = p - component;
            if (!length || (length == 1 && component[0] == '.') ||
                (length == 2 && component[0] == '.' && component[1] == '.'))
                return false;
            if (!c) return true;
            component = p + 1;
        }
    }
}

bool MsxJoinSdPath(const char *directory, const char *name, char *output, size_t capacity)
{
    if (!directory || !name || !output || !capacity ||
        strchr(name, '/') || strchr(name, '\\') || !*name)
        return false;
    const int written = snprintf(output, capacity, "%s%s%s", directory,
                                 !strcmp(directory, "/") ? "" : "/", name);
    return written > 0 && static_cast<size_t>(written) < capacity && MsxValidSdPath(output);
}

void MsxParentSdPath(char *path)
{
    char *slash = strrchr(path, '/');
    if (!slash || slash == path) strcpy(path, "/");
    else *slash = '\0';
}

bool MsxDecodeSettings(const void *data, size_t size, MsxBootSettings &settings)
{
    settings = {};
    if (!data || (size != sizeof(MsxBootSettingsV1) && size != sizeof(MsxBootSettingsV2) &&
                  size != sizeof(MsxBootSettingsV3) && size != sizeof(MsxBootSettings)))
        return false;
    MsxBootSettings candidate = {};
    candidate.mappers[0] = candidate.mappers[1] = MsxMapperAuto;
    memcpy(&candidate, data, size);
    const MsxBootSettingsV1 &machine = candidate.machine;
    if ((size == sizeof(MsxBootSettingsV1) && machine.version != 1) ||
        (size == sizeof(MsxBootSettingsV2) && machine.version != 2) ||
        (size == sizeof(MsxBootSettingsV3) && machine.version != 3) ||
        (size == sizeof(MsxBootSettings) && machine.version != 4) ||
        !memchr(machine.profile, '\0', sizeof(machine.profile)) || !machine.profile[0] ||
        (machine.ramPages != 4 && machine.ramPages != 8 && machine.ramPages != 16 && machine.ramPages != 32) ||
        machine.sound > 1 || machine.autoBoot > 1 ||
        !MsxValidMapper(candidate.mappers[0]) || !MsxValidMapper(candidate.mappers[1]) ||
        candidate.audioProfile >= MsxAudioProfileCount || candidate.reserved)
        return false;
    for (const char *id = machine.profile; *id; ++id)
        if (!((*id >= 'a' && *id <= 'z') || (*id >= 'A' && *id <= 'Z') ||
              (*id >= '0' && *id <= '9') || *id == '-' || *id == '_'))
            return false;
    for (const auto &cartridge : candidate.cartridges)
        if (!MsxValidSdPath(cartridge, true) || (*cartridge && !MsxHasExtension(cartridge, ".rom")))
            return false;
    for (const auto &disk : candidate.disks)
        if (!MsxValidSdPath(disk, true) || (*disk && !MsxHasExtension(disk, ".dsk")))
            return false;
    if (!MsxValidSdPath(candidate.tape, true) || (*candidate.tape && !MsxHasExtension(candidate.tape, ".cas")))
        return false;
    candidate.machine.version = 4;
    settings = candidate;
    return true;
}
