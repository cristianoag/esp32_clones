#include "MapperDatabase.h"
#include "SHA1.h"
#include "../fMSX/MSX.h"
#include "../MapperDatabase/romdb.h"

int FmsxMapperFromSha1(const unsigned char digest[20])
{
  const int pico=romdb_lookup(digest);
  switch(pico)
  {
    case 0: return(-1);
    case 3: return(MAP_KONAMI5);
    case 5: return(MAP_ASCII8);
    case 6: return(MAP_ASCII16);
    case 7: return(MAP_KONAMI4);
    default: return(-pico);
  }
}

int FmsxKnownMapper(const unsigned char *data,unsigned int size)
{
  SHA1 state;
  unsigned char digest[20];
  unsigned int i;
  int mapper;
  ResetSHA1(&state);
  if(!InputSHA1(&state,data,size)||!ComputeSHA1(&state)) return(-100);
  for(i=0;i<20;++i)
    digest[i]=(unsigned char)(state.Msg[i/4]>>(24-8*(i%4)));
  mapper=FmsxMapperFromSha1(digest);
  if(mapper!=-1) return(mapper);

  if(size>=24&&data[0]=='A'&&data[1]=='B')
  {
    if(!memcmp(data+16,"ASCII16X",8)) return(-12);
    if(!memcmp(data+16,"ROM_NEO8",8)) return(-8);
    if(!memcmp(data+16,"ROM_NE16",8)) return(-9);
    if(size==524288&&!memcmp(data+0x28000,"Manbow 2",8)) return(-14);
  }
  return(-1);
}

const char *FmsxUnsupportedMapperName(int mapper)
{
  switch(mapper)
  {
    case -8: return("NEO8");
    case -9: return("NEO16");
    case -12: return("ASCII16-X");
    case -14: return("Manbow2");
    case -100: return("SHA-1 calculation failure");
    case -101: return("cannot read mapper override database");
    case -102: return("invalid mapper ID in override database");
    default: return("unknown mapper");
  }
}
