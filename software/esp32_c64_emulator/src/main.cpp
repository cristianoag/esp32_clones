#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Preferences.h>
#include <SD_MMC.h>
#include <VGA.h>
#include <esp_heap_caps.h>
#include <memory>
#include <new>
#include <algorithm>
#include "C64Core.h"
#include "C64Input.h"
#include "C64Media.h"
#include "C64Video.h"
#include "C64Settings.h"
#include "C64Platform.h"
#include "C64Board.h"
#include "C64Audio.h"
#include "C64Keyboard.h"
#include "C64Joysticks.h"
#include "C64JoystickHost.h"
#include "C64FirmwareUpdate.h"

static VGA video;
static C64Core *machine = nullptr;
static C64Settings settings;
static Preferences preferences;
static bool running = false, audioReady = false, sdReady = false, nvsReady = false;
static bool wantResume = false, audioFailed = false;
static char status[160] = {};
static const char *joystickNames[] = {"Disabled", "C64 digital joystick"};

class Display : public Adafruit_GFX
{
public:
    Display() : Adafruit_GFX(320, 240) {}
    void drawPixel(int16_t x, int16_t y, uint16_t color) override
    {
        if (x >= 0 && x < 320 && y >= 0 && y < 240)
        {
            video.dot(x, y, color & 255);
        }
    }
};
static Display display;

static bool error(const char *message)
{
    snprintf(status, sizeof(status), "%s", message);
    Serial.printf("C64: %s\n", status);
    return false;
}

void c64ReportError(const char *message)
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
    text(8, 8, "ESP32 Clone Series - Commodore 64");
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
        const uint8_t value = C64KeyboardMenuKey();
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
    C64KeyboardClearEvents();
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
    SD_MMC.setPins(C64Board::SdClk, C64Board::SdCmd, C64Board::SdData);
    sdReady = SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_DEFAULT, 5);
    if (!sdReady) return error("SD mount failed. Insert a FAT32 card and retry in F12.");
    return true;
}

static bool hasExtension(const char *name, const char *extension)
{
    const size_t length = strlen(name), suffix = strlen(extension);
    return length > suffix && !strcasecmp(name + length - suffix, extension);
}

static bool browse(const char *title, const char *extension, char result[C64PathSize])
{
    if (!mountSd()) return false;
    char directory[C64PathSize] = "/";
    unsigned selected = 0;
    C64KeyboardClearEvents();
    for (;;)
    {
        struct Entry { char name[C64PathSize]; bool directory; };
        Entry entries[10] = {};
        unsigned total = 0, count = 0;
        const unsigned first = (selected / 10) * 10;
        File folder = SD_MMC.open(directory);
        if (!folder || !folder.isDirectory()) return error("Cannot read SD directory.");
        if (strcmp(directory, "/"))
        {
            if (!first) { snprintf(entries[0].name, C64PathSize, ".."); entries[0].directory = true; ++count; }
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
            char path[C64PathSize];
            const int length = snprintf(path, sizeof(path), "%s%s%s", directory,
                                         strcmp(directory, "/") ? "/" : "", name);
            if (length < 0 || unsigned(length) >= sizeof(path) || !C64ValidPath(path))
            {
                ++skipped;
                Serial.printf("C64 browser: invalid/overlong path: %s\n", name);
                continue;
            }
            if (total >= first && count < 10)
            {
                snprintf(entries[count].name, C64PathSize, "%s", name);
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
                char path[C64PathSize];
                const int length = snprintf(path, sizeof(path), "%s%s%s", directory,
                                             strcmp(directory, "/") ? "/" : "", entry.name);
                if (length < 0 || unsigned(length) >= sizeof(path)) return error("SD path is too long.");
                if (entry.directory) { memcpy(directory, path, sizeof(path)); selected = 0; }
                else { snprintf(result, C64PathSize, "%s", path); status[0] = 0; return true; }
            }
        }
    }
}

static bool readFile(const char *path, size_t maximum, std::unique_ptr<uint8_t[]> &data, size_t &size)
{
    if (!C64ValidPath(path)) return error("Invalid SD path.");
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

struct BootFiles
{
    std::unique_ptr<uint8_t[]> rom[3];
};

static bool stageBoot(BootFiles &files)
{
    if (!C64ValidSettings(settings)) return error("Invalid machine configuration.");
    for (unsigned i = 0; i < 3; ++i)
    {
        const size_t expected = i == 2 ? 4096 : 8192;
        size_t size = 0;
        if (!readFile(settings.rom[i], expected, files.rom[i], size)) return false;
        if (size != expected) return error("ROM sizes: BASIC/KERNAL 8192, character ROM 4096.");
    }
    return true;
}

static bool saveSettings()
{
    if (!nvsReady) return error("NVS unavailable; settings were not saved.");
    if (!C64JoystickHostPause()) return error("Cannot pause joystick host for NVS write.");
    const bool saved = preferences.putBytes("boot", &settings, sizeof(settings)) == sizeof(settings);
    C64JoystickHostResume();
    if (!saved) return error("NVS write failed; settings were not saved.");
    snprintf(status, sizeof(status), "Complete boot configuration saved.");
    return true;
}

static bool boot(bool save, bool onlySave = false)
{
    BootFiles files;
    if (!stageBoot(files) || (save && !saveSettings())) return false;
    if (onlySave) return true;
    if (!machine->boot(files.rom[0].get(), 8192, files.rom[1].get(), 8192,
                       files.rom[2].get(), 4096, status, sizeof(status)))
    {
        Serial.println(status);
        return false;
    }
    running = true;
    status[0] = 0;
    return true;
}

static bool loadProgram(const uint8_t *data, size_t size)
{
    if (!machine->loadPrg(data, size, status, sizeof(status))) { Serial.println(status); return false; }
    snprintf(status, sizeof(status), "PRG loaded. BASIC: RUN. Machine code: SYS address.");
    return true;
}

static void loadD64()
{
    char path[C64PathSize] = {};
    if (!browse("Select 35-track D64 (read-only)", ".d64", path)) return;
    std::unique_ptr<uint8_t[]> image;
    size_t size = 0;
    if (!readFile(path, C64D64::ImageSize + 683, image, size)) return;
    C64D64 disk;
    if (!disk.open(image.get(), size, status, sizeof(status))) { Serial.println(status); return; }
    if (!disk.count()) { error("No closed PRG entries in this D64."); return; }
    unsigned selected = 0;
    C64KeyboardClearEvents();
    for (;;)
    {
        frame("D64 programs (no drive emulation)");
        const unsigned first = selected / 10 * 10;
        for (unsigned i = first; i < std::min(first + 10, disk.count()); ++i)
            row(i - first, i == selected, disk.entry(i).name);
        footer("Enter: load PRG  Esc: back  PgUp/PgDn");
        const uint8_t value = key();
        if (back(value)) return;
        if (value == 82 && selected) --selected;
        else if (value == 81 && selected + 1 < disk.count()) ++selected;
        else if (value == 75) selected = selected >= 10 ? selected - 10 : 0;
        else if (value == 78) selected = std::min(selected + 10, disk.count() - 1);
        else if (value == 40)
        {
            std::unique_ptr<uint8_t[]> program(new (std::nothrow) uint8_t[65538]);
            if (!program) { error("Not enough memory to extract PRG."); return; }
            size_t length = 0;
            if (!disk.extract(selected, program.get(), 65538, length, status, sizeof(status)))
            { Serial.println(status); return; }
            loadProgram(program.get(), length);
            return;
        }
    }
}

static void mediaMenu()
{
    unsigned selected = 0;
    for (;;)
    {
        frame("Media");
        row(0, selected == 0, "Load PRG into current machine");
        row(1, selected == 1, "Load a PRG from D64");
        row(2, selected == 2, "Queue BASIC RUN and resume");
        text(8, 104, "Load at the READY prompt, not during boot.");
        text(8, 118, "D64: program extraction only, read-only.");
        text(8, 132, "No 1541, multi-load or disk SAVE support.");
        footer("Enter: select  Esc: back  F12: resume");
        const uint8_t value = key();
        if (back(value)) return;
        if (value == 82) selected = (selected + 2) % 3;
        else if (value == 81) selected = (selected + 1) % 3;
        else if (value == 40)
        {
            if (!running) { error("Cold boot ROMs and wait for READY before loading."); continue; }
            if (selected == 0)
            {
                char path[C64PathSize] = {};
                if (browse("Select PRG", ".prg", path))
                {
                    std::unique_ptr<uint8_t[]> data;
                    size_t size = 0;
                    if (readFile(path, 65538, data, size)) loadProgram(data.get(), size);
                }
            }
            else if (selected == 1) loadD64();
            else
            {
                machine->queueRun();
                wantResume = true;
                return;
            }
        }
        if (wantResume) return;
    }
}

static bool samePad(const C64JoystickSnapshot &a, const C64JoystickSnapshot &b)
{
    return b.connected && a.generation == b.generation && a.vid == b.vid && a.pid == b.pid &&
           a.endpoint == b.endpoint && a.length == b.length;
}

static void calibrate(unsigned port)
{
    C64JoystickSnapshot identity;
    C64JoystickSnapshotFor(port, identity);
    if (!identity.connected || !identity.length) { error("Connect a low-speed USB joystick with reports first."); return; }
    const char *poses[] = {"Release all controls", "Hold UP", "Hold RIGHT", "Hold DOWN", "Hold LEFT", "Hold button A", "Hold button B"};
    C64JoystickSample samples[7] = {};
    for (unsigned pose = 0; pose < 7; ++pose)
    {
        frame("Joystick calibration");
        text(8, 55, poses[pose]);
        text(8, 80, "Keep holding until the next prompt.");
        footer("Enter: capture  Esc/F12: cancel");
        C64KeyboardClearEvents();
        uint8_t value;
        do { value = key(); if (back(value)) return; } while (value != 40);
        uint8_t baseline[8] = {};
        bool first = true;
        uint32_t sequence = 0;
        unsigned freshReports = 0;
        C64JoystickSnapshot current;
        C64JoystickSnapshotFor(port, current);
        sequence = current.sequence;
        const uint32_t start = millis();
        while (millis() - start < 600)
        {
            if (back(C64KeyboardMenuKey())) return;
            C64JoystickSnapshotFor(port, current);
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
    C64JoystickMapping mapping = {};
    if (!C64BuildJoystickCardinalMapping(identity.length, samples, mapping, status, sizeof(status)) ||
        !C64JoystickSave(port, identity, mapping, status, sizeof(status))) Serial.println(status);
}

static void joystickMenu()
{
    unsigned selected = 0;
    for (;;)
    {
        frame("USB joysticks");
        for (unsigned port = 0; port < 2; ++port)
        {
            C64JoystickSnapshot pad;
            C64JoystickSnapshotFor(port, pad);
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
        while (!value && millis() - start < 100) { value = C64KeyboardMenuKey(); delay(10); }
        if (back(value)) return;
        if (value == 82) selected = (selected + 5) % 6;
        else if (value == 81) selected = (selected + 1) % 6;
        else if (value == 40 || value == 79 || value == 80)
        {
            const unsigned port = selected / 3;
            if (selected % 3 == 0)
                settings.joystick[port] ^= 1;
            else if (selected % 3 == 1 && value == 40) calibrate(port);
            else if (selected % 3 == 2 && value == 40 && confirm("Clear this joystick calibration?"))
                C64JoystickClear(port, status, sizeof(status));
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
    char path[C64PathSize] = {};
    if (!browse("Select firmware .FLH", ".flh", path) || !confirm("Install firmware and restart board?")) return;
    if (!C64JoystickHostPause()) { error("Cannot pause joystick host for firmware update."); return; }
    const bool installed = C64InstallFirmware(path, progress, status, sizeof(status));
    C64JoystickHostResume();
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
        frame("Configuration (ROM changes need cold boot)");
        char labels[11][64];
        snprintf(labels[0], 64, "BASIC ROM: %.39s", baseName(settings.rom[0]));
        snprintf(labels[1], 64, "KERNAL ROM: %.38s", baseName(settings.rom[1]));
        snprintf(labels[2], 64, "Character ROM: %.35s", baseName(settings.rom[2]));
        snprintf(labels[3], 64, "Sound: %s", settings.sound ? "On" : "Off");
        snprintf(labels[4], 64, "Volume: %u%%", settings.volume);
        snprintf(labels[5], 64, "Auto boot: %s", settings.autoBoot ? "On" : "Off");
        snprintf(labels[6], 64, "Render: every %u frame(s)", settings.frameSkip + 1);
        snprintf(labels[7], 64, "Save complete boot configuration");
        snprintf(labels[8], 64, "Reboot and save configuration");
        snprintf(labels[9], 64, "Reboot without saving");
        snprintf(labels[10], 64, "Swap joystick ports: %s", settings.swapPorts ? "Yes" : "No");
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
            case 0: case 1: case 2:
                if (enter)
                {
                    char path[C64PathSize] = {};
                    const size_t expected = selected == 2 ? 4096 : 8192;
                    if (browse(selected == 2 ? "Select 4096-byte character ROM" : "Select 8192-byte BASIC/KERNAL ROM", ".rom", path))
                    {
                        std::unique_ptr<uint8_t[]> rom;
                        size_t size;
                        if (readFile(path, expected, rom, size))
                        {
                            if (size != expected) error("ROM size does not match the selected hardware.");
                            else snprintf(settings.rom[selected], C64PathSize, "%s", path);
                        }
                    }
                }
                break;
            case 3: settings.sound ^= 1; break;
            case 4: settings.volume = value == 80 ? std::max(0, int(settings.volume) - 10) :
                                                   std::min(100, int(settings.volume) + 10); break;
            case 5: settings.autoBoot ^= 1; break;
            case 6: settings.frameSkip = (settings.frameSkip + (value == 80 ? 2 : 1)) % 3; break;
            case 10: settings.swapPorts ^= 1; break;
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
    C64AudioEnable(false);
    C64KeyboardClearEvents();
    wantResume = false;
    unsigned selected = 0;
    for (;;)
    {
        frame("F12 menu");
        const char *labels[] = {"Resume emulation", "Configuration / ROM", "Media / PRG and D64",
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
                else if (confirm("Reset C64? RAM retained.")) { machine->reset(); break; }
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
                text(8, 90, "Ctrl: CONTROL. Alt/GUI: Commodore.");
                text(8, 104, "Esc: RUN/STOP. F11: RESTORE. F12: menu.");
                text(8, 118, "F1-F8: C64 F1-F8. Caps: Shift Lock.");
                text(8, 132, "USB pads: low-speed only; calibrate first.");
                text(8, 146, "Pad 1/2 = C64 port 1/2; swap in setup.");
                text(8, 160, "PAL / 64K. F12 pauses CPU and audio.");
                footer("Any key: back");
                back(key());
            }
        }
        if (wantResume && running) break;
    }
    C64KeyboardClearEvents();
    C64AudioEnable(audioReady && settings.sound && !audioFailed);
}

static void present()
{
    uint8_t line[320];
    for (unsigned y = 0; y < 240; ++y)
    {
        C64VideoLine(machine->pixels, machine->vic.vicreg[0x20], y, line);
        memcpy(video.dmaBuffer->getLineAddr8(y, video.backBuffer), line, sizeof(line));
        video.dmaBuffer->flush(video.backBuffer, y);
    }
    video.show();
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
    Serial.printf("\nESP32 Commodore 64 %s\n", FW_VERSION);
    if (!psramFound()) fatal("8 MiB OPI PSRAM required.", false);
    void *memory = ps_malloc(sizeof(C64Core));
    if (!memory) fatal("C64 PSRAM allocation failed.", false);
    machine = new (memory) C64Core;
    video.bufferCount = 1;
    const PinConfig pins(-1,-1,-1,5,4, -1,-1,-1,-1,7,6, -1,-1,-1,9,8, 2,1);
    const Mode timing(8,48,24,320, 10,2,33,240, 12587500,0,0,2);
    if (!video.init(pins, timing, 8)) fatal("VGA allocation failed.", false);
    pinMode(C64Board::RgbLed, INPUT);
    neopixelWrite(C64Board::RgbLed, 0, 0, 0);
    pinMode(C64Board::RgbLed, INPUT);
    if (!video.start()) fatal("VGA start failed.", false);
    frame("Starting shared board hardware...");
    video.show();
    if (!C64KeyboardStart()) fatal("USB keyboard host failed; see UART.", true);
    audioReady = C64AudioStart();
    if (!audioReady) error("Audio initialization failed; see UART.");
    mountSd();
    nvsReady = preferences.begin("c64", false);
    bool validBoot = nvsReady;
    if (!nvsReady) error("NVS unavailable. Automatic boot disabled.");
    else
    {
        const size_t size = preferences.getBytesLength("boot");
        if (size)
        {
            uint8_t stored[sizeof(C64Settings)];
            if (size != sizeof(stored) || preferences.getBytes("boot", stored, size) != size ||
                !C64DecodeSettings(stored, size, settings))
            {
                validBoot = false;
                error("Invalid saved settings; select configuration in F12.");
            }
        }
    }
    if (!C64JoysticksStart()) error("Joystick host initialization failed; see UART.");
    Serial.printf("C64 memory: internal free %u, largest block %u, PSRAM free %u\n",
                  heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                  heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
                  heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    frame("Press F12 for setup. Automatic boot in 2.5s.");
    footer("");
    const uint32_t start = millis();
    bool interrupted = false;
    while (millis() - start < 2500)
    {
        if (C64KeyboardMenuKey() == 69) { interrupted = true; break; }
        delay(10);
    }
    if (!interrupted && validBoot && settings.autoBoot && sdReady) boot(false);
    C64AudioEnable(audioReady && settings.sound && running);
}

void loop()
{
    static uint32_t lastReport = millis(), measuredFrames = 0;
    bool openMenu = !running || audioFailed;
    for (uint8_t value = C64KeyboardMenuKey(); value; value = C64KeyboardMenuKey())
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
    uint8_t report[8];
    C64KeyboardReport(report);
    uint16_t pads = C64JoysticksRead();
    if (!settings.joystick[0]) pads &= 0xff00;
    if (!settings.joystick[1]) pads &= 0x00ff;
    machine->input(report, pads, settings.swapPorts);
    machine->runFrame();
    if (machine->cpuhalted)
    {
        error("C64 CPU halted on a JAM opcode. Reset or cold boot in F12.");
        menu();
        return;
    }
    if (machine->frames % (settings.frameSkip + 1) == 0) present();
    if (audioReady && settings.sound)
    {
        int16_t samples[SID::SampleCount];
        for (unsigned i = 0; i < SID::SampleCount; ++i)
            samples[i] = machine->sid.audio()[i] * settings.volume / 100;
        C64AudioSubmit(samples, SID::SampleCount);
    }
    const uint32_t target = 20000;
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
        Serial.printf("C64 speed: %.2f emulated fps, target %.2f, rendering 1/%u\n",
                      measuredFrames * 1000.0 / (now - lastReport),
                      1000000.0 / target, settings.frameSkip + 1);
        Serial.printf("C64 PC: %04X\n", machine->programCounter());
        lastReport = now;
        measuredFrames = 0;
    }
}
