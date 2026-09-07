// Host lifecycle regression: executes synthetic Z80 code, not a copyrighted BIOS.
#include "../../../src/MsxCore.h"
#include "../../../src/MsxPlatform.h"
#include "../Esp32Port.h"
#include "MSX.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

static unsigned frames, audioSamples, nonzeroSamples, coloredFrames, errors, allocation;
static unsigned failAllocation;
static unsigned frameLimit = 3;
static bool realBios;
static bool basicPrompt;

static void captureBoot(const uint8_t* pixels, int width, int height,
                        const uint32_t* palette)
{
  FILE* f = fopen("boot.ppm", "wb");
  assert(f);
  fprintf(f, "P6\n%d %d\n255\n", width, height);
  for (int i = 0; i < width * height; ++i) {
    const uint32_t color = palette[pixels[i]];
    const unsigned char rgb[] = {
      static_cast<unsigned char>(color >> 16),
      static_cast<unsigned char>(color >> 8),
      static_cast<unsigned char>(color)
    };
    assert(fwrite(rgb, 1, sizeof(rgb), f) == sizeof(rgb));
  }
  fclose(f);
  printf("Final screen: mode=%u, PC=%04X, RAM=%d KiB\n", ScrMode, CPU.PC.W, RAMPages * 16);
  if (ScrMode == 0 || ScrMode == 1) {
    const int columns = ScrMode == 0 ? 40 : 32;
    for (int row = 0; row < 24; ++row) {
      char text[41];
      for (int col = 0; col < columns; ++col) {
        const unsigned char c = ChrTab[row * columns + col];
        text[col] = c >= 32 && c < 127 ? c : ' ';
      }
      text[columns] = 0;
      if (strstr(text, "Ok")) basicPrompt = true;
      printf("%s\n", text);
    }
  }
}

extern "C" void* heap_caps_malloc(size_t size, unsigned)
{
  if (++allocation == failAllocation) return nullptr;
  return malloc(size);
}
extern "C" void heap_caps_free(void* ptr) { free(ptr); }

void msxPresent(const uint8_t* pixels, int width, int height, const uint32_t* palette)
{
  assert(pixels && palette && width == 256 && height == 240);
  assert(palette[255] == 0xFFFFFF);
  for (int i = 0; i < width * height; ++i)
    if (pixels[i]) { ++coloredFrames; break; }
  ++frames;
  if (realBios && frames == frameLimit) captureBoot(pixels, width, height, palette);
}
void msxPollKeyboard(uint8_t matrix[16]) { if (!realBios) matrix[0] = 0xFE; }
bool msxShouldExit() { return frames >= frameLimit; }
void msxSubmitAudio(const int16_t* samples, unsigned count)
{
  assert(samples);
  for (unsigned i = 0; i < count; ++i) if (samples[i]) ++nonzeroSamples;
  audioSamples += count;
}
void msxReportError(const char* text)
{
  ++errors;
  if (realBios) fprintf(stderr, "%s\n", text);
}

int main(int argc, char** argv)
{
  if (argc > 1) {
    assert(argc == 5);
    realBios = true;
    frameLimit = static_cast<unsigned>(atoi(argv[4]));
    assert(frameLimit > 0);
    const bool result = MsxCoreRun(argv[1], atoi(argv[2]), atoi(argv[3]));
    printf("BIOS boot: result=%d, frames=%u, samples=%u, BASIC prompt=%s\n",
           result, frames, audioSamples, basicPrompt ? "yes" : "not detected");
    return result && frames == frameLimit && basicPrompt ? 0 : 1;
  }
  char directory[1024];
  assert(getcwd(directory, sizeof(directory)));
  for (char* p = directory; *p; ++p) if (*p == '\\') *p = '/';
  // On Windows a leading slash addresses the current drive's root.
  const char* path = directory[1] == ':' ? directory + 2 : directory;
  const char* names[] = {"MSX.ROM","MSX2.ROM","MSX2EXT.ROM","MSX2P.ROM","MSX2PEXT.ROM"};
  unsigned char rom[32768] = {};
  // Enable PSG tone A and a blue VDP background, then HALT without interrupts.
  const unsigned char code[] = {
    0xF3,
    0x3E,0x00,0xD3,0xA0,0x3E,0x40,0xD3,0xA1,
    0x3E,0x07,0xD3,0xA0,0x3E,0x3E,0xD3,0xA1,
    0x3E,0x08,0xD3,0xA0,0x3E,0x0F,0xD3,0xA1,
    0x3E,0x00,0xD3,0x99,0x3E,0x82,0xD3,0x99,
    0x3E,0x00,0xD3,0x99,0x3E,0x83,0xD3,0x99,
    0x3E,0x00,0xD3,0x99,0x3E,0x84,0xD3,0x99,
    0x3E,0x00,0xD3,0x99,0x3E,0x85,0xD3,0x99,
    0x3E,0x00,0xD3,0x99,0x3E,0x86,0xD3,0x99,
    0x3E,0xF4,0xD3,0x99,0x3E,0x87,0xD3,0x99,
    0x3E,0x40,0xD3,0x99,0x3E,0x81,0xD3,0x99,
    0x76,0xC3,0x51,0x00
  };
  memcpy(rom, code, sizeof(code));
  for (unsigned i = 0; i < 5; ++i) {
    FILE* f = fopen(names[i], "wb");
    assert(f);
    const size_t size = (i == 2 || i == 4) ? 16384 : sizeof(rom);
    assert(fwrite(rom, 1, size, f) == size);
    fclose(f);
  }
  for (int pass = 0; pass < 2; ++pass)
    for (int model = 0; model < 3; ++model)
      for (int pages = 4; pages <= 32; pages *= 2) {
        frames = audioSamples = errors = allocation = failAllocation = 0;
        nonzeroSamples = coloredFrames = 0;
        assert(MsxCoreRun(path, model, pages));
        assert(frames == 3 && audioSamples > 0 && errors == 0);
        assert(nonzeroSamples > 0 && coloredFrames > 0);
        assert(RAMPages == pages);
        assert(KeyState[0] == 0xFE);
      }
  for (unsigned failure = 1; failure <= 7; ++failure) {
    frames = audioSamples = errors = allocation = 0;
    failAllocation = failure;
    assert(!MsxCoreRun(path, 2, 32));
    assert(errors == 1);
    frames = audioSamples = errors = allocation = failAllocation = 0;
    assert(MsxCoreRun(path, 0, 4));
  }
  for (const char* name : names) assert(remove(name) == 0);
  frames = errors = 0;
  assert(!MsxCoreRun(path, 2, 8));
  assert(errors == 1);
  puts("PASS: 24 boots, 7 allocation failures/recoveries, missing BIOS, input, audio, video.");
}
