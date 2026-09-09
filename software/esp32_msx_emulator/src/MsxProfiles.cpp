#include "MsxProfiles.h"
#include "MsxBoard.h"
#include <Arduino.h>
#include <SD_MMC.h>
#include <ctype.h>
#include <string.h>

MsxProfile MsxProfiles[MsxMaxProfiles];
size_t MsxProfileCount = 0;
static bool mounted = false;

static bool validId(const char *id)
{
    const size_t length = strlen(id);
    if (length == 0 || length > 32) return false;
    for (size_t i = 0; i < length; ++i)
        if (!isalnum(static_cast<unsigned char>(id[i])) && id[i] != '_' && id[i] != '-')
            return false;
    return true;
}

static bool fail(MsxProfile &profile, const char *message)
{
    profile.available = false;
    snprintf(profile.error, sizeof(profile.error), "%s", message);
    Serial.printf("ROM profile %s: %s\n", profile.id, message);
    return false;
}

static bool sizedRom(const MsxProfile &profile, const char *filename, size_t size)
{
    char path[96];
    snprintf(path, sizeof(path), "/msx/bios/%s/%s", profile.id, filename);
    File file = SD_MMC.open(path, FILE_READ);
    return file && !file.isDirectory() && file.size() == size;
}

bool MsxValidateProfile(MsxProfile &profile)
{
    profile.available = false;
    profile.error[0] = '\0';
    if (!mounted) return fail(profile, "SD card not mounted. Insert FAT32 card, rescan.");
    char path[96];
    snprintf(path, sizeof(path), "/msx/bios/%s/profile.ini", profile.id);
    File manifest = SD_MMC.open(path, FILE_READ);
    if (!manifest || manifest.isDirectory()) return fail(profile, "Missing profile.ini. Import ROMs onto SD card.");
    const size_t length = manifest.size();
    if (length > 1024) return fail(profile, "profile.ini exceeds 1024 bytes.");
    char contents[1025];
    if (manifest.readBytes(contents, length) != length)
        return fail(profile, "Could not read complete profile.ini from SD.");
    if (memchr(contents, '\0', length))
        return fail(profile, "profile.ini contains an embedded NUL byte.");
    contents[length] = '\0';
    if (!MsxParseProfile(contents, profile))
    {
        Serial.printf("ROM profile %s: %s\n", profile.id, profile.error);
        return false;
    }
    static const char *mainNames[] = {"MSX.ROM", "MSX2.ROM", "MSX2P.ROM"};
    static const char *subNames[] = {"", "MSX2EXT.ROM", "MSX2PEXT.ROM"};
    if (profile.model == 2)
    {
        snprintf(path, sizeof(path), "/msx/bios/%s/OMEGA.ROM", profile.id);
        if (SD_MMC.exists(path))
        {
            if (!sizedRom(profile, "OMEGA.ROM", 262144))
                return fail(profile, "Invalid OMEGA.ROM; expected one 262144-byte bank.");
            profile.available = true;
            return true;
        }
    }
    if (!sizedRom(profile, mainNames[profile.model], 32768))
        return fail(profile, "Missing/invalid main BIOS; expected 32768 bytes.");
    if (profile.model && !sizedRom(profile, subNames[profile.model], 16384))
        return fail(profile, "Missing/invalid extension; expected 16384 bytes.");
    if (profile.model == 2)
    {
        snprintf(path, sizeof(path), "/msx/bios/%s/MSX2PLOGO.ROM", profile.id);
        if (SD_MMC.exists(path) && !sizedRom(profile, "MSX2PLOGO.ROM", 16384))
            return fail(profile, "Invalid logo ROM; expected 16384 bytes.");
    }
    profile.available = true;
    return true;
}

bool MsxMountSd()
{
    if (mounted) SD_MMC.end();
    mounted = false;
    if (!SD_MMC.setPins(MsxBoard::SdClk, MsxBoard::SdCmd, MsxBoard::SdData))
    {
        Serial.println("SD/MMC pin configuration failed.");
        return false;
    }
    mounted = SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_DEFAULT, 8);
    if (!mounted) Serial.println("SD/MMC mount failed; insert a FAT32 card and rescan from F12.");
    return mounted;
}

void MsxScanProfiles()
{
    MsxProfileCount = 3;
    MsxProfiles[0] = MsxProfile{"omega", "Omega MSX2+ NTSC", 2, 32, false, ""};
    MsxProfiles[1] = MsxProfile{"expert", "Gradiente Expert 1.1", 0, 4, false, ""};
    MsxProfiles[2] = MsxProfile{"hotbit", "Sharp Hotbit 1.2", 0, 4, false, ""};
    if (mounted)
    {
        File directory = SD_MMC.open("/msx/bios");
        if (directory && directory.isDirectory())
        {
            for (File entry = directory.openNextFile(); entry; entry = directory.openNextFile())
            {
                if (!entry.isDirectory()) continue;
                const char *name = strrchr(entry.name(), '/');
                name = name ? name + 1 : entry.name();
                if (!validId(name))
                {
                    Serial.printf("Skipping invalid ROM profile directory: %s\n", name);
                    continue;
                }
                bool duplicate = false;
                for (size_t i = 0; i < MsxProfileCount; ++i)
                    if (!strcasecmp(name, MsxProfiles[i].id)) duplicate = true;
                if (duplicate) continue;
                if (MsxProfileCount == MsxMaxProfiles)
                {
                    Serial.println("ROM profile limit (32) reached; additional profiles were not loaded.");
                    break;
                }
                MsxProfile &profile = MsxProfiles[MsxProfileCount++];
                profile = {};
                snprintf(profile.id, sizeof(profile.id), "%s", name);
                snprintf(profile.name, sizeof(profile.name), "%s", name);
                profile.ramPages = 4;
            }
        }
        else Serial.println("No /msx/bios directory. Use tools\\Import-Roms.ps1.");
    }
    for (size_t i = 0; i < MsxProfileCount; ++i) MsxValidateProfile(MsxProfiles[i]);
}
