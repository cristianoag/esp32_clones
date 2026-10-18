/******************************************************************************
 * Native USB host adapted from CP400UsbKeyboard by The Retro Hacker.
 * CP400 code and modifications copyright (c) 2026 The Retro Hacker
 *
 * Permission is granted for personal, non-commercial use only.
 * Commercial use, distribution, sublicensing, or modification
 * for commercial purposes is strictly prohibited without
 * prior written permission from the author.
 * Please, keep this in the source code.
 * All rights reserved.
 ******************************************************************************/
#pragma once

#include <stdbool.h>
#include <stdint.h>

// ESP32-S3 native USB host: GPIO19 D-, GPIO20 D+. Board must supply USB VBUS.
// Returns host initialization status, not keyboard presence. Call once at setup.
bool MsxKeyboardStart();
// Atomic active-low snapshot; rows not present on the keyboard remain 0xff.
void MsxKeyboardMatrix(uint8_t matrix[16]);
// Menu press edges: arrows, Enter, Escape, F12, PgUp/PgDn, Delete, Y/N.
uint8_t MsxKeyboardMenuKey();
// Discards queued edges, without forgetting held keys or generating repeats.
void MsxKeyboardClearEvents();
// True after boot protocol was selected and interrupt polling was submitted.
bool MsxKeyboardConnected();
