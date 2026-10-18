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

namespace
{
VGA video;
Preferences preferences;
A1stSettings settings;
bool videoReady, audioReady, sdReady, nvsReady, running;
uint8_t* backup;
char status[160] = {};
const char* const BiosDirectory = "/sdcard/msxturbor/bios/fs-a1st";
int64_t wallAnchor, reportStart, lastYield;
uint64_t tickAnchor, reportTicks;
uint32_t presented;
bool pacingReady;

class MenuDisplay : public Adafruit_GFX
{
public:
    MenuDisplay() : Adafruit_GFX(320, 240) {}
    void drawPixel(int16_t x, int16_t y, uint16_t color) override
    {
        if (x >= 0 && y >= 0 && x < width() && y < height()) video.dot(x, y, color & 255);
    }
} display;

void text(int x, int y, const char* value, uint8_t color = 255)
{
    display.setCursor(x, y);
    display.setTextColor(color);
    display.print(value);
}
void frame(const char* title)
{
    video.clear(0);
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
    snprintf(status, sizeof(status), "%s", message);
    Serial.printf("A1ST: %s\n", message);
    if (videoReady) {
        frame("A1ST message");
        messageLines(message);
        video.show();
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
    SD_MMC.end();
    if (!SD_MMC.setPins(MsxBoard::SdClk, MsxBoard::SdCmd, MsxBoard::SdData) ||
        !SD_MMC.begin("/sdcard", true)) {
        sdReady = false;
        reportError("SD/MMC mount failed. Check FAT32 card, seating and power.");
        return false;
    }
    sdReady = true;
    return true;
}
bool saveSettings()
{
    if (!nvsReady || preferences.putBytes("boot", &settings, sizeof(settings)) != sizeof(settings)) {
        reportError("Cannot save A1ST boot settings to NVS.");
        return false;
    }
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
            const char* name = entry.kind == MsxFileKind::Eject ? "<Eject disk>" : entry.name;
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
        video.show();
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
    video.show();
    const A1stHost host = { reportError, MsxAudioSubmit };
    char firmware[160], kanji[160];
    snprintf(firmware, sizeof(firmware), "%s/fs-a1st_firmware.rom", BiosDirectory);
    snprintf(kanji, sizeof(kanji), "%s/fs-a1st_kanjifont.rom", BiosDirectory);
    if (!a1stCreate(firmware, kanji, BiosDirectory, &host, status, sizeof(status))) return false;
    a1stSetFirmwareEnabled(settings.firmware);
    char path[MsxSdPathCapacity + 8] = {};
    if (settings.disk[0]) snprintf(path, sizeof(path), "/sdcard%s", settings.disk);
    if (!a1stMountDisk(path, status, sizeof(status))) { a1stDestroy(); return false; }
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
        video.show();
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
    video.show();
}
void firmwareUpdate()
{
    char path[MsxSdPathCapacity] = {};
    if (!sdReady) { reportError("Mount the SD card before updating firmware."); return; }
    if (!chooseFile("Firmware update - choose .FLH", ".flh", false, path) ||
        !confirmUpdate(path) || !flushState()) return;
    if (!MsxInstallFirmware(path, firmwareProgress, status, sizeof(status))) return;
    frame("Firmware update complete");
    text(8, 53, "Verified. Rebooting into selected firmware...");
    video.show();
    delay(1500);
    ESP.restart();
}
void selectDisk()
{
    char candidate[MsxSdPathCapacity], path[MsxSdPathCapacity + 8] = {};
    strcpy(candidate, settings.disk);
    if (!chooseFile("Drive A - raw .DSK", ".dsk", true, candidate)) return;
    if (candidate[0]) snprintf(path, sizeof(path), "/sdcard%s", candidate);
    if (!a1stMountDisk(path, status, sizeof(status))) return;
    strcpy(settings.disk, candidate);
    snprintf(status, sizeof(status), "Drive A %s. Disk writes persist immediately.",
             candidate[0] ? "attached" : "ejected");
}
bool menu()
{
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
        char labels[11][64];
        snprintf(labels[0], 64, "Resume emulation%s", running ? "" : " (not booted)");
        snprintf(labels[1], 64, "Machine: FS-A1ST / 256 KiB / V9958");
        snprintf(labels[2], 64, "Drive A: %.39s", settings.disk[0] ? settings.disk : "<empty>");
        snprintf(labels[3], 64, "Sound: %s (PSG + MSX-MUSIC + PCM)", settings.sound ? "On" : "Off");
        snprintf(labels[4], 64, "Built-in firmware: %s (cold boot)", settings.firmware ? "On" : "Off");
        snprintf(labels[5], 64, "Auto boot: %s", settings.autoBoot ? "On" : "Off");
        snprintf(labels[6], 64, "Present every %u frame(s)", settings.drawEvery);
        snprintf(labels[7], 64, "Save boot configuration");
        snprintf(labels[8], 64, "Cold boot with selected configuration");
        snprintf(labels[9], 64, "Rescan SD (stopped machine only)");
        snprintf(labels[10], 64, "Firmware update / switch emulator");
        for (unsigned row = 0; row < 11; ++row) {
            if (selected == row) display.fillRect(6, 48 + row * 11, 308, 10, 0x48);
            text(8, 48 + row * 11, labels[row]);
        }
        text(8, 176, "Disk: Enter browse / Delete eject");
        text(8, 188, "Arrows: choose  Enter: apply  Esc/F12: resume");
        messageLines(status);
        video.show();
        const uint8_t key = waitKey();
        if ((key == 41 || key == 69 || (key == 40 && selected == 0)) && running) break;
        if (key == 82 || key == 81) selected = MsxMoveSelection(selected, key == 82 ? -1 : 1, 11);
        else if ((key == 40 || key == 79 || key == 80) && selected >= 3 && selected <= 6) {
            if (selected == 3) settings.sound = !settings.sound;
            if (selected == 4) settings.firmware = !settings.firmware;
            if (selected == 5) settings.autoBoot = !settings.autoBoot;
            if (selected == 6) settings.drawEvery = key == 80 ?
                (settings.drawEvery == 1 ? 6 : settings.drawEvery - 1) : settings.drawEvery % 6 + 1;
        } else if (key == 40 && selected == 2) selectDisk();
        else if (key == 76 && selected == 2) {
            if (a1stMountDisk("", status, sizeof(status))) settings.disk[0] = 0;
        } else if (key == 40 && selected == 7) saveSettings();
        else if (key == 40 && selected == 8 && coldBoot()) { booted = true; break; }
        else if (key == 40 && selected == 9) mountSd();
        else if (key == 40 && selected == 10) firmwareUpdate();
    }
    if (running && !booted) backupScreen(true);
    MsxKeyboardClearEvents();
    MsxAudioEnable(audioReady && settings.sound);
    pacingReady = false;
    return running;
}
void present()
{
    const uint16_t* pixels = a1stPixels();
    uint8_t row[320];
    for (unsigned y = 0; y < 240; ++y) {
        A1stScaleVideoRow(row, pixels + y * A1ST_FRAME_WIDTH, a1stLineWidth(y));
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
    const Mode timing(8, 48, 24, 320, 10, 2, 33, 240, 12587500, 0, 0, 2);
    if (!video.init(pins, timing, 8)) fatal("VGA framebuffer allocation failed.");
    pinMode(MsxBoard::RgbLed, INPUT);
    neopixelWrite(MsxBoard::RgbLed, 0, 0, 0);
    pinMode(MsxBoard::RgbLed, INPUT);
    videoReady = video.start();
    if (!videoReady) fatal("VGA output could not start.");
    backup = static_cast<uint8_t*>(heap_caps_malloc(320 * 240, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!backup) fatal("Cannot allocate F12 screen backup.");
    frame("INITIALIZING PANASONIC FS-A1ST");
    text(8, 53, "Starting keyboard, audio and SD/MMC...");
    video.show();
    if (!MsxKeyboardStart()) fatal("USB keyboard initialization failed. See UART.");
    audioReady = MsxAudioStart();
    if (!audioReady) reportError("Audio unavailable; emulation can continue without sound.");
    mountSd();
    nvsReady = preferences.begin("msxturbor", false);
    if (!nvsReady) reportError("NVS unavailable; boot settings will not persist.");
    else {
        const size_t length = preferences.getBytesLength("boot");
        if (length) {
            A1stSettings saved;
            if (length != sizeof(saved) ||
                preferences.getBytes("boot", &saved, sizeof(saved)) != sizeof(saved) ||
                !A1stDecodeSettings(&saved, sizeof(saved), settings))
                reportError("Invalid saved A1ST settings; using defaults without overwriting NVS.");
        }
    }
    frame("PANASONIC FS-A1ST READY");
    text(8, 53, "Press F12 for configuration and firmware updates.");
    messageLines(status);
    video.show();
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
    a1stRunFrame();
    A1stStats stats;
    a1stGetStats(&stats);
    if (stats.frames % settings.drawEvery == 0) present();
    pace(stats);
}
