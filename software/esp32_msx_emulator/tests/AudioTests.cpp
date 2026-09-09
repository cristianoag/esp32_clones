#include "MsxAudio.h"
#include "MsxAudioSignal.h"
#include "MsxJoystickDisplay.h"
#include "Arduino.h"
#include "soc/ledc_struct.h"
#include <assert.h>
#include <string>
#include <vector>

AudioTestSerial Serial;
AudioTestLedc LEDC;
hw_timer_t AudioTimer;
unsigned AudioPin, AudioCarrier, AudioResolution, AudioTicks;
uint32_t AudioTimeMs;
static std::string error;

double ledcSetup(unsigned channel, unsigned frequency, unsigned resolution)
{
    assert(channel == 0);
    AudioCarrier = frequency;
    AudioResolution = resolution;
    return frequency;
}
void ledcAttachPin(unsigned pin, unsigned channel) { assert(channel == 0 && pin == 47); AudioPin = pin; }
void ledcWrite(unsigned channel, unsigned duty) { assert(channel == 0); LEDC.channel_group[0].channel[0].duty.duty = duty << 4; }
hw_timer_t *timerBegin(unsigned number, unsigned divider, bool up)
{
    assert(number == 0 && divider == 2 && up);
    AudioTimer.running = true;
    return &AudioTimer;
}
void timerStop(hw_timer_t *t) { t->running = false; }
void timerStart(hw_timer_t *t) { t->running = true; }
void timerAttachInterrupt(hw_timer_t *t, void (*fn)(), bool) { t->handler = fn; }
void timerAlarmWrite(hw_timer_t *, uint64_t interval, bool repeat) { assert(interval == 1814 && repeat); }
void timerAlarmEnable(hw_timer_t *t) { t->alarm = true; }
void timerAlarmDisable(hw_timer_t *t) { t->alarm = false; }
void timerWrite(hw_timer_t *, uint64_t count) { assert(!count); }
void timer_group_set_counter_enable_in_isr(int group, int timer, int enabled)
{
    assert(group == 0 && timer == 0 && enabled == 0);
    AudioTimer.running = false;
}
static void ticks(unsigned count)
{
    for (unsigned i = 0; i < count && AudioTimer.running && AudioTimer.alarm; ++i)
    {
        ++AudioTicks;
        AudioTimer.handler();
    }
}
void delay(unsigned ms) { ticks(ms * 23); AudioTimeMs += ms; }
void delayMicroseconds(unsigned) { assert(!AudioTimer.running); }
uint32_t millis() { return AudioTimeMs; }
void msxReportError(const char *text) { error = text; }
static unsigned duty() { return LEDC.channel_group[0].channel[0].duty.duty >> 4; }

int main()
{
    MsxAudioSignal s;
    for (unsigned i = 0; i < 10000; ++i) assert(s.tick(true, 128) == 0 && s.sleeping());
    assert(s.needsWake(129)); // No amplitude threshold that would discard quiet tones.
    for (unsigned i = 0; i < 10000; ++i)
    {
        s.tick(true, i % 2 ? 127 : 129);
        assert(!s.sleeping());
    }
    for (unsigned i = 0; i < MsxAudioSignal::IdleSamples + MsxAudioSignal::RampSamples + 1; ++i)
        s.tick(true, 128);
    assert(s.sleeping() && s.tick(false, 0) == 0);
    assert(s.tick(true, 200) > 0 && !s.sleeping());
    for (unsigned i = 0; i < 4000; ++i) s.tick(true, 200);
    assert(s.sleeping() && !s.needsWake(200)); // A constant DC value is not ongoing sound.
    assert(!s.needsWake(128));
    s.reset();
    assert(s.sleeping());

    assert(MsxAudioStart());
    assert(AudioPin == 47 && AudioCarrier == 156250 && AudioResolution == 8);
    assert(!AudioTimer.running && !AudioTimer.alarm && duty() == 0);
    MsxAudioEnable(true);
    std::vector<int16_t> silence(10000);
    MsxAudioSubmit(silence.data(), silence.size());
    assert(!AudioTimer.running && duty() == 0 && AudioTicks == 0);
    std::vector<int16_t> tone(512);
    for (unsigned i = 0; i < tone.size(); ++i) tone[i] = i % 2 ? 12000 : -12000;
    MsxAudioSubmit(tone.data(), tone.size());
    assert(AudioTimer.running && AudioTimer.alarm);
    ticks(512);
    const unsigned held = duty();
    assert(held > 128);
    ticks(100);
    assert(duty() == held); // Underflow holds the last level, not a jump to midscale.
    ticks(5000);
    assert(!AudioTimer.running && duty() == 0 && !LEDC.channel_group[0].channel[0].conf0.sig_out_en);
    MsxAudioSubmit(silence.data(), silence.size());
    assert(!AudioTimer.running);
    MsxAudioSubmit(tone.data(), tone.size());
    ticks(512);
    for (unsigned i = 0; i < 120; ++i)
    {
        MsxAudioSubmit(silence.data(), 50);
        ticks(50);
    }
    assert(!AudioTimer.running && duty() == 0);
    MsxAudioSubmit(tone.data(), tone.size());
    ticks(150);
    assert(AudioTimer.running && duty() != 0);
    MsxAudioEnable(false);
    assert(!AudioTimer.running && !AudioTimer.alarm && duty() == 0);
    const unsigned stoppedTicks = AudioTicks;
    MsxAudioSubmit(tone.data(), tone.size());
    ticks(1000);
    assert(AudioTicks == stoppedTicks && duty() == 0);
    MsxAudioEnable(true);
    assert(!AudioTimer.running);
    std::vector<int16_t> longTone(10000);
    for (unsigned i = 0; i < longTone.size(); ++i) longTone[i] = i % 2 ? 30000 : -30000;
    MsxAudioSubmit(longTone.data(), longTone.size());
    assert(error.empty());
    ticks(16000);
    assert(!AudioTimer.running && duty() == 0);
    MsxAudioEnable(false);

    MsxJoystickSnapshot before = {}, after = {};
    after.sequence = 500;
    after.generation = 3;
    assert(!MsxJoystickDisplayChanged(before, after));
    after.connected = true;
    assert(MsxJoystickDisplayChanged(before, after));
    before = after;
    after.report[0] = 127;
    assert(MsxJoystickDisplayChanged(before, after));
    before = after;
    after.buttons = 1;
    assert(MsxJoystickDisplayChanged(before, after));
    puts("PASS: GPIO47-only output, real mute, silence/timer stop, tone wake/ramp, underflow, and changed-only joystick display.");
}
