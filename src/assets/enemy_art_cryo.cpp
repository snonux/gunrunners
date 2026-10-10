// The Cryo Labs' staff (Level 16) in the style of enemy_art.cpp: each drawn
// in screen pixels into a texture with a 32 px margin, the cell box at
// (32, 32) .. (32 + w, 32 + h), facing right.

#include "assets/enemy_art_cryo.hpp"

#include <cmath>

namespace gr
{

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr Color kInk = rgb(14, 16, 28);
constexpr double kLine = 2.4;
constexpr double kM = 32.0;

void setColor(cairo_t* cr, Color c) { cairo_set_source_rgb(cr, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0); }

void setRgba(cairo_t* cr, Color c, double a)
{
  cairo_set_source_rgba(cr, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, a);
}

void circle(cairo_t* cr, double x, double y, double r) { cairo_arc(cr, x, y, r, 0, 2 * kPi); }

void rounded(cairo_t* cr, double x, double y, double w, double h, double r)
{
  cairo_new_sub_path(cr);
  cairo_arc(cr, x + w - r, y + r, r, -kPi / 2, 0);
  cairo_arc(cr, x + w - r, y + h - r, r, 0, kPi / 2);
  cairo_arc(cr, x + r, y + h - r, r, kPi / 2, kPi);
  cairo_arc(cr, x + r, y + r, r, kPi, 1.5 * kPi);
  cairo_close_path(cr);
}

void fillShaded(cairo_t* cr, double y0, double y1, Color top, Color bottom)
{
  cairo_pattern_t* g = cairo_pattern_create_linear(0, y0, 0, y1);
  cairo_pattern_add_color_stop_rgb(g, 0, redOf(top) / 255.0, greenOf(top) / 255.0, blueOf(top) / 255.0);
  cairo_pattern_add_color_stop_rgb(g, 1, redOf(bottom) / 255.0, greenOf(bottom) / 255.0, blueOf(bottom) / 255.0);
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  setColor(cr, kInk);
  cairo_set_line_width(cr, kLine);
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

// Puck Drone: a flat black-and-steel disc on an air cushion, a ring of
// lights round its rim. Variant 1: spinning up (lights white, blur);
// 2: sliding (a skirt of frost spray).
void puckDrone(cairo_t* cr, double w, double h, int variant, int frame)
{
  const double cx = kM + w * 0.5, cy = kM + h * 0.6;
  // Air cushion.
  cairo_save(cr);
  cairo_translate(cr, cx, kM + h - 6);
  cairo_scale(cr, 1.0, 0.18);
  circle(cr, 0, 0, w * 0.46);
  cairo_restore(cr);
  setRgba(cr, rgb(170, 220, 255), 0.45);
  cairo_fill(cr);
  // Body: an ellipse with a raised cap.
  cairo_save(cr);
  cairo_translate(cr, cx, cy);
  cairo_scale(cr, 1.0, 0.42);
  circle(cr, 0, 0, w * 0.46);
  cairo_restore(cr);
  fillShaded(cr, cy - h * 0.25, cy + h * 0.25, rgb(70, 76, 92), rgb(20, 22, 30));
  cairo_save(cr);
  cairo_translate(cr, cx, cy - h * 0.16);
  cairo_scale(cr, 1.0, 0.36);
  circle(cr, 0, 0, w * 0.3);
  cairo_restore(cr);
  fillShaded(cr, cy - h * 0.3, cy, rgb(200, 210, 228), rgb(120, 130, 150));
  // Rim lights.
  const Color lamp = variant == 1 ? rgb(255, 255, 255) : (variant == 3 ? rgb(255, 70, 60) : rgb(80, 220, 255));
  for (int k = 0; k < 7; ++k)
  {
    const double a = kPi * (0.1 + 0.8 * k / 6.0) + (variant == 1 && frame ? 0.2 : 0.0);
    const double lx = cx - std::cos(a) * w * 0.42, ly = cy + std::sin(a) * h * 0.14;
    glow(cr, lx, ly, variant == 1 ? 8 : 5, lamp, 0.95);
  }
  if (variant == 1)
  {
    // The whine: motion arcs.
    for (int k = 0; k < 3; ++k)
    {
      cairo_arc(cr, cx, cy - h * 0.1, w * (0.5 + 0.06 * k), kPi * (1.1 + 0.1 * frame), kPi * (1.4 + 0.1 * frame));
      setRgba(cr, rgb(220, 240, 255), 0.6 - 0.15 * k);
      cairo_set_line_width(cr, 2);
      cairo_stroke(cr);
    }
  }
  if (variant == 3)
  {
    // The goalie's cage mask on its cap.
    for (int k = -2; k <= 2; ++k)
    {
      cairo_move_to(cr, cx + k * 9.0, cy - h * 0.42);
      cairo_line_to(cr, cx + k * 9.0, cy - h * 0.05);
    }
    cairo_move_to(cr, cx - 22, cy - h * 0.3);
    cairo_line_to(cr, cx + 22, cy - h * 0.3);
    setColor(cr, rgb(255, 210, 60));
    cairo_set_line_width(cr, 3);
    cairo_stroke(cr);
  }
  if (variant == 2)
    for (int k = 0; k < 6; ++k)
    {
      circle(cr, kM + 4 + k * 5.0, kM + h - 6 - (k % 3) * 4.0, 3);
      setRgba(cr, rgb(240, 250, 255), 0.7);
      cairo_fill(cr);
    }
}

// Sleeper Pod: a capsule in its alcove, frost on the glass. Variant 0: frosted
// shut, a shape inside; 1: the frost gone, the mutant awake; 2: open and
// empty. +3: a carrier's pod (green frost, green lights).
void sleeperPod(cairo_t* cr, double w, double h, int variant, int frame)
{
  const bool green = variant >= 3;
  const int state = variant % 3;
  const Color frostCol = green ? rgb(170, 255, 150) : rgb(220, 240, 255);
  // Alcove and shell.
  cairo_rectangle(cr, kM, kM, w, h);
  setColor(cr, rgb(30, 36, 48));
  cairo_fill(cr);
  rounded(cr, kM + 6, kM + 4, w - 12, h - 8, w * 0.3);
  fillShaded(cr, kM, kM + h, rgb(190, 200, 214), rgb(100, 110, 128));
  // Glass.
  rounded(cr, kM + 18, kM + 18, w - 36, h - 50, w * 0.22);
  if (state == 2)
  {
    setColor(cr, rgb(12, 16, 24));
    cairo_fill(cr);
    // The door swung open.
    rounded(cr, kM + w - 22, kM + 18, 16, h - 50, 6);
    setRgba(cr, frostCol, 0.6);
    cairo_fill(cr);
  }
  else
  {
    setColor(cr, rgb(40, 64, 90));
    cairo_fill(cr);
    // The sleeper: head and shoulders.
    const double bx = kM + w * 0.5, by = kM + h * 0.35;
    circle(cr, bx, by, w * 0.13);
    setColor(cr, state == 1 ? rgb(150, 190, 140) : rgb(110, 140, 160));
    cairo_fill(cr);
    rounded(cr, bx - w * 0.22, by + w * 0.14, w * 0.44, h * 0.35, 8);
    cairo_fill(cr);
    if (state == 1)
    {
      circle(cr, bx - 6, by - 2, 3);
      circle(cr, bx + 6, by - 2, 3);
      setColor(cr, rgb(255, 230, 80));
      cairo_fill(cr);
    }
    if (state == 0)
    {
      rounded(cr, kM + 18, kM + 18, w - 36, h - 50, w * 0.22);
      setRgba(cr, frostCol, 0.8);
      cairo_fill(cr);
      for (int k = 0; k < 6; ++k)
      {
        const double fx = kM + 24 + (k * 37) % int(w - 48), fy = kM + 30 + (k * 53) % int(h - 80);
        cairo_move_to(cr, fx, fy);
        cairo_line_to(cr, fx + 12, fy - 10);
        cairo_move_to(cr, fx, fy);
        cairo_line_to(cr, fx - 9, fy - 11);
        setRgba(cr, rgb(255, 255, 255), 0.8);
        cairo_set_line_width(cr, 2);
        cairo_stroke(cr);
      }
    }
  }
  // Status lights.
  for (int k = 0; k < 3; ++k)
    glow(cr, kM + 30 + k * 18.0, kM + h - 18, 6,
      state == 0 ? (green ? rgb(120, 255, 80) : rgb(80, 220, 255)) : ((frame + k) % 2 ? rgb(255, 60, 40) : rgb(120, 20, 20)),
      0.95);
}

// Pod Mutant: a hunched, frost-bitten lab escapee in a torn smock. Variant
// 1: arms up before a lunge; 2: lunging forward.
void podMutant(cairo_t* cr, double w, double h, int variant, int frame)
{
  const double cx = kM + w * 0.5, base = kM + h;
  const double lean = variant == 2 ? w * 0.18 : 0.0;
  // Legs.
  for (int k = 0; k < 2; ++k)
  {
    const double lx = cx - w * 0.22 + k * w * 0.26 + (frame && k == 0 ? 3 : 0);
    rounded(cr, lx, base - h * 0.32, w * 0.18, h * 0.32, 4);
    fillShaded(cr, base - h * 0.32, base, rgb(110, 130, 120), rgb(60, 74, 70));
  }
  // Body in a smock.
  cairo_move_to(cr, cx - w * 0.34 + lean, base - h * 0.72);
  cairo_line_to(cr, cx + w * 0.3 + lean, base - h * 0.74);
  cairo_line_to(cr, cx + w * 0.36, base - h * 0.28);
  cairo_line_to(cr, cx - w * 0.38, base - h * 0.28);
  cairo_close_path(cr);
  fillShaded(cr, base - h * 0.74, base - h * 0.28, rgb(220, 232, 236), rgb(150, 170, 176));
  // Head with frost on it.
  const double hx = cx + w * 0.08 + lean, hy = base - h * 0.82;
  circle(cr, hx, hy, w * 0.22);
  fillShaded(cr, hy - w * 0.22, hy + w * 0.22, rgb(170, 200, 170), rgb(100, 130, 110));
  cairo_arc(cr, hx, hy, w * 0.22, kPi * 1.1, kPi * 1.9);
  setRgba(cr, rgb(240, 250, 255), 0.9);
  cairo_set_line_width(cr, 4);
  cairo_stroke(cr);
  circle(cr, hx + w * 0.08, hy - 2, 3.5);
  setColor(cr, rgb(255, 220, 60));
  cairo_fill(cr);
  // Arms: down and dragging, up (tell) or out (lunge).
  const double sy = base - h * 0.66;
  for (int k = 0; k < 2; ++k)
  {
    const double sx = cx + (k ? w * 0.22 : -w * 0.26) + lean;
    double ex = sx + w * 0.16, ey = sy + h * 0.3;
    if (variant == 1)
    {
      ex = sx + (k ? w * 0.1 : -w * 0.06);
      ey = sy - h * 0.26;
    }
    else if (variant == 2)
    {
      ex = sx + w * 0.55;
      ey = sy + h * 0.02;
    }
    cairo_move_to(cr, sx, sy);
    cairo_line_to(cr, ex, ey);
    setColor(cr, kInk);
    cairo_set_line_width(cr, 11);
    cairo_stroke(cr);
    cairo_move_to(cr, sx, sy);
    cairo_line_to(cr, ex, ey);
    setColor(cr, rgb(150, 180, 150));
    cairo_set_line_width(cr, 7);
    cairo_stroke(cr);
    circle(cr, ex, ey, 6);
    setColor(cr, rgb(170, 200, 170));
    cairo_fill(cr);
  }
}

// Lab Arm: a claw on a telescoping wrist under its ceiling trolley. Variant
// 0: riding the rail; 1: the lamp red, claws open; 2: dropping, claws wide;
// 3: claws shut (holding).
void labArm(cairo_t* cr, double w, double h, int variant, int frame)
{
  (void)frame;
  const double cx = kM + w * 0.5;
  // The wrist housing.
  rounded(cr, cx - w * 0.28, kM, w * 0.56, h * 0.36, 8);
  fillShaded(cr, kM, kM + h * 0.36, rgb(240, 244, 250), rgb(160, 170, 186));
  cairo_rectangle(cr, cx - w * 0.28, kM + h * 0.24, w * 0.56, 6);
  setColor(cr, rgb(255, 196, 30));
  cairo_fill(cr);
  glow(cr, cx, kM + h * 0.12, variant == 1 ? 26 : 10, variant == 1 ? rgb(255, 40, 40) : rgb(90, 220, 255), 0.95);
  // Three fingers.
  const double open = variant == 3 ? 0.05 : (variant == 0 ? 0.25 : (variant == 1 ? 0.5 : 0.7));
  for (int k = -1; k <= 1; ++k)
  {
    const double sx = cx + k * w * 0.14, sy = kM + h * 0.36;
    const double mx = sx + k * w * open * 0.5, my = sy + h * 0.3;
    const double ex = mx - k * w * 0.12 * (1.0 - open), ey = kM + h - 2;
    cairo_move_to(cr, sx, sy);
    cairo_line_to(cr, mx, my);
    cairo_line_to(cr, ex, ey);
    setColor(cr, kInk);
    cairo_set_line_width(cr, 13);
    cairo_stroke(cr);
    cairo_move_to(cr, sx, sy);
    cairo_line_to(cr, mx, my);
    cairo_line_to(cr, ex, ey);
    setColor(cr, rgb(130, 140, 160));
    cairo_set_line_width(cr, 8);
    cairo_stroke(cr);
    circle(cr, mx, my, 6);
    setColor(cr, rgb(210, 216, 228));
    cairo_fill(cr);
  }
}

} // namespace

bool drawCryoArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame)
{
  (void)t;
  if (key == "puck_drone")
    puckDrone(cr, w, h, variant, frame);
  else if (key == "sleeper_pod")
    sleeperPod(cr, w, h, variant, frame);
  else if (key == "pod_mutant")
    podMutant(cr, w, h, variant, frame);
  else if (key == "lab_arm")
    labArm(cr, w, h, variant, frame);
  else
    return false;
  return true;
}

} // namespace gr
