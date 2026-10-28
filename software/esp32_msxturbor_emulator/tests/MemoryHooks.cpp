#include "MemoryHooks.h"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstdint>

struct Allocation
{
    void* pointer;
    size_t size;
};

static Allocation allocations[4096];
static size_t allocated;
static size_t limit = SIZE_MAX;

extern "C" {
void* __real_malloc(size_t size);
void* __real_calloc(size_t count, size_t size);
void* __real_realloc(void* pointer, size_t size);
void __real_free(void* pointer);
}

static Allocation* find(void* pointer)
{
    if (pointer)
        for (auto& entry : allocations)
            if (entry.pointer == pointer) return &entry;
    return nullptr;
}

static void record(void* pointer, size_t size)
{
    if (!pointer) return;
    for (auto& entry : allocations) {
        if (entry.pointer) continue;
        entry = {pointer, size};
        allocated += size;
        return;
    }
    std::fputs("FAIL: Allocation ledger capacity exceeded.\n", stderr);
    std::abort();
}

static bool fits(size_t size, size_t replaced = 0)
{
    if (allocated - replaced <= limit && size <= limit - (allocated - replaced)) return true;
    errno = ENOMEM;
    return false;
}

extern "C" void* __wrap_malloc(size_t size)
{
    if (!fits(size)) return nullptr;
    void* pointer = __real_malloc(size);
    record(pointer, size);
    return pointer;
}

extern "C" void* __wrap_calloc(size_t count, size_t size)
{
    if (count && size > SIZE_MAX / count) { errno = ENOMEM; return nullptr; }
    if (!fits(count * size)) return nullptr;
    void* pointer = __real_calloc(count, size);
    record(pointer, count * size);
    return pointer;
}

extern "C" void* __wrap_realloc(void* pointer, size_t size)
{
    Allocation* previous = find(pointer);
    if (!fits(size, previous ? previous->size : 0)) return nullptr;
    void* result = __real_realloc(pointer, size);
    if (!result && size) return nullptr;
    if (previous) { allocated -= previous->size; *previous = {}; }
    record(result, size);
    return result;
}

extern "C" void __wrap_free(void* pointer)
{
    Allocation* entry = find(pointer);
    if (entry) { allocated -= entry->size; *entry = {}; }
    __real_free(pointer);
}

size_t a1stTestAllocatedBytes() { return allocated; }
size_t a1stTestAllocationLimit() { return limit; }
void a1stTestSetAllocationLimit(size_t bytes) { limit = bytes; }
