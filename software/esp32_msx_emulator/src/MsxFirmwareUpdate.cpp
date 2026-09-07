#include "MsxFirmwareUpdate.h"
#include "MsxFlh.h"
#include <Arduino.h>
#include <SD_MMC.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <stdio.h>
#include <string.h>

namespace
{
bool Fail(char *error, size_t capacity, const char *message)
{
    if (error && capacity) snprintf(error, capacity, "%s", message);
    Serial.printf("[Firmware update] %s\n", message);
    return false;
}

const esp_partition_t *CheckLayout()
{
    struct Expected
    {
        const char *label;
        esp_partition_type_t type;
        esp_partition_subtype_t subtype;
        uint32_t address;
        uint32_t size;
    };
    const Expected expected[] = {
        {"nvs", ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, 0x9000, 0x5000},
        {"otadata", ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, 0xe000, 0x2000},
        {"app0", ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, 0x10000, 0x400000},
        {"app1", ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, 0x410000, 0x400000}
    };
    size_t count = 0;
    for (esp_partition_iterator_t it = esp_partition_find(
             ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, nullptr);
         it; it = esp_partition_next(it))
        ++count;
    if (count != sizeof(expected) / sizeof(expected[0])) return nullptr;
    for (const auto &entry : expected)
    {
        const esp_partition_t *partition = esp_partition_find_first(
            entry.type, entry.subtype, entry.label);
        if (!partition || partition->address != entry.address ||
            partition->size != entry.size || partition->encrypted)
            return nullptr;
    }
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *boot = esp_ota_get_boot_partition();
    if (!running || !boot || boot->address != running->address ||
        running->type != ESP_PARTITION_TYPE_APP ||
        (running->subtype != ESP_PARTITION_SUBTYPE_APP_OTA_0 &&
         running->subtype != ESP_PARTITION_SUBTYPE_APP_OTA_1))
        return nullptr;
    const esp_partition_t *target = esp_ota_get_next_update_partition(running);
    if (!target || target->address == running->address ||
        target->size != MsxFlh::MaxImageSize ||
        (target->address != 0x10000 && target->address != 0x410000))
        return nullptr;
    return target;
}

class SdReader : public MsxFlh::Reader
{
public:
    explicit SdReader(File &file) : file_(file) {}
    size_t size() override { return file_.size(); }
    bool seek(size_t offset) override { return file_.seek(offset); }
    size_t read(uint8_t *buffer, size_t length) override
    {
        const size_t count = file_.read(buffer, length);
        if (length >= 128) delay(1);
        return count <= length ? count : 0;
    }
private:
    File &file_;
};

class OtaFlash : public MsxFlh::Flash
{
public:
    explicit OtaFlash(const esp_partition_t *target)
        : target_(target), previousBoot_(esp_ota_get_boot_partition()) {}
    ~OtaFlash() override { abort(); }
    bool begin(size_t imageSize) override
    {
        const esp_err_t result = esp_ota_begin(target_, imageSize, &handle_);
        active_ = result == ESP_OK;
        return Check(result, "OTA begin");
    }
    bool write(const uint8_t *buffer, size_t length) override
    {
        const bool ok = Check(esp_ota_write(handle_, buffer, length), "OTA write");
        delay(1);
        return ok;
    }
    bool finish() override
    {
        const esp_err_t result = esp_ota_end(handle_);
        // esp_ota_end releases its handle even when image verification fails.
        active_ = false;
        return Check(result, "OTA image verification");
    }
    bool commit() override
    {
        if (Check(esp_ota_set_boot_partition(target_), "OTA boot selection")) return true;
        // A flash error may have partially updated otadata. Try to restore the
        // original boot choice, but never hide the initial commit failure.
        const esp_err_t restored = previousBoot_
            ? esp_ota_set_boot_partition(previousBoot_) : ESP_ERR_INVALID_STATE;
        if (restored != ESP_OK)
        {
            snprintf(error_, sizeof(error_),
                     "OTA boot selection failed; restoring old boot also failed: %s",
                     esp_err_to_name(restored));
            Serial.printf("[Firmware update] %s\n", error_);
        }
        return false;
    }
    void abort() override
    {
        if (active_)
        {
            const esp_err_t result = esp_ota_abort(handle_);
            active_ = false;
            if (result != ESP_OK)
                Serial.printf("[Firmware update] OTA abort: %s\n", esp_err_to_name(result));
        }
    }
    const char *error() const override { return error_; }
private:
    bool Check(esp_err_t result, const char *stage)
    {
        if (result == ESP_OK) return true;
        snprintf(error_, sizeof(error_), "%s failed: %s", stage, esp_err_to_name(result));
        Serial.printf("[Firmware update] %s\n", error_);
        return false;
    }
    const esp_partition_t *target_;
    const esp_partition_t *previousBoot_;
    esp_ota_handle_t handle_ = 0;
    bool active_ = false;
    char error_[128] = {};
};
}

bool MsxInstallFirmware(const char *sdPath, void (*progress)(const char *, uint8_t),
                        char *error, size_t errorSize)
{
    if (error && errorSize) error[0] = '\0';
    if (!MsxFlh::IsMsxPath(sdPath))
        return Fail(error, errorSize, "Select an ESP32_MSX-*.FLH file on the SD card.");
    const esp_partition_t *target = CheckLayout();
    if (!target)
        return Fail(error, errorSize, "MSX OTA layout mismatch or pending boot update.");
    File file = SD_MMC.open(sdPath, FILE_READ);
    if (!file || file.isDirectory())
    {
        file.close();
        return Fail(error, errorSize, "Cannot open firmware file on SD card.");
    }
    Serial.printf("[Firmware update] Validating %s for %s\n", sdPath, target->label);
    SdReader reader(file);
    OtaFlash flash(target);
    char detail[160] = {};
    const bool ok = MsxFlh::Install(reader, flash, target->size, progress, detail, sizeof(detail));
    file.close();
    if (!ok) return Fail(error, errorSize, detail);
    Serial.printf("[Firmware update] %s selected for next boot; source FLH preserved.\n",
                  target->label);
    return true;
}
