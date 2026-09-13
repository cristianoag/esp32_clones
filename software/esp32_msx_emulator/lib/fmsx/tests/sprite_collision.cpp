#include "../fMSX/SpriteCollision.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>

struct Sprite { unsigned pattern; int x,zoom; };
static Sprite scenes[1024][8];
static volatile unsigned checksum;

static void reference(const Sprite *sprites,unsigned char occupied[256],unsigned char collisions[256])
{
  memset(occupied,0,256);
  memset(collisions,0,256);
  for(unsigned s=0;s<8;++s)
    for(unsigned dot=0;dot<16;++dot)
      if(sprites[s].pattern&(0x8000>>dot))
        for(int copy=0;copy<sprites[s].zoom;++copy)
        {
          const int x=sprites[s].x+dot*sprites[s].zoom+copy;
          if(x>=0&&x<256)
          {
            collisions[x]|=occupied[x];
            occupied[x]=1;
          }
        }
}

static unsigned oldPacked(const Sprite *sprites)
{
  unsigned char occupied[32]={},collisions[32]={};
  for(unsigned s=0;s<8;++s)
    for(unsigned dot=0;dot<16;++dot)
      if(sprites[s].pattern&(0x8000>>dot))
        for(int copy=0;copy<sprites[s].zoom;++copy)
        {
          const int x=sprites[s].x+dot*sprites[s].zoom+copy;
          if(x>=0&&x<256)
          {
            const unsigned char mask=1<<(x&7);
            collisions[x>>3]|=occupied[x>>3]&mask;
            occupied[x>>3]|=mask;
          }
        }
  for(unsigned x=0;x<256;++x) if(collisions[x>>3]&(1<<(x&7))) return x+1;
  return 0;
}

static unsigned optimized(const Sprite *sprites)
{
  uint32_t occupied[8]={},collisions[8]={};
  for(unsigned s=0;s<8;++s)
    AddSpriteDots(occupied,collisions,sprites[s].x,SpriteDots(sprites[s].pattern,sprites[s].zoom));
  return FirstSpriteCollision(collisions,0,256)+1;
}

int main()
{
  unsigned seed=0x12345678;
  for(auto &scene:scenes)
    for(auto &sprite:scene)
    {
      seed=seed*1664525+1013904223;
      sprite.pattern=seed>>16;
      sprite.zoom=1+((seed>>9)&1);
      sprite.x=int(seed%288)-32;
    }
  for(const auto &scene:scenes)
  {
    unsigned char occupied[256],collisions[256];
    uint32_t packedOccupied[8]={},packedCollisions[8]={};
    reference(scene,occupied,collisions);
    for(const auto &sprite:scene)
      AddSpriteDots(packedOccupied,packedCollisions,sprite.x,SpriteDots(sprite.pattern,sprite.zoom));
    for(unsigned x=0;x<256;++x)
    {
      assert(((packedOccupied[x>>5]>>(x&31))&1)==occupied[x]);
      assert(((packedCollisions[x>>5]>>(x&31))&1)==collisions[x]);
    }
    for(int start=0;start<=256;++start)
      for(int end=start;end<=256;++end)
      {
        int first=start;
        while(first<end&&!collisions[first]) ++first;
        assert(FirstSpriteCollision(packedCollisions,start,end)==(first<end?first:-1));
      }
  }
  // Include every possible 16-dot pattern and both zoom factors.
  for(unsigned pattern=0;pattern<65536;++pattern)
    for(int zoom=1;zoom<=2;++zoom)
    {
      const uint32_t bits=SpriteDots(pattern,zoom);
      for(unsigned x=0;x<16U*zoom;++x)
        assert(((bits>>x)&1)==!!(pattern&(0x8000>>(x/zoom))));
    }
  unsigned expected=0,actual=0;
  const auto start=std::chrono::steady_clock::now();
  for(unsigned i=0;i<250000;++i) expected+=oldPacked(scenes[i%1024]);
  const auto middle=std::chrono::steady_clock::now();
  for(unsigned i=0;i<250000;++i) actual+=optimized(scenes[i%1024]);
  const auto end=std::chrono::steady_clock::now();
  checksum=actual;
  assert(actual==expected);
  const double before=std::chrono::duration<double>(middle-start).count();
  const double after=std::chrono::duration<double>(end-middle).count();
  printf("PASS: collision masks/cursors match scalar reference; all patterns, zoom, clipping and beam intervals.\n");
  printf("Native collision-only benchmark: scalar %.3fs, bitset %.3fs, %.2fx (not ESP32 FPS).\n",before,after,before/after);
}
