#pragma once
#include "C64JoystickMapping.h"
#include <stddef.h>
#include <stdint.h>

struct C64JoystickSnapshot
{
    uint16_t vid, pid;
    uint32_t generation, sequence;
    uint8_t endpoint, length, report[8], buttons;
    bool connected, calibrated;
};

bool C64JoysticksStart();
void C64JoystickSnapshotFor(unsigned port, C64JoystickSnapshot &snapshot);
uint16_t C64JoysticksRead();
bool C64JoystickSave(unsigned port, const C64JoystickSnapshot &identity,
                     const C64JoystickMapping &mapping, char *error, size_t errorSize);
bool C64JoystickClear(unsigned port, char *error, size_t errorSize);
