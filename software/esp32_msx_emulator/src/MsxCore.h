#pragma once
#include <stddef.h>

// Runs until the platform requests exit. Models: 0=MSX1, 1=MSX2, 2=MSX2+.
// ramPages is 4, 8, 16 or 32 (16 KiB pages). BIOS paths are absolute POSIX paths.
// fMSX uses byte-sized mapper registers addressing 256 banks of 8 KiB.
constexpr unsigned MSX_MAX_CART_BYTES = 2U * 1024U * 1024U;
// Read-only size/header preflight; safe while the core runs. Null/empty is valid.
// An optional error buffer is cleared on success and NUL-terminated on failure.
bool MsxValidateCartridge(const char* absolutePath, char* error, size_t errorSize);
// Cartridges occupy physical primary slots 1 and 2; null/empty means ejected.
// Paths are absolute POSIX paths and remain valid until the run returns.
// Requested cartridges must load completely before any emulated code runs.
bool MsxCoreRun(const char* romDirectory, int model, int ramPages,
                const char* slot1 = nullptr, const char* slot2 = nullptr);
