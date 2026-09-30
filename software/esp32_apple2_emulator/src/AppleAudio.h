#pragma once
#include <stdint.h>

bool AppleAudioStart();
void AppleAudioEnable(bool enabled);
void AppleAudioSubmit(const int16_t *samples, unsigned count);
