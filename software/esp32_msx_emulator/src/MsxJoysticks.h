#pragma once
#include "MsxJoystickMapping.h"
#include <stddef.h>
#include <stdint.h>

struct MsxJoystickSnapshot
{
    uint16_t vid, pid;
    uint32_t generation, sequence;
    uint8_t endpoint, length, report[8], buttons;
    bool connected, calibrated;
};

bool MsxJoysticksStart();
void MsxJoystickSnapshotFor(unsigned port, MsxJoystickSnapshot &snapshot);
uint16_t MsxJoysticksRead();
bool MsxJoystickSave(unsigned port, const MsxJoystickSnapshot &identity,
                     const MsxJoystickMapping &mapping, char *error, size_t errorSize);
bool MsxJoystickClear(unsigned port, char *error, size_t errorSize);
