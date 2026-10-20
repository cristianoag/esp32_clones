#pragma once
#include <cstdint>
#ifdef ARDUINO
#include <esp_timer.h>
#else
#include <chrono>
#endif

class PlatformManager
{
public:
    static PlatformManager &getInstance()
    {
        static PlatformManager instance;
        return instance;
    }
    int64_t getTimeUS() const
    {
#ifdef ARDUINO
        return esp_timer_get_time();
#else
        return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
#endif
    }
};
