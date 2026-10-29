#include "Cp400DiskActivity.h"
#include "Mode.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <vector>

static uint32_t nowMs;
uint32_t millis() { return nowMs; }
void vTaskDelay(unsigned ms) { nowMs += ms; }

struct TestVga
{
    struct { unsigned hRes, vRes; } mode;
    std::vector<uint8_t> buffers[2];
    unsigned backBuffer = 0, frontBuffer = 1;
    void reset(const Mode &selected, uint8_t border)
    {
        mode = {selected.hRes, selected.vRes};
        backBuffer = 0;
        frontBuffer = 1;
        for (auto &buffer : buffers) buffer.assign(mode.hRes * mode.vRes, border);
    }
    size_t position(unsigned x, unsigned y) const
    {
        assert(x < mode.hRes && y < mode.vRes);
        return y * mode.hRes + x;
    }
    void dot(unsigned x, unsigned y, uint8_t color) { buffers[backBuffer][position(x, y)] = color; }
    uint8_t getPixel(unsigned x, unsigned y) const { return buffers[backBuffer][position(x, y)]; }
    void show() { frontBuffer = backBuffer; backBuffer ^= 1; }
} video;
TestVga *vga = &video;

struct { uint8_t Name_Disk[4][256]; uint8_t LastNumber_Accessed; bool isFileAlreadyOpen; } Disk_Drive;
struct {
    uint8_t DIRECT_Key_Code, CP400GraphicMode;
    bool CPU_HALTED_BY_EMULATOR, PHYSICAL_Drive_Must_Be_Saved, nmi_pin, Artefact;
} sf;
constexpr size_t DISK_SIZE = 161280;
static uint8_t RAM_Disk0[DISK_SIZE], RAM_Disk1[DISK_SIZE], RAM_Disk2[DISK_SIZE], RAM_Disk3[DISK_SIZE];
static uint8_t rom[32769];
enum { M_FF40 = 0xff40, M_FF48 = 0xff48, M_FF49, M_FF4A, M_FF4B };
enum { ROM_FF40 = M_FF40 - 0x8000, ROM_FF48 = M_FF48 - 0x8000, ROM_FF49, ROM_FF4A, ROM_FF4B };
enum { MENU_F12 = 69 };
void DISKETTE_LED_ON() {}
void DISKETTE_LED_OFF() {}
bool saveSucceeds = true;
bool SaveConfigToSD() { return saveSucceeds; }
bool WriteCP400DiskImage(const char *, uint8_t) { return true; }

#include "DiskState.inc"
#include "DiskBytes.inc"
#include "DriveNumber.inc"
#include "DiskCommands.inc"
#include "DiskEject.inc"
void readRegister(uint16_t address)
{
    switch (address) {
#include "DiskRead.inc"
    default: break;
    }
}
void writeRegister(uint16_t address, uint8_t value)
{
    switch (address) {
#include "DiskWrite.inc"
    default: break;
    }
}

#include "ActivityVideo.inc"
#include "VideoModes.inc"

bool CPU_in_WAIT_STATE = false;
unsigned menus, frames;
void EMULATOR_Menu()
{
    assert(CPU_in_WAIT_STATE && sf.CPU_HALTED_BY_EMULATOR);
    for (unsigned drive = 0; drive < 4; ++drive) assert(!DiskActivity.active(drive, nowMs));
    for (const auto &buffer : vga->buffers)
        for (uint8_t pixel : buffer) assert(pixel == 0);
    ++menus;
    nowMs += 1000;
}
void RenderCP400GraphMode_32X16_8X12() { ++frames; }
void RenderCP400GraphMode_256X192X2_Artefact() { ++frames; }
void RenderCP400GraphMode_256X192X2() { ++frames; }
void RenderCP400GraphMode_128X192X4() { ++frames; }
void RenderCP400GraphMode_128X192X2() { ++frames; }
void RenderCP400GraphMode_128X96X4() { ++frames; }
void RenderCP400GraphMode_128X96X2() { ++frames; }
void RenderCP400GraphMode_128X64X4() { ++frames; }
void RenderCP400GraphMode_128X64X2() { ++frames; }
void RenderCP400GraphMode_64X64X4() { ++frames; }
void Field_Synch_Interrupt_Flag() {}
struct EndFrame {};
void FillKeyboardMatrix() { throw EndFrame(); }
#define WAIT_VSYNCH_LOW() ((void)0)
#define IRAM_ATTR
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "VideoCore.inc"
#pragma GCC diagnostic pop

static void runFrame()
{
    try { VideoCore(nullptr); }
    catch (const EndFrame &) { return; }
    assert(false);
}

static void timingTests()
{
    Cp400DiskActivity::State state;
    for (unsigned drive = 0; drive < 4; ++drive) assert(!state.active(drive, 0));
    state.record(0, 0);
    state.record(3, 100);
    assert(state.active(0, 199) && !state.active(0, 200));
    assert(state.active(3, 299) && !state.active(3, 300));
    state.record(3, 250);
    assert(state.active(3, 449) && !state.active(3, 450));
    state.clear(3);
    assert(!state.active(3, 250));
    state.record(1, UINT32_MAX - 100);
    assert(state.active(1, 98) && !state.active(1, 99));
    state.clear();
    for (unsigned drive = 0; drive < 4; ++drive) assert(!state.active(drive, 0));
}

static void controllerTests()
{
    const uint8_t select[4] = {1, 2, 4, 64};
    uint8_t *ram[] = {RAM_Disk0, RAM_Disk1, RAM_Disk2, RAM_Disk3};
    for (unsigned drive = 0; drive < 4; ++drive)
    {
        strcpy(reinterpret_cast<char *>(Disk_Drive.Name_Disk[drive]), "/disk.dsk");
        memset(ram[drive], 0x40 + drive, DISK_SIZE);
    }
    for (unsigned drive = 0; drive < 4; ++drive)
    {
        DiskActivity.clear();
        DiskAccess = {};
        nowMs = 100;
        writeRegister(M_FF40, select[drive]);
        writeRegister(M_FF49, 0);
        writeRegister(M_FF4A, 1);
        readRegister(M_FF48);
        readRegister(M_FF4B);
        assert(!DiskActivity.active(drive, nowMs));
        writeRegister(M_FF48, DSKCMD_READ_SECTOR);
        readRegister(M_FF48);
        assert(!DiskActivity.active(drive, nowMs));
        readRegister(M_FF4B);
        assert(rom[ROM_FF4B] == 0x40 + drive && DiskActivity.active(drive, nowMs));
        nowMs += 199;
        readRegister(M_FF48);
        assert(DiskActivity.active(drive, nowMs));
        ++nowMs;
        readRegister(M_FF48);
        assert(!DiskActivity.active(drive, nowMs));

        DiskAccess.IsinReadProcess = false;
        DiskAccess.DriveSelected = (drive + 1) % 4;
        writeRegister(M_FF48, DSKCMD_WRITE_SECTOR);
        assert(DiskAccess.DriveSelected == drive && !DiskActivity.active(drive, nowMs));
        writeRegister(M_FF4B, 0x80 + drive);
        assert(ram[drive][0] == 0x80 + drive);
        for (unsigned other = 0; other < 4; ++other)
            assert(DiskActivity.active(other, nowMs) == (other == drive));

        saveSucceeds = false;
        assert(!EjectCP400Disk(drive) && DiskActivity.active(drive, nowMs));
        saveSucceeds = true;
        assert(EjectCP400Disk(drive) && !DiskActivity.active(drive, nowMs));
        assert(ram[drive][0] == 0xff && !Disk_Drive.Name_Disk[drive][0]);
        ReadDiskByte(0);
        WriteDiskByte(0, 0x11);
        assert(!DiskActivity.active(drive, nowMs));
    }
    puts("PASS: drives 0-3 read/write activity, idle/status filtering, selection and ejection.");
}

static void renderingTests(const Mode &mode, uint8_t border)
{
    vga->reset(mode, border);
    const unsigned left = mode.hRes - 64 - 8, top = mode.vRes - 12 - 6;
    assert(top >= (mode.vRes - 192) / 2 + 192);
    for (unsigned drive = 0; drive < 4; ++drive) Disk_Drive.Name_Disk[drive][0] = '/';
    for (unsigned mask = 0; mask < 16; ++mask)
    {
        DiskActivity.clear();
        nowMs = 0;
        for (unsigned drive = 0; drive < 4; ++drive)
            if (mask & (1u << drive)) DiskActivity.record(drive, nowMs);
        for (uint32_t time : {0u, 128u, 200u})
        {
            nowMs = time;
            for (unsigned buffer = 0; buffer < 2; ++buffer)
            {
                vga->backBuffer = buffer;
                DrawDiskActivity();
                for (unsigned y = 0; y < mode.vRes; ++y)
                    for (unsigned x = 0; x < mode.hRes; ++x)
                    {
                        uint8_t color = border;
                        if (x >= left && x < left + 64 && y >= top && y < top + 12)
                        {
                            const uint8_t icon = Cp400DiskActivity::pixel(
                                x - left, y - top, time < 200 ? mask : 0, time == 0);
                            if (icon) color = icon;
                        }
                        assert(vga->getPixel(x, y) == color);
                    }
            }
        }
    }
    assert(!Cp400DiskActivity::pixel(64, 0, 15, true));
    assert(!Cp400DiskActivity::pixel(0, 12, 15, true));
    for (unsigned drive = 0; drive < 4; ++drive)
    {
        assert(Cp400DiskActivity::pixel(drive * 16 + 1, 4, 1u << drive, true) == 0x38);
        assert(Cp400DiskActivity::pixel(drive * 16 + 1, 4, 1u << drive, false) == 0x18);
        for (unsigned x = 12; x < 16; ++x)
            assert(!Cp400DiskActivity::pixel(drive * 16 + x, 4, 15, true));
    }
    printf("PASS: %ux%u border %u, four pulsing icons, 200 ms expiry, picture untouched in both buffers.\n",
           mode.hRes, mode.vRes, border);
}

static void videoCoreTests()
{
    vga->reset(Mode::MODE_320x240x60_4_3, 0);
    DiskActivity.clear();
    DiskAccess = {};
    sf = {};
    nowMs = 0;
    Disk_Drive.Name_Disk[0][0] = '/';
    DiskActivity.record(0, nowMs);
    for (uint8_t mode : {CP400_GRAPH_MODE_32X16_8X12, CP400_GRAPH_MODE_256X192X2,
                        CP400_GRAPH_MODE_128X192X4, CP400_GRAPH_MODE_128X192X2,
                        CP400_GRAPH_MODE_128X96X4, CP400_GRAPH_MODE_128X64X4,
                        CP400_GRAPH_MODE_128X64X2, CP400_GRAPH_MODE_64X64X4})
    {
        sf.CP400GraphicMode = mode;
        runFrame();
        const unsigned left = vga->mode.hRes - 64 - 8, top = vga->mode.vRes - 12 - 6;
        assert(vga->buffers[vga->frontBuffer][vga->position(left + 1, top + 4)] == 0x38);
    }
    assert(frames == 8);
    sf.DIRECT_Key_Code = MENU_F12;
    runFrame();
    assert(menus == 1 && !CPU_in_WAIT_STATE && !sf.CPU_HALTED_BY_EMULATOR);
    sf.DIRECT_Key_Code = 0;
    runFrame();
    for (const auto &buffer : vga->buffers)
        for (uint8_t pixel : buffer) assert(pixel == 0);
    puts("PASS: production video loop presents icons in every mode and clears both buffers before F12.");
}

int main()
{
    timingTests();
    controllerTests();
    for (const Mode &mode : {Mode::MODE_320x240x60, Mode::MODE_320x240x60_4_3,
                            Mode::MODE_640x240x60, Mode::MODE_640x240x60_4_3})
        for (uint8_t border : {0, 255}) renderingTests(mode, border);
    videoCoreTests();
}
