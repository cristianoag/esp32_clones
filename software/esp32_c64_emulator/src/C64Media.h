#pragma once
#include <stddef.h>
#include <stdint.h>

class C64D64
{
public:
    struct Entry { char name[17]; uint8_t track, sector; };
    static constexpr size_t ImageSize = 174848;
    bool open(const uint8_t *data, size_t size, char *error, size_t errorSize);
    bool extract(unsigned index, uint8_t *output, size_t capacity, size_t &size,
                 char *error, size_t errorSize) const;
    unsigned count() const { return count_; }
    const Entry &entry(unsigned index) const { return entries_[index]; }
private:
    const uint8_t *data_ = nullptr;
    const uint8_t *errors_ = nullptr;
    unsigned count_ = 0;
    Entry entries_[144] = {};
    static int sectorIndex(unsigned track, unsigned sector);
};
