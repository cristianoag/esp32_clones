#include "MsxCore.h"
#include "MsxPlatform.h"
#include "Esp32Port.h"
#include "MSX.h"
#include "Sound.h"
#include <esp_heap_caps.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifdef ESP_PLATFORM
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

// Common.h needs 8 pixels of side border for the VDP horizontal-adjust register.
#define WIDTH 272
#define HEIGHT 240
static_assert(sizeof(pixel) == 1, "fMSX requires BPP8");
static pixel* XBuf;
static uint8_t* output;
static unsigned int XPal[80], BPal[256], XPal0;
static uint32_t rgbPalette[256];
static char biosDirectory[256];
static unsigned audioFraction;
static bool running;
#ifdef ESP_PLATFORM
static int64_t nextFrameDeadline;
#endif
static void PutImage();
#include "Common.h"

extern "C" void* fmsxAllocate(size_t size)
{
  return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

extern "C" FILE* fmsxOpen(const char* name, const char* mode)
{
  if (!name) { errno = EINVAL; return nullptr; }
  if (name[0] == '/') return fopen(name, mode);
  char path[512];
  const int n = snprintf(path, sizeof(path), "%s/%s", biosDirectory, name);
  if (n < 0 || n >= static_cast<int>(sizeof(path))) {
    errno = ENAMETOOLONG;
    return nullptr;
  }
  return fopen(path, mode);
}

static void PutImage()
{
#ifdef ESP_PLATFORM
  // Always let the core's idle task run, including when emulation is over budget.
  vTaskDelay(1);
  const int64_t period = PALVideo ? 20000 : 16667;
  int64_t now = esp_timer_get_time();
  // A blocking firmware menu must not be followed by a burst of catch-up frames.
  if (!nextFrameDeadline || now - nextFrameDeadline > period)
    nextFrameDeadline = now;
  while (now < nextFrameDeadline) {
    TickType_t ticks = pdMS_TO_TICKS((nextFrameDeadline - now + 999) / 1000);
    vTaskDelay(ticks ? ticks : 1);
    now = esp_timer_get_time();
  }
  nextFrameDeadline += period;
#endif
  for (int y = 0; y < HEIGHT; ++y)
    memcpy(output + y * 256, XBuf + y * WIDTH + 8, 256);
  msxPresent(output, 256, HEIGHT, rgbPalette);
  if (msxShouldExit()) ExitNow = 1;
}

extern "C" void SetColor(byte n, byte r, byte g, byte b)
{
  if (n >= 80) return;
  // A fixed GGGRRRBB palette preserves scanline palette changes and SCREEN 8/YJK.
  XPal[n] = (g & 0xE0) | ((r & 0xE0) >> 3) | (b >> 6);
  if (!n) XPal0 = XPal[n];
}

extern "C" void Keyboard()
{
  uint8_t matrix[16];
  memset(matrix, 0xFF, sizeof(matrix));
  msxPollKeyboard(matrix);
  for (unsigned i = 0; i < sizeof(matrix); ++i) KeyState[i] = matrix[i];
  if (msxShouldExit()) ExitNow = 1;
}

extern "C" unsigned int Joystick() { return 0; }
extern "C" unsigned int Mouse(byte) { return 0; }
extern "C" unsigned int InitAudio(unsigned int rate, unsigned int) { return rate; }
extern "C" void TrashAudio() {}
extern "C" unsigned int GetFreeAudio() { return 1024; }
extern "C" unsigned int WriteAudio(sample* data, unsigned int count)
{
  static_assert(sizeof(sample) == sizeof(int16_t), "fMSX requires BPS16");
  msxSubmitAudio(data, count);
  return count;
}

extern "C" void PlayAllSound(int microseconds)
{
  audioFraction += static_cast<unsigned>(microseconds) * 22050U;
  const unsigned samples = audioFraction / 1000000U;
  audioFraction %= 1000000U;
  if (samples) RenderAndPlayAudio(samples);
}

bool MsxCoreRun(const char* romDirectory, int model, int ramPages)
{
  if (running || !romDirectory || romDirectory[0] != '/' ||
      strlen(romDirectory) >= sizeof(biosDirectory) || model < 0 || model > 2 ||
      (ramPages != 4 && ramPages != 8 && ramPages != 16 && ramPages != 32)) {
    msxReportError("Invalid MSX model, RAM size, BIOS directory, or concurrent run.");
    return false;
  }
  running = true;
  strcpy(biosDirectory, romDirectory);
  XBuf = static_cast<pixel*>(fmsxAllocate(WIDTH * HEIGHT * sizeof(pixel)));
  output = static_cast<uint8_t*>(fmsxAllocate(256 * HEIGHT));
  if (!XBuf || !output) {
    heap_caps_free(XBuf);
    heap_caps_free(output);
    XBuf = nullptr;
    output = nullptr;
    running = false;
    msxReportError("Not enough PSRAM for MSX video.");
    return false;
  }
  memset(XBuf, 0, WIDTH * HEIGHT * sizeof(pixel));
  memset(XPal, 0, sizeof(XPal));
  XPal0 = 0;
  for (unsigned i = 0; i < 256; ++i) {
    BPal[i] = i;
    rgbPalette[i] = (((i >> 2) & 7) * 255 / 7 << 16) |
                    (((i >> 5) & 7) * 255 / 7 << 8) | ((i & 3) * 255 / 3);
  }
  audioFraction = 0;
#ifdef ESP_PLATFORM
  nextFrameDeadline = 0;
#endif
  InitSound(22050, 0);
  SetNoise(0x10000, 16, 14);
  SetChannels(128, (1 << MAXCHANNELS) - 1);
  // Use explicit profile-root resolution rather than fMSX's desktop chdir model.
  ProgDir = nullptr;
  Verbose = 0;
  UPeriod = 100;
  for (int i = 0; i < MAXCARTS; ++i) ROMName[i] = nullptr;
  for (int i = 0; i < MAXDRIVES; ++i) DSKName[i] = nullptr;
  SndName = PrnName = CasName = ComName = STAName = FNTName = nullptr;
  CPU.Trap = 0xFFFF;
  CPU.Trace = 0;
  const bool result = StartMSX(model | MSX_NTSC, ramPages, model ? 8 : 2) != 0;
  ResetVDP();
  TrashMSX();
  TrashSound();
  heap_caps_free(XBuf);
  heap_caps_free(output);
  XBuf = nullptr;
  output = nullptr;
  running = false;
  if (!result) msxReportError("MSX boot failed: check BIOS filenames, sizes and free PSRAM.");
  return result;
}
