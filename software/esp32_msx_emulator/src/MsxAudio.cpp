#include "MsxAudio.h"
#include "MsxBoard.h"
#include "MsxPlatform.h"
#include "MsxAudioSignal.h"
#include <Arduino.h>
#include <driver/timer.h>
#include <soc/ledc_struct.h>

static constexpr unsigned BufferSize = 4096;
static uint8_t samples[BufferSize];
static unsigned readIndex = 0, writeIndex = 0;
static unsigned nonNeutralQueued = 0;
static bool enabled = false;
static bool clockRunning = false;
static uint8_t outputDuty = 0;
static MsxAudioSignal audioSignal;
static hw_timer_t *sampleTimer = nullptr;
static portMUX_TYPE audioLock = portMUX_INITIALIZER_UNLOCKED;

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
    const uint8_t sample = available ? samples[readIndex] : 128;
    if (available)
    {
        readIndex = (readIndex + 1) % BufferSize;
        if (sample != 128) --nonNeutralQueued;
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
    timerAlarmWrite(sampleTimer, (40000000 + MsxBoard::AudioRate / 2) / MsxBoard::AudioRate, true);
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
    portEXIT_CRITICAL(&audioLock);
    if (sampleTimer)
    {
        timerAlarmDisable(sampleTimer);
        timerStop(sampleTimer);
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
    unsigned position = 0;
    const uint32_t start = millis();
    while (position < count)
    {
        portENTER_CRITICAL(&audioLock);
        if (!enabled)
        {
            portEXIT_CRITICAL(&audioLock);
            return;
        }
        unsigned copied = 0;
        while (position < count && (writeIndex + 1) % BufferSize != readIndex && copied < 32)
        {
            const uint8_t sample = static_cast<uint8_t>((static_cast<int>(source[position++]) + 32768) >> 8);
            const bool quietQueue = readIndex == writeIndex || nonNeutralQueued == 0;
            if (!quietQueue || !audioSignal.sleeping() || audioSignal.needsWake(sample))
            {
                samples[writeIndex] = sample;
                writeIndex = (writeIndex + 1) % BufferSize;
                if (sample != 128) ++nonNeutralQueued;
            }
            ++copied;
        }
        const bool wake = !clockRunning && readIndex != writeIndex;
        if (wake) clockRunning = true;
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
                msxReportError("Audio output stalled.");
                return;
            }
            delay(1);
        }
    }
}
