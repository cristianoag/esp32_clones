#pragma once
#include "SmsJoystickMapping.h"
#include <stddef.h>
#include <stdint.h>

struct SmsJoystickSnapshot
{
    uint16_t vid, pid;
    uint32_t generation, sequence;
    uint8_t endpoint, length, report[8], buttons;
    bool connected, calibrated;
};

bool SmsJoysticksStart();
void SmsJoystickSnapshotFor(unsigned port, SmsJoystickSnapshot &snapshot);
uint16_t SmsJoysticksRead();
bool SmsJoystickSave(unsigned port, const SmsJoystickSnapshot &identity,
                     const SmsJoystickMapping &mapping, char *error, size_t errorSize);
bool SmsJoystickClear(unsigned port, char *error, size_t errorSize);
