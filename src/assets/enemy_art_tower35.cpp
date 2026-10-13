// The Transmitter (Level 35) in the style of enemy_art.cpp: smooth shapes
// with a dark ink line, lit from the dawn side (top left) with a warm rim,
// cool shadows. The Mast Climber is a rigger in hi-vis with a big wrench,
// the Relay Drone a white quadcopter carrying a little relay dish, the
// Feedback Node a squat coil-topped emitter with horn speakers, the
// Halcyon Titan a glossy white showroom mech in Halcyon blue. The Copycat
// dummies are crash-test mannequins in hard hats; the duck wears one too.
// Each is drawn in screen pixels into a texture with a 32 px margin, the
// cell box at (32, 32) .. (32 + w, 32 + h), facing right.

#include "assets/enemy_art_tower35.hpp"

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
constexpr Color kInk = rgb(20, 16, 26);
constexpr Color kVest = rgb(255, 120, 30);
constexpr Color kVestLo = rgb(190, 70, 20);
constexpr Color kReflect = rgb(226, 232, 240);
constexpr Color kNavy = rgb(44, 54, 84);
constexpr Color kNavyLo = rgb(26, 30, 50);
constexpr Color kSkin = rgb(224, 170, 130);
constexpr Color kSteel = rgb(170, 176, 188);
constexpr Color kSteelLo = rgb(84, 88, 102);
constexpr Color kWhite = rgb(244, 246, 250);
constexpr Color kShell = rgb(198, 204, 216);
constexpr Color kHalcyon = rgb(60, 140, 255);
constexpr Color kRed = rgb(214, 50, 40);

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
void limb(cairo_t* cr, std::initializer_list<std::array<double, 2>> pts, double width, Color c)
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
    stroke(cr, pass == 0 ? kInk : c, pass == 0 ? width + 3.0 : width);
  }
}

// --- Mast Climber ---------------------------------------------------------------------------------

// A big adjustable wrench, its jaw at (x, y), the handle running `len` px
// along `angle` (radians, 0 = to the right).
void wrench(cairo_t* cr, double x, double y, double angle, double len, bool glint)
{
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_rotate(cr, angle);
  // The handle.
  roundRect(cr, 4, -3.5, len, 7, 3.5);
  hgrad(cr, 0, len, {{0.0, rgb(230, 234, 242)}, {1.0, kSteelLo}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  roundRect(cr, len - 14, -4.5, 16, 9, 4);
  setRgba(cr, kRed, 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.4);
  // The head: an open jaw.
  cairo_move_to(cr, 6, -6);
  cairo_line_to(cr, -8, -10);
  cairo_line_to(cr, -12, -4);
  cairo_line_to(cr, -4, -2);
  cairo_line_to(cr, -4, 2);
  cairo_line_to(cr, -12, 4);
  cairo_line_to(cr, -8, 10);
  cairo_line_to(cr, 6, 6);
  cairo_close_path(cr);
  vgrad(cr, -10, 10, {{0.0, rgb(240, 244, 250)}, {0.5, kSteel}, {1.0, kSteelLo}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  circle(cr, 2, 0, 2);
  setRgba(cr, kSteelLo, 1.0);
  cairo_fill(cr);
  if (glint)
  {
    for (int k = 0; k < 4; ++k)
    {
      const double a = k * kPi * 0.5 + kPi * 0.25;
      cairo_move_to(cr, -6, -8);
      cairo_line_to(cr, -6 + std::cos(a) * 9, -8 + std::sin(a) * 9);
      stroke(cr, rgb(255, 255, 240), 1.6, 0.95);
    }
    glow(cr, -6, -8, 12, rgb(255, 250, 220), 0.9);
  }
  cairo_restore(cr);
}

// A hard hat, its brim's middle at (x, y), facing right; `back` turns it
// round (no peak).
void hardHat(cairo_t* cr, double x, double y, double s, Color c, bool stripe, bool back = false)
{
  cairo_move_to(cr, x - 13 * s, y);
  cairo_curve_to(cr, x - 13 * s, y - 16 * s, x + 12 * s, y - 16 * s, x + 12 * s, y);
  cairo_close_path(cr);
  vgrad(cr, y - 15 * s, y, {{0.0, lighten(c, 0.5f)}, {0.5, c}, {1.0, darken(c, 0.25f)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  // The ridge.
  cairo_move_to(cr, x - 2 * s, y - 12 * s);
  cairo_curve_to(cr, x + 4 * s, y - 13 * s, x + 9 * s, y - 8 * s, x + 10 * s, y - 2 * s);
  stroke(cr, lighten(c, 0.6f), 2.2 * s, 0.9);
  if (stripe)
  {
    cairo_move_to(cr, x - 12 * s, y - 4 * s);
    cairo_curve_to(cr, x - 6 * s, y - 7 * s, x + 4 * s, y - 7 * s, x + 11.5 * s, y - 4 * s);
    stroke(cr, kRed, 2.6 * s);
  }
  // The brim (a peak in front).
  cairo_move_to(cr, x - 15 * s, y + 1);
  cairo_line_to(cr, x + (back ? 14 : 20) * s, y + 1);
  stroke(cr, kInk, 4.4 * s);
  cairo_move_to(cr, x - 15 * s, y + 1);
  cairo_line_to(cr, x + (back ? 14 : 20) * s, y + 1);
  stroke(cr, darken(c, 0.1f), 2.4 * s);
}

void climberBoot(cairo_t* cr, double x, double y)
{
  roundRect(cr, x - 8, y - 9, 18, 9, 3);
  vgrad(cr, y - 9, y, {{0.0, rgb(120, 80, 50)}, {1.0, rgb(60, 36, 24)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  cairo_rectangle(cr, x - 8, y - 2, 18, 2);
  setRgba(cr, rgb(30, 22, 18), 1.0);
  cairo_fill(cr);
}

// The rigger's torso: navy shirt under an orange vest with silver stripes,
// harness straps over it, a tool belt.
void climberTorso(cairo_t* cr, double cx, double top, double bottom, bool back)
{
  roundRect(cr, cx - 18, top, 36, bottom - top, 9);
  vgrad(cr, top, bottom, {{0.0, lighten(kVest, 0.15f)}, {0.6, kVest}, {1.0, kVestLo}}, 1.0, true);
  stroke(cr, kInk, 1.8);
  if (!back)
  {
    // The shirt showing at the collar, the vest's front opening.
    cairo_move_to(cr, cx + 2, top + 1);
    cairo_line_to(cr, cx + 8, top + 12);
    cairo_line_to(cr, cx + 14, top + 1);
    cairo_close_path(cr);
    setRgba(cr, kNavy, 1.0);
    cairo_fill(cr);
  }
  // The reflective stripes.
  for (const double y : {top + 18, top + 30})
  {
    cairo_rectangle(cr, cx - 17, y, 34, 4);
    vgrad(cr, y, y + 4, {{0.0, rgb(255, 255, 255)}, {1.0, rgb(170, 180, 196)}});
  }
  if (back)
  {
    cairo_rectangle(cr, cx - 2, top + 2, 4, bottom - top - 8);
    setRgba(cr, kReflect, 0.95);
    cairo_fill(cr);
  }
  // Harness straps.
  cairo_move_to(cr, cx - 12, top + 2);
  cairo_line_to(cr, cx + 6, bottom - 6);
  cairo_move_to(cr, cx + 12, top + 2);
  cairo_line_to(cr, cx - 4, bottom - 6);
  stroke(cr, rgb(30, 30, 36), 3.2);
  cairo_move_to(cr, cx - 12, top + 2);
  cairo_line_to(cr, cx + 6, bottom - 6);
  cairo_move_to(cr, cx + 12, top + 2);
  cairo_line_to(cr, cx - 4, bottom - 6);
  stroke(cr, rgb(250, 210, 40), 1.2, 0.9);
  // The tool belt and a pouch, a carabiner.
  cairo_rectangle(cr, cx - 19, bottom - 7, 38, 7);
  setRgba(cr, rgb(70, 46, 30), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.4);
  roundRect(cr, back ? cx + 4 : cx - 16, bottom - 6, 12, 12, 2);
  setRgba(cr, rgb(110, 76, 46), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.2);
  circle(cr, back ? cx - 10 : cx + 12, bottom + 2, 3.5);
  stroke(cr, kSteel, 1.8);
}

void climberHead(cairo_t* cr, double cx, double cy, bool back)
{
  circle(cr, cx, cy, 12);
  vgrad(cr, cy - 12, cy + 12, {{0.0, lighten(kSkin, 0.15f)}, {1.0, darken(kSkin, 0.2f)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  if (back)
  {
    // The back of the neck, the strap of the hat.
    cairo_move_to(cr, cx - 9, cy + 2);
    cairo_line_to(cr, cx + 9, cy + 2);
    stroke(cr, rgb(90, 60, 40), 2.0, 0.8);
    hardHat(cr, cx, cy - 4, 1.0, kWhite, true, true);
    return;
  }
  // Safety glasses, a nose, a grim mouth, stubble.
  roundRect(cr, cx + 1, cy - 4, 13, 6, 2);
  setRgba(cr, rgb(40, 60, 80), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.2);
  cairo_move_to(cr, cx + 4, cy - 3);
  cairo_line_to(cr, cx + 8, cy - 3);
  stroke(cr, rgb(200, 230, 255), 1.0, 0.9);
  cairo_move_to(cr, cx + 12, cy + 2);
  cairo_line_to(cr, cx + 14, cy + 5);
  stroke(cr, darken(kSkin, 0.35f), 1.4);
  cairo_move_to(cr, cx + 5, cy + 8);
  cairo_line_to(cr, cx + 11, cy + 7);
  stroke(cr, kInk, 1.4);
  for (int k = 0; k < 6; ++k)
  {
    circle(cr, cx + 1 + k * 2.0, cy + 9 + (k % 2), 0.6);
    setRgba(cr, rgb(90, 60, 40), 0.6);
    cairo_fill(cr);
  }
  // The chin strap.
  cairo_move_to(cr, cx - 6, cy - 4);
  cairo_line_to(cr, cx - 2, cy + 10);
  stroke(cr, rgb(40, 40, 44), 1.4);
  hardHat(cr, cx + 1, cy - 4, 1.0, kWhite, true);
}

void climberWalk(cairo_t* cr, int variant, int frame)
{
  const double cx = 46;
  const bool falling = variant == 6;
  const double stride = frame % 2 ? 1.0 : -1.0;
  // Legs.
  if (falling)
  {
    limb(cr, {{cx - 6, 94}, {cx - 18, 118}, {cx - 12, 140}}, 11, kNavyLo);
    limb(cr, {{cx + 6, 94}, {cx + 20, 112}, {cx + 26, 136}}, 11, kNavy);
    climberBoot(cr, cx - 10, 148);
    climberBoot(cr, cx + 28, 144);
  }
  else
  {
    limb(cr, {{cx - 4, 94}, {cx - 4 - 8 * stride, 124}, {cx - 6 - 12 * stride, 150}}, 11, kNavyLo);
    climberBoot(cr, cx - 4 - 12 * stride, 160);
    limb(cr, {{cx + 4, 94}, {cx + 4 + 8 * stride, 124}, {cx + 6 + 12 * stride, 150}}, 11, kNavy);
    climberBoot(cr, cx + 8 + 12 * stride, 160);
  }
  // The far arm.
  if (falling)
    limb(cr, {{cx - 12, 58}, {cx - 26, 40}, {cx - 30, 20}}, 9, kVestLo);
  else
    limb(cr, {{cx - 12, 58}, {cx - 18, 76}, {cx - 14 + 6 * stride, 92}}, 9, kVestLo);
  climberTorso(cr, cx, 50, 98, false);
  // The near arm and the wrench.
  const double sx = cx + 10, sy = 58;
  if (variant == 1)
  {
    // Raised high behind the head, about to come down (the head in front
    // of it).
    limb(cr, {{sx, sy}, {sx + 6, 34}, {sx - 16, 10}}, 9, kVest);
    wrench(cr, sx - 38, -4, 0.55, 32, true);
    circle(cr, sx - 16, 10, 5);
    setRgba(cr, kSkin, 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.2);
  }
  climberHead(cr, cx + 2, 36, false);
  switch (variant)
  {
    case 1:
      break;
    case 2:
      // Brought down in front.
      limb(cr, {{sx, sy}, {sx + 16, sy + 4}, {sx + 30, sy + 12}}, 9, kVest);
      wrench(cr, sx + 52, sy + 26, kPi + 0.55, 30, false);
      circle(cr, sx + 30, sy + 12, 5);
      setRgba(cr, kSkin, 1.0);
      cairo_fill_preserve(cr);
      stroke(cr, kInk, 1.2);
      break;
    case 6:
      limb(cr, {{sx, sy}, {sx + 14, 36}, {sx + 18, 14}}, 9, kVest);
      wrench(cr, sx + 30, 0, kPi * 0.8, 30, false);
      break;
    default:
      // At its side, the jaw by its knee.
      limb(cr, {{sx, sy}, {sx + 6, 76}, {sx + 10, 90}}, 9, kVest);
      wrench(cr, sx + 18, 120 + 2 * stride, -kPi * 0.6, 30, false);
      circle(cr, sx + 10, 90, 5);
      setRgba(cr, kSkin, 1.0);
      cairo_fill_preserve(cr);
      stroke(cr, kInk, 1.2);
      break;
  }
}

// Seen from behind on a ladder: hands up on the rungs, one higher than the
// other by turns, the wrench hooked on its belt.
void climberClimb(cairo_t* cr, int frame)
{
  const double cx = 48;
  const double up = frame % 2 ? 1.0 : -1.0;
  limb(cr, {{cx - 8, 94}, {cx - 12, 120 - 10 * up}, {cx - 10, 146 - 10 * up}}, 11, kNavyLo);
  limb(cr, {{cx + 8, 94}, {cx + 12, 120 + 10 * up}, {cx + 10, 146 + 10 * up}}, 11, kNavy);
  climberBoot(cr, cx - 12, 154 - 10 * up);
  climberBoot(cr, cx + 10, 154 + 10 * up);
  climberTorso(cr, cx, 50, 98, true);
  wrench(cr, cx + 22, 104, kPi * 0.55, 26, false);
  for (int side = -1; side <= 1; side += 2)
  {
    const double lift = side * up * 12.0;
    limb(cr, {{cx + side * 14, 56}, {cx + side * 24, 38 + lift}, {cx + side * 20, 14 + lift}}, 9, kVest);
    circle(cr, cx + side * 20, 12 + lift, 5);
    setRgba(cr, kSkin, 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.2);
  }
  climberHead(cr, cx, 38, true);
}

// The Copycat dummy: a crash-test mannequin, pale yellow with black joints
// and the quartered target marks, an orange hard hat.
void dummyMark(cairo_t* cr, double x, double y, double r)
{
  circle(cr, x, y, r);
  setRgba(cr, rgb(250, 250, 240), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.2);
  for (int q = 0; q < 2; ++q)
  {
    cairo_move_to(cr, x, y);
    cairo_arc(cr, x, y, r, q * kPi, q * kPi + kPi * 0.5);
    cairo_close_path(cr);
    setRgba(cr, kInk, 1.0);
    cairo_fill(cr);
  }
}

void dummy(cairo_t* cr, int variant, int frame)
{
  const Color body = rgb(244, 214, 90), bodyLo = rgb(190, 150, 50);
  const double cx = 46;
  const bool air = variant == 5;
  const double stride = frame % 2 ? 1.0 : -1.0;
  if (air)
  {
    limb(cr, {{cx - 6, 94}, {cx - 14, 112}, {cx - 4, 128}}, 11, bodyLo);
    limb(cr, {{cx + 6, 94}, {cx + 16, 110}, {cx + 10, 128}}, 11, body);
  }
  else
  {
    limb(cr, {{cx - 4, 94}, {cx - 4 - 8 * stride, 124}, {cx - 6 - 12 * stride, 156}}, 11, bodyLo);
    limb(cr, {{cx + 4, 94}, {cx + 4 + 8 * stride, 124}, {cx + 6 + 12 * stride, 156}}, 11, body);
  }
  // Knee joints.
  for (const double k : {-1.0, 1.0})
  {
    circle(cr, cx + k * 4 + (air ? k * 10 : k * 8 * stride), air ? 111 : 124, 4);
    setRgba(cr, rgb(40, 40, 46), 1.0);
    cairo_fill(cr);
  }
  if (air)
    limb(cr, {{cx - 12, 58}, {cx - 22, 36}, {cx - 20, 14}}, 9, bodyLo);
  else
    limb(cr, {{cx - 12, 58}, {cx - 18, 76}, {cx - 14 + 6 * stride, 92}}, 9, bodyLo);
  roundRect(cr, cx - 17, 50, 34, 46, 10);
  vgrad(cr, 50, 96, {{0.0, lighten(body, 0.3f)}, {1.0, bodyLo}}, 1.0, true);
  stroke(cr, kInk, 1.8);
  cairo_move_to(cr, cx - 16, 80);
  cairo_line_to(cr, cx + 16, 80);
  stroke(cr, rgb(40, 40, 46), 2.0);
  dummyMark(cr, cx + 4, 66, 7);
  // The head: an egg with a target on its side.
  ellipse(cr, cx + 2, 34, 12, 14);
  vgrad(cr, 20, 48, {{0.0, lighten(body, 0.35f)}, {1.0, bodyLo}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  dummyMark(cr, cx - 2, 36, 5);
  hardHat(cr, cx + 2, 24, 1.0, rgb(255, 130, 30), false);
  if (air)
    limb(cr, {{cx + 10, 58}, {cx + 20, 36}, {cx + 18, 14}}, 9, body);
  else
    limb(cr, {{cx + 10, 58}, {cx + 16, 76}, {cx + 12 - 6 * stride, 92}}, 9, body);
}

void mastClimber(cairo_t* cr, double w, double h, int variant, int frame)
{
  cairo_save(cr);
  frameBox(cr, w, h, 96, 160);
  if (variant == 3)
    climberClimb(cr, frame);
  else if (variant == 4 || variant == 5)
    dummy(cr, variant, frame);
  else
    climberWalk(cr, variant, frame);
  cairo_restore(cr);
}

// --- Relay Drone --------------------------------------------------------------------------------------

// A rotor seen from the side: its hub at (x, y), the blade's ends
// foreshortened by the frame; a faint disk of blur under it.
void sideRotor(cairo_t* cr, double x, double y, double span, int frame, double a)
{
  ellipse(cr, x, y, span, 3.0);
  setRgba(cr, rgb(200, 220, 240), 0.25 * a);
  cairo_fill(cr);
  const double c = std::cos(double(frame) * kPi * 0.25 + (x > 64 ? 0.7 : 0.0));
  cairo_move_to(cr, x - span * c, y);
  cairo_line_to(cr, x + span * c, y);
  stroke(cr, kInk, 4.0, a);
  cairo_move_to(cr, x - span * c, y);
  cairo_line_to(cr, x + span * c, y);
  stroke(cr, rgb(60, 64, 76), 2.2, a);
  roundRect(cr, x - 3, y - 2, 6, 8, 2);
  setRgba(cr, kSteelLo, a);
  cairo_fill(cr);
}

void relayDrone(cairo_t* cr, double w, double h, int variant, int frame)
{
  cairo_save(cr);
  frameBox(cr, w, h, 128, 96);
  const bool tell = variant == 1;
  // The far rotors, dim behind.
  sideRotor(cr, 30, 30, 20, frame + 1, 0.45);
  sideRotor(cr, 100, 30, 20, frame + 3, 0.45);
  // The arms out to the rotors.
  for (const double x : {14.0, 114.0})
  {
    cairo_move_to(cr, 64, 46);
    cairo_line_to(cr, x, 32);
    stroke(cr, kInk, 6.0);
    cairo_move_to(cr, 64, 46);
    cairo_line_to(cr, x, 32);
    stroke(cr, rgb(210, 214, 224), 3.6);
    cairo_rectangle(cr, x - 5, 30, 10, 10);
    vgrad(cr, 30, 40, {{0.0, rgb(120, 126, 140)}, {1.0, rgb(50, 54, 66)}}, 1.0, true);
    stroke(cr, kInk, 1.2);
  }
  // The whip antenna, its red tip; the little relay dish.
  cairo_move_to(cr, 48, 38);
  cairo_line_to(cr, 42, 8);
  stroke(cr, kInk, 2.6);
  cairo_move_to(cr, 48, 38);
  cairo_line_to(cr, 42, 8);
  stroke(cr, rgb(200, 204, 214), 1.2);
  circle(cr, 42, 7, 2.4);
  setRgba(cr, kRed, 1.0);
  cairo_fill(cr);
  cairo_save(cr);
  cairo_translate(cr, 76, 30);
  cairo_rotate(cr, -0.5);
  ellipse(cr, 0, 0, 5, 12);
  vgrad(cr, -12, 12, {{0.0, rgb(255, 255, 255)}, {1.0, rgb(170, 176, 190)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  cairo_move_to(cr, 0, 0);
  cairo_line_to(cr, 10, 0);
  stroke(cr, kInk, 1.4);
  cairo_restore(cr);
  // The fuselage.
  roundRect(cr, 32, 36, 64, 28, 12);
  vgrad(cr, 36, 64, {{0.0, rgb(255, 255, 255)}, {0.5, kShell}, {1.0, rgb(110, 116, 130)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  cairo_move_to(cr, 36, 52);
  cairo_line_to(cr, 92, 52);
  stroke(cr, rgb(214, 50, 40), 3.0);
  // The camera eye.
  circle(cr, 86, 46, 6);
  setRgba(cr, rgb(20, 24, 34), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.2);
  glow(cr, 86, 46, 7, tell ? rgb(255, 60, 40) : rgb(120, 220, 255), 0.9);
  circle(cr, 84, 44, 1.6);
  setRgba(cr, rgb(255, 255, 255), 0.9);
  cairo_fill(cr);
  // The status lamp.
  circle(cr, 44, 44, 2.6);
  setRgba(cr, tell ? rgb(255, 60, 40) : rgb(100, 255, 140), 1.0);
  cairo_fill(cr);
  // Landing skids.
  cairo_move_to(cr, 44, 64);
  cairo_line_to(cr, 40, 76);
  cairo_line_to(cr, 84, 76);
  cairo_line_to(cr, 80, 64);
  stroke(cr, kInk, 3.6);
  cairo_move_to(cr, 44, 64);
  cairo_line_to(cr, 40, 76);
  cairo_line_to(cr, 84, 76);
  cairo_line_to(cr, 80, 64);
  stroke(cr, rgb(120, 126, 140), 1.8);
  // The blaster pod under its nose.
  roundRect(cr, 84, 60, 30, 12, 5);
  vgrad(cr, 60, 72, {{0.0, rgb(110, 116, 130)}, {1.0, rgb(40, 44, 54)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  cairo_rectangle(cr, 112, 63, 8, 6);
  setRgba(cr, rgb(40, 44, 54), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.2);
  if (tell)
  {
    glow(cr, 121, 66, 18, rgb(255, 120, 80), 0.9);
    glow(cr, 121, 66, 8, rgb(255, 255, 230), 1.0);
  }
  // The near rotors.
  sideRotor(cr, 14, 26, 22, frame, 1.0);
  sideRotor(cr, 114, 26, 22, frame + 2, 1.0);
  cairo_restore(cr);
}

// --- Feedback Node --------------------------------------------------------------------------------------

void feedbackNode(cairo_t* cr, double w, double h, int variant, int frame)
{
  cairo_save(cr);
  frameBox(cr, w, h, 128, 128);
  const double fl = frame % 2 ? 1.0 : 0.85;
  Color hot = rgb(120, 140, 170);
  if (variant == 1)
    hot = rgb(255, 255, 255);
  else if (variant == 2)
    hot = rgb(140, 230, 255);
  else if (variant == 3)
    hot = rgb(255, 110, 60);
  // Its glow behind.
  if (variant == 0)
    glow(cr, 64, 30, 40, rgb(120, 200, 255), 0.25 * fl);
  else
    glow(cr, 64, 30, 64, hot, (variant == 3 ? 0.35 : 0.7) * fl);
  // The horn speakers on its sides.
  for (int side = -1; side <= 1; side += 2)
  {
    cairo_move_to(cr, 64 + side * 30, 100);
    cairo_line_to(cr, 64 + side * 50, 90);
    cairo_line_to(cr, 64 + side * 58, 84);
    cairo_line_to(cr, 64 + side * 58, 122);
    cairo_line_to(cr, 64 + side * 50, 116);
    cairo_line_to(cr, 64 + side * 30, 110);
    cairo_close_path(cr);
    hgrad(cr, 64 + side * 30, 64 + side * 58, {{0.0, rgb(90, 96, 110)}, {1.0, rgb(200, 206, 218)}}, 1.0, true);
    stroke(cr, kInk, 1.8);
    ellipse(cr, 64 + side * 58, 103, 3, 19);
    setRgba(cr, rgb(30, 32, 40), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.2);
  }
  // The cabinet: steel with cooling fins, a hazard stripe, bolted down.
  roundRect(cr, 32, 88, 64, 40, 4);
  vgrad(cr, 88, 128, {{0.0, rgb(150, 156, 170)}, {1.0, rgb(60, 64, 76)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  for (double x = 38; x < 92; x += 6)
  {
    cairo_move_to(cr, x, 96);
    cairo_line_to(cr, x, 114);
    stroke(cr, rgb(40, 42, 52), 2.0, 0.8);
  }
  cairo_save(cr);
  cairo_rectangle(cr, 32, 118, 64, 6);
  cairo_clip(cr);
  for (double x = 26; x < 100; x += 10)
  {
    cairo_move_to(cr, x, 124);
    cairo_line_to(cr, x + 6, 118);
    cairo_line_to(cr, x + 11, 118);
    cairo_line_to(cr, x + 5, 124);
    cairo_close_path(cr);
    setRgba(cr, rgb(250, 200, 30), 1.0);
    cairo_fill(cr);
  }
  cairo_restore(cr);
  // Its gauge light.
  circle(cr, 86, 92, 2.4);
  setRgba(cr, variant == 1 ? rgb(255, 80, 60) : rgb(120, 255, 160), 1.0);
  cairo_fill(cr);
  // The insulator stack.
  for (int k = 0; k < 4; ++k)
  {
    const double y = 84 - k * 9;
    ellipse(cr, 64, y, 13 - k * 0.6, 4.2);
    vgrad(cr, y - 4, y + 4, {{0.0, rgb(250, 246, 236)}, {1.0, rgb(170, 150, 120)}}, 1.0, true);
    stroke(cr, kInk, 1.2);
  }
  cairo_rectangle(cr, 60, 50, 8, 34);
  setRgba(cr, rgb(120, 110, 100), 0.5);
  cairo_fill(cr);
  // The coil ring round the sphere (its back half first).
  const Color copper = variant == 0 ? rgb(200, 120, 60) : lerpColor(rgb(200, 120, 60), hot, variant == 3 ? 0.5f : 0.8f);
  cairo_save(cr);
  cairo_translate(cr, 64, 34);
  cairo_scale(cr, 34, 9);
  cairo_arc(cr, 0, 0, 1, kPi, 2 * kPi);
  cairo_restore(cr);
  stroke(cr, kInk, 7.0);
  cairo_save(cr);
  cairo_translate(cr, 64, 34);
  cairo_scale(cr, 34, 9);
  cairo_arc(cr, 0, 0, 1, kPi, 2 * kPi);
  cairo_restore(cr);
  stroke(cr, copper, 4.0);
  // The sphere.
  circle(cr, 64, 30, 18);
  {
    cairo_pattern_t* g = cairo_pattern_create_radial(58, 24, 2, 64, 30, 18);
    if (variant == 0)
    {
      stop(g, 0.0, rgb(230, 236, 246));
      stop(g, 0.5, rgb(130, 140, 160));
      stop(g, 1.0, rgb(50, 54, 70));
    }
    else
    {
      stop(g, 0.0, rgb(255, 255, 255));
      stop(g, 0.6, hot);
      stop(g, 1.0, darken(hot, 0.35f));
    }
    cairo_set_source(cr, g);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(g);
  }
  stroke(cr, kInk, 2.0);
  // Its front half.
  cairo_save(cr);
  cairo_translate(cr, 64, 34);
  cairo_scale(cr, 34, 9);
  cairo_arc(cr, 0, 0, 1, 0, kPi);
  cairo_restore(cr);
  stroke(cr, kInk, 7.0);
  cairo_save(cr);
  cairo_translate(cr, 64, 34);
  cairo_scale(cr, 34, 9);
  cairo_arc(cr, 0, 0, 1, 0, kPi);
  cairo_restore(cr);
  stroke(cr, copper, 4.0);
  for (double a = 0.3; a < kPi; a += 0.45)
  {
    cairo_move_to(cr, 64 + 34 * std::cos(a), 34 + 9 * std::sin(a) - 3);
    cairo_line_to(cr, 64 + 34 * std::cos(a), 34 + 9 * std::sin(a) + 3);
    stroke(cr, darken(copper, 0.4f), 1.2);
  }
  // The pulse: arcs crackling off the sphere.
  if (variant == 2 || variant == 1)
  {
    Rng rng(35201u + std::uint32_t(frame));
    for (int k = 0; k < (variant == 2 ? 5 : 2); ++k)
    {
      const double a = rng.range(0.0f, 6.28f);
      double x = 64 + std::cos(a) * 18, y = 30 + std::sin(a) * 18;
      cairo_move_to(cr, x, y);
      for (int s = 0; s < 4; ++s)
      {
        x += std::cos(a) * 8 + rng.range(-5, 5);
        y += std::sin(a) * 8 + rng.range(-5, 5);
        cairo_line_to(cr, x, y);
      }
      stroke(cr, rgb(200, 240, 255), 1.8, 0.9);
    }
  }
  cairo_restore(cr);
}

// --- The Halcyon Titan ----------------------------------------------------------------------------------

// A glossy white shell plate: a cool shadow under, a bright rim on top.
void shell(cairo_t* cr, double y0, double y1, double ink = 2.4)
{
  vgrad(cr, y0, y1, {{0.0, rgb(255, 255, 255)}, {0.35, kWhite}, {0.8, kShell}, {1.0, rgb(130, 138, 156)}}, 1.0, true);
  stroke(cr, kInk, ink);
}

void titanLeg(cairo_t* cr, double hipX, double hipY, double kneeX, double kneeY, double footX, bool far)
{
  const Color tint = far ? rgb(150, 156, 172) : rgb(255, 255, 255);
  // Thigh and shin as tapered blocks, a knee joint, a big foot.
  cairo_move_to(cr, hipX - 20, hipY);
  cairo_line_to(cr, hipX + 20, hipY);
  cairo_line_to(cr, kneeX + 16, kneeY);
  cairo_line_to(cr, kneeX - 16, kneeY);
  cairo_close_path(cr);
  vgrad(cr, hipY, kneeY, {{0.0, lerpColor(kWhite, tint, 0.5f)}, {1.0, lerpColor(kShell, tint, 0.3f)}}, 1.0, true);
  stroke(cr, kInk, 2.4);
  cairo_move_to(cr, kneeX - 18, kneeY);
  cairo_line_to(cr, kneeX + 18, kneeY);
  cairo_line_to(cr, footX + 14, 350);
  cairo_line_to(cr, footX - 14, 350);
  cairo_close_path(cr);
  vgrad(cr, kneeY, 350, {{0.0, far ? rgb(150, 156, 172) : kWhite}, {1.0, far ? rgb(90, 96, 112) : kShell}}, 1.0, true);
  stroke(cr, kInk, 2.4);
  // The blue stripe down the shin.
  cairo_move_to(cr, kneeX + 6, kneeY + 10);
  cairo_line_to(cr, footX + 6, 340);
  stroke(cr, far ? darken(kHalcyon, 0.4f) : kHalcyon, 4.0);
  circle(cr, kneeX, kneeY, 15);
  vgrad(cr, kneeY - 15, kneeY + 15, {{0.0, rgb(120, 126, 140)}, {1.0, rgb(40, 44, 56)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  // The foot.
  cairo_move_to(cr, footX - 30, 384);
  cairo_line_to(cr, footX - 26, 352);
  cairo_line_to(cr, footX + 24, 348);
  cairo_line_to(cr, footX + 44, 368);
  cairo_line_to(cr, footX + 46, 384);
  cairo_close_path(cr);
  vgrad(cr, 348, 384, {{0.0, far ? rgb(140, 146, 160) : kWhite}, {1.0, far ? rgb(70, 74, 90) : rgb(120, 128, 146)}}, 1.0, true);
  stroke(cr, kInk, 2.4);
  cairo_rectangle(cr, footX - 30, 378, 76, 6);
  setRgba(cr, rgb(40, 44, 56), 1.0);
  cairo_fill(cr);
}

void missilePod(cairo_t* cr, double x, double y, bool far)
{
  roundRect(cr, x, y, 64, 40, 8);
  if (far)
  {
    vgrad(cr, y, y + 40, {{0.0, rgb(170, 176, 190)}, {1.0, rgb(100, 106, 122)}}, 1.0, true);
    stroke(cr, kInk, 2.2);
  }
  else
    shell(cr, y, y + 40);
  for (int k = 0; k < 4; ++k)
  {
    circle(cr, x + 12 + (k % 2) * 20 + (k / 2) * 10, y + 13 + (k / 2) * 14, 6);
    setRgba(cr, rgb(30, 34, 46), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.2);
    circle(cr, x + 12 + (k % 2) * 20 + (k / 2) * 10, y + 13 + (k / 2) * 14, 3);
    setRgba(cr, kRed, 0.9);
    cairo_fill(cr);
  }
}

void titan(cairo_t* cr, double w, double h, int variant)
{
  cairo_save(cr);
  frameBox(cr, w, h, 320, 384);
  const double drop = variant == 1 ? double(kTitanCrouch) : 0.0;
  // Legs: the stride (variants 2, 3) swings them.
  const double s = variant == 2 ? 1.0 : (variant == 3 ? -1.0 : 0.0);
  const double kneeY = 300 + drop * 0.6;
  titanLeg(cr, 120, 262 + drop, 112 - 14 * s, kneeY, 110 - 26 * s, true);
  // The flamethrower arm (the far one), reaching forward at hip height.
  limb(cr, {{140, 160 + drop}, {176, 236 + drop}, {236, 268 + drop}}, 22, rgb(150, 156, 172));
  roundRect(cr, 226, 270 + drop, 70, 26, 8);
  vgrad(cr, 270 + drop, 296 + drop, {{0.0, rgb(130, 136, 150)}, {1.0, rgb(50, 54, 66)}}, 1.0, true);
  stroke(cr, kInk, 2.2);
  // Its fuel tank and nozzle.
  roundRect(cr, 234, 296 + drop, 36, 16, 7);
  vgrad(cr, 296 + drop, 312 + drop, {{0.0, rgb(255, 150, 60)}, {1.0, rgb(160, 60, 20)}}, 1.0, true);
  stroke(cr, kInk, 1.8);
  cairo_rectangle(cr, 290, 278 + drop, 12, 16);
  setRgba(cr, rgb(40, 40, 50), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.6);
  // The far missile pod.
  missilePod(cr, 166, 76 + drop, true);
  // The reactor pack on its back.
  roundRect(cr, 18, 120 + drop, 64, 124, 16);
  vgrad(cr, 120 + drop, 244 + drop, {{0.0, rgb(200, 206, 220)}, {1.0, rgb(96, 102, 120)}}, 1.0, true);
  stroke(cr, kInk, 2.4);
  for (int k = 0; k < 5; ++k)
  {
    cairo_move_to(cr, 24, 200 + drop + k * 8);
    cairo_line_to(cr, 76, 200 + drop + k * 8);
    stroke(cr, rgb(50, 54, 68), 2.2);
  }
  circle(cr, kTitanPackX, kTitanPackY + drop, 14);
  setRgba(cr, rgb(30, 50, 80), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 2.0);
  glow(cr, kTitanPackX, kTitanPackY + drop, 12, rgb(255, 170, 70), 0.6);
  // The near leg.
  titanLeg(cr, 190, 262 + drop, 196 + 14 * s, kneeY, 200 + 26 * s, false);
  // The hips.
  roundRect(cr, 84, 236 + drop, 170, 46, 14);
  shell(cr, 236 + drop, 282 + drop);
  // The torso: a big rounded chest, the blue Halcyon chevron.
  cairo_move_to(cr, 80, 120 + drop);
  cairo_curve_to(cr, 90, 100 + drop, 230, 96 + drop, 256, 116 + drop);
  cairo_curve_to(cr, 270, 160 + drop, 262, 220 + drop, 238, 248 + drop);
  cairo_line_to(cr, 100, 248 + drop);
  cairo_curve_to(cr, 80, 220 + drop, 72, 160 + drop, 80, 120 + drop);
  cairo_close_path(cr);
  shell(cr, 100 + drop, 248 + drop, 3.0);
  cairo_move_to(cr, 110, 130 + drop);
  cairo_line_to(cr, 150, 150 + drop);
  cairo_line_to(cr, 110, 170 + drop);
  stroke(cr, kHalcyon, 7.0);
  cairo_move_to(cr, 96, 226 + drop);
  cairo_line_to(cr, 244, 226 + drop);
  stroke(cr, kHalcyon, 5.0);
  // Panel seams on the chest.
  cairo_move_to(cr, 160, 108 + drop);
  cairo_line_to(cr, 160, 132 + drop);
  stroke(cr, rgb(140, 146, 164), 1.6);
  // The power core's shutter.
  circle(cr, kTitanCoreX, kTitanCoreY + drop, kTitanCoreR + 6);
  vgrad(cr, kTitanCoreY + drop - 32, kTitanCoreY + drop + 32, {{0.0, rgb(150, 156, 172)}, {1.0, rgb(60, 64, 80)}}, 1.0, true);
  stroke(cr, kInk, 2.4);
  for (int k = 0; k < 6; ++k)
  {
    const double a = k * kPi / 3.0;
    cairo_move_to(cr, kTitanCoreX, kTitanCoreY + drop);
    cairo_line_to(cr, kTitanCoreX + std::cos(a) * kTitanCoreR, kTitanCoreY + drop + std::sin(a) * kTitanCoreR);
    stroke(cr, rgb(40, 44, 58), 2.0);
  }
  circle(cr, kTitanCoreX, kTitanCoreY + drop, kTitanCoreR);
  stroke(cr, rgb(40, 44, 58), 2.0);
  // The vent panels' outlines.
  for (int i = 0; i < 4; ++i)
  {
    const auto& p = kTitanPanel[i];
    const double dy = p[1] < 236 ? drop : 0.0;
    roundRect(cr, p[0], p[1] + dy, p[2], p[3], 4);
    setRgba(cr, rgb(170, 178, 196), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.8);
    for (double y = p[1] + dy + 6; y < p[1] + dy + p[3] - 4; y += 6)
    {
      cairo_move_to(cr, p[0] + 4, y);
      cairo_line_to(cr, p[0] + p[2] - 4, y);
      stroke(cr, rgb(90, 96, 112), 1.4);
    }
  }
  // The lettering on its chest.
  selectGameFont(cr);
  cairo_set_font_size(cr, 15);
  cairo_move_to(cr, 102, 210 + drop);
  setRgba(cr, rgb(40, 60, 110), 0.9);
  cairo_show_text(cr, "HALCYON");
  // The head: a dome with a visor slit.
  cairo_move_to(cr, 150, 108 + drop);
  cairo_curve_to(cr, 146, 50 + drop, 230, 48 + drop, 228, 108 + drop);
  cairo_close_path(cr);
  shell(cr, 54 + drop, 108 + drop);
  roundRect(cr, 176, 76 + drop, 50, 14, 7);
  setRgba(cr, rgb(20, 26, 40), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.6);
  glow(cr, kTitanEyeX, kTitanEyeY + drop, 16, rgb(120, 200, 255), 0.9);
  roundRect(cr, 186, 80 + drop, 30, 6, 3);
  setRgba(cr, rgb(200, 240, 255), 1.0);
  cairo_fill(cr);
  // The near missile pod.
  missilePod(cr, kTitanPodX - 30, 62 + drop, false);
  // The laser arm (the near one): shoulder, elbow, the cannon at a runner's
  // head height.
  circle(cr, 240, 132 + drop, 26);
  shell(cr, 106 + drop, 158 + drop);
  limb(cr, {{240, 140 + drop}, {262, 192 + drop * 0.5}, {262, 222}}, 24, kShell);
  roundRect(cr, 236, 214, 80, 30, 10);
  shell(cr, 214, 244);
  cairo_rectangle(cr, 300, 220, 16, 18);
  setRgba(cr, rgb(30, 36, 50), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.6);
  cairo_move_to(cr, 244, 236);
  cairo_line_to(cr, 300, 236);
  stroke(cr, kHalcyon, 3.0);
  cairo_restore(cr);
}

// --- The duck's hard hat ----------------------------------------------------------------------------------

void duckHardHat(cairo_t* cr, double w, double h, int variant)
{
  if (variant != 1)
    return;
  cairo_save(cr);
  cairo_translate(cr, kM, kM);
  cairo_scale(cr, w / 64.0, h / 64.0);
  // The little beacon on top.
  cairo_rectangle(cr, 39, 0, 6, 5);
  setRgba(cr, rgb(255, 60, 40), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 0.9);
  glow(cr, 42, 2, 7, rgb(255, 80, 60), 0.6);
  hardHat(cr, 42, 19, 1.05, rgb(255, 210, 40), false);
  cairo_restore(cr);
}

} // namespace

bool drawTower35Art(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame)
{
  (void)t;
  if (key == "mast_climber")
    mastClimber(cr, w, h, std::clamp(variant, 0, 6), frame % 2);
  else if (key == "relay_drone")
    relayDrone(cr, w, h, std::clamp(variant, 0, 1), frame % 4);
  else if (key == "feedback_node")
    feedbackNode(cr, w, h, std::clamp(variant, 0, 3), frame % 2);
  else if (key == "halcyon_titan")
    titan(cr, w, h, std::clamp(variant, 0, 3));
  else if (key == "duck_hard_hat")
    duckHardHat(cr, w, h, variant);
  else
    return false;
  return true;
}

} // namespace gr
