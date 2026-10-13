// Magma Mainframe (Level 33) in the style of enemy_art.cpp: smooth shapes
// with a dark ink line and a hot orange rim of magma light along their lower
// edges, blue status lights where something is a machine. The Firewall is a
// literal one, a low wall of molten bricks in an iron frame with a padlock
// badge and a sheet of flame on top; the Lava Daemon an imp of cooling lava,
// black crust plates over a glowing core; the Fan Bot a wall-mounted blower
// with a motor-box face. The rubber duck wears a fire proximity hood. Each
// is drawn in screen pixels into a texture with a 32 px margin, the cell box
// at (32, 32) .. (32 + w, 32 + h), facing right.

#include "assets/enemy_art_magma33.hpp"

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
constexpr Color kInk = rgb(22, 12, 10);
constexpr Color kCrust = rgb(46, 36, 38);
constexpr Color kCrustHi = rgb(92, 78, 80);
constexpr Color kCore = rgb(255, 120, 30);
constexpr Color kHot = rgb(255, 226, 140);
constexpr Color kWhiteHot = rgb(255, 252, 230);
constexpr Color kSteel = rgb(110, 116, 130);
constexpr Color kSteelDark = rgb(46, 48, 58);
constexpr Color kLed = rgb(80, 180, 255);

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

// Puts the box's (0, 0) at its top-left and scales a design drawn in
// (dw x dh) units to fill it.
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

// --- Firewall ---------------------------------------------------------------------------------------

// One layer of the flame sheet between `top` and `bottom`: tongues along
// its top edge, each rising and falling with `phase`.
void flameLayer(cairo_t* cr, double x0, double x1, double top, double bottom, double phase, double inset)
{
  const int tongues = 7;
  const double span = (x1 - x0) / tongues;
  const double h = bottom - top;
  cairo_move_to(cr, x0 + inset, bottom);
  cairo_line_to(cr, x0 + inset, top + h * 0.55);
  for (int j = 0; j < tongues; ++j)
  {
    const double a = x0 + span * j, b = a + span;
    const double peak = top + inset * 0.6 + h * 0.3 * (0.5 + 0.5 * std::sin(phase * 1.7 + j * 2.3));
    const double lean = std::sin(phase + j * 1.1) * span * 0.25;
    const double valley = top + h * (0.42 + 0.1 * std::sin(phase * 2.1 + j));
    cairo_curve_to(cr, a + span * 0.2, valley, a + span * 0.35 + lean, peak + h * 0.08, a + span * 0.5 + lean, peak);
    cairo_curve_to(cr, a + span * 0.65 + lean, peak + h * 0.08, b - span * 0.2, valley, std::min(b, x1 - inset), valley);
  }
  cairo_line_to(cr, x1 - inset, bottom);
  cairo_close_path(cr);
}

void flames(cairo_t* cr, double top, double bottom, double phase, bool hot)
{
  const double x0 = 4, x1 = 124;
  glow(cr, 64, (top + bottom) * 0.5, (bottom - top) * 0.7 + 30, rgb(255, 100, 30), 0.35);
  flameLayer(cr, x0, x1, top, bottom, phase, 0);
  vgrad(cr, top, bottom, {{0.0, hot ? rgb(255, 200, 80) : rgb(220, 50, 20)}, {0.5, rgb(255, 110, 30)}, {1.0, rgb(255, 170, 50)}}, 0.92,
    true);
  stroke(cr, hot ? rgb(255, 120, 40) : rgb(120, 20, 10), 1.6, 0.8);
  const double h = bottom - top;
  flameLayer(cr, x0 + 10, x1 - 10, top + h * 0.22, bottom, phase + 1.3, 6);
  vgrad(cr, top + h * 0.2, bottom, {{0.0, rgb(255, 160, 40)}, {1.0, rgb(255, 214, 90)}}, 0.95);
  flameLayer(cr, x0 + 22, x1 - 22, top + h * 0.45, bottom, phase + 2.6, 10);
  vgrad(cr, top + h * 0.45, bottom, {{0.0, hot ? kWhiteHot : kHot}, {1.0, kWhiteHot}}, 0.95);
}

void firewall(cairo_t* cr, double w, double h, int variant, int frame)
{
  cairo_save(cr);
  frameBox(cr, w, h, 128, 192);
  const double phase = frame * 1.5708;
  const double wallTop = 150;
  // The flames, behind the wall's top rail.
  if (variant == 2)
  {
    // Dipped: a bed of low flames, embers drifting up out of it.
    flames(cr, 112, wallTop + 4, phase, false);
    for (int k = 0; k < 7; ++k)
    {
      const double ex = 14 + k * 17 + std::sin(phase + k) * 4, ey = 100 - ((k * 23 + frame * 9) % 60);
      circle(cr, ex, ey, 1.8);
      setRgba(cr, k % 2 ? kHot : rgb(255, 140, 50), 0.8);
      cairo_fill(cr);
    }
  }
  else if (variant == 1)
  {
    // The tell: the sheet flickers like a bad signal, sliced and shifted,
    // white-hot.
    for (int band = 0; band < 13; ++band)
    {
      const double y0 = band * 12.0;
      if ((band + frame) % 5 == 0)
        continue; // a dropped slice
      cairo_save(cr);
      cairo_rectangle(cr, -20, y0, 168, 12);
      cairo_clip(cr);
      cairo_translate(cr, ((band + frame) % 2 ? 7.0 : -7.0) * ((band % 3) ? 1.0 : 0.4), 0);
      flames(cr, 0, wallTop + 4, phase, true);
      cairo_restore(cr);
    }
    for (int k = 0; k < 4; ++k)
    {
      const double y = 20 + ((k * 37 + frame * 23) % 120);
      cairo_move_to(cr, 10, y);
      cairo_line_to(cr, 118, y);
      stroke(cr, kWhiteHot, 1.5, 0.7);
    }
  }
  else
    flames(cr, 0, wallTop + 4, phase, false);
  // Its eyes, white-hot in the flames (squinting in the tell; just above
  // the rail when dipped).
  {
    const double ey = variant == 2 ? 132 : 58;
    const double open = variant == 1 ? 2.0 : 5.0;
    for (const double ex : {46.0, 82.0})
    {
      glow(cr, ex, ey, 16, kWhiteHot, 0.7);
      cairo_move_to(cr, ex - 10, ey + (ex < 64 ? -3 : 3) * 0.6);
      cairo_curve_to(cr, ex - 4, ey - open, ex + 4, ey - open, ex + 10, ey + (ex < 64 ? 3 : -3) * 0.6);
      cairo_curve_to(cr, ex + 4, ey + open, ex - 4, ey + open, ex - 10, ey + (ex < 64 ? -3 : 3) * 0.6);
      cairo_close_path(cr);
      setRgba(cr, kWhiteHot, 1.0);
      cairo_fill_preserve(cr);
      stroke(cr, rgb(255, 90, 20), 1.4);
      // Angry brows.
      cairo_move_to(cr, ex - 12, ey - 9 + (ex < 64 ? -3 : 3));
      cairo_line_to(cr, ex + 10, ey - 9 + (ex < 64 ? 3 : -3));
      stroke(cr, rgb(140, 20, 10), 3.0, 0.9);
    }
  }
  // The wall: molten bricks in an iron frame.
  cairo_rectangle(cr, 6, wallTop, 116, 42);
  setRgba(cr, rgb(40, 10, 8), 1.0);
  cairo_fill(cr);
  for (int row = 0; row < 3; ++row)
  {
    const double y = wallTop + 6 + row * 12.0;
    for (double x = row % 2 ? -8.0 : 4.0; x < 122; x += 24)
    {
      const double bx0 = std::max(8.0, x + 1), bx1 = std::min(120.0, x + 23);
      if (bx1 - bx0 < 4)
        continue;
      cairo_rectangle(cr, bx0, y + 1, bx1 - bx0, 10);
      const double heat = 0.5 + 0.5 * std::sin(phase + x * 0.13 + row);
      vgrad(cr, y + 1, y + 11, {{0.0, lerpColor(rgb(200, 60, 20), kHot, float(heat) * 0.6f)}, {1.0, rgb(140, 30, 14)}}, 1.0, true);
      stroke(cr, rgb(255, 150, 60), 0.8, 0.5);
      // A trace of circuitry etched in each brick.
      cairo_move_to(cr, bx0 + 3, y + 6);
      cairo_line_to(cr, bx0 + (bx1 - bx0) * 0.5, y + 6);
      cairo_line_to(cr, bx0 + (bx1 - bx0) * 0.5 + 3, y + 3);
      stroke(cr, rgb(90, 20, 10), 1.0, 0.7);
    }
  }
  // The frame: a top rail and two posts, riveted.
  cairo_rectangle(cr, 2, wallTop - 4, 124, 8);
  vgrad(cr, wallTop - 4, wallTop + 4, {{0.0, rgb(150, 140, 140)}, {1.0, rgb(60, 50, 52)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  for (const double x : {2.0, 118.0})
  {
    cairo_rectangle(cr, x, wallTop - 4, 8, 46);
    hgrad(cr, x, x + 8, {{0.0, rgb(140, 130, 132)}, {1.0, rgb(50, 42, 44)}}, 1.0, true);
    stroke(cr, kInk, 2.0);
  }
  for (double x = 14; x < 120; x += 16)
  {
    circle(cr, x, wallTop, 1.4);
    setRgba(cr, rgb(220, 210, 200), 0.9);
    cairo_fill(cr);
  }
  // The padlock badge.
  cairo_arc(cr, 64, wallTop + 16, 8, kPi, 2 * kPi);
  stroke(cr, kInk, 6.0);
  cairo_arc(cr, 64, wallTop + 16, 8, kPi, 2 * kPi);
  stroke(cr, rgb(200, 204, 214), 3.0);
  cairo_rectangle(cr, 52, wallTop + 16, 24, 20);
  vgrad(cr, wallTop + 16, wallTop + 36, {{0.0, rgb(255, 220, 110)}, {1.0, rgb(190, 130, 30)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  circle(cr, 64, wallTop + 24, 3);
  setRgba(cr, kInk, 1.0);
  cairo_fill(cr);
  cairo_move_to(cr, 64, wallTop + 25);
  cairo_line_to(cr, 64, wallTop + 31);
  stroke(cr, kInk, 2.4);
  cairo_restore(cr);
}

// --- Lava Daemon ------------------------------------------------------------------------------------

// The body: a hunched blob of molten core with crust plates over it.
void daemonBody(cairo_t* cr, double cx, double top, double bottom, double glowK, std::uint32_t seed)
{
  const double hw = 27;
  cairo_move_to(cr, cx - hw * 0.6, top + 8);
  cairo_curve_to(cr, cx - hw * 0.4, top - 4, cx + hw * 0.4, top - 4, cx + hw * 0.6, top + 8);
  cairo_curve_to(cr, cx + hw * 1.15, top + 30, cx + hw * 1.1, bottom - 6, cx + hw * 0.6, bottom);
  cairo_curve_to(cr, cx + hw * 0.2, bottom + 4, cx - hw * 0.2, bottom + 4, cx - hw * 0.6, bottom);
  cairo_curve_to(cr, cx - hw * 1.1, bottom - 6, cx - hw * 1.15, top + 30, cx - hw * 0.6, top + 8);
  cairo_close_path(cr);
  cairo_save(cr);
  cairo_clip_preserve(cr);
  // The core, showing through every crack.
  vgrad(cr, top, bottom, {{0.0, lerpColor(kCore, kHot, float(glowK) * 0.5f)}, {1.0, rgb(200, 50, 10)}}, 1.0, true);
  cairo_new_path(cr);
  // Crust plates: a jittered grid of rounded polygons, gaps between them.
  Rng rng(seed);
  int row = 0;
  for (double y = top - 4; y < bottom + 10; y += 19, ++row)
    for (double x = cx - hw * 1.3 + (row % 2) * 10; x < cx + hw * 1.3; x += 20)
    {
      const double jx = rng.range(-2, 2), jy = rng.range(-2, 2), s = rng.range(9.0f, 10.2f);
      cairo_move_to(cr, x + jx - s, y + jy - s * 0.6);
      cairo_line_to(cr, x + jx + s * 0.4, y + jy - s);
      cairo_line_to(cr, x + jx + s, y + jy - s * 0.1);
      cairo_line_to(cr, x + jx + s * 0.6, y + jy + s);
      cairo_line_to(cr, x + jx - s * 0.7, y + jy + s * 0.8);
      cairo_close_path(cr);
      vgrad(cr, y - s, y + s, {{0.0, kCrustHi}, {0.5, kCrust}, {1.0, darken(kCrust, 0.55f)}}, 1.0, true);
      stroke(cr, rgb(20, 14, 16), 1.0, 0.6);
    }
  cairo_restore(cr);
  // The outline, with a hot rim along its underside.
  stroke(cr, kInk, 2.4);
  cairo_move_to(cr, cx - hw * 0.9, bottom - 8);
  cairo_curve_to(cr, cx - hw * 0.5, bottom + 2, cx + hw * 0.5, bottom + 2, cx + hw * 0.9, bottom - 8);
  stroke(cr, rgb(255, 160, 70), 1.6, 0.8);
}

void daemonFace(cairo_t* cr, double cx, double ey, int variant)
{
  // Horns.
  for (const double s : {-1.0, 1.0})
  {
    cairo_move_to(cr, cx + s * 10, ey - 14);
    cairo_curve_to(cr, cx + s * 16, ey - 22, cx + s * 20, ey - 28, cx + s * 22, ey - 36);
    cairo_curve_to(cr, cx + s * 14, ey - 30, cx + s * 8, ey - 24, cx + s * 4, ey - 16);
    cairo_close_path(cr);
    vgrad(cr, ey - 36, ey - 14, {{0.0, rgb(30, 24, 26)}, {1.0, kCrustHi}}, 1.0, true);
    stroke(cr, kInk, 1.6);
  }
  // Eyes: white-hot slits (half shut and content when roasting).
  const double open = variant == 4 ? 1.2 : (variant == 1 ? 4.5 : 3.2);
  for (const double ex : {cx - 4.0, cx + 11.0})
  {
    glow(cr, ex, ey, 9, kHot, 0.8);
    ellipse(cr, ex, ey, 4.4, open, 0.12);
    setRgba(cr, kWhiteHot, 1.0);
    cairo_fill(cr);
  }
  // The mouth: a glowing grin (open wide on the throw).
  if (variant == 2)
  {
    ellipse(cr, cx + 5, ey + 12, 6, 5);
    setRgba(cr, kHot, 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.4);
  }
  else
  {
    cairo_move_to(cr, cx - 6, ey + 10);
    cairo_curve_to(cr, cx, ey + 15, cx + 10, ey + 15, cx + 15, ey + 9);
    stroke(cr, kInk, 4.0);
    cairo_move_to(cr, cx - 6, ey + 10);
    cairo_curve_to(cr, cx, ey + 15, cx + 10, ey + 15, cx + 15, ey + 9);
    stroke(cr, kHot, 2.0);
  }
}

void fireballAt(cairo_t* cr, double x, double y, double rad)
{
  glow(cr, x, y, rad * 2.4, rgb(255, 120, 30), 0.8);
  circle(cr, x, y, rad);
  cairo_pattern_t* g = cairo_pattern_create_radial(x - rad * 0.3, y - rad * 0.3, 0, x, y, rad);
  stop(g, 0.0, kWhiteHot);
  stop(g, 0.5, kHot);
  stop(g, 1.0, rgb(255, 100, 20));
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
}

void lavaDaemon(cairo_t* cr, double w, double h, int variant, int frame)
{
  cairo_save(cr);
  frameBox(cr, w, h, 96, 128);
  const double glowK = frame % 2 ? 1.0 : 0.6;
  const bool sitting = variant == 4, leaping = variant == 3;
  const double cx = sitting ? 40 : 46;
  const double top = sitting ? 50 : (leaping ? 30 : 38), bottom = sitting ? 116 : (leaping ? 96 : 104);
  glow(cr, cx, (top + bottom) * 0.5, 54, rgb(255, 110, 30), 0.25 + 0.1 * glowK);
  // Legs.
  if (sitting)
  {
    limb(cr, {{cx - 8, 110}, {cx + 10, 118}, {cx + 24, 124}}, 9, kCrust);
    limb(cr, {{cx + 6, 110}, {cx + 22, 116}, {cx + 34, 124}}, 9, kCrustHi);
  }
  else if (leaping)
  {
    limb(cr, {{cx - 10, 92}, {cx - 18, 104}, {cx - 6, 112}}, 9, kCrust);
    limb(cr, {{cx + 10, 92}, {cx + 20, 102}, {cx + 12, 112}}, 9, kCrustHi);
  }
  else
  {
    limb(cr, {{cx - 12, 98}, {cx - 16, 114}, {cx - 20, 126}}, 10, kCrust);
    limb(cr, {{cx + 12, 98}, {cx + 16, 114}, {cx + 20, 126}}, 10, kCrustHi);
    for (const double fx : {cx - 20, cx + 20})
    {
      ellipse(cr, fx + 3, 125, 8, 3.4);
      setRgba(cr, kCrust, 1.0);
      cairo_fill_preserve(cr);
      stroke(cr, kInk, 1.4);
    }
  }
  // The far arm.
  if (leaping)
    limb(cr, {{cx - 18, top + 24}, {cx - 26, top + 8}, {cx - 22, top - 6}}, 8, kCrust);
  else
    limb(cr, {{cx - 18, top + 26}, {cx - 28, top + 44}, {cx - 26, top + 60}}, 8, kCrust);
  // A stubby tail.
  cairo_move_to(cr, cx - 22, bottom - 10);
  cairo_curve_to(cr, cx - 36, bottom - 6, cx - 42, bottom - 18, cx - 40, bottom - 28);
  stroke(cr, kInk, 7.0);
  cairo_move_to(cr, cx - 22, bottom - 10);
  cairo_curve_to(cr, cx - 36, bottom - 6, cx - 42, bottom - 18, cx - 40, bottom - 28);
  stroke(cr, kCrust, 4.0);
  daemonBody(cr, cx, top, bottom, glowK, 33101u + std::uint32_t(variant));
  daemonFace(cr, cx, top + 16, variant);
  // The near arm, which throws.
  const double sx = cx + 18, sy = top + 26;
  switch (variant)
  {
    case 1:
    {
      // Wound back over its head, glowing, a fireball forming in its fist.
      limb(cr, {{sx, sy}, {cx + 4, top - 6}, {cx - 14, top - 16}}, 9, rgb(255, 140, 50));
      cairo_move_to(cr, sx, sy);
      cairo_line_to(cr, cx + 4, top - 6);
      cairo_line_to(cr, cx - 14, top - 16);
      stroke(cr, kHot, 3.0, 0.9);
      fireballAt(cr, cx - 18, top - 20, 7 + glowK * 2);
      break;
    }
    case 2:
      // Flung forward, hand open.
      limb(cr, {{sx, sy}, {sx + 18, sy - 4}, {sx + 32, sy - 10}}, 9, rgb(255, 120, 40));
      for (int k = -1; k <= 1; ++k)
      {
        cairo_move_to(cr, sx + 32, sy - 10);
        cairo_line_to(cr, sx + 40, sy - 12 + k * 5);
        stroke(cr, kInk, 3.4);
      }
      break;
    case 3:
      limb(cr, {{sx, sy}, {sx + 10, top + 6}, {sx + 6, top - 8}}, 8, kCrustHi);
      break;
    case 4:
    {
      // Holding a stick out over the vent, a marshmallow on its end.
      limb(cr, {{sx - 4, sy + 8}, {sx + 10, sy + 20}, {sx + 20, sy + 14}}, 8, kCrustHi);
      cairo_move_to(cr, sx + 18, sy + 16);
      cairo_line_to(cr, 92, 44);
      stroke(cr, rgb(60, 36, 18), 3.2);
      cairo_move_to(cr, sx + 18, sy + 16);
      cairo_line_to(cr, 92, 44);
      stroke(cr, rgb(170, 120, 70), 1.6);
      cairo_save(cr);
      cairo_translate(cr, 92, 44);
      cairo_rotate(cr, -0.5);
      cairo_rectangle(cr, -5, -6, 10, 12);
      vgrad(cr, -6, 6, {{0.0, rgb(255, 250, 240)}, {0.6, rgb(240, 220, 190)}, {1.0, rgb(170, 100, 40)}}, 1.0, true);
      stroke(cr, kInk, 1.2);
      cairo_restore(cr);
      break;
    }
    default:
      limb(cr, {{sx, sy}, {sx + 10, sy + 16}, {sx + 8, sy + 32}}, 8, kCrustHi);
      break;
  }
  cairo_restore(cr);
}

// --- Fan Bot ----------------------------------------------------------------------------------------

// The rotor in the cowl's front ellipse (centre fx, fy; radii rx, ry), its
// four blades at `angle`; `smear` > 0 draws ghost copies trailing behind.
void rotor(cairo_t* cr, double fx, double fy, double rx, double ry, double angle, int variant)
{
  cairo_save(cr);
  cairo_translate(cr, fx, fy);
  cairo_scale(cr, rx, ry);
  // The duct's dark inside.
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_pattern_t* g = cairo_pattern_create_radial(0, 0, 0, 0, 0, 1);
  stop(g, 0.0, rgb(60, 66, 80));
  stop(g, 1.0, rgb(10, 10, 16));
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
  if (variant == 2)
  {
    // Blowing: a blur of a disk, streaked round.
    cairo_arc(cr, 0, 0, 0.92, 0, 2 * kPi);
    setRgba(cr, rgb(190, 200, 220), 0.45);
    cairo_fill(cr);
    for (int k = 0; k < 5; ++k)
    {
      const double rr = 0.35 + k * 0.13;
      cairo_arc(cr, 0, 0, rr, angle + k, angle + k + 2.0);
      cairo_set_line_width(cr, 0.04);
      setRgba(cr, rgb(250, 252, 255), 0.6);
      cairo_stroke(cr);
    }
  }
  else
  {
    const int ghosts = variant == 1 ? 3 : 1;
    for (int gIdx = ghosts - 1; gIdx >= 0; --gIdx)
      for (int b = 0; b < 4; ++b)
      {
        const double a = angle + b * kPi * 0.5 - gIdx * 0.28;
        cairo_save(cr);
        cairo_rotate(cr, a);
        cairo_move_to(cr, 0.12, -0.05);
        cairo_curve_to(cr, 0.4, -0.3, 0.8, -0.32, 0.92, -0.12);
        cairo_curve_to(cr, 0.9, 0.06, 0.5, 0.12, 0.12, 0.08);
        cairo_close_path(cr);
        setRgba(cr, gIdx == 0 ? rgb(196, 202, 216) : rgb(170, 180, 200), gIdx == 0 ? 1.0 : 0.3);
        cairo_fill_preserve(cr);
        cairo_set_line_width(cr, 0.03);
        setRgba(cr, kInk, gIdx == 0 ? 0.9 : 0.2);
        cairo_stroke(cr);
        cairo_restore(cr);
      }
  }
  // The hub.
  cairo_arc(cr, 0, 0, 0.16, 0, 2 * kPi);
  setRgba(cr, rgb(230, 180, 50), 1.0);
  cairo_fill_preserve(cr);
  cairo_set_line_width(cr, 0.03);
  setRgba(cr, kInk, 1.0);
  cairo_stroke(cr);
  // The guard: rings and spokes.
  for (const double rr : {0.45, 0.75, 1.0})
  {
    cairo_arc(cr, 0, 0, rr, 0, 2 * kPi);
    cairo_set_line_width(cr, rr > 0.99 ? 0.07 : 0.025);
    setRgba(cr, rr > 0.99 ? kSteelDark : rgb(150, 156, 170), 0.9);
    cairo_stroke(cr);
  }
  for (int k = 0; k < 6; ++k)
  {
    cairo_move_to(cr, 0, 0);
    cairo_line_to(cr, std::cos(k * kPi / 3), std::sin(k * kPi / 3));
  }
  cairo_set_line_width(cr, 0.02);
  setRgba(cr, rgb(150, 156, 170), 0.8);
  cairo_stroke(cr);
  cairo_restore(cr);
}

void fanBot(cairo_t* cr, double w, double h, int variant, int frame)
{
  cairo_save(cr);
  frameBox(cr, w, h, 128, 192);
  // The bracket bolted to the shaft wall.
  cairo_rectangle(cr, 0, 36, 16, 120);
  hgrad(cr, 0, 16, {{0.0, rgb(70, 74, 86)}, {1.0, kSteel}}, 1.0, true);
  stroke(cr, kInk, 2.2);
  for (const double y : {46.0, 90.0, 146.0})
  {
    circle(cr, 8, y, 3);
    setRgba(cr, rgb(200, 204, 214), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.0);
  }
  limb(cr, {{14, 96}, {34, 96}}, 14, kSteelDark);
  // The motor box: its face, two eye lamps, a warning light on top.
  cairo_rectangle(cr, 28, 58, 40, 76);
  hgrad(cr, 28, 68, {{0.0, rgb(150, 156, 170)}, {0.5, kSteel}, {1.0, kSteelDark}}, 1.0, true);
  stroke(cr, kInk, 2.4);
  for (double y = 104; y < 130; y += 6)
  {
    cairo_move_to(cr, 33, y);
    cairo_line_to(cr, 63, y);
    stroke(cr, kInk, 1.6, 0.8);
  }
  const Color eye = variant == 0 ? kLed : (variant == 1 ? rgb(255, 210, 80) : rgb(255, 255, 255));
  for (const double ex : {40.0, 56.0})
  {
    glow(cr, ex, 80, 10, eye, 0.7);
    circle(cr, ex, 80, 4.2);
    setRgba(cr, eye, 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.4);
  }
  cairo_rectangle(cr, 42, 50, 12, 8);
  setRgba(cr, kSteelDark, 1.0);
  cairo_fill(cr);
  const bool warn = variant == 1 && frame % 2 == 0;
  if (warn)
    glow(cr, 48, 46, 18, rgb(255, 50, 30), 0.9);
  ellipse(cr, 48, 46, 7, 6);
  setRgba(cr, warn ? rgb(255, 90, 70) : rgb(120, 30, 26), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.4);
  // The cowl: a short drum, the front ellipse to the right.
  const double fx = 98, fy = 96, rx = 26, ry = 84;
  cairo_move_to(cr, 66, fy - ry * 0.92);
  cairo_line_to(cr, fx, fy - ry);
  cairo_line_to(cr, fx, fy + ry);
  cairo_line_to(cr, 66, fy + ry * 0.92);
  cairo_curve_to(cr, 56, fy + ry * 0.6, 56, fy - ry * 0.6, 66, fy - ry * 0.92);
  cairo_close_path(cr);
  vgrad(cr, fy - ry, fy + ry, {{0.0, rgb(170, 176, 190)}, {0.3, kSteel}, {1.0, rgb(40, 42, 52)}}, 1.0, true);
  stroke(cr, kInk, 2.4);
  // Hazard bands round the drum.
  for (const double y : {fy - ry * 0.55, fy + ry * 0.5})
  {
    cairo_move_to(cr, 60, y);
    cairo_line_to(cr, fx, y + (y < fy ? -4 : 4));
    stroke(cr, rgb(240, 190, 40), 5.0, 0.9);
  }
  // A hot rim from below.
  cairo_move_to(cr, 64, fy + ry * 0.9);
  cairo_line_to(cr, fx, fy + ry - 1);
  stroke(cr, rgb(255, 150, 60), 1.6, 0.7);
  rotor(cr, fx, fy, rx, ry, (frame % 4) * kPi / 8.0, variant);
  ellipse(cr, fx, fy, rx, ry);
  stroke(cr, kInk, 2.6);
  // Blowing: the louvres open, wind lines leaving.
  if (variant == 2)
    for (int k = 0; k < 5; ++k)
    {
      const double y = fy - 60 + k * 30;
      cairo_move_to(cr, fx + 4, y);
      cairo_line_to(cr, fx + 22, y + (k - 2) * 3);
      stroke(cr, kInk, 4.0);
      cairo_move_to(cr, fx + 4, y);
      cairo_line_to(cr, fx + 22, y + (k - 2) * 3);
      stroke(cr, rgb(190, 196, 210), 2.0);
    }
  cairo_restore(cr);
}

// --- The duck's heat suit -----------------------------------------------------------------------------

void duckHeatSuit(cairo_t* cr, double w, double h, int variant)
{
  if (variant != 1)
    return;
  cairo_save(cr);
  cairo_translate(cr, kM, kM);
  cairo_scale(cr, w / 64.0, h / 64.0);
  const Color foil = rgb(214, 218, 226);
  // The cape on its shoulders.
  cairo_move_to(cr, 26, 28);
  cairo_curve_to(cr, 22, 34, 22, 40, 26, 42);
  cairo_curve_to(cr, 36, 44, 48, 42, 54, 36);
  cairo_curve_to(cr, 52, 32, 50, 29, 48, 28);
  cairo_close_path(cr);
  vgrad(cr, 28, 44, {{0.0, lighten(foil, 0.3f)}, {0.5, foil}, {1.0, rgb(130, 136, 150)}}, 1.0, true);
  stroke(cr, rgb(50, 52, 62), 1.1);
  // The hood: a domed aluminized cowl over the head.
  cairo_move_to(cr, 30, 30);
  cairo_curve_to(cr, 26, 18, 28, 2, 42, 1);
  cairo_curve_to(cr, 52, 1, 55, 10, 54, 18);
  cairo_curve_to(cr, 54, 24, 52, 28, 50, 30);
  cairo_close_path(cr);
  vgrad(cr, 1, 30, {{0.0, rgb(250, 252, 255)}, {0.4, foil}, {1.0, rgb(140, 146, 160)}}, 1.0, true);
  stroke(cr, rgb(50, 52, 62), 1.2);
  // Crinkles in the foil.
  for (const auto& c : {std::array<double, 4>{33, 8, 36, 20}, {31, 22, 35, 27}, {46, 4, 49, 8}})
  {
    cairo_move_to(cr, c[0], c[1]);
    cairo_line_to(cr, (c[0] + c[2]) * 0.5 + 1.5, (c[1] + c[3]) * 0.5);
    cairo_line_to(cr, c[2], c[3]);
    stroke(cr, rgb(255, 255, 255), 0.8, 0.8);
  }
  // The gold visor.
  cairo_move_to(cr, 39, 12);
  cairo_curve_to(cr, 44, 11, 50, 11, 54, 13);
  cairo_curve_to(cr, 55, 18, 54, 22, 52, 24);
  cairo_curve_to(cr, 47, 25, 42, 25, 39, 23);
  cairo_close_path(cr);
  vgrad(cr, 11, 25, {{0.0, rgb(255, 230, 140)}, {0.5, rgb(220, 160, 40)}, {1.0, rgb(150, 90, 20)}}, 1.0, true);
  stroke(cr, rgb(50, 40, 20), 1.1);
  // The glow of the volcano in the visor.
  cairo_move_to(cr, 41, 21);
  cairo_curve_to(cr, 45, 22, 49, 22, 52, 20);
  stroke(cr, rgb(255, 120, 40), 1.4, 0.8);
  cairo_move_to(cr, 42, 14);
  cairo_line_to(cr, 45, 13.5);
  stroke(cr, rgb(255, 255, 255), 1.2, 0.9);
  cairo_restore(cr);
}

} // namespace

bool drawMagma33Art(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame)
{
  (void)t;
  if (key == "firewall")
    firewall(cr, w, h, std::clamp(variant, 0, 2), frame % 4);
  else if (key == "lava_daemon")
    lavaDaemon(cr, w, h, std::clamp(variant, 0, 4), frame % 2);
  else if (key == "fan_bot")
    fanBot(cr, w, h, std::clamp(variant, 0, 2), frame % 4);
  else if (key == "duck_heat_suit")
    duckHeatSuit(cr, w, h, variant);
  else
    return false;
  return true;
}

} // namespace gr
