// Dish Array (Level 32) in the style of enemy_art.cpp: smooth shapes with a
// dark ink line and a warm rim of desert sun along their upper edges. The
// Sand Shark is the colour of the dunes, mottled, its back plated like a
// lizard's; the Vulture Drone a gunmetal rotor drone hunched like a vulture
// with ragged sheet-metal feathers, a hooded camera head and talons; the
// Scorpion Mech a walker in sand-yellow site paint with hazard stripes, its
// tail a laser. The rubber duck wears aviator sunglasses. Each is drawn in
// screen pixels into a texture with a 32 px margin, the cell box at (32, 32)
// .. (32 + w, 32 + h), facing right.

#include "assets/enemy_art_dish.hpp"

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
constexpr Color kInk = rgb(30, 18, 12);
constexpr Color kSun = rgb(255, 238, 200);
constexpr Color kSharkBack = rgb(204, 156, 98);
constexpr Color kSharkDark = rgb(150, 104, 62);
constexpr Color kBelly = rgb(246, 232, 204);
constexpr Color kGun = rgb(84, 86, 96);
constexpr Color kGunDark = rgb(38, 38, 46);
constexpr Color kSite = rgb(232, 184, 64);

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

// --- Sand Shark -------------------------------------------------------------------------------------

// The body's outline in its own frame (head to +x, back to -y).
void sharkBody(cairo_t* cr, double tailFlick)
{
  cairo_move_to(cr, 96, -4);
  cairo_curve_to(cr, 84, -22, 50, -30, 10, -28);
  cairo_curve_to(cr, -30, -26, -70, -18, -104, -8 + tailFlick);
  cairo_line_to(cr, -120, -30 + tailFlick * 1.5);
  cairo_curve_to(cr, -118, -6 + tailFlick, -118, 8 + tailFlick, -124, 30 + tailFlick);
  cairo_line_to(cr, -100, 12 + tailFlick);
  cairo_curve_to(cr, -60, 24, 0, 34, 50, 26);
  cairo_curve_to(cr, 76, 20, 92, 10, 96, -4);
  cairo_close_path(cr);
}

void sandShark(cairo_t* cr, double w, double h, int variant, int frame)
{
  cairo_save(cr);
  frameBox(cr, w, h, 192, 128);
  if (variant >= 2)
  {
    // Only the fin, cutting the sand (2), or stopped with the sand mounding
    // up round it (3).
    const bool tell = variant == 3;
    const double tremble = tell ? (frame % 2 ? 1.5 : -1.5) : 0.0;
    if (tell)
    {
      // The mound: sand heaped up over the shark, cracked, grains rolling.
      cairo_move_to(cr, 26, 128);
      cairo_curve_to(cr, 50, 104 + tremble, 140, 102 - tremble, 166, 128);
      cairo_close_path(cr);
      vgrad(cr, 100, 128, {{0.0, rgb(250, 228, 186)}, {1.0, rgb(214, 172, 118)}});
      for (const auto& c : {std::array<double, 4>{60, 116, 74, 110}, std::array<double, 4>{120, 112, 134, 118},
             std::array<double, 4>{90, 108, 98, 116}})
      {
        cairo_move_to(cr, c[0], c[1]);
        cairo_line_to(cr, c[2], c[3]);
        stroke(cr, rgb(170, 120, 70), 1.4, 0.8);
      }
      cairo_move_to(cr, 40, 120);
      cairo_curve_to(cr, 60, 106, 130, 104, 152, 120);
      stroke(cr, rgb(255, 250, 230), 1.4, 0.7);
    }
    const double base = tell ? 112 : 128, top = tell ? 88 : 72;
    cairo_move_to(cr, 70 + tremble, base);
    cairo_curve_to(cr, 74 + tremble, base - 20, 70 + tremble, top + 10, 64 + tremble, top);
    cairo_curve_to(cr, 86 + tremble, top + 6, 104 + tremble, base - 18, 118 + tremble, base);
    cairo_close_path(cr);
    hgrad(cr, 64, 118, {{0.0, lighten(kSharkBack, 0.15f)}, {0.6, kSharkBack}, {1.0, kSharkDark}}, 1.0, true);
    stroke(cr, kInk, 2.4);
    // The fin's leading edge in the sun, a scar notch, mottling.
    cairo_move_to(cr, 66 + tremble, top + 3);
    cairo_curve_to(cr, 70 + tremble, top + 12, 74 + tremble, base - 22, 72 + tremble, base - 4);
    stroke(cr, kSun, 1.6, 0.7);
    cairo_move_to(cr, 92 + tremble, top + 18);
    cairo_line_to(cr, 97 + tremble, top + 22);
    stroke(cr, kInk, 1.4, 0.7);
    for (const auto& s : {std::array<double, 3>{86, base - 12, 3}, std::array<double, 3>{98, base - 6, 2.4}})
    {
      circle(cr, s[0] + tremble, s[1], s[2]);
      setRgba(cr, kSharkDark, 0.6);
      cairo_fill(cr);
    }
    cairo_restore(cr);
    return;
  }
  const bool bite = variant == 1;
  const double flick = frame % 2 ? 6.0 : -4.0;
  // The sand surface cuts it: clip to above the box's foot.
  cairo_rectangle(cr, -40, -60, 280, 188);
  cairo_clip(cr);
  cairo_translate(cr, 96, 96);
  cairo_rotate(cr, -0.42);
  // The pectoral fin, behind.
  cairo_move_to(cr, 26, 22);
  cairo_curve_to(cr, 16, 40, 2, 54, -8, 58);
  cairo_curve_to(cr, 14, 50, 36, 34, 48, 22);
  cairo_close_path(cr);
  vgrad(cr, 20, 58, {{0.0, kSharkBack}, {1.0, kSharkDark}}, 1.0, true);
  stroke(cr, kInk, 2.2);
  // The dorsal fin.
  cairo_move_to(cr, -8, -27);
  cairo_curve_to(cr, -14, -44, -26, -60, -40, -66);
  cairo_curve_to(cr, -18, -60, 10, -40, 22, -28);
  cairo_close_path(cr);
  hgrad(cr, -40, 22, {{0.0, kSharkDark}, {1.0, kSharkBack}}, 1.0, true);
  stroke(cr, kInk, 2.4);
  cairo_move_to(cr, -38, -63);
  cairo_curve_to(cr, -18, -56, 8, -38, 18, -29);
  stroke(cr, kSun, 1.4, 0.7);
  // The body: dune-coloured back, pale belly.
  sharkBody(cr, flick);
  vgrad(cr, -30, 32, {{0.0, lighten(kSharkBack, 0.12f)}, {0.45, kSharkBack}, {0.62, lerpColor(kSharkBack, kBelly, 0.7f)}, {1.0, kBelly}}, 1.0, true);
  stroke(cr, kInk, 2.6);
  // The belly line.
  cairo_move_to(cr, 88, 4);
  cairo_curve_to(cr, 60, 16, 0, 16, -100, 4 + flick);
  stroke(cr, lerpColor(kSharkBack, kBelly, 0.5f), 1.6, 0.6);
  // Plates down its back, like a lizard's scales.
  for (int i = 0; i < 9; ++i)
  {
    const double x = 70 - i * 18.0, y = -24 + std::abs(i - 4) * 0.8 + (i > 6 ? (i - 6) * 3.0 : 0.0);
    cairo_move_to(cr, x - 8, y + 4);
    cairo_curve_to(cr, x - 5, y - 1, x + 3, y - 1, x + 6, y + 4);
    stroke(cr, kSharkDark, 1.3, 0.7);
    cairo_move_to(cr, x - 6, y + 2.5);
    cairo_curve_to(cr, x - 4, y - 0.5, x + 2, y - 0.5, x + 4, y + 2.5);
    stroke(cr, kSun, 0.9, 0.5);
  }
  // Mottling.
  Rng rng(32101u);
  for (int i = 0; i < 16; ++i)
  {
    const double x = rng.range(-90, 70), y = rng.range(-22, 2);
    ellipse(cr, x, y, rng.range(2.0f, 5.0f), rng.range(1.5f, 3.0f), rng.range(-0.5f, 0.5f));
    setRgba(cr, kSharkDark, 0.45);
    cairo_fill(cr);
  }
  // The rim of sun along its back.
  cairo_move_to(cr, 92, -8);
  cairo_curve_to(cr, 80, -22, 50, -28, 10, -26);
  cairo_curve_to(cr, -30, -24, -70, -16, -100, -8 + flick);
  stroke(cr, kSun, 1.8, 0.75);
  // Gills.
  for (int i = 0; i < 3; ++i)
  {
    const double x = 34 - i * 7.0;
    cairo_move_to(cr, x, -8);
    cairo_curve_to(cr, x - 3, 0, x - 3, 8, x, 14);
    stroke(cr, kInk, 1.6, 0.75);
  }
  // The mouth.
  if (bite)
  {
    // Jaws wide: the lower jaw swung down, teeth in rows, a dark throat.
    cairo_move_to(cr, 98, -4);
    cairo_line_to(cr, 54, 4);
    cairo_line_to(cr, 92, 38);
    cairo_curve_to(cr, 98, 30, 100, 10, 98, -4);
    cairo_close_path(cr);
    hgrad(cr, 54, 98, {{0.0, rgb(60, 14, 20)}, {1.0, rgb(170, 60, 64)}}, 1.0, true);
    stroke(cr, kInk, 2.2);
    // The lower jaw.
    cairo_move_to(cr, 54, 6);
    cairo_line_to(cr, 92, 38);
    cairo_curve_to(cr, 86, 46, 60, 36, 46, 20);
    cairo_close_path(cr);
    vgrad(cr, 6, 46, {{0.0, lerpColor(kSharkBack, kBelly, 0.6f)}, {1.0, kBelly}}, 1.0, true);
    stroke(cr, kInk, 2.2);
    for (int i = 0; i < 6; ++i)
    {
      // Upper teeth hang down off the upper jaw; lower ones stand up.
      const double u = (i + 0.5) / 6.0;
      const double ux = 98 - 44 * u, uy = -4 + 8 * u;
      cairo_move_to(cr, ux - 3, uy);
      cairo_line_to(cr, ux, uy + 8);
      cairo_line_to(cr, ux + 3, uy);
      cairo_close_path(cr);
      setRgba(cr, rgb(255, 252, 240), 1.0);
      cairo_fill_preserve(cr);
      stroke(cr, kInk, 0.8, 0.8);
      const double lx = 54 + 38 * u, ly = 6 + 32 * u;
      cairo_move_to(cr, lx - 3 + 2, ly + 1);
      cairo_line_to(cr, lx - 4, ly - 7);
      cairo_line_to(cr, lx + 2 + 2, ly - 1);
      cairo_close_path(cr);
      setRgba(cr, rgb(255, 252, 240), 1.0);
      cairo_fill_preserve(cr);
      stroke(cr, kInk, 0.8, 0.8);
    }
  }
  else
  {
    cairo_move_to(cr, 95, 0);
    cairo_curve_to(cr, 84, 8, 70, 10, 56, 6);
    stroke(cr, kInk, 2.2);
    for (int i = 0; i < 5; ++i)
    {
      const double x = 90 - i * 7.0, y = 3.5 + i * 0.8;
      cairo_move_to(cr, x - 2, y);
      cairo_line_to(cr, x, y + 4);
      cairo_line_to(cr, x + 2, y);
      cairo_close_path(cr);
      setRgba(cr, rgb(255, 250, 236), 1.0);
      cairo_fill(cr);
    }
  }
  // The eye: small, black, a glint.
  circle(cr, 66, -10, 4.2);
  setRgba(cr, rgb(16, 10, 8), 1.0);
  cairo_fill(cr);
  circle(cr, 64.8, -11.4, 1.4);
  setRgba(cr, rgb(255, 255, 255), 0.9);
  cairo_fill(cr);
  cairo_move_to(cr, 58, -16);
  cairo_line_to(cr, 72, -15);
  stroke(cr, kSharkDark, 1.8, 0.8); // a heavy brow
  cairo_restore(cr);
}

// --- Vulture Drone ------------------------------------------------------------------------------------

void rotor(cairo_t* cr, double x, double y, double rx, int frame, Color metal)
{
  // The hub's mast, the blurred disc, two blades caught by the sun.
  cairo_move_to(cr, x, y);
  cairo_line_to(cr, x, y + 9);
  stroke(cr, kGunDark, 3.0);
  ellipse(cr, x, y, rx, rx * 0.17);
  setRgba(cr, rgb(200, 206, 220), 0.28);
  cairo_fill_preserve(cr);
  stroke(cr, rgb(240, 244, 255), 0.8, 0.35);
  const double a = frame % 2 ? 0.55 : -0.35;
  for (const double s : {1.0, -1.0})
  {
    cairo_move_to(cr, x, y);
    cairo_line_to(cr, x + s * rx * std::cos(a), y + s * rx * 0.17 * std::sin(a));
    stroke(cr, metal, 2.6, 0.85);
  }
  circle(cr, x, y, 3.5);
  setRgba(cr, kGunDark, 1.0);
  cairo_fill(cr);
}

// A lump of junk metal in a box of 64 x 64 design units.
void scrapLump(cairo_t* cr, bool carrier, int frame)
{
  const Color rust = carrier ? rgb(112, 150, 70) : rgb(150, 86, 48);
  const Color metal = carrier ? rgb(150, 170, 120) : rgb(150, 150, 156);
  cairo_save(cr);
  cairo_translate(cr, 32, 32);
  cairo_rotate(cr, frame * kPi * 0.5 + 0.3);
  // A bent plate.
  cairo_move_to(cr, -22, -8);
  cairo_line_to(cr, 4, -16);
  cairo_line_to(cr, 20, -6);
  cairo_line_to(cr, 14, 10);
  cairo_line_to(cr, -6, 14);
  cairo_line_to(cr, -20, 8);
  cairo_close_path(cr);
  vgrad(cr, -16, 14, {{0.0, lighten(metal, 0.25f)}, {0.5, rust}, {1.0, darken(rust, 0.3f)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  cairo_move_to(cr, -8, -12);
  cairo_line_to(cr, 2, 12);
  stroke(cr, darken(rust, 0.35f), 1.6, 0.8); // its bend
  // A gear stuck to it.
  cairo_save(cr);
  cairo_translate(cr, 8, -2);
  for (int k = 0; k < 8; ++k)
  {
    const double a = k * kPi / 4;
    cairo_move_to(cr, std::cos(a) * 7, std::sin(a) * 7);
    cairo_line_to(cr, std::cos(a) * 10, std::sin(a) * 10);
    stroke(cr, metal, 3.0);
  }
  circle(cr, 0, 0, 7.5);
  setRgba(cr, metal, 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.4);
  circle(cr, 0, 0, 2.5);
  setRgba(cr, kInk, 1.0);
  cairo_fill(cr);
  cairo_restore(cr);
  // A pipe end.
  cairo_rectangle(cr, -18, -2, 12, 7);
  hgrad(cr, -18, -6, {{0.0, metal}, {1.0, darken(metal, 0.4f)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  if (carrier)
    for (const auto& d : {std::array<double, 2>{-10, 10}, std::array<double, 2>{6, 13}})
    {
      ellipse(cr, d[0], d[1] + 3, 2.2, 3.4);
      setRgba(cr, rgb(150, 255, 110), 0.9);
      cairo_fill(cr);
    }
  cairo_restore(cr);
}

void vultureDrone(cairo_t* cr, double w, double h, int variant, int frame)
{
  const bool carrying = variant % 2 == 0, carrier = variant >= 2;
  const Color body = carrier ? rgb(92, 104, 70) : kGun;
  const Color bodyDark = carrier ? rgb(46, 54, 34) : kGunDark;
  cairo_save(cr);
  frameBox(cr, w, h, 192, 96);
  // The far arm and its rotor, behind.
  cairo_move_to(cr, 92, 36);
  cairo_line_to(cr, 40, 16);
  stroke(cr, darken(body, 0.35f), 5.0);
  rotor(cr, 36, 14, 30, frame + 1, rgb(140, 146, 160));
  // Talons: two legs down from the belly, clutching scrap (or curled).
  for (const double lx : {88.0, 104.0})
  {
    cairo_move_to(cr, lx, 56);
    cairo_line_to(cr, lx - 2, 70);
    stroke(cr, kInk, 5.0);
    cairo_move_to(cr, lx, 56);
    cairo_line_to(cr, lx - 2, 70);
    stroke(cr, rgb(130, 132, 140), 3.0);
  }
  if (carrying)
  {
    cairo_save(cr);
    cairo_translate(cr, 96 - 18, 66);
    cairo_scale(cr, 36.0 / 64.0, 30.0 / 64.0);
    scrapLump(cr, carrier, 0);
    cairo_restore(cr);
  }
  for (const double lx : {86.0, 102.0})
  {
    // The claw: three hooked toes.
    const double cy = carrying ? 74.0 : 70.0;
    for (int k = -1; k <= 1; ++k)
    {
      cairo_move_to(cr, lx, cy - 2);
      if (carrying)
        cairo_curve_to(cr, lx + k * 7, cy + 2, lx + k * 9, cy + 8, lx + k * 5, cy + 11);
      else
        cairo_curve_to(cr, lx + k * 5, cy + 3, lx + k * 5, cy + 7, lx + k * 2, cy + 6);
      stroke(cr, kInk, 2.6);
    }
  }
  // The ragged "wings": sheet-metal feathers hanging off the near arm.
  cairo_move_to(cr, 70, 30);
  cairo_line_to(cr, 150, 18);
  cairo_line_to(cr, 146, 30);
  for (int k = 0; k < 6; ++k)
  {
    const double x = 140 - k * 13.0;
    cairo_line_to(cr, x - 2, 44 + (k % 2) * 6);
    cairo_line_to(cr, x - 8, 34);
  }
  cairo_close_path(cr);
  vgrad(cr, 18, 50, {{0.0, darken(body, 0.1f)}, {1.0, darken(body, 0.55f)}}, 1.0, true);
  stroke(cr, kInk, 1.8);
  for (int k = 0; k < 5; ++k)
  {
    cairo_move_to(cr, 134 - k * 13.0, 26 + k * 1.2);
    cairo_line_to(cr, 132 - k * 13.0, 42);
    stroke(cr, bodyDark, 1.0, 0.7);
  }
  // The body: a hunched shell, plated, riveted.
  cairo_move_to(cr, 58, 46);
  cairo_curve_to(cr, 56, 30, 76, 20, 104, 22);
  cairo_curve_to(cr, 126, 24, 140, 32, 140, 44);
  cairo_curve_to(cr, 136, 56, 120, 60, 98, 60);
  cairo_curve_to(cr, 76, 60, 60, 56, 58, 46);
  cairo_close_path(cr);
  vgrad(cr, 20, 60, {{0.0, lighten(body, 0.35f)}, {0.4, body}, {1.0, bodyDark}}, 1.0, true);
  stroke(cr, kInk, 2.4);
  cairo_move_to(cr, 74, 26);
  cairo_curve_to(cr, 80, 40, 80, 50, 76, 58);
  stroke(cr, bodyDark, 1.2, 0.7);
  cairo_move_to(cr, 104, 22);
  cairo_curve_to(cr, 108, 36, 108, 50, 104, 60);
  stroke(cr, bodyDark, 1.2, 0.7);
  for (const auto& rv : {std::array<double, 2>{68, 46}, std::array<double, 2>{92, 32}, std::array<double, 2>{118, 34}, std::array<double, 2>{124, 50}})
  {
    circle(cr, rv[0], rv[1], 1.4);
    setRgba(cr, lighten(body, 0.5f), 0.9);
    cairo_fill(cr);
  }
  cairo_move_to(cr, 64, 34);
  cairo_curve_to(cr, 76, 22, 100, 20, 124, 26);
  stroke(cr, kSun, 1.6, 0.6);
  if (carrier)
  {
    // Rust bloom and green crust, a drip.
    for (const auto& c : {std::array<double, 3>{84, 40, 7}, std::array<double, 3>{116, 48, 6}})
      glow(cr, c[0], c[1], c[2], rgb(140, 220, 90), 0.6);
    ellipse(cr, 100, 64, 2.0, 3.4);
    setRgba(cr, rgb(150, 255, 110), 0.85);
    cairo_fill(cr);
  }
  // The neck ruff: a collar of dark notched plates.
  cairo_move_to(cr, 128, 26);
  for (int k = 0; k < 6; ++k)
  {
    const double a = -1.2 + k * 0.48;
    cairo_line_to(cr, 140 + std::cos(a) * 16, 38 + std::sin(a) * 16);
    cairo_line_to(cr, 140 + std::cos(a + 0.24) * 10, 38 + std::sin(a + 0.24) * 10);
  }
  cairo_close_path(cr);
  setRgba(cr, rgb(30, 28, 34), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.4);
  // The head: a bald, hooded camera on a short neck, a hooked visor beak
  // and a red eye.
  cairo_move_to(cr, 140, 30);
  cairo_curve_to(cr, 146, 18, 166, 16, 174, 26);
  cairo_curve_to(cr, 182, 30, 184, 38, 178, 44);
  cairo_curve_to(cr, 176, 38, 170, 38, 166, 42);
  cairo_curve_to(cr, 156, 46, 144, 44, 140, 30);
  cairo_close_path(cr);
  vgrad(cr, 16, 46, {{0.0, rgb(214, 160, 150)}, {0.5, rgb(170, 110, 104)}, {1.0, rgb(110, 66, 64)}}, 1.0, true);
  stroke(cr, kInk, 2.2);
  // The hooked visor.
  cairo_move_to(cr, 160, 22);
  cairo_curve_to(cr, 176, 20, 186, 28, 184, 42);
  cairo_curve_to(cr, 180, 36, 172, 32, 162, 32);
  cairo_close_path(cr);
  vgrad(cr, 20, 42, {{0.0, rgb(230, 222, 200)}, {1.0, rgb(150, 140, 120)}}, 1.0, true);
  stroke(cr, kInk, 1.8);
  glow(cr, 158, 30, 9, rgb(255, 60, 40), 0.7);
  circle(cr, 158, 30, 3.6);
  setRgba(cr, rgb(255, 70, 50), 1.0);
  cairo_fill(cr);
  circle(cr, 157, 29, 1.2);
  setRgba(cr, rgb(255, 230, 220), 1.0);
  cairo_fill(cr);
  // The near arm and its rotor, out over the head.
  cairo_move_to(cr, 112, 30);
  cairo_line_to(cr, 162, 10);
  stroke(cr, kInk, 6.0);
  cairo_move_to(cr, 112, 30);
  cairo_line_to(cr, 162, 10);
  stroke(cr, lighten(body, 0.2f), 3.4);
  rotor(cr, 164, 8, 32, frame, rgb(190, 196, 210));
  // A short antenna and its tip light.
  cairo_move_to(cr, 96, 22);
  cairo_line_to(cr, 90, 6);
  stroke(cr, kInk, 1.6);
  circle(cr, 90, 6, 2.2);
  setRgba(cr, frame % 2 ? rgb(255, 80, 60) : rgb(120, 40, 30), 1.0);
  cairo_fill(cr);
  cairo_restore(cr);
}

void scrap(cairo_t* cr, double w, double h, int variant, int frame)
{
  cairo_save(cr);
  frameBox(cr, w, h, 64, 64);
  scrapLump(cr, variant == 1, frame % 4);
  cairo_restore(cr);
}

// --- Scorpion Mech -------------------------------------------------------------------------------------

void mechLeg(cairo_t* cr, double hx, double hy, double kx, double ky, double fx, double fy, Color c, bool far)
{
  const Color ink = far ? darken(kInk, 0.0f) : kInk;
  for (int pass = 0; pass < 2; ++pass)
  {
    cairo_move_to(cr, hx, hy);
    cairo_line_to(cr, kx, ky);
    cairo_line_to(cr, fx, fy);
    stroke(cr, pass == 0 ? ink : c, pass == 0 ? 9.0 : 5.6);
  }
  circle(cr, kx, ky, 4.4);
  setRgba(cr, far ? darken(c, 0.3f) : lighten(c, 0.2f), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, ink, 1.4);
  // The foot: a little claw tip.
  cairo_move_to(cr, fx - 5, fy);
  cairo_line_to(cr, fx + 5, fy);
  stroke(cr, ink, 3.0);
}

void scorpionMech(cairo_t* cr, double w, double h, int variant, int frame)
{
  cairo_save(cr);
  frameBox(cr, w, h, 256, 192);
  const double step = frame % 2 ? 6.0 : -6.0;
  const Color farLeg = rgb(120, 100, 60), nearLeg = rgb(196, 160, 70);
  // Far legs.
  for (int i = 0; i < 3; ++i)
  {
    const double hx = 92 + i * 34.0, s = (i % 2 ? step : -step);
    mechLeg(cr, hx, 132, hx - 18 + s * 0.5, 106, hx - 30 + s, 190, farLeg, true);
  }
  // The tail: armoured segments arching from the rear over the back, the
  // stinger hanging over the middle (the top middle of the box).
  struct Seg
  {
    double x, y, r;
  };
  std::array<Seg, 8> segs{};
  for (std::size_t i = 0; i < segs.size(); ++i)
  {
    const double u = double(i) / double(segs.size() - 1);
    // A cubic from the rear (58, 120) up over (14, 40), (60, -6) to (128, 22).
    const double a = 1 - u;
    const double x = a * a * a * 58 + 3 * a * a * u * 10 + 3 * a * u * u * 52 + u * u * u * 124;
    const double y = a * a * a * 122 + 3 * a * a * u * 44 + 3 * a * u * u * -8 + u * u * u * 16;
    segs[i] = {x, y, 15.0 - u * 6.0};
  }
  for (std::size_t i = 0; i < segs.size(); ++i)
  {
    const auto& s = segs[i];
    ellipse(cr, s.x, s.y, s.r * 1.15, s.r);
    vgrad(cr, s.y - s.r, s.y + s.r, {{0.0, lighten(kSite, 0.3f)}, {0.5, kSite}, {1.0, darken(kSite, 0.4f)}}, 1.0, true);
    stroke(cr, kInk, 2.2);
    cairo_move_to(cr, s.x - s.r * 0.7, s.y - s.r * 0.55);
    cairo_curve_to(cr, s.x - s.r * 0.3, s.y - s.r * 0.95, s.x + s.r * 0.3, s.y - s.r * 0.95, s.x + s.r * 0.7, s.y - s.r * 0.55);
    stroke(cr, kSun, 1.2, 0.7);
    if (i % 2 == 0)
    {
      // A hazard band on every other segment.
      cairo_move_to(cr, s.x - s.r * 0.3, s.y - s.r * 0.9);
      cairo_line_to(cr, s.x + s.r * 0.1, s.y + s.r * 0.9);
      stroke(cr, rgb(30, 26, 22), 2.6, 0.8);
    }
  }
  // The stinger: a laser housing pointing down and forward, its lens.
  const double sx = 128, sy = 16;
  cairo_move_to(cr, sx - 14, sy - 10);
  cairo_line_to(cr, sx + 12, sy - 12);
  cairo_line_to(cr, sx + 10, sy + 6);
  cairo_curve_to(cr, sx + 6, sy + 14, sx - 4, sy + 16, sx - 10, sy + 8);
  cairo_close_path(cr);
  vgrad(cr, sy - 12, sy + 16, {{0.0, rgb(120, 124, 136)}, {1.0, rgb(40, 40, 48)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  const Color lens = variant == 2 ? rgb(255, 250, 240) : (variant == 1 ? rgb(255, 50, 40) : rgb(120, 30, 30));
  if (variant > 0)
    glow(cr, sx, sy + 4, variant == 2 ? 24 : 16, variant == 2 ? rgb(255, 160, 140) : rgb(255, 40, 30), 0.85);
  circle(cr, sx, sy + 4, 4.6);
  setRgba(cr, lens, 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.2);
  // The carapace: a low armoured dome in site paint, plated.
  cairo_move_to(cr, 48, 140);
  cairo_curve_to(cr, 50, 108, 90, 92, 140, 92);
  cairo_curve_to(cr, 186, 92, 214, 106, 220, 132);
  cairo_curve_to(cr, 214, 150, 180, 156, 136, 156);
  cairo_curve_to(cr, 90, 156, 56, 152, 48, 140);
  cairo_close_path(cr);
  vgrad(cr, 92, 156, {{0.0, lighten(kSite, 0.35f)}, {0.45, kSite}, {1.0, darken(kSite, 0.45f)}}, 1.0, true);
  stroke(cr, kInk, 2.8);
  for (const double x : {92.0, 130.0, 168.0})
  {
    cairo_move_to(cr, x, 94);
    cairo_curve_to(cr, x + 4, 116, x + 4, 140, x, 156);
    stroke(cr, darken(kSite, 0.45f), 1.6, 0.8);
  }
  // Rust and grime, a dent.
  Rng rng(32121u);
  for (int i = 0; i < 6; ++i)
    glow(cr, rng.range(60, 210), rng.range(110, 150), rng.range(5, 10), rgb(150, 76, 36), 0.5);
  cairo_move_to(cr, 60, 116);
  cairo_curve_to(cr, 90, 96, 170, 92, 206, 110);
  stroke(cr, kSun, 1.8, 0.7);
  // A hazard-striped skirt along the front.
  cairo_save(cr);
  cairo_move_to(cr, 176, 100);
  cairo_curve_to(cr, 200, 104, 214, 116, 220, 132);
  cairo_curve_to(cr, 214, 146, 196, 152, 180, 154);
  cairo_close_path(cr);
  cairo_clip_preserve(cr);
  setRgba(cr, rgb(30, 26, 22), 1.0);
  cairo_fill(cr);
  for (double x = 160; x < 240; x += 12)
  {
    cairo_move_to(cr, x, 96);
    cairo_line_to(cr, x + 6, 96);
    cairo_line_to(cr, x - 24, 160);
    cairo_line_to(cr, x - 30, 160);
    cairo_close_path(cr);
    setRgba(cr, kSite, 1.0);
    cairo_fill(cr);
  }
  cairo_restore(cr);
  // The sensor head: a low wedge with three red lenses.
  cairo_move_to(cr, 200, 118);
  cairo_line_to(cr, 236, 124);
  cairo_line_to(cr, 232, 142);
  cairo_line_to(cr, 200, 146);
  cairo_close_path(cr);
  vgrad(cr, 118, 146, {{0.0, rgb(110, 112, 122)}, {1.0, rgb(40, 40, 48)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  for (int k = 0; k < 3; ++k)
  {
    const double x = 212 + k * 7.0;
    circle(cr, x, 131, 2.6);
    setRgba(cr, variant > 0 ? rgb(255, 80, 60) : rgb(220, 60, 40), 1.0);
    cairo_fill(cr);
  }
  // The pincers: an arm out front and an open claw near the ground.
  for (int side = 0; side < 2; ++side)
  {
    const double off = side == 0 ? -6.0 : 6.0;
    const Color c = side == 0 ? farLeg : nearLeg;
    cairo_move_to(cr, 206, 140 + off);
    cairo_line_to(cr, 226, 160 + off);
    cairo_line_to(cr, 240, 152 + off);
    stroke(cr, kInk, 9.0);
    cairo_move_to(cr, 206, 140 + off);
    cairo_line_to(cr, 226, 160 + off);
    cairo_line_to(cr, 240, 152 + off);
    stroke(cr, c, 5.4);
    // The claw.
    cairo_move_to(cr, 236, 146 + off);
    cairo_curve_to(cr, 252, 138 + off, 260, 146 + off, 256, 154 + off);
    cairo_line_to(cr, 244, 152 + off);
    cairo_curve_to(cr, 254, 158 + off, 252, 166 + off, 240, 164 + off);
    cairo_close_path(cr);
    vgrad(cr, 138 + off, 166 + off, {{0.0, lighten(c, 0.3f)}, {1.0, darken(c, 0.3f)}}, 1.0, true);
    stroke(cr, kInk, 1.8);
  }
  // Near legs.
  for (int i = 0; i < 3; ++i)
  {
    const double hx = 84 + i * 40.0, s = (i % 2 ? -step : step);
    mechLeg(cr, hx, 142, hx - 22 + s * 0.5, 116, hx - 34 + s, 191, nearLeg, false);
  }
  cairo_restore(cr);
}

// --- The duck's sunglasses -----------------------------------------------------------------------------

void duckSunglasses(cairo_t* cr, double w, double h, int variant)
{
  if (variant != 1)
    return;
  cairo_save(cr);
  cairo_translate(cr, kM, kM);
  cairo_scale(cr, w / 64.0, h / 64.0);
  // The arm back over the head.
  cairo_move_to(cr, 36, 20);
  cairo_line_to(cr, 27, 19);
  stroke(cr, rgb(30, 24, 14), 2.0);
  cairo_move_to(cr, 36, 20);
  cairo_line_to(cr, 27, 19);
  stroke(cr, rgb(220, 180, 70), 0.9);
  // The far lens peeking past the beak's root, then the near lens.
  ellipse(cr, 50.5, 21.5, 2.6, 4.0, 0.2);
  vgrad(cr, 17, 26, {{0.0, rgb(60, 70, 90)}, {1.0, rgb(8, 8, 14)}}, 1.0, true);
  stroke(cr, rgb(200, 160, 50), 1.0);
  cairo_move_to(cr, 47, 19.5);
  cairo_line_to(cr, 49, 19);
  stroke(cr, rgb(210, 170, 60), 1.2);
  cairo_move_to(cr, 33, 18);
  cairo_curve_to(cr, 37, 16.5, 44, 16.5, 47, 18);
  cairo_curve_to(cr, 48, 22, 46, 27, 41, 27.5);
  cairo_curve_to(cr, 36, 28, 33, 24, 33, 18);
  cairo_close_path(cr);
  vgrad(cr, 16.5, 28, {{0.0, rgb(70, 84, 110)}, {0.5, rgb(20, 22, 34)}, {1.0, rgb(6, 6, 10)}}, 1.0, true);
  stroke(cr, rgb(214, 172, 60), 1.3);
  // The glint.
  cairo_move_to(cr, 36, 20);
  cairo_line_to(cr, 39.5, 19);
  stroke(cr, rgb(255, 255, 255), 1.3, 0.9);
  circle(cr, 44.5, 24, 0.9);
  setRgba(cr, rgb(255, 255, 255), 0.7);
  cairo_fill(cr);
  cairo_restore(cr);
}

} // namespace

bool drawDishArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame)
{
  (void)t;
  if (key == "sand_shark")
    sandShark(cr, w, h, std::clamp(variant, 0, 3), frame % 2);
  else if (key == "vulture_drone")
    vultureDrone(cr, w, h, std::clamp(variant, 0, 3), frame % 2);
  else if (key == "scrap")
    scrap(cr, w, h, std::clamp(variant, 0, 1), frame % 4);
  else if (key == "scorpion_mech")
    scorpionMech(cr, w, h, std::clamp(variant, 0, 2), frame % 2);
  else if (key == "duck_sunglasses")
    duckSunglasses(cr, w, h, variant);
  else
    return false;
  return true;
}

} // namespace gr
