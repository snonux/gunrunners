// Level 17's drawing (world_green.cpp has the rules): the trench water, the
// grow lamps and their light, the shootable switches, the plants they grow
// (bridges, leaves, creepers, stairs, the giant flower), the basil, spore
// clouds, the Hedge Trimmer's blades, seed pods and thorn walls, and the
// Growth Spurt's size readout. The pieces are baked with Cairo the first time
// they are drawn and kept in the Art's sprite cache.

#include "game/world.hpp"

#include "assets/art.hpp"
#include "base/math.hpp"
#include "render/vector.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>

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
constexpr float kCellPx = float(kCellSize) * kPixelScale; // 32
constexpr float kTilePx = kCellPx * float(kCellsPerTile);  // 64
constexpr int kUnit = 90; // a plant's growth per tile (green.hpp)
constexpr Color kInk = rgb(16, 26, 18);
const Color kSteel = rgb(170, 184, 196);
const Color kSteelDark = rgb(70, 82, 96);
const Color kLampPink = rgb(255, 90, 210);

bool visible(float x, float y, float w, float h)
{
  return x + w > -96.0f && x < float(kScreenW) + 96.0f && y + h > -96.0f && y < float(kScreenH) + 96.0f;
}

unsigned hashCell(int x, int y)
{
  unsigned h = unsigned(x) * 73856093u ^ unsigned(y) * 19349663u;
  h ^= h >> 13;
  h *= 0x5bd1e995u;
  return h ^ (h >> 15);
}

// A texture baked once and kept in the Art's sprite cache under `key`.
const Texture& baked(const Art& art, const Renderer& r, const std::string& key, int w, int h, float ax, float ay,
  const std::function<void(cairo_t*)>& paint)
{
  const std::string id = "~green/" + key;
  auto it = art.styled.find(id);
  if (it != art.styled.end())
    return it->second.facing[0];
  VectorImage img(w, h);
  paint(img.cr());
  Sprite s;
  s.facing[0] = img.toTexture(r, ax, ay);
  return art.styled.emplace(id, std::move(s)).first->second.facing[0];
}

void setRgba(cairo_t* cr, Color c, double a)
{
  cairo_set_source_rgba(cr, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, a);
}

void linearFill(cairo_t* cr, double x0, double y0, double x1, double y1, Color a, Color b, bool preserve = false)
{
  cairo_pattern_t* g = cairo_pattern_create_linear(x0, y0, x1, y1);
  cairo_pattern_add_color_stop_rgba(g, 0, redOf(a) / 255.0, greenOf(a) / 255.0, blueOf(a) / 255.0, alphaOf(a) / 255.0);
  cairo_pattern_add_color_stop_rgba(g, 1, redOf(b) / 255.0, greenOf(b) / 255.0, blueOf(b) / 255.0, alphaOf(b) / 255.0);
  cairo_set_source(cr, g);
  if (preserve)
    cairo_fill_preserve(cr);
  else
    cairo_fill(cr);
  cairo_pattern_destroy(g);
}

void outline(cairo_t* cr, double width = 2.0, Color c = kInk)
{
  setColor(cr, c);
  cairo_set_line_width(cr, width);
  cairo_stroke(cr);
}

// A pointed leaf from (x, y) along angle a.
void leafShape(cairo_t* cr, double x, double y, double a, double len, double width, Color light, Color dark)
{
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_rotate(cr, a);
  cairo_move_to(cr, 0, 0);
  cairo_curve_to(cr, len * 0.3, -width, len * 0.75, -width * 0.8, len, 0);
  cairo_curve_to(cr, len * 0.75, width * 0.8, len * 0.3, width, 0, 0);
  cairo_close_path(cr);
  linearFill(cr, 0, -width, 0, width, light, dark, true);
  outline(cr, 1.6);
  cairo_move_to(cr, len * 0.06, 0);
  cairo_line_to(cr, len * 0.85, 0);
  setRgba(cr, dark, 0.9);
  cairo_set_line_width(cr, 1.3);
  cairo_stroke(cr);
  cairo_restore(cr);
}

// Plant colours, fresh or withered.
struct Greens
{
  Color light, mid, dark, stem;
};

Greens greens(bool dry)
{
  if (dry)
    return {rgb(214, 170, 96), rgb(160, 112, 58), rgb(96, 62, 30), rgb(120, 82, 44)};
  return {rgb(160, 236, 100), rgb(84, 168, 60), rgb(34, 96, 36), rgb(70, 130, 46)};
}

// --- Plant tiles (64 x 64, anchored at their centre) ---------------------------

void paintBridge(cairo_t* cr, bool dry, int seed)
{
  const Greens g = greens(dry);
  // Hanging roots first, behind.
  Rng rng(std::uint32_t(seed * 31 + 7));
  for (int k = 0; k < 3; ++k)
  {
    const double x = 8 + k * 22 + rng.range(-4, 4);
    cairo_move_to(cr, x, 24);
    cairo_curve_to(cr, x + 6, 34, x - 6, 44, x + 2, 40 + rng.range(6, 20));
    setRgba(cr, g.stem, 0.9);
    cairo_set_line_width(cr, 2.2);
    cairo_stroke(cr);
  }
  // Two braided vines, one period per tile so the tiles join up.
  for (int pass = 0; pass < 2; ++pass)
    for (int v = 0; v < 2; ++v)
    {
      for (int i = 0; i <= 32; ++i)
      {
        const double x = i * 2.0, y = 15 + 6 * std::sin(x * 2 * kPi / 64.0 + v * kPi);
        if (i == 0)
          cairo_move_to(cr, x - 2, y);
        else
          cairo_line_to(cr, x, y);
      }
      cairo_line_to(cr, 66, 15 + 6 * std::sin(2 * kPi + v * kPi));
      setColor(cr, pass == 0 ? kInk : (v ? g.mid : g.stem));
      cairo_set_line_width(cr, pass == 0 ? 12 : 8);
      cairo_stroke(cr);
    }
  // Highlights and ties.
  for (int i = 0; i <= 32; ++i)
  {
    const double x = i * 2.0, y = 13 + 6 * std::sin(x * 2 * kPi / 64.0);
    if (i == 0)
      cairo_move_to(cr, x, y);
    else
      cairo_line_to(cr, x, y);
  }
  setRgba(cr, g.light, 0.7);
  cairo_set_line_width(cr, 2);
  cairo_stroke(cr);
  for (const double x : {16.0, 48.0})
  {
    cairo_move_to(cr, x - 4, 8);
    cairo_line_to(cr, x + 4, 22);
    setColor(cr, g.dark);
    cairo_set_line_width(cr, 3);
    cairo_stroke(cr);
  }
  // A few leaves sprouting on top.
  for (int k = 0; k < 2; ++k)
    leafShape(cr, 10 + k * 30 + rng.range(0, 14), 10, -kPi * 0.5 + rng.range(-1.1f, 1.1f), rng.range(12, 18), 5, g.light,
      g.dark);
}

void paintLeaf(cairo_t* cr, bool dry, int seed, bool flower)
{
  Greens g = greens(dry);
  if (flower && !dry)
    g = {rgb(190, 250, 130), rgb(110, 200, 80), rgb(40, 110, 50), rgb(80, 150, 60)};
  Rng rng(std::uint32_t(seed * 17 + 3));
  // Its stalk, curling down out of the tile.
  cairo_move_to(cr, 32, 20);
  cairo_curve_to(cr, 30, 34, 40, 44, 34 + rng.range(-6, 6), 62);
  setColor(cr, kInk);
  cairo_set_line_width(cr, 7);
  cairo_stroke_preserve(cr);
  setColor(cr, g.stem);
  cairo_set_line_width(cr, 4);
  cairo_stroke(cr);
  // The leaf: broad and flat, a touch wider than the tile so a row joins up.
  const double droop = rng.range(-2, 3);
  cairo_move_to(cr, -6, 14 + droop);
  cairo_curve_to(cr, 10, -4, 54, -4, 70, 12 - droop);
  cairo_curve_to(cr, 54, 30, 10, 30, -6, 14 + droop);
  cairo_close_path(cr);
  linearFill(cr, 0, 0, 0, 26, g.light, g.mid, true);
  outline(cr, 2.2);
  cairo_move_to(cr, -2, 14 + droop);
  cairo_curve_to(cr, 20, 10, 44, 10, 66, 12 - droop);
  setColor(cr, g.dark);
  cairo_set_line_width(cr, 1.8);
  cairo_stroke(cr);
  for (int k = 0; k < 4; ++k)
  {
    const double x = 8 + k * 15;
    cairo_move_to(cr, x, 12);
    cairo_line_to(cr, x + 8, 4);
    cairo_move_to(cr, x, 13);
    cairo_line_to(cr, x + 8, 21);
  }
  setRgba(cr, g.dark, 0.6);
  cairo_set_line_width(cr, 1.2);
  cairo_stroke(cr);
  cairo_move_to(cr, 8, 6);
  cairo_curve_to(cr, 20, 1, 36, 1, 50, 4);
  setRgba(cr, rgb(255, 255, 255), dry ? 0.2 : 0.45);
  cairo_set_line_width(cr, 2);
  cairo_stroke(cr);
  if (flower && !dry)
  {
    // A pink rim: the flower's leaves.
    cairo_move_to(cr, 6, 22);
    cairo_curve_to(cr, 20, 28, 44, 28, 58, 22);
    setRgba(cr, rgb(255, 120, 200), 0.7);
    cairo_set_line_width(cr, 2.5);
    cairo_stroke(cr);
  }
}

void paintLadder(cairo_t* cr, bool dry, int seed)
{
  const Greens g = greens(dry);
  Rng rng(std::uint32_t(seed * 13 + 5));
  // A curly tendril behind.
  const double side = (seed % 2) ? 1.0 : -1.0;
  cairo_move_to(cr, 32, 30);
  cairo_curve_to(cr, 32 + side * 18, 26, 32 + side * 22, 40, 32 + side * 14, 42);
  cairo_curve_to(cr, 32 + side * 8, 44, 32 + side * 10, 36, 32 + side * 15, 37);
  setColor(cr, g.stem);
  cairo_set_line_width(cr, 2);
  cairo_stroke(cr);
  // The stem: leaves the tile where it came in, so creepers join up.
  for (int pass = 0; pass < 3; ++pass)
  {
    cairo_move_to(cr, 32, -2);
    cairo_curve_to(cr, 22, 18, 42, 40, 32, 66);
    setColor(cr, pass == 0 ? kInk : (pass == 1 ? g.mid : g.light));
    cairo_set_line_width(cr, pass == 0 ? 10 : (pass == 1 ? 6 : 1.8));
    cairo_stroke(cr);
  }
  // Leaves on alternate sides, as rungs to hold.
  leafShape(cr, 29, 14, kPi + 0.45 + rng.range(-0.2f, 0.2f), 24, 8, g.light, g.dark);
  leafShape(cr, 35, 30, -0.35 + rng.range(-0.2f, 0.2f), 24, 8, g.light, g.dark);
  leafShape(cr, 31, 50, kPi + 0.3 + rng.range(-0.2f, 0.2f), 20, 7, g.light, g.dark);
  leafShape(cr, 34, 58, -0.6, 14, 5, g.light, g.dark);
}

void paintStairs(cairo_t* cr, bool dry, int seed)
{
  const Greens g = greens(dry);
  Rng rng(std::uint32_t(seed * 19 + 11));
  const Color barkLight = dry ? rgb(170, 130, 80) : rgb(150, 160, 80);
  const Color barkDark = dry ? rgb(90, 60, 30) : rgb(70, 90, 40);
  roundedRect(cr, 1.5, 6, 61, 56.5, 8);
  linearFill(cr, 0, 6, 64, 64, barkLight, barkDark, true);
  outline(cr, 2.2);
  // Bark grooves, twisting.
  for (int k = 0; k < 4; ++k)
  {
    const double x = 10 + k * 14 + rng.range(-3, 3);
    cairo_move_to(cr, x, 14);
    cairo_curve_to(cr, x + 6, 28, x - 6, 42, x + 2, 60);
  }
  setRgba(cr, barkDark, 0.8);
  cairo_set_line_width(cr, 2.2);
  cairo_stroke(cr);
  // A knot.
  cairo_arc(cr, 20 + rng.range(0, 24), 40, 4, 0, 2 * kPi);
  setRgba(cr, barkDark, 0.9);
  cairo_fill(cr);
  // The leafy top of the step.
  cairo_move_to(cr, 0, 14);
  for (int k = 0; k <= 8; ++k)
    cairo_line_to(cr, k * 8.0, 2 + (k % 2) * 6 + rng.range(-1, 1));
  cairo_line_to(cr, 64, 16);
  cairo_curve_to(cr, 44, 20, 20, 20, 0, 16);
  cairo_close_path(cr);
  linearFill(cr, 0, 0, 0, 20, g.light, g.mid, true);
  outline(cr, 1.8);
  leafShape(cr, 8, 8, -kPi * 0.8, 16, 6, g.light, g.dark);
  leafShape(cr, 56, 8, -kPi * 0.2, 16, 6, g.light, g.dark);
}

// The giant flower's bloom: 320 x 200, its flat heart (a platform) on the
// line y = 70.
void paintBloom(cairo_t* cr)
{
  const double cx = 160, cy = 74;
  auto petal = [&](double a, double len, double width, Color light, Color dark) {
    cairo_save(cr);
    cairo_translate(cr, cx, cy);
    cairo_scale(cr, 1.0, 0.62);
    cairo_rotate(cr, a);
    cairo_move_to(cr, 0, 0);
    cairo_curve_to(cr, len * 0.35, -width, len * 0.9, -width * 0.9, len, 0);
    cairo_curve_to(cr, len * 0.9, width * 0.9, len * 0.35, width, 0, 0);
    cairo_close_path(cr);
    cairo_restore(cr);
    linearFill(cr, cx - len, cy - len * 0.6, cx + len, cy + len * 0.6, light, dark, true);
    outline(cr, 2.2);
  };
  radialGlow(cr, cx, cy, 160, rgb(255, 120, 220), 0.35);
  // Back petals, then the front ones drooping over.
  for (int k = 0; k < 7; ++k)
    petal(kPi + k * kPi / 6.0, 150, 42, rgb(255, 170, 230), rgb(200, 50, 150));
  for (int k = 0; k < 6; ++k)
    petal(0.18 + k * (kPi - 0.36) / 5.0, 140 + (k % 2) * 16, 40, rgb(255, 150, 220), rgb(170, 30, 120));
  // Petal veins.
  for (int k = 0; k < 6; ++k)
  {
    const double a = 0.18 + k * (kPi - 0.36) / 5.0;
    cairo_move_to(cr, cx + std::cos(a) * 40, cy + std::sin(a) * 40 * 0.62);
    cairo_line_to(cr, cx + std::cos(a) * 120, cy + std::sin(a) * 120 * 0.62);
  }
  setRgba(cr, rgb(255, 230, 250), 0.6);
  cairo_set_line_width(cr, 2);
  cairo_stroke(cr);
  // The flat golden heart and its stamens.
  cairo_save(cr);
  cairo_translate(cr, cx, cy);
  cairo_scale(cr, 1.0, 0.24);
  cairo_arc(cr, 0, 0, 74, 0, 2 * kPi);
  cairo_restore(cr);
  linearFill(cr, 0, cy - 18, 0, cy + 18, rgb(255, 236, 120), rgb(230, 150, 40), true);
  outline(cr, 2.2);
  Rng rng(1717u);
  for (int k = 0; k < 26; ++k)
  {
    const double a = rng.range(0, float(2 * kPi)), d = rng.range(10, 64);
    cairo_arc(cr, cx + std::cos(a) * d, cy + std::sin(a) * d * 0.22 - 2, 2.4, 0, 2 * kPi);
    setColor(cr, k % 3 ? rgb(255, 250, 200) : rgb(200, 100, 30));
    cairo_fill(cr);
  }
}

void paintBasil(cairo_t* cr)
{
  // The pot: 64 x 80, anchored at the middle of its base.
  cairo_move_to(cr, 14, 50);
  cairo_line_to(cr, 50, 50);
  cairo_line_to(cr, 45, 79);
  cairo_line_to(cr, 19, 79);
  cairo_close_path(cr);
  linearFill(cr, 14, 0, 50, 0, rgb(230, 130, 80), rgb(150, 70, 40), true);
  outline(cr, 2);
  roundedRect(cr, 10, 44, 44, 10, 3);
  linearFill(cr, 0, 44, 0, 54, rgb(240, 150, 100), rgb(170, 80, 46), true);
  outline(cr, 2);
  cairo_rectangle(cr, 16, 60, 32, 8);
  setColor(cr, rgb(250, 240, 220));
  cairo_fill(cr);
  // Basil: round, glossy leaves in pairs on short stems.
  const double leaves[][4] = {{32, 44, -1.57, 22}, {24, 40, -2.3, 18}, {40, 40, -0.8, 18}, {20, 30, -2.6, 16},
    {44, 30, -0.5, 16}, {30, 24, -1.9, 16}, {36, 22, -1.2, 15}, {32, 14, -1.57, 13}};
  for (const auto& l : leaves)
  {
    cairo_save(cr);
    cairo_translate(cr, l[0], l[1]);
    cairo_rotate(cr, l[2]);
    cairo_move_to(cr, 0, 0);
    cairo_curve_to(cr, l[3] * 0.2, -l[3] * 0.55, l[3] * 0.95, -l[3] * 0.45, l[3], 0);
    cairo_curve_to(cr, l[3] * 0.95, l[3] * 0.45, l[3] * 0.2, l[3] * 0.55, 0, 0);
    cairo_close_path(cr);
    cairo_restore(cr);
    linearFill(cr, 0, 6, 0, 46, rgb(150, 240, 110), rgb(40, 140, 50), true);
    outline(cr, 1.5);
  }
}

void paintNote(cairo_t* cr)
{
  cairo_save(cr);
  cairo_translate(cr, 12, 38);
  cairo_rotate(cr, -0.35);
  cairo_scale(cr, 1.0, 0.72);
  cairo_arc(cr, 0, 0, 8, 0, 2 * kPi);
  cairo_restore(cr);
  setColor(cr, rgb(255, 230, 250));
  cairo_fill_preserve(cr);
  outline(cr, 2, rgb(120, 30, 100));
  cairo_move_to(cr, 19, 36);
  cairo_line_to(cr, 19, 6);
  cairo_curve_to(cr, 24, 12, 34, 14, 30, 26);
  setColor(cr, rgb(120, 30, 100));
  cairo_set_line_width(cr, 5);
  cairo_stroke_preserve(cr);
  setColor(cr, rgb(255, 230, 250));
  cairo_set_line_width(cr, 2.5);
  cairo_stroke(cr);
}

// A grow lamp's hood: 80 x 44, anchored at the middle of its top.
void paintLamp(cairo_t* cr, bool lit)
{
  roundedRect(cr, 32, 0, 16, 8, 3);
  setColor(cr, kSteelDark);
  cairo_fill(cr);
  cairo_move_to(cr, 26, 6);
  cairo_line_to(cr, 54, 6);
  cairo_line_to(cr, 76, 30);
  cairo_line_to(cr, 4, 30);
  cairo_close_path(cr);
  linearFill(cr, 0, 6, 0, 30, rgb(250, 252, 255), rgb(170, 184, 200), true);
  outline(cr, 2);
  cairo_move_to(cr, 30, 10);
  cairo_line_to(cr, 14, 27);
  setRgba(cr, rgb(255, 255, 255), 0.8);
  cairo_set_line_width(cr, 2);
  cairo_stroke(cr);
  roundedRect(cr, 8, 30, 64, 9, 4.5);
  setColor(cr, lit ? rgb(255, 214, 248) : rgb(84, 70, 92));
  cairo_fill_preserve(cr);
  outline(cr, 1.6);
  if (lit)
    radialGlow(cr, 40, 34, 40, kLampPink, 0.6);
}

// The light falling from a lit lamp: 320 x 480, anchored at the middle of
// its top. Drawn additively.
void paintCone(cairo_t* cr)
{
  cairo_move_to(cr, 128, 0);
  cairo_line_to(cr, 192, 0);
  cairo_line_to(cr, 320, 480);
  cairo_line_to(cr, 0, 480);
  cairo_close_path(cr);
  cairo_pattern_t* g = cairo_pattern_create_linear(0, 0, 0, 480);
  cairo_pattern_add_color_stop_rgba(g, 0, 1.0, 0.45, 0.85, 0.5);
  cairo_pattern_add_color_stop_rgba(g, 0.5, 1.0, 0.35, 0.8, 0.18);
  cairo_pattern_add_color_stop_rgba(g, 1, 1.0, 0.3, 0.8, 0.0);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
  // Softer sides: a narrower, brighter core.
  cairo_move_to(cr, 140, 0);
  cairo_line_to(cr, 180, 0);
  cairo_line_to(cr, 230, 360);
  cairo_line_to(cr, 90, 360);
  cairo_close_path(cr);
  g = cairo_pattern_create_linear(0, 0, 0, 360);
  cairo_pattern_add_color_stop_rgba(g, 0, 1.0, 0.75, 0.95, 0.35);
  cairo_pattern_add_color_stop_rgba(g, 1, 1.0, 0.5, 0.9, 0.0);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
}

// A shootable switch: 52 x 52, anchored at its centre. lit 0 red, 1 green.
void paintSwitch(cairo_t* cr, int lit)
{
  cairo_arc(cr, 26, 26, 23, 0, 2 * kPi);
  linearFill(cr, 0, 3, 0, 49, rgb(236, 240, 246), rgb(120, 132, 150), true);
  outline(cr, 2);
  cairo_arc(cr, 26, 26, 17, 0, 2 * kPi);
  setColor(cr, rgb(40, 46, 58));
  cairo_fill(cr);
  for (int k = 0; k < 8; ++k)
  {
    const double a = k * kPi / 4.0;
    cairo_move_to(cr, 26 + std::cos(a) * 18, 26 + std::sin(a) * 18);
    cairo_line_to(cr, 26 + std::cos(a) * 22, 26 + std::sin(a) * 22);
  }
  setColor(cr, kSteelDark);
  cairo_set_line_width(cr, 2);
  cairo_stroke(cr);
  const Color c = lit ? rgb(90, 255, 110) : rgb(255, 60, 60);
  radialGlow(cr, 26, 26, 18, c, 0.8);
  cairo_arc(cr, 26, 26, 10, 0, 2 * kPi);
  setColor(cr, c);
  cairo_fill(cr);
  cairo_arc(cr, 23, 23, 3.5, 0, 2 * kPi);
  setRgba(cr, rgb(255, 255, 255), 0.85);
  cairo_fill(cr);
}

// A puff of spores: 128 x 128, anchored at its centre.
void paintPuff(cairo_t* cr, bool carrier)
{
  const Color c = carrier ? rgb(150, 235, 80) : rgb(250, 226, 180);
  cairo_pattern_t* g = cairo_pattern_create_radial(64, 64, 4, 64, 64, 62);
  cairo_pattern_add_color_stop_rgba(g, 0, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, 0.55);
  cairo_pattern_add_color_stop_rgba(g, 0.6, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, 0.3);
  cairo_pattern_add_color_stop_rgba(g, 1, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, 0.0);
  cairo_set_source(cr, g);
  cairo_arc(cr, 64, 64, 62, 0, 2 * kPi);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
  Rng rng(carrier ? 1771u : 1777u);
  for (int k = 0; k < 40; ++k)
  {
    const double a = rng.range(0, float(2 * kPi)), d = rng.range(0, 52);
    cairo_arc(cr, 64 + std::cos(a) * d, 64 + std::sin(a) * d, rng.range(1.2f, 2.6f), 0, 2 * kPi);
    setRgba(cr, carrier ? rgb(60, 140, 30) : rgb(200, 150, 90), 0.8);
    cairo_fill(cr);
  }
}

// A hanging seed pod, w x h px plus 48 px of vine above (anchored at the
// rect's top-left). dmg 0..3: cracks.
void paintPod(cairo_t* cr, double w, double h, int dmg)
{
  cairo_translate(cr, 0, 48);
  const double cx = w * 0.5;
  // The wall vine it hangs from.
  cairo_move_to(cr, cx - 10, -48);
  cairo_curve_to(cr, cx + 14, -30, cx - 12, -14, cx, 6);
  setColor(cr, kInk);
  cairo_set_line_width(cr, 9);
  cairo_stroke_preserve(cr);
  setColor(cr, rgb(70, 130, 46));
  cairo_set_line_width(cr, 5);
  cairo_stroke(cr);
  leafShape(cr, cx + 2, -26, -0.4, 22, 7, rgb(150, 230, 100), rgb(40, 100, 36));
  leafShape(cr, cx - 4, -12, kPi + 0.5, 20, 7, rgb(150, 230, 100), rgb(40, 100, 36));
  // The pod: a fat teardrop.
  const double top = 4, bottom = h - 3, rx = w * 0.44;
  cairo_move_to(cr, cx, top);
  cairo_curve_to(cr, cx + rx * 0.6, top + h * 0.15, cx + rx * 1.1, bottom - h * 0.4, cx + rx * 0.7, bottom - h * 0.08);
  cairo_curve_to(cr, cx + rx * 0.4, bottom + 2, cx - rx * 0.4, bottom + 2, cx - rx * 0.7, bottom - h * 0.08);
  cairo_curve_to(cr, cx - rx * 1.1, bottom - h * 0.4, cx - rx * 0.6, top + h * 0.15, cx, top);
  cairo_close_path(cr);
  cairo_pattern_t* g = cairo_pattern_create_radial(cx - rx * 0.3, h * 0.45, 4, cx, h * 0.6, h * 0.7);
  cairo_pattern_add_color_stop_rgb(g, 0, 0.72, 0.85, 0.36);
  cairo_pattern_add_color_stop_rgb(g, 0.6, 0.45, 0.55, 0.2);
  cairo_pattern_add_color_stop_rgb(g, 1, 0.35, 0.24, 0.12);
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  outline(cr, 2.4);
  // Ribs.
  for (int k = -2; k <= 2; ++k)
  {
    cairo_move_to(cr, cx + k * 2.0, top + 6);
    cairo_curve_to(cr, cx + k * rx * 0.45, h * 0.4, cx + k * rx * 0.42, h * 0.75, cx + k * rx * 0.25, bottom - 4);
  }
  setRgba(cr, rgb(60, 70, 24), 0.6);
  cairo_set_line_width(cr, 2);
  cairo_stroke(cr);
  cairo_save(cr);
  cairo_translate(cr, cx - rx * 0.4, h * 0.42);
  cairo_scale(cr, 1.0, 2.2);
  cairo_arc(cr, 0, 0, rx * 0.12, 0, 2 * kPi);
  cairo_restore(cr);
  setRgba(cr, rgb(255, 255, 230), 0.4);
  cairo_fill(cr);
  // Cracks, glowing from the seeds inside.
  for (int k = 0; k < dmg; ++k)
  {
    const double sx = cx + (k - 1) * rx * 0.45, sy = h * (0.35 + 0.12 * k);
    cairo_move_to(cr, sx, sy);
    cairo_line_to(cr, sx + 7, sy + 10);
    cairo_line_to(cr, sx - 3, sy + 18);
    cairo_line_to(cr, sx + 6, sy + 28);
    cairo_line_to(cr, sx + 1, sy + 36);
    setColor(cr, rgb(40, 24, 10));
    cairo_set_line_width(cr, 4);
    cairo_stroke_preserve(cr);
    setColor(cr, rgb(255, 230, 120));
    cairo_set_line_width(cr, 1.6);
    cairo_stroke(cr);
  }
}

// A thorn wall: brambles filling w x h px; density 0 (thin) .. 2 (dense).
void paintThorns(cairo_t* cr, double w, double h, int density, int seed)
{
  Rng rng(std::uint32_t(seed * 7 + 1));
  // Dark tangle behind.
  roundedRect(cr, 2, 2, w - 4, h - 4, 14);
  setRgba(cr, rgb(30, 40, 20), 0.35 + 0.2 * density);
  cairo_fill(cr);
  const int stems = int((3 + 4 * density) * h / 64.0 * std::max(1.0, w / 80.0));
  for (int s = 0; s < stems; ++s)
  {
    const double x0 = rng.range(0, float(w)), y0 = rng.range(-10, float(h) + 10);
    const double x3 = rng.range(0, float(w)), y3 = rng.range(-10, float(h) + 10);
    const double x1 = rng.range(-10, float(w) + 10), y1 = rng.range(0, float(h));
    const double x2 = rng.range(-10, float(w) + 10), y2 = rng.range(0, float(h));
    const bool brown = s % 3 == 0;
    cairo_move_to(cr, x0, y0);
    cairo_curve_to(cr, x1, y1, x2, y2, x3, y3);
    setColor(cr, kInk);
    cairo_set_line_width(cr, 8);
    cairo_stroke_preserve(cr);
    setColor(cr, brown ? rgb(120, 80, 40) : rgb(70, 110, 40));
    cairo_set_line_width(cr, 5);
    cairo_stroke(cr);
    // Thorns along it.
    for (int k = 1; k < 7; ++k)
    {
      const double t = k / 7.0, u = 1 - t;
      const double px = u * u * u * x0 + 3 * u * u * t * x1 + 3 * u * t * t * x2 + t * t * t * x3;
      const double py = u * u * u * y0 + 3 * u * u * t * y1 + 3 * u * t * t * y2 + t * t * t * y3;
      const double a = rng.range(0, float(2 * kPi));
      cairo_move_to(cr, px + std::cos(a + 1.4) * 3, py + std::sin(a + 1.4) * 3);
      cairo_line_to(cr, px + std::cos(a) * 11, py + std::sin(a) * 11);
      cairo_line_to(cr, px + std::cos(a - 1.4) * 3, py + std::sin(a - 1.4) * 3);
      cairo_close_path(cr);
      setColor(cr, rgb(236, 220, 180));
      cairo_fill_preserve(cr);
      setColor(cr, kInk);
      cairo_set_line_width(cr, 1);
      cairo_stroke(cr);
    }
  }
  for (int k = 0; k < 2 + density * 2; ++k)
    leafShape(cr, rng.range(6, float(w) - 6), rng.range(8, float(h) - 8), rng.range(0, float(2 * kPi)), 16, 6,
      rgb(130, 200, 80), rgb(40, 90, 30));
}

} // namespace

void World::drawGreenBack(Renderer& r, float camX, float camY, int frame) const
{
  const auto& g = mGreen;
  if (!g.on)
    return;
  auto solidBlock = [&](int bx, int by) {
    return mMap.solid(bx * kCellsPerTile, by * kCellsPerTile) || mMap.solidTop(bx * kCellsPerTile, by * kCellsPerTile);
  };

  // Trench water.
  for (const auto& pool : g.pools)
  {
    const float x = float(pool.x0) * kTilePx - camX, y = float(pool.y0) * kTilePx - camY;
    const float w = float(pool.x1 - pool.x0 + 1) * kTilePx, h = float(pool.y1 - pool.y0 + 1) * kTilePx;
    if (!visible(x, y, w, h))
      continue;
    const float top = y + 14.0f;
    r.fillRect(x, top, w, y + h - top, rgba(60, 150, 220, 110));
    r.fillRect(x, top + (y + h - top) * 0.5f, w, (y + h - top) * 0.5f, rgba(20, 70, 140, 80));
    for (float k = x; k < x + w; k += 8.0f)
    {
      const float wy = top + std::sin((k - x) * 0.05f + float(frame) * 0.12f) * 2.5f;
      r.fillRect(k, wy - 2.0f, 8.0f, 4.0f, rgba(200, 240, 255, 170));
    }
    for (int i = 0; i < 6; ++i)
    {
      const unsigned hsh = hashCell(pool.x0 + i, pool.y0);
      const float rx = x + float(hsh % 1000u) / 1000.0f * w;
      const float phase = std::fmod(float(frame + int(hsh % 97u)) / 60.0f, 1.0f);
      const float rw = 10.0f + phase * 36.0f;
      r.fillRect(rx - rw * 0.5f, top + 6.0f, rw, 2.0f, rgba(220, 250, 255, int(150 * (1.0f - phase))));
    }
  }

  // Plants.
  for (const auto& pl : g.plants)
  {
    const int n = int(pl.tiles.size());
    if (n == 0 || pl.shown <= 0)
      continue;
    int topRow = pl.tiles[0].second;
    for (const auto& t : pl.tiles)
      topRow = std::min(topRow, t.second);
    const bool bloom = pl.kind == PlantKind::Flower && pl.shown >= n;
    for (int i = 0; i < pl.shown && i < n; ++i)
    {
      const auto [bx, by] = pl.tiles[std::size_t(i)];
      if (bloom && by == topRow)
        continue;
      const float cx = (float(bx) + 0.5f) * kTilePx - camX, cy = (float(by) + 0.5f) * kTilePx - camY;
      if (!visible(cx - 40.0f, cy - 40.0f, 80.0f, 80.0f))
        continue;
      const int ahead = pl.prog - i * kUnit;
      const bool dry = !pl.growing && ahead <= 24 * n;
      const int var = int(hashCell(bx, by) % 3u);
      const int kind = int(pl.kind);
      const std::string key = "plant/" + std::to_string(kind) + "/" + std::to_string(var) + (dry ? "d" : "");
      const Texture& tex = baked(mArt, r, key, 64, 64, 32.0f, 32.0f, [&](cairo_t* cr) {
        switch (pl.kind)
        {
          case PlantKind::Bridge: paintBridge(cr, dry, var); break;
          case PlantKind::Leaf: paintLeaf(cr, dry, var, false); break;
          case PlantKind::Ladder: paintLadder(cr, dry, var); break;
          case PlantKind::Stairs: paintStairs(cr, dry, var); break;
          case PlantKind::Flower: paintLeaf(cr, dry, var, true); break;
        }
      });
      DrawOpts o;
      const float f = std::clamp(float(ahead) / float(kUnit), 0.0f, 1.0f);
      if (pl.growing && f < 1.0f)
      {
        o.scale = 0.4f + 0.6f * f;
        o.alpha = 0.3f + 0.7f * f;
      }
      r.draw(tex, cx, cy, o);
    }
    if (bloom)
    {
      int x0 = 1 << 30, x1 = -(1 << 30);
      for (const auto& t : pl.tiles)
        if (t.second == topRow)
        {
          x0 = std::min(x0, t.first);
          x1 = std::max(x1, t.first);
        }
      const float w = float(x1 - x0 + 1) * kTilePx;
      const float cx = float(x0) * kTilePx + w * 0.5f - camX, sy = float(topRow) * kTilePx - camY;
      if (visible(cx - w, sy - 120.0f, w * 2.0f, 260.0f))
      {
        const Texture& tex = baked(mArt, r, "bloom", 320, 200, 160.0f, 74.0f, paintBloom);
        DrawOpts o;
        o.scale = (w + 48.0f) / 300.0f;
        o.scale *= 1.0f + 0.015f * std::sin(float(frame) * 0.08f);
        r.draw(tex, cx, sy, o);
      }
    }
  }

  // Switches, on a post or a bracket.
  for (std::size_t si = 0; si < g.switches.size(); ++si)
  {
    const auto& s = g.switches[si];
    const float cx = (float(s.bx) + 0.5f) * kTilePx - camX, cy = (float(s.by) + 0.5f) * kTilePx - camY;
    if (!visible(cx - 200.0f, cy - 200.0f, 400.0f, 400.0f))
      continue;
    bool lit = false;
    for (const auto& l : g.lamps)
      lit = lit || (l.sw == int(si) && l.lit);
    int floor = -1;
    for (int d = 1; d <= 3 && floor < 0; ++d)
      if (solidBlock(s.bx, s.by + d))
        floor = s.by + d;
    if (floor >= 0)
    {
      const float fy = float(floor) * kTilePx - camY;
      r.fillRect(cx - 5.0f, cy + 18.0f, 10.0f, fy - cy - 18.0f, kSteelDark);
      r.fillRect(cx - 3.0f, cy + 18.0f, 3.0f, fy - cy - 18.0f, kSteel);
      r.fillRect(cx - 14.0f, fy - 6.0f, 28.0f, 6.0f, kSteelDark);
    }
    else
    {
      // The nearest wall or ceiling within 3 blocks.
      int best = 99, dx = 0, dy = 0;
      for (int d = 1; d <= 3; ++d)
      {
        if (d < best && solidBlock(s.bx, s.by - d))
        {
          best = d;
          dx = 0;
          dy = -1;
        }
        if (d < best && solidBlock(s.bx - d, s.by))
        {
          best = d;
          dx = -1;
          dy = 0;
        }
        if (d < best && solidBlock(s.bx + d, s.by))
        {
          best = d;
          dx = 1;
          dy = 0;
        }
      }
      if (best < 99)
      {
        float ex = cx, ey = cy;
        if (dy < 0)
          ey = float(s.by - best + 1) * kTilePx - camY;
        else if (dx < 0)
          ex = float(s.bx - best + 1) * kTilePx - camX;
        else
          ex = float(s.bx + best) * kTilePx - camX;
        r.drawLine(cx, cy, ex, ey, 8.0f, kSteelDark);
        r.drawLine(cx, cy, ex, ey, 3.0f, kSteel);
        if (dy < 0)
          r.fillRect(ex - 12.0f, ey, 24.0f, 6.0f, kSteelDark);
        else
          r.fillRect(ex - 3.0f, ey - 12.0f, 6.0f, 24.0f, kSteelDark);
      }
    }
    const Texture& tex =
      baked(mArt, r, std::string("switch/") + (lit ? "1" : "0"), 52, 52, 26.0f, 26.0f, [&](cairo_t* cr) {
        paintSwitch(cr, lit ? 1 : 0);
      });
    r.draw(tex, cx, cy);
    drawGlow(r, mArt, cx, cy, 30.0f, lit ? rgb(90, 255, 110) : rgb(255, 60, 60), 0.35f);
    if (s.flash > 0)
      drawGlow(r, mArt, cx, cy, 34.0f + float(s.flash) * 2.0f, rgb(255, 255, 255), 0.9f);
  }

  // Grow lamps: the cable, the hood, the light.
  for (const auto& l : g.lamps)
  {
    const float cx = (float(l.bx) + 0.5f) * kTilePx - camX, top = float(l.by) * kTilePx + 10.0f - camY;
    // Its floor (and the basil's pot on it), its ceiling.
    int floor = -1;
    for (int d = 1; d <= 24 && floor < 0; ++d)
      if (l.by + d >= mLevel->height || solidBlock(l.bx, l.by + d))
        floor = l.by + d;
    int ceil = l.by;
    for (int d = 0; d <= 24; ++d)
      if (l.by - d < 0 || solidBlock(l.bx, l.by - d))
      {
        ceil = l.by - d + 1;
        break;
      }
    const float fy = float(std::max(floor, l.by + 1)) * kTilePx - camY;
    const float cy = float(ceil) * kTilePx - camY;
    if (!visible(cx - 200.0f, std::min(cy, top) - 40.0f, 400.0f, fy - std::min(cy, top) + 120.0f))
      continue;
    bool on = l.lit;
    float bright = 1.0f;
    if (on && !l.locked && l.timer > 0 && l.left <= 22)
    {
      const unsigned hsh = hashCell(l.bx, frame / 2);
      on = hsh % 3u != 0u;
      bright = 0.5f + float(hsh % 50u) / 100.0f;
    }
    if (on)
    {
      const Texture& cone = baked(mArt, r, "cone", 320, 480, 160.0f, 0.0f, paintCone);
      DrawOpts o;
      o.blend = Blend::Add;
      o.alpha = (l.locked ? 0.95f : 0.75f) * bright;
      o.scale = std::clamp((fy - top) / 420.0f, 0.5f, 1.3f);
      r.draw(cone, cx, top + 34.0f, o);
      drawGlow(r, mArt, cx, fy - 6.0f, 90.0f * o.scale, kLampPink, 0.25f * bright);
    }
    r.fillRect(cx - 2.0f, cy, 4.0f, std::max(0.0f, top - cy), rgb(40, 44, 54));
    const Texture& hood =
      baked(mArt, r, std::string("lamp/") + (on ? "1" : "0"), 80, 44, 40.0f, 0.0f, [&](cairo_t* cr) {
        paintLamp(cr, on);
      });
    r.draw(hood, cx, top);
    if (on)
      drawGlow(r, mArt, cx, top + 34.0f, 60.0f, kLampPink, 0.55f * bright);
    if (l.basil)
    {
      const Texture& pot = baked(mArt, r, "basil", 64, 80, 32.0f, 80.0f, paintBasil);
      r.draw(pot, cx, fy);
      if (l.lit)
      {
        // The name plate on a stick in its soil.
        const float py = fy - 128.0f, px = cx + 16.0f;
        r.fillRect(px - 1.5f, py + 34.0f, 3.0f, 64.0f, rgb(150, 110, 70));
        r.fillRect(px - 64.0f, py, 128.0f, 36.0f, rgb(60, 44, 30));
        r.fillRect(px - 62.0f, py + 2.0f, 124.0f, 32.0f, rgb(236, 220, 180));
        r.drawText("STATION'S OLDEST", px, py + 4.0f, {10.0f, rgb(60, 40, 20)}, Align::Center);
        r.drawText("CREW MEMBER", px, py + 18.0f, {10.0f, rgb(60, 40, 20)}, Align::Center);
      }
      if (l.hum > 0)
      {
        const Texture& note = baked(mArt, r, "note", 40, 48, 20.0f, 24.0f, paintNote);
        DrawOpts o;
        o.alpha = std::min(1.0f, float(l.hum) / 10.0f);
        const float rise = float(30 - std::min(30, l.hum)) * 1.2f;
        r.draw(note, cx + 26.0f + std::sin(float(frame) * 0.2f) * 6.0f, top - 16.0f - rise, o);
      }
    }
  }
}

void World::drawGreenFront(Renderer& r, float camX, float camY, int frame) const
{
  const auto& g = mGreen;
  if (!g.on)
    return;
  // Spore clouds.
  for (std::size_t ci = 0; ci < g.clouds.size(); ++ci)
  {
    const auto& c = g.clouds[ci];
    const float size = 6.0f * kCellPx;
    const float x = c.x * kCellPx - camX, y = c.y * kCellPx - camY;
    if (!visible(x, y, size, size) || c.life <= 0)
      continue;
    const float a = std::min(1.0f, float(45 - c.life) / 8.0f + 0.15f) * std::min(1.0f, float(c.life) / 12.0f);
    const Texture& puff = baked(mArt, r, std::string("puff/") + (c.carrier ? "1" : "0"), 128, 128, 64.0f, 64.0f,
      [&](cairo_t* cr) { paintPuff(cr, c.carrier); });
    const float grow = 0.8f + 0.4f * float(45 - c.life) / 45.0f;
    for (int k = 0; k < 5; ++k)
    {
      const float ang = float(k) * 1.2566f + float(frame) * 0.03f + float(ci);
      const float d = k == 0 ? 0.0f : size * 0.22f;
      DrawOpts o;
      o.alpha = a * (k == 0 ? 0.9f : 0.7f);
      o.scale = grow * (k == 0 ? 1.2f : 0.85f);
      o.angle = float(frame * (k % 2 ? 2 : -2) + k * 40);
      r.draw(puff, x + size * 0.5f + std::cos(ang) * d, y + size * 0.5f + std::sin(ang) * d, o);
    }
  }

  // The Hedge Trimmer's cone: a toothed bar chattering, clippings flying.
  if (g.trim > 0)
  {
    const CellBox b = trimBox();
    const float x = float(b.x) * kCellPx - camX, y = float(b.y) * kCellPx - camY;
    const float w = float(b.w) * kCellPx, h = float(b.h) * kCellPx;
    const auto& p = mPlayer;
    const bool up = b.bottom() < p.y - 3;
    const float dir = float(g.trimFace);
    drawGlow(r, mArt, x + w * 0.5f, y + h * 0.5f, std::max(w, h) * 0.6f, rgb(200, 255, 170), 0.25f);
    const int jig = (frame + g.trim) % 2;
    if (up)
    {
      const float bx = x + w * 0.5f;
      r.fillRect(bx - 6.0f, y + 6.0f, 12.0f, h - 6.0f, kSteelDark);
      r.fillRect(bx - 3.0f, y + 6.0f, 4.0f, h - 6.0f, kSteel);
      for (float ty = y + 10.0f + float(jig) * 6.0f; ty < y + h - 6.0f; ty += 12.0f)
      {
        r.drawLine(bx - 5.0f, ty, bx - 18.0f, ty - 5.0f, 7.0f, kSteelDark);
        r.drawLine(bx + 5.0f, ty, bx + 18.0f, ty - 5.0f, 7.0f, kSteelDark);
        r.drawLine(bx - 6.0f, ty, bx - 16.0f, ty - 5.0f, 4.0f, rgb(236, 242, 250));
        r.drawLine(bx + 6.0f, ty, bx + 16.0f, ty - 5.0f, 4.0f, rgb(236, 242, 250));
      }
    }
    else
    {
      const float by = y + h * 0.5f;
      const float x0 = dir > 0 ? x : x + 6.0f, len = w - 6.0f;
      r.fillRect(x0, by - 6.0f, len, 12.0f, kSteelDark);
      r.fillRect(x0, by - 4.0f, len, 4.0f, kSteel);
      // The rounded tip.
      r.fillRect(dir > 0 ? x0 + len : x0 - 6.0f, by - 4.0f, 6.0f, 8.0f, kSteelDark);
      for (float tx = x0 + 4.0f + float(jig) * 6.0f; tx < x0 + len - 4.0f; tx += 12.0f)
      {
        r.drawLine(tx, by - 5.0f, tx + 5.0f * dir, by - 18.0f, 7.0f, kSteelDark);
        r.drawLine(tx, by + 5.0f, tx + 5.0f * dir, by + 18.0f, 7.0f, kSteelDark);
        r.drawLine(tx, by - 6.0f, tx + 5.0f * dir, by - 16.0f, 4.0f, rgb(236, 242, 250));
        r.drawLine(tx, by + 6.0f, tx + 5.0f * dir, by + 16.0f, 4.0f, rgb(236, 242, 250));
      }
    }
    // Leaf clippings.
    for (int k = 0; k < 10; ++k)
    {
      const unsigned hsh = hashCell(k, (frame + k * 3) / 6);
      const float t = float((frame + k * 3) % 6) / 6.0f;
      float px = x + float(hsh % 100u) / 100.0f * w, py = y + float((hsh >> 8) % 100u) / 100.0f * h;
      if (up)
        py -= t * 30.0f;
      else
      {
        px += dir * t * 30.0f;
        py -= t * 10.0f - t * t * 20.0f;
      }
      const float ang = float(hsh % 628u) / 100.0f + float(frame) * 0.4f;
      const Color c = k % 3 ? rgba(110, 210, 70, int(230 * (1.0f - t))) : rgba(190, 250, 120, int(230 * (1.0f - t)));
      r.drawLine(px - std::cos(ang) * 6.0f, py - std::sin(ang) * 6.0f, px + std::cos(ang) * 6.0f,
        py + std::sin(ang) * 6.0f, 4.0f, c);
    }
  }
}

void World::drawGreenHud(Renderer& r, int frame) const
{
  const auto& g = mGreen;
  if (!g.grow)
    return;
  const float x = float(kScreenW) * 0.5f - 120.0f, y = 150.0f;
  r.fillRect(x, y, 240, 48, rgba(10, 30, 14, 190));
  const bool flash = g.sizeFlash > 0 && (frame / 3) % 2;
  char buf[32];
  std::snprintf(buf, sizeof(buf), "SIZE x%.2f", double(g.scale()));
  r.drawText(buf, x + 120, y + 8, {24.0f, flash ? rgb(255, 255, 255) : rgb(170, 255, 140), rgb(4, 20, 8)},
    Align::Center);
  // Pips for the five steps.
  for (int k = 0; k < 5; ++k)
    r.fillRect(x + 70.0f + float(k) * 22.0f, y + 40.0f, 14.0f, 4.0f,
      k <= g.size ? rgb(170, 255, 140) : rgba(170, 255, 140, 60));
}

void World::drawGreenBreakable(Renderer& r, const Breakable& b, float camX, float camY, int frame) const
{
  const float x = float(b.x0) * kTilePx - camX, y = float(b.y0) * kTilePx - camY;
  const int wpx = (b.x1 - b.x0 + 1) * int(kTilePx), hpx = (b.y1 - b.y0 + 1) * int(kTilePx);
  const float w = float(wpx), h = float(hpx);
  if (!visible(x, y - 48.0f, w, h + 48.0f))
    return;
  const std::string size = std::to_string(wpx) + "x" + std::to_string(hpx);
  if (b.look == 11)
  {
    const int dmg = std::clamp(4 - b.hp, 0, 3);
    const Texture& tex = baked(mArt, r, "pod/" + size + "/" + std::to_string(dmg), wpx, hpx + 48, w * 0.5f, 0.0f,
      [&](cairo_t* cr) { paintPod(cr, w, h, dmg); });
    DrawOpts o;
    o.angle = std::sin(float(frame) * 0.04f + float(b.x0)) * 2.0f;
    r.draw(tex, x + w * 0.5f, y - 48.0f, o);
    return;
  }
  // Look 12: a thorn wall, thinning as it is cut back.
  const int density = b.hp > 8 ? 2 : (b.hp > 4 ? 1 : 0);
  const int seed = b.x0 * 31 + b.y0;
  const Texture& tex = baked(mArt, r, "thorns/" + size + "/" + std::to_string(density) + "/" + std::to_string(seed),
    wpx + 24, hpx + 8, 12.0f, 4.0f, [&](cairo_t* cr) { paintThorns(cr, w + 24.0f, h + 8.0f, density, seed); });
  r.draw(tex, x, y);
}

} // namespace gr
