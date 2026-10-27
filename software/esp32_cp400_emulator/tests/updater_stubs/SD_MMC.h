#pragma once
#include "EspOtaTest.h"
#include <algorithm>
#include <string.h>

constexpr const char *FILE_READ = "r";
class File {
    bool open_ = false;
    size_t position_ = 0;
public:
    File() = default;
    explicit File(bool open) : open_(open) {}
    explicit operator bool() const { return open_; }
    bool isDirectory() const { return false; }
    size_t size() const { return UpdateState.package.size(); }
    bool seek(size_t position) {
        if (position > size()) return false;
        position_ = position;
        return true;
    }
    size_t read(uint8_t *buffer, size_t length) {
        if (UpdateState.readFailure) return 0;
        length = std::min(length, size() - position_);
        if (length) memcpy(buffer, UpdateState.package.data() + position_, length);
        position_ += length;
        return length;
    }
    void close() { open_ = false; }
};
struct UpdateTestSd {
    File open(const char *path, const char *) {
        assert(UpdateState.paused);
        UpdateState.path = path;
        return File(!UpdateState.openFailure);
    }
};
extern UpdateTestSd SD_MMC;
