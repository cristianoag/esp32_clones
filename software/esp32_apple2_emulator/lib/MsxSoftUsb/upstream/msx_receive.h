#ifndef MSX_USB_RECEIVE_H
#define MSX_USB_RECEIVE_H

#include <stdint.h>

typedef struct {
  uint16_t *samples;
  uint32_t lastEdge;
  uint32_t timeoutCycles;
  uint32_t pins;
  unsigned count;
  unsigned shift;
  int overflow;
} msx_usb_capture_t;

static inline __attribute__((always_inline)) void msx_usb_capture_begin(
    msx_usb_capture_t *capture, uint16_t *samples, uint32_t pins,
    unsigned shift, uint32_t now, uint32_t timeoutCycles)
{
  capture->samples = samples;
  capture->lastEdge = now;
  capture->timeoutCycles = timeoutCycles;
  capture->pins = pins;
  capture->count = 1;
  capture->shift = shift;
  capture->overflow = 0;
  samples[0] = (uint16_t)(((pins >> shift) << 8) | ((now >> 3) & 255));
}

/* Returns zero after EOP, inactivity or overflow. Timestamp layout matches the
 * existing NRZI decoder, but sampling timeouts no longer depend on compiler
 * loop speed. At most 255 entries fit the decoder's eight-bit counter. */
static inline __attribute__((always_inline)) int msx_usb_capture_sample(
    msx_usb_capture_t *capture, uint32_t pins, uint32_t now)
{
  if(pins != capture->pins) {
    if(capture->count == 255) {
      capture->overflow = 1;
      capture->count = 0;
      return 0;
    }
    capture->samples[capture->count++] =
        (uint16_t)(((pins >> capture->shift) << 8) | ((now >> 3) & 255));
    capture->pins = pins;
    capture->lastEdge = now;
    if(!pins) return 0;
  }
  return (uint32_t)(now - capture->lastEdge) < capture->timeoutCycles;
}
#endif
