#include "A1stCore.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
extern "C" {
#include "A1stInternal.h"
#include "Disk.h"
#include "Board.h"
extern UInt32* boardSysTime;
}

static void require(bool value, const char* message)
{
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

static void writeFile(const char* path, const std::vector<unsigned char>& bytes)
{
    FILE* file = std::fopen(path, "wb");
    require(file && std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size(), "Write media fixture.");
    require(std::fclose(file) == 0, "Close media fixture.");
}

static std::vector<unsigned char> cartridge(unsigned size)
{
    std::vector<unsigned char> rom(size);
    for (unsigned bank = 0; bank < size / 8192; ++bank)
        std::memset(rom.data() + bank * 8192, bank, 8192);
    rom[0] = 'A';
    rom[1] = 'B';
    return rom;
}

void a1stMediaTests()
{
    char error[512];
    int detected;
    const uint8_t originalSlots = a1stReadPort(0xa8);
    auto rom = cartridge(131072);
    writeFile("media-cart.rom", rom);
    require(a1stAttachCartridge(0, "media-cart.rom", A1ST_KONAMI, error, sizeof(error)), error);
    require(a1stAttachCartridge(1, "media-cart.rom", A1ST_ASCII8, error, sizeof(error)), error);
    a1stWritePort(0xa8, 0x55);
    a1stWriteMemory(0x6000, 5);
    require(a1stReadMemory(0x6001) == 5 && a1stReadMemory(0x4000) == 'A', "Slot 1 Konami banks and fixed first bank.");
    a1stWritePort(0xa8, 0xaa);
    a1stWriteMemory(0x6800, 7);
    require(a1stReadMemory(0x6001) == 7, "Slot 2 ASCII8 bank selection.");
    a1stWritePort(0xa8, 0x55);
    require(a1stReadMemory(0x6001) == 5, "Cartridge slots keep independent bank registers.");
    require(!a1stAttachCartridge(0, "absent.rom", A1ST_MAPPER_AUTO, error, sizeof(error)), "Missing cartridge rejected.");
    require(a1stReadMemory(0x6001) == 5, "Failed attachment preserves the previous ROM.");
    require(!a1stAttachCartridge(2, "media-cart.rom", A1ST_MAPPER_AUTO, error, sizeof(error)), "Reject third slot.");
    require(!a1stAttachCartridge(0, "media-cart.rom", 255, error, sizeof(error)), "Reject invalid mapper.");
    for (unsigned mapper : {unsigned(A1ST_GENERIC8), unsigned(A1ST_GENERIC16),
                            unsigned(A1ST_ASCII16), unsigned(A1ST_KONAMI_SCC)}) {
        require(a1stAttachCartridge(0, "media-cart.rom", mapper, error, sizeof(error)), error);
        if (mapper == A1ST_GENERIC8) {
            a1stWriteMemory(0x8000, 6);
            require(a1stReadMemory(0x8001) == 6, "Generic 8 KiB banks.");
        }
        else if (mapper == A1ST_GENERIC16) {
            a1stWriteMemory(0x8000, 3);
            require(a1stReadMemory(0x8001) == 6 && a1stReadMemory(0xa001) == 7, "Generic 16 KiB bank pairs.");
        }
        else if (mapper == A1ST_ASCII16) {
            a1stWriteMemory(0x7000, 3);
            require(a1stReadMemory(0x8001) == 6 && a1stReadMemory(0xa001) == 7, "ASCII16 bank pairs.");
        }
        else {
            a1stWriteMemory(0x9000, 0x3f);
            a1stWriteMemory(0x9800, 0x36);
            require(a1stReadMemory(0x9800) == 0x36, "SCC wave RAM is reachable when enabled.");
            for (unsigned x = 0; x < 32; ++x) a1stWriteMemory(0x9800 + x, x < 16 ? 64 : 192);
            a1stWriteMemory(0x9880, 99);
            a1stWriteMemory(0x9881, 0);
            a1stWriteMemory(0x988a, 15);
            a1stWriteMemory(0x988f, 1);
            require(mixerGetSampleRate(boardGetMixer()) == 22050, "SCC uses the board's sample rate.");
            *boardSysTime += boardFrequency() / 10;
            mixerSync(boardGetMixer());
            UInt32 samples;
            const Int16* audio = mixerGetBuffer(boardGetMixer(), &samples);
            unsigned crossings = 0;
            for (unsigned i = 1; i < samples; ++i)
                if (audio[i - 1] <= 0 && audio[i] > 0) ++crossings;
            require(samples >= 2200 && samples <= 2210 && crossings >= 95 && crossings <= 125,
                    "SCC generates an approximately 1119 Hz tone at 22050 Hz, not half-speed audio.");
            a1stWriteMemory(0x9000, 4);
            require(a1stReadMemory(0x9800) == 4, "Disabling SCC restores cartridge ROM.");
        }
        a1stWriteMemory(0x7000, 255);
        require(a1stReadMemory(0x7001) < 16, "Out-of-range bank values stay inside the ROM.");
    }
    for (unsigned size : {8192U, 16384U, 24576U, 32768U, 49152U, 65536U}) {
        auto plain = cartridge(size);
        if (size > 32768) {
            plain[0] = plain[1] = 0;
            plain[0x4000] = 'A'; plain[0x4001] = 'B';
        }
        writeFile("media-plain.rom", plain);
        require(a1stInspectCartridge("media-plain.rom", &detected, error, sizeof(error)) && detected == -1,
                "Auto detects small plain ROMs.");
        require(a1stAttachCartridge(0, "media-plain.rom", A1ST_MAPPER_AUTO, error, sizeof(error)), error);
        require(a1stReadMemory(0x4000) == 'A', "Plain ROM base and mirroring.");
        a1stWriteMemory(0x6000, 15);
        require(a1stReadMemory(0x4000) == 'A', "Plain ROM writes do not bank-switch.");
    }
    for (unsigned mapper : {unsigned(A1ST_KONAMI), unsigned(A1ST_KONAMI_SCC),
                            unsigned(A1ST_ASCII8), unsigned(A1ST_ASCII16)}) {
        auto heuristic = cartridge(131072);
        const unsigned address = mapper == A1ST_KONAMI ? 0x8000 :
                                 mapper == A1ST_KONAMI_SCC ? 0x9000 :
                                 mapper == A1ST_ASCII8 ? 0x6800 : 0x77ff;
        heuristic[16] = 0x32; heuristic[17] = address & 255; heuristic[18] = address >> 8;
        writeFile("media-auto.rom", heuristic);
        require(a1stInspectCartridge("media-auto.rom", &detected, error, sizeof(error)) &&
                detected == static_cast<int>(mapper), "Auto detects common mapper register writes.");
    }
    for (unsigned mapper : {unsigned(A1ST_GAMEMASTER2), unsigned(A1ST_FMPAC)}) {
        auto persistent = cartridge(mapper == A1ST_FMPAC ? 65536 : 131072);
        writeFile("media-sram.rom", persistent);
        std::remove("media-sram.rom.slot1.sram");
        require(a1stAttachCartridge(0, "media-sram.rom", mapper, error, sizeof(error)), error);
        if (mapper == A1ST_GAMEMASTER2) {
            a1stWriteMemory(0xa000, 0x30);
            a1stWriteMemory(0xb123, 0x69);
            require(a1stReadMemory(0xa123) == 0x69 && a1stReadMemory(0xb123) == 0x69, "GameMaster2 SRAM mirroring.");
        }
        else {
            a1stWriteMemory(0x7ff7, 2);
            require(a1stReadMemory(0x4001) == 4, "FM-PAC bank selection.");
            a1stWriteMemory(0x5ffe, 0x4d); a1stWriteMemory(0x5fff, 0x69);
            a1stWriteMemory(0x4123, 0x69);
            require(a1stReadMemory(0x4123) == 0x69, "FM-PAC SRAM unlock and writes.");
            a1stWriteMemory(0x7ff4, 0x30); a1stWriteMemory(0x7ff5, 0x10);
        }
        require(a1stCartridgeFlush(error, sizeof(error)), error);
        require(a1stAttachCartridge(0, "", mapper, error, sizeof(error)), error);
        require(a1stReadMemory(0x4000) == 0xff, "Cartridge eject clears primary slot.");
        require(a1stAttachCartridge(0, "media-sram.rom", mapper, error, sizeof(error)), error);
        if (mapper == A1ST_GAMEMASTER2) a1stWriteMemory(0xa000, 0x30);
        else { a1stWriteMemory(0x5ffe, 0x4d); a1stWriteMemory(0x5fff, 0x69); }
        require(a1stReadMemory(mapper == A1ST_FMPAC ? 0x4123 : 0xb123) == 0x69, "Cartridge SRAM persists across reattachment.");
        std::remove("media-sram.rom.slot1.sram");
    }
    require(a1stAttachCartridge(0, "", A1ST_MAPPER_AUTO, error, sizeof(error)), error);
    require(a1stAttachCartridge(1, "", A1ST_MAPPER_AUTO, error, sizeof(error)), error);
    a1stWritePort(0xa8, originalSlots);

    std::vector<unsigned char> disk(368640, 0);
    disk[0] = 0x42;
    writeFile("media-b.dsk", disk);
    require(a1stAttachDisk(1, "media-b.dsk", error, sizeof(error)), error);
    unsigned char sector[512];
    require(diskRead(1, sector, 0) == DSKE_OK && sector[0] == 0x42, "Read disk drive B.");
    sector[0] = 0x69;
    require(diskWrite(1, sector, 0), "Write drive B synchronously.");
    require(!a1stAttachDisk(0, "media-b.dsk", error, sizeof(error)), "Prevent sharing one writable DSK across drives.");
    require(!a1stAttachDisk(1, "absent.dsk", error, sizeof(error)), "Reject missing replacement disk.");
    require(diskRead(1, sector, 0) == DSKE_OK && sector[0] == 0x69, "Failed replacement retains drive B.");
    require(a1stAttachDisk(1, "", error, sizeof(error)) && !diskPresent(1), "Eject drive B.");

    for (const char* path : {"media-cart.rom", "media-plain.rom", "media-auto.rom",
                              "media-sram.rom", "media-b.dsk"})
        require(std::remove(path) == 0, "Clean up media fixtures.");
    std::puts("A1ST dual cartridges, all mapper choices, SCC, SRAM and drive B tests passed.");
}
