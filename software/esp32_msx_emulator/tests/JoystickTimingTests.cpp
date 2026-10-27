#include "../lib/MsxSoftUsb/upstream/msx_timing.h"
#include "../lib/MsxSoftUsb/upstream/msx_receive.h"
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
    msx_usb_capture_begin(&capture, samples, dm, dm | dp, 15, 0, 160, 1920);
    assert(msx_usb_capture_sample(&capture, dm, 1919));
    assert(!msx_usb_capture_sample(&capture, dm, 1920));
    assert(capture.count == 1 && !capture.overflow);
    msx_usb_capture_begin(&capture, samples, dm, dm | dp, 15, UINT32_MAX - 100, 160, 1920);
    assert(msx_usb_capture_sample(&capture, dp, 59)); // Wraparound, one bit later.
    assert(capture.count == 2 && (samples[1] >> 8) == 2);
    assert(msx_usb_capture_sample(&capture, dm, 219));
    assert(msx_usb_capture_sample(&capture, 0, 379));
    assert(capture.count == 3); // A single SE0 sample must not terminate data.
    assert(msx_usb_capture_sample(&capture, 0, 538));
    assert(!msx_usb_capture_sample(&capture, 0, 539));
    assert(capture.count == 4 && (samples[3] >> 8) == 0);
    assert(((samples[2] - samples[1]) & 255) == 20); // 160 cycles / 8.
    assert(capture.lastEdge == 379); // ACK timing uses EOP start, not confirmation.
    assert(capture.lastEdge + 3 * 160 - 539 == 2 * 160);
    msx_usb_capture_begin(&capture, samples, dm, dm | dp, 15, 0, 160, 1920);
    assert(msx_usb_capture_sample(&capture, 0, 160));
    assert(msx_usb_capture_sample(&capture, dp, 184));
    assert(capture.count == 2 && capture.lastEdge == 160);
    assert(msx_usb_capture_sample(&capture, dm | dp, 320));
    assert(msx_usb_capture_sample(&capture, dm, 344));
    assert(capture.count == 3 && capture.lastEdge == 320);
    assert(msx_usb_capture_sample(&capture, 0, 400));
    assert(msx_usb_capture_sample(&capture, dm, 424));
    assert(capture.count == 3 && capture.lastEdge == 320);
    assert(!msx_usb_capture_sample(&capture, dm, 2240)); // Glitches cannot extend inactivity.
    msx_usb_capture_begin(&capture, samples, dm, dm | dp, 15, 0, 160, 1920);
    assert(msx_usb_capture_sample(&capture, dm | dp, 160));
    assert(!msx_usb_capture_sample(&capture, dm | dp, 320));
    assert(capture.count == 1 && capture.pins == dm); // Invalid SE1 never becomes EOP/ACK.
    msx_usb_capture_begin(&capture, samples, 0, dm | dp, 15, 0, 160, 1920);
    assert(msx_usb_capture_sample(&capture, 0, 159));
    assert(!msx_usb_capture_sample(&capture, 0, 160) && capture.count == 1);
    msx_usb_capture_begin(&capture, samples, dm | dp, dm | dp, 15, 0, 160, 1920);
    assert(msx_usb_capture_sample(&capture, dm | dp, 159));
    assert(!msx_usb_capture_sample(&capture, dm | dp, 160) && capture.count == 1);
    msx_usb_capture_begin(&capture, samples, dm, dm | dp, 15, 0, 160, 1920);
    for (unsigned i = 1; i < 255; ++i)
        assert(msx_usb_capture_sample(&capture, i & 1 ? dp : dm, i * 160));
    assert(!msx_usb_capture_sample(&capture, dp, 255 * 160));
    assert(capture.overflow && capture.count == 0);
    puts("PASS: transition filtering, qualified EOP/ACK timestamps, SE1 rejection, bounded capture and rollover.");
}
