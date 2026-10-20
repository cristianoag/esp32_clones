#pragma once
#include "CPU6502.h"
#include "CIA.h"
#include "VIC.h"
#include "SID.h"
#include <stddef.h>

class C64Core : public CPU6502
{
public:
    C64Core();
    bool boot(const uint8_t *basic, size_t basicSize, const uint8_t *kernal,
              size_t kernalSize, const uint8_t *chargen, size_t chargenSize,
              char *error, size_t errorSize);
    void reset();
    void input(const uint8_t report[8], uint16_t pads, bool swapPorts);
    void run() override { runFrame(); }
    void runFrame();
    uint8_t getMem(uint16_t address) override;
    void setMem(uint16_t address, uint8_t value) override;
    bool loadPrg(const uint8_t *data, size_t size, char *error, size_t errorSize);
    void queueRun();
    uint16_t programCounter() const { return pc; }
    uint8_t ram[65536] = {};
    uint8_t pixels[320 * 200] = {};
    uint32_t frames = 0;
    VIC vic;
    CIA cia1, cia2;
    SID sid;
private:
    uint8_t basic_[8192] = {}, kernal_[8192] = {}, chargen_[4096] = {};
    uint8_t colors_[1024] = {}, matrix_[8], joysticks_[2] = {255,255};
    bool restoreHeld_ = false, restorePending_ = false, nmiLine_ = false;
    bool capsHeld_ = false, shiftLock_ = false;
    int cycleDebt_ = 0;
    uint8_t port() const;
    void vicAddresses();
    unsigned step();
    void timers(unsigned cycles);
};
