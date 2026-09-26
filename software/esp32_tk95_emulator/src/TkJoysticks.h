#pragma once
#include "TkJoystickMapping.h"
#include <stddef.h>
#include <stdint.h>

struct TkJoystickSnapshot
{
    uint16_t vid, pid;
    uint32_t generation, sequence;
    uint8_t endpoint, length, report[8], buttons;
    bool connected, calibrated;
};

bool TkJoysticksStart();
void TkJoystickSnapshotFor(unsigned port, TkJoystickSnapshot &snapshot);
uint16_t TkJoysticksRead();
bool TkJoystickSave(unsigned port, const TkJoystickSnapshot &identity,
                     const TkJoystickMapping &mapping, char *error, size_t errorSize);
bool TkJoystickClear(unsigned port, char *error, size_t errorSize);
