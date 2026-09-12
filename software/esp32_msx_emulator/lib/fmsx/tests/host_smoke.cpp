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
#include <string>
#include <vector>
#include <sys/stat.h>
#include <cerrno>
#ifdef _WIN32
#include <io.h>
#endif

extern "C" {
extern AY8910 PSG;
extern I8255 PPI;
extern byte *ROMData[MAXSLOTS], *MemMap[4][4][8], *EmptyRAM, *RAM[8];
extern byte ROMType[MAXSLOTS], ROMMapper[MAXSLOTS][4], ROMMask[MAXSLOTS], SCCOn[2];
extern int NChunks;
extern byte *Kanji;
}

static unsigned frames, audioSamples, nonzeroSamples, coloredFrames, errors, allocation;
static unsigned failAllocation;
static unsigned frameLimit = 3;
static bool realBios;
static bool realCartridge;
static int expectedRealMapper = -1;
static bool basicPrompt;
static bool cartridgeTest;
static std::vector<byte> expectedCarts[2];
static int expectedMappers[2] = {-1, -1};
static byte expectedResults[2];
static char lastError[256];
static uint16_t sampledJoysticks;
static unsigned joystickPolls;
static unsigned keyboardPolls, requestedDrawPercent = 100;
static bool requestedPal;
static bool expectRealLogo, logoTest, expectLogo;
static bool expectKanjiLogo;
static bool logoResetTest;
static bool truncateOmegaOnAllocation;
static bool truncatePanasonicOnAllocation, panasonicTest, panasonicResetTest;
static bool truncateMediaOnAllocation;
static bool panasonicOnly;
static bool mediaTest;
static void checkMedia();
static bool realMedia, realDiskASeen, realDiskBSeen, realTapeSeen;
static bool realMediaReload, realSavedASeen, realSavedBSeen;
static unsigned mediaCommands;
static void injectMediaCommand();
static std::vector<byte> expectedPanasonic;
static unsigned logoPcSamples, logoPixels, logoFrame;
static std::vector<byte> expectedKanjiBasic;
static std::vector<byte> expectedLogo;
static std::vector<byte> expectedExtension;

static bool executingLogo()
{
  return CPU.PC.W >= 0x8000 && CPU.PC.W < 0xC000 &&
         MemMap[0][0][4] != EmptyRAM && RAM[CPU.PC.W >> 13] == MemMap[0][0][CPU.PC.W >> 13];
}

uint16_t msxPollJoysticks()
{
  // Exercise all combinations independently; disconnected ports also return zero.
  sampledJoysticks = realBios ? 0 : static_cast<uint16_t>(
      ((joystickPolls * 7) & 0x3f) | (((joystickPolls * 11 + 3) & 0x3f) << 8));
  ++joystickPolls;
  return sampledJoysticks;
}

static void checkJoystickPorts()
{
  assert(JOYTYPE(0) == JOY_STICK && JOYTYPE(1) == JOY_STICK);
  const byte savedRegister = PSG.Latch;
  const byte savedControl = PSG.R[15];
  for (unsigned port = 0; port < 2; ++port)
  {
    OutZ80(0xA0, 15);
    OutZ80(0xA1, static_cast<byte>(0x0f | (port << 6)));
    OutZ80(0xA0, 14);
    assert((InZ80(0xA2) & 0x3f) == ((~(sampledJoysticks >> (port * 8))) & 0x3f));
  }
  OutZ80(0xA0, 15);
  OutZ80(0xA1, savedControl);
  OutZ80(0xA0, savedRegister);
}

static void checkStartupHardware()
{
  if (MODEL(MSX_MSX2P)) {
    assert(InZ80(0xF4) == 0xFF);
    OutZ80(0xF4, 0);
    assert(InZ80(0xF4) == 0x7F);
    OutZ80(0xF4, 0x80);
    assert(InZ80(0xF4) == 0xFF);
  } else {
    OutZ80(0xF4, 0);
    assert(InZ80(0xF4) == 0xFF);
  }
  // Beam-timed sprite collision regressions live in animation.ps1.
}

static void checkCartridges()
{
  for (int slot = 0; slot < 2; ++slot) {
    const word pc = CPU.PC.W;
    const int mode = Mode;
    const unsigned allocations = allocation;
    const unsigned errorCount = errors;
    char error[128] = "old error";
    assert(MsxValidateCartridge(ROMName[slot], error, sizeof(error)) && !error[0]);
    assert(!MsxValidateCartridge("relative.rom", error, sizeof(error)) && error[0]);
    assert(CPU.PC.W == pc && Mode == mode && allocation == allocations);
    assert(errors == errorCount && !ExitNow);
    if (RdZ80(0xC100 + slot) != expectedResults[slot])
      fprintf(stderr, "Slot %d: result=%02X expected=%02X PC=%04X PSL=%02X SSL3=%02X size=%zu mapper=%d\n",
              slot + 1, RdZ80(0xC100 + slot), expectedResults[slot], CPU.PC.W,
              PSLReg, SSLReg[3], expectedCarts[slot].size(), expectedMappers[slot]);
    assert(RdZ80(0xC100 + slot) == expectedResults[slot]);
    if (expectedCarts[slot].empty()) {
      assert(!ROMData[slot] && !ROMMask[slot] && !SCCOn[slot]);
      for (int bank = 0; bank < 8; ++bank)
        assert(MemMap[slot + 1][0][bank] == EmptyRAM);
      continue;
    }
    assert(ROMData[slot]);
    assert(memcmp(ROMData[slot], expectedCarts[slot].data(),
                  expectedCarts[slot].size()) == 0);
    assert(!SCCOn[slot]);
    const int mapper = expectedMappers[slot];
    if (mapper < 0) { assert(!ROMMask[slot]); continue; }
    assert(ROMType[slot] == mapper);
    assert(ROMMapper[slot][0] == 0 && ROMMapper[slot][1] == 1);
    assert(ROMMapper[slot][2] == 2 && ROMMapper[slot][3] == 3);
    OutZ80(0xA8, 0xC0 | ((slot + 1) * 0x14));
    const word registers[] = {0xA000,0x8000,0xB000,0xA000,0x7800,0x77FF,0xA000,0x7FF7};
    const bool wide = mapper == MAP_GEN16 || mapper == MAP_ASCII16 || mapper == MAP_FMPAC;
    const unsigned bank = expectedCarts[slot].size() / 8192 - 2;
    WrZ80(registers[mapper], wide ? bank / 2 : bank);
    const word address = mapper == MAP_FMPAC ? 0x4100 : wide ? 0x8100 : 0xA100;
    // GameMaster2 has a fixed 16-bank mask; its test image has 16 banks.
    assert(RdZ80(address) == expectedCarts[slot][bank * 8192 + 0x100]);
    if (mapper == MAP_KONAMI5) {
      WrZ80(0x9000, 0x3F);
      assert(SCCOn[slot]);
    }
  }
}

static void captureImage(const char* name, const uint8_t* pixels, int width, int height,
                         const uint32_t* palette)
{
  FILE* f = fopen(name, "wb");
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
}

static void captureBoot(const uint8_t* pixels, int width, int height,
                        const uint32_t* palette)
{
  captureImage("boot.ppm", pixels, width, height, palette);
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
      if (strstr(text, "DISKA")) realDiskASeen = true;
      if (strstr(text, "DISKB")) realDiskBSeen = true;
      if (strstr(text, "TAPEOK")) realTapeSeen = true;
      if (strstr(text, "SAVEDAOK")) realSavedASeen = true;
      if (strstr(text, "SAVEDBOK")) realSavedBSeen = true;
      printf("%s\n", text);
    }
  }
}

extern "C" void* heap_caps_malloc(size_t size, unsigned)
{
  if (++allocation == failAllocation) return nullptr;
  if (truncateMediaOnAllocation && size > 300000) {
    truncateMediaOnAllocation = false;
    FILE* f = fopen("media-short.dsk","wb");
    assert(f && fclose(f)==0);
  }
  if (truncateOmegaOnAllocation && size == 32768) {
    // Simulate a short read after the loader has checked the bank's exact size.
    truncateOmegaOnAllocation = false;
    FILE* f = fopen("OMEGA.ROM", "wb");
    assert(f && fclose(f) == 0);
  }
  if (truncatePanasonicOnAllocation && size == 32768) {
    truncatePanasonicOnAllocation = false;
    FILE* f = fopen("PANASONIC.ROM", "wb");
    assert(f && fclose(f) == 0);
  }
  return malloc(size);
}
extern "C" void heap_caps_free(void* ptr) { free(ptr); }

static void checkPanasonic()
{
  assert(Kanji && !memcmp(Kanji, expectedPanasonic.data() + 0x14000,
                           expectedPanasonic.size() - 0x14000));
  for (int bank = 0; bank < 6; ++bank) {
    assert(MemMap[3][1][bank] == MemMap[3][1][0] + bank * 8192);
    assert(!memcmp(MemMap[3][1][bank], expectedPanasonic.data() + 0x8000 + bank * 8192, 8192));
  }
  assert(MemMap[0][0][4] == EmptyRAM && MemMap[0][0][5] == EmptyRAM);
  for (int bank = 0; bank < 8; ++bank)
    assert(MemMap[3][3][bank] == EmptyRAM); // No Cockpit, firmware mapper or disk.
  const byte primary = PSLReg, secondary = SSLReg[3];
  const I8255 savedPpi = PPI;
  OutZ80(0xAB, 0x82);
  OutZ80(0xA8, 0xFF);
  WrZ80(0xFFFF, 0x95); // Extension in pages 0..2, RAM in page 3.
  for (word address : {0x0123, 0x4123, 0x8123}) {
    const byte value = RdZ80(address);
    if (value != expectedPanasonic[0x8000 + address])
      fprintf(stderr, "Panasonic slot read: addr=%04X value=%02X expected=%02X primary=%02X secondary=%02X\n",
              address, value, expectedPanasonic[0x8000 + address], PSLReg, SSLReg[3]);
    assert(value == expectedPanasonic[0x8000 + address]);
    WrZ80(address, ~value);
    assert(RdZ80(address) == value);
  }
  WrZ80(0xFFFF, secondary);
  OutZ80(0xA8, primary);
  PPI = savedPpi;

  const bool level2 = expectedPanasonic.size() == 0x54000;
  for (unsigned letter : {0U, 65U, 2047U, 4095U}) {
    OutZ80(0xD8, static_cast<byte>((letter & 63) | 0xC0));
    OutZ80(0xD9, static_cast<byte>((letter >> 6) | 0xC0));
    assert(InZ80(0xDB) == 0xFF); // Wrong read level does not advance shared count.
    for (unsigned count = 0; count < 33; ++count)
      assert(InZ80(0xD9) == Kanji[letter * 32 + (count & 31)]);
    OutZ80(0xDA, static_cast<byte>((letter & 63) | 0xC0));
    OutZ80(0xDB, static_cast<byte>((letter >> 6) | 0xC0));
    if (level2) {
      assert(InZ80(0xD9) == 0xFF);
      for (unsigned count = 0; count < 33; ++count)
        assert(InZ80(0xDB) == Kanji[0x20000 + letter * 32 + (count & 31)]);
      OutZ80(0xD8, letter & 63); // The other level shares the high address bits.
      assert(InZ80(0xD9) == Kanji[letter * 32]);
      OutZ80(0xDB, letter >> 6); // Either address write resets the count.
      assert(InZ80(0xDB) == Kanji[0x20000 + letter * 32]);
    } else {
      assert(InZ80(0xDB) == 0xFF);
      assert(InZ80(0xD9) == Kanji[letter * 32 + 1]); // Unmapped DA/DB do nothing.
    }
  }
}

void msxPresent(const uint8_t* pixels, int width, int height, const uint32_t* palette)
{
  assert(pixels && palette && width == 256 && height == 240);
  assert(palette[255] == 0xFFFFFF);
  for (int i = 0; i < width * height; ++i)
    if (pixels[i]) { ++coloredFrames; break; }
  ++frames;
  if(realCartridge&&frames==1&&expectedRealMapper>=0)
    assert(ROMType[0]==expectedRealMapper);
  if(realCartridge&&(frames==1||frames==frameLimit))
    printf("Cartridge diagnostic: frame=%u mapper=%u banks=%u PC=%04X mapped=%u/%u/%u/%u\n",
           frames,ROMType[0],ROMMask[0]+1,CPU.PC.W,
           ROMMapper[0][0],ROMMapper[0][1],ROMMapper[0][2],ROMMapper[0][3]);
  if (mediaTest && frames == 1) checkMedia();
  if (!realBios && frames == 1) checkStartupHardware();
  if (panasonicTest && frames == 1) checkPanasonic();
  if (logoTest && frames == 1) {
    if (!expectedExtension.empty())
      assert(memcmp(MemMap[3][1][0], expectedExtension.data(), expectedExtension.size()) == 0);
    if (expectLogo) {
      assert(executingLogo());
      assert(memcmp(MemMap[0][0][4], expectedLogo.data(), expectedLogo.size()) == 0);
      if (!expectedKanjiBasic.empty())
        for (unsigned bank = 0; bank < 4; ++bank)
          assert(!memcmp(MemMap[3][1][bank + 2], expectedKanjiBasic.data() + bank * 8192, 8192));
      assert(MemMap[0][0][5] == MemMap[0][0][4] + 0x2000);
      assert(RdZ80(0xC102) == 0x5A && RdZ80(0xC103) == 0xA5);
      const byte value = RdZ80(0x8000);
      WrZ80(0x8000, ~value);
      assert(RdZ80(0x8000) == value);
    } else {
      assert(MemMap[0][0][4] == EmptyRAM && MemMap[0][0][5] == EmptyRAM);
      assert(RdZ80(0xC102) == 0xFF);
    }
  }
  if (realBios && (expectRealLogo || expectKanjiLogo)) {
    if (executingLogo()) ++logoPcSamples;
    unsigned histogram[256] = {};
    for (int i = 0; i < width * height; ++i) ++histogram[pixels[i]];
    unsigned background = 0;
    for (unsigned count : histogram) if (count > background) background = count;
    unsigned detail = width * height - background;
    if (expectKanjiLogo) detail = histogram[255]; // White MSX lettering, not a plain blue bitmap.
    const bool logoMode = expectKanjiLogo ? ScrMode == 6 : executingLogo();
    if (logoMode && detail > logoPixels && detail > (expectKanjiLogo ? 500U : 100U)) {
      logoPixels = detail;
      logoFrame = frames;
      captureImage("boot-logo.ppm", pixels, width, height, palette);
      printf("Logo screen: frame=%u, mode=%u, PC=%04X, non-background=%u\n",
             frames, ScrMode, CPU.PC.W, detail);
    }
  }
  if (!realBios && joystickPolls) checkJoystickPorts();
  if (cartridgeTest && frames == 1) checkCartridges();
  if (realBios && frames == frameLimit) captureBoot(pixels, width, height, palette);
  if (realBios && frames == (frameLimit < 30 ? frameLimit : 30))
    captureImage("boot-early.ppm", pixels, width, height, palette);
  if (logoResetTest && frames == frameLimit) {
    byte* logo = MemMap[0][0][4];
    const int chunks = NChunks;
    assert(ResetMSX(Mode, RAMPages, VRAMPages) == Mode);
    assert(MemMap[0][0][4] == logo && NChunks == chunks);
    for (int model : {0, 1, 2}) {
      const int mode = (Mode & ~MSX_MODEL) | model;
      assert((ResetMSX(mode, 4, model ? 8 : 2) & MSX_MODEL) == model);
      if (model == 2) {
        assert(MemMap[0][0][4] != EmptyRAM);
        assert(memcmp(MemMap[0][0][4], expectedLogo.data(), expectedLogo.size()) == 0);
      } else assert(MemMap[0][0][4] == EmptyRAM && MemMap[0][0][5] == EmptyRAM);
      assert(RAM[4] == MemMap[0][0][4] && RAM[5] == MemMap[0][0][5]);
    }
    assert(NChunks == chunks);
  }
  if (panasonicResetTest && frames == frameLimit) {
    const int chunks = NChunks;
    byte* font = Kanji;
    assert(ResetMSX(Mode, RAMPages, VRAMPages) == Mode);
    assert(Kanji == font && NChunks == chunks);
    assert(InZ80(0xD9) == Kanji[0] && InZ80(0xDB) == 0xFF);
    FILE* bank = fopen("PANASONIC.ROM", "wb");
    assert(bank && fclose(bank) == 0);
    const int previousMode = Mode;
    const int otherMode = (Mode & ~MSX_MODEL) | (MODEL(MSX_MSX2P) ? MSX_MSX2 : MSX_MSX2P);
    assert(ResetMSX(otherMode, 4, 8) == previousMode);
    assert(Kanji == font && NChunks == chunks);
    checkPanasonic(); // A failed model change must keep the previous map intact.
    bank = fopen("PANASONIC.ROM", "wb");
    assert(bank && fwrite(expectedPanasonic.data(), 1, expectedPanasonic.size(), bank) == expectedPanasonic.size());
    assert(fclose(bank) == 0);
    for (int model : {0, 1, 2}) {
      if (model == 1 && expectedPanasonic.size() == 0x54000) {
        const int previousMode = Mode;
        const int previousChunks = NChunks;
        assert(ResetMSX((Mode & ~MSX_MODEL) | model, 4, 8) == previousMode);
        assert(!Kanji && NChunks == previousChunks);
        continue;
      }
      assert((ResetMSX((Mode & ~MSX_MODEL) | model, 4, model ? 8 : 2) & MSX_MODEL) == model);
      if (model) checkPanasonic();
      else {
        assert(!Kanji && InZ80(0xD9) == 0xFF && InZ80(0xDB) == 0xFF);
        for (int bank = 0; bank < 6; ++bank) assert(MemMap[3][1][bank] == EmptyRAM);
      }
    }
    assert(NChunks == chunks);
  }
}
void msxPollKeyboard(uint8_t matrix[16])
{
  ++keyboardPolls;
  if (realMedia) injectMediaCommand();
  if (!realBios) {
    matrix[0] = 0xFE;
    UPeriod = requestedDrawPercent;
    if (requestedPal && !PALVideo) {
      OutZ80(0x99, VDP[9] | 2);
      OutZ80(0x99, 0x89);
    }
  }
}
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
  snprintf(lastError, sizeof(lastError), "%s", text);
  if (realBios) fprintf(stderr, "%s\n", text);
}

static void writeImage(const char* name, const std::vector<byte>& image)
{
  FILE* f = fopen(name, "wb");
  assert(f);
  assert(fwrite(image.data(), 1, image.size(), f) == image.size());
  assert(fclose(f) == 0);
}

static std::vector<byte> makeCartridge(unsigned size, byte marker, int mapper = -1)
{
  std::vector<byte> image(size);
  for (unsigned bank = 0; bank < size / 8192; ++bank)
    memset(image.data() + bank * 8192, byte(marker + bank), 8192);
  const unsigned header = mapper < 0 && size > 32768 ? 0x4000 : 0;
  memset(image.data() + header, 0, 16);
  image[header] = 'A'; image[header + 1] = 'B';
  image[header + 2] = 0x10; image[header + 3] = 0x40;
  // Cartridge initialization returns its marker to the synthetic slot-scanning BIOS.
  image[header + 16] = 0x3E; image[header + 17] = marker; image[header + 18] = 0xC9;
  const word hints[] = {0,0,0xB000,0xA000,0x7800,0x77FF,0,0};
  if (mapper >= 0 && hints[mapper]) {
    for (unsigned i = 0x200; i < 0x230; i += 3) {
      image[i] = 0x32;
      image[i + 1] = hints[mapper] & 255;
      image[i + 2] = hints[mapper] >> 8;
    }
  }
  return image;
}

static void resetCounters()
{
  frames = errors = audioSamples = allocation = failAllocation = 0;
  lastError[0] = 0;
}

static void runCartridges(const char* bios, const char* slot1, const char* slot2, int model = 0)
{
  char error[128] = "old error";
  assert(MsxValidateCartridge(slot1, error, sizeof(error)) && !error[0]);
  assert(MsxValidateCartridge(slot2, error, sizeof(error)) && !error[0]);
  resetCounters();
  assert(MsxCoreRun(bios, model, 4, slot1, slot2));
  assert(frames == frameLimit && !errors);
  assert(!ROMName[0] && !ROMName[1]);
  assert(NChunks == 0);
}

static std::vector<byte> slotScanningBios()
{
  // Select slot 3's RAM subslot, then inspect/call the AB header in each physical slot.
  const byte boot[] = {
    0xF3,0x31,0x00,0xF0,0x3E,0x82,0xD3,0xAB,
    0x3E,0xF0,0xD3,0xA8,0x3E,0xAA,0x32,0xFF,0xFF,
    0x3E,0xF4,0xD3,0xA8,0xCD,0x30,0x00,0x32,0x00,0xC1,
    0x3E,0xF8,0xD3,0xA8,0xCD,0x30,0x00,0x32,0x01,0xC1,
    0x76,0xC3,0x25,0x00
  };
  const byte scan[] = {
    0x3A,0x00,0x40,0xFE,0x41,0x20,0x0D,
    0x3A,0x01,0x40,0xFE,0x42,0x20,0x06,
    0xCD,0x10,0x40,0xC9,0x00,0x00,0x3E,0xFF,0xC9
  };
  std::vector<byte> bios(32768);
  memcpy(bios.data(), boot, sizeof(boot));
  memcpy(bios.data() + 0x30, scan, sizeof(scan));
  return bios;
}

static void cartridgeRegression(const char* directory)
{
  writeImage("MSX.ROM", slotScanningBios());
  const std::string paths[] = {std::string(directory) + "/slot1.rom",
                               std::string(directory) + "/slot2.rom"};
  cartridgeTest = true;
  for (unsigned size : {8192U,16384U,32768U,49152U,65536U}) {
    for (int selection = 1; selection <= 3; ++selection) {
      for (int slot = 0; slot < 2; ++slot) {
        const bool inserted = selection & (1 << slot);
        expectedCarts[slot] = inserted ? makeCartridge(size, 0x40 + slot * 0x20) : std::vector<byte>();
        expectedMappers[slot] = -1;
        expectedResults[slot] = inserted ? 0x40 + slot * 0x20 : 0xFF;
        if (inserted) writeImage(paths[slot].c_str(), expectedCarts[slot]);
      }
      runCartridges(directory, selection & 1 ? paths[0].c_str() : nullptr,
                     selection & 2 ? paths[1].c_str() : "");
    }
  }
  for (int mapper = 0; mapper < MAXMAPPERS; ++mapper) {
    for (int slot = 0; slot < 2; ++slot) {
      expectedCarts[slot] = makeCartridge(131072, 0x50 + slot * 0x20, mapper);
      expectedMappers[slot] = mapper;
      expectedResults[slot] = 0x50 + slot * 0x20;
      writeImage(paths[slot].c_str(), expectedCarts[slot]);
    }
    // The original heuristic cannot distinguish these three; use its optional CRC database.
    if (mapper == MAP_GEN16 || mapper == MAP_GMASTER2 || mapper == MAP_FMPAC) {
      FILE* f = fopen("CARTS.CRC", "wb");
      assert(f);
      for (const auto& image : expectedCarts) {
        unsigned sum = 0;
        for (byte value : image) sum += value;
        fprintf(f, "%08X %d\n", sum, mapper);
      }
      fclose(f);
    }
    runCartridges(directory, paths[0].c_str(), paths[1].c_str());
    if (mapper == MAP_GEN16 || mapper == MAP_GMASTER2 || mapper == MAP_FMPAC)
      assert(remove("CARTS.CRC") == 0);
  }
  // Non-power-of-two ROMs are mirrored to the next power of two by fMSX.
  for (int slot = 0; slot < 2; ++slot) {
    expectedCarts[slot] = makeCartridge(393216, 0x40, MAP_KONAMI5);
    expectedMappers[slot] = MAP_KONAMI5;
    expectedResults[slot] = 0x40;
  }
  writeImage(paths[0].c_str(), expectedCarts[0]);
  runCartridges(directory, paths[0].c_str(), paths[0].c_str());
  // Exercise the largest supported bank index, both slots, without mapper-mask truncation.
  for (int slot = 0; slot < 2; ++slot) {
    expectedCarts[slot] = makeCartridge(MSX_MAX_CART_BYTES, 0x40 + slot * 0x20, MAP_KONAMI5);
    expectedMappers[slot] = MAP_KONAMI5;
    expectedResults[slot] = 0x40 + slot * 0x20;
    writeImage(paths[slot].c_str(), expectedCarts[slot]);
  }
  runCartridges(directory, paths[0].c_str(), paths[1].c_str());
  // Every required cartridge allocation, including SRAM and its filename, must be fatal.
  for (int slot = 0; slot < 2; ++slot) {
    expectedCarts[slot] = makeCartridge(131072, 0x40 + slot * 0x20, MAP_ASCII8);
    expectedMappers[slot] = MAP_ASCII8;
    writeImage(paths[slot].c_str(), expectedCarts[slot]);
  }
  runCartridges(directory, paths[0].c_str(), paths[1].c_str());
  const unsigned allocations = allocation;
  for (unsigned failure = 1; failure <= allocations; ++failure) {
    resetCounters();
    failAllocation = failure;
    assert(!MsxCoreRun(directory, 0, 4, paths[0].c_str(), paths[1].c_str()));
    assert(errors == 1 && frames == 0 && strstr(lastError, "PSRAM"));
    runCartridges(directory, paths[0].c_str(), paths[1].c_str());
  }
  cartridgeTest = false;
  for(const char* signature : {"ROM_NEO8","ROM_NE16","ASCII16X"})
  {
    auto unsupported=makeCartridge(131072,0x40,MAP_KONAMI5);
    memcpy(unsupported.data()+16,signature,8);
    writeImage(paths[1].c_str(),unsupported);
    for(int slot=0;slot<2;++slot)
    {
      resetCounters();
      assert(!MsxCoreRun(directory,0,4,slot?nullptr:paths[1].c_str(),slot?paths[1].c_str():nullptr));
      assert(!frames&&errors==1&&strstr(lastError,"mapper")&&!NChunks&&!ROMData[0]&&!ROMData[1]);
    }
  }
  const std::vector<std::vector<byte>> badImages = {
    {}, std::vector<byte>(1), std::vector<byte>(8192),
    makeCartridge(8193, 0x40), makeCartridge(MSX_MAX_CART_BYTES + 8192, 0x40)
  };
  for (const auto& image : badImages) {
    writeImage(paths[1].c_str(), image);
    char error[128] = {};
    assert(!MsxValidateCartridge(paths[1].c_str(), error, sizeof(error)) && error[0]);
    char shortError[2] = {'x', 'x'};
    assert(!MsxValidateCartridge(paths[1].c_str(), shortError, sizeof(shortError)));
    assert(shortError[0] != 0 && shortError[1] == 0);
    assert(!MsxValidateCartridge(paths[1].c_str(), nullptr, 0));
    resetCounters();
    assert(!MsxCoreRun(directory, 0, 4, paths[0].c_str(), paths[1].c_str()));
    assert(errors == 1 && frames == 0);
    resetCounters();
    assert(!MsxCoreRun(directory, 0, 4, paths[1].c_str(), nullptr));
    assert(errors == 1 && frames == 0);
  }
  assert(remove(paths[1].c_str()) == 0);
  for (const char* invalid : {paths[1].c_str(), directory, "relative.rom"}) {
    char error[128] = {};
    assert(!MsxValidateCartridge(invalid, error, sizeof(error)) && error[0]);
    resetCounters();
    assert(!MsxCoreRun(directory, 0, 4, invalid, nullptr));
    assert(errors == 1 && frames == 0);
    resetCounters();
    assert(!MsxCoreRun(directory, 0, 4, nullptr, invalid));
    assert(errors == 1 && frames == 0);
  }
  expectedCarts[0].clear(); expectedCarts[1].clear();
  assert(MsxValidateCartridge(nullptr, nullptr, 0));
  char ignored = 'x';
  assert(MsxValidateCartridge("", &ignored, 0) && ignored == 'x');
  expectedResults[0] = expectedResults[1] = 0xFF;
  cartridgeTest = true;
  runCartridges(directory, nullptr, "");
  cartridgeTest = false;
  assert(remove(paths[0].c_str()) == 0);
  puts("PASS: physical slots 1/2/both, plain 8/16/32/48/64 KiB, all 8 mappers, 2 MiB, eject/switch, bad files and cartridge OOM recovery.");
}

static void logoRegression(const char* directory)
{
  auto bios = slotScanningBios();
  bios[0x25] = 0xC3; bios[0x26] = 0x60; bios[0x27] = 0x00;
  // Read slot 0 page 2, then execute it only when the synthetic logo is present.
  const byte probe[] = {
    0x3E,0xC0,0xD3,0xA8,0x3A,0x00,0x80,0x32,0x02,0xC1,
    0xFE,0x5A,0xCA,0x10,0x80,0x76,0xC3,0x6F,0x00
  };
  memcpy(bios.data() + 0x60, probe, sizeof(probe));
  for (const char* name : {"MSX.ROM", "MSX2.ROM", "MSX2P.ROM"})
    writeImage(name, bios);
  expectedLogo.assign(16384, 0);
  expectedLogo[0] = 0x5A;
  expectedLogo[0x2000] = 0xA5;
  const byte program[] = {
    0x3A,0x00,0xA0,0x32,0x03,0xC1,
    0x3E,0xF4,0xD3,0x99,0x3E,0x87,0xD3,0x99,
    0x76,0xC3,0x1E,0x80
  };
  memcpy(expectedLogo.data() + 0x10, program, sizeof(program));
  const std::string paths[] = {std::string(directory) + "/slot1.rom",
                               std::string(directory) + "/slot2.rom"};
  logoTest = cartridgeTest = true;
  for (int pass = 0; pass < 2; ++pass) {
    for (int present = 0; present < 2; ++present) {
      if (present) writeImage("MSX2PLOGO.ROM", expectedLogo);
      for (int model : {2, 0, 1, 2})
        for (int selection = 0; selection < 4; ++selection) {
          expectLogo = present && model == 2;
          for (int slot = 0; slot < 2; ++slot) {
            const bool inserted = selection & (1 << slot);
            expectedCarts[slot] = inserted ? makeCartridge(16384, 0x40 + slot * 0x20)
                                            : std::vector<byte>();
            expectedResults[slot] = inserted ? 0x40 + slot * 0x20 : 0xFF;
            expectedMappers[slot] = -1;
            if (inserted) writeImage(paths[slot].c_str(), expectedCarts[slot]);
          }
          runCartridges(directory, selection & 1 ? paths[0].c_str() : nullptr,
                         selection & 2 ? paths[1].c_str() : nullptr, model);
        }
      if (present) assert(remove("MSX2PLOGO.ROM") == 0);
    }
  }
  cartridgeTest = false;
  expectLogo = true;
  writeImage("MSX2PLOGO.ROM", expectedLogo);
  resetCounters();
  logoResetTest = true;
  assert(MsxCoreRun(directory, 2, 4));
  logoResetTest = false;
  assert(NChunks == 0);
  resetCounters();
  assert(MsxCoreRun(directory, 2, 4));
  const unsigned allocations = allocation;
  for (unsigned failure = 1; failure <= allocations; ++failure) {
    resetCounters();
    failAllocation = failure;
    assert(!MsxCoreRun(directory, 2, 4));
    assert(errors == 1 && frames == 0 && strstr(lastError, "PSRAM"));
    assert(!MemMap[0][0][4] && !MemMap[0][0][5]);
    assert(NChunks == 0);
    resetCounters();
    assert(MsxCoreRun(directory, 2, 4));
  }
  for (unsigned size : {0U, 1U, 16383U, 16385U, 32768U}) {
    writeImage("MSX2PLOGO.ROM", std::vector<byte>(size));
    resetCounters();
    assert(!MsxCoreRun(directory, 2, 4));
    assert(errors == 1 && frames == 0);
    assert(!MemMap[0][0][4] && !MemMap[0][0][5]);
    // Other models ignore even a malformed optional MSX2+ file.
    expectLogo = false;
    resetCounters();
    assert(MsxCoreRun(directory, 0, 4));
    writeImage("MSX2PLOGO.ROM", expectedLogo);
    expectLogo = true;
    resetCounters();
    assert(MsxCoreRun(directory, 2, 4));
  }
  assert(remove("MSX2PLOGO.ROM") == 0);
  // An existing path that cannot be opened/read is not an absent optional ROM.
#ifdef _WIN32
  assert(mkdir("MSX2PLOGO.ROM") == 0);
#else
  assert(mkdir("MSX2PLOGO.ROM", 0700) == 0);
#endif
  resetCounters();
  assert(!MsxCoreRun(directory, 2, 4));
  assert(errors == 1 && frames == 0);
  assert(rmdir("MSX2PLOGO.ROM") == 0);
  expectLogo = false;
  resetCounters();
  assert(MsxCoreRun(directory, 2, 4));
  logoTest = false;
  for (const auto& name : paths) assert(remove(name.c_str()) == 0);
  puts("PASS: optional logo Z80 read/execute, read-only slot 0 page 2, both cartridges, model/restart/eject, malformed files and OOM recovery.");
}

static void omegaRegression(const char* directory)
{
  std::vector<byte> main(32768), ext(16384), bank(262144, 0xFF);
  FILE* f = fopen("MSX2P.ROM", "rb");
  assert(f && fread(main.data(), 1, main.size(), f) == main.size());
  fclose(f);
  f = fopen("MSX2PEXT.ROM", "rb");
  assert(f && fread(ext.data(), 1, ext.size(), f) == ext.size());
  fclose(f);
  memcpy(bank.data(), main.data(), main.size());
  memcpy(bank.data() + 0x8000, expectedLogo.data(), expectedLogo.size());
  expectedKanjiBasic.assign(32768, 0x4b);
  memcpy(bank.data() + 0x14000, expectedKanjiBasic.data(), expectedKanjiBasic.size());
  memcpy(bank.data() + 0x10000, ext.data(), ext.size());
  expectedExtension = ext;
  writeImage("OMEGA.ROM", bank);
  // The combined bank wins over malformed legacy files without opening them.
  for (const char* name : {"MSX2P.ROM", "MSX2PEXT.ROM", "MSX2PLOGO.ROM"})
    writeImage(name, {});
  const std::string paths[] = {std::string(directory) + "/slot1.rom",
                               std::string(directory) + "/slot2.rom"};
  expectLogo = logoTest = cartridgeTest = true;
  for (int selection = 0; selection < 4; ++selection) {
    for (int slot = 0; slot < 2; ++slot) {
      const bool inserted = selection & (1 << slot);
      expectedCarts[slot] = inserted ? makeCartridge(16384, 0x40 + slot * 0x20)
                                    : std::vector<byte>();
      expectedResults[slot] = inserted ? 0x40 + slot * 0x20 : 0xFF;
      expectedMappers[slot] = -1;
      if (inserted) writeImage(paths[slot].c_str(), expectedCarts[slot]);
    }
    runCartridges(directory, selection & 1 ? paths[0].c_str() : nullptr,
                   selection & 2 ? paths[1].c_str() : nullptr, 2);
  }
  cartridgeTest = false;
  for (const char* name : {"MSX2P.ROM", "MSX2PEXT.ROM", "MSX2PLOGO.ROM"})
    assert(remove(name) == 0);
  resetCounters();
  logoResetTest = true;
  assert(MsxCoreRun(directory, 2, 4));
  logoResetTest = false;
  resetCounters();
  assert(MsxCoreRun(directory, 2, 4));
  const unsigned allocations = allocation;
  for (unsigned failure = 1; failure <= allocations; ++failure) {
    resetCounters();
    failAllocation = failure;
    assert(!MsxCoreRun(directory, 2, 4));
    assert(errors == 1 && frames == 0 && strstr(lastError, "PSRAM"));
    assert(!NChunks && !MemMap[0][0][4] && !MemMap[0][0][5]);
    resetCounters();
    assert(MsxCoreRun(directory, 2, 4));
  }
  // Loading a single bank must not produce any split files.
  for (const char* name : {"MSX2P.ROM", "MSX2PEXT.ROM", "MSX2PLOGO.ROM"})
    assert(access(name, F_OK) != 0);
  writeImage("MSX2P.ROM", main);
  writeImage("MSX2PEXT.ROM", ext);
  truncateOmegaOnAllocation = true;
  resetCounters();
  assert(!MsxCoreRun(directory, 2, 4));
  assert(!truncateOmegaOnAllocation && errors == 1 && frames == 0 && !NChunks);
  assert(!MemMap[0][0][4] && !MemMap[0][0][5]);
  writeImage("OMEGA.ROM", bank);
  resetCounters();
  assert(MsxCoreRun(directory, 2, 4));
  expectedExtension.clear();
  for (unsigned size : {0U, 1U, 262143U, 262145U, 524288U}) {
    writeImage("OMEGA.ROM", std::vector<byte>(size));
    resetCounters();
    assert(!MsxCoreRun(directory, 2, 4));
    assert(errors == 1 && frames == 0 && !NChunks);
    expectLogo = false;
    for (int model : {0, 1}) {
      resetCounters();
      assert(MsxCoreRun(directory, model, 4));
    }
    expectLogo = true;
    writeImage("OMEGA.ROM", bank);
    resetCounters();
    assert(MsxCoreRun(directory, 2, 4));
  }
  assert(remove("OMEGA.ROM") == 0);
#ifdef _WIN32
  assert(mkdir("OMEGA.ROM") == 0);
#else
  assert(mkdir("OMEGA.ROM", 0700) == 0);
#endif
  resetCounters();
  assert(!MsxCoreRun(directory, 2, 4));
  assert(errors == 1 && frames == 0 && !NChunks);
  assert(rmdir("OMEGA.ROM") == 0);
  expectLogo = false;
  resetCounters();
  assert(MsxCoreRun(directory, 2, 4));
  logoTest = false;
  for (const auto& name : paths) assert(remove(name.c_str()) == 0);
  puts("PASS: single authoritative 256 KiB Omega bank, direct MAIN/LOGO/SUB mapping, no extracted files, cartridge slots, model resets, invalid/unreadable banks and OOM recovery.");
  expectedKanjiBasic.clear();
}

static void panasonicRegression(const char* directory)
{
  const auto main = slotScanningBios();
  const std::string paths[] = {std::string(directory) + "/slot1.rom",
                               std::string(directory) + "/slot2.rom"};
  for (unsigned size : {0x34000U, 0x54000U}) {
    expectedPanasonic.assign(size, 0);
    memcpy(expectedPanasonic.data(), main.data(), main.size());
    for (unsigned i = 0x8000; i < size; ++i)
      expectedPanasonic[i] = static_cast<byte>((i >> 17) * 71 + (i >> 11) * 19 + (i >> 5) * 13 + i);
    writeImage("PANASONIC.ROM", expectedPanasonic);
    // Neither broken legacy files nor separate fonts/disks override this bank.
    for (const char* name : {"MSX2.ROM", "MSX2EXT.ROM", "MSX2P.ROM", "MSX2PEXT.ROM",
                             "MSX2PLOGO.ROM", "KANJI.ROM", "DISK.ROM"}) writeImage(name, {});
    panasonicTest = cartridgeTest = true;
    for (int model : {1, 2}) {
      if (model == 1 && size == 0x54000) {
        resetCounters();
        assert(!MsxCoreRun(directory, model, 4));
        assert(errors == 1 && !frames && !NChunks && !Kanji);
        continue;
      }
      for (int selection = 0; selection < 4; ++selection) {
        for (int slot = 0; slot < 2; ++slot) {
          const bool inserted = selection & (1 << slot);
          expectedCarts[slot] = inserted ? makeCartridge(16384, 0x40 + slot * 0x20) : std::vector<byte>();
          expectedMappers[slot] = -1;
          expectedResults[slot] = inserted ? 0x40 + slot * 0x20 : 0xFF;
          if (inserted) writeImage(paths[slot].c_str(), expectedCarts[slot]);
        }
        runCartridges(directory, selection & 1 ? paths[0].c_str() : nullptr,
                       selection & 2 ? paths[1].c_str() : nullptr, model);
      }
    }
    cartridgeTest = false;
    panasonicResetTest = true;
    resetCounters();
    assert(MsxCoreRun(directory, 2, 4));
    panasonicResetTest = false;
    resetCounters();
    assert(MsxCoreRun(directory, 2, 4));
    const unsigned allocations = allocation;
    for (unsigned failure = 1; failure <= allocations; ++failure) {
      resetCounters();
      failAllocation = failure;
      assert(!MsxCoreRun(directory, 2, 4));
      assert(errors == 1 && frames == 0 && !NChunks && !Kanji && strstr(lastError, "PSRAM"));
      resetCounters();
      assert(MsxCoreRun(directory, 2, 4));
    }
    for (const char* name : {"MSX2.ROM", "MSX2P.ROM"}) writeImage(name, main);
    for (const char* name : {"MSX2EXT.ROM", "MSX2PEXT.ROM"}) writeImage(name, std::vector<byte>(16384));
    for (const char* name : {"MSX2PLOGO.ROM", "KANJI.ROM", "DISK.ROM"}) assert(remove(name) == 0);
    truncatePanasonicOnAllocation = true;
    resetCounters();
    assert(!MsxCoreRun(directory, 2, 4));
    assert(!truncatePanasonicOnAllocation && errors == 1 && !frames && !NChunks && !Kanji);
    writeImage("PANASONIC.ROM", expectedPanasonic);
    writeImage("OMEGA.ROM", std::vector<byte>(0x40000));
    for (int model : {1, 2}) {
      resetCounters();
      assert(!MsxCoreRun(directory, model, 4));
      assert(errors == 1 && !frames && !NChunks && !Kanji);
    }
    assert(remove("OMEGA.ROM") == 0);
    for (unsigned bad : {0U, 1U, size - 1, size + 1, 0x40000U}) {
      writeImage("PANASONIC.ROM", std::vector<byte>(bad));
      for (int model : {1, 2}) {
        resetCounters();
        assert(!MsxCoreRun(directory, model, 4));
        assert(errors == 1 && !frames && !NChunks && !Kanji);
      }
      panasonicTest = false;
      resetCounters();
      assert(MsxCoreRun(directory, 0, 4)); // MSX1 ignores combined Panasonic images.
      panasonicTest = true;
      writeImage("PANASONIC.ROM", expectedPanasonic);
      resetCounters();
      assert(MsxCoreRun(directory, 2, 4));
    }
    assert(remove("PANASONIC.ROM") == 0);
    panasonicTest = false;
    resetCounters();
    assert(MsxCoreRun(directory, 2, 4));
    assert(!Kanji);
  }
#ifdef _WIN32
  assert(mkdir("PANASONIC.ROM") == 0);
#else
  assert(mkdir("PANASONIC.ROM", 0700) == 0);
#endif
  resetCounters();
  assert(!MsxCoreRun(directory, 2, 4));
  assert(errors == 1 && !frames && !NChunks && !Kanji);
  assert(rmdir("PANASONIC.ROM") == 0);
  for (const auto& name : paths) assert(remove(name.c_str()) == 0);
  expectedPanasonic.clear();
  puts("PASS: authoritative Panasonic 208/336 KiB banks, read-only 48 KiB extension, both cartridges, JIS1/2 addressing/interlock/wrap, resets, invalid/short reads, OOM and legacy recovery.");
}

#include "media_regression.h"

int main(int argc, char** argv)
{
  const bool mediaOnly = argc == 2 && strcmp(argv[1], "--media") == 0;
  panasonicOnly = argc == 2 && strcmp(argv[1], "--panasonic") == 0;
  if (argc > 1 && !panasonicOnly && !mediaOnly) {
    assert(argc == 5 || argc == 6 || argc == 7);
    realCartridge = argc >= 6;
    if(argc == 7) expectedRealMapper = atoi(argv[6]);
    realBios = true;
    frameLimit = static_cast<unsigned>(atoi(argv[4]));
    assert(frameLimit > 0);
    const std::string omegaPath = std::string(argv[1]) + "/OMEGA.ROM";
    const bool omega = atoi(argv[2]) == 2 && access(omegaPath.c_str(), F_OK) == 0;
    const std::string panasonicPath = std::string(argv[1]) + "/PANASONIC.ROM";
    if (atoi(argv[2]) == 2 && access(panasonicPath.c_str(), F_OK) == 0)
      expectKanjiLogo = true;
    const std::string logoPath = omega ? omegaPath : std::string(argv[1]) + "/MSX2PLOGO.ROM";
    if (atoi(argv[2]) == 2) {
      FILE* logo = fopen(logoPath.c_str(), "rb");
      if (logo) {
        if (omega) assert(fseek(logo, 0x8000, SEEK_SET) == 0);
        unsigned bytesRead = 0;
        for (unsigned i = 0; i < 16384; ++i) {
          const int value = fgetc(logo);
          if (value == EOF) break;
          ++bytesRead;
          if (value != 0xFF) expectRealLogo = true;
        }
        if (omega) {
          assert(fseek(logo, 0x14000, SEEK_SET) == 0);
          expectKanjiLogo = fgetc(logo) == 'A' && fgetc(logo) == 'B';
        }
        fclose(logo);
        if (bytesRead == 16384 && !expectRealLogo)
          puts("Auxiliary slot-0 page-2 area is erased; checking the built-in Kanji BASIC startup logo instead.");
      }
    }
    const std::string diskBiosPath = std::string(argv[1]) + "/DISK.ROM";
    realMedia = !realCartridge && access(diskBiosPath.c_str(),F_OK)==0;
    if (realMedia && frameLimit < 900) {
      fprintf(stderr,"Real media save/reload verification needs at least 900 frames.\n");
      return 1;
    }
    char currentDirectory[1024];
    assert(getcwd(currentDirectory,sizeof(currentDirectory)));
    for (char* p=currentDirectory;*p;++p) if(*p=='\\') *p='/';
    const char* mediaRoot=currentDirectory[1]==':'? currentDirectory+2:currentDirectory;
    const std::string diskAPath=std::string(mediaRoot)+"/real-a.dsk";
    const std::string diskBPath=std::string(mediaRoot)+"/real-b.dsk";
    const std::string tapePath=std::string(mediaRoot)+"/real.cas";
    if (realMedia) prepareRealMedia();
    const bool result = MsxCoreRun(argv[1], atoi(argv[2]), atoi(argv[3]),realCartridge?argv[5]:nullptr,nullptr,
                                   realMedia?diskAPath.c_str():nullptr,
                                   realMedia?diskBPath.c_str():nullptr,
                                   realMedia?tapePath.c_str():nullptr);
    printf("BIOS boot: result=%d, frames=%u, samples=%u, BASIC prompt=%s\n",
           result, frames, audioSamples, basicPrompt ? "yes" : "not detected");
    if (expectKanjiLogo)
      printf("Kanji BASIC startup logo: frame=%u, white lettering pixels=%u\n", logoFrame, logoPixels);
    else if (expectRealLogo)
      printf("Logo boot: executing slot-0 page-2 samples=%u, captured frame=%u, detail pixels=%u\n",
             logoPcSamples, logoFrame, logoPixels);
    if (realMedia) {
      printf("Real Disk BASIC FILES A/B and CAS BLOAD: %d/%d/%d\n",realDiskASeen,realDiskBSeen,realTapeSeen);
      assert(realDiskASeen&&realDiskBSeen&&realTapeSeen);
      assertSavedProgram("real-a.dsk");
      assertSavedProgram("real-b.dsk");
      realMediaReload=true;
      mediaCommands=0;
      resetCounters();
      assert(MsxCoreRun(argv[1],atoi(argv[2]),atoi(argv[3]),nullptr,nullptr,
                        diskAPath.c_str(),diskBPath.c_str(),tapePath.c_str()));
      assert(realSavedASeen&&realSavedBSeen&&!errors);
      printf("Disk BASIC SAVE A/B persisted across cold boot and LOAD/RUN: %d/%d\n",realSavedASeen,realSavedBSeen);
      for (const char* name : {"real-a.dsk","real-b.dsk","real.cas"}) assert(remove(name)==0);
    }
    return result && frames == frameLimit && (realCartridge || basicPrompt) &&
           (!realMedia||(realDiskASeen&&realDiskBSeen&&realTapeSeen)) &&
           (realCartridge || !expectRealLogo || (logoPcSamples && logoFrame)) &&
           (realCartridge || !expectKanjiLogo || logoFrame) ? 0 : 1;
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
  if (panasonicOnly) {
    panasonicRegression(path);
    for (const char* name : names) assert(remove(name) == 0);
    return 0;
  }
  if (mediaOnly) {
    mediaRegression(path);
    for (const char* name : names) assert(remove(name) == 0);
    return 0;
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
  cartridgeRegression(path);
  logoRegression(path);
  omegaRegression(path);
  panasonicRegression(path);
  mediaRegression(path);
  frameLimit = 30;
  for (int pal = 0; pal < 2; ++pal) {
    requestedPal = pal != 0;
    requestedDrawPercent = 100;
    resetCounters();
    keyboardPolls = 0;
    assert(MsxCoreRun(path, 0, 4));
    const unsigned fullPolls = keyboardPolls, fullAudio = audioSamples;
    requestedDrawPercent = 25;
    resetCounters();
    keyboardPolls = 0;
    assert(MsxCoreRun(path, 0, 4));
    assert(frames == frameLimit && !errors);
    assert(keyboardPolls > fullPolls * 3 && keyboardPolls <= fullPolls * 4);
    assert(audioSamples > fullAudio * 3 && audioSamples <= fullAudio * 4);
    assert(bool(PALVideo) == requestedPal);
  }
  requestedDrawPercent = 100;
  requestedPal = false;
  frameLimit = 3;
  puts("PASS: renderer skipping preserves keyboard/audio cadence in PAL and NTSC.");
  for (const char* name : names) assert(remove(name) == 0);
  frames = errors = 0;
  assert(!MsxCoreRun(path, 2, 8));
  assert(errors == 1);
  puts("PASS: 24 boots, 7 allocation failures/recoveries, missing BIOS, input, audio, video.");
}
