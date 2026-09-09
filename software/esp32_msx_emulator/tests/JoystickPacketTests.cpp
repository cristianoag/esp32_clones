#include "../../esp32_cp400_emulator/lib/SoftUSB/msx_decode.h"
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
}
