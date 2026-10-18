#pragma once
#include "A1stCore.h"
#include "MsxTypes.h"

void a1stLog(const char* message);
const char* a1stStateDirectory(void);
void a1stDiskClose(void);
void a1stPcmCreate(void);
bool a1stSaveSram(char* error, size_t capacity);
void a1stCheckFrameBounds(void);
