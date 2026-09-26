#include "TkCore.h"
#include "TkInput.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static TkCore core;
static unsigned samples;
static int16_t audio[448];
static void frames(unsigned count)
{
    while (count--) core.runFrame(nullptr, audio, samples);
}
static unsigned word(unsigned address)
{
    return core.memory[address] | (unsigned(core.memory[address + 1]) << 8);
}
static void press(uint8_t code, uint8_t modifiers = 0)
{
    uint8_t report[8] = {modifiers,0,code,0,0,0,0,0}, rows[8];
    TkKeyboard(report, rows); core.input(rows, 0); frames(5);
    std::memset(report, 0, sizeof(report));
    TkKeyboard(report, rows); core.input(rows, 0); frames(5);
}
static bool check(bool condition, const char *message)
{
    if (!condition) std::fprintf(stderr, "%s (PC=%04X SP=%04X E_LINE=%04X)\n",
                                message, core.cpu.pc, core.cpu.sp, word(23641));
    return condition;
}
static void block(std::vector<uint8_t> &tape, const std::vector<uint8_t> &bytes)
{
    tape.push_back((bytes.size() + 1) & 255);
    tape.push_back((bytes.size() + 1) >> 8);
    uint8_t checksum = 0;
    for (uint8_t byte : bytes) { tape.push_back(byte); checksum ^= byte; }
    tape.push_back(checksum);
}

int main(int argc, char **argv)
{
    if (argc != 3) { std::fprintf(stderr, "Usage: RomSmoke <16KiB-ROM> <tk95|tk90x>\n"); return 1; }
    const TkModel model = !std::strcmp(argv[2], "tk90x") ? TkModel::TK90X : TkModel::TK95;
    FILE *file = std::fopen(argv[1], "rb");
    if (!file) { std::perror("Cannot open ROM"); return 1; }
    std::vector<uint8_t> rom(16384);
    const size_t size = std::fread(rom.data(), 1, rom.size(), file);
    const bool extra = std::fgetc(file) != EOF;
    std::fclose(file);
    if (size != rom.size() || extra) { std::fprintf(stderr, "ROM must be exactly 16384 bytes.\n"); return 1; }
    for (TkTiming timing : {TkTiming::Hz60, TkTiming::Hz50})
    {
        char error[160];
        if (!core.boot(rom.data(), rom.size(), model, timing, error, sizeof(error)))
        {
            std::fprintf(stderr, "%s\n", error);
            return 1;
        }
        core.portuguese = true;
        uint8_t rows[8]; std::memset(rows, 255, sizeof(rows)); core.input(rows, 0);
        frames(350);
        const unsigned edit = word(23641);
        if (!check(word(23730) >= 0xff00 && edit >= 0x5ccb && edit < 0xff00,
                   "ROM did not initialize BASIC RAMTOP/edit buffer")) return 1;
        const unsigned printAddress = word(23684);
        press(19); // P -> PRINT token in keyword mode.
        press(30); // 1
        press(14, 1); // SYMBOL SHIFT + K -> +
        press(30);
        const uint8_t expected[] = {0xf5, '1', '+', '1', 13};
        if (!check(!std::memcmp(core.memory + word(23641), expected, sizeof(expected)),
                   "BASIC keyboard entry did not produce PRINT 1+1")) return 1;
        press(40); // ENTER
        frames(50);
        bool printed = true;
        for (unsigned y = 0; y < 8; ++y)
            printed &= core.memory[printAddress + y * 256] == rom[0x3c00 + '2' * 8 + y];
        if (!printed)
        {
            std::fprintf(stderr, "ERR_NR=%02X CHARS=%04X DF_CC=%04X start=%04X glyph actual/expected:",
                         core.memory[23610], word(23606), word(23684), printAddress);
            for (unsigned y = 0; y < 8; ++y)
                std::fprintf(stderr, " %02X/%02X", core.memory[printAddress + y * 256], rom[0x3c00 + '2' * 8 + y]);
            std::fprintf(stderr, "\n");
        }
        if (!check(printed, "BASIC PRINT 1+1 did not render digit 2")) return 1;
        std::printf("PASS: %s, %s Hz, BASIC boot + keyboard PRINT 1+1 = 2, PC=%04X RAMTOP=%04X\n",
                    argv[1], timing == TkTiming::Hz60 ? "60" : "50", core.cpu.pc, word(23730));

        // Original synthetic BASIC line: 10 REM TK. No external tape is required.
        const uint8_t program[] = {0,10,4,0,0xea,'T','K',13};
        std::vector<uint8_t> tape;
        block(tape, {0,0,'T','K','S','M','O','K','E',' ',' ',' ',8,0,0,0x80,8,0});
        std::vector<uint8_t> data = {255};
        data.insert(data.end(), program, program + sizeof(program));
        block(tape, data);
        if (!check(core.tape.attach(tape.data(), tape.size(), error, sizeof(error)), error)) return 1;
        press(13); press(19, 1); press(19, 1); // LOAD ""
        const uint8_t load[] = {0xef,'"', '"',13};
        if (!check(!std::memcmp(core.memory + word(23641), load, sizeof(load)), "LOAD command entry failed")) return 1;
        press(40);
        core.tape.play(true);
        frames(950);
        const unsigned prog = word(23635);
        if (!check(prog >= 0x5ccb && prog < 0xff00 &&
                   !std::memcmp(core.memory + prog, program, sizeof(program)), "ROM failed to load synthetic TAP")) return 1;
        if (!check(!core.tape.playing(), "TAP did not finish")) return 1;
        std::printf("PASS: %s at %s Hz loaded synthetic BASIC TAP through ROM EAR routines.\n",
                    argv[2], timing == TkTiming::Hz60 ? "60" : "50");
        core.tape.eject();
    }
}
