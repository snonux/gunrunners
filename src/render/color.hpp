#pragma once

#include <cstdint>

namespace td
{

using Color = std::uint32_t; // 0xAARRGGBB

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

} // namespace td
