#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Preferences.h>
#include <SD_MMC.h>
#include <VGA.h>
#include <esp_heap_caps.h>
#include <algorithm>
#include "MacBoard.h"
#include "MacSettings.h"
#include "MacKeyboard.h"
#include "MacMouseHost.h"
#include "MacInput.h"
#include "MacAudio.h"
#include "MacFirmwareUpdate.h"
#include "MacScsi.h"
#include "MacDiskActivity.h"
#include "MacFloppy.h"
#include "MacVideo.h"
#include "MacRom.h"
extern "C" {
#include "tme/emu.h"
#include "tme/tmeconfig.h"
#include "tme/via.h"
#include "tme/rtc.h"
#include "tme/mouse.h"
#include "tme/iwm.h"
}

static VGA video;
static Preferences preferences;
static MacSettings settings;
static MacDisk disk;
static MacDiskActivity::State diskActivity;
static MacDiskActivity::State floppyActivity;
static MacFloppy floppy;
static char floppyPath[MacPathSize] = {};
static MacMouseState mouseInput;
static portMUX_TYPE mouseLock = portMUX_INITIALIZER_UNLOCKED;
static bool acceptMouse = false, mouseConnected = false;
static bool sdReady = false, nvsReady = false, running = false, audioReady = false;
static bool machineFault = false, pramDirty = false;
static uint8_t *romData = nullptr, *ramData = nullptr;
static char pram[32] = {}, status[160] = {};

class Display : public Adafruit_GFX
{
public:
    Display() : Adafruit_GFX(320, 240) {}
    void drawPixel(int16_t x, int16_t y, uint16_t color) override
    {
        if (x < 0 || x >= 320 || y < 0 || y >= 240) return;
        for (unsigned dy = 0; dy < 2; ++dy)
            for (unsigned dx = 0; dx < 2; ++dx) video.dot(x * 2 + dx, y * 2 + dy, color & 255);
    }
};
static Display display;

static bool error(const char *message)
{
    snprintf(status, sizeof(status), "%s", message);
    Serial.printf("MACPLUS: %s\n", message);
    return false;
}

void macReportError(const char *message)
{
    error(message);
    machineFault = true;
}

extern "C" void saveRtcMem(char *mem)
{
    memcpy(pram, mem, sizeof(pram));
    pramDirty = true;
}

static void text(int x, int y, const char *value, uint8_t color = 255)
{
    display.setCursor(x, y);
    display.setTextColor(color);
    display.print(value);
}

static void frame(const char *title)
{
    video.clear(0);
    display.setTextSize(1);
    display.setTextWrap(false);
    text(8, 8, "ESP32 Clone Series - Macintosh Plus");
    text(284, 29, FW_VERSION);
    display.drawFastHLine(8, 21, 304, 255);
    text(8, 29, title, 0xdf);
    display.drawFastHLine(8, 199, 304, 255);
}

static void footer(const char *hint = "Arrows: select  Enter: apply  Esc: back")
{
    text(8, 187, hint);
    const size_t length = strlen(status);
    for (size_t offset = 0; offset < length && offset < 150; offset += 50) {
        char line[51] = {};
        memcpy(line, status + offset, std::min<size_t>(50, length - offset));
        text(8, 205 + offset / 50 * 10, line, 0xdf);
    }
    video.show();
}

static void row(unsigned index, bool selected, const char *label, unsigned spacing = 12)
{
    if (selected) display.fillRect(6, 46 + index * spacing, 308, 10, 0x48);
    text(8, 47 + index * spacing, label);
}

static uint8_t key()
{
    for (;;) {
        const uint8_t value = MacKeyboardMenuKey();
        if (value) return value;
        delay(10);
    }
}

static bool confirm(const char *title)
{
    frame(title);
    text(8, 60, "Current Macintosh state may be lost.");
    text(8, 84, "Y: confirm     N / Esc / F12: cancel");
    footer("");
    MacKeyboardClearEvents();
    for (;;) {
        const uint8_t value = key();
        if (value == 28) return true;
        if (value == 17 || value == 41 || value == 69) return false;
    }
}

static bool mountSd()
{
    if (sdReady) return true;
    SD_MMC.setPins(MacBoard::SdClk, MacBoard::SdCmd, MacBoard::SdData);
    sdReady = SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_DEFAULT, 5);
    return sdReady || error("SD mount failed. Insert a FAT32 card and retry.");
}

static const char *baseName(const char *path)
{
    const char *last = strrchr(path, '/');
    return last ? last + 1 : path;
}

static bool browse(const char *title, const char *extension, char result[MacPathSize],
                   const char *alternateExtension = nullptr)
{
    if (!mountSd()) return false;
    char directory[MacPathSize] = "/";
    unsigned selected = 0;
    MacKeyboardClearEvents();
    for (;;) {
        struct Entry { char name[MacPathSize]; bool directory; };
        Entry entries[10] = {};
        const unsigned first = selected / 10 * 10;
        unsigned count = 0, total = 0, visited = 0;
        File folder = SD_MMC.open(directory);
        if (!folder || !folder.isDirectory()) return error("Cannot read SD directory.");
        if (strcmp(directory, "/")) {
            if (!first) { strcpy(entries[count].name, ".."); entries[count++].directory = true; }
            ++total;
        }
        for (File file = folder.openNextFile(); file; file = folder.openNextFile()) {
            if (++visited % 16 == 0) delay(1);
            const char *name = baseName(file.name());
            if (!strcmp(name, ".") || !strcmp(name, "..") || !strcasecmp(name, "System Volume Information")) continue;
            const size_t length = strlen(name);
            const auto matches = [name, length](const char *suffix) {
                if (!suffix) return false;
                const size_t size = strlen(suffix);
                return length > size && !strcasecmp(name + length - size, suffix);
            };
            if (!file.isDirectory() && !matches(extension) && !matches(alternateExtension)) continue;
            char path[MacPathSize];
            const int used = snprintf(path, sizeof(path), "%s%s%s", directory, strcmp(directory, "/") ? "/" : "", name);
            if (used < 0 || unsigned(used) >= sizeof(path) || !MacValidPath(path)) {
                Serial.printf("MACPLUS browser: invalid/overlong path: %s\n", name);
                continue;
            }
            if (total >= first && count < 10) {
                snprintf(entries[count].name, MacPathSize, "%s", name);
                entries[count++].directory = file.isDirectory();
            }
            ++total;
        }
        folder.close();
        if (selected >= total && selected) { selected = total ? total - 1 : 0; continue; }
        frame(title);
        for (unsigned i = 0; i < count; ++i) {
            char label[52];
            snprintf(label, sizeof(label), "%s%.46s", entries[i].directory ? "> " : "  ", entries[i].name);
            row(i, selected == first + i, label);
        }
        if (!total) text(8, 55, "No matching files.");
        snprintf(status, sizeof(status), "%.150s", directory);
        footer("Enter: open  PgUp/PgDn: page  Esc: cancel");
        const uint8_t value = key();
        if (value == 41 || value == 69) return false;
        if (value == 82 && selected) --selected;
        else if (value == 81 && selected + 1 < total) ++selected;
        else if (value == 75) selected = selected >= 10 ? selected - 10 : 0;
        else if (value == 78 && total) selected = std::min(selected + 10, total - 1);
        else if (value == 40 && count) {
            const Entry &entry = entries[selected - first];
            if (!strcmp(entry.name, "..")) {
                char *slash = strrchr(directory, '/');
                if (slash == directory) directory[1] = 0;
                else *slash = 0;
                selected = 0;
            } else {
                char path[MacPathSize];
                const int used = snprintf(path, sizeof(path), "%s%s%s", directory, strcmp(directory, "/") ? "/" : "", entry.name);
                if (used < 0 || unsigned(used) >= sizeof(path)) return error("SD path is too long.");
                if (entry.directory) { strcpy(directory, path); selected = 0; }
                else { strcpy(result, path); status[0] = 0; return true; }
            }
        }
    }
}

static bool persistPram()
{
    if (!pramDirty) return true;
    if (!nvsReady) return error("PRAM not saved: NVS is unavailable.");
    if (!MacMouseHostPause()) return error("Cannot pause USB mouse for PRAM save.");
    const bool saved = preferences.putBytes("pram", pram, sizeof(pram)) == sizeof(pram);
    MacMouseHostResume();
    if (!saved) return error("PRAM save failed.");
    pramDirty = false;
    return true;
}

static bool saveSettings()
{
    if (!nvsReady) return error("Settings not saved: NVS is unavailable.");
    if (!MacMouseHostPause()) return error("Cannot pause USB mouse for settings save.");
    const bool saved = preferences.putBytes("boot", &settings, sizeof(settings)) == sizeof(settings);
    MacMouseHostResume();
    if (!saved) return error("Settings save failed.");
    snprintf(status, sizeof(status), "Complete boot configuration saved.");
    return persistPram();
}

static bool boot()
{
    MacAudioEnable(false);
    running = false;
    machineFault = false;
    diskActivity.clear();
    disk.takeActivity();
    floppyActivity.clear();
    floppy.close();
    floppyPath[0] = 0;
    if (!mountSd()) return false;
    File file = SD_MMC.open(settings.rom, FILE_READ);
    if (!file || file.isDirectory() || file.size() != TME_ROMSIZE)
        return error("Supply an exact 128 KiB Macintosh Plus ROM in F12.");
    uint8_t *candidate = static_cast<uint8_t *>(heap_caps_malloc(TME_ROMSIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!candidate) return error("Not enough PSRAM for the Macintosh ROM.");
    const bool read = file.read(candidate, TME_ROMSIZE) == TME_ROMSIZE;
    file.close();
    if (!read) { free(candidate); return error("Cannot read the complete Macintosh ROM."); }
    if (!MacValidRom(candidate, TME_ROMSIZE)) {
        free(candidate);
        return error("Invalid Plus ROM checksum/reset vector. Use an unmodified 128 KiB ROM.");
    }
    if (!ramData) ramData = static_cast<uint8_t *>(heap_caps_malloc(TME_RAMSIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!ramData) { free(candidate); return error("Not enough PSRAM for 4 MiB Macintosh RAM."); }
    char path[MacPathSize + 8];
    snprintf(path, sizeof(path), "/sdcard%s", settings.disk);
    if (settings.disk[0] && !disk.open(path, settings.readOnly, status, sizeof(status))) {
        free(candidate);
        Serial.println(status);
        return false;
    }
    if (!settings.disk[0]) disk.close();
    if (settings.disk[0])
        Serial.printf("MACPLUS: SCSI 6: %s (%u sectors, %s)\n", settings.disk,
                      unsigned(disk.blocks()), settings.readOnly ? "read-only" : "read/write");
    else Serial.println("MACPLUS: SCSI 6: no disk attached.");
    free(romData);
    romData = candidate;
    rtcInit(pram);
    tmeInit(romData, ramData);
    MacKeyboardReset();
    MacAttachScsi(settings.disk[0] ? &disk : nullptr);
    running = true;
    status[0] = 0;
    return true;
}

static void progress(const char *stage, uint8_t percent)
{
    frame("Firmware update - keep power connected");
    text(8, 58, stage);
    display.drawRect(8, 78, 304, 14, 255);
    display.fillRect(10, 80, 300 * percent / 100, 10, 0xdf);
    video.show();
}

static void firmwareUpdate()
{
    char path[MacPathSize] = {};
    if (!browse("Select firmware .FLH", ".flh", path) || !confirm("Install firmware and restart board?")) return;
    if (!persistPram()) return;
    if (!MacMouseHostPause()) { error("Cannot pause USB mouse for firmware update."); return; }
    const bool installed = MacInstallFirmware(path, progress, status, sizeof(status));
    MacMouseHostResume();
    if (!installed) return;
    frame("Firmware verified. Rebooting...");
    video.show();
    delay(1000);
    ESP.restart();
}

static void mouseEnabled(bool enabled)
{
    portENTER_CRITICAL(&mouseLock);
    acceptMouse = enabled;
    mouseInput.reset();
    portEXIT_CRITICAL(&mouseLock);
}

static void presentDiskActivity(bool force = false)
{
    static uint8_t previous = 255;
    const uint32_t now = millis();
    const uint8_t drives = (floppyActivity.active(now) ? MacDiskActivity::Floppy : 0) |
                           (diskActivity.active(now) ? MacDiskActivity::Scsi : 0);
    const bool bright = !(now & 128);
    const uint8_t state = drives | (drives && bright ? 4 : 0);
    if (!force && previous == state) return;
    previous = state;
    for (unsigned y = 0; y < MacDiskActivity::Height; ++y) {
        uint8_t *line = video.dmaBuffer->getLineAddr8(MacDiskActivity::Y + y) + MacDiskActivity::X;
        for (unsigned x = 0; x < MacDiskActivity::Width; ++x)
            line[x] = MacDiskActivity::pixel(x, y, drives, bright);
        video.dmaBuffer->flush(0, MacDiskActivity::Y + y);
    }
}

static bool ejectFloppy()
{
    if (!floppy.inserted()) return error("No floppy is inserted.");
    Serial.printf("MACPLUS: host ejected floppy %s\n", floppyPath);
    floppy.close();
    floppyPath[0] = 0;
    floppyActivity.clear();
    snprintf(status, sizeof(status), "Floppy ejected; resuming Macintosh.");
    return true;
}

static bool floppyMenu()
{
    if (!running) return error("Boot the Macintosh from SCSI before inserting a floppy.");
    for (;;) {
        frame("Floppy drive - read-only");
        char label[52];
        snprintf(label, sizeof(label), "Inserted: %.40s", floppy.inserted() ? baseName(floppyPath) : "None");
        text(8, 50, label);
        text(8, 70, "Raw 400 KB / 800 KB .dsk and .img only.");
        text(8, 90, "Enter: browse and insert a floppy.");
        text(8, 104, "Delete: eject and resume Macintosh.");
        text(8, 124, "Prefer Eject in the Macintosh first.");
        text(8, 140, "Disks are cached; images never change.");
        text(8, 156, "Floppies are not saved in boot settings.");
        footer("Enter: insert  Del: eject  Esc: back");
        const uint8_t value = key();
        if (value == 41 || value == 69) return false;
        if (value == 76 && ejectFloppy()) return true;
        if (value != 40) continue;
        if (floppy.inserted()) {
            error("Eject the current floppy before inserting another.");
            continue;
        }
        char path[MacPathSize] = {}, nativePath[MacPathSize + 8];
        if (!browse("Insert read-only raw floppy", ".dsk", path, ".img")) continue;
        snprintf(nativePath, sizeof(nativePath), "/sdcard%s", path);
        if (!floppy.open(nativePath, status, sizeof(status))) {
            Serial.printf("MACPLUS: %s\n", status);
            continue;
        }
        strcpy(floppyPath, path);
        Serial.printf("MACPLUS: inserted floppy %s (read-only)\n", path);
        status[0] = 0;
        return true;
    }
}

static void menu()
{
    MacAudioEnable(false);
    diskActivity.clear();
    disk.takeActivity();
    floppyActivity.clear();
    iwmTakeActivity();
    MacKeyboardEmulation(false);
    mouseEnabled(false);
    mouseMove(0, 0, 0);
    MacKeyboardClearEvents();
    unsigned selected = 0;
    for (;;) {
        frame("F12 - Macintosh Plus");
        constexpr unsigned rows = 12;
        char labels[rows][64];
        snprintf(labels[0], 64, "%s", running ? "Resume Macintosh" : "Boot Macintosh");
        snprintf(labels[1], 64, "ROM: %.43s", baseName(settings.rom));
        snprintf(labels[2], 64, "SCSI 6: %.40s", settings.disk[0] ? baseName(settings.disk) : "No disk");
        snprintf(labels[3], 64, "Floppy: %.43s", floppy.inserted() ? baseName(floppyPath) : "Empty (read-only)");
        snprintf(labels[4], 64, "Disk access: %s (next boot)", settings.readOnly ? "Read-only" : "Read/write");
        snprintf(labels[5], 64, "Sound: %s", settings.sound ? "On" : "Off");
        snprintf(labels[6], 64, "Volume: %u%%", settings.volume);
        snprintf(labels[7], 64, "Auto boot: %s / render 1/%u", settings.autoBoot ? "On" : "Off", settings.frameSkip + 1);
        snprintf(labels[8], 64, "Save boot configuration");
        snprintf(labels[9], 64, "Cold boot selected configuration");
        snprintf(labels[10], 64, "Firmware update (any emulator .FLH)");
        snprintf(labels[11], 64, "Hardware / keyboard / mouse help");
        for (unsigned i = 0; i < rows; ++i) row(i, i == selected, labels[i], 11);
        footer("Enter: apply  L/R: change  Del: eject disk");
        const uint8_t value = key();
        if ((value == 41 || value == 69) && running) break;
        if (value == 82) selected = (selected + rows - 1) % rows;
        else if (value == 81) selected = (selected + 1) % rows;
        else if (value == 76 && selected == 2) settings.disk[0] = 0;
        else if (value == 76 && selected == 3 && ejectFloppy()) break;
        else if (value == 40 || value == 79 || value == 80) {
            const bool enter = value == 40;
            if (selected == 0 && enter) {
                if (running || boot()) break;
            } else if ((selected == 1 || selected == 2) && enter) {
                char path[MacPathSize] = {};
                if (browse(selected == 1 ? "Select Macintosh Plus ROM" : "Select whole SCSI disk image",
                           selected == 1 ? ".rom" : ".img", path))
                    strcpy(selected == 1 ? settings.rom : settings.disk, path);
            } else if (selected == 3 && enter) {
                if (floppyMenu()) break;
            } else if (selected == 4) settings.readOnly ^= 1;
            else if (selected == 5) settings.sound ^= 1;
            else if (selected == 6) {
                const int next = int(settings.volume) + (value == 80 ? -10 : 10);
                settings.volume = std::max(0, std::min(100, next));
            } else if (selected == 7) {
                if (enter) settings.autoBoot ^= 1;
                else settings.frameSkip = (settings.frameSkip + (value == 80 ? 3 : 1)) % 4;
            } else if (selected == 8 && enter) saveSettings();
            else if (selected == 9 && enter && confirm("Cold boot selected configuration?")) {
                if (persistPram() && boot()) break;
            } else if (selected == 10 && enter) firmwareUpdate();
            else if (selected == 11 && enter) {
                frame("Hardware / keyboard / mouse");
                text(8, 48, "4 MiB Mac Plus / 512x342 native display.");
                text(8, 62, "Native USB keyboard: D-19 / D+20.");
                text(8, 76, "Mouse: joystick PORT 2, D-17 / D+18.");
                text(8, 90, "Low-speed HID BOOT mice only. No hubs.");
                text(8, 104, "GUI keys: Command. Alt: Option.");
                text(8, 118, "Left mouse button; wheel ignored.");
                text(8, 132, "ROM / SCSI changes need cold boot.");
                text(8, 146, "Floppy: 400/800 KB raw, read-only.");
                text(8, 160, "F12 pauses CPU, sound and disk access.");
                portENTER_CRITICAL(&mouseLock);
                const bool connected = mouseConnected;
                portEXIT_CRITICAL(&mouseLock);
                text(8, 174, connected ? "Port 2 mouse: connected" : "Port 2 mouse: disconnected");
                footer("Enter / Esc / F12: back");
                key();
            }
        }
    }
    MacKeyboardClearEvents();
    machineFault = false;
    MacKeyboardClearEvents();
    MacKeyboardEmulation(true);
    mouseEnabled(true);
    video.clear(0);
    presentDiskActivity(true);
    MacAudioEnable(audioReady && settings.sound);
}

static void present()
{
    const uint8_t *pixels = tmeFramebuffer();
    for (unsigned y = 69; y < 411; ++y) {
        uint8_t line[640];
        MacRenderLine(pixels, y, line);
        memcpy(video.dmaBuffer->getLineAddr8(y), line, sizeof(line));
        video.dmaBuffer->flush(0, y);
    }
    presentDiskActivity(true);
    video.show();
}

static void fatal(const char *message)
{
    Serial.println(message);
    for (;;) delay(1000);
}

void setup()
{
    Serial.begin(115200);
    Serial.printf("\nESP32 Macintosh Plus %s\n", FW_VERSION);
    if (!psramFound()) fatal("8 MiB OPI PSRAM required.");
    video.bufferCount = 1;
    const PinConfig pins(-1,-1,-1,5,4, -1,-1,-1,-1,7,6, -1,-1,-1,9,8, 2,1);
    const Mode timing(16,96,48,640, 10,2,33,480, 25175000,0,0,1);
    if (!video.init(pins, timing, 8)) fatal("VGA allocation failed.");
    neopixelWrite(MacBoard::RgbLed, 0, 0, 0);
    pinMode(MacBoard::RgbLed, INPUT);
    if (!video.start()) fatal("VGA start failed.");
    frame("Starting shared board hardware...");
    footer("");
    if (!MacKeyboardStart()) fatal("Native USB keyboard host failed; see UART.");
    audioReady = MacAudioStart();
    if (!audioReady) error("Audio initialization failed; see UART.");
    mountSd();
    nvsReady = preferences.begin("macplus", false);
    bool validBoot = nvsReady;
    if (!nvsReady) error("NVS unavailable; automatic boot disabled.");
    else {
        const size_t size = preferences.getBytesLength("boot");
        if (size) {
            MacSettings stored;
            if (size != sizeof(stored) || preferences.getBytes("boot", &stored, size) != size ||
                !MacDecodeSettings(&stored, size, settings)) {
                validBoot = false;
                error("Invalid saved settings; select configuration in F12.");
            }
        }
        const size_t pramSize = preferences.getBytesLength("pram");
        if (pramSize && (pramSize != sizeof(pram) ||
            preferences.getBytes("pram", pram, sizeof(pram)) != sizeof(pram))) {
            memset(pram, 0, sizeof(pram));
            error("Invalid saved PRAM; using cleared PRAM.");
        }
    }
    if (!MacMouseHostStart(
        [](unsigned port, uint16_t vid, uint16_t pid, bool connected) {
            portENTER_CRITICAL(&mouseLock);
            mouseConnected = port == 1 && connected;
            mouseInput.reset();
            portEXIT_CRITICAL(&mouseLock);
            Serial.printf("MACPLUS mouse port %u: %04X:%04X %s\n", port + 1, vid, pid, connected ? "connected" : "disconnected");
        },
        [](unsigned port, uint8_t, const uint8_t *data, size_t length) {
            if (port != 1) return;
            portENTER_CRITICAL(&mouseLock);
            const bool valid = !acceptMouse || mouseInput.report(data, length);
            portEXIT_CRITICAL(&mouseLock);
            if (!valid) Serial.printf("MACPLUS: malformed boot mouse report (%u bytes)\n", unsigned(length));
        })) error("USB mouse host initialization failed; see UART.");
    frame("F12: setup. Automatic boot in 2.5 seconds.");
    footer("");
    const uint32_t start = millis();
    bool interrupted = false;
    while (millis() - start < 2500) {
        if (MacKeyboardMenuKey() == 69) { interrupted = true; break; }
        delay(10);
    }
    if (!interrupted && validBoot && settings.autoBoot && sdReady) boot();
    if (running) video.clear(0);
    MacKeyboardEmulation(running);
    mouseEnabled(running);
    MacAudioEnable(audioReady && settings.sound && running);
}

void loop()
{
    static uint32_t frames = 0, lastPram = millis(), lastReport = millis(), measuredFrames = 0;
    bool openMenu = !running || machineFault;
    for (uint8_t value = MacKeyboardMenuKey(); value; value = MacKeyboardMenuKey())
        if (value == 69) openMenu = true;
    if (openMenu) menu();
    const uint32_t start = micros();
    uint8_t scan;
    while (MacKeyboardCharacter(scan)) kbdPushKey(scan & 0x7f, scan & 0x80);
    int dx, dy;
    bool button;
    portENTER_CRITICAL(&mouseLock);
    mouseInput.take(dx, dy, button);
    portEXIT_CRITICAL(&mouseLock);
    mouseMove(dx, dy, button);
    tmeRunFrame();
    if (disk.takeActivity()) diskActivity.record(millis());
    if (iwmTakeActivity()) floppyActivity.record(millis());
    if (floppy.hasImage() && !floppy.inserted()) {
        Serial.printf("MACPLUS: Macintosh ejected floppy %s\n", floppyPath);
        floppy.close();
        floppyPath[0] = 0;
        floppyActivity.clear();
    }
    if (frames++ % (settings.frameSkip + 1) == 0) present();
    else presentDiskActivity();
    if (audioReady && settings.sound) {
        int16_t samples[370];
        tmeAudio(samples, settings.volume);
        MacAudioSubmit(samples, 370);
    }
    const uint32_t elapsed = micros() - start;
    if (elapsed < 16625) {
        const uint32_t remaining = 16625 - elapsed;
        if (remaining >= 1000) delay(remaining / 1000);
        const uint32_t paced = micros() - start;
        if (paced < 16625) delayMicroseconds(16625 - paced);
    } else delay(1);
    ++measuredFrames;
    const uint32_t now = millis();
    if (pramDirty && now - lastPram >= 5000) {
        MacAudioEnable(false);
        if (!persistPram()) machineFault = true;
        MacAudioEnable(audioReady && settings.sound);
        lastPram = now;
    }
    if (now - lastReport >= 5000) {
        Serial.printf("MACPLUS: %.2f emulated fps (target 60.15); PSRAM free %u\n",
                      measuredFrames * 1000.0 / (now - lastReport),
                      heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        lastReport = now;
        measuredFrames = 0;
    }
}
