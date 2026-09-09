#include "MsxJoystickHost.h"

#include <Arduino.h>
#include <MsxSoftUsb.h>
#include <esp_intr_alloc.h>
#include <freertos/semphr.h>

namespace {
constexpr timer_group_t kGroup = TIMER_GROUP_1;
constexpr timer_idx_t kTimer = TIMER_0;
constexpr unsigned kPorts = 2;
TaskHandle_t hostTask;
SemaphoreHandle_t hostMutex;
SemaphoreHandle_t startupDone;
SemaphoreHandle_t pauseGate;
SemaphoreHandle_t pauseDone;
portMUX_TYPE signalMux = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE busMux = portMUX_INITIALIZER_UNLOCKED;
TaskHandle_t pauseOwner;
bool started;
bool startupOk;
bool timerInitialized;
bool callbackInstalled;
bool timerEnabled;
bool pauseRequested;
bool pauseOk;
void (*deviceCallback)(unsigned, uint16_t, uint16_t, bool);
void (*reportCallback)(unsigned, uint8_t, const uint8_t *, size_t);

struct Port {
  uint16_t vid;
  uint16_t pid;
  bool connected;
  bool devicePending;
  uint8_t endpoint;
  uint8_t length;
  uint8_t data[8];
};
Port ports[kPorts];

bool IRAM_ATTR tick(void *) {
  BaseType_t wake = pdFALSE;
  if (hostTask) vTaskNotifyGiveFromISR(hostTask, &wake);
  return wake == pdTRUE;
}

void detected(uint8_t port, void *descriptor) {
  if (port >= kPorts) return;
  const sDevDesc *device = static_cast<const sDevDesc *>(descriptor);
  Port &state = ports[port];
  state.vid = device->idVendor;
  state.pid = device->idProduct;
  state.connected = true;
  state.devicePending = true;
  state.length = 0;
}

void disconnected(uint8_t port) {
  if (port >= kPorts) return;
  Port &state = ports[port];
  state.length = 0;
  if (state.connected) {
    state.connected = false;
    state.devicePending = true;
  }
}

void received(uint8_t port, uint8_t endpoint, uint8_t length, const uint8_t *data) {
  if (port >= kPorts || !length || length > 8 || !ports[port].connected) return;
  Port &state = ports[port];
  state.endpoint = endpoint;
  state.length = length;
  memcpy(state.data, data, length);
}

void dispatch() {
  for (unsigned port = 0; port < kPorts; ++port) {
    Port &state = ports[port];
    if (state.devicePending) {
      state.devicePending = false;
      deviceCallback(port, state.vid, state.pid, state.connected);
    }
    if (state.length && state.connected) {
      const uint8_t length = state.length;
      state.length = 0;
      reportCallback(port, state.endpoint, state.data, length);
    }
  }
}

bool initializeTimer() {
  timer_config_t config = {};
  config.alarm_en = TIMER_ALARM_EN;
  config.counter_en = TIMER_PAUSE;
  config.intr_type = TIMER_INTR_LEVEL;
  config.counter_dir = TIMER_COUNT_UP;
  config.auto_reload = TIMER_AUTORELOAD_EN;
  config.divider = 80;
  esp_err_t error = timer_init(kGroup, kTimer, &config);
  if (error == ESP_OK) {
    timerInitialized = true;
    error = timer_set_counter_value(kGroup, kTimer, 0);
  }
  if (error == ESP_OK) error = timer_set_alarm_value(kGroup, kTimer, 1000);
  if (error == ESP_OK) error = timer_enable_intr(kGroup, kTimer);
  if (error == ESP_OK) {
    error = timer_isr_callback_add(kGroup, kTimer, tick, nullptr, ESP_INTR_FLAG_IRAM);
    callbackInstalled = error == ESP_OK;
  }
  if (error == ESP_OK) error = timer_start(kGroup, kTimer);
  if (error == ESP_OK) {
    timerEnabled = true;
    return true;
  }
  Serial.printf("MSX joystick USB: timer initialization failed (%d)\n", error);
  if (callbackInstalled) timer_isr_callback_remove(kGroup, kTimer);
  if (timerInitialized) timer_deinit(kGroup, kTimer);
  callbackInstalled = timerInitialized = false;
  return false;
}

void worker(void *) {
  // Allocation and interrupt registration occur on core 0, not the audio/core-1
  // setup task. Only wire transactions mask interrupts; consumer work does not.
  set_ondetect_cb(detected);
  set_ondisconnect_cb(disconnected);
  set_usb_raw_cb(received);
  set_onled_blink_cb(nullptr);
  initStates(16, 15, 18, 17, -1, -1, -1, -1);
  startupOk = msx_usb_timing_valid() && initializeTimer();
  xSemaphoreGive(startupDone);
  if (!startupOk) {
    vTaskSuspend(nullptr);
    return;
  }
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    xSemaphoreTake(hostMutex, portMAX_DELAY);
    portENTER_CRITICAL(&signalMux);
    const bool pausing = pauseRequested;
    pauseRequested = false;
    portEXIT_CRITICAL(&signalMux);
    if (pausing) {
      pauseOk = timer_pause(kGroup, kTimer) == ESP_OK;
      if (pauseOk) {
        timerEnabled = false;
        portENTER_CRITICAL(&busMux);
        msx_usb_reset();
        portEXIT_CRITICAL(&busMux);
        for (unsigned i = 0; i < NUM_USB; ++i) printState();
        for (unsigned port = 0; port < kPorts; ++port) disconnected(port);
        dispatch();
      }
      // The caller also takes hostMutex after this acknowledgement, so it
      // cannot disable flash cache until this iteration has fully finished.
      xSemaphoreGive(pauseDone);
      xSemaphoreGive(hostMutex);
      continue;
    }
    if (timerEnabled) {
      // Coalesce missed ticks instead of issuing back-to-back USB frames.
      ulTaskNotifyTake(pdTRUE, 0);
      portENTER_CRITICAL(&busMux);
      usb_process();
      portEXIT_CRITICAL(&busMux);
      for (unsigned i = 0; i < NUM_USB; ++i) printState();
      dispatch();
    } else {
      for (unsigned port = 0; port < kPorts; ++port) disconnected(port);
      dispatch();
    }
    xSemaphoreGive(hostMutex);
  }
}
}  // namespace

bool MsxJoystickHostStart(
    void (*device)(unsigned, uint16_t, uint16_t, bool),
    void (*report)(unsigned, uint8_t, const uint8_t *, size_t)) {
  if (started) return true;
  if (!device || !report) {
    Serial.println("MSX joystick USB: missing callbacks");
    return false;
  }
  hostMutex = xSemaphoreCreateMutex();
  startupDone = xSemaphoreCreateBinary();
  pauseGate = xSemaphoreCreateMutex();
  pauseDone = xSemaphoreCreateBinary();
  if (!hostMutex || !startupDone || !pauseGate || !pauseDone) {
    Serial.println("MSX joystick USB: semaphore allocation failed");
    if (hostMutex) vSemaphoreDelete(hostMutex);
    if (startupDone) vSemaphoreDelete(startupDone);
    if (pauseGate) vSemaphoreDelete(pauseGate);
    if (pauseDone) vSemaphoreDelete(pauseDone);
    hostMutex = startupDone = nullptr;
    pauseGate = pauseDone = nullptr;
    return false;
  }
  deviceCallback = device;
  reportCallback = report;
  // Keep callbacks behind startup publication, even if core 0 runs first.
  xSemaphoreTake(hostMutex, portMAX_DELAY);
  if (xTaskCreatePinnedToCore(worker, "MSX joystick USB", 6144, nullptr, 5,
                            &hostTask, 0) != pdPASS) {
    Serial.println("MSX joystick USB: task allocation failed");
    xSemaphoreGive(hostMutex);
    vSemaphoreDelete(hostMutex);
    vSemaphoreDelete(startupDone);
    vSemaphoreDelete(pauseGate);
    vSemaphoreDelete(pauseDone);
    hostMutex = startupDone = nullptr;
    pauseGate = pauseDone = nullptr;
    hostTask = nullptr;
    return false;
  }
  xSemaphoreTake(startupDone, portMAX_DELAY);
  vSemaphoreDelete(startupDone);
  startupDone = nullptr;
  if (!startupOk) {
    vTaskDelete(hostTask);
    hostTask = nullptr;
    xSemaphoreGive(hostMutex);
    vSemaphoreDelete(hostMutex);
    vSemaphoreDelete(pauseGate);
    vSemaphoreDelete(pauseDone);
    hostMutex = nullptr;
    pauseGate = pauseDone = nullptr;
    return false;
  }
  started = true;
  xSemaphoreGive(hostMutex);
  return true;
}

bool MsxJoystickHostPause() {
  if (!started) return true;
  TaskHandle_t caller = xTaskGetCurrentTaskHandle();
  if (caller == hostTask) return false;
  portENTER_CRITICAL(&signalMux);
  const bool nested = pauseOwner == caller;
  portEXIT_CRITICAL(&signalMux);
  if (nested) return false;
  if (xSemaphoreTake(pauseGate, portMAX_DELAY) != pdTRUE) return false;
  portENTER_CRITICAL(&signalMux);
  pauseRequested = true;
  portEXIT_CRITICAL(&signalMux);
  xTaskNotifyGive(hostTask);
  xSemaphoreTake(pauseDone, portMAX_DELAY);
  xSemaphoreTake(hostMutex, portMAX_DELAY);
  if (!pauseOk) {
    xSemaphoreGive(hostMutex);
    xSemaphoreGive(pauseGate);
    return false;
  }
  portENTER_CRITICAL(&signalMux);
  pauseOwner = caller;
  portEXIT_CRITICAL(&signalMux);
  return true;
}

void MsxJoystickHostResume() {
  if (!started) return;
  TaskHandle_t caller = xTaskGetCurrentTaskHandle();
  portENTER_CRITICAL(&signalMux);
  const bool owner = pauseOwner == caller;
  portEXIT_CRITICAL(&signalMux);
  if (!owner) {
    Serial.println("MSX joystick USB: resume must be called by the task that paused the host");
    return;
  }
  timer_set_counter_value(kGroup, kTimer, 0);
  timerEnabled = timer_start(kGroup, kTimer) == ESP_OK;
  if (!timerEnabled) {
    Serial.println("MSX joystick USB: timer resume failed");
    xTaskNotifyGive(hostTask);
  }
  portENTER_CRITICAL(&signalMux);
  pauseOwner = nullptr;
  portEXIT_CRITICAL(&signalMux);
  xSemaphoreGive(hostMutex);
  xSemaphoreGive(pauseGate);
}
