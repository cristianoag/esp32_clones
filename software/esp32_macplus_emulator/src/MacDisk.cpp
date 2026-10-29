#include "MacDisk.h"

namespace {
bool fail(char *error, size_t capacity, const char *message)
{
    if (error && capacity) snprintf(error, capacity, "%s", message);
    return false;
}
}

bool MacDisk::open(const char *path, bool readOnly, char *error, size_t capacity)
{
    FILE *candidate = fopen(path, readOnly ? "rb" : "r+b");
    if (!candidate) return fail(error, capacity, "Cannot open SCSI image in the selected access mode.");
    const bool seekOk = fseek(candidate, 0, SEEK_END) == 0;
    const long size = seekOk ? ftell(candidate) : -1;
    if (size < 512 || size > 0x40000000L || size % 512 || fseek(candidate, 0, SEEK_SET)) {
        fclose(candidate);
        return fail(error, capacity, "SCSI image must be sector-aligned, 512 bytes to 1 GiB.");
    }
    close();
    file = candidate;
    blockCount = static_cast<uint32_t>(size / 512);
    protectedDisk = readOnly;
    return true;
}

void MacDisk::close()
{
    if (file && fclose(file)) fprintf(stderr, "MACPLUS: closing SCSI image failed\n");
    file = nullptr;
    blockCount = 0;
    activityPending = false;
}

bool MacDisk::transfer(bool write, uint32_t lba, uint32_t count, uint8_t *data,
                       size_t capacity, char *error, size_t errorSize)
{
    if (!file) return fail(error, errorSize, "No SCSI image is attached.");
    if (write && protectedDisk) return fail(error, errorSize, "SCSI image is write-protected.");
    if (!data || count > capacity / 512 || lba > blockCount || count > blockCount - lba)
        return fail(error, errorSize, "SCSI transfer exceeds the image or controller buffer.");
    const size_t bytes = static_cast<size_t>(count) * 512;
    if (fseek(file, static_cast<long>(lba * 512), SEEK_SET))
        return fail(error, errorSize, "SCSI image seek failed.");
    if (write) {
        if (fwrite(data, 1, bytes, file) != bytes || fflush(file))
            return fail(error, errorSize, "SCSI image write/flush failed. Check the SD card.");
    } else if (fread(data, 1, bytes, file) != bytes) {
        return fail(error, errorSize, "SCSI image read failed. Check the SD card.");
    }
    if (bytes) activityPending = true;
    return true;
}
