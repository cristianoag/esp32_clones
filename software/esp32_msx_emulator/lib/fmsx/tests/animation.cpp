// Native evidence from a user's BIOS, never a replacement startup screen.
#include "../../../src/MsxCore.h"
#include "../../../src/MsxPlatform.h"
#include "MSX.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include "animation_gif.h"

static unsigned frames, polls, firstLogo, lastLogo, basicFrame, distinct;
static unsigned limit = 900, drawPercent = 100;
static unsigned lastHash;
static unsigned logoWhite, transitions;
static unsigned rasterWrites, rasterLines;
static int lastRasterLine = -1;
static unsigned lastRasterFrame;
static std::set<unsigned> hashes;
static FILE *trace, *raster;
static bool capture, capturedBasic;
static AnimationGif preview;

static void captureImage(const char* name, const uint8_t* pixels, int w, int h,
                         const uint32_t* palette)
{
  FILE* f = fopen(name, "wb");
  assert(f);
  fprintf(f, "P6\n%d %d\n255\n", w, h);
  for (int i = 0; i < w * h; ++i) {
    uint32_t c = palette[pixels[i]];
    const unsigned char rgb[] = {static_cast<unsigned char>(c >> 16),
        static_cast<unsigned char>(c >> 8), static_cast<unsigned char>(c)};
    assert(fwrite(rgb, 1, 3, f) == 3);
  }
  fclose(f);
}

extern "C" void* heap_caps_malloc(size_t size, unsigned) { return malloc(size); }
extern "C" void heap_caps_free(void* p) { free(p); }
uint16_t msxPollJoysticks() { return 0; }
void msxSubmitAudio(const int16_t*, unsigned) {}
void msxReportError(const char* text) { fprintf(stderr, "%s\n", text); }
bool msxShouldExit() { return polls >= limit; }
void msxPollKeyboard(uint8_t*)
{
  ++polls;
  UPeriod = drawPercent;
  if (ScrMode == 6) {
    if (!firstLogo) firstLogo = polls;
    lastLogo = polls;
  }
  if (!basicFrame && (ScrMode == 0 || ScrMode == 1))
    for (unsigned i = 0; i < (ScrMode == 0 ? 960U : 768U) - 1; ++i)
      if (ChrTab[i] == 'O' && ChrTab[i + 1] == 'k') basicFrame = polls;
}

extern "C" byte DebugZ80(Z80*)
{
  static byte old26, old27, old23;
  if (raster && (old26 != VDP[26] || old27 != VDP[27] || old23 != VDP[23])) {
    fprintf(raster, "%u,%d,%d,%04x,%u,%u,%u\n", polls, ScanLine,
            CPU.IPeriod - CPU.ICount, CPU.PC.W, VDP[26], VDP[27], VDP[23]);
    old26 = VDP[26]; old27 = VDP[27]; old23 = VDP[23];
    ++rasterWrites;
    if (lastRasterFrame != polls || lastRasterLine != ScanLine) {
      ++rasterLines;
      lastRasterFrame = polls;
      lastRasterLine = ScanLine;
    }
  }
  return 1;
}

void msxPresent(const uint8_t* pixels, int w, int h, const uint32_t* palette)
{
  ++frames;
  unsigned hash = 2166136261U, white = 0;
  for (int i = 0; i < w * h; ++i) {
    hash = (hash ^ pixels[i]) * 16777619U;
    white += pixels[i] == 255;
  }
  if (ScrMode == 6) {
    if (hashes.insert(hash).second) ++distinct;
    if (hash != lastHash) ++transitions;
    if (white > logoWhite) {
      logoWhite = white;
      if (capture && white > 500) captureImage("animation-logo.ppm", pixels, w, h, palette);
    }
  }
  fprintf(trace, "%u,%u,%u,%04x,%08x,%u,%u,%u,%u\n", polls + 1, frames,
          ScrMode, CPU.PC.W, hash, white, VDP[26], VDP[27], VDP[23]);
  if (capture && firstLogo && !capturedBasic)
    preview.frame(pixels, w, h, palette, polls + 1, basicFrame != 0);
  if (capture && ScrMode == 6 && hash != lastHash) {
    char name[64];
    snprintf(name, sizeof(name), "animation-%04u.ppm", polls + 1);
    captureImage(name, pixels, w, h, palette);
  }
  if (capture && basicFrame && !capturedBasic && (ScrMode == 0 || ScrMode == 1)) {
    captureImage("animation-basic.ppm", pixels, w, h, palette);
    capturedBasic = true;
  }
  lastHash = hash;
  // Enable the existing Z80 debug hook only for optional raster diagnostics.
  if (raster) CPU.Trace = 1;
}

int main(int argc, char** argv)
{
  assert(argc >= 2);
  if (argc > 2) limit = atoi(argv[2]);
  if (argc > 3) drawPercent = atoi(argv[3]);
  capture = getenv("FMSX_CAPTURE") != nullptr;
  trace = fopen("animation.csv", "w");
  assert(trace);
  fprintf(trace, "frame,presented,mode,pc,hash,white,r26,r27,r23\n");
  if (getenv("FMSX_RASTER")) {
    raster = fopen("raster.csv", "w");
    assert(raster);
    fprintf(raster, "frame,line,cycle,pc,r26,r27,r23\n");
  }
  const int ramPages = argc > 4 ? atoi(argv[4]) : 32;
  assert(MsxCoreRun(argv[1], 2, ramPages));
  if (capture) preview.save("animation.gif");
  fclose(trace);
  if (raster) fclose(raster);
  printf("Animation: first=%u last=%u duration=%u frames (%.3fs NTSC), "
         "distinct=%u, transitions=%u, white=%u, presented=%u, BASIC=%u, draw=%u%%\n",
         firstLogo, lastLogo, lastLogo - firstLogo + 1,
         (lastLogo - firstLogo + 1) / 60.0, distinct, transitions, logoWhite,
         frames, basicFrame, drawPercent);
  if (raster) printf("Raster: %u changed-register writes across %u scanlines.\n",
                     rasterWrites, rasterLines);
  fflush(stdout);
  assert(firstLogo && lastLogo > firstLogo && distinct >= 40 * drawPercent / 100);
  assert(transitions + 1 >= distinct && logoWhite > 500 && basicFrame > lastLogo);
}
