#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static inline bool macIsBootMouseInterface(const uint8_t *descriptor, size_t length, unsigned port)
{
    return descriptor && length >= 9 && descriptor[0] >= 9 &&
           descriptor[1] == 4 && descriptor[3] == 0 &&
           descriptor[5] == 3 && descriptor[6] == 1 &&
           descriptor[7] == 2 && port == 1;
}
