#pragma once

// Runs until the platform requests exit. Models: 0=MSX1, 1=MSX2, 2=MSX2+.
// ramPages is 4, 8, 16 or 32 (16 KiB pages). BIOS paths are absolute POSIX paths.
bool MsxCoreRun(const char* romDirectory, int model, int ramPages);
