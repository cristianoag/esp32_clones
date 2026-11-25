#include "../../../src/MsxAudioProfiles.h"
#include "AudioProfiles.h"
#include "MSX.h"
#include "Sound.h"
#include "Esp32Port.h"
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#include <fcntl.h>
#include <io.h>
#include <share.h>
#define makeDirectory(path) _mkdir(path)
#define removeDirectory(path) _rmdir(path)
#else
#include <unistd.h>
#define makeDirectory(path) mkdir(path, 0700)
#define removeDirectory(path) rmdir(path)
#endif

extern "C" {
extern AY8910 PSG;
extern SCC SCChip;
extern YM2413 OPLL;
extern byte *MemMap[4][4][8], *ROMData[MAXSLOTS], *EmptyRAM, *RAM[8];
extern byte ROMMapper[MAXSLOTS][4], ROMMask[MAXSLOTS], SCCOn[2];
extern int NChunks;
void* fmsxAllocate(size_t n) { return malloc(n); }
void* fmsxAllocateCpu(size_t n) { return malloc(n); }
FILE* fmsxOpen(const char* name, const char* mode) { return fopen(name, mode); }
void fmsxFrame() { ExitNow = 1; }
void Keyboard() {}
unsigned Joystick() { return 0; }
unsigned Mouse(byte) { return 0; }
void SetColor(byte, byte, byte, byte) {}
void RefreshScreen() {}
#define LINE(n) void RefreshLine##n(byte) {}
LINE(0) LINE(1) LINE(2) LINE(3) LINE(4) LINE(5) LINE(6)
LINE(7) LINE(8) LINE(10) LINE(12) LINE(Tx80)
byte DebugZ80(Z80*) { return 1; }
unsigned InitAudio(unsigned rate, unsigned) { return rate; }
void TrashAudio() {}
unsigned GetFreeAudio() { return 1024; }
unsigned WriteAudio(sample*, unsigned count) { return count; }
void PlayAllSound(int) {}
}

static void writeFile(const char* path, const std::vector<byte>& bytes)
{
  FILE* f = fopen(path, "wb");
  assert(f);
  assert(fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size());
  assert(!fclose(f));
}

static std::vector<byte> fmImage(size_t size)
{
  std::vector<byte> data(size, 0);
  for(size_t i = 0; i < size; ++i) data[i] = static_cast<byte>(i / 0x4000);
  data[0] = 'A'; data[1] = 'B';
  memcpy(data.data() + 0x18, size == 0x4000 ? "APRLOPLL" : "PAC2OPLL", 8);
  return data;
}

static void resolverTests()
{
  makeDirectory("preferred");
  char path[512], error[512];
  for(unsigned p = 0; p < MsxAudioProfileCount; ++p) {
    assert(MsxAudioProfileValid(p));
    assert(strcmp(MsxAudioProfileName(p), "Unknown"));
    const bool required = p == MsxAudioPsgFm || p == MsxAudioPsgSccFm;
    assert(MsxResolveAudioProfile(p, "preferred", path, sizeof(path), error,
                                sizeof(error), "shared.rom") == !required);
    assert(!path[0]);
    assert(required == !!error[0]);
  }
  assert(!MsxAudioProfileValid(5));
  assert(!MsxAudioProfileValid(~0U));
  assert(!MsxResolveAudioProfile(5, "preferred", path, sizeof(path), error, sizeof(error)));
  writeFile("shared.rom", fmImage(0x4000));
  assert(MsxResolveAudioProfile(MsxAudioPsgFm, "preferred", path, sizeof(path),
                               error, sizeof(error), "shared.rom"));
  assert(!strcmp(path, "shared.rom") && !error[0]);
  writeFile("preferred/FMPAC.ROM", fmImage(0x10000));
  assert(MsxResolveAudioProfile(MsxAudioAuto, "preferred", path, sizeof(path),
                               error, sizeof(error), "shared.rom"));
  assert(!strcmp(path, "preferred/FMPAC.ROM"));
#ifdef _WIN32
  const int locked = _sopen("preferred/FMPAC.ROM", _O_RDONLY | _O_BINARY, _SH_DENYRW);
  assert(locked >= 0);
#else
  assert(!chmod("preferred/FMPAC.ROM", 0));
#endif
  assert(!MsxResolveAudioProfile(MsxAudioAuto, "preferred", path, sizeof(path),
                                error, sizeof(error), "shared.rom"));
  assert(!path[0] && error[0]);
#ifdef _WIN32
  _close(locked);
#else
  assert(!chmod("preferred/FMPAC.ROM", 0600));
#endif
  for(size_t n : {size_t(0), size_t(31), size_t(0x4000), size_t(0x8000), size_t(0x10000)}) {
    writeFile("preferred/FMPAC.ROM", std::vector<byte>(n, 0));
    assert(!MsxResolveAudioProfile(MsxAudioAuto, "preferred", path, sizeof(path),
                                  error, sizeof(error), "shared.rom"));
    assert(!path[0] && error[0]);
    assert(MsxResolveAudioProfile(MsxAudioPsgScc, "preferred", path, sizeof(path),
                                 error, sizeof(error), "shared.rom"));
  }
  remove("preferred/FMPAC.ROM");
  makeDirectory("preferred/FMPAC.ROM");
  assert(!MsxResolveAudioProfile(MsxAudioAuto, "preferred", path, sizeof(path),
                                error, sizeof(error), "shared.rom"));
  removeDirectory("preferred/FMPAC.ROM");
  assert(!MsxResolveAudioProfile(MsxAudioAuto, "shared.rom", path, sizeof(path),
                                error, sizeof(error), "shared.rom"));
  assert(!MsxResolveAudioProfile(MsxAudioPsgFm, "preferred", path, 3,
                                error, sizeof(error), "shared.rom"));
  assert(!MsxResolveAudioProfile(MsxAudioPsgFm, nullptr, path, sizeof(path),
                                error, sizeof(error), "shared.rom"));
  assert(!MsxResolveAudioProfile(MsxAudioAuto, "preferred", nullptr, 0, nullptr, 0));
  remove("shared.rom");
  writeFile("shared.rom", std::vector<byte>(0x4000, 0));
  assert(!MsxResolveAudioProfile(MsxAudioAuto, "preferred", path, sizeof(path),
                                error, sizeof(error), "shared.rom"));
  remove("shared.rom");
  removeDirectory("preferred");
}

static void selectFM()
{
  OutZ80(0xAB, 0x82);
  OutZ80(0xA8, 0xCC); // FM slot 3:0, page 1; RAM page 3.
  WrZ80(0xFFFF, 0x80);
}

static void fmWrite(byte reg, byte value)
{
  OutZ80(0x7C, reg);
  OutZ80(0x7D, value);
}

static void flush()
{
  ExitNow = 0;
  for(int i = 0; i < 32; ++i) {
    CPU.ICount = 0;
    LoopZ80(&CPU);
  }
}

static bool signal(unsigned mask)
{
  int samples[1024] = {};
  SetChannels(128, mask);
  RenderAudio(samples, 1024);
  for(int n : samples) if(n) return true;
  return false;
}

static void exercise(unsigned profile, const char* fm, int kind)
{
  assert(FmsxSetAudioProfile(profile, fm));
  ROMName[0] = "cart-a.rom";
  ROMName[1] = "cart-b.rom";
  assert(StartMSX(MSX_MSX1 | MSX_NTSC | (MAP_KONAMI5 << 8), 8, 2));
  assert(NChunks > 0 && ROMData[0] && ROMData[1]);
  assert(PSG.First == 0 && SCChip.First == AY8910_CHANNELS);
  assert(OPLL.First == AY8910_CHANNELS + SCC_CHANNELS);
  const bool useFM = kind && profile != MsxAudioPsg && profile != MsxAudioPsgScc;
  const bool useSCC = profile == MsxAudioAuto || profile == MsxAudioPsgScc || profile == MsxAudioPsgSccFm;
  selectFM();
  if(useFM) {
    if(RdZ80(0x4000) != 'A')
      fprintf(stderr, "FM profile=%u kind=%d selected=%02X/%02X read=%02X ROM=%p map=%p RAM=%p\n",
              profile, kind, PSLReg, SSLReg[3], RdZ80(0x4000),
              static_cast<void*>(ROMData[2]), static_cast<void*>(MemMap[3][0][2]),
              static_cast<void*>(RAM[2]));
    assert(RdZ80(0x4000) == 'A' && RdZ80(0x4001) == 'B');
    assert(!memcmp(MemMap[3][0][2] + 0x18, kind == 2 ? "PAC2OPLL" : "APRLOPLL", 8));
    assert(MemMap[3][0][0] == EmptyRAM && MemMap[3][0][4] == EmptyRAM);
    assert(MemMap[3][1][2] == EmptyRAM); // No MSX1 sub-ROM or disk overwrite.
    if(kind == 2) {
      assert(RdZ80(0x7FF6) == 0 && RdZ80(0x7FF7) == 0);
      fmWrite(0x10, 0x91);
      assert(OPLL.R[0x10] == 0); // I/O disabled until FM-PAC enable bit.
      WrZ80(0x7FF4, 0x10); WrZ80(0x7FF5, 0x92);
      assert(OPLL.R[0x10] == 0x92); // Memory interface does not need enable.
      WrZ80(0x7FF6, 0xFF);
      assert(RdZ80(0x7FF6) == 0x11);
      WrZ80(0x5FFE, 0x4D); WrZ80(0x5FFF, 0x69);
      assert(RdZ80(0x4000) == 'A'); // Bit 4 prevents SRAM unlocking.
      WrZ80(0x7FF6, 1);
      for(byte bank = 0; bank < 4; ++bank) {
        WrZ80(0x7FF7, bank);
        assert(RdZ80(0x4100) == bank && RdZ80(0x7FF7) == bank);
      }
      WrZ80(0x5FFE, 0x4D); WrZ80(0x5FFF, 0x69);
      WrZ80(0x4000, 0xC7);
      assert(RdZ80(0x4000) == 0xC7 && RdZ80(0x6000) == NORAM);
      assert(RdZ80(0x5FFE) == 0x4D && RdZ80(0x5FFF) == 0x69);
      WrZ80(0x7FF7, 2);
      assert(RdZ80(0x4000) == 0xC7 && RdZ80(0x7FF7) == 2);
      WrZ80(0x5FFE, 0);
      assert(RdZ80(0x4100) == 2 && RdZ80(0x6100) == 2);
      WrZ80(0x5FFE, 0x4D);
      assert(RdZ80(0x4000) == 0xC7);
      WrZ80(0x7FF6, 0x11);
      assert(RdZ80(0x4100) == 2);
      WrZ80(0x7FF7, 0);
      WrZ80(0x7FF6, 1);
    }
  } else {
    assert(MemMap[3][0][2] == EmptyRAM);
  }
  fmWrite(0x30, 0x10);
  fmWrite(0x10, 0x80);
  fmWrite(0x20, 0x19);
  // Also exercise the ninth OPLL channel beyond the former 16-channel mixer.
  fmWrite(0x38, 0x10);
  fmWrite(0x18, 0x80);
  fmWrite(0x28, 0x19);
  assert((OPLL.R[0x10] == 0x80) == useFM);
  OutZ80(0xA8, 0xD4); // User cartridge A, pages 1 and 2.
  WrZ80(0x9000, 3);
  assert(ROMMapper[0][2] == 3 && RdZ80(0xA100) == 3);
  WrZ80(0x9000, 0x3F);
  assert(SCCOn[0]); // Mapper remains functional even with SCC sound disabled.
  for(byte i = 0; i < 32; ++i) WrZ80(0x9800 + i, i < 16 ? 96 : 160);
  WrZ80(0x9880, 0x80); WrZ80(0x9881, 1);
  WrZ80(0x988A, 15); WrZ80(0x988F, 1);
  OutZ80(0xA0, 0); OutZ80(0xA1, 0x80);
  OutZ80(0xA0, 1); OutZ80(0xA1, 1);
  OutZ80(0xA0, 7); OutZ80(0xA1, 0x3E);
  OutZ80(0xA0, 8); OutZ80(0xA1, 15);
  flush();
  assert(signal(1));
  assert(signal(1U << AY8910_CHANNELS) == useSCC);
  assert(signal(1U << OPLL.First) == useFM);
  assert(signal(1U << (OPLL.First + 8)) == useFM);
  assert(signal((1U << MAXCHANNELS) - 1));
  if(useFM && useSCC) {
    const int sccFrequency = SCChip.Freq[0];
    fmWrite(0x10, 0x40);
    flush();
    assert(SCChip.Freq[0] == sccFrequency);
    assert(signal(1U << SCChip.First));
    fmWrite(0x20, 0);
    fmWrite(0x28, 0);
    flush();
    assert(!signal(1U << OPLL.First));
    assert(signal(1U << SCChip.First));
  }
  ResetMSX(Mode, RAMPages, VRAMPages);
  assert(!signal((1U << MAXCHANNELS) - 1));
  if(useFM) {
    selectFM();
    assert(RdZ80(0x4000) == 'A');
    if(kind == 2) assert(RdZ80(0x7FF6) == 0 && RdZ80(0x7FF7) == 0);
  }
  TrashMSX();
  assert(NChunks == 0);
  assert(FmsxSetAudioProfile(MsxAudioAuto, nullptr));
}

static void systemSlots()
{
  std::vector<byte> bios(0x8000, 0);
  bios[0] = 0x76;
  writeFile("MSX2.ROM", bios);
  writeFile("MSX2EXT.ROM", std::vector<byte>(0x4000, 0x42));
  auto disk = fmImage(0x4000);
  for(unsigned offset : {0x10U, 0x13U, 0x16U, 0x1CU, 0x1FU}) disk[offset] = 0xC3;
  disk[0x100] = 0x53;
  writeFile("DISK.ROM", disk);
  auto dos = fmImage(0x10000);
  dos[0x100] = 0x64;
  writeFile("MSXDOS2.ROM", dos);
  assert(FmsxSetAudioProfile(MsxAudioPsgSccFm, "fm16.rom"));
  assert(StartMSX(MSX_MSX2 | MSX_MSXDOS2 | MSX_PATCHBDOS, 8, 8));
  assert(ROMData[0] && ROMData[1]);
  assert(ROMData[2] && ROMData[3]); // DOS2 and FM coexist in separate system slots.
  assert(MemMap[3][0][2][0x100] == 0x64);
  assert(MemMap[3][1][0][0x100] == 0x42);
  assert(MemMap[3][3][2][0x100] == 0x53);
  assert(!memcmp(MemMap[0][1][2] + 0x18, "APRLOPLL", 8));
  OutZ80(0xAB, 0x82);
  OutZ80(0xA8, 0);
  WrZ80(0xFFFF, 4);
  assert(RdZ80(0x4000) == 'A');
  fmWrite(0x10, 0x82);
  assert(OPLL.R[0x10] == 0x82);
  TrashMSX();
  for(const char* name : {"MSX2.ROM", "MSX2EXT.ROM", "DISK.ROM", "MSXDOS2.ROM"})
    remove(name);
}

int main(int argc, char** argv)
{
  resolverTests();
  Verbose = 0;
  SndName = CasName = FNTName = nullptr;
  for(unsigned i = 0; i < MAXDRIVES; ++i) DSKName[i] = nullptr;
  std::vector<byte> bios(0x8000, 0);
  bios[0] = 0x76; // HALT; platform callback terminates after one frame.
  writeFile("MSX.ROM", bios);
  std::vector<byte> cart(0x10000, 0);
  for(size_t i = 0; i < cart.size(); ++i) cart[i] = static_cast<byte>(i / 0x2000);
  cart[0] = 'A'; cart[1] = 'B';
  writeFile("cart-a.rom", cart);
  writeFile("cart-b.rom", cart);
  writeFile("fm16.rom", fmImage(0x4000));
  writeFile("fm64.rom", fmImage(0x10000));
  assert(InitSound(22050, 10));
  SetChannels(128, (1 << MAXCHANNELS) - 1);
  for(unsigned profile = 0; profile < MsxAudioProfileCount; ++profile) {
    exercise(profile, "fm16.rom", 1);
    exercise(profile, "fm64.rom", 2);
  }
  exercise(MsxAudioAuto, nullptr, 0);
  systemSlots();
  assert(!FmsxSetAudioProfile(-1, nullptr));
  assert(!FmsxSetAudioProfile(5, nullptr));
  assert(FmsxSetAudioProfile(MsxAudioPsgFm, nullptr));
  assert(!StartMSX(MSX_MSX1, 8, 2));
  TrashMSX();
  assert(FmsxSetAudioProfile(MsxAudioAuto, "missing.rom"));
  assert(!StartMSX(MSX_MSX1, 8, 2));
  TrashMSX();
  writeFile("invalid.rom", std::vector<byte>(0x4000, 0));
  assert(FmsxSetAudioProfile(MsxAudioAuto, "invalid.rom"));
  assert(!StartMSX(MSX_MSX1, 8, 2));
  TrashMSX();
  for(int i = 1; i < argc; ++i) {
    char path[1024], error[512];
    assert(MsxResolveAudioProfile(MsxAudioPsgFm, "nonexistent", path, sizeof(path),
                                 error, sizeof(error), argv[i]));
    // Originals are read-only inputs; core SRAM sidecars stay in the test tree.
    FILE* f = fopen(path, "rb");
    assert(f);
    std::vector<byte> rom;
    int c;
    while((c = fgetc(f)) != EOF) rom.push_back(static_cast<byte>(c));
    fclose(f);
    writeFile("private-fm.rom", rom);
    // Compare against this input's own bytes, never a bundled BIOS fixture.
    assert(FmsxSetAudioProfile(MsxAudioPsgSccFm, "private-fm.rom"));
    assert(StartMSX(MSX_MSX1, 8, 2));
    selectFM();
    assert(!memcmp(MemMap[3][0][2], rom.data(), 0x2000));
    const int kind = FmsxAudioRomKind(rom.data(), rom.size());
    if(kind == 2) {
      assert(RdZ80(0x7FF6) == 0);
      fmWrite(0x10, 0x91);
      assert(OPLL.R[0x10] == 0);
      WrZ80(0x7FF4, 0x10); WrZ80(0x7FF5, 0x92);
      assert(OPLL.R[0x10] == 0x92);
      for(byte bank = 0; bank < 4; ++bank) {
        WrZ80(0x7FF7, bank);
        assert(RdZ80(0x7FF7) == bank);
        assert(!memcmp(MemMap[3][0][2], rom.data() + bank * 0x4000, 0x2000));
        assert(!memcmp(MemMap[3][0][3], rom.data() + bank * 0x4000 + 0x2000, 0x2000));
      }
      WrZ80(0x5FFE, 0x4D); WrZ80(0x5FFF, 0x69);
      WrZ80(0x4100, 0xC7);
      assert(RdZ80(0x4100) == 0xC7 && RdZ80(0x6100) == NORAM);
      WrZ80(0x7FF6, 0x11);
      assert(RdZ80(0x4100) == rom[3 * 0x4000 + 0x100]);
      WrZ80(0x7FF7, 0);
      WrZ80(0x7FF6, 1);
    }
    fmWrite(0x30, 0x10); fmWrite(0x10, 0x80); fmWrite(0x20, 0x19);
    fmWrite(0x38, 0x10); fmWrite(0x18, 0x80); fmWrite(0x28, 0x19);
    flush();
    assert(signal(1U << OPLL.First));
    assert(signal(1U << (OPLL.First + 8)));
    ResetMSX(Mode, RAMPages, VRAMPages);
    selectFM();
    assert(!memcmp(MemMap[3][0][2], rom.data(), 0x2000));
    assert(!signal((1U << MAXCHANNELS) - 1));
    if(kind == 2) assert(RdZ80(0x7FF6) == 0 && RdZ80(0x7FF7) == 0);
    TrashMSX();
    remove("private-fm.rom");
    remove("private-fm.sav");
    puts("Private original FM BIOS: unchanged mapping, bank/register/SRAM behavior, synthesis and reset passed.");
  }
  TrashSound();
  for(const char* name : {"MSX.ROM", "cart-a.rom", "cart-b.rom", "fm16.rom",
                         "fm64.rom", "fm64.sav", "invalid.rom"}) remove(name);
  puts("Audio resolver/core: all five profiles, BIOS formats, mapping, SRAM, gates, independent 20-channel output and reset passed.");
}
