#include "MsxJoystickMapping.h"

#include <assert.h>
#include <initializer_list>
#include <stdio.h>
#include <string.h>
#include <type_traits>

namespace {
const uint8_t directions[9] = {0, 1, 9, 8, 10, 2, 6, 4, 5};
typedef MsxJoystickSample Samples[11];

MsxJoystickMapping build(uint8_t length, const Samples &samples)
{
    MsxJoystickMapping result = {};
    char error[128] = {};
    if (!MsxBuildJoystickMapping(length, samples, result, error, sizeof(error))) {
        fprintf(stderr, "Calibration failed: %s\n", error);
        assert(false);
    }
    assert(!error[0]);
    assert(MsxValidJoystickMapping(result));
    return result;
}

void check(const MsxJoystickMapping &m, const uint8_t *report, uint8_t expected)
{
    uint8_t result = 255;
    const bool decoded = MsxDecodeJoystickReport(m, report, m.length, result);
    if (!decoded || result != expected) {
        fprintf(stderr, "Decode expected %02x, got %02x (%s), report:", expected, result, decoded ? "valid" : "invalid");
        for (unsigned i = 0; i < m.length; ++i) fprintf(stderr, " %02x", report[i]);
        fprintf(stderr, "\n");
    }
    assert(decoded);
    assert(result == expected);
}

void reject(const MsxJoystickMapping &m, const uint8_t *report, size_t length)
{
    uint8_t result = 255;
    assert(!MsxDecodeJoystickReport(m, report, length, result));
    assert(result == 0);
}

void rejectedBuild(uint8_t length, const Samples &samples)
{
    MsxJoystickMapping result;
    memset(&result, 0xff, sizeof(result));
    char error[128] = {};
    assert(!MsxBuildJoystickMapping(length, samples, result, error, sizeof(error)));
    assert(error[0]);
    assert(!MsxValidJoystickMapping(result));
    const MsxJoystickMapping empty = {};
    assert(!memcmp(&result, &empty, sizeof(result)));
}

void testPackedDpad()
{
    Samples samples = {};
    for (unsigned p = 0; p < 9; ++p) samples[p].bytes[0] = directions[p];
    samples[9].bytes[0] = 0x10;
    samples[10].bytes[0] = 0x20;
    const MsxJoystickMapping m = build(1, samples);
    for (unsigned p = 0; p < 9; ++p) {
        for (unsigned b = 0; b < 4; ++b) {
            const uint8_t report = directions[p] | (b << 4);
            check(m, &report, report);
        }
    }
    for (uint8_t contradiction : {uint8_t(3), uint8_t(12), uint8_t(15)})
        reject(m, &contradiction, 1);
    uint8_t extraUnusedButton = 0xc1;
    check(m, &extraUnusedButton, 1);
}

void testPackedHatAndActiveLowButtons()
{
    Samples samples = {};
    // Nonstandard but distinct clockwise hat encoding.
    const uint8_t hat[9] = {15, 7, 2, 5, 0, 6, 3, 1, 4};
    for (unsigned p = 0; p < 11; ++p) {
        samples[p].bytes[0] = 9; // Report ID.
        samples[p].bytes[1] = 0x30 | hat[p < 9 ? p : 0];
        samples[p].bytes[2] = uint8_t(p * 17);
        samples[p].changed[2] = 0xff; // Free-running counter.
    }
    samples[9].bytes[1] &= ~0x10;
    samples[10].bytes[1] &= ~0x20;
    const MsxJoystickMapping m = build(3, samples);
    assert(m.fixedMask[0] == 0xff);
    assert(m.ignored[2] == 0xff);
    for (unsigned p = 0; p < 9; ++p) {
        for (unsigned b = 0; b < 4; ++b) {
            uint8_t report[3] = {9, uint8_t(hat[p] | (0x30 ^ (b << 4))), 241};
            check(m, report, directions[p] | (b << 4));
        }
    }
    uint8_t report[4] = {9, 0x3e, 0, 0};
    reject(m, report, 3); // Unknown hat code.
    report[1] = 0x3f;
    report[0] = 8;
    reject(m, report, 3); // Wrong report ID.
    report[0] = 9;
    reject(m, report, 2);
    reject(m, report, 4);
    reject(m, 0, 3);
}

Samples &axisSamples(Samples &samples, bool signedAxes, bool reversed)
{
    memset(samples, 0, sizeof(samples));
    for (unsigned p = 0; p < 11; ++p) {
        const uint8_t bits = p < 9 ? directions[p] : 0;
        samples[p].bytes[0] = 4;
        for (unsigned a = 0; a < 2; ++a) {
            const uint8_t negative = a ? 1 : 4, positive = a ? 2 : 8;
            const bool diagonal = (bits & 3) && (bits & 12);
            int offset = (bits & negative) ? -(diagonal ? 80 : 120) :
                         (bits & positive) ? (diagonal ? 80 : 120) : 0;
            if (reversed) offset = -offset;
            samples[p].bytes[1 + a] = uint8_t(offset + (signedAxes ? 0 : 128));
        }
        samples[p].bytes[3] = p == 9 ? 0x40 : p == 10 ? 2 : 0;
        samples[p].bytes[4] = uint8_t(p * 23);
        samples[p].changed[4] = 0xff;
    }
    return samples;
}

void testAxes()
{
    for (unsigned signedAxes = 0; signedAxes < 2; ++signedAxes) {
        for (unsigned reversed = 0; reversed < 2; ++reversed) {
            Samples samples;
            axisSamples(samples, signedAxes != 0, reversed != 0);
            const MsxJoystickMapping m = build(5, samples);
            assert(m.axes[0].byteIndex == 1);
            assert(m.axes[1].byteIndex == 2);
            assert(m.axes[0].signFlip == (signedAxes ? 0x80 : 0));
            for (unsigned p = 0; p < 11; ++p)
                check(m, samples[p].bytes, p < 9 ? directions[p] : uint8_t(0x10 << (p - 9)));
            const int center = signedAxes ? 0 : 128;
            uint8_t report[5] = {4, uint8_t(center + 10), uint8_t(center - 10), 0, 255};
            check(m, report, 0); // Neutral deadzone.
            report[1] = uint8_t(center + (reversed ? 70 : -70));
            report[2] = uint8_t(center + (reversed ? -70 : 70));
            report[3] = 0x42;
            check(m, report, 6 | 0x30); // Intermediate down-left with both buttons.
            report[1] = report[2] = uint8_t(center);
            check(m, report, 0x30);
        }
    }
}

void testNoiseAndIndependentModels()
{
    Samples samples = {};
    for (unsigned p = 0; p < 11; ++p) {
        samples[p].bytes[0] = 1;
        samples[p].bytes[1] = p < 9 ? directions[p] : p == 9 ? 0x10 : 0x20;
        samples[p].bytes[1] |= (p & 1) ? 0x80 : 0;
    }
    // Variation observed in any one capture is ignored in all captures.
    samples[4].changed[1] = 0x80;
    const MsxJoystickMapping digital = build(2, samples);
    Samples analogSamples;
    axisSamples(analogSamples, true, false);
    const MsxJoystickMapping analog = build(5, analogSamples);
    uint8_t digitalReport[2] = {1, 0xb9};
    check(digital, digitalReport, 0x39);
    digitalReport[1] &= ~0x80;
    check(digital, digitalReport, 0x39);
    check(analog, analogSamples[5].bytes, 2);
    check(digital, digitalReport, 0x39);
    reject(analog, digitalReport, sizeof(digitalReport));

    MsxJoystickMapping restored;
    uint8_t persisted[sizeof(restored)];
    memcpy(persisted, &digital, sizeof(digital));
    memcpy(&restored, persisted, sizeof(restored));
    check(restored, digitalReport, 0x39);
}

void testHybridAndMultibitButtons()
{
    Samples samples = {};
    for (unsigned p = 0; p < 11; ++p) {
        const uint8_t bits = p < 9 ? directions[p] : 0;
        samples[p].bytes[0] = 3;
        samples[p].bytes[1] = (bits & 4) ? 0 : (bits & 8) ? 248 : 128;
        samples[p].bytes[2] = bits & 3;
        samples[p].bytes[7] = p == 9 ? 0x30 : p == 10 ? 0xc0 : 0;
        samples[p].changed[1] = 3; // Low analog jitter bits.
    }
    const MsxJoystickMapping m = build(8, samples);
    assert(m.axes[0].byteIndex == 1);
    assert(m.axes[1].byteIndex == 0xff);
    for (unsigned p = 0; p < 11; ++p)
        check(m, samples[p].bytes, p < 9 ? directions[p] : uint8_t(0x10 << (p - 9)));
    uint8_t report[8] = {3, 191, 1, 0, 0, 0, 0, 0xf0};
    check(m, report, 9 | 0x30);
    report[7] = 0x10; // Half of a multi-bit button signature is not a press.
    reject(m, report, 8);
    report[7] = 0;
    report[2] = 3; // Opposite vertical directions are not a learned direction.
    reject(m, report, 8);
}

void testRejectedAndInvalid()
{
    Samples samples = {};
    for (unsigned p = 0; p < 9; ++p) samples[p].bytes[0] = directions[p];
    samples[9].bytes[0] = 0x10;
    samples[10].bytes[0] = 0x20;
    const MsxJoystickMapping valid = build(1, samples);
    rejectedBuild(0, samples);
    rejectedBuild(9, samples);
    samples[2] = samples[1];
    rejectedBuild(1, samples);
    samples[2].bytes[0] = directions[2];
    samples[9].bytes[0] = 1;
    rejectedBuild(1, samples); // Button also moves up.
    samples[9].bytes[0] = 0x20;
    rejectedBuild(1, samples); // Both buttons identical.
    samples[9].bytes[0] = 0x10;
    samples[10].bytes[0] = 0;
    rejectedBuild(1, samples); // No second button.
    samples[10].bytes[0] = 0x20;
    samples[0].changed[0] = 0x0f;
    rejectedBuild(1, samples); // Directions lost in capture noise.

    for (unsigned corruption = 0; corruption < 8; ++corruption) {
        MsxJoystickMapping invalid = valid;
        switch (corruption) {
        case 0: invalid.version = 255; break;
        case 1: invalid.length = 9; break;
        case 2: invalid.buttonMask[0][0] = 0; break;
        case 3: invalid.buttonMask[1][0] |= 0x10; break;
        case 4: invalid.axes[0].byteIndex = 8; break;
        case 5: invalid.directions[0][0] = 0x80; break;
        case 6: invalid.ignored[0] = 0xff; break;
        case 7: invalid.directionMask[7] = 1; break;
        }
        assert(!MsxValidJoystickMapping(invalid));
        reject(invalid, samples[0].bytes, invalid.length);
    }
    Samples analog;
    axisSamples(analog, false, false);
    MsxJoystickMapping invalidAxis = build(5, analog);
    invalidAxis.axes[0].highBit = 1;
    assert(!MsxValidJoystickMapping(invalidAxis));
    invalidAxis = build(5, analog);
    invalidAxis.axes[0].lowThreshold = invalidAxis.axes[0].highThreshold;
    assert(!MsxValidJoystickMapping(invalidAxis));

    char tiny[1] = {'x'};
    MsxJoystickMapping empty;
    assert(!MsxBuildJoystickMapping(0, samples, empty, tiny, sizeof(tiny)));
    assert(tiny[0] == 0);
    assert(!MsxBuildJoystickMapping(0, samples, empty, 0, 0));
}
}

static void testSevenStepCalibration()
{
    MsxJoystickSample poses[7] = {};
    // Actual working CP400 report layout supplied for VID 2e24 / PID 386a.
    const uint8_t neutral[] = {0x7f,0x7f,0x80,0x80,0x80,0x0f,0,0};
    for (auto &pose : poses) memcpy(pose.bytes, neutral, sizeof(neutral));
    poses[1].bytes[1] = 0;    // Up
    poses[2].bytes[0] = 255;  // Right
    poses[3].bytes[1] = 255;  // Down
    poses[4].bytes[0] = 0;    // Left
    poses[5].bytes[5] = 0x8f; // A
    poses[6].bytes[5] = 0x1f; // B
    MsxJoystickMapping m;
    char error[160];
    assert(MsxBuildJoystickCardinalMapping(8, poses, m, error, sizeof(error)));
    for (unsigned direction = 0; direction < 9; ++direction)
        for (unsigned buttons = 0; buttons < 4; ++buttons) {
            uint8_t report[8];
            memcpy(report, neutral, sizeof(report));
            report[0] = directions[direction] & 4 ? 0 : directions[direction] & 8 ? 255 : 127;
            report[1] = directions[direction] & 1 ? 0 : directions[direction] & 2 ? 255 : 127;
            report[5] |= (buttons & 1 ? 0x80 : 0) | (buttons & 2 ? 0x10 : 0);
            check(m, report, directions[direction] | (buttons << 4));
        }
    for (unsigned base = 0; base <= 1; ++base)
        for (unsigned reverseDirection = 0; reverseDirection < 2; ++reverseDirection) {
            memset(poses, 0, sizeof(poses));
            const uint8_t hatNeutral = base ? 0 : 15;
            for (auto &pose : poses) pose.bytes[0] = hatNeutral;
            const unsigned step = reverseDirection ? 6 : 2;
            for (unsigned i = 0; i < 4; ++i) poses[i + 1].bytes[0] = ((step * i) & 7) + base;
            poses[5].bytes[0] = hatNeutral | 0x10;
            poses[6].bytes[0] = hatNeutral | 0x20;
            assert(MsxBuildJoystickCardinalMapping(1, poses, m, error, sizeof(error)));
            for (unsigned i = 0; i < 8; ++i) {
                const uint8_t raw = (((reverseDirection ? 8 - i : i) & 7) + base) | 0x30;
                check(m, &raw, directions[1 + i] | 0x30);
            }
        }
    memset(poses, 0, sizeof(poses));
    poses[1].bytes[0] = 1;
    poses[2].bytes[0] = 8;
    poses[3].bytes[0] = 2;
    poses[4].bytes[0] = 4;
    poses[5].bytes[0] = 0x10;
    poses[6].bytes[0] = 0x20;
    assert(MsxBuildJoystickCardinalMapping(1, poses, m, error, sizeof(error)));
    for (uint8_t raw : {uint8_t(9), uint8_t(10), uint8_t(6), uint8_t(5)})
        check(m, &raw, raw);
    poses[6] = poses[5];
    assert(!MsxBuildJoystickCardinalMapping(1, poses, m, error, sizeof(error)));
    assert(!MsxValidJoystickMapping(m));
    puts("PASS: seven-step calibration for real 2e24:386a pad, derived diagonals, hats and button combinations.");
}

int main()
{
    static_assert(std::is_trivial<MsxJoystickMapping>::value, "Mapping must be POD");
    static_assert(std::is_standard_layout<MsxJoystickMapping>::value, "Mapping must be serializable");
    static_assert(sizeof(MsxJoystickMapping) == 152, "Update persistence version if layout changes");
    testPackedDpad();
    testPackedHatAndActiveLowButtons();
    testAxes();
    testNoiseAndIndependentModels();
    testHybridAndMultibitButtons();
    testRejectedAndInvalid();
    testSevenStepCalibration();
    puts("Joystick mapping tests passed.");
}
