#include "MsxSettings.h"
#include "MsxMenuLayout.h"
#include "MsxBootProgress.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <string>

int main()
{
    assert(MsxBootPercent(MsxBootBuffers) == 0);
    for (unsigned i = 1; i <= MsxBootStageCount; ++i)
    {
        assert(MsxBootPercent(i) > MsxBootPercent(i - 1));
        assert(MsxProgressWidth(MsxBootPercent(i)) <= 300);
    }
    assert(MsxBootPercent(MsxBootJoysticks) < 100);
    assert(MsxBootPercent(MsxBootStageCount) == 100);
    assert(MsxBootPercent(1000) == 100);
    assert(MsxProgressWidth(0) == 0 && MsxProgressWidth(50) == 150);
    assert(MsxProgressWidth(100) == 300 && MsxProgressWidth(255) == 300);
    static_assert(MsxMenuCount == 12, "All F12 options must be reachable.");
    for (unsigned i = 1; i < MsxMenuCount; ++i)
        assert(MsxMenuRowY(i) - MsxMenuRowY(i - 1) == 8);
    assert(MsxMenuRowHeight == 8);
    assert(MsxMoveSelection(0, -1, MsxMenuCount) == MsxMenuJoysticks);
    assert(MsxMoveSelection(MsxMenuJoysticks, 1, MsxMenuCount) == MsxMenuResume);
    assert(MsxMoveSelection(15, 1, 33) == 16);
    assert(MsxMoveSelection(32, 1, 33) == 0);
    assert(MsxMoveSelection(0, -1, 0) == 0);

    MsxBootSettingsV1 previous = {1, "omega", 32, 1, 1};
    MsxBootSettings decoded;
    assert(MsxDecodeSettings(&previous, sizeof(previous), decoded));
    assert(decoded.machine.version == 2 && decoded.machine.ramPages == 32);
    assert(!strcmp(decoded.machine.profile, "omega"));
    assert(!decoded.cartridges[0][0] && !decoded.cartridges[1][0]);
    strcpy(decoded.cartridges[0], "/msx/roms/game one.ROM");
    strcpy(decoded.cartridges[1], "/Games/subfolder/utility.rom");
    MsxBootSettings saved = decoded;
    assert(MsxDecodeSettings(&saved, sizeof(saved), decoded));
    assert(!memcmp(&saved, &decoded, sizeof(saved)));
    saved.cartridges[0][0] = '\0';
    assert(MsxDecodeSettings(&saved, sizeof(saved), decoded));
    assert(!decoded.cartridges[0][0] && decoded.cartridges[1][0]);
    saved.cartridges[1][0] = '\0';
    assert(MsxDecodeSettings(&saved, sizeof(saved), decoded));
    assert(!decoded.cartridges[0][0] && !decoded.cartridges[1][0]);
    saved.machine.version = 3;
    assert(!MsxDecodeSettings(&saved, sizeof(saved), decoded));
    saved.machine.version = 2;
    saved.machine.ramPages = 5;
    assert(!MsxDecodeSettings(&saved, sizeof(saved), decoded));
    saved.machine.ramPages = 4;
    saved.machine.autoBoot = 2;
    assert(!MsxDecodeSettings(&saved, sizeof(saved), decoded));
    saved.machine.autoBoot = 0;
    strcpy(saved.machine.profile, "../bad");
    assert(!MsxDecodeSettings(&saved, sizeof(saved), decoded));
    strcpy(saved.machine.profile, "expert");
    memset(saved.cartridges[0], 'x', sizeof(saved.cartridges[0]));
    assert(!MsxDecodeSettings(&saved, sizeof(saved), decoded));
    strcpy(saved.cartridges[0], "/bad.flh");
    assert(!MsxDecodeSettings(&saved, sizeof(saved), decoded));
    strcpy(saved.cartridges[0], "/../game.rom");
    assert(!MsxDecodeSettings(&saved, sizeof(saved), decoded));
    assert(!MsxDecodeSettings(&previous, sizeof(previous) - 1, decoded));
    previous.version = 2;
    assert(!MsxDecodeSettings(&previous, sizeof(previous), decoded));
    assert(!MsxDecodeSettings(nullptr, sizeof(saved), decoded));

    assert(MsxValidSdPath("", true));
    assert(!MsxValidSdPath(""));
    assert(!MsxValidSdPath("/"));
    assert(MsxValidSdPath("/my game.rom"));
    assert(MsxValidSdPath("/folder/more/game.rom"));
    for (const char *path : {"relative.rom", "/../game.rom", "/x/./game.rom", "/x//game.rom",
                             "/x/", "C:\\game.rom", "/x\\game.rom", "/x:game.rom", "/x\n.rom"})
        assert(!MsxValidSdPath(path));
    assert(MsxValidSdPath(("/" + std::string(238, 'x')).c_str()));
    assert(!MsxValidSdPath(("/" + std::string(239, 'x')).c_str()));
    char path[MsxSdPathCapacity];
    assert(MsxJoinSdPath("/", "GAME.ROM", path, sizeof(path)));
    assert(!strcmp(path, "/GAME.ROM"));
    assert(MsxJoinSdPath("/games", "a rom.rom", path, sizeof(path)));
    assert(!strcmp(path, "/games/a rom.rom"));
    MsxParentSdPath(path);
    assert(!strcmp(path, "/games"));
    MsxParentSdPath(path);
    assert(!strcmp(path, "/"));
    MsxParentSdPath(path);
    assert(!strcmp(path, "/"));
    assert(!MsxJoinSdPath("/", "../game.rom", path, sizeof(path)));
    assert(!MsxJoinSdPath("/", "game.rom", path, 4));
    assert(!MsxJoinSdPath("/games", "..", path, sizeof(path)));
    assert(!MsxJoinSdPath("/", "", path, sizeof(path)));
    assert(MsxHasExtension("game.ROM", ".rom"));
    assert(MsxHasExtension("ESP32_MSX-1.00.flh", ".FLH"));
    assert(!MsxHasExtension("rom", ".rom"));
    assert(!MsxHasExtension("game.rom.zip", ".rom"));
    puts("PASS: compact 8px menu, navigation, saved-slot round trips, v1 migration, invalid settings, SD paths.");
}
