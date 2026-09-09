#ifndef FMSX_FRAME_PACER_H
#define FMSX_FRAME_PACER_H

#include <stdint.h>

// Wall-clock policy only: never changes Z80 cycles, VDP lines or sound cadence.
class FramePacer {
public:
  void reset() { started = false; drawPercent = 100; }

  int64_t frame(int64_t now, bool pal)
  {
    const unsigned rate = pal ? 50 : 60;
    if (!started || rate != hz) {
      started = true;
      hz = rate;
      deadline = lastRelease = now;
      fraction = windowFrames = 0;
      windowWork = 0;
      drawPercent = 100;
      advance();
      return 0;
    }

    windowWork += now - lastRelease;
    if (++windowFrames >= hz / 2) {
      const int64_t budget = int64_t(windowFrames) * 1000000 / hz;
      if (windowWork > budget * 97 / 100)
        drawPercent = drawPercent > 20 ? drawPercent - 10 : 10;
      else if (windowWork < budget * 80 / 100)
        drawPercent = drawPercent < 95 ? drawPercent + 5 : 100;
      windowFrames = 0;
      windowWork = 0;
    }

    // Retain small scheduler jitter, but never accumulate a long catch-up debt.
    if (now - deadline > 1000000 / hz) deadline = now;
    const int64_t wait = deadline > now ? deadline - now : 0;
    advance();
    return wait;
  }

  void released(int64_t now) { lastRelease = now; }
  unsigned renderingPercent() const { return drawPercent; }

private:
  void advance()
  {
    fraction += 1000000;
    deadline += fraction / hz;
    fraction %= hz;
  }

  bool started = false;
  unsigned hz = 60, fraction = 0, windowFrames = 0, drawPercent = 100;
  int64_t deadline = 0, lastRelease = 0, windowWork = 0;
};

#endif
