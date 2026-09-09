#pragma once
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
using portMUX_TYPE = int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(lock) ((void)(lock))
#define portEXIT_CRITICAL(lock) ((void)(lock))
struct JoystickTestSerial
{
    template<typename... Args> int printf(const char *format, Args... args) { return ::printf(format, args...); }
    void println(const char *message) { puts(message); }
};
extern JoystickTestSerial Serial;
