#pragma once
#include <stddef.h>
#include <stdint.h>

class MacFloppy
{
public:
    MacFloppy() = default;
    MacFloppy(const MacFloppy &) = delete;
    MacFloppy &operator=(const MacFloppy &) = delete;
    ~MacFloppy() { close(); }
    bool open(const char *path, char *error, size_t capacity);
    void close();
    bool hasImage() const { return image != nullptr; }
    bool inserted() const;

private:
    uint8_t *image = nullptr;
};
