/******************************************************************************
 * Project      : esp32_cp400_emulator
 * File         : CP400Input.h
 * Last Updated : 2026-08-17
 *
 * Description  : USB keyboard and joystick input handling for the CP400 emulator
 *
 * Original work copyright (c) 2026 Cedric Beaudoin
 * CP400 code and modifications copyright (c) 2026 The Retro Hacker
 *
 * Permission is granted for personal, non-commercial use only.
 * Commercial use, distribution, sublicensing, or modification
 * for commercial purposes is strictly prohibited without
 * prior written permission from the author.
 * Please, keep this in the source code.
 * All rights reserved.
 ******************************************************************************/


#ifndef CP400_INPUT_H
#define CP400_INPUT_H

#include <Arduino.h>

#define JOYSTICK_REPORT_SIZE 32
#define JOYSTICK_CONTROL_COUNT 6

enum JoystickControl : uint8_t
{
    JOYSTICK_UP,
    JOYSTICK_DOWN,
    JOYSTICK_LEFT,
    JOYSTICK_RIGHT,
    JOYSTICK_BUTTON_1,
    JOYSTICK_BUTTON_2
};

struct JoystickCalibration
{
    uint8_t reportLength;
    uint8_t neutral[JOYSTICK_REPORT_SIZE];
    uint8_t stable[JOYSTICK_REPORT_SIZE];   //0 for bytes that change on their own at rest
    uint8_t control[JOYSTICK_CONTROL_COUNT][JOYSTICK_REPORT_SIZE];
    uint8_t controlBits[JOYSTICK_CONTROL_COUNT][JOYSTICK_REPORT_SIZE];
    uint8_t directionBits[JOYSTICK_REPORT_SIZE];
    uint8_t valid;
};

struct USB_DEVICES_CTRL
{
    uint8_t SHIFT;
    uint8_t CTRL;
    uint8_t ALT;
    uint16_t ScanArray[256];
    uint16_t CP400ScanCodes[256];
    uint8_t USB_CP400_Key_Array[8];
    uint8_t PORT_JOY1;
    uint8_t PORT_JOY2;
    uint8_t PORT_KEYBOARD;
    uint8_t JOY1_BUTT1;
    uint8_t JOY1_BUTT2;
    uint8_t JOY1_X_AXIS;
    uint8_t JOY1_Y_AXIS;
    uint8_t JOY2_BUTT1;
    uint8_t JOY2_BUTT2;
    uint8_t JOY2_X_AXIS;
    uint8_t JOY2_Y_AXIS;
    uint8_t JOYSTICK_REPORT[2][JOYSTICK_REPORT_SIZE];
    uint8_t JOYSTICK_REPORT_LENGTH[2];
    JoystickCalibration JOYSTICK_CALIBRATION[2];
    bool    Message_From_Stm32;
    

};

extern USB_DEVICES_CTRL USB_DEV_CONTROL;

void Setup_USB(void);
void fillKeysStruct(void);
void UpdateKeyMap(uint8_t * Data);
extern void FPGA_Write_Byte(uint16_t Address, uint8_t Data);
extern uint8_t SendRequest(uint8_t DataToSend);

void UpdateJoyMap(uint8_t * Data, uint8_t dataLength, uint8_t usbNum);
bool IsJoystickControlActive(uint8_t joystick, JoystickControl control);
void PrintJoystickCalibration(uint8_t joystick);


#endif
