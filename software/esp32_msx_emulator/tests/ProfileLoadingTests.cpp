#include "MsxProfiles.h"
#include "Arduino.h"
#include "SD_MMC.h"
#include <cassert>
#include <cstring>

JoystickTestSerial Serial;
ProfileTestSd SD_MMC;

static void manifest(const char *id, const char *model)
{
    const std::string path = std::string("/msx/bios/") + id;
    SD_MMC.add(path, 0, "", true);
    const std::string content = std::string("name=Panasonic ") + id + "\nmodel=" + model + "\nram=64\n";
    SD_MMC.add(path + "/profile.ini", content.size(), content);
}
int main()
{
    assert(MsxMountSd());
    SD_MMC.add("/msx/bios", 0, "", true);
    manifest("fs-a1wsx", "MSX2+");
    manifest("fs-a1f", "MSX2");
    manifest("fs-a1fx", "MSX2+");
    SD_MMC.add("/msx/bios/fs-a1wsx/PANASONIC.ROM", 0x54000);
    SD_MMC.add("/msx/bios/fs-a1f/PANASONIC.ROM", 0x34000);
    SD_MMC.add("/msx/bios/fs-a1fx/PANASONIC.ROM", 0x34000);
    MsxScanProfiles();
    assert(MsxProfileCount == 6);
    assert(!strcmp(MsxProfiles[0].id, "omega") && !strcmp(MsxProfiles[1].id, "expert"));
    for (size_t i = 3; i < 6; ++i)
    {
        assert(MsxProfiles[i].available && MsxProfiles[i].ramPages == 4);
        assert(MsxProfiles[i].model == (i == 4 ? 1 : 2));
    }
    auto &profile = MsxProfiles[3];
    SD_MMC.nodes["/msx/bios/fs-a1wsx/PANASONIC.ROM"]->size = 32768;
    assert(!MsxValidateProfile(profile) && profile.error[0]);
    SD_MMC.nodes["/msx/bios/fs-a1wsx/PANASONIC.ROM"]->size = 0x54000;
    SD_MMC.nodes["/msx/bios/fs-a1wsx/PANASONIC.ROM"]->directory = true;
    assert(!MsxValidateProfile(profile));
    SD_MMC.nodes["/msx/bios/fs-a1wsx/PANASONIC.ROM"]->directory = false;
    SD_MMC.add("/msx/bios/fs-a1wsx/OMEGA.ROM", 262144);
    assert(!MsxValidateProfile(profile) && strstr(profile.error, "Ambiguous"));
    SD_MMC.nodes.erase("/msx/bios/fs-a1wsx/OMEGA.ROM");
    assert(MsxValidateProfile(profile));
    SD_MMC.nodes["/msx/bios/fs-a1f/PANASONIC.ROM"]->size = 0x54000;
    assert(!MsxValidateProfile(MsxProfiles[4]));
    SD_MMC.nodes.erase("/msx/bios/fs-a1f/PANASONIC.ROM");
    assert(!MsxValidateProfile(MsxProfiles[4]));
    SD_MMC.add("/msx/bios/fs-a1f/MSX2.ROM", 32768);
    SD_MMC.add("/msx/bios/fs-a1f/MSX2EXT.ROM", 16384);
    assert(MsxValidateProfile(MsxProfiles[4])); // Legacy generic layout remains usable.
    puts("PASS: six F12 profiles, Panasonic models/RAM, combined-file validation, ambiguity, malformed and legacy paths.");
}
