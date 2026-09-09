#include "../FramePacer.h"
#include <cassert>
#include <cstdio>

static void steadyRate(bool pal)
{
  FramePacer pacer;
  const unsigned hz = pal ? 50 : 60;
  int64_t now = 1000000;
  assert(pacer.frame(now, pal) == 0);
  pacer.released(now);
  for (unsigned i = 0; i < hz * 10; ++i) {
    now += 5000;
    const int64_t wait = pacer.frame(now, pal);
    assert(wait > 0);
    now += wait;
    pacer.released(now);
  }
  assert(now == 11000000); // Fractional NTSC deadlines do not drift.
  assert(pacer.renderingPercent() == 100);
}

int main()
{
  steadyRate(false);
  steadyRate(true);
  FramePacer pacer;
  int64_t now = 1000000;
  pacer.frame(now, false);
  pacer.released(now);
  for (unsigned i = 0; i < 600; ++i) {
    now += 30000;
    assert(pacer.frame(now, false) == 0); // No full-frame delay when overloaded.
    pacer.released(now);
    assert(pacer.renderingPercent() >= 10 && pacer.renderingPercent() <= 100);
  }
  assert(pacer.renderingPercent() == 10);
  for (unsigned i = 0; i < 600; ++i) {
    now += 5000;
    now += pacer.frame(now, false);
    pacer.released(now);
  }
  assert(pacer.renderingPercent() == 100);

  // A blocking menu discards deadlines, adaptive history and catch-up debt.
  now += 30000000;
  pacer.reset();
  assert(pacer.frame(now, false) == 0);
  pacer.released(now);
  now += 5000;
  assert(pacer.frame(now, false) == 11666);
  pacer.released(now + 11666);
  now += 11666;
  assert(pacer.frame(now, true) == 0); // Runtime NTSC -> PAL change.
  pacer.released(now);
  now += 5000;
  assert(pacer.frame(now, true) == 15000);

  // Simulate a 10 ms RTOS tick: small oversleeps retain the correct mean rate.
  pacer.reset();
  now = 1000000;
  pacer.frame(now, false);
  pacer.released(now);
  for (unsigned i = 0; i < 600; ++i) {
    now += 2000;
    const int64_t wait = pacer.frame(now, false);
    now += ((wait + 9999) / 10000) * 10000;
    pacer.released(now);
  }
  assert(now >= 11000000 && now < 11010000);
  assert(pacer.renderingPercent() == 100);

  // Deterministic cost model, not an ESP32 benchmark: 9 ms CPU + 15 ms drawing.
  // Match upstream UCount so expensive rendered frames alternate with cheap ones.
  pacer.reset();
  now = 1000000;
  pacer.frame(now, false);
  pacer.released(now);
  unsigned credit = 100;
  int64_t settledStart = 0;
  for (unsigned i = 0; i < 600; ++i) {
    if (i == 300) settledStart = now;
    const bool draw = credit >= 100;
    if (draw) credit -= 100;
    now += 9000 + (draw ? 15000 : 0);
    now += pacer.frame(now, false);
    pacer.released(now);
    credit += pacer.renderingPercent();
  }
  const double simulatedFps = 300000000.0 / (now - settledStart);
  assert(simulatedFps > 59.0 && simulatedFps < 61.0);
  assert(pacer.renderingPercent() < 100);
  printf("Synthetic cost model only: full-render=41.7 fps, adaptive=%.1f fps (draw=%u%%).\n",
         simulatedFps, pacer.renderingPercent());
  puts("PASS: PAL/NTSC pacing, overload/recovery, menu reset and RTOS tick jitter.");
}
