// The Gallery (Level 39) in the style of enemy_art.cpp: smooth shapes with
// a dark ink line, lit cold from the monitors (top left) with a warm fill
// from the studio lamps. The crew: the Floor Manager in studio blacks with
// his headset and clipboard, counting down on his fingers; the Cable
// Wrangler, stocky in a hi-vis vest, a coil of cable over his shoulder;
// the Boom Op in big headphones with the sound bag on his chest, hands up
// on the boom. The duck wears studio headphones. Each is drawn in screen
// pixels into a texture with a 32 px margin, the cell box at (32, 32) ..
// (32 + w, 32 + h), facing right.

#include "assets/enemy_art_gallery39.hpp"

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
constexpr Color kInk = rgb(14, 14, 22);
constexpr Color kBlack = rgb(36, 38, 46);
constexpr Color kCargo = rgb(70, 68, 58);
constexpr Color kCargoHi = rgb(116, 112, 96);
constexpr Color kSkinA = rgb(176, 120, 84);
constexpr Color kSkinB = rgb(236, 190, 160);
constexpr Color kSkinC = rgb(110, 70, 50);
constexpr Color kHiVis = rgb(255, 150, 20);
constexpr Color kHiVisHi = rgb(255, 214, 110);
constexpr Color kReflect = rgb(230, 236, 240);
constexpr Color kHoodie = rgb(80, 86, 100);
constexpr Color kGlove = rgb(190, 150, 60);
constexpr Color kCable = rgb(28, 28, 34);
constexpr Color kSteel = rgb(160, 166, 178);
constexpr Color kTally = rgb(240, 40, 40);

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

// Fills the path with a gradient from the top-left light to the shade.
void lit(cairo_t* cr, double x0, double y0, double x1, double y1, Color c, bool preserve = true)
{
  cairo_pattern_t* g = cairo_pattern_create_linear(x0, y0, x1, y1);
  stop(g, 0.0, lighten(c, 0.22f));
  stop(g, 0.45, c);
  stop(g, 1.0, darken(c, 0.35f));
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

// A limb: an ink outline under a coloured stroke through the points, a
// cold highlight along its top.
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

void shoe(cairo_t* cr, double x, double y, Color c)
{
  cairo_move_to(cr, x - 9, y);
  cairo_line_to(cr, x - 9, y - 8);
  cairo_curve_to(cr, x - 2, y - 11, x + 6, y - 9, x + 13, y - 3);
  cairo_line_to(cr, x + 13, y);
  cairo_close_path(cr);
  lit(cr, x - 9, y - 10, x + 10, y, c);
  ink(cr, 1.6);
  cairo_rectangle(cr, x - 9, y - 2.2, 22, 2.2);
  setRgba(cr, rgb(230, 230, 230), 0.9);
  cairo_fill(cr);
}

// A face in three-quarter view looking right: the head at (cx, cy), radius
// r, an eye, a brow, a nose, a mouth (`mouth` 0 set, 1 open shouting).
void face(cairo_t* cr, double cx, double cy, double r, Color skin, int mouth, double brow)
{
  circle(cr, cx, cy, r);
  lit(cr, cx - r, cy - r, cx + r * 0.8, cy + r, skin);
  ink(cr, 2.0);
  // The ear.
  ellipse(cr, cx - r * 0.35, cy + r * 0.05, r * 0.2, r * 0.28);
  lit(cr, cx - r * 0.6, cy - r * 0.3, cx, cy + r * 0.3, darken(skin, 0.08f));
  ink(cr, 1.2);
  // The eye and the brow.
  ellipse(cr, cx + r * 0.45, cy - r * 0.12, r * 0.15, r * 0.2);
  setRgba(cr, rgb(255, 255, 255), 1.0);
  cairo_fill_preserve(cr);
  ink(cr, 1.0);
  circle(cr, cx + r * 0.52, cy - r * 0.1, r * 0.09);
  setRgba(cr, kInk, 1.0);
  cairo_fill(cr);
  cairo_move_to(cr, cx + r * 0.25, cy - r * 0.38 - brow);
  cairo_line_to(cr, cx + r * 0.7, cy - r * 0.32 + brow);
  stroke(cr, darken(skin, 0.6f), 2.4);
  // The nose.
  cairo_move_to(cr, cx + r * 0.85, cy - r * 0.05);
  cairo_line_to(cr, cx + r * 1.08, cy + r * 0.2);
  cairo_line_to(cr, cx + r * 0.85, cy + r * 0.28);
  stroke(cr, darken(skin, 0.45f), 1.4);
  if (mouth == 1)
  {
    ellipse(cr, cx + r * 0.72, cy + r * 0.52, r * 0.18, r * 0.14);
    setRgba(cr, rgb(70, 20, 24), 1.0);
    cairo_fill_preserve(cr);
    ink(cr, 1.0);
  }
  else
  {
    cairo_move_to(cr, cx + r * 0.55, cy + r * 0.5);
    cairo_line_to(cr, cx + r * 0.85, cy + r * 0.47);
    stroke(cr, darken(skin, 0.55f), 1.6);
  }
}

// A headset: a thin band over the head, a cup on the ear, a boom to the
// mouth.
void headset(cairo_t* cr, double cx, double cy, double r)
{
  cairo_arc(cr, cx - r * 0.1, cy, r * 1.04, kPi * 1.1, kPi * 1.95);
  stroke(cr, kInk, 3.6);
  cairo_arc(cr, cx - r * 0.1, cy, r * 1.04, kPi * 1.1, kPi * 1.95);
  stroke(cr, rgb(70, 72, 84), 1.8);
  ellipse(cr, cx - r * 0.35, cy + r * 0.05, r * 0.3, r * 0.36);
  lit(cr, cx - r * 0.7, cy - r * 0.3, cx, cy + r * 0.4, rgb(40, 42, 50));
  ink(cr, 1.4);
  cairo_move_to(cr, cx - r * 0.2, cy + r * 0.25);
  cairo_curve_to(cr, cx + r * 0.2, cy + r * 0.85, cx + r * 0.6, cy + r * 0.8, cx + r * 0.85, cy + r * 0.62);
  stroke(cr, kInk, 2.6);
  circle(cr, cx + r * 0.88, cy + r * 0.6, r * 0.12);
  setRgba(cr, rgb(50, 50, 58), 1.0);
  cairo_fill(cr);
}

// --- Floor Manager --------------------------------------------------------------------------

void floorManager(cairo_t* cr, double w, double h, int variant, int frame)
{
  cairo_save(cr);
  frameBox(cr, w, h, 96, 160);
  ellipse(cr, 48, 157, 30, 4);
  setRgba(cr, kInk, 0.35);
  cairo_fill(cr);
  const double stride = variant == 0 ? (frame % 2 ? 7.0 : -7.0) : 0.0;
  // Legs: cargo trousers with a thigh pocket, sneakers.
  limb(cr, {{42, 94}, {42 - stride, 122}, {40 - stride, 146}}, 15, darken(kCargo, 0.15f));
  shoe(cr, 41 - stride, 156, rgb(30, 30, 36));
  limb(cr, {{54, 94}, {55 + stride, 122}, {56 + stride, 146}}, 15, kCargo);
  shoe(cr, 57 + stride, 156, rgb(40, 40, 48));
  cairo_rectangle(cr, 48 + stride * 0.5, 108, 12, 13);
  setRgba(cr, kCargoHi, 0.8);
  cairo_fill_preserve(cr);
  ink(cr, 1.0);
  // The torso: a black crew t-shirt.
  cairo_move_to(cr, 30, 52);
  cairo_curve_to(cr, 36, 44, 60, 43, 66, 52);
  cairo_line_to(cr, 66, 98);
  cairo_line_to(cr, 30, 98);
  cairo_close_path(cr);
  lit(cr, 30, 44, 70, 98, kBlack);
  ink(cr, 2.0);
  // CREW across the chest, faded.
  selectGameFont(cr);
  cairo_set_font_size(cr, 7.5);
  cairo_move_to(cr, 40, 68);
  setRgba(cr, rgb(200, 200, 210), 0.75);
  cairo_show_text(cr, "CREW");
  // Belt and the walkie-talkie on it, its lead up to the headset.
  cairo_rectangle(cr, 30, 93, 36, 6);
  setRgba(cr, rgb(20, 20, 24), 1.0);
  cairo_fill(cr);
  roundRect(cr, 24, 84, 11, 18, 2);
  lit(cr, 24, 84, 35, 102, rgb(50, 52, 60));
  ink(cr, 1.2);
  cairo_move_to(cr, 27, 84);
  cairo_line_to(cr, 27, 74);
  stroke(cr, kInk, 2.0);
  circle(cr, 32, 88, 1.4);
  setRgba(cr, rgb(80, 255, 120), 1.0);
  cairo_fill(cr);
  cairo_move_to(cr, 31, 84);
  cairo_curve_to(cr, 26, 70, 30, 46, 38, 34);
  stroke(cr, kInk, 1.2, 0.9);
  // The back arm holds the clipboard against the chest (pose 0, 1) or
  // down at the side.
  if (variant <= 1)
  {
    limb(cr, {{36, 54}, {34, 72}, {46, 78}}, 10, kBlack);
    cairo_save(cr);
    cairo_translate(cr, 48, 70);
    cairo_rotate(cr, -0.18);
    roundRect(cr, -10, -14, 20, 27, 2);
    lit(cr, -10, -14, 10, 13, rgb(170, 120, 70));
    ink(cr, 1.4);
    cairo_rectangle(cr, -7, -10, 14, 20);
    setRgba(cr, rgb(244, 242, 232), 1.0);
    cairo_fill(cr);
    for (int k = 0; k < 4; ++k)
    {
      cairo_move_to(cr, -5, -6 + k * 4.5);
      cairo_line_to(cr, 4 - (k % 2) * 3, -6 + k * 4.5);
    }
    stroke(cr, rgb(60, 60, 80), 0.9);
    cairo_rectangle(cr, -4, -16, 8, 4);
    setRgba(cr, kSteel, 1.0);
    cairo_fill(cr);
    cairo_restore(cr);
    hand(cr, 45, 78, 5, kSkinA);
  }
  else
  {
    limb(cr, {{36, 54}, {32, 76}, {34, 92}}, 10, kBlack);
    hand(cr, 34, 94, 5, kSkinA);
  }
  // The head, the headset, his lanyard.
  cairo_move_to(cr, 44, 46);
  cairo_line_to(cr, 50, 64);
  cairo_line_to(cr, 56, 46);
  stroke(cr, kTally, 1.6);
  cairo_rectangle(cr, 46.5, 63, 7, 9);
  setRgba(cr, rgb(240, 240, 240), 1.0);
  cairo_fill(cr);
  limb(cr, {{50, 44}, {50, 38}}, 9, kSkinA);
  const bool shout = variant >= 2;
  face(cr, 50, 26, 14, kSkinA, shout ? 1 : 0, variant >= 1 ? 1.5 : -0.5);
  // Short hair.
  cairo_arc(cr, 49, 24, 14.5, kPi * 1.02, kPi * 1.9);
  cairo_curve_to(cr, 60, 14, 46, 12, 36, 24);
  cairo_close_path(cr);
  setRgba(cr, rgb(30, 22, 18), 1.0);
  cairo_fill(cr);
  headset(cr, 50, 26, 14);
  // The front arm: walking, pointing, counting, cueing.
  if (variant == 0)
  {
    limb(cr, {{58, 54}, {62 - stride * 0.6, 74}, {60 - stride * 0.8, 90}}, 10, kBlack);
    hand(cr, 60 - stride * 0.8, 92, 5, kSkinA);
  }
  else if (variant == 1)
  {
    // Pointing straight at the runner, index finger out.
    limb(cr, {{58, 54}, {76, 52}, {90, 50}}, 9, kBlack);
    hand(cr, 91, 50, 5, kSkinA);
    limb(cr, {{93, 49}, {99, 48}}, 3, kSkinA);
  }
  else if (variant <= 4)
  {
    // The hand up beside his head, fingers counting.
    const int fingers = 5 - variant; // 3, 2, 1
    limb(cr, {{58, 54}, {74, 52}, {76, 30}}, 9, kBlack);
    roundRect(cr, 70, 16, 12, 14, 4);
    lit(cr, 70, 16, 82, 30, kSkinA);
    ink(cr, 1.4);
    for (int f = 0; f < fingers; ++f)
      limb(cr, {{72.5 + f * 3.6, 17}, {71.5 + f * 4.2, 6 - (f == 1 ? 2.0 : 0.0)}}, 2.6, kSkinA);
    // The folded thumb.
    limb(cr, {{70, 24}, {74, 22}}, 2.6, kSkinA);
  }
  else
  {
    // The cue: the arm swept down and out at the runner.
    limb(cr, {{58, 54}, {76, 66}, {92, 78}}, 9, kBlack);
    hand(cr, 93, 79, 5, kSkinA);
    for (int k = 0; k < 3; ++k)
    {
      cairo_move_to(cr, 78 - k * 6, 40 + k * 3);
      cairo_line_to(cr, 88 - k * 6, 52 + k * 3);
      stroke(cr, rgb(255, 255, 255), 1.4, 0.6);
    }
  }
  cairo_restore(cr);
}

// --- Cable Wrangler -------------------------------------------------------------------------

void wrangler(cairo_t* cr, double w, double h, int variant, int frame)
{
  cairo_save(cr);
  frameBox(cr, w, h, 128, 160);
  ellipse(cr, 62, 157, 40, 5);
  setRgba(cr, kInk, 0.35);
  cairo_fill(cr);
  const double stride = variant == 0 ? (frame % 2 ? 8.0 : -8.0) : 0.0;
  const double lean = variant == 1 ? -8.0 : 0.0; // leaning back on the yank
  // Legs: work trousers, boots.
  limb(cr, {{54, 98}, {52 - stride + lean * 0.5, 126}, {50 - stride + lean, 146}}, 18, rgb(54, 60, 72));
  shoe(cr, 52 - stride + lean, 156, rgb(70, 50, 36));
  limb(cr, {{70, 98}, {72 + stride, 126}, {74 + stride, 146}}, 18, rgb(64, 70, 84));
  shoe(cr, 76 + stride, 156, rgb(84, 60, 40));
  cairo_save(cr);
  cairo_translate(cr, lean, 0);
  // The back arm: dragging the cable (its hand at about (34, 104)).
  if (variant == 2)
    limb(cr, {{46, 56}, {34, 44}, {32, 30}}, 13, kHoodie);
  else if (variant == 1)
    limb(cr, {{46, 58}, {36, 82}, {30, 100}}, 13, kHoodie);
  else
    limb(cr, {{46, 58}, {40 - stride * 0.5, 82}, {36 - stride * 0.6, 100}}, 13, kHoodie);
  if (variant == 2)
    hand(cr, 32, 28, 7, kGlove);
  else
    hand(cr, variant == 1 ? 30 : 35 - stride * 0.6, 102, 7, kGlove);
  // The torso: a hoodie under a hi-vis vest, a belly.
  cairo_move_to(cr, 40, 58);
  cairo_curve_to(cr, 44, 44, 80, 42, 86, 58);
  cairo_curve_to(cr, 94, 76, 92, 96, 86, 102);
  cairo_line_to(cr, 42, 102);
  cairo_curve_to(cr, 38, 88, 38, 70, 40, 58);
  cairo_close_path(cr);
  lit(cr, 40, 44, 92, 102, kHoodie);
  ink(cr, 2.2);
  cairo_move_to(cr, 46, 54);
  cairo_curve_to(cr, 52, 48, 74, 47, 82, 54);
  cairo_curve_to(cr, 90, 74, 88, 92, 84, 100);
  cairo_line_to(cr, 46, 100);
  cairo_curve_to(cr, 43, 86, 43, 68, 46, 54);
  cairo_close_path(cr);
  lit(cr, 46, 48, 90, 100, kHiVis);
  ink(cr, 1.6);
  for (const double y : {72.0, 86.0})
  {
    cairo_move_to(cr, 45, y);
    cairo_curve_to(cr, 60, y + 1, 76, y + 1, 88, y - 1);
    stroke(cr, kReflect, 4.0);
    cairo_move_to(cr, 45, y);
    cairo_curve_to(cr, 60, y + 1, 76, y + 1, 88, y - 1);
    stroke(cr, rgb(255, 255, 255), 1.0, 0.8);
  }
  // A coil of cable over his shoulder.
  for (int k = 0; k < 3; ++k)
  {
    ellipse(cr, 58 + k * 2.0, 70, 16 - k, 24 - k, 0.35);
    stroke(cr, kInk, 5.4);
    ellipse(cr, 58 + k * 2.0, 70, 16 - k, 24 - k, 0.35);
    stroke(cr, k == 1 ? rgb(70, 72, 84) : kCable, 3.2);
  }
  // A roll of gaffer tape on his belt.
  cairo_rectangle(cr, 42, 98, 46, 6);
  setRgba(cr, rgb(30, 26, 22), 1.0);
  cairo_fill(cr);
  circle(cr, 84, 104, 7);
  setRgba(cr, rgb(170, 170, 176), 1.0);
  cairo_fill_preserve(cr);
  ink(cr, 1.4);
  circle(cr, 84, 104, 3);
  setRgba(cr, rgb(40, 40, 46), 1.0);
  cairo_fill(cr);
  // Head: beard, cap on backwards.
  limb(cr, {{66, 48}, {68, 40}}, 11, kSkinB);
  face(cr, 68, 28, 15, kSkinB, variant >= 1 ? 1 : 0, variant == 1 ? 2.0 : 0.0);
  cairo_move_to(cr, 56, 36);
  cairo_curve_to(cr, 60, 50, 80, 50, 84, 34);
  cairo_curve_to(cr, 80, 40, 74, 44, 68, 42);
  cairo_curve_to(cr, 62, 42, 58, 40, 56, 36);
  cairo_close_path(cr);
  setRgba(cr, rgb(170, 80, 30), 1.0);
  cairo_fill_preserve(cr);
  ink(cr, 1.2);
  if (variant >= 1)
  {
    ellipse(cr, 79, 36, 3.2, 2.6);
    setRgba(cr, rgb(70, 20, 24), 1.0);
    cairo_fill(cr);
  }
  cairo_move_to(cr, 53, 26);
  cairo_curve_to(cr, 52, 8, 82, 6, 84, 22);
  cairo_line_to(cr, 53, 26);
  cairo_close_path(cr);
  lit(cr, 52, 8, 84, 26, rgb(40, 90, 170));
  ink(cr, 1.6);
  cairo_move_to(cr, 54, 24);
  cairo_line_to(cr, 42, 26);
  cairo_line_to(cr, 44, 21);
  cairo_close_path(cr);
  setRgba(cr, rgb(30, 70, 140), 1.0);
  cairo_fill_preserve(cr);
  ink(cr, 1.2);
  // The front arm.
  if (variant == 2)
  {
    limb(cr, {{80, 58}, {96, 46}, {98, 30}}, 13, kHoodie);
    hand(cr, 98, 28, 7, kGlove);
  }
  else if (variant == 1)
  {
    limb(cr, {{78, 60}, {62, 84}, {40, 98}}, 13, kHoodie);
    hand(cr, 38, 100, 7, kGlove);
  }
  else
  {
    limb(cr, {{80, 60}, {88 + stride * 0.5, 80}, {90 + stride * 0.6, 98}}, 13, kHoodie);
    hand(cr, 90 + stride * 0.6, 100, 7, kGlove);
  }
  cairo_restore(cr);
  cairo_restore(cr);
}

// --- Boom Op ---------------------------------------------------------------------------------

void boomOp(cairo_t* cr, double w, double h, int variant)
{
  cairo_save(cr);
  frameBox(cr, w, h, 128, 160);
  ellipse(cr, 60, 157, 38, 5);
  setRgba(cr, kInk, 0.35);
  cairo_fill(cr);
  const double crouch = variant == 1 ? 8.0 : 0.0;
  const double lean = variant == 1 ? -6.0 : variant == 2 ? 6.0 : 0.0;
  // Legs, braced apart.
  limb(cr, {{52, 100 + crouch}, {44, 126 + crouch * 0.4}, {42, 146}}, 16, rgb(40, 44, 56));
  shoe(cr, 44, 156, rgb(26, 26, 30));
  limb(cr, {{68, 100 + crouch}, {78, 126 + crouch * 0.4}, {82, 146}}, 16, rgb(48, 52, 66));
  shoe(cr, 84, 156, rgb(34, 34, 40));
  cairo_save(cr);
  cairo_translate(cr, lean, crouch);
  // The back arm up on the pole's butt.
  limb(cr, {{48, 58}, {46, 40}, {64, 28}}, 12, rgb(70, 60, 50));
  hand(cr, 64, 28, 6, kSkinC);
  // The torso: a field jacket.
  cairo_move_to(cr, 40, 58);
  cairo_curve_to(cr, 44, 46, 76, 44, 82, 58);
  cairo_line_to(cr, 84, 102);
  cairo_line_to(cr, 38, 102);
  cairo_close_path(cr);
  lit(cr, 38, 46, 84, 102, rgb(84, 74, 58));
  ink(cr, 2.0);
  // The sound bag on its harness, meters glowing.
  cairo_move_to(cr, 44, 54);
  cairo_line_to(cr, 70, 76);
  cairo_move_to(cr, 76, 54);
  cairo_line_to(cr, 58, 76);
  stroke(cr, rgb(20, 20, 24), 3.0);
  roundRect(cr, 48, 74, 34, 22, 3);
  lit(cr, 48, 74, 82, 96, rgb(46, 48, 56));
  ink(cr, 1.6);
  for (int k = 0; k < 3; ++k)
  {
    cairo_rectangle(cr, 53 + k * 9, 78, 6, 9);
    setRgba(cr, rgb(10, 12, 14), 1.0);
    cairo_fill(cr);
    const double lv = variant == 2 ? 8.0 : 3.0 + k * 1.5;
    cairo_rectangle(cr, 53 + k * 9, 87 - lv, 6, lv);
    setRgba(cr, lv > 6 ? rgb(255, 80, 60) : rgb(80, 240, 120), 1.0);
    cairo_fill(cr);
  }
  for (int k = 0; k < 3; ++k)
  {
    circle(cr, 55 + k * 9, 92, 1.6);
    setRgba(cr, rgb(200, 200, 210), 1.0);
    cairo_fill(cr);
  }
  // The head: beanie, the big headphones.
  limb(cr, {{62, 48}, {62, 40}}, 10, kSkinC);
  face(cr, 62, 27, 14, kSkinC, variant == 2 ? 1 : 0, variant == 1 ? 1.5 : 0.0);
  cairo_move_to(cr, 47, 26);
  cairo_curve_to(cr, 46, 6, 78, 4, 78, 22);
  cairo_line_to(cr, 47, 26);
  cairo_close_path(cr);
  lit(cr, 46, 6, 78, 26, rgb(150, 40, 50));
  ink(cr, 1.6);
  cairo_rectangle(cr, 46, 21, 33, 6);
  setRgba(cr, rgb(120, 30, 40), 1.0);
  cairo_fill_preserve(cr);
  ink(cr, 1.2);
  cairo_arc(cr, 61, 26, 18, kPi * 1.08, kPi * 1.92);
  stroke(cr, kInk, 6.0);
  cairo_arc(cr, 61, 26, 18, kPi * 1.08, kPi * 1.92);
  stroke(cr, rgb(60, 62, 72), 3.6);
  ellipse(cr, 56, 30, 8, 10);
  lit(cr, 48, 20, 64, 40, rgb(44, 46, 54));
  ink(cr, 1.6);
  ellipse(cr, 57.5, 30, 4, 6);
  setRgba(cr, rgb(90, 94, 108), 1.0);
  cairo_fill(cr);
  // The front arm up on the pole.
  limb(cr, {{76, 58}, {84, 44}, {72, 26}}, 12, rgb(84, 74, 58));
  hand(cr, 71, 25, 6, kSkinC);
  cairo_restore(cr);
  cairo_restore(cr);
}

// --- The duck's studio headphones -----------------------------------------------------------

void duckHeadphones(cairo_t* cr, double w, double h, int variant)
{
  cairo_save(cr);
  cairo_translate(cr, kM, kM);
  cairo_scale(cr, w / 64.0, h / 64.0);
  if (variant == 0)
  {
    // The far cup, peeking out past the head, and the band's far side.
    cairo_arc(cr, 39, 24, 14, kPi * 1.15, kPi * 1.85);
    stroke(cr, kInk, 3.6);
    cairo_arc(cr, 39, 24, 14, kPi * 1.15, kPi * 1.85);
    stroke(cr, rgb(70, 72, 84), 2.0);
    ellipse(cr, 46, 21, 4, 6, 0.2);
    lit(cr, 42, 15, 50, 27, rgb(40, 42, 50), false);
  }
  else
  {
    // The band, padded, over the top of the head.
    cairo_arc(cr, 37, 25, 14.5, kPi * 1.05, kPi * 1.95);
    stroke(cr, kInk, 4.4);
    cairo_arc(cr, 37, 25, 14.5, kPi * 1.05, kPi * 1.95);
    stroke(cr, rgb(80, 82, 96), 2.6);
    cairo_arc(cr, 37, 25, 14.5, kPi * 1.35, kPi * 1.65);
    stroke(cr, rgb(30, 30, 36), 3.2);
    // The near cup over the ear.
    ellipse(cr, 32, 27, 6.5, 8.5);
    lit(cr, 25, 18, 39, 36, rgb(44, 46, 56), true);
    ink(cr, 1.4);
    ellipse(cr, 32, 27, 3.6, 5.2);
    setRgba(cr, rgb(120, 126, 140), 1.0);
    cairo_fill(cr);
    circle(cr, 32, 27, 1.4);
    setRgba(cr, kTally, 1.0);
    cairo_fill(cr);
    // The coiled lead down to the desk.
    cairo_move_to(cr, 28, 34);
    for (int k = 0; k < 8; ++k)
    {
      const double y = 36 + k * 3.4;
      cairo_curve_to(cr, 22, y, 22, y + 2.4, 27, y + 1.7);
      cairo_curve_to(cr, 30, y + 1.2, 28, y + 3.0, 24, y + 3.4);
    }
    stroke(cr, kInk, 1.6);
    cairo_move_to(cr, 24, 63);
    cairo_line_to(cr, 10, 64);
    stroke(cr, kInk, 1.6);
  }
  cairo_restore(cr);
}

} // namespace

bool drawGallery39Art(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame)
{
  (void)t;
  if (key == "floormgr")
    floorManager(cr, w, h, std::clamp(variant, 0, 5), frame % 2);
  else if (key == "wrangler")
    wrangler(cr, w, h, std::clamp(variant, 0, 2), frame % 2);
  else if (key == "boomop")
    boomOp(cr, w, h, std::clamp(variant, 0, 2));
  else if (key == "duck_headphones")
    duckHeadphones(cr, w, h, variant);
  else
    return false;
  return true;
}

} // namespace gr
