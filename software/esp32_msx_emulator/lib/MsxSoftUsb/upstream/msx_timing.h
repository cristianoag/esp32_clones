#ifndef MSX_USB_TIMING_H
#define MSX_USB_TIMING_H

#include <stdint.h>

/* The MSX board runs at 240 MHz: one 1.5 MHz USB bit is exactly 160 cycles.
 * Reject clocks that cannot represent that period exactly. */
static inline uint32_t msx_usb_bit_cycles(unsigned cpuMhz)
{
  return cpuMhz >= 120 && cpuMhz <= 240 && cpuMhz % 3 == 0 ? cpuMhz * 2 / 3 : 0;
}

static inline int msx_usb_before_deadline(uint32_t now, uint32_t deadline)
{
  return (int32_t)(now - deadline) < 0;
}
#endif
