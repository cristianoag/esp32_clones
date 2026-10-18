/******************************************************************************
 * Native USB host adapted from CP400UsbKeyboard by The Retro Hacker.
 * CP400 code and modifications copyright (c) 2026 The Retro Hacker
 *
 * Permission is granted for personal, non-commercial use only.
 * Commercial use, distribution, sublicensing, or modification
 * for commercial purposes is strictly prohibited without
 * prior written permission from the author.
 * Please, keep this in the source code.
 * All rights reserved.
 ******************************************************************************/
#include "MacKeyboard.h"
#include "MacInput.h"
#include "MacPlatform.h"

#include <Arduino.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "usb/usb_host.h"

namespace {
constexpr uint8_t ReportSize = 8;
constexpr uint8_t EventCapacity = 128;
portMUX_TYPE stateLock = portMUX_INITIALIZER_UNLOCKED;
uint8_t events[EventCapacity];
uint8_t macReport[ReportSize] = {};
uint8_t eventHead = 0, eventCount = 0;
uint8_t characters[EventCapacity], characterHead = 0, characterCount = 0;
bool emulationInput = false;
MacKeyState keyState;
bool connected = false;
bool started = false, starting = false;

// Only HostTask owns USB state; callbacks execute synchronously in its event pump.
usb_host_client_handle_t client = nullptr;
usb_device_handle_t device = nullptr;
usb_transfer_t *reportTransfer = nullptr, *controlTransfer = nullptr;
uint8_t interfaceNumber = 0, endpointAddress = 0, pendingAddress = 0;
uint16_t maxPacket = ReportSize;
bool claimed = false, stopping = false, cancellationRequested = false;
bool reportPending = false, controlPending = false;
bool malformedReportLogged = false;
uint8_t previousReport[ReportSize] = {};
enum class Stage { None, Protocol, Idle, Ready };
Stage stage = Stage::None;
TickType_t controlStarted = 0;

bool scancodeOverflow = false;
void QueueScan(unsigned code, bool release)
{
    if (characterCount == EventCapacity) { scancodeOverflow = true; return; }
    characters[(characterHead + characterCount) % EventCapacity] = code | (release ? 0x80 : 0);
    ++characterCount;
}

void PublishDisconnected()
{
    portENTER_CRITICAL(&stateLock);
    connected = false;
    memset(macReport, 0, sizeof(macReport));
    keyState.release(QueueScan);
    eventHead = eventCount = 0;
    portEXIT_CRITICAL(&stateLock);
    memset(previousReport, 0, sizeof(previousReport));
}

void Fail(const char *operation, esp_err_t error)
{
    Serial.printf("MACPLUS USB keyboard: %s failed (%s)\n", operation, esp_err_to_name(error));
    stopping = true;
    PublishDisconnected();
}

bool ContainsKey(const uint8_t *report, uint8_t key)
{
    for (unsigned i = 2; i < ReportSize; ++i)
        if (report[i] == key) return true;
    return false;
}

bool IsMenuKey(uint8_t key)
{
    return key == 0x28 || key == 0x29 || key == 0x45 || key == 0x11 || key == 0x1c ||
           key == 0x4b || key == 0x4c || key == 0x4e ||
           (key >= 0x4f && key <= 0x52);
}

void ProcessReport(const uint8_t *report)
{
    for (unsigned i = 2; i < ReportSize; ++i)
        if (report[i] >= 1 && report[i] <= 3) return; // Rollover / POST / undefined.

    uint8_t pressed[6];
    unsigned count = 0;
    for (unsigned i = 2; i < ReportSize; ++i) {
        const uint8_t key = report[i];
        bool duplicate = false;
        for (unsigned j = 2; j < i; ++j) duplicate |= report[j] == key;
        if (!duplicate && IsMenuKey(key) && !ContainsKey(previousReport, key))
            pressed[count++] = key;
    }
    memcpy(previousReport, report, ReportSize);
    bool overflow = false;
    portENTER_CRITICAL(&stateLock);
    if (emulationInput)
        keyState.update(report, QueueScan);
    memcpy(macReport, report, sizeof(macReport));
    for (unsigned i = 0; i < count; ++i) {
        if (eventCount == EventCapacity) {
            eventHead = (eventHead + 1) % EventCapacity;
            --eventCount;
            overflow = true;
        }

        events[(eventHead + eventCount) % EventCapacity] = pressed[i];
        ++eventCount;
    }
    portEXIT_CRITICAL(&stateLock);
    if (overflow) Serial.println("MACPLUS USB keyboard: menu event queue overflow (oldest dropped)");
}

bool FindInterface(const usb_config_desc_t *config, uint8_t &alternate)
{
    const uint8_t *data = reinterpret_cast<const uint8_t *>(config);
    const unsigned total = config->wTotalLength;
    bool candidate = false;
    for (unsigned offset = 0; offset + 2 <= total;) {
        const unsigned length = data[offset], type = data[offset + 1];
        if (length < 2 || offset + length > total) return false;
        if (type == USB_B_DESCRIPTOR_TYPE_INTERFACE) {
            if (length < sizeof(usb_intf_desc_t)) return false;
            const auto *intf = reinterpret_cast<const usb_intf_desc_t *>(data + offset);
            candidate = intf->bInterfaceClass == USB_CLASS_HID &&
                        intf->bInterfaceSubClass == 1 && intf->bInterfaceProtocol == 1;
            interfaceNumber = intf->bInterfaceNumber;
            alternate = intf->bAlternateSetting;
        } else if (type == USB_B_DESCRIPTOR_TYPE_ENDPOINT && candidate) {
            if (length < sizeof(usb_ep_desc_t)) return false;
            const auto *ep = reinterpret_cast<const usb_ep_desc_t *>(data + offset);
            if ((ep->bEndpointAddress & 0x80) && (ep->bEndpointAddress & 0x0f) &&
                (ep->bmAttributes & USB_BM_ATTRIBUTES_XFERTYPE_MASK) == USB_BM_ATTRIBUTES_XFER_INT) {
                maxPacket = USB_EP_DESC_GET_MPS(ep);
                if (maxPacket < ReportSize || maxPacket > 64 || !ep->bInterval) return false;
                endpointAddress = ep->bEndpointAddress;
                return true;
            }
        }
        offset += length;
    }
    return false;
}

void ReportCallback(usb_transfer_t *transfer)
{
    reportPending = false;
    if (stopping) return;
    if (transfer->status != USB_TRANSFER_STATUS_COMPLETED) {
        Serial.printf("MACPLUS USB keyboard: report status %d; disconnect/reconnect to retry\n",
                      static_cast<int>(transfer->status));
        stopping = true;
        PublishDisconnected();
        return;
    }
    // Boot reports are exactly eight bytes; never pad a malformed short report.
    if (transfer->actual_num_bytes == ReportSize) {
        ProcessReport(transfer->data_buffer);
    } else if (!malformedReportLogged) {
        malformedReportLogged = true;
        Serial.printf("MACPLUS USB keyboard: ignoring malformed boot report (%d bytes)\n",
                      transfer->actual_num_bytes);
    }
}

void ControlCallback(usb_transfer_t *transfer)
{
    controlPending = false;
    if (stopping) return;
    if (transfer->status == USB_TRANSFER_STATUS_COMPLETED ||
        (stage == Stage::Idle && transfer->status == USB_TRANSFER_STATUS_STALL)) {
        if (transfer->status == USB_TRANSFER_STATUS_STALL)
            Serial.println("MACPLUS USB keyboard: SET_IDLE unsupported; continuing");
        stage = stage == Stage::Protocol ? Stage::Idle : Stage::Ready;
    } else {
        Serial.printf("MACPLUS USB keyboard: %s status %d; boot keyboard rejected\n",
                      stage == Stage::Protocol ? "SET_PROTOCOL" : "SET_IDLE",
                      static_cast<int>(transfer->status));
        stopping = true;
        PublishDisconnected();
    }
}

void SubmitControl()
{
    auto *setup = reinterpret_cast<usb_setup_packet_t *>(controlTransfer->data_buffer);
    setup->bmRequestType = 0x21;
    setup->bRequest = stage == Stage::Protocol ? 0x0b : 0x0a;
    setup->wValue = 0; // Boot protocol / idle only on changes.
    setup->wIndex = interfaceNumber;
    setup->wLength = 0;
    controlTransfer->device_handle = device;
    controlTransfer->bEndpointAddress = 0;
    controlTransfer->num_bytes = sizeof(usb_setup_packet_t);
    controlTransfer->callback = ControlCallback;
    const esp_err_t error = usb_host_transfer_submit_control(client, controlTransfer);
    if (error != ESP_OK) Fail("control submit", error);
    else {
        controlPending = true;
        controlStarted = xTaskGetTickCount();
    }
}

void ReleaseDevice()
{
    // A GONE notification may precede completion callbacks. Retain all resources
    // until both callbacks have returned; never free a transfer from a callback.
    if (!cancellationRequested) {
        cancellationRequested = true;
        if (claimed && reportPending) {
            esp_err_t error = usb_host_endpoint_halt(device, endpointAddress);
            if (error != ESP_OK) Serial.printf("MACPLUS USB keyboard: halt: %s\n", esp_err_to_name(error));
            error = usb_host_endpoint_flush(device, endpointAddress);
            if (error != ESP_OK) Serial.printf("MACPLUS USB keyboard: flush: %s\n", esp_err_to_name(error));
        }
    }
    // EP0 belongs to the host stack. A pending control transfer is retired by
    // its completion/disconnect callback, never by halting or freeing EP0 here.
    if (reportPending || controlPending) return;
    if (claimed) {
        const esp_err_t error = usb_host_interface_release(client, device, interfaceNumber);
        if (error != ESP_OK) { Fail("interface release", error); return; }
        claimed = false;
    }
    if (reportTransfer) {
        const esp_err_t error = usb_host_transfer_free(reportTransfer);
        if (error != ESP_OK) { Fail("report free", error); return; }
        reportTransfer = nullptr;
    }
    if (controlTransfer) {
        const esp_err_t error = usb_host_transfer_free(controlTransfer);
        if (error != ESP_OK) { Fail("control free", error); return; }
        controlTransfer = nullptr;
    }
    if (device) {
        const esp_err_t error = usb_host_device_close(client, device);
        if (error != ESP_OK) { Fail("device close", error); return; }
        device = nullptr;
    }
    stopping = cancellationRequested = false;
    stage = Stage::None;
}

void OpenDevice(uint8_t address)
{
    esp_err_t error = usb_host_device_open(client, address, &device);
    if (error != ESP_OK) { Fail("device open", error); return; }
    const usb_config_desc_t *config = nullptr;
    error = usb_host_get_active_config_descriptor(device, &config);
    if (error != ESP_OK) { Fail("configuration descriptor", error); return; }
    uint8_t alternate = 0;
    if (!config || !FindInterface(config, alternate)) {
        Fail("find boot HID keyboard interface", ESP_ERR_NOT_SUPPORTED);
        return;
    }
    error = usb_host_interface_claim(client, device, interfaceNumber, alternate);
    if (error != ESP_OK) { Fail("interface claim", error); return; }
    claimed = true;
    error = usb_host_transfer_alloc(sizeof(usb_setup_packet_t), 0, &controlTransfer);
    if (error != ESP_OK) { Fail("control allocation", error); return; }
    error = usb_host_transfer_alloc(maxPacket, 0, &reportTransfer);
    if (error != ESP_OK) { Fail("report allocation", error); return; }
    reportTransfer->device_handle = device;
    reportTransfer->bEndpointAddress = endpointAddress;
    reportTransfer->num_bytes = maxPacket; // IN buffers must be a multiple of MPS.
    reportTransfer->callback = ReportCallback;
    malformedReportLogged = false;
    stage = Stage::Protocol;
    Serial.printf("MACPLUS USB keyboard: boot interface %u, endpoint 0x%02x\n",
                  interfaceNumber, endpointAddress);
}

void ClientEvent(const usb_host_client_event_msg_t *message, void *)
{
    if (message->event == USB_HOST_CLIENT_EVENT_NEW_DEV) {
        if (!device || stopping) pendingAddress = message->new_dev.address;
    } else if (message->event == USB_HOST_CLIENT_EVENT_DEV_GONE &&
               message->dev_gone.dev_hdl == device) {
        Serial.println("MACPLUS USB keyboard: disconnected");
        stopping = true;
        PublishDisconnected();
    }
}

void HostTask(void *)
{
    for (;;) {
        uint32_t flags = 0;
        esp_err_t error = usb_host_lib_handle_events(0, &flags);
        if (error != ESP_OK && error != ESP_ERR_TIMEOUT) Fail("host events", error);
        error = usb_host_client_handle_events(client, 0);
        if (error != ESP_OK && error != ESP_ERR_TIMEOUT) Fail("client events", error);
        if (stopping) ReleaseDevice();
        if (!device && !stopping && pendingAddress) {
            const uint8_t address = pendingAddress;
            pendingAddress = 0;
            OpenDevice(address);
        }
        if (device && !stopping) {
            if (controlPending && xTaskGetTickCount() - controlStarted > pdMS_TO_TICKS(3000)) {
                // IDF 4 does not implement transfer timeout_ms. Fail visibly but
                // retain the outstanding transfer until completion or unplug.
                Fail("control response timeout (unplug keyboard to retry)", ESP_ERR_TIMEOUT);
            } else if (!controlPending && (stage == Stage::Protocol || stage == Stage::Idle)) {
                SubmitControl();
            } else if (stage == Stage::Ready && !reportPending) {
                error = usb_host_transfer_submit(reportTransfer);
                if (error != ESP_OK) Fail("report submit", error);
                else {
                    reportPending = true;
                    portENTER_CRITICAL(&stateLock);
                    connected = true;
                    portEXIT_CRITICAL(&stateLock);
                }
            }
        }
        vTaskDelay(1);
    }
}
} // namespace

void MacKeyboardReport(uint8_t report[8])
{
    portENTER_CRITICAL(&stateLock);
    memcpy(report, macReport, sizeof(macReport));
    portEXIT_CRITICAL(&stateLock);
}

bool MacKeyboardCharacter(uint8_t &character)
{
    portENTER_CRITICAL(&stateLock);
    const bool overflow = scancodeOverflow;
    scancodeOverflow = false;
    const bool available = characterCount != 0;
    if (available)
    {
        character = characters[characterHead];
        characterHead = (characterHead + 1) % EventCapacity;
        --characterCount;
    }
    portEXIT_CRITICAL(&stateLock);
    if (overflow) {
        Serial.println("MACPLUS USB keyboard: scancode queue overflow; reconnect keyboard.");
        macReportError("Keyboard input overflow; reconnect keyboard to release keys.");
    }
    return available;
}

void MacKeyboardEmulation(bool enabled)
{
    portENTER_CRITICAL(&stateLock);
    if (!enabled && emulationInput) keyState.release(QueueScan, true);
    emulationInput = enabled;
    if (enabled) keyState.update(macReport, QueueScan);
    portEXIT_CRITICAL(&stateLock);
}

void MacKeyboardReset()
{
    portENTER_CRITICAL(&stateLock);
    characterHead = characterCount = 0;
    keyState.reset();
    portEXIT_CRITICAL(&stateLock);
}

bool MacKeyboardStart()
{
    portENTER_CRITICAL(&stateLock);
    if (started || starting) {
        const bool result = started;
        portEXIT_CRITICAL(&stateLock);
        return result;
    }
    starting = true;
    portEXIT_CRITICAL(&stateLock);

    usb_host_config_t hostConfig = {};
    hostConfig.intr_flags = ESP_INTR_FLAG_LEVEL1;
    esp_err_t error = usb_host_install(&hostConfig);
    const bool installed = error == ESP_OK;
    if (installed) {
        usb_host_client_config_t clientConfig = {};
        clientConfig.max_num_event_msg = 8;
        clientConfig.async.client_event_callback = ClientEvent;
        error = usb_host_client_register(&clientConfig, &client);
        if (error == ESP_OK &&
            xTaskCreatePinnedToCore(HostTask, "MacUsbKeyboard", 4096, nullptr, 3, nullptr, 0) != pdPASS) {
            const esp_err_t cleanup = usb_host_client_deregister(client);
            if (cleanup != ESP_OK)
                Serial.printf("MACPLUS USB keyboard: client cleanup: %s\n", esp_err_to_name(cleanup));
            client = nullptr;
            error = ESP_ERR_NO_MEM;
        }
    }
    if (error != ESP_OK) {
        Serial.printf("MACPLUS USB keyboard: startup failed (%s)\n", esp_err_to_name(error));
        if (installed) {
            const esp_err_t cleanup = usb_host_uninstall();
            if (cleanup != ESP_OK)
                Serial.printf("MACPLUS USB keyboard: host cleanup: %s\n", esp_err_to_name(cleanup));
        }
    } else {
        Serial.println("MACPLUS USB keyboard: native host on GPIO19 D- / GPIO20 D+");
    }
    portENTER_CRITICAL(&stateLock);
    starting = false;
    started = error == ESP_OK;
    portEXIT_CRITICAL(&stateLock);
    return error == ESP_OK;
}

uint8_t MacKeyboardMenuKey()
{
    portENTER_CRITICAL(&stateLock);
    uint8_t key = 0;
    if (eventCount) {
        key = events[eventHead];
        eventHead = (eventHead + 1) % EventCapacity;
        --eventCount;
    }
    portEXIT_CRITICAL(&stateLock);
    return key;
}

void MacKeyboardClearEvents()
{
    portENTER_CRITICAL(&stateLock);
    eventHead = eventCount = 0;
    portEXIT_CRITICAL(&stateLock);
}

bool MacKeyboardConnected()
{
    portENTER_CRITICAL(&stateLock);
    const bool result = connected;
    portEXIT_CRITICAL(&stateLock);
    return result;
}
