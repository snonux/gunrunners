// Level 43, Starfall (Episode 7, DEEP SPACE): open space over the planet
// Vurr. Asteroid rock for the tiles, a starfield with a nebula, the ringed
// giant and Vurr's curve below, and far-off rocks drifting past.

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
constexpr Color kInk = rgb(14, 10, 20);

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

// A crater: a dark bowl with a lit lower rim (the light comes from the
// upper left).
void crater(cairo_t* cr, double x, double y, double r, Color base)
{
  circle(cr, x, y, r);
  setColor(cr, withAlpha(darken(base, 0.45f), 210));
  cairo_fill(cr);
  cairo_arc(cr, x, y, r, kPi * 0.05, kPi * 0.95);
  cairo_set_line_width(cr, std::max(1.2, r * 0.3));
  setColor(cr, withAlpha(lighten(base, 0.35f), 170));
  cairo_stroke(cr);
}

// A ringed gas giant, its night side to the lower right.
void giant(cairo_t* cr, double gx, double gy, double gr, Color body, Color band, Color ring)
{
  radialGlow(cr, gx, gy, gr * 1.5, body, 0.16);
  cairo_save(cr);
  circle(cr, gx, gy, gr);
  cairo_clip(cr);
  cairo_pattern_t* p = cairo_pattern_create_linear(gx, gy - gr, gx, gy + gr);
  cairo_pattern_add_color_stop_rgb(p, 0, redOf(lighten(body, 0.25f)) / 255.0, greenOf(lighten(body, 0.25f)) / 255.0,
    blueOf(lighten(body, 0.25f)) / 255.0);
  cairo_pattern_add_color_stop_rgb(p, 1, redOf(darken(body, 0.3f)) / 255.0, greenOf(darken(body, 0.3f)) / 255.0,
    blueOf(darken(body, 0.3f)) / 255.0);
  cairo_rectangle(cr, gx - gr, gy - gr, gr * 2, gr * 2);
  cairo_set_source(cr, p);
  cairo_fill(cr);
  cairo_pattern_destroy(p);
  Rng rng(777u);
  for (int b = 0; b < 8; ++b)
  {
    const double y = gy - gr + b * gr / 4 + rng.range(-6, 6);
    cairo_move_to(cr, gx - gr, y);
    cairo_curve_to(cr, gx - gr * 0.3, y + 8, gx + gr * 0.3, y - 8, gx + gr, y + 4);
    cairo_set_line_width(cr, rng.range(gr * 0.05, gr * 0.12));
    setColor(cr, withAlpha(band, 110));
    cairo_stroke(cr);
  }
  circle(cr, gx + gr * 0.5, gy + gr * 0.4, gr * 1.08);
  setColor(cr, rgba(6, 4, 16, 170));
  cairo_fill(cr);
  cairo_restore(cr);
  for (int k = 0; k < 2; ++k)
  {
    cairo_save(cr);
    cairo_translate(cr, gx, gy);
    cairo_rotate(cr, -0.32);
    cairo_scale(cr, gr * (2.0 + k * 0.25), gr * (0.36 + k * 0.05));
    circle(cr, 0, 0, 1);
    cairo_restore(cr);
    cairo_set_line_width(cr, k ? 3 : 8);
    setColor(cr, withAlpha(ring, k ? 110 : 160));
    cairo_stroke(cr);
  }
}

} // namespace

bool isStarfall(const Theme& t) { return std::string_view(t.look) == "alien_space"; }

// Asteroid rock: grey-brown stone with craters and pebbles, lit from the
// upper left. Variant 1 has a vein of violet crystal, 2 a big crater.
Texture bakeStarfallSolid(const Renderer& r, const Theme& t, int variant)
{
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  Rng rng(std::uint32_t(variant * 6151 + 43));
  gradient(cr, 64, 64, {{0.0, t.rock}, {1.0, darken(t.rock, 0.18f)}});
  // Lumps of stone, overlapping, that wrap at the edges.
  for (int i = 0; i < 9; ++i)
  {
    const double x = rng.range(0, 64), y = rng.range(0, 64), rx = rng.range(8, 16), ry = rng.range(6, 12);
    for (int wx = -1; wx <= 1; ++wx)
      for (int wy = -1; wy <= 1; ++wy)
      {
        ellipse(cr, x + wx * 64, y + wy * 64, rx, ry);
        setColor(cr, withAlpha(lerpColor(t.rock, t.rockLight, float(rng.range(0.0, 0.4))), 120));
        cairo_fill(cr);
      }
  }
  for (int i = 0; i < 3; ++i)
    crater(cr, rng.range(10, 54), rng.range(10, 54), rng.range(3, 6), t.rock);
  for (int i = 0; i < 10; ++i)
  {
    circle(cr, rng.range(2, 62), rng.range(2, 62), rng.range(0.8, 1.8));
    setColor(cr, withAlpha(lighten(t.rockLight, 0.2f), 140));
    cairo_fill(cr);
  }
  if (variant == 1)
  {
    double x = 0, y = rng.range(20, 44);
    cairo_move_to(cr, x, y);
    while (x < 64)
    {
      x += rng.range(8, 14);
      y = std::clamp(y + rng.range(-10, 10), 8.0, 56.0);
      cairo_line_to(cr, x, y);
    }
    cairo_set_line_width(cr, 5.0);
    setColor(cr, withAlpha(t.trim, 120));
    cairo_stroke_preserve(cr);
    cairo_set_line_width(cr, 2.0);
    setColor(cr, t.trimGlow);
    cairo_stroke(cr);
  }
  else if (variant == 2)
    crater(cr, 32 + rng.range(-6, 6), 32 + rng.range(-6, 6), 12, t.rock);
  return img.toTexture(r, 0.0f, 0.0f);
}

// The top of a rock: a rough, sunlit rim with a little space dust on it.
Texture bakeStarfallSolidTop(const Renderer& r, const Theme& t, float topOffset, int height)
{
  VectorImage img(64, height);
  cairo_t* cr = img.cr();
  Rng rng(4343u);
  const double y = topOffset;
  cairo_move_to(cr, 0, y + 3);
  for (int x = 0; x <= 64; x += 8)
    cairo_line_to(cr, x, y + (x == 0 || x == 64 ? 3 : rng.range(0, 5)));
  cairo_line_to(cr, 64, y + 12);
  cairo_line_to(cr, 0, y + 12);
  cairo_close_path(cr);
  fillGradientOutline(cr, y, y + 12, lighten(t.rockLight, 0.3f), t.rock, darken(t.rockDark, 0.3f), 1.2);
  for (int i = 0; i < 5; ++i)
  {
    circle(cr, rng.range(4, 60), y + rng.range(3, 8), rng.range(1, 2));
    setColor(cr, rgba(255, 240, 220, 120));
    cairo_fill(cr);
  }
  return img.toTexture(r, 0.0f, topOffset);
}

// One-way: a strut of the old beacon girders, riveted.
Texture bakeStarfallPlatform(const Renderer& r, const Theme& t)
{
  VectorImage img(64, 40);
  cairo_t* cr = img.cr();
  cairo_rectangle(cr, 0, 1, 64, 11);
  fillGradientOutline(cr, 1, 12, lighten(t.platform, 0.3f), t.platformDark, kInk, 1.6);
  for (int x = 6; x < 64; x += 14)
  {
    circle(cr, x, 6.5, 1.8);
    setColor(cr, lighten(t.platform, 0.5f));
    cairo_fill(cr);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// Deep space: black going indigo, thousands of stars, a nebula.
Texture bakeStarfallSky(const Renderer& r, const Theme& t)
{
  VectorImage img(kScreenW, kScreenH);
  cairo_t* cr = img.cr();
  gradient(cr, kScreenW, kScreenH, {{0.0, t.skyTop}, {0.65, t.skyMid}, {1.0, t.skyBottom}});
  Rng rng(4300u);
  // The nebula: overlapping glows in the accent colours across the sky.
  for (int i = 0; i < 14; ++i)
    radialGlow(cr, 120 + i * 80 + rng.range(-40, 40), 200 + 90 * std::sin(i * 0.7) + rng.range(-40, 40),
      rng.range(90, 190), i % 3 == 0 ? t.accentA : (i % 3 == 1 ? t.accentB : t.trim), 0.07);
  for (int i = 0; i < 520; ++i)
  {
    const double x = rng.range(0, kScreenW), y = rng.range(0, kScreenH);
    const double s = rng.uniform() < 0.95 ? rng.range(0.4, 1.3) : rng.range(1.5, 2.4);
    circle(cr, x, y, s);
    setColor(cr, rgba(255, 250, 240, int(60 + 190 * rng.uniform())));
    cairo_fill(cr);
    if (s > 1.6)
      radialGlow(cr, x, y, s * 5, rgb(200, 220, 255), 0.4);
  }
  // A distant ringed giant, high on the left.
  giant(cr, 240, 170, 92, rgb(230, 170, 110), rgb(150, 90, 60), rgb(240, 220, 190));
  return img.toTexture(r, 0.0f, 0.0f);
}

// Far: Vurr below, a violet curve with cloud bands, and its two moons.
Texture bakeStarfallFar(const Renderer& r, const Theme& t, int layerW)
{
  VectorImage img(layerW, kScreenH);
  cairo_t* cr = img.cr();
  Rng rng(4401u);
  // The planet's limb: a huge circle whose top shows along the bottom.
  const double R = layerW * 1.4, cx = layerW * 0.5, cy = kScreenH + R - 170;
  radialGlow(cr, cx, kScreenH - 150, layerW * 0.6, t.accentB, 0.14);
  cairo_save(cr);
  circle(cr, cx, cy, R);
  cairo_clip(cr);
  gradient(cr, layerW, kScreenH, {{0.0, t.farLayer}, {0.75, t.farLayer}, {0.85, lighten(t.farLayer, 0.15f)}, {1.0, darken(t.farLayer, 0.35f)}});
  for (int b = 0; b < 9; ++b)
  {
    const double y = kScreenH - 160 + b * 20 + rng.range(-4, 4);
    for (double x = 0; x < layerW; x += rng.range(120, 260))
    {
      ellipse(cr, x, y, rng.range(80, 200), rng.range(4, 9));
      setColor(cr, withAlpha(lighten(t.farLayer, float(rng.range(0.1, 0.3))), 120));
      cairo_fill(cr);
    }
  }
  cairo_restore(cr);
  // The atmosphere's glow along the rim.
  circle(cr, cx, cy, R + 4);
  cairo_set_line_width(cr, 10);
  setColor(cr, withAlpha(lighten(t.accentB, 0.3f), 90));
  cairo_stroke(cr);
  // The two moons.
  const double moons[2][3] = {{layerW * 0.3, 260, 34}, {layerW * 0.72, 150, 16}};
  for (const auto& m : moons)
  {
    radialGlow(cr, m[0], m[1], m[2] * 2.2, rgb(220, 255, 240), 0.18);
    circle(cr, m[0], m[1], m[2]);
    setColor(cr, rgb(214, 232, 226));
    cairo_fill(cr);
    for (int c = 0; c < 4; ++c)
      crater(cr, m[0] + rng.range(-m[2] * 0.5, m[2] * 0.5), m[1] + rng.range(-m[2] * 0.5, m[2] * 0.5),
        m[2] * rng.range(0.1, 0.2), rgb(180, 200, 196));
    circle(cr, m[0] + m[2] * 0.45, m[1] + m[2] * 0.2, m[2] * 0.95);
    setColor(cr, rgba(10, 6, 30, 140));
    cairo_fill(cr);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// Near: far-off asteroids tumbling past, dim, and streaks of space dust.
Texture bakeStarfallNear(const Renderer& r, const Theme& t, int layerW)
{
  VectorImage img(layerW, kScreenH);
  cairo_t* cr = img.cr();
  Rng rng(4502u);
  for (double x = 40; x < layerW - 40; x += rng.range(160, 340))
  {
    const double y = rng.range(80, kScreenH - 240), s = rng.range(14, 40);
    cairo_move_to(cr, x + s, y);
    for (int k = 1; k < 9; ++k)
    {
      const double a = k * 2 * kPi / 9, rr = s * rng.range(0.7, 1.1);
      cairo_line_to(cr, x + std::cos(a) * rr, y + std::sin(a) * rr);
    }
    cairo_close_path(cr);
    setColor(cr, withAlpha(t.nearLayer, 230));
    cairo_fill_preserve(cr);
    setColor(cr, withAlpha(lighten(t.nearLayer, 0.3f), 160));
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);
    crater(cr, x - s * 0.2, y - s * 0.1, s * 0.25, t.nearLayer);
  }
  for (int i = 0; i < 40; ++i)
  {
    const double x = rng.range(0, layerW), y = rng.range(0, kScreenH), l = rng.range(10, 40);
    cairo_move_to(cr, x, y);
    cairo_line_to(cr, x + l, y);
    cairo_set_line_width(cr, 1.2);
    setColor(cr, rgba(200, 210, 255, 50));
    cairo_stroke(cr);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

} // namespace gr
