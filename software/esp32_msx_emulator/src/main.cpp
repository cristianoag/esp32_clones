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
static char statusMessage[160] = "";
static Preferences preferences;
static bool preferencesReady = false;

struct BootSettings
{
    uint32_t version;
    char profile[33];
    uint8_t ramPages;
    uint8_t sound;
    uint8_t autoBoot;
};

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
    text(8, 8, "RETRO HACKER  MSX / fMSX");
    text(268, 8, FW_VERSION);
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

static bool validRam(int pages)
{
    return pages == 4 || pages == 8 || pages == 16 || pages == 32;
}

static void saveSettings()
{
    if (!preferencesReady)
    {
        snprintf(statusMessage, sizeof(statusMessage), "NVS unavailable; settings were not saved.");
        Serial.println(statusMessage);
        return;
    }
    BootSettings settings = {};
    settings.version = 1;
    snprintf(settings.profile, sizeof(settings.profile), "%s", MsxProfiles[selectedProfile].id);
    settings.ramPages = selectedRamPages;
    settings.sound = soundEnabled;
    settings.autoBoot = autoBoot;
    if (preferences.putBytes("boot", &settings, sizeof(settings)) != sizeof(settings))
    {
        snprintf(statusMessage, sizeof(statusMessage), "NVS write failed; settings were not saved.");
        Serial.println(statusMessage);
        return;
    }
    snprintf(statusMessage, sizeof(statusMessage), "Boot profile and settings saved.");
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
    BootSettings settings = {};
    if (size != sizeof(settings) ||
        preferences.getBytes("boot", &settings, sizeof(settings)) != sizeof(settings) ||
        settings.version != 1 || !memchr(settings.profile, '\0', sizeof(settings.profile)) ||
        !validRam(settings.ramPages) || settings.sound > 1 || settings.autoBoot > 1)
    {
        snprintf(statusMessage, sizeof(statusMessage), "Invalid saved settings. Select a ROM and save.");
        Serial.println(statusMessage);
        autoBoot = false;
        return;
    }
    soundEnabled = settings.sound;
    autoBoot = settings.autoBoot;
    for (size_t i = 0; i < MsxProfileCount; ++i)
    {
        if (!strcmp(settings.profile, MsxProfiles[i].id))
        {
            selectedProfile = i;
            selectedRamPages = settings.ramPages;
            return;
        }
    }
    snprintf(statusMessage, sizeof(statusMessage), "Saved profile '%s' was not found on SD.", settings.profile);
    Serial.println(statusMessage);
    autoBoot = false;
}

static void drawMenu(unsigned row)
{
    frame(running ? "F12 CONFIGURATION - emulation paused" : "F12 CONFIGURATION - select boot ROM");
    char label[80];
    const MsxProfile &profile = MsxProfiles[selectedProfile];
    for (unsigned i = 0; i < 8; ++i)
    {
        const int y = 48 + i * 17;
        if (i == row) display.fillRect(6, y - 2, 308, 13, 0x48);
        switch (i)
        {
        case 0: snprintf(label, sizeof(label), running ? "Resume emulation" : "No machine running"); break;
        case 1: snprintf(label, sizeof(label), "ROM: %s", profile.name); break;
        case 2: snprintf(label, sizeof(label), "RAM on next boot: %d KiB", selectedRamPages * 16); break;
        case 3: snprintf(label, sizeof(label), "Sound: %s", soundEnabled ? "On" : "Off"); break;
        case 4: snprintf(label, sizeof(label), "Auto boot saved ROM: %s", autoBoot ? "On" : "Off"); break;
        case 5: snprintf(label, sizeof(label), "Save selected ROM as boot default"); break;
        case 6: snprintf(label, sizeof(label), "Boot selected ROM (cold reset)"); break;
        default: snprintf(label, sizeof(label), "Rescan SD card / other ROM profiles"); break;
        }
        text(10, y, label);
    }
    text(8, 188, "Arrows: choose  Enter: apply  Esc/F12: resume");
    messageLines(statusMessage[0] ? statusMessage : profile.error);
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

static void menu()
{
    if (running) backupScreen(false);
    MsxAudioEnable(false);
    MsxKeyboardClearEvents();
    unsigned row = running ? 0 : 1;
    bool leave = false;
    bool redraw = true;
    while (!leave)
    {
        if (redraw) { drawMenu(row); redraw = false; }
        const uint8_t key = MsxKeyboardMenuKey();
        if (!key) { delay(10); continue; }
        redraw = true;
        if (key == 82) row = (row + 7) % 8;
        else if (key == 81) row = (row + 1) % 8;
        else if ((key == 41 || key == 69) && running) leave = true;
        else if (key == 40 || key == 79 || key == 80)
        {
            const int direction = key == 80 ? -1 : 1;
            switch (row)
            {
            case 0:
                if (key == 40 && running) leave = true;
                break;
            case 1:
                selectedProfile = (selectedProfile + MsxProfileCount + direction) % MsxProfileCount;
                selectedRamPages = MsxProfiles[selectedProfile].ramPages;
                statusMessage[0] = '\0';
                break;
            case 2:
                if (direction > 0) selectedRamPages = selectedRamPages == 32 ? 4 : selectedRamPages * 2;
                else selectedRamPages = selectedRamPages == 4 ? 32 : selectedRamPages / 2;
                break;
            case 3:
                soundEnabled = !soundEnabled;
                if (soundEnabled && !audioReady)
                    snprintf(statusMessage, sizeof(statusMessage), "Audio driver unavailable; sound remains silent.");
                break;
            case 4: autoBoot = !autoBoot; break;
            case 5:
                if (key == 40)
                {
                    if (MsxValidateProfile(MsxProfiles[selectedProfile])) saveSettings();
                    else snprintf(statusMessage, sizeof(statusMessage), "%s", MsxProfiles[selectedProfile].error);
                }
                break;
            case 6:
                if (key == 40)
                {
                    if (MsxValidateProfile(MsxProfiles[selectedProfile]))
                    {
                        bootRequested = true;
                        exitRequested = running;
                        leave = true;
                        statusMessage[0] = '\0';
                    }
                    else snprintf(statusMessage, sizeof(statusMessage), "%s", MsxProfiles[selectedProfile].error);
                }
                break;
            case 7:
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
    bootRequested = !interrupted && autoBoot && MsxProfiles[selectedProfile].available;
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
    char directory[96];
    snprintf(directory, sizeof(directory), "/sdcard/msx/bios/%s", profile.id);
    frame("BOOTING");
    text(8, 53, profile.name);
    video.show();
    running = true;
    MsxAudioEnable(audioReady && soundEnabled);
    const bool success = MsxCoreRun(directory, profile.model, selectedRamPages);
    running = false;
    MsxAudioEnable(false);
    if (!success && !statusMessage[0])
        snprintf(statusMessage, sizeof(statusMessage), "Emulator initialization failed. See UART log.");
}
