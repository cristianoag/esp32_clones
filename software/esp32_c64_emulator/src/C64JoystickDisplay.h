#pragma once
#include "C64Joysticks.h"

inline bool C64JoystickDisplayChanged(const C64JoystickSnapshot &before, const C64JoystickSnapshot &after)
{
    if (before.vid != after.vid || before.pid != after.pid ||
        before.endpoint != after.endpoint || before.length != after.length ||
        before.connected != after.connected || before.calibrated != after.calibrated ||
        before.buttons != after.buttons) return true;
    for (unsigned i = 0; i < sizeof(before.report); ++i)
        if (before.report[i] != after.report[i]) return true;
    return false;
}
