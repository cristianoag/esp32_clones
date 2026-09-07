#include "MsxAudio.h"
#include "MsxBoard.h"
#include "MsxPlatform.h"
#include <Arduino.h>
#include <soc/ledc_struct.h>

static constexpr unsigned BufferSize = 4096;
static uint8_t samples[BufferSize];
static unsigned readIndex = 0, writeIndex = 0;
static bool enabled = false;
static hw_timer_t *sampleTimer = nullptr;
static portMUX_TYPE audioLock = portMUX_INITIALIZER_UNLOCKED;

static void IRAM_ATTR sampleInterrupt()
{
    uint8_t sample = 128;
    portENTER_CRITICAL_ISR(&audioLock);
    if (enabled && readIndex != writeIndex)
    {
        sample = samples[readIndex];
        readIndex = (readIndex + 1) % BufferSize;
    }
    portEXIT_CRITICAL_ISR(&audioLock);
    // Register writes keep the 22 kHz ISR independent of flash-resident LEDC APIs.
    LEDC.channel_group[0].channel[0].duty.duty = static_cast<uint32_t>(sample) << 4;
    LEDC.channel_group[0].channel[0].conf1.duty_start = 1;
    LEDC.channel_group[0].channel[0].conf0.low_speed_update = 1;
}

bool MsxAudioStart()
{
    if (!ledcSetup(0, 156250, 8))
    {
        Serial.println("Audio PWM initialization failed.");
        return false;
    }
    ledcAttachPin(MsxBoard::Audio, 0);
    ledcWrite(0, 128);
    sampleTimer = timerBegin(0, 2, true);
    if (!sampleTimer)
    {
        Serial.println("Audio sample timer allocation failed.");
        return false;
    }
    timerAttachInterrupt(sampleTimer, sampleInterrupt, true);
    timerAlarmWrite(sampleTimer, (40000000 + MsxBoard::AudioRate / 2) / MsxBoard::AudioRate, true);
    timerAlarmEnable(sampleTimer);
    return true;
}

void MsxAudioEnable(bool value)
{
    portENTER_CRITICAL(&audioLock);
    enabled = value;
    readIndex = writeIndex = 0;
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
            samples[writeIndex] = static_cast<uint8_t>((static_cast<int>(source[position++]) + 32768) >> 8);
            writeIndex = (writeIndex + 1) % BufferSize;
            ++copied;
        }
        portEXIT_CRITICAL(&audioLock);
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
