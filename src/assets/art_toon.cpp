// Level 28, Toon Town Throwdown (theme look "toon"): a 1930s rubber-hose
// cartoon. The masses are painted earth laid in flat with a dry brush (a
// pebble, a crack, a little cross-hatching inked here and there); where you
// stand, a scalloped band of painted grass under a thick wobbly ink line,
// tufts poking up; one-way floors are wobbly painted planks nailed end to
// end, climbables a crooked wooden ladder. Behind: a cream paper sky (its
// grain and fibres, a scratch or two on the film and the projector's dark
// corners), inked puffy clouds and a grinning sun with wavy rays; far off,
// pastel hills with ink outlines, lollipop trees, a red barn and a
// windmill; close by, rounder green hills with bushes, daisies and runs of
// picket fence.

#include "assets/art_toon.hpp"

#include "base/math.hpp"
#include "render/vector.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <string_view>
#include <vector>

namespace gr
{

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr Color kInk = rgb(30, 22, 20);
constexpr Color kCream = rgb(252, 244, 222);

Color lighten(Color c, float t) { return lerpColor(c, rgb(255, 255, 255), t); }
Color darken(Color c, float t) { return lerpColor(c, rgb(0, 0, 0), t); }

void stop(cairo_pattern_t* g, double at, Color c, double a = 1.0)
{
  cairo_pattern_add_color_stop_rgba(g, at, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, a);
}

void setRgba(cairo_t* cr, Color c, double a)
{
  cairo_set_source_rgba(cr, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, a);
}

void circle(cairo_t* cr, double x, double y, double r)
{
  cairo_new_sub_path(cr);
  cairo_arc(cr, x, y, r, 0, 2 * kPi);
}

void ellipse(cairo_t* cr, double x, double y, double rx, double ry, double angle = 0.0)
{
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_rotate(cr, angle);
  cairo_scale(cr, rx, ry);
  cairo_new_sub_path(cr);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_restore(cr);
}

void fillLinear(cairo_t* cr, double x0, double y0, double x1, double y1, std::initializer_list<std::pair<double, Color>> stops,
  double alpha = 1.0, bool preserve = false)
{
  cairo_pattern_t* g = cairo_pattern_create_linear(x0, y0, x1, y1);
  for (const auto& s : stops)
    stop(g, s.first, s.second, alpha);
  cairo_set_source(cr, g);
  if (preserve)
    cairo_fill_preserve(cr);
  else
    cairo_fill(cr);
  cairo_pattern_destroy(g);
}

void inkStroke(cairo_t* cr, Color c, double w, double a = 1.0)
{
  setRgba(cr, c, a);
  cairo_set_line_width(cr, w);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
  cairo_stroke(cr);
}

// Fills the path flat and inks its outline.
void fillInk(cairo_t* cr, Color fill, double line)
{
  setRgba(cr, fill, 1.0);
  cairo_fill_preserve(cr);
  inkStroke(cr, kInk, line);
}

// The painted earth's colour: the theme's terracotta, softened toward cream.
Color earthOf(const Theme& t) { return lerpColor(lerpColor(t.rock, t.rockLight, 0.5f), kCream, 0.18f); }

// Dry-brush strokes across a tile, wrapped both ways so the blocks tile.
void dryBrush(cairo_t* cr, Color base, Rng& rng, int count)
{
  for (int i = 0; i < count; ++i)
  {
    const double x = rng.range(0, 64), y = rng.range(0, 64), l = rng.range(14, 34), bow = rng.range(-2.5f, 2.5f);
    const bool light = i % 2 == 0;
    for (const double ox : {-64.0, 0.0, 64.0})
      for (const double oy : {-64.0, 0.0, 64.0})
      {
        cairo_move_to(cr, x + ox, y + oy);
        cairo_curve_to(cr, x + ox + l * 0.3, y + oy + bow, x + ox + l * 0.7, y + oy + bow, x + ox + l, y + oy + rng.range(-1, 1));
      }
    setRgba(cr, light ? lighten(base, 0.28f) : darken(base, 0.18f), rng.range(0.12f, 0.24f));
    cairo_set_line_width(cr, rng.range(2.5f, 5.5f));
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_stroke(cr);
  }
}

// A hill's top edge across a layer `w` wide: sums of sines with whole
// periods, so the layer wraps.
double hillY(double x, double w, double base, std::initializer_list<std::array<double, 3>> waves)
{
  double y = base;
  for (const auto& wv : waves)
    y += wv[0] * std::sin(2 * kPi * wv[1] * x / w + wv[2]);
  return y;
}

// One band of hills: filled, inked along the top, a lighter brushed band
// just under the ink.
template <typename F>
void hillBand(cairo_t* cr, double w, F top, Color fill, Color bottom, double line)
{
  cairo_move_to(cr, -4, 724);
  for (double x = -4; x <= w + 4; x += 8)
    cairo_line_to(cr, x, top(x));
  cairo_line_to(cr, w + 4, 724);
  cairo_close_path(cr);
  fillLinear(cr, 0, 380, 0, 720, {{0.0, fill}, {1.0, bottom}});
  for (double x = -4; x <= w + 4; x += 8)
    (x < -3 ? cairo_move_to : cairo_line_to)(cr, x, top(x) + 7);
  setRgba(cr, lighten(fill, 0.35f), 0.55);
  cairo_set_line_width(cr, 7);
  cairo_stroke(cr);
  for (double x = -4; x <= w + 4; x += 8)
    (x < -3 ? cairo_move_to : cairo_line_to)(cr, x, top(x));
  inkStroke(cr, kInk, line);
}

// A lollipop tree: a wobbly trunk and a round inked canopy with a shine.
void lollipop(cairo_t* cr, double x, double ground, double s, Color leaf)
{
  cairo_move_to(cr, x, ground + 4);
  cairo_curve_to(cr, x - 3 * s, ground - 14 * s, x + 3 * s, ground - 26 * s, x, ground - 36 * s);
  setRgba(cr, kInk, 1.0);
  cairo_set_line_width(cr, 7 * s);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  cairo_stroke_preserve(cr);
  setRgba(cr, rgb(150, 100, 66), 1.0);
  cairo_set_line_width(cr, 3.6 * s);
  cairo_stroke(cr);
  circle(cr, x, ground - 52 * s, 22 * s);
  fillInk(cr, leaf, 2.6);
  ellipse(cr, x - 8 * s, ground - 60 * s, 7 * s, 4.5 * s, -0.6);
  setRgba(cr, lighten(leaf, 0.5f), 0.8);
  cairo_fill(cr);
}

} // namespace

bool isToon(const Theme& t) { return std::string_view(t.look) == "toon"; }

// Painted earth: one flat colour, dry-brushed. Variant 1 two inked pebbles,
// variant 2 a crack and a patch of cross-hatching.
Texture bakeToonSolid(const Renderer& r, const Theme& t, int variant)
{
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  const Color earth = earthOf(t);
  cairo_rectangle(cr, 0, 0, 64, 64);
  setRgba(cr, earth, 1.0);
  cairo_fill(cr);
  Rng brush(4211u); // the same strokes on every variant, so they sit together
  dryBrush(cr, earth, brush, 12);
  Rng rng(std::uint32_t(variant * 7919 + 4217));
  // Speckles: tiny ink flicks.
  for (int i = 0; i < 4; ++i)
  {
    const double x = rng.range(6, 58), y = rng.range(6, 58);
    cairo_move_to(cr, x, y);
    cairo_line_to(cr, x + rng.range(2, 4), y + rng.range(-1, 1));
    inkStroke(cr, darken(earth, 0.45f), 1.4, 0.6);
  }
  if (variant == 1)
  {
    // Two pebbles, inked, with a dab of light.
    for (const auto& p : {std::array<double, 3>{22, 38, 7.5}, std::array<double, 3>{40, 46, 5}})
    {
      ellipse(cr, p[0], p[1], p[2], p[2] * 0.7, 0.2);
      fillInk(cr, lerpColor(earth, kCream, 0.45f), 1.8);
      ellipse(cr, p[0] - p[2] * 0.3, p[1] - p[2] * 0.25, p[2] * 0.35, p[2] * 0.2, -0.3);
      setRgba(cr, rgb(255, 255, 255), 0.8);
      cairo_fill(cr);
    }
  }
  else if (variant == 2)
  {
    // A crack, and a patch of cross-hatching beside it.
    cairo_move_to(cr, 14, 18);
    cairo_line_to(cr, 20, 26);
    cairo_line_to(cr, 17, 31);
    cairo_line_to(cr, 24, 40);
    cairo_move_to(cr, 20, 26);
    cairo_line_to(cr, 27, 27);
    inkStroke(cr, kInk, 1.8, 0.85);
    for (int i = 0; i < 4; ++i)
    {
      cairo_move_to(cr, 38 + i * 4, 52);
      cairo_line_to(cr, 44 + i * 4, 44);
    }
    for (int i = 0; i < 3; ++i)
    {
      cairo_move_to(cr, 40 + i * 4, 44);
      cairo_line_to(cr, 46 + i * 4, 52);
    }
    inkStroke(cr, darken(earth, 0.5f), 1.1, 0.55);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// Where you stand: a band of painted grass, its bottom edge scalloped and
// inked, a thick wobbly ink line along the top and tufts poking up.
Texture bakeToonSolidTop(const Renderer& r, const Theme& t, float topOffset, int height)
{
  VectorImage img(64, height);
  cairo_t* cr = img.cr();
  const double y = topOffset;
  const Color grass = t.platform, grassLight = lighten(t.platform, 0.35f);
  auto top = [&](double x) { return y - 1.0 + 1.3 * std::sin(2 * kPi * x / 64.0 + 0.7); };
  auto bottom = [&](double x) {
    const double u = std::fmod(x + 64.0, 16.0) / 16.0;
    return y + 10.0 + 5.0 * std::sin(kPi * u);
  };
  // The scallops' shadow on the earth.
  cairo_move_to(cr, 0, y);
  for (double x = 0; x <= 64; x += 2)
    cairo_line_to(cr, x, bottom(x) + 4);
  cairo_line_to(cr, 64, y);
  cairo_close_path(cr);
  setRgba(cr, rgb(60, 30, 20), 0.22);
  cairo_fill(cr);
  // The band.
  cairo_move_to(cr, 0, top(0));
  for (double x = 0; x <= 64; x += 2)
    cairo_line_to(cr, x, top(x));
  for (double x = 64; x >= 0; x -= 2)
    cairo_line_to(cr, x, bottom(x));
  cairo_close_path(cr);
  fillLinear(cr, 0, y - 2, 0, y + 15, {{0.0, grassLight}, {0.45, grass}, {1.0, darken(grass, 0.12f)}});
  // Brushed blades in the band.
  Rng rng(4229u);
  for (int i = 0; i < 9; ++i)
  {
    const double x = rng.range(2, 62);
    cairo_move_to(cr, x, y + 9);
    cairo_line_to(cr, x + rng.range(-1.5f, 1.5f), y + 3);
  }
  inkStroke(cr, darken(grass, 0.3f), 1.2, 0.5);
  // The scalloped edge, inked.
  cairo_move_to(cr, 0, bottom(0));
  for (double x = 0; x <= 64; x += 2)
    cairo_line_to(cr, x, bottom(x));
  inkStroke(cr, kInk, 2.0);
  // Tufts poking up: three inked blades each.
  for (const double tx : {14.0, 45.0})
  {
    const double ty = top(tx) + 1;
    cairo_move_to(cr, tx - 6, ty);
    cairo_line_to(cr, tx - 5, ty - 7);
    cairo_line_to(cr, tx - 2, ty - 1);
    cairo_line_to(cr, tx, ty - 11);
    cairo_line_to(cr, tx + 2, ty - 1);
    cairo_line_to(cr, tx + 6, ty - 8);
    cairo_line_to(cr, tx + 6, ty);
    cairo_close_path(cr);
    fillInk(cr, grassLight, 1.6);
  }
  // The thick ink line along the top, a highlight under it.
  cairo_move_to(cr, -2, top(-2) + 3.5);
  for (double x = 0; x <= 66; x += 2)
    cairo_line_to(cr, x, top(x) + 3.5);
  inkStroke(cr, lighten(grassLight, 0.4f), 1.6, 0.7);
  cairo_move_to(cr, -2, top(-2));
  for (double x = 0; x <= 66; x += 2)
    cairo_line_to(cr, x, top(x));
  inkStroke(cr, kInk, 3.4);
  return img.toTexture(r, 0.0f, topOffset);
}

// A one-way floor: a wobbly painted plank, nailed at its end, its grain
// brushed in and its shadow soft under it.
Texture bakeToonPlatform(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(64, 40);
  cairo_t* cr = img.cr();
  const Color wood = rgb(238, 196, 122), woodDark = rgb(196, 140, 78);
  auto top = [](double x) { return 2.5 + 1.2 * std::sin(2 * kPi * x / 64.0 + 1.9); };
  auto bot = [](double x) { return 17.0 + 1.0 * std::sin(2 * kPi * x / 64.0 + 0.4); };
  // The shadow.
  cairo_rectangle(cr, 0, 16, 64, 12);
  {
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 16, 0, 28);
    cairo_pattern_add_color_stop_rgba(g, 0, 0.2, 0.1, 0.05, 0.3);
    cairo_pattern_add_color_stop_rgba(g, 1, 0.2, 0.1, 0.05, 0.0);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  cairo_move_to(cr, 0, top(0));
  for (double x = 0; x <= 64; x += 2)
    cairo_line_to(cr, x, top(x));
  for (double x = 64; x >= 0; x -= 2)
    cairo_line_to(cr, x, bot(x));
  cairo_close_path(cr);
  fillLinear(cr, 0, 2, 0, 18, {{0.0, lighten(wood, 0.3f)}, {0.5, wood}, {1.0, woodDark}});
  // Grain.
  for (const double gy : {7.5, 12.0})
  {
    cairo_move_to(cr, -2, gy);
    cairo_curve_to(cr, 18, gy - 1.6, 30, gy + 1.8, 66, gy);
  }
  inkStroke(cr, darken(woodDark, 0.25f), 1.0, 0.55);
  ellipse(cr, 26, 10, 3.2, 1.6);
  inkStroke(cr, darken(woodDark, 0.3f), 1.0, 0.55);
  // The joint with the next plank and its nails.
  cairo_move_to(cr, 63.2, top(63.2));
  cairo_line_to(cr, 62.6, bot(62.6));
  inkStroke(cr, kInk, 1.6);
  for (const double ny : {6.0, 13.5})
  {
    circle(cr, 57.5, ny, 1.5);
    setRgba(cr, kInk, 1.0);
    cairo_fill(cr);
  }
  // Ink along top and bottom.
  cairo_move_to(cr, -2, top(-2));
  for (double x = 0; x <= 66; x += 2)
    cairo_line_to(cr, x, top(x));
  inkStroke(cr, kInk, 2.8);
  cairo_move_to(cr, -2, bot(-2));
  for (double x = 0; x <= 66; x += 2)
    cairo_line_to(cr, x, bot(x));
  inkStroke(cr, kInk, 2.4);
  cairo_move_to(cr, 2, top(2) + 2.6);
  cairo_line_to(cr, 50, top(50) + 2.6);
  inkStroke(cr, rgb(255, 246, 220), 1.2, 0.8);
  return img.toTexture(r, 0.0f, 0.0f);
}

// A climbable: a crooked wooden ladder, rails and rungs inked (drawn 16 px
// left of its block, so its middle is the left cell's middle).
Texture bakeToonLadder(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  const Color wood = rgb(214, 160, 96);
  for (const double y : {16.0, 46.0})
  {
    cairo_move_to(cr, 17, y + 1);
    cairo_line_to(cr, 47, y - 1);
    setRgba(cr, kInk, 1.0);
    cairo_set_line_width(cr, 8);
    cairo_stroke_preserve(cr);
    setRgba(cr, wood, 1.0);
    cairo_set_line_width(cr, 4.4);
    cairo_stroke(cr);
  }
  for (const double x : {17.0, 47.0})
  {
    cairo_move_to(cr, x - 1, -2);
    cairo_curve_to(cr, x + 2, 20, x - 2, 44, x + 1, 66);
    setRgba(cr, kInk, 1.0);
    cairo_set_line_width(cr, 9);
    cairo_stroke_preserve(cr);
    setRgba(cr, lighten(wood, 0.15f), 1.0);
    cairo_set_line_width(cr, 5.4);
    cairo_stroke(cr);
  }
  return img.toTexture(r, 16.0f, 0.0f);
}

// The cream paper sky: its grain and fibres, puffy inked clouds, the sun
// grinning under its wavy rays, a scratch on the film and the projector's
// dark corners.
Texture bakeToonSky(const Renderer& r, const Theme& t)
{
  VectorImage img(kScreenW, kScreenH);
  cairo_t* cr = img.cr();
  cairo_rectangle(cr, 0, 0, kScreenW, kScreenH);
  fillLinear(cr, 0, 0, 0, kScreenH, {{0.0, t.skyTop}, {0.6, t.skyMid}, {1.0, t.skyBottom}});
  Rng rng(4231u);
  // Paper grain: specks and fibres.
  for (int i = 0; i < 2600; ++i)
  {
    cairo_rectangle(cr, rng.range(0, float(kScreenW)), rng.range(0, float(kScreenH)), 1.2, 1.2);
    setRgba(cr, i % 3 ? rgb(150, 110, 70) : rgb(255, 255, 255), rng.range(0.05f, 0.14f));
    cairo_fill(cr);
  }
  for (int i = 0; i < 160; ++i)
  {
    const double x = rng.range(0, float(kScreenW)), y = rng.range(0, float(kScreenH)), l = rng.range(6, 18);
    cairo_move_to(cr, x, y);
    cairo_curve_to(cr, x + l * 0.3, y + rng.range(-3, 3), x + l * 0.7, y + rng.range(-3, 3), x + l, y + rng.range(-2, 2));
    inkStroke(cr, rgb(170, 130, 90), 0.7, 0.12);
  }
  // The sun: wavy rays, a round inked face with pie-cut eyes and a grin.
  {
    const double sx = 1040, sy = 150, sr = 62;
    for (int i = 0; i < 14; ++i)
    {
      const double a = i * 2 * kPi / 14.0, b = a + kPi / 14.0 * 0.55, c = a - kPi / 14.0 * 0.55;
      const double r0 = sr + 10, r1 = sr + (i % 2 ? 46 : 62);
      cairo_move_to(cr, sx + std::cos(b) * r0, sy + std::sin(b) * r0);
      cairo_curve_to(cr, sx + std::cos(a + 0.12) * (r0 + r1) * 0.5, sy + std::sin(a + 0.12) * (r0 + r1) * 0.5,
        sx + std::cos(a - 0.05) * r1 * 0.95, sy + std::sin(a - 0.05) * r1 * 0.95, sx + std::cos(a) * r1, sy + std::sin(a) * r1);
      cairo_line_to(cr, sx + std::cos(c) * r0, sy + std::sin(c) * r0);
      cairo_close_path(cr);
      fillInk(cr, rgb(255, 222, 110), 2.4);
    }
    circle(cr, sx, sy, sr);
    cairo_pattern_t* g = cairo_pattern_create_radial(sx - 20, sy - 24, 6, sx, sy, sr);
    stop(g, 0, rgb(255, 250, 200));
    stop(g, 1, rgb(255, 204, 80));
    cairo_set_source(cr, g);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(g);
    inkStroke(cr, kInk, 3.6);
    for (const double ex : {-20.0, 20.0})
    {
      ellipse(cr, sx + ex, sy - 14, 9, 14);
      setRgba(cr, kInk, 1.0);
      cairo_fill(cr);
      // The pie cut.
      cairo_move_to(cr, sx + ex, sy - 14);
      cairo_arc(cr, sx + ex, sy - 14, 9.5, -kPi * 0.42, -kPi * 0.12);
      cairo_close_path(cr);
      setRgba(cr, rgb(255, 250, 230), 1.0);
      cairo_fill(cr);
    }
    for (const double cx : {-36.0, 36.0})
    {
      ellipse(cr, sx + cx, sy + 12, 10, 6);
      setRgba(cr, rgb(255, 120, 90), 0.45);
      cairo_fill(cr);
    }
    cairo_move_to(cr, sx - 30, sy + 10);
    cairo_curve_to(cr, sx - 18, sy + 36, sx + 18, sy + 36, sx + 30, sy + 10);
    cairo_curve_to(cr, sx + 14, sy + 20, sx - 14, sy + 20, sx - 30, sy + 10);
    cairo_close_path(cr);
    fillInk(cr, rgb(160, 40, 40), 2.6);
  }
  // Puffy clouds: lumps along a flat bottom, inked, shaded underneath.
  for (int c = 0; c < 5; ++c)
  {
    const double cx = 90 + c * 270 + rng.range(-40, 40), cy = rng.range(70, 300), cw = rng.range(130, 200);
    if (std::abs(cx - 1040) < 220 && cy < 260)
      continue; // not over the sun
    Rng lumps(std::uint32_t(4241 + c * 17));
    cairo_new_path(cr);
    const int n = 4 + c % 2;
    cairo_move_to(cr, cx - cw * 0.5, cy);
    for (int i = 0; i < n; ++i)
    {
      const double x0 = cx - cw * 0.5 + cw * i / n, x1 = cx - cw * 0.5 + cw * (i + 1) / n;
      const double hump = lumps.range(26, 44) * (1.0 - std::abs((i + 0.5) / n - 0.5));
      cairo_curve_to(cr, x0, cy - hump * 1.6, x1, cy - hump * 1.6, x1, cy);
    }
    cairo_curve_to(cr, cx + cw * 0.5 + 18, cy + 4, cx + cw * 0.5, cy + 18, cx + cw * 0.3, cy + 16);
    cairo_line_to(cr, cx - cw * 0.3, cy + 16);
    cairo_curve_to(cr, cx - cw * 0.5, cy + 18, cx - cw * 0.5 - 18, cy + 4, cx - cw * 0.5, cy);
    cairo_close_path(cr);
    fillLinear(cr, 0, cy - 50, 0, cy + 16, {{0.0, rgb(255, 254, 246)}, {0.7, rgb(255, 248, 230)}, {1.0, rgb(232, 214, 190)}}, 1.0,
      true);
    inkStroke(cr, kInk, 2.8);
  }
  // A scratch down the film.
  for (int i = 0; i < 3; ++i)
  {
    const double x = rng.range(100, float(kScreenW) - 100);
    cairo_move_to(cr, x, rng.range(0, 200));
    cairo_curve_to(cr, x + rng.range(-4, 4), 300, x + rng.range(-6, 6), 500, x + rng.range(-3, 3), rng.range(560, 720));
    inkStroke(cr, rgb(120, 90, 60), 0.8, 0.14);
  }
  // The projector's dark corners.
  cairo_rectangle(cr, 0, 0, kScreenW, kScreenH);
  cairo_pattern_t* v = cairo_pattern_create_radial(640, 360, 360, 640, 360, 820);
  cairo_pattern_add_color_stop_rgba(v, 0, 0.35, 0.22, 0.1, 0.0);
  cairo_pattern_add_color_stop_rgba(v, 1, 0.35, 0.22, 0.1, 0.32);
  cairo_set_source(cr, v);
  cairo_fill(cr);
  cairo_pattern_destroy(v);
  return img.toTexture(r, 0.0f, 0.0f);
}

// Far: two bands of pastel hills, inked along the top: lavender behind with
// a windmill, pale green in front with lollipop trees and a red barn.
Texture bakeToonFar(const Renderer& r, const Theme& t, int layerW)
{
  VectorImage img(layerW, kScreenH);
  cairo_t* cr = img.cr();
  const double W = layerW;
  Rng rng(4253u);
  auto back = [&](double x) { return hillY(x, W, 470, {{38, 2, 0.4}, {22, 5, 1.3}, {12, 9, 2.1}}); };
  auto front = [&](double x) { return hillY(x, W, 548, {{30, 3, 0.9}, {18, 7, 1.7}, {8, 13, 0.2}}); };
  const Color lav = lerpColor(t.farLayer, rgb(196, 182, 226), 0.65f);
  hillBand(cr, W, back, lav, darken(lav, 0.12f), 2.6);
  // A windmill on the back hills.
  {
    const double wx = 1820, wy = back(wx) + 8;
    cairo_move_to(cr, wx - 22, wy);
    cairo_line_to(cr, wx - 12, wy - 92);
    cairo_line_to(cr, wx + 12, wy - 92);
    cairo_line_to(cr, wx + 22, wy);
    cairo_close_path(cr);
    fillInk(cr, rgb(246, 236, 214), 2.4);
    cairo_move_to(cr, wx - 15, wy - 92);
    cairo_line_to(cr, wx, wy - 112);
    cairo_line_to(cr, wx + 15, wy - 92);
    cairo_close_path(cr);
    fillInk(cr, rgb(200, 90, 80), 2.4);
    for (int i = 0; i < 4; ++i)
    {
      const double a = 0.3 + i * kPi / 2;
      cairo_save(cr);
      cairo_translate(cr, wx, wy - 96);
      cairo_rotate(cr, a);
      cairo_rectangle(cr, 4, -7, 54, 14);
      fillInk(cr, rgb(250, 244, 226), 2.0);
      for (int k = 1; k < 4; ++k)
      {
        cairo_move_to(cr, 4 + k * 13.5, -7);
        cairo_line_to(cr, 4 + k * 13.5, 7);
      }
      inkStroke(cr, kInk, 1.0, 0.7);
      cairo_restore(cr);
    }
    circle(cr, wx, wy - 96, 5);
    fillInk(cr, rgb(200, 90, 80), 2.0);
  }
  hillBand(cr, W, front, t.farLayer, darken(t.farLayer, 0.15f), 2.8);
  // A red barn on the front hills.
  {
    const double bx = 700, by = front(bx) + 10;
    cairo_move_to(cr, bx - 44, by);
    cairo_line_to(cr, bx - 44, by - 50);
    cairo_line_to(cr, bx - 30, by - 74);
    cairo_line_to(cr, bx, by - 88);
    cairo_line_to(cr, bx + 30, by - 74);
    cairo_line_to(cr, bx + 44, by - 50);
    cairo_line_to(cr, bx + 44, by);
    cairo_close_path(cr);
    fillInk(cr, rgb(214, 86, 74), 2.6);
    cairo_rectangle(cr, bx - 16, by - 36, 32, 36);
    fillInk(cr, rgb(250, 240, 220), 2.0);
    cairo_move_to(cr, bx - 16, by - 36);
    cairo_line_to(cr, bx + 16, by);
    cairo_move_to(cr, bx + 16, by - 36);
    cairo_line_to(cr, bx - 16, by);
    inkStroke(cr, kInk, 1.8);
    circle(cr, bx, by - 58, 7);
    fillInk(cr, rgb(250, 240, 220), 1.8);
  }
  // Lollipop trees along the front hills.
  for (int i = 0; i < 16; ++i)
  {
    const double x = 60 + i * (W - 120) / 16.0 + rng.range(-30, 30);
    if (std::abs(x - 700) < 70)
      continue;
    const double s = rng.range(0.7f, 1.05f);
    lollipop(cr, x, front(x) + 6, s, i % 3 ? lerpColor(t.farLayer, rgb(110, 170, 100), 0.6f) : rgb(150, 190, 110));
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// Near: rounder green hills inked thick, bushes, daisies and runs of white
// picket fence.
Texture bakeToonNear(const Renderer& r, const Theme& t, int layerW)
{
  VectorImage img(layerW, kScreenH);
  cairo_t* cr = img.cr();
  const double W = layerW;
  Rng rng(4261u);
  auto top = [&](double x) { return hillY(x, W, 612, {{34, 2, 2.2}, {20, 5, 0.3}, {9, 11, 1.1}}); };
  hillBand(cr, W, top, t.nearLayer, darken(t.nearLayer, 0.2f), 3.2);
  // Daisies.
  for (int i = 0; i < 90; ++i)
  {
    const double x = rng.range(10, float(W) - 10), y = top(x) + rng.range(16, 90);
    for (int p = 0; p < 5; ++p)
    {
      const double a = p * 2 * kPi / 5;
      circle(cr, x + std::cos(a) * 3.2, y + std::sin(a) * 3.2, 2.4);
    }
    setRgba(cr, i % 4 ? rgb(255, 252, 240) : rgb(255, 190, 200), 0.9);
    cairo_fill(cr);
    circle(cr, x, y, 1.8);
    setRgba(cr, rgb(250, 200, 60), 1.0);
    cairo_fill(cr);
  }
  // Runs of picket fence along the hilltops.
  for (const double fx : {260.0, 1180.0, 2040.0})
  {
    const int n = 9;
    std::vector<std::pair<double, double>> posts;
    for (int i = 0; i < n; ++i)
    {
      const double x = fx + i * 22;
      posts.push_back({x, top(x) + 8});
    }
    for (const double ry : {16.0, 30.0})
    {
      cairo_move_to(cr, posts.front().first - 6, posts.front().second - ry);
      for (const auto& p : posts)
        cairo_line_to(cr, p.first, p.second - ry);
      cairo_line_to(cr, posts.back().first + 6, posts.back().second - ry);
      setRgba(cr, kInk, 1.0);
      cairo_set_line_width(cr, 6.5);
      cairo_stroke_preserve(cr);
      setRgba(cr, rgb(246, 240, 226), 1.0);
      cairo_set_line_width(cr, 3.2);
      cairo_stroke(cr);
    }
    for (const auto& p : posts)
    {
      const double x = p.first, y = p.second;
      cairo_move_to(cr, x - 6, y);
      cairo_line_to(cr, x - 6, y - 38);
      cairo_line_to(cr, x, y - 46);
      cairo_line_to(cr, x + 6, y - 38);
      cairo_line_to(cr, x + 6, y);
      cairo_close_path(cr);
      fillInk(cr, rgb(250, 246, 234), 2.0);
    }
  }
  // Bushes: three inked lumps, a shine on each.
  for (int i = 0; i < 12; ++i)
  {
    const double x = 90 + i * (W - 180) / 12.0 + rng.range(-40, 40), y = top(x) + 10;
    const double s = rng.range(0.8f, 1.2f);
    const Color leaf = lerpColor(t.nearLayer, rgb(80, 140, 80), 0.5f);
    cairo_new_path(cr);
    for (const auto& l : {std::array<double, 3>{-20, -14, 16}, std::array<double, 3>{20, -14, 16}, std::array<double, 3>{0, -24, 20}})
      circle(cr, x + l[0] * s, y + l[1] * s, l[2] * s);
    setRgba(cr, kInk, 1.0);
    cairo_set_line_width(cr, 6);
    cairo_stroke_preserve(cr);
    setRgba(cr, leaf, 1.0);
    cairo_fill(cr);
    for (const auto& l : {std::array<double, 3>{-24, -20, 4}, std::array<double, 3>{-4, -34, 5}, std::array<double, 3>{16, -20, 4}})
    {
      ellipse(cr, x + l[0] * s, y + l[1] * s, l[2] * s * 1.4, l[2] * s * 0.8, -0.5);
      setRgba(cr, lighten(leaf, 0.45f), 0.8);
      cairo_fill(cr);
    }
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

} // namespace gr
