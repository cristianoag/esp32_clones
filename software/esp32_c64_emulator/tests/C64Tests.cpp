#include "C64Core.h"
#include "C64Input.h"
#include "C64Media.h"
#include "C64Settings.h"
#include "C64Video.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

static char error[200];
static void fixture(C64Core &machine, const uint8_t *program = nullptr, size_t programSize = 0)
{
    uint8_t basic[8192], kernal[8192], chars[4096];
    memset(basic, 0xa5, sizeof(basic));
    memset(kernal, 0xea, sizeof(kernal));
    memset(chars, 0x3c, sizeof(chars));
    const uint8_t loop[] = {0xee,0x00,0x04,0x4c,0x00,0xe0};
    memcpy(kernal, program ? program : loop, program ? programSize : sizeof(loop));
    kernal[0x100] = 0x69; kernal[0x101] = 1; kernal[0x102] = 0x40;
    kernal[0x1ffc] = 0;
    kernal[0x1ffd] = 0xe0;
    kernal[0x1ffe] = 0; kernal[0x1fff] = 0xe1;
    assert(machine.boot(basic, sizeof(basic), kernal, sizeof(kernal), chars, sizeof(chars), error, sizeof(error)));
}

static void inputTests()
{
    uint8_t report[8] = {}, matrix[8];
    report[2] = 4; report[3] = 5; // A and B, independent rows.
    report[0] = 0x05; // Control and Alt.
    C64MapReport(report, matrix);
    assert(!(matrix[1] & 4) && !(matrix[3] & 16));
    assert(!(matrix[7] & 4) && !(matrix[7] & 32));
    assert(!(C64ReadMatrix(matrix, 0xfd, false) & 4));
    assert(!(C64ReadMatrix(matrix, 0xfb, true) & 2));
    memset(report, 0, sizeof(report)); report[2] = 69; // F12 never reaches the C64.
    C64MapReport(report, matrix);
    for (unsigned i = 0; i < 8; ++i) assert(matrix[i] == 255);
    for (unsigned key = 58; key <= 65; ++key)
    {
        report[2] = key;
        C64MapReport(report, matrix);
        const unsigned bits[] = {4,4,5,5,6,6,3,3};
        assert(!(matrix[0] & (1u << bits[key - 58])));
        assert(bool(!(matrix[1] & 128)) == bool((key - 58) & 1));
    }
    report[0] = 2; report[2] = 31; // US Shift+2 -> C64 @, not Shift+2.
    C64MapReport(report, matrix);
    assert(!(matrix[5] & 64) && matrix[1] == 255);
    report[0] = 0; report[2] = 52; // US apostrophe -> C64 Shift+7.
    C64MapReport(report, matrix);
    assert(!(matrix[3] & 1) && !(matrix[1] & 128));
    report[2] = 49; C64MapReport(report, matrix);
    assert(!(matrix[6] & 1));
    report[0] = 2; report[2] = 47; C64MapReport(report, matrix);
    for (unsigned i = 0; i < 8; ++i) assert(matrix[i] == 255); // No C64 equivalent of US {.
    report[2] = 1; C64MapReport(report, matrix);
    for (unsigned i = 0; i < 8; ++i) assert(matrix[i] == 255);
    assert(C64JoystickValue(0x31) == 0xee); // B is not a second C64 fire line.
}

static void machineTests()
{
    std::unique_ptr<C64Core> machine(new C64Core);
    fixture(*machine);
    assert(machine->programCounter() == 0xe000);
    machine->runFrame();
    assert(machine->ram[0x400] && machine->frames == 1 && !machine->cpuhalted);
    assert(machine->getMem(0xa000) == 0xa5);
    machine->setMem(0xa000, 0x77);
    machine->setMem(1, 0x34);
    assert(machine->getMem(0xa000) == 0x77);
    machine->setMem(1, 0x33);
    assert(machine->getMem(0xd000) == 0x3c);
    machine->setMem(0xd000, 0x55);
    machine->setMem(1, 0x30);
    assert(machine->getMem(0xd000) == 0x55);
    machine->setMem(0, 0); // Input pins pull high even when the output latch is zero.
    machine->setMem(1, 0);
    assert((machine->getMem(1) & 7) == 7 && machine->getMem(0xa000) == 0xa5);
    machine->setMem(0, 0x2f); machine->setMem(1, 0x37);
    machine->setMem(0xdd02, 3);
    machine->setMem(0xdd00, 2);
    assert(machine->vic.vicmem == 0x4000);
    machine->setMem(0xd018, 0x18);
    assert(machine->vic.screenmemstart == 0x4400 && machine->vic.bitmapstart == 0x6000);
    machine->setMem(0xd800, 0xac);
    assert(machine->getMem(0xd800) == 0xfc);
    machine->vic.vicreg[0x19] = 0x87;
    machine->setMem(0xd01a, 7);
    machine->setMem(0xd019, 1);
    assert(machine->vic.vicreg[0x19] == 0x86);
    machine->setMem(0xd019, 6);
    assert(machine->vic.vicreg[0x19] == 0);

    uint8_t report[8] = {};
    machine->setMem(0xdc02, 0); machine->setMem(0xdc03, 0);
    machine->input(report, 0x0811, false);
    assert((machine->getMem(0xdc01) & 31) == 14); // Port 1: up + fire.
    assert((machine->getMem(0xdc00) & 31) == 23); // Port 2: right.
    machine->input(report, 0x0811, true);
    assert((machine->getMem(0xdc00) & 31) == 14);
    report[2] = 4;
    machine->input(report, 0x1000, false);
    machine->setMem(0xdc02, 255); machine->setMem(0xdc00, 0xfd);
    assert(!(machine->getMem(0xdc01) & 4)); // Keyboard remains active with port 2 fire held.
    machine->input(report, 0x0010, false);
    assert((machine->getMem(0xdc01) & 0x14) == 0);
    const uint8_t prg[] = {1,8,0,0,0};
    assert(machine->loadPrg(prg, sizeof(prg), error, sizeof(error)));
    assert(machine->ram[0x2d] == 4 && machine->ram[0x2e] == 8);
    const uint8_t invalid[] = {255,255,1,2};
    const uint8_t before = machine->ram[65535];
    assert(!machine->loadPrg(invalid, sizeof(invalid), error, sizeof(error)));
    assert(machine->ram[65535] == before);
    const uint8_t zeroPage[] = {0,0,0x2f};
    assert(machine->loadPrg(zeroPage, sizeof(zeroPage), error, sizeof(error)));
    machine->queueRun();
    assert(machine->ram[0xc6] == 4 && !memcmp(machine->ram + 0x277, "RUN\r", 4));
    machine->ram[0x2345] = 42; machine->reset();
    assert(machine->ram[0x2345] == 42);
    // Exercise a complete SID buffer at this board's 22050 Hz output rate.
    machine->setMem(0xd400, 0xff); machine->setMem(0xd401, 0x20);
    machine->setMem(0xd405, 0); machine->setMem(0xd406, 0xf0);
    machine->setMem(0xd404, 0x21); machine->setMem(0xd418, 15);
    machine->runFrame();
    bool audible = false;
    for (unsigned i = 0; i < SID::SampleCount; ++i) audible |= machine->sid.audio()[i] != 0;
    assert(SID::SampleCount == 441 && audible);
    machine->setMem(0xdc04, 10); machine->setMem(0xdc05, 0);
    machine->setMem(0xdc0d, 0x81); machine->setMem(0xdc0e, 0x11);
    machine->runFrame();
    assert(machine->getMem(0xdc0d) & 0x81);
    assert(!(machine->getMem(0xdc0d) & 0x81));
    const uint8_t indirect[] = {0xa0,1,0xb1,0xff,0x8d,1,4,0x4c,7,0xe0};
    fixture(*machine, indirect, sizeof(indirect));
    machine->ram[0] = 8;
    machine->ram[0xff] = 0;
    machine->ram[0x100] = 9;
    machine->ram[0x801] = 42;
    machine->ram[0x901] = 77;
    machine->runFrame();
    assert(machine->ram[0x401] == 42); // ($FF),Y wraps the high-byte fetch to $00.
    const uint8_t decimalIrq[] = {0xf8,0xa9,9,0x18,0,0xea,0x8d,1,4,0x4c,9,0xe0};
    fixture(*machine, decimalIrq, sizeof(decimalIrq));
    machine->runFrame();
    assert(machine->ram[0x401] == 0x10); // NMOS interrupt entry preserves decimal mode.
}

static void mediaTests()
{
    std::vector<uint8_t> image(C64D64::ImageSize);
    const unsigned directory = (17 * 21 + 1) * 256;
    image[directory + 2] = 0x82;
    image[directory + 3] = 1;
    memset(image.data() + directory + 5, 0xa0, 16);
    memcpy(image.data() + directory + 5, "HELLO", 5);
    image[1] = 6;
    const uint8_t prg[] = {1,8,0,0,0};
    memcpy(image.data() + 2, prg, sizeof(prg));
    C64D64 disk;
    assert(disk.open(image.data(), image.size(), error, sizeof(error)));
    assert(disk.count() == 1 && !strcmp(disk.entry(0).name, "HELLO"));
    uint8_t output[10] = {}; size_t size = 0;
    assert(disk.extract(0, output, sizeof(output), size, error, sizeof(error)));
    assert(size == sizeof(prg) && !memcmp(output, prg, size));
    assert(!disk.extract(0, output, 2, size, error, sizeof(error)));
    image[0] = 1; image[1] = 0;
    uint8_t large[65538];
    assert(!disk.extract(0, large, sizeof(large), size, error, sizeof(error)));
    image[directory] = 18; image[directory + 1] = 1;
    assert(!disk.open(image.data(), image.size(), error, sizeof(error)));
    assert(!disk.open(image.data(), 100, error, sizeof(error)));
    image[directory] = 0; image[directory + 1] = 0; image[0] = 0; image[1] = 6;
    image.resize(C64D64::ImageSize + 683, 1);
    assert(disk.open(image.data(), image.size(), error, sizeof(error)));
    image[C64D64::ImageSize] = 2;
    assert(!disk.extract(0, large, sizeof(large), size, error, sizeof(error)));
}

static bool screenContains(const C64Core &machine, const char *text)
{
    const size_t length = strlen(text);
    for (unsigned start = 0x400; start + length <= 0x7e8; ++start)
    {
        bool match = true;
        for (unsigned i = 0; i < length; ++i) match &= (machine.ram[start + i] & 63) == (text[i] & 63);
        if (match) return true;
    }
    return false;
}

static bool screenLine(const C64Core &machine, const char *text)
{
    const size_t length = strlen(text);
    for (unsigned row = 0; row < 25; ++row)
    {
        bool match = true;
        for (unsigned i = 0; i < length; ++i) match &= (machine.ram[0x400 + row * 40 + i] & 63) == (text[i] & 63);
        if (match && machine.ram[0x400 + row * 40 + length] == 32) return true;
    }
    return false;
}

static void type(C64Core &machine, const char *text)
{
    for (; *text; ++text)
    {
        uint8_t report[8] = {};
        if (*text >= 'A' && *text <= 'Z') report[2] = *text - 'A' + 4;
        else if (*text == '"') { report[2] = 52; report[0] = 2; }
        else if (*text == ' ') report[2] = 44;
        else if (*text == '\r') report[2] = 40;
        else assert(false);
        machine.input(report, 0, false);
        for (unsigned i = 0; i < 3; ++i) machine.runFrame();
        memset(report, 0, sizeof(report));
        machine.input(report, 0, false);
        for (unsigned i = 0; i < 3; ++i) machine.runFrame();
    }
}

static bool smoke(const char *directory)
{
    const char *names[] = {"basic.rom","kernal.rom","chargen.rom"};
    std::vector<uint8_t> roms[3];
    for (unsigned i = 0; i < 3; ++i)
    {
        std::ifstream file(std::string(directory) + "\\" + names[i], std::ios::binary);
        if (!file) { fprintf(stderr, "Supply legally obtained %s in %s\n", names[i], directory); return false; }
        roms[i] = std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});
    }
    std::unique_ptr<C64Core> machine(new C64Core);
    if (!machine->boot(roms[0].data(), roms[0].size(), roms[1].data(), roms[1].size(),
                       roms[2].data(), roms[2].size(), error, sizeof(error)))
    { fprintf(stderr, "%s\n", error); return false; }
    for (unsigned i = 0; i < 250 && !machine->cpuhalted; ++i) machine->runFrame();
    if (!screenContains(*machine, "READY.") || machine->cpuhalted)
    { fprintf(stderr, "C64 failed to reach READY; PC=%04X\n", machine->programCounter()); return false; }
    type(*machine, "PRINT\"KEY OK\"\r");
    if (!screenLine(*machine, "KEY OK") || machine->cpuhalted)
    { fprintf(stderr, "Keyboard-to-BASIC PRINT command failed\n"); return false; }
    // 10 PRINT"C64 OK"
    const uint8_t prg[] = {1,8,15,8,10,0,0x99,34,'C','6','4',' ','O','K',34,0,0,0};
    assert(machine->loadPrg(prg, sizeof(prg), error, sizeof(error)));
    machine->queueRun();
    for (unsigned i = 0; i < 100 && !machine->cpuhalted; ++i) machine->runFrame();
    if (!screenContains(*machine, "C64 OK") || machine->cpuhalted)
    { fprintf(stderr, "BASIC PRG execution failed\n"); return false; }
    puts("User-ROM smoke: BASIC READY, matrix keyboard PRINT and PRG RUN passed.");
    return true;
}

int main(int argc, char **argv)
{
    inputTests();
    machineTests();
    mediaTests();
    C64Settings settings, decoded;
    assert(C64DecodeSettings(&settings, sizeof(settings), decoded));
    settings.swapPorts = 2;
    assert(!C64DecodeSettings(&settings, sizeof(settings), decoded));
    assert(!C64ValidPath("/c64/../bad.rom"));
    uint8_t pixels[320 * 200] = {}, line[320];
    pixels[0] = 1; pixels[319] = 2; pixels[320 * 200 - 1] = 1;
    C64VideoLine(pixels, 6, 19, line);
    for (unsigned i = 0; i < 320; ++i) assert(line[i] == C64Rgb(0,0,170));
    C64VideoLine(pixels, 6, 20, line);
    assert(line[0] == 255 && line[318] == 0 && line[319] == C64Rgb(136,0,0));
    C64VideoLine(pixels, 6, 219, line);
    assert(line[319] == 255);
    C64VideoLine(pixels, 6, 220, line);
    assert(line[319] == C64Rgb(0,0,170));
    puts("C64 CPU/banking, VIC/CIA/SID, matrix, dual joysticks, media and settings passed.");
    return argc > 1 && !smoke(argv[1]) ? 1 : 0;
}
