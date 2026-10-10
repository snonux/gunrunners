// Station Zero's crew (Episode 3) in the style of enemy_art.cpp: each drawn
// in screen pixels into a texture with a 32 px margin, the cell box at
// (32, 32) .. (32 + w, 32 + h), facing right.

#include "assets/enemy_art_station.hpp"

#include <cmath>

namespace gr
{

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr Color kInk = rgb(14, 16, 28);
constexpr double kLine = 2.4;
constexpr double kM = 32.0;

void setColor(cairo_t* cr, Color c) { cairo_set_source_rgb(cr, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0); }

void setRgba(cairo_t* cr, Color c, double a)
{
  cairo_set_source_rgba(cr, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, a);
}

void circle(cairo_t* cr, double x, double y, double r) { cairo_arc(cr, x, y, r, 0, 2 * kPi); }

void rounded(cairo_t* cr, double x, double y, double w, double h, double r)
{
  cairo_new_sub_path(cr);
  cairo_arc(cr, x + w - r, y + r, r, -kPi / 2, 0);
  cairo_arc(cr, x + w - r, y + h - r, r, 0, kPi / 2);
  cairo_arc(cr, x + r, y + h - r, r, kPi / 2, kPi);
  cairo_arc(cr, x + r, y + r, r, kPi, 1.5 * kPi);
  cairo_close_path(cr);
}

// Fills the current path top to bottom from `top` to `bottom`, then inks it.
void fillShaded(cairo_t* cr, double y0, double y1, Color top, Color bottom)
{
  cairo_pattern_t* g = cairo_pattern_create_linear(0, y0, 0, y1);
  cairo_pattern_add_color_stop_rgb(g, 0, redOf(top) / 255.0, greenOf(top) / 255.0, blueOf(top) / 255.0);
  cairo_pattern_add_color_stop_rgb(g, 1, redOf(bottom) / 255.0, greenOf(bottom) / 255.0, blueOf(bottom) / 255.0);
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  setColor(cr, kInk);
  cairo_set_line_width(cr, kLine);
  cairo_stroke(cr);
}

void glow(cairo_t* cr, double x, double y, double r, Color c, double a)
{
  cairo_pattern_t* g = cairo_pattern_create_radial(x, y, 0, x, y, r);
  cairo_pattern_add_color_stop_rgba(g, 0, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, a);
  cairo_pattern_add_color_stop_rgba(g, 1, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, 0);
  cairo_set_source(cr, g);
  circle(cr, x, y, r);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
}

void hazardStripe(cairo_t* cr, double x, double y, double w, double h)
{
  cairo_save(cr);
  cairo_rectangle(cr, x, y, w, h);
  cairo_clip(cr);
  setColor(cr, rgb(255, 196, 30));
  cairo_paint(cr);
  setColor(cr, kInk);
  for (double k = -h; k < w + h; k += 12)
  {
    cairo_move_to(cr, x + k, y + h);
    cairo_line_to(cr, x + k + 6, y + h);
    cairo_line_to(cr, x + k + 6 + h, y);
    cairo_line_to(cr, x + k + h, y);
    cairo_close_path(cr);
    cairo_fill(cr);
  }
  cairo_restore(cr);
}

// Loader Mech: a squat yellow cargo walker, a cab with a lit visor on a
// boxy body, two pincer arms, two thick legs on magnetic feet. Variant bit
// 0: the legs are shot off (it sits on its hips); bit 1: arms up (lifting).
void loaderMech(cairo_t* cr, double w, double h, int variant, int frame)
{
  const bool legless = variant & 1, lifting = variant & 2;
  const double x0 = kM, y0 = kM;
  const double legTop = y0 + h * 0.62;
  const double sit = legless ? h * 0.3 : 0.0;
  if (!legless)
  {
    for (int k = 0; k < 2; ++k)
    {
      const double lx = x0 + w * (0.18 + 0.48 * k);
      const double step = (frame && k == 0) ? -4.0 : 0.0;
      cairo_rectangle(cr, lx, legTop, w * 0.18, h * 0.3 + step);
      fillShaded(cr, legTop, y0 + h, rgb(90, 98, 112), rgb(50, 56, 68));
      // Magnetic foot.
      rounded(cr, lx - 6, y0 + h - 12 + step, w * 0.18 + 12, 12, 3);
      fillShaded(cr, y0 + h - 12, y0 + h, rgb(120, 130, 150), rgb(60, 66, 80));
      glow(cr, lx + w * 0.09, y0 + h - 4 + step, 10, rgb(120, 220, 255), 0.5);
    }
  }
  else
  {
    // Torn leg stumps, sparking.
    for (int k = 0; k < 2; ++k)
    {
      const double lx = x0 + w * (0.2 + 0.46 * k);
      cairo_rectangle(cr, lx, y0 + h - 14, w * 0.16, 14);
      fillShaded(cr, y0 + h - 14, y0 + h, rgb(70, 76, 88), rgb(40, 44, 54));
      glow(cr, lx + w * 0.08, y0 + h - 14, 12, rgb(255, 200, 60), frame ? 0.8 : 0.4);
    }
  }
  // Body.
  const double by = y0 + h * 0.3 + sit, bh = h * 0.36;
  rounded(cr, x0 + w * 0.06, by, w * 0.88, bh, 10);
  fillShaded(cr, by, by + bh, rgb(255, 210, 70), rgb(200, 140, 30));
  hazardStripe(cr, x0 + w * 0.1, by + bh - 16, w * 0.8, 10);
  // Cab and visor.
  const double cy = y0 + h * 0.1 + sit;
  rounded(cr, x0 + w * 0.3, cy, w * 0.5, h * 0.22, 8);
  fillShaded(cr, cy, cy + h * 0.22, rgb(240, 196, 60), rgb(190, 130, 30));
  rounded(cr, x0 + w * 0.5, cy + 10, w * 0.26, h * 0.1, 4);
  fillShaded(cr, cy + 10, cy + 10 + h * 0.1, rgb(140, 230, 255), rgb(40, 100, 160));
  glow(cr, x0 + w * 0.66, cy + 16, 18, rgb(120, 220, 255), 0.6);
  // Arms with pincers, down at its sides or up over its head.
  for (int k = 0; k < 2; ++k)
  {
    const double ax = k == 0 ? x0 + w * 0.02 : x0 + w * 0.86;
    if (lifting)
    {
      cairo_rectangle(cr, ax, y0 - 6 + sit, 16, by - y0 + 16);
      fillShaded(cr, y0, by, rgb(110, 120, 136), rgb(70, 78, 92));
    }
    else
    {
      cairo_rectangle(cr, ax, by + 8, 16, bh + 10);
      fillShaded(cr, by, by + bh + 18, rgb(110, 120, 136), rgb(70, 78, 92));
      cairo_move_to(cr, ax - 4, by + bh + 18);
      cairo_line_to(cr, ax + 20, by + bh + 18);
      cairo_line_to(cr, ax + 16, by + bh + 30);
      cairo_line_to(cr, ax, by + bh + 30);
      cairo_close_path(cr);
      fillShaded(cr, by + bh + 18, by + bh + 30, rgb(150, 160, 176), rgb(80, 88, 100));
    }
  }
  // Frost on its shoulders.
  setRgba(cr, rgb(230, 245, 255), 0.8);
  cairo_rectangle(cr, x0 + w * 0.1, by + 2, w * 0.3, 4);
  cairo_fill(cr);
}

// Weld Drone: a small round crawler, a torch arm out front. Variant 1: the
// torch flares white; 2: clinging to a wall (drawn turned up).
void weldDrone(cairo_t* cr, double w, double h, int variant, int frame)
{
  const double cx = kM + w * 0.5, cy = kM + h * 0.55;
  if (variant == 2)
  {
    cairo_save(cr);
    cairo_translate(cr, cx, cy);
    cairo_rotate(cr, -kPi / 2);
    cairo_translate(cr, -cx, -cy);
  }
  // Treads.
  rounded(cr, kM + 2, kM + h - 14, w - 4, 12, 5);
  fillShaded(cr, kM + h - 14, kM + h, rgb(70, 76, 90), rgb(30, 34, 44));
  // Shell.
  circle(cr, cx, cy, w * 0.36);
  fillShaded(cr, cy - w * 0.36, cy + w * 0.36, rgb(200, 210, 224), rgb(110, 120, 138));
  hazardStripe(cr, cx - w * 0.3, cy - 3, w * 0.6, 6);
  // Torch arm and tip.
  cairo_move_to(cr, cx + w * 0.2, cy);
  cairo_line_to(cr, cx + w * 0.48, cy + h * 0.2);
  setColor(cr, rgb(80, 86, 100));
  cairo_set_line_width(cr, 5);
  cairo_stroke(cr);
  const Color flame = variant == 1 ? rgb(255, 255, 255) : rgb(255, 160, 40);
  glow(cr, cx + w * 0.5, cy + h * 0.24, variant == 1 ? 22 : 12 + (frame ? 3 : 0), flame, 0.9);
  // Eye.
  circle(cr, cx - w * 0.1, cy - h * 0.1, 4);
  setColor(cr, rgb(255, 80, 40));
  cairo_fill(cr);
  if (variant == 2)
    cairo_restore(cr);
}

// Tether Drone: a hovering disc with a beam emitter underneath (the lower
// drone's on top). Variant 1: wobbling red before a ram.
void tetherDrone(cairo_t* cr, double w, double h, int variant, int frame)
{
  const double cx = kM + w * 0.5, cy = kM + h * 0.5;
  const double tilt = variant == 1 ? (frame ? 0.25 : -0.25) : 0.0;
  cairo_save(cr);
  cairo_translate(cr, cx, cy);
  cairo_rotate(cr, tilt);
  cairo_scale(cr, 1.0, 0.55);
  circle(cr, 0, 0, w * 0.48);
  cairo_restore(cr);
  fillShaded(cr, cy - h * 0.3, cy + h * 0.3, rgb(190, 200, 220), rgb(90, 100, 124));
  circle(cr, cx, cy - h * 0.12, w * 0.2);
  fillShaded(cr, cy - h * 0.3, cy, rgb(255, 150, 220), rgb(200, 40, 140));
  glow(cr, cx, cy - h * 0.12, w * 0.4, variant == 1 ? rgb(255, 60, 60) : rgb(255, 80, 180), 0.6);
  // Emitters, top and bottom.
  cairo_rectangle(cr, cx - 5, cy + h * 0.18, 10, h * 0.2);
  setColor(cr, rgb(60, 64, 80));
  cairo_fill(cr);
  cairo_rectangle(cr, cx - 5, cy - h * 0.38, 10, h * 0.12);
  cairo_fill(cr);
}

// An asteroid of the belt (Level 15's bonus): a lumpy rock filling its
// w x h box, cratered, frost on its sunny side. Variant: which rock.
void beltAsteroid(cairo_t* cr, double w, double h, int variant)
{
  const double cx = kM + w * 0.5, cy = kM + h * 0.5, rx = w * 0.5, ry = h * 0.5;
  const Color base = variant == 1 ? rgb(132, 110, 98) : (variant == 2 ? rgb(100, 104, 120) : rgb(118, 112, 106));
  unsigned seed = 2654435761u * unsigned(variant + 7) + unsigned(w * 13 + h);
  auto rnd = [&seed](double lo, double hi) {
    seed = seed * 1664525u + 1013904223u;
    return lo + (hi - lo) * double(seed >> 8) / double(1u << 24);
  };
  constexpr int n = 12;
  double pts[n][2];
  for (int k = 0; k < n; ++k)
  {
    const double a = k * 2 * kPi / n, d = rnd(0.8, 1.0);
    pts[k][0] = cx + std::cos(a) * rx * d;
    pts[k][1] = cy + std::sin(a) * ry * d;
  }
  cairo_move_to(cr, (pts[0][0] + pts[1][0]) * 0.5, (pts[0][1] + pts[1][1]) * 0.5);
  for (int k = 1; k <= n; ++k)
  {
    const auto& p = pts[k % n];
    const auto& q = pts[(k + 1) % n];
    cairo_curve_to(cr, p[0], p[1], p[0], p[1], (p[0] + q[0]) * 0.5, (p[1] + q[1]) * 0.5);
  }
  cairo_close_path(cr);
  cairo_pattern_t* g = cairo_pattern_create_radial(cx - rx * 0.3, cy - ry * 0.35, 0, cx, cy, std::max(rx, ry) * 1.1);
  cairo_pattern_add_color_stop_rgb(g, 0, std::min(255, redOf(base) + 70) / 255.0, std::min(255, greenOf(base) + 70) / 255.0,
    std::min(255, blueOf(base) + 80) / 255.0);
  cairo_pattern_add_color_stop_rgb(g, 1, redOf(base) * 0.4 / 255.0, greenOf(base) * 0.4 / 255.0, blueOf(base) * 0.45 / 255.0);
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  setColor(cr, kInk);
  cairo_set_line_width(cr, kLine);
  cairo_stroke(cr);
  // Craters, each with a lit lower rim.
  const int craters = 2 + int(w * h / 9000.0);
  for (int c = 0; c < craters; ++c)
  {
    const double x = cx + rnd(-0.5, 0.5) * rx, y = cy + rnd(-0.5, 0.5) * ry;
    const double r = std::min(rx, ry) * rnd(0.12, 0.26);
    circle(cr, x, y, r);
    setRgba(cr, rgb(40, 36, 40), 0.55);
    cairo_fill(cr);
    cairo_arc(cr, x, y, r, kPi * 0.1, kPi * 0.9);
    cairo_set_line_width(cr, std::max(1.5, r * 0.25));
    setRgba(cr, rgb(220, 210, 200), 0.6);
    cairo_stroke(cr);
  }
  // Frost on the sunny side.
  cairo_arc(cr, cx, cy, std::min(rx, ry) * 0.85, kPi * 1.1, kPi * 1.45);
  cairo_set_line_width(cr, 4);
  setRgba(cr, rgb(230, 245, 255), 0.7);
  cairo_stroke(cr);
}

} // namespace

bool drawStationArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame)
{
  (void)t;
  if (key == "loader_mech")
    loaderMech(cr, w, h, variant, frame);
  else if (key == "weld_drone")
    weldDrone(cr, w, h, variant, frame);
  else if (key == "tether_pair")
    tetherDrone(cr, w, h, variant, frame);
  else if (key == "belt_asteroid")
    beltAsteroid(cr, w, h, variant);
  else
    return false;
  return true;
}

} // namespace gr
