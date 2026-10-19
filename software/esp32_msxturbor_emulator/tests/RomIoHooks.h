#pragma once
#include <stddef.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

long a1stTestRomRead(int file, void* buffer, size_t count);
#define read a1stTestRomRead
