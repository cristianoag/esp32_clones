#pragma once

#include <stddef.h>
#include <stdint.h>

// Only physical connector 2 is enabled: port 1, D+18 / D-17.
// Reserves TG1/T0 (Arduino timer 2); native USB GPIO19/20 is untouched.
// Direct low-speed HID boot mice only. Selects the first alternate-zero boot
// mouse interface and interrupt-IN endpoint, then requests SET_PROTOCOL(0).
// Configuration descriptors must fit 255 bytes. No hubs or full-speed mice.
// Callbacks run serially in the core-0 host task, never the timer ISR; they must
// be brief and must not write flash. Report storage is valid only in the call.
bool MacMouseHostStart(
    void (*device)(unsigned port, uint16_t vid, uint16_t pid, bool connected),
    void (*report)(unsigned port, uint8_t endpoint, const uint8_t *data, size_t length));

// Stops the timer, discards queued/in-flight reports, and delivers disconnect
// callbacks on the host task before returning true (neutralize input there).
// Pair with Resume on the SAME task after flash writes finish. Resume forces
// USB reset/re-enumeration even for unchanged devices; input stays neutral until
// a new live report arrives. Change-only mice have no held-input timeout.
// Not recursive: a nested call or call from a host callback returns false
// promptly rather than deadlocking. A host which was never started is safe to
// pause/resume. Serialize startup with flash operations.
bool MacMouseHostPause();
void MacMouseHostResume();
