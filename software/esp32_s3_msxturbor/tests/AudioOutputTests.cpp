#include "MsxAudio.h"
#include "Arduino.h"
#include "soc/ledc_struct.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

AudioTestSerial Serial;
AudioTestLedc LEDC;
static hw_timer_t timer;
static uint64_t now;
static unsigned flat, longestFlat, crossings, previousDuty;
static bool measuring;
static std::string error;
static const std::vector<uint8_t>* reference;
static const std::vector<int16_t>* originals;
static size_t consumed;
static bool startFails;
static double signalEnergy, quantizationEnergy;
static unsigned expectedGain;
double ledcSetup(unsigned channel, unsigned frequency, unsigned resolution)
{
    assert(channel == 0 && frequency == 156250 && resolution == 8);
    return frequency;
}
void ledcAttachPin(unsigned pin, unsigned channel) { assert(pin == 47 && channel == 0); }
void ledcWrite(unsigned, unsigned duty) { LEDC.channel_group[0].channel[0].duty.duty = duty << 4; }
hw_timer_t* timerBegin(unsigned number, unsigned divider, bool up)
{
    assert(number == 0 && divider == 2 && up);
    return &timer;
}
void timerStop(hw_timer_t* value) { value->running = false; }
void timerStart(hw_timer_t* value) { value->running = !startFails; value->next = now + value->interval; }
void timerAttachInterrupt(hw_timer_t* value, void (*handler)(), bool) { value->handler = handler; }
void timerAlarmWrite(hw_timer_t* value, uint64_t interval, bool repeat)
{
    assert(interval == 1814 && repeat);
    value->interval = interval;
}
void timerAlarmEnable(hw_timer_t* value) { value->alarm = true; }
void timerAlarmDisable(hw_timer_t* value) { value->alarm = false; }
void timerWrite(hw_timer_t* value, uint64_t count) { assert(count == 0); value->next = now + value->interval; }
void timer_group_set_counter_enable_in_isr(int group, int index, int enabled)
{
    assert(group == 0 && index == 0 && enabled == 0);
    timer.running = false;
}
static unsigned duty() { return LEDC.channel_group[0].channel[0].duty.duty >> 4; }
static void advance(unsigned microseconds)
{
    const uint64_t end = now + uint64_t(microseconds) * 40;
    while (timer.running && timer.alarm && timer.next <= end) {
        now = timer.next;
        timer.next += timer.interval;
        const bool available = reference && MsxAudioGetStats().queued != 0;
        timer.handler();
        if (available) {
            assert(consumed < reference->size());
            if (measuring) {
                assert(duty() == (*reference)[consumed]);
                const double source = (*originals)[consumed];
                const double decoded = (int(duty()) - 128) * 256.0 / expectedGain;
                signalEnergy += source * source;
                quantizationEnergy += (source - decoded) * (source - decoded);
            }
            ++consumed;
        }
        if (measuring) {
            const unsigned sample = duty();
            if (sample == previousDuty) ++flat;
            else flat = 0;
            longestFlat = std::max(longestFlat, flat);
            if (previousDuty < 128 && sample >= 128) ++crossings;
            previousDuty = sample;
        }
    }
    now = end;
}
void delay(unsigned milliseconds) { advance(milliseconds * 1000); }
void delayMicroseconds(unsigned microseconds) { advance(microseconds); }
uint32_t micros() { return uint32_t(now / 40); }
uint32_t millis() { return uint32_t(now / 40000); }
int64_t esp_timer_get_time() { return int64_t(now / 40); }
void msxReportError(const char* text) { error = text; }

static double tone(unsigned speed, bool jitter, bool transitions = false, unsigned amplitude = 14000)
{
    MsxAudioEnable(false);
    MsxAudioEnable(true);
    unsigned sample = 0;
    std::vector<int16_t> block;
    std::vector<uint8_t> expected;
    std::vector<int16_t> raw;
    reference = &expected;
    originals = &raw;
    expectedGain = MsxAudioGetStats().gain;
    signalEnergy = quantizationEnergy = 0;
    consumed = 0;
    const uint64_t start = now;
    uint64_t measureStart = 0;
    flat = longestFlat = crossings = 0;
    measuring = false;
    for (unsigned frame = 0; now - start < 5ULL * 40000000; ++frame) {
        const unsigned rates[] = {100, 110, 100, 105, 100};
        const unsigned stage = std::min(unsigned((now - start) / 40000000), 4U);
        unsigned interval = 16667 * 100 / (transitions ? rates[stage] : speed);
        if (jitter) interval = interval * (frame % 2 ? 120 : 80) / 100;
        advance(interval);
        block.resize(367 + (frame & 1));
        for (auto& value : block) {
            value = int16_t(amplitude * std::sin(6.283185307179586 * 440 * sample++ / 22050));
            const int scaled = std::max(-32768, std::min(32767, int(value) * int(expectedGain)));
            const uint8_t encoded = uint8_t(std::min(255, (scaled + 32768 + 128) >> 8));
            if (!expected.empty() || encoded != 128) {
                expected.push_back(encoded);
                raw.push_back(value);
            }
        }
        MsxAudioSubmit(block.data(), block.size());
        if (!measuring && now - start > 40000000) {
            measuring = true;
            measureStart = now;
            previousDuty = duty();
        }
    }
    measuring = false;
    const double frequency = crossings * 40000000.0 / (now - measureStart);
    const MsxAudioStats stats = MsxAudioGetStats();
    const double snr = 10 * std::log10(signalEnergy / quantizationEnergy);
    std::printf("PWM %u%% speed%s: %.2f Hz, longest flat run %u samples, underruns %u, gain %ux SNR %.1f dB\n",
                speed, transitions ? " with speed changes" : jitter ? " with frame jitter" : "",
                frequency, longestFlat, stats.underruns, stats.gain, snr);
    assert(stats.rate == 22050 && timer.interval == 1814 && stats.queued < 4096);
    if (speed >= 100) {
        assert(longestFlat < 20 && stats.underruns == 0);
        assert(std::abs(frequency - 440) < 440 * 0.025);
    }
    else {
        assert(stats.underruns > 0);
    }
    assert(error.empty());
    reference = nullptr;
    originals = nullptr;
    return snr;
}

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    assert(MsxAudioStart());
    assert(MsxAudioSetGain(1));
    assert(!timer.running && duty() == 0);
    tone(100, false);
    tone(65, false);
    tone(50, false);
    tone(100, true);
    tone(100, false, true);
    const double lowLevel = tone(100, false, false, 512);
    assert(MsxAudioSetGain(8));
    const double amplified = tone(100, false, false, 512);
    assert(amplified > 28 && amplified - lowLevel > 15);
    assert(MsxAudioSetGain(1));
    MsxAudioEnable(false);
    assert(!timer.running && !timer.alarm && duty() == 0);
    assert(MsxAudioGetStats().queued == 0 && MsxAudioGetStats().underruns == 0);
    MsxAudioEnable(true);
    std::vector<int16_t> silence(367);
    for (unsigned frame = 0; frame < 60; ++frame) {
        advance(16667);
        MsxAudioSubmit(silence.data(), silence.size());
    }
    assert(!timer.running && duty() == 0);
    now = ((1ULL << 32) - 200000) * 40;
    tone(100, false);
    std::vector<int16_t> longTone(10000);
    for (unsigned i = 0; i < longTone.size(); ++i) longTone[i] = i % 2 ? 12000 : -12000;
    MsxAudioSubmit(longTone.data(), longTone.size());
    assert(error.empty());
    MsxAudioEnable(false);
    MsxAudioSetDiagnostics(true);
    MsxAudioEnable(true);
    for (unsigned i = 0; i < longTone.size(); ++i) longTone[i] = i % 2 ? 32767 : -32768;
    MsxAudioSubmit(longTone.data(), longTone.size());
    now += 2000 * 40;
    timer.next = now + timer.interval;
    advance(1000);
    const MsxAudioStats diagnostics = MsxAudioGetStats(true);
    assert(diagnostics.inputSamples == 10000 && diagnostics.clippedSamples == 10000);
    assert(diagnostics.inputMin == -32768 && diagnostics.inputMax == 32767);
    assert(diagnostics.inputMean == 0 && diagnostics.inputRms == 32767);
    assert(diagnostics.playedSamples > 0 && diagnostics.lateTicks > 0 && diagnostics.maxGapUs >= 2000);
    assert(MsxAudioGetStats().inputSamples == 0);
    MsxAudioEnable(false);
    assert(MsxAudioSetGain(8));
    MsxAudioEnable(true);
    for (unsigned i = 0; i < longTone.size(); ++i) longTone[i] = i % 2 ? 6000 : -6000;
    MsxAudioSubmit(longTone.data(), longTone.size());
    assert(MsxAudioGetStats().clippedSamples == 0 && MsxAudioGetStats().pwmClippedSamples == 10000);
    assert(duty() == 0 || duty() == 255);
    MsxAudioEnable(false);
    assert(MsxAudioPlayTestTone());
    assert(MsxAudioGetStats().gain == 8);
    assert(!timer.running && !timer.alarm && duty() == 0);
    startFails = true;
    assert(!MsxAudioPlayTestTone() && error == "Audio output stalled.");
    assert(!timer.running && !timer.alarm && duty() == 0);
    startFails = false;
    assert(!MsxAudioSetGain(3) && error == "Invalid PWM output gain." && MsxAudioGetStats().gain == 8);
    MsxAudioSetGain(1);
    MsxAudioEnable(true);
    std::vector<int16_t> wakeTone(1100);
    for (unsigned i = 0; i < wakeTone.size(); ++i)
        wakeTone[i] = int16_t(12000 * std::sin(6.283185307179586 * 440 * i / 22050));
    MsxAudioSubmit(wakeTone.data(), wakeTone.size());
    for (unsigned frame = 0; frame < 60; ++frame) {
        advance(16667);
        MsxAudioSubmit(silence.data(), silence.size());
    }
    assert(!timer.running);
    MsxAudioGetStats(true);
    MsxAudioSubmit(wakeTone.data(), wakeTone.size());
    advance(5000);
    assert(timer.running && MsxAudioGetStats().maxGapUs < 200);
    MsxAudioEnable(false);
    MsxAudioSetDiagnostics(false);
    std::puts("PASS: fixed native 22050 Hz PWM, unchanged pitch under frame jitter, explicit starvation reporting, bounded queue and mute.");
    std::puts("PASS: diagnostic levels/clipping/late ISR timing, window reset and source-independent PWM tone.");
    std::puts("PASS: low-level PWM SNR improvement, saturating gain, clipping diagnostics and unamplified reference tone.");
    std::puts("PASS: intentional timer sleep is excluded from late-ISR gap measurements.");
}
