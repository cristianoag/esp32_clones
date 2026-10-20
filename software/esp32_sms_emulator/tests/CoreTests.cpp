#include "SmsCore.h"
#include "SmsInput.h"
#include <sms_internal.h>
#include <algorithm>
#include <assert.h>
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
    assert(SMS_init(&sms));
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

int main()
{
    settingsAndInput();
    loaderAndFrames();
    hardwareCore();
    puts("SMS core, synthetic cartridge video/audio, ROM loading, mappers, input and settings passed.");
}
