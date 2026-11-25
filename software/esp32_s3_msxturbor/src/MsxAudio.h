#pragma once
#include <stdint.h>

bool MsxAudioStart();
void MsxAudioEnable(bool enabled);
void MsxAudioSubmit(const int16_t *samples, unsigned count);

struct MsxAudioStats
{
    unsigned rate;
    unsigned queued;
    uint32_t underruns;
    uint32_t inputSamples, clippedSamples, playedSamples, lateTicks, maxGapUs, pwmClippedSamples;
    int inputMin, inputMax, inputMean;
    unsigned inputRms;
    unsigned gain;
};
MsxAudioStats MsxAudioGetStats(bool resetDiagnostics = false);
void MsxAudioSetDiagnostics(bool enabled);
bool MsxAudioSetGain(unsigned gain);
bool MsxAudioPlayTestTone();
