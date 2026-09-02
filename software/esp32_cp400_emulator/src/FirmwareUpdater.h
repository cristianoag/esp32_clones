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

#include <Arduino.h>
#include "LittleFS.h"




typedef void (*FirmwareProgressFn)(uint8_t percent);

void InitFilesystem(void);
bool copyFile(const char* srcFilename, const char* destFilename, FirmwareProgressFn progress);
void flashFromSD(const char* filename);
bool ValidFirmwareFile(const char* filename, FirmwareProgressFn progress);
uint32_t asciiToUint32(const char* str);




#endif
