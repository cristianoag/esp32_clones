#include "MacInput.h"
#include "MacSettings.h"
#include "MacVideo.h"
#include "MacRom.h"
#include "MacScsi.h"
#include "../lib/MsxSoftUsb/upstream/mac_mouse.h"
#include <assert.h>
#include <vector>
#include <string>
extern "C" {
#include "tme/emu.h"
#include "tme/tmeconfig.h"
#include "tme/via.h"
#include "tme/rtc.h"
#include "tme/mouse.h"
#include "tme/musashi/m68k.h"
}

static unsigned reportedErrors = 0;
void macReportError(const char *) { ++reportedErrors; }
extern "C" void saveRtcMem(char *) {}

static void testInput()
{
    MacKeyState state;
    std::vector<unsigned> events;
    auto emit = [&](unsigned code, bool release) { events.push_back(code | (release ? 128 : 0)); };
    uint8_t report[8] = {0x22,0,4};
    state.update(report, emit);
    assert((events == std::vector<unsigned>{0x38,0}));
    events.clear();
    report[0] = 0x20;
    state.update(report, emit);
    assert(events.empty());
    memset(report, 0, sizeof(report));
    state.update(report, emit);
    assert((events == std::vector<unsigned>{128,0xb8}));
    events.clear();
    report[2] = 69;
    state.update(report, emit);
    assert(events.empty()); // F12 belongs only to the host menu.
    report[2] = 57;
    state.update(report, emit);
    report[2] = 0;
    state.update(report, emit);
    assert((events == std::vector<unsigned>{0x39}));
    report[2] = 57;
    state.update(report, emit);
    assert(events.back() == 0xb9);
    events.clear();
    report[2] = 4;
    state.update(report, emit);
    report[2] = 1;
    state.update(report, emit);
    assert((events == std::vector<unsigned>{0}));
    state.release(emit);
    assert(events.back() == 128);
    assert(MacScanCode(80) == 0x46 && MacScanCode(79) == 0x42);
    assert(MacScanCode(81) == 0x48 && MacScanCode(82) == 0x4d);

    MacMouseState mouse;
    const uint8_t first[] = {1,0xff,0x80,9};
    const uint8_t second[] = {0,3,5};
    assert(mouse.report(first, sizeof(first)));
    assert(mouse.report(second, sizeof(second)));
    assert(!mouse.report(first, 2));
    int x, y;
    bool down;
    mouse.take(x, y, down);
    assert(x == 2 && y == -123 && !down);
    mouse.take(x, y, down);
    assert(x == 0 && y == 0);
    mouse.report(first, sizeof(first));
    mouse.reset();
    mouse.take(x, y, down);
    assert(!x && !y && !down);
    const uint8_t descriptor[] = {9,4,3,0,1,3,1,2,0};
    assert(macIsBootMouseInterface(descriptor, sizeof(descriptor), 1));
    assert(!macIsBootMouseInterface(descriptor, sizeof(descriptor), 0));
    assert(!macIsBootMouseInterface(descriptor, 8, 1));
    uint8_t keyboard[9];
    memcpy(keyboard, descriptor, sizeof(keyboard));
    keyboard[7] = 1;
    assert(!macIsBootMouseInterface(keyboard, 9, 1));
}

static void testSettingsAndVideo()
{
    MacSettings settings, decoded;
    assert(MacDecodeSettings(&settings, sizeof(settings), decoded));
    settings.volume = 101;
    assert(!MacDecodeSettings(&settings, sizeof(settings), decoded));
    settings.volume = 50;
    memset(settings.rom, 'x', sizeof(settings.rom));
    assert(!MacDecodeSettings(&settings, sizeof(settings), decoded));
    assert(!MacValidPath("/macplus/../bad.rom"));
    assert(!MacValidPath("/bad\\name.rom"));
    std::vector<uint8_t> framebuffer(64 * 342, 0xff);
    framebuffer[0] = 0x7f;
    uint8_t line[640];
    MacRenderLine(framebuffer.data(), 69, line);
    assert(line[63] == 0 && line[64] == 255 && line[65] == 0 && line[576] == 0);
    MacRenderLine(framebuffer.data(), 68, line);
    for (uint8_t pixel : line) assert(pixel == 0);
    MacRenderLine(framebuffer.data(), 410, line);
    MacRenderLine(framebuffer.data(), 411, line);
    for (uint8_t pixel : line) assert(pixel == 0);
}

static void put16(std::vector<uint8_t> &rom, size_t offset, uint16_t value)
{
    rom[offset] = value >> 8;
    rom[offset + 1] = value;
}

static void put32(std::vector<uint8_t> &rom, size_t offset, uint32_t value)
{
    put16(rom, offset, value >> 16);
    put16(rom, offset + 2, value);
}

static void testCore()
{
    std::vector<uint8_t> rom(TME_ROMSIZE), ram(TME_RAMSIZE);
    put32(rom, 0, 0x003ff000);
    put32(rom, 4, 0x00400100);
    put16(rom, 0x100, 0x13fc); // MOVE.B #$12,$1000
    put16(rom, 0x102, 0x0012);
    put32(rom, 0x104, 0x1000);
    put16(rom, 0x108, 0x60fe); // BRA.S self
    tmeInit(rom.data(), ram.data());
    assert(m68k_read_memory_32(0) == 0x003ff000);
    m68k_write_memory_8(0x400100, 0);
    assert(rom[0x100] == 0x13);
    viaWrite(1, 0x6f);
    tmeRunFrame();
    assert(ram[0x1000] == 0x12);
    assert(m68k_get_reg(nullptr, M68K_REG_PC) == 0x400108);
    assert(tmeFramebuffer() == ram.data() + TME_SCREENBUF);
    viaWrite(1, 0x2f);
    assert(tmeFramebuffer() == ram.data() + TME_SCREENBUF_ALT);
    m68k_write_memory_32(0x1fffe, 0x11223344);
    assert(m68k_read_memory_32(0x1fffe) == 0x11223344);
    m68k_write_memory_8(0x1001000, 0x34);
    assert(ram[0x1000] == 0x34);
    viaWrite(1, 0x6f);
    viaWrite(0, 0);
    ram[TME_SNDBUF] = 255;
    ram[TME_SNDBUF + 738] = 0;
    int16_t samples[370];
    tmeAudio(samples, 100);
    assert(samples[0] == 32512 && samples[369] == -32768);
    tmeAudio(samples, 0);
    for (int16_t sample : samples) assert(sample == 0);
    kbdPushKey(0x46, 0);
    viaWrite(10, 0x12);
    assert(viaRead(10) == 0x79);
    viaWrite(10, 0x12);
    assert(viaRead(10) == 0x0d);
    kbdPushKey(0x46, 1);
    viaWrite(10, 0x10);
    assert(viaRead(10) == 0x79);
    viaWrite(10, 0x10);
    assert(viaRead(10) == 0x8d);
    viaWrite(10, 0x16);
    assert(viaRead(10) == 0x7d);
    mouseMove(5, -2, 1);
    assert(mouseTick() & MOUSE_BTN);
    tmeInit(rom.data(), ram.data());
    assert(!(mouseTick() & MOUSE_BTN));
    viaWrite(10, 0x10);
    assert(viaRead(10) == 0x7b);
    assert(!MacValidRom(rom.data(), rom.size()));
    uint32_t sum = 0;
    for (size_t i = 4; i < rom.size(); i += 2) sum += (uint32_t(rom[i]) << 8) | rom[i + 1];
    put32(rom, 0, sum);
    assert(MacValidRom(rom.data(), rom.size()));
}

static std::vector<uint8_t> scsi(const std::vector<uint8_t> &cmd, unsigned received,
                                 const uint8_t *write = nullptr, size_t size = 0)
{
    ncrWrite(2, 0, 1);
    ncrWrite(0, 0, 0xc0);
    ncrWrite(1, 0, 0x0d);
    ncrWrite(1, 0, 0x05);
    ncrWrite(1, 0, 0x01);
    ncrWrite(2, 0, 2);
    ncrWrite(3, 0, 2);
    for (uint8_t byte : cmd) ncrWrite(0, 1, byte);
    ncrWrite(3, 0, write ? 0 : received ? 1 : 3);
    if (write) {
        assert(!(ncrRead(5, 0) & 0x80));
        for (size_t i = 0; i < size; ++i) {
            ncrWrite(0, 1, write[i]);
            assert(bool(ncrRead(5, 0) & 0x80) == (i + 1 == size));
        }
    }
    std::vector<uint8_t> result;
    for (unsigned i = 0; i < received; ++i) result.push_back(ncrRead(0, 1));
    if (received || write) ncrWrite(3, 0, 3);
    result.push_back(ncrRead(0, 1));
    ncrWrite(3, 0, 7);
    return result;
}

static void testDisk(const char *path)
{
    std::vector<uint8_t> bytes(512 * 8);
    for (size_t i = 0; i < bytes.size(); ++i) bytes[i] = i;
    FILE *file = fopen(path, "wb");
    assert(file && fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size());
    assert(fclose(file) == 0);
    MacDisk disk;
    char error[160];
    assert(disk.open(path, false, error, sizeof(error)));
    uint8_t sector[512] = {};
    assert(disk.transfer(false, 7, 1, sector, sizeof(sector), error, sizeof(error)));
    assert(!disk.transfer(false, 8, 1, sector, sizeof(sector), error, sizeof(error)));
    assert(!disk.transfer(false, 0, UINT32_MAX, sector, sizeof(sector), error, sizeof(error)));
    ncrInit();
    MacAttachScsi(&disk);
    auto result = scsi({0x25,0,0,0,0,0,0,0,0,0}, 8);
    assert(result.size() == 9 && result[3] == 7 && result[6] == 2 && result[8] == 0);
    result = scsi({0x08,0,0,7,1,0}, 512);
    assert(result[0] == 0 && result[255] == 255 && result.back() == 0);
    memset(sector, 0x5a, sizeof(sector));
    result = scsi({0x0a,0,0,1,1,0}, 0, sector, sizeof(sector));
    assert(result.back() == 0);
    assert(disk.transfer(false, 1, 1, sector, sizeof(sector), error, sizeof(error)));
    assert(sector[0] == 0x5a && sector[511] == 0x5a);
    assert(disk.open(path, true, error, sizeof(error)));
    result = scsi({0x0a,0,0,1,1,0}, 0, sector, sizeof(sector));
    assert(result.back() == 2 && reportedErrors == 1);
    result = scsi({0x03,0,0,0,18,0}, 18);
    assert(result[2] == 7 && result.back() == 0);
    result = scsi({0x00,0,0,0,0,0}, 0);
    assert(result.back() == 0);
    disk.close();
    assert(remove(path) == 0);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    testInput();
    testSettingsAndVideo();
    testCore();
    testDisk(argv[1]);
    puts("Mac Plus CPU, bus, video, audio, keyboard, mouse, settings and SCSI tests passed.");
}
