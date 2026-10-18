#pragma once
#include <stdint.h>

bool MacAudioStart();
void MacAudioEnable(bool enabled);
void MacAudioSubmit(const int16_t *samples, unsigned count);
