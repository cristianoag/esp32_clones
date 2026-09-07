/******************************************************************************
 * Project      : esp32_cp400_emulator
 * File         : CP400Emulator.h
 * Last Updated : 2026-08-17
 *
 * Description  : Core CP400 emulator definitions, structures and declarations
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


#ifndef CP400_EMULATOR_H
#define CP400_EMULATOR_H


#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "SD_MMC.h"
#include "CP400Menu.h"
#include "CP400Roms.h"
#include <EEPROM.h>
//----------------------
#define VSYNC_PORT GPIO_NUM_1
#define HSYNC_PORT GPIO_NUM_2




//Keeping the emulated ROM and RAM in PSRAM puts an octal SPI transaction behind
//every 6809 memory access. Those accesses run from the CPU timer interrupt, so
//the bus traffic is strictly periodic and couples into the analog section as an
//audible tone whenever the emulated CPU is running. Internal SRAM has room for
//both arrays and is far faster, so the emulation no longer touches PSRAM. The
//large buffers, menu backup, RAM disks and video lines, stay in PSRAM.
//#define PSRAM_EMU
//#define DEBUG_ALL
//#define DEBUG_PRINT
//#define PRINT_DEBUG

//------------
#ifdef PRINT_DEBUG
#define debug Serial.print
#define debugln Serial.println
#define debugf Serial.printf
#else
#define debug //
#define debugln //
#define debugf //
#endif


#define DEBUG1_SET GPIO.out_w1ts = (1 << DEBUG1)
#define DEBUG1_CLR GPIO.out_w1tc = (1 << DEBUG1)


#define MACRO_DO_CASSETTE_BIT_OUTPUT \
if ((value & 0b00000010) == 0) \
{ \
    GPIO.out_w1tc = (1 << BOARD_CASSETTE_OUT);  /* Output 0 */ \
} \
else \
{ \
    GPIO.out_w1ts = (1 << BOARD_CASSETTE_OUT);  /* Output 1 */ \
}


struct SpecialFunctionStruct
{
  uint8_t DIRECT_Key_Code;
  bool CPU_HALTED_BY_EMULATOR; //CPU Halted because the Emulator is in menus.
  bool PHYSICAL_Drive_Must_Be_Saved; //To Save VIrtual Disk to Physical SD Card.
  bool nmi_pin; //Mapped to CPU pîn NMI
  bool firq_pin; //Mapped to CPU pîn FIRQ
  bool irq_pin; //Mapped to CPU pîn IRQ
  bool V_Synch;
  bool V_Synch_Int_Enabled;
  uint16_t ROM_Offset; //Rom address offset wuen executing from ROM address.
  bool AnyKeypress;   //Global VAR for any keypress check;
  bool CP400Mode;   //True at boot
  bool CP400VideoMode;
  bool CPU_Speed;
  uint8_t CP400VideoGenMODE;
  uint8_t CP400GraphicMode;
  bool CP400_32K_UPPER_ENABLED;
  uint8_t CP400ColorMode;
  uint8_t CP400VideoPageOffset_Registers;   //7 bits data of SET/CLR only offset register to do mult to final usable offset of CP400VideoPageOffset
  uint16_t VideoEmulatorXpixels;   //define the number of X pixel to center the Emulation screen.
  bool is_JOY1_B1_WasPressed;
  bool is_JOY1_B2_WasPressed;
  bool is_JOY2_B1_WasPressed;
  bool is_JOY2_B2_WasPressed;
  bool is_LastKeyboardScanned;
  bool Artefact;
  bool JoystickDebug;
};

struct DriveStruct
{
  uint8_t Name_Disk[4][256];
  uint8_t LastNumber_Accessed;
  uint8_t LastAccessType; //Read Write
  bool Error;
  bool isFileAlreadyOpen;
  uint8_t DriveNumber;
};

enum class DiskRomSelection : uint8_t
{
  CP400 = 0,
  CoCo2 = 1
};

extern DiskRomSelection selectedDiskRom;




#define SD_MMC_CMD 38 //Please do not modify it.
#define SD_MMC_CLK 39 //Please do not modify it. 
#define SD_MMC_D0  40 //Please do not modify it.

//-------------Keys definition for Emulator in Custom Menu-----------------------

#define MENU_F12 69
#define MENU_UP 82
#define MENU_DOWN 81
#define MENU_LEFT 80
#define MENU_RIGHT 79
#define MENU_ESC 41
#define MENU_1 30
#define MENU_2 31
#define MENU_3 32
#define MENU_4 33
#define MENU_5 34
#define MENU_6 35
#define MENU_7 36
#define MENU_8 37
#define MENU_9 38
#define MENU_0 39
#define MENU_ENTER 40
#define MENU_PGDWN 78
#define MENU_PGUP 75
#define MENU_DELETE 76
#define MENU_R 21
#define MENU_F 9
#define MENU_D 7
#define MENU_Y 28
#define MENU_N 17
#define MENU_A 4
#define MENU_B 5




//---------------------------------------------------------------

//#define PSRAM_EMU  //Load Rom and RAM of emulation in PSRAM instead of RAM


void DoCPU(void);


//-----------Audio output-----------------------------------------
//The CP400 has a single mono DAC, so the board drives one audio output on
//GPIO47. GPIO48 is deliberately not used: on the ESP32-S3-DevKitC-1 module that
//pin is the data line of the onboard WS2812 RGB LED (PIN_NEOPIXEL in the variant
//header). Wiring it into the audio network made the LED latch the sound signal
//as colour data and inject its supply current into the analog section as noise.
#define AUDIO_PIN 47
#define AUDIO_CHANNEL 0
//The onboard RGB LED. Only used to switch it off at startup.
#define BOARD_RGB_LED_PIN 48
//The output is filtered by 330R/47nF + 330R/100nF (about 4kHz, two poles) and AC
//coupled through a 10uF cap. 80MHz / (156250 * 256) is an exact divider of 2, so
//the carrier has no divider jitter and lands roughly 54dB down in that filter
//instead of the 30dB that 40kHz left behind as ultrasonic hiss.
#define AUDIO_PWM_FREQUENCY 156250
#define AUDIO_PWM_RESOLUTION 8
//The CP400 DAC is only the top 6 bits of FF20. Bits 1 and 0 are the RS232 and
//cassette lines and must never modulate the audio.
#define AUDIO_DAC_MASK 0b11111100

//A DAC level nobody is updating is silence on real hardware, where it is simply
//a DC voltage. A PWM pin has to keep switching to hold that same level, and that
//switching is the noise left at the BASIC prompt. Once the machine stops writing
//the DAC there is no sound left to reproduce, so the output is faded out and the
//pin stops toggling until the next write arrives. The fade avoids the thump a
//hard step would push through the 10uF coupling caps.
#define AUDIO_IDLE_TIMEOUT_MS 150
#define AUDIO_FADE_TICK_MS 5
#define AUDIO_FADE_STEP 4

void AudioWriteSample(uint8_t value);
void AudioSilence(void);


#define JOY1_X_AN_PIN 4  //Joy Right X
#define JOY1_Y_AN_PIN 5  //Joy Right Y
#define JOY2_X_AN_PIN 6  //Joy Left X
#define JOY2_Y_AN_PIN 10 //Joy Left Y
#define JOY1_B1 46
#define JOY1_B2 17
#define JOY2_B1 3
#define JOY2_B2 45


//PIA mux order is fixed by the CP400/CoCo hardware: 0=Right X, 1=Right Y, 2=Left X, 3=Left Y
#define JOY1_X_SELECT 0
#define JOY1_Y_SELECT 1
#define JOY2_X_SELECT 2
#define JOY2_Y_SELECT 3

//-----------Videos modes for the emulator
#define VIDEO_MODE_320X240_16_9 0
#define VIDEO_MODE_320X240_4_3 1
#define VIDEO_MODE_640X240_16_9 2
#define VIDEO_MODE_640X240_4_3 3


#define CP400_GRAPH_OFFSET_LEN 512
//For Rom access
#define ROM_OFFSET 0x8000
//#define ROM_OFFSET 0


#define ROM_FF00 0xff00 - ROM_OFFSET
#define ROM_FF01 0xff01 - ROM_OFFSET
#define ROM_FF03 0xff03 - ROM_OFFSET
#define ROM_FF02 0xff02 - ROM_OFFSET
#define ROM_FF03 0xff03 - ROM_OFFSET
#define ROM_FF20 0xff20 - ROM_OFFSET
#define ROM_FF23 0xff23 - ROM_OFFSET
//DISK ACCESS
#define ROM_FF40 0xff40 - ROM_OFFSET
#define ROM_FF48 0xff48 - ROM_OFFSET
#define ROM_FF49 0xff49 - ROM_OFFSET
#define ROM_FF4A 0xff4a - ROM_OFFSET
#define ROM_FF4B 0xff4b - ROM_OFFSET

//For Switch Case
#define M_FF00 0xff00
#define M_FF01 0xff01
#define M_FF02 0xff02
#define M_FF03 0xff03
#define M_FF20 0xff20

#define M_FF22 0xff22
#define M_FF23 0xff23

//Disk access
#define M_FF40 0xff40
#define M_FF48 0xff48
#define M_FF49 0xff49
#define M_FF4A 0xff4a
#define M_FF4B 0xff4b

#define M_FFC0 0xffc0
#define M_FFC1 0xffc1
#define M_FFC2 0xffc2
#define M_FFC3 0xffc3
#define M_FFC4 0xffc4
#define M_FFC5 0xffc5
#define M_FFC6 0xffc6
#define M_FFC7 0xffc7
#define M_FFC8 0xffc8
#define M_FFC9 0xffc9
#define M_FFCA 0xffca
#define M_FFCB 0xffcb
#define M_FFCC 0xffcc
#define M_FFCD 0xffcd
#define M_FFCE 0xffce
#define M_FFCF 0xffcf
#define M_FFD0 0xffd0
#define M_FFD1 0xffd1
#define M_FFD2 0xffd2
#define M_FFD3 0xffd3
#define M_FFD4 0xffd4
#define M_FFD5 0xffd5
#define M_FFD8 0xffd8
#define M_FFD9 0xffd9
#define M_FFDE 0xffde //ROM 32K
#define M_FFDF 0xffdf //RAM 32K UPPER
  
  
  
  #define ROM_FF22 0xff22 - ROM_OFFSET  



#define CPU_FAST true // Double Speed (~1.78 MHz)
#define CPU_SLOW false // Single Speed (~0.79 MHz)



#define VDG_BLACK       0x0000      /*   0,   0,   0 */
#define VDG_NAVY        0x000F      /*   0,   0, 128 */
#define VDG_DARKGREEN   0x03E0      /*   0, 128,   0 */
#define VDG_DARKCYAN    0x03EF      /*   0, 128, 128 */
#define VDG_MAROON      0x7800      /* 128,   0,   0 */
#define VDG_PURPLE      0x780F      /* 128,   0, 128 */
#define VDG_OLIVE       0x7BE0      /* 128, 128,   0 */
#define VDG_LIGHTGREY   0xD69A      /* 211, 211, 211 */
#define VDG_DARKGREY    0x7BEF      /* 128, 128, 128 */
#define VDG_BLUE        0x001F      /*   0,   0, 255 */
#define VDG_GREEN       0x07E0      /*   0, 255,   0 */
#define VDG_CYAN        0x07FF      /*   0, 255, 255 */
#define VDG_RED         0xF800      /* 255,   0,   0 */
#define VDG_MAGENTA     0xF81F      /* 255,   0, 255 */
#define VDG_YELLOW      0xFFE0      /* 255, 255,   0 */
#define VDG_WHITE       0xFFFF      /* 255, 255, 255 */
#define VDG_ORANGE      0xFDA0      /* 255, 180,   0 */
#define VDG_GREENYELLOW 0xB7E0      /* 180, 255,   0 */
#define VDG_PINK        0xFE19      /* 255, 192, 203 */ //Lighter pink, was 0xFC9F
#define VDG_BROWN       0x9A60      /* 150,  75,   0 */
#define VDG_GOLD        0xFEA0      /* 255, 215,   0 */
#define VDG_SILVER      0xC618      /* 192, 192, 192 */
#define VDG_SKYBLUE     0x867D      /* 135, 206, 235 */
#define VDG_VIOLET      0x915C      /* 180,  46, 226 */

constexpr uint8_t RGB332ToVGAPacked(uint8_t color)
{
  return ((color & 0b11100000) >> 5)
       | ((color & 0b00011100) << 1)
       | ((color & 0b00000011) << 6);
}

static_assert(RGB332ToVGAPacked(0b00011100) == 0b00111000,
              "RGB332 green must map to the VGA green bits");

void CheckFirmwareUpdate(void);

void InitSD_Card(void);
void InitSD_Card1(void);
extern bool SD_Card_Mounted;
uint8_t GetDriveNumber(uint8_t Address);
void InitDisks(void);
uint8_t MountFileSystem(void);
void WriteDiskByte(uint32_t BytePos, uint8_t ByteData);
uint8_t ReadDiskByte(uint32_t BytePos);

bool ReadCP400DiskImage(const char* filename, uint8_t DriveNumber);
bool EjectCP400Disk(uint8_t DriveNumber);
bool WriteCP400DiskImage(const char* filename, uint8_t DriveNumber);
bool SaveConfigToSD(void);
bool LoadConfigFromSD(void);



void InitPorts(void);
uint8_t ReadCP400Buttons(void);
uint8_t ReadJoysticks(uint8_t JoyNum);
void CopyDiskToRamDisk(void);
void DisplayVDGchar(uint8_t charNum, uint16_t Xpos, uint16_t Ypos);
bool IsButtonPressed(void);
void ManagePeripherals_Write(uint16_t address, uint8_t value);
void ManagePeripherals_Read(uint16_t address);
void ManageKeyboardScan(uint8_t value);
void FillKeyboardMatrix(void);
void CopyCP400ROMS(void);
void CopyCoCo3ROMS(void); //Optional CoCo 3 compatibility ROMs, kept for reference/testing.

void SetVideoMode(uint8_t VideoMode);
void VideoCore(void *pvParameters);
void line(int x0, int y0, int x1, int y1, int rgb);

void InitPeripherals_and_Others(void);

//---------------Videos modes----------

void RenderCP400GraphMode_32X16_8X12(void);
void RenderCP400GraphMode_256X192X2(void);
void RenderCP400GraphMode_256X192X2_Artefact(void);
void RenderCP400GraphMode_128X192X4(void);
void RenderCP400GraphMode_128X192X2(void);
void RenderCP400GraphMode_128X96X4(void);
void RenderCP400GraphMode_128X96X2(void);
void RenderCP400GraphMode_128X64X4(void);
void RenderCP400GraphMode_128X64X2(void);
void RenderCP400GraphMode_64X64X4(void);



#define DEBUG1 13
#define DEBUG2 13




#endif
