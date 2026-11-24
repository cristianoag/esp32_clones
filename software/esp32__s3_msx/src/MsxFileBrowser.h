#pragma once
#include "MsxMenuLayout.h"
#include "MsxSettings.h"

enum class MsxFileKind { File, Directory, Parent, Eject };

struct MsxFileEntry
{
    char name[MsxSdPathCapacity];
    MsxFileKind kind;
};

bool MsxReadDirectoryPage(const char *directory, const char *extension, bool allowEject,
                          size_t first, MsxFileEntry *entries, size_t &count, size_t &total,
                          char *error, size_t errorSize);
