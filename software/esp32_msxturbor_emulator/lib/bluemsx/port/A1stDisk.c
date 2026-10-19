#include "A1stInternal.h"
#include "Disk.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

static FILE* images[2];
static int sides[2];
static int changed[2];
static int writeFailed[2];
static char paths[2][512];
int diskOffset;

static void ioError(const char* stage)
{
    char message[160];
    snprintf(message, sizeof(message), "A1ST disk %s failed: %s.", stage, strerror(errno));
    a1stLog(message);
}

bool a1stMountDisk(const char* path, char* error, size_t capacity)
{
    return a1stAttachDisk(0, path, error, capacity);
}

bool a1stAttachDisk(unsigned drive, const char* path, char* error, size_t capacity)
{
    FILE* candidate;
    long size;
    if (drive >= 2 || (path && strlen(path) >= sizeof(paths[0]))) {
        snprintf(error, capacity, "Invalid disk drive or overlong DSK path.");
        a1stLog(error); return false;
    }
    if (!path || !*path) {
        if (images[drive] && fclose(images[drive])) ioError("close");
        images[drive] = NULL;
        paths[drive][0] = 0;
        changed[drive] = 1;
        error[0] = 0;
        return true;
    }
    if (images[1 - drive] && !strcasecmp(paths[1 - drive], path)) {
        snprintf(error, capacity, "DSK is already attached to the other drive; eject it there first.");
        a1stLog(error); return false;
    }
    candidate = fopen(path, "r+b");
    if (!candidate) {
        snprintf(error, capacity, "Cannot open writable disk: %s.", strerror(errno));
        a1stLog(error); return false;
    }
    size = fseek(candidate, 0, SEEK_END) ? -1 : ftell(candidate);
    if (size != 368640 && size != 737280) {
        fclose(candidate);
        snprintf(error, capacity, "Disk must be a raw 360 or 720 KiB DSK.");
        a1stLog(error); return false;
    }
    if (images[drive] && fclose(images[drive])) ioError("close");
    images[drive] = candidate;
    strcpy(paths[drive], path);
    sides[drive] = size == 737280 ? 2 : 1;
    changed[drive] = 1;
    writeFailed[drive] = 0;
    if (capacity) error[0] = 0;
    return true;
}

void a1stDiskClose(void)
{
    unsigned drive;
    for (drive = 0; drive < 2; ++drive) {
        if (images[drive] && fclose(images[drive])) ioError("close");
        images[drive] = NULL;
        paths[drive][0] = 0;
    }
}
UInt8 diskEnabled(int drive) { return drive >= 0 && drive < 2; }
UInt8 diskPresent(int drive) { return diskEnabled(drive) && images[drive] != NULL; }
UInt8 diskReadOnly(int drive) { return !diskEnabled(drive) || writeFailed[drive]; }
int diskChanged(int drive)
{
    int value = diskEnabled(drive) && changed[drive];
    if (diskEnabled(drive)) changed[drive] = 0;
    return value;
}
int diskGetSides(int drive) { return diskPresent(drive) ? sides[drive] : 2; }
int diskGetSectorsPerTrack(int drive) { (void)drive; return 9; }
int diskGetSectorSize(int drive, int side, int track, int density)
{ (void)drive; (void)side; (void)track; (void)density; return 512; }

static int sectorIndex(int drive, int sector, int side, int track)
{
    if (!diskPresent(drive) || sector < 1 || sector > 9 ||
        side < 0 || side >= sides[drive] || track < 0 || track >= 80) return -1;
    return (track * sides[drive] + side) * 9 + sector - 1;
}
DSKE diskRead(int drive, UInt8* buffer, int sector)
{
    if (!diskPresent(drive) || sector < 0 || sector >= 80 * sides[drive] * 9)
        return DSKE_NO_DATA;
    if (fseek(images[drive], sector * 512L, SEEK_SET) || fread(buffer, 1, 512, images[drive]) != 512) {
        ioError("read"); return DSKE_NO_DATA;
    }
    return DSKE_OK;
}
DSKE diskReadSector(int drive, UInt8* buffer, int sector, int side, int track,
                    int density, int* sectorSize)
{
    int index = sectorIndex(drive, sector, side, track);
    (void)density;
    if (sectorSize) *sectorSize = 512;
    return index < 0 ? DSKE_NO_DATA : diskRead(drive, buffer, index);
}
UInt8 diskWrite(int drive, UInt8* buffer, int sector)
{
    bool ok;
    if (!diskPresent(drive) || diskReadOnly(drive) ||
        sector < 0 || sector >= 80 * sides[drive] * 9) return 0;
    ok = !fseek(images[drive], sector * 512L, SEEK_SET) &&
         fwrite(buffer, 1, 512, images[drive]) == 512 && !fflush(images[drive]);
#ifdef _WIN32
    if (ok) ok = !_commit(_fileno(images[drive]));
#else
    if (ok) ok = !fsync(fileno(images[drive]));
#endif
    if (!ok) { writeFailed[drive] = 1; ioError("write/sync; further writes blocked"); }
    return ok;
}
UInt8 diskWriteSector(int drive, UInt8* buffer, int sector, int side, int track, int density)
{
    int index = sectorIndex(drive, sector, side, track);
    (void)density;
    return index >= 0 && diskWrite(drive, buffer, index);
}
