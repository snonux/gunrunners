#include "gfx/canvas.hpp"

#include <algorithm>
#include <cctype>

namespace td
{

Color lerpColor(Color a, Color b, float t)
{
  t = std::min(std::max(t, 0.0f), 1.0f);
  auto mix = [t](int x, int y) { return int(float(x) + float(y - x) * t); };
  return rgba(
    mix(redOf(a), redOf(b)),
    mix(greenOf(a), greenOf(b)),
    mix(blueOf(a), blueOf(b)),
    mix(alphaOf(a), alphaOf(b)));
}

Color scaleColor(Color c, float f)
{
  auto s = [f](int v) { return std::min(255, std::max(0, int(float(v) * f))); };
  return rgba(s(redOf(c)), s(greenOf(c)), s(blueOf(c)), alphaOf(c));
}

Color withAlpha(Color c, int a) { return (c & 0x00FFFFFFu) | (Color(a & 255) << 24); }

static inline Color blendOver(Color dst, Color src)
{
  const int a = alphaOf(src);
  if (a == 255)
    return src;
  if (a == 0)
    return dst;
  const int ia = 255 - a;
  return rgba(
    (redOf(src) * a + redOf(dst) * ia) / 255,
    (greenOf(src) * a + greenOf(dst) * ia) / 255,
    (blueOf(src) * a + blueOf(dst) * ia) / 255,
    std::max(alphaOf(dst), a));
}

void Image::blend(int x, int y, Color c)
{
  if (x >= 0 && y >= 0 && x < w && y < h)
  {
    auto& d = px[std::size_t(y * w + x)];
    d = blendOver(d, c);
  }
}

void Image::fill(int x, int y, int fw, int fh, Color c)
{
  const int x0 = std::max(0, x), y0 = std::max(0, y);
  const int x1 = std::min(w, x + fw), y1 = std::min(h, y + fh);
  for (int yy = y0; yy < y1; ++yy)
    for (int xx = x0; xx < x1; ++xx)
      px[std::size_t(yy * w + xx)] = blendOver(px[std::size_t(yy * w + xx)], c);
}

Image imageFromAscii(const std::vector<std::string>& rows, const Palette& pal)
{
  int width = 0;
  for (const auto& r : rows)
    width = std::max(width, int(r.size()));
  Image img(width, int(rows.size()), 0);
  for (int y = 0; y < int(rows.size()); ++y)
  {
    for (int x = 0; x < int(rows[std::size_t(y)].size()); ++x)
    {
      const char ch = rows[std::size_t(y)][std::size_t(x)];
      if (ch == '.' || ch == ' ')
        continue;
      img.set(x, y, pal[std::size_t(ch & 127)]);
    }
  }
  return img;
}

Canvas::Canvas(int width, int height) : Image(width, height, rgb(0, 0, 0)) {}

void Canvas::clear(Color c) { std::fill(px.begin(), px.end(), c); }

void Canvas::fillRect(int x, int y, int fw, int fh, Color c) { fill(x, y, fw, fh, c); }

void Canvas::frameRect(int x, int y, int fw, int fh, Color c)
{
  fill(x, y, fw, 1, c);
  fill(x, y + fh - 1, fw, 1, c);
  fill(x, y + 1, 1, fh - 2, c);
  fill(x + fw - 1, y + 1, 1, fh - 2, c);
}

void Canvas::blit(const Image& img, int x, int y, bool flipX, Color flash, int scale)
{
  for (int sy = 0; sy < img.h; ++sy)
  {
    for (int sx = 0; sx < img.w; ++sx)
    {
      Color c = img.get(flipX ? img.w - 1 - sx : sx, sy);
      if (alphaOf(c) == 0)
        continue;
      if (flash)
        c = withAlpha(flash, alphaOf(c));
      if (scale == 1)
      {
        blend(x + sx, y + sy, c);
      }
      else
      {
        fill(x + sx * scale, y + sy * scale, scale, scale, c);
      }
    }
  }
}

int Canvas::textWidth(const std::string& s, int scale)
{
  return s.empty() ? 0 : (int(s.size()) * 6 - 1) * scale;
}

void Canvas::drawText(
  const std::string& s,
  int x,
  int y,
  Color c,
  int scale,
  Color shadow)
{
  if (shadow)
    drawText(s, x + scale, y + scale, shadow, scale, 0);
  int cx = x;
  for (char ch : s)
  {
    if (const auto* g = glyphFor(char(std::toupper(static_cast<unsigned char>(ch)))))
    {
      for (int gy = 0; gy < 7; ++gy)
        for (int gx = 0; gx < 5; ++gx)
          if ((*g)[std::size_t(gy)][gx] == '#')
            fill(cx + gx * scale, y + gy * scale, scale, scale, c);
    }
    cx += 6 * scale;
  }
}

void Canvas::drawTextCentered(
  const std::string& s,
  int cx,
  int y,
  Color c,
  int scale,
  Color shadow)
{
  drawText(s, cx - textWidth(s, scale) / 2, y, c, scale, shadow);
}

} // namespace td
