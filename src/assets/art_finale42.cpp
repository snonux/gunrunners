// Level 42, The Live Finale (theme key main_stage, look "main_stage"): the
// season's last live show on the main stage, midnight blue, gold trim and
// ON AIR red. See art_finale42.hpp.

#include "assets/art_finale42.hpp"

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

void roundRect(cairo_t* cr, double x, double y, double w, double h, double r)
{
  r = std::min(r, std::min(w, h) * 0.5);
  cairo_new_sub_path(cr);
  cairo_arc(cr, x + w - r, y + r, r, -kPi * 0.5, 0);
  cairo_arc(cr, x + w - r, y + h - r, r, 0, kPi * 0.5);
  cairo_arc(cr, x + r, y + h - r, r, kPi * 0.5, kPi);
  cairo_arc(cr, x + r, y + r, r, kPi, kPi * 1.5);
  cairo_close_path(cr);
}

// Centred text; `glow` > 0 strokes a soft halo of `halo` under it first.
void text(cairo_t* cr, const char* s, double cx, double baseline, double size, Color c, double a = 1.0, double glow = 0.0,
  Color halo = rgb(0, 0, 0))
{
  selectGameFont(cr);
  cairo_set_font_size(cr, size);
  cairo_text_extents_t e;
  cairo_text_extents(cr, s, &e);
  cairo_move_to(cr, cx - e.width * 0.5 - e.x_bearing, baseline);
  if (glow > 0.0)
  {
    cairo_text_path(cr, s);
    setRgba(cr, halo, a * 0.35);
    cairo_set_line_width(cr, glow);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_stroke(cr);
    cairo_move_to(cr, cx - e.width * 0.5 - e.x_bearing, baseline);
  }
  setRgba(cr, c, a);
  cairo_show_text(cr, s);
}

// A beam of light from (x, y) spreading to `spread` either side at `len`
// away along `angle` (radians, 0 = straight down, pi = straight up).
void beam(cairo_t* cr, double x, double y, double angle, double len, double spread, Color c, double a)
{
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_rotate(cr, angle);
  cairo_move_to(cr, -5, 0);
  cairo_line_to(cr, 5, 0);
  cairo_line_to(cr, spread, len);
  cairo_line_to(cr, -spread, len);
  cairo_close_path(cr);
  cairo_pattern_t* g = cairo_pattern_create_linear(0, 0, 0, len);
  stop(g, 0.0, c, a);
  stop(g, 0.5, c, a * 0.4);
  stop(g, 1.0, c, 0.0);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
  cairo_restore(cr);
}

constexpr Color kNavy = rgb(20, 24, 66);
constexpr Color kNavyHi = rgb(58, 66, 140);
constexpr Color kNavyLo = rgb(6, 8, 26);
constexpr Color kGold = rgb(236, 186, 74);
constexpr Color kGoldHi = rgb(255, 242, 180);
constexpr Color kGoldLo = rgb(124, 84, 24);
constexpr Color kLed = rgb(90, 200, 255);
constexpr Color kChrome = rgb(196, 204, 222);
constexpr Color kChromeLo = rgb(70, 76, 98);
constexpr Color kInk = rgb(6, 6, 16);

// A chrome tube from (x0, y0) to (x1, y1): dark edge, bright core.
void tube(cairo_t* cr, double x0, double y0, double x1, double y1, double w)
{
  cairo_move_to(cr, x0, y0);
  cairo_line_to(cr, x1, y1);
  stroke(cr, kChromeLo, w);
  cairo_move_to(cr, x0, y0);
  cairo_line_to(cr, x1, y1);
  stroke(cr, kChrome, w * 0.55);
  cairo_move_to(cr, x0, y0 - w * 0.12);
  cairo_line_to(cr, x1, y1 - w * 0.12);
  stroke(cr, rgb(255, 255, 255), w * 0.18, 0.7);
}

} // namespace

bool isFinale(const Theme& t) { return std::string_view(t.look) == "main_stage"; }

// An LED riser: a deep navy panel with its pixel grid faintly showing, the
// seams between the modules, a sheen across the top-left; variant 2 has a
// few pixels lit gold.
Texture bakeFinaleSolid(const Renderer& r, const Theme& t, int variant)
{
  (void)t;
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  const Color base = variant == 1 ? darken(kNavy, 0.18f) : kNavy;
  cairo_rectangle(cr, 0, 0, 64, 64);
  fillLinear(cr, 0, 0, 64, 64, {{0.0, lighten(base, 0.08f)}, {0.5, base}, {1.0, darken(base, 0.35f)}});
  // The pixel grid.
  for (double y = 4; y < 64; y += 8)
    for (double x = 4; x < 64; x += 8)
    {
      cairo_arc(cr, x, y, 1.3, 0, 2 * kPi);
      setRgba(cr, kNavyHi, 0.45);
      cairo_fill(cr);
    }
  if (variant == 2)
  {
    Rng rng(42011u);
    for (int i = 0; i < 5; ++i)
    {
      const double x = 4 + 8 * std::floor(rng.range(0, 7.99f)), y = 4 + 8 * std::floor(rng.range(0, 7.99f));
      softDot(cr, x, y, 5, 5, kGold, 0.5);
      cairo_arc(cr, x, y, 1.6, 0, 2 * kPi);
      setRgba(cr, kGoldHi, 0.95);
      cairo_fill(cr);
    }
  }
  // The module seams and a sheen.
  rect(cr, 0, 31, 64, 2, kNavyLo, 0.9);
  rect(cr, 31, 0, 2, 64, kNavyLo, 0.9);
  rect(cr, 0, 0, 64, 1, kNavyHi, 0.5);
  rect(cr, 0, 33, 64, 1, kNavyHi, 0.35);
  cairo_move_to(cr, 0, 0);
  cairo_line_to(cr, 26, 0);
  cairo_line_to(cr, 0, 26);
  cairo_close_path(cr);
  setRgba(cr, rgb(255, 255, 255), 0.04);
  cairo_fill(cr);
  return img.toTexture(r, 0.0f, 0.0f);
}

// Where you stand: the gloss-black stage floor with the lights mirrored in
// it, a gold edge, a blue LED strip glowing under the nosing, the riser
// below in its shadow.
Texture bakeFinaleSolidTop(const Renderer& r, const Theme& t, float topOffset, int height)
{
  (void)t;
  VectorImage img(64, height);
  cairo_t* cr = img.cr();
  const double y = topOffset;
  // The floor.
  cairo_rectangle(cr, 0, y - 5, 64, 10);
  fillLinear(cr, 0, y - 5, 0, y + 5, {{0.0, rgb(150, 150, 200)}, {0.2, rgb(30, 32, 60)}, {1.0, rgb(8, 8, 20)}});
  rect(cr, 0, y - 5, 64, 1.2, rgb(230, 235, 255), 0.75);
  softDot(cr, 20, y - 1, 18, 1.8, kGoldHi, 0.35);
  softDot(cr, 50, y - 1, 10, 1.4, kLed, 0.3);
  // The gold edge.
  cairo_rectangle(cr, 0, y + 5, 64, 4);
  fillLinear(cr, 0, y + 5, 0, y + 9, {{0.0, kGoldHi}, {0.4, kGold}, {1.0, kGoldLo}});
  // The LED strip and its glow on the riser.
  rect(cr, 0, y + 9, 64, 6, rgb(10, 14, 36));
  softDot(cr, 32, y + 14, 44, 9, kLed, 0.35);
  rect(cr, 0, y + 11, 64, 2.4, rgb(170, 230, 255));
  rect(cr, 0, y + 11.6, 64, 1.0, rgb(255, 255, 255), 0.8);
  // Its shadow down the riser.
  cairo_pattern_t* g = cairo_pattern_create_linear(0, y + 15, 0, y + 34);
  stop(g, 0.0, kInk, 0.6);
  stop(g, 1.0, kInk, 0.0);
  cairo_rectangle(cr, 0, y + 15, 64, 19);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
  return img.toTexture(r, 0.0f, topOffset);
}

// A chrome truss deck: a brushed plate on top with a blue light line
// along its front, the box truss's tubes and diagonals under it. 64 x 40,
// the walking surface at the top; it tiles.
Texture bakeFinalePlatform(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(64, 40);
  cairo_t* cr = img.cr();
  // The truss under the plate.
  for (double x = 0; x < 64; x += 32)
  {
    tube(cr, x, 30, x + 16, 10, 3.0);
    tube(cr, x + 16, 10, x + 32, 30, 3.0);
  }
  tube(cr, -2, 30, 66, 30, 4.5);
  // The plate.
  cairo_rectangle(cr, 0, 0, 64, 9);
  fillLinear(cr, 0, 0, 0, 9, {{0.0, rgb(250, 250, 255)}, {0.3, kChrome}, {1.0, kChromeLo}});
  for (double x = 3; x < 64; x += 7)
  {
    cairo_move_to(cr, x, 2);
    cairo_line_to(cr, x + 3, 2);
    stroke(cr, rgb(120, 126, 150), 0.8, 0.6);
  }
  // The light line.
  rect(cr, 0, 9, 64, 3, rgb(10, 14, 36));
  softDot(cr, 32, 10.5, 40, 4, kLed, 0.5);
  rect(cr, 0, 9.8, 64, 1.4, rgb(200, 240, 255));
  // The shadow under it.
  cairo_pattern_t* g = cairo_pattern_create_linear(0, 32, 0, 40);
  stop(g, 0.0, kInk, 0.35);
  stop(g, 1.0, kInk, 0.0);
  cairo_rectangle(cr, 0, 32, 64, 8);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
  return img.toTexture(r, 0.0f, 0.0f);
}

// One block of chrome ladder, its rungs lit blue from inside. Drawn 16 px
// left of its block, the rails straddling the left cell column.
Texture bakeFinaleLadder(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  for (const double y : {12.0, 44.0})
  {
    softDot(cr, 32, y, 22, 5, kLed, 0.35);
    roundRect(cr, 14, y - 3, 36, 6, 3);
    fillLinear(cr, 0, y - 3, 0, y + 3, {{0.0, rgb(220, 245, 255)}, {0.5, kLed}, {1.0, rgb(30, 90, 160)}}, 1.0, true);
    stroke(cr, kInk, 1.0);
  }
  for (const double x : {9.0, 49.0})
  {
    cairo_rectangle(cr, x, -2, 7, 68);
    fillLinear(cr, x, 0, x + 7, 0, {{0.0, rgb(255, 255, 255)}, {0.35, kChrome}, {1.0, kChromeLo}}, 1.0, true);
    stroke(cr, kInk, 1.0);
  }
  return img.toTexture(r, 16.0f, 0.0f);
}

// A row of pyro gerbs: black canisters on a hazard-striped rail, each
// throwing a fountain of gold sparks.
Texture bakeFinaleSpikes(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  softDot(cr, 32, 40, 34, 24, rgb(255, 170, 60), 0.4);
  Rng rng(42021u);
  for (int i = 0; i < 2; ++i)
  {
    const double cx = 16 + i * 32;
    // The sparks.
    for (int k = 0; k < 22; ++k)
    {
      const double a = -kPi / 2 + rng.range(-0.45f, 0.45f), len = rng.range(10, 34);
      const double x1 = cx + std::cos(a) * len, y1 = 40 + std::sin(a) * len;
      cairo_move_to(cr, cx + std::cos(a) * 4, 40 + std::sin(a) * 4);
      cairo_line_to(cr, x1, y1);
      stroke(cr, k % 3 ? kGoldHi : rgb(255, 255, 255), rng.range(0.8f, 1.6f), rng.range(0.5f, 0.95f));
      cairo_arc(cr, x1, y1, 1.0, 0, 2 * kPi);
      setRgba(cr, rgb(255, 250, 220), 0.9);
      cairo_fill(cr);
    }
    softDot(cr, cx, 38, 10, 8, rgb(255, 255, 230), 0.9);
    // The canister.
    cairo_rectangle(cr, cx - 7, 40, 14, 15);
    fillLinear(cr, cx - 7, 0, cx + 7, 0, {{0.0, rgb(60, 60, 76)}, {0.4, rgb(26, 26, 36)}, {1.0, rgb(8, 8, 14)}}, 1.0, true);
    stroke(cr, kInk, 1.0);
    rect(cr, cx - 7, 44, 14, 2, rgb(255, 60, 70), 0.9);
  }
  // The rail, yellow and black.
  rect(cr, 0, 54, 64, 10, rgb(20, 20, 28));
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

// The arena's dark: midnight blue going black at the top, beams climbing
// out of the show below, a haze, a ring of lights high up.
Texture bakeFinaleSky(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(kScreenW, kScreenH);
  cairo_t* cr = img.cr();
  cairo_rectangle(cr, 0, 0, kScreenW, kScreenH);
  fillLinear(cr, 0, 0, 0, kScreenH, {{0.0, rgb(2, 2, 10)}, {0.5, rgb(10, 12, 40)}, {1.0, rgb(30, 30, 90)}});
  struct Beam
  {
    double x, angle, spread;
    Color c;
    double a;
  };
  static constexpr Beam kBeams[] = {{140, kPi + 0.35, 90, kLed, 0.10}, {420, kPi - 0.1, 70, kGoldHi, 0.08},
    {760, kPi + 0.12, 70, rgb(160, 120, 255), 0.08}, {1140, kPi - 0.38, 90, kLed, 0.10}, {600, kPi + 0.55, 80, kGoldHi, 0.06}};
  for (const auto& b : kBeams)
    beam(cr, b.x, kScreenH + 20, b.angle, 900, b.spread, b.c, b.a);
  // The haze.
  softDot(cr, 640, 560, 760, 220, rgb(60, 70, 160), 0.25);
  // A ring of lights high up (the halo rig), seen from below.
  for (int i = 0; i < 28; ++i)
  {
    const double a = i * 2 * kPi / 28;
    const double x = 640 + std::cos(a) * 560, y = 60 + std::sin(a) * 40;
    softDot(cr, x, y, 10, 10, i % 4 ? kGoldHi : kLed, 0.5);
    cairo_arc(cr, x, y, 1.8, 0, 2 * kPi);
    setRgba(cr, rgb(255, 255, 255), 0.8);
    cairo_fill(cr);
  }
  Rng rng(42041u);
  for (int i = 0; i < 70; ++i)
  {
    cairo_arc(cr, rng.range(0, float(kScreenW)), rng.range(0, float(kScreenH)), rng.range(0.5f, 1.2f), 0, 2 * kPi);
    setRgba(cr, kGoldHi, rng.range(0.08f, 0.3f));
    cairo_fill(cr);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

namespace
{

constexpr int kFarH = kScreenH + 360;
constexpr int kNearH = kScreenH + 420;

// One of the audience in silhouette, head and shoulders, `s` the head's
// radius, the chin at (x, y).
void fan(cairo_t* cr, double x, double y, double s, Color c)
{
  cairo_arc(cr, x, y - s, s, 0, 2 * kPi);
  cairo_move_to(cr, x - s * 2.4, y + s * 3);
  cairo_curve_to(cr, x - s * 2.3, y + s * 0.6, x - s, y + s * 0.2, x, y + s * 0.2);
  cairo_curve_to(cr, x + s, y + s * 0.2, x + s * 2.3, y + s * 0.6, x + s * 2.4, y + s * 3);
  cairo_close_path(cr);
  setRgba(cr, c, 1.0);
  cairo_fill(cr);
}

} // namespace

// The far wall: a curved LED wall in modules, THE LIVE FINALE and a big
// gold 42 on it every screen, its blue glow falling on the audience's
// tiers below, phones and glow sticks held up.
Texture bakeFinaleFar(const Renderer& r, const Theme& t, int layerW)
{
  (void)t;
  const int h = kFarH;
  VectorImage img(layerW, h);
  cairo_t* cr = img.cr();
  const double W = layerW;
  // The wall's frame and its modules.
  const double wy0 = 120, wy1 = 560;
  cairo_rectangle(cr, 0, wy0 - 14, W, wy1 - wy0 + 28);
  fillLinear(cr, 0, wy0 - 14, 0, wy1 + 14, {{0.0, rgb(30, 30, 50)}, {1.0, rgb(12, 12, 24)}});
  cairo_rectangle(cr, 0, wy0, W, wy1 - wy0);
  fillLinear(cr, 0, wy0, 0, wy1, {{0.0, rgb(14, 20, 70)}, {0.5, rgb(26, 40, 120)}, {1.0, rgb(10, 14, 50)}});
  // A slow wave of light across the wall.
  for (double x = 0; x < W; x += 6)
  {
    const double v = 0.5 + 0.5 * std::sin(x / W * 2 * kPi * 4);
    cairo_rectangle(cr, x, wy0 + 30 + v * 120, 6, 160);
    setRgba(cr, rgb(70, 140, 255), 0.10 + 0.08 * v);
    cairo_fill(cr);
  }
  for (double x = 0; x < W; x += 80)
    rect(cr, x, wy0, 1.5, wy1 - wy0, rgb(4, 6, 20), 0.8);
  for (double y = wy0; y < wy1; y += 80)
    rect(cr, 0, y, W, 1.5, rgb(4, 6, 20), 0.8);
  for (double x = 640; x < W; x += 1280)
  {
    // The 42, in gold with a glow, and the show's name.
    selectGameFont(cr);
    cairo_set_font_size(cr, 260);
    cairo_text_extents_t e;
    cairo_text_extents(cr, "42", &e);
    const double tx = x - e.width * 0.5 - e.x_bearing, ty = wy0 + 300;
    softDot(cr, x, ty - 100, 260, 160, kGold, 0.25);
    cairo_move_to(cr, tx, ty);
    cairo_text_path(cr, "42");
    cairo_set_line_width(cr, 12);
    setRgba(cr, rgb(255, 200, 80), 0.3);
    cairo_stroke_preserve(cr);
    fillLinear(cr, 0, ty - 200, 0, ty, {{0.0, kGoldHi}, {0.5, kGold}, {1.0, rgb(200, 110, 30)}});
    text(cr, "THE LIVE FINALE", x, wy0 + 380, 54, rgb(255, 255, 255), 0.95, 14, kLed);
    text(cr, "SEASON RECAP  -  LIVE ON AIR", x, wy0 + 420, 22, kLed, 0.9);
  }
  // Its glow, and the audience's tiers below.
  softDot(cr, W * 0.5, wy1 + 40, W * 0.7, 120, rgb(60, 100, 255), 0.25);
  Rng rng(42051u);
  for (int row = 0; row < 7; ++row)
  {
    const double y = wy1 + 70 + row * 46, s = 9 + row * 1.4;
    const Color c = lerpColor(rgb(26, 30, 70), rgb(6, 6, 18), float(row) / 6.0f);
    rect(cr, 0, y + s * 2.2, W, 46, darken(c, 0.3f));
    for (double x = rng.range(0, 20); x < W; x += s * 4.2 + rng.range(0, 6))
    {
      fan(cr, x, y, s, c);
      const double u = rng.uniform();
      if (u < 0.12)
      {
        // A phone held up.
        softDot(cr, x + s * 1.6, y - s * 2.2, s * 1.4, s * 1.4, rgb(220, 235, 255), 0.4);
        rect(cr, x + s * 1.2, y - s * 2.8, s * 0.8, s * 1.3, rgb(230, 240, 255));
      }
      else if (u < 0.2)
      {
        // A glow stick.
        const Color gs = u < 0.16 ? kLed : rgb(255, 90, 180);
        cairo_move_to(cr, x + s * 1.4, y - s * 0.5);
        cairo_line_to(cr, x + s * 2.0, y - s * 3.0);
        stroke(cr, gs, s * 0.45, 0.95);
        softDot(cr, x + s * 1.7, y - s * 1.8, s * 1.2, s * 1.6, gs, 0.4);
      }
    }
  }
  cairo_rectangle(cr, 0, wy1 + 300, W, h - wy1 - 300);
  fillLinear(cr, 0, wy1 + 300, 0, h, {{0.0, rgb(4, 4, 14)}, {1.0, rgb(2, 2, 8)}}, 0.6);
  return img.toTexture(r, 0.0f, 0.0f);
}

// The rig: an arched truss across the top with moving heads hanging off it,
// their beams cutting down through the haze, gold streamers between them.
Texture bakeFinaleNear(const Renderer& r, const Theme& t, int layerW)
{
  (void)t;
  VectorImage img(layerW, kNearH);
  cairo_t* cr = img.cr();
  const double W = layerW;
  Rng rng(42061u);
  auto archY = [](double x) { return 70.0 + 26.0 * std::cos(std::fmod(x, 640.0) / 640.0 * 2 * kPi); };
  // The truss.
  for (const double off : {0.0, 24.0})
  {
    cairo_move_to(cr, 0, archY(0) + off);
    for (double x = 8; x <= W; x += 8)
      cairo_line_to(cr, x, archY(x) + off);
    stroke(cr, rgb(70, 72, 96), 4.0);
  }
  for (double x = 0; x < W; x += 24)
  {
    cairo_move_to(cr, x, archY(x));
    cairo_line_to(cr, x + 24, archY(x + 24) + 24);
    stroke(cr, rgb(56, 58, 80), 1.8);
  }
  static constexpr Color kGels[] = {rgb(90, 200, 255), rgb(255, 215, 90), rgb(170, 120, 255), rgb(255, 80, 120)};
  for (double x = 80; x < W; x += 160)
  {
    const Color gel = kGels[std::size_t(int(x / 160) % 4)];
    const double y = archY(x) + 24;
    beam(cr, x, y + 30, rng.range(-0.5f, 0.5f), 760, 70, gel, 0.08);
    // The moving head: a yoke and a round head.
    cairo_move_to(cr, x - 12, y);
    cairo_line_to(cr, x - 12, y + 22);
    cairo_move_to(cr, x + 12, y);
    cairo_line_to(cr, x + 12, y + 22);
    stroke(cr, rgb(40, 40, 54), 4.0);
    cairo_arc(cr, x, y + 22, 11, 0, 2 * kPi);
    fillLinear(cr, x - 11, 0, x + 11, 0, {{0.0, rgb(70, 72, 90)}, {1.0, rgb(14, 14, 22)}});
    softDot(cr, x, y + 30, 12, 6, gel, 0.9);
  }
  // Gold streamers.
  for (double x = 160; x < W; x += 320)
  {
    const double y = archY(x) + 24, len = 120 + rng.range(0, 140);
    for (int k = 0; k < 5; ++k)
    {
      const double sx = x - 16 + k * 8;
      cairo_move_to(cr, sx, y);
      cairo_curve_to(cr, sx + 10, y + len * 0.3, sx - 10, y + len * 0.6, sx + 4, y + len);
      stroke(cr, k % 2 ? kGoldHi : kGold, 2.2, 0.85);
    }
  }
  cairo_set_operator(cr, CAIRO_OPERATOR_ATOP);
  cairo_rectangle(cr, 0, 0, W, kNearH);
  setRgba(cr, rgb(10, 10, 30), 0.25);
  cairo_fill(cr);
  cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
  return img.toTexture(r, 0.0f, 0.0f);
}

// The master switch (96 x 144; (0, 0) the top-left of the 64 x 128 exit
// area): a steel panel with rivets, a big knife switch with a red handle
// thrown up, MASTER over it, a red lamp.
Texture bakeFinaleExitBase(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(96, 144);
  cairo_t* cr = img.cr();
  cairo_translate(cr, 16, 8);
  // The panel.
  roundRect(cr, -6, 14, 76, 114, 6);
  fillLinear(cr, -6, 0, 70, 0, {{0.0, rgb(110, 116, 140)}, {0.3, rgb(70, 74, 96)}, {1.0, rgb(30, 32, 46)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  for (const double rx : {0.0, 64.0})
    for (const double ry : {20.0, 120.0})
    {
      cairo_arc(cr, rx, ry, 2.0, 0, 2 * kPi);
      setRgba(cr, kChrome, 1.0);
      cairo_fill(cr);
    }
  // The hazard stripes along the foot.
  cairo_save(cr);
  cairo_rectangle(cr, -4, 112, 72, 12);
  cairo_clip(cr);
  for (double x = -16; x < 72; x += 12)
  {
    cairo_move_to(cr, x, 124);
    cairo_line_to(cr, x + 6, 112);
    cairo_line_to(cr, x + 12, 112);
    cairo_line_to(cr, x + 6, 124);
    cairo_close_path(cr);
    setRgba(cr, rgb(240, 200, 40), 0.9);
    cairo_fill(cr);
  }
  cairo_restore(cr);
  // The switch: its contacts, its blades thrown up, the red handle.
  rect(cr, 18, 86, 28, 10, rgb(30, 30, 40));
  rect(cr, 20, 88, 6, 6, rgb(200, 150, 70));
  rect(cr, 38, 88, 6, 6, rgb(200, 150, 70));
  for (const double bx : {23.0, 41.0})
  {
    cairo_move_to(cr, bx, 90);
    cairo_line_to(cr, bx, 44);
    stroke(cr, kChromeLo, 5.0);
    cairo_move_to(cr, bx, 90);
    cairo_line_to(cr, bx, 44);
    stroke(cr, kChrome, 2.5);
  }
  roundRect(cr, 14, 34, 36, 12, 5);
  fillLinear(cr, 0, 34, 0, 46, {{0.0, rgb(255, 120, 120)}, {0.5, rgb(220, 30, 40)}, {1.0, rgb(120, 10, 20)}}, 1.0, true);
  stroke(cr, kInk, 1.2);
  // The lamp.
  softDot(cr, 54, 64, 10, 10, rgb(255, 60, 60), 0.6);
  cairo_arc(cr, 54, 64, 4, 0, 2 * kPi);
  setRgba(cr, rgb(255, 120, 110), 1.0);
  cairo_fill(cr);
  // The plate.
  roundRect(cr, 0, -4, 64, 16, 3);
  fillLinear(cr, 0, -4, 0, 12, {{0.0, kGoldHi}, {0.5, kGold}, {1.0, kGoldLo}}, 1.0, true);
  stroke(cr, kInk, 1.2);
  text(cr, "MASTER", 32, 8, 10, kInk);
  return img.toTexture(r, 16.0f, 8.0f);
}

} // namespace gr
