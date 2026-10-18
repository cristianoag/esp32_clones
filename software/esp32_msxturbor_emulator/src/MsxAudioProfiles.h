#pragma once
#include <stddef.h>

enum MsxAudioProfile {
  MsxAudioAuto = 0,
  MsxAudioPsg = 1,
  MsxAudioPsgScc = 2,
  MsxAudioPsgFm = 3,
  MsxAudioPsgSccFm = 4,
  MsxAudioProfileCount = 5
};

bool MsxAudioProfileValid(unsigned profile);
const char* MsxAudioProfileName(unsigned profile);

// A successful optional-FM resolution returns an empty fmPath when no ROM exists.
// An existing but invalid/unreadable preferred ROM is an error, not a fallback.
bool MsxResolveAudioProfile(unsigned profile, const char* biosDirectory,
                           char* fmPath, size_t fmPathSize,
                           char* error, size_t errorSize,
                           const char* sharedFmPath = "/sdcard/msx/audio/FMPAC.ROM");
