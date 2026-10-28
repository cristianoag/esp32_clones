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
#ifdef C64_USB_CONTROL
#define SET_I ((void)0)
#define SET_O ((void)0)
#define SE_0 ((void)0)
#define SE_J ((void)0)
#define READ_BOTH_PINS (M_ONE << 8)
#define SMALL_NO_DATA 36
static bool hid_device_connected, msxAcknowledgeReplay, msxReplayAcked;
static unsigned received_NRZI_buffer_bytesCnt;
static unsigned traceSaves, decodedIndex;
static int wireResult;
static bool wireReplayAck;
static uint8_t decoded[] = {T_START, T_DATA1, 0, 0};
static void SOF(void) {}
static void pu_Addr(uint8_t cmd, uint8_t addr, uint8_t ep)
{ (void)cmd; (void)addr; (void)ep; }
static void pu_Cmd(uint8_t pid, uint8_t type, uint8_t request,
                   uint16_t value, uint16_t index, uint16_t length)
{ (void)pid; (void)type; (void)request; (void)value; (void)index; (void)length; }
static void pu_MSB(uint32_t value, int bits) { (void)value; (void)bits; }
static void pu_LSB(uint32_t value, int bits) { (void)value; (void)bits; }
static uint32_t cal16(void) { return 0; }
static void repack(void) {}
static void sendOnly(void) {}
static void ACK(void) {}
static void pu_ShortCmd(uint8_t pid) { (void)pid; }
static uint8_t rev8(uint8_t value)
{
    uint8_t result = 0;
    for (unsigned i = 0; i < 8; ++i) {
        result = (uint8_t)((result << 1) | (value & 1));
        value >>= 1;
    }
    return result;
}
static void decoded_receive_buffer_clear(void) { decodedIndex = 0; }
static unsigned decoded_receive_buffer_size(void) { return sizeof(decoded); }
static uint8_t decoded_receive_buffer_get(void)
{ assert(decodedIndex < sizeof(decoded)); return decoded[decodedIndex++]; }
static int parse_received_NRZI_buffer(void) { return wireResult; }
static void sendRecieveNParse(void)
{ msxReplayAcked = msxAcknowledgeReplay && wireReplayAck; }
static int sendRecieve(void) { sendRecieveNParse(); return wireResult; }
static void msxSaveControlTrace(void) { ++traceSaves; }
#endif
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
#ifdef C64_USB_CONTROL
static void poll(int result, bool replayAck)
{
    wireResult = result;
    wireReplayAck = replayAck;
    received_NRZI_buffer_bytesCnt = result == T_NEED_ACK ? 28 : 12;
    ++current->tick;
    timerCallBack();
    fsm_Mashine();
}
static void configure(unsigned port)
{
    prepare(port);
    parse();
    step();
    assert(current->fsm_state == 98 && current->cb_Cmd == CB_5);
    poll(T_ACK, false);
    assert(current->fsm_state == 98 && current->cb_Cmd == CB_6);
}
static void controlStatusRegression(void)
{
    for (unsigned port = 0; port < 2; ++port) {
        configure(port);
        for (unsigned i = 0; i < 50; ++i) {
            poll(T_NACK, false); // SET_CONFIGURATION may need time to finish.
            assert(current->fsm_state == 98 && detectedPort == 99);
        }
        poll(T_NEED_ACK, false); // CRC-valid DATA1 ZLP, not yet acknowledged.
        assert(current->cb_Cmd == CB_7 && detectedPort == 99);
        const unsigned failures = current->controlFailures;
        const unsigned traces = traceSaves;
        for (unsigned i = 0; i < 50; ++i) {
            poll(T_NACK, false); // Retry the status replay, not SETUP.
            assert(current->fsm_state == 98 && detectedPort == 99);
        }
        assert(current->controlFailures > failures && traceSaves > traces);
        assert(current->lastControlCommand == CB_7);
        poll(T_NEED_ACK, true);
        assert(current->fsm_state == 90 && current->rq.bmRequest == 0x0a);
        assert(detectedPort == 99); // Configuration status ACK is mandatory.
        poll(T_ACK, false);
        poll(T_STALL, false); // SET_IDLE remains optional.
        assert(current->fsm_state == 100 && detectedPort == port);

        for (unsigned replay = 0; replay < 2; ++replay) {
            configure(port);
            if (replay) poll(T_NEED_ACK, false);
            for (unsigned i = 0; i < 99; ++i) {
                poll(T_NACK, false);
                assert(current->fsm_state == 98 && detectedPort == 99);
            }
            poll(T_NACK, false);
            assert(current->fsm_state == 104 && detectedPort == 99);
            assert(current->failedEnumerationState == 98);
        }
        configure(port);
        poll(T_STALL, false);
        assert(current->fsm_state == 104 && detectedPort == 99);
        configure(port);
        poll(T_NEED_ACK, false);
        poll(T_STALL, false);
        assert(current->fsm_state == 104 && detectedPort == 99);
        configure(port);
        for (unsigned i = 0; i < 200 && current->fsm_state == 98; ++i)
            poll(T_CHK_ERR, false); // Corruption can never enable input.
        assert(current->fsm_state == 104 && detectedPort == 99);

        prepare(port);
        current->fsm_state = 8;
        Request(T_SETUP, ASSIGNED_USB_ADDRESS, 0, T_DATA0, 0x80, 6, 0x0200, 0, 9, 9);
        poll(T_ACK, false);
        for (unsigned i = 0; i < 3; ++i) {
            poll(T_NACK, false);
            assert(current->fsm_state == 8);
        }
        poll(T_NACK, false);
        assert(current->fsm_state == 0); // Descriptor-read retry policy is unchanged.
    }
    puts("PASS: delayed configuration status/replay, bounded timeouts, STALL, CRC rejection and replay diagnostics on both ports.");
}
#endif
int main(void)
{
#ifdef C64_USB_CONTROL
    controlStatusRegression();
#endif
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
