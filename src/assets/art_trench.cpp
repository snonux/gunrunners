// Level 30, Fiber Trench (theme look "fiber_trench"): the sea floor off the
// coast. The masses are reef rock, slate blue and worn smooth into rounded
// stones, a fine grain of silt over them; now and then a block carries a
// clump of sponges and little coral heads, or a crust of barnacles and a
// starfish. Where you stand, a lip of pale silt with tufts of sea grass and
// a faint shimmer of light along its edge. Fire coral, red at the root and
// glowing gold at the tips, is the hazard. Behind: open water, bright blue
// under the surface going down into the dark, the sun's shafts slanting
// through it; far off, reef ridges, arches and a sunken freighter in the
// haze; close by, a kelp forest in silhouette. The exit is the cable
// station's relay, the fiber coming in at its foot. The depth (the water
// darkening as you go down), the moving light, the motes, the glowing cable
// and the wreck's hull are drawn live by the level (world_trench_draw.cpp).

#include "assets/art_trench.hpp"

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
constexpr Color kRockHi = rgb(78, 102, 120);
constexpr Color kRock = rgb(54, 74, 94);
constexpr Color kRockLo = rgb(34, 48, 66);
constexpr Color kSilt = rgb(196, 202, 178);
constexpr Color kGrass = rgb(64, 168, 124);
constexpr Color kSteel = rgb(122, 140, 152);
constexpr Color kSteelDark = rgb(38, 50, 62);
constexpr Color kOutline = rgb(8, 14, 24);
constexpr Color kFiber = rgb(90, 240, 255);

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
  cairo_scale(cr, rx, ry);
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

// The rock under every variant: rounded stones lit from above left, worn
// smooth, with a silt grain over them. The same on every variant (and
// wrapped) so the blocks tile without seams.
void rockBase(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 0, 64, 64);
  fillLinear(cr, 0, 0, 64, 64, {{0.0, kRockHi}, {0.5, kRock}, {1.0, kRockLo}});
  Rng rng(30011u);
  struct Stone
  {
    double x, y, rx, ry, a;
  };
  std::array<Stone, 7> stones{};
  for (auto& s : stones)
    s = {rng.range(0, 64), rng.range(0, 64), rng.range(9, 17), rng.range(7, 12), rng.range(-0.6f, 0.6f)};
  for (const auto& s : stones)
    wrap64([&](double ox, double oy) {
      // Its shadow below right, its body, its lit top left.
      softDot(cr, s.x + ox + 2.5, s.y + oy + 3.0, s.rx * 1.05, s.ry * 1.05, rgb(14, 22, 34), 0.35);
      softDot(cr, s.x + ox, s.y + oy, s.rx, s.ry, lerpColor(kRock, kRockHi, 0.55f), 0.55);
      softDot(cr, s.x + ox - s.rx * 0.3, s.y + oy - s.ry * 0.35, s.rx * 0.55, s.ry * 0.45, rgb(150, 180, 196), 0.2);
    });
  // Cracks between some of them.
  for (int i = 0; i < 3; ++i)
  {
    const double x = rng.range(4, 60), y = rng.range(4, 60), a = rng.range(0.0f, float(kPi)), l = rng.range(8, 16);
    wrap64([&](double ox, double oy) {
      cairo_move_to(cr, x + ox, y + oy);
      cairo_curve_to(cr, x + ox + std::cos(a) * l * 0.4, y + oy + std::sin(a) * l * 0.4 + 2, x + ox + std::cos(a) * l * 0.7,
        y + oy + std::sin(a) * l * 0.7 - 2, x + ox + std::cos(a) * l, y + oy + std::sin(a) * l);
      stroke(cr, rgb(16, 24, 36), 1.4, 0.5);
    });
  }
  // The grain.
  for (int i = 0; i < 90; ++i)
  {
    const double x = rng.range(0, 64), y = rng.range(0, 64), s = rng.range(0.7f, 1.6f);
    cairo_arc(cr, x, y, s * 0.5, 0, 2 * kPi);
    setRgba(cr, i % 3 ? rgb(20, 30, 44) : rgb(170, 196, 206), rng.range(0.08f, 0.22f));
    cairo_fill(cr);
  }
}

// A clump of sponges and coral heads (variant 1), kept off the edges.
void spongeClump(cairo_t* cr)
{
  // Two tube sponges.
  for (const auto& tb : {std::array<double, 4>{22, 44, 6, 15}, std::array<double, 4>{31, 47, 5, 11}})
  {
    const double x = tb[0], y = tb[1], rw = tb[2], hgt = tb[3];
    cairo_move_to(cr, x - rw, y);
    cairo_curve_to(cr, x - rw, y - hgt * 0.6, x - rw * 0.8, y - hgt, x - rw * 0.7, y - hgt);
    cairo_line_to(cr, x + rw * 0.7, y - hgt);
    cairo_curve_to(cr, x + rw * 0.8, y - hgt, x + rw, y - hgt * 0.6, x + rw, y);
    cairo_close_path(cr);
    fillLinear(cr, x - rw, 0, x + rw, 0, {{0.0, rgb(214, 132, 92)}, {0.5, rgb(236, 168, 110)}, {1.0, rgb(150, 82, 64)}}, 1.0,
      true);
    stroke(cr, rgb(70, 34, 30), 1.2, 0.8);
    ellipse(cr, x, y - hgt, rw * 0.7, 2.0);
    setRgba(cr, rgb(70, 30, 30), 1.0);
    cairo_fill(cr);
  }
  // Coral heads: knobbly pink domes.
  for (const auto& h : {std::array<double, 3>{43, 46, 7}, std::array<double, 3>{50, 49, 5}})
  {
    ellipse(cr, h[0], h[1], h[2], h[2] * 0.8);
    fillLinear(cr, 0, h[1] - h[2], 0, h[1] + h[2], {{0.0, rgb(255, 150, 176)}, {1.0, rgb(170, 70, 110)}}, 1.0, true);
    stroke(cr, rgb(80, 26, 50), 1.2, 0.8);
    for (int k = 0; k < 5; ++k)
    {
      const double a = -kPi * 0.9 + k * 0.45;
      cairo_arc(cr, h[0] + std::cos(a) * h[2] * 0.55, h[1] + std::sin(a) * h[2] * 0.45, 1.3, 0, 2 * kPi);
      setRgba(cr, rgb(255, 210, 220), 0.8);
      cairo_fill(cr);
    }
  }
}

// A crust of barnacles and a starfish (variant 2).
void barnacles(cairo_t* cr)
{
  for (const auto& b : {std::array<double, 3>{16, 18, 4.5}, std::array<double, 3>{24, 14, 3.5}, std::array<double, 3>{21, 25, 3.8},
         std::array<double, 3>{12, 26, 3.0}})
  {
    cairo_arc(cr, b[0], b[1], b[2], 0, 2 * kPi);
    fillLinear(cr, 0, b[1] - b[2], 0, b[1] + b[2], {{0.0, rgb(220, 222, 210)}, {1.0, rgb(130, 136, 132)}}, 1.0, true);
    stroke(cr, rgb(40, 44, 50), 1.0, 0.8);
    cairo_arc(cr, b[0], b[1], b[2] * 0.4, 0, 2 * kPi);
    setRgba(cr, rgb(30, 34, 40), 1.0);
    cairo_fill(cr);
  }
  // The starfish.
  const double cx = 44, cy = 42;
  cairo_new_path(cr);
  for (int k = 0; k <= 10; ++k)
  {
    const double a = -kPi * 0.5 + 0.3 + k * kPi / 5, rr = k % 2 ? 4.0 : 11.0;
    const double x = cx + std::cos(a) * rr, y = cy + std::sin(a) * rr;
    if (k == 0)
      cairo_move_to(cr, x, y);
    else
      cairo_line_to(cr, x, y);
  }
  cairo_close_path(cr);
  cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
  fillLinear(cr, 0, cy - 11, 0, cy + 11, {{0.0, rgb(255, 160, 80)}, {1.0, rgb(214, 90, 50)}}, 1.0, true);
  stroke(cr, rgb(90, 36, 20), 1.6, 0.9);
  for (int k = 0; k < 5; ++k)
  {
    const double a = -kPi * 0.5 + 0.3 + k * 2 * kPi / 5;
    for (int d = 1; d <= 2; ++d)
    {
      cairo_arc(cr, cx + std::cos(a) * d * 3.4, cy + std::sin(a) * d * 3.4, 0.9, 0, 2 * kPi);
      setRgba(cr, rgb(255, 230, 190), 0.9);
      cairo_fill(cr);
    }
  }
}

// One kelp strand from (x, base) up, swaying `sway` px, `h` tall, with
// blades along it.
void kelp(cairo_t* cr, Rng& rng, double x, double base, double h, double sway, Color c, double alpha)
{
  cairo_move_to(cr, x, base);
  cairo_curve_to(cr, x + sway, base - h * 0.35, x - sway, base - h * 0.7, x + sway * 0.6, base - h);
  stroke(cr, c, rng.range(4, 7), alpha);
  const int blades = int(h / 34.0);
  for (int l = 1; l < blades; ++l)
  {
    const double u = double(l) / blades;
    // A point on the stem (the same curve, sampled).
    const double s = 1 - u;
    const double px = s * s * s * x + 3 * s * s * u * (x + sway) + 3 * s * u * u * (x - sway) + u * u * u * (x + sway * 0.6);
    const double py = base - h * u;
    const double side = l % 2 ? 1.0 : -1.0, len = rng.range(16, 30) * (1.1 - u * 0.5);
    cairo_move_to(cr, px, py);
    cairo_curve_to(cr, px + side * len * 0.4, py - len * 0.5, px + side * len * 0.9, py - len * 0.4, px + side * len, py - len * 0.15);
    cairo_curve_to(cr, px + side * len * 0.7, py - len * 0.05, px + side * len * 0.3, py + 2, px, py);
    setRgba(cr, c, alpha * 0.9);
    cairo_fill(cr);
  }
}

} // namespace

bool isFiberTrench(const Theme& t) { return std::string_view(t.look) == "fiber_trench"; }

Texture bakeTrenchSolid(const Renderer& r, const Theme& t, int variant)
{
  (void)t;
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  rockBase(cr);
  if (variant == 1)
    spongeClump(cr);
  else if (variant == 2)
    barnacles(cr);
  return img.toTexture(r, 0.0f, 0.0f);
}

// Where you stand: a lip of pale silt, its edge rippled and lit by the
// light from above, a soft shade under it, pebbles and a shell on it and
// tufts of sea grass. The wave repeats every 64 px so the lips join.
Texture bakeTrenchSolidTop(const Renderer& r, const Theme& t, float topOffset, int height)
{
  (void)t;
  VectorImage img(64, height);
  cairo_t* cr = img.cr();
  const double y = topOffset;
  auto edge = [&](double x) { return y - 1.0 + 1.2 * std::sin(x / 64.0 * 2 * kPi) + 0.6 * std::sin(x / 32.0 * 2 * kPi + 1.0); };
  // The shade under the lip.
  cairo_rectangle(cr, 0, y + 8, 64, 12);
  {
    cairo_pattern_t* g = cairo_pattern_create_linear(0, y + 8, 0, y + 20);
    stop(g, 0.0, rgb(10, 18, 30), 0.4);
    stop(g, 1.0, rgb(10, 18, 30), 0.0);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  // The silt.
  cairo_move_to(cr, 0, edge(0));
  for (double x = 2; x <= 64; x += 2)
    cairo_line_to(cr, x, edge(x));
  for (double x = 64; x >= 0; x -= 4)
    cairo_line_to(cr, x, y + 8 + 1.5 * std::sin(x / 64.0 * 4 * kPi + 0.5));
  cairo_close_path(cr);
  fillLinear(cr, 0, y - 2, 0, y + 10, {{0.0, kSilt}, {0.45, rgb(160, 172, 160)}, {1.0, rgb(98, 118, 124)}});
  // Ripples in it.
  for (int k = 0; k < 2; ++k)
  {
    cairo_move_to(cr, 0, y + 3 + k * 3);
    for (double x = 4; x <= 64; x += 4)
      cairo_line_to(cr, x, y + 3 + k * 3 + 0.8 * std::sin(x / 64.0 * 6 * kPi + k));
    stroke(cr, rgb(250, 252, 236), 0.8, 0.25);
  }
  // Light along the very edge.
  cairo_move_to(cr, 0, edge(0));
  for (double x = 2; x <= 64; x += 2)
    cairo_line_to(cr, x, edge(x));
  stroke(cr, rgb(214, 250, 255), 1.4, 0.75);
  // Pebbles and a shell.
  Rng rng(30021u);
  for (int i = 0; i < 4; ++i)
  {
    const double x = rng.range(6, 58), py = y + rng.range(2, 6);
    ellipse(cr, x, py, rng.range(1.4f, 2.6f), rng.range(1.0f, 1.6f));
    setRgba(cr, i % 2 ? rgb(120, 130, 136) : rgb(220, 214, 196), 0.9);
    cairo_fill(cr);
  }
  cairo_move_to(cr, 40, y + 3);
  cairo_curve_to(cr, 40, y - 3, 48, y - 3, 48, y + 3);
  cairo_close_path(cr);
  setRgba(cr, rgb(250, 226, 214), 0.95);
  cairo_fill_preserve(cr);
  stroke(cr, rgb(150, 110, 100), 0.8, 0.8);
  for (int k = 0; k < 3; ++k)
  {
    cairo_move_to(cr, 44, y + 3);
    cairo_line_to(cr, 41.5 + k * 2.5, y - 1.2);
    stroke(cr, rgb(180, 130, 120), 0.6, 0.7);
  }
  // Tufts of sea grass.
  for (const auto& tuft : {std::array<double, 2>{12, 14}, std::array<double, 2>{29, 9}, std::array<double, 2>{55, 12}})
    for (int b = -1; b <= 1; ++b)
    {
      const double bx = tuft[0] + b * 2.2, hgt = tuft[1] * (b == 0 ? 1.0 : 0.72);
      cairo_move_to(cr, bx - 1.4, y + 2);
      cairo_curve_to(cr, bx - 1.2, y - hgt * 0.5, bx + b * 3.0, y - hgt * 0.8, bx + b * 4.0, y - hgt);
      cairo_curve_to(cr, bx + b * 2.6, y - hgt * 0.7, bx + 1.2, y - hgt * 0.45, bx + 1.4, y + 2);
      cairo_close_path(cr);
      fillLinear(cr, 0, y - hgt, 0, y + 2, {{0.0, rgb(150, 230, 170)}, {1.0, kGrass}});
    }
  return img.toTexture(r, 0.0f, topOffset);
}

// Fire coral: branching spines from a red clump, gold at the tips with a
// glow, sharp enough to read as "don't". Point up (hanging ones are turned
// over by the tiles).
Texture bakeTrenchSpikes(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  struct Spine
  {
    double x, lean, h, w;
  };
  static constexpr std::array<Spine, 6> kSpines{{{7, -5, 30, 5}, {18, -2, 40, 6}, {29, 3, 32, 5.5}, {38, -3, 44, 6.5},
    {49, 4, 34, 5.5}, {58, 6, 26, 4.5}}};
  for (const auto& s : kSpines)
    softDot(cr, s.x + s.lean, 62 - s.h, 7, 7, rgb(255, 200, 90), 0.5);
  for (const auto& s : kSpines)
  {
    const double tipX = s.x + s.lean, tipY = 62 - s.h;
    cairo_move_to(cr, s.x - s.w, 62);
    cairo_curve_to(cr, s.x - s.w * 0.6, 62 - s.h * 0.5, tipX - 1.5, tipY + 6, tipX, tipY);
    cairo_curve_to(cr, tipX + 1.5, tipY + 6, s.x + s.w * 0.6, 62 - s.h * 0.5, s.x + s.w, 62);
    cairo_close_path(cr);
    fillLinear(cr, 0, 62, 0, tipY, {{0.0, rgb(170, 30, 50)}, {0.55, rgb(240, 90, 70)}, {1.0, rgb(255, 226, 130)}}, 1.0, true);
    stroke(cr, rgb(60, 8, 20), 1.6);
    // A little side branch on the tall ones.
    if (s.h > 35)
    {
      const double bx = s.x + s.lean * 0.5, by = 62 - s.h * 0.5;
      cairo_move_to(cr, bx, by + 4);
      cairo_curve_to(cr, bx + 4, by, bx + 8, by - 4, bx + 10, by - 10);
      cairo_curve_to(cr, bx + 8, by - 2, bx + 5, by + 4, bx + 1, by + 8);
      cairo_close_path(cr);
      fillLinear(cr, 0, by + 8, 0, by - 10, {{0.0, rgb(220, 70, 60)}, {1.0, rgb(255, 220, 120)}}, 1.0, true);
      stroke(cr, rgb(60, 8, 20), 1.2);
    }
    cairo_arc(cr, tipX, tipY + 1.5, 1.6, 0, 2 * kPi);
    setRgba(cr, rgb(255, 255, 220), 1.0);
    cairo_fill(cr);
  }
  // The clump at the root.
  for (int k = 0; k < 6; ++k)
  {
    ellipse(cr, 5 + k * 11, 61, 8, 4.5);
    fillLinear(cr, 0, 56, 0, 66, {{0.0, rgb(200, 50, 60)}, {1.0, rgb(90, 14, 30)}}, 1.0, true);
    stroke(cr, rgb(50, 6, 16), 1.2);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// Open water: bright blue under the surface fading down into the dark, a
// shimmer of caustics up top, the sun's glow and faint shafts, specks.
Texture bakeTrenchSky(const Renderer& r, const Theme& t)
{
  VectorImage img(kScreenW, kScreenH);
  cairo_t* cr = img.cr();
  cairo_rectangle(cr, 0, 0, kScreenW, kScreenH);
  fillLinear(cr, 0, 0, 0, kScreenH, {{0.0, t.skyBottom}, {0.45, t.skyMid}, {1.0, t.skyTop}});
  // The sun's glow from above.
  softDot(cr, 520, -80, 760, 420, rgb(170, 240, 255), 0.32);
  // Caustics: a loose net of light, fading down.
  Rng rng(30031u);
  for (int i = 0; i < 70; ++i)
  {
    const double x = rng.range(-40, float(kScreenW) + 40), y = rng.range(0, 260), w = rng.range(30, 90);
    cairo_move_to(cr, x, y);
    cairo_curve_to(cr, x + w * 0.3, y - 10, x + w * 0.6, y + 12, x + w, y + rng.range(-6, 6));
    stroke(cr, rgb(200, 250, 255), rng.range(1.0f, 2.4f), 0.07 * (1.0 - y / 300.0));
  }
  // Faint shafts (the bright ones move, world_trench_draw.cpp).
  for (int i = 0; i < 7; ++i)
  {
    const double x = rng.range(-200, float(kScreenW)), w = rng.range(30, 110), lean = rng.range(160, 300);
    cairo_move_to(cr, x, 0);
    cairo_line_to(cr, x + w, 0);
    cairo_line_to(cr, x + w + lean, kScreenH);
    cairo_line_to(cr, x + lean - w * 0.4, kScreenH);
    cairo_close_path(cr);
    cairo_pattern_t* p = cairo_pattern_create_linear(0, 0, 0, kScreenH);
    cairo_pattern_add_color_stop_rgba(p, 0, 0.8, 1.0, 1.0, 0.1);
    cairo_pattern_add_color_stop_rgba(p, 0.6, 0.8, 1.0, 1.0, 0.02);
    cairo_pattern_add_color_stop_rgba(p, 1, 0.8, 1.0, 1.0, 0.0);
    cairo_set_source(cr, p);
    cairo_fill(cr);
    cairo_pattern_destroy(p);
  }
  // Specks hanging in the water.
  for (int i = 0; i < 260; ++i)
  {
    cairo_arc(cr, rng.uniform() * kScreenW, rng.uniform() * kScreenH, 0.6 + rng.uniform() * 1.3, 0, 2 * kPi);
    setRgba(cr, rgb(210, 240, 255), 0.06 + rng.uniform() * 0.18);
    cairo_fill(cr);
  }
  // Darker toward the corners.
  cairo_rectangle(cr, 0, 0, kScreenW, kScreenH);
  cairo_pattern_t* v = cairo_pattern_create_radial(640, 300, 300, 640, 360, 860);
  cairo_pattern_add_color_stop_rgba(v, 0, 0, 0.02, 0.06, 0.0);
  cairo_pattern_add_color_stop_rgba(v, 1, 0, 0.02, 0.06, 0.45);
  cairo_set_source(cr, v);
  cairo_fill(cr);
  cairo_pattern_destroy(v);
  return img.toTexture(r, 0.0f, 0.0f);
}

// Far: reef ridges and arches in the haze, a sunken freighter lying on its
// side, distant kelp, all fading into the blue.
Texture bakeTrenchFar(const Renderer& r, const Theme& t, int layerW)
{
  VectorImage img(layerW, kScreenH);
  cairo_t* cr = img.cr();
  const double W = layerW;
  const Color haze = lerpColor(t.farLayer, t.skyMid, 0.35f);
  const Color body = t.farLayer;
  Rng rng(30041u);
  // Distant kelp first, thin and pale.
  for (double x = 20; x < W; x += rng.range(60, 150))
  {
    Rng k(std::uint32_t(30051 + int(x)));
    const double h = rng.range(160, 340), sway = rng.range(-30, 30);
    for (const double ox : {-W, 0.0, W})
      if (x + ox > -80 && x + ox < W + 80)
      {
        Rng kk = k;
        kelp(cr, kk, x + ox, kScreenH, h, sway, lerpColor(haze, rgb(40, 110, 110), 0.3f), 0.35);
      }
  }
  // Ridges: rounded towers, wrapped.
  for (double x = 0; x < W; x += rng.range(150, 260))
  {
    const double w = rng.range(100, 220), top = rng.range(300, 520);
    for (const double ox : {-W, 0.0, W})
    {
      if (x + ox + w < -10 || x + ox - w > W + 10)
        continue;
      cairo_move_to(cr, x + ox - w * 0.65, kScreenH);
      cairo_curve_to(cr, x + ox - w * 0.55, top + 90, x + ox - w * 0.35, top, x + ox, top);
      cairo_curve_to(cr, x + ox + w * 0.35, top, x + ox + w * 0.55, top + 90, x + ox + w * 0.65, kScreenH);
      cairo_close_path(cr);
      fillLinear(cr, 0, top, 0, kScreenH, {{0.0, haze}, {1.0, body}});
    }
  }
  // An arch.
  for (const double ox : {-W, 0.0, W})
  {
    const double ax = 1500 + ox;
    if (ax < -400 || ax > W + 400)
      continue;
    cairo_move_to(cr, ax - 260, kScreenH);
    cairo_curve_to(cr, ax - 250, 380, ax - 120, 330, ax, 330);
    cairo_curve_to(cr, ax + 120, 330, ax + 250, 380, ax + 260, kScreenH);
    cairo_line_to(cr, ax + 170, kScreenH);
    cairo_curve_to(cr, ax + 160, 470, ax + 90, 420, ax, 420);
    cairo_curve_to(cr, ax - 90, 420, ax - 160, 470, ax - 170, kScreenH);
    cairo_close_path(cr);
    fillLinear(cr, 0, 330, 0, kScreenH, {{0.0, lerpColor(haze, body, 0.3f)}, {1.0, body}});
  }
  // A sunken freighter on its side, bow up.
  for (const double ox : {-W, 0.0, W})
  {
    const double wx = 560 + ox, wy = 600;
    if (wx < -700 || wx > W + 100)
      continue;
    cairo_move_to(cr, wx - 360, wy + 40);
    cairo_line_to(cr, wx + 220, wy - 70);
    cairo_curve_to(cr, wx + 300, wy - 90, wx + 330, wy - 40, wx + 300, wy + 10);
    cairo_line_to(cr, wx - 320, wy + 140);
    cairo_close_path(cr);
    setRgba(cr, lerpColor(body, rgb(6, 14, 24), 0.3f), 0.85);
    cairo_fill(cr);
    // Its superstructure and a mast.
    cairo_rectangle(cr, wx - 140, wy - 50, 90, 60);
    setRgba(cr, lerpColor(body, rgb(6, 14, 24), 0.35f), 0.85);
    cairo_fill(cr);
    cairo_move_to(cr, wx + 60, wy - 30);
    cairo_line_to(cr, wx + 100, wy - 190);
    stroke(cr, lerpColor(body, rgb(6, 14, 24), 0.3f), 6, 0.8);
    for (int k = 0; k < 7; ++k)
    {
      cairo_arc(cr, wx - 260 + k * 62, wy + 66 - k * 13, 6, 0, 2 * kPi);
      setRgba(cr, rgb(120, 200, 220), 0.12);
      cairo_fill(cr);
    }
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// Near: a kelp forest in silhouette, with rocks and coral heads at its feet.
Texture bakeTrenchNear(const Renderer& r, const Theme& t, int layerW)
{
  VectorImage img(layerW, kScreenH);
  cairo_t* cr = img.cr();
  const double W = layerW;
  Rng rng(30061u);
  const Color stem = lerpColor(t.nearLayer, rgb(20, 90, 70), 0.45f);
  for (double x = 10; x < W; x += rng.range(40, 120))
  {
    const double h = rng.range(240, 560), sway = rng.range(-60, 60);
    const std::uint32_t seed = std::uint32_t(30071 + int(x) * 3);
    for (const double ox : {-W, 0.0, W})
      if (x + ox > -120 && x + ox < W + 120)
      {
        Rng k(seed);
        kelp(cr, k, x + ox, kScreenH + 10, h, sway, stem, 0.85);
      }
  }
  // Rocks and coral heads along the bottom.
  for (double x = 0; x < W; x += rng.range(90, 200))
  {
    const double w = rng.range(60, 140), h = rng.range(30, 80);
    const Color c = rng.uniform() < 0.3f ? lerpColor(t.nearLayer, rgb(150, 60, 90), 0.35f) : t.nearLayer;
    for (const double ox : {-W, 0.0, W})
    {
      if (x + ox + w < 0 || x + ox - w > W)
        continue;
      ellipse(cr, x + ox, kScreenH, w, h);
      setRgba(cr, c, 0.95);
      cairo_fill(cr);
    }
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

Texture bakeTrenchExitBase(const Renderer& r, const Theme& t)
{
  (void)t;
  // 96 wide, 144 tall; (0, 0) is the top-left of the 64 x 128 exit area.
  VectorImage img(96, 144);
  cairo_t* cr = img.cr();
  cairo_translate(cr, 16, 8);
  // The rails, with their lights.
  for (const double x : {-6.0, 62.0})
  {
    cairo_rectangle(cr, x, 4, 8, 106);
    fillLinear(cr, x, 0, x + 8, 0, {{0.0, kSteel}, {1.0, kSteelDark}}, 1.0, true);
    stroke(cr, kOutline, 1.6);
    for (int i = 0; i < 5; ++i)
    {
      cairo_arc(cr, x + 4, 20 + i * 20, 2.2, 0, 2 * kPi);
      setRgba(cr, kFiber, 1.0);
      cairo_fill(cr);
    }
  }
  // The emitter cap.
  cairo_move_to(cr, -12, 6);
  cairo_line_to(cr, -8, -6);
  cairo_line_to(cr, 72, -6);
  cairo_line_to(cr, 76, 6);
  cairo_close_path(cr);
  fillLinear(cr, 0, -6, 0, 6, {{0.0, rgb(170, 188, 198)}, {1.0, kSteelDark}}, 1.0, true);
  stroke(cr, kOutline, 2.0);
  ellipse(cr, 32, 7, 26, 3.5);
  setRgba(cr, rgb(200, 255, 255), 0.9);
  cairo_fill(cr);
  // The plinth: a heavy housing, hazard striped, a gland on its left where
  // the fiber comes in.
  cairo_move_to(cr, -14, 132);
  cairo_line_to(cr, -10, 108);
  cairo_line_to(cr, 74, 108);
  cairo_line_to(cr, 78, 132);
  cairo_close_path(cr);
  fillLinear(cr, 0, 108, 0, 132, {{0.0, rgb(150, 168, 178)}, {0.4, kSteel}, {1.0, kSteelDark}}, 1.0, true);
  stroke(cr, kOutline, 2.2);
  cairo_save(cr);
  cairo_rectangle(cr, 4, 122, 56, 6);
  cairo_clip(cr);
  for (double x = -4; x < 64; x += 10)
  {
    cairo_move_to(cr, x, 128);
    cairo_line_to(cr, x + 5, 122);
    cairo_line_to(cr, x + 10, 122);
    cairo_line_to(cr, x + 5, 128);
    cairo_close_path(cr);
    setRgba(cr, rgb(250, 200, 40), 1.0);
    cairo_fill(cr);
  }
  cairo_restore(cr);
  cairo_rectangle(cr, 4, 122, 56, 6);
  stroke(cr, kOutline, 1.0, 0.7);
  ellipse(cr, 32, 109, 28, 6);
  setRgba(cr, rgb(200, 255, 255), 0.95);
  cairo_fill(cr);
  // The gland and the fiber going in.
  cairo_move_to(cr, -16, 124);
  cairo_line_to(cr, -8, 124);
  stroke(cr, rgb(20, 40, 50), 9);
  cairo_move_to(cr, -16, 124);
  cairo_line_to(cr, -8, 124);
  stroke(cr, kFiber, 4);
  cairo_arc(cr, -8, 124, 6, 0, 2 * kPi);
  fillLinear(cr, 0, 118, 0, 130, {{0.0, rgb(230, 190, 90)}, {1.0, rgb(130, 90, 30)}}, 1.0, true);
  stroke(cr, kOutline, 1.4);
  cairo_arc(cr, -8, 124, 2.4, 0, 2 * kPi);
  setRgba(cr, kFiber, 1.0);
  cairo_fill(cr);
  return img.toTexture(r, 16.0f, 8.0f);
}

} // namespace gr
