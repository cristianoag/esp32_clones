/******************************************************************************
 * Project      : esp32_cp400_emulator
 * File         : CP400Menu.cpp
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


#include "CP400Menu.h"
#include "CP400Emulator.h"
#include "CP400Input.h"
#include "CP400Roms.h"

extern uint8_t *MENU_Backup;
extern uint8_t *MENU_BackupPage2;
extern SpecialFunctionStruct sf;
extern DriveStruct Disk_Drive;

extern void flashFromSD(const char* filename);
extern void InitFilesystem(void);
extern bool copyFile(const char* srcFilename, const char* destFilename, void (*progress)(uint8_t));
extern bool ValidFirmwareFile(const char* filename, void (*progress)(uint8_t));

void WaitForMenuKeyRelease(void);

constexpr uint8_t MENU_X_ORIGIN = 6;
constexpr uint8_t MENU_Y_ORIGIN = 8;
constexpr uint16_t MENU_RIGHT_EDGE = 271;
constexpr uint16_t MENU_BOTTOM_EDGE = 239;
constexpr uint16_t MENU_CONTENT_RIGHT = 264;

#ifdef PSRAM_EMU
extern uint8_t *rom;
extern uint8_t *memory;
#else
extern uint8_t rom[32769];
extern uint8_t memory[65536];
#endif



struct FileArrayStruct
{
  uint8_t FileBuffer[31][255];
  int16_t FileStart;
  int16_t FileEnd;
  uint8_t FileList[69][11];
};
FileArrayStruct FileArray;

bool IsSDCardAvailable(void)
{
    if (!SD_Card_Mounted)
    {
        return false;
    }

    if (SD_MMC.cardType() == CARD_NONE)
    {
        SD_Card_Mounted = false;
        return false;
    }

    return true;
}

void DrawMenuLine(int x0, int y0, int x1, int y1, int color)
{
    line(x0 + MENU_X_ORIGIN, y0 + MENU_Y_ORIGIN,
         x1 + MENU_X_ORIGIN, y1 + MENU_Y_ORIGIN,
         RGB332ToVGAPacked(color));
}

constexpr uint8_t MENU_TEXT_COLUMNS = 44;
constexpr uint8_t MENU_CONTENT_ROW = 3;
constexpr uint8_t MENU_ACCENT_COLOR = 0b00011100;
constexpr uint8_t MENU_TEXT_COLOR = 255;
constexpr uint8_t MENU_ALERT_COLOR = 0b11100000;
constexpr uint8_t MENU_PROGRESS_BAR_ROW = 5;
constexpr uint8_t MENU_PROGRESS_TEXT_ROW = 7;

static void RenderScreenFrame(const char *title, const char *hint1, const char *hint2)
{
    DrawText("ESP32 Clone Series", 0, 0, 0, 0, MENU_TEXT_COLOR, 0);

    const size_t titleLength = strlen(title);
    if (titleLength < MENU_TEXT_COLUMNS)
    {
        DrawText(title, (uint8_t)(MENU_TEXT_COLUMNS - titleLength), 0, 0, 0,
                 MENU_ACCENT_COLOR, 0);
    }

    DrawMenuLine(0, 10, MENU_CONTENT_RIGHT, 10, MENU_ACCENT_COLOR);

    const bool secondHint = hint2 != nullptr && hint2[0] != '\0';
    const uint16_t footerLine = secondHint ? 199 : 207;
    DrawMenuLine(0, footerLine, MENU_CONTENT_RIGHT, footerLine, MENU_ACCENT_COLOR);

    DrawText(hint1, 0, secondHint ? 26 : 27, 0, 0, MENU_TEXT_COLOR, 0);
    if (secondHint)
    {
        DrawText(hint2, 0, 27, 0, 0, MENU_TEXT_COLOR, 0);
    }
}

//Shared frame for every F12 screen: product name, right-aligned title, separators and hints.
void DrawScreenFrame(const char *title, const char *hint1, const char *hint2)
{
    vga->clear(0);
    vga->show();
    vga->clear(0);

    RenderScreenFrame(title, hint1, hint2);
}

void DrawProgressBar(uint8_t row, uint8_t percent)
{
    if (percent > 100)
    {
        percent = 100;
    }

    const uint16_t top = (uint16_t)row * 8;
    const uint16_t bottom = top + 9;

    DrawMenuLine(0, top, MENU_CONTENT_RIGHT, top, MENU_ACCENT_COLOR);
    DrawMenuLine(0, bottom, MENU_CONTENT_RIGHT, bottom, MENU_ACCENT_COLOR);
    DrawMenuLine(0, top, 0, bottom, MENU_ACCENT_COLOR);
    DrawMenuLine(MENU_CONTENT_RIGHT, top, MENU_CONTENT_RIGHT, bottom, MENU_ACCENT_COLOR);

    const uint16_t span = (uint16_t)(((uint32_t)(MENU_CONTENT_RIGHT - 4) * percent) / 100);
    if (span > 0)
    {
        for (uint16_t fill = top + 2; fill + 2 <= bottom; fill++)
        {
            DrawMenuLine(2, fill, 2 + span, fill, MENU_TEXT_COLOR);
        }
    }
}

static const char *ProgressMessage = "";

static void DrawProgressValue(uint8_t percent)
{
    char value[8];
    snprintf(value, sizeof(value), "%u%%", percent);
    DrawProgressBar(MENU_PROGRESS_BAR_ROW, percent);
    DrawText(value, 0, MENU_PROGRESS_TEXT_ROW, 0, 0, MENU_ACCENT_COLOR, 0);
}

//Both buffers get the same frame, otherwise each update would flash the blank one.
void DrawProgressScreen(const char *message, uint8_t percent)
{
    for (uint8_t buffer = 0; buffer < 2; buffer++)
    {
        vga->clear(0);
        RenderScreenFrame("Firmware update", "Do not switch off the CP400.", nullptr);
        DrawText(message, 0, MENU_CONTENT_ROW, 0, 0, MENU_TEXT_COLOR, 0);
        DrawProgressValue(percent);
        vga->show();
    }
}

//Only the gauge changes during an operation, so the frame is left untouched.
static void FirmwareProgressUpdate(uint8_t percent)
{
    for (uint8_t buffer = 0; buffer < 2; buffer++)
    {
        DrawProgressValue(percent);
        vga->show();
    }
}

void ShowMenuError(const char *message)
{
    DrawScreenFrame("Error", "Returning to the previous screen...", nullptr);
    DrawText(message, 0, MENU_CONTENT_ROW, 0, 0, MENU_ALERT_COLOR, 0);
    vga->show();
    vTaskDelay(2000);
}

int8_t FillFileBuffer(uint16_t startIndex, int8_t MaxIndex, const char* fileExt)
{
    ClearFileBuffer();

    if (!IsSDCardAvailable() || MaxIndex < 0)
    {
        return -1;
    }

    File root = SD_MMC.open("/");
    if (!root)
    {
#ifdef DEBUG_PRINT
        Serial.println("Failed to open root directory");
#endif
        return -1;
    }

    if (!root.isDirectory())
    {
#ifdef DEBUG_PRINT
        Serial.println("Root is not a directory");
#endif
        root.close();
        return -1;
    }

    File file = root.openNextFile();
    uint16_t currentIndex = 0;
    int8_t bufferIndex = 0;
    while (file && bufferIndex <= MaxIndex)
    {
        const char* name = file.name();
        bool validEntry = false;

        //Directories cannot be opened as images, so only matching files are listed.
        if (!file.isDirectory())
        {
            const char* ext = strrchr(name, '.');
            if (ext && strcasecmp(ext, fileExt) == 0)            
            {
                validEntry = true;
            }
        }

        if (currentIndex >= startIndex && validEntry)
        {
            snprintf((char*)FileArray.FileBuffer[bufferIndex],
                     255,
                     "%s",
                     name);

            bufferIndex++;
        }

        currentIndex++;

        file.close();
        file = root.openNextFile();
    }

    root.close();

#ifdef DEBUG_PRINT
    Serial.print("Loaded ");
    Serial.print(bufferIndex);
    Serial.println(" entries into FileBuffer");
#endif

    if (bufferIndex == 0)
        return -1; // Aucun fichier chargé

    return bufferIndex - 1; // Dernier index rempli
}



void PopulateDiskContent(const char *FileToLoad)
{
    File f;
    uint8_t DataDisk;
    uint8_t LoopFile = 0;
    uint32_t LoopSeek;
    char tmpBuf[100];
    volatile uint32_t BytePos = 256 * 18 * 17 + 512; // Start of filenames
    ClearFileList();
    snprintf(tmpBuf, sizeof(tmpBuf), "/%s", FileToLoad);

    f = SD_MMC.open(tmpBuf, FILE_READ);
    if (!f)
    {
#ifdef DEBUG_PRINT
        Serial.print("Failed to open file: ");
#endif
        return;
    }

   
    for (LoopFile = 0; LoopFile < 68; LoopFile++)
    {
        for (LoopSeek = 0; LoopSeek < 11; LoopSeek++)
        {
            
            if (!f.seek(BytePos++))
            {
#ifdef DEBUG_PRINT
                Serial.printf("Seek failed at %lu\n", BytePos-1);
#endif
                f.close();
                return;
            }

           
            int byteRead = f.read(&DataDisk, 1);
            if (byteRead != 1)
            {
#ifdef DEBUG_PRINT
                Serial.printf("Read failed at %lu\n", BytePos-1);
#endif
                f.close();
                return;
            }

           
            FileArray.FileList[LoopFile][LoopSeek] = DataDisk;
        }

        
        BytePos += 21;
    }

    f.close();      
    
}

void ClearFileList(void)
{
    memset(FileArray.FileList, 0xFF, sizeof(FileArray.FileList));
}
void ClearFileBuffer(void)
{
    memset(FileArray.FileBuffer, 0, sizeof(FileArray.FileBuffer));
}

void PrintFileName(uint8_t LoopFile)
{

    for (int i = 0; i < 11; i++) 
    {
       debug((char)FileArray.FileList[LoopFile][i]);
    }
    debugln("");
}



void MENU_DisplayGIMEchar(uint8_t charNum, uint8_t ODDbyte, uint16_t Xpos, uint16_t Ypos, uint8_t ForeColor, uint8_t BackColor)
{
    // uint32_t BlinkPhase = (sf.GIME_BLINK_COUTER_COPY & 0b00010000);
    uint32_t BlinkPhase = 0;

    uint32_t loop1, loop2;
    uint16_t XposLoop, YposLoop;
    uint8_t tmpChar, loopArray;
    uint8_t Underline, Blink;

    // ForeColor = 0b00000000;
    // BackColor = 0b00011100;

    Blink     = (ODDbyte & 0b10000000);
    Underline = (ODDbyte & 0b01000000);

    // Precalculate an array of 8 pixels, it's faster in PSRAM
    uint8_t tmpArray[8];

    loop1 = charNum * 12 + 1;

    for (YposLoop = Ypos; YposLoop != Ypos + 8; YposLoop++)
    {
        loopArray = 0;
        tmpChar   = font_gimeChip[loop1++];

        for (uint8_t bit = 0; bit != 8; bit++)
        {
            if ((tmpChar & 0b10000000) != 0)
            {
                if (Blink == 0b10000000)
                {
                    if (BlinkPhase != 0)
                    {
                        tmpArray[loopArray++] = BackColor;
                    }
                    else
                    {
                        tmpArray[loopArray++] = ForeColor;
                    }
                }
                else
                {
                    tmpArray[loopArray++] = ForeColor;
                }
            }
            else
            {
                tmpArray[loopArray++] = BackColor;
            }

            tmpChar <<= 1;
        }

        if ((YposLoop == Ypos + 7) && (Underline != 0))
        {
            
            for (uint8_t loop3 = 0; loop3 !=8; loop3++)
            {
                tmpArray[loop3] = ForeColor;                
            }

            vga->drawLineFromMemory8(Xpos, YposLoop, &tmpArray[0]);
        }
        else
        {
            vga->drawLineFromMemory8(Xpos, YposLoop, &tmpArray[0]);
        }
    }
}





void DrawText(const char *text, uint8_t Xpos, uint8_t Ypos, uint8_t XpixOffset, uint8_t YpixOffset, uint8_t ForeColor, uint8_t BackColor)
{
    uint16_t X, Y;
    X = (uint16_t)Xpos * 6 + XpixOffset + MENU_X_ORIGIN;
    Y = (uint16_t)Ypos * 8 + YpixOffset + MENU_Y_ORIGIN;
    ForeColor = RGB332ToVGAPacked(ForeColor);
    BackColor = RGB332ToVGAPacked(BackColor);

    if (Y + 7 > MENU_BOTTOM_EDGE)
    {
        return;
    }

    while (*text != '\0' && X + 7 <= MENU_RIGHT_EDGE)
    {
        MENU_DisplayGIMEchar(*text, 0, X, Y, ForeColor, BackColor); 
        text++;
        X +=6;
    }
}

void EMULATOR_Menu(void)
{
    BackDisplay();
    EMU_Draw_Menu();
    RestoreDisplay();
}

void BackDisplay(void)
{
    uint32_t loopBack = 0;
    uint16_t X, Y;
    vga->show();    //Swap Buffer
    for (Y=0; Y!=240;Y++)
    {
        for (X=0; X!=320;X++)
        {
            MENU_Backup[loopBack++] = vga->getPixel(X, Y);
        }

    }
    loopBack = 0;
    vga->show();    //Swap Buffer
    for (Y=0; Y!=240;Y++)
    {
        for (X=0; X!=320;X++)
        {
            MENU_BackupPage2[loopBack++] = vga->getPixel(X, Y);
        }

    }

    vga->clear(0);
    vga->show();
    vga->clear(0);
    vga->show();


}

void RestoreDisplay(void)
{
    uint32_t loopBack = 0;
    uint16_t X, Y;


    for (Y = 0; Y < 240; Y++)
    {
        for (X = 0; X < 320; X++)
        {
            vga->dot(X, Y, MENU_Backup[loopBack++]);
        }
    }
    vga->show();
    loopBack = 0;
    for (Y = 0; Y < 240; Y++)
    {
        for (X = 0; X < 320; X++)
        {
            vga->dot(X, Y, MENU_BackupPage2[loopBack++]);
        }
    }

}

enum MainMenuItem : uint8_t
{
    MAIN_MENU_DISK_DRIVES,
    MAIN_MENU_FIRMWARE,
    MAIN_MENU_ARTIFACT_COLORS,
    MAIN_MENU_DISK_ROM,
    MAIN_MENU_JOYSTICK_CALIBRATION,
    MAIN_MENU_REBOOT,
    MAIN_MENU_EXIT,
    MAIN_MENU_ITEM_COUNT
};

void DrawSelectableRow(uint8_t row, const char *label, const char *value, bool selected)
{
    constexpr size_t rowWidth = 43;
    char text[rowWidth + 1];
    memset(text, ' ', rowWidth);
    text[rowWidth] = '\0';

    snprintf(text, rowWidth + 1, "%-23s %s", label, value);
    const size_t textLength = strlen(text);
    if (textLength < rowWidth)
    {
        memset(text + textLength, ' ', rowWidth - textLength);
        text[rowWidth] = '\0';
    }

    DrawText(text, 0, row, 0, 0,
             selected ? 0b00000000 : 0b11111111,
             selected ? 0b00011100 : 0b00000000);
}

void ChangeMainMenuValue(uint8_t selectedItem)
{
    if (selectedItem == MAIN_MENU_ARTIFACT_COLORS)
    {
        sf.Artefact = !sf.Artefact;
        return;
    }

    if (selectedItem == MAIN_MENU_DISK_ROM)
    {
        const DiskRomSelection previousSelection = selectedDiskRom;
        selectedDiskRom = selectedDiskRom == DiskRomSelection::CP400
                            ? DiskRomSelection::CoCo2
                            : DiskRomSelection::CP400;
        if (!SaveConfigToSD())
        {
            selectedDiskRom = previousSelection;
            ShowMenuError("Unable to save Disk ROM.");
        }
    }
}

void RunFirmwareUpdateFlow(void)
{
    char *file = Firmware_Choose();
    if (strcmp(file, "NF") == 0)
    {
        return;
    }

    DrawScreenFrame("Firmware update", "Press Y or N to choose", "ESC also cancels");
    DrawText("Install the selected firmware?", 0, MENU_CONTENT_ROW, 0, 0, MENU_TEXT_COLOR, 0);
    DrawText("Y  Yes - install and reboot", 2, MENU_CONTENT_ROW + 2, 0, 0,
             MENU_ACCENT_COLOR, 0);
    DrawText("N  No  - cancel update", 2, MENU_CONTENT_ROW + 4, 0, 0,
             MENU_TEXT_COLOR, 0);
    vga->show();
    WaitForMenuKeyRelease();

    while (1)
    {
        switch (sf.DIRECT_Key_Code)
        {
        case MENU_Y:
            ProgressMessage = "Checking firmware...";
            DrawProgressScreen(ProgressMessage, 0);

            if (!ValidFirmwareFile(file, FirmwareProgressUpdate))
            {
                ShowMenuError("Invalid firmware file.");
                return;
            }

            ProgressMessage = "Copying update file...";
            DrawProgressScreen(ProgressMessage, 0);

            if (!copyFile(file, "/qprcx.rty", FirmwareProgressUpdate))
            {
                ShowMenuError("Unable to copy firmware.");
                return;
            }

            DrawScreenFrame("Firmware update", "Do not switch off the CP400.", nullptr);
            DrawText("Rebooting to install firmware...", 0, MENU_CONTENT_ROW, 0, 0,
                     MENU_TEXT_COLOR, 0);
            vga->show();
            vTaskDelay(2000);
            esp_restart();
            return;

        case MENU_N:
        case MENU_ESC:
            return;

        default:
            break;
        }
        vTaskDelay(2);
    }
}

void WaitForMenuKeyRelease(void)
{
    while (sf.DIRECT_Key_Code != 0)
    {
        vTaskDelay(2);
    }
}

void DrawJoystickCalibrationScreen(uint8_t joystick, const char *line1, const char *line2)
{
    char title[16];
    snprintf(title, sizeof(title), "Joystick %u", joystick + 1);
    DrawScreenFrame(title, "ENTER continue  ESC cancel", nullptr);
    DrawText(line1, 0, MENU_CONTENT_ROW, 0, 0, MENU_TEXT_COLOR, 0);
    DrawText(line2, 0, MENU_CONTENT_ROW + 2, 0, 0, MENU_TEXT_COLOR, 0);
    vga->show();
}

bool CaptureJoystickReport(uint8_t joystick, uint8_t *destination)
{
    WaitForMenuKeyRelease();
    while (sf.DIRECT_Key_Code != MENU_ENTER)
    {
        if (sf.DIRECT_Key_Code == MENU_ESC)
        {
            return false;
        }
        vTaskDelay(2);
    }

    memcpy(destination, USB_DEV_CONTROL.JOYSTICK_REPORT[joystick], JOYSTICK_REPORT_SIZE);
    return true;
}

void SampleJoystickRestState(uint8_t joystick, const uint8_t *neutral, uint8_t *stable,
                             uint8_t reportLength)
{
    memset(stable, 1, JOYSTICK_REPORT_SIZE);
    for (uint16_t sample = 0; sample < 250; sample++)
    {
        for (uint8_t index = 0; index < reportLength; index++)
        {
            if (USB_DEV_CONTROL.JOYSTICK_REPORT[joystick][index] != neutral[index])
            {
                stable[index] = 0;
            }
        }
        vTaskDelay(2);
    }
}

bool JoystickControlsMatch(const JoystickCalibration &calibration, uint8_t first, uint8_t second)
{
    for (uint8_t index = 0; index < calibration.reportLength; index++)
    {
        const uint8_t bits = calibration.controlBits[first][index];
        if (bits != calibration.controlBits[second][index] ||
            (calibration.control[first][index] & bits) !=
            (calibration.control[second][index] & bits))
        {
            return false;
        }
    }
    return true;
}

void RunJoystickCalibration(uint8_t joystick)
{
    JoystickCalibration previousCalibration = USB_DEV_CONTROL.JOYSTICK_CALIBRATION[joystick];
    JoystickCalibration calibration = {};

    if (USB_DEV_CONTROL.JOYSTICK_REPORT_LENGTH[joystick] == 0)
    {
        DrawJoystickCalibrationScreen(joystick, "Joystick is not connected.",
                                      "Check USB connection and retry.");
        vTaskDelay(1500);
        return;
    }

    calibration.reportLength = USB_DEV_CONTROL.JOYSTICK_REPORT_LENGTH[joystick];

    DrawJoystickCalibrationScreen(joystick, "Release and center the joystick.",
                                  "Press ENTER when centered.");
    if (!CaptureJoystickReport(joystick, calibration.neutral))
    {
        return;
    }

    DrawJoystickCalibrationScreen(joystick, "Checking joystick at rest...",
                                  "Do not touch the joystick.");
    SampleJoystickRestState(joystick, calibration.neutral, calibration.stable,
                            calibration.reportLength);

    const char *controlNames[JOYSTICK_CONTROL_COUNT] =
    {
        "UP", "DOWN", "LEFT", "RIGHT", "BUTTON 1", "BUTTON 2"
    };
    char instruction[40];
    for (uint8_t control = 0; control < JOYSTICK_CONTROL_COUNT; control++)
    {
        snprintf(instruction, sizeof(instruction), "Hold %s on joystick %u.",
                 controlNames[control], joystick + 1);
        DrawJoystickCalibrationScreen(joystick, instruction,
                                      "Press ENTER while holding it.");
        if (!CaptureJoystickReport(joystick, calibration.control[control]))
        {
            return;
        }

        bool controlDetected = false;
        for (uint8_t index = 0; index < calibration.reportLength; index++)
        {
            const uint8_t changed =
                calibration.stable[index]
                  ? (calibration.neutral[index] ^ calibration.control[control][index])
                  : 0;
            calibration.controlBits[control][index] = changed;
            controlDetected = controlDetected || changed != 0;
        }

        if (USB_DEV_CONTROL.JOYSTICK_REPORT_LENGTH[joystick] != calibration.reportLength ||
            !controlDetected)
        {
            DrawJoystickCalibrationScreen(joystick, "Control was not detected.",
                                          "Hold it firmly and retry.");
            WaitForMenuKeyRelease();
            vTaskDelay(1200);
            return;
        }

        for (uint8_t previousControl = 0; previousControl < control; previousControl++)
        {
            if (JoystickControlsMatch(calibration, previousControl, control))
            {
                DrawJoystickCalibrationScreen(joystick, "Duplicate control detected.",
                                              "Use a different control and retry.");
                WaitForMenuKeyRelease();
                vTaskDelay(1200);
                return;
            }
        }
    }

    for (uint8_t index = 0; index < calibration.reportLength; index++)
    {
        uint8_t directionBits = 0;
        for (uint8_t direction = 0; direction <= JOYSTICK_RIGHT; direction++)
        {
            directionBits |= calibration.controlBits[direction][index];
        }
        calibration.directionBits[index] = directionBits;
    }

    calibration.valid = 1;
    USB_DEV_CONTROL.JOYSTICK_CALIBRATION[joystick] = calibration;
    if (!SaveConfigToSD())
    {
        USB_DEV_CONTROL.JOYSTICK_CALIBRATION[joystick] = previousCalibration;
        ShowMenuError("Unable to save calibration.");
        return;
    }

    PrintJoystickCalibration(joystick);
    DrawJoystickCalibrationScreen(joystick, "Calibration saved.", "");
    WaitForMenuKeyRelease();
    vTaskDelay(800);
}

void DrawJoystickCalibrationMenu(uint8_t selectedJoystick)
{
    DrawScreenFrame("Joystick setup", "UP/DOWN move  ENTER select  ESC back", nullptr);
    DrawSelectableRow(3, "Joystick 1",
                      USB_DEV_CONTROL.JOYSTICK_CALIBRATION[0].valid ? "Calibrated >" : "Calibrate >",
                      selectedJoystick == 0);
    DrawSelectableRow(4, "Joystick 2",
                      USB_DEV_CONTROL.JOYSTICK_CALIBRATION[1].valid ? "Calibrated >" : "Calibrate >",
                      selectedJoystick == 1);
    DrawSelectableRow(5, "Serial debug", sf.JoystickDebug ? "< Enabled >" : "< Disabled >",
                      selectedJoystick == 2);
    DrawSelectableRow(6, "Back", "Enter", selectedJoystick == 3);
}

void JoystickCalibrationMenu(void)
{
    uint8_t selectedJoystick = 0;
    DrawJoystickCalibrationMenu(selectedJoystick);
    vga->show();
    WaitForMenuKeyRelease();

    while (1)
    {
        switch (sf.DIRECT_Key_Code)
        {
        case MENU_UP:
            if (selectedJoystick > 0)
            {
                selectedJoystick--;
                DrawJoystickCalibrationMenu(selectedJoystick);
                vga->show();
            }
            vTaskDelay(120);
            break;
        case MENU_DOWN:
            if (selectedJoystick < 3)
            {
                selectedJoystick++;
                DrawJoystickCalibrationMenu(selectedJoystick);
                vga->show();
            }
            vTaskDelay(120);
            break;
        case MENU_LEFT:
        case MENU_RIGHT:
            if (selectedJoystick == 2)
            {
                sf.JoystickDebug = !sf.JoystickDebug;
                DrawJoystickCalibrationMenu(selectedJoystick);
                vga->show();
            }
            vTaskDelay(150);
            break;
        case MENU_ENTER:
            if (selectedJoystick == 3)
            {
                return;
            }
            if (selectedJoystick == 2)
            {
                sf.JoystickDebug = !sf.JoystickDebug;
                if (sf.JoystickDebug)
                {
                    PrintJoystickCalibration(0);
                    PrintJoystickCalibration(1);
                }
                DrawJoystickCalibrationMenu(selectedJoystick);
                vga->show();
                vTaskDelay(150);
                break;
            }
            RunJoystickCalibration(selectedJoystick);
            DrawJoystickCalibrationMenu(selectedJoystick);
            vga->show();
            WaitForMenuKeyRelease();
            break;
        case MENU_ESC:
            return;
        default:
            break;
        }
        vTaskDelay(2);
    }
}



void EMU_Draw_Menu(void)
{
    uint8_t selectedItem = MAIN_MENU_DISK_DRIVES;
    DrawMainMenuOptions(selectedItem);
    vga->show();

    while (1)
    {
        switch (sf.DIRECT_Key_Code)
        {
        case MENU_UP:
            if (selectedItem > 0)
            {
                selectedItem--;
                DrawMainMenuOptions(selectedItem);
                vga->show();
            }
            vTaskDelay(120);
            break;

        case MENU_DOWN:
            if (selectedItem + 1 < MAIN_MENU_ITEM_COUNT)
            {
                selectedItem++;
                DrawMainMenuOptions(selectedItem);
                vga->show();
            }
            vTaskDelay(120);
            break;

        case MENU_LEFT:
        case MENU_RIGHT:
            if (selectedItem == MAIN_MENU_ARTIFACT_COLORS ||
                selectedItem == MAIN_MENU_DISK_ROM)
            {
                ChangeMainMenuValue(selectedItem);
                DrawMainMenuOptions(selectedItem);
                vga->show();
            }
            vTaskDelay(150);
            break;

        case MENU_ENTER:
            switch (selectedItem)
            {
            case MAIN_MENU_DISK_DRIVES:
                DiskMenuChoose();
                break;

            case MAIN_MENU_FIRMWARE:
                RunFirmwareUpdateFlow();
                break;

            case MAIN_MENU_ARTIFACT_COLORS:
                ChangeMainMenuValue(selectedItem);
                break;

            case MAIN_MENU_DISK_ROM:
                ChangeMainMenuValue(selectedItem);
                break;

            case MAIN_MENU_JOYSTICK_CALIBRATION:
                JoystickCalibrationMenu();
                break;

            case MAIN_MENU_REBOOT:
                memset(memory, 255, 65536);
                esp_restart();
                return;

            case MAIN_MENU_EXIT:
                return;
            }

            DrawMainMenuOptions(selectedItem);
            vga->show();
            vTaskDelay(150);
            break;

        case MENU_ESC:
            return;

        default:
            break;
        }
        vTaskDelay(2);
    }
}


void DiskMenuChoose(void)
{
    if (!IsSDCardAvailable())
    {
        ShowMenuError("No microSD card detected.");
        return;
    }

    WaitForMenuKeyRelease();

    uint8_t selectedDrive = 0;
    DrawMenuDiskChoose(selectedDrive);
    vga->show();
    
    while (1)
    {
        switch (sf.DIRECT_Key_Code)
        {
        case MENU_UP:
            if (selectedDrive > 0)
            {
                selectedDrive--;
                DrawMenuDiskChoose(selectedDrive);
                vga->show();
            }
            vTaskDelay(120);
            break;

        case MENU_DOWN:
            if (selectedDrive < 3)
            {
                selectedDrive++;
                DrawMenuDiskChoose(selectedDrive);
                vga->show();
            }
            vTaskDelay(120);
            break;

        case MENU_ENTER:
            DiskMenuChoose_1(selectedDrive);
            WaitForMenuKeyRelease();
            DrawMenuDiskChoose(selectedDrive);
            vga->show();
            vTaskDelay(150);
            break;

        case MENU_DELETE:
            if (Disk_Drive.Name_Disk[selectedDrive][0] != '\0')
            {
                if (!EjectCP400Disk(selectedDrive))
                {
                    ShowMenuError("Unable to save disk ejection.");
                    return;
                }
                DrawMenuDiskChoose(selectedDrive);
                vga->show();
            }
            WaitForMenuKeyRelease();
            break;

        case MENU_ESC:
            return;

        default:
            break;
        }
        vTaskDelay(2);
    }
}

void DrawMainMenuOptions(uint8_t selectedItem)
{
    char title[24];
    snprintf(title, sizeof(title), "Firmware %s", FW_VERSION);
    DrawScreenFrame(title, "UP/DOWN move  LEFT/RIGHT change", "ENTER select  ESC exit");

    DrawSelectableRow(3, "Disk drives", "Enter >", selectedItem == MAIN_MENU_DISK_DRIVES);
    DrawSelectableRow(4, "Firmware update", "Enter >", selectedItem == MAIN_MENU_FIRMWARE);
    DrawSelectableRow(5, "Artifact colors", sf.Artefact ? "< Enabled >" : "< Disabled >",
                      selectedItem == MAIN_MENU_ARTIFACT_COLORS);
    DrawSelectableRow(6, "Boot Disk ROM",
                      selectedDiskRom == DiskRomSelection::CP400 ? "< CP400 >" : "< CoCo 2 >",
                      selectedItem == MAIN_MENU_DISK_ROM);
    DrawSelectableRow(7, "Joystick setup", "Enter >",
                      selectedItem == MAIN_MENU_JOYSTICK_CALIBRATION);
    DrawSelectableRow(8, "Reboot CP400", "Enter", selectedItem == MAIN_MENU_REBOOT);
    DrawSelectableRow(9, "Exit menu", "Enter", selectedItem == MAIN_MENU_EXIT);
}


void DrawMenuDiskChoose(uint8_t selectedDrive)
{
    DrawScreenFrame("Disk drives", "UP/DOWN move  ENTER assign", "DELETE eject  ESC back");

    for (uint8_t drive = 0; drive < 4; drive++)
    {
        char label[16];
        snprintf(label, sizeof(label), "Drive %u", drive);
        const char *diskName = Disk_Drive.Name_Disk[drive][0] != '\0'
                                 ? (char*)Disk_Drive.Name_Disk[drive] + 1
                                 : "<empty>";
        DrawSelectableRow(3 + drive, label, diskName, drive == selectedDrive);
    }
}

void DrawDiskMenuChoose_1(void)
{
    DrawScreenFrame("Disk image", "Arrows move  PgUp/PgDn page", "ENTER select  ESC back");
    return;
}

void DrawFirmwareUpdateMenuChoose(void)
{
    DrawScreenFrame("Firmware update", "Arrows move  ENTER select  ESC back", nullptr);
    return;
}

void DiskMenuChoose_1(uint8_t DriveNumber)
{
    #define DELAY_MENU_SELECT 80
    int16_t FileStart = 0;
    int16_t MenuPick = 0;
    
    int8_t MenuMaxIndex;
    char buf[15];

    if (!IsSDCardAvailable())
    {
        ShowMenuError("No microSD card detected.");
        return;
    }

    vga->clear(0);
    vga->show();
    vga->clear(0);
    vga->show();

    DrawDiskMenuChoose_1();
    MenuMaxIndex =  FillFileBuffer(MenuPick, 10,".DSK");
    if (MenuMaxIndex < 0)
    {
        ShowMenuError("No .DSK files found.");
        return;
    }
    DrawFiles(MenuPick,0b11100000, 0b00000011);
    DisplayDiskContent();
    DrawFrames();
    
    vga->show();
    vTaskDelay(200);
    while(1)
    {

        switch (sf.DIRECT_Key_Code)
        {
        case MENU_DOWN:
            MenuPick++;
            //FileStart++;
            if (MenuPick > MenuMaxIndex)
            {
                FileStart +=11;
                MenuMaxIndex =  FillFileBuffer(FileStart, 10,".DSK");
                if (MenuMaxIndex == -1) //Error
                {
                    MenuPick--;
                    FileStart-=11;
                }
                else
                {
                    MenuPick = 0;
                }
            }
            vga->clear(0);

            DrawDiskMenuChoose_1();
            
            DrawFiles(MenuPick,0b11100000, 0b00000011);
            DisplayDiskContent();
            DrawFrames();
            vga->show();
            vTaskDelay(DELAY_MENU_SELECT);
            break;
        case MENU_PGDWN:
            
            for (uint8_t loop1 = 0; loop1 !=11; loop1++)
            {
                MenuPick++;
                //FileStart++;
                if (MenuPick > MenuMaxIndex)
                {
                    FileStart +=11;
                    MenuMaxIndex =  FillFileBuffer(FileStart, 10,".DSK");
                    if (MenuMaxIndex == -1) //Error
                    {
                        MenuPick--;
                        FileStart-=11;
                    }
                    else
                    {
                        MenuPick = 0;
                    }
                }
            }
            vga->clear(0);
            //DrawText ("Choose the file to assign to disk drive:",0,0,0,0,255,0);
            DrawDiskMenuChoose_1();

            DrawFiles(MenuPick,0b11100000, 0b00000011);
            DisplayDiskContent();
            DrawFrames();
            vga->show();
            vTaskDelay(DELAY_MENU_SELECT);
            break;
        case MENU_UP:
        
            MenuPick--;
            //FileStart--;
            if (MenuPick < 0 && FileStart < 0)
            {
                MenuPick = 0;
                FileStart = 0;
            }
            else if (MenuPick < 0 && FileStart >= 0)
            {
                MenuPick+=11;
                FileStart-=11;
                if (FileStart <0)
                {
                    FileStart = 0;
                    MenuPick = 0;
                }
                MenuMaxIndex =  FillFileBuffer(FileStart, 10,".DSK");
            }
            vga->clear(0);

            DrawDiskMenuChoose_1();
            DrawFiles(MenuPick,0b11100000, 0b00000011);
            DisplayDiskContent();
            DrawFrames();
            vga->show();
            vTaskDelay(DELAY_MENU_SELECT);
            break;
        case MENU_PGUP:
            for (uint8_t loop1 = 0; loop1 !=11; loop1++)
            {
                MenuPick--;
                //FileStart--;
                if (MenuPick < 0 && FileStart < 0)
                {
                    MenuPick = 0;
                    FileStart = 0;
                }
                else if (MenuPick < 0 && FileStart >= 0)
                {
                    MenuPick+=11;
                    FileStart-=11;
                    if (FileStart <0)
                    {
                        FileStart = 0;
                        MenuPick = 0;
                    }
                    MenuMaxIndex =  FillFileBuffer(FileStart, 10,".DSK");
                }
            }
                vga->clear(0);
                DrawDiskMenuChoose_1();
                DrawFiles(MenuPick,0b11100000, 0b00000011);
                DisplayDiskContent();
                DrawFrames();
                vga->show();
                vTaskDelay(DELAY_MENU_SELECT);
                break;
        case MENU_ENTER:
                if (!IsSDCardAvailable())
                {
                    ShowMenuError("microSD card was removed.");
                    return;
                }
                snprintf((char*)Disk_Drive.Name_Disk[DriveNumber],
                         sizeof(Disk_Drive.Name_Disk[DriveNumber]), "/%s",
                         (char*)FileArray.FileBuffer[MenuPick]);
                if (!ReadCP400DiskImage((const char*)Disk_Drive.Name_Disk[DriveNumber], DriveNumber))
                {
                    ShowMenuError("Unable to read disk image.");
                    return;
                }
                if (!SaveConfigToSD())
                {
                    ShowMenuError("Unable to save configuration.");
                    return;
                }
                return;
            break;
            
        case MENU_ESC:
            return;
            break;
            
        
        default:
            break;
        }
        vTaskDelay(10);
        
    }

}


char* Firmware_Choose(void)
{
    #define DELAY_MENU_SELECT 80
    int16_t FileStart = 0;
    int16_t MenuPick = 0;
    
    int8_t MenuMaxIndex;
    char buf[15];

    if (!IsSDCardAvailable())
    {
        ShowMenuError("No microSD card detected.");
        return (char*)"NF";
    }

    vga->clear(0);
    vga->show();
    vga->clear(0);
    vga->show();

    DrawFirmwareUpdateMenuChoose();
    MenuMaxIndex =  FillFileBuffer(MenuPick, 10,".FLH");
    if (MenuMaxIndex < 0)
    {
        ShowMenuError("No .FLH files found.");
        return (char*)"NF";
    }
    DrawFiles(MenuPick,0b11100000, 0b00000011);
    
    //DrawFrames();
    
    vga->show();
    vTaskDelay(200);
    while(1)
    {

        switch (sf.DIRECT_Key_Code)
        {
        case MENU_DOWN:
            MenuPick++;
            //FileStart++;
            if (MenuPick > MenuMaxIndex)
            {
                FileStart +=11;
                MenuMaxIndex =  FillFileBuffer(FileStart, 10,".FLH");
                if (MenuMaxIndex == -1) //Error
                {
                    MenuPick--;
                    FileStart-=11;
                }
                else
                {
                    MenuPick = 0;
                }
            }
            vga->clear(0);

            DrawFirmwareUpdateMenuChoose();
            
            DrawFiles(MenuPick,0b11100000, 0b00000011);
            
            
            vga->show();
            vTaskDelay(DELAY_MENU_SELECT);
            break;
        case MENU_UP:
        
            MenuPick--;
            //FileStart--;
            if (MenuPick < 0 && FileStart < 0)
            {
                MenuPick = 0;
                FileStart = 0;
            }
            else if (MenuPick < 0 && FileStart >= 0)
            {
                MenuPick+=11;
                FileStart-=11;
                if (FileStart <0)
                {
                    FileStart = 0;
                    MenuPick = 0;
                }
                MenuMaxIndex =  FillFileBuffer(FileStart, 10,".FLH");
            }
            vga->clear(0);

            DrawFirmwareUpdateMenuChoose();
            DrawFiles(MenuPick,0b11100000, 0b00000011);

            vga->show();
            vTaskDelay(DELAY_MENU_SELECT);
            break;
        case MENU_ENTER:
            if (!IsSDCardAvailable())
            {
                ShowMenuError("microSD card was removed.");
                return (char*)"NF";
            }
            FileArray.FileBuffer[15][0] = '/';
            strcpy((char*)&FileArray.FileBuffer[15][1], (const char*)FileArray.FileBuffer[MenuPick]);
            return (char*)FileArray.FileBuffer[15];
            break;
            
        case MENU_ESC:
            return (char*)"NF";  // No file selected.
            break;
            
        
        default:
            break;
        }
        vTaskDelay(10);
        
    }

}


void DisplayDiskContent(void)
{
    uint16_t LoopFile, LoopChar, Xpos, Ypos, XposHome, YposHome, Loop1;
    char buf[15];

    LoopFile = 0;
    LoopChar = 0;
    Xpos = 0;
    XposHome = 0;
    YposHome = 14;
    Ypos = YposHome;

        
    for (LoopFile = 0;
         LoopFile < 30 && FileArray.FileList[LoopFile][0] != 255;
         LoopFile++)
    {
        Loop1 = 0;
        for(LoopChar = 0; LoopChar !=11; LoopChar++)
        {
            buf[Loop1++] = FileArray.FileList[LoopFile][LoopChar];
            
            if (Loop1 == 8)
            {
                buf[Loop1++] = '.';
            }
        }
        buf[Loop1] = '\0';
        DrawText(buf, Xpos, Ypos, 3,3,0b11100000, 0);

        Ypos+=1;
        if (Ypos == YposHome + 10)
        {
            Xpos+=14;
            Ypos = YposHome;
        }

    }



}


void DrawFrames(void)
{
    #define FRAME_COL_1 0b00011100
    #define FRAME_OFFSET 2
    DrawMenuLine(0, 108, MENU_CONTENT_RIGHT, 108, FRAME_COL_1);
    DrawMenuLine(0, 108, 0, 199, FRAME_COL_1);
    DrawMenuLine(MENU_CONTENT_RIGHT, 108, MENU_CONTENT_RIGHT, 199, FRAME_COL_1);
    DrawMenuLine(0, 199, MENU_CONTENT_RIGHT, 199, FRAME_COL_1);
    DrawMenuLine(88, 108, 88, 199, FRAME_COL_1);
    DrawMenuLine(176, 108, 176, 199, FRAME_COL_1);
}

uint8_t DrawFiles(uint16_t FileNumber, uint8_t ForeColor, uint8_t BackColor)
{
    uint16_t Loop1;
    uint16_t Ypos = 2;
    for (Loop1 = 0;
         Loop1 <= 10 && FileArray.FileBuffer[Loop1][0] != 0;
         Loop1++)
    {
        if (FileNumber == Loop1)
        {
            DrawText((char*)FileArray.FileBuffer[Loop1],0, Ypos++,0,0,ForeColor, 255);
            PopulateDiskContent((char*)FileArray.FileBuffer[Loop1]);
        }
        else
        {
            DrawText((char*)FileArray.FileBuffer[Loop1], 0,Ypos++,0,0,ForeColor, BackColor);
        }
    }
    return Loop1;

}