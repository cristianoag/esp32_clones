#include "A1stCore.h"
#include "MemoryHooks.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static uint64_t samples, active, clipped;
static double energy;
static unsigned peak;

static void require(bool value, const char* message)
{
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

static void message(const char* text) { std::fprintf(stderr, "A1ST: %s\n", text); }
static void measure(const int16_t* source, unsigned count)
{
    require(count > 300 && count < 500, "Each game frame supplies a bounded PAL/NTSC audio block.");
    for (unsigned i = 0; i < count; ++i) {
        const int value = source[i];
        ++samples;
        if (std::abs(value) > 256) ++active;
        if (std::abs(value) >= 32767) ++clipped;
        energy += double(value) * value;
        peak = std::max(peak, unsigned(std::abs(value)));
    }
}

void a1stGameAudioTests(const char* firmware, const char* kanji, const char* game)
{
    a1stTestSetAllocationLimit(8 * 1024 * 1024 - 512 * 1024);
    char error[512];
    const A1stHost host = {message, measure};
    require(a1stCreate(firmware, kanji, "original-state", &host, error, sizeof(error)), error);
    a1stSetFirmwareEnabled(false);
    require(a1stAttachCartridge(0, game, A1ST_MAPPER_AUTO, error, sizeof(error)), error);
    for (unsigned frame = 0; frame < 1800; ++frame) {
        uint8_t keyboard[16];
        std::memset(keyboard, 0xff, sizeof(keyboard));
        if (frame >= 600 && frame < 615) keyboard[8] &= ~1U;
        a1stSetKeyboard(keyboard);
        a1stSetJoysticks(frame >= 600 && frame < 615 ? 0x10 : 0);
        a1stRunFrame();
    }
    require(samples > 600000 && active > 10000, "User-supplied game boots and produces sustained audio.");
    require(clipped * 100 < samples, "Game audio clipping remains below one percent.");
    std::printf("Game audio: %llu samples, %llu active, %llu clipped, peak %u, RMS %.2f; no audio recorded.\n",
                static_cast<unsigned long long>(samples), static_cast<unsigned long long>(active),
                static_cast<unsigned long long>(clipped), peak, std::sqrt(energy / samples));
    a1stDestroy();
}
