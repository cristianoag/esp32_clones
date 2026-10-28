#pragma once
#include "A1stCore.h"
#include "MsxTypes.h"

void a1stLog(const char* message);
const char* a1stStateDirectory(void);
void a1stDiskClose(void);
void a1stPcmCreate(void);
bool a1stSaveSram(char* error, size_t capacity);
void a1stCheckFrameBounds(void);
bool a1stReadRom(const char* path, size_t expected, UInt8* buffer,
                 size_t count, char* error, size_t capacity);
bool a1stReadRomBlocks(const char* path, size_t expected, UInt8* const* blocks,
                       size_t blockSize, size_t count, char* error, size_t capacity);
bool a1stReadRomStream(const char* path, size_t expected, size_t count,
                       void (*visit)(void*, size_t, const UInt8*, size_t), void* context,
                       char* error, size_t capacity);
bool a1stCartridgeFlush(char* error, size_t capacity);
void a1stCartridgeClose(void);
