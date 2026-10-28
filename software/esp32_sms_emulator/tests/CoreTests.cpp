#include "SmsCore.h"
#include "SmsInput.h"
#include <sms_internal.h>
#include <algorithm>
#include <assert.h>
#include <chrono>
#include <stdio.h>
#include <string.h>
#include <vector>

static std::vector<uint8_t> cartridge(size_t size = 32768)
{
    std::vector<uint8_t> rom(size);
    const uint8_t program[] = {
        0xf3, 0x31, 0xf0, 0xdf,
        0x3e, 0x04, 0xd3, 0xbf, 0x3e, 0x80, 0xd3, 0xbf,
        0x3e, 0x40, 0xd3, 0xbf, 0x3e, 0x81, 0xd3, 0xbf,
        0x3e, 0x00, 0xd3, 0xbf, 0x3e, 0xc0, 0xd3, 0xbf,
        0x3e, 0x03, 0xd3, 0xbe,
        0x3e, 0x80, 0xd3, 0x7f, 0x3e, 0x10, 0xd3, 0x7f,
        0x3e, 0x90, 0xd3, 0x7f,
        0xc3, 0x2c, 0x00
    };
    memcpy(rom.data(), program, sizeof(program));
    const uint8_t nmi[] = {0x21, 0x00, 0xc0, 0x34, 0xed, 0x45};
    memcpy(rom.data() + 0x66, nmi, sizeof(nmi));
    return rom;
}

static void settingsAndInput()
{
    SmsSettings settings, decoded;
    assert(SmsDecodeSettings(&settings, sizeof(settings), decoded));
    assert(!SmsDecodeSettings(&settings, sizeof(settings) - 1, decoded));
    assert(!SmsDecodeSettings(nullptr, sizeof(settings), decoded));
    const char *invalid[] = {"relative.sms", "/", "/../rom.sms", "/a/./rom.sms",
                             "/a//b.sms", "/a\\b.sms", "/c:rom.sms", "/rom.sms/"};
    for (const char *path : invalid) assert(!SmsValidPath(path));
    assert(SmsValidPath("/sms/roms/My Game.sms"));
    assert(SmsValidPath("", true) && !SmsValidPath(""));
    settings.rom[0] = '/';
    memset(settings.rom + 1, 'a', sizeof(settings.rom) - 1);
    assert(!SmsValidSettings(settings));
    settings = SmsSettings{};
    settings.volume = 101;
    assert(!SmsValidSettings(settings));
    settings = SmsSettings{};
    settings.joystick[1] = 2;
    assert(!SmsValidSettings(settings));
    uint8_t report[8] = {}, enabled[2] = {1, 1};
    assert(SmsInput(report, 0x3f3f, enabled) == 0xfff);
    enabled[0] = 0;
    assert(SmsInput(report, 0x3f3f, enabled) == 0xfc0);
    enabled[1] = 0;
    assert(!SmsInput(report, 0x3f3f, enabled));
    bool waitForRelease = true;
    assert(SmsFilterMenuPause((1 << 13) | 16, waitForRelease) == 16 && waitForRelease);
    assert(!SmsFilterMenuPause(1 << 13, waitForRelease) && waitForRelease);
    assert(!SmsFilterMenuPause(0, waitForRelease) && !waitForRelease);
    assert(SmsFilterMenuPause(1 << 13, waitForRelease) == (1 << 13));
    report[2] = 82; report[3] = 29; report[4] = 27; report[5] = 40; report[6] = 69;
    assert(SmsInput(report, 0, enabled) == (1 | 16 | 32 | (1 << 13)));
    report[2] = 1;
    assert(!SmsInput(report, 0, enabled));
    memset(report, 0, sizeof(report));
    report[2] = 26; report[3] = 10; report[4] = 11; report[5] = 21;
    assert(SmsInput(report, 0, enabled) == ((1 << 6) | (1 << 10) | (1 << 11) | (1 << 12)));
    for (unsigned red = 0; red < 4; ++red)
        for (unsigned green = 0; green < 4; ++green)
            for (unsigned blue = 0; blue < 4; ++blue)
            {
                const uint8_t colour = SmsColour(red, green, blue);
                assert(((colour >> 1) & 3) == red);
                assert(((colour >> 4) & 3) == green);
                assert((colour >> 6) == blue);
            }
    uint8_t source[256], destination[322] = {};
    for (unsigned i = 0; i < 256; ++i) source[i] = i;
    destination[0] = destination[321] = 0xa5;
    SmsVideoRow(destination + 1, source);
    assert(destination[0] == 0xa5 && destination[321] == 0xa5);
    for (unsigned i = 0; i < 320; ++i) assert(destination[i + 1] == source[i * 4 / 5]);
    for (unsigned sourceOffset = 0; sourceOffset < 4; ++sourceOffset)
        for (unsigned destinationOffset = 0; destinationOffset < 4; ++destinationOffset)
        {
            uint8_t input[260], output[328];
            for (unsigned i = 0; i < sizeof(input); ++i) input[i] = uint8_t(i * 73 + 19);
            memset(output, 0xa5, sizeof(output));
            const unsigned start = destinationOffset + 1;
            SmsVideoRow(output + start, input + sourceOffset);
            for (unsigned i = 0; i < sizeof(output); ++i)
                assert(output[i] == (i >= start && i < start + 320 ?
                       input[sourceOffset + (i - start) * 4 / 5] : 0xa5));
        }
}

static void loaderAndFrames()
{
    SmsCore machine;
    SmsSettings settings;
    char error[160];
    std::vector<uint8_t> pixels(SmsCore::Width * SmsCore::Height + 2, 0xa5);
    auto rom = cartridge();
    size_t offset, length;
    assert(!SmsInspectRom(nullptr, 32768, offset, length, error, sizeof(error)));
    for (size_t size : {size_t(0), size_t(8191), size_t(8193), size_t(16385), size_t(1048577)})
        assert(!SmsInspectRom(rom.data(), size, offset, length, error, sizeof(error)));
    assert(machine.load(rom.data(), rom.size(), settings, pixels.data() + 1, error, sizeof(error)));
    assert(!strcmp(machine.mapperName(), "Sega"));
    unsigned total = 0, audible = 0;
    for (unsigned frame = 0; frame < 60; ++frame)
    {
        machine.runFrame(0, true);
        const unsigned count = machine.audioCount();
        assert(count >= 350 && count <= 400);
        total += count;
        for (unsigned i = 0; i < count; ++i) audible += machine.audio()[i] != 0;
    }
    assert(total >= 21900 && total <= 22200 && audible > 20000);
    assert(machine.frames == 60 && machine.height() == 192);
    assert(machine.frameMicros() >= 16666 && machine.frameMicros() <= 16667);
    assert(pixels.front() == 0xa5 && pixels.back() == 0xa5);
    assert(std::count(pixels.begin() + 1, pixels.end() - 1, SmsColour(3, 0, 0)) >= 48000);
    const auto before = pixels;
    machine.runFrame(0, false);
    assert(pixels == before);
    auto gear = rom;
    memcpy(gear.data() + 0x7ff0, "TMR SEGA", 8);
    gear[0x7fff] = 0x5c;
    assert(!machine.load(gear.data(), gear.size(), settings, pixels.data() + 1, error, sizeof(error)));
    assert(machine.frames == 61 && pixels == before);
    assert(!machine.load(rom.data(), 100, settings, pixels.data() + 1, error, sizeof(error)));
    machine.runFrame(0, true);
    assert(machine.frames == 62);
    machine.reset();
    assert(machine.frames == 0);
    machine.runFrame(0, true);
    assert(machine.frames == 1);
    settings.pal = 1;
    assert(machine.load(rom.data(), rom.size(), settings, pixels.data() + 1, error, sizeof(error)));
    total = 0;
    for (unsigned i = 0; i < 50; ++i)
    {
        machine.runFrame(0, true);
        assert(machine.audioCount() >= 425 && machine.audioCount() <= 460);
        total += machine.audioCount();
    }
    assert(total >= 21900 && total <= 22200);
    assert(machine.frameMicros() == 20000);
    for (size_t size : {size_t(8192), size_t(16384), size_t(49152), size_t(1048576)})
    {
        auto small = cartridge(size);
        const size_t header = size >= 32768 ? 0x7ff0 : size - 16;
        memcpy(small.data() + header, "TMR SEGA", 8);
        small[header + 15] = size == 8192 ? 0x4a : size == 16384 ? 0x4b :
                              size == 49152 ? 0x4d : 0x42;
        assert(machine.load(small.data(), small.size(), settings, pixels.data() + 1, error, sizeof(error)));
        machine.runFrame(0, true);
    }
    std::vector<uint8_t> headered(512, 0xff);
    headered.insert(headered.end(), rom.begin(), rom.end());
    assert(machine.load(headered.data(), headered.size(), settings, pixels.data() + 1, error, sizeof(error)));
    assert(SmsInspectRom(headered.data(), headered.size(), offset, length, error, sizeof(error)) && offset == 512 && length == 32768);
    for (auto mapper : {SmsMapper::Sega, SmsMapper::Codemasters, SmsMapper::Korean})
    {
        settings.mapper = mapper;
        assert(machine.load(rom.data(), rom.size(), settings, pixels.data() + 1, error, sizeof(error)));
        machine.runFrame(0, true);
        machine.reset();
        machine.runFrame(0, true);
    }
    assert(pixels.front() == 0xa5 && pixels.back() == 0xa5);
    auto legacyMode = cartridge();
    legacyMode[5] = 0;
    const uint8_t backdrop[] = {0x3e, 0x0f, 0xd3, 0xbf, 0x3e, 0x87, 0xd3, 0xbf, 0xc3, 0x34, 0x00};
    memcpy(legacyMode.data() + 44, backdrop, sizeof(backdrop));
    assert(machine.load(legacyMode.data(), legacyMode.size(), settings, pixels.data() + 1, error, sizeof(error)));
    machine.runFrame(0, true);
    assert(std::count(pixels.begin() + 1, pixels.end() - 1, SmsColour(3, 3, 3)) >= 48000);
    assert(pixels.front() == 0xa5 && pixels.back() == 0xa5);
    for (unsigned pal = 0; pal < 2; ++pal)
        for (unsigned height : {224u, 240u})
        {
            settings.pal = pal;
            auto extended = cartridge();
            extended[5] = 0x06;
            extended[13] = height == 224 ? 0x50 : 0x48;
            assert(machine.load(extended.data(), extended.size(), settings, pixels.data() + 1, error, sizeof(error)));
            machine.runFrame(0, true);
            machine.runFrame(0, true);
            assert(machine.height() == height);
            assert(std::count(pixels.begin() + 1, pixels.end() - 1, SmsColour(3, 0, 0)) >= int(256 * height - 512));
            assert(pixels.front() == 0xa5 && pixels.back() == 0xa5);
        }
}

static void hardwareCore()
{
    SMS_Core sms = {};
    static_assert(sizeof(SMS_Core) <= 32 * 1024, "Hot core must fit in internal SRAM.");
    assert(SMS_init(&sms));
    assert(sms.cart_ram.ram && sms.vdp.cached_palette);
    auto rom = cartridge(65536);
    for (unsigned bank = 0; bank < 4; ++bank) rom[bank * 16384 + 1024] = bank + 10;
    assert(SMS_loadromEx(&sms, rom.data(), rom.size(), SMS_System_SMS, -1, -1));
    assert(SMS_read8(&sms, 0x4400) == 11);
    SMS_write8(&sms, 0xfffe, 3);
    assert(SMS_read8(&sms, 0x4400) == 13);
    SMS_write8(&sms, 0xfffd, 2);
    assert(SMS_read8(&sms, 0x400) == 12 && SMS_read8(&sms, 0) == 0xf3);
    SMS_write8(&sms, 0xc123, 0x5a);
    assert(SMS_read8(&sms, 0xe123) == 0x5a);
    SMS_write8(&sms, 0xfffc, 8);
    SMS_write8(&sms, 0x8000, 0x42);
    assert(SMS_read8(&sms, 0x8000) == 0x42 && SMS_used_sram(&sms));
    SMS_set_buttons(&sms, SMS_Button_JOY1_A | SMS_Button_JOY2_B | SMS_Button_RESET, true);
    assert(!(SMS_read_io(&sms, 0xdc) & 0x10));
    assert(!(SMS_read_io(&sms, 0xdd) & 0x18));
    SMS_set_buttons(&sms, 0x3fff, false);
    assert((SMS_read_io(&sms, 0xdc) & 0x3f) == 0x3f);
    SMS_set_buttons(&sms, SMS_Button_PAUSE, true);
    assert(sms.vdp.nmi_pending);
    SMS_run(&sms, SMS_RunEndFrame);
    assert(!sms.vdp.nmi_pending);
    SMS_set_use_exact_timing(&sms, true);
    assert(SMS_target_fps(&sms) < 60);
    SMS_set_use_exact_timing(&sms, false);
    assert(SMS_target_fps(&sms) == 60);
    auto *cache = sms.vdp.cached_palette;
    auto *ram = sms.cart_ram.ram;
    std::vector<uint8_t> save(SMS_SRAM_SIZE_MAX, 0x5a);
    save.back() = 0x9c;
    assert(!SMS_loadsave(&sms, save.data(), sizeof(sms.cart_ram.ram)));
    assert(SMS_loadsave(&sms, save.data(), save.size()));
    assert(sms.cart_ram.ram[0][0] == 0x5a && sms.cart_ram.ram[1][16383] == 0x9c);
    const SMS_StateConfig config = {false, false};
    std::vector<uint8_t> state(SMS_get_state_size(&sms, &config));
    assert(SMS_savestate(&sms, state.data(), state.size(), &config));
    sms.cart_ram.ram[1][16383] = 0;
    assert(SMS_loadstate(&sms, state.data(), state.size(), &config));
    assert(sms.cart_ram.ram[1][16383] == 0x9c);
    sms.vdp.cached_palette[4095].normal = 0xffffffff;
    SMS_reset(&sms);
    mapper_init(&sms);
    assert(sms.vdp.cached_palette == cache && sms.cart_ram.ram == ram);
    assert(!cache[4095].normal && !ram[0][0] && !ram[1][16383]);
    SMS_quit(&sms);
    assert(!sms.cart_ram.ram && !sms.vdp.cached_palette);
    SMS_quit(&sms);
    for (size_t size : {size_t(8192), size_t(16384), size_t(32768), size_t(49152)})
    {
        auto small = cartridge(size);
        assert(SMS_init(&sms));
        assert(SMS_loadromEx(&sms, small.data(), size, SMS_System_SMS, -1, -1));
        for (unsigned address = 0; address < 0xc000; ++address)
            assert(SMS_read8(&sms, address) == small[address % size]);
        SMS_write8(&sms, 0xfffe, 255);
        for (unsigned address = 0x4000; address < 0x8000; address += 0x400)
            assert(sms.rmap[address >> 10] >= small.data() &&
                   sms.rmap[address >> 10] < small.data() + size);
        SMS_quit(&sms);
    }
}

static void benchmark()
{
    SmsCore machine;
    SmsSettings settings;
    char error[160];
    auto rom = cartridge();
    printf("Hot core size: %u bytes; cartridge SRAM and tile cache allocated separately\n", unsigned(sizeof(SMS_Core)));
    std::vector<uint8_t> pixels(SmsCore::Width * SmsCore::Height);
    assert(machine.load(rom.data(), rom.size(), settings, pixels.data(), error, sizeof(error)));
    for (bool draw : {false, true})
    {
        const auto start = std::chrono::steady_clock::now();
        for (unsigned frame = 0; frame < 3000; ++frame) machine.runFrame(0, draw);
        const double micros = std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - start).count();
        printf("Host synthetic core (%s): %.2f us/frame\n", draw ? "rendered" : "skipped", micros / 3000);
    }
    uint8_t line[320];
    const auto start = std::chrono::steady_clock::now();
    unsigned checksum = 0;
    for (unsigned frame = 0; frame < 3000; ++frame)
        for (unsigned y = 0; y < 240; ++y)
        {
            SmsVideoRow(line, pixels.data() + y * SmsCore::Width);
            checksum += line[frame % 320];
        }
    const double micros = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    printf("Host video scaling (240 rows): %.2f us/frame, checksum %u\n", micros / 3000, checksum);
}

static void rendererScenes()
{
    const uint64_t expected[] = {0x04931991e0b67fb4ULL, 0xd4d5913e0a130ef2ULL, 0xd6e318b27b0c50e2ULL};
    unsigned mode = 0;
    for (unsigned height : {192u, 224u, 240u})
    {
        uint64_t hash = 0xcbf29ce484222325ULL;
        auto hashByte = [&hash](uint8_t value) { hash = (hash ^ value) * 0x100000001b3ULL; };
        for (unsigned scroll : {0u, 1u, 7u, 8u, 255u})
            for (unsigned flags : {0u, 0x20u, 0x40u, 0x80u, 0xe0u})
            {
                SMS_Core sms = {};
                assert(SMS_init(&sms));
                std::vector<uint8_t> rom(32768), pixels(256 * 240 + 2, 0xa5);
                rom[0] = 0xc3;
                assert(SMS_loadromEx(&sms, rom.data(), rom.size(), SMS_System_SMS, -1, -1));
                SMS_set_pixels(&sms, pixels.data() + 1, 256, 8);
                SMS_set_colour_callback(&sms, [](void *, uint8_t r, uint8_t g, uint8_t b) -> uint32_t
                {
                    return SmsColour(r, g, b);
                });
                uint32_t seed = height ^ scroll ^ flags;
                for (uint8_t &value : sms.vdp.vram)
                {
                    seed = seed * 1664525u + 1013904223u;
                    value = seed >> 24;
                }
                for (unsigned i = 0; i < 32; ++i) sms.vdp.cram[i] = (i * 7 + scroll) & 0x3f;
                auto reg = [&sms](uint8_t index, uint8_t value)
                {
                    SMS_write_io(&sms, 0xbf, value);
                    SMS_write_io(&sms, 0xbf, 0x80 | index);
                };
                reg(0, (height == 192 ? 0x04 : 0x06) | flags);
                reg(1, height == 192 ? 0x40 : height == 224 ? 0x50 : 0x48);
                reg(2, 0x0e);
                reg(5, 0x7e);
                reg(6, 0x04);
                reg(8, scroll);
                reg(9, (scroll * 7 + flags) & 255);
                SMS_run(&sms, SMS_RunEndFrame);
                SMS_run(&sms, SMS_RunEndFrame);
                assert(pixels.front() == 0xa5 && pixels.back() == 0xa5);
                for (uint8_t value : pixels) hashByte(value);
                hashByte(sms.vdp.sprite_collision);
                hashByte(sms.vdp.sprite_overflow);
                hashByte(sms.vdp.fifth_sprite_num);
                SMS_quit(&sms);
            }
        assert(hash == expected[mode++]);
    }
    puts("75 reference VDP scenes matched: scroll/wrap, clipping, flips, priority, sprites and 192/224/240 rows.");
}

int main(int argc, char **argv)
{
    settingsAndInput();
    loaderAndFrames();
    hardwareCore();
    rendererScenes();
    puts("SMS core, synthetic cartridge video/audio, ROM loading, mappers, input and settings passed.");
    if (argc == 2 && !strcmp(argv[1], "--benchmark")) benchmark();
}
