#pragma once
#include <stdint.h>
#include <string.h>

enum class TkJoystick : uint8_t { None, Kempston, Sinclair1, Sinclair2, Cursor };

inline void TkPress(uint8_t rows[8], unsigned row, unsigned bit)
{
    rows[row] &= static_cast<uint8_t>(~(1u << bit));
}

inline void TkKeyboard(const uint8_t report[8], uint8_t rows[8])
{
    memset(rows, 0xff, 8);
    static const uint8_t letters[26][2] = {
        {1,0},{7,4},{0,3},{1,2},{2,2},{1,3},{1,4},{6,4},{5,2},
        {6,3},{6,2},{6,1},{7,2},{7,3},{5,1},{5,0},{2,0},{2,3},
        {1,1},{2,4},{5,3},{0,4},{2,1},{0,2},{5,4},{0,1}
    };
    if (report[0] & 0x22) TkPress(rows, 0, 0);
    if (report[0] & 0x55) TkPress(rows, 7, 1);
    for (unsigned i = 2; i < 8; ++i)
    {
        const unsigned key = report[i];
        if (key >= 4 && key <= 29) TkPress(rows, letters[key - 4][0], letters[key - 4][1]);
        else if (key >= 30 && key <= 34) TkPress(rows, 3, key - 30);
        else if (key >= 35 && key <= 39) TkPress(rows, 4, 39 - key);
        else switch (key)
        {
        case 40: case 88: TkPress(rows, 6, 0); break;
        case 44: TkPress(rows, 7, 0); break;
        case 41: TkPress(rows, 0, 0); TkPress(rows, 7, 0); break;
        case 42: case 76: TkPress(rows, 0, 0); TkPress(rows, 4, 0); break;
        case 57: TkPress(rows, 0, 0); TkPress(rows, 3, 1); break;
        case 79: TkPress(rows, 0, 0); TkPress(rows, 4, 2); break;
        case 80: TkPress(rows, 0, 0); TkPress(rows, 3, 4); break;
        case 81: TkPress(rows, 0, 0); TkPress(rows, 4, 4); break;
        case 82: TkPress(rows, 0, 0); TkPress(rows, 4, 3); break;
        case 45: TkPress(rows, 7, 1); TkPress(rows, 6, 3); break;
        case 46: TkPress(rows, 7, 1); TkPress(rows, 6, 1); break;
        case 51: TkPress(rows, 7, 1); TkPress(rows, 5, 1); break;
        case 52: TkPress(rows, 7, 1); TkPress(rows, 5, 0); break;
        case 54: TkPress(rows, 7, 1); TkPress(rows, 7, 3); break;
        case 55: TkPress(rows, 7, 1); TkPress(rows, 7, 2); break;
        case 56: TkPress(rows, 7, 1); TkPress(rows, 0, 4); break;
        default: break;
        }
    }
}

inline uint8_t TkApplyJoystick(uint8_t rows[8], uint8_t buttons, TkJoystick mode)
{
    if (mode == TkJoystick::Kempston)
        return ((buttons & 8) >> 3) | ((buttons & 4) >> 1) |
               ((buttons & 2) << 1) | ((buttons & 1) << 3) | (buttons & 16);
    if (mode == TkJoystick::Sinclair1 || mode == TkJoystick::Sinclair2)
    {
        const unsigned row = mode == TkJoystick::Sinclair1 ? 4 : 3;
        const uint8_t bits1[] = {1,2,4,3,0};
        const uint8_t bits2[] = {3,2,0,1,4};
        for (unsigned bit = 0; bit < 5; ++bit)
            if (buttons & (1u << bit)) TkPress(rows, row, row == 4 ? bits1[bit] : bits2[bit]);
    }
    else if (mode == TkJoystick::Cursor)
    {
        if (buttons & 1) TkPress(rows, 4, 3);
        if (buttons & 2) TkPress(rows, 4, 4);
        if (buttons & 4) TkPress(rows, 3, 4);
        if (buttons & 8) TkPress(rows, 4, 2);
        if (buttons & 16) TkPress(rows, 4, 0);
    }
    return 0;
}
