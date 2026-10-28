#include "A1stSettings.h"
#include "MsxAudio.h"
#include <cassert>
#include <cstdio>
#include <cstring>

static int64_t now;
struct TestSerial
{
    bool started = false, rejectSize = false, failDriver = false;
    size_t capacity = 128, queued = 0, bytes = 0;
    uint64_t blockedUs = 0;
    size_t setTxBufferSize(size_t size) {
        assert(!started);
        if (rejectSize) return 0;
        capacity = size;
        return size;
    }
    void begin(unsigned baud) { assert(baud == 115200); started = true; }
    explicit operator bool() const { return started && !failDriver; }
    template<typename... Args> int printf(const char* format, Args... args) {
        const int length = std::snprintf(nullptr, 0, format, args...);
        assert(length >= 0);
        bytes += length;
        if (queued + length > capacity) {
            const uint64_t wait = (queued + length - capacity) * 10000000ULL / 115200;
            blockedUs += wait;
            now += wait;
            queued = capacity;
        }
        else queued += length;
        return length;
    }
    void println(const char* message) { printf("%s\n", message); }
} Serial;

static A1stSettings settings;
static int64_t wallAnchor, reportStart, lastYield;
static uint64_t tickAnchor, reportTicks, blitUs;
static unsigned presented;
static bool pacingReady;
static constexpr const char* BuildTag = "default-6-buffered";
static constexpr unsigned MALLOC_CAP_SPIRAM = 1;
static int64_t esp_timer_get_time() { return now; }
static void delay(unsigned milliseconds) { now += milliseconds * 1000; }
static unsigned heap_caps_get_free_size(unsigned) { return 4753367; }
MsxAudioStats MsxAudioGetStats(bool)
{
    MsxAudioStats stats = {};
    stats.rate = 22050; stats.queued = 1100; stats.inputSamples = 110250;
    stats.inputMin = -1000; stats.inputMax = 3000; stats.inputMean = 10;
    stats.inputRms = 400; stats.playedSamples = 110250; stats.maxGapUs = 46; stats.gain = 8;
    return stats;
}
void a1stGetWorkStats(A1stWorkStats* stats)
{
    *stats = {};
    stats->samples = 17; stats->coreUs = 170000; stats->videoUs = 40000;
    stats->audioUs = 17000; stats->cpuBytes = 336;
}
void a1stGetAudioDebug(A1stAudioDebug* stats)
{
    std::memset(stats, 0, sizeof(*stats));
    stats->sources = A1ST_SOUND_PSG; stats->psg[7] = 0x38;
    stats->psg[8] = 10; stats->psg[9] = 12; stats->pcmStatus = 0x83;
}

#include "SerialTiming.inc"

static void report()
{
    now = 5000000;
    wallAnchor = reportStart = 0;
    lastYield = now;
    tickAnchor = reportTicks = 0;
    blitUs = 750000;
    presented = 50;
    pacingReady = true;
    settings.audioOptions = A1ST_SOUND_PSG | A1stAudioDiagnostics | A1stAudioDefaultGain;
    A1stStats stats = {};
    stats.masterTicks = 21477270ULL * 5;
    pace(stats);
}

int main()
{
    Serial.begin(115200);
    report();
    std::fprintf(stderr, "Diagnostic burst: %zu bytes, unbuffered wait %llu us\n",
                 Serial.bytes, static_cast<unsigned long long>(Serial.blockedUs));
    const size_t reportBytes = Serial.bytes;
    assert(reportBytes > 128 && Serial.blockedUs > 25000);
    Serial = TestSerial();
    assert(startBufferedSerial());
    report();
    assert(Serial.capacity == 2048 && Serial.bytes == reportBytes && Serial.bytes < 2048);
    assert(Serial.blockedUs == 0 && now == 5000000);
    Serial = TestSerial();
    Serial.rejectSize = true;
    assert(!startBufferedSerial());
    Serial = TestSerial();
    Serial.failDriver = true;
    assert(!startBufferedSerial());
    std::puts("PASS: real diagnostic burst stalls an unbuffered UART but fits the 2048-byte TX queue without waiting.");
    std::puts("PASS: UART is configured before begin, and configuration/driver failures are reported.");
}
