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
#include "FirmwareUpdater.h"
#include "Cp400SdBrowser.h"

extern uint8_t *MENU_Backup;
extern uint8_t *MENU_BackupPage2;
extern SpecialFunctionStruct sf;
extern DriveStruct Disk_Drive;

void WaitForMenuKeyRelease(void);

constexpr uint8_t MENU_TEXT_COLUMNS = 44;
constexpr uint16_t MENU_WIDTH = MENU_TEXT_COLUMNS * 6 + 2; // The last 8-pixel glyph overlaps its 6-pixel cell.
constexpr uint16_t MENU_HEIGHT = 28 * 8;
constexpr uint16_t MENU_CONTENT_RIGHT = 264;

static uint16_t MenuOriginX(void)
{
    return vga->mode.hRes > MENU_WIDTH ? (vga->mode.hRes - MENU_WIDTH) / 2 : 0;
}

static uint16_t MenuOriginY(void)
{
    return vga->mode.vRes > MENU_HEIGHT ? (vga->mode.vRes - MENU_HEIGHT) / 2 : 0;
}

#ifdef PSRAM_EMU
extern uint8_t *rom;
extern uint8_t *memory;
#else
extern uint8_t rom[32769];
extern uint8_t memory[65536];
#endif



struct FileArrayStruct
{
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
    const uint16_t originX = MenuOriginX();
    const uint16_t originY = MenuOriginY();
    line(x0 + originX, y0 + originY,
         x1 + originX, y1 + originY,
         RGB332ToVGAPacked(color));
}

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
static void FirmwareProgressUpdate(const char *stage, uint8_t percent)
{
    if (strcmp(ProgressMessage, stage) != 0)
    {
        ProgressMessage = stage;
        DrawProgressScreen(stage, percent);
        return;
    }
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

void PopulateDiskContent(const char *FileToLoad)
{
    File f;
    uint8_t DataDisk;
    uint8_t LoopFile = 0;
    uint32_t LoopSeek;
    volatile uint32_t BytePos = 256 * 18 * 17 + 512; // Start of filenames
    ClearFileList();

    f = SD_MMC.open(FileToLoad, FILE_READ);
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

    uint32_t loop1;
    uint16_t YposLoop;
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
    X = (uint16_t)Xpos * 6 + XpixOffset;
    Y = (uint16_t)Ypos * 8 + YpixOffset;
    const uint16_t originX = MenuOriginX();
    const uint16_t originY = MenuOriginY();
    ForeColor = RGB332ToVGAPacked(ForeColor);
    BackColor = RGB332ToVGAPacked(BackColor);

    if (Y + 8 > MENU_HEIGHT || Y + originY + 8u > vga->mode.vRes)
    {
        return;
    }

    while (*text != '\0' && X + 8 <= MENU_WIDTH && X + originX + 8u <= vga->mode.hRes)
    {
        MENU_DisplayGIMEchar(*text, 0, X + originX, Y + originY, ForeColor, BackColor);
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
    for (Y = 0; Y < vga->mode.vRes; Y++)
    {
        for (X = 0; X < vga->mode.hRes; X++)
        {
            MENU_Backup[loopBack++] = vga->getPixel(X, Y);
        }

    }
    loopBack = 0;
    vga->show();    //Swap Buffer
    for (Y = 0; Y < vga->mode.vRes; Y++)
    {
        for (X = 0; X < vga->mode.hRes; X++)
        {
            MENU_BackupPage2[loopBack++] = vga->getPixel(X, Y);
        }

    }

    vga->show();    //Return to the first saved buffer.
    vga->clear(0);
    vga->show();
    vga->clear(0);
    vga->show();


}

void RestoreDisplay(void)
{
    uint32_t loopBack = 0;
    uint16_t X, Y;


    for (Y = 0; Y < vga->mode.vRes; Y++)
    {
        for (X = 0; X < vga->mode.hRes; X++)
        {
            vga->dot(X, Y, MENU_Backup[loopBack++]);
        }
    }
    vga->show();
    loopBack = 0;
    for (Y = 0; Y < vga->mode.vRes; Y++)
    {
        for (X = 0; X < vga->mode.hRes; X++)
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

static void ShowFirmwareUpdateError(const char *message)
{
    DrawScreenFrame("Firmware update failed", "ENTER or ESC: return to menu", nullptr);
    for (uint8_t row = MENU_CONTENT_ROW; *message && row < 24; row++)
    {
        size_t length = strcspn(message, "\n");
        if (length > MENU_TEXT_COLUMNS)
        {
            length = MENU_TEXT_COLUMNS;
            size_t split = length;
            while (split && message[split] != ' ') --split;
            if (split) length = split;
        }
        char line[MENU_TEXT_COLUMNS + 1];
        memcpy(line, message, length);
        line[length] = '\0';
        DrawText(line, 0, row, 0, 0, MENU_ALERT_COLOR, 0);
        message += length;
        while (*message == ' ' || *message == '\n') ++message;
    }
    vga->show();
    WaitForMenuKeyRelease();
    while (sf.DIRECT_Key_Code != MENU_ENTER && sf.DIRECT_Key_Code != MENU_ESC)
        vTaskDelay(2);
    WaitForMenuKeyRelease();
}

void RunFirmwareUpdateFlow(void)
{
    char *file = Firmware_Choose();
    if (strcmp(file, "NF") == 0)
    {
        return;
    }
    Serial.printf("[Firmware update] Selected %s\n", file);

    DrawScreenFrame("Firmware update", "Press Y or N to choose", "ESC also cancels");
    DrawText("Install the selected firmware?", 0, MENU_CONTENT_ROW, 0, 0, MENU_TEXT_COLOR, 0);
    const char *name = strrchr(file, '/');
    char selectedName[MENU_TEXT_COLUMNS + 1];
    snprintf(selectedName, sizeof(selectedName), "%.*s", MENU_TEXT_COLUMNS, name ? name + 1 : file);
    DrawText(selectedName, 0, MENU_CONTENT_ROW + 1, 0, 0, MENU_ACCENT_COLOR, 0);
    DrawText("Y  Yes - install and reboot", 2, MENU_CONTENT_ROW + 2, 0, 0,
             MENU_ACCENT_COLOR, 0);
    DrawText("N  No  - cancel update", 2, MENU_CONTENT_ROW + 4, 0, 0,
             MENU_TEXT_COLOR, 0);
    vga->show();
    while (1)
    {
        switch (sf.DIRECT_Key_Code)
        {
        case MENU_Y:
        {
            Serial.printf("[Firmware update] Confirmed %s; starting installation.\n", file);
            ProgressMessage = "";
            FirmwareProgressUpdate("Preparing update", 0);
            char error[160] = {};
            if (!Cp400InstallFirmware(file, FirmwareProgressUpdate, error, sizeof(error)))
            {
                ShowFirmwareUpdateError(error);
                return;
            }

            DrawScreenFrame("Firmware update", "Do not switch off the CP400.", nullptr);
            DrawText("Update complete. Rebooting...", 0, MENU_CONTENT_ROW, 0, 0,
                     MENU_TEXT_COLOR, 0);
            vga->show();
            vTaskDelay(2000);
            esp_restart();
            return;
        }

        case MENU_N:
        case MENU_ESC:
            Serial.println("[Firmware update] Cancelled by user.");
            WaitForMenuKeyRelease();
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
        const char *path = (const char*)Disk_Drive.Name_Disk[drive];
        const char *slash = strrchr(path, '/');
        const char *diskName = *path ? (slash ? slash + 1 : path) : "<empty>";
        DrawSelectableRow(3 + drive, label, diskName, drive == selectedDrive);
    }
}

static char* ChooseSdFile(bool diskImage)
{
    static char selectedPath[Cp400SdPathCapacity];
    static Cp400SdEntry entries[Cp400SdPageSize];
    char directory[Cp400SdPathCapacity] = "/";
    const char *extension = diskImage ? ".DSK" : ".FLH";
    size_t first = 0, selected = 0;
    if (!IsSDCardAvailable())
    {
        ShowMenuError("No microSD card detected.");
        return (char*)"NF";
    }

    WaitForMenuKeyRelease();
    while (true)
    {
        size_t count = 0, total = 0;
        char error[160] = {};
        if (!IsSDCardAvailable())
        {
            ShowMenuError("microSD card was removed.");
            return (char*)"NF";
        }
        if (!Cp400ReadSdPage(directory, extension, first, entries, count, total, error, sizeof(error)))
        {
            ShowMenuError(error);
            return (char*)"NF";
        }
        if (error[0]) ShowMenuError(error);
        if (selected >= count) selected = count ? count - 1 : 0;
        DrawScreenFrame(diskImage ? "Disk image" : "Firmware update",
                        diskImage ? "Arrows/PgUp/PgDn move  ENTER open/select" : "UP/DOWN move  ENTER open/install",
                        "ESC parent folder / cancel at root");
        char caption[MENU_TEXT_COLUMNS + 1];
        snprintf(caption, sizeof(caption), "%.*s", MENU_TEXT_COLUMNS, directory);
        DrawText(caption, 0, diskImage ? 25 : MENU_CONTENT_ROW, 0, 0, MENU_ACCENT_COLOR, 0);
        const uint8_t firstRow = diskImage ? 2 : MENU_CONTENT_ROW + 2;
        for (size_t i = 0; i < count; ++i)
        {
            const char *kind = entries[i].kind == Cp400SdEntryKind::File ? extension : "<DIR>";
            DrawSelectableRow(firstRow + i, entries[i].name, kind, i == selected);
        }
        if (!count)
        {
            snprintf(caption, sizeof(caption), "No %s files or folders.", extension);
            DrawText(caption, 0, firstRow, 0, 0, MENU_TEXT_COLOR, 0);
        }
        if (diskImage)
        {
            ClearFileList();
            if (count && entries[selected].kind == Cp400SdEntryKind::File &&
                Cp400JoinSdPath(directory, entries[selected].name, selectedPath, sizeof(selectedPath)))
                PopulateDiskContent(selectedPath);
            DisplayDiskContent();
            DrawFrames();
        }
        vga->show();
        while (!sf.DIRECT_Key_Code) vTaskDelay(10);
        const auto key = sf.DIRECT_Key_Code;
        WaitForMenuKeyRelease();
        switch (key)
        {
        case MENU_DOWN:
            if (count && first + selected + 1 < total)
            {
                if (++selected == count)
                {
                    first += count;
                    selected = 0;
                }
            }
            break;
        case MENU_UP:
            if (selected) --selected;
            else if (first)
            {
                first -= Cp400SdPageSize;
                selected = Cp400SdPageSize - 1;
            }
            break;
        case MENU_PGDWN:
            if (count && first + selected + 1 < total)
            {
                const size_t remaining = total - first - selected - 1;
                const size_t target = first + selected + (remaining < Cp400SdPageSize ? remaining : Cp400SdPageSize);
                first = target / Cp400SdPageSize * Cp400SdPageSize;
                selected = target - first;
            }
            break;
        case MENU_PGUP:
            {
                const size_t current = first + selected;
                const size_t target = current > Cp400SdPageSize ? current - Cp400SdPageSize : 0;
                first = target / Cp400SdPageSize * Cp400SdPageSize;
                selected = target - first;
            }
            break;
        case MENU_ENTER:
            if (!count) break;
            if (entries[selected].kind == Cp400SdEntryKind::Parent)
                Cp400ParentSdPath(directory);
            else
            {
                if (!Cp400JoinSdPath(directory, entries[selected].name,
                                     selectedPath, sizeof(selectedPath)))
                {
                    ShowMenuError("Invalid or overlong SD path.");
                    break;
                }
                if (entries[selected].kind == Cp400SdEntryKind::File)
                {
                    if (diskImage && strlen(selectedPath) >= sizeof(Disk_Drive.Name_Disk[0]))
                    {
                        ShowMenuError("Disk path too long (max 255 characters).");
                        break;
                    }
                    return selectedPath;
                }
                strcpy(directory, selectedPath);
            }
            first = selected = 0;
            break;
        case MENU_ESC:
            if (!strcmp(directory, "/")) return (char*)"NF";
            Cp400ParentSdPath(directory);
            first = selected = 0;
            break;
        default:
            break;
        }
    }
}

void DiskMenuChoose_1(uint8_t DriveNumber)
{
    const char *path = ChooseSdFile(true);
    if (!strcmp(path, "NF")) return;
    if (!IsSDCardAvailable())
    {
        ShowMenuError("microSD card was removed.");
        return;
    }
    if (!ReadCP400DiskImage(path, DriveNumber))
    {
        ShowMenuError("Unable to read disk image.");
        return;
    }
    strcpy((char*)Disk_Drive.Name_Disk[DriveNumber], path);
    if (!SaveConfigToSD())
        ShowMenuError("Unable to save configuration.");
}

char* Firmware_Choose(void)
{
    return ChooseSdFile(false);
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
