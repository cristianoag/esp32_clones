#pragma once
#include <stdint.h>

bool SmsAudioStart();
void SmsAudioEnable(bool enabled);
void SmsAudioSubmit(const int16_t *samples, unsigned count);
