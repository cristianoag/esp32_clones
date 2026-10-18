/* Embedded A1ST integration; blueMSX and openMSX hardware references are listed
 * in docs/licenses.md. No fMSX core or BIOS patches are used. */
#include "A1stInternal.h"
#include "R800.h"
#include "Board.h"
#include "SlotManager.h"
#include "DeviceManager.h"
#include "DebugDeviceManager.h"
#include "IoPort.h"
#include "ramMapper.h"
#include "ramMapperIo.h"
#include "romMapperDRAM.h"
#include "romMapperPanasonic.h"
#include "romMapperS1990.h"
#include "romMapperTurboRTimer.h"
#include "romMapperTC8566AF.h"
#include "romMapperKanji.h"
#include "romMapperMsxMusic.h"
#include "romMapperNormal.h"
#include "romMapperF4device.h"
#include "TurboRIO.h"
#include "I8255.h"
#include "AY8910.h"
#include "../Src/IoDevice/RTC.h"
#include "DAC.h"
#include "Switches.h"
#include "VideoManager.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifdef ESP_PLATFORM
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

static A1stHost host;
static R800* cpu;
static Mixer* mixer;
static AY8910* psg;
static I8255* ppi;
static RTC* rtc;
static DAC* click;
static UInt8* ram;
static UInt32 ramSize;
static char stateDirectory[384];
static UInt8 keyboard[16];
static UInt16 joysticks;
static UInt8 keyboardRow;
static UInt8 psgOutput;
static UInt32 interrupts;
static UInt32 frames;
static UInt32 switches;
static UInt32 lastTime;
static UInt64 ticks;
static int frameComplete;
static int frameTimedOut;
UInt32* boardSysTime;

struct BoardTimer {
    BoardTimerCb callback;
    void* ref;
    UInt32 timeout;
    int active;
};
static BoardTimer* timers[64];
static BoardTimer* frameFence;

static void updateTime(void)
{
    UInt32 now = cpu->systemTime;
    ticks += (UInt32)(now - lastTime);
    lastTime = now;
}

void a1stLog(const char* message)
{
    if (host.message) host.message(message);
    else fprintf(stderr, "A1ST: %s\n", message);
}

const char* a1stStateDirectory(void) { return stateDirectory; }

static void scheduleCpu(void)
{
    UInt32 now = cpu->systemTime;
    UInt32 delay = 0x3fffffff;
    unsigned i;
    for (i = 0; i < 64; ++i) {
        if (timers[i] && timers[i]->active) {
            Int32 remaining = (Int32)(timers[i]->timeout - now);
            if (remaining <= 0) { delay = 0; break; }
            if ((UInt32)remaining < delay) delay = (UInt32)remaining;
        }
    }
    r800SetTimeoutAt(cpu, now + delay);
}

BoardTimer* boardTimerCreate(BoardTimerCb callback, void* ref)
{
    unsigned i;
    for (i = 0; i < 64; ++i) {
        if (!timers[i]) {
            timers[i] = calloc(1, sizeof(BoardTimer));
            timers[i]->callback = callback;
            timers[i]->ref = ref;
            return timers[i];
        }
    }
    a1stPanic("Emulated timer capacity exceeded.");
    return NULL;
}

void boardTimerRemove(BoardTimer* timer)
{
    timer->active = 0;
    scheduleCpu();
}

void boardTimerDestroy(BoardTimer* timer)
{
    unsigned i;
    for (i = 0; i < 64; ++i) {
        if (timers[i] == timer) {
            timers[i] = NULL;
            free(timer);
            scheduleCpu();
            return;
        }
    }
    a1stPanic("Destroying an unknown emulated timer.");
}

void boardTimerAdd(BoardTimer* timer, UInt32 timeout)
{
    timer->timeout = timeout;
    timer->active = 1;
    scheduleCpu();
}

void boardTimerCheckTimeout(void* ref)
{
    unsigned iterations;
    (void)ref;
    updateTime();
    for (iterations = 0; iterations < 10000; ++iterations) {
        BoardTimer* next = NULL;
        unsigned i;
        for (i = 0; i < 64; ++i) {
            BoardTimer* timer = timers[i];
            if (timer && timer->active &&
                (Int32)(timer->timeout - cpu->systemTime) <= 0 &&
                (!next || (Int32)(timer->timeout - next->timeout) < 0))
                next = timer;
        }
        if (!next) { scheduleCpu(); return; }
        next->active = 0;
        next->callback(next->ref, next->timeout);
    }
    a1stPanic("Emulated timer failed to advance.");
}

UInt64 boardSystemTime64(void)
{
    updateTime();
    return ticks * 100000;
}

UInt32 boardCalcRelativeTimeout(UInt32 frequency, UInt32 next)
{
    UInt64 interval = (UInt64)100000 * boardFrequency() / frequency;
    UInt64 now = boardSystemTime64();
    return (UInt32)((interval * (now / interval + next)) / 100000);
}

void boardSetInt(UInt32 mask)
{
    interrupts |= mask;
    r800SetInt(cpu);
}
void boardClearInt(UInt32 mask)
{
    interrupts &= ~mask;
    if (!interrupts) r800ClearInt(cpu);
}
UInt32 boardGetInt(UInt32 mask) { return interrupts & mask; }
Mixer* boardGetMixer(void) { return mixer; }
BoardType boardGetType(void) { return BOARD_MSX_T9769C; }
int boardGetYm2413Enable(void) { return 1; }
int boardGetYm2413Oversampling(void) { return 1; }
int boardGetVideoAutodetect(void) { return 0; }
void boardSetFdcActive(void) {}
void boardOnBreakpoint(UInt16 pc) { (void)pc; a1stPanic("Unexpected CPU breakpoint."); }

UInt8* boardGetRamPage(int page)
{
    return ram ? ram + (((unsigned)page * 0x2000) & (ramSize - 1)) : NULL;
}

void msxSetCpu(int mode)
{
    CpuMode next = mode ? CPU_R800 : CPU_Z80;
    if (cpu->cpuMode != next) ++switches;
    r800SetMode(cpu, next);
}

static void ppiWriteA(void* ref, UInt8 value)
{
    unsigned page;
    (void)ref;
    for (page = 0; page < 4; ++page)
        slotSetRamSlot(page, (value >> (2 * page)) & 3);
}
static UInt8 ppiReadB(void* ref) { (void)ref; return keyboard[keyboardRow & 15]; }
static UInt8 ppiReadPort(void* ref, UInt16 port) { return i8255Read(ref, port); }
static void ppiWritePort(void* ref, UInt16 port, UInt8 value) { i8255Write(ref, port, value); }
static void ppiWriteLow(void* ref, UInt8 value) { (void)ref; keyboardRow = value & 15; }
static void ppiWriteHigh(void* ref, UInt8 value)
{
    (void)ref;
    dacWrite(click, DAC_CH_MONO, (value & 8) ? 255 : 0);
}
static UInt8 psgRead(void* ref, UInt16 port)
{
    unsigned selected = (psgOutput >> 6) & 1;
    (void)ref;
    return (port & 1) ? psgOutput :
        (0xc0 | ((~(joysticks >> (selected * 8))) & 0x3f));
}
static void psgWrite(void* ref, UInt16 port, UInt8 value)
{
    (void)ref;
    if (port & 1) psgOutput = value;
}

static void deadline(void* ref, UInt32 time)
{
    (void)ref; (void)time;
    frameTimedOut = 1;
    r800StopExecution(cpu);
}

void RefreshScreen(int mode)
{
    (void)mode;
    ++frames;
    frameComplete = 1;
    r800StopExecution(cpu);
}

static bool readRom(const char* path, size_t expected, UInt8* buffer,
                     size_t count, char* error, size_t capacity)
{
    FILE* file = fopen(path, "rb");
    long length = -1;
    bool ok = false;
    UInt8 chunk[4096] __attribute__((aligned(4)));
    if (file) {
        if (!fseek(file, 0, SEEK_END)) length = ftell(file);
        if (length == (long)expected && !fseek(file, 0, SEEK_SET)) {
            size_t position = 0;
            while (position < count) {
                size_t bytes = count - position;
                if (bytes > sizeof(chunk)) bytes = sizeof(chunk);
                if (fread(chunk, 1, bytes, file) != bytes) break;
                memcpy(buffer + position, chunk, bytes);
                position += bytes;
#ifdef ESP_PLATFORM
                vTaskDelay(1);
#endif
            }
            ok = position == count;
        }
        if (fclose(file)) ok = false;
    }
    if (!ok) {
        snprintf(error, capacity, "Cannot read %s (expected %u bytes; found %ld).",
                 path, (unsigned)expected, length);
        a1stLog(error);
    }
    return ok;
}

bool a1stCreate(const char* firmware, const char* kanji, const char* directory,
                const A1stHost* callbacks, char* error, size_t capacity)
{
    UInt8* rom;
    UInt8* font;
    unsigned i;
    int pan[3] = { 0, 0, 0 };
    char cmos[432];
    if (!error || !capacity) {
        a1stLog("A1ST startup requires an error buffer.");
        return false;
    }
    if (cpu) { snprintf(error, capacity, "A1ST is already running."); a1stLog(error); return false; }
    if (!firmware || !kanji || !directory || !callbacks) {
        snprintf(error, capacity, "Invalid A1ST startup arguments.");
        a1stLog(error);
        return false;
    }
    host = *callbacks;
    if (!directory || strlen(directory) >= sizeof(stateDirectory)) {
        snprintf(error, capacity, "Invalid A1ST state directory."); a1stLog(error); return false;
    }
    strcpy(stateDirectory, directory);
    /* The ST maps 256 8 KiB banks, even when its dump contains 4 MiB. */
    rom = malloc(0x200000);
    font = malloc(A1ST_KANJI_BYTES);
    if (!readRom(firmware, A1ST_FIRMWARE_BYTES, rom, 0x200000, error, capacity) ||
        !readRom(kanji, A1ST_KANJI_BYTES, font, A1ST_KANJI_BYTES, error, capacity)) {
        free(rom); free(font); return false;
    }
    frames = switches = interrupts = lastTime = 0;
    ticks = 0;
    memset(keyboard, 0xff, sizeof(keyboard));
    keyboardRow = psgOutput = 0;
    joysticks = 0;
    memset(timers, 0, sizeof(timers));
    cpu = r800Create(CPU_ENABLE_M1 | CPU_VDP_IO_DELAY, slotRead, slotWrite,
                     ioPortRead, ioPortWrite, NULL, boardTimerCheckTimeout,
                     NULL, NULL, NULL, NULL, NULL, NULL);
    boardSysTime = &cpu->systemTime;
    slotManagerSetR800(cpu);
    deviceManagerCreate();
    debugDeviceManagerReset();
    videoManagerReset();
    ioPortReset();
    ramMapperIoCreate();
    slotManagerCreate();
    slotSetSubslotted(0, 1);
    slotSetSubslotted(3, 1);
    mixer = mixerCreate();
    mixerSetBoardFrequencyFixed(3579545);
    mixerSetSampleRate(mixer, 22050);
    mixerSetStereo(mixer, 0);
    mixerSetMasterVolume(mixer, 80);
    mixerEnableMaster(mixer, 1);
    for (i = 0; i < MIXER_CHANNEL_TYPE_COUNT; ++i) {
        mixerSetChannelTypeVolume(mixer, i, 80);
        mixerEnableChannelType(mixer, i, 1);
    }
    mixerReset(mixer);
    if (!ramMapperCreate(A1ST_RAM_BYTES, 3, 0, 0, &ram, &ramSize))
        a1stPanic("Cannot create A1ST RAM mapper.");
    if (!romMapperDramCreate("BIOS", rom + 40 * 0x2000, 0x8000, 0, 0, 0) ||
        !romMapperDramCreate("SUB", rom + 56 * 0x2000, 0x4000, 3, 1, 0) ||
        !romMapperDramCreate("KANJI BASIC", rom + 58 * 0x2000, 0x8000, 3, 1, 2) ||
        !romMapperNormalCreate("OPENING", rom + 36 * 0x2000, 0x4000, 0, 3, 2) ||
        !romMapperMsxMusicCreate("MSX-MUSIC", rom + 62 * 0x2000, 0x4000, 0, 2, 2) ||
        !romMapperTC8566AFCreate("DISK", rom + 48 * 0x2000, 0x10000, 3, 2, 2, ROM_TC8566AF_TR) ||
        !romMapperPanasonicCreate("fs-a1st", rom, 0x200000, 3, 3, 0, 0x4000, 8) ||
        !romMapperKanjiCreate(font, A1ST_KANJI_BYTES))
        a1stPanic("Cannot construct A1ST ROM mappings.");
    free(rom);
    free(font);
    click = dacCreate(mixer, DAC_MONO);
    ppi = i8255Create(NULL, NULL, ppiWriteA, ppiReadB, ppiReadB, NULL,
                      NULL, NULL, ppiWriteLow, NULL, NULL, ppiWriteHigh, NULL, 0);
    for (i = 0xa8; i <= 0xab; ++i)
        ioPortRegister(i, ppiReadPort, ppiWritePort, ppi);
    psg = ay8910Create(mixer, AY8910_MSX, PSGTYPE_YM2149, 0, pan);
    ay8910SetIoPort(psg, psgRead, psgRead, psgWrite, NULL);
    snprintf(cmos, sizeof(cmos), "%s/fs-a1st.cmos", directory);
    rtc = rtcCreate(1, cmos);
    romMapperS1990Create();
    romMapperF4deviceCreate(0);
    romMapperTurboRTimerCreate();
    romMapperTurboRIOCreate();
    a1stPcmCreate();
    vdpCreate(VDP_MSX, VDP_V9958, VDP_SYNC_AUTO, 8);
    for (i = 0; i < 4; ++i) slotSetRamSlot(i, 0);
    switchSetFront(0);
    frameFence = boardTimerCreate(deadline, NULL);
    if (capacity) error[0] = 0;
    return true;
}

void a1stRunFrame(void)
{
    UInt32 count;
    Int16* samples;
    if (!cpu) a1stPanic("A1ST has not been initialized.");
    frameComplete = frameTimedOut = 0;
    boardTimerAdd(frameFence, cpu->systemTime + boardFrequency() / 10);
    cpu->terminate = 0;
    r800Execute(cpu);
    boardTimerRemove(frameFence);
    updateTime();
    if (!frameComplete || frameTimedOut) a1stPanic("A1ST VDP frame deadline expired.");
    a1stCheckFrameBounds();
    mixerSync(mixer);
    samples = mixerGetBuffer(mixer, &count);
    if (host.audio && count) host.audio(samples, count);
}

void a1stSetKeyboard(const uint8_t matrix[16]) { memcpy(keyboard, matrix, 16); }
void a1stSetJoysticks(uint16_t value) { joysticks = value; }
void a1stSetFirmwareEnabled(bool enabled) { switchSetFront(enabled ? 1 : 0); }
uint8_t a1stReadMemory(uint16_t address) { return slotRead(NULL, address); }
void a1stWriteMemory(uint16_t address, uint8_t value) { slotWrite(NULL, address, value); }
uint8_t a1stReadPort(uint16_t port) { return ioPortRead(NULL, port); }
void a1stWritePort(uint16_t port, uint8_t value) { ioPortWrite(NULL, port, value); }

void a1stGetStats(A1stStats* stats)
{
    UInt8 selection;
    memset(stats, 0, sizeof(*stats));
    if (!cpu) return;
    updateTime();
    stats->masterTicks = ticks;
    stats->frames = frames;
    stats->cpuSwitches = switches;
    stats->pc = cpu->regs.PC.W;
    stats->r800 = cpu->cpuMode == CPU_R800;
    selection = ioPortRead(NULL, 0xe4);
    ioPortWrite(NULL, 0xe4, 6);
    stats->dram = !(ioPortRead(NULL, 0xe5) & 0x40);
    ioPortWrite(NULL, 0xe4, selection);
    stats->refreshRate = vdpGetRefreshRate();
}

bool a1stFlushState(char* error, size_t capacity)
{
    if (!a1stSaveSram(error, capacity)) return false;
    if (rtc && !rtcFlush(rtc)) {
        snprintf(error, capacity, "Cannot persist A1ST CMOS: %s.", strerror(errno));
        a1stLog(error);
        return false;
    }
    return true;
}

void a1stDestroy(void)
{
    unsigned i;
    if (!cpu) return;
    a1stDiskClose();
    rtcDestroy(rtc);
    ay8910Destroy(psg);
    i8255Destroy(ppi);
    dacDestroy(click);
    deviceManagerDestroy();
    slotManagerDestroy();
    mixerDestroy(mixer);
    for (i = 0; i < 64; ++i) { free(timers[i]); timers[i] = NULL; }
    r800Destroy(cpu);
    cpu = NULL; boardSysTime = NULL; ram = NULL; mixer = NULL;
    rtc = NULL; psg = NULL; ppi = NULL; click = NULL;
}
