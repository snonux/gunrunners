// Level 31, Storm Relay (theme look "storm"): a sea cliff in a thunderstorm
// at night. The masses are wet black cliff rock, basalt split into angular
// slabs, each upper edge catching a cold sheen of rain, dark cracks between
// them with water seeping down; now and then a block carries a crust of
// barnacles and limpets, or a white quartz vein and a streak of wet green
// weed. Where you stand, a glossy lip of wet rock with puddles holding the
// sky and pebbles. The one-way floors are rusted steel grating, the ladders
// rusted iron. Behind: a night sky of storm cloud, its
// bellies lit faintly from below, over a dark sea with a pale strip at the
// horizon; far off, cloud banks trailing rain curtains; close by, low scud
// blowing past. The exit is the relay dish. The rain, the lightning, the
// waves and the surf are drawn live (world_storm_draw.cpp).

#include "assets/art_storm.hpp"

#include "base/math.hpp"
#include "render/vector.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <string_view>

namespace gr
{

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr Color kRockHi = rgb(66, 72, 84);
constexpr Color kRock = rgb(36, 40, 48);
constexpr Color kRockLo = rgb(14, 16, 22);
constexpr Color kSheen = rgb(170, 200, 235);
constexpr Color kRust = rgb(140, 70, 36);
constexpr Color kSteel = rgb(104, 104, 104);
constexpr Color kSteelDark = rgb(40, 38, 40);
constexpr Color kOutline = rgb(6, 8, 12);

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

void fillLinear(cairo_t* cr, double x0, double y0, double x1, double y1, std::initializer_list<std::pair<double, Color>> stops,
  double alpha = 1.0, bool preserve = false)
{
  cairo_pattern_t* g = cairo_pattern_create_linear(x0, y0, x1, y1);
  for (const auto& s : stops)
    stop(g, s.first, s.second, alpha);
  cairo_set_source(cr, g);
  if (preserve)
    cairo_fill_preserve(cr);
  else
    cairo_fill(cr);
  cairo_pattern_destroy(g);
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

void stroke(cairo_t* cr, Color c, double w, double a = 1.0)
{
  setRgba(cr, c, a);
  cairo_set_line_width(cr, w);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
  cairo_stroke(cr);
}

// A soft round light (or shade) of colour c.
void softDot(cairo_t* cr, double x, double y, double rx, double ry, Color c, double a)
{
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_scale(cr, std::max(0.1, rx), std::max(0.1, ry));
  cairo_pattern_t* g = cairo_pattern_create_radial(0, 0, 0, 0, 0, 1);
  stop(g, 0.0, c, a);
  stop(g, 0.6, c, a * 0.45);
  stop(g, 1.0, c, 0.0);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
  cairo_restore(cr);
}

// Draws `f(ox, oy)` at every offset a wrapped 64 px tile needs.
template <typename F>
void wrap64(F f)
{
  for (const double ox : {-64.0, 0.0, 64.0})
    for (const double oy : {-64.0, 0.0, 64.0})
      f(ox, oy);
}

// The rock under every variant: dark basalt slabs, angular, each lit a
// little differently, a cold rain sheen along their upper edges and dark
// cracks between them. Wrapped so the blocks tile without seams.
void rockBase(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 0, 64, 64);
  fillLinear(cr, 0, 0, 64, 64, {{0.0, kRock}, {1.0, kRockLo}});
  // Slabs: quads on a skewed grid (shared corners, so the cracks meet).
  Rng rng(31011u);
  constexpr int kN = 3;
  std::array<std::array<std::pair<double, double>, kN + 1>, kN + 1> p{};
  for (int j = 0; j <= kN; ++j)
    for (int i = 0; i <= kN; ++i)
    {
      const double jx = (i == 0 || i == kN) ? 0.0 : rng.range(-5, 5);
      const double jy = (j == 0 || j == kN) ? 0.0 : rng.range(-5, 5);
      p[std::size_t(j)][std::size_t(i)] = {i * 64.0 / kN + jx + (j % 2) * 6.0, j * 64.0 / kN + jy};
    }
  for (int j = 0; j < kN; ++j)
    for (int i = 0; i < kN; ++i)
    {
      const auto a = p[std::size_t(j)][std::size_t(i)], b = p[std::size_t(j)][std::size_t(i + 1)];
      const auto c = p[std::size_t(j + 1)][std::size_t(i + 1)], d = p[std::size_t(j + 1)][std::size_t(i)];
      const double shade = rng.range(0.0f, 1.0f);
      const Color face = lerpColor(kRockLo, kRockHi, float(0.25 + 0.5 * shade));
      wrap64([&](double ox, double oy) {
        cairo_move_to(cr, a.first + ox + 1.2, a.second + oy + 1.2);
        cairo_line_to(cr, b.first + ox - 1.2, b.second + oy + 1.2);
        cairo_line_to(cr, c.first + ox - 1.2, c.second + oy - 1.2);
        cairo_line_to(cr, d.first + ox + 1.2, d.second + oy - 1.2);
        cairo_close_path(cr);
        fillLinear(cr, 0, a.second + oy, 0, d.second + oy, {{0.0, lighten(face, 0.12f)}, {0.5, face}, {1.0, darken(face, 0.35f)}});
        // The wet sheen along the slab's upper edge.
        cairo_move_to(cr, a.first + ox + 3, a.second + oy + 2.2);
        cairo_line_to(cr, b.first + ox - 3, b.second + oy + 2.2);
        stroke(cr, kSheen, 1.3, 0.22 + 0.2 * shade);
        // The dark shadow along its lower edge.
        cairo_move_to(cr, d.first + ox + 2, d.second + oy - 1.6);
        cairo_line_to(cr, c.first + ox - 2, c.second + oy - 1.6);
        stroke(cr, kOutline, 1.6, 0.55);
      });
    }
  // Drops of rain standing on the rock, each with a glint.
  for (int i = 0; i < 9; ++i)
  {
    const double x = rng.range(2, 62), y = rng.range(2, 62), s = rng.range(0.8f, 1.7f);
    wrap64([&](double ox, double oy) {
      cairo_arc(cr, x + ox, y + oy, s, 0, 2 * kPi);
      setRgba(cr, rgb(120, 140, 170), 0.35);
      cairo_fill(cr);
      cairo_arc(cr, x + ox - s * 0.35, y + oy - s * 0.35, s * 0.4, 0, 2 * kPi);
      setRgba(cr, rgb(235, 245, 255), 0.6);
      cairo_fill(cr);
    });
  }
  // A fine grain.
  for (int i = 0; i < 70; ++i)
  {
    cairo_arc(cr, rng.range(0, 64), rng.range(0, 64), rng.range(0.4f, 0.9f), 0, 2 * kPi);
    setRgba(cr, i % 3 ? rgb(4, 6, 10) : rgb(130, 140, 160), rng.range(0.1f, 0.25f));
    cairo_fill(cr);
  }
}

// Barnacles and limpets (variant 1), kept off the edges.
void barnacles(cairo_t* cr)
{
  for (const auto& b : {std::array<double, 3>{18, 40, 4.0}, std::array<double, 3>{25, 44, 3.2}, std::array<double, 3>{21, 49, 3.6},
         std::array<double, 3>{29, 50, 2.6}, std::array<double, 3>{14, 47, 2.8}})
  {
    cairo_arc(cr, b[0], b[1], b[2], 0, 2 * kPi);
    fillLinear(cr, 0, b[1] - b[2], 0, b[1] + b[2], {{0.0, rgb(196, 198, 190)}, {1.0, rgb(96, 100, 100)}}, 1.0);
    cairo_arc(cr, b[0], b[1], b[2] * 0.42, 0, 2 * kPi);
    setRgba(cr, rgb(24, 26, 30), 1.0);
    cairo_fill(cr);
  }
  // Two limpets: low ribbed cones.
  for (const auto& l : {std::array<double, 3>{43, 22, 6.5}, std::array<double, 3>{50, 29, 5.0}})
  {
    ellipse(cr, l[0], l[1], l[2], l[2] * 0.62);
    fillLinear(cr, 0, l[1] - l[2], 0, l[1] + l[2], {{0.0, rgb(190, 170, 140)}, {1.0, rgb(90, 76, 60)}}, 1.0, true);
    stroke(cr, kOutline, 1.0, 0.7);
    for (int k = -2; k <= 2; ++k)
    {
      cairo_move_to(cr, l[0], l[1] - l[2] * 0.15);
      cairo_line_to(cr, l[0] + k * l[2] * 0.4, l[1] + l[2] * 0.55);
      stroke(cr, rgb(70, 56, 44), 0.7, 0.6);
    }
    cairo_arc(cr, l[0] - 1.5, l[1] - l[2] * 0.3, 1.2, 0, 2 * kPi);
    setRgba(cr, rgb(255, 250, 235), 0.6);
    cairo_fill(cr);
  }
}

// A white quartz vein and a seep of water with wet weed (variant 2).
void veinAndSeep(cairo_t* cr)
{
  cairo_move_to(cr, 8, 18);
  cairo_curve_to(cr, 20, 22, 30, 14, 42, 24);
  cairo_curve_to(cr, 48, 28, 52, 26, 57, 30);
  stroke(cr, rgb(200, 206, 214), 2.2, 0.7);
  cairo_move_to(cr, 8, 18);
  cairo_curve_to(cr, 20, 22, 30, 14, 42, 24);
  stroke(cr, rgb(255, 255, 255), 0.8, 0.5);
  // The seep: a dark wet streak down the rock with a glint running in it.
  cairo_move_to(cr, 30, 26);
  cairo_curve_to(cr, 28, 36, 33, 46, 31, 60);
  stroke(cr, rgb(6, 8, 12), 5.0, 0.5);
  cairo_move_to(cr, 30, 28);
  cairo_curve_to(cr, 28, 38, 33, 46, 31, 58);
  stroke(cr, kSheen, 1.0, 0.45);
  // Weed hanging at the seep's foot.
  for (int k = 0; k < 5; ++k)
  {
    const double x = 24 + k * 3.2;
    cairo_move_to(cr, x, 46);
    cairo_curve_to(cr, x - 1, 51, x + 2, 54, x + (k % 2 ? 1.5 : -1.5), 59);
    stroke(cr, k % 2 ? rgb(52, 92, 60) : rgb(36, 70, 48), 2.2, 0.9);
  }
}

} // namespace

bool isStormCliffs(const Theme& t) { return std::string_view(t.look) == "storm"; }

Texture bakeStormSolid(const Renderer& r, const Theme& t, int variant)
{
  (void)t;
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  rockBase(cr);
  if (variant == 1)
    barnacles(cr);
  else if (variant == 2)
    veinAndSeep(cr);
  return img.toTexture(r, 0.0f, 0.0f);
}

// Where you stand: a glossy lip of wet rock, its edge bright with the sky in
// it, puddles along it and pebbles. Kept low (2 px over the surface at
// most) so a sand, plank or metal overlay covers it.
Texture bakeStormSolidTop(const Renderer& r, const Theme& t, float topOffset, int height)
{
  (void)t;
  VectorImage img(64, height);
  cairo_t* cr = img.cr();
  const double y = topOffset;
  auto edge = [&](double x) { return y - 1.0 + 0.9 * std::sin(x / 64.0 * 2 * kPi) + 0.5 * std::sin(x / 32.0 * 2 * kPi + 0.7); };
  // The shade under the lip.
  cairo_rectangle(cr, 0, y + 6, 64, 14);
  {
    cairo_pattern_t* g = cairo_pattern_create_linear(0, y + 6, 0, y + 20);
    stop(g, 0.0, rgb(0, 0, 4), 0.45);
    stop(g, 1.0, rgb(0, 0, 4), 0.0);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  // The lip.
  cairo_move_to(cr, 0, edge(0));
  for (double x = 2; x <= 64; x += 2)
    cairo_line_to(cr, x, edge(x));
  for (double x = 64; x >= 0; x -= 4)
    cairo_line_to(cr, x, y + 7 + 1.2 * std::sin(x / 64.0 * 4 * kPi + 0.5));
  cairo_close_path(cr);
  fillLinear(cr, 0, y - 2, 0, y + 8, {{0.0, rgb(92, 100, 116)}, {0.4, rgb(52, 58, 70)}, {1.0, rgb(24, 26, 34)}});
  // Puddles holding the sky.
  for (const auto& pd : {std::array<double, 3>{14, 2.5, 9}, std::array<double, 3>{44, 3.0, 7}})
  {
    ellipse(cr, pd[0], y + pd[1], pd[2], 1.6);
    fillLinear(cr, pd[0] - pd[2], 0, pd[0] + pd[2], 0, {{0.0, rgb(60, 72, 96)}, {0.5, rgb(150, 170, 200)}, {1.0, rgb(60, 72, 96)}}, 0.75);
  }
  // The bright edge.
  cairo_move_to(cr, 0, edge(0));
  for (double x = 2; x <= 64; x += 2)
    cairo_line_to(cr, x, edge(x));
  stroke(cr, rgb(200, 220, 245), 1.4, 0.7);
  // Pebbles.
  Rng rng(31021u);
  for (int i = 0; i < 4; ++i)
  {
    const double x = rng.range(4, 60), py = y + rng.range(1, 4);
    ellipse(cr, x, py, rng.range(1.4f, 2.4f), rng.range(1.0f, 1.5f));
    setRgba(cr, i % 2 ? rgb(70, 76, 86) : rgb(120, 126, 136), 0.95);
    cairo_fill(cr);
    cairo_arc(cr, x - 0.6, py - 0.6, 0.6, 0, 2 * kPi);
    setRgba(cr, rgb(240, 248, 255), 0.6);
    cairo_fill(cr);
  }
  return img.toTexture(r, 0.0f, topOffset);
}

// One-way floors: rusted steel grating, a flat bar frame round square
// holes, rust bleeding down from the joints, rain beading on the top. 64 x
// 40, the walking surface at the top.
Texture bakeStormPlatform(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(64, 40);
  cairo_t* cr = img.cr();
  // The side rail (a channel) and the grating seen edge-on above it.
  cairo_rectangle(cr, 0, 2, 64, 14);
  fillLinear(cr, 0, 2, 0, 16, {{0.0, rgb(150, 138, 126)}, {0.35, kSteel}, {1.0, kSteelDark}});
  // The bars, seen from the side: slots of dark between them.
  for (double x = 3; x < 64; x += 8)
  {
    cairo_rectangle(cr, x, 5, 4.5, 7);
    setRgba(cr, rgb(10, 10, 14), 0.85);
    cairo_fill(cr);
  }
  // Rust: blotches and runs from the joints.
  Rng rng(31031u);
  for (int i = 0; i < 6; ++i)
  {
    const double x = rng.range(2, 62), y0 = rng.range(3, 12);
    softDot(cr, x, y0, rng.range(3, 6), rng.range(2, 4), kRust, 0.55);
    cairo_move_to(cr, x, y0);
    cairo_line_to(cr, x + rng.range(-1, 1), y0 + rng.range(6, 14));
    stroke(cr, rgb(120, 56, 28), 1.0, 0.5);
  }
  // The top edge, wet.
  cairo_rectangle(cr, 0, 0, 64, 3);
  fillLinear(cr, 0, 0, 0, 3, {{0.0, rgb(200, 206, 214)}, {1.0, rgb(110, 104, 100)}});
  for (int i = 0; i < 5; ++i)
  {
    cairo_arc(cr, 6 + i * 13 + rng.range(-2, 2), 1.5, 1.0, 0, 2 * kPi);
    setRgba(cr, rgb(250, 252, 255), 0.75);
    cairo_fill(cr);
  }
  // A rivet line and the outline.
  for (double x = 4; x < 64; x += 16)
  {
    cairo_arc(cr, x, 14, 1.2, 0, 2 * kPi);
    setRgba(cr, rgb(180, 170, 160), 0.8);
    cairo_fill(cr);
  }
  cairo_rectangle(cr, 0, 0, 64, 16);
  stroke(cr, kOutline, 1.2, 0.8);
  // A bracket under each end.
  for (const double x : {6.0, 50.0})
  {
    cairo_move_to(cr, x, 16);
    cairo_line_to(cr, x + 8, 16);
    cairo_line_to(cr, x + 4, 26);
    cairo_close_path(cr);
    fillLinear(cr, 0, 16, 0, 26, {{0.0, kSteel}, {1.0, kSteelDark}}, 1.0);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// Rusted iron rungs between two flat rails, wet and streaked.
Texture bakeStormLadder(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  for (const double y : {12.0, 44.0})
  {
    cairo_rectangle(cr, 12, y - 2.5, 40, 5);
    fillLinear(cr, 0, y - 2.5, 0, y + 2.5, {{0.0, rgb(170, 150, 130)}, {0.5, rgb(110, 70, 46)}, {1.0, rgb(50, 30, 22)}}, 1.0);
    cairo_rectangle(cr, 12, y - 2.5, 40, 5);
    stroke(cr, kOutline, 1.2, 0.9);
    cairo_move_to(cr, 16, y - 1.6);
    cairo_line_to(cr, 48, y - 1.6);
    stroke(cr, rgb(220, 230, 245), 0.8, 0.5);
  }
  for (const double x : {10.0, 48.0})
  {
    cairo_rectangle(cr, x, -2, 6, 68);
    fillLinear(cr, x, 0, x + 6, 0, {{0.0, rgb(150, 140, 132)}, {0.5, rgb(90, 80, 76)}, {1.0, rgb(36, 30, 30)}}, 1.0);
    cairo_rectangle(cr, x, -2, 6, 68);
    stroke(cr, kOutline, 1.4, 0.9);
    // Rust running down it.
    for (int k = 0; k < 3; ++k)
    {
      cairo_move_to(cr, x + 2 + k, 8 + k * 20);
      cairo_line_to(cr, x + 2 + k, 18 + k * 20);
      stroke(cr, kRust, 1.4, 0.6);
    }
    cairo_arc(cr, x + 3, 12, 1.2, 0, 2 * kPi);
    cairo_arc(cr, x + 3, 44, 1.2, 0, 2 * kPi);
    setRgba(cr, rgb(200, 190, 180), 0.8);
    cairo_fill(cr);
  }
  return img.toTexture(r, 16.0f, 0.0f);
}

// The night sky: storm cloud from the top down to a paler strip at the
// horizon, the clouds' bellies faintly lit; below it the dark sea, long
// swells, a shimmer under the bright strip, and whitecaps.
Texture bakeStormSky(const Renderer& r, const Theme& t)
{
  VectorImage img(kScreenW, kScreenH);
  cairo_t* cr = img.cr();
  const double hz = kStormHorizonY;
  cairo_rectangle(cr, 0, 0, kScreenW, hz + 1);
  fillLinear(cr, 0, 0, 0, hz, {{0.0, t.skyTop}, {0.55, t.skyMid}, {0.92, t.skyBottom}, {1.0, lighten(t.skyBottom, 0.25f)}});
  Rng rng(31041u);
  // A hidden moon's glow behind the cloud, high on the left.
  softDot(cr, 300, 90, 360, 220, rgb(120, 140, 190), 0.18);
  // Cloud masses: rows of soft billows, darker at the top, their bellies
  // lit from below by the distant strip of light.
  for (int row = 0; row < 5; ++row)
  {
    const double base = 50.0 + row * 70.0;
    for (int i = 0; i < 22; ++i)
    {
      const double x = rng.range(-120, float(kScreenW) + 120), rx = rng.range(90, 220), ry = rng.range(30, 70);
      const Color body = lerpColor(rgb(8, 10, 18), rgb(40, 46, 64), float(row) / 5.0f);
      softDot(cr, x, base + rng.range(-20, 20), rx, ry, body, 0.75);
      softDot(cr, x + rng.range(-20, 20), base + ry * 0.55, rx * 0.8, ry * 0.35, rgb(110, 124, 156), 0.08 + 0.03 * row);
    }
  }
  // A clear strip along the horizon, rain curtains hanging over it.
  softDot(cr, 640, hz - 6, 900, 40, rgb(150, 164, 190), 0.35);
  for (int i = 0; i < 6; ++i)
  {
    const double x = rng.range(0, float(kScreenW)), w = rng.range(60, 160);
    cairo_move_to(cr, x, 230);
    cairo_line_to(cr, x + w, 230);
    cairo_line_to(cr, x + w + 40, hz);
    cairo_line_to(cr, x + 30, hz);
    cairo_close_path(cr);
    fillLinear(cr, 0, 230, 0, hz, {{0.0, rgb(20, 24, 36)}, {1.0, rgb(30, 36, 52)}}, 0.35);
  }
  // Far headlands on either side, low and blue-black.
  for (const double sx : {0.0, 1.0})
  {
    const double x0 = sx == 0.0 ? -40.0 : 1020.0, x1 = sx == 0.0 ? 260.0 : 1320.0;
    cairo_move_to(cr, x0, hz + 1);
    cairo_curve_to(cr, x0 + 40, hz - 26, x0 + 90, hz - 34, (x0 + x1) * 0.5, hz - 30);
    cairo_curve_to(cr, x1 - 80, hz - 26, x1 - 40, hz - 8, x1, hz + 1);
    cairo_close_path(cr);
    setRgba(cr, rgb(16, 20, 30), 0.95);
    cairo_fill(cr);
  }
  // The sea: dark, a little lighter toward the horizon.
  cairo_rectangle(cr, 0, hz, kScreenW, kScreenH - hz);
  fillLinear(cr, 0, hz, 0, kScreenH, {{0.0, rgb(44, 54, 72)}, {0.08, rgb(22, 30, 44)}, {1.0, rgb(6, 9, 16)}});
  // The shimmer under the bright strip.
  softDot(cr, 640, hz + 14, 700, 22, rgb(150, 170, 200), 0.18);
  // Long swells: faint lines, closer together near the horizon.
  for (int i = 0; i < 26; ++i)
  {
    const double u = double(i) / 26.0, y = hz + 4 + (kScreenH - hz) * u * u;
    cairo_move_to(cr, 0, y);
    for (double x = 0; x <= kScreenW; x += 40)
      cairo_line_to(cr, x, y + std::sin(x * 0.006 + i * 1.7) * (2 + 6 * u));
    stroke(cr, rgb(120, 140, 170), 1.0 + 1.5 * u, 0.08 + 0.05 * (1 - u));
  }
  // Whitecaps, small and many near the horizon.
  for (int i = 0; i < 120; ++i)
  {
    const double u = rng.uniform(), y = hz + 3 + (kScreenH - hz) * u * u, x = rng.range(0, float(kScreenW));
    const double w = 3 + 22 * u * u;
    cairo_move_to(cr, x, y);
    cairo_curve_to(cr, x + w * 0.3, y - 1.5 - u * 2, x + w * 0.7, y - 1.5 - u * 2, x + w, y);
    stroke(cr, rgb(200, 214, 230), 0.8 + 1.4 * u, 0.18 + 0.15 * u);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// Far: banks of storm cloud trailing rain curtains, their edges lit cold.
// Kept to the top, so they float over the sea as the camera climbs.
Texture bakeStormFar(const Renderer& r, const Theme& t, int layerW)
{
  VectorImage img(layerW, kScreenH);
  cairo_t* cr = img.cr();
  const double W = layerW;
  Rng rng(31051u);
  const Color body = lerpColor(t.farLayer, rgb(8, 10, 18), 0.4f);
  for (double x = 0; x < W; x += rng.range(260, 420))
  {
    const double w = rng.range(260, 460), y = rng.range(60, 190);
    for (const double ox : {-W, 0.0, W})
    {
      const double cx = x + ox;
      if (cx + w < -40 || cx - w > W + 40)
        continue;
      Rng k(std::uint32_t(31061 + int(x)));
      // Rain curtain under it.
      {
        cairo_pattern_t* g = cairo_pattern_create_linear(0, y + 20, 0, y + 260);
        stop(g, 0.0, rgb(40, 48, 66), 0.32);
        stop(g, 1.0, rgb(40, 48, 66), 0.0);
        cairo_move_to(cr, cx - w * 0.4, y + 20);
        cairo_line_to(cr, cx + w * 0.3, y + 20);
        cairo_line_to(cr, cx + w * 0.3 + 60, y + 260);
        cairo_line_to(cr, cx - w * 0.4 + 60, y + 260);
        cairo_close_path(cr);
        cairo_set_source(cr, g);
        cairo_fill(cr);
        cairo_pattern_destroy(g);
      }
      // The bank: billows.
      for (int b = 0; b < 9; ++b)
      {
        const double bx = cx + k.range(-float(w) * 0.5f, float(w) * 0.5f), by = y + k.range(-30, 20);
        const double rx = k.range(60, 130), ry = k.range(26, 50);
        softDot(cr, bx, by, rx, ry, body, 0.85);
        softDot(cr, bx - rx * 0.2, by - ry * 0.45, rx * 0.6, ry * 0.3, rgb(120, 136, 170), 0.12);
      }
    }
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// Near: low scud blowing past, thin and ragged.
Texture bakeStormNear(const Renderer& r, const Theme& t, int layerW)
{
  VectorImage img(layerW, kScreenH);
  cairo_t* cr = img.cr();
  const double W = layerW;
  Rng rng(31071u);
  const Color c = lerpColor(t.nearLayer, rgb(60, 66, 84), 0.4f);
  for (double x = 0; x < W; x += rng.range(180, 340))
  {
    const double y = rng.range(120, 360), w = rng.range(160, 320);
    for (const double ox : {-W, 0.0, W})
    {
      const double cx = x + ox;
      if (cx + w < -40 || cx - w > W + 40)
        continue;
      Rng k(std::uint32_t(31081 + int(x)));
      for (int b = 0; b < 7; ++b)
        softDot(cr, cx + k.range(-float(w) * 0.5f, float(w) * 0.5f), y + k.range(-12, 12), k.range(50, 110), k.range(10, 20), c,
          0.28);
    }
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

Texture bakeStormExitBase(const Renderer& r, const Theme& t)
{
  (void)t;
  // 96 wide, 144 tall; (0, 0) is the top-left of the 64 x 128 exit area.
  VectorImage img(96, 144);
  cairo_t* cr = img.cr();
  cairo_translate(cr, 16, 8);
  // The tripod.
  for (const auto& leg : {std::array<double, 2>{-10, 32}, std::array<double, 2>{74, 32}, std::array<double, 2>{30, 34}})
  {
    cairo_move_to(cr, leg[1], 74);
    cairo_line_to(cr, leg[0], 130);
    stroke(cr, kOutline, 7);
    cairo_move_to(cr, leg[1], 74);
    cairo_line_to(cr, leg[0], 130);
    stroke(cr, rgb(150, 156, 166), 4);
  }
  // Its feet and a cross brace.
  for (const double x : {-10.0, 74.0, 30.0})
  {
    cairo_rectangle(cr, x - 6, 127, 12, 5);
    setRgba(cr, rgb(60, 62, 70), 1.0);
    cairo_fill(cr);
  }
  cairo_move_to(cr, 4, 108);
  cairo_line_to(cr, 60, 108);
  stroke(cr, rgb(110, 116, 126), 3);
  // The mast head and the cable down to a junction box.
  cairo_rectangle(cr, 24, 60, 16, 18);
  fillLinear(cr, 24, 0, 40, 0, {{0.0, rgb(190, 196, 206)}, {1.0, rgb(70, 74, 84)}}, 1.0, true);
  stroke(cr, kOutline, 1.6);
  cairo_rectangle(cr, 42, 92, 18, 14);
  fillLinear(cr, 0, 92, 0, 106, {{0.0, rgb(200, 190, 120)}, {1.0, rgb(120, 104, 50)}}, 1.0, true);
  stroke(cr, kOutline, 1.4);
  cairo_move_to(cr, 36, 78);
  cairo_curve_to(cr, 40, 92, 46, 88, 50, 92);
  stroke(cr, rgb(20, 20, 24), 2.4);
  // The dish, tipped up toward the sky on the left: a shallow bowl, its
  // rim bright, ribs on its back.
  cairo_save(cr);
  cairo_translate(cr, 32, 44);
  cairo_rotate(cr, -0.42);
  cairo_move_to(cr, -42, -6);
  cairo_curve_to(cr, -30, 26, 30, 26, 42, -6);
  cairo_close_path(cr);
  fillLinear(cr, 0, -8, 0, 22, {{0.0, rgb(236, 240, 246)}, {0.5, rgb(170, 178, 192)}, {1.0, rgb(90, 96, 110)}}, 1.0, true);
  stroke(cr, kOutline, 2.2);
  for (const double x : {-24.0, 0.0, 24.0})
  {
    cairo_move_to(cr, x, -4);
    cairo_line_to(cr, x * 0.7, 16 - std::abs(x) * 0.2);
    stroke(cr, rgb(110, 116, 130), 1.2, 0.8);
  }
  ellipse(cr, 0, -6, 42, 5.5);
  fillLinear(cr, 0, -11, 0, -1, {{0.0, rgb(150, 160, 178)}, {1.0, rgb(232, 238, 248)}}, 1.0, true);
  stroke(cr, kOutline, 1.6);
  // The feed: three struts up to the horn.
  for (const double x : {-30.0, 30.0, 0.0})
  {
    cairo_move_to(cr, x, -6);
    cairo_line_to(cr, 0, -40);
    stroke(cr, rgb(200, 206, 216), 1.6);
  }
  cairo_rectangle(cr, -4, -48, 8, 10);
  fillLinear(cr, -4, 0, 4, 0, {{0.0, rgb(240, 200, 110)}, {1.0, rgb(150, 100, 40)}}, 1.0, true);
  stroke(cr, kOutline, 1.2);
  cairo_restore(cr);
  // The beacon on the mast head.
  softDot(cr, 32, 58, 9, 9, rgb(255, 80, 70), 0.8);
  cairo_arc(cr, 32, 58, 2.6, 0, 2 * kPi);
  setRgba(cr, rgb(255, 220, 210), 1.0);
  cairo_fill(cr);
  return img.toTexture(r, 16.0f, 8.0f);
}

} // namespace gr
