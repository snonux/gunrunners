// Sweeps Week (Level 41) in the style of enemy_art.cpp: smooth shapes with
// a dark ink line, lit warm from the stage. The Warm-up Comic in a loud
// yellow check; the Audience Plant, badly disguised; the Cue Card Drone;
// the studio audience (fans, and a few faces from earlier in the season in
// plain clothes); the duck's sequined bow tie. Each is drawn in screen
// pixels into a texture with a 32 px margin, the cell box at (32, 32) ..
// (32 + w, 32 + h), facing right.

#include "assets/enemy_art_sweeps41.hpp"

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
constexpr Color kSkinA = rgb(200, 146, 110);
constexpr Color kSkinB = rgb(130, 86, 60);

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

constexpr Color kCheck = rgb(236, 196, 64);
constexpr Color kCheckLine = rgb(150, 92, 30);
constexpr Color kBowRed = rgb(220, 30, 50);
constexpr Color kShirt = rgb(245, 240, 230);
constexpr Color kTrench = rgb(196, 168, 112);
constexpr Color kChrome = rgb(196, 204, 220);
constexpr Color kTomato = rgb(230, 50, 30);

void shoe(cairo_t* cr, double x, double y, Color c)
{
  cairo_move_to(cr, x - 9, y);
  cairo_line_to(cr, x - 9, y - 9);
  cairo_curve_to(cr, x, y - 11, x + 8, y - 8, x + 15, y - 3);
  cairo_line_to(cr, x + 15, y);
  cairo_close_path(cr);
  lit(cr, x - 9, y - 11, x + 15, y, c);
  ink(cr, 1.6);
  cairo_move_to(cr, x + 4, y - 9);
  cairo_line_to(cr, x + 14, y - 3);
  stroke(cr, lighten(c, 0.4f), 1.2, 0.6);
}

// Scales what follows by k about (x, y): a giant head on the same neck.
void bigAbout(cairo_t* cr, double x, double y, double k)
{
  cairo_translate(cr, x, y);
  cairo_scale(cr, k, k);
  cairo_translate(cr, -x, -y);
}

// The path's fill in a check: the base, then darker lines both ways.
void checked(cairo_t* cr, double x0, double y0, double x1, double y1, Color base, Color line, double pitch)
{
  cairo_save(cr);
  cairo_clip_preserve(cr);
  lit(cr, x0, y0, x1, y1, base);
  for (double x = x0 - 2; x < x1 + 2; x += pitch)
  {
    cairo_move_to(cr, x, y0 - 2);
    cairo_line_to(cr, x, y1 + 2);
  }
  stroke(cr, line, 1.6, 0.55);
  for (double y = y0; y < y1 + 2; y += pitch)
  {
    cairo_move_to(cr, x0 - 2, y);
    cairo_line_to(cr, x1 + 2, y);
  }
  stroke(cr, line, 1.6, 0.55);
  for (double x = x0 + pitch * 0.5; x < x1 + 2; x += pitch)
  {
    cairo_move_to(cr, x, y0 - 2);
    cairo_line_to(cr, x, y1 + 2);
  }
  stroke(cr, rgb(200, 40, 40), 0.8, 0.45);
  cairo_restore(cr);
  cairo_new_path(cr);
}

void bowTie(cairo_t* cr, double x, double y, double s, Color c)
{
  cairo_move_to(cr, x, y);
  cairo_line_to(cr, x - 9 * s, y - 6 * s);
  cairo_line_to(cr, x - 9 * s, y + 6 * s);
  cairo_close_path(cr);
  cairo_move_to(cr, x, y);
  cairo_line_to(cr, x + 9 * s, y - 6 * s);
  cairo_line_to(cr, x + 9 * s, y + 6 * s);
  cairo_close_path(cr);
  lit(cr, x - 9 * s, y - 6 * s, x + 9 * s, y + 6 * s, c);
  ink(cr, 1.2);
  circle(cr, x, y, 2.6 * s);
  setRgba(cr, darken(c, 0.25f), 1.0);
  cairo_fill_preserve(cr);
  ink(cr, 1.0);
}

// A rubber chicken: limp, yellow, its red comb, its beak open in a honk.
void rubberChicken(cairo_t* cr, double x, double y, double angle, double s)
{
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_rotate(cr, angle);
  cairo_scale(cr, s, s);
  // Body and neck, plucked and limp.
  cairo_move_to(cr, -18, 0);
  cairo_curve_to(cr, -16, -10, 2, -12, 8, -6);
  cairo_curve_to(cr, 12, -14, 14, -22, 20, -24);
  cairo_curve_to(cr, 26, -24, 26, -16, 22, -14);
  cairo_curve_to(cr, 18, -8, 16, 4, 6, 8);
  cairo_curve_to(cr, -6, 12, -18, 8, -18, 0);
  cairo_close_path(cr);
  lit(cr, -18, -24, 26, 12, rgb(250, 226, 120));
  ink(cr, 1.6);
  // Goose bumps.
  for (int k = 0; k < 6; ++k)
  {
    circle(cr, -12 + k * 3.4, -2 + (k % 2) * 3, 0.9);
    setRgba(cr, rgb(210, 170, 70), 1.0);
    cairo_fill(cr);
  }
  // The comb, the beak, an eye crossed out.
  cairo_move_to(cr, 18, -24);
  cairo_curve_to(cr, 17, -30, 21, -30, 22, -26);
  cairo_curve_to(cr, 23, -31, 27, -30, 25, -24);
  setRgba(cr, kBowRed, 1.0);
  cairo_fill_preserve(cr);
  ink(cr, 1.0);
  cairo_move_to(cr, 25, -20);
  cairo_line_to(cr, 33, -22);
  cairo_line_to(cr, 26, -17);
  cairo_line_to(cr, 32, -14);
  cairo_line_to(cr, 24, -15);
  setRgba(cr, rgb(255, 150, 30), 1.0);
  cairo_fill_preserve(cr);
  ink(cr, 1.0);
  cairo_move_to(cr, 19, -21);
  cairo_line_to(cr, 22, -18);
  cairo_move_to(cr, 22, -21);
  cairo_line_to(cr, 19, -18);
  stroke(cr, kInk, 1.2);
  // Drumsticks dangling.
  for (const double lx : {-12.0, -4.0})
  {
    cairo_move_to(cr, lx, 8);
    cairo_line_to(cr, lx - 2, 18);
    stroke(cr, kInk, 3.4);
    cairo_move_to(cr, lx, 8);
    cairo_line_to(cr, lx - 2, 18);
    stroke(cr, rgb(255, 150, 30), 1.8);
  }
  cairo_restore(cr);
}

void microphone(cairo_t* cr, double x, double y, double angle)
{
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_rotate(cr, angle);
  roundRect(cr, -3, 0, 6, 18, 2);
  lit(cr, -3, 0, 3, 18, rgb(40, 40, 46));
  ink(cr, 1.2);
  circle(cr, 0, -4, 6);
  lit(cr, -6, -10, 6, 2, rgb(200, 204, 214));
  ink(cr, 1.3);
  for (int k = -1; k <= 1; ++k)
  {
    cairo_move_to(cr, -5, -4 + k * 2.6);
    cairo_line_to(cr, 5, -4 + k * 2.6);
  }
  stroke(cr, rgb(110, 110, 120), 0.7);
  cairo_restore(cr);
}

// --- The Warm-up Comic --------------------------------------------------------------

void comic(cairo_t* cr, double w, double h, int variant, int frame)
{
  const bool tell = (variant & 1) != 0, big = (variant & 2) != 0;
  cairo_save(cr);
  frameBox(cr, w, h, 96, 160);
  ellipse(cr, 48, 157, 30, 4);
  setRgba(cr, kInk, 0.35);
  cairo_fill(cr);
  // Legs: the checked trousers, two-tone shoes.
  limb(cr, {{40, 98}, {37, 126}, {36, 148}}, 15, darken(kCheck, 0.1f));
  limb(cr, {{55, 98}, {58, 126}, {60, 148}}, 15, kCheck);
  shoe(cr, 37, 156, rgb(250, 248, 240));
  shoe(cr, 61, 156, rgb(250, 248, 240));
  for (const double sx : {37.0, 61.0})
  {
    cairo_move_to(cr, sx + 6, 147);
    cairo_line_to(cr, sx + 15, 153);
    cairo_line_to(cr, sx + 15, 156);
    cairo_line_to(cr, sx + 4, 156);
    cairo_close_path(cr);
    setRgba(cr, rgb(30, 24, 22), 1.0);
    cairo_fill(cr);
  }
  // The back arm: up behind with the chicken (the wind-up), or on the hip.
  if (tell)
  {
    limb(cr, {{36, 56}, {24, 40}, {20, 22}}, 11, darken(kCheck, 0.12f));
    rubberChicken(cr, 18, 14, -0.9, 1.0);
    hand(cr, 20, 20, 5, kSkinA);
  }
  else
    limb(cr, {{36, 56}, {28, 74}, {36, 86}}, 11, darken(kCheck, 0.12f));
  // The jacket: wide shoulders, the check, lapels, the shirt and bow tie.
  cairo_move_to(cr, 28, 56);
  cairo_curve_to(cr, 34, 44, 62, 43, 68, 56);
  cairo_line_to(cr, 66, 102);
  cairo_line_to(cr, 30, 102);
  cairo_close_path(cr);
  checked(cr, 28, 43, 68, 102, kCheck, kCheckLine, 7);
  cairo_move_to(cr, 28, 56);
  cairo_curve_to(cr, 34, 44, 62, 43, 68, 56);
  cairo_line_to(cr, 66, 102);
  cairo_line_to(cr, 30, 102);
  cairo_close_path(cr);
  ink(cr, 2.0);
  cairo_move_to(cr, 44, 48);
  cairo_line_to(cr, 56, 48);
  cairo_line_to(cr, 52, 82);
  cairo_close_path(cr);
  setRgba(cr, kShirt, 1.0);
  cairo_fill_preserve(cr);
  ink(cr, 1.2);
  for (const double s : {-1.0, 1.0})
  {
    cairo_move_to(cr, 50 + s * 6, 48);
    cairo_line_to(cr, 50 + s * 13, 60);
    cairo_line_to(cr, 50 + s * 3, 80);
    setRgba(cr, darken(kCheck, 0.3f), 1.0);
    cairo_fill_preserve(cr);
    ink(cr, 1.2);
  }
  // A carnation in the buttonhole.
  circle(cr, 60, 60, 3.4);
  setRgba(cr, kBowRed, 1.0);
  cairo_fill_preserve(cr);
  ink(cr, 1.0);
  // The head: a big grin, eyebrows up, the pompadour.
  cairo_save(cr);
  if (big)
    bigAbout(cr, 50, 46, 1.75);
  limb(cr, {{50, 48}, {50, 40}}, 9, kSkinA);
  face(cr, 50, 28, 13, kSkinA, -2.5, false, false, false);
  // The grin (open on alternate frames: he's talking).
  cairo_move_to(cr, 52, 34);
  cairo_curve_to(cr, 56, 40 + (frame % 2) * 2.0, 62, 39, 63, 33);
  cairo_close_path(cr);
  setRgba(cr, frame % 2 ? rgb(120, 30, 40) : rgb(255, 255, 255), 1.0);
  cairo_fill_preserve(cr);
  ink(cr, 1.2);
  // The pompadour, glossy.
  cairo_move_to(cr, 37, 24);
  cairo_curve_to(cr, 34, 10, 46, 4, 58, 8);
  cairo_curve_to(cr, 68, 10, 70, 16, 62, 18);
  cairo_curve_to(cr, 54, 16, 46, 18, 42, 26);
  cairo_close_path(cr);
  lit(cr, 34, 4, 70, 26, rgb(50, 30, 22));
  ink(cr, 1.4);
  cairo_move_to(cr, 44, 10);
  cairo_curve_to(cr, 50, 7, 58, 8, 64, 12);
  stroke(cr, rgb(170, 130, 110), 1.4, 0.7);
  cairo_restore(cr);
  bowTie(cr, 50, 49, 1.0, kBowRed);
  // The front arm and the mic: at his mouth, or flung out in the wind-up.
  if (tell)
  {
    limb(cr, {{62, 56}, {74, 60}, {84, 56}}, 11, kCheck);
    hand(cr, 85, 55, 5, kSkinA);
    microphone(cr, 85, 51, 0.5);
  }
  else
  {
    limb(cr, {{62, 56}, {70, 70}, {66, 52}}, 11, kCheck);
    hand(cr, 66, 50, 5, kSkinA);
    microphone(cr, big ? 72 : 67, big ? 40 : 44, 0.35);
  }
  cairo_restore(cr);
}

// --- The Audience Plant -------------------------------------------------------------

void plant(cairo_t* cr, double w, double h, int variant, int frame)
{
  const int pose = variant & 3;
  const bool big = (variant & 4) != 0;
  cairo_save(cr);
  frameBox(cr, w, h, 96, 160);
  ellipse(cr, 48, 157, 28, 4);
  setRgba(cr, kInk, 0.3);
  cairo_fill(cr);
  // Sitting drops him 34 px; getting up, half that.
  const double drop = pose == 0 ? 34.0 : pose == 1 ? 16.0 : 0.0;
  const Color trench = kTrench, slacks = rgb(70, 64, 60);
  // Legs.
  if (pose == 0)
  {
    limb(cr, {{38, 128}, {58, 128}, {60, 148}}, 15, darken(slacks, 0.1f));
    limb(cr, {{44, 132}, {66, 130}, {68, 148}}, 15, slacks);
  }
  else
  {
    limb(cr, {{40, 98 + drop}, {38, 126 + drop * 0.5}, {36, 148}}, 15, darken(slacks, 0.1f));
    limb(cr, {{55, 98 + drop}, {58, 126 + drop * 0.5}, {60, 148}}, 15, slacks);
  }
  shoe(cr, pose == 0 ? 60 : 37, 156, rgb(60, 40, 30));
  shoe(cr, pose == 0 ? 69 : 61, 156, rgb(70, 46, 34));
  cairo_save(cr);
  cairo_translate(cr, 0, drop);
  // The back arm.
  if (pose == 2)
  {
    limb(cr, {{36, 56}, {28, 74}, {34, 88}}, 11, darken(trench, 0.15f));
  }
  else
    limb(cr, {{36, 56}, {30, 72}, {44, 76}}, 11, darken(trench, 0.15f));
  // The trench coat: belted, collar up.
  cairo_move_to(cr, 29, 56);
  cairo_curve_to(cr, 34, 44, 62, 43, 67, 56);
  cairo_line_to(cr, 70, pose == 0 ? 100 : 112);
  cairo_line_to(cr, 26, pose == 0 ? 100 : 112);
  cairo_close_path(cr);
  lit(cr, 26, 43, 70, 112, trench);
  ink(cr, 2.0);
  cairo_rectangle(cr, 29, 84, 39, 6);
  setRgba(cr, darken(trench, 0.3f), 1.0);
  cairo_fill_preserve(cr);
  ink(cr, 1.0);
  for (const double s : {-1.0, 1.0})
  {
    cairo_move_to(cr, 50 + s * 4, 46);
    cairo_line_to(cr, 50 + s * 14, 40);
    cairo_line_to(cr, 50 + s * 12, 58);
    cairo_close_path(cr);
    lit(cr, 36, 40, 64, 58, darken(trench, 0.08f));
    ink(cr, 1.2);
  }
  // The lanyard and its pass, AUDIENCE in big letters (to be sure).
  cairo_move_to(cr, 44, 48);
  cairo_line_to(cr, 48, 66);
  cairo_line_to(cr, 54, 48);
  stroke(cr, rgb(40, 120, 220), 1.6);
  roundRect(cr, 40, 64, 18, 12, 1.5);
  setRgba(cr, rgb(250, 250, 245), 1.0);
  cairo_fill_preserve(cr);
  ink(cr, 1.0);
  selectGameFont(cr);
  cairo_set_font_size(cr, 4.2);
  cairo_move_to(cr, 41.2, 71.5);
  setRgba(cr, kInk, 1.0);
  cairo_show_text(cr, "AUDIENCE");
  // The front arm: folded across (sitting), pushing up, or the tomato up.
  if (pose == 0)
  {
    limb(cr, {{62, 56}, {66, 72}, {40, 74}}, 11, trench);
    hand(cr, 40, 74, 5, kSkinA);
  }
  else if (pose == 1)
  {
    limb(cr, {{62, 56}, {72, 74}, {76, 88}}, 11, trench);
    hand(cr, 76, 89, 5, kSkinA);
  }
  else
  {
    limb(cr, {{62, 56}, {70, 40}, {74, 24}}, 11, trench);
    circle(cr, 76, 16, 8);
    lit(cr, 68, 8, 84, 24, kTomato);
    ink(cr, 1.4);
    cairo_move_to(cr, 72, 9);
    cairo_line_to(cr, 76, 11);
    cairo_line_to(cr, 80, 8);
    stroke(cr, rgb(60, 150, 50), 2.0);
    hand(cr, 74, 24, 5, kSkinA);
  }
  // The head: sunglasses, a stuck-on moustache, the fedora.
  cairo_save(cr);
  if (big)
    bigAbout(cr, 50, 46, 1.75);
  limb(cr, {{50, 48}, {50, 40}}, 9, kSkinA);
  face(cr, 50, 28, 13, kSkinA, 1.5, false, false, true);
  roundRect(cr, 50, 22, 16, 6, 2);
  setRgba(cr, rgb(16, 16, 20), 1.0);
  cairo_fill_preserve(cr);
  ink(cr, 1.0);
  cairo_move_to(cr, 52, 23.5);
  cairo_line_to(cr, 58, 23.5);
  stroke(cr, rgb(255, 255, 255), 1.0, 0.6);
  const double tw = frame % 2 ? 1.5 : 0.0;
  cairo_move_to(cr, 52, 34 - tw);
  cairo_curve_to(cr, 56, 30, 62, 30, 66, 34 + tw);
  cairo_curve_to(cr, 62, 33, 56, 33, 52, 34 - tw);
  setRgba(cr, rgb(40, 26, 20), 1.0);
  cairo_fill_preserve(cr);
  ink(cr, 1.0);
  ellipse(cr, 50, 17, 22, 4);
  lit(cr, 28, 13, 72, 21, rgb(90, 76, 60));
  ink(cr, 1.4);
  cairo_move_to(cr, 38, 16);
  cairo_curve_to(cr, 38, 4, 62, 4, 62, 16);
  cairo_close_path(cr);
  lit(cr, 38, 4, 62, 16, rgb(100, 86, 66));
  ink(cr, 1.4);
  cairo_rectangle(cr, 38.5, 12, 23, 3.5);
  setRgba(cr, rgb(40, 30, 26), 1.0);
  cairo_fill(cr);
  cairo_restore(cr);
  cairo_restore(cr);
  cairo_restore(cr);
}

// --- The Cue Card Drone -------------------------------------------------------------

void cueDrone(cairo_t* cr, double w, double h, int variant, int frame)
{
  cairo_save(cr);
  frameBox(cr, w, h, 128, 96);
  cairo_translate(cr, 64, 30);
  if (variant == 1)
    cairo_rotate(cr, 0.28);
  // The arms reaching down for the card.
  for (const double s : {-1.0, 1.0})
  {
    cairo_move_to(cr, s * 14, 6);
    cairo_line_to(cr, s * 30, 22);
    cairo_line_to(cr, s * 30, 34);
    stroke(cr, kInk, 5.0);
    cairo_move_to(cr, s * 14, 6);
    cairo_line_to(cr, s * 30, 22);
    cairo_line_to(cr, s * 30, 34);
    stroke(cr, rgb(200, 204, 214), 2.6);
    circle(cr, s * 30, 36, 3);
    setRgba(cr, rgb(60, 60, 70), 1.0);
    cairo_fill(cr);
  }
  // Booms out to the rotors, the rotors spinning.
  for (const double s : {-1.0, 1.0})
  {
    cairo_move_to(cr, 0, 0);
    cairo_line_to(cr, s * 46, -10);
    stroke(cr, kInk, 6.0);
    cairo_move_to(cr, 0, 0);
    cairo_line_to(cr, s * 46, -10);
    stroke(cr, rgb(235, 236, 240), 3.4);
    roundRect(cr, s * 46 - 5, -16, 10, 10, 2);
    lit(cr, s * 46 - 5, -16, s * 46 + 5, -6, rgb(60, 60, 70));
    ink(cr, 1.0);
    ellipse(cr, s * 46, -18, 24, 4);
    setRgba(cr, rgb(200, 210, 230), 0.25);
    cairo_fill(cr);
    const double a = frame % 2 ? 0.4 : -0.3;
    cairo_move_to(cr, s * 46 - std::cos(a) * 22, -18 - std::sin(a) * 2);
    cairo_line_to(cr, s * 46 + std::cos(a) * 22, -18 + std::sin(a) * 2);
    stroke(cr, rgb(40, 40, 50), 2.0, 0.7);
  }
  // The body: a white shell, the camera dome under it, the red tally.
  roundRect(cr, -22, -12, 44, 20, 9);
  lit(cr, -22, -12, 22, 8, rgb(240, 240, 244));
  ink(cr, 1.8);
  cairo_move_to(cr, -14, -6);
  cairo_curve_to(cr, -6, -10, 6, -10, 14, -6);
  stroke(cr, rgb(255, 255, 255), 1.6, 0.8);
  circle(cr, 6, 9, 7);
  lit(cr, -1, 2, 13, 16, rgb(30, 30, 40));
  ink(cr, 1.2);
  circle(cr, 8, 9, 3);
  setRgba(cr, rgb(90, 150, 255), 1.0);
  cairo_fill(cr);
  circle(cr, 9, 8, 1);
  setRgba(cr, rgb(255, 255, 255), 0.9);
  cairo_fill(cr);
  circle(cr, -12, -4, 2.6);
  setRgba(cr, rgb(255, 40, 40), 1.0);
  cairo_fill(cr);
  // MAXTV on its flank.
  selectGameFont(cr);
  cairo_set_font_size(cr, 5.0);
  cairo_move_to(cr, -6, 2);
  setRgba(cr, rgb(200, 30, 60), 1.0);
  cairo_show_text(cr, "MAXTV");
  cairo_restore(cr);
}

// --- The audience ---------------------------------------------------------------------

struct Look
{
  Color shirt, pants, hair, skin;
};

void clappingArms(cairo_t* cr, Color sleeve, Color skin, int frame, bool still)
{
  if (still)
  {
    limb(cr, {{34, 60}, {30, 80}, {32, 98}}, 10, sleeve);
    limb(cr, {{62, 60}, {66, 80}, {64, 98}}, 10, sleeve);
    hand(cr, 32, 100, 4.6, skin);
    hand(cr, 64, 100, 4.6, skin);
    return;
  }
  const double gap = frame % 2 ? 1.0 : 9.0;
  limb(cr, {{34, 60}, {32, 78}, {48 - gap, 66}}, 10, sleeve);
  limb(cr, {{62, 60}, {66, 78}, {48 + gap, 66}}, 10, sleeve);
  hand(cr, 48 - gap, 65, 5, skin);
  hand(cr, 48 + gap, 65, 5, skin);
  if (frame % 2)
  {
    for (int k = 0; k < 3; ++k)
    {
      const double a = -kPi / 2 + (k - 1) * 0.6;
      cairo_move_to(cr, 48 + std::cos(a) * 9, 60 + std::sin(a) * 9);
      cairo_line_to(cr, 48 + std::cos(a) * 15, 60 + std::sin(a) * 15);
    }
    stroke(cr, rgb(255, 240, 200), 1.6, 0.8);
  }
}

// A head: front (side 0) or turned right (side 1).
void crowdHead(cairo_t* cr, double cx, double cy, Color skin, Color hair, int side, int style)
{
  limb(cr, {{48, 52}, {48, 42}}, 9, skin);
  if (side == 1)
  {
    face(cr, cx, cy, 13, skin, 0.0, false, false, false);
    cairo_move_to(cr, cx + 4, cy + 7);
    cairo_curve_to(cr, cx + 7, cy + 10, cx + 10, cy + 9, cx + 11, cy + 6);
    stroke(cr, darken(skin, 0.55f), 1.4);
  }
  else
  {
    circle(cr, cx, cy, 13);
    lit(cr, cx - 13, cy - 13, cx + 13, cy + 13, skin);
    ink(cr, 2.0);
    for (const double s : {-1.0, 1.0})
    {
      ellipse(cr, cx + s * 5, cy - 1, 2.0, 2.6);
      setRgba(cr, kInk, 1.0);
      cairo_fill(cr);
    }
    // Cheering: the mouth open.
    ellipse(cr, cx, cy + 6, 4, 3);
    setRgba(cr, rgb(120, 30, 40), 1.0);
    cairo_fill_preserve(cr);
    ink(cr, 1.0);
  }
  // Hair.
  if (style == 0)
  {
    cairo_move_to(cr, cx - 13, cy - 2);
    cairo_curve_to(cr, cx - 14, cy - 18, cx + 14, cy - 18, cx + 13, cy - 2);
    cairo_curve_to(cr, cx + 6, cy - 9, cx - 6, cy - 9, cx - 13, cy - 2);
  }
  else if (style == 1)
  {
    circle(cr, cx, cy - 10, 10);
    circle(cr, cx - 9, cy - 4, 6);
    circle(cr, cx + 9, cy - 4, 6);
  }
  else
  {
    cairo_move_to(cr, cx - 14, cy + 10);
    cairo_curve_to(cr, cx - 18, cy - 20, cx + 18, cy - 20, cx + 14, cy + 10);
    cairo_curve_to(cr, cx + 8, cy - 6, cx - 8, cy - 6, cx - 14, cy + 10);
  }
  lit(cr, cx - 14, cy - 20, cx + 14, cy + 10, hair);
  ink(cr, 1.4);
}

void torso(cairo_t* cr, Color c)
{
  cairo_move_to(cr, 32, 60);
  cairo_curve_to(cr, 36, 50, 60, 50, 64, 60);
  cairo_line_to(cr, 62, 104);
  cairo_line_to(cr, 34, 104);
  cairo_close_path(cr);
  lit(cr, 32, 50, 64, 104, c);
  ink(cr, 2.0);
}

void crowd(cairo_t* cr, double w, double h, int variant, int frame)
{
  const int look = std::clamp(variant / 16, 0, 4), palette = (variant / 2) % 8, side = variant % 2;
  static constexpr Look kFans[4] = {{rgb(60, 140, 230), rgb(50, 50, 70), rgb(60, 36, 24), rgb(220, 170, 130)},
    {rgb(240, 90, 150), rgb(40, 60, 110), rgb(230, 190, 90), rgb(240, 196, 160)},
    {rgb(90, 200, 120), rgb(80, 60, 50), rgb(30, 20, 20), rgb(150, 100, 70)},
    {rgb(250, 160, 50), rgb(60, 60, 64), rgb(150, 60, 40), rgb(110, 72, 50)}};
  cairo_save(cr);
  frameBox(cr, w, h, 96, 160);
  ellipse(cr, 48, 157, 24, 3.5);
  setRgba(cr, kInk, 0.3);
  cairo_fill(cr);
  if (look == 0)
  {
    const Look& l = kFans[palette % 4];
    limb(cr, {{41, 104}, {40, 146}}, 13, darken(l.pants, 0.1f));
    limb(cr, {{55, 104}, {56, 146}}, 13, l.pants);
    shoe(cr, 41, 156, rgb(240, 240, 240));
    shoe(cr, 57, 156, rgb(240, 240, 240));
    torso(cr, l.shirt);
    // A big star on the shirt.
    cairo_save(cr);
    cairo_translate(cr, 48, 80);
    for (int i = 0; i < 10; ++i)
    {
      const double a = -kPi / 2 + i * kPi / 5, rr = i % 2 ? 3.5 : 8;
      if (i == 0)
        cairo_move_to(cr, std::cos(a) * rr, std::sin(a) * rr);
      else
        cairo_line_to(cr, std::cos(a) * rr, std::sin(a) * rr);
    }
    cairo_close_path(cr);
    setRgba(cr, rgb(255, 255, 255), 0.8);
    cairo_fill(cr);
    cairo_restore(cr);
    crowdHead(cr, 48, 30, l.skin, l.hair, side, palette % 3);
    clappingArms(cr, l.shirt, l.skin, frame, false);
  }
  else if (look == 1)
  {
    // The Chrome Cop, off duty: chrome all over, a box of popcorn.
    limb(cr, {{41, 104}, {40, 146}}, 13, darken(kChrome, 0.25f));
    limb(cr, {{55, 104}, {56, 146}}, 13, darken(kChrome, 0.15f));
    shoe(cr, 41, 156, rgb(40, 40, 50));
    shoe(cr, 57, 156, rgb(40, 40, 50));
    torso(cr, rgb(80, 90, 120));
    selectGameFont(cr);
    cairo_set_font_size(cr, 6);
    cairo_move_to(cr, 37, 92);
    setRgba(cr, rgb(230, 230, 240), 0.9);
    cairo_show_text(cr, "I <3 TV");
    limb(cr, {{48, 52}, {48, 42}}, 9, kChrome);
    roundRect(cr, 34, 14, 28, 30, 10);
    lit(cr, 34, 14, 62, 44, kChrome);
    ink(cr, 2.0);
    roundRect(cr, side ? 44 : 38, 24, side ? 18 : 20, 7, 3);
    setRgba(cr, rgb(255, 60, 40), 1.0);
    cairo_fill_preserve(cr);
    ink(cr, 1.0);
    cairo_move_to(cr, 38, 18);
    cairo_line_to(cr, 44, 16);
    stroke(cr, rgb(255, 255, 255), 2.0, 0.8);
    // The popcorn box in one hand, a piece on its way up.
    limb(cr, {{34, 60}, {30, 80}, {40, 86}}, 10, rgb(80, 90, 120));
    cairo_move_to(cr, 36, 78);
    cairo_line_to(cr, 58, 78);
    cairo_line_to(cr, 55, 102);
    cairo_line_to(cr, 39, 102);
    cairo_close_path(cr);
    cairo_save(cr);
    cairo_clip_preserve(cr);
    setRgba(cr, rgb(250, 250, 245), 1.0);
    cairo_fill_preserve(cr);
    for (double x = 38; x < 60; x += 7)
      cairo_rectangle(cr, x, 76, 3.5, 30);
    setRgba(cr, rgb(220, 40, 40), 1.0);
    cairo_fill(cr);
    cairo_restore(cr);
    ink(cr, 1.4);
    for (int k = 0; k < 6; ++k)
    {
      circle(cr, 39 + k * 3.6, 76 - (k % 2) * 2.5, 2.8);
      setRgba(cr, rgb(255, 245, 200), 1.0);
      cairo_fill_preserve(cr);
      ink(cr, 0.8);
    }
    const double up = frame % 2 ? 46.0 : 60.0;
    limb(cr, {{62, 60}, {66, 76}, {56, up}}, 10, rgb(80, 90, 120));
    hand(cr, 56, up, 4.6, kChrome);
  }
  else if (look == 2)
  {
    // The Bouncer, a fan tonight: a SECURITY tee, a blue foam finger.
    limb(cr, {{40, 104}, {38, 146}}, 15, rgb(40, 40, 46));
    limb(cr, {{56, 104}, {58, 146}}, 15, rgb(48, 48, 54));
    shoe(cr, 39, 156, rgb(30, 30, 34));
    shoe(cr, 59, 156, rgb(30, 30, 34));
    cairo_move_to(cr, 26, 60);
    cairo_curve_to(cr, 32, 46, 64, 46, 70, 60);
    cairo_line_to(cr, 64, 104);
    cairo_line_to(cr, 32, 104);
    cairo_close_path(cr);
    lit(cr, 26, 46, 70, 104, rgb(30, 30, 36));
    ink(cr, 2.0);
    selectGameFont(cr);
    cairo_set_font_size(cr, 6.5);
    cairo_move_to(cr, 33, 76);
    setRgba(cr, rgb(250, 220, 60), 1.0);
    cairo_show_text(cr, "SECURITY");
    crowdHead(cr, 48, 30, rgb(150, 100, 70), rgb(20, 16, 16), side, 0);
    roundRect(cr, side ? 46 : 36, 24, side ? 18 : 24, 6, 2);
    setRgba(cr, rgb(10, 10, 14), 1.0);
    cairo_fill(cr);
    limb(cr, {{30, 60}, {24, 80}, {30, 96}}, 13, rgb(150, 100, 70));
    // The finger, waving.
    const double wv = frame % 2 ? 0.15 : -0.1;
    limb(cr, {{66, 60}, {76, 44}, {78, 30}}, 13, rgb(150, 100, 70));
    cairo_save(cr);
    cairo_translate(cr, 78, 30);
    cairo_rotate(cr, wv);
    roundRect(cr, -10, -20, 20, 24, 6);
    lit(cr, -10, -20, 10, 4, rgb(50, 120, 255));
    ink(cr, 1.6);
    roundRect(cr, -4, -46, 9, 30, 4);
    lit(cr, -4, -46, 5, -16, rgb(60, 140, 255));
    ink(cr, 1.6);
    selectGameFont(cr);
    cairo_set_font_size(cr, 8);
    cairo_move_to(cr, -7, -2);
    setRgba(cr, rgb(255, 255, 255), 1.0);
    cairo_show_text(cr, "#1");
    cairo_restore(cr);
  }
  else if (look == 3)
  {
    // A Mannequin in a seat: smooth plastic, no face, arms at its sides.
    const Color plastic = rgb(232, 214, 186);
    limb(cr, {{41, 104}, {40, 146}}, 12, darken(plastic, 0.08f));
    limb(cr, {{55, 104}, {56, 146}}, 12, plastic);
    for (const double fx : {40.0, 56.0})
    {
      ellipse(cr, fx + 3, 152, 8, 4);
      lit(cr, fx - 5, 148, fx + 11, 156, plastic);
      ink(cr, 1.2);
    }
    torso(cr, rgb(120, 110, 150));
    limb(cr, {{48, 52}, {48, 42}}, 8, plastic);
    ellipse(cr, 48, 28, 12, 15);
    lit(cr, 36, 13, 60, 43, plastic);
    ink(cr, 2.0);
    ellipse(cr, side ? 54 : 48, 30, 5, 8);
    setRgba(cr, rgb(255, 255, 255), 0.3);
    cairo_fill(cr);
    // The joint lines.
    cairo_move_to(cr, 40, 46);
    cairo_line_to(cr, 56, 46);
    stroke(cr, darken(plastic, 0.3f), 1.0);
    clappingArms(cr, plastic, plastic, 0, true);
  }
  else
  {
    // Black Halo's pilot: a brown flight jacket with a fleece collar,
    // aviators, a headset round his neck.
    const Color jacket = rgb(120, 76, 40);
    limb(cr, {{41, 104}, {40, 146}}, 13, rgb(70, 80, 60));
    limb(cr, {{55, 104}, {56, 146}}, 13, rgb(80, 90, 66));
    shoe(cr, 41, 156, rgb(40, 30, 24));
    shoe(cr, 57, 156, rgb(40, 30, 24));
    torso(cr, jacket);
    cairo_move_to(cr, 34, 56);
    cairo_curve_to(cr, 40, 64, 56, 64, 62, 56);
    cairo_curve_to(cr, 56, 52, 40, 52, 34, 56);
    setRgba(cr, rgb(235, 225, 205), 1.0);
    cairo_fill_preserve(cr);
    ink(cr, 1.2);
    // A squadron patch.
    circle(cr, 40, 78, 5);
    setRgba(cr, rgb(20, 20, 26), 1.0);
    cairo_fill_preserve(cr);
    ink(cr, 1.0);
    circle(cr, 40, 78, 2.6);
    stroke(cr, rgb(255, 200, 80), 1.2);
    crowdHead(cr, 48, 30, rgb(220, 170, 130), rgb(170, 130, 60), side, 0);
    roundRect(cr, side ? 47 : 37, 25, side ? 17 : 22, 6, 3);
    setRgba(cr, rgb(40, 40, 50), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, rgb(220, 190, 90), 1.0);
    cairo_arc(cr, 48, 52, 12, 0.1, kPi - 0.1);
    stroke(cr, rgb(40, 40, 44), 2.4);
    clappingArms(cr, jacket, rgb(220, 170, 130), frame, false);
  }
  cairo_restore(cr);
}

// --- The duck's bow tie -------------------------------------------------------------

void duckBowtie(cairo_t* cr, double w, double h, int variant, int frame)
{
  if (variant != 1)
    return;
  cairo_save(cr);
  cairo_translate(cr, kM, kM);
  cairo_scale(cr, w / 64.0, h / 64.0);
  bowTie(cr, 36, 36, 1.0, rgb(255, 50, 120));
  for (int k = 0; k < 6; ++k)
  {
    const double x = 36 + (k < 3 ? -5.5 : 5.5) + ((k % 3) - 1) * 1.8, y = 36 + ((k % 3) - 1) * 2.8;
    circle(cr, x, y, 0.9);
    setRgba(cr, (k + frame) % 2 ? rgb(255, 255, 255) : rgb(255, 220, 120), 0.95);
    cairo_fill(cr);
  }
  cairo_restore(cr);
}

} // namespace

bool drawSweeps41Art(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame)
{
  (void)t;
  if (key == "comic")
    comic(cr, w, h, variant & 3, frame % 2);
  else if (key == "audience_plant")
    plant(cr, w, h, variant & 7, frame % 2);
  else if (key == "cuedrone")
    cueDrone(cr, w, h, variant & 1, frame % 2);
  else if (key == "crowd")
    crowd(cr, w, h, variant, frame % 2);
  else if (key == "duck_bowtie")
    duckBowtie(cr, w, h, variant, frame % 2);
  else
    return false;
  return true;
}

} // namespace gr
