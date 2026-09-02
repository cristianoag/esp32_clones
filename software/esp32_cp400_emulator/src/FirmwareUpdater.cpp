/******************************************************************************
 * Project      : esp32_cp400_emulator
 * File         : FirmwareUpdater.cpp
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


#include "CP400Emulator.h"
#include "FirmwareUpdater.h"
#include <Update.h>
#include "esp_ota_ops.h"
#include "esp_heap_caps.h"
#define FORMAT_SPIFFS_IF_FAILED true


bool filesystemOK = false;
void InitFilesystem(void)
{
  // Initialize LittleFS
  if (!LittleFS.begin(false /* false: Do not format if mount failed */)) 
  {
    debugln("LittleFS mount fail");
    if (!LittleFS.begin(true /* true: format */)) 
    {
      debugln("Failed to format LittleFS");
    } 
    else 
    {
      debugln("LittleFS formatted successfully");
      filesystemOK = true;
    }
  } 
  else 
  { // Initial mount success

    filesystemOK = true;
    debugln("File System OK");
}
}

void flashFromSD(const char* filename)
{
    if (!SD_MMC.exists(filename)) 
    {
        debugln("Firmware file not found on SD!");
        return;
    }

    debugln(filename);

    File firmwareFile = SD_MMC.open(filename, FILE_READ);
    if (!firmwareFile) 
    {
        debugln("Failed to open firmware file!");
        return;
    }
    size_t firmwareSize = firmwareFile.size();
    debugf("Firmware size: %u bytes\n", firmwareSize);
    // Vérifie si ça rentre dans la partition OTA
    if (!Update.begin(firmwareSize)) {
        debugln("Not enough space for OTA!");
        firmwareFile.close();
        return;
    }
    debugln("Starting OTA update from SD...");

    size_t written = Update.writeStream(firmwareFile);
    firmwareFile.close();

    if (written != firmwareSize) 
    {
        debugf("Written only %u/%u bytes\n", written, firmwareSize);
        debugln("Update failed!");
        return;
    }

    if (Update.end(true)) 
    {
        debugln("OTA Update successful! Deleting firmware file...");

        // Supprime le fichier passé en paramètre
        if (SD_MMC.remove(filename)) 
        {
            debugln("Firmware file deleted from SD.");
        } 
        else 
        {
            debugln("Failed to delete firmware file!");
        }

        
        esp_restart();
    } 
    else 
    {
        debugln("OTA Update failed!");
    }
}

bool copyFile(const char* srcFilename, const char* destFilename, FirmwareProgressFn progress)
{
    // Check if source file exists
    if (!SD_MMC.exists(srcFilename))
    {
        debugln("Source file not found!");
        return false;
    }

    // Open source file for reading
    File srcFile = SD_MMC.open(srcFilename, FILE_READ);
    if (!srcFile)
    {
        debugln("Failed to open source file!");
        return false;
    }

    if (SD_MMC.exists(destFilename) && !SD_MMC.remove(destFilename))
    {
        debugln("Failed to remove previous firmware staging file!");
        srcFile.close();
        return false;
    }

    File destFile = SD_MMC.open(destFilename, FILE_WRITE);
    if (!destFile)
    {
        debugln("Failed to open destination file!");
        srcFile.close();
        return false;
    }

    uint8_t buf[512]; // Temporary buffer
    size_t bytesRead;
    size_t bytesWritten = 0;
    bool foundSeparator = false;
    const size_t totalSize = srcFile.size();
    size_t bytesProcessed = 0;
    uint8_t lastPercent = 255;

    // Copy the file chunk by chunk
    while ((bytesRead = srcFile.read(buf, sizeof(buf))) > 0)
    {
        size_t startIndex = 0;

        bytesProcessed += bytesRead;
        if (progress != nullptr && totalSize > 0)
        {
            const uint8_t percent = (uint8_t)((uint64_t)bytesProcessed * 100 / totalSize);
            if (percent != lastPercent)
            {
                lastPercent = percent;
                progress(percent);
            }
        }

        if (!foundSeparator)
        {
            // Look for first '~'
            for (size_t i = 0; i < bytesRead; i++)
            {
                if (buf[i] == '~')
                {
                    startIndex = i + 1; // Start after '~'
                    foundSeparator = true;
                    break;
                }
            }

            if (!foundSeparator)
                continue;
        }

        const size_t bytesToWrite = bytesRead - startIndex;
        if (bytesToWrite > 0 && destFile.write(buf + startIndex, bytesToWrite) != bytesToWrite)
        {
            debugln("Write error!");
            srcFile.close();
            destFile.close();
            SD_MMC.remove(destFilename);
            return false;
        }
        bytesWritten += bytesToWrite;
    }

    srcFile.close();
    destFile.close();

    if (!foundSeparator || bytesWritten == 0)
    {
        debugln("Firmware separator or payload not found!");
        SD_MMC.remove(destFilename);
        return false;
    }

    File stagedFile = SD_MMC.open(destFilename, FILE_READ);
    const bool validSize = stagedFile && stagedFile.size() == bytesWritten;
    if (stagedFile)
    {
        stagedFile.close();
    }
    if (!validSize)
    {
        debugln("Firmware staging file size mismatch!");
        SD_MMC.remove(destFilename);
        return false;
    }

    debugln("Firmware staged successfully!");
    return true;
}


bool copyFile1(const char* srcFilename, const char* destFilename)
{
    // Check if source file exists
    if (!SD_MMC.exists(srcFilename))
    {
        debugln("Source file not found!");
        return false;
    }

    // Open source file for reading
    File srcFile = SD_MMC.open(srcFilename, FILE_READ);
    if (!srcFile)
    {
        debugln("Failed to open source file!");
        return false;
    }

    // Open or create destination file for writing
    File destFile = SD_MMC.open(destFilename, FILE_WRITE);
    if (!destFile)
    {
        debugln("Failed to open destination file!");
        srcFile.close();
        return false;
    }

    uint8_t buf[512]; // Temporary buffer
    size_t bytesRead;

    // Copy the file chunk by chunk
    while ((bytesRead = srcFile.read(buf, sizeof(buf))) > 0)
    {
        if (destFile.write(buf, bytesRead) != bytesRead)
        {
            debugln("Write error!");
            srcFile.close();
            destFile.close();
            return false;
        }
    }

    srcFile.close();
    destFile.close();

    debugln("File copied successfully!");
    return true;
}

// Function to read a file byte by byte
// Calls user code on each byte
bool ValidFirmwareFile(const char* filename, FirmwareProgressFn progress)
{
    uint8_t ValidateSource[15];
    uint32_t ValidateSourceConverted;
    uint32_t ValidateCalculated = 0;
    bool FirmwareValide = false;
    // Check if file exists
    if (!SD_MMC.exists(filename))
    {
        debugln("File not found!");
        return false;
    }

    // Open file for reading
    File file = SD_MMC.open(filename, FILE_READ);
    if (!file)
    {
        debugln("Failed to open file!");
        return false;
    }

    uint8_t byteValue;
    uint8_t StateMachine = 0;
    uint32_t loop1 = 0;
    const size_t totalSize = file.size();
    size_t bytesProcessed = 0;
    uint8_t lastPercent = 255;
    // Read the file byte by byte
    while (file.available())
    {
        byteValue = file.read(); // Read one byte

        bytesProcessed++;
        if (progress != nullptr && totalSize > 0)
        {
            const uint8_t percent = (uint8_t)((uint64_t)bytesProcessed * 100 / totalSize);
            if (percent != lastPercent)
            {
                lastPercent = percent;
                progress(percent);
            }
        }
        if (byteValue== '-' && StateMachine == 0)
        {
            StateMachine = 1;
            ValidateSource[loop1] = 0;
            ValidateSourceConverted = asciiToUint32((char*)ValidateSource);
            debugln(ValidateCalculated);
        }
        if (StateMachine == 0)
        {
            if (byteValue > 47 && byteValue < 58)
            {
                ValidateSource[loop1++] = byteValue;
            }
            else
            {
                return false;
            }
            if (loop1 > 14)
            {
                return false;
            }
        }
        else if (StateMachine == 1)
        {
            ValidateCalculated+=byteValue;
        }
    }
    debug(ValidateCalculated);
    debug(" ");
    debugln(ValidateSourceConverted);
    if (ValidateCalculated == ValidateSourceConverted)
    {
        FirmwareValide = true;
    }
    file.close();
    debugln("File reading finished!");
    return FirmwareValide;
}

uint32_t asciiToUint32(const char* str)
{
    uint32_t result = 0;
    size_t i = 0;

    while (str[i] != '\0')
    {
        result = result * 10 + (str[i] - '0');
        i++;
    }

    return result;
}
