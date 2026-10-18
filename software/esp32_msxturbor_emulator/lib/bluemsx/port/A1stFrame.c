#include "A1stInternal.h"
#include "FrameBuffer.h"
#include <string.h>

static Pixel* pixels;
static Pixel* allocation;
static unsigned widths[A1ST_FRAME_HEIGHT];
static int scanline;
static int lineCount;

FrameBufferData* frameBufferDataCreate(int width, int height, int zoom)
{
    unsigned y;
    (void)width; (void)height; (void)zoom;
    allocation = calloc(A1ST_FRAME_WIDTH * A1ST_FRAME_HEIGHT + 32, sizeof(Pixel));
    pixels = allocation + 16;
    for (y = 0; y < 16; ++y) {
        allocation[y] = 0x5aa5;
        pixels[A1ST_FRAME_WIDTH * A1ST_FRAME_HEIGHT + y] = 0x5aa5;
    }
    for (y = 0; y < A1ST_FRAME_HEIGHT; ++y) widths[y] = 272;
    scanline = 0;
    lineCount = A1ST_FRAME_HEIGHT;
    return (FrameBufferData*)pixels;
}
void frameBufferDataDestroy(FrameBufferData* data)
{
    (void)data;
    a1stCheckFrameBounds();
    free(allocation);
    allocation = pixels = NULL;
}
void a1stCheckFrameBounds(void)
{
    unsigned i;
    if (!allocation) return;
    for (i = 0; i < 16; ++i)
        if (allocation[i] != 0x5aa5 ||
            pixels[A1ST_FRAME_WIDTH * A1ST_FRAME_HEIGHT + i] != 0x5aa5)
            a1stPanic("VDP framebuffer boundary was overwritten.");
}
FrameBuffer* frameBufferGetDrawFrame(void) { return pixels; }
FrameBuffer* frameBufferFlipDrawFrame(void) { return pixels; }
FrameBufferData* frameBufferGetActive(void) { return (FrameBufferData*)pixels; }
Pixel* frameBufferGetLine(FrameBuffer* frame, int y)
{
    (void)frame;
    if (y < 0 || y >= A1ST_FRAME_HEIGHT) a1stPanic("VDP scanline outside framebuffer.");
    return pixels + y * A1ST_FRAME_WIDTH;
}
int frameBufferGetDoubleWidth(FrameBuffer* frame, int y) { (void)frame; return widths[y] == 544; }
void frameBufferSetDoubleWidth(FrameBuffer* frame, int y, int doubled)
{
    (void)frame;
    if (y < 0 || y >= A1ST_FRAME_HEIGHT) a1stPanic("Invalid VDP line width.");
    widths[y] = doubled ? 544 : 272;
}
void frameBufferSetLineCount(FrameBuffer* frame, int count) { (void)frame; lineCount = count; }
int frameBufferGetLineCount(FrameBuffer* frame) { (void)frame; return lineCount; }
int frameBufferGetMaxWidth(FrameBuffer* frame) { (void)frame; return A1ST_FRAME_WIDTH; }
void frameBufferSetScanline(int line) { scanline = line; }
int frameBufferGetScanline(void) { return scanline; }
void frameBufferSetActive(FrameBufferData* frame) { (void)frame; }
void frameBufferSetMixMode(FrameBufferMixMode mode, FrameBufferMixMode mask) { (void)mode; (void)mask; }
void frameBufferSetInterlace(FrameBuffer* frame, int mode) { (void)frame; (void)mode; }
void frameBufferClearDeinterlace(void) {}
const uint16_t* a1stPixels(void) { return pixels; }
unsigned a1stLineWidth(unsigned line) { return line < A1ST_FRAME_HEIGHT ? widths[line] : 0; }
