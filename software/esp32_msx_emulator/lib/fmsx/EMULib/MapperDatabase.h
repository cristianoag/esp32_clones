#ifndef FMSX_MAPPER_DATABASE_H
#define FMSX_MAPPER_DATABASE_H

#ifdef __cplusplus
extern "C" {
#endif

/* fMSX mapper ID, -1 for unknown, or negative PicoVerse ID for unsupported hardware. */
int FmsxMapperFromSha1(const unsigned char digest[20]);
int FmsxKnownMapper(const unsigned char *data,unsigned int size);
const char *FmsxUnsupportedMapperName(int mapper);

#ifdef __cplusplus
}
#endif
#endif
