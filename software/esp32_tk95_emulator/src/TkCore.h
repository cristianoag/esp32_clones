#pragma once
#include <stddef.h>
#include <stdint.h>
#include <z80.h>

enum class TkModel : uint8_t { TK95, TK90X };
enum class TkTiming : uint8_t { Hz60, Hz50 };

struct TkClock
{
    uint32_t frameCycles, frameMicros;
    unsigned firstLine;
};
inline TkClock TkGetClock(TkTiming timing)
{
    return timing == TkTiming::Hz50 ? TkClock{71136, 19895, 64} : TkClock{59736, 16707, 38};
}

class TkTape
{
public:
    bool attach(const uint8_t *data, size_t size, char *error, size_t capacity);
    void eject();
    void rewind();
    void play(bool enabled) { playing_ = enabled && data_ && offset_ < size_; }
    bool playing() const { return playing_; }
    bool loaded() const { return data_ != nullptr; }
    bool tick();
    size_t position() const { return offset_; }
private:
    void block();
    const uint8_t *data_ = nullptr;
    size_t size_ = 0, offset_ = 0, end_ = 0;
    uint32_t remaining_ = 0, pulses_ = 0;
    uint8_t phase_ = 0, bit_ = 0, half_ = 0;
    bool level_ = false, playing_ = false;
};

class TkCore
{
public:
    static constexpr unsigned Width = 320, Height = 240, AudioRate = 22050;
    bool boot(const uint8_t *rom, size_t size, TkModel model, TkTiming timing,
              char *error, size_t capacity);
    void reset();
    void runFrame(uint8_t *pixels, int16_t *audio, unsigned &samples);
    void input(const uint8_t rows[8], uint8_t kempston);
    uint8_t readPort(uint16_t address) const;
    void writePort(uint16_t address, uint8_t value);
    uint8_t floatingBus() const;
    unsigned contention(uint16_t address, bool io) const;
    uint8_t memory[65536] = {};
    z80_t cpu = {};
    TkTape tape;
    TkModel model = TkModel::TK95;
    TkTiming timing = TkTiming::Hz60;
    bool portuguese = true;
    uint32_t cycle = 0, frames = 0;
    uint8_t border = 0;
    void restoreCpu(uint16_t pc);
private:
    void raster(uint8_t *pixels) const;
    uint64_t pins_ = 0;
    unsigned stall_ = 0;
    uint32_t audioPhase_ = 0;
    uint8_t rows_[8] = {255,255,255,255,255,255,255,255};
    uint8_t kempston_ = 0, output_ = 0;
    bool ear_ = false;
};

bool TkLoadSnapshot(TkCore &core, const uint8_t *data, size_t size, bool sna,
                    char *error, size_t capacity);
uint8_t TkColor(uint8_t color, bool bright);
