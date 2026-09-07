#include "MsxFlh.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace
{
size_t checks = 0;
#define CHECK(condition) do { ++checks; if (!(condition)) { \
    std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); std::exit(1); } } while (0)

std::vector<uint8_t> Image(size_t length)
{
    std::vector<uint8_t> bytes(length);
    for (size_t i = 0; i < length; ++i) bytes[i] = static_cast<uint8_t>(i * 17);
    if (length >= 24)
    {
        bytes[0] = 0xe9;
        bytes[1] = 1;
        bytes[12] = 9;
        bytes[13] = 0;
    }
    return bytes;
}

std::vector<uint8_t> Pack(const std::vector<uint8_t> &image)
{
    uint32_t checksum = '-' + '~';
    for (uint8_t byte : image) checksum += byte;
    const std::string header = std::to_string(checksum) + "-~";
    std::vector<uint8_t> bytes(header.begin(), header.end());
    bytes.insert(bytes.end(), image.begin(), image.end());
    return bytes;
}

class MemoryReader : public MsxFlh::Reader
{
public:
    explicit MemoryReader(std::vector<uint8_t> bytes) : bytes(bytes) {}
    size_t size() override { return advertisedSize ? advertisedSize : bytes.size(); }
    bool seek(size_t offset) override
    {
        ++seeks;
        if (failSeek || offset > bytes.size()) return false;
        position = offset;
        if (seeks == 2)
        {
            if (changePayload) bytes.back() ^= 0x80;
            if (reorderPayload) std::swap(bytes[bytes.size() - 1], bytes[bytes.size() - 2]);
            if (changeHeader) bytes[0] = bytes[0] == '9' ? '8' : bytes[0] + 1;
        }
        return true;
    }
    size_t read(uint8_t *buffer, size_t length) override
    {
        if (position >= stopAt || (seeks >= 2 && position >= secondStopAt)) return 0;
        length = std::min(length, std::min(fragment, bytes.size() - position));
        length = std::min(length, stopAt - position);
        if (seeks >= 2) length = std::min(length, secondStopAt - position);
        if (length) std::memcpy(buffer, bytes.data() + position, length);
        position += length;
        return length;
    }
    std::vector<uint8_t> bytes;
    size_t position = 0;
    size_t fragment = SIZE_MAX;
    size_t advertisedSize = 0;
    size_t stopAt = SIZE_MAX;
    size_t secondStopAt = SIZE_MAX;
    size_t seeks = 0;
    bool changePayload = false;
    bool reorderPayload = false;
    bool changeHeader = false;
    bool failSeek = false;
};

class MockFlash : public MsxFlh::Flash
{
public:
    bool begin(size_t imageSize) override { ++begins; expectedSize = imageSize; return !failBegin; }
    bool write(const uint8_t *buffer, size_t length) override
    {
        ++writes;
        if (writes == failWrite) return false;
        output.insert(output.end(), buffer, buffer + length);
        return true;
    }
    bool finish() override { ++finishes; return !failFinish; }
    bool commit() override { ++commits; return !failCommit; }
    void abort() override { ++aborts; }
    const char *error() const override { return "Mock flash failure."; }
    size_t expectedSize = 0;
    size_t begins = 0;
    size_t writes = 0;
    size_t finishes = 0;
    size_t commits = 0;
    size_t aborts = 0;
    size_t failWrite = 0;
    bool failBegin = false;
    bool failFinish = false;
    bool failCommit = false;
    std::vector<uint8_t> output;
};

std::vector<std::string> stages;
std::vector<uint8_t> percentages;
void Progress(const char *stage, uint8_t percent)
{
    CHECK(percent <= 100);
    stages.push_back(stage);
    percentages.push_back(percent);
}

bool Install(MemoryReader &reader, MockFlash &flash, char *error)
{
    return MsxFlh::Install(reader, flash, MsxFlh::MaxImageSize, Progress, error, 160);
}

void RejectedBeforeFlash(std::vector<uint8_t> bytes, const char *message)
{
    MemoryReader reader(bytes);
    MockFlash flash;
    char error[160] = {};
    CHECK(!Install(reader, flash, error));
    CHECK(flash.begins == 0);
    CHECK(flash.commits == 0);
    CHECK(std::strstr(error, message) != nullptr);
}

void HeaderTests()
{
    for (const char *text : {"", "123", "123-", "-~", " 123-~", "+123-~",
                             "12x-~", "123-!", "00000000000-~", "4294967296-~"})
    {
        MemoryReader reader(std::vector<uint8_t>(text, text + std::strlen(text)));
        MockFlash flash;
        char error[160] = {};
        CHECK(!Install(reader, flash, error));
        CHECK(flash.begins == 0);
        CHECK(error[0] != '\0');
    }
    auto bytes = Image(64);
    bytes[0] = 0;
    RejectedBeforeFlash(Pack(bytes), "magic");
    bytes = Image(64);
    bytes[12] = 0;
    RejectedBeforeFlash(Pack(bytes), "ESP32-S3");
    bytes[12] = 9;
    bytes[13] = 1;
    RejectedBeforeFlash(Pack(bytes), "ESP32-S3");
    bytes = Image(64);
    bytes[1] = 0;
    RejectedBeforeFlash(Pack(bytes), "segment");
    RejectedBeforeFlash(Pack({}), "truncated");
    RejectedBeforeFlash(Pack(Image(23)), "truncated");
    bytes = Pack(Image(512));
    bytes.back() ^= 1;
    RejectedBeforeFlash(bytes, "checksum mismatch");
    bytes = Pack(Image(512));
    bytes.pop_back();
    RejectedBeforeFlash(bytes, "checksum mismatch");

    MemoryReader overflow(std::vector<uint8_t>{'4','2','9','4','9','6','7','2','9','5','-','~'});
    MsxFlh::Package info;
    char error[160];
    CHECK(!MsxFlh::Validate(overflow, MsxFlh::MaxImageSize, info, nullptr, error, sizeof(error)));
    CHECK(std::strstr(error, "overflow") == nullptr);
}

void BoundaryTests()
{
    for (size_t length : {size_t(24), size_t(4095), size_t(4096), size_t(4097), size_t(8201)})
    {
        const auto image = Image(length);
        for (size_t fragment : {size_t(1), size_t(7), size_t(511), size_t(4096)})
        {
            MemoryReader reader(Pack(image));
            const auto original = reader.bytes;
            reader.fragment = fragment;
            MockFlash flash;
            char error[160] = "old error";
            stages.clear();
            percentages.clear();
            CHECK(Install(reader, flash, error));
            CHECK(error[0] == '\0');
            CHECK(flash.output == image);
            CHECK(flash.expectedSize == length);
            CHECK(flash.begins == 1 && flash.finishes == 1 && flash.commits == 1 && flash.aborts == 0);
            CHECK(reader.bytes == original);
            CHECK(stages.front() == "Validating" && stages.back() == "Complete");
            CHECK(percentages.front() == 0 && percentages.back() == 100);
            for (size_t i = 1; i < stages.size(); ++i)
                if (stages[i] == stages[i - 1]) CHECK(percentages[i] >= percentages[i - 1]);
        }
    }
    char error[160];
    MsxFlh::Package info;
    MemoryReader exact(Pack(Image(MsxFlh::MaxImageSize)));
    CHECK(MsxFlh::Validate(exact, MsxFlh::MaxImageSize, info, nullptr, error, sizeof(error)));
    CHECK(info.imageSize == MsxFlh::MaxImageSize);
    MemoryReader oversized(Pack(Image(MsxFlh::MaxImageSize + 1)));
    CHECK(!MsxFlh::Validate(oversized, MsxFlh::MaxImageSize, info, nullptr, error, sizeof(error)));
    CHECK(std::strstr(error, "size limit") != nullptr);
    MemoryReader limited(Pack(Image(4097)));
    CHECK(!MsxFlh::Validate(limited, 4096, info, nullptr, error, sizeof(error)));
    CHECK(!MsxFlh::Validate(limited, 0, info, nullptr, error, sizeof(error)));
    limited.advertisedSize = SIZE_MAX;
    CHECK(!MsxFlh::Validate(limited, 4096, info, nullptr, error, sizeof(error)));
}

void FailureTests()
{
    const auto valid = Pack(Image(9000));
    char error[160];
    for (size_t stop : {size_t(0), size_t(1), size_t(10), size_t(4100), valid.size() - 1})
    {
        MemoryReader reader(valid);
        reader.stopAt = stop;
        MockFlash flash;
        CHECK(!Install(reader, flash, error));
        CHECK(flash.begins == 0 && flash.commits == 0);
    }
    for (int mode = 0; mode < 10; ++mode)
    {
        MemoryReader reader(valid);
        MockFlash flash;
        if (mode == 0) reader.changeHeader = true;
        if (mode == 1) reader.changePayload = true;
        if (mode == 2) reader.reorderPayload = true;
        if (mode == 3) reader.secondStopAt = 4500;
        if (mode == 4) flash.failWrite = 2;
        if (mode == 5) flash.failBegin = true;
        if (mode == 6) flash.failFinish = true;
        if (mode == 7) flash.failCommit = true;
        if (mode == 8) reader.failSeek = true;
        if (mode == 9) reader.advertisedSize = valid.size() - 1;
        CHECK(!Install(reader, flash, error));
        CHECK(error[0] != '\0');
        if (mode == 0 || mode == 8 || mode == 9)
            CHECK(flash.begins == 0 && flash.aborts == 0);
        else
            CHECK(flash.begins == 1 && flash.aborts == 1);
        CHECK(flash.commits == (mode == 7 ? 1u : 0u));
        if (mode >= 1 && mode <= 5) CHECK(flash.finishes == 0);
        if (mode == 2) CHECK(std::strstr(error, "payload changed") != nullptr);
    }
    MemoryReader invalid(std::vector<uint8_t>{});
    MockFlash flash;
    char tiny[2] = {'x', 'x'};
    CHECK(!MsxFlh::Install(invalid, flash, 4096, nullptr, tiny, sizeof(tiny)));
    CHECK(tiny[1] == '\0');
    CHECK(!MsxFlh::Install(invalid, flash, 4096, nullptr, nullptr, 0));
}

void PathTests()
{
    CHECK(MsxFlh::IsMsxPath("/ESP32_MSX-1.00.FLH"));
    CHECK(MsxFlh::IsMsxPath("/updates/esp32_msx-1.00.flh"));
    for (const char *path : {"/ESP32_CP400-1.13.FLH", "/ESP32_MSX-.FLH",
                             "/ESP32_MSX-1.00.bin", "/ESP32_MSX-1.00.FLH.bak",
                             "ESP32_MSX-1.00.FLH", "/../ESP32_MSX-1.FLH",
                             "/a//ESP32_MSX-1.FLH", "/./ESP32_MSX-1.FLH",
                             "/a\\ESP32_MSX-1.FLH", "/C:/ESP32_MSX-1.FLH", "/"})
        CHECK(!MsxFlh::IsMsxPath(path));
    CHECK(!MsxFlh::IsMsxPath(nullptr));
    CHECK(!MsxFlh::IsMsxPath(("/" + std::string(512, 'a') + "/ESP32_MSX-1.FLH").c_str()));
}

class FileReader : public MsxFlh::Reader
{
public:
    explicit FileReader(std::FILE *file) : file_(file) {}
    size_t size() override
    {
        const long position = std::ftell(file_);
        if (position < 0 || std::fseek(file_, 0, SEEK_END)) return 0;
        const long length = std::ftell(file_);
        if (std::fseek(file_, position, SEEK_SET) || length < 0) return 0;
        return static_cast<size_t>(length);
    }
    bool seek(size_t offset) override { return !std::fseek(file_, static_cast<long>(offset), SEEK_SET); }
    size_t read(uint8_t *buffer, size_t length) override { return std::fread(buffer, 1, length, file_); }
private:
    std::FILE *file_;
};
}

int main(int argc, char **argv)
{
    HeaderTests();
    BoundaryTests();
    FailureTests();
    PathTests();
    for (int i = 1; i < argc; ++i)
    {
        std::FILE *file = std::fopen(argv[i], "rb");
        CHECK(file != nullptr);
        FileReader reader(file);
        MsxFlh::Package package;
        char error[160] = {};
        const bool ok = MsxFlh::Validate(reader, MsxFlh::MaxImageSize, package, nullptr, error, sizeof(error));
        std::fclose(file);
        if (!ok) std::fprintf(stderr, "%s: %s\n", argv[i], error);
        CHECK(ok);
        std::printf("Validated real FLH: %s (%zu image bytes)\n", argv[i], package.imageSize);
    }
    std::printf("Firmware update tests passed (%zu checks).\n", checks);
    return 0;
}
