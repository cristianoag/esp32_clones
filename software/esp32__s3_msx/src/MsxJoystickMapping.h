#pragma once

#include <stddef.h>
#include <stdint.h>

enum MsxJoystickPose {
    MSX_JOYSTICK_NEUTRAL = 0,
    MSX_JOYSTICK_UP,
    MSX_JOYSTICK_UP_RIGHT,
    MSX_JOYSTICK_RIGHT,
    MSX_JOYSTICK_DOWN_RIGHT,
    MSX_JOYSTICK_DOWN,
    MSX_JOYSTICK_DOWN_LEFT,
    MSX_JOYSTICK_LEFT,
    MSX_JOYSTICK_UP_LEFT,
    MSX_JOYSTICK_BUTTON_A,
    MSX_JOYSTICK_BUTTON_B,
    MSX_JOYSTICK_POSE_COUNT
};

struct MsxJoystickSample {
    uint8_t bytes[8];
    // OR of (report XOR first report) throughout this held pose's capture.
    uint8_t changed[8];
};

struct MsxJoystickAxis {
    uint8_t byteIndex; // 0xff disables this axis.
    uint8_t mask;
    uint8_t signFlip; // 0x80 orders signed bytes; zero orders unsigned bytes.
    uint8_t lowThreshold;
    uint8_t highThreshold;
    uint8_t lowBit;
    uint8_t highBit;
};

// Byte-only, bounded POD: persist the entire object, guarded by its version.
struct MsxJoystickMapping {
    uint8_t version;
    uint8_t length;
    uint8_t ignored[8];
    uint8_t fixedMask[8];
    uint8_t fixedValue[8];
    uint8_t directionMask[8];
    uint8_t directions[9][8];
    uint8_t buttonMask[2][8];
    uint8_t buttonPressed[2][8];
    MsxJoystickAxis axes[2]; // Horizontal, vertical.
};

static const uint8_t MSX_JOYSTICK_MAPPING_VERSION = 1;

bool MsxBuildJoystickMapping(uint8_t length,
                             const MsxJoystickSample (&poses)[11],
                             MsxJoystickMapping &mapping,
                             char *error, size_t errorSize);
// Neutral, Up, Right, Down, Left, A, B. Derives diagonals from independent
// axes/direction bits or a standard circular 4-bit HID hat.
bool MsxBuildJoystickCardinalMapping(uint8_t length,
                                     const MsxJoystickSample (&poses)[7],
                                     MsxJoystickMapping &mapping,
                                     char *error, size_t errorSize);
bool MsxValidJoystickMapping(const MsxJoystickMapping &mapping);
// Output uses active-high fMSX bits: Up, Down, Left, Right, A, B.
// On every failure result is zero, including unknown hats and report lengths.
bool MsxDecodeJoystickReport(const MsxJoystickMapping &mapping,
                            const uint8_t *report, size_t length,
                            uint8_t &result);
