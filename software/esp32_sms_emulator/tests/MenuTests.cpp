#include <assert.h>
#include <deque>
#define FW_VERSION "1.00"
#include "main.cpp"

std::vector<std::string> MenuText;
std::map<std::string, MenuNode> MenuFiles;
uint32_t MenuTime = 0;
bool MenuWriteFailure = false, MenuReadFailure = false;
TestSerial Serial;
TestEsp ESP;
TestSd SD_MMC;
static std::deque<uint8_t> keys;
static unsigned audioStops = 0, audioStarts = 0, flashCalls = 0, pauses = 0, resumes = 0;
static bool pauseFailure = false;
static std::string flashedPath;

bool SmsKeyboardStart() { return true; }
void SmsKeyboardClearEvents() {}
uint8_t SmsKeyboardMenuKey()
{
    assert(!keys.empty() && "Menu consumed more input than expected.");
    const uint8_t value = keys.front();
    keys.pop_front();
    return value;
}
void SmsKeyboardReport(uint8_t report[8]) { memset(report, 0, 8); }
bool SmsAudioStart() { return true; }
void SmsAudioEnable(bool enabled) { if (enabled) ++audioStarts; else ++audioStops; }
void SmsAudioSubmit(const int16_t *, unsigned) {}
bool SmsJoysticksStart() { return true; }
uint16_t SmsJoysticksRead() { return 0; }
void SmsJoystickSnapshotFor(unsigned, SmsJoystickSnapshot &pad) { pad = {}; }
bool SmsJoystickHostPause() { ++pauses; return !pauseFailure; }
void SmsJoystickHostResume() { ++resumes; }
bool SmsJoystickSave(unsigned, const SmsJoystickSnapshot &, const SmsJoystickMapping &, char *, size_t) { return true; }
bool SmsJoystickClear(unsigned, char *, size_t) { return true; }
bool SmsInstallFirmware(const char *path, void (*)(const char *, uint8_t), char *message, size_t capacity)
{
    ++flashCalls;
    flashedPath = path;
    snprintf(message, capacity, "Mock OTA failure; old boot retained.");
    return false;
}

static void input(std::initializer_list<uint8_t> values)
{
    assert(keys.empty());
    keys.assign(values);
}

static bool saw(const char *text)
{
    return std::any_of(MenuText.begin(), MenuText.end(),
                       [text](const std::string &line) { return line.find(text) != std::string::npos; });
}

int main()
{
    MenuFiles["/"] = {true, {}};
    MenuFiles["/sms"] = {true, {}};
    MenuFiles["/sms/roms"] = {true, {}};
    for (unsigned i = 0; i < 12; ++i)
    {
        char path[64];
        snprintf(path, sizeof(path), "/sms/roms/game%02u.%s", i, i == 11 ? "ROM" : "sms");
        MenuFiles[path] = {false, std::vector<uint8_t>(32768)};
    }
    MenuFiles["/sms/roms/ignored.txt"] = {false, {1}};
    MenuFiles["/update.FLH"] = {false, {1}};
    sdReady = nvsReady = audioReady = true;
    std::vector<uint8_t> framebuffer(SmsCore::Width * SmsCore::Height);
    pixels = framebuffer.data();
    char result[SmsPathSize] = {};
    input({78, 40});
    assert(browse("ROM test", ".sms", result));
    assert(!strcmp(result, "/sms/roms/game09.sms"));
    input({78, 81, 81, 40});
    assert(browse("ROM test", ".sms", result));
    assert(!strcmp(result, "/sms/roms/game11.ROM"));
    input({40, 41});
    assert(!browse("Parent directory test", ".sms", result));
    input({81, 40});
    loadRom();
    assert(running && wantResume && !strcmp(settings.rom, "/sms/roms/game00.sms"));
    running = true;
    wantResume = false;
    input({69});
    assert(!browse("F12 test", ".sms", result) && wantResume);
    wantResume = false;
    MenuReadFailure = true;
    const SmsSettings previous = settings;
    input({81, 81, 40, 28});
    loadRom();
    assert(!memcmp(&previous, &settings, sizeof(settings)) && running && !wantResume);
    assert(strstr(status, "SD read failed"));
    MenuReadFailure = false;
    const uint32_t frames = machine.frames;
    input({81, 81, 40, 79, 41, 81, 40, 79, 81, 40, 41, 69});
    menu();
    assert(settings.pal && !settings.joystick[0] && settings.joystick[1]);
    assert(machine.frames == frames && machine.frameMicros() < 17000);
    assert(audioStops == 1 && audioStarts == 1);
    assert(waitForPauseRelease);
    assert(saw("F12 menu") && saw("USB joysticks") && saw("Configuration"));
    assert(saw("needs") || strstr(status, "Connect a low-speed"));
    assert(saveSettings() && pauses == 1 && resumes == 1);
    MenuWriteFailure = true;
    assert(!saveSettings() && pauses == 2 && resumes == 2 && strstr(status, "NVS write failed"));
    MenuWriteFailure = false;
    pauseFailure = true;
    assert(!saveSettings() && pauses == 3 && resumes == 2);
    pauseFailure = false;
    input({81, 40, 17});
    firmwareUpdate();
    assert(!flashCalls);
    input({81, 40, 28});
    firmwareUpdate();
    assert(flashCalls == 1 && flashedPath == "/update.FLH" && pauses == 4 && resumes == 3);
    assert(running && machine.frames == frames && strstr(status, "Mock OTA failure"));
    assert(keys.empty());
    puts("F12 menus, ROM browser paging/filtering, cancellation, failed-load retention, joystick options and update/save gating passed.");
}
