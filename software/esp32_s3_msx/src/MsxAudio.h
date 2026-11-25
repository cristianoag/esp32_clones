#pragma once
#include <stdint.h>

bool MsxAudioStart();
void MsxAudioEnable(bool enabled);
void MsxAudioSubmit(const int16_t *samples, unsigned count);
