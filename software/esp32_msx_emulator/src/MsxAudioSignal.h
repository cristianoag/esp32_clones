#pragma once
#include <stdint.h>

class MsxAudioSignal
{
public:
    static constexpr unsigned IdleSamples = 22050 * 150 / 1000;
    static constexpr unsigned RampSamples = 128;

    void reset()
    {
        gain = 0;
        steadySamples = 0;
        lastSample = 128;
        asleep = true;
    }

    __attribute__((always_inline)) bool sleeping() const { return asleep; }
    bool needsWake(uint8_t sample) const { return sample != 128 && sample != lastSample; }

    __attribute__((always_inline)) uint8_t tick(bool hasSample, uint8_t sample)
    {
        if (!hasSample) sample = lastSample;
        const bool changed = hasSample && sample != lastSample;
        if (asleep && changed && sample != 128) asleep = false;
        if (changed) steadySamples = 0;
        else if (steadySamples < IdleSamples) ++steadySamples;
        lastSample = sample;
        if (asleep) return 0;
        if (steadySamples >= IdleSamples)
        {
            if (gain) --gain;
            if (!gain) asleep = true;
        }
        else if (gain < RampSamples) ++gain;
        return static_cast<uint8_t>(static_cast<unsigned>(sample) * gain / RampSamples);
    }

private:
    unsigned gain = 0;
    unsigned steadySamples = 0;
    uint8_t lastSample = 128;
    bool asleep = true;
};
