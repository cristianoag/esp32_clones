#include "MacDisk.h"
#include "MacScsi.h"
#include "MacRom.h"
#include <cstdio>
#include <cstring>
#include <vector>
extern "C" {
#include "tme/emu.h"
#include "tme/tmeconfig.h"
#include "tme/rtc.h"
}

static unsigned errors = 0;

void macReportError(const char *message)
{
    fprintf(stderr, "Mac boot I/O error: %s\n", message);
    ++errors;
}

extern "C" void saveRtcMem(char *) {}

static int fail(const char *message)
{
    fprintf(stderr, "Mac boot test failed: %s\n", message);
    return 1;
}

int main(int argc, char **argv)
{
    if (argc != 3) return fail("Supply a ROM and a disposable whole-disk copy.");
    std::vector<uint8_t> rom(TME_ROMSIZE), ram(TME_RAMSIZE);
    FILE *file = fopen(argv[1], "rb");
    if (!file) return fail("Cannot open the supplied ROM.");
    const size_t size = fread(rom.data(), 1, rom.size(), file);
    const bool exact = fgetc(file) == EOF;
    const bool closed = fclose(file) == 0;
    if (!exact || !closed || !MacValidRom(rom.data(), size))
        return fail("Supply an unmodified 128 KiB Macintosh Plus ROM.");
    MacDisk disk;
    char error[160] = {}, pram[32] = {};
    if (!disk.open(argv[2], false, error, sizeof(error))) return fail(error);
    rtcInit(pram);
    tmeInit(rom.data(), ram.data());
    MacAttachScsi(&disk);
    unsigned accessFrames = 0, finderFrames = 0;
    for (unsigned frame = 0; frame < 18000; ++frame) {
        tmeRunFrame();
        if (disk.takeActivity()) ++accessFrames;
        if (errors) return fail("The firmware core reported a disk I/O fault.");
        if (ram[0x910] == 6 && !memcmp(ram.data() + 0x911, "Finder", 6)) {
            if (++finderFrames == 1200) {
                if (!accessFrames || (ram[0xb22] & 0xc0) != 0xc0)
                    return fail("Finder did not boot through the Plus SCSI controller.");
                printf("Mac Plus native boot passed: Finder stable for 1200 frames; "
                       "%u disk-active frames; %u total frames.\n", accessFrames, frame + 1);
                return 0;
            }
        } else finderFrames = 0;
    }
    fprintf(stderr, "Disk-active frames: %u\n", accessFrames);
    return fail("Finder did not become stable within 18000 emulated frames.");
}
