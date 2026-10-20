#pragma once
#include <stdint.h>

inline uint16_t SmsFilterMenuPause(uint16_t buttons, bool &waitForRelease)
{
    constexpr uint16_t pause = 1 << 13;
    if (!(buttons & pause)) waitForRelease = false;
    return waitForRelease ? buttons & ~pause : buttons;
}

inline uint16_t SmsInput(const uint8_t report[8], uint16_t pads, const uint8_t enabled[2])
{
    uint16_t buttons = (enabled[0] ? pads & 63 : 0) |
                       (enabled[1] ? ((pads >> 8) & 63) << 6 : 0);
    for (unsigned i = 2; i < 8; ++i)
        if (report[i] >= 1 && report[i] <= 3) return buttons;
    for (unsigned i = 2; i < 8; ++i)
    {
        switch (report[i])
        {
        case 82: buttons |= 1 << 0; break;
        case 81: buttons |= 1 << 1; break;
        case 80: buttons |= 1 << 2; break;
        case 79: buttons |= 1 << 3; break;
        case 29: case 44: buttons |= 1 << 4; break; // Z / Space: button 1.
        case 27: buttons |= 1 << 5; break;          // X: button 2.
        case 26: buttons |= 1 << 6; break;          // W/S/A/D: player 2.
        case 22: buttons |= 1 << 7; break;
        case 4:  buttons |= 1 << 8; break;
        case 7:  buttons |= 1 << 9; break;
        case 10: buttons |= 1 << 10; break;         // G/H: player 2 buttons.
        case 11: buttons |= 1 << 11; break;
        case 21: buttons |= 1 << 12; break;         // R: console Reset.
        case 40: case 72: buttons |= 1 << 13; break; // Enter / Pause: console Pause.
        default: break;
        }
    }
    return buttons;
}
