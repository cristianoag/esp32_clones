/******************************************************************************
 * Project      : esp32_cp400_emulator
 * File         : FileSystemUtils.h
 * Last Updated : 2026-08-17
 *
 * Description  : Generic SD card / LittleFS filesystem helper utilities
 *                (list, create, read, write, rename, delete) used by the
 *                CP400 emulator
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


#ifndef FILE_SYSTEM_UTILS_H
#define FILE_SYSTEM_UTILS_H

#include "Arduino.h"
#include "FS.h"


void listDir(fs::FS &fs, const char * dirname, uint8_t levels);
void createDir(fs::FS &fs, const char * path);
void removeDir(fs::FS &fs, const char * path);
void readFile(fs::FS &fs, const char * path);
void writeFile(fs::FS &fs, const char * path, const char * message);
void appendFile(fs::FS &fs, const char * path, const char * message);
void renameFile(fs::FS &fs, const char * path1, const char * path2);
void deleteFile(fs::FS &fs, const char * path);
void testFileIO(fs::FS &fs, const char * path);

#endif
