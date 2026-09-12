#include "MsxAudioProfiles.h"
#include "../lib/fmsx/fMSX/AudioProfiles.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

bool MsxAudioProfileValid(unsigned profile)
{
  return profile < MsxAudioProfileCount;
}

const char* MsxAudioProfileName(unsigned profile)
{
  static const char* const names[] = {
    "Auto", "PSG", "PSG + SCC", "PSG + FM", "PSG + SCC + FM"
  };
  return MsxAudioProfileValid(profile) ? names[profile] : "Unknown";
}

static bool fail(char* error, size_t size, const char* reason, const char* path)
{
  if(error && size) snprintf(error, size, "%s: %s", reason, path ? path : "");
  return false;
}

static int inspect(const char* path, char* error, size_t errorSize)
{
  struct stat info;
  if(stat(path, &info)) {
    if(errno == ENOENT) return 0;
    fail(error, errorSize, "Cannot access FM BIOS", path);
    return -1;
  }
  if(!S_ISREG(info.st_mode) || !(info.st_mode & (S_IRUSR | S_IRGRP | S_IROTH))) {
    fail(error, errorSize, "FM BIOS is not a readable file", path);
    return -1;
  }
  if(info.st_size != 0x4000 && info.st_size != 0x10000) {
    fail(error, errorSize, "FM BIOS must be exactly 16 KiB or 64 KiB", path);
    return -1;
  }
  FILE* file = fopen(path, "rb");
  if(!file) {
    fail(error, errorSize, "Cannot read FM BIOS", path);
    return -1;
  }
  unsigned char header[32];
  size_t total = fread(header, 1, sizeof(header), file);
  unsigned char block[1024];
  size_t count;
  while((count = fread(block, 1, sizeof(block), file)) != 0) total += count;
  const bool readError = ferror(file) != 0;
  const bool closeError = fclose(file) != 0;
  if(readError || closeError || total != static_cast<size_t>(info.st_size)) {
    fail(error, errorSize, "Failed reading complete FM BIOS", path);
    return -1;
  }
  if(!FmsxAudioRomKind(header, total)) {
    fail(error, errorSize, "Use an original 16 KiB MSX-MUSIC or 64 KiB FM-PAC BIOS (AB/OPLL)", path);
    return -1;
  }
  return 1;
}

bool MsxResolveAudioProfile(unsigned profile, const char* biosDirectory,
                           char* fmPath, size_t fmPathSize,
                           char* error, size_t errorSize, const char* sharedFmPath)
{
  if(error && errorSize) error[0] = 0;
  if(fmPath && fmPathSize) fmPath[0] = 0;
  if(!MsxAudioProfileValid(profile))
    return fail(error, errorSize, "Invalid audio profile", "");
  if(!fmPath || !fmPathSize)
    return fail(error, errorSize, "FM BIOS path buffer is missing", "");
  if(profile == MsxAudioPsg || profile == MsxAudioPsgScc) return true;
  if(!biosDirectory || !biosDirectory[0])
    return fail(error, errorSize, "BIOS profile directory is missing", "");
  struct stat directory;
  if(!stat(biosDirectory, &directory)) {
    if(!S_ISDIR(directory.st_mode))
      return fail(error, errorSize, "BIOS profile path is not a directory", biosDirectory);
  } else if(errno != ENOENT) {
    return fail(error, errorSize, "Cannot access BIOS profile directory", biosDirectory);
  }

  const size_t length = strlen(biosDirectory);
  const bool separator = biosDirectory[length - 1] != '/' && biosDirectory[length - 1] != '\\';
  const int written = snprintf(fmPath, fmPathSize, "%s%sFMPAC.ROM", biosDirectory, separator ? "/" : "");
  if(written < 0 || static_cast<size_t>(written) >= fmPathSize) {
    fmPath[0] = 0;
    return fail(error, errorSize, "FM BIOS path is too long", biosDirectory);
  }
  int result = inspect(fmPath, error, errorSize);
  if(result == 1) return true;
  if(result < 0) { fmPath[0] = 0; return false; }

  if(sharedFmPath && sharedFmPath[0]) {
    if(strlen(sharedFmPath) >= fmPathSize) {
      fmPath[0] = 0;
      return fail(error, errorSize, "FM BIOS path is too long", sharedFmPath);
    }
    snprintf(fmPath, fmPathSize, "%s", sharedFmPath);
    result = inspect(fmPath, error, errorSize);
    if(result == 1) return true;
    if(result < 0) { fmPath[0] = 0; return false; }
  }
  fmPath[0] = 0;
  if(profile == MsxAudioAuto) return true;
  if(error && errorSize)
    snprintf(error, errorSize, "FM requires FMPAC.ROM in %s or %s", biosDirectory,
             sharedFmPath && sharedFmPath[0] ? sharedFmPath : "the shared audio directory");
  return false;
}
