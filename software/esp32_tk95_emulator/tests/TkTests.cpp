#include "TkCore.h"
#include "TkInput.h"
#include "TkSettings.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static unsigned checks = 0;
#define CHECK(condition) do { ++checks; if (!(condition)) { \
    std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); std::exit(1); } } while (0)

static char error[160];
static TkCore core;
static uint8_t rom[16384];
static int16_t audio[448];
static unsigned samples;

static void boot(TkTiming timing = TkTiming::Hz60)
{
    std::memset(rom, 0, sizeof(rom));
    rom[0] = 0xf3; rom[1] = 0x76; // DI; HALT: synthetic, ROM-free fixture.
    CHECK(core.boot(rom, sizeof(rom), TkModel::TK95, timing, error, sizeof(error)));
}

static void cpu()
{
    boot();
    const uint8_t program[] = {
        0xf3, 0x31,0x00,0xff, 0x3e,0x12, 0x32,0x00,0x00,
        0x21,0x00,0x80, 0x36,0x81, 0xcb,0x06,
        0xdd,0x21,0x10,0x80, 0xdd,0x36,0x02,0x11,
        0xdd,0xcb,0x02,0x06, 0x3e,0x02, 0xd3,0xfe,
        0x01,0xfe,0xfd, 0xed,0x78, 0x32,0x01,0x80,
        0xfb,0x76,0x18,0xfd
    };
    core.memory[0] = 0xc3; core.memory[1] = 0; core.memory[2] = 1;
    std::memcpy(core.memory + 0x100, program, sizeof(program));
    const uint8_t irq[] = {0x21,0x20,0x80,0x34,0xfb,0xed,0x4d};
    std::memcpy(core.memory + 0x38, irq, sizeof(irq));
    uint8_t rows[8]; std::memset(rows, 255, 8); rows[1] = 0xfe;
    core.input(rows, 0x11);
    core.cpu.im = 1;
    core.runFrame(nullptr, audio, samples);
    CHECK(core.memory[0] == 0xc3);
    CHECK(core.memory[0x8000] == 3);
    CHECK(core.memory[0x8012] == 0x22);
    CHECK(core.memory[0x8001] == 0x3e);
    CHECK(core.border == 2);
    CHECK(core.readPort(0x1f) == 0x11);
    CHECK(core.cpu.iff1);
    core.runFrame(nullptr, audio, samples);
    CHECK(core.memory[0x8020] == 1);
    core.runFrame(nullptr, audio, samples);
    CHECK(core.memory[0x8020] == 2);
    core.reset();
    CHECK(core.cpu.pc == 0 && core.frames == 0 && core.cycle == 0);
    CHECK(core.memory[0x8012] == 0x22);
    CHECK(!core.boot(rom, 100, TkModel::TK95, TkTiming::Hz60, error, sizeof(error)));
    CHECK(core.memory[0x8012] == 0x22);
}

static void video()
{
    CHECK(TkGetClock(TkTiming::Hz50).frameCycles == 228 * 312);
    CHECK(TkGetClock(TkTiming::Hz60).frameCycles == 228 * 262);
    CHECK(TkColor(2, true) == 7 && TkColor(1, true) == 192 && TkColor(4, true) == 56);
    for (TkTiming timing : {TkTiming::Hz50, TkTiming::Hz60})
    {
        boot(timing);
        core.writePort(0xfe, 4);
        std::vector<uint8_t> pixels(320 * 240, 255);
        for (unsigned y = 0; y < 192; ++y)
        {
            const unsigned address = 0x4000 + ((y & 0xc0) << 5) + ((y & 7) << 8) + ((y & 0x38) << 2);
            std::memset(core.memory + address, 0x80, 32);
        }
        std::memset(core.memory + 0x5800, 0x80 | 0x40 | (1 << 3) | 2, 768);
        core.runFrame(pixels.data(), audio, samples);
        CHECK(samples <= 439 && samples >= 368);
        for (unsigned y = 0; y < 240; ++y)
            for (unsigned x = 0; x < 320; ++x)
                CHECK(pixels[y * 320 + x] == ((y < 24 || y >= 216 || x < 32 || x >= 288) ?
                      TkColor(4, false) : ((x & 7) == 0 ? TkColor(2, true) : TkColor(1, true))));
        core.frames = 16;
        core.runFrame(pixels.data(), audio, samples);
        CHECK(pixels[24 * 320 + 32] == TkColor(1, true));
        CHECK(pixels[24 * 320 + 33] == TkColor(2, true));
        core.cycle = TkGetClock(timing).firstLine * 228 + 99;
        CHECK(core.floatingBus() == 0x80);
        ++core.cycle;
        CHECK(core.floatingBus() == 0xca);
        core.cycle += 3;
        CHECK(core.floatingBus() == 255);
        core.cycle = TkGetClock(timing).firstLine * 228 + 93;
        CHECK(core.contention(0x4000, false) == 6);
        CHECK(core.contention(0x8000, false) == 0);
        CHECK(core.contention(0x00fe, true) == 6);
        CHECK(core.contention(0x00ff, true) == 0);
        core.cycle = 0;
        CHECK(core.floatingBus() == 255 && core.contention(0x4000, false) == 0);
        unsigned total = 0;
        core.reset();
        for (unsigned frame = 0; frame < 60; ++frame)
        {
            core.runFrame(nullptr, audio, samples);
            total += samples;
        }
        const unsigned expected = uint64_t(TkGetClock(timing).frameMicros) * 60 * TkCore::AudioRate / 1000000;
        CHECK(total >= expected - 1 && total <= expected + 1);
    }
}

static void input()
{
    uint8_t report[8] = {}, rows[8];
    report[2] = 4; report[3] = 69; // A plus host-only F12.
    TkKeyboard(report, rows);
    CHECK(rows[1] == 0xfe);
    CHECK(rows[0] == 255 && rows[7] == 255);
    report[0] = 0x22 | 0x11;
    report[4] = 80;
    TkKeyboard(report, rows);
    CHECK(!(rows[0] & 1) && !(rows[7] & 2) && !(rows[3] & 16));
    std::memset(rows, 255, 8);
    CHECK(TkApplyJoystick(rows, 1 | 8 | 16, TkJoystick::Kempston) == (8 | 1 | 16));
    CHECK(TkApplyJoystick(rows, 1 | 4 | 16, TkJoystick::Sinclair1) == 0);
    CHECK(rows[4] == uint8_t(255 & ~19));
    TkApplyJoystick(rows, 1 | 4 | 16, TkJoystick::Sinclair2);
    CHECK(rows[3] == uint8_t(255 & ~25));
    boot();
    core.input(rows, 0);
    CHECK((core.readPort(0xe7fe) & 31) == ((rows[3] & rows[4]) & 31));
    core.portuguese = false;
    CHECK(core.readPort(0xfffe) == 0xbf);
    core.writePort(0xfe, 16);
    CHECK(core.readPort(0xfffe) == 0xff);
    core.portuguese = true;
    CHECK(core.readPort(0xfffe) == 0x7f);
}

static void tape()
{
    TkTape tape;
    const uint8_t valid[] = {2,0,0,0, 2,0,255,255};
    CHECK(tape.attach(valid, sizeof(valid), error, sizeof(error)));
    CHECK(tape.loaded() && !tape.playing());
    tape.play(true);
    for (unsigned i = 0; i < 2167; ++i) CHECK(!tape.tick());
    CHECK(tape.tick());
    tape.play(false);
    for (unsigned i = 0; i < 100; ++i) CHECK(tape.tick());
    tape.play(true);
    for (unsigned i = 0; i < 2167; ++i) CHECK(tape.tick());
    CHECK(!tape.tick());
    uint8_t bad[] = {2,0,0,1};
    CHECK(!tape.attach(bad, sizeof(bad), error, sizeof(error)));
    CHECK(tape.playing() && tape.position() == 2);
    for (size_t size = 0; size < 4; ++size)
        CHECK(!tape.attach(valid, size, error, sizeof(error)));
    tape.rewind();
    CHECK(!tape.playing() && tape.position() == 0);
    tape.play(true);
    unsigned ticks = 0;
    while (tape.playing() && ticks < 40000000) { tape.tick(); ++ticks; }
    // Exact pilot, sync, two pulses/bit and pause for both blocks.
    const unsigned expected = (8063 + 3223) * 2168 + 2 * (667 + 735 + 3579545) +
                              16 * 2 * 855 + 16 * 2 * 1710;
    CHECK(ticks == expected && !tape.playing());
    CHECK(tape.position() == sizeof(valid));
    tape.play(true);
    CHECK(!tape.playing());
    tape.rewind();
    tape.play(true);
    CHECK(tape.playing());
    tape.eject();
    CHECK(!tape.loaded() && !tape.playing());
}

static void snapshots()
{
    boot();
    std::vector<uint8_t> sna(49179, 0x55);
    std::fill(sna.begin(), sna.begin() + 27, 0);
    sna[23] = 0; sna[24] = 0x80; sna[25] = 1; sna[26] = 5; sna[19] = 4;
    sna[27 + 0x4000] = 0x34; sna[27 + 0x4001] = 0x12;
    CHECK(TkLoadSnapshot(core, sna.data(), sna.size(), true, error, sizeof(error)));
    CHECK(core.cpu.pc == 0x1234 && core.cpu.sp == 0x8002 && core.cpu.iff1);
    CHECK(core.border == 5 && core.memory[0x6000] == 0x55 && core.memory[0] == 0xf3);
    sna[24] = 0x3f;
    CHECK(!TkLoadSnapshot(core, sna.data(), sna.size(), true, error, sizeof(error)));
    CHECK(core.cpu.pc == 0x1234 && core.memory[0x6000] == 0x55);
    CHECK(!TkLoadSnapshot(core, sna.data(), sna.size() - 1, true, error, sizeof(error)));
    std::vector<uint8_t> z80(49182, 0x66);
    std::fill(z80.begin(), z80.begin() + 30, 0);
    z80[6] = 0x78; z80[7] = 0x56; z80[12] = 0x0f; z80[29] = 2;
    CHECK(TkLoadSnapshot(core, z80.data(), z80.size(), false, error, sizeof(error)));
    CHECK(core.cpu.pc == 0x5678 && core.cpu.im == 2 && core.cpu.r == 128 && core.border == 7);
    CHECK(core.memory[0x4000] == 0x66 && core.memory[0xffff] == 0x66);
    std::vector<uint8_t> compressed(30, 0);
    compressed[6] = 1; compressed[12] = 32;
    for (unsigned left = 49152; left;)
    {
        const unsigned count = std::min(255u, left);
        compressed.insert(compressed.end(), {0xed,0xed,uint8_t(count),0x88});
        left -= count;
    }
    compressed.insert(compressed.end(), {0,0xed,0xed,0});
    CHECK(TkLoadSnapshot(core, compressed.data(), compressed.size(), false, error, sizeof(error)));
    CHECK(core.cpu.pc == 1 && core.memory[0x4000] == 0x88 && core.memory[0xffff] == 0x88);
    compressed[32] = 0;
    CHECK(!TkLoadSnapshot(core, compressed.data(), compressed.size(), false, error, sizeof(error)));
    CHECK(core.cpu.pc == 1 && core.memory[0x4000] == 0x88);
    for (unsigned header : {23u,54u,55u})
    {
        std::vector<uint8_t> extended(32 + header, 0);
        extended[30] = header; extended[32] = 0x56; extended[33] = 0x34;
        for (uint8_t page : {8,4,5})
        {
            extended.insert(extended.end(), {255,255,page});
            extended.insert(extended.end(), 16384, page);
        }
        CHECK(TkLoadSnapshot(core, extended.data(), extended.size(), false, error, sizeof(error)));
        CHECK(core.cpu.pc == 0x3456);
        CHECK(core.memory[0x4000] == 8 && core.memory[0x8000] == 4 && core.memory[0xc000] == 5);
        extended[34] = 4;
        CHECK(!TkLoadSnapshot(core, extended.data(), extended.size(), false, error, sizeof(error)));
        extended[34] = 0;
        CHECK(!TkLoadSnapshot(core, extended.data(), extended.size() - 1, false, error, sizeof(error)));
        extended[32 + header + 2] = 5;
        CHECK(!TkLoadSnapshot(core, extended.data(), extended.size(), false, error, sizeof(error)));
        CHECK(core.cpu.pc == 0x3456 && core.memory[0x4000] == 8);
    }
}

static void settings()
{
    TkSettings settings, decoded;
    CHECK(TkValidSettings(settings));
    CHECK(settings.version == 2);
    CHECK(!std::strcmp(settings.rom[0], "/tk/bios/tk95.rom"));
    CHECK(!std::strcmp(settings.rom[1], "/tk/bios/tk90.rom"));
    CHECK(TkDecodeSettings(&settings, sizeof(settings), decoded));
    CHECK(!std::strcmp(decoded.rom[0], settings.rom[0]));
    CHECK(!std::strcmp(decoded.rom[1], settings.rom[1]));
    TkSettings legacy = settings;
    legacy.version = 1;
    std::strcpy(legacy.rom[0], "/bios/tk95.rom");
    std::strcpy(legacy.rom[1], "/bios/tk90.rom");
    std::strcpy(legacy.tape, "/games/example.tap");
    legacy.volume = 40;
    CHECK(TkDecodeSettings(&legacy, sizeof(legacy), decoded));
    CHECK(decoded.version == 2 && decoded.volume == 40);
    CHECK(!std::strcmp(decoded.rom[0], settings.rom[0]));
    CHECK(!std::strcmp(decoded.rom[1], settings.rom[1]));
    CHECK(!std::strcmp(decoded.tape, legacy.tape));
    std::strcpy(legacy.rom[0], "/custom/my-tk95.rom");
    CHECK(TkDecodeSettings(&legacy, sizeof(legacy), decoded));
    CHECK(!std::strcmp(decoded.rom[0], legacy.rom[0]));
    CHECK(!std::strcmp(decoded.rom[1], settings.rom[1]));
    std::memset(legacy.rom[0], 'x', TkPathSize);
    CHECK(!TkDecodeSettings(&legacy, sizeof(legacy), decoded));
    legacy = settings;
    legacy.version = 3;
    CHECK(!TkDecodeSettings(&legacy, sizeof(legacy), decoded));
    legacy = settings;
    std::strcpy(legacy.rom[0], "/bios/tk95.rom");
    CHECK(TkDecodeSettings(&legacy, sizeof(legacy), decoded));
    CHECK(!std::strcmp(decoded.rom[0], legacy.rom[0]));
    CHECK(TkDecodeSettings(&settings, sizeof(settings), decoded));
    CHECK(!TkDecodeSettings(&settings, sizeof(settings) - 1, decoded));
    CHECK(!TkValidPath(nullptr) && !TkValidPath("/") && TkValidPath("", true));
    for (const char *path : {"/a/../b", "/a//b", "/./a", "a.rom", "/a\\b", "/a:b"})
        CHECK(!TkValidPath(path));
    CHECK(TkValidPath("/tk/roms/Test.ROM"));
    settings.volume = 101;
    CHECK(!TkDecodeSettings(&settings, sizeof(settings), decoded));
    CHECK(decoded.volume == 70);
    settings.volume = 100;
    std::memset(settings.tape, 'a', sizeof(settings.tape));
    CHECK(!TkValidSettings(settings));
    settings.tape[0] = 0;
    settings.model = TkModel(2);
    CHECK(!TkValidSettings(settings));
}

int main()
{
    cpu(); video(); input(); tape(); snapshots(); settings();
    std::printf("TK core/media/input/settings: %u checks passed.\n", checks);
}
