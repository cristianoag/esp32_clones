#pragma once
#include <stdarg.h>
#include <stdio.h>

struct TestSerial
{
    unsigned messages = 0;
    int printf(const char *format, ...)
    {
        ++messages;
        va_list args;
        va_start(args, format);
        const int result = vprintf(format, args);
        va_end(args);
        return result;
    }
};
extern TestSerial Serial;
extern unsigned BrowserYields;
inline void delay(unsigned) { ++BrowserYields; }
