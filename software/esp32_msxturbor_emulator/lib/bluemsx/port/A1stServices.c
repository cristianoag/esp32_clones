#include "A1stInternal.h"
#include "SaveState.h"
#include "sramLoader.h"
#include "FdcAudio.h"
#include "ArchTimer.h"
#include "ArchMidi.h"
#include "ArchNotifications.h"
#include "Language.h"
#include "Board.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#ifdef ESP_PLATFORM
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <unistd.h>
#else
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#endif

#undef malloc
#undef calloc
#undef realloc

void a1stPanic(const char* message)
{
    a1stLog(message);
#ifdef ESP_PLATFORM
    for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
#else
    abort();
#endif
}

void* a1stAllocate(size_t size)
{
    void* pointer;
#ifdef ESP_PLATFORM
    const unsigned caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    pointer = NULL;
    if (size <= 16384 && heap_caps_get_free_size(caps) > size + 65536)
        pointer = heap_caps_malloc(size, caps);
    if (!pointer) pointer = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    pointer = malloc(size ? size : 1);
#endif
    if (!pointer) a1stPanic("A1ST memory allocation failed.");
    return pointer;
}
void* a1stCalloc(size_t count, size_t size)
{
    void* pointer;
    if (count && size > SIZE_MAX / count) a1stPanic("A1ST allocation size overflow.");
    pointer = a1stAllocate(count * size);
    memset(pointer, 0, count * size);
    return pointer;
}
void* a1stReallocate(void* pointer, size_t size)
{
    void* result;
#ifdef ESP_PLATFORM
    result = heap_caps_realloc(pointer, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    result = realloc(pointer, size);
#endif
    if (!result && size) a1stPanic("A1ST memory reallocation failed.");
    return result;
}

static UInt8* firmwareSram;
static UInt32 firmwareSramSize;
static char sramPath[432];

void panasonicSramCreate(UInt8* memory, UInt32 size)
{
    firmwareSram = memory; firmwareSramSize = size;
}
void panasonicSramDestroy(void) { firmwareSram = NULL; firmwareSramSize = 0; }
const char* sramCreateFilenameWithSuffix(const char* rom, char* suffix, char* extension)
{
    (void)rom; (void)suffix; (void)extension;
    snprintf(sramPath, sizeof(sramPath), "%s/fs-a1st.sram", a1stStateDirectory());
    return sramPath;
}
const char* sramCreateFilename(const char* rom)
{
    return sramCreateFilenameWithSuffix(rom, NULL, NULL);
}
void sramLoad(const char* path, UInt8* memory, int length, void* header, int headerLength)
{
    FILE* file = fopen(path, "rb");
    (void)header;
    if (headerLength) a1stPanic("Unexpected SRAM header format.");
    if (!file) {
        if (errno != ENOENT) a1stPanic("Cannot read A1ST firmware SRAM.");
        return;
    }
    if (fread(memory, 1, length, file) != (size_t)length || fgetc(file) != EOF) {
        fclose(file); a1stPanic("A1ST firmware SRAM has the wrong size.");
    }
    if (fclose(file)) a1stPanic("Cannot close A1ST firmware SRAM.");
}

bool a1stSaveSram(char* error, size_t capacity)
{
    FILE* file;
    bool ok;
    if (!firmwareSram) return true;
    file = fopen(sramPath, "wb");
    ok = file && fwrite(firmwareSram, 1, firmwareSramSize, file) == firmwareSramSize;
    if (file) {
        if (fflush(file)) ok = false;
#ifdef _WIN32
        if (_commit(_fileno(file))) ok = false;
#else
        if (fsync(fileno(file))) ok = false;
#endif
        if (fclose(file)) ok = false;
    }
    if (!ok) {
        snprintf(error, capacity, "Cannot persist A1ST firmware SRAM: %s.", strerror(errno));
        a1stLog(error);
    }
    return ok;
}
void sramSave(const char* path, UInt8* memory, int length, void* header, int headerLength)
{
    char error[160];
    (void)path; (void)memory; (void)length; (void)header;
    if (headerLength) a1stPanic("Unexpected SRAM save header.");
    a1stSaveSram(error, sizeof(error));
}

/* Save states and the desktop debugger are not exposed by this frontend. */
void saveStateCreateForRead(const char* path) { (void)path; a1stPanic("Save states are not supported."); }
void saveStateCreateForWrite(const char* path) { (void)path; a1stPanic("Save states are not supported."); }
void saveStateDestroy(void) { a1stPanic("Save states are not supported."); }
SaveState* saveStateOpenForRead(const char* path) { (void)path; a1stPanic("Save states are not supported."); return NULL; }
SaveState* saveStateOpenForWrite(const char* path) { (void)path; a1stPanic("Save states are not supported."); return NULL; }
void saveStateClose(SaveState* state) { (void)state; a1stPanic("Save states are not supported."); }
UInt32 saveStateGet(SaveState* state, const char* tag, UInt32 value)
{ (void)state; (void)tag; (void)value; a1stPanic("Save states are not supported."); return 0; }
void saveStateSet(SaveState* state, const char* tag, UInt32 value)
{ (void)state; (void)tag; (void)value; a1stPanic("Save states are not supported."); }
void saveStateGetBuffer(SaveState* state, const char* tag, void* buffer, UInt32 length)
{ (void)state; (void)tag; (void)buffer; (void)length; a1stPanic("Save states are not supported."); }
void saveStateSetBuffer(SaveState* state, const char* tag, void* buffer, UInt32 length)
{ (void)state; (void)tag; (void)buffer; (void)length; a1stPanic("Save states are not supported."); }
void frameBufferSaveState(void) { a1stPanic("Save states are not supported."); }
void frameBufferLoadState(void) { a1stPanic("Save states are not supported."); }
int debuggerCheckVramAccess(void) { return 0; }

UInt32 archGetSystemUpTime(UInt32 frequency)
{
    return (UInt32)(boardSystemTime64() / 100000 * frequency / boardFrequency());
}
int archMidiGetNoteOn(void) { return 0; }
void archMidiUpdateVolume(int left, int right) { (void)left; (void)right; }
void archVideoOutputChange(void) {}

/* Mechanical drive noise is deliberately omitted, not the FDC itself. */
struct FdcAudio { int motor; };
FdcAudio* fdcAudioCreate(FdcAudioType type) { (void)type; return a1stCalloc(1, sizeof(FdcAudio)); }
void fdcAudioDestroy(FdcAudio* audio) { free(audio); }
void fdcAudioReset(FdcAudio* audio) { audio->motor = 0; }
void fdcAudioSetMotor(FdcAudio* audio, int motor) { audio->motor = motor; }
void fdcAudioSetReadWrite(FdcAudio* audio) { (void)audio; }

#define LABEL(function, text) char* function(void) { static char label[] = text; return label; }
LABEL(langDbgDevAy8910, "PSG")
LABEL(langDbgMemScc, "SCC")
LABEL(langDbgDevKanji, "Kanji")
LABEL(langDbgDevF4Device, "Reset status")
LABEL(langDbgDevMsxMusic, "MSX-MUSIC")
LABEL(langDbgDevRam, "RAM")
LABEL(langDbgDevRamMapper, "RAM mapper")
LABEL(langDbgDevRtc, "RTC")
LABEL(langDbgDevS1990, "S1990")
LABEL(langDbgDevTms9929A, "TMS9929A")
LABEL(langDbgDevTms99x8A, "TMS99x8A")
LABEL(langDbgDevTrPause, "TurboR pause")
LABEL(langDbgDevV9938, "V9938")
LABEL(langDbgDevV9958, "V9958")
LABEL(langDbgMemRamMapped, "Mapped")
LABEL(langDbgMemVram, "VRAM")
LABEL(langDbgRegs, "Registers")
LABEL(langDbgRegsYm2413, "YM2413 registers")
