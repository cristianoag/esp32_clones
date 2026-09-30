#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef ESP_PLATFORM
#define APPLE_IRAM __attribute__((section(".iram1.apple")))
#define M6502_TICK_ATTR APPLE_IRAM
#else
#define APPLE_IRAM
#endif
#include <m6502.h>

enum class AppleModel : uint8_t { IIPlus, IIe };

class AppleDisk
{
public:
    static constexpr size_t ImageSize = 35 * 6656;
    bool attach(const uint8_t *data, size_t size, char *error, size_t capacity);
    void eject();
    void reset();
    void phase(unsigned address);
    APPLE_IRAM uint8_t read(uint64_t cycles);
    bool loaded() const { return data_ != nullptr; }
    unsigned track() const { return (halfTrack_ + 1) / 2; }
private:
    const uint8_t *data_ = nullptr;
    unsigned nibble_ = 0;
    int halfTrack_ = 0;
    bool phases_[4] = {}, previous_[4] = {}, beforePrevious_[4] = {};
    unsigned phase_ = 0, previousPhase_ = 0;
    uint64_t lastRead_ = 0;
};

class AppleCore
{
public:
    static constexpr unsigned Width = 640, Height = 240;
    static constexpr unsigned ClockRate = 1023000, FrameCycles = 65 * 262;
    static constexpr unsigned FrameMicros = 16647, AudioRate = 22050;
    static constexpr unsigned MaxSamples = 368;
    bool boot(const uint8_t *rom, size_t size, const uint8_t *diskRom,
              size_t diskSize, AppleModel model, char *error, size_t capacity);
    void reset();
    APPLE_IRAM void tick();
    APPLE_IRAM void runFrame(uint8_t *pixels, int16_t *audio, unsigned &samples, bool monochrome);
    void render(uint8_t *pixels, bool monochrome);
    bool needsRender(bool monochrome) const
    {
        return videoDirty_ || renderedVideoState_ != videoState(monochrome) ||
               (flashingText_ && renderedFlashPhase_ != ((frames >> 4) & 1));
    }
    APPLE_IRAM uint8_t read(uint16_t address);
    APPLE_IRAM void write(uint16_t address, uint8_t value);
    void key(uint8_t ascii) { keyboard_ = (ascii & 127) | 128; }
    bool keyPending() const { return keyboard_ & 128; }
    void input(bool anyKey, uint8_t modifiers, uint16_t joysticks);
    APPLE_IRAM uint8_t floatingBus() const;
    uint8_t ram[2][65536] = {};
    uint8_t rom[16384] = {};
    uint8_t diskRom[256] = {};
    AppleDisk disks[2];
    m6502_t cpu = {};
    AppleModel model = AppleModel::IIPlus;
    uint64_t cycles = 0;
    uint32_t frames = 0;
    bool text = true, mixed = false, page2 = false, hires = false;
    bool col80 = false, altCharset = false, doubleHires = false, store80 = false;
private:
    unsigned bank(uint16_t address, bool write) const;
    unsigned languageAddress(uint16_t address) const;
    APPLE_IRAM uint8_t io(uint16_t address, uint8_t value, bool write);
    unsigned videoState(bool monochrome) const
    {
        return unsigned(model) | (unsigned(text) << 1) | (unsigned(mixed) << 2) |
               (unsigned(hires) << 3) | (unsigned(page2) << 4) | (unsigned(store80) << 5) |
               (unsigned(col80) << 6) | (unsigned(altCharset) << 7) |
               (unsigned(doubleHires) << 8) | (unsigned(monochrome) << 9);
    }
    unsigned renderedVideoState_ = 0, renderedFlashPhase_ = 0;
    bool videoDirty_ = true, flashingText_ = false;
    uint64_t pins_ = 0, paddleStart_ = 0, motorOffCycle_ = 0;
    uint32_t audioPhase_ = 0;
    uint8_t keyboard_ = 0, buttons_ = 0, paddles_[4] = {127,127,127,127};
    unsigned drive_ = 0;
    bool ramRead_ = false, ramWrite_ = false, altZp_ = false;
    bool intCxRom_ = false, slotC3Rom_ = false, intC8Rom_ = false;
    bool lcRead_ = false, lcWrite_ = true, lcBank2_ = true, lcPrewrite_ = false;
    bool speaker_ = false, anyKey_ = false, motor_ = false, q6_ = false, q7_ = false;
};
