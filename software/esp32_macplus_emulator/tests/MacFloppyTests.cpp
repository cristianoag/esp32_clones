#include "MacFloppy.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <vector>
extern "C" {
#include "tme/iwm.h"
#include "tme/musashi/m68k.h"
}

static void selectSense(unsigned reg)
{
    iwmRead(13);
    iwmRead(14);
    iwmSetHeadSel(reg & 8);
    for (unsigned bit = 0; bit < 3; ++bit) iwmRead(bit * 2 + ((reg >> bit) & 1));
}

static bool sense(unsigned reg)
{
    selectSense(reg);
    return iwmRead(13) & 0x80;
}

static void command(unsigned reg)
{
    iwmRead(6);
    selectSense(reg);
    iwmRead(7);
}

static unsigned decode(uint8_t value)
{
    static const uint8_t codes[64] = {
        0x96,0x97,0x9a,0x9b,0x9d,0x9e,0x9f,0xa6,
        0xa7,0xab,0xac,0xad,0xae,0xaf,0xb2,0xb3,
        0xb4,0xb5,0xb6,0xb7,0xb9,0xba,0xbb,0xbc,
        0xbd,0xbe,0xbf,0xcb,0xcd,0xce,0xcf,0xd3,
        0xd6,0xd7,0xd9,0xda,0xdb,0xdc,0xdd,0xde,
        0xdf,0xe5,0xe6,0xe7,0xe9,0xea,0xeb,0xec,
        0xed,0xee,0xef,0xf2,0xf3,0xf4,0xf5,0xf6,
        0xf7,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff
    };
    for (unsigned i = 0; i < 64; ++i) if (codes[i] == value) return i;
    assert(false);
    return 0;
}

static void decodeThree(const uint8_t *input, unsigned count, uint8_t output[3])
{
    const unsigned high = decode(input[0]);
    output[0] = ((high & 0x30) << 2) | decode(input[1]);
    output[1] = ((high & 0x0c) << 4) | decode(input[2]);
    output[2] = count == 4 ? ((high & 3) << 6) | decode(input[3]) : 0;
}

static void verifyTrack(const std::vector<uint8_t> &image, unsigned track, unsigned head)
{
    const unsigned heads = image.size() == 800 * 1024 ? 2 : 1;
    const unsigned sectors = 12 - track / 16;
    unsigned firstBlock = 0;
    for (unsigned t = 0; t < track; ++t) firstBlock += (12 - t / 16) * heads;
    firstBlock += head * sectors;
    iwmSetHeadSel(head);
    iwmRead(14);
    iwmRead(12);
    std::vector<uint8_t> stream(sectors * 800 * 2);
    for (auto &byte : stream) byte = iwmRead(12);
    assert(iwmTakeActivity() && !iwmTakeActivity());
    unsigned seen = 0;
    for (unsigned pos = 0; pos + 760 < stream.size(); ++pos) {
        const uint8_t *p = stream.data() + pos;
        if (p[0] != 0xd5 || p[1] != 0xaa || p[2] != 0x96) continue;
        const unsigned sector = decode(p[4]);
        const unsigned side = (track >> 6) | (head << 5);
        const unsigned format = heads == 2 ? 0x22 : 0x02;
        assert(decode(p[3]) == (track & 63));
        assert(sector < sectors && decode(p[5]) == side && decode(p[6]) == format);
        assert(decode(p[7]) == ((track ^ sector ^ side ^ format) & 63));
        assert(p[8] == 0xde && p[9] == 0xaa);
        p += 16;
        assert(p[0] == 0xd5 && p[1] == 0xaa && p[2] == 0xad);
        assert(decode(p[3]) == sector);
        p += 4;
        uint8_t raw[524], a = 0, b = 0, c = 0;
        for (unsigned i = 0; i < 175; ++i) {
            uint8_t values[3];
            const unsigned count = i == 174 ? 3 : 4;
            decodeThree(p, count, values);
            p += count;
            c = (c << 1) | (c >> 7);
            values[0] ^= c;
            const unsigned sumA = a + values[0] + (c & 1);
            a = sumA;
            values[1] ^= a;
            const unsigned sumB = b + values[1] + (sumA >> 8);
            b = sumB;
            if (i != 174) {
                values[2] ^= b;
                c += values[2] + (sumB >> 8);
            }
            raw[i * 3] = values[0];
            raw[i * 3 + 1] = values[1];
            if (i != 174) raw[i * 3 + 2] = values[2];
        }
        uint8_t checks[3];
        decodeThree(p, 4, checks);
        assert(checks[0] == a && checks[1] == b && checks[2] == c);
        for (unsigned i = 0; i < 12; ++i) assert(raw[i] == 0);
        assert(!memcmp(raw + 12, image.data() + (firstBlock + sector) * 512, 512));
        seen |= 1u << sector;
    }
    assert(seen == (1u << sectors) - 1);
}

void testFloppy(const char *path)
{
    iwmInit();
    assert(!iwmInserted());
    assert(!iwmInsert(nullptr, 800 * 1024));
    iwmRead(9);
    assert(!sense(7) && sense(8) && !sense(9));
    for (unsigned heads = 1; heads <= 2; ++heads) {
        std::vector<uint8_t> image(heads * 400 * 1024);
        for (unsigned i = 0; i < image.size(); ++i) image[i] = (i * 13) ^ (i >> 9);
        FILE *file = fopen(path, "wb");
        assert(file && fwrite(image.data(), 1, image.size(), file) == image.size());
        assert(fclose(file) == 0);
        MacFloppy floppy;
        char error[160] = {};
        assert(floppy.open(path, error, sizeof(error)));
        assert(floppy.inserted() && floppy.hasImage() && !sense(8) && !sense(9));
        assert(!floppy.open(path, error, sizeof(error)) && floppy.inserted());
        iwmWrite(15, 0x55);
        assert(floppy.inserted() && !iwmTakeActivity());
        command(4);
        for (unsigned i = 0; i < 80; ++i) command(1);
        assert(!sense(10));
        command(0);
        command(2);
        assert(!sense(2));
        assert(!iwmTakeActivity());
        selectSense(0);
        for (unsigned i = 0; i < 36; ++i) assert(m68k_read_memory_16(0xdff9fe) == 0xffff);
        assert(m68k_read_memory_16(0xdff9fe) == 0xffd5);
        assert(iwmTakeActivity());
        assert(m68k_read_memory_8(0xdff9fe) == 0xff && !iwmTakeActivity());
        iwmRead(11);
        assert(sense(7) && sense(8));
        iwmRead(10);
        assert(!sense(7));
        const bool tach = sense(11);
        iwmStep(10000);
        assert(sense(11) != tach);
        for (unsigned track = 0; track < 80; ++track) {
            for (unsigned head = 0; head < heads; ++head) verifyTrack(image, track, head);
            command(0);
            command(1);
        }
        command(7);
        assert(!floppy.inserted() && floppy.hasImage() && sense(8));
        MacFloppy replacement;
        assert(replacement.open(path, error, sizeof(error)));
        floppy.close();
        assert(replacement.inserted());
        replacement.close();
        assert(!floppy.hasImage() && !iwmTakeActivity());
        file = fopen(path, "rb");
        std::vector<uint8_t> check(image.size());
        assert(file && fread(check.data(), 1, check.size(), file) == check.size());
        assert(fclose(file) == 0 && check == image);
    }
    FILE *file = fopen(path, "wb");
    assert(file && fputc(0, file) == 0 && fclose(file) == 0);
    MacFloppy floppy;
    char error[160] = {};
    assert(!floppy.open(path, error, sizeof(error)) && error[0] && !floppy.hasImage());
    for (unsigned size : {400u * 1024 + 84, 800u * 1024 + 84, 1440u * 1024}) {
        file = fopen(path, "wb");
        assert(file && fseek(file, size - 1, SEEK_SET) == 0 && fputc(0, file) == 0 && fclose(file) == 0);
        assert(!floppy.open(path, error, sizeof(error)) && !floppy.inserted());
    }
    assert(remove(path) == 0);
}
