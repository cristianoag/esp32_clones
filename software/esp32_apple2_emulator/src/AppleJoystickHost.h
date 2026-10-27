#pragma once

#include <stddef.h>
#include <stdint.h>

// Physical connectors 1/2 map to ports 0/1, DP/DM 16/15 and 18/17.
// Reserves TG1/T0 (Arduino timer 2); native USB GPIO19/20 is untouched.
// Direct low-speed devices only: first configuration, first alternate-zero
// HID interface with protocol zero, up to four interrupt-IN endpoints,
// 1..8 bytes including any report ID. Configuration must fit 255 bytes.
// Like CP400, reports are opaque and decoded only by saved calibration;
// no report descriptor fetch/size restriction. SET_IDLE is optional.
// No hubs, full-speed devices, alternate settings or output/feature reports.
// Keyboard/mouse interface protocols are excluded.
// Callbacks run serially in the core-0 host task, never the timer ISR; they must
// be brief and must not write flash. Report storage is valid only in the call.
bool AppleJoystickHostStart(
    void (*device)(unsigned port, uint16_t vid, uint16_t pid, bool connected),
    void (*report)(unsigned port, uint8_t endpoint, const uint8_t *data, size_t length));

// Stops the timer, discards queued/in-flight reports, and delivers disconnect
// callbacks on the host task before returning true (neutralize input there).
// Pair with Resume on the SAME task after flash writes finish. Resume forces
// USB reset/re-enumeration even for unchanged devices; input stays neutral until
// a new live report arrives. Change-only pads have no held-input timeout.
// Not recursive: a nested call or call from a host callback returns false
// promptly rather than deadlocking. A host which was never started is safe to
// pause/resume. Serialize startup with flash operations.
bool AppleJoystickHostPause();
void AppleJoystickHostResume();
