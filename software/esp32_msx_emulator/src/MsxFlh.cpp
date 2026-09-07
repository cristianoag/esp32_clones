#include "MsxFlh.h"
#include <stdio.h>
#include <string.h>

namespace MsxFlh
{
namespace
{
constexpr size_t ImageHeaderSize = 24;
constexpr uint16_t Esp32S3ChipId = 9;
constexpr uint64_t FingerprintSeed = UINT64_C(14695981039346656037);

bool Fail(char *error, size_t capacity, const char *message)
{
    if (error && capacity) snprintf(error, capacity, "%s", message);
    return false;
}

void Clear(char *error, size_t capacity)
{
    if (error && capacity) error[0] = '\0';
}

bool Exact(Reader &reader, uint8_t *buffer, size_t length)
{
    size_t done = 0;
    while (done < length)
    {
        const size_t count = reader.read(buffer + done, length - done);
        if (!count || count > length - done) return false;
        done += count;
    }
    return true;
}

void Report(Progress progress, const char *stage, size_t done, size_t total,
            uint8_t &lastPercent)
{
    const uint8_t percent = static_cast<uint8_t>(static_cast<uint64_t>(done) * 100 / total);
    if (percent != lastPercent && progress) progress(stage, percent);
    lastPercent = percent;
}

bool Header(Reader &reader, size_t imageLimit, Package &package,
            char *error, size_t capacity)
{
    package = {};
    package.fileSize = reader.size();
    if (!imageLimit) return Fail(error, capacity, "OTA partition has no capacity.");
    if (imageLimit > MaxImageSize) imageLimit = MaxImageSize;
    if (package.fileSize > imageLimit + 12)
        return Fail(error, capacity, "FLH package exceeds OTA size limit.");
    if (!reader.seek(0)) return Fail(error, capacity, "Cannot rewind FLH file.");
    uint8_t byte = 0;
    size_t digits = 0;
    for (;;)
    {
        if (!Exact(reader, &byte, 1))
            return Fail(error, capacity, "Truncated FLH checksum header.");
        ++package.payloadOffset;
        if (byte == '-') break;
        if (byte < '0' || byte > '9' || ++digits > 10)
            return Fail(error, capacity, "Invalid FLH decimal checksum.");
        const uint32_t digit = byte - '0';
        if (package.checksum > (UINT32_MAX - digit) / 10)
            return Fail(error, capacity, "FLH checksum overflow.");
        package.checksum = package.checksum * 10 + digit;
    }
    if (!digits || !Exact(reader, &byte, 1) || byte != '~')
        return Fail(error, capacity, "Invalid FLH -~ separator.");
    ++package.payloadOffset;
    if (package.fileSize < package.payloadOffset ||
        package.fileSize - package.payloadOffset < ImageHeaderSize)
        return Fail(error, capacity, "Empty or truncated ESP image.");
    package.imageSize = package.fileSize - package.payloadOffset;
    if (package.imageSize > imageLimit)
        return Fail(error, capacity, "ESP image exceeds OTA size limit.");
    return true;
}

bool ImageHeader(const uint8_t *buffer, char *error, size_t capacity)
{
    if (buffer[0] != 0xe9)
        return Fail(error, capacity, "Invalid ESP image magic.");
    const uint16_t chip = buffer[12] | (static_cast<uint16_t>(buffer[13]) << 8);
    if (chip != Esp32S3ChipId)
        return Fail(error, capacity, "Firmware is not an ESP32-S3 image.");
    if (buffer[1] == 0 || buffer[1] > 16)
        return Fail(error, capacity, "Invalid ESP image segment count.");
    return true;
}

bool Stream(Reader &reader, const Package &package, Flash *flash,
            uint64_t &fingerprint, Progress progress, char *error, size_t capacity)
{
    uint8_t buffer[ChunkSize];
    uint32_t checksum = '-' + '~';
    fingerprint = FingerprintSeed;
    uint8_t lastPercent = 255;
    const char *stage = flash ? "Writing" : "Validating";
    Report(progress, stage, 0, package.imageSize, lastPercent);
    for (size_t done = 0; done < package.imageSize;)
    {
        const size_t left = package.imageSize - done;
        const size_t count = left < sizeof(buffer) ? left : sizeof(buffer);
        if (!Exact(reader, buffer, count))
            return Fail(error, capacity, "Short read or SD error in FLH payload.");
        if (!done && !ImageHeader(buffer, error, capacity)) return false;
        for (size_t i = 0; i < count; ++i)
        {
            checksum += buffer[i];
            // Non-cryptographic second-pass fingerprint also detects byte reordering.
            fingerprint = (fingerprint ^ buffer[i]) * UINT64_C(1099511628211);
        }
        if (flash && !flash->write(buffer, count))
            return Fail(error, capacity, flash->error());
        done += count;
        Report(progress, stage, done, package.imageSize, lastPercent);
    }
    uint8_t extra;
    if (reader.size() != package.fileSize || reader.read(&extra, 1) != 0)
        return Fail(error, capacity, "FLH file size changed while reading.");
    if (checksum != package.checksum)
        return Fail(error, capacity, "FLH checksum mismatch.");
    return true;
}

char Lower(char c)
{
    return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
}

bool Equal(const char *a, const char *b, size_t count)
{
    for (size_t i = 0; i < count; ++i)
        if (Lower(a[i]) != Lower(b[i])) return false;
    return true;
}
}

bool IsMsxPath(const char *path)
{
    if (!path || path[0] != '/') return false;
    size_t length = 0;
    while (length < 512 && path[length]) ++length;
    if (length == 512) return false;
    const char *component = path + 1;
    for (const char *p = component;; ++p)
    {
        const unsigned char c = *p;
        if (c && (c < 32 || c == 127 || c == '\\' || c == ':')) return false;
        if (!c || c == '/')
        {
            const size_t count = p - component;
            if (!count || (count == 1 && component[0] == '.') ||
                (count == 2 && component[0] == '.' && component[1] == '.'))
                return false;
            if (!c)
                return count > 14 && Equal(component, "ESP32_MSX-", 10) &&
                       Equal(p - 4, ".FLH", 4);
            component = p + 1;
        }
    }
}

bool Validate(Reader &reader, size_t imageLimit, Package &package,
              Progress progress, char *error, size_t errorSize)
{
    Clear(error, errorSize);
    if (!Header(reader, imageLimit, package, error, errorSize)) return false;
    return Stream(reader, package, nullptr, package.fingerprint, progress, error, errorSize);
}

bool Install(Reader &reader, Flash &flash, size_t imageLimit,
             Progress progress, char *error, size_t errorSize)
{
    Package validated, current;
    if (!Validate(reader, imageLimit, validated, progress, error, errorSize)) return false;
    if (!Header(reader, imageLimit, current, error, errorSize)) return false;
    if (current.fileSize != validated.fileSize ||
        current.payloadOffset != validated.payloadOffset ||
        current.checksum != validated.checksum)
        return Fail(error, errorSize, "FLH header changed after validation.");
    if (!flash.begin(validated.imageSize))
    {
        Fail(error, errorSize, flash.error());
        flash.abort();
        return false;
    }
    uint64_t fingerprint = 0;
    bool ok = Stream(reader, current, &flash, fingerprint, progress, error, errorSize);
    if (ok && fingerprint != validated.fingerprint)
        ok = Fail(error, errorSize, "FLH payload changed after validation.");
    if (ok)
    {
        if (progress) progress("Finalizing", 0);
        if (!flash.finish() || !flash.commit())
            ok = Fail(error, errorSize, flash.error());
    }
    if (!ok)
    {
        flash.abort();
        return false;
    }
    Clear(error, errorSize);
    if (progress) progress("Complete", 100);
    return true;
}
}
