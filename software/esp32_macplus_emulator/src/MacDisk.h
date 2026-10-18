#pragma once
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

class MacDisk
{
public:
    ~MacDisk() { close(); }
    bool open(const char *path, bool readOnly, char *error, size_t capacity);
    void close();
    bool transfer(bool write, uint32_t lba, uint32_t count, uint8_t *data,
                  size_t capacity, char *error, size_t errorSize);
    uint32_t blocks() const { return blockCount; }
    bool readOnly() const { return protectedDisk; }
private:
    FILE *file = nullptr;
    uint32_t blockCount = 0;
    bool protectedDisk = true;
};
