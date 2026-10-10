// The Gravity Lab's residents (Level 19) in the style of enemy_art.cpp: each
// drawn in screen pixels into a texture with a 32 px margin, the cell box at
// (32, 32) .. (32 + w, 32 + h), facing right.

#include "assets/enemy_art_grav.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace gr
{

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr Color kInk = rgb(20, 18, 30);
constexpr double kLine = 2.4;
constexpr double kM = 32.0;
constexpr Color kOrange = rgb(255, 136, 36);

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

// Fills the current path with a gradient from (x0, y0) to (x1, y1) and
// outlines it.
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

// A shaded ball: light from the top left.
void fillBall(cairo_t* cr, double x, double y, double r, Color light, Color dark, double line = kLine)
{
  circle(cr, x, y, r);
  cairo_pattern_t* g = cairo_pattern_create_radial(x - r * 0.4, y - r * 0.45, r * 0.1, x, y, r);
  cairo_pattern_add_color_stop_rgb(g, 0, redOf(light) / 255.0, greenOf(light) / 255.0, blueOf(light) / 255.0);
  cairo_pattern_add_color_stop_rgb(g, 1, redOf(dark) / 255.0, greenOf(dark) / 255.0, blueOf(dark) / 255.0);
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  setColor(cr, kInk);
  cairo_set_line_width(cr, line);
  cairo_stroke(cr);
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

// Diagonal orange and charcoal hazard stripes over the current clip.
void hazardBand(cairo_t* cr, double x, double y, double w, double h, double step)
{
  cairo_save(cr);
  cairo_rectangle(cr, x, y, w, h);
  cairo_clip(cr);
  setColor(cr, kOrange);
  cairo_paint(cr);
  for (double sx = x - h; sx < x + w + h; sx += step)
  {
    cairo_move_to(cr, sx, y + h);
    cairo_line_to(cr, sx + step * 0.5, y + h);
    cairo_line_to(cr, sx + step * 0.5 + h, y);
    cairo_line_to(cr, sx + h, y);
    cairo_close_path(cr);
  }
  setColor(cr, rgb(44, 46, 56));
  cairo_fill(cr);
  cairo_restore(cr);
}

// --- Flip Walker -------------------------------------------------------------------

// A magnetic boot: a red horseshoe block with silver pole shoes underneath
// and faint field lines clinging to the floor.
void magBoot(cairo_t* cr, double x, double y, double s, bool sparking)
{
  roundedRect(cr, x - 15 * s, y - 15 * s, 30 * s, 13 * s, 4 * s);
  fillLinear(cr, 0, y - 15 * s, 0, y - 2 * s, rgb(255, 96, 80), rgb(170, 30, 34));
  for (int k = -1; k <= 1; k += 2)
  {
    cairo_rectangle(cr, x + k * 9 * s - 4.5 * s, y - 4 * s, 9 * s, 4 * s);
    fillLinear(cr, 0, y - 4 * s, 0, y, rgb(240, 244, 250), rgb(140, 148, 164), 1.6);
  }
  // A little N/S mark on the side.
  cairo_rectangle(cr, x - 9 * s, y - 12 * s, 6 * s, 3 * s);
  setColor(cr, rgba(255, 255, 255, 170));
  cairo_fill(cr);
  if (sparking)
    for (int k = 0; k < 4; ++k)
    {
      const double a = kPi * 0.25 + k * kPi * 0.17;
      cairo_move_to(cr, x + std::cos(a) * 6 * s, y + std::sin(a) * 3 * s);
      cairo_line_to(cr, x + std::cos(a) * 16 * s, y + std::sin(a) * 9 * s);
    }
  else
    for (int k = 0; k < 2; ++k)
    {
      cairo_move_to(cr, x - 14 * s - k * 3 * s, y + 0.5);
      cairo_curve_to(cr, x - 10 * s, y - 6 * s - k * 3 * s, x + 10 * s, y - 6 * s - k * 3 * s, x + 14 * s + k * 3 * s, y + 0.5);
    }
  setRgba(cr, rgb(120, 220, 255), sparking ? 0.9 : 0.55);
  cairo_set_line_width(cr, 1.4);
  cairo_stroke(cr);
}

// A squat lab robot on magnetic boots: a white shell with an orange hazard
// belt, a glass dome with its antenna lamp, a visor eye. Variant 1 has the
// visor swung round to face you (red: it is about to walk at you), variant
// 2 is falling, flailing, boots kicking off sparks.
void flipWalker(cairo_t* cr, double w, double h, int variant, int frame)
{
  const double s = w / 96.0;
  const bool flail = variant == 2, turn = variant == 1;
  const double floorY = kM + h;
  const double bx0 = kM + 6 * s, bx1 = kM + w - 6 * s;
  const double by0 = kM + h * 0.30, by1 = floorY - 34 * s;
  const double cx = (bx0 + bx1) * 0.5;
  // Legs and boots.
  for (int k = 0; k < 2; ++k)
  {
    const double hx = cx + (k == 0 ? -20 : 20) * s;
    double fx = hx, fy = floorY;
    if (flail)
    {
      fx = hx + (k == 0 ? -14 : 14) * s;
      fy = floorY - (k == frame % 2 ? 2 : 8) * s;
    }
    else
      fy = floorY - ((k + frame) % 2 ? 5 : 0) * s;
    limb(cr, hx, by1 - 4 * s, (hx + fx) * 0.5 + (k == 0 ? -3 : 3) * s, (by1 + fy) * 0.5 - 6 * s, fx, fy - 12 * s, 8 * s,
      rgb(150, 158, 174));
    magBoot(cr, fx, fy, s, flail);
  }
  // Arms: little grabbers at the sides (thrown up while it falls).
  for (int k = -1; k <= 1; k += 2)
  {
    const double sx = k < 0 ? bx0 + 4 * s : bx1 - 4 * s, sy = by0 + 26 * s;
    double hx = sx + k * 12 * s, hy = sy + 14 * s;
    if (flail)
    {
      hx = sx + k * 18 * s;
      hy = sy - (22 + (frame % 2) * 6) * s;
    }
    limb(cr, sx, sy, sx + k * 8 * s, (sy + hy) * 0.5, hx, hy, 6 * s, rgb(170, 178, 194));
    circle(cr, hx, hy, 5 * s);
    fillLinear(cr, 0, hy - 5 * s, 0, hy + 5 * s, rgb(255, 170, 90), rgb(200, 90, 30), 1.6);
  }
  // The antenna and its lamp.
  const double domeY = by0 + 2 * s;
  cairo_move_to(cr, cx - 6 * s, domeY - 22 * s);
  cairo_line_to(cr, cx - 10 * s, domeY - 38 * s);
  setColor(cr, kInk);
  cairo_set_line_width(cr, 2.6 * s);
  cairo_stroke(cr);
  glow(cr, cx - 10 * s, domeY - 40 * s, 10 * s, flail || turn ? rgb(255, 80, 60) : kOrange, 0.8);
  circle(cr, cx - 10 * s, domeY - 40 * s, 3.6 * s);
  fillLinear(cr, 0, domeY - 44 * s, 0, domeY - 36 * s, rgb(255, 230, 180), flail || turn ? rgb(230, 40, 40) : kOrange, 1.4);
  // The glass dome with a tiny brain of blinking circuits inside.
  cairo_new_sub_path(cr);
  cairo_arc(cr, cx, domeY, 24 * s, kPi, 2 * kPi);
  cairo_close_path(cr);
  cairo_pattern_t* dg = cairo_pattern_create_linear(0, domeY - 24 * s, 0, domeY);
  cairo_pattern_add_color_stop_rgba(dg, 0, 0.85, 0.95, 1.0, 0.9);
  cairo_pattern_add_color_stop_rgba(dg, 1, 0.45, 0.62, 0.78, 0.9);
  cairo_set_source(cr, dg);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(dg);
  setColor(cr, kInk);
  cairo_set_line_width(cr, kLine);
  cairo_stroke(cr);
  for (int k = 0; k < 3; ++k)
  {
    roundedRect(cr, cx - 12 * s + k * 9 * s, domeY - 12 * s + (k % 2) * 3 * s, 6 * s, 6 * s, 1.5 * s);
    setColor(cr, (k + frame) % 3 == 0 ? rgb(120, 255, 170) : rgb(60, 110, 140));
    cairo_fill(cr);
  }
  cairo_arc(cr, cx - 8 * s, domeY - 10 * s, 10 * s, kPi * 1.1, kPi * 1.45);
  setRgba(cr, rgb(255, 255, 255), 0.85);
  cairo_set_line_width(cr, 3 * s);
  cairo_stroke(cr);
  // The shell.
  roundedRect(cr, bx0, by0, bx1 - bx0, by1 - by0, 14 * s);
  fillLinear(cr, 0, by0, 0, by1, rgb(252, 253, 255), rgb(170, 180, 196));
  cairo_save(cr);
  roundedRect(cr, bx0, by0, bx1 - bx0, by1 - by0, 14 * s);
  cairo_clip(cr);
  hazardBand(cr, bx0, by1 - 18 * s, bx1 - bx0, 9 * s, 12 * s);
  cairo_rectangle(cr, bx0, by1 - 9 * s, bx1 - bx0, 9 * s);
  setRgba(cr, rgb(110, 120, 138), 0.6);
  cairo_fill(cr);
  cairo_restore(cr);
  roundedRect(cr, bx0, by0, bx1 - bx0, by1 - by0, 14 * s);
  setColor(cr, kInk);
  cairo_set_line_width(cr, kLine);
  cairo_stroke(cr);
  cairo_move_to(cr, bx0 + 12 * s, by0 + 5 * s);
  cairo_line_to(cr, bx1 - 24 * s, by0 + 5 * s);
  setRgba(cr, rgb(255, 255, 255), 0.95);
  cairo_set_line_width(cr, 3 * s);
  cairo_stroke(cr);
  // Vent slots on the side.
  for (int k = 0; k < 3; ++k)
  {
    roundedRect(cr, bx0 + 9 * s, by0 + 14 * s + k * 6 * s, 16 * s, 3 * s, 1.5 * s);
    setColor(cr, rgb(120, 130, 148));
    cairo_fill(cr);
  }
  // The visor: on the front while it walks, swung round to face you as it
  // turns, a dizzy spiral while it falls.
  const double vw = 34 * s, vh = 18 * s;
  const double vx = turn ? cx - vw * 0.5 : bx1 - vw - 4 * s, vy = by0 + 10 * s;
  roundedRect(cr, vx, vy, vw, vh, 8 * s);
  fillLinear(cr, 0, vy, 0, vy + vh, rgb(40, 48, 66), rgb(12, 14, 22), 2.0);
  const Color eye = turn ? rgb(255, 70, 50) : rgb(110, 230, 255);
  if (flail)
  {
    for (int k = 0; k < 2; ++k)
    {
      const double ex = vx + vw * (0.3 + 0.4 * k), ey = vy + vh * 0.5;
      cairo_new_sub_path(cr);
      for (int i = 0; i <= 16; ++i)
      {
        const double a = i * 0.7 + frame * 1.5, rr = i * 0.42 * s;
        if (i == 0)
          cairo_move_to(cr, ex, ey);
        else
          cairo_line_to(cr, ex + std::cos(a) * rr, ey + std::sin(a) * rr);
      }
      setColor(cr, rgb(255, 220, 90));
      cairo_set_line_width(cr, 1.6);
      cairo_stroke(cr);
    }
  }
  else
  {
    const double ex = turn ? cx : vx + vw * 0.62, ey = vy + vh * 0.5;
    glow(cr, ex, ey, 16 * s, eye, 0.7);
    roundedRect(cr, ex - 8 * s, ey - 4 * s, 16 * s, 8 * s, 4 * s);
    setColor(cr, eye);
    cairo_fill(cr);
    if (turn)
    {
      // The brows knit: two angry lines over the eye.
      cairo_move_to(cr, ex - 11 * s, ey - 9 * s);
      cairo_line_to(cr, ex - 2 * s, ey - 6 * s);
      cairo_move_to(cr, ex + 11 * s, ey - 9 * s);
      cairo_line_to(cr, ex + 2 * s, ey - 6 * s);
      setColor(cr, rgb(255, 200, 190));
      cairo_set_line_width(cr, 2.2 * s);
      cairo_stroke(cr);
    }
  }
  if (flail)
  {
    // Motion lines: it is falling.
    for (int k = 0; k < 3; ++k)
    {
      const double lx = bx0 + 10 * s + k * 30 * s;
      cairo_move_to(cr, lx, kM - 6 + (frame % 2) * 4);
      cairo_line_to(cr, lx, kM - 22 + (frame % 2) * 4);
    }
    setRgba(cr, rgb(255, 255, 255), 0.8);
    cairo_set_line_width(cr, 2.0);
    cairo_stroke(cr);
  }
}

// --- Gravity Probe -----------------------------------------------------------------

// A floating sphere in a gimballed ring, its core a little gravity well
// behind glass. Variant 1: the core flares before a shot. The ring turns
// with the frame. Its field is drawn live (world_draw.cpp).
void gravityProbe(cairo_t* cr, double w, double h, int variant, int frame)
{
  const double s = w / 128.0;
  const double cx = kM + w * 0.5, cy = kM + h * 0.46;
  const double rad = 34 * s;
  const bool hot = variant == 1;
  const Color core = hot ? rgb(255, 120, 255) : rgb(170, 90, 255);
  glow(cr, cx, cy, 70 * s, core, hot ? 0.55 : 0.25);
  // The ring's back half (behind the sphere).
  const double tilt = frame % 2 ? 0.32 : 0.22, rx = 56 * s, ry = 56 * s * tilt;
  const double rot = frame % 2 ? -0.18 : 0.12;
  auto ringArc = [&](double a0, double a1) {
    cairo_save(cr);
    cairo_translate(cr, cx, cy);
    cairo_rotate(cr, rot);
    cairo_scale(cr, rx, ry);
    cairo_new_sub_path(cr);
    cairo_arc(cr, 0, 0, 1, a0, a1);
    cairo_restore(cr);
    setColor(cr, kInk);
    cairo_set_line_width(cr, 9 * s);
    cairo_stroke_preserve(cr);
    setColor(cr, rgb(200, 208, 222));
    cairo_set_line_width(cr, 5 * s);
    cairo_stroke_preserve(cr);
    setColor(cr, kOrange);
    cairo_set_line_width(cr, 1.6 * s);
    cairo_stroke(cr);
  };
  ringArc(kPi, 2 * kPi);
  // Stabiliser fins top and bottom.
  for (int k = -1; k <= 1; k += 2)
  {
    cairo_move_to(cr, cx - 10 * s, cy + k * rad * 0.8);
    cairo_line_to(cr, cx, cy + k * (rad + 16 * s));
    cairo_line_to(cr, cx + 10 * s, cy + k * rad * 0.8);
    cairo_close_path(cr);
    fillLinear(cr, cx - 10 * s, 0, cx + 10 * s, 0, rgb(240, 244, 250), rgb(120, 130, 148), 2.0);
  }
  // The sphere: white plates, a dark equator.
  fillBall(cr, cx, cy, rad, rgb(255, 255, 255), rgb(140, 150, 170));
  cairo_save(cr);
  circle(cr, cx, cy, rad);
  cairo_clip(cr);
  cairo_rectangle(cr, cx - rad, cy + rad * 0.18, rad * 2, rad * 0.22);
  setColor(cr, rgb(60, 66, 86));
  cairo_fill(cr);
  for (int k = 0; k < 6; ++k)
  {
    circle(cr, cx - rad + 6 * s + k * 12 * s, cy + rad * 0.29, 1.6 * s);
    setColor(cr, (k + frame) % 3 == 0 ? kOrange : rgb(110, 230, 255));
    cairo_fill(cr);
  }
  cairo_restore(cr);
  circle(cr, cx, cy, rad);
  setColor(cr, kInk);
  cairo_set_line_width(cr, kLine);
  cairo_stroke(cr);
  // The core window: a swirl falling into a dark middle.
  const double kx = cx + 4 * s, ky = cy - 6 * s, kr = 15 * s;
  circle(cr, kx, ky, kr + 3 * s);
  fillLinear(cr, 0, ky - kr, 0, ky + kr, rgb(180, 188, 204), rgb(90, 98, 116), 2.0);
  cairo_pattern_t* cg = cairo_pattern_create_radial(kx, ky, 0, kx, ky, kr);
  cairo_pattern_add_color_stop_rgb(cg, 0, 0.05, 0.0, 0.1);
  cairo_pattern_add_color_stop_rgb(cg, 0.45, redOf(core) / 255.0 * 0.5, greenOf(core) / 255.0 * 0.4, blueOf(core) / 255.0 * 0.7);
  cairo_pattern_add_color_stop_rgb(cg, 1, redOf(core) / 255.0, greenOf(core) / 255.0, blueOf(core) / 255.0);
  circle(cr, kx, ky, kr);
  cairo_set_source(cr, cg);
  cairo_fill(cr);
  cairo_pattern_destroy(cg);
  for (int arm = 0; arm < 3; ++arm)
  {
    cairo_new_sub_path(cr);
    for (int i = 0; i <= 12; ++i)
    {
      const double a = arm * 2.094 + i * 0.3 + frame * 0.8, rr = kr * (1.0 - i / 13.0);
      if (i == 0)
        cairo_move_to(cr, kx + std::cos(a) * rr, ky + std::sin(a) * rr);
      else
        cairo_line_to(cr, kx + std::cos(a) * rr, ky + std::sin(a) * rr);
    }
    setRgba(cr, rgb(255, 230, 255), hot ? 0.95 : 0.6);
    cairo_set_line_width(cr, 1.6 * s);
    cairo_stroke(cr);
  }
  if (hot)
  {
    glow(cr, kx, ky, kr * 2.2, rgb(255, 200, 255), 0.8);
    circle(cr, kx, ky, kr * 0.35);
    setColor(cr, rgb(255, 255, 255));
    cairo_fill(cr);
  }
  // The sensor lens toward its facing.
  circle(cr, cx + rad * 0.78, cy + 8 * s, 7 * s);
  fillLinear(cr, 0, cy + s, 0, cy + 15 * s, rgb(70, 80, 100), rgb(20, 22, 30), 2.0);
  circle(cr, cx + rad * 0.8, cy + 7 * s, 3 * s);
  setColor(cr, hot ? rgb(255, 80, 80) : rgb(120, 230, 255));
  cairo_fill(cr);
  // Shine.
  ellipse(cr, cx - rad * 0.42, cy - rad * 0.5, rad * 0.24, rad * 0.12);
  setRgba(cr, rgb(255, 255, 255), 0.9);
  cairo_fill(cr);
  // The ring's front half.
  ringArc(0, kPi);
}

// --- Test Subject ------------------------------------------------------------------

// A volunteer in a white lab coat that is two sizes too big, safety goggles
// pushed up into wild hair and a numbered bib pinned on. State 0 walks
// with its arms out like a sleepwalker, 1 crouches (about to copy your jump),
// 2 flails while it falls. A carrier's coat is tinged green.
void testSubject(cairo_t* cr, double w, double h, int variant, int frame)
{
  const int state = variant % 4;
  const bool carrier = (variant / 4) % 2 == 1;
  const int number = variant / 8;
  const double s = w / 96.0;
  const double floorY = kM + h;
  const double drop = state == 1 ? 26 * s : 0.0; // crouching
  const bool flail = state == 2;
  const double cx = kM + w * 0.46;
  const Color skin = rgb(244, 200, 166), skinDark = rgb(200, 146, 112);
  const Color coatL = carrier ? rgb(214, 250, 196) : rgb(252, 253, 255);
  const Color coatD = carrier ? rgb(132, 190, 120) : rgb(184, 194, 210);
  const Color trousers = rgb(70, 80, 110);
  const double hipY = floorY - 54 * s + drop * 0.6;
  const double shoulderY = kM + 62 * s + drop;
  const double headY = kM + 36 * s + drop;
  if (carrier)
    glow(cr, cx, hipY - 30 * s, 70 * s, rgb(120, 255, 90), 0.25);
  // Legs (the far one first).
  for (int k = 0; k < 2; ++k)
  {
    const double hx = cx + (k == 0 ? -8 : 8) * s;
    double kx = hx, ky = hipY + 26 * s, fx = hx, fy = floorY - 4 * s;
    if (state == 1)
    {
      kx = hx + 16 * s;
      ky = hipY + 14 * s;
      fx = hx + 2 * s;
    }
    else if (flail)
    {
      kx = hx + (k == 0 ? -16 : 16) * s;
      ky = hipY + 16 * s;
      fx = hx + (k == 0 ? -22 : 22) * s;
      fy = hipY + (k == frame % 2 ? 40 : 30) * s;
    }
    else
    {
      const double step = ((k + frame) % 2 ? 1.0 : -1.0) * 9 * s;
      kx = hx + step * 0.6 + 3 * s;
      fx = hx + step;
      fy = floorY - 4 * s - (step > 0 ? 3 * s : 0.0);
    }
    limb(cr, hx, hipY, kx, ky, fx, fy, 10 * s, k == 0 ? lerpColor(trousers, rgb(0, 0, 0), 0.2f) : trousers);
    // Shoe: a scuffed orange lab clog.
    ellipse(cr, fx + 5 * s, fy + 1 * s, 10 * s, 5 * s);
    fillLinear(cr, 0, fy - 4 * s, 0, fy + 6 * s, rgb(255, 170, 90), rgb(210, 90, 30), 2.0);
  }
  // The far arm.
  auto arm = [&](int side) {
    const double sx = cx + side * 10 * s, sy = shoulderY + 8 * s;
    double ex = sx + 14 * s, ey = sy + 8 * s, hx = sx + 30 * s, hy = sy + 4 * s; // out in front
    if (state == 1)
    {
      ex = sx + 8 * s;
      ey = sy + 18 * s;
      hx = sx - 4 * s;
      hy = sy + 28 * s; // swung back, ready to spring
    }
    else if (flail)
    {
      ex = sx + side * 10 * s;
      ey = sy - 16 * s;
      hx = sx + side * 18 * s + (frame % 2 ? 4 : -4) * s;
      hy = sy - 34 * s;
    }
    else
    {
      hy += ((frame + (side > 0 ? 1 : 0)) % 2 ? 3 : -2) * s;
    }
    limb(cr, sx, sy, ex, ey, hx, hy, 9 * s, side < 0 ? coatD : coatL);
    circle(cr, hx, hy, 5 * s);
    fillLinear(cr, 0, hy - 5 * s, 0, hy + 5 * s, skin, skinDark, 1.8);
  };
  arm(-1);
  // The coat: flared, hanging to the knees, open at the front.
  const double hem = hipY + 22 * s - drop * 0.2;
  cairo_move_to(cr, cx - 15 * s, shoulderY);
  cairo_curve_to(cr, cx - 20 * s, shoulderY + 30 * s, cx - 24 * s, hem - 20 * s, cx - 26 * s, hem);
  cairo_line_to(cr, cx + 24 * s, hem);
  cairo_curve_to(cr, cx + 22 * s, hem - 20 * s, cx + 20 * s, shoulderY + 30 * s, cx + 15 * s, shoulderY);
  cairo_close_path(cr);
  fillLinear(cr, cx - 26 * s, 0, cx + 24 * s, 0, coatD, coatL);
  // The shirt in the opening and the coat's lapels.
  cairo_move_to(cr, cx + 4 * s, shoulderY + 2 * s);
  cairo_line_to(cr, cx + 13 * s, shoulderY + 2 * s);
  cairo_line_to(cr, cx + 10 * s, hem);
  cairo_line_to(cr, cx + 6 * s, hem);
  cairo_close_path(cr);
  fillLinear(cr, 0, shoulderY, 0, hem, rgb(150, 200, 236), rgb(90, 140, 190), 1.6);
  // The bib: a white card with the number, pinned at its corners.
  const double bx = cx - 12 * s, by = shoulderY + 14 * s, bw = 22 * s, bh = 20 * s;
  roundedRect(cr, bx, by, bw, bh, 2 * s);
  fillLinear(cr, 0, by, 0, by + bh, rgb(255, 255, 255), rgb(226, 230, 238), 1.8);
  cairo_rectangle(cr, bx, by, bw, 4 * s);
  setColor(cr, kOrange);
  cairo_fill(cr);
  static const char* kNumbers[4] = {"8", "08", "80", "88"}; // they read the same mirrored (facing left)
  selectGameFont(cr);
  cairo_set_font_size(cr, 12 * s);
  cairo_text_extents_t ext;
  const char* num = kNumbers[std::clamp(number, 0, 3)];
  cairo_text_extents(cr, num, &ext);
  cairo_move_to(cr, bx + bw * 0.5 - ext.width * 0.5 - ext.x_bearing, by + bh - 3 * s);
  setColor(cr, kInk);
  cairo_show_text(cr, num);
  for (int k = 0; k < 2; ++k)
  {
    circle(cr, bx + 2.5 * s + k * (bw - 5 * s), by + 2 * s, 1.4 * s);
    setColor(cr, rgb(200, 206, 220));
    cairo_fill(cr);
  }
  // A pen in the breast pocket.
  cairo_rectangle(cr, cx + 15 * s, shoulderY + 10 * s, 2.5 * s, 10 * s);
  setColor(cr, rgb(40, 90, 220));
  cairo_fill(cr);
  // Neck and head.
  cairo_rectangle(cr, cx - 4 * s, headY + 12 * s, 9 * s, shoulderY - headY - 10 * s);
  setColor(cr, skinDark);
  cairo_fill(cr);
  const double hx = cx + 2 * s;
  // Wild hair behind the head.
  for (int k = 0; k < 7; ++k)
  {
    const double a = kPi * (1.05 + k * 0.14);
    circle(cr, hx - 2 * s + std::cos(a) * 15 * s, headY + std::sin(a) * 14 * s, 7 * s);
  }
  setColor(cr, rgb(120, 70, 40));
  cairo_fill_preserve(cr);
  setColor(cr, kInk);
  cairo_set_line_width(cr, 2.0);
  cairo_stroke(cr);
  ellipse(cr, hx, headY + 2 * s, 15 * s, 16 * s);
  fillLinear(cr, 0, headY - 14 * s, 0, headY + 18 * s, skin, skinDark);
  // A tuft on top that never lies flat.
  cairo_move_to(cr, hx - 10 * s, headY - 10 * s);
  cairo_curve_to(cr, hx - 6 * s, headY - 24 * s, hx + 4 * s, headY - 22 * s, hx + 2 * s, headY - 30 * s);
  cairo_curve_to(cr, hx + 8 * s, headY - 22 * s, hx + 12 * s, headY - 18 * s, hx + 12 * s, headY - 10 * s);
  cairo_close_path(cr);
  fillLinear(cr, 0, headY - 30 * s, 0, headY - 8 * s, rgb(160, 100, 60), rgb(110, 62, 34), 2.0);
  // Goggles: pushed up while it walks, down over the eyes when it means it.
  const bool down = state == 1;
  const double gy = down ? headY + 1 * s : headY - 9 * s;
  cairo_rectangle(cr, hx - 15 * s, gy - 2 * s, 30 * s, 4 * s);
  setColor(cr, rgb(50, 54, 66));
  cairo_fill(cr);
  for (int k = 0; k < 2; ++k)
  {
    const double gx = hx + 1 * s + k * 10 * s;
    ellipse(cr, gx, gy, 6 * s, 5 * s);
    fillLinear(cr, 0, gy - 5 * s, 0, gy + 5 * s, rgb(255, 196, 120), kOrange, 1.8);
    ellipse(cr, gx - 1.5 * s, gy - 1.5 * s, 2 * s, 1.4 * s);
    setRgba(cr, rgb(255, 255, 255), 0.85);
    cairo_fill(cr);
  }
  if (!down)
  {
    // Eyes: wide and blank, fixed on what you do next.
    for (int k = 0; k < 2; ++k)
    {
      const double ex = hx + 3 * s + k * 9 * s, ey = headY + 2 * s;
      circle(cr, ex, ey, 3.6 * s);
      setColor(cr, rgb(255, 255, 255));
      cairo_fill_preserve(cr);
      setColor(cr, kInk);
      cairo_set_line_width(cr, 1.4);
      cairo_stroke(cr);
      circle(cr, ex + (flail ? 0 : 1.2 * s), ey + (flail ? -1.5 * s : 0), 1.5 * s);
      setColor(cr, kInk);
      cairo_fill(cr);
    }
  }
  // Mouth: a flat line, an O while it falls.
  if (flail)
  {
    ellipse(cr, hx + 8 * s, headY + 11 * s, 3 * s, 4 * s);
    setColor(cr, rgb(90, 30, 40));
    cairo_fill(cr);
  }
  else
  {
    cairo_move_to(cr, hx + 4 * s, headY + 10 * s);
    cairo_line_to(cr, hx + 12 * s, headY + 10 * s);
    setColor(cr, kInk);
    cairo_set_line_width(cr, 1.6 * s);
    cairo_stroke(cr);
  }
  // The near arm.
  arm(1);
  if (carrier)
  {
    // A few green motes rising off the coat.
    for (int k = 0; k < 3; ++k)
    {
      circle(cr, cx - 18 * s + k * 16 * s, hipY - 10 * s - ((k + frame) % 3) * 10 * s, 2.4 * s);
      setRgba(cr, rgb(140, 255, 110), 0.8);
      cairo_fill(cr);
    }
  }
  if (flail)
  {
    for (int k = 0; k < 3; ++k)
    {
      const double lx = cx - 20 * s + k * 20 * s;
      cairo_move_to(cr, lx, kM - 8 + (frame % 2) * 4);
      cairo_line_to(cr, lx, kM - 24 + (frame % 2) * 4);
    }
    setRgba(cr, rgb(255, 255, 255), 0.8);
    cairo_set_line_width(cr, 2.0);
    cairo_stroke(cr);
  }
}

} // namespace

bool drawGravArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame)
{
  (void)t;
  if (key == "flip_walker")
    flipWalker(cr, w, h, variant, frame);
  else if (key == "gravity_probe")
    gravityProbe(cr, w, h, variant, frame);
  else if (key == "test_subject")
    testSubject(cr, w, h, variant, frame);
  else
    return false;
  return true;
}

} // namespace gr
