// Level 48, Bounder Plains (Episode 7, DEEP SPACE): open plains of thorny
// violet grass on Vurr under two moons. Dusky plum soil for the tiles with
// grass on top, an evening sky with a big ringed moon and a small one,
// rolling hills and rock arches far off with a herd of Bounders grazing,
// and tall thorn grass close by. Its spikes are a clump of thorn grass.

#include "assets/art_alien.hpp"

#include "base/math.hpp"
#include "render/vector.hpp"

#include <algorithm>
#include <cmath>
#include <string_view>

namespace gr
{

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr Color kInk = rgb(36, 20, 40);

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

void gradient(cairo_t* cr, double w, double h, std::initializer_list<std::pair<double, Color>> stops)
{
  cairo_pattern_t* p = cairo_pattern_create_linear(0, 0, 0, h);
  for (const auto& s : stops)
    cairo_pattern_add_color_stop_rgba(p, s.first, redOf(s.second) / 255.0, greenOf(s.second) / 255.0,
      blueOf(s.second) / 255.0, alphaOf(s.second) / 255.0);
  cairo_rectangle(cr, 0, 0, w, h);
  cairo_set_source(cr, p);
  cairo_fill(cr);
  cairo_pattern_destroy(p);
}

// A blade of thorn grass from (x, y) up `len`, bending with `bend`, a few
// hooked thorns along it.
void blade(cairo_t* cr, double x, double y, double len, double bend, double width, Color c, bool thorns)
{
  const double tx = x + bend, ty = y - len;
  cairo_move_to(cr, x - width, y);
  cairo_curve_to(cr, x - width * 0.5, y - len * 0.5, tx - bend * 0.3, ty + len * 0.3, tx, ty);
  cairo_curve_to(cr, tx - bend * 0.3 + width * 0.3, ty + len * 0.3, x + width * 0.5, y - len * 0.5, x + width, y);
  cairo_close_path(cr);
  setColor(cr, c);
  cairo_fill(cr);
  if (!thorns)
    return;
  for (int k = 1; k <= 2; ++k)
  {
    const double u = 0.3 * k;
    const double px = x + bend * u * u, py = y - len * u;
    const double s = k % 2 ? 1.0 : -1.0;
    cairo_move_to(cr, px, py);
    cairo_line_to(cr, px + s * 6, py - 2);
    cairo_line_to(cr, px, py - 4);
    cairo_close_path(cr);
    setColor(cr, lighten(c, 0.45f));
    cairo_fill(cr);
  }
}

// A Bounder in silhouette, grazing: a round body on folded legs.
void grazer(cairo_t* cr, double x, double y, double s, Color c, bool flip)
{
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_scale(cr, flip ? -s : s, s);
  ellipse(cr, 0, -26, 30, 20);
  setColor(cr, c);
  cairo_fill(cr);
  circle(cr, 30, -18, 10);
  cairo_fill(cr);
  cairo_move_to(cr, -18, -18);
  cairo_line_to(cr, -38, -30);
  cairo_line_to(cr, -24, 0);
  cairo_line_to(cr, -16, 0);
  cairo_line_to(cr, -28, -26);
  cairo_close_path(cr);
  cairo_fill(cr);
  cairo_rectangle(cr, 12, -10, 5, 10);
  cairo_fill(cr);
  cairo_move_to(cr, 32, -26);
  cairo_curve_to(cr, 36, -40, 46, -44, 50, -40);
  cairo_set_line_width(cr, 2.0);
  cairo_set_source_rgba(cr, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, alphaOf(c) / 255.0);
  cairo_stroke(cr);
  cairo_restore(cr);
}

} // namespace

bool isPlains(const Theme& t) { return std::string_view(t.look) == "alien_plains"; }

// Plum soil: layered, stones and roots in it. Variant 1 a buried stone,
// 2 an old bone.
Texture bakePlainsSolid(const Renderer& r, const Theme& t, int variant)
{
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  Rng rng(std::uint32_t(variant * 7919 + 48));
  gradient(cr, 64, 64, {{0.0, t.rock}, {1.0, darken(t.rock, 0.2f)}});
  for (int k = 0; k < 3; ++k)
  {
    const double y = 10 + k * 20 + rng.range(-3, 3);
    cairo_move_to(cr, 0, y);
    for (int x = 4; x <= 64; x += 4)
      cairo_line_to(cr, x, y + 1.5 * std::sin(double(x) / 64.0 * 2 * kPi + k));
    cairo_set_line_width(cr, rng.range(2, 4));
    setColor(cr, withAlpha(darken(t.rock, 0.3f), 110));
    cairo_stroke(cr);
  }
  for (int i = 0; i < 6; ++i)
  {
    ellipse(cr, rng.range(6, 58), rng.range(6, 58), rng.range(2.0, 4.5), rng.range(1.5, 3.0));
    fillOutline(cr, withAlpha(t.rockLight, 200), withAlpha(t.rockDark, 160), 1.0);
  }
  // A root wandering through.
  const double rx = rng.range(10, 54);
  cairo_move_to(cr, rx, 0);
  cairo_curve_to(cr, rx + 8, 20, rx - 8, 40, rx + rng.range(-6, 6), 64);
  cairo_set_line_width(cr, 1.6);
  setColor(cr, withAlpha(rgb(60, 30, 50), 140));
  cairo_stroke(cr);
  if (variant == 1)
  {
    // A buried stone.
    ellipse(cr, rng.range(20, 44), rng.range(24, 40), 9, 6);
    fillOutline(cr, withAlpha(lighten(t.rockLight, 0.1f), 220), withAlpha(t.rockDark, 180), 1.2);
  }
  else if (variant == 2)
  {
    // An old bone, half sunk.
    cairo_move_to(cr, 22, 40);
    cairo_line_to(cr, 40, 33);
    cairo_set_line_width(cr, 3.5);
    setColor(cr, rgba(230, 216, 200, 120));
    cairo_stroke(cr);
    for (const double bx : {22.0, 40.0})
    {
      circle(cr, bx, bx < 30 ? 40 : 33, 3);
      setColor(cr, rgba(230, 216, 200, 130));
      cairo_fill(cr);
    }
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// The top of the ground: a mat of violet grass, blades standing up.
Texture bakePlainsSolidTop(const Renderer& r, const Theme& t, float topOffset, int height)
{
  VectorImage img(64, height);
  cairo_t* cr = img.cr();
  Rng rng(4848u);
  const double y = topOffset;
  cairo_rectangle(cr, 0, y, 64, 10);
  fillGradientOutline(cr, y, y + 10, rgb(170, 110, 200), rgb(110, 60, 140), darken(t.rock, 0.3f), 1.0);
  for (int i = 0; i < 9; ++i)
  {
    const double x = 3 + i * 7 + rng.range(-2, 2);
    blade(cr, x, y + 4, rng.range(7, 15), rng.range(-4, 4), 2.2,
      i % 2 ? rgb(190, 130, 230) : rgb(150, 90, 200), false);
  }
  return img.toTexture(r, 0.0f, topOffset);
}

// One-way: a log of pale alien wood lashed across, bark curling.
Texture bakePlainsPlatform(const Renderer& r, const Theme& t)
{
  VectorImage img(64, 40);
  cairo_t* cr = img.cr();
  roundedRect(cr, 0, 2, 64, 12, 5);
  fillGradientOutline(cr, 2, 14, lighten(t.platform, 0.3f), t.platformDark, kInk, 1.4);
  for (int x = 8; x < 64; x += 14)
  {
    cairo_move_to(cr, x, 5);
    cairo_curve_to(cr, x + 4, 7, x + 2, 10, x + 7, 11);
    cairo_set_line_width(cr, 1.2);
    setColor(cr, withAlpha(darken(t.platformDark, 0.3f), 200));
    cairo_stroke(cr);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// The sky: an indigo evening fading to peach at the horizon, a big ringed
// moon and a small one, the first stars.
Texture bakePlainsSky(const Renderer& r, const Theme& t)
{
  VectorImage img(kScreenW, kScreenH);
  cairo_t* cr = img.cr();
  gradient(cr, kScreenW, kScreenH, {{0.0, t.skyTop}, {0.55, t.skyMid}, {1.0, t.skyBottom}});
  Rng rng(4800u);
  for (int i = 0; i < 90; ++i)
  {
    const double y = rng.range(0, kScreenH * 0.5);
    circle(cr, rng.range(0, kScreenW), y, rng.range(0.6, 1.6));
    setColor(cr, rgba(255, 250, 255, int(200 * (1.0 - y / (kScreenH * 0.5)))));
    cairo_fill(cr);
  }
  // The big moon: banded, a tilted ring.
  const double mx = 1300, my = 210, mr = 120;
  radialGlow(cr, mx, my, mr * 1.8, rgb(255, 210, 240), 0.35);
  circle(cr, mx, my, mr);
  cairo_pattern_t* p = cairo_pattern_create_linear(mx - mr, my - mr, mx + mr, my + mr);
  cairo_pattern_add_color_stop_rgb(p, 0, 1.0, 0.92, 0.85);
  cairo_pattern_add_color_stop_rgb(p, 1, 0.75, 0.55, 0.75);
  cairo_set_source(cr, p);
  cairo_fill(cr);
  cairo_pattern_destroy(p);
  for (int k = 0; k < 4; ++k)
  {
    ellipse(cr, mx, my - mr * 0.5 + k * mr * 0.32, mr * 0.95, 6);
    setColor(cr, rgba(200, 140, 190, 70));
    cairo_fill(cr);
  }
  cairo_save(cr);
  cairo_translate(cr, mx, my);
  cairo_rotate(cr, -0.3);
  cairo_scale(cr, mr * 1.8, mr * 0.32);
  cairo_arc(cr, 0, 0, 1, 0.1, kPi - 0.1);
  cairo_restore(cr);
  cairo_set_line_width(cr, 9.0);
  setColor(cr, rgba(255, 236, 220, 170));
  cairo_stroke(cr);
  // The small moon.
  circle(cr, 420, 140, 34);
  setColor(cr, rgb(220, 240, 255));
  cairo_fill(cr);
  circle(cr, 434, 132, 30);
  setColor(cr, withAlpha(t.skyTop, 140));
  cairo_fill(cr);
  return img.toTexture(r, 0.0f, 0.0f);
}

// Far: rolling hills in the haze, rock arches, Bounders grazing.
Texture bakePlainsFar(const Renderer& r, const Theme& t, int layerW)
{
  VectorImage img(layerW, kScreenH);
  cairo_t* cr = img.cr();
  Rng rng(4811u);
  const double base = kScreenH * 0.68;
  cairo_move_to(cr, 0, kScreenH);
  for (double x = 0; x <= layerW; x += 20)
    cairo_line_to(cr, x, base - 40 * std::sin(x / layerW * 2 * kPi * 3) - 20 * std::sin(x / layerW * 2 * kPi * 7 + 1));
  cairo_line_to(cr, layerW, kScreenH);
  cairo_close_path(cr);
  setColor(cr, withAlpha(t.farLayer, 200));
  cairo_fill(cr);
  for (double x = 200; x < layerW - 200; x += rng.range(600, 900))
  {
    // An arch: two legs and a span, eroded.
    const double w = rng.range(160, 260), hh = rng.range(150, 230), y = base - 10;
    cairo_move_to(cr, x, y);
    cairo_line_to(cr, x + 10, y - hh);
    cairo_curve_to(cr, x + w * 0.3, y - hh - 30, x + w * 0.7, y - hh - 30, x + w - 10, y - hh);
    cairo_line_to(cr, x + w, y);
    cairo_line_to(cr, x + w - 36, y);
    cairo_curve_to(cr, x + w - 40, y - hh + 60, x + 40, y - hh + 60, x + 36, y);
    cairo_close_path(cr);
    setColor(cr, withAlpha(darken(t.farLayer, 0.2f), 220));
    cairo_fill(cr);
  }
  for (double x = 120; x < layerW; x += rng.range(240, 520))
    grazer(cr, x, base + rng.range(10, 40), rng.range(0.5, 0.8), withAlpha(darken(t.farLayer, 0.35f), 230),
      rng.uniform() < 0.5);
  return img.toTexture(r, 0.0f, 0.0f);
}

// Near: tall thorn grass in clumps along the bottom of the view.
Texture bakePlainsNear(const Renderer& r, const Theme& t, int layerW)
{
  VectorImage img(layerW, kScreenH);
  cairo_t* cr = img.cr();
  Rng rng(4822u);
  for (double x = 40; x < layerW; x += rng.range(90, 220))
  {
    const int blades = 5 + int(rng.uniform() * 5);
    for (int k = 0; k < blades; ++k)
      blade(cr, x + rng.range(-30, 30), kScreenH + 10, rng.range(90, 220), rng.range(-50, 50), 5.0,
        withAlpha(k % 2 ? t.nearLayer : lighten(t.nearLayer, 0.15f), 210), true);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// The spikes: a clump of thorn grass, stiff blades with hooked thorns.
Texture bakePlainsSpikes(const Renderer& r, const Theme& t)
{
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  Rng rng(4833u);
  for (int k = 0; k < 7; ++k)
  {
    const double x = 4 + k * 9.3;
    blade(cr, x, 63, rng.range(28, 40), rng.range(-6, 6), 3.6, k % 2 ? t.hazard : darken(t.hazard, 0.2f), true);
  }
  for (int k = 0; k < 6; ++k)
  {
    const double x = 9 + k * 9.3, y = rng.range(30, 44);
    cairo_move_to(cr, x, y);
    cairo_line_to(cr, x + 4, y - 8);
    cairo_line_to(cr, x + 7, y + 1);
    cairo_close_path(cr);
    fillOutline(cr, t.hazardLight, kInk, 1.0);
  }
  roundedRect(cr, 0, 58, 64, 6, 2);
  fillOutline(cr, darken(t.hazard, 0.5f), kInk, 1.0);
  return img.toTexture(r, 0.0f, 0.0f);
}

} // namespace gr
