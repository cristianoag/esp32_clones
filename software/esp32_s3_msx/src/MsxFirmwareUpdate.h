#pragma once

#include <stddef.h>
#include <stdint.h>

// sdPath is an SD_MMC path ("/folder/ESP32_MSX-1.00.FLH", without "/sdcard").
// Success selects the new image for next boot; this function never reboots.
bool MsxInstallFirmware(const char *sdPath,
                        void (*progress)(const char *stage, uint8_t percent),
                        char *error, size_t errorSize);
