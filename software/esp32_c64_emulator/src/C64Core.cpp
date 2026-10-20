#include "C64Core.h"
#include "C64Input.h"
#include <stdio.h>
#include <string.h>
#include <initializer_list>

namespace
{
bool fail(char *error, size_t capacity, const char *message)
{
    if (error && capacity) snprintf(error, capacity, "%s", message);
    return false;
}
}

C64Core::C64Core() : cia1(true), cia2(false)
{
    memset(matrix_, 255, sizeof(matrix_));
    vic.init(ram, chargen_, pixels, colors_);
    cpuhalted = true;
}

bool C64Core::boot(const uint8_t *basic, size_t basicSize, const uint8_t *kernal,
                  size_t kernalSize, const uint8_t *chargen, size_t chargenSize,
                  char *error, size_t errorSize)
{
    if (!basic || basicSize != 8192 || !kernal || kernalSize != 8192 ||
        !chargen || chargenSize != 4096)
        return fail(error, errorSize, "ROM sizes: BASIC/KERNAL 8192, character ROM 4096.");
    memcpy(basic_, basic, sizeof(basic_));
    memcpy(kernal_, kernal, sizeof(kernal_));
    memcpy(chargen_, chargen, sizeof(chargen_));
    memset(ram, 0, sizeof(ram));
    memset(pixels, 0, sizeof(pixels));
    memset(colors_, 0, sizeof(colors_));
    frames = 0;
    reset();
    return true;
}

void C64Core::reset()
{
    ram[0] = 0x2f;
    ram[1] = 0x37;
    a = x = y = 0;
    sp = 0xfd;
    sr = 0x24;
    cflag = zflag = dflag = bflag = vflag = nflag = false;
    iflag = true;
    numofcycles = 0;
    cpuhalted = false;
    cia1.init(true);
    cia2.init(false);
    cia1.isTODRunning.store(false);
    cia2.isTODRunning.store(false);
    vic.init(ram, chargen_, pixels, colors_);
    sid.initSID();
    sid.setEmuVolume(100);
    pc = kernal_[0x1ffc] | (kernal_[0x1ffd] << 8);
    nmiLine_ = restorePending_ = false;
    cycleDebt_ = 0;
    vicAddresses();
}

uint8_t C64Core::port() const
{
    return (ram[1] & ram[0]) | static_cast<uint8_t>(~ram[0]);
}

void C64Core::vicAddresses()
{
    const unsigned pins = cia2.ciareg[0] | static_cast<uint8_t>(~cia2.ciareg[2]);
    vic.vicmem = ((~pins) & 3) * 0x4000;
    vic.screenmemstart = vic.vicmem + ((vic.vicreg[0x18] & 0xf0) << 6);
    vic.bitmapstart = vic.vicmem + ((vic.vicreg[0x18] & 8) << 10);
    const unsigned address = vic.vicmem + ((vic.vicreg[0x18] & 14) << 10);
    vic.charset = (address & 0x7000) == 0x1000 ? chargen_ + (address & 0x800) : ram + address;
}

uint8_t C64Core::getMem(uint16_t address)
{
    const uint8_t bank = port();
    if (address == 1) return bank;
    if (address >= 0xa000 && address < 0xc000 && (bank & 3) == 3)
        return basic_[address - 0xa000];
    if (address >= 0xe000 && (bank & 2)) return kernal_[address - 0xe000];
    if (address < 0xd000 || address >= 0xe000 || !(bank & 3)) return ram[address];
    if (!(bank & 4)) return chargen_[address & 0xfff];
    if (address < 0xd400)
    {
        const unsigned reg = address & 63;
        if (reg == 0x11) return (vic.vicreg[reg] & 127) | (vic.rasterline >= 256 ? 128 : 0);
        const uint8_t value = vic.vicreg[reg];
        if (reg == 0x1e || reg == 0x1f) vic.vicreg[reg] = 0;
        return value;
    }
    if (address < 0xd800)
    {
        const unsigned reg = address & 31;
        if (reg == 0x19 || reg == 0x1a) return 255;
        if (reg == 0x1b) return static_cast<uint8_t>((sid.sidVoice[2].sample + 1.0f) * 127.5f);
        if (reg == 0x1c) return static_cast<uint8_t>(sid.sidVoice[2].envelope * 255.0f);
        return sid.sidreg[reg];
    }
    if (address < 0xdc00) return colors_[address & 1023] | 0xf0;
    if (address >= 0xde00) return 255;
    CIA &cia = address < 0xdd00 ? cia1 : cia2;
    const unsigned reg = address & 15;
    if (reg < 2)
    {
        uint8_t input = 255;
        if (&cia == &cia1)
        {
            const unsigned other = reg ^ 1;
            const uint8_t selected = (cia.ciareg[other] | static_cast<uint8_t>(~cia.ciareg[other + 2])) &
                                     joysticks_[reg ? 1 : 0];
            input = C64ReadMatrix(matrix_, selected, reg == 0) & joysticks_[reg ? 0 : 1];
        }
        return (cia.ciareg[reg] | static_cast<uint8_t>(~cia.ciareg[reg + 2])) & input;
    }
    return cia.getCommonCIAReg(reg);
}

void C64Core::setMem(uint16_t address, uint8_t value)
{
    if (address < 0xd000 || address >= 0xe000 || !(port() & 3) || !(port() & 4))
    {
        ram[address] = value;
        return;
    }
    if (address < 0xd400)
    {
        const unsigned reg = address & 63;
        if (reg == 0x11) vic.latchd011 = value;
        if (reg == 0x12) { vic.latchd012 = value; return; }
        if (reg == 0x19)
        {
            vic.vicreg[reg] &= ~(value & 15);
            vic.vicreg[reg] = (vic.vicreg[reg] & 15) |
                ((vic.vicreg[reg] & vic.vicreg[0x1a] & 15) ? 128 : 0);
            return;
        }
        if (reg == 0x1e || reg == 0x1f) return;
        vic.vicreg[reg] = value;
        if (reg == 0x18) vicAddresses();
    }
    else if (address < 0xd800)
    {
        const unsigned reg = address & 31;
        sid.sidreg[reg] = value;
        if (reg < 21)
        {
            const unsigned voice = reg / 7, base = voice * 7;
            SIDVoice &target = sid.sidVoice[voice];
            switch (reg % 7)
            {
            case 0: case 1: target.updVarFrequency(sid.sidreg[base] | (sid.sidreg[base + 1] << 8)); break;
            case 2: case 3: target.updVarPulseWidth(sid.sidreg[base + 2] | (sid.sidreg[base + 3] << 8)); break;
            case 4: target.updVarControl(value); break;
            case 5: target.updVarEnvelopeAD(value); break;
            case 6: target.updVarEnvelopeSR(value); break;
            }
        }
        else if (reg == 0x18) sid.c64Volume = float(value & 15) / 15.0f;
    }
    else if (address < 0xdc00) colors_[address & 1023] = value & 15;
    else if (address < 0xde00)
    {
        CIA &cia = address < 0xdd00 ? cia1 : cia2;
        const unsigned reg = address & 15;
        if (reg < 2) cia.ciareg[reg] = value;
        else cia.setCommonCIAReg(reg, value);
        if (&cia == &cia2 && (reg == 0 || reg == 2)) vicAddresses();
    }
}

void C64Core::input(const uint8_t report[8], uint16_t pads, bool swapPorts)
{
    bool restore = false, caps = false;
    for (unsigned i = 2; i < 8; ++i) { restore |= report[i] == 68; caps |= report[i] == 57; }
    if (caps && !capsHeld_) shiftLock_ = !shiftLock_;
    capsHeld_ = caps;
    restorePending_ |= restore && !restoreHeld_;
    restoreHeld_ = restore;
    C64MapReport(report, matrix_, shiftLock_);
    joysticks_[swapPorts ? 1 : 0] = C64JoystickValue(pads & 255);
    joysticks_[swapPorts ? 0 : 1] = C64JoystickValue(pads >> 8);
}

void C64Core::timers(unsigned cycles)
{
    for (CIA *cia : {&cia1, &cia2})
    {
        cia->checkAlarm();
        cia->checkTimerA(cycles);
        cia->checkTimerB(cycles);
    }
}

unsigned C64Core::step()
{
    numofcycles = 0;
    const bool nmi = (cia2.latchdc0d & 0x80) != 0;
    if (restorePending_ || (nmi && !nmiLine_))
    {
        restorePending_ = false;
        setPCToIntVec(getMem(0xfffa) | (getMem(0xfffb) << 8), false);
    }
    else if (!iflag && ((cia1.latchdc0d & 0x80) ||
             (vic.vicreg[0x19] & vic.vicreg[0x1a] & 15)))
        setPCToIntVec(getMem(0xfffe) | (getMem(0xffff) << 8), false);
    nmiLine_ = nmi;
    if (!numofcycles) execute(getMem(pc++));
    return numofcycles;
}

void C64Core::runFrame()
{
    sid.beginFrame();
    for (unsigned raster = 0; raster < 312; ++raster)
    {
        // Match upstream's game-compatible 63-cycle line budget (no badline steals).
        vic.nextRasterline();
        cycleDebt_ -= 63;
        while (cycleDebt_ < 0 && !cpuhalted)
        {
            const unsigned elapsed = step();
            timers(elapsed);
            cycleDebt_ += elapsed;
        }
        vic.drawRasterline();
        sid.fillBuffer(raster);
    }
    if (++frames % 5 == 0) { cia1.updateTOD(); cia2.updateTOD(); }
}

bool C64Core::loadPrg(const uint8_t *data, size_t size, char *error, size_t errorSize)
{
    if (!data || size < 3 || size > 65538)
        return fail(error, errorSize, "PRG must contain an address and at least one data byte.");
    const unsigned address = data[0] | (data[1] << 8);
    if (size - 2 > 65536u - address)
        return fail(error, errorSize, "PRG address range is invalid; machine was not changed.");
    memcpy(ram + address, data + 2, size - 2);
    if (address == 0x0801)
    {
        const uint16_t end = address + size - 2;
        for (unsigned pointer : {0x2du,0x2fu,0x31u,0xaeu})
        {
            ram[pointer] = end & 255;
            ram[pointer + 1] = end >> 8;
        }
    }
    return true;
}

void C64Core::queueRun()
{
    memcpy(ram + 0x277, "RUN\r", 4);
    ram[0xc6] = 4;
}
