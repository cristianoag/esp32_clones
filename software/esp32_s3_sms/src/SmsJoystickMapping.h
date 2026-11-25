#pragma once

#include <stddef.h>
#include <stdint.h>

enum SmsJoystickPose {
    SMS_JOYSTICK_NEUTRAL = 0,
    SMS_JOYSTICK_UP,
    SMS_JOYSTICK_UP_RIGHT,
    SMS_JOYSTICK_RIGHT,
    SMS_JOYSTICK_DOWN_RIGHT,
    SMS_JOYSTICK_DOWN,
    SMS_JOYSTICK_DOWN_LEFT,
    SMS_JOYSTICK_LEFT,
    SMS_JOYSTICK_UP_LEFT,
    SMS_JOYSTICK_BUTTON_A,
    SMS_JOYSTICK_BUTTON_B,
    SMS_JOYSTICK_POSE_COUNT
};

struct SmsJoystickSample {
    uint8_t bytes[8];
    // OR of (report XOR first report) throughout this held pose's capture.
    uint8_t changed[8];
};

struct SmsJoystickAxis {
    uint8_t byteIndex; // 0xff disables this axis.
    uint8_t mask;
    uint8_t signFlip; // 0x80 orders signed bytes; zero orders unsigned bytes.
    uint8_t lowThreshold;
    uint8_t highThreshold;
    uint8_t lowBit;
    uint8_t highBit;
};

// Byte-only, bounded POD: persist the entire object, guarded by its version.
struct SmsJoystickMapping {
    uint8_t version;
    uint8_t length;
    uint8_t ignored[8];
    uint8_t fixedMask[8];
    uint8_t fixedValue[8];
    uint8_t directionMask[8];
    uint8_t directions[9][8];
    uint8_t buttonMask[2][8];
    uint8_t buttonPressed[2][8];
    SmsJoystickAxis axes[2]; // Horizontal, vertical.
};

static const uint8_t SMS_JOYSTICK_MAPPING_VERSION = 1;

bool SmsBuildJoystickMapping(uint8_t length,
                             const SmsJoystickSample (&poses)[11],
                             SmsJoystickMapping &mapping,
                             char *error, size_t errorSize);
// Neutral, Up, Right, Down, Left, A, B. Derives diagonals from independent
// axes/direction bits or a standard circular 4-bit HID hat.
bool SmsBuildJoystickCardinalMapping(uint8_t length,
                                     const SmsJoystickSample (&poses)[7],
                                     SmsJoystickMapping &mapping,
                                     char *error, size_t errorSize);
bool SmsValidJoystickMapping(const SmsJoystickMapping &mapping);
// Output uses active-high fMSX bits: Up, Down, Left, Right, A, B.
// On every failure result is zero, including unknown hats and report lengths.
bool SmsDecodeJoystickReport(const SmsJoystickMapping &mapping,
                            const uint8_t *report, size_t length,
                            uint8_t &result);
