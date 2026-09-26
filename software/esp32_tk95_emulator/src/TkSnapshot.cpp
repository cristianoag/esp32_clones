#include "TkCore.h"
#include <memory>
#include <new>
#include <stdio.h>
#include <string.h>

namespace
{
uint16_t word(const uint8_t *p) { return p[0] | (unsigned(p[1]) << 8); }
bool fail(char *error, size_t capacity, const char *message)
{
    snprintf(error, capacity, "%s", message);
    return false;
}
bool unpack(const uint8_t *source, size_t length, uint8_t *output, size_t count)
{
    size_t in = 0, out = 0;
    while (in < length && out < count)
    {
        if (length - in >= 2 && source[in] == 0xed && source[in + 1] == 0xed)
        {
            if (length - in < 4 || !source[in + 2] || source[in + 2] > count - out) return false;
            memset(output + out, source[in + 3], source[in + 2]);
            out += source[in + 2];
            in += 4;
        }
        else output[out++] = source[in++];
    }
    return in == length && out == count;
}
}

bool TkLoadSnapshot(TkCore &core, const uint8_t *data, size_t size, bool sna,
                    char *error, size_t capacity)
{
    if (!data || size < 27) return fail(error, capacity, "Truncated snapshot.");
    std::unique_ptr<uint8_t[]> ram(new (std::nothrow) uint8_t[49152]);
    if (!ram) return fail(error, capacity, "Cannot allocate snapshot staging RAM.");
    z80_t cpu = {};
    z80_init(&cpu);
    uint16_t pc;
    uint8_t border;
    if (sna)
    {
        if (size != 49179) return fail(error, capacity, "Only 48K SNA snapshots are supported.");
        const uint16_t sp = word(data + 23);
        if (sp < 0x4000 || sp > 0xfffe || data[25] > 2 || data[26] > 7)
            return fail(error, capacity, "Invalid 48K SNA registers or stack.");
        memcpy(ram.get(), data + 27, 49152);
        pc = word(ram.get() + sp - 0x4000);
        cpu.i = data[0]; cpu.hl2 = word(data + 1); cpu.de2 = word(data + 3);
        cpu.bc2 = word(data + 5); cpu.af2 = word(data + 7);
        cpu.hl = word(data + 9); cpu.de = word(data + 11); cpu.bc = word(data + 13);
        cpu.iy = word(data + 15); cpu.ix = word(data + 17);
        cpu.iff1 = cpu.iff2 = (data[19] & 4) != 0;
        cpu.r = data[20]; cpu.af = word(data + 21); cpu.sp = sp + 2; cpu.im = data[25];
        border = data[26];
    }
    else
    {
        if (size < 30 || (data[29] & 3) > 2)
            return fail(error, capacity, "Invalid Z80 snapshot header.");
        const uint8_t flags = data[12] == 255 ? 1 : data[12];
        cpu.a = data[0]; cpu.f = data[1]; cpu.bc = word(data + 2); cpu.hl = word(data + 4);
        pc = word(data + 6); cpu.sp = word(data + 8); cpu.i = data[10];
        cpu.r = (data[11] & 127) | ((flags & 1) << 7);
        cpu.de = word(data + 13); cpu.bc2 = word(data + 15);
        cpu.de2 = word(data + 17); cpu.hl2 = word(data + 19);
        cpu.af2 = (unsigned(data[21]) << 8) | data[22];
        cpu.iy = word(data + 23); cpu.ix = word(data + 25);
        cpu.iff1 = data[27] != 0; cpu.iff2 = data[28] != 0; cpu.im = data[29] & 3;
        border = (flags >> 1) & 7;
        if (pc)
        {
            if (!(flags & 32))
            {
                if (size != 49182) return fail(error, capacity, "Invalid uncompressed Z80 v1 size.");
                memcpy(ram.get(), data + 30, 49152);
            }
            else
            {
                size_t length = size - 30;
                if (length >= 4 && !memcmp(data + size - 4, "\x00\xed\xed\x00", 4)) length -= 4;
                if (!unpack(data + 30, length, ram.get(), 49152))
                    return fail(error, capacity, "Invalid compressed Z80 v1 RAM.");
            }
        }
        else
        {
            if (size < 32) return fail(error, capacity, "Missing extended Z80 header.");
            const unsigned extra = word(data + 30);
            if ((extra != 23 && extra != 54 && extra != 55) || size < 32 + extra ||
                data[34] != 0 || data[36] != 0 || (data[37] & 128))
                return fail(error, capacity, "Only plain 48K Z80 v2/v3 snapshots are supported.");
            pc = word(data + 32);
            unsigned seen = 0;
            for (size_t pos = 32 + extra; pos < size;)
            {
                if (size - pos < 3) return fail(error, capacity, "Truncated Z80 page header.");
                const unsigned packed = word(data + pos), page = data[pos + 2];
                const unsigned index = page == 8 ? 0 : page == 4 ? 1 : page == 5 ? 2 : 3;
                if (index == 3 || (seen & (1u << index)))
                    return fail(error, capacity, "Unsupported or duplicate Z80 RAM page.");
                pos += 3;
                const size_t length = packed == 65535 ? 16384 : packed;
                if (length > size - pos) return fail(error, capacity, "Truncated Z80 RAM page.");
                uint8_t *destination = ram.get() + index * 16384;
                if (packed == 65535) memcpy(destination, data + pos, length);
                else if (!unpack(data + pos, length, destination, 16384))
                    return fail(error, capacity, "Invalid compressed Z80 RAM page.");
                seen |= 1u << index;
                pos += length;
            }
            if (seen != 7) return fail(error, capacity, "Z80 snapshot is missing RAM pages.");
        }
    }
    memcpy(core.memory + 16384, ram.get(), 49152);
    core.cpu = cpu;
    core.border = border;
    core.frames = 0;
    core.tape.play(false);
    core.restoreCpu(pc);
    return true;
}
