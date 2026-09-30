#pragma once

#include <stddef.h>
#include <stdint.h>

enum AppleJoystickPose {
    APPLE_JOYSTICK_NEUTRAL = 0,
    APPLE_JOYSTICK_UP,
    APPLE_JOYSTICK_UP_RIGHT,
    APPLE_JOYSTICK_RIGHT,
    APPLE_JOYSTICK_DOWN_RIGHT,
    APPLE_JOYSTICK_DOWN,
    APPLE_JOYSTICK_DOWN_LEFT,
    APPLE_JOYSTICK_LEFT,
    APPLE_JOYSTICK_UP_LEFT,
    APPLE_JOYSTICK_BUTTON_A,
    APPLE_JOYSTICK_BUTTON_B,
    APPLE_JOYSTICK_POSE_COUNT
};

struct AppleJoystickSample {
    uint8_t bytes[8];
    // OR of (report XOR first report) throughout this held pose's capture.
    uint8_t changed[8];
};

struct AppleJoystickAxis {
    uint8_t byteIndex; // 0xff disables this axis.
    uint8_t mask;
    uint8_t signFlip; // 0x80 orders signed bytes; zero orders unsigned bytes.
    uint8_t lowThreshold;
    uint8_t highThreshold;
    uint8_t lowBit;
    uint8_t highBit;
};

// Byte-only, bounded POD: persist the entire object, guarded by its version.
struct AppleJoystickMapping {
    uint8_t version;
    uint8_t length;
    uint8_t ignored[8];
    uint8_t fixedMask[8];
    uint8_t fixedValue[8];
    uint8_t directionMask[8];
    uint8_t directions[9][8];
    uint8_t buttonMask[2][8];
    uint8_t buttonPressed[2][8];
    AppleJoystickAxis axes[2]; // Horizontal, vertical.
};

static const uint8_t APPLE_JOYSTICK_MAPPING_VERSION = 1;

bool AppleBuildJoystickMapping(uint8_t length,
                             const AppleJoystickSample (&poses)[11],
                             AppleJoystickMapping &mapping,
                             char *error, size_t errorSize);
// Neutral, Up, Right, Down, Left, A, B. Derives diagonals from independent
// axes/direction bits or a standard circular 4-bit HID hat.
bool AppleBuildJoystickCardinalMapping(uint8_t length,
                                     const AppleJoystickSample (&poses)[7],
                                     AppleJoystickMapping &mapping,
                                     char *error, size_t errorSize);
bool AppleValidJoystickMapping(const AppleJoystickMapping &mapping);
// Output uses active-high fMSX bits: Up, Down, Left, Right, A, B.
// On every failure result is zero, including unknown hats and report lengths.
bool AppleDecodeJoystickReport(const AppleJoystickMapping &mapping,
                            const uint8_t *report, size_t length,
                            uint8_t &result);
