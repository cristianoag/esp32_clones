#pragma once
#include "MsxTypes.h"

static inline UInt32 ay8910ToneSamples(UInt32* phase, UInt32 step)
{
    const UInt32 start = *phase;
    const UInt32 end = start + step * 16;
    UInt32 high = 0;
    if (step <= 0x08000000U) {
        /* At most one half-cycle boundary is crossed by these 16 samples. */
        if ((start ^ end) & 0x80000000U) {
            const UInt32 before = (0x7fffffffU - (start & 0x7fffffffU)) / step;
            high = (start & 0x80000000U) ? before : 16 - before;
        }
        else high = (start & 0x80000000U) ? 16 : 0;
    }
    else {
        UInt32 current = start;
        unsigned sample;
        for (sample = 0; sample < 16; ++sample) {
            current += step;
            high += current >> 31;
        }
    }
    *phase = end;
    return high;
}
