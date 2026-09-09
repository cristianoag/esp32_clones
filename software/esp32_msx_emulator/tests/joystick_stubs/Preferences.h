#pragma once
#include <map>
#include <string>
#include <vector>
#include <string.h>
#include <stdint.h>

extern bool JoystickStorageFailure;
extern std::map<std::string, std::vector<uint8_t>> JoystickStored;
class Preferences
{
public:
    bool begin(const char *, bool) { return true; }
    size_t getBytesLength(const char *key) { return JoystickStored.count(key) ? JoystickStored[key].size() : 0; }
    size_t getBytes(const char *key, void *output, size_t capacity)
    {
        const size_t count = getBytesLength(key);
        if (count > capacity) return 0;
        if (count) memcpy(output, JoystickStored[key].data(), count);
        return count;
    }
    size_t putBytes(const char *key, const void *data, size_t length)
    {
        if (JoystickStorageFailure) return 0;
        const auto *bytes = static_cast<const uint8_t *>(data);
        JoystickStored[key] = std::vector<uint8_t>(bytes, bytes + length);
        return length;
    }
    bool isKey(const char *key) { return JoystickStored.count(key) != 0; }
    bool remove(const char *key)
    {
        if (JoystickStorageFailure) return false;
        return JoystickStored.erase(key) != 0;
    }
};
