#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    A1ST_FIRMWARE_BYTES = 4194304,
    A1ST_KANJI_BYTES = 262144,
    A1ST_RAM_BYTES = 262144,
    A1ST_FRAME_WIDTH = 544,
    A1ST_FRAME_HEIGHT = 240
};

typedef enum {
    A1ST_GENERIC8, A1ST_GENERIC16, A1ST_KONAMI_SCC, A1ST_KONAMI,
    A1ST_ASCII8, A1ST_ASCII16, A1ST_GAMEMASTER2, A1ST_FMPAC, A1ST_MAPPER_AUTO,
    A1ST_ASCII16X, A1ST_MAPPER_COUNT
} A1stMapper;

enum {
    A1ST_SOUND_PSG = 1, A1ST_SOUND_MSXMUSIC = 2, A1ST_SOUND_PCM = 4,
    A1ST_SOUND_SCC = 8, A1ST_SOUND_CLICK = 16, A1ST_SOUND_ALL = 31
};

typedef struct {
    uint8_t psg[16];
    uint8_t pcmStatus;
    uint8_t sources;
    uint32_t clickEdges;
} A1stAudioDebug;

typedef struct {
    void (*message)(const char* message);
    void (*audio)(const int16_t* samples, unsigned count);
} A1stHost;

typedef struct {
    uint64_t masterTicks;
    uint32_t frames;
    uint32_t cpuSwitches;
    uint16_t pc;
    bool r800;
    bool dram;
    unsigned refreshRate;
} A1stStats;

bool a1stCreate(const char* firmware, const char* kanji, const char* stateDirectory,
                const A1stHost* host, char* error, size_t errorSize);
void a1stDestroy(void);
void a1stRunFrame(void);
void a1stSetKeyboard(const uint8_t matrix[16]);
void a1stSetJoysticks(uint16_t activeHigh);
void a1stSetFirmwareEnabled(bool enabled);
bool a1stSetSoundSources(unsigned sources, char* error, size_t errorSize);
void a1stGetAudioDebug(A1stAudioDebug* stats);
const uint16_t* a1stPixels(void);
unsigned a1stLineWidth(unsigned line);
void a1stGetStats(A1stStats* stats);
bool a1stMountDisk(const char* path, char* error, size_t errorSize);
// Drives and slots use zero-based indices. Empty paths eject.
bool a1stAttachDisk(unsigned drive, const char* path, char* error, size_t errorSize);
bool a1stInspectCartridge(const char* path, int* detected, char* error, size_t errorSize);
// Attach ROMs after a1stCreate, while the CPU is paused.
bool a1stAttachCartridge(unsigned slot, const char* path, unsigned mapper,
                         char* error, size_t errorSize);
bool a1stFlushState(char* error, size_t errorSize);
uint8_t a1stReadMemory(uint16_t address);
void a1stWriteMemory(uint16_t address, uint8_t value);
uint8_t a1stReadPort(uint16_t port);
void a1stWritePort(uint16_t port, uint8_t value);

#ifdef __cplusplus
}
#endif
