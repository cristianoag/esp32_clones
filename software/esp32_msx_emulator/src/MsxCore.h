#pragma once
#include <stddef.h>
#include "MsxMappers.h"
#include "MsxAudioProfiles.h"

// Runs until the platform requests exit. Models: 0=MSX1, 1=MSX2, 2=MSX2+.
// ramPages is 4, 8, 16 or 32 (16 KiB pages). BIOS paths are absolute POSIX paths.
// fMSX uses byte-sized mapper registers addressing 256 banks of 8 KiB.
constexpr unsigned MSX_MAX_CART_BYTES = 2U * 1024U * 1024U;
// Read-only size/header preflight; safe while the core runs. Null/empty is valid.
// An optional error buffer is cleared on success and NUL-terminated on failure.
bool MsxValidateCartridge(const char* absolutePath, char* error, size_t errorSize);
// Full read-only inspection using the chosen profile, safe while another BIOS runs.
bool MsxInspectCartridge(const char* absolutePath, const char* profileDirectory,
                         MsxCartridgeInfo& info, char* error, size_t errorSize);
// Raw DSK: 80 tracks, 9 x 512-byte sectors, 1 or 2 sides (360/720 KiB).
// CAS: standard 8-byte CAS marker; WAV and writes are not supported.
bool MsxValidateDisk(const char* absolutePath, char* error, size_t errorSize);
bool MsxValidateTape(const char* absolutePath, char* error, size_t errorSize);
// Invoke only on the CPU task while paused. Empty/null ejects; failures preserve
// the previous attachment and return an error without stopping the emulator.
bool MsxAttachDisk(unsigned drive, const char* path, char* error, size_t errorSize);
bool MsxAttachTape(const char* path, char* error, size_t errorSize);
bool MsxRewindTape(char* error, size_t errorSize);
bool MsxDiskAvailable();
// Cartridges occupy physical primary slots 1 and 2; null/empty means ejected.
// Paths are absolute POSIX paths and remain valid until the run returns.
// Requested cartridges must load completely before any emulated code runs.
bool MsxCoreRun(const char* romDirectory, int model, int ramPages,
                const char* slot1 = nullptr, const char* slot2 = nullptr,
                const char* diskA = nullptr, const char* diskB = nullptr,
                const char* tape = nullptr,
                unsigned mapper1 = MsxMapperAuto, unsigned mapper2 = MsxMapperAuto,
                unsigned audioProfile = MsxAudioAuto);
