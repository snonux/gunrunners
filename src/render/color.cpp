#include "render/color.hpp"

#include <algorithm>

namespace gr
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

} // namespace gr
