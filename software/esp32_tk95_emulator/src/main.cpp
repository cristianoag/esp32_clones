#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Preferences.h>
#include <SD_MMC.h>
#include <VGA.h>
#include <esp_heap_caps.h>
#include <memory>
#include <new>
#include <algorithm>
#include "TkCore.h"
#include "TkInput.h"
#include "TkSettings.h"
#include "TkPlatform.h"
#include "TkBoard.h"
#include "TkAudio.h"
#include "TkKeyboard.h"
#include "TkJoysticks.h"
#include "TkJoystickHost.h"
#include "TkFirmwareUpdate.h"

static VGA video;
static TkCore machine;
static TkSettings settings;
static Preferences preferences;
static uint8_t *pixels;
static std::unique_ptr<uint8_t[]> tapeData;
static bool running = false, audioReady = false, sdReady = false, nvsReady = false;
static bool wantResume = false, audioFailed = false;
static char status[160] = {};
static const char *joystickNames[] = {"Disabled", "Kempston", "Sinclair 1", "Sinclair 2", "Cursor"};

class Display : public Adafruit_GFX
{
public:
    Display() : Adafruit_GFX(320, 240) {}
    void drawPixel(int16_t x, int16_t y, uint16_t color) override
    {
        if (x >= 0 && x < 320 && y >= 0 && y < 240) video.dot(x, y, color & 255);
    }
};
static Display display;

static bool error(const char *message)
{
    snprintf(status, sizeof(status), "%s", message);
    Serial.printf("TK: %s\n", status);
    return false;
}

void tkReportError(const char *message)
{
    error(message);
    audioFailed = true;
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
    display.setTextWrap(false);
    display.setTextSize(1);
    text(8, 8, "ESP32 Clone Series - TK95 / TK90X");
    text(284, 8, FW_VERSION);
    display.drawFastHLine(8, 21, 304, 255);
    text(8, 29, title, 0xdf);
    display.drawFastHLine(8, 199, 304, 255);
}

static void footer(const char *hint = "Arrows: select  Enter: apply  Esc: back")
{
    text(8, 187, hint);
    const size_t length = strlen(status);
    for (size_t pos = 0; pos < length && pos < 150; pos += 50)
    {
        char line[51] = {};
        memcpy(line, status + pos, std::min<size_t>(50, length - pos));
        text(8, 205 + (pos / 50) * 10, line, 0xdf);
    }
    video.show();
}

static void row(unsigned index, bool selected, const char *label)
{
    if (selected) display.fillRect(6, 46 + index * 12, 308, 10, 0x48);
    text(8, 47 + index * 12, label);
}

static uint8_t key()
{
    for (;;)
    {
        const uint8_t value = TkKeyboardMenuKey();
        if (value) return value;
        delay(10);
    }
}

static bool back(uint8_t value)
{
    if (value == 69 && running) wantResume = true;
    return value == 41 || value == 69;
}

static const char *baseName(const char *path)
{
    const char *last = strrchr(path, '/');
    return last ? last + 1 : path;
}

static bool confirm(const char *title)
{
    frame(title);
    text(8, 60, "Current machine state may be lost.");
    text(8, 84, "Y: confirm     N / Esc / F12: cancel");
    footer("");
    TkKeyboardClearEvents();
    for (;;)
    {
        const uint8_t value = key();
        if (value == 28) return true;
        if (value == 17 || value == 41 || value == 69) return false;
    }
}

static bool mountSd()
{
    if (sdReady) return true;
    SD_MMC.setPins(TkBoard::SdClk, TkBoard::SdCmd, TkBoard::SdData);
    sdReady = SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_DEFAULT, 5);
    if (!sdReady) return error("SD mount failed. Insert a FAT32 card and retry in F12.");
    return true;
}

static bool hasExtension(const char *name, const char *extension)
{
    const size_t length = strlen(name), suffix = strlen(extension);
    return length > suffix && !strcasecmp(name + length - suffix, extension);
}

static bool browse(const char *title, const char *extension, char result[TkPathSize])
{
    if (!mountSd()) return false;
    char directory[TkPathSize] = "/";
    unsigned selected = 0;
    TkKeyboardClearEvents();
    for (;;)
    {
        struct Entry { char name[TkPathSize]; bool directory; };
        Entry entries[10] = {};
        unsigned total = 0, count = 0;
        const unsigned first = (selected / 10) * 10;
        File folder = SD_MMC.open(directory);
        if (!folder || !folder.isDirectory()) return error("Cannot read SD directory.");
        if (strcmp(directory, "/"))
        {
            if (!first) { snprintf(entries[0].name, TkPathSize, ".."); entries[0].directory = true; ++count; }
            ++total;
        }
        unsigned visited = 0, skipped = 0;
        for (File entry = folder.openNextFile(); entry; entry = folder.openNextFile())
        {
            if (++visited % 16 == 0) delay(1);
            const char *name = baseName(entry.name());
            if (!strcmp(name, ".") || !strcmp(name, "..") ||
                !strcasecmp(name, "System Volume Information")) continue;
            if (!entry.isDirectory() && !hasExtension(name, extension)) continue;
            char path[TkPathSize];
            const int length = snprintf(path, sizeof(path), "%s%s%s", directory,
                                         strcmp(directory, "/") ? "/" : "", name);
            if (length < 0 || unsigned(length) >= sizeof(path) || !TkValidPath(path))
            {
                ++skipped;
                Serial.printf("TK browser: invalid/overlong path: %s\n", name);
                continue;
            }
            if (total >= first && count < 10)
            {
                snprintf(entries[count].name, TkPathSize, "%s", name);
                entries[count++].directory = entry.isDirectory();
            }
            ++total;
        }
        folder.close();
        if (selected >= total && selected) { selected = total ? total - 1 : 0; continue; }
        frame(title);
        for (unsigned i = 0; i < count; ++i)
        {
            char label[52];
            snprintf(label, sizeof(label), "%s%.46s", entries[i].directory ? "> " : "  ", entries[i].name);
            row(i, selected == first + i, label);
        }
        if (!total) text(8, 55, "No matching files.");
        if (skipped) snprintf(status, sizeof(status), "%u unsupported names skipped in %.100s", skipped, directory);
        else snprintf(status, sizeof(status), "%.150s", directory);
        footer("Enter: open  PgUp/PgDn: page  Esc: cancel");
        const uint8_t value = key();
        if (back(value)) return false;
        if (value == 82 && selected) --selected;
        else if (value == 81 && selected + 1 < total) ++selected;
        else if (value == 75) selected = selected >= 10 ? selected - 10 : 0;
        else if (value == 78 && total) selected = std::min(selected + 10, total - 1);
        else if (value == 40 && count)
        {
            const Entry &entry = entries[selected - first];
            if (!strcmp(entry.name, ".."))
            {
                char *slash = strrchr(directory, '/');
                if (slash == directory) directory[1] = 0;
                else *slash = 0;
                selected = 0;
            }
            else
            {
                char path[TkPathSize];
                const int length = snprintf(path, sizeof(path), "%s%s%s", directory,
                                             strcmp(directory, "/") ? "/" : "", entry.name);
                if (length < 0 || unsigned(length) >= sizeof(path)) return error("SD path is too long.");
                if (entry.directory) { memcpy(directory, path, sizeof(path)); selected = 0; }
                else { snprintf(result, TkPathSize, "%s", path); status[0] = 0; return true; }
            }
        }
    }
}

static bool readFile(const char *path, size_t maximum, std::unique_ptr<uint8_t[]> &data, size_t &size)
{
    if (!TkValidPath(path)) return error("Invalid SD path.");
    if (!mountSd()) return false;
    File file = SD_MMC.open(path);
    if (!file || file.isDirectory())
    {
        snprintf(status, sizeof(status), "Cannot open %.130s", path);
        Serial.println(status);
        return false;
    }
    const size_t length = file.size();
    if (!length || length > maximum) return error("File is empty or exceeds the supported size.");
    std::unique_ptr<uint8_t[]> candidate(new (std::nothrow) uint8_t[length]);
    if (!candidate) return error("Not enough memory to stage SD file.");
    for (size_t pos = 0; pos < length;)
    {
        const size_t chunk = std::min<size_t>(4096, length - pos);
        if (file.read(candidate.get() + pos, chunk) != chunk) return error("SD read failed; previous selection kept.");
        pos += chunk;
        delay(1);
    }
    if (file.size() != length) return error("SD file changed during loading.");
    data = std::move(candidate);
    size = length;
    return true;
}

static bool stageBoot(std::unique_ptr<uint8_t[]> &rom, std::unique_ptr<uint8_t[]> &tape, size_t &tapeSize)
{
    size_t romSize = 0;
    if (!TkValidSettings(settings)) return error("Invalid machine configuration.");
    if (!readFile(settings.rom[unsigned(settings.model)], 16384, rom, romSize)) return false;
    if (romSize != 16384) return error("TK ROM must be exactly 16384 bytes.");
    tapeSize = 0;
    if (!settings.tape[0]) return true;
    if (!readFile(settings.tape, 4 * 1024 * 1024, tape, tapeSize)) return false;
    TkTape check;
    if (!check.attach(tape.get(), tapeSize, status, sizeof(status))) { Serial.println(status); return false; }
    return true;
}

static bool saveSettings()
{
    if (!nvsReady) return error("NVS unavailable; settings were not saved.");
    if (!TkJoystickHostPause()) return error("Cannot pause joystick host for NVS write.");
    const bool saved = preferences.putBytes("boot", &settings, sizeof(settings)) == sizeof(settings);
    TkJoystickHostResume();
    if (!saved) return error("NVS write failed; settings were not saved.");
    snprintf(status, sizeof(status), "Complete boot configuration saved.");
    return true;
}

static bool boot(bool save, bool onlySave = false)
{
    std::unique_ptr<uint8_t[]> rom, tape;
    size_t tapeSize = 0;
    if (!stageBoot(rom, tape, tapeSize) || (save && !saveSettings())) return false;
    if (onlySave) return true;
    if (!machine.boot(rom.get(), 16384, settings.model, settings.timing, status, sizeof(status)))
    {
        Serial.println(status);
        return false;
    }
    machine.tape.eject();
    tapeData = std::move(tape);
    if (tapeData && !machine.tape.attach(tapeData.get(), tapeSize, status, sizeof(status)))
    {
        running = false;
        Serial.println(status);
        return false;
    }
    running = true;
    machine.portuguese = settings.portuguese != 0;
    status[0] = 0;
    memset(pixels, 0, TkCore::Width * TkCore::Height);
    return true;
}

static void attachTape()
{
    char path[TkPathSize] = {};
    if (!browse("Select TAP tape (read-only)", ".tap", path)) return;
    std::unique_ptr<uint8_t[]> candidate;
    size_t size = 0;
    if (!readFile(path, 4 * 1024 * 1024, candidate, size)) return;
    if (!machine.tape.attach(candidate.get(), size, status, sizeof(status)))
    {
        Serial.println(status);
        return;
    }
    tapeData = std::move(candidate);
    snprintf(settings.tape, TkPathSize, "%s", path);
    snprintf(status, sizeof(status), "Tape attached, stopped. Use LOAD \"\" then F12 > Play.");
}

static void mediaMenu()
{
    unsigned selected = 0;
    for (;;)
    {
        frame("Media");
        char label[60];
        snprintf(label, sizeof(label), "Tape: %.43s", settings.tape[0] ? baseName(settings.tape) : "<empty>");
        row(0, selected == 0, label);
        row(1, selected == 1, machine.tape.playing() ? "Stop tape" : "Play tape");
        row(2, selected == 2, "Rewind tape");
        row(3, selected == 3, "Eject tape");
        row(4, selected == 4, "Load 48K .SNA snapshot");
        row(5, selected == 5, "Load 48K .Z80 snapshot");
        row(6, selected == 6, "Reboot and save configuration");
        row(7, selected == 7, "Reboot without saving");
        footer("Enter: select  Esc: back  F12: resume");
        const uint8_t value = key();
        if (back(value)) return;
        if (value == 82) selected = (selected + 7) % 8;
        else if (value == 81) selected = (selected + 1) % 8;
        else if (value == 76 && selected == 0) { machine.tape.eject(); tapeData.reset(); settings.tape[0] = 0; }
        else if (value == 40)
        {
            if (selected == 0) attachTape();
            else if (selected == 1)
            {
                if (!running || !machine.tape.loaded()) error("Boot a machine and attach a tape first.");
                else
                {
                    machine.tape.play(!machine.tape.playing());
                    if (!machine.tape.playing()) snprintf(status, sizeof(status), "Tape stopped; rewind if at end.");
                }
            }
            else if (selected == 2) { machine.tape.rewind(); snprintf(status, sizeof(status), "Tape rewound and stopped."); }
            else if (selected == 3) { machine.tape.eject(); tapeData.reset(); settings.tape[0] = 0; }
            else if (selected <= 5)
            {
                if (!running) { error("Boot a ROM before loading a snapshot."); continue; }
                char path[TkPathSize] = {};
                const bool sna = selected == 4;
                if (browse("Select 48K snapshot", sna ? ".sna" : ".z80", path) &&
                    confirm("Load snapshot and replace current state?"))
                {
                    std::unique_ptr<uint8_t[]> data;
                    size_t size = 0;
                    if (readFile(path, 256 * 1024, data, size))
                    {
                        if (TkLoadSnapshot(machine, data.get(), size, sna, status, sizeof(status)))
                        {
                            wantResume = true;
                            return;
                        }
                        Serial.println(status);
                    }
                }
            }
            else if (confirm("Cold reboot selected configuration?") && boot(selected == 6))
            {
                wantResume = true;
                return;
            }
        }
        if (wantResume) return;
    }
}

static bool samePad(const TkJoystickSnapshot &a, const TkJoystickSnapshot &b)
{
    return b.connected && a.generation == b.generation && a.vid == b.vid && a.pid == b.pid &&
           a.endpoint == b.endpoint && a.length == b.length;
}

static void calibrate(unsigned port)
{
    TkJoystickSnapshot identity;
    TkJoystickSnapshotFor(port, identity);
    if (!identity.connected || !identity.length) { error("Connect a low-speed USB joystick with reports first."); return; }
    const char *poses[] = {"Release all controls", "Hold UP", "Hold RIGHT", "Hold DOWN", "Hold LEFT", "Hold button A", "Hold button B"};
    TkJoystickSample samples[7] = {};
    for (unsigned pose = 0; pose < 7; ++pose)
    {
        frame("Joystick calibration");
        text(8, 55, poses[pose]);
        text(8, 80, "Keep holding until the next prompt.");
        footer("Enter: capture  Esc/F12: cancel");
        TkKeyboardClearEvents();
        uint8_t value;
        do { value = key(); if (back(value)) return; } while (value != 40);
        uint8_t baseline[8] = {};
        bool first = true;
        uint32_t sequence = 0;
        unsigned freshReports = 0;
        TkJoystickSnapshot current;
        TkJoystickSnapshotFor(port, current);
        sequence = current.sequence;
        const uint32_t start = millis();
        while (millis() - start < 600)
        {
            if (back(TkKeyboardMenuKey())) return;
            TkJoystickSnapshotFor(port, current);
            if (!samePad(identity, current)) { error("Joystick changed; calibration cancelled."); return; }
            if (millis() - start >= 250 && current.sequence != sequence)
            {
                sequence = current.sequence;
                ++freshReports;
                if (first) { memcpy(baseline, current.report, identity.length); first = false; }
                if (!pose)
                    for (unsigned i = 0; i < identity.length; ++i)
                        samples[pose].changed[i] |= baseline[i] ^ current.report[i];
                memcpy(samples[pose].bytes, current.report, identity.length);
            }
            delay(10);
        }
        if (!freshReports) { error("No fresh joystick reports during capture; retry."); return; }
    }
    TkJoystickMapping mapping = {};
    if (!TkBuildJoystickCardinalMapping(identity.length, samples, mapping, status, sizeof(status)) ||
        !TkJoystickSave(port, identity, mapping, status, sizeof(status))) Serial.println(status);
}

static void joystickMenu()
{
    unsigned selected = 0;
    for (;;)
    {
        frame("USB joysticks");
        for (unsigned port = 0; port < 2; ++port)
        {
            TkJoystickSnapshot pad;
            TkJoystickSnapshotFor(port, pad);
            char label[64];
            snprintf(label, sizeof(label), "Port %u: %s", port + 1, joystickNames[unsigned(settings.joystick[port])]);
            row(port * 3, selected == port * 3, label);
            snprintf(label, sizeof(label), "Calibrate %u: %s  %02X", port + 1, !pad.connected ? "disconnected" :
                     pad.calibrated ? "calibrated" : "needs calibration", pad.buttons);
            row(port * 3 + 1, selected == port * 3 + 1, label);
            row(port * 3 + 2, selected == port * 3 + 2, "Clear saved calibration");
            snprintf(label, sizeof(label), "%u VID %04X PID %04X report %u bytes", port + 1, pad.vid, pad.pid, pad.length);
            text(8, 134 + port * 12, label);
        }
        footer("Enter: change  Esc: back  F12: resume");
        uint8_t value = 0;
        const uint32_t start = millis();
        while (!value && millis() - start < 100) { value = TkKeyboardMenuKey(); delay(10); }
        if (back(value)) return;
        if (value == 82) selected = (selected + 5) % 6;
        else if (value == 81) selected = (selected + 1) % 6;
        else if (value == 40 || value == 79 || value == 80)
        {
            const unsigned port = selected / 3;
            if (selected % 3 == 0)
                settings.joystick[port] = TkJoystick((unsigned(settings.joystick[port]) + (value == 80 ? 4 : 1)) % 5);
            else if (selected % 3 == 1 && value == 40) calibrate(port);
            else if (selected % 3 == 2 && value == 40 && confirm("Clear this joystick calibration?"))
                TkJoystickClear(port, status, sizeof(status));
        }
        if (wantResume) return;
    }
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
    char path[TkPathSize] = {};
    if (!browse("Select ESP32_TK95-*.FLH", ".flh", path) || !confirm("Install firmware and restart board?")) return;
    if (!TkJoystickHostPause()) { error("Cannot pause joystick host for firmware update."); return; }
    const bool installed = TkInstallFirmware(path, progress, status, sizeof(status));
    TkJoystickHostResume();
    if (!installed) return;
    frame("Firmware verified. Rebooting...");
    video.show();
    delay(1000);
    ESP.restart();
}

static void configuration()
{
    unsigned selected = 0;
    for (;;)
    {
        frame("Configuration (model/ROM/timing need reboot)");
        char labels[11][64];
        snprintf(labels[0], 64, "Machine: %s", settings.model == TkModel::TK95 ? "TK95" : "TK90X");
        snprintf(labels[1], 64, "ROM: %.44s", baseName(settings.rom[unsigned(settings.model)]));
        snprintf(labels[2], 64, "Microdigital ULA: %s", settings.timing == TkTiming::Hz60 ? "60 Hz" : "50 Hz");
        snprintf(labels[3], 64, "Sound: %s", settings.sound ? "On" : "Off");
        snprintf(labels[4], 64, "Volume: %u%%", settings.volume);
        snprintf(labels[5], 64, "Auto boot: %s", settings.autoBoot ? "On" : "Off");
        snprintf(labels[6], 64, "Render: every %u frame(s)", settings.frameSkip + 1);
        snprintf(labels[7], 64, "Save complete boot configuration");
        snprintf(labels[8], 64, "Reboot and save configuration");
        snprintf(labels[9], 64, "Reboot without saving");
        snprintf(labels[10], 64, "ROM language pin: %s", settings.portuguese ? "Portuguese" : "Spanish");
        for (unsigned i = 0; i < 11; ++i) row(i, i == selected, labels[i]);
        footer("Enter/L/R: change  Esc: back  F12: resume");
        const uint8_t value = key();
        if (back(value)) return;
        if (value == 82) selected = (selected + 10) % 11;
        else if (value == 81) selected = (selected + 1) % 11;
        else if (value == 40 || value == 79 || value == 80)
        {
            const bool enter = value == 40;
            switch (selected)
            {
            case 0: settings.model = settings.model == TkModel::TK95 ? TkModel::TK90X : TkModel::TK95; break;
            case 1:
                if (enter)
                {
                    char path[TkPathSize] = {};
                    if (browse("Select 16 KiB TK ROM", ".rom", path))
                    {
                        std::unique_ptr<uint8_t[]> rom;
                        size_t size;
                        if (readFile(path, 16384, rom, size))
                        {
                            if (size != 16384) error("ROM must be exactly 16384 bytes.");
                            else snprintf(settings.rom[unsigned(settings.model)], TkPathSize, "%s", path);
                        }
                    }
                }
                break;
            case 2: settings.timing = settings.timing == TkTiming::Hz60 ? TkTiming::Hz50 : TkTiming::Hz60; break;
            case 3: settings.sound ^= 1; break;
            case 4: settings.volume = value == 80 ? std::max(0, int(settings.volume) - 10) :
                                                   std::min(100, int(settings.volume) + 10); break;
            case 5: settings.autoBoot ^= 1; break;
            case 6: settings.frameSkip = (settings.frameSkip + (value == 80 ? 2 : 1)) % 3; break;
            case 10: settings.portuguese ^= 1; break;
            case 7: if (enter) boot(true, true); break;
            case 8: case 9:
                if (enter && confirm("Cold reboot selected configuration?") && boot(selected == 8))
                {
                    wantResume = true;
                    return;
                }
                break;
            }
        }
        if (wantResume) return;
    }
}

static void menu()
{
    TkAudioEnable(false);
    TkKeyboardClearEvents();
    wantResume = false;
    unsigned selected = 0;
    for (;;)
    {
        frame("F12 menu");
        const char *labels[] = {"Resume emulation", "Configuration / ROM", "Media / tape / snapshots",
                                "USB joysticks", "Warm reset", "Cold boot selected configuration",
                                "Retry microSD", "Firmware update", "Hardware / help"};
        for (unsigned i = 0; i < 9; ++i) row(i, selected == i, labels[i]);
        footer("Enter: select  Esc/F12: resume");
        const uint8_t value = key();
        if ((value == 41 || value == 69) && running) break;
        if (value == 82) selected = (selected + 8) % 9;
        else if (value == 81) selected = (selected + 1) % 9;
        else if (value == 40)
        {
            if (selected == 0) { if (running) break; error("Select a ROM and cold boot first."); }
            else if (selected == 1) configuration();
            else if (selected == 2) mediaMenu();
            else if (selected == 3) joystickMenu();
            else if (selected == 4)
            {
                if (!running) error("No machine is running.");
                else if (confirm("Warm reset? RAM retained, tape rewound.")) { machine.reset(); break; }
            }
            else if (selected == 5)
            {
                if ((!running || confirm("Cold boot and clear RAM?")) && boot(false)) break;
            }
            else if (selected == 6)
            {
                if (mountSd()) snprintf(status, sizeof(status), "SD mounted. Restart board after replacing the card.");
            }
            else if (selected == 7) firmwareUpdate();
            else
            {
                frame("Hardware / keyboard help");
                text(8, 48, "ESP32-S3 N16R8 / VGA RGB222 320x240");
                text(8, 62, "USB keyboard 19/20. Pads 15/16, 17/18.");
                text(8, 76, "SD CMD/CLK/D0: 38/39/40. Audio: 47.");
                text(8, 90, "Shift: CAPS SHIFT. Ctrl/Alt: SYMBOL SHIFT.");
                text(8, 104, "Arrows: CAPS + 5/6/7/8. Esc: BREAK.");
                text(8, 118, "Tape: LOAD \"\" then F12 > Media > Play.");
                text(8, 132, "USB pads: low-speed only; calibrate first.");
                text(8, 146, "TAP read-only; 48K SNA/Z80 load only.");
                text(8, 160, "F12 pauses CPU/audio/tape. No AY or disk.");
                footer("Any key: back");
                back(key());
            }
        }
        if (wantResume && running) break;
    }
    TkKeyboardClearEvents();
    TkAudioEnable(audioReady && settings.sound && !audioFailed);
}

static void present()
{
    for (unsigned y = 0; y < TkCore::Height; ++y)
    {
        memcpy(video.dmaBuffer->getLineAddr8(y), pixels + y * TkCore::Width, TkCore::Width);
        video.dmaBuffer->flush(0, y);
    }
}

static void fatal(const char *message, bool visible)
{
    Serial.println(message);
    if (visible) { frame("Initialization failed"); error(message); footer(""); }
    for (;;) delay(1000);
}

void setup()
{
    Serial.begin(115200);
    Serial.printf("\nESP32 TK95 / TK90X %s\n", FW_VERSION);
    if (!psramFound()) fatal("8 MiB OPI PSRAM required.", false);
    video.bufferCount = 1;
    const PinConfig pins(-1,-1,-1,5,4, -1,-1,-1,-1,7,6, -1,-1,-1,9,8, 2,1);
    const Mode timing(8,48,24,320, 10,2,33,240, 12587500,0,0,2);
    if (!video.init(pins, timing, 8)) fatal("VGA allocation failed.", false);
    pinMode(TkBoard::RgbLed, INPUT);
    neopixelWrite(TkBoard::RgbLed, 0, 0, 0);
    pinMode(TkBoard::RgbLed, INPUT);
    if (!video.start()) fatal("VGA start failed.", false);
    frame("Starting shared board hardware...");
    video.show();
    pixels = static_cast<uint8_t *>(heap_caps_calloc(TkCore::Width * TkCore::Height, 1, MALLOC_CAP_SPIRAM));
    if (!pixels) fatal("Emulator framebuffer allocation failed.", true);
    if (!TkKeyboardStart()) fatal("USB keyboard host failed; see UART.", true);
    audioReady = TkAudioStart();
    if (!audioReady) error("Audio initialization failed; see UART.");
    mountSd();
    nvsReady = preferences.begin("tk95", false);
    bool validBoot = nvsReady;
    if (!nvsReady) error("NVS unavailable. Automatic boot disabled.");
    else
    {
        const size_t size = preferences.getBytesLength("boot");
        if (size)
        {
            uint8_t stored[sizeof(TkSettings)];
            if (size != sizeof(stored) || preferences.getBytes("boot", stored, size) != size ||
                !TkDecodeSettings(stored, size, settings))
            {
                validBoot = false;
                error("Invalid saved settings; select configuration in F12.");
            }
        }
    }
    if (!TkJoysticksStart()) error("Joystick host initialization failed; see UART.");
    frame("Press F12 for setup. Automatic boot in 2.5s.");
    footer("");
    const uint32_t start = millis();
    bool interrupted = false;
    while (millis() - start < 2500)
    {
        if (TkKeyboardMenuKey() == 69) { interrupted = true; break; }
        delay(10);
    }
    if (!interrupted && validBoot && settings.autoBoot && sdReady) boot(false);
    TkAudioEnable(audioReady && settings.sound && running);
}

void loop()
{
    static uint32_t lastReport = 0, measuredFrames = 0;
    bool openMenu = !running || audioFailed;
    for (uint8_t value = TkKeyboardMenuKey(); value; value = TkKeyboardMenuKey())
        if (value == 69) openMenu = true;
    if (openMenu)
    {
        if (audioFailed) { settings.sound = 0; audioFailed = false; }
        menu();
        present();
        lastReport = millis();
        measuredFrames = 0;
    }
    const uint32_t start = micros();
    uint8_t report[8], rows[8];
    TkKeyboardReport(report);
    TkKeyboard(report, rows);
    const uint16_t pads = TkJoysticksRead();
    uint8_t kempston = TkApplyJoystick(rows, pads & 63, settings.joystick[0]);
    kempston |= TkApplyJoystick(rows, (pads >> 8) & 63, settings.joystick[1]);
    machine.input(rows, kempston);
    int16_t samples[448];
    unsigned count = 0;
    const bool draw = machine.frames % (settings.frameSkip + 1) == 0;
    machine.runFrame(draw ? pixels : nullptr, samples, count);
    if (draw) present();
    if (audioReady && settings.sound)
    {
        for (unsigned i = 0; i < count; ++i) samples[i] = samples[i] * settings.volume / 100;
        TkAudioSubmit(samples, count);
    }
    const uint32_t target = TkGetClock(machine.timing).frameMicros;
    const uint32_t elapsed = micros() - start;
    if (elapsed < target)
    {
        const uint32_t remaining = target - elapsed;
        if (remaining >= 1000) delay(remaining / 1000);
        while (micros() - start < target) delayMicroseconds(10);
    }
    else delay(1);
    ++measuredFrames;
    const uint32_t now = millis();
    if (now - lastReport >= 5000)
    {
        Serial.printf("TK speed: %.2f emulated fps, target %.2f, rendering 1/%u\n",
                      measuredFrames * 1000.0 / (now - lastReport),
                      1000000.0 / target, settings.frameSkip + 1);
        lastReport = now;
        measuredFrames = 0;
    }
}
