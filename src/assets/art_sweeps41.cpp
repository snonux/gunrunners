// Level 41, Sweeps Week (theme key sweeps_week, look "sweeps_week"): the
// studio of a Saturday-night variety show in ratings week, burgundy velvet
// and gold. See art_sweeps41.hpp.

#include "assets/art_sweeps41.hpp"

#include "base/math.hpp"
#include "render/vector.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <string_view>

namespace gr
{

namespace
{

constexpr double kPi = 3.14159265358979;

Color darken(Color c, float t) { return lerpColor(c, rgb(0, 0, 0), t); }
Color lighten(Color c, float t) { return lerpColor(c, rgb(255, 255, 255), t); }

void setRgba(cairo_t* cr, Color c, double a)
{
  cairo_set_source_rgba(cr, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, a);
}

void stop(cairo_pattern_t* g, double at, Color c, double a = 1.0)
{
  cairo_pattern_add_color_stop_rgba(g, at, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, a);
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

void stroke(cairo_t* cr, Color c, double w, double a = 1.0)
{
  setRgba(cr, c, a);
  cairo_set_line_width(cr, w);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
  cairo_stroke(cr);
}

void rect(cairo_t* cr, double x, double y, double w, double h, Color c, double a = 1.0)
{
  cairo_rectangle(cr, x, y, w, h);
  setRgba(cr, c, a);
  cairo_fill(cr);
}

void softDot(cairo_t* cr, double x, double y, double rx, double ry, Color c, double a)
{
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_scale(cr, std::max(0.1, rx), std::max(0.1, ry));
  cairo_pattern_t* g = cairo_pattern_create_radial(0, 0, 0, 0, 0, 1);
  stop(g, 0.0, c, a);
  stop(g, 0.6, c, a * 0.4);
  stop(g, 1.0, c, 0.0);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
  cairo_restore(cr);
}

void text(cairo_t* cr, const char* s, double cx, double baseline, double size, Color c, double a = 1.0)
{
  selectGameFont(cr);
  cairo_set_font_size(cr, size);
  cairo_text_extents_t e;
  cairo_text_extents(cr, s, &e);
  cairo_move_to(cr, cx - e.width * 0.5 - e.x_bearing, baseline);
  setRgba(cr, c, a);
  cairo_show_text(cr, s);
}

// A beam of light from (x, y) (its source) spreading to `spread` either
// side at `len` away along `angle` (radians, 0 = straight down).
void beam(cairo_t* cr, double x, double y, double angle, double len, double spread, Color c, double a)
{
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_rotate(cr, angle);
  cairo_move_to(cr, -6, 0);
  cairo_line_to(cr, 6, 0);
  cairo_line_to(cr, spread, len);
  cairo_line_to(cr, -spread, len);
  cairo_close_path(cr);
  cairo_pattern_t* g = cairo_pattern_create_linear(0, 0, 0, len);
  stop(g, 0.0, c, a);
  stop(g, 0.5, c, a * 0.45);
  stop(g, 1.0, c, 0.0);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
  cairo_restore(cr);
}

constexpr Color kVel = rgb(122, 22, 44);
constexpr Color kVelHi = rgb(196, 56, 80);
constexpr Color kVelLo = rgb(46, 6, 18);
constexpr Color kGold = rgb(226, 176, 70);
constexpr Color kGoldHi = rgb(255, 238, 170);
constexpr Color kGoldLo = rgb(120, 80, 26);
constexpr Color kLacquer = rgb(22, 14, 20);
constexpr Color kBulb = rgb(255, 232, 160);
constexpr Color kInk = rgb(12, 6, 10);

// Velvet pleats: deep folds across the 64 px, each a gradient from shadow
// to a soft highlight; they tile across.
void pleats(cairo_t* cr, double x0, double y0, double w, double h, double pitch, Color base, double phase)
{
  rect(cr, x0, y0, w, h, darken(base, 0.4f));
  for (double x = x0 - pitch + std::fmod(phase, pitch); x < x0 + w; x += pitch)
  {
    cairo_rectangle(cr, x, y0, pitch, h);
    fillLinear(cr, x, 0, x + pitch, 0,
      {{0.0, darken(base, 0.62f)}, {0.3, base}, {0.48, lighten(base, 0.22f)}, {0.62, base}, {1.0, darken(base, 0.62f)}});
  }
}

void bulb(cairo_t* cr, double x, double y, double r, double glow)
{
  softDot(cr, x, y, r * 3.2, r * 3.2, kBulb, 0.35 * glow);
  cairo_arc(cr, x, y, r, 0, 2 * kPi);
  setRgba(cr, lerpColor(rgb(150, 110, 60), kBulb, float(glow)), 1.0);
  cairo_fill(cr);
  cairo_arc(cr, x - r * 0.3, y - r * 0.35, r * 0.4, 0, 2 * kPi);
  setRgba(cr, rgb(255, 255, 255), 0.8 * glow);
  cairo_fill(cr);
}

void sequin(cairo_t* cr, double x, double y, double r, double a)
{
  softDot(cr, x, y, r * 2.5, r * 2.5, kGoldHi, a * 0.5);
  cairo_move_to(cr, x - r * 2.2, y);
  cairo_line_to(cr, x + r * 2.2, y);
  cairo_move_to(cr, x, y - r * 2.2);
  cairo_line_to(cr, x, y + r * 2.2);
  stroke(cr, rgb(255, 255, 240), 0.8, a);
}

void star(cairo_t* cr, double cx, double cy, double ro, double ri)
{
  for (int i = 0; i < 10; ++i)
  {
    const double a = -kPi / 2 + i * kPi / 5, rr = i % 2 ? ri : ro;
    if (i == 0)
      cairo_move_to(cr, cx + std::cos(a) * rr, cy + std::sin(a) * rr);
    else
      cairo_line_to(cr, cx + std::cos(a) * rr, cy + std::sin(a) * rr);
  }
  cairo_close_path(cr);
}

} // namespace

bool isSweeps(const Theme& t) { return std::string_view(t.look) == "sweeps_week"; }

// The stage's velvet skirting: pleats, a sequin here and there.
Texture bakeSweepsSolid(const Renderer& r, const Theme& t, int variant)
{
  (void)t;
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  pleats(cr, 0, 0, 64, 64, 16, variant == 1 ? darken(kVel, 0.08f) : kVel, 0);
  // The nap: a faint vertical grain.
  Rng rng(41011u + std::uint32_t(variant) * 7u);
  for (int i = 0; i < 26; ++i)
    rect(cr, rng.range(0, 63), rng.range(0, 60), 1, rng.range(3, 10), i % 2 ? kVelHi : kVelLo, rng.range(0.08f, 0.2f));
  if (variant == 2)
  {
    sequin(cr, 22, 18, 1.4, 0.8);
    sequin(cr, 46, 44, 1.1, 0.6);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// Where you stand: the stage's black lacquer deck, the lights reflected
// along it, a gold nosing, little marquee bulbs on the fascia under it,
// the velvet starting below.
Texture bakeSweepsSolidTop(const Renderer& r, const Theme& t, float topOffset, int height)
{
  (void)t;
  VectorImage img(64, height);
  cairo_t* cr = img.cr();
  const double y = topOffset;
  // The deck.
  cairo_rectangle(cr, 0, y - 5, 64, 9);
  fillLinear(cr, 0, y - 5, 0, y + 4, {{0.0, rgb(120, 70, 80)}, {0.25, rgb(44, 28, 36)}, {1.0, kLacquer}});
  rect(cr, 0, y - 5, 64, 1.2, rgb(255, 210, 190), 0.7);
  softDot(cr, 18, y - 2, 16, 1.5, kGoldHi, 0.35);
  softDot(cr, 50, y - 2, 10, 1.2, rgb(255, 120, 170), 0.3);
  // The gold nosing.
  cairo_rectangle(cr, 0, y + 4, 64, 5);
  fillLinear(cr, 0, y + 4, 0, y + 9, {{0.0, kGoldHi}, {0.4, kGold}, {1.0, kGoldLo}});
  // The fascia and its bulbs.
  cairo_rectangle(cr, 0, y + 9, 64, 13);
  fillLinear(cr, 0, y + 9, 0, y + 22, {{0.0, rgb(40, 18, 26)}, {1.0, rgb(24, 8, 14)}});
  for (double x = 8; x < 64; x += 16)
    bulb(cr, x, y + 15.5, 3.0, 1.0);
  rect(cr, 0, y + 22, 64, 2, kGoldLo);
  // Its shadow on the velvet.
  cairo_pattern_t* g = cairo_pattern_create_linear(0, y + 24, 0, y + 40);
  stop(g, 0.0, kInk, 0.6);
  stop(g, 1.0, kInk, 0.0);
  cairo_rectangle(cr, 0, y + 24, 64, 16);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
  return img.toTexture(r, 0.0f, topOffset);
}

// A box-seat balcony front: a brass rail over tufted red velvet with gold
// buttons, gold trim along the bottom. 64 x 40, the walking surface at the
// top; it tiles.
Texture bakeSweepsPlatform(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(64, 40);
  cairo_t* cr = img.cr();
  // The rail.
  cairo_rectangle(cr, 0, 0, 64, 6);
  fillLinear(cr, 0, 0, 0, 6, {{0.0, kGoldHi}, {0.35, kGold}, {1.0, kGoldLo}});
  rect(cr, 0, 1, 64, 1, rgb(255, 255, 240), 0.7);
  // The tufted velvet.
  cairo_rectangle(cr, 0, 6, 64, 22);
  fillLinear(cr, 0, 6, 0, 28, {{0.0, kVelHi}, {0.4, kVel}, {1.0, kVelLo}});
  for (int row = 0; row < 2; ++row)
    for (double x = (row ? 0 : 16); x < 64 + 1; x += 32)
    {
      const double cy = 11 + row * 11, cx = x;
      // Puckers around each button.
      for (int k = 0; k < 4; ++k)
      {
        const double a = kPi / 4 + k * kPi / 2;
        cairo_move_to(cr, cx, cy);
        cairo_line_to(cr, cx + std::cos(a) * 9, cy + std::sin(a) * 6);
        stroke(cr, kVelLo, 1.2, 0.6);
      }
      cairo_arc(cr, cx, cy, 2.2, 0, 2 * kPi);
      setRgba(cr, kGold, 1.0);
      cairo_fill(cr);
      cairo_arc(cr, cx - 0.6, cy - 0.7, 0.9, 0, 2 * kPi);
      setRgba(cr, kGoldHi, 1.0);
      cairo_fill(cr);
    }
  // The trim and the shadow.
  cairo_rectangle(cr, 0, 28, 64, 4);
  fillLinear(cr, 0, 28, 0, 32, {{0.0, kGold}, {1.0, kGoldLo}});
  cairo_pattern_t* g = cairo_pattern_create_linear(0, 32, 0, 40);
  stop(g, 0.0, kInk, 0.5);
  stop(g, 1.0, kInk, 0.0);
  cairo_rectangle(cr, 0, 32, 64, 8);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
  return img.toTexture(r, 0.0f, 0.0f);
}

// One block of brass ladder: polished rails, rungs with black rubber
// treads. Drawn 16 px left of its block, the rails straddling the left
// cell column.
Texture bakeSweepsLadder(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  for (const double y : {12.0, 44.0})
  {
    cairo_rectangle(cr, 14, y - 3, 36, 6);
    fillLinear(cr, 0, y - 3, 0, y + 3, {{0.0, kGoldHi}, {0.4, kGold}, {1.0, kGoldLo}}, 1.0, true);
    stroke(cr, kInk, 1.0);
    rect(cr, 16, y - 1, 32, 3, rgb(30, 26, 26));
  }
  for (const double x : {9.0, 49.0})
  {
    cairo_rectangle(cr, x, -2, 7, 68);
    fillLinear(cr, x, 0, x + 7, 0, {{0.0, kGoldHi}, {0.35, kGold}, {1.0, kGoldLo}}, 1.0, true);
    stroke(cr, kInk, 1.0);
  }
  return img.toTexture(r, 16.0f, 0.0f);
}

// The orchestra pit's hot stage lights: a tray of footlights turned up,
// their lenses burning white-hot, the air over them wavering.
Texture bakeSweepsSpikes(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  softDot(cr, 32, 34, 34, 22, rgb(255, 200, 120), 0.45);
  for (int i = 0; i < 2; ++i)
  {
    const double cx = 16 + i * 32;
    // The can, tilted up.
    cairo_move_to(cr, cx - 13, 56);
    cairo_line_to(cr, cx - 11, 36);
    cairo_line_to(cr, cx + 11, 36);
    cairo_line_to(cr, cx + 13, 56);
    cairo_close_path(cr);
    fillLinear(cr, cx - 13, 0, cx + 13, 0, {{0.0, rgb(70, 64, 70)}, {0.4, rgb(36, 32, 38)}, {1.0, rgb(14, 12, 16)}}, 1.0, true);
    stroke(cr, kInk, 1.2);
    // The lens: white-hot.
    cairo_save(cr);
    cairo_translate(cr, cx, 36);
    cairo_scale(cr, 11, 4);
    cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
    cairo_restore(cr);
    setRgba(cr, rgb(255, 250, 230), 1.0);
    cairo_fill(cr);
    softDot(cr, cx, 34, 16, 10, rgb(255, 255, 230), 0.8);
    // Heat wavering up.
    for (int k = 0; k < 3; ++k)
    {
      const double hx = cx - 6 + k * 6;
      cairo_move_to(cr, hx, 30);
      cairo_curve_to(cr, hx - 4, 24, hx + 4, 18, hx, 10);
      stroke(cr, rgb(255, 220, 160), 1.4, 0.35);
    }
  }
  // The tray, with a yellow-and-black hazard edge.
  rect(cr, 0, 54, 64, 10, rgb(26, 22, 26));
  for (double x = -8; x < 64; x += 12)
  {
    cairo_move_to(cr, x, 64);
    cairo_line_to(cr, x + 6, 54);
    cairo_line_to(cr, x + 12, 54);
    cairo_line_to(cr, x + 6, 64);
    cairo_close_path(cr);
    setRgba(cr, rgb(240, 200, 40), 0.9);
    cairo_fill(cr);
  }
  rect(cr, 0, 54, 64, 1.5, kInk);
  return img.toTexture(r, 0.0f, 0.0f);
}

// The studio's dark: plum black over the lighting grid, follow spots from
// the corners crossing it, sparkles of glitter in the air.
Texture bakeSweepsSky(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(kScreenW, kScreenH);
  cairo_t* cr = img.cr();
  cairo_rectangle(cr, 0, 0, kScreenW, kScreenH);
  fillLinear(cr, 0, 0, 0, kScreenH, {{0.0, rgb(10, 4, 10)}, {0.55, rgb(36, 8, 24)}, {1.0, rgb(84, 20, 40)}});
  struct Beam
  {
    double x, angle, spread;
    Color c;
    double a;
  };
  static constexpr Beam kBeams[] = {{-40, -0.75, 160, kGoldHi, 0.12}, {kScreenW + 40, 0.75, 160, rgb(255, 150, 200), 0.10},
    {420, -0.15, 120, kGoldHi, 0.06}, {880, 0.2, 120, rgb(255, 120, 160), 0.06}};
  for (const auto& b : kBeams)
    beam(cr, b.x, -20, b.angle, 1000, b.spread, b.c, b.a);
  Rng rng(41041u);
  for (int i = 0; i < 110; ++i)
  {
    const double x = rng.range(0, float(kScreenW)), y = rng.range(0, float(kScreenH));
    if (i % 5 == 0)
      sequin(cr, x, y, rng.range(0.6f, 1.2f), rng.range(0.2f, 0.5f));
    else
    {
      cairo_arc(cr, x, y, rng.range(0.5f, 1.3f), 0, 2 * kPi);
      setRgba(cr, kGoldHi, rng.range(0.1f, 0.35f));
      cairo_fill(cr);
    }
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

namespace
{

constexpr int kFarH = kScreenH + 360;
constexpr int kNearH = kScreenH + 420;

// Marquee letters: each glyph's outline traced in bulbs.
void marqueeWord(cairo_t* cr, const char* s, double cx, double baseline, double size)
{
  selectGameFont(cr);
  cairo_set_font_size(cr, size);
  cairo_text_extents_t e;
  cairo_text_extents(cr, s, &e);
  const double x = cx - e.width * 0.5 - e.x_bearing;
  // The board behind.
  cairo_rectangle(cr, x - 30, baseline - size * 0.95, e.width + 60, size * 1.25);
  fillLinear(cr, 0, baseline - size, 0, baseline + size * 0.3, {{0.0, rgb(60, 20, 30)}, {1.0, rgb(30, 8, 16)}}, 0.9, true);
  stroke(cr, kGold, 5.0);
  for (double bx = x - 22; bx < x + e.width + 26; bx += 22)
  {
    bulb(cr, bx, baseline - size * 0.95, 3.5, 1.0);
    bulb(cr, bx, baseline + size * 0.3, 3.5, 1.0);
  }
  // The letters, gold with a glow.
  cairo_move_to(cr, x, baseline);
  cairo_text_path(cr, s);
  cairo_set_line_width(cr, 10);
  setRgba(cr, rgb(255, 170, 60), 0.25);
  cairo_stroke_preserve(cr);
  fillLinear(cr, 0, baseline - size * 0.8, 0, baseline, {{0.0, kGoldHi}, {0.5, kGold}, {1.0, rgb(255, 140, 60)}});
}

} // namespace

// A gold sequin curtain in deep folds, catching the lights in glints,
// under a swagged valance with a fringe; SWEEPS WEEK in marquee bulbs on it
// every screen and a half.
Texture bakeSweepsFar(const Renderer& r, const Theme& t, int layerW)
{
  (void)t;
  const int h = kFarH;
  VectorImage img(layerW, h);
  cairo_t* cr = img.cr();
  const double W = layerW;
  const Color curtain = rgb(150, 100, 40);
  for (double x = 0; x < W; x += 80)
  {
    cairo_rectangle(cr, x, 0, 80, h);
    fillLinear(cr, x, 0, x + 80, 0,
      {{0.0, darken(curtain, 0.7f)}, {0.25, darken(curtain, 0.2f)}, {0.45, lighten(curtain, 0.25f)}, {0.6, curtain}, {1.0, darken(curtain, 0.7f)}});
  }
  Rng rng(41051u);
  for (int i = 0; i < 2600; ++i)
  {
    const double x = rng.range(0, float(W)), y = rng.range(0, float(h));
    const double fold = std::fmod(x, 80.0) / 80.0;
    const double light = std::max(0.0, 1.0 - std::abs(fold - 0.45) * 3.0);
    if (rng.uniform() < 0.06f + light * 0.1)
      sequin(cr, x, y, rng.range(0.6f, 1.3f), 0.3 + light * 0.5);
    else
    {
      cairo_arc(cr, x, y, 1.0, 0, 2 * kPi);
      setRgba(cr, lighten(curtain, 0.5f), 0.08 + light * 0.25);
      cairo_fill(cr);
    }
  }
  // Darker towards the floor (the curtain falls into shadow), a haze over all.
  cairo_rectangle(cr, 0, 0, W, h);
  fillLinear(cr, 0, 0, 0, h, {{0.0, rgb(20, 4, 10)}, {0.4, rgb(40, 10, 20)}, {1.0, rgb(10, 2, 6)}}, 0.55);
  // The valance: red velvet swags with a gold fringe.
  for (double x = 0; x < W; x += 320)
  {
    cairo_move_to(cr, x, 0);
    cairo_curve_to(cr, x + 60, 150, x + 260, 150, x + 320, 0);
    cairo_close_path(cr);
    fillLinear(cr, 0, 0, 0, 120, {{0.0, kVelLo}, {0.6, kVel}, {1.0, kVelHi}});
    cairo_move_to(cr, x, 0);
    cairo_curve_to(cr, x + 60, 150, x + 260, 150, x + 320, 0);
    stroke(cr, kGold, 6.0);
    for (double s = 0.05; s < 1.0; s += 0.05)
    {
      const double u = s, mu = 1 - u;
      const double px = mu * mu * mu * x + 3 * mu * mu * u * (x + 60) + 3 * mu * u * u * (x + 260) + u * u * u * (x + 320);
      const double py = 3 * mu * mu * u * 150 + 3 * mu * u * u * 150;
      cairo_move_to(cr, px, py + 3);
      cairo_line_to(cr, px, py + 18);
      stroke(cr, kGold, 2.0, 0.9);
    }
  }
  for (double x = 640; x < W; x += 1280)
    marqueeWord(cr, "SWEEPS WEEK", x, 330, 72);
  return img.toTexture(r, 0.0f, 0.0f);
}

// A lighting truss across the top with PAR cans and their coloured light,
// glitter stars hanging on wires below it.
Texture bakeSweepsNear(const Renderer& r, const Theme& t, int layerW)
{
  (void)t;
  VectorImage img(layerW, kNearH);
  cairo_t* cr = img.cr();
  const double W = layerW;
  Rng rng(41061u);
  // The truss.
  for (const double y : {40.0, 72.0})
  {
    cairo_move_to(cr, 0, y);
    cairo_line_to(cr, W, y);
    stroke(cr, rgb(60, 50, 56), 5.0);
  }
  for (double x = 0; x < W; x += 32)
  {
    cairo_move_to(cr, x, 40);
    cairo_line_to(cr, x + 32, 72);
    stroke(cr, rgb(50, 40, 46), 2.0);
  }
  static constexpr Color kGels[] = {rgb(255, 120, 170), rgb(255, 200, 90), rgb(120, 170, 255), rgb(255, 90, 90)};
  for (double x = 90; x < W; x += 180)
  {
    const Color gel = kGels[std::size_t(int(x / 180) % 4)];
    beam(cr, x, 104, rng.range(-0.25f, 0.25f), 700, 110, gel, 0.07);
    cairo_rectangle(cr, x - 14, 74, 28, 32);
    fillLinear(cr, x - 14, 0, x + 14, 0, {{0.0, rgb(60, 56, 60)}, {1.0, rgb(16, 14, 16)}});
    softDot(cr, x, 106, 16, 6, gel, 0.8);
  }
  // Glitter stars on wires.
  for (double x = 200; x < W; x += 420)
  {
    const double drop = 160 + rng.range(0, 160);
    cairo_move_to(cr, x, 72);
    cairo_line_to(cr, x, 72 + drop);
    stroke(cr, rgb(180, 170, 160), 1.0, 0.4);
    star(cr, x, 72 + drop + 28, 30, 13);
    fillLinear(cr, x - 30, 0, x + 30, 0, {{0.0, kGoldLo}, {0.5, kGoldHi}, {1.0, kGold}}, 1.0, true);
    stroke(cr, kGoldLo, 1.5);
    for (int k = 0; k < 6; ++k)
      sequin(cr, x + rng.range(-18, 18), 72 + drop + 28 + rng.range(-14, 14), 0.8, 0.8);
  }
  // In the dark under the lights.
  cairo_set_operator(cr, CAIRO_OPERATOR_ATOP);
  cairo_rectangle(cr, 0, 0, W, kNearH);
  setRgba(cr, rgb(30, 10, 20), 0.3);
  cairo_fill(cr);
  cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
  return img.toTexture(r, 0.0f, 0.0f);
}

// The stage door (96 x 144; (0, 0) the top-left of the 64 x 128 exit area).
Texture bakeSweepsExitBase(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(96, 144);
  cairo_t* cr = img.cr();
  cairo_translate(cr, 16, 8);
  // The frame of bulbs.
  cairo_rectangle(cr, -6, 6, 76, 122);
  fillLinear(cr, 0, 6, 0, 128, {{0.0, rgb(70, 30, 40)}, {1.0, rgb(30, 10, 18)}}, 1.0, true);
  stroke(cr, kGold, 2.0);
  // The door: red lacquer, a gold star, a brass push bar.
  cairo_rectangle(cr, 6, 20, 52, 108);
  fillLinear(cr, 6, 0, 58, 0, {{0.0, rgb(150, 20, 40)}, {0.4, rgb(210, 40, 60)}, {1.0, rgb(90, 10, 24)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  rect(cr, 10, 24, 44, 1.5, rgb(255, 200, 200), 0.5);
  star(cr, 32, 52, 15, 6.5);
  fillLinear(cr, 17, 0, 47, 0, {{0.0, kGoldLo}, {0.5, kGoldHi}, {1.0, kGold}}, 1.0, true);
  stroke(cr, kGoldLo, 1.0);
  cairo_rectangle(cr, 10, 82, 44, 6);
  fillLinear(cr, 0, 82, 0, 88, {{0.0, kGoldHi}, {1.0, kGoldLo}});
  for (double y = 12; y < 128; y += 14)
  {
    bulb(cr, -2, y, 3.0, 1.0);
    bulb(cr, 66, y, 3.0, 1.0);
  }
  // The sign.
  cairo_rectangle(cr, -4, -6, 72, 16);
  fillLinear(cr, 0, -6, 0, 10, {{0.0, rgb(40, 14, 20)}, {1.0, rgb(20, 4, 10)}}, 1.0, true);
  stroke(cr, kGold, 1.5);
  text(cr, "STAGE DOOR", 32, 6, 9, kGoldHi);
  return img.toTexture(r, 16.0f, 8.0f);
}

} // namespace gr
