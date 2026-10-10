// Level 25, Tiny Kingdom (theme look "garden"): a back garden filmed at ant
// height in golden light. The masses are dark loam, crumbly, with pebbles
// like boulders and roots threading through; where you stand, a turf of
// grass blades rim-lit gold; the one-way floors are broad leaves with their
// veins; climbables are green stalks with leaf nubs to hold; spikes are
// bramble thorns. Behind: a golden sky full of soft bokeh and light shafts,
// giant out-of-focus grass blades and flower heads far off, and nearer the
// garden fence's weathered planks with clover and dew drops in front.

#include "assets/art_garden.hpp"

#include "base/math.hpp"
#include "render/vector.hpp"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <string_view>

namespace gr
{

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr Color kInk = rgb(34, 26, 16);

Color lighten(Color c, float t) { return lerpColor(c, rgb(255, 255, 255), t); }
Color darken(Color c, float t) { return lerpColor(c, rgb(0, 0, 0), t); }

void stop(cairo_pattern_t* g, double at, Color c, double a = 1.0)
{
  cairo_pattern_add_color_stop_rgba(g, at, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, a);
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

void setRgba(cairo_t* cr, Color c, double a)
{
  cairo_set_source_rgba(cr, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, a);
}

// Fills the current path with a linear gradient through `stops`.
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
  cairo_stroke(cr);
}

// A tapered blade from its base (x, y) up `len`, its tip pushed over by
// `bend`, `width` across at the base. Leaves the path.
void bladePath(cairo_t* cr, double x, double y, double len, double bend, double width)
{
  const double tx = x + bend, ty = y - len;
  cairo_move_to(cr, x - width * 0.5, y);
  cairo_curve_to(cr, x - width * 0.45, y - len * 0.45, tx - bend * 0.35 - width * 0.12, ty + len * 0.25, tx, ty);
  cairo_curve_to(cr, tx - bend * 0.3 + width * 0.2, ty + len * 0.3, x + width * 0.5, y - len * 0.4, x + width * 0.5, y);
  cairo_close_path(cr);
}

// A blade filled dark at the base to light at the tip, rim-lit gold on its
// left (the sun's) edge.
void blade(cairo_t* cr, double x, double y, double len, double bend, double width, Color base, Color tip, Color rim,
  double alpha = 1.0)
{
  bladePath(cr, x, y, len, bend, width);
  fillLinear(cr, x, y, x + bend, y - len, {{0.0, base}, {1.0, tip}}, alpha);
  // The rim: a thin bright line up the left edge.
  const double tx = x + bend, ty = y - len;
  cairo_move_to(cr, x - width * 0.42, y - len * 0.1);
  cairo_curve_to(cr, x - width * 0.4, y - len * 0.45, tx - bend * 0.35 - width * 0.1, ty + len * 0.25, tx, ty);
  setRgba(cr, rim, 0.55 * alpha);
  cairo_set_line_width(cr, std::max(0.8, width * 0.12));
  cairo_stroke(cr);
  // The midrib.
  cairo_move_to(cr, x, y);
  cairo_curve_to(cr, x, y - len * 0.45, tx - bend * 0.35, ty + len * 0.25, tx, ty);
  setRgba(cr, darken(base, 0.25f), 0.35 * alpha);
  cairo_set_line_width(cr, std::max(0.6, width * 0.06));
  cairo_stroke(cr);
}

// A pebble half sunk in the loam: a smooth stone, lit from the upper left.
void pebble(cairo_t* cr, double x, double y, double rx, double ry, double angle, Color c)
{
  ellipse(cr, x + rx * 0.15, y + ry * 0.25, rx * 1.02, ry * 1.0, angle);
  setRgba(cr, rgb(20, 12, 6), 0.45);
  cairo_fill(cr);
  ellipse(cr, x, y, rx, ry, angle);
  cairo_pattern_t* g = cairo_pattern_create_radial(x - rx * 0.4, y - ry * 0.45, std::min(rx, ry) * 0.1, x, y, std::max(rx, ry));
  stop(g, 0, lighten(c, 0.45f));
  stop(g, 0.55, c);
  stop(g, 1, darken(c, 0.45f));
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  setRgba(cr, darken(c, 0.6f), 0.7);
  cairo_set_line_width(cr, 0.9);
  cairo_stroke(cr);
  ellipse(cr, x - rx * 0.38, y - ry * 0.42, rx * 0.28, ry * 0.16, angle - 0.4);
  setRgba(cr, rgb(255, 250, 230), 0.55);
  cairo_fill(cr);
}

// A drop of dew: clear, the light it bends pooling at its foot, a sharp
// highlight at its top left and a dark rim.
void dewDrop(cairo_t* cr, double x, double y, double r, Color behind)
{
  ellipse(cr, x + r * 0.2, y + r * 0.85, r * 0.9, r * 0.25);
  setRgba(cr, darken(behind, 0.4f), 0.35);
  cairo_fill(cr);
  circle(cr, x, y, r);
  cairo_pattern_t* g = cairo_pattern_create_radial(x + r * 0.1, y + r * 0.35, r * 0.1, x, y, r);
  stop(g, 0, lighten(behind, 0.7f), 0.85);
  stop(g, 0.6, lighten(behind, 0.25f), 0.45);
  stop(g, 1, darken(behind, 0.35f), 0.75);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  circle(cr, x, y, r);
  setRgba(cr, darken(behind, 0.5f), 0.5);
  cairo_set_line_width(cr, std::max(0.7, r * 0.08));
  cairo_stroke(cr);
  ellipse(cr, x - r * 0.38, y - r * 0.42, r * 0.28, r * 0.18, -0.6);
  setRgba(cr, rgb(255, 255, 255), 0.95);
  cairo_fill(cr);
  circle(cr, x + r * 0.35, y + r * 0.45, r * 0.08);
  setRgba(cr, rgb(255, 255, 255), 0.7);
  cairo_fill(cr);
}

// A soft out-of-focus disc of light, brighter at its rim (lens bokeh).
void bokeh(cairo_t* cr, double x, double y, double r, Color c, double a)
{
  circle(cr, x, y, r);
  cairo_pattern_t* g = cairo_pattern_create_radial(x, y, 0, x, y, r);
  stop(g, 0, c, a * 0.75);
  stop(g, 0.82, c, a);
  stop(g, 0.92, lighten(c, 0.3f), a * 1.25);
  stop(g, 1, c, 0.0);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
}

void glowAt(cairo_t* cr, double x, double y, double r, Color c, double a)
{
  circle(cr, x, y, r);
  cairo_pattern_t* g = cairo_pattern_create_radial(x, y, 0, x, y, r);
  stop(g, 0, c, a);
  stop(g, 0.4, c, a * 0.45);
  stop(g, 1, c, 0.0);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
}

// A clover leaf: three heart-shaped leaflets round (x, y), `s` px each,
// turned by `a`, with the pale chevron marks.
void clover(cairo_t* cr, double x, double y, double s, double a, Color c, double tilt)
{
  for (int i = 0; i < 3; ++i)
  {
    const double ang = a + i * 2 * kPi / 3 - kPi / 2;
    cairo_save(cr);
    cairo_translate(cr, x, y);
    cairo_scale(cr, 1.0, tilt);
    cairo_rotate(cr, ang);
    // A heart pointing in at the middle, its notch out at (s, 0).
    cairo_move_to(cr, 0, 0);
    cairo_curve_to(cr, s * 0.3, -s * 0.55, s * 1.05, -s * 0.75, s * 1.0, -s * 0.12);
    cairo_curve_to(cr, s * 0.98, -s * 0.02, s * 0.9, 0, s * 0.82, 0);
    cairo_curve_to(cr, s * 0.9, 0, s * 0.98, s * 0.02, s * 1.0, s * 0.12);
    cairo_curve_to(cr, s * 1.05, s * 0.75, s * 0.3, s * 0.55, 0, 0);
    cairo_close_path(cr);
    cairo_pattern_t* g = cairo_pattern_create_linear(0, -s * 0.6, s, s * 0.6);
    stop(g, 0, lighten(c, 0.3f));
    stop(g, 1, darken(c, 0.2f));
    cairo_set_source(cr, g);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(g);
    setRgba(cr, darken(c, 0.45f), 0.6);
    cairo_set_line_width(cr, 1.2);
    cairo_stroke(cr);
    // The chevron.
    cairo_move_to(cr, s * 0.35, -s * 0.3);
    cairo_line_to(cr, s * 0.55, 0);
    cairo_line_to(cr, s * 0.35, s * 0.3);
    setRgba(cr, lighten(c, 0.55f), 0.55);
    cairo_set_line_width(cr, s * 0.07);
    cairo_stroke(cr);
    cairo_move_to(cr, 0, 0);
    cairo_line_to(cr, s * 0.8, 0);
    setRgba(cr, darken(c, 0.3f), 0.4);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);
    cairo_restore(cr);
  }
}

// A flower head out of focus on its stem (the far layer): 0 a daisy, 1 a
// poppy, 2 a cornflower, 3 a dandelion clock.
void farFlower(cairo_t* cr, double x, double y, double s, int kind, Color haze, double hz)
{
  auto hazed = [&](Color c) { return lerpColor(c, haze, float(hz)); };
  glowAt(cr, x, y, s * 2.0, hazed(rgb(255, 240, 200)), 0.18);
  if (kind == 0)
  {
    for (int i = 0; i < 16; ++i)
    {
      const double a = i * 2 * kPi / 16;
      ellipse(cr, x + std::cos(a) * s * 0.62, y + std::sin(a) * s * 0.3, s * 0.42, s * 0.11, a * 0.98);
      setRgba(cr, hazed(i % 2 ? rgb(255, 252, 244) : rgb(240, 236, 230)), 0.9);
      cairo_fill(cr);
    }
    ellipse(cr, x, y - s * 0.04, s * 0.26, s * 0.16);
    setRgba(cr, hazed(rgb(250, 196, 40)), 1.0);
    cairo_fill(cr);
  }
  else if (kind == 1)
  {
    for (int i = 0; i < 4; ++i)
    {
      const double a = -kPi / 2 + (i - 1.5) * 0.7;
      ellipse(cr, x + std::cos(a) * s * 0.35, y + std::sin(a) * s * 0.25 + s * 0.1, s * 0.55, s * 0.42, a);
      setRgba(cr, hazed(i % 2 ? rgb(236, 70, 40) : rgb(214, 46, 30)), 0.92);
      cairo_fill(cr);
    }
    ellipse(cr, x, y + s * 0.1, s * 0.16, s * 0.1);
    setRgba(cr, hazed(rgb(40, 30, 30)), 0.9);
    cairo_fill(cr);
  }
  else if (kind == 2)
  {
    for (int i = 0; i < 10; ++i)
    {
      const double a = -kPi + i * kPi / 9;
      cairo_move_to(cr, x, y);
      cairo_line_to(cr, x + std::cos(a - 0.12) * s * 0.75, y + std::sin(a - 0.12) * s * 0.55);
      cairo_line_to(cr, x + std::cos(a) * s * 0.9, y + std::sin(a) * s * 0.65);
      cairo_line_to(cr, x + std::cos(a + 0.12) * s * 0.75, y + std::sin(a + 0.12) * s * 0.55);
      cairo_close_path(cr);
      setRgba(cr, hazed(rgb(80, 120, 230)), 0.9);
      cairo_fill(cr);
    }
  }
  else
  {
    for (int i = 0; i < 40; ++i)
    {
      const double a = i * 2 * kPi / 40;
      cairo_move_to(cr, x, y);
      cairo_line_to(cr, x + std::cos(a) * s * 0.85, y + std::sin(a) * s * 0.85);
    }
    setRgba(cr, hazed(rgb(255, 255, 250)), 0.35);
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);
    glowAt(cr, x, y, s * 0.95, hazed(rgb(255, 255, 255)), 0.35);
  }
}

} // namespace

bool isGarden(const Theme& t) { return std::string_view(t.look) == "garden"; }

// The loam: dark, crumbly, flecked; variant 1 a root threading down through
// it, variant 2 a big pebble. Variant 0 has two small stones.
Texture bakeGardenSolid(const Renderer& r, const Theme& t, int variant)
{
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  Rng rng(std::uint32_t(variant * 7919 + 2521));
  cairo_rectangle(cr, 0, 0, 64, 64);
  fillLinear(cr, 0, 0, 64, 64, {{0.0, lerpColor(t.rock, t.rockLight, 0.18f)}, {0.6, t.rock}, {1.0, darken(t.rock, 0.12f)}});
  // Crumbs, light and dark, wrapped so the blocks tile.
  for (int i = 0; i < 34; ++i)
  {
    const bool light = i % 3 == 0;
    const double x = rng.range(0, 64), y = rng.range(0, 64), rr = light ? rng.range(0.8f, 2.0f) : rng.range(1.0f, 3.4f);
    for (const double ox : {-64.0, 0.0, 64.0})
      for (const double oy : {-64.0, 0.0, 64.0})
        ellipse(cr, x + ox, y + oy, rr, rr * 0.75, rng.range(0, 3));
    setRgba(cr, light ? lerpColor(t.rockLight, rgb(214, 170, 120), 0.4f) : t.rockDark, light ? 0.32 : 0.45);
    cairo_fill(cr);
  }
  // Fine rootlets (not on the plain block, the one most of the soil is).
  for (int i = 0; i < (variant == 0 ? 0 : 2); ++i)
  {
    const double x = rng.range(4, 60), y = rng.range(4, 60);
    cairo_move_to(cr, x, y);
    cairo_curve_to(cr, x + rng.range(-8, 8), y + 5, x + rng.range(-8, 8), y + 9, x + rng.range(-10, 10), y + 14);
    inkStroke(cr, rgb(214, 186, 140), 0.7, 0.35);
  }
  // Grit sparkles.
  for (int i = 0; i < 5; ++i)
  {
    circle(cr, rng.range(3, 61), rng.range(3, 61), 0.8);
    setRgba(cr, rgb(255, 236, 190), 0.5);
    cairo_fill(cr);
  }
  if (variant == 1)
  {
    // A root coming down from above, tapering to a hairy tip.
    cairo_move_to(cr, 14, -2);
    cairo_curve_to(cr, 18, 16, 34, 20, 38, 34);
    cairo_curve_to(cr, 41, 44, 46, 50, 50, 58);
    cairo_curve_to(cr, 48, 50, 38, 46, 34, 36);
    cairo_curve_to(cr, 30, 26, 10, 18, 7, -2);
    cairo_close_path(cr);
    fillLinear(cr, 0, 0, 12, 4, {{0.0, rgb(226, 196, 150)}, {1.0, rgb(150, 112, 70)}}, 1.0, true);
    inkStroke(cr, rgb(80, 52, 30), 0.9, 0.8);
    for (int k = 0; k < 7; ++k)
    {
      const double u = 0.15 + k * 0.12;
      const double px = 10 + u * 40, py = u * 58;
      cairo_move_to(cr, px, py);
      cairo_line_to(cr, px + (k % 2 ? 6 : -5), py + 4);
    }
    inkStroke(cr, rgb(220, 196, 150), 0.6, 0.6);
    cairo_move_to(cr, 12, 2);
    cairo_curve_to(cr, 17, 16, 30, 22, 35, 33);
    inkStroke(cr, rgb(255, 236, 196), 0.8, 0.45);
  }
  else if (variant == 2)
  {
    pebble(cr, 34, 36, 15, 10, -0.15, rgb(140, 146, 150));
    pebble(cr, 54, 56, 5, 3.5, 0.3, rgb(176, 150, 118));
    circle(cr, 40, 30, 1.2);
    setRgba(cr, rgb(255, 255, 255), 0.8);
    cairo_fill(cr);
    pebble(cr, 8, 12, 4, 3, 0.4, rgb(186, 160, 120));
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// Where you stand: a turf of grass, the blades rim-lit gold, roots in the
// sod and its shadow on the loam under it.
Texture bakeGardenSolidTop(const Renderer& r, const Theme& t, float topOffset, int height)
{
  VectorImage img(64, height);
  cairo_t* cr = img.cr();
  const double y = topOffset;
  Rng rng(2531u);
  {
    cairo_pattern_t* g = cairo_pattern_create_linear(0, y + 9, 0, y + 23);
    cairo_pattern_add_color_stop_rgba(g, 0, 0.08, 0.05, 0.02, 0.5);
    cairo_pattern_add_color_stop_rgba(g, 1, 0.08, 0.05, 0.02, 0.0);
    cairo_rectangle(cr, 0, y + 9, 64, 14);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  // The sod: a band of dark green going brown, its top edge tufted.
  cairo_move_to(cr, 0, y + 11);
  cairo_line_to(cr, 0, y - 1);
  for (int i = 0; i < 8; ++i)
    cairo_curve_to(cr, i * 8 + 2, y - 3, i * 8 + 6, y - 3, i * 8 + 8, y - 1);
  cairo_line_to(cr, 64, y + 11);
  for (int i = 8; i > 0; --i)
    cairo_curve_to(cr, i * 8 - 2, y + 13, i * 8 - 6, y + 13, i * 8 - 8, y + 11);
  cairo_close_path(cr);
  fillLinear(cr, 0, y - 3, 0, y + 13, {{0.0, darken(t.trim, 0.15f)}, {0.5, darken(t.trim, 0.5f)}, {1.0, lerpColor(t.rock, t.rockDark, 0.4f)}});
  for (int i = 0; i < 6; ++i)
  {
    const double x = rng.range(2, 62);
    cairo_move_to(cr, x, y + 6);
    cairo_curve_to(cr, x + 2, y + 9, x - 2, y + 12, x + rng.range(-3, 3), y + 15);
    inkStroke(cr, rgb(214, 190, 140), 0.7, 0.45);
  }
  // Blades: a back row, darker and shorter, then the front row.
  for (int pass = 0; pass < 2; ++pass)
  {
    const int n = pass == 0 ? 11 : 9;
    for (int i = 0; i < n; ++i)
    {
      const double x = (i + rng.range(0.1f, 0.9f)) * 64.0 / n;
      const double len = pass == 0 ? rng.range(10, 20) : rng.range(14, 28);
      const double bend = rng.range(-7, 7), w = pass == 0 ? rng.range(2.5f, 3.5f) : rng.range(3.0f, 4.6f);
      const Color base = pass == 0 ? darken(t.trim, 0.45f) : darken(t.trim, 0.25f);
      const Color tip = pass == 0 ? lerpColor(t.trim, t.trimGlow, 0.15f) : lerpColor(t.trim, t.trimGlow, 0.45f);
      for (const double ox : {-64.0, 0.0, 64.0})
        blade(cr, x + ox, y + 3, len, bend, w, base, tip, rgb(255, 226, 130));
    }
  }
  // A tiny dew drop on a blade.
  dewDrop(cr, 41, y - 8, 2.4, t.trim);
  return img.toTexture(r, 0.0f, topOffset);
}

// A one-way floor: a broad leaf seen edge on, the veins fanning from its
// midrib, its underside scalloped and in shade.
Texture bakeGardenPlatform(const Renderer& r, const Theme& t)
{
  VectorImage img(64, 40);
  cairo_t* cr = img.cr();
  // The shadow it throws.
  cairo_rectangle(cr, 0, 14, 64, 10);
  {
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 14, 0, 24);
    cairo_pattern_add_color_stop_rgba(g, 0, 0.05, 0.08, 0.02, 0.32);
    cairo_pattern_add_color_stop_rgba(g, 1, 0.05, 0.08, 0.02, 0.0);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  cairo_move_to(cr, 0, 1);
  cairo_curve_to(cr, 20, 0, 44, 0, 64, 1);
  cairo_line_to(cr, 64, 11);
  cairo_curve_to(cr, 56, 16.5, 40, 16.5, 32, 11.5);
  cairo_curve_to(cr, 24, 16.5, 8, 16.5, 0, 11);
  cairo_close_path(cr);
  fillLinear(cr, 0, 0, 0, 16, {{0.0, lighten(t.platform, 0.4f)}, {0.35, t.platform}, {1.0, t.platformDark}}, 1.0, true);
  setRgba(cr, darken(t.platformDark, 0.45f), 0.8);
  cairo_set_line_width(cr, 1.0);
  cairo_stroke(cr);
  // The midrib and the veins.
  cairo_move_to(cr, 0, 4.5);
  cairo_line_to(cr, 64, 4.5);
  inkStroke(cr, lighten(t.platform, 0.55f), 1.3, 0.7);
  for (int i = 0; i < 4; ++i)
  {
    const double x = i * 16 + 2;
    cairo_move_to(cr, x, 4.5);
    cairo_curve_to(cr, x + 4, 7, x + 8, 10, x + 13, 13.5);
    cairo_move_to(cr, x + 6, 4.5);
    cairo_curve_to(cr, x + 9, 3, x + 11, 2, x + 14, 1.2);
  }
  inkStroke(cr, lighten(t.platform, 0.45f), 0.8, 0.45);
  // The sun on its top edge.
  cairo_move_to(cr, 0, 1.2);
  cairo_curve_to(cr, 20, 0.2, 44, 0.2, 64, 1.2);
  inkStroke(cr, rgb(255, 240, 170), 1.3, 0.75);
  return img.toTexture(r, 0.0f, 0.0f);
}

// A climbable: a green stalk with leaf nubs to hold (drawn 16 px left of
// its block, so its middle is the left cell's middle).
Texture bakeGardenLadder(const Renderer& r, const Theme& t)
{
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  const Color stalk = lerpColor(t.trim, t.platformDark, 0.3f);
  // The leaf nubs: one to each side.
  for (int side = -1; side <= 1; side += 2)
  {
    const double y = side < 0 ? 16 : 48;
    const double x0 = 32 + side * 5;
    cairo_move_to(cr, x0, y + 4);
    cairo_curve_to(cr, x0 + side * 6, y - 4, x0 + side * 14, y - 8, x0 + side * 22, y - 10);
    cairo_curve_to(cr, x0 + side * 16, y - 1, x0 + side * 8, y + 4, x0, y + 4);
    cairo_close_path(cr);
    fillLinear(cr, x0, y - 10, x0, y + 4, {{0.0, lighten(t.platform, 0.35f)}, {1.0, t.platformDark}}, 1.0, true);
    inkStroke(cr, darken(t.platformDark, 0.5f), 1.0, 0.8);
    cairo_move_to(cr, x0, y + 3);
    cairo_curve_to(cr, x0 + side * 8, y - 2, x0 + side * 14, y - 5, x0 + side * 20, y - 9);
    inkStroke(cr, lighten(t.platform, 0.5f), 0.8, 0.6);
  }
  // The stalk.
  cairo_rectangle(cr, 25, -1, 14, 66);
  fillLinear(cr, 25, 0, 39, 0, {{0.0, lighten(stalk, 0.55f)}, {0.25, lighten(stalk, 0.15f)}, {0.7, stalk}, {1.0, darken(stalk, 0.45f)}});
  for (const double x : {29.0, 33.0, 36.0})
  {
    cairo_move_to(cr, x, 0);
    cairo_line_to(cr, x, 64);
  }
  inkStroke(cr, darken(stalk, 0.3f), 0.6, 0.4);
  cairo_move_to(cr, 25, -1);
  cairo_line_to(cr, 25, 65);
  cairo_move_to(cr, 39, -1);
  cairo_line_to(cr, 39, 65);
  inkStroke(cr, darken(stalk, 0.6f), 1.2, 0.9);
  cairo_move_to(cr, 26.5, 0);
  cairo_line_to(cr, 26.5, 64);
  inkStroke(cr, rgb(255, 230, 150), 1.0, 0.6);
  // A node.
  ellipse(cr, 32, 62, 8.5, 2.6);
  fillLinear(cr, 24, 0, 40, 0, {{0.0, lighten(stalk, 0.4f)}, {1.0, darken(stalk, 0.4f)}}, 1.0, true);
  inkStroke(cr, darken(stalk, 0.6f), 0.8, 0.8);
  return img.toTexture(r, 16.0f, 0.0f);
}

// Spikes: a bramble along the ground, its hooked thorns pointing up.
Texture bakeGardenSpikes(const Renderer& r, const Theme& t)
{
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  const Color cane = lerpColor(t.hazard, rgb(90, 110, 50), 0.35f);
  // Thorns first, so the cane covers their bases.
  struct Thorn
  {
    double x, h, lean, w;
  };
  const Thorn thorns[] = {{8, 30, -4, 11}, {24, 36, 3, 13}, {40, 28, -2, 11}, {55, 34, 5, 12}, {16, 18, 6, 7}, {48, 16, -6, 7}};
  for (const auto& th : thorns)
  {
    const double by = 54, tx = th.x + th.lean, ty = by - th.h;
    cairo_move_to(cr, th.x - th.w * 0.5, by);
    cairo_curve_to(cr, th.x - th.w * 0.3, by - th.h * 0.5, tx - th.lean * 0.2, ty + 6, tx, ty);
    cairo_curve_to(cr, tx + th.w * 0.1, ty + 8, th.x + th.w * 0.4, by - th.h * 0.4, th.x + th.w * 0.5, by);
    cairo_close_path(cr);
    fillLinear(cr, 0, by, 0, ty, {{0.0, darken(t.hazard, 0.3f)}, {0.6, t.hazard}, {1.0, t.hazardLight}}, 1.0, true);
    inkStroke(cr, kInk, 1.4);
    cairo_move_to(cr, th.x - th.w * 0.22, by - 4);
    cairo_curve_to(cr, th.x - th.w * 0.15, by - th.h * 0.5, tx - th.lean * 0.2 - 1, ty + 7, tx - 0.5, ty + 2);
    inkStroke(cr, rgb(255, 220, 190), 1.0, 0.55);
  }
  // The cane, wavy, and a leaf.
  cairo_move_to(cr, 0, 54);
  cairo_curve_to(cr, 16, 50, 24, 60, 32, 56);
  cairo_curve_to(cr, 40, 52, 50, 60, 64, 54);
  cairo_line_to(cr, 64, 63);
  cairo_curve_to(cr, 50, 67, 40, 60, 32, 64);
  cairo_curve_to(cr, 24, 68, 16, 59, 0, 63);
  cairo_close_path(cr);
  fillLinear(cr, 0, 50, 0, 66, {{0.0, lighten(cane, 0.3f)}, {1.0, darken(cane, 0.35f)}}, 1.0, true);
  inkStroke(cr, kInk, 1.4);
  cairo_move_to(cr, 30, 56);
  cairo_curve_to(cr, 36, 46, 46, 44, 52, 46);
  cairo_curve_to(cr, 46, 52, 38, 56, 30, 56);
  cairo_close_path(cr);
  fillLinear(cr, 30, 44, 30, 56, {{0.0, rgb(130, 170, 80)}, {1.0, rgb(60, 100, 44)}}, 1.0, true);
  inkStroke(cr, kInk, 1.0, 0.8);
  return img.toTexture(r, 0.0f, 0.0f);
}

// The sky: golden afternoon light, a warm haze low down, light shafts
// slanting in and the bokeh of sunlit dew and pollen out of focus.
Texture bakeGardenSky(const Renderer& r, const Theme& t)
{
  VectorImage img(kScreenW, kScreenH);
  cairo_t* cr = img.cr();
  cairo_rectangle(cr, 0, 0, kScreenW, kScreenH);
  fillLinear(cr, 0, 0, 0, kScreenH, {{0.0, t.skyTop}, {0.55, t.skyMid}, {1.0, t.skyBottom}});
  Rng rng(2539u);
  glowAt(cr, 1040, 470, 760, rgb(255, 214, 120), 0.45);
  glowAt(cr, 1040, 470, 260, rgb(255, 240, 200), 0.35);
  // Light shafts from the upper right.
  for (int i = 0; i < 6; ++i)
  {
    const double x = 380 + i * 170 + rng.range(-40, 40), w = rng.range(40, 110);
    cairo_move_to(cr, x + 520, -10);
    cairo_line_to(cr, x + 520 + w, -10);
    cairo_line_to(cr, x + w * 2.4, kScreenH + 10);
    cairo_line_to(cr, x, kScreenH + 10);
    cairo_close_path(cr);
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 0, 0, kScreenH);
    stop(g, 0, rgb(255, 246, 210), 0.12);
    stop(g, 1, rgb(255, 246, 210), 0.0);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  // Bokeh: warm discs of every size, more of them low and in the light.
  const Color tones[5] = {rgb(255, 236, 170), rgb(255, 250, 230), rgb(255, 196, 120), rgb(255, 190, 220), rgb(210, 255, 200)};
  for (int i = 0; i < 70; ++i)
  {
    const double x = rng.range(-40, float(kScreenW) + 40), y = rng.range(40, float(kScreenH) - 60);
    const double rad = i < 12 ? rng.range(50, 90) : rng.range(8, 46);
    const double a = i < 12 ? rng.range(0.04f, 0.08f) : rng.range(0.06f, 0.2f);
    bokeh(cr, x, y, rad, tones[i % 5], a);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// Far: giant grass blades out of focus, soft in the haze, and flower heads
// nodding on long stems over them.
Texture bakeGardenFar(const Renderer& r, const Theme& t, int layerW)
{
  VectorImage img(layerW, kScreenH);
  cairo_t* cr = img.cr();
  Rng rng(2543u);
  const double W = layerW;
  const Color haze = t.skyMid;
  // The back row, faint; then the front row of blades.
  for (int pass = 0; pass < 2; ++pass)
  {
    const int n = pass == 0 ? 44 : 34;
    for (int i = 0; i < n; ++i)
    {
      const double x = rng.range(0, float(W)), len = pass == 0 ? rng.range(260, 520) : rng.range(340, 660);
      const double bend = rng.range(-140, 140), w = pass == 0 ? rng.range(26, 50) : rng.range(36, 76);
      const float hz = pass == 0 ? 0.62f : 0.42f;
      const Color base = lerpColor(darken(t.farLayer, 0.2f), haze, hz), tip = lerpColor(lighten(t.farLayer, 0.25f), haze, hz);
      for (const double ox : {-W, 0.0, W})
      {
        if (x + ox + std::abs(bend) + w < 0 || x + ox - std::abs(bend) - w > W)
          continue;
        // Out of focus: a wide faint halo, then the blade.
        blade(cr, x + ox, kScreenH + 10, len + 14, bend, w * 1.5, base, tip, rgb(255, 230, 150), 0.25);
        blade(cr, x + ox, kScreenH + 10, len, bend, w, base, tip, rgb(255, 230, 150), 0.85);
      }
    }
    if (pass == 0)
    {
      // Flower heads between the rows.
      for (int i = 0; i < 9; ++i)
      {
        const double x = (i + rng.range(0.2f, 0.8f)) * W / 9, y = rng.range(150, 330), s = rng.range(48, 90);
        const int kind = i % 4;
        for (const double ox : {-W, 0.0, W})
        {
          if (x + ox + s * 2 < 0 || x + ox - s * 2 > W)
            continue;
          cairo_move_to(cr, x + ox + rng.range(-30, 30), kScreenH + 10);
          cairo_curve_to(cr, x + ox - 20, 560, x + ox + 24, y + 140, x + ox, y + s * 0.2);
          setRgba(cr, lerpColor(t.farLayer, haze, 0.5f), 0.85);
          cairo_set_line_width(cr, s * 0.12);
          cairo_stroke(cr);
          farFlower(cr, x + ox, y, s, kind, haze, 0.35);
        }
      }
    }
  }
  // The haze pooling low.
  {
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 420, 0, 720);
    stop(g, 0, t.skyBottom, 0.0);
    stop(g, 1, t.skyBottom, 0.45);
    cairo_rectangle(cr, 0, 420, W, 300);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// Near: the garden fence's weathered planks (a run of them, gaps between),
// clover in front on its stalks, dew drops on the leaves.
Texture bakeGardenNear(const Renderer& r, const Theme& t, int layerW)
{
  VectorImage img(layerW, kScreenH);
  cairo_t* cr = img.cr();
  Rng rng(2549u);
  const double W = layerW;
  const Color wood = lerpColor(lerpColor(rgb(150, 116, 84), t.nearLayer, 0.25f), t.skyMid, 0.3f);
  // The fence: two runs of planks.
  for (const auto& run : {std::pair<double, double>{140, 1180}, std::pair<double, double>{1700, 2340}})
  {
    // The rail behind them.
    cairo_rectangle(cr, run.first - 40, 380, run.second - run.first + 80, 46);
    fillLinear(cr, 0, 380, 0, 426, {{0.0, lighten(wood, 0.1f)}, {1.0, darken(wood, 0.35f)}});
    for (double x = run.first; x < run.second; x += 236)
    {
      const double w = 214, top = 230 + rng.range(-14, 14);
      cairo_move_to(cr, x, kScreenH);
      cairo_line_to(cr, x, top + 50);
      cairo_line_to(cr, x + w * 0.5, top);
      cairo_line_to(cr, x + w, top + 50);
      cairo_line_to(cr, x + w, kScreenH);
      cairo_close_path(cr);
      fillLinear(cr, x, 0, x + w, 0, {{0.0, lighten(wood, 0.25f)}, {0.2, wood}, {0.85, darken(wood, 0.12f)}, {1.0, darken(wood, 0.35f)}}, 1.0, true);
      setRgba(cr, darken(wood, 0.55f), 0.9);
      cairo_set_line_width(cr, 2.5);
      cairo_stroke(cr);
      // The sun on the point.
      cairo_move_to(cr, x + 2, top + 51);
      cairo_line_to(cr, x + w * 0.5, top + 2);
      inkStroke(cr, rgb(255, 226, 150), 3.0, 0.65);
      // Grain, a knot, a rusty nail with its streak.
      for (int g = 0; g < 7; ++g)
      {
        const double gx = x + 18 + g * 28 + rng.range(-6, 6);
        cairo_move_to(cr, gx, top + 60);
        cairo_curve_to(cr, gx + rng.range(-10, 10), 330, gx + rng.range(-10, 10), 520, gx + rng.range(-8, 8), kScreenH);
      }
      inkStroke(cr, darken(wood, 0.3f), 1.4, 0.45);
      const double kx = x + rng.range(50, 160), ky = rng.range(470, 620);
      ellipse(cr, kx, ky, 6, 10);
      setRgba(cr, darken(wood, 0.4f), 0.7);
      cairo_fill(cr);
      ellipse(cr, kx, ky, 12, 20);
      inkStroke(cr, darken(wood, 0.3f), 1.4, 0.45);
      const double nx = x + w * 0.5, ny = 402;
      cairo_move_to(cr, nx, ny);
      cairo_curve_to(cr, nx + 2, ny + 40, nx - 3, ny + 80, nx + 1, ny + 120);
      inkStroke(cr, rgb(150, 80, 40), 5.0, 0.35);
      circle(cr, nx, ny, 5);
      setRgba(cr, rgb(110, 70, 50), 1.0);
      cairo_fill(cr);
    }
  }
  // Clover: clumps of stalks up from the bottom, leaves at their tops.
  const Color leaf = lerpColor(rgb(80, 150, 64), t.nearLayer, 0.25f);
  for (int i = 0; i < 26; ++i)
  {
    const double x = rng.range(0, float(W)), top = rng.range(430, 610), s = rng.range(26, 48);
    for (const double ox : {-W, 0.0, W})
    {
      if (x + ox + s * 2.5 < 0 || x + ox - s * 2.5 > W)
        continue;
      const double bx = x + ox + rng.range(-20, 20);
      cairo_move_to(cr, bx, kScreenH + 4);
      cairo_curve_to(cr, bx + 10, 650, x + ox - 10, top + 60, x + ox, top);
      setRgba(cr, darken(leaf, 0.2f), 1.0);
      cairo_set_line_width(cr, 6);
      cairo_stroke(cr);
      clover(cr, x + ox, top, s, rng.range(0, 2), leaf, rng.range(0.45f, 0.8f));
      if (i % 3 == 0)
        dewDrop(cr, x + ox + s * 0.4, top - s * 0.2, rng.range(6, 11), leaf);
    }
  }
  // A few drops hanging from the fence's rail, catching the sun.
  for (int i = 0; i < 5; ++i)
  {
    const double x = 200 + i * 230 + rng.range(-30, 30);
    dewDrop(cr, x, 436, rng.range(6, 9), wood);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

} // namespace gr
