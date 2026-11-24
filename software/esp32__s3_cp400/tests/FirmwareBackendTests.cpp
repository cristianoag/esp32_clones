#include "FirmwareUpdater.h"
#include "Arduino.h"
#include "SD_MMC.h"
#include <string.h>

UpdateTestState UpdateState;
UpdateTestSerial Serial;
UpdateTestSd SD_MMC;
const esp_partition_t UpdatePartitions[4] = {
    {"nvs", 1, 2, 0x9000, 0x5000, false},
    {"otadata", 1, 0, 0xe000, 0x2000, false},
    {"app0", 0, 16, 0x10000, 0x400000, false},
    {"app1", 0, 17, 0x410000, 0x400000, false}
};

static void cacheOperation()
{
    // esp_ota_get_boot_partition() maps otadata and disables cache on hardware.
    assert(UpdateState.paused && "Cache-disabled OTA metadata accessed with CPU timer running.");
    ++UpdateState.partitionCalls;
}
const char *esp_err_to_name(esp_err_t error) { return error == ESP_OK ? "ESP_OK" : "ESP_FAIL"; }
esp_partition_iterator_t esp_partition_find(int, int, const char *) {
    cacheOperation();
    return UpdatePartitions;
}
esp_partition_iterator_t esp_partition_next(esp_partition_iterator_t it) {
    cacheOperation();
    return it + 1 == UpdatePartitions + (UpdateState.legacyLayout ? 3 : 4) ? nullptr : it + 1;
}
const esp_partition_t *esp_partition_get(esp_partition_iterator_t it) { cacheOperation(); return it; }
const esp_partition_t *esp_partition_find_first(int type, int subtype, const char *label) {
    cacheOperation();
    for (const auto &partition : UpdatePartitions)
        if (partition.type == type && partition.subtype == subtype && !strcmp(partition.label, label))
            return &partition;
    return nullptr;
}
const esp_partition_t *esp_ota_get_running_partition() { cacheOperation(); return UpdatePartitions + 2; }
const esp_partition_t *esp_ota_get_boot_partition() {
    cacheOperation();
    return UpdatePartitions + (UpdateState.pendingBoot || UpdateState.committed ? 3 : 2);
}
const esp_partition_t *esp_ota_get_next_update_partition(const esp_partition_t *) {
    cacheOperation();
    return UpdatePartitions + 3;
}
esp_err_t esp_ota_begin(const esp_partition_t *target, size_t, esp_ota_handle_t *handle) {
    cacheOperation();
    assert(target == UpdatePartitions + 3);
    ++UpdateState.begins;
    if (UpdateState.failure == UpdateFailure::Begin) return ESP_FAIL;
    *handle = 1;
    UpdateState.active = true;
    return ESP_OK;
}
esp_err_t esp_ota_write(esp_ota_handle_t, const void *buffer, size_t length) {
    cacheOperation();
    assert(UpdateState.active);
    if (UpdateState.failure == UpdateFailure::Write) return ESP_FAIL;
    const auto *bytes = static_cast<const uint8_t *>(buffer);
    UpdateState.output.insert(UpdateState.output.end(), bytes, bytes + length);
    return ESP_OK;
}
esp_err_t esp_ota_end(esp_ota_handle_t) {
    cacheOperation();
    UpdateState.active = false;
    return UpdateState.failure == UpdateFailure::End ? ESP_FAIL : ESP_OK;
}
esp_err_t esp_ota_set_boot_partition(const esp_partition_t *target) {
    cacheOperation();
    ++UpdateState.commits;
    if (UpdateState.failure == UpdateFailure::Commit && UpdateState.commits == 1) return ESP_FAIL;
    UpdateState.committed = target == UpdatePartitions + 3;
    return ESP_OK;
}
esp_err_t esp_ota_abort(esp_ota_handle_t) {
    cacheOperation();
    assert(UpdateState.active);
    UpdateState.active = false;
    ++UpdateState.aborts;
    return ESP_OK;
}
esp_err_t timer_disable_intr(int group, int timer) {
    assert(group == 0 && timer == 0 && !UpdateState.paused);
    ++UpdateState.pauses;
    if (UpdateState.failure == UpdateFailure::Pause) return ESP_FAIL;
    UpdateState.paused = true;
    return ESP_OK;
}
esp_err_t timer_enable_intr(int group, int timer) {
    assert(group == 0 && timer == 0 && UpdateState.paused && !UpdateState.active);
    ++UpdateState.resumes;
    if (UpdateState.failure == UpdateFailure::Resume) return ESP_FAIL;
    UpdateState.paused = false;
    return ESP_OK;
}
static void reset()
{
    UpdateState = {};
    std::vector<uint8_t> image(64);
    image[0] = 0xe9; image[1] = 1; image[12] = 9;
    unsigned checksum = '-' + '~';
    for (uint8_t byte : image) checksum += byte;
    const std::string header = std::to_string(checksum) + "-~";
    UpdateState.package.assign(header.begin(), header.end());
    UpdateState.package.insert(UpdateState.package.end(), image.begin(), image.end());
}
static void progress(const char *, uint8_t) { assert(UpdateState.paused); }
static bool install(char *error) {
    return Cp400InstallFirmware("/updates/ESP32_MSX.FLH", progress, error, 160);
}
int main()
{
    char error[160];
    reset();
    assert(install(error));
    assert(!*error && UpdateState.committed && UpdateState.output.size() == 64);
    assert(UpdateState.pauses == 1 && UpdateState.resumes == 1 && !UpdateState.paused);
    assert(UpdateState.path == "/updates/ESP32_MSX.FLH");
    for (UpdateFailure failure : {UpdateFailure::Begin, UpdateFailure::Write, UpdateFailure::End,
                                 UpdateFailure::Commit}) {
        reset();
        UpdateState.failure = failure;
        assert(!install(error) && *error);
        assert(!UpdateState.paused && !UpdateState.active && !UpdateState.committed);
        assert(UpdateState.resumes == 1);
        if (failure == UpdateFailure::Write) assert(UpdateState.aborts == 1);
        if (failure == UpdateFailure::Commit) assert(UpdateState.commits == 2);
    }
    for (unsigned mode = 0; mode < 5; ++mode) {
        reset();
        if (mode == 0) UpdateState.legacyLayout = true;
        if (mode == 1) UpdateState.pendingBoot = true;
        if (mode == 2) UpdateState.openFailure = true;
        if (mode == 3) UpdateState.readFailure = true;
        if (mode == 4) UpdateState.package.back() ^= 1;
        assert(!install(error) && *error && !UpdateState.begins && !UpdateState.committed);
        assert(!UpdateState.paused && UpdateState.resumes == 1);
        if (mode == 0) assert(strstr(error, "UART"));
        if (mode == 1) assert(strstr(error, "already selected"));
    }
    reset();
    UpdateState.failure = UpdateFailure::Pause;
    assert(!install(error) && strstr(error, "Cannot pause"));
    assert(!UpdateState.partitionCalls && !UpdateState.resumes);
    reset();
    UpdateState.failure = UpdateFailure::Resume;
    assert(!install(error) && strstr(error, "Reboot board"));
    assert(UpdateState.committed && UpdateState.resumes == 1);
    reset();
    assert(!Cp400InstallFirmware("/bad.bin", progress, error, sizeof(error)));
    assert(!UpdateState.pauses && !UpdateState.partitionCalls);
    puts("PASS: actual OTA backend pauses before metadata, keeps cleanup cache-safe and restores every failure path.");
}
