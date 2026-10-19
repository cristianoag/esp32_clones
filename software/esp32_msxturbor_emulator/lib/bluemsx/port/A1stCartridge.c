#include "A1stInternal.h"
#include "SlotManager.h"
#include "IoPort.h"
#include "SCC.h"
#include "Board.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#ifdef ESP_PLATFORM
#include <esp_heap_caps.h>
#endif
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#undef malloc

typedef struct {
    UInt8* rom;
    size_t size;
    int mapper;
    unsigned slot;
    unsigned banks[4];
    UInt8 sram[8192];
    UInt8 fmKey[2];
    UInt8 fmEnable;
    bool dirty;
    SCC* scc;
    char statePath[512];
} Cartridge;

static Cartridge carts[2];

static bool fail(char* error, size_t capacity, const char* message)
{
    snprintf(error, capacity, "%s", message);
    a1stLog(error);
    return false;
}

static int detect(const UInt8* rom, size_t size)
{
    unsigned scores[6] = {0};
    size_t i;
    int best = A1ST_GENERIC8;
    if (size == 65536 && !memcmp(rom + 0x18, "PAC2", 4)) return A1ST_FMPAC;
    if (size <= 32768 || (size <= 65536 && rom[0x4000] == 'A' && rom[0x4001] == 'B')) return -1;
    for (i = 0; i + 2 < size; ++i) {
        unsigned address;
        if (rom[i] != 0x32) continue; /* LD (nn),A bank-register writes. */
        address = rom[i + 1] | (rom[i + 2] << 8);
        if (address == 0x5000 || address == 0x9000 || address == 0xb000)
            scores[A1ST_KONAMI_SCC] += 3;
        if (address == 0x8000 || address == 0xa000) scores[A1ST_KONAMI] += 2;
        if (address == 0x6800 || address == 0x7800) scores[A1ST_ASCII8] += 3;
        if (address == 0x6000) { ++scores[A1ST_ASCII8]; ++scores[A1ST_ASCII16]; ++scores[A1ST_KONAMI]; }
        if (address == 0x7000) { ++scores[A1ST_ASCII8]; ++scores[A1ST_ASCII16]; ++scores[A1ST_KONAMI_SCC]; }
        if (address == 0x77ff) scores[A1ST_ASCII16] += 3;
    }
    for (i = 1; i < 6; ++i) if (scores[i] > scores[best]) best = (int)i;
    return best;
}

static UInt8* load(const char* path, size_t* size, char* error, size_t capacity)
{
    struct stat info;
    UInt8* rom;
    bool signature;
    if (!path || !*path || stat(path, &info) || !S_ISREG(info.st_mode)) {
        fail(error, capacity, "Cartridge ROM is missing or is not a regular file.");
        return NULL;
    }
    if (info.st_size < 8192 || info.st_size > 2097152 || info.st_size % 8192) {
        fail(error, capacity, "ROM must use 8 KiB multiples, from 8 KiB to 2 MiB.");
        return NULL;
    }
    *size = (size_t)info.st_size;
#ifdef ESP_PLATFORM
    rom = heap_caps_malloc(*size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    rom = malloc(*size);
#endif
    if (!rom) {
        fail(error, capacity, "Not enough memory for cartridge ROM; previous slot retained.");
        return NULL;
    }
    if (!a1stReadRom(path, *size, rom, *size, error, capacity)) { free(rom); return NULL; }
    signature = rom[0] == 'A' && rom[1] == 'B';
    if (*size >= 0x4002) signature |= rom[0x4000] == 'A' && rom[0x4001] == 'B';
    if (*size >= 0x4000) signature |= rom[*size - 0x4000] == 'A' && rom[*size - 0x3fff] == 'B';
    if (!signature) {
        free(rom);
        fail(error, capacity, "Invalid cartridge ROM: AB header not found.");
        return NULL;
    }
    return rom;
}

bool a1stInspectCartridge(const char* path, int* detected, char* error, size_t capacity)
{
    size_t size;
    UInt8* rom = load(path, &size, error, capacity);
    if (!rom) return false;
    *detected = detect(rom, size);
    free(rom);
    error[0] = 0;
    return true;
}

static bool sramEnabled(const Cartridge* cart)
{
    return cart->fmKey[0] == 0x4d && cart->fmKey[1] == 0x69;
}

static void mapBank(Cartridge* cart, unsigned window, unsigned bank)
{
    unsigned page = window + 2;
    bool scc = cart->mapper == A1ST_KONAMI_SCC && window == 2 && (bank & 63) == 63;
    cart->banks[window] = bank;
    if (cart->mapper == A1ST_GAMEMASTER2 && (bank & 0x10)) {
        /* Each 4 KiB SRAM bank is mirrored inside its 8 KiB window. */
        slotMapPage(cart->slot, 0, page, cart->sram, 0, 0);
    }
    else {
        if (cart->mapper == A1ST_GAMEMASTER2) bank &= 15;
        slotMapPage(cart->slot, 0, page,
                    cart->rom + (bank % (cart->size / 8192)) * 8192, !scc, 0);
    }
}

static UInt8 readCart(void* ref, UInt16 address)
{
    Cartridge* cart = ref;
    unsigned window;
    unsigned bank;
    if (cart->mapper == A1ST_FMPAC) {
        if (address < 0x4000 || address >= 0x8000) return 0xff;
        if (address == 0x7ff6) return cart->fmEnable;
        if (address == 0x7ff7) return cart->banks[0];
        if (!sramEnabled(cart)) return cart->rom[cart->banks[0] * 16384 + (address & 0x3fff)];
        if (address == 0x5ffe || address == 0x5fff) return cart->fmKey[address & 1];
        return address < 0x6000 ? cart->sram[address & 0x1fff] : 0xff;
    }
    if (address < 0x4000 || address >= 0xc000) return 0xff;
    window = (address - 0x4000) / 8192;
    bank = cart->banks[window];
    if (cart->mapper == A1ST_KONAMI_SCC && window == 2 && (bank & 63) == 63 &&
        address >= 0x9800 && address < 0xa000)
        return sccRead(cart->scc, address & 255);
    if (cart->mapper == A1ST_GAMEMASTER2 && (bank & 0x10))
        return cart->sram[((bank & 0x20) ? 4096 : 0) + (address & 4095)];
    return cart->rom[(bank % (cart->size / 8192)) * 8192 + (address & 8191)];
}

static void writeCart(void* ref, UInt16 address, UInt8 value)
{
    Cartridge* cart = ref;
    unsigned window;
    if (address < 0x4000 || address >= 0xc000) return;
    window = (address - 0x4000) / 8192;
    switch (cart->mapper) {
    case A1ST_GENERIC8:
        mapBank(cart, window, value);
        break;
    case A1ST_GENERIC16:
        window &= ~1U;
        mapBank(cart, window, value * 2);
        mapBank(cart, window + 1, value * 2 + 1);
        break;
    case A1ST_KONAMI:
        if (address >= 0x6000) mapBank(cart, window, value);
        break;
    case A1ST_KONAMI_SCC:
        if ((address & 0xf800) == 0x5000 || (address & 0xf800) == 0x7000 ||
            (address & 0xf800) == 0x9000 || (address & 0xf800) == 0xb000)
            mapBank(cart, window, value);
        else if (address >= 0x9800 && address < 0xa000 && (cart->banks[2] & 63) == 63)
            sccWrite(cart->scc, address & 255, value);
        break;
    case A1ST_ASCII8:
        if (address >= 0x6000 && address < 0x8000)
            mapBank(cart, (address - 0x6000) / 0x800, value);
        break;
    case A1ST_ASCII16:
        if ((address & 0xf800) == 0x6000 || (address & 0xf800) == 0x7000) {
            window = (address & 0x1000) ? 2 : 0;
            mapBank(cart, window, value * 2);
            mapBank(cart, window + 1, value * 2 + 1);
        }
        break;
    case A1ST_GAMEMASTER2:
        if ((address & 0xf000) == 0x6000 || (address & 0xf000) == 0x8000 ||
            (address & 0xf000) == 0xa000) mapBank(cart, window, value);
        else if (address >= 0xb000 && (cart->banks[3] & 0x10)) {
            cart->sram[((cart->banks[3] & 0x20) ? 4096 : 0) + (address & 4095)] = value;
            cart->dirty = true;
        }
        break;
    case A1ST_FMPAC:
        if (address == 0x5ffe || address == 0x5fff) {
            if (!(cart->fmEnable & 0x10)) cart->fmKey[address & 1] = value;
        }
        else if (address == 0x7ff4 || address == 0x7ff5)
            ioPortWrite(NULL, 0x7c + (address & 1), value);
        else if (address == 0x7ff6) {
            cart->fmEnable = value & 0x11;
            if (value & 0x10) cart->fmKey[0] = cart->fmKey[1] = 0;
        }
        else if (address == 0x7ff7) cart->banks[0] = value & 3;
        else if (address < 0x5ffe && sramEnabled(cart)) {
            cart->sram[address & 8191] = value;
            cart->dirty = true;
        }
        break;
    }
}

static bool flushCart(Cartridge* cart, char* error, size_t capacity)
{
    FILE* file;
    bool ok;
    if (!cart->dirty) return true;
    file = fopen(cart->statePath, "wb");
    ok = file && fwrite(cart->sram, 1, sizeof(cart->sram), file) == sizeof(cart->sram) && !fflush(file);
    if (file) {
#ifdef _WIN32
        if (_commit(_fileno(file))) ok = false;
#else
        if (fsync(fileno(file))) ok = false;
#endif
        if (fclose(file)) ok = false;
    }
    if (!ok) return fail(error, capacity, "Cannot persist cartridge SRAM; check SD card.");
    cart->dirty = false;
    return true;
}

bool a1stCartridgeFlush(char* error, size_t capacity)
{
    return flushCart(&carts[0], error, capacity) && flushCart(&carts[1], error, capacity);
}

static void closeCart(Cartridge* cart)
{
    if (!cart->rom) return;
    slotUnregister(cart->slot, 0, 0);
    if (cart->scc) sccDestroy(cart->scc);
    free(cart->rom);
    memset(cart, 0, sizeof(*cart));
}

void a1stCartridgeClose(void)
{
    closeCart(&carts[0]);
    closeCart(&carts[1]);
}

bool a1stAttachCartridge(unsigned slot, const char* path, unsigned mapper,
                         char* error, size_t capacity)
{
    Cartridge* cart;
    UInt8* rom;
    size_t size;
    int resolved;
    unsigned page;
    UInt8 saved[8192];
    char statePath[512];
    if (slot > 1 || mapper > A1ST_MAPPER_AUTO)
        return fail(error, capacity, "Invalid cartridge slot or mapper.");
    cart = &carts[slot];
    if (!flushCart(cart, error, capacity)) return false;
    if (!path || !*path) { closeCart(cart); error[0] = 0; return true; }
    if (!boardGetMixer()) return fail(error, capacity, "Boot the A1ST core before attaching a cartridge.");
    rom = load(path, &size, error, capacity);
    if (!rom) return false;
    resolved = mapper == A1ST_MAPPER_AUTO ? detect(rom, size) : (int)mapper;
    if (resolved == A1ST_FMPAC && size != 65536) {
        free(rom);
        return fail(error, capacity, "FM-PAC requires a 64 KiB ROM.");
    }
    if (resolved == A1ST_GAMEMASTER2 && size != 131072) {
        free(rom);
        return fail(error, capacity, "GameMaster2 requires a 128 KiB ROM.");
    }
    if (snprintf(statePath, sizeof(statePath), "%s.slot%u.sram", path, slot + 1) >= (int)sizeof(statePath)) {
        free(rom);
        return fail(error, capacity, "Cartridge SRAM path is too long.");
    }
    memset(saved, 0xff, sizeof(saved));
    if (resolved == A1ST_FMPAC || resolved == A1ST_GAMEMASTER2) {
        struct stat info;
        if (!stat(statePath, &info)) {
            if (!a1stReadRom(statePath, sizeof(saved), saved, sizeof(saved), error, capacity)) {
                free(rom); return false;
            }
        }
        else if (errno != ENOENT) {
            free(rom);
            return fail(error, capacity, "Cannot inspect cartridge SRAM; previous slot retained.");
        }
    }
    closeCart(cart);
    cart->rom = rom;
    cart->size = size;
    cart->mapper = resolved;
    cart->slot = slot + 1;
    memcpy(cart->sram, saved, sizeof(saved));
    strcpy(cart->statePath, statePath);
    if (resolved == A1ST_KONAMI_SCC) cart->scc = sccCreate(boardGetMixer());
    slotRegister(cart->slot, 0, 0, 8, readCart, readCart, writeCart, NULL, cart);
    for (page = 0; page < 8; ++page) slotMapPage(cart->slot, 0, page, rom, 0, 0);
    if (resolved < 0) {
        unsigned pages = size / 8192;
        bool basic = rom[0] == 'A' && rom[1] == 'B' && !rom[2] && !rom[3] && (rom[8] || rom[9]);
        for (page = 0; page < 8; ++page) {
            unsigned bank;
            if (basic && pages <= 2 && (page < 4 || page > 5)) continue;
            if (pages <= 2) bank = page % pages;
            else if (pages <= 4) {
                bank = (page < 4 ? 0 : 2) + (page & 1);
                if (bank >= pages) bank = pages - 1;
            }
            else bank = page < pages ? page : page - 4;
            slotMapPage(cart->slot, 0, page, rom + bank * 8192, 1, 0);
        }
    }
    else if (resolved == A1ST_FMPAC) {
        slotMapPage(cart->slot, 0, 2, rom, 0, 0);
        slotMapPage(cart->slot, 0, 3, rom, 0, 0);
    }
    else for (page = 0; page < 4; ++page) {
        unsigned bank = page;
        if (resolved == A1ST_GENERIC16 && size > 32768 && page >= 2)
            bank = size / 8192 - 4 + page;
        mapBank(cart, page, bank);
    }
    error[0] = 0;
    return true;
}
