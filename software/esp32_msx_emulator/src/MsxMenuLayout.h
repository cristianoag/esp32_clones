#pragma once
#include <stddef.h>

enum MsxMenuItem : unsigned
{
    MsxMenuResume, MsxMenuBios, MsxMenuSlot1, MsxMenuSlot2, MsxMenuRam,
    MsxMenuSound, MsxMenuAutoBoot, MsxMenuSave, MsxMenuBoot, MsxMenuRescan,
    MsxMenuUpdate, MsxMenuJoysticks, MsxMenuCount
};

constexpr int MsxMenuTop = 48;
constexpr int MsxMenuRowHeight = 8;
constexpr size_t MsxBrowserPageSize = 16;
constexpr int MsxMenuRowY(unsigned row) { return MsxMenuTop + row * MsxMenuRowHeight; }
static_assert(MsxMenuRowY(MsxMenuCount) < 180, "Menu must not overlap navigation hints.");
static_assert(MsxMenuRowY(MsxBrowserPageSize) <= 180, "File list must not overlap page information.");

inline size_t MsxMoveSelection(size_t current, int direction, size_t count)
{
    if (!count) return 0;
    return direction < 0 ? (current ? current - 1 : count - 1) : (current + 1) % count;
}
