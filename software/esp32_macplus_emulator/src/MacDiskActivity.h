#pragma once
#include <stdint.h>

namespace MacDiskActivity
{
constexpr unsigned Width = 16, Height = 12;
constexpr unsigned RightMargin = 24, BottomMargin = 12;
constexpr unsigned X = 640 - Width - RightMargin, Y = 480 - Height - BottomMargin;
constexpr uint32_t HoldMs = 200;

class State
{
public:
    void record(uint32_t now) { lastAccess = now; valid = true; }
    bool active(uint32_t now) const { return valid && now - lastAccess < HoldMs; }
    void clear() { valid = false; }

private:
    uint32_t lastAccess = 0;
    bool valid = false;
};

inline uint8_t pixel(unsigned x, unsigned y, bool active, bool bright)
{
    if (!active || x >= 12 || y >= Height) return 0;
    if (y == 0 && x > 9) return 0;
    if (!x || x == 11 || !y || y == 11 || (y <= 3 && x >= 3 && x <= 8)) return 255;
    static const uint8_t digit[5] = {6,4,7,5,7};
    if (y >= 5 && y <= 9 && x >= 5 && x <= 7 && (digit[y - 5] & (4u >> (x - 5)))) return 255;
    return bright ? 0x38 : 0x18;
}
}
