#include <assert.h>
#include <deque>
#define FW_VERSION "1.00"
#include "main.cpp"

std::vector<std::string> MenuText;
std::map<std::string, MenuNode> MenuFiles;
uint32_t MenuTime = 0;
bool MenuWriteFailure = false, MenuReadFailure = false;
bool MenuSdOpenFailure = false;
unsigned MenuSdOpens = 0, MenuEntryOpens = 0, MenuDirectoryReads = 0;
int MenuAllocationBudget = -1, MenuDirectoryReadBudget = -1;
size_t MenuBrowserBytes = 0, MenuBrowserMaxBytes = 0;
size_t MenuInternalFreeBytes = 256 * 1024;
bool MenuFailInternalCore = false, MenuFailExternalCore = false;
unsigned MenuInternalCoreAllocations = 0, MenuExternalCoreAllocations = 0;
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

static void testCoreMemory()
{
    SmsCore core;
    SmsSettings config;
    std::vector<uint8_t> rom(32768), buffer(SmsCore::Width * SmsCore::Height);
    char message[160];
    assert(core.load(rom.data(), rom.size(), config, buffer.data(), message, sizeof(message)));
    assert(MenuInternalCoreAllocations == 1 && !MenuExternalCoreAllocations);
    MenuInternalFreeBytes = 64 * 1024;
    assert(core.load(rom.data(), rom.size(), config, buffer.data(), message, sizeof(message)));
    assert(MenuInternalCoreAllocations == 1 && MenuExternalCoreAllocations == 1);
    MenuInternalFreeBytes = 256 * 1024;
    MenuFailInternalCore = true;
    assert(core.load(rom.data(), rom.size(), config, buffer.data(), message, sizeof(message)));
    assert(MenuInternalCoreAllocations == 2 && MenuExternalCoreAllocations == 2);
    core.runFrame(0, true);
    MenuFailExternalCore = true;
    assert(!core.load(rom.data(), rom.size(), config, buffer.data(), message, sizeof(message)));
    assert(strstr(message, "memory") && core.frames == 1);
    core.runFrame(0, false);
    assert(core.frames == 2);
    MenuFailInternalCore = MenuFailExternalCore = false;
}

static void testFramebufferMemory()
{
    constexpr size_t size = SmsCore::Width * SmsCore::Height;
    constexpr size_t threshold = size + 64 * 1024 + 2 * sizeof(SMS_Core);
    const unsigned internal = MenuInternalCoreAllocations, external = MenuExternalCoreAllocations;
    MenuInternalFreeBytes = threshold;
    uint8_t *buffer = allocateFramebuffer();
    assert(buffer && MenuInternalCoreAllocations == internal + 1 && MenuExternalCoreAllocations == external);
    for (size_t i = 0; i < size; ++i) assert(!buffer[i]);
    free(buffer);
    MenuInternalFreeBytes = threshold - 1;
    buffer = allocateFramebuffer();
    assert(buffer && MenuInternalCoreAllocations == internal + 1 && MenuExternalCoreAllocations == external + 1);
    free(buffer);
    MenuInternalFreeBytes = threshold;
    MenuFailInternalCore = true;
    buffer = allocateFramebuffer();
    assert(buffer && MenuInternalCoreAllocations == internal + 2 && MenuExternalCoreAllocations == external + 2);
    free(buffer);
    MenuFailExternalCore = true;
    assert(!allocateFramebuffer());
    MenuFailInternalCore = MenuFailExternalCore = false;
    MenuInternalFreeBytes = 256 * 1024;
    puts("Framebuffer SRAM reserve, fragmentation fallback, zeroing and allocation failure passed.");
}

static void testVideoPresentation()
{
    for (unsigned height : {192u, 224u, 240u, 192u})
    {
        SmsSettings config;
        char message[160];
        std::vector<uint8_t> rom(32768);
        const uint8_t program[] = {
            0xf3, 0x3e, uint8_t(height == 192 ? 0x04 : 0x06), 0xd3, 0xbf, 0x3e, 0x80, 0xd3, 0xbf,
            0x3e, uint8_t(height == 224 ? 0x50 : 0x48), 0xd3, 0xbf, 0x3e, 0x81, 0xd3, 0xbf, 0xc3, 0x11, 0x00
        };
        memcpy(rom.data(), program, sizeof(program));
        assert(machine.load(rom.data(), rom.size(), config, pixels, message, sizeof(message)));
        machine.runFrame(0, true);
        machine.runFrame(0, true);
        assert(machine.height() == height);
        for (unsigned y = 0; y < 240; ++y)
            for (unsigned x = 0; x < 256; ++x) pixels[y * 256 + x] = uint8_t(x + y + 1);
        video.dmaBuffer->flushes = 0;
        present();
        assert(video.dmaBuffer->flushes == 240);
        auto checkPixels = [height]()
        {
            const unsigned top = (240 - height) / 2;
            for (unsigned y = 0; y < 240; ++y)
                for (unsigned x = 0; x < 320; ++x)
                    assert(video.dmaBuffer->lines[y][x] ==
                           (y >= top && y < top + height ? pixels[(y - top) * 256 + x * 4 / 5] : 0));
        };
        checkPixels();
        present();
        assert(video.dmaBuffer->flushes == 240 + height);
        memset(video.dmaBuffer->lines, 0xa5, sizeof(video.dmaBuffer->lines));
        present(true);
        assert(video.dmaBuffer->flushes == 480 + height);
        checkPixels();
    }
    puts("Internal-SRAM selection/fallback, failed-load retention and VGA scaling/border restoration passed.");
}

static void resetBrowserIo()
{
    assert(!MenuBrowserBytes);
    MenuSdOpens = MenuEntryOpens = MenuDirectoryReads = 0;
    MenuBrowserMaxBytes = 0;
}

static void testLargeBrowser()
{
    MenuFiles["/"] = {true, {}};
    MenuFiles["/sms"] = {true, {}};
    MenuFiles["/sms/roms"] = {true, {}};
    for (unsigned i = 0; i < 600; ++i)
    {
        char path[64];
        snprintf(path, sizeof(path), "/sms/roms/game%04u.SMS", i);
        MenuFiles[path] = {false, {1}};
    }
    MenuFiles["/sms/roms/ignored.txt"] = {false, {1}};
    MenuFiles["/sms/roms/System Volume Information"] = {true, {}};
    MenuFiles["/sms/roms/bad:name.sms"] = {false, {1}};
    MenuFiles["/sms/roms/" + std::string(240, 'x') + ".sms"] = {false, {1}};
    char result[SmsPathSize] = {};
    resetBrowserIo();
    keys.assign(537, 81);
    keys.insert(keys.end(), {78, 75, 82, 40});
    assert(browse("Large library", ".sms", result));
    assert(!strcmp(result, "/sms/roms/game0535.SMS"));
    assert(MenuSdOpens == 1 && MenuEntryOpens == 0 && MenuDirectoryReads == 605);
    assert(MenuBrowserMaxBytes <= 256 * 1024 && !MenuBrowserBytes);

    resetBrowserIo();
    keys.assign(65, 78);
    keys.push_back(40);
    assert(browse("Last page", ".sms", result));
    assert(!strcmp(result, "/sms/roms/game0599.SMS"));
    assert(MenuSdOpens == 1 && MenuEntryOpens == 0 && MenuDirectoryReads == 605);

    resetBrowserIo();
    input({78, 75, 82, 81, 40});
    assert(browse("First page", ".sms", result));
    assert(!strcmp(result, "/sms/roms/game0000.SMS"));
    assert(MenuSdOpens == 1 && MenuDirectoryReads == 605);

    MenuFiles["/sms/roms/game0600.rom"] = {false, {1}};
    resetBrowserIo();
    keys.assign(65, 78);
    keys.push_back(40);
    assert(browse("Refreshed library", ".sms", result));
    assert(!strcmp(result, "/sms/roms/game0600.rom"));
    assert(MenuSdOpens == 1 && MenuEntryOpens == 0 && MenuDirectoryReads == 606);

    MenuAllocationBudget = 0;
    assert(!browse("Allocation failure", ".sms", result));
    assert(strstr(status, "memory") && !MenuBrowserBytes);
    MenuAllocationBudget = 1;
    assert(!browse("Growth failure", ".sms", result));
    assert(strstr(status, "memory") && !MenuBrowserBytes);
    MenuAllocationBudget = -1;
    MenuDirectoryReadBudget = 12;
    assert(!browse("Read failure", ".sms", result));
    assert(strstr(status, "directory") && !MenuBrowserBytes);
    MenuDirectoryReadBudget = -1;
    MenuSdOpenFailure = true;
    assert(!browse("Open failure", ".sms", result));
    assert(strstr(status, "directory") && !MenuBrowserBytes);
    MenuSdOpenFailure = false;

    resetBrowserIo();
    input({81, 41});
    assert(!browse("Cancel library", ".sms", result));
    assert(MenuSdOpens == 1 && MenuDirectoryReads == 606 && !MenuBrowserBytes);

    MenuFiles.clear();
    MenuFiles["/"] = {true, {}};
    MenuFiles["/sms"] = {true, {}};
    MenuFiles["/sms/roms"] = {true, {}};
    MenuFiles["/sms/roms/collection"] = {true, {}};
    MenuFiles["/sms/roms/collection/nested.ROM"] = {false, {1}};
    MenuFiles["/sms/roms/root.sms"] = {false, {1}};
    resetBrowserIo();
    input({81, 40, 81, 40});
    assert(browse("Nested directory", ".sms", result));
    assert(!strcmp(result, "/sms/roms/collection/nested.ROM"));
    assert(MenuSdOpens == 2 && MenuEntryOpens == 0 && MenuDirectoryReads == 5);
    resetBrowserIo();
    input({81, 40, 40, 81, 81, 40});
    assert(browse("Return to parent", ".sms", result));
    assert(!strcmp(result, "/sms/roms/root.sms"));
    assert(MenuSdOpens == 3 && MenuEntryOpens == 0 && MenuDirectoryReads == 8);

    MenuFiles.clear();
    MenuFiles["/"] = {true, {}};
    input({82, 81, 78, 75, 40, 41});
    assert(!browse("Empty directory", ".sms", result));
    assert(saw("No matching files.") && !MenuBrowserBytes && keys.empty());
    MenuFiles.clear();
    puts("600-cartridge browsing: one metadata scan, zero file opens during navigation, bounded PSRAM and error cleanup passed.");
}

int main()
{
    sdReady = nvsReady = audioReady = true;
    std::vector<uint8_t> framebuffer(SmsCore::Width * SmsCore::Height);
    pixels = framebuffer.data();
    testCoreMemory();
    testFramebufferMemory();
    testVideoPresentation();
    testLargeBrowser();
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
