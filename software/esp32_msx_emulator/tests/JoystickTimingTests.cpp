#include "../../esp32_cp400_emulator/lib/SoftUSB/msx_timing.h"
#include "../../esp32_cp400_emulator/lib/SoftUSB/msx_receive.h"
#include <assert.h>
#include <stdio.h>

int main()
{
    assert(msx_usb_bit_cycles(240) == 160);
    assert(msx_usb_bit_cycles(120) == 80);
    assert(msx_usb_bit_cycles(0) == 0);
    assert(msx_usb_bit_cycles(80) == 0);
    assert(msx_usb_bit_cycles(160) == 0);
    assert(msx_usb_bit_cycles(241) == 0);
    assert(msx_usb_bit_cycles(UINT32_MAX) == 0);
    assert(msx_usb_before_deadline(159, 160));
    assert(!msx_usb_before_deadline(160, 160));
    assert(!msx_usb_before_deadline(161, 160));
    assert(msx_usb_before_deadline(UINT32_MAX - 10, 20));
    assert(!msx_usb_before_deadline(20, UINT32_MAX - 10));
    // Absolute scheduling does not accumulate per-bit GPIO/loop overhead.
    const uint32_t starts[] = {1000, UINT32_MAX - 500};
    for (uint32_t start : starts) {
        uint32_t deadline = start + 160;
        for (unsigned bit = 0; bit < 255; ++bit) {
            uint32_t now = deadline - 30;
            while (msx_usb_before_deadline(now, deadline)) now += 3;
            assert(now == deadline);
            deadline += 160;
        }
        assert(uint32_t(deadline - start) == 256 * 160);
    }
    puts("PASS: exact USB cycle periods, fixed deadlines, maximum packet length and counter wraparound.");

    uint16_t samples[256] = {};
    msx_usb_capture_t capture;
    const uint32_t dm = 1U << 15, dp = 1U << 16;
    msx_usb_capture_begin(&capture, samples, dm, 15, 0, 1920);
    assert(msx_usb_capture_sample(&capture, dm, 1919));
    assert(!msx_usb_capture_sample(&capture, dm, 1920));
    assert(capture.count == 1 && !capture.overflow);
    msx_usb_capture_begin(&capture, samples, dm, 15, UINT32_MAX - 100, 1920);
    assert(msx_usb_capture_sample(&capture, dp, 59)); // Wraparound, one bit later.
    assert(capture.count == 2 && (samples[1] >> 8) == 2);
    assert(msx_usb_capture_sample(&capture, dm, 219));
    assert(!msx_usb_capture_sample(&capture, 0, 379)); // EOP captured, not discarded.
    assert(capture.count == 4 && (samples[3] >> 8) == 0);
    assert(((samples[2] - samples[1]) & 255) == 20); // 160 cycles / 8.
    msx_usb_capture_begin(&capture, samples, dm, 15, 0, 1920);
    for (unsigned i = 1; i < 255; ++i)
        assert(msx_usb_capture_sample(&capture, i & 1 ? dp : dm, i * 160));
    assert(!msx_usb_capture_sample(&capture, dp, 255 * 160));
    assert(capture.overflow && capture.count == 0);
    puts("PASS: receive timeout, EOP timestamps, bounded edge buffer and cycle-counter rollover.");
}
