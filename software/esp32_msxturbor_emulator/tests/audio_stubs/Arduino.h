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
struct AudioTestSerial {
    void println(const char* text) { puts(text); }
    template<typename... Args> int printf(const char* format, Args... args) { return ::printf(format, args...); }
};
extern AudioTestSerial Serial;
struct hw_timer_t {
    bool running = false, alarm = false;
    void (*handler)() = nullptr;
    uint64_t interval = 0, next = 0;
};
double ledcSetup(unsigned, unsigned, unsigned);
void ledcAttachPin(unsigned, unsigned);
void ledcWrite(unsigned, unsigned);
hw_timer_t* timerBegin(unsigned, unsigned, bool);
void timerStop(hw_timer_t*);
void timerStart(hw_timer_t*);
void timerAttachInterrupt(hw_timer_t*, void (*)(), bool);
void timerAlarmWrite(hw_timer_t*, uint64_t, bool);
void timerAlarmEnable(hw_timer_t*);
void timerAlarmDisable(hw_timer_t*);
void timerWrite(hw_timer_t*, uint64_t);
void delay(unsigned);
void delayMicroseconds(unsigned);
uint32_t millis();
uint32_t micros();
