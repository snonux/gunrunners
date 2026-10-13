// The Prop Warehouse (Level 36) in the style of enemy_art.cpp: smooth shapes
// with a dark ink line, lit warm from the work lights above, cool in the
// shadows. The Forklift Bot is a squat yellow robot forklift with a screen
// for a face, the Stagehand a figure in stage blacks of whom only the white
// gloves and toe caps show, the Prop Mimic a prop chest that is not one,
// the bonus's practice dummy a straw-stuffed sack on stubby legs. The duck
// wears a hard hat and a hi-vis vest. Each is drawn in screen pixels into a
// texture with a 32 px margin, the cell box at (32, 32) .. (32 + w, 32 + h),
// facing right.

#include "assets/enemy_art_warehouse36.hpp"

#include "base/math.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>

namespace gr
{

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr double kM = 32.0;
constexpr Color kInk = rgb(18, 14, 20);
constexpr Color kYellow = rgb(246, 192, 36);
constexpr Color kYellowLo = rgb(170, 110, 20);
constexpr Color kSteel = rgb(150, 156, 168);
constexpr Color kSteelLo = rgb(64, 68, 80);
constexpr Color kBlack = rgb(20, 18, 24);
constexpr Color kBlackRim = rgb(62, 58, 74);
constexpr Color kGlove = rgb(250, 250, 252);
constexpr Color kWood = rgb(156, 108, 62);
constexpr Color kWoodLo = rgb(92, 58, 32);
constexpr Color kIron = rgb(58, 56, 62);

Color lighten(Color c, float t) { return lerpColor(c, rgb(255, 255, 255), t); }
Color darken(Color c, float t) { return lerpColor(c, rgb(0, 0, 0), t); }

void setRgba(cairo_t* cr, Color c, double a)
{
  cairo_set_source_rgba(cr, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, a);
}

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
  cairo_scale(cr, std::max(0.01, rx), std::max(0.01, ry));
  cairo_new_sub_path(cr);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
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

void vgrad(cairo_t* cr, double y0, double y1, std::initializer_list<std::pair<double, Color>> stops, double a = 1.0,
  bool preserve = false)
{
  cairo_pattern_t* g = cairo_pattern_create_linear(0, y0, 0, y1);
  for (const auto& s : stops)
    stop(g, s.first, s.second, a);
  cairo_set_source(cr, g);
  if (preserve)
    cairo_fill_preserve(cr);
  else
    cairo_fill(cr);
  cairo_pattern_destroy(g);
}

void hgrad(cairo_t* cr, double x0, double x1, std::initializer_list<std::pair<double, Color>> stops, double a = 1.0,
  bool preserve = false)
{
  cairo_pattern_t* g = cairo_pattern_create_linear(x0, 0, x1, 0);
  for (const auto& s : stops)
    stop(g, s.first, s.second, a);
  cairo_set_source(cr, g);
  if (preserve)
    cairo_fill_preserve(cr);
  else
    cairo_fill(cr);
  cairo_pattern_destroy(g);
}

void stroke(cairo_t* cr, Color c, double w, double a = 1.0, bool preserve = false)
{
  setRgba(cr, c, a);
  cairo_set_line_width(cr, w);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
  if (preserve)
    cairo_stroke_preserve(cr);
  else
    cairo_stroke(cr);
}

void glow(cairo_t* cr, double x, double y, double rad, Color c, double a)
{
  cairo_pattern_t* g = cairo_pattern_create_radial(x, y, 0, x, y, rad);
  stop(g, 0.0, c, a);
  stop(g, 0.5, c, a * 0.35);
  stop(g, 1.0, c, 0.0);
  circle(cr, x, y, rad);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
}

void frameBox(cairo_t* cr, double w, double h, double dw, double dh)
{
  cairo_translate(cr, kM, kM);
  cairo_scale(cr, w / dw, h / dh);
}

// A limb: an ink outline under a coloured stroke through the points.
void limb(cairo_t* cr, std::initializer_list<std::array<double, 2>> pts, double width, Color c, Color outline = kInk)
{
  for (int pass = 0; pass < 2; ++pass)
  {
    bool first = true;
    for (const auto& p : pts)
    {
      if (first)
        cairo_move_to(cr, p[0], p[1]);
      else
        cairo_line_to(cr, p[0], p[1]);
      first = false;
    }
    stroke(cr, pass == 0 ? outline : c, pass == 0 ? width + 3.0 : width);
  }
}

// --- Forklift Bot -----------------------------------------------------------------------------------

void forkWheel(cairo_t* cr, double x, double y, double r, int frame)
{
  circle(cr, x, y, r);
  setRgba(cr, rgb(28, 26, 30), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 2.0);
  circle(cr, x, y, r * 0.55);
  vgrad(cr, y - r * 0.55, y + r * 0.55, {{0.0, rgb(250, 220, 90)}, {1.0, kYellowLo}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  for (int k = 0; k < 3; ++k)
  {
    const double a = k * 2.0 * kPi / 3.0 + frame * kPi / 3.0;
    cairo_move_to(cr, x, y);
    cairo_line_to(cr, x + std::cos(a) * r * 0.5, y + std::sin(a) * r * 0.5);
  }
  stroke(cr, kInk, 1.6);
  circle(cr, x, y, 2.5);
  setRgba(cr, kSteelLo, 1.0);
  cairo_fill(cr);
  // Tread blocks.
  for (int k = 0; k < 10; ++k)
  {
    const double a = k * kPi / 5.0 + frame * kPi / 10.0;
    cairo_move_to(cr, x + std::cos(a) * (r - 1), y + std::sin(a) * (r - 1));
    cairo_line_to(cr, x + std::cos(a) * (r - 4), y + std::sin(a) * (r - 4));
  }
  stroke(cr, rgb(70, 66, 72), 1.4);
}

// variant: 0 patrol, 1 tell, 2 charge, 3 carrying, 4 hanging on a magnet.
void forkliftBot(cairo_t* cr, double w, double h, int variant, int frame)
{
  cairo_save(cr);
  frameBox(cr, w, h, 192, 128);
  const double drop = variant == 1 ? 5.0 : (variant == 4 ? -8.0 : 0.0);
  // The wheels (hanging lower off a magnet).
  const double wy = variant == 4 ? 112.0 : 110.0;
  // The mast at its nose: two channel rails, a chain between.
  for (const double x : {154.0, 168.0})
  {
    cairo_rectangle(cr, x, 2 + drop, 10, 124 - drop);
    hgrad(cr, x, x + 10, {{0.0, rgb(120, 124, 136)}, {0.5, rgb(84, 88, 100)}, {1.0, rgb(40, 42, 52)}}, 1.0, true);
    stroke(cr, kInk, 1.6);
  }
  for (double y = 8 + drop; y < 120; y += 6)
  {
    cairo_arc(cr, 166, y, 1.6, 0, 2 * kPi);
    setRgba(cr, rgb(30, 30, 36), 1.0);
    cairo_fill(cr);
  }
  cairo_rectangle(cr, 150, 2 + drop, 32, 6);
  setRgba(cr, rgb(50, 52, 62), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.4);
  cairo_save(cr);
  if (variant == 2)
  {
    // Leaning back as it digs in: pivot round the rear wheel.
    cairo_translate(cr, 40, wy);
    cairo_rotate(cr, -0.06);
    cairo_translate(cr, -40, -wy);
  }
  cairo_translate(cr, 0, drop);
  // The overhead guard: four bars and a roof.
  for (const double x : {54.0, 126.0})
  {
    cairo_move_to(cr, x, 64);
    cairo_line_to(cr, x + (x < 100 ? 4 : -2), 12);
    stroke(cr, kInk, 7.0);
    cairo_move_to(cr, x, 64);
    cairo_line_to(cr, x + (x < 100 ? 4 : -2), 12);
    stroke(cr, rgb(70, 72, 84), 4.0);
  }
  roundRect(cr, 50, 6, 84, 10, 4);
  vgrad(cr, 6, 16, {{0.0, rgb(130, 134, 146)}, {1.0, rgb(46, 48, 58)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  // The amber beacon on the roof.
  cairo_move_to(cr, 82, 6);
  cairo_curve_to(cr, 82, -4, 98, -4, 98, 6);
  cairo_close_path(cr);
  setRgba(cr, variant == 1 || variant == 2 ? rgb(255, 160, 40) : rgb(200, 120, 30), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.2);
  if (variant == 1 || variant == 2)
    glow(cr, 90, 1, 18, rgb(255, 170, 60), 0.7);
  // The robot's head: a screen in a yellow housing.
  roundRect(cr, 66, 22, 50, 40, 8);
  vgrad(cr, 22, 62, {{0.0, lighten(kYellow, 0.3f)}, {1.0, kYellowLo}}, 1.0, true);
  stroke(cr, kInk, 1.8);
  roundRect(cr, 72, 27, 38, 28, 5);
  setRgba(cr, rgb(16, 22, 30), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.2);
  const Color eye = variant == 1 ? rgb(255, 60, 40) : (variant == 2 ? rgb(255, 240, 160) : rgb(110, 230, 255));
  glow(cr, 91, 41, 18, eye, 0.35);
  for (const double ex : {83.0, 101.0})
  {
    if (variant == 1)
    {
      // Narrowed: two angry slits.
      cairo_move_to(cr, ex - 6, 37 + (ex < 90 ? -1 : 2));
      cairo_line_to(cr, ex + 5, 37 + (ex < 90 ? 2 : -1));
      stroke(cr, eye, 3.0);
    }
    else if (variant == 3)
    {
      // Pleased: little upturned arcs.
      cairo_arc(cr, ex, 44, 5, kPi * 1.1, kPi * 1.9);
      stroke(cr, eye, 2.6);
    }
    else
    {
      const double r = variant == 2 ? 5.5 : 4.0;
      roundRect(cr, ex - r * 0.8, 41 - r, r * 1.6, r * 2, r * 0.7);
      setRgba(cr, eye, 1.0);
      cairo_fill(cr);
    }
  }
  // The chassis, its counterweight at the back with hazard chevrons.
  roundRect(cr, 8, 60, 144, 50, 10);
  vgrad(cr, 60, 110, {{0.0, lighten(kYellow, 0.35f)}, {0.25, kYellow}, {0.8, darken(kYellow, 0.15f)}, {1.0, kYellowLo}}, 1.0, true);
  stroke(cr, kInk, 2.2);
  cairo_save(cr);
  roundRect(cr, 8, 66, 34, 36, 6);
  cairo_clip_preserve(cr);
  setRgba(cr, rgb(28, 26, 30), 1.0);
  cairo_fill(cr);
  for (double k = -40; k < 60; k += 14)
  {
    cairo_move_to(cr, 8, 66 + k);
    cairo_line_to(cr, 25, 84 + k);
    cairo_line_to(cr, 42, 66 + k);
    cairo_line_to(cr, 42, 73 + k);
    cairo_line_to(cr, 25, 91 + k);
    cairo_line_to(cr, 8, 73 + k);
    cairo_close_path(cr);
  }
  setRgba(cr, kYellow, 1.0);
  cairo_fill(cr);
  cairo_restore(cr);
  roundRect(cr, 8, 66, 34, 36, 6);
  stroke(cr, kInk, 1.4);
  // A vent. (Its fleet number, FB-36, is stencilled beside it by the
  // world, so it reads the right way round whichever way it faces.)
  for (int k = 0; k < 3; ++k)
  {
    cairo_move_to(cr, 52, 72 + k * 6);
    cairo_line_to(cr, 72, 72 + k * 6);
  }
  stroke(cr, rgb(110, 70, 10), 2.0, 0.8);
  // The exhaust stack at the back, smoking on the tell.
  cairo_rectangle(cr, 14, 42, 8, 20);
  hgrad(cr, 14, 22, {{0.0, rgb(140, 144, 150)}, {1.0, rgb(50, 52, 60)}}, 1.0, true);
  stroke(cr, kInk, 1.2);
  if (variant == 1 || variant == 2)
    for (int k = 0; k < 3; ++k)
    {
      const double py = 34 - k * 10 - (frame % 2) * 4, px = 16 - k * 6;
      circle(cr, px, py, 5 + k * 2.5);
      setRgba(cr, rgb(90, 90, 96), 0.6 - k * 0.15);
      cairo_fill(cr);
    }
  // The bumper at the back.
  roundRect(cr, 2, 96, 12, 10, 3);
  setRgba(cr, rgb(40, 40, 46), 1.0);
  cairo_fill(cr);
  cairo_restore(cr);
  // The wheels in their arches.
  forkWheel(cr, 42, wy, 17, frame);
  forkWheel(cr, 134, wy, 15, frame);
  cairo_restore(cr);
}

// --- Stagehand --------------------------------------------------------------------------------------

void glove(cairo_t* cr, double x, double y, double s, bool open)
{
  if (open)
  {
    // A flat hand with spread fingers.
    for (int k = 0; k < 4; ++k)
    {
      const double a = -kPi * 0.6 + k * kPi * 0.2;
      cairo_move_to(cr, x, y);
      cairo_line_to(cr, x + std::cos(a) * 9 * s, y + std::sin(a) * 9 * s);
    }
    stroke(cr, kInk, 5.6 * s);
    for (int k = 0; k < 4; ++k)
    {
      const double a = -kPi * 0.6 + k * kPi * 0.2;
      cairo_move_to(cr, x, y);
      cairo_line_to(cr, x + std::cos(a) * 9 * s, y + std::sin(a) * 9 * s);
    }
    stroke(cr, kGlove, 3.4 * s);
  }
  circle(cr, x, y, 6.5 * s);
  setRgba(cr, kGlove, 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.4);
  cairo_move_to(cr, x - 4 * s, y + 5 * s);
  cairo_line_to(cr, x + 4 * s, y + 5 * s);
  stroke(cr, rgb(190, 190, 200), 1.6 * s);
}

void blackShoe(cairo_t* cr, double x, double y)
{
  roundRect(cr, x - 9, y - 8, 20, 8, 3);
  setRgba(cr, kBlack, 1.0);
  cairo_fill(cr);
  // The white toe cap.
  cairo_move_to(cr, x + 4, y - 8);
  cairo_curve_to(cr, x + 10, y - 8, x + 12, y - 4, x + 12, y);
  cairo_line_to(cr, x + 4, y);
  cairo_close_path(cr);
  setRgba(cr, rgb(240, 240, 244), 1.0);
  cairo_fill(cr);
  cairo_rectangle(cr, x - 9, y - 1.5, 21, 1.5);
  setRgba(cr, rgb(200, 200, 206), 0.9);
  cairo_fill(cr);
}

// A figure in blacks: dark on dark, a cool rim of light down its front edge.
void blackShape(cairo_t* cr)
{
  setRgba(cr, kBlack, 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kBlackRim, 1.4, 0.8);
}

// variant: 0 walking with the flat, 1 crouched, 2 leaning out (the tell), 3 the throw.
void stagehand(cairo_t* cr, double w, double h, int variant, int frame)
{
  cairo_save(cr);
  frameBox(cr, w, h, 96, 160);
  const double crouch = variant == 1 ? 26.0 : 0.0;
  const double lean = variant == 2 ? 0.32 : (variant == 3 ? 0.18 : 0.0);
  const double stride = variant == 0 ? (frame ? 9.0 : -9.0) : 0.0;
  // Legs (a knee bend when crouched).
  const double hipY = 92 + crouch;
  limb(cr, {{42, hipY}, {40 - stride - crouch * 0.4, 128 + crouch * 0.4}, {38 - stride, 154}}, 11, kBlack, kBlackRim);
  limb(cr, {{52, hipY}, {56 + stride + crouch * 0.6, 126 + crouch * 0.4}, {54 + stride, 154}}, 11, kBlack, kBlackRim);
  blackShoe(cr, 38 - stride, 160);
  blackShoe(cr, 54 + stride, 160);
  cairo_save(cr);
  cairo_translate(cr, 47, hipY);
  cairo_rotate(cr, lean);
  cairo_translate(cr, -47, -hipY);
  cairo_translate(cr, 0, crouch);
  // The torso and the back arm.
  if (variant == 0)
    limb(cr, {{40, 50}, {62, 58}, {84, 48}}, 9, kBlack, kBlackRim);
  else if (variant == 2)
    limb(cr, {{40, 50}, {60, 58}, {78, 66}}, 9, kBlack, kBlackRim);
  roundRect(cr, 30, 40, 34, 56, 12);
  blackShape(cr);
  // The head: a black cap, the face in shadow, a headset.
  circle(cr, 50, 24, 13);
  blackShape(cr);
  cairo_move_to(cr, 37, 18);
  cairo_curve_to(cr, 40, 6, 60, 6, 63, 18);
  cairo_line_to(cr, 72, 20);
  stroke(cr, rgb(10, 8, 12), 4.0);
  cairo_move_to(cr, 44, 30);
  cairo_curve_to(cr, 50, 36, 58, 34, 62, 30);
  stroke(cr, rgb(40, 38, 44), 1.4);
  circle(cr, 63, 31, 1.6);
  setRgba(cr, rgb(255, 50, 40), 0.9);
  cairo_fill(cr);
  // The near arm and its glove.
  switch (variant)
  {
    case 0:
      limb(cr, {{50, 50}, {70, 62}, {88, 52}}, 9, kBlack, kBlackRim);
      glove(cr, 88, 52, 1.0, false);
      glove(cr, 84, 46, 0.9, false);
      break;
    case 1:
      limb(cr, {{50, 50}, {62, 66}, {70, 82}}, 9, kBlack, kBlackRim);
      glove(cr, 70, 82, 1.0, false);
      break;
    case 2:
      limb(cr, {{50, 50}, {68, 40}, {82, 22}}, 9, kBlack, kBlackRim);
      glove(cr, 82, 22, 1.15, true);
      glove(cr, 78, 66, 1.0, false);
      break;
    default:
      limb(cr, {{50, 50}, {72, 48}, {94, 44}}, 9, kBlack, kBlackRim);
      glove(cr, 94, 44, 1.1, true);
      break;
  }
  cairo_restore(cr);
  cairo_restore(cr);
}

// --- Prop Mimic -------------------------------------------------------------------------------------

// The chest's planks and iron, (x, y) .. (x + w, y + h).
void chestPlanks(cairo_t* cr, double x, double y, double w, double hh, bool lid)
{
  roundRect(cr, x, y, w, hh, lid ? 10 : 4);
  vgrad(cr, y, y + hh, {{0.0, lighten(kWood, 0.18f)}, {0.5, kWood}, {1.0, kWoodLo}}, 1.0, true);
  stroke(cr, kInk, 2.2);
  for (double py = y + 12; py < y + hh - 4; py += 14)
  {
    cairo_move_to(cr, x + 3, py);
    cairo_line_to(cr, x + w - 3, py);
    stroke(cr, darken(kWood, 0.45f), 1.4, 0.8);
  }
  // Wood grain.
  for (int k = 0; k < 6; ++k)
  {
    const double gy = y + 6 + (k * 37 % int(std::max(8.0, hh - 10)));
    cairo_move_to(cr, x + 10 + k * 17, gy);
    cairo_curve_to(cr, x + 20 + k * 17, gy - 2, x + 30 + k * 17, gy + 2, x + 40 + k * 17, gy);
    stroke(cr, lighten(kWood, 0.3f), 0.8, 0.4);
  }
  // Iron corners.
  for (const double cx : {x, x + w - 14})
    for (const double cy : {y, y + hh - 14})
    {
      cairo_rectangle(cr, cx, cy, 14, 14);
      vgrad(cr, cy, cy + 14, {{0.0, rgb(110, 108, 116)}, {1.0, kIron}}, 1.0, true);
      stroke(cr, kInk, 1.2);
      circle(cr, cx + 7, cy + 7, 1.8);
      setRgba(cr, rgb(170, 168, 176), 1.0);
      cairo_fill(cr);
    }
}

// A row of teeth along y between x0 and x1, pointing down (dir 1) or up (-1).
void teeth(cairo_t* cr, double x0, double x1, double y, int dir, double size)
{
  const int n = int((x1 - x0) / (size * 1.1));
  const double step = (x1 - x0) / n;
  for (int k = 0; k < n; ++k)
  {
    const double x = x0 + k * step;
    cairo_move_to(cr, x, y);
    cairo_line_to(cr, x + step * 0.5, y + dir * size * (k % 2 ? 1.0 : 1.35));
    cairo_line_to(cr, x + step, y);
    cairo_close_path(cr);
  }
  setRgba(cr, rgb(246, 240, 220), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.0);
}

// variant: 0 shut, 1 breath, 2 tell (open on teeth), 3 bite; carrier: green.
void propMimic(cairo_t* cr, double w, double h, int variant, bool carrier)
{
  cairo_save(cr);
  frameBox(cr, w, h, 128, 128);
  const Color tongue = carrier ? rgb(110, 230, 60) : rgb(220, 70, 90);
  const double gap = variant == 1 ? 32.0 : (variant == 2 ? 34.0 : (variant == 3 ? 12.0 : 0.0));
  const double tilt = variant == 2 ? -0.22 : (variant == 1 ? -0.05 : 0.0);
  // The mouth's dark inside and what is in it.
  if (gap > 0)
  {
    cairo_move_to(cr, 6, 44);
    cairo_line_to(cr, 122, 44);
    cairo_line_to(cr, 122, 44 - gap - (variant == 2 ? 18 : 0));
    cairo_line_to(cr, 6, 44 - gap * 0.6);
    cairo_close_path(cr);
    setRgba(cr, rgb(30, 8, 14), 1.0);
    cairo_fill(cr);
    if (carrier)
      glow(cr, 70, 40 - gap * 0.4, 40, rgb(120, 255, 60), 0.5);
    if (variant == 2)
    {
      // The tongue lolling out over the front.
      cairo_move_to(cr, 60, 44);
      cairo_curve_to(cr, 80, 36, 112, 40, 120, 52);
      cairo_curve_to(cr, 124, 62, 116, 70, 110, 64);
      cairo_curve_to(cr, 104, 56, 88, 50, 70, 50);
      cairo_close_path(cr);
      vgrad(cr, 36, 70, {{0.0, lighten(tongue, 0.25f)}, {1.0, darken(tongue, 0.3f)}}, 1.0, true);
      stroke(cr, kInk, 1.4);
    }
    // Eyes glinting in the dark under the lid.
    const double ey = 44 - gap * 0.55;
    for (const double ex : {84.0, 104.0})
    {
      ellipse(cr, ex, ey, variant == 2 ? 6 : 4.5, variant == 2 ? 5 : 2.5);
      setRgba(cr, carrier ? rgb(200, 255, 120) : rgb(255, 220, 90), 1.0);
      cairo_fill(cr);
      circle(cr, ex + 1.5, ey, variant == 2 ? 2.2 : 1.4);
      setRgba(cr, kInk, 1.0);
      cairo_fill(cr);
    }
  }
  // The body.
  chestPlanks(cr, 4, 44, 120, 84, false);
  // (Its PROPS stencil is drawn over the sprite by the world, so it reads
  // the right way round whichever way the mimic faces.)
  if (gap > 0)
    teeth(cr, 10, 120, 44, -1, variant == 3 ? 9 : 11);
  // The lid, hinged at the back (left).
  cairo_save(cr);
  cairo_translate(cr, 4, 44 - gap);
  cairo_rotate(cr, tilt);
  chestPlanks(cr, 0, -36, 120, 36, true);
  // The hasp at the front.
  cairo_rectangle(cr, 104, -14, 10, 16);
  vgrad(cr, -14, 2, {{0.0, rgb(140, 138, 146)}, {1.0, kIron}}, 1.0, true);
  stroke(cr, kInk, 1.2);
  if (gap > 0)
    teeth(cr, 6, 116, 0, 1, variant == 3 ? 9 : 11);
  cairo_restore(cr);
  // The bite: a slobber of tongue over the teeth.
  if (variant == 3)
  {
    cairo_move_to(cr, 96, 46);
    cairo_curve_to(cr, 104, 54, 114, 56, 118, 50);
    stroke(cr, tongue, 4.0);
  }
  cairo_restore(cr);
}

// --- The practice dummy -----------------------------------------------------------------------------

void practiceDummy(cairo_t* cr, double w, double h, int frame)
{
  cairo_save(cr);
  frameBox(cr, w, h, 96, 160);
  const double step = frame ? 6.0 : -6.0;
  // Stubby wooden legs.
  for (const auto& lg : {std::array<double, 2>{36, -step}, {60, step}})
  {
    roundRect(cr, lg[0] - 5 + lg[1], 120, 10, 38, 4);
    hgrad(cr, lg[0] - 5, lg[0] + 5, {{0.0, rgb(200, 150, 90)}, {1.0, rgb(120, 80, 40)}}, 1.0, true);
    stroke(cr, kInk, 1.6);
    ellipse(cr, lg[0] + lg[1] + 3, 157, 9, 4);
    setRgba(cr, rgb(90, 60, 30), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.2);
  }
  // The sack body, tied at the neck, straw poking out.
  cairo_move_to(cr, 48, 40);
  cairo_curve_to(cr, 84, 44, 84, 126, 48, 128);
  cairo_curve_to(cr, 12, 126, 12, 44, 48, 40);
  cairo_close_path(cr);
  vgrad(cr, 40, 128, {{0.0, rgb(222, 196, 146)}, {1.0, rgb(160, 128, 84)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  for (int k = 0; k < 12; ++k)
  {
    const double y = 52 + k * 6.0;
    cairo_move_to(cr, 24 + (k % 2) * 2, y);
    cairo_line_to(cr, 72 - (k % 2) * 2, y);
  }
  stroke(cr, rgb(150, 120, 80), 0.7, 0.4);
  // The target.
  for (int k = 0; k < 3; ++k)
  {
    circle(cr, 52, 82, 18 - k * 6);
    setRgba(cr, k % 2 ? rgb(250, 246, 236) : rgb(214, 40, 40), 1.0);
    cairo_fill(cr);
  }
  circle(cr, 52, 82, 18);
  stroke(cr, kInk, 1.4);
  // Head and neck tie.
  circle(cr, 48, 24, 18);
  vgrad(cr, 6, 42, {{0.0, rgb(230, 206, 160)}, {1.0, rgb(168, 136, 92)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  cairo_move_to(cr, 34, 40);
  cairo_line_to(cr, 62, 40);
  stroke(cr, rgb(120, 70, 30), 4.0);
  for (const double a : {-0.9, -0.5, 0.4})
  {
    cairo_move_to(cr, 48, 8);
    cairo_line_to(cr, 48 + std::sin(a) * 12, 8 - std::cos(a) * 12);
  }
  stroke(cr, rgb(230, 200, 90), 1.6);
  // Stitched cross eyes and a stitched smile.
  for (const double ex : {48.0, 60.0})
  {
    cairo_move_to(cr, ex - 3, 19);
    cairo_line_to(cr, ex + 3, 25);
    cairo_move_to(cr, ex + 3, 19);
    cairo_line_to(cr, ex - 3, 25);
  }
  stroke(cr, kInk, 1.8);
  cairo_move_to(cr, 46, 32);
  cairo_curve_to(cr, 52, 35, 58, 35, 63, 31);
  stroke(cr, kInk, 1.4);
  cairo_restore(cr);
}

// --- The duck's hard hat and vest ---------------------------------------------------------------------

void duckHardHatVest(cairo_t* cr, double w, double h, int variant)
{
  if (variant != 1)
    return;
  cairo_save(cr);
  cairo_translate(cr, kM, kM);
  cairo_scale(cr, w / 64.0, h / 64.0);
  // The vest: orange over the body, two silver stripes, the front open.
  cairo_move_to(cr, 27, 34);
  cairo_curve_to(cr, 22, 38, 22, 46, 28, 50);
  cairo_curve_to(cr, 38, 53, 50, 50, 54, 42);
  cairo_curve_to(cr, 54, 38, 52, 35, 49, 33);
  cairo_close_path(cr);
  vgrad(cr, 33, 52, {{0.0, rgb(255, 150, 60)}, {1.0, rgb(214, 80, 20)}}, 1.0, true);
  stroke(cr, kInk, 1.1);
  cairo_save(cr);
  cairo_move_to(cr, 27, 34);
  cairo_curve_to(cr, 22, 38, 22, 46, 28, 50);
  cairo_curve_to(cr, 38, 53, 50, 50, 54, 42);
  cairo_curve_to(cr, 54, 38, 52, 35, 49, 33);
  cairo_close_path(cr);
  cairo_clip(cr);
  for (const double y : {38.0, 44.5})
  {
    cairo_rectangle(cr, 18, y, 40, 2.6);
    setRgba(cr, rgb(232, 236, 244), 1.0);
    cairo_fill(cr);
  }
  cairo_restore(cr);
  cairo_move_to(cr, 46, 34);
  cairo_line_to(cr, 50, 46);
  stroke(cr, rgb(170, 60, 10), 1.0);
  // The hard hat.
  const double x = 42, y = 19, s = 1.0;
  cairo_move_to(cr, x - 13 * s, y);
  cairo_curve_to(cr, x - 13 * s, y - 16 * s, x + 12 * s, y - 16 * s, x + 12 * s, y);
  cairo_close_path(cr);
  vgrad(cr, y - 15 * s, y, {{0.0, rgb(255, 240, 150)}, {0.5, rgb(255, 210, 40)}, {1.0, rgb(200, 150, 20)}}, 1.0, true);
  stroke(cr, kInk, 1.2);
  cairo_move_to(cr, x - 2 * s, y - 12 * s);
  cairo_curve_to(cr, x + 4 * s, y - 13 * s, x + 9 * s, y - 8 * s, x + 10 * s, y - 2 * s);
  stroke(cr, rgb(255, 246, 200), 1.6, 0.9);
  cairo_move_to(cr, x - 15 * s, y + 1);
  cairo_line_to(cr, x + 20 * s, y + 1);
  stroke(cr, kInk, 3.4);
  cairo_move_to(cr, x - 15 * s, y + 1);
  cairo_line_to(cr, x + 20 * s, y + 1);
  stroke(cr, rgb(230, 180, 30), 1.8);
  cairo_restore(cr);
}

} // namespace

bool drawWarehouse36Art(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame)
{
  (void)t;
  if (key == "forklift_bot")
    forkliftBot(cr, w, h, std::clamp(variant, 0, 4), frame % 2);
  else if (key == "stagehand")
    stagehand(cr, w, h, std::clamp(variant, 0, 3), frame % 2);
  else if (key == "prop_mimic")
    propMimic(cr, w, h, std::clamp(variant, 0, 3), frame % 2 == 1);
  else if (key == "practice_dummy")
    practiceDummy(cr, w, h, frame % 2);
  else if (key == "duck_hard_hat_vest")
    duckHardHatVest(cr, w, h, variant);
  else
    return false;
  return true;
}

} // namespace gr
