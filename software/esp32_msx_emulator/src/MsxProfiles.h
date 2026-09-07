#pragma once
#include <stddef.h>

struct MsxProfile
{
    char id[33];
    char name[41];
    int model;
    int ramPages;
    bool available;
    char error[96];
};

constexpr size_t MsxMaxProfiles = 32;
extern MsxProfile MsxProfiles[MsxMaxProfiles];
extern size_t MsxProfileCount;
bool MsxParseProfile(const char *text, MsxProfile &profile);
bool MsxMountSd();
void MsxScanProfiles();
bool MsxValidateProfile(MsxProfile &profile);
