// Kaiju Tonight (Level 26) in the style of enemy_art.cpp: the Model Tank,
// the Toy Jet and the Maser Truck (tin toys and a studio prop), Gorgazilla
// (a stuntman in a rubber lizard suit) and the tired man who climbs out of
// it, and the rubber duck's own monster suit. Each is drawn in screen
// pixels into a texture with a 32 px margin, the cell box at (32, 32) ..
// (32 + w, 32 + h), facing right.

#include "assets/enemy_art_city.hpp"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <utility>
#include <vector>

namespace gr
{

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr Color kInk = rgb(20, 18, 24);
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

void ink(cairo_t* cr, double line = 2.0)
{
  setColor(cr, kInk);
  cairo_set_line_width(cr, line);
  cairo_stroke(cr);
}

// Fills the path with a linear gradient between two points and inks it.
void fillGrad(cairo_t* cr, double x0, double y0, double x1, double y1, Color a, Color b, double line = 2.0)
{
  cairo_pattern_t* g = cairo_pattern_create_linear(x0, y0, x1, y1);
  stop(g, 0, a);
  stop(g, 1, b);
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  if (line > 0)
    ink(cr, line);
  else
    cairo_new_path(cr);
}

// A ball lit from the upper left, inked.
void ball(cairo_t* cr, double x, double y, double r, Color c, double line = 2.0)
{
  circle(cr, x, y, r);
  cairo_pattern_t* g = cairo_pattern_create_radial(x - r * 0.4, y - r * 0.45, r * 0.1, x, y, r);
  stop(g, 0, lighten(c, 0.5f));
  stop(g, 0.6, c);
  stop(g, 1, darken(c, 0.4f));
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  if (line > 0)
    ink(cr, line);
  else
    cairo_new_path(cr);
}

void glow(cairo_t* cr, double x, double y, double r, Color c, double a)
{
  circle(cr, x, y, r);
  cairo_pattern_t* g = cairo_pattern_create_radial(x, y, 0, x, y, r);
  stop(g, 0, c, a);
  stop(g, 0.45, c, a * 0.4);
  stop(g, 1, c, 0.0);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
}

// A shine: a small soft white crescent.
void shine(cairo_t* cr, double x, double y, double rx, double ry, double a, double alpha = 0.7)
{
  ellipse(cr, x, y, rx, ry, a);
  setRgba(cr, rgb(255, 255, 255), alpha);
  cairo_fill(cr);
}

// A thick limb along points, inked: an ink stroke under a colour stroke and
// a highlight along it.
void limb(cairo_t* cr, std::initializer_list<std::pair<double, double>> pts, double w, Color c, double line = 2.0)
{
  auto path = [&] {
    bool first = true;
    for (const auto& p : pts)
    {
      if (first)
        cairo_move_to(cr, p.first, p.second);
      else
        cairo_line_to(cr, p.first, p.second);
      first = false;
    }
  };
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
  path();
  setColor(cr, kInk);
  cairo_set_line_width(cr, w + line * 2);
  cairo_stroke(cr);
  path();
  setColor(cr, c);
  cairo_set_line_width(cr, w);
  cairo_stroke(cr);
  cairo_save(cr);
  cairo_translate(cr, -w * 0.18, -w * 0.2);
  path();
  setRgba(cr, lighten(c, 0.35f), 0.55);
  cairo_set_line_width(cr, w * 0.3);
  cairo_stroke(cr);
  cairo_restore(cr);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
}

// A tapered tube along a cubic Bezier, w0 wide at p0 to w1 at p3 (a tail).
void taperPath(cairo_t* cr, double x0, double y0, double x1, double y1, double x2, double y2, double x3, double y3, double w0,
  double w1)
{
  constexpr int kN = 20;
  std::vector<std::pair<double, double>> left, right;
  for (int i = 0; i <= kN; ++i)
  {
    const double u = double(i) / kN, m = 1 - u;
    const double px = m * m * m * x0 + 3 * m * m * u * x1 + 3 * m * u * u * x2 + u * u * u * x3;
    const double py = m * m * m * y0 + 3 * m * m * u * y1 + 3 * m * u * u * y2 + u * u * u * y3;
    const double dx = 3 * m * m * (x1 - x0) + 6 * m * u * (x2 - x1) + 3 * u * u * (x3 - x2);
    const double dy = 3 * m * m * (y1 - y0) + 6 * m * u * (y2 - y1) + 3 * u * u * (y3 - y2);
    const double len = std::max(1e-6, std::hypot(dx, dy));
    const double hw = (w0 + (w1 - w0) * u) * 0.5;
    left.push_back({px - dy / len * hw, py + dx / len * hw});
    right.push_back({px + dy / len * hw, py - dx / len * hw});
  }
  cairo_move_to(cr, left[0].first, left[0].second);
  for (std::size_t i = 1; i < left.size(); ++i)
    cairo_line_to(cr, left[i].first, left[i].second);
  for (std::size_t i = right.size(); i-- > 0;)
    cairo_line_to(cr, right[i].first, right[i].second);
  cairo_close_path(cr);
}

// --- The toy army ------------------------------------------------------------------

constexpr Color kOlive = rgb(104, 116, 66), kOliveLight = rgb(160, 170, 104), kOliveDark = rgb(56, 64, 36);

// A white five-pointed star, its middle at (x, y).
void star(cairo_t* cr, double x, double y, double r, double a = 0.0)
{
  for (int i = 0; i < 10; ++i)
  {
    const double ang = a - kPi / 2 + i * kPi / 5, rr = i % 2 ? r * 0.42 : r;
    if (i == 0)
      cairo_move_to(cr, x + std::cos(ang) * rr, y + std::sin(ang) * rr);
    else
      cairo_line_to(cr, x + std::cos(ang) * rr, y + std::sin(ang) * rr);
  }
  cairo_close_path(cr);
}

void muzzleFlash(cairo_t* cr, double x, double y, double r, double a)
{
  glow(cr, x, y, r * 2.2, rgb(255, 210, 90), 0.8);
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_rotate(cr, a);
  for (int i = 0; i < 8; ++i)
  {
    const double ang = i * kPi / 4, rr = i % 2 ? r * 0.55 : r;
    if (i == 0)
      cairo_move_to(cr, std::cos(ang) * rr, std::sin(ang) * rr);
    cairo_line_to(cr, std::cos(ang + kPi / 8) * rr * 0.3, std::sin(ang + kPi / 8) * rr * 0.3);
    cairo_line_to(cr, std::cos(ang + kPi / 4) * (i % 2 ? r : r * 0.55), std::sin(ang + kPi / 4) * (i % 2 ? r : r * 0.55));
  }
  cairo_close_path(cr);
  setRgba(cr, rgb(255, 250, 210), 0.95);
  cairo_fill(cr);
  cairo_restore(cr);
}

// The tin toy tank side on: tracks and road wheels, the hull with its
// star, the turret (lifted on its ring in the tell) and the barrel at 45
// degrees.
void modelTank(cairo_t* cr, double w, double h, bool tell, int frame)
{
  const double x0 = kM, y0 = kM;
  const double lift = tell ? 9.0 : 0.0;
  // The barrel, behind the turret: up at 45 degrees.
  const double tx = x0 + w * 0.46, ty = y0 + h * 0.36 - lift;
  const double bl = w * 0.42;
  const double bx = tx + std::cos(-kPi / 4) * bl, by = ty + std::sin(-kPi / 4) * bl;
  limb(cr, {{tx, ty}, {bx, by}}, 6.0, kOliveDark, 1.6);
  cairo_save(cr);
  cairo_translate(cr, bx, by);
  cairo_rotate(cr, -kPi / 4);
  cairo_rectangle(cr, -2, -5, 8, 10);
  fillGrad(cr, 0, -5, 0, 5, kOliveLight, kOliveDark, 1.6);
  cairo_restore(cr);
  if (tell)
  {
    // The turret ring post, showing under the lifted turret.
    cairo_rectangle(cr, x0 + w * 0.34, y0 + h * 0.36, w * 0.22, 10);
    fillGrad(cr, 0, y0 + h * 0.36, 0, y0 + h * 0.36 + 10, rgb(150, 150, 150), rgb(80, 80, 80), 1.6);
  }
  // The turret: a rounded dome, a hatch, a little periscope.
  cairo_move_to(cr, x0 + w * 0.22, y0 + h * 0.5 - lift);
  cairo_curve_to(cr, x0 + w * 0.22, y0 + h * 0.12 - lift, x0 + w * 0.66, y0 + h * 0.1 - lift, x0 + w * 0.7, y0 + h * 0.5 - lift);
  cairo_close_path(cr);
  fillGrad(cr, 0, y0 + h * 0.1 - lift, 0, y0 + h * 0.5 - lift, kOliveLight, kOlive, 2.0);
  ellipse(cr, x0 + w * 0.4, y0 + h * 0.15 - lift, w * 0.08, 3);
  fillGrad(cr, 0, y0 + h * 0.1 - lift, 0, y0 + h * 0.2 - lift, kOliveLight, kOliveDark, 1.4);
  shine(cr, x0 + w * 0.36, y0 + h * 0.26 - lift, 6, 2.5, -0.3, 0.5);
  // The hull.
  cairo_move_to(cr, x0 + w * 0.04, y0 + h * 0.66);
  cairo_line_to(cr, x0 + w * 0.12, y0 + h * 0.46);
  cairo_line_to(cr, x0 + w * 0.86, y0 + h * 0.46);
  cairo_line_to(cr, x0 + w * 0.98, y0 + h * 0.62);
  cairo_line_to(cr, x0 + w * 0.96, y0 + h * 0.7);
  cairo_line_to(cr, x0 + w * 0.04, y0 + h * 0.7);
  cairo_close_path(cr);
  fillGrad(cr, 0, y0 + h * 0.46, 0, y0 + h * 0.7, kOliveLight, kOliveDark, 2.0);
  star(cr, x0 + w * 0.52, y0 + h * 0.58, 6.0);
  setColor(cr, rgb(250, 250, 240));
  cairo_fill(cr);
  // The tracks and their road wheels.
  roundedRect(cr, x0 + w * 0.03, y0 + h * 0.66, w * 0.94, h * 0.33, h * 0.16);
  fillGrad(cr, 0, y0 + h * 0.66, 0, y0 + h, rgb(70, 70, 66), rgb(30, 30, 30), 2.0);
  for (int i = 0; i < 5; ++i)
  {
    const double wx = x0 + w * (0.16 + i * 0.17), wy = y0 + h * 0.83;
    ball(cr, wx, wy, h * 0.11, rgb(130, 136, 110), 1.4);
    const double a = (frame ? 0.6 : 0.0) + i;
    cairo_move_to(cr, wx + std::cos(a) * h * 0.08, wy + std::sin(a) * h * 0.08);
    cairo_line_to(cr, wx - std::cos(a) * h * 0.08, wy - std::sin(a) * h * 0.08);
    setRgba(cr, rgb(50, 50, 40), 0.8);
    cairo_set_line_width(cr, 1.4);
    cairo_stroke(cr);
  }
  for (int i = 0; i < 12; ++i)
  {
    const double lx = x0 + w * 0.07 + i * w * 0.075 + (frame ? w * 0.035 : 0.0);
    cairo_move_to(cr, lx, y0 + h * 0.67);
    cairo_line_to(cr, lx, y0 + h * 0.71);
  }
  setRgba(cr, rgb(110, 110, 100), 0.8);
  cairo_set_line_width(cr, 1.5);
  cairo_stroke(cr);
  if (tell)
    muzzleFlash(cr, bx + std::cos(-kPi / 4) * 6, by + std::sin(-kPi / 4) * 6, 12, frame * 0.4);
}

// The same tank from straight above, its barrel along +x.
void modelTankTop(cairo_t* cr, double w, double h, bool tell, int frame)
{
  const double x0 = kM, y0 = kM;
  // Shadow.
  roundedRect(cr, x0 + 4, y0 + 5, w - 4, h - 4, 8);
  setRgba(cr, rgb(0, 0, 0), 0.3);
  cairo_fill(cr);
  for (const double ty : {y0 + 1.0, y0 + h - h * 0.26 - 1})
  {
    roundedRect(cr, x0 + 2, ty, w - 8, h * 0.26, 5);
    fillGrad(cr, 0, ty, 0, ty + h * 0.26, rgb(70, 70, 66), rgb(30, 30, 30), 1.8);
    for (int i = 0; i < 14; ++i)
    {
      const double lx = x0 + 6 + i * (w - 14) / 14.0 + (frame ? 3.0 : 0.0);
      cairo_move_to(cr, lx, ty + 2);
      cairo_line_to(cr, lx, ty + h * 0.26 - 2);
    }
    setRgba(cr, rgb(120, 120, 110), 0.7);
    cairo_set_line_width(cr, 1.4);
    cairo_stroke(cr);
  }
  roundedRect(cr, x0 + 6, y0 + h * 0.2, w - 14, h * 0.6, 6);
  fillGrad(cr, x0, y0 + h * 0.2, x0, y0 + h * 0.8, kOliveLight, kOliveDark, 2.0);
  star(cr, x0 + w * 0.2, y0 + h * 0.5, 7, kPi / 2);
  setColor(cr, rgb(250, 250, 240));
  cairo_fill(cr);
  const double cx = x0 + w * 0.48, cy = y0 + h * 0.5;
  limb(cr, {{cx, cy}, {x0 + w + 10, cy}}, 6.0, kOliveDark, 1.6);
  ball(cr, cx, cy, h * 0.26, tell ? lighten(kOlive, 0.25f) : kOlive, 2.0);
  circle(cr, cx - 5, cy - 4, 4.5);
  fillGrad(cr, 0, cy - 9, 0, cy, kOliveLight, kOliveDark, 1.2);
  if (tell)
    muzzleFlash(cr, x0 + w + 14, cy, 13, frame * 0.4);
}

// The tin toy jet: a silver fuselage with a red nose ring and stripe, the
// bubble canopy, swept wing and fin, printed rivets, a 42 on its side.
void toyJet(cairo_t* cr, double w, double h, int spin, int frame)
{
  const double cx = kM + w * 0.5, cy = kM + h * 0.5;
  cairo_save(cr);
  cairo_translate(cr, cx, cy);
  if (spin > 0)
    cairo_rotate(cr, 0.35 + (spin - 1) * kPi / 2);
  const double L = w * 0.92, R = h * 0.17;
  const Color tin = rgb(196, 204, 214), tinDark = rgb(110, 116, 130), red = rgb(214, 44, 40);
  // The far wing, then the tail fin.
  cairo_move_to(cr, -L * 0.05, -R * 0.4);
  cairo_line_to(cr, -L * 0.3, -R * 2.4);
  cairo_line_to(cr, -L * 0.18, -R * 2.4);
  cairo_line_to(cr, L * 0.12, -R * 0.4);
  cairo_close_path(cr);
  fillGrad(cr, 0, -R * 2.4, 0, 0, darken(tin, 0.25f), darken(tin, 0.45f), 1.6);
  cairo_move_to(cr, -L * 0.34, -R * 0.6);
  cairo_line_to(cr, -L * 0.48, -R * 2.6);
  cairo_line_to(cr, -L * 0.38, -R * 2.6);
  cairo_line_to(cr, -L * 0.2, -R * 0.6);
  cairo_close_path(cr);
  fillGrad(cr, 0, -R * 2.6, 0, 0, lighten(tin, 0.2f), tinDark, 1.6);
  cairo_rectangle(cr, -L * 0.47, -R * 2.3, L * 0.06, R * 0.8);
  setColor(cr, red);
  cairo_fill(cr);
  // The fuselage.
  cairo_move_to(cr, L * 0.48, -R * 0.7);
  cairo_curve_to(cr, L * 0.3, -R * 1.1, -L * 0.3, -R * 1.0, -L * 0.48, -R * 0.45);
  cairo_line_to(cr, -L * 0.48, R * 0.45);
  cairo_curve_to(cr, -L * 0.3, R * 1.0, L * 0.3, R * 1.1, L * 0.48, R * 0.7);
  cairo_close_path(cr);
  fillGrad(cr, 0, -R, 0, R, lighten(tin, 0.4f), tinDark, 2.0);
  // The red nose ring and the intake.
  ellipse(cr, L * 0.48, 0, R * 0.32, R * 0.72);
  setColor(cr, red);
  cairo_fill_preserve(cr);
  ink(cr, 1.6);
  ellipse(cr, L * 0.49, 0, R * 0.16, R * 0.42);
  setColor(cr, rgb(30, 30, 36));
  cairo_fill(cr);
  // The stripe and the number.
  cairo_rectangle(cr, -L * 0.05, -R * 0.15, L * 0.38, R * 0.3);
  setRgba(cr, red, 0.9);
  cairo_fill(cr);
  selectGameFont(cr);
  cairo_set_font_size(cr, R * 1.0);
  cairo_move_to(cr, -L * 0.3, R * 0.62);
  cairo_text_path(cr, "42");
  setColor(cr, rgb(30, 40, 80));
  cairo_fill(cr);
  // The bubble canopy.
  cairo_move_to(cr, L * 0.32, -R * 0.75);
  cairo_curve_to(cr, L * 0.28, -R * 1.9, L * 0.06, -R * 1.9, L * 0.02, -R * 0.8);
  cairo_close_path(cr);
  fillGrad(cr, 0, -R * 1.9, 0, -R * 0.7, rgb(200, 240, 255), rgb(60, 110, 160), 1.6);
  shine(cr, L * 0.2, -R * 1.45, R * 0.4, R * 0.18, -0.3, 0.8);
  // The near wing.
  cairo_move_to(cr, L * 0.08, R * 0.2);
  cairo_line_to(cr, -L * 0.22, R * 1.9);
  cairo_line_to(cr, -L * 0.08, R * 1.9);
  cairo_line_to(cr, L * 0.24, R * 0.2);
  cairo_close_path(cr);
  fillGrad(cr, 0, R * 0.2, 0, R * 1.9, lighten(tin, 0.3f), tinDark, 1.8);
  cairo_rectangle(cr, -L * 0.2, R * 1.55, L * 0.12, R * 0.3);
  setColor(cr, red);
  cairo_fill(cr);
  // Printed rivets and the tin's seam.
  for (int i = 0; i < 9; ++i)
  {
    circle(cr, -L * 0.4 + i * L * 0.09, R * 0.78, 0.9);
    setRgba(cr, tinDark, 0.8);
    cairo_fill(cr);
  }
  cairo_move_to(cr, -L * 0.48, 0);
  cairo_line_to(cr, L * 0.4, 0);
  setRgba(cr, rgb(255, 255, 255), 0.4);
  cairo_set_line_width(cr, 1.0);
  cairo_stroke(cr);
  shine(cr, L * 0.05, -R * 0.62, L * 0.2, R * 0.12, 0.0, 0.6);
  // The exhaust, or the smoke of a jet going down.
  if (spin == 0)
  {
    glow(cr, -L * 0.52, 0, R * (frame ? 1.6 : 1.2), rgb(255, 170, 60), 0.85);
    ellipse(cr, -L * 0.52, 0, R * (frame ? 0.7 : 0.5), R * 0.32);
    setRgba(cr, rgb(255, 250, 200), 0.95);
    cairo_fill(cr);
  }
  else
    for (int i = 0; i < 4; ++i)
    {
      ellipse(cr, -L * 0.55 - i * 7, i * 2.0, 6 + i * 3, 5 + i * 2);
      setRgba(cr, rgb(60, 56, 60), 0.5 - i * 0.1);
      cairo_fill(cr);
    }
  cairo_restore(cr);
}

// The maser truck: an olive army flatbed, its cab at the front (right), and
// on the bed a turret carrying a parabolic dish, its emitter rod and coils
// pointing ahead. lit 0 idling, 1 charging, -1 powerless.
void maserTruck(cairo_t* cr, double w, double h, int lit, int frame)
{
  const double x0 = kM, y0 = kM;
  const Color glass = lit < 0 ? rgb(40, 50, 60) : rgb(150, 200, 230);
  // The chassis and the bed.
  cairo_rectangle(cr, x0 + w * 0.04, y0 + h * 0.7, w * 0.92, h * 0.1);
  fillGrad(cr, 0, y0 + h * 0.7, 0, y0 + h * 0.8, rgb(60, 60, 58), rgb(26, 26, 26), 1.8);
  cairo_rectangle(cr, x0 + w * 0.02, y0 + h * 0.6, w * 0.68, h * 0.12);
  fillGrad(cr, 0, y0 + h * 0.6, 0, y0 + h * 0.72, kOliveLight, kOliveDark, 2.0);
  for (int i = 0; i < 6; ++i)
  {
    cairo_move_to(cr, x0 + w * (0.06 + i * 0.11), y0 + h * 0.61);
    cairo_line_to(cr, x0 + w * (0.06 + i * 0.11), y0 + h * 0.71);
  }
  setRgba(cr, kOliveDark, 0.8);
  cairo_set_line_width(cr, 1.4);
  cairo_stroke(cr);
  star(cr, x0 + w * 0.12, y0 + h * 0.66, 5.0);
  setColor(cr, rgb(250, 250, 240));
  cairo_fill(cr);
  // The cab: a rounded nose, the windshield, the door, a headlight.
  cairo_move_to(cr, x0 + w * 0.7, y0 + h * 0.74);
  cairo_line_to(cr, x0 + w * 0.7, y0 + h * 0.3);
  cairo_line_to(cr, x0 + w * 0.82, y0 + h * 0.28);
  cairo_line_to(cr, x0 + w * 0.88, y0 + h * 0.46);
  cairo_curve_to(cr, x0 + w * 0.98, y0 + h * 0.48, x0 + w, y0 + h * 0.52, x0 + w * 0.99, y0 + h * 0.74);
  cairo_close_path(cr);
  fillGrad(cr, 0, y0 + h * 0.28, 0, y0 + h * 0.74, kOliveLight, kOliveDark, 2.0);
  cairo_move_to(cr, x0 + w * 0.76, y0 + h * 0.33);
  cairo_line_to(cr, x0 + w * 0.82, y0 + h * 0.32);
  cairo_line_to(cr, x0 + w * 0.86, y0 + h * 0.45);
  cairo_line_to(cr, x0 + w * 0.76, y0 + h * 0.45);
  cairo_close_path(cr);
  fillGrad(cr, 0, y0 + h * 0.32, 0, y0 + h * 0.45, lighten(glass, 0.3f), darken(glass, 0.3f), 1.4);
  roundedRect(cr, x0 + w * 0.72, y0 + h * 0.48, w * 0.1, h * 0.18, 2);
  setRgba(cr, kOliveDark, 0.9);
  cairo_set_line_width(cr, 1.2);
  cairo_stroke(cr);
  for (int i = 0; i < 4; ++i)
  {
    cairo_move_to(cr, x0 + w * 0.93, y0 + h * (0.53 + i * 0.045));
    cairo_line_to(cr, x0 + w * 0.99, y0 + h * (0.53 + i * 0.045));
  }
  setRgba(cr, rgb(200, 200, 190), 0.8);
  cairo_set_line_width(cr, 1.4);
  cairo_stroke(cr);
  ball(cr, x0 + w * 0.97, y0 + h * 0.5, 4.0, lit < 0 ? rgb(90, 90, 80) : rgb(255, 240, 180), 1.2);
  if (lit >= 0)
    glow(cr, x0 + w * 0.99, y0 + h * 0.5, 16, rgb(255, 240, 180), 0.5);
  // The wheels.
  for (const double wx : {0.16, 0.36, 0.85})
  {
    const double cx = x0 + w * wx, cy = y0 + h * 0.84, rr = h * 0.14;
    ball(cr, cx, cy, rr, rgb(40, 40, 40), 2.0);
    circle(cr, cx, cy, rr * 0.5);
    setColor(cr, rgb(130, 136, 110));
    cairo_fill(cr);
    const double a = frame ? 0.8 : 0.0;
    for (int k = 0; k < 3; ++k)
    {
      const double aa = a + k * 2 * kPi / 3;
      circle(cr, cx + std::cos(aa) * rr * 0.3, cy + std::sin(aa) * rr * 0.3, 1.4);
      setColor(cr, rgb(60, 60, 50));
      cairo_fill(cr);
    }
  }
  // The turret and its yoke.
  cairo_rectangle(cr, x0 + w * 0.26, y0 + h * 0.46, w * 0.24, h * 0.15);
  fillGrad(cr, 0, y0 + h * 0.46, 0, y0 + h * 0.61, kOliveLight, kOliveDark, 2.0);
  limb(cr, {{x0 + w * 0.38, y0 + h * 0.48}, {x0 + w * 0.46, y0 + h * 0.3}}, 7.0, rgb(90, 96, 100), 1.6);
  // The dish, side on: its back, then the rim's ellipse and the bowl.
  const double dx = x0 + w * 0.5, dy = y0 + h * 0.28, rx = w * 0.07, ry = h * 0.3;
  ellipse(cr, dx - 4, dy, rx * 1.4, ry);
  fillGrad(cr, dx - rx * 2, 0, dx + rx, 0, rgb(120, 126, 136), rgb(60, 64, 72), 2.0);
  ellipse(cr, dx + rx * 0.4, dy, rx, ry);
  {
    cairo_pattern_t* g = cairo_pattern_create_radial(dx + rx * 0.4, dy, 1, dx + rx * 0.4, dy, ry);
    stop(g, 0, lit > 0 ? rgb(220, 250, 255) : rgb(210, 214, 224));
    stop(g, 0.7, lit > 0 ? rgb(130, 210, 240) : rgb(160, 166, 178));
    stop(g, 1, rgb(90, 94, 104));
    cairo_set_source(cr, g);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(g);
  }
  ink(cr, 2.0);
  for (int i = 1; i <= 3; ++i)
  {
    ellipse(cr, dx + rx * 0.4, dy, rx * i / 4.0, ry * i / 4.0);
    setRgba(cr, rgb(80, 84, 96), 0.4);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);
  }
  // The emitter rod, its coils and the bulb.
  const double ex = dx + w * 0.16;
  limb(cr, {{dx + rx * 0.4, dy}, {ex, dy}}, 4.0, rgb(170, 176, 186), 1.4);
  for (int i = 0; i < 3; ++i)
  {
    const double cx = dx + rx + 6 + i * 6;
    ellipse(cr, cx, dy, 2.2, 7 - i);
    setColor(cr, lit > 0 ? rgb(140, 240, 255) : (lit < 0 ? rgb(70, 74, 80) : rgb(190, 150, 90)));
    cairo_fill_preserve(cr);
    ink(cr, 1.0);
  }
  if (lit > 0)
  {
    glow(cr, ex, dy, 30, rgb(120, 230, 255), 0.9);
    ball(cr, ex, dy, 5.5, rgb(220, 255, 255), 1.2);
  }
  else if (lit == 0)
  {
    glow(cr, ex, dy, 14, rgb(120, 230, 255), 0.45);
    ball(cr, ex, dy, 4.5, rgb(140, 220, 240), 1.2);
  }
  else
    ball(cr, ex, dy, 4.5, rgb(60, 66, 74), 1.2);
}

// --- Gorgazilla --------------------------------------------------------------------

constexpr Color kSuit = rgb(78, 112, 74), kSuitLight = rgb(132, 168, 110), kSuitDark = rgb(40, 62, 42);
constexpr Color kBelly = rgb(186, 184, 128), kPlate = rgb(224, 226, 210);

struct LizardPose
{
  double lean = 0.0;    // radians about the feet, + forward
  double head = 0.0;    // the head's tilt, - back
  double jaw = 0.0;     // 0 shut .. 1 wide
  double arm = 0.0;     // 0 down .. 1 raised
  double wave = -1.0;   // the front hand waving at this angle (radians), -1 not
  int tail = 0;         // 0 resting, 1 coiled up, 2 swiping low
  bool dizzy = false;
  bool sit = false;
  double zip = 0.0;     // the zipper's opening, 0 .. 1
  bool manOut = false;  // the man's head out of the zipper
  bool empty = false;   // the suit slumped, empty
};

// A dorsal plate: a pale maple-leaf spike at (x, y), pointing at angle a.
void plate(cairo_t* cr, double x, double y, double size, double a)
{
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_rotate(cr, a);
  cairo_move_to(cr, -size * 0.45, 0);
  cairo_line_to(cr, -size * 0.55, -size * 0.35);
  cairo_line_to(cr, -size * 0.28, -size * 0.4);
  cairo_line_to(cr, -size * 0.2, -size * 0.8);
  cairo_line_to(cr, 0, -size * 0.55);
  cairo_line_to(cr, size * 0.15, -size);
  cairo_line_to(cr, size * 0.3, -size * 0.5);
  cairo_line_to(cr, size * 0.55, -size * 0.55);
  cairo_line_to(cr, size * 0.45, 0);
  cairo_close_path(cr);
  fillGrad(cr, 0, -size, 0, 0, lighten(kPlate, 0.4f), darken(kPlate, 0.3f), 1.6);
  cairo_restore(cr);
}

// Rubber bumps and wrinkles over whatever path is clipped.
void rubber(cairo_t* cr, double x0, double y0, double w, double h, unsigned seed)
{
  std::uint32_t s = seed * 2654435761u + 7u;
  auto rnd = [&s] {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return double(s & 0xFFFFu) / 65535.0;
  };
  for (int i = 0; i < 70; ++i)
  {
    const double x = x0 + rnd() * w, y = y0 + rnd() * h, r = 1.2 + rnd() * 2.6;
    circle(cr, x + 0.6, y + 0.8, r);
    setRgba(cr, kSuitDark, 0.35);
    cairo_fill(cr);
    circle(cr, x, y, r);
    setRgba(cr, kSuitLight, 0.35);
    cairo_fill(cr);
  }
}

void paintLizard(cairo_t* cr, double cx, double by, double s, const LizardPose& p, int frame)
{
  cairo_save(cr);
  cairo_translate(cr, cx, by);
  cairo_scale(cr, s, s);
  // Everything in local px: the feet's middle at (0, 0), up is -y.
  const double sink = p.sit ? 30.0 : 0.0;
  // The tail, behind it all.
  if (p.tail == 1)
    taperPath(cr, -16, -60 + sink, -66, -66, -66, -120, -34, -112, 26, 6);
  else if (p.tail == 2)
    taperPath(cr, -16, -52 + sink, -40, -24, -56, -10, -62, -6, 26, 6);
  else
    taperPath(cr, -16, -56 + sink, -40, -38, -50, -12, -62, -4, 26, 5);
  fillGrad(cr, -60, -80, -20, -10, kSuitLight, kSuitDark, 2.0);
  // Plates down the tail.
  if (p.tail == 1)
    for (int i = 0; i < 3; ++i)
      plate(cr, -40 - i * 9, -88 - i * 10, 12 - i * 2, -1.6 - i * 0.3);
  else
    for (int i = 0; i < 3; ++i)
      plate(cr, -28 - i * 11, -48 + sink * 0.3 + i * 14, 12 - i * 2.5, -0.9 - i * 0.15);
  // The legs: standing, or stretched out ahead sitting.
  auto foot = [&](double x, double y, bool back) {
    ellipse(cr, x + 6, y - 6, 17, 8);
    fillGrad(cr, 0, y - 14, 0, y, back ? kSuit : kSuitLight, kSuitDark, 2.0);
    for (int k = 0; k < 3; ++k)
    {
      const double tx = x + 14 + k * 4.5, ty = y - 2 - k * 1.2;
      cairo_move_to(cr, tx, ty - 3);
      cairo_line_to(cr, tx + 6, ty);
      cairo_line_to(cr, tx, ty + 2);
      cairo_close_path(cr);
      fillGrad(cr, 0, ty - 3, 0, ty + 2, rgb(250, 248, 236), rgb(170, 168, 150), 1.0);
    }
  };
  if (p.sit)
  {
    limb(cr, {{-8, -30}, {18, -26}, {30, -10}}, 26, darken(kSuit, 0.15f));
    foot(26, 0, true);
    limb(cr, {{-2, -26}, {26, -22}, {40, -8}}, 28, kSuit);
    foot(36, 0, false);
  }
  else
  {
    const double sw = std::sin(frame * kPi / 2) * 2.0;
    limb(cr, {{-10, -60}, {-6, -32}, {-12, -10}}, 26, darken(kSuit, 0.15f));
    foot(-18 + sw, 0, true);
  }
  // The body leans about the feet (or the seat).
  cairo_save(cr);
  if (p.sit)
    cairo_translate(cr, -6, -24);
  cairo_rotate(cr, p.lean);
  if (p.sit)
    cairo_translate(cr, 6, 24 + sink);
  // The back arm.
  const double ar = p.arm;
  limb(cr, {{4, -122}, {10 + 8 * ar, -104 - 20 * ar}, {22 + 6 * ar, -110 - 30 * ar}}, 14, darken(kSuit, 0.2f));
  // The dorsal plates along the back, then the body over their roots.
  static constexpr double kBack[6][3] = {{-10, -160, 22}, {-18, -146, 25}, {-24, -128, 26}, {-28, -110, 24}, {-30, -92, 21},
    {-30, -74, 17}};
  for (int i = 0; i < 6; ++i)
    plate(cr, kBack[i][0] + 2, kBack[i][1], kBack[i][2] * (p.empty ? 0.85 : 1.0), -0.55 - i * 0.12);
  // The body: a pear, the belly out front.
  cairo_move_to(cr, -12, -158);
  cairo_curve_to(cr, -26, -140, -34, -110, -32, -86);
  cairo_curve_to(cr, -31, -66, -28, -54, -14, -46);
  cairo_curve_to(cr, 0, -40, 22, -42, 32, -58);
  cairo_curve_to(cr, 42 - (p.empty ? 10 : 0), -76, 40 - (p.empty ? 8 : 0), -112, 28, -134);
  cairo_curve_to(cr, 22, -146, 18, -156, 14, -162);
  cairo_close_path(cr);
  cairo_path_t* body = cairo_copy_path(cr);
  {
    cairo_pattern_t* g = cairo_pattern_create_radial(10, -120, 6, 0, -100, 80);
    stop(g, 0, kSuitLight);
    stop(g, 0.6, kSuit);
    stop(g, 1, kSuitDark);
    cairo_set_source(cr, g);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(g);
  }
  ink(cr, 2.2);
  cairo_save(cr);
  cairo_append_path(cr, body);
  cairo_clip(cr);
  rubber(cr, -34, -164, 76, 124, 26u);
  // The belly: ribbed plates, pale.
  cairo_move_to(cr, 8, -150);
  cairo_curve_to(cr, 28, -132, 44, -96, 34, -50);
  cairo_line_to(cr, 0, -44);
  cairo_curve_to(cr, 10, -80, 8, -120, 2, -150);
  cairo_close_path(cr);
  fillGrad(cr, 0, -150, 40, -50, lighten(kBelly, 0.25f), darken(kBelly, 0.25f), 1.4);
  for (int i = 0; i < 8; ++i)
  {
    const double yy = -142 + i * 12.5;
    cairo_move_to(cr, 2, yy);
    cairo_curve_to(cr, 14, yy + 3, 26, yy + 3, 44, yy - 2);
  }
  setRgba(cr, darken(kBelly, 0.45f), 0.7);
  cairo_set_line_width(cr, 1.3);
  cairo_stroke(cr);
  // Wrinkles where the rubber folds at the waist and the neck.
  for (int i = 0; i < 3; ++i)
  {
    cairo_move_to(cr, -30, -64 + i * 5);
    cairo_curve_to(cr, -24, -62 + i * 5, -18, -64 + i * 5, -12, -60 + i * 5);
    cairo_move_to(cr, -8, -150 + i * 5);
    cairo_curve_to(cr, -2, -148 + i * 5, 4, -150 + i * 5, 8, -148 + i * 5);
  }
  setRgba(cr, kSuitDark, 0.55);
  cairo_set_line_width(cr, 1.3);
  cairo_stroke(cr);
  // The zipper down the back: a line of teeth, and its opening.
  if (p.sit || p.empty)
  {
    const double o = p.zip * 9.0;
    if (p.zip > 0.0)
    {
      cairo_move_to(cr, -14, -152);
      cairo_curve_to(cr, -26 - o, -126, -30 - o, -96, -26, -64);
      cairo_curve_to(cr, -22 + o * 0.6, -96, -18 + o * 0.6, -126, -14, -152);
      cairo_close_path(cr);
      setColor(cr, rgb(20, 14, 12));
      cairo_fill_preserve(cr);
      setRgba(cr, rgb(200, 200, 190), 0.9);
      cairo_set_line_width(cr, 1.6);
      cairo_stroke(cr);
    }
    else
    {
      // Shut: a line of teeth.
      cairo_move_to(cr, -14, -152);
      cairo_curve_to(cr, -25, -126, -28, -96, -26, -64);
      setRgba(cr, rgb(30, 30, 30), 0.9);
      cairo_set_line_width(cr, 2.4);
      cairo_stroke_preserve(cr);
      const double dash[2] = {1.6, 1.6};
      cairo_set_dash(cr, dash, 2, 0.0);
      setRgba(cr, rgb(210, 210, 200), 0.9);
      cairo_set_line_width(cr, 1.6);
      cairo_stroke(cr);
      cairo_set_dash(cr, nullptr, 0, 0.0);
    }
    if (p.zip > 0.4 && !p.empty)
    {
      // A sweaty undershirt inside.
      cairo_move_to(cr, -18, -140);
      cairo_curve_to(cr, -24 - o * 0.5, -120, -26 - o * 0.5, -100, -24, -80);
      setRgba(cr, rgb(236, 232, 220), 0.8);
      cairo_set_line_width(cr, 3.0 * p.zip);
      cairo_stroke(cr);
    }
    // The pull tab at the bottom.
    roundedRect(cr, -29, -66, 6, 10, 2);
    fillGrad(cr, 0, -66, 0, -56, rgb(230, 230, 236), rgb(120, 120, 130), 1.0);
  }
  cairo_restore(cr);
  cairo_path_destroy(body);
  // The front leg (standing), over the body's foot.
  if (!p.sit)
  {
    const double sw = -std::sin(frame * kPi / 2) * 2.0;
    limb(cr, {{4, -58}, {10, -32}, {4, -10}}, 30, kSuit);
    foot(-4 + sw, 0, false);
  }
  // The head on its neck: a long snout, a brow, an eye, the jaw.
  cairo_save(cr);
  cairo_translate(cr, 2, -150);
  cairo_rotate(cr, p.head);
  // The lower jaw, hinged at (6, -6).
  cairo_save(cr);
  cairo_translate(cr, 6, -6);
  cairo_rotate(cr, p.jaw * 0.55);
  cairo_move_to(cr, -8, 0);
  cairo_curve_to(cr, 8, 10, 30, 8, 40, -2);
  cairo_line_to(cr, 36, -6);
  cairo_curve_to(cr, 20, -2, 6, -2, -8, -8);
  cairo_close_path(cr);
  fillGrad(cr, 0, -6, 0, 8, kSuit, kSuitDark, 2.0);
  if (p.jaw > 0.1)
    for (int k = 0; k < 4; ++k)
    {
      cairo_move_to(cr, 12 + k * 6, -4);
      cairo_line_to(cr, 15 + k * 6, -10);
      cairo_line_to(cr, 18 + k * 6, -4);
      cairo_close_path(cr);
      setColor(cr, rgb(250, 248, 236));
      cairo_fill(cr);
    }
  cairo_restore(cr);
  if (p.jaw > 0.1)
  {
    // The mouth, red inside.
    cairo_move_to(cr, 0, -8);
    cairo_line_to(cr, 44, -12);
    cairo_line_to(cr, 40, -2 + p.jaw * 20);
    cairo_close_path(cr);
    setColor(cr, rgb(170, 40, 50));
    cairo_fill(cr);
  }
  // The skull and snout.
  cairo_move_to(cr, -14, -6);
  cairo_curve_to(cr, -18, -26, -8, -36, 8, -34);
  cairo_curve_to(cr, 24, -32, 36, -26, 46, -20);
  cairo_curve_to(cr, 52, -16, 50, -8, 44, -8);
  cairo_curve_to(cr, 30, -6, 10, -4, -14, -6);
  cairo_close_path(cr);
  cairo_path_t* skull = cairo_copy_path(cr);
  fillGrad(cr, 0, -36, 0, -4, kSuitLight, kSuit, 2.2);
  cairo_save(cr);
  cairo_append_path(cr, skull);
  cairo_clip(cr);
  rubber(cr, -16, -36, 64, 30, 27u);
  cairo_restore(cr);
  cairo_path_destroy(skull);
  // Teeth along the upper jaw.
  for (int k = 0; k < 5; ++k)
  {
    cairo_move_to(cr, 14 + k * 6, -8);
    cairo_line_to(cr, 17 + k * 6, -3 + (p.jaw > 0.1 ? 0 : -2));
    cairo_line_to(cr, 20 + k * 6, -8);
    cairo_close_path(cr);
    setColor(cr, rgb(250, 248, 236));
    cairo_fill_preserve(cr);
    setRgba(cr, kInk, 0.6);
    cairo_set_line_width(cr, 0.8);
    cairo_stroke(cr);
  }
  // The brow ridge, the nostril.
  cairo_move_to(cr, 6, -30);
  cairo_curve_to(cr, 14, -36, 24, -34, 30, -26);
  setColor(cr, kSuitDark);
  cairo_set_line_width(cr, 4.0);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  cairo_stroke(cr);
  ellipse(cr, 44, -17, 2.4, 1.6, 0.3);
  setColor(cr, kInk);
  cairo_fill(cr);
  // The eye: a rubber eye with a slit, a bit too round (or spirals, dizzy;
  // or shut, emptied).
  if (p.empty)
  {
    cairo_move_to(cr, 14, -24);
    cairo_curve_to(cr, 18, -21, 22, -21, 26, -24);
    ink(cr, 2.0);
  }
  else if (p.dizzy)
  {
    for (int k = 0; k < 18; ++k)
    {
      const double a = k * 0.6 + frame, r = 0.4 * k;
      if (k == 0)
        cairo_move_to(cr, 20 + std::cos(a) * r, -24 + std::sin(a) * r);
      else
        cairo_line_to(cr, 20 + std::cos(a) * r, -24 + std::sin(a) * r);
    }
    ball(cr, 20, -24, 7.5, rgb(250, 240, 200), 1.6);
    for (int k = 0; k < 18; ++k)
    {
      const double a = k * 0.6 + frame * 1.5, r = 0.36 * k;
      if (k == 0)
        cairo_move_to(cr, 20 + std::cos(a) * r, -24 + std::sin(a) * r);
      else
        cairo_line_to(cr, 20 + std::cos(a) * r, -24 + std::sin(a) * r);
    }
    ink(cr, 1.4);
  }
  else
  {
    ball(cr, 20, -24, 7.5, rgb(250, 226, 110), 1.6);
    ellipse(cr, 22, -24, 1.8, 5.5);
    setColor(cr, kInk);
    cairo_fill(cr);
    shine(cr, 17.5, -27, 2.2, 1.4, -0.4, 0.9);
  }
  cairo_restore(cr);
  // The front arm: stubby, three claws (waving, or raised in the roar).
  double hx = 30 + 10 * ar, hy = -112 - 34 * ar;
  if (p.wave >= 0.0)
  {
    hx = 22 + std::cos(p.wave) * 30;
    hy = -150 + std::sin(p.wave) * 18;
    limb(cr, {{16, -124}, {30, -128}, {hx, hy}}, 15, kSuit);
  }
  else
    limb(cr, {{16, -124}, {26 + 6 * ar, -104 - 22 * ar}, {hx, hy}}, 15, kSuit);
  for (int k = 0; k < 3; ++k)
  {
    const double a = (p.wave >= 0.0 ? -1.6 : -0.4 - ar * 0.9) + k * 0.5;
    cairo_move_to(cr, hx + std::cos(a) * 5, hy + std::sin(a) * 5);
    cairo_line_to(cr, hx + std::cos(a) * 12, hy + std::sin(a) * 12);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    setColor(cr, kInk);
    cairo_set_line_width(cr, 5.0);
    cairo_stroke(cr);
    cairo_move_to(cr, hx + std::cos(a) * 5, hy + std::sin(a) * 5);
    cairo_line_to(cr, hx + std::cos(a) * 11, hy + std::sin(a) * 11);
    setColor(cr, rgb(240, 238, 222));
    cairo_set_line_width(cr, 2.6);
    cairo_stroke(cr);
  }
  // The man's head out of the zipper, blinking at the lights.
  if (p.manOut)
  {
    ball(cr, -24, -160, 11, rgb(236, 196, 160), 1.8);
    cairo_move_to(cr, -34, -164);
    cairo_curve_to(cr, -30, -176, -16, -176, -14, -166);
    setColor(cr, rgb(70, 50, 36));
    cairo_set_line_width(cr, 5.0);
    cairo_stroke(cr);
    circle(cr, -18, -160, 1.6);
    setColor(cr, kInk);
    cairo_fill(cr);
    // Sweat.
    ellipse(cr, -30, -152, 1.8, 3.0);
    setRgba(cr, rgb(200, 230, 255), 0.9);
    cairo_fill(cr);
  }
  cairo_restore(cr);
  cairo_restore(cr);
}

LizardPose lizardPose(int pose, int frame)
{
  LizardPose p;
  if (pose >= 0 && pose <= 3)
  {
    p.lean = 0.035 * std::sin(pose * kPi / 2);
    p.arm = 0.1 + 0.08 * std::cos(pose * kPi / 2);
  }
  else if (pose == 4)
  {
    p.lean = -0.12;
    p.head = -0.5;
    p.jaw = 1.0;
    p.arm = 1.0;
  }
  else if (pose == 5)
  {
    p.lean = 0.08;
    p.tail = 1;
    p.arm = 0.3;
  }
  else if (pose == 6)
  {
    p.lean = 0.14;
    p.tail = 2;
    p.arm = 0.5;
    p.jaw = 0.4;
  }
  else if (pose == 7 || pose == 8)
  {
    p.lean = pose == 7 ? -0.16 : -0.06;
    p.dizzy = true;
    p.arm = pose == 7 ? 0.8 : 0.4;
    p.jaw = 0.3;
  }
  else if (pose >= 9 && pose <= 13)
  {
    p.sit = true;
    p.lean = 0.18;
    p.head = 0.15;
    p.zip = (pose - 9) / 4.0;
    p.manOut = pose == 13;
    p.arm = 0.0;
  }
  else if (pose == 14)
  {
    p.sit = true;
    p.empty = true;
    p.lean = 0.32;
    p.head = 0.55;
    p.zip = 1.0;
  }
  else if (pose == 15)
  {
    p.wave = -1.2 + 0.5 * std::sin(frame * kPi / 3);
    p.lean = 0.02 * std::sin(frame * kPi / 3);
  }
  return p;
}

// The stuntman out of the suit: a crew cut, a sweaty undershirt, the suit's
// green rubber feet still on, a towel round his neck, shoulders down.
void tiredMan(cairo_t* cr, double w, double h, bool waving, int frame)
{
  const double cx = kM + w * 0.5, by = kM + h;
  const double s = h / 170.0;
  cairo_save(cr);
  cairo_translate(cr, cx, by);
  cairo_scale(cr, s, s);
  const double ph = frame * kPi / 2, sw = std::sin(ph);
  const double bob = std::abs(std::cos(ph)) * 3.0;
  const Color skin = rgb(236, 196, 160), shirt = rgb(240, 236, 224), pants = rgb(70, 74, 92);
  // The legs, the back one first; the suit's green feet.
  auto leg = [&](double a, bool back) {
    const double hx = 0, hy = -76 + bob;
    const double kx = hx + std::sin(a) * 38, ky = hy + std::cos(a) * 38;
    const double fx = kx + std::sin(a * 0.4) * 36, fy = std::min(-6.0, ky + std::cos(a * 0.4) * 36);
    limb(cr, {{hx, hy}, {kx, ky}, {fx, fy}}, 19, back ? darken(pants, 0.25f) : pants, 1.8);
    ellipse(cr, fx + 7, fy + 1, 13, 7);
    fillGrad(cr, 0, fy - 6, 0, fy + 8, kSuitLight, kSuitDark, 1.8);
  };
  leg(-sw * 0.45, true);
  // The back arm, hanging.
  limb(cr, {{-8, -124 + bob}, {-12 - sw * 6, -98 + bob}, {-8 - sw * 10, -74 + bob}}, 13, darken(skin, 0.15f), 1.6);
  leg(sw * 0.45, false);
  // The undershirt, its sweat patches.
  cairo_move_to(cr, -18, -130 + bob);
  cairo_curve_to(cr, -24, -110 + bob, -22, -90 + bob, -17, -70 + bob);
  cairo_line_to(cr, 19, -70 + bob);
  cairo_curve_to(cr, 30, -90 + bob, 26, -116 + bob, 16, -130 + bob);
  cairo_curve_to(cr, 6, -122 + bob, -8, -122 + bob, -18, -130 + bob);
  cairo_close_path(cr);
  fillGrad(cr, -16, 0, 16, 0, shirt, darken(shirt, 0.18f), 2.0);
  ellipse(cr, 0, -110 + bob, 7, 10);
  setRgba(cr, rgb(200, 190, 160), 0.45);
  cairo_fill(cr);
  // The towel round his neck.
  cairo_move_to(cr, -14, -132 + bob);
  cairo_curve_to(cr, -6, -124 + bob, 8, -124 + bob, 14, -132 + bob);
  cairo_line_to(cr, 16, -104 + bob);
  cairo_line_to(cr, 8, -104 + bob);
  cairo_line_to(cr, 8, -126 + bob);
  cairo_close_path(cr);
  fillGrad(cr, 0, -132 + bob, 0, -104 + bob, rgb(250, 110, 110), rgb(190, 60, 60), 1.6);
  // The head, hung a little, a crew cut, sweat flying.
  ball(cr, 4, -146 + bob, 14, skin, 2.0);
  cairo_move_to(cr, -10, -150 + bob);
  cairo_curve_to(cr, -8, -164 + bob, 12, -166 + bob, 18, -152 + bob);
  cairo_line_to(cr, 14, -156 + bob);
  cairo_curve_to(cr, 6, -160 + bob, -4, -158 + bob, -10, -150 + bob);
  cairo_close_path(cr);
  setColor(cr, rgb(70, 50, 36));
  cairo_fill(cr);
  cairo_move_to(cr, 9, -146 + bob);
  cairo_line_to(cr, 14, -145 + bob);
  ink(cr, 1.8);
  cairo_move_to(cr, 8, -138 + bob);
  cairo_curve_to(cr, 11, -137 + bob, 14, -137 + bob, 16, -139 + bob);
  ink(cr, 1.4);
  ellipse(cr, -10, -150 + bob, 1.8, 3.2, 0.4);
  setRgba(cr, rgb(200, 230, 255), 0.9);
  cairo_fill(cr);
  // The front arm: hanging, or waving over his head.
  if (waving)
  {
    const double a = -0.5 + 0.35 * std::sin(frame * kPi / 2);
    const double ex = 30, ey = -134 + bob;
    const double wx = ex + std::sin(a) * 24, wy = ey - std::cos(a) * 24;
    limb(cr, {{12, -124 + bob}, {ex, ey}, {wx, wy}}, 12, skin, 1.6);
    ball(cr, wx, wy - 2, 6.5, skin, 1.4);
  }
  else
  {
    limb(cr, {{10, -124 + bob}, {14 + sw * 6, -98 + bob}, {12 + sw * 10, -74 + bob}}, 12, skin, 1.6);
    ball(cr, 12 + sw * 10, -72 + bob, 6, skin, 1.4);
  }
  cairo_restore(cr);
}

// The duck's monster suit: the tail behind it (variant 0); over it, a green
// rubber body with belly plates and a zipper, and a hood with plates and
// googly eyes on top, its face peeking out (variant 1). The duck icon's body
// is round (30, 42), its head at (38, 24).
void duckSuit(cairo_t* cr, double w, double h, int variant)
{
  const double s = std::min(w, h) / 64.0;
  cairo_save(cr);
  cairo_translate(cr, kM, kM);
  cairo_scale(cr, s, s);
  if (variant == 0)
  {
    taperPath(cr, 14, 46, 4, 50, 0, 58, -6, 56, 9, 2);
    fillGrad(cr, -6, 46, 14, 58, kSuitLight, kSuitDark, 1.6);
    for (int i = 0; i < 3; ++i)
      plate(cr, 16 - i * 4, 30 - i * 0.5 + i * 3, 7 - i, -0.7);
  }
  else
  {
    // The body suit.
    cairo_save(cr);
    cairo_translate(cr, 30, 43);
    cairo_scale(cr, 1.0, 0.7);
    cairo_arc(cr, 0, 0, 20.5, 0, 2 * kPi);
    cairo_restore(cr);
    fillGrad(cr, 0, 28, 0, 58, kSuitLight, kSuitDark, 2.0);
    ellipse(cr, 38, 46, 9, 7);
    fillGrad(cr, 0, 39, 0, 53, lighten(kBelly, 0.2f), darken(kBelly, 0.2f), 1.2);
    for (int i = 0; i < 3; ++i)
    {
      cairo_move_to(cr, 31, 42 + i * 4);
      cairo_line_to(cr, 45, 42 + i * 4);
    }
    setRgba(cr, darken(kBelly, 0.4f), 0.7);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);
    cairo_move_to(cr, 14, 36);
    cairo_curve_to(cr, 12, 42, 14, 48, 20, 52);
    setRgba(cr, rgb(220, 220, 220), 0.9);
    cairo_set_line_width(cr, 1.2);
    cairo_stroke(cr);
    // The hood over the head, its jaw over the duck's brow.
    cairo_move_to(cr, 25, 30);
    cairo_curve_to(cr, 22, 14, 34, 7, 44, 10);
    cairo_curve_to(cr, 52, 12, 56, 18, 54, 22);
    cairo_line_to(cr, 44, 21);
    cairo_curve_to(cr, 38, 22, 34, 28, 34, 34);
    cairo_close_path(cr);
    fillGrad(cr, 0, 8, 0, 34, kSuitLight, kSuitDark, 1.8);
    for (int k = 0; k < 4; ++k)
    {
      cairo_move_to(cr, 42 + k * 3, 21);
      cairo_line_to(cr, 43.5 + k * 3, 24);
      cairo_line_to(cr, 45 + k * 3, 21);
      cairo_close_path(cr);
      setColor(cr, rgb(250, 248, 236));
      cairo_fill(cr);
    }
    for (int i = 0; i < 3; ++i)
      plate(cr, 30 - i * 3, 13 + i * 6, 7 - i, -0.9 - i * 0.2);
    ball(cr, 40, 11, 3.6, rgb(250, 230, 120), 1.2);
    circle(cr, 41, 11, 1.4);
    setColor(cr, kInk);
    cairo_fill(cr);
    ball(cr, 47, 12.5, 3.2, rgb(250, 230, 120), 1.2);
    circle(cr, 48, 12.5, 1.3);
    setColor(cr, kInk);
    cairo_fill(cr);
  }
  cairo_restore(cr);
}

} // namespace

void paintGorgazilla(cairo_t* cr, double cx, double feetY, double s, int pose, int frame)
{
  paintLizard(cr, cx, feetY, s, lizardPose(pose, frame), frame);
}

bool drawCityArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame)
{
  (void)t;
  if (key == "model_tank")
  {
    if (variant >= 2)
      modelTankTop(cr, w, h, variant == 3, frame);
    else
      modelTank(cr, w, h, variant == 1, frame);
  }
  else if (key == "toy_jet")
    toyJet(cr, w, h, std::clamp(variant, 0, 4), frame);
  else if (key == "maser_truck")
    maserTruck(cr, w, h, variant == 2 ? -1 : variant, frame);
  else if (key == "gorgazilla")
    paintLizard(cr, kM + w * 0.5, kM + h, h / 192.0, lizardPose(variant, frame), frame);
  else if (key == "tired_man")
    tiredMan(cr, w, h, variant == 1, frame);
  else if (key == "duck_suit")
    duckSuit(cr, w, h, variant);
  else
    return false;
  return true;
}

} // namespace gr
