// Level 47, Silk Canyon (Episode 7, DEEP SPACE): the Loom Spider, the
// Cocoon Pod and its Dropling, and the cocoons hanging on the webs, in the
// style of enemy_art.cpp: each drawn in screen pixels into a texture with a
// 32 px margin, the cell box at (32, 32) .. (32 + w, 32 + h), facing right.

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
constexpr Color kInk = rgb(40, 20, 12);
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

void setRgba(cairo_t* cr, Color c, double a)
{
  cairo_set_source_rgba(cr, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, a);
}

struct Box
{
  cairo_t* cr;
  const Theme& t;
  double w, h;
  int variant, frame;
};

// A jointed leg from (x, y) out to (x + dx, y + dy), the knee up high.
void leg(cairo_t* cr, double x, double y, double dx, double dy, double knee, Color c)
{
  cairo_move_to(cr, x, y);
  cairo_line_to(cr, x + dx * 0.55, y - knee);
  cairo_line_to(cr, x + dx, y + dy);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
  cairo_set_line_width(cr, 5.0);
  setRgba(cr, kInk, 1.0);
  cairo_stroke_preserve(cr);
  cairo_set_line_width(cr, 2.6);
  setRgba(cr, c, 1.0);
  cairo_stroke(cr);
}

// Wrapping: pale silk bands round an egg shape.
void wrap(cairo_t* cr, double cx, double cy, double rx, double ry, Color silk, Color shade, int bands)
{
  ellipse(cr, cx, cy, rx, ry);
  cairo_pattern_t* p = cairo_pattern_create_linear(cx - rx, 0, cx + rx, 0);
  cairo_pattern_add_color_stop_rgba(p, 0.0, redOf(silk) / 255.0, greenOf(silk) / 255.0, blueOf(silk) / 255.0, 1.0);
  cairo_pattern_add_color_stop_rgba(p, 1.0, redOf(shade) / 255.0, greenOf(shade) / 255.0, blueOf(shade) / 255.0, 1.0);
  cairo_set_source(cr, p);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(p);
  setRgba(cr, kInk, 1.0);
  cairo_set_line_width(cr, kLine);
  cairo_stroke(cr);
  for (int k = 1; k <= bands; ++k)
  {
    const double y = cy - ry + 2.0 * ry * double(k) / double(bands + 1);
    const double half = rx * std::sqrt(std::max(0.0, 1.0 - std::pow((y - cy) / ry, 2.0)));
    cairo_move_to(cr, cx - half, y - 3);
    cairo_curve_to(cr, cx - half * 0.3, y + 4, cx + half * 0.3, y + 4, cx + half, y - 3);
    cairo_set_line_width(cr, 1.6);
    setRgba(cr, darken(shade, 0.2f), 0.7);
    cairo_stroke(cr);
  }
}

// Loom Spider: a long-legged amber spider astride its line (the line runs
// through the middle of the box), four legs up on it, four hanging.
// Variant 1: fangs out and red, about to cut. Frame 1: the legs' other step.
void loomSpider(const Box& b)
{
  cairo_t* cr = b.cr;
  const double cx = kM + b.w * 0.5, top = kM + b.h * 0.5, cy = kM + b.h * 0.62;
  const Color body = rgb(196, 120, 40), bodyDark = rgb(110, 54, 20), band = rgb(250, 220, 120);
  // Eight legs: four hands up on the line, four splayed.
  for (int s : {-1, 1})
    for (int k = 0; k < 4; ++k)
    {
      const double step = (b.frame ^ (k & 1)) ? 4.0 : -4.0;
      const double reach = b.w * (0.22 + 0.1 * k) * s + step;
      if (k < 2)
        leg(cr, cx + s * 4, cy - 6, reach, top - (cy - 6), 10, body);
      else
        leg(cr, cx + s * 5, cy, reach * 1.1, b.h * 0.3, 8, body);
    }
  // Abdomen and head.
  ellipse(cr, cx - b.w * 0.1, cy, b.w * 0.26, b.h * 0.24);
  fillGradientOutline(cr, cy - b.h * 0.24, cy + b.h * 0.24, lighten(body, 0.2f), bodyDark, kInk, kLine);
  for (int k = 0; k < 3; ++k)
  {
    cairo_move_to(cr, cx - b.w * (0.24 - 0.1 * k), cy - b.h * 0.18);
    cairo_line_to(cr, cx - b.w * (0.26 - 0.1 * k), cy + b.h * 0.18);
    cairo_set_line_width(cr, 2.6);
    setRgba(cr, band, 0.85);
    cairo_stroke(cr);
  }
  circle(cr, cx + b.w * 0.2, cy - 2, b.w * 0.13);
  fillGradientOutline(cr, cy - 2 - b.w * 0.13, cy - 2 + b.w * 0.13, lighten(body, 0.3f), body, kInk, kLine);
  // Eyes: a cluster, red when it is about to cut.
  const Color eye = b.variant == 1 ? rgb(255, 60, 40) : rgb(255, 250, 200);
  for (int k = 0; k < 4; ++k)
  {
    circle(cr, cx + b.w * 0.24 + (k % 2) * 4.0, cy - 6 + (k / 2) * 4.0, 1.8);
    setRgba(cr, eye, 1.0);
    cairo_fill(cr);
  }
  if (b.variant == 1)
  {
    // Fangs out, like shears.
    for (int s : {-1, 1})
    {
      cairo_move_to(cr, cx + b.w * 0.3, cy + 2);
      cairo_line_to(cr, cx + b.w * 0.42, cy + 2 + s * 7);
      cairo_set_line_width(cr, 3.0);
      setRgba(cr, rgb(255, 240, 230), 1.0);
      cairo_stroke(cr);
    }
    radialGlow(cr, cx + b.w * 0.3, cy, b.w * 0.4, rgb(255, 80, 40), 0.5);
  }
}

// Cocoon Pod: a fat silk sack with a dark slit; something moves inside.
// Variant 1: the slit open (its Dropling is out). Frame 1: a bulge.
void cocoonPod(const Box& b)
{
  cairo_t* cr = b.cr;
  const double cx = kM + b.w * 0.5, cy = kM + b.h * 0.5;
  wrap(cr, cx, cy, b.w * 0.46, b.h * 0.48, rgb(240, 226, 196), rgb(170, 140, 100), 4);
  if (b.frame == 1 && b.variant == 0)
  {
    ellipse(cr, cx + 4, cy + 6, b.w * 0.16, b.h * 0.12);
    setRgba(cr, rgb(120, 90, 60), 0.35);
    cairo_fill(cr);
  }
  // The slit at the bottom.
  cairo_move_to(cr, cx - b.w * 0.18, cy + b.h * 0.3);
  cairo_curve_to(cr, cx - 4, cy + b.h * (b.variant ? 0.5 : 0.4), cx + 4, cy + b.h * (b.variant ? 0.5 : 0.4),
    cx + b.w * 0.18, cy + b.h * 0.3);
  cairo_set_line_width(cr, b.variant ? 5.0 : 3.0);
  setRgba(cr, rgb(50, 20, 10), 1.0);
  cairo_stroke(cr);
}

// Dropling: a small round biter on a thread, legs tucked, big eyes.
// Variant 1: biting, its mouth wide.
void dropling(const Box& b)
{
  cairo_t* cr = b.cr;
  const double cx = kM + b.w * 0.5, cy = kM + b.h * 0.55;
  const Color body = rgb(120, 70, 150);
  for (int s : {-1, 1})
    for (int k = 0; k < 3; ++k)
      leg(cr, cx + s * 3, cy, s * b.w * (0.3 + 0.08 * k) + (b.frame ? s * 2 : 0), b.h * 0.25 - k * 3, 6,
        lighten(body, 0.2f));
  circle(cr, cx, cy, b.w * 0.32);
  fillGradientOutline(cr, cy - b.w * 0.32, cy + b.w * 0.32, lighten(body, 0.35f), darken(body, 0.2f), kInk, kLine);
  for (int s : {-1, 1})
  {
    circle(cr, cx + s * 5, cy - 4, 4.0);
    setRgba(cr, rgb(255, 255, 230), 1.0);
    cairo_fill(cr);
    circle(cr, cx + s * 5 + 1, cy - 3, 1.8);
    setRgba(cr, kInk, 1.0);
    cairo_fill(cr);
  }
  if (b.variant == 1)
  {
    ellipse(cr, cx, cy + 7, 6, 4);
    setRgba(cr, rgb(200, 30, 60), 1.0);
    cairo_fill(cr);
    for (int s : {-1, 1})
    {
      cairo_move_to(cr, cx + s * 4, cy + 4);
      cairo_line_to(cr, cx + s * 3, cy + 9);
      cairo_set_line_width(cr, 1.6);
      setRgba(cr, rgb(255, 255, 255), 1.0);
      cairo_stroke(cr);
    }
  }
}

// A cocoon on the web: variant 0 pale with a gleam of gems through the
// silk, 1 the green one (the Virus) dripping, 2 an empty torn husk.
void silkCocoon(const Box& b)
{
  cairo_t* cr = b.cr;
  const double cx = kM + b.w * 0.5, cy = kM + b.h * 0.5;
  if (b.variant == 2)
  {
    cairo_move_to(cr, cx - b.w * 0.3, cy - b.h * 0.4);
    cairo_curve_to(cr, cx - b.w * 0.5, cy, cx - b.w * 0.2, cy + b.h * 0.2, cx - b.w * 0.1, cy + b.h * 0.1);
    cairo_line_to(cr, cx, cy + b.h * 0.3);
    cairo_line_to(cr, cx + b.w * 0.12, cy + b.h * 0.08);
    cairo_curve_to(cr, cx + b.w * 0.3, cy + b.h * 0.2, cx + b.w * 0.45, cy, cx + b.w * 0.3, cy - b.h * 0.4);
    cairo_close_path(cr);
    setRgba(cr, rgb(220, 205, 175), 0.8);
    cairo_fill_preserve(cr);
    setRgba(cr, kInk, 0.8);
    cairo_set_line_width(cr, 1.8);
    cairo_stroke(cr);
    return;
  }
  const bool virus = b.variant == 1;
  wrap(cr, cx, cy, b.w * 0.4, b.h * 0.46, virus ? rgb(190, 255, 120) : rgb(250, 240, 214),
    virus ? rgb(60, 150, 40) : rgb(190, 160, 120), 5);
  if (virus)
  {
    // Drips.
    for (int k = 0; k < 2; ++k)
    {
      const double x = cx - 5 + k * 10, y = cy + b.h * 0.42 + (b.frame ? 5 : 0) * (k ? 1 : 0.5);
      ellipse(cr, x, y, 3, 4.5);
      setRgba(cr, rgb(150, 255, 70), 0.9);
      cairo_fill(cr);
    }
    return;
  }
  // The gems glinting through.
  static const Color kGems[] = {rgb(255, 90, 150), rgb(90, 220, 255), rgb(255, 220, 80)};
  for (int k = 0; k < 3; ++k)
  {
    circle(cr, cx - 6 + k * 6, cy - 8 + (k % 2) * 12, 3.0);
    setRgba(cr, kGems[k], b.frame == (k & 1) ? 0.85 : 0.5);
    cairo_fill(cr);
  }
}

using DrawFn = void (*)(const Box&);

} // namespace

bool drawSilkArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame)
{
  static const std::map<std::string, DrawFn> kRoutines{
    {"loom_spider", loomSpider},
    {"cocoon_pod", cocoonPod},
    {"dropling", dropling},
    {"silk_cocoon", silkCocoon},
  };
  const auto it = kRoutines.find(key);
  if (it == kRoutines.end())
    return false;
  it->second(Box{cr, t, w, h, variant, frame});
  return true;
}

} // namespace gr
