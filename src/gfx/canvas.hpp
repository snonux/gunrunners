#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace td
{

using Color = std::uint32_t; // 0xAARRGGBB, matches SDL_PIXELFORMAT_ARGB8888

constexpr Color rgba(int r, int g, int b, int a)
{
  return (Color(a & 255) << 24) | (Color(r & 255) << 16) |
    (Color(g & 255) << 8) | Color(b & 255);
}
constexpr Color rgb(int r, int g, int b) { return rgba(r, g, b, 255); }
constexpr int alphaOf(Color c) { return int((c >> 24) & 255); }
constexpr int redOf(Color c) { return int((c >> 16) & 255); }
constexpr int greenOf(Color c) { return int((c >> 8) & 255); }
constexpr int blueOf(Color c) { return int(c & 255); }

Color lerpColor(Color a, Color b, float t);
Color scaleColor(Color c, float f);
Color withAlpha(Color c, int a);

struct Image
{
  int w = 0;
  int h = 0;
  std::vector<Color> px;

  Image() = default;
  Image(int width, int height, Color fill = 0)
    : w(width), h(height), px(std::size_t(width * height), fill)
  {
  }

  bool empty() const { return w == 0 || h == 0; }
  Color get(int x, int y) const { return px[std::size_t(y * w + x)]; }
  void set(int x, int y, Color c)
  {
    if (x >= 0 && y >= 0 && x < w && y < h)
      px[std::size_t(y * w + x)] = c;
  }
  void blend(int x, int y, Color c);
  void fill(int x, int y, int fw, int fh, Color c);
};

// Palette indexed by ASCII character; '.' and ' ' are always transparent.
using Palette = std::array<Color, 128>;
Image imageFromAscii(const std::vector<std::string>& rows, const Palette& pal);

class Canvas : public Image
{
public:
  Canvas(int width, int height);

  void clear(Color c);
  void fillRect(int x, int y, int fw, int fh, Color c);
  void frameRect(int x, int y, int fw, int fh, Color c);
  // flash != 0 draws the sprite's silhouette in that color (hit flash).
  void blit(
    const Image& img,
    int x,
    int y,
    bool flipX = false,
    Color flash = 0,
    int scale = 1);
  void drawText(
    const std::string& s,
    int x,
    int y,
    Color c,
    int scale = 1,
    Color shadow = 0);
  void drawTextCentered(
    const std::string& s,
    int cx,
    int y,
    Color c,
    int scale = 1,
    Color shadow = 0);
  static int textWidth(const std::string& s, int scale = 1);
};

// 5x7 bitmap font; returns nullptr for unknown glyphs.
const std::array<const char*, 7>* glyphFor(char c);

} // namespace td
