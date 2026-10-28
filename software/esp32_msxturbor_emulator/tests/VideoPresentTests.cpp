#include "A1stVideo.h"
#include "A1stCore.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <vector>

static uint16_t pixels[A1ST_FRAME_WIDTH * A1ST_FRAME_HEIGHT];
static unsigned widths[240];
static struct { unsigned vgaFit = 90; } settings;
struct alignas(64) Row { uint8_t bytes[384]; };
static Row rows[240];
static struct {
    uint8_t* getLineAddr8(unsigned y) { return rows[y].bytes; }
} dma;
static struct { decltype(dma)* dmaBuffer = &dma; } video;
static uint32_t presented;
static uint64_t blitUs;
static int64_t now;
struct Flush { uintptr_t address; size_t bytes; };
static std::vector<Flush> flushes;
static bool flushFailure;
static unsigned errors;
static void reportError(const char* message) { assert(*message); ++errors; }
static int64_t esp_timer_get_time() { now += 100; return now; }
const uint16_t* a1stPixels() { return pixels; }
unsigned a1stLineWidth(unsigned y) { return widths[y]; }
static int Cache_WriteBack_Addr(uintptr_t address, size_t bytes)
{
    assert(address % 64 == 0 && bytes % 64 == 0);
    flushes.push_back({address, bytes});
    return flushFailure ? 1 : 0;
}

#include "VideoPresent.inc"

int main()
{
    for (unsigned y = 0; y < 240; ++y) {
        widths[y] = y % 2 ? 544 : 272;
        for (unsigned x = 0; x < 544; ++x) pixels[y * 544 + x] = uint16_t(x * 19937 + y * 31);
    }
    for (unsigned fit : {80U, 85U, 90U, 95U, 100U}) {
        settings.vgaFit = fit;
        std::memset(rows, 0x5a, sizeof(rows));
        flushes.clear();
        present();
        assert(flushes.size() == 30);
        for (unsigned group = 0; group < 30; ++group) {
            const uintptr_t first = reinterpret_cast<uintptr_t>(rows[group * 8].bytes);
            const uintptr_t last = reinterpret_cast<uintptr_t>(rows[group * 8 + 7].bytes) + 320;
            assert(flushes[group].address == first && flushes[group].bytes == last - first);
        }
        const A1stViewport viewport(fit);
        for (unsigned y = 0; y < 240; ++y) {
            for (unsigned x = 0; x < 320; ++x) {
                uint8_t expected = 0;
                if (viewport.containsRow(y) && x >= viewport.left && x < viewport.left + viewport.width) {
                    const unsigned sy = viewport.sourceRow(y);
                    const unsigned scale = widths[sy] / 272;
                    const unsigned sx = 8 * scale + (x - viewport.left) * (256 * scale - 1) / (viewport.width - 1);
                    expected = A1stVgaColor(pixels[sy * 544 + sx]);
                }
                assert(rows[y].bytes[x] == expected);
            }
            for (unsigned x = 320; x < 384; ++x) assert(rows[y].bytes[x] == 0x5a);
        }
    }
    assert(presented == 5 && blitUs == 500);
    flushFailure = true;
    flushes.clear();
    present();
    assert(errors == 1 && presented == 5 && flushes.size() == 1);
    std::puts("PASS: real presentation is pixel-exact, bounded and uses 30 aligned cache writebacks instead of 240.");
}
