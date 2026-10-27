#pragma once
#include "EspOtaTest.h"

struct UpdateTestSerial {
    template<typename... Args> int printf(const char *format, Args... args) {
        return ::printf(format, args...);
    }
};
extern UpdateTestSerial Serial;
inline void delay(unsigned) {}
