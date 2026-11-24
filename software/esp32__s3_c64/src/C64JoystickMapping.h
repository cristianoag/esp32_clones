#pragma once

#include <stddef.h>
#include <stdint.h>

enum C64JoystickPose {
    C64_JOYSTICK_NEUTRAL = 0,
    C64_JOYSTICK_UP,
    C64_JOYSTICK_UP_RIGHT,
    C64_JOYSTICK_RIGHT,
    C64_JOYSTICK_DOWN_RIGHT,
    C64_JOYSTICK_DOWN,
    C64_JOYSTICK_DOWN_LEFT,
    C64_JOYSTICK_LEFT,
    C64_JOYSTICK_UP_LEFT,
    C64_JOYSTICK_BUTTON_A,
    C64_JOYSTICK_BUTTON_B,
    C64_JOYSTICK_POSE_COUNT
};

struct C64JoystickSample {
    uint8_t bytes[8];
    // OR of (report XOR first report) throughout this held pose's capture.
    uint8_t changed[8];
};

struct C64JoystickAxis {
    uint8_t byteIndex; // 0xff disables this axis.
    uint8_t mask;
    uint8_t signFlip; // 0x80 orders signed bytes; zero orders unsigned bytes.
    uint8_t lowThreshold;
    uint8_t highThreshold;
    uint8_t lowBit;
    uint8_t highBit;
};

// Byte-only, bounded POD: persist the entire object, guarded by its version.
struct C64JoystickMapping {
    uint8_t version;
    uint8_t length;
    uint8_t ignored[8];
    uint8_t fixedMask[8];
    uint8_t fixedValue[8];
    uint8_t directionMask[8];
    uint8_t directions[9][8];
    uint8_t buttonMask[2][8];
    uint8_t buttonPressed[2][8];
    C64JoystickAxis axes[2]; // Horizontal, vertical.
};

static const uint8_t C64_JOYSTICK_MAPPING_VERSION = 1;

bool C64BuildJoystickMapping(uint8_t length,
                             const C64JoystickSample (&poses)[11],
                             C64JoystickMapping &mapping,
                             char *error, size_t errorSize);
// Neutral, Up, Right, Down, Left, A, B. Derives diagonals from independent
// axes/direction bits or a standard circular 4-bit HID hat.
bool C64BuildJoystickCardinalMapping(uint8_t length,
                                     const C64JoystickSample (&poses)[7],
                                     C64JoystickMapping &mapping,
                                     char *error, size_t errorSize);
bool C64ValidJoystickMapping(const C64JoystickMapping &mapping);
// Output uses active-high fMSX bits: Up, Down, Left, Right, A, B.
// On every failure result is zero, including unknown hats and report lengths.
bool C64DecodeJoystickReport(const C64JoystickMapping &mapping,
                            const uint8_t *report, size_t length,
                            uint8_t &result);
