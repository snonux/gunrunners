// Dry Gulch (Level 22) in the style of enemy_art.cpp: the Duelist, the
// Tumble Mine and the Window Bandit, each drawn in screen pixels into a
// texture with a 32 px margin, the cell box at (32, 32) .. (32 + w, 32 + h),
// facing right. Also the Miracle Tonic, the duck's cowboy hat and the pieces
// the level and the cutscenes share: a cowboy hat, a tumbleweed, the
// outlaw's face on the wanted posters and a saloon's false front.

#include "assets/enemy_art_west.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace gr
{

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr Color kInk = rgb(30, 18, 12);
constexpr double kLine = 2.2;
constexpr double kM = 32.0;

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

double rnd(std::uint32_t& s)
{
  s ^= s << 13;
  s ^= s >> 17;
  s ^= s << 5;
  return double(s & 0xFFFF) / 65535.0;
}

void centred(cairo_t* cr, const char* s, double cx, double baseline, double size)
{
  selectGameFont(cr);
  cairo_set_font_size(cr, size);
  cairo_text_extents_t e;
  cairo_text_extents(cr, s, &e);
  cairo_move_to(cr, cx - e.width * 0.5 - e.x_bearing, baseline);
  cairo_show_text(cr, s);
}

// A revolver lying along +x from its grip at (0, 0) in the current
// transform, `s` px to a unit (about 26 units long).
void revolver(cairo_t* cr, double s, Color metal)
{
  // The grip.
  cairo_move_to(cr, -2 * s, -2 * s);
  cairo_line_to(cr, 4 * s, -2 * s);
  cairo_line_to(cr, 2 * s, 9 * s);
  cairo_line_to(cr, -4 * s, 8 * s);
  cairo_close_path(cr);
  fillLinear(cr, -4 * s, 0, 4 * s, 0, rgb(150, 92, 50), rgb(80, 44, 22), 1.4);
  // The frame and the cylinder.
  roundedRect(cr, 0, -5 * s, 11 * s, 6 * s, 1.5 * s);
  fillLinear(cr, 0, -5 * s, 0, 1 * s, lighten(metal, 0.4f), darken(metal, 0.45f), 1.4);
  // The barrel.
  cairo_rectangle(cr, 10 * s, -4.5 * s, 16 * s, 3 * s);
  fillLinear(cr, 0, -4.5 * s, 0, -1.5 * s, lighten(metal, 0.5f), darken(metal, 0.4f), 1.4);
  // The hammer and the trigger guard.
  cairo_move_to(cr, 0, -5 * s);
  cairo_line_to(cr, -3 * s, -8 * s);
  setColor(cr, kInk);
  cairo_set_line_width(cr, 2.0 * s);
  cairo_stroke(cr);
  cairo_arc(cr, 6 * s, 2.5 * s, 2.6 * s, 0, kPi);
  cairo_set_line_width(cr, 1.2 * s);
  cairo_stroke(cr);
  // A glint along the barrel.
  cairo_move_to(cr, 12 * s, -4 * s);
  cairo_line_to(cr, 24 * s, -4 * s);
  setRgba(cr, rgb(255, 255, 255), 0.6);
  cairo_set_line_width(cr, 0.8 * s);
  cairo_stroke(cr);
}

// --- The Duelist ---------------------------------------------------------------------------

// A gunslinger in a long duster and a black hat, standing square. Variant 0
// his gun hand hangs loose, 2 it hovers over the holster (frame 1 twitches
// the fingers), 1 the gun is out at chest height.
void duelist(cairo_t* cr, double w, double h, int variant, int frame)
{
  const double sx = w / 96.0, sy = h / 160.0, s = std::min(sx, sy);
  auto X = [&](double x) { return kM + x * sx; };
  auto Y = [&](double y) { return kM + y * sy; };
  const Color coat = rgb(150, 112, 74), coatDark = rgb(92, 62, 40), coatLight = rgb(196, 160, 112);
  const Color shirt = rgb(206, 196, 170), vest = rgb(70, 52, 46), trousers = rgb(70, 62, 60);
  const Color boot = rgb(92, 54, 30), skin = rgb(222, 170, 128), skinDark = rgb(170, 116, 82);
  const Color felt = rgb(46, 36, 34), steel = rgb(150, 154, 166);
  const bool drawn = variant == 1, hover = variant == 2;
  // The back tail of the coat, behind everything.
  cairo_move_to(cr, X(26), Y(52));
  cairo_curve_to(cr, X(16), Y(90), X(8), Y(130), X(6), Y(152));
  cairo_line_to(cr, X(36), Y(154));
  cairo_line_to(cr, X(40), Y(100));
  cairo_close_path(cr);
  fillLinear(cr, X(6), 0, X(40), 0, darken(coat, 0.3f), darken(coatDark, 0.2f));
  // The back arm, hanging along the coat.
  limb(cr, X(32), Y(56), X(24), Y(84), X(26), Y(110), 11 * s, coatDark);
  circle(cr, X(26), Y(112), 5 * s);
  fillRadial(cr, X(26), Y(112), 5 * s, skin, skinDark, 1.6);
  // Legs and boots.
  for (const int k : {0, 1})
  {
    const double lx = k == 0 ? 34 : 52;
    cairo_rectangle(cr, X(lx), Y(98), 12 * sx, 46 * sy);
    fillLinear(cr, X(lx), 0, X(lx + 12), 0, k == 0 ? darken(trousers, 0.25f) : trousers, darken(trousers, 0.4f), 1.8);
    cairo_move_to(cr, X(lx - 1), Y(136));
    cairo_line_to(cr, X(lx + 13), Y(136));
    cairo_line_to(cr, X(lx + 14), Y(152));
    cairo_line_to(cr, X(lx + 22), Y(154));
    cairo_line_to(cr, X(lx + 22), Y(160));
    cairo_line_to(cr, X(lx - 2), Y(160));
    cairo_close_path(cr);
    fillLinear(cr, 0, Y(136), 0, Y(160), lighten(boot, 0.2f), darken(boot, 0.4f), 1.8);
    // The spur.
    circle(cr, X(lx - 3), Y(152), 3 * s);
    setColor(cr, rgb(210, 190, 120));
    cairo_fill(cr);
  }
  // The shirt and the vest.
  cairo_rectangle(cr, X(36), Y(48), 26 * sx, 52 * sy);
  fillLinear(cr, X(36), 0, X(62), 0, shirt, darken(shirt, 0.25f), 1.8);
  for (const int side : {-1, 1})
  {
    cairo_move_to(cr, X(49 + side * 3), Y(50));
    cairo_line_to(cr, X(49 + side * 13), Y(50));
    cairo_line_to(cr, X(49 + side * 13), Y(98));
    cairo_line_to(cr, X(49 + side * 2), Y(98));
    cairo_close_path(cr);
    fillLinear(cr, 0, Y(50), 0, Y(98), lighten(vest, 0.15f), darken(vest, 0.3f), 1.4);
  }
  // The gun belt, its buckle and cartridge loops.
  cairo_rectangle(cr, X(34), Y(94), 32 * sx, 8 * sy);
  fillLinear(cr, 0, Y(94), 0, Y(102), rgb(110, 70, 40), rgb(56, 32, 18), 1.4);
  for (double bx = 36; bx < 60; bx += 4)
  {
    cairo_rectangle(cr, X(bx), Y(95), 2 * sx, 4 * sy);
    setColor(cr, rgb(220, 186, 90));
    cairo_fill(cr);
  }
  roundedRect(cr, X(45), Y(93.5), 8 * sx, 9 * sy, 1.5);
  fillLinear(cr, 0, Y(93), 0, Y(103), rgb(255, 230, 130), rgb(170, 120, 30), 1.2);
  // The coat's front panels, swept back from the holster.
  cairo_move_to(cr, X(26), Y(46));
  cairo_line_to(cr, X(40), Y(50));
  cairo_curve_to(cr, X(40), Y(80), X(38), Y(120), X(32), Y(150));
  cairo_line_to(cr, X(12), Y(152));
  cairo_curve_to(cr, X(16), Y(110), X(20), Y(70), X(26), Y(46));
  cairo_close_path(cr);
  fillLinear(cr, X(12), 0, X(40), 0, coatLight, coat);
  cairo_move_to(cr, X(58), Y(50));
  cairo_line_to(cr, X(70), Y(46));
  cairo_curve_to(cr, X(76), Y(70), X(78), Y(86), X(72), Y(100));
  cairo_curve_to(cr, X(80), Y(120), X(86), Y(140), X(88), Y(148));
  cairo_line_to(cr, X(68), Y(152));
  cairo_curve_to(cr, X(64), Y(120), X(60), Y(90), X(58), Y(50));
  cairo_close_path(cr);
  fillLinear(cr, X(58), 0, X(88), 0, coat, coatDark);
  // Creases in the coat.
  for (const double cx : {20.0, 28.0, 76.0})
  {
    cairo_move_to(cr, X(cx), Y(108));
    cairo_line_to(cr, X(cx + (cx > 50 ? 4 : -3)), Y(146));
  }
  setRgba(cr, darken(coatDark, 0.4f), 0.5);
  cairo_set_line_width(cr, 1.4);
  cairo_stroke(cr);
  // The holster on his right hip, the gun's grip in it unless drawn.
  cairo_move_to(cr, X(62), Y(98));
  cairo_line_to(cr, X(74), Y(98));
  cairo_line_to(cr, X(72), Y(128));
  cairo_line_to(cr, X(64), Y(128));
  cairo_close_path(cr);
  fillLinear(cr, X(62), 0, X(74), 0, rgb(130, 82, 46), rgb(70, 40, 20), 1.6);
  if (!drawn)
  {
    cairo_move_to(cr, X(64), Y(99));
    cairo_line_to(cr, X(71), Y(99));
    cairo_line_to(cr, X(74), Y(86));
    cairo_line_to(cr, X(67), Y(84));
    cairo_close_path(cr);
    fillLinear(cr, X(64), 0, X(74), 0, rgb(170, 110, 60), rgb(90, 52, 26), 1.4);
  }
  // The bandana and the neck.
  limb(cr, X(49), Y(38), X(49), Y(44), X(49), Y(48), 9 * s, skinDark, 1.6);
  cairo_move_to(cr, X(38), Y(44));
  cairo_line_to(cr, X(60), Y(44));
  cairo_line_to(cr, X(51), Y(58));
  cairo_close_path(cr);
  fillLinear(cr, 0, Y(44), 0, Y(58), rgb(220, 60, 50), rgb(130, 24, 24), 1.6);
  // The head.
  ellipse(cr, X(50), Y(30), 11 * sx, 13 * sy);
  fillRadial(cr, X(52), Y(30), 13 * s, skin, skinDark, 2.0);
  // Stubble, the moustache, the eye in the brim's shade.
  ellipse(cr, X(52), Y(38), 8 * sx, 5 * sy);
  setRgba(cr, rgb(90, 60, 40), 0.35);
  cairo_fill(cr);
  cairo_move_to(cr, X(48), Y(36));
  cairo_curve_to(cr, X(52), Y(33), X(58), Y(33), X(62), Y(37));
  cairo_curve_to(cr, X(58), Y(36), X(53), Y(37), X(48), Y(36));
  setColor(cr, rgb(70, 44, 28));
  cairo_set_line_width(cr, 2.4 * s);
  cairo_stroke(cr);
  cairo_rectangle(cr, X(38), Y(19), 24 * sx, 7 * sy);
  setRgba(cr, rgb(30, 20, 14), 0.35);
  cairo_fill(cr);
  cairo_move_to(cr, X(53), Y(25));
  cairo_line_to(cr, X(60), Y(24.5));
  setColor(cr, rgb(20, 14, 10));
  cairo_set_line_width(cr, 1.8 * s);
  cairo_stroke(cr);
  circle(cr, X(57.5), Y(25), 1.1 * s);
  setColor(cr, rgb(255, 230, 200));
  cairo_fill(cr);
  // The hat, pulled low.
  paintCowboyHat(cr, X(50), Y(20), 56 * sx, felt, 0.04);
  // The gun arm.
  if (drawn)
  {
    limb(cr, X(62), Y(54), X(76), Y(68), X(90), Y(74), 11 * s, coat);
    cairo_save(cr);
    cairo_translate(cr, X(90), Y(76));
    revolver(cr, 1.0 * s, steel);
    cairo_restore(cr);
    circle(cr, X(90), Y(75), 5.2 * s);
    fillRadial(cr, X(90), Y(75), 5.2 * s, skin, skinDark, 1.6);
  }
  else
  {
    const double twitch = hover && frame % 2 == 1 ? -4.0 : 0.0;
    const double hx = hover ? 72 : 76, hy = hover ? 86 + twitch : 104;
    limb(cr, X(64), Y(54), X(hover ? 80 : 74), Y(hover ? 72 : 80), X(hx), Y(hy), 11 * s, coat);
    circle(cr, X(hx), Y(hy + 2), 5.4 * s);
    fillRadial(cr, X(hx), Y(hy + 2), 5.4 * s, skin, skinDark, 1.6);
    if (hover)
      for (int f = 0; f < 3; ++f)
      {
        // Fingers spread over the grip, twitching.
        const double fa = 1.2 + f * 0.35 + (frame % 2 ? 0.25 : 0.0);
        cairo_move_to(cr, X(hx), Y(hy + 3));
        cairo_line_to(cr, X(hx) + std::cos(fa) * 8 * s, Y(hy + 3) + std::sin(fa) * 8 * s);
        setColor(cr, skinDark);
        cairo_set_line_width(cr, 2.4 * s);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_stroke(cr);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
      }
  }
}

// --- The Tumble Mine -----------------------------------------------------------------------

// A spiked iron mine bundled up in a tumbleweed. Variant 1: its lamp is lit
// red (the rattle).
void tumbleMine(cairo_t* cr, double w, double h, int variant, int frame)
{
  (void)frame;
  const double cx = kM + w * 0.5, cy = kM + h * 0.5, R = std::min(w, h) * 0.5;
  // The back of the tumbleweed.
  paintTumbleweed(cr, cx, cy, R * 0.96, 7u);
  // The mine: an iron ball with horns.
  const double mr = R * 0.42;
  for (int k = 0; k < 8; ++k)
  {
    const double a = k * kPi / 4.0 + kPi / 8.0;
    const double ux = std::cos(a), uy = std::sin(a);
    const double bx = cx + ux * mr * 0.85, by = cy + uy * mr * 0.85;
    const double tx = cx + ux * (mr + R * 0.2), ty = cy + uy * (mr + R * 0.2);
    cairo_move_to(cr, bx - uy * 4.5, by + ux * 4.5);
    cairo_line_to(cr, tx - uy * 2.5, ty + ux * 2.5);
    cairo_line_to(cr, tx + uy * 2.5, ty - ux * 2.5);
    cairo_line_to(cr, bx + uy * 4.5, by - ux * 4.5);
    cairo_close_path(cr);
    fillLinear(cr, bx, by, tx, ty, rgb(90, 92, 100), rgb(40, 40, 46), 1.4);
    circle(cr, tx, ty, 3.4);
    fillRadial(cr, tx, ty, 3.4, rgb(255, 226, 140), rgb(170, 110, 30), 1.2);
  }
  circle(cr, cx, cy, mr);
  fillRadial(cr, cx, cy, mr, rgb(120, 122, 132), rgb(28, 28, 34), 2.4);
  // The riveted seam.
  cairo_save(cr);
  cairo_translate(cr, cx, cy);
  cairo_scale(cr, 1.0, 0.32);
  cairo_arc(cr, 0, 0, mr, 0, kPi);
  cairo_restore(cr);
  setColor(cr, rgb(20, 20, 24));
  cairo_set_line_width(cr, 2.0);
  cairo_stroke(cr);
  for (int k = 1; k < 8; ++k)
  {
    const double a = k * kPi / 8.0;
    circle(cr, cx + std::cos(a) * mr, cy + std::sin(a) * mr * 0.32, 1.6);
    setColor(cr, rgb(170, 172, 184));
    cairo_fill(cr);
  }
  // A skull and crossbones, stencilled.
  setRgba(cr, rgb(235, 225, 200), 0.75);
  circle(cr, cx - 2, cy - mr * 0.35, mr * 0.2);
  cairo_fill(cr);
  for (const int side : {-1, 1})
  {
    cairo_move_to(cr, cx - mr * 0.3, cy - mr * 0.05 + side * mr * 0.1);
    cairo_line_to(cr, cx + mr * 0.26, cy - mr * 0.05 - side * mr * 0.1);
  }
  cairo_set_line_width(cr, 2.6);
  cairo_stroke(cr);
  circle(cr, cx - 6, cy - mr * 0.37, 1.8);
  circle(cr, cx + 2, cy - mr * 0.37, 1.8);
  setColor(cr, rgb(30, 30, 34));
  cairo_fill(cr);
  // The lamp on its crown.
  const bool lit = variant == 1;
  const double lx = cx, ly = cy - mr - 1;
  roundedRect(cr, lx - 6, ly - 2, 12, 5, 1.5);
  fillLinear(cr, 0, ly - 2, 0, ly + 3, rgb(150, 150, 160), rgb(50, 50, 60), 1.2);
  cairo_arc(cr, lx, ly - 2, 5.5, kPi, 2 * kPi);
  cairo_close_path(cr);
  fillRadial(cr, lx, ly - 4, 6, lit ? rgb(255, 210, 200) : rgb(150, 40, 40), lit ? rgb(255, 30, 30) : rgb(70, 10, 12), 1.2);
  if (lit)
    glow(cr, lx, ly - 4, 22, rgb(255, 40, 30), 0.85);
  // Twigs over the front.
  std::uint32_t sd = 991u;
  for (int i = 0; i < 18; ++i)
  {
    const double a0 = rnd(sd) * 2 * kPi, a1 = a0 + 0.6 + rnd(sd) * 1.4;
    const double r0 = R * (0.35 + rnd(sd) * 0.6), r1 = R * (0.35 + rnd(sd) * 0.6);
    const double mx = cx + std::cos((a0 + a1) * 0.5) * R * (0.2 + rnd(sd) * 0.9);
    const double my = cy + std::sin((a0 + a1) * 0.5) * R * (0.2 + rnd(sd) * 0.9);
    cairo_move_to(cr, cx + std::cos(a0) * r0, cy + std::sin(a0) * r0);
    cairo_curve_to(cr, mx, my, mx, my, cx + std::cos(a1) * r1, cy + std::sin(a1) * r1);
    setRgba(cr, i % 3 == 0 ? rgb(214, 178, 120) : rgb(160, 120, 74), 0.85);
    cairo_set_line_width(cr, 1.2 + rnd(sd) * 1.2);
    cairo_stroke(cr);
  }
}

// --- The Window Bandit ---------------------------------------------------------------------

// A bandit up behind a window sill (the level draws the sill over his
// waist): black hat, a red bandana over his face, his gun out and aimed.
void windowBandit(cairo_t* cr, double w, double h, int frame)
{
  const double s = std::min(w, h) / 96.0;
  auto X = [&](double x) { return kM + x * w / 96.0; };
  auto Y = [&](double y) { return kM + y * h / 96.0; };
  const Color shirt = rgb(120, 70, 50), vest = rgb(50, 42, 40), skin = rgb(214, 156, 110), skinDark = rgb(160, 104, 70);
  // Shoulders and chest.
  cairo_move_to(cr, X(14), Y(96));
  cairo_curve_to(cr, X(14), Y(66), X(24), Y(52), X(44), Y(52));
  cairo_curve_to(cr, X(64), Y(52), X(74), Y(66), X(74), Y(96));
  cairo_close_path(cr);
  fillLinear(cr, X(14), 0, X(74), 0, lighten(shirt, 0.15f), darken(shirt, 0.35f));
  cairo_move_to(cr, X(30), Y(56));
  cairo_line_to(cr, X(40), Y(56));
  cairo_line_to(cr, X(42), Y(96));
  cairo_line_to(cr, X(26), Y(96));
  cairo_close_path(cr);
  fillLinear(cr, 0, Y(56), 0, Y(96), lighten(vest, 0.15f), darken(vest, 0.3f), 1.4);
  // A bandolier.
  cairo_move_to(cr, X(22), Y(60));
  cairo_line_to(cr, X(66), Y(94));
  setColor(cr, rgb(90, 56, 30));
  cairo_set_line_width(cr, 7 * s);
  cairo_stroke(cr);
  for (int k = 0; k < 6; ++k)
  {
    const double t = (k + 0.5) / 6.0;
    circle(cr, X(22 + 44 * t), Y(60 + 34 * t), 1.8 * s);
    setColor(cr, rgb(230, 190, 90));
    cairo_fill(cr);
  }
  // The neck and head.
  limb(cr, X(44), Y(40), X(44), Y(46), X(44), Y(52), 9 * s, skinDark, 1.6);
  ellipse(cr, X(44), Y(32), 13 * s, 14 * s);
  fillRadial(cr, X(46), Y(32), 14 * s, skin, skinDark, 2.0);
  // The bandana over his nose (its tails flap).
  cairo_move_to(cr, X(30), Y(30));
  cairo_line_to(cr, X(58), Y(30));
  cairo_curve_to(cr, X(58), Y(40), X(52), Y(48), X(46), Y(50));
  cairo_curve_to(cr, X(38), Y(48), X(31), Y(40), X(30), Y(30));
  cairo_close_path(cr);
  fillLinear(cr, 0, Y(30), 0, Y(50), rgb(230, 64, 52), rgb(140, 24, 26), 1.6);
  for (int k = 0; k < 3; ++k)
  {
    circle(cr, X(38 + k * 7), Y(38 + (k % 2) * 4), 1.4 * s);
    setRgba(cr, rgb(255, 230, 220), 0.8);
    cairo_fill(cr);
  }
  const double flap = frame % 2 ? 3.0 : 0.0;
  cairo_move_to(cr, X(30), Y(31));
  cairo_line_to(cr, X(20), Y(36 + flap));
  cairo_line_to(cr, X(22), Y(30));
  cairo_close_path(cr);
  fillLinear(cr, 0, Y(30), 0, Y(38), rgb(220, 60, 50), rgb(140, 24, 26), 1.2);
  // Mean eyes.
  for (const double ex : {40.0, 51.0})
  {
    ellipse(cr, X(ex), Y(25), 3.6 * s, 2.2 * s);
    setColor(cr, rgb(250, 246, 236));
    cairo_fill(cr);
    circle(cr, X(ex + 1), Y(25.2), 1.4 * s);
    setColor(cr, rgb(20, 14, 10));
    cairo_fill(cr);
    cairo_move_to(cr, X(ex - 4), Y(20.5 + (ex > 45 ? 1.5 : 0)));
    cairo_line_to(cr, X(ex + 4), Y(21.5 - (ex > 45 ? 0.5 : -1)));
    cairo_set_line_width(cr, 2.0 * s);
    cairo_stroke(cr);
  }
  // The hat.
  paintCowboyHat(cr, X(44), Y(18), 52 * s, rgb(34, 28, 28), -0.05);
  // The gun arm, aimed.
  limb(cr, X(62), Y(60), X(74), Y(50), X(82), Y(40), 10 * s, shirt);
  cairo_save(cr);
  cairo_translate(cr, X(82), Y(41));
  cairo_rotate(cr, -0.12);
  revolver(cr, 0.95 * s, rgb(150, 154, 166));
  cairo_restore(cr);
  circle(cr, X(82), Y(40), 5 * s);
  fillRadial(cr, X(82), Y(40), 5 * s, skin, skinDark, 1.6);
}

// --- The Miracle Tonic ---------------------------------------------------------------------

// A green glass medicine bottle with a cork and a paper label, glowing a
// little from the inside (the Virus in it).
void tonicBottle(cairo_t* cr, double w, double h)
{
  const double s = std::min(w, h) / 64.0;
  auto X = [&](double x) { return kM + x * w / 64.0; };
  auto Y = [&](double y) { return kM + y * h / 64.0; };
  glow(cr, X(32), Y(42), 30 * s, rgb(120, 255, 70), 0.35);
  // The bottle.
  cairo_move_to(cr, X(28), Y(10));
  cairo_line_to(cr, X(36), Y(10));
  cairo_line_to(cr, X(36), Y(20));
  cairo_curve_to(cr, X(46), Y(24), X(48), Y(28), X(48), Y(34));
  cairo_line_to(cr, X(48), Y(58));
  cairo_curve_to(cr, X(48), Y(62), X(46), Y(63), X(42), Y(63));
  cairo_line_to(cr, X(22), Y(63));
  cairo_curve_to(cr, X(18), Y(63), X(16), Y(62), X(16), Y(58));
  cairo_line_to(cr, X(16), Y(34));
  cairo_curve_to(cr, X(16), Y(28), X(18), Y(24), X(28), Y(20));
  cairo_close_path(cr);
  fillLinear(cr, X(16), 0, X(48), 0, rgb(70, 130, 60), rgb(20, 56, 26), 2.0);
  // The tonic inside, bubbling green.
  cairo_rectangle(cr, X(19), Y(36), 26 * s, 24 * s);
  fillLinear(cr, 0, Y(36), 0, Y(60), rgb(150, 255, 90), rgb(40, 160, 40), 0);
  for (int k = 0; k < 5; ++k)
  {
    circle(cr, X(22 + k * 5), Y(52 - (k * 7) % 14), 1.3 * s);
    setRgba(cr, rgb(230, 255, 210), 0.8);
    cairo_fill(cr);
  }
  // The label.
  roundedRect(cr, X(18), Y(38), 28 * s, 18 * s, 1.5);
  fillLinear(cr, 0, Y(38), 0, Y(56), rgb(250, 238, 200), rgb(214, 190, 140), 1.2);
  cairo_rectangle(cr, X(20), Y(40), 24 * s, 14 * s);
  setColor(cr, rgb(190, 40, 40));
  cairo_set_line_width(cr, 1.0);
  cairo_stroke(cr);
  setColor(cr, rgb(140, 30, 30));
  centred(cr, "DR. FIZZ", X(32), Y(46), 5.5 * s);
  setColor(cr, rgb(40, 30, 20));
  centred(cr, "TONIC", X(32), Y(52.5), 6.0 * s);
  // The cork and a wax seal.
  roundedRect(cr, X(27), Y(3), 10 * s, 9 * s, 2);
  fillLinear(cr, X(27), 0, X(37), 0, rgb(220, 170, 110), rgb(140, 96, 52), 1.6);
  cairo_rectangle(cr, X(27), Y(10), 10 * s, 3 * s);
  fillLinear(cr, 0, Y(10), 0, Y(13), rgb(220, 60, 60), rgb(130, 20, 20), 1.0);
  // Glass highlights.
  cairo_move_to(cr, X(21), Y(32));
  cairo_line_to(cr, X(21), Y(58));
  setRgba(cr, rgb(255, 255, 255), 0.45);
  cairo_set_line_width(cr, 2.4 * s);
  cairo_stroke(cr);
  cairo_move_to(cr, X(30), Y(12));
  cairo_line_to(cr, X(30), Y(19));
  setRgba(cr, rgb(255, 255, 255), 0.4);
  cairo_set_line_width(cr, 1.4 * s);
  cairo_stroke(cr);
}

} // namespace

// --- Shared pieces --------------------------------------------------------------------------

void paintCowboyHat(cairo_t* cr, double cx, double brimY, double w, Color felt, double tilt)
{
  cairo_save(cr);
  cairo_translate(cr, cx, brimY);
  cairo_rotate(cr, tilt);
  const double hw = w * 0.5;
  // The crown: tall, pinched at the top, a dent in the middle.
  cairo_move_to(cr, -hw * 0.5, 0);
  cairo_curve_to(cr, -hw * 0.56, -w * 0.22, -hw * 0.5, -w * 0.36, -hw * 0.3, -w * 0.4);
  cairo_curve_to(cr, -hw * 0.12, -w * 0.36, -hw * 0.06, -w * 0.32, 0, -w * 0.33);
  cairo_curve_to(cr, hw * 0.06, -w * 0.32, hw * 0.14, -w * 0.38, hw * 0.3, -w * 0.4);
  cairo_curve_to(cr, hw * 0.5, -w * 0.36, hw * 0.56, -w * 0.22, hw * 0.5, 0);
  cairo_close_path(cr);
  fillLinear(cr, -hw * 0.5, 0, hw * 0.5, 0, lighten(felt, 0.25f), darken(felt, 0.35f), std::max(1.2, w * 0.035));
  // The band.
  cairo_move_to(cr, -hw * 0.52, -w * 0.04);
  cairo_curve_to(cr, -hw * 0.2, -w * 0.07, hw * 0.2, -w * 0.07, hw * 0.52, -w * 0.04);
  cairo_line_to(cr, hw * 0.53, -w * 0.11);
  cairo_curve_to(cr, hw * 0.2, -w * 0.14, -hw * 0.2, -w * 0.14, -hw * 0.53, -w * 0.11);
  cairo_close_path(cr);
  fillLinear(cr, 0, -w * 0.14, 0, -w * 0.04, rgb(200, 170, 110), rgb(120, 90, 50), 0);
  // The brim, curled up at both sides.
  cairo_move_to(cr, -hw, -w * 0.1);
  cairo_curve_to(cr, -hw * 0.8, w * 0.02, -hw * 0.4, w * 0.06, 0, w * 0.06);
  cairo_curve_to(cr, hw * 0.4, w * 0.06, hw * 0.8, w * 0.02, hw, -w * 0.1);
  cairo_curve_to(cr, hw * 0.8, -w * 0.03, hw * 0.5, -w * 0.01, 0, -w * 0.01);
  cairo_curve_to(cr, -hw * 0.5, -w * 0.01, -hw * 0.8, -w * 0.03, -hw, -w * 0.1);
  cairo_close_path(cr);
  fillLinear(cr, 0, -w * 0.1, 0, w * 0.06, lighten(felt, 0.3f), darken(felt, 0.3f), std::max(1.2, w * 0.035));
  // A sheen across the crown.
  cairo_move_to(cr, -hw * 0.34, -w * 0.3);
  cairo_curve_to(cr, -hw * 0.4, -w * 0.2, -hw * 0.38, -w * 0.12, -hw * 0.34, -w * 0.15);
  setRgba(cr, rgb(255, 255, 255), 0.25);
  cairo_set_line_width(cr, std::max(1.0, w * 0.03));
  cairo_stroke(cr);
  cairo_restore(cr);
}

void paintTumbleweed(cairo_t* cr, double cx, double cy, double rad, unsigned seed)
{
  std::uint32_t sd = seed * 2654435761u + 17u;
  if (sd == 0)
    sd = 1;
  // A faint body so it reads as a ball, then loops of twigs, darker inside.
  circle(cr, cx, cy, rad * 0.9);
  setRgba(cr, rgb(120, 86, 50), 0.22);
  cairo_fill(cr);
  for (int i = 0; i < 70; ++i)
  {
    const double depth = double(i) / 70.0;
    const double a0 = rnd(sd) * 2 * kPi, span = 0.8 + rnd(sd) * 1.8;
    const double r0 = rad * (0.3 + rnd(sd) * 0.7), r1 = rad * (0.3 + rnd(sd) * 0.7);
    const double bulge = rad * (0.6 + rnd(sd) * 0.5);
    const double am = a0 + span * 0.5;
    cairo_move_to(cr, cx + std::cos(a0) * r0, cy + std::sin(a0) * r0);
    cairo_curve_to(cr, cx + std::cos(a0 + span * 0.3) * bulge, cy + std::sin(a0 + span * 0.3) * bulge,
      cx + std::cos(am + span * 0.2) * bulge, cy + std::sin(am + span * 0.2) * bulge, cx + std::cos(a0 + span) * r1,
      cy + std::sin(a0 + span) * r1);
    const Color c = lerpColor(rgb(96, 66, 38), rgb(222, 186, 128), float(depth));
    setRgba(cr, c, 0.9);
    cairo_set_line_width(cr, std::max(0.8, rad * (0.018 + rnd(sd) * 0.02)));
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_stroke(cr);
  }
  // Little snapped-off twig ends sticking out of the ball.
  for (int i = 0; i < 16; ++i)
  {
    const double a = rnd(sd) * 2 * kPi, r0 = rad * 0.85, r1 = rad * (1.0 + rnd(sd) * 0.12);
    cairo_move_to(cr, cx + std::cos(a) * r0, cy + std::sin(a) * r0);
    cairo_line_to(cr, cx + std::cos(a + 0.12) * r1, cy + std::sin(a + 0.12) * r1);
    setRgba(cr, rgb(170, 130, 80), 0.9);
    cairo_set_line_width(cr, std::max(0.8, rad * 0.02));
    cairo_stroke(cr);
  }
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
}

void paintOutlawFace(cairo_t* cr, double cx, double cy, double s, bool sepia)
{
  auto tone = [&](Color c) {
    if (!sepia)
      return c;
    const int l = (redOf(c) * 30 + greenOf(c) * 59 + blueOf(c) * 11) / 100;
    return lerpColor(rgb(60, 34, 18), rgb(236, 214, 170), float(l) / 255.0f);
  };
  const Color skin = tone(rgb(222, 170, 128)), skinDark = tone(rgb(150, 100, 70));
  const double r = s * 0.5;
  // Shoulders and the bandana knot.
  cairo_move_to(cr, cx - r * 1.5, cy + r * 1.8);
  cairo_curve_to(cr, cx - r * 1.4, cy + r * 1.1, cx - r * 0.8, cy + r * 0.95, cx, cy + r * 0.95);
  cairo_curve_to(cr, cx + r * 0.8, cy + r * 0.95, cx + r * 1.4, cy + r * 1.1, cx + r * 1.5, cy + r * 1.8);
  cairo_close_path(cr);
  fillLinear(cr, cx - r, 0, cx + r, 0, tone(rgb(150, 112, 74)), tone(rgb(80, 54, 34)), std::max(1.0, s * 0.03));
  cairo_move_to(cr, cx - r * 0.55, cy + r * 0.85);
  cairo_line_to(cr, cx + r * 0.55, cy + r * 0.85);
  cairo_line_to(cr, cx, cy + r * 1.45);
  cairo_close_path(cr);
  fillLinear(cr, 0, cy + r * 0.85, 0, cy + r * 1.45, tone(rgb(220, 60, 50)), tone(rgb(130, 24, 24)), std::max(1.0, s * 0.03));
  // The head.
  ellipse(cr, cx, cy, r * 0.8, r * 0.95);
  fillRadial(cr, cx + r * 0.1, cy, r, skin, skinDark, std::max(1.0, s * 0.035));
  // Stubble.
  ellipse(cr, cx, cy + r * 0.5, r * 0.6, r * 0.35);
  setRgba(cr, tone(rgb(90, 60, 40)), 0.35);
  cairo_fill(cr);
  // Eyes in the hat's shade, squinting; a scar.
  cairo_rectangle(cr, cx - r * 0.8, cy - r * 0.5, r * 1.6, r * 0.35);
  setRgba(cr, tone(rgb(40, 26, 18)), 0.3);
  cairo_fill(cr);
  for (const int side : {-1, 1})
  {
    cairo_move_to(cr, cx + side * r * 0.5, cy - r * 0.2);
    cairo_line_to(cr, cx + side * r * 0.12, cy - r * 0.16);
    setColor(cr, tone(rgb(20, 14, 10)));
    cairo_set_line_width(cr, std::max(1.2, s * 0.05));
    cairo_stroke(cr);
    cairo_move_to(cr, cx + side * r * 0.56, cy - r * 0.36);
    cairo_line_to(cr, cx + side * r * 0.1, cy - r * 0.28);
    cairo_set_line_width(cr, std::max(1.2, s * 0.06));
    cairo_stroke(cr);
  }
  cairo_move_to(cr, cx + r * 0.3, cy + r * 0.05);
  cairo_line_to(cr, cx + r * 0.55, cy + r * 0.35);
  setColor(cr, tone(rgb(170, 70, 60)));
  cairo_set_line_width(cr, std::max(1.0, s * 0.03));
  cairo_stroke(cr);
  // The nose and the moustache.
  cairo_move_to(cr, cx, cy - r * 0.12);
  cairo_curve_to(cr, cx + r * 0.08, cy + r * 0.15, cx + r * 0.1, cy + r * 0.2, cx - r * 0.06, cy + r * 0.22);
  setColor(cr, skinDark);
  cairo_set_line_width(cr, std::max(1.0, s * 0.035));
  cairo_stroke(cr);
  cairo_move_to(cr, cx - r * 0.55, cy + r * 0.52);
  cairo_curve_to(cr, cx - r * 0.35, cy + r * 0.25, cx - r * 0.1, cy + r * 0.28, cx, cy + r * 0.34);
  cairo_curve_to(cr, cx + r * 0.1, cy + r * 0.28, cx + r * 0.35, cy + r * 0.25, cx + r * 0.55, cy + r * 0.52);
  cairo_curve_to(cr, cx + r * 0.3, cy + r * 0.42, cx + r * 0.1, cy + r * 0.44, cx, cy + r * 0.46);
  cairo_curve_to(cr, cx - r * 0.1, cy + r * 0.44, cx - r * 0.3, cy + r * 0.42, cx - r * 0.55, cy + r * 0.52);
  cairo_close_path(cr);
  setColor(cr, tone(rgb(70, 44, 28)));
  cairo_fill(cr);
  // The hat.
  paintCowboyHat(cr, cx, cy - r * 0.5, s * 1.25, tone(rgb(46, 36, 34)), 0.0);
}

void paintSaloonFront(cairo_t* cr, double w, double h, const char* sign)
{
  const Color plank = rgb(196, 150, 100), plankDark = rgb(120, 84, 50), trim = rgb(236, 220, 186);
  const double boardTop = h * 0.02, wallTop = h * 0.3, porchY = h * 0.52, deck = h * 0.92;
  // The false front: a tall board with a stepped, scrolled top.
  cairo_move_to(cr, w * 0.04, h);
  cairo_line_to(cr, w * 0.04, wallTop);
  cairo_line_to(cr, w * 0.12, wallTop);
  cairo_line_to(cr, w * 0.12, h * 0.16);
  cairo_line_to(cr, w * 0.3, h * 0.16);
  cairo_curve_to(cr, w * 0.34, h * 0.06, w * 0.42, boardTop + h * 0.02, w * 0.5, boardTop);
  cairo_curve_to(cr, w * 0.58, boardTop + h * 0.02, w * 0.66, h * 0.06, w * 0.7, h * 0.16);
  cairo_line_to(cr, w * 0.88, h * 0.16);
  cairo_line_to(cr, w * 0.88, wallTop);
  cairo_line_to(cr, w * 0.96, wallTop);
  cairo_line_to(cr, w * 0.96, h);
  cairo_close_path(cr);
  cairo_save(cr);
  cairo_clip_preserve(cr);
  fillLinear(cr, 0, 0, 0, h, lighten(plank, 0.1f), darken(plank, 0.2f), 0);
  // Horizontal clapboards with weathered grain.
  std::uint32_t sd = 4711u;
  for (double y = boardTop; y < h; y += h * 0.035)
  {
    cairo_rectangle(cr, 0, y, w, 1.6);
    setRgba(cr, plankDark, 0.55);
    cairo_fill(cr);
    for (int k = 0; k < 3; ++k)
    {
      const double gx = rnd(sd) * w, gl = w * (0.05 + rnd(sd) * 0.12);
      cairo_move_to(cr, gx, y + h * 0.012 + rnd(sd) * h * 0.012);
      cairo_line_to(cr, gx + gl, y + h * 0.014 + rnd(sd) * h * 0.012);
      setRgba(cr, plankDark, 0.3);
      cairo_set_line_width(cr, 1.0);
      cairo_stroke(cr);
    }
  }
  cairo_restore(cr);
  setColor(cr, kInk);
  cairo_set_line_width(cr, 3.0);
  cairo_stroke(cr);
  // The sign board.
  roundedRect(cr, w * 0.18, h * 0.07, w * 0.64, h * 0.17, 6);
  fillLinear(cr, 0, h * 0.07, 0, h * 0.24, rgb(130, 40, 30), rgb(70, 18, 14), 3.0);
  roundedRect(cr, w * 0.2, h * 0.085, w * 0.6, h * 0.14, 4);
  setColor(cr, rgb(232, 196, 110));
  cairo_set_line_width(cr, 2.0);
  cairo_stroke(cr);
  selectGameFont(cr);
  cairo_set_font_size(cr, h * 0.11);
  cairo_text_extents_t e;
  cairo_text_extents(cr, sign, &e);
  cairo_move_to(cr, w * 0.5 - e.width * 0.5 - e.x_bearing + 3, h * 0.2 + 3);
  setRgba(cr, rgb(0, 0, 0), 0.5);
  cairo_show_text(cr, sign);
  cairo_move_to(cr, w * 0.5 - e.width * 0.5 - e.x_bearing, h * 0.2);
  cairo_text_path(cr, sign);
  cairo_pattern_t* g = cairo_pattern_create_linear(0, h * 0.1, 0, h * 0.2);
  stop(g, 0, rgb(255, 240, 160));
  stop(g, 1, rgb(220, 150, 40));
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  setColor(cr, rgb(60, 20, 10));
  cairo_set_line_width(cr, 1.6);
  cairo_stroke(cr);
  // Upper windows.
  for (const double wx : {0.2, 0.62})
  {
    cairo_rectangle(cr, w * wx, h * 0.33, w * 0.18, h * 0.14);
    fillLinear(cr, 0, h * 0.33, 0, h * 0.47, rgb(255, 214, 140), rgb(200, 120, 60), 2.4);
    cairo_move_to(cr, w * (wx + 0.09), h * 0.33);
    cairo_line_to(cr, w * (wx + 0.09), h * 0.47);
    cairo_move_to(cr, w * wx, h * 0.4);
    cairo_line_to(cr, w * (wx + 0.18), h * 0.4);
    setColor(cr, plankDark);
    cairo_set_line_width(cr, 3.0);
    cairo_stroke(cr);
  }
  // The porch roof on its posts.
  cairo_move_to(cr, 0, porchY + h * 0.05);
  cairo_line_to(cr, w * 0.04, porchY - h * 0.01);
  cairo_line_to(cr, w * 0.96, porchY - h * 0.01);
  cairo_line_to(cr, w, porchY + h * 0.05);
  cairo_close_path(cr);
  fillLinear(cr, 0, porchY, 0, porchY + h * 0.05, rgb(170, 120, 74), rgb(96, 62, 36), 2.4);
  // The ground floor wall in shade under the porch.
  cairo_rectangle(cr, w * 0.04, porchY + h * 0.05, w * 0.92, deck - porchY - h * 0.05);
  setRgba(cr, rgb(40, 20, 10), 0.35);
  cairo_fill(cr);
  // Windows either side of the doors, warm light inside.
  for (const double wx : {0.1, 0.7})
  {
    cairo_rectangle(cr, w * wx, h * 0.62, w * 0.2, h * 0.2);
    fillLinear(cr, 0, h * 0.62, 0, h * 0.82, rgb(255, 210, 120), rgb(190, 100, 40), 2.4);
    for (int k = 1; k < 3; ++k)
    {
      cairo_move_to(cr, w * (wx + 0.2 * k / 3.0), h * 0.62);
      cairo_line_to(cr, w * (wx + 0.2 * k / 3.0), h * 0.82);
    }
    setColor(cr, plankDark);
    cairo_set_line_width(cr, 2.4);
    cairo_stroke(cr);
  }
  // The doorway and its batwing doors.
  cairo_rectangle(cr, w * 0.4, h * 0.6, w * 0.2, deck - h * 0.6);
  fillLinear(cr, 0, h * 0.6, 0, deck, rgb(255, 196, 110), rgb(120, 60, 24), 2.4);
  for (const int side : {0, 1})
  {
    const double dx = w * (0.405 + side * 0.1);
    cairo_move_to(cr, dx, h * 0.68);
    cairo_curve_to(cr, dx + w * 0.045, h * 0.66, dx + w * 0.045, h * 0.66, dx + w * 0.09, h * 0.68);
    cairo_line_to(cr, dx + w * 0.09, h * 0.83);
    cairo_line_to(cr, dx, h * 0.83);
    cairo_close_path(cr);
    fillLinear(cr, dx, 0, dx + w * 0.09, 0, lighten(plank, 0.15f), plankDark, 2.0);
    for (int k = 1; k < 4; ++k)
    {
      cairo_move_to(cr, dx + w * 0.0225 * k, h * 0.7);
      cairo_line_to(cr, dx + w * 0.0225 * k, h * 0.81);
    }
    setRgba(cr, plankDark, 0.8);
    cairo_set_line_width(cr, 1.4);
    cairo_stroke(cr);
  }
  // Posts.
  for (const double px : {0.06, 0.36, 0.64, 0.94})
  {
    cairo_rectangle(cr, w * px - w * 0.012, porchY + h * 0.04, w * 0.024, deck - porchY - h * 0.04);
    fillLinear(cr, w * px - w * 0.012, 0, w * px + w * 0.012, 0, trim, rgb(150, 120, 80), 2.0);
  }
  // The boardwalk.
  cairo_rectangle(cr, 0, deck, w, h - deck);
  fillLinear(cr, 0, deck, 0, h, rgb(180, 130, 80), rgb(100, 66, 36), 2.4);
  for (double x = 0; x < w; x += w * 0.06)
  {
    cairo_move_to(cr, x, deck);
    cairo_line_to(cr, x, h);
  }
  setRgba(cr, rgb(60, 36, 18), 0.6);
  cairo_set_line_width(cr, 1.4);
  cairo_stroke(cr);
}

void paintRevolver(cairo_t* cr, double s, Color metal) { revolver(cr, s, metal); }

bool drawWestArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame)
{
  (void)t;
  if (key == "duelist")
    duelist(cr, w, h, variant, frame);
  else if (key == "tumble_mine")
    tumbleMine(cr, w, h, variant, frame);
  else if (key == "window_bandit")
    windowBandit(cr, w, h, frame);
  else if (key == "tonic_bottle")
    tonicBottle(cr, w, h);
  else if (key == "duck_cowboy")
    paintCowboyHat(cr, kM + w * 38.0 / 64.0, kM + h * 15.0 / 64.0, w * 36.0 / 64.0, rgb(160, 104, 56), -0.15);
  else
    return false;
  return true;
}

} // namespace gr
