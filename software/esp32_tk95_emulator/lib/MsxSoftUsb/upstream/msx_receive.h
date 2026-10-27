#ifndef MSX_USB_RECEIVE_H
#define MSX_USB_RECEIVE_H

#include <stdint.h>

typedef struct {
  uint16_t *samples;
  uint32_t lastEdge;
  uint32_t timeoutCycles;
  uint32_t pins;
  uint32_t lineMask;
  uint32_t bitCycles;
  uint32_t transitionStart;
  uint32_t transitionPins;
  int transitionPending;
  unsigned count;
  unsigned shift;
  int overflow;
} msx_usb_capture_t;

static inline __attribute__((always_inline)) void msx_usb_capture_begin(
    msx_usb_capture_t *capture, uint16_t *samples, uint32_t pins,
    uint32_t lineMask, unsigned shift, uint32_t now,
    uint32_t bitCycles, uint32_t timeoutCycles)
{
  capture->samples = samples;
  capture->lastEdge = now;
  capture->timeoutCycles = timeoutCycles;
  capture->pins = pins;
  capture->lineMask = lineMask;
  capture->bitCycles = bitCycles;
  capture->transitionStart = now;
  capture->transitionPins = pins;
  capture->transitionPending = !pins || pins == lineMask;
  capture->count = 1;
  capture->shift = shift;
  capture->overflow = 0;
  samples[0] = (uint16_t)(((pins >> shift) << 8) | ((now >> 3) & 255));
}

/* J/K transitions can briefly read SE0 or SE1. Confirm SE0 for one bit before
 * ending capture; true EOP drives it for two bits. Preserve the first transition
 * timestamp for decoding and ACK turnaround, not the later confirmation time.
 * At most 255 entries fit the decoder's eight-bit counter. */
static inline __attribute__((always_inline)) int msx_usb_capture_sample(
    msx_usb_capture_t *capture, uint32_t pins, uint32_t now)
{
  if(pins == capture->pins && !capture->transitionPending)
    return (uint32_t)(now - capture->lastEdge) < capture->timeoutCycles;
  uint32_t edgeTime = now;
  if(!pins || pins == capture->lineMask) {
    if(!capture->transitionPending || capture->transitionPins != pins) {
      capture->transitionStart = now;
      capture->transitionPins = pins;
      capture->transitionPending = 1;
    }
    if((uint32_t)(now - capture->transitionStart) < capture->bitCycles)
      return (uint32_t)(now - capture->lastEdge) < capture->timeoutCycles;
    if(pins) return 0; /* Sustained SE1 is an invalid bus state, not EOP. */
    edgeTime = capture->transitionStart;
  } else if(capture->transitionPending) {
    edgeTime = capture->transitionStart;
    capture->transitionPending = 0;
  }
  if(pins != capture->pins) {
    if(capture->count == 255) {
      capture->overflow = 1;
      capture->count = 0;
      return 0;
    }
    capture->samples[capture->count++] =
        (uint16_t)(((pins >> capture->shift) << 8) | ((edgeTime >> 3) & 255));
    capture->pins = pins;
    capture->lastEdge = edgeTime;
  }
  return pins && (uint32_t)(now - capture->lastEdge) < capture->timeoutCycles;
}
#endif
