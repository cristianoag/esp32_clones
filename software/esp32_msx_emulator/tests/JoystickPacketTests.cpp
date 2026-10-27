#include "../lib/MsxSoftUsb/upstream/msx_decode.h"
#include "../lib/MsxSoftUsb/upstream/msx_receive.h"
#include <assert.h>
#include <stdio.h>
#include <vector>

static uint8_t reverse(uint8_t value)
{
    uint8_t result = 0;
    for (unsigned i = 0; i < 8; ++i) { result = (result << 1) | (value & 1); value >>= 1; }
    return result;
}
static std::vector<uint16_t> encode(std::vector<uint8_t> bytes, unsigned idleBits)
{
    std::vector<uint16_t> edges;
    unsigned tick = 253, state = 1, ones = 0;
    edges.push_back((state << 8) | (tick & 255));
    tick += idleBits * 20;
    auto bit = [&](bool one) {
        if (!one) { state ^= 3; edges.push_back((state << 8) | (tick & 255)); }
        tick += 20;
    };
    for (uint8_t byte : bytes)
        for (unsigned i = 0; i < 8; ++i) {
            const bool one = (byte >> i) & 1;
            bit(one);
            ones = one ? ones + 1 : 0;
            if (ones == 6) { bit(false); ones = 0; }
        }
    edges.push_back(tick & 255);
    return edges;
}
static int decode(const std::vector<uint16_t> &edges, uint8_t *bytes, unsigned &length)
{
    return msx_usb_decode_edges(edges.data(), edges.size(), 1, 2, 51, bytes, 16, &length);
}
struct WireEvent
{
    unsigned cycles;
    uint32_t pins;
};

static std::vector<uint8_t> dataPacket(uint8_t pid, const std::vector<uint8_t> &payload)
{
    std::vector<uint8_t> bytes = {0x80, pid};
    unsigned crc = 0xffff;
    for (uint8_t value : payload) {
        bytes.push_back(value);
        crc ^= value;
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = crc & 1 ? (crc >> 1) ^ 0xa001 : crc >> 1;
    }
    crc ^= 0xffff;
    bytes.push_back(crc & 255);
    bytes.push_back(crc >> 8);
    return bytes;
}

static int samplePacket(const std::vector<uint8_t> &wire, unsigned glitchCycles,
                        unsigned sampleCycles, unsigned phase, uint32_t start,
                        uint8_t *bytes, unsigned &length, unsigned wireBitCycles = 160,
                        unsigned port = 0)
{
    assert(port < 2);
    const unsigned shift = 15 + port * 2;
    const uint32_t dm = 1U << shift, dp = 1U << (shift + 1);
    constexpr unsigned bitCycles = 160;
    std::vector<WireEvent> events = {{0, dm}};
    unsigned cursor = 4 * wireBitCycles, state = 1, ones = 0;
    auto bit = [&](bool one, bool data) {
        if (!one) {
            state ^= 3;
            if (data && glitchCycles) {
                events.push_back({cursor, state == 2 ? 0U : dm | dp});
                events.push_back({cursor + glitchCycles, state == 1 ? dm : dp});
            } else events.push_back({cursor, state == 1 ? dm : dp});
        }
        cursor += wireBitCycles;
    };
    for (size_t byte = 0; byte < wire.size(); ++byte)
        for (unsigned b = 0; b < 8; ++b) {
            const bool one = (wire[byte] >> b) & 1;
            bit(one, byte >= 2);
            ones = one ? ones + 1 : 0;
            if (ones == 6) { bit(false, byte >= 2); ones = 0; }
        }
    events.push_back({cursor, 0});
    events.push_back({cursor + 2 * wireBitCycles, dm});
    uint16_t samples[256] = {};
    msx_usb_capture_t capture;
    msx_usb_capture_begin(&capture, samples, dm, dm | dp, shift, start, bitCycles, 12 * bitCycles);
    size_t event = 0;
    for (unsigned elapsed = phase; elapsed < cursor + 16 * bitCycles; elapsed += sampleCycles) {
        while (event + 1 < events.size() && events[event + 1].cycles <= elapsed) ++event;
        if (!msx_usb_capture_sample(&capture, events[event].pins, start + elapsed)) {
            assert(!capture.overflow);
            return msx_usb_decode_edges(samples, capture.count, 1, 2, 51, bytes, 16, &length);
        }
    }
    assert(false && "USB capture did not terminate.");
    return 0;
}

static void descriptorCaptureRegression()
{
    // The device descriptor travels as DATA1(8), DATA0(8), DATA1(2).
    const std::vector<uint8_t> descriptor = {
        18, 1, 0x10, 1, 0, 0, 0, 8, 0x34, 0x12, 0x78, 0x56, 0, 1, 1, 2, 0, 1
    };
    for (unsigned port : {0U, 1U})
      for (unsigned offset : {0U, 8U, 16U}) {
        const unsigned end = offset + 8 < descriptor.size() ? offset + 8 : descriptor.size();
        const std::vector<uint8_t> payload(descriptor.begin() + offset, descriptor.begin() + end);
        const auto packet = dataPacket(offset == 8 ? 0xc3 : 0x4b, payload);
        uint8_t bytes[16];
        unsigned length;
        assert(samplePacket(packet, 0, 8, 0, 0, bytes, length, 160, port) == 0x7b);
        for (unsigned wireClock : {158U, 160U, 162U})
          for (unsigned glitch : {8U, 24U, 50U})
            for (unsigned period : {8U, 24U, 48U, 64U})
                for (unsigned phase = 0; phase < period; ++phase) {
                    const int result = samplePacket(packet, glitch, period, phase,
                                                    UINT32_MAX - 1000, bytes, length, wireClock, port);
                    if (result != 0x7b)
                        fprintf(stderr, "Descriptor capture: port=%u offset=%u clock=%u glitch=%u sample=%u phase=%u result=%02x\n",
                                port + 1, offset, wireClock, glitch, period, phase, result);
                    assert(result == 0x7b && length == packet.size());
                    for (unsigned i = 0; i < length; ++i) assert(bytes[i] == reverse(packet[i]));
                }
    }
    uint8_t bytes[16];
    unsigned length;
    for (unsigned size = 0; size <= 8; ++size)
        for (uint8_t value : {uint8_t(0), uint8_t(0xff), uint8_t(0x7f)}) {
            auto packet = dataPacket(0x4b, std::vector<uint8_t>(size, value));
            assert(samplePacket(packet, 50, 64, 17, 0, bytes, length, 162) == 0x7b);
            packet.back() ^= 1;
            assert(samplePacket(packet, 50, 64, 17, 0, bytes, length, 162) == 0x7f);
        }
    for (uint8_t pid : {uint8_t(0xd2), uint8_t(0x5a), uint8_t(0x1e)})
        assert(samplePacket({0x80, pid}, 0, 24, 13, 0, bytes, length) == reverse(pid) && length == 2);
    puts("PASS: both ports capture descriptor DATA1/0/1 through short SE0/SE1 states, clock/sample phases and rollover; CRC rejects corruption.");
}
int main()
{
    uint8_t bytes[16];
    unsigned length;
    const std::vector<uint16_t> trace1 = {
        0x011e,0x029b,0x01af,0x02c4,0x01d9,0x02e9,0x01fe,
        0x0214,0x0138,0x025f,0x0372,0x0177,0x029e,0x00d9};
    const std::vector<uint16_t> trace2 = {
        0x01fd,0x0275,0x0189,0x02a1,0x01b5,0x02c8,0x01dc,
        0x02ef,0x0116,0x023d,0x0151,0x0278,0x00b2};
    assert(decode(trace1, bytes, length) == 0x4b && length == 2);
    assert(decode(trace2, bytes, length) == 0x4b && length == 2);
    for (unsigned idle = 0; idle < 40; ++idle) {
        assert(decode(encode({0x80, 0xd2}, idle), bytes, length) == 0x4b);
        assert(decode(encode({0x80, 0x5a}, idle), bytes, length) == 0x5a);
        assert(decode(encode({0x80, 0x1e}, idle), bytes, length) == 0x78);
    }
    for (uint8_t pid : {uint8_t(0xc3), uint8_t(0x4b)})
        for (unsigned size = 0; size <= 8; ++size) {
            std::vector<uint8_t> wire = {0x80, pid};
            unsigned crc = 0xffff;
            for (unsigned i = 0; i < size; ++i) {
                const uint8_t value = i % 2 ? 0xff : 0x7f;
                wire.push_back(value);
                crc ^= value;
                for (unsigned b = 0; b < 8; ++b) crc = crc & 1 ? (crc >> 1) ^ 0xa001 : crc >> 1;
            }
            crc ^= 0xffff;
            wire.push_back(crc & 255);
            wire.push_back(crc >> 8);
            assert(decode(encode(wire, 6), bytes, length) == 0x7b && length == wire.size());
            for (unsigned i = 0; i < wire.size(); ++i) assert(bytes[i] == reverse(wire[i]));
            wire.back() ^= 1;
            assert(decode(encode(wire, 6), bytes, length) == 0x7f);
        }
    auto truncated = trace2;
    truncated.pop_back();
    assert(decode(truncated, bytes, length) == 0x7f);
    assert(decode({}, bytes, length) == 0);
    puts("PASS: both real joystick ACK traces, idle gaps, DATA0/1, CRC, stuffing and truncated replies.");
    descriptorCaptureRegression();
}
