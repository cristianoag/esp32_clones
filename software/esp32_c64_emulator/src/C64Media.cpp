#include "C64Media.h"
#include <stdio.h>
#include <string.h>

namespace
{
bool fail(char *error, size_t size, const char *message)
{
    if (error && size) snprintf(error, size, "%s", message);
    return false;
}
unsigned sectors(unsigned track)
{
    return track <= 17 ? 21 : track <= 24 ? 19 : track <= 30 ? 18 : 17;
}
}

int C64D64::sectorIndex(unsigned track, unsigned sector)
{
    if (!track || track > 35 || sector >= sectors(track)) return -1;
    unsigned index = sector;
    for (unsigned t = 1; t < track; ++t) index += sectors(t);
    return index;
}

bool C64D64::open(const uint8_t *data, size_t size, char *error, size_t errorSize)
{
    if (!data || (size != ImageSize && size != ImageSize + 683))
        return fail(error, errorSize, "Only 35-track D64 images (with/without error table) are supported.");
    C64D64 candidate;
    candidate.data_ = data;
    candidate.errors_ = size > ImageSize ? data + ImageSize : nullptr;
    bool visited[683] = {};
    unsigned track = 18, sector = 1;
    while (track)
    {
        const int index = sectorIndex(track, sector);
        if (index < 0 || visited[index])
            return fail(error, errorSize, "Invalid or cyclic D64 directory chain.");
        visited[index] = true;
        if (candidate.errors_ && candidate.errors_[index] != 1)
            return fail(error, errorSize, "D64 directory sector reports a disk error.");
        const uint8_t *block = data + index * 256;
        for (unsigned i = 0; i < 8; ++i)
        {
            const uint8_t *record = block + i * 32;
            if ((record[2] & 0x87) != 0x82) continue;
            if (candidate.count_ == 144 || sectorIndex(record[3], record[4]) < 0)
                return fail(error, errorSize, "Invalid D64 program entry or directory too large.");
            Entry &entry = candidate.entries_[candidate.count_++];
            entry.track = record[3];
            entry.sector = record[4];
            unsigned length = 0;
            for (; length < 16 && record[5 + length] != 0xa0; ++length)
            {
                const uint8_t c = record[5 + length];
                entry.name[length] = c >= 32 && c < 127 ? c : '?';
            }
            entry.name[length] = 0;
        }
        track = block[0];
        sector = block[1];
    }
    *this = candidate;
    return true;
}

bool C64D64::extract(unsigned index, uint8_t *output, size_t capacity, size_t &size,
                     char *error, size_t errorSize) const
{
    if (!data_ || index >= count_ || !output)
        return fail(error, errorSize, "Select a valid D64 PRG entry first.");
    bool visited[683] = {};
    unsigned track = entries_[index].track, sector = entries_[index].sector;
    size_t length = 0;
    while (track)
    {
        const int blockIndex = sectorIndex(track, sector);
        if (blockIndex < 0 || visited[blockIndex])
            return fail(error, errorSize, "Invalid or cyclic D64 file chain.");
        visited[blockIndex] = true;
        if (errors_ && errors_[blockIndex] != 1)
            return fail(error, errorSize, "D64 program sector reports a disk error.");
        const uint8_t *block = data_ + blockIndex * 256;
        if (!block[0] && !block[1])
            return fail(error, errorSize, "Invalid final D64 sector length.");
        const size_t bytes = block[0] ? 254 : block[1] - 1;
        if (length > capacity || bytes > capacity - length)
            return fail(error, errorSize, "D64 program exceeds the C64 address space.");
        memcpy(output + length, block + 2, bytes);
        length += bytes;
        track = block[0];
        sector = block[1];
    }
    if (length < 3) return fail(error, errorSize, "D64 program is empty or truncated.");
    size = length;
    return true;
}
