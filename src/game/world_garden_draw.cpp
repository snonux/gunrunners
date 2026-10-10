// Level 25, Tiny Kingdom: drawing. See world_garden.cpp and world_glide.cpp
// for the logic.
//
// Behind the tiles (drawGardenWalls): the soil's back wall in the burrows
// (the ant pantry, the hollow stem, the ditch). Over the tiles
// (drawGardenBack): the pond (its water, ripples and lily pads), the
// garden's day and dusk platforms (open flowers and unfurled leaves by day,
// glowing mushroom caps by dusk; they droop for the last 45 frames of their
// half, flowers curling shut and caps shrinking, and sleep through the
// other half), the misted buds, the seeds and their stalks, the dandelion
// (its leaves, its stalk, the yellow head that turns into a seed clock) and
// the seed head the runner rides, the ladybug whose spots spell G-U-N and
// then 4-2, the LOT 42 seed packet, the rare butterfly's specimen frame. In
// front (drawGardenFront): the rare butterfly, the Chrono Mister's mist,
// the beetles' boiling jets, the ants' alarm, then the dusk light (the
// caps and the glow-worms glowing through it) and the time-lapse sweep (the
// sun or the moon racing over the sky). drawGardenHud: the sundial and the
// narrator's subtitles. The Dandelion Drift bonus (drawGlideBack/Front):
// the lawn, the pond and the fence, the gusts, the dragonflies and the seed
// head the runner hangs from.

#include "game/world.hpp"

#include "assets/art.hpp"
#include "base/math.hpp"
#include "render/vector.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <initializer_list>
#include <string>
#include <vector>

// baked() returns a reference into the Art's cache, not into its key or
// painter arguments; GCC's heuristic flags every call that passes temporaries.
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 13
#pragma GCC diagnostic ignored "-Wdangling-reference"
#endif

namespace gr
{

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr float kTilePx = float(kTileSize) * kPixelScale; // 64
constexpr float kCellPx = float(kCellSize) * kPixelScale; // 32
constexpr Color kInk = rgb(34, 24, 16);
constexpr int kFlowerStages = 8;

bool offScreen(float x, float y, float w, float h)
{
  return x > float(kScreenW) + 96.0f || x + w < -96.0f || y > float(kScreenH) + 96.0f || y + h < -96.0f;
}

// A texture baked once and kept in the Art's sprite cache under `key`.
const Texture& baked(const Art& art, const Renderer& r, const std::string& key, int w, int h, float ax, float ay,
  const std::function<void(cairo_t*)>& paint)
{
  const std::string id = "~garden/" + key;
  auto it = art.styled.find(id);
  if (it != art.styled.end())
    return it->second.facing[0];
  VectorImage img(w, h);
  paint(img.cr());
  Sprite s;
  s.facing[0] = img.toTexture(r, ax, ay);
  return art.styled.emplace(id, std::move(s)).first->second.facing[0];
}

Color lighten(Color c, float t) { return lerpColor(c, rgb(255, 255, 255), t); }
Color darken(Color c, float t) { return lerpColor(c, rgb(0, 0, 0), t); }

void stop(cairo_pattern_t* g, double at, Color c, double a = 1.0)
{
  cairo_pattern_add_color_stop_rgba(g, at, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, a);
}

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
  cairo_scale(cr, std::max(0.01, rx), std::max(0.01, ry));
  cairo_new_sub_path(cr);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_restore(cr);
}

// Fills the path with a linear gradient; ink > 0 strokes it too.
void fillLinear(cairo_t* cr, double x0, double y0, double x1, double y1, std::initializer_list<std::pair<double, Color>> stops,
  double ink = 0.0, Color inkC = kInk)
{
  cairo_pattern_t* g = cairo_pattern_create_linear(x0, y0, x1, y1);
  for (const auto& s : stops)
    stop(g, s.first, s.second, alphaOf(s.second) / 255.0);
  cairo_set_source(cr, g);
  if (ink > 0)
  {
    cairo_fill_preserve(cr);
    setColor(cr, inkC);
    cairo_set_line_width(cr, ink);
    cairo_stroke(cr);
  }
  else
    cairo_fill(cr);
  cairo_pattern_destroy(g);
}

void fillRadial(cairo_t* cr, double x, double y, double r, std::initializer_list<std::pair<double, Color>> stops, double ink = 0.0)
{
  cairo_pattern_t* g = cairo_pattern_create_radial(x - r * 0.35, y - r * 0.4, r * 0.05, x, y, r);
  for (const auto& s : stops)
    stop(g, s.first, s.second, alphaOf(s.second) / 255.0);
  cairo_set_source(cr, g);
  if (ink > 0)
  {
    cairo_fill_preserve(cr);
    setColor(cr, kInk);
    cairo_set_line_width(cr, ink);
    cairo_stroke(cr);
  }
  else
    cairo_fill(cr);
  cairo_pattern_destroy(g);
}

void softGlow(cairo_t* cr, double x, double y, double r, Color c, double a)
{
  cairo_pattern_t* g = cairo_pattern_create_radial(x, y, 0, x, y, r);
  stop(g, 0, c, a);
  stop(g, 0.4, c, a * 0.45);
  stop(g, 1, c, 0.0);
  circle(cr, x, y, r);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
}

void stroke(cairo_t* cr, Color c, double w, double a = 1.0)
{
  setRgba(cr, c, a);
  cairo_set_line_width(cr, w);
  cairo_stroke(cr);
}

// A dew drop: clear, the light pooling at its foot, a sharp highlight.
void dewDrop(cairo_t* cr, double x, double y, double r, Color behind)
{
  circle(cr, x, y, r);
  cairo_pattern_t* g = cairo_pattern_create_radial(x + r * 0.1, y + r * 0.35, r * 0.1, x, y, r);
  stop(g, 0, lighten(behind, 0.75f), 0.9);
  stop(g, 0.6, lighten(behind, 0.3f), 0.5);
  stop(g, 1, darken(behind, 0.35f), 0.8);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
  circle(cr, x, y, r);
  stroke(cr, darken(behind, 0.5f), std::max(0.7, r * 0.08), 0.5);
  ellipse(cr, x - r * 0.38, y - r * 0.42, r * 0.28, r * 0.18, -0.6);
  setRgba(cr, rgb(255, 255, 255), 0.95);
  cairo_fill(cr);
}

// --- Painters: the garden's platforms ----------------------------------------------

struct Bloom
{
  Color light, dark, heart;
};

constexpr Bloom kDayBloom{rgb(255, 246, 250), rgb(236, 118, 172), rgb(255, 200, 40)};
constexpr Bloom kBudBloom{rgb(248, 214, 255), rgb(164, 70, 206), rgb(255, 238, 140)};

// One petal from its base at (0, 0) along +x, `len` long, `wd` wide.
void petal(cairo_t* cr, double len, double wd, const Bloom& b)
{
  cairo_move_to(cr, 0, 0);
  cairo_curve_to(cr, len * 0.22, -wd * 0.62, len * 0.78, -wd * 0.66, len, -wd * 0.1);
  cairo_curve_to(cr, len * 1.03, 0, len * 1.03, 0, len, wd * 0.1);
  cairo_curve_to(cr, len * 0.78, wd * 0.66, len * 0.22, wd * 0.62, 0, 0);
  cairo_close_path(cr);
  fillLinear(cr, 0, 0, len, 0, {{0.0, b.dark}, {0.55, lerpColor(b.dark, b.light, 0.7f)}, {1.0, b.light}}, 1.1,
    darken(b.dark, 0.45f));
  cairo_move_to(cr, len * 0.08, 0);
  cairo_curve_to(cr, len * 0.4, -wd * 0.05, len * 0.6, wd * 0.05, len * 0.86, 0);
  stroke(cr, lighten(b.dark, 0.5f), 0.9, 0.55);
}

// A flower `w` px wide whose floor is the block row's top at y = 40 (the
// canvas is w + 80 by 150, the floor's middle at (w / 2 + 40, 40)). open 0
// is a shut bud standing up, 1 a wide open bloom: twelve petals round a
// ring, the back ones behind its heart, the front ones over it.
void paintFlower(cairo_t* cr, double w, double open, const Bloom& b)
{
  const double cx = w * 0.5 + 40, rimY = 46;
  const double rx = w * 0.13 + (w * 0.4 - w * 0.13) * open, ry = 5 + 7 * open;
  const double len = 58 + (w * 0.3 - 58) * open, wd = 20 + (w * 0.11 - 14) * open;
  // The sepals and the top of the stem.
  cairo_rectangle(cr, cx - 5, rimY + 6, 10, 104);
  fillLinear(cr, cx - 5, 0, cx + 5, 0, {{0.0, rgb(150, 210, 90)}, {1.0, rgb(50, 104, 40)}});
  for (int side = -1; side <= 1; side += 2)
  {
    cairo_save(cr);
    cairo_translate(cr, cx, rimY + 10);
    cairo_rotate(cr, side > 0 ? 0.4 - open * 0.3 : kPi - 0.4 + open * 0.3);
    cairo_move_to(cr, 0, 0);
    cairo_curve_to(cr, 8, -6, 18, -6, 24, -2);
    cairo_curve_to(cr, 18, 4, 8, 4, 0, 0);
    cairo_close_path(cr);
    fillLinear(cr, 0, -6, 0, 4, {{0.0, rgb(140, 200, 80)}, {1.0, rgb(60, 110, 40)}}, 1.0, rgb(30, 60, 20));
    cairo_restore(cr);
  }
  struct P
  {
    double bx, by, ang, s;
  };
  std::vector<P> back, front;
  const int n = 12;
  for (int i = 0; i < n; ++i)
  {
    const double th = i * 2 * kPi / n + 0.13;
    const double bx = cx + std::cos(th) * rx, by = rimY + std::sin(th) * ry;
    // Outward and a little down when open, straight up when shut.
    const double ox = std::cos(th), oy = 0.3 * std::sin(th) + 0.18;
    const double sx = 0.18 * std::cos(th), sy = -1.0;
    const double dx = sx + (ox - sx) * open, dy = sy + (oy - sy) * open;
    const P p{bx, by, std::atan2(dy, dx), 0.9 + 0.1 * std::cos(th * 3)};
    (std::sin(th) < 0 ? back : front).push_back(p);
  }
  auto draw = [&](const P& p, bool shade) {
    cairo_save(cr);
    cairo_translate(cr, p.bx, p.by);
    cairo_rotate(cr, p.ang);
    Bloom bb = b;
    if (shade)
    {
      bb.light = lerpColor(b.light, b.dark, 0.25f);
      bb.dark = darken(b.dark, 0.15f);
    }
    petal(cr, len * p.s, wd, bb);
    cairo_restore(cr);
  };
  for (const auto& p : back)
    draw(p, true);
  if (open > 0.25)
  {
    // The heart: a cushion of florets, pollen on it.
    const double hr = (w * 0.11 + 4) * open;
    ellipse(cr, cx, rimY - 3, hr, hr * 0.45 + 2);
    fillRadial(cr, cx, rimY - 3, hr, {{0.0, lighten(b.heart, 0.6f)}, {0.6, b.heart}, {1.0, darken(b.heart, 0.35f)}}, 1.0);
    for (int i = 0; i < 9; ++i)
    {
      const double a = i * 2.4;
      circle(cr, cx + std::cos(a) * hr * 0.6, rimY - 4 + std::sin(a) * (hr * 0.25), 1.6);
      setRgba(cr, rgb(255, 250, 200), 0.9);
      cairo_fill(cr);
    }
  }
  std::sort(front.begin(), front.end(), [&](const P& a, const P& c) { return a.by < c.by; });
  for (const auto& p : front)
    draw(p, false);
  if (open < 0.3)
  {
    // The shut bud's tip catching the light.
    ellipse(cr, cx - 6, rimY - len * 0.75, 3, 9, 0.1);
    setRgba(cr, rgb(255, 255, 255), 0.35);
    cairo_fill(cr);
  }
}

// An unfurling leaf `w` px wide from its stalk at the canvas's left (x =
// 40), its floor at y = 40; open 0 is a tight fiddlehead curl, 1 flat out.
void paintLeaf(cairo_t* cr, double w, double open)
{
  const double x0 = 40, y = 40;
  const double len = std::max(0.0, (w - 20) * open);
  const Color top = rgb(150, 206, 90), mid = rgb(96, 160, 58), under = rgb(52, 100, 38);
  // The stalk down the bank.
  cairo_move_to(cr, x0 - 4, y + 4);
  cairo_curve_to(cr, x0 - 14, y + 30, x0 - 18, y + 60, x0 - 20, y + 110);
  stroke(cr, under, 9);
  cairo_move_to(cr, x0 - 4, y + 4);
  cairo_curve_to(cr, x0 - 14, y + 30, x0 - 18, y + 60, x0 - 20, y + 110);
  stroke(cr, rgb(130, 190, 80), 4, 0.8);
  if (len > 4)
  {
    // The blade: its top surface a band, toothed underneath.
    cairo_move_to(cr, x0, y);
    cairo_curve_to(cr, x0 + len * 0.3, y - 4, x0 + len * 0.7, y - 4, x0 + len, y + 1);
    cairo_line_to(cr, x0 + len, y + 9);
    const int teeth = std::max(2, int(len / 24));
    for (int i = teeth; i > 0; --i)
    {
      const double xa = x0 + len * (i - 0.5) / teeth, xb = x0 + len * (i - 1) / teeth;
      cairo_line_to(cr, xa, y + 17);
      cairo_line_to(cr, xb, y + 12);
    }
    cairo_close_path(cr);
    fillLinear(cr, 0, y - 4, 0, y + 17, {{0.0, top}, {0.45, mid}, {1.0, under}}, 1.2, rgb(30, 60, 20));
    cairo_move_to(cr, x0, y + 3);
    cairo_line_to(cr, x0 + len, y + 4);
    stroke(cr, rgb(210, 240, 150), 1.6, 0.8);
    for (double vx = x0 + 18; vx < x0 + len - 10; vx += 22)
    {
      cairo_move_to(cr, vx, y + 3.5);
      cairo_line_to(cr, vx + 14, y - 1);
      cairo_move_to(cr, vx + 4, y + 4);
      cairo_line_to(cr, vx + 16, y + 12);
    }
    stroke(cr, rgb(200, 236, 140), 0.9, 0.5);
    cairo_move_to(cr, x0, y - 0.5);
    cairo_curve_to(cr, x0 + len * 0.3, y - 4.5, x0 + len * 0.7, y - 4.5, x0 + len, y + 0.5);
    stroke(cr, rgb(255, 240, 170), 1.4, 0.7);
    if (open > 0.6)
      for (const double u : {0.3, 0.72})
        dewDrop(cr, x0 + len * u, y - 5, 4.5, mid);
  }
  // The curl still rolled up at its tip.
  const double rr = 6 + (1.0 - open) * std::min(34.0, w * 0.07);
  if (open < 0.98)
  {
    const double cx = x0 + len + rr * 0.4, cy = y + 4 - rr * 0.4;
    cairo_new_path(cr);
    for (int i = 0; i <= 40; ++i)
    {
      const double t = double(i) / 40.0;
      const double a = -kPi * 0.5 + t * 3.2 * kPi;
      const double rad = rr * (1.0 - t * 0.78);
      const double px = cx + std::cos(a) * rad, py = cy + std::sin(a) * rad;
      if (i == 0)
        cairo_move_to(cr, px, py);
      else
        cairo_line_to(cr, px, py);
    }
    setRgba(cr, under, 1.0);
    cairo_set_line_width(cr, 7 + rr * 0.2);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_stroke_preserve(cr);
    setRgba(cr, mid, 1.0);
    cairo_set_line_width(cr, 4 + rr * 0.12);
    cairo_stroke(cr);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
  }
}

// A glowing mushroom cap `w` px wide, its top's middle at (w / 2 + 30, 34)
// on a canvas w + 60 by 120: a flat dome, luminous spots, gills under it.
void paintCap(cairo_t* cr, double w)
{
  const double cx = w * 0.5 + 30, y = 34;
  const double hw = w * 0.5 + 10;
  softGlow(cr, cx, y + 22, hw * 0.9, rgb(120, 255, 210), 0.25);
  // The stem's top under the cap.
  cairo_rectangle(cr, cx - 11, y + 18, 22, 70);
  fillLinear(cr, cx - 11, 0, cx + 11, 0, {{0.0, rgb(240, 250, 236)}, {0.6, rgb(196, 214, 200)}, {1.0, rgb(120, 140, 140)}});
  // The gills: a glowing fringe under the rim.
  cairo_move_to(cr, cx - hw, y + 16);
  cairo_curve_to(cr, cx - hw * 0.5, y + 30, cx + hw * 0.5, y + 30, cx + hw, y + 16);
  cairo_close_path(cr);
  fillLinear(cr, 0, y + 14, 0, y + 28, {{0.0, rgb(200, 255, 230)}, {1.0, rgb(90, 200, 180)}});
  for (int i = -8; i <= 8; ++i)
  {
    cairo_move_to(cr, cx + i * hw / 9.0, y + 17);
    cairo_line_to(cr, cx + i * hw / 11.0, y + 24 - std::abs(i) * 0.5);
  }
  stroke(cr, rgb(60, 140, 130), 0.9, 0.6);
  // The cap.
  cairo_move_to(cr, cx - hw, y + 17);
  cairo_curve_to(cr, cx - hw * 1.02, y + 2, cx - hw * 0.65, y - 7, cx, y - 8);
  cairo_curve_to(cr, cx + hw * 0.65, y - 7, cx + hw * 1.02, y + 2, cx + hw, y + 17);
  cairo_curve_to(cr, cx + hw * 0.5, y + 21, cx - hw * 0.5, y + 21, cx - hw, y + 17);
  cairo_close_path(cr);
  fillLinear(cr, 0, y - 8, 0, y + 20, {{0.0, rgb(110, 120, 230)}, {0.5, rgb(64, 70, 170)}, {1.0, rgb(30, 34, 96)}}, 1.4,
    rgb(16, 14, 40));
  // Luminous spots.
  Rng rng(std::uint32_t(w * 13));
  for (int i = 0; i < int(w / 22) + 3; ++i)
  {
    const double u = rng.range(-0.8f, 0.8f), sx = cx + u * hw, sy = y - 3 + std::abs(u) * 10 + rng.range(-2, 3);
    const double sr = rng.range(2.4f, 4.6f);
    softGlow(cr, sx, sy, sr * 3, rgb(150, 255, 220), 0.5);
    ellipse(cr, sx, sy, sr, sr * 0.7);
    setRgba(cr, rgb(220, 255, 240), 0.95);
    cairo_fill(cr);
  }
  // The rim catching the glow.
  cairo_move_to(cr, cx - hw + 2, y + 15);
  cairo_curve_to(cr, cx - hw * 0.5, y + 19, cx + hw * 0.5, y + 19, cx + hw - 2, y + 15);
  stroke(cr, rgb(170, 255, 200), 1.8, 0.8);
  ellipse(cr, cx - hw * 0.35, y - 3, hw * 0.25, 2.4, -0.06);
  setRgba(cr, rgb(220, 230, 255), 0.5);
  cairo_fill(cr);
}

// --- Painters: the garden's things -------------------------------------------------

// A seed lying in the soil, 56 x 32, its middle at (28, 20): a striped
// sunflower seed; split, the shoot's hook comes out of it.
void paintSeed(cairo_t* cr, bool split)
{
  ellipse(cr, 30, 27, 22, 4);
  setRgba(cr, rgb(20, 12, 4), 0.35);
  cairo_fill(cr);
  for (int half = 0; half < (split ? 2 : 1); ++half)
  {
    cairo_save(cr);
    if (split)
    {
      cairo_translate(cr, 28, 24);
      cairo_rotate(cr, half == 0 ? -0.35 : 0.35);
      cairo_translate(cr, half == 0 ? -34 : -22, -24);
    }
    ellipse(cr, 28, 20, 22, 9, -0.12);
    fillLinear(cr, 0, 11, 0, 29, {{0.0, rgb(90, 80, 70)}, {0.5, rgb(46, 40, 36)}, {1.0, rgb(26, 22, 20)}}, 1.4);
    for (int k = -2; k <= 2; ++k)
    {
      cairo_move_to(cr, 8, 20 + k * 2.5);
      cairo_curve_to(cr, 20, 15 + k * 3.2, 36, 15 + k * 3.2, 48, 18 + k * 2.2);
    }
    stroke(cr, rgb(230, 224, 210), 1.1, 0.7);
    ellipse(cr, 22, 15, 8, 2.4, -0.2);
    setRgba(cr, rgb(255, 255, 255), 0.35);
    cairo_fill(cr);
    cairo_restore(cr);
  }
}

// The crown on a grown stalk, 120 x 90, the stalk's top at (60, 80): two
// leaves spread and a small green bud.
void paintCrown(cairo_t* cr)
{
  for (int side = -1; side <= 1; side += 2)
  {
    cairo_save(cr);
    cairo_translate(cr, 60, 74);
    cairo_scale(cr, side, 1);
    cairo_move_to(cr, 0, 0);
    cairo_curve_to(cr, 14, -20, 34, -26, 54, -18);
    cairo_curve_to(cr, 40, -4, 20, 4, 0, 0);
    cairo_close_path(cr);
    fillLinear(cr, 0, -24, 0, 4, {{0.0, rgb(170, 220, 100)}, {1.0, rgb(60, 116, 44)}}, 1.3, rgb(30, 60, 20));
    cairo_move_to(cr, 2, -1);
    cairo_curve_to(cr, 18, -10, 34, -16, 50, -17);
    stroke(cr, rgb(220, 250, 160), 1.0, 0.7);
    cairo_restore(cr);
  }
  cairo_rectangle(cr, 56, 46, 8, 36);
  fillLinear(cr, 56, 0, 64, 0, {{0.0, rgb(170, 220, 100)}, {1.0, rgb(60, 116, 44)}});
  ellipse(cr, 60, 40, 9, 13);
  fillRadial(cr, 60, 40, 13, {{0.0, rgb(200, 240, 140)}, {1.0, rgb(70, 130, 50)}}, 1.3);
}

// The dandelion's flower, 240 x 240, its middle at (120, 120): ray florets
// in rings, deep yellow in the middle, the green bracts under it.
void paintDandelionHead(cairo_t* cr)
{
  const double cx = 120, cy = 120;
  softGlow(cr, cx, cy, 118, rgb(255, 230, 90), 0.35);
  for (int i = 0; i < 14; ++i)
  {
    const double a = kPi * 0.25 + i * kPi * 0.5 / 13;
    cairo_save(cr);
    cairo_translate(cr, cx, cy + 10);
    cairo_rotate(cr, a);
    cairo_move_to(cr, 0, 0);
    cairo_curve_to(cr, 30, -6, 60, -4, 78, 2);
    cairo_curve_to(cr, 60, 6, 30, 6, 0, 0);
    fillLinear(cr, 0, -6, 0, 6, {{0.0, rgb(150, 200, 80)}, {1.0, rgb(60, 110, 40)}}, 1.0, rgb(30, 60, 20));
    cairo_restore(cr);
  }
  for (int ring = 0; ring < 5; ++ring)
  {
    const double rr = 92 - ring * 18;
    const int n = 34 - ring * 5;
    for (int i = 0; i < n; ++i)
    {
      const double a = i * 2 * kPi / n + ring * 0.21;
      cairo_save(cr);
      cairo_translate(cr, cx, cy);
      cairo_rotate(cr, a);
      cairo_move_to(cr, rr * 0.25, -4);
      cairo_line_to(cr, rr - 2, -5.5);
      cairo_line_to(cr, rr, -3);
      cairo_line_to(cr, rr + 1, 0);
      cairo_line_to(cr, rr, 3);
      cairo_line_to(cr, rr - 2, 5.5);
      cairo_line_to(cr, rr * 0.25, 4);
      cairo_close_path(cr);
      const Color c = lerpColor(rgb(255, 236, 90), rgb(250, 170, 20), float(ring) / 4.0f);
      fillLinear(cr, 0, 0, rr, 0, {{0.0, darken(c, 0.15f)}, {1.0, lighten(c, 0.25f)}}, 0.7, darken(c, 0.4f));
      cairo_restore(cr);
    }
  }
  circle(cr, cx, cy, 14);
  fillRadial(cr, cx, cy, 14, {{0.0, rgb(255, 210, 60)}, {1.0, rgb(220, 130, 10)}});
}

// The seed clock, 300 x 300, its middle at (150, 150): a sphere of seeds on
// their beaks, each a little parachute; `missing` seeds gone from its right.
void paintSeedClock(cairo_t* cr, int missing)
{
  const double cx = 150, cy = 150;
  softGlow(cr, cx, cy, 150, rgb(255, 255, 240), 0.35);
  Rng rng(2557u);
  for (int i = 0; i < 90; ++i)
  {
    // Points on a sphere, seen from the front; the back ones fainter.
    const double z = rng.range(-1, 1), a = rng.range(0, float(2 * kPi));
    const double rxy = std::sqrt(1 - z * z);
    const double dx = std::cos(a) * rxy, dy = std::sin(a) * rxy;
    if (missing > 0 && dx > 0.35 && std::abs(dy) < 0.5 && z > -0.2 && i % 4 < missing)
      continue;
    const double depth = 0.45 + 0.55 * (z + 1) * 0.5;
    const double ex = cx + dx * 128, ey = cy + dy * 128, bx = cx + dx * 26, by = cy + dy * 26;
    cairo_move_to(cr, bx, by);
    cairo_line_to(cr, ex, ey);
    stroke(cr, rgb(250, 250, 240), 1.0, 0.55 * depth);
    for (int k = 0; k < 9; ++k)
    {
      const double pa = std::atan2(dy, dx) + (k - 4) * 0.2;
      cairo_move_to(cr, ex, ey);
      cairo_line_to(cr, ex + std::cos(pa) * 18, ey + std::sin(pa) * 18);
    }
    stroke(cr, rgb(255, 255, 255), 0.8, 0.6 * depth);
  }
  circle(cr, cx, cy, 24);
  fillRadial(cr, cx, cy, 24, {{0.0, rgb(240, 236, 220)}, {1.0, rgb(170, 160, 130)}});
  for (int i = 0; i < 30; ++i)
  {
    const double a = rng.range(0, float(2 * kPi)), d = rng.range(4, 22);
    circle(cr, cx + std::cos(a) * d, cy + std::sin(a) * d, 2.2);
    setRgba(cr, rgb(110, 80, 50), 0.8);
    cairo_fill(cr);
  }
}

// The dandelion's leaves, a rosette 1000 x 220 round its foot at (500, 210):
// long leaves toothed like a lion's jaw.
void paintRosette(cairo_t* cr)
{
  struct L
  {
    double ang, len, wd;
  };
  const L leaves[] = {{-2.95, 420, 70}, {-2.6, 360, 64}, {-2.2, 260, 56}, {-0.95, 270, 56}, {-0.5, 360, 64}, {-0.15, 440, 72},
    {-1.6, 180, 50}};
  for (const auto& l : leaves)
  {
    cairo_save(cr);
    cairo_translate(cr, 500, 210);
    cairo_rotate(cr, l.ang);
    cairo_scale(cr, 1.0, 0.55);
    cairo_move_to(cr, 0, 0);
    const int teeth = 6;
    for (int i = 1; i <= teeth; ++i)
    {
      const double u = double(i) / teeth;
      const double half = l.wd * std::sin(kPi * std::min(1.0, u * 0.9 + 0.05)) * 0.5;
      cairo_line_to(cr, l.len * (u - 0.08), -half * 0.45);
      cairo_line_to(cr, l.len * (u - 0.14), -half);
    }
    cairo_line_to(cr, l.len, 0);
    for (int i = teeth; i >= 1; --i)
    {
      const double u = double(i) / teeth;
      const double half = l.wd * std::sin(kPi * std::min(1.0, u * 0.9 + 0.05)) * 0.5;
      cairo_line_to(cr, l.len * (u - 0.14), half);
      cairo_line_to(cr, l.len * (u - 0.08), half * 0.45);
    }
    cairo_close_path(cr);
    fillLinear(cr, 0, -l.wd * 0.5, 0, l.wd * 0.5, {{0.0, rgb(150, 206, 90)}, {0.5, rgb(84, 148, 54)}, {1.0, rgb(44, 92, 34)}},
      2.0, rgb(26, 54, 18));
    cairo_move_to(cr, 0, 0);
    cairo_line_to(cr, l.len * 0.95, 0);
    stroke(cr, rgb(220, 240, 170), 3.0, 0.7);
    cairo_restore(cr);
  }
}

// The seed the runner rides, 360 x 520, its pod's top middle at (180, 470):
// a ribbed brown pod to stand on, its beak rising behind to the pappus, a
// white feathered parachute.
void paintSeedRider(cairo_t* cr)
{
  const double cx = 180, top = 470, beakTop = 150;
  softGlow(cr, cx, beakTop - 10, 170, rgb(255, 255, 240), 0.3);
  // The pappus: fine filaments in a shallow bowl, each feathered.
  for (int i = 0; i < 44; ++i)
  {
    const double a = -kPi + 0.12 + i * (kPi - 0.24) / 43;
    const double ex = cx + std::cos(a) * 160, ey = beakTop + std::sin(a) * 92 + 30;
    cairo_move_to(cr, cx, beakTop + 16);
    cairo_curve_to(cr, cx + std::cos(a) * 60, beakTop + std::sin(a) * 40 + 20, ex - std::cos(a) * 30, ey, ex, ey);
    stroke(cr, rgb(255, 255, 250), 1.3, 0.8);
    for (int k = 1; k < 6; ++k)
    {
      const double u = 0.45 + k * 0.1;
      const double px = cx + (ex - cx) * u, py = beakTop + 16 + (ey - beakTop - 16) * u;
      cairo_move_to(cr, px, py);
      cairo_line_to(cr, px + std::cos(a - 0.6) * 9, py + std::sin(a - 0.6) * 9);
      cairo_move_to(cr, px, py);
      cairo_line_to(cr, px + std::cos(a + 0.6) * 9, py + std::sin(a + 0.6) * 9);
    }
    stroke(cr, rgb(255, 255, 255), 0.7, 0.55);
  }
  // The beak.
  cairo_move_to(cr, cx, top);
  cairo_line_to(cr, cx, beakTop + 16);
  stroke(cr, rgb(120, 100, 70), 4.0);
  cairo_move_to(cr, cx - 0.8, top);
  cairo_line_to(cr, cx - 0.8, beakTop + 16);
  stroke(cr, rgb(240, 230, 200), 1.6);
  // The pod: a spindle 64 wide under the floor line.
  cairo_move_to(cr, cx - 32, top + 6);
  cairo_curve_to(cr, cx - 30, top - 2, cx + 30, top - 2, cx + 32, top + 6);
  cairo_curve_to(cr, cx + 22, top + 26, cx - 22, top + 26, cx - 32, top + 6);
  cairo_close_path(cr);
  fillLinear(cr, 0, top - 2, 0, top + 24, {{0.0, rgb(200, 150, 90)}, {0.5, rgb(140, 90, 50)}, {1.0, rgb(80, 50, 26)}}, 1.6);
  for (int k = -3; k <= 3; ++k)
  {
    cairo_move_to(cr, cx + k * 8, top);
    cairo_curve_to(cr, cx + k * 7, top + 8, cx + k * 5, top + 14, cx + k * 3, top + 20);
  }
  stroke(cr, rgb(90, 56, 30), 1.0, 0.7);
  for (int k = -3; k <= 3; ++k)
  {
    cairo_move_to(cr, cx + k * 8 - 3, top + 3);
    cairo_line_to(cr, cx + k * 8 + 2, top + 1);
  }
  stroke(cr, rgb(240, 210, 160), 1.0, 0.7);
}

// The pond, w x h: murky golden-green water deepening to dark, light
// dancing in it, the bed's pebbles faint below.
void paintPond(cairo_t* cr, double w, double h)
{
  cairo_rectangle(cr, 0, 0, w, h);
  fillLinear(cr, 0, 0, 0, h, {{0.0, rgba(150, 190, 150, 235)}, {0.15, rgba(90, 140, 120, 240)}, {0.6, rgba(46, 86, 76, 250)},
    {1.0, rgb(24, 46, 40)}});
  Rng rng(2563u);
  for (int i = 0; i < int(w / 40); ++i)
  {
    const double x = rng.range(0, float(w)), y = h - rng.range(6, 40);
    ellipse(cr, x, y, rng.range(8, 22), rng.range(4, 9));
    setRgba(cr, rgb(120, 130, 110), 0.25);
    cairo_fill(cr);
  }
  // Caustics: thin bright wavy lines in the top half.
  for (int i = 0; i < int(w / 24); ++i)
  {
    const double x = rng.range(0, float(w)), y = rng.range(14, float(h) * 0.6), l = rng.range(14, 40);
    cairo_move_to(cr, x, y);
    cairo_curve_to(cr, x + l * 0.3, y - 4, x + l * 0.6, y + 4, x + l, y);
    stroke(cr, rgb(230, 255, 210), 1.4, 0.18);
  }
  // Weed fronds swaying up from the bed.
  for (int i = 0; i < int(w / 140); ++i)
  {
    const double x = rng.range(10, float(w) - 10);
    cairo_move_to(cr, x, h);
    cairo_curve_to(cr, x - 12, h * 0.7, x + 14, h * 0.5, x - 4, h * 0.3);
    stroke(cr, rgb(40, 90, 50), 5, 0.6);
  }
  // The surface: a bright skin of reflected sky.
  cairo_rectangle(cr, 0, 0, w, 10);
  fillLinear(cr, 0, 0, 0, 10, {{0.0, rgba(255, 240, 200, 220)}, {1.0, rgba(255, 240, 200, 0)}});
}

// A lily pad, 150 x 50, its middle at (75, 25); `flower` puts a little
// water lily on it.
void paintLilyPad(cairo_t* cr, bool flower)
{
  ellipse(cr, 75, 30, 68, 15);
  setRgba(cr, rgb(10, 30, 20), 0.3);
  cairo_fill(cr);
  cairo_save(cr);
  cairo_translate(cr, 75, 25);
  cairo_scale(cr, 66, 13);
  cairo_new_path(cr);
  cairo_move_to(cr, 0, 0);
  cairo_arc(cr, 0, 0, 1, 0.25, 2 * kPi - 0.05);
  cairo_close_path(cr);
  cairo_restore(cr);
  fillLinear(cr, 0, 12, 0, 38, {{0.0, rgb(150, 210, 90)}, {0.6, rgb(70, 140, 60)}, {1.0, rgb(40, 90, 40)}}, 1.4,
    rgb(20, 50, 20));
  for (int k = 0; k < 9; ++k)
  {
    const double a = 0.5 + k * 0.62;
    cairo_move_to(cr, 75, 25);
    cairo_line_to(cr, 75 + std::cos(a) * 60, 25 + std::sin(a) * 11);
  }
  stroke(cr, rgb(200, 240, 150), 0.9, 0.5);
  ellipse(cr, 52, 20, 20, 3);
  setRgba(cr, rgb(255, 250, 220), 0.35);
  cairo_fill(cr);
  if (flower)
  {
    for (int i = 0; i < 8; ++i)
    {
      const double a = -kPi * 0.5 + (i - 3.5) * 0.32;
      cairo_save(cr);
      cairo_translate(cr, 92, 20);
      cairo_rotate(cr, a);
      ellipse(cr, 12, 0, 13, 4.2);
      fillLinear(cr, 0, 0, 24, 0, {{0.0, rgb(240, 160, 200)}, {1.0, rgb(255, 240, 250)}}, 0.9, rgb(160, 70, 120));
      cairo_restore(cr);
    }
    circle(cr, 92, 18, 4);
    setColor(cr, rgb(255, 210, 60));
    cairo_fill(cr);
  }
}

// A ripple ring, 160 x 40, centred.
void paintRipple(cairo_t* cr)
{
  ellipse(cr, 80, 20, 76, 16);
  stroke(cr, rgb(255, 250, 230), 2.4, 0.8);
  ellipse(cr, 80, 21, 64, 12);
  stroke(cr, rgb(255, 250, 230), 1.2, 0.4);
}

// The rare butterfly, 140 x 110, its body's middle at (70, 55), wings open
// by `spread` (1 flat open, small: folded edge on).
void paintButterfly(cairo_t* cr, double spread)
{
  for (int side = -1; side <= 1; side += 2)
  {
    cairo_save(cr);
    cairo_translate(cr, 70, 55);
    cairo_scale(cr, side * spread, 1);
    // Forewing.
    cairo_move_to(cr, 2, -4);
    cairo_curve_to(cr, 20, -40, 54, -50, 64, -38);
    cairo_curve_to(cr, 68, -20, 52, -2, 4, 2);
    cairo_close_path(cr);
    cairo_pattern_t* g = cairo_pattern_create_linear(0, -10, 60, -40);
    stop(g, 0, rgb(30, 60, 160));
    stop(g, 0.5, rgb(60, 180, 255));
    stop(g, 0.85, rgb(140, 230, 255));
    stop(g, 1, rgb(20, 30, 60));
    cairo_set_source(cr, g);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(g);
    stroke(cr, rgb(10, 14, 30), 2.4);
    // Hindwing with a tail.
    cairo_move_to(cr, 3, 2);
    cairo_curve_to(cr, 40, 0, 52, 16, 44, 30);
    cairo_curve_to(cr, 40, 36, 36, 50, 30, 54);
    cairo_curve_to(cr, 28, 44, 20, 38, 4, 12);
    cairo_close_path(cr);
    cairo_pattern_t* g2 = cairo_pattern_create_linear(0, 0, 40, 40);
    stop(g2, 0, rgb(30, 70, 170));
    stop(g2, 0.6, rgb(70, 190, 250));
    stop(g2, 1, rgb(20, 30, 70));
    cairo_set_source(cr, g2);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(g2);
    stroke(cr, rgb(10, 14, 30), 2.2);
    // The black margins' white dots and an eyespot.
    for (int k = 0; k < 4; ++k)
    {
      circle(cr, 50 + k * 3, -38 + k * 6, 2.0);
      setColor(cr, rgb(255, 255, 255));
      cairo_fill(cr);
    }
    circle(cr, 36, 24, 5);
    setColor(cr, rgb(255, 180, 40));
    cairo_fill(cr);
    circle(cr, 36, 24, 2.4);
    setColor(cr, rgb(20, 14, 20));
    cairo_fill(cr);
    // The sheen.
    ellipse(cr, 30, -22, 16, 5, -0.6);
    setRgba(cr, rgb(220, 255, 255), 0.45);
    cairo_fill(cr);
    cairo_restore(cr);
  }
  // The body and antennae.
  ellipse(cr, 70, 58, 4.5, 22);
  fillLinear(cr, 66, 0, 75, 0, {{0.0, rgb(90, 80, 70)}, {1.0, rgb(20, 16, 14)}}, 1.2);
  circle(cr, 70, 34, 5);
  setColor(cr, rgb(30, 24, 20));
  cairo_fill(cr);
  for (int side = -1; side <= 1; side += 2)
  {
    cairo_move_to(cr, 70, 30);
    cairo_curve_to(cr, 70 + side * 6, 16, 70 + side * 12, 8, 70 + side * 16, 4);
    stroke(cr, rgb(30, 24, 20), 1.6);
    circle(cr, 70 + side * 16, 4, 2.4);
    setColor(cr, rgb(30, 24, 20));
    cairo_fill(cr);
  }
}

// The specimen frame, 200 x 170, its middle at (100, 85): a walnut box
// frame, cream backing, the butterfly pinned, a typed label, glass glare.
void paintSpecimen(cairo_t* cr)
{
  roundedRect(cr, 6, 6, 188, 158, 6);
  fillLinear(cr, 6, 6, 194, 164, {{0.0, rgb(150, 96, 56)}, {0.5, rgb(100, 60, 32)}, {1.0, rgb(64, 36, 20)}}, 2.0);
  cairo_rectangle(cr, 22, 22, 156, 126);
  fillLinear(cr, 0, 22, 0, 148, {{0.0, rgb(250, 244, 226)}, {1.0, rgb(226, 214, 186)}}, 1.4, rgb(70, 40, 20));
  cairo_rectangle(cr, 22, 22, 156, 6);
  setRgba(cr, rgb(0, 0, 0), 0.18);
  cairo_fill(cr);
  cairo_save(cr);
  cairo_translate(cr, 30, 12);
  cairo_scale(cr, 1.0, 0.92);
  paintButterfly(cr, 1.0);
  cairo_restore(cr);
  // The pin.
  circle(cr, 100, 52, 3.4);
  fillRadial(cr, 100, 52, 3.4, {{0.0, rgb(255, 120, 120)}, {1.0, rgb(150, 20, 20)}});
  // The label.
  cairo_rectangle(cr, 62, 126, 76, 16);
  setColor(cr, rgb(255, 255, 250));
  cairo_fill_preserve(cr);
  stroke(cr, rgb(120, 100, 80), 0.8);
  selectGameFont(cr, true);
  cairo_set_font_size(cr, 9);
  cairo_text_extents_t te;
  cairo_text_extents(cr, "MORPHO RARISSIMA", &te);
  cairo_move_to(cr, 100 - te.x_advance / 2, 137);
  setColor(cr, rgb(60, 50, 40));
  cairo_show_text(cr, "MORPHO RARISSIMA");
  // Glass glare.
  cairo_move_to(cr, 30, 22);
  cairo_line_to(cr, 70, 22);
  cairo_line_to(cr, 22, 100);
  cairo_line_to(cr, 22, 60);
  cairo_close_path(cr);
  setRgba(cr, rgb(255, 255, 255), 0.22);
  cairo_fill(cr);
}

// The ladybug, 180 x 130, standing at (90, 126), facing right; her shell's
// middle at (82, 74), 70 x 46 across (the spots go on at run time).
constexpr double kBugCx = 82, kBugCy = 74, kBugRx = 70, kBugRy = 46;

void paintLadybug(cairo_t* cr, bool lifted)
{
  ellipse(cr, 92, 124, 78, 6);
  setRgba(cr, rgb(0, 0, 0), 0.3);
  cairo_fill(cr);
  // Legs.
  for (int k = 0; k < 3; ++k)
  {
    const double hx = 70 + k * 26;
    cairo_move_to(cr, hx, 104);
    cairo_line_to(cr, hx - 6 + k * 6, 114);
    cairo_line_to(cr, hx - 12 + k * 10, 125);
    stroke(cr, rgb(20, 16, 18), 5);
  }
  // The head: black with white cheeks.
  ellipse(cr, 148, 94, 24, 20);
  fillRadial(cr, 148, 94, 24, {{0.0, rgb(90, 86, 96)}, {1.0, rgb(12, 10, 14)}}, 2.0);
  ellipse(cr, 160, 86, 6, 5, 0.3);
  setColor(cr, rgb(255, 255, 255));
  cairo_fill(cr);
  ellipse(cr, 140, 104, 7, 4, -0.2);
  setColor(cr, rgb(255, 255, 255));
  cairo_fill(cr);
  circle(cr, 164, 96, 3.4);
  setColor(cr, rgb(250, 250, 255));
  cairo_fill(cr);
  circle(cr, 165, 96, 1.8);
  setColor(cr, rgb(10, 10, 14));
  cairo_fill(cr);
  for (int k = 0; k < 2; ++k)
  {
    cairo_move_to(cr, 160 + k * 4, 78);
    cairo_curve_to(cr, 166 + k * 4, 66, 172 + k * 3, 62, 176 + k * 2, 58);
    stroke(cr, rgb(20, 16, 18), 2);
  }
  // The shell: glossy red, split down the middle; lifted a touch once she
  // lets you on (the wings' edge showing under it).
  if (lifted)
  {
    ellipse(cr, kBugCx - 8, kBugCy + 8, kBugRx * 0.9, kBugRy * 0.8, -0.15);
    setRgba(cr, rgb(230, 220, 200), 0.75);
    cairo_fill(cr);
  }
  ellipse(cr, kBugCx, kBugCy - (lifted ? 4 : 0), kBugRx, kBugRy, lifted ? -0.08 : 0.0);
  cairo_pattern_t* g = cairo_pattern_create_radial(kBugCx - 26, kBugCy - 26, 6, kBugCx, kBugCy, kBugRx * 1.05);
  stop(g, 0, rgb(255, 150, 120));
  stop(g, 0.35, rgb(230, 40, 30));
  stop(g, 1, rgb(130, 10, 14));
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  stroke(cr, rgb(40, 6, 8), 2.4);
  cairo_move_to(cr, kBugCx + kBugRx * 0.92, kBugCy - 10);
  cairo_curve_to(cr, kBugCx + 20, kBugCy - kBugRy * 0.7, kBugCx - 30, kBugCy - kBugRy * 0.9, kBugCx - kBugRx * 0.95, kBugCy - 8);
  stroke(cr, rgb(60, 6, 8), 2.0, 0.85);
  ellipse(cr, kBugCx - 26, kBugCy - 26, 22, 8, -0.35);
  setRgba(cr, rgb(255, 255, 255), 0.5);
  cairo_fill(cr);
}

void paintSpot(cairo_t* cr)
{
  circle(cr, 10, 10, 7);
  fillRadial(cr, 10, 10, 7, {{0.0, rgb(80, 70, 70)}, {1.0, rgb(8, 6, 8)}});
  circle(cr, 8, 7.5, 1.6);
  setRgba(cr, rgb(255, 255, 255), 0.6);
  cairo_fill(cr);
}

// The seed packet on its stake, 130 x 200, the stake's foot at (65, 196):
// a paper packet with a painted flower, "LOT 42" printed on it, dog-eared.
void paintSeedPacket(cairo_t* cr, const std::string& text)
{
  cairo_rectangle(cr, 60, 70, 10, 128);
  fillLinear(cr, 60, 0, 70, 0, {{0.0, rgb(200, 160, 110)}, {1.0, rgb(120, 86, 50)}}, 1.4);
  cairo_save(cr);
  cairo_translate(cr, 65, 60);
  cairo_rotate(cr, -0.05);
  roundedRect(cr, -52, -54, 104, 116, 3);
  fillLinear(cr, 0, -54, 0, 62, {{0.0, rgb(255, 250, 236)}, {1.0, rgb(232, 220, 190)}}, 1.6, rgb(90, 70, 50));
  // The painted picture: a sky, a flower.
  cairo_rectangle(cr, -44, -46, 88, 58);
  fillLinear(cr, 0, -46, 0, 12, {{0.0, rgb(130, 200, 240)}, {1.0, rgb(220, 240, 200)}});
  cairo_rectangle(cr, -44, 0, 88, 12);
  setColor(cr, rgb(110, 170, 70));
  cairo_fill(cr);
  cairo_move_to(cr, 0, 10);
  cairo_curve_to(cr, 2, -4, -2, -14, 0, -20);
  stroke(cr, rgb(60, 120, 40), 2.4);
  for (int i = 0; i < 10; ++i)
  {
    const double a = i * 2 * kPi / 10;
    ellipse(cr, std::cos(a) * 9, -26 + std::sin(a) * 9, 7, 3.4, a);
    setColor(cr, rgb(250, 150, 60));
    cairo_fill(cr);
  }
  circle(cr, 0, -26, 5);
  setColor(cr, rgb(110, 60, 30));
  cairo_fill(cr);
  // The print.
  selectGameFont(cr);
  cairo_set_font_size(cr, 20);
  cairo_text_extents_t te;
  cairo_text_extents(cr, text.c_str(), &te);
  const double fs = 20 * std::min(1.0, 92.0 / std::max(1.0, te.x_advance));
  cairo_set_font_size(cr, fs);
  cairo_text_extents(cr, text.c_str(), &te);
  cairo_move_to(cr, -te.x_advance / 2, 34);
  setColor(cr, rgb(170, 40, 30));
  cairo_show_text(cr, text.c_str());
  cairo_set_font_size(cr, 8);
  cairo_text_extents(cr, "FINEST SEED", &te);
  cairo_move_to(cr, -te.x_advance / 2, 48);
  setColor(cr, rgb(90, 70, 50));
  cairo_show_text(cr, "FINEST SEED");
  // The dog-ear.
  cairo_move_to(cr, 52, -54);
  cairo_line_to(cr, 38, -54);
  cairo_line_to(cr, 52, -40);
  cairo_close_path(cr);
  setColor(cr, rgb(214, 200, 168));
  cairo_fill_preserve(cr);
  stroke(cr, rgb(90, 70, 50), 1.0);
  cairo_restore(cr);
}

// The Chrono Mister's cone, 220 x 160, its nozzle at (8, 80): mist widening
// and thinning, brighter at its core.
void paintMistCone(cairo_t* cr)
{
  for (int layer = 0; layer < 3; ++layer)
  {
    const double spread = 66 + layer * 6, a = 0.32 - layer * 0.08;
    cairo_move_to(cr, 8, 80 - 6);
    cairo_curve_to(cr, 70, 80 - 18 - layer * 4, 150, 80 - spread, 214, 80 - spread + 6);
    cairo_curve_to(cr, 222, 80 - spread * 0.4, 222, 80 + spread * 0.4, 214, 80 + spread - 6);
    cairo_curve_to(cr, 150, 80 + spread, 70, 80 + 18 + layer * 4, 8, 80 + 6);
    cairo_close_path(cr);
    cairo_pattern_t* g = cairo_pattern_create_linear(8, 0, 220, 0);
    stop(g, 0, rgb(255, 255, 255), a * 1.4);
    stop(g, 0.4, rgb(210, 255, 240), a);
    stop(g, 1, rgb(170, 255, 220), 0.0);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
}

// A four-pointed sparkle, 32 x 32, centred.
void paintSparkle(cairo_t* cr)
{
  softGlow(cr, 16, 16, 14, rgb(255, 255, 255), 0.6);
  cairo_move_to(cr, 16, 1);
  cairo_curve_to(cr, 17, 14, 18, 15, 31, 16);
  cairo_curve_to(cr, 18, 17, 17, 18, 16, 31);
  cairo_curve_to(cr, 15, 18, 14, 17, 1, 16);
  cairo_curve_to(cr, 14, 15, 15, 14, 16, 1);
  cairo_close_path(cr);
  setColor(cr, rgb(255, 255, 255));
  cairo_fill(cr);
}

// A beetle's jet, 260 x 140, coming out of its tail at (250, 70) and
// spraying left: a hot core, boiling puffs round it.
void paintJet(cairo_t* cr)
{
  cairo_move_to(cr, 250, 62);
  cairo_curve_to(cr, 180, 40, 80, 18, 10, 26);
  cairo_curve_to(cr, 0, 70, 0, 70, 10, 114);
  cairo_curve_to(cr, 80, 122, 180, 100, 250, 78);
  cairo_close_path(cr);
  cairo_pattern_t* g = cairo_pattern_create_linear(250, 0, 0, 0);
  stop(g, 0, rgb(255, 255, 230), 0.95);
  stop(g, 0.3, rgb(255, 220, 140), 0.75);
  stop(g, 0.7, rgb(250, 240, 220), 0.4);
  stop(g, 1, rgb(250, 246, 240), 0.0);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
  Rng rng(2579u);
  for (int i = 0; i < 18; ++i)
  {
    const double u = rng.range(0.05f, 0.95f);
    const double x = 250 - u * 230, y = 70 + rng.range(-1, 1) * (8 + u * 40), rr = 6 + u * 16;
    softGlow(cr, x, y, rr, u < 0.35 ? rgb(255, 230, 160) : rgb(255, 252, 245), 0.55 * (1.0 - u * 0.6));
  }
}

// The HUD sundial, 112 x 112, centred: a bronze plate with its hour marks,
// warm by day, moonlit by dusk; the gnomon's shadow goes on at run time.
void paintSundial(cairo_t* cr, bool dusk)
{
  const double c = 56;
  circle(cr, c, c + 2, 52);
  setRgba(cr, rgb(0, 0, 0), 0.35);
  cairo_fill(cr);
  circle(cr, c, c, 50);
  if (dusk)
    fillRadial(cr, c, c, 50, {{0.0, rgb(190, 200, 240)}, {0.6, rgb(110, 120, 170)}, {1.0, rgb(50, 54, 90)}}, 2.0);
  else
    fillRadial(cr, c, c, 50, {{0.0, rgb(255, 238, 180)}, {0.6, rgb(214, 170, 90)}, {1.0, rgb(130, 90, 40)}}, 2.0);
  circle(cr, c, c, 42);
  stroke(cr, dusk ? rgb(40, 44, 80) : rgb(110, 70, 30), 1.4, 0.8);
  for (int i = 0; i < 12; ++i)
  {
    const double a = -kPi / 2 + i * kPi / 6;
    cairo_move_to(cr, c + std::cos(a) * 36, c + std::sin(a) * 36);
    cairo_line_to(cr, c + std::cos(a) * (i % 3 == 0 ? 26 : 31), c + std::sin(a) * (i % 3 == 0 ? 26 : 31));
  }
  stroke(cr, dusk ? rgb(40, 44, 80) : rgb(90, 56, 20), 2.0);
  // A sun or a moon engraved in the middle.
  if (dusk)
  {
    circle(cr, c + 8, c + 14, 9);
    setColor(cr, rgb(240, 240, 255));
    cairo_fill(cr);
    circle(cr, c + 12, c + 11, 8);
    setColor(cr, rgb(110, 120, 170));
    cairo_fill(cr);
  }
  else
  {
    circle(cr, c, c + 16, 6);
    setColor(cr, rgb(255, 210, 80));
    cairo_fill(cr);
    for (int i = 0; i < 8; ++i)
    {
      const double a = i * kPi / 4;
      cairo_move_to(cr, c + std::cos(a) * 8, c + 16 + std::sin(a) * 8);
      cairo_line_to(cr, c + std::cos(a) * 11, c + 16 + std::sin(a) * 11);
    }
    stroke(cr, rgb(255, 210, 80), 1.6);
  }
  ellipse(cr, c - 18, c - 22, 16, 7, -0.6);
  setRgba(cr, rgb(255, 255, 255), 0.35);
  cairo_fill(cr);
}

// The sun racing over in the time-lapse, 220 x 220, centred.
void paintSun(cairo_t* cr)
{
  softGlow(cr, 110, 110, 110, rgb(255, 220, 120), 0.6);
  circle(cr, 110, 110, 40);
  fillRadial(cr, 110, 110, 40, {{0.0, rgb(255, 255, 240)}, {0.7, rgb(255, 230, 140)}, {1.0, rgb(255, 180, 60)}});
}

// The moon, 160 x 160, centred.
void paintMoon(cairo_t* cr)
{
  softGlow(cr, 80, 80, 80, rgb(200, 210, 255), 0.45);
  circle(cr, 80, 80, 32);
  fillRadial(cr, 80, 80, 32, {{0.0, rgb(255, 255, 250)}, {1.0, rgb(190, 196, 220)}});
  const double craters[3][3] = {{70, 72, 7}, {90, 90, 5}, {86, 66, 4}};
  for (const auto& c : craters)
  {
    circle(cr, c[0], c[1], c[2]);
    setRgba(cr, rgb(160, 166, 196), 0.6);
    cairo_fill(cr);
  }
}

// A narrator's line, 1280 x 64, centred on (640, 32): the game's face
// slanted by hand (fontconfig has no bold oblique everywhere), cream with a
// soft dark halo, like a documentary's subtitle.
void paintSubtitle(cairo_t* cr, const std::string& text)
{
  selectGameFont(cr, false);
  const double size = 27.0;
  cairo_matrix_t m;
  cairo_matrix_init(&m, size, 0.0, -0.21 * size, size, 0.0, 0.0);
  cairo_set_font_matrix(cr, &m);
  cairo_text_extents_t ext;
  cairo_text_extents(cr, text.c_str(), &ext);
  const double x = 640.0 - (ext.x_bearing + ext.width / 2.0), y = 32.0 - (ext.y_bearing + ext.height / 2.0);
  cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
  for (const auto& [wdt, al] : std::initializer_list<std::pair<double, double>>{{9.0, 0.16}, {5.0, 0.3}, {2.4, 0.6}})
  {
    cairo_move_to(cr, x, y + 1.5);
    cairo_text_path(cr, text.c_str());
    cairo_set_line_width(cr, wdt);
    setRgba(cr, rgb(12, 8, 2), al);
    cairo_stroke(cr);
  }
  cairo_move_to(cr, x, y);
  cairo_text_path(cr, text.c_str());
  cairo_pattern_t* g = cairo_pattern_create_linear(0, y - size * 0.75, 0, y);
  stop(g, 0, rgb(255, 252, 240));
  stop(g, 1, rgb(250, 230, 196));
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
}

// The subtitle's backing, 1280 x 110: soft dark in the middle, fading out.
void paintBand(cairo_t* cr)
{
  cairo_pattern_t* g = cairo_pattern_create_linear(0, 0, 1280, 0);
  stop(g, 0, rgb(10, 8, 4), 0.0);
  stop(g, 0.25, rgb(10, 8, 4), 0.5);
  stop(g, 0.75, rgb(10, 8, 4), 0.5);
  stop(g, 1, rgb(10, 8, 4), 0.0);
  cairo_rectangle(cr, 0, 10, 1280, 90);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
  cairo_pattern_t* v = cairo_pattern_create_linear(0, 0, 0, 110);
  stop(v, 0, rgb(0, 0, 0), 1.0);
  stop(v, 0.3, rgb(0, 0, 0), 0.0);
  stop(v, 0.7, rgb(0, 0, 0), 0.0);
  stop(v, 1, rgb(0, 0, 0), 1.0);
  cairo_set_operator(cr, CAIRO_OPERATOR_DEST_OUT);
  cairo_rectangle(cr, 0, 0, 1280, 110);
  cairo_set_source(cr, v);
  cairo_fill(cr);
  cairo_pattern_destroy(v);
  cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
}

// A dragonfly, 200 x 90, its body's middle at (100, 45), facing left;
// `beat` 0/1 the wings up or down.
void paintDragonfly(cairo_t* cr, int beat)
{
  // Wings: two pairs, glassy, veined.
  for (int pair = 0; pair < 2; ++pair)
    for (int side = -1; side <= 1; side += 2)
    {
      cairo_save(cr);
      cairo_translate(cr, 92 + pair * 18, 42);
      cairo_rotate(cr, side * (beat ? 0.35 : 1.1) * (side < 0 ? 1 : 1) + (pair ? 0.15 : -0.15) * side);
      ellipse(cr, 0, side * -34, 9, 34);
      fillLinear(cr, -9, 0, 9, 0, {{0.0, rgba(220, 250, 255, 150)}, {1.0, rgba(170, 220, 255, 90)}}, 1.0, rgb(60, 90, 110));
      for (int k = 1; k < 5; ++k)
      {
        cairo_move_to(cr, -6, side * -k * 13);
        cairo_line_to(cr, 6, side * -k * 13 - side * 2);
      }
      stroke(cr, rgb(90, 130, 150), 0.6, 0.6);
      cairo_restore(cr);
    }
  // The body: a long segmented abdomen to the right, thorax, big eyes left.
  for (int k = 0; k < 9; ++k)
  {
    ellipse(cr, 118 + k * 8, 46, 5.5 - k * 0.25, 4.2 - k * 0.15);
    fillLinear(cr, 0, 42, 0, 50, {{0.0, rgb(120, 230, 210)}, {1.0, rgb(20, 110, 120)}}, 0.8, rgb(10, 40, 50));
  }
  ellipse(cr, 98, 45, 15, 9);
  fillRadial(cr, 98, 45, 15, {{0.0, rgb(150, 240, 200)}, {1.0, rgb(30, 120, 110)}}, 1.4);
  circle(cr, 78, 42, 10);
  fillRadial(cr, 78, 42, 10, {{0.0, rgb(160, 230, 255)}, {0.6, rgb(40, 100, 200)}, {1.0, rgb(10, 30, 80)}}, 1.4);
  circle(cr, 74, 38, 2.6);
  setRgba(cr, rgb(255, 255, 255), 0.85);
  cairo_fill(cr);
}

// A puff of dandelion fluff (a drifting seed), 40 x 40, centred.
void paintFluff(cairo_t* cr)
{
  for (int i = 0; i < 14; ++i)
  {
    const double a = -kPi + i * kPi / 13;
    cairo_move_to(cr, 20, 22);
    cairo_line_to(cr, 20 + std::cos(a) * 16, 22 + std::sin(a) * 12);
  }
  stroke(cr, rgb(255, 255, 255), 1.0, 0.85);
  cairo_move_to(cr, 20, 22);
  cairo_line_to(cr, 20, 36);
  stroke(cr, rgb(200, 180, 140), 1.2);
}

// A fence picket's block, 64 x 64 (the top one pointed): weathered planks.
void paintPicket(cairo_t* cr, bool top)
{
  if (top)
  {
    cairo_move_to(cr, 4, 64);
    cairo_line_to(cr, 4, 24);
    cairo_line_to(cr, 32, 2);
    cairo_line_to(cr, 60, 24);
    cairo_line_to(cr, 60, 64);
  }
  else
    cairo_rectangle(cr, 4, -1, 56, 66);
  cairo_close_path(cr);
  fillLinear(cr, 4, 0, 60, 0, {{0.0, rgb(214, 196, 166)}, {0.3, rgb(190, 168, 134)}, {1.0, rgb(124, 104, 80)}}, 2.0, rgb(70, 54, 40));
  for (int k = 0; k < 4; ++k)
  {
    cairo_move_to(cr, 14 + k * 11, top ? 24 : 0);
    cairo_curve_to(cr, 16 + k * 11, 30, 12 + k * 11, 50, 15 + k * 11, 64);
  }
  stroke(cr, rgb(110, 90, 70), 1.0, 0.5);
  if (top)
  {
    cairo_move_to(cr, 6, 24);
    cairo_line_to(cr, 32, 4);
    stroke(cr, rgb(255, 230, 160), 2.0, 0.7);
  }
}

// The soil's back wall at a burrow's open end: dark loam fading out to the
// right (mirrored for the left), 64 x 64.
void paintWallFade(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 0, 64, 64);
  fillLinear(cr, 0, 0, 64, 0, {{0.0, rgba(52, 34, 22, 245)}, {0.55, rgba(52, 34, 22, 150)}, {1.0, rgba(52, 34, 22, 0)}});
}

void paintWallShade(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 0, 64, 40);
  fillLinear(cr, 0, 0, 0, 40, {{0.0, rgba(10, 6, 2, 170)}, {1.0, rgba(10, 6, 2, 0)}});
}

// Dot patterns the ladybug's spots make: G-U-N, then 4-2 (one string per
// row, '#' a spot), and her own seven.
const std::vector<std::pair<float, float>>& spotLayout(int which)
{
  static std::array<std::vector<std::pair<float, float>>, 3> layouts;
  auto& out = layouts[std::size_t(std::clamp(which, 0, 2))];
  if (!out.empty())
    return out;
  if (which == 0)
  {
    out = {{-38, -14}, {-44, 14}, {-14, -26}, {-12, 4}, {14, -22}, {20, 12}, {42, -6}};
    return out;
  }
  const std::vector<std::vector<const char*>> glyphs = which == 1
    ? std::vector<std::vector<const char*>>{{"###", "#..", "#.#", "#.#", "###"}, {"#.#", "#.#", "#.#", "#.#", "###"},
        {"#..#", "##.#", "#.##", "#..#", "#..#"}}
    : std::vector<std::vector<const char*>>{{"#.#", "#.#", "###", "..#", "..#"}, {"...", "...", "###", "...", "..."},
        {"###", "..#", "###", "#..", "###"}};
  const float step = 8.0f, gap = 9.0f;
  float width = 0.0f;
  for (const auto& g : glyphs)
    width += float(std::string(g[0]).size() - 1) * step + gap + step;
  width -= gap + step;
  float x0 = -width * 0.5f;
  for (const auto& g : glyphs)
  {
    const int cols = int(std::string(g[0]).size());
    for (int row = 0; row < 5; ++row)
      for (int col = 0; col < cols; ++col)
        if (g[std::size_t(row)][col] == '#')
          out.emplace_back(x0 + float(col) * step, -20.0f + float(row) * step);
    x0 += float(cols - 1) * step + gap + step;
  }
  return out;
}

// When the ladybug last changed her spots (drawing is const, so this
// remembers it here): the World it belongs to, what she showed, the frame.
struct LadyMemo
{
  const void* world = nullptr;
  int shown = 0, from = 0;
  int since = -1000;
};
LadyMemo gLady;

} // namespace

// --- The burrows' back wall ----------------------------------------------------------

void World::drawGardenWalls(Renderer& r, float camX, float camY, int frame) const
{
  (void)frame;
  if (mGlide.on)
    return;
  const int H = mLevel->height;
  const auto open = [&](int x, int y) {
    const Tile t = mMap.block(x, y);
    return t == Tile::Empty || t == Tile::Ladder || t == Tile::Platform;
  };
  const auto solidIn = [&](int x, int y0, int y1) {
    for (int y = std::max(0, y0); y <= std::min(H - 1, y1); ++y)
      if (mMap.block(x, y) == Tile::Solid)
        return true;
    return false;
  };
  // A pocket in the soil: open, with soil under it, and soil over it or
  // on both sides at its height (the pantry, the stem, the burrow).
  const auto pocket = [&](int x, int y) {
    if (!open(x, y) || !solidIn(x, y + 1, y + 6))
      return false;
    if (solidIn(x, y - 5, y - 1))
      return true;
    bool left = false, right = false;
    for (int d = 1; d <= 3; ++d)
    {
      left = left || solidIn(x - d, y - 5, y);
      right = right || solidIn(x + d, y - 5, y);
    }
    return left && right;
  };
  const int tx0 = std::max(0, int(camX / kTilePx) - 1), tx1 = std::min(mLevel->width - 1, int((camX + kScreenW) / kTilePx) + 1);
  const int ty0 = std::max(0, int(camY / kTilePx) - 1), ty1 = std::min(H - 1, int((camY + kScreenH) / kTilePx) + 1);
  const Texture& fadeR = baked(mArt, r, "wallfade", 64, 64, 0.0f, 0.0f, [](cairo_t* cr) { paintWallFade(cr); });
  const Texture& fadeL = baked(mArt, r, "wallfade_l", 64, 64, 0.0f, 0.0f, [](cairo_t* cr) {
    cairo_translate(cr, 64, 0);
    cairo_scale(cr, -1, 1);
    paintWallFade(cr);
  });
  const Texture& shade = baked(mArt, r, "wallshade", 64, 40, 0.0f, 0.0f, [](cairo_t* cr) { paintWallShade(cr); });
  for (int ty = ty0; ty <= ty1; ++ty)
    for (int tx = tx0; tx <= tx1; ++tx)
    {
      if (!pocket(tx, ty))
        continue;
      const float x = float(tx) * kTilePx - camX, y = float(ty) * kTilePx - camY;
      const bool openL = open(tx - 1, ty) && !pocket(tx - 1, ty), openR = open(tx + 1, ty) && !pocket(tx + 1, ty);
      if (openL || openR)
        r.draw(openL ? fadeL : fadeR, x, y);
      else
      {
        DrawOpts o;
        o.tint = rgb(118, 96, 84);
        r.draw(mArt.solid[std::size_t(hash2(tx, ty) % 3u == 0u ? 1 : 0)], x, y, o);
      }
      if (mMap.block(tx, ty - 1) == Tile::Solid)
        r.draw(shade, x, y);
    }
}

// --- Over the tiles ------------------------------------------------------------------

void World::drawGardenBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& g = mGarden;
  const int clock = gardenClock();
  const bool dusk = gardenDusk();
  const int inHalf = clock % kGardenHalf;
  const int H = mLevel->height;
  // Where a stem coming down from block (bx, by) meets the soil or the pond (px, world).
  const auto groundBelow = [&](int bx, int by) {
    for (int y = std::max(0, by); y < H; ++y)
    {
      if (mMap.block(bx, y) == Tile::Solid)
        return float(y) * kTilePx;
      for (const auto& pd : g.ponds)
        if (bx * kCellsPerTile >= pd.x0 && bx * kCellsPerTile <= pd.x1 && y * kCellsPerTile >= pd.y0 &&
            y * kCellsPerTile <= pd.y1)
          return float(pd.y0) * kCellPx + 6.0f;
    }
    return float(H) * kTilePx;
  };
  // A stem from y0 down to y1 (screen px) round x: crisp vertical strips.
  const auto stem = [&](float x, float y0, float y1, float half, Color light, Color dark) {
    if (y1 <= y0)
      return;
    r.fillRect(x - half - 1.5f, y0, half * 2.0f + 3.0f, y1 - y0, darken(dark, 0.45f));
    r.fillRect(x - half, y0, half * 0.7f, y1 - y0, light);
    r.fillRect(x - half * 0.3f, y0, half * 0.6f, y1 - y0, lerpColor(light, dark, 0.5f));
    r.fillRect(x + half * 0.3f, y0, half * 0.7f, y1 - y0, dark);
    r.fillRect(x - half + 1.0f, y0, 1.5f, y1 - y0, withAlpha(rgb(255, 240, 170), 140));
  };

  // The pond: the water, ripples spreading, lily pads, the shimmer.
  for (const auto& pd : g.ponds)
  {
    const float x = float(pd.x0) * kCellPx - camX, y = float(pd.y0) * kCellPx - camY;
    const int w = (pd.x1 - pd.x0 + 1) * int(kCellPx), h = (pd.y1 - pd.y0 + 1) * int(kCellPx);
    if (offScreen(x, y - 40.0f, float(w), float(h) + 40.0f))
      continue;
    r.draw(baked(mArt, r, "pond/" + std::to_string(w) + "x" + std::to_string(h), w, h, 0.0f, 0.0f,
             [&](cairo_t* cr) { paintPond(cr, w, h); }),
      x, y);
    const Texture& ripple = baked(mArt, r, "ripple", 160, 40, 80.0f, 20.0f, [](cairo_t* cr) { paintRipple(cr); });
    for (int i = 0; i < w / 260; ++i)
    {
      const float period = 110.0f + float(i % 3) * 23.0f;
      const float t = std::fmod(float(frame) + alpha + float(i) * 37.0f, period) / period;
      const float rx = x + float((hash2(i, pd.x0) % unsigned(w)));
      DrawOpts o;
      o.scale = 0.25f + t * 0.9f;
      o.alpha = 0.6f * (1.0f - t);
      r.draw(ripple, rx, y + 8.0f, o);
    }
    for (int i = 0; i < w / 220; ++i)
    {
      const unsigned hh = hash2(i * 7, pd.y0);
      const float px = x + 60.0f + float(i) * 220.0f + float(hh % 80u);
      if (px > x + float(w) - 80.0f)
        continue;
      const float bob = std::sin(float(frame) * 0.04f + float(i) * 1.7f) * 2.0f;
      r.draw(baked(mArt, r, i % 3 == 1 ? "lilypad_f" : "lilypad", 150, 50, 75.0f, 25.0f,
               [i](cairo_t* cr) { paintLilyPad(cr, i % 3 == 1); }),
        px, y + 6.0f + bob);
    }
    for (int i = 0; i < w / 90; ++i)
    {
      const float sx = x + float((hash2(i, 91) % unsigned(w))) + std::sin(float(frame) * 0.03f + float(i)) * 20.0f;
      const float a = 0.5f + 0.5f * std::sin(float(frame) * 0.08f + float(i) * 2.1f);
      r.fillRect(sx, y + 3.0f + float(i % 3) * 3.0f, 26.0f, 2.0f, withAlpha(rgb(255, 250, 220), int(150.0f * a)));
    }
  }

  // The LOT 42 seed packet on its stake.
  for (const auto& pr : mProps)
  {
    if (pr.kind != PropKind::Deco42)
      continue;
    const int bx = (pr.x + pr.w / 2) / kCellsPerTile, by = pr.y / kCellsPerTile;
    const float x = float(pr.x + pr.w / 2) * kCellPx - camX, gy = groundBelow(bx, by) - camY;
    if (offScreen(x - 70.0f, gy - 200.0f, 140.0f, 200.0f))
      continue;
    DrawOpts o;
    o.angle = std::sin(float(frame) * 0.03f) * 1.2f;
    r.draw(baked(mArt, r, "packet/" + pr.text, 130, 200, 65.0f, 196.0f, [&](cairo_t* cr) { paintSeedPacket(cr, pr.text); }),
      x, gy + 4.0f, o);
  }

  // The day's flowers and leaves, the dusk's mushrooms.
  for (const auto& l : mLayers)
  {
    const bool isDay = l.id.rfind("day_", 0) == 0, isDusk = l.id.rfind("dusk_", 0) == 0;
    if (!g.cycle || (!isDay && !isDusk))
      continue;
    const int W = (l.x1 - l.x0 + 1) * int(kTilePx);
    const float x = float(l.x0) * kTilePx - camX, y = float(l.y0) * kTilePx - camY;
    if (offScreen(x - 80.0f, y - 160.0f, float(W) + 160.0f, 600.0f))
      continue;
    const bool mine = isDay ? !dusk : dusk;
    float open = 0.0f, wilt = 0.0f;
    if (l.solid)
    {
      const float bloom = mine ? std::min(1.0f, (float(inHalf) + alpha) / 14.0f) : 1.0f;
      if (mine && inHalf >= kGardenHalf - kGardenDroop)
        wilt = std::min(1.0f, (float(inHalf - (kGardenHalf - kGardenDroop)) + alpha) / float(kGardenDroop));
      open = bloom * (1.0f - 0.6f * wilt);
    }
    else if (mine)
      open = 0.2f + 0.05f * std::sin(float(frame) * 0.3f); // waiting for the runner to step out of it
    const float ground = groundBelow((l.x0 + l.x1) / 2, l.y0 + 1) - camY;
    if (isDay && W >= 6 * int(kTilePx))
    {
      const int stage = std::clamp(int(std::lround(open * kFlowerStages)), 0, kFlowerStages);
      DrawOpts o;
      o.tint = lerpColor(rgb(255, 255, 255), rgb(226, 196, 150), wilt);
      r.draw(baked(mArt, r, "leaf/" + std::to_string(W) + "/" + std::to_string(stage), W + 80, 160, 40.0f, 40.0f,
               [W, stage](cairo_t* cr) { paintLeaf(cr, W, double(stage) / kFlowerStages); }),
        x, y + wilt * std::sin(float(frame) * 0.8f) * 2.0f, o);
    }
    else if (isDay)
    {
      const int stage = std::clamp(int(std::lround(open * kFlowerStages)), 0, kFlowerStages);
      const float cx = x + float(W) * 0.5f;
      stem(cx, y + 100.0f, ground, 5.0f, rgb(130, 196, 80), rgb(56, 108, 42));
      DrawOpts o;
      o.tint = l.solid ? lerpColor(rgb(255, 255, 255), rgb(230, 190, 160), wilt) : rgb(200, 200, 210);
      o.angle = wilt * (6.0f + std::sin(float(frame) * 0.9f) * 1.5f);
      r.draw(baked(mArt, r, "flower/d/" + std::to_string(W) + "/" + std::to_string(stage), W + 80, 150, float(W + 80) * 0.5f,
               40.0f, [W, stage](cairo_t* cr) { paintFlower(cr, W, double(stage) / kFlowerStages, kDayBloom); }),
        cx, y, o);
    }
    else
    {
      const Texture& cap = baked(mArt, r, "cap/" + std::to_string(W), W + 60, 120, float(W + 60) * 0.5f, 34.0f,
        [W](cairo_t* cr) { paintCap(cr, W); });
      const float cx = x + float(W) * 0.5f;
      if (l.solid)
      {
        // A cap at full size, shrinking as it is about to go.
        const float sc = 0.35f + 0.65f * open;
        stem(cx, y + 18.0f * sc, ground, 10.0f * sc + 2.0f, rgb(236, 246, 232), rgb(150, 170, 166));
        DrawOpts o;
        o.scale = sc;
        o.tint = lerpColor(rgb(255, 255, 255), rgb(200, 190, 210), wilt);
        if (wilt > 0.0f && (frame / 3) % 2 == 0)
          o.alpha = 1.0f - 0.25f * wilt;
        r.draw(cap, cx, y, o);
      }
      else
      {
        // A button pushing up out of the soil (or the water), waiting.
        const float sc = 0.28f + (mine ? 0.1f : 0.0f);
        stem(cx, ground - 20.0f, ground, 4.0f, rgb(236, 246, 232), rgb(150, 170, 166));
        DrawOpts o;
        o.scale = sc;
        o.alpha = 0.9f;
        r.draw(cap, cx, ground - 26.0f, o);
      }
    }
  }

  // Buds: a shut bulb; misted, an open bloom to stand on, closing again.
  for (const auto& b : g.buds)
  {
    const int W = b.w * int(kTilePx);
    const float x = float(b.x) * kTilePx - camX, y = float(b.y) * kTilePx - camY;
    if (offScreen(x - 80.0f, y - 120.0f, float(W) + 160.0f, 600.0f))
      continue;
    float open = 0.0f;
    if (b.open > 0)
      open = std::min(1.0f, (float(kBudOpen - b.open) + alpha) / 8.0f) * std::min(1.0f, float(b.open) / 30.0f);
    const int stage = std::clamp(int(std::lround(open * kFlowerStages)), 0, kFlowerStages);
    const float cx = x + float(W) * 0.5f;
    stem(cx, y + 100.0f, groundBelow(b.x + b.w / 2, b.y + 1) - camY, 5.0f, rgb(150, 190, 90), rgb(66, 110, 50));
    DrawOpts o;
    if (b.open > 0 && b.open < 30 && (frame / 4) % 2)
      o.tint = rgb(255, 220, 230); // about to shut
    else if (b.open == 0)
      o.angle = std::sin(float(frame) * 0.05f + float(b.x)) * 3.0f;
    r.draw(baked(mArt, r, "flower/b/" + std::to_string(W) + "/" + std::to_string(stage), W + 80, 150, float(W + 80) * 0.5f, 40.0f,
             [W, stage](cairo_t* cr) { paintFlower(cr, W, double(stage) / kFlowerStages, kBudBloom); }),
      cx, y, o);
  }

  // Seeds: one in the soil; misted, it splits and the stalk shoots up (the
  // stalk is the climbable, the tiles' own); grown, a crown of leaves on top.
  for (const auto& s : g.seeds)
  {
    const float cx = float(s.x) * kTilePx + 16.0f - camX, foot = float(s.y + 1) * kTilePx - camY;
    const float top = float(s.y - s.h + 1) * kTilePx - camY;
    if (offScreen(cx - 80.0f, top - 100.0f, 160.0f, foot - top + 140.0f))
      continue;
    const bool split = s.grown || s.sprout > 0;
    r.draw(baked(mArt, r, split ? "seed_split" : "seed", 56, 32, 28.0f, 24.0f, [split](cairo_t* cr) { paintSeed(cr, split); }),
      cx, foot - 2.0f);
    const Texture& crown = baked(mArt, r, "crown", 120, 90, 60.0f, 80.0f, [](cairo_t* cr) { paintCrown(cr); });
    if (s.grown)
      r.draw(crown, cx, top + 6.0f);
    else if (s.sprout > 0)
    {
      const float p = std::min(1.0f, (float(kSeedSprout - s.sprout) + alpha) / float(kSeedSprout));
      const float hgt = p * float(s.h) * kTilePx;
      for (float k = 0.0f; k < hgt; k += kTilePx)
        r.draw(mArt.ladder, cx - 16.0f, foot - k - kTilePx);
      DrawOpts o;
      o.scale = 0.5f + 0.5f * p;
      r.draw(crown, cx, foot - std::ceil(hgt / kTilePx) * kTilePx + 6.0f, o);
    }
    else
      drawGlow(r, mArt, cx, foot - 14.0f, 26.0f, rgb(255, 240, 160), 0.25f + 0.15f * std::sin(float(frame) * 0.1f));
  }

  // The dandelion: its leaves, its stalk, its head (yellow, going to seed, a clock).
  if (g.dandelionX >= 0)
  {
    const float x = (float(g.dandelionX) + 0.5f) * kTilePx - camX;
    const float foot = float(g.dandelionY + 1) * kTilePx - camY, head = 10.0f * kTilePx + 32.0f - camY;
    if (!offScreen(x - 520.0f, head - 160.0f, 1040.0f, foot - head + 200.0f))
    {
      stem(x, head + 20.0f, foot, 8.0f, rgb(170, 210, 120), rgb(90, 120, 60));
      r.fillRect(x - 8.0f, foot - 120.0f, 16.0f, 120.0f, withAlpha(rgb(170, 70, 60), 70)); // reddish at its foot
      r.draw(baked(mArt, r, "rosette", 1000, 220, 500.0f, 210.0f, [](cairo_t* cr) { paintRosette(cr); }), x, foot + 2.0f);
      const float sway = std::sin(float(frame) * 0.02f) * 2.0f;
      const Texture& flower = baked(mArt, r, "dandelion", 240, 240, 120.0f, 120.0f, [](cairo_t* cr) { paintDandelionHead(cr); });
      const bool gone = g.seedhead >= 0 && mPlatforms[std::size_t(g.seedhead)].y < (g.dandelionY - 2) * kCellsPerTile;
      const Texture& clockTex = baked(mArt, r, gone ? "clock_gone" : "clock", 300, 300, 150.0f, 150.0f,
        [gone](cairo_t* cr) { paintSeedClock(cr, gone ? 3 : 0); });
      DrawOpts o;
      o.angle = sway;
      if (!g.seeded && g.seeding == 0)
        r.draw(flower, x, head, o);
      else if (!g.seeded)
      {
        // Going to seed: the flower closes and fades, the clock swells.
        const float t = 1.0f - (float(g.seeding) - alpha) / float(kDandelionSeed);
        DrawOpts f = o;
        f.alpha = std::clamp(1.0f - t, 0.0f, 1.0f);
        f.scale = 1.0f - 0.5f * t;
        r.draw(flower, x, head, f);
        DrawOpts c = o;
        c.alpha = std::clamp(t, 0.0f, 1.0f);
        c.scale = 0.4f + 0.6f * t;
        r.draw(clockTex, x, head, c);
      }
      else
      {
        r.draw(clockTex, x, head, o);
        // A few seeds letting go on the breeze.
        const Texture& fluff = baked(mArt, r, "fluff", 40, 40, 20.0f, 22.0f, [](cairo_t* cr) { paintFluff(cr); });
        for (int i = 0; i < 6; ++i)
        {
          const float t = std::fmod(float(frame) * 0.004f + float(i) / 6.0f, 1.0f);
          DrawOpts fo;
          fo.alpha = std::sin(t * 3.14159f) * 0.9f;
          fo.angle = std::sin(float(frame) * 0.05f + float(i)) * 15.0f;
          r.draw(fluff, x + 60.0f + t * 900.0f, head - 40.0f - t * 260.0f + std::sin(t * 12.0f + float(i)) * 30.0f, fo);
        }
      }
    }
  }

  // The seed head the runner rides.
  if (g.seedhead >= 0)
  {
    const auto& pl = mPlatforms[std::size_t(g.seedhead)];
    if (!pl.hidden)
    {
      const float px = (float(pl.prevX) + float(pl.x - pl.prevX) * alpha + float(pl.w) * 0.5f) * kCellPx - camX;
      const float py = (float(pl.prevY) + float(pl.y - pl.prevY) * alpha) * kCellPx - camY;
      if (!offScreen(px - 180.0f, py - 470.0f, 360.0f, 520.0f))
      {
        DrawOpts o;
        o.angle = std::sin(float(frame) * 0.05f) * 2.5f;
        r.draw(baked(mArt, r, "rider", 360, 520, 180.0f, 470.0f, [](cairo_t* cr) { paintSeedRider(cr); }), px, py, o);
      }
    }
  }

  // The ladybug, her spots moving into letters when she is misted.
  if (g.ladybugX >= 0)
  {
    const float cx = (float(g.ladybugX) + 1.0f) * kTilePx - camX, gy = float(g.ladybugY + 3) * kTilePx - camY;
    if (!offScreen(cx - 100.0f, gy - 140.0f, 200.0f, 150.0f))
    {
      if (gLady.world != this || g.ladybug < gLady.shown)
        gLady = {this, g.ladybug, g.ladybug, -1000};
      if (g.ladybug != gLady.shown)
      {
        gLady.from = gLady.shown;
        gLady.shown = g.ladybug;
        gLady.since = frame;
      }
      const bool lifted = g.ladybug >= 1;
      if (lifted)
        drawGlow(r, mArt, cx, gy - 60.0f, 110.0f, rgb(255, 230, 140), 0.25f + 0.1f * std::sin(float(frame) * 0.1f));
      const float bob = std::sin(float(frame) * 0.06f) * 1.0f;
      r.draw(baked(mArt, r, lifted ? "ladybug_up" : "ladybug", 180, 130, 90.0f, 126.0f,
               [lifted](cairo_t* cr) { paintLadybug(cr, lifted); }),
        cx, gy + 2.0f + bob);
      const Texture& spot = baked(mArt, r, "spot", 20, 20, 10.0f, 10.0f, [](cairo_t* cr) { paintSpot(cr); });
      const float t = std::clamp(float(frame - gLady.since) / 30.0f, 0.0f, 1.0f);
      const float e = t * t * (3.0f - 2.0f * t);
      const auto& A = spotLayout(gLady.from);
      const auto& B = spotLayout(gLady.shown);
      const std::size_t n = std::max(A.size(), B.size());
      const float ox = cx + float(kBugCx) - 90.0f, oy = gy + 2.0f + bob - 126.0f + float(kBugCy) - (lifted ? 4.0f : 0.0f);
      for (std::size_t i = 0; i < n; ++i)
      {
        const auto& a = A[i % A.size()];
        const auto& b2 = B[i % B.size()];
        const float sx = a.first + (b2.first - a.first) * e;
        const float sy = a.second + (b2.second - a.second) * e - std::sin(t * 3.14159f) * 8.0f;
        DrawOpts so;
        so.scale = gLady.shown == 0 ? 1.15f : 0.62f;
        r.draw(spot, ox + sx, oy + sy, so);
      }
    }
  }

  // The rare butterfly, caught: pinned in its specimen frame where it was.
  if (g.butterfly && g.bflyState == 1)
  {
    const float x = g.bx * kCellPx - camX, y = g.by * kCellPx - camY;
    if (!offScreen(x - 100.0f, y - 90.0f, 200.0f, 180.0f))
    {
      drawGlow(r, mArt, x, y, 140.0f, rgb(255, 240, 180), 0.3f);
      r.draw(baked(mArt, r, "specimen", 200, 170, 100.0f, 85.0f, [](cairo_t* cr) { paintSpecimen(cr); }), x,
        y + std::sin(float(frame) * 0.05f) * 3.0f);
    }
  }
}

// --- In front ----------------------------------------------------------------------------

void World::drawGardenFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& g = mGarden;
  const int clock = gardenClock();
  const bool dusk = gardenDusk();
  const int inHalf = clock % kGardenHalf;

  // The rare butterfly fluttering (or flying off for good).
  if (g.butterfly && g.bflyState != 1)
  {
    const float x = g.bx * kCellPx - camX, y = g.by * kCellPx - camY;
    if (!offScreen(x - 70.0f, y - 60.0f, 140.0f, 120.0f))
    {
      const float flap = std::abs(std::sin(float(frame) * 0.45f));
      const int st = std::clamp(int(flap * 4.0f), 0, 3);
      static constexpr double kSpread[4] = {0.16, 0.42, 0.72, 1.0};
      DrawOpts o;
      o.scale = 0.75f;
      o.angle = std::sin(float(frame) * 0.07f) * 12.0f;
      if (g.bflyState == 0)
        for (int k = 0; k < 3; ++k)
        {
          const float tx = x - float(k + 1) * 16.0f + std::sin(float(frame) * 0.3f + float(k)) * 6.0f;
          drawGlow(r, mArt, tx, y + float(k) * 6.0f, 10.0f, rgb(150, 230, 255), 0.4f - float(k) * 0.1f);
        }
      r.draw(baked(mArt, r, "bfly/" + std::to_string(st), 140, 110, 70.0f, 55.0f,
               [st](cairo_t* cr) { paintButterfly(cr, kSpread[st]); }),
        x, y, o);
    }
  }

  // The Chrono Mister's mist: a cone of fine spray, time sparkling in it.
  if (g.mist > 0)
  {
    const CellBox c = mistBox();
    const float ax = float(g.mistFace > 0 ? c.x : c.x + c.w) * kCellPx - camX;
    const float ay = (float(c.y) + float(c.h) * 0.5f) * kCellPx - camY;
    const float ramp = std::min(1.0f, float(g.mist) / 6.0f);
    DrawOpts o;
    o.blend = Blend::Add;
    o.tint = rgb(190, 255, 230);
    o.alpha = (0.75f + 0.15f * std::sin(float(frame) * 0.9f)) * ramp;
    o.scale = 0.92f * (0.6f + 0.4f * ramp);
    if (g.mistFace < 0)
      o.angle = 180.0f;
    r.draw(baked(mArt, r, "mist", 220, 160, 8.0f, 80.0f, [](cairo_t* cr) { paintMistCone(cr); }), ax, ay, o);
    const Texture& spark = baked(mArt, r, "sparkle", 32, 32, 16.0f, 16.0f, [](cairo_t* cr) { paintSparkle(cr); });
    for (int i = 0; i < 14; ++i)
    {
      const float t = std::fmod(float(frame) * 0.045f + float(i) / 14.0f, 1.0f);
      const float d = t * 190.0f * ramp, lat = std::sin(float(i) * 2.3f) * t * 58.0f;
      DrawOpts so;
      so.blend = Blend::Add;
      so.tint = i % 3 == 0 ? rgb(255, 240, 160) : rgb(200, 255, 240);
      so.alpha = std::sin(t * 3.14159f) * (0.6f + 0.4f * std::sin(float(frame) * 0.6f + float(i)));
      so.scale = 0.35f + 0.45f * (1.0f - t);
      so.angle = float((frame * 7 + i * 40) % 360);
      r.draw(spark, ax + float(g.mistFace) * d, ay + lat, so);
    }
    drawGlow(r, mArt, ax, ay, 26.0f, rgb(220, 255, 240), 0.6f * ramp);
  }

  // Bombardier Beetles: the abdomen's heat on the tell, then the boiling jet.
  const Texture& jet = baked(mArt, r, "jet", 260, 140, 250.0f, 70.0f, [](cairo_t* cr) { paintJet(cr); });
  for (const auto& e : mEnemies)
  {
    if (!e.alive || e.hidden || e.kind != EnemyKind::BombardierBeetle)
      continue;
    const EnemyDef& def = enemyDef(e.def);
    const float left = e.drawX * kCellPx - camX, bottom = (e.drawY + 1.0f) * kCellPx - camY;
    const float w = float(e.w) * kCellPx;
    const float tx = e.dir > 0 ? left + 16.0f : left + w - 16.0f, ty = bottom - 26.0f;
    if (offScreen(tx - 260.0f, ty - 80.0f, 520.0f, 160.0f))
      continue;
    if (e.attach == 1)
    {
      const float u = float(def.tell - e.tell) / float(std::max(1, def.tell));
      drawGlow(r, mArt, tx, ty, 30.0f + u * 40.0f, rgb(255, 150, 50), 0.4f + 0.4f * u * float((frame / 2) % 2 ? 1 : 0.7f));
    }
    else if (e.cool > def.cooldown - 8)
    {
      const float k = float(e.cool - (def.cooldown - 8)) / 8.0f;
      DrawOpts o;
      o.alpha = k;
      o.scale = 0.8f + (1.0f - k) * 0.35f;
      if (e.dir < 0)
        o.angle = 180.0f;
      r.draw(jet, tx, ty, o);
      drawGlow(r, mArt, tx - float(e.dir) * 60.0f, ty, 90.0f, rgb(255, 200, 120), 0.5f * k);
      for (int i = 0; i < 5; ++i)
      {
        const float u = float(i) / 5.0f + (1.0f - k) * 0.3f;
        drawGlow(r, mArt, tx - float(e.dir) * (40.0f + u * 160.0f), ty + std::sin(float(i * 5 + frame)) * (8.0f + u * 30.0f),
          20.0f + u * 24.0f, rgb(255, 250, 240), 0.35f * k);
      }
    }
  }

  // Soldier Ants: the column's alarm, a glint on every head.
  for (const auto& c : g.columns)
    if (c.tell > 0)
      for (int a : c.ants)
      {
        const Enemy& e = mEnemies[std::size_t(a)];
        if (!e.alive || e.hidden)
          continue;
        const float hx = e.drawX * kCellPx + (e.dir > 0 ? 0.82f : 0.18f) * float(e.w) * kCellPx - camX;
        const float hy = (e.drawY + 1.0f) * kCellPx - 72.0f - camY;
        drawGlow(r, mArt, hx, hy, 22.0f + float(c.tell % 4) * 3.0f, c.carrier ? rgb(160, 255, 80) : rgb(255, 120, 60), 0.75f);
      }

  if (!g.cycle)
    return;
  // The light: golden by day; by dusk a deep blue with the caps and the
  // glow-worms glowing through it; the time-lapse sweeps from one to the other.
  const float sweep = inHalf >= kGardenHalf - kGardenSweep
    ? std::min(1.0f, (float(inHalf - (kGardenHalf - kGardenSweep)) + alpha) / float(kGardenSweep))
    : -1.0f;
  float night = dusk ? 1.0f : 0.0f;
  if (sweep >= 0.0f)
    night = dusk ? 1.0f - sweep : sweep;
  if (night < 1.0f)
    drawGlow(r, mArt, float(kScreenW) - 160.0f, 60.0f, 700.0f, rgb(255, 200, 110), 0.14f * (1.0f - night));
  if (night > 0.0f)
  {
    r.fillRect(0, 0, float(kScreenW), float(kScreenH), rgba(22, 20, 78, int(118.0f * night)));
    DrawOpts v;
    v.alpha = 0.6f * night;
    r.draw(mArt.vignette, 0.0f, 0.0f, v);
    // The caps glow.
    for (const auto& l : mLayers)
    {
      if (!l.solid || l.id.rfind("dusk_", 0) != 0)
        continue;
      const float W = float(l.x1 - l.x0 + 1) * kTilePx;
      const float x = float(l.x0) * kTilePx - camX + W * 0.5f, y = float(l.y0) * kTilePx - camY + 10.0f;
      if (offScreen(x - W, y - 100.0f, W * 2.0f, 200.0f))
        continue;
      float shrink = 1.0f;
      if (dusk && inHalf >= kGardenHalf - kGardenDroop)
        shrink = 1.0f - 0.6f * float(inHalf - (kGardenHalf - kGardenDroop)) / float(kGardenDroop);
      const float pulse = 0.85f + 0.15f * std::sin(float(frame) * 0.07f + x * 0.01f);
      drawGlow(r, mArt, x, y, W * 0.75f * shrink + 40.0f, rgb(110, 255, 210), 0.45f * night * pulse * shrink);
      drawGlow(r, mArt, x, y + 18.0f, W * 0.3f * shrink + 20.0f, rgb(200, 255, 240), 0.5f * night * shrink);
    }
    // Glow-worms where the soil meets the air.
    const int tx0 = std::max(0, int(camX / kTilePx)), tx1 = std::min(mLevel->width - 1, int((camX + kScreenW) / kTilePx));
    const int ty0 = std::max(0, int(camY / kTilePx)), ty1 = std::min(mLevel->height - 2, int((camY + kScreenH) / kTilePx));
    for (int ty = ty0; ty <= ty1; ++ty)
      for (int tx = tx0; tx <= tx1; ++tx)
      {
        const unsigned h = hash2(tx * 7, ty * 13);
        if (h % 5u != 0u || mMap.block(tx, ty) != Tile::Empty || mMap.block(tx, ty + 1) != Tile::Solid)
          continue;
        const float ph = float(h % 628u) / 100.0f;
        const float gx = float(tx) * kTilePx + 32.0f + std::sin(float(frame) * 0.013f + ph) * 24.0f - camX;
        const float gyy = float(ty) * kTilePx + 40.0f + std::cos(float(frame) * 0.017f + ph) * 14.0f - camY;
        const float blink = 0.5f + 0.5f * std::sin(float(frame) * 0.05f + ph * 3.0f);
        drawGlow(r, mArt, gx, gyy, 18.0f, rgb(190, 255, 110), 0.7f * night * blink);
        drawGlow(r, mArt, gx, gyy, 5.0f, rgb(255, 255, 220), 0.9f * night * blink);
      }
  }
  // The time-lapse: the sun or the moon racing over, light sweeping across.
  if (sweep >= 0.0f)
  {
    const Texture& sun = baked(mArt, r, "sun", 220, 220, 110.0f, 110.0f, [](cairo_t* cr) { paintSun(cr); });
    const Texture& moon = baked(mArt, r, "moon", 160, 160, 80.0f, 80.0f, [](cairo_t* cr) { paintMoon(cr); });
    const auto arc = [&](float t, float& x, float& y) {
      x = -0.1f * float(kScreenW) + t * 1.2f * float(kScreenW);
      y = 70.0f + 380.0f * (2.0f * t - 1.0f) * (2.0f * t - 1.0f);
    };
    float sx, sy, mx, my;
    // Setting: across and down; rising: in from the left behind it.
    arc(0.45f + 0.55f * sweep, sx, sy);
    arc(std::max(0.0f, sweep * 1.2f - 0.2f) * 0.55f, mx, my);
    const Texture& setting = dusk ? moon : sun;
    const Texture& rising = dusk ? sun : moon;
    for (int k = 4; k >= 1; --k)
    {
      // Its streak: where it was a frame or two ago.
      float px, py;
      arc(0.45f + 0.55f * std::max(0.0f, sweep - 0.03f * float(k)), px, py);
      DrawOpts tr;
      tr.blend = Blend::Add;
      tr.alpha = 0.12f * (1.0f - float(k) / 5.0f);
      r.draw(setting, px, py, tr);
    }
    DrawOpts so;
    so.blend = dusk ? Blend::Alpha : Blend::Add;
    so.alpha = 1.0f - std::max(0.0f, sweep - 0.85f) / 0.15f;
    r.draw(setting, sx, sy, so);
    DrawOpts ro;
    ro.blend = dusk ? Blend::Add : Blend::Alpha;
    ro.alpha = std::clamp(sweep * 1.6f - 0.3f, 0.0f, 1.0f);
    r.draw(rising, mx, my, ro);
    // A band of light sweeping over the garden.
    const float bx = -200.0f + sweep * (float(kScreenW) + 400.0f);
    for (int k = 0; k < 6; ++k)
      drawGlow(r, mArt, bx, float(k) * 140.0f + 40.0f, 220.0f, dusk ? rgb(255, 220, 150) : rgb(170, 160, 255), 0.1f);
    // The flicker of the frames racing by.
    if ((frame / 2) % 3 == 0)
      r.fillRect(0, 0, float(kScreenW), float(kScreenH), rgba(255, 250, 230, 14));
  }
}

// --- The HUD ---------------------------------------------------------------------------

void World::drawGardenHud(Renderer& r, int frame) const
{
  const auto& g = mGarden;
  // The sundial: the shadow sweeping round, the dots round the rim the
  // time left in this half.
  if (g.cycle)
  {
    const bool dusk = gardenDusk();
    const int inHalf = gardenClock() % kGardenHalf;
    const float cx = float(kScreenW) - 76.0f, cy = 146.0f;
    const bool warn = inHalf >= kGardenHalf - kGardenDroop;
    if (warn)
      drawGlow(r, mArt, cx, cy, 80.0f, rgb(255, 120, 80), 0.35f + 0.25f * float((frame / 6) % 2));
    r.draw(baked(mArt, r, dusk ? "dial_dusk" : "dial_day", 112, 112, 56.0f, 56.0f, [dusk](cairo_t* cr) { paintSundial(cr, dusk); }),
      cx, cy);
    const float left = 1.0f - float(inHalf) / float(kGardenHalf);
    for (int i = 0; i < 24; ++i)
    {
      const float a = -1.5708f + float(i) * 6.2832f / 24.0f;
      const bool lit = float(i) / 24.0f < left;
      DrawOpts o;
      o.scale = lit ? 0.42f : 0.3f;
      o.tint = !lit ? rgb(60, 50, 50) : (warn ? rgb(255, 110, 80) : (dusk ? rgb(190, 210, 255) : rgb(255, 220, 110)));
      o.alpha = lit ? 1.0f : 0.5f;
      r.draw(mArt.dot, cx + std::cos(a) * 60.0f, cy + std::sin(a) * 60.0f, o);
    }
    DrawOpts sh;
    sh.angle = -90.0f + 360.0f * float(inHalf) / float(kGardenHalf);
    r.draw(baked(mArt, r, "dial_shadow", 50, 12, 4.0f, 6.0f, [](cairo_t* cr) {
      cairo_move_to(cr, 2, 6);
      cairo_line_to(cr, 48, 3);
      cairo_line_to(cr, 48, 9);
      cairo_close_path(cr);
      setRgba(cr, rgb(30, 20, 14), 0.75);
      cairo_fill(cr);
      circle(cr, 5, 6, 4);
      setRgba(cr, rgb(40, 28, 18), 1.0);
      cairo_fill(cr);
    }),
      cx, cy, sh);
    if (warn)
      r.drawText(dusk ? "DAWN" : "DUSK", cx, cy + 66.0f, {18.0f, rgb(255, 236, 200), rgba(40, 10, 0, 220), true}, Align::Center,
        0.6f + 0.4f * float((frame / 6) % 2));
  }
  // The narrator: an italic subtitle in the lower third, like a nature film.
  if (g.subtitle > 0)
  {
    const int total = 30 + int(g.subtitleText.size()) * 2;
    const float a = std::min({1.0f, float(g.subtitle) / 12.0f, float(total - g.subtitle + 1) / 8.0f});
    DrawOpts bo;
    bo.alpha = a;
    r.draw(baked(mArt, r, "band", 1280, 110, 0.0f, 0.0f, [](cairo_t* cr) { paintBand(cr); }), 0.0f, float(kScreenH) - 150.0f, bo);
    const std::string& line = g.subtitleText;
    r.draw(baked(mArt, r, "sub/" + line, 1280, 64, 640.0f, 32.0f, [&line](cairo_t* cr) { paintSubtitle(cr, line); }),
      float(kScreenW) / 2.0f, float(kScreenH) - 97.0f, bo);
  }
}

// --- Dandelion Drift -------------------------------------------------------------------

void World::drawGlideBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& gl = mGlide;
  const int W = mLevel->width, H = mLevel->height;
  const int tx0 = std::max(0, int(camX / kTilePx) - 1), tx1 = std::min(W - 1, int((camX + kScreenW) / kTilePx) + 1);
  // The fence's pickets (solid blocks up in the air) and the pond (solid
  // blocks just over the soil's last row).
  const Texture& picketTop = baked(mArt, r, "picket_top", 64, 64, 0.0f, 0.0f, [](cairo_t* cr) { paintPicket(cr, true); });
  const Texture& picket = baked(mArt, r, "picket", 64, 64, 0.0f, 0.0f, [](cairo_t* cr) { paintPicket(cr, false); });
  int pondX0 = -1;
  for (int tx = tx0; tx <= tx1 + 1; ++tx)
  {
    const bool water = tx <= tx1 && mMap.block(tx, H - 3) == Tile::Solid && mMap.block(tx, H - 2) == Tile::Solid;
    if (water && pondX0 < 0)
      pondX0 = tx;
    if (!water && pondX0 >= 0)
    {
      const float x = float(pondX0) * kTilePx - camX, y = float(H - 3) * kTilePx - camY + 10.0f;
      const int pw = (tx - pondX0) * int(kTilePx);
      r.fillRect(x, y - 10.0f, float(pw), 10.0f, rgb(60, 110, 90));
      r.draw(baked(mArt, r, "pond/" + std::to_string(pw) + "x118", pw, 118, 0.0f, 0.0f, [pw](cairo_t* cr) { paintPond(cr, pw, 118); }),
        x, y);
      for (int i = 0; i < pw / 200; ++i)
      {
        const float bob = std::sin(float(frame) * 0.04f + float(i)) * 2.0f;
        r.draw(baked(mArt, r, i % 2 ? "lilypad_f" : "lilypad", 150, 50, 75.0f, 25.0f, [i](cairo_t* cr) { paintLilyPad(cr, i % 2 == 1); }),
          x + 90.0f + float(i) * 200.0f, y + 4.0f + bob);
      }
      pondX0 = -1;
    }
    if (tx > tx1)
      break;
    for (int ty = 0; ty < H - 3; ++ty)
      if (mMap.block(tx, ty) == Tile::Solid)
      {
        const float x = float(tx) * kTilePx - camX, y = float(ty) * kTilePx - camY;
        if (y < -64.0f || y > float(kScreenH))
          continue;
        r.draw(ty > 0 && mMap.block(tx, ty - 1) != Tile::Solid ? picketTop : picket, x, y);
      }
  }
  // Gusts: warm air, streaks and fluff riding it.
  const Texture& fluff = baked(mArt, r, "fluff", 40, 40, 20.0f, 22.0f, [](cairo_t* cr) { paintFluff(cr); });
  for (const auto& w : gl.winds)
  {
    const float x = float(w.x0) * kCellPx - camX, y = float(w.y0) * kCellPx - camY;
    const float ww = float(w.x1 - w.x0 + 1) * kCellPx, h = float(w.y1 - w.y0 + 1) * kCellPx;
    if (offScreen(x, y, ww, h))
      continue;
    for (int k = 0; k < 3; ++k)
      drawGlow(r, mArt, x + ww * 0.5f, y + h * (0.2f + 0.3f * float(k)), ww * 0.9f, w.dir < 0 ? rgb(255, 230, 170) : rgb(170, 200, 255),
        0.1f);
    for (int i = 0; i < 10; ++i)
    {
      const float t = std::fmod(float(frame) * 0.012f * float(w.push) + float(i) / 10.0f, 1.0f);
      const float sy = w.dir < 0 ? y + h * (1.0f - t) : y + h * t;
      const float sx = x + ww * (0.12f + 0.08f * float(i % 10)) + std::sin(t * 9.0f + float(i)) * 6.0f;
      const float a = std::sin(t * 3.14159f);
      r.fillRect(sx, sy, 3.0f, 26.0f + float(i % 3) * 10.0f, withAlpha(rgb(255, 255, 255), int(90.0f * a)));
      if (i % 3 == 0)
      {
        DrawOpts fo;
        fo.alpha = a * 0.9f;
        fo.scale = 0.7f;
        fo.angle = std::sin(float(frame) * 0.1f + float(i)) * 20.0f;
        r.draw(fluff, sx + 10.0f, sy, fo);
      }
    }
  }
  // Dragonflies hovering on their beats, facing the runner.
  for (const auto& d : gl.flies)
  {
    const float x = (d.prevX + (d.x - d.prevX) * alpha) * kCellPx - camX;
    const float y = (d.prevY + (d.y - d.prevY) * alpha) * kCellPx - camY;
    if (offScreen(x - 100.0f, y - 50.0f, 200.0f, 100.0f))
      continue;
    const int beat = (frame / 2) % 2;
    drawGlow(r, mArt, x, y, 60.0f, rgb(150, 255, 240), 0.15f);
    r.draw(baked(mArt, r, "dragonfly/" + std::to_string(beat), 200, 90, 100.0f, 45.0f, [beat](cairo_t* cr) { paintDragonfly(cr, beat); }),
      x, y);
  }
}

void World::drawGlideFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& p = mPlayer;
  // The seed head the runner hangs from: its pod in their hands, the
  // parachute over them.
  const float x = (float(p.prevX) + float(p.x - p.prevX) * alpha + 1.5f) * kCellPx - camX;
  const float y = (float(p.prevY) + float(p.y - p.prevY) * alpha - 5.0f) * kCellPx - camY;
  DrawOpts o;
  o.scale = 0.5f;
  o.angle = std::sin(float(frame) * 0.06f) * 4.0f + (mGlide.bounce > 0 ? -8.0f : 0.0f);
  r.draw(baked(mArt, r, "rider", 360, 520, 180.0f, 470.0f, [](cairo_t* cr) { paintSeedRider(cr); }), x, y + 4.0f, o);
}

} // namespace gr
