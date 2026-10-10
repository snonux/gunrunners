// Level 26, Kaiju Tonight (theme look "city"): a tabletop model city at night
// on a 1950s monster-movie soundstage. The masses are the set's plywood
// base (its grain, a countersunk screw and a pencil mark, a knot); where you stand, the model street seen a little
// from above (the far sidewalk, the asphalt with its dashed centre line, the
// near sidewalk, the curb, the base's painted edge); the one-way floors are
// tread-plate steel planks, climbables a galvanized manhole ladder. Behind:
// a painted Technicolor night (brushy moon, clouds and stars, two painted
// searchlights, the burning city's glow low down); far off the cardboard
// skyline cut-outs with lit windows, their braces and the seam between two
// backdrop canvases taped over; close by the studio's lighting truss with
// its lamps, their soft beams, cables, a boom mic dipping into shot and a
// bank of smoke-machine haze.

#include "assets/art_city.hpp"

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
constexpr Color kInk = rgb(16, 14, 26);

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
  cairo_stroke(cr);
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

// The plywood's grain: long wavy lines across the tile, wrapped both ways so
// the blocks tile.
void plyGrain(cairo_t* cr, const Theme& t, Rng& rng)
{
  for (int i = 0; i < 14; ++i)
  {
    const double y = rng.range(0, 64), amp = rng.range(0.6f, 2.6f), ph = rng.range(0, 6.28f);
    const bool light = i % 3 == 0;
    for (const double oy : {-64.0, 0.0, 64.0})
    {
      cairo_move_to(cr, -2, y + oy + std::sin(ph) * amp);
      for (int s = 1; s <= 8; ++s)
      {
        const double x = s * 8.5 - 2;
        cairo_line_to(cr, x, y + oy + std::sin(ph + x / 64.0 * 2 * kPi) * amp);
      }
    }
    setRgba(cr, light ? lighten(t.rockLight, 0.2f) : darken(t.rock, 0.3f), light ? 0.28 : 0.3);
    cairo_set_line_width(cr, light ? 1.4 : rng.range(0.7f, 1.5f));
    cairo_stroke(cr);
  }
  // Pores: short dark dashes along the grain.
  for (int i = 0; i < 26; ++i)
  {
    const double x = rng.range(0, 64), y = rng.range(0, 64), l = rng.range(2, 6);
    for (const double ox : {-64.0, 0.0, 64.0})
    {
      cairo_move_to(cr, x + ox, y);
      cairo_line_to(cr, x + ox + l, y + rng.range(-0.3f, 0.3f));
    }
    setRgba(cr, darken(t.rock, 0.45f), 0.25);
    cairo_set_line_width(cr, 0.8);
    cairo_stroke(cr);
  }
}

// Cardboard cut-out building silhouettes for the far layer: the outline of
// one building with its base on `base`, `w` wide and `h` tall. kind 0 a
// stepped Art Deco tower with a spire, 1 a plain block, 2 a dome, 3 a slab
// with a water tank on its roof, 4 a tower with a mast.
void skylineShape(cairo_t* cr, double x, double base, double w, double h, int kind)
{
  const double top = base - h;
  switch (kind)
  {
    case 0:
    {
      const double s1 = w * 0.16, s2 = w * 0.3;
      cairo_move_to(cr, x, base);
      cairo_line_to(cr, x, top + h * 0.3);
      cairo_line_to(cr, x + s1, top + h * 0.3);
      cairo_line_to(cr, x + s1, top + h * 0.16);
      cairo_line_to(cr, x + s2, top + h * 0.16);
      cairo_line_to(cr, x + s2, top + h * 0.06);
      cairo_line_to(cr, x + w * 0.47, top + h * 0.06);
      cairo_line_to(cr, x + w * 0.5, top - h * 0.14);
      cairo_line_to(cr, x + w * 0.53, top + h * 0.06);
      cairo_line_to(cr, x + w - s2, top + h * 0.06);
      cairo_line_to(cr, x + w - s2, top + h * 0.16);
      cairo_line_to(cr, x + w - s1, top + h * 0.16);
      cairo_line_to(cr, x + w - s1, top + h * 0.3);
      cairo_line_to(cr, x + w, top + h * 0.3);
      cairo_line_to(cr, x + w, base);
      cairo_close_path(cr);
      break;
    }
    case 2:
      cairo_move_to(cr, x, base);
      cairo_line_to(cr, x, top + h * 0.45);
      cairo_line_to(cr, x + w * 0.12, top + h * 0.45);
      cairo_curve_to(cr, x + w * 0.14, top - h * 0.05, x + w * 0.86, top - h * 0.05, x + w * 0.88, top + h * 0.45);
      cairo_line_to(cr, x + w, top + h * 0.45);
      cairo_line_to(cr, x + w, base);
      cairo_close_path(cr);
      // The lantern on the dome.
      cairo_rectangle(cr, x + w * 0.46, top - h * 0.12, w * 0.08, h * 0.12);
      break;
    case 3:
      cairo_rectangle(cr, x, top + h * 0.18, w, h * 0.82);
      // A water tank on stilts.
      cairo_rectangle(cr, x + w * 0.58, top, w * 0.24, h * 0.12);
      cairo_move_to(cr, x + w * 0.56, top + h * 0.02);
      cairo_line_to(cr, x + w * 0.7, top - h * 0.06);
      cairo_line_to(cr, x + w * 0.84, top + h * 0.02);
      cairo_close_path(cr);
      for (const double lx : {0.6, 0.78})
        cairo_rectangle(cr, x + w * lx, top + h * 0.1, 2.5, h * 0.09);
      break;
    case 4:
      cairo_rectangle(cr, x, top + h * 0.2, w, h * 0.8);
      cairo_rectangle(cr, x + w * 0.2, top + h * 0.1, w * 0.6, h * 0.12);
      cairo_move_to(cr, x + w * 0.5 - 3, top + h * 0.1);
      cairo_line_to(cr, x + w * 0.5 - 1, top - h * 0.3);
      cairo_line_to(cr, x + w * 0.5 + 1, top - h * 0.3);
      cairo_line_to(cr, x + w * 0.5 + 3, top + h * 0.1);
      cairo_close_path(cr);
      break;
    default:
      cairo_rectangle(cr, x, top, w, h);
      cairo_rectangle(cr, x + w * 0.1, top - 6, w * 0.8, 7);
      break;
  }
}

// Hand-painted lit windows over a cut-out: a grid, some lit warm, some
// cool, most dark, each a little crooked.
void skylineWindows(cairo_t* cr, Rng& rng, double x, double base, double w, double h, int kind, double litRate)
{
  const double top = base - h + (kind == 0 ? h * 0.32 : (kind == 2 ? h * 0.5 : h * 0.24));
  const double cw = 9, ch = 12, gx = 15, gy = 20;
  for (double wy = top + 8; wy + ch < base - 10; wy += gy)
    for (double wx = x + 8; wx + cw < x + w - 6; wx += gx)
    {
      const float roll = rng.uniform();
      if (roll > litRate)
        continue;
      const bool warm = rng.uniform() < 0.8f;
      const double jx = rng.range(-1.2f, 1.2f), jy = rng.range(-1.2f, 1.2f);
      cairo_rectangle(cr, wx + jx, wy + jy, cw + rng.range(-1, 1), ch + rng.range(-1, 1));
      setRgba(cr, warm ? rgb(255, 214, 120) : rgb(190, 220, 255), rng.range(0.55f, 0.95f));
      cairo_fill(cr);
      if (rng.uniform() < 0.25f)
        glowAt(cr, wx + cw * 0.5, wy + ch * 0.5, 16, rgb(255, 200, 110), 0.18);
    }
}

} // namespace

bool isCity(const Theme& t) { return std::string_view(t.look) == "city"; }

// The set's plywood base: the face grain; variant 1 a countersunk screw and
// a pencil mark, variant 2 a knot in the veneer.
Texture bakeCitySolid(const Renderer& r, const Theme& t, int variant)
{
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  Rng rng(std::uint32_t(variant * 7907 + 2617));
  cairo_rectangle(cr, 0, 0, 64, 64);
  fillLinear(cr, 0, 0, 64, 0, {{0.0, lerpColor(t.rock, t.rockLight, 0.25f)}, {0.5, t.rock}, {1.0, lerpColor(t.rock, t.rockLight, 0.25f)}});
  Rng grain(2621u); // the same grain on every variant, so they sit together
  plyGrain(cr, t, grain);
  if (variant == 1)
  {
    // A countersunk screw: a dimple, the head, its cross slot.
    circle(cr, 42, 24, 5.2);
    setRgba(cr, darken(t.rock, 0.4f), 0.5);
    cairo_fill(cr);
    circle(cr, 42, 24, 3.8);
    cairo_pattern_t* g = cairo_pattern_create_radial(40.6, 22.6, 0.4, 42, 24, 3.8);
    stop(g, 0, rgb(236, 236, 240));
    stop(g, 1, rgb(120, 122, 132));
    cairo_set_source(cr, g);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(g);
    inkStroke(cr, rgb(70, 70, 80), 0.8);
    cairo_move_to(cr, 39.6, 22.4);
    cairo_line_to(cr, 44.4, 25.6);
    cairo_move_to(cr, 39.6, 25.6);
    cairo_line_to(cr, 44.4, 22.4);
    inkStroke(cr, rgb(60, 60, 70), 1.0);
    // A pencil tick and cross where somebody measured.
    cairo_move_to(cr, 10, 46);
    cairo_line_to(cr, 24, 45.4);
    cairo_move_to(cr, 16, 41);
    cairo_line_to(cr, 19, 50);
    inkStroke(cr, rgb(70, 66, 76), 0.9, 0.55);
  }
  else if (variant == 2)
  {
    // A knot in the veneer, the grain flowing round it.
    ellipse(cr, 26, 30, 7, 4.2, 0.08);
    cairo_pattern_t* g = cairo_pattern_create_radial(26, 30, 0.5, 26, 30, 7);
    stop(g, 0, darken(t.rock, 0.55f));
    stop(g, 0.6, darken(t.rock, 0.35f));
    stop(g, 1, darken(t.rock, 0.1f), 0.0);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    for (int i = 0; i < 3; ++i)
    {
      ellipse(cr, 26, 30, 9.0 + i * 3.5, 5.4 + i * 2.2, 0.08);
      inkStroke(cr, darken(t.rock, 0.35f), 0.8, 0.35 - i * 0.08);
    }
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// Where you stand: the model street seen a little from above. From the back:
// the far sidewalk, the asphalt with its dashed yellow centre line (your
// feet on it), the near sidewalk with its joints, the curb's face, the
// base's painted green edge and its shadow on the plywood.
Texture bakeCitySolidTop(const Renderer& r, const Theme& t, float topOffset, int height)
{
  VectorImage img(64, height);
  cairo_t* cr = img.cr();
  const double y = topOffset;
  Rng rng(2633u);
  const Color asphalt = t.trim, concrete = rgb(182, 178, 168);
  // The shadow under the base's edge.
  {
    cairo_pattern_t* g = cairo_pattern_create_linear(0, y + 26, 0, y + 40);
    cairo_pattern_add_color_stop_rgba(g, 0, 0.05, 0.03, 0.02, 0.55);
    cairo_pattern_add_color_stop_rgba(g, 1, 0.05, 0.03, 0.02, 0.0);
    cairo_rectangle(cr, 0, y + 26, 64, 14);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  // The base's painted edge: dark green, brushed.
  cairo_rectangle(cr, 0, y + 18, 64, 8);
  fillLinear(cr, 0, y + 18, 0, y + 26, {{0.0, rgb(58, 92, 62)}, {1.0, rgb(28, 48, 34)}});
  for (int i = 0; i < 5; ++i)
  {
    const double by = y + 19.5 + i * 1.4;
    cairo_move_to(cr, -2, by);
    cairo_curve_to(cr, 20, by + rng.range(-0.6f, 0.6f), 44, by + rng.range(-0.6f, 0.6f), 66, by);
  }
  inkStroke(cr, rgb(110, 150, 110), 0.5, 0.3);
  // The curb's face.
  cairo_rectangle(cr, 0, y + 12, 64, 6);
  fillLinear(cr, 0, y + 12, 0, y + 18, {{0.0, darken(concrete, 0.12f)}, {1.0, darken(concrete, 0.38f)}});
  // The near sidewalk, its slabs' joints.
  cairo_rectangle(cr, 0, y + 6, 64, 6);
  fillLinear(cr, 0, y + 6, 0, y + 12, {{0.0, lighten(concrete, 0.12f)}, {1.0, concrete}});
  for (const double jx : {0.5, 32.5})
  {
    cairo_move_to(cr, jx, y + 6);
    cairo_line_to(cr, jx - 1.2, y + 18);
  }
  inkStroke(cr, darken(concrete, 0.45f), 0.9, 0.7);
  cairo_move_to(cr, 0, y + 12);
  cairo_line_to(cr, 64, 12 + y);
  inkStroke(cr, lighten(concrete, 0.5f), 0.8, 0.8);
  // The gutter.
  cairo_rectangle(cr, 0, y + 5, 64, 1.2);
  setRgba(cr, rgb(30, 30, 36), 0.8);
  cairo_fill(cr);
  // The asphalt: speckled, its dashed centre line in perspective.
  cairo_rectangle(cr, 0, y - 9, 64, 14);
  fillLinear(cr, 0, y - 9, 0, y + 5, {{0.0, darken(asphalt, 0.2f)}, {1.0, lighten(asphalt, 0.06f)}});
  for (int i = 0; i < 40; ++i)
  {
    const double sx = rng.range(0, 64), sy = rng.range(float(y - 8), float(y + 4));
    cairo_rectangle(cr, sx, sy, 1.0, 0.8);
    setRgba(cr, i % 2 ? lighten(asphalt, 0.35f) : darken(asphalt, 0.4f), 0.5);
    cairo_fill(cr);
  }
  for (const double dx : {4.0, 36.0})
  {
    cairo_move_to(cr, dx, y - 2.6);
    cairo_line_to(cr, dx + 19, y - 2.6);
    cairo_line_to(cr, dx + 20, y - 0.8);
    cairo_line_to(cr, dx + 1, y - 0.8);
    cairo_close_path(cr);
    setRgba(cr, t.trimGlow, 0.9);
    cairo_fill(cr);
  }
  // A tar seam crossing the road.
  cairo_move_to(cr, 50, y - 9);
  cairo_curve_to(cr, 48, y - 5, 53, y - 2, 51, y + 5);
  inkStroke(cr, darken(asphalt, 0.5f), 0.9, 0.6);
  // The far sidewalk, a thin pale strip.
  cairo_rectangle(cr, 0, y - 12, 64, 3);
  fillLinear(cr, 0, y - 12, 0, y - 9, {{0.0, lighten(concrete, 0.2f)}, {1.0, darken(concrete, 0.25f)}});
  return img.toTexture(r, 0.0f, topOffset);
}

// A one-way floor: a tread-plate steel plank (a manhole's hatch), its
// diamond pattern catching the light, rivets at the ends, a yellow edge.
Texture bakeCityPlatform(const Renderer& r, const Theme& t)
{
  VectorImage img(64, 40);
  cairo_t* cr = img.cr();
  cairo_rectangle(cr, 0, 12, 64, 10);
  {
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 12, 0, 22);
    cairo_pattern_add_color_stop_rgba(g, 0, 0.02, 0.02, 0.05, 0.4);
    cairo_pattern_add_color_stop_rgba(g, 1, 0.02, 0.02, 0.05, 0.0);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  cairo_rectangle(cr, 0, 0, 64, 12);
  fillLinear(cr, 0, 0, 0, 12, {{0.0, lighten(t.platform, 0.35f)}, {0.4, t.platform}, {1.0, t.platformDark}});
  // The tread: little raised lozenges, alternating.
  for (int row = 0; row < 2; ++row)
    for (int i = 0; i < 9; ++i)
    {
      const double x = 3 + i * 7 + (row ? 3.5 : 0), yy = 3 + row * 3.4;
      ellipse(cr, x, yy, 2.6, 0.8, row ? 0.5 : -0.5);
      setRgba(cr, lighten(t.platform, 0.55f), 0.7);
      cairo_fill(cr);
    }
  cairo_rectangle(cr, 0, 9, 64, 3);
  fillLinear(cr, 0, 0, 64, 0, {{0.0, rgb(240, 200, 60)}, {1.0, rgb(200, 150, 30)}});
  for (int i = 0; i < 8; ++i)
  {
    cairo_move_to(cr, i * 8 + 2, 12);
    cairo_line_to(cr, i * 8 + 5, 9);
    cairo_line_to(cr, i * 8 + 8, 9);
    cairo_line_to(cr, i * 8 + 5, 12);
    cairo_close_path(cr);
  }
  setRgba(cr, rgb(30, 30, 36), 0.85);
  cairo_fill(cr);
  for (const double rx : {4.0, 60.0})
  {
    circle(cr, rx, 5, 1.6);
    setRgba(cr, rgb(240, 240, 250), 0.8);
    cairo_fill(cr);
  }
  cairo_move_to(cr, 0, 0.6);
  cairo_line_to(cr, 64, 0.6);
  inkStroke(cr, rgb(240, 244, 255), 1.0, 0.7);
  cairo_rectangle(cr, 0, 0, 64, 12);
  inkStroke(cr, darken(t.platformDark, 0.5f), 0.8, 0.8);
  return img.toTexture(r, 0.0f, 0.0f);
}

// A climbable: a galvanized steel ladder down a manhole (drawn 16 px left of
// its block, so its middle is the left cell's middle).
Texture bakeCityLadder(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  const Color steel = rgb(170, 176, 188);
  for (const double y : {14.0, 46.0})
  {
    cairo_rectangle(cr, 18, y - 2.5, 28, 5);
    fillLinear(cr, 0, y - 2.5, 0, y + 2.5, {{0.0, lighten(steel, 0.4f)}, {1.0, darken(steel, 0.4f)}}, 1.0, true);
    inkStroke(cr, kInk, 1.0, 0.8);
    cairo_rectangle(cr, 18, y + 2.5, 28, 3);
    setRgba(cr, rgb(0, 0, 0), 0.25);
    cairo_fill(cr);
  }
  for (const double x : {14.0, 44.0})
  {
    cairo_rectangle(cr, x, -1, 6, 66);
    fillLinear(cr, x, 0, x + 6, 0, {{0.0, lighten(steel, 0.5f)}, {0.4, steel}, {1.0, darken(steel, 0.45f)}}, 1.0, true);
    inkStroke(cr, kInk, 1.0, 0.8);
  }
  // Rust where the rungs meet the rails.
  for (const double y : {14.0, 46.0})
    for (const double x : {20.0, 44.0})
    {
      ellipse(cr, x, y + 2, 2.5, 1.5);
      setRgba(cr, rgb(160, 90, 50), 0.5);
      cairo_fill(cr);
    }
  return img.toTexture(r, 16.0f, 0.0f);
}

// The painted night: a Technicolor gradient laid in with a broad brush, a
// big brushy moon, painted stars and clouds lit along their tops, two
// painted searchlights, and the burning city's glow along the bottom.
Texture bakeCitySky(const Renderer& r, const Theme& t)
{
  VectorImage img(kScreenW, kScreenH);
  cairo_t* cr = img.cr();
  cairo_rectangle(cr, 0, 0, kScreenW, kScreenH);
  fillLinear(cr, 0, 0, 0, kScreenH,
    {{0.0, t.skyTop}, {0.45, lerpColor(t.skyTop, t.skyMid, 0.7f)}, {0.72, t.skyMid}, {1.0, t.skyBottom}});
  Rng rng(2647u);
  // Broad brush strokes: long, faint, overlapping.
  for (int i = 0; i < 90; ++i)
  {
    const double y = rng.range(0, float(kScreenH)), x = rng.range(-200, float(kScreenW)), l = rng.range(200, 600);
    cairo_move_to(cr, x, y);
    cairo_curve_to(cr, x + l * 0.3, y + rng.range(-6, 6), x + l * 0.7, y + rng.range(-6, 6), x + l, y + rng.range(-4, 4));
    setRgba(cr, i % 2 ? lighten(t.skyMid, 0.25f) : darken(t.skyTop, 0.3f), rng.range(0.03f, 0.07f));
    cairo_set_line_width(cr, rng.range(6, 22));
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_stroke(cr);
  }
  // The city burning, low down.
  for (int i = 0; i < 5; ++i)
    glowAt(cr, 140 + i * 260 + rng.range(-60, 60), kScreenH + 40, rng.range(260, 380), rgb(255, 120, 60), 0.32);
  // Painted searchlights fanning up from behind the city.
  for (const auto& sl : {std::pair<double, double>{260, -0.32}, std::pair<double, double>{820, 0.22}})
  {
    const double x = sl.first, a = sl.second;
    cairo_save(cr);
    cairo_translate(cr, x, kScreenH + 30);
    cairo_rotate(cr, a);
    cairo_move_to(cr, -8, 0);
    cairo_line_to(cr, -70, -820);
    cairo_line_to(cr, 70, -820);
    cairo_line_to(cr, 8, 0);
    cairo_close_path(cr);
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 0, 0, -820);
    stop(g, 0, rgb(230, 236, 255), 0.22);
    stop(g, 0.7, rgb(200, 210, 255), 0.06);
    stop(g, 1, rgb(200, 210, 255), 0.0);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
    cairo_restore(cr);
  }
  // Stars: dots, and a few hand-painted four-point twinkles.
  for (int i = 0; i < 150; ++i)
  {
    const double x = rng.range(0, float(kScreenW)), y = rng.range(0, 430) * rng.range(0.3f, 1.0f);
    circle(cr, x, y, rng.range(0.6f, 1.8f));
    setRgba(cr, i % 5 ? rgb(255, 250, 230) : rgb(200, 220, 255), rng.range(0.4f, 0.95f));
    cairo_fill(cr);
  }
  for (int i = 0; i < 12; ++i)
  {
    const double x = rng.range(30, float(kScreenW) - 30), y = rng.range(20, 300), s = rng.range(5, 10);
    glowAt(cr, x, y, s * 2.4, rgb(255, 250, 220), 0.25);
    cairo_move_to(cr, x - s, y);
    cairo_curve_to(cr, x - s * 0.2, y - s * 0.15, x - s * 0.15, y - s * 0.2, x, y - s);
    cairo_curve_to(cr, x + s * 0.15, y - s * 0.2, x + s * 0.2, y - s * 0.15, x + s, y);
    cairo_curve_to(cr, x + s * 0.2, y + s * 0.15, x + s * 0.15, y + s * 0.2, x, y + s);
    cairo_curve_to(cr, x - s * 0.15, y + s * 0.2, x - s * 0.2, y + s * 0.15, x - s, y);
    cairo_close_path(cr);
    setRgba(cr, rgb(255, 252, 236), 0.9);
    cairo_fill(cr);
  }
  // The moon: a big painted disc, brushy, its seas dabbed on, a halo.
  {
    const double mx = 1010, my = 150, mr = 78;
    glowAt(cr, mx, my, mr * 3.2, rgb(255, 236, 190), 0.22);
    glowAt(cr, mx, my, mr * 1.6, rgb(255, 246, 220), 0.3);
    circle(cr, mx, my, mr);
    cairo_pattern_t* g = cairo_pattern_create_radial(mx - mr * 0.3, my - mr * 0.35, mr * 0.1, mx, my, mr);
    stop(g, 0, rgb(255, 252, 232));
    stop(g, 0.7, rgb(250, 234, 190));
    stop(g, 1, rgb(226, 200, 150));
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
    cairo_save(cr);
    circle(cr, mx, my, mr);
    cairo_clip(cr);
    for (int i = 0; i < 9; ++i)
    {
      const double a = rng.range(0, 6.28f), d = rng.range(0, float(mr) * 0.75f);
      ellipse(cr, mx + std::cos(a) * d, my + std::sin(a) * d, rng.range(8, 22), rng.range(6, 16), rng.range(0, 3));
      setRgba(cr, rgb(196, 176, 140), rng.range(0.25f, 0.45f));
      cairo_fill(cr);
    }
    for (int i = 0; i < 26; ++i)
    {
      const double yy = my - mr + rng.range(0, float(mr) * 2);
      cairo_move_to(cr, mx - mr, yy);
      cairo_curve_to(cr, mx - mr * 0.3, yy + rng.range(-5, 5), mx + mr * 0.3, yy + rng.range(-5, 5), mx + mr, yy);
      setRgba(cr, i % 2 ? rgb(255, 255, 240) : rgb(210, 190, 150), 0.08);
      cairo_set_line_width(cr, rng.range(3, 7));
      cairo_stroke(cr);
    }
    cairo_restore(cr);
  }
  // Painted clouds: lumpy bands, dark violet, lit along their tops.
  for (int c = 0; c < 6; ++c)
  {
    const double cx = rng.range(-80, float(kScreenW) + 80), cy = rng.range(170, 360), cw = rng.range(260, 460);
    const Color body = lerpColor(t.skyMid, rgb(30, 22, 60), 0.45f);
    for (int pass = 0; pass < 2; ++pass)
    {
      Rng lumps(std::uint32_t(2659 + c * 31));
      for (int i = 0; i < 9; ++i)
      {
        const double u = double(i) / 8.0;
        const double lx = cx - cw * 0.5 + cw * u, rx = lumps.range(30, 70) * (1.0 - std::abs(u - 0.5));
        const double ly = cy - lumps.range(0, 18) * (1.0 - std::abs(u - 0.5) * 1.4);
        if (pass == 0)
        {
          ellipse(cr, lx, ly - 4, rx * 1.05 + 10, rx * 0.5 + 8);
          setRgba(cr, lighten(t.skyMid, 0.35f), 0.3);
        }
        else
        {
          ellipse(cr, lx, ly, rx + 10, rx * 0.48 + 6);
          setRgba(cr, body, 0.92);
        }
        cairo_fill(cr);
      }
    }
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// Far: painted mountains faint in the haze, then two rows of cardboard
// skyline cut-outs (the back row hazed, the front row with lit windows and
// the cardboard's cut edge catching the light), a wooden brace here and
// there, and the seams where two backdrop canvases meet, taped over.
Texture bakeCityFar(const Renderer& r, const Theme& t, int layerW)
{
  VectorImage img(layerW, kScreenH);
  cairo_t* cr = img.cr();
  Rng rng(2663u);
  const double W = layerW;
  const Color haze = t.skyMid;
  // The second canvas: a hair lighter, so its edges read as seams.
  cairo_rectangle(cr, 1700, 0, W - 1700, kScreenH);
  setRgba(cr, rgb(255, 240, 255), 0.035);
  cairo_fill(cr);
  for (const double sx : {1700.0, 0.5, W - 0.5})
  {
    cairo_move_to(cr, sx, 0);
    cairo_line_to(cr, sx + 2, kScreenH);
    inkStroke(cr, rgb(10, 8, 24), 2.0, 0.45);
    cairo_move_to(cr, sx + 2.5, 0);
    cairo_line_to(cr, sx + 4.5, kScreenH);
    inkStroke(cr, rgb(200, 190, 255), 1.0, 0.12);
    if (sx > 1.0 && sx < W - 1.0)
      for (int i = 0; i < 4; ++i)
      {
        // Strips of masking tape over the seam.
        const double ty = 60 + i * 120 + rng.range(-20, 20);
        cairo_save(cr);
        cairo_translate(cr, sx + 1, ty);
        cairo_rotate(cr, rng.range(-0.12f, 0.12f));
        cairo_rectangle(cr, -16, -5, 32, 10);
        setRgba(cr, rgb(214, 200, 160), 0.32);
        cairo_fill(cr);
        cairo_restore(cr);
      }
  }
  // Painted mountains, far off and pale.
  cairo_move_to(cr, -10, kScreenH);
  for (int i = 0; i <= 16; ++i)
  {
    const double x = W * i / 16.0, y = 420 + 40 * std::sin(i * 1.7) + 30 * std::sin(i * 0.6 + 1.0);
    cairo_line_to(cr, x, i % 2 ? y : y - 50);
  }
  cairo_line_to(cr, W + 10, kScreenH);
  cairo_close_path(cr);
  setRgba(cr, lerpColor(t.farLayer, haze, 0.55f), 0.55);
  cairo_fill(cr);
  // Two rows of cut-outs.
  for (int row = 0; row < 2; ++row)
  {
    const double base = row == 0 ? 600 : 640;
    double x = row == 0 ? -20 : 40;
    while (x < W)
    {
      const double w = rng.range(70, 150), h = row == 0 ? rng.range(170, 300) : rng.range(110, 250);
      const int kind = int(rng.next() % 5u);
      const double tilt = rng.range(-0.015f, 0.015f);
      const Color body = row == 0 ? lerpColor(t.farLayer, haze, 0.45f) : darken(t.farLayer, 0.15f);
      for (const double ox : {-W, 0.0, W})
      {
        if (x + ox + w < -10 || x + ox > W + 10)
          continue;
        cairo_save(cr);
        cairo_translate(cr, x + ox + w * 0.5, base);
        cairo_rotate(cr, tilt);
        cairo_translate(cr, -(x + ox + w * 0.5), -base);
        // The cut edge: the shape again, offset and pale.
        cairo_save(cr);
        cairo_translate(cr, 3, 0);
        skylineShape(cr, x + ox, base, w, h, kind);
        setRgba(cr, lighten(body, row == 0 ? 0.25f : 0.35f), 1.0);
        cairo_fill(cr);
        cairo_restore(cr);
        skylineShape(cr, x + ox, base, w, h, kind);
        fillLinear(cr, 0, base - h, 0, base, {{0.0, lighten(body, 0.08f)}, {1.0, darken(body, 0.2f)}});
        Rng wr(std::uint32_t(x * 13 + row * 7 + 1));
        skylineWindows(cr, wr, x + ox, base, w, h, kind, row == 0 ? 0.18 : 0.32);
        cairo_restore(cr);
        if (row == 1 && kind == 4)
        {
          // A red aircraft light on the mast.
          glowAt(cr, x + ox + w * 0.5, base - h - h * 0.3, 14, rgb(255, 60, 50), 0.6);
          circle(cr, x + ox + w * 0.5, base - h - h * 0.3, 2.2);
          setRgba(cr, rgb(255, 120, 110), 1.0);
          cairo_fill(cr);
        }
        if (row == 1 && int(x) % 3 == 0)
        {
          // A wooden brace propping the cut-out up from behind.
          cairo_move_to(cr, x + ox + w + 2, base - h * 0.55);
          cairo_line_to(cr, x + ox + w + 34, base + 4);
          inkStroke(cr, rgb(70, 52, 40), 5.0, 0.9);
          cairo_move_to(cr, x + ox + w + 3, base - h * 0.55);
          cairo_line_to(cr, x + ox + w + 35, base + 4);
          inkStroke(cr, rgb(150, 116, 80), 1.2, 0.6);
        }
      }
      x += w + (row == 0 ? rng.range(-30, 20) : rng.range(10, 90));
    }
  }
  // Fire glow and haze pooling along the bottom.
  {
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 470, 0, 720);
    stop(g, 0, t.skyBottom, 0.0);
    stop(g, 1, t.skyBottom, 0.55);
    cairo_rectangle(cr, 0, 470, W, 250);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// Near: the studio's lighting truss along the top, its lamps (barn doors,
// warm lenses) throwing soft beams down through the haze, cables sagging
// between them, a boom mic that dipped into shot, and a bank of smoke
// machine fog low down.
Texture bakeCityNear(const Renderer& r, const Theme& t, int layerW)
{
  VectorImage img(layerW, kScreenH);
  cairo_t* cr = img.cr();
  Rng rng(2671u);
  const double W = layerW;
  const Color metal = lerpColor(rgb(120, 124, 140), t.nearLayer, 0.45f);
  // The beams first, so the lamps sit over them.
  struct Lamp
  {
    double x, aim;
  };
  Lamp lamps[8];
  for (int i = 0; i < 8; ++i)
    lamps[i] = {i * W / 8 + 120 + rng.range(-40, 40), rng.range(-0.35f, 0.35f)};
  for (const auto& l : lamps)
    for (const double ox : {-W, 0.0, W})
    {
      if (l.x + ox < -500 || l.x + ox > W + 500)
        continue;
      cairo_save(cr);
      cairo_translate(cr, l.x + ox, 92);
      cairo_rotate(cr, l.aim);
      cairo_move_to(cr, -14, 0);
      cairo_line_to(cr, -150, 560);
      cairo_line_to(cr, 150, 560);
      cairo_line_to(cr, 14, 0);
      cairo_close_path(cr);
      cairo_pattern_t* g = cairo_pattern_create_linear(0, 0, 0, 560);
      stop(g, 0, rgb(255, 236, 200), 0.16);
      stop(g, 0.6, rgb(255, 236, 200), 0.05);
      stop(g, 1, rgb(255, 236, 200), 0.0);
      cairo_set_source(cr, g);
      cairo_fill(cr);
      cairo_pattern_destroy(g);
      cairo_restore(cr);
    }
  // The smoke machine's fog bank.
  for (int i = 0; i < 60; ++i)
  {
    const double x = rng.range(0, float(W)), y = rng.range(500, 600), rx = rng.range(60, 160);
    for (const double ox : {-W, 0.0, W})
    {
      if (x + ox + rx < 0 || x + ox - rx > W)
        continue;
      ellipse(cr, x + ox, y, rx, rx * 0.35);
      setRgba(cr, i % 3 ? rgb(190, 196, 230) : rgb(230, 220, 240), 0.07);
      cairo_fill(cr);
    }
  }
  {
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 540, 0, 720);
    stop(g, 0, rgb(180, 186, 220), 0.0);
    stop(g, 1, rgb(180, 186, 220), 0.35);
    cairo_rectangle(cr, 0, 540, W, 180);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  // Cables sagging between the lamps, a few hanging loose.
  for (int i = 0; i < 8; ++i)
  {
    const double x0 = lamps[i].x, x1 = i + 1 < 8 ? lamps[i + 1].x : lamps[0].x + W;
    const double sag = rng.range(30, 70);
    for (const double ox : {-W, 0.0})
    {
      cairo_move_to(cr, x0 + ox, 60);
      cairo_curve_to(cr, x0 + ox + (x1 - x0) * 0.3, 60 + sag, x0 + ox + (x1 - x0) * 0.7, 60 + sag, x1 + ox, 60);
      inkStroke(cr, rgb(14, 12, 20), 3.0, 0.9);
    }
    if (i % 3 == 1)
    {
      const double hx = (x0 + x1) * 0.5 + 40, hl = rng.range(90, 170);
      cairo_move_to(cr, hx, 50);
      cairo_curve_to(cr, hx + 6, 50 + hl * 0.4, hx - 8, 50 + hl * 0.7, hx + 4, 50 + hl);
      inkStroke(cr, rgb(14, 12, 20), 2.5, 0.9);
      cairo_rectangle(cr, hx, 50 + hl, 8, 10);
      setRgba(cr, rgb(40, 40, 50), 1.0);
      cairo_fill(cr);
    }
  }
  // The truss: two chords and the zig-zag between them.
  for (const double y : {22.0, 56.0})
  {
    cairo_rectangle(cr, 0, y - 3, W, 6);
    fillLinear(cr, 0, y - 3, 0, y + 3, {{0.0, lighten(metal, 0.45f)}, {0.5, metal}, {1.0, darken(metal, 0.5f)}});
  }
  for (double x = 0; x < W; x += 34)
  {
    cairo_move_to(cr, x, 25);
    cairo_line_to(cr, x + 17, 53);
    cairo_line_to(cr, x + 34, 25);
  }
  inkStroke(cr, darken(metal, 0.2f), 2.6, 1.0);
  for (double x = 0; x < W; x += 34)
  {
    cairo_move_to(cr, x + 1, 24);
    cairo_line_to(cr, x + 18, 52);
  }
  inkStroke(cr, lighten(metal, 0.4f), 0.8, 0.6);
  // The lamps: a clamp, the yoke, the can with barn doors, the lens.
  for (const auto& l : lamps)
    for (const double ox : {-W, 0.0, W})
    {
      if (l.x + ox < -80 || l.x + ox > W + 80)
        continue;
      const double x = l.x + ox;
      cairo_rectangle(cr, x - 6, 56, 12, 14);
      setRgba(cr, rgb(30, 30, 38), 1.0);
      cairo_fill(cr);
      cairo_move_to(cr, x - 22, 92);
      cairo_line_to(cr, x - 22, 68);
      cairo_line_to(cr, x + 22, 68);
      cairo_line_to(cr, x + 22, 92);
      inkStroke(cr, rgb(30, 30, 38), 4.0);
      cairo_save(cr);
      cairo_translate(cr, x, 92);
      cairo_rotate(cr, l.aim);
      roundedRect(cr, -18, -26, 36, 40, 5);
      fillLinear(cr, -18, 0, 18, 0, {{0.0, rgb(70, 70, 82)}, {0.35, rgb(40, 40, 50)}, {1.0, rgb(16, 16, 22)}}, 1.0, true);
      inkStroke(cr, kInk, 1.4);
      for (int k = 0; k < 4; ++k)
      {
        cairo_move_to(cr, -14 + k * 9, -24);
        cairo_line_to(cr, -14 + k * 9, -6);
      }
      inkStroke(cr, rgb(90, 90, 104), 1.0, 0.6);
      // Barn doors.
      cairo_move_to(cr, -18, 14);
      cairo_line_to(cr, -30, 30);
      cairo_move_to(cr, 18, 14);
      cairo_line_to(cr, 30, 30);
      inkStroke(cr, rgb(24, 24, 30), 4.0);
      ellipse(cr, 0, 15, 15, 4);
      cairo_pattern_t* g = cairo_pattern_create_radial(0, 15, 1, 0, 15, 15);
      stop(g, 0, rgb(255, 255, 240));
      stop(g, 1, rgb(255, 196, 110));
      cairo_set_source(cr, g);
      cairo_fill(cr);
      cairo_pattern_destroy(g);
      glowAt(cr, 0, 22, 46, rgb(255, 220, 160), 0.35);
      cairo_restore(cr);
    }
  // The boom mic dipping into shot (it happens in the best pictures).
  {
    const double mx = 1260, my = 210;
    cairo_move_to(cr, mx - 520, -10);
    cairo_line_to(cr, mx, my - 30);
    inkStroke(cr, rgb(26, 26, 32), 6.0);
    cairo_move_to(cr, mx - 520, -12);
    cairo_line_to(cr, mx, my - 32);
    inkStroke(cr, rgb(130, 130, 150), 1.4, 0.5);
    cairo_move_to(cr, mx, my - 32);
    cairo_line_to(cr, mx + 4, my - 14);
    inkStroke(cr, rgb(26, 26, 32), 3.0);
    ellipse(cr, mx + 6, my + 8, 16, 30, -0.1);
    setRgba(cr, rgb(70, 66, 74), 1.0);
    cairo_fill(cr);
    for (int i = 0; i < 40; ++i)
    {
      const double a = rng.range(0, 6.28f), d = rng.range(0.7f, 1.0f);
      cairo_move_to(cr, mx + 6 + std::cos(a) * 15 * d, my + 8 + std::sin(a) * 29 * d);
      cairo_line_to(cr, mx + 6 + std::cos(a) * 19 * d, my + 8 + std::sin(a) * 34 * d);
    }
    inkStroke(cr, rgb(110, 104, 112), 1.4, 0.8);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

} // namespace gr
