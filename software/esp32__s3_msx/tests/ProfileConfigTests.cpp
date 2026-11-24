#include "MsxProfiles.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <string>

int main()
{
    MsxProfile profile = {};
    assert(MsxParseProfile("name=Omega MSX2+ NTSC (bank 0)\nmodel=MSX2+\nram=512\n", profile));
    assert(profile.model == 2 && profile.ramPages == 32 && !profile.available && !profile.error[0]);
    assert(!strcmp(profile.name, "Omega MSX2+ NTSC (bank 0)"));
    assert(MsxParseProfile("name=Gradiente Expert 1.1\nmodel=MSX1\nram=64\n", profile));
    assert(profile.model == 0 && profile.ramPages == 4);
    assert(MsxParseProfile("# comment\r\n name = Custom \r\nmodel = MSX2\r\nram = 128", profile));
    assert(profile.model == 1 && profile.ramPages == 8 && !strcmp(profile.name, "Custom"));
    assert(MsxParseProfile("name=Test\nmodel=MSX2\nram=256\n", profile));
    assert(profile.ramPages == 16);
    const char *invalid[] = {
        "", "name=X\nram=64\n", "name=X\nmodel=MSX1\n", "model=MSX1\nram=64\n",
        "name=X\nmodel=MSX1\nram=64\nram=128\n",
        "name=X\nmodel=MSX1\nmodel=MSX2\nram=64\n",
        "name=X\nname=Y\nmodel=MSX1\nram=64\n",
        "name=X\nmodel=MSX3\nram=64\n", "name=X\nmodel=MSX1\nram=65\n",
        "name=X\nmodel=MSX1\nram=64junk\n", "name=\nmodel=MSX1\nram=64\n",
        "name=X\nmodel=MSX1\nram=64\nunknown=yes\n",
        "name=X\nmodel MSX1\nram=64\n", "name=X\nmodel=MSX1\nram=-64\n",
        "name=bad\x01\nmodel=MSX1\nram=64\n"
    };
    for (const char *text : invalid)
    {
        profile.available = true;
        assert(!MsxParseProfile(text, profile));
        assert(!profile.available && profile.error[0]);
    }
    assert(!MsxParseProfile((std::string("name=") + std::string(41, 'A') + "\nmodel=MSX1\nram=64").c_str(), profile));
    assert(!MsxParseProfile(std::string(1025, 'A').c_str(), profile));
    assert(!MsxParseProfile(std::string(128, 'A').c_str(), profile));
    puts("All profile parser checks passed.");
}
