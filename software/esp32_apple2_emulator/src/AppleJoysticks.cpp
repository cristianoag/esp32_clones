#include "AppleJoysticks.h"
#include "AppleJoystickHost.h"
#include <Arduino.h>
#include <Preferences.h>
#include <string.h>

namespace
{
struct SavedJoystick
{
    uint32_t version;
    uint16_t vid, pid;
    uint8_t endpoint;
    uint8_t reserved[3];
    AppleJoystickMapping mapping;
};

portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
AppleJoystickSnapshot state[2] = {};
SavedJoystick saved[2] = {};
bool hasCalibration[2] = {};
bool badReportLogged[2] = {};
bool otherEndpointLogged[2] = {};
Preferences storage;
bool storageReady = false;
const char *keys[] = {"port1", "port2"};

bool fail(char *error, size_t size, const char *message)
{
    snprintf(error, size, "%s", message);
    Serial.printf("APPLE joystick: %s\n", message);
    return false;
}

void device(unsigned port, uint16_t vid, uint16_t pid, bool connected)
{
    if (port > 1) return;
    portENTER_CRITICAL(&lock);
    const uint32_t generation = state[port].generation + 1;
    state[port] = {};
    state[port].generation = generation;
    state[port].vid = vid;
    state[port].pid = pid;
    state[port].connected = connected;
    state[port].calibrated = connected && hasCalibration[port] &&
                            saved[port].vid == vid && saved[port].pid == pid;
    badReportLogged[port] = otherEndpointLogged[port] = false;
    portEXIT_CRITICAL(&lock);
    Serial.printf("APPLE joystick %u: %s VID=%04x PID=%04x\n",
                  port + 1, connected ? "connected" : "disconnected", vid, pid);
}

void report(unsigned port, uint8_t endpoint, const uint8_t *data, size_t length)
{
    if (port > 1 || !data) return;
    bool logBad = false, logEndpoint = false;
    portENTER_CRITICAL(&lock);
    AppleJoystickSnapshot &current = state[port];
    if (!current.connected)
    {
        portEXIT_CRITICAL(&lock);
        return;
    }
    const uint8_t expectedEndpoint = current.calibrated ? saved[port].endpoint : current.endpoint;
    if (expectedEndpoint && endpoint != expectedEndpoint)
    {
        logEndpoint = !otherEndpointLogged[port];
        otherEndpointLogged[port] = true;
    }
    else if (!length || length > sizeof(current.report))
    {
        current.buttons = 0;
        current.length = 0;
        ++current.sequence;
        logBad = !badReportLogged[port];
        badReportLogged[port] = true;
    }
    else
    {
        current.endpoint = endpoint;
        current.length = length;
        memcpy(current.report, data, length);
        ++current.sequence;
        current.buttons = 0;
        if (current.calibrated &&
            !AppleDecodeJoystickReport(saved[port].mapping, data, length, current.buttons))
        {
            current.buttons = 0;
            logBad = !badReportLogged[port];
            badReportLogged[port] = true;
        }
    }
    portEXIT_CRITICAL(&lock);
    if (logBad) Serial.printf("APPLE joystick %u: report does not match calibration; input released.\n", port + 1);
    if (logEndpoint) Serial.printf("APPLE joystick %u: ignoring second endpoint; one report stream per pad supported.\n", port + 1);
}
}

bool AppleJoysticksStart()
{
    storageReady = storage.begin("apple-joystick", false);
    if (!storageReady) Serial.println("APPLE joystick: calibration NVS unavailable.");
    if (storageReady)
    {
        for (unsigned port = 0; port < 2; ++port)
        {
            if (!storage.isKey(keys[port])) continue;
            const size_t size = storage.getBytesLength(keys[port]);
            if (!size) continue;
            SavedJoystick candidate = {};
            if (size != sizeof(candidate) ||
                storage.getBytes(keys[port], &candidate, sizeof(candidate)) != sizeof(candidate) ||
                candidate.version != 1 || !(candidate.endpoint & 0x80) || !(candidate.endpoint & 0x0f) ||
                !AppleValidJoystickMapping(candidate.mapping))
            {
                Serial.printf("APPLE joystick %u: invalid saved calibration; recalibrate in F12.\n", port + 1);
                continue;
            }
            saved[port] = candidate;
            hasCalibration[port] = true;
        }
    }
    return AppleJoystickHostStart(device, report);
}

void AppleJoystickSnapshotFor(unsigned port, AppleJoystickSnapshot &snapshot)
{
    if (port > 1)
    {
        snapshot = {};
        Serial.println("APPLE joystick: invalid port snapshot.");
        return;
    }
    portENTER_CRITICAL(&lock);
    snapshot = state[port];
    portEXIT_CRITICAL(&lock);
}

uint16_t AppleJoysticksRead()
{
    portENTER_CRITICAL(&lock);
    const uint16_t result = (state[0].buttons & 0x3f) | ((state[1].buttons & 0x3f) << 8);
    portEXIT_CRITICAL(&lock);
    return result;
}

bool AppleJoystickSave(unsigned port, const AppleJoystickSnapshot &identity,
                     const AppleJoystickMapping &mapping, char *error, size_t errorSize)
{
    if (port > 1 || !identity.length || identity.length > 8 || mapping.length != identity.length ||
        !(identity.endpoint & 0x80) || !(identity.endpoint & 0x0f) ||
        !AppleValidJoystickMapping(mapping))
        return fail(error, errorSize, "Invalid joystick calibration.");
    if (!storageReady) return fail(error, errorSize, "Calibration storage unavailable.");
    AppleJoystickSnapshot current;
    AppleJoystickSnapshotFor(port, current);
    if (!current.connected || current.generation != identity.generation ||
        current.vid != identity.vid || current.pid != identity.pid ||
        current.endpoint != identity.endpoint || current.length != identity.length)
        return fail(error, errorSize, "Joystick changed during calibration; retry.");
    SavedJoystick candidate = {};
    candidate.version = 1;
    candidate.vid = identity.vid;
    candidate.pid = identity.pid;
    candidate.endpoint = identity.endpoint;
    candidate.mapping = mapping;
    if (!AppleJoystickHostPause()) return fail(error, errorSize, "Cannot pause joystick host for saving.");
    const bool written = storage.putBytes(keys[port], &candidate, sizeof(candidate)) == sizeof(candidate);
    if (written)
    {
        portENTER_CRITICAL(&lock);
        saved[port] = candidate;
        hasCalibration[port] = true;
        state[port].buttons = 0;
        portEXIT_CRITICAL(&lock);
    }
    AppleJoystickHostResume();
    if (!written) return fail(error, errorSize, "Cannot save joystick calibration.");
    snprintf(error, errorSize, "Joystick %u calibration saved.", port + 1);
    return true;
}

bool AppleJoystickClear(unsigned port, char *error, size_t errorSize)
{
    if (port > 1 || !storageReady) return fail(error, errorSize, "Calibration storage unavailable.");
    if (!AppleJoystickHostPause()) return fail(error, errorSize, "Cannot pause joystick host for saving.");
    const bool removed = !storage.isKey(keys[port]) || storage.remove(keys[port]);
    if (removed)
    {
        portENTER_CRITICAL(&lock);
        hasCalibration[port] = false;
        state[port].buttons = 0;
        state[port].calibrated = false;
        portEXIT_CRITICAL(&lock);
    }
    AppleJoystickHostResume();
    if (!removed) return fail(error, errorSize, "Cannot remove saved joystick calibration.");
    snprintf(error, errorSize, "Joystick %u calibration cleared.", port + 1);
    return true;
}
