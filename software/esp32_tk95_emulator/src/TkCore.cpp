#define CHIPS_IMPL
#include "TkCore.h"
#include <stdio.h>
#include <string.h>

namespace
{
bool fail(char *error, size_t capacity, const char *message)
{
    snprintf(error, capacity, "%s", message);
    return false;
}
}

bool TkTape::attach(const uint8_t *data, size_t size, char *error, size_t capacity)
{
    if (!data || size < 4) return fail(error, capacity, "Empty or truncated TAP image.");
    for (size_t pos = 0; pos < size;)
    {
        if (size - pos < 2) return fail(error, capacity, "Truncated TAP block header.");
        const size_t length = data[pos] | (unsigned(data[pos + 1]) << 8);
        pos += 2;
        if (length < 2 || length > size - pos)
            return fail(error, capacity, "Invalid TAP block length.");
        uint8_t checksum = 0;
        for (size_t i = 0; i < length; ++i) checksum ^= data[pos + i];
        if (checksum) return fail(error, capacity, "TAP block checksum mismatch.");
        pos += length;
    }
    data_ = data;
    size_ = size;
    rewind();
    return true;
}

void TkTape::eject()
{
    data_ = nullptr;
    size_ = 0;
    rewind();
}

void TkTape::rewind()
{
    playing_ = level_ = false;
    offset_ = end_ = 0;
    phase_ = bit_ = half_ = 0;
    remaining_ = pulses_ = 0;
}

void TkTape::block()
{
    const size_t length = data_[offset_] | (unsigned(data_[offset_ + 1]) << 8);
    offset_ += 2;
    end_ = offset_ + length;
    pulses_ = data_[offset_] < 0x80 ? 8063 : 3223;
    phase_ = 1;
    remaining_ = 2168;
}

bool TkTape::tick()
{
    if (!playing_) return level_;
    if (!phase_) block();
    if (--remaining_) return level_;
    level_ = !level_;
    switch (phase_)
    {
    case 1:
        if (--pulses_) remaining_ = 2168;
        else { phase_ = 2; remaining_ = 667; }
        break;
    case 2: phase_ = 3; remaining_ = 735; break;
    case 3:
        phase_ = 4; bit_ = half_ = 0;
        remaining_ = data_[offset_] & 0x80 ? 1710 : 855;
        break;
    case 4:
        if (++half_ == 2)
        {
            half_ = 0;
            if (++bit_ == 8) { bit_ = 0; ++offset_; }
        }
        if (offset_ == end_) { phase_ = 5; remaining_ = 3579545; }
        else remaining_ = data_[offset_] & (0x80 >> bit_) ? 1710 : 855;
        break;
    case 5:
        level_ = false;
        if (offset_ == size_) { playing_ = false; rewind(); offset_ = size_; }
        else block();
        break;
    }
    return level_;
}

bool TkCore::boot(const uint8_t *rom, size_t size, TkModel selectedModel,
                  TkTiming selectedTiming, char *error, size_t capacity)
{
    if (!rom || size != 16384) return fail(error, capacity, "TK ROM must be exactly 16384 bytes.");
    if (selectedModel > TkModel::TK90X || selectedTiming > TkTiming::Hz50)
        return fail(error, capacity, "Invalid TK model or video timing.");
    memcpy(memory, rom, size);
    memset(memory + 16384, 0, 49152);
    model = selectedModel;
    timing = selectedTiming;
    reset();
    return true;
}

void TkCore::reset()
{
    pins_ = z80_init(&cpu);
    cycle = frames = audioPhase_ = stall_ = 0;
    border = output_ = 0;
    tape.rewind();
}

void TkCore::restoreCpu(uint16_t pc)
{
    pins_ = z80_prefetch(&cpu, pc);
    stall_ = cycle = audioPhase_ = 0;
    output_ = border;
}

void TkCore::input(const uint8_t rows[8], uint8_t kempston)
{
    memcpy(rows_, rows, 8);
    kempston_ = kempston & 31;
}

uint8_t TkCore::floatingBus() const
{
    const TkClock clock = TkGetClock(timing);
    const unsigned y = cycle / 228 - clock.firstLine;
    const unsigned phase = cycle % 228 - 99;
    if (y >= 192 || phase >= 125 || (phase & 4)) return 255;
    const unsigned x = (phase >> 2) + ((phase >> 1) & 1);
    const unsigned address = phase & 1 ? 0x5800 + (y >> 3) * 32 :
        0x4000 + ((y & 0xc0) << 5) + ((y & 7) << 8) + ((y & 0x38) << 2);
    return memory[address + x];
}

uint8_t TkCore::readPort(uint16_t address) const
{
    if (!(address & 1))
    {
        uint8_t value = portuguese ? 0x3f : 0xbf;
        for (unsigned row = 0; row < 8; ++row)
            if (!(address & (0x100u << row))) value &= rows_[row];
        return value | (((output_ & 16) != 0) != ear_ ? 0x40 : 0);
    }
    if ((address & 0xff) == 0x1f) return kempston_;
    return floatingBus();
}

void TkCore::writePort(uint16_t address, uint8_t value)
{
    if (!(address & 1)) { output_ = value; border = value & 7; }
}

unsigned TkCore::contention(uint16_t address, bool io) const
{
    const unsigned y = cycle / 228 - TkGetClock(timing).firstLine;
    const unsigned x = cycle % 228;
    if (y >= 192 || x < 93 || x >= 221) return 0;
    if ((address & 0xc000) != 0x4000 && (!io || (address & 1))) return 0;
    const unsigned phase = (x - 93) & 7;
    return phase < 6 ? 6 - phase : 0;
}

uint8_t TkColor(uint8_t color, bool bright)
{
    const uint8_t rg = bright ? 7 : 5, b = bright ? 3 : 2;
    return ((color & 2) ? rg : 0) | ((color & 4) ? rg << 3 : 0) |
           ((color & 1) ? b << 6 : 0);
}

void TkCore::raster(uint8_t *pixels) const
{
    if (!pixels) return;
    const TkClock clock = TkGetClock(timing);
    const unsigned beam = cycle >= 83 ? cycle - 83 : cycle + clock.frameCycles - 83;
    const unsigned x = (beam % 228) * 2;
    const unsigned y = beam / 228 - (clock.firstLine - 24);
    if (x >= Width || y >= Height || (x & 7)) return;
    uint8_t *destination = pixels + y * Width + x;
    if (x < 32 || x >= 288 || y < 24 || y >= 216)
    {
        memset(destination, TkColor(border, false), 8);
        return;
    }
    const unsigned sy = y - 24, sx = (x - 32) / 8;
    const uint8_t attribute = memory[0x5800 + (sy / 8) * 32 + sx];
    uint8_t bits = memory[0x4000 + ((sy & 0xc0) << 5) + ((sy & 7) << 8) +
                          ((sy & 0x38) << 2) + sx];
    if ((attribute & 128) && (frames & 16)) bits ^= 255;
    const uint8_t ink = TkColor(attribute & 7, attribute & 64);
    const uint8_t paper = TkColor((attribute >> 3) & 7, attribute & 64);
    for (unsigned bit = 0; bit < 8; ++bit) destination[bit] = bits & (128 >> bit) ? ink : paper;
}

void TkCore::runFrame(uint8_t *pixels, int16_t *audio, unsigned &samples)
{
    const TkClock clock = TkGetClock(timing);
    const uint32_t cpuHz = (uint64_t(clock.frameCycles) * 1000000 + clock.frameMicros / 2) / clock.frameMicros;
    samples = 0;
    do
    {
        ear_ = tape.tick();
        if (pixels && (cycle & 3) == 3) raster(pixels);
        audioPhase_ += AudioRate;
        if (audioPhase_ >= cpuHz)
        {
            audioPhase_ -= cpuHz;
            if (audio) audio[samples] = (output_ & 16) ? 12000 : -12000;
            ++samples;
        }
        if (stall_) --stall_;
        else
        {
            if (cycle < 32) pins_ |= Z80_INT;
            else pins_ &= ~Z80_INT;
            const uint64_t old = pins_;
            pins_ = z80_tick(&cpu, pins_);
            const uint16_t address = Z80_GET_ADDR(pins_);
            if (pins_ & Z80_MREQ)
            {
                if (pins_ & Z80_RD) { Z80_SET_DATA(pins_, memory[address]); }
                else if ((pins_ & Z80_WR) && address >= 16384) memory[address] = Z80_GET_DATA(pins_);
            }
            else if (pins_ & Z80_IORQ)
            {
                if (pins_ & Z80_M1) { Z80_SET_DATA(pins_, 255); }
                else if (pins_ & Z80_RD) { Z80_SET_DATA(pins_, readPort(address)); }
                else if (pins_ & Z80_WR) writePort(address, Z80_GET_DATA(pins_));
            }
            const uint64_t request = pins_ & (Z80_MREQ | Z80_IORQ);
            if (request && !(old & request) && !(pins_ & (Z80_RFSH)) &&
                !((pins_ & Z80_IORQ) && (pins_ & Z80_M1)))
                stall_ = contention(address, pins_ & Z80_IORQ);
        }
        if (++cycle == clock.frameCycles) cycle = 0;
    } while (cycle);
    ++frames;
}
