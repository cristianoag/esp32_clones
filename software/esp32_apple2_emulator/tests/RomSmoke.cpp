#include "AppleCore.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <algorithm>

static AppleCore core;

static void require(bool condition, const char *message)
{
    if (!condition) { fprintf(stderr, "%s\n", message); exit(1); }
}

static std::vector<uint8_t> load(const char *path, size_t size)
{
    FILE *file = fopen(path, "rb");
    require(file != nullptr, "Cannot open supplied ROM file.");
    std::vector<uint8_t> bytes(size);
    require(fread(bytes.data(), 1, size, file) == size && fgetc(file) == EOF, "ROM size mismatch.");
    require(!ferror(file), "ROM read failed.");
    fclose(file);
    return bytes;
}

static void run(unsigned cycles)
{
    int16_t audio[AppleCore::MaxSamples];
    unsigned samples;
    while (cycles >= AppleCore::FrameCycles)
    {
        core.runFrame(nullptr, audio, samples, false);
        cycles -= AppleCore::FrameCycles;
    }
    for (unsigned i = 0; i < cycles; ++i) core.tick();
}

static void type(const char *text)
{
    for (; *text; ++text)
    {
        core.key(*text);
        run(100000);
        require(!core.keyPending(), "ROM did not consume keyboard input.");
    }
    run(500000);
}

static bool screenContains(const char *text)
{
    char screen[24 * 81 + 1];
    unsigned offset = 0;
    const unsigned columns = core.col80 ? 80 : 40;
    for (unsigned row = 0; row < 24; ++row)
    {
        for (unsigned col = 0; col < columns; ++col)
        {
            const unsigned bank = core.col80 && !(col & 1) ? 1 : 0;
            uint8_t value = core.ram[bank][0x400 + (row & 7) * 128 + row / 8 * 40 + (core.col80 ? col / 2 : col)] & 127;
            if (value < 32) value += 64;
            screen[offset++] = value;
        }
        screen[offset++] = '\n';
    }
    screen[offset] = 0;
    if (strstr(screen, text)) return true;
    fprintf(stderr, "Screen (PC=%04X):\n%s", m6502_pc(&core.cpu), screen);
    return false;
}

static std::vector<uint8_t> bootDisk()
{
    static const uint8_t gcr[] = {
        0x96,0x97,0x9a,0x9b,0x9d,0x9e,0x9f,0xa6,0xa7,0xab,0xac,0xad,0xae,0xaf,0xb2,0xb3,
        0xb4,0xb5,0xb6,0xb7,0xb9,0xba,0xbb,0xbc,0xbd,0xbe,0xbf,0xcb,0xcd,0xce,0xcf,0xd3,
        0xd6,0xd7,0xd9,0xda,0xdb,0xdc,0xdd,0xde,0xdf,0xe5,0xe6,0xe7,0xe9,0xea,0xeb,0xec,
        0xed,0xee,0xef,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff
    };
    uint8_t sector[256] = {1, 0xa9,0x5a, 0x8d,0x00,0x02, 0x4c,0x06,0x08};
    uint8_t low[86] = {};
    for (int i = 255; i >= 0; --i)
        low[85 - i % 86] = (low[85 - i % 86] << 2) | ((sector[i] & 1) << 1) | ((sector[i] & 2) >> 1);
    std::vector<uint8_t> image(AppleDisk::ImageSize, 0xff);
    size_t position = 48;
    for (uint8_t b : {0xd5,0xaa,0x96}) image[position++] = b;
    for (uint8_t b : {0xfe,0x00,0x00,0xfe})
    {
        image[position++] = (b >> 1) | 0xaa;
        image[position++] = b | 0xaa;
    }
    for (uint8_t b : {0xde,0xaa,0xeb}) image[position++] = b;
    position += 16;
    for (uint8_t b : {0xd5,0xaa,0xad}) image[position++] = b;
    uint8_t previous = 0;
    for (int i = 85; i >= 0; --i)
    {
        image[position++] = gcr[previous ^ low[i]];
        previous = low[i];
    }
    for (uint8_t b : sector)
    {
        image[position++] = gcr[previous ^ (b >> 2)];
        previous = b >> 2;
    }
    image[position++] = gcr[previous];
    for (uint8_t b : {0xde,0xaa,0xeb}) image[position++] = b;
    return image;
}

static void karatekaBoot(const std::vector<uint8_t> &rom, const std::vector<uint8_t> &slot,
                         const std::vector<uint8_t> &disk, AppleModel model, bool fromBasic)
{
    char error[160];
    require(core.boot(rom.data(), rom.size(), slot.data(), slot.size(), model, error, sizeof(error)), error);
    if (fromBasic)
    {
        run(2000000);
        core.reset();
        run(1000000);
    }
    require(core.disks[0].attach(disk.data(), disk.size(), error, sizeof(error)), error);
    if (fromBasic) type("PR#6\r");
    unsigned firstGraphics = 0;
    for (unsigned second = 1; second <= 60; ++second)
    {
        run(AppleCore::ClockRate);
        if (!firstGraphics && core.hires && !core.text) firstGraphics = second;
    }
    std::vector<uint8_t> intro(AppleCore::Width * AppleCore::Height), game(intro.size());
    core.render(intro.data(), false);
    require(core.hires && !core.text && std::count_if(intro.begin(), intro.end(),
            [](uint8_t pixel) { return pixel != 0; }) > 1024,
            "Karateka did not reach its visible intro within 60 emulated seconds.");
    core.key(' ');
    run(60 * AppleCore::ClockRate);
    core.render(game.data(), false);
    require(!core.keyPending(), "Karateka did not consume Space from its intro.");
    require(core.hires && !core.text && game != intro &&
            std::count_if(game.begin(), game.end(), [](uint8_t pixel) { return pixel != 0; }) > 1024 &&
            m6502_pc(&core.cpu) >= 0x800 && m6502_pc(&core.cpu) < 0xb000,
            "Karateka did not leave the intro/loader and enter its game display after Space.");
    printf("Karateka %s %s: graphics in %u emulated seconds, intro and game/input passed.\n",
           model == AppleModel::IIe ? "IIe" : "II+", fromBasic ? "PR#6" : "cold boot", firstGraphics);
    core.disks[0].eject();
}

int main(int argc, char **argv)
{
    require(argc == 4 || argc == 5, "Usage: rom-smoke apple2plus.rom apple2e.rom disk2.rom [karateka.nib]");
    auto slot = load(argv[3], 256);
    std::vector<uint8_t> karateka;
    if (argc == 5) karateka = load(argv[4], AppleDisk::ImageSize);
    for (unsigned model = 0; model < 2; ++model)
    {
        auto rom = load(argv[model + 1], model ? 16384 : 12288);
        char error[160];
        require(core.boot(rom.data(), rom.size(), slot.data(), slot.size(), AppleModel(model), error, sizeof(error)), error);
        run(2000000);
        core.reset(); // Like the physical Reset key, escape an empty Disk II boot.
        run(1000000);
        type("PRINT 6*7\r");
        require(screenContains("42"), "Applesoft did not print 42.");
        if (model)
        {
            type("PR#3\r");
            require(core.col80, "Original IIe ROM did not enable 80-column firmware.");
            type("PRINT 7*7\r");
            require(screenContains("49"), "Applesoft did not print 49 across the 80-column display banks.");
            bool auxUsed = false;
            for (unsigned i = 0x400; i < 0x800; ++i) auxUsed |= core.ram[1][i] != 0;
            require(auxUsed, "80-column firmware did not use auxiliary display memory.");
        }
        printf("%s ROM: reset, keyboard and Applesoft%s passed.\n",
               model ? "Original IIe" : "II+", model ? ", 80-column firmware" : "");
        auto disk = bootDisk();
        require(core.boot(rom.data(), rom.size(), slot.data(), slot.size(), AppleModel(model), error, sizeof(error)), error);
        require(core.disks[0].attach(disk.data(), disk.size(), error, sizeof(error)), error);
        run(3000000);
        if (core.ram[0][0x200] != 0x5a)
            fprintf(stderr, "Disk boot PC=%04X, sector prefix=%02X %02X %02X, track=%u\n",
                    m6502_pc(&core.cpu), core.ram[0][0x800], core.ram[0][0x801], core.ram[0][0x802], core.disks[0].track());
        require(core.ram[0][0x200] == 0x5a, "Disk II ROM did not boot the synthetic NIB program.");
        core.disks[0].eject();
        puts("Disk II ROM: decoded and executed the original synthetic NIB boot sector.");
        if (!karateka.empty())
        {
            karatekaBoot(rom, slot, karateka, AppleModel(model), false);
            karatekaBoot(rom, slot, karateka, AppleModel(model), true);
        }
    }
}
