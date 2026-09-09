#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Preferences.h>
#include <VGA.h>
#include <esp_heap_caps.h>
#include <algorithm>
#include "MsxAudio.h"
#include "MsxBoard.h"
#include "MsxCore.h"
#include "MsxKeyboard.h"
#include "MsxPlatform.h"
#include "MsxProfiles.h"
#include "MsxFileBrowser.h"
#include "MsxFirmwareUpdate.h"
#include "MsxSettings.h"
#include "MsxJoysticks.h"
#include "MsxJoystickHost.h"
#include "MsxJoystickDisplay.h"

static VGA video;
static uint8_t *screenBackup = nullptr;
static bool videoReady = false;
static bool audioReady = false;
static bool running = false;
static bool exitRequested = false;
static bool bootRequested = false;
static size_t selectedProfile = 0;
static int selectedRamPages = 32;
static bool soundEnabled = true;
static bool autoBoot = true;
static char selectedCartridges[2][MsxSdPathCapacity] = {};
static char statusMessage[160] = "";
static Preferences preferences;
static bool preferencesReady = false;

class MenuDisplay : public Adafruit_GFX
{
public:
    MenuDisplay() : Adafruit_GFX(MsxBoard::Width, MsxBoard::Height) {}
    void drawPixel(int16_t x, int16_t y, uint16_t color) override
    {
        if (x >= 0 && x < width() && y >= 0 && y < height())
            video.dot(x, y, color & 0xff);
    }
};

static MenuDisplay display;

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
    text(8, 8, "ESP32 Clone Series - MSX Firmware");
    text(288, 8, FW_VERSION);
    display.drawFastHLine(8, 21, 304, 255);
    text(8, 29, title, 0xdf);
    display.drawFastHLine(8, 199, 304, 255);
}

static void messageLines(const char *message)
{
    char line[51];
    for (int row = 0; row < 3 && *message; ++row)
    {
        const size_t count = std::min<size_t>(strlen(message), 50);
        memcpy(line, message, count);
        line[count] = '\0';
        text(8, 206 + row * 10, line, 0xdf);
        message += count;
    }
}

static bool validateCartridges()
{
    for (unsigned slot = 0; slot < 2; ++slot)
    {
        if (!selectedCartridges[slot][0]) continue;
        char path[MsxSdPathCapacity + 8], error[112];
        snprintf(path, sizeof(path), "/sdcard%s", selectedCartridges[slot]);
        if (!MsxValidateCartridge(path, error, sizeof(error)))
        {
            snprintf(statusMessage, sizeof(statusMessage), "Slot %u: %s", slot + 1, error);
            Serial.println(statusMessage);
            return false;
        }
    }
    return true;
}

static void saveSettings()
{
    if (!preferencesReady)
    {
        snprintf(statusMessage, sizeof(statusMessage), "NVS unavailable; settings were not saved.");
        Serial.println(statusMessage);
        return;
    }
    MsxBootSettings settings = {};
    settings.machine.version = 2;
    snprintf(settings.machine.profile, sizeof(settings.machine.profile), "%s", MsxProfiles[selectedProfile].id);
    settings.machine.ramPages = selectedRamPages;
    settings.machine.sound = soundEnabled;
    settings.machine.autoBoot = autoBoot;
    memcpy(settings.cartridges, selectedCartridges, sizeof(selectedCartridges));
    if (!MsxJoystickHostPause())
    {
        snprintf(statusMessage, sizeof(statusMessage), "Cannot pause joystick host to save settings.");
        Serial.println(statusMessage);
        return;
    }
    const bool written = preferences.putBytes("boot", &settings, sizeof(settings)) == sizeof(settings);
    MsxJoystickHostResume();
    if (!written)
    {
        snprintf(statusMessage, sizeof(statusMessage), "NVS write failed; settings were not saved.");
        Serial.println(statusMessage);
        return;
    }
    snprintf(statusMessage, sizeof(statusMessage), "Boot profile, slots and settings saved.");
}

static void readSettings()
{
    preferencesReady = preferences.begin("msx", false);
    if (!preferencesReady)
    {
        snprintf(statusMessage, sizeof(statusMessage), "NVS unavailable. Settings cannot be saved.");
        Serial.println(statusMessage);
        autoBoot = false;
        return;
    }
    const size_t size = preferences.getBytesLength("boot");
    if (!size) return;
    MsxBootSettings settings = {};
    uint8_t stored[sizeof(settings)];
    if (size > sizeof(stored) || preferences.getBytes("boot", stored, size) != size ||
        !MsxDecodeSettings(stored, size, settings))
    {
        snprintf(statusMessage, sizeof(statusMessage), "Invalid saved settings. Select a ROM and save.");
        Serial.println(statusMessage);
        autoBoot = false;
        return;
    }
    soundEnabled = settings.machine.sound;
    autoBoot = settings.machine.autoBoot;
    memcpy(selectedCartridges, settings.cartridges, sizeof(selectedCartridges));
    for (size_t i = 0; i < MsxProfileCount; ++i)
    {
        if (!strcmp(settings.machine.profile, MsxProfiles[i].id))
        {
            selectedProfile = i;
            selectedRamPages = settings.machine.ramPages;
            return;
        }
    }
    snprintf(statusMessage, sizeof(statusMessage), "Saved profile '%s' was not found on SD.", settings.machine.profile);
    Serial.println(statusMessage);
    autoBoot = false;
}

static const char *baseName(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static void drawMenu(unsigned row)
{
    frame(running ? "F12 CONFIGURATION - emulation paused" : "F12 CONFIGURATION - select boot ROM");
    char label[80];
    const MsxProfile &profile = MsxProfiles[selectedProfile];
    for (unsigned i = 0; i < MsxMenuCount; ++i)
    {
        const int y = MsxMenuRowY(i);
        if (i == row) display.fillRect(6, y, 308, MsxMenuRowHeight, 0x48);
        switch (i)
        {
        case MsxMenuResume: snprintf(label, sizeof(label), running ? "Resume emulation" : "No machine running"); break;
        case MsxMenuBios: snprintf(label, sizeof(label), "BIOS: %s", profile.name); break;
        case MsxMenuSlot1:
        case MsxMenuSlot2:
        {
            const unsigned slot = i - MsxMenuSlot1;
            snprintf(label, sizeof(label), "Slot %u: %.40s", slot + 1,
                     selectedCartridges[slot][0] ? baseName(selectedCartridges[slot]) : "<empty>");
            break;
        }
        case MsxMenuRam: snprintf(label, sizeof(label), "RAM on next boot: %d KiB", selectedRamPages * 16); break;
        case MsxMenuSound: snprintf(label, sizeof(label), "Sound: %s", soundEnabled ? "On" : "Off"); break;
        case MsxMenuAutoBoot: snprintf(label, sizeof(label), "Auto boot saved settings: %s", autoBoot ? "On" : "Off"); break;
        case MsxMenuSave: snprintf(label, sizeof(label), "Save BIOS + slots as boot default"); break;
        case MsxMenuBoot: snprintf(label, sizeof(label), "Boot BIOS + slots (cold reset)"); break;
        case MsxMenuRescan: snprintf(label, sizeof(label), "Rescan SD card / other BIOS profiles"); break;
        case MsxMenuUpdate: snprintf(label, sizeof(label), "Firmware update from SD"); break;
        case MsxMenuJoysticks: snprintf(label, sizeof(label), "USB joysticks - calibrate / test"); break;
        }
        text(10, y, label);
    }
    text(8, 156, "BIOS / slot / RAM changes apply on cold boot.", 0xdf);
    text(8, 188, "Arrows: choose  Enter: apply  Esc/F12: resume");
    const char *hint = profile.error;
    if (row == MsxMenuSlot1 || row == MsxMenuSlot2)
        hint = selectedCartridges[row - MsxMenuSlot1];
    messageLines(statusMessage[0] ? statusMessage : hint);
    video.show();
}

static void backupScreen(bool restore)
{
    for (int y = 0; y < MsxBoard::Height; ++y)
    {
        uint8_t *line = video.dmaBuffer->getLineAddr8(y);
        uint8_t *backup = screenBackup + y * MsxBoard::Width;
        if (restore) memcpy(line, backup, MsxBoard::Width);
        else memcpy(backup, line, MsxBoard::Width);
    }
    if (restore) video.show();
}

static bool chooseSdFile(const char *title, const char *extension, bool allowEject,
                         char selection[MsxSdPathCapacity])
{
    char directory[MsxSdPathCapacity] = "/";
    if (*selection)
    {
        snprintf(directory, sizeof(directory), "%s", selection);
        MsxParentSdPath(directory);
    }
    MsxFileEntry entries[MsxBrowserPageSize];
    size_t count = 0, total = 0, selected = 0, first = 0;
    bool reload = true, redraw = true;
    char warning[160] = "";
    MsxKeyboardClearEvents();
    for (;;)
    {
        if (reload)
        {
            if (!MsxReadDirectoryPage(directory, extension, allowEject, first, entries, count, total,
                                      warning, sizeof(warning)))
            {
                snprintf(statusMessage, sizeof(statusMessage), "%s", warning);
                if (strcmp(directory, "/"))
                {
                    strcpy(directory, "/");
                    selected = first = 0;
                    continue;
                }
                return false;
            }
            // A changed/removed directory entry must not leave a selection outside the page.
            if (total && selected >= total)
            {
                selected = 0;
                first = 0;
                continue;
            }
            reload = false;
            redraw = true;
        }
        if (redraw)
        {
            frame(title);
            char label[80];
            snprintf(label, sizeof(label), "%.50s", directory);
            text(8, 40, label, 0xdf);
            for (size_t i = 0; i < count; ++i)
            {
                if (first + i == selected)
                    display.fillRect(6, MsxMenuRowY(i), 308, MsxMenuRowHeight, 0x48);
                snprintf(label, sizeof(label), "%s%.47s",
                         entries[i].kind == MsxFileKind::Directory ? "> " : "  ", entries[i].name);
                text(8, MsxMenuRowY(i), label);
            }
            if (!total) text(8, MsxMenuRowY(0), "No matching files. Esc returns to menu.");
            snprintf(label, sizeof(label), "Page %u/%u   %u entries",
                     static_cast<unsigned>(first / MsxBrowserPageSize + 1),
                     static_cast<unsigned>(std::max<size_t>(1, (total + MsxBrowserPageSize - 1) / MsxBrowserPageSize)),
                     static_cast<unsigned>(total));
            text(8, 178, label);
            text(8, 188, "Enter: select  Left: up  PgUp/Dn  Esc: back");
            if (*warning) messageLines(warning);
            else if (count && selected >= first && selected - first < count)
                messageLines(entries[selected - first].name);
            video.show();
            redraw = false;
        }
        const uint8_t key = MsxKeyboardMenuKey();
        if (!key) { delay(10); continue; }
        if (key == 41 || key == 69) return false;
        if (key == 76 && allowEject)
        {
            selection[0] = '\0';
            return true;
        }
        if (key == 80)
        {
            MsxParentSdPath(directory);
            selected = first = 0;
            reload = true;
            continue;
        }
        if ((key == 81 || key == 82 || key == 75 || key == 78 || key == 79) && total)
        {
            if (key == 81 || key == 82) selected = MsxMoveSelection(selected, key == 82 ? -1 : 1, total);
            else if (key == 75) selected = selected >= MsxBrowserPageSize ? selected - MsxBrowserPageSize : 0;
            else selected = std::min(selected + MsxBrowserPageSize, total - 1);
            const size_t page = selected / MsxBrowserPageSize * MsxBrowserPageSize;
            if (page != first) { first = page; reload = true; }
            redraw = true;
        }
        else if (key == 40 && count && selected >= first && selected - first < count)
        {
            const MsxFileEntry &entry = entries[selected - first];
            if (entry.kind == MsxFileKind::Eject)
            {
                selection[0] = '\0';
                return true;
            }
            if (entry.kind == MsxFileKind::Parent) MsxParentSdPath(directory);
            else
            {
                char path[MsxSdPathCapacity];
                if (!MsxJoinSdPath(directory, entry.name, path, sizeof(path)))
                {
                    snprintf(warning, sizeof(warning), "Invalid SD path or path exceeds 239 bytes.");
                    Serial.println(warning);
                    redraw = true;
                    continue;
                }
                if (entry.kind == MsxFileKind::File)
                {
                    snprintf(selection, MsxSdPathCapacity, "%s", path);
                    return true;
                }
                snprintf(directory, sizeof(directory), "%s", path);
            }
            selected = first = 0;
            reload = true;
        }
    }
}

static void selectCartridge(unsigned slot, const char *path)
{
    snprintf(selectedCartridges[slot], sizeof(selectedCartridges[slot]), "%s", path);
    snprintf(statusMessage, sizeof(statusMessage), "Slot %u %s. Choose cold reset to apply; save for auto boot.",
             slot + 1, *path ? "selected" : "ejected");
}

static void chooseCartridge(unsigned slot)
{
    char candidate[MsxSdPathCapacity], title[40];
    snprintf(candidate, sizeof(candidate), "%s", selectedCartridges[slot]);
    snprintf(title, sizeof(title), "Cartridge slot %u - choose .ROM", slot + 1);
    if (!chooseSdFile(title, ".rom", true, candidate)) return;
    if (*candidate)
    {
        char path[MsxSdPathCapacity + 8];
        snprintf(path, sizeof(path), "/sdcard%s", candidate);
        if (!MsxValidateCartridge(path, statusMessage, sizeof(statusMessage)))
        {
            Serial.println(statusMessage);
            return;
        }
    }
    selectCartridge(slot, candidate);
    MsxKeyboardClearEvents();
}

static void firmwareProgress(const char *stage, uint8_t percent)
{
    frame("Firmware update");
    text(8, MsxMenuRowY(0), stage);
    display.drawRect(8, 72, 304, 12, 255);
    display.fillRect(10, 74, 300 * std::min<unsigned>(percent, 100) / 100, 8, 0xdf);
    char label[16];
    snprintf(label, sizeof(label), "%u%%", percent);
    text(8, 90, label);
    text(8, 188, "Do not switch off or remove the SD card.");
    video.show();
}

static void updateFirmware()
{
    char path[MsxSdPathCapacity] = "";
    if (!chooseSdFile("Firmware update - choose .FLH", ".flh", false, path)) return;
    MsxKeyboardClearEvents();
    bool install = false, redraw = true;
    for (;;)
    {
        if (redraw)
        {
            frame("Firmware update - confirmation");
            text(8, MsxMenuRowY(0), "Install selected MSX firmware and reboot?");
            text(8, MsxMenuRowY(1), "Current machine state will be lost.");
            display.fillRect(6, MsxMenuRowY(3 + (install ? 0 : 1)), 308, MsxMenuRowHeight, 0x48);
            text(8, MsxMenuRowY(3), "Y  Yes - install and reboot");
            text(8, MsxMenuRowY(4), "N  No  - cancel update");
            text(8, 188, "Y/N or arrows + Enter. Esc/F12 cancels.");
            messageLines(path);
            video.show();
            redraw = false;
        }
        const uint8_t key = MsxKeyboardMenuKey();
        if (!key) { delay(10); continue; }
        if (key == 41 || key == 69 || key == 17 || (key == 40 && !install)) return;
        if (key == 82 || key == 81 || key == 79 || key == 80) { install = !install; redraw = true; }
        else if (key == 28 || (key == 40 && install)) break;
    }
    if (!MsxJoystickHostPause())
    {
        snprintf(statusMessage, sizeof(statusMessage), "Cannot pause joystick host for firmware update.");
        Serial.println(statusMessage);
        return;
    }
    if (!MsxInstallFirmware(path, firmwareProgress, statusMessage, sizeof(statusMessage)))
    {
        MsxJoystickHostResume();
        return;
    }
    frame("Firmware update complete");
    text(8, MsxMenuRowY(0), "Verified. Rebooting into new firmware...");
    video.show();
    delay(1500);
    ESP.restart();
}

static bool sameJoystick(const MsxJoystickSnapshot &a, const MsxJoystickSnapshot &b)
{
    return b.connected && a.generation == b.generation && a.vid == b.vid && a.pid == b.pid &&
           a.endpoint == b.endpoint && a.length == b.length;
}

static void calibrateJoystick(unsigned port)
{
    MsxJoystickSnapshot identity;
    MsxJoystickSnapshotFor(port, identity);
    if (!identity.connected || !identity.length)
    {
        snprintf(statusMessage, sizeof(statusMessage), "Joystick %u has no reports. Check low-speed pad connection.", port + 1);
        return;
    }
    static const char *poses[] = {
        "Release ALL controls (neutral)", "Hold UP only",
        "Hold RIGHT only", "Hold DOWN only", "Hold LEFT only",
        "Hold fire button A only", "Hold fire button B only"
    };
    MsxJoystickSample samples[7] = {};
    MsxKeyboardClearEvents();
    for (unsigned pose = 0; pose < 7; ++pose)
    {
        char title[60];
        snprintf(title, sizeof(title), "Joystick %u calibration - step %u/7", port + 1, pose + 1);
        frame(title);
        text(8, 48, poses[pose]);
        text(8, 64, "Keep holding while the sample is collected.");
        text(8, 80, "Use the same stick/D-pad for all directions.");
        text(8, 96, "Release other buttons before each step.");
        text(8, 188, "Enter: capture  Esc/F12: cancel unchanged");
        video.show();
        for (;;)
        {
            MsxJoystickSnapshot current;
            MsxJoystickSnapshotFor(port, current);
            if (!sameJoystick(identity, current))
            {
                snprintf(statusMessage, sizeof(statusMessage), "Joystick disconnected or report changed; calibration cancelled.");
                return;
            }
            const uint8_t key = MsxKeyboardMenuKey();
            if (key == 41 || key == 69)
            {
                snprintf(statusMessage, sizeof(statusMessage), "Calibration cancelled; previous mapping kept.");
                return;
            }
            if (key == 40) break;
            delay(10);
        }
        text(8, 112, "Hold until the next prompt appears.", 0xdf);
        video.show();
        bool first = true;
        uint8_t baseline[8] = {};
        const uint32_t start = millis();
        while (millis() - start < 600)
        {
            const uint8_t key = MsxKeyboardMenuKey();
            if (key == 41 || key == 69)
            {
                snprintf(statusMessage, sizeof(statusMessage), "Calibration cancelled; previous mapping kept.");
                return;
            }
            MsxJoystickSnapshot current;
            MsxJoystickSnapshotFor(port, current);
            if (!sameJoystick(identity, current))
            {
                snprintf(statusMessage, sizeof(statusMessage), "Joystick changed during capture; retry calibration.");
                return;
            }
            // Let queued USB reports settle before capture. Learn noise only at
            // neutral: an actual button/axis transition is not random noise.
            if (millis() - start >= 250)
            {
                if (first) { memcpy(baseline, current.report, identity.length); first = false; }
                if (!pose)
                    for (unsigned byte = 0; byte < identity.length; ++byte)
                        samples[pose].changed[byte] |= baseline[byte] ^ current.report[byte];
                memcpy(samples[pose].bytes, current.report, identity.length);
            }
            delay(10);
        }
        Serial.printf("Joystick %u calibration %s:", port + 1, poses[pose]);
        for (unsigned byte = 0; byte < identity.length; ++byte)
            Serial.printf(" %02x", samples[pose].bytes[byte]);
        Serial.println();
        if (pose)
        {
            bool changed = false;
            for (unsigned byte = 0; byte < identity.length; ++byte)
                changed |= ((samples[pose].bytes[byte] ^ samples[0].bytes[byte]) &
                            ~samples[0].changed[byte]) != 0;
            if (!changed)
            {
                snprintf(statusMessage, sizeof(statusMessage), "No control captured for %s. Hold it until the next prompt.", poses[pose]);
                Serial.println(statusMessage);
                return;
            }
        }
        MsxKeyboardClearEvents();
    }
    MsxJoystickMapping mapping = {};
    if (!MsxBuildJoystickCardinalMapping(identity.length, samples, mapping, statusMessage, sizeof(statusMessage)))
    {
        Serial.printf("Joystick calibration: %s\n", statusMessage);
        return;
    }
    const bool saved = MsxJoystickSave(port, identity, mapping, statusMessage, sizeof(statusMessage));
    Serial.printf("Joystick calibration %s: %s\n", saved ? "saved" : "failed", statusMessage);
}

static void joystickMenu()
{
    unsigned selected = 0;
    uint32_t lastDraw = 0;
    MsxJoystickSnapshot displayed[2] = {};
    bool haveDisplay = false;
    MsxKeyboardClearEvents();
    for (;;)
    {
        if (!lastDraw || millis() - lastDraw >= 100)
        {
            MsxJoystickSnapshot current[2];
            bool changed = !haveDisplay || !lastDraw;
            for (unsigned port = 0; port < 2; ++port)
            {
                MsxJoystickSnapshotFor(port, current[port]);
                changed |= MsxJoystickDisplayChanged(displayed[port], current[port]);
            }
            if (changed)
            {
                frame("USB joystick calibration / live input");
                text(8, 40, "Low-speed USB only; one pad per connector.", 0xdf);
                for (unsigned port = 0; port < 2; ++port)
                {
                    const MsxJoystickSnapshot &state = current[port];
                    char label[80];
                    const int y = 56 + port * 48;
                    if (port == selected) display.fillRect(6, y, 308, 8, 0x48);
                    snprintf(label, sizeof(label), "Joystick %u: %s", port + 1, !state.connected ? "disconnected" :
                             !state.length ? "waiting for report" : state.calibrated ? "calibrated" : "needs calibration");
                    text(8, y, label);
                    snprintf(label, sizeof(label), "VID %04X PID %04X  EP %02X  len %u",
                             state.vid, state.pid, state.endpoint, state.length);
                    text(8, y + 8, label);
                    snprintf(label, sizeof(label), "U:%u D:%u L:%u R:%u A:%u B:%u",
                             !!(state.buttons & 1), !!(state.buttons & 2), !!(state.buttons & 4),
                             !!(state.buttons & 8), !!(state.buttons & 16), !!(state.buttons & 32));
                    text(8, y + 16, label);
                    snprintf(label, sizeof(label), "Raw %02X %02X %02X %02X %02X %02X %02X %02X",
                             state.report[0], state.report[1], state.report[2], state.report[3],
                             state.report[4], state.report[5], state.report[6], state.report[7]);
                    text(8, y + 24, label);
                }
                text(8, 164, "USB1 -> port1   USB2 -> port2; no mirroring.");
                text(8, 180, "Arrows: port  Enter: calibrate  Del: clear");
                text(8, 188, "Esc/F12: back. Calibration saves immediately.");
                messageLines(statusMessage);
                video.show();
                for (unsigned port = 0; port < 2; ++port) displayed[port] = current[port];
                haveDisplay = true;
            }
            lastDraw = millis();
        }
        const uint8_t key = MsxKeyboardMenuKey();
        if (key == 41 || key == 69) { MsxKeyboardClearEvents(); return; }
        if (key == 81 || key == 82 || key == 79 || key == 80) { selected ^= 1; lastDraw = 0; }
        else if (key == 40) { calibrateJoystick(selected); lastDraw = 0; }
        else if (key == 76) { MsxJoystickClear(selected, statusMessage, sizeof(statusMessage)); lastDraw = 0; }
        delay(10);
    }
}

static void menu()
{
    if (running) backupScreen(false);
    MsxAudioEnable(false);
    MsxKeyboardClearEvents();
    unsigned row = running ? MsxMenuResume : MsxMenuBios;
    bool leave = false;
    bool redraw = true;
    while (!leave)
    {
        if (redraw) { drawMenu(row); redraw = false; }
        const uint8_t key = MsxKeyboardMenuKey();
        if (!key) { delay(10); continue; }
        redraw = true;
        if (key == 82) row = MsxMoveSelection(row, -1, MsxMenuCount);
        else if (key == 81) row = MsxMoveSelection(row, 1, MsxMenuCount);
        else if (key == 76 && (row == MsxMenuSlot1 || row == MsxMenuSlot2))
            selectCartridge(row - MsxMenuSlot1, "");
        else if ((key == 41 || key == 69) && running) leave = true;
        else if (key == 40 || key == 79 || key == 80)
        {
            const int direction = key == 80 ? -1 : 1;
            switch (row)
            {
            case MsxMenuResume:
                if (key == 40 && running) leave = true;
                break;
            case MsxMenuBios:
                selectedProfile = MsxMoveSelection(selectedProfile, direction, MsxProfileCount);
                selectedRamPages = MsxProfiles[selectedProfile].ramPages;
                statusMessage[0] = '\0';
                break;
            case MsxMenuSlot1:
            case MsxMenuSlot2:
                if (key == 40) chooseCartridge(row - MsxMenuSlot1);
                break;
            case MsxMenuRam:
                if (direction > 0) selectedRamPages = selectedRamPages == 32 ? 4 : selectedRamPages * 2;
                else selectedRamPages = selectedRamPages == 4 ? 32 : selectedRamPages / 2;
                break;
            case MsxMenuSound:
                soundEnabled = !soundEnabled;
                if (soundEnabled && !audioReady)
                    snprintf(statusMessage, sizeof(statusMessage), "Audio driver unavailable; sound remains silent.");
                break;
            case MsxMenuAutoBoot: autoBoot = !autoBoot; break;
            case MsxMenuSave:
                if (key == 40)
                {
                    if (MsxValidateProfile(MsxProfiles[selectedProfile]))
                    {
                        if (validateCartridges()) saveSettings();
                    }
                    else snprintf(statusMessage, sizeof(statusMessage), "%s", MsxProfiles[selectedProfile].error);
                }
                break;
            case MsxMenuBoot:
                if (key == 40)
                {
                    if (MsxValidateProfile(MsxProfiles[selectedProfile]))
                    {
                        if (!validateCartridges()) break;
                        bootRequested = true;
                        exitRequested = running;
                        leave = true;
                        statusMessage[0] = '\0';
                    }
                    else snprintf(statusMessage, sizeof(statusMessage), "%s", MsxProfiles[selectedProfile].error);
                }
                break;
            case MsxMenuRescan:
                if (key == 40)
                {
                    char previous[33];
                    snprintf(previous, sizeof(previous), "%s", MsxProfiles[selectedProfile].id);
                    MsxMountSd();
                    MsxScanProfiles();
                    selectedProfile = 0;
                    for (size_t i = 0; i < MsxProfileCount; ++i)
                        if (!strcmp(MsxProfiles[i].id, previous)) selectedProfile = i;
                    selectedRamPages = MsxProfiles[selectedProfile].ramPages;
                    snprintf(statusMessage, sizeof(statusMessage), "%u profiles. Select ROM with Left/Right.",
                             static_cast<unsigned>(MsxProfileCount));
                }
                break;
            case MsxMenuUpdate:
                if (key == 40) updateFirmware();
                break;
            case MsxMenuJoysticks:
                if (key == 40) joystickMenu();
                break;
            }
        }
    }
    MsxKeyboardClearEvents();
    if (running && !exitRequested) backupScreen(true);
    MsxAudioEnable(audioReady && soundEnabled && !exitRequested);
}

void msxPresent(const uint8_t *pixels, int width, int height, const uint32_t *palette)
{
    if (!pixels || !palette || width <= 0 || height <= 0)
    {
        msxReportError("Invalid core video frame.");
        return;
    }
    uint8_t colors[256];
    for (unsigned i = 0; i < 256; ++i)
    {
        const uint32_t rgb = palette[i];
        colors[i] = ((rgb >> 21) & 7) | ((rgb >> 10) & 0x38) | (rgb & 0xc0);
    }
    const int outputWidth = std::min(width, MsxBoard::Width);
    const int outputHeight = std::min(height, MsxBoard::Height);
    const int left = (MsxBoard::Width - outputWidth) / 2;
    const int top = (MsxBoard::Height - outputHeight) / 2;
    for (int y = 0; y < MsxBoard::Height; ++y)
    {
        uint8_t *line = video.dmaBuffer->getLineAddr8(y);
        if (y < top || y >= top + outputHeight) memset(line, colors[0], MsxBoard::Width);
        else
        {
            memset(line, colors[0], left);
            const uint8_t *source = pixels + ((y - top) * height / outputHeight) * width;
            if (width == outputWidth)
                for (int x = 0; x < outputWidth; ++x) line[left + x] = colors[source[x]];
            else
                for (int x = 0; x < outputWidth; ++x) line[left + x] = colors[source[x * width / outputWidth]];
            memset(line + left + outputWidth, colors[0], MsxBoard::Width - left - outputWidth);
        }
    }
    video.show();
}

void msxPollKeyboard(uint8_t matrix[16])
{
    for (uint8_t key = MsxKeyboardMenuKey(); key; key = MsxKeyboardMenuKey())
    {
        if (key == 69)
        {
            statusMessage[0] = '\0';
            menu();
            break;
        }
    }
    MsxKeyboardMatrix(matrix);
}

bool msxShouldExit() { return exitRequested; }
uint16_t msxPollJoysticks() { return MsxJoysticksRead(); }
void msxSubmitAudio(const int16_t *samples, unsigned count) { MsxAudioSubmit(samples, count); }

void msxReportError(const char *message)
{
    snprintf(statusMessage, sizeof(statusMessage), "%s", message);
    Serial.printf("MSX: %s\n", statusMessage);
    exitRequested = true;
    bootRequested = false;
}

static void fatal(const char *message)
{
    Serial.println(message);
    if (videoReady)
    {
        frame("INITIALIZATION FAILED");
        messageLines(message);
        video.show();
    }
    while (true) delay(1000);
}

void setup()
{
    Serial.begin(115200);
    Serial.printf("\nESP32 MSX / fMSX %s\n", FW_VERSION);
    if (!psramFound()) fatal("8 MiB OPI PSRAM is required. Check N16R8 board configuration.");
    video.bufferCount = 1;
    const PinConfig pins(-1, -1, -1, MsxBoard::RedLow, MsxBoard::RedHigh,
                         -1, -1, -1, -1, MsxBoard::GreenLow, MsxBoard::GreenHigh,
                         -1, -1, -1, MsxBoard::BlueLow, MsxBoard::BlueHigh,
                         MsxBoard::HSync, MsxBoard::VSync);
    const Mode timing(8, 48, 24, MsxBoard::Width, 10, 2, 33, MsxBoard::Height,
                      12587500, 0, 0, 2);
    if (!video.init(pins, timing, 8)) fatal("VGA framebuffer allocation failed.");
    // The shared VGA driver temporarily routes its pixel clock to the LED pin.
    pinMode(MsxBoard::RgbLed, INPUT);
    neopixelWrite(MsxBoard::RgbLed, 0, 0, 0);
    pinMode(MsxBoard::RgbLed, INPUT);
    videoReady = video.start();
    if (!videoReady) fatal("VGA output could not start.");
    screenBackup = static_cast<uint8_t *>(heap_caps_malloc(MsxBoard::Width * MsxBoard::Height,
                                                          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!screenBackup) fatal("Cannot allocate F12 screen backup.");
    if (!MsxKeyboardStart()) fatal("USB keyboard host initialization failed. See UART log.");
    audioReady = MsxAudioStart();
    MsxMountSd();
    MsxScanProfiles();
    readSettings();
    if (!MsxJoysticksStart())
    {
        snprintf(statusMessage, sizeof(statusMessage), "Joystick USB host unavailable; see UART log.");
        Serial.println(statusMessage);
    }
    frame("INITIALIZING");
    text(8, 53, "USB keyboard: GPIO19/20");
    text(8, 70, "Press F12 for ROM selection and settings.");
    text(8, 87, MsxProfiles[selectedProfile].name);
    if (!audioReady) messageLines("Audio unavailable. Check UART log.");
    video.show();
    const uint32_t start = millis();
    bool interrupted = false;
    while (millis() - start < 2500)
    {
        if (MsxKeyboardMenuKey() == 69) { interrupted = true; break; }
        delay(10);
    }
    bootRequested = !interrupted && autoBoot && MsxProfiles[selectedProfile].available && validateCartridges();
}

void loop()
{
    if (!bootRequested) menu();
    if (!bootRequested) return;
    bootRequested = false;
    exitRequested = false;
    MsxProfile &profile = MsxProfiles[selectedProfile];
    if (!MsxValidateProfile(profile))
    {
        snprintf(statusMessage, sizeof(statusMessage), "%s", profile.error);
        return;
    }
    if (!validateCartridges()) return;
    char directory[96];
    snprintf(directory, sizeof(directory), "/sdcard/msx/bios/%s", profile.id);
    frame("BOOTING");
    text(8, 53, profile.name);
    video.show();
    running = true;
    MsxAudioEnable(audioReady && soundEnabled);
    char slotPaths[2][MsxSdPathCapacity + 8] = {};
    for (unsigned slot = 0; slot < 2; ++slot)
        if (selectedCartridges[slot][0])
            snprintf(slotPaths[slot], sizeof(slotPaths[slot]), "/sdcard%s", selectedCartridges[slot]);
    const bool success = MsxCoreRun(directory, profile.model, selectedRamPages, slotPaths[0], slotPaths[1]);
    running = false;
    MsxAudioEnable(false);
    if (!success && !statusMessage[0])
        snprintf(statusMessage, sizeof(statusMessage), "Emulator initialization failed. See UART log.");
}
