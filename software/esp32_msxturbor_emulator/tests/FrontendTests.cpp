#include "A1stVideo.h"
#include "A1stSettings.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void require(bool condition, const char* message)
{
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
int main()
{
    require(A1stVgaColor(0xffff) == 255, "White RGB565 maps to white VGA.");
    require(A1stVgaColor(0xf800) == 7, "Red VGA channel ordering.");
    require(A1stVgaColor(0x07e0) == 0x38, "Green VGA channel ordering.");
    require(A1stVgaColor(0x001f) == 0xc0, "Blue VGA channel ordering.");
    std::array<uint16_t, 544> source;
    std::array<uint8_t, 322> output;
    for (unsigned width : {272U, 544U}) {
        const unsigned scale = width / 272;
        source.fill(0xffff);
        for (unsigned x = 8 * scale; x < 264 * scale; ++x) source[x] = 0xf800;
        output.fill(0x55);
        A1stScaleVideoRow(output.data() + 1, source.data(), width);
        require(output.front() == 0x55 && output.back() == 0x55, "Video output bounds.");
        for (unsigned x = 1; x <= 320; ++x) require(output[x] == 7, "Exact 256/512 to 320 crop.");
    }
    A1stSettings defaults, decoded;
    require(A1stDecodeSettings(&defaults, sizeof(defaults), decoded), "Default settings round trip.");
    auto invalid = defaults;
    invalid.drawEvery = 0;
    require(!A1stDecodeSettings(&invalid, sizeof(invalid), decoded), "Reject zero draw interval.");
    invalid = defaults;
    std::memset(invalid.disk, 'x', sizeof(invalid.disk));
    require(!A1stDecodeSettings(&invalid, sizeof(invalid), decoded), "Reject unterminated paths.");
    invalid = defaults;
    std::strcpy(invalid.disk, "/../bad.dsk");
    require(!A1stDecodeSettings(&invalid, sizeof(invalid), decoded), "Reject path traversal.");
    std::puts("A1ST video geometry, color and boot-settings tests passed.");
}
