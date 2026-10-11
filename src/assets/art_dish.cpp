// Level 32, Dish Array (theme look "dish"): a radio telescope array in the
// desert at midday. The masses are warm sandstone in wavy strata, the beds
// crossed by fine slanting laminae, pebbles weathering out of them, now and
// then an iron-red band or a dark streak of desert varnish and a fossil
// shell. Where you stand, a crust of pale wind-blown sand, rippled, with a
// pebble or two. The one-way floors are the dishes' white steel catwalks
// (perforated deck, a truss under it), the ladders galvanized steel. Behind:
// a deep blue sky paling to a hot haze at the horizon, thin cirrus high up;
// far off, red mesas and buttes, their strata lit orange on the sunny side
// and blued by the distance; close by, pale dunes with a line of small white
// dishes and a power line along a crest. The exit is an uplink antenna. The
// satellite, the heat shimmer, the sand sea and the array itself are drawn
// live (world_dish_draw.cpp).

#include "assets/art_dish.hpp"

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
constexpr Color kSandHi = rgb(240, 214, 168);
constexpr Color kSand = rgb(222, 184, 132);
constexpr Color kStone = rgb(214, 168, 116);
constexpr Color kStoneLo = rgb(170, 120, 78);
constexpr Color kIron = rgb(170, 84, 50);
constexpr Color kOutline = rgb(60, 36, 24);
constexpr Color kWhite = rgb(244, 244, 246);
constexpr Color kSteelShade = rgb(150, 156, 170);

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

// A wavy line across the tile that repeats every 64 px.
double bed(double x, double y0, double amp, double phase) { return y0 + amp * std::sin(x / 64.0 * 2 * kPi + phase) + amp * 0.4 * std::sin(x / 32.0 * 2 * kPi + phase * 1.7); }

// The sandstone under every variant: beds of slightly different tone
// between wavy bedding planes, fine cross-bedded laminae slanting inside
// them, pebbles weathering out and a fine grain. Wrapped so blocks tile.
void stoneBase(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 0, 64, 64);
  fillLinear(cr, 0, 0, 0, 64, {{0.0, kStone}, {1.0, lerpColor(kStone, kStoneLo, 0.35f)}});
  // Beds: bands between the bedding planes, each its own tone.
  constexpr std::array<double, 5> kPlanes{2, 15, 30, 41, 54};
  constexpr std::array<double, 5> kPhase{0.3, 1.9, 4.0, 2.6, 5.1};
  constexpr std::array<float, 5> kTone{0.2f, 0.55f, 0.1f, 0.7f, 0.35f};
  for (std::size_t i = 0; i < kPlanes.size(); ++i)
  {
    const double y0 = kPlanes[i], y1 = i + 1 < kPlanes.size() ? kPlanes[i + 1] : 66.0;
    const double p0 = kPhase[i], p1 = i + 1 < kPlanes.size() ? kPhase[i + 1] : kPhase[0];
    for (const double oy : {-64.0, 0.0, 64.0})
    {
      cairo_move_to(cr, 0, bed(0, y0 + oy, 1.6, p0));
      for (double x = 2; x <= 64; x += 2)
        cairo_line_to(cr, x, bed(x, y0 + oy, 1.6, p0));
      for (double x = 64; x >= 0; x -= 2)
        cairo_line_to(cr, x, bed(x, y1 + oy, 1.6, p1));
      cairo_close_path(cr);
      const Color tone = lerpColor(lighten(kStone, 0.12f), kStoneLo, kTone[i]);
      fillLinear(cr, 0, y0 + oy, 0, y1 + oy, {{0.0, lighten(tone, 0.1f)}, {1.0, darken(tone, 0.06f)}});
      // Cross-bedding: fine laminae slanting down across the bed.
      for (double lx = -8; lx < 72; lx += 7)
      {
        cairo_move_to(cr, lx, bed(lx, y0 + oy, 1.6, p0) + 1.5);
        cairo_curve_to(cr, lx + 4, bed(lx, y0 + oy, 1.6, p0) + (y1 - y0) * 0.4, lx + 9, y1 + oy - 3, lx + 14, y1 + oy - 1.5);
        stroke(cr, darken(tone, 0.18f), 0.6, 0.35);
      }
    }
  }
  // The bedding planes: a dark crease with a sunlit edge over it.
  for (std::size_t i = 0; i < kPlanes.size(); ++i)
    for (const double oy : {-64.0, 0.0, 64.0})
    {
      cairo_move_to(cr, 0, bed(0, kPlanes[i] + oy, 1.6, kPhase[i]));
      for (double x = 2; x <= 64; x += 2)
        cairo_line_to(cr, x, bed(x, kPlanes[i] + oy, 1.6, kPhase[i]));
      stroke(cr, kOutline, 1.3, 0.35);
      cairo_move_to(cr, 0, bed(0, kPlanes[i] + oy, 1.6, kPhase[i]) - 1.3);
      for (double x = 2; x <= 64; x += 2)
        cairo_line_to(cr, x, bed(x, kPlanes[i] + oy, 1.6, kPhase[i]) - 1.3);
      stroke(cr, rgb(255, 236, 200), 0.8, 0.35);
    }
  Rng rng(32011u);
  // Pebbles weathering out, each with its shadow and a sunlit cap.
  for (int i = 0; i < 6; ++i)
  {
    const double x = rng.range(3, 61), y = rng.range(3, 61), rx = rng.range(1.4f, 3.0f), ry = rx * rng.range(0.6f, 0.85f);
    const Color c = i % 3 == 0 ? rgb(150, 118, 96) : (i % 3 == 1 ? rgb(196, 160, 120) : rgb(120, 92, 74));
    wrap64([&](double ox, double oy) {
      ellipse(cr, x + ox + 0.8, y + oy + 1.0, rx, ry);
      setRgba(cr, kOutline, 0.35);
      cairo_fill(cr);
      ellipse(cr, x + ox, y + oy, rx, ry);
      fillLinear(cr, 0, y + oy - ry, 0, y + oy + ry, {{0.0, lighten(c, 0.35f)}, {1.0, c}});
    });
  }
  // A fine grain.
  for (int i = 0; i < 90; ++i)
  {
    cairo_arc(cr, rng.range(0, 64), rng.range(0, 64), rng.range(0.35f, 0.8f), 0, 2 * kPi);
    setRgba(cr, i % 3 ? rgb(120, 80, 52) : rgb(255, 240, 210), rng.range(0.12f, 0.3f));
    cairo_fill(cr);
  }
}

// Variant 1: an iron-red band through the middle, its cobbles darker.
void ironBand(cairo_t* cr)
{
  cairo_move_to(cr, 0, bed(0, 30, 1.6, 4.0));
  for (double x = 2; x <= 64; x += 2)
    cairo_line_to(cr, x, bed(x, 30, 1.6, 4.0));
  for (double x = 64; x >= 0; x -= 2)
    cairo_line_to(cr, x, bed(x, 41, 1.6, 2.6));
  cairo_close_path(cr);
  fillLinear(cr, 0, 30, 0, 41, {{0.0, lighten(kIron, 0.15f)}, {1.0, darken(kIron, 0.2f)}}, 0.75);
  for (const auto& c : {std::array<double, 3>{12, 35, 2.6}, std::array<double, 3>{30, 36, 2.0}, std::array<double, 3>{47, 34.5, 2.8}})
  {
    ellipse(cr, c[0], c[1], c[2], c[2] * 0.7);
    fillLinear(cr, 0, c[1] - c[2], 0, c[1] + c[2], {{0.0, rgb(150, 86, 60)}, {1.0, rgb(80, 40, 28)}});
  }
}

// Variant 2: a dark streak of desert varnish down from a crack, and a little
// fossil shell weathering out beside it.
void varnishAndShell(cairo_t* cr)
{
  cairo_move_to(cr, 22, 4);
  cairo_curve_to(cr, 20, 18, 26, 26, 23, 40);
  cairo_curve_to(cr, 22, 48, 25, 54, 24, 60);
  stroke(cr, rgb(90, 52, 36), 5.0, 0.32);
  cairo_move_to(cr, 22, 4);
  cairo_curve_to(cr, 20, 16, 25, 22, 23, 30);
  stroke(cr, kOutline, 1.2, 0.6);
  // The shell: a little ammonite spiral.
  cairo_save(cr);
  cairo_translate(cr, 44, 22);
  cairo_arc(cr, 0, 0, 6.5, 0, 2 * kPi);
  fillLinear(cr, 0, -6.5, 0, 6.5, {{0.0, rgb(236, 210, 170)}, {1.0, rgb(170, 130, 90)}});
  cairo_move_to(cr, 0, 0);
  for (double a = 0; a < 5 * kPi; a += 0.2)
    cairo_line_to(cr, std::cos(a) * a * 0.42, std::sin(a) * a * 0.42);
  stroke(cr, rgb(110, 76, 50), 0.9, 0.8);
  for (int k = 0; k < 10; ++k)
  {
    const double a = k * 0.63;
    cairo_move_to(cr, std::cos(a) * 4.2, std::sin(a) * 4.2);
    cairo_line_to(cr, std::cos(a) * 6.3, std::sin(a) * 6.3);
    stroke(cr, rgb(130, 92, 60), 0.6, 0.6);
  }
  cairo_restore(cr);
}

} // namespace

bool isDishArray(const Theme& t) { return std::string_view(t.look) == "dish"; }

Texture bakeDishSolid(const Renderer& r, const Theme& t, int variant)
{
  (void)t;
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  stoneBase(cr);
  if (variant == 1)
    ironBand(cr);
  else if (variant == 2)
    varnishAndShell(cr);
  return img.toTexture(r, 0.0f, 0.0f);
}

// Where you stand: a crust of pale wind-blown sand over the rock's edge,
// rippled, a pebble or two, a shadow under its lip. Kept low (2 px over the
// surface at most).
Texture bakeDishSolidTop(const Renderer& r, const Theme& t, float topOffset, int height)
{
  (void)t;
  VectorImage img(64, height);
  cairo_t* cr = img.cr();
  const double y = topOffset;
  auto edge = [&](double x) { return y - 1.2 + 0.8 * std::sin(x / 64.0 * 2 * kPi + 0.4) + 0.4 * std::sin(x / 16.0 * 2 * kPi); };
  auto under = [&](double x) { return y + 9 + 2.0 * std::sin(x / 64.0 * 4 * kPi + 1.2); };
  // The shade under the crust.
  {
    cairo_pattern_t* g = cairo_pattern_create_linear(0, y + 8, 0, y + 20);
    stop(g, 0.0, rgb(100, 56, 30), 0.35);
    stop(g, 1.0, rgb(100, 56, 30), 0.0);
    cairo_rectangle(cr, 0, y + 8, 64, 12);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  // The crust.
  cairo_move_to(cr, 0, edge(0));
  for (double x = 2; x <= 64; x += 2)
    cairo_line_to(cr, x, edge(x));
  for (double x = 64; x >= 0; x -= 2)
    cairo_line_to(cr, x, under(x));
  cairo_close_path(cr);
  fillLinear(cr, 0, y - 2, 0, y + 11, {{0.0, rgb(252, 234, 196)}, {0.35, kSandHi}, {1.0, kSand}});
  // Ripples in it.
  for (int k = 0; k < 6; ++k)
  {
    const double x0 = k * 11.0 + 2.0;
    cairo_move_to(cr, x0, y + 5.5);
    cairo_curve_to(cr, x0 + 2, y + 2.5, x0 + 6, y + 2.5, x0 + 8, y + 5.0);
    stroke(cr, rgb(200, 158, 108), 0.9, 0.55);
    cairo_move_to(cr, x0 + 1, y + 4.6);
    cairo_curve_to(cr, x0 + 2.5, y + 2.4, x0 + 5, y + 2.2, x0 + 6.5, y + 3.6);
    stroke(cr, rgb(255, 248, 228), 0.7, 0.6);
  }
  // The sunlit edge.
  cairo_move_to(cr, 0, edge(0));
  for (double x = 2; x <= 64; x += 2)
    cairo_line_to(cr, x, edge(x));
  stroke(cr, rgb(255, 250, 236), 1.4, 0.8);
  // Sand trickling over the lip in places.
  for (const double x : {14.0, 45.0})
  {
    cairo_move_to(cr, x - 4, under(x - 4) - 1);
    cairo_curve_to(cr, x - 2, under(x) + 4, x + 2, under(x) + 4, x + 4, under(x + 4) - 1);
    cairo_close_path(cr);
    setRgba(cr, kSand, 0.9);
    cairo_fill(cr);
  }
  // Pebbles.
  Rng rng(32021u);
  for (int i = 0; i < 3; ++i)
  {
    const double x = rng.range(6, 58), py = y + rng.range(1.5f, 4.0f);
    ellipse(cr, x + 0.6, py + 0.8, 1.9, 1.2);
    setRgba(cr, kOutline, 0.35);
    cairo_fill(cr);
    ellipse(cr, x, py, 1.9, 1.3);
    setRgba(cr, i % 2 ? rgb(150, 110, 84) : rgb(110, 84, 70), 1.0);
    cairo_fill(cr);
    cairo_arc(cr, x - 0.5, py - 0.5, 0.6, 0, 2 * kPi);
    setRgba(cr, rgb(255, 245, 225), 0.7);
    cairo_fill(cr);
  }
  return img.toTexture(r, 0.0f, topOffset);
}

// One-way floors: a white steel catwalk, its deck perforated, a toe board
// with a yellow edge, a small truss under it. 64 x 40, the walking surface
// at the top; it tiles side by side.
Texture bakeDishPlatform(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(64, 40);
  cairo_t* cr = img.cr();
  // The truss under the deck: a bottom chord and a V of diagonals.
  cairo_move_to(cr, 0, 14);
  cairo_line_to(cr, 32, 28);
  cairo_line_to(cr, 64, 14);
  stroke(cr, kSteelShade, 3.0);
  cairo_move_to(cr, 0, 28);
  cairo_line_to(cr, 64, 28);
  stroke(cr, rgb(70, 72, 84), 4.0);
  cairo_move_to(cr, 0, 27);
  cairo_line_to(cr, 64, 27);
  stroke(cr, rgb(214, 216, 224), 1.4);
  cairo_arc(cr, 32, 28, 2.0, 0, 2 * kPi);
  setRgba(cr, rgb(120, 124, 136), 1.0);
  cairo_fill(cr);
  // The deck edge (the toe board), white with a little shade.
  cairo_rectangle(cr, 0, 2, 64, 12);
  fillLinear(cr, 0, 2, 0, 14, {{0.0, kWhite}, {0.6, rgb(220, 222, 228)}, {1.0, kSteelShade}});
  // Perforations along it.
  for (double x = 4; x < 64; x += 8)
  {
    cairo_arc(cr, x, 8.5, 1.8, 0, 2 * kPi);
    setRgba(cr, rgb(60, 62, 74), 0.85);
    cairo_fill(cr);
  }
  // The deck's top, the yellow safety edge and its highlight.
  cairo_rectangle(cr, 0, 0, 64, 3);
  fillLinear(cr, 0, 0, 0, 3, {{0.0, rgb(255, 255, 255)}, {1.0, rgb(200, 202, 210)}});
  for (double x = 0; x < 64; x += 16)
  {
    cairo_rectangle(cr, x, 3, 8, 2);
    setRgba(cr, rgb(250, 196, 40), 0.95);
    cairo_fill(cr);
  }
  cairo_move_to(cr, 0, 14);
  cairo_line_to(cr, 64, 14);
  stroke(cr, rgb(70, 72, 86), 1.4, 0.9);
  // A little dust on the deck.
  softDot(cr, 20, 1.5, 10, 1.5, kSand, 0.35);
  return img.toTexture(r, 0.0f, 0.0f);
}

// Galvanized steel: two round rails, ribbed rungs, a sunlit edge on each.
Texture bakeDishLadder(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  for (const double y : {12.0, 44.0})
  {
    cairo_rectangle(cr, 12, y - 2.5, 40, 5);
    fillLinear(cr, 0, y - 2.5, 0, y + 2.5, {{0.0, rgb(240, 242, 246)}, {0.5, rgb(170, 176, 188)}, {1.0, rgb(90, 94, 106)}});
    cairo_rectangle(cr, 12, y - 2.5, 40, 5);
    stroke(cr, rgb(50, 52, 62), 1.1, 0.8);
    for (double x = 18; x < 48; x += 6)
    {
      cairo_move_to(cr, x, y - 2);
      cairo_line_to(cr, x, y + 2);
      stroke(cr, rgb(120, 124, 136), 0.7, 0.6);
    }
  }
  for (const double x : {10.0, 48.0})
  {
    cairo_rectangle(cr, x, -2, 6, 68);
    fillLinear(cr, x, 0, x + 6, 0, {{0.0, rgb(250, 250, 252)}, {0.45, rgb(184, 188, 198)}, {1.0, rgb(96, 100, 112)}});
    cairo_rectangle(cr, x, -2, 6, 68);
    stroke(cr, rgb(50, 52, 62), 1.2, 0.85);
    // Galvanizing spangle.
    for (int k = 0; k < 4; ++k)
    {
      cairo_rectangle(cr, x + 1.5, 6 + k * 16, 2, 5);
      setRgba(cr, rgb(255, 255, 255), 0.35);
      cairo_fill(cr);
    }
  }
  return img.toTexture(r, 16.0f, 0.0f);
}

// The sky: deep blue at the top paling through cyan to a hot white haze at
// the horizon, the sun's glare in the top left, thin cirrus streaks; under
// the horizon the far desert floor in pale bands.
Texture bakeDishSky(const Renderer& r, const Theme& t)
{
  VectorImage img(kScreenW, kScreenH);
  cairo_t* cr = img.cr();
  const double hz = kDishHorizonY;
  cairo_rectangle(cr, 0, 0, kScreenW, hz + 1);
  fillLinear(cr, 0, 0, 0, hz,
    {{0.0, t.skyTop}, {0.45, lerpColor(t.skyTop, t.skyMid, 0.7f)}, {0.8, lerpColor(t.skyMid, rgb(170, 200, 230), 0.5f)},
      {1.0, rgb(236, 226, 214)}});
  // The sun's glare off the top left.
  softDot(cr, 120, -40, 520, 420, rgb(255, 250, 230), 0.32);
  softDot(cr, 120, -40, 160, 140, rgb(255, 255, 245), 0.5);
  // Cirrus: long thin streaks, brushed.
  Rng rng(32041u);
  for (int i = 0; i < 9; ++i)
  {
    const double x = rng.range(-100, float(kScreenW) + 100), y = rng.range(60, 260), w = rng.range(180, 420);
    for (int k = 0; k < 6; ++k)
    {
      const double yy = y + k * 3.0 + rng.range(-2, 2);
      cairo_move_to(cr, x + rng.range(-20, 20), yy);
      cairo_curve_to(cr, x + w * 0.3, yy - 6, x + w * 0.7, yy + 4, x + w + rng.range(-30, 30), yy - 8);
      stroke(cr, rgb(240, 246, 255), rng.range(1.0f, 3.0f), rng.range(0.05f, 0.14f));
    }
  }
  // The hot haze along the horizon.
  softDot(cr, 640, hz, 980, 70, rgb(255, 240, 220), 0.45);
  // The desert floor far off, in pale bands toward us.
  cairo_rectangle(cr, 0, hz, kScreenW, kScreenH - hz);
  fillLinear(cr, 0, hz, 0, kScreenH, {{0.0, rgb(226, 196, 160)}, {0.2, rgb(220, 180, 132)}, {1.0, rgb(210, 160, 108)}});
  for (int i = 0; i < 14; ++i)
  {
    const double u = double(i) / 14.0, y = hz + 3 + (kScreenH - hz) * u * u;
    cairo_move_to(cr, 0, y);
    for (double x = 0; x <= kScreenW; x += 40)
      cairo_line_to(cr, x, y + std::sin(x * 0.004 + i * 1.3) * (1 + 4 * u));
    stroke(cr, i % 2 ? rgb(244, 220, 180) : rgb(190, 140, 96), 1.0 + 2.0 * u, 0.2);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

namespace
{

// A mesa (or a butte): a flat cap, steep cliffs banded with strata, a talus
// apron at its foot. The sun is to the left, so the left faces are lit.
void mesa(cairo_t* cr, double cx, double base, double w, double h, double haze, std::uint32_t seed)
{
  Rng k(seed);
  const double top = base - h, half = w * 0.5;
  const double capL = cx - half * k.range(0.55f, 0.75f), capR = cx + half * k.range(0.55f, 0.75f);
  const Color lit = lerpColor(rgb(214, 112, 66), rgb(214, 186, 196), float(haze));
  const Color shade = lerpColor(rgb(132, 56, 40), rgb(170, 150, 176), float(haze));
  const Color capC = lerpColor(rgb(196, 104, 62), rgb(206, 180, 190), float(haze));
  // The talus apron.
  cairo_move_to(cr, cx - half - h * 0.5, base);
  cairo_curve_to(cr, cx - half, base - h * 0.2, capL, base - h * 0.42, capL + 4, base - h * 0.42);
  cairo_line_to(cr, capR - 4, base - h * 0.42);
  cairo_curve_to(cr, capR, base - h * 0.42, cx + half, base - h * 0.2, cx + half + h * 0.5, base);
  cairo_close_path(cr);
  fillLinear(cr, cx - half, 0, cx + half, 0, {{0.0, lerpColor(lit, rgb(230, 190, 150), 0.3f)}, {1.0, shade}});
  // The cliffs: the lit left face, the shaded right, a notch or two.
  const double mid = capL + (capR - capL) * k.range(0.35f, 0.55f);
  cairo_move_to(cr, capL, top);
  cairo_line_to(cr, mid, top);
  cairo_line_to(cr, mid + k.range(-6, 6), base - h * 0.4);
  cairo_line_to(cr, capL - 6, base - h * 0.4);
  cairo_close_path(cr);
  fillLinear(cr, 0, top, 0, base - h * 0.4, {{0.0, lighten(lit, 0.08f)}, {1.0, lit}});
  cairo_move_to(cr, mid, top);
  cairo_line_to(cr, capR, top);
  cairo_line_to(cr, capR + 6, base - h * 0.4);
  cairo_line_to(cr, mid + k.range(-6, 6), base - h * 0.4);
  cairo_close_path(cr);
  fillLinear(cr, 0, top, 0, base - h * 0.4, {{0.0, shade}, {1.0, darken(shade, 0.1f)}});
  // Strata across both faces.
  for (int i = 1; i < 6; ++i)
  {
    const double y = top + (h * 0.6) * i / 6.0 + k.range(-2, 2);
    cairo_move_to(cr, capL - 3, y);
    cairo_line_to(cr, capR + 3, y + k.range(-1.5f, 1.5f));
    stroke(cr, i % 2 ? darken(lit, 0.25f) : lighten(lit, 0.18f), i % 2 ? 2.0 : 1.2, 0.4 * (1.0 - haze * 0.6));
  }
  // Vertical runnels down the cliff.
  for (int i = 0; i < 6; ++i)
  {
    const double x = capL + (capR - capL) * k.range(0.05f, 0.95f);
    cairo_move_to(cr, x, top + 4);
    cairo_line_to(cr, x + k.range(-3, 3), base - h * 0.42);
    stroke(cr, darken(shade, 0.2f), 1.2, 0.3 * (1.0 - haze * 0.6));
  }
  // The cap rock: a darker, harder band, its top edge sunlit.
  cairo_rectangle(cr, capL - 4, top - 3, capR - capL + 8, 7);
  fillLinear(cr, 0, top - 3, 0, top + 4, {{0.0, lighten(capC, 0.15f)}, {1.0, darken(capC, 0.2f)}});
  cairo_move_to(cr, capL - 4, top - 3);
  cairo_line_to(cr, capR + 4, top - 3);
  stroke(cr, rgb(255, 214, 170), 1.2, 0.5 * (1.0 - haze * 0.5));
}

} // namespace

// Far: red mesas and buttes along the horizon, the farthest pale and blue
// with distance, the nearer ones redder; a haze along their feet.
Texture bakeDishFar(const Renderer& r, const Theme& t, int layerW)
{
  (void)t;
  VectorImage img(layerW, kScreenH);
  cairo_t* cr = img.cr();
  const double W = layerW, hz = kDishHorizonY + 6.0;
  Rng rng(32051u);
  for (int row = 0; row < 2; ++row)
  {
    for (double x = rng.range(0, 200); x < W; x += rng.range(row == 0 ? 280.0f : 360.0f, row == 0 ? 520.0f : 700.0f))
    {
      const double w = row == 0 ? rng.range(140, 380) : rng.range(160, 460);
      const double h = row == 0 ? rng.range(50, 110) : rng.range(80, 170);
      const std::uint32_t seed = std::uint32_t(32061 + row * 1000 + int(x));
      for (const double ox : {-W, 0.0, W})
      {
        const double cx = x + ox;
        if (cx + w < -80 || cx - w > W + 80)
          continue;
        mesa(cr, cx, hz + row * 8.0, w, h, row == 0 ? 0.6 : 0.22, seed);
      }
    }
  }
  // Haze at the feet of it all.
  cairo_rectangle(cr, 0, hz - 50, W, 70);
  {
    cairo_pattern_t* g = cairo_pattern_create_linear(0, hz - 50, 0, hz + 20);
    stop(g, 0.0, rgb(255, 236, 214), 0.0);
    stop(g, 0.7, rgb(255, 236, 214), 0.45);
    stop(g, 1.0, rgb(240, 214, 180), 0.0);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// Near: pale dunes rolling toward us, each crest sharp with its shadow
// side; on a far crest a line of small white dishes on pylons and a power
// line strung between poles.
Texture bakeDishNear(const Renderer& r, const Theme& t, int layerW)
{
  (void)t;
  VectorImage img(layerW, kScreenH);
  cairo_t* cr = img.cr();
  const double W = layerW;
  // Each dune ridge: a sum of sines that repeats over the layer's width.
  auto ridge = [&](double x, double base, double amp, int seed) {
    const double u = x / W * 2 * kPi;
    return base - amp * (0.55 + 0.25 * std::sin(u * 3 + seed) + 0.15 * std::sin(u * 7 + seed * 2.3) + 0.08 * std::sin(u * 13 + seed * 0.7));
  };
  constexpr std::array<double, 3> kBase{500, 548, 610};
  constexpr std::array<double, 3> kAmp{60, 70, 80};
  for (int i = 0; i < 3; ++i)
  {
    const Color lit = lerpColor(rgb(246, 226, 188), rgb(236, 206, 160), float(i) / 2.0f);
    const Color shade = lerpColor(rgb(214, 180, 150), rgb(204, 160, 112), float(i) / 2.0f);
    cairo_move_to(cr, 0, ridge(0, kBase[std::size_t(i)], kAmp[std::size_t(i)], i));
    for (double x = 8; x <= W; x += 8)
      cairo_line_to(cr, x, ridge(x, kBase[std::size_t(i)], kAmp[std::size_t(i)], i));
    cairo_line_to(cr, W, kScreenH);
    cairo_line_to(cr, 0, kScreenH);
    cairo_close_path(cr);
    fillLinear(cr, 0, kBase[std::size_t(i)] - kAmp[std::size_t(i)], 0, kScreenH, {{0.0, lit}, {0.5, shade}, {1.0, darken(shade, 0.05f)}});
    // The shadow side behind each crest (the sun is to the left): where the
    // ridge falls away to the right, a darker slope.
    for (double x = 0; x < W; x += 8)
    {
      const double y0 = ridge(x, kBase[std::size_t(i)], kAmp[std::size_t(i)], i);
      const double y1 = ridge(x + 8, kBase[std::size_t(i)], kAmp[std::size_t(i)], i);
      if (y1 > y0 + 0.4)
      {
        cairo_move_to(cr, x, y0);
        cairo_line_to(cr, x + 8, y1);
        cairo_line_to(cr, x + 8, y1 + 26 + (y1 - y0) * 6);
        cairo_line_to(cr, x, y0 + 26 + (y1 - y0) * 4);
        cairo_close_path(cr);
        setRgba(cr, rgb(176, 130, 100), 0.18);
        cairo_fill(cr);
      }
    }
    // The crest line, sunlit.
    cairo_move_to(cr, 0, ridge(0, kBase[std::size_t(i)], kAmp[std::size_t(i)], i));
    for (double x = 8; x <= W; x += 8)
      cairo_line_to(cr, x, ridge(x, kBase[std::size_t(i)], kAmp[std::size_t(i)], i));
    stroke(cr, rgb(255, 248, 230), 1.6, 0.6);
    // Ripples on the slopes.
    Rng rng(std::uint32_t(32071 + i));
    for (int k = 0; k < 120; ++k)
    {
      const double x = rng.range(0, float(W)), top = ridge(x, kBase[std::size_t(i)], kAmp[std::size_t(i)], i);
      const double y = top + rng.range(8, 90);
      const double w = rng.range(16, 46);
      cairo_move_to(cr, x, y);
      cairo_curve_to(cr, x + w * 0.3, y - 3, x + w * 0.7, y - 3, x + w, y + 1);
      stroke(cr, rgb(190, 146, 104), 1.0, 0.16);
    }
    if (i == 0)
    {
      // The small dishes along this crest, and the power line.
      double lastX = -1, lastY = 0;
      for (double x = 120; x < W; x += 210)
      {
        const double y = ridge(x, kBase[0], kAmp[0], 0) + 4;
        // Pylon.
        cairo_move_to(cr, x, y);
        cairo_line_to(cr, x, y - 26);
        stroke(cr, rgb(180, 176, 180), 2.2);
        // Dish, tipped up to the right.
        cairo_save(cr);
        cairo_translate(cr, x, y - 30);
        cairo_rotate(cr, -0.5);
        cairo_move_to(cr, -15, 0);
        cairo_curve_to(cr, -10, 11, 10, 11, 15, 0);
        cairo_close_path(cr);
        fillLinear(cr, 0, -2, 0, 9, {{0.0, rgb(250, 250, 252)}, {1.0, rgb(184, 186, 198)}});
        ellipse(cr, 0, 0, 15, 2.6);
        setRgba(cr, rgb(214, 218, 230), 1.0);
        cairo_fill(cr);
        cairo_move_to(cr, 0, 0);
        cairo_line_to(cr, 0, -12);
        stroke(cr, rgb(170, 170, 180), 1.0);
        cairo_restore(cr);
        // A pole and its wire.
        const double px = x + 105, py = ridge(px, kBase[0], kAmp[0], 0) + 4;
        cairo_move_to(cr, px, py);
        cairo_line_to(cr, px, py - 44);
        stroke(cr, rgb(120, 90, 70), 2.0, 0.8);
        cairo_move_to(cr, px - 6, py - 40);
        cairo_line_to(cr, px + 6, py - 40);
        stroke(cr, rgb(120, 90, 70), 1.6, 0.8);
        if (lastX >= 0)
        {
          cairo_move_to(cr, lastX, lastY - 40);
          cairo_curve_to(cr, lastX + 70, lastY - 22, px - 70, py - 22, px, py - 40);
          stroke(cr, rgb(70, 60, 60), 0.9, 0.5);
        }
        lastX = px;
        lastY = py;
      }
    }
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

Texture bakeDishExitBase(const Renderer& r, const Theme& t)
{
  (void)t;
  // 96 wide, 144 tall; (0, 0) is the top-left of the 64 x 128 exit area.
  VectorImage img(96, 144);
  cairo_t* cr = img.cr();
  cairo_translate(cr, 16, 8);
  // The tripod: white tubes, guyed.
  for (const auto& leg : {std::array<double, 2>{-8, 32}, std::array<double, 2>{72, 32}, std::array<double, 2>{32, 32}})
  {
    cairo_move_to(cr, leg[1], 70);
    cairo_line_to(cr, leg[0], 130);
    stroke(cr, rgb(40, 40, 50), 7);
    cairo_move_to(cr, leg[1], 70);
    cairo_line_to(cr, leg[0], 130);
    stroke(cr, rgb(236, 238, 244), 4);
  }
  for (const double x : {-8.0, 72.0, 32.0})
  {
    cairo_rectangle(cr, x - 7, 127, 14, 5);
    setRgba(cr, rgb(110, 100, 96), 1.0);
    cairo_fill(cr);
  }
  cairo_move_to(cr, 6, 104);
  cairo_line_to(cr, 58, 104);
  stroke(cr, rgb(190, 194, 204), 3);
  // The equipment box with its cable.
  cairo_rectangle(cr, 40, 90, 22, 16);
  fillLinear(cr, 0, 90, 0, 106, {{0.0, rgb(250, 250, 252)}, {1.0, rgb(170, 174, 186)}}, 1.0, true);
  stroke(cr, rgb(40, 40, 50), 1.4);
  cairo_rectangle(cr, 44, 94, 8, 3);
  setRgba(cr, rgb(90, 220, 120), 1.0);
  cairo_fill(cr);
  cairo_move_to(cr, 36, 76);
  cairo_curve_to(cr, 40, 92, 44, 86, 48, 90);
  stroke(cr, rgb(30, 30, 36), 2.2);
  // The mount head.
  cairo_rectangle(cr, 24, 58, 16, 18);
  fillLinear(cr, 24, 0, 40, 0, {{0.0, rgb(250, 250, 252)}, {1.0, rgb(150, 156, 170)}}, 1.0, true);
  stroke(cr, rgb(40, 40, 50), 1.6);
  // The dish, tipped up to the right toward the satellite: a deep white
  // bowl, its rim bright, ribs on its back, the feed on three legs.
  cairo_save(cr);
  cairo_translate(cr, 32, 44);
  cairo_rotate(cr, 0.45);
  cairo_move_to(cr, -40, -6);
  cairo_curve_to(cr, -28, 26, 28, 26, 40, -6);
  cairo_close_path(cr);
  fillLinear(cr, 0, -8, 0, 22, {{0.0, rgb(255, 255, 255)}, {0.55, rgb(214, 218, 228)}, {1.0, rgb(140, 146, 162)}}, 1.0, true);
  stroke(cr, rgb(40, 40, 50), 2.0);
  for (const double x : {-22.0, 0.0, 22.0})
  {
    cairo_move_to(cr, x, -4);
    cairo_line_to(cr, x * 0.7, 16 - std::abs(x) * 0.2);
    stroke(cr, rgb(150, 156, 172), 1.1, 0.8);
  }
  ellipse(cr, 0, -6, 40, 5.5);
  fillLinear(cr, 0, -11, 0, -1, {{0.0, rgb(190, 196, 212)}, {1.0, rgb(250, 252, 255)}}, 1.0, true);
  stroke(cr, rgb(40, 40, 50), 1.5);
  for (const double x : {-28.0, 28.0, 0.0})
  {
    cairo_move_to(cr, x, -6);
    cairo_line_to(cr, 0, -38);
    stroke(cr, rgb(220, 224, 232), 1.5);
  }
  cairo_rectangle(cr, -4, -46, 8, 10);
  fillLinear(cr, -4, 0, 4, 0, {{0.0, rgb(240, 200, 110)}, {1.0, rgb(150, 100, 40)}}, 1.0, true);
  stroke(cr, rgb(40, 40, 50), 1.2);
  cairo_restore(cr);
  // The beacon on the mount head.
  softDot(cr, 32, 56, 10, 10, rgb(255, 70, 50), 0.8);
  cairo_arc(cr, 32, 56, 2.6, 0, 2 * kPi);
  setRgba(cr, rgb(255, 220, 210), 1.0);
  cairo_fill(cr);
  return img.toTexture(r, 16.0f, 8.0f);
}

} // namespace gr
