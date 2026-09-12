#include "MsxFileBrowser.h"
#include "Arduino.h"
#include "SD_MMC.h"
#include <assert.h>
#include <string.h>

TestSerial Serial;
TestSd SD_MMC;
unsigned BrowserYields = 0;

int main()
{
    SD_MMC.nodes["/"] = std::make_shared<TestNode>("/", true);
    SD_MMC.add("/", "System Volume Information", true);
    SD_MMC.add("/", "games", true);
    SD_MMC.add("/games", "sYsTeM VoLuMe InFoRmAtIoN", true);
    SD_MMC.add("/", "readme.txt", false);
    SD_MMC.add("/", "ESP32_MSX-1.00.FLH", false);
    for (unsigned i = 0; i < 140; ++i)
        SD_MMC.add("/", "game" + std::to_string(i) + (i % 2 ? ".ROM" : ".rom"), false);
    MsxFileEntry page[MsxBrowserPageSize];
    char error[160];
    size_t count, total;
    assert(MsxReadDirectoryPage("/", ".rom", true, 0, page, count, total, error, sizeof(error)));
    assert(!*error && count == 16 && total == 142);
    assert(page[0].kind == MsxFileKind::Eject);
    assert(page[1].kind == MsxFileKind::Directory && !strcmp(page[1].name, "games"));
    assert(page[2].kind == MsxFileKind::File && !strcmp(page[2].name, "game0.rom"));
    assert(MsxReadDirectoryPage("/", ".rom", true, 16, page, count, total, error, sizeof(error)));
    assert(count == 16 && !strcmp(page[0].name, "game14.rom"));
    assert(MsxReadDirectoryPage("/", ".rom", true, 128, page, count, total, error, sizeof(error)));
    assert(count == 14 && !strcmp(page[13].name, "game139.ROM"));
    assert(BrowserYields > 0);
    assert(MsxReadDirectoryPage("/", ".flh", false, 0, page, count, total, error, sizeof(error)));
    assert(count == 2 && total == 2);
    assert(page[0].kind == MsxFileKind::Directory);
    assert(page[1].kind == MsxFileKind::File && !strcmp(page[1].name, "ESP32_MSX-1.00.FLH"));
    assert(MsxReadDirectoryPage("/games", ".rom", true, 0, page, count, total, error, sizeof(error)));
    assert(count == 2 && page[0].kind == MsxFileKind::Eject && page[1].kind == MsxFileKind::Parent);
    SD_MMC.add("/games", std::string(240, 'x') + ".rom", false);
    SD_MMC.add("/games", "normal.ROM", false);
    assert(MsxReadDirectoryPage("/games", ".rom", true, 0, page, count, total, error, sizeof(error)));
    assert(count == 3 && total == 3 && *error && Serial.messages == 1);
    assert(!strcmp(page[2].name, "normal.ROM"));
    assert(!MsxReadDirectoryPage("/missing", ".rom", true, 0, page, count, total, error, sizeof(error)));
    assert(!count && !total && *error);
    assert(!MsxReadDirectoryPage("/readme.txt", ".rom", true, 0, page, count, total, error, sizeof(error)));
    assert(!count && !total && *error);
    SD_MMC.nodes["/"] = std::make_shared<TestNode>("/", true);
    assert(MsxReadDirectoryPage("/", ".flh", false, 0, page, count, total, error, sizeof(error)));
    assert(!count && !total && !*error);
    SD_MMC.add("/", "SYSTEM VOLUME INFORMATION", true);
    assert(MsxReadDirectoryPage("/", ".flh", false, 0, page, count, total, error, sizeof(error)));
    assert(!count && !total && !*error);
    SD_MMC.add("/", "System Volume Information backup", true);
    SD_MMC.add("/", "System Volume Information.rom", false);
    assert(MsxReadDirectoryPage("/", ".rom", false, 0, page, count, total, error, sizeof(error)));
    assert(count == 2 && total == 2 && !*error);
    assert(page[0].kind == MsxFileKind::Directory && !strcmp(page[0].name, "System Volume Information backup"));
    assert(page[1].kind == MsxFileKind::File && !strcmp(page[1].name, "System Volume Information.rom"));
    SD_MMC.add("/", "game disk.DsK", false);
    SD_MMC.add("/", "basic.CaS", false);
    SD_MMC.add("/", "audio.wav", false);
    assert(MsxReadDirectoryPage("/", ".dsk", true, 0, page, count, total, error, sizeof(error)));
    assert(count == 3 && total == 3);
    assert(page[0].kind == MsxFileKind::Eject);
    assert(!strcmp(page[2].name, "game disk.DsK"));
    assert(MsxReadDirectoryPage("/", ".cas", true, 0, page, count, total, error, sizeof(error)));
    assert(count == 3 && total == 3);
    assert(!strcmp(page[2].name, "basic.CaS"));
    puts("PASS: browser pagination, ROM/FLH filtering, hidden Windows metadata folder, ordinary names and empty SD.");
}
