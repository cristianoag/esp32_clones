#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#define IRAM_ATTR
using portMUX_TYPE = int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(lock) ((void)(lock))
#define portEXIT_CRITICAL(lock) ((void)(lock))
#define portENTER_CRITICAL_ISR(lock) ((void)(lock))
#define portEXIT_CRITICAL_ISR(lock) ((void)(lock))
struct AudioTestSerial { void println(const char *text) { puts(text); } };
extern AudioTestSerial Serial;
struct hw_timer_t { bool running = false, alarm = false; void (*handler)() = nullptr; };
extern hw_timer_t AudioTimer;
extern unsigned AudioPin, AudioCarrier, AudioResolution, AudioTicks;
extern uint32_t AudioTimeMs;
double ledcSetup(unsigned, unsigned frequency, unsigned resolution);
void ledcAttachPin(unsigned pin, unsigned);
void ledcWrite(unsigned, unsigned duty);
hw_timer_t *timerBegin(unsigned, unsigned, bool);
void timerStop(hw_timer_t *);
void timerStart(hw_timer_t *);
void timerAttachInterrupt(hw_timer_t *, void (*)(), bool);
void timerAlarmWrite(hw_timer_t *, uint64_t, bool);
void timerAlarmEnable(hw_timer_t *);
void timerAlarmDisable(hw_timer_t *);
void timerWrite(hw_timer_t *, uint64_t);
void delay(unsigned);
void delayMicroseconds(unsigned);
uint32_t millis();
