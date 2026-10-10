// Level 47, Silk Canyon (Episode 7, DEEP SPACE): a deep amber canyon on
// Vurr strung with giant silver webs. Banded sandstone for the tiles, a
// strip of dusky sky high above, the far walls fading into the haze with
// webs slung between them, and loose strands hanging close by.

#include "assets/art_alien.hpp"

#include "base/math.hpp"
#include "render/vector.hpp"

#include <algorithm>
#include <cmath>
#include <string_view>

namespace gr
{

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr Color kInk = rgb(46, 22, 12);

Color lighten(Color c, float t) { return lerpColor(c, rgb(255, 255, 255), t); }
Color darken(Color c, float t) { return lerpColor(c, rgb(0, 0, 0), t); }

void circle(cairo_t* cr, double x, double y, double r) { cairo_arc(cr, x, y, r, 0, 2 * kPi); }

void gradient(cairo_t* cr, double w, double h, std::initializer_list<std::pair<double, Color>> stops)
{
  cairo_pattern_t* p = cairo_pattern_create_linear(0, 0, 0, h);
  for (const auto& s : stops)
    cairo_pattern_add_color_stop_rgba(p, s.first, redOf(s.second) / 255.0, greenOf(s.second) / 255.0,
      blueOf(s.second) / 255.0, alphaOf(s.second) / 255.0);
  cairo_rectangle(cr, 0, 0, w, h);
  cairo_set_source(cr, p);
  cairo_fill(cr);
  cairo_pattern_destroy(p);
}

// An orb web centred on (cx, cy): spokes and a spiral, sagging a little.
void web(cairo_t* cr, double cx, double cy, double r, Color c, double alpha, double width)
{
  const int spokes = 12;
  cairo_set_line_width(cr, width);
  setColor(cr, withAlpha(c, int(alpha * 255)));
  for (int k = 0; k < spokes; ++k)
  {
    const double a = 2 * kPi * k / spokes;
    cairo_move_to(cr, cx, cy);
    cairo_line_to(cr, cx + std::cos(a) * r, cy + std::sin(a) * r);
  }
  cairo_stroke(cr);
  for (double rr = r * 0.12; rr < r; rr += r * 0.1)
  {
    for (int k = 0; k <= spokes; ++k)
    {
      const double a = 2 * kPi * k / spokes;
      const double sag = rr * 0.05 * std::sin(a * 0.5 + 1.0);
      const double x = cx + std::cos(a) * rr, y = cy + std::sin(a) * rr + sag;
      if (k == 0)
        cairo_move_to(cr, x, y);
      else
        cairo_line_to(cr, x, y);
    }
    cairo_stroke(cr);
  }
}

// A canyon wall's silhouette along one side: ragged, with ledges.
void wall(cairo_t* cr, Rng& rng, double x0, double w, double h, bool left, Color c)
{
  const double edge = left ? x0 + w : x0;
  cairo_move_to(cr, left ? x0 : x0 + w, 0);
  for (double y = 0; y <= h; y += rng.range(30, 70))
    cairo_line_to(cr, edge + (left ? -1 : 1) * rng.range(0, w * 0.4), y);
  cairo_line_to(cr, left ? x0 : x0 + w, h);
  cairo_close_path(cr);
  setColor(cr, c);
  cairo_fill(cr);
}

} // namespace

bool isSilk(const Theme& t) { return std::string_view(t.look) == "alien_silk"; }

// Sandstone: amber bands, darker seams, pebbles and silk caught in the
// cracks. Variant 1 has a band of dark rock, 2 a tuft of web.
Texture bakeSilkSolid(const Renderer& r, const Theme& t, int variant)
{
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  Rng rng(std::uint32_t(variant * 7907 + 47));
  gradient(cr, 64, 64, {{0.0, t.rock}, {1.0, darken(t.rock, 0.15f)}});
  // Strata: wavy bands, wrapping at the edges (the same y at x 0 and 64).
  for (int k = 0; k < 4; ++k)
  {
    const double y = 8 + k * 16 + rng.range(-3, 3), amp = rng.range(1, 3);
    cairo_move_to(cr, 0, y);
    for (int x = 4; x <= 64; x += 4)
      cairo_line_to(cr, x, y + amp * std::sin(double(x) / 64.0 * 2 * kPi));
    cairo_set_line_width(cr, rng.range(3, 7));
    setColor(cr, withAlpha(k % 2 ? lighten(t.rockLight, 0.15f) : darken(t.rock, 0.2f), 120));
    cairo_stroke(cr);
  }
  for (int i = 0; i < 7; ++i)
  {
    circle(cr, rng.range(4, 60), rng.range(4, 60), rng.range(1.0, 2.6));
    setColor(cr, withAlpha(t.rockDark, 120));
    cairo_fill(cr);
  }
  if (variant == 1)
  {
    cairo_rectangle(cr, 0, 26, 64, 12);
    setColor(cr, withAlpha(t.rockDark, 120));
    cairo_fill(cr);
  }
  else if (variant == 2)
    web(cr, rng.range(20, 44), rng.range(20, 44), 16, rgb(250, 245, 230), 0.45, 1.0);
  return img.toTexture(r, 0.0f, 0.0f);
}

// The top of a ledge: a sandy lip, wisps of silk caught on it.
Texture bakeSilkSolidTop(const Renderer& r, const Theme& t, float topOffset, int height)
{
  VectorImage img(64, height);
  cairo_t* cr = img.cr();
  Rng rng(4747u);
  const double y = topOffset;
  cairo_rectangle(cr, 0, y, 64, 9);
  fillGradientOutline(cr, y, y + 9, lighten(t.rockLight, 0.3f), t.rockLight, darken(t.rock, 0.2f), 1.0);
  for (int i = 0; i < 3; ++i)
  {
    const double x = rng.range(6, 58);
    cairo_move_to(cr, x, y + 2);
    cairo_curve_to(cr, x + 4, y - 6, x + 10, y - 4, x + 14, y - 10);
    cairo_set_line_width(cr, 1.2);
    setColor(cr, rgba(255, 250, 235, 170));
    cairo_stroke(cr);
  }
  cairo_move_to(cr, 0, y + 1);
  cairo_line_to(cr, 64, y + 1);
  cairo_set_line_width(cr, 2.0);
  setColor(cr, rgba(255, 240, 200, 170));
  cairo_stroke(cr);
  return img.toTexture(r, 0.0f, topOffset);
}

// One-way: a hammock of woven silk slung between two pegs.
Texture bakeSilkPlatform(const Renderer& r, const Theme& t)
{
  VectorImage img(64, 40);
  cairo_t* cr = img.cr();
  cairo_move_to(cr, 0, 2);
  cairo_curve_to(cr, 20, 10, 44, 10, 64, 2);
  cairo_line_to(cr, 64, 6);
  cairo_curve_to(cr, 44, 14, 20, 14, 0, 6);
  cairo_close_path(cr);
  fillGradientOutline(cr, 2, 14, lighten(t.platform, 0.4f), t.platformDark, kInk, 1.2);
  for (int x = 6; x < 64; x += 8)
  {
    cairo_move_to(cr, x, 3 + 4 * std::sin(double(x) / 64.0 * kPi));
    cairo_line_to(cr, x + 4, 9 + 4 * std::sin(double(x) / 64.0 * kPi));
    cairo_set_line_width(cr, 1.0);
    setColor(cr, rgba(255, 255, 255, 140));
    cairo_stroke(cr);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// The sky: the deep canyon's amber air, a strip of violet dusk far above
// with Vurr's moons in it, and sunlight slanting down the walls.
Texture bakeSilkSky(const Renderer& r, const Theme& t)
{
  VectorImage img(kScreenW, kScreenH);
  cairo_t* cr = img.cr();
  gradient(cr, kScreenW, kScreenH, {{0.0, t.skyTop}, {0.45, t.skyMid}, {1.0, t.skyBottom}});
  Rng rng(4700u);
  const double moons[2][3] = {{780, 70, 30}, {860, 110, 12}};
  for (const auto& m : moons)
  {
    circle(cr, m[0], m[1], m[2]);
    setColor(cr, rgba(255, 240, 220, 160));
    cairo_fill(cr);
  }
  // Shafts of light.
  for (int k = 0; k < 3; ++k)
  {
    const double x = 300 + k * 420 + rng.range(-60, 60);
    cairo_move_to(cr, x, 0);
    cairo_line_to(cr, x + 120, 0);
    cairo_line_to(cr, x + 420, kScreenH);
    cairo_line_to(cr, x + 180, kScreenH);
    cairo_close_path(cr);
    setColor(cr, rgba(255, 220, 150, 26));
    cairo_fill(cr);
  }
  // Dust in the light.
  for (int i = 0; i < 70; ++i)
  {
    circle(cr, rng.range(0, kScreenW), rng.range(0, kScreenH), rng.range(0.8, 1.8));
    setColor(cr, rgba(255, 230, 180, int(60 + 100 * rng.uniform())));
    cairo_fill(cr);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// Far: the canyon's walls in the haze, giant webs slung across.
Texture bakeSilkFar(const Renderer& r, const Theme& t, int layerW)
{
  VectorImage img(layerW, kScreenH);
  cairo_t* cr = img.cr();
  Rng rng(4711u);
  for (double x = 0; x < layerW; x += rng.range(260, 420))
  {
    const double w = rng.range(90, 160);
    wall(cr, rng, x, w, kScreenH, rng.uniform() < 0.5, withAlpha(t.farLayer, 190));
    web(cr, x + w + rng.range(40, 120), rng.range(120, 420), rng.range(60, 110), rgb(250, 240, 220), 0.35, 1.2);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// Near: long strands hanging down, a few with cocoons, and a big web.
Texture bakeSilkNear(const Renderer& r, const Theme& t, int layerW)
{
  VectorImage img(layerW, kScreenH);
  cairo_t* cr = img.cr();
  Rng rng(4812u);
  for (double x = 60; x < layerW; x += rng.range(120, 260))
  {
    const double len = rng.range(120, kScreenH * 0.7);
    cairo_move_to(cr, x, 0);
    cairo_curve_to(cr, x + 6, len * 0.3, x - 6, len * 0.6, x + rng.range(-10, 10), len);
    cairo_set_line_width(cr, 1.6);
    setColor(cr, rgba(255, 248, 232, 120));
    cairo_stroke(cr);
    if (rng.uniform() < 0.4)
    {
      cairo_save(cr);
      cairo_translate(cr, x, len + 14);
      cairo_scale(cr, 8, 16);
      cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
      cairo_restore(cr);
      setColor(cr, withAlpha(lighten(t.nearLayer, 0.4f), 170));
      cairo_fill(cr);
    }
  }
  for (double x = 300; x < layerW; x += rng.range(700, 1100))
    web(cr, x, rng.range(80, 260), rng.range(110, 170), rgb(255, 250, 235), 0.28, 1.6);
  return img.toTexture(r, 0.0f, 0.0f);
}

} // namespace gr
