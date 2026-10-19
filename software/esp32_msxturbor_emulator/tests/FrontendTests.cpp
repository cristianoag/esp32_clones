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
        for (unsigned fit : {80U, 85U, 90U, 95U, 100U}) {
            const A1stViewport viewport(fit);
            require(viewport.left * 2 + viewport.width == 320 &&
                    viewport.top * 2 + viewport.height == 240,
                    "VGA safe area stays centered at every supported fit.");
            require(viewport.width * 3 == viewport.height * 4, "Preserve the 4:3 picture aspect.");
            require(viewport.sourceRow(viewport.top) == 0 &&
                    viewport.sourceRow(viewport.top + viewport.height - 1) == 239,
                    "Both top and bottom source rows remain visible.");
            for (unsigned y = 0; y < 240; ++y)
                require(viewport.containsRow(y) ?
                    viewport.sourceRow(y) < 240 : y < viewport.top || y >= viewport.top + viewport.height,
                    "Menu and guest share the same bounded vertical viewport.");
            source[8 * scale] = 0x07e0;
            source[264 * scale - 1] = 0x001f;
            output.fill(0x55);
            A1stScaleVideoRow(output.data() + 1, source.data(), width, fit);
            require(output.front() == 0x55 && output.back() == 0x55, "Fitted video bounds.");
            require(output[1 + viewport.left] == 0x38 &&
                    output[viewport.left + viewport.width] == 0xc0, "Both guest picture edges remain visible.");
            for (unsigned x = 0; x < viewport.left; ++x)
                require(output[1 + x] == 0 && output[320 - x] == 0, "Black VGA side margins.");
            std::array<uint8_t, 320> menu;
            menu.fill(0xff);
            menu.front() = 7;
            menu.back() = 0x38;
            A1stFitMenuRow(output.data() + 1, menu.data(), viewport);
            require(output.front() == 0x55 && output.back() == 0x55, "Fitted menu bounds.");
            require(output[1 + viewport.left] == 7 &&
                    output[viewport.left + viewport.width] == 0x38, "Both menu edges remain visible.");
        }
    }
    A1stSettings defaults, decoded;
    require(A1stDecodeSettings(&defaults, sizeof(defaults), decoded), "Default settings round trip.");
    auto invalid = defaults;
    invalid.drawEvery = 0;
    require(!A1stDecodeSettings(&invalid, sizeof(invalid), decoded), "Reject zero draw interval.");
    invalid = defaults;
    std::memset(invalid.disks[0], 'x', sizeof(invalid.disks[0]));
    require(!A1stDecodeSettings(&invalid, sizeof(invalid), decoded), "Reject unterminated paths.");
    invalid = defaults;
    std::strcpy(invalid.disks[0], "/../bad.dsk");
    require(!A1stDecodeSettings(&invalid, sizeof(invalid), decoded), "Reject path traversal.");
    A1stSettingsV1 legacy;
    std::strcpy(legacy.disk, "/msxturbor/disks/boot.dsk");
    legacy.sound = 0;
    legacy.drawEvery = 5;
    require(A1stDecodeSettings(&legacy, sizeof(legacy), decoded), "Migrate existing A1ST NVS settings.");
    require(decoded.version == 2 && decoded.sound == 0 && decoded.drawEvery == 5 &&
            !std::strcmp(decoded.disks[0], legacy.disk) && decoded.vgaFit == 90 &&
            !decoded.disks[1][0] && !decoded.cartridges[0][0],
            "Migration retains boot preferences and starts with empty added slots.");
    auto full = defaults;
    std::strcpy(full.cartridges[0], "/games/slot1.ROM");
    std::strcpy(full.cartridges[1], "/games/slot2.rom");
    std::strcpy(full.disks[0], "/disks/a.dsk");
    std::strcpy(full.disks[1], "/disks/b.DSK");
    full.mappers[0] = MsxKonamiScc;
    full.mappers[1] = MsxAscii16;
    full.vgaFit = 85;
    require(A1stDecodeSettings(&full, sizeof(full), decoded) && !std::memcmp(&full, &decoded, sizeof(full)),
            "Both slots, mappers, drives and VGA fit round-trip.");
    invalid = full;
    invalid.mappers[1] = 255;
    require(!A1stDecodeSettings(&invalid, sizeof(invalid), decoded), "Reject invalid mapper.");
    invalid = full;
    invalid.vgaFit = 89;
    require(!A1stDecodeSettings(&invalid, sizeof(invalid), decoded), "Reject invalid VGA fit.");
    invalid = full;
    std::strcpy(invalid.cartridges[1], "/games/not-a-rom.dsk");
    require(!A1stDecodeSettings(&invalid, sizeof(invalid), decoded), "Reject wrong cartridge extension.");
    legacy.version = 2;
    require(!A1stDecodeSettings(&legacy, sizeof(legacy), decoded), "Reject version/size mismatch.");
    std::puts("A1ST video geometry, color and boot-settings tests passed.");
}
