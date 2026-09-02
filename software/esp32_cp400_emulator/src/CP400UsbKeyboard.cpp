/******************************************************************************
 * Project      : esp32_cp400_emulator
 * File         : CP400UsbKeyboard.cpp
 * Last Updated : 2026-08-29
 *
 * Description  : Native USB-OTG host for the CP400 keyboard port
 *
 * CP400 code and modifications copyright (c) 2026 The Retro Hacker
 *
 * Permission is granted for personal, non-commercial use only.
 * Commercial use, distribution, sublicensing, or modification
 * for commercial purposes is strictly prohibited without
 * prior written permission from the author.
 * Please, keep this in the source code.
 * All rights reserved.
 ******************************************************************************/

#include "CP400UsbKeyboard.h"
#include "CP400Input.h"

#include <Arduino.h>
#include <string.h>

#include "usb/usb_host.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

//Core 1 runs the video task, so the host lives on core 0 next to the soft host.
//It stays below the soft host timer task priority of 5 because that task also
//clocks the emulated 6809.
#define USB_KBD_CORE            0
#define USB_KBD_DAEMON_PRIORITY 3
#define USB_KBD_CLIENT_PRIORITY 3
#define USB_KBD_STACK_SIZE      4096

#define HID_BOOT_REPORT_SIZE    8
#define HID_SUBCLASS_BOOT       0x01
#define HID_PROTOCOL_KEYBOARD   0x01
#define HID_REQUEST_SET_IDLE    0x0A
#define HID_REQUEST_SET_PROTOCOL 0x0B
//Host to device, class request, interface recipient.
#define HID_REQUEST_TYPE_OUT    0x21
#define HID_PROTOCOL_BOOT       0x00

#define CONTROL_BUFFER_SIZE     64
//Boot keyboards use 8 byte packets; the cap only guards against a bogus
//descriptor asking for an unreasonable buffer.
#define MAX_REPORT_PACKET_SIZE  64

enum ControlStage
{
    CONTROL_STAGE_NONE,
    CONTROL_STAGE_SET_PROTOCOL,
    CONTROL_STAGE_SET_IDLE,
    CONTROL_STAGE_READY
};

static usb_host_client_handle_t sClient      = NULL;
static usb_device_handle_t      sDevice      = NULL;
static usb_transfer_t          *sReportXfer  = NULL;
static usb_transfer_t          *sControlXfer = NULL;
static uint8_t                  sInterface   = 0;
static uint8_t                  sEndpoint    = 0;
static uint16_t                 sMaxPacket   = HID_BOOT_REPORT_SIZE;
static bool                     sClaimed     = false;
static volatile bool            sConnected   = false;
static ControlStage             sStage       = CONTROL_STAGE_NONE;

static void StartReportTransfer(void);

bool CP400UsbKeyboard_Connected(void)
{
    return sConnected;
}

static void ClearKeyboardState(void)
{
    for (uint8_t index = 0; index < sizeof(USB_DEV_CONTROL.USB_CP400_Key_Array); index++)
    {
        USB_DEV_CONTROL.USB_CP400_Key_Array[index] = 0;
    }
}

/*
 * Walks the configuration descriptor for a HID interface that carries an
 * interrupt IN endpoint. Boot keyboards are preferred because forcing the boot
 * protocol is what guarantees the fixed 8 byte report UpdateKeyMap() expects.
 */
static bool FindKeyboardInterface(const usb_config_desc_t *config, bool requireBootKeyboard,
                                  uint8_t *interfaceNumber, uint8_t *alternateSetting,
                                  uint8_t *endpointAddress, uint16_t *maxPacketSize)
{
    const uint8_t *data = (const uint8_t *)config;
    const uint16_t total = config->wTotalLength;
    uint16_t offset = 0;

    bool candidate = false;
    uint8_t candidateInterface = 0;
    uint8_t candidateAlternate = 0;

    while (offset + 2 <= total)
    {
        const uint8_t length = data[offset];
        const uint8_t type = data[offset + 1];

        if (length == 0 || offset + length > total)
        {
            break;
        }

        if (type == USB_B_DESCRIPTOR_TYPE_INTERFACE)
        {
            const usb_intf_desc_t *interface = (const usb_intf_desc_t *)&data[offset];
            candidate = (interface->bInterfaceClass == USB_CLASS_HID);
            if (candidate && requireBootKeyboard)
            {
                candidate = (interface->bInterfaceSubClass == HID_SUBCLASS_BOOT &&
                             interface->bInterfaceProtocol == HID_PROTOCOL_KEYBOARD);
            }
            candidateInterface = interface->bInterfaceNumber;
            candidateAlternate = interface->bAlternateSetting;
        }
        else if (type == USB_B_DESCRIPTOR_TYPE_ENDPOINT && candidate)
        {
            const usb_ep_desc_t *endpoint = (const usb_ep_desc_t *)&data[offset];
            const bool directionIn = (USB_EP_DESC_GET_EP_DIR(endpoint) != 0);
            const bool interruptType =
                ((endpoint->bmAttributes & USB_BM_ATTRIBUTES_XFERTYPE_MASK) == USB_BM_ATTRIBUTES_XFER_INT);

            if (directionIn && interruptType)
            {
                *interfaceNumber = candidateInterface;
                *alternateSetting = candidateAlternate;
                *endpointAddress = endpoint->bEndpointAddress;
                *maxPacketSize = USB_EP_DESC_GET_MPS(endpoint);
                return true;
            }
        }

        offset += length;
    }

    return false;
}

static void ReportCallback(usb_transfer_t *transfer)
{
    if (transfer->status == USB_TRANSFER_STATUS_NO_DEVICE ||
        transfer->status == USB_TRANSFER_STATUS_CANCELED)
    {
        //Teardown is handled by the device gone event.
        return;
    }

    if (transfer->status == USB_TRANSFER_STATUS_COMPLETED)
    {
        //Short reports are padded so a partial packet cannot leave stale keycodes.
        uint8_t report[HID_BOOT_REPORT_SIZE];
        memset(report, 0, sizeof(report));

        int copied = transfer->actual_num_bytes;
        if (copied > HID_BOOT_REPORT_SIZE)
        {
            copied = HID_BOOT_REPORT_SIZE;
        }
        if (copied > 0)
        {
            memcpy(report, transfer->data_buffer, copied);
        }

        UpdateKeyMap(report);
    }
    else if (transfer->status == USB_TRANSFER_STATUS_STALL)
    {
        usb_host_endpoint_clear(transfer->device_handle, transfer->bEndpointAddress);
    }

    if (sConnected)
    {
        usb_host_transfer_submit(transfer);
    }
}

static void SubmitControlRequest(uint8_t request, uint16_t value)
{
    if (sControlXfer == NULL || sDevice == NULL)
    {
        return;
    }

    usb_setup_packet_t *setup = (usb_setup_packet_t *)sControlXfer->data_buffer;
    setup->bmRequestType = HID_REQUEST_TYPE_OUT;
    setup->bRequest = request;
    setup->wValue = value;
    setup->wIndex = sInterface;
    setup->wLength = 0;

    sControlXfer->device_handle = sDevice;
    sControlXfer->bEndpointAddress = 0;
    sControlXfer->num_bytes = sizeof(usb_setup_packet_t);
    sControlXfer->context = NULL;

    if (usb_host_transfer_submit_control(sClient, sControlXfer) != ESP_OK)
    {
        //If the request cannot even be queued, still try to read the keyboard.
        sStage = CONTROL_STAGE_READY;
        StartReportTransfer();
    }
}

/*
 * SET_PROTOCOL and SET_IDLE are chained through this callback rather than
 * waited on, because the completion runs in the same task that pumps client
 * events and blocking there would deadlock. A stall is not fatal: plenty of
 * keyboards reject SET_IDLE yet still stream reports.
 */
static void ControlCallback(usb_transfer_t *transfer)
{
    (void)transfer;

    if (!sConnected)
    {
        return;
    }

    switch (sStage)
    {
    case CONTROL_STAGE_SET_PROTOCOL:
        sStage = CONTROL_STAGE_SET_IDLE;
        SubmitControlRequest(HID_REQUEST_SET_IDLE, 0);
        break;

    case CONTROL_STAGE_SET_IDLE:
        sStage = CONTROL_STAGE_READY;
        StartReportTransfer();
        break;

    default:
        break;
    }
}

static void StartReportTransfer(void)
{
    if (sReportXfer == NULL)
    {
        return;
    }

    sReportXfer->device_handle = sDevice;
    sReportXfer->bEndpointAddress = sEndpoint;
    sReportXfer->callback = ReportCallback;
    sReportXfer->context = NULL;
    //IN transfers must request a whole number of max sized packets.
    sReportXfer->num_bytes = sMaxPacket;

    const esp_err_t err = usb_host_transfer_submit(sReportXfer);
    if (err != ESP_OK)
    {
        Serial.printf("USB keyboard: could not start reports (%s)\n", esp_err_to_name(err));
    }
}

static void ReleaseDevice(void)
{
    sConnected = false;
    sStage = CONTROL_STAGE_NONE;

    if (sReportXfer != NULL)
    {
        usb_host_transfer_free(sReportXfer);
        sReportXfer = NULL;
    }
    if (sControlXfer != NULL)
    {
        usb_host_transfer_free(sControlXfer);
        sControlXfer = NULL;
    }
    if (sClaimed && sDevice != NULL)
    {
        usb_host_interface_release(sClient, sDevice, sInterface);
        sClaimed = false;
    }
    if (sDevice != NULL)
    {
        usb_host_device_close(sClient, sDevice);
        sDevice = NULL;
    }

    ClearKeyboardState();
}

static void OpenDevice(uint8_t address)
{
    if (sDevice != NULL)
    {
        //Only one keyboard is supported; the OTG controller has no hub support
        //in this ESP-IDF version anyway.
        return;
    }

    if (usb_host_device_open(sClient, address, &sDevice) != ESP_OK)
    {
        Serial.printf("USB keyboard: could not open device at address %u\n", address);
        sDevice = NULL;
        return;
    }

    usb_device_info_t info;
    if (usb_host_device_info(sDevice, &info) == ESP_OK)
    {
        Serial.printf("USB keyboard: device at address %u is %s speed\n",
                      address, (info.speed == USB_SPEED_LOW) ? "low" : "full");
    }

    const usb_device_desc_t *deviceDescriptor = NULL;
    if (usb_host_get_device_descriptor(sDevice, &deviceDescriptor) == ESP_OK &&
        deviceDescriptor != NULL)
    {
        Serial.printf("USB keyboard: VID=0x%04x PID=0x%04x\n",
                      deviceDescriptor->idVendor, deviceDescriptor->idProduct);
    }

    const usb_config_desc_t *configDescriptor = NULL;
    if (usb_host_get_active_config_descriptor(sDevice, &configDescriptor) != ESP_OK ||
        configDescriptor == NULL)
    {
        Serial.println("USB keyboard: configuration descriptor unavailable");
        ReleaseDevice();
        return;
    }

    uint8_t alternateSetting = 0;
    bool found = FindKeyboardInterface(configDescriptor, true, &sInterface, &alternateSetting,
                                       &sEndpoint, &sMaxPacket);
    if (!found)
    {
        //Some keyboards do not advertise the boot subclass but still answer
        //SET_PROTOCOL, so fall back to any HID interrupt IN interface.
        found = FindKeyboardInterface(configDescriptor, false, &sInterface, &alternateSetting,
                                      &sEndpoint, &sMaxPacket);
    }

    if (!found)
    {
        Serial.println("USB keyboard: no HID interface with an interrupt IN endpoint");
        ReleaseDevice();
        return;
    }

    if (sMaxPacket == 0 || sMaxPacket > MAX_REPORT_PACKET_SIZE)
    {
        sMaxPacket = HID_BOOT_REPORT_SIZE;
    }

    if (usb_host_interface_claim(sClient, sDevice, sInterface, alternateSetting) != ESP_OK)
    {
        Serial.printf("USB keyboard: could not claim interface %u\n", sInterface);
        ReleaseDevice();
        return;
    }
    sClaimed = true;

    if (usb_host_transfer_alloc(CONTROL_BUFFER_SIZE, 0, &sControlXfer) != ESP_OK ||
        usb_host_transfer_alloc(sMaxPacket, 0, &sReportXfer) != ESP_OK)
    {
        Serial.println("USB keyboard: out of memory for USB transfers");
        ReleaseDevice();
        return;
    }

    sControlXfer->callback = ControlCallback;
    sConnected = true;

    Serial.printf("USB keyboard ready on interface %u endpoint 0x%02x\n", sInterface, sEndpoint);

    sStage = CONTROL_STAGE_SET_PROTOCOL;
    SubmitControlRequest(HID_REQUEST_SET_PROTOCOL, HID_PROTOCOL_BOOT);
}

static void ClientEventCallback(const usb_host_client_event_msg_t *message, void *arg)
{
    (void)arg;

    switch (message->event)
    {
    case USB_HOST_CLIENT_EVENT_NEW_DEV:
        OpenDevice(message->new_dev.address);
        break;

    case USB_HOST_CLIENT_EVENT_DEV_GONE:
        if (message->dev_gone.dev_hdl == sDevice)
        {
            Serial.println("USB keyboard disconnected");
            ReleaseDevice();
        }
        break;

    default:
        break;
    }
}

static void ClientTask(void *parameters)
{
    (void)parameters;

    usb_host_client_config_t clientConfig = {};
    clientConfig.is_synchronous = false;
    clientConfig.max_num_event_msg = 5;
    clientConfig.async.client_event_callback = ClientEventCallback;
    clientConfig.async.callback_arg = NULL;

    const esp_err_t err = usb_host_client_register(&clientConfig, &sClient);
    if (err != ESP_OK)
    {
        Serial.printf("USB keyboard: client registration failed (%s)\n", esp_err_to_name(err));
        vTaskDelete(NULL);
        return;
    }

    while (true)
    {
        usb_host_client_handle_events(sClient, portMAX_DELAY);
    }
}

static void DaemonTask(void *parameters)
{
    (void)parameters;

    usb_host_config_t hostConfig = {};
    hostConfig.skip_phy_setup = false;
    hostConfig.intr_flags = ESP_INTR_FLAG_LEVEL1;

    const esp_err_t err = usb_host_install(&hostConfig);
    if (err != ESP_OK)
    {
        Serial.printf("USB keyboard: host install failed (%s)\n", esp_err_to_name(err));
        vTaskDelete(NULL);
        return;
    }

    Serial.println("USB keyboard host started on GPIO19/GPIO20");

    xTaskCreatePinnedToCore(ClientTask, "UsbKbdClient", USB_KBD_STACK_SIZE, NULL,
                            USB_KBD_CLIENT_PRIORITY, NULL, USB_KBD_CORE);

    while (true)
    {
        uint32_t eventFlags = 0;
        usb_host_lib_handle_events(portMAX_DELAY, &eventFlags);

        if (eventFlags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS)
        {
            usb_host_device_free_all();
        }
    }
}

void CP400UsbKeyboard_Start(void)
{
    xTaskCreatePinnedToCore(DaemonTask, "UsbKbdDaemon", USB_KBD_STACK_SIZE, NULL,
                            USB_KBD_DAEMON_PRIORITY, NULL, USB_KBD_CORE);
}
