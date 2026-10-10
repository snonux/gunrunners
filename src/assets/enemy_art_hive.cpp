// Level 45, Hive Gullets (Episode 7, DEEP SPACE): the hive's aliens and its
// Gullet Tubes' mouths, valves and pores, in the style of enemy_art.cpp:
// each drawn in screen pixels into a texture with a 32 px margin, the cell
// box at (32, 32) .. (32 + w, 32 + h), facing right.

#include "assets/enemy_art_alien.hpp"

#include <cmath>
#include <map>

namespace gr
{

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr Color kInk = rgb(30, 6, 22);
constexpr double kLine = 2.4;
constexpr double kM = 32.0; // margin
const Color kLip = rgb(240, 110, 150);
const Color kLipDeep = rgb(150, 30, 80);
const Color kThroat = rgb(60, 6, 30);
const Color kBile = rgb(210, 240, 70);

void circle(cairo_t* cr, double x, double y, double r) { cairo_arc(cr, x, y, r, 0, 2 * kPi); }

void ellipse(cairo_t* cr, double x, double y, double rx, double ry)
{
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_scale(cr, rx, ry);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_restore(cr);
}

void setRgba(cairo_t* cr, Color c, double a)
{
  cairo_set_source_rgba(cr, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, a);
}

void radialFill(cairo_t* cr, double x, double y, double r, Color inner, Color outer)
{
  cairo_pattern_t* p = cairo_pattern_create_radial(x - r * 0.3, y - r * 0.35, r * 0.1, x, y, r);
  cairo_pattern_add_color_stop_rgb(p, 0, redOf(inner) / 255.0, greenOf(inner) / 255.0, blueOf(inner) / 255.0);
  cairo_pattern_add_color_stop_rgb(p, 1, redOf(outer) / 255.0, greenOf(outer) / 255.0, blueOf(outer) / 255.0);
  cairo_set_source(cr, p);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(p);
}

void gloss(cairo_t* cr, double x, double y, double rx, double ry, double a)
{
  ellipse(cr, x - rx * 0.32, y - ry * 0.42, rx * 0.34, ry * 0.18);
  setRgba(cr, rgb(255, 255, 255), a);
  cairo_fill(cr);
}

struct Box
{
  cairo_t* cr;
  const Theme& t;
  double w, h;
  int variant, frame;
};

// Hive Mite: a fat red tick on six short legs with big pale mandibles and
// two glowing yellow eyes.
void mite(const Box& b)
{
  cairo_t* cr = b.cr;
  const double cx = kM + b.w * 0.45, cy = kM + b.h * 0.58;
  const double r = b.w * 0.34;
  for (int near = 0; near < 2; ++near)
    for (int i = 0; i < 3; ++i)
    {
      const double hx = cx - r * 0.5 + double(i) * r * 0.5;
      const double swing = ((i + b.frame + near) % 2 == 0 ? 4.0 : -4.0);
      strokeLimb(cr, {{hx, cy}, {hx - 3 + swing * 0.5, cy + r * 0.55}, {hx - 6 + swing, cy + r * 1.05}}, near ? 3.6 : 3.0,
        near ? rgb(170, 50, 60) : rgb(110, 26, 40), kInk, 1.2);
    }
  ellipse(cr, cx, cy, r * 1.05, r * 0.8);
  radialFill(cr, cx, cy, r, rgb(255, 110, 100), rgb(150, 20, 40));
  setColor(cr, kInk);
  cairo_set_line_width(cr, kLine);
  cairo_stroke(cr);
  // Back plates.
  for (int i = 0; i < 2; ++i)
  {
    cairo_arc(cr, cx - r * 0.2 + double(i) * r * 0.4, cy - r * 0.1, r * 0.55, kPi * 1.15, kPi * 1.75);
    setRgba(cr, rgb(255, 190, 170), 0.5);
    cairo_set_line_width(cr, 2.0);
    cairo_stroke(cr);
  }
  gloss(cr, cx, cy, r * 1.05, r * 0.8, 0.45);
  // Head, mandibles and eyes.
  const double hx = cx + r * 0.95, hy = cy - r * 0.05;
  for (int s : {-1, 1})
  {
    cairo_move_to(cr, hx + 2, hy + double(s) * 3);
    cairo_curve_to(cr, hx + 10, hy + double(s) * 2, hx + 14, hy + double(s) * 7, hx + 10, hy + double(s) * 11);
    cairo_set_line_width(cr, 3.0);
    setColor(cr, rgb(250, 230, 200));
    cairo_stroke(cr);
  }
  circle(cr, hx, hy, r * 0.42);
  fillOutline(cr, rgb(120, 20, 40), kInk, 1.8);
  for (int s : {-1, 1})
  {
    radialGlow(cr, hx + 3, hy - 3 + double(s) * 4, 6, rgb(255, 230, 60), 0.6);
    circle(cr, hx + 3, hy - 3 + double(s) * 4, 2.4);
    setColor(cr, rgb(255, 250, 150));
    cairo_fill(cr);
  }
}

// Polyp: a fleshy bulb growing out of the wall (on its left), its round
// mouth on the right. Variant 0 shut, 1 puckering (the tell), 2 breathing
// in, wide open.
void polyp(const Box& b)
{
  cairo_t* cr = b.cr;
  const double x0 = kM, y0 = kM, w = b.w, h = b.h;
  const double wob = b.frame ? 2.0 : -2.0;
  // The stalk out of the wall and the bulb.
  cairo_move_to(cr, x0 - 6, y0 + h * 0.1);
  cairo_curve_to(cr, x0 + w * 0.5, y0 - 6 + wob, x0 + w * 1.05, y0 + h * 0.15, x0 + w * 1.02, y0 + h * 0.5);
  cairo_curve_to(cr, x0 + w * 1.05, y0 + h * 0.85, x0 + w * 0.5, y0 + h + 6 - wob, x0 - 6, y0 + h * 0.9);
  cairo_close_path(cr);
  fillGradientOutline(cr, y0, y0 + h, rgb(250, 150, 190), rgb(150, 40, 100), kInk, kLine);
  // Veins.
  for (int i = 0; i < 3; ++i)
  {
    const double y = y0 + h * (0.25 + 0.25 * double(i));
    cairo_move_to(cr, x0, y);
    cairo_curve_to(cr, x0 + w * 0.3, y - 8, x0 + w * 0.5, y + 8, x0 + w * 0.7, y - 2);
    setRgba(cr, rgb(120, 220, 255), 0.6);
    cairo_set_line_width(cr, 2.0);
    cairo_stroke(cr);
  }
  gloss(cr, x0 + w * 0.5, y0 + h * 0.45, w * 0.5, h * 0.45, 0.3);
  // The mouth.
  const double mx = x0 + w * 0.86, my = y0 + h * 0.5;
  const double open = b.variant == 2 ? 1.0 : (b.variant == 1 ? 0.25 : 0.08);
  ellipse(cr, mx, my, 12 + 6 * open, 22 + 10 * open);
  fillOutline(cr, kLip, kInk, kLine);
  ellipse(cr, mx + 2, my, 4 + 10 * open, 6 + 22 * open);
  setColor(cr, kThroat);
  cairo_fill(cr);
  if (b.variant == 2)
  {
    // Rings of throat going back, and teeth.
    for (int i = 1; i <= 2; ++i)
    {
      ellipse(cr, mx + 2, my, 10.0 - 3.0 * i, 24.0 - 7.0 * i);
      setRgba(cr, rgb(200, 60, 110), 0.6);
      cairo_set_line_width(cr, 2.0);
      cairo_stroke(cr);
    }
    for (int i = 0; i < 6; ++i)
    {
      const double a = -kPi * 0.5 + double(i) * kPi / 5.0;
      const double tx = mx + 2 + std::cos(a) * 13, ty = my + std::sin(a) * 28;
      cairo_move_to(cr, tx, ty);
      cairo_line_to(cr, tx - std::cos(a) * 6, ty - std::sin(a) * 6 + 2);
      cairo_line_to(cr, tx - std::cos(a) * 6, ty - std::sin(a) * 6 - 2);
      cairo_close_path(cr);
      setColor(cr, rgb(255, 245, 225));
      cairo_fill(cr);
    }
  }
  else if (b.variant == 1)
    radialGlow(cr, mx, my, 26, rgb(255, 120, 200), 0.6);
  // Creases around the lips.
  for (int i = 0; i < 8; ++i)
  {
    const double a = double(i) * kPi / 4.0;
    cairo_move_to(cr, mx + std::cos(a) * (14 + 6 * open), my + std::sin(a) * (24 + 10 * open));
    cairo_line_to(cr, mx + std::cos(a) * (19 + 6 * open), my + std::sin(a) * (30 + 10 * open));
  }
  setRgba(cr, kLipDeep, 0.8);
  cairo_set_line_width(cr, 1.6);
  cairo_stroke(cr);
}

// Drone Warden: an armoured flying beetle, plated in dark teal chitin, with
// buzzing wings and a glowing belly sac (bright while it calls its mites).
void warden(const Box& b)
{
  cairo_t* cr = b.cr;
  const double cx = kM + b.w * 0.5, cy = kM + b.h * 0.5;
  // Wings, a blur that flaps by frame.
  for (int s : {-1, 1})
  {
    cairo_save(cr);
    cairo_translate(cr, cx - b.w * 0.05, cy - b.h * 0.25);
    cairo_rotate(cr, double(s) * (b.frame ? 0.5 : 0.2) - 0.3);
    ellipse(cr, -b.w * 0.18, -b.h * 0.25, b.w * 0.3, b.h * 0.16);
    setRgba(cr, rgb(200, 240, 255), 0.35);
    cairo_fill_preserve(cr);
    setRgba(cr, rgb(230, 250, 255), 0.7);
    cairo_set_line_width(cr, 1.4);
    cairo_stroke(cr);
    cairo_restore(cr);
  }
  // The belly sac.
  ellipse(cr, cx - b.w * 0.05, cy + b.h * 0.18, b.w * 0.24, b.h * 0.24);
  const bool calling = b.variant == 1;
  radialFill(cr, cx - b.w * 0.05, cy + b.h * 0.18, b.w * 0.24, calling ? rgb(255, 250, 170) : rgb(240, 150, 120),
    calling ? rgb(240, 120, 40) : rgb(140, 40, 60));
  setColor(cr, kInk);
  cairo_set_line_width(cr, kLine);
  cairo_stroke(cr);
  if (calling)
    radialGlow(cr, cx - b.w * 0.05, cy + b.h * 0.2, b.w * 0.4, rgb(255, 200, 80), 0.7);
  // The armour: three overlapping plates.
  for (int i = 2; i >= 0; --i)
  {
    const double px = cx - b.w * 0.28 + double(i) * b.w * 0.18;
    cairo_move_to(cr, px - b.w * 0.16, cy + b.h * 0.05);
    cairo_curve_to(cr, px - b.w * 0.16, cy - b.h * 0.42, px + b.w * 0.16, cy - b.h * 0.42, px + b.w * 0.16, cy + b.h * 0.05);
    cairo_close_path(cr);
    fillGradientOutline(cr, cy - b.h * 0.4, cy + b.h * 0.05, rgb(70, 170, 170), rgb(20, 60, 80), kInk, kLine);
    cairo_move_to(cr, px - b.w * 0.1, cy - b.h * 0.22);
    cairo_curve_to(cr, px - b.w * 0.05, cy - b.h * 0.3, px + b.w * 0.05, cy - b.h * 0.3, px + b.w * 0.1, cy - b.h * 0.22);
    setRgba(cr, rgb(180, 255, 240), 0.6);
    cairo_set_line_width(cr, 2.0);
    cairo_stroke(cr);
  }
  // The head: horned, with a red eye band.
  const double hx = cx + b.w * 0.36, hy = cy - b.h * 0.08;
  cairo_move_to(cr, hx - 8, hy - 10);
  cairo_curve_to(cr, hx + 6, hy - 26, hx + 18, hy - 22, hx + 22, hy - 30);
  cairo_curve_to(cr, hx + 20, hy - 14, hx + 18, hy, hx + 8, hy + 10);
  cairo_curve_to(cr, hx, hy + 12, hx - 10, hy + 4, hx - 8, hy - 10);
  fillOutline(cr, rgb(40, 110, 120), kInk, kLine);
  cairo_rectangle(cr, hx, hy - 6, 14, 5);
  setColor(cr, rgb(255, 60, 60));
  cairo_fill(cr);
  radialGlow(cr, hx + 8, hy - 4, 12, rgb(255, 60, 60), 0.6);
  // Dangling legs.
  for (int i = 0; i < 3; ++i)
  {
    const double lx = cx - b.w * 0.2 + double(i) * b.w * 0.16;
    strokeLimb(cr, {{lx, cy + b.h * 0.08}, {lx - 4, cy + b.h * 0.3}, {lx + 2, cy + b.h * 0.48}}, 3.0, rgb(30, 80, 90), kInk,
      1.0);
  }
}

// A mouth's ring of lips. (cx, cy) its middle, rx/ry its size; `open` 0..1.2
// (shut .. gulping).
void lips(cairo_t* cr, double cx, double cy, double rx, double ry, double open, bool sideways, int frame)
{
  const double wob = frame ? 1.06 : 1.0;
  radialGlow(cr, cx, cy, std::max(rx, ry) * 1.6, rgb(255, 90, 140), 0.25 + 0.2 * open);
  ellipse(cr, cx, cy, rx * wob, ry * wob);
  radialFill(cr, cx, cy, std::max(rx, ry), rgb(255, 180, 200), kLipDeep);
  setColor(cr, kInk);
  cairo_set_line_width(cr, kLine);
  cairo_stroke(cr);
  // Creases running into the middle.
  for (int i = 0; i < 12; ++i)
  {
    const double a = double(i) * kPi / 6.0;
    cairo_move_to(cr, cx + std::cos(a) * rx * 0.9, cy + std::sin(a) * ry * 0.9);
    cairo_line_to(cr, cx + std::cos(a) * rx * (0.35 + 0.3 * open), cy + std::sin(a) * ry * (0.35 + 0.3 * open));
  }
  setRgba(cr, kLipDeep, 0.7);
  cairo_set_line_width(cr, 1.8);
  cairo_stroke(cr);
  // The hole.
  const double hx = rx * (sideways ? 0.18 + 0.5 * open : 0.12 + 0.62 * open);
  const double hy = ry * (sideways ? 0.12 + 0.62 * open : 0.18 + 0.5 * open);
  ellipse(cr, cx, cy, std::max(1.5, hx), std::max(1.5, hy));
  cairo_pattern_t* p = cairo_pattern_create_radial(cx, cy, 0, cx, cy, std::max(hx, hy));
  cairo_pattern_add_color_stop_rgb(p, 0, 0.05, 0.0, 0.03);
  cairo_pattern_add_color_stop_rgb(p, 1, redOf(kThroat) / 255.0, greenOf(kThroat) / 255.0, blueOf(kThroat) / 255.0);
  cairo_set_source(cr, p);
  cairo_fill(cr);
  cairo_pattern_destroy(p);
  if (open > 0.5)
    for (int i = 1; i <= 2; ++i)
    {
      ellipse(cr, cx, cy, hx * (1.0 - 0.3 * i), hy * (1.0 - 0.3 * i));
      setRgba(cr, rgb(220, 70, 120), 0.55);
      cairo_set_line_width(cr, 2.0);
      cairo_stroke(cr);
    }
  gloss(cr, cx, cy, rx, ry, 0.4);
}

double openOf(int variant) { return variant == 3 ? 1.2 : (variant == 2 ? 1.0 : (variant == 1 ? 0.45 : 0.0)); }

// A Gullet Tube's mouth in a wall (opening to the right).
void mouth(const Box& b)
{
  lips(b.cr, kM + b.w - 4, kM + b.h * 0.5, 22, 58, openOf(b.variant), true, b.frame);
}

// A mouth in a floor (opening upward).
void mouthUp(const Box& b)
{
  lips(b.cr, kM + b.w * 0.5, kM + 4, 58, 20, openOf(b.variant), false, b.frame);
}

// A small pore you could miss: the way into a secret.
void pore(const Box& b)
{
  const double cx = kM + b.w - 2, cy = kM + b.h * 0.62;
  ellipse(b.cr, cx, cy, 9, 16);
  setRgba(b.cr, rgb(90, 20, 50), 0.9);
  cairo_fill(b.cr);
  ellipse(b.cr, cx, cy, 4, 9);
  setColor(b.cr, rgb(20, 0, 10));
  cairo_fill(b.cr);
}

// Where a tube lets go: a slack lip, wetter just after it spat.
void lip(const Box& b)
{
  cairo_t* cr = b.cr;
  const double cx = kM + b.w - 4, cy = kM + b.h * 0.5;
  ellipse(cr, cx, cy, 14, 44);
  radialFill(cr, cx, cy, 44, rgb(250, 160, 190), kLipDeep);
  setColor(cr, kInk);
  cairo_set_line_width(cr, kLine);
  cairo_stroke(cr);
  ellipse(cr, cx + 2, cy, 5 + 3 * b.variant, 30 + 6 * b.variant);
  setColor(cr, kThroat);
  cairo_fill(cr);
  // A drool hanging from it.
  cairo_move_to(cr, cx + 6, cy + 30);
  cairo_curve_to(cr, cx + 10, cy + 40, cx + 6, cy + 48, cx + 8, cy + 56 + 8 * b.variant);
  cairo_set_line_width(cr, 4.0);
  setRgba(cr, rgb(255, 200, 220), 0.8);
  cairo_stroke(cr);
}

void lipUp(const Box& b)
{
  cairo_t* cr = b.cr;
  const double cx = kM + b.w * 0.5, cy = kM + 4;
  ellipse(cr, cx, cy, 44, 12);
  radialFill(cr, cx, cy, 44, rgb(250, 160, 190), kLipDeep);
  setColor(cr, kInk);
  cairo_set_line_width(cr, kLine);
  cairo_stroke(cr);
  ellipse(cr, cx, cy, 30 + 6 * b.variant, 4 + 3 * b.variant);
  setColor(cr, kThroat);
  cairo_fill(cr);
}

// A valve on a tube: a knot of muscle with an iris, pink for one branch,
// bile-yellow for the other.
void valve(const Box& b)
{
  cairo_t* cr = b.cr;
  const double cx = kM + b.w * 0.5, cy = kM + b.h * 0.5, r = b.w * 0.42;
  circle(cr, cx, cy, r);
  radialFill(cr, cx, cy, r, rgb(240, 170, 190), rgb(120, 30, 70));
  setColor(cr, kInk);
  cairo_set_line_width(cr, kLine);
  cairo_stroke(cr);
  // Muscle bands.
  for (int i = 0; i < 6; ++i)
  {
    const double a = double(i) * kPi / 3.0 + (b.frame ? 0.1 : 0.0);
    cairo_move_to(cr, cx + std::cos(a) * r * 0.55, cy + std::sin(a) * r * 0.55);
    cairo_line_to(cr, cx + std::cos(a) * r * 0.95, cy + std::sin(a) * r * 0.95);
  }
  setRgba(cr, rgb(110, 20, 60), 0.7);
  cairo_set_line_width(cr, 3.0);
  cairo_stroke(cr);
  const Color iris = b.variant == 0 ? rgb(255, 110, 170) : kBile;
  radialGlow(cr, cx, cy, r * 0.9, iris, 0.7);
  circle(cr, cx, cy, r * 0.45);
  fillOutline(cr, iris, kInk, 2.0);
  // The pupil: a slit turned toward the branch it opens.
  cairo_save(cr);
  cairo_translate(cr, cx, cy);
  cairo_rotate(cr, b.variant == 0 ? 0.0 : kPi * 0.5);
  ellipse(cr, 0, 0, r * 0.12, r * 0.36);
  setColor(cr, rgb(20, 0, 10));
  cairo_fill(cr);
  cairo_restore(cr);
  gloss(cr, cx, cy, r, r, 0.45);
}

// A wall pore that spits mites (opening to the right): a crusty ring;
// variant 1 rattling, eyes glinting inside.
void mitePore(const Box& b)
{
  cairo_t* cr = b.cr;
  const double cx = kM + b.w * 0.5, cy = kM + b.h * 0.7;
  ellipse(cr, cx, cy, 15, 22);
  fillOutline(cr, rgb(170, 90, 110), kInk, kLine);
  for (int i = 0; i < 7; ++i)
  {
    const double a = double(i) * 2.0 * kPi / 7.0;
    circle(cr, cx + std::cos(a) * 13, cy + std::sin(a) * 19, 4.0);
    fillOutline(cr, rgb(200, 120, 130), kInk, 1.0);
  }
  ellipse(cr, cx, cy, 8, 13);
  setColor(cr, rgb(30, 0, 12));
  cairo_fill(cr);
  if (b.variant == 1)
  {
    radialGlow(cr, cx, cy, 20, rgb(255, 60, 60), 0.6);
    for (int s : {-1, 1})
    {
      circle(cr, cx + double(s) * 3, cy - 3 + (b.frame ? 2 : 0), 1.8);
      setColor(cr, rgb(255, 240, 100));
      cairo_fill(cr);
    }
  }
}

// The green dripping pore in a ceiling: where the Virus comes from.
void dripPore(const Box& b)
{
  cairo_t* cr = b.cr;
  const double cx = kM + b.w * 0.5, cy = kM;
  ellipse(cr, cx, cy, 26, 12);
  fillOutline(cr, rgb(120, 160, 70), kInk, kLine);
  ellipse(cr, cx, cy + 2, 14, 6);
  setColor(cr, rgb(20, 40, 10));
  cairo_fill(cr);
  for (int i = 0; i < 3; ++i)
  {
    const double x = cx - 12 + double(i) * 12, len = 10 + double((i + b.frame) % 3) * 8;
    cairo_move_to(cr, x - 3, cy + 4);
    cairo_curve_to(cr, x - 3, cy + len, x + 3, cy + len, x + 3, cy + 4);
    setRgba(cr, rgb(150, 255, 90), 0.85);
    cairo_fill(cr);
  }
  radialGlow(cr, cx, cy + 10, 30, rgb(150, 255, 90), 0.4);
}

// A Bile Blaster gob: yellow-green, wobbling, with a drop behind it.
void bileBlob(const Box& b)
{
  cairo_t* cr = b.cr;
  const double cx = kM + b.w * 0.6, cy = kM + b.h * 0.5;
  const double sq = b.frame ? 1.1 : 0.92;
  ellipse(cr, cx, cy, 11 * sq, 9 / sq);
  radialFill(cr, cx, cy, 11, rgb(245, 255, 170), rgb(150, 190, 30));
  setColor(cr, rgb(60, 70, 10));
  cairo_set_line_width(cr, 1.8);
  cairo_stroke(cr);
  circle(cr, cx - 15, cy + 1, 4);
  setColor(cr, rgb(200, 235, 60));
  cairo_fill(cr);
  circle(cr, cx - 23, cy + 2, 2.4);
  cairo_fill(cr);
  gloss(cr, cx, cy, 11, 9, 0.6);
}

using DrawFn = void (*)(const Box&);

} // namespace

bool drawHiveArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame)
{
  static const std::map<std::string, DrawFn> kRoutines{
    {"hive_mite", mite},
    {"polyp", polyp},
    {"drone_warden", warden},
    {"gullet_mouth", mouth},
    {"gullet_mouth_up", mouthUp},
    {"gullet_pore", pore},
    {"gullet_lip", lip},
    {"gullet_lip_up", lipUp},
    {"gullet_valve", valve},
    {"mite_pore", mitePore},
    {"drip_pore", dripPore},
    {"bile_blob", bileBlob},
  };
  const auto it = kRoutines.find(key);
  if (it == kRoutines.end())
    return false;
  it->second(Box{cr, t, w, h, variant, frame});
  return true;
}

} // namespace gr
