/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain
 * this notice you can do whatever you want with this stuff. If we meet some day,
 * and you think this stuff is worth it, you can buy me a beer in return.
 * ----------------------------------------------------------------------------
 * Adapted from epatel/esp32_mac for the ESP32 Clone Series board.
 */
#include "emu.h"
#include "tmeconfig.h"
#include "m68k.h"
#include "via.h"
#include "scc.h"
#include "iwm.h"
#include "ncr.h"
#include "rtc.h"
#include "mouse.h"
#include <stdio.h>
#include <string.h>

static uint8_t *macRom, *macRam;
static int overlay, videoPage, audioPage, audioVolume, audioEnabled;
static int viaInterrupt, sccInterrupt;
static unsigned secondCycles;
static unsigned viaRemainder;
static int cycleDebt;

static uint8_t peripheral(unsigned address, unsigned value, int write)
{
    if (address >= 0x580000 && address < 0x600000) {
        if (write) ncrWrite((address >> 4) & 7, (address >> 9) & 1, value);
        else return ncrRead((address >> 4) & 7, (address >> 9) & 1);
    } else if (address >= 0x800000 && address < 0xc00000) {
        if (write) sccWrite(address, value);
        else return sccRead(address);
    } else if (address >= 0xc00000 && address < 0xe00000) {
        if (!(address & 1)) return 0xff;
        if (write) iwmWrite((address >> 9) & 15, value);
        else return iwmRead((address >> 9) & 15);
    } else if (address >= 0xe80000 && address < 0xf00000) {
        // The VIA is connected only to the upper (even-addressed) data lane.
        if (address & 1) return 0xff;
        if (write) viaWrite((address >> 9) & 15, value);
        else return viaRead((address >> 9) & 15);
    } else if (address >= 0xf00000 && address < 0xf80000) {
        if (!write) return sccRead(address);
    }
    return 0xff;
}

unsigned m68k_read_memory_8(unsigned address)
{
    address &= 0xffffff;
    if (address < 0x400000)
        return overlay ? macRom[address & (TME_ROMSIZE - 1)] : macRam[address];
    if (address < 0x500000) {
        const unsigned offset = address - 0x400000;
        if (offset < TME_ROMSIZE) return macRom[offset];
        // Match MAME's non-mirrored Plus ROM probe windows; identical mirrors disable SCSI.
        if ((offset & ~1u) == 0x20000) return 0xff;
        if ((offset & ~1u) == 0x40000) return 0xaa;
        return 0;
    }
    if (address >= 0x600000 && address < 0x700000)
        return macRam[address & (TME_RAMSIZE - 1)];
    return peripheral(address, 0, 0);
}

unsigned m68k_read_memory_16(unsigned address)
{
    return (m68k_read_memory_8(address) << 8) | m68k_read_memory_8(address + 1);
}

unsigned m68k_read_memory_32(unsigned address)
{
    return (m68k_read_memory_16(address) << 16) | m68k_read_memory_16(address + 2);
}

void m68k_write_memory_8(unsigned address, unsigned value)
{
    address &= 0xffffff;
    if (address < 0x400000) {
        if (!overlay) macRam[address] = value;
    } else if (address >= 0x600000 && address < 0x700000) {
        macRam[address & (TME_RAMSIZE - 1)] = value;
    } else if (address >= 0x500000) {
        peripheral(address, value, 1);
    }
}

void m68k_write_memory_16(unsigned address, unsigned value)
{
    m68k_write_memory_8(address, value >> 8);
    m68k_write_memory_8(address + 1, value);
}

void m68k_write_memory_32(unsigned address, unsigned value)
{
    m68k_write_memory_16(address, value >> 16);
    m68k_write_memory_16(address + 2, value);
}

void viaIrq(int requested)
{
    viaInterrupt = requested;
    m68k_set_irq(sccInterrupt ? 2 : viaInterrupt ? 1 : 0);
}

void sccIrq(int requested)
{
    sccInterrupt = requested;
    m68k_set_irq(sccInterrupt ? 2 : viaInterrupt ? 1 : 0);
}

void viaCbPortAWrite(unsigned value)
{
    videoPage = !(value & 0x40);
    overlay = (value >> 4) & 1;
    audioPage = !(value & 0x08);
    audioVolume = value & 7;
    iwmSetHeadSel(value & (1 << 5));
}

void viaCbPortBWrite(unsigned value)
{
    if (rtcCom(value & 4, value & 1, value & 2)) viaSet(VIA_PORTB, 1);
    else viaClear(VIA_PORTB, 1);
    audioEnabled = !(value & 0x80);
}

void tmeInit(uint8_t *rom, uint8_t *ram)
{
    macRom = rom;
    macRam = ram;
    memset(macRam, 0, TME_RAMSIZE);
    overlay = 1;
    videoPage = audioPage = audioVolume = audioEnabled = 0;
    viaInterrupt = sccInterrupt = 0;
    secondCycles = 0;
    viaRemainder = 0;
    cycleDebt = 0;
    viaInit();
    ncrInit();
    iwmInit();
    mouseReset();
    sccInit();
    viaSet(VIA_PORTA, 0x80);
    viaSet(VIA_PORTB, 0x38);
    m68k_init();
    m68k_set_cpu_type(M68K_CPU_TYPE_68000);
    m68k_pulse_reset();
}

void tmeRunFrame(void)
{
    // 370 scan lines, 352 CPU clocks per line. VIA runs at CPU clock / 10.
    for (unsigned line = 0; line < 370; ++line) {
        const int executed = m68k_execute(352 - cycleDebt);
        cycleDebt += executed - 352;
        iwmStep(executed);
        viaRemainder += executed;
        viaStep(viaRemainder / 10);
        viaRemainder %= 10;
        sccTick(executed / 10);
        const int pins = mouseTick();
        const int bits[] = {MOUSE_BTN, MOUSE_QXB, MOUSE_QYB};
        for (unsigned i = 0; i < 3; ++i) {
            if (pins & bits[i]) viaClear(VIA_PORTB, 1 << (i + 3));
            else viaSet(VIA_PORTB, 1 << (i + 3));
        }
        sccSetDcd(SCC_CHANA, pins & MOUSE_QXA);
        sccSetDcd(SCC_CHANB, pins & MOUSE_QYA);
    }
    viaControlWrite(VIA_CA1, 1);
    viaControlWrite(VIA_CA1, 0);
    secondCycles += 130240;
    if (secondCycles >= 7833600) {
        secondCycles -= 7833600;
        viaControlWrite(VIA_CA2, 1);
        viaControlWrite(VIA_CA2, 0);
        rtcTick();
    }
}

const uint8_t *tmeFramebuffer(void)
{
    return macRam + (videoPage ? TME_SCREENBUF_ALT : TME_SCREENBUF);
}

void tmeAudio(int16_t samples[370], unsigned volume)
{
    const uint8_t *sound = macRam + (audioPage ? TME_SNDBUF_ALT : TME_SNDBUF);
    for (unsigned i = 0; i < 370; ++i)
        samples[i] = audioEnabled ? ((int)sound[i * 2] - 128) * 256 *
                     audioVolume / 7 * (int)volume / 100 : 0;
}
