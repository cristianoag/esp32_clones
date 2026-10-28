#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace
{
bool nvsReady = true, sdReady = true, running = true;
bool paused, pauseFailure, writeFailure, installFailure, restarted;
unsigned writes, installs, frames, menus, errors;
unsigned presentations;
bool lastRendered;
uint32_t videoFrame;
uint16_t controls, coreControls;
uint8_t menuKey;
char status[160];
constexpr size_t MsxSdPathCapacity = 240;
struct { uint8_t drawEvery = 1; } settings;
struct A1stStats { unsigned frames; };
struct {
    size_t putBytes(const char *, const void *, size_t size) {
        assert(paused);
        ++writes;
        return writeFailure ? 0 : size;
    }
} preferences;
struct { void restart() { assert(paused); restarted = true; } } ESP;
void reportError(const char *) { ++errors; }
bool MsxJoystickHostPause() { if (pauseFailure) return false; paused = true; return true; }
void MsxJoystickHostResume() { assert(paused); paused = false; }
bool chooseFile(const char *, const char *, bool, char *path) { strcpy(path, "/updates/other.FLH"); return true; }
bool confirmUpdate(const char *) { return true; }
bool flushState() { return true; }
void firmwareProgress(const char *, uint8_t) {}
bool MsxInstallFirmware(const char *path, void (*)(const char *, uint8_t), char *, size_t) {
    assert(paused && !strcmp(path, "/updates/other.FLH"));
    ++installs;
    return !installFailure;
}
void frame(const char *) {}
void text(int, int, const char *) {}
void showMenu() {}
void delay(unsigned) {}
void menu() { ++menus; }
uint8_t MsxKeyboardMenuKey() { return menuKey; }
void MsxKeyboardMatrix(uint8_t *matrix) { memset(matrix, 0xff, 16); }
void a1stSetKeyboard(const uint8_t *) {}
uint16_t MsxJoysticksRead() { return controls; }
void a1stSetJoysticks(uint16_t value) { coreControls = value; }
void a1stRunFrameWithVideo(bool render) { lastRendered = render; ++frames; }
void a1stGetStats(A1stStats *stats) { stats->frames = frames; }
void present() { ++presentations; }
void pace(const A1stStats &) {}
#include "JoystickFrontend.inc"
}

int main()
{
    controls = 0x2419;
    loop();
    assert(frames == 1 && coreControls == controls);
    controls = 0;
    loop();
    assert(frames == 2 && !coreControls);
    menuKey = 69;
    loop();
    assert(menus == 1 && frames == 2);
    menuKey = 0;
    settings.drawEvery = 2;
    videoFrame = 0;
    loop();
    assert(!lastRendered && presentations == 2);
    loop();
    assert(lastRendered && presentations == 3);
    assert(saveSettings() && writes == 1 && !paused);
    writeFailure = true;
    assert(!saveSettings() && writes == 2 && !paused);
    pauseFailure = true;
    assert(!saveSettings() && writes == 2 && !paused);
    firmwareUpdate();
    assert(installs == 0 && !paused && !restarted);
    pauseFailure = false;
    installFailure = true;
    firmwareUpdate();
    assert(installs == 1 && !paused && !restarted);
    installFailure = false;
    firmwareUpdate();
    assert(installs == 2 && paused && restarted);
    assert(errors >= 3);
    puts("PASS: live controls reach TurboR core, disconnect releases, F12 pauses input, flash writes quiesce USB.");
}
