// Level 29, Dead Air (theme look "dead_air"): a black void of TV static
// between channels. The masses are clean white studio panels, matte, a
// hairline seam where they meet and a faint grain; now and then a grey
// registration mark or a little step wedge printed on one, as if the floor
// were cut out of a test card. Where you stand, a bright white lip that
// glows into the dark, its edge fringed red above and blue below like a
// picture out of convergence. One-way floors are thin white bars, climbables
// a white rack. Behind: the black of a dead tube (its faint glow in the
// middle, frozen grain, scanlines, the dark corners); far off, the ghosts of
// lost broadcasts (colour bars sliced and slipped by a rolling hold, test
// card rings); close by, clouds of snow drifting and the thin bright lines
// of interference. The live snow over all of it is drawn by the level
// (world_signal_draw.cpp).

#include "assets/art_signal.hpp"

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
constexpr Color kPanel = rgb(240, 240, 244);
constexpr Color kPanelLow = rgb(222, 223, 229);
constexpr Color kSeam = rgb(150, 152, 164);

// SMPTE's seven bars, the castellations under them and the bottom row
// (-I, white, +Q, black, the PLUGE blacks), in 75% colours.
constexpr std::array<Color, 7> kBars{rgb(192, 192, 192), rgb(192, 192, 0), rgb(0, 192, 192), rgb(0, 192, 0),
  rgb(192, 0, 192), rgb(192, 0, 0), rgb(0, 0, 192)};
constexpr std::array<Color, 7> kCastle{rgb(0, 0, 192), rgb(19, 19, 19), rgb(192, 0, 192), rgb(19, 19, 19),
  rgb(0, 192, 192), rgb(19, 19, 19), rgb(192, 192, 192)};

void setRgba(cairo_t* cr, Color c, double a)
{
  cairo_set_source_rgba(cr, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, a);
}

void stop(cairo_pattern_t* g, double at, Color c, double a = 1.0)
{
  cairo_pattern_add_color_stop_rgba(g, at, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, a);
}

void fillLinear(cairo_t* cr, double x0, double y0, double x1, double y1, std::initializer_list<std::pair<double, Color>> stops,
  double alpha = 1.0)
{
  cairo_pattern_t* g = cairo_pattern_create_linear(x0, y0, x1, y1);
  for (const auto& s : stops)
    stop(g, s.first, s.second, alpha);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
}

void line(cairo_t* cr, double x0, double y0, double x1, double y1, Color c, double a, double w)
{
  cairo_move_to(cr, x0, y0);
  cairo_line_to(cr, x1, y1);
  setRgba(cr, c, a);
  cairo_set_line_width(cr, w);
  cairo_stroke(cr);
}

// A matte grain over a tile: tiny grey flecks, wrapped so the blocks tile.
void grain(cairo_t* cr, Rng& rng, int count, double alpha)
{
  for (int i = 0; i < count; ++i)
  {
    const double x = rng.range(0, 64), y = rng.range(0, 64), s = rng.range(0.8f, 1.6f);
    cairo_rectangle(cr, x, y, s, s);
    setRgba(cr, i % 3 ? rgb(120, 122, 136) : rgb(255, 255, 255), alpha * rng.range(0.5f, 1.0f));
    cairo_fill(cr);
  }
}

// One lost broadcast: a field of colour bars `w` x `h` at (x, y), its rows
// slipped sideways in bands by a rolling hold, scanned, and faded out at
// every edge.
void ghostBroadcast(cairo_t* cr, double x, double y, double w, double h, double alpha, std::uint32_t seed)
{
  Rng rng(seed);
  cairo_push_group(cr);
  double by = y;
  while (by < y + h)
  {
    const double bandH = rng.range(8, 34);
    const double slip = rng.uniform() < 0.35f ? rng.range(-60, 60) : 0.0;
    const double y1 = std::min(y + h, by + bandH);
    cairo_save(cr);
    cairo_rectangle(cr, x - 80, by, w + 160, y1 - by);
    cairo_clip(cr);
    const double bw = w / 7.0;
    for (int i = 0; i < 7; ++i)
    {
      const double bx = x + slip + i * bw;
      // The bars, the castellations, then the bottom row.
      cairo_rectangle(cr, bx, y, bw + 0.5, h * 0.67);
      setRgba(cr, kBars[std::size_t(i)], 1.0);
      cairo_fill(cr);
      cairo_rectangle(cr, bx, y + h * 0.67, bw + 0.5, h * 0.08);
      setRgba(cr, kCastle[std::size_t(i)], 1.0);
      cairo_fill(cr);
    }
    const std::array<std::pair<double, Color>, 6> bottom{{{1.25, rgb(0, 33, 76)}, {1.25, rgb(235, 235, 235)},
      {1.25, rgb(50, 0, 106)}, {1.5, rgb(19, 19, 19)}, {0.33, rgb(8, 8, 8)}, {1.42, rgb(19, 19, 19)}}};
    double cx = x + slip;
    for (const auto& b : bottom)
    {
      cairo_rectangle(cr, cx, y + h * 0.75, b.first * bw + 0.5, h * 0.25);
      setRgba(cr, b.second, 1.0);
      cairo_fill(cr);
      cx += b.first * bw;
    }
    cairo_restore(cr);
    by = y1;
  }
  // Scan lines, darker every third px.
  for (double sy = y; sy < y + h; sy += 3)
  {
    cairo_rectangle(cr, x - 80, sy, w + 160, 1.2);
    setRgba(cr, rgb(0, 0, 0), 0.45);
    cairo_fill(cr);
  }
  cairo_pop_group_to_source(cr);
  // Faded at the edges: across, and again up and down.
  cairo_pattern_t* m = cairo_pattern_create_radial(x + w * 0.5, y + h * 0.5, std::min(w, h) * 0.15, x + w * 0.5,
    y + h * 0.5, std::max(w, h) * 0.62);
  cairo_pattern_add_color_stop_rgba(m, 0.0, 0, 0, 0, alpha);
  cairo_pattern_add_color_stop_rgba(m, 0.6, 0, 0, 0, alpha * 0.6);
  cairo_pattern_add_color_stop_rgba(m, 1.0, 0, 0, 0, 0.0);
  cairo_mask(cr, m);
  cairo_pattern_destroy(m);
}

// A cloud of snow: flecks bunched round (x, y), brighter in the middle.
void snowCloud(cairo_t* cr, double x, double y, double rx, double ry, int count, double alpha, Rng& rng)
{
  cairo_pattern_t* g = cairo_pattern_create_radial(x, y, 0, x, y, std::max(rx, ry));
  cairo_pattern_add_color_stop_rgba(g, 0, 0.6, 0.6, 0.65, 0.05 * alpha / 0.3);
  cairo_pattern_add_color_stop_rgba(g, 1, 0.6, 0.6, 0.65, 0.0);
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_scale(cr, 1.0, ry / rx);
  cairo_translate(cr, -x, -y);
  cairo_arc(cr, x, y, rx, 0, 2 * kPi);
  cairo_restore(cr);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
  for (int i = 0; i < count; ++i)
  {
    // Roughly normal: the sum of three uniforms.
    const double u = (rng.uniform() + rng.uniform() + rng.uniform()) / 1.5 - 1.0;
    const double v = (rng.uniform() + rng.uniform() + rng.uniform()) / 1.5 - 1.0;
    const double fall = 1.0 - std::min(1.0, std::sqrt(u * u + v * v));
    const double w = rng.range(1.4f, 3.6f);
    cairo_rectangle(cr, x + u * rx, y + v * ry, w, rng.range(1.0f, 1.8f));
    const int g8 = int(rng.range(150, 255));
    setRgba(cr, rgb(g8, g8, std::min(255, g8 + 8)), alpha * (0.35 + 0.65 * fall) * rng.range(0.4f, 1.0f));
    cairo_fill(cr);
  }
}

} // namespace

bool isDeadAir(const Theme& t) { return std::string_view(t.look) == "dead_air"; }

// A white studio panel: matte, a hair of light along its top and left edges
// and a grey seam along the others. Variant 1 carries a faint registration
// mark, variant 2 a little grey step wedge.
Texture bakeSignalSolid(const Renderer& r, const Theme& t, int variant)
{
  (void)t;
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  cairo_rectangle(cr, 0, 0, 64, 64);
  fillLinear(cr, 0, 0, 64, 64, {{0.0, rgb(246, 246, 249)}, {0.55, kPanel}, {1.0, kPanelLow}});
  Rng rng(29011u); // the same grain on every variant, so they sit together
  grain(cr, rng, 70, 0.08);
  // A soft sheen across the panel.
  cairo_rectangle(cr, 0, 0, 64, 64);
  {
    cairo_pattern_t* g = cairo_pattern_create_radial(18, 14, 2, 18, 14, 58);
    cairo_pattern_add_color_stop_rgba(g, 0, 1, 1, 1, 0.35);
    cairo_pattern_add_color_stop_rgba(g, 1, 1, 1, 1, 0.0);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  if (variant == 1)
  {
    // A registration mark: a ring and a cross, printed pale.
    cairo_arc(cr, 44, 42, 7, 0, 2 * kPi);
    setRgba(cr, kSeam, 0.4);
    cairo_set_line_width(cr, 1.2);
    cairo_stroke(cr);
    line(cr, 34, 42, 54, 42, kSeam, 0.4, 1.0);
    line(cr, 44, 32, 44, 52, kSeam, 0.4, 1.0);
  }
  else if (variant == 2)
  {
    // A step wedge: four greys in a row.
    for (int i = 0; i < 4; ++i)
    {
      const int v = 200 - i * 36;
      cairo_rectangle(cr, 12 + i * 9, 46, 9, 6);
      setRgba(cr, rgb(v, v, v + 6), 0.55);
      cairo_fill(cr);
    }
    cairo_rectangle(cr, 12, 46, 36, 6);
    setRgba(cr, kSeam, 0.35);
    cairo_set_line_width(cr, 0.8);
    cairo_stroke(cr);
  }
  // The edges: light in along the top and left, a seam along the bottom
  // and right.
  cairo_rectangle(cr, 0, 0, 64, 1.5);
  setRgba(cr, rgb(255, 255, 255), 0.95);
  cairo_fill(cr);
  cairo_rectangle(cr, 0, 0, 1.5, 64);
  setRgba(cr, rgb(255, 255, 255), 0.8);
  cairo_fill(cr);
  cairo_rectangle(cr, 0, 63, 64, 1);
  setRgba(cr, kSeam, 0.55);
  cairo_fill(cr);
  cairo_rectangle(cr, 63, 0, 1, 64);
  setRgba(cr, kSeam, 0.45);
  cairo_fill(cr);
  return img.toTexture(r, 0.0f, 0.0f);
}

// Where you stand: a bright white lip, a soft glow over it into the void,
// a hairline of red above and blue below (the picture out of convergence)
// and a shadow under the lip on the panel.
Texture bakeSignalSolidTop(const Renderer& r, const Theme& t, float topOffset, int height)
{
  (void)t;
  VectorImage img(64, height);
  cairo_t* cr = img.cr();
  const double y = topOffset;
  // The glow into the dark.
  cairo_rectangle(cr, 0, y - 18, 64, 18);
  {
    cairo_pattern_t* g = cairo_pattern_create_linear(0, y - 18, 0, y);
    cairo_pattern_add_color_stop_rgba(g, 0, 1, 1, 1, 0.0);
    cairo_pattern_add_color_stop_rgba(g, 1, 1, 1, 1, 0.16);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  // The red fringe above.
  cairo_rectangle(cr, 0, y - 2.2, 64, 1.4);
  setRgba(cr, rgb(255, 70, 80), 0.32);
  cairo_fill(cr);
  // The lip.
  cairo_rectangle(cr, 0, y - 0.8, 64, 8);
  fillLinear(cr, 0, y - 1, 0, y + 7, {{0.0, rgb(255, 255, 255)}, {0.7, rgb(250, 250, 253)}, {1.0, rgb(232, 233, 238)}});
  cairo_rectangle(cr, 0, y + 7, 64, 1.2);
  setRgba(cr, rgb(168, 170, 182), 0.8);
  cairo_fill(cr);
  // The blue fringe under it.
  cairo_rectangle(cr, 0, y + 8.2, 64, 1.2);
  setRgba(cr, rgb(70, 130, 255), 0.22);
  cairo_fill(cr);
  // The lip's shadow on the panel.
  cairo_rectangle(cr, 0, y + 8, 64, 10);
  {
    cairo_pattern_t* g = cairo_pattern_create_linear(0, y + 8, 0, y + 18);
    cairo_pattern_add_color_stop_rgba(g, 0, 0.3, 0.3, 0.38, 0.16);
    cairo_pattern_add_color_stop_rgba(g, 1, 0.3, 0.3, 0.38, 0.0);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  // A sharp highlight right on the edge.
  cairo_rectangle(cr, 0, y - 0.8, 64, 1.4);
  setRgba(cr, rgb(255, 255, 255), 1.0);
  cairo_fill(cr);
  return img.toTexture(r, 0.0f, topOffset);
}

// A one-way floor: a thin white bar with a grey underside and a faint glow
// on top.
Texture bakeSignalPlatform(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(64, 40);
  cairo_t* cr = img.cr();
  cairo_rectangle(cr, 0, 16, 64, 10);
  {
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 16, 0, 26);
    cairo_pattern_add_color_stop_rgba(g, 0, 0.0, 0.0, 0.0, 0.35);
    cairo_pattern_add_color_stop_rgba(g, 1, 0.0, 0.0, 0.0, 0.0);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  cairo_rectangle(cr, 0, 2, 64, 14);
  fillLinear(cr, 0, 2, 0, 16, {{0.0, rgb(255, 255, 255)}, {0.5, kPanel}, {1.0, rgb(196, 198, 208)}});
  cairo_rectangle(cr, 0, 2, 64, 1.4);
  setRgba(cr, rgb(255, 255, 255), 1.0);
  cairo_fill(cr);
  cairo_rectangle(cr, 0, 15, 64, 1);
  setRgba(cr, kSeam, 0.9);
  cairo_fill(cr);
  // A joint and two rivets.
  cairo_rectangle(cr, 63, 2, 1, 14);
  setRgba(cr, kSeam, 0.6);
  cairo_fill(cr);
  for (const double x : {8.0, 56.0})
  {
    cairo_arc(cr, x, 9, 1.6, 0, 2 * kPi);
    setRgba(cr, kSeam, 0.7);
    cairo_fill(cr);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// A climbable: a white rack, two rails and a rung, drawn 16 px left of its
// block like the others.
Texture bakeSignalLadder(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  for (const double y : {16.0, 48.0})
  {
    cairo_rectangle(cr, 18, y - 3, 28, 6);
    fillLinear(cr, 0, y - 3, 0, y + 3, {{0.0, rgb(255, 255, 255)}, {1.0, rgb(190, 192, 204)}});
  }
  for (const double x : {17.0, 47.0})
  {
    cairo_rectangle(cr, x - 4, 0, 8, 64);
    fillLinear(cr, x - 4, 0, x + 4, 0, {{0.0, rgb(255, 255, 255)}, {0.6, kPanel}, {1.0, rgb(170, 172, 186)}});
  }
  return img.toTexture(r, 16.0f, 0.0f);
}

// The black of a dead tube: a faint glow in the middle, frozen grain, scan
// lines and the dark corners.
Texture bakeSignalSky(const Renderer& r, const Theme& t)
{
  VectorImage img(kScreenW, kScreenH);
  cairo_t* cr = img.cr();
  cairo_rectangle(cr, 0, 0, kScreenW, kScreenH);
  fillLinear(cr, 0, 0, 0, kScreenH, {{0.0, t.skyTop}, {0.55, t.skyMid}, {1.0, t.skyTop}});
  // The tube's faint glow.
  cairo_rectangle(cr, 0, 0, kScreenW, kScreenH);
  {
    cairo_pattern_t* g = cairo_pattern_create_radial(640, 380, 40, 640, 380, 760);
    stop(g, 0.0, t.skyBottom, 0.75);
    stop(g, 0.5, t.skyMid, 0.35);
    stop(g, 1.0, t.skyTop, 0.0);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  // Frozen grain: fine grey flecks, more in the middle.
  Rng rng(29021u);
  for (int i = 0; i < 9000; ++i)
  {
    const double x = rng.range(0, float(kScreenW)), y = rng.range(0, float(kScreenH));
    const double d = std::hypot((x - 640) / 640.0, (y - 360) / 360.0);
    cairo_rectangle(cr, x, y, rng.range(1.0f, 2.4f), 1.1);
    setRgba(cr, rgb(200, 200, 210), rng.range(0.03f, 0.12f) * std::max(0.2, 1.0 - d * 0.7));
    cairo_fill(cr);
  }
  // Scan lines.
  for (int y = 0; y < kScreenH; y += 3)
  {
    cairo_rectangle(cr, 0, y, kScreenW, 1);
    setRgba(cr, rgb(0, 0, 0), 0.28);
    cairo_fill(cr);
  }
  // The dark corners.
  cairo_rectangle(cr, 0, 0, kScreenW, kScreenH);
  cairo_pattern_t* v = cairo_pattern_create_radial(640, 360, 380, 640, 360, 820);
  cairo_pattern_add_color_stop_rgba(v, 0, 0, 0, 0, 0.0);
  cairo_pattern_add_color_stop_rgba(v, 1, 0, 0, 0, 0.7);
  cairo_set_source(cr, v);
  cairo_fill(cr);
  cairo_pattern_destroy(v);
  return img.toTexture(r, 0.0f, 0.0f);
}

// Far: the ghosts of lost broadcasts, colour bars sliced by a slipping
// hold, and the faint rings of test cards between them.
Texture bakeSignalFar(const Renderer& r, const Theme& t, int layerW)
{
  (void)t;
  VectorImage img(layerW, kScreenH);
  cairo_t* cr = img.cr();
  const double W = layerW;
  // Test-card rings, wrapped across the seam.
  for (const auto& ring : {std::array<double, 3>{620, 300, 230}, std::array<double, 3>{1900, 420, 280}})
    for (const double ox : {-W, 0.0, W})
    {
      for (int k = 0; k < 3; ++k)
      {
        cairo_arc(cr, ring[0] + ox, ring[1], ring[2] - k * 34, 0, 2 * kPi);
        setRgba(cr, rgb(220, 220, 235), 0.07 - k * 0.015);
        cairo_set_line_width(cr, k == 0 ? 3.0 : 1.5);
        cairo_stroke(cr);
      }
      line(cr, ring[0] + ox - ring[2], ring[1], ring[0] + ox + ring[2], ring[1], rgb(220, 220, 235), 0.04, 1.2);
      line(cr, ring[0] + ox, ring[1] - ring[2], ring[0] + ox, ring[1] + ring[2], rgb(220, 220, 235), 0.04, 1.2);
    }
  // Three lost broadcasts.
  const std::array<std::array<double, 5>, 3> casts{{{180, 150, 520, 300, 0.07}, {1080, 300, 420, 250, 0.05},
    {1820, 120, 560, 320, 0.06}}};
  std::uint32_t seed = 29031u;
  for (const auto& c : casts)
  {
    for (const double ox : {-W, 0.0, W})
      if (c[0] + ox < W + 200 && c[0] + c[2] + ox > -200)
        ghostBroadcast(cr, c[0] + ox, c[1], c[2], c[3], c[4], seed);
    seed += 7u;
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// Near: clouds of snow drifting and the thin bright lines of interference,
// each fringed red and blue.
Texture bakeSignalNear(const Renderer& r, const Theme& t, int layerW)
{
  (void)t;
  VectorImage img(layerW, kScreenH);
  cairo_t* cr = img.cr();
  const double W = layerW;
  Rng rng(29041u);
  for (int i = 0; i < 9; ++i)
  {
    const double x = 140 + i * (W - 280) / 8.0 + rng.range(-80, 80), y = rng.range(120, 600);
    const double rx = rng.range(110, 220), ry = rx * rng.range(0.35f, 0.6f);
    Rng flecks(std::uint32_t(29051 + i * 13));
    for (const double ox : {-W, 0.0, W})
      if (x + ox - rx < W && x + ox + rx > 0)
      {
        Rng f = flecks;
        snowCloud(cr, x + ox, y, rx, ry, 700, 0.3, f);
      }
  }
  for (int i = 0; i < 7; ++i)
  {
    const double x = rng.range(0, float(W)), y = rng.range(60, 660), l = rng.range(260, 820);
    for (const double ox : {-W, 0.0, W})
    {
      if (x + ox > W || x + ox + l < 0)
        continue;
      line(cr, x + ox, y - 2, x + ox + l, y - 2, rgb(255, 60, 70), 0.08, 1.2);
      line(cr, x + ox, y + 2, x + ox + l, y + 2, rgb(60, 120, 255), 0.08, 1.2);
      line(cr, x + ox, y, x + ox + l, y, rgb(240, 240, 255), 0.16, 1.4);
    }
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

} // namespace gr
