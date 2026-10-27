#include "MsxCore.h"
#include "MsxSettings.h"
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <algorithm>
#include <string>
#include <vector>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

static std::vector<std::string> uart;
static std::string deniedStatPath, deniedOpenPath;
static bool failRead, failSeek;
static int diagnosticPrintf(const char *format, ...)
{
    char message[2048];
    va_list args;
    va_start(args, format);
    const int count = vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    assert(count >= 0 && static_cast<size_t>(count) < sizeof(message));
    uart.emplace_back(message);
    return count;
}
static int diagnosticStat(const char *path, struct stat *info)
{
    if (path == deniedStatPath) { errno = EACCES; return -1; }
    return stat(path, info);
}
static FILE *diagnosticOpen(const char *path, const char *mode)
{
    if (path == deniedOpenPath) { errno = EACCES; return nullptr; }
    return fopen(path, mode);
}
static int diagnosticSeek(FILE *file, long offset, int origin)
{
    if (failSeek) { errno = EIO; return -1; }
    return fseek(file, offset, origin);
}
static size_t diagnosticRead(void *buffer, size_t size, size_t count, FILE *file)
{
    if (failRead) { errno = EIO; return 0; }
    return fread(buffer, size, count, file);
}
static int diagnosticClose(FILE *file)
{
    const int result = fclose(file);
    errno = EINVAL; // Cleanup must not overwrite the error being reported.
    return result;
}
#define printf diagnosticPrintf
#define stat(path, info) diagnosticStat(path, info)
#define fopen diagnosticOpen
#define fseek diagnosticSeek
#define fread diagnosticRead
#define fclose diagnosticClose
#include "FileValidationUnderTest.inc"
#undef printf
#undef stat
#undef fopen
#undef fseek
#undef fread
#undef fclose

static char selectedCartridges[2][MsxSdPathCapacity], selectedDisks[2][MsxSdPathCapacity];
static char selectedTape[MsxSdPathCapacity], statusMessage[MsxFileErrorCapacity + 32];
struct {
    template<typename... Args> void printf(const char *format, Args... args) {
        diagnosticPrintf(format, args...);
    }
} Serial;
#include "FilePreflightUnderTest.inc"

static void fixture(const char *name, size_t size, bool cartridge = false, bool tape = false)
{
    std::vector<uint8_t> bytes(size);
    if (cartridge) { bytes[0] = 'A'; bytes[1] = 'B'; }
    if (tape) {
        const uint8_t marker[] = {0x1f, 0xa6, 0xde, 0xba, 0xcc, 0x13, 0x7d, 0x74};
        assert(size >= sizeof(marker));
        memcpy(bytes.data(), marker, sizeof(marker));
    }
    FILE *file = fopen(name, "wb");
    assert(file && fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size());
    assert(!fclose(file));
}
static void detail(const char *error, const std::string &path, const char *reason)
{
    assert(strstr(error, path.c_str()) && strstr(error, reason));
    assert(!uart.empty() && uart.back().find(path) != std::string::npos);
}
int main()
{
    char directory[1024];
    assert(getcwd(directory, sizeof(directory)));
    std::string root = directory;
#ifdef _WIN32
    if (root.size() > 2 && root[1] == ':') root.erase(0, 2);
    std::replace(root.begin(), root.end(), '\\', '/');
#endif
    const std::string rom = root + "/valid.rom", missing = root + "/missing.rom";
    const std::string disk = root + "/valid.dsk", tape = root + "/valid.cas";
    fixture("valid.rom", 8192, true);
    fixture("bad-size.rom", 12345);
    fixture("bad-header.rom", 8192);
    fixture("valid.dsk", 368640);
    fixture("bad-size.dsk", 17);
    fixture("valid.cas", 8, false, true);
    fixture("bad-marker.cas", 8);
    fixture("short.cas", 2);
    char error[MsxFileErrorCapacity];
    assert(MsxValidateCartridge(rom.c_str(), error, sizeof(error)) && !*error);
    assert(MsxValidateDisk(disk.c_str(), error, sizeof(error)) && !*error);
    assert(MsxValidateTape(tape.c_str(), error, sizeof(error)) && !*error);
    assert(!MsxValidateCartridge(missing.c_str(), error, sizeof(error)));
    detail(error, missing, "cannot stat cartridge");
    assert(strstr(error, "errno 2") && strstr(error, strerror(ENOENT)));
    errno = EACCES;
    assert(!MsxValidateCartridge(root.c_str(), error, sizeof(error)));
    detail(error, root, "directory");
    assert(!strstr(error, "errno"));
    assert(!MsxValidateCartridge((root + "/bad-size.rom").c_str(), error, sizeof(error)));
    assert(strstr(error, "12345 bytes") && strstr(error, "8192..2097152"));
    assert(!MsxValidateCartridge((root + "/bad-header.rom").c_str(), error, sizeof(error)));
    assert(strstr(error, "AB header not found"));
    assert(!MsxValidateDisk((root + "/bad-size.dsk").c_str(), error, sizeof(error)));
    assert(strstr(error, "17 bytes") && strstr(error, "368640 or 737280"));
    assert(!MsxValidateTape((root + "/bad-marker.cas").c_str(), error, sizeof(error)));
    assert(strstr(error, "invalid CAS marker"));
    assert(!MsxValidateTape((root + "/short.cas").c_str(), error, sizeof(error)));
    assert(strstr(error, "got 2 of 8 bytes") && !strstr(error, "errno"));
    deniedStatPath = rom;
    assert(!MsxValidateCartridge(rom.c_str(), error, sizeof(error)));
    detail(error, rom, "errno 13");
    deniedStatPath.clear();
    deniedOpenPath = disk;
    assert(!MsxValidateDisk(disk.c_str(), error, sizeof(error)));
    detail(error, disk, "not writable");
    assert(strstr(error, strerror(EACCES)));
    deniedOpenPath.clear();
    failSeek = true;
    assert(!MsxValidateCartridge(rom.c_str(), error, sizeof(error)));
    detail(error, rom, "cannot seek");
    assert(strstr(error, "errno 5"));
    failSeek = false;
    failRead = true;
    assert(!MsxValidateCartridge(rom.c_str(), error, sizeof(error)));
    assert(strstr(error, "cannot read") && strstr(error, "errno 5"));
    assert(!MsxValidateTape(tape.c_str(), error, sizeof(error)));
    assert(strstr(error, "got 0 of 8") && strstr(error, "errno 5"));
    failRead = false;
    const std::string longPath = "/sdcard/msx/roms/" + std::string(180, 'x') + "/missing-game.rom";
    char tiny[3] = {'x', 'x', 'z'};
    assert(!MsxValidateCartridge(longPath.c_str(), tiny, 2));
    assert(tiny[0] && !tiny[1] && tiny[2] == 'z');
    assert(uart.back().find(longPath) != std::string::npos);
    assert(!MsxValidateCartridge(longPath.c_str(), nullptr, 0));
    assert(uart.back().find(longPath) != std::string::npos);
    assert(!MsxValidateCartridge(longPath.c_str(), error, sizeof(error)));
    assert(strstr(error, "missing-game.rom") == error);
    assert(strstr(error, longPath.c_str()));
    const std::string prefix = "/sdcard/msx/roms/", suffix = "last-file.rom";
    const std::string maximumPath = prefix +
        std::string(MsxSdPathCapacity + 6 - prefix.size() - suffix.size(), 'y') + suffix;
    assert(!MsxValidateCartridge(maximumPath.c_str(), error, sizeof(error)));
    assert(!strncmp(error, "...", 3) && strstr(error, suffix.c_str()));
    assert(strstr(error, maximumPath.c_str()));
    assert(uart.back().find(maximumPath) != std::string::npos);
    assert(MsxValidateCartridge(nullptr, error, sizeof(error)) && !*error);
    assert(MsxValidateDisk("", error, sizeof(error)) && !*error);
    assert(MsxValidateTape(nullptr, error, sizeof(error)) && !*error);
    for (unsigned slot = 0; slot < 2; ++slot) {
        memset(selectedCartridges, 0, sizeof(selectedCartridges));
        strcpy(selectedCartridges[slot], "/msx/roms/missing.rom");
        assert(!validateCartridges());
        assert(strstr(statusMessage, slot ? "Slot 2:" : "Slot 1:"));
        assert(strstr(statusMessage, "/sdcard/msx/roms/missing.rom"));
        const std::string footer(statusMessage, std::min<size_t>(strlen(statusMessage), 150));
        assert(footer.find("missing.rom") != std::string::npos && footer.find("errno 2") != std::string::npos);
    }
    memset(selectedCartridges, 0, sizeof(selectedCartridges));
    assert(validateCartridges());
    for (unsigned drive = 0; drive < 2; ++drive) {
        memset(selectedDisks, 0, sizeof(selectedDisks));
        strcpy(selectedDisks[drive], "/msx/disks/missing.dsk");
        assert(!validateMedia());
        assert(strstr(statusMessage, drive ? "Drive B:" : "Drive A:"));
        assert(strstr(statusMessage, "/sdcard/msx/disks/missing.dsk"));
    }
    memset(selectedDisks, 0, sizeof(selectedDisks));
    strcpy(selectedTape, "/msx/tapes/missing.cas");
    assert(!validateMedia() && strstr(statusMessage, "Tape:"));
    assert(strstr(statusMessage, "/sdcard/msx/tapes/missing.cas"));
    selectedTape[0] = 0;
    assert(validateMedia());
    puts("PASS: file names/paths, errno, directories, sizes, headers, I/O errors, small buffers and every media port.");
}
