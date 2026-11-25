#pragma once
#include "AppleJoysticks.h"

inline bool AppleJoystickDisplayChanged(const AppleJoystickSnapshot &before, const AppleJoystickSnapshot &after)
{
    if (before.vid != after.vid || before.pid != after.pid ||
        before.endpoint != after.endpoint || before.length != after.length ||
        before.connected != after.connected || before.calibrated != after.calibrated ||
        before.buttons != after.buttons) return true;
    for (unsigned i = 0; i < sizeof(before.report); ++i)
        if (before.report[i] != after.report[i]) return true;
    return false;
}
