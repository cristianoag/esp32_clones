#include "MapperDatabase.h"
#include "MSX.h"
#include "romdb.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <vector>

int main(int argc,char **argv)
{
    static_assert(ROMDB_COUNT == 3115, "Review the database snapshot and tests when updating it.");
    unsigned unsupported = 0;
    for (unsigned i = 0; i < ROMDB_COUNT; ++i)
    {
        if (i) assert(memcmp(romdb_sha1s[i-1], romdb_sha1s[i], 20) < 0);
        int expected;
        switch (romdb_mappers[i])
        {
        case 3: expected = MAP_KONAMI5; break;
        case 5: expected = MAP_ASCII8; break;
        case 6: expected = MAP_ASCII16; break;
        case 7: expected = MAP_KONAMI4; break;
        case 12: expected = -12; ++unsupported; break;
        case 14: expected = -14; ++unsupported; break;
        default: assert(false); return 1;
        }
        assert(FmsxMapperFromSha1(romdb_sha1s[i]) == expected);
    }
    const unsigned char tinyMagic[] = {0xb9,0x66,0x4e,0x60,0x94,0xd8,0x48,0x8e,0x8e,0x0b,
                                      0x81,0x03,0xb1,0xbe,0x54,0x43,0xd8,0xda,0x0c,0xd0};
    assert(FmsxMapperFromSha1(tinyMagic) == MAP_KONAMI5);
    unsigned char missing[20] = {};
    assert(FmsxMapperFromSha1(missing) == -1);
    memset(missing,255,sizeof(missing));
    assert(FmsxMapperFromSha1(missing) == -1);
    for (const char *signature : {"ROM_NEO8","ROM_NE16","ASCII16X"})
    {
        std::vector<unsigned char> image(131072);
        image[0] = 'A'; image[1] = 'B';
        memcpy(image.data()+16,signature,8);
        const int expected = !strcmp(signature,"ROM_NEO8") ? -8 : !strcmp(signature,"ROM_NE16") ? -9 : -12;
        assert(FmsxKnownMapper(image.data(),image.size()) == expected);
        image[23] ^= 1;
        assert(FmsxKnownMapper(image.data(),image.size()) == -1);
    }
    std::vector<unsigned char> image(524288);
    image[0] = 'A'; image[1] = 'B';
    memcpy(image.data()+0x28000,"Manbow 2",8);
    assert(FmsxKnownMapper(image.data(),image.size()) == -14);
    assert(FmsxKnownMapper(image.data(),65536) == -1);
    for (unsigned size : {0U,1U,2U,16U,23U}) assert(FmsxKnownMapper(image.data(),size) == -1);
    if (argc == 2)
    {
        FILE *file = fopen(argv[1],"rb");
        assert(file && !fseek(file,0,SEEK_END));
        const long size = ftell(file);
        assert(size == 524288);
        rewind(file);
        assert(fread(image.data(),1,image.size(),file) == image.size());
        assert(!fclose(file));
        assert(FmsxKnownMapper(image.data(),image.size()) == MAP_KONAMI5);
        image.back() ^= 1;
        assert(FmsxKnownMapper(image.data(),image.size()) == -1); // Hash includes the final bank.
        puts("PASS: user Tiny Magic ROM identified as Konami SCC using its complete SHA-1.");
    }
    printf("PASS: %u sorted mapper records, %u explicit unsupported records, ID translation, misses and signatures.\n",
           ROMDB_COUNT,unsupported);
}
