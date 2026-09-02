/******************************************************************************
 * Project      : esp32_cp400_emulator
 * File         : CP400Menu.h
 * Last Updated : 2026-08-17
 *
 * Description  : CP400 emulator on-screen menu system (disk, firmware, settings)
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


#ifndef CP400_MENU_H
#define CP400_MENU_H

#include <Arduino.h>
#include <VGA.h>

extern VGA* vga;
extern void line(int x0, int y0, int x1, int y1, int rgb);


void MENU_DisplayGIMEchar(uint8_t charNum, uint8_t ODDbyte, uint16_t Xpos, uint16_t Ypos, uint8_t ForeColor, uint8_t BackColor);
void DrawText(const char *text, uint8_t Xpos, uint8_t Ypos, uint8_t XpixOffset, uint8_t YpixOffset, uint8_t ForeColor, uint8_t BackColor);
void DrawScreenFrame(const char *title, const char *hint1, const char *hint2);
void DrawProgressBar(uint8_t row, uint8_t percent);
void DrawProgressScreen(const char *message, uint8_t percent);
void EMULATOR_Menu(void);
void EMU_Draw_Menu(void);
void BackDisplay(void);
void RestoreDisplay(void);
void PopulateDiskContent(const char *FileToLoad);
void PrintFileName(uint8_t LoopFile);
void DiskMenuChoose(void);
void DiskMenuChoose_1(uint8_t DriveNumber);
void ClearFileList(void);
void ClearFileBuffer(void);
uint8_t DrawFiles(uint16_t FileNumber, uint8_t ForeColor, uint8_t BackColor);
void DisplayDiskContent(void);
int8_t FillFileBuffer(uint16_t startIndex, int8_t MaxIndex, const char* fileExt);
void DrawFrames(void);
void DrawMenuDiskChoose(uint8_t selectedDrive);
void DrawDiskMenuChoose_1(void);
void DrawMainMenuOptions(uint8_t selectedItem);
void DrawFirmwareUpdateMenuChoose(void);
char* Firmware_Choose(void);


#endif