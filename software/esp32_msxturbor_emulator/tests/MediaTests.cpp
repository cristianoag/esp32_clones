#include "A1stCore.h"
#include "MemoryHooks.h"
#include <algorithm>
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
    if (size >= 2) { rom[0] = 'A'; rom[1] = 'B'; }
    return rom;
}

static void ascii16XTests(const char* path, const std::vector<unsigned char>& rom)
{
    const size_t allocationLimit = a1stTestAllocationLimit();
    char error[512];
    int detected;
    require(a1stInspectCartridge(path, &detected, error, sizeof(error)) &&
            detected == A1ST_ASCII16X, "Auto detects the ASCII16X header.");
    require(a1stAttachCartridge(0, path, A1ST_MAPPER_AUTO, error, sizeof(error)), error);
    a1stWritePort(0xa8, 0x55);
    require(a1stReadMemory(0x4000) == rom[0] && a1stReadMemory(0x8000) == rom[16384],
            "ASCII16-X resets to banks 0 and 1.");
    for (unsigned bank = 0; bank < rom.size() / 16384; ++bank) {
        const unsigned other = rom.size() / 16384 - bank - 1;
        a1stWriteMemory(0x6000, bank);
        a1stWriteMemory(0x7000, other);
        for (unsigned offset = 0; offset < 16384; ++offset) {
            require(a1stReadMemory(0x4000 + offset) == rom[bank * 16384 + offset] &&
                    a1stReadMemory(0xc000 + offset) == rom[bank * 16384 + offset],
                    "Every byte of the first ASCII16-X bank and its mirror matches the file.");
            require(a1stReadMemory(0x8000 + offset) == rom[other * 16384 + offset] &&
                    a1stReadMemory(offset) == rom[other * 16384 + offset],
                    "Every byte of the independent second ASCII16-X bank and its mirror matches the file.");
        }
    }
    for (unsigned base : {0x2000U, 0x6000U, 0xa000U, 0xe000U}) {
        for (unsigned low : {0U, 0xffU}) {
            a1stWriteMemory(base + low, 3);
            a1stWriteMemory(base + 0x1000 + low, 5);
            require(a1stReadMemory(0x4002) == rom[3 * 16384 + 2] &&
                    a1stReadMemory(0x8002) == rom[5 * 16384 + 2],
                    "All mirrored ASCII16-X register ranges select independent banks.");
            a1stWriteMemory(base + 0x100 + low, 0);
            require(a1stReadMemory(0x4002) == 0xff && a1stReadMemory(0xc002) == 0xff &&
                    a1stReadMemory(0x8002) == rom[5 * 16384 + 2],
                    "Address bit 8 selects unpopulated flash, not a wrapped ROM bank.");
            a1stWriteMemory(base + 0x1100 + low, 0xff);
            require(a1stReadMemory(0x8002) == 0xff && a1stReadMemory(2) == 0xff,
                    "The second register also uses address bits for high flash banks.");
            a1stWriteMemory(base + 0xe00 + low, 3);
            require(a1stReadMemory(0x4002) == rom[3 * 16384 + 2],
                    "Address bits beyond the 8 MiB flash capacity mirror connected bank bits.");
        }
    }
    a1stWriteMemory(0x6000, 0);
    const size_t residentBytes = a1stTestAllocatedBytes();
    a1stTestSetAllocationLimit(residentBytes);
    require(a1stInspectCartridge(path, &detected, error, sizeof(error)) &&
            detected == A1ST_ASCII16X && a1stTestAllocatedBytes() == residentBytes,
            "Inspecting a resident 4 MiB cartridge needs no extra heap allocation.");
    require(a1stReadMemory(0x4000) == 'A', "Inspection preserves the resident ROM and mapper state.");
    a1stTestSetAllocationLimit(std::min(residentBytes + 32768, allocationLimit));
    char requested[32];
    std::snprintf(requested, sizeof(requested), "%zu", rom.size());
    require(!a1stAttachCartridge(0, path, A1ST_ASCII16X, error, sizeof(error)) &&
            std::strstr(error, "PSRAM exhausted") && std::strstr(error, requested),
            "Insufficient replacement memory reports requested ROM bytes.");
    require(a1stTestAllocatedBytes() == residentBytes && a1stReadMemory(0x4000) == 'A',
            "Partial allocation failure frees every new bank and retains the old cartridge.");
    a1stTestSetAllocationLimit(allocationLimit);
    for (unsigned address : {0U, 0x1000U, 0x4000U, 0x5000U, 0x8000U, 0x9000U, 0xc000U, 0xd000U})
        a1stWriteMemory(address, 255);
    require(a1stReadMemory(0x4000) == 'A', "Non-register writes leave ASCII16-X ROM unchanged.");
    require(a1stAttachCartridge(0, "", A1ST_MAPPER_AUTO, error, sizeof(error)), error);
    require(a1stAttachCartridge(0, path, A1ST_ASCII16X, error, sizeof(error)), error);
    require(a1stReadMemory(0x4000) == 'A' && a1stReadMemory(0x8000) == rom[16384],
            "Manual ASCII16-X selection and reattachment reset both bank registers.");
    a1stTestSetAllocationLimit(allocationLimit);
}

void a1stCartridgeFileTests(const char* path)
{
    FILE* file = std::fopen(path, "rb");
    require(file != nullptr, "Open user-supplied cartridge for read-only verification.");
    require(std::fseek(file, 0, SEEK_END) == 0, "Measure user-supplied cartridge.");
    const long size = std::ftell(file);
    require(size >= 131072 && size <= 4194304 && size % 16384 == 0,
            "ASCII16-X verification requires a 128 KiB to 4 MiB image in 16 KiB banks.");
    require(std::fseek(file, 0, SEEK_SET) == 0, "Rewind user-supplied cartridge.");
    std::vector<unsigned char> rom(static_cast<size_t>(size));
    require(std::fread(rom.data(), 1, rom.size(), file) == rom.size(), "Read verification bytes.");
    require(std::fclose(file) == 0, "Close user-supplied cartridge.");
    ascii16XTests(path, rom);
    char error[512];
    require(a1stAttachCartridge(0, "", A1ST_MAPPER_AUTO, error, sizeof(error)), error);
    std::puts("User-supplied ASCII16-X cartridge: Auto/manual detection and every bank/mirror byte passed.");
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
            require(a1stSetSoundSources(A1ST_SOUND_ALL & ~A1ST_SOUND_SCC, error, sizeof(error)), error);
            *boardSysTime += boardFrequency() / 100;
            mixerSync(boardGetMixer());
            audio = mixerGetBuffer(boardGetMixer(), &samples);
            require(std::all_of(audio, audio + samples, [](Int16 value) { return value == 0; }),
                    "SCC output can be muted while its cartridge mapper remains active.");
            require(a1stSetSoundSources(A1ST_SOUND_ALL, error, sizeof(error)), error);
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
        heuristic[8191] = 0x32; heuristic[8192] = address & 255; heuristic[8193] = address >> 8;
        writeFile("media-auto.rom", heuristic);
        require(a1stInspectCartridge("media-auto.rom", &detected, error, sizeof(error)) &&
                detected == static_cast<int>(mapper), "Auto detects common mapper register writes.");
    }
    {
        auto large = cartridge(4194304);
        for (unsigned bank = 0; bank < large.size() / 8192; ++bank)
            large[bank * 8192 + 2] = bank >> 8;
        std::memcpy(large.data() + 0x10, "ASCII16X", 8);
        writeFile("media-large.rom", large);
        ascii16XTests("media-large.rom", large);
        require(a1stAttachCartridge(0, "media-large.rom", A1ST_ASCII16, error, sizeof(error)), error);
        a1stWriteMemory(0x6000, 255);
        require(a1stReadMemory(0x4001) == 254 && a1stReadMemory(0x6001) == 255 &&
                a1stReadMemory(0x4002) == 1, "Standard ASCII16 also addresses its full 4 MiB capacity.");
        for (unsigned size : {0U, 8191U, 8193U, 4202496U}) {
            auto invalid = cartridge(size);
            writeFile("media-invalid.rom", invalid);
            require(!a1stInspectCartridge("media-invalid.rom", &detected, error, sizeof(error)) &&
                    std::strstr(error, "8 KiB to 4 MiB"), "Reject invalid cartridge sizes with the updated limit.");
            require(!a1stAttachCartridge(0, "media-invalid.rom", A1ST_MAPPER_AUTO, error, sizeof(error)),
                    "Invalid large cartridge attachment fails.");
            require(a1stReadMemory(0x4002) == 1, "Size failures retain the previous cartridge and selected bank.");
        }
        std::fill(large.begin(), large.end(), 0);
        writeFile("media-invalid.rom", large);
        require(!a1stInspectCartridge("media-invalid.rom", &detected, error, sizeof(error)) &&
                std::strstr(error, "AB header"), "Streaming inspection rejects invalid cartridge headers.");
        require(!a1stAttachCartridge(0, "media-invalid.rom", A1ST_MAPPER_AUTO, error, sizeof(error)) &&
                std::strstr(error, "AB header"), "Large files still require a valid cartridge header.");
        require(a1stReadMemory(0x4002) == 1, "Header failures retain the previous large cartridge.");
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
                              "media-sram.rom", "media-b.dsk", "media-large.rom", "media-invalid.rom"})
        require(std::remove(path) == 0, "Clean up media fixtures.");
    std::puts("A1ST dual cartridges, all mapper choices, SCC, SRAM and drive B tests passed.");
}
