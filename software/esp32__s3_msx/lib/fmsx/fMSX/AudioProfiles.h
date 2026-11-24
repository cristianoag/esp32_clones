#ifndef FMSX_AUDIO_PROFILES_H
#define FMSX_AUDIO_PROFILES_H

#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Values match MsxAudioProfile. Configure before StartMSX; path is borrowed
 * through TrashMSX. Auto + NULL means PSG/SCC, without an FM BIOS. */
int FmsxSetAudioProfile(int profile, const char *fmRomPath);

/* Both supported BIOS formats identify MSX-MUSIC at address 4018h. */
static inline int FmsxAudioRomKind(const unsigned char *data, size_t size)
{
  if(!data || (size != 0x4000 && size != 0x10000)) return 0;
  if(data[0] != 'A' || data[1] != 'B') return 0;
  if(memcmp(data + 0x18, "APRLOPLL", 8) &&
     memcmp(data + 0x18, "PAC2OPLL", 8)) return 0;
  return size == 0x10000 ? 2 : 1;
}

#ifdef __cplusplus
}
#endif
#endif
