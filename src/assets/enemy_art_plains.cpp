// Level 48, Bounder Plains (Episode 7, DEEP SPACE): the Thorn Hog, the Sky
// Gulper and the thornbushes, in the style of enemy_art.cpp: each drawn in
// screen pixels into a texture with a 32 px margin, the cell box at
// (32, 32) .. (32 + w, 32 + h), facing right. (The Bounders are vehicles:
// vehicle_art.cpp.)

#include "assets/enemy_art_alien.hpp"

#include "base/math.hpp"

#include <algorithm>
#include <cmath>
#include <map>

namespace gr
{

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr Color kInk = rgb(36, 20, 40);
constexpr double kLine = 2.4;
constexpr double kM = 32.0; // margin

Color lighten(Color c, float t) { return lerpColor(c, rgb(255, 255, 255), t); }
Color darken(Color c, float t) { return lerpColor(c, rgb(0, 0, 0), t); }

void circle(cairo_t* cr, double x, double y, double r) { cairo_arc(cr, x, y, r, 0, 2 * kPi); }

void ellipse(cairo_t* cr, double x, double y, double rx, double ry)
{
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_scale(cr, rx, ry);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_restore(cr);
}

struct Box
{
  cairo_t* cr;
  const Theme& t;
  double w, h;
  int variant, frame;
};

void thorn(cairo_t* cr, double x, double y, double len, double angle, Color c)
{
  const double s = std::sin(angle), co = std::cos(angle);
  cairo_move_to(cr, x - s * 4, y + co * 4);
  cairo_line_to(cr, x + co * len, y + s * len);
  cairo_line_to(cr, x + s * 4, y - co * 4);
  cairo_close_path(cr);
  fillOutline(cr, c, kInk, 1.4);
}

// Thorn Hog: a squat violet boar with a ridge of thorns down its back and
// curling tusks. 1 pawing the ground (tusks down), 2 charging (dust,
// stretched), 3 dazed (stars).
void thornHog(const Box& b)
{
  cairo_t* cr = b.cr;
  const double x0 = kM, y0 = kM, w = b.w, h = b.h;
  const Color hide = rgb(132, 82, 150);
  const bool paw = b.variant == 1, charge = b.variant == 2, dazed = b.variant == 3;
  const double lean = charge ? 0.06 : (paw ? 0.03 : 0.0);
  const double step = b.frame % 2 ? 4.0 : -4.0;
  // Legs.
  for (int k = 0; k < 4; ++k)
  {
    const double lx = x0 + w * (0.2 + 0.18 * k) + (k % 2 ? step : -step) * (charge ? 1.6 : 1.0);
    cairo_rectangle(cr, lx - 5, y0 + h * 0.66, 10, h * 0.34);
    fillOutline(cr, darken(hide, k < 2 ? 0.35f : 0.2f), kInk, 1.6);
  }
  // Body.
  ellipse(cr, x0 + w * (0.46 + lean), y0 + h * 0.5, w * 0.4, h * 0.3);
  fillGradientOutline(cr, y0 + h * 0.2, y0 + h * 0.8, lighten(hide, 0.2f), darken(hide, 0.25f), kInk, kLine);
  // Thorns along the back.
  for (int k = 0; k < 6; ++k)
  {
    const double u = 0.18 + k * 0.11;
    const double tx = x0 + w * (u + lean), ty = y0 + h * (0.24 + 0.08 * std::pow(std::abs(u - 0.46) * 2.4, 2.0));
    thorn(cr, tx, ty, 10 + (k % 2) * 5, -kPi / 2 - 0.5 + k * 0.12, rgb(220, 170, 255));
  }
  // Head with a snout and tusks.
  const double hx = x0 + w * (0.84 + lean), hy = y0 + h * (paw || charge ? 0.58 : 0.46);
  ellipse(cr, hx, hy, w * 0.16, h * 0.22);
  fillGradientOutline(cr, hy - h * 0.22, hy + h * 0.22, lighten(hide, 0.25f), hide, kInk, kLine);
  ellipse(cr, hx + w * 0.13, hy + h * 0.06, w * 0.06, h * 0.09);
  fillOutline(cr, rgb(220, 150, 200), kInk, 1.6);
  for (int k = 0; k < 2; ++k)
  {
    cairo_move_to(cr, hx + w * 0.08, hy + h * 0.12);
    cairo_curve_to(cr, hx + w * 0.18, hy + h * 0.3, hx + w * (0.24 + k * 0.02), hy + h * 0.06, hx + w * 0.2,
      hy - h * 0.06);
  }
  cairo_set_line_width(cr, 5.0);
  setColor(cr, kInk);
  cairo_stroke_preserve(cr);
  cairo_set_line_width(cr, 3.0);
  setColor(cr, rgb(255, 246, 220));
  cairo_stroke(cr);
  // Eye: angry when it means it, crossed when dazed.
  const double ex = hx + w * 0.02, ey = hy - h * 0.08;
  if (dazed)
  {
    for (int k = -1; k <= 1; k += 2)
    {
      cairo_move_to(cr, ex - 4, ey - 4 * k);
      cairo_line_to(cr, ex + 4, ey + 4 * k);
    }
    cairo_set_line_width(cr, 2.0);
    setColor(cr, kInk);
    cairo_stroke(cr);
    for (int k = 0; k < 3; ++k)
    {
      const double a = b.frame * 0.9 + k * 2.1;
      circle(cr, hx + std::cos(a) * w * 0.14, y0 + h * 0.08 + std::sin(a) * 5, 4);
      setColor(cr, rgb(255, 240, 120));
      cairo_fill(cr);
    }
  }
  else
  {
    circle(cr, ex, ey, 4.5);
    setColor(cr, charge || paw ? rgb(255, 80, 60) : rgb(255, 230, 120));
    cairo_fill(cr);
    cairo_move_to(cr, ex - 7, ey - 8);
    cairo_line_to(cr, ex + 6, ey - 4);
    cairo_set_line_width(cr, 2.4);
    setColor(cr, kInk);
    cairo_stroke(cr);
  }
  if (charge)
    for (int k = 0; k < 3; ++k)
    {
      circle(cr, x0 - 6 - k * 10, y0 + h * 0.9 - k * 4, 6 - k);
      setColor(cr, rgba(200, 170, 210, 140));
      cairo_fill(cr);
    }
}

// Sky Gulper: a floating balloon of a mouth with little fins, its lips
// open wide (variant 1: shut tight on a mouthful, cheeks puffed).
void skyGulper(const Box& b)
{
  cairo_t* cr = b.cr;
  const double x0 = kM, y0 = kM, w = b.w, h = b.h;
  const Color skin = rgb(240, 120, 170);
  const bool shut = b.variant == 1;
  const double cx = x0 + w * 0.5, cy = y0 + h * 0.5;
  const double puff = shut ? 1.12 : 1.0;
  // Fins, fluttering.
  const double flap = std::sin(b.frame * 1.3) * 8.0;
  for (int k = -1; k <= 1; k += 2)
  {
    cairo_move_to(cr, cx + k * w * 0.1, cy - h * 0.38);
    cairo_line_to(cr, cx + k * w * 0.32, cy - h * 0.62 - flap);
    cairo_line_to(cr, cx + k * w * 0.36, cy - h * 0.34);
    cairo_close_path(cr);
    fillOutline(cr, rgb(170, 220, 255), kInk, 1.6);
  }
  // A dangling tail.
  cairo_move_to(cr, cx - w * 0.1, cy + h * 0.4);
  cairo_curve_to(cr, cx - w * 0.2, cy + h * 0.6, cx + w * 0.1 + flap, cy + h * 0.6, cx, cy + h * 0.75);
  cairo_set_line_width(cr, 4.0);
  setColor(cr, darken(skin, 0.3f));
  cairo_stroke(cr);
  ellipse(cr, cx, cy, w * 0.46 * puff, h * 0.42 * puff);
  fillGradientOutline(cr, cy - h * 0.42, cy + h * 0.42, lighten(skin, 0.3f), darken(skin, 0.2f), kInk, kLine);
  // Spots.
  for (int k = 0; k < 5; ++k)
  {
    circle(cr, cx - w * 0.3 + k * w * 0.13, cy - h * 0.26 + (k % 2) * 6, 3 + (k % 3));
    setColor(cr, rgba(255, 255, 255, 90));
    cairo_fill(cr);
  }
  // The mouth, facing ahead.
  const double mx = cx + w * 0.12, my = cy + h * 0.06;
  if (shut)
  {
    cairo_move_to(cr, mx - w * 0.2, my);
    cairo_curve_to(cr, mx - w * 0.05, my + 8, mx + w * 0.1, my + 8, mx + w * 0.26, my - 2);
    cairo_set_line_width(cr, 4.0);
    setColor(cr, kInk);
    cairo_stroke(cr);
  }
  else
  {
    ellipse(cr, mx, my, w * 0.24, h * 0.2);
    fillOutline(cr, rgb(60, 10, 40), kInk, 3.0);
    ellipse(cr, mx, my + h * 0.1, w * 0.14, h * 0.07);
    setColor(cr, rgb(230, 90, 120));
    cairo_fill(cr);
    for (int k = 0; k < 5; ++k)
    {
      const double tx = mx - w * 0.16 + k * w * 0.08;
      cairo_move_to(cr, tx - 3, my - h * 0.17);
      cairo_line_to(cr, tx, my - h * 0.1);
      cairo_line_to(cr, tx + 3, my - h * 0.17);
      cairo_close_path(cr);
    }
    setColor(cr, rgb(255, 250, 235));
    cairo_fill(cr);
  }
  // Eyes on top.
  for (int k = 0; k < 2; ++k)
  {
    const double ex = cx - w * 0.06 + k * w * 0.2, ey = cy - h * 0.24;
    circle(cr, ex, ey, 7);
    fillOutline(cr, rgb(255, 255, 255), kInk, 1.6);
    circle(cr, ex + 2, ey + 1, 3);
    setColor(cr, kInk);
    cairo_fill(cr);
  }
}

// A thornbush: a clump of hooked violet thorns (variant 1: the sickly green
// one with the Virus in it).
void thornbush(const Box& b)
{
  cairo_t* cr = b.cr;
  const double x0 = kM, y0 = kM, w = b.w, h = b.h;
  const bool sick = b.variant == 1;
  const Color leaf = sick ? rgb(130, 220, 60) : rgb(150, 80, 190);
  for (int k = 0; k < 3; ++k)
  {
    ellipse(cr, x0 + w * (0.25 + 0.25 * k), y0 + h * (0.62 - 0.12 * (k == 1)), w * 0.24, h * 0.4);
    fillGradientOutline(cr, y0, y0 + h, lighten(leaf, 0.25f), darken(leaf, 0.3f), kInk, kLine);
  }
  for (int k = 0; k < 9; ++k)
  {
    const double a = -kPi + k * kPi / 8.0;
    thorn(cr, x0 + w * 0.5 + std::cos(a) * w * 0.36, y0 + h * 0.62 + std::sin(a) * h * 0.4, 9, a,
      sick ? rgb(220, 255, 140) : rgb(236, 200, 255));
  }
  if (sick)
  {
    radialGlow(cr, x0 + w * 0.5, y0 + h * 0.5, w * 0.5, rgb(160, 255, 60), 0.5 + 0.2 * std::sin(b.frame * 0.7));
    for (int k = 0; k < 4; ++k)
    {
      circle(cr, x0 + w * (0.2 + 0.2 * k), y0 + h * (0.4 + 0.15 * (k % 2)), 3);
      setColor(cr, rgb(240, 255, 120));
      cairo_fill(cr);
    }
  }
}

using DrawFn = void (*)(const Box&);

} // namespace

bool drawPlainsArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame)
{
  static const std::map<std::string, DrawFn> kRoutines{
    {"thorn_hog", thornHog},
    {"sky_gulper", skyGulper},
    {"thornbush", thornbush},
  };
  const auto it = kRoutines.find(key);
  if (it == kRoutines.end())
    return false;
  it->second(Box{cr, t, w, h, variant, frame});
  return true;
}

} // namespace gr
