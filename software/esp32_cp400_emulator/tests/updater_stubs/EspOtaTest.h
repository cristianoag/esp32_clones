#pragma once
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <vector>

using esp_err_t = int;
using esp_partition_type_t = int;
using esp_partition_subtype_t = int;
using esp_ota_handle_t = unsigned;
constexpr int ESP_OK = 0, ESP_FAIL = -1, ESP_ERR_INVALID_STATE = -2;
constexpr int ESP_PARTITION_TYPE_APP = 0, ESP_PARTITION_TYPE_DATA = 1;
constexpr int ESP_PARTITION_TYPE_ANY = 255, ESP_PARTITION_SUBTYPE_ANY = 255;
constexpr int ESP_PARTITION_SUBTYPE_DATA_NVS = 2, ESP_PARTITION_SUBTYPE_DATA_OTA = 0;
constexpr int ESP_PARTITION_SUBTYPE_APP_OTA_0 = 16, ESP_PARTITION_SUBTYPE_APP_OTA_1 = 17;
constexpr int TIMER_GROUP_0 = 0, TIMER_0 = 0;

struct esp_partition_t {
    const char *label;
    int type, subtype;
    uint32_t address, size;
    bool encrypted;
};
using esp_partition_iterator_t = const esp_partition_t *;
enum class UpdateFailure { None, Pause, Begin, Write, End, Commit, Resume };
struct UpdateTestState {
    bool paused = false, active = false, committed = false;
    bool legacyLayout = false, pendingBoot = false, openFailure = false, readFailure = false;
    unsigned pauses = 0, resumes = 0, partitionCalls = 0, begins = 0, aborts = 0, commits = 0;
    UpdateFailure failure = UpdateFailure::None;
    std::string path;
    std::vector<uint8_t> package, output;
};
extern UpdateTestState UpdateState;
extern const esp_partition_t UpdatePartitions[4];

const char *esp_err_to_name(esp_err_t error);
esp_partition_iterator_t esp_partition_find(int, int, const char *);
esp_partition_iterator_t esp_partition_next(esp_partition_iterator_t);
const esp_partition_t *esp_partition_get(esp_partition_iterator_t);
const esp_partition_t *esp_partition_find_first(int, int, const char *);
const esp_partition_t *esp_ota_get_running_partition();
const esp_partition_t *esp_ota_get_boot_partition();
const esp_partition_t *esp_ota_get_next_update_partition(const esp_partition_t *);
esp_err_t esp_ota_begin(const esp_partition_t *, size_t, esp_ota_handle_t *);
esp_err_t esp_ota_write(esp_ota_handle_t, const void *, size_t);
esp_err_t esp_ota_end(esp_ota_handle_t);
esp_err_t esp_ota_set_boot_partition(const esp_partition_t *);
esp_err_t esp_ota_abort(esp_ota_handle_t);
esp_err_t timer_disable_intr(int, int);
esp_err_t timer_enable_intr(int, int);
