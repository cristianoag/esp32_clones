#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

inline int MacScanCode(uint8_t usb)
{
    static const uint8_t letters[] = {
        0x00,0x0b,0x08,0x02,0x0e,0x03,0x05,0x04,0x22,0x26,0x28,0x25,0x2e,
        0x2d,0x1f,0x23,0x0c,0x0f,0x01,0x11,0x20,0x09,0x0d,0x07,0x10,0x06
    };
    static const uint8_t digits[] = {0x12,0x13,0x14,0x15,0x17,0x16,0x1a,0x1c,0x19,0x1d};
    if (usb >= 4 && usb <= 29) return letters[usb - 4];
    if (usb >= 30 && usb <= 39) return digits[usb - 30];
    switch (usb) {
    case 40: return 0x24;
    case 41: return 0x35;
    case 42: case 76: return 0x33;
    case 43: return 0x30;
    case 44: return 0x31;
    case 45: return 0x1b;
    case 46: return 0x18;
    case 47: return 0x21;
    case 48: return 0x1e;
    case 49: case 50: return 0x2a;
    case 51: return 0x29;
    case 52: return 0x27;
    case 53: return 0x32;
    case 54: return 0x2b;
    case 55: return 0x2f;
    case 56: return 0x2c;
    case 79: return 0x42;
    case 80: return 0x46;
    case 81: return 0x48;
    case 82: return 0x4d;
    case 88: return 0x4c;
    case 89: return 0x53;
    case 90: return 0x54;
    case 91: return 0x55;
    case 92: return 0x56;
    case 93: return 0x57;
    case 94: return 0x58;
    case 95: return 0x59;
    case 96: return 0x5b;
    case 97: return 0x5c;
    case 98: return 0x52;
    case 99: return 0x41;
    default: return -1;
    }
}

class MacKeyState
{
public:
    template<class Emit> void update(const uint8_t report[8], Emit emit)
    {
        for (unsigned i = 2; i < 8; ++i)
            if (report[i] >= 1 && report[i] <= 3) return;
        bool next[128] = {};
        next[0x38] = (report[0] & 0x22) != 0;
        next[0x36] = (report[0] & 0x11) != 0;
        next[0x3a] = (report[0] & 0x44) != 0;
        next[0x37] = (report[0] & 0x88) != 0;
        bool capsDown = false;
        for (unsigned i = 2; i < 8; ++i) {
            capsDown |= report[i] == 57;
            const int code = MacScanCode(report[i]);
            if (code >= 0) next[code] = true;
        }
        if (capsDown && !previousCaps) caps = !caps;
        previousCaps = capsDown;
        next[0x39] = caps;
        for (unsigned code = 0; code < 128; ++code)
            if (held[code] && !next[code]) emit(code, true);
        // Modifiers precede ordinary press events.
        const unsigned modifiers[] = {0x36,0x37,0x38,0x39,0x3a};
        for (unsigned code : modifiers)
            if (next[code] && !held[code]) emit(code, false);
        for (unsigned code = 0; code < 128; ++code)
            if ((code < 0x36 || code > 0x3a) && next[code] && !held[code]) emit(code, false);
        memcpy(held, next, sizeof(held));
    }
    void reset() { memset(held, 0, sizeof(held)); caps = previousCaps = false; }
    template<class Emit> void release(Emit emit, bool preserveCaps = false)
    {
        for (unsigned code = 0; code < 128; ++code)
            if (held[code] && !(preserveCaps && code == 0x39)) emit(code, true);
        memset(held, 0, sizeof(held));
        if (preserveCaps) held[0x39] = caps;
        else caps = previousCaps = false;
    }
private:
    bool held[128] = {};
    bool caps = false, previousCaps = false;
};

class MacMouseState
{
public:
    bool report(const uint8_t *data, size_t length)
    {
        if (!data || length < 3 || length > 8) return false;
        dx += static_cast<int8_t>(data[1]);
        dy += static_cast<int8_t>(data[2]);
        if (dx > 32767) dx = 32767;
        if (dx < -32767) dx = -32767;
        if (dy > 32767) dy = 32767;
        if (dy < -32767) dy = -32767;
        button = (data[0] & 1) != 0;
        return true;
    }
    void take(int &x, int &y, bool &down)
    {
        x = dx; y = dy; down = button; dx = dy = 0;
    }
    void reset() { dx = dy = 0; button = false; }
private:
    int dx = 0, dy = 0;
    bool button = false;
};
