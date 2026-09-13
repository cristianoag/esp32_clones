#ifndef FMSX_SPRITE_COLLISION_H
#define FMSX_SPRITE_COLLISION_H
#include <stdint.h>

/* Bit zero represents the leftmost dot of a sprite or screen word. */
static inline uint32_t SpriteDots(unsigned int pattern,int zoom)
{
  uint32_t dots=pattern;
  dots=((dots&0x5555)<<1)|((dots>>1)&0x5555);
  dots=((dots&0x3333)<<2)|((dots>>2)&0x3333);
  dots=((dots&0x0F0F)<<4)|((dots>>4)&0x0F0F);
  dots=((dots&0x00FF)<<8)|((dots>>8)&0x00FF);
  if(zoom==2)
  {
    dots=(dots|(dots<<8))&0x00FF00FF;
    dots=(dots|(dots<<4))&0x0F0F0F0F;
    dots=(dots|(dots<<2))&0x33333333;
    dots=(dots|(dots<<1))&0x55555555;
    dots|=dots<<1;
  }
  return(dots);
}

static inline int AddSpriteDots(uint32_t occupied[8],uint32_t collisions[8],int x,uint32_t dots)
{
  unsigned int word,shift;
  uint32_t mask,overlap;
  int collided;
  if(x<0)
  {
    if(x<=-32) return(0);
    dots>>=-x;
    x=0;
  }
  if(!dots||x>=256) return(0);
  word=(unsigned int)x>>5;
  shift=x&31;
  mask=dots<<shift;
  overlap=occupied[word]&mask;
  collisions[word]|=overlap;
  occupied[word]|=mask;
  collided=overlap!=0;
  if(shift&&word<7)
  {
    mask=dots>>(32-shift);
    overlap=occupied[word+1]&mask;
    collisions[word+1]|=overlap;
    occupied[word+1]|=mask;
    collided|=overlap!=0;
  }
  return(collided);
}

static inline int FirstSpriteCollision(const uint32_t collisions[8],int start,int end)
{
  unsigned int word=(unsigned int)start>>5;
  if(start>=end) return(-1);
  while(word<8&&(int)(word*32)<end)
  {
    uint32_t dots=collisions[word];
    if((int)(word*32)<start) dots&=~UINT32_C(0)<<(start&31);
    if(end<(int)(word*32+32)) dots&=(UINT32_C(1)<<(end&31))-1;
    if(dots) return(word*32+__builtin_ctz(dots));
    ++word;
  }
  return(-1);
}
#endif
