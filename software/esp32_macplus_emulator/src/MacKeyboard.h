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
bool MacKeyboardStart();
// Menu press edges: arrows, Enter, Escape, F12, PgUp/PgDn, Delete, Y/N.
uint8_t MacKeyboardMenuKey();
// Discards queued edges, without forgetting held keys or generating repeats.
void MacKeyboardClearEvents();
// True after boot protocol was selected and interrupt polling was submitted.
bool MacKeyboardConnected();
// Raw boot report; queued Mac scancodes use bit 7 for release.
void MacKeyboardReport(uint8_t report[8]);
bool MacKeyboardCharacter(uint8_t &character);
void MacKeyboardEmulation(bool enabled);
void MacKeyboardReset();
