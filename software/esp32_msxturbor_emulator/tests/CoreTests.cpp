#include "A1stCore.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <algorithm>
#include <vector>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
extern "C" {
#include "R800.h"
extern UInt32* boardSysTime;
}

static void require(bool value, const char* message)
{
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
static size_t romBytesRead;
static size_t romReadLimit = SIZE_MAX;
static size_t romReadFragment = SIZE_MAX;
static int romReadError;
static bool interruptRomRead;

extern "C" long a1stTestRomRead(int file, void* buffer, size_t count)
{
    require(count <= 4096 && reinterpret_cast<uintptr_t>(buffer) % 4 == 0,
            "ROM reads use bounded, aligned staging memory.");
    if (interruptRomRead) {
        interruptRomRead = false;
        errno = EINTR;
        return -1;
    }
    if (romBytesRead >= romReadLimit) {
        errno = romReadError;
        return romReadError ? -1 : 0;
    }
    count = std::min(count, std::min(romReadFragment, romReadLimit - romBytesRead));
    const long received = read(file, buffer, count);
    if (received > 0) romBytesRead += static_cast<size_t>(received);
    return received;
}

static void writeFixture(const char* path, const unsigned char* bytes, size_t count)
{
    FILE* file = std::fopen(path, "wb");
    require(file && std::fwrite(bytes, 1, count, file) == count, "Write ROM fixture.");
    require(std::fclose(file) == 0, "Close ROM fixture.");
}
static void message(const char* value) { std::fprintf(stderr, "A1ST: %s\n", value); }
static unsigned audioSamples;
void a1stMediaTests();
static void audio(const int16_t*, unsigned count) { audioSamples += count; }
static std::vector<char> readVram()
{
    a1stWritePort(0x99, 0);
    a1stWritePort(0x99, 0x8e);
    a1stWritePort(0x99, 0);
    a1stWritePort(0x99, 0);
    std::vector<char> bytes(16384);
    for (auto& byte : bytes) byte = static_cast<char>(a1stReadPort(0x98));
    return bytes;
}
static unsigned occurrences(const std::vector<char>& bytes, const char* text)
{
    unsigned count = 0;
    const size_t length = std::strlen(text);
    for (size_t i = 0; i + length <= bytes.size(); ++i)
        if (!std::memcmp(bytes.data() + i, text, length)) ++count;
    return count;
}
static void typeBasic(const char* command)
{
    uint8_t matrix[16];
    for (const char* character = command; *character; ++character) {
        std::memset(matrix, 0xff, sizeof(matrix));
        unsigned position;
        if (*character >= 'A' && *character <= 'Z') {
            position = 22 + *character - 'A';
            matrix[6] &= ~1U;
        }
        else if (*character >= '0' && *character <= '9') position = *character - '0';
        else if (*character == ' ') position = 64;
        else if (*character == '\n') position = 63;
        else if (*character == ':') position = 16;
        else if (*character == '"') { position = 2; matrix[6] &= ~1U; }
        else { require(false, "Unsupported fixture key."); return; }
        matrix[position / 8] &= static_cast<uint8_t>(~(1U << (position % 8)));
        a1stSetKeyboard(matrix);
        for (unsigned frame = 0; frame < 3; ++frame) a1stRunFrame();
        std::memset(matrix, 0xff, sizeof(matrix));
        a1stSetKeyboard(matrix);
        for (unsigned frame = 0; frame < 3; ++frame) a1stRunFrame();
    }
    for (unsigned frame = 0; frame < 60; ++frame) a1stRunFrame();
}
static void formattedDisk(const char* path = "original-fixture.dsk")
{
    std::vector<unsigned char> disk(737280, 0);
    disk[0] = 0xeb; disk[1] = 0xfe; disk[2] = 0x90;
    std::memcpy(disk.data() + 3, "A1STTEST", 8);
    disk[11] = 0; disk[12] = 2; disk[13] = 2;
    disk[14] = 1; disk[16] = 2; disk[17] = 112;
    disk[19] = 0xa0; disk[20] = 5; disk[21] = 0xf9;
    disk[22] = 3; disk[24] = 9; disk[26] = 2;
    disk[510] = 0x55; disk[511] = 0xaa;
    for (unsigned fat : {512U, 2048U}) {
        disk[fat] = 0xf9; disk[fat + 1] = disk[fat + 2] = 0xff;
        disk[fat + 3] = 0xff; disk[fat + 4] = 0x0f;
    }
    const char program[] = "10 PRINT 12345\r\n";
    unsigned char* entry = disk.data() + 7 * 512;
    std::memcpy(entry, "HELLO   BAS", 11);
    entry[11] = 0x20; entry[26] = 2; entry[28] = sizeof(program) - 1;
    std::memcpy(disk.data() + 14 * 512, program, sizeof(program) - 1);
    FILE* file = std::fopen(path, "wb");
    require(file && std::fwrite(disk.data(), 1, disk.size(), file) == disk.size(), "Create FAT12 fixture.");
    std::fclose(file);
}
static UInt8 cpuMemory[65536];
static UInt8 readCpu(void*, UInt16 address) { return cpuMemory[address]; }
static void writeCpu(void*, UInt16 address, UInt8 value) { cpuMemory[address] = value; }
static void cpuTests()
{
    R800* cpu = r800Create(CPU_ENABLE_M1, readCpu, writeCpu, nullptr, nullptr,
                           nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    r800SetMode(cpu, CPU_R800);
    r800ExecuteUntil(cpu, 1);
    cpu->regs.PC.W = 0;
    cpuMemory[0] = 0xed;
    cpuMemory[1] = 0xc1;
    cpu->regs.AF.W = 13 << 8;
    cpu->regs.BC.W = 7 << 8;
    r800ExecuteInstruction(cpu);
    require(cpu->regs.HL.W == 91, "R800 MULUB A,B.");
    cpu->regs.PC.W = 0;
    cpu->regs.AF.W = 255 << 8;
    cpu->regs.BC.W = 255 << 8;
    r800ExecuteInstruction(cpu);
    require(cpu->regs.HL.W == 65025 && (cpu->regs.AF.B.l & C_FLAG), "MULUB carry threshold.");
    cpu->regs.PC.W = 0;
    cpuMemory[1] = 0xc3;
    cpu->regs.HL.W = 0x1234;
    cpu->regs.BC.W = 0x10;
    r800ExecuteInstruction(cpu);
    require(cpu->regs.DE.W == 1 && cpu->regs.HL.W == 0x2340, "R800 MULUW HL,BC.");
    r800Destroy(cpu);
}

int main(int argc, char** argv)
{
    A1stHost host = { message, audio };
    char error[512];
    if (argc == 3) {
        require(a1stCreate(argv[1], argv[2], "original-state", &host, error, sizeof(error)), error);
        for (unsigned slot = 0; slot < 2; ++slot) {
            std::vector<unsigned char> cartridge(8192, 0);
            cartridge[0] = 'A'; cartridge[1] = 'B';
            cartridge[2] = 0x10; cartridge[3] = 0x40;
            const unsigned char init[] = {0x3e, static_cast<unsigned char>(0x5a + slot),
                                          0x32, static_cast<unsigned char>(slot), 0xc1, 0xc9};
            std::memcpy(cartridge.data() + 0x10, init, sizeof(init));
            const char* path = slot ? "original-slot2.rom" : "original-slot1.rom";
            writeFixture(path, cartridge.data(), cartridge.size());
            require(a1stAttachCartridge(slot, path, A1ST_MAPPER_AUTO, error, sizeof(error)), error);
        }
        formattedDisk("original-fixture-b.dsk");
        require(a1stAttachDisk(1, "original-fixture-b.dsk", error, sizeof(error)), error);
        for (unsigned frame = 0; frame < 1200; ++frame) a1stRunFrame();
        A1stStats stats;
        a1stGetStats(&stats);
        std::printf("Boot: frames=%u pc=%04x R800=%u DRAM=%u switches=%u ticks=%llu audio=%u\n",
                    stats.frames, stats.pc, stats.r800, stats.dram, stats.cpuSwitches,
                    static_cast<unsigned long long>(stats.masterTicks), audioSamples);
        const auto vram = readVram();
        unsigned basic = 16384;
        for (unsigned i = 0; i + 9 < 16384; ++i)
            if (!std::memcmp(vram.data() + i, "MSX BASIC", 9)) basic = i;
        const unsigned start = basic < 16384 ? basic / 40 * 40 : 0;
        for (unsigned y = 0; y < 24; ++y) {
            for (unsigned x = 0; x < 40; ++x) {
                unsigned char c = static_cast<unsigned char>(vram[(start + y * 40 + x) % 16384]);
                std::putchar(c >= 32 && c < 127 ? c : '.');
            }
            std::putchar('\n');
        }
        require(basic < 16384, "Original A1ST ROM reached MSX BASIC.");
        require(a1stReadMemory(0xc100) == 0x5a && a1stReadMemory(0xc101) == 0x5b,
                "Original BIOS executes initialization ROMs in both external slots.");
        require(stats.cpuSwitches > 0, "A1ST BIOS exercised S1990 CPU switching.");
        require(audioSamples > 400000, "Audio followed emulated time.");
        typeBasic("PRINT 1234567\n");
        require(occurrences(readVram(), "1234567") >= 2, "Original BASIC executes matrix keyboard input.");
        formattedDisk();
        require(a1stMountDisk("original-fixture.dsk", error, sizeof(error)), error);
        typeBasic("FILES\n");
        require(occurrences(readVram(), "HELLO") > 0, "Original disk BIOS lists the FAT12 fixture through TC8566AF.");
        const unsigned listings = occurrences(readVram(), "HELLO");
        typeBasic("FILES \"B:\"\n");
        require(occurrences(readVram(), "HELLO") > listings,
                "Original disk BIOS lists a real second drive through TC8566AF.");
        require(a1stFlushState(error, sizeof(error)), error);
        a1stDestroy();
        std::remove("original-fixture.dsk");
        std::remove("original-fixture-b.dsk");
        std::remove("original-slot1.rom");
        std::remove("original-slot2.rom");
        std::puts("Original-ROM A1ST BASIC boot, both cartridge initializers, keyboard and Disk BASIC drives A/B passed.");
        return 0;
    }
    require(!a1stCreate("missing.rom", "missing.rom", ".", &host, error, sizeof(error)),
            "Missing firmware must fail.");
    require(std::strstr(error, "ROM open failed") && std::strstr(error, "errno"),
            "Missing-ROM error identifies the failed operation.");
    std::vector<unsigned char> rom(A1ST_FIRMWARE_BYTES + 1);
    for (unsigned bank = 0; bank < 512; ++bank)
        std::memset(rom.data() + bank * 8192, bank & 255, 8192);
    writeFixture("synthetic-kanji.rom", rom.data(), A1ST_KANJI_BYTES);
    for (size_t length : {size_t(A1ST_FIRMWARE_BYTES - 1), size_t(A1ST_FIRMWARE_BYTES + 1)}) {
        writeFixture("synthetic.rom", rom.data(), length);
        require(!a1stCreate("synthetic.rom", "synthetic-kanji.rom", "synthetic-state", &host, error, sizeof(error)),
                "Reject both undersized and oversized firmware.");
        require(std::strstr(error, "ROM size mismatch") && romBytesRead == 0,
                "Validate exact full dump size before reading the mapped ROM prefix.");
    }
    writeFixture("synthetic.rom", rom.data(), A1ST_FIRMWARE_BYTES);
    romReadLimit = 8192;
    romReadError = EIO;
    require(!a1stCreate("synthetic.rom", "synthetic-kanji.rom", "synthetic-state", &host, error, sizeof(error)),
            "A mid-ROM I/O error must fail even when file size is correct.");
    require(std::strstr(error, "ROM read failed at 8192/2097152") && std::strstr(error, "errno"),
            "Report read offset and OS error, not a misleading size mismatch.");
    romBytesRead = 0;
    romReadError = 0;
    require(!a1stCreate("synthetic.rom", "synthetic-kanji.rom", "synthetic-state", &host, error, sizeof(error)),
            "Unexpected EOF must fail even when file size is correct.");
    require(std::strstr(error, "ROM ended after 8192/2097152"),
            "Distinguish unexpected EOF from an OS read error.");
    romBytesRead = 0;
    romReadLimit = SIZE_MAX;
    romReadFragment = 513;
    interruptRomRead = true;
    require(a1stCreate("synthetic.rom", "synthetic-kanji.rom", "synthetic-state", &host, error, sizeof(error)), error);
    require(!interruptRomRead && romBytesRead == 0x200000 + A1ST_KANJI_BYTES,
            "Retry interrupted/short reads and load exactly the mapped ROM and Kanji bytes.");
    romReadFragment = SIZE_MAX;
    a1stMediaTests();
    a1stWritePort(0xa0, 7);
    a1stWritePort(0xa1, 0x40);
    for (unsigned port = 0; port < 2; ++port) {
        a1stWritePort(0xa0, 15);
        a1stWritePort(0xa1, port ? 0x40 : 0);
        a1stWritePort(0xa0, 14);
        for (unsigned bit = 0; bit < 6; ++bit) {
            a1stSetJoysticks(1U << (port * 8 + bit));
            require(a1stReadPort(0xa2) == (0xff ^ (1U << bit)),
                    "Both PSG joystick ports expose all directions and buttons active-low.");
        }
        a1stSetJoysticks(0);
        require(a1stReadPort(0xa2) == 0xff, "Disconnected joystick releases every PSG input.");
    }
    FILE* file;
    require(a1stReadMemory(0) == 40, "BIOS comes from firmware block 40.");
    a1stWritePort(0xa8, 0xff);
    a1stWriteMemory(0xffff, 0x55);
    require(a1stReadMemory(0) == 56, "Sub-ROM comes from block 56.");
    require(a1stReadMemory(0x8000) == 60, "Upper Kanji BASIC ROM is mapped.");
    a1stWriteMemory(0xffff, 0xff);
    a1stWriteMemory(0x6000, 7);
    require(a1stReadMemory(0) == 7, "Panasonic bank selection.");
    a1stWriteMemory(0x7ff9, 0x1c);
    require(a1stReadMemory(0x7ff0) == 7, "Panasonic bank readback.");
    a1stWriteMemory(0x6000, 0x80);
    a1stWriteMemory(0x1234, 0xa5);
    a1stWriteMemory(0x7000, 0x80);
    require(a1stReadMemory(0x9234) == 0xa5, "SRAM writes work outside 8000-BFFF.");
    a1stWriteMemory(0xffff, 0);
    a1stWritePort(0xfc, 12);
    a1stWriteMemory(0, 0x33);
    a1stWritePort(0xa8, 0);
    a1stWritePort(0xe4, 6);
    a1stWritePort(0xe5, 0x20);
    require(a1stReadMemory(0) == 0x33, "DRAM BIOS aliases the top 64 KiB of main RAM.");
    a1stWritePort(0xa8, 0xff);
    a1stWriteMemory(0xffff, 0xff);
    a1stWriteMemory(0x6000, 0x28);
    require(a1stReadMemory(0) == 0x33, "Panasonic firmware mapper observes DRAM BIOS overlays.");
    a1stWritePort(0xe5, 0x60);
    a1stWritePort(0xe4, 6);
    require(a1stReadPort(0xe5) == 0x60, "S1990 resets in Z80 ROM mode.");
    a1stWritePort(0xe5, 0);
    require(a1stReadPort(0xe5) == 0, "S1990 accepts R800 DRAM mode.");
    a1stWritePort(0xe4, 14);
    A1stStats stats;
    a1stGetStats(&stats);
    require(a1stReadPort(0xe4) == 14, "Telemetry preserves selected S1990 register.");
    a1stWritePort(0xe4, 6);
    a1stWritePort(0xe5, 0x60);
    a1stWritePort(0xa8, 0);
    uint8_t matrix[16];
    std::memset(matrix, 0xff, sizeof(matrix));
    matrix[3] = 0xfe;
    a1stSetKeyboard(matrix);
    a1stWritePort(0xab, 0x82);
    a1stWritePort(0xaa, 3);
    require(a1stReadPort(0xa9) == 0xfe, "PPI reads keyboard matrix.");
    *boardSysTime = 83;
    a1stWritePort(0xe6, 0);
    *boardSysTime = 84;
    require(a1stReadPort(0xe6) == 1, "E6 reset preserves oscillator phase.");
    a1stWritePort(0xe7, 0);
    require(a1stReadPort(0xe6) == 1, "Writing E7 does not reset the timer.");
    std::vector<unsigned char> disk(737280, 0);
    disk[0] = 0x5a;
    file = std::fopen("synthetic.dsk", "wb");
    require(file && std::fwrite(disk.data(), 1, disk.size(), file) == disk.size(), "Create disk fixture.");
    std::fclose(file);
    require(a1stMountDisk("synthetic.dsk", error, sizeof(error)), error);
    require(!a1stMountDisk("missing.dsk", error, sizeof(error)), "Failed mount rejects missing disk.");
    a1stWritePort(0xa8, 0xff);
    a1stWriteMemory(0xffff, 0xaa);
    a1stWriteMemory(0x7ff2, 0x10);
    const uint8_t readCommand[] = {0x46, 0, 0, 0, 1, 2, 1, 0x1b, 0xff};
    for (auto byte : readCommand) a1stWriteMemory(0x7ff5, byte);
    require(a1stReadMemory(0x7ff5) == 0x5a, "Original TC8566AF reads a sector after failed remount.");
    for (unsigned i = 1; i < 512; ++i) a1stReadMemory(0x7ff5);
    for (unsigned i = 0; i < 7; ++i) a1stReadMemory(0x7ff5);
    const uint8_t writeCommand[] = {0x45, 0, 0, 0, 1, 2, 1, 0x1b, 0xff};
    for (auto byte : writeCommand) a1stWriteMemory(0x7ff5, byte);
    for (unsigned i = 0; i < 512; ++i) a1stWriteMemory(0x7ff5, 0x69);
    file = std::fopen("synthetic.dsk", "rb");
    require(file && std::fgetc(file) == 0x69, "FDC sector writes persist before success.");
    std::fclose(file);
    require(a1stFlushState(error, sizeof(error)), error);
    a1stDestroy();
    std::remove("synthetic.rom");
    std::remove("synthetic-kanji.rom");
    std::remove("synthetic.dsk");
    cpuTests();
    std::puts("A1ST mapping, R800 multiply, timer, keyboard, FDC and persistence tests passed.");
}
