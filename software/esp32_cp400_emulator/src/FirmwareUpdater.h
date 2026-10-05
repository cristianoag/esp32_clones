/******************************************************************************
 * Project      : esp32_cp400_emulator
 * File         : FirmwareUpdater.h
 * Last Updated : 2026-08-17
 *
 * Description  : OTA firmware update helpers for flashing the CP400 emulator
 *                firmware from the SD card
 *
 * Original work copyright (c) 2026 Cedric Beaudoin
 * CP400 code and modifications copyright (c) 2026 The Retro Hacker
 *
 * Permission is granted for personal, non-commercial use only.
 * Commercial use, distribution, sublicensing, or modification
 * for commercial purposes is strictly prohibited without
 * prior written permission from the author.
 * Please, keep this in the source code.
 * All rights reserved.
 ******************************************************************************/


#ifndef FIRMWARE_UPDATER_H
#define FIRMWARE_UPDATER_H

#include <stddef.h>
#include <stdint.h>

// sdPath is an SD_MMC path ("/firmware.FLH", without "/sdcard").
bool Cp400InstallFirmware(const char *sdPath, void (*progress)(const char *, uint8_t),
                          char *error, size_t errorSize);

#endif
