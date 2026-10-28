/* Embedded A1ST integration; blueMSX and openMSX hardware references are listed
 * in docs/licenses.md. No fMSX core or BIOS patches are used. */
#include "A1stInternal.h"
#include "A1stProfile.h"
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
#include <fcntl.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
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
static unsigned soundSources = A1ST_SOUND_ALL;
static bool renderFrame = true;
static UInt8 clickLevel;
static UInt32 clickEdges;
static void ppiWriteHigh(void* ref, UInt8 value)
{
    (void)ref;
    if ((value & 8) != clickLevel) ++clickEdges;
    clickLevel = value & 8;
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

bool a1stReadRom(const char* path, size_t expected, UInt8* buffer,
                     size_t count, char* error, size_t capacity)
{
    return a1stReadRomBlocks(path, expected, &buffer, count, count, error, capacity);
}

typedef struct {
    UInt8* const* blocks;
    size_t blockSize;
} RomBlocks;

static void copyRomBlocks(void* context, size_t offset, const UInt8* bytes, size_t count)
{
    const RomBlocks* output = context;
    while (count) {
        size_t blockOffset = offset % output->blockSize;
        size_t copied = output->blockSize - blockOffset;
        if (copied > count) copied = count;
        memcpy(output->blocks[offset / output->blockSize] + blockOffset, bytes, copied);
        offset += copied;
        bytes += copied;
        count -= copied;
    }
}

bool a1stReadRomBlocks(const char* path, size_t expected, UInt8* const* blocks,
                       size_t blockSize, size_t count, char* error, size_t capacity)
{
    RomBlocks output = { blocks, blockSize };
    return a1stReadRomStream(path, expected, count, copyRomBlocks, &output, error, capacity);
}

bool a1stReadRomStream(const char* path, size_t expected, size_t count,
                       void (*visit)(void*, size_t, const UInt8*, size_t), void* context,
                       char* error, size_t capacity)
{
    int flags = O_RDONLY;
#ifdef _WIN32
    flags |= O_BINARY;
#endif
    int file = open(path, flags);
    struct stat info;
    long length = -1;
    size_t position = 0;
    const char* stage = NULL;
    int savedErrno = 0;
    UInt8 chunk[4096] __attribute__((aligned(4)));
    if (file < 0) {
        stage = "open";
        savedErrno = errno;
    }
    else {
        if (fstat(file, &info)) {
            stage = "stat";
            savedErrno = errno;
        }
        else if ((length = info.st_size) != (long)expected) stage = "size";
        else {
            /* Read sequentially without FAT seeks or heap-backed stdio buffers. */
            while (position < count) {
                size_t bytes = count - position;
                long received;
                if (bytes > sizeof(chunk)) bytes = sizeof(chunk);
                received = read(file, chunk, bytes);
                if (received < 0) {
                    if (errno == EINTR) continue;
                    stage = "read";
                    savedErrno = errno;
                    break;
                }
                if (!received) { stage = "EOF"; break; }
                visit(context, position, chunk, (size_t)received);
                position += (size_t)received;
#ifdef ESP_PLATFORM
                vTaskDelay(1);
#endif
            }
        }
        if (close(file) && !stage) {
            stage = "close";
            savedErrno = errno;
        }
    }
    if (stage) {
        char message[512];
        if (!strcmp(stage, "size"))
            snprintf(message, sizeof(message),
                     "ROM size mismatch: expected %u bytes; found %ld. %s",
                     (unsigned)expected, length, path);
        else if (!strcmp(stage, "EOF"))
            snprintf(message, sizeof(message),
                     "ROM ended after %u/%u bytes. %s",
                     (unsigned)position, (unsigned)count, path);
        else
            snprintf(message, sizeof(message),
                     "ROM %s failed at %u/%u bytes: %s (errno %d). %s",
                     stage, (unsigned)position, (unsigned)count,
                     strerror(savedErrno), savedErrno, path);
        a1stLog(message);
        snprintf(error, capacity, "%s", message);
        return false;
    }
    return true;
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
    a1stProfileReset();
    if (!directory || strlen(directory) >= sizeof(stateDirectory)) {
        snprintf(error, capacity, "Invalid A1ST state directory."); a1stLog(error); return false;
    }
    strcpy(stateDirectory, directory);
    /* The ST maps 256 8 KiB banks, even when its dump contains 4 MiB. */
    rom = malloc(0x200000);
    font = malloc(A1ST_KANJI_BYTES);
    if (!a1stReadRom(firmware, A1ST_FIRMWARE_BYTES, rom, 0x200000, error, capacity) ||
        !a1stReadRom(kanji, A1ST_KANJI_BYTES, font, A1ST_KANJI_BYTES, error, capacity)) {
        free(rom); free(font); return false;
    }
    frames = switches = interrupts = lastTime = 0;
    renderFrame = true;
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
    soundSources = A1ST_SOUND_ALL;
    clickLevel = 0;
    clickEdges = 0;
    click = dacCreateTyped(mixer, DAC_MONO, MIXER_CHANNEL_KEYBOARD);
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

bool a1stFrameRendering(void) { return renderFrame; }

void a1stRunFrame(void)
{
    a1stRunFrameWithVideo(true);
}

void a1stRunFrameWithVideo(bool renderPixels)
{
    UInt32 count;
    Int16* samples;
    if (!cpu) a1stPanic("A1ST has not been initialized.");
    renderFrame = renderPixels;
    a1stProfileStart(a1stProfileNow());
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
    a1stProfileFinish(a1stProfileNow());
}

void a1stSetKeyboard(const uint8_t matrix[16]) { memcpy(keyboard, matrix, 16); }
void a1stSetJoysticks(uint16_t value) { joysticks = value; }
void a1stSetFirmwareEnabled(bool enabled) { switchSetFront(enabled ? 1 : 0); }
bool a1stSetSoundSources(unsigned sources, char* error, size_t capacity)
{
    if (!mixer || (sources & ~A1ST_SOUND_ALL)) {
        snprintf(error, capacity, "Invalid sound sources or A1ST is not running.");
        a1stLog(error);
        return false;
    }
    mixerSync(mixer);
    mixerEnableChannelType(mixer, MIXER_CHANNEL_PSG, sources & A1ST_SOUND_PSG);
    mixerEnableChannelType(mixer, MIXER_CHANNEL_MSXMUSIC, !!(sources & A1ST_SOUND_MSXMUSIC));
    mixerEnableChannelType(mixer, MIXER_CHANNEL_PCM, !!(sources & A1ST_SOUND_PCM));
    mixerEnableChannelType(mixer, MIXER_CHANNEL_SCC, !!(sources & A1ST_SOUND_SCC));
    mixerEnableChannelType(mixer, MIXER_CHANNEL_KEYBOARD, !!(sources & A1ST_SOUND_CLICK));
    soundSources = sources;
    error[0] = 0;
    return true;
}

void a1stGetAudioDebug(A1stAudioDebug* stats)
{
    memset(stats, 0, sizeof(*stats));
    if (!cpu) return;
    ay8910GetRegisters(psg, stats->psg);
    stats->pcmStatus = ioPortRead(NULL, 0xa5);
    stats->sources = soundSources;
    stats->clickEdges = clickEdges;
}
uint8_t a1stReadMemory(uint16_t address) { return slotRead(NULL, address); }
void a1stWriteMemory(uint16_t address, uint8_t value) { slotWrite(NULL, address, value); }
uint8_t a1stReadPort(uint16_t port) { return ioPortRead(NULL, port); }
void a1stWritePort(uint16_t port, uint8_t value) { ioPortWrite(NULL, port, value); }

void a1stGetWorkStats(A1stWorkStats* stats)
{
    a1stProfileTake(stats);
    stats->cpuBytes = cpu ? sizeof(*cpu) : 0;
}

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
    if (!a1stCartridgeFlush(error, capacity)) return false;
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
    a1stCartridgeClose();
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
