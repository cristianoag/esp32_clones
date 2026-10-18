#pragma once
#include <stdlib.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif
void* a1stAllocate(size_t size);
void* a1stCalloc(size_t count, size_t size);
void* a1stReallocate(void* pointer, size_t size);
void a1stPanic(const char* message);
#ifdef __cplusplus
}
#else
#define malloc a1stAllocate
#define calloc a1stCalloc
#define realloc a1stReallocate
#endif
