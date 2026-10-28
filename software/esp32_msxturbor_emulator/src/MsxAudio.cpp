#include "MsxAudio.h"
#include "MsxBoard.h"
#include "MsxPlatform.h"
#include "MsxAudioSignal.h"
#include <Arduino.h>
#include <driver/timer.h>
#include <soc/ledc_struct.h>
#include <esp_timer.h>
#include <cmath>
#include <cstring>

static constexpr unsigned BufferSize = 4096;
static constexpr unsigned Prefill = 1024;
static constexpr unsigned TimerFrequency = 40000000;
static uint8_t samples[BufferSize];
static unsigned readIndex = 0, writeIndex = 0;
static unsigned nonNeutralQueued = 0;
static bool enabled = false;
static bool clockRunning = false;
static uint8_t outputDuty = 0;
static constexpr unsigned outputRate = MsxBoard::AudioRate;
static unsigned outputGain = 8;
static uint32_t underruns = 0;
static bool starved = false;
static bool outputStalled = false;
static MsxAudioSignal audioSignal;
static hw_timer_t *sampleTimer = nullptr;
static portMUX_TYPE audioLock = portMUX_INITIALIZER_UNLOCKED;
static bool diagnosticsEnabled = false;
static uint32_t previousIsrTime;
static bool haveIsrTime;
static struct DiagnosticWindow
{
    uint32_t samples = 0, clipped = 0, played = 0, late = 0, maxGap = 0, pwmClipped = 0;
    int minimum = 0, maximum = 0;
    int64_t sum = 0;
    uint64_t squares = 0;
} diagnostic;

static void resetDiagnostics()
{
    diagnostic = DiagnosticWindow();
    haveIsrTime = false;
}

static void IRAM_ATTR writeDuty(uint8_t duty)
{
    if (duty == outputDuty) return;
    outputDuty = duty;
    LEDC.channel_group[0].channel[0].duty.duty = static_cast<uint32_t>(duty) << 4;
    LEDC.channel_group[0].channel[0].conf1.duty_start = 1;
    LEDC.channel_group[0].channel[0].conf0.idle_lv = 0;
    LEDC.channel_group[0].channel[0].conf0.sig_out_en = duty != 0;
    LEDC.channel_group[0].channel[0].conf0.low_speed_update = 1;
}

static void IRAM_ATTR sampleInterrupt()
{
    portENTER_CRITICAL_ISR(&audioLock);
    if (!enabled || !clockRunning)
    {
        portEXIT_CRITICAL_ISR(&audioLock);
        return;
    }
    const bool available = readIndex != writeIndex;
    if (diagnosticsEnabled)
    {
        const uint32_t now = uint32_t(esp_timer_get_time());
        if (haveIsrTime)
        {
            const uint32_t gap = now - previousIsrTime;
            if (gap > diagnostic.maxGap) diagnostic.maxGap = gap;
            if (gap > 2 * ((1000000 + outputRate / 2) / outputRate)) ++diagnostic.late;
        }
        previousIsrTime = now;
        haveIsrTime = true;
        if (available) ++diagnostic.played;
    }
    const uint8_t sample = available ? samples[readIndex] : MsxAudioSignal::Neutral;
    if (available)
    {
        starved = false;
        readIndex = (readIndex + 1) % BufferSize;
        if (sample != MsxAudioSignal::Neutral) --nonNeutralQueued;
    }
    else if (!starved && !audioSignal.sleeping())
    {
        ++underruns;
        starved = true;
    }
    writeDuty(audioSignal.tick(available, sample));
    if (audioSignal.sleeping() && readIndex == writeIndex)
    {
        // Audio owns Arduino timer 0, which is TG0/T0 on this pinned framework.
        // Use the ISR-safe driver entry point, not the flash-resident Arduino API.
        timer_group_set_counter_enable_in_isr(TIMER_GROUP_0, TIMER_0, TIMER_PAUSE);
        clockRunning = false;
    }
    portEXIT_CRITICAL_ISR(&audioLock);
}

bool MsxAudioStart()
{
    if (!ledcSetup(0, 156250, 8))
    {
        Serial.println("Audio PWM initialization failed.");
        return false;
    }
    ledcAttachPin(MsxBoard::Audio, 0);
    ledcWrite(0, 0);
    sampleTimer = timerBegin(0, 2, true);
    if (!sampleTimer)
    {
        Serial.println("Audio sample timer allocation failed.");
        return false;
    }
    timerStop(sampleTimer);
    timerAttachInterrupt(sampleTimer, sampleInterrupt, true);
    timerAlarmWrite(sampleTimer, (TimerFrequency + MsxBoard::AudioRate / 2) / MsxBoard::AudioRate, true);
    // Leave the timer and output quiet until actual non-silent PCM arrives.
    return true;
}

void MsxAudioEnable(bool value)
{
    portENTER_CRITICAL(&audioLock);
    enabled = false;
    clockRunning = false;
    readIndex = writeIndex = 0;
    nonNeutralQueued = 0;
    underruns = 0;
    starved = false;
    outputStalled = false;
    resetDiagnostics();
    portEXIT_CRITICAL(&audioLock);
    if (sampleTimer)
    {
        timerAlarmDisable(sampleTimer);
        timerStop(sampleTimer);
        timerAlarmWrite(sampleTimer, (TimerFrequency + MsxBoard::AudioRate / 2) / MsxBoard::AudioRate, true);
    }
    // A short ramp avoids a hard midscale-to-ground step through the audio
    // coupling capacitor when F12 pauses the machine.
    while (outputDuty)
    {
        const uint8_t next = outputDuty > 4 ? outputDuty - 4 : 0;
        writeDuty(next);
        delayMicroseconds(100);
    }
    portENTER_CRITICAL(&audioLock);
    audioSignal.reset();
    enabled = value && sampleTimer;
    portEXIT_CRITICAL(&audioLock);
}

void MsxAudioSubmit(const int16_t *source, unsigned count)
{
    if (!count) return;
    portENTER_CRITICAL(&audioLock);
    const bool accepting = enabled;
    portEXIT_CRITICAL(&audioLock);
    if (!accepting) return;
    if (diagnosticsEnabled)
    {
        int minimum = source[0], maximum = source[0];
        int64_t sum = 0;
        uint64_t squares = 0;
        uint32_t clipped = 0;
        for (unsigned i = 0; i < count; ++i)
        {
            const int sample = source[i];
            if (sample < minimum) minimum = sample;
            if (sample > maximum) maximum = sample;
            sum += sample;
            squares += uint64_t(sample * sample);
            if (sample <= -32767 || sample >= 32767) ++clipped;
        }
        portENTER_CRITICAL(&audioLock);
        if (!diagnostic.samples || minimum < diagnostic.minimum) diagnostic.minimum = minimum;
        if (!diagnostic.samples || maximum > diagnostic.maximum) diagnostic.maximum = maximum;
        diagnostic.samples += count;
        diagnostic.clipped += clipped;
        diagnostic.sum += sum;
        diagnostic.squares += squares;
        portEXIT_CRITICAL(&audioLock);
    }
    unsigned position = 0;
    const uint32_t start = millis();
    while (position < count)
    {
        alignas(4) int16_t staging[32];
        const unsigned batch = count - position < 32 ? count - position : 32;
        std::memcpy(staging, source + position, batch * sizeof(staging[0]));
        portENTER_CRITICAL(&audioLock);
        if (!enabled)
        {
            portEXIT_CRITICAL(&audioLock);
            return;
        }
        unsigned copied = 0;
        while (position < count && (writeIndex + 1) % BufferSize != readIndex && copied < batch)
        {
            int scaled = int(staging[copied]) * int(outputGain);
            ++position;
            if (scaled < -32768 || scaled > 32767)
            {
                if (diagnosticsEnabled) ++diagnostic.pwmClipped;
                scaled = scaled < -32768 ? -32768 : 32767;
            }
            const unsigned rounded = unsigned(scaled + 32768 + 128) >> 8;
            const uint8_t sample = uint8_t(rounded > 255 ? 255 : rounded);
            const bool quietQueue = readIndex == writeIndex || nonNeutralQueued == 0;
            if (!quietQueue || !audioSignal.sleeping() || audioSignal.needsWake(sample))
            {
                samples[writeIndex] = sample;
                writeIndex = (writeIndex + 1) % BufferSize;
                if (sample != MsxAudioSignal::Neutral) ++nonNeutralQueued;
            }
            ++copied;
        }
        const bool wake = !clockRunning &&
            (writeIndex + BufferSize - readIndex) % BufferSize >= Prefill;
        if (wake) { clockRunning = true; haveIsrTime = false; }
        portEXIT_CRITICAL(&audioLock);
        if (wake)
        {
            timerWrite(sampleTimer, 0);
            timerAlarmEnable(sampleTimer);
            timerStart(sampleTimer);
        }
        if (!copied)
        {
            if (millis() - start > 1000)
            {
                outputStalled = true;
                msxReportError("Audio output stalled.");
                return;
            }
            delay(1);
        }
    }
}

MsxAudioStats MsxAudioGetStats(bool resetWindow)
{
    portENTER_CRITICAL(&audioLock);
    const DiagnosticWindow window = diagnostic;
    MsxAudioStats result = {};
    result.rate = outputRate;
    result.queued = (writeIndex + BufferSize - readIndex) % BufferSize;
    result.underruns = underruns;
    result.gain = outputGain;
    if (resetWindow) diagnostic = DiagnosticWindow();
    portEXIT_CRITICAL(&audioLock);
    result.inputSamples = window.samples;
    result.clippedSamples = window.clipped;
    result.pwmClippedSamples = window.pwmClipped;
    result.playedSamples = window.played;
    result.lateTicks = window.late;
    result.maxGapUs = window.maxGap;
    result.inputMin = window.minimum;
    result.inputMax = window.maximum;
    if (window.samples)
    {
        result.inputMean = int(window.sum / window.samples);
        result.inputRms = unsigned(std::sqrt(double(window.squares) / window.samples));
    }
    return result;
}

void MsxAudioSetDiagnostics(bool enabled)
{
    portENTER_CRITICAL(&audioLock);
    diagnosticsEnabled = enabled;
    resetDiagnostics();
    portEXIT_CRITICAL(&audioLock);
}

bool MsxAudioSetGain(unsigned gain)
{
    if (gain != 1 && gain != 2 && gain != 4 && gain != 8) {
        msxReportError("Invalid PWM output gain.");
        return false;
    }
    portENTER_CRITICAL(&audioLock);
    outputGain = gain;
    portEXIT_CRITICAL(&audioLock);
    return true;
}

bool MsxAudioPlayTestTone()
{
    if (!sampleTimer) { msxReportError("Audio output is not initialized."); return false; }
    const bool previousDiagnostics = diagnosticsEnabled;
    const unsigned previousGain = outputGain;
    MsxAudioEnable(false);
    MsxAudioSetGain(1);
    MsxAudioSetDiagnostics(true);
    Serial.println("A1ST audio test: 440 Hz sine, bypassing all emulator sound sources.");
    MsxAudioEnable(true);
    int16_t block[441];
    unsigned position = 0;
    bool success = true;
    for (unsigned frame = 0; frame < 100; ++frame)
    {
        for (auto& sample : block)
            sample = int16_t(12000 * std::sin(6.283185307179586 * 440 * position++ / MsxBoard::AudioRate));
        MsxAudioSubmit(block, 441);
        if (outputStalled) { success = false; break; }
        delay(20);
    }
    const MsxAudioStats stats = MsxAudioGetStats();
    MsxAudioEnable(false);
    MsxAudioSetGain(previousGain);
    MsxAudioSetDiagnostics(previousDiagnostics);
    Serial.printf("A1ST audio test: result=%s in=%u rms=%u clip=%u rate=%u underruns=%u late=%u maxGap=%uus gain=%ux pwmclip=%u\n",
                  success ? "OK" : "FAILED", stats.inputSamples, stats.inputRms, stats.clippedSamples, stats.rate,
                  stats.underruns, stats.lateTicks, stats.maxGapUs, stats.gain, stats.pwmClippedSamples);
    return success;
}
