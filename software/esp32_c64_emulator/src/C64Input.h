#pragma once
#include <stdint.h>
#include <string.h>

inline void C64Press(uint8_t matrix[8], unsigned position)
{
    matrix[position / 8] &= static_cast<uint8_t>(~(1u << (position % 8)));
}

inline int C64Symbol(char c)
{
    switch (c)
    {
    case '1': return 56; case '2': return 59; case '3': return 8;
    case '4': return 11; case '5': return 16; case '6': return 19;
    case '7': return 24; case '8': return 27; case '9': return 32;
    case '0': return 35; case '+': return 40; case '-': return 43;
    case '.': return 44; case ':': return 45; case '@': return 46;
    case ',': return 47; case '*': return 49; case ';': return 50;
    case '=': return 53; case '^': return 54; case '/': return 55;
    case '!': return 56 | 128; case '"': return 59 | 128;
    case '#': return 8 | 128; case '$': return 11 | 128;
    case '%': return 16 | 128; case '&': return 19 | 128;
    case '\'': return 24 | 128; case '(': return 27 | 128;
    case ')': return 32 | 128; case '[': return 45 | 128;
    case ']': return 50 | 128; case '<': return 47 | 128;
    case '>': return 44 | 128; case '?': return 55 | 128;
    case '_': return 57;
    default: return -1;
    }
}

inline void C64MapReport(const uint8_t report[8], uint8_t matrix[8], bool shiftLock = false)
{
    memset(matrix, 255, 8);
    for (unsigned i = 2; i < 8; ++i)
        if (report[i] >= 1 && report[i] <= 3) return;
    static const uint8_t letters[26] = {
        10,28,20,18,14,21,26,29,33,34,37,42,36,
        39,38,41,62,17,13,22,30,31,9,23,25,12
    };
    const bool shifted = (report[0] & 0x22) || shiftLock;
    bool physicalShift = false, symbolShift = false;
    for (unsigned i = 2; i < 8; ++i)
    {
        const uint8_t key = report[i];
        int position = -1;
        if (key >= 4 && key <= 29)
        {
            position = letters[key - 4];
            physicalShift |= shifted;
        }
        else if (key >= 30 && key <= 39)
            position = C64Symbol((shifted ? "!@#$%^&*()" : "1234567890")[key - 30]);
        else if (key == 49 || key == 50)
        {
            position = 48;
            physicalShift |= shifted;
        }
        else if (key == 53) position = shifted ? -1 : 57;
        else if (key >= 45 && key <= 56)
            position = C64Symbol((shifted ? "_+    :\" <>?" : "-=[]  ;' ,./")[key - 45]);
        else if (key >= 58 && key <= 65)
        {
            static const uint8_t functions[] = {4,4,5,5,6,6,3,3};
            position = functions[key - 58] | (((key - 58) & 1) ? 128 : 0);
        }
        else
        {
            switch (key)
            {
            case 40: case 88: position = 1; break;
            case 41: position = 63; break;
            case 42: case 76: position = 0; break;
            case 44: position = 60; break;
            case 49: case 50: position = 48; break;
            case 74: position = 51; break;
            case 79: position = 2; break;
            case 80: position = 2 | 128; break;
            case 81: position = 7; break;
            case 82: position = 7 | 128; break;
            default: break;
            }
            if (position >= 0) physicalShift |= shifted;
        }
        if (position >= 0)
        {
            C64Press(matrix, position & 63);
            symbolShift |= position & 128;
        }
    }
    // Shift alone is meaningful to C64 software; punctuation may remap it.
    bool anyKey = false;
    for (unsigned i = 2; i < 8; ++i) anyKey |= report[i] != 0;
    if (symbolShift || physicalShift || (shifted && !anyKey)) C64Press(matrix, 15);
    if (report[0] & 0x11) C64Press(matrix, 58);
    if (report[0] & 0xcc) C64Press(matrix, 61);
}

inline uint8_t C64ReadMatrix(const uint8_t matrix[8], uint8_t selected, bool reverse)
{
    uint8_t result = 255;
    for (unsigned row = 0; row < 8; ++row)
        for (unsigned column = 0; column < 8; ++column)
            if (!(matrix[row] & (1u << column)) &&
                !(selected & (1u << (reverse ? column : row))))
                result &= static_cast<uint8_t>(~(1u << (reverse ? row : column)));
    return result;
}

inline uint8_t C64JoystickValue(uint8_t buttons)
{
    return static_cast<uint8_t>(~(buttons & 31));
}
