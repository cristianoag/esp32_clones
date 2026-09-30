#include "AppleCore.h"
#include "AppleInput.h"
#include "AppleSettings.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

static AppleCore core;
static uint8_t rom[16384], slot[256];
static char error[160];

static void boot(AppleModel model = AppleModel::IIe)
{
    memset(rom, 0xea, sizeof(rom));
    rom[0x3ffc] = 0;
    rom[0x3ffd] = 0xd0;
    // JMP D000, keeping the synthetic ROM away from hardware registers.
    rom[0x1000] = 0x4c; rom[0x1001] = 0; rom[0x1002] = 0xd0;
    const unsigned offset = model == AppleModel::IIe ? 0 : 4096;
    assert(core.boot(rom + offset, sizeof(rom) - offset, slot, sizeof(slot), model, error, sizeof(error)));
}

static void memoryTests()
{
    boot();
    core.write(0x200, 0x11);
    core.write(0xc005, 0);
    core.write(0x200, 0x22);
    assert(core.read(0x200) == 0x11 && core.ram[1][0x200] == 0x22);
    core.write(0xc003, 0);
    assert(core.read(0x200) == 0x22);
    assert(core.read(0xc013) & 128);
    core.read(0xc002); // IIe RAM switches are write-only.
    assert(core.read(0x200) == 0x22);
    core.write(0xc009, 0);
    core.write(0x42, 0x55);
    core.write(0x142, 0x66);
    assert(core.ram[1][0x42] == 0x55 && core.ram[1][0x142] == 0x66 && !core.ram[0][0x42]);
    core.write(0xc008, 0);
    core.write(0xc001, 0);
    core.read(0xc054);
    core.write(0x400, 0x33);
    core.read(0xc055);
    core.write(0x400, 0x44);
    assert(core.ram[0][0x400] == 0x33 && core.read(0x400) == 0x44);
    core.write(0x2000, 0x77); // 80STORE must not override RAMWRT when HIRES is off.
    assert(core.ram[1][0x2000] == 0x77);
    core.read(0xc057);
    core.read(0xc054);
    core.write(0x2000, 0x88);
    assert(core.ram[0][0x2000] == 0x88 && core.read(0x2000) == 0x88);
    core.write(0xc000, 0);
    assert(core.read(0x2000) == 0x77);

    core.read(0xc082);
    core.write(0xd000, 0x99);
    assert(core.read(0xd000) == 0x4c);
    core.read(0xc083);
    core.write(0xd000, 0xaa);
    assert(core.read(0xd000) == 0);
    core.read(0xc083);
    core.write(0xd000, 0xbb);
    core.write(0xe000, 0xcc);
    assert(core.read(0xd000) == 0xbb);
    core.read(0xc08b); core.read(0xc08b);
    core.write(0xd000, 0xdd);
    assert(core.read(0xd000) == 0xdd && core.read(0xe000) == 0xcc);
    core.read(0xc083);
    assert(core.read(0xd000) == 0xbb);
    core.write(0xc009, 0);
    core.write(0xd000, 0xee);
    assert(core.read(0xd000) == 0xee && core.ram[0][0xc000] == 0xbb);
    core.write(0xc008, 0);
    core.read(0xc082);
    core.write(0xc081, 0); core.read(0xc081);
    core.write(0xd000, 0x12); // A write must not arm the language-card prewrite latch.
    core.read(0xc080);
    assert(core.read(0xd000) == 0xbb);

    core.rom[0x300] = 0x31; core.rom[0x800] = 0x81; core.rom[0x600] = 0x61;
    core.diskRom[0] = 0x62;
    assert(core.read(0xc600) == 0x62);
    assert(core.read(0xc300) == 0x31);
    core.write(0xc00b, 0);
    assert(core.read(0xc800) == 0x81); // C3 access latches internal C8 ROM.
    core.write(0xcfff, 0);
    assert(core.read(0xc800) == core.floatingBus());
    core.write(0xc007, 0);
    assert(core.read(0xc600) == 0x61);
    core.ram[0][0x400] = 0x7a;
    core.reset();
    assert(core.ram[0][0x400] == 0x7a && core.read(0xc600) == 0x62);
    assert(!core.col80 && !core.store80 && core.text);
    assert(!core.boot(rom, 12, slot, sizeof(slot), AppleModel::IIe, error, sizeof(error)));
    assert(core.ram[0][0x400] == 0x7a && error[0]);

    boot(AppleModel::IIPlus);
    core.write(0xc005, 0); core.write(0xc003, 0); core.write(0xc009, 0);
    core.write(0x200, 0x5a);
    assert(core.ram[0][0x200] == 0x5a && core.ram[1][0x200] == 0);
    core.write(0xc00d, 0);
    assert(!core.col80);
}

static void cpuTests()
{
    boot();
    const uint8_t program[] = {0xf8,0x18,0xa9,0x45,0x69,0x55,0x8d,0x00,0x02,0x08,0x68,0x8d,0x01,0x02,0x4c,0x0e,0xd0};
    memcpy(core.rom + 4096, program, sizeof(program));
    for (unsigned i = 0; i < 200; ++i) core.tick();
    assert(core.ram[0][0x200] == 0 && (core.ram[0][0x201] & 1)); // NMOS decimal ADC.
    assert(m6502_pc(&core.cpu) >= 0xd00e && m6502_pc(&core.cpu) <= 0xd011);
}

static void memoryMapTests()
{
    for (unsigned model = 0; model < 2; ++model)
    {
        boot(AppleModel(model));
        for (unsigned flags = 0; flags < 64; ++flags)
        {
            core.write(0xc008 + (flags & 1), 0);
            core.write(0xc002 + ((flags >> 1) & 1), 0);
            core.write(0xc004 + ((flags >> 2) & 1), 0);
            core.write(0xc000 + ((flags >> 3) & 1), 0);
            core.read(0xc054 + ((flags >> 4) & 1));
            core.read(0xc056 + ((flags >> 5) & 1));
            for (unsigned page = 0; page < 0xc0; ++page)
                for (unsigned offset : {0u, 17u, 255u})
                {
                    const uint16_t address = page * 256 + offset;
                    unsigned readBank = model && (flags & 2) ? 1 : 0;
                    unsigned writeBank = model && (flags & 4) ? 1 : 0;
                    if (model && address < 0x200) readBank = writeBank = flags & 1;
                    else if (model && (flags & 8) &&
                             ((address >= 0x400 && address < 0x800) ||
                              ((flags & 32) && address >= 0x2000 && address < 0x4000)))
                        readBank = writeBank = (flags >> 4) & 1;
                    core.ram[0][address] = 0x11;
                    core.ram[1][address] = 0xee;
                    assert(core.read(address) == (readBank ? 0xee : 0x11));
                    core.write(address, 0x55);
                    assert(core.ram[writeBank][address] == 0x55);
                    assert(core.ram[!writeBank][address] == (writeBank ? 0x11 : 0xee));
                }
        }
        core.reset();
        for (unsigned auxiliary = 0; auxiliary < 2; ++auxiliary)
        {
            core.write(0xc008 + auxiliary, 0);
            for (unsigned selection = 0; selection < 16; ++selection)
            {
                core.read(0xc082);
                core.read(0xc080 + selection);
                core.read(0xc080 + selection);
                for (unsigned address : {0xd000u, 0xd017u, 0xdfffu, 0xe000u, 0xffffu})
                {
                    const unsigned bank = model ? auxiliary : 0;
                    const unsigned location = address < 0xe000 && !(selection & 8) ? address - 0x1000 : address;
                    core.ram[0][location] = 0x11;
                    core.ram[1][location] = 0xee;
                    const bool ramRead = (selection & 3) == 0 || (selection & 3) == 3;
                    assert(core.read(address) == (ramRead ? (bank ? 0xee : 0x11) : core.rom[address - 0xc000]));
                    core.write(address, 0x77);
                    assert(core.ram[bank][location] == (selection & 1 ? 0x77 : bank ? 0xee : 0x11));
                    assert(core.ram[!bank][location] == (bank ? 0x11 : 0xee));
                }
            }
        }
    }
}

static void frameStepTests()
{
    boot();
    static AppleCore stepped;
    assert(stepped.boot(rom, sizeof(rom), slot, sizeof(slot), AppleModel::IIe, error, sizeof(error)));
    // Exercise reads, writes, bank changes and the speaker through both stepping APIs.
    const uint8_t program[] = {
        0xa9,0x01,0x8d,0x05,0xc0,0xee,0x00,0x04,0xad,0x30,0xc0,
        0x8d,0x04,0xc0,0xee,0x01,0x04,0x4c,0x00,0xd0
    };
    memcpy(core.rom + 4096, program, sizeof(program));
    memcpy(stepped.rom + 4096, program, sizeof(program));
    int16_t audio[AppleCore::MaxSamples];
    unsigned samples;
    for (unsigned frame = 0; frame < 4; ++frame)
    {
        core.runFrame(nullptr, audio, samples, false);
        for (unsigned i = 0; i < AppleCore::FrameCycles; ++i) stepped.tick();
        assert(core.cycles == stepped.cycles);
        assert(!memcmp(&core.cpu, &stepped.cpu, sizeof(core.cpu)));
        assert(!memcmp(core.ram, stepped.ram, sizeof(core.ram)));
        core.tick();
        stepped.tick();
        assert(!memcmp(&core.cpu, &stepped.cpu, sizeof(core.cpu)));
    }
}

static void diskTests()
{
    boot();
    std::vector<uint8_t> image(AppleDisk::ImageSize, 0xff);
    image[0] = 0xd5; image[1] = 0xaa;
    assert(core.disks[0].attach(image.data(), image.size(), error, sizeof(error)));
    assert(!core.disks[0].attach(image.data(), image.size() - 1, error, sizeof(error)));
    assert(core.disks[0].loaded());
    std::vector<uint8_t> invalid = image;
    invalid.back() = 0x7f;
    assert(!core.disks[0].attach(invalid.data(), invalid.size(), error, sizeof(error)));
    assert(!core.read(0xc0ec));
    core.read(0xc0e9); core.cycles = 32;
    assert(core.read(0xc0ec) == 0xd5);
    assert(!core.read(0xc0ec));
    core.cycles += 32;
    assert(core.read(0xc0ec) == 0xaa);
    core.read(0xc0ed);
    assert(core.read(0xc0ee) == 0x80);
    core.write(0xc0ef, 0);
    core.write(0xc0ed, 0x11); core.write(0xc0ec, 0);
    assert(image[0] == 0xd5 && image[1] == 0xaa);
    core.read(0xc0ee); core.read(0xc0eb);
    core.cycles += 32;
    assert(core.read(0xc0ec) == 0);
    for (unsigned i = 0; i < 10000; ++i)
    {
        core.disks[0].phase((i % 4) * 2 + 1);
        core.disks[0].phase(((i + 3) % 4) * 2);
        assert(core.disks[0].track() < 35);
        core.disks[0].read(i * 32);
    }
    core.disks[0].eject();
    assert(!core.disks[0].loaded() && !core.disks[0].read(1000000));
}

static void diskMotorTests()
{
    boot();
    std::vector<uint8_t> image(AppleDisk::ImageSize, 0xff);
    assert(core.disks[0].attach(image.data(), image.size(), error, sizeof(error)));
    assert(core.disks[1].attach(image.data(), image.size(), error, sizeof(error)));
    core.cycles = 100;
    core.read(0xc0e8); // Stopping a motor that never ran must not start a timer.
    assert(core.read(0xc0ec) == 0);
    core.write(0xc0e9, 0);
    assert(core.read(0xc0ec) == 0xff);
    core.read(0xc0e8);
    const uint64_t deadline = core.cycles + AppleCore::ClockRate;
    core.cycles += 32;
    assert(core.read(0xc0ec) == 0xff);
    core.read(0xc0eb); // Delayed power follows the selected drive.
    assert(core.read(0xc0ec) == 0xff);
    core.cycles = deadline - 1;
    core.write(0xc0e8, 0); // Repeated off accesses do not restart the delay.
    assert(core.read(0xc0ec) == 0xff);
    core.cycles = deadline;
    assert(core.read(0xc0ec) == 0);
    core.cycles += 32;
    assert(core.read(0xc0ec) == 0);

    core.read(0xc0e9);
    core.write(0xc0e8, 0);
    const uint64_t cancelledDeadline = core.cycles + AppleCore::ClockRate;
    core.cycles += 32;
    core.read(0xc0e9); // A new RWTS operation cancels the pending motor stop.
    core.cycles = cancelledDeadline + 32;
    assert(core.read(0xc0ec) == 0xff);
    core.read(0xc0e8);
    core.reset();
    core.cycles += 32;
    assert(core.read(0xc0ec) == 0);
    core.disks[0].eject();
    core.disks[1].eject();
}

static void videoAudioTests()
{
    boot();
    std::vector<uint8_t> pixels(AppleCore::Width * AppleCore::Height + 2, 0xa5);
    core.ram[0][0x400] = 0xc1;
    core.ram[1][0x400] = 0xc2;
    core.write(0xc00d, 0);
    core.render(pixels.data() + 1, false);
    assert(pixels.front() == 0xa5 && pixels.back() == 0xa5);
    std::vector<uint8_t> first = pixels;
    core.ram[1][0x400] = 0xa0;
    core.render(pixels.data() + 1, false);
    bool different = false;
    for (unsigned y = 24; y < 32; ++y)
    {
        for (unsigned x = 40; x < 47; ++x) different |= pixels[1 + y * 640 + x] != first[1 + y * 640 + x];
        for (unsigned x = 47; x < 54; ++x) assert(pixels[1 + y * 640 + x] == first[1 + y * 640 + x]);
    }
    assert(different); // 80-column AUX and MAIN characters must remain independently legible.
    core.write(0xc00c, 0);
    core.render(pixels.data() + 1, false);
    first = pixels;
    core.ram[0][0x400] = 0x81; // Normal IIe 80-9F aliases C0-DF, not PC control glyphs.
    core.render(pixels.data() + 1, false);
    assert(pixels == first);
    for (unsigned y = 24; y < 32; ++y)
        for (unsigned x = 40; x < 600; x += 2) assert(pixels[1 + y * 640 + x] == pixels[2 + y * 640 + x]);
    core.read(0xc050); core.read(0xc056);
    core.ram[0][0x400] = 0x0f;
    core.render(pixels.data() + 1, false);
    assert(pixels[1 + 24 * 640 + 40] == 255 && pixels[1 + 28 * 640 + 40] == 0);
    core.read(0xc057);
    core.ram[0][0x2000] = 3;
    core.render(pixels.data() + 1, false);
    assert(pixels[1 + 24 * 640 + 40] == 255);
    core.read(0xc053);
    core.render(pixels.data() + 1, false);
    core.read(0xc051); // TEXT has priority over MIXED and HIRES.
    core.render(pixels.data() + 1, false);
    core.read(0xc050); core.read(0xc052);
    core.write(0xc00d, 0); core.read(0xc05e);
    core.ram[1][0x2000] = 0x7f; core.ram[0][0x2000] = 0;
    core.render(pixels.data() + 1, true);
    assert(pixels[1 + 24 * 640 + 40] != 0 && pixels[1 + 24 * 640 + 47] == 0);
    assert(pixels.front() == 0xa5 && pixels.back() == 0xa5);

    int16_t audio[AppleCore::MaxSamples + 1];
    unsigned count;
    uint64_t total = 0;
    const uint64_t start = core.cycles;
    for (unsigned frame = 0; frame < 60; ++frame)
    {
        audio[AppleCore::MaxSamples] = 1234;
        core.runFrame(nullptr, audio, count, false);
        assert(count <= AppleCore::MaxSamples && audio[AppleCore::MaxSamples] == 1234);
        total += count;
    }
    assert(core.cycles - start == 60 * AppleCore::FrameCycles);
    assert(total == uint64_t(60) * AppleCore::FrameCycles * AppleCore::AudioRate / AppleCore::ClockRate);
    assert(audio[0] < 0);
    core.read(0xc033);
    core.runFrame(nullptr, audio, count, false);
    assert(audio[0] > 0);
}

static void renderEquivalenceTests()
{
    std::vector<uint8_t> pixels(AppleCore::Width * AppleCore::Height);
    uint32_t hash = 2166136261u;
    for (unsigned model = 0; model < 2; ++model)
    {
        boot(AppleModel(model));
        for (unsigned bank = 0; bank < 2; ++bank)
            for (unsigned address = 0; address < 65536; ++address)
                core.ram[bank][address] = (address * 37 + address / 17 + bank * 113) & 255;
        for (unsigned mode = 0; mode < 8; ++mode)
            for (unsigned option = 0; option < 32; ++option)
            {
                core.text = mode < 2;
                core.hires = mode >= 4;
                core.col80 = mode & 1;
                core.doubleHires = mode & 1;
                core.mixed = mode >= 6;
                core.page2 = option & 1;
                core.store80 = option & 2;
                core.altCharset = option & 4;
                core.frames = option & 16;
                core.render(pixels.data(), option & 8);
                for (uint8_t pixel : pixels) hash = (hash ^ pixel) * 16777619u;
            }
    }
    assert(hash == 0x720f406du); // Pixel-for-pixel aggregate from the pre-optimization renderer.
}

static void renderInvalidationTests()
{
    boot();
    std::vector<uint8_t> pixels(AppleCore::Width * AppleCore::Height);
    assert(core.needsRender(false));
    core.render(pixels.data(), false);
    assert(!core.needsRender(false));
    core.frames = 16;
    assert(!core.needsRender(false)); // Inverse characters do not flash.
    core.write(0x200, 1);
    assert(!core.needsRender(false));
    core.write(0x400, 0xc1);
    assert(core.needsRender(false));
    core.render(pixels.data(), false);
    core.write(0x400, 0xc1);
    assert(!core.needsRender(false));
    core.write(0x400, 0x41);
    core.render(pixels.data(), false);
    core.frames = 31;
    assert(!core.needsRender(false));
    core.frames = 32;
    assert(core.needsRender(false));
    core.render(pixels.data(), false);
    assert(!core.needsRender(false) && core.needsRender(true));
    core.render(pixels.data(), true);
    assert(!core.needsRender(true));
    core.write(0xc00f, 0);
    assert(core.needsRender(true));
    core.render(pixels.data(), true);
    core.frames = 48;
    assert(!core.needsRender(true)); // The alternate character set disables flashing.
    core.write(0xc00d, 0);
    core.render(pixels.data(), false);
    core.write(0xc005, 0);
    core.write(0x400, 0xc2);
    assert(core.needsRender(false)); // AUX text writes also invalidate.
    core.render(pixels.data(), false);
    core.write(0x800, 1);
    assert(!core.needsRender(false)); // Writes to the hidden page must not redraw.
    core.render(pixels.data(), false);
    core.write(0x2000, 1);
    assert(!core.needsRender(false)); // DOS loading hi-res RAM must not redraw a text screen.
    core.render(pixels.data(), false);
    core.write(0x4000, 1);
    assert(!core.needsRender(false));
    core.render(pixels.data(), false);
    core.read(0xc055);
    assert(core.needsRender(false));
    core.render(pixels.data(), false);
    core.read(0xc054);
    core.write(0x800, 2); // Modify the cached page while it is temporarily hidden.
    core.read(0xc055);
    assert(core.needsRender(false));
    core.render(pixels.data(), false);
    core.read(0xc050); core.read(0xc057); core.read(0xc05e);
    assert(core.needsRender(false));
    core.render(pixels.data(), false);
    core.read(0xc053);
    assert(core.needsRender(false));
    core.render(pixels.data(), false);
    core.write(0xc001, 0);
    assert(core.needsRender(false));
    core.render(pixels.data(), false);
    core.reset();
    assert(core.needsRender(false));
}

static void inputSettingsTests()
{
    assert(AppleAscii(4, 0, true) == 'A' && AppleAscii(4, 0, false) == 'a');
    assert(AppleAscii(4, 2, true) == 'a' && AppleAscii(4, 1, true) == 1);
    assert(AppleAscii(31, 3, false) == 0);
    assert(AppleAscii(69, 0, true) == -1);
    assert(AppleAscii(51, 0, false) == ';' && AppleAscii(51, 2, false) == ':');
    assert(AppleAscii(56, 2, false) == '?' && AppleAscii(80, 0, false) == 8);
    boot();
    core.key('A');
    assert(core.keyPending() && core.read(0xc000) == 0xc1);
    core.input(true, 0x44, 0);
    assert(core.read(0xc010) == 0xc1 && !core.keyPending());
    assert((core.read(0xc061) & 128) && (core.read(0xc062) & 128));
    core.input(false, 0, 4);
    assert(core.read(0xc010) == 'A');
    core.read(0xc070);
    assert(core.read(0xc064) & 128);
    core.cycles += 9;
    assert(!(core.read(0xc064) & 128));
    core.input(false, 0, 8);
    core.read(0xc070);
    core.cycles += 2000;
    assert(core.read(0xc064) & 128);
    core.cycles += 1000;
    assert(!(core.read(0xc064) & 128));
    AppleSettings settings, decoded;
    assert(AppleValidSettings(settings));
    assert(AppleDecodeSettings(&settings, sizeof(settings), decoded));
    settings.volume = 101;
    assert(!AppleDecodeSettings(&settings, sizeof(settings), decoded) && decoded.volume == 70);
    assert(!AppleValidPath("/apple2/../file.rom"));
    assert(!AppleValidPath("/apple2//file.rom"));
    assert(!AppleValidPath("/apple2\\file.rom"));
    assert(AppleValidPath("", true) && !AppleValidPath(""));
    memset(settings.rom[0], 'a', ApplePathSize);
    assert(!AppleValidSettings(settings));
}

int main()
{
#ifdef _WIN32
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    memoryTests();
    memoryMapTests();
    cpuTests();
    frameStepTests();
    diskTests();
    diskMotorTests();
    videoAudioTests();
    renderEquivalenceTests();
    renderInvalidationTests();
    inputSettingsTests();
    puts("Apple II+/IIe core, banking, CPU, media, video, audio, input and settings tests passed.");
}
