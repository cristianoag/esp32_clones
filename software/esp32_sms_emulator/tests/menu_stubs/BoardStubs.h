#pragma once
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

extern std::vector<std::string> MenuText;
extern uint32_t MenuTime;
extern bool MenuWriteFailure, MenuReadFailure;
extern bool MenuSdOpenFailure;
extern unsigned MenuSdOpens, MenuEntryOpens, MenuDirectoryReads;
extern int MenuAllocationBudget, MenuDirectoryReadBudget;
extern size_t MenuBrowserBytes, MenuBrowserMaxBytes;
extern size_t MenuInternalFreeBytes;
extern bool MenuFailInternalCore, MenuFailExternalCore;
extern unsigned MenuInternalCoreAllocations, MenuExternalCoreAllocations;
struct MenuNode { bool directory; std::vector<uint8_t> data; };
extern std::map<std::string, MenuNode> MenuFiles;

struct TestSerial
{
    void begin(unsigned) {}
    void println(const char *) {}
    template<typename... Args> void printf(const char *, Args...) {}
};
extern TestSerial Serial;
inline uint32_t millis() { return MenuTime; }
inline uint32_t micros() { return MenuTime * 1000; }
inline void delay(unsigned value) { MenuTime += value; }
inline void delayMicroseconds(unsigned) { ++MenuTime; }
constexpr unsigned INPUT = 0, MALLOC_CAP_SPIRAM = 1, MALLOC_CAP_INTERNAL = 2,
                   MALLOC_CAP_8BIT = 4, SDMMC_FREQ_DEFAULT = 20000;
inline void pinMode(unsigned, unsigned) {}
inline void neopixelWrite(unsigned, unsigned, unsigned, unsigned) {}
inline bool psramFound() { return true; }
inline size_t heap_caps_get_free_size(unsigned) { return MenuInternalFreeBytes; }
inline void *heap_caps_calloc(size_t count, size_t size, unsigned caps)
{
    if (caps & MALLOC_CAP_INTERNAL)
    {
        ++MenuInternalCoreAllocations;
        if (MenuFailInternalCore) return nullptr;
    }
    else if (caps & MALLOC_CAP_SPIRAM)
    {
        ++MenuExternalCoreAllocations;
        if (MenuFailExternalCore) return nullptr;
    }
    return calloc(count, size);
}
inline void *heap_caps_realloc(void *memory, size_t size, unsigned)
{
    if (!MenuAllocationBudget) return nullptr;
    if (MenuAllocationBudget > 0) --MenuAllocationBudget;
    void *result = realloc(memory, size);
    if (result)
    {
        MenuBrowserBytes = size;
        MenuBrowserMaxBytes = std::max(MenuBrowserMaxBytes, size);
    }
    return result;
}
inline void heap_caps_free(void *memory)
{
    if (memory) MenuBrowserBytes = 0;
    free(memory);
}
struct TestEsp { void restart() {} };
extern TestEsp ESP;

class File
{
public:
    File() = default;
    explicit File(const std::string &path) : path_(path), opened_(MenuFiles.count(path) != 0) {}
    explicit operator bool() const { return opened_; }
    bool isDirectory() const { return opened_ && MenuFiles.at(path_).directory; }
    const char *name() const { return path_.c_str(); }
    size_t size() const { return opened_ ? MenuFiles.at(path_).data.size() : 0; }
    size_t read(uint8_t *data, size_t count)
    {
        const auto &bytes = MenuFiles.at(path_).data;
        count = std::min(count, bytes.size() - position_);
        if (MenuReadFailure && count) --count;
        memcpy(data, bytes.data() + position_, count);
        position_ += count;
        return count;
    }
    File openNextFile()
    {
        const std::string path = nextPath();
        if (path.empty()) return File();
        ++MenuEntryOpens;
        return File(path);
    }
    std::string getNextFileName(bool *directory)
    {
        const std::string path = nextPath();
        if (!path.empty()) *directory = MenuFiles.at(path).directory;
        return path;
    }
    void close() { opened_ = false; }
private:
    std::string nextPath()
    {
        ++MenuDirectoryReads;
        if (!MenuDirectoryReadBudget) { errno = EIO; return {}; }
        if (MenuDirectoryReadBudget > 0) --MenuDirectoryReadBudget;
        const std::string prefix = path_ == "/" ? "/" : path_ + "/";
        size_t index = 0;
        for (const auto &node : MenuFiles)
        {
            if (node.first.compare(0, prefix.size(), prefix) ||
                node.first.size() == prefix.size() ||
                node.first.find('/', prefix.size()) != std::string::npos) continue;
            if (index++ == child_) { ++child_; return node.first; }
        }
        return {};
    }
    std::string path_;
    bool opened_ = false;
    size_t position_ = 0, child_ = 0;
};

struct TestSd
{
    void setPins(unsigned, unsigned, unsigned) {}
    bool begin(const char *, bool, bool, unsigned, unsigned) { return true; }
    bool exists(const char *path) { return MenuFiles.count(path) != 0; }
    File open(const char *path) { ++MenuSdOpens; return MenuSdOpenFailure ? File() : File(path); }
};
extern TestSd SD_MMC;

class Preferences
{
public:
    bool begin(const char *, bool) { return true; }
    size_t getBytesLength(const char *) { return stored_.size(); }
    size_t getBytes(const char *, void *data, size_t size)
    {
        size = std::min(size, stored_.size());
        memcpy(data, stored_.data(), size);
        return size;
    }
    size_t putBytes(const char *, const void *data, size_t size)
    {
        if (MenuWriteFailure) return 0;
        const auto *bytes = static_cast<const uint8_t *>(data);
        stored_.assign(bytes, bytes + size);
        return size;
    }
private:
    std::vector<uint8_t> stored_;
};

struct PinConfig { template<typename... Args> explicit PinConfig(Args...) {} };
struct Mode { template<typename... Args> explicit Mode(Args...) {} };
struct TestDma
{
    uint8_t lines[240][320] = {};
    unsigned flushes = 0;
    uint8_t *getLineAddr8(unsigned y) { return lines[y]; }
    void flush(unsigned, unsigned) { ++flushes; }
};
struct VGA
{
    unsigned bufferCount = 1;
    TestDma buffer;
    TestDma *dmaBuffer = &buffer;
    bool init(const PinConfig &, const Mode &, unsigned) { return true; }
    bool start() { return true; }
    void clear(unsigned) {}
    void dot(int, int, unsigned) {}
    void show() {}
};

class Adafruit_GFX
{
public:
    Adafruit_GFX(int, int) {}
    virtual ~Adafruit_GFX() = default;
    virtual void drawPixel(int16_t, int16_t, uint16_t) = 0;
    void setCursor(int, int) {}
    void setTextColor(unsigned) {}
    void print(const char *message) { MenuText.emplace_back(message); }
    void setTextWrap(bool) {}
    void setTextSize(unsigned) {}
    void drawFastHLine(int, int, int, unsigned) {}
    void fillRect(int, int, int, int, unsigned) {}
    void drawRect(int, int, int, int, unsigned) {}
};
