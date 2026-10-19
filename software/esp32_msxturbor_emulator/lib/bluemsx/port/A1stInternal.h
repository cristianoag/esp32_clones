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
bool a1stCartridgeFlush(char* error, size_t capacity);
void a1stCartridgeClose(void);
