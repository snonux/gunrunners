// Level 40, The Fly Tower (theme key fly_tower, look "fly_tower"): a
// theatre's fly tower during the show, black steel in stage haze. The
// masses are black-painted deck plate with a diamond tread, riveted at the
// seams; now and then an I-beam's web with a bolted splice, or a DMX
// junction box. Where you stand, the deck's black top with a sheen of
// amber light, glow tape along the front edge and a steel angle nosing.
// The one-way ledges are catwalks (bar grating, kick plate, stringer). The
// ladders are black steel with glow-tape marks; the spikes stage-brace
// spikes in a tray. Behind: haze lit by coloured beams, the sooty brick
// back wall with its columns, counterweight arbors and purchase lines, and
// nearer, battens with black borders and lamps. The rigging the runner
// climbs through is drawn by the level (world_flytower_draw.cpp).

#include "assets/art_flytower40.hpp"

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
constexpr Color kPlate = rgb(38, 38, 44);
constexpr Color kPlateHi = rgb(78, 78, 88);
constexpr Color kPlateLo = rgb(18, 18, 22);
constexpr Color kSteel = rgb(120, 122, 132);
constexpr Color kSteelHi = rgb(196, 198, 208);
constexpr Color kSteelLo = rgb(58, 60, 68);
constexpr Color kInk = rgb(8, 8, 12);
constexpr Color kAmber = rgb(255, 170, 60);
constexpr Color kMagenta = rgb(230, 70, 170);
constexpr Color kCyan = rgb(70, 190, 255);
constexpr Color kGlow = rgb(170, 255, 190); // glow tape
constexpr Color kBrick = rgb(44, 32, 32);

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

void rivet(cairo_t* cr, double x, double y, double r = 1.8)
{
  cairo_arc(cr, x, y, r, 0, 2 * kPi);
  setRgba(cr, kPlateLo, 1.0);
  cairo_fill(cr);
  cairo_arc(cr, x - r * 0.3, y - r * 0.3, r * 0.55, 0, 2 * kPi);
  setRgba(cr, kPlateHi, 0.9);
  cairo_fill(cr);
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

// The deck plate: black paint over steel, a diamond tread, seams and
// rivets, a breath of amber from the stage below on the top edge.
void plate(cairo_t* cr, Rng& rng)
{
  rect(cr, 0, 0, 64, 64, kInk);
  cairo_rectangle(cr, 1, 1, 62, 62);
  fillLinear(cr, 0, 1, 0, 63, {{0.0, lighten(kPlate, 0.05f)}, {0.5, kPlate}, {1.0, darken(kPlate, 0.25f)}});
  // The tread: little raised lozenges turned alternately.
  for (int row = 0; row < 8; ++row)
    for (int col = 0; col < 8; ++col)
    {
      const double cx = 4 + col * 8.0 + (row % 2 ? 4.0 : 0.0), cy = 4 + row * 8.0;
      if (cx > 62)
        continue;
      cairo_save(cr);
      cairo_translate(cr, cx, cy);
      cairo_rotate(cr, (row + col) % 2 ? 0.75 : -0.75);
      cairo_rectangle(cr, -2.6, -0.8, 5.2, 1.6);
      cairo_restore(cr);
      setRgba(cr, kPlateHi, 0.35);
      cairo_fill(cr);
    }
  // Scuffs where boots go.
  for (int i = 0; i < 16; ++i)
    rect(cr, rng.range(3, 60), rng.range(3, 60), rng.range(1, 4), 1.0, i % 2 ? kPlateHi : kPlateLo, rng.range(0.15f, 0.4f));
  // Seams and rivets.
  cairo_move_to(cr, 1.5, 62.5);
  cairo_line_to(cr, 1.5, 1.5);
  cairo_line_to(cr, 62.5, 1.5);
  stroke(cr, kPlateHi, 1.0, 0.7);
  cairo_move_to(cr, 62.5, 1.5);
  cairo_line_to(cr, 62.5, 62.5);
  cairo_line_to(cr, 1.5, 62.5);
  stroke(cr, kPlateLo, 1.2, 0.9);
  for (const double x : {6.0, 32.0, 58.0})
  {
    rivet(cr, x, 5.5);
    rivet(cr, x, 58.5);
  }
  rivet(cr, 5.5, 32.0);
  rivet(cr, 58.5, 32.0);
  rect(cr, 2, 2, 60, 1.4, kAmber, 0.08);
}

// Variant 1: an I-beam's web with a bolted splice plate.
void beamWeb(cairo_t* cr)
{
  rect(cr, 0, 0, 64, 64, kInk);
  // The flanges top and bottom, the web between.
  cairo_rectangle(cr, 1, 1, 62, 62);
  fillLinear(cr, 0, 0, 64, 0, {{0.0, rgb(30, 30, 36)}, {0.5, rgb(44, 44, 52)}, {1.0, rgb(26, 26, 30)}});
  for (const double y : {1.0, 54.0})
  {
    cairo_rectangle(cr, 1, y, 62, 9);
    fillLinear(cr, 0, y, 0, y + 9, {{0.0, rgb(84, 84, 94)}, {0.4, rgb(52, 52, 60)}, {1.0, rgb(22, 22, 26)}});
  }
  // The splice plate.
  cairo_rectangle(cr, 18, 14, 28, 36);
  fillLinear(cr, 18, 14, 46, 50, {{0.0, rgb(70, 70, 80)}, {1.0, rgb(30, 30, 36)}}, 1.0, true);
  stroke(cr, kInk, 1.0);
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 2; ++j)
    {
      const double x = 24 + j * 16.0, y = 20 + i * 12.0;
      cairo_arc(cr, x, y, 2.6, 0, 2 * kPi);
      setRgba(cr, rgb(90, 90, 100), 1.0);
      cairo_fill(cr);
      cairo_move_to(cr, x - 1.6, y);
      cairo_line_to(cr, x + 1.6, y);
      stroke(cr, kInk, 0.8);
    }
  // A white load stencil, half worn off.
  text(cr, "SWL 1T", 32, 61, 6, rgb(220, 220, 220), 0.35);
}

// Variant 2: a DMX junction box on the plate, two 5-pin sockets, a red
// pilot light and a scrap of spike tape with the circuit number.
void dmxBox(cairo_t* cr)
{
  cairo_rectangle(cr, 12, 16, 40, 30);
  fillLinear(cr, 0, 16, 0, 46, {{0.0, rgb(70, 70, 80)}, {0.3, rgb(40, 40, 48)}, {1.0, rgb(20, 20, 24)}}, 1.0, true);
  stroke(cr, kInk, 1.2);
  for (int k = 0; k < 2; ++k)
  {
    const double cx = 23 + k * 18.0, cy = 30;
    cairo_arc(cr, cx, cy, 6, 0, 2 * kPi);
    setRgba(cr, rgb(16, 16, 20), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, rgb(150, 150, 160), 1.0, 0.9);
    for (int p = 0; p < 5; ++p)
    {
      const double a = kPi * (0.75 + p * 0.375);
      cairo_arc(cr, cx + std::cos(a) * 3.0, cy + std::sin(a) * 3.0, 0.7, 0, 2 * kPi);
      setRgba(cr, rgb(210, 200, 160), 1.0);
      cairo_fill(cr);
    }
  }
  cairo_arc(cr, 46, 20, 1.6, 0, 2 * kPi);
  setRgba(cr, rgb(255, 50, 40), 1.0);
  cairo_fill(cr);
  softDot(cr, 46, 20, 5, 5, rgb(255, 60, 40), 0.6);
  text(cr, "DMX", 32, 43, 6, rgb(230, 230, 230), 0.7);
  cairo_save(cr);
  cairo_translate(cr, 32, 52);
  cairo_rotate(cr, 0.05);
  rect(cr, -10, -3, 20, 6, rgb(240, 200, 40), 0.9);
  text(cr, "40", 0, 2.2, 5, kInk, 0.9);
  cairo_restore(cr);
}

} // namespace

bool isFlyTower(const Theme& t) { return std::string_view(t.look) == "fly_tower"; }

Texture bakeFlyTowerSolid(const Renderer& r, const Theme& t, int variant)
{
  (void)t;
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  Rng rng(40011u + std::uint32_t(variant));
  if (variant == 1)
    beamWeb(cr);
  else
  {
    plate(cr, rng);
    if (variant == 2)
      dmxBox(cr);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// Where you stand: the deck's black top catching the amber from below, a
// strip of glow tape along the front edge, the steel angle nosing under
// it with its bolts, and its shadow on the plate.
Texture bakeFlyTowerSolidTop(const Renderer& r, const Theme& t, float topOffset, int height)
{
  (void)t;
  VectorImage img(64, height);
  cairo_t* cr = img.cr();
  const double y = topOffset;
  {
    cairo_pattern_t* g = cairo_pattern_create_linear(0, y + 12, 0, y + 28);
    stop(g, 0.0, kInk, 0.6);
    stop(g, 1.0, kInk, 0.0);
    cairo_rectangle(cr, 0, y + 12, 64, 16);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  // The deck's top.
  cairo_rectangle(cr, 0, y - 4, 64, 8);
  fillLinear(cr, 0, y - 4, 0, y + 4, {{0.0, rgb(74, 66, 66)}, {0.35, rgb(34, 32, 36)}, {1.0, rgb(20, 20, 24)}});
  cairo_rectangle(cr, 0, y - 4, 64, 1.4);
  setRgba(cr, rgb(150, 120, 100), 0.8);
  cairo_fill(cr);
  softDot(cr, 26, y - 1, 30, 1.6, kAmber, 0.25);
  Rng rng(40021u);
  for (int i = 0; i < 18; ++i)
    rect(cr, rng.range(0, 63), rng.range(float(y) - 2.5f, float(y) + 3.0f), rng.range(1, 3), 1.0, i % 3 ? rgb(90, 84, 84) : kInk,
      rng.range(0.2f, 0.5f));
  // Glow tape along the edge, in dashes.
  for (double x = 3; x < 64; x += 32)
  {
    rect(cr, x, y + 3.2, 22, 2.2, kGlow, 0.85);
    rect(cr, x, y + 3.2, 22, 0.8, rgb(255, 255, 255), 0.5);
  }
  // The angle nosing.
  cairo_rectangle(cr, 0, y + 5.5, 64, 6.5);
  fillLinear(cr, 0, y + 5.5, 0, y + 12, {{0.0, kSteelHi}, {0.35, kSteel}, {1.0, kSteelLo}});
  for (double x = 10; x < 64; x += 32)
  {
    cairo_arc(cr, x, y + 9, 1.4, 0, 2 * kPi);
    setRgba(cr, kSteelLo, 1.0);
    cairo_fill(cr);
  }
  rect(cr, 0, y + 12, 64, 1, kInk, 0.9);
  return img.toTexture(r, 0.0f, topOffset);
}

// A catwalk: the bar grating's top seen just from above, a black kick
// plate with a thin yellow edge, the steel channel stringer bolted under
// it, the shadow below. 64 x 40, the walking surface at the top; it tiles.
Texture bakeFlyTowerPlatform(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(64, 40);
  cairo_t* cr = img.cr();
  {
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 20, 0, 36);
    stop(g, 0.0, kInk, 0.5);
    stop(g, 1.0, kInk, 0.0);
    cairo_rectangle(cr, 0, 20, 64, 16);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  // The grating: bars across, seen from a little above.
  rect(cr, 0, 0, 64, 5, rgb(22, 22, 26));
  for (double x = 0.5; x < 64; x += 4)
    rect(cr, x, 0.5, 1.6, 4.2, rgb(96, 96, 106), 0.9);
  rect(cr, 0, 2.4, 64, 0.8, rgb(30, 30, 34), 0.9);
  rect(cr, 0, 0, 64, 0.9, rgb(200, 190, 180), 0.55);
  // The kick plate.
  cairo_rectangle(cr, 0, 5, 64, 10);
  fillLinear(cr, 0, 5, 0, 15, {{0.0, rgb(58, 56, 62)}, {0.3, rgb(34, 34, 40)}, {1.0, rgb(18, 18, 22)}});
  rect(cr, 0, 5, 64, 1.4, rgb(230, 190, 40), 0.85);
  softDot(cr, 40, 9, 24, 3, kAmber, 0.12);
  // The stringer.
  cairo_rectangle(cr, 0, 15, 64, 7);
  fillLinear(cr, 0, 15, 0, 22, {{0.0, kSteelHi}, {0.3, kSteel}, {1.0, kSteelLo}});
  for (double x = 8; x < 64; x += 16)
  {
    cairo_arc(cr, x, 18.5, 1.3, 0, 2 * kPi);
    setRgba(cr, kSteelLo, 1.0);
    cairo_fill(cr);
  }
  rect(cr, 0, 22, 64, 1, kInk, 0.9);
  return img.toTexture(r, 0.0f, 0.0f);
}

// One block of fixed ladder: black steel rails with spots of glow tape on
// them, round steel rungs. The rails straddle the left cell column, so the
// texture is drawn 16 px left of its block.
Texture bakeFlyTowerLadder(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  for (const double y : {12.0, 44.0})
  {
    cairo_rectangle(cr, 14, y - 2.5, 36, 5);
    fillLinear(cr, 0, y - 2.5, 0, y + 2.5, {{0.0, kSteelHi}, {0.4, kSteel}, {1.0, kSteelLo}}, 1.0, true);
    stroke(cr, kInk, 1.0);
  }
  for (const double x : {9.0, 49.0})
  {
    cairo_rectangle(cr, x, -2, 7, 68);
    fillLinear(cr, x, 0, x + 7, 0, {{0.0, rgb(70, 70, 80)}, {0.4, rgb(40, 40, 46)}, {1.0, rgb(16, 16, 20)}}, 1.0, true);
    stroke(cr, kInk, 1.2);
    rect(cr, x + 1.5, 28, 4, 7, kGlow, 0.75);
  }
  return img.toTexture(r, 16.0f, 0.0f);
}

// Stage-brace spikes in a tray: steel points turned up, the tray black and
// edged in yellow.
Texture bakeFlyTowerSpikes(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  for (int i = 0; i < 6; ++i)
  {
    const double x = 6 + i * 10.4, top = 18 + (i % 2) * 8;
    cairo_move_to(cr, x - 4.5, 52);
    cairo_line_to(cr, x, top);
    cairo_line_to(cr, x + 4.5, 52);
    cairo_close_path(cr);
    fillLinear(cr, x - 4.5, 0, x + 4.5, 0, {{0.0, kSteelHi}, {0.45, kSteel}, {1.0, kSteelLo}}, 1.0, true);
    stroke(cr, kInk, 1.0);
    softDot(cr, x, top + 2, 3, 3, kAmber, 0.5);
  }
  rect(cr, 0, 50, 64, 14, rgb(20, 20, 24));
  rect(cr, 0, 50, 64, 2.5, rgb(230, 190, 40));
  rect(cr, 0, 62, 64, 2, kInk);
  return img.toTexture(r, 0.0f, 0.0f);
}

// The stage house's height in haze: near-black at the top warming to a
// dusky plum, coloured beams (amber, magenta, cyan) slanting up out of the
// show below, slow drifts of haze, dust in the light.
Texture bakeFlyTowerSky(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(kScreenW, kScreenH);
  cairo_t* cr = img.cr();
  cairo_rectangle(cr, 0, 0, kScreenW, kScreenH);
  fillLinear(cr, 0, 0, 0, kScreenH, {{0.0, rgb(8, 7, 12)}, {0.5, rgb(22, 16, 28)}, {1.0, rgb(48, 28, 40)}});
  struct Beam
  {
    double x, angle, spread;
    Color c;
    double a;
  };
  static constexpr Beam kBeams[] = {{180, kPi - 0.35, 220, kAmber, 0.10}, {520, kPi + 0.2, 180, kMagenta, 0.08},
    {760, kPi - 0.15, 200, kCyan, 0.07}, {1100, kPi + 0.4, 240, kAmber, 0.09}};
  for (const auto& b : kBeams)
    beam(cr, b.x, kScreenH + 40, b.angle, 900, b.spread, b.c, b.a);
  Rng rng(40041u);
  for (int i = 0; i < 22; ++i)
    softDot(cr, rng.range(0, float(kScreenW)), rng.range(120, 720), rng.range(140, 320), rng.range(40, 90), rgb(90, 70, 100), 0.06);
  for (int i = 0; i < 90; ++i)
  {
    cairo_arc(cr, rng.range(0, float(kScreenW)), rng.range(0, float(kScreenH)), rng.range(0.5f, 1.4f), 0, 2 * kPi);
    setRgba(cr, rgb(255, 230, 200), rng.range(0.05f, 0.25f));
    cairo_fill(cr);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

namespace
{

constexpr int kFarH = kScreenH + 360;
constexpr int kNearH = kScreenH + 420;

// Black-painted brick: courses of bricks, the mortar a shade lighter, soot
// streaks running down.
void brickWall(cairo_t* cr, double x0, double y0, double w, double h, Rng& rng)
{
  rect(cr, x0, y0, w, h, darken(kBrick, 0.3f));
  for (double y = y0; y < y0 + h; y += 22)
  {
    const double off = std::fmod((y - y0) / 22.0, 2.0) < 1.0 ? 0.0 : 26.0;
    for (double x = x0 - off; x < x0 + w; x += 52)
    {
      const double v = rng.range(-0.08f, 0.1f);
      rect(cr, x + 1.5, y + 1.5, 49, 19, lerpColor(kBrick, v < 0 ? rgb(0, 0, 0) : rgb(110, 80, 70), float(std::abs(v))));
      rect(cr, x + 1.5, y + 1.5, 49, 2, rgb(90, 70, 64), 0.25);
    }
  }
  for (int i = 0; i < int(w / 90); ++i)
  {
    const double sx = x0 + rng.range(0, float(w)), sy = y0 + rng.range(0, float(h) * 0.6f);
    cairo_rectangle(cr, sx, sy, rng.range(14, 40), rng.range(120, 360));
    setRgba(cr, kInk, rng.range(0.15f, 0.35f));
    cairo_fill(cr);
  }
}

// A steel column (an I-beam on edge) up the wall.
void column(cairo_t* cr, double x, double y0, double y1)
{
  cairo_rectangle(cr, x - 22, y0, 44, y1 - y0);
  fillLinear(cr, x - 22, 0, x + 22, 0, {{0.0, rgb(26, 26, 30)}, {0.2, rgb(58, 58, 66)}, {0.5, rgb(36, 36, 42)}, {1.0, rgb(16, 16, 20)}});
  for (double y = y0 + 40; y < y1; y += 160)
  {
    rect(cr, x - 26, y, 52, 18, rgb(48, 48, 56));
    for (const double bx : {-16.0, 0.0, 16.0})
    {
      cairo_arc(cr, x + bx, y + 9, 2.4, 0, 2 * kPi);
      setRgba(cr, rgb(90, 90, 100), 1.0);
      cairo_fill(cr);
    }
  }
}

// A counterweight arbor in its T-track: the track up the wall, the arbor's
// frame stacked with steel bricks at `ay`, the purchase line running up.
void arbor(cairo_t* cr, double x, double y0, double y1, double ay)
{
  rect(cr, x - 3, y0, 6, y1 - y0, rgb(40, 40, 46));
  rect(cr, x + 22, y0, 6, y1 - y0, rgb(40, 40, 46));
  cairo_move_to(cr, x + 12, y0);
  cairo_line_to(cr, x + 12, y1);
  stroke(cr, rgb(110, 96, 76), 1.6, 0.6);
  cairo_rectangle(cr, x - 2, ay, 28, 120);
  setRgba(cr, rgb(30, 30, 36), 1.0);
  cairo_fill(cr);
  for (double y = ay + 10; y < ay + 110; y += 9)
    rect(cr, x + 1, y, 22, 7, rgb(60, 58, 64));
  rect(cr, x - 2, ay, 28, 4, rgb(90, 90, 100));
  rect(cr, x - 2, ay + 116, 28, 4, rgb(90, 90, 100));
}

// A caged worklight: a blue-white bulb in a wire cage on the wall.
void worklight(cairo_t* cr, double x, double y)
{
  softDot(cr, x, y, 60, 50, rgb(120, 150, 220), 0.18);
  cairo_arc(cr, x, y, 7, 0, 2 * kPi);
  setRgba(cr, rgb(220, 230, 255), 0.9);
  cairo_fill(cr);
  for (int k = -1; k <= 1; ++k)
  {
    cairo_move_to(cr, x + k * 5, y - 10);
    cairo_line_to(cr, x + k * 5, y + 10);
  }
  stroke(cr, rgb(40, 40, 46), 1.4);
  rect(cr, x - 9, y - 14, 18, 5, rgb(40, 40, 46));
}

} // namespace

// The stage house's back wall (layerW x kFarH, tiles side by side): black
// sooty brick, steel columns with bolted plates, counterweight arbors in
// their tracks with the purchase lines running up past them, caged
// worklights, all sitting back in the haze.
Texture bakeFlyTowerFar(const Renderer& r, const Theme& t, int layerW)
{
  (void)t;
  const int h = kFarH;
  VectorImage img(layerW, h);
  cairo_t* cr = img.cr();
  const double W = layerW;
  Rng rng(40051u);
  brickWall(cr, 0, 0, W, h, rng);
  for (double x = 160; x < W; x += 640)
    column(cr, x, 0, h);
  for (double x = 300; x < W; x += 640)
    for (int k = 0; k < 5; ++k)
      arbor(cr, x + k * 46, 0, h, 120 + std::fmod(k * 263.0 + x, 600.0));
  for (double x = 520; x < W; x += 640)
  {
    worklight(cr, x, 220);
    worklight(cr, x + 40, 760);
  }
  // A cross girder.
  for (const double y : {80.0, 640.0})
  {
    cairo_rectangle(cr, 0, y, W, 28);
    fillLinear(cr, 0, y, 0, y + 28, {{0.0, rgb(60, 60, 68)}, {0.3, rgb(34, 34, 40)}, {1.0, rgb(14, 14, 18)}});
  }
  // The haze between.
  cairo_rectangle(cr, 0, 0, W, h);
  fillLinear(cr, 0, 0, 0, h, {{0.0, rgb(16, 12, 22)}, {1.0, rgb(50, 30, 44)}}, 0.45);
  return img.toTexture(r, 0.0f, 0.0f);
}

namespace
{

// A batten across the width at y: a pipe with black velour border hanging
// from it in folds, and every so often a PAR can or a moving head under it.
void batten(cairo_t* cr, double W, double y, double drop, Rng& rng)
{
  // The border: vertical folds, a hem.
  for (double x = 0; x < W; x += 18)
  {
    cairo_rectangle(cr, x, y, 18, drop);
    fillLinear(cr, x, 0, x + 18, 0, {{0.0, rgb(10, 8, 12)}, {0.5, rgb(30, 24, 32)}, {1.0, rgb(8, 6, 10)}});
  }
  cairo_move_to(cr, 0, y + drop);
  for (double x = 0; x <= W; x += 18)
    cairo_line_to(cr, x, y + drop + (std::fmod(x, 36.0) < 18 ? 4 : 0));
  cairo_line_to(cr, W, y + drop - 8);
  cairo_line_to(cr, 0, y + drop - 8);
  cairo_close_path(cr);
  setRgba(cr, rgb(20, 16, 22), 1.0);
  cairo_fill(cr);
  // The pipe.
  cairo_rectangle(cr, 0, y - 6, W, 8);
  fillLinear(cr, 0, y - 6, 0, y + 2, {{0.0, rgb(110, 110, 120)}, {0.5, rgb(50, 50, 58)}, {1.0, rgb(20, 20, 24)}});
  // Lamps hung under the next batten up (on the pipe's front).
  for (double x = rng.range(40, 120); x < W - 40; x += rng.range(110, 220))
  {
    const bool head = rng.uniform() < 0.3f;
    cairo_save(cr);
    cairo_translate(cr, x, y - 4);
    if (head)
    {
      rect(cr, -14, -24, 28, 10, rgb(30, 30, 36));
      cairo_arc(cr, 0, -36, 14, 0, 2 * kPi);
      setRgba(cr, rgb(34, 34, 40), 1.0);
      cairo_fill(cr);
      cairo_arc(cr, 0, -36, 7, 0, 2 * kPi);
      setRgba(cr, rgb(255, 230, 200), 0.35);
      cairo_fill(cr);
    }
    else
    {
      rect(cr, -3, -10, 6, 10, rgb(30, 30, 36));
      cairo_rotate(cr, rng.range(-0.4f, 0.4f));
      cairo_rectangle(cr, -11, -40, 22, 32);
      fillLinear(cr, -11, 0, 11, 0, {{0.0, rgb(30, 30, 36)}, {0.4, rgb(64, 64, 74)}, {1.0, rgb(20, 20, 24)}});
      rect(cr, -10, -42, 20, 3, rgb(240, 180, 120), 0.4);
    }
    cairo_restore(cr);
  }
}

} // namespace

// The near layer (layerW x kNearH, tiles side by side; see-through):
// battens hanging across with black velour borders in folds, PAR cans and
// moving heads on the pipes, the lift lines running up from them out of
// sight; darkened by the haze.
Texture bakeFlyTowerNear(const Renderer& r, const Theme& t, int layerW)
{
  (void)t;
  VectorImage img(layerW, kNearH);
  cairo_t* cr = img.cr();
  Rng rng(40061u);
  const double W = layerW;
  for (const double y : {110.0, 560.0})
  {
    for (double x = 200; x < W; x += 520)
    {
      cairo_move_to(cr, x, 0);
      cairo_line_to(cr, x, y);
      stroke(cr, rgb(70, 70, 80), 1.4, 0.7);
    }
    batten(cr, W, y, 120, rng);
  }
  cairo_set_operator(cr, CAIRO_OPERATOR_ATOP);
  cairo_rectangle(cr, 0, 0, W, kNearH);
  setRgba(cr, rgb(30, 20, 34), 0.35);
  cairo_fill(cr);
  cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
  return img.toTexture(r, 0.0f, 0.0f);
}

// The hatch to the lighting bridge (96 x 144; (0, 0) the top-left of the
// 64 x 128 exit area): a steel door frame, through it a ladder climbing up
// into the warm light of the bridge, LIGHTING BRIDGE stencilled over the
// frame and a green running-man EXIT sign lit above.
Texture bakeFlyTowerExitBase(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(96, 144);
  cairo_t* cr = img.cr();
  cairo_translate(cr, 16, 8);
  // The opening and the light coming down it.
  cairo_rectangle(cr, 4, 18, 56, 110);
  fillLinear(cr, 0, 18, 0, 128, {{0.0, rgb(255, 236, 190)}, {0.35, rgb(200, 150, 90)}, {1.0, rgb(40, 30, 30)}});
  softDot(cr, 32, 26, 34, 26, rgb(255, 250, 230), 0.8);
  // The ladder up through it.
  for (const double x : {16.0, 46.0})
  {
    cairo_rectangle(cr, x - 2.5, 14, 5, 114);
    fillLinear(cr, x - 2.5, 0, x + 2.5, 0, {{0.0, rgb(90, 90, 100)}, {1.0, rgb(24, 24, 28)}});
  }
  for (double y = 26; y < 126; y += 14)
  {
    cairo_rectangle(cr, 16, y, 30, 3);
    setRgba(cr, rgb(70, 70, 80), 1.0);
    cairo_fill(cr);
    rect(cr, 16, y, 30, 1, rgb(220, 200, 170), 0.6);
  }
  // The frame.
  cairo_rectangle(cr, -2, 12, 68, 116);
  cairo_rectangle(cr, 4, 18, 56, 110);
  cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
  fillLinear(cr, -2, 0, 66, 0, {{0.0, rgb(60, 60, 68)}, {0.5, rgb(34, 34, 40)}, {1.0, rgb(18, 18, 22)}});
  cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
  for (double y = 22; y < 128; y += 24)
  {
    rect(cr, -1, y, 4, 10, rgb(230, 190, 40), 0.9);
    rect(cr, 61, y + 12, 4, 10, rgb(230, 190, 40), 0.9);
  }
  // The stencil and the EXIT sign.
  rect(cr, 0, 2, 64, 10, rgb(20, 20, 24));
  text(cr, "LIGHTING BRIDGE", 32, 9.6, 6.2, rgb(240, 240, 230), 0.9);
  cairo_rectangle(cr, 18, -8, 28, 10);
  setRgba(cr, rgb(20, 120, 60), 1.0);
  cairo_fill(cr);
  softDot(cr, 32, -3, 26, 12, rgb(80, 255, 140), 0.45);
  text(cr, "EXIT", 32, -0.6, 7.5, rgb(220, 255, 230));
  return img.toTexture(r, 16.0f, 8.0f);
}

} // namespace gr
