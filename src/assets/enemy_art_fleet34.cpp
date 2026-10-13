// Zeppelin Relay (Level 34) in the style of enemy_art.cpp: smooth shapes with
// a dark ink line, a warm sunset rim along their upper edges and brass where
// something is a machine. The Sky Pirate is a bearded buccaneer with a brass
// jet pack on his back, a cutlass and a flintlock; the Peck Parrot a scarlet
// macaw in flying goggles, the carrier an Amazon parrot in a mail carrier's
// cap with a satchel of letters; the Balloon Mine a horned naval mine slung
// under a small patched balloon. The rubber duck wears a flying cap and
// goggles. Each is drawn in screen pixels into a texture with a 32 px
// margin, the cell box at (32, 32) .. (32 + w, 32 + h), facing right.

#include "assets/enemy_art_fleet34.hpp"

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
constexpr Color kInk = rgb(30, 16, 20);
constexpr Color kBrass = rgb(220, 178, 88);
constexpr Color kBrassHi = rgb(255, 236, 160);
constexpr Color kBrassLo = rgb(130, 90, 36);
constexpr Color kRim = rgb(255, 196, 150);
constexpr Color kSkin = rgb(236, 180, 140);
constexpr Color kLeather = rgb(120, 70, 40);
constexpr Color kLeatherLo = rgb(70, 40, 24);

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
  cairo_scale(cr, rx, ry);
  cairo_new_sub_path(cr);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_restore(cr);
}

void grad(cairo_t* cr, double x0, double y0, double x1, double y1, std::initializer_list<std::pair<double, Color>> stops,
  double a = 1.0, bool preserve = false)
{
  cairo_pattern_t* g = cairo_pattern_create_linear(x0, y0, x1, y1);
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

// Fills the current path with `c` (lit toward the top) and inks it.
void solid(cairo_t* cr, Color c, double y0, double y1, double ink = 1.6)
{
  grad(cr, 0, y0, 0, y1, {{0.0, lighten(c, 0.22f)}, {0.45, c}, {1.0, darken(c, 0.25f)}}, 1.0, true);
  stroke(cr, kInk, ink);
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

// --- Sky Pirate ------------------------------------------------------------------------------------

// A cutlass from the hilt at (x, y) along `angle` (radians, 0 = right),
// `len` long, curving a little; `bright` for the tell's flash.
void cutlass(cairo_t* cr, double x, double y, double angle, double len, bool bright)
{
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_rotate(cr, angle);
  // The blade.
  cairo_move_to(cr, 2, -2.5);
  cairo_curve_to(cr, len * 0.4, -4.5, len * 0.8, -5.5, len, -9);
  cairo_curve_to(cr, len * 0.85, -1, len * 0.45, 3.5, 2, 3.5);
  cairo_close_path(cr);
  if (bright)
    grad(cr, 0, -6, 0, 4, {{0.0, rgb(255, 255, 255)}, {0.5, rgb(240, 246, 255)}, {1.0, rgb(170, 190, 220)}}, 1.0, true);
  else
    grad(cr, 0, -6, 0, 4, {{0.0, rgb(240, 240, 250)}, {0.5, rgb(180, 186, 200)}, {1.0, rgb(100, 106, 124)}}, 1.0, true);
  stroke(cr, kInk, 1.3);
  cairo_move_to(cr, 6, -1.5);
  cairo_curve_to(cr, len * 0.4, -3, len * 0.75, -4, len * 0.95, -7.5);
  stroke(cr, rgb(255, 255, 255), 0.9, bright ? 1.0 : 0.6);
  // The basket guard and the grip.
  cairo_move_to(cr, 1, -6);
  cairo_curve_to(cr, -4, -8, -14, -6, -14, 0);
  cairo_curve_to(cr, -14, 6, -6, 8, 1, 6);
  stroke(cr, kInk, 4.0);
  cairo_move_to(cr, 1, -6);
  cairo_curve_to(cr, -4, -8, -14, -6, -14, 0);
  cairo_curve_to(cr, -14, 6, -6, 8, 1, 6);
  stroke(cr, kBrass, 2.2);
  cairo_rectangle(cr, -12, -2.5, 12, 5);
  setRgba(cr, rgb(60, 34, 24), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.0);
  cairo_restore(cr);
}

// A flintlock pistol, its grip at (x, y), the barrel along `angle`.
void flintlock(cairo_t* cr, double x, double y, double angle)
{
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_rotate(cr, angle);
  // The stock: a curved wooden grip.
  cairo_move_to(cr, -4, 8);
  cairo_curve_to(cr, -8, 2, -6, -4, 0, -5);
  cairo_line_to(cr, 8, -5);
  cairo_line_to(cr, 8, 1);
  cairo_curve_to(cr, 2, 2, 2, 6, 3, 9);
  cairo_close_path(cr);
  solid(cr, rgb(130, 72, 40), -5, 9, 1.2);
  // The barrel, brass bands.
  cairo_rectangle(cr, 6, -6, 24, 4.5);
  grad(cr, 0, -6, 0, -1.5, {{0.0, rgb(200, 204, 214)}, {1.0, rgb(80, 84, 96)}}, 1.0, true);
  stroke(cr, kInk, 1.1);
  for (const double bx : {12.0, 24.0})
  {
    cairo_rectangle(cr, bx, -6.5, 2, 5.5);
    setRgba(cr, kBrass, 1.0);
    cairo_fill(cr);
  }
  // The lock and its hammer.
  cairo_move_to(cr, 4, -6);
  cairo_line_to(cr, 2, -11);
  cairo_line_to(cr, 6, -12);
  stroke(cr, kInk, 2.6);
  cairo_move_to(cr, 4, -6);
  cairo_line_to(cr, 2, -11);
  cairo_line_to(cr, 6, -12);
  stroke(cr, rgb(150, 150, 160), 1.4);
  cairo_restore(cr);
}

void pirateLeg(cairo_t* cr, double hx, double hy, double kx, double ky, double ax, double ay, bool back)
{
  const Color pants = back ? darken(rgb(170, 40, 46), 0.25f) : rgb(170, 40, 46);
  const Color boot = back ? rgb(26, 20, 22) : rgb(46, 34, 34);
  limb(cr, {{hx, hy}, {kx, ky}}, 15, pants);
  limb(cr, {{kx, ky - 2}, {ax, ay}}, 11, boot);
  // The boot's cuff and foot.
  ellipse(cr, kx, ky + 1, 8, 4, std::atan2(ay - ky, ax - kx) + kPi / 2);
  setRgba(cr, boot, 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.2);
  ellipse(cr, ax + 5, ay + 2, 9, 4);
  setRgba(cr, boot, 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.3);
}

void skyPirate(cairo_t* cr, double w, double h, int variant, int frame)
{
  cairo_save(cr);
  frameBox(cr, w, h, 96, 160);
  const bool jet = variant == 3;
  const int stride = frame % 2;
  // The jet pack's flames (or its pilot lights).
  for (const double nx : {20.0, 31.0})
  {
    if (jet)
    {
      const double len = 34.0 + (stride ? 10.0 : 0.0) + (nx > 25 ? 4.0 : 0.0);
      glow(cr, nx - 6, 118 + len * 0.4, len * 0.7, rgb(255, 140, 60), 0.5);
      cairo_move_to(cr, nx - 5, 110);
      cairo_curve_to(cr, nx - 8, 118 + len * 0.4, nx - 14, 112 + len * 0.8, nx - 16, 110 + len);
      cairo_curve_to(cr, nx - 4, 112 + len * 0.7, nx + 4, 118 + len * 0.3, nx + 5, 110);
      cairo_close_path(cr);
      grad(cr, 0, 110, 0, 110 + len, {{0.0, rgb(255, 250, 220)}, {0.35, rgb(255, 200, 90)}, {1.0, rgb(255, 90, 40)}}, 0.95);
    }
    else
      glow(cr, nx, 112, 6, rgb(255, 170, 80), 0.6);
  }
  // The pack: twin brass tanks with nozzles, on his back.
  for (const double tx : {14.0, 25.0})
  {
    cairo_move_to(cr, tx, 62);
    cairo_curve_to(cr, tx, 56, tx + 11, 56, tx + 11, 62);
    cairo_line_to(cr, tx + 11, 102);
    cairo_line_to(cr, tx, 102);
    cairo_close_path(cr);
    grad(cr, tx, 0, tx + 11, 0, {{0.0, kBrassLo}, {0.35, kBrassHi}, {0.7, kBrass}, {1.0, kBrassLo}}, 1.0, true);
    stroke(cr, kInk, 1.4);
    cairo_move_to(cr, tx + 1.5, 102);
    cairo_line_to(cr, tx + 3, 110);
    cairo_line_to(cr, tx + 8, 110);
    cairo_line_to(cr, tx + 9.5, 102);
    cairo_close_path(cr);
    setRgba(cr, rgb(70, 64, 70), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.2);
  }
  for (const double by : {70.0, 92.0})
  {
    cairo_rectangle(cr, 13, by, 24, 3);
    setRgba(cr, kLeatherLo, 1.0);
    cairo_fill(cr);
  }
  // A dial on the pack.
  circle(cr, 25, 80, 4);
  setRgba(cr, rgb(250, 240, 210), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.0);
  cairo_move_to(cr, 25, 80);
  cairo_line_to(cr, 27, 77.5);
  stroke(cr, rgb(200, 40, 30), 0.9);
  // The legs.
  if (jet)
  {
    pirateLeg(cr, 46, 104, 36, 124, 24, 140, true);
    pirateLeg(cr, 56, 104, 50, 126, 40, 144, false);
  }
  else if (stride)
  {
    pirateLeg(cr, 46, 104, 40, 130, 34, 152, true);
    pirateLeg(cr, 58, 104, 66, 128, 72, 152, false);
  }
  else
  {
    pirateLeg(cr, 46, 104, 46, 130, 44, 152, true);
    pirateLeg(cr, 58, 104, 60, 130, 62, 152, false);
  }
  // The far arm.
  if (variant == 2)
  {
    // Holding the cutlass back and down.
    limb(cr, {{44, 66}, {38, 84}, {36, 98}}, 10, rgb(214, 206, 196));
    cutlass(cr, 36, 100, 2.1, 38, false);
  }
  else if (jet)
    limb(cr, {{44, 66}, {58, 76}, {70, 74}}, 10, rgb(200, 192, 182));
  else
    limb(cr, {{44, 66}, {40, 84}, {42, 98}}, 10, rgb(200, 192, 182));
  // The torso: a striped shirt under an open leather waistcoat.
  cairo_move_to(cr, 40, 62);
  cairo_curve_to(cr, 50, 58, 62, 58, 70, 64);
  cairo_curve_to(cr, 72, 80, 70, 96, 66, 106);
  cairo_line_to(cr, 42, 106);
  cairo_curve_to(cr, 38, 92, 37, 76, 40, 62);
  cairo_close_path(cr);
  cairo_save(cr);
  cairo_clip_preserve(cr);
  setRgba(cr, rgb(246, 240, 230), 1.0);
  cairo_fill(cr);
  for (double y = 62; y < 108; y += 9)
  {
    cairo_rectangle(cr, 30, y, 50, 4.5);
    setRgba(cr, rgb(40, 60, 120), 1.0);
    cairo_fill(cr);
  }
  // The waistcoat's back panel and its front edge.
  cairo_move_to(cr, 36, 60);
  cairo_line_to(cr, 58, 60);
  cairo_curve_to(cr, 56, 76, 58, 92, 60, 108);
  cairo_line_to(cr, 36, 108);
  cairo_close_path(cr);
  grad(cr, 36, 0, 60, 0, {{0.0, kLeatherLo}, {0.6, kLeather}, {1.0, lighten(kLeather, 0.15f)}});
  cairo_move_to(cr, 68, 62);
  cairo_curve_to(cr, 66, 76, 66, 90, 70, 108);
  cairo_line_to(cr, 80, 108);
  cairo_line_to(cr, 80, 60);
  cairo_close_path(cr);
  setRgba(cr, kLeather, 1.0);
  cairo_fill(cr);
  cairo_restore(cr);
  cairo_move_to(cr, 40, 62);
  cairo_curve_to(cr, 50, 58, 62, 58, 70, 64);
  cairo_curve_to(cr, 72, 80, 70, 96, 66, 106);
  cairo_line_to(cr, 42, 106);
  cairo_curve_to(cr, 38, 92, 37, 76, 40, 62);
  cairo_close_path(cr);
  stroke(cr, kInk, 1.8);
  // The pack's strap over the shoulder.
  cairo_move_to(cr, 42, 62);
  cairo_curve_to(cr, 52, 70, 58, 84, 62, 96);
  stroke(cr, kInk, 5.0);
  cairo_move_to(cr, 42, 62);
  cairo_curve_to(cr, 52, 70, 58, 84, 62, 96);
  stroke(cr, kLeatherLo, 3.0);
  // The belt, its buckle, the pistol tucked in it when it is not in hand.
  cairo_rectangle(cr, 41, 97, 27, 7);
  setRgba(cr, rgb(30, 22, 22), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.0);
  cairo_rectangle(cr, 59, 96, 8, 9);
  setRgba(cr, kBrass, 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.0);
  if (variant != 2)
    flintlock(cr, 50, 98, -1.9);
  // The red scarf at his neck, its tail streaming back.
  cairo_move_to(cr, 44, 58);
  cairo_curve_to(cr, 36, 56, 28, jet ? 50 : 60, 18, jet ? 48 : 64);
  cairo_curve_to(cr, 26, jet ? 56 : 68, 36, 64, 46, 64);
  cairo_close_path(cr);
  solid(cr, rgb(210, 50, 50), 50, 66, 1.3);
  ellipse(cr, 56, 60, 13, 5);
  solid(cr, rgb(210, 50, 50), 55, 65, 1.3);
  // The head.
  const double hx = 57, hy = 38;
  circle(cr, hx, hy, 16);
  solid(cr, kSkin, hy - 16, hy + 16);
  // The beard: ginger, bushy, round his jaw.
  cairo_move_to(cr, hx - 13, hy + 2);
  cairo_curve_to(cr, hx - 12, hy + 18, hx - 2, hy + 24, hx + 6, hy + 23);
  cairo_curve_to(cr, hx + 14, hy + 22, hx + 18, hy + 14, hx + 17, hy + 6);
  cairo_curve_to(cr, hx + 12, hy + 9, hx + 6, hy + 10, hx, hy + 8);
  cairo_curve_to(cr, hx - 6, hy + 8, hx - 10, hy + 5, hx - 13, hy + 2);
  cairo_close_path(cr);
  solid(cr, rgb(200, 96, 40), hy, hy + 24, 1.4);
  for (int k = 0; k < 5; ++k)
  {
    cairo_move_to(cr, hx - 8 + k * 5, hy + 12);
    cairo_line_to(cr, hx - 7 + k * 5, hy + 19);
    stroke(cr, rgb(150, 60, 24), 1.0, 0.8);
  }
  // The grin, a gold tooth in it.
  cairo_move_to(cr, hx + 4, hy + 11);
  cairo_curve_to(cr, hx + 8, hy + 15, hx + 13, hy + 14, hx + 15, hy + 10);
  cairo_close_path(cr);
  setRgba(cr, rgb(60, 20, 20), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.0);
  cairo_rectangle(cr, hx + 9, hy + 10.5, 3, 2.6);
  setRgba(cr, rgb(255, 210, 90), 1.0);
  cairo_fill(cr);
  // The nose.
  ellipse(cr, hx + 15, hy + 3, 4.5, 4);
  solid(cr, rgb(232, 150, 120), hy - 1, hy + 7, 1.2);
  // The near eye under a scowl; the far one behind a patch.
  circle(cr, hx + 8, hy - 2, 3.2);
  setRgba(cr, rgb(255, 255, 255), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.0);
  circle(cr, hx + 9, hy - 1.6, 1.6);
  setRgba(cr, kInk, 1.0);
  cairo_fill(cr);
  cairo_move_to(cr, hx + 3, hy - 8);
  cairo_line_to(cr, hx + 13, hy - 4.5);
  stroke(cr, rgb(150, 60, 24), 2.6);
  cairo_move_to(cr, hx - 16, hy - 8);
  cairo_line_to(cr, hx + 16, hy - 13);
  stroke(cr, kInk, 1.6);
  ellipse(cr, hx - 2, hy - 2, 5, 4.5);
  setRgba(cr, rgb(20, 14, 16), 1.0);
  cairo_fill(cr);
  // The flying cap: a leather dome, an ear flap, goggles pushed up on it.
  cairo_move_to(cr, hx - 17, hy + 2);
  cairo_curve_to(cr, hx - 18, hy - 18, hx - 2, hy - 22, hx + 10, hy - 18);
  cairo_curve_to(cr, hx + 16, hy - 16, hx + 18, hy - 12, hx + 17, hy - 8);
  cairo_curve_to(cr, hx + 6, hy - 12, hx - 6, hy - 10, hx - 12, hy - 4);
  cairo_close_path(cr);
  solid(cr, kLeather, hy - 22, hy, 1.5);
  cairo_move_to(cr, hx - 6, hy - 19);
  cairo_curve_to(cr, hx - 10, hy - 12, hx - 12, hy - 6, hx - 13, hy);
  stroke(cr, kLeatherLo, 1.0, 0.8);
  cairo_move_to(cr, hx - 14, hy - 4);
  cairo_curve_to(cr, hx - 16, hy + 6, hx - 14, hy + 12, hx - 8, hy + 14);
  cairo_line_to(cr, hx - 5, hy + 4);
  cairo_close_path(cr);
  solid(cr, kLeatherLo, hy - 4, hy + 14, 1.3);
  for (const auto& g : {std::array<double, 3>{hx + 1, hy - 18, 5.5}, {hx + 11, hy - 16, 5.0}})
  {
    circle(cr, g[0], g[1], g[2]);
    grad(cr, g[0] - g[2], g[1] - g[2], g[0] + g[2], g[1] + g[2], {{0.0, kBrassHi}, {1.0, kBrassLo}}, 1.0, true);
    stroke(cr, kInk, 1.2);
    circle(cr, g[0], g[1], g[2] * 0.6);
    grad(cr, 0, g[1] - g[2], 0, g[1] + g[2], {{0.0, rgb(150, 220, 230)}, {1.0, rgb(40, 90, 110)}});
    circle(cr, g[0] - 1, g[1] - 1, 1.0);
    setRgba(cr, rgb(255, 255, 255), 0.9);
    cairo_fill(cr);
  }
  // The near arm and what it holds.
  const Color sleeve = rgb(236, 230, 220);
  switch (variant)
  {
    case 1:
      // The tell: wound up over his head, the blade flashing.
      limb(cr, {{64, 66}, {64, 46}, {52, 28}}, 10, sleeve);
      circle(cr, 52, 28, 5);
      solid(cr, kSkin, 23, 33, 1.2);
      cutlass(cr, 52, 28, -2.4, 44, true);
      break;
    case 2:
      // The tell: the flintlock levelled at you.
      limb(cr, {{64, 66}, {80, 68}, {94, 66}}, 10, sleeve);
      flintlock(cr, 96, 68, 0.0);
      circle(cr, 95, 67, 5);
      solid(cr, kSkin, 62, 72, 1.2);
      break;
    case 3:
      limb(cr, {{64, 66}, {80, 74}, {92, 72}}, 10, sleeve);
      circle(cr, 93, 72, 5);
      solid(cr, kSkin, 67, 77, 1.2);
      cutlass(cr, 94, 72, 0.15, 40, false);
      break;
    default:
      limb(cr, {{64, 66}, {72, 84}, {80, 94}}, 10, sleeve);
      circle(cr, 81, 95, 5);
      solid(cr, kSkin, 90, 100, 1.2);
      cutlass(cr, 82, 96, 0.95 + (stride ? 0.1 : 0.0), 40, false);
      break;
  }
  // The sunset on his shoulders and cap.
  cairo_move_to(cr, hx - 14, hy - 12);
  cairo_curve_to(cr, hx - 8, hy - 21, hx + 4, hy - 22, hx + 12, hy - 18);
  stroke(cr, kRim, 1.6, 0.7);
  cairo_restore(cr);
}

// --- Parrots ---------------------------------------------------------------------------------------

struct Bird
{
  Color body, wingTop, wingMid, wingTip, tail, head;
  bool carrier;
};

constexpr Bird kMacaw{rgb(220, 36, 40), rgb(230, 60, 46), rgb(250, 206, 50), rgb(40, 96, 210), rgb(50, 110, 220),
  rgb(226, 40, 42), false};
constexpr Bird kAmazon{rgb(70, 170, 70), rgb(96, 190, 80), rgb(40, 140, 60), rgb(40, 90, 200), rgb(80, 170, 70),
  rgb(250, 220, 60), true};

// A feather from (x0, y0) to its tip at (x1, y1), `wid` wide.
void feather(cairo_t* cr, double x0, double y0, double x1, double y1, double wid, Color c)
{
  const double dx = x1 - x0, dy = y1 - y0, len = std::max(0.01, std::sqrt(dx * dx + dy * dy));
  const double nx = -dy / len * wid * 0.5, ny = dx / len * wid * 0.5;
  cairo_move_to(cr, x0 + nx * 0.6, y0 + ny * 0.6);
  cairo_curve_to(cr, x0 + dx * 0.4 + nx, y0 + dy * 0.4 + ny, x0 + dx * 0.85 + nx * 0.7, y0 + dy * 0.85 + ny * 0.7, x1, y1);
  cairo_curve_to(cr, x0 + dx * 0.85 - nx * 0.7, y0 + dy * 0.85 - ny * 0.7, x0 + dx * 0.4 - nx, y0 + dy * 0.4 - ny,
    x0 - nx * 0.6, y0 - ny * 0.6);
  cairo_close_path(cr);
  setRgba(cr, c, 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.1);
  cairo_move_to(cr, x0, y0);
  cairo_line_to(cr, x0 + dx * 0.9, y0 + dy * 0.9);
  stroke(cr, lighten(c, 0.35f), 0.7, 0.7);
}

// A wing from the shoulder (x, y) along `angle`, `len` long, its feathers
// fanned `spread` apart: blue primaries, a yellow (or green) band, the
// coverts in the body's colour.
void wing(cairo_t* cr, const Bird& b, double x, double y, double angle, double len, double spread)
{
  for (int k = 4; k >= 0; --k)
  {
    const double a = angle + spread * (double(k) - 1.0);
    const double l = len * (1.0 - double(k) * 0.08);
    feather(cr, x, y, x + std::cos(a) * l, y + std::sin(a) * l, 11, b.wingTip);
  }
  for (int k = 3; k >= 0; --k)
  {
    const double a = angle + spread * (double(k) - 0.5) * 1.2;
    const double l = len * 0.62;
    feather(cr, x, y, x + std::cos(a) * l, y + std::sin(a) * l, 12, b.wingMid);
  }
  ellipse(cr, x + std::cos(angle) * len * 0.18, y + std::sin(angle) * len * 0.18, len * 0.26, 9, angle);
  setRgba(cr, b.wingTop, 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.2);
}

// The head at (hx, hy), the beak along `angle`; `open` 0..1.
void birdHead(cairo_t* cr, const Bird& b, double hx, double hy, double angle, double open, bool crest)
{
  cairo_save(cr);
  cairo_translate(cr, hx, hy);
  cairo_rotate(cr, angle);
  if (crest)
    for (int k = 0; k < 3; ++k)
      feather(cr, -4, -9, -14 + k * 5.0, -26 + k * 2.0, 6, b.head);
  circle(cr, 0, 0, 13);
  solid(cr, b.head, -13, 13, 1.5);
  // The bare white face patch (the macaw), the eye.
  if (!b.carrier)
  {
    ellipse(cr, 5, 1, 7, 6);
    setRgba(cr, rgb(250, 246, 240), 1.0);
    cairo_fill(cr);
    for (int k = 0; k < 3; ++k)
    {
      cairo_move_to(cr, 1, 1 + k * 2.4);
      cairo_line_to(cr, 8, 1.5 + k * 2.4);
      stroke(cr, rgb(120, 60, 60), 0.5, 0.7);
    }
  }
  circle(cr, 4, -2, 3.2);
  setRgba(cr, rgb(255, 220, 70), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.0);
  circle(cr, 4.6, -2, 1.6);
  setRgba(cr, kInk, 1.0);
  cairo_fill(cr);
  // The beak: a hooked upper mandible, a black lower one.
  const double drop = open * 9.0;
  cairo_move_to(cr, 9, 2);
  cairo_curve_to(cr, 13, 3 + drop * 0.4, 16, 6 + drop, 13, 11 + drop);
  cairo_curve_to(cr, 11, 8 + drop, 9, 7 + drop * 0.5, 8, 6);
  cairo_close_path(cr);
  setRgba(cr, rgb(40, 34, 36), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.0);
  cairo_move_to(cr, 8, -5);
  cairo_curve_to(cr, 16, -8 - open * 3.0, 24, -2 - open * 3.0, 20, 10 - open * 4.0);
  cairo_curve_to(cr, 19, 6 - open * 4.0, 15, 3, 9, 3);
  cairo_close_path(cr);
  grad(cr, 0, -8, 0, 10, {{0.0, rgb(250, 244, 226)}, {1.0, rgb(190, 176, 150)}}, 1.0, true);
  stroke(cr, kInk, 1.2);
  if (open > 0.5)
  {
    // The tongue.
    ellipse(cr, 14, 4 + drop * 0.5, 3, 2);
    setRgba(cr, rgb(60, 50, 56), 1.0);
    cairo_fill(cr);
  }
  if (b.carrier)
  {
    // The mail carrier's cap: navy, a visor, a brass badge.
    cairo_move_to(cr, -12, -5);
    cairo_curve_to(cr, -12, -16, 6, -18, 10, -9);
    cairo_line_to(cr, -12, -5);
    cairo_close_path(cr);
    solid(cr, rgb(40, 54, 110), -16, -5, 1.3);
    cairo_move_to(cr, 4, -8);
    cairo_curve_to(cr, 10, -10, 16, -9, 18, -7);
    cairo_line_to(cr, 8, -5);
    cairo_close_path(cr);
    setRgba(cr, rgb(20, 24, 40), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.0);
    circle(cr, -1, -11, 2.4);
    setRgba(cr, kBrass, 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 0.7);
  }
  else
  {
    // Flying goggles on its brow, the strap round its head.
    cairo_move_to(cr, -13, -2);
    cairo_curve_to(cr, -8, -9, 0, -11, 8, -9);
    stroke(cr, kLeatherLo, 3.0);
    circle(cr, 4, -10, 4.6);
    grad(cr, 0, -15, 0, -5, {{0.0, kBrassHi}, {1.0, kBrassLo}}, 1.0, true);
    stroke(cr, kInk, 1.1);
    circle(cr, 4, -10, 2.8);
    grad(cr, 0, -13, 0, -7, {{0.0, rgb(160, 230, 240)}, {1.0, rgb(40, 100, 120)}});
    circle(cr, 3, -11, 0.9);
    setRgba(cr, rgb(255, 255, 255), 0.9);
    cairo_fill(cr);
  }
  cairo_restore(cr);
}

void satchel(cairo_t* cr, double x, double y)
{
  cairo_move_to(cr, x + 14, y - 26);
  cairo_curve_to(cr, x + 6, y - 16, x + 2, y - 8, x, y);
  stroke(cr, kLeatherLo, 2.6);
  // An envelope sticking out.
  cairo_save(cr);
  cairo_translate(cr, x + 2, y - 3);
  cairo_rotate(cr, -0.3);
  cairo_rectangle(cr, -6, -6, 13, 9);
  setRgba(cr, rgb(250, 246, 236), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 0.9);
  cairo_move_to(cr, -6, -6);
  cairo_line_to(cr, 0.5, -1);
  cairo_line_to(cr, 7, -6);
  stroke(cr, rgb(160, 150, 140), 0.7);
  cairo_rectangle(cr, 3, -5, 3, 3);
  setRgba(cr, rgb(220, 60, 50), 1.0);
  cairo_fill(cr);
  cairo_restore(cr);
  cairo_rectangle(cr, x - 9, y - 2, 18, 13);
  solid(cr, kLeather, y - 2, y + 11, 1.2);
  cairo_move_to(cr, x - 9, y + 3);
  cairo_line_to(cr, x + 9, y + 3);
  stroke(cr, kLeatherLo, 1.2);
  cairo_rectangle(cr, x - 1.5, y + 2, 3, 3);
  setRgba(cr, kBrass, 1.0);
  cairo_fill(cr);
}

void feet(cairo_t* cr, double x, double y, bool tucked)
{
  for (const double fx : {x - 5.0, x + 5.0})
  {
    if (tucked)
    {
      cairo_move_to(cr, fx, y - 6);
      cairo_line_to(cr, fx - 4, y);
      stroke(cr, kInk, 4.0);
      cairo_move_to(cr, fx, y - 6);
      cairo_line_to(cr, fx - 4, y);
      stroke(cr, rgb(110, 110, 120), 2.2);
      continue;
    }
    cairo_move_to(cr, fx, y - 8);
    cairo_line_to(cr, fx, y - 1);
    stroke(cr, kInk, 4.4);
    cairo_move_to(cr, fx, y - 8);
    cairo_line_to(cr, fx, y - 1);
    stroke(cr, rgb(120, 120, 130), 2.4);
    for (const double tx : {-5.0, 5.0})
    {
      cairo_move_to(cr, fx, y - 1);
      cairo_line_to(cr, fx + tx, y + 1);
      stroke(cr, kInk, 3.4);
      cairo_move_to(cr, fx, y - 1);
      cairo_line_to(cr, fx + tx, y + 1);
      stroke(cr, rgb(120, 120, 130), 1.8);
    }
  }
}

// pose: 0 perched (frame 1 pecking), 1 the squawk, 2 diving, 3 flying
// (frame 0 wings up, 1 down), 4 mid-hop.
void parrot(cairo_t* cr, const Bird& b, double w, double h, int pose, int frame)
{
  cairo_save(cr);
  frameBox(cr, w, h, 96, 96);
  if (pose == 2 || pose == 3)
  {
    // Level: the body along the flight, the tail trailing.
    const double by = 48;
    for (int k = 0; k < 3; ++k)
      feather(cr, 26, by + 2, -8 + k * 2.0, by + 6 + k * 6.0, 9, k == 1 ? b.wingTop : b.tail);
    if (pose == 3 && frame % 2 == 0)
      wing(cr, b, 44, by - 6, -2.2, 46, 0.16); // the far wing up
    ellipse(cr, 46, by, 27, 13, -0.05);
    solid(cr, b.body, by - 13, by + 13);
    if (b.carrier)
      satchel(cr, 44, by + 6);
    feet(cr, 40, by + 18, true);
    if (pose == 2)
      wing(cr, b, 54, by - 6, 3.0, 46, 0.1); // swept back
    else
      wing(cr, b, 52, by - 4, frame % 2 == 0 ? -2.0 : 2.3, 46, 0.18);
    birdHead(cr, b, 72, by - 6, pose == 2 ? 0.2 : 0.05, 0.0, false);
    cairo_restore(cr);
    return;
  }
  const bool squawk = pose == 1, hop = pose == 4;
  const bool peck = pose == 0 && frame % 2 == 1;
  const double lift = hop ? -8.0 : 0.0;
  // The tail, long, down behind the perch.
  for (int k = 0; k < 3; ++k)
    feather(cr, 40, 74 + lift, 10 + k * 5.0, 104 + lift - k * 2.0, 9, k == 1 ? b.wingTop : b.tail);
  if (squawk)
    wing(cr, b, 40, 48, -2.3, 44, 0.16); // the far wing flared up
  if (hop)
    wing(cr, b, 40, 50 + lift, -2.7, 40, 0.16);
  // The body, upright, leaning a little forward; puffed up in the squawk.
  ellipse(cr, 46, 62 + lift, squawk ? 20 : 17, squawk ? 27 : 25, -0.3);
  solid(cr, b.body, 36 + lift, 88 + lift);
  // The neck up to the head.
  const double hx = peck ? 66 : (squawk ? 56 : 58), hy = (peck ? 52 : (squawk ? 26 : 32)) + lift;
  cairo_move_to(cr, 50, 46 + lift);
  cairo_line_to(cr, hx - 2, hy + 4);
  stroke(cr, kInk, 19);
  cairo_move_to(cr, 50, 46 + lift);
  cairo_line_to(cr, hx - 2, hy + 4);
  stroke(cr, b.body, 15);
  if (b.carrier)
    satchel(cr, 40, 68 + lift);
  feet(cr, 48, 95 + lift, hop);
  // The near wing, folded (or flared, or out for the hop).
  if (squawk)
    wing(cr, b, 50, 50, -1.0, 44, 0.16);
  else if (hop)
    wing(cr, b, 48, 52 + lift, 2.6, 42, 0.18);
  else
    wing(cr, b, 46, 48 + lift, 1.95, 44, 0.05);
  birdHead(cr, b, hx, hy, peck ? 1.0 : (squawk ? -0.45 : 0.0), squawk ? 1.0 : 0.0, squawk);
  // The sunset along its back.
  cairo_arc(cr, 46, 62 + lift, 18, kPi * 1.15, kPi * 1.55);
  stroke(cr, kRim, 1.4, 0.6);
  cairo_restore(cr);
}

// --- Balloon Mine ----------------------------------------------------------------------------------

void balloonMine(cairo_t* cr, double w, double h, int variant, int frame)
{
  cairo_save(cr);
  frameBox(cr, w, h, 128, 128);
  const bool lit = variant == 1;
  const bool flash = lit && frame % 2 == 0;
  // A little sway about the balloon's crown.
  cairo_translate(cr, 64, 6);
  cairo_rotate(cr, frame % 2 ? 0.04 : -0.04);
  cairo_translate(cr, -64, -6);
  // The balloon: patched canvas, its gores, a net over it.
  circle(cr, 64, 32, 27);
  grad(cr, 40, 8, 90, 58, {{0.0, rgb(232, 220, 186)}, {0.55, rgb(196, 180, 140)}, {1.0, rgb(120, 104, 84)}}, 1.0, true);
  stroke(cr, kInk, 1.8);
  cairo_save(cr);
  circle(cr, 64, 32, 27);
  cairo_clip(cr);
  for (const double gx : {-0.55, 0.0, 0.55})
  {
    cairo_move_to(cr, 64, 5);
    cairo_curve_to(cr, 64 + gx * 50, 20, 64 + gx * 50, 44, 64, 59);
    stroke(cr, rgb(120, 104, 84), 1.0, 0.8);
  }
  // Patches, stitched.
  cairo_rectangle(cr, 46, 20, 11, 9);
  setRgba(cr, rgb(180, 90, 70), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 0.9);
  cairo_rectangle(cr, 72, 36, 10, 8);
  setRgba(cr, rgb(110, 130, 150), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 0.9);
  for (double sx = 47; sx < 57; sx += 2.5)
  {
    cairo_move_to(cr, sx, 19);
    cairo_line_to(cr, sx + 1, 21);
    stroke(cr, rgb(250, 240, 220), 0.6);
  }
  cairo_restore(cr);
  cairo_arc(cr, 64, 32, 25, kPi * 1.15, kPi * 1.7);
  stroke(cr, kRim, 2.0, 0.7);
  // The lines down to the mine.
  for (const double lx : {44.0, 56.0, 72.0, 84.0})
  {
    cairo_move_to(cr, lx, 50);
    cairo_line_to(cr, 64 + (lx - 64) * 0.4, 78);
    stroke(cr, rgb(80, 60, 50), 1.1);
  }
  // The mine: an iron sphere with brass-capped horns.
  const double mx = 64, my = 100, mr = 22;
  if (lit)
    glow(cr, mx, my, 54, rgb(255, 60, 40), flash ? 0.7 : 0.45);
  for (int k = 0; k < 7; ++k)
  {
    const double a = -kPi + kPi * double(k) / 6.0; // round the upper half
    const double ax = mx + std::cos(a) * (mr + 7), ay = my + std::sin(a) * (mr + 7);
    if (k == 3)
      continue; // the lamp sits on the crown
    cairo_move_to(cr, mx + std::cos(a) * mr * 0.9, my + std::sin(a) * mr * 0.9);
    cairo_line_to(cr, ax, ay);
    stroke(cr, kInk, 7.0);
    cairo_move_to(cr, mx + std::cos(a) * mr * 0.9, my + std::sin(a) * mr * 0.9);
    cairo_line_to(cr, ax, ay);
    stroke(cr, rgb(90, 94, 104), 4.4);
    circle(cr, ax, ay, 3.6);
    if (lit)
      setRgba(cr, flash ? rgb(255, 250, 200) : rgb(255, 170, 80), 1.0);
    else
      setRgba(cr, kBrass, 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.0);
  }
  for (const double a : {kPi * 0.25, kPi * 0.75})
  {
    const double ax = mx + std::cos(a) * (mr + 6), ay = my + std::sin(a) * (mr + 6);
    cairo_move_to(cr, mx + std::cos(a) * mr * 0.9, my + std::sin(a) * mr * 0.9);
    cairo_line_to(cr, ax, ay);
    stroke(cr, kInk, 6.0);
    circle(cr, ax, ay, 3.0);
    setRgba(cr, lit ? rgb(255, 170, 80) : kBrass, 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.0);
  }
  circle(cr, mx, my, mr);
  if (lit)
    grad(cr, mx - mr, my - mr, mx + mr, my + mr,
      {{0.0, flash ? rgb(255, 200, 160) : rgb(230, 110, 90)}, {0.5, rgb(200, 40, 30)}, {1.0, rgb(90, 10, 10)}}, 1.0, true);
  else
    grad(cr, mx - mr, my - mr, mx + mr, my + mr, {{0.0, rgb(130, 136, 150)}, {0.45, rgb(70, 74, 86)}, {1.0, rgb(24, 24, 32)}},
      1.0, true);
  stroke(cr, kInk, 1.8);
  // The seam and its rivets.
  cairo_move_to(cr, mx - mr, my);
  cairo_curve_to(cr, mx - mr * 0.5, my + 5, mx + mr * 0.5, my + 5, mx + mr, my);
  stroke(cr, kInk, 1.2, 0.8);
  for (double k = -0.8; k <= 0.81; k += 0.4)
  {
    circle(cr, mx + k * mr, my + 3.6 * (1.0 - k * k), 1.1);
    setRgba(cr, lit ? rgb(255, 200, 150) : rgb(170, 176, 190), 1.0);
    cairo_fill(cr);
  }
  // A painted skull.
  circle(cr, mx + 4, my + 10, 4.2);
  setRgba(cr, rgb(250, 246, 236), lit ? 0.5 : 0.85);
  cairo_fill(cr);
  for (const double ex : {mx + 2.4, mx + 5.8})
  {
    circle(cr, ex, my + 9.5, 1.1);
    setRgba(cr, kInk, 1.0);
    cairo_fill(cr);
  }
  // The highlight.
  ellipse(cr, mx - 8, my - 9, 6, 3.5, -0.6);
  setRgba(cr, rgb(255, 255, 255), lit ? 0.6 : 0.35);
  cairo_fill(cr);
  // The lamp on its crown.
  cairo_rectangle(cr, mx - 4, my - mr - 9, 8, 10);
  setRgba(cr, kBrassLo, 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.0);
  circle(cr, mx, my - mr - 10, 5);
  setRgba(cr, lit ? (flash ? rgb(255, 255, 230) : rgb(255, 80, 60)) : rgb(150, 40, 40), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.1);
  if (lit)
    glow(cr, mx, my - mr - 10, 18, rgb(255, 90, 60), 0.8);
  cairo_restore(cr);
}

// --- The duck's flying cap --------------------------------------------------------------------------

void duckAviator(cairo_t* cr, double w, double h, int variant)
{
  if (variant != 1)
    return;
  cairo_save(cr);
  cairo_translate(cr, kM, kM);
  cairo_scale(cr, w / 64.0, h / 64.0);
  // The duck's head is a circle at (38, 24), radius 12; its neck meets the
  // body at y 35. The silk scarf round its neck, the tail streaming back.
  cairo_move_to(cr, 30, 34);
  cairo_curve_to(cr, 22, 32, 14, 35, 5, 31);
  cairo_curve_to(cr, 11, 38, 22, 40, 31, 38);
  cairo_close_path(cr);
  solid(cr, rgb(250, 248, 240), 31, 40, 1.0);
  ellipse(cr, 38, 35.5, 11, 3.4);
  solid(cr, rgb(250, 248, 240), 32, 39, 1.0);
  // The cap: a leather dome hugging the top of the head down to the brow,
  // an ear flap at the back with its strap.
  cairo_arc(cr, 38, 24, 13.2, kPi * 1.02, kPi * 1.86);
  cairo_curve_to(cr, 44, 17, 34, 17.5, 25, 21);
  cairo_close_path(cr);
  solid(cr, kLeather, 10, 21, 1.1);
  cairo_move_to(cr, 38, 11);
  cairo_curve_to(cr, 36, 14, 36, 17, 37, 19);
  stroke(cr, kLeatherLo, 0.9, 0.8);
  cairo_move_to(cr, 26, 19);
  cairo_curve_to(cr, 24, 24, 25, 29, 29, 31);
  cairo_line_to(cr, 32, 21);
  cairo_close_path(cr);
  solid(cr, kLeatherLo, 19, 31, 1.0);
  cairo_move_to(cr, 30, 30);
  cairo_curve_to(cr, 34, 33, 40, 33, 44, 31);
  stroke(cr, kLeatherLo, 1.4);
  // The goggles pushed up on its brow.
  cairo_move_to(cr, 27, 16);
  cairo_curve_to(cr, 34, 12, 44, 11, 51, 15);
  stroke(cr, rgb(50, 36, 30), 2.2);
  for (const auto& g : {std::array<double, 3>{40, 13, 4.2}, {48, 14, 3.6}})
  {
    circle(cr, g[0], g[1], g[2]);
    grad(cr, 0, g[1] - g[2], 0, g[1] + g[2], {{0.0, kBrassHi}, {1.0, kBrassLo}}, 1.0, true);
    stroke(cr, kInk, 0.9);
    circle(cr, g[0], g[1], g[2] * 0.6);
    grad(cr, 0, g[1] - g[2], 0, g[1] + g[2], {{0.0, rgb(160, 230, 240)}, {1.0, rgb(40, 100, 120)}});
    circle(cr, g[0] - 0.8, g[1] - 0.8, 0.8);
    setRgba(cr, rgb(255, 255, 255), 0.9);
    cairo_fill(cr);
  }
  cairo_restore(cr);
}

} // namespace

bool drawFleet34Art(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame)
{
  (void)t;
  if (key == "sky_pirate")
    skyPirate(cr, w, h, std::clamp(variant, 0, 3), frame % 2);
  else if (key == "peck_parrot")
    parrot(cr, kMacaw, w, h, std::clamp(variant, 0, 3), frame % 2);
  else if (key == "carrier_parrot")
  {
    static constexpr std::array<int, 5> kPose{0, 4, 1, 2, 3};
    parrot(cr, kAmazon, w, h, kPose[std::size_t(std::clamp(variant, 0, 4))], frame % 2);
  }
  else if (key == "balloon_mine")
    balloonMine(cr, w, h, std::clamp(variant, 0, 1), frame % 2);
  else if (key == "duck_aviator")
    duckAviator(cr, w, h, variant);
  else
    return false;
  return true;
}

} // namespace gr
