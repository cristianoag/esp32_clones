// Include the port so the production scanline buffer can be inspected directly.
#include "../../../src/MsxCore.cpp"
#include "../../../src/MsxVideo.h"
#include <cassert>
#include <cstdlib>
#include <initializer_list>

extern "C" void* heap_caps_malloc(size_t size, unsigned) { return malloc(size); }
extern "C" void heap_caps_free(void* p) { free(p); }
uint16_t msxPollJoysticks() { return 0; }
void msxSubmitAudio(const int16_t*, unsigned) {}
void msxReportError(const char*) { assert(false); }
bool msxShouldExit() { return false; }
void msxPollKeyboard(uint8_t*) {}
static int presentedWidth;
void msxPresent(const uint8_t*, int width, int height, const uint32_t*)
{
  presentedWidth=width;
  assert(height==240);
}
extern "C" byte DebugZ80(Z80*) { return 1; }

static byte video[0x20000], attributes[0x280], patterns[2048];
static pixel framebuffer[WIDTH * HEIGHT];

static void setup()
{
  memset(VDP, 0, sizeof(VDP));
  memset(VDPStatus, 0, sizeof(VDPStatus));
  memset(&CPU, 0, sizeof(CPU));
  memset(attributes, 0, sizeof(attributes));
  memset(patterns, 0, sizeof(patterns));
  memset(video, 0, sizeof(video));
  Mode = MSX_MSX2P;
  ScrMode = 6;
  VDP[1] = 0x40;
  VRAM = video;
  SprTab = attributes + 0x200;
  SprGen = patterns;
  ChrTab = video;
  ChrTabM = 0x1FFFF;
  XBuf = framebuffer;
  for (unsigned i = 0; i < 80; ++i) XPal[i] = i;
  XPal0 = BGColor = 0;
  UPeriod = 0;
  // Two adjacent 8x8 sprites become overlapping when magnified.
  memset(attributes, 5, 0x200);
  memset(patterns, 0xff, 8);
  SprTab[0] = SprTab[4] = 10;
  SprTab[1] = 20;
  SprTab[5] = 28;
  SprTab[8] = 216;
}

static void beginLine(int y = 11)
{
  ScanLine = y - 1;
  VDPStatus[2] |= 0x20;
  CPU.ICount = 0;
  LoopZ80(&CPU);
  CPU.ICount = CPU.IPeriod;
}

static byte statusAt(int cycles)
{
  CPU.ICount = CPU.IPeriod - cycles;
  VDP[15] = 0;
  return InZ80(0x99);
}

static void collisions()
{
  setup();
  beginLine();
  assert(!(statusAt(CPU_H256) & 0x20));
  VDP[1] |= 1;
  beginLine();
  assert(!(statusAt(18) & 0x20)); // Beam is before x=28.
  assert(statusAt(20) & 0x20);
  assert(VDPStatus[3] == 28 + 12 && VDPStatus[5] == 11 + 8);
  assert(!(statusAt(20) & 0x20)); // Same pixels must never collide twice.
  assert(statusAt(21) & 0x20); // Later overlapping pixels may relatch.
  assert(statusAt(30) & 0x20);
  assert(!(statusAt(31) & 0x20)); // Beam has passed the complete overlap.
  assert(!(statusAt(CPU_H256) & 0x20));
  beginLine(12);
  assert(statusAt(20) & 0x20); // A later scanline is a new event.

  beginLine();
  // Preserve the real counter when EI delays interrupts for one instruction.
  CPU.IFF = IFF_EI;
  CPU.IBackup = CPU.IPeriod - 18;
  CPU.ICount = 1;
  assert(!(InZ80(0x99) & 0x20));
  CPU.IFF = 0;
  assert(statusAt(20) & 0x20);

  // Without a status read during active display, HBlank still latches it.
  beginLine();
  CPU.ICount = 0;
  LoopZ80(&CPU);
  assert(InZ80(0x99) & 0x20);
  assert(!(InZ80(0x99) & 0x20));
  assert(!UPeriod); // No render dependency.

  VDP[8] = 2;
  beginLine();
  assert(!(statusAt(CPU_H256) & 0x20));
  VDP[8] = 0;
  VDP[1] &= ~0x40;
  beginLine();
  assert(!(statusAt(CPU_H256) & 0x20));
  VDP[1] |= 0x40;
  for (byte color : {byte(0), byte(0x25), byte(0x45)}) {
    attributes[16] = color; // Transparent, IC and CC do not collide.
    beginLine();
    assert(!(statusAt(CPU_H256) & 0x20));
  }
  attributes[16] = 0;
  VDP[8] = 0x20;
  beginLine();
  assert(statusAt(20) & 0x20);

  // Early clock clips at the left edge; it must not wrap to x=255.
  memset(attributes, 0x85, 0x200);
  SprTab[1] = SprTab[5] = 31;
  beginLine();
  assert(statusAt(1) & 0x20);
  assert(VDPStatus[3] == 12);
  SprTab[1] = SprTab[5] = 0;
  beginLine();
  assert(!(statusAt(CPU_H256) & 0x20));

  // Magnified sprites crossing the top wrap at 256, not at native height.
  memset(attributes, 5, 0x200);
  SprTab[0] = SprTab[4] = 240;
  SprTab[1] = SprTab[5] = 250;
  VDP[1] |= 2;
  memset(patterns, 0xff, sizeof(patterns));
  beginLine(1);
  assert(statusAt(CPU_H256) & 0x20);
  assert(VDPStatus[3] == 6 && (VDPStatus[4] & 1));

  setup();
  VDP[1] |= 1;
  VDP[23] = 3;
  beginLine(8);
  assert(statusAt(20) & 0x20);
  beginLine(192);
  assert(!(statusAt(CPU_H256) & 0x20)); // Vertical border is not displayed.

  // Invisible sprites still occupy one of the eight evaluation slots.
  setup();
  memset(SprTab, 0, 128);
  for (unsigned i = 0; i < 9; ++i) {
    SprTab[i * 4] = 10;
    SprTab[i * 4 + 1] = i == 8 ? 0 : i * 16;
  }
  SprTab[36] = 216;
  beginLine();
  assert(!(statusAt(CPU_H256) & 0x20)); // Ninth sprite cannot collide.
  SprTab[7 * 4 + 1] = 0;
  beginLine();
  assert(statusAt(1) & 0x20);

  setup();
  Mode = MSX_MSX1;
  ScrMode = 2;
  SprTab[3] = SprTab[7] = 5;
  SprTab[8] = 208;
  VDP[1] |= 1;
  beginLine();
  assert(statusAt(20) & 0x20);
  assert(!(statusAt(20) & 0x20));
}

static pixel* render()
{
  RefreshLine6(0);
  return framebuffer + FirstLine * WIDTH + (WIDTH-256)/2;
}

static void text80()
{
  setup();
  ScrMode=MAXSCREEN+1;
  ChrTab=video+0x1000;
  ColTab=video+0x3000;
  ChrGen=patterns;
  FontBuf=nullptr;
  ChrTabM=ColTabM=0x1FFFF;
  FGColor=15;BGColor=1;XFGColor=9;XBGColor=2;
  for(unsigned i=0;i<2160;++i) ChrTab[i]=i%64;
  for(unsigned glyph=0;glyph<64;++glyph)
    for(unsigned row=0;row<8;++row) patterns[glyph*8+row]=((glyph+row)&63)<<2;
  memset(ColTab,0xAA,270);
  for(int horizontal=-7;horizontal<=8;++horizontal)
    for(int vertical=-7;vertical<=8;++vertical)
    {
      VDP[18]=((-vertical&15)<<4)|(-horizontal&15);
      VScroll=3;
      VDP[9]=0x80;
      memset(framebuffer,0xFE,sizeof(framebuffer));
      RefreshLineTx80(0);
      assert(FirstLine==8+vertical);
      for(unsigned y : {0U,7U,8U,191U,211U})
      {
        RefreshLineTx80(y);
        pixel* row=framebuffer+(FirstLine+y)*WIDTH+(WIDTH-256)/2+horizontal;
        for(unsigned x=0;x<9;++x) assert(row[x]==1);
        for(unsigned column=0;column<80;++column)
        {
          const byte glyph=ChrTab[(y/8)*80+column];
          const byte dots=patterns[glyph*8+((y+VScroll)&7)];
          for(unsigned dot=0;dot<3;++dot)
            assert(row[9+column*3+dot]==
                   (dots&(0xC0>>(dot*2))? (column%2?15:9):(column%2?1:2)));
        }
        for(unsigned x=249;x<256;++x) assert(row[x]==1);
      }
      // Blanking must cover the entire narrow text line.
      VDP[1]=0;
      RefreshLineTx80(0);
      const pixel* row=framebuffer+FirstLine*WIDTH+(WIDTH-256)/2+horizontal;
      for(unsigned x=0;x<256;++x) assert(row[x]==1);
      VDP[1]=0x40;
    }
  static uint8_t packed[256*HEIGHT];
  output=packed;
  PutImage();
  assert(presentedWidth==256);
  ScrMode=1;
  PutImage();
  assert(presentedWidth==256);
  output=nullptr;
}

static void scaleVideo()
{
  static_assert(MsxVideoWidth==320 && MsxVideoHeight==240 && MsxVideoLineRepeat==2, "Keep low-bandwidth VGA timing.");
  static_assert(WIDTH==272 && HEIGHT==240, "Keep the lightweight core framebuffer.");
  static_assert(MsxMenuPixelScaleX*320==MsxVideoWidth, "F12 logical coordinates must fill VGA.");
  uint16_t columns[MsxVideoWidth];
  uint8_t colors[256], source[256], destination[MsxVideoWidth+2];
  for(unsigned x=0;x<256;++x) colors[x]=255-x;
  for(unsigned x=0;x<256;++x) source[x]=x;
  for(unsigned width : {256U})
  {
    memset(destination,0xAC,sizeof(destination));
    MsxVideoColumns(width,columns);
    MsxScaleVideoRow(destination+1,source,colors,columns);
    assert(destination[0]==0xAC && destination[MsxVideoWidth+1]==0xAC);
    unsigned previous=0;
    for(unsigned x=0;x<MsxVideoWidth;++x)
    {
      assert(columns[x]==x*width/MsxVideoWidth);
      assert(columns[x]>=previous && columns[x]<=previous+1);
      assert(destination[x+1]==colors[source[columns[x]]]);
      previous=columns[x];
    }
    assert(previous==width-1); // No source dot is lost, and no side bars remain.
  }
}
static void scroll()
{
  setup();
  VDP[8] = 2;
  // Four distinct colors, including odd native pixels which are not sampled.
  for (unsigned x = 0; x < 256; x += 2)
    video[x / 2] = (((x / 5) & 3) << 6) | (3 << 4) |
                   ((((x + 1) / 5) & 3) << 2) | 3;
  pixel* p = render();
  for (unsigned x = 0; x < 256; ++x) assert(p[x] == ((x / 5) & 3));
  for (unsigned coarse = 0; coarse < 64; ++coarse)
    for (unsigned fine = 0; fine < 8; ++fine) {
      VDP[26] = coarse;
      VDP[27] = fine;
      p = render();
      for (unsigned x = 0; x < 256; ++x)
        assert(p[x] == (x < fine ? 0 : (((x + coarse * 8 - fine) & 255) / 5) & 3));
    }
  VDP[25] = 2;
  p = render();
  for (unsigned x = 0; x < 8; ++x) assert(!p[x]);

  // Multi-page wrap joins the 32 KiB pages, respecting R#2's address mask.
  memset(video, 0x55, 0x8000);
  memset(video + 0x8000, 0xaa, 0x8000);
  VDP[25] = 1;
  VDP[26] = 31;
  VDP[27] = 0;
  ChrTab = video + 0x8000;
  p = render();
  assert(p[0] == 1 && p[7] == 1 && p[8] == 2 && p[255] == 2);
  VDP[26] = 63;
  p = render();
  assert(p[0] == 2 && p[7] == 2 && p[8] == 1 && p[255] == 1);
  ChrTabM &= ~0x8000;
  p = render();
  assert(p[0] == 1 && p[255] == 1);

  // The V9938 ignores V9958-only horizontal scrolling registers.
  Mode = MSX_MSX2;
  ChrTab = video;
  p = render();
  assert(p[0] == 1 && p[255] == 1);

  // SCREEN 6 sprite color encodes two 2-bit pixels, not a 16-color index.
  Mode = MSX_MSX2P;
  VDP[8] = VDP[25] = VDP[26] = VDP[27] = 0;
  SprTab[0] = 255;
  SprTab[4] = 216;
  SprTab[1] = 20;
  attributes[0] = 9; // Left pixel color 2, right pixel color 1.
  p = render();
  assert(p[20] == 2 && p[19] == 1);
  VDP[26] = 2;
  p = render();
  assert(p[20] == 2 && p[19] == 1); // Sprites stay fixed during scrolling.
}

int main()
{
  collisions();
  scroll();
  text80();
  scaleVideo();
  puts("PASS: VDP animation, narrow TEXT80 colors/scroll/adjust/blanking, full-width 320x240 scaling.");
}
