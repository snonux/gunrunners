// The aliens of Vurr (Episode 7, DEEP SPACE) and their goo, in the style of
// enemy_art.cpp: each drawn in screen pixels into a texture with a 32 px
// margin, the cell box at (32, 32) .. (32 + w, 32 + h), facing right.

#include "assets/enemy_art_alien.hpp"

#include <cmath>
#include <map>

namespace gr
{

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr Color kInk = rgb(18, 10, 30);
constexpr double kLine = 2.4;
constexpr double kM = 32.0; // margin
const Color kGooLight = rgb(200, 255, 140);
const Color kGoo = rgb(130, 240, 80);
const Color kGooDeep = rgb(40, 140, 60);

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

void setRgba(cairo_t* cr, Color c, double a)
{
  cairo_set_source_rgba(cr, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, a);
}

// A glossy highlight: a pale crescent on the upper left of a round shape.
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

// Skitter: a flat six-legged scuttler, violet chitin, a cluster of glowing
// eyes in front. Variant 0 runs (legs alternate by frame), 1 clicks (front
// up, mandibles open), 2 leaps (legs tucked under).
void skitter(const Box& b)
{
  cairo_t* cr = b.cr;
  const double x0 = kM, y0 = kM, w = b.w, h = b.h;
  const bool click = b.variant == 1, leap = b.variant == 2;
  const double tilt = click ? -0.16 : (leap ? -0.08 : 0.0);
  cairo_save(cr);
  cairo_translate(cr, x0 + w * 0.5, y0 + h * 0.62);
  cairo_rotate(cr, tilt);
  // Legs: three a side, jointed, the near ones drawn after the body.
  const Color leg = rgb(70, 40, 96), legLight = rgb(120, 80, 150);
  auto legs = [&](bool near) {
    for (int i = 0; i < 3; ++i)
    {
      const double hx = -w * 0.25 + double(i) * w * 0.22;
      const double swing = leap ? 0.0 : ((i + b.frame + (near ? 1 : 0)) % 2 == 0 ? 6.0 : -6.0);
      const double kx = hx + (leap ? -4.0 : swing * 0.5) - 6.0, ky = -h * 0.28;
      const double fx = hx + (leap ? -14.0 : swing) - 4.0, fy = leap ? h * 0.05 : h * 0.36;
      strokeLimb(cr, {{hx, 0.0}, {kx, ky}, {fx, fy}}, near ? 5.0 : 4.0, near ? legLight : leg, kInk, 1.4);
    }
  };
  legs(false);
  // The body: a low oval shell with ridges.
  ellipse(cr, 0.0, -h * 0.05, w * 0.42, h * 0.3);
  cairo_pattern_t* p = cairo_pattern_create_linear(0, -h * 0.35, 0, h * 0.25);
  cairo_pattern_add_color_stop_rgb(p, 0, 0.62, 0.42, 0.86);
  cairo_pattern_add_color_stop_rgb(p, 1, 0.22, 0.12, 0.36);
  cairo_set_source(cr, p);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(p);
  setColor(cr, kInk);
  cairo_set_line_width(cr, kLine);
  cairo_stroke(cr);
  for (int i = 0; i < 3; ++i)
  {
    const double rx = -w * 0.22 + double(i) * w * 0.16;
    cairo_move_to(cr, rx, -h * 0.3);
    cairo_curve_to(cr, rx + 6, -h * 0.12, rx + 6, h * 0.05, rx, h * 0.18);
  }
  setRgba(cr, rgb(40, 200, 200), 0.8);
  cairo_set_line_width(cr, 2.2);
  cairo_stroke(cr);
  gloss(cr, 0.0, -h * 0.05, w * 0.42, h * 0.3, 0.35);
  // Head: mandibles and the eye cluster, glowing brighter while it clicks.
  const double hx = w * 0.4, hy = -h * 0.04;
  const double open = click ? 0.5 : 0.18;
  for (int s : {-1, 1})
  {
    cairo_save(cr);
    cairo_translate(cr, hx + 4, hy + 4);
    cairo_rotate(cr, double(s) * open);
    cairo_move_to(cr, 0, 0);
    cairo_curve_to(cr, 10, -3 * s, 16, 2 * s, 14, 8 * s);
    cairo_set_line_width(cr, 3.4);
    setColor(cr, rgb(220, 210, 160));
    cairo_stroke(cr);
    cairo_restore(cr);
  }
  circle(cr, hx, hy, h * 0.17);
  fillOutline(cr, rgb(90, 50, 120), kInk, kLine);
  radialGlow(cr, hx + 3, hy - 2, click ? 22 : 14, rgb(80, 255, 220), click ? 0.8 : 0.5);
  for (int i = 0; i < 3; ++i)
  {
    circle(cr, hx + 2 + double(i % 2) * 6, hy - 5 + double(i) * 4, 2.6);
    setColor(cr, rgb(170, 255, 240));
    cairo_fill(cr);
  }
  legs(true);
  cairo_restore(cr);
}

// Spitpod: a rooted plant, a ribbed stalk and a bulb with a puckered mouth
// facing right. Variant 1: the bulb swells and glows before it spits.
void spitpod(const Box& b)
{
  cairo_t* cr = b.cr;
  const double x0 = kM, y0 = kM, w = b.w, h = b.h;
  const bool swell = b.variant == 1;
  const double sway = b.frame ? 2.0 : -2.0;
  // Roots and leaves at the foot.
  for (int s : {-1, 1})
  {
    cairo_move_to(cr, x0 + w * 0.5, y0 + h);
    cairo_curve_to(cr, x0 + w * 0.5 + s * w * 0.2, y0 + h * 0.8, x0 + w * 0.5 + s * w * 0.45, y0 + h * 0.78,
      x0 + w * 0.5 + s * w * 0.5, y0 + h * 0.9);
    cairo_curve_to(cr, x0 + w * 0.5 + s * w * 0.32, y0 + h * 0.92, x0 + w * 0.5 + s * w * 0.15, y0 + h * 0.98,
      x0 + w * 0.5, y0 + h);
    fillOutline(cr, rgb(50, 140, 110), kInk, 1.8);
  }
  // The stalk.
  const double bx = x0 + w * 0.52 + sway, by = y0 + h * (swell ? 0.3 : 0.34);
  strokeLimb(cr, {{x0 + w * 0.5, y0 + h * 0.98}, {x0 + w * 0.46, y0 + h * 0.7}, {bx - 2, by + h * 0.12}}, 11.0,
    rgb(70, 160, 120), kInk, 1.8);
  // The bulb.
  const double r = w * (swell ? 0.4 : 0.32);
  ellipse(cr, bx, by, r, r * 0.92);
  cairo_pattern_t* p = cairo_pattern_create_radial(bx - r * 0.3, by - r * 0.3, 0, bx, by, r);
  const Color hi = swell ? rgb(230, 255, 150) : rgb(220, 120, 210), lo = swell ? rgb(90, 190, 60) : rgb(120, 40, 120);
  cairo_pattern_add_color_stop_rgb(p, 0, redOf(hi) / 255.0, greenOf(hi) / 255.0, blueOf(hi) / 255.0);
  cairo_pattern_add_color_stop_rgb(p, 1, redOf(lo) / 255.0, greenOf(lo) / 255.0, blueOf(lo) / 255.0);
  cairo_set_source(cr, p);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(p);
  setColor(cr, kInk);
  cairo_set_line_width(cr, kLine);
  cairo_stroke(cr);
  // Spots, glowing acid green inside when swollen.
  for (int i = 0; i < 4; ++i)
  {
    const double a = 2.2 + double(i) * 0.9;
    circle(cr, bx + std::cos(a) * r * 0.55, by + std::sin(a) * r * 0.5, r * 0.11);
    setColor(cr, swell ? rgb(240, 255, 160) : rgb(255, 180, 240));
    cairo_fill(cr);
  }
  if (swell)
    radialGlow(cr, bx, by, r * 1.8, kGoo, 0.5);
  gloss(cr, bx, by, r, r * 0.92, 0.3);
  // The mouth, on the right.
  const double mx = bx + r * 0.85, my = by + r * 0.05;
  ellipse(cr, mx, my, r * (swell ? 0.22 : 0.16), r * (swell ? 0.3 : 0.2));
  fillOutline(cr, rgb(60, 10, 40), kInk, 2.0);
  if (swell)
  {
    circle(cr, mx + 2, my, r * 0.12);
    setColor(cr, kGooLight);
    cairo_fill(cr);
  }
}

// Gloop: a translucent goo blob with two eyes on stalks. Variant 0 sits
// (a slow wobble), 1 squashes before a hop, 2 is in the air.
void gloop(const Box& b)
{
  cairo_t* cr = b.cr;
  const double x0 = kM, y0 = kM, w = b.w, h = b.h;
  double sx = 1.0, sy = 1.0;
  if (b.variant == 1)
  {
    sx = 1.15;
    sy = 0.72;
  }
  else if (b.variant == 2)
  {
    sx = 0.86;
    sy = 1.12;
  }
  else if (b.frame)
  {
    sx = 1.04;
    sy = 0.95;
  }
  const double cx = x0 + w * 0.5, base = y0 + h, rx = w * 0.46 * sx, ry = h * 0.42 * sy;
  const double cy = base - ry;
  // The eyes on their stalks, behind the body's top.
  for (int s : {-1, 1})
  {
    const double ex = cx + double(s) * rx * 0.35 + rx * 0.15, ey = cy - ry * 1.05;
    strokeLimb(cr, {{cx + double(s) * rx * 0.25 + rx * 0.1, cy - ry * 0.6}, {ex, ey}}, 4.0, kGoo, kInk, 1.2);
    circle(cr, ex, ey, w * 0.09);
    fillOutline(cr, rgb(250, 255, 235), kInk, 1.6);
    circle(cr, ex + w * 0.03, ey, w * 0.04);
    setColor(cr, kInk);
    cairo_fill(cr);
  }
  // The body: a dome with a flat, spreading foot.
  cairo_move_to(cr, cx - rx * 1.08, base);
  cairo_curve_to(cr, cx - rx * 1.1, base - ry * 1.3, cx - rx * 0.4, cy - ry, cx, cy - ry);
  cairo_curve_to(cr, cx + rx * 0.4, cy - ry, cx + rx * 1.1, base - ry * 1.3, cx + rx * 1.08, base);
  cairo_close_path(cr);
  cairo_pattern_t* p = cairo_pattern_create_linear(0, cy - ry, 0, base);
  cairo_pattern_add_color_stop_rgba(p, 0, 0.75, 1.0, 0.55, 0.92);
  cairo_pattern_add_color_stop_rgba(p, 1, 0.16, 0.55, 0.24, 0.95);
  cairo_set_source(cr, p);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(p);
  setColor(cr, darken(kGooDeep, 0.3f));
  cairo_set_line_width(cr, kLine);
  cairo_stroke(cr);
  // Bubbles and a swallowed pebble inside.
  for (int i = 0; i < 3; ++i)
  {
    circle(cr, cx - rx * 0.4 + double(i) * rx * 0.35, cy + ry * (0.15 + 0.12 * double(i % 2)), w * (0.05 + 0.02 * i));
    setRgba(cr, rgb(230, 255, 200), 0.55);
    cairo_fill(cr);
  }
  ellipse(cr, cx + rx * 0.3, cy + ry * 0.35, rx * 0.16, ry * 0.12);
  setRgba(cr, rgb(90, 60, 110), 0.8);
  cairo_fill(cr);
  gloss(cr, cx, cy, rx, ry, 0.45);
  // A grin.
  cairo_move_to(cr, cx + rx * 0.05, cy + ry * 0.05);
  cairo_curve_to(cr, cx + rx * 0.25, cy + ry * 0.25, cx + rx * 0.5, cy + ry * 0.2, cx + rx * 0.62, cy - ry * 0.02);
  cairo_set_line_width(cr, 2.4);
  setColor(cr, darken(kGooDeep, 0.4f));
  cairo_stroke(cr);
}

// Goo on a wall face, one block tall: the wall is at the left of the box,
// the coat bulges out to the right with drips. Frame 0/1: the drips move.
void gooFace(const Box& b)
{
  cairo_t* cr = b.cr;
  const double x0 = kM, y0 = kM, h = b.h;
  const double over = 10.0, out = 22.0 + (b.frame ? 2.0 : 0.0);
  cairo_move_to(cr, x0, y0 - 2);
  cairo_curve_to(cr, x0 + out * 0.8, y0 + 2, x0 + out, y0 + h * 0.3, x0 + out * 0.85, y0 + h * 0.5);
  cairo_curve_to(cr, x0 + out * 0.7, y0 + h * 0.7, x0 + out * 1.05, y0 + h * 0.85, x0 + out * 0.8, y0 + h + 2);
  cairo_line_to(cr, x0 - over, y0 + h + 2);
  cairo_line_to(cr, x0 - over, y0 - 2);
  cairo_close_path(cr);
  cairo_pattern_t* p = cairo_pattern_create_linear(x0 - over, 0, x0 + out, 0);
  cairo_pattern_add_color_stop_rgba(p, 0, 0.2, 0.6, 0.25, 0.55);
  cairo_pattern_add_color_stop_rgba(p, 0.6, 0.5, 0.95, 0.3, 0.9);
  cairo_pattern_add_color_stop_rgba(p, 1, 0.8, 1.0, 0.55, 0.95);
  cairo_set_source(cr, p);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(p);
  setRgba(cr, kGooDeep, 0.9);
  cairo_set_line_width(cr, 1.8);
  cairo_stroke(cr);
  // Drips hanging off the bottom.
  const double dy = b.frame ? 7.0 : 3.0;
  for (int i = 0; i < 2; ++i)
  {
    const double dx = x0 + 4.0 + double(i) * 9.0;
    const double len = (i == 0 ? 8.0 : 4.0) + dy;
    cairo_move_to(cr, dx - 3, y0 + h);
    cairo_curve_to(cr, dx - 3, y0 + h + len * 0.6, dx - 2, y0 + h + len, dx, y0 + h + len);
    cairo_curve_to(cr, dx + 2, y0 + h + len, dx + 3, y0 + h + len * 0.6, dx + 3, y0 + h);
    setRgba(cr, kGoo, 0.9);
    cairo_fill(cr);
  }
  // Bubbles and a wet shine.
  circle(cr, x0 + out * 0.5, y0 + h * 0.3, 2.6);
  circle(cr, x0 + out * 0.35, y0 + h * 0.72, 1.8);
  setRgba(cr, rgb(240, 255, 220), 0.8);
  cairo_fill(cr);
  cairo_move_to(cr, x0 + out * 0.62, y0 + 6);
  cairo_curve_to(cr, x0 + out * 0.8, y0 + h * 0.25, x0 + out * 0.7, y0 + h * 0.4, x0 + out * 0.6, y0 + h * 0.45);
  cairo_set_line_width(cr, 2.4);
  setRgba(cr, rgb(255, 255, 255), 0.55);
  cairo_stroke(cr);
}

// A Goo Gun blob in flight: a wobbling glob with a bright core.
void gooBlob(const Box& b)
{
  cairo_t* cr = b.cr;
  const double cx = kM + b.w * 0.5, cy = kM + b.h * 0.5, r = b.w * 0.42;
  radialGlow(cr, cx, cy, r * 2.2, kGoo, 0.5);
  ellipse(cr, cx, cy, r * (b.frame ? 1.12 : 0.95), r * (b.frame ? 0.88 : 1.04));
  fillOutline(cr, kGoo, kGooDeep, 1.8);
  // A tail behind it (to the left: shots face right).
  ellipse(cr, cx - r * 1.2, cy + 1, r * 0.4, r * 0.3);
  setRgba(cr, kGoo, 0.8);
  cairo_fill(cr);
  gloss(cr, cx, cy, r, r, 0.6);
}

// Goo gluing an alien's feet: lumps over the bottom of its box.
void gooGlue(const Box& b)
{
  cairo_t* cr = b.cr;
  const double x0 = kM, base = kM + b.h, w = b.w;
  const double top = base - std::min(b.h * 0.45, 30.0);
  cairo_move_to(cr, x0 - 6, base + 2);
  const int lumps = std::max(2, int(w / 22.0));
  for (int i = 0; i <= lumps; ++i)
  {
    const double x = x0 - 6 + (w + 12) * double(i) / double(lumps);
    const double y = top + ((i + b.frame) % 2 ? 6.0 : -2.0);
    cairo_line_to(cr, x, y);
  }
  cairo_line_to(cr, x0 + w + 6, base + 2);
  cairo_close_path(cr);
  setRgba(cr, kGoo, 0.82);
  cairo_fill_preserve(cr);
  setRgba(cr, kGooDeep, 0.9);
  cairo_set_line_width(cr, 2.0);
  cairo_stroke(cr);
  for (int i = 0; i < lumps; ++i)
  {
    circle(cr, x0 + (w + 12) * (double(i) + 0.5) / double(lumps) - 6, top + 10, 2.4);
    setRgba(cr, rgb(240, 255, 220), 0.8);
    cairo_fill(cr);
  }
}

using DrawFn = void (*)(const Box&);

} // namespace

bool drawAlienArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame)
{
  static const std::map<std::string, DrawFn> kRoutines{
    {"skitter", skitter},
    {"spitpod", spitpod},
    {"gloop", gloop},
    {"gloop_small", gloop},
    {"goo_face", gooFace},
    {"goo_blob", gooBlob},
    {"goo_glue", gooGlue},
  };
  const auto it = kRoutines.find(key);
  if (it == kRoutines.end())
    return false;
  it->second(Box{cr, t, w, h, variant, frame});
  return true;
}

} // namespace gr
