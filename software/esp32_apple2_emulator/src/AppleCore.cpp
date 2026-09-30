// Disk stepper adapted from codesafe/ESP32-VGA_AppleII_Emulator.
// IIe banking informed by Arthur Ferreira's reinetteIIe (MIT; lib/Reinette-LICENSE).
#define CHIPS_IMPL
#include "AppleCore.h"
#include <stdio.h>
#include <string.h>

namespace
{
bool fail(char *error, size_t capacity, const char *message)
{
    if (error && capacity) snprintf(error, capacity, "%s", message);
    return false;
}
}

bool AppleDisk::attach(const uint8_t *data, size_t size, char *error, size_t capacity)
{
    if (!data || size != ImageSize)
        return fail(error, capacity, "NIB image must be exactly 232960 bytes (35 tracks).");
    for (size_t i = 0; i < size; ++i)
        if (!(data[i] & 128))
            return fail(error, capacity, "Invalid NIB image: disk bytes must have bit 7 set.");
    data_ = data;
    reset();
    return true;
}

void AppleDisk::reset()
{
    nibble_ = phase_ = previousPhase_ = 0;
    halfTrack_ = 0;
    lastRead_ = 0;
    memset(phases_, 0, sizeof(phases_));
    memset(previous_, 0, sizeof(previous_));
    memset(beforePrevious_, 0, sizeof(beforePrevious_));
}

void AppleDisk::eject()
{
    data_ = nullptr;
    reset();
}

void AppleDisk::phase(unsigned address)
{
    const unsigned phase = (address & 7) >> 1;
    beforePrevious_[previousPhase_] = previous_[previousPhase_];
    previous_[phase_] = phases_[phase_];
    previousPhase_ = phase_;
    phase_ = phase;
    if (!(address & 1)) { phases_[phase] = false; return; }
    if (beforePrevious_[(phase + 1) & 3] && halfTrack_ > 0) --halfTrack_;
    if (beforePrevious_[(phase + 3) & 3] && halfTrack_ < 68) ++halfTrack_;
    phases_[phase] = true;
}

uint8_t AppleDisk::read(uint64_t cycles)
{
    if (!data_ || cycles - lastRead_ < 32) return 0;
    lastRead_ = cycles;
    const uint8_t value = data_[track() * 6656 + nibble_];
    nibble_ = (nibble_ + 1) % 6656;
    return value;
}

bool AppleCore::boot(const uint8_t *image, size_t size, const uint8_t *slot,
                     size_t slotSize, AppleModel selected, char *error, size_t capacity)
{
    if (selected > AppleModel::IIe)
        return fail(error, capacity, "Unsupported Apple model.");
    const size_t expected = selected == AppleModel::IIe ? 16384 : 12288;
    if (!image || size != expected)
        return fail(error, capacity, "ROM size mismatch: II+ needs 12288 bytes; original IIe needs 16384.");
    if (!slot || slotSize != sizeof(diskRom))
        return fail(error, capacity, "Disk II slot 6 ROM must be exactly 256 bytes.");
    model = selected;
    memset(ram, 0, sizeof(ram));
    memset(rom, 0xff, sizeof(rom));
    memcpy(rom + sizeof(rom) - size, image, size);
    memcpy(diskRom, slot, sizeof(diskRom));
    for (auto &disk : disks) disk.reset();
    cycles = frames = audioPhase_ = 0;
    reset();
    if (error && capacity) error[0] = 0;
    return true;
}

void AppleCore::reset()
{
    videoDirty_ = true;
    text = true;
    mixed = page2 = hires = col80 = altCharset = doubleHires = store80 = false;
    ramRead_ = ramWrite_ = altZp_ = intCxRom_ = slotC3Rom_ = intC8Rom_ = false;
    lcRead_ = lcPrewrite_ = false;
    lcWrite_ = lcBank2_ = true;
    speaker_ = anyKey_ = motor_ = q6_ = q7_ = false;
    motorOffCycle_ = 0;
    keyboard_ = buttons_ = drive_ = 0;
    paddleStart_ = cycles;
    audioPhase_ = 0;
    const m6502_desc_t description = {};
    pins_ = m6502_init(&cpu, &description);
}

unsigned AppleCore::bank(uint16_t address, bool writing) const
{
    if (model == AppleModel::IIPlus) return 0;
    if (address < 0x200 || address >= 0xd000) return altZp_;
    if (store80 && ((address >= 0x400 && address < 0x800) ||
                    (hires && address >= 0x2000 && address < 0x4000))) return page2;
    return writing ? ramWrite_ : ramRead_;
}

unsigned AppleCore::languageAddress(uint16_t address) const
{
    // The unused C000-CFFF RAM holds the second 4 KiB language-card bank.
    return address < 0xe000 && lcBank2_ ? address - 0x1000 : address;
}

uint8_t AppleCore::read(uint16_t address)
{
    if (address < 0xc000) return ram[bank(address, false)][address];
    if (address < 0xc100) return io(address, 0, false);
    if (address >= 0xd000)
        return lcRead_ ? ram[bank(address, false)][languageAddress(address)] : rom[address - 0xc000];
    if (model == AppleModel::IIe)
    {
        if (address >= 0xc300 && address < 0xc400 && !slotC3Rom_) intC8Rom_ = true;
        const bool internal = intCxRom_ ||
            (address >= 0xc300 && address < 0xc400 && !slotC3Rom_) ||
            (address >= 0xc800 && intC8Rom_);
        const uint8_t value = internal ? rom[address - 0xc000] :
            (address >= 0xc600 && address < 0xc700 ? diskRom[address & 255] : floatingBus());
        if (address == 0xcfff) intC8Rom_ = false;
        return value;
    }
    return address >= 0xc600 && address < 0xc700 ? diskRom[address & 255] : floatingBus();
}

void AppleCore::write(uint16_t address, uint8_t value)
{
    if (address < 0xc000)
    {
        uint8_t &byte = ram[bank(address, true)][address];
        if (byte != value && ((address >= 0x400 && address < 0xc00) ||
                             (address >= 0x2000 && address < 0x6000))) videoDirty_ = true;
        byte = value;
    }
    else if (address < 0xc100) io(address, value, true);
    else if (address >= 0xd000 && lcWrite_) ram[bank(address, true)][languageAddress(address)] = value;
    else if (model == AppleModel::IIe)
    {
        if (address >= 0xc300 && address < 0xc400 && !slotC3Rom_) intC8Rom_ = true;
        if (address == 0xcfff) intC8Rom_ = false;
    }
}

uint8_t AppleCore::floatingBus() const
{
    const unsigned line = (cycles % FrameCycles) / 65;
    const unsigned y = line % 192, column = (cycles % 65) % 40;
    const bool graphics = hires && !text && !(mixed && y >= 160);
    const unsigned base = graphics ? (page2 && !store80 ? 0x4000 : 0x2000) :
                                    (page2 && !store80 ? 0x800 : 0x400);
    const unsigned row = y / 8;
    const unsigned offset = (row & 7) * 128 + (row / 8) * 40 +
                            (graphics ? (y & 7) * 1024 : 0);
    return ram[0][base + offset + column];
}

uint8_t AppleCore::io(uint16_t address, uint8_t, bool writing)
{
    const bool iie = model == AppleModel::IIe;
    if (address < 0xc010)
    {
        if (!writing) return keyboard_;
        if (iie)
        {
            const bool on = address & 1;
            switch (address & 14)
            {
            case 0: store80 = on; break;
            case 2: ramRead_ = on; break;
            case 4: ramWrite_ = on; break;
            case 6: intCxRom_ = on; break;
            case 8: altZp_ = on; break;
            case 10: slotC3Rom_ = on; break;
            case 12: col80 = on; break;
            case 14: altCharset = on; break;
            }
        }
        return floatingBus();
    }
    if (address < 0xc020)
    {
        if (!iie || writing || address == 0xc010)
        {
            keyboard_ &= 127;
            return (keyboard_ & 127) | (iie && anyKey_ ? 128 : 0);
        }
        bool on = false;
        switch (address)
        {
        case 0xc011: on = lcBank2_; break;
        case 0xc012: on = lcRead_; break;
        case 0xc013: on = ramRead_; break;
        case 0xc014: on = ramWrite_; break;
        case 0xc015: on = intCxRom_; break;
        case 0xc016: on = altZp_; break;
        case 0xc017: on = slotC3Rom_; break;
        case 0xc018: on = store80; break;
        case 0xc019: on = cycles % FrameCycles < 192 * 65; break; // VBL-bar
        case 0xc01a: on = text; break;
        case 0xc01b: on = mixed; break;
        case 0xc01c: on = page2; break;
        case 0xc01d: on = hires; break;
        case 0xc01e: on = altCharset; break;
        case 0xc01f: on = col80; break;
        }
        return (keyboard_ & 127) | (on ? 128 : 0);
    }
    const bool diskLatch = address >= 0xc0e0 && address <= 0xc0ef && !(address & 1) && !writing;
    const uint8_t bus = diskLatch ? 0 : floatingBus();
    if (address >= 0xc030 && address < 0xc040) speaker_ = !speaker_;
    if (address >= 0xc050 && address <= 0xc057)
    {
        const bool on = address & 1;
        switch (address & 6)
        {
        case 0: text = on; break;
        case 2: mixed = on; break;
        case 4: page2 = on; break;
        case 6: hires = on; break;
        }
    }
    if (iie && (address == 0xc05e || address == 0xc05f)) doubleHires = !(address & 1);
    if (address >= 0xc061 && address <= 0xc063)
        return (bus & 127) | ((buttons_ & (1u << (address - 0xc061))) ? 128 : 0);
    if (address >= 0xc064 && address <= 0xc067)
        return (bus & 127) | (cycles - paddleStart_ < 8 + 11u * paddles_[address - 0xc064] ? 128 : 0);
    if (address >= 0xc070 && address <= 0xc07f) paddleStart_ = cycles;
    if (address >= 0xc080 && address <= 0xc08f)
    {
        lcBank2_ = !(address & 8);
        lcRead_ = (address & 3) == 0 || (address & 3) == 3;
        if (!(address & 1)) lcWrite_ = lcPrewrite_ = false;
        else
        {
            if (!writing && lcPrewrite_) lcWrite_ = true;
            lcPrewrite_ = !writing;
        }
    }
    if (address >= 0xc0e0 && address <= 0xc0ef)
    {
        const unsigned reg = address & 15;
        if (reg < 8) disks[drive_].phase(reg);
        else switch (reg)
        {
        case 8:
            // Disk II's delayed motor stop lets DOS keep spinning between RWTS calls.
            if (motor_) motorOffCycle_ = cycles + ClockRate;
            motor_ = false;
            break;
        case 9: motor_ = true; motorOffCycle_ = 0; break;
        case 10: drive_ = 0; break;
        case 11: drive_ = 1; break;
        case 12: q6_ = false; break;
        case 13: q6_ = true; break;
        case 14: q7_ = false; break;
        case 15: q7_ = true; break;
        }
        if (!writing && !(address & 1))
        {
            if (q7_) return 0;
            if (q6_) return 128; // All mounted media is hardware write-protected.
            return motor_ || cycles < motorOffCycle_ ? disks[drive_].read(cycles) : 0;
        }
    }
    return bus;
}

void AppleCore::input(bool anyKey, uint8_t modifiers, uint16_t pads)
{
    anyKey_ = anyKey;
    buttons_ = ((modifiers & 0x04) ? 1 : 0) | ((modifiers & 0x40) ? 2 : 0) |
               ((modifiers & 0x22) ? 4 : 0);
    for (unsigned port = 0; port < 2; ++port)
    {
        const uint8_t pad = pads >> (8 * port);
        paddles_[port * 2] = (pad & 12) == 4 ? 0 : (pad & 12) == 8 ? 255 : 127;
        paddles_[port * 2 + 1] = (pad & 3) == 1 ? 0 : (pad & 3) == 2 ? 255 : 127;
        if (!port) buttons_ |= (pad & 16 ? 1 : 0) | (pad & 32 ? 2 : 0);
        else buttons_ |= pad & 48 ? 4 : 0;
    }
}

void AppleCore::tick()
{
    pins_ = m6502_tick(&cpu, pins_);
    const uint16_t address = M6502_GET_ADDR(pins_);
    if (pins_ & M6502_RW) { M6502_SET_DATA(pins_, read(address)); }
    else write(address, M6502_GET_DATA(pins_));
    ++cycles;
}

void AppleCore::runFrame(uint8_t *pixels, int16_t *audio, unsigned &samples, bool monochrome)
{
    samples = 0;
    for (unsigned i = 0; i < FrameCycles; ++i)
    {
        tick();
        audioPhase_ += AudioRate;
        if (audioPhase_ >= ClockRate)
        {
            audioPhase_ -= ClockRate;
            audio[samples++] = speaker_ ? 10000 : -10000;
        }
    }
    ++frames;
    if (pixels) render(pixels, monochrome);
}
