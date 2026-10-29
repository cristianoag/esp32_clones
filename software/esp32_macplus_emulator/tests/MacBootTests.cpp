#include "MacDisk.h"
#include "MacScsi.h"
#include "MacRom.h"
#include "MacFloppy.h"
#include <cstdio>
#include <cstring>
#include <vector>
extern "C" {
#include "tme/emu.h"
#include "tme/tmeconfig.h"
#include "tme/rtc.h"
#include "tme/iwm.h"
#include "tme/via.h"
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

static unsigned read16(const std::vector<uint8_t> &ram, unsigned address)
{
    return (unsigned(ram[address]) << 8) | ram[address + 1];
}

static unsigned read32(const std::vector<uint8_t> &ram, unsigned address)
{
    return (read16(ram, address) << 16) | read16(ram, address + 2);
}

static bool floppyVolumeName(const char *path, char name[28])
{
    FILE *source = fopen(path, "rb");
    if (!source) return false;
    uint8_t mdb[64];
    const bool read = fseek(source, 1024, SEEK_SET) == 0 && fread(mdb, 1, sizeof(mdb), source) == sizeof(mdb);
    const bool closed = fclose(source) == 0;
    if (!read || !closed || mdb[36] > 27 || !mdb[36]) return false;
    memset(name, 0, 28);
    memcpy(name, mdb + 37, mdb[36]);
    return true;
}

int main(int argc, char **argv)
{
    if (argc < 3 || argc > 5) return fail("Supply a ROM, a disk copy and optional one or two raw floppies.");
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
    MacFloppy floppy;
    unsigned accessFrames = 0, finderFrames = 0, floppyFrames = 0, insertedFrame = 0, ejectFrame = 0;
    unsigned mountedFrame = 0, ejectKeyFrame = 0;
    bool keysReleased = false;
    bool swapped = false;
    char volumeName[28] = {};
    if (argc >= 4 && !floppyVolumeName(argv[3], volumeName))
        return fail("The optional native-test floppy needs a named HFS or MFS volume.");
    for (unsigned frame = 0; frame < 18000; ++frame) {
        tmeRunFrame();
        if (disk.takeActivity()) ++accessFrames;
        if (iwmTakeActivity()) ++floppyFrames;
        if (errors) return fail("The firmware core reported a disk I/O fault.");
        if (ram[0x910] == 6 && !memcmp(ram.data() + 0x911, "Finder", 6)) {
            if (!insertedFrame && ++finderFrames == 1200) {
                if (!accessFrames || (ram[0xb22] & 0xc0) != 0xc0)
                    return fail("Finder did not boot through the Plus SCSI controller.");
                printf("Mac Plus native boot passed: Finder stable for 1200 frames; "
                       "%u disk-active frames; %u total frames.\n", accessFrames, frame + 1);
                if (argc == 3) return 0;
                if (!floppy.open(argv[3], error, sizeof(error))) return fail(error);
                insertedFrame = frame;
                printf("Inserted runtime floppy %s at frame %u.\n", volumeName, frame);
            }
        } else finderFrames = 0;
        if (mountedFrame && !ejectKeyFrame && frame - mountedFrame == 300) {
            kbdPushKey(0x37, 0);
            kbdPushKey(0x38, 0);
            kbdPushKey(0x12, 0);
            ejectKeyFrame = frame;
        }
        if (ejectKeyFrame && !keysReleased && frame - ejectKeyFrame == 10) {
            kbdPushKey(0x12, 1);
            kbdPushKey(0x38, 1);
            kbdPushKey(0x37, 1);
            keysReleased = true;
        }
        if (ejectKeyFrame && !ejectFrame && !floppy.inserted()) {
            floppy.close();
            ejectFrame = frame;
            printf("Macintosh ejected the first floppy through the drive controller.\n");
        }
        if (ejectFrame && frame - ejectFrame == 120) {
            if (!floppyVolumeName(argv[4], volumeName) ||
                !floppy.open(argv[4], error, sizeof(error))) return fail("Cannot insert the replacement floppy.");
            swapped = true;
            floppyFrames = 0;
            printf("Inserted replacement floppy %s.\n", volumeName);
        }
        if (insertedFrame && (!ejectFrame || swapped)) {
            unsigned vcb = read32(ram, 0x358);
            for (unsigned i = 0; i < 32 && vcb && vcb <= ram.size() - 128; ++i) {
                if (ram[vcb + 44] == strlen(volumeName) &&
                    !memcmp(ram.data() + vcb + 45, volumeName, strlen(volumeName)) &&
                    read16(ram, vcb + 72) == 1) {
                    if (!floppyFrames) return fail("The mounted floppy had no IWM data transfers.");
                    if (!(read16(ram, vcb + 18) & 0x80)) return fail("The floppy volume is not write-protected.");
                    if (!mountedFrame || swapped)
                        printf("Runtime floppy mounted: %s; %u IWM-active frames; attributes=%04x.\n",
                               volumeName, floppyFrames, read16(ram, vcb + 18));
                    if (argc < 5 || swapped) return 0;
                    if (!mountedFrame) mountedFrame = frame;
                    break;
                }
                vcb = read32(ram, vcb);
            }
            if (!(frame % 1200)) {
                printf("Waiting for floppy mount: frame=%u, IWM-active frames=%u.\n", frame, floppyFrames);
                fflush(stdout);
            }
        }
    }
    fprintf(stderr, "Disk-active frames: %u\n", accessFrames);
    return fail("Finder or the inserted floppy did not become ready within 18000 emulated frames.");
}
