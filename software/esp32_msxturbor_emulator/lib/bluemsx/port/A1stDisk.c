#include "A1stInternal.h"
#include "Disk.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

static FILE* image;
static int sides;
static int changed;
static int writeFailed;
int diskOffset;

static void ioError(const char* stage)
{
    char message[160];
    snprintf(message, sizeof(message), "A1ST disk %s failed: %s.", stage, strerror(errno));
    a1stLog(message);
}

bool a1stMountDisk(const char* path, char* error, size_t capacity)
{
    FILE* candidate;
    long size;
    if (!path || !*path) { a1stDiskClose(); changed = 1; return true; }
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
    a1stDiskClose();
    image = candidate;
    sides = size == 737280 ? 2 : 1;
    changed = 1;
    writeFailed = 0;
    if (capacity) error[0] = 0;
    return true;
}

void a1stDiskClose(void)
{
    if (image && fclose(image)) ioError("close");
    image = NULL;
}
UInt8 diskEnabled(int drive) { return drive == 0; }
UInt8 diskPresent(int drive) { return drive == 0 && image != NULL; }
UInt8 diskReadOnly(int drive) { return drive != 0 || writeFailed; }
int diskChanged(int drive)
{
    int value = drive == 0 && changed;
    if (drive == 0) changed = 0;
    return value;
}
int diskGetSides(int drive) { return diskPresent(drive) ? sides : 2; }
int diskGetSectorsPerTrack(int drive) { (void)drive; return 9; }
int diskGetSectorSize(int drive, int side, int track, int density)
{ (void)drive; (void)side; (void)track; (void)density; return 512; }

static int sectorIndex(int drive, int sector, int side, int track)
{
    if (!diskPresent(drive) || sector < 1 || sector > 9 ||
        side < 0 || side >= sides || track < 0 || track >= 80) return -1;
    return (track * sides + side) * 9 + sector - 1;
}
DSKE diskRead(int drive, UInt8* buffer, int sector)
{
    if (!diskPresent(drive) || sector < 0 || sector >= 80 * sides * 9)
        return DSKE_NO_DATA;
    if (fseek(image, sector * 512L, SEEK_SET) || fread(buffer, 1, 512, image) != 512) {
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
        sector < 0 || sector >= 80 * sides * 9) return 0;
    ok = !fseek(image, sector * 512L, SEEK_SET) &&
         fwrite(buffer, 1, 512, image) == 512 && !fflush(image);
#ifdef _WIN32
    if (ok) ok = !_commit(_fileno(image));
#else
    if (ok) ok = !fsync(fileno(image));
#endif
    if (!ok) { writeFailed = 1; ioError("write/sync; further writes blocked"); }
    return ok;
}
UInt8 diskWriteSector(int drive, UInt8* buffer, int sector, int side, int track, int density)
{
    int index = sectorIndex(drive, sector, side, track);
    (void)density;
    return index >= 0 && diskWrite(drive, buffer, index);
}
