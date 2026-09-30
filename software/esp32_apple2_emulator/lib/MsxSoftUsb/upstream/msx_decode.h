#ifndef MSX_USB_DECODE_H
#define MSX_USB_DECODE_H

#include <stdint.h>

/* Uses the timestamp/bit-order conventions of the original transport:
 * The original was written by Dima Samsonov @ Israel
 * sdima1357@gmail.com on 3/2021.
 * Copyright (C) 2021 Dmitry Samsonov.
 *
 * Decode from the first K edge, not from the preceding idle-J interval.
 * Bytes remain bit-reversed to match the shared transport's PID constants. */
static inline int msx_usb_decode_edges(const uint16_t *edges, unsigned count,
    unsigned jState, unsigned kState, unsigned timeMultiplier,
    uint8_t *bytes, unsigned capacity, unsigned *length)
{
  *length = 0;
  unsigned first = 0;
  while(first < count && (edges[first] >> 8) != kState) ++first;
  if(first == count) return 0;
  uint16_t previous = edges[first];
  unsigned previousBitState = jState, ones = 0, bitCount = 0;
  uint8_t value = 0;
  int ended = 0;
  for(unsigned i = first + 1; i < count; ++i) {
    unsigned state = edges[i] >> 8;
    /* A brief both-high sample can occur between the two pin transitions.
     * Merge it into the preceding run rather than turning SE1 into data. */
    if(state == (jState | kState)) continue;
    if(state != 0 && state != jState && state != kState) return 0x7f;
    unsigned runState = previous >> 8;
    unsigned ticks = (uint8_t)(edges[i] - previous);
    unsigned bits = (ticks * timeMultiplier + 512) / 1024;
    if(!bits || bits > 7) return 0x7f;
    for(unsigned b = 0; b < bits; ++b) {
      unsigned bit = runState == previousBitState;
      previousBitState = runState;
      if(ones == 6) {
        if(bit) return 0x7f;
        ones = 0;
        continue;
      }
      ones = bit ? ones + 1 : 0;
      value = (uint8_t)((value << 1) | bit);
      if(++bitCount == 8) {
        if(*length == capacity) return 0x7f;
        if(!*length && value != 1) return 0; /* Full low-speed SYNC required. */
        bytes[(*length)++] = value;
        bitCount = 0;
        value = 0;
      }
    }
    if(!state) { ended = 1; break; }
    previous = edges[i];
  }
  if(!ended || bitCount || *length < 2) return 0x7f;
  const unsigned pid = bytes[1];
  if(((pid >> 4) ^ (pid & 15)) != 15) return 0x7f;
  if(pid == 0x4b || pid == 0x5a || pid == 0x78)
    return *length == 2 ? (int)pid : 0x7f;
  if((pid != 0xc3 && pid != 0xd2) || *length < 4) return 0x7f;
  unsigned crc = 0xffff;
  for(unsigned i = 2; i < *length; ++i)
    for(unsigned bit = 0; bit < 8; ++bit) {
      unsigned feedback = ((crc >> 15) ^ (bytes[i] >> (7 - bit))) & 1;
      crc = (crc << 1) & 0xffff;
      if(feedback) crc ^= 0x8005;
    }
  return crc == 0x800d ? 0x7b : 0x7f;
}
#endif
