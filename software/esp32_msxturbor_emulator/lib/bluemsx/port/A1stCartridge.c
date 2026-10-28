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
#undef calloc

typedef struct {
    UInt8* pages[512];
    size_t size;
} CartridgeRom;

typedef struct {
    CartridgeRom* rom;
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

static UInt8 romByte(const CartridgeRom* rom, size_t offset)
{
    return rom->pages[offset / 8192][offset & 8191];
}

static void freeRom(CartridgeRom* rom)
{
    unsigned page;
    if (!rom) return;
    for (page = 0; page < rom->size / 8192; ++page) free(rom->pages[page]);
    free(rom);
}

static bool fail(char* error, size_t capacity, const char* message)
{
    snprintf(error, capacity, "%s", message);
    a1stLog(error);
    return false;
}

typedef struct {
    size_t size;
    UInt8 header[32];
    UInt8 secondHeader[2];
    UInt8 lastHeader[2];
    UInt8 previous[2];
    unsigned scores[6];
} CartridgeInspection;

static void inspectBytes(void* context, size_t offset, const UInt8* bytes, size_t count)
{
    CartridgeInspection* inspection = context;
    unsigned* scores = inspection->scores;
    size_t i;
    for (i = 0; i < count; ++i) {
        const size_t position = offset + i;
        unsigned address;
        bool bankWrite = position >= 2 && inspection->previous[0] == 0x32;
        address = inspection->previous[1] | (bytes[i] << 8);
        inspection->previous[0] = inspection->previous[1];
        inspection->previous[1] = bytes[i];
        if (position < sizeof(inspection->header)) inspection->header[position] = bytes[i];
        if (position >= 0x4000 && position < 0x4002)
            inspection->secondHeader[position - 0x4000] = bytes[i];
        if (inspection->size >= 0x4000 && position >= inspection->size - 0x4000 &&
            position < inspection->size - 0x3ffe)
            inspection->lastHeader[position - (inspection->size - 0x4000)] = bytes[i];
        if (!bankWrite) continue; /* LD (nn),A bank-register writes. */
        if (address == 0x5000 || address == 0x9000 || address == 0xb000)
            scores[A1ST_KONAMI_SCC] += 3;
        if (address == 0x8000 || address == 0xa000) scores[A1ST_KONAMI] += 2;
        if (address == 0x6800 || address == 0x7800) scores[A1ST_ASCII8] += 3;
        if (address == 0x6000) { ++scores[A1ST_ASCII8]; ++scores[A1ST_ASCII16]; ++scores[A1ST_KONAMI]; }
        if (address == 0x7000) { ++scores[A1ST_ASCII8]; ++scores[A1ST_ASCII16]; ++scores[A1ST_KONAMI_SCC]; }
        if (address == 0x77ff) scores[A1ST_ASCII16] += 3;
    }
}

static int detect(const CartridgeInspection* inspection)
{
    const size_t size = inspection->size;
    const unsigned* scores = inspection->scores;
    int best = A1ST_GENERIC8;
    unsigned i;
    if (!memcmp(inspection->header + 0x10, "ASCII16X", 8)) return A1ST_ASCII16X;
    if (size == 65536 && !memcmp(inspection->header + 0x18, "PAC2", 4)) return A1ST_FMPAC;
    if (size <= 32768 || (size <= 65536 && inspection->secondHeader[0] == 'A' &&
                                         inspection->secondHeader[1] == 'B')) return -1;
    for (i = 1; i < 6; ++i) if (scores[i] > scores[best]) best = (int)i;
    return best;
}

static int detectRom(const CartridgeRom* rom)
{
    CartridgeInspection inspection = {0};
    unsigned page;
    inspection.size = rom->size;
    for (page = 0; page < rom->size / 8192; ++page)
        inspectBytes(&inspection, page * 8192, rom->pages[page], 8192);
    return detect(&inspection);
}

static size_t romSize(const char* path, char* error, size_t capacity)
{
    struct stat info;
    if (!path || !*path || stat(path, &info) || !S_ISREG(info.st_mode)) {
        fail(error, capacity, "Cartridge ROM is missing or is not a regular file.");
        return 0;
    }
    if (info.st_size < 8192 || info.st_size > 4194304 || info.st_size % 8192) {
        fail(error, capacity, "ROM must use 8 KiB multiples, from 8 KiB to 4 MiB.");
        return 0;
    }
    return (size_t)info.st_size;
}

static CartridgeRom* load(const char* path, char* error, size_t capacity)
{
    const size_t size = romSize(path, error, capacity);
    CartridgeRom* rom;
    unsigned page;
    bool signature;
    if (!size) return NULL;
    rom = calloc(1, sizeof(*rom));
    if (!rom) {
        fail(error, capacity, "Cartridge page table allocation failed; previous slot retained.");
        return NULL;
    }
    rom->size = size;
    /* Bank-sized allocations fit the holes left by A1ST startup in 8 MiB PSRAM. */
    for (page = 0; page < rom->size / 8192; ++page) {
#ifdef ESP_PLATFORM
        rom->pages[page] = heap_caps_malloc(8192, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
        rom->pages[page] = malloc(8192);
#endif
        if (!rom->pages[page]) {
            char message[192];
#ifdef ESP_PLATFORM
            snprintf(message, sizeof(message),
                     "Cartridge PSRAM exhausted: need %u bytes, allocated %u; free %u, largest %u. Previous slot retained.",
                     (unsigned)size, page * 8192,
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
                     (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
#else
            snprintf(message, sizeof(message),
                     "Cartridge PSRAM exhausted: need %u bytes, allocated %u. Previous slot retained.",
                     (unsigned)size, page * 8192);
#endif
            fail(error, capacity, message);
            freeRom(rom);
            return NULL;
        }
    }
    if (!a1stReadRomBlocks(path, rom->size, rom->pages, 8192, rom->size, error, capacity)) {
        freeRom(rom); return NULL;
    }
    signature = romByte(rom, 0) == 'A' && romByte(rom, 1) == 'B';
    if (rom->size >= 0x4002) signature |= romByte(rom, 0x4000) == 'A' && romByte(rom, 0x4001) == 'B';
    if (rom->size >= 0x4000) signature |= romByte(rom, rom->size - 0x4000) == 'A' &&
                                                       romByte(rom, rom->size - 0x3fff) == 'B';
    if (!signature) {
        freeRom(rom);
        fail(error, capacity, "Invalid cartridge ROM: AB header not found.");
        return NULL;
    }
    return rom;
}

bool a1stInspectCartridge(const char* path, int* detected, char* error, size_t capacity)
{
    CartridgeInspection inspection = {0};
    inspection.size = romSize(path, error, capacity);
    if (!inspection.size) return false;
    if (!a1stReadRomStream(path, inspection.size, inspection.size, inspectBytes, &inspection,
                           error, capacity)) return false;
    if (!((inspection.header[0] == 'A' && inspection.header[1] == 'B') ||
          (inspection.secondHeader[0] == 'A' && inspection.secondHeader[1] == 'B') ||
          (inspection.lastHeader[0] == 'A' && inspection.lastHeader[1] == 'B')))
        return fail(error, capacity, "Invalid cartridge ROM: AB header not found.");
    *detected = detect(&inspection);
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
    if (cart->mapper == A1ST_ASCII16X) {
        /* The cartridge's 8 MiB flash connects nine 16 KiB bank address bits. */
        bank &= 1023;
        cart->banks[window] = bank;
        slotMapPage(cart->slot, 0, page, cart->rom->pages[bank % (cart->rom->size / 8192)],
                    bank < cart->rom->size / 8192, 0);
        slotMapPage(cart->slot, 0, page ^ 4, cart->rom->pages[bank % (cart->rom->size / 8192)],
                    bank < cart->rom->size / 8192, 0);
        return;
    }
    cart->banks[window] = bank;
    if (cart->mapper == A1ST_GAMEMASTER2 && (bank & 0x10)) {
        /* Each 4 KiB SRAM bank is mirrored inside its 8 KiB window. */
        slotMapPage(cart->slot, 0, page, cart->sram, 0, 0);
    }
    else {
        if (cart->mapper == A1ST_GAMEMASTER2) bank &= 15;
        slotMapPage(cart->slot, 0, page,
                    cart->rom->pages[bank % (cart->rom->size / 8192)], !scc, 0);
    }
}

static UInt8 readCart(void* ref, UInt16 address)
{
    Cartridge* cart = ref;
    unsigned window;
    unsigned bank;
    if (cart->mapper == A1ST_ASCII16X) {
        window = ((address >> 13) ^ 2) & 3;
        bank = cart->banks[window];
        return bank < cart->rom->size / 8192 ? cart->rom->pages[bank][address & 8191] : 0xff;
    }
    if (cart->mapper == A1ST_FMPAC) {
        if (address < 0x4000 || address >= 0x8000) return 0xff;
        if (address == 0x7ff6) return cart->fmEnable;
        if (address == 0x7ff7) return cart->banks[0];
        if (!sramEnabled(cart)) return romByte(cart->rom, cart->banks[0] * 16384 + (address & 0x3fff));
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
    return cart->rom->pages[bank % (cart->rom->size / 8192)][address & 8191];
}

static void writeCart(void* ref, UInt16 address, UInt8 value)
{
    Cartridge* cart = ref;
    unsigned window;
    if (cart->mapper == A1ST_ASCII16X) {
        if ((address & 0x3fff) >= 0x2000) {
            unsigned bank = ((address & 0x0f00) | value) * 2;
            window = (address & 0x1000) ? 2 : 0;
            mapBank(cart, window, bank);
            mapBank(cart, window + 1, bank + 1);
        }
        return;
    }
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
    freeRom(cart->rom);
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
    CartridgeRom* rom;
    size_t size;
    int resolved;
    unsigned page;
    UInt8 saved[8192];
    char statePath[512];
    if (slot > 1 || mapper >= A1ST_MAPPER_COUNT)
        return fail(error, capacity, "Invalid cartridge slot or mapper.");
    cart = &carts[slot];
    if (!flushCart(cart, error, capacity)) return false;
    if (!path || !*path) { closeCart(cart); error[0] = 0; return true; }
    if (!boardGetMixer()) return fail(error, capacity, "Boot the A1ST core before attaching a cartridge.");
    rom = load(path, error, capacity);
    if (!rom) return false;
    size = rom->size;
    resolved = mapper == A1ST_MAPPER_AUTO ? detectRom(rom) : (int)mapper;
    if (resolved == A1ST_FMPAC && size != 65536) {
        freeRom(rom);
        return fail(error, capacity, "FM-PAC requires a 64 KiB ROM.");
    }
    if (resolved == A1ST_GAMEMASTER2 && size != 131072) {
        freeRom(rom);
        return fail(error, capacity, "GameMaster2 requires a 128 KiB ROM.");
    }
    if (snprintf(statePath, sizeof(statePath), "%s.slot%u.sram", path, slot + 1) >= (int)sizeof(statePath)) {
        freeRom(rom);
        return fail(error, capacity, "Cartridge SRAM path is too long.");
    }
    memset(saved, 0xff, sizeof(saved));
    if (resolved == A1ST_FMPAC || resolved == A1ST_GAMEMASTER2) {
        struct stat info;
        if (!stat(statePath, &info)) {
            if (!a1stReadRom(statePath, sizeof(saved), saved, sizeof(saved), error, capacity)) {
                freeRom(rom); return false;
            }
        }
        else if (errno != ENOENT) {
            freeRom(rom);
            return fail(error, capacity, "Cannot inspect cartridge SRAM; previous slot retained.");
        }
    }
    closeCart(cart);
    cart->rom = rom;
    cart->mapper = resolved;
    cart->slot = slot + 1;
    memcpy(cart->sram, saved, sizeof(saved));
    strcpy(cart->statePath, statePath);
    if (resolved == A1ST_KONAMI_SCC) cart->scc = sccCreate(boardGetMixer());
    slotRegister(cart->slot, 0, 0, 8, readCart, readCart, writeCart, NULL, cart);
    for (page = 0; page < 8; ++page) slotMapPage(cart->slot, 0, page, rom->pages[0], 0, 0);
    if (resolved < 0) {
        unsigned pages = size / 8192;
        const UInt8* header = rom->pages[0];
        bool basic = header[0] == 'A' && header[1] == 'B' && !header[2] && !header[3] && (header[8] || header[9]);
        for (page = 0; page < 8; ++page) {
            unsigned bank;
            if (basic && pages <= 2 && (page < 4 || page > 5)) continue;
            if (pages <= 2) bank = page % pages;
            else if (pages <= 4) {
                bank = (page < 4 ? 0 : 2) + (page & 1);
                if (bank >= pages) bank = pages - 1;
            }
            else bank = page < pages ? page : page - 4;
            slotMapPage(cart->slot, 0, page, rom->pages[bank], 1, 0);
        }
    }
    else if (resolved == A1ST_FMPAC) {
        slotMapPage(cart->slot, 0, 2, rom->pages[0], 0, 0);
        slotMapPage(cart->slot, 0, 3, rom->pages[0], 0, 0);
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
