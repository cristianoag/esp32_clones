#ifndef FMSX_ANIMATION_GIF_H
#define FMSX_ANIMATION_GIF_H

#include <cassert>
#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <vector>

// Test artifact only. Literal LZW blocks avoid an external image dependency;
// frame delays come from the emulator's presented-frame timestamps.
// Sample at <=30 Hz because browsers stretch 10 ms GIF delays to 100 ms.
class AnimationGif {
public:
  void frame(const uint8_t* pixels, unsigned width, unsigned height,
             const uint32_t* colors, unsigned timestamp, bool final)
  {
    if (images.empty()) {
      w = width; h = height;
      palette.assign(colors, colors + 256);
    }
    if (!images.empty() &&
        std::equal(images.back().begin(), images.back().end(), pixels)) return;
    if (!images.empty() && timestamp - times.back() < 2) {
      if (final) images.back().assign(pixels, pixels + width * height);
      return;
    }
    images.emplace_back(pixels, pixels + width * height);
    times.push_back(timestamp);
  }

  void save(const char* path) const
  {
    if (images.empty()) return;
    FILE* f = fopen(path, "wb");
    assert(f);
    fwrite("GIF89a", 1, 6, f);
    word(f, w); word(f, h);
    fputc(0xf7, f); fputc(0, f); fputc(0, f);
    for (uint32_t color : palette) {
      fputc(color >> 16, f); fputc(color >> 8, f); fputc(color, f);
    }
    const uint8_t loop[] = {0x21,0xff,11,'N','E','T','S','C','A','P','E','2','.','0',
                            3,1,0,0,0};
    fwrite(loop, 1, sizeof(loop), f);
    for (unsigned i = 0; i < images.size(); ++i) {
      // Preserve cumulative 60 Hz timing despite GIF's centisecond resolution.
      const unsigned start = (times[i] - times[0]) * 100 / 60;
      const unsigned end = i + 1 < times.size()
          ? (times[i + 1] - times[0]) * 100 / 60 : start + 200;
      const uint8_t control[] = {0x21,0xf9,4,0};
      fwrite(control, 1, sizeof(control), f);
      word(f, end - start);
      fputc(0, f); fputc(0, f);
      fputc(0x2c, f);
      word(f, 0); word(f, 0); word(f, w); word(f, h);
      fputc(0, f); fputc(8, f);

      std::vector<uint8_t> data;
      unsigned bits = 0, count = 0;
      auto code = [&](unsigned value) {
        bits |= value << count;
        count += 9;
        while (count >= 8) {
          data.push_back(bits & 255);
          bits >>= 8;
          count -= 8;
        }
      };
      for (unsigned p = 0; p < images[i].size(); ++p) {
        if (!(p & 127)) code(256); // Reset before the dictionary grows to 10 bits.
        code(images[i][p]);
      }
      code(257);
      if (count) data.push_back(bits & 255);
      for (unsigned p = 0; p < data.size();) {
        const unsigned size = std::min<unsigned>(255, data.size() - p);
        fputc(size, f);
        fwrite(data.data() + p, 1, size, f);
        p += size;
      }
      fputc(0, f);
    }
    fputc(0x3b, f);
    assert(!ferror(f));
    assert(fclose(f) == 0);
  }

private:
  static void word(FILE* f, unsigned value)
  {
    fputc(value & 255, f);
    fputc((value >> 8) & 255, f);
  }
  unsigned w = 0, h = 0;
  std::vector<uint32_t> palette;
  std::vector<std::vector<uint8_t>> images;
  std::vector<unsigned> times;
};

#endif
