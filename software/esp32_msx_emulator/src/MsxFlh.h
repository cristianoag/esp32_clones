#pragma once

#include <stddef.h>
#include <stdint.h>

namespace MsxFlh
{
constexpr size_t MaxImageSize = 0x400000;
constexpr size_t ChunkSize = 4096;
using Progress = void (*)(const char *stage, uint8_t percent);

class Reader
{
public:
    virtual ~Reader() {}
    virtual size_t size() = 0;
    virtual bool seek(size_t offset) = 0;
    // Return 0 on EOF/error. Positive short reads are supported.
    virtual size_t read(uint8_t *buffer, size_t length) = 0;
};

class Flash
{
public:
    virtual ~Flash() {}
    virtual bool begin(size_t imageSize) = 0;
    // Success means every byte was written.
    virtual bool write(const uint8_t *buffer, size_t length) = 0;
    virtual bool finish() = 0;
    virtual bool commit() = 0;
    virtual void abort() = 0;
    virtual const char *error() const = 0;
};

struct Package
{
    size_t fileSize;
    size_t payloadOffset;
    size_t imageSize;
    uint32_t checksum;
    uint64_t fingerprint;
};

bool IsMsxPath(const char *path);
bool Validate(Reader &reader, size_t imageLimit, Package &package,
              Progress progress, char *error, size_t errorSize);
bool Install(Reader &reader, Flash &flash, size_t imageLimit,
             Progress progress, char *error, size_t errorSize);
}
