#include "SmsCore.h"
#include <sms_internal.h>
#include <algorithm>
#include <new>
#include <stdio.h>
#include <string.h>

namespace
{
bool fail(char *error, size_t capacity, const char *message)
{
    snprintf(error, capacity, "%s", message);
    return false;
}

uint32_t colour(void *, uint8_t red, uint8_t green, uint8_t blue)
{
    return SmsColour(red, green, blue);
}
}

uint8_t SmsColour(uint8_t red, uint8_t green, uint8_t blue)
{
    return ((red & 3) << 1) | ((red & 3) >> 1) |
           ((green & 3) << 4) | ((green & 3) >> 1 << 3) | ((blue & 3) << 6);
}

void SmsVideoRow(uint8_t destination[320], const uint8_t source[256])
{
    for (unsigned x = 0; x < 320; ++x) destination[x] = source[x * 4 / 5];
}

bool SmsInspectRom(const uint8_t *data, size_t size, size_t &offset, size_t &length,
                   char *error, size_t capacity)
{
    offset = (size % 8192 == 512) ? 512 : 0;
    length = size - offset;
    if (!data || length < 8192 || length > SmsCore::MaxRomSize ||
        (length != 8192 && length % 16384))
        return fail(error, capacity, "Use an 8 KiB or 16 KiB-aligned SMS ROM, at most 1 MiB.");
    const size_t headers[] = {0x7ff0, 0x3ff0, 0x1ff0};
    for (size_t header : headers)
    {
        if (header + 16 > length || memcmp(data + offset + header, "TMR SEGA", 8)) continue;
        const unsigned region = data[offset + header + 15] >> 4;
        if (region != 3 && region != 4)
            return fail(error, capacity, "This cartridge is not a Master System ROM (Game Gear is unsupported).");
        break;
    }
    return true;
}

SmsCore::~SmsCore()
{
    if (core_) SMS_quit(core_.get());
}

bool SmsCore::load(const uint8_t *data, size_t size, const SmsSettings &settings,
                   uint8_t *pixels, char *error, size_t capacity)
{
    size_t offset, length;
    if (!SmsValidSettings(settings) || !pixels)
        return fail(error, capacity, "Invalid cartridge configuration or framebuffer.");
    if (!SmsInspectRom(data, size, offset, length, error, capacity)) return false;
    std::unique_ptr<uint8_t[]> rom(new (std::nothrow) uint8_t[length]);
    std::unique_ptr<SMS_Core> candidate(new (std::nothrow) SMS_Core{});
    if (!rom || !candidate) return fail(error, capacity, "Not enough memory to stage cartridge.");
    memcpy(rom.get(), data + offset, length);
    if (!SMS_init(candidate.get()))
    {
        SMS_quit(candidate.get());
        return fail(error, capacity, "SMS core initialization failed.");
    }
    SMS_set_colour_callback(candidate.get(), colour);
    const uint32_t tmsRgb[] = {
        0x000000, 0x000000, 0x21c842, 0x5edc78, 0x5455ed, 0x7d76fc, 0xd4524d, 0x42ebf5,
        0xfc5554, 0xff7978, 0xd4c154, 0xe6ce80, 0x21b03b, 0xc95bba, 0xcccccc, 0xffffff
    };
    uint32_t palette[16];
    for (unsigned i = 0; i < 16; ++i)
        palette[i] = SmsColour((tmsRgb[i] >> 22) & 3, (tmsRgb[i] >> 14) & 3, (tmsRgb[i] >> 6) & 3);
    SMS_set_builtin_palette(candidate.get(), palette);
    SMS_set_pixels(candidate.get(), pixels, Width, 8);
    const bool loaded = SMS_loadromEx(candidate.get(), rom.get(), length, SMS_System_SMS,
                                      settings.pal ? SMS_Region_PAL : SMS_Region_NTSC,
                                      settings.japanese ? SMS_Console_JAPANESE : SMS_Console_EXPORT);
    if (!loaded || !SMS_is_system_type_sms(candidate.get()) ||
        !SMS_set_apu_callback(candidate.get(), samples, stereo_, 1024, AudioRate))
    {
        SMS_quit(candidate.get());
        return fail(error, capacity, "Cannot load SMS cartridge or initialize PSG audio.");
    }
    if (settings.mapper != SmsMapper::Auto)
    {
        const SMS_MapperType mappers[] = {MAPPER_TYPE_SEGA, MAPPER_TYPE_SEGA,
                                         MAPPER_TYPE_CODEMASTERS, MAPPER_TYPE_KOREAN};
        candidate->cart.mapper_type = mappers[unsigned(settings.mapper)];
        mapper_init(candidate.get());
    }
    if (core_) SMS_quit(core_.get());
    core_ = std::move(candidate);
    rom_ = std::move(rom);
    SMS_set_userdata(core_.get(), this);
    memset(pixels, 0, Width * Height);
    frames = count_ = previousButtons_ = 0;
    return true;
}

void SmsCore::reset()
{
    if (!core_) return;
    SMS_reset(core_.get());
    mapper_init(core_.get());
    previousButtons_ = count_ = 0;
    frames = 0;
}

void SmsCore::samples(void *user, int16_t *data, uint32_t count)
{
    auto &self = *static_cast<SmsCore *>(user);
    for (uint32_t i = 0; i + 1 < count && self.count_ < 512; i += 2)
        self.mono_[self.count_++] = (int(data[i]) + data[i + 1]) / 2;
}

void SmsCore::runFrame(uint16_t buttons, bool draw)
{
    if (!core_) return;
    SMS_set_buttons(core_.get(), previousButtons_ & ~buttons, false);
    SMS_set_buttons(core_.get(), buttons, true);
    previousButtons_ = buttons;
    SMS_skip_frame(core_.get(), !draw);
    count_ = 0;
    SMS_run(core_.get(), SMS_RunEndFrame);
    ++frames;
}

unsigned SmsCore::height() const
{
    if (!core_) return 192;
    int x, y, width, height;
    SMS_get_pixel_region(core_.get(), &x, &y, &width, &height);
    return std::min(Height, unsigned(height));
}

unsigned SmsCore::frameMicros() const
{
    return core_ ? unsigned(1000000.0 / SMS_target_fps(core_.get())) : 16667;
}

const char *SmsCore::mapperName() const
{
    if (!core_) return "No cartridge";
    switch (core_->cart.mapper_type)
    {
    case MAPPER_TYPE_SEGA: return "Sega";
    case MAPPER_TYPE_CODEMASTERS: return "Codemasters";
    case MAPPER_TYPE_KOREAN: return "Korean";
    case MAPPER_TYPE_NONE: return "Unbanked";
    default: return "Unsupported";
    }
}
