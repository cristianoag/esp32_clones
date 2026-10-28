#include "A1stSettings.h"
#include "MsxAudio.h"
#include "MsxMenuLayout.h"
#include "Arduino.h"
#include <cassert>
#include <cstring>
#include <map>
#include <string>
#include <vector>

AudioTestSerial Serial;
static A1stSettings settings;
static bool running = true, debugEnabled, coreFailure;
static unsigned appliedSources, applies, plays, errors;
static unsigned gain;
static char status[160];
static std::vector<uint8_t> keys;
static size_t keyIndex;
static std::map<int, std::string> labels;

static void reportError(const char* message) { assert(*message); ++errors; }
void MsxAudioSetDiagnostics(bool enabled) { debugEnabled = enabled; }
bool MsxAudioSetGain(unsigned value) { gain = value; return true; }
bool a1stSetSoundSources(unsigned sources, char*, size_t)
{
    ++applies;
    if (coreFailure) return false;
    appliedSources = sources;
    return true;
}
bool MsxAudioPlayTestTone() { ++plays; return true; }
static void frame(const char*) {}
static void text(int, int y, const char* message) { labels[y] = message; }
static void messageLines(const char*) {}
static void showMenu() {}
static void MsxKeyboardClearEvents() {}
static uint8_t waitKey() { assert(keyIndex < keys.size()); return keys[keyIndex++]; }
static struct { void fillRect(int, int, int, int, unsigned) {} } display;

#include "SoundMenu.inc"

static void menu(std::initializer_list<uint8_t> input)
{
    keys = input;
    keyIndex = 0;
    soundMenu();
    assert(keyIndex == keys.size());
}

int main()
{
    assert(applySoundSettings() && appliedSources == A1ST_SOUND_ALL && !debugEnabled);
    assert(gain == 8);
    menu({81,81,81,81,81,81,81,81,40,41});
    assert(settings.sound && settings.audioOptions == (A1ST_SOUND_PSG | A1stAudioDiagnostics | A1stAudioDefaultGain));
    assert(appliedSources == A1ST_SOUND_PSG && debugEnabled);
    assert(labels[48 + 4 * 11].find("SCC cartridge") == 0);
    assert(labels[48 + 5 * 11].find("Keyboard click") == 0);
    menu({81,81,40,69});
    assert(appliedSources == (A1ST_SOUND_PSG | A1ST_SOUND_MSXMUSIC));
    menu({81,81,81,81,81,81,81,81,81,40,41});
    assert(appliedSources == A1ST_SOUND_ALL && debugEnabled);
    menu({81,81,81,40,41});
    assert(!(appliedSources & A1ST_SOUND_PCM) && (appliedSources & A1ST_SOUND_CLICK));
    menu({81,81,81,81,81,40,41});
    assert(!(appliedSources & (A1ST_SOUND_PCM | A1ST_SOUND_CLICK)));
    menu({81,81,81,81,81,81,40,41});
    assert(!debugEnabled);
    menu({40,41});
    assert(!settings.sound);
    const uint8_t before = settings.audioOptions;
    menu({81,81,81,81,81,81,81,81,81,81,40,41});
    assert(plays == 1 && !settings.sound && settings.audioOptions == before);
    menu({81,81,81,81,81,81,81,40,41});
    assert(gain == 1);
    menu({81,81,81,81,81,81,81,80,41});
    assert(gain == 8);
    running = false;
    const unsigned beforeApplies = applies;
    assert(applySoundSettings() && applies == beforeApplies);
    running = true;
    coreFailure = true;
    std::strcpy(status, "Sound apply failed.");
    assert(!applySoundSettings() && errors == 1);
    std::puts("PASS: real sound page, independent sources/click, PSG-only preset, diagnostics, master, bypass tone and paused/stopped application.");
}
