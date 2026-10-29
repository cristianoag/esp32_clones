#include "MacFloppy.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef ARDUINO
#include <esp_heap_caps.h>
#endif
extern "C" {
#include "tme/iwm.h"
}

namespace {
bool fail(char *error, size_t capacity, const char *message)
{
    if (error && capacity) snprintf(error, capacity, "%s", message);
    return false;
}
}

bool MacFloppy::open(const char *path, char *error, size_t capacity)
{
    if (iwmInserted()) return fail(error, capacity, "Eject the current floppy before inserting another.");
    FILE *file = fopen(path, "rb");
    if (!file) return fail(error, capacity, "Cannot open the floppy image.");
    const bool seek = fseek(file, 0, SEEK_END) == 0;
    const long size = seek ? ftell(file) : -1;
    if ((size != 400 * 1024 && size != 800 * 1024) || fseek(file, 0, SEEK_SET)) {
        fclose(file);
        return fail(error, capacity, "Use a raw 400 KB or 800 KB floppy, not Disk Copy, compressed or 1.44 MB.");
    }
#ifdef ARDUINO
    uint8_t *candidate = static_cast<uint8_t *>(heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
#else
    uint8_t *candidate = static_cast<uint8_t *>(malloc(size));
#endif
    if (!candidate) {
        fclose(file);
        return fail(error, capacity, "Not enough memory to cache the read-only floppy.");
    }
    const bool read = fread(candidate, 1, size, file) == static_cast<size_t>(size);
    const bool closed = fclose(file) == 0;
    if (!read || !closed) {
        free(candidate);
        return fail(error, capacity, "Cannot read the complete floppy image.");
    }
    close();
    if (!iwmInsert(candidate, size)) {
        free(candidate);
        return fail(error, capacity, "The floppy controller rejected the image.");
    }
    image = candidate;
    return true;
}

void MacFloppy::close()
{
    if (inserted()) iwmEject();
    free(image);
    image = nullptr;
}

bool MacFloppy::inserted() const
{
    return image && iwmImage() == image;
}
