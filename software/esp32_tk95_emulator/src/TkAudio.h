#pragma once
#include <stdint.h>

bool TkAudioStart();
void TkAudioEnable(bool enabled);
void TkAudioSubmit(const int16_t *samples, unsigned count);
