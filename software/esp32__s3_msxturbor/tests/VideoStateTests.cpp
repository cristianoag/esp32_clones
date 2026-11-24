#include "A1stCore.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

static void require(bool value, const char* message)
{
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

static void reg(unsigned index, unsigned value)
{
    a1stWritePort(0x99, value);
    a1stWritePort(0x99, 0x80 | index);
}

static void vram(unsigned address, const std::vector<uint8_t>& bytes)
{
    reg(14, address >> 14);
    a1stWritePort(0x99, address & 255);
    a1stWritePort(0x99, 0x40 | ((address >> 8) & 63));
    for (uint8_t byte : bytes) a1stWritePort(0x98, byte);
}

static uint8_t status(unsigned index)
{
    reg(15, index);
    return a1stReadPort(0x99);
}

static std::vector<uint8_t> spriteStatus(bool render, uint64_t& elapsed)
{
    status(0);
    A1stStats start, end;
    a1stGetStats(&start);
    a1stRunFrameWithVideo(render);
    a1stRunFrameWithVideo(render);
    a1stGetStats(&end);
    require(end.frames - start.frames == 2, "Skipped pixels still execute every guest frame.");
    elapsed = end.masterTicks - start.masterTicks;
    std::vector<uint8_t> result;
    result.push_back(status(0));
    for (unsigned index = 3; index <= 6; ++index) result.push_back(status(index));
    return result;
}

void a1stVideoStateTests()
{
    a1stWritePort(0xa8, 0xff);
    a1stWriteMemory(0xffff, 0);
    for (unsigned page = 0; page < 4; ++page) a1stWritePort(0xfc + page, page);
    for (unsigned address = 0; address < 0xffff; ++address) a1stWriteMemory(address, 0x76);
    require(a1stReadMemory(0) == 0x76, "Map a harmless HALT fixture for VDP-only integration tests.");
    const unsigned modes[] = {1, 2, 3, 4, 5, 6, 7, 8, 10, 12};
    for (unsigned mode : modes) {
        reg(0, 0); reg(1, 0x50); reg(25, 0);
        a1stRunFrame();
        const unsigned r0 = mode == 2 ? 2 : mode == 4 ? 4 : mode == 5 ? 6 :
                            mode == 6 ? 8 : mode == 7 ? 10 : mode >= 8 ? 14 : 0;
        reg(0, r0); reg(1, mode == 3 ? 0x48 : 0x40);
        reg(2, 6); reg(3, 0x80); reg(4, 0); reg(5, 0x3f); reg(6, 7);
        reg(7, 0xf1); reg(8, 0); reg(9, 0); reg(23, 0);
        reg(25, mode == 10 ? 0x18 : mode == 12 ? 8 : 0);
        reg(26, 0); reg(27, 0);
        a1stRunFrame();
        const unsigned count = mode <= 3 ? 5 : 9;
        std::vector<uint8_t> attributes(128, 0);
        for (unsigned sprite = 0; sprite < count; ++sprite) {
            attributes[sprite * 4] = 20;
            attributes[sprite * 4 + 1] = 40;
            attributes[sprite * 4 + 3] = 15;
        }
        attributes[count * 4] = mode <= 3 ? 208 : 216;
        vram(mode <= 3 ? 0x1f80 : 0x1e00, attributes);
        vram(0x1c00, std::vector<uint8_t>(512, 15));
        vram(0x3800, std::vector<uint8_t>(8, 255));
        a1stRunFrame(); a1stRunFrame();
        uint64_t renderedTicks, skippedTicks;
        const auto rendered = spriteStatus(true, renderedTicks);
        if ((rendered[0] & 0xe0) != 0xe0)
            std::fprintf(stderr, "VDP mode %u: sprite flags=%02x\n", mode, rendered[0]);
        require((rendered[0] & 0xe0) == 0xe0, "Rendered fixture produces VBlank, sprite collision and overflow.");
        reg(7, 0xf4);
        std::vector<uint16_t> pixels(a1stPixels(), a1stPixels() + A1ST_FRAME_WIDTH * A1ST_FRAME_HEIGHT);
        const auto skipped = spriteStatus(false, skippedTicks);
        require(skipped == rendered, "Skipped frames preserve sprite flags and collision coordinates in every graphics mode.");
        const uint64_t difference = skippedTicks > renderedTicks ? skippedTicks - renderedTicks : renderedTicks - skippedTicks;
        if (difference > 60)
            std::fprintf(stderr, "VDP mode %u: rendered=%llu ticks skipped=%llu ticks\n", mode,
                         static_cast<unsigned long long>(renderedTicks),
                         static_cast<unsigned long long>(skippedTicks));
        // Each endpoint is sampled after a HALT/M1 fetch, which can overshoot by 30 master ticks.
        require(difference <= 60, "Rendering policy preserves guest timing within the CPU fetch-boundary quantization.");
        require(std::equal(pixels.begin(), pixels.end(), a1stPixels()), "Skipped frames leave the pixel buffer intact.");
        a1stRunFrame();
        require(!std::equal(pixels.begin(), pixels.end(), a1stPixels()), "Rendering resumes with updated guest colors.");
    }
    reg(0, 6); reg(1, 0x40); reg(25, 0);
    a1stRunFrame();
    vram(0, std::vector<uint8_t>(4, 0));
    reg(32, 0); reg(33, 0); reg(34, 0); reg(35, 0);
    reg(36, 0); reg(37, 0); reg(38, 0); reg(39, 0);
    reg(40, 8); reg(41, 0); reg(42, 1); reg(43, 0);
    reg(44, 5); reg(45, 0); reg(46, 0x80);
    a1stRunFrameWithVideo(false);
    reg(14, 0); a1stWritePort(0x99, 0); a1stWritePort(0x99, 0);
    for (unsigned byte = 0; byte < 4; ++byte) {
        const uint8_t value = a1stReadPort(0x98);
        if (value != 0x55) std::fprintf(stderr, "VDP fill byte %u = %02x\n", byte, value);
        require(value == 0x55, "VDP fill commands continue while pixel drawing is skipped.");
    }
    reg(15, 2);
    require(!(a1stReadPort(0x99) & 1), "Skipped rendering does not leave VDP commands busy.");
    std::puts("A1ST pixel skipping preserves sprite status/coordinates, guest clocks and VDP command progress.");
}
