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
#include "MsxKeyboard.h"

#include <Arduino.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "usb/usb_host.h"

namespace {
constexpr uint8_t ReportSize = 8;
constexpr uint8_t EventCapacity = 32;
portMUX_TYPE stateLock = portMUX_INITIALIZER_UNLOCKED;
uint8_t matrixState[16] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};
uint8_t events[EventCapacity];
uint8_t eventHead = 0, eventCount = 0;
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

void PublishDisconnected()
{
    portENTER_CRITICAL(&stateLock);
    connected = false;
    memset(matrixState, 0xff, sizeof(matrixState));
    eventHead = eventCount = 0;
    portEXIT_CRITICAL(&stateLock);
    memset(previousReport, 0, sizeof(previousReport));
}

void Fail(const char *operation, esp_err_t error)
{
    Serial.printf("MSX USB keyboard: %s failed (%s)\n", operation, esp_err_to_name(error));
    stopping = true;
    PublishDisconnected();
}

void Press(uint8_t *matrix, unsigned row, unsigned bit)
{
    matrix[row] &= static_cast<uint8_t>(~(1u << bit));
}

void MapKey(uint8_t *matrix, uint8_t key)
{
    // International MSX physical positions, not a ROM-dependent character map.
    if (key >= 0x04 && key <= 0x1d) {
        const unsigned position = 2 * 8 + 6 + key - 0x04;
        Press(matrix, position / 8, position % 8);
    } else if (key >= 0x1e && key <= 0x26) {
        const unsigned digit = key - 0x1e + 1;
        Press(matrix, digit / 8, digit % 8);
    } else if (key >= 0x3a && key <= 0x43) {
        unsigned function = key - 0x3a;
        if (function >= 5) {
            Press(matrix, 6, 0); // F6-F10 are Shift+F1-F5 on an MSX.
            function -= 5;
        }
        const unsigned position = 6 * 8 + 5 + function;
        Press(matrix, position / 8, position % 8);
    } else {
        switch (key) {
        case 0x27: Press(matrix, 0, 0); break; // 0
        case 0x28: case 0x58: Press(matrix, 7, 7); break; // Enter / keypad Enter
        case 0x29: Press(matrix, 7, 2); break;
        case 0x2a: Press(matrix, 7, 5); break;
        case 0x2b: Press(matrix, 7, 3); break;
        case 0x2c: Press(matrix, 8, 0); break;
        case 0x2d: Press(matrix, 1, 2); break; // -
        case 0x2e: Press(matrix, 1, 3); break; // =
        case 0x2f: Press(matrix, 1, 5); break; // [
        case 0x30: Press(matrix, 1, 6); break; // ]
        case 0x31: case 0x32: Press(matrix, 1, 4); break; // backslash / non-US #
        case 0x33: Press(matrix, 1, 7); break; // ;
        case 0x34: Press(matrix, 2, 0); break; // '
        case 0x35: Press(matrix, 2, 1); break; // `
        case 0x36: Press(matrix, 2, 2); break; // ,
        case 0x37: Press(matrix, 2, 3); break; // .
        case 0x38: Press(matrix, 2, 4); break; // /
        case 0x39: Press(matrix, 6, 3); break; // Caps
        case 0x44: Press(matrix, 7, 6); break; // F11 -> Select
        // F12 (0x45) is exclusively a host menu key.
        case 0x48: Press(matrix, 7, 4); break; // Pause -> Stop
        case 0x49: Press(matrix, 8, 2); break;
        case 0x4a: Press(matrix, 8, 1); break; // Home/Clear
        case 0x4c: Press(matrix, 8, 3); break;
        case 0x4f: Press(matrix, 8, 7); break;
        case 0x50: Press(matrix, 8, 4); break;
        case 0x51: Press(matrix, 8, 6); break;
        case 0x52: Press(matrix, 8, 5); break;
        case 0x54: Press(matrix, 9, 2); break; // keypad /
        case 0x55: Press(matrix, 9, 0); break;
        case 0x56: Press(matrix, 10, 5); break;
        case 0x57: Press(matrix, 9, 1); break;
        case 0x59: case 0x5a: case 0x5b: case 0x5c:
            Press(matrix, 9, 4 + key - 0x59); break;
        case 0x5d: case 0x5e: case 0x5f: case 0x60: case 0x61:
            Press(matrix, 10, key - 0x5d); break;
        case 0x62: Press(matrix, 9, 3); break;
        case 0x63: Press(matrix, 10, 7); break;
        case 0x64: Press(matrix, 2, 5); break; // non-US extra key / dead key
        case 0x85: Press(matrix, 10, 6); break; // keypad comma
        default: break;
        }
    }
}

bool ContainsKey(const uint8_t *report, uint8_t key)
{
    for (unsigned i = 2; i < ReportSize; ++i)
        if (report[i] == key) return true;
    return false;
}

bool IsMenuKey(uint8_t key)
{
    return key == 0x28 || key == 0x29 || key == 0x45 ||
           (key >= 0x4f && key <= 0x52);
}

void ProcessReport(const uint8_t *report)
{
    for (unsigned i = 2; i < ReportSize; ++i)
        if (report[i] >= 1 && report[i] <= 3) return; // Rollover / POST / undefined.

    uint8_t matrix[16], pressed[6];
    unsigned count = 0;
    memset(matrix, 0xff, sizeof(matrix));
    if (report[0] & 0x22) Press(matrix, 6, 0); // Either Shift
    if (report[0] & 0x11) Press(matrix, 6, 1); // Either Ctrl
    if (report[0] & 0x04) Press(matrix, 6, 2); // Left Alt -> Graph
    if (report[0] & 0x40) Press(matrix, 6, 4); // Right Alt -> Code
    for (unsigned i = 2; i < ReportSize; ++i) {
        const uint8_t key = report[i];
        MapKey(matrix, key);
        bool duplicate = false;
        for (unsigned j = 2; j < i; ++j) duplicate |= report[j] == key;
        if (!duplicate && IsMenuKey(key) && !ContainsKey(previousReport, key))
            pressed[count++] = key;
    }
    memcpy(previousReport, report, ReportSize);
    bool overflow = false;
    portENTER_CRITICAL(&stateLock);
    memcpy(matrixState, matrix, sizeof(matrix));
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
    if (overflow) Serial.println("MSX USB keyboard: menu event queue overflow (oldest dropped)");
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
        Serial.printf("MSX USB keyboard: report status %d; disconnect/reconnect to retry\n",
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
        Serial.printf("MSX USB keyboard: ignoring malformed boot report (%d bytes)\n",
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
            Serial.println("MSX USB keyboard: SET_IDLE unsupported; continuing");
        stage = stage == Stage::Protocol ? Stage::Idle : Stage::Ready;
    } else {
        Serial.printf("MSX USB keyboard: %s status %d; boot keyboard rejected\n",
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
            if (error != ESP_OK) Serial.printf("MSX USB keyboard: halt: %s\n", esp_err_to_name(error));
            error = usb_host_endpoint_flush(device, endpointAddress);
            if (error != ESP_OK) Serial.printf("MSX USB keyboard: flush: %s\n", esp_err_to_name(error));
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
    Serial.printf("MSX USB keyboard: boot interface %u, endpoint 0x%02x\n",
                  interfaceNumber, endpointAddress);
}

void ClientEvent(const usb_host_client_event_msg_t *message, void *)
{
    if (message->event == USB_HOST_CLIENT_EVENT_NEW_DEV) {
        if (!device || stopping) pendingAddress = message->new_dev.address;
    } else if (message->event == USB_HOST_CLIENT_EVENT_DEV_GONE &&
               message->dev_gone.dev_hdl == device) {
        Serial.println("MSX USB keyboard: disconnected");
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

bool MsxKeyboardStart()
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
            xTaskCreatePinnedToCore(HostTask, "MsxUsbKeyboard", 4096, nullptr, 3, nullptr, 0) != pdPASS) {
            const esp_err_t cleanup = usb_host_client_deregister(client);
            if (cleanup != ESP_OK)
                Serial.printf("MSX USB keyboard: client cleanup: %s\n", esp_err_to_name(cleanup));
            client = nullptr;
            error = ESP_ERR_NO_MEM;
        }
    }
    if (error != ESP_OK) {
        Serial.printf("MSX USB keyboard: startup failed (%s)\n", esp_err_to_name(error));
        if (installed) {
            const esp_err_t cleanup = usb_host_uninstall();
            if (cleanup != ESP_OK)
                Serial.printf("MSX USB keyboard: host cleanup: %s\n", esp_err_to_name(cleanup));
        }
    } else {
        Serial.println("MSX USB keyboard: native host on GPIO19 D- / GPIO20 D+");
    }
    portENTER_CRITICAL(&stateLock);
    starting = false;
    started = error == ESP_OK;
    portEXIT_CRITICAL(&stateLock);
    return error == ESP_OK;
}

void MsxKeyboardMatrix(uint8_t matrix[16])
{
    if (!matrix) {
        Serial.println("MSX USB keyboard: null matrix destination");
        return;
    }
    portENTER_CRITICAL(&stateLock);
    memcpy(matrix, matrixState, sizeof(matrixState));
    portEXIT_CRITICAL(&stateLock);
}

uint8_t MsxKeyboardMenuKey()
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

void MsxKeyboardClearEvents()
{
    portENTER_CRITICAL(&stateLock);
    eventHead = eventCount = 0;
    portEXIT_CRITICAL(&stateLock);
}

bool MsxKeyboardConnected()
{
    portENTER_CRITICAL(&stateLock);
    const bool result = connected;
    portEXIT_CRITICAL(&stateLock);
    return result;
}
