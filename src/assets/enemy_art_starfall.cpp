// Level 43, Starfall (Episode 7, DEEP SPACE): its aliens, the drifting
// asteroids, the derelict probe, the landing pad, Vurr's cloud banks and
// the duck's space helmet, in the style of enemy_art.cpp: each drawn in
// screen pixels into a texture with a 32 px margin, the cell box at
// (32, 32) .. (32 + w, 32 + h), facing right.

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
constexpr Color kInk = rgb(14, 8, 26);
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

void radialFill(cairo_t* cr, double x, double y, double r, Color inner, Color outer)
{
  cairo_pattern_t* p = cairo_pattern_create_radial(x - r * 0.3, y - r * 0.35, r * 0.1, x, y, r);
  cairo_pattern_add_color_stop_rgb(p, 0, redOf(inner) / 255.0, greenOf(inner) / 255.0, blueOf(inner) / 255.0);
  cairo_pattern_add_color_stop_rgb(p, 1, redOf(outer) / 255.0, greenOf(outer) / 255.0, blueOf(outer) / 255.0);
  cairo_set_source(cr, p);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(p);
}

struct Box
{
  cairo_t* cr;
  const Theme& t;
  double w, h;
  int variant, frame;
};

// Void Ray: a glowing manta, wide wings and a whip of a tail. Variant 0
// glides (frame 1 the wingtips down), 1 its fins light up (the tell), 2
// dives, wings swept back.
void voidRay(const Box& b)
{
  cairo_t* cr = b.cr;
  const double cx = kM + b.w * 0.55, cy = kM + b.h * 0.55;
  const double span = b.w * 0.5, flap = b.variant == 2 ? 0.0 : (b.frame ? 6.0 : -6.0);
  const double sweep = b.variant == 2 ? b.w * 0.18 : 0.0;
  const Color body = rgb(70, 40, 140), glow = b.variant == 1 ? rgb(255, 90, 220) : rgb(110, 230, 255);
  if (b.variant >= 1)
    radialGlow(cr, cx, cy, b.w * 0.6, glow, b.variant == 1 ? 0.55 : 0.3);
  // The tail, trailing behind with a barb.
  cairo_move_to(cr, cx - span * 0.5, cy + 2);
  cairo_curve_to(cr, cx - span * 0.9, cy + 6, cx - span * 1.1, cy - 4, cx - span * 1.25, cy + 2);
  cairo_set_line_width(cr, 2.6);
  setColor(cr, darken(body, 0.2f));
  cairo_stroke(cr);
  circle(cr, cx - span * 1.25, cy + 2, 2.6);
  setColor(cr, glow);
  cairo_fill(cr);
  // Wings: a wide diamond, the near wingtip up or down with the flap.
  cairo_move_to(cr, cx + span * 0.7, cy);
  cairo_curve_to(cr, cx + span * 0.3, cy - b.h * 0.25, cx - sweep, cy - b.h * 0.45 + flap, cx - span * 0.2 - sweep,
    cy - b.h * 0.6 + flap);
  cairo_curve_to(cr, cx - span * 0.1, cy - b.h * 0.1, cx - span * 0.55, cy, cx - span * 0.6, cy + 2);
  cairo_curve_to(cr, cx - span * 0.1, cy + b.h * 0.15, cx - sweep, cy + b.h * 0.45 - flap, cx - span * 0.1 - sweep,
    cy + b.h * 0.55 - flap);
  cairo_curve_to(cr, cx + span * 0.2, cy + b.h * 0.25, cx + span * 0.5, cy + b.h * 0.15, cx + span * 0.7, cy);
  cairo_close_path(cr);
  radialFill(cr, cx + span * 0.1, cy, span, lighten(body, 0.35f), darken(body, 0.35f));
  setColor(cr, kInk);
  cairo_set_line_width(cr, kLine);
  cairo_stroke(cr);
  // The glowing fin edges.
  cairo_move_to(cr, cx + span * 0.55, cy - 3);
  cairo_curve_to(cr, cx + span * 0.2, cy - b.h * 0.25, cx - sweep, cy - b.h * 0.4 + flap, cx - span * 0.18 - sweep,
    cy - b.h * 0.52 + flap);
  cairo_move_to(cr, cx + span * 0.55, cy + 3);
  cairo_curve_to(cr, cx + span * 0.2, cy + b.h * 0.2, cx - sweep, cy + b.h * 0.38 - flap, cx - span * 0.08 - sweep,
    cy + b.h * 0.47 - flap);
  cairo_set_line_width(cr, b.variant == 1 ? 4.0 : 2.4);
  setRgba(cr, glow, b.variant == 1 ? 1.0 : 0.8);
  cairo_stroke(cr);
  // Spots down its back and two small eyes at the front.
  for (int i = 0; i < 3; ++i)
  {
    circle(cr, cx + span * (0.25 - 0.2 * i), cy - 1, 2.2);
    setRgba(cr, glow, 0.85);
    cairo_fill(cr);
  }
  for (int s : {-1, 1})
  {
    circle(cr, cx + span * 0.58, cy + s * 4, 2.6);
    setColor(cr, rgb(255, 250, 200));
    cairo_fill(cr);
  }
  if (b.variant == 2)
    for (int i = 0; i < 3; ++i)
    {
      cairo_move_to(cr, cx - span * 0.7, cy - 8 + i * 8);
      cairo_line_to(cr, cx - span * 1.5, cy - 8 + i * 8);
      cairo_set_line_width(cr, 2);
      setRgba(cr, glow, 0.5);
      cairo_stroke(cr);
    }
}

// Rock Leech: a fat ringed slug with a round sucker mouth, clamped to its
// rock. Variant 1: swollen, its throat glowing (the tell).
void rockLeech(const Box& b)
{
  cairo_t* cr = b.cr;
  const double sw = b.variant == 1 ? 1.15 : 1.0;
  const double cx = kM + b.w * 0.45, cy = kM + b.h * 0.6, rx = b.w * 0.45 * sw, ry = b.h * 0.36 * sw;
  ellipse(cr, cx, cy, rx, ry);
  radialFill(cr, cx, cy, rx, rgb(170, 220, 120), rgb(70, 110, 50));
  setColor(cr, kInk);
  cairo_set_line_width(cr, kLine);
  cairo_stroke(cr);
  for (int i = -1; i <= 1; ++i)
  {
    cairo_arc(cr, cx + i * rx * 0.45, cy, ry * 0.95, -kPi * 0.4, kPi * 0.4);
    setRgba(cr, rgb(40, 70, 30), 0.6);
    cairo_set_line_width(cr, 1.6);
    cairo_stroke(cr);
  }
  // The sucker mouth at the front.
  const double mx = cx + rx * 0.9, my = cy - ry * 0.1;
  if (b.variant == 1)
    radialGlow(cr, mx, my, 14, rgb(170, 255, 60), 0.8);
  circle(cr, mx, my, ry * 0.55);
  fillOutline(cr, rgb(120, 40, 70), kInk, 1.6);
  circle(cr, mx, my, ry * 0.28);
  setColor(cr, b.variant == 1 ? rgb(220, 255, 120) : rgb(40, 10, 24));
  cairo_fill(cr);
  // Little eyes on stalks.
  for (int s : {0, 1})
  {
    const double ex = cx + rx * (0.35 + s * 0.15), ey = cy - ry * 1.15 - s * 2;
    cairo_move_to(cr, ex - 2, cy - ry * 0.7);
    cairo_line_to(cr, ex, ey);
    cairo_set_line_width(cr, 1.8);
    setColor(cr, rgb(90, 130, 60));
    cairo_stroke(cr);
    circle(cr, ex, ey, 2.4);
    setColor(cr, rgb(255, 240, 120));
    cairo_fill(cr);
  }
  ellipse(cr, cx - rx * 0.3, cy - ry * 0.45, rx * 0.3, ry * 0.15);
  setRgba(cr, rgb(255, 255, 255), 0.35);
  cairo_fill(cr);
}

// A drifting asteroid: a lumpy rock, lit from the upper left, cratered.
// Three looks (the variant): grey, reddish, with a vein of violet crystal.
void driftRock(const Box& b)
{
  cairo_t* cr = b.cr;
  Rng rng(std::uint32_t(911 + b.variant * 97 + int(b.w)));
  const double cx = kM + b.w * 0.5, cy = kM + b.h * 0.5, rr = b.w * 0.5;
  const Color base = b.variant == 1 ? rgb(140, 104, 92) : (b.variant == 2 ? rgb(104, 100, 116) : rgb(122, 112, 104));
  const int n = 11;
  double pts[n][2];
  for (int k = 0; k < n; ++k)
  {
    const double a = k * 2 * kPi / n, d = rr * rng.range(0.82f, 1.0f);
    pts[k][0] = cx + std::cos(a) * d;
    pts[k][1] = cy + std::sin(a) * d;
  }
  cairo_move_to(cr, (pts[0][0] + pts[1][0]) * 0.5, (pts[0][1] + pts[1][1]) * 0.5);
  for (int k = 1; k <= n; ++k)
  {
    const auto& p = pts[k % n];
    const auto& q = pts[(k + 1) % n];
    cairo_curve_to(cr, p[0], p[1], p[0], p[1], (p[0] + q[0]) * 0.5, (p[1] + q[1]) * 0.5);
  }
  cairo_close_path(cr);
  radialFill(cr, cx, cy, rr * 1.1, lighten(base, 0.35f), darken(base, 0.55f));
  setColor(cr, kInk);
  cairo_set_line_width(cr, kLine);
  cairo_stroke(cr);
  for (int c = 0; c < int(b.w / 20) + 1; ++c)
  {
    const double x = cx + rng.range(-0.45f, 0.45f) * rr, y = cy + rng.range(-0.45f, 0.45f) * rr;
    const double r = rr * rng.range(0.1f, 0.22f);
    circle(cr, x, y, r);
    setRgba(cr, darken(base, 0.5f), 0.8);
    cairo_fill(cr);
    cairo_arc(cr, x, y, r, kPi * 0.05, kPi * 0.95);
    cairo_set_line_width(cr, std::max(1.2, r * 0.3));
    setRgba(cr, lighten(base, 0.35f), 0.7);
    cairo_stroke(cr);
  }
  if (b.variant == 2)
  {
    cairo_move_to(cr, cx - rr * 0.6, cy + rr * 0.2);
    cairo_line_to(cr, cx - rr * 0.1, cy - rr * 0.1);
    cairo_line_to(cr, cx + rr * 0.5, cy + rr * 0.15);
    cairo_set_line_width(cr, 3.5);
    setColor(cr, rgb(200, 140, 255));
    cairo_stroke(cr);
    radialGlow(cr, cx - rr * 0.1, cy - rr * 0.1, rr * 0.5, rgb(200, 140, 255), 0.4);
  }
  // The dark side, lower right.
  cairo_save(cr);
  circle(cr, cx, cy, rr * 0.98);
  cairo_clip(cr);
  circle(cr, cx + rr * 0.55, cy + rr * 0.5, rr * 0.95);
  setRgba(cr, rgb(10, 6, 20), 0.35);
  cairo_fill(cr);
  cairo_restore(cr);
}

// The derelict probe: an old cylinder with a dish, two solar panels (one
// snapped), a round window in its middle and SN-42 stencilled on its side.
// Frame 1 its warning light is on.
void probe(const Box& b)
{
  cairo_t* cr = b.cr;
  const double x0 = kM, y0 = kM, w = b.w, h = b.h;
  const double bx = x0 + w * 0.3, by = y0 + h * 0.3, bw = w * 0.4, bh = h * 0.42;
  // Panels.
  for (int s : {-1, 1})
  {
    const double px = s < 0 ? x0 + 4 : bx + bw + 8, pw = bx - x0 - 12;
    cairo_save(cr);
    if (s > 0)
    {
      cairo_translate(cr, px, by + bh * 0.5);
      cairo_rotate(cr, 0.35); // snapped
      cairo_translate(cr, -px, -(by + bh * 0.5));
    }
    cairo_rectangle(cr, px, by + bh * 0.25, pw, bh * 0.5);
    fillOutline(cr, rgb(40, 60, 120), kInk, 2);
    for (double gx = px + pw / 4; gx < px + pw; gx += pw / 4)
    {
      cairo_move_to(cr, gx, by + bh * 0.25);
      cairo_line_to(cr, gx, by + bh * 0.75);
      setRgba(cr, rgb(140, 180, 255), 0.6);
      cairo_set_line_width(cr, 1.2);
      cairo_stroke(cr);
    }
    cairo_restore(cr);
    cairo_move_to(cr, s < 0 ? x0 + 4 + pw : bx + bw, by + bh * 0.5);
    cairo_line_to(cr, s < 0 ? bx : bx + bw + 8, by + bh * 0.5);
    cairo_set_line_width(cr, 3);
    setColor(cr, rgb(150, 150, 160));
    cairo_stroke(cr);
  }
  // Body.
  cairo_rectangle(cr, bx, by, bw, bh);
  fillGradientOutline(cr, by, by + bh, rgb(220, 214, 196), rgb(120, 112, 100), kInk, 2.4);
  for (int i = 1; i < 4; ++i)
  {
    cairo_move_to(cr, bx + bw * i / 4, by);
    cairo_line_to(cr, bx + bw * i / 4, by + bh);
    setRgba(cr, rgb(60, 50, 40), 0.4);
    cairo_set_line_width(cr, 1.2);
    cairo_stroke(cr);
  }
  // The dish on top.
  cairo_move_to(cr, bx + bw * 0.5, by);
  cairo_line_to(cr, bx + bw * 0.5, by - h * 0.12);
  cairo_set_line_width(cr, 3);
  setColor(cr, rgb(150, 150, 160));
  cairo_stroke(cr);
  cairo_arc(cr, bx + bw * 0.5, by - h * 0.22, w * 0.14, kPi * 0.15, kPi * 0.85);
  cairo_close_path(cr);
  fillOutline(cr, rgb(200, 200, 210), kInk, 2);
  // The window.
  const double wx = bx + bw * 0.5, wy = by + bh * 0.48, wr = std::min(bw, bh) * 0.32;
  circle(cr, wx, wy, wr + 4);
  fillOutline(cr, rgb(170, 160, 150), kInk, 2);
  circle(cr, wx, wy, wr);
  setRgba(cr, rgb(30, 60, 90), 0.55);
  cairo_fill(cr);
  ellipse(cr, wx - wr * 0.35, wy - wr * 0.4, wr * 0.35, wr * 0.18);
  setRgba(cr, rgb(255, 255, 255), 0.4);
  cairo_fill(cr);
  // SN-42, stencilled.
  cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, std::max(10.0, bh * 0.16));
  cairo_move_to(cr, bx + 6, by + bh - 6);
  setColor(cr, rgb(170, 40, 30));
  cairo_show_text(cr, "SN-42");
  // The warning light.
  circle(cr, bx + bw - 10, by + 9, 4);
  setColor(cr, b.frame ? rgb(255, 60, 40) : rgb(110, 30, 30));
  cairo_fill(cr);
  if (b.frame)
    radialGlow(cr, bx + bw - 10, by + 9, 16, rgb(255, 60, 40), 0.7);
}

// The landing pad: steel plates with hazard stripes on the rock, landing
// lights along the front that blink in turn.
void landingPad(const Box& b)
{
  cairo_t* cr = b.cr;
  const double x0 = kM, y0 = kM, w = b.w;
  cairo_rectangle(cr, x0, y0, w, 16);
  fillGradientOutline(cr, y0, y0 + 16, rgb(190, 196, 210), rgb(90, 96, 112), kInk, 2);
  for (double x = x0 + 8; x < x0 + w - 8; x += 24)
  {
    cairo_move_to(cr, x, y0 + 16);
    cairo_line_to(cr, x + 10, y0 + 16);
    cairo_line_to(cr, x + 18, y0 + 2);
    cairo_line_to(cr, x + 8, y0 + 2);
    cairo_close_path(cr);
    setRgba(cr, rgb(255, 200, 40), 0.85);
    cairo_fill(cr);
  }
  // The pad's big H, in the middle.
  cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 14);
  cairo_move_to(cr, x0 + w * 0.5 - 6, y0 + 13);
  setColor(cr, rgb(255, 255, 255));
  cairo_show_text(cr, "H");
  // Struts down into the rock.
  for (double x = x0 + 20; x < x0 + w; x += 70)
  {
    cairo_rectangle(cr, x, y0 + 16, 8, std::max(8.0, b.h - 16));
    fillOutline(cr, rgb(110, 116, 130), kInk, 1.5);
  }
  int k = 0;
  for (double x = x0 + 12; x < x0 + w; x += 32, ++k)
  {
    const bool on = (k + b.frame) % 2 == 0;
    circle(cr, x, y0 - 3, 3.5);
    setColor(cr, on ? rgb(120, 255, 160) : rgb(40, 90, 60));
    cairo_fill(cr);
    if (on)
      radialGlow(cr, x, y0 - 3, 14, rgb(120, 255, 160), 0.6);
  }
}

// A bank of Vurr's violet clouds, soft at the edges. Variant: how far away
// (0 far and pale, 2 close and dense).
void cloudBand(const Box& b)
{
  cairo_t* cr = b.cr;
  Rng rng(std::uint32_t(3301 + b.variant * 13));
  const Color c = b.variant == 0 ? rgb(200, 160, 240) : (b.variant == 1 ? rgb(170, 110, 220) : rgb(130, 70, 190));
  for (double x = kM; x < kM + b.w; x += rng.range(50, 110))
  {
    const double rx = rng.range(70, 150), ry = rng.range(30, 70), y = kM + b.h * 0.55 + rng.range(-20, 20);
    cairo_pattern_t* p = cairo_pattern_create_radial(x, y, 0, x, y, rx);
    cairo_pattern_add_color_stop_rgba(p, 0, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, 0.55);
    cairo_pattern_add_color_stop_rgba(p, 1, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, 0.0);
    ellipse(cr, x, y, rx, ry);
    cairo_set_source(cr, p);
    cairo_fill(cr);
    cairo_pattern_destroy(p);
  }
}

// A glass bubble with a collar, for Starfall's rubber duck.
void duckHelmet(const Box& b)
{
  cairo_t* cr = b.cr;
  const double cx = kM + b.w * 0.5, cy = kM + b.h * 0.45, r = b.w * 0.52;
  circle(cr, cx, cy, r);
  setRgba(cr, rgb(180, 230, 255), 0.18);
  cairo_fill_preserve(cr);
  setRgba(cr, rgb(220, 245, 255), 0.85);
  cairo_set_line_width(cr, 2.2);
  cairo_stroke(cr);
  cairo_arc(cr, cx, cy, r * 0.8, kPi * 1.1, kPi * 1.45);
  setRgba(cr, rgb(255, 255, 255), 0.8);
  cairo_set_line_width(cr, 3);
  cairo_stroke(cr);
  ellipse(cr, cx, cy + r * 0.92, r * 0.7, r * 0.14);
  fillOutline(cr, rgb(200, 204, 214), kInk, 1.4);
}

using DrawFn = void (*)(const Box&);

} // namespace

bool drawStarfallArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame)
{
  static const std::map<std::string, DrawFn> kRoutines{
    {"void_ray", voidRay},
    {"rock_leech", rockLeech},
    {"drift_rock", driftRock},
    {"derelict_probe", probe},
    {"landing_pad", landingPad},
    {"cloud_band", cloudBand},
    {"duck_helmet", duckHelmet},
  };
  const auto it = kRoutines.find(key);
  if (it == kRoutines.end())
    return false;
  it->second(Box{cr, t, w, h, variant, frame});
  return true;
}

} // namespace gr
