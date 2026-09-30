#pragma once
#include "AppleJoystickMapping.h"
#include <stddef.h>
#include <stdint.h>

struct AppleJoystickSnapshot
{
    uint16_t vid, pid;
    uint32_t generation, sequence;
    uint8_t endpoint, length, report[8], buttons;
    bool connected, calibrated;
};

bool AppleJoysticksStart();
void AppleJoystickSnapshotFor(unsigned port, AppleJoystickSnapshot &snapshot);
uint16_t AppleJoysticksRead();
bool AppleJoystickSave(unsigned port, const AppleJoystickSnapshot &identity,
                     const AppleJoystickMapping &mapping, char *error, size_t errorSize);
bool AppleJoystickClear(unsigned port, char *error, size_t errorSize);
