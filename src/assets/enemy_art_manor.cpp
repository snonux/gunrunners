// Fright Night Manor (Level 23) in the style of enemy_art.cpp: the Portrait
// Ghost, the Haunted Armor and the Poltergeist's haunted objects, each drawn
// in screen pixels into a texture with a 32 px margin, the cell box at
// (32, 32) .. (32 + w, 32 + h), facing right. Also what a Poltergeist
// throws, the duck's vampire cape and the pieces the level and the
// cutscenes share: a ghost's painted face, the manor on its hill, a
// vampire's cape and a crossbow bolt.

#include "assets/enemy_art_manor.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace gr
{

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr Color kInk = rgb(16, 12, 24);
constexpr double kLine = 2.2;
constexpr double kM = 32.0;

constexpr Color kEcto = rgb(196, 255, 222);    // a ghost's body
constexpr Color kEctoDark = rgb(70, 170, 140); // its edges and shadows
constexpr Color kHaunt = rgb(120, 255, 150);   // the haunting glow

Color darken(Color c, float t) { return lerpColor(c, rgb(0, 0, 0), t); }
Color lighten(Color c, float t) { return lerpColor(c, rgb(255, 255, 255), t); }

void setRgba(cairo_t* cr, Color c, double a)
{
  cairo_set_source_rgba(cr, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, a);
}

void circle(cairo_t* cr, double x, double y, double r)
{
  cairo_new_sub_path(cr);
  cairo_arc(cr, x, y, r, 0, 2 * kPi);
}

void ellipse(cairo_t* cr, double x, double y, double rx, double ry)
{
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_scale(cr, rx, ry);
  cairo_new_sub_path(cr);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_restore(cr);
}

void stop(cairo_pattern_t* g, double at, Color c, double a = 1.0)
{
  cairo_pattern_add_color_stop_rgba(g, at, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, a);
}

void fillLinear(cairo_t* cr, double x0, double y0, double x1, double y1, Color a, Color b, double line = kLine)
{
  cairo_pattern_t* g = cairo_pattern_create_linear(x0, y0, x1, y1);
  stop(g, 0, a);
  stop(g, 1, b);
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
  stop(g, 0, light);
  stop(g, 1, dark);
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
  stop(g, 0, c, a);
  stop(g, 1, c, 0);
  cairo_set_source(cr, g);
  cairo_new_path(cr);
  cairo_arc(cr, x, y, r, 0, 2 * kPi);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
}

// A limb: an inked stroke with a coloured core through three points.
void limb(cairo_t* cr, double x0, double y0, double x1, double y1, double x2, double y2, double width, Color c,
  double line = kLine)
{
  cairo_move_to(cr, x0, y0);
  cairo_line_to(cr, x1, y1);
  cairo_line_to(cr, x2, y2);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
  setColor(cr, kInk);
  cairo_set_line_width(cr, width + 2 * line);
  cairo_stroke_preserve(cr);
  setColor(cr, c);
  cairo_set_line_width(cr, width);
  cairo_stroke(cr);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
  cairo_set_line_join(cr, CAIRO_LINE_JOIN_MITER);
}

// A ghostly fill: the body colour fading out towards (x1, y1).
void fillEcto(cairo_t* cr, double x0, double y0, double x1, double y1, double a0, double a1, Color edge = kEctoDark)
{
  cairo_pattern_t* g = cairo_pattern_create_linear(x0, y0, x1, y1);
  stop(g, 0, kEcto, a0);
  stop(g, 0.6, lerpColor(kEcto, kHaunt, 0.4f), (a0 + a1) * 0.5);
  stop(g, 1, kHaunt, a1);
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  setRgba(cr, edge, 0.7);
  cairo_set_line_width(cr, 1.6);
  cairo_stroke(cr);
}

// --- The Portrait Ghost ----------------------------------------------------------

// A powdered wig: rolled curls over the ears, a queue tied behind with a
// black bow. (cx, cy) the head's middle, s its width.
void wig(cairo_t* cr, double cx, double cy, double s, double alpha)
{
  const Color powder = rgb(236, 240, 246), shade = rgb(170, 186, 200);
  // The queue and its bow behind.
  cairo_move_to(cr, cx - s * 0.42, cy + s * 0.05);
  cairo_curve_to(cr, cx - s * 0.7, cy + s * 0.2, cx - s * 0.74, cy + s * 0.5, cx - s * 0.62, cy + s * 0.72);
  cairo_set_line_width(cr, s * 0.16);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  setRgba(cr, shade, alpha);
  cairo_stroke(cr);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
  for (const int side : {-1, 1})
  {
    cairo_move_to(cr, cx - s * 0.58, cy + s * 0.22);
    cairo_line_to(cr, cx - s * 0.58 + side * s * 0.18, cy + s * 0.12);
    cairo_line_to(cr, cx - s * 0.58 + side * s * 0.18, cy + s * 0.34);
    cairo_close_path(cr);
    setRgba(cr, rgb(30, 30, 40), alpha);
    cairo_fill(cr);
  }
  // The crown of the wig.
  ellipse(cr, cx - s * 0.04, cy - s * 0.26, s * 0.52, s * 0.34);
  cairo_pattern_t* g = cairo_pattern_create_linear(0, cy - s * 0.6, 0, cy);
  stop(g, 0, powder, alpha);
  stop(g, 1, shade, alpha);
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  setRgba(cr, darken(shade, 0.35f), alpha);
  cairo_set_line_width(cr, 1.4);
  cairo_stroke(cr);
  // Two rolled curls over each other at the side.
  for (int k = 0; k < 2; ++k)
  {
    const double ry = cy - s * 0.02 + k * s * 0.2;
    ellipse(cr, cx - s * 0.36, ry, s * 0.17, s * 0.09);
    cairo_pattern_t* cg = cairo_pattern_create_linear(0, ry - s * 0.09, 0, ry + s * 0.09);
    stop(cg, 0, powder, alpha);
    stop(cg, 1, shade, alpha);
    cairo_set_source(cr, cg);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(cg);
    setRgba(cr, darken(shade, 0.35f), alpha);
    cairo_stroke(cr);
    cairo_arc(cr, cx - s * 0.36, ry, s * 0.05, 0, 2 * kPi);
    setRgba(cr, darken(shade, 0.2f), alpha);
    cairo_fill(cr);
  }
}

// The ghost's head: hollow eye sockets with a point of light in each, a
// gaunt cheek, a mouth `open` 0..1. (cx, cy) the head's middle, s its
// width. eyes 0..1: how hard the eyes burn.
void ghostHead(cairo_t* cr, double cx, double cy, double s, double open, double eyes, bool dazed, double alpha)
{
  // The face: a long skull-like oval, chin forward.
  cairo_move_to(cr, cx - s * 0.36, cy - s * 0.2);
  cairo_curve_to(cr, cx - s * 0.4, cy + s * 0.25, cx - s * 0.1, cy + s * 0.62, cx + s * 0.16, cy + s * 0.58);
  cairo_curve_to(cr, cx + s * 0.4, cy + s * 0.52, cx + s * 0.46, cy + s * 0.1, cx + s * 0.42, cy - s * 0.18);
  cairo_curve_to(cr, cx + s * 0.36, cy - s * 0.52, cx - s * 0.3, cy - s * 0.56, cx - s * 0.36, cy - s * 0.2);
  cairo_close_path(cr);
  cairo_pattern_t* g = cairo_pattern_create_radial(cx + s * 0.1, cy - s * 0.1, s * 0.05, cx, cy, s * 0.6);
  stop(g, 0, rgb(246, 255, 250), alpha);
  stop(g, 1, kEcto, alpha * 0.85);
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  setRgba(cr, kEctoDark, alpha);
  cairo_set_line_width(cr, 1.8);
  cairo_stroke(cr);
  // A hollow cheek.
  cairo_move_to(cr, cx + s * 0.02, cy + s * 0.14);
  cairo_curve_to(cr, cx + s * 0.06, cy + s * 0.3, cx + s * 0.14, cy + s * 0.36, cx + s * 0.18, cy + s * 0.4);
  setRgba(cr, kEctoDark, alpha * 0.6);
  cairo_set_line_width(cr, 1.4);
  cairo_stroke(cr);
  // The sockets.
  for (const double ex : {0.06, 0.3})
  {
    const double x = cx + s * ex, y = cy - s * 0.06;
    ellipse(cr, x, y, s * 0.1, s * (dazed ? 0.08 : 0.12));
    setRgba(cr, rgb(20, 40, 40), alpha);
    cairo_fill(cr);
    if (dazed)
    {
      // X-ed out: pinned to the wall and none too happy about it.
      for (const int d : {-1, 1})
      {
        cairo_move_to(cr, x - s * 0.06, y - d * s * 0.05);
        cairo_line_to(cr, x + s * 0.06, y + d * s * 0.05);
      }
      setRgba(cr, kHaunt, alpha);
      cairo_set_line_width(cr, 2.0);
      cairo_stroke(cr);
      continue;
    }
    if (eyes > 0.05)
      glow(cr, x + s * 0.02, y, s * (0.1 + 0.22 * eyes), lerpColor(kHaunt, rgb(255, 255, 255), float(eyes) * 0.5f),
        0.5 + 0.5 * eyes);
    circle(cr, x + s * 0.02, y, s * (0.025 + 0.03 * eyes));
    setRgba(cr, lighten(kHaunt, 0.5f + 0.5f * float(eyes)), alpha);
    cairo_fill(cr);
  }
  // The brow, drawn down hard.
  cairo_move_to(cr, cx - s * 0.06, cy - s * 0.24);
  cairo_line_to(cr, cx + s * 0.38, cy - s * 0.18);
  setRgba(cr, kEctoDark, alpha);
  cairo_set_line_width(cr, 2.0);
  cairo_stroke(cr);
  // The mouth: a dark gaping oval, or a thin wavering line.
  if (open > 0.1)
  {
    ellipse(cr, cx + s * 0.2, cy + s * 0.32, s * (0.08 + 0.06 * open), s * (0.04 + 0.14 * open));
    setRgba(cr, rgb(16, 36, 34), alpha);
    cairo_fill(cr);
  }
  else
  {
    cairo_move_to(cr, cx + s * 0.08, cy + s * 0.32);
    cairo_curve_to(cr, cx + s * 0.14, cy + s * 0.28, cx + s * 0.22, cy + s * 0.36, cx + s * 0.3, cy + s * 0.3);
    setRgba(cr, rgb(16, 36, 34), alpha);
    cairo_set_line_width(cr, 1.8);
    cairo_stroke(cr);
  }
}

// A lace jabot and the coat's collar at the throat, (cx, cy) the knot.
void jabot(cairo_t* cr, double cx, double cy, double s, double alpha)
{
  for (int k = 0; k < 3; ++k)
  {
    const double y = cy + k * s * 0.12;
    cairo_move_to(cr, cx - s * 0.12, y);
    cairo_curve_to(cr, cx - s * 0.16, y + s * 0.1, cx + s * 0.16, y + s * 0.1, cx + s * 0.12, y);
    cairo_close_path(cr);
    setRgba(cr, rgb(250, 255, 252), alpha);
    cairo_fill_preserve(cr);
    setRgba(cr, kEctoDark, alpha);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);
  }
}

void portraitGhost(cairo_t* cr, double w, double h, int variant, int frame)
{
  const double sx = w / 128.0, sy = h / 128.0, s = std::min(sx, sy);
  auto X = [&](double x) { return kM + x * sx; };
  auto Y = [&](double y) { return kM + y * sy; };
  const double wave = frame % 2 == 0 ? 1.0 : -1.0;
  const double alpha = variant == 0 ? 0.55 : 0.9;
  if (variant == 4)
  {
    // Pinned: spread flat like a sheet nailed to the wall, the top edge a
    // ledge you can stand on, the bolt through its middle.
    glow(cr, X(64), Y(40), 70 * s, kHaunt, 0.35);
    cairo_move_to(cr, X(2), Y(6));
    cairo_line_to(cr, X(126), Y(6));
    cairo_curve_to(cr, X(122), Y(30), X(112), Y(44), X(118), Y(64));
    cairo_curve_to(cr, X(100), Y(58), X(94), Y(80), X(84), Y(96));
    cairo_curve_to(cr, X(76), Y(84), X(70), Y(100), X(62), Y(118));
    cairo_curve_to(cr, X(52), Y(100), X(46), Y(86), X(40), Y(96));
    cairo_curve_to(cr, X(30), Y(80), X(24), Y(60), X(8), Y(64));
    cairo_curve_to(cr, X(14), Y(44), X(6), Y(30), X(2), Y(6));
    cairo_close_path(cr);
    fillEcto(cr, 0, Y(6), 0, Y(118), 0.95, 0.35);
    // The ledge: the flattened top, glowing.
    cairo_rectangle(cr, X(0), Y(0), 128 * sx, 9 * sy);
    fillLinear(cr, 0, Y(0), 0, Y(9), rgb(240, 255, 246), kEctoDark, 1.4);
    // Arms flung out along the top, claws hooked over it.
    for (const int side : {-1, 1})
    {
      const double ax = side < 0 ? 10 : 118;
      limb(cr, X(64 + side * 16), Y(34), X(64 + side * 34), Y(18), X(ax), Y(12), 9 * s, kEcto, 1.2);
    }
    wig(cr, X(64), Y(36), 46 * s, 0.95);
    ghostHead(cr, X(64), Y(38), 44 * s, 0.4, 0.0, true, 0.95);
    jabot(cr, X(68), Y(60), 40 * s, 0.95);
    // The bolt, driven in at a slant, its fletching sticking out.
    cairo_save(cr);
    cairo_translate(cr, X(52), Y(74));
    cairo_rotate(cr, kPi + 0.32);
    paintBolt(cr, 58 * s);
    cairo_restore(cr);
    // A ripple round the wound.
    ellipse(cr, X(52), Y(74), 9 * s, 6 * s);
    setRgba(cr, rgb(255, 255, 255), 0.6);
    cairo_set_line_width(cr, 1.4);
    cairo_stroke(cr);
    return;
  }
  const bool tell = variant == 2, lunge = variant == 3;
  // The haunting glow round it.
  glow(cr, X(70), Y(52), (tell ? 80 : 62) * s, kHaunt, tell ? 0.45 : 0.28);
  // The body: a long-coated torso that thins to a wavering tail behind.
  const double lean = lunge ? 16 : 0;
  const double headX = 74 + lean, headY = lunge ? 40 : 34;
  cairo_move_to(cr, X(headX - 26), Y(headY + 18));
  cairo_curve_to(cr, X(headX - 40), Y(70), X(40 - lean), Y(86), X(24 - lean * 1.5), Y(98 + wave * 6));
  cairo_curve_to(cr, X(14 - lean), Y(110 - wave * 4), X(4 - lean * 0.5), Y(118), X(2), Y(126 + wave * 2));
  cairo_curve_to(cr, X(20), Y(116 + wave * 3), X(36), Y(120 - wave * 4), X(50), Y(112));
  cairo_curve_to(cr, X(64), Y(118 + wave * 4), X(78), Y(104), X(86 + lean * 0.4), Y(92));
  cairo_curve_to(cr, X(98 + lean * 0.5), Y(78), X(headX + 22), Y(headY + 40), X(headX + 16), Y(headY + 18));
  cairo_close_path(cr);
  fillEcto(cr, X(headX), Y(headY), X(10), Y(124), alpha, 0.0);
  // The coat's lapels and a row of buttons, faint.
  cairo_move_to(cr, X(headX - 14), Y(headY + 24));
  cairo_curve_to(cr, X(headX - 8), Y(headY + 40), X(headX - 4), Y(headY + 52), X(headX - 10), Y(headY + 64));
  cairo_move_to(cr, X(headX + 10), Y(headY + 22));
  cairo_curve_to(cr, X(headX + 6), Y(headY + 40), X(headX + 4), Y(headY + 54), X(headX - 2), Y(headY + 66));
  setRgba(cr, kEctoDark, alpha * 0.6);
  cairo_set_line_width(cr, 1.4);
  cairo_stroke(cr);
  for (int k = 0; k < 3; ++k)
  {
    circle(cr, X(headX - 4), Y(headY + 36 + k * 10), 2.0 * s);
    setRgba(cr, kEctoDark, alpha * 0.8);
    cairo_fill(cr);
  }
  // The far arm trailing back.
  if (!tell)
    limb(cr, X(headX - 22), Y(headY + 24), X(headX - 40), Y(headY + 44), X(headX - 52), Y(headY + 58), 8 * s,
      lerpColor(kEcto, kHaunt, 0.4f), 1.0);
  // The head.
  wig(cr, X(headX), Y(headY), 50 * s, alpha);
  ghostHead(cr, X(headX), Y(headY + 2), 48 * s, lunge ? 1.0 : (tell ? 0.7 : 0.25), tell ? 1.0 : (lunge ? 0.7 : 0.2),
    false, alpha);
  jabot(cr, X(headX + 2), Y(headY + 26), 36 * s, alpha);
  // The near arm: reaching for you, or both raised, claws out (the tell).
  const Color arm = lerpColor(kEcto, kHaunt, 0.2f);
  auto claw = [&](double x, double y, double dirA) {
    for (int k = -1; k <= 1; ++k)
    {
      const double a = dirA + k * 0.38;
      cairo_move_to(cr, x, y);
      cairo_line_to(cr, x + std::cos(a) * 11 * s, y + std::sin(a) * 11 * s);
    }
    setRgba(cr, kEcto, alpha);
    cairo_set_line_width(cr, 2.6 * s);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_stroke(cr);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
  };
  if (tell)
  {
    limb(cr, X(headX - 18), Y(headY + 26), X(headX - 34), Y(headY + 4), X(headX - 22), Y(headY - 22), 8 * s, arm, 1.0);
    claw(X(headX - 22), Y(headY - 22), -kPi * 0.5);
    limb(cr, X(headX + 16), Y(headY + 26), X(headX + 38), Y(headY + 8), X(headX + 34), Y(headY - 18), 8 * s, arm, 1.0);
    claw(X(headX + 34), Y(headY - 18), -kPi * 0.4);
  }
  else
  {
    const double reach = lunge ? 30 : 16;
    limb(cr, X(headX + 12), Y(headY + 28), X(headX + 26), Y(headY + 40), X(headX + 26 + reach), Y(headY + 34 - wave * 3),
      8 * s, arm, 1.0);
    claw(X(headX + 26 + reach), Y(headY + 34 - wave * 3), -0.1);
  }
  if (lunge)
  {
    // Streaks behind: it is coming fast.
    for (int k = 0; k < 4; ++k)
    {
      const double y = Y(40 + k * 16);
      cairo_move_to(cr, X(4), y);
      cairo_line_to(cr, X(30 - k * 3), y);
    }
    setRgba(cr, kEcto, 0.5);
    cairo_set_line_width(cr, 2.0);
    cairo_stroke(cr);
  }
}

// --- The Haunted Armor -------------------------------------------------------------

// A halberd lying along +y from the butt at (0, 0) upwards (-y), `len` px:
// an ash shaft, langets, the axe blade facing +x, the back hook and a spike.
void halberd(cairo_t* cr, double len, double s)
{
  const Color steel = rgb(196, 202, 218), steelDark = rgb(90, 96, 112);
  // The shaft.
  cairo_rectangle(cr, -2.6 * s, -len, 5.2 * s, len);
  fillLinear(cr, -2.6 * s, 0, 2.6 * s, 0, rgb(150, 104, 64), rgb(80, 50, 28), 1.4);
  // Leather grips.
  for (const double gy : {0.35, 0.62})
  {
    cairo_rectangle(cr, -3.4 * s, -len * gy - 5 * s, 6.8 * s, 10 * s);
    fillLinear(cr, 0, -len * gy - 5 * s, 0, -len * gy + 5 * s, rgb(90, 50, 40), rgb(46, 24, 20), 1.0);
  }
  const double top = -len;
  // The spike.
  cairo_move_to(cr, -3 * s, top + 6 * s);
  cairo_line_to(cr, 0, top - 22 * s);
  cairo_line_to(cr, 3 * s, top + 6 * s);
  cairo_close_path(cr);
  fillLinear(cr, -3 * s, 0, 3 * s, 0, lighten(steel, 0.3f), steelDark, 1.4);
  // The axe blade: a broad crescent facing forward.
  cairo_move_to(cr, 2 * s, top + 8 * s);
  cairo_line_to(cr, 14 * s, top + 2 * s);
  cairo_curve_to(cr, 22 * s, top + 10 * s, 24 * s, top + 26 * s, 20 * s, top + 40 * s);
  cairo_line_to(cr, 2 * s, top + 30 * s);
  cairo_close_path(cr);
  fillLinear(cr, 0, top, 24 * s, top, steel, lighten(steel, 0.4f), 1.6);
  cairo_move_to(cr, 15 * s, top + 4 * s);
  cairo_curve_to(cr, 22 * s, top + 12 * s, 23 * s, top + 26 * s, 19 * s, top + 38 * s);
  setRgba(cr, rgb(255, 255, 255), 0.8);
  cairo_set_line_width(cr, 1.2);
  cairo_stroke(cr);
  // The back hook.
  cairo_move_to(cr, -2 * s, top + 14 * s);
  cairo_line_to(cr, -12 * s, top + 12 * s);
  cairo_line_to(cr, -16 * s, top + 22 * s);
  cairo_line_to(cr, -2 * s, top + 22 * s);
  cairo_close_path(cr);
  fillLinear(cr, -16 * s, 0, 0, 0, steelDark, steel, 1.4);
  // The socket and its rivets.
  cairo_rectangle(cr, -4 * s, top + 4 * s, 8 * s, 30 * s);
  fillLinear(cr, -4 * s, 0, 4 * s, 0, steel, steelDark, 1.2);
  for (const double ry : {10.0, 26.0})
  {
    circle(cr, 0, top + ry * s, 1.4 * s);
    setColor(cr, rgb(240, 240, 250));
    cairo_fill(cr);
  }
}

// A plate: a rounded shape filled steel with a cold highlight.
void plateFill(cairo_t* cr, double x0, double x1, Color steel, double line = 1.8)
{
  fillLinear(cr, x0, 0, x1, 0, lighten(steel, 0.35f), darken(steel, 0.35f), line);
}

void hauntedArmor(cairo_t* cr, double w, double h, int variant, int frame)
{
  const double sx = w / 96.0, sy = h / 192.0, s = std::min(sx, sy);
  auto X = [&](double x) { return kM + x * sx; };
  auto Y = [&](double y) { return kM + y * sy; };
  const Color steel = rgb(150, 156, 176), steelDark = darken(steel, 0.4f);
  const Color rust = rgb(140, 80, 50);
  const bool walking = variant == 1, tell = variant == 2, swing = variant == 3;
  const double stride = walking ? (frame % 2 == 0 ? 7.0 : -7.0) : 0.0;
  const double bob = walking ? 2.0 : 0.0;
  // The halberd behind the body when it is held upright or raised.
  if (variant == 0 || walking)
  {
    cairo_save(cr);
    cairo_translate(cr, X(80), Y(188));
    halberd(cr, 182 * sy, s);
    cairo_restore(cr);
  }
  else if (tell)
  {
    // Raised back over the shoulder, blade high.
    cairo_save(cr);
    cairo_translate(cr, X(60), Y(120));
    cairo_rotate(cr, -0.75);
    halberd(cr, 170 * sy, s);
    cairo_restore(cr);
  }
  // Legs: cuisses, poleyns, greaves, sabatons.
  for (const int k : {0, 1})
  {
    const double off = k == 0 ? -stride : stride;
    const double lx = (k == 0 ? 30 : 50) + off * 0.5;
    const Color c = k == 0 ? darken(steel, 0.2f) : steel;
    cairo_rectangle(cr, X(lx), Y(118 + bob), 16 * sx, 30 * sy);
    plateFill(cr, X(lx), X(lx + 16), c);
    circle(cr, X(lx + 8), Y(150 + bob), 8 * s);
    fillRadial(cr, X(lx + 8), Y(150 + bob), 8 * s, lighten(c, 0.4f), darken(c, 0.4f), 1.6);
    cairo_rectangle(cr, X(lx + 1 + off * 0.3), Y(156), 14 * sx, 26 * sy);
    plateFill(cr, X(lx), X(lx + 16), c);
    cairo_move_to(cr, X(lx - 1 + off * 0.4), Y(180));
    cairo_line_to(cr, X(lx + 16 + off * 0.4), Y(180));
    cairo_line_to(cr, X(lx + 24 + off * 0.4), Y(190));
    cairo_line_to(cr, X(lx - 1 + off * 0.4), Y(191));
    cairo_close_path(cr);
    plateFill(cr, X(lx), X(lx + 24), c, 1.6);
  }
  // The tassets over the hips.
  cairo_move_to(cr, X(24), Y(104 + bob));
  cairo_line_to(cr, X(72), Y(104 + bob));
  cairo_line_to(cr, X(76), Y(126 + bob));
  cairo_line_to(cr, X(20), Y(126 + bob));
  cairo_close_path(cr);
  plateFill(cr, X(20), X(76), steel);
  for (const double ty : {111.0, 118.0})
  {
    cairo_move_to(cr, X(23), Y(ty + bob));
    cairo_line_to(cr, X(73), Y(ty + bob));
  }
  setRgba(cr, steelDark, 0.9);
  cairo_set_line_width(cr, 1.2);
  cairo_stroke(cr);
  // The back arm.
  if (!swing)
    limb(cr, X(30), Y(56 + bob), X(22), Y(80 + bob), X(26), Y(100 + bob), 12 * s, darken(steel, 0.3f), 1.8);
  // The breastplate: a ridged cuirass.
  cairo_move_to(cr, X(24), Y(50 + bob));
  cairo_curve_to(cr, X(22), Y(76 + bob), X(28), Y(98 + bob), X(48), Y(106 + bob));
  cairo_curve_to(cr, X(68), Y(98 + bob), X(74), Y(76 + bob), X(72), Y(50 + bob));
  cairo_curve_to(cr, X(60), Y(44 + bob), X(36), Y(44 + bob), X(24), Y(50 + bob));
  cairo_close_path(cr);
  plateFill(cr, X(22), X(74), steel, 2.0);
  cairo_move_to(cr, X(50), Y(48 + bob));
  cairo_curve_to(cr, X(48), Y(70 + bob), X(48), Y(90 + bob), X(48), Y(104 + bob));
  setRgba(cr, rgb(240, 244, 255), 0.7);
  cairo_set_line_width(cr, 1.6);
  cairo_stroke(cr);
  // Rust blooms and a dent.
  for (const auto& rs : {std::pair<double, double>{34, 86}, {62, 66}, {40, 120}})
  {
    ellipse(cr, X(rs.first), Y(rs.second + bob), 4 * s, 2.6 * s);
    setRgba(cr, rust, 0.6);
    cairo_fill(cr);
  }
  ellipse(cr, X(60), Y(84 + bob), 5 * s, 3 * s);
  setRgba(cr, darken(steel, 0.5f), 0.5);
  cairo_set_line_width(cr, 1.0);
  cairo_stroke(cr);
  // Pauldrons.
  for (const int side : {-1, 1})
  {
    const double px = 48 + side * 26;
    ellipse(cr, X(px), Y(52 + bob), 14 * sx, 11 * sy);
    fillRadial(cr, X(px), Y(52 + bob), 14 * s, lighten(steel, 0.35f), darken(steel, 0.4f), 1.8);
  }
  // The gorget and the helmet: an armet with a pointed visor and a crest.
  cairo_rectangle(cr, X(38), Y(36 + bob), 22 * sx, 12 * sy);
  plateFill(cr, X(38), X(60), steel, 1.6);
  const double hx = 50, hy = 22 + bob;
  cairo_move_to(cr, X(hx - 15), Y(hy + 14));
  cairo_curve_to(cr, X(hx - 18), Y(hy - 6), X(hx - 10), Y(hy - 18), X(hx + 2), Y(hy - 18));
  cairo_curve_to(cr, X(hx + 14), Y(hy - 18), X(hx + 18), Y(hy - 8), X(hx + 17), Y(hy + 2));
  cairo_line_to(cr, X(hx + 25), Y(hy + 6));
  cairo_line_to(cr, X(hx + 15), Y(hy + 16));
  cairo_close_path(cr);
  plateFill(cr, X(hx - 18), X(hx + 25), steel, 2.0);
  // The visor's slit and breaths.
  cairo_rectangle(cr, X(hx), Y(hy - 2), 20 * sx, 3.4 * sy);
  setColor(cr, rgb(10, 10, 14));
  cairo_fill(cr);
  if (walking || tell || swing)
  {
    const double a = tell ? 1.0 : (swing ? 0.8 : 0.35);
    const Color eye = tell ? rgb(255, 80, 60) : kHaunt;
    glow(cr, X(hx + 12), Y(hy), (tell ? 26 : 14) * s, eye, 0.6 * a + 0.2);
    cairo_rectangle(cr, X(hx + 4), Y(hy - 1.4), 14 * sx, 2 * sy);
    setRgba(cr, lighten(eye, 0.4f), a);
    cairo_fill(cr);
  }
  for (int k = 0; k < 3; ++k)
  {
    circle(cr, X(hx + 12 + k * 3), Y(hy + 8), 0.9 * s);
    setColor(cr, rgb(20, 20, 26));
    cairo_fill(cr);
  }
  // The crest: a tired, moth-eaten red plume.
  cairo_move_to(cr, X(hx), Y(hy - 17));
  cairo_curve_to(cr, X(hx - 10), Y(hy - 32), X(hx - 28), Y(hy - 28), X(hx - 34 - (walking ? stride * 0.4 : 0)), Y(hy - 12));
  cairo_curve_to(cr, X(hx - 22), Y(hy - 20), X(hx - 10), Y(hy - 18), X(hx), Y(hy - 17));
  cairo_close_path(cr);
  fillLinear(cr, X(hx - 34), 0, X(hx), 0, rgb(120, 20, 30), rgb(200, 50, 60), 1.4);
  // The near arm and the gauntlet; the halberd in front when swinging.
  if (swing)
  {
    // Down in front: the shaft from the hands out past the box, the blade
    // at the end biting downwards.
    limb(cr, X(64), Y(56 + bob), X(80), Y(78), X(92), Y(90), 13 * s, steel, 1.8);
    cairo_save(cr);
    cairo_translate(cr, X(36), Y(84));
    cairo_rotate(cr, kPi * 0.5 + 0.18);
    halberd(cr, 120 * s, s);
    cairo_restore(cr);
    circle(cr, X(92), Y(90), 7 * s);
    fillRadial(cr, X(92), Y(90), 7 * s, lighten(steel, 0.3f), steelDark, 1.6);
  }
  else if (tell)
  {
    limb(cr, X(66), Y(56 + bob), X(76), Y(76 + bob), X(62), Y(96 + bob), 13 * s, steel, 1.8);
    circle(cr, X(62), Y(98 + bob), 7 * s);
    fillRadial(cr, X(62), Y(98 + bob), 7 * s, lighten(steel, 0.3f), steelDark, 1.6);
  }
  else
  {
    limb(cr, X(66), Y(56 + bob), X(76), Y(80 + bob), X(80), Y(104 + bob), 13 * s, steel, 1.8);
    circle(cr, X(80), Y(104 + bob), 7 * s);
    fillRadial(cr, X(80), Y(104 + bob), 7 * s, lighten(steel, 0.3f), steelDark, 1.6);
  }
  if (variant == 0)
  {
    // A cobweb from the helmet to the pauldron: it has stood here a while.
    cairo_set_line_width(cr, 0.7);
    setRgba(cr, rgb(230, 230, 240), 0.55);
    for (int k = 0; k < 5; ++k)
    {
      cairo_move_to(cr, X(hx - 14), Y(hy + 6));
      cairo_line_to(cr, X(20 + k * 3), Y(44 + k * 2.5));
    }
    cairo_stroke(cr);
    for (int ring = 1; ring <= 3; ++ring)
    {
      const double f = ring / 4.0;
      cairo_move_to(cr, X(hx - 14 + (20 - (hx - 14)) * f), Y(hy + 6 + (44 - (hy + 6)) * f));
      cairo_line_to(cr, X(hx - 14 + (32 - (hx - 14)) * f), Y(hy + 6 + (54 - (hy + 6)) * f));
    }
    cairo_stroke(cr);
  }
}

// --- The Poltergeist's things --------------------------------------------------------

void ghostGlow(cairo_t* cr, double cx, double cy, double r, int frame, bool rattle)
{
  glow(cr, cx, cy, r * (rattle ? 1.3 : 1.0), kHaunt, (frame % 2 ? 0.32 : 0.24) + (rattle ? 0.2 : 0.0));
}

// A heavy tome lying on its side, gold corners and a sigil. open 0 shut
// .. 1 its cover flung up and pages fanning.
void book(cairo_t* cr, double cx, double by, double s, double open, Color cover)
{
  const double bw = 46 * s, bh = 14 * s;
  // Pages (the block's edge).
  cairo_rectangle(cr, cx - bw * 0.5 + 2 * s, by - bh, bw - 4 * s, bh - 3 * s);
  fillLinear(cr, 0, by - bh, 0, by, rgb(250, 240, 210), rgb(190, 170, 130), 1.2);
  for (int k = 1; k < 4; ++k)
  {
    cairo_move_to(cr, cx - bw * 0.5 + 3 * s, by - bh + k * 3 * s);
    cairo_line_to(cr, cx + bw * 0.5 - 3 * s, by - bh + k * 3 * s);
  }
  setRgba(cr, rgb(150, 130, 100), 0.6);
  cairo_set_line_width(cr, 0.7);
  cairo_stroke(cr);
  // The back cover under it.
  cairo_rectangle(cr, cx - bw * 0.5, by - 3 * s, bw, 3 * s);
  fillLinear(cr, 0, by - 3 * s, 0, by, cover, darken(cover, 0.4f), 1.2);
  if (open < 0.05)
  {
    // The front cover lying on top, a sigil glowing on it.
    cairo_rectangle(cr, cx - bw * 0.5, by - bh - 3 * s, bw, 4 * s);
    fillLinear(cr, 0, by - bh - 3 * s, 0, by - bh + s, lighten(cover, 0.2f), cover, 1.2);
    for (const int side : {-1, 1})
    {
      cairo_rectangle(cr, cx + side * (bw * 0.5 - 4 * s) - 3 * s, by - bh - 3 * s, 6 * s, 4 * s);
      setColor(cr, rgb(230, 190, 80));
      cairo_fill(cr);
    }
    return;
  }
  // The cover thrown up on its spine (the left edge), pages fanning.
  for (int k = 0; k < 4; ++k)
  {
    const double a = -open * (0.4 + k * 0.25);
    cairo_save(cr);
    cairo_translate(cr, cx - bw * 0.5 + 2 * s, by - bh);
    cairo_rotate(cr, a);
    cairo_move_to(cr, 0, 0);
    cairo_curve_to(cr, bw * 0.3, -3 * s, bw * 0.6, -4 * s, bw - 4 * s, -1 * s);
    cairo_line_to(cr, bw - 4 * s, 0);
    cairo_close_path(cr);
    setRgba(cr, rgb(248, 238, 210), 0.95);
    cairo_fill_preserve(cr);
    setRgba(cr, rgb(150, 130, 100), 0.9);
    cairo_set_line_width(cr, 0.8);
    cairo_stroke(cr);
    cairo_restore(cr);
  }
  cairo_save(cr);
  cairo_translate(cr, cx - bw * 0.5, by - bh);
  cairo_rotate(cr, -open * 1.6);
  cairo_rectangle(cr, 0, -2 * s, bw, 4 * s);
  fillLinear(cr, 0, -2 * s, 0, 2 * s, lighten(cover, 0.2f), cover, 1.2);
  cairo_restore(cr);
}

void tureen(cairo_t* cr, double cx, double by, double s, double lid)
{
  const Color china = rgb(240, 244, 250), chinaDark = rgb(170, 184, 210), blue = rgb(50, 80, 170);
  // The foot.
  cairo_move_to(cr, cx - 12 * s, by);
  cairo_line_to(cr, cx - 9 * s, by - 5 * s);
  cairo_line_to(cr, cx + 9 * s, by - 5 * s);
  cairo_line_to(cr, cx + 12 * s, by);
  cairo_close_path(cr);
  fillLinear(cr, cx - 12 * s, 0, cx + 12 * s, 0, china, chinaDark, 1.4);
  // The bowl.
  cairo_move_to(cr, cx - 22 * s, by - 22 * s);
  cairo_curve_to(cr, cx - 22 * s, by - 6 * s, cx - 12 * s, by - 4 * s, cx, by - 4 * s);
  cairo_curve_to(cr, cx + 12 * s, by - 4 * s, cx + 22 * s, by - 6 * s, cx + 22 * s, by - 22 * s);
  cairo_close_path(cr);
  fillLinear(cr, cx - 22 * s, 0, cx + 22 * s, 0, china, chinaDark, 1.6);
  // A band of blue willow pattern and the handles.
  cairo_rectangle(cr, cx - 21 * s, by - 18 * s, 42 * s, 4 * s);
  setRgba(cr, blue, 0.85);
  cairo_fill(cr);
  for (int k = 0; k < 5; ++k)
  {
    circle(cr, cx - 16 * s + k * 8 * s, by - 10 * s, 1.6 * s);
    setRgba(cr, blue, 0.7);
    cairo_fill(cr);
  }
  for (const int side : {-1, 1})
  {
    ellipse(cr, cx + side * 24 * s, by - 17 * s, 4 * s, 3 * s);
    setColor(cr, kInk);
    cairo_set_line_width(cr, 3.2 * s);
    cairo_stroke_preserve(cr);
    setColor(cr, china);
    cairo_set_line_width(cr, 1.8 * s);
    cairo_stroke(cr);
  }
  // Green soup breathing out when the lid lifts.
  if (lid > 0.05)
  {
    ellipse(cr, cx, by - 22 * s, 20 * s, 3 * s);
    setColor(cr, rgb(120, 220, 90));
    cairo_fill(cr);
    for (int k = 0; k < 3; ++k)
    {
      cairo_move_to(cr, cx - 8 * s + k * 8 * s, by - 24 * s);
      cairo_curve_to(cr, cx - 14 * s + k * 8 * s, by - 32 * s, cx - 2 * s + k * 8 * s, by - 36 * s, cx - 8 * s + k * 8 * s,
        by - 44 * s);
    }
    setRgba(cr, kHaunt, 0.6);
    cairo_set_line_width(cr, 2.0 * s);
    cairo_stroke(cr);
  }
  // The lid and its knob, lifted and tipped while it rattles.
  cairo_save(cr);
  cairo_translate(cr, cx, by - 22 * s - lid * 14 * s);
  cairo_rotate(cr, lid * 0.3);
  cairo_move_to(cr, -21 * s, 0);
  cairo_curve_to(cr, -18 * s, -10 * s, 18 * s, -10 * s, 21 * s, 0);
  cairo_close_path(cr);
  fillLinear(cr, -21 * s, 0, 21 * s, 0, china, chinaDark, 1.6);
  circle(cr, 0, -9 * s, 3.5 * s);
  fillRadial(cr, 0, -9 * s, 3.5 * s, rgb(255, 255, 255), chinaDark, 1.2);
  cairo_restore(cr);
}

// A brass candle holder: (cx, by) its foot; `arms` 1 (a candlestick) or 3
// (a candelabra); flare 0..1 the flames leaping green.
void candles(cairo_t* cr, double cx, double by, double s, int arms, double flare, int frame)
{
  const Color brass = rgb(214, 170, 70), brassDark = rgb(120, 84, 30);
  ellipse(cr, cx, by - 2 * s, 11 * s, 3 * s);
  fillLinear(cr, cx - 11 * s, 0, cx + 11 * s, 0, brass, brassDark, 1.4);
  cairo_rectangle(cr, cx - 2 * s, by - (arms == 3 ? 22 : 14) * s, 4 * s, (arms == 3 ? 20 : 12) * s);
  fillLinear(cr, cx - 2 * s, 0, cx + 2 * s, 0, brass, brassDark, 1.2);
  std::vector<std::pair<double, double>> cups;
  if (arms == 3)
  {
    cairo_move_to(cr, cx - 16 * s, by - 34 * s);
    cairo_curve_to(cr, cx - 16 * s, by - 22 * s, cx + 16 * s, by - 22 * s, cx + 16 * s, by - 34 * s);
    setColor(cr, kInk);
    cairo_set_line_width(cr, 4.4 * s);
    cairo_stroke_preserve(cr);
    setColor(cr, brass);
    cairo_set_line_width(cr, 2.6 * s);
    cairo_stroke(cr);
    cups = {{cx - 16 * s, by - 34 * s}, {cx, by - 38 * s}, {cx + 16 * s, by - 34 * s}};
    cairo_rectangle(cr, cx - 1.6 * s, by - 38 * s, 3.2 * s, 16 * s);
    fillLinear(cr, cx - 2 * s, 0, cx + 2 * s, 0, brass, brassDark, 1.0);
  }
  else
    cups = {{cx, by - 14 * s}};
  int k = 0;
  for (const auto& c : cups)
  {
    ellipse(cr, c.first, c.second, 5 * s, 2 * s);
    fillLinear(cr, c.first - 5 * s, 0, c.first + 5 * s, 0, brass, brassDark, 1.0);
    // The candle, wax dripping.
    const double ch = (arms == 3 ? 12 : 16) * s;
    cairo_rectangle(cr, c.first - 2.6 * s, c.second - ch, 5.2 * s, ch);
    fillLinear(cr, c.first - 2.6 * s, 0, c.first + 2.6 * s, 0, rgb(250, 246, 230), rgb(200, 190, 170), 1.0);
    cairo_move_to(cr, c.first + 1 * s, c.second - ch);
    cairo_line_to(cr, c.first + 1.5 * s, c.second - ch + 5 * s);
    setColor(cr, rgb(250, 246, 230));
    cairo_set_line_width(cr, 1.6 * s);
    cairo_stroke(cr);
    // The flame.
    const double fh = (7 + 6 * flare) * s, fy = c.second - ch - 1 * s;
    const double lean = ((frame + k) % 2 ? 1.0 : -1.0) * (1 + 2 * flare) * s;
    const Color fc = lerpColor(rgb(255, 200, 80), kHaunt, float(0.35 + 0.65 * flare));
    glow(cr, c.first, fy - fh * 0.5, fh * 1.6, fc, 0.5);
    cairo_move_to(cr, c.first - 2.4 * s, fy);
    cairo_curve_to(cr, c.first - 3 * s, fy - fh * 0.5, c.first + lean, fy - fh * 0.8, c.first + lean, fy - fh);
    cairo_curve_to(cr, c.first + lean * 0.5, fy - fh * 0.6, c.first + 3 * s, fy - fh * 0.4, c.first + 2.4 * s, fy);
    cairo_close_path(cr);
    setColor(cr, fc);
    cairo_fill(cr);
    ellipse(cr, c.first, fy - fh * 0.3, 1.2 * s, 2.2 * s);
    setColor(cr, rgb(255, 255, 240));
    cairo_fill(cr);
    ++k;
  }
}

void poltergeist(cairo_t* cr, double w, double h, int variant, int frame)
{
  const double s = std::min(w, h) / 64.0;
  const bool rattle = variant >= 3;
  const int kind = variant % 3;
  const double cx = kM + w * 0.5, by = kM + h - 2 * s;
  ghostGlow(cr, cx, by - 22 * s, 34 * s, frame, rattle);
  if (kind == 0)
  {
    book(cr, cx, by, s * 1.2, rattle ? (frame % 2 ? 0.7 : 0.4) : 0.0, rgb(110, 30, 40));
    if (!rattle)
    {
      // A sigil burning on the cover.
      circle(cr, cx, by - 19 * s, 4.5 * s);
      setRgba(cr, kHaunt, 0.9);
      cairo_set_line_width(cr, 1.2 * s);
      cairo_stroke(cr);
    }
  }
  else if (kind == 1)
    tureen(cr, cx, by, s * 1.2, rattle ? (frame % 2 ? 1.0 : 0.6) : 0.0);
  else
    candles(cr, cx, by, s * 1.25, 3, rattle ? 1.0 : 0.2, frame);
  // A pair of eyes in the glow, watching.
  const double ey = by - (kind == 2 ? 50 : 36) * s;
  for (const double ex : {-5.0, 5.0})
  {
    ellipse(cr, cx + ex * s + 4 * s, ey, 2.4 * s, (rattle ? 3.2 : 1.6) * s);
    setRgba(cr, lighten(kHaunt, 0.5f), rattle ? 0.95 : 0.6);
    cairo_fill(cr);
  }
}

void thrown(cairo_t* cr, double w, double h, int variant, int frame)
{
  const double s = std::min(w, h) / 64.0;
  const double cx = kM + w * 0.5, cy = kM + h * 0.5;
  if (variant == 0)
  {
    // A book flapping through the air like a bat.
    const double flap = frame % 2 ? 0.9 : 0.25;
    for (const int side : {-1, 1})
    {
      cairo_move_to(cr, cx, cy + 4 * s);
      cairo_line_to(cr, cx + side * 22 * s, cy + 4 * s - flap * 18 * s);
      cairo_line_to(cr, cx + side * 22 * s, cy + 18 * s - flap * 18 * s);
      cairo_line_to(cr, cx, cy + 18 * s);
      cairo_close_path(cr);
      fillLinear(cr, cx, 0, cx + side * 22 * s, 0, rgb(250, 240, 210), rgb(200, 180, 140), 1.2);
      cairo_move_to(cr, cx - side * 0.0, cy + 2 * s);
      cairo_line_to(cr, cx + side * 24 * s, cy + 2 * s - flap * 18 * s);
      cairo_line_to(cr, cx + side * 24 * s, cy + 6 * s - flap * 18 * s);
      cairo_line_to(cr, cx, cy + 6 * s);
      cairo_close_path(cr);
      fillLinear(cr, 0, cy, 0, cy + 6 * s, rgb(150, 40, 50), rgb(90, 20, 30), 1.2);
    }
  }
  else if (variant == 1)
  {
    // A dinner plate on edge, spinning: fine china with a blue rim.
    ellipse(cr, cx, cy, 20 * s, 20 * s);
    fillRadial(cr, cx, cy, 20 * s, rgb(255, 255, 255), rgb(196, 206, 226), 1.6);
    ellipse(cr, cx, cy, 16 * s, 16 * s);
    setRgba(cr, rgb(50, 80, 170), 0.9);
    cairo_set_line_width(cr, 2.0 * s);
    cairo_stroke(cr);
    ellipse(cr, cx, cy, 9 * s, 9 * s);
    setRgba(cr, rgb(170, 184, 210), 0.8);
    cairo_set_line_width(cr, 1.0 * s);
    cairo_stroke(cr);
    for (int k = 0; k < 8; ++k)
    {
      const double a = k * kPi / 4;
      circle(cr, cx + std::cos(a) * 13 * s, cy + std::sin(a) * 13 * s, 1.3 * s);
      setRgba(cr, rgb(50, 80, 170), 0.8);
      cairo_fill(cr);
    }
  }
  else
  {
    // A lit candlestick, end over end.
    candles(cr, cx, cy + 16 * s, s * 1.3, 1, 0.3, frame);
  }
}

} // namespace

// --- Shared pieces -----------------------------------------------------------------

void paintGhostFace(cairo_t* cr, double cx, double cy, double s, double flare)
{
  // The coat and its collar at the bottom of the frame.
  cairo_move_to(cr, cx - s * 0.9, cy + s * 1.2);
  cairo_curve_to(cr, cx - s * 0.8, cy + s * 0.7, cx - s * 0.4, cy + s * 0.55, cx, cy + s * 0.55);
  cairo_curve_to(cr, cx + s * 0.4, cy + s * 0.55, cx + s * 0.8, cy + s * 0.7, cx + s * 0.9, cy + s * 1.2);
  cairo_close_path(cr);
  fillLinear(cr, 0, cy + s * 0.5, 0, cy + s * 1.2, rgb(70, 40, 80), rgb(30, 16, 36), 1.6);
  jabot(cr, cx + s * 0.04, cy + s * 0.58, s * 0.7, 1.0);
  wig(cr, cx, cy, s, 1.0);
  // Painted flesh, gone grey-green with age, and its eyes following you.
  ghostHead(cr, cx, cy + s * 0.04, s * 0.96, flare > 0.6 ? 0.5 : 0.0, flare, false, 1.0);
}

void paintHauntedManor(cairo_t* cr, double cx, double base, double s, Color body, Color lit)
{
  auto rect = [&](double x, double y, double w, double h) { cairo_rectangle(cr, cx + x * s, base - y * s, w * s, h * s); };
  auto tri = [&](double x0, double y0, double x1, double y1, double x2, double y2) {
    cairo_move_to(cr, cx + x0 * s, base - y0 * s);
    cairo_line_to(cr, cx + x1 * s, base - y1 * s);
    cairo_line_to(cr, cx + x2 * s, base - y2 * s);
    cairo_close_path(cr);
  };
  rect(-150, 130, 300, 130);
  tri(-162, 130, 0, 196, 162, 130);
  rect(-210, 240, 64, 240);
  tri(-218, 240, -178, 350, -138, 240);
  rect(132, 190, 60, 190);
  tri(124, 190, 162, 280, 200, 190);
  tri(-44, 130, 0, 230, 44, 130);
  rect(-90, 230, 16, 60);
  rect(70, 220, 16, 54);
  rect(-190, 30, 380, 30);
  setColor(cr, body);
  cairo_fill(cr);
  // Spikes on the spires and a crooked weathervane.
  cairo_move_to(cr, cx - 178 * s, base - 350 * s);
  cairo_line_to(cr, cx - 178 * s, base - 380 * s);
  cairo_move_to(cr, cx - 190 * s, base - 370 * s);
  cairo_line_to(cr, cx - 166 * s, base - 372 * s);
  cairo_move_to(cr, cx + 162 * s, base - 280 * s);
  cairo_line_to(cr, cx + 162 * s, base - 300 * s);
  setColor(cr, body);
  cairo_set_line_width(cr, 3 * s);
  cairo_stroke(cr);
  auto window = [&](double x, double y, bool on) {
    const double w = 14 * s, h = 26 * s;
    const double wx = cx + x * s, wy = base - y * s;
    if (on)
      radialGlow(cr, wx + w * 0.5, wy + h * 0.5, 30 * s, lit, 0.35);
    cairo_move_to(cr, wx, wy + h);
    cairo_line_to(cr, wx, wy + w * 0.5);
    cairo_arc(cr, wx + w * 0.5, wy + w * 0.5, w * 0.5, kPi, 2 * kPi);
    cairo_line_to(cr, wx + w, wy + h);
    cairo_close_path(cr);
    setColor(cr, on ? lit : darken(body, 0.5f));
    cairo_fill(cr);
    if (on)
    {
      cairo_move_to(cr, wx + w * 0.5, wy);
      cairo_line_to(cr, wx + w * 0.5, wy + h);
      cairo_move_to(cr, wx, wy + h * 0.55);
      cairo_line_to(cr, wx + w, wy + h * 0.55);
      setColor(cr, body);
      cairo_set_line_width(cr, 1.6 * s);
      cairo_stroke(cr);
    }
  };
  static const bool kLit[16] = {false, true, false, false, true, false, false, true,
                                true, false, false, false, false, true, false, false};
  for (int row = 0; row < 2; ++row)
    for (int i = 0; i < 8; ++i)
      window(-136 + i * 36, 112 - row * 50, kLit[row * 8 + i]);
  window(-185, 210, true);
  window(-185, 150, false);
  window(155, 160, true);
  // The door under the gable, a light over it.
  cairo_move_to(cr, cx - 16 * s, base - 30 * s);
  cairo_line_to(cr, cx - 16 * s, base - 70 * s);
  cairo_arc(cr, cx, base - 70 * s, 16 * s, kPi, 2 * kPi);
  cairo_line_to(cr, cx + 16 * s, base - 30 * s);
  cairo_close_path(cr);
  setColor(cr, darken(body, 0.6f));
  cairo_fill(cr);
  radialGlow(cr, cx, base - 96 * s, 22 * s, lit, 0.5);
}

void paintCapeCollar(cairo_t* cr, double cx, double neckY, double w, double h)
{
  for (const int side : {-1, 1})
  {
    cairo_move_to(cr, cx, neckY);
    cairo_curve_to(cr, cx + side * w * 0.25, neckY - h * 0.2, cx + side * w * 0.45, neckY - h * 0.6, cx + side * w * 0.5,
      neckY - h);
    cairo_curve_to(cr, cx + side * w * 0.4, neckY - h * 0.7, cx + side * w * 0.28, neckY - h * 0.55, cx + side * w * 0.12,
      neckY - h * 0.5);
    cairo_close_path(cr);
    fillLinear(cr, cx, neckY - h, cx + side * w * 0.5, neckY, rgb(200, 30, 46), rgb(90, 8, 20), 1.8);
    // The black outside showing along the collar's rim.
    cairo_move_to(cr, cx + side * w * 0.5, neckY - h);
    cairo_curve_to(cr, cx + side * w * 0.45, neckY - h * 0.6, cx + side * w * 0.25, neckY - h * 0.2, cx, neckY);
    setColor(cr, rgb(20, 16, 26));
    cairo_set_line_width(cr, std::max(2.0, w * 0.04));
    cairo_stroke(cr);
  }
}

void paintVampireCape(cairo_t* cr, double cx, double neckY, double w, double h, double swirl, double open)
{
  const double ph = swirl * 2 * kPi;
  const double half = w * 0.5 * (0.55 + 0.45 * open);
  const double hem = neckY + h;
  // The hem: a wave that runs along it (the swirl), scalloped like wings
  // when it is spread.
  auto hemY = [&](double u) { return hem + std::sin(u * 3 * kPi + ph) * h * 0.05 - open * h * 0.12 * std::abs(std::sin(u * 2 * kPi)); };
  // The red lining (the inside), seen past the black where it flares.
  cairo_move_to(cr, cx - w * 0.08, neckY);
  cairo_curve_to(cr, cx - half * 0.7, neckY + h * 0.2, cx - half * 1.05, neckY + h * 0.5, cx - half, hemY(0.0));
  for (int i = 1; i <= 16; ++i)
  {
    const double u = double(i) / 16.0;
    cairo_line_to(cr, cx - half + u * 2 * half, hemY(u));
  }
  cairo_curve_to(cr, cx + half * 1.05, neckY + h * 0.5, cx + half * 0.7, neckY + h * 0.2, cx + w * 0.08, neckY);
  cairo_close_path(cr);
  fillLinear(cr, 0, neckY, 0, hem, rgb(220, 40, 56), rgb(110, 10, 24), 2.2);
  // The black outside, folded over the edges and swirling.
  for (const int side : {-1, 1})
  {
    const double edge = cx + side * half;
    const double fold = side * (0.12 + 0.06 * std::sin(ph + side)) * half;
    cairo_move_to(cr, cx + side * w * 0.06, neckY);
    cairo_curve_to(cr, cx + side * half * 0.75, neckY + h * 0.2, edge + side * half * 0.05, neckY + h * 0.5, edge,
      hemY(side < 0 ? 0.0 : 1.0));
    cairo_line_to(cr, edge - fold, hemY(side < 0 ? 0.12 : 0.88) - h * 0.02);
    cairo_curve_to(cr, edge - fold * 1.4, neckY + h * 0.55, cx + side * half * 0.5, neckY + h * 0.25,
      cx + side * w * 0.02, neckY + h * 0.06);
    cairo_close_path(cr);
    fillLinear(cr, cx, 0, edge, 0, rgb(46, 40, 58), rgb(10, 8, 14), 2.0);
  }
  // A sheen on the lining's folds.
  for (int k = -2; k <= 2; ++k)
  {
    const double fx = cx + k * half * 0.32;
    cairo_move_to(cr, cx + k * w * 0.02, neckY + h * 0.08);
    cairo_curve_to(cr, fx, neckY + h * 0.4, fx + std::sin(ph + k) * w * 0.03, neckY + h * 0.7, fx, hemY(0.5 + k * 0.16) - 4);
  }
  setRgba(cr, rgb(255, 120, 130), 0.35);
  cairo_set_line_width(cr, std::max(1.0, w * 0.012));
  cairo_stroke(cr);
}

void paintBolt(cairo_t* cr, double len)
{
  const double t = std::max(2.0, len * 0.055);
  // The shaft.
  cairo_rectangle(cr, t * 1.5, -t * 0.4, len - t * 4, t * 0.8);
  fillLinear(cr, 0, -t * 0.4, 0, t * 0.4, rgb(250, 250, 255), rgb(130, 136, 156), 1.0);
  // The barbed silver head at the tip.
  cairo_move_to(cr, 0, 0);
  cairo_line_to(cr, t * 3.2, -t * 1.3);
  cairo_line_to(cr, t * 2.4, 0);
  cairo_line_to(cr, t * 3.2, t * 1.3);
  cairo_close_path(cr);
  fillLinear(cr, 0, -t, 0, t, rgb(255, 255, 255), rgb(150, 156, 176), 1.0);
  // Fletching.
  for (const int side : {-1, 1})
  {
    cairo_move_to(cr, len - t * 4.5, 0);
    cairo_line_to(cr, len - t * 0.5, side * t * 1.8);
    cairo_line_to(cr, len, side * t * 1.8);
    cairo_line_to(cr, len - t * 2.2, 0);
    cairo_close_path(cr);
    fillLinear(cr, len - t * 4.5, 0, len, 0, rgb(250, 250, 250), rgb(190, 196, 210), 0.8);
  }
}

bool drawManorArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame)
{
  (void)t;
  if (key == "portrait_ghost")
    portraitGhost(cr, w, h, variant, frame);
  else if (key == "haunted_armor")
    hauntedArmor(cr, w, h, variant, frame);
  else if (key == "poltergeist")
    poltergeist(cr, w, h, variant, frame);
  else if (key == "manor_thrown")
    thrown(cr, w, h, variant, frame);
  else if (key == "duck_cape")
  {
    // Over the item icon's duck: its head at about (38, 24), body (30, 42).
    const double s = std::min(w, h) / 64.0;
    auto X = [&](double x) { return kM + x * s; };
    auto Y = [&](double y) { return kM + y * s; };
    if (variant == 0)
    {
      paintCapeCollar(cr, X(36), Y(30), 40 * s, 22 * s);
      paintVampireCape(cr, X(26), Y(30), 52 * s, 30 * s, 0.25, 0.4);
    }
    else
    {
      // The clasp at the throat and two little fangs under the bill.
      cairo_move_to(cr, X(28), Y(34));
      cairo_line_to(cr, X(44), Y(34));
      setColor(cr, rgb(230, 190, 80));
      cairo_set_line_width(cr, 1.6 * s);
      cairo_stroke(cr);
      circle(cr, X(36), Y(34), 2.6 * s);
      fillRadial(cr, X(36), Y(34), 2.6 * s, rgb(255, 230, 140), rgb(170, 110, 30), 1.0);
      for (const double fx : {50.0, 54.0})
      {
        cairo_move_to(cr, X(fx - 1.4), Y(29));
        cairo_line_to(cr, X(fx), Y(33));
        cairo_line_to(cr, X(fx + 1.4), Y(29));
        cairo_close_path(cr);
        setColor(cr, rgb(255, 255, 255));
        cairo_fill_preserve(cr);
        setColor(cr, kInk);
        cairo_set_line_width(cr, 0.8);
        cairo_stroke(cr);
      }
    }
  }
  else
    return false;
  return true;
}

} // namespace gr
