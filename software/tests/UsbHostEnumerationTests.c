#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MSX_SOFT_USB 1
#define DEF_BUFF_SIZE 256
#define NUM_USB 4
#define ZERO_USB_ADDRESS 0
#define ASSIGNED_USB_ADDRESS 3
#define STDCLASS 0
#define HIDCLASS 3
#ifdef MACPLUS_MOUSE
#include "mac_mouse.h"
#endif
#include "UsbTypes.inc"

static sUsbContStruct current_usb[NUM_USB];
static unsigned detectedPort, reportPort, reportEndpoint, reportLength, reports;
static uint8_t delivered[8];
static unsigned M_ONE = 1, P_ONE = 2;
static void detected(uint8_t port, void *descriptor)
{
    assert(descriptor == &current_usb[port].desc);
    detectedPort = port;
}
static void received(uint8_t port, uint8_t endpoint, uint8_t length, const uint8_t *data)
{
    reportPort = port;
    reportEndpoint = endpoint;
    reportLength = length;
    memcpy(delivered, data, length);
    ++reports;
}
static void (*onDetectCB)(uint8_t, void *) = detected;
static void (*onDisconnectCB)(uint8_t);
static void (*usbRaw)(uint8_t, uint8_t, uint8_t, const uint8_t *) = received;
#include "UsbLogic.inc"

static uint8_t config[] = {
    9, 2, 34, 0, 1, 2, 0, 0x80, 50,
    9, 4, 3, 0, 1, 3, 0, 0, 0,
    9, 0x21, 0x11, 1, 0, 1, 0x22, 0x34, 0x01,
    7, 5, 0x83, 3, 8, 0, 10
};
static void step(void)
{
    current->bComplete = 1;
    fsm_Mashine();
}
static void prepare(unsigned port)
{
    memset(current_usb, 0, sizeof(current_usb));
    current = &current_usb[port];
    current->selfNum = port;
    current->isValid = 1;
    current->connected = true;
    current->desc.bDeviceClass = STDCLASS;
    current->cfg.bCV = 2;
    current->fsm_state = 97;
    current->wires_last_state = M_ONE;
    current->ufPrintDesc = 4;
    current->descrBufferLen = sizeof(config);
    memcpy(current->descrBuffer, config, sizeof(config));
    detectedPort = 99;
}
static void parse(void)
{
    for (unsigned i = 0; i < NUM_USB; ++i) printState();
    assert(!(current->ufPrintDesc & 4));
}
static void enumerate(unsigned port, bool stallIdle)
{
    prepare(port);
    step(); // Wait for task-context descriptor parsing.
    assert(current->fsm_state == 97 && current->cb_Cmd == CB_TICK);
    parse();
    assert(current->inputCount == 1 && current->inputInterface == 3);
    step();
    assert(current->fsm_state == 98 && current->rq.bmRequest == 9);
    assert(current->rq.wValue == 2 && current->rq.bmRequestType == 0);
    current->numb_reps_errors_allowed = 4;
    step();
    assert(current->fsm_state == 90 && current->rq.bmRequestType == 0x21);
#ifdef MACPLUS_MOUSE
    assert(current->rq.bmRequest == 0x0b && current->rq.wValue == 0);
#else
    assert(current->rq.bmRequest == 0x0a);
#endif
    assert(current->rq.wIndex == 3);
    assert(current->rq.wLen == 0);
    current->numb_reps_errors_allowed = stallIdle ? 0 : 4;
    step();
#ifdef MACPLUS_MOUSE
    if (stallIdle) {
        assert(current->fsm_state == 104 && detectedPort == 99);
        return;
    }
#endif
    assert(current->fsm_state == 100 && detectedPort == port);
    step();
    assert(current->fsm_state == 101 && current->rq.eop == 3);
    assert(current->asckedReceiveBytes == 8);
    const uint8_t payload[] = {0, 127, 255, 0x10, 0x20, 0x55, 0xaa, 0};
    memcpy(current->acc_decoded_resp, payload, sizeof(payload));
#ifdef MACPLUS_MOUSE
    const unsigned payloadSize = 3;
#else
    const unsigned payloadSize = sizeof(payload);
#endif
    current->acc_decoded_resp_counter = payloadSize;
    unsigned before = reports;
    step();
    assert(reports == before + 1 && reportPort == port && reportEndpoint == 0x83);
    assert(reportLength == payloadSize && !memcmp(delivered, payload, payloadSize));
    current->wires_last_state = 0;
    step();
    assert(current->fsm_state == 0 && current->cb_Cmd == CB_CHECK);
}
int main(void)
{
#ifdef MACPLUS_MOUSE
    config[15] = 1;
    config[16] = 2;
    enumerate(1, false);
    enumerate(1, true); // Boot protocol must succeed for a boot mouse.
    config[25] = config[26] = 0;
    enumerate(1, false); // Boot protocol does not need a report descriptor.
    prepare(0);
    parse();
    step();
    assert(current->fsm_state == 104 && !current->inputCount); // Port 1 remains disabled.
    const unsigned validPort = 1;
#else
    for (unsigned port = 0; port < 2; ++port) {
        enumerate(port, false); // Report descriptor is 308 bytes, not fetched.
        enumerate(port, true);  // Optional HID SET_IDLE can STALL.
        config[25] = config[26] = 0; // No usable report descriptor.
        enumerate(port, false);
        config[25] = 0x34;
        config[26] = 1;
    }
    const unsigned validPort = 0;
#endif
    prepare(validPort);
    parse();
    step();
    current->numb_reps_errors_allowed = 0;
    step();
    assert(current->fsm_state == 104 && detectedPort == 99); // Configuration is mandatory.
#ifdef MACPLUS_MOUSE
    const unsigned firstProtocol = 0, lastProtocol = 1;
#else
    const unsigned firstProtocol = 1, lastProtocol = 2;
#endif
    for (unsigned protocol = firstProtocol; protocol <= lastProtocol; ++protocol) {
        prepare(validPort);
        current->descrBuffer[16] = protocol; // Reject inappropriate HID interface protocols.
        parse();
        step();
        assert(current->fsm_state == 104 && !current->inputCount);
    }
    prepare(validPort);
    current->descrBuffer[31] = 9; // Oversized low-speed interrupt packet.
    parse();
    step();
    assert(current->fsm_state == 104 && !current->inputCount);
    prepare(validPort);
    current->descrBuffer[27] = 0; // Malformed descriptor cannot loop forever.
    parse();
    step();
    assert(current->fsm_state == 104 && !current->inputCount);
#ifdef MACPLUS_MOUSE
    prepare(validPort);
    current->descrBuffer[31] = 2;
    parse();
    step();
    assert(current->fsm_state == 104 && !current->inputCount);
    puts("PASS: boot mouse on port 2 only, SET_PROTOCOL, short reports, configuration failure and unplug.");
#else
    puts("PASS: both ports, opaque/long HID descriptors, SET_IDLE, config failure, reports and unplug.");
#endif
}
