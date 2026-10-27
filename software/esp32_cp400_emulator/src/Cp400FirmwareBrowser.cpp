#include "Cp400FirmwareBrowser.h"
#include "Cp400Flh.h"
#include <Arduino.h>
#include <SD_MMC.h>
#include <stdio.h>
#include <string.h>

bool Cp400JoinFirmwarePath(const char *directory, const char *name,
                          char *output, size_t capacity)
{
    if (!directory || !name || !output || !capacity || !*name ||
        strchr(name, '/') || strchr(name, '\\') ||
        !strcmp(name, ".") || !strcmp(name, ".."))
        return false;
    for (const unsigned char *p = reinterpret_cast<const unsigned char *>(name); *p; ++p)
        if (*p < 32 || *p == 127 || *p == ':') return false;
    const int written = snprintf(output, capacity, "%s%s%s", directory,
                                 !strcmp(directory, "/") ? "" : "/", name);
    return written > 0 && static_cast<size_t>(written) < capacity &&
           static_cast<size_t>(written) < Cp400FirmwarePathCapacity;
}

void Cp400ParentFirmwarePath(char *path)
{
    char *slash = strrchr(path, '/');
    if (!slash || slash == path) strcpy(path, "/");
    else *slash = '\0';
}

bool Cp400ReadFirmwarePage(const char *directory, size_t first,
                          Cp400FirmwareEntry *entries, size_t &count, size_t &total,
                          char *error, size_t errorSize)
{
    count = total = 0;
    error[0] = '\0';
    File folder = SD_MMC.open(directory, FILE_READ);
    if (!folder || !folder.isDirectory())
    {
        snprintf(error, errorSize, "Cannot open SD folder.");
        Serial.printf("CP400 firmware browser: cannot open %s\n", directory);
        return false;
    }
    auto append = [&](const char *name, Cp400FirmwareEntryKind kind)
    {
        if (total >= first && count < Cp400FirmwarePageSize)
        {
            snprintf(entries[count].name, sizeof(entries[count].name), "%s", name);
            entries[count++].kind = kind;
        }
        ++total;
    };
    if (strcmp(directory, "/")) append("..", Cp400FirmwareEntryKind::Parent);
    unsigned visited = 0, skipped = 0;
    char path[Cp400FirmwarePathCapacity];
    for (File entry = folder.openNextFile(); entry; entry = folder.openNextFile())
    {
        if (++visited % 16 == 0) delay(1);
        const char *name = strrchr(entry.name(), '/');
        name = name ? name + 1 : entry.name();
        if (!strcmp(name, ".") || !strcmp(name, "..")) continue;
        const bool directoryEntry = entry.isDirectory();
        if (directoryEntry && !strcasecmp(name, "System Volume Information")) continue;
        if (strlen(name) >= sizeof(entries[0].name) ||
            !Cp400JoinFirmwarePath(directory, name, path, sizeof(path)))
        {
            ++skipped;
            Serial.printf("CP400 firmware browser: unsupported SD name in %s: %s\n", directory, name);
            continue;
        }
        if (!directoryEntry && !Cp400Flh::IsFirmwarePath(path)) continue;
        append(name, directoryEntry ? Cp400FirmwareEntryKind::Directory : Cp400FirmwareEntryKind::File);
    }
    if (skipped) snprintf(error, errorSize, "%u invalid or overlong SD names skipped.", skipped);
    return true;
}
