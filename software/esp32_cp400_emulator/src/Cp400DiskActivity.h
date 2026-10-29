#pragma once
#include <atomic>
#include <stdint.h>

namespace Cp400DiskActivity
{
constexpr unsigned DriveCount = 4, Width = DriveCount * 16, Height = 12;
constexpr unsigned RightMargin = 8, BottomMargin = 6;
constexpr uint32_t HoldMs = 200;

class State
{
public:
    void record(unsigned drive, uint32_t now)
    {
        lastAccess_[drive].store(now, std::memory_order_relaxed);
        valid_[drive].store(true, std::memory_order_release);
    }

    bool active(unsigned drive, uint32_t now) const
    {
        return valid_[drive].load(std::memory_order_acquire) &&
               now - lastAccess_[drive].load(std::memory_order_relaxed) < HoldMs;
    }

    void clear(unsigned drive) { valid_[drive].store(false, std::memory_order_release); }
    void clear()
    {
        for (unsigned drive = 0; drive < DriveCount; ++drive) clear(drive);
    }

private:
    // The CPU timer interrupt records transfers; the video task reads the timestamps.
    std::atomic<uint32_t> lastAccess_[DriveCount] = {};
    std::atomic<bool> valid_[DriveCount] = {};
};

inline uint8_t pixel(unsigned x, unsigned y, uint8_t drives, bool bright)
{
    const unsigned drive = x / 16;
    x %= 16;
    if (drive >= DriveCount || y >= Height || x >= 12 || !(drives & (1u << drive))) return 0;
    if (y == 0 && x > 9) return 0;
    if (!x || x == 11 || !y || y == 11 || (y <= 3 && x >= 3 && x <= 8)) return 255;
    static const uint8_t digits[DriveCount][5] =
        {{7,5,5,5,7}, {2,6,2,2,7}, {6,1,2,4,7}, {6,1,2,1,6}};
    if (y >= 5 && y <= 9 && x >= 5 && x <= 7 && (digits[drive][y - 5] & (4u >> (x - 5)))) return 255;
    return bright ? 0x38 : 0x18;
}
}
