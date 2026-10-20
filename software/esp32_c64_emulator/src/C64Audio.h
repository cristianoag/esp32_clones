#pragma once
#include <stdint.h>

bool C64AudioStart();
void C64AudioEnable(bool enabled);
void C64AudioSubmit(const int16_t *samples, unsigned count);
