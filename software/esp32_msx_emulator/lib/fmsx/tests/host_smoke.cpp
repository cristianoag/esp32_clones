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

extern "C" {
extern AY8910 PSG;
extern byte *ROMData[MAXSLOTS], *MemMap[4][4][8], *EmptyRAM;
extern byte ROMType[MAXSLOTS], ROMMapper[MAXSLOTS][4], ROMMask[MAXSLOTS], SCCOn[2];
}

static unsigned frames, audioSamples, nonzeroSamples, coloredFrames, errors, allocation;
static unsigned failAllocation;
static unsigned frameLimit = 3;
static bool realBios;
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
  if (!realBios && joystickPolls) checkJoystickPorts();
  if (cartridgeTest && frames == 1) checkCartridges();
  if (realBios && frames == frameLimit) captureBoot(pixels, width, height, palette);
}
void msxPollKeyboard(uint8_t matrix[16])
{
  ++keyboardPolls;
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

static void runCartridges(const char* bios, const char* slot1, const char* slot2)
{
  char error[128] = "old error";
  assert(MsxValidateCartridge(slot1, error, sizeof(error)) && !error[0]);
  assert(MsxValidateCartridge(slot2, error, sizeof(error)) && !error[0]);
  resetCounters();
  assert(MsxCoreRun(bios, 0, 4, slot1, slot2));
  assert(frames == frameLimit && !errors);
  assert(!ROMName[0] && !ROMName[1]);
}

static void cartridgeRegression(const char* directory)
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
  writeImage("MSX.ROM", bios);
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
  cartridgeRegression(path);
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
