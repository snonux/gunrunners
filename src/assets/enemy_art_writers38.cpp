// Writers' Room (Level 38) in the style of enemy_art.cpp: smooth shapes with
// a dark ink line, lit warm from the desk lamps (top left). The Typo is a
// blot of ink crawling on spidery legs, the Deadline a twin-bell alarm
// clock on long legs, the Writer's Block a sullen block of concrete, the
// sketch guard a sentry drawn in pencil with a pencil for a rifle. The duck
// sits in a coffee mug in reading glasses. Each is drawn in screen pixels
// into a texture with a 32 px margin, the cell box at (32, 32) .. (32 + w,
// 32 + h), facing right.

#include "assets/enemy_art_writers38.hpp"

#include "base/math.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <initializer_list>

namespace gr
{

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr double kM = 32.0;
constexpr Color kInk = rgb(22, 20, 30);
constexpr Color kBlot = rgb(26, 24, 40);
constexpr Color kBlotHi = rgb(90, 96, 140);
constexpr Color kSick = rgb(34, 58, 26);
constexpr Color kSickHi = rgb(140, 214, 90);
constexpr Color kCase = rgb(200, 40, 40);
constexpr Color kCaseHi = rgb(255, 120, 110);
constexpr Color kCaseLo = rgb(110, 14, 18);
constexpr Color kDial = rgb(250, 244, 226);
constexpr Color kBrass = rgb(220, 178, 80);
constexpr Color kBrassHi = rgb(255, 236, 160);
constexpr Color kBrassLo = rgb(130, 90, 30);
constexpr Color kStone = rgb(140, 138, 140);
constexpr Color kStoneHi = rgb(186, 184, 184);
constexpr Color kStoneLo = rgb(84, 82, 88);
constexpr Color kPaper = rgb(248, 244, 232);
constexpr Color kGraphite = rgb(56, 56, 66);
constexpr Color kPencil = rgb(244, 186, 36);
constexpr Color kPencilHi = rgb(255, 228, 120);
constexpr Color kPencilLo = rgb(190, 124, 16);

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

// A radial fill, lit from (lx, ly).
void rgrad(cairo_t* cr, double lx, double ly, double r0, double cx, double cy, double r1, Color hi, Color mid, Color lo,
  bool preserve = false)
{
  cairo_pattern_t* g = cairo_pattern_create_radial(lx, ly, r0, cx, cy, r1);
  stop(g, 0.0, hi);
  stop(g, 0.55, mid);
  stop(g, 1.0, lo);
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

// A cartoon eye looking ahead (to the right): white, a pupil, a glint.
void eye(cairo_t* cr, double x, double y, double rx, double ry, double look, double lid = 0.0)
{
  ellipse(cr, x, y, rx, ry);
  setRgba(cr, rgb(255, 255, 255), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.4);
  circle(cr, x + look * rx * 0.4, y + ry * 0.1, std::min(rx, ry) * 0.5);
  setRgba(cr, kInk, 1.0);
  cairo_fill(cr);
  circle(cr, x + look * rx * 0.4 - rx * 0.18, y - ry * 0.15, std::min(rx, ry) * 0.16);
  setRgba(cr, rgb(255, 255, 255), 1.0);
  cairo_fill(cr);
  if (lid > 0.0)
  {
    // A heavy lid over the top `lid` of it.
    cairo_save(cr);
    ellipse(cr, x, y, rx + 0.5, ry + 0.5);
    cairo_clip(cr);
    cairo_rectangle(cr, x - rx - 1, y - ry - 1, rx * 2 + 2, ry * 2 * lid + 1);
    setRgba(cr, kStoneLo, 1.0);
    cairo_fill(cr);
    cairo_restore(cr);
    cairo_move_to(cr, x - rx, y - ry + ry * 2 * lid);
    cairo_line_to(cr, x + rx, y - ry + ry * 2 * lid);
    stroke(cr, kInk, 1.6);
  }
}

// --- Typo ------------------------------------------------------------------------------------

// The blot's outline: a wobbly round splat, its bumps from `seed`.
void blotPath(cairo_t* cr, double cx, double cy, double rx, double ry, int seed)
{
  const int n = 14;
  for (int k = 0; k <= n; ++k)
  {
    const double a = k * 2 * kPi / n;
    const double bump = 1.0 + 0.09 * std::sin(a * 3.0 + seed) + 0.06 * std::cos(a * 5.0 + seed * 2);
    const double x = cx + std::cos(a) * rx * bump, y = cy + std::sin(a) * ry * bump;
    if (k == 0)
      cairo_move_to(cr, x, y);
    else
    {
      const double a0 = (k - 0.5) * 2 * kPi / n;
      cairo_curve_to(cr, cx + std::cos(a0) * rx * 1.12, cy + std::sin(a0) * ry * 1.12, cx + std::cos(a0) * rx * 1.12,
        cy + std::sin(a0) * ry * 1.12, x, y);
    }
  }
  cairo_close_path(cr);
}

void typo(cairo_t* cr, double w, double h, int variant)
{
  const bool carrier = variant >= 3;
  const int pose = variant % 3;
  const bool tell = pose == 2;
  cairo_save(cr);
  frameBox(cr, w, h, 64, 64);
  const Color body = carrier ? kSick : kBlot, hi = carrier ? kSickHi : kBlotHi;
  const double cy = tell ? 26 : 34, ry = tell ? 21 : 18;
  // The legs: three a side, spidery ink, bent at the knee.
  for (int side = -1; side <= 1; side += 2)
    for (int k = 0; k < 3; ++k)
    {
      const double sx = 32 + side * (10 + k * 5), sy = cy + 8;
      const double step = (pose == 0 ? (k % 2 ? 4 : -4) : (k % 2 ? -4 : 4)) * (tell ? 0 : 1);
      const double fx = 32 + side * (20 + k * 6) + step, fy = 63;
      const double kx = (sx + fx) * 0.5 + side * 6, ky = tell ? cy + 22 : cy + 6;
      cairo_move_to(cr, sx, sy);
      cairo_line_to(cr, kx, ky);
      cairo_line_to(cr, fx, fy);
      stroke(cr, body, 2.6);
      circle(cr, fx, fy - 0.5, 1.6);
      setRgba(cr, body, 1.0);
      cairo_fill(cr);
    }
  // The blot and its drips.
  blotPath(cr, 32, cy, 23, ry, carrier ? 3 : 1);
  rgrad(cr, 24, cy - 10, 1, 32, cy, 28, lighten(hi, 0.1f), body, darken(body, 0.5f), true);
  stroke(cr, kInk, 1.6);
  for (int k = 0; k < 3; ++k)
  {
    const double dx = 18 + k * 13, len = 5 + (k * 7 % 5) + (tell ? 4 : 0);
    cairo_move_to(cr, dx - 2.5, cy + ry * 0.8);
    cairo_line_to(cr, dx - 1.5, cy + ry * 0.8 + len);
    cairo_arc(cr, dx, cy + ry * 0.8 + len, 2.5, kPi, 2 * kPi);
    cairo_line_to(cr, dx + 2.5, cy + ry * 0.8);
    cairo_close_path(cr);
    setRgba(cr, body, 1.0);
    cairo_fill(cr);
  }
  // A wet sheen.
  ellipse(cr, 22, cy - 9, 7, 3.5, -0.4);
  setRgba(cr, rgb(255, 255, 255), 0.35);
  cairo_fill(cr);
  // Splashes as it springs.
  if (tell)
    for (int k = 0; k < 6; ++k)
    {
      const double a = kPi * (0.15 + k * 0.14);
      circle(cr, 32 + std::cos(a) * 30, 60 - std::sin(a) * 6, 1.8 + (k % 2));
      setRgba(cr, body, 1.0);
      cairo_fill(cr);
    }
  // The eyes, goggling over the top of its letter.
  eye(cr, 36, cy - ry + 3, 6, tell ? 8 : 7, 1.0);
  eye(cr, 48, cy - ry + 5, 5, tell ? 7 : 6, 1.0);
  if (carrier)
  {
    // A fever: a thermometer in its mouth corner, sweat.
    cairo_move_to(cr, 52, cy + 4);
    cairo_line_to(cr, 64, cy - 6);
    stroke(cr, kInk, 4.0);
    cairo_move_to(cr, 52, cy + 4);
    cairo_line_to(cr, 64, cy - 6);
    stroke(cr, rgb(240, 244, 250), 2.4);
    circle(cr, 52, cy + 4, 2.4);
    setRgba(cr, rgb(230, 40, 40), 1.0);
    cairo_fill(cr);
    for (const double sx : {14.0, 20.0})
    {
      cairo_move_to(cr, sx, cy - ry + 2);
      cairo_curve_to(cr, sx - 2.5, cy - ry + 6, sx - 2, cy - ry + 9, sx, cy - ry + 9);
      cairo_curve_to(cr, sx + 2, cy - ry + 9, sx + 2.5, cy - ry + 6, sx, cy - ry + 2);
      setRgba(cr, rgb(170, 230, 255), 0.9);
      cairo_fill(cr);
    }
  }
  cairo_restore(cr);
}

// --- Deadline -------------------------------------------------------------------------------------

void deadline(cairo_t* cr, double w, double h, int variant, int frame)
{
  const int look = std::clamp(variant % 5, 0, 4);
  const bool ring = variant >= 5;
  cairo_save(cr);
  frameBox(cr, w, h, 128, 160);
  const double cx = 64, cy = 70, R = 46;
  // The legs, long and thin, in sneakers; the stride.
  const double s = frame % 2 ? 1.0 : -1.0;
  for (int k = 0; k < 2; ++k)
  {
    const double hx = cx + (k ? 16 : -16), fx = hx + s * (k ? 12 : -12), fy = 150;
    limb(cr, {{hx, cy + R - 6}, {(hx + fx) * 0.5 - s * 4, 132}, {fx, fy}}, 5.0, rgb(60, 56, 70));
    // The sneaker.
    cairo_move_to(cr, fx - 10, fy + 8);
    cairo_curve_to(cr, fx - 12, fy - 4, fx + 4, fy - 6, fx + 10, fy + 2);
    cairo_curve_to(cr, fx + 18, fy + 4, fx + 18, fy + 9, fx + 12, fy + 9);
    cairo_close_path(cr);
    vgrad(cr, fy - 6, fy + 9, {{0.0, rgb(250, 250, 250)}, {1.0, rgb(190, 190, 200)}}, 1.0, true);
    stroke(cr, kInk, 1.6);
    cairo_move_to(cr, fx - 10, fy + 7);
    cairo_line_to(cr, fx + 14, fy + 7);
    stroke(cr, kCase, 2.2);
  }
  // The bells, the hammer between them on its post, the carry handle.
  const double shake = ring ? 3.0 : 0.0;
  for (int k = 0; k < 2; ++k)
  {
    const double bx = k ? cx + 34 : cx - 34, by = cy - R + 2, tilt = k ? 0.5 : -0.5;
    cairo_save(cr);
    cairo_translate(cr, bx + (k ? shake : -shake), by);
    cairo_rotate(cr, tilt);
    cairo_move_to(cr, -18, 6);
    cairo_curve_to(cr, -18, -16, 18, -16, 18, 6);
    cairo_close_path(cr);
    rgrad(cr, -6, -10, 1, 0, -2, 22, kBrassHi, kBrass, kBrassLo, true);
    stroke(cr, kInk, 1.8);
    cairo_move_to(cr, 0, -10);
    cairo_line_to(cr, 0, -15);
    stroke(cr, kInk, 3.0);
    cairo_restore(cr);
  }
  cairo_move_to(cr, cx - 30, cy - R - 14);
  cairo_curve_to(cr, cx - 20, cy - R - 34, cx + 20, cy - R - 34, cx + 30, cy - R - 14);
  stroke(cr, kInk, 6.0);
  cairo_move_to(cr, cx - 30, cy - R - 14);
  cairo_curve_to(cr, cx - 20, cy - R - 34, cx + 20, cy - R - 34, cx + 30, cy - R - 14);
  stroke(cr, kBrass, 3.2);
  if (ring)
  {
    // The hammer a blur between the bells, ring marks off them.
    for (int k = -1; k <= 1; k += 2)
    {
      cairo_move_to(cr, cx, cy - R + 2);
      cairo_line_to(cr, cx + k * 14, cy - R - 18);
      stroke(cr, kInk, 2.2, 0.55);
      circle(cr, cx + k * 14, cy - R - 18, 4);
      setRgba(cr, kBrassLo, 0.6);
      cairo_fill(cr);
    }
    for (int k = 0; k < 2; ++k)
      for (int a = 0; a < 3; ++a)
      {
        const double bx = k ? cx + 40 : cx - 40, by = cy - R - 6, rr = 22 + a * 7;
        cairo_arc(cr, bx, by, rr, k ? -kPi * 0.45 : -kPi * 0.95, k ? -kPi * 0.05 : -kPi * 0.55);
        stroke(cr, rgb(255, 240, 160), 2.2, 0.9 - a * 0.25);
      }
  }
  else
  {
    cairo_move_to(cr, cx, cy - R + 2);
    cairo_line_to(cr, cx, cy - R - 16);
    stroke(cr, kInk, 2.2);
    circle(cr, cx, cy - R - 16, 4);
    setRgba(cr, kBrassLo, 1.0);
    cairo_fill(cr);
  }
  // The case: a fat red drum, redder the later it gets.
  const Color body = look >= 3 ? lerpColor(kCase, rgb(255, 60, 30), 0.4f) : kCase;
  circle(cr, cx, cy, R + 8);
  rgrad(cr, cx - 20, cy - 24, 2, cx, cy, R + 10, kCaseHi, body, kCaseLo, true);
  stroke(cr, kInk, 2.2);
  circle(cr, cx, cy, R + 1);
  vgrad(cr, cy - R, cy + R, {{0.0, kBrassHi}, {1.0, kBrassLo}});
  // The dial; frantic, it flushes orange.
  const Color dial = look == 4 ? rgb(255, 200, 150) : look == 3 ? rgb(255, 222, 196) : kDial;
  circle(cr, cx, cy, R - 3);
  rgrad(cr, cx - 14, cy - 16, 2, cx, cy, R, rgb(255, 255, 250), dial, darken(dial, 0.12f), true);
  stroke(cr, kInk, 1.2, 0.8);
  for (int k = 0; k < 12; ++k)
  {
    const double a = k * kPi / 6;
    const double r0 = k % 3 == 0 ? R - 13 : R - 9;
    cairo_move_to(cr, cx + std::cos(a) * r0, cy + std::sin(a) * r0);
    cairo_line_to(cr, cx + std::cos(a) * (R - 5), cy + std::sin(a) * (R - 5));
    stroke(cr, kInk, k % 3 == 0 ? 2.6 : 1.4, 0.85);
  }
  // Its face on the dial.
  const double ex = 14 + look * 0.6, ey = cy - 14;
  const double er = 6.5 + look * 0.6;
  eye(cr, cx - ex, ey, er, er + 1.5, 1.0);
  eye(cr, cx + ex, ey, er, er + 1.5, 1.0);
  // Brows climbing with the worry.
  for (int k = -1; k <= 1; k += 2)
  {
    cairo_move_to(cr, cx + k * ex - 7, ey - er - 4 - (look >= 2 ? (k < 0 ? 3 : 0) : 0));
    cairo_line_to(cr, cx + k * ex + 7, ey - er - 4 - (look >= 2 ? (k > 0 ? 3 : 0) : 0));
    stroke(cr, kInk, 2.4);
  }
  const double my = cy + 22;
  if (look == 0)
  {
    cairo_move_to(cr, cx - 10, my);
    cairo_curve_to(cr, cx - 4, my + 5, cx + 4, my + 5, cx + 10, my);
    stroke(cr, kInk, 2.2);
  }
  else if (look <= 2)
  {
    cairo_move_to(cr, cx - 10, my + 2);
    cairo_curve_to(cr, cx - 5, my - 2, cx, my + 4, cx + 4, my);
    cairo_curve_to(cr, cx + 7, my - 2, cx + 9, my + 2, cx + 11, my + 1);
    stroke(cr, kInk, 2.2);
  }
  else
  {
    ellipse(cr, cx, my + 2, look == 4 ? 10 : 8, look == 4 ? 8 : 4);
    setRgba(cr, rgb(90, 20, 30), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.8);
    cairo_rectangle(cr, cx - 6, my - 2, 12, 2.5);
    setRgba(cr, rgb(255, 255, 255), 0.9);
    cairo_fill(cr);
  }
  if (look >= 1)
    for (int k = -1; k <= 1; k += 2)
    {
      ellipse(cr, cx + k * (ex + 6), cy + 8, 6, 3.5);
      setRgba(cr, rgb(255, 90, 100), 0.35 + 0.1 * look);
      cairo_fill(cr);
    }
  // Sweat, then steam.
  if (look >= 2)
    for (int k = 0; k < look - 1; ++k)
    {
      const double sx = k % 2 ? cx + R + 4 + k * 3 : cx - R - 4 - k * 3, sy = cy - 20 + k * 12;
      cairo_move_to(cr, sx, sy);
      cairo_curve_to(cr, sx - 4, sy + 6, sx - 3.5, sy + 11, sx, sy + 11);
      cairo_curve_to(cr, sx + 3.5, sy + 11, sx + 4, sy + 6, sx, sy);
      setRgba(cr, rgb(150, 220, 255), 0.95);
      cairo_fill_preserve(cr);
      stroke(cr, kInk, 1.0, 0.7);
    }
  if (look >= 3)
    for (int k = 0; k < 3; ++k)
    {
      const double px = cx - 24 + k * 24, py = cy - R - 30 - (k % 2) * 6;
      for (int p = 0; p < 3; ++p)
      {
        circle(cr, px + std::sin(p * 1.7 + k) * 4, py - p * 8, 6 - p);
        setRgba(cr, rgb(255, 255, 255), 0.5 - p * 0.12);
        cairo_fill(cr);
      }
    }
  // The glass's glint.
  cairo_arc(cr, cx, cy, R - 8, kPi * 1.1, kPi * 1.4);
  stroke(cr, rgb(255, 255, 255), 3.0, 0.5);
  cairo_restore(cr);
}

// --- Writer's Block ---------------------------------------------------------------------------------

void writersBlock(cairo_t* cr, double w, double h, int variant)
{
  cairo_save(cr);
  frameBox(cr, w, h, 128, 128);
  const double lean = variant == 2 ? 4 : 0;
  // Its shadow.
  ellipse(cr, 64, 126, 64, 5);
  setRgba(cr, kInk, 0.3);
  cairo_fill(cr);
  // The block: a little chipped at the corners, lit top left.
  cairo_move_to(cr, 6 + lean, 6);
  cairo_line_to(cr, 108 + lean, 3);
  cairo_line_to(cr, 116 + lean, 10);
  cairo_line_to(cr, 124, 122);
  cairo_line_to(cr, 118, 127);
  cairo_line_to(cr, 8, 127);
  cairo_line_to(cr, 3, 120);
  cairo_line_to(cr, 2 + lean, 14);
  cairo_close_path(cr);
  rgrad(cr, 30, 20, 4, 60, 60, 120, kStoneHi, kStone, kStoneLo, true);
  stroke(cr, kInk, 2.4);
  // The top face's lit lip.
  cairo_move_to(cr, 6 + lean, 7);
  cairo_line_to(cr, 108 + lean, 4);
  stroke(cr, lighten(kStoneHi, 0.4f), 2.0, 0.8);
  // Aggregate: pits and pebbles.
  Rng rng(38201u);
  for (int i = 0; i < 70; ++i)
  {
    circle(cr, rng.range(8, 120), rng.range(10, 122), rng.range(0.6f, 1.8f));
    setRgba(cr, i % 3 ? kStoneLo : kStoneHi, rng.range(0.3f, 0.7f));
    cairo_fill(cr);
  }
  // Cracks.
  cairo_move_to(cr, 116, 30);
  cairo_line_to(cr, 102, 40);
  cairo_line_to(cr, 106, 52);
  cairo_line_to(cr, 94, 60);
  cairo_move_to(cr, 14, 98);
  cairo_line_to(cr, 26, 104);
  cairo_line_to(cr, 22, 116);
  stroke(cr, kInk, 1.4, 0.75);
  // A cobweb in the top-left corner.
  for (int k = 0; k < 4; ++k)
  {
    cairo_move_to(cr, 4 + lean, 8);
    cairo_line_to(cr, 4 + lean + std::cos(k * 0.45) * 26, 8 + std::sin(k * 0.45) * 26);
  }
  for (int k = 1; k < 4; ++k)
  {
    cairo_arc(cr, 4 + lean, 8, k * 7.0, 0, kPi * 0.6);
  }
  stroke(cr, rgb(240, 240, 250), 0.7, 0.55);
  // The face, low on it.
  const double ey = 82;
  if (variant == 0)
  {
    eye(cr, 46, ey, 9, 7, 0.6, 0.6);
    eye(cr, 82, ey, 9, 7, 0.6, 0.6);
    cairo_move_to(cr, 52, 106);
    cairo_line_to(cr, 76, 106);
    stroke(cr, kInk, 2.6);
  }
  else if (variant == 1)
  {
    eye(cr, 46, ey, 10, 9, 1.0, 0.25);
    eye(cr, 82, ey, 10, 9, 1.0, 0.25);
    for (int k = -1; k <= 1; k += 2)
    {
      cairo_move_to(cr, 64 + k * 30, ey - 14);
      cairo_line_to(cr, 64 + k * 8, ey - 8);
      stroke(cr, kInk, 3.4);
    }
    roundRect(cr, 48, 100, 32, 12, 3);
    setRgba(cr, rgb(250, 250, 240), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 2.0);
    for (int k = 1; k < 4; ++k)
    {
      cairo_move_to(cr, 48 + k * 8, 100);
      cairo_line_to(cr, 48 + k * 8, 112);
    }
    cairo_move_to(cr, 48, 106);
    cairo_line_to(cr, 80, 106);
    stroke(cr, kInk, 1.2);
  }
  else
  {
    for (int k = -1; k <= 1; k += 2)
    {
      cairo_move_to(cr, 64 + k * 26, ey - 4);
      cairo_line_to(cr, 64 + k * 14, ey);
      cairo_line_to(cr, 64 + k * 26, ey + 4);
      stroke(cr, kInk, 3.0);
    }
    cairo_move_to(cr, 50, 108);
    cairo_curve_to(cr, 58, 102, 70, 102, 78, 108);
    stroke(cr, kInk, 2.8);
  }
  cairo_restore(cr);
}

// --- The sketch guard ---------------------------------------------------------------------------------

// A path stroked twice in graphite, the second pass a little off, as a
// quick pencil sketch is.
void sketch(cairo_t* cr, const std::function<void()>& path, double width, double a = 1.0)
{
  path();
  stroke(cr, kGraphite, width, a);
  cairo_save(cr);
  cairo_translate(cr, 0.9, -0.7);
  path();
  stroke(cr, kGraphite, width * 0.5, a * 0.55);
  cairo_restore(cr);
}

// Hatching inside the current clip.
void hatchArea(cairo_t* cr, double x, double y, double w, double h, double gap, double a)
{
  for (double k = -h; k < w; k += gap)
  {
    cairo_move_to(cr, x + k, y + h);
    cairo_line_to(cr, x + k + h, y);
  }
  stroke(cr, kGraphite, 0.8, a);
}

// The rifle: a long yellow pencil from (x0, y0) toward (x1, y1), its point
// the muzzle, its eraser the butt.
void pencilRifle(cairo_t* cr, double x0, double y0, double x1, double y1)
{
  const double dx = x1 - x0, dy = y1 - y0, len = std::sqrt(dx * dx + dy * dy);
  cairo_save(cr);
  cairo_translate(cr, x0, y0);
  cairo_rotate(cr, std::atan2(dy, dx));
  cairo_rectangle(cr, 0, -4, 8, 8);
  setRgba(cr, rgb(240, 140, 150), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.2);
  cairo_rectangle(cr, 8, -4, 6, 8);
  hgrad(cr, 8, 14, {{0.0, rgb(230, 230, 236)}, {1.0, rgb(140, 140, 150)}}, 1.0, true);
  stroke(cr, kInk, 1.2);
  cairo_rectangle(cr, 14, -4, len - 28, 8);
  vgrad(cr, -4, 4, {{0.0, kPencilHi}, {0.5, kPencil}, {1.0, kPencilLo}}, 1.0, true);
  stroke(cr, kInk, 1.2);
  cairo_move_to(cr, len - 14, -4);
  cairo_line_to(cr, len - 2, -0.8);
  cairo_line_to(cr, len - 2, 0.8);
  cairo_line_to(cr, len - 14, 4);
  cairo_close_path(cr);
  setRgba(cr, rgb(230, 190, 140), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.1);
  cairo_move_to(cr, len - 5, -1.6);
  cairo_line_to(cr, len, 0);
  cairo_line_to(cr, len - 5, 1.6);
  cairo_close_path(cr);
  setRgba(cr, kGraphite, 1.0);
  cairo_fill(cr);
  cairo_restore(cr);
}

void sketchGuard(cairo_t* cr, double w, double h, int variant)
{
  cairo_save(cr);
  frameBox(cr, w, h, 96, 160);
  // Its shadow, smudged.
  ellipse(cr, 48, 157, 30, 4);
  setRgba(cr, kGraphite, 0.3);
  cairo_fill(cr);
  // The legs and boots.
  for (int k = 0; k < 2; ++k)
  {
    const double x = k ? 56 : 34;
    auto leg = [&]() {
      cairo_rectangle(cr, x - 7, 108, 14, 38);
    };
    leg();
    setRgba(cr, kPaper, 1.0);
    cairo_fill(cr);
    sketch(cr, leg, 1.8);
    auto boot = [&]() {
      cairo_move_to(cr, x - 9, 158);
      cairo_line_to(cr, x - 8, 144);
      cairo_line_to(cr, x + 8, 144);
      cairo_line_to(cr, x + 16, 152);
      cairo_line_to(cr, x + 16, 158);
      cairo_close_path(cr);
    };
    boot();
    setRgba(cr, rgb(90, 90, 100), 1.0);
    cairo_fill(cr);
    sketch(cr, boot, 1.8);
  }
  // The tunic, belted, hatched down its shaded side.
  auto tunic = [&]() {
    cairo_move_to(cr, 24, 46);
    cairo_curve_to(cr, 34, 38, 62, 38, 72, 46);
    cairo_line_to(cr, 76, 112);
    cairo_line_to(cr, 20, 112);
    cairo_close_path(cr);
  };
  tunic();
  setRgba(cr, kPaper, 1.0);
  cairo_fill(cr);
  cairo_save(cr);
  tunic();
  cairo_clip(cr);
  hatchArea(cr, 56, 40, 24, 72, 4.0, 0.45);
  cairo_restore(cr);
  sketch(cr, tunic, 2.0);
  auto belt = [&]() {
    cairo_rectangle(cr, 20, 98, 56, 8);
  };
  belt();
  setRgba(cr, rgb(110, 110, 120), 1.0);
  cairo_fill(cr);
  sketch(cr, belt, 1.4);
  // The head under its helmet, a big mustache.
  auto head = [&]() {
    cairo_arc(cr, 48, 28, 13, 0, 2 * kPi);
  };
  head();
  setRgba(cr, kPaper, 1.0);
  cairo_fill(cr);
  sketch(cr, head, 1.8);
  circle(cr, 53, 26, 1.8);
  setRgba(cr, kGraphite, 1.0);
  cairo_fill(cr);
  auto stache = [&]() {
    cairo_move_to(cr, 44, 33);
    cairo_curve_to(cr, 48, 30, 56, 30, 64, 34);
    cairo_curve_to(cr, 58, 37, 50, 37, 44, 33);
  };
  stache();
  setRgba(cr, kGraphite, 1.0);
  cairo_fill(cr);
  auto helmet = [&]() {
    cairo_move_to(cr, 32, 24);
    cairo_curve_to(cr, 32, 6, 64, 6, 64, 24);
    cairo_close_path(cr);
    cairo_move_to(cr, 28, 24);
    cairo_line_to(cr, 70, 24);
  };
  helmet();
  setRgba(cr, rgb(150, 150, 160), 1.0);
  cairo_fill(cr);
  cairo_save(cr);
  cairo_move_to(cr, 32, 24);
  cairo_curve_to(cr, 32, 6, 64, 6, 64, 24);
  cairo_close_path(cr);
  cairo_clip(cr);
  hatchArea(cr, 30, 6, 36, 18, 3.0, 0.5);
  cairo_restore(cr);
  sketch(cr, helmet, 2.0);
  // The arms and the pencil rifle.
  double hx = 0, hy = 0;
  if (variant == 0)
  {
    // At ease: sloped on the shoulder.
    pencilRifle(cr, 52, 112, 66, 22);
    hx = 60;
    hy = 70;
  }
  else if (variant == 1)
  {
    // Coming up.
    pencilRifle(cr, 30, 104, 104, 70);
    hx = 62;
    hy = 90;
  }
  else
  {
    // Aimed from the hip.
    pencilRifle(cr, 22, 96, 110, 96);
    hx = 66;
    hy = 96;
    glow(cr, 112, 96, 10, rgb(255, 240, 200), 0.8);
  }
  auto arm = [&]() {
    cairo_move_to(cr, 30, 50);
    cairo_curve_to(cr, 26, 74, 40, 92, hx - 18, hy);
    cairo_move_to(cr, 66, 50);
    cairo_curve_to(cr, 74, 64, 72, 80, hx, hy);
  };
  sketch(cr, arm, 3.2);
  for (const double ax : {hx - 18, hx})
  {
    circle(cr, ax, hy, 4.2);
    setRgba(cr, kPaper, 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kGraphite, 1.4);
  }
  cairo_restore(cr);
}

// --- The duck in its mug ---------------------------------------------------------------------------

void duckGlassesPencil(cairo_t* cr, double w, double h, int variant)
{
  cairo_save(cr);
  cairo_translate(cr, kM, kM);
  cairo_scale(cr, w / 64.0, h / 64.0);
  const Color mug = rgb(240, 238, 230);
  if (variant == 0)
  {
    // Inside the mug: its far wall and a surface of coffee.
    ellipse(cr, 31, 42, 24, 6);
    setRgba(cr, rgb(190, 186, 176), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.4);
    ellipse(cr, 31, 43.5, 21, 4.4);
    setRgba(cr, rgb(90, 50, 24), 1.0);
    cairo_fill(cr);
    ellipse(cr, 24, 42.5, 6, 1.2);
    setRgba(cr, rgb(200, 150, 100), 0.6);
    cairo_fill(cr);
    // Steam.
    for (int k = 0; k < 2; ++k)
    {
      cairo_move_to(cr, 12 + k * 6, 40);
      cairo_curve_to(cr, 8 + k * 6, 34, 16 + k * 6, 30, 12 + k * 6, 24);
      stroke(cr, rgb(255, 255, 255), 1.6, 0.45);
    }
  }
  else
  {
    // The mug's front, its rim and handle; a slogan.
    cairo_move_to(cr, 7, 42);
    cairo_line_to(cr, 9, 62);
    cairo_curve_to(cr, 20, 65, 42, 65, 53, 62);
    cairo_line_to(cr, 55, 42);
    cairo_curve_to(cr, 44, 49, 18, 49, 7, 42);
    cairo_close_path(cr);
    hgrad(cr, 7, 55, {{0.0, rgb(180, 178, 170)}, {0.3, mug}, {0.7, mug}, {1.0, rgb(170, 168, 160)}}, 1.0, true);
    stroke(cr, kInk, 1.6);
    cairo_arc(cr, 4, 51, 6, kPi * 0.5, kPi * 1.5);
    stroke(cr, kInk, 4.4);
    cairo_arc(cr, 4, 51, 6, kPi * 0.5, kPi * 1.5);
    stroke(cr, mug, 2.6);
    cairo_rectangle(cr, 14, 51, 34, 8);
    setRgba(cr, rgb(214, 34, 44), 1.0);
    cairo_fill(cr);
    selectGameFont(cr);
    cairo_set_font_size(cr, 6.0);
    cairo_text_extents_t e;
    cairo_text_extents(cr, "DRAFT 9", &e);
    cairo_move_to(cr, 31 - e.width * 0.5 - e.x_bearing, 57.5);
    setRgba(cr, rgb(255, 255, 255), 1.0);
    cairo_show_text(cr, "DRAFT 9");
    // A pencil behind its ear, tipped back.
    cairo_save(cr);
    cairo_translate(cr, 30, 14);
    cairo_rotate(cr, -0.5);
    cairo_rectangle(cr, -12, -2, 20, 4);
    vgrad(cr, -2, 2, {{0.0, kPencilHi}, {1.0, kPencilLo}}, 1.0, true);
    stroke(cr, kInk, 0.9);
    cairo_rectangle(cr, -15, -2, 3, 4);
    setRgba(cr, rgb(240, 140, 150), 1.0);
    cairo_fill(cr);
    cairo_move_to(cr, 8, -2);
    cairo_line_to(cr, 13, 0);
    cairo_line_to(cr, 8, 2);
    cairo_close_path(cr);
    setRgba(cr, rgb(230, 190, 140), 1.0);
    cairo_fill(cr);
    cairo_restore(cr);
    // Little round reading glasses low on its beak, over the shades.
    for (const double gx : {46.0, 54.0})
    {
      circle(cr, gx, 27, 3.6);
      setRgba(cr, rgb(220, 240, 255), 0.35);
      cairo_fill_preserve(cr);
      stroke(cr, rgb(40, 30, 20), 1.2);
    }
    cairo_move_to(cr, 49.6, 27);
    cairo_line_to(cr, 50.4, 27);
    cairo_move_to(cr, 42.4, 26.5);
    cairo_line_to(cr, 36, 22);
    stroke(cr, rgb(40, 30, 20), 1.0);
  }
  cairo_restore(cr);
}

} // namespace

bool drawWriters38Art(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame)
{
  (void)t;
  if (key == "typo")
    typo(cr, w, h, std::clamp(variant, 0, 5));
  else if (key == "deadline")
    deadline(cr, w, h, std::clamp(variant, 0, 9), frame % 2);
  else if (key == "writersblock")
    writersBlock(cr, w, h, std::clamp(variant, 0, 2));
  else if (key == "sketch_guard")
    sketchGuard(cr, w, h, std::clamp(variant, 0, 2));
  else if (key == "duck_glasses_pencil")
    duckGlassesPencil(cr, w, h, variant);
  else
    return false;
  return true;
}

} // namespace gr
