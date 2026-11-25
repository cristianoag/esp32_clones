#pragma once
#include <stddef.h>

constexpr size_t Cp400SdPathCapacity = 512;
constexpr size_t Cp400SdPageSize = 11;
enum class Cp400SdEntryKind { File, Directory, Parent };
struct Cp400SdEntry
{
    char name[256];
    Cp400SdEntryKind kind;
};

bool Cp400JoinSdPath(const char *directory, const char *name,
                     char *output, size_t capacity);
void Cp400ParentSdPath(char *path);
bool Cp400ReadSdPage(const char *directory, const char *extension, size_t first,
                     Cp400SdEntry *entries, size_t &count, size_t &total,
                     char *error, size_t errorSize);
