// The Fly Tower (Level 40) in the style of enemy_art.cpp: smooth shapes
// with a dark ink line, lit warm from the stage below and cold from the
// worklights above. The fly crew: the Rigger in blacks and a climbing
// harness, a hard hat with a red headlamp, a coil of rope over his
// shoulder and a laser-sighted line launcher; the Sandbag Drop, a fly-crew
// hand in a beanie and a black hoodie on a hemp line, a knife on his belt.
// The duck wears a stagehand's headset and a black CREW T-shirt. Each is
// drawn in screen pixels into a texture with a 32 px margin, the cell box
// at (32, 32) .. (32 + w, 32 + h), facing right.

#include "assets/enemy_art_flytower40.hpp"

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
constexpr Color kInk = rgb(12, 10, 16);
constexpr Color kBlack = rgb(38, 36, 44);
constexpr Color kBlackHi = rgb(70, 66, 78);
constexpr Color kSkinA = rgb(200, 146, 110);
constexpr Color kSkinB = rgb(130, 86, 60);
constexpr Color kWeb = rgb(240, 120, 30);   // harness webbing
constexpr Color kRope = rgb(214, 190, 140); // hemp
constexpr Color kGlove = rgb(150, 120, 80);
constexpr Color kSteel = rgb(176, 180, 192);
constexpr Color kLaser = rgb(255, 40, 40);

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

// Fills the path lit warm from below right (the stage) over a cool top.
void lit(cairo_t* cr, double x0, double y0, double x1, double y1, Color c, bool preserve = true)
{
  cairo_pattern_t* g = cairo_pattern_create_linear(x0, y0, x1, y1);
  stop(g, 0.0, lighten(c, 0.18f));
  stop(g, 0.5, c);
  stop(g, 1.0, lerpColor(darken(c, 0.2f), rgb(255, 150, 60), 0.12f));
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

void ink(cairo_t* cr, double w = 2.0) { stroke(cr, kInk, w); }

void frameBox(cairo_t* cr, double w, double h, double dw, double dh)
{
  cairo_translate(cr, kM, kM);
  cairo_scale(cr, w / dw, h / dh);
}

void limb(cairo_t* cr, std::initializer_list<std::array<double, 2>> pts, double width, Color c)
{
  for (int pass = 0; pass < 3; ++pass)
  {
    bool first = true;
    for (const auto& p : pts)
    {
      if (first)
        cairo_move_to(cr, p[0], p[1] - (pass == 2 ? width * 0.25 : 0.0));
      else
        cairo_line_to(cr, p[0], p[1] - (pass == 2 ? width * 0.25 : 0.0));
      first = false;
    }
    if (pass == 0)
      stroke(cr, kInk, width + 3.4);
    else if (pass == 1)
      stroke(cr, c, width);
    else
      stroke(cr, lighten(c, 0.3f), width * 0.25, 0.5);
  }
}

void hand(cairo_t* cr, double x, double y, double r, Color c)
{
  circle(cr, x, y, r);
  lit(cr, x - r, y - r, x + r, y + r, c);
  ink(cr, 1.6);
}

void boot(cairo_t* cr, double x, double y)
{
  cairo_move_to(cr, x - 10, y);
  cairo_line_to(cr, x - 10, y - 12);
  cairo_curve_to(cr, x - 2, y - 14, x + 6, y - 10, x + 14, y - 4);
  cairo_line_to(cr, x + 14, y);
  cairo_close_path(cr);
  lit(cr, x - 10, y - 14, x + 12, y, rgb(30, 26, 24));
  ink(cr, 1.6);
  cairo_rectangle(cr, x - 10, y - 3, 24, 3);
  setRgba(cr, rgb(70, 60, 50), 1.0);
  cairo_fill(cr);
}

// A face in three-quarter view looking right; `squint` narrows the eye
// along a sight, `blink` shuts it.
void face(cairo_t* cr, double cx, double cy, double r, Color skin, double brow, bool squint, bool blink, bool stubble)
{
  circle(cr, cx, cy, r);
  lit(cr, cx - r, cy - r, cx + r * 0.8, cy + r, skin);
  ink(cr, 2.0);
  if (stubble)
  {
    cairo_save(cr);
    circle(cr, cx, cy, r - 1);
    cairo_clip(cr);
    ellipse(cr, cx + r * 0.45, cy + r * 0.55, r * 0.75, r * 0.5);
    setRgba(cr, darken(skin, 0.45f), 0.45);
    cairo_fill(cr);
    cairo_restore(cr);
  }
  ellipse(cr, cx - r * 0.35, cy + r * 0.05, r * 0.2, r * 0.28);
  lit(cr, cx - r * 0.6, cy - r * 0.3, cx, cy + r * 0.3, darken(skin, 0.08f));
  ink(cr, 1.2);
  if (blink || squint)
  {
    cairo_move_to(cr, cx + r * 0.3, cy - r * 0.1);
    cairo_line_to(cr, cx + r * 0.65, cy - r * (squint ? 0.14 : 0.06));
    stroke(cr, kInk, 1.6);
  }
  else
  {
    ellipse(cr, cx + r * 0.45, cy - r * 0.12, r * 0.15, r * 0.2);
    setRgba(cr, rgb(255, 255, 255), 1.0);
    cairo_fill_preserve(cr);
    ink(cr, 1.0);
    circle(cr, cx + r * 0.52, cy - r * 0.1, r * 0.09);
    setRgba(cr, kInk, 1.0);
    cairo_fill(cr);
  }
  cairo_move_to(cr, cx + r * 0.25, cy - r * 0.38 - brow);
  cairo_line_to(cr, cx + r * 0.7, cy - r * 0.32 + brow);
  stroke(cr, darken(skin, 0.6f), 2.4);
  cairo_move_to(cr, cx + r * 0.85, cy - r * 0.05);
  cairo_line_to(cr, cx + r * 1.08, cy + r * 0.2);
  cairo_line_to(cr, cx + r * 0.85, cy + r * 0.28);
  stroke(cr, darken(skin, 0.45f), 1.4);
  cairo_move_to(cr, cx + r * 0.55, cy + r * 0.52);
  cairo_line_to(cr, cx + r * 0.85, cy + r * 0.5);
  stroke(cr, darken(skin, 0.55f), 1.6);
}

// A coil of rope: loops seen at a slant.
void ropeCoil(cairo_t* cr, double cx, double cy, double rx, double ry, double angle)
{
  for (int k = 0; k < 4; ++k)
  {
    ellipse(cr, cx + k * 1.6, cy + k * 0.8, rx - k * 0.8, ry - k, angle);
    stroke(cr, kInk, 4.6);
    ellipse(cr, cx + k * 1.6, cy + k * 0.8, rx - k * 0.8, ry - k, angle);
    stroke(cr, k % 2 ? darken(kRope, 0.15f) : kRope, 2.6);
  }
}

void carabiner(cairo_t* cr, double x, double y)
{
  ellipse(cr, x, y, 2.6, 4.6);
  stroke(cr, kInk, 2.6);
  ellipse(cr, x, y, 2.6, 4.6);
  stroke(cr, kSteel, 1.4);
}

// --- Rigger ----------------------------------------------------------------------------------

void rigger(cairo_t* cr, double w, double h, int variant, int frame)
{
  cairo_save(cr);
  frameBox(cr, w, h, 96, 160);
  ellipse(cr, 48, 157, 32, 4);
  setRgba(cr, kInk, 0.35);
  cairo_fill(cr);
  const bool aim = variant == 1;
  // Legs: black cargo trousers, knee pads, boots planted wide.
  limb(cr, {{40, 96}, {34, 124}, {32, 146}}, 15, darken(kBlack, 0.1f));
  boot(cr, 33, 156);
  limb(cr, {{54, 96}, {60, 124}, {62, 146}}, 15, kBlack);
  boot(cr, 64, 156);
  for (const double kx : {34.0, 60.0})
  {
    roundRect(cr, kx - 6, 116, 13, 13, 3);
    lit(cr, kx - 6, 116, kx + 7, 129, rgb(70, 70, 78));
    ink(cr, 1.2);
  }
  // The harness's leg loops.
  for (const double lx : {38.0, 56.0})
  {
    ellipse(cr, lx, 102, 9, 3.4, lx < 48 ? 0.2 : -0.2);
    stroke(cr, kInk, 4.4);
    ellipse(cr, lx, 102, 9, 3.4, lx < 48 ? 0.2 : -0.2);
    stroke(cr, kWeb, 2.6);
  }
  // The back arm: steadying the launcher (aiming) or hanging loose.
  if (aim)
    limb(cr, {{38, 54}, {52, 44}, {70, 36}}, 10, kBlack);
  else
    limb(cr, {{38, 54}, {34, 74}, {38, 90}}, 10, kBlack);
  // The torso: a black long-sleeve, the harness's waist belt and its gear.
  cairo_move_to(cr, 30, 54);
  cairo_curve_to(cr, 36, 44, 60, 43, 66, 54);
  cairo_line_to(cr, 64, 96);
  cairo_line_to(cr, 32, 96);
  cairo_close_path(cr);
  lit(cr, 30, 44, 66, 96, kBlack);
  ink(cr, 2.0);
  // The coil over the shoulder, across the chest.
  ropeCoil(cr, 44, 66, 14, 22, 0.55);
  cairo_rectangle(cr, 30, 88, 36, 8);
  setRgba(cr, kWeb, 1.0);
  cairo_fill_preserve(cr);
  ink(cr, 1.2);
  roundRect(cr, 44, 87, 8, 10, 1.5);
  setRgba(cr, kSteel, 1.0);
  cairo_fill_preserve(cr);
  ink(cr, 1.0);
  for (int k = 0; k < 3; ++k)
    carabiner(cr, 34 + k * 5.0, 101 + (k % 2) * 2.0);
  // The head: a black hard hat with its red headlamp.
  limb(cr, {{50, 46}, {50, 38}}, 9, kSkinA);
  const bool blink = !aim && frame % 2 == 1;
  face(cr, 50, 27, 13, kSkinA, aim ? 2.0 : 0.0, aim, blink, true);
  cairo_move_to(cr, 34, 24);
  cairo_curve_to(cr, 34, 6, 66, 6, 66, 22);
  cairo_line_to(cr, 70, 24);
  cairo_line_to(cr, 70, 27);
  cairo_line_to(cr, 33, 27);
  cairo_close_path(cr);
  lit(cr, 34, 6, 70, 27, rgb(32, 30, 36));
  ink(cr, 1.6);
  cairo_move_to(cr, 40, 12);
  cairo_curve_to(cr, 46, 8, 56, 8, 60, 12);
  stroke(cr, rgb(110, 110, 120), 1.4, 0.6);
  roundRect(cr, 58, 14, 8, 7, 2);
  setRgba(cr, rgb(30, 30, 34), 1.0);
  cairo_fill_preserve(cr);
  ink(cr, 1.0);
  circle(cr, 64.5, 17.5, 2.2);
  setRgba(cr, kLaser, 1.0);
  cairo_fill(cr);
  // The front arm and the launcher: up at the eye and down the sight, or
  // held low across the body.
  cairo_save(cr);
  if (aim)
  {
    limb(cr, {{60, 54}, {72, 44}, {78, 38}}, 10, kBlack);
    cairo_translate(cr, 78, 36);
  }
  else
  {
    limb(cr, {{60, 54}, {66, 70}, {70, 80}}, 10, kBlack);
    cairo_translate(cr, 70, 80);
    cairo_rotate(cr, 0.55);
  }
  // The launcher: a stubby black body, a grip, a fat barrel with the line's
  // spool under it, the laser module on its rail.
  roundRect(cr, -10, -6, 26, 11, 3);
  lit(cr, -10, -6, 16, 5, rgb(40, 40, 46));
  ink(cr, 1.4);
  roundRect(cr, -6, 3, 7, 12, 2);
  lit(cr, -6, 3, 1, 15, rgb(30, 30, 34));
  ink(cr, 1.2);
  cairo_rectangle(cr, 14, -5, 6, 9);
  setRgba(cr, rgb(70, 70, 80), 1.0);
  cairo_fill_preserve(cr);
  ink(cr, 1.0);
  circle(cr, 4, 8, 4);
  setRgba(cr, kRope, 1.0);
  cairo_fill_preserve(cr);
  ink(cr, 1.0);
  roundRect(cr, -2, -11, 16, 5, 1.5);
  setRgba(cr, rgb(24, 24, 28), 1.0);
  cairo_fill_preserve(cr);
  ink(cr, 1.0);
  circle(cr, 14, -8.5, 1.8);
  setRgba(cr, aim ? kLaser : rgb(90, 20, 20), 1.0);
  cairo_fill(cr);
  if (aim)
  {
    cairo_pattern_t* g = cairo_pattern_create_radial(14, -8.5, 0, 14, -8.5, 8);
    stop(g, 0.0, kLaser, 0.8);
    stop(g, 1.0, kLaser, 0.0);
    cairo_set_source(cr, g);
    circle(cr, 14, -8.5, 8);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  hand(cr, -2, 2, 5.5, kGlove);
  cairo_restore(cr);
  cairo_restore(cr);
}

// --- Sandbag Drop ----------------------------------------------------------------------------

void sandbagDrop(cairo_t* cr, double w, double h, int variant, int frame)
{
  cairo_save(cr);
  frameBox(cr, w, h, 96, 160);
  ellipse(cr, 48, 157, 30, 4);
  setRgba(cr, kInk, 0.35);
  cairo_fill(cr);
  // The line: from the top of the box down to his hands (cut, its end
  // whips up out of reach).
  if (variant == 2)
  {
    cairo_move_to(cr, 66, 0);
    cairo_curve_to(cr, 70, 14, 60, 22, 72, 30);
    stroke(cr, kInk, 5.0);
    cairo_move_to(cr, 66, 0);
    cairo_curve_to(cr, 70, 14, 60, 22, 72, 30);
    stroke(cr, kRope, 3.0);
  }
  else
  {
    cairo_move_to(cr, 66, 0);
    cairo_line_to(cr, 64, 76);
    stroke(cr, kInk, 5.0);
    cairo_move_to(cr, 66, 0);
    cairo_line_to(cr, 64, 76);
    stroke(cr, kRope, 3.0);
    for (double y = 6; y < 76; y += 7)
    {
      cairo_move_to(cr, 63.5, y);
      cairo_line_to(cr, 66.5, y + 3);
      stroke(cr, darken(kRope, 0.3f), 0.8, 0.8);
    }
  }
  // Legs, one knee forward, a stance for pulling.
  limb(cr, {{40, 96}, {32, 122}, {30, 146}}, 15, rgb(46, 46, 54));
  boot(cr, 31, 156);
  limb(cr, {{54, 96}, {62, 120}, {62, 146}}, 15, rgb(56, 56, 64));
  boot(cr, 64, 156);
  // The back arm on the line.
  if (variant == 2)
    limb(cr, {{38, 56}, {30, 76}, {32, 92}}, 10, kBlack);
  else
    limb(cr, {{38, 56}, {50, 62}, {62, 60}}, 10, kBlack);
  // The torso: a black hoodie, its pocket, a knife sheath on the belt.
  cairo_move_to(cr, 28, 56);
  cairo_curve_to(cr, 34, 44, 62, 44, 66, 56);
  cairo_curve_to(cr, 70, 74, 68, 90, 66, 98);
  cairo_line_to(cr, 30, 98);
  cairo_curve_to(cr, 27, 86, 26, 70, 28, 56);
  cairo_close_path(cr);
  lit(cr, 26, 44, 70, 98, kBlack);
  ink(cr, 2.0);
  cairo_move_to(cr, 36, 78);
  cairo_curve_to(cr, 44, 76, 54, 76, 60, 78);
  cairo_line_to(cr, 58, 90);
  cairo_line_to(cr, 38, 90);
  cairo_close_path(cr);
  setRgba(cr, kBlackHi, 0.6);
  cairo_fill_preserve(cr);
  ink(cr, 1.2);
  cairo_move_to(cr, 40, 50);
  cairo_curve_to(cr, 42, 58, 44, 60, 44, 64);
  cairo_move_to(cr, 52, 50);
  cairo_curve_to(cr, 52, 58, 52, 60, 54, 64);
  stroke(cr, rgb(200, 200, 210), 1.2, 0.7);
  if (variant == 0)
  {
    roundRect(cr, 26, 88, 7, 16, 2);
    lit(cr, 26, 88, 33, 104, rgb(90, 60, 30));
    ink(cr, 1.0);
  }
  // The head: a black beanie, a beard.
  limb(cr, {{48, 46}, {48, 38}}, 9, kSkinB);
  face(cr, 48, 27, 13, kSkinB, variant >= 1 ? 2.0 : 0.0, variant == 1, variant == 0 && frame % 2 == 1, false);
  cairo_move_to(cr, 40, 32);
  cairo_curve_to(cr, 42, 44, 58, 44, 62, 30);
  cairo_curve_to(cr, 58, 36, 52, 38, 48, 36);
  cairo_close_path(cr);
  setRgba(cr, rgb(40, 26, 18), 1.0);
  cairo_fill(cr);
  cairo_move_to(cr, 34, 26);
  cairo_curve_to(cr, 32, 6, 62, 4, 63, 24);
  cairo_close_path(cr);
  lit(cr, 32, 4, 63, 26, rgb(30, 30, 36));
  ink(cr, 1.6);
  for (double x = 38; x < 62; x += 4)
  {
    cairo_move_to(cr, x, 12 + std::abs(x - 48) * 0.3);
    cairo_line_to(cr, x, 24);
  }
  stroke(cr, rgb(60, 60, 70), 1.0, 0.7);
  cairo_rectangle(cr, 33, 21, 31, 5);
  setRgba(cr, rgb(24, 24, 28), 1.0);
  cairo_fill_preserve(cr);
  ink(cr, 1.0);
  // The front arm: on the line, the knife up to it, or the cut.
  if (variant == 0)
  {
    limb(cr, {{58, 56}, {62, 46}, {64, 40}}, 10, kBlack);
    hand(cr, 64, 40, 5.5, kGlove);
    hand(cr, 62, 60, 5.5, kGlove);
  }
  else
  {
    const bool cut = variant == 2;
    if (cut)
      limb(cr, {{58, 56}, {74, 64}, {82, 76}}, 10, kBlack);
    else
      limb(cr, {{58, 56}, {72, 46}, {74, 36}}, 10, kBlack);
    cairo_save(cr);
    cairo_translate(cr, cut ? 82 : 74, cut ? 77 : 35);
    cairo_rotate(cr, cut ? 0.9 : -0.6);
    roundRect(cr, -3, -2, 6, 12, 2);
    lit(cr, -3, -2, 3, 10, rgb(90, 60, 30));
    ink(cr, 1.0);
    cairo_move_to(cr, -2.5, -2);
    cairo_line_to(cr, -2.5, -18);
    cairo_line_to(cr, 3, -12);
    cairo_line_to(cr, 3, -2);
    cairo_close_path(cr);
    setRgba(cr, rgb(220, 226, 236), 1.0);
    cairo_fill_preserve(cr);
    ink(cr, 1.0);
    cairo_restore(cr);
    hand(cr, cut ? 82 : 74, cut ? 77 : 35, 5.5, kGlove);
    if (!cut)
      hand(cr, 62, 60, 5.5, kGlove);
    else
      for (int k = 0; k < 3; ++k)
      {
        cairo_move_to(cr, 70 - k * 4, 30 + k * 6);
        cairo_line_to(cr, 84 - k * 4, 40 + k * 6);
        stroke(cr, rgb(255, 255, 255), 1.4, 0.6);
      }
  }
  cairo_restore(cr);
}

// --- The duck's headset and CREW tee ---------------------------------------------------------

void duckHeadset(cairo_t* cr, double w, double h, int variant)
{
  cairo_save(cr);
  cairo_translate(cr, kM, kM);
  cairo_scale(cr, w / 64.0, h / 64.0);
  if (variant == 0)
  {
    cairo_arc(cr, 39, 24, 13, kPi * 1.15, kPi * 1.85);
    stroke(cr, kInk, 3.4);
    cairo_arc(cr, 39, 24, 13, kPi * 1.15, kPi * 1.85);
    stroke(cr, rgb(70, 70, 80), 1.8);
  }
  else
  {
    // The CREW tee over the body.
    cairo_save(cr);
    ellipse(cr, 30, 42, 20, 14);
    cairo_clip(cr);
    cairo_move_to(cr, 6, 30);
    cairo_line_to(cr, 54, 30);
    cairo_line_to(cr, 54, 50);
    cairo_curve_to(cr, 40, 53, 20, 53, 6, 50);
    cairo_close_path(cr);
    lit(cr, 10, 30, 50, 52, rgb(30, 30, 36), false);
    ellipse(cr, 36, 31, 8, 4);
    setRgba(cr, rgb(245, 190, 30), 1.0);
    cairo_fill(cr);
    cairo_restore(cr);
    cairo_move_to(cr, 12, 50);
    cairo_curve_to(cr, 24, 53, 40, 53, 50, 49);
    stroke(cr, kInk, 1.6);
    selectGameFont(cr);
    cairo_set_font_size(cr, 6.5);
    cairo_move_to(cr, 19, 45);
    setRgba(cr, rgb(235, 235, 240), 0.95);
    cairo_show_text(cr, "CREW");
    // The band and the near cup.
    cairo_arc(cr, 37, 24, 13.5, kPi * 1.05, kPi * 1.95);
    stroke(cr, kInk, 3.6);
    cairo_arc(cr, 37, 24, 13.5, kPi * 1.05, kPi * 1.95);
    stroke(cr, rgb(84, 84, 96), 2.0);
    ellipse(cr, 32, 26, 5.5, 7);
    lit(cr, 26, 19, 38, 33, rgb(40, 40, 48), true);
    ink(cr, 1.3);
    // The talkback mic on its boom, round to the beak.
    cairo_move_to(cr, 33, 31);
    cairo_curve_to(cr, 36, 38, 44, 37, 49, 33);
    stroke(cr, kInk, 2.4);
    cairo_move_to(cr, 33, 31);
    cairo_curve_to(cr, 36, 38, 44, 37, 49, 33);
    stroke(cr, rgb(90, 90, 100), 1.0);
    ellipse(cr, 49.5, 32.5, 2.6, 2.0);
    setRgba(cr, rgb(30, 30, 34), 1.0);
    cairo_fill(cr);
  }
  cairo_restore(cr);
}

} // namespace

bool drawFlyTower40Art(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame)
{
  (void)t;
  if (key == "rigger")
    rigger(cr, w, h, std::clamp(variant, 0, 1), frame % 2);
  else if (key == "sandbag")
    sandbagDrop(cr, w, h, std::clamp(variant, 0, 2), frame % 2);
  else if (key == "duck_headset")
    duckHeadset(cr, w, h, variant);
  else
    return false;
  return true;
}

} // namespace gr
