/**
 * Copyright 2024 TotalJustice.
 * SPDX-License-Identifier: MIT
 */
#include "scheduler.h"
#include <stdlib.h>
#include <string.h>

static void find_next_event(struct Scheduler* s) {
    unsigned i;
    unsigned new_id = 0;
    for (i = 1; i < s->max_events; i++) {
        /* this implicitly checks if the event is enabled :big_brain: */
        if (s->events[i].time < s->events[new_id].time) {
            new_id = i;
        }
    }
    s->next_event_id = new_id;
}

static void default_reset_event(void* user, unsigned id, unsigned cycles_late_unused) {
    struct Scheduler* s = user;
    scheduler_reset_event(s);
    scheduler_add_absolute(s, id, SCHEDULER_TIMEOUT_CYCLES, default_reset_event, user);
}

int scheduler_init(struct Scheduler* s, unsigned max_events) {
    s->max_events = max_events + 1;
    s->events = calloc(s->max_events, sizeof(*s->events));
    s->callbacks = calloc(s->max_events, sizeof(*s->callbacks));

    if (!s->events || !s->callbacks) {
        scheduler_quit(s);
        return -1;
    }

    return 0;
}

void scheduler_quit(struct Scheduler* s) {
    if (s) {
        if (s->events) {
            free(s->events);
            s->events = NULL;
        }
        if (s->callbacks) {
            free(s->callbacks);
            s->callbacks = NULL;
        }
    }
}

void scheduler_reset(struct Scheduler* s, int starting_cycles, SchedulerCallback reset_cb, void* user) {
    unsigned i;
    for (i = 0; i < s->max_events; i++) {
        s->events[i].time = SCHEDULER_DISABLE_VALUE;
    }

    s->next_event_id = 0;
    s->cycles = starting_cycles < SCHEDULER_TIMEOUT_CYCLES ? starting_cycles : SCHEDULER_TIMEOUT_CYCLES;
    reset_cb = reset_cb ? reset_cb : default_reset_event;
    user = user ? user : s;
    scheduler_add_absolute(s, s->max_events - 1, SCHEDULER_TIMEOUT_CYCLES, reset_cb, user);
}

void scheduler_fire(struct Scheduler* s) {
    while (scheduler_should_fire(s)) {
        /* store local copy. */
        const unsigned id = s->next_event_id;
        const struct SchedulerEvent event = s->events[id];

        /* mark as disabled and find next event. */
        s->events[id].time = SCHEDULER_DISABLE_VALUE;
        find_next_event(s);

        /* call the expired event. */
        s->callbacks[id].callback(s->callbacks[id].user, id, s->cycles - event.time);
    }
}

void scheduler_add(struct Scheduler* s, unsigned id, int event_time, SchedulerCallback cb, void* user) {
    scheduler_add_absolute(s, id, s->cycles + event_time, cb, user);
}

void scheduler_add_absolute(struct Scheduler* s, unsigned id, int event_time, SchedulerCallback cb, void* user) {
    s->events[id].time = event_time;
    s->callbacks[id].callback = cb;
    s->callbacks[id].user = user;

    /* check if we updated the next event. */
    if (id == s->next_event_id) {
        find_next_event(s);
    }
    /* check if new event fires earlier. */
    else if (s->events[id].time < s->events[s->next_event_id].time) {
        s->next_event_id = id;
    }
}

void scheduler_remove(struct Scheduler* s, unsigned id) {
    s->events[id].time = SCHEDULER_DISABLE_VALUE;
    if (id == s->next_event_id) {
        find_next_event(s);
    }
}

void scheduler_advance_to_next_event(struct Scheduler* s) {
    /* only advance if the next event time is greater than current time. */
    if (s->events[s->next_event_id].time > s->cycles) {
        s->cycles = s->events[s->next_event_id].time;
    }
}

void scheduler_reset_event(struct Scheduler* s) {
    /* no sort because order remains the same. */
    unsigned i;
    for (i = 0; i < s->max_events; i++) {
        struct SchedulerEvent* e = &s->events[i];
        if (e->time != SCHEDULER_DISABLE_VALUE) {
            e->time -= SCHEDULER_TIMEOUT_CYCLES;
        }
    }
    s->cycles -= SCHEDULER_TIMEOUT_CYCLES;
}

void scheduler_dummy_event(void* user_unused, unsigned id_unused, unsigned cycles_late_unused) {
}

unsigned scheduler_get_reset_event_id(const struct Scheduler* s) {
    return s->max_events - 1;
}

static void save_state_write(void* dst, const void* src, unsigned size, unsigned* offset) {
    memcpy((unsigned char*)dst + *offset, src, size);
    *offset += size;
}

static void save_state_read(void* dst, const void* src, unsigned size, unsigned* offset) {
    memcpy(dst, (unsigned char*)src + *offset, size);
    *offset += size;
}

static unsigned calculate_state_size(unsigned max_events) {
    /* cycles + next_event_id + max_events + events. */
    return sizeof(int) + sizeof(unsigned) * 2 + max_events * sizeof(struct SchedulerEvent);
}

unsigned scheduler_state_size(const struct Scheduler* s) {
    return calculate_state_size(s->max_events);
}

int scheduler_save_state(const struct Scheduler* s, void* data, unsigned size) {
    unsigned offset = 0;

    if (size < scheduler_state_size(s)) {
        return -1;
    }

    save_state_write(data, &s->max_events, sizeof(s->max_events), &offset);
    save_state_write(data, &s->cycles, sizeof(s->cycles), &offset);
    save_state_write(data, &s->next_event_id, sizeof(s->next_event_id), &offset);
    save_state_write(data, s->events, sizeof(*s->events) * s->max_events, &offset);

    return 0;
}

int scheduler_load_state(struct Scheduler* s, const void* data, unsigned size) {
    struct Scheduler s_new;
    unsigned max_events;
    unsigned offset = 0;

    if (!s || !data || size < sizeof(unsigned)) {
        return -1;
    }

    save_state_read(&max_events, data, sizeof(max_events), &offset);

    if (!max_events || size < calculate_state_size(max_events)) {
        return -1;
    }

    if (scheduler_init(&s_new, max_events - 1)) {
        return -1;
    }

    scheduler_quit(s);
    *s = s_new;

    save_state_read(&s->cycles, data, sizeof(s->cycles), &offset);
    save_state_read(&s->next_event_id, data, sizeof(s->next_event_id), &offset);
    save_state_read(s->events, data, sizeof(*s->events) * s->max_events, &offset);

    return 0;
}
