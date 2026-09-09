#include "MsxJoysticks.h"
#include "MsxJoystickHost.h"
#include "Arduino.h"
#include "Preferences.h"
#include <assert.h>
#include <string.h>

JoystickTestSerial Serial;
bool JoystickStorageFailure = false;
std::map<std::string, std::vector<uint8_t>> JoystickStored;
static void (*onDevice)(unsigned, uint16_t, uint16_t, bool);
static void (*onReport)(unsigned, uint8_t, const uint8_t *, size_t);
static bool connected[2];
static bool pauseFailure = false, paused = false;
static unsigned pauses, resumes;
static uint16_t vids[] = {0x1234, 0x5678}, pids[] = {1, 2};

bool MsxJoystickHostStart(void (*device)(unsigned, uint16_t, uint16_t, bool),
                          void (*report)(unsigned, uint8_t, const uint8_t *, size_t))
{
    onDevice = device;
    onReport = report;
    return true;
}
bool MsxJoystickHostPause()
{
    if (pauseFailure) return false;
    paused = true;
    ++pauses;
    for (unsigned i = 0; i < 2; ++i) onDevice(i, vids[i], pids[i], false);
    return true;
}
void MsxJoystickHostResume()
{
    assert(paused);
    paused = false;
    ++resumes;
    for (unsigned i = 0; i < 2; ++i)
        if (connected[i]) onDevice(i, vids[i], pids[i], true);
}
static void input(unsigned port, uint8_t directions, uint8_t buttons)
{
    const uint8_t data[] = {directions, buttons};
    onReport(port, 0x81, data, sizeof(data));
}
static void attach(unsigned port)
{
    connected[port] = true;
    onDevice(port, vids[port], pids[port], true);
    input(port, 0, 0);
}

int main()
{
    const uint8_t directions[] = {0, 1, 9, 8, 10, 2, 6, 4, 5, 0, 0};
    MsxJoystickSample samples[11] = {};
    for (unsigned i = 0; i < 11; ++i) samples[i].bytes[0] = directions[i];
    samples[9].bytes[1] = 1;
    samples[10].bytes[1] = 2;
    MsxJoystickMapping mapping;
    char error[160];
    assert(MsxBuildJoystickMapping(2, samples, mapping, error, sizeof(error)));
    assert(MsxJoysticksStart());
    attach(0);
    attach(1);
    input(0, 1, 1);
    assert(MsxJoysticksRead() == 0); // Never guess unknown report layouts.
    for (unsigned i = 0; i < 2; ++i)
    {
        input(i, 0, 0);
        MsxJoystickSnapshot snapshot;
        MsxJoystickSnapshotFor(i, snapshot);
        assert(snapshot.connected && !snapshot.calibrated);
        assert(MsxJoystickSave(i, snapshot, mapping, error, sizeof(error)));
        assert(!MsxJoysticksRead());
    }
    input(0, 9, 1);
    input(1, 6, 2);
    assert(MsxJoysticksRead() == (0x19 | (0x26 << 8)));
    input(0, 0, 0);
    assert(MsxJoysticksRead() == 0x2600);
    const uint8_t shortReport[] = {0};
    onReport(1, 0x81, shortReport, sizeof(shortReport));
    assert(!MsxJoysticksRead());
    input(1, 2, 3);
    assert(MsxJoysticksRead() == 0x3200);
    const uint8_t otherEndpoint[] = {0, 0};
    onReport(1, 0x82, otherEndpoint, sizeof(otherEndpoint));
    assert(MsxJoysticksRead() == 0x3200);
    connected[1] = false;
    onDevice(1, vids[1], pids[1], false);
    assert(!MsxJoysticksRead());
    input(1, 2, 3); // Stale queued report after disconnect cannot resurrect input.
    assert(!MsxJoysticksRead());
    attach(1);
    input(1, 4, 2);
    assert(MsxJoysticksRead() == 0x2400);
    MsxJoystickSnapshot before;
    MsxJoystickSnapshotFor(1, before);
    ++pids[1];
    attach(1);
    input(1, 4, 2);
    assert(!MsxJoysticksRead());
    assert(!MsxJoystickSave(1, before, mapping, error, sizeof(error)));
    --pids[1];
    attach(1);
    input(1, 4, 2);
    assert(MsxJoysticksRead() == 0x2400);
    MsxJoystickSnapshotFor(1, before);
    pauseFailure = true;
    assert(!MsxJoystickSave(1, before, mapping, error, sizeof(error)));
    assert(MsxJoysticksRead() == 0x2400);
    pauseFailure = false;
    JoystickStorageFailure = true;
    assert(!MsxJoystickSave(1, before, mapping, error, sizeof(error)));
    assert(!paused);
    input(1, 4, 2);
    assert(MsxJoysticksRead() == 0x2400);
    JoystickStorageFailure = false;
    assert(MsxJoystickClear(1, error, sizeof(error)));
    input(1, 1, 1);
    input(0, 8, 1);
    assert(MsxJoysticksRead() == 0x18);
    assert(JoystickStored.count("port1") && !JoystickStored.count("port2"));
    assert(pauses == resumes);
    assert(MsxJoysticksStart()); // Exercise reloading persisted calibration.
    attach(0);
    input(0, 2, 2);
    assert(MsxJoysticksRead() == 0x22);
    puts("PASS: actual joystick backend, independent ports, persistence, disconnect, identity, short reports and flash pauses.");
}
