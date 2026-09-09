#include "MsxJoystickMapping.h"

#include <string.h>

namespace {
const uint8_t poseBits[9] = {0, 1, 9, 8, 10, 2, 6, 4, 5};

bool fail(char *error, size_t size, const char *text)
{
    if (error && size) {
        strncpy(error, text, size - 1);
        error[size - 1] = '\0';
    }
    return false;
}

uint8_t axisBits(const MsxJoystickAxis &axis, const uint8_t *report)
{
    if (axis.byteIndex == 0xff) return 0;
    const uint8_t value = (report[axis.byteIndex] ^ axis.signFlip) & axis.mask;
    if (value <= axis.lowThreshold) return axis.lowBit;
    if (value >= axis.highThreshold) return axis.highBit;
    return 0;
}

bool learnAxis(const MsxJoystickSample (&poses)[11], uint8_t byte,
               uint8_t mask, uint8_t negativeBit, uint8_t positiveBit,
               MsxJoystickAxis &axis)
{
    // Try both unsigned and two's-complement ordering, including reversed axes.
    for (unsigned flip = 0; flip < 2; ++flip) {
        uint8_t minimum[3] = {255, 255, 255};
        uint8_t maximum[3] = {0, 0, 0};
        for (unsigned p = 0; p < 11; ++p) {
            const uint8_t bits = p < 9 ? poseBits[p] : 0;
            const unsigned group = (bits & negativeBit) ? 0 :
                                   (bits & positiveBit) ? 2 : 1;
            const uint8_t value = (poses[p].bytes[byte] ^ (flip ? 0x80 : 0)) & mask;
            if (value < minimum[group]) minimum[group] = value;
            if (value > maximum[group]) maximum[group] = value;
        }
        for (unsigned reverse = 0; reverse < 2; ++reverse) {
            const unsigned low = reverse ? 2 : 0;
            const unsigned high = reverse ? 0 : 2;
            if (int(minimum[1]) - maximum[low] < 8 ||
                int(minimum[high]) - maximum[1] < 8) continue;
            axis.byteIndex = byte;
            axis.mask = mask;
            axis.signFlip = flip ? 0x80 : 0;
            axis.lowThreshold = (unsigned(maximum[low]) + minimum[1]) / 2;
            axis.highThreshold = (unsigned(maximum[1]) + minimum[high] + 1) / 2;
            axis.lowBit = reverse ? positiveBit : negativeBit;
            axis.highBit = reverse ? negativeBit : positiveBit;
            return true;
        }
    }
    return false;
}

bool decode(const MsxJoystickMapping &m, const uint8_t *report, uint8_t &result)
{
    uint8_t buttons = 0;
    for (unsigned b = 0; b < 2; ++b) {
        bool pressed = true, released = true;
        for (unsigned i = 0; i < m.length; ++i) {
            const uint8_t value = report[i] & m.buttonMask[b][i];
            pressed &= value == m.buttonPressed[b][i];
            released &= value == (m.buttonPressed[b][i] ^ m.buttonMask[b][i]);
        }
        if (!pressed && !released) return false;
        if (pressed) buttons |= 0x10 << b;
    }
    const uint8_t axes = axisBits(m.axes[0], report) | axisBits(m.axes[1], report);
    uint8_t axisMask = 0;
    for (unsigned a = 0; a < 2; ++a)
        if (m.axes[a].byteIndex != 0xff) axisMask |= m.axes[a].lowBit | m.axes[a].highBit;
    for (unsigned p = 0; p < 9; ++p) {
        if ((poseBits[p] & axisMask) != axes) continue;
        bool match = true;
        for (unsigned i = 0; i < m.length; ++i) {
            if ((report[i] & m.fixedMask[i]) != m.fixedValue[i] ||
                (report[i] & m.directionMask[i]) != m.directions[p][i]) {
                match = false;
                break;
            }
        }
        if (match) {
            result = poseBits[p] | buttons;
            return true;
        }
    }
    return false;
}
}

bool MsxValidJoystickMapping(const MsxJoystickMapping &m)
{
    if (m.version != MSX_JOYSTICK_MAPPING_VERSION || !m.length || m.length > 8)
        return false;
    uint8_t axisMask = 0;
    for (unsigned a = 0; a < 2; ++a) {
        const MsxJoystickAxis &axis = m.axes[a];
        if (axis.byteIndex == 0xff) {
            if (axis.mask || axis.signFlip || axis.lowThreshold ||
                axis.highThreshold || axis.lowBit || axis.highBit) return false;
            continue;
        }
        const uint8_t negative = a ? 1 : 4, positive = a ? 2 : 8;
        if (axis.byteIndex >= m.length || !axis.mask ||
            (axis.signFlip != 0 && axis.signFlip != 0x80) ||
            axis.lowThreshold >= axis.highThreshold ||
            axis.highThreshold > axis.mask ||
            !((axis.lowBit == negative && axis.highBit == positive) ||
              (axis.lowBit == positive && axis.highBit == negative))) return false;
        const unsigned i = axis.byteIndex;
        if (axis.mask & (m.ignored[i] | m.directionMask[i] | m.fixedMask[i] |
                        m.buttonMask[0][i] | m.buttonMask[1][i])) return false;
        axisMask |= negative | positive;
    }
    if (m.axes[0].byteIndex != 0xff &&
        m.axes[0].byteIndex == m.axes[1].byteIndex) return false;
    bool buttonUsed[2] = {false, false};
    for (unsigned i = 0; i < 8; ++i) {
        const uint8_t buttons = m.buttonMask[0][i] | m.buttonMask[1][i];
        if ((m.buttonMask[0][i] & m.buttonMask[1][i]) ||
            (m.directionMask[i] & buttons) ||
            (m.fixedMask[i] & (buttons | m.directionMask[i])) ||
            (m.ignored[i] & (buttons | m.directionMask[i] | m.fixedMask[i])) ||
            (m.fixedValue[i] & ~m.fixedMask[i])) return false;
        for (unsigned b = 0; b < 2; ++b) {
            if (m.buttonPressed[b][i] & ~m.buttonMask[b][i]) return false;
            buttonUsed[b] |= m.buttonMask[b][i] != 0;
        }
        for (unsigned p = 0; p < 9; ++p)
            if (m.directions[p][i] & ~m.directionMask[i]) return false;
        if (i >= m.length && (m.ignored[i] || m.fixedMask[i] ||
            m.directionMask[i] || buttons)) return false;
    }
    if (!buttonUsed[0] || !buttonUsed[1]) return false;
    // Every direction, including neutral, must have a unique complete signature.
    for (unsigned p = 0; p < 9; ++p) {
        for (unsigned q = p + 1; q < 9; ++q) {
            if ((poseBits[p] & axisMask) != (poseBits[q] & axisMask)) continue;
            if (!memcmp(m.directions[p], m.directions[q], 8)) return false;
        }
    }
    return true;
}

bool MsxBuildJoystickMapping(uint8_t length,
                             const MsxJoystickSample (&poses)[11],
                             MsxJoystickMapping &mapping,
                             char *error, size_t errorSize)
{
    memset(&mapping, 0, sizeof(mapping));
    if (error && errorSize) error[0] = '\0';
    if (!length || length > 8)
        return fail(error, errorSize, "Unsupported report length (expected 1-8 bytes).");
    MsxJoystickMapping m = {};
    m.version = MSX_JOYSTICK_MAPPING_VERSION;
    m.length = length;
    m.axes[0].byteIndex = m.axes[1].byteIndex = 0xff;
    uint8_t movement[8] = {};
    bool buttonUsed[2] = {false, false};
    for (unsigned i = 0; i < length; ++i) {
        for (unsigned p = 0; p < 11; ++p) m.ignored[i] |= poses[p].changed[i];
        for (unsigned p = 1; p < 9; ++p)
            movement[i] |= (poses[p].bytes[i] ^ poses[0].bytes[i]) & ~m.ignored[i];
        for (unsigned b = 0; b < 2; ++b) {
            m.buttonMask[b][i] = (poses[9 + b].bytes[i] ^ poses[0].bytes[i]) & ~m.ignored[i];
            m.buttonPressed[b][i] = poses[9 + b].bytes[i] & m.buttonMask[b][i];
            buttonUsed[b] |= m.buttonMask[b][i] != 0;
        }
        if ((m.buttonMask[0][i] & m.buttonMask[1][i]) ||
            (movement[i] & (m.buttonMask[0][i] | m.buttonMask[1][i])))
            return fail(error, errorSize, "Overlapping controls; hold only the requested direction or button.");
        m.directionMask[i] = movement[i];
    }
    if (!buttonUsed[0] || !buttonUsed[1])
        return fail(error, errorSize, "Buttons were not distinct from neutral; retry with two independent buttons.");

    // A constant first byte is treated as a report ID. Other unused controls stay free.
    bool constantFirst = !m.ignored[0];
    for (unsigned p = 1; p < 11; ++p)
        constantFirst &= poses[p].bytes[0] == poses[0].bytes[0];
    if (length > 1 && constantFirst) {
        m.fixedMask[0] = 0xff;
        m.fixedValue[0] = poses[0].bytes[0];
    }

    for (unsigned a = 0; a < 2; ++a) {
        for (unsigned i = 0; i < length; ++i) {
            if (!movement[i] || m.buttonMask[0][i] || m.buttonMask[1][i] ||
                (a && m.axes[0].byteIndex == i)) continue;
            if (learnAxis(poses, i, uint8_t(~m.ignored[i]),
                          a ? 1 : 4, a ? 2 : 8, m.axes[a])) {
                m.directionMask[i] = 0;
                break;
            }
        }
    }
    for (unsigned p = 0; p < 9; ++p)
        for (unsigned i = 0; i < length; ++i)
            m.directions[p][i] = poses[p].bytes[i] & m.directionMask[i];
    if (!MsxValidJoystickMapping(m))
        return fail(error, errorSize, "Directions were duplicate, unstable or ambiguous; repeat all eight held poses.");
    for (unsigned p = 0; p < 11; ++p) {
        uint8_t decoded = 0;
        const uint8_t expected = p < 9 ? poseBits[p] : uint8_t(0x10 << (p - 9));
        if (!decode(m, poses[p].bytes, decoded) || decoded != expected)
            return fail(error, errorSize, "Reports do not describe independent direction and button controls.");
    }
    mapping = m;
    return true;
}

bool MsxDecodeJoystickReport(const MsxJoystickMapping &mapping,
                            const uint8_t *report, size_t length,
                            uint8_t &result)
{
    result = 0;
    if (!report || length != mapping.length || !MsxValidJoystickMapping(mapping))
        return false;
    return decode(mapping, report, result);
}

bool MsxBuildJoystickCardinalMapping(uint8_t length,
                                     const MsxJoystickSample (&poses)[7],
                                     MsxJoystickMapping &mapping,
                                     char *error, size_t errorSize)
{
    memset(&mapping, 0, sizeof(mapping));
    if (!length || length > 8)
        return fail(error, errorSize, "Unsupported report length.");
    MsxJoystickSample expanded[11] = {};
    const unsigned index[] = {0, 1, 3, 5, 7, 9, 10};
    for (unsigned i = 0; i < 7; ++i) expanded[index[i]] = poses[i];
    for (unsigned diagonal = 0; diagonal < 4; ++diagonal)
        expanded[2 + diagonal * 2] = poses[0];
    for (unsigned byte = 0; byte < length; ++byte) {
        uint8_t noise = 0;
        for (unsigned i = 0; i < 7; ++i) noise |= poses[i].changed[byte];
        const uint8_t neutral = poses[0].bytes[byte];
        const uint8_t horizontal = ((poses[2].bytes[byte] ^ neutral) |
                                    (poses[4].bytes[byte] ^ neutral)) & ~noise;
        const uint8_t vertical = ((poses[1].bytes[byte] ^ neutral) |
                                  (poses[3].bytes[byte] ^ neutral)) & ~noise;
        const uint8_t overlap = horizontal & vertical;
        uint8_t hatMask = 0, hatValue[4] = {};
        if (overlap) {
            for (unsigned shift = 0; shift <= 4 && !hatMask; shift += 4) {
                const uint8_t mask = 15 << shift;
                if ((overlap & ~mask) || (noise & mask)) continue;
                for (unsigned base = 0; base <= 1 && !hatMask; ++base) {
                    const unsigned n = (neutral >> shift) & 15;
                    if (n >= base && n < base + 8) continue;
                    unsigned cardinal[4];
                    bool valid = true;
                    for (unsigned i = 0; i < 4; ++i) {
                        const unsigned raw = (poses[1 + i].bytes[byte] >> shift) & 15;
                        if (raw < base || raw >= base + 8) { valid = false; break; }
                        cardinal[i] = raw - base;
                    }
                    if (!valid) continue;
                    const unsigned step = (cardinal[1] - cardinal[0]) & 7;
                    if (step != 2 && step != 6) continue;
                    if (cardinal[2] != ((cardinal[0] + step * 2) & 7) ||
                        cardinal[3] != ((cardinal[0] + step * 3) & 7)) continue;
                    hatMask = mask;
                    for (unsigned i = 0; i < 4; ++i)
                        hatValue[i] = (((cardinal[i] + (step == 2 ? 1 : 7)) & 7) + base) << shift;
                }
            }
            if (!hatMask)
                return fail(error, errorSize, "Directions overlap or use an unsupported hat encoding; retry cardinal controls.");
        }
        for (unsigned i = 0; i < 4; ++i) {
            const unsigned h = i < 2 ? 2 : 4;
            const unsigned v = i == 0 || i == 3 ? 1 : 3;
            uint8_t value = (neutral & ~(horizontal | vertical)) |
                            (poses[h].bytes[byte] & horizontal) |
                            (poses[v].bytes[byte] & vertical);
            value = (value & ~hatMask) | hatValue[i];
            expanded[2 + i * 2].bytes[byte] = value;
            expanded[2 + i * 2].changed[byte] = noise;
        }
    }
    return MsxBuildJoystickMapping(length, expanded, mapping, error, errorSize);
}
