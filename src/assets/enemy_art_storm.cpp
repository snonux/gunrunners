// Storm Relay (Level 31) in the style of enemy_art.cpp: smooth shapes with
// a dark ink line and a cold rim of storm light along their upper edges so
// they read on a black sky. The Storm Gull is a big herring gull, white and
// grey with black wingtips, a yellow beak with its red spot; the Kite Drone
// a diamond kite of yellow and navy sailcloth on steel spars with a ducted
// rotor at each tip and a camera eye; the Foghorn a big brass horn on a
// wooden stand with a leather bellows behind it, green-crusted on the
// carrier. The beach gull eats its french fry; the rubber duck wears a
// sou'wester. Each is drawn in screen pixels into a texture with a 32 px
// margin, the cell box at (32, 32) .. (32 + w, 32 + h), facing right.

#include "assets/enemy_art_storm.hpp"

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
constexpr Color kInk = rgb(10, 12, 18);
constexpr Color kRim = rgb(190, 215, 255);
constexpr Color kWhite = rgb(242, 244, 248);
constexpr Color kWhiteShade = rgb(160, 172, 192);
constexpr Color kGullGrey = rgb(128, 140, 158);
constexpr Color kTip = rgb(22, 24, 30);
constexpr Color kBeak = rgb(244, 200, 60);
constexpr Color kBrass = rgb(226, 178, 80);

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

// --- The Storm Gull ------------------------------------------------------------------------------

// A gull's wing from the shoulder (sx, sy): `lift` -1 (down) .. 1 (up),
// `fold` 0 (spread) .. 1 (swept back along the body); the near wing is
// lighter, the far one in shade.
void gullWing(cairo_t* cr, double sx, double sy, double lift, double fold, bool near)
{
  const double span = 70.0 * (1.0 - 0.55 * fold);
  const double ang = -lift * 1.15 + fold * 0.9; // radians below the horizontal-back line
  // The wing runs back (to the left) and up/down from the shoulder.
  const double ex = sx - std::cos(ang * 0.6) * span * (0.55 + 0.45 * fold) - fold * 20.0;
  const double ey = sy - std::sin(-ang) * span * 0.85 * (1.0 - fold * 0.6);
  const double mx = (sx + ex) * 0.5 + (lift > 0 ? -6.0 : 6.0) * (1.0 - fold), my = (sy + ey) * 0.5 - 8.0 * (1.0 - fold);
  const Color top = near ? kGullGrey : darken(kGullGrey, 0.3f);
  // The wing's body: a long tapering blade.
  cairo_move_to(cr, sx + 6, sy - 4);
  cairo_curve_to(cr, mx + 8, my - 10, ex + 18, ey - 6, ex, ey);
  cairo_curve_to(cr, ex + 14, ey + 10, mx + 4, my + 14, sx - 10, sy + 8);
  cairo_close_path(cr);
  vgrad(cr, std::min(sy, ey) - 10, std::max(sy, ey) + 10, {{0.0, lighten(top, 0.15f)}, {1.0, darken(top, 0.2f)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  // The black tip with its white mirror spots.
  const double tx = ex + (mx - ex) * 0.35, ty = ey + (my - ey) * 0.35;
  cairo_move_to(cr, tx + 2, ty - 6);
  cairo_curve_to(cr, (tx + ex) * 0.5 + 4, (ty + ey) * 0.5 - 6, ex + 6, ey - 3, ex, ey);
  cairo_curve_to(cr, ex + 8, ey + 6, (tx + ex) * 0.5 + 4, (ty + ey) * 0.5 + 6, tx + 2, ty + 7);
  cairo_close_path(cr);
  setRgba(cr, near ? kTip : darken(kTip, 0.3f), 1.0);
  cairo_fill(cr);
  circle(cr, (tx + ex) * 0.5 + 1, (ty + ey) * 0.5, 2.2);
  setRgba(cr, kWhite, near ? 0.95 : 0.6);
  cairo_fill(cr);
  // The trailing edge's white line and a cold rim along the leading edge.
  if (near)
  {
    cairo_move_to(cr, sx + 6, sy - 4);
    cairo_curve_to(cr, mx + 8, my - 10, ex + 18, ey - 6, ex + 2, ey - 1);
    stroke(cr, kRim, 1.4, 0.55);
  }
}

// The gull's body, head and tail. `cry` 0..1 throws the head back with the
// beak open; `dive` tucks it all into a dart.
void gullBody(cairo_t* cr, double cry, bool dive)
{
  // The tail: a short white fan on the left.
  cairo_move_to(cr, 34, 50);
  cairo_line_to(cr, 8, dive ? 48 : 42);
  cairo_line_to(cr, 8, dive ? 56 : 60);
  cairo_close_path(cr);
  vgrad(cr, 40, 60, {{0.0, kWhite}, {1.0, kWhiteShade}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  // The body.
  cairo_move_to(cr, 26, 52);
  cairo_curve_to(cr, 34, 34, 70, 32, 88, 38);
  cairo_curve_to(cr, 100, 44, 98, 62, 82, 66);
  cairo_curve_to(cr, 60, 72, 36, 64, 26, 52);
  cairo_close_path(cr);
  vgrad(cr, 32, 70, {{0.0, kWhite}, {0.6, kWhite}, {1.0, kWhiteShade}}, 1.0, true);
  stroke(cr, kInk, 2.2);
  // The grey mantle over its back.
  cairo_move_to(cr, 30, 48);
  cairo_curve_to(cr, 40, 36, 66, 33, 84, 39);
  cairo_curve_to(cr, 70, 44, 50, 48, 30, 48);
  cairo_close_path(cr);
  setRgba(cr, kGullGrey, 1.0);
  cairo_fill(cr);
  // Feet tucked under (pink).
  if (!dive)
  {
    cairo_move_to(cr, 52, 66);
    cairo_line_to(cr, 44, 72);
    cairo_line_to(cr, 50, 73);
    cairo_close_path(cr);
    setRgba(cr, rgb(232, 150, 140), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.2);
  }
  // The head, thrown back by the cry.
  const double hx = 96.0 - cry * 4.0, hy = 36.0 - cry * 6.0;
  const double tilt = -cry * 0.55;
  cairo_save(cr);
  cairo_translate(cr, hx, hy);
  cairo_rotate(cr, tilt);
  // The neck joins it to the body.
  cairo_move_to(cr, -18, 2);
  cairo_curve_to(cr, -14, -8, -4, -16, 6, -14);
  cairo_curve_to(cr, 18, -12, 20, 4, 10, 10);
  cairo_curve_to(cr, 0, 16, -12, 14, -18, 2);
  cairo_close_path(cr);
  vgrad(cr, -16, 14, {{0.0, kWhite}, {1.0, rgb(214, 220, 230)}}, 1.0, true);
  stroke(cr, kInk, 2.2);
  // The beak: upper and lower mandibles, opening with the cry.
  const double gape = cry * 0.5;
  cairo_save(cr);
  cairo_translate(cr, 14, -2);
  cairo_save(cr);
  cairo_rotate(cr, -gape * 0.6);
  cairo_move_to(cr, 0, -4);
  cairo_curve_to(cr, 8, -5, 18, -3, 22, 1);
  cairo_curve_to(cr, 20, 3, 14, 2, 0, 2);
  cairo_close_path(cr);
  vgrad(cr, -5, 3, {{0.0, lighten(kBeak, 0.2f)}, {1.0, darken(kBeak, 0.15f)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  cairo_restore(cr);
  cairo_save(cr);
  cairo_rotate(cr, gape);
  cairo_move_to(cr, 0, 2);
  cairo_line_to(cr, 17, 2);
  cairo_curve_to(cr, 18, 5, 16, 7, 12, 6);
  cairo_line_to(cr, 0, 5);
  cairo_close_path(cr);
  setRgba(cr, kBeak, 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.4);
  circle(cr, 13, 4.2, 1.8); // the red spot
  setRgba(cr, rgb(214, 40, 34), 1.0);
  cairo_fill(cr);
  cairo_restore(cr);
  if (cry > 0.3)
  {
    // The red gape inside.
    cairo_move_to(cr, 1, 1.5);
    cairo_line_to(cr, 14, -6 * cry);
    cairo_line_to(cr, 14, 6 * cry + 2);
    cairo_close_path(cr);
    setRgba(cr, rgb(200, 50, 60), 0.9);
    cairo_fill(cr);
  }
  cairo_restore(cr);
  // The eye: pale, a hard pupil, a red ring; a frown above it.
  circle(cr, 6, -5, 3.4);
  setRgba(cr, rgb(250, 236, 170), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, rgb(200, 40, 30), 1.2);
  circle(cr, 6.8, -5, 1.5);
  setRgba(cr, kInk, 1.0);
  cairo_fill(cr);
  cairo_move_to(cr, 1, -10);
  cairo_line_to(cr, 11, -8);
  stroke(cr, kInk, 2.0);
  // Storm light along the crown.
  cairo_move_to(cr, -12, -9);
  cairo_curve_to(cr, -6, -15, 4, -16, 10, -13);
  stroke(cr, kRim, 1.4, 0.6);
  cairo_restore(cr);
  // Rim of light along the back.
  cairo_move_to(cr, 32, 44);
  cairo_curve_to(cr, 44, 35, 66, 33, 84, 37);
  stroke(cr, kRim, 1.4, 0.5);
}

void stormGull(cairo_t* cr, double w, double h, int variant, int frame)
{
  cairo_save(cr);
  cairo_translate(cr, kM, kM);
  cairo_scale(cr, w / 128.0, h / 96.0);
  if (variant == 2)
  {
    // The dive: wings swept back tight, a dart.
    gullWing(cr, 58, 46, 0.3, 1.0, false);
    gullBody(cr, 0.0, true);
    gullWing(cr, 62, 48, 0.1, 1.0, true);
    // Speed lines off the wings.
    for (int k = 0; k < 3; ++k)
    {
      cairo_move_to(cr, 6 - k * 4, 38 + k * 10);
      cairo_line_to(cr, -18 - k * 6, 38 + k * 10);
      stroke(cr, kRim, 1.6, 0.45 - k * 0.1);
    }
  }
  else if (variant == 1)
  {
    // The cry: wings folded up over its back in a W, head thrown back.
    const double shake = frame % 2 ? 1.5 : -1.5;
    cairo_translate(cr, shake, 0);
    gullWing(cr, 58, 46, 0.95, 0.45, false);
    gullBody(cr, 1.0, false);
    gullWing(cr, 62, 48, 0.85, 0.45, true);
  }
  else
  {
    static constexpr std::array<double, 4> kLift{{0.9, 0.2, -0.7, 0.2}};
    const double lift = kLift[std::size_t(frame % 4)];
    gullWing(cr, 58, 46, lift * 0.9 + 0.15, 0.0, false);
    gullBody(cr, 0.0, false);
    gullWing(cr, 62, 48, lift, 0.0, true);
  }
  cairo_restore(cr);
}

// The beach gull: smaller, standing on its post with a fry, or flapping
// off with it. 96 x 96 design.
void fryGull(cairo_t* cr, double w, double h, int variant, int frame)
{
  cairo_save(cr);
  cairo_translate(cr, kM, kM);
  cairo_scale(cr, w / 96.0, h / 96.0);
  if (variant == 1)
  {
    // Off with the fry.
    cairo_translate(cr, -10, 6);
    cairo_scale(cr, 0.85, 0.85);
    const double lift = frame % 2 ? 0.9 : -0.6;
    gullWing(cr, 58, 46, lift * 0.9 + 0.15, 0.0, false);
    gullBody(cr, 0.0, false);
    gullWing(cr, 62, 48, lift, 0.0, true);
    // The fry in its beak.
    cairo_move_to(cr, 112, 36);
    cairo_line_to(cr, 128, 44);
    stroke(cr, rgb(120, 70, 20), 6.0);
    cairo_move_to(cr, 112, 36);
    cairo_line_to(cr, 128, 44);
    stroke(cr, rgb(250, 206, 80), 4.0);
    cairo_restore(cr);
    return;
  }
  // The fries' carton at its feet (on the post top, 0..96 bottom).
  cairo_move_to(cr, 66, 96);
  cairo_line_to(cr, 64, 80);
  cairo_line_to(cr, 86, 80);
  cairo_line_to(cr, 84, 96);
  cairo_close_path(cr);
  vgrad(cr, 80, 96, {{0.0, rgb(230, 50, 50)}, {1.0, rgb(150, 20, 30)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  for (int k = 0; k < 5; ++k)
  {
    cairo_move_to(cr, 67 + k * 4, 80);
    cairo_line_to(cr, 66 + k * 4.5 + (k % 2), 70 + (k % 3) * 2);
    stroke(cr, rgb(120, 70, 20), 4.0);
    cairo_move_to(cr, 67 + k * 4, 80);
    cairo_line_to(cr, 66 + k * 4.5 + (k % 2), 70 + (k % 3) * 2);
    stroke(cr, rgb(250, 210, 90), 2.4);
  }
  // Legs.
  for (const double x : {34.0, 44.0})
  {
    cairo_move_to(cr, x, 70);
    cairo_line_to(cr, x - 1, 94);
    stroke(cr, rgb(232, 150, 140), 3.0);
    cairo_move_to(cr, x - 6, 95);
    cairo_line_to(cr, x + 5, 95);
    stroke(cr, rgb(232, 150, 140), 2.6);
  }
  // Standing: the body upright-ish, wings folded along it.
  const double head = frame == 1 ? 1.0 : (frame == 3 ? -1.0 : 0.0);
  cairo_save(cr);
  cairo_translate(cr, 40, 60);
  cairo_rotate(cr, -0.35);
  cairo_scale(cr, 0.72, 0.72);
  cairo_translate(cr, -60, -52);
  gullWing(cr, 58, 46, 0.25, 1.0, false);
  gullBody(cr, std::max(0.0, head) * 0.6, false);
  gullWing(cr, 62, 48, 0.2, 1.0, true);
  cairo_restore(cr);
  // The fry: in its beak (frames 0, 1), going down (2), or being picked.
  if (frame <= 2)
  {
    const double bx = frame == 1 ? 74 : 80, by = frame == 1 ? 10 : 22;
    const double len = frame == 2 ? 5.0 : 14.0;
    const double a = frame == 1 ? -1.1 : -0.25;
    cairo_move_to(cr, bx, by);
    cairo_line_to(cr, bx + std::cos(a) * len, by + std::sin(a) * len);
    stroke(cr, rgb(120, 70, 20), 5.0);
    cairo_move_to(cr, bx, by);
    cairo_line_to(cr, bx + std::cos(a) * len, by + std::sin(a) * len);
    stroke(cr, rgb(250, 210, 90), 3.2);
  }
  cairo_restore(cr);
}

// --- The Kite Drone ------------------------------------------------------------------------------

void kiteDrone(cairo_t* cr, double w, double h, int variant, int frame)
{
  cairo_save(cr);
  cairo_translate(cr, kM, kM);
  cairo_scale(cr, w / 128.0, h / 128.0);
  const bool tell = variant == 1, struck = variant == 2;
  // The ribbon tail from the bottom point, fluttering.
  {
    const double f = frame % 2 ? 1.0 : -1.0;
    cairo_move_to(cr, 64, 108);
    cairo_curve_to(cr, 64 + 10 * f, 116, 64 - 12 * f, 122, 64 + 6 * f, 130);
    stroke(cr, rgb(30, 34, 50), 2.0);
    for (int k = 0; k < 2; ++k)
    {
      const double by = 116 + k * 9, bx = 64 + (k ? -6 : 7) * f;
      cairo_move_to(cr, bx - 6, by - 3);
      cairo_line_to(cr, bx + 6, by + 3);
      cairo_line_to(cr, bx + 6, by - 3);
      cairo_line_to(cr, bx - 6, by + 3);
      cairo_close_path(cr);
      setRgba(cr, struck ? rgb(90, 80, 60) : rgb(250, 200, 50), 1.0);
      cairo_fill_preserve(cr);
      stroke(cr, kInk, 1.0);
    }
  }
  // The sails: four panels of a diamond, two yellow, two navy.
  const std::array<std::pair<double, double>, 4> pt{{{64, 8}, {116, 54}, {64, 110}, {12, 54}}};
  const std::pair<double, double> mid{64, 54};
  for (int i = 0; i < 4; ++i)
  {
    const auto a = pt[std::size_t(i)], b = pt[std::size_t((i + 1) % 4)];
    cairo_move_to(cr, mid.first, mid.second);
    cairo_line_to(cr, a.first, a.second);
    // A little billow in the cloth.
    const double cx = (a.first + b.first) * 0.5 + (mid.first - (a.first + b.first) * 0.5) * -0.08;
    const double cy = (a.second + b.second) * 0.5 + (mid.second - (a.second + b.second) * 0.5) * -0.08;
    cairo_curve_to(cr, cx, cy, cx, cy, b.first, b.second);
    cairo_close_path(cr);
    Color cloth = i % 2 == 0 ? rgb(244, 196, 48) : rgb(34, 46, 86);
    if (struck)
      cloth = darken(cloth, 0.55f);
    vgrad(cr, 8, 110, {{0.0, lighten(cloth, 0.15f)}, {1.0, darken(cloth, 0.25f)}}, 1.0, true);
    stroke(cr, kInk, 2.0);
  }
  if (struck)
  {
    // Scorch marks and a hole burnt through.
    for (const auto& s : {std::array<double, 3>{40, 40, 10}, std::array<double, 3>{86, 70, 8}})
    {
      glow(cr, s[0], s[1], s[2] * 1.4, rgb(10, 8, 6), 0.8);
      circle(cr, s[0], s[1], s[2] * 0.35);
      setRgba(cr, rgb(255, 170, 60), 0.9);
      cairo_fill(cr);
    }
  }
  // The steel spars: the cross, rusted at the joints.
  for (const auto& s : {std::array<double, 4>{64, 6, 64, 112}, std::array<double, 4>{10, 54, 118, 54}})
  {
    cairo_move_to(cr, s[0], s[1]);
    cairo_line_to(cr, s[2], s[3]);
    stroke(cr, kInk, 5.0);
    cairo_move_to(cr, s[0], s[1]);
    cairo_line_to(cr, s[2], s[3]);
    stroke(cr, rgb(170, 176, 188), 2.6);
  }
  // Rim of storm light on the upper edges.
  cairo_move_to(cr, 14, 52);
  cairo_line_to(cr, 64, 9);
  cairo_line_to(cr, 114, 52);
  stroke(cr, kRim, 1.5, 0.55);
  // The ducted rotors at the wing tips: a ring and a blur of blades.
  for (const double rx : {12.0, 116.0})
  {
    const double ry = 46.0;
    ellipse(cr, rx, ry, 17, 6);
    setRgba(cr, rgb(40, 44, 54), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.8);
    // The blur: brighter and wider on the tell.
    const double spin = tell ? 1.0 : 0.55;
    ellipse(cr, rx, ry - 4, 22 + 4 * spin, 4.5);
    setRgba(cr, rgb(210, 220, 236), 0.22 + 0.25 * spin);
    cairo_fill(cr);
    const double bl = frame % 2 ? 1.0 : -1.0;
    cairo_move_to(cr, rx - 20 * bl, ry - 5);
    cairo_line_to(cr, rx + 20 * bl, ry - 3);
    stroke(cr, rgb(230, 236, 246), 2.0, 0.5);
    // The duct's rim and the motor.
    ellipse(cr, rx, ry - 4, 24, 6);
    stroke(cr, rgb(150, 156, 170), 2.0);
    cairo_rectangle(cr, rx - 4, ry - 4, 8, 12);
    setRgba(cr, rgb(70, 74, 86), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.2);
    if (tell)
      for (int k = 0; k < 3; ++k)
      {
        // Whine lines off the rotors.
        cairo_arc(cr, rx, ry - 4, 28 + k * 6, -kPi * 0.85, -kPi * 0.15);
        stroke(cr, rgb(255, 210, 160), 1.4, 0.5 - k * 0.14);
      }
  }
  // The camera pod in the middle, its eye looking ahead.
  circle(cr, 64, 56, 13);
  vgrad(cr, 43, 69, {{0.0, rgb(120, 126, 140)}, {1.0, rgb(30, 32, 40)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  const Color eye = struck ? rgb(255, 255, 255) : (tell ? rgb(255, 50, 40) : rgb(255, 180, 50));
  glow(cr, 69, 56, tell ? 16 : 10, eye, tell ? 0.8 : 0.5);
  circle(cr, 69, 56, 5.5);
  setRgba(cr, darken(eye, 0.3f), 1.0);
  cairo_fill(cr);
  circle(cr, 69, 56, 2.6);
  setRgba(cr, lighten(eye, 0.6f), 1.0);
  cairo_fill(cr);
  circle(cr, 66.5, 53.5, 1.4);
  setRgba(cr, rgb(255, 255, 255), 0.9);
  cairo_fill(cr);
  // The tether ring at the bottom point.
  circle(cr, 64, 110, 4);
  stroke(cr, rgb(170, 176, 188), 2.0);
  cairo_restore(cr);
}

// --- The Foghorn ---------------------------------------------------------------------------------

void foghorn(cairo_t* cr, double w, double h, int variant, int frame)
{
  cairo_save(cr);
  cairo_translate(cr, kM, kM);
  cairo_scale(cr, w / 192.0, h / 192.0);
  const bool carrier = variant >= 4;
  const int v = variant % 4;
  const double swell = v == 1 ? 0.55 : (v == 2 ? 1.0 : (v == 3 ? -0.6 : 0.0));
  const double shake = (v == 2 || v == 3) ? (frame % 2 ? 1.6 : -1.6) : 0.0;
  const Color brass = carrier ? lerpColor(kBrass, rgb(120, 190, 110), 0.35f) : kBrass;
  // The stand: a wooden trestle, two legs splayed, a cross bar.
  for (const auto& leg : {std::array<double, 4>{60, 132, 34, 190}, std::array<double, 4>{120, 132, 146, 190},
         std::array<double, 4>{90, 132, 90, 190}})
  {
    cairo_move_to(cr, leg[0], leg[1]);
    cairo_line_to(cr, leg[2], leg[3]);
    stroke(cr, kInk, 11.0);
    cairo_move_to(cr, leg[0], leg[1]);
    cairo_line_to(cr, leg[2], leg[3]);
    stroke(cr, rgb(120, 82, 50), 7.0);
  }
  cairo_move_to(cr, 46, 168);
  cairo_line_to(cr, 136, 168);
  stroke(cr, kInk, 8.0);
  cairo_move_to(cr, 46, 168);
  cairo_line_to(cr, 136, 168);
  stroke(cr, rgb(140, 98, 60), 4.5);
  // The platform the horn sits on.
  cairo_rectangle(cr, 30, 124, 120, 12);
  vgrad(cr, 124, 136, {{0.0, rgb(170, 122, 76)}, {1.0, rgb(90, 58, 34)}}, 1.0, true);
  stroke(cr, kInk, 2.2);
  cairo_save(cr);
  cairo_translate(cr, shake, 0);
  // The bellows: a leather accordion between two boards, swelling on the
  // tell and squeezed in the blast.
  const double bTop = 74.0 - swell * 22.0, bBot = 122.0;
  const double bx0 = 22.0, bx1 = 84.0;
  cairo_rectangle(cr, bx0, bTop - 8, bx1 - bx0, 8);
  vgrad(cr, bTop - 8, bTop, {{0.0, rgb(160, 112, 70)}, {1.0, rgb(96, 62, 36)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  const int pleats = 5;
  for (int i = 0; i < pleats; ++i)
  {
    const double y0 = bTop + (bBot - bTop) * i / pleats, y1 = bTop + (bBot - bTop) * (i + 1) / pleats;
    const double bulge = 6.0 + 6.0 * std::max(0.0, swell);
    cairo_move_to(cr, bx0 + 4, y0);
    cairo_line_to(cr, bx1 - 4, y0);
    cairo_line_to(cr, bx1 - 4 + bulge, (y0 + y1) * 0.5);
    cairo_line_to(cr, bx1 - 4, y1);
    cairo_line_to(cr, bx0 + 4, y1);
    cairo_line_to(cr, bx0 + 4 - bulge, (y0 + y1) * 0.5);
    cairo_close_path(cr);
    Color leather = i % 2 ? rgb(92, 50, 34) : rgb(120, 66, 44);
    if (carrier)
      leather = lerpColor(leather, rgb(60, 110, 60), 0.3f);
    vgrad(cr, y0, y1, {{0.0, lighten(leather, 0.15f)}, {1.0, darken(leather, 0.25f)}}, 1.0, true);
    stroke(cr, kInk, 1.6);
  }
  // The handle on top.
  cairo_move_to(cr, 36, bTop - 8);
  cairo_curve_to(cr, 40, bTop - 26, 66, bTop - 26, 70, bTop - 8);
  stroke(cr, kInk, 6.0);
  cairo_move_to(cr, 36, bTop - 8);
  cairo_curve_to(cr, 40, bTop - 26, 66, bTop - 26, 70, bTop - 8);
  stroke(cr, rgb(140, 100, 60), 3.0);
  // The horn: a brass tube flaring out to a big bell on the right.
  const double cy = 92.0;
  cairo_move_to(cr, 82, cy - 9);
  cairo_curve_to(cr, 120, cy - 10, 140, cy - 22, 150, cy - 48);
  cairo_line_to(cr, 162, cy - 52);
  cairo_line_to(cr, 162, cy + 52);
  cairo_line_to(cr, 150, cy + 48);
  cairo_curve_to(cr, 140, cy + 22, 120, cy + 10, 82, cy + 9);
  cairo_close_path(cr);
  vgrad(cr, cy - 52, cy + 52, {{0.0, lighten(brass, 0.35f)}, {0.3, brass}, {0.7, darken(brass, 0.3f)}, {1.0, darken(brass, 0.55f)}}, 1.0,
    true);
  stroke(cr, kInk, 2.4);
  // Bands round the tube.
  for (const double x : {98.0, 120.0})
  {
    const double hh = x < 110 ? 10.0 : 14.0;
    cairo_rectangle(cr, x - 3, cy - hh, 6, hh * 2);
    vgrad(cr, cy - hh, cy + hh, {{0.0, rgb(250, 230, 170)}, {1.0, rgb(130, 90, 30)}}, 1.0, true);
    stroke(cr, kInk, 1.2);
  }
  // The bell's mouth, seen a little from the side.
  ellipse(cr, 162, cy, 14, 54);
  vgrad(cr, cy - 54, cy + 54, {{0.0, darken(brass, 0.2f)}, {1.0, darken(brass, 0.6f)}}, 1.0, true);
  stroke(cr, kInk, 2.4);
  ellipse(cr, 164, cy, 8, 40);
  setRgba(cr, rgb(30, 20, 10), 0.9);
  cairo_fill(cr);
  // The shine along its top, and storm light on the rim.
  cairo_move_to(cr, 90, cy - 6);
  cairo_curve_to(cr, 122, cy - 7, 140, cy - 18, 148, cy - 42);
  stroke(cr, rgb(255, 250, 220), 2.2, 0.7);
  cairo_arc(cr, 162, cy, 54, -kPi * 0.5, -kPi * 0.1);
  stroke(cr, kRim, 1.6, 0.5);
  // Verdigris, and on the carrier a green crust dripping salt spray.
  Rng rng(31111u);
  for (int i = 0; i < (carrier ? 14 : 5); ++i)
  {
    const double x = rng.range(100, 158), y = cy + rng.range(-36, 36) * (x - 90) / 70.0;
    glow(cr, x, y, rng.range(3, 7), carrier ? rgb(120, 255, 110) : rgb(90, 170, 150), carrier ? 0.65 : 0.45);
  }
  if (carrier)
    for (int i = 0; i < 4; ++i)
    {
      const double x = 150 + i * 4.0;
      cairo_move_to(cr, x, cy + 44 - i * 3);
      cairo_curve_to(cr, x + 1, cy + 52, x - 1, cy + 58, x, cy + 62 + i * 2);
      stroke(cr, rgb(130, 255, 110), 2.2, 0.75);
    }
  // The blast: the bell's lip a blur.
  if (v == 3)
  {
    ellipse(cr, 166, cy, 18, 60);
    stroke(cr, rgb(255, 240, 200), 2.0, 0.45);
  }
  cairo_restore(cr);
  cairo_restore(cr);
}

// --- The duck's sou'wester -------------------------------------------------------------------------

void duckSouWester(cairo_t* cr, double w, double h, int variant)
{
  if (variant != 1)
    return;
  cairo_save(cr);
  cairo_translate(cr, kM, kM);
  cairo_scale(cr, w / 64.0, h / 64.0);
  const Color oil = rgb(250, 206, 40);
  // The strap under its chin.
  cairo_move_to(cr, 36, 14);
  cairo_curve_to(cr, 34, 22, 38, 30, 46, 30);
  stroke(cr, rgb(40, 30, 10), 2.6);
  cairo_move_to(cr, 36, 14);
  cairo_curve_to(cr, 34, 22, 38, 30, 46, 30);
  stroke(cr, rgb(200, 150, 30), 1.4);
  // The brim: short over its brow, long and turned down at the back.
  cairo_move_to(cr, 56, 11);
  cairo_curve_to(cr, 50, 15, 38, 15, 30, 16);
  cairo_curve_to(cr, 24, 18, 20, 24, 18, 27);
  cairo_curve_to(cr, 22, 20, 26, 12, 32, 9);
  cairo_curve_to(cr, 42, 8, 52, 7, 56, 11);
  cairo_close_path(cr);
  vgrad(cr, 7, 27, {{0.0, lighten(oil, 0.2f)}, {1.0, darken(oil, 0.25f)}}, 1.0, true);
  stroke(cr, rgb(60, 40, 10), 1.2);
  // The crown: a soft dome with seams.
  cairo_move_to(cr, 33, 10);
  cairo_curve_to(cr, 33, 0, 51, -1, 51, 9);
  cairo_curve_to(cr, 45, 11, 39, 11, 33, 10);
  cairo_close_path(cr);
  vgrad(cr, -1, 11, {{0.0, lighten(oil, 0.3f)}, {1.0, darken(oil, 0.15f)}}, 1.0, true);
  stroke(cr, rgb(60, 40, 10), 1.2);
  cairo_move_to(cr, 42, 0.5);
  cairo_line_to(cr, 42, 10);
  stroke(cr, rgb(180, 130, 20), 0.8, 0.8);
  // A gleam of rain on it.
  cairo_move_to(cr, 37, 4);
  cairo_curve_to(cr, 39, 2, 43, 1.5, 46, 2.5);
  stroke(cr, rgb(255, 255, 255), 1.2, 0.8);
  cairo_restore(cr);
}

} // namespace

bool drawStormArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame)
{
  (void)t;
  if (key == "storm_gull")
    stormGull(cr, w, h, std::clamp(variant, 0, 2), frame % 4);
  else if (key == "fry_gull")
    fryGull(cr, w, h, variant, frame % 4);
  else if (key == "kite_drone")
    kiteDrone(cr, w, h, std::clamp(variant, 0, 2), frame % 2);
  else if (key == "foghorn")
    foghorn(cr, w, h, std::clamp(variant, 0, 7), frame % 2);
  else if (key == "duck_sou_wester")
    duckSouWester(cr, w, h, variant);
  else
    return false;
  return true;
}

} // namespace gr
