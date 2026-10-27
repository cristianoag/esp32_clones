#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Preferences.h>
#include <SD_MMC.h>
#include <VGA.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <algorithm>
#include "A1stCore.h"
#include "A1stSettings.h"
#include "A1stVideo.h"
#include "MsxBoard.h"
#include "MsxKeyboard.h"
#include "MsxAudio.h"
#include "MsxFileBrowser.h"
#include "MsxFirmwareUpdate.h"
#include "MsxJoysticks.h"
#include "MsxJoystickHost.h"

namespace
{
VGA video;
Preferences preferences;
A1stSettings settings;
bool videoReady, audioReady, sdReady, nvsReady, running;
uint8_t* backup;
uint8_t* menuPixels;
char status[160] = {};
const char* const BiosDirectory = "/sdcard/msxturbor/bios/fs-a1st";
int64_t wallAnchor, reportStart, lastYield;
uint64_t tickAnchor, reportTicks;
uint32_t presented;
bool pacingReady;
static_assert(static_cast<int>(MsxMapperAuto) == A1ST_MAPPER_AUTO &&
              static_cast<int>(MsxFmPac) == A1ST_FMPAC, "Keep frontend and core mapper IDs aligned.");

enum MenuItem : unsigned
{
    Resume, Machine, Media, Sound, Firmware, AutoBoot, DrawEvery, VgaFit,
    Save, Boot, Rescan, Update, Joysticks, MenuCount
};
enum MediaItem : unsigned
{
    Slot1, Mapper1, Slot2, Mapper2, DiskA, DiskB,
    MediaSaveBoot, MediaBoot, MediaCount
};

class MenuDisplay : public Adafruit_GFX
{
public:
    MenuDisplay() : Adafruit_GFX(320, 240) {}
    void drawPixel(int16_t x, int16_t y, uint16_t color) override
    {
        if (x >= 0 && y >= 0 && x < width() && y < height()) menuPixels[y * 320 + x] = color & 255;
    }
} display;

void showMenu()
{
    const A1stViewport viewport(settings.vgaFit);
    uint8_t row[A1stVideoWidth];
    for (unsigned y = 0; y < A1stVideoHeight; ++y) {
        if (!viewport.containsRow(y)) memset(row, 0, sizeof(row));
        else A1stFitMenuRow(row, menuPixels + viewport.sourceRow(y) * 320, viewport);
        uint8_t* destination = video.dmaBuffer->getLineAddr8(y);
        memcpy(destination, row, sizeof(row));
        Cache_WriteBack_Addr(reinterpret_cast<uint32_t>(destination), sizeof(row));
    }
    video.show();
}

void text(int x, int y, const char* value, uint8_t color = 255)
{
    display.setCursor(x, y);
    display.setTextColor(color);
    display.print(value);
}
void frame(const char* title)
{
    memset(menuPixels, 0, A1stVideoWidth * A1stVideoHeight);
    display.setTextSize(1);
    display.setTextWrap(false);
    text(8, 8, "ESP32 Clone Series - MSX TurboR");
    text(288, 8, FW_VERSION);
    display.drawFastHLine(8, 21, 304, 255);
    text(8, 29, title, 0xdf);
    display.drawFastHLine(8, 199, 304, 255);
}
void messageLines(const char* message)
{
    for (unsigned row = 0; row < 3 && *message; ++row) {
        char line[51];
        const size_t length = std::min<size_t>(50, strlen(message));
        memcpy(line, message, length);
        line[length] = 0;
        text(8, 206 + row * 10, line, 0xdf);
        message += length;
    }
}
void reportError(const char* message)
{
    if (message != status) snprintf(status, sizeof(status), "%s", message);
    Serial.printf("A1ST: %s\n", message);
    if (videoReady && menuPixels) {
        frame("A1ST message");
        messageLines(message);
        showMenu();
    }
}
[[noreturn]] void fatal(const char* message)
{
    reportError(message);
    while (true) delay(1000);
}
uint8_t waitKey()
{
    for (;;) {
        const uint8_t key = MsxKeyboardMenuKey();
        if (key) return key;
        delay(10);
    }
}
void backupScreen(bool restore)
{
    for (unsigned y = 0; y < 240; ++y) {
        uint8_t* line = video.dmaBuffer->getLineAddr8(y);
        if (restore) memcpy(line, backup + y * 320, 320);
        else memcpy(backup + y * 320, line, 320);
    }
    if (restore) video.show();
}
bool mountSd()
{
    if (running) {
        reportError("Stop the machine before remounting SD. Do not remove a mounted card.");
        return false;
    }
    char error[160];
    for (unsigned drive = 0; drive < 2; ++drive) a1stAttachDisk(drive, "", error, sizeof(error));
    SD_MMC.end();
    // Match the working Clone Series clock; the library defaults to 40 MHz.
    if (!SD_MMC.setPins(MsxBoard::SdClk, MsxBoard::SdCmd, MsxBoard::SdData) ||
        !SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_DEFAULT, 8)) {
        sdReady = false;
        reportError("SD/MMC mount failed. Check FAT32 card, seating and power.");
        return false;
    }
    sdReady = true;
    return true;
}
bool saveSettings()
{
    if (!nvsReady) {
        reportError("Cannot save A1ST boot settings to NVS.");
        return false;
    }
    if (!MsxJoystickHostPause()) {
        reportError("Cannot pause joystick USB for saving settings.");
        return false;
    }
    const bool saved = preferences.putBytes("boot", &settings, sizeof(settings)) == sizeof(settings);
    MsxJoystickHostResume();
    if (!saved) { reportError("Cannot save A1ST boot settings to NVS."); return false; }
    snprintf(status, sizeof(status), "A1ST boot configuration saved.");
    return true;
}
bool chooseFile(const char* title, const char* extension, bool eject,
                 char selection[MsxSdPathCapacity])
{
    char directory[MsxSdPathCapacity] = "/";
    if (selection[0]) {
        strcpy(directory, selection);
        MsxParentSdPath(directory);
    }
    size_t selected = 0, first = 0;
    MsxKeyboardClearEvents();
    for (;;) {
        MsxFileEntry entries[MsxBrowserPageSize];
        size_t count, total;
        char warning[160] = {};
        if (!MsxReadDirectoryPage(directory, extension, eject, first, entries, count, total,
                                  warning, sizeof(warning))) {
            reportError(warning);
            return false;
        }
        if (selected >= total && total) { selected = first = 0; continue; }
        frame(title);
        char line[64];
        snprintf(line, sizeof(line), "%.50s", directory);
        text(8, 40, line, 0xdf);
        for (size_t row = 0; row < count; ++row) {
            if (selected == first + row)
                display.fillRect(6, MsxMenuRowY(row), 308, 8, 0x48);
            const MsxFileEntry& entry = entries[row];
            const char* name = entry.kind == MsxFileKind::Eject ? "<Eject media>" : entry.name;
            snprintf(line, sizeof(line), "%s%.47s",
                     entry.kind == MsxFileKind::Directory ? "> " : "  ", name);
            text(8, MsxMenuRowY(row), line);
        }
        snprintf(line, sizeof(line), "Page %u/%u   %u entries",
                 static_cast<unsigned>(first / MsxBrowserPageSize + 1),
                 static_cast<unsigned>(std::max<size_t>(1, (total + 15) / 16)),
                 static_cast<unsigned>(total));
        text(8, 178, line);
        text(8, 188, "Enter: select  Left: up  PgUp/Dn  Esc: back");
        if (*warning) messageLines(warning);
        else if (count) messageLines(entries[selected - first].name);
        showMenu();
        const uint8_t key = waitKey();
        if (key == 41 || key == 69) return false;
        if (key == 76 && eject) { selection[0] = 0; return true; }
        if (key == 80) {
            MsxParentSdPath(directory);
            selected = first = 0;
        } else if (total && (key == 81 || key == 82 || key == 75 || key == 78)) {
            if (key == 81 || key == 82) selected = MsxMoveSelection(selected, key == 82 ? -1 : 1, total);
            else if (key == 75) selected = selected >= 16 ? selected - 16 : 0;
            else selected = std::min(selected + 16, total - 1);
            first = selected / 16 * 16;
        } else if (key == 40 && count) {
            const MsxFileEntry& entry = entries[selected - first];
            if (entry.kind == MsxFileKind::Eject) { selection[0] = 0; return true; }
            if (entry.kind == MsxFileKind::Parent) MsxParentSdPath(directory);
            else {
                char path[MsxSdPathCapacity];
                if (!MsxJoinSdPath(directory, entry.name, path, sizeof(path))) {
                    reportError("Invalid or overlong SD path.");
                    return false;
                }
                if (entry.kind == MsxFileKind::File) { strcpy(selection, path); return true; }
                strcpy(directory, path);
            }
            selected = first = 0;
        }
    }
}
bool flushState()
{
    return !running || a1stFlushState(status, sizeof(status));
}
bool coldBoot()
{
    if (!sdReady) { reportError("Mount the SD card before booting."); return false; }
    if (!flushState()) return false;
    if (running) { a1stDestroy(); running = false; }
    frame("BOOTING PANASONIC FS-A1ST");
    text(8, 53, "Loading BIOS, firmware and Kanji from SD...");
    text(8, 69, "256 KiB RAM / 128 KiB VRAM / Z80 + R800");
    showMenu();
    const A1stHost host = { reportError, MsxAudioSubmit };
    char firmware[160], kanji[160];
    snprintf(firmware, sizeof(firmware), "%s/fs-a1st_firmware.rom", BiosDirectory);
    snprintf(kanji, sizeof(kanji), "%s/fs-a1st_kanjifont.rom", BiosDirectory);
    if (!a1stCreate(firmware, kanji, BiosDirectory, &host, status, sizeof(status))) return false;
    a1stSetFirmwareEnabled(settings.firmware);
    for (unsigned slot = 0; slot < 2; ++slot) {
        char path[MsxSdPathCapacity + 8] = {};
        if (settings.cartridges[slot][0]) snprintf(path, sizeof(path), "/sdcard%s", settings.cartridges[slot]);
        if (!a1stAttachCartridge(slot, path, settings.mappers[slot], status, sizeof(status))) {
            a1stDestroy(); return false;
        }
    }
    for (unsigned drive = 0; drive < 2; ++drive) {
        char path[MsxSdPathCapacity + 8] = {};
        if (settings.disks[drive][0]) snprintf(path, sizeof(path), "/sdcard%s", settings.disks[drive]);
        if (!a1stAttachDisk(drive, path, status, sizeof(status))) { a1stDestroy(); return false; }
    }
    running = true;
    pacingReady = false;
    status[0] = 0;
    return true;
}
bool confirmUpdate(const char* path)
{
    bool yes = false;
    MsxKeyboardClearEvents();
    for (;;) {
        frame("Firmware update - confirmation");
        text(8, 53, "Install selected firmware and reboot?");
        text(8, 65, "Current machine state will be lost.");
        display.fillRect(6, yes ? 85 : 97, 308, 10, 0x48);
        text(8, 85, "Y  Yes - install and reboot");
        text(8, 97, "N  No  - cancel");
        text(8, 188, "Y/N or arrows + Enter. Esc/F12 cancels.");
        messageLines(path);
        showMenu();
        const uint8_t key = waitKey();
        if (key == 41 || key == 69 || key == 17 || (key == 40 && !yes)) return false;
        if (key == 28 || (key == 40 && yes)) return true;
        if (key >= 79 && key <= 82) yes = !yes;
    }
}
void firmwareProgress(const char* stage, uint8_t percent)
{
    frame("Firmware update");
    text(8, 53, stage);
    display.drawRect(8, 77, 304, 12, 255);
    display.fillRect(10, 79, 300 * percent / 100, 8, 0x1c);
    text(8, 188, "Do not power off or remove the SD card.");
    showMenu();
}
void firmwareUpdate()
{
    char path[MsxSdPathCapacity] = {};
    if (!sdReady) { reportError("Mount the SD card before updating firmware."); return; }
    if (!chooseFile("Firmware update - choose .FLH", ".flh", false, path) ||
        !confirmUpdate(path) || !flushState()) return;
    if (!MsxJoystickHostPause()) {
        reportError("Cannot pause joystick USB for firmware update.");
        return;
    }
    if (!MsxInstallFirmware(path, firmwareProgress, status, sizeof(status))) {
        MsxJoystickHostResume();
        reportError(status);
        return;
    }
    frame("Firmware update complete");
    text(8, 53, "Verified. Rebooting into selected firmware...");
    showMenu();
    delay(1500);
    ESP.restart();
}
const char* baseName(const char* path)
{
    const char* slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}
void selectMedia(unsigned item, bool eject)
{
    if (!sdReady) { reportError("Mount the SD card before selecting media."); return; }
    char* selection = item < DiskA ? settings.cartridges[item / 2] : settings.disks[item - DiskA];
    const char* extension = item < DiskA ? ".rom" : ".dsk";
    char candidate[MsxSdPathCapacity], path[MsxSdPathCapacity + 8] = {}, title[48];
    strcpy(candidate, selection);
    snprintf(title, sizeof(title), "%s %u - choose %s", item < DiskA ? "Cartridge slot" : "Disk drive",
             item < DiskA ? item / 2 + 1 : item - DiskA + 1, extension);
    if (eject) candidate[0] = 0;
    else if (!chooseFile(title, extension, true, candidate)) return;
    if (candidate[0]) snprintf(path, sizeof(path), "/sdcard%s", candidate);
    if (item < DiskA) {
        if (candidate[0]) {
            frame("INSPECTING CARTRIDGE ROM");
            text(8, 53, "Checking ROM header and automatic mapper...");
            messageLines(candidate);
            showMenu();
            int detected;
            if (!a1stInspectCartridge(path, &detected, status, sizeof(status))) return;
        }
        if (!candidate[0] || strcmp(candidate, selection)) settings.mappers[item / 2] = MsxMapperAuto;
    }
    else {
        if (!a1stAttachDisk(item - DiskA, path, status, sizeof(status))) return;
    }
    strcpy(selection, candidate);
    snprintf(status, sizeof(status), "%s %s. %s", item < DiskA ? "ROM" : "Disk", candidate[0] ? "selected" : "ejected",
             item < DiskA ? "Cold boot to apply; save to retain." : "Save to retain the selection.");
}
bool mediaMenu()
{
    unsigned selected = 0;
    for (;;) {
        frame("MEDIA - PANASONIC FS-A1ST");
        for (unsigned row = 0; row < MediaCount; ++row) {
            char label[64];
            if (row < DiskA) {
                unsigned slot = row / 2;
                if (!(row & 1)) snprintf(label, sizeof(label), "Slot %u: %.40s", slot + 1,
                    settings.cartridges[slot][0] ? baseName(settings.cartridges[slot]) : "<empty>");
                else snprintf(label, sizeof(label), "  Mapper: %s", MsxMapperName(settings.mappers[slot]));
            }
            else if (row <= DiskB) snprintf(label, sizeof(label), "Drive %c: %.39s", 'A' + row - DiskA,
                settings.disks[row - DiskA][0] ? baseName(settings.disks[row - DiskA]) : "<empty>");
            else snprintf(label, sizeof(label), "%s",
                row == MediaSaveBoot ? "Reboot and save configuration" : "Reboot without saving");
            if (row == selected) display.fillRect(6, MsxMenuRowY(row), 308, 8, 0x48);
            text(8, MsxMenuRowY(row), label);
        }
        text(8, 140, "ROM: cold boot. Disks: apply now.");
        text(8, 150, "Tape: use the normal MSX firmware.");
        text(8, 178, "Enter: choose  L/R: mapper  Del: eject/Auto");
        text(8, 188, "Esc/F12: back  Save includes all media.");
        messageLines(status);
        showMenu();
        const uint8_t key = waitKey();
        if (key == 41 || key == 69) { MsxKeyboardClearEvents(); return false; }
        if (key == 81 || key == 82) selected = MsxMoveSelection(selected, key == 82 ? -1 : 1, MediaCount);
        else if (selected < DiskA && (selected & 1) &&
                 (key == 40 || key == 79 || key == 80 || key == 76)) {
            unsigned slot = selected / 2;
            if (!settings.cartridges[slot][0]) reportError("Select a cartridge ROM first.");
            else {
                settings.mappers[slot] = key == 76 ? static_cast<uint8_t>(MsxMapperAuto) :
                    MsxMoveSelection(settings.mappers[slot], key == 80 ? -1 : 1, MsxMapperAuto + 1);
                snprintf(status, sizeof(status), "Mapper changed. Cold boot to apply; save to retain.");
            }
        }
        else if ((key == 40 || key == 76) && selected <= DiskB) selectMedia(selected, key == 76);
        else if (key == 40 && (selected == MediaSaveBoot || selected == MediaBoot)) {
            if (selected == MediaSaveBoot && !saveSettings()) continue;
            if (coldBoot()) return true;
        }
    }
}
bool sameJoystick(const MsxJoystickSnapshot& identity, const MsxJoystickSnapshot& current)
{
    return current.connected && identity.generation == current.generation &&
           identity.vid == current.vid && identity.pid == current.pid &&
           identity.endpoint == current.endpoint && identity.length == current.length;
}
void calibrateJoystick(unsigned port)
{
    MsxJoystickSnapshot identity;
    MsxJoystickSnapshotFor(port, identity);
    if (!identity.connected || !identity.length) {
        reportError("No joystick reports. Use a directly connected low-speed USB pad.");
        return;
    }
    const char* const poses[] = {
        "Release ALL controls", "Hold UP only", "Hold RIGHT only",
        "Hold DOWN only", "Hold LEFT only", "Hold button A only", "Hold button B only"
    };
    MsxJoystickSample samples[7] = {};
    for (unsigned pose = 0; pose < 7; ++pose) {
        char title[64];
        snprintf(title, sizeof(title), "Joystick %u calibration - %u/7", port + 1, pose + 1);
        frame(title);
        text(8, 53, poses[pose]);
        text(8, 69, "Enter: capture while holding. Esc/F12: cancel.");
        showMenu();
        MsxKeyboardClearEvents();
        for (;;) {
            MsxJoystickSnapshot current;
            MsxJoystickSnapshotFor(port, current);
            if (!sameJoystick(identity, current)) {
                reportError("Joystick changed; calibration cancelled.");
                return;
            }
            const uint8_t key = MsxKeyboardMenuKey();
            if (key == 41 || key == 69) { reportError("Calibration cancelled; mapping unchanged."); return; }
            if (key == 40) break;
            delay(10);
        }
        bool captured = false;
        uint8_t baseline[8] = {};
        const uint32_t start = millis();
        while (millis() - start < 600) {
            const uint8_t key = MsxKeyboardMenuKey();
            if (key == 41 || key == 69) { reportError("Calibration cancelled; mapping unchanged."); return; }
            MsxJoystickSnapshot current;
            MsxJoystickSnapshotFor(port, current);
            if (!sameJoystick(identity, current)) {
                reportError("Joystick changed during capture; retry.");
                return;
            }
            if (millis() - start >= 250) {
                if (!captured) memcpy(baseline, current.report, identity.length);
                captured = true;
                if (!pose)
                    for (unsigned byte = 0; byte < identity.length; ++byte)
                        samples[pose].changed[byte] |= baseline[byte] ^ current.report[byte];
                memcpy(samples[pose].bytes, current.report, identity.length);
            }
            delay(10);
        }
        if (!captured) { reportError("Joystick capture timed out; retry."); return; }
    }
    MsxJoystickMapping mapping = {};
    if (!MsxBuildJoystickCardinalMapping(identity.length, samples, mapping, status, sizeof(status)) ||
        !MsxJoystickSave(port, identity, mapping, status, sizeof(status))) {
        reportError(status);
        return;
    }
    Serial.println(status);
}
void joystickMenu()
{
    unsigned selected = 0;
    MsxKeyboardClearEvents();
    for (;;) {
        frame("USB JOYSTICKS - CALIBRATION / LIVE INPUT");
        text(8, 40, "Direct low-speed USB pads only.");
        for (unsigned port = 0; port < 2; ++port) {
            MsxJoystickSnapshot snapshot;
            MsxJoystickSnapshotFor(port, snapshot);
            const int y = 56 + port * 48;
            if (selected == port) display.fillRect(6, y, 308, 8, 0x48);
            char label[80];
            snprintf(label, sizeof(label), "Port %u: %s", port + 1, !snapshot.connected ? "disconnected" :
                     !snapshot.length ? "waiting for reports" : snapshot.calibrated ? "calibrated" : "needs calibration");
            text(8, y, label);
            snprintf(label, sizeof(label), "VID %04X PID %04X EP %02X len %u",
                     snapshot.vid, snapshot.pid, snapshot.endpoint, snapshot.length);
            text(8, y + 10, label);
            snprintf(label, sizeof(label), "U:%u D:%u L:%u R:%u A:%u B:%u",
                     !!(snapshot.buttons & 1), !!(snapshot.buttons & 2), !!(snapshot.buttons & 4),
                     !!(snapshot.buttons & 8), !!(snapshot.buttons & 16), !!(snapshot.buttons & 32));
            text(8, y + 20, label);
        }
        text(8, 164, "USB1 -> MSX port 1; USB2 -> MSX port 2.");
        text(8, 180, "Arrows: port  Enter: calibrate  Del: clear");
        text(8, 188, "Esc/F12: back. Calibration saves immediately.");
        messageLines(status);
        showMenu();
        const uint8_t key = MsxKeyboardMenuKey();
        if (key == 41 || key == 69) { MsxKeyboardClearEvents(); return; }
        if (key >= 79 && key <= 82) selected ^= 1;
        else if (key == 40) calibrateJoystick(selected);
        else if (key == 76 && !MsxJoystickClear(selected, status, sizeof(status))) reportError(status);
        delay(100);
    }
}
void present();
bool menu()
{
    const uint8_t previousFit = settings.vgaFit;
    if (running) {
        backupScreen(false);
        flushState();
    }
    MsxAudioEnable(false);
    MsxKeyboardClearEvents();
    size_t selected = 0;
    bool booted = false;
    for (;;) {
        frame("F12 CONFIGURATION - PANASONIC FS-A1ST");
        char labels[MenuCount][64];
        snprintf(labels[Resume], 64, "Resume emulation%s", running ? "" : " (not booted)");
        snprintf(labels[Machine], 64, "Machine: FS-A1ST / 256 KiB / V9958");
        snprintf(labels[Media], 64, "Media: ROM slots 1/2 and drives A/B");
        snprintf(labels[Sound], 64, "Sound: %s (PSG + MSX-MUSIC + PCM + SCC)", settings.sound ? "On" : "Off");
        snprintf(labels[Firmware], 64, "Built-in firmware: %s (cold boot)", settings.firmware ? "On" : "Off");
        snprintf(labels[AutoBoot], 64, "Auto boot: %s", settings.autoBoot ? "On" : "Off");
        snprintf(labels[DrawEvery], 64, "Present every %u frame(s)", settings.drawEvery);
        snprintf(labels[VgaFit], 64, "VGA fit: %u%% (both picture and menus)", settings.vgaFit);
        snprintf(labels[Save], 64, "Save boot configuration");
        snprintf(labels[Boot], 64, "Cold boot with selected configuration");
        snprintf(labels[Rescan], 64, "Rescan SD (stopped machine only)");
        snprintf(labels[Update], 64, "Firmware update / switch emulator");
        snprintf(labels[Joysticks], 64, "USB joysticks / calibration");
        for (unsigned row = 0; row < MenuCount; ++row) {
            if (selected == row) display.fillRect(6, 48 + row * 10, 308, 9, 0x48);
            text(8, 48 + row * 10, labels[row]);
        }
        text(8, 176, "Media: Enter  VGA fit: Left/Right");
        text(8, 188, "Arrows: choose  Enter: apply  Esc/F12: resume");
        messageLines(status);
        showMenu();
        const uint8_t key = waitKey();
        if ((key == 41 || key == 69 || (key == 40 && selected == Resume)) && running) break;
        if (key == 82 || key == 81) selected = MsxMoveSelection(selected, key == 82 ? -1 : 1, MenuCount);
        else if ((key == 40 || key == 79 || key == 80) && selected >= Sound && selected <= VgaFit) {
            if (selected == Sound) settings.sound = !settings.sound;
            if (selected == Firmware) settings.firmware = !settings.firmware;
            if (selected == AutoBoot) settings.autoBoot = !settings.autoBoot;
            if (selected == DrawEvery) settings.drawEvery = key == 80 ?
                (settings.drawEvery == 1 ? 6 : settings.drawEvery - 1) : settings.drawEvery % 6 + 1;
            if (selected == VgaFit) settings.vgaFit = key == 80 ?
                (settings.vgaFit == 80 ? 100 : settings.vgaFit - 5) :
                (settings.vgaFit == 100 ? 80 : settings.vgaFit + 5);
        } else if (key == 40 && selected == Media && mediaMenu()) { booted = true; break; }
        else if (key == 40 && selected == Save) saveSettings();
        else if (key == 40 && selected == Boot && coldBoot()) { booted = true; break; }
        else if (key == 40 && selected == Rescan) mountSd();
        else if (key == 40 && selected == Update) firmwareUpdate();
        else if (key == 40 && selected == Joysticks) joystickMenu();
    }
    if (running && !booted) {
        if (settings.vgaFit != previousFit) present();
        else backupScreen(true);
    }
    MsxKeyboardClearEvents();
    MsxAudioEnable(audioReady && settings.sound);
    pacingReady = false;
    return running;
}
void present()
{
    const uint16_t* pixels = a1stPixels();
    const A1stViewport viewport(settings.vgaFit);
    uint8_t row[320];
    for (unsigned y = 0; y < 240; ++y) {
        if (!viewport.containsRow(y)) memset(row, 0, sizeof(row));
        else {
            unsigned sourceY = viewport.sourceRow(y);
            A1stScaleVideoRow(row, pixels + sourceY * A1ST_FRAME_WIDTH, a1stLineWidth(sourceY), settings.vgaFit);
        }
        uint8_t* destination = video.dmaBuffer->getLineAddr8(y);
        memcpy(destination, row, sizeof(row));
        Cache_WriteBack_Addr(reinterpret_cast<uint32_t>(destination), sizeof(row));
    }
    ++presented;
}
void pace(const A1stStats& stats)
{
    int64_t now = esp_timer_get_time();
    if (!pacingReady) {
        wallAnchor = reportStart = lastYield = now;
        tickAnchor = reportTicks = stats.masterTicks;
        presented = 0;
        pacingReady = true;
    }
    const int64_t target = wallAnchor + (stats.masterTicks - tickAnchor) * 1000000 / 21477270;
    while (now < target) { delay(1); now = esp_timer_get_time(); lastYield = now; }
    if (now - lastYield >= 50000) { delay(1); now = esp_timer_get_time(); lastYield = now; }
    if (now - target > 20000) wallAnchor += now - target - 20000;
    const int64_t elapsed = now - reportStart;
    if (elapsed >= 5000000) {
        const double speed = (stats.masterTicks - reportTicks) * 100000000.0 / (21477270.0 * elapsed);
        Serial.printf("A1ST speed: %.1f%%, %.1f presented fps, CPU=%s DRAM=%u, PSRAM free=%u\n",
                      speed, presented * 1000000.0 / elapsed, stats.r800 ? "R800" : "Z80",
                      stats.dram, heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        reportStart = now; reportTicks = stats.masterTicks; presented = 0;
    }
}
}

void msxReportError(const char* message) { reportError(message); }

void setup()
{
    Serial.begin(115200);
    Serial.printf("\nESP32 MSX TurboR / A1ST %s\n", FW_VERSION);
    if (!psramFound()) fatal("8 MiB OPI PSRAM is required. Select the N16R8 board.");
    heap_caps_malloc_extmem_enable(16384);
    video.bufferCount = 1;
    const PinConfig pins(-1, -1, -1, MsxBoard::RedLow, MsxBoard::RedHigh,
                         -1, -1, -1, -1, MsxBoard::GreenLow, MsxBoard::GreenHigh,
                         -1, -1, -1, MsxBoard::BlueLow, MsxBoard::BlueHigh,
                         MsxBoard::HSync, MsxBoard::VSync);
    const Mode timing(8, 48, 24, A1stVideoWidth, 10, 2, 33, A1stVideoHeight,
                      12587500, 0, 0, A1stVideoLineRepeat);
    if (!video.init(pins, timing, 8)) fatal("VGA framebuffer allocation failed.");
    pinMode(MsxBoard::RgbLed, INPUT);
    neopixelWrite(MsxBoard::RgbLed, 0, 0, 0);
    pinMode(MsxBoard::RgbLed, INPUT);
    videoReady = video.start();
    if (!videoReady) fatal("VGA output could not start.");
    backup = static_cast<uint8_t*>(heap_caps_malloc(320 * 240, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!backup) fatal("Cannot allocate F12 screen backup.");
    menuPixels = static_cast<uint8_t*>(heap_caps_malloc(320 * 240, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!menuPixels) fatal("Cannot allocate F12 menu framebuffer.");
    frame("INITIALIZING PANASONIC FS-A1ST");
    text(8, 53, "Starting keyboard, audio and SD/MMC...");
    showMenu();
    if (!MsxKeyboardStart()) fatal("USB keyboard initialization failed. See UART.");
    audioReady = MsxAudioStart();
    if (!audioReady) reportError("Audio unavailable; emulation can continue without sound.");
    mountSd();
    nvsReady = preferences.begin("msxturbor", false);
    if (!nvsReady) reportError("NVS unavailable; boot settings will not persist.");
    else {
        const size_t length = preferences.getBytesLength("boot");
        if (length) {
            uint8_t saved[sizeof(A1stSettings)];
            if (length > sizeof(saved) ||
                preferences.getBytes("boot", saved, length) != length ||
                !A1stDecodeSettings(saved, length, settings))
                reportError("Invalid saved A1ST settings; using defaults without overwriting NVS.");
        }
    }
    if (!MsxJoysticksStart()) reportError("Joystick USB initialization failed. See UART.");
    frame("PANASONIC FS-A1ST READY");
    text(8, 53, "Press F12 for configuration and firmware updates.");
    messageLines(status);
    showMenu();
    bool interrupted = false;
    const uint32_t start = millis();
    while (millis() - start < 2500) {
        if (MsxKeyboardMenuKey() == 69) { interrupted = true; break; }
        delay(10);
    }
    if (!interrupted && settings.autoBoot && sdReady && coldBoot())
        MsxAudioEnable(audioReady && settings.sound);
}

void loop()
{
    if (!running) menu();
    if (!running) return;
    if (MsxKeyboardMenuKey() == 69) { menu(); return; }
    uint8_t matrix[16];
    MsxKeyboardMatrix(matrix);
    a1stSetKeyboard(matrix);
    a1stSetJoysticks(MsxJoysticksRead());
    a1stRunFrame();
    A1stStats stats;
    a1stGetStats(&stats);
    if (stats.frames % settings.drawEvery == 0) present();
    pace(stats);
}
