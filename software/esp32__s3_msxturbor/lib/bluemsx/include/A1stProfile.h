#pragma once
#include "A1stCore.h"

#ifdef __cplusplus
extern "C" {
#endif
enum { A1ST_WORK_VIDEO, A1ST_WORK_AUDIO, A1ST_WORK_COUNT };
extern bool a1stProfileActive;
uint64_t a1stProfileNow(void);
void a1stProfileReset(void);
void a1stProfileStart(uint64_t now);
void a1stProfileFinish(uint64_t now);
void a1stProfileBegin(unsigned section, uint64_t now);
void a1stProfileEnd(unsigned section, uint64_t now);
void a1stProfileTake(A1stWorkStats* stats);
#ifdef __cplusplus
}
#endif

#define A1ST_WORK_BEGIN(section) do { if (a1stProfileActive) a1stProfileBegin(section, a1stProfileNow()); } while (0)
#define A1ST_WORK_END(section) do { if (a1stProfileActive) a1stProfileEnd(section, a1stProfileNow()); } while (0)
