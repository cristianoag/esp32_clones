#include "MsxVideo.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

static unsigned errors;
static void msxReportError(const char* message) { assert(message&&*message); ++errors; }
struct Dma
{
    uint8_t rows[MsxVideoHeight][MsxVideoWidth+2];
    unsigned flushes;
    uint8_t* getLineAddr8(int y) { assert(y>=0&&y<MsxVideoHeight); return rows[y]+1; }
    void flush(int buffer,int y) { assert(buffer==0&&y==int(flushes)); ++flushes; }
} dma;
struct Video
{
    int bufferCount=1;
    Dma* dmaBuffer=&dma;
} video;

#include "VideoUnderTest.inc"

int main()
{
    uint32_t palette[256];
    for(unsigned i=0;i<256;++i) palette[i]=(i*0x1234567U)&0xFFFFFF;
    for(int stride : {256,272,300})
    {
        std::vector<uint8_t> frame(stride*240+2,0xED);
        for(int y=0;y<240;++y)
            for(int x=0;x<256;++x) frame[1+y*stride+x]=(x+y*11)&255;
        const auto original=frame;
        memset(dma.rows,0xDA,sizeof(dma.rows));dma.flushes=0;errors=0;
        msxPresent(frame.data()+1,256,240,palette,stride);
        assert(!errors&&dma.flushes==240&&original==frame);
        for(int y=0;y<240;++y)
        {
            assert(dma.rows[y][0]==0xDA&&dma.rows[y][321]==0xDA);
            for(int x=0;x<320;++x)
            {
                const uint32_t rgb=palette[frame[1+y*stride+x*256/320]];
                const uint8_t expected=((rgb>>21)&7)|((rgb>>10)&0x38)|(rgb&0xC0);
                assert(dma.rows[y][x+1]==expected);
            }
        }
    }
    std::vector<uint8_t> snapshot(reinterpret_cast<uint8_t*>(dma.rows),
                                  reinterpret_cast<uint8_t*>(dma.rows)+sizeof(dma.rows));
    dma.flushes=0;
    uint8_t dummy=0;
    msxPresent(nullptr,256,240,palette,272);
    msxPresent(&dummy,256,240,nullptr,272);
    msxPresent(&dummy,512,240,palette,512);
    msxPresent(&dummy,256,239,palette,272);
    msxPresent(&dummy,256,240,palette,255);
    video.bufferCount=2;
    msxPresent(&dummy,256,240,palette,272);
    assert(errors==6&&!dma.flushes);
    assert(!memcmp(snapshot.data(),dma.rows,sizeof(dma.rows)));
    puts("PASS: production strided VGA blit, exact 5:4 pixels/colors, per-row flush, guards and invalid inputs.");
}
