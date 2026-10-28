#pragma once
#include <stdint.h>

enum MsxMapper : uint8_t
{
    MsxGeneric8, MsxGeneric16, MsxKonamiScc, MsxKonami, MsxAscii8, MsxAscii16,
    MsxGameMaster2, MsxFmPac, MsxMapperAuto, MsxAscii16X, MsxMapperCount
};

struct MsxCartridgeInfo
{
    int detectedMapper;
    const char *source;
};

inline bool MsxValidMapper(unsigned mapper) { return mapper < MsxMapperCount; }

inline const char *MsxMapperName(int mapper)
{
    switch (mapper)
    {
    case -1: return "Plain / mirrored";
    case MsxGeneric8: return "Generic 8 KiB";
    case MsxGeneric16: return "Generic 16 KiB";
    case MsxKonamiScc: return "Konami SCC";
    case MsxKonami: return "Konami";
    case MsxAscii8: return "ASCII8";
    case MsxAscii16: return "ASCII16";
    case MsxGameMaster2: return "GameMaster2";
    case MsxFmPac: return "FM-PAC";
    case MsxMapperAuto: return "Auto";
    case MsxAscii16X: return "ASCII16-X";
    case -8: return "NEO8 (unsupported)";
    case -9: return "NEO16 (unsupported)";
    case -14: return "Manbow2 (unsupported)";
    default: return "Unknown";
    }
}
