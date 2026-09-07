#include "MsxFileBrowser.h"
#include <Arduino.h>
#include <SD_MMC.h>
#include <string.h>

bool MsxReadDirectoryPage(const char *directory, const char *extension, bool allowEject,
                          size_t first, MsxFileEntry *entries, size_t &count, size_t &total,
                          char *error, size_t errorSize)
{
    count = total = 0;
    error[0] = '\0';
    File folder = SD_MMC.open(directory, FILE_READ);
    if (!folder || !folder.isDirectory())
    {
        snprintf(error, errorSize, "Cannot open SD directory. Insert card and rescan.");
        Serial.printf("MSX browser: cannot open %s\n", directory);
        return false;
    }
    auto append = [&](const char *name, MsxFileKind kind)
    {
        if (total >= first && count < MsxBrowserPageSize)
        {
            snprintf(entries[count].name, sizeof(entries[count].name), "%s", name);
            entries[count++].kind = kind;
        }
        ++total;
    };
    if (allowEject) append("<Eject slot on next boot>", MsxFileKind::Eject);
    if (strcmp(directory, "/")) append("..", MsxFileKind::Parent);
    unsigned visited = 0, skipped = 0;
    char path[MsxSdPathCapacity];
    for (File entry = folder.openNextFile(); entry; entry = folder.openNextFile())
    {
        if (++visited % 16 == 0) delay(1);
        const char *name = strrchr(entry.name(), '/');
        name = name ? name + 1 : entry.name();
        if (!strcmp(name, ".") || !strcmp(name, "..")) continue;
        if (!entry.isDirectory() && !MsxHasExtension(name, extension)) continue;
        if (!MsxJoinSdPath(directory, name, path, sizeof(path)))
        {
            ++skipped;
            Serial.printf("MSX browser: unsupported/overlong SD path in %s: %s\n", directory, name);
            continue;
        }
        append(name, entry.isDirectory() ? MsxFileKind::Directory : MsxFileKind::File);
    }
    if (skipped) snprintf(error, errorSize, "%u names skipped: invalid or path exceeds 239 bytes.", skipped);
    return true;
}
