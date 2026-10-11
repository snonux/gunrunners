// Toon Town Throwdown (Level 28) in the style of enemy_art.cpp, but inked
// like a 1930s rubber-hose cartoon: flat fills, thick warm-black outlines,
// pie-cut eyes, white gloves and hose limbs. The Grinning Bomb, the Piano
// Bird (and its upright piano), the Rubber Goon and his stretched arm, the
// rubber duck's gloves, and Buddy Boom, MaxTV's mascot, in all his poses.
// Each enemy is drawn in screen pixels into a texture with a 32 px margin,
// the cell box at (32, 32) .. (32 + w, 32 + h), facing right.

#include "assets/enemy_art_toon.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <utility>

namespace gr
{

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr Color kInk = rgb(30, 22, 20);
constexpr Color kGloveWhite = rgb(255, 253, 246);
constexpr Color kPeach = rgb(250, 214, 178);
constexpr double kM = 32.0;

Color darken(Color c, float t) { return lerpColor(c, rgb(0, 0, 0), t); }

void setRgba(cairo_t* cr, Color c, double a)
{
  cairo_set_source_rgba(cr, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, a);
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
  cairo_scale(cr, rx, ry);
  cairo_new_sub_path(cr);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_restore(cr);
}

void stop(cairo_pattern_t* g, double at, Color c, double a = 1.0)
{
  cairo_pattern_add_color_stop_rgba(g, at, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, a);
}

void ink(cairo_t* cr, double line = 3.0)
{
  setColor(cr, kInk);
  cairo_set_line_width(cr, line);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
  cairo_stroke(cr);
}

// Fills the path flat and inks it.
void fillInk(cairo_t* cr, Color fill, double line = 3.0)
{
  setColor(cr, fill);
  cairo_fill_preserve(cr);
  ink(cr, line);
}

// Fills the path with a vertical two-colour gradient and inks it.
void fillGrad(cairo_t* cr, double y0, double y1, Color a, Color b, double line = 3.0)
{
  cairo_pattern_t* g = cairo_pattern_create_linear(0, y0, 0, y1);
  stop(g, 0, a);
  stop(g, 1, b);
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  ink(cr, line);
}

// A white shine dabbed on something round.
void shine(cairo_t* cr, double x, double y, double rx, double ry, double angle = -0.6, double a = 0.85)
{
  ellipse(cr, x, y, rx, ry, angle);
  setRgba(cr, rgb(255, 255, 255), a);
  cairo_fill(cr);
}

// A rubber-hose limb: a black tube from a to b bowing through c, a dull
// sheen down its middle.
void hose(cairo_t* cr, double ax, double ay, double cx, double cy, double bx, double by, double width)
{
  cairo_move_to(cr, ax, ay);
  cairo_curve_to(cr, ax + (cx - ax) * 0.66, ay + (cy - ay) * 0.66, bx + (cx - bx) * 0.66, by + (cy - by) * 0.66, bx, by);
  setColor(cr, kInk);
  cairo_set_line_width(cr, width);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  cairo_stroke_preserve(cr);
  setRgba(cr, rgb(120, 110, 130), 0.45);
  cairo_set_line_width(cr, width * 0.2);
  cairo_stroke(cr);
}

// A big cartoon shoe, its sole on y, the toe toward `dir`.
void shoe(cairo_t* cr, double x, double y, double size, int dir, Color c = rgb(236, 178, 70), double tilt = 0.0)
{
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_rotate(cr, tilt);
  cairo_scale(cr, dir, 1.0);
  cairo_move_to(cr, -size * 0.55, 0);
  cairo_curve_to(cr, -size * 0.75, -size * 0.1, -size * 0.6, -size * 0.55, -size * 0.2, -size * 0.55);
  cairo_curve_to(cr, size * 0.2, -size * 0.6, size * 0.55, -size * 0.75, size * 0.95, -size * 0.45);
  cairo_curve_to(cr, size * 1.2, -size * 0.3, size * 1.15, 0, size * 0.85, 0);
  cairo_close_path(cr);
  cairo_restore(cr);
  fillInk(cr, c, 3.0);
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_rotate(cr, tilt);
  cairo_scale(cr, dir, 1.0);
  shine(cr, size * 0.55, -size * 0.45, size * 0.2, size * 0.09, -0.3, 0.75);
  cairo_move_to(cr, -size * 0.6, -size * 0.1);
  cairo_line_to(cr, size * 1.0, -size * 0.1);
  cairo_restore(cr);
  ink(cr, 2.0);
}

// A cream pie on its tin plate, centred at (x, y).
void pie(cairo_t* cr, double x, double y, double s)
{
  ellipse(cr, x, y + 8 * s, 34 * s, 9 * s);
  fillInk(cr, rgb(196, 196, 206), 2.4);
  cairo_move_to(cr, x - 30 * s, y + 6 * s);
  cairo_curve_to(cr, x - 30 * s, y - 14 * s, x - 18 * s, y - 24 * s, x - 10 * s, y - 20 * s);
  for (int i = 0; i < 4; ++i)
  {
    const double bx = x - 10 * s + i * 7 * s;
    cairo_curve_to(cr, bx + 2 * s, y - 30 * s, bx + 6 * s, y - 30 * s, bx + 7 * s, y - 21 * s);
  }
  cairo_curve_to(cr, x + 26 * s, y - 22 * s, x + 32 * s, y - 10 * s, x + 30 * s, y + 6 * s);
  cairo_close_path(cr);
  fillInk(cr, rgb(255, 252, 240), 2.4);
  cairo_rectangle(cr, x - 29 * s, y + 1 * s, 58 * s, 6 * s);
  setColor(cr, rgb(214, 160, 96));
  cairo_fill(cr);
  circle(cr, x + 2 * s, y - 30 * s, 6 * s);
  fillInk(cr, rgb(220, 30, 40), 2.0);
  shine(cr, x, y - 32 * s, 2 * s, 1.4 * s);
}

// --- Grinning Bomb -----------------------------------------------------------------------------

// A black bomb on hose legs: a grin (0), the frown (1), the lunge (2).
void grinningBomb(cairo_t* cr, double w, double h, int variant, int frame)
{
  const double cx = kM + w * 0.5, cy = kM + h * 0.5, r = w * 0.36;
  cairo_save(cr);
  if (variant == 2)
  {
    // The lunge: leaning in and stretched, speed lines streaming behind.
    for (int i = 0; i < 3; ++i)
    {
      cairo_move_to(cr, kM - 26, cy - 18 + i * 18);
      cairo_line_to(cr, kM + 2, cy - 18 + i * 18);
      ink(cr, 3.0);
    }
    cairo_translate(cr, cx, kM + h);
    cairo_rotate(cr, 0.28);
    cairo_scale(cr, 1.12, 0.92);
    cairo_translate(cr, -cx, -(kM + h));
  }
  // Legs and shoes: a run cycle in two frames.
  const double hipY = cy + r * 0.75, footY = kM + h - 1;
  const bool a = frame % 2 == 0;
  const double fFar = variant == 2 ? -26 : (a ? -14 : 10), fNear = variant == 2 ? 6 : (a ? 14 : -12);
  hose(cr, cx - 8, hipY, cx - 8 + fFar * 0.3, footY - 12, cx - 8 + fFar, footY - 8, 7);
  shoe(cr, cx - 8 + fFar, footY, 16, 1, rgb(210, 70, 60));
  // The body: a black ball lit from the upper left.
  circle(cr, cx, cy, r);
  {
    cairo_pattern_t* g = cairo_pattern_create_radial(cx - r * 0.4, cy - r * 0.4, r * 0.1, cx, cy, r);
    stop(g, 0, rgb(90, 84, 100));
    stop(g, 0.5, rgb(40, 34, 44));
    stop(g, 1, rgb(18, 14, 18));
    cairo_set_source(cr, g);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(g);
  }
  ink(cr, 3.0);
  shine(cr, cx - r * 0.45, cy - r * 0.5, r * 0.26, r * 0.13, -0.7, 0.75);
  // The fuse cap and the fuse, curling, a spark at its end.
  cairo_save(cr);
  cairo_translate(cr, cx + r * 0.42, cy - r * 0.82);
  cairo_rotate(cr, 0.55);
  cairo_rectangle(cr, -8, -8, 16, 11);
  cairo_restore(cr);
  fillInk(cr, rgb(150, 146, 160), 2.4);
  const double fl = variant == 2 ? 0.5 : 1.0;
  const double fx0 = cx + r * 0.55, fy0 = cy - r * 0.98;
  const double fx1 = fx0 + 14 * fl, fy1 = fy0 - 16 * fl;
  cairo_move_to(cr, fx0, fy0);
  cairo_curve_to(cr, fx0 + 4, fy0 - 10 * fl, fx1 - 10 * fl, fy1 + 2, fx1, fy1);
  setColor(cr, rgb(170, 130, 90));
  cairo_set_line_width(cr, 3.5);
  cairo_stroke_preserve(cr);
  ink(cr, 1.2);
  const double sp = variant >= 1 ? 1.7 : 1.0;
  for (int i = 0; i < 6; ++i)
  {
    const double ang = i * kPi / 3 + frame * 0.5;
    const double l = (i % 2 ? 9 : 6) * sp;
    cairo_move_to(cr, fx1, fy1);
    cairo_line_to(cr, fx1 + std::cos(ang) * l, fy1 + std::sin(ang) * l);
  }
  setColor(cr, variant >= 1 ? rgb(255, 90, 40) : rgb(255, 210, 60));
  cairo_set_line_width(cr, 2.6);
  cairo_stroke(cr);
  circle(cr, fx1, fy1, 3.4 * sp);
  setColor(cr, rgb(255, 250, 200));
  cairo_fill(cr);
  // The eyes: pie-cut, looking ahead; angry brows from the tell on.
  for (int i = 0; i < 2; ++i)
  {
    const double ex = cx + r * (0.12 + i * 0.4), ey = cy - r * 0.25;
    ellipse(cr, ex, ey, r * 0.17, r * 0.27);
    fillInk(cr, rgb(255, 255, 255), 2.0);
    ellipse(cr, ex + r * 0.06, ey + (variant ? 0.02 : 0.06) * r, r * 0.09, r * 0.16);
    setColor(cr, kInk);
    cairo_fill(cr);
    cairo_move_to(cr, ex + r * 0.06, ey + 0.04 * r);
    cairo_arc(cr, ex + r * 0.06, ey + 0.04 * r, r * 0.1, -kPi * 0.45, -kPi * 0.15);
    cairo_close_path(cr);
    setColor(cr, rgb(255, 255, 255));
    cairo_fill(cr);
    if (variant >= 1)
    {
      cairo_move_to(cr, ex - r * 0.2, ey - r * (i ? 0.42 : 0.3));
      cairo_line_to(cr, ex + r * 0.2, ey - r * (i ? 0.3 : 0.42));
      setColor(cr, rgb(255, 255, 255));
      cairo_set_line_width(cr, 4.5);
      cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
      cairo_stroke(cr);
    }
  }
  // The mouth.
  if (variant == 0)
  {
    // A wide toothy grin.
    cairo_move_to(cr, cx - r * 0.35, cy + r * 0.15);
    cairo_curve_to(cr, cx - r * 0.1, cy + r * 0.75, cx + r * 0.7, cy + r * 0.65, cx + r * 0.85, cy + r * 0.05);
    cairo_curve_to(cr, cx + r * 0.5, cy + r * 0.28, cx, cy + r * 0.3, cx - r * 0.35, cy + r * 0.15);
    cairo_close_path(cr);
    fillInk(cr, rgb(255, 255, 255), 2.2);
    for (int i = 1; i < 5; ++i)
    {
      const double tx = cx - r * 0.3 + i * r * 0.23;
      cairo_move_to(cr, tx, cy + r * 0.2);
      cairo_line_to(cr, tx - r * 0.02, cy + r * 0.55);
    }
    cairo_move_to(cr, cx - r * 0.25, cy + r * 0.36);
    cairo_curve_to(cr, cx + r * 0.1, cy + r * 0.48, cx + r * 0.5, cy + r * 0.42, cx + r * 0.8, cy + r * 0.2);
    ink(cr, 1.4);
  }
  else if (variant == 1)
  {
    // The frown.
    cairo_move_to(cr, cx - r * 0.1, cy + r * 0.55);
    cairo_curve_to(cr, cx + r * 0.1, cy + r * 0.22, cx + r * 0.55, cy + r * 0.22, cx + r * 0.75, cy + r * 0.5);
    setColor(cr, rgb(255, 255, 255));
    cairo_set_line_width(cr, 5.0);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_stroke(cr);
    // A bead of sweat.
    cairo_move_to(cr, cx - r * 0.55, cy - r * 0.55);
    cairo_curve_to(cr, cx - r * 0.7, cy - r * 0.3, cx - r * 0.4, cy - r * 0.3, cx - r * 0.55, cy - r * 0.55);
    fillInk(cr, rgb(170, 220, 255), 1.6);
  }
  else
  {
    // Teeth gritted.
    cairo_rectangle(cr, cx - r * 0.05, cy + r * 0.25, r * 0.8, r * 0.3);
    fillInk(cr, rgb(255, 255, 255), 2.2);
    cairo_move_to(cr, cx - r * 0.05, cy + r * 0.4);
    cairo_line_to(cr, cx + r * 0.75, cy + r * 0.4);
    for (int i = 1; i < 4; ++i)
    {
      cairo_move_to(cr, cx - r * 0.05 + i * r * 0.2, cy + r * 0.25);
      cairo_line_to(cr, cx - r * 0.05 + i * r * 0.2, cy + r * 0.55);
    }
    ink(cr, 1.4);
  }
  // The near leg over the body.
  hose(cr, cx + 8, hipY, cx + 8 + fNear * 0.3, footY - 12, cx + 8 + fNear, footY - 8, 7);
  shoe(cr, cx + 8 + fNear, footY, 16, 1, rgb(210, 70, 60));
  cairo_restore(cr);
}

// --- Piano Bird and its piano ---------------------------------------------------------------------

// A stork in a red bow tie: carrying (0) its feet gripping the ropes, or
// empty (1) with its legs tucked; frame 0/1 the wings up and down.
void pianoBird(cairo_t* cr, double w, double h, int variant, int frame)
{
  const double cx = kM + w * 0.45, cy = kM + h * 0.42;
  const Color body = rgb(244, 246, 250), wingC = rgb(214, 222, 236);
  const bool up = frame % 2 == 0;
  // The legs: straight down to the rope grips, or tucked.
  if (variant == 0)
  {
    for (const double lx : {-10.0, 12.0})
    {
      cairo_move_to(cr, cx + lx, cy + 14);
      cairo_line_to(cr, cx + lx * 1.4, kM + h - 2);
      setColor(cr, rgb(250, 150, 60));
      cairo_set_line_width(cr, 5);
      cairo_stroke_preserve(cr);
      ink(cr, 1.4);
      // The toes curled round the rope.
      circle(cr, cx + lx * 1.4, kM + h - 2, 5);
      fillInk(cr, rgb(250, 150, 60), 1.8);
    }
  }
  else
  {
    cairo_move_to(cr, cx - 6, cy + 14);
    cairo_line_to(cr, cx - 34, cy + 22);
    cairo_line_to(cr, cx - 44, cy + 16);
    setColor(cr, rgb(250, 150, 60));
    cairo_set_line_width(cr, 5);
    cairo_stroke_preserve(cr);
    ink(cr, 1.4);
  }
  // The far wing.
  auto wing = [&](double ox, double lift, Color c) {
    cairo_move_to(cr, cx + ox - 10, cy - 6);
    if (lift > 0)
      cairo_curve_to(cr, cx + ox - 40, cy - 60, cx + ox - 70, cy - 70, cx + ox - 74, cy - 54);
    else
      cairo_curve_to(cr, cx + ox - 40, cy + 30, cx + ox - 70, cy + 40, cx + ox - 76, cy + 26);
    cairo_curve_to(cr, cx + ox - 60, cy + (lift > 0 ? -40 : 12), cx + ox - 40, cy + (lift > 0 ? -24 : 8), cx + ox + 8, cy + 4);
    cairo_close_path(cr);
    fillInk(cr, c, 2.4);
    // Black flight feathers at the tip.
    cairo_move_to(cr, cx + ox - 74, cy + (lift > 0 ? -54 : 26));
    cairo_line_to(cr, cx + ox - 56, cy + (lift > 0 ? -50 : 20));
    cairo_line_to(cr, cx + ox - 62, cy + (lift > 0 ? -38 : 12));
    cairo_close_path(cr);
    fillInk(cr, kInk, 1.4);
  };
  wing(10, up ? 1 : -1, darken(wingC, 0.1f));
  // The body: a plump oval with a stubby tail.
  cairo_move_to(cr, cx - 40, cy - 2);
  cairo_line_to(cr, cx - 58, cy - 10);
  cairo_line_to(cr, cx - 52, cy + 6);
  cairo_close_path(cr);
  fillInk(cr, kInk, 2.0);
  ellipse(cr, cx, cy, 40, 24, -0.12);
  fillInk(cr, body, 3.0);
  // The neck and head, the long beak.
  cairo_move_to(cr, cx + 26, cy - 12);
  cairo_curve_to(cr, cx + 40, cy - 30, cx + 30, cy - 46, cx + 42, cy - 56);
  setColor(cr, kInk);
  cairo_set_line_width(cr, 16);
  cairo_stroke_preserve(cr);
  setColor(cr, body);
  cairo_set_line_width(cr, 10);
  cairo_stroke(cr);
  const double hx = cx + 46, hy = cy - 60;
  cairo_move_to(cr, hx + 8, hy - 4);
  cairo_line_to(cr, hx + 52, hy + 10);
  cairo_line_to(cr, hx + 8, hy + 8);
  cairo_close_path(cr);
  fillInk(cr, rgb(250, 170, 60), 2.2);
  cairo_move_to(cr, hx + 10, hy + 3);
  cairo_line_to(cr, hx + 44, hy + 8);
  ink(cr, 1.2);
  circle(cr, hx, hy, 15);
  fillInk(cr, body, 2.6);
  ellipse(cr, hx + 3, hy - 4, 5.5, 8);
  fillInk(cr, rgb(255, 255, 255), 1.6);
  ellipse(cr, hx + 5, hy - 2, 3, 5);
  setColor(cr, kInk);
  cairo_fill(cr);
  // A little sailor's cap.
  cairo_move_to(cr, hx - 14, hy - 10);
  cairo_curve_to(cr, hx - 10, hy - 26, hx + 10, hy - 26, hx + 12, hy - 12);
  cairo_close_path(cr);
  fillInk(cr, rgb(255, 255, 255), 2.0);
  cairo_rectangle(cr, hx - 15, hy - 13, 28, 5);
  fillInk(cr, rgb(70, 110, 200), 1.6);
  // The bow tie.
  const double bx = cx + 30, by = cy - 14;
  cairo_move_to(cr, bx, by);
  cairo_line_to(cr, bx - 10, by - 8);
  cairo_line_to(cr, bx - 10, by + 8);
  cairo_close_path(cr);
  cairo_move_to(cr, bx, by);
  cairo_line_to(cr, bx + 10, by - 8);
  cairo_line_to(cr, bx + 10, by + 8);
  cairo_close_path(cr);
  fillInk(cr, rgb(220, 40, 50), 1.8);
  // The near wing.
  wing(-6, up ? 1 : -1, wingC);
  // Sweat flicks when it carries the piano.
  if (variant == 0 && frame % 2)
    for (int i = 0; i < 2; ++i)
    {
      cairo_move_to(cr, hx - 18 - i * 8, hy - 18 + i * 6);
      cairo_line_to(cr, hx - 26 - i * 8, hy - 26 + i * 6);
      ink(cr, 2.0);
    }
}

// --- Rubber Goon -----------------------------------------------------------------------------------

void goonHead(cairo_t* cr, double hx, double hy, double sx, double sy, bool angry, bool flat)
{
  cairo_save(cr);
  cairo_translate(cr, hx, hy);
  cairo_scale(cr, sx, sy);
  // The jowly bulldog head.
  cairo_move_to(cr, -26, -18);
  cairo_curve_to(cr, -26, -40, 26, -42, 30, -18);
  cairo_curve_to(cr, 46, -10, 48, 22, 30, 30);
  cairo_curve_to(cr, 10, 38, -24, 36, -30, 18);
  cairo_curve_to(cr, -36, 4, -30, -8, -26, -18);
  cairo_close_path(cr);
  fillInk(cr, rgb(200, 180, 196), 3.0);
  // The stubble.
  for (int i = 0; i < 14; ++i)
  {
    const double a = 0.2 + i * 0.18;
    circle(cr, 10 + std::cos(a) * 26, 14 + std::sin(a) * 12, 1.1);
  }
  setColor(cr, rgb(110, 100, 120));
  cairo_fill(cr);
  // The underbite, two teeth up.
  cairo_move_to(cr, 8, 18);
  cairo_curve_to(cr, 20, 26, 34, 24, 40, 12);
  ink(cr, 2.4);
  for (const double tx : {18.0, 32.0})
  {
    cairo_move_to(cr, tx - 3, 22);
    cairo_line_to(cr, tx, 14);
    cairo_line_to(cr, tx + 3, 21);
    cairo_close_path(cr);
    fillInk(cr, rgb(255, 255, 250), 1.4);
  }
  // The nose.
  ellipse(cr, 38, -6, 9, 7);
  setColor(cr, kInk);
  cairo_fill(cr);
  shine(cr, 36, -9, 3, 1.6);
  if (flat)
  {
    // Seeing stars: Xs for eyes.
    for (const double ex : {6.0, 22.0})
    {
      cairo_move_to(cr, ex - 5, -16);
      cairo_line_to(cr, ex + 5, -6);
      cairo_move_to(cr, ex + 5, -16);
      cairo_line_to(cr, ex - 5, -6);
    }
    ink(cr, 2.6);
  }
  else
    for (const double ex : {6.0, 22.0})
    {
      ellipse(cr, ex, -12, 5.5, 8);
      fillInk(cr, rgb(255, 255, 255), 1.6);
      ellipse(cr, ex + 2, -10, 3, 5);
      setColor(cr, kInk);
      cairo_fill(cr);
      cairo_move_to(cr, ex - 8, angry ? -26 : -22);
      cairo_line_to(cr, ex + 8, angry ? -20 : -23);
      ink(cr, 3.2);
    }
  // The bowler hat.
  cairo_move_to(cr, -22, -26);
  cairo_curve_to(cr, -22, -56, 22, -56, 22, -28);
  cairo_close_path(cr);
  fillInk(cr, rgb(50, 44, 54), 2.6);
  ellipse(cr, 0, -26, 32, 6, -0.05);
  fillInk(cr, rgb(50, 44, 54), 2.6);
  cairo_rectangle(cr, -21, -34, 42, 5);
  setColor(cr, rgb(170, 60, 60));
  cairo_fill(cr);
  shine(cr, -8, -44, 5, 2.4, -0.4, 0.4);
  cairo_restore(cr);
}

// The heavy: a striped sweater over a barrel chest, short legs, long hose
// arms and white gloves. variant 0 walking, 1 the arm wound into a coil, 2
// the punch going out of the box.
void rubberGoon(cairo_t* cr, double w, double h, int variant, int frame)
{
  const double cx = kM + w * 0.45, foot = kM + h - 1;
  if (h < 100)
  {
    // Squashed flat: a pancake of sweater, the head squashed on top,
    // gloves and shoes sticking out; wobbling after a bounce.
    const double wob = variant == 4 ? (frame % 2 ? 1.08 : 0.94) : 1.0;
    cairo_save(cr);
    cairo_translate(cr, kM + w * 0.5, foot);
    cairo_scale(cr, wob, 2.0 - wob);
    shoe(cr, -w * 0.5, 0, 22, -1, rgb(120, 80, 60), 0.2);
    shoe(cr, w * 0.48, 0, 22, 1, rgb(120, 80, 60), -0.2);
    ellipse(cr, 0, -16, w * 0.52, 16);
    cairo_save(cr);
    cairo_clip_preserve(cr);
    setColor(cr, rgb(120, 96, 170));
    cairo_fill(cr);
    for (int i = 0; i < 3; ++i)
    {
      cairo_rectangle(cr, -w, -28 + i * 9, 2 * w, 4);
      setColor(cr, rgb(250, 220, 90));
      cairo_fill(cr);
    }
    cairo_restore(cr);
    ellipse(cr, 0, -16, w * 0.52, 16);
    ink(cr, 3.0);
    paintToonGlove(cr, -w * 0.62, -14, 22, kPi, true);
    paintToonGlove(cr, w * 0.62, -14, 22, 0, true);
    goonHead(cr, 0, -36, 1.15, 0.45, false, true);
    cairo_restore(cr);
    if (variant == 4)
      for (int i = 0; i < 3; ++i)
      {
        // Spring lines.
        const double x = kM + w * (0.2 + i * 0.3);
        cairo_move_to(cr, x, foot - 48);
        cairo_line_to(cr, x + (i - 1) * 6, foot - 64);
        ink(cr, 2.6);
      }
    return;
  }
  const bool a = frame % 2 == 0;
  const double hipY = foot - 46, shY = kM + 70;
  // Back leg and arm.
  const double fb = variant == 0 ? (a ? -14 : 12) : -16, ff = variant == 0 ? (a ? 16 : -10) : 18;
  hose(cr, cx - 14, hipY, cx - 14 + fb * 0.3, foot - 20, cx - 14 + fb, foot - 10, 13);
  shoe(cr, cx - 14 + fb, foot, 22, 1, rgb(120, 80, 60));
  const double backHandX = cx - 40, backHandY = variant == 0 ? (a ? foot - 60 : foot - 70) : foot - 74;
  hose(cr, cx - 26, shY + 12, cx - 52, shY + 50, backHandX, backHandY, 12);
  paintToonGlove(cr, backHandX, backHandY + 6, 24, kPi * 0.55, false);
  // The barrel chest in a striped sweater.
  cairo_move_to(cr, cx - 40, shY - 6);
  cairo_curve_to(cr, cx - 64, shY + 30, cx - 52, hipY + 6, cx - 30, hipY + 10);
  cairo_line_to(cr, cx + 34, hipY + 10);
  cairo_curve_to(cr, cx + 56, hipY + 6, cx + 64, shY + 30, cx + 40, shY - 6);
  cairo_curve_to(cr, cx + 20, shY - 16, cx - 20, shY - 16, cx - 40, shY - 6);
  cairo_close_path(cr);
  cairo_save(cr);
  cairo_clip_preserve(cr);
  setColor(cr, rgb(120, 96, 170));
  cairo_fill(cr);
  for (int i = 0; i < 6; ++i)
  {
    cairo_rectangle(cr, cx - 70, shY + 6 + i * 16, 140, 6);
    setColor(cr, rgb(250, 220, 90));
    cairo_fill(cr);
  }
  cairo_restore(cr);
  ink(cr, 3.2);
  // Front leg.
  hose(cr, cx + 14, hipY, cx + 14 + ff * 0.3, foot - 20, cx + 14 + ff, foot - 10, 13);
  shoe(cr, cx + 14 + ff, foot, 22, 1, rgb(120, 80, 60));
  // The head.
  goonHead(cr, cx + 8, shY - 30, 1.0, 1.0, variant >= 1, false);
  // The front arm.
  const double sx = cx + 30, sy = shY + 14;
  if (variant == 0)
  {
    const double hx = cx + 42, hy = a ? foot - 72 : foot - 62;
    hose(cr, sx, sy, cx + 60, shY + 46, hx, hy, 12);
    paintToonGlove(cr, hx, hy + 6, 26, kPi * 0.45, false);
  }
  else if (variant == 1)
  {
    // Wound up: the arm coiled like a spring behind the shoulder, the fist cocked.
    cairo_move_to(cr, sx, sy);
    for (int i = 0; i <= 40; ++i)
    {
      const double u = i / 40.0;
      const double x = sx - 10 - u * 46 + std::cos(u * 6 * kPi) * 10;
      const double y = sy - 14 + std::sin(u * 6 * kPi) * 16 - u * 10;
      cairo_line_to(cr, x, y);
    }
    setColor(cr, kInk);
    cairo_set_line_width(cr, 11);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_stroke_preserve(cr);
    setRgba(cr, rgb(120, 110, 130), 0.45);
    cairo_set_line_width(cr, 2.2);
    cairo_stroke(cr);
    paintToonGlove(cr, sx - 62, sy - 26, 28, kPi, false);
    // Wiggle lines: it is about to go.
    for (int i = 0; i < 3; ++i)
    {
      cairo_move_to(cr, sx - 86, sy - 46 + i * 16);
      cairo_line_to(cr, sx - 96, sy - 44 + i * 16);
      ink(cr, 2.4);
    }
  }
  else
  {
    // The punch: straight out of the box (the rest is goon_arm).
    cairo_move_to(cr, sx, sy);
    cairo_line_to(cr, kM + w + kM + 2, sy - 4);
    setColor(cr, kInk);
    cairo_set_line_width(cr, 11);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
    cairo_stroke_preserve(cr);
    setRgba(cr, rgb(120, 110, 130), 0.45);
    cairo_set_line_width(cr, 2.2);
    cairo_stroke(cr);
  }
}

// The punch's arm, stretched thin along the box, a big fist at its end.
void goonArm(cairo_t* cr, double w, double h)
{
  const double y = kM + h * 0.5;
  cairo_move_to(cr, 0, y + 2);
  cairo_curve_to(cr, kM + w * 0.3, y + 4, kM + w * 0.6, y - 2, kM + w - 18, y);
  setColor(cr, kInk);
  cairo_set_line_width(cr, 10);
  cairo_stroke_preserve(cr);
  setRgba(cr, rgb(120, 110, 130), 0.45);
  cairo_set_line_width(cr, 2);
  cairo_stroke(cr);
  // Stretch lines.
  for (int i = 0; i < 4; ++i)
  {
    const double x = kM + w * (0.15 + i * 0.2);
    cairo_move_to(cr, x, y - 14);
    cairo_line_to(cr, x + 16, y - 14);
    cairo_move_to(cr, x + 6, y + 15);
    cairo_line_to(cr, x + 22, y + 15);
  }
  ink(cr, 2.0);
  paintToonGlove(cr, kM + w - 4, y, 30, 0, false);
  // The impact star.
  for (int i = 0; i < 8; ++i)
  {
    const double a = i * kPi / 4;
    cairo_move_to(cr, kM + w + 20 + std::cos(a) * 10, y + std::sin(a) * 10);
    cairo_line_to(cr, kM + w + 20 + std::cos(a) * (i % 2 ? 18 : 24), y + std::sin(a) * (i % 2 ? 18 : 24));
  }
  ink(cr, 2.4);
}

// The rubber duck's white gloves, one either side (the over layer).
void duckGloves(cairo_t* cr, double w, double h, int variant)
{
  if (variant != 1)
    return;
  paintToonGlove(cr, kM + w * 0.1, kM + h * 0.62, 15, kPi * 1.15, true);
  paintToonGlove(cr, kM + w * 0.9, kM + h * 0.6, 15, -kPi * 0.2, true);
}

// --- Buddy Boom ------------------------------------------------------------------------------------

// The pose: where his hips, shoulders and head are and where his hands and
// feet go (design px, feet at y 0, facing +x).
struct Rig
{
  double hipX = 0, hipY = -150, shX = 10, shY = -290, headX = 40, headY = -370, tilt = 0;
  // Each limb: the bend's control point, then the hand or foot.
  std::array<double, 4> legFar{-35, -80, -42, -2}, legNear{40, -80, 48, -2};
  std::array<double, 4> armFar{-60, -260, -70, -200}, armNear{90, -270, 92, -200};
  double gloveFar = kPi * 0.5, gloveNear = kPi * 0.5;
  bool openFar = false, openNear = false;
  double shoeFar = 0, shoeNear = 0;
  int eyes = 0;  // 0 ahead, 1 smug, 2 wide, 3 spirals, 4 down
  int mouth = 0; // 0 grin, 1 O, 2 tongue out, 3 gritted
  int mallet = 0; // 0 none, 1 on the shoulder, 2 overhead, 3 on the floor
  bool pie = false;
  bool pencil = false;
  bool armBehind = false; // the near arm goes up behind the head
};

void buddyShirt(cairo_t* cr, const Rig& g)
{
  const double dx = g.shX - g.hipX, dy = g.shY - g.hipY;
  const double len = std::sqrt(dx * dx + dy * dy);
  const double ang = std::atan2(dx, -dy);
  cairo_save(cr);
  cairo_translate(cr, g.hipX, g.hipY);
  cairo_rotate(cr, ang);
  // Shorts, then the shirt (a pear) over them.
  cairo_move_to(cr, -70, -14);
  cairo_curve_to(cr, -74, 10, -66, 26, -40, 30);
  cairo_line_to(cr, 40, 30);
  cairo_curve_to(cr, 66, 26, 74, 10, 70, -14);
  cairo_close_path(cr);
  fillInk(cr, rgb(70, 100, 190), 3.4);
  for (const double bx : {-30.0, 30.0})
  {
    ellipse(cr, bx, 8, 10, 8);
    fillInk(cr, rgb(250, 220, 90), 2.0);
  }
  cairo_move_to(cr, -72, -4);
  cairo_curve_to(cr, -84, -len * 0.55, -60, -len - 8, -10, -len - 6);
  cairo_line_to(cr, 10, -len - 6);
  cairo_curve_to(cr, 60, -len - 8, 84, -len * 0.55, 72, -4);
  cairo_curve_to(cr, 30, 6, -30, 6, -72, -4);
  cairo_close_path(cr);
  fillGrad(cr, -len, 0, rgb(236, 72, 60), rgb(196, 42, 40), 3.4);
  // A yellow BOOM burst on the chest (no lettering: the sprite is mirrored
  // when he faces left).
  const double ey = -len * 0.5;
  for (int layer = 0; layer < 2; ++layer)
  {
    const double ro = layer ? 22 : 38, ri = layer ? 12 : 22;
    for (int i = 0; i < 20; ++i)
    {
      const double a = i * kPi / 10 - kPi * 0.5, rr = i % 2 ? ri : ro * (i % 4 ? 1.0 : 0.86);
      const double x = 6 + std::cos(a) * rr, y = ey + std::sin(a) * rr;
      if (i == 0)
        cairo_move_to(cr, x, y);
      else
        cairo_line_to(cr, x, y);
    }
    cairo_close_path(cr);
    fillInk(cr, layer ? rgb(246, 140, 40) : rgb(252, 214, 70), layer ? 1.6 : 2.6);
  }
  cairo_restore(cr);
}

// The head in profile (or from the front): the ears and the antenna, the
// black head, the peach face, pie-cut eyes, the nose and the mouth.
void buddyHead(cairo_t* cr, const Rig& g, bool front)
{
  cairo_save(cr);
  cairo_translate(cr, g.headX, g.headY);
  cairo_rotate(cr, g.tilt);
  // The antenna, MaxTV's signature.
  for (const auto& a : {std::array<double, 2>{-36, -128}, std::array<double, 2>{8, -134}})
  {
    cairo_move_to(cr, -14, -72);
    cairo_curve_to(cr, -14 + a[0] * 0.2, -100, a[0] * 0.9, a[1] + 20, a[0], a[1]);
    ink(cr, 4.0);
    circle(cr, a[0], a[1], 8);
    fillInk(cr, a[0] < 0 ? rgb(250, 210, 70) : rgb(230, 60, 60), 2.4);
  }
  // Floppy puppy ears, hanging behind the head (both sides from the front).
  const std::array<std::array<double, 5>, 2> ears =
    front ? std::array<std::array<double, 5>, 2>{{{-78, 2, 26, 62, 0.28}, {78, 2, 26, 62, -0.28}}}
          : std::array<std::array<double, 5>, 2>{{{-34, -66, 22, 44, -0.6}, {-70, 0, 27, 64, 0.38}}};
  for (const auto& e : ears)
  {
    ellipse(cr, e[0], e[1], e[2], e[3], e[4]);
    fillInk(cr, rgb(28, 22, 26), 3.0);
    shine(cr, e[0] - e[2] * 0.25, e[1] - e[3] * 0.45, e[2] * 0.22, e[3] * 0.16, e[4] - 0.3, 0.3);
  }
  // A curl of hair on top.
  for (int i = 0; i < 3; ++i)
  {
    const double hx = -10 + i * 14;
    cairo_move_to(cr, hx - 6, -74);
    cairo_curve_to(cr, hx - 10, -96, hx + 12, -100, hx + 8, -84);
  }
  ink(cr, 4.0);
  // The head.
  circle(cr, 0, 0, 78);
  {
    cairo_pattern_t* p = cairo_pattern_create_radial(-26, -30, 8, 0, 0, 78);
    stop(p, 0, rgb(80, 72, 86));
    stop(p, 0.55, rgb(36, 30, 36));
    stop(p, 1, rgb(20, 16, 20));
    cairo_set_source(cr, p);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(p);
  }
  ink(cr, 3.4);
  // The face: two lobes of peach, outlined as one (inked thick, then filled).
  const double fx = front ? 0 : 38, mx = front ? 0 : 60;
  auto faceShape = [&]() {
    ellipse(cr, fx - (front ? 22 : 0), -18, front ? 30 : 44, 44);
    if (front)
      ellipse(cr, fx + 22, -18, 30, 44);
    ellipse(cr, mx, 34, front ? 54 : 50, 36);
  };
  faceShape();
  setColor(cr, kInk);
  cairo_set_line_width(cr, 6.4);
  cairo_stroke(cr);
  faceShape();
  setColor(cr, kPeach);
  cairo_fill(cr);
  // Eyes.
  const std::array<double, 2> eyeX = front ? std::array<double, 2>{-20, 20} : std::array<double, 2>{26, 58};
  for (const double ex : eyeX)
  {
    const double ey = -28;
    const double rx = g.eyes == 2 ? 15 : 12, ry = g.eyes == 2 ? 26 : 22;
    ellipse(cr, ex, ey, rx, ry);
    fillInk(cr, rgb(255, 255, 255), 2.6);
    if (g.eyes == 3)
    {
      // Spirals.
      cairo_move_to(cr, ex, ey);
      for (int i = 0; i <= 30; ++i)
      {
        const double a = i * 0.6, rr = i * 0.36;
        cairo_line_to(cr, ex + std::cos(a) * rr, ey + std::sin(a) * rr * 1.6);
      }
      ink(cr, 2.0);
      continue;
    }
    const double px = ex + (front ? 0 : 4), py = ey + (g.eyes == 4 ? 10 : 4);
    ellipse(cr, px, py, g.eyes == 2 ? 5 : 7, g.eyes == 2 ? 9 : 13);
    setColor(cr, kInk);
    cairo_fill(cr);
    if (g.eyes != 2)
    {
      cairo_move_to(cr, px, py);
      cairo_arc(cr, px, py, 8, -kPi * 0.45, -kPi * 0.15);
      cairo_close_path(cr);
      setColor(cr, rgb(255, 255, 255));
      cairo_fill(cr);
    }
    if (g.eyes == 1)
    {
      // Half-lidded and pleased with himself.
      cairo_save(cr);
      ellipse(cr, ex, ey, rx + 1, ry + 1);
      cairo_clip(cr);
      cairo_rectangle(cr, ex - 20, ey - 30, 40, 26);
      setColor(cr, kPeach);
      cairo_fill(cr);
      cairo_restore(cr);
      cairo_move_to(cr, ex - rx, ey - 4);
      cairo_line_to(cr, ex + rx, ey - 4);
      ink(cr, 2.6);
    }
  }
  // The mouth.
  const double mo = front ? 0 : 1;
  if (g.mouth == 1)
  {
    ellipse(cr, front ? 0 : 66, 46, 14, 18);
    fillInk(cr, rgb(120, 24, 34), 2.6);
  }
  else if (g.mouth == 3)
  {
    cairo_rectangle(cr, front ? -26 : 36, 38, 52, 18);
    fillInk(cr, rgb(255, 255, 255), 2.4);
    cairo_move_to(cr, front ? -26 : 36, 47);
    cairo_line_to(cr, front ? 26 : 88, 47);
    ink(cr, 1.6);
  }
  else
  {
    const double x0 = front ? -36 : 22, x1 = front ? 36 : 96;
    cairo_move_to(cr, x0, 34);
    cairo_curve_to(cr, x0 + 10, 76, x1 - 10 * mo, 78, x1, 30);
    cairo_curve_to(cr, x1 - 20, 44, x0 + 18, 46, x0, 34);
    cairo_close_path(cr);
    fillInk(cr, rgb(120, 24, 34), 2.8);
    ellipse(cr, (x0 + x1) * 0.5 + 4, 58, 16, 8);
    setColor(cr, rgb(240, 110, 120));
    cairo_fill(cr);
    if (g.mouth == 2)
    {
      cairo_move_to(cr, (x0 + x1) * 0.5 - 6, 56);
      cairo_curve_to(cr, (x0 + x1) * 0.5 - 8, 90, (x0 + x1) * 0.5 + 18, 90, (x0 + x1) * 0.5 + 14, 56);
      cairo_close_path(cr);
      fillInk(cr, rgb(240, 110, 120), 2.4);
    }
    // Cheek creases.
    cairo_move_to(cr, x0 - 4, 28);
    cairo_curve_to(cr, x0 - 6, 34, x0 - 2, 38, x0 + 2, 38);
    ink(cr, 2.4);
  }
  // The nose: a black bulb (the game flashes it red).
  const double nx = front ? 0 : 104, ny = front ? 14 : 18;
  ellipse(cr, nx, ny, 22, 19);
  {
    cairo_pattern_t* p = cairo_pattern_create_radial(nx - 8, ny - 8, 2, nx, ny, 22);
    stop(p, 0, rgb(90, 84, 96));
    stop(p, 1, rgb(18, 14, 18));
    cairo_set_source(cr, p);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(p);
  }
  ink(cr, 2.6);
  shine(cr, nx - 7, ny - 8, 6, 3.5, -0.5, 0.8);
  cairo_restore(cr);
}

// The mallet: a wooden handle from the grip at (x, y) along `angle`, the
// barrel-shaped head at its far end.
void mallet(cairo_t* cr, double x, double y, double angle, double len)
{
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_rotate(cr, angle);
  cairo_rectangle(cr, -20, -7, len + 20, 14);
  fillInk(cr, rgb(214, 160, 96), 2.8);
  cairo_move_to(cr, 0, -2);
  cairo_line_to(cr, len, -2);
  ink(cr, 0.8);
  // The head across the handle's end.
  cairo_move_to(cr, len - 34, -56);
  cairo_curve_to(cr, len - 42, -20, len - 42, 20, len - 34, 56);
  cairo_line_to(cr, len + 34, 56);
  cairo_curve_to(cr, len + 42, 20, len + 42, -20, len + 34, -56);
  cairo_close_path(cr);
  cairo_pattern_t* p = cairo_pattern_create_linear(len - 40, 0, len + 40, 0);
  stop(p, 0, rgb(250, 214, 150));
  stop(p, 0.6, rgb(222, 166, 100));
  stop(p, 1, rgb(176, 120, 64));
  cairo_set_source(cr, p);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(p);
  ink(cr, 3.2);
  for (const double by : {-38.0, 38.0})
  {
    cairo_rectangle(cr, len - 39, by - 6, 78, 12);
    fillInk(cr, rgb(200, 60, 50), 2.2);
  }
  shine(cr, len - 16, -18, 6, 16, 0, 0.5);
  cairo_restore(cr);
}

// The giant pencil from its back end (the eraser) to its point.
void pencil(cairo_t* cr, double x0, double y0, double x1, double y1, double width)
{
  const double dx = x1 - x0, dy = y1 - y0, len = std::sqrt(dx * dx + dy * dy);
  cairo_save(cr);
  cairo_translate(cr, x0, y0);
  cairo_rotate(cr, std::atan2(dy, dx));
  const double hw = width * 0.5, cone = width * 1.5;
  // The eraser and the ferrule.
  cairo_move_to(cr, 0, -hw);
  cairo_curve_to(cr, -hw * 0.8, -hw, -hw * 0.8, hw, 0, hw);
  cairo_line_to(cr, width * 0.6, hw);
  cairo_line_to(cr, width * 0.6, -hw);
  cairo_close_path(cr);
  fillInk(cr, rgb(244, 150, 160), 2.6);
  cairo_rectangle(cr, width * 0.6, -hw, width * 0.7, width);
  fillInk(cr, rgb(196, 200, 210), 2.6);
  for (int i = 1; i < 3; ++i)
  {
    cairo_move_to(cr, width * (0.6 + i * 0.23), -hw);
    cairo_line_to(cr, width * (0.6 + i * 0.23), hw);
  }
  ink(cr, 1.4);
  // The yellow body, its facets.
  cairo_rectangle(cr, width * 1.3, -hw, len - width * 1.3 - cone, width);
  cairo_pattern_t* p = cairo_pattern_create_linear(0, -hw, 0, hw);
  stop(p, 0, rgb(255, 228, 110));
  stop(p, 0.45, rgb(250, 200, 60));
  stop(p, 1, rgb(214, 160, 40));
  cairo_set_source(cr, p);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(p);
  ink(cr, 2.8);
  cairo_move_to(cr, width * 1.3, -hw * 0.3);
  cairo_line_to(cr, len - cone, -hw * 0.3);
  cairo_move_to(cr, width * 1.3, hw * 0.35);
  cairo_line_to(cr, len - cone, hw * 0.35);
  ink(cr, 1.2);
  // The sharpened wood and the lead.
  cairo_move_to(cr, len - cone, -hw);
  cairo_line_to(cr, len, 0);
  cairo_line_to(cr, len - cone, hw);
  cairo_close_path(cr);
  fillInk(cr, rgb(246, 216, 170), 2.6);
  cairo_move_to(cr, len - cone * 0.35, -hw * 0.35);
  cairo_line_to(cr, len, 0);
  cairo_line_to(cr, len - cone * 0.35, hw * 0.35);
  cairo_close_path(cr);
  fillInk(cr, rgb(50, 46, 56), 1.6);
  cairo_restore(cr);
}

Rig buddyRig(int pose, int frame)
{
  Rig g;
  switch (pose)
  {
    case 0:
    case 1:
    {
      // Walking with the mallet on his shoulder.
      const bool b = pose == 1;
      g.legFar = b ? std::array<double, 4>{-10, -70, 30, -2} : std::array<double, 4>{-40, -70, -58, -2};
      g.legNear = b ? std::array<double, 4>{-10, -80, -40, -2} : std::array<double, 4>{50, -80, 58, -2};
      g.shoeFar = b ? 0.0 : -0.15;
      g.shoeNear = b ? -0.15 : 0.0;
      g.armFar = {-70, -250, b ? -40.0 : -80.0, -200};
      g.armNear = {80, -300, 40, -300};
      g.gloveNear = -kPi * 0.6;
      g.mallet = 1;
      g.hipY = b ? -156 : -150;
      g.shY = g.hipY - 140;
      g.headY = g.shY - 80;
      break;
    }
    case 2:
      // The mallet up overhead, leaning back.
      g.shX = -14;
      g.headX = 14;
      g.headY = -362;
      g.legFar = {-50, -80, -70, -2};
      g.legNear = {50, -80, 66, -2};
      g.armFar = {-70, -350, -34, -412};
      g.armNear = {-10, -370, -12, -418};
      g.gloveFar = -kPi * 0.5;
      g.gloveNear = -kPi * 0.5;
      g.mallet = 2;
      g.armBehind = true;
      g.mouth = 3;
      g.eyes = 2;
      break;
    case 3:
      // Bent over the mallet after the slam, nose low.
      g.hipX = -64;
      g.hipY = -168;
      g.shX = 34;
      g.shY = -206;
      g.headX = 40;
      g.headY = -114;
      g.tilt = 0.0;
      g.legFar = {-120, -90, -112, -2};
      g.legNear = {-20, -90, 6, -2};
      g.armFar = {-30, -140, -56, -18};
      g.armNear = {-60, -150, -84, -18};
      g.gloveFar = kPi * 0.5;
      g.gloveNear = kPi * 0.55;
      g.openFar = g.openNear = true;
      g.armBehind = true;
      g.mallet = 3;
      g.eyes = 3;
      g.mouth = 2;
      break;
    case 5:
      // A hop: knees up, arms flung up.
      g.hipY = -170;
      g.shY = -306;
      g.headY = -384;
      g.legFar = {-60, -150, -36, -60};
      g.legNear = {70, -160, 46, -52};
      g.shoeFar = 0.3;
      g.shoeNear = 0.4;
      g.armFar = {-110, -330, -110, -420};
      g.armNear = {120, -330, 126, -424};
      g.gloveFar = -kPi * 0.6;
      g.gloveNear = -kPi * 0.4;
      g.openFar = g.openNear = true;
      g.mouth = 1;
      g.eyes = 2;
      g.armBehind = true;
      break;
    case 6:
      // A pie wound up behind his head.
      g.shX = -10;
      g.headX = 22;
      g.legNear = {50, -80, 70, -2};
      g.armFar = {60, -250, 100, -240};
      g.gloveFar = 0;
      g.openFar = true;
      g.armNear = {-60, -330, -70, -410};
      g.gloveNear = -kPi * 0.6;
      g.openNear = true;
      g.pie = true;
      g.armBehind = true;
      g.eyes = 1;
      break;
    case 7:
      // The throw: leaning in, the near arm out, the glove open.
      g.shX = 30;
      g.headX = 60;
      g.headY = -360;
      g.legFar = {-60, -80, -80, -2};
      g.legNear = {60, -80, 70, -2};
      g.armNear = {120, -310, 150, -280};
      g.gloveNear = -0.2;
      g.openNear = true;
      g.armFar = {-90, -300, -110, -330};
      g.gloveFar = -kPi * 0.8;
      g.mouth = 1;
      break;
    case 8:
      // Riding the pencil: feet together, arms out, balancing.
      g.legFar = {-20, -70, -14, -2};
      g.legNear = {20, -70, 18, -2};
      g.armFar = {-100, -270, -150, -300};
      g.armNear = {110, -270, 150, -310};
      g.gloveFar = kPi * 1.05;
      g.gloveNear = -0.1;
      g.openFar = g.openNear = true;
      g.tilt = 0.08 * std::sin(frame * 1.6);
      break;
    case 9:
      // On his drawing, hands on hips, tapping a foot.
      g.armFar = {-130, -240, -66, -178};
      g.armNear = {140, -240, 72, -178};
      g.gloveFar = kPi * 0.1;
      g.gloveNear = kPi * 0.9;
      g.legNear = {44, -80, 56, frame % 2 ? -10.0 : -2.0};
      g.shoeNear = frame % 2 ? -0.25 : 0.0;
      g.eyes = 1;
      break;
    case 10:
      // Drawing: crouched over the giant pencil.
      g.hipX = -20;
      g.hipY = -128;
      g.shX = 18;
      g.shY = -262;
      g.headX = 56;
      g.headY = -338;
      g.legFar = {-80, -60, -70, -2};
      g.legNear = {40, -60, 50, -2};
      g.armFar = {30, -220, 40, -196};
      g.armNear = {90, -230, 100, -160};
      g.gloveFar = kPi * 0.2;
      g.gloveNear = kPi * 0.3;
      g.pencil = true;
      g.eyes = 4;
      g.mouth = 2;
      break;
    case 11:
      // Pointing the eraser down at the floor.
      g.armNear = {120, -260, 160, -200};
      g.gloveNear = kPi * 0.2;
      g.openNear = true;
      g.armFar = {-130, -240, -66, -178};
      g.gloveFar = kPi * 0.1;
      g.eyes = 4;
      break;
    case 12:
      // Sitting dizzy, legs out, head lolling.
      g.hipX = -10;
      g.hipY = -60;
      g.shX = -24;
      g.shY = -196;
      g.headX = 4;
      g.headY = -270;
      g.tilt = 0.22 * std::sin(frame * 0.8);
      g.legFar = {30, -40, 70, -6};
      g.legNear = {60, -30, 110, -4};
      g.shoeFar = -0.5;
      g.shoeNear = -0.6;
      g.armFar = {-90, -150, -96, -30};
      g.armNear = {70, -150, 96, -40};
      g.gloveFar = kPi * 0.5;
      g.gloveNear = kPi * 0.5;
      g.openFar = g.openNear = true;
      g.eyes = 3;
      g.mouth = 2;
      break;
    case 13:
    case 14:
      // Falling: arms and legs flung up, eyes wide.
      g.hipY = -170;
      g.shY = -300;
      g.headY = -380;
      g.legFar = {-80, -120, -100, -70};
      g.legNear = {80, -110, 100, -60};
      g.shoeFar = 0.6;
      g.shoeNear = -0.4;
      g.armFar = {-120, -360, -120, -450};
      g.armNear = {130, -360, 130, -446};
      g.gloveFar = -kPi * 0.55;
      g.gloveNear = -kPi * 0.45;
      g.openFar = g.openNear = true;
      g.eyes = pose == 14 ? 3 : 2;
      g.armBehind = true;
      g.mouth = 1;
      break;
    case 15:
      // Waving at the audience.
      g.armNear = {120, -320, 130, -420};
      g.gloveNear = -kPi * 0.5 + 0.3 * std::sin(frame * 1.2);
      g.openNear = true;
      g.armBehind = true;
      g.armFar = {-130, -240, -66, -178};
      g.gloveFar = kPi * 0.1;
      break;
    default:
      break;
  }
  return g;
}

// Spinning on the spot: seen from the front, squeezed as he turns, the
// limbs flung out and speed lines round him.
void buddySpin(cairo_t* cr, int frame)
{
  const double sq = std::abs(std::cos(frame * kPi / 4.0)) * 0.6 + 0.4;
  for (int i = 0; i < 6; ++i)
  {
    const double y = -420 + i * 70;
    const double side = i % 2 ? 1 : -1;
    cairo_move_to(cr, side * 120, y);
    cairo_curve_to(cr, side * 150, y + 10, side * 160, y + 26, side * 150, y + 40);
    ink(cr, 3.0);
  }
  cairo_save(cr);
  cairo_scale(cr, sq, 1.0);
  Rig g;
  g.hipX = 0;
  g.shX = 0;
  g.headX = 0;
  hose(cr, -30, -150, -36, -80, -40, -4, 20);
  hose(cr, 30, -150, 36, -80, 40, -4, 20);
  shoe(cr, -40, 0, 46, -1);
  shoe(cr, 40, 0, 46, 1);
  hose(cr, -60, -270, -110, -280, -150, -320, 18);
  hose(cr, 60, -270, 110, -260, 150, -230, 18);
  paintToonGlove(cr, -150, -320, 30, -kPi * 0.8, true);
  paintToonGlove(cr, 150, -230, 30, kPi * 0.15, true);
  buddyShirt(cr, g);
  g.eyes = 3;
  g.mouth = 1;
  buddyHead(cr, g, true);
  cairo_restore(cr);
}

// The empty skin after the air has gone: flat on its back in the ink, the
// ears and gloves and shoes round it.
void buddySkin(cairo_t* cr)
{
  shoe(cr, -150, 0, 40, -1, rgb(236, 178, 70), 0.3);
  shoe(cr, -110, 4, 40, -1, rgb(236, 178, 70), 0.2);
  ellipse(cr, -30, -14, 110, 16);
  fillInk(cr, rgb(214, 56, 50), 3.0);
  ellipse(cr, -90, -12, 30, 10);
  fillInk(cr, rgb(70, 100, 190), 2.4);
  circle(cr, 96, -24, 20);
  fillInk(cr, rgb(28, 22, 26), 2.6);
  circle(cr, 134, -20, 18);
  fillInk(cr, rgb(28, 22, 26), 2.6);
  ellipse(cr, 110, -10, 46, 14);
  fillInk(cr, rgb(28, 22, 26), 3.0);
  ellipse(cr, 130, -12, 22, 8);
  fillInk(cr, kPeach, 2.0);
  for (const double ex : {122.0, 138.0})
  {
    cairo_move_to(cr, ex - 4, -16);
    cairo_line_to(cr, ex + 4, -8);
    cairo_move_to(cr, ex + 4, -16);
    cairo_line_to(cr, ex - 4, -8);
  }
  ink(cr, 2.0);
  paintToonGlove(cr, -10, -34, 26, -kPi * 0.5, true);
  paintToonGlove(cr, 40, -30, 26, -kPi * 0.4, true);
}

} // namespace

void paintToonGlove(cairo_t* cr, double x, double y, double size, double angle, bool open)
{
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_rotate(cr, angle);
  const double s = size;
  // The flared cuff.
  cairo_move_to(cr, -s * 0.55, -s * 0.38);
  cairo_line_to(cr, -s * 1.0, -s * 0.55);
  cairo_curve_to(cr, -s * 1.12, -s * 0.2, -s * 1.12, s * 0.2, -s * 1.0, s * 0.55);
  cairo_line_to(cr, -s * 0.55, s * 0.38);
  cairo_close_path(cr);
  fillInk(cr, kGloveWhite, std::max(1.6, s * 0.1));
  if (open)
  {
    // Four fingers spread, a thumb.
    for (int i = 0; i < 4; ++i)
    {
      const double a = -0.55 + i * 0.36;
      cairo_save(cr);
      cairo_rotate(cr, a);
      ellipse(cr, s * 0.7, 0, s * 0.42, s * 0.15);
      cairo_restore(cr);
      fillInk(cr, kGloveWhite, std::max(1.4, s * 0.08));
    }
    ellipse(cr, s * 0.1, -s * 0.52, s * 0.14, s * 0.3, -0.5);
    fillInk(cr, kGloveWhite, std::max(1.4, s * 0.08));
  }
  ellipse(cr, 0, 0, s * 0.62, s * 0.5);
  fillInk(cr, kGloveWhite, std::max(1.6, s * 0.1));
  if (!open)
  {
    // A fist: the knuckles' bumps.
    for (int i = 0; i < 4; ++i)
    {
      const double ky = -s * 0.33 + i * s * 0.22;
      circle(cr, s * 0.52, ky, s * 0.15);
      fillInk(cr, kGloveWhite, std::max(1.3, s * 0.07));
    }
  }
  // The three lines down the back.
  for (int i = -1; i <= 1; ++i)
  {
    cairo_move_to(cr, -s * 0.25, i * s * 0.16);
    cairo_line_to(cr, s * 0.15, i * s * 0.16);
  }
  ink(cr, std::max(1.2, s * 0.07));
  cairo_restore(cr);
}

void paintToonPencil(cairo_t* cr, double x0, double y0, double x1, double y1, double width)
{
  pencil(cr, x0, y0, x1, y1, width);
}

void paintCreamPie(cairo_t* cr, double x, double y, double s)
{
  pie(cr, x, y, s);
}

void paintBuddy(cairo_t* cr, double cx, double feetY, double s, int pose, int frame)
{
  cairo_save(cr);
  cairo_translate(cr, cx, feetY);
  cairo_scale(cr, s, s);
  if (pose == 4)
  {
    buddySpin(cr, frame);
    cairo_restore(cr);
    return;
  }
  if (pose == 14 && frame >= 4)
  {
    buddySkin(cr);
    cairo_restore(cr);
    return;
  }
  if (pose == 14)
  {
    // Deflating: wider and lower, wrinkling, an air jet out of an ear.
    const double k = frame / 4.0;
    for (int i = 0; i < 3; ++i)
    {
      cairo_move_to(cr, -80 - i * 20, -440 + 120 * k + i * 16);
      cairo_line_to(cr, -130 - i * 26, -470 + 120 * k + i * 22);
      ink(cr, 2.6);
    }
    cairo_scale(cr, 1.0 + 0.25 * k, 1.0 - 0.55 * k);
  }
  const Rig g = buddyRig(pose, frame);
  // The far limbs.
  hose(cr, g.hipX - 26, g.hipY + 10, g.legFar[0], g.legFar[1], g.legFar[2], g.legFar[3] - 12, 20);
  shoe(cr, g.legFar[2], g.legFar[3], 46, 1, darken(rgb(236, 178, 70), 0.12f), g.shoeFar);
  const double shFarX = g.shX - 52, shFarY = g.shY + 26;
  hose(cr, shFarX, shFarY, g.armFar[0], g.armFar[1], g.armFar[2], g.armFar[3], 18);
  if (g.mallet == 2)
    mallet(cr, -20, -410, -kPi * 0.8, 80);
  paintToonGlove(cr, g.armFar[2], g.armFar[3], 30, g.gloveFar, g.openFar);
  // The mallet on the floor, under his nose.
  if (g.mallet == 3)
    mallet(cr, 66, -150, kPi * 0.5, 104);
  // The body.
  buddyShirt(cr, g);
  hose(cr, g.hipX + 26, g.hipY + 10, g.legNear[0], g.legNear[1], g.legNear[2], g.legNear[3] - 12, 20);
  shoe(cr, g.legNear[2], g.legNear[3], 48, 1, rgb(236, 178, 70), g.shoeNear);
  if (pose == 14)
  {
    // Wrinkles as the air goes.
    for (int i = 0; i < 4; ++i)
    {
      cairo_move_to(cr, g.hipX - 50 + i * 30, g.hipY - 40 - i * 16);
      cairo_curve_to(cr, g.hipX - 40 + i * 30, g.hipY - 52 - i * 16, g.hipX - 30 + i * 30, g.hipY - 30 - i * 16,
        g.hipX - 20 + i * 30, g.hipY - 44 - i * 16);
      ink(cr, 2.4);
    }
  }
  // The near arm and what it holds (behind the head when it goes up).
  const double shNearX = g.shX + 46, shNearY = g.shY + 24;
  auto nearArm = [&]() {
    if (g.pencil)
      pencil(cr, -50, -330, 168, -4, 44);
    hose(cr, shNearX, shNearY, g.armNear[0], g.armNear[1], g.armNear[2], g.armNear[3], 18);
    if (g.mallet == 1)
      mallet(cr, 40, -300, -kPi * 0.82, 120);
    if (g.pie)
      pie(cr, g.armNear[2] - 4, g.armNear[3] - 36, 1.3);
    paintToonGlove(cr, g.armNear[2], g.armNear[3], 32, g.gloveNear, g.openNear);
  };
  if (g.armBehind)
    nearArm();
  buddyHead(cr, g, false);
  if (!g.armBehind)
    nearArm();
  cairo_restore(cr);
}

void paintPiano(cairo_t* cr, double x, double y, double s, int variant)
{
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_scale(cr, s, s);
  const bool landed = variant == 1;
  // The legs and the pedals.
  for (const double lx : {22.0, 160.0})
  {
    const bool buckled = landed && lx > 100;
    cairo_move_to(cr, lx, 96);
    cairo_line_to(cr, lx + (buckled ? 14 : 0), buckled ? 112 : 126);
    cairo_line_to(cr, lx + (buckled ? 26 : 12), buckled ? 124 : 126);
    cairo_line_to(cr, lx + 12, 96);
    cairo_close_path(cr);
    fillInk(cr, rgb(70, 40, 34), 2.6);
  }
  // The case: tall, the lid, the panel.
  roundedRect(cr, 4, 4, 184, 92, 8);
  cairo_pattern_t* p = cairo_pattern_create_linear(0, 4, 0, 96);
  stop(p, 0, rgb(120, 70, 50));
  stop(p, 1, rgb(80, 44, 34));
  cairo_set_source(cr, p);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(p);
  ink(cr, 3.4);
  roundedRect(cr, 0, 0, 192, 12, 5);
  fillInk(cr, rgb(140, 84, 60), 3.0);
  roundedRect(cr, 22, 18, 148, 30, 6);
  fillInk(cr, rgb(100, 58, 42), 2.2);
  // Gold scrolls on the panel and a music sheet.
  for (const double sx : {50.0, 142.0})
  {
    cairo_move_to(cr, sx - 12, 34);
    cairo_curve_to(cr, sx - 12, 24, sx, 24, sx, 32);
    cairo_curve_to(cr, sx, 38, sx - 6, 38, sx - 6, 33);
    setColor(cr, rgb(240, 200, 90));
    cairo_set_line_width(cr, 2.4);
    cairo_stroke(cr);
  }
  cairo_rectangle(cr, 80, 20, 32, 24);
  fillInk(cr, rgb(252, 246, 230), 1.8);
  for (int i = 0; i < 3; ++i)
  {
    cairo_move_to(cr, 84, 26 + i * 6);
    cairo_line_to(cr, 108, 26 + i * 6);
  }
  ink(cr, 0.8);
  // The keyboard's shelf and the keys.
  cairo_rectangle(cr, 0, 54, 192, 26);
  fillInk(cr, rgb(110, 64, 46), 3.0);
  cairo_rectangle(cr, 10, 58, 172, 18);
  fillInk(cr, rgb(252, 250, 242), 2.0);
  for (int k = 1; k < 16; ++k)
  {
    cairo_move_to(cr, 10 + k * 172.0 / 16, 58);
    cairo_line_to(cr, 10 + k * 172.0 / 16, 76);
  }
  ink(cr, 1.0);
  for (int k = 0; k < 15; ++k)
  {
    if (k % 7 == 2 || k % 7 == 6)
      continue;
    const double kx = 10 + (k + 1) * 172.0 / 16 - 3.5;
    cairo_rectangle(cr, kx, 58, 7, 11);
    setColor(cr, kInk);
    cairo_fill(cr);
  }
  if (landed)
  {
    // Keys knocked loose, flying up; a crack down the case.
    for (int i = 0; i < 3; ++i)
    {
      cairo_save(cr);
      cairo_translate(cr, 50 + i * 46, 40 - i % 2 * 14);
      cairo_rotate(cr, -0.6 + i * 0.5);
      cairo_rectangle(cr, -4, -10, 8, 20);
      cairo_restore(cr);
      fillInk(cr, i == 1 ? kInk : rgb(252, 250, 242), 1.6);
    }
    cairo_move_to(cr, 120, 4);
    cairo_line_to(cr, 112, 24);
    cairo_line_to(cr, 124, 34);
    cairo_line_to(cr, 116, 52);
    ink(cr, 2.4);
  }
  shine(cr, 30, 8, 18, 2.4, 0, 0.45);
  cairo_restore(cr);
}

bool drawToonArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame)
{
  (void)t;
  if (key == "grinning_bomb")
    grinningBomb(cr, w, h, std::clamp(variant, 0, 2), frame);
  else if (key == "piano_bird")
    pianoBird(cr, w, h, variant == 1 ? 1 : 0, frame);
  else if (key == "rubber_goon")
    rubberGoon(cr, w, h, std::clamp(variant, 0, 4), frame);
  else if (key == "goon_arm")
    goonArm(cr, w, h);
  else if (key == "duck_gloves")
    duckGloves(cr, w, h, variant);
  else if (key == "buddy_boom")
    paintBuddy(cr, kM + w * 0.5, kM + h, h / 480.0, variant, frame);
  else
    return false;
  return true;
}

} // namespace gr
