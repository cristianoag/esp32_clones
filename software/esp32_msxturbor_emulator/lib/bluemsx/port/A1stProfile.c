#include "A1stProfile.h"
#include <string.h>
#ifdef ESP_PLATFORM
#include <esp_timer.h>
#else
#include <time.h>
#endif

bool a1stProfileActive;
static unsigned sequence;
static uint64_t frameStart, starts[A1ST_WORK_COUNT], current[A1ST_WORK_COUNT];
static A1stWorkStats window;

uint64_t a1stProfileNow(void)
{
#ifdef ESP_PLATFORM
    return esp_timer_get_time();
#else
    return (uint64_t)clock() * 1000000 / CLOCKS_PER_SEC;
#endif
}

void a1stProfileReset(void)
{
    a1stProfileActive = false;
    sequence = 0;
    memset(&window, 0, sizeof(window));
}

void a1stProfileStart(uint64_t now)
{
    a1stProfileActive = (++sequence % 17) == 0;
    if (!a1stProfileActive) return;
    frameStart = now;
    memset(current, 0, sizeof(current));
}

void a1stProfileFinish(uint64_t now)
{
    if (!a1stProfileActive) return;
    ++window.samples;
    window.coreUs += now - frameStart;
    window.videoUs += current[A1ST_WORK_VIDEO];
    window.audioUs += current[A1ST_WORK_AUDIO];
    a1stProfileActive = false;
}

void a1stProfileBegin(unsigned section, uint64_t now) { starts[section] = now; }
void a1stProfileEnd(unsigned section, uint64_t now) { current[section] += now - starts[section]; }
void a1stProfileTake(A1stWorkStats* stats)
{
    *stats = window;
    memset(&window, 0, sizeof(window));
}
