#include "A1stCore.h"
#include "OpenMsxYM2413_2.h"
#include "Ay8910Phase.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
extern "C" {
#include "Board.h"
extern UInt32* boardSysTime;
}

static void require(bool value, const char* message)
{
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

static void psg(unsigned reg, unsigned value)
{
    a1stWritePort(0xa0, reg);
    a1stWritePort(0xa1, value);
}

static void fm(unsigned reg, unsigned value)
{
    a1stWritePort(0x7c, reg);
    a1stWritePort(0x7d, value);
}

static std::vector<int16_t> capture()
{
    std::vector<int16_t> result;
    for (unsigned block = 0; block < 40; ++block) {
        *boardSysTime += boardFrequency() / 100;
        mixerSync(boardGetMixer());
        UInt32 count;
        const Int16* samples = mixerGetBuffer(boardGetMixer(), &count);
        result.insert(result.end(), samples, samples + count);
    }
    return result;
}

static double frequency(const std::vector<int16_t>& samples, unsigned rate)
{
    unsigned crossings = 0, peak = 0, clipped = 0;
    const unsigned start = rate / 10;
    for (unsigned i = start + 1; i < samples.size(); ++i) {
        if (samples[i - 1] <= 0 && samples[i] > 0) ++crossings;
        peak = std::max(peak, unsigned(std::abs(int(samples[i]))));
        if (std::abs(int(samples[i])) >= 32767) ++clipped;
    }
    require(peak > 512 && !clipped, "Synthetic tone is audible without clipping.");
    return crossings * double(rate) / (samples.size() - start);
}

static unsigned peak(const std::vector<int16_t>& samples)
{
    unsigned result = 0;
    for (int16_t sample : samples) result = std::max(result, unsigned(std::abs(int(sample))));
    return result;
}

void a1stAudioTests()
{
    UInt32 random = 0x12345678;
    for (unsigned test = 0; test < 200000; ++test) {
        random = random * 1664525U + 1013904223U;
        UInt32 phase = random;
        random = random * 1664525U + 1013904223U;
        const UInt32 special[] = {0, 1, 0x08000000U, 0x08000001U, 0x80000000U, 0xffffffffU};
        const UInt32 step = test < 6 ? special[test] : random;
        UInt32 reference = phase, expected = 0;
        for (unsigned sample = 0; sample < 16; ++sample) {
            reference += step;
            expected += reference >> 31;
        }
        require(ay8910ToneSamples(&phase, step) == expected && phase == reference,
                "Fast PSG oversampling exactly matches all 16 original phase updates.");
    }
    for (unsigned period = 1; period < 4096; ++period) {
        const UInt32 step = (0x28959becU * 2) / period;
        const UInt32 starts[] = {0, 0x7fffffffU, 0x80000000U, 0xffffffffU, 0x7ffffffeU};
        for (UInt32 start : starts) {
            UInt32 phase = start, reference = start, expected = 0;
            for (unsigned sample = 0; sample < 16; ++sample) {
                reference += step;
                expected += reference >> 31;
            }
            require(ay8910ToneSamples(&phase, step) == expected && phase == reference,
                    "Fast PSG math matches every valid tone period at phase boundaries.");
        }
    }
    {
        OpenYM2413_2 idle("idle-fm", 100, 0);
        idle.setSampleRate(22050, 1);
        idle.setVolume(29490);
        for (unsigned block = 0; block < 1000; ++block)
            require(idle.updateBuffer(441) == nullptr, "Inactive FM skips synthesis instead of rendering silent operators.");
        idle.writeReg(0, 0x21, 0); idle.writeReg(1, 0x21, 0);
        idle.writeReg(2, 0x3f, 0); idle.writeReg(4, 0xf0, 0); idle.writeReg(5, 0xf0, 0);
        idle.writeReg(0x10, 0x20, 0); idle.writeReg(0x20, 0x19, 0);
        require(idle.updateBuffer(441) != nullptr, "FM key-on immediately resumes synthesis.");
        idle.reset(0);
        require(idle.updateBuffer(441) == nullptr, "FM reset returns to a cleared silent fast path.");
    }
    char error[256];
    require(a1stSetSoundSources(A1ST_SOUND_ALL, error, sizeof(error)), error);
    UInt32 ignored;
    mixerGetBuffer(boardGetMixer(), &ignored);
    psg(7, 0x3e);
    psg(8, 15); psg(9, 0); psg(10, 0);
    psg(0, 254); psg(1, 0);
    for (unsigned rate : {22050U, 44100U, 22050U}) {
        mixerSetSampleRate(boardGetMixer(), rate);
        const double measured = frequency(capture(), rate);
        std::printf("PSG sample rate %u: %.2f Hz (expected 440.40 Hz)\n", rate, measured);
        require(std::abs(measured - 440.40) < 6,
                "PSG pitch follows its 1.789 MHz clock at every mixer sample rate.");
    }
    require(a1stSetSoundSources(A1ST_SOUND_ALL & ~A1ST_SOUND_PSG, error, sizeof(error)), error);
    require(peak(capture()) == 0, "PSG can be muted without changing guest registers.");
    A1stAudioDebug debug;
    a1stWritePort(0xa0, 1);
    a1stGetAudioDebug(&debug);
    require(debug.psg[8] == 15 && a1stReadPort(0xa2) == debug.psg[1],
            "Debug snapshots preserve PSG levels and the selected register.");
    require(a1stSetSoundSources(A1ST_SOUND_ALL, error, sizeof(error)), error);
    require(std::abs(frequency(capture(), 22050) - 440.4) < 6, "PSG resumes at the correct pitch.");
    psg(8, 0);
    for (unsigned channel : {1U, 2U}) {
        const unsigned period = channel == 1 ? 127 : 339;
        psg(7, 0x3f ^ (1U << channel));
        psg(channel * 2, period & 255); psg(channel * 2 + 1, period >> 8);
        psg(8 + channel, 15);
        const double measured = frequency(capture(), 22050);
        require(std::abs(measured - 3579545.0 / 32 / period) < 6,
                "All three PSG tone channels use the corrected clock.");
        psg(8 + channel, 0);
    }
    capture();
    fm(0, 0x21); fm(1, 0x21); fm(2, 0x3f); fm(3, 0);
    fm(4, 0xf0); fm(5, 0xf0); fm(6, 0); fm(7, 0);
    fm(0x30, 0); fm(0x10, 0x20); fm(0x20, 0x19);
    const double measured = frequency(capture(), 22050);
    require(std::abs(measured - 436.96) < 6, "MSX-MUSIC keeps its correct native pitch.");
    require(a1stSetSoundSources(A1ST_SOUND_ALL & ~A1ST_SOUND_MSXMUSIC, error, sizeof(error)), error);
    require(peak(capture()) == 0, "MSX-MUSIC can be muted independently.");
    require(a1stSetSoundSources(A1ST_SOUND_ALL, error, sizeof(error)), error);
    require(std::abs(frequency(capture(), 22050) - 436.96) < 6, "FM resumes without resetting guest registers.");
    fm(7, 0x0f);
    fm(0x20, 0);
    capture();
    require(a1stSetSoundSources(A1ST_SOUND_PCM | A1ST_SOUND_CLICK, error, sizeof(error)), error);
    a1stWritePort(0xa5, 2);
    a1stWritePort(0xa4, 255);
    require(peak(capture()) > 512, "PCM output is audible when enabled.");
    require(a1stSetSoundSources(A1ST_SOUND_CLICK, error, sizeof(error)), error);
    a1stWritePort(0xa4, 0);
    require(peak(capture()) == 0, "PCM can be muted independently.");
    a1stWritePort(0xab, 0x82);
    capture();
    a1stWritePort(0xaa, 0x80);
    require(peak(capture()) > 512, "Keyboard click remains audible with PCM disabled.");
    require(a1stSetSoundSources(0, error, sizeof(error)), error);
    a1stWritePort(0xaa, 0);
    require(peak(capture()) == 0, "Keyboard click can be muted separately from PCM.");
    require(!a1stSetSoundSources(64, error, sizeof(error)), "Invalid audio mask is reported.");
    require(a1stSetSoundSources(A1ST_SOUND_ALL, error, sizeof(error)), error);
    a1stWritePort(0xa4, 128);
    capture();
    std::puts("A1ST PSG/MSX-MUSIC pitch and clipping regressions passed.");
    std::puts("A1ST independent PSG/FM/PCM/click mute and side-effect-free debug snapshots passed.");
}
