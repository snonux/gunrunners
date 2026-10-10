// Level 46, Crystal Drift (Episode 7, DEEP SPACE): the Swap Crystals, the
// Blinker and its shimmer, the Shard Golem and the Prism Bat, in the style
// of enemy_art.cpp: each drawn in screen pixels into a texture with a 32 px
// margin, the cell box at (32, 32) .. (32 + w, 32 + h), facing right.

#include "assets/enemy_art_alien.hpp"

#include "base/math.hpp"

#include <algorithm>
#include <cmath>
#include <map>

namespace gr
{

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr Color kInk = rgb(30, 14, 58);
constexpr double kLine = 2.4;
constexpr double kM = 32.0; // margin

Color lighten(Color c, float t) { return lerpColor(c, rgb(255, 255, 255), t); }
Color darken(Color c, float t) { return lerpColor(c, rgb(0, 0, 0), t); }

void circle(cairo_t* cr, double x, double y, double r) { cairo_arc(cr, x, y, r, 0, 2 * kPi); }

void ellipse(cairo_t* cr, double x, double y, double rx, double ry)
{
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_scale(cr, rx, ry);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_restore(cr);
}

void setRgba(cairo_t* cr, Color c, double a)
{
  cairo_set_source_rgba(cr, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, a);
}

struct Box
{
  cairo_t* cr;
  const Theme& t;
  double w, h;
  int variant, frame;
};

// A faceted gem shape: a long octahedron, point up and point down, centred
// on (cx, cy). Lit faces on the left.
void gem(cairo_t* cr, double cx, double cy, double hw, double hh, Color c, Color outline, double alpha = 1.0)
{
  const double mid = cy - hh * 0.15;
  // Left (lit) half and right (shaded) half, then the outline.
  cairo_move_to(cr, cx, cy - hh);
  cairo_line_to(cr, cx - hw, mid);
  cairo_line_to(cr, cx, cy + hh);
  cairo_close_path(cr);
  setRgba(cr, lighten(c, 0.35f), alpha);
  cairo_fill(cr);
  cairo_move_to(cr, cx, cy - hh);
  cairo_line_to(cr, cx + hw, mid);
  cairo_line_to(cr, cx, cy + hh);
  cairo_close_path(cr);
  setRgba(cr, darken(c, 0.15f), alpha);
  cairo_fill(cr);
  // An inner facet.
  cairo_move_to(cr, cx, cy - hh);
  cairo_line_to(cr, cx - hw * 0.35, mid);
  cairo_line_to(cr, cx, cy + hh);
  cairo_line_to(cr, cx + hw * 0.4, mid);
  cairo_close_path(cr);
  setRgba(cr, c, alpha * 0.8);
  cairo_fill(cr);
  cairo_move_to(cr, cx, cy - hh);
  cairo_line_to(cr, cx - hw, mid);
  cairo_line_to(cr, cx, cy + hh);
  cairo_line_to(cr, cx + hw, mid);
  cairo_close_path(cr);
  setRgba(cr, outline, alpha);
  cairo_set_line_width(cr, kLine);
  cairo_stroke(cr);
  // The ridge catching the light.
  cairo_move_to(cr, cx, cy - hh + 3);
  cairo_line_to(cr, cx - hw * 0.35, mid);
  cairo_set_line_width(cr, 2.0);
  setRgba(cr, rgb(255, 255, 255), 0.7 * alpha);
  cairo_stroke(cr);
}

// Swap Crystal: a tall violet gem, hovering, with a ring of light round its
// waist. Variant 1 is the cracked green one (the Virus's), 2 ringing (just
// swapped). Frame 1: the ring turned.
void swapCrystal(const Box& b)
{
  cairo_t* cr = b.cr;
  const double cx = kM + b.w * 0.5, cy = kM + b.h * 0.45;
  const Color c = b.variant == 1 ? rgb(130, 230, 70) : rgb(176, 120, 255);
  const Color glow = b.variant == 1 ? rgb(170, 255, 60) : (b.variant == 2 ? rgb(255, 255, 255) : rgb(120, 240, 255));
  radialGlow(cr, cx, cy, b.w * 0.9, glow, b.variant == 2 ? 0.8 : 0.4);
  gem(cr, cx, cy, b.w * 0.36, b.h * 0.42, c, kInk);
  if (b.variant == 1)
  {
    // Cracks, oozing green.
    cairo_move_to(cr, cx - 4, cy - b.h * 0.3);
    cairo_line_to(cr, cx + 3, cy - b.h * 0.1);
    cairo_line_to(cr, cx - 5, cy + b.h * 0.05);
    cairo_line_to(cr, cx + 4, cy + b.h * 0.25);
    cairo_set_line_width(cr, 2.2);
    setRgba(cr, rgb(30, 70, 20), 0.9);
    cairo_stroke(cr);
    circle(cr, cx + 4, cy + b.h * 0.33, 3);
    setRgba(cr, rgb(170, 255, 60), 0.9);
    cairo_fill(cr);
  }
  // The ring: a thin ellipse round its waist, half behind it.
  const double ry = b.frame ? 5.0 : 3.0;
  cairo_save(cr);
  cairo_translate(cr, cx, cy + b.h * 0.05);
  cairo_scale(cr, b.w * 0.55, ry);
  cairo_arc(cr, 0, 0, 1, 0, kPi);
  cairo_restore(cr);
  cairo_set_line_width(cr, 2.4);
  setRgba(cr, glow, 0.9);
  cairo_stroke(cr);
  // Sparkles.
  for (int i = 0; i < 3; ++i)
  {
    const double sx = cx + (i - 1) * b.w * 0.45 + (b.frame ? 3 : -3), sy = cy - b.h * 0.35 + i * 10;
    cairo_move_to(cr, sx - 4, sy);
    cairo_line_to(cr, sx + 4, sy);
    cairo_move_to(cr, sx, sy - 4);
    cairo_line_to(cr, sx, sy + 4);
    cairo_set_line_width(cr, 1.4);
    setRgba(cr, rgb(255, 255, 255), 0.85);
    cairo_stroke(cr);
  }
}

// Blinker: a lean, long-legged hunter of glassy teal crystal, its head a
// single shard with a slit eye. Variant 1: fading out (it is about to
// blink), 2: there, crouched to lunge, claws forward.
void blinker(const Box& b)
{
  cairo_t* cr = b.cr;
  const double a = b.variant == 1 ? 0.45 : 1.0;
  const double bx = kM, by = kM, w = b.w, h = b.h;
  const Color body = rgb(70, 200, 210), dark = rgb(30, 90, 120);
  const bool lunge = b.variant == 2;
  const double hipY = by + h * (lunge ? 0.58 : 0.5);
  // Legs: two pairs, thin and jointed.
  for (int k = 0; k < 2; ++k)
  {
    const double fx = bx + w * (0.2 + 0.55 * k) + (b.frame ? (k ? -2 : 2) : 0);
    cairo_move_to(cr, bx + w * (0.35 + 0.25 * k), hipY);
    cairo_line_to(cr, fx + (k ? 4 : -4), by + h * 0.78);
    cairo_line_to(cr, fx, by + h);
    cairo_set_line_width(cr, 3.0);
    setRgba(cr, dark, a);
    cairo_stroke(cr);
  }
  // Body: a long shard, slanted forward.
  cairo_move_to(cr, bx + w * 0.12, hipY + 2);
  cairo_line_to(cr, bx + w * 0.4, by + h * (lunge ? 0.4 : 0.3));
  cairo_line_to(cr, bx + w * 0.82, hipY - (lunge ? 4 : 8));
  cairo_line_to(cr, bx + w * 0.6, hipY + 6);
  cairo_close_path(cr);
  setRgba(cr, body, a);
  cairo_fill_preserve(cr);
  setRgba(cr, kInk, a);
  cairo_set_line_width(cr, kLine);
  cairo_stroke(cr);
  cairo_move_to(cr, bx + w * 0.22, hipY);
  cairo_line_to(cr, bx + w * 0.42, by + h * (lunge ? 0.43 : 0.33));
  setRgba(cr, rgb(220, 255, 255), 0.7 * a);
  cairo_set_line_width(cr, 2.0);
  cairo_stroke(cr);
  // Head: one shard with a slit eye.
  const double hx = bx + w * (lunge ? 0.92 : 0.8), hy = by + h * (lunge ? 0.36 : 0.18);
  gem(cr, hx, hy, w * 0.13, h * 0.16, rgb(120, 240, 240), kInk, a);
  cairo_move_to(cr, hx - 2, hy - 1);
  cairo_line_to(cr, hx + 5, hy - 2);
  cairo_set_line_width(cr, 2.4);
  setRgba(cr, rgb(255, 60, 140), a);
  cairo_stroke(cr);
  if (lunge)
  {
    // Claws out in front.
    for (int k = 0; k < 2; ++k)
    {
      cairo_move_to(cr, bx + w * 0.75, hipY - 2 + k * 6);
      cairo_line_to(cr, bx + w * 1.1, hipY - 4 + k * 8);
      cairo_set_line_width(cr, 2.4);
      setRgba(cr, rgb(220, 255, 255), a);
      cairo_stroke(cr);
    }
  }
  if (b.variant == 1)
    for (int i = 0; i < 6; ++i)
    {
      const double sx = bx + w * (0.1 + 0.15 * i), sy = by + h * (0.2 + 0.11 * ((i * 3 + b.frame) % 6));
      circle(cr, sx, sy, 2.2);
      setRgba(cr, rgb(255, 255, 255), 0.8);
      cairo_fill(cr);
    }
}

// Where a Blinker is about to appear: a column of glittering light in its
// outline.
void blinkShimmer(const Box& b)
{
  cairo_t* cr = b.cr;
  const double cx = kM + b.w * 0.5, cy = kM + b.h * 0.55;
  radialGlow(cr, cx, cy, b.w * 0.9, rgb(120, 240, 255), 0.6);
  ellipse(cr, cx, kM + b.h - 2, b.w * 0.5, 4);
  setRgba(cr, rgb(200, 255, 255), 0.5);
  cairo_fill(cr);
  Rng rng(std::uint32_t(77 + b.frame * 13));
  for (int i = 0; i < 14; ++i)
  {
    const double x = kM + rng.range(0.0, b.w), y = kM + rng.range(0.0, b.h);
    cairo_move_to(cr, x - 3, y);
    cairo_line_to(cr, x + 3, y);
    cairo_move_to(cr, x, y - 3);
    cairo_line_to(cr, x, y + 3);
    cairo_set_line_width(cr, 1.6);
    setRgba(cr, rgb(255, 255, 255), 0.9);
    cairo_stroke(cr);
  }
}

// Shard Golem: a hulking block of dull violet crystal on stumpy legs, with
// a glowing pink shard sticking out of its back (the only place shots hurt
// it). Variant 1: turning round, its head swung. Frame 1: the other step.
void shardGolem(const Box& b)
{
  cairo_t* cr = b.cr;
  const double bx = kM, by = kM, w = b.w, h = b.h;
  const Color hide = rgb(110, 90, 150), hideDark = rgb(60, 46, 92);
  // Legs.
  for (int k = 0; k < 2; ++k)
  {
    const double lx = bx + w * (0.22 + 0.42 * k) + (b.frame ? (k ? -3 : 3) : 0);
    cairo_rectangle(cr, lx, by + h * 0.74, w * 0.2, h * 0.26);
    fillGradientOutline(cr, by + h * 0.74, by + h, hide, hideDark, kInk, kLine);
  }
  // The body: a slab with chunky facets.
  cairo_move_to(cr, bx + w * 0.08, by + h * 0.78);
  cairo_line_to(cr, bx + w * 0.02, by + h * 0.36);
  cairo_line_to(cr, bx + w * 0.2, by + h * 0.12);
  cairo_line_to(cr, bx + w * 0.8, by + h * 0.1);
  cairo_line_to(cr, bx + w * 0.98, by + h * 0.4);
  cairo_line_to(cr, bx + w * 0.92, by + h * 0.8);
  cairo_close_path(cr);
  fillGradientOutline(cr, by + h * 0.1, by + h * 0.8, lighten(hide, 0.2f), hideDark, kInk, kLine);
  for (const auto& f : {std::pair<double, double>{0.3, 0.3}, {0.55, 0.5}, {0.75, 0.25}})
  {
    cairo_move_to(cr, bx + w * f.first, by + h * f.second);
    cairo_line_to(cr, bx + w * (f.first + 0.12), by + h * (f.second + 0.14));
    cairo_line_to(cr, bx + w * (f.first - 0.04), by + h * (f.second + 0.2));
    cairo_close_path(cr);
    setRgba(cr, lighten(hide, 0.35f), 0.6);
    cairo_fill(cr);
  }
  // The shard on its back (the left, facing right), glowing.
  const double sx = bx + w * 0.12, sy = by + h * 0.3;
  radialGlow(cr, sx, sy, w * 0.45, rgb(255, 120, 220), 0.7);
  gem(cr, sx - 2, sy - 4, w * 0.11, h * 0.2, rgb(255, 120, 220), kInk);
  // Head: a lump in front with two small hot eyes; swung round when turning.
  const double hx = bx + w * (b.variant == 1 ? 0.62 : 0.84), hy = by + h * 0.2;
  cairo_rectangle(cr, hx - w * 0.14, hy - h * 0.08, w * 0.28, h * 0.18);
  fillGradientOutline(cr, hy - h * 0.08, hy + h * 0.1, lighten(hide, 0.3f), hide, kInk, kLine);
  for (int k = 0; k < 2; ++k)
  {
    circle(cr, hx + w * (0.02 + 0.07 * k), hy, 2.6);
    setRgba(cr, rgb(255, 230, 90), 1.0);
    cairo_fill(cr);
  }
}

// Prism Bat: a small bat whose wings are thin panes of prism glass, a
// rainbow on each. Frame 1: wings down.
void prismBat(const Box& b)
{
  cairo_t* cr = b.cr;
  const double cx = kM + b.w * 0.5, cy = kM + b.h * 0.5;
  const double flap = b.frame ? 10.0 : -10.0;
  static const Color kBands[] = {rgb(255, 90, 90), rgb(255, 210, 70), rgb(110, 240, 120), rgb(90, 200, 255),
    rgb(190, 120, 255)};
  for (int s : {-1, 1})
  {
    cairo_move_to(cr, cx + s * 4, cy - 2);
    cairo_line_to(cr, cx + s * b.w * 0.6, cy + flap - 6);
    cairo_line_to(cr, cx + s * b.w * 0.45, cy + flap + 6);
    cairo_line_to(cr, cx + s * 4, cy + 5);
    cairo_close_path(cr);
    setRgba(cr, rgb(230, 230, 255), 0.55);
    cairo_fill_preserve(cr);
    setRgba(cr, kInk, 1.0);
    cairo_set_line_width(cr, 2.0);
    cairo_stroke(cr);
    for (int k = 0; k < 5; ++k)
    {
      const double t = 0.3 + k * 0.12;
      cairo_move_to(cr, cx + s * 6, cy + 1);
      cairo_line_to(cr, cx + s * b.w * 0.55 * t * 1.6, cy + (flap - 2) * t * 1.4 + k * 1.5);
      cairo_set_line_width(cr, 1.6);
      setRgba(cr, kBands[k], 0.75);
      cairo_stroke(cr);
    }
  }
  // Body: a small dark gem with ears and two eyes.
  gem(cr, cx, cy + 1, 7, 10, rgb(80, 60, 130), kInk);
  for (int s : {-1, 1})
  {
    cairo_move_to(cr, cx + s * 2, cy - 7);
    cairo_line_to(cr, cx + s * 5, cy - 14);
    cairo_line_to(cr, cx + s * 6, cy - 5);
    setRgba(cr, rgb(80, 60, 130), 1.0);
    cairo_fill(cr);
    circle(cr, cx + s * 2.5, cy - 2, 1.8);
    setRgba(cr, rgb(255, 240, 120), 1.0);
    cairo_fill(cr);
  }
}

using DrawFn = void (*)(const Box&);

} // namespace

bool drawCrystalArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame)
{
  static const std::map<std::string, DrawFn> kRoutines{
    {"swap_crystal", swapCrystal},
    {"blinker", blinker},
    {"blink_shimmer", blinkShimmer},
    {"shard_golem", shardGolem},
    {"prism_bat", prismBat},
  };
  const auto it = kRoutines.find(key);
  if (it == kRoutines.end())
    return false;
  it->second(Box{cr, t, w, h, variant, frame});
  return true;
}

} // namespace gr
