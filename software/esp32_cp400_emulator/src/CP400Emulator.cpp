/******************************************************************************
 * Project      : esp32_cp400_emulator
 * File         : CP400Emulator.cpp
 * Last Updated : 2026-08-17
 *
 * Description  : CP400 emulator main code (CPU core, video, disk, peripherals)
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




#include <Arduino.h>
#include "mc6809.hpp"
#include "esp_timer.h"
#include "esp_system.h"

#include "CP400Emulator.h"

#include "ESP32S3vga.h"
#include <GfxWrapper.h>
#include <Fonts/FreeMonoBoldOblique24pt7b.h>
#include <Fonts/FreeSerif24pt7b.h>
#include <Fonts/FreeMono9pt7b.h>



//----------------------------USB STACK-------------------------------------

#include <ESP32-USB-Soft-Host.h>
#include "CP400Input.h"
#include "CP400UsbKeyboard.h"
#define KEY_SHIFT_LEFT




USB_DEVICES_CTRL USB_DEV_CONTROL;



  //Each pin pair is one USB connector: P1 joystick 1, P2 joystick 2.
  //The keyboard no longer uses the soft host. It runs on the USB-OTG controller
  //through GPIO19 and GPIO20 so full speed keyboards enumerate, which the
  //bit-banged stack could never do. See CP400UsbKeyboard.cpp.
  #define DP_P0  -1
  #define DM_P0  -1
  #define DP_P1  16
  #define DM_P1  15
  #define DP_P2  18
  #define DM_P2  17
  #define DP_P3  -1
  #define DM_P3  -1

extern USB_DEVICES_CTRL USB_DEV_CONTROL;

extern void Setup_USB(void);





//-----------------------------------------------------------



#define LED_PORT_ANODE 45
#define LED_PORT_CATHODE 46

#define LED_PORT_CATHODE_HI() GPIO.out1_w1ts.val = (1 << (LED_PORT_CATHODE - 32));  // Set HIGH
#define LED_PORT_CATHODE_LOW() GPIO.out1_w1tc.val = (1 << (LED_PORT_CATHODE - 32));  // Set LOW

#define LED_PORT_ANODE_HI() GPIO.out1_w1ts.val = (1 << (LED_PORT_ANODE - 32));  // Set HIGH
#define LED_PORT_ANODE_LOW()  GPIO.out1_w1tc.val = (1 << (LED_PORT_ANODE - 32));  // Set LOW

void inline DISKETTE_LED_ON(void)
{
  LED_PORT_CATHODE_HI();
  LED_PORT_ANODE_LOW();
}
void inline DISKETTE_LED_OFF(void)
{
  LED_PORT_CATHODE_LOW();
  LED_PORT_ANODE_HI();
}
void inline DISKETTE_LED_FLOAT(void)
{
  LED_PORT_CATHODE_LOW();
  LED_PORT_ANODE_LOW();
}




uint8_t uint8_t_VarGlobal;
uint8_t DEBUGloop1, DEBUGloop2;
void Debug1Toggle(void)
{
  if ((DEBUGloop1 & 0b00000001) == 0)
  {
    GPIO.out_w1ts = (1 << DEBUG1);
  }
  else
  {
    GPIO.out_w1tc = (1 << DEBUG1);
  }
  DEBUGloop1++;
}
void Debug2Toggle(void)
{
  if ((DEBUGloop2 & 0b00000001) == 0)
  {
    GPIO.out_w1ts = (1 << DEBUG2);
  }
  else
  {
    GPIO.out_w1tc = (1 << DEBUG2);
  }
  DEBUGloop2++;
}

void Debug2Hi(void)
{
    GPIO.out_w1ts = (1 << DEBUG2);
}

void Debug2Low(void)
{
    GPIO.out_w1tc = (1 << DEBUG2);
}



uint8_t ResetVectors[16] = {0xA6, 0x81, 0x01, 0x00, 0x01, 0x03, 0x01, 0x0F,
	0x01, 0x0C, 0x01, 0x06, 0x01, 0x09, 0xA0, 0x27};




bool CPU_in_WAIT_STATE = false;



File file;
bool SD_Card_Mounted = false;


#ifndef ISR_CORE
char text_buffer[150];
#endif



uint8_t *MENU_Backup = (uint8_t *)heap_caps_malloc(640*250, MALLOC_CAP_SPIRAM);
uint8_t *MENU_BackupPage2 = (uint8_t *)heap_caps_malloc(640*250, MALLOC_CAP_SPIRAM);
#define DISK_SIZE 161280

uint8_t *RAM_Disk0 = (uint8_t *)heap_caps_malloc(161280, MALLOC_CAP_SPIRAM);
uint8_t *RAM_Disk1 = (uint8_t *)heap_caps_malloc(161280, MALLOC_CAP_SPIRAM);
uint8_t *RAM_Disk2 = (uint8_t *)heap_caps_malloc(161280, MALLOC_CAP_SPIRAM);
uint8_t *RAM_Disk3 = (uint8_t *)heap_caps_malloc(161280, MALLOC_CAP_SPIRAM);
#ifdef PSRAM_EMU
uint8_t *rom = (uint8_t *)heap_caps_malloc(32769, MALLOC_CAP_SPIRAM);
uint8_t *memory = (uint8_t *)heap_caps_malloc(65536, MALLOC_CAP_SPIRAM);
#else
uint8_t rom[32769];
uint8_t memory[65536];
#endif



SpecialFunctionStruct sf;

struct DiskAccessStruct
{
  uint8_t TrackPos;
  uint8_t DriveSelected;
  uint8_t MotorOnOff;
  uint8_t SectorPos;
  uint8_t DRIVE_COMMAND;
  uint8_t DataRegisterValue;
  uint32_t DSK_FILE_DataPTR;
  bool IsinReadProcess;
  bool IsInWriteProcess;
  bool NMI_Int_Started;
  uint8_t NMI_Delay;    //To Allow the CP400 to read the last byte from the FD502 before the NMI.
  uint16_t RW_Process_ByteRemainingCounter;
};

DiskAccessStruct DiskAccess;


DriveStruct Disk_Drive;
DiskRomSelection selectedDiskRom = DiskRomSelection::CP400;



  class cpu_t : public mc6809 
{
  public:
      uint8_t read8(uint16_t address) const 
      {
        uint8_t t_data;
        ManagePeripherals_Read(address);
      if (!sf.CP400_32K_UPPER_ENABLED)
      {
        if ((address > 0x7fff))
        {

          return rom[address - ROM_OFFSET];
        }
        else
        {
          return memory[address];
        }

      }
      else
      {
        if ((address > 0xdfff))
        {
          
          return rom[address - ROM_OFFSET];
        }
        else
        {
          return memory[address];
        }
      }

        return 0;
      }

      

      void write8(uint16_t address, uint8_t value) const 
      {
          ManagePeripherals_Write(address , value);


        if (!sf.CP400_32K_UPPER_ENABLED)
        {
          if (address >0x7fff)
          {
              
              //rom[address - 0x8000] = value;
          
          } 
          else
          {
              memory[address] = value;
          }
        }
        else
        {
              memory[address] = value;
        }



          
        }


};



bool ReadCP400DiskImage(const char* filename, uint8_t DriveNumber)
{
    if (!SD_Card_Mounted)
    {
      return false;
    }
    
    
    size_t bytesRead;
    File f = SD_MMC.open(filename, FILE_READ);
    if(!f)
    {
#ifdef DEBUG_PRINT    
        Serial.print("Failed to open file: ");
        Serial.println(filename);
#endif
        return false;
    }

    
    switch (DriveNumber)
    {
    case 0:
      bytesRead = f.read(RAM_Disk0, DISK_SIZE);
    break;
    case 1:
      bytesRead = f.read(RAM_Disk1, DISK_SIZE);
    break;
    case 2:
      bytesRead = f.read(RAM_Disk2, DISK_SIZE);
    break;
    case 3:
      bytesRead = f.read(RAM_Disk3, DISK_SIZE);
    break;
    
    default:
      break;
    }
    
    

#ifdef DEBUG_PRINT    
    Serial.print("Read ");
    Serial.print(bytesRead);
    Serial.print(" bytes into RAM_Disk ");
    Serial.println(DriveNumber);
#endif
    f.close();
    

    return true;
}

bool SaveConfigToSD(void) 
{
    if (!SD_Card_Mounted)
    {
      return false;
    }

    if (SD_MMC.exists("/config.ccc") && !SD_MMC.remove("/config.ccc"))
    {
      Serial.println("Failed to replace /config.ccc");
      return false;
    }

    File f = SD_MMC.open("/config.ccc", FILE_WRITE);
    if (!f) {
        Serial.println("Failed to open /config.ccc for writing");
        return false;
    }

    for (int i = 0; i < 4; i++) 
    {
        size_t written = f.write(Disk_Drive.Name_Disk[i], 256);
        if (written != 256) 
        {
            Serial.printf("Write failed for disk %d\n", i);
            f.close();
            return false;
        }
    }

    const uint8_t diskRomValue = static_cast<uint8_t>(selectedDiskRom);
    if (f.write(&diskRomValue, sizeof(diskRomValue)) != sizeof(diskRomValue))
    {
        Serial.println("Failed to save Disk ROM selection");
        f.close();
        return false;
    }

    const uint8_t calibrationHeader[] = {'J', 'C', 'A', 'L', 4};
    if (f.write(calibrationHeader, sizeof(calibrationHeader)) != sizeof(calibrationHeader) ||
      f.write(reinterpret_cast<const uint8_t*>(USB_DEV_CONTROL.JOYSTICK_CALIBRATION),
          sizeof(USB_DEV_CONTROL.JOYSTICK_CALIBRATION)) != sizeof(USB_DEV_CONTROL.JOYSTICK_CALIBRATION))
    {
      Serial.println("Failed to save joystick calibration");
      f.close();
      return false;
    }

    f.close();
    Serial.println("Configuration saved to SD");
    return true;
}

bool LoadConfigFromSD(void)
{
  memset(USB_DEV_CONTROL.JOYSTICK_CALIBRATION, 0,
       sizeof(USB_DEV_CONTROL.JOYSTICK_CALIBRATION));

    if (!SD_Card_Mounted)
    {
      return false;
    }

    File f = SD_MMC.open("/config.ccc", FILE_READ);
    if (!f) 
    {
        Serial.println("Failed to open /config.ccc for reading");
        return false;
    }

    for (int i = 0; i < 4; i++) 
    {
        size_t readBytes = f.read(Disk_Drive.Name_Disk[i], 256);
        if (readBytes != 256) 
        {
            Serial.printf("Read failed for disk %d\n", i);
            f.close();
            return false;
        }
    }

    uint8_t diskRomValue = static_cast<uint8_t>(DiskRomSelection::CP400);
    const size_t selectionBytes = f.read(&diskRomValue, sizeof(diskRomValue));
    if (selectionBytes == 0)
    {
        Serial.println("Legacy configuration found; using CP400 Disk ROM");
        selectedDiskRom = DiskRomSelection::CP400;
    }
    else if (diskRomValue <= static_cast<uint8_t>(DiskRomSelection::CoCo2))
    {
        selectedDiskRom = static_cast<DiskRomSelection>(diskRomValue);
    }
    else
    {
        Serial.println("Invalid Disk ROM selection; using CP400 Disk ROM");
        selectedDiskRom = DiskRomSelection::CP400;
    }

    uint8_t calibrationHeader[5];
    JoystickCalibration calibration[2];
    if (f.read(calibrationHeader, sizeof(calibrationHeader)) == sizeof(calibrationHeader) &&
      memcmp(calibrationHeader, "JCAL\x04", sizeof(calibrationHeader)) == 0 &&
      f.read(reinterpret_cast<uint8_t*>(calibration), sizeof(calibration)) == sizeof(calibration))
    {
      for (uint8_t joystick = 0; joystick < 2; joystick++)
      {
        if (calibration[joystick].valid && calibration[joystick].reportLength > 0 &&
            calibration[joystick].reportLength <= JOYSTICK_REPORT_SIZE)
        {
          USB_DEV_CONTROL.JOYSTICK_CALIBRATION[joystick] = calibration[joystick];
        }
      }
    }

    f.close();
    Serial.println("Configuration loaded from SD");
    return true;
}

bool EjectCP400Disk(uint8_t DriveNumber)
{
  if (DriveNumber >= 4)
  {
    return false;
  }

  uint8_t previousName[sizeof(Disk_Drive.Name_Disk[DriveNumber])];
  memcpy(previousName, Disk_Drive.Name_Disk[DriveNumber], sizeof(previousName));
  memset(Disk_Drive.Name_Disk[DriveNumber], 0, sizeof(Disk_Drive.Name_Disk[DriveNumber]));

  if (!SaveConfigToSD())
  {
    memcpy(Disk_Drive.Name_Disk[DriveNumber], previousName, sizeof(previousName));
    return false;
  }

  uint8_t *diskMemory = nullptr;
  switch (DriveNumber)
  {
  case 0:
    diskMemory = RAM_Disk0;
    break;
  case 1:
    diskMemory = RAM_Disk1;
    break;
  case 2:
    diskMemory = RAM_Disk2;
    break;
  case 3:
    diskMemory = RAM_Disk3;
    break;
  }

  if (diskMemory != nullptr)
  {
    memset(diskMemory, 0xFF, DISK_SIZE);
  }

  if (DiskAccess.DriveSelected == DriveNumber)
  {
    DiskAccess.IsinReadProcess = false;
    DiskAccess.IsInWriteProcess = false;
    DiskAccess.RW_Process_ByteRemainingCounter = 0;
    sf.PHYSICAL_Drive_Must_Be_Saved = false;
  }

  return true;
}


bool WriteCP400DiskImage(const char* filename, uint8_t DriveNumber)
{
  //return true;
  if (!SD_Card_Mounted)
  {
    return false;
  }

  size_t bytesWritten = 0;
    
    
    File f = SD_MMC.open(filename, FILE_WRITE);
    if(!f)
    {
#ifdef DEBUG_PRINT    
      Serial.print("Failed to open file for writing: ");
      Serial.println(filename);
#endif
      return false;
    }

    switch(DriveNumber)
    {
        case 0:
            bytesWritten = f.write(RAM_Disk0, DISK_SIZE);
        break;

        case 1:
            bytesWritten = f.write(RAM_Disk1, DISK_SIZE);
        break;

        case 2:
            bytesWritten = f.write(RAM_Disk2, DISK_SIZE);
        break;

        case 3:
        bytesWritten = f.write(RAM_Disk3, DISK_SIZE);
        break;

        default:
#ifdef DEBUG_PRINT    
        Serial.println("Invalid DriveNumber");
#endif
        f.close();
        return false;
    }

    f.flush();     // force physical write
    f.close();

#ifdef DEBUG_PRINT    
    Serial.print("Written ");
    Serial.print(bytesWritten);
    Serial.print(" bytes from RAM_Disk ");
    Serial.println(DriveNumber);
#endif
    return (bytesWritten == DISK_SIZE);
}




cpu_t cpu;

void DoCPU(void)
{
  //Debug1Toggle();
  
  if (CPU_in_WAIT_STATE)
  {
    if (sf.CPU_HALTED_BY_EMULATOR) //If Emulator is in Menus, 
    {
      return;
    }
    
    if (gpio_get_level(VSYNC_PORT) != 0)  //Manage CWAI instruction
    {
      return;
    }
    else
    {
      CPU_in_WAIT_STATE = false;
    }
  }
  //Debug1Toggle();
  
  cpu.execute();
  cpu.execute();
  if (sf.CPU_Speed == CPU_FAST)
  {
    cpu.execute();
    cpu.execute();
  }

  return;
}

void CopyDiskToRamDisk(void)
{
  if (!SD_Card_Mounted)
  {
    return;
  }

  ReadCP400DiskImage((const char*)Disk_Drive.Name_Disk[0], 0);
  ReadCP400DiskImage((const char*)Disk_Drive.Name_Disk[1], 1);
  ReadCP400DiskImage((const char*)Disk_Drive.Name_Disk[2], 2);
  ReadCP400DiskImage((const char*)Disk_Drive.Name_Disk[3], 3);
  
}



  const PinConfig pins(-1,-1,-1,5,4,  -1,-1,-1,-1,7,6,  -1,-1,-1,9,8,  2,1);
//                             B B X       G  G  X         R  R
    //6 bit mode for Coco 3    0 1 X       0  1  X         0  1

VGA* vga;
GfxWrapper<VGA> *gfx;


Mode mode0 = Mode::MODE_320x240x60;
Mode mode1 = Mode::MODE_320x240x60_4_3;
Mode mode2 = Mode::MODE_640x240x60;
Mode mode3 = Mode::MODE_640x240x60_4_3;

constexpr uint16_t VIDEO_ACTIVE_WIDTH = 256;
constexpr uint16_t VIDEO_ACTIVE_HEIGHT = 192;
uint16_t VideoViewportX = (mode1.hRes - VIDEO_ACTIVE_WIDTH) / 2;
uint16_t VideoViewportY = (mode1.vRes - VIDEO_ACTIVE_HEIGHT) / 2;

#define VIDEO_GRAPHICS_X_OFFSET VideoViewportX
#define VIDEO_Y_OFFSET VideoViewportY

void SetVideoViewport(const Mode &mode)
{
  sf.VideoEmulatorXpixels = mode.hRes;
  VideoViewportX = mode.hRes > VIDEO_ACTIVE_WIDTH
                     ? (mode.hRes - VIDEO_ACTIVE_WIDTH) / 2
                     : 0;
  VideoViewportY = mode.vRes > VIDEO_ACTIVE_HEIGHT
                     ? (mode.vRes - VIDEO_ACTIVE_HEIGHT) / 2
                     : 0;
}



uint8_t SCAN_Keyboard_Matrix[8][7];


uint8_t ReadCP400Buttons(void)
{
  uint8_t val1, val2;
  val1 = USB_DEV_CONTROL.JOY1_BUTT1 & USB_DEV_CONTROL.JOY1_BUTT2;
  val2 = (USB_DEV_CONTROL.JOY2_BUTT1 & USB_DEV_CONTROL.JOY2_BUTT2) << 1;

  return val1 | val2;
  
}

void InitSD_Card1(void)
{
  SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);
  if (!SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_DEFAULT, 5)) 
  {
    Serial.println("Card Mount Failed");
    return;
  }

 file = SD_MMC.open("/GAMES01.DSK", FILE_READ);
  if (!file)
  {
    Serial.println("Failed to open /GAMES01.DSK");
    return;
  }
  else
  {
    Serial.println("File Correct");
  }
  while(1)
  {

  }
}
extern void flashFromSD(const char* filename);
void CheckFirmwareUpdate(void)
{
  if (!SD_Card_Mounted)
  {
    return;
  }

  debugln("Before flash");
  flashFromSD("/qprcx.rty");  //Random name for flash file (Created from the Flash menu).  Will be ereased after flash.
}


void InitSD_Card(void)
{

  SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);
  
  if (!SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_DEFAULT, 5)) {
    SD_Card_Mounted = false;
    debugln("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD_MMC.cardType();
  if(cardType == CARD_NONE){
    SD_Card_Mounted = false;
    debugln("No SD_MMC card attached");
    return;
  }

  SD_Card_Mounted = true;

  Serial.print("SD_MMC Card Type: ");
  if(cardType == CARD_MMC){
    debugln("MMC");
  } else if(cardType == CARD_SD){
    debugln("SDSC");
  } else if(cardType == CARD_SDHC){
    debugln("SDHC");
  } else {
    debugln("UNKNOWN");
  }

}

#define BOARD_CASSETTE_OUT 13
#define BOARD_CASSETTE_IN 14
#define BOARD_CASSETTE_RELAY 10

#define ROM_OFFSET 0x8000
#define RAM_MODE 0x0000
void InitPeripherals_and_Others(void)
{
  

  for(uint32_t i = 0; i !=65536; i++)
  {
      memory[i] = 255;
  }
  
  Serial.begin(115200);
  delay(10);
  
  InitSD_Card();
  
  CheckFirmwareUpdate();
  
  
  InitDisks();

  cpu.assign_nmi_line(&sf.nmi_pin);
  cpu.assign_firq_line(&sf.firq_pin);
  cpu.assign_irq_line(&sf.irq_pin);

  
  //------Special registers------
  sf.CPU_HALTED_BY_EMULATOR = false;
  sf.DIRECT_Key_Code = 0;  //Nothing
  sf.PHYSICAL_Drive_Must_Be_Saved = false;
  sf.V_Synch_Int_Enabled = false;
  sf.nmi_pin = true;  //True = disabled
  sf.firq_pin = true; //True = disabled
  sf.irq_pin = true; //True = disabled
  sf.V_Synch = false;
  sf.CP400_32K_UPPER_ENABLED = false;
  sf.ROM_Offset = ROM_OFFSET;
  sf.AnyKeypress = false; //Used for Keyboard Scan.
  
  sf.CPU_Speed = CPU_SLOW;
  sf.CP400VideoGenMODE = 255;    //To reset in first execution
  sf.CP400VideoPageOffset_Registers = 0b00000010; //Init to be at address 400 (Even if Basic set it at boot)
  sf.CP400ColorMode = 0;
  sf.VideoEmulatorXpixels = 320;
  sf.Artefact = true; //Artefact mode by default.
  sf.CP400GraphicMode = 0;

  sf.is_JOY1_B1_WasPressed = false;
  sf.is_JOY1_B2_WasPressed = false;
  sf.is_JOY2_B1_WasPressed = false;
  sf.is_JOY2_B2_WasPressed = false;
  sf.is_LastKeyboardScanned = false;
  sf.JoystickDebug = false;
//------------Init of Disk Registers------

  DiskAccess.IsinReadProcess = false;
  DiskAccess.IsInWriteProcess = false;
  DiskAccess.NMI_Int_Started = false;
  DiskAccess.NMI_Delay = 0;

  InitPorts();

  
  
  CopyCP400ROMS();
  //CopyCoCo3ROMS();

  //The ROM copy leaves the whole I/O page reading back as 0xFF, which would look
  //like an audio mux that is already open. A 6821 clears its control registers
  //on reset, so start with the mux closed and let the ROM open it.
  rom[ROM_FF23] = 0;
	
  CopyDiskToRamDisk();
  
  vga = new VGA();
  gfx = new GfxWrapper<VGA>(*vga, mode1.hRes, mode1.vRes);
  vga->bufferCount = 2;
	if(!vga->init(pins, mode1, 8)) while(1) delay(1);
	vga->start();

  SetVideoMode(VIDEO_MODE_320X240_4_3);
  for (int y = 0; y < mode1.vRes; y++)
  {
    for (int x = 0; x < mode1.hRes; x++)
    {
      if (x < mode1.hRes / 4)
      {
        vga->dot(x, y, vga->rgb(255, 255, 255));
      }
      else if (x < mode1.hRes / 2)
      {
        vga->dot(x, y, vga->rgb(255, 0, 0));
      }
      else if (x < (mode1.hRes * 3) / 4)
      {
        vga->dot(x, y, vga->rgb(0, 255, 0));
      }
      else
      {
        vga->dot(x, y, vga->rgb(0, 0, 255));
      }
    }
  }
  vga->show();
  //SetVideoMode(MODE_320x240x60_4_3);
  //SetVideoMode(VIDEO_MODE_320X240_16_9);
  //SetVideoMode(VIDEO_MODE_640X240_16_9);
  //SetVideoMode(VIDEO_MODE_640X240_4_3);

}

static volatile uint8_t g_AudioDuty = 0;
static volatile uint32_t g_AudioLastWriteMs = 0;

void AudioWriteSample(uint8_t value)
{
  g_AudioDuty = value;
  g_AudioLastWriteMs = millis();
  ledcWrite(AUDIO_CHANNEL, value);
}

void AudioSilence(void)
{
  g_AudioDuty = 0;
  ledcWrite(AUDIO_CHANNEL, 0);
}

//Fades the output out once the machine stops feeding the DAC, so the pins stop
//switching instead of holding a carrier that has nothing left to carry.
void AudioIdleCore(void *pvParameters)
{
  while (true)
  {
    if ((millis() - g_AudioLastWriteMs) >= AUDIO_IDLE_TIMEOUT_MS)
    {
      uint8_t duty = g_AudioDuty;

      if (duty != 0)
      {
        duty = (duty > AUDIO_FADE_STEP) ? (uint8_t)(duty - AUDIO_FADE_STEP) : 0;
        g_AudioDuty = duty;
        ledcWrite(AUDIO_CHANNEL, duty);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(AUDIO_FADE_TICK_MS));
  }
}

bool IsAudioMuxEnabled(void)
{
  //PIA1 CRB drives CB2, which is the CP400 audio mux enable. Bit 5 makes CB2 an
  //output and bit 3 is the level it drives. BASIC keeps the mux closed while it
  //is idle, so honouring it is what keeps FF20 traffic that is not music (RS232
  //framing, DDR setup, joystick comparator sweeps) out of the speaker.
  return (rom[ROM_FF23] & 0b00101000) == 0b00101000;
}

void InitPorts(void)
{
  // PWM Sound Configuration.
  ledcSetup(AUDIO_CHANNEL, AUDIO_PWM_FREQUENCY, AUDIO_PWM_RESOLUTION);
  ledcAttachPin(AUDIO_PIN, AUDIO_CHANNEL);
  AudioSilence();

  //A WS2812 latches the last colour it was given and holds it until it is given
  //a new one, so the LED has to be told to switch off rather than just left
  //alone. Doing it once here keeps it dark for the rest of the session.
  neopixelWrite(BOARD_RGB_LED_PIN, 0, 0, 0);
  pinMode(BOARD_RGB_LED_PIN, INPUT);


  //analogReadResolution(8);
  //int initval = analogRead(BOARD_CASSETTE_IN);
  //adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_0); // 0-1.1V
  
  //pinMode(BOARD_CASSETTE_OUT,OUTPUT);
  //pinMode(BOARD_CASSETTE_RELAY, OUTPUT);
  //digitalWrite(BOARD_CASSETTE_RELAY, LOW);  //Disable Relay.

  return;
}
void SetVideoMode(uint8_t VideoMode)
{
  vga->stop();
  switch (VideoMode)
  {
  case VIDEO_MODE_320X240_16_9:
    SetVideoViewport(mode0);
    if(!vga->Reinit(pins, mode0, 8)) while(1) delay(1);
    break;
    case VIDEO_MODE_320X240_4_3:
      SetVideoViewport(mode1);
      if(!vga->Reinit(pins, mode1, 8)) while(1) delay(1);
    break;
    case VIDEO_MODE_640X240_16_9:
      SetVideoViewport(mode2);
      if(!vga->Reinit(pins, mode2, 8)) while(1) delay(1);
    break;
    case VIDEO_MODE_640X240_4_3:
      SetVideoViewport(mode3);
      if(!vga->Reinit(pins, mode3, 8)) while(1) delay(1);
    break;
  
  default:
    break;
  }
  vga->start();

}



const int Field_Synch_Interrupt_PIN = VSYNC_PORT;  // VSynch pin


void IRAM_ATTR Field_Synch_Interrupt_Flag() 
{
  /*
    This interrupt is tied to the Vertical Pin of the ESP32
    and is used to update the Field interrupt flag of FF03 bit 7.
    A read of FF02 reset this flag (1 = reset 0 = set)
  
    */

  rom[ROM_FF03] |= 0b10000000;  //Set bit 7

  if (sf.V_Synch_Int_Enabled)
  {
    sf.irq_pin = false;  //Interrupt enabled.
  }
  sf.V_Synch = true;
  //Debug2Toggle();

}


/*

---------------------------------
FF22:
Resolution Selection
F8 = Pmode 4 256x192 2 colors 11111XXX
E8 = Pmode 3 128x192 4 colors 11101XXX
D8 = pmode 2 128x192 2 colors 11011XXX
C8 = pmode 1 128x96  4 colors 11001XXX
B8 = pmode 0 128x96  2 colors 10111XXX

--SAM CONTROL REGISTERS:




76543210
||||||||_RS232 DATA INPUT
|||||||__SINGLE BIT SOUND OUTPUT
||||||___RAM SIZE INPUT
|||||____VDG CSS -Color set (0 = white, 1 = green)
||||_____VDG GM0
|||______VDG GM1
||_______VDG GM2
|________VDG _A/G

SCREEN X,X 
       | |_____COLOR SET     0 = COLOR SET 1  1 = COLOR SET 2
       |_______DISPLAY MODE  0 = TEXT 1 = GRAPHIC


Mode VDG Settings SAM
                      A/G GM2 GM1 GM0 V2/V1/V0    Desc.   RAM used x,y,clrs in hex(dec)
Internal alphanumeric  0   X   X   0   0  0  0    32x16 ( 5x7 pixel ch)
External alphanumeric  0   X   X   1   0  0  0    32x16 (8x12 pixel ch)
Semigraphic-4          0   X   X   0   0  0  0    32x16 ch, 64x32 pixels
Semigraphic-6          0   X   X   1   0  0  0    64x48 pixels
Full graphic 1-C       1   0   0   0   0  0  1    64x64x4 $400(1024)
Full graphic 1-R       1   0   0   1   0  0  1   128x64x2 $400(1024)
Full graphic 2-C       1   0   1   0   0  1  0   128x64x4 $800(2048)
Full graphic 2-R       1   0   1   1   0  1  1   128x96x2 $600(1536)
Full graphic 3-C       1   1   0   0   1  0  0   128x96x4 $C00(3072)
Full graphic 3-R       1   1   0   1   1  0  1   128x192x2 $C00(3072)
Full graphic 6-C       1   1   1   0   1  1  0   128x192x4 $1800(6144)
Full graphic 6-R       1   1   1   1   1  1  0   256x192x2 $1800(6144)
Direct memory access   X   X   X   X   1  1  1

- The graphic modes with -C are 4 color, -R is 2 color.
- 2 color mode - 8 pixels per byte (each bit denotes on/off)
4 color mode - 4 pixels per byte (each 2 bits denotes color)
- CSS (in FF22) is the color select bit:
Color set 0: 0 = black, 1 = green for -R modes
00 = green, 01 = yellow for -C modes
10 = blue, 11 = red for -C modes
Color set 1: 0 = black, 1 = buff for -R modes


*/

bool IsButtonPressed(void)
{
  if (digitalRead(21) == true)
  {
    return false;
  }
  else
  {
    return true;
  }
}
uint8_t debugloop = 0;

hw_timer_t* cpuTimer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

uint32_t testloop1 = 0;

void IRAM_ATTR onCPUTimer() 
{
  
  Debug1Toggle();
  cpu.execute(); 
  cpu.execute(); 
  testloop1++;
  if (testloop1 == 20000)
  {
    testloop1 = 0;
  }
  
  
}

void setTimerInterval(uint32_t newInterval) //To set Slow/Fast CPU speed
{
  // Disable actual alarm
  timerAlarmDisable(cpuTimer);
  
  // Change timer interval
  timerAlarmWrite(cpuTimer, newInterval, true);  // newInterval est le nombre de ticks
  
  // Réactiver l'alarme
  timerAlarmEnable(cpuTimer);
}

static bool JoystickByteMatches(uint8_t currentValue, uint8_t neutralValue,
                                uint8_t controlValue, uint8_t compareBits)
{
  const int delta = static_cast<int>(controlValue) - static_cast<int>(neutralValue);

  //A large move is an analog axis, so accept anything past the halfway point.
  if (delta >= 32 || delta <= -32)
  {
    const int threshold = static_cast<int>(neutralValue) + delta / 2;
    return delta > 0 ? static_cast<int>(currentValue) >= threshold
                     : static_cast<int>(currentValue) <= threshold;
  }

  //An empty mask would match every value, so compare the whole byte instead.
  if (compareBits == 0)
  {
    return currentValue == controlValue;
  }

  return ((currentValue ^ controlValue) & compareBits) == 0;
}

bool IsJoystickControlActive(uint8_t joystick, JoystickControl control)
{
  const JoystickCalibration &calibration = USB_DEV_CONTROL.JOYSTICK_CALIBRATION[joystick];
  if (!calibration.valid ||
      USB_DEV_CONTROL.JOYSTICK_REPORT_LENGTH[joystick] != calibration.reportLength)
  {
    return false;
  }

  const uint8_t *current = USB_DEV_CONTROL.JOYSTICK_REPORT[joystick];
  const bool isDirection = control <= JOYSTICK_RIGHT;
  bool checkedAnyByte = false;

  for (uint8_t index = 0; index < calibration.reportLength; index++)
  {
    const uint8_t changedBits = calibration.controlBits[control][index];
    if (!calibration.stable[index] || changedBits == 0)
    {
      continue;
    }

    //Directions share one mask so a hat value cannot also match its neighbours.
    const uint8_t compareBits = isDirection ? calibration.directionBits[index] : changedBits;
    if (!JoystickByteMatches(current[index], calibration.neutral[index],
                             calibration.control[control][index], compareBits))
    {
      return false;
    }
    checkedAnyByte = true;
  }

  return checkedAnyByte;
}

void PrintJoystickCalibration(uint8_t joystick)
{
  const JoystickCalibration &calibration = USB_DEV_CONTROL.JOYSTICK_CALIBRATION[joystick];
  if (!calibration.valid)
  {
    printf("JOY%u calibration: none\n", joystick + 1);
    return;
  }

  static const char *controlNames[JOYSTICK_CONTROL_COUNT] =
  {
    "UP", "DOWN", "LEFT", "RIGHT", "BTN1", "BTN2"
  };

  printf("JOY%u calibration len=%u\n", joystick + 1, calibration.reportLength);
  printf("  neutral :");
  for (uint8_t index = 0; index < calibration.reportLength; index++)
  {
    printf(" %02X", calibration.neutral[index]);
  }
  printf("\n  stable  :");
  for (uint8_t index = 0; index < calibration.reportLength; index++)
  {
    printf(" %02X", calibration.stable[index]);
  }
  printf("\n");

  for (uint8_t control = 0; control < JOYSTICK_CONTROL_COUNT; control++)
  {
    printf("  %-5s v:", controlNames[control]);
    for (uint8_t index = 0; index < calibration.reportLength; index++)
    {
      printf(" %02X", calibration.control[control][index]);
    }
    printf("  b:");
    for (uint8_t index = 0; index < calibration.reportLength; index++)
    {
      printf(" %02X", calibration.controlBits[control][index]);
    }
    printf("\n");
  }

  printf("  dirbits :");
  for (uint8_t index = 0; index < calibration.reportLength; index++)
  {
    printf(" %02X", calibration.directionBits[index]);
  }
  printf("\n");
}

uint8_t ReadJoysticks(uint8_t JoyNum)
{
  switch (JoyNum)
  {
  case JOY2_X_SELECT:
    return USB_DEV_CONTROL.JOY2_X_AXIS;
  case JOY1_X_SELECT:
    return USB_DEV_CONTROL.JOY1_X_AXIS;
  case JOY2_Y_SELECT:
    return USB_DEV_CONTROL.JOY2_Y_AXIS;
  case JOY1_Y_SELECT:
    return USB_DEV_CONTROL.JOY1_Y_AXIS;
  default:
    return 255;
  }
}


void PrintJoystickState(uint8_t joystick);

//Runs on the free core so the video task cannot starve it and Serial cannot stall USB.
void JoystickDebugCore(void *pvParameters)
{
  static uint8_t lastReport[2][JOYSTICK_REPORT_SIZE];
  static uint8_t lastLength[2] = {255, 255};

  while (true)
  {
    if (sf.JoystickDebug)
    {
      for (uint8_t joystick = 0; joystick < 2; joystick++)
      {
        const uint8_t reportLength = USB_DEV_CONTROL.JOYSTICK_REPORT_LENGTH[joystick];
        if (reportLength == 0)
        {
          continue;
        }

        if (reportLength == lastLength[joystick] &&
            memcmp(lastReport[joystick], USB_DEV_CONTROL.JOYSTICK_REPORT[joystick],
                   reportLength) == 0)
        {
          continue;
        }

        memcpy(lastReport[joystick], USB_DEV_CONTROL.JOYSTICK_REPORT[joystick],
               JOYSTICK_REPORT_SIZE);
        lastLength[joystick] = reportLength;
        PrintJoystickState(joystick);
      }
    }

    vTaskDelay(10);
  }
}

//initial setup


void setup()
{
  
  
  pinMode(DEBUG1,OUTPUT);
  pinMode(DEBUG2,OUTPUT);
  pinMode(LED_PORT_ANODE,OUTPUT);
  pinMode(LED_PORT_CATHODE,OUTPUT);




  InitPeripherals_and_Others();

  DISKETTE_LED_OFF();

#define SERIAL1_TX 41
#define SERIAL1_RX 42
#define SERIAL1_BAUD 115200

  Serial1.begin(SERIAL1_BAUD, SERIAL_8N1, SERIAL1_RX, SERIAL1_TX);



  cpu.reset();

  Setup_USB();

  PrintJoystickCalibration(0);
  PrintJoystickCalibration(1);


  xTaskCreatePinnedToCore
  (
    VideoCore,        // Fonction
    "SystemCore",      // Name
    6048,              // stack
    NULL,              // Parameter
    2,                 // Priority
    NULL,              // Handle 
    1                  // CPU 0 or 1
  );

  xTaskCreatePinnedToCore(JoystickDebugCore, "JoyDebug", 4096, NULL, 1, NULL, 0);

  xTaskCreatePinnedToCore(AudioIdleCore, "AudioIdle", 2048, NULL, 1, NULL, 0);

}


void loop()
{
  //Nothig to do here, all the work is in ISR and RTOS
}


#define CP400_GRAPH_MODE_32X16_8X12 0b0 //Text 32x16
#define CP400_GRAPH_MODE_256X192X2 0b11110110  //PMODE 4
#define CP400_GRAPH_MODE_128X192X4 0b11100110  //PMODE 3  OK


#define CP400_GRAPH_MODE_128X192X2 0b11010101  //PMODE 2
#define CP400_GRAPH_MODE_128X96X4 0b11000100  //PMODE 1
#define CP400_GRAPH_MODE_128X96X4_1 0b11110100  //PMODE 1 Other mode??


#define CP400_GRAPH_MODE_128X96X2 0b10110011  //PMODE 0


//---------Other modes in assembly language only, not officialy supported in Basic CP400
#define CP400_GRAPH_MODE_128X64X4 0b10100010  //
#define CP400_GRAPH_MODE_128X64X2 0b10010001  //

#define CP400_GRAPH_MODE_64X64X4 0b10000001  //


#ifdef DEBUG_ALL
#define WAIT_VSYNCH_LOW()  while(gpio_get_level(VSYNC_PORT) != 0) {DEBUG1_SET;}DEBUG1_CLR
#else
#define WAIT_VSYNCH_LOW()  while(gpio_get_level(VSYNC_PORT) != 0) {}
#endif
void IRAM_ATTR VideoCore(void *pvParameters) 
{
  uint8_t ModeValue;
  while (true) 
  {
    //vTaskDelay(1);
    if (sf.DIRECT_Key_Code == MENU_F12) //Emulator menu entrance
    {
      
      CPU_in_WAIT_STATE = true;
      sf.CPU_HALTED_BY_EMULATOR = true;
      vTaskDelay(1);

      EMULATOR_Menu();
      CPU_in_WAIT_STATE = false;
      sf.CPU_HALTED_BY_EMULATOR = false;
      vTaskDelay(200);
    }
    



    ModeValue = sf.CP400GraphicMode & 0b11110111;    //Remove the Color bit
    
    switch (ModeValue)
    {
    
      case CP400_GRAPH_MODE_32X16_8X12:
        RenderCP400GraphMode_32X16_8X12();
      break;

      case CP400_GRAPH_MODE_256X192X2:
        if (sf.Artefact)
        {
          RenderCP400GraphMode_256X192X2_Artefact();
        }
        else
        {
          RenderCP400GraphMode_256X192X2();
        }
      break;

      case CP400_GRAPH_MODE_128X192X4:
        RenderCP400GraphMode_128X192X4();
      break;

      case CP400_GRAPH_MODE_128X192X2:
        RenderCP400GraphMode_128X192X2();
      break;

      case CP400_GRAPH_MODE_128X96X4:
      case CP400_GRAPH_MODE_128X96X4_1:
      RenderCP400GraphMode_128X96X4();
      break;

      case CP400_GRAPH_MODE_128X96X2:
        RenderCP400GraphMode_128X96X2();
      break;

      case CP400_GRAPH_MODE_128X64X4:
        RenderCP400GraphMode_128X64X4();
      break;

      case CP400_GRAPH_MODE_128X64X2:
        RenderCP400GraphMode_128X64X2();
    break;

    case CP400_GRAPH_MODE_64X64X4:
      RenderCP400GraphMode_64X64X4();
break;

      
      
      default:
      break;
    }

    
    
    /*
    while (!sf.V_Synch) //While no interrupt
    {
      //vTaskDelay(1);
      NOP();
    }*/


    WAIT_VSYNCH_LOW();
    Field_Synch_Interrupt_Flag();
  
    vga->show();
    
    if (sf.PHYSICAL_Drive_Must_Be_Saved)
    {
      DISKETTE_LED_ON();
      WriteCP400DiskImage((const char*)Disk_Drive.Name_Disk[DiskAccess.DriveSelected],DiskAccess.DriveSelected);
      DISKETTE_LED_OFF();
      sf.PHYSICAL_Drive_Must_Be_Saved = false;
    }

    FillKeyboardMatrix();
  }
}


void RenderCP400GraphMode_64X64X4(void)
{


  if (sf.CP400VideoGenMODE != CP400_GRAPH_MODE_64X64X4)
  {
    vga->clear(0b11111111);
    vga->show();
    vga->clear(0b11111111);
    vga->show();

    sf.CP400VideoGenMODE = CP400_GRAPH_MODE_64X64X4;
  }




  
  uint32_t MemLoop = sf.CP400VideoPageOffset_Registers * CP400_GRAPH_OFFSET_LEN;
  uint16_t Xloop, Yloop, Yloop1;
  uint8_t Dot1bit[8];
  uint8_t Color[4];
  
  if ((sf.CP400GraphicMode & 0b00001000) == 0)
  {
    Color[0] = RGB332ToVGAPacked(0b00011100);  //Green
    Color[1] = RGB332ToVGAPacked(0b11111100);  //Yellow
    Color[2] = RGB332ToVGAPacked(0b00000011);  //Blue
    Color[3] = RGB332ToVGAPacked(0b11100000);  //Red
  }
  else
  {
    Color[0] = RGB332ToVGAPacked(0xff); //blanc
    Color[1] = RGB332ToVGAPacked(0b01011011);  //Turquoise
    Color[2] = RGB332ToVGAPacked(0b11100011);  //lilas
    Color[3] = RGB332ToVGAPacked(0b11101000);  //orange(buff)
  }  

  for (Yloop = VIDEO_Y_OFFSET; Yloop < VIDEO_Y_OFFSET + VIDEO_ACTIVE_HEIGHT; Yloop+=3)
  {
        for (Xloop = VIDEO_GRAPHICS_X_OFFSET; Xloop < VIDEO_GRAPHICS_X_OFFSET + 256; Xloop+=16)
    {

      Dot1bit[3] = (memory[MemLoop] >> 6) & 0b11;  // Bits 6-7
      Dot1bit[2] = (memory[MemLoop] >> 4) & 0b11;  // Bits 4-5
      Dot1bit[1] = (memory[MemLoop] >> 2) & 0b11;  // Bits 2-3
      Dot1bit[0] = (memory[MemLoop++] >> 0) & 0b11;  // Bits 0-1

      //Line one
      vga->dot(Xloop,Yloop, Color[Dot1bit[3]]);
      vga->dot(Xloop+1,Yloop, Color[Dot1bit[3]]);
      vga->dot(Xloop+2,Yloop, Color[Dot1bit[3]]);
      vga->dot(Xloop+3,Yloop, Color[Dot1bit[3]]);

      vga->dot(Xloop+4,Yloop, Color[Dot1bit[2]]);
      vga->dot(Xloop+5,Yloop, Color[Dot1bit[2]]);
      vga->dot(Xloop+6,Yloop, Color[Dot1bit[2]]);
      vga->dot(Xloop+7,Yloop, Color[Dot1bit[2]]);

      vga->dot(Xloop+8,Yloop, Color[Dot1bit[1]]);
      vga->dot(Xloop+9,Yloop, Color[Dot1bit[1]]);
      vga->dot(Xloop+10,Yloop, Color[Dot1bit[1]]);
      vga->dot(Xloop+11,Yloop, Color[Dot1bit[1]]);

      vga->dot(Xloop+12,Yloop, Color[Dot1bit[0]]);
      vga->dot(Xloop+13,Yloop, Color[Dot1bit[0]]);
      vga->dot(Xloop+14,Yloop, Color[Dot1bit[0]]);
      vga->dot(Xloop+15,Yloop, Color[Dot1bit[0]]);

      Yloop1 = Yloop+1;
      //Line 2

      vga->dot(Xloop,Yloop1, Color[Dot1bit[3]]);
      vga->dot(Xloop+1,Yloop1, Color[Dot1bit[3]]);
      vga->dot(Xloop+2,Yloop1, Color[Dot1bit[3]]);
      vga->dot(Xloop+3,Yloop1, Color[Dot1bit[3]]);

      vga->dot(Xloop+4,Yloop1, Color[Dot1bit[2]]);
      vga->dot(Xloop+5,Yloop1, Color[Dot1bit[2]]);
      vga->dot(Xloop+6,Yloop1, Color[Dot1bit[2]]);
      vga->dot(Xloop+7,Yloop1, Color[Dot1bit[2]]);

      vga->dot(Xloop+8,Yloop1, Color[Dot1bit[1]]);
      vga->dot(Xloop+9,Yloop1, Color[Dot1bit[1]]);
      vga->dot(Xloop+10,Yloop1, Color[Dot1bit[1]]);
      vga->dot(Xloop+11,Yloop1, Color[Dot1bit[1]]);

      vga->dot(Xloop+12,Yloop1, Color[Dot1bit[0]]);
      vga->dot(Xloop+13,Yloop1, Color[Dot1bit[0]]);
      vga->dot(Xloop+14,Yloop1, Color[Dot1bit[0]]);
      vga->dot(Xloop+15,Yloop1, Color[Dot1bit[0]]);
      //Line 3
      Yloop1++;

      vga->dot(Xloop,Yloop1, Color[Dot1bit[3]]);
      vga->dot(Xloop+1,Yloop1, Color[Dot1bit[3]]);
      vga->dot(Xloop+2,Yloop1, Color[Dot1bit[3]]);
      vga->dot(Xloop+3,Yloop1, Color[Dot1bit[3]]);

      vga->dot(Xloop+4,Yloop1, Color[Dot1bit[2]]);
      vga->dot(Xloop+5,Yloop1, Color[Dot1bit[2]]);
      vga->dot(Xloop+6,Yloop1, Color[Dot1bit[2]]);
      vga->dot(Xloop+7,Yloop1, Color[Dot1bit[2]]);

      vga->dot(Xloop+8,Yloop1, Color[Dot1bit[1]]);
      vga->dot(Xloop+9,Yloop1, Color[Dot1bit[1]]);
      vga->dot(Xloop+10,Yloop1, Color[Dot1bit[1]]);
      vga->dot(Xloop+11,Yloop1, Color[Dot1bit[1]]);

      vga->dot(Xloop+12,Yloop1, Color[Dot1bit[0]]);
      vga->dot(Xloop+13,Yloop1, Color[Dot1bit[0]]);
      vga->dot(Xloop+14,Yloop1, Color[Dot1bit[0]]);
      vga->dot(Xloop+15,Yloop1, Color[Dot1bit[0]]);

      
    }
  }

}



void RenderCP400GraphMode_128X64X2(void)
{
  
  
  if (sf.CP400VideoGenMODE != CP400_GRAPH_MODE_128X64X2)
  {
    vga->clear(0b11111111);
    vga->show();
    vga->clear(0b11111111);
    vga->show();

    sf.CP400VideoGenMODE = CP400_GRAPH_MODE_128X64X2;
  }


  

  uint32_t MemLoop = sf.CP400VideoPageOffset_Registers * CP400_GRAPH_OFFSET_LEN;
  uint16_t Xloop, Yloop, Yloop1;
  uint8_t Dot1bit[8];
  uint8_t Color[2];

  if ((sf.CP400GraphicMode & 0b00001000) == 0)
  {
    Color[0] = RGB332ToVGAPacked(0b00000000);  //black
    Color[1] = RGB332ToVGAPacked(0b00011100); //green
  }
  else
  {
    Color[0] = RGB332ToVGAPacked(0x00);
    Color[1] = RGB332ToVGAPacked(0xff);
  }  

  for (Yloop = VIDEO_Y_OFFSET; Yloop < VIDEO_Y_OFFSET + VIDEO_ACTIVE_HEIGHT; Yloop+=3)
  {
    for (Xloop = VIDEO_GRAPHICS_X_OFFSET; Xloop < VIDEO_GRAPHICS_X_OFFSET + 256; Xloop+=16)
    {
      Dot1bit[7] = ((memory[MemLoop]>>7) & 0b00000001);
      Dot1bit[6] = ((memory[MemLoop]>>6) & 0b00000001);
      Dot1bit[5] = ((memory[MemLoop]>>5) & 0b00000001);
      Dot1bit[4] = ((memory[MemLoop]>>4) & 0b00000001);
      Dot1bit[3] = ((memory[MemLoop]>>3) & 0b00000001);
      Dot1bit[2] = ((memory[MemLoop]>>2) & 0b00000001);
      Dot1bit[1] = ((memory[MemLoop]>>1) & 0b00000001);
      Dot1bit[0] = (memory[MemLoop++] & 0b00000001);
      
      vga->dot(Xloop,Yloop, Color[Dot1bit[7]]);
      vga->dot(Xloop+1,Yloop, Color[Dot1bit[7]]);

      vga->dot(Xloop+2,Yloop, Color[Dot1bit[6]]);
      vga->dot(Xloop+3,Yloop, Color[Dot1bit[6]]);

      vga->dot(Xloop+4,Yloop, Color[Dot1bit[5]]);
      vga->dot(Xloop+5,Yloop, Color[Dot1bit[5]]);

      vga->dot(Xloop+6,Yloop, Color[Dot1bit[4]]);
      vga->dot(Xloop+7,Yloop, Color[Dot1bit[4]]);

      vga->dot(Xloop+8,Yloop, Color[Dot1bit[3]]);
      vga->dot(Xloop+9,Yloop, Color[Dot1bit[3]]);

      vga->dot(Xloop+10,Yloop, Color[Dot1bit[2]]);
      vga->dot(Xloop+11,Yloop, Color[Dot1bit[2]]);

      vga->dot(Xloop+12,Yloop, Color[Dot1bit[1]]);
      vga->dot(Xloop+13,Yloop, Color[Dot1bit[1]]);

      vga->dot(Xloop+14,Yloop, Color[Dot1bit[0]]);
      vga->dot(Xloop+15,Yloop, Color[Dot1bit[0]]);
      
      //-------------------Second Line----------------------
      Yloop1 = Yloop+1;
      vga->dot(Xloop,Yloop1, Color[Dot1bit[7]]);
      vga->dot(Xloop+1,Yloop1, Color[Dot1bit[7]]);

      vga->dot(Xloop+2,Yloop1, Color[Dot1bit[6]]);
      vga->dot(Xloop+3,Yloop1, Color[Dot1bit[6]]);

      vga->dot(Xloop+4,Yloop1, Color[Dot1bit[5]]);
      vga->dot(Xloop+5,Yloop1, Color[Dot1bit[5]]);

      vga->dot(Xloop+6,Yloop1, Color[Dot1bit[4]]);
      vga->dot(Xloop+7,Yloop1, Color[Dot1bit[4]]);

      vga->dot(Xloop+8,Yloop1, Color[Dot1bit[3]]);
      vga->dot(Xloop+9,Yloop1, Color[Dot1bit[3]]);

      vga->dot(Xloop+10,Yloop1, Color[Dot1bit[2]]);
      vga->dot(Xloop+11,Yloop1, Color[Dot1bit[2]]);

      vga->dot(Xloop+12,Yloop1, Color[Dot1bit[1]]);
      vga->dot(Xloop+13,Yloop1, Color[Dot1bit[1]]);

      vga->dot(Xloop+14,Yloop1, Color[Dot1bit[0]]);
      vga->dot(Xloop+15,Yloop1, Color[Dot1bit[0]]);
      //----Line 3

      Yloop1++;
      vga->dot(Xloop,Yloop1, Color[Dot1bit[7]]);
      vga->dot(Xloop+1,Yloop1, Color[Dot1bit[7]]);

      vga->dot(Xloop+2,Yloop1, Color[Dot1bit[6]]);
      vga->dot(Xloop+3,Yloop1, Color[Dot1bit[6]]);

      vga->dot(Xloop+4,Yloop1, Color[Dot1bit[5]]);
      vga->dot(Xloop+5,Yloop1, Color[Dot1bit[5]]);

      vga->dot(Xloop+6,Yloop1, Color[Dot1bit[4]]);
      vga->dot(Xloop+7,Yloop1, Color[Dot1bit[4]]);

      vga->dot(Xloop+8,Yloop1, Color[Dot1bit[3]]);
      vga->dot(Xloop+9,Yloop1, Color[Dot1bit[3]]);

      vga->dot(Xloop+10,Yloop1, Color[Dot1bit[2]]);
      vga->dot(Xloop+11,Yloop1, Color[Dot1bit[2]]);

      vga->dot(Xloop+12,Yloop1, Color[Dot1bit[1]]);
      vga->dot(Xloop+13,Yloop1, Color[Dot1bit[1]]);

      vga->dot(Xloop+14,Yloop1, Color[Dot1bit[0]]);
      vga->dot(Xloop+15,Yloop1, Color[Dot1bit[0]]);

    
    
    }
  }
}


void RenderCP400GraphMode_128X64X4(void)
{
  
  if (sf.CP400VideoGenMODE != CP400_GRAPH_MODE_128X64X4)
  {
    vga->clear(0b11111111);
    vga->show();
    vga->clear(0b11111111);
    vga->show();

    sf.CP400VideoGenMODE = CP400_GRAPH_MODE_128X64X4;
  }


  uint32_t MemLoop = sf.CP400VideoPageOffset_Registers * CP400_GRAPH_OFFSET_LEN;
  uint16_t Xloop, Yloop, Yloop1;
  uint8_t Dot1bit[8];
  uint8_t Color[4];
  
  if ((sf.CP400GraphicMode & 0b00001000) == 0)
  {
    Color[0] = RGB332ToVGAPacked(0b00011100);  //Green
    Color[1] = RGB332ToVGAPacked(0b11111100);  //Yellow
    Color[2] = RGB332ToVGAPacked(0b00000011);  //Blue
    Color[3] = RGB332ToVGAPacked(0b11100000);  //Red
  }
  else
  {
    Color[0] = RGB332ToVGAPacked(0xff); //blanc
    Color[1] = RGB332ToVGAPacked(0b01011011);  //Turquoise
    Color[2] = RGB332ToVGAPacked(0b11100011);  //lilas
    Color[3] = RGB332ToVGAPacked(0b11101000);  //orange(buff)
  }  

  for (Yloop = VIDEO_Y_OFFSET; Yloop < VIDEO_Y_OFFSET + VIDEO_ACTIVE_HEIGHT; Yloop+=3)
  {
    for (Xloop = VIDEO_GRAPHICS_X_OFFSET; Xloop < VIDEO_GRAPHICS_X_OFFSET + 256; Xloop+=8)
    {

      Dot1bit[3] = (memory[MemLoop] >> 6) & 0b11;  // Bits 6-7
      Dot1bit[2] = (memory[MemLoop] >> 4) & 0b11;  // Bits 4-5
      Dot1bit[1] = (memory[MemLoop] >> 2) & 0b11;  // Bits 2-3
      Dot1bit[0] = (memory[MemLoop++] >> 0) & 0b11;  // Bits 0-1

      //Line one
      vga->dot(Xloop,Yloop, Color[Dot1bit[3]]);
      vga->dot(Xloop+1,Yloop, Color[Dot1bit[3]]);

      vga->dot(Xloop+2,Yloop, Color[Dot1bit[2]]);
      vga->dot(Xloop+3,Yloop, Color[Dot1bit[2]]);

      vga->dot(Xloop+4,Yloop, Color[Dot1bit[1]]);
      vga->dot(Xloop+5,Yloop, Color[Dot1bit[1]]);

      vga->dot(Xloop+6,Yloop, Color[Dot1bit[0]]);
      vga->dot(Xloop+7,Yloop, Color[Dot1bit[0]]);

      Yloop1 = Yloop+1;
      //Line 2

      vga->dot(Xloop,Yloop1, Color[Dot1bit[3]]);
      vga->dot(Xloop+1,Yloop1, Color[Dot1bit[3]]);

      vga->dot(Xloop+2,Yloop1, Color[Dot1bit[2]]);
      vga->dot(Xloop+3,Yloop1, Color[Dot1bit[2]]);

      vga->dot(Xloop+4,Yloop1, Color[Dot1bit[1]]);
      vga->dot(Xloop+5,Yloop1, Color[Dot1bit[1]]);

      vga->dot(Xloop+6,Yloop1, Color[Dot1bit[0]]);
      vga->dot(Xloop+7,Yloop1, Color[Dot1bit[0]]);
      Yloop1++;
      //Line 3

      vga->dot(Xloop,Yloop1, Color[Dot1bit[3]]);
      vga->dot(Xloop+1,Yloop1, Color[Dot1bit[3]]);

      vga->dot(Xloop+2,Yloop1, Color[Dot1bit[2]]);
      vga->dot(Xloop+3,Yloop1, Color[Dot1bit[2]]);

      vga->dot(Xloop+4,Yloop1, Color[Dot1bit[1]]);
      vga->dot(Xloop+5,Yloop1, Color[Dot1bit[1]]);

      vga->dot(Xloop+6,Yloop1, Color[Dot1bit[0]]);
      vga->dot(Xloop+7,Yloop1, Color[Dot1bit[0]]);

      
    }
  }

}


void RenderCP400GraphMode_128X96X2(void)
{
  
  
  if (sf.CP400VideoGenMODE != CP400_GRAPH_MODE_128X96X2)
  {
    vga->clear(0b11111111);
    vga->show();
    vga->clear(0b11111111);
    vga->show();

    sf.CP400VideoGenMODE = CP400_GRAPH_MODE_128X96X2;
  }



  uint32_t MemLoop = sf.CP400VideoPageOffset_Registers * CP400_GRAPH_OFFSET_LEN;
  uint16_t Xloop, Yloop, Yloop1;
  uint8_t Dot1bit[8];
  uint8_t Color[2];

  if ((sf.CP400GraphicMode & 0b00001000) == 0)
  {
    Color[0] = RGB332ToVGAPacked(0b00000000);  //black
    Color[1] = RGB332ToVGAPacked(0b00011100); //green
  }
  else
  {
    Color[0] = RGB332ToVGAPacked(0x00);
    Color[1] = RGB332ToVGAPacked(0xff);
  }  

  for (Yloop = VIDEO_Y_OFFSET; Yloop < VIDEO_Y_OFFSET + VIDEO_ACTIVE_HEIGHT; Yloop+=2)
  {
        for (Xloop = VIDEO_GRAPHICS_X_OFFSET; Xloop < VIDEO_GRAPHICS_X_OFFSET + 256; Xloop+=16)
    {
      Dot1bit[7] = ((memory[MemLoop]>>7) & 0b00000001);
      Dot1bit[6] = ((memory[MemLoop]>>6) & 0b00000001);
      Dot1bit[5] = ((memory[MemLoop]>>5) & 0b00000001);
      Dot1bit[4] = ((memory[MemLoop]>>4) & 0b00000001);
      Dot1bit[3] = ((memory[MemLoop]>>3) & 0b00000001);
      Dot1bit[2] = ((memory[MemLoop]>>2) & 0b00000001);
      Dot1bit[1] = ((memory[MemLoop]>>1) & 0b00000001);
      Dot1bit[0] = (memory[MemLoop++] & 0b00000001);
      
      vga->dot(Xloop,Yloop, Color[Dot1bit[7]]);
      vga->dot(Xloop+1,Yloop, Color[Dot1bit[7]]);

      vga->dot(Xloop+2,Yloop, Color[Dot1bit[6]]);
      vga->dot(Xloop+3,Yloop, Color[Dot1bit[6]]);

      vga->dot(Xloop+4,Yloop, Color[Dot1bit[5]]);
      vga->dot(Xloop+5,Yloop, Color[Dot1bit[5]]);

      vga->dot(Xloop+6,Yloop, Color[Dot1bit[4]]);
      vga->dot(Xloop+7,Yloop, Color[Dot1bit[4]]);

      vga->dot(Xloop+8,Yloop, Color[Dot1bit[3]]);
      vga->dot(Xloop+9,Yloop, Color[Dot1bit[3]]);

      vga->dot(Xloop+10,Yloop, Color[Dot1bit[2]]);
      vga->dot(Xloop+11,Yloop, Color[Dot1bit[2]]);

      vga->dot(Xloop+12,Yloop, Color[Dot1bit[1]]);
      vga->dot(Xloop+13,Yloop, Color[Dot1bit[1]]);

      vga->dot(Xloop+14,Yloop, Color[Dot1bit[0]]);
      vga->dot(Xloop+15,Yloop, Color[Dot1bit[0]]);
      
      //-------------------Second Line----------------------
      Yloop1 = Yloop+1;
      vga->dot(Xloop,Yloop1, Color[Dot1bit[7]]);
      vga->dot(Xloop+1,Yloop1, Color[Dot1bit[7]]);

      vga->dot(Xloop+2,Yloop1, Color[Dot1bit[6]]);
      vga->dot(Xloop+3,Yloop1, Color[Dot1bit[6]]);

      vga->dot(Xloop+4,Yloop1, Color[Dot1bit[5]]);
      vga->dot(Xloop+5,Yloop1, Color[Dot1bit[5]]);

      vga->dot(Xloop+6,Yloop1, Color[Dot1bit[4]]);
      vga->dot(Xloop+7,Yloop1, Color[Dot1bit[4]]);

      vga->dot(Xloop+8,Yloop1, Color[Dot1bit[3]]);
      vga->dot(Xloop+9,Yloop1, Color[Dot1bit[3]]);

      vga->dot(Xloop+10,Yloop1, Color[Dot1bit[2]]);
      vga->dot(Xloop+11,Yloop1, Color[Dot1bit[2]]);

      vga->dot(Xloop+12,Yloop1, Color[Dot1bit[1]]);
      vga->dot(Xloop+13,Yloop1, Color[Dot1bit[1]]);

      vga->dot(Xloop+14,Yloop1, Color[Dot1bit[0]]);
      vga->dot(Xloop+15,Yloop1, Color[Dot1bit[0]]);

    
    
    }
  }




}


void RenderCP400GraphMode_128X96X4(void)
{


  if (sf.CP400VideoGenMODE != CP400_GRAPH_MODE_128X96X4)
  {
    vga->clear(0b11111111);
    vga->show();
    vga->clear(0b11111111);
    vga->show();

    sf.CP400VideoGenMODE = CP400_GRAPH_MODE_128X96X4;
  }




  uint32_t MemLoop = sf.CP400VideoPageOffset_Registers * CP400_GRAPH_OFFSET_LEN;
  uint16_t Xloop, Yloop;
  uint8_t Dot1bit[8];
  uint8_t Color[4];
  
  if ((sf.CP400GraphicMode & 0b00001000) == 0)
  {
    Color[0] = RGB332ToVGAPacked(0b00011100);  //Green
    Color[1] = RGB332ToVGAPacked(0b11111100);  //Yellow
    Color[2] = RGB332ToVGAPacked(0b00000011);  //Blue
    Color[3] = RGB332ToVGAPacked(0b11100000);  //Red
  }
  else
  {
    Color[0] = RGB332ToVGAPacked(0b00000000);  // Black
    Color[2] = RGB332ToVGAPacked(0b11100000);  // Red
    Color[1] = RGB332ToVGAPacked(0b00000011);  // blue
    Color[3] = RGB332ToVGAPacked(0b11111111);  // White
    /*
    Color[0] = RGB332ToVGAPacked(0xff); //blanc
    Color[1] = RGB332ToVGAPacked(0b01011011);  //Turquoise
    Color[2] = RGB332ToVGAPacked(0b11100011);  //lilas
    Color[3] = RGB332ToVGAPacked(0b11101000);  //orange(buff)
    */
  }  

  for (Yloop = VIDEO_Y_OFFSET; Yloop < VIDEO_Y_OFFSET + VIDEO_ACTIVE_HEIGHT; Yloop+=2)
  {
    for (Xloop = VIDEO_GRAPHICS_X_OFFSET; Xloop < VIDEO_GRAPHICS_X_OFFSET + 256; Xloop+=8)
    {

      Dot1bit[3] = (memory[MemLoop] >> 6) & 0b11;  // Bits 6-7
      Dot1bit[2] = (memory[MemLoop] >> 4) & 0b11;  // Bits 4-5
      Dot1bit[1] = (memory[MemLoop] >> 2) & 0b11;  // Bits 2-3
      Dot1bit[0] = (memory[MemLoop++] >> 0) & 0b11;  // Bits 0-1

      //Line one
      vga->dot(Xloop,Yloop, Color[Dot1bit[3]]);
      vga->dot(Xloop+1,Yloop, Color[Dot1bit[3]]);

      vga->dot(Xloop+2,Yloop, Color[Dot1bit[2]]);
      vga->dot(Xloop+3,Yloop, Color[Dot1bit[2]]);

      vga->dot(Xloop+4,Yloop, Color[Dot1bit[1]]);
      vga->dot(Xloop+5,Yloop, Color[Dot1bit[1]]);

      vga->dot(Xloop+6,Yloop, Color[Dot1bit[0]]);
      vga->dot(Xloop+7,Yloop, Color[Dot1bit[0]]);

      //Line 2

      vga->dot(Xloop,Yloop+1, Color[Dot1bit[3]]);
      vga->dot(Xloop+1,Yloop+1, Color[Dot1bit[3]]);

      vga->dot(Xloop+2,Yloop+1, Color[Dot1bit[2]]);
      vga->dot(Xloop+3,Yloop+1, Color[Dot1bit[2]]);

      vga->dot(Xloop+4,Yloop+1, Color[Dot1bit[1]]);
      vga->dot(Xloop+5,Yloop+1, Color[Dot1bit[1]]);

      vga->dot(Xloop+6,Yloop+1, Color[Dot1bit[0]]);
      vga->dot(Xloop+7,Yloop+1, Color[Dot1bit[0]]);

      
    }
  }

}


void RenderCP400GraphMode_128X192X2(void)
{

  if (sf.CP400VideoGenMODE != CP400_GRAPH_MODE_128X192X2)
  {
    vga->clear(0b11111111);
    vga->show();
    vga->clear(0b11111111);
    vga->show();

    sf.CP400VideoGenMODE = CP400_GRAPH_MODE_128X192X2;
  }



  uint32_t MemLoop = sf.CP400VideoPageOffset_Registers * CP400_GRAPH_OFFSET_LEN;
  uint16_t Xloop, Yloop;
  uint8_t Dot1bit[8];
  uint8_t Color[2];

  if ((sf.CP400GraphicMode & 0b00001000) == 0)
  {
    Color[0] = RGB332ToVGAPacked(0b00000000);  //black
    Color[1] = RGB332ToVGAPacked(0b00011100); //green
  }
  else
  {
    Color[0] = RGB332ToVGAPacked(0x00);
    Color[1] = RGB332ToVGAPacked(0xff);
  }  

  for (Yloop = VIDEO_Y_OFFSET; Yloop < VIDEO_Y_OFFSET + VIDEO_ACTIVE_HEIGHT; Yloop++)
  {
    for (Xloop = VIDEO_GRAPHICS_X_OFFSET; Xloop < VIDEO_GRAPHICS_X_OFFSET + 256; Xloop+=16)
    {
      Dot1bit[7] = ((memory[MemLoop]>>7) & 0b00000001);
      Dot1bit[6] = ((memory[MemLoop]>>6) & 0b00000001);
      Dot1bit[5] = ((memory[MemLoop]>>5) & 0b00000001);
      Dot1bit[4] = ((memory[MemLoop]>>4) & 0b00000001);
      Dot1bit[3] = ((memory[MemLoop]>>3) & 0b00000001);
      Dot1bit[2] = ((memory[MemLoop]>>2) & 0b00000001);
      Dot1bit[1] = ((memory[MemLoop]>>1) & 0b00000001);
      Dot1bit[0] = (memory[MemLoop++] & 0b00000001);
      
      vga->dot(Xloop,Yloop, Color[Dot1bit[7]]);
      vga->dot(Xloop+1,Yloop, Color[Dot1bit[7]]);

      vga->dot(Xloop+2,Yloop, Color[Dot1bit[6]]);
      vga->dot(Xloop+3,Yloop, Color[Dot1bit[6]]);

      vga->dot(Xloop+4,Yloop, Color[Dot1bit[5]]);
      vga->dot(Xloop+5,Yloop, Color[Dot1bit[5]]);

      vga->dot(Xloop+6,Yloop, Color[Dot1bit[4]]);
      vga->dot(Xloop+7,Yloop, Color[Dot1bit[4]]);

      vga->dot(Xloop+8,Yloop, Color[Dot1bit[3]]);
      vga->dot(Xloop+9,Yloop, Color[Dot1bit[3]]);

      vga->dot(Xloop+10,Yloop, Color[Dot1bit[2]]);
      vga->dot(Xloop+11,Yloop, Color[Dot1bit[2]]);

      vga->dot(Xloop+12,Yloop, Color[Dot1bit[1]]);
      vga->dot(Xloop+13,Yloop, Color[Dot1bit[1]]);

      vga->dot(Xloop+14,Yloop, Color[Dot1bit[0]]);
      vga->dot(Xloop+15,Yloop, Color[Dot1bit[0]]);
      
    }
  }
}

void RenderCP400GraphMode_128X192X4(void)
{
  

  if (sf.CP400VideoGenMODE != CP400_GRAPH_MODE_128X192X4)
  {
    vga->clear(0b11111111);
    vga->show();
    vga->clear(0b11111111);
    vga->show();

    sf.CP400VideoGenMODE = CP400_GRAPH_MODE_128X192X4;
    Serial.println("MODE");
  }

  uint32_t MemLoop = sf.CP400VideoPageOffset_Registers * CP400_GRAPH_OFFSET_LEN;
  uint16_t Xloop, Yloop;
  uint8_t Dot1bit[8];
  uint8_t Color[4];
  
  if ((sf.CP400GraphicMode & 0b00001000) == 0)
  {
    Color[0] = RGB332ToVGAPacked(0b00011100);  //Green
    Color[1] = RGB332ToVGAPacked(0b11111100);  //Yellow
    Color[2] = RGB332ToVGAPacked(0b00000011);  //Blue
    Color[3] = RGB332ToVGAPacked(0b11100000);  //Red
  }
  else
  {
    Color[0] = RGB332ToVGAPacked(0xff); //blanc
    Color[1] = RGB332ToVGAPacked(0b01011011);  //Turquoise
    Color[2] = RGB332ToVGAPacked(0b11100011);  //lilas
    Color[3] = RGB332ToVGAPacked(0b11101000);  //orange(buff)
  }  



  for (Yloop = VIDEO_Y_OFFSET; Yloop < VIDEO_Y_OFFSET + VIDEO_ACTIVE_HEIGHT; Yloop++)
  {
    for (Xloop = VIDEO_GRAPHICS_X_OFFSET; Xloop < VIDEO_GRAPHICS_X_OFFSET + 256; Xloop+=8)
    {
      Dot1bit[3] = (memory[MemLoop] >> 6) & 0b11;  // Bits 6-7
      Dot1bit[2] = (memory[MemLoop] >> 4) & 0b11;  // Bits 4-5
      Dot1bit[1] = (memory[MemLoop] >> 2) & 0b11;  // Bits 2-3
      Dot1bit[0] = (memory[MemLoop++] >> 0) & 0b11;  // Bits 0-1
      
      vga->dot(Xloop,Yloop, Color[Dot1bit[3]]);
      vga->dot(Xloop+1,Yloop, Color[Dot1bit[3]]);

      vga->dot(Xloop+2,Yloop, Color[Dot1bit[2]]);
      vga->dot(Xloop+3,Yloop, Color[Dot1bit[2]]);

      vga->dot(Xloop+4,Yloop, Color[Dot1bit[1]]);
      vga->dot(Xloop+5,Yloop, Color[Dot1bit[1]]);

      vga->dot(Xloop+6,Yloop, Color[Dot1bit[0]]);
      vga->dot(Xloop+7,Yloop, Color[Dot1bit[0]]);
  
  
    }
  }

}

void RenderCP400GraphMode_256X192X2(void)
{
  if (sf.CP400VideoGenMODE != CP400_GRAPH_MODE_256X192X2)
  {
    vga->clear(0b11111111);
    vga->show();
    vga->clear(0b11111111);
    vga->show();
    sf.CP400VideoGenMODE = CP400_GRAPH_MODE_256X192X2;
  }

  uint32_t MemLoop = (sf.CP400VideoPageOffset_Registers * CP400_GRAPH_OFFSET_LEN);
  uint16_t Xloop, Yloop;
  uint8_t Dot1bit[16];
  uint8_t Color[2];

  if ((sf.CP400GraphicMode & 0b00001000) == 0)
  {
    Color[0] = RGB332ToVGAPacked(0b00000000);  //black
    Color[1] = RGB332ToVGAPacked(0b00011100); //green
  }
  else
  {
    Color[0] = RGB332ToVGAPacked(0x00);
    Color[1] = RGB332ToVGAPacked(0xff);
  }  

  for (Yloop = VIDEO_Y_OFFSET; Yloop < VIDEO_Y_OFFSET + VIDEO_ACTIVE_HEIGHT; Yloop++)

  {
    for (Xloop = VIDEO_GRAPHICS_X_OFFSET; Xloop < VIDEO_GRAPHICS_X_OFFSET + 256; Xloop+=16)
    {
      
      Dot1bit[0] = ((memory[MemLoop]>>7) & 0b00000001);
      Dot1bit[1] = ((memory[MemLoop]>>6) & 0b00000001);
      Dot1bit[2] = ((memory[MemLoop]>>5) & 0b00000001);
      Dot1bit[3] = ((memory[MemLoop]>>4) & 0b00000001);
      Dot1bit[4] = ((memory[MemLoop]>>3) & 0b00000001);
      Dot1bit[5] = ((memory[MemLoop]>>2) & 0b00000001);
      Dot1bit[6] = ((memory[MemLoop]>>1) & 0b00000001);
      Dot1bit[7] = (memory[MemLoop++] & 0b00000001);

      Dot1bit[7] = Color[Dot1bit[7]];
      Dot1bit[6] = Color[Dot1bit[6]];
      Dot1bit[5] = Color[Dot1bit[5]];
      Dot1bit[4] = Color[Dot1bit[4]];
      Dot1bit[3] = Color[Dot1bit[3]];
      Dot1bit[2] = Color[Dot1bit[2]];
      Dot1bit[1] = Color[Dot1bit[1]];
      Dot1bit[0] = Color[Dot1bit[0]];

      Dot1bit[8] = ((memory[MemLoop]>>7) & 0b00000001);
      Dot1bit[9] = ((memory[MemLoop]>>6) & 0b00000001);
      Dot1bit[10] = ((memory[MemLoop]>>5) & 0b00000001);
      Dot1bit[11] = ((memory[MemLoop]>>4) & 0b00000001);
      Dot1bit[12] = ((memory[MemLoop]>>3) & 0b00000001);
      Dot1bit[13] = ((memory[MemLoop]>>2) & 0b00000001);
      Dot1bit[14] = ((memory[MemLoop]>>1) & 0b00000001);
      Dot1bit[15] = (memory[MemLoop++] & 0b00000001);

      Dot1bit[15] = Color[Dot1bit[15]];
      Dot1bit[14] = Color[Dot1bit[14]];
      Dot1bit[13] = Color[Dot1bit[13]];
      Dot1bit[12] = Color[Dot1bit[12]];
      Dot1bit[11] = Color[Dot1bit[11]];
      Dot1bit[10] = Color[Dot1bit[10]];
      Dot1bit[9] = Color[Dot1bit[9]];
      Dot1bit[8] = Color[Dot1bit[8]];


      vga->drawLineFromMemory16(Xloop, Yloop,&Dot1bit[0]);

    }
  }
}

void IRAM_ATTR RenderCP400GraphMode_256X192X2_Artefact(void)
{
  if (sf.CP400VideoGenMODE != CP400_GRAPH_MODE_256X192X2)
  {
    vga->clear(0b11111111);
    vga->show();
    vga->clear(0b11111111);
    vga->show();
    sf.CP400VideoGenMODE = CP400_GRAPH_MODE_256X192X2;
    
  }
  uint32_t MemLoop = sf.CP400VideoPageOffset_Registers * CP400_GRAPH_OFFSET_LEN;
  uint16_t Xloop, Yloop;
  uint8_t Dot1bit[8];
  uint8_t Color[4];
  uint16_t DotCount = 0;
  uint16_t Xcount;
  uint8_t BitCount;
  bool NoBlackFlag = false;
  Color[0] = RGB332ToVGAPacked(0b00000000);  // Black
  Color[2] = RGB332ToVGAPacked(0b11100000);  // Red
  Color[1] = RGB332ToVGAPacked(0b00000011);  // blue
  Color[3] = RGB332ToVGAPacked(0b11111111);  // White
  uint8_t LinePrep[266];
  uint16_t LinePrepLoop;
  for (Yloop = VIDEO_Y_OFFSET; Yloop < VIDEO_Y_OFFSET + VIDEO_ACTIVE_HEIGHT; Yloop++)
  {
    Xcount = 0;
    DotCount = 0;
    LinePrepLoop = 0;

    for (Xloop = VIDEO_GRAPHICS_X_OFFSET; Xloop < VIDEO_GRAPHICS_X_OFFSET + 256; Xloop+=8)
    {
      Dot1bit[0] = ((memory[MemLoop]>>7) & 0b00000001);
      Dot1bit[1] = ((memory[MemLoop]>>6) & 0b00000001);
      Dot1bit[2] = ((memory[MemLoop]>>5) & 0b00000001);
      Dot1bit[3] = ((memory[MemLoop]>>4) & 0b00000001);
      Dot1bit[4] = ((memory[MemLoop]>>3) & 0b00000001);
      Dot1bit[5] = ((memory[MemLoop]>>2) & 0b00000001);
      Dot1bit[6] = ((memory[MemLoop]>>1) & 0b00000001);
      Dot1bit[7] = (memory[MemLoop++] & 0b00000001);
      
      //---The following 8 ittération used without loop for optimisation.  
      //   With FOR LOOP 0 to 7, ther is not enough time to fill the video page before the Vsynch occur
      //   --All the optimisations are validated with an oscilloscope and does the job quite well.
      BitCount = 0;
      if (Dot1bit[BitCount]==1)
        {
          DotCount++; 
          if ((Xloop + BitCount & 0b00000001))    //Check if dot is pair or impair
          {
            LinePrep[LinePrepLoop + BitCount] = Color[1];
            LinePrep[LinePrepLoop + BitCount + 1] = Color[1];
            
            NoBlackFlag = true;
          }
          else
          {
            LinePrep[LinePrepLoop + BitCount] = Color[2];
            LinePrep[LinePrepLoop + BitCount + 1] = Color[2];
            NoBlackFlag = true;
          }
          if (DotCount >1)
          {
            LinePrep[LinePrepLoop + BitCount - 1] = Color[3];
            LinePrep[LinePrepLoop + BitCount] = Color[3];
            NoBlackFlag = true;
          }
        }
        else
        {
          if (NoBlackFlag ==false)
          {
            LinePrep[LinePrepLoop + BitCount] = Color[0];
            LinePrep[LinePrepLoop + BitCount - 1] = Color[0];
          }
          DotCount = 0;  //Reset the DotCount That make Artefact possible if value is 2 or more
          NoBlackFlag = false;
        }
        

      BitCount = 1;
      if (Dot1bit[BitCount]==1)
        {
          DotCount++; 
          if ((Xloop + BitCount & 0b00000001))    //Check if dot is pair or impair
          {
            LinePrep[LinePrepLoop + BitCount] = Color[1];
            LinePrep[LinePrepLoop + BitCount + 1] = Color[1];
            
            NoBlackFlag = true;
          }
          else
          {
            LinePrep[LinePrepLoop + BitCount] = Color[2];
            LinePrep[LinePrepLoop + BitCount + 1] = Color[2];
            NoBlackFlag = true;
          }
          if (DotCount >1)
          {
            LinePrep[LinePrepLoop + BitCount - 1] = Color[3];
            LinePrep[LinePrepLoop + BitCount] = Color[3];
            NoBlackFlag = true;
          }
        }
        else
        {
          if (NoBlackFlag ==false)
          {
            LinePrep[LinePrepLoop + BitCount] = Color[0];
            LinePrep[LinePrepLoop + BitCount - 1] = Color[0];
          }
          DotCount = 0;  //Reset the DotCount That make Artefact possible if value is 2 or more
          NoBlackFlag = false;
        }

      BitCount = 2;
      if (Dot1bit[BitCount]==1)
        {
          DotCount++; 
          if ((Xloop + BitCount & 0b00000001))    //Check if dot is pair or impair
          {
            LinePrep[LinePrepLoop + BitCount] = Color[1];
            LinePrep[LinePrepLoop + BitCount + 1] = Color[1];
            
            NoBlackFlag = true;
          }
          else
          {
            LinePrep[LinePrepLoop + BitCount] = Color[2];
            LinePrep[LinePrepLoop + BitCount + 1] = Color[2];
            NoBlackFlag = true;
          }
          if (DotCount >1)
          {
            LinePrep[LinePrepLoop + BitCount - 1] = Color[3];
            LinePrep[LinePrepLoop + BitCount] = Color[3];
            NoBlackFlag = true;
          }
        }
        else
        {
          if (NoBlackFlag ==false)
          {
            LinePrep[LinePrepLoop + BitCount] = Color[0];
            LinePrep[LinePrepLoop + BitCount - 1] = Color[0];
          }
          DotCount = 0;  //Reset the DotCount That make Artefact possible if value is 2 or more
          NoBlackFlag = false;
        }

      BitCount = 3;
      if (Dot1bit[BitCount]==1)
        {
          DotCount++; 
          if ((Xloop + BitCount & 0b00000001))    //Check if dot is pair or impair
          {
            LinePrep[LinePrepLoop + BitCount] = Color[1];
            LinePrep[LinePrepLoop + BitCount + 1] = Color[1];
            
            NoBlackFlag = true;
          }
          else
          {
            LinePrep[LinePrepLoop + BitCount] = Color[2];
            LinePrep[LinePrepLoop + BitCount + 1] = Color[2];
            NoBlackFlag = true;
          }
          if (DotCount >1)
          {
            LinePrep[LinePrepLoop + BitCount - 1] = Color[3];
            LinePrep[LinePrepLoop + BitCount] = Color[3];
            NoBlackFlag = true;
          }
        }
        else
        {
          if (NoBlackFlag ==false)
          {
            LinePrep[LinePrepLoop + BitCount] = Color[0];
            LinePrep[LinePrepLoop + BitCount - 1] = Color[0];
          }
          DotCount = 0;  //Reset the DotCount That make Artefact possible if value is 2 or more
          NoBlackFlag = false;
        }

      BitCount = 4;
      if (Dot1bit[BitCount]==1)
        {
          DotCount++; 
          if ((Xloop + BitCount & 0b00000001))    //Check if dot is pair or impair
          {
            LinePrep[LinePrepLoop + BitCount] = Color[1];
            LinePrep[LinePrepLoop + BitCount + 1] = Color[1];
            
            NoBlackFlag = true;
          }
          else
          {
            LinePrep[LinePrepLoop + BitCount] = Color[2];
            LinePrep[LinePrepLoop + BitCount + 1] = Color[2];
            NoBlackFlag = true;
          }
          if (DotCount >1)
          {
            LinePrep[LinePrepLoop + BitCount - 1] = Color[3];
            LinePrep[LinePrepLoop + BitCount] = Color[3];
            NoBlackFlag = true;
          }
        }
        else
        {
          if (NoBlackFlag ==false)
          {
            LinePrep[LinePrepLoop + BitCount] = Color[0];
            LinePrep[LinePrepLoop + BitCount - 1] = Color[0];
          }
          DotCount = 0;  //Reset the DotCount That make Artefact possible if value is 2 or more
          NoBlackFlag = false;
        }

      BitCount = 5;
      if (Dot1bit[BitCount]==1)
        {
          DotCount++; 
          if ((Xloop + BitCount & 0b00000001))    //Check if dot is pair or impair
          {
            LinePrep[LinePrepLoop + BitCount] = Color[1];
            LinePrep[LinePrepLoop + BitCount + 1] = Color[1];
            
            NoBlackFlag = true;
          }
          else
          {
            LinePrep[LinePrepLoop + BitCount] = Color[2];
            LinePrep[LinePrepLoop + BitCount + 1] = Color[2];
            NoBlackFlag = true;
          }
          if (DotCount >1)
          {
            LinePrep[LinePrepLoop + BitCount - 1] = Color[3];
            LinePrep[LinePrepLoop + BitCount] = Color[3];
            NoBlackFlag = true;
          }
        }
        else
        {
          if (NoBlackFlag ==false)
          {
            LinePrep[LinePrepLoop + BitCount] = Color[0];
            LinePrep[LinePrepLoop + BitCount - 1] = Color[0];
          }
          DotCount = 0;  //Reset the DotCount That make Artefact possible if value is 2 or more
          NoBlackFlag = false;
        }

      BitCount = 6;
      if (Dot1bit[BitCount]==1)
        {
          DotCount++; 
          if ((Xloop + BitCount & 0b00000001))    //Check if dot is pair or impair
          {
            LinePrep[LinePrepLoop + BitCount] = Color[1];
            LinePrep[LinePrepLoop + BitCount + 1] = Color[1];
            
            NoBlackFlag = true;
          }
          else
          {
            LinePrep[LinePrepLoop + BitCount] = Color[2];
            LinePrep[LinePrepLoop + BitCount + 1] = Color[2];
            NoBlackFlag = true;
          }
          if (DotCount >1)
          {
            LinePrep[LinePrepLoop + BitCount - 1] = Color[3];
            LinePrep[LinePrepLoop + BitCount] = Color[3];
            NoBlackFlag = true;
          }
        }
        else
        {
          if (NoBlackFlag ==false)
          {
            LinePrep[LinePrepLoop + BitCount] = Color[0];
            LinePrep[LinePrepLoop + BitCount - 1] = Color[0];
          }
          DotCount = 0;  //Reset the DotCount That make Artefact possible if value is 2 or more
          NoBlackFlag = false;
        }

      BitCount = 7;
      if (Dot1bit[BitCount]==1)
        {
          DotCount++; 
          if ((Xloop + BitCount & 0b00000001))    //Check if dot is pair or impair
          {
            LinePrep[LinePrepLoop + BitCount] = Color[1];
            LinePrep[LinePrepLoop + BitCount + 1] = Color[1];
            
            NoBlackFlag = true;
          }
          else
          {
            LinePrep[LinePrepLoop + BitCount] = Color[2];
            LinePrep[LinePrepLoop + BitCount + 1] = Color[2];
            NoBlackFlag = true;
          }
          if (DotCount >1)
          {
            LinePrep[LinePrepLoop + BitCount - 1] = Color[3];
            LinePrep[LinePrepLoop + BitCount] = Color[3];
            NoBlackFlag = true;
          }
        }
        else
        {
          if (NoBlackFlag ==false)
          {
            LinePrep[LinePrepLoop + BitCount] = Color[0];
            LinePrep[LinePrepLoop + BitCount - 1] = Color[0];
          }
          DotCount = 0;  //Reset the DotCount That make Artefact possible if value is 2 or more
          NoBlackFlag = false;
        }



      //--------------------------------------
      LinePrepLoop+=8;
    }
    vga->drawLineFromMemory256(VIDEO_GRAPHICS_X_OFFSET, Yloop,&LinePrep[0]);
  }
}


void RenderCP400GraphMode_256X192X2_Artefact_Back(void) //(almost)
{
     vga->clear(0b11111111);
    uint32_t MemLoop = sf.CP400VideoPageOffset_Registers * CP400_GRAPH_OFFSET_LEN;
    uint16_t Xloop, Yloop;
    uint8_t Dot1bit[8];
    uint8_t Color[4];

    // Définition des couleurs artefact NTSC
    Color[0] = RGB332ToVGAPacked(0b00000000);  // noir
    Color[1] = RGB332ToVGAPacked(0b11100000);  // rouge
    Color[2] = RGB332ToVGAPacked(0b00000011);  // bleu
    Color[3] = RGB332ToVGAPacked(0b11111111);  // blanc

    for (Yloop = VIDEO_Y_OFFSET; Yloop < VIDEO_Y_OFFSET + VIDEO_ACTIVE_HEIGHT; Yloop++)
    {
        for (Xloop = VIDEO_GRAPHICS_X_OFFSET;
             Xloop < VIDEO_GRAPHICS_X_OFFSET + 256;
             Xloop += 8)
        {
            uint8_t byte = memory[MemLoop++];
            
            // 8 Bit decoding for this line
            for (int i = 0; i < 8; i += 2)
            {
                // Group pixels in pairs for artefacts NTSC
                uint8_t pair = (byte >> (6 - i)) & 0b11;
                vga->dot(Xloop + i, Yloop, Color[pair]);
                vga->dot(Xloop + i + 1, Yloop, Color[pair]);
            }
        }
    }
}




void RenderCP400GraphMode_32X16_8X12(void)
{
  if (sf.CP400VideoGenMODE != CP400_GRAPH_MODE_32X16_8X12)
  {
    vga->clear(0b00000000);
    vga->show();
    vga->clear(0b00000000);
    vga->show();
    sf.CP400VideoGenMODE = CP400_GRAPH_MODE_32X16_8X12;
  }
  gfx->fillRect(VIDEO_GRAPHICS_X_OFFSET, VIDEO_Y_OFFSET - 1,
                256, VIDEO_ACTIVE_HEIGHT, VDG_GREEN);
  gfx->setTextColor(0);
  uint32_t MemLoop = sf.CP400VideoPageOffset_Registers * CP400_GRAPH_OFFSET_LEN;
  uint8_t tmpchar;
  for (uint8_t Yloop = 0; Yloop !=16; Yloop++)
  {
    for (uint8_t Xloop = 0; Xloop != 32; Xloop++)
    {
      tmpchar = memory[MemLoop++];
      DisplayVDGchar(tmpchar, Xloop * 8 + VIDEO_GRAPHICS_X_OFFSET,
                     Yloop * 12 + VIDEO_Y_OFFSET - 1);
    }
  }
}




void CopyCP400ROMS1(void)
{
  uint32_t LoopRomSource, LoopRam1;

  // Copy extbas11 to 0x8000 (8K)
  LoopRam1 = 0x8000;
  for (LoopRomSource = 0; LoopRomSource < 8192; LoopRomSource++)
    memory[LoopRam1++] = extbas11[LoopRomSource];

  // Copy bas12 to 0xA000 (8K)
  LoopRam1 = 0xA000;
  for (LoopRomSource = 0; LoopRomSource < 8192; LoopRomSource++)
  memory[LoopRam1++] = bas13[LoopRomSource];


  const uint8_t *diskRom = selectedDiskRom == DiskRomSelection::CP400
                             ? cp400dsk
                             : disk11;

  // Copy the selected disk controller ROM to 0xC000 (8K)
  LoopRam1 = 0xC000;
  for (LoopRomSource = 0; LoopRomSource < 8192; LoopRomSource++)
    memory[LoopRam1++] = diskRom[LoopRomSource];

  // copy memory[0x8000 - 0xFFFF] to rom[0 - 0x7FFF]
  LoopRomSource = 0;
  for (LoopRam1 = 0x8000; LoopRam1 < 0x10000; LoopRam1++)
  {
    rom[LoopRomSource++] = memory[LoopRam1];
  }
  // Reset vectors
  
  uint32_t VectorsLoop = 0;
  LoopRomSource = 0xfff0 - 0x8000;
  for (VectorsLoop = 0; VectorsLoop !=16 ; VectorsLoop++)
  {
    memory[LoopRomSource + 0x8000] = ResetVectors[VectorsLoop];
    rom[LoopRomSource++] = ResetVectors[VectorsLoop];
  }
}


// Loads the active CP400 ROM set (extended BASIC, BASIC, disk controller ROM)
// into emulated memory/ROM space.
void CopyCP400ROMS(void)
{
  uint32_t LoopRomSource, LoopRam1;

  // Copy extbas11 to 0x8000 (8K)
  LoopRam1 = 0x8000;
  for (LoopRomSource = 0; LoopRomSource < 8192; LoopRomSource++)
    memory[LoopRam1++] = extbas11[LoopRomSource];

  // Copy bas12 to 0xA000 (8K)
  LoopRam1 = 0xA000;
  for (LoopRomSource = 0; LoopRomSource < 8192; LoopRomSource++)
  memory[LoopRam1++] = bas13[LoopRomSource];


  const uint8_t *diskRom = selectedDiskRom == DiskRomSelection::CP400
                             ? cp400dsk
                             : disk11;

  // Copy the selected disk controller ROM to 0xC000 (8K)
  LoopRam1 = 0xC000;
  for (LoopRomSource = 0; LoopRomSource < 8192; LoopRomSource++)
    memory[LoopRam1++] = diskRom[LoopRomSource];

  // copy memory[0x8000 - 0xFFFF] to rom[0 - 0x7FFF]
  LoopRomSource = 0;
  for (LoopRam1 = 0x8000; LoopRam1 < 0x10000; LoopRam1++)
  {
    rom[LoopRomSource++] = memory[LoopRam1];
  }
  // Reset vectors
  
  uint32_t VectorsLoop = 0;
  LoopRomSource = 0xfff0 - 0x8000;
  for (VectorsLoop = 0; VectorsLoop !=16 ; VectorsLoop++)
  {
    memory[LoopRomSource + 0x8000] = ResetVectors[VectorsLoop];
    rom[LoopRomSource++] = ResetVectors[VectorsLoop];
  }
}


// Optional CoCo 3 ROM loader, kept for compatibility/reference; not part of
// the active CP400 emulation path (see CopyCP400ROMS()).
void CopyCoCo3ROMS(void)
{
  uint32_t LoopRom1, LoopRam1, LoopRomSource;

  LoopRam1 = 0x8000;  //coco3p
  for (LoopRom1 = 0; LoopRom1 != 8192 * 3; LoopRom1++)
  {
    memory[LoopRam1++] = coco3[LoopRom1];
  }

  const uint8_t *diskRom = selectedDiskRom == DiskRomSelection::CP400
                             ? cp400dsk
                             : disk11;

  LoopRam1 = 0xc000;  // Selected disk controller ROM
  for (LoopRom1 = 0; LoopRom1 != 8192; LoopRom1++)
  {
    memory[LoopRam1++] = diskRom[LoopRom1];
  }

 
    // Copy de memory[0x8000 - 0xFFFF] to rom[0 - 0x7FFF]
    LoopRomSource = 0;
    for (LoopRam1 = 0x8000; LoopRam1 < 0x10000; LoopRam1++)
    {
      rom[LoopRomSource++] = memory[LoopRam1];
    }

    
 
  return;
}

/*
DRQ = 1 when the Computer can read data or write data to the register.
DRQ is 0 when is busy.
*/

#define MACRO_TRACK_R DiskAccess.DSK_FILE_DataPTR = (DiskAccess.TrackPos * 18 * 256) + ((DiskAccess.SectorPos - 1) * 256)
#define MACRO_TRACK_W DiskAccess.DSK_FILE_DataPTR = (DiskAccess.TrackPos * 18 * 256) + ((DiskAccess.SectorPos - 1) * 256)
#define M_DRQ_BIT_READY 0b00000010
  
  
  
void ManagePeripherals_Read(uint16_t address)
{
  
  
  if (DiskAccess.NMI_Delay >0)
  {
    DiskAccess.NMI_Delay--;
    if (DiskAccess.NMI_Delay==2)
    {
      sf.nmi_pin = false;  //NMI Started
    }

    if (DiskAccess.NMI_Delay == 1)
    {
      sf.nmi_pin = true;  //NMI Stopped

    }


  }


  //Manage all the read operation of memory addresses
  switch (address)
  {
    case M_FF02: //ROM_FF02:
      rom[ROM_FF03] &= 0b01111111;  //If ff02 Address is read, it reset the Vsynch interrupt flag AT ff03.
      if (sf.V_Synch_Int_Enabled)
      {
        sf.irq_pin = true;  //Interrupt Disabled.
      }
      sf.V_Synch = false; 

      break;
      //All next are drive related--------------------------

      
    case M_FF00: //ROM_FF02:

    uint8_t val1, buttons, tmp1;
    uint8_t MultiplexerCB2_CA2;
    if (true)  
    {
      ManageKeyboardScan(rom[ROM_FF02]);
      //Do the joystick stuff first
      buttons = ReadCP400Buttons();
      
      if ((buttons & 0b00000001) == 0)
      {
        //Serial.print("JO");
        sf.is_JOY1_B1_WasPressed=true;
        rom[ROM_FF00] &=0b11111110;
      }
      else
      {

        if (!sf.is_LastKeyboardScanned)
        {
          rom[ROM_FF00] |=0b00000001;
        }
        else
        {
          sf.is_LastKeyboardScanned = false;
        }
      }

      if ((buttons & 0b00000010) == 0)
      {
        
        rom[ROM_FF00] &=0b11111101;
      }
      
      
    
      MultiplexerCB2_CA2 = (((rom[ROM_FF03] & 0b00001000)>>2) | ((rom[ROM_FF01] & 0b00001000)>>3));   //Get the two bits Comparator Selection
      val1 = ReadJoysticks(MultiplexerCB2_CA2);   //Sample Requested joystick
      
      if (val1 > (rom[ROM_FF20] & 0b11111100))
      {
        rom[ROM_FF00] |= 0b10000000;

      }
      else
      {
        rom[ROM_FF00] &= 0b01111111;

      }

    }
    else //Manage Keyboard
    {
      uint8_t val1;
      ManageKeyboardScan(rom[ROM_FF02]);
    }

    

      break;


    case M_FF48: 

      if (DiskAccess.IsinReadProcess)   //If Disk Access transaction;
      {
        rom[ROM_FF48] = M_DRQ_BIT_READY;  //Will step the ASM routine to read the next Byte
      }
      else
      {
        rom[ROM_FF48] = M_DRQ_BIT_READY;  //Always no error
      }

    break;    //error here, this is why the next block works
    case  M_FF4B:
      
    if (DiskAccess.IsinReadProcess)
    {
      
      DiskAccess.DriveSelected = GetDriveNumber(rom[ROM_FF40]);
      if (DiskAccess.DriveSelected != Disk_Drive.LastNumber_Accessed)
      {
        Disk_Drive.LastNumber_Accessed = DiskAccess.DriveSelected;
        if (Disk_Drive.isFileAlreadyOpen)
        {
          //file.close(); //Close the actual
        }


        Disk_Drive.isFileAlreadyOpen = true;
      }
      rom[ROM_FF4B] = ReadDiskByte(DiskAccess.DSK_FILE_DataPTR);  //RAM_Disk[DiskAccess.DSK_FILE_DataPTR++];
      DiskAccess.DSK_FILE_DataPTR++;
      DiskAccess.RW_Process_ByteRemainingCounter--;
      if (DiskAccess.RW_Process_ByteRemainingCounter==0)
      {
        rom[ROM_FF48] = 0;
        DiskAccess.IsinReadProcess = false;
        DISKETTE_LED_OFF();
        DiskAccess.NMI_Delay = 4;
        sf.nmi_pin = false;  //NMI Started
        
      }
    }
    else
    {
      Disk_Drive.isFileAlreadyOpen = false;
      //file.close();  //Close the actual
      Disk_Drive.LastNumber_Accessed = 254;
    }
    break;


    default:
    break;
  }
  //return rom[address-0xff40];
}


uint8_t ReadDiskByte(uint32_t BytePos)
{
  uint8_t ByteRead;
  switch (DiskAccess.DriveSelected)
  {
  case 0:
  ByteRead = RAM_Disk0[BytePos];
  break;
  case 1:
  ByteRead = RAM_Disk1[BytePos];
  break;
  case 2:
  ByteRead = RAM_Disk2[BytePos];
  break;
  case 3:
  ByteRead = RAM_Disk3[BytePos];
  break;
  
  default:
    break;
  }
  return ByteRead;
}

void WriteDiskByte(uint32_t BytePos, uint8_t ByteData)
{

  switch (DiskAccess.DriveSelected)
  {
  case 0:
    RAM_Disk0[BytePos] = ByteData;
  break;
  case 1:
    RAM_Disk1[BytePos] = ByteData;
  break;
  case 2:
    RAM_Disk2[BytePos] = ByteData;
  break;
  case 3:
    RAM_Disk3[BytePos] = ByteData;
  break;
  
  default:
    break;
  }


}


uint8_t MountFileSystem(void)
{
  if (!SD_MMC.begin())
    return 2; // No Card / init fail

  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE)
    return 3; // No media

  return 0;
}

uint8_t GetDriveNumber(uint8_t Address)
{
  uint8_t tmpDrive = 0;
  if (Address & 0b00000001)
  {
    tmpDrive = 0;
  }
  else if (Address & 0b00000010)
  {
    tmpDrive = 1;
  }
  else if (Address & 0b00000100)
  {
    tmpDrive = 2;
  }
  else if (Address & 0b01000000)
  {
    tmpDrive = 3;
  }
  return tmpDrive;  
}


void InitDisks(void)
{
  memset(Disk_Drive.Name_Disk, 0, sizeof(Disk_Drive.Name_Disk));
  
  LoadConfigFromSD();

  Disk_Drive.Name_Disk[0][255] = 0; //Just to be sure.
  Disk_Drive.Name_Disk[1][255] = 0; //Just to be sure.
  Disk_Drive.Name_Disk[2][255] = 0; //Just to be sure.
  Disk_Drive.Name_Disk[3][255] = 0; //Just to be sure.

  Disk_Drive.LastNumber_Accessed = 255; //No Last Accessed
  Disk_Drive.isFileAlreadyOpen = false;

  Disk_Drive.Error = false;
  Disk_Drive.LastAccessType = 255;

  return;
}



  void DebugTrack(void)
  {
    Serial.print("T: ");
    Serial.print(rom[ROM_FF49]);
    Serial.print("  S: ");
    Serial.print(rom[ROM_FF4A]);
    Serial.print(" SF T:");
    Serial.print(DiskAccess.TrackPos);
    Serial.print(" SF S:");
    Serial.print(DiskAccess.SectorPos);
    Serial.print(" Data Pointer: ");
    Serial.print(DiskAccess.DSK_FILE_DataPTR);

    Serial.print(" PC: ");
    Serial.println(cpu.get_pc(),HEX);


    delay(40);
  }


  void ManagePeripherals_Write(uint16_t address, uint8_t value)
  {


    uint8_t tmpvar8t;
    uint16_t tmpvar16t;

    if (DiskAccess.NMI_Delay >0)
    {
      DiskAccess.NMI_Delay--;
      if (DiskAccess.NMI_Delay==2)
      {
        sf.nmi_pin = false;  //NMI Started

        rom[ROM_FF48] = 0;
      }

      if (DiskAccess.NMI_Delay == 1)
      {
        sf.nmi_pin = true;  //NMI Stopped
      }

    }




    switch (address)
    {
      case M_FF20:
        rom[ROM_FF20] = value;
        
        if (IsAudioMuxEnabled())  //So, Sound is enabled
        {
          AudioWriteSample(value & AUDIO_DAC_MASK);
        }
      break;
      case M_FF23:
        //PIA1 CRB. Bits 7 and 6 are read only interrupt flags on real hardware.
        //Storing the rest is what makes the ROM read-modify-write that opens and
        //closes the audio mux actually work.
        rom[ROM_FF23] = value & 0b00111111;

        if (IsAudioMuxEnabled())
        {
          //Reopening the mux hands the speaker back the current DAC level.
          AudioWriteSample(rom[ROM_FF20] & AUDIO_DAC_MASK);
        }
        else
        {
          //Closing the mux disconnects the DAC on real hardware. Stop switching
          //so the idle output is silent instead of parking on a carrier.
          AudioSilence();
        }
      break;
    case M_FF01:
        //CA2 (bit 3) selects the joystick mux channel; bit 7 is the read-only interrupt flag.
        rom[ROM_FF01] = (rom[ROM_FF01] & 0b10000000) | (value & 0b01111111);
      break;
    case M_FF02:
        ManageKeyboardScan(value);
        rom[ROM_FF02] = value;
      break;
    case M_FF03:
        if ((value & 0b00000001) == 0)
        {
          sf.V_Synch_Int_Enabled = false;
        }
        else
        {
          sf.V_Synch_Int_Enabled = true;
        }  
        //CB2 (bit 3) selects the joystick mux channel; bit 7 is the read-only interrupt flag.
        rom[ROM_FF03] = (rom[ROM_FF03] & 0b10000000) | (value & 0b01111111);
      break;

      case M_FFD9:
        sf.CPU_Speed = CPU_FAST;
      break;
      case M_FFD8:
      sf.CPU_Speed = CPU_SLOW;
      break;
//----------------Disk Related---------------
   case M_FF40:
    rom[ROM_FF40] = value;
    
    break;


#define DSKCMD_RESTORE 0x03
#define DSKCMD_SEEK 0x10
#define DSKCMD_STEP 0x20
#define DSKCMD_STEP_IN 0x40
#define DSKCMD_STEP_OUT 0x50
#define DSKCMD_READ_SECTOR 0x80
#define DSKCMD_WRITE_SECTOR 0xa0
#define DSKCMD_READ_ADDRESS 0xc0
#define DSKCMD_READ_TRACK 0xe0
#define DSKCMD_WRITE_TRACK 0xf0
#define DSKCMD_FORCE_INT 0xd0


    case M_FF48:
      if (value > 0b00001111)
      {
        value &=0b11110000;
      }
    
      rom[ROM_FF48] = value;
      #ifdef DEBUG_ALL
      Serial.print("Drive:");
      Serial.print(GetDriveNumber(rom[ROM_FF40]));
      Serial.print(" Track:");
      Serial.print(rom[ROM_FF49]);
      Serial.print(" Sec:");
      Serial.println(rom[ROM_FF4A]);
      Serial.print("FF48:");
      Serial.println(value,HEX);
      #endif
      DiskAccess.DRIVE_COMMAND = value;
      switch (value)
      {
        case DSKCMD_STEP:

        while(1)
        {
          //sleep(100);
        }    


        case DSKCMD_SEEK:
        rom[ROM_FF48] = 0;
        rom[ROM_FF49] = rom[ROM_FF4B];    //New Rom track is in Data register
        DiskAccess.TrackPos = rom[ROM_FF49];
        DiskAccess.SectorPos = rom[ROM_FF4A];
        break;
      case DSKCMD_RESTORE:
        DiskAccess.TrackPos = 0;
        DiskAccess.SectorPos = 0;
        rom[ROM_FF49] = 0;  //Track position 0
        rom[ROM_FF4A] = 0;  //Sector position 0
        rom[ROM_FF48] = 0;  //Always OK
        break;
      case DSKCMD_READ_SECTOR:
        DISKETTE_LED_ON();
        DiskAccess.IsinReadProcess = true;
        DiskAccess.RW_Process_ByteRemainingCounter = 257; //Init the loop counter for data to be transfered;
        DiskAccess.TrackPos = rom[ROM_FF49];
        DiskAccess.SectorPos = rom[ROM_FF4A];
        MACRO_TRACK_R;
        rom[ROM_FF48] = 0;  //Reset the bit
        
        break;
      
      case DSKCMD_STEP_IN:
        rom[ROM_FF49]+=1; 
        DiskAccess.TrackPos +=1;
        break;
      case DSKCMD_STEP_OUT:
        rom[ROM_FF49]-=1; 
        DiskAccess.TrackPos -=1;
        break;
      case DSKCMD_WRITE_SECTOR:
        DISKETTE_LED_ON();  
        DiskAccess.IsInWriteProcess = true;
        DiskAccess.RW_Process_ByteRemainingCounter = 257; //Init the loop counter for data to be transfered;
        DiskAccess.TrackPos = rom[ROM_FF49];
        DiskAccess.SectorPos = rom[ROM_FF4A];
        
        MACRO_TRACK_W;
        rom[ROM_FF48] = M_DRQ_BIT_READY;  //Reset the bit
        break;
      default:
        break;
      }
    break;
    case M_FF49:
      rom[ROM_FF49] = value;
      DiskAccess.TrackPos = value;
      rom[ROM_FF48] = M_DRQ_BIT_READY;
      break;

    case M_FF4A:
      rom[ROM_FF4A] = value;
      DiskAccess.SectorPos = value;
      rom[ROM_FF48] = M_DRQ_BIT_READY;
    break;



  case  M_FF4B:
    rom[ROM_FF4B] = value;
    if (DiskAccess.IsInWriteProcess)
    {
      if (DiskAccess.RW_Process_ByteRemainingCounter<258)   //Skip the first write (Not valid)
      {
        if (DiskAccess.DriveSelected != Disk_Drive.LastNumber_Accessed)
        {
          Disk_Drive.LastNumber_Accessed = DiskAccess.DriveSelected;
          if (Disk_Drive.isFileAlreadyOpen)
          {
            //file.close(); //Close the actual
          }
          Disk_Drive.isFileAlreadyOpen = true;
        }

        //Serial.print("W");
        WriteDiskByte(DiskAccess.DSK_FILE_DataPTR, rom[ROM_FF4B]);
        DiskAccess.DSK_FILE_DataPTR++;
        rom[ROM_FF48] = M_DRQ_BIT_READY;
      }
      else
      {
      }
      DiskAccess.RW_Process_ByteRemainingCounter--;

      if (DiskAccess.RW_Process_ByteRemainingCounter==1)
      {
        DiskAccess.RW_Process_ByteRemainingCounter = 0;
        //file.flush(); 
        DISKETTE_LED_OFF();
        
        sf.PHYSICAL_Drive_Must_Be_Saved = true; //Flag to request a File save (can't do it in Fotwrare Interrupt)
        rom[ROM_FF48] = M_DRQ_BIT_READY;

        DiskAccess.IsInWriteProcess = false;
        DiskAccess.NMI_Delay = 4;
        sf.nmi_pin = false;  //NMI Started
        Disk_Drive.isFileAlreadyOpen = false;

        Disk_Drive.LastNumber_Accessed = 254;
        
      }
    }
    else
    {
      Disk_Drive.isFileAlreadyOpen = false; //ADDED

      Disk_Drive.LastNumber_Accessed = 254; //ADDED
      
      
      rom[ROM_FF4B] = value;
      DiskAccess.DataRegisterValue = value;
      rom[ROM_FF48] = M_DRQ_BIT_READY;
    }
    break;


//------------All the next logic is for videomodes---------------------
    case M_FF22:
      
      

      rom[ROM_FF22] = value;
      sf.CP400GraphicMode = (sf.CP400GraphicMode & 0b00000111) | (value & 0b11111000);
      
      break;


      //In the next decode, no need to store data at address because it's only set reset.  Address are not readables
    case M_FFC0:
      sf.CP400GraphicMode &=0b11111110;
    break;
    case M_FFC1:
      sf.CP400GraphicMode |=0b00000001;
    break;
    case M_FFC2:
      sf.CP400GraphicMode &=0b11111101;
    break;
    case M_FFC3:
      sf.CP400GraphicMode |=0b00000010;
    break;

    case M_FFC4:
      sf.CP400GraphicMode &=0b11111011;
    break;
    case M_FFC5:
      sf.CP400GraphicMode |=0b00000100;
    break;
    //--------------------Next are Address offset SET/RESET registers--------------------
    case M_FFC6:
      sf.CP400VideoPageOffset_Registers &=0b11111110; //Clear the bit
    break;
    case M_FFC7:
    sf.CP400VideoPageOffset_Registers |=0b00000001; //Set the bit
    break;
    case M_FFC8:
      sf.CP400VideoPageOffset_Registers &=0b11111101; //Clear the bit
    break;
    case M_FFC9:
    sf.CP400VideoPageOffset_Registers |=0b00000010; //Set the bit
    break;
    case M_FFCA:
      sf.CP400VideoPageOffset_Registers &=0b11111011; //Clear the bit
    break;
    case M_FFCB:
    sf.CP400VideoPageOffset_Registers |=0b00000100; //Set the bit
    break;
    case M_FFCC:
      sf.CP400VideoPageOffset_Registers &=0b11110111; //Clear the bit
    break;
    case M_FFCD:
    sf.CP400VideoPageOffset_Registers |=0b00001000; //Set the bit
    break;
    case M_FFCE:
      sf.CP400VideoPageOffset_Registers &=0b11101111; //Clear the bit
    break;
    case M_FFCF:
    sf.CP400VideoPageOffset_Registers |=0b00010000; //Set the bit
    break;
    case M_FFD0:
      sf.CP400VideoPageOffset_Registers &=0b11011111; //Clear the bit
    break;
    case M_FFD1:
    sf.CP400VideoPageOffset_Registers |=0b00100000; //Set the bit
    break;
    case M_FFD2:
      sf.CP400VideoPageOffset_Registers &=0b10111111; //Clear the bit
    break;
    case M_FFD3:
    sf.CP400VideoPageOffset_Registers |=0b01000000; //Set the bit
    break;

    case M_FFDE:
      sf.CP400_32K_UPPER_ENABLED = false;
    break;
    case M_FFDF:
      sf.CP400_32K_UPPER_ENABLED = true;
    break;




      default:
      break;
    }

  }

void FillKeyboardMatrix(void)
{
  for (int loop1 = 0; loop1 != 8; loop1++)
  {
    for (int loop2 = 0; loop2 != 7; loop2++)
    {
      SCAN_Keyboard_Matrix[loop1][loop2] = 0;
    }
  }
  sf.AnyKeypress = false; 
  
  for (uint8_t loop1 = 0; loop1 !=8; loop1++)
  {
    if (USB_DEV_CONTROL.USB_CP400_Key_Array[loop1] & 0b01000000)
    SCAN_Keyboard_Matrix[loop1][6] = ((USB_DEV_CONTROL.USB_CP400_Key_Array[loop1] & 0b01000000) ^ 0b01111111);

    if (USB_DEV_CONTROL.USB_CP400_Key_Array[loop1] & 0b00100000)
        SCAN_Keyboard_Matrix[loop1][5] = ((USB_DEV_CONTROL.USB_CP400_Key_Array[loop1] & 0b00100000) ^ 0b01111111);

    if (USB_DEV_CONTROL.USB_CP400_Key_Array[loop1] & 0b00010000)
        SCAN_Keyboard_Matrix[loop1][4] = ((USB_DEV_CONTROL.USB_CP400_Key_Array[loop1] & 0b00010000) ^ 0b01111111);

    if (USB_DEV_CONTROL.USB_CP400_Key_Array[loop1] & 0b00001000)
        SCAN_Keyboard_Matrix[loop1][3] = ((USB_DEV_CONTROL.USB_CP400_Key_Array[loop1] & 0b00001000) ^ 0b01111111);

    if (USB_DEV_CONTROL.USB_CP400_Key_Array[loop1] & 0b00000100)
        SCAN_Keyboard_Matrix[loop1][2] = ((USB_DEV_CONTROL.USB_CP400_Key_Array[loop1] & 0b00000100) ^ 0b01111111);

    if (USB_DEV_CONTROL.USB_CP400_Key_Array[loop1] & 0b00000010)
        SCAN_Keyboard_Matrix[loop1][1] = ((USB_DEV_CONTROL.USB_CP400_Key_Array[loop1] & 0b00000010) ^ 0b01111111);

    if (USB_DEV_CONTROL.USB_CP400_Key_Array[loop1] & 0b00000001)
        SCAN_Keyboard_Matrix[loop1][0] = ((USB_DEV_CONTROL.USB_CP400_Key_Array[loop1] & 0b00000001) ^ 0b01111111);
    if (USB_DEV_CONTROL.USB_CP400_Key_Array[loop1] != 0)
    {
      sf.AnyKeypress = true; 
    }
  }


}
  
  
  void ManageKeyboardScan(uint8_t value)
  {
    uint8_t loop1, loop2, Val1;
    bool ValDone = false;

    Val1 = value ^ 0b11111111;
    rom[ROM_FF00] |= 0b01111111;   //Reset to nothing to scan.
    
    if (ReadCP400Buttons()==3 && !sf.is_JOY1_B1_WasPressed) //All buttons released and was not pressed
    {
      sf.is_LastKeyboardScanned = true;
      
      if ((Val1 & 0b10000000) != 0 && ValDone == false) //case 0b01111111
      {
        for (loop1 = 0; loop1 !=7; loop1++)  //Scan the collumn for active keypress
        {
          if (SCAN_Keyboard_Matrix[7][loop1] != 0)
          {

            rom[ROM_FF00] &= SCAN_Keyboard_Matrix[7][loop1];
            ValDone = true;
          }
        }
      }

      if ((Val1 & 0b01000000) != 0 && ValDone == false) //case 0b10111111
      {
        for (loop1 = 0; loop1 !=7; loop1++)  //Scan the collumn for active keypress
        {
          if (SCAN_Keyboard_Matrix[6][loop1] != 0)
          {
            rom[ROM_FF00] &= SCAN_Keyboard_Matrix[6][loop1];
            ValDone = true;
          }
        }
      }

      if ((Val1 & 0b00100000) != 0 && ValDone == false) //case 0b11011111
      {
        for (loop1 = 0; loop1 !=7; loop1++)  //Scan the collumn for active keypress
        {
          if (SCAN_Keyboard_Matrix[5][loop1] != 0)
          {
            rom[ROM_FF00] &= SCAN_Keyboard_Matrix[5][loop1];
            ValDone = true;
          }
        }
      }
        
      if ((Val1 & 0b00010000) != 0 && ValDone == false) //case 0b11101111
      {
        for (loop1 = 0; loop1 !=7; loop1++)  //Scan the collumn for active keypress
        {
          if (SCAN_Keyboard_Matrix[4][loop1] != 0)
          {
            rom[ROM_FF00] &= SCAN_Keyboard_Matrix[4][loop1];
            ValDone = true;
          }
        }
      }

      if ((Val1 & 0b00001000) != 0 && ValDone == false) //case 0b11110111
      {
        for (loop1 = 0; loop1 !=7; loop1++)  //Scan the collumn for active keypress
        {
          if (SCAN_Keyboard_Matrix[3][loop1] != 0)
          {
            rom[ROM_FF00] &= SCAN_Keyboard_Matrix[3][loop1];
            ValDone = true;
          }
        }
      } 
      
      if ((Val1 & 0b00000100) != 0 && ValDone == false) //case 0b11111011
      {
        for (loop1 = 0; loop1 !=7; loop1++)  //Scan the collumn for active keypress
        {
          if (SCAN_Keyboard_Matrix[2][loop1] != 0)
          {
            rom[ROM_FF00] &= SCAN_Keyboard_Matrix[2][loop1];
            ValDone = true;
          }
        }
      }

      if ((Val1 & 0b00000010) != 0 && ValDone == false) //case 0b11111101
      {
        for (loop1 = 0; loop1 !=7; loop1++)  //Scan the collumn for active keypress
        {
          if (SCAN_Keyboard_Matrix[1][loop1] != 0)
          {
            rom[ROM_FF00] &= SCAN_Keyboard_Matrix[1][loop1];
            ValDone = true;
          }
        }
      }
        
      if ((Val1 & 0b00000001) != 0 && ValDone == false) //case 0b11111110
      {
        for (loop1 = 0; loop1 !=7; loop1++)  //Scan the collumn for active keypress
        {
          if (SCAN_Keyboard_Matrix[0][loop1] != 0)
          {
            rom[ROM_FF00] &= SCAN_Keyboard_Matrix[0][loop1];
            ValDone = true;
          }
        }
      }

      
    }
    else
    {

      sf.is_JOY1_B1_WasPressed=false;
      rom[ROM_FF00] &= 0x7f;
    }
  }


  //--------------------------------VDG MAP----------------------------------------------------------
  


  void DisplayVDGchar(uint8_t charNum, uint16_t Xpos, uint16_t Ypos)
  {
    uint32_t loop1;
    uint16_t XposLoop, YposLoop;
    uint8_t convertedLine[8];
    loop1 = charNum * 8 * 12;
    for (YposLoop = Ypos; YposLoop != Ypos+12; YposLoop++)
    {
      for (uint8_t pixel = 0; pixel < sizeof(convertedLine); pixel++)
      {
        convertedLine[pixel] = RGB332ToVGAPacked(cc2_VDG_MAP[loop1 + pixel]);
      }
      vga->drawLineFromMemory8(Xpos, YposLoop, convertedLine);
      loop1+=8;
    }
  }

void line(int x0, int y0, int x1, int y1, int rgb)
{
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;

    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;

    int err = dx + dy; 

    while (true)
    {
        vga->dot(x0, y0, rgb); 

        if (x0 == x1 && y0 == y1)
            break;

        int e2 = 2 * err;
        if (e2 >= dy)
        {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}


  //---------------------------------------USB STACK FOR CP400 KEYBOARD MAPPING AND JOYSTICKS-----------------------




static void my_USB_DetectCB( uint8_t usbNum, void * dev )
{
  sDevDesc *device = (sDevDesc*)dev;
  printf("USB device detected on port %u: VID=0x%04x PID=0x%04x\n",
         usbNum, device->idVendor, device->idProduct);
#ifdef DEBUG_PRINT  
  printf("New device detected on USB#%d\n", usbNum);
  printf("desc.bcdUSB             = 0x%04x\n", device->bcdUSB);
  printf("desc.bDeviceClass       = 0x%02x\n", device->bDeviceClass);
  printf("desc.bDeviceSubClass    = 0x%02x\n", device->bDeviceSubClass);
  printf("desc.bDeviceProtocol    = 0x%02x\n", device->bDeviceProtocol);
  printf("desc.bMaxPacketSize0    = 0x%02x\n", device->bMaxPacketSize0);
  printf("desc.idVendor           = 0x%04x\n", device->idVendor);
  printf("desc.idProduct          = 0x%04x\n", device->idProduct);
  printf("desc.bcdDevice          = 0x%04x\n", device->bcdDevice);
  printf("desc.iManufacturer      = 0x%02x\n", device->iManufacturer);
  printf("desc.iProduct           = 0x%02x\n", device->iProduct);
  printf("desc.iSerialNumber      = 0x%02x\n", device->iSerialNumber);
  printf("desc.bNumConfigurations = 0x%02x\n", device->bNumConfigurations);
#endif
  // if( device->iProduct == mySupportedIdProduct && device->iManufacturer == mySupportedManufacturer ) {
  //   myListenUSBPort = usbNum;
  // }
}


static void my_USB_PrintCB(uint8_t usbNum, uint8_t byte_depth, uint8_t* data, uint8_t data_len)
{
  // if( myListenUSBPort != usbNum ) return;
  //The keyboard is handled by the USB-OTG host, so only the joystick ports
  //arrive here.
  if (usbNum == USB_DEV_CONTROL.PORT_JOY1)
  {
    UpdateJoyMap(data, data_len, usbNum);
  }
  else if (usbNum == USB_DEV_CONTROL.PORT_JOY2)
  {
    UpdateJoyMap(data, data_len, usbNum);
  }
  
  #ifdef DEBUG_ALL
  printf(" in: ");
  for(int k=0;k<data_len;k++) {
    printf("0x%02x ", data[k] );
  }
  printf("\n");
#endif

  }

usb_pins_config_t USB_Pins_Config =
{
  DP_P0, DM_P0,
  DP_P1, DM_P1,
  DP_P2, DM_P2,
  DP_P3, DM_P3
};

void Setup_USB(void)
{
    fillKeysStruct();
  
    USH.setOnConfigDescCB( Default_USB_ConfigDescCB );
    USH.setOnIfaceDescCb( Default_USB_IfaceDescCb );
    USH.setOnHIDDevDescCb( Default_USB_HIDDevDescCb );
    USH.setOnEPDescCb( Default_USB_EPDescCb );

    //Joysticks only. The soft host timer also clocks the 6809, so it keeps
    //running even though the keyboard port has moved off it.
    USH.init( USB_Pins_Config, my_USB_DetectCB, my_USB_PrintCB );

    CP400UsbKeyboard_Start();

}

#define K_ESC 27
#define K_F1 1
#define K_F2 2
#define K_BACKSPACE 8
#define K_APOSTROPHE 39
#define K_ENTER 13
#define K_QUOTE 34
#define K_SPACE 32
#define K_CLEAR 3
#define K_SHIFT 0x82
#define K_CTRL 0x81
#define K_ALT 0x84
#define K_CAPS 0x0a  //SHIFT 0 on CP400

#define K_UP 4
#define K_DOWN 5
#define K_LEFT 6
#define K_RIGHT 7


#define lookupOR_0 0b00000001
#define lookupOR_1 0b00000010
#define lookupOR_2 0b00000100
#define lookupOR_3 0b00001000
#define lookupOR_4 0b00010000
#define lookupOR_5 0b00100000
#define lookupOR_6 0b01000000
#define lookupOR_7 0b10000000

#define lookupAND_0 0b11111110
#define lookupAND_1 0b11111101
#define lookupAND_2 0b11111011
#define lookupAND_3 0b11110111
#define lookupAND_4 0b11101111
#define lookupAND_5 0b11011111
#define lookupAND_6 0b10111111
#define lookupAND_7 0b01111111
void fillKeysStruct(void)
{
    USB_DEV_CONTROL.JOY1_BUTT1 = 1;
    USB_DEV_CONTROL.JOY1_BUTT2 = 1;
    USB_DEV_CONTROL.JOY1_X_AXIS = 128;
    USB_DEV_CONTROL.JOY1_Y_AXIS = 128;
    USB_DEV_CONTROL.JOY2_BUTT1 = 1;
    USB_DEV_CONTROL.JOY2_BUTT2 = 1;
    USB_DEV_CONTROL.JOY2_X_AXIS = 128;
    USB_DEV_CONTROL.JOY2_Y_AXIS = 128;
    memset(USB_DEV_CONTROL.JOYSTICK_REPORT, 0, sizeof(USB_DEV_CONTROL.JOYSTICK_REPORT));
    memset(USB_DEV_CONTROL.JOYSTICK_REPORT_LENGTH, 0, sizeof(USB_DEV_CONTROL.JOYSTICK_REPORT_LENGTH));

    
    USB_DEV_CONTROL.PORT_JOY1 = 1;
    USB_DEV_CONTROL.PORT_JOY2 = 2;
    USB_DEV_CONTROL.PORT_KEYBOARD = 0;
    for (uint16_t u=0; u!=256; u++)
    {
        USB_DEV_CONTROL.ScanArray[u] = 0;
    }


    USB_DEV_CONTROL.ScanArray[0x29] = K_ESC;
    USB_DEV_CONTROL.ScanArray[0x3a] = K_F1;
    USB_DEV_CONTROL.ScanArray[0x3b] = K_F2;
    USB_DEV_CONTROL.ScanArray[0x1e] = '1';
    USB_DEV_CONTROL.ScanArray[0x1f] = '2';
    USB_DEV_CONTROL.ScanArray[0x20] = '3';
    USB_DEV_CONTROL.ScanArray[0x21] = '4';
    USB_DEV_CONTROL.ScanArray[0x22] = '5';
    USB_DEV_CONTROL.ScanArray[0x23] = '6';
    USB_DEV_CONTROL.ScanArray[0x24] = '7';
    USB_DEV_CONTROL.ScanArray[0x25] = '8';
    USB_DEV_CONTROL.ScanArray[0x26] = '9';
    USB_DEV_CONTROL.ScanArray[0x27] = '0';
    USB_DEV_CONTROL.ScanArray[0x2d] = '-';
    USB_DEV_CONTROL.ScanArray[0x2e] = '=';
    USB_DEV_CONTROL.ScanArray[0x2a] = K_BACKSPACE;
    USB_DEV_CONTROL.ScanArray[0x14] = 'Q';
    USB_DEV_CONTROL.ScanArray[0x1a] = 'W';
    USB_DEV_CONTROL.ScanArray[0x08] = 'E';
    USB_DEV_CONTROL.ScanArray[0x15] = 'R';
    USB_DEV_CONTROL.ScanArray[0x17] = 'T';
    USB_DEV_CONTROL.ScanArray[0x1c] = 'Y';
    USB_DEV_CONTROL.ScanArray[0x18] = 'U';
    USB_DEV_CONTROL.ScanArray[0x0c] = 'I';
    USB_DEV_CONTROL.ScanArray[0x12] = 'O';
    USB_DEV_CONTROL.ScanArray[0x13] = 'P';
    USB_DEV_CONTROL.ScanArray[0x04] = 'A';
    USB_DEV_CONTROL.ScanArray[0x16] = 'S';
    USB_DEV_CONTROL.ScanArray[0x07] = 'D';
    USB_DEV_CONTROL.ScanArray[0x09] = 'F';
    USB_DEV_CONTROL.ScanArray[0x0a] = 'G';
    USB_DEV_CONTROL.ScanArray[0x0b] = 'H';
    USB_DEV_CONTROL.ScanArray[0x0d] = 'J';
    USB_DEV_CONTROL.ScanArray[0x0e] = 'K';
    USB_DEV_CONTROL.ScanArray[0x0f] = 'L';
    USB_DEV_CONTROL.ScanArray[0x33] = ';';
    USB_DEV_CONTROL.ScanArray[0x34] = K_APOSTROPHE;
    USB_DEV_CONTROL.ScanArray[0x28] = K_ENTER;
    USB_DEV_CONTROL.ScanArray[0x1d] = 'Z';
    USB_DEV_CONTROL.ScanArray[0x1b] = 'X';
    USB_DEV_CONTROL.ScanArray[0x06] = 'C';
    USB_DEV_CONTROL.ScanArray[0x19] = 'V';
    USB_DEV_CONTROL.ScanArray[0x05] = 'B';
    USB_DEV_CONTROL.ScanArray[0x11] = 'N';
    USB_DEV_CONTROL.ScanArray[0x10] = 'M';
    USB_DEV_CONTROL.ScanArray[0x36] = ',';
    USB_DEV_CONTROL.ScanArray[0x37] = '.';
    USB_DEV_CONTROL.ScanArray[0x38] = '/';
    USB_DEV_CONTROL.ScanArray[0x39] = K_CAPS;  //SHIFT 0 on CP400

    USB_DEV_CONTROL.ScanArray[0x52] = K_UP;
    USB_DEV_CONTROL.ScanArray[0x51] = K_DOWN;
    USB_DEV_CONTROL.ScanArray[0x50] = K_LEFT;
    USB_DEV_CONTROL.ScanArray[0x4f] = K_RIGHT;


    USB_DEV_CONTROL.ScanArray[0x80 | 0x1e] = '!';
    USB_DEV_CONTROL.ScanArray[0x80 | 0x1f] = '@';
    USB_DEV_CONTROL.ScanArray[0x80 | 0x20] = '#';
    USB_DEV_CONTROL.ScanArray[0x80 | 0x21] = '$';
    USB_DEV_CONTROL.ScanArray[0x80 | 0x22] = '%';
    USB_DEV_CONTROL.ScanArray[0x80 | 0x23] = '^';
    USB_DEV_CONTROL.ScanArray[0x80 | 0x24] = '&';
    USB_DEV_CONTROL.ScanArray[0x80 | 0x25] = '*';
    USB_DEV_CONTROL.ScanArray[0x80 | 0x26] = '(';
    USB_DEV_CONTROL.ScanArray[0x80 | 0x27] = ')';
    USB_DEV_CONTROL.ScanArray[0x80 | 0x2d] = '_';
    USB_DEV_CONTROL.ScanArray[0x80 | 0x2e] = '+';
    USB_DEV_CONTROL.ScanArray[0x80 | 0x34] = K_QUOTE;
    USB_DEV_CONTROL.ScanArray[0x80 | 0x33] = ':';
    USB_DEV_CONTROL.ScanArray[0x80 | 0x36] = '<';
    USB_DEV_CONTROL.ScanArray[0x80 | 0x37] = '>';
    USB_DEV_CONTROL.ScanArray[0x80 | 0x38] = '?';
    USB_DEV_CONTROL.ScanArray[0x2c] = K_SPACE;
    USB_DEV_CONTROL.ScanArray[0x4c] = K_CLEAR;

    USB_DEV_CONTROL.ScanArray[0x80 | 0x82] = K_SHIFT;
    USB_DEV_CONTROL.ScanArray[0x80 | 0x81] = K_CTRL;
    USB_DEV_CONTROL.ScanArray[0x80 | 0x84] = K_ALT;



}



void UpdateKeyMap(uint8_t * Data)
{
    bool ShiftDisabled = false;
    bool ForceShift = false;
    uint8_t SpecialKey;
    uint8_t KeyTranslated = 0;
    uint8_t loop1;
    SpecialKey = (((Data[0] >> 4) | (Data[0] & 0x0f)) | 0x80);
    for (uint8_t u = 0; u!=8; u++)
    {
        USB_DEV_CONTROL.USB_CP400_Key_Array[u] = 0;
    }
    

    

    //Special keys case
    if (SpecialKey !=0) //If Special key and no key press
    {
        switch (SpecialKey)
        {
        case K_SHIFT:
            USB_DEV_CONTROL.USB_CP400_Key_Array[7] |= lookupOR_6;
            
            break;
        case K_CTRL:
            USB_DEV_CONTROL.USB_CP400_Key_Array[4] |= lookupOR_6;
            break;
        case K_ALT:
            USB_DEV_CONTROL.USB_CP400_Key_Array[3] |= lookupOR_6;
            break;
        default:
            break;
        }
    }
    
    sf.DIRECT_Key_Code = Data[2];
    
    for (loop1=2; loop1 !=7; loop1++)
    {
        KeyTranslated = Data[loop1];
        if (SpecialKey == K_SHIFT)
        {
            KeyTranslated = (KeyTranslated | 0b10000000);
        }
        
        KeyTranslated = USB_DEV_CONTROL.ScanArray[KeyTranslated];
        
        
        
        switch (KeyTranslated)
        {
        case 'H':
            USB_DEV_CONTROL.USB_CP400_Key_Array[0] |= lookupOR_1;
            break;
        case 'P':
            USB_DEV_CONTROL.USB_CP400_Key_Array[0] |= lookupOR_2;
            break;
        case 'X':
            USB_DEV_CONTROL.USB_CP400_Key_Array[0] |= lookupOR_3;
            break;
        case '0':
            USB_DEV_CONTROL.USB_CP400_Key_Array[0] |= lookupOR_4;
            break;
        case '8':
            USB_DEV_CONTROL.USB_CP400_Key_Array[0] |= lookupOR_5;
            break;
        case K_ENTER:
            USB_DEV_CONTROL.USB_CP400_Key_Array[0] |= lookupOR_6;
            break;
        //-------------------------------------------
        case 'A':
            USB_DEV_CONTROL.USB_CP400_Key_Array[1] |= lookupOR_0;
            break;
        case 'I':
            USB_DEV_CONTROL.USB_CP400_Key_Array[1] |= lookupOR_1;
            break;
        case 'Q':
            USB_DEV_CONTROL.USB_CP400_Key_Array[1] |= lookupOR_2;
            break;
        case 'Y':
            USB_DEV_CONTROL.USB_CP400_Key_Array[1] |= lookupOR_3;
            break;
        case '1':
        case '!':
            USB_DEV_CONTROL.USB_CP400_Key_Array[1] |= lookupOR_4;
            break;
        case '9':
            USB_DEV_CONTROL.USB_CP400_Key_Array[1] |= lookupOR_5;
            break;
        case K_CLEAR:
            USB_DEV_CONTROL.USB_CP400_Key_Array[1] |= lookupOR_6;
            break;
        //-------------------------------------------
        case 'B':
            USB_DEV_CONTROL.USB_CP400_Key_Array[2] |= lookupOR_0;
            break;
        case 'J':
            USB_DEV_CONTROL.USB_CP400_Key_Array[2] |= lookupOR_1;
            break;
        case 'R':
            USB_DEV_CONTROL.USB_CP400_Key_Array[2] |= lookupOR_2;
            break;
        case 'Z':
            USB_DEV_CONTROL.USB_CP400_Key_Array[2] |= lookupOR_3;
            break;
        case '2':
            USB_DEV_CONTROL.USB_CP400_Key_Array[2] |= lookupOR_4;
            break;
        case K_ESC:
            USB_DEV_CONTROL.USB_CP400_Key_Array[2] |= lookupOR_6;
            break;
        //-------------------------------------------
        case 'C':
            USB_DEV_CONTROL.USB_CP400_Key_Array[3] |= lookupOR_0;
            break;
        case 'K':
            USB_DEV_CONTROL.USB_CP400_Key_Array[3] |= lookupOR_1;
            break;
        case 'S':
            USB_DEV_CONTROL.USB_CP400_Key_Array[3] |= lookupOR_2;
            break;
        case K_UP:
            USB_DEV_CONTROL.USB_CP400_Key_Array[3] |= lookupOR_3;
            break;
        case '3':
        case '#':
            USB_DEV_CONTROL.USB_CP400_Key_Array[3] |= lookupOR_4;
            break;
        case ';':
            USB_DEV_CONTROL.USB_CP400_Key_Array[3] |= lookupOR_5;
            break;
        //-------------------------------------------
        case 'D':
            USB_DEV_CONTROL.USB_CP400_Key_Array[4] |= lookupOR_0;
            break;
        case 'L':
            USB_DEV_CONTROL.USB_CP400_Key_Array[4] |= lookupOR_1;
            break;
        case 'T':
            USB_DEV_CONTROL.USB_CP400_Key_Array[4] |= lookupOR_2;
            break;
        case K_DOWN:
            USB_DEV_CONTROL.USB_CP400_Key_Array[4] |= lookupOR_3;
            break;
        case '4':
        case '$':
            USB_DEV_CONTROL.USB_CP400_Key_Array[4] |= lookupOR_4;
            break;
        case ',':
        case '<':
            USB_DEV_CONTROL.USB_CP400_Key_Array[4] |= lookupOR_5;
            break;
        //-------------------------------------------
        case 'E':
            USB_DEV_CONTROL.USB_CP400_Key_Array[5] |= lookupOR_0;
            break;
        case 'M':
            USB_DEV_CONTROL.USB_CP400_Key_Array[5] |= lookupOR_1;
            break;
        case 'U':
            USB_DEV_CONTROL.USB_CP400_Key_Array[5] |= lookupOR_2;
            break;
        case K_LEFT:
        case K_BACKSPACE:
            USB_DEV_CONTROL.USB_CP400_Key_Array[5] |= lookupOR_3;
            break;
        case '5':
        case '%':
            USB_DEV_CONTROL.USB_CP400_Key_Array[5] |= lookupOR_4;
            break;
        case '-':
            USB_DEV_CONTROL.USB_CP400_Key_Array[5] |= lookupOR_5;
            break;
        case K_F1:
            USB_DEV_CONTROL.USB_CP400_Key_Array[5] |= lookupOR_6;
            break;
        //-------------------------------------------
        case 'F':
            USB_DEV_CONTROL.USB_CP400_Key_Array[6] |= lookupOR_0;
            break;
        case 'N':
            USB_DEV_CONTROL.USB_CP400_Key_Array[6] |= lookupOR_1;
            break;
        case 'V':
            USB_DEV_CONTROL.USB_CP400_Key_Array[6] |= lookupOR_2;
            break;
        case K_RIGHT:
            USB_DEV_CONTROL.USB_CP400_Key_Array[6] |= lookupOR_3;
            break;
        case '6':
            USB_DEV_CONTROL.USB_CP400_Key_Array[6] |= lookupOR_4;
            break;
        case '.':
        case '>':
            USB_DEV_CONTROL.USB_CP400_Key_Array[6] |= lookupOR_5;
            break;
        case K_F2:
            USB_DEV_CONTROL.USB_CP400_Key_Array[6] |= lookupOR_6;
            break;
        //-------------------------------------------
        case 'G':
            USB_DEV_CONTROL.USB_CP400_Key_Array[7] |= lookupOR_0;
            break;
        case 'O':
            USB_DEV_CONTROL.USB_CP400_Key_Array[7] |= lookupOR_1;
            break;
        case 'W':
            USB_DEV_CONTROL.USB_CP400_Key_Array[7] |= lookupOR_2;
            break;
        case K_SPACE:
            USB_DEV_CONTROL.USB_CP400_Key_Array[7] |= lookupOR_3;
            break;
        case '7':
            USB_DEV_CONTROL.USB_CP400_Key_Array[7] |= lookupOR_4;
            break;
        case '/':
            USB_DEV_CONTROL.USB_CP400_Key_Array[7] |= lookupOR_5;
            break;
        case '&':
            USB_DEV_CONTROL.USB_CP400_Key_Array[6] |= lookupOR_4;
            break;
        case '?':
            USB_DEV_CONTROL.USB_CP400_Key_Array[7] |= lookupOR_5;
            break;
        case '@':
            USB_DEV_CONTROL.USB_CP400_Key_Array[0] |= lookupOR_0;
            ShiftDisabled = true;
            break;
        case '*':
            USB_DEV_CONTROL.USB_CP400_Key_Array[2] |= lookupOR_5;
            break;
        case '(':
            USB_DEV_CONTROL.USB_CP400_Key_Array[0] |= lookupOR_5;
            break;
        case ')':
            USB_DEV_CONTROL.USB_CP400_Key_Array[1] |= lookupOR_5;
            break;
        case '=':
            USB_DEV_CONTROL.USB_CP400_Key_Array[5] |= lookupOR_5;
            ForceShift = true;
            break;
        case '+':
            USB_DEV_CONTROL.USB_CP400_Key_Array[3] |= lookupOR_5;
            ForceShift = true;
            break;
        case K_CAPS:
            USB_DEV_CONTROL.USB_CP400_Key_Array[0] |= lookupOR_4;
            ForceShift = true;
            break;
        case ':':
            USB_DEV_CONTROL.USB_CP400_Key_Array[2] |= lookupOR_5;
            ShiftDisabled = true;
            break;
        case K_APOSTROPHE:
            USB_DEV_CONTROL.USB_CP400_Key_Array[7] |= lookupOR_4;
            ForceShift = true;
            break;
        case K_QUOTE:
            USB_DEV_CONTROL.USB_CP400_Key_Array[2] |= lookupOR_4;
            ForceShift = true;
            break;

        


            //-------------------------------------------
        
        default:
            break;
        }
        if (ShiftDisabled == true)
        {
            USB_DEV_CONTROL.USB_CP400_Key_Array[7] &= lookupAND_6;
        }
        if (ForceShift == true)
        {
            USB_DEV_CONTROL.USB_CP400_Key_Array[7] |= lookupOR_6;
        }
    }

}

void PrintJoystickState(uint8_t joystick)
{
  const uint8_t reportLength = USB_DEV_CONTROL.JOYSTICK_REPORT_LENGTH[joystick];
  const uint8_t *report = USB_DEV_CONTROL.JOYSTICK_REPORT[joystick];

  printf("JOY%u raw[%u]:", joystick + 1, reportLength);
  for (uint8_t index = 0; index < reportLength; index++)
  {
    printf(" %02X", report[index]);
  }

  if (USB_DEV_CONTROL.JOYSTICK_CALIBRATION[joystick].valid)
  {
    printf("  U%u D%u L%u R%u B1:%u B2:%u",
           IsJoystickControlActive(joystick, JOYSTICK_UP) ? 1 : 0,
           IsJoystickControlActive(joystick, JOYSTICK_DOWN) ? 1 : 0,
           IsJoystickControlActive(joystick, JOYSTICK_LEFT) ? 1 : 0,
           IsJoystickControlActive(joystick, JOYSTICK_RIGHT) ? 1 : 0,
           IsJoystickControlActive(joystick, JOYSTICK_BUTTON_1) ? 1 : 0,
           IsJoystickControlActive(joystick, JOYSTICK_BUTTON_2) ? 1 : 0);
  }
  else
  {
    printf("  [uncalibrated]");
  }

  printf("  J1 X=%u Y=%u F=%u%u  J2 X=%u Y=%u F=%u%u\n",
         USB_DEV_CONTROL.JOY1_X_AXIS, USB_DEV_CONTROL.JOY1_Y_AXIS,
         USB_DEV_CONTROL.JOY1_BUTT1, USB_DEV_CONTROL.JOY1_BUTT2,
         USB_DEV_CONTROL.JOY2_X_AXIS, USB_DEV_CONTROL.JOY2_Y_AXIS,
         USB_DEV_CONTROL.JOY2_BUTT1, USB_DEV_CONTROL.JOY2_BUTT2);
}

void UpdateJoyMap(uint8_t * Data, uint8_t dataLength, uint8_t usbNum)
{
  if (usbNum != USB_DEV_CONTROL.PORT_JOY1 && usbNum != USB_DEV_CONTROL.PORT_JOY2)
  {
    return;
  }

  const uint8_t joystick = usbNum == USB_DEV_CONTROL.PORT_JOY1 ? 0 : 1;
  const uint8_t reportLength = min(dataLength, static_cast<uint8_t>(JOYSTICK_REPORT_SIZE));

  memcpy(USB_DEV_CONTROL.JOYSTICK_REPORT[joystick], Data, reportLength);
  memset(USB_DEV_CONTROL.JOYSTICK_REPORT[joystick] + reportLength, 0,
         JOYSTICK_REPORT_SIZE - reportLength);
  USB_DEV_CONTROL.JOYSTICK_REPORT_LENGTH[joystick] = reportLength;

  uint8_t xAxis;
  uint8_t yAxis;
  uint8_t button1;
  uint8_t button2;

  if (USB_DEV_CONTROL.JOYSTICK_CALIBRATION[joystick].valid)
  {
    const bool up = IsJoystickControlActive(joystick, JOYSTICK_UP);
    const bool down = IsJoystickControlActive(joystick, JOYSTICK_DOWN);
    const bool left = IsJoystickControlActive(joystick, JOYSTICK_LEFT);
    const bool right = IsJoystickControlActive(joystick, JOYSTICK_RIGHT);
    xAxis = (left == right) ? 128 : (left ? 0 : 255);
    yAxis = (up == down) ? 128 : (up ? 0 : 255);
    button1 = IsJoystickControlActive(joystick, JOYSTICK_BUTTON_1) ? 0 : 1;
    button2 = IsJoystickControlActive(joystick, JOYSTICK_BUTTON_2) ? 0 : 1;
  }
  else if (dataLength >= 2)
  {
    const uint8_t buttonBits = dataLength > 4 ? Data[4] : 0;
    xAxis = Data[0];
    yAxis = Data[1];
    button1 = (buttonBits & 0x10) ? 0 : 1;
    button2 = (buttonBits & 0x20) ? 0 : 1;
  }
  else
  {
    return;
  }

  if (joystick == 0)
  {
    USB_DEV_CONTROL.JOY1_X_AXIS = xAxis;
    USB_DEV_CONTROL.JOY1_Y_AXIS = yAxis;
    USB_DEV_CONTROL.JOY1_BUTT1 = button1;
    USB_DEV_CONTROL.JOY1_BUTT2 = button2;
  }
  else
  {
    USB_DEV_CONTROL.JOY2_X_AXIS = xAxis;
    USB_DEV_CONTROL.JOY2_Y_AXIS = yAxis;
    USB_DEV_CONTROL.JOY2_BUTT1 = button1;
    USB_DEV_CONTROL.JOY2_BUTT2 = button2;
  }

  //One pad drives both CP400 axis pairs, since games differ on which joystick they read.
  if (USB_DEV_CONTROL.JOYSTICK_REPORT_LENGTH[joystick ^ 1] == 0)
  {
    if (joystick == 0)
    {
      USB_DEV_CONTROL.JOY2_X_AXIS = xAxis;
      USB_DEV_CONTROL.JOY2_Y_AXIS = yAxis;
    }
    else
    {
      USB_DEV_CONTROL.JOY1_X_AXIS = xAxis;
      USB_DEV_CONTROL.JOY1_Y_AXIS = yAxis;
    }
  }
}

//-----------------End USB CP400 MAP--------------------------------------------
