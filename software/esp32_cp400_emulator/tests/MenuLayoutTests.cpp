#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <vector>
#include "Mode.h"
#include "CP400Roms.h"

#define FW_VERSION "1.16"

struct TestVga
{
    struct { uint32_t hRes, vRes; } mode;
    unsigned backBuffer = 0;
    size_t glyphLines = 0;
    std::vector<uint8_t> buffers[2];

    void reset(const Mode &selected)
    {
        mode = {selected.hRes, selected.vRes};
        backBuffer = 0;
        glyphLines = 0;
        for (auto &buffer : buffers) buffer.assign(mode.hRes * mode.vRes, 0);
    }

    size_t position(int x, int y) const
    {
        assert(x >= 0 && static_cast<unsigned>(x) < mode.hRes);
        assert(y >= 0 && static_cast<unsigned>(y) < mode.vRes);
        return y * mode.hRes + x;
    }

    void dot(int x, int y, int color) { buffers[backBuffer][position(x, y)] = color; }
    uint8_t getPixel(int x, int y) { return buffers[backBuffer][position(x, y)]; }
    void clear(int color) { std::fill(buffers[backBuffer].begin(), buffers[backBuffer].end(), color); }
    void show() { backBuffer ^= 1; }
    void drawLineFromMemory8(int x, int y, const uint8_t *pixels)
    {
        ++glyphLines;
        for (int i = 0; i < 8; ++i) dot(x + i, y, pixels[i]);
    }
} video;

TestVga *vga = &video;
static uint8_t backup[640 * 250], backupPage2[640 * 250];
uint8_t *MENU_Backup = backup;
uint8_t *MENU_BackupPage2 = backupPage2;
struct { uint8_t FileList[69][11]; } FileArray;
struct { bool Artefact = false; } sf;
struct { uint8_t Name_Disk[4][256] = {}; } Disk_Drive;
enum class DiskRomSelection { CP400, CoCo2 };
DiskRomSelection selectedDiskRom = DiskRomSelection::CP400;
uint8_t RGB332ToVGAPacked(uint8_t color)
{
    return (color >> 5) | ((color & 0x1c) << 1) | ((color & 3) << 6);
}
void vTaskDelay(int) {}
void DrawText(const char *, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t);

#include "MenuLayout.inc"
#include "MenuLine.inc"
#include "MenuText.inc"
#include "MenuFrame.inc"
#include "MenuBackup.inc"
#include "MenuRows.inc"
#include "MenuOptions.inc"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#include "MenuPreview.inc"
#pragma GCC diagnostic pop

static void checkSeparator(int row, int left, int right)
{
    const uint8_t color = RGB332ToVGAPacked(MENU_ACCENT_COLOR);
    for (int x = left; x <= right; ++x) assert(vga->getPixel(x, row) == color);
    assert(vga->getPixel(left - 1, row) == 0);
    assert(vga->getPixel(right + 1, row) == 0);
}

static void checkLayout(const Mode &mode)
{
    vga->reset(mode);
    const int left = (mode.hRes - 266) / 2;
    const int top = (mode.vRes - 224) / 2;
    const int right = left + 264;
    assert(abs(left - static_cast<int>(mode.hRes - left - 266)) <= 1);
    assert(abs(top - static_cast<int>(mode.vRes - top - 224)) <= 1);
    assert(MenuOriginX() == left && MenuOriginY() == top);

    DrawMainMenuOptions(MAIN_MENU_DISK_DRIVES);
    checkSeparator(top + 10, left, right);
    checkSeparator(top + 199, left, right);
    assert(abs(left - static_cast<int>(mode.hRes - right - 1)) <= 1);
    assert(vga->getPixel(left, top + 24) == RGB332ToVGAPacked(MENU_ACCENT_COLOR));
    for (unsigned y = 0; y < mode.vRes; ++y)
        for (unsigned x = 0; x < mode.hRes; ++x)
            if (static_cast<int>(x) < left || static_cast<int>(x) >= left + 266 ||
                static_cast<int>(y) < top || static_cast<int>(y) >= top + 224)
                assert(vga->getPixel(x, y) == 0);

    DrawMenuDiskChoose(3);
    checkSeparator(top + 10, left, right);
    assert(vga->getPixel(left, top + 48) == RGB332ToVGAPacked(MENU_ACCENT_COLOR));
    memset(FileArray.FileList, 255, sizeof(FileArray.FileList));
    memcpy(FileArray.FileList[0], "TEST    BAS", 11);
    DisplayDiskContent();
    DrawFrames();
    checkSeparator(top + 108, left, right);
    for (int x : {left, left + 88, left + 176, right})
        assert(vga->getPixel(x, top + 150) == RGB332ToVGAPacked(MENU_ACCENT_COLOR));
    assert(vga->getPixel(left + 5, top + 115) == RGB332ToVGAPacked(MENU_ALERT_COLOR));

    DrawScreenFrame("Firmware update", "One hint", nullptr);
    checkSeparator(top + 207, left, right);
    ProgressMessage = "";
    FirmwareProgressUpdate("Validating", 50);
    for (unsigned buffer = 0; buffer < 2; ++buffer)
    {
        vga->backBuffer = buffer;
        checkSeparator(top + 10, left, right);
        checkSeparator(top + 40, left, right);
        assert(vga->getPixel(left + 2, top + 42) == RGB332ToVGAPacked(MENU_TEXT_COLOR));
        assert(vga->getPixel(left + 133, top + 42) == 0);
    }
    FirmwareProgressUpdate("Validating", 100);
    for (unsigned buffer = 0; buffer < 2; ++buffer)
    {
        vga->backBuffer = buffer;
        assert(vga->getPixel(left + 133, top + 42) == RGB332ToVGAPacked(MENU_TEXT_COLOR));
    }

    vga->clear(0);
    char longText[100];
    memset(longText, 'M', sizeof(longText) - 1);
    longText[sizeof(longText) - 1] = '\0';
    vga->glyphLines = 0;
    DrawText(longText, 0, 27, 0, 0, 255, 0);
    assert(vga->glyphLines == 44 * 8);
    vga->glyphLines = 0;
    DrawText("M", 43, 27, 0, 0, 255, 0);
    assert(vga->glyphLines == 8);
    DrawText("M", 44, 27, 0, 0, 255, 0);
    DrawText("M", 0, 28, 0, 0, 255, 0);
    DrawText("M", 43, 27, 1, 0, 255, 0);
    DrawText("M", 0, 27, 0, 1, 255, 0);
    assert(vga->glyphLines == 8);
}

static void checkRestoration(const Mode &mode)
{
    vga->reset(mode);
    memset(backup, 0xa5, sizeof(backup));
    memset(backupPage2, 0xa5, sizeof(backupPage2));
    for (unsigned buffer = 0; buffer < 2; ++buffer)
        for (size_t pixel = 0; pixel < vga->buffers[buffer].size(); ++pixel)
            vga->buffers[buffer][pixel] = (pixel + buffer * 31) % 255 + 1;
    const std::vector<uint8_t> original[] = {vga->buffers[0], vga->buffers[1]};
    BackDisplay();
    for (const auto &buffer : vga->buffers)
        for (uint8_t pixel : buffer) assert(pixel == 0);
    DrawProgressScreen("Testing restoration", 100);
    RestoreDisplay();
    for (unsigned buffer = 0; buffer < 2; ++buffer)
        assert(vga->buffers[buffer] == original[buffer]);
    for (size_t pixel = mode.hRes * mode.vRes; pixel < sizeof(backup); ++pixel)
        assert(backup[pixel] == 0xa5 && backupPage2[pixel] == 0xa5);
}

int main()
{
    for (const Mode &mode : {Mode::MODE_320x240x60, Mode::MODE_320x240x60_4_3,
                            Mode::MODE_640x240x60, Mode::MODE_640x240x60_4_3})
    {
        checkLayout(mode);
        checkRestoration(mode);
        printf("PASS: %ux%u menu centered, text clipped, both full buffers restored.\n",
               mode.hRes, mode.vRes);
    }
}
