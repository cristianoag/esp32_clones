#pragma once
#include <stddef.h>

constexpr size_t Cp400FirmwarePathCapacity = 512;
constexpr size_t Cp400FirmwarePageSize = 11;
enum class Cp400FirmwareEntryKind { File, Directory, Parent };
struct Cp400FirmwareEntry
{
    char name[256];
    Cp400FirmwareEntryKind kind;
};

bool Cp400JoinFirmwarePath(const char *directory, const char *name,
                          char *output, size_t capacity);
void Cp400ParentFirmwarePath(char *path);
bool Cp400ReadFirmwarePage(const char *directory, size_t first,
                          Cp400FirmwareEntry *entries, size_t &count, size_t &total,
                          char *error, size_t errorSize);
