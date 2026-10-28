#pragma once
#include "SmsSettings.h"
#include <sms.h>
#include <memory>
#include <stdlib.h>

class SmsCore
{
public:
    static constexpr unsigned Width = 256, Height = 240, AudioRate = 22050;
    static constexpr size_t MaxRomSize = 1024 * 1024;
    ~SmsCore();
    bool load(const uint8_t *data, size_t size, const SmsSettings &settings,
              uint8_t *pixels, char *error, size_t capacity);
    void reset();
    void runFrame(uint16_t buttons, bool draw);
    const int16_t *audio() const { return mono_; }
    unsigned audioCount() const { return count_; }
    unsigned height() const;
    unsigned frameMicros() const;
    const char *mapperName() const;
    uint32_t frames = 0;

private:
    static void samples(void *user, int16_t *data, uint32_t count);
    std::unique_ptr<SMS_Core, decltype(&free)> core_{nullptr, free};
    std::unique_ptr<uint8_t[]> rom_;
    int16_t stereo_[1024] = {}, mono_[512] = {};
    unsigned count_ = 0;
    uint16_t previousButtons_ = 0;
};

bool SmsInspectRom(const uint8_t *data, size_t size, size_t &offset, size_t &length,
                   char *error, size_t capacity);
uint8_t SmsColour(uint8_t red, uint8_t green, uint8_t blue);
void SmsVideoRow(uint8_t destination[320], const uint8_t source[256]);
