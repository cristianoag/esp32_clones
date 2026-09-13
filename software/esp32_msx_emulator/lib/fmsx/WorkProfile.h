#ifndef FMSX_WORK_PROFILE_H
#define FMSX_WORK_PROFILE_H
#include <stdint.h>

enum FmsxWorkSection
{
  FmsxWorkVdp, FmsxWorkDraw, FmsxWorkBlit, FmsxWorkSprites,
  FmsxWorkAudio, FmsxWorkInput, FmsxWorkCount
};

#ifdef ESP_PLATFORM
#ifdef __cplusplus
extern "C" {
#endif
extern unsigned char fmsxProfileActive;
void fmsxProfileBegin(unsigned section);
void fmsxProfileEnd(unsigned section);
#ifdef __cplusplus
}
#endif
#define FMSX_WORK_BEGIN(section) do { if(fmsxProfileActive) fmsxProfileBegin(section); } while(0)
#define FMSX_WORK_END(section) do { if(fmsxProfileActive) fmsxProfileEnd(section); } while(0)
#else
#define FMSX_WORK_BEGIN(section) ((void)0)
#define FMSX_WORK_END(section) ((void)0)
#endif

#ifdef __cplusplus
class FmsxWorkProfile
{
public:
  void reset() { *this=FmsxWorkProfile(); }
  bool startFrame(int64_t now)
  {
    // Avoid locking sampling to the common 2/5/10-frame rendering intervals.
    active=(++sequence%17)==0;
    start=now;
    for(unsigned i=0;i<FmsxWorkCount;++i) current[i]=0;
    return active;
  }
  void begin(unsigned section,int64_t now) { starts[section]=now; }
  void end(unsigned section,int64_t now) { current[section]+=now-starts[section]; }
  void finishFrame(int64_t now)
  {
    if(!active) return;
    total+=now-start;
    for(unsigned i=0;i<FmsxWorkCount;++i) times[i]+=current[i];
    ++samples;
    active=false;
  }
  void clearWindow()
  {
    total=0;samples=0;
    for(unsigned i=0;i<FmsxWorkCount;++i) times[i]=0;
  }
  int64_t cpuOther() const
  {
    int64_t remainder=total;
    for(unsigned i=0;i<FmsxWorkCount;++i) remainder-=times[i];
    return remainder>0?remainder:0;
  }
  unsigned samples=0;
  int64_t total=0,times[FmsxWorkCount]={};
private:
  bool active=false;
  unsigned sequence=0;
  int64_t start=0,starts[FmsxWorkCount]={},current[FmsxWorkCount]={};
};
#endif
#endif
