/******************************************************************************
 * Project      : esp32_cp400_emulator
 * File         : CP400UsbKeyboard.h
 * Last Updated : 2026-08-29
 *
 * Description  : Native USB-OTG host for the CP400 keyboard port
 *
 * CP400 code and modifications copyright (c) 2026 The Retro Hacker
 *
 * Permission is granted for personal, non-commercial use only.
 * Commercial use, distribution, sublicensing, or modification
 * for commercial purposes is strictly prohibited without
 * prior written permission from the author.
 * Please, keep this in the source code.
 * All rights reserved.
 ******************************************************************************/

#ifndef CP400_USB_KEYBOARD_H
#define CP400_USB_KEYBOARD_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Drives the keyboard port from the ESP32-S3 USB-OTG controller instead of the
 * bit-banged soft host, so full speed keyboards enumerate and not just the rare
 * low speed ones. D- is GPIO19 and D+ is GPIO20; those pads are wired straight
 * to the USB PHY inside the chip and cannot be moved with the GPIO matrix.
 *
 * The joystick ports stay on the soft host, whose timer interrupt also clocks
 * the 6809 emulation, so both stacks run side by side.
 */
void CP400UsbKeyboard_Start(void);

/* True while a keyboard is enumerated and delivering reports. */
bool CP400UsbKeyboard_Connected(void);

#endif
