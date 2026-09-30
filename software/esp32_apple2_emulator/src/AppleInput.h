#pragma once
#include <stdint.h>

inline int AppleAscii(uint8_t key, uint8_t modifiers, bool caps)
{
    const bool shift = modifiers & 0x22, control = modifiers & 0x11;
    if (key >= 4 && key <= 29)
        return control ? key - 3 : (shift != caps ? 'A' : 'a') + key - 4;
    if (key >= 30 && key <= 39)
    {
        const char *digits = shift ? "!@#$%^&*()" : "1234567890";
        const char value = digits[key - 30];
        return control && value == '@' ? 0 : control && value == '^' ? 30 : value;
    }
    if (key >= 45 && key <= 56)
    {
        const char *symbols = shift ? "_+{}||:\"~<>?" : "-=[]\\\\;'`,./";
        const char value = symbols[key - 45];
        if (control && value >= '[' && value <= '_') return value & 31;
        return value;
    }
    switch (key)
    {
    case 40: case 88: return 13;
    case 41: return 27;
    case 42: case 80: return 8;
    case 43: return 9;
    case 44: return control ? 0 : 32;
    case 76: return 127;
    case 79: return 21;
    case 81: return 10;
    case 82: return 11;
    case 84: return '/';
    case 85: return '*';
    case 86: return '-';
    case 87: return '+';
    case 89: case 90: case 91: case 92: case 93: case 94: case 95: case 96: case 97:
        return '1' + key - 89;
    case 98: return '0';
    case 99: return '.';
    default: return -1;
    }
}
