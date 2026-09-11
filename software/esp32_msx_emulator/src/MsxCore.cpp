#include "MsxCore.h"
#include "MsxPlatform.h"
#include "Esp32Port.h"
#include "FramePacer.h"
#include "MSX.h"
#include "Sound.h"
#include <esp_heap_caps.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
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
static bool allocationFailed;
#ifdef ESP_PLATFORM
static FramePacer framePacer;
static int64_t lastYield, speedStart;
static unsigned speedFrames, speedPresented;
#endif
static void PutImage();
#include "Common.h"

extern "C" void* fmsxAllocate(size_t size)
{
  void* p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p) { allocationFailed = true; errno = ENOMEM; }
  return p;
}

extern "C" void* fmsxAllocateCpu(size_t size)
{
#ifdef ESP_PLATFORM
  // Keep video/large mapper buffers in PSRAM and leave headroom for other tasks.
  constexpr size_t reserve = 64U * 1024U;
  constexpr unsigned caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
  void* p = nullptr;
  if (size <= 64U * 1024U && heap_caps_get_free_size(caps) >= size + reserve)
    p = heap_caps_malloc(size, caps);
  if (p) {
    printf("MSX CPU memory: %u bytes in internal SRAM\n", static_cast<unsigned>(size));
    return p;
  }
  printf("MSX CPU memory: %u bytes in PSRAM (size/reserve/fragmentation fallback)\n",
         static_cast<unsigned>(size));
#endif
  return fmsxAllocate(size);
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

extern "C" void fmsxFrame()
{
#ifdef ESP_PLATFORM
  int64_t now = esp_timer_get_time();
  const int64_t wait = framePacer.frame(now, PALVideo);
  const int64_t target = now + wait;
  while (now < target) {
    TickType_t ticks = pdMS_TO_TICKS((target - now) / 1000);
    vTaskDelay(ticks ? ticks : 1);
    now = esp_timer_get_time();
    lastYield = now;
  }
  // Yield periodically when behind, not on every frame (one tick can be costly).
  if (now - lastYield >= 50000) {
    vTaskDelay(1);
    now = esp_timer_get_time();
    lastYield = now;
  }
  UPeriod = framePacer.renderingPercent();
  if (!speedStart) {
    speedStart = now;
    speedFrames = speedPresented = 0;
  } else ++speedFrames;
  const int64_t elapsed = now - speedStart;
  if (elapsed >= 5000000) {
    printf("MSX speed: %.1f/%u emulated fps, %.1f presented fps, draw=%u%%\n",
           speedFrames * 1000000.0 / elapsed, PALVideo ? 50U : 60U,
           speedPresented * 1000000.0 / elapsed, static_cast<unsigned>(UPeriod));
    speedStart = now;
    speedFrames = speedPresented = 0;
  }
  framePacer.released(esp_timer_get_time());
#endif
  if (msxShouldExit()) ExitNow = 1;
}

static void PutImage()
{
  for (int y = 0; y < HEIGHT; ++y)
    memcpy(output + y * 256, XBuf + y * WIDTH + 8, 256);
  msxPresent(output, 256, HEIGHT, rgbPalette);
#ifdef ESP_PLATFORM
  ++speedPresented;
#endif
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
#ifdef ESP_PLATFORM
  const int64_t pollStart = esp_timer_get_time();
#endif
  msxPollKeyboard(matrix);
#ifdef ESP_PLATFORM
  // F12 menus block inside the platform callback. Discard their wall time/debt.
  if (esp_timer_get_time() - pollStart >= 100000) {
    framePacer.reset(true);
    speedStart = 0;
    speedFrames = speedPresented = 0;
  }
#endif
  for (unsigned i = 0; i < sizeof(matrix); ++i) KeyState[i] = matrix[i];
  if (msxShouldExit()) ExitNow = 1;
}

extern "C" unsigned int Joystick() { return msxPollJoysticks() & 0x3f3f; }
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

bool MsxValidateCartridge(const char* path, char* error, size_t errorSize)
{
  if (error && errorSize) error[0] = 0;
  if (!path || !*path) return true;
  const char* problem = nullptr;
  struct stat info;
  if (path[0] != '/')
    problem = "requires an absolute path";
  else if (stat(path, &info) != 0 || !S_ISREG(info.st_mode))
    problem = "file is missing or is not a regular file";
  else if (info.st_size < 8192 || info.st_size > MSX_MAX_CART_BYTES ||
           (info.st_size % 8192) != 0)
    problem = "invalid ROM size (8 KiB multiples, maximum 2 MiB)";
  if (!problem) {
    FILE* file = fopen(path, "rb");
    if (!file) problem = "cannot open cartridge ROM";
    else {
      bool signature = false;
      const long offsets[] = {0, 0x4000, static_cast<long>(info.st_size) - 0x4000};
      for (long offset : offsets) {
        if (offset < 0 || offset + 2 > info.st_size) continue;
        unsigned char header[2];
        if (fseek(file, offset, SEEK_SET) != 0 ||
            fread(header, 1, sizeof(header), file) != sizeof(header)) {
          problem = "cannot read cartridge ROM header";
          break;
        }
#ifdef ESP_PLATFORM
        vTaskDelay(1);
#endif
        if (header[0] == 'A' && header[1] == 'B') { signature = true; break; }
      }
      if (!problem && !signature) problem = "invalid cartridge ROM (AB header not found)";
      fclose(file);
    }
  }
  if (problem && error && errorSize) snprintf(error, errorSize, "%s", problem);
  return !problem;
}

static bool validateCartridge(const char* path, int slot)
{
  char error[128];
  if (MsxValidateCartridge(path, error, sizeof(error))) return true;
  char message[192];
  snprintf(message, sizeof(message), "MSX slot %d: %s.", slot, error);
  msxReportError(message);
  return false;
}

bool MsxCoreRun(const char* romDirectory, int model, int ramPages,
                const char* slot1, const char* slot2)
{
  if (running || !romDirectory || romDirectory[0] != '/' ||
      strlen(romDirectory) >= sizeof(biosDirectory) || model < 0 || model > 2 ||
      (ramPages != 4 && ramPages != 8 && ramPages != 16 && ramPages != 32)) {
    msxReportError("Invalid MSX model, RAM size, BIOS directory, or concurrent run.");
    return false;
  }
  if (!validateCartridge(slot1, 1) || !validateCartridge(slot2, 2)) return false;
  running = true;
  allocationFailed = false;
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
  framePacer.reset();
  lastYield = esp_timer_get_time();
  speedStart = 0;
  speedFrames = speedPresented = 0;
#endif
  InitSound(22050, 0);
  SetNoise(0x10000, 16, 14);
  SetChannels(128, (1 << MAXCHANNELS) - 1);
  // Use explicit profile-root resolution rather than fMSX's desktop chdir model.
  ProgDir = nullptr;
  Verbose = 0;
  UPeriod = 100;
  for (int i = 0; i < MAXCARTS; ++i) ROMName[i] = nullptr;
  ROMName[0] = slot1 && *slot1 ? slot1 : nullptr;
  ROMName[1] = slot2 && *slot2 ? slot2 : nullptr;
  for (int i = 0; i < MAXDRIVES; ++i) DSKName[i] = nullptr;
  SndName = PrnName = CasName = ComName = STAName = FNTName = nullptr;
  CPU.Trap = 0xFFFF;
  CPU.Trace = 0;
  errno = 0;
  const bool result = StartMSX(model | MSX_NTSC | MSX_GUESSA | MSX_GUESSB |
                               (JOY_STICK << 4) | (JOY_STICK << 6),
                               ramPages, model ? 8 : 2) != 0;
  const int bootError = errno;
  ResetVDP();
  TrashMSX();
  for (int i = 0; i < MAXCARTS; ++i) ROMName[i] = nullptr;
  TrashSound();
  heap_caps_free(XBuf);
  heap_caps_free(output);
  XBuf = nullptr;
  output = nullptr;
  running = false;
  if (!result) {
    if (bootError == ENOEXEC || bootError == EFBIG)
      msxReportError("Invalid cartridge ROM: check the AB header and size (maximum 2 MiB).");
    else if (allocationFailed || bootError == ENOMEM)
      msxReportError("Not enough PSRAM to load the MSX BIOS, RAM or selected cartridges.");
    else
      msxReportError("MSX boot failed: could not fully load BIOS or selected cartridge files.");
  }
  return result;
}
