// The Reactor Core's residents (Level 20) in the style of enemy_art.cpp:
// each drawn in screen pixels into a texture with a 32 px margin, the cell
// box at (32, 32) .. (32 + w, 32 + h), facing right.

#include "assets/enemy_art_reactor.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace gr
{

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr Color kInk = rgb(16, 18, 30);
constexpr double kLine = 2.4;
constexpr double kM = 32.0;
constexpr Color kYellow = rgb(255, 204, 40);

Color darken(Color c, float t) { return lerpColor(c, rgb(0, 0, 0), t); }

void setRgba(cairo_t* cr, Color c, double a)
{
  cairo_set_source_rgba(cr, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, a);
}

void circle(cairo_t* cr, double x, double y, double r) { cairo_arc(cr, x, y, r, 0, 2 * kPi); }

void ellipse(cairo_t* cr, double x, double y, double rx, double ry)
{
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_scale(cr, rx, ry);
  cairo_new_sub_path(cr);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_restore(cr);
}

void fillLinear(cairo_t* cr, double x0, double y0, double x1, double y1, Color a, Color b, double line = kLine)
{
  cairo_pattern_t* g = cairo_pattern_create_linear(x0, y0, x1, y1);
  cairo_pattern_add_color_stop_rgb(g, 0, redOf(a) / 255.0, greenOf(a) / 255.0, blueOf(a) / 255.0);
  cairo_pattern_add_color_stop_rgb(g, 1, redOf(b) / 255.0, greenOf(b) / 255.0, blueOf(b) / 255.0);
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  if (line > 0)
  {
    setColor(cr, kInk);
    cairo_set_line_width(cr, line);
    cairo_stroke(cr);
  }
  else
    cairo_new_path(cr);
}

void fillRadial(cairo_t* cr, double x, double y, double r, Color light, Color dark, double line = kLine)
{
  cairo_pattern_t* g = cairo_pattern_create_radial(x - r * 0.35, y - r * 0.4, r * 0.1, x, y, r);
  cairo_pattern_add_color_stop_rgb(g, 0, redOf(light) / 255.0, greenOf(light) / 255.0, blueOf(light) / 255.0);
  cairo_pattern_add_color_stop_rgb(g, 1, redOf(dark) / 255.0, greenOf(dark) / 255.0, blueOf(dark) / 255.0);
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  if (line > 0)
  {
    setColor(cr, kInk);
    cairo_set_line_width(cr, line);
    cairo_stroke(cr);
  }
  else
    cairo_new_path(cr);
}

void glow(cairo_t* cr, double x, double y, double r, Color c, double a)
{
  cairo_pattern_t* g = cairo_pattern_create_radial(x, y, 0, x, y, r);
  cairo_pattern_add_color_stop_rgba(g, 0, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, a);
  cairo_pattern_add_color_stop_rgba(g, 1, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, 0);
  cairo_set_source(cr, g);
  circle(cr, x, y, r);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
}

// A limb: an inked stroke with a coloured core through three points.
void limb(cairo_t* cr, double x0, double y0, double x1, double y1, double x2, double y2, double width, Color c)
{
  cairo_move_to(cr, x0, y0);
  cairo_line_to(cr, x1, y1);
  cairo_line_to(cr, x2, y2);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
  setColor(cr, kInk);
  cairo_set_line_width(cr, width + 2 * kLine);
  cairo_stroke_preserve(cr);
  setColor(cr, c);
  cairo_set_line_width(cr, width);
  cairo_stroke(cr);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
  cairo_set_line_join(cr, CAIRO_LINE_JOIN_MITER);
}

// A cheap repeatable random number in 0..1.
double rnd(std::uint32_t& s)
{
  s ^= s << 13;
  s ^= s >> 17;
  s ^= s << 5;
  return double(s & 0xFFFF) / 65535.0;
}

// A jagged bolt of lightning from (x0, y0) to (x1, y1).
void bolt(cairo_t* cr, double x0, double y0, double x1, double y1, int steps, double jag, std::uint32_t& s)
{
  const double dx = x1 - x0, dy = y1 - y0, len = std::max(1.0, std::hypot(dx, dy));
  const double nx = -dy / len, ny = dx / len;
  cairo_move_to(cr, x0, y0);
  for (int i = 1; i < steps; ++i)
  {
    const double t = double(i) / steps, off = (rnd(s) * 2.0 - 1.0) * jag;
    cairo_line_to(cr, x0 + dx * t + nx * off, y0 + dy * t + ny * off);
  }
  cairo_line_to(cr, x1, y1);
}

// --- Conduit Spark ------------------------------------------------------------------

// A crackling ball of electricity: a white-hot core in a blue halo, bolts
// jumping off it, a few sparks trailing behind.
void conduitSpark(cairo_t* cr, double w, double h, int frame)
{
  const double cx = kM + w * 0.5, cy = kM + h * 0.5, s = w / 64.0;
  std::uint32_t seed = 0x9e3779b9u ^ std::uint32_t(frame * 7919 + 13);
  glow(cr, cx, cy, 44 * s, rgb(60, 150, 255), 0.55);
  glow(cr, cx, cy, 26 * s, rgb(170, 230, 255), 0.7);
  // Bolts, a wide blue stroke under a thin white one.
  for (int k = 0; k < 6; ++k)
  {
    const double a = k * kPi / 3.0 + rnd(seed) * 0.8 + frame * 0.5;
    const double r0 = 10 * s, r1 = (24 + rnd(seed) * 12) * s;
    const std::uint32_t keep = seed;
    for (int pass = 0; pass < 2; ++pass)
    {
      seed = keep;
      bolt(cr, cx + std::cos(a) * r0, cy + std::sin(a) * r0, cx + std::cos(a + 0.35) * r1, cy + std::sin(a + 0.35) * r1, 4,
        4.0 * s, seed);
      if (pass == 0)
        setRgba(cr, rgb(90, 180, 255), 0.8);
      else
        setRgba(cr, rgb(250, 254, 255), 1.0);
      cairo_set_line_width(cr, pass == 0 ? 4.2 * s : 1.6 * s);
      cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
      cairo_stroke(cr);
    }
  }
  // The core.
  circle(cr, cx, cy, 13 * s);
  cairo_pattern_t* g = cairo_pattern_create_radial(cx - 3 * s, cy - 4 * s, 1, cx, cy, 13 * s);
  cairo_pattern_add_color_stop_rgb(g, 0, 1, 1, 1);
  cairo_pattern_add_color_stop_rgb(g, 0.55, 0.75, 0.92, 1.0);
  cairo_pattern_add_color_stop_rgb(g, 1, 0.25, 0.55, 1.0);
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  setColor(cr, rgb(30, 70, 170));
  cairo_set_line_width(cr, 1.8 * s);
  cairo_stroke(cr);
  // A swirl in the core.
  cairo_arc(cr, cx, cy, 7 * s, frame * 1.3, frame * 1.3 + kPi * 1.2);
  setRgba(cr, rgb(40, 110, 230), 0.8);
  cairo_set_line_width(cr, 2.0 * s);
  cairo_stroke(cr);
  // Sparks trailing behind it (to the left: it faces right).
  for (int k = 0; k < 5; ++k)
  {
    const double px = cx - (16 + k * 7 + rnd(seed) * 4) * s, py = cy + (rnd(seed) * 2 - 1) * 10 * s;
    circle(cr, px, py, (2.6 - k * 0.35) * s);
    setRgba(cr, rgb(220, 245, 255), 0.9 - k * 0.12);
    cairo_fill(cr);
  }
}

// --- Shield Drone ---------------------------------------------------------------------

// A hovering drone: a white steel saucer with a dark sensor band, two
// thruster pods glowing underneath and the bubble emitter, a blue crystal
// in a ring, on a mast on top. Variant 1: the emitter flares.
void shieldDrone(cairo_t* cr, double w, double h, int variant, int frame)
{
  const double s = w / 128.0;
  const double cx = kM + w * 0.5, cy = kM + h * 0.58;
  const bool flare = variant == 1;
  // Thruster glow under the pods.
  for (int k = -1; k <= 1; k += 2)
  {
    const double px = cx + k * 40 * s, py = cy + 18 * s;
    glow(cr, px, py + 14 * s, (frame % 2 ? 20 : 16) * s, rgb(100, 200, 255), 0.7);
    roundedRect(cr, px - 13 * s, py - 6 * s, 26 * s, 18 * s, 6 * s);
    fillLinear(cr, 0, py - 6 * s, 0, py + 12 * s, rgb(150, 160, 180), rgb(60, 66, 84));
    ellipse(cr, px, py + 12 * s, 9 * s, 3 * s);
    setColor(cr, rgb(200, 240, 255));
    cairo_fill(cr);
  }
  // The emitter mast and its ring.
  cairo_rectangle(cr, cx - 3.5 * s, cy - 36 * s, 7 * s, 18 * s);
  fillLinear(cr, cx - 3.5 * s, 0, cx + 3.5 * s, 0, rgb(220, 226, 236), rgb(90, 98, 116), 1.8);
  const double ey = cy - 40 * s;
  glow(cr, cx, ey, (flare ? 40 : 24) * s, rgb(110, 210, 255), flare ? 0.9 : 0.6);
  ellipse(cr, cx, ey, 15 * s, 5 * s);
  setColor(cr, kInk);
  cairo_set_line_width(cr, 6 * s);
  cairo_stroke(cr);
  ellipse(cr, cx, ey, 15 * s, 5 * s);
  setColor(cr, rgb(190, 198, 214));
  cairo_set_line_width(cr, 3 * s);
  cairo_stroke(cr);
  // The crystal.
  cairo_move_to(cr, cx, ey - 12 * s);
  cairo_line_to(cr, cx + 6 * s, ey - 2 * s);
  cairo_line_to(cr, cx, ey + 6 * s);
  cairo_line_to(cr, cx - 6 * s, ey - 2 * s);
  cairo_close_path(cr);
  fillLinear(cr, cx - 6 * s, ey - 12 * s, cx + 6 * s, ey + 6 * s, flare ? rgb(255, 255, 255) : rgb(200, 245, 255),
    rgb(40, 140, 240), 1.6);
  if (flare)
  {
    for (int k = 0; k < 8; ++k)
    {
      const double a = k * kPi / 4.0 + 0.2;
      cairo_move_to(cr, cx + std::cos(a) * 10 * s, ey - 3 * s + std::sin(a) * 10 * s);
      cairo_line_to(cr, cx + std::cos(a) * 22 * s, ey - 3 * s + std::sin(a) * 22 * s);
    }
    setRgba(cr, rgb(230, 250, 255), 0.9);
    cairo_set_line_width(cr, 2 * s);
    cairo_stroke(cr);
  }
  // The saucer.
  ellipse(cr, cx, cy, 56 * s, 20 * s);
  fillLinear(cr, 0, cy - 20 * s, 0, cy + 20 * s, rgb(250, 252, 255), rgb(130, 140, 160));
  // The dome on top.
  cairo_new_sub_path(cr);
  cairo_arc(cr, cx, cy - 6 * s, 26 * s, kPi, 2 * kPi);
  cairo_close_path(cr);
  fillLinear(cr, 0, cy - 32 * s, 0, cy - 6 * s, rgb(236, 240, 248), rgb(160, 170, 190));
  // The dark sensor band with its lights.
  cairo_save(cr);
  ellipse(cr, cx, cy, 56 * s, 20 * s);
  cairo_clip(cr);
  cairo_rectangle(cr, cx - 60 * s, cy + 1 * s, 120 * s, 8 * s);
  setColor(cr, rgb(40, 46, 62));
  cairo_fill(cr);
  for (int k = 0; k < 7; ++k)
  {
    circle(cr, cx - 42 * s + k * 14 * s, cy + 5 * s, 2 * s);
    setColor(cr, (k + frame) % 3 == 0 ? kYellow : rgb(110, 220, 255));
    cairo_fill(cr);
  }
  cairo_restore(cr);
  ellipse(cr, cx, cy, 56 * s, 20 * s);
  setColor(cr, kInk);
  cairo_set_line_width(cr, kLine);
  cairo_stroke(cr);
  // A hazard tab on its flank.
  cairo_save(cr);
  cairo_rectangle(cr, cx - 44 * s, cy - 12 * s, 16 * s, 7 * s);
  cairo_clip(cr);
  setColor(cr, kYellow);
  cairo_paint(cr);
  for (int k = -2; k < 6; ++k)
  {
    cairo_move_to(cr, cx - 44 * s + k * 6 * s, cy - 5 * s);
    cairo_line_to(cr, cx - 41 * s + k * 6 * s, cy - 5 * s);
    cairo_line_to(cr, cx - 34 * s + k * 6 * s, cy - 12 * s);
    cairo_line_to(cr, cx - 37 * s + k * 6 * s, cy - 12 * s);
    cairo_close_path(cr);
  }
  setColor(cr, rgb(30, 32, 40));
  cairo_fill(cr);
  cairo_restore(cr);
  // The eye toward its facing.
  circle(cr, cx + 30 * s, cy - 8 * s, 8 * s);
  fillLinear(cr, 0, cy - 16 * s, 0, cy, rgb(70, 80, 100), rgb(16, 18, 28), 2.0);
  circle(cr, cx + 32 * s, cy - 8 * s, 3.6 * s);
  setColor(cr, flare ? rgb(255, 255, 255) : rgb(120, 230, 255));
  cairo_fill(cr);
  glow(cr, cx + 32 * s, cy - 8 * s, 9 * s, rgb(120, 230, 255), 0.6);
  // Shine.
  ellipse(cr, cx - 10 * s, cy - 22 * s, 10 * s, 4 * s);
  setRgba(cr, rgb(255, 255, 255), 0.85);
  cairo_fill(cr);
}

// --- Isotope Imp -------------------------------------------------------------------------

// A small scuttling critter of glowing green-yellow goo with a bright
// nucleus, two goggle eyes and six stubby legs. It glows hotter with every
// pulse it lives through. States: scuttling, crouched and glowing before a
// lunge, lunging with its mouth wide, standing up to salute (in its hard hat).
void isotopeImp(cairo_t* cr, double w, double h, int variant, int frame)
{
  const int state = variant % 4;
  const bool hat = (variant / 4) % 2 == 1;
  const int heat = std::clamp(variant / 8, 0, 4);
  const double s = w / 96.0;
  const double floor = kM + h;
  const float hk = float(heat) / 4.0f;
  const Color hot = lerpColor(rgb(150, 255, 60), rgb(255, 250, 150), hk);
  const Color mid = lerpColor(rgb(70, 190, 40), rgb(230, 200, 40), hk);
  const Color dark = lerpColor(rgb(30, 100, 30), rgb(150, 100, 20), hk);
  const bool tell = state == 1, lunge = state == 2, salute = state == 3;
  double cx = kM + w * 0.48, cy = floor - 22 * s;
  double rx = 30 * s, ry = 17 * s;
  if (tell)
  {
    cy += 4 * s;
    rx *= 1.08;
    ry *= 0.85;
  }
  if (lunge)
  {
    cx += 6 * s;
    rx *= 1.2;
    ry *= 0.85;
  }
  glow(cr, cx, cy, (40 + heat * 5 + (tell ? 16 : 0)) * s, hot, tell ? 0.75 : 0.35 + 0.08 * heat);
  cairo_save(cr);
  if (salute)
  {
    // Rocked back onto its rear legs, chest out.
    cairo_translate(cr, cx - 22 * s, floor - 2 * s);
    cairo_rotate(cr, -0.3);
    cairo_translate(cr, -(cx - 22 * s), -(floor - 2 * s));
    cy -= 4 * s;
  }
  // Legs: three pairs, the far ones darker; they alternate as it scuttles.
  for (int side = 0; side < 2; ++side)
    for (int k = 0; k < 3; ++k)
    {
      const double lx = cx - 18 * s + k * 18 * s + (side ? 4 * s : 0);
      const bool up = !salute && !tell && ((k + side + frame) % 2 == 0);
      const double footX = lx + (lunge ? -10 * s : (up ? 5 * s : -2 * s));
      const double footY = floor - (up ? 5 * s : 1 * s);
      limb(cr, lx, cy + ry * 0.4, lx + (lunge ? -4 * s : 2 * s), floor - 9 * s, footX, footY, 4.5 * s,
        side ? darken(dark, 0.25f) : dark);
    }
  // The body: a translucent goo shell over a glowing middle.
  ellipse(cr, cx, cy, rx, ry);
  cairo_pattern_t* g = cairo_pattern_create_radial(cx - rx * 0.2, cy - ry * 0.3, 1, cx, cy, rx);
  cairo_pattern_add_color_stop_rgb(g, 0, redOf(hot) / 255.0, greenOf(hot) / 255.0, blueOf(hot) / 255.0);
  cairo_pattern_add_color_stop_rgb(g, 0.6, redOf(mid) / 255.0, greenOf(mid) / 255.0, blueOf(mid) / 255.0);
  cairo_pattern_add_color_stop_rgb(g, 1, redOf(dark) / 255.0, greenOf(dark) / 255.0, blueOf(dark) / 255.0);
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  setColor(cr, kInk);
  cairo_set_line_width(cr, kLine);
  cairo_stroke(cr);
  // Little drips of goo off its belly.
  for (int k = 0; k < 3; ++k)
  {
    const double dx = cx - 14 * s + k * 13 * s;
    ellipse(cr, dx, cy + ry * 0.92, 3 * s, 4 * s);
    setColor(cr, mid);
    cairo_fill(cr);
  }
  // The nucleus: a white-hot dot with two orbiting electrons.
  const double nx = cx - 6 * s, ny = cy + 2 * s;
  glow(cr, nx, ny, (12 + heat * 2 + (tell ? 8 : 0)) * s, rgb(255, 255, 230), 0.9);
  for (int k = 0; k < 2; ++k)
  {
    cairo_save(cr);
    cairo_translate(cr, nx, ny);
    cairo_rotate(cr, k ? 0.7 : -0.7);
    ellipse(cr, 0, 0, 11 * s, 4 * s);
    cairo_restore(cr);
    setRgba(cr, rgb(255, 255, 255), 0.55);
    cairo_set_line_width(cr, 1.2 * s);
    cairo_stroke(cr);
    const double a = frame * 2.1 + k * 2.5;
    cairo_save(cr);
    cairo_translate(cr, nx, ny);
    cairo_rotate(cr, k ? 0.7 : -0.7);
    circle(cr, std::cos(a) * 11 * s, std::sin(a) * 4 * s, 1.8 * s);
    cairo_restore(cr);
    setColor(cr, rgb(255, 255, 255));
    cairo_fill(cr);
  }
  if (hat)
  {
    // A yellow hard hat with a brim and a radiation sticker, sitting behind
    // the eye stalks so the goggles stay in view.
    const double hx = cx - 2 * s, hy = cy - ry * 0.85;
    roundedRect(cr, hx - 20 * s, hy - 2 * s, 40 * s, 5 * s, 2 * s);
    fillLinear(cr, 0, hy - 2 * s, 0, hy + 3 * s, rgb(255, 222, 80), rgb(210, 150, 20), 1.8);
    cairo_new_sub_path(cr);
    cairo_arc(cr, hx, hy - 1 * s, 14 * s, kPi, 2 * kPi);
    cairo_close_path(cr);
    fillLinear(cr, 0, hy - 15 * s, 0, hy, rgb(255, 236, 120), rgb(240, 180, 30), 2.0);
    cairo_rectangle(cr, hx - 2 * s, hy - 15 * s, 4 * s, 14 * s);
    setRgba(cr, rgb(255, 255, 255), 0.5);
    cairo_fill(cr);
    circle(cr, hx + 7 * s, hy - 6 * s, 3.4 * s);
    setColor(cr, rgb(30, 30, 30));
    cairo_fill(cr);
    for (int k = 0; k < 3; ++k)
    {
      const double a = -kPi * 0.5 + k * kPi * 2.0 / 3.0;
      cairo_move_to(cr, hx + 7 * s, hy - 6 * s);
      cairo_arc(cr, hx + 7 * s, hy - 6 * s, 3 * s, a - 0.5, a + 0.5);
      cairo_close_path(cr);
    }
    setColor(cr, kYellow);
    cairo_fill(cr);
  }
  // Eyes on short stalks toward its facing.
  const double ex = cx + rx * 0.45, eyY = cy - ry * 0.75;
  for (int k = 0; k < 2; ++k)
  {
    const double px = ex + k * 11 * s, py = eyY - (k ? 2 * s : 0);
    limb(cr, px - 2 * s, cy - ry * 0.4, px - 1 * s, py + 4 * s, px, py, 2.4 * s, mid);
    circle(cr, px, py, 6 * s);
    fillRadial(cr, px, py, 6 * s, rgb(255, 255, 255), rgb(200, 210, 190), 2.0);
    if (tell)
    {
      // Narrowed, glaring.
      cairo_rectangle(cr, px - 6 * s, py - 6 * s, 12 * s, 5 * s);
      setColor(cr, dark);
      cairo_fill(cr);
    }
    circle(cr, px + 2 * s, py + (tell ? 1 * s : 0), 2.4 * s);
    setColor(cr, kInk);
    cairo_fill(cr);
  }
  // The mouth: a grin, gaping on a lunge.
  const double mx = cx + rx * 0.62, my = cy + 3 * s;
  if (lunge)
  {
    ellipse(cr, mx, my, 8 * s, 7 * s);
    setColor(cr, rgb(40, 20, 20));
    cairo_fill_preserve(cr);
    setColor(cr, kInk);
    cairo_set_line_width(cr, 2);
    cairo_stroke(cr);
    for (int k = 0; k < 3; ++k)
    {
      cairo_move_to(cr, mx - 6 * s + k * 5 * s, my - 6 * s);
      cairo_line_to(cr, mx - 4 * s + k * 5 * s, my - 2 * s);
      cairo_line_to(cr, mx - 2 * s + k * 5 * s, my - 6 * s);
      setColor(cr, rgb(255, 255, 240));
      cairo_fill(cr);
    }
  }
  else
  {
    cairo_arc(cr, mx - 2 * s, my - 2 * s, 7 * s, 0.2, kPi * 0.8);
    setColor(cr, kInk);
    cairo_set_line_width(cr, 2.2 * s);
    cairo_stroke(cr);
  }
  if (salute)
  {
    // The little arm up to the brim of its hat.
    const double ax = cx + rx * 0.35, ay = cy + 2 * s;
    limb(cr, ax, ay, ax + 16 * s, ay - 8 * s, cx + 15 * s, cy - ry * 0.85 - 1 * s, 4.0 * s, mid);
  }
  // Shine on the goo.
  ellipse(cr, cx - rx * 0.4, cy - ry * 0.5, rx * 0.25, ry * 0.18);
  setRgba(cr, rgb(255, 255, 255), 0.7);
  cairo_fill(cr);
  cairo_restore(cr);
}

} // namespace

bool drawReactorArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame)
{
  (void)t;
  if (key == "conduit_spark")
    conduitSpark(cr, w, h, frame);
  else if (key == "shield_drone")
    shieldDrone(cr, w, h, variant, frame);
  else if (key == "isotope_imp")
    isotopeImp(cr, w, h, variant, frame);
  else
    return false;
  return true;
}

} // namespace gr
