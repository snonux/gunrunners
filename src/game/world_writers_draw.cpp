// Level 38, Writers' Room: drawing. See world_writers.cpp for the logic.
//
// Behind the tiles (drawWritersSet, writers_draw.hpp), keyed to the map:
// the doorway with its coat rack, sign and the sticky note; the desks, oak
// with drawer pedestals, their tops cluttered with paper stacks, mugs,
// pencil cups and green desk lamps; the dark under the FLOOR plank; the
// corkboard in its frame with index cards and loose red yarn, the rejected
// pitch cards (spinning when shot); the filing cabinet's shell and its
// snack drawer; the top shelf's binders; the big typewriter behind its
// keys, its roller and the paper rising out of it with the sentence; the
// final draft's script page, the bound FINAL DRAFT with PAGE 42 on its
// cover, the press's housing and its warning sign, the line THE RUNNERS
// EXIT over the door. In the Breakout bonus, a corkboard wall.
//
// Behind the runner (drawWritersBack): what the words name (the inked
// brick WALLs, the LANCE WINS door, the FLOOR plank, a ladder or the
// pencils fading out or sketched back), the desks' carpet, the typewriter
// keys (the next one the sentence wants lit) and the bridge of typed paper,
// the red strings and their pins, the inkwells, the original pitch. In
// front (drawWritersFront): Typos, Deadlines, Writer's Blocks and the
// sketch guard, the filing cabinet's front, the rubber-stamp press, the
// handwritten words in every state (struck through in red, written back
// letter by letter, sketched in pencil), the egg and its chick, and the
// Breakout ball and the runner's ruler. The HUD (drawWritersHud): the
// Typo's scrambled word, the Breakout balls and bricks.

#include "game/world.hpp"

#include "assets/art.hpp"
#include "assets/enemy_art.hpp"
#include "base/math.hpp"
#include "game/writers_draw.hpp"
#include "render/vector.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <initializer_list>
#include <string>

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
constexpr float kTilePx = float(kTileSize) * kPixelScale; // 64
constexpr Color kHudInk = rgb(10, 8, 20);
constexpr Color kInk = rgb(24, 22, 34);
constexpr Color kRedInk = rgb(222, 28, 40);
constexpr Color kRedHi = rgb(255, 120, 120);
constexpr Color kPaper = rgb(248, 243, 228);
constexpr Color kPaperLo = rgb(210, 200, 178);
constexpr Color kGraphite = rgb(70, 70, 84);
constexpr Color kWood = rgb(152, 104, 62);
constexpr Color kWoodHi = rgb(204, 156, 100);
constexpr Color kWoodLo = rgb(88, 56, 30);
constexpr Color kSteel = rgb(150, 156, 168);
constexpr Color kSteelLo = rgb(84, 88, 100);
constexpr Color kCork = rgb(196, 150, 100);
constexpr Color kPencil = rgb(244, 186, 36);
constexpr Color kPencilLo = rgb(190, 124, 16);
constexpr Color kLamp = rgb(255, 214, 140);
constexpr Color kNote = rgb(255, 232, 110);

float lerpCells(int prev, int cur, float alpha) { return (float(prev) + float(cur - prev) * alpha) * kCellPx; }

bool offScreen(float x, float y, float w, float h, float m = 96.0f)
{
  return x > float(kScreenW) + m || x + w < -m || y > float(kScreenH) + m || y + h < -m;
}

float unit(std::uint32_t h) { return float(h % 10000u) / 10000.0f; }

const Sprite& bakedSprite(const Art& art, const Renderer& r, const std::string& key, int w, int h, float ax, float ay,
  const std::function<void(cairo_t*)>& paint)
{
  const std::string id = "~writers/" + key;
  auto it = art.styled.find(id);
  if (it != art.styled.end())
    return it->second;
  VectorImage img(std::max(1, w), std::max(1, h));
  paint(img.cr());
  Sprite s;
  s.facing[0] = img.toTexture(r, ax, ay);
  s.facing[1] = img.toTexture(r, float(w) - ax, ay, true);
  return art.styled.emplace(id, std::move(s)).first->second;
}

const Texture& baked(const Art& art, const Renderer& r, const std::string& key, int w, int h, float ax, float ay,
  const std::function<void(cairo_t*)>& paint)
{
  return bakedSprite(art, r, key, w, h, ax, ay, paint).facing[0];
}

void setRgba(cairo_t* cr, Color c, double a)
{
  cairo_set_source_rgba(cr, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, a);
}

void stop(cairo_pattern_t* g, double at, Color c, double a = 1.0)
{
  cairo_pattern_add_color_stop_rgba(g, at, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, a);
}

void circle(cairo_t* cr, double x, double y, double r)
{
  cairo_new_sub_path(cr);
  cairo_arc(cr, x, y, r, 0, 2 * kPi);
}

void ellipse(cairo_t* cr, double x, double y, double rx, double ry)
{
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_scale(cr, std::max(0.01, rx), std::max(0.01, ry));
  cairo_new_sub_path(cr);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_restore(cr);
}

void roundRect(cairo_t* cr, double x, double y, double w, double h, double r)
{
  r = std::min(r, std::min(w, h) * 0.5);
  cairo_new_sub_path(cr);
  cairo_arc(cr, x + w - r, y + r, r, -kPi * 0.5, 0);
  cairo_arc(cr, x + w - r, y + h - r, r, 0, kPi * 0.5);
  cairo_arc(cr, x + r, y + h - r, r, kPi * 0.5, kPi);
  cairo_arc(cr, x + r, y + r, r, kPi, kPi * 1.5);
  cairo_close_path(cr);
}

void lgrad(cairo_t* cr, double x0, double y0, double x1, double y1, std::initializer_list<std::pair<double, Color>> stops,
  double a = 1.0, bool preserve = false)
{
  cairo_pattern_t* g = cairo_pattern_create_linear(x0, y0, x1, y1);
  for (const auto& s : stops)
    stop(g, s.first, s.second, a);
  cairo_set_source(cr, g);
  if (preserve)
    cairo_fill_preserve(cr);
  else
    cairo_fill(cr);
  cairo_pattern_destroy(g);
}

void stroke(cairo_t* cr, Color c, double w, double a = 1.0, bool preserve = false)
{
  setRgba(cr, c, a);
  cairo_set_line_width(cr, w);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
  if (preserve)
    cairo_stroke_preserve(cr);
  else
    cairo_stroke(cr);
}

void glow(cairo_t* cr, double x, double y, double rad, Color c, double a)
{
  cairo_pattern_t* g = cairo_pattern_create_radial(x, y, 0, x, y, rad);
  stop(g, 0.0, c, a);
  stop(g, 0.5, c, a * 0.35);
  stop(g, 1.0, c, 0.0);
  circle(cr, x, y, rad);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
}

void lettering(cairo_t* cr, const char* text, double cx, double baseline, double size, Color c, double a = 1.0, bool italic = false)
{
  selectGameFont(cr, italic);
  cairo_set_font_size(cr, size);
  cairo_text_extents_t e;
  cairo_text_extents(cr, text, &e);
  cairo_move_to(cr, cx - e.width * 0.5 - e.x_bearing, baseline);
  setRgba(cr, c, a);
  cairo_show_text(cr, text);
}

// Lettering that shrinks to fit `maxW`.
void fitLettering(cairo_t* cr, const char* text, double cx, double baseline, double size, double maxW, Color c, double a = 1.0)
{
  selectGameFont(cr);
  cairo_set_font_size(cr, size);
  cairo_text_extents_t e;
  cairo_text_extents(cr, text, &e);
  if (e.width > maxW)
    size *= maxW / e.width;
  lettering(cr, text, cx, baseline, size, c, a);
}

// Wood grain across a rectangle.
void grain(cairo_t* cr, double x, double y, double w, double h, int seed, double a = 0.3)
{
  Rng rng(std::uint32_t(38300 + seed));
  cairo_save(cr);
  cairo_rectangle(cr, x, y, w, h);
  cairo_clip(cr);
  for (double k = 2; k < h; k += rng.range(3.0f, 6.0f))
  {
    const double yy = y + k;
    cairo_move_to(cr, x, yy);
    cairo_curve_to(cr, x + w * 0.3, yy + rng.range(-2, 2), x + w * 0.7, yy + rng.range(-2, 2), x + w, yy + rng.range(-1, 1));
    stroke(cr, rng.uniform() < 0.5f ? kWoodLo : kWoodHi, rng.range(0.6f, 1.3f), a * rng.range(0.5f, 1.0f));
  }
  cairo_restore(cr);
}

// A wobbly hand-drawn line from (x0, y0) to (x1, y1): ink a quick pen put
// down, its wobble from `seed`.
void inkLine(cairo_t* cr, double x0, double y0, double x1, double y1, int seed, double wob = 1.2)
{
  Rng rng(std::uint32_t(38310 + seed));
  const int n = std::max(2, int(std::hypot(x1 - x0, y1 - y0) / 24.0));
  cairo_move_to(cr, x0, y0);
  for (int k = 1; k <= n; ++k)
  {
    const double u = double(k) / n;
    cairo_line_to(cr, x0 + (x1 - x0) * u + (k < n ? rng.range(float(-wob), float(wob)) : 0.0),
      y0 + (y1 - y0) * u + (k < n ? rng.range(float(-wob), float(wob)) : 0.0));
  }
}

// --- Painters ----------------------------------------------------------------------------------------

// A marker letter (56 x 68, centred): `style` 0 is marker ink with a paper
// halo round it so it reads on anything, 1 is a pencil sketch of it
// (outlined twice in graphite, hatched inside).
void paintGlyph(cairo_t* cr, char c, int style)
{
  const char s[2] = {c, 0};
  selectGameFont(cr);
  cairo_set_font_size(cr, 40.0);
  cairo_text_extents_t e;
  cairo_text_extents(cr, s, &e);
  const double x = 28 - e.width * 0.5 - e.x_bearing, y = 34 - e.height * 0.5 - e.y_bearing;
  if (style == 0)
  {
    cairo_move_to(cr, x, y);
    cairo_text_path(cr, s);
    stroke(cr, rgb(255, 250, 236), 9.0, 0.8);
    cairo_move_to(cr, x, y);
    cairo_text_path(cr, s);
    setRgba(cr, kInk, 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 2.6);
    // The marker's wet sheen down one stroke.
    cairo_move_to(cr, x + 1.5, y - 1.5);
    cairo_text_path(cr, s);
    cairo_save(cr);
    cairo_clip(cr);
    cairo_rectangle(cr, 0, 0, 56, 30);
    setRgba(cr, rgb(120, 120, 170), 0.25);
    cairo_fill(cr);
    cairo_restore(cr);
  }
  else
  {
    cairo_move_to(cr, x, y);
    cairo_text_path(cr, s);
    stroke(cr, rgb(255, 250, 236), 7.0, 0.5);
    cairo_move_to(cr, x, y);
    cairo_text_path(cr, s);
    cairo_save(cr);
    cairo_clip_preserve(cr);
    cairo_new_path(cr);
    for (double k = -60; k < 60; k += 3.5)
    {
      cairo_move_to(cr, k, 68);
      cairo_line_to(cr, k + 68, 0);
    }
    stroke(cr, kGraphite, 1.0, 0.75);
    cairo_restore(cr);
    cairo_move_to(cr, x, y);
    cairo_text_path(cr, s);
    stroke(cr, kGraphite, 1.6, 0.95);
    cairo_move_to(cr, x + 1.2, y - 0.8);
    cairo_text_path(cr, s);
    stroke(cr, kGraphite, 0.9, 0.5);
  }
}

// An oak desk `n` blocks wide, 192 tall (its three rows of blocks): a thick
// top lit along its front edge, drawer pedestals at both ends (brass pulls,
// label slots), a dark kneehole and modesty panel between, plinths.
void paintDesk(cairo_t* cr, int n, int seed)
{
  const double W = n * 64.0;
  // The modesty panel, recessed in shadow.
  cairo_rectangle(cr, 0, 20, W, 172);
  lgrad(cr, 0, 20, 0, 192, {{0.0, rgb(40, 26, 16)}, {1.0, rgb(70, 46, 28)}});
  cairo_rectangle(cr, 40, 30, W - 80, 120);
  lgrad(cr, 0, 30, 0, 150, {{0.0, rgb(96, 64, 38)}, {1.0, rgb(70, 46, 26)}});
  grain(cr, 40, 30, W - 80, 120, seed, 0.2);
  // The pedestals.
  const double pw = std::min(176.0, W * 0.3);
  for (int side = 0; side < 2; ++side)
  {
    const double px = side ? W - pw : 0.0;
    cairo_rectangle(cr, px, 20, pw, 166);
    lgrad(cr, px, 0, px + pw, 0, {{0.0, kWoodLo}, {0.2, kWood}, {0.8, kWood}, {1.0, kWoodLo}}, 1.0, true);
    stroke(cr, kInk, 2.0, 0.9);
    for (int d = 0; d < 3; ++d)
    {
      const double dy = 30 + d * 50;
      roundRect(cr, px + 10, dy, pw - 20, 42, 4);
      lgrad(cr, 0, dy, 0, dy + 42, {{0.0, kWoodHi}, {0.5, kWood}, {1.0, rgb(120, 80, 46)}}, 1.0, true);
      stroke(cr, kWoodLo, 1.6, 0.9);
      grain(cr, px + 12, dy + 2, pw - 24, 38, seed + d + side * 3, 0.25);
      // The brass pull and the label slot.
      roundRect(cr, px + pw * 0.5 - 22, dy + 24, 44, 8, 4);
      lgrad(cr, 0, dy + 24, 0, dy + 32, {{0.0, rgb(255, 230, 150)}, {1.0, rgb(150, 110, 40)}}, 1.0, true);
      stroke(cr, kInk, 1.0, 0.8);
      cairo_rectangle(cr, px + pw * 0.5 - 16, dy + 7, 32, 12);
      setRgba(cr, rgb(180, 140, 70), 1.0);
      cairo_fill(cr);
      cairo_rectangle(cr, px + pw * 0.5 - 13, dy + 9, 26, 8);
      setRgba(cr, kPaper, 1.0);
      cairo_fill(cr);
    }
    cairo_rectangle(cr, px + 4, 184, pw - 8, 8);
    setRgba(cr, rgb(30, 18, 10), 1.0);
    cairo_fill(cr);
  }
  // The top.
  cairo_rectangle(cr, 0, 0, W, 22);
  lgrad(cr, 0, 0, 0, 22, {{0.0, rgb(230, 186, 126)}, {0.2, kWoodHi}, {0.6, kWood}, {1.0, kWoodLo}}, 1.0, true);
  stroke(cr, kInk, 2.0, 0.9);
  grain(cr, 2, 4, W - 4, 16, seed + 9, 0.35);
  cairo_rectangle(cr, 0, 1, W, 2);
  setRgba(cr, rgb(255, 230, 180), 0.7);
  cairo_fill(cr);
  // A shadow under the top.
  cairo_rectangle(cr, 0, 22, W, 10);
  lgrad(cr, 0, 22, 0, 32, {{0.0, kInk}, {1.0, kInk}}, 0.0);
  cairo_pattern_t* g = cairo_pattern_create_linear(0, 22, 0, 34);
  stop(g, 0.0, kInk, 0.6);
  stop(g, 1.0, kInk, 0.0);
  cairo_rectangle(cr, 0, 22, W, 12);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
}

// Desk clutter, each with its anchor at the middle of its base.
// A stack of paper (90 x 70): sheets fanned a little, a binder clip, the
// top sheet typed.
void paintPaperStack(cairo_t* cr, int seed)
{
  Rng rng(std::uint32_t(38320 + seed));
  const int sheets = 6 + int(rng.next() % 8u);
  double y = 68;
  for (int k = 0; k < sheets; ++k)
  {
    const double off = rng.range(-4, 4);
    cairo_rectangle(cr, 8 + off, y - 3.2, 74, 3.2);
    setRgba(cr, k % 3 == 0 ? rgb(236, 230, 214) : kPaper, 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kPaperLo, 0.7, 0.9);
    y -= 3.4;
  }
  // The top sheet, seen a little from above.
  cairo_move_to(cr, 10, y);
  cairo_line_to(cr, 80, y);
  cairo_line_to(cr, 84, y - 10);
  cairo_line_to(cr, 14, y - 10);
  cairo_close_path(cr);
  setRgba(cr, kPaper, 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kPaperLo, 0.8);
  for (int l = 0; l < 3; ++l)
  {
    cairo_move_to(cr, 20 + l * 1.2, y - 3 - l * 2.4);
    cairo_line_to(cr, 20 + l * 1.2 + rng.range(30, 54), y - 3 - l * 2.4);
    stroke(cr, kGraphite, 0.8, 0.6);
  }
  // A binder clip.
  cairo_rectangle(cr, 38, y - 6, 14, 10);
  setRgba(cr, rgb(30, 30, 36), 1.0);
  cairo_fill(cr);
  cairo_move_to(cr, 40, y - 6);
  cairo_line_to(cr, 37, y - 16);
  cairo_move_to(cr, 50, y - 6);
  cairo_line_to(cr, 53, y - 16);
  stroke(cr, rgb(200, 200, 210), 1.6);
}

// A coffee mug (48 x 56), its colour and slogan from `seed`, steam rising.
void paintMug(cairo_t* cr, int seed)
{
  static constexpr Color kMugs[4] = {rgb(214, 40, 40), rgb(50, 110, 190), rgb(240, 236, 226), rgb(60, 150, 90)};
  const Color c = kMugs[seed % 4];
  for (int k = 0; k < 2; ++k)
  {
    cairo_move_to(cr, 18 + k * 8, 18);
    cairo_curve_to(cr, 14 + k * 8, 12, 22 + k * 8, 8, 18 + k * 8, 2);
    stroke(cr, rgb(255, 255, 255), 1.6, 0.4);
  }
  cairo_arc(cr, 36, 38, 7, -kPi * 0.5, kPi * 0.5);
  stroke(cr, kInk, 5.0);
  cairo_arc(cr, 36, 38, 7, -kPi * 0.5, kPi * 0.5);
  stroke(cr, c, 3.0);
  cairo_rectangle(cr, 8, 22, 28, 34);
  lgrad(cr, 8, 0, 36, 0, {{0.0, lerpColor(c, rgb(0, 0, 0), 0.3f)}, {0.35, lerpColor(c, rgb(255, 255, 255), 0.25f)}, {1.0, lerpColor(c, rgb(0, 0, 0), 0.35f)}},
    1.0, true);
  stroke(cr, kInk, 1.6);
  ellipse(cr, 22, 22, 14, 3);
  setRgba(cr, rgb(70, 40, 20), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.2);
  if (seed % 3 == 0)
    fitLettering(cr, "#1 HACK", 22, 42, 8, 24, seed % 4 == 2 ? kRedInk : rgb(255, 255, 255));
}

// A green banker's lamp (120 x 120): a brass stem and base, the green glass
// shade lit from inside.
void paintLamp(cairo_t* cr)
{
  ellipse(cr, 60, 114, 34, 6);
  lgrad(cr, 0, 108, 0, 120, {{0.0, rgb(255, 226, 140)}, {1.0, rgb(130, 90, 30)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  cairo_rectangle(cr, 56, 52, 8, 60);
  lgrad(cr, 56, 0, 64, 0, {{0.0, rgb(150, 110, 40)}, {0.4, rgb(255, 226, 140)}, {1.0, rgb(120, 80, 30)}}, 1.0, true);
  stroke(cr, kInk, 1.2);
  cairo_move_to(cr, 8, 54);
  cairo_curve_to(cr, 12, 30, 108, 30, 112, 54);
  cairo_close_path(cr);
  lgrad(cr, 0, 30, 0, 54, {{0.0, rgb(90, 200, 120)}, {0.5, rgb(30, 120, 60)}, {1.0, rgb(10, 60, 30)}}, 1.0, true);
  stroke(cr, kInk, 1.8);
  cairo_move_to(cr, 14, 44);
  cairo_curve_to(cr, 30, 36, 60, 34, 80, 36);
  stroke(cr, rgb(200, 255, 210), 2.0, 0.5);
  ellipse(cr, 60, 55, 50, 4);
  setRgba(cr, rgb(255, 240, 190), 1.0);
  cairo_fill(cr);
  cairo_move_to(cr, 92, 56);
  cairo_line_to(cr, 92, 72);
  stroke(cr, rgb(200, 170, 80), 1.2);
  circle(cr, 92, 74, 2.5);
  setRgba(cr, rgb(220, 190, 90), 1.0);
  cairo_fill(cr);
}

// A pencil cup (40 x 70) full of pencils and a pair of scissors.
void paintPencilCup(cairo_t* cr, int seed)
{
  static constexpr Color kCols[4] = {kPencil, rgb(220, 40, 50), rgb(60, 120, 210), kPencil};
  for (int k = 0; k < 4; ++k)
  {
    const double x0 = 12 + k * 5, lean = (k - 1.5) * 4;
    cairo_move_to(cr, x0, 40);
    cairo_line_to(cr, x0 + lean, 6 + (k % 2) * 6);
    stroke(cr, kInk, 5.0);
    cairo_move_to(cr, x0, 40);
    cairo_line_to(cr, x0 + lean, 6 + (k % 2) * 6);
    stroke(cr, kCols[(k + seed) % 4], 3.2);
  }
  cairo_rectangle(cr, 6, 34, 28, 34);
  lgrad(cr, 6, 0, 34, 0, {{0.0, rgb(40, 40, 50)}, {0.4, rgb(110, 110, 124)}, {1.0, rgb(30, 30, 40)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
}

// A crumpled ball of paper (30 x 28): a draft that didn't make it.
void paintCrumple(cairo_t* cr, int seed)
{
  Rng rng(std::uint32_t(38330 + seed));
  const int n = 11;
  for (int k = 0; k <= n; ++k)
  {
    const double a = k * 2 * kPi / n, rr = 11 + rng.range(-2.5f, 2.5f);
    const double x = 15 + std::cos(a) * rr, y = 15 + std::sin(a) * rr;
    if (k == 0)
      cairo_move_to(cr, x, y);
    else
      cairo_line_to(cr, x, y);
  }
  cairo_close_path(cr);
  lgrad(cr, 4, 4, 26, 26, {{0.0, rgb(255, 255, 250)}, {1.0, kPaperLo}}, 1.0, true);
  stroke(cr, kInk, 1.2);
  for (int k = 0; k < 6; ++k)
  {
    const double a = rng.range(0, 6.28f);
    cairo_move_to(cr, 15 + std::cos(a) * 2, 15 + std::sin(a) * 2);
    cairo_line_to(cr, 15 + std::cos(a + 0.4) * 10, 15 + std::sin(a + 0.4) * 10);
    stroke(cr, rgb(150, 140, 120), 0.9, 0.7);
  }
}

// A cork tile (256 x 256): speckled cork, pinholes, the odd old staple.
void paintCork(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 0, 256, 256);
  setRgba(cr, kCork, 1.0);
  cairo_fill(cr);
  Rng rng(38341u);
  for (int i = 0; i < 1600; ++i)
  {
    const double s = rng.range(0.8f, 2.4f);
    cairo_rectangle(cr, rng.range(0, 256), rng.range(0, 256), s, s);
    const float u = rng.uniform();
    setRgba(cr, u < 0.4f ? rgb(150, 100, 56) : u < 0.8f ? rgb(226, 186, 136) : rgb(110, 70, 40), rng.range(0.4f, 0.9f));
    cairo_fill(cr);
  }
  for (int i = 0; i < 14; ++i)
  {
    circle(cr, rng.range(4, 252), rng.range(4, 252), 1.2);
    setRgba(cr, rgb(60, 36, 20), 0.8);
    cairo_fill(cr);
  }
  for (int i = 0; i < 2; ++i)
  {
    const double x = rng.range(20, 236), y = rng.range(20, 236);
    cairo_move_to(cr, x, y);
    cairo_line_to(cr, x + 8, y);
    stroke(cr, rgb(210, 210, 220), 1.2, 0.8);
  }
}

// An index card (120 x 84) of a scene idea: a coloured header rule, ruled
// lines, a few scribbled words; `seed` picks the scribbles.
void paintIndexCard(cairo_t* cr, int seed)
{
  static const char* const kIdeas[12] = {"DASH TRIPS", "TWIST?", "MAX LIES", "CUT THIS", "ROCCO CRIES", "CAMEO",
    "BIGGER!", "LANCE AGAIN?", "RATINGS", "NOVA KNOWS", "ACT 3", "MORE GUNS"};
  static constexpr Color kRules[4] = {rgb(220, 60, 70), rgb(60, 110, 200), rgb(60, 160, 90), rgb(240, 170, 40)};
  cairo_rectangle(cr, 4, 6, 112, 76);
  setRgba(cr, kInk, 0.3);
  cairo_fill(cr);
  cairo_rectangle(cr, 2, 2, 112, 76);
  lgrad(cr, 0, 2, 0, 78, {{0.0, rgb(255, 253, 244)}, {1.0, rgb(232, 226, 210)}}, 1.0, true);
  stroke(cr, kInk, 0.8, 0.5);
  cairo_move_to(cr, 2, 16);
  cairo_line_to(cr, 114, 16);
  stroke(cr, kRules[seed % 4], 1.4);
  for (int l = 0; l < 5; ++l)
  {
    cairo_move_to(cr, 2, 28 + l * 10);
    cairo_line_to(cr, 114, 28 + l * 10);
  }
  stroke(cr, rgb(150, 180, 220), 0.7, 0.6);
  fitLettering(cr, kIdeas[seed % 12], 58, 13, 11, 100, kInk, 0.9);
  Rng rng(std::uint32_t(38350 + seed));
  for (int l = 0; l < 3; ++l)
  {
    cairo_move_to(cr, 10, 26 + l * 10);
    for (double x = 10; x < 10 + rng.range(40, 92); x += 6)
      cairo_line_to(cr, x + 3, 26 + l * 10 + (int(x) % 12 < 6 ? -2.5 : 0.5));
    stroke(cr, kGraphite, 1.0, 0.75);
  }
}

// A rejected pitch (220 x 140): a big index card, two lines of marker, a
// red REJECTED stamp across it; `phase` 0..3 the card turned edge-on, `back`
// the card's blank back.
void paintPitchCard(cairo_t* cr, const std::string& text, int phase, bool back)
{
  static constexpr double kWide[4] = {1.0, 0.7, 0.4, 0.12};
  cairo_translate(cr, 110, 0);
  cairo_scale(cr, kWide[std::clamp(phase, 0, 3)], 1.0);
  cairo_translate(cr, -110, 0);
  cairo_rectangle(cr, 6, 8, 208, 128);
  setRgba(cr, kInk, 0.3);
  cairo_fill(cr);
  cairo_rectangle(cr, 2, 4, 208, 128);
  lgrad(cr, 0, 4, 0, 132, {{0.0, rgb(255, 254, 246)}, {1.0, rgb(232, 228, 210)}}, 1.0, true);
  stroke(cr, kInk, 1.0, 0.6);
  if (back)
  {
    cairo_move_to(cr, 20, 40);
    cairo_line_to(cr, 190, 100);
    stroke(cr, kGraphite, 1.0, 0.3);
    return;
  }
  cairo_move_to(cr, 2, 24);
  cairo_line_to(cr, 210, 24);
  stroke(cr, rgb(220, 70, 80), 1.6);
  for (int l = 0; l < 7; ++l)
  {
    cairo_move_to(cr, 2, 38 + l * 13);
    cairo_line_to(cr, 210, 38 + l * 13);
  }
  stroke(cr, rgb(150, 180, 220), 0.8, 0.6);
  // "LEVEL 43: THE TAX OFFICE" on two lines: the level, the title.
  const auto colon = text.find(':');
  const std::string head = colon == std::string::npos ? text : text.substr(0, colon + 1);
  const std::string title = colon == std::string::npos ? "" : text.substr(colon + 2 <= text.size() ? colon + 2 : text.size());
  fitLettering(cr, head.c_str(), 106, 20, 15, 190, kInk);
  fitLettering(cr, title.c_str(), 106, 70, 22, 190, kInk);
  // The stamp, across it at an angle.
  cairo_save(cr);
  cairo_translate(cr, 110, 104);
  cairo_rotate(cr, -0.14);
  roundRect(cr, -74, -18, 148, 34, 5);
  stroke(cr, kRedInk, 3.0, 0.85);
  selectGameFont(cr);
  cairo_set_font_size(cr, 24);
  cairo_text_extents_t e;
  cairo_text_extents(cr, "REJECTED", &e);
  cairo_move_to(cr, -e.width * 0.5 - e.x_bearing, 9);
  setRgba(cr, kRedInk, 0.85);
  cairo_show_text(cr, "REJECTED");
  cairo_restore(cr);
}

// A pushpin's head (30 x 30, centred) in `c`.
void paintPin(cairo_t* cr, Color c)
{
  ellipse(cr, 17, 18, 11, 10);
  setRgba(cr, kInk, 0.35);
  cairo_fill(cr);
  circle(cr, 15, 15, 11);
  cairo_pattern_t* g = cairo_pattern_create_radial(11, 11, 1, 15, 15, 12);
  stop(g, 0.0, lerpColor(c, rgb(255, 255, 255), 0.6f));
  stop(g, 0.5, c);
  stop(g, 1.0, lerpColor(c, rgb(0, 0, 0), 0.45f));
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  stroke(cr, kInk, 1.4);
  circle(cr, 11, 10, 3);
  setRgba(cr, rgb(255, 255, 255), 0.7);
  cairo_fill(cr);
}

// A polaroid of a suspect (100 x 116) under a pin: `who` 0-6.
void paintPolaroid(cairo_t* cr, int who)
{
  static const char* const kNames[7] = {"LANCE?", "MAX", "DASH", "THE NET", "ROCCO", "NOVA", "WHO??"};
  static constexpr Color kTones[7] = {rgb(230, 190, 60), rgb(150, 90, 200), rgb(60, 150, 255), rgb(90, 90, 110), rgb(255, 130, 40),
    rgb(170, 90, 255), rgb(120, 120, 120)};
  cairo_rectangle(cr, 6, 8, 92, 106);
  setRgba(cr, kInk, 0.3);
  cairo_fill(cr);
  cairo_rectangle(cr, 2, 4, 92, 106);
  setRgba(cr, rgb(250, 248, 240), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 0.8, 0.5);
  cairo_rectangle(cr, 10, 12, 76, 70);
  lgrad(cr, 0, 12, 0, 82, {{0.0, rgb(60, 70, 90)}, {1.0, rgb(30, 30, 40)}});
  // A head-and-shoulders silhouette in the runner's colour.
  circle(cr, 48, 40, 14);
  setRgba(cr, kTones[who % 7], 0.9);
  cairo_fill(cr);
  cairo_move_to(cr, 22, 82);
  cairo_curve_to(cr, 24, 60, 72, 60, 74, 82);
  cairo_close_path(cr);
  cairo_fill(cr);
  fitLettering(cr, kNames[who % 7], 48, 102, 14, 80, kInk);
  if (who == 0)
  {
    circle(cr, 48, 48, 32);
    stroke(cr, kRedInk, 2.4, 0.9);
  }
}

// The filing cabinet's shell and inside (320 x 320; blocks x 2-6, rows
// 35-39): its steel top with a spider plant, its side, and inside the
// writers' snack drawer: shelves of crisps, cookies and a jar of sweets,
// a SNACKS label. The front is drawn over it until SECRET is struck.
void paintCabinetInside(cairo_t* cr)
{
  // The inside, in shadow.
  cairo_rectangle(cr, 64, 64, 256, 256);
  lgrad(cr, 0, 64, 0, 320, {{0.0, rgb(40, 42, 50)}, {1.0, rgb(70, 72, 82)}});
  for (int s = 0; s < 3; ++s)
  {
    const double y = 64 + 80 + s * 80;
    cairo_rectangle(cr, 64, y, 256, 8);
    lgrad(cr, 0, y, 0, y + 8, {{0.0, kSteel}, {1.0, kSteelLo}});
  }
  // Snacks on the shelves.
  static constexpr Color kBags[5] = {rgb(230, 60, 60), rgb(250, 200, 40), rgb(60, 140, 230), rgb(120, 200, 80), rgb(240, 120, 200)};
  for (int k = 0; k < 6; ++k)
  {
    const double x = 80 + (k % 3) * 70, base = 64 + 80 + (k / 3) * 80;
    cairo_move_to(cr, x, base);
    cairo_line_to(cr, x + 4, base - 50);
    cairo_line_to(cr, x + 40, base - 52);
    cairo_line_to(cr, x + 44, base);
    cairo_close_path(cr);
    setRgba(cr, kBags[k % 5], 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.2, 0.8);
    ellipse(cr, x + 22, base - 26, 10, 7);
    setRgba(cr, rgb(255, 255, 255), 0.8);
    cairo_fill(cr);
  }
  roundRect(cr, 210, 64 + 160 - 46, 40, 46, 6);
  setRgba(cr, rgb(220, 240, 255), 0.5);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.2, 0.7);
  for (int k = 0; k < 7; ++k)
  {
    circle(cr, 218 + (k % 3) * 11, 64 + 160 - 10 - (k / 3) * 11, 4.5);
    setRgba(cr, kBags[k % 5], 1.0);
    cairo_fill(cr);
  }
  roundRect(cr, 120, 76, 80, 22, 3);
  setRgba(cr, kPaper, 1.0);
  cairo_fill(cr);
  lettering(cr, "SNACKS", 160, 93, 14, kRedInk);
  // The side and the top, steel.
  cairo_rectangle(cr, 0, 64, 64, 256);
  lgrad(cr, 0, 0, 64, 0, {{0.0, kSteelLo}, {0.5, kSteel}, {1.0, rgb(110, 116, 128)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  cairo_rectangle(cr, 0, 0, 320, 64);
  lgrad(cr, 0, 0, 0, 64, {{0.0, rgb(200, 206, 216)}, {0.2, kSteel}, {1.0, kSteelLo}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  cairo_rectangle(cr, 0, 0, 320, 4);
  setRgba(cr, rgb(240, 244, 250), 0.8);
  cairo_fill(cr);
}

// The filing cabinet's front (320 x 320): four drawers spanning it, each
// with a chrome pull and a typed label, the top with a plant on it.
void paintCabinetFace(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 0, 320, 320);
  lgrad(cr, 0, 0, 320, 0, {{0.0, rgb(110, 116, 128)}, {0.3, rgb(176, 182, 194)}, {1.0, rgb(96, 102, 114)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  static const char* const kLabels[4] = {"PITCHES", "NOTES", "DRAFTS", "MISC."};
  for (int d = 0; d < 4; ++d)
  {
    const double y = 64 + d * 64;
    roundRect(cr, 10, y + 4, 300, 56, 4);
    lgrad(cr, 0, y + 4, 0, y + 60, {{0.0, rgb(196, 202, 214)}, {1.0, rgb(126, 132, 144)}}, 1.0, true);
    stroke(cr, kInk, 1.2, 0.8);
    roundRect(cr, 130, y + 34, 60, 12, 5);
    lgrad(cr, 0, y + 34, 0, y + 46, {{0.0, rgb(250, 250, 255)}, {1.0, rgb(110, 114, 126)}}, 1.0, true);
    stroke(cr, kInk, 1.0, 0.8);
    cairo_rectangle(cr, 136, y + 12, 48, 16);
    setRgba(cr, kPaper, 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kSteelLo, 1.2);
    fitLettering(cr, kLabels[d], 160, y + 25, 10, 42, kInk);
  }
  // The top.
  cairo_rectangle(cr, 0, 0, 320, 64);
  lgrad(cr, 0, 0, 0, 64, {{0.0, rgb(214, 220, 230)}, {0.3, kSteel}, {1.0, kSteelLo}}, 1.0, true);
  stroke(cr, kInk, 1.6);
}

// A stack of fat binders lying flat (`n` blocks wide, 64 tall): the top
// shelf's steps.
void paintBinderStack(cairo_t* cr, int n, int seed)
{
  static constexpr Color kB[5] = {rgb(200, 50, 50), rgb(50, 90, 170), rgb(40, 40, 46), rgb(60, 140, 90), rgb(230, 190, 60)};
  const double W = n * 64.0;
  for (int k = 0; k < 3; ++k)
  {
    const double y = 64 - (k + 1) * 21.3, inset = (k * 7 + seed * 3) % 9;
    roundRect(cr, inset, y, W - inset - (k % 2) * 6, 20, 3);
    const Color c = kB[(k + seed) % 5];
    lgrad(cr, 0, y, 0, y + 20, {{0.0, lerpColor(c, rgb(255, 255, 255), 0.3f)}, {0.5, c}, {1.0, lerpColor(c, rgb(0, 0, 0), 0.4f)}}, 1.0, true);
    stroke(cr, kInk, 1.4);
    cairo_rectangle(cr, inset + W * 0.3, y + 5, W * 0.3, 10);
    setRgba(cr, kPaper, 0.9);
    cairo_fill(cr);
  }
}

// The big typewriter's body (w x h, its top-left at the texture's): black
// enamel swelling out of the floor, a gold pinstripe round it, the type
// basket's fan of bars in its opening, the maker's plate, and three rows of
// little round keys (chrome rings, black caps) behind the big ones.
void paintTypewriterBody(cairo_t* cr, int w, int h)
{
  const double W = w, H = h;
  auto shape = [&](double in) {
    cairo_move_to(cr, in, H);
    cairo_line_to(cr, in + 30, 110 + in);
    cairo_curve_to(cr, in + 40, 40 + in, in + 90, 14 + in, in + 170, 14 + in);
    cairo_line_to(cr, W - in - 170, 14 + in);
    cairo_curve_to(cr, W - in - 90, 14 + in, W - in - 40, 40 + in, W - in - 30, 110 + in);
    cairo_line_to(cr, W - in, H);
    cairo_close_path(cr);
  };
  shape(0);
  lgrad(cr, 0, 14, 0, H, {{0.0, rgb(78, 78, 90)}, {0.08, rgb(40, 40, 48)}, {0.5, rgb(24, 24, 30)}, {1.0, rgb(12, 12, 16)}}, 1.0, true);
  stroke(cr, rgb(6, 6, 10), 3.0);
  // The shine along its shoulder.
  cairo_move_to(cr, 60, 70);
  cairo_curve_to(cr, 80, 34, 120, 24, 190, 24);
  cairo_line_to(cr, W - 190, 24);
  cairo_curve_to(cr, W - 120, 24, W - 80, 34, W - 60, 70);
  stroke(cr, rgb(255, 255, 255), 3.0, 0.22);
  // The gold pinstripe.
  shape(22);
  stroke(cr, rgb(214, 172, 72), 2.0, 0.85);
  // The type basket: a half-moon opening, its bars fanned up.
  const double fx = W * 0.5, fy = 150;
  cairo_save(cr);
  cairo_new_path(cr);
  cairo_move_to(cr, fx - 430, fy);
  cairo_curve_to(cr, fx - 430, fy - 150, fx + 430, fy - 150, fx + 430, fy);
  cairo_close_path(cr);
  lgrad(cr, 0, fy - 120, 0, fy, {{0.0, rgb(6, 6, 10)}, {1.0, rgb(30, 30, 38)}}, 1.0, true);
  cairo_clip_preserve(cr);
  cairo_new_path(cr);
  for (int k = 0; k < 44; ++k)
  {
    const double a = kPi * (0.06 + 0.88 * k / 43.0);
    const double c = std::cos(a), sn = std::sin(a);
    cairo_move_to(cr, fx + c * 80, fy - sn * 14);
    cairo_line_to(cr, fx + c * 410, fy - sn * 108);
    stroke(cr, k % 2 ? rgb(150, 154, 166) : rgb(110, 114, 126), 4.0);
    cairo_rectangle(cr, fx + c * 404 - 5, fy - sn * 106 - 4, 10, 8);
    setRgba(cr, rgb(200, 204, 214), 1.0);
    cairo_fill(cr);
  }
  cairo_restore(cr);
  cairo_new_path(cr);
  cairo_move_to(cr, fx - 430, fy);
  cairo_curve_to(cr, fx - 430, fy - 150, fx + 430, fy - 150, fx + 430, fy);
  stroke(cr, rgb(214, 172, 72), 3.0, 0.9);
  // The maker's plate.
  roundRect(cr, fx - 180, fy + 8, 360, 56, 10);
  lgrad(cr, 0, fy + 8, 0, fy + 64, {{0.0, rgb(250, 214, 120)}, {0.5, rgb(196, 150, 54)}, {1.0, rgb(120, 84, 24)}}, 1.0, true);
  stroke(cr, rgb(60, 40, 10), 1.5);
  roundRect(cr, fx - 170, fy + 16, 340, 40, 6);
  setRgba(cr, rgb(26, 20, 16), 1.0);
  cairo_fill(cr);
  lettering(cr, "MAXTYPE 38", fx, fy + 48, 28, rgb(244, 212, 124), 1.0, true);
  // Three rows of little round keys.
  static const char* const kRows[3] = {"1234567890-=", "QWERTYUIOP[]", "ASDFGHJKL;'#"};
  for (int row = 0; row < 3; ++row)
  {
    const double ry = 236 + row * 48.0;
    const double off = 50 + row * 18.0;
    int k = 0;
    for (double kx = off; kx < W - off; kx += 64.0, ++k)
    {
      cairo_move_to(cr, kx, ry + 18);
      cairo_line_to(cr, kx + 6, ry + 44);
      stroke(cr, rgb(70, 72, 82), 3.0);
      circle(cr, kx, ry, 19);
      lgrad(cr, 0, ry - 19, 0, ry + 19, {{0.0, rgb(240, 242, 248)}, {0.5, rgb(150, 154, 166)}, {1.0, rgb(70, 72, 82)}}, 1.0, true);
      stroke(cr, rgb(10, 10, 14), 1.2);
      circle(cr, kx, ry, 14.5);
      lgrad(cr, 0, ry - 15, 0, ry + 15, {{0.0, rgb(50, 50, 58)}, {1.0, rgb(10, 10, 14)}});
      const char c[2] = {kRows[row][k % 12], 0};
      lettering(cr, c, kx, ry + 6, 16, rgb(236, 232, 218));
    }
  }
}

// A typewriter key (136 x 76, its top-left 4 px left of its block, the
// stem going down to the floor): a round-shouldered keycap with a chrome
// ring, black face and the letter in white; `lit` warms the ring to gold.
void paintKey(cairo_t* cr, char ch, bool lit)
{
  const bool space = ch == ' ';
  // The stem.
  cairo_rectangle(cr, 62, 50, 12, 26);
  lgrad(cr, 62, 0, 74, 0, {{0.0, kSteelLo}, {0.5, rgb(220, 224, 232)}, {1.0, kSteelLo}});
  // The cap.
  roundRect(cr, 4, 2, 128, 58, space ? 12 : 29);
  const Color c0 = lit ? rgb(255, 244, 190) : rgb(250, 252, 255);
  const Color c1 = lit ? rgb(230, 180, 60) : rgb(170, 176, 190);
  const Color c2 = lit ? rgb(140, 90, 20) : rgb(80, 84, 96);
  lgrad(cr, 0, 2, 0, 60, {{0.0, c0}, {0.5, c1}, {1.0, c2}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  roundRect(cr, 12, 8, 112, 44, space ? 8 : 22);
  lgrad(cr, 0, 8, 0, 52, {{0.0, rgb(60, 60, 70)}, {0.4, rgb(20, 20, 26)}, {1.0, rgb(10, 10, 14)}}, 1.0, true);
  stroke(cr, kInk, 1.2);
  cairo_move_to(cr, 26, 14);
  cairo_curve_to(cr, 50, 10, 86, 10, 110, 14);
  stroke(cr, rgb(255, 255, 255), 2.0, 0.35);
  const char s[2] = {ch, 0};
  if (space)
    lettering(cr, "SPACE", 68, 38, 18, rgb(250, 248, 236));
  else
    lettering(cr, s, 68, 43, 32, rgb(250, 248, 236));
}

// A block of the bridge (64 x 64): typed paper tape, the letter in black
// ink a little off its line, the strip's edges.
void paintTape(cairo_t* cr, char ch, int i)
{
  cairo_rectangle(cr, 0, 6, 64, 52);
  lgrad(cr, 0, 6, 0, 58, {{0.0, rgb(255, 254, 246)}, {0.8, kPaper}, {1.0, kPaperLo}}, 1.0, true);
  cairo_move_to(cr, 0, 6.5);
  cairo_line_to(cr, 64, 6.5);
  cairo_move_to(cr, 0, 57.5);
  cairo_line_to(cr, 64, 57.5);
  stroke(cr, kInk, 1.4, 0.8);
  cairo_move_to(cr, 0, 14);
  cairo_line_to(cr, 64, 14);
  stroke(cr, rgb(220, 90, 100), 0.9, 0.6);
  const char s[2] = {ch, 0};
  if (ch != ' ')
  {
    selectGameFont(cr);
    cairo_set_font_size(cr, 34);
    cairo_text_extents_t e;
    cairo_text_extents(cr, s, &e);
    cairo_move_to(cr, 32 - e.width * 0.5 - e.x_bearing, 48 + (i % 3 - 1) * 1.5);
    setRgba(cr, kInk, 0.92);
    cairo_show_text(cr, s);
  }
  // The joint to the next block.
  cairo_move_to(cr, 63.5, 8);
  cairo_line_to(cr, 63.5, 56);
  stroke(cr, kPaperLo, 0.8, 0.8);
}

// The rubber-stamp press's head (256 x 128, its top-left at the press's
// top-left): a varnished wooden block on a brass collar, REJECTED on its
// label, the red rubber pad under it.
void paintStamp(cairo_t* cr)
{
  roundRect(cr, 70, 0, 116, 24, 6);
  lgrad(cr, 0, 0, 0, 24, {{0.0, rgb(255, 230, 150)}, {1.0, rgb(140, 100, 30)}}, 1.0, true);
  stroke(cr, kInk, 1.8);
  roundRect(cr, 6, 20, 244, 84, 8);
  lgrad(cr, 6, 0, 250, 0, {{0.0, kWoodLo}, {0.25, kWoodHi}, {0.6, kWood}, {1.0, kWoodLo}}, 1.0, true);
  stroke(cr, kInk, 2.4);
  grain(cr, 10, 24, 236, 76, 7, 0.35);
  roundRect(cr, 46, 40, 164, 42, 4);
  setRgba(cr, kPaper, 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.4);
  lettering(cr, "REJECTED", 128, 70, 26, kRedInk);
  cairo_rectangle(cr, 0, 104, 256, 24);
  lgrad(cr, 0, 104, 0, 128, {{0.0, rgb(240, 70, 70)}, {0.5, rgb(190, 30, 36)}, {1.0, rgb(110, 10, 16)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
}

// The press's housing in the ceiling (300 x 120): a steel cylinder block
// bolted up, hazard stripes along its bottom.
void paintPressHousing(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 0, 300, 100);
  lgrad(cr, 0, 0, 300, 0, {{0.0, kSteelLo}, {0.3, rgb(200, 206, 216)}, {1.0, kSteelLo}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  for (int k = 0; k < 6; ++k)
    circle(cr, 20 + k * 52, 16, 4);
  setRgba(cr, rgb(60, 62, 70), 1.0);
  cairo_fill(cr);
  cairo_save(cr);
  cairo_rectangle(cr, 0, 100, 300, 20);
  cairo_clip(cr);
  cairo_rectangle(cr, 0, 100, 300, 20);
  setRgba(cr, rgb(250, 200, 30), 1.0);
  cairo_fill(cr);
  for (double x = -20; x < 320; x += 30)
  {
    cairo_move_to(cr, x, 120);
    cairo_line_to(cr, x + 15, 100);
    cairo_line_to(cr, x + 30, 100);
    cairo_line_to(cr, x + 15, 120);
    cairo_close_path(cr);
  }
  setRgba(cr, kInk, 1.0);
  cairo_fill(cr);
  cairo_restore(cr);
}

// The press's warning sign (340 x 140): hazard stripes round a yellow board
// on a post (its word goes on it), a little skull-and-pencils.
void paintWarningSign(cairo_t* cr)
{
  cairo_rectangle(cr, 162, 100, 16, 40);
  setRgba(cr, kSteelLo, 1.0);
  cairo_fill(cr);
  roundRect(cr, 4, 4, 332, 100, 8);
  setRgba(cr, kInk, 1.0);
  cairo_fill(cr);
  cairo_save(cr);
  roundRect(cr, 4, 4, 332, 100, 8);
  cairo_clip(cr);
  for (double x = -40; x < 360; x += 28)
  {
    cairo_move_to(cr, x, 104);
    cairo_line_to(cr, x + 40, 4);
    cairo_line_to(cr, x + 54, 4);
    cairo_line_to(cr, x + 14, 104);
    cairo_close_path(cr);
  }
  setRgba(cr, rgb(250, 200, 30), 1.0);
  cairo_fill(cr);
  cairo_restore(cr);
  roundRect(cr, 16, 14, 308, 80, 5);
  setRgba(cr, rgb(255, 236, 120), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 2.0);
  lettering(cr, "DANGER: YOU WILL BE", 170, 30, 13, kInk);
}

// An inkwell (136 x 112, its anchor at the bottom-left): a squat bottle of
// dark glass full of black ink, a quill standing in it, a drip down its
// side, a paper label.
void paintInkwell(cairo_t* cr)
{
  ellipse(cr, 68, 106, 62, 6);
  setRgba(cr, kInk, 0.35);
  cairo_fill(cr);
  // A blot round its foot.
  ellipse(cr, 40, 104, 30, 5);
  setRgba(cr, rgb(16, 14, 30), 0.9);
  cairo_fill(cr);
  // The bottle.
  cairo_move_to(cr, 20, 104);
  cairo_curve_to(cr, 10, 104, 10, 60, 24, 52);
  cairo_line_to(cr, 48, 46);
  cairo_line_to(cr, 48, 30);
  cairo_line_to(cr, 88, 30);
  cairo_line_to(cr, 88, 46);
  cairo_line_to(cr, 112, 52);
  cairo_curve_to(cr, 126, 60, 126, 104, 116, 104);
  cairo_close_path(cr);
  lgrad(cr, 10, 0, 126, 0, {{0.0, rgb(30, 30, 60)}, {0.3, rgb(70, 80, 130)}, {0.6, rgb(26, 26, 50)}, {1.0, rgb(10, 10, 24)}}, 1.0, true);
  stroke(cr, kInk, 2.2);
  // The ink inside, near black.
  cairo_rectangle(cr, 18, 66, 100, 34);
  setRgba(cr, rgb(8, 8, 18), 0.7);
  cairo_fill(cr);
  // The glass's glint.
  cairo_move_to(cr, 26, 62);
  cairo_curve_to(cr, 20, 72, 20, 86, 24, 96);
  stroke(cr, rgb(255, 255, 255), 2.4, 0.45);
  // The label.
  roundRect(cr, 40, 70, 56, 24, 3);
  setRgba(cr, kPaper, 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.0);
  lettering(cr, "INK", 68, 87, 13, kInk);
  // The neck's rim and a drip.
  roundRect(cr, 44, 26, 48, 8, 3);
  setRgba(cr, rgb(40, 40, 70), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.4);
  cairo_move_to(cr, 88, 34);
  cairo_curve_to(cr, 92, 42, 90, 48, 94, 54);
  stroke(cr, rgb(10, 10, 20), 3.0);
  // The quill.
  cairo_move_to(cr, 62, 30);
  cairo_curve_to(cr, 56, 14, 40, -2, 18, -8);
  cairo_curve_to(cr, 40, 6, 48, 18, 60, 30);
  cairo_close_path(cr);
  lgrad(cr, 18, -8, 62, 30, {{0.0, rgb(255, 255, 255)}, {1.0, rgb(200, 200, 210)}}, 1.0, true);
  stroke(cr, kInk, 1.2);
  cairo_move_to(cr, 62, 32);
  cairo_curve_to(cr, 50, 14, 34, 0, 20, -6);
  stroke(cr, rgb(120, 120, 130), 1.0);
}

// The LANCE WINS door (136 x 392, its top-left 4 px left of its blocks):
// a tall studio door of dark varnished wood, a gold star with LANCE on it,
// a PRIVATE plate, a chain and padlock across it.
void paintLanceDoor(cairo_t* cr)
{
  cairo_rectangle(cr, 2, 2, 132, 388);
  lgrad(cr, 2, 0, 134, 0, {{0.0, rgb(50, 24, 20)}, {0.3, rgb(110, 50, 40)}, {0.7, rgb(90, 40, 32)}, {1.0, rgb(40, 18, 16)}}, 1.0, true);
  stroke(cr, kInk, 3.0);
  for (int k = 0; k < 2; ++k)
  {
    roundRect(cr, 16, 30 + k * 190, 104, 160, 4);
    stroke(cr, rgb(30, 12, 10), 3.0, 0.8);
    roundRect(cr, 20, 34 + k * 190, 96, 152, 3);
    stroke(cr, rgb(170, 90, 70), 1.2, 0.5);
  }
  // The star.
  cairo_save(cr);
  cairo_translate(cr, 68, 110);
  cairo_move_to(cr, 0, -40);
  for (int k = 1; k < 10; ++k)
  {
    const double a = -kPi / 2 + k * kPi / 5, rr = k % 2 ? 17.0 : 40.0;
    cairo_line_to(cr, std::cos(a) * rr, std::sin(a) * rr);
  }
  cairo_close_path(cr);
  lgrad(cr, 0, -40, 0, 40, {{0.0, rgb(255, 240, 160)}, {0.5, rgb(230, 180, 50)}, {1.0, rgb(150, 100, 20)}}, 1.0, true);
  stroke(cr, kInk, 1.8);
  cairo_restore(cr);
  lettering(cr, "LANCE", 68, 117, 15, rgb(80, 30, 20));
  roundRect(cr, 34, 170, 68, 22, 3);
  lgrad(cr, 0, 170, 0, 192, {{0.0, rgb(255, 230, 150)}, {1.0, rgb(150, 110, 40)}}, 1.0, true);
  stroke(cr, kInk, 1.2);
  lettering(cr, "PRIVATE", 68, 186, 11, rgb(60, 30, 10));
  // The handle.
  roundRect(cr, 104, 230, 14, 34, 6);
  lgrad(cr, 104, 0, 118, 0, {{0.0, rgb(255, 230, 150)}, {1.0, rgb(130, 90, 30)}}, 1.0, true);
  stroke(cr, kInk, 1.2);
  // The chain across and its padlock.
  for (int k = 0; k < 12; ++k)
  {
    const double x = 6 + k * 11, y = 280 + std::sin(k * 0.55) * 8;
    ellipse(cr, x, y, 7, 4);
    stroke(cr, k % 2 ? rgb(200, 204, 214) : rgb(130, 134, 146), 2.6);
  }
  roundRect(cr, 54, 288, 30, 26, 4);
  lgrad(cr, 0, 288, 0, 314, {{0.0, rgb(255, 220, 120)}, {1.0, rgb(150, 100, 30)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  cairo_arc(cr, 69, 288, 9, kPi, 2 * kPi);
  stroke(cr, rgb(180, 184, 196), 3.0);
}

// An inked brick wall `rows` blocks tall (136 wide, a pen drawing come to
// life): paper-white bricks with wobbly ink outlines and hatched shadows,
// a capping course on top.
void paintInkWall(cairo_t* cr, int rows, int seed)
{
  const double H = rows * 64.0;
  cairo_rectangle(cr, 4, 0, 128, H);
  setRgba(cr, rgb(238, 230, 212), 1.0);
  cairo_fill(cr);
  Rng rng(std::uint32_t(38360 + seed));
  int k = 0;
  for (double y = 18; y < H; y += 26, ++k)
  {
    inkLine(cr, 4, y, 132, y + rng.range(-1, 1), k * 7 + seed);
    stroke(cr, kInk, 2.0, 0.85);
    const double off = (k % 2) ? 22 : 54;
    for (double x = off; x < 128; x += 64)
    {
      inkLine(cr, 4 + x, y - 26, 4 + x + rng.range(-1.5f, 1.5f), y, k * 13 + int(x) + seed, 0.8);
      stroke(cr, kInk, 1.8, 0.85);
      // A hatched shadow down the brick's right end.
      for (double hy = y - 22; hy < y - 3; hy += 4)
      {
        cairo_move_to(cr, 4 + x - 9, hy + 4);
        cairo_line_to(cr, 4 + x - 3, hy);
      }
      stroke(cr, kGraphite, 0.8, 0.55);
    }
    if (rng.uniform() < 0.2f)
    {
      // A crack doodled in.
      const double cx = 4 + rng.range(20, 100);
      cairo_move_to(cr, cx, y - 20);
      cairo_line_to(cr, cx + 5, y - 13);
      cairo_line_to(cr, cx + 1, y - 6);
      stroke(cr, kInk, 1.0, 0.7);
    }
  }
  // The capping.
  cairo_rectangle(cr, 0, 0, 136, 18);
  setRgba(cr, rgb(214, 204, 186), 1.0);
  cairo_fill(cr);
  inkLine(cr, 0, 1, 136, 1, seed + 1);
  inkLine(cr, 0, 18, 136, 18, seed + 2);
  stroke(cr, kInk, 2.2);
  // The outline down both sides.
  inkLine(cr, 4, 0, 4, H, seed + 3, 1.6);
  inkLine(cr, 132, 0, 132, H, seed + 4, 1.6);
  stroke(cr, kInk, 2.6);
}

// The FLOOR plank (768 x 40, across the pit): a long board, grain, a nail
// at each end, a knot.
void paintPlank(cairo_t* cr)
{
  const double W = 768;
  cairo_rectangle(cr, 0, 0, W, 24);
  lgrad(cr, 0, 0, 0, 24, {{0.0, kWoodHi}, {0.35, kWood}, {1.0, kWoodLo}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  grain(cr, 2, 2, W - 4, 20, 21, 0.5);
  ellipse(cr, 300, 12, 9, 4);
  stroke(cr, kWoodLo, 1.4);
  for (const double x : {10.0, W - 10})
  {
    circle(cr, x, 12, 3);
    setRgba(cr, rgb(70, 70, 80), 1.0);
    cairo_fill(cr);
  }
  cairo_rectangle(cr, 0, 1, W, 2);
  setRgba(cr, rgb(255, 230, 180), 0.6);
  cairo_fill(cr);
  cairo_pattern_t* g = cairo_pattern_create_linear(0, 24, 0, 40);
  stop(g, 0.0, kInk, 0.5);
  stop(g, 1.0, kInk, 0.0);
  cairo_rectangle(cr, 0, 24, W, 16);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
}

// The sticky note (200 x 150; its top edge the ledge): a big yellow note
// stuck to the wall, its corner curling up, a doodle and "CALL MAX".
void paintStickyNote(cairo_t* cr)
{
  cairo_move_to(cr, 8, 10);
  cairo_line_to(cr, 200, 10);
  cairo_line_to(cr, 198, 150);
  cairo_line_to(cr, 10, 146);
  cairo_close_path(cr);
  setRgba(cr, kInk, 0.3);
  cairo_fill(cr);
  cairo_move_to(cr, 4, 0);
  cairo_line_to(cr, 192, 0);
  cairo_line_to(cr, 192, 112);
  cairo_curve_to(cr, 186, 130, 170, 138, 150, 142);
  cairo_line_to(cr, 4, 140);
  cairo_close_path(cr);
  lgrad(cr, 0, 0, 0, 142, {{0.0, rgb(255, 238, 130)}, {0.15, kNote}, {1.0, rgb(240, 206, 70)}}, 1.0, true);
  stroke(cr, kInk, 1.4, 0.8);
  // The glue strip's sheen along the top.
  cairo_rectangle(cr, 4, 0, 188, 14);
  setRgba(cr, rgb(255, 255, 220), 0.4);
  cairo_fill(cr);
  // The curl.
  cairo_move_to(cr, 192, 112);
  cairo_curve_to(cr, 178, 116, 164, 128, 150, 142);
  cairo_curve_to(cr, 170, 130, 184, 124, 192, 112);
  setRgba(cr, rgb(210, 176, 50), 1.0);
  cairo_fill(cr);
  lettering(cr, "CALL MAX!", 96, 58, 22, rgb(40, 50, 140), 0.9, true);
  cairo_move_to(cr, 30, 76);
  cairo_curve_to(cr, 70, 70, 120, 82, 160, 72);
  stroke(cr, rgb(40, 50, 140), 2.2, 0.8);
  // A doodle: a little runner, a stick figure, mid-leap.
  circle(cr, 60, 100, 6);
  cairo_move_to(cr, 60, 106);
  cairo_line_to(cr, 64, 122);
  cairo_move_to(cr, 64, 122);
  cairo_line_to(cr, 54, 132);
  cairo_move_to(cr, 64, 122);
  cairo_line_to(cr, 76, 128);
  cairo_move_to(cr, 50, 110);
  cairo_line_to(cr, 72, 108);
  stroke(cr, rgb(40, 50, 140), 2.0, 0.8);
  lettering(cr, "42?", 140, 118, 16, kRedInk, 0.8);
}

// The original pitch (150 x 110, its anchor at the base's middle): a
// yellowed, rolled script tied with red ribbon, a luggage tag on it.
void paintPitchScroll(cairo_t* cr)
{
  glow(cr, 75, 60, 70, rgb(255, 230, 140), 0.5);
  cairo_rectangle(cr, 18, 48, 110, 46);
  lgrad(cr, 0, 48, 0, 94, {{0.0, rgb(255, 244, 200)}, {0.5, rgb(234, 214, 160)}, {1.0, rgb(180, 150, 100)}}, 1.0, true);
  stroke(cr, kInk, 1.8);
  for (const double x : {18.0, 128.0})
  {
    ellipse(cr, x, 71, 8, 23);
    lgrad(cr, 0, 48, 0, 94, {{0.0, rgb(255, 250, 220)}, {1.0, rgb(190, 160, 110)}}, 1.0, true);
    stroke(cr, kInk, 1.6);
  }
  ellipse(cr, 128, 71, 3, 12);
  stroke(cr, rgb(150, 120, 80), 1.0);
  cairo_rectangle(cr, 66, 46, 12, 50);
  setRgba(cr, kRedInk, 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.2);
  cairo_move_to(cr, 72, 48);
  cairo_curve_to(cr, 60, 30, 50, 34, 52, 44);
  cairo_curve_to(cr, 56, 50, 66, 48, 72, 48);
  cairo_move_to(cr, 72, 48);
  cairo_curve_to(cr, 84, 30, 94, 34, 92, 44);
  cairo_curve_to(cr, 88, 50, 78, 48, 72, 48);
  setRgba(cr, kRedInk, 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.2);
  // The tag.
  cairo_move_to(cr, 72, 50);
  cairo_line_to(cr, 100, 20);
  stroke(cr, rgb(120, 90, 60), 1.2);
  cairo_save(cr);
  cairo_translate(cr, 112, 14);
  cairo_rotate(cr, 0.2);
  roundRect(cr, -22, -12, 50, 24, 4);
  setRgba(cr, kPaper, 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.0);
  lettering(cr, "THE PITCH", 3, 4, 8, kInk);
  cairo_restore(cr);
}

// The egg (48 x 56, anchor at its foot): a painted Easter egg, zigzags and
// dots; `cracks` 0..3 how far it has hatched.
void paintEgg(cairo_t* cr, int cracks)
{
  ellipse(cr, 24, 53, 16, 3);
  setRgba(cr, kInk, 0.3);
  cairo_fill(cr);
  cairo_move_to(cr, 24, 4);
  cairo_curve_to(cr, 40, 4, 44, 40, 40, 46);
  cairo_curve_to(cr, 36, 54, 12, 54, 8, 46);
  cairo_curve_to(cr, 4, 40, 8, 4, 24, 4);
  cairo_close_path(cr);
  lgrad(cr, 6, 4, 42, 54, {{0.0, rgb(255, 250, 255)}, {0.5, rgb(200, 230, 255)}, {1.0, rgb(150, 180, 230)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  cairo_save(cr);
  cairo_move_to(cr, 24, 4);
  cairo_curve_to(cr, 40, 4, 44, 40, 40, 46);
  cairo_curve_to(cr, 36, 54, 12, 54, 8, 46);
  cairo_curve_to(cr, 4, 40, 8, 4, 24, 4);
  cairo_clip(cr);
  cairo_move_to(cr, 4, 26);
  for (int k = 0; k < 8; ++k)
    cairo_line_to(cr, 8 + k * 5, k % 2 ? 22 : 30);
  stroke(cr, rgb(240, 90, 150), 3.0);
  cairo_rectangle(cr, 4, 36, 40, 4);
  setRgba(cr, rgb(250, 200, 40), 1.0);
  cairo_fill(cr);
  for (int k = 0; k < 4; ++k)
    circle(cr, 12 + k * 8, 14 + (k % 2) * 3, 2.2);
  setRgba(cr, rgb(90, 190, 110), 1.0);
  cairo_fill(cr);
  cairo_restore(cr);
  if (cracks >= 1)
  {
    cairo_move_to(cr, 16, 20);
    cairo_line_to(cr, 20, 26);
    cairo_line_to(cr, 24, 22);
    stroke(cr, kInk, 1.4);
  }
  if (cracks >= 2)
  {
    cairo_move_to(cr, 24, 22);
    cairo_line_to(cr, 28, 28);
    cairo_line_to(cr, 33, 22);
    stroke(cr, kInk, 1.4);
  }
  if (cracks >= 3)
  {
    cairo_move_to(cr, 8, 24);
    cairo_line_to(cr, 16, 20);
    cairo_move_to(cr, 33, 22);
    cairo_line_to(cr, 41, 25);
    stroke(cr, kInk, 1.4);
    circle(cr, 22, 16, 2);
    setRgba(cr, rgb(250, 210, 40), 1.0);
    cairo_fill(cr);
  }
}

// The chick (52 x 52, anchor at its feet): a fluffy yellow ball, beak,
// little wings, an eggshell cap; `hop` 0/1.
void paintChick(cairo_t* cr, int hop)
{
  const double up = hop ? 4 : 0;
  cairo_move_to(cr, 20, 50 - up);
  cairo_line_to(cr, 22, 44 - up);
  cairo_move_to(cr, 30, 50 - up);
  cairo_line_to(cr, 30, 44 - up);
  stroke(cr, rgb(240, 140, 30), 2.0);
  circle(cr, 26, 30 - up, 16);
  lgrad(cr, 0, 14 - up, 0, 46 - up, {{0.0, rgb(255, 250, 160)}, {0.6, rgb(255, 220, 60)}, {1.0, rgb(230, 170, 20)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  for (int k = 0; k < 7; ++k)
  {
    const double a = -kPi * 0.9 + k * 0.3;
    cairo_move_to(cr, 26 + std::cos(a) * 15, 30 - up + std::sin(a) * 15);
    cairo_line_to(cr, 26 + std::cos(a) * 18, 30 - up + std::sin(a) * 18);
  }
  stroke(cr, rgb(255, 230, 90), 1.2);
  cairo_move_to(cr, 16, 32 - up);
  cairo_curve_to(cr, 10, 30 - up, 8, 38 - up, 14, 40 - up);
  stroke(cr, rgb(220, 160, 20), 1.6);
  circle(cr, 32, 26 - up, 2.6);
  setRgba(cr, kInk, 1.0);
  cairo_fill(cr);
  cairo_move_to(cr, 40, 28 - up);
  cairo_line_to(cr, 48, 30 - up);
  cairo_line_to(cr, 40, 33 - up);
  cairo_close_path(cr);
  setRgba(cr, rgb(250, 140, 30), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.0);
  // The shell cap.
  cairo_move_to(cr, 14, 18 - up);
  cairo_line_to(cr, 18, 12 - up);
  cairo_line_to(cr, 22, 16 - up);
  cairo_line_to(cr, 27, 10 - up);
  cairo_line_to(cr, 32, 15 - up);
  cairo_line_to(cr, 36, 12 - up);
  cairo_line_to(cr, 38, 18 - up);
  cairo_curve_to(cr, 32, 8 - up, 20, 8 - up, 14, 18 - up);
  cairo_close_path(cr);
  setRgba(cr, rgb(210, 236, 255), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.2);
}

// The Red Pen's icon (64 x 64): a fat red marker on the slant, its cap
// clipped on the back, a white label, the felt nib wet, a red tick drawn.
void paintRedPenIcon(cairo_t* cr)
{
  cairo_move_to(cr, 6, 54);
  cairo_curve_to(cr, 14, 48, 18, 60, 24, 52);
  cairo_curve_to(cr, 28, 46, 30, 56, 36, 50);
  stroke(cr, kRedInk, 3.0);
  cairo_save(cr);
  cairo_translate(cr, 34, 30);
  cairo_rotate(cr, -0.75);
  roundRect(cr, -24, -8, 40, 16, 5);
  lgrad(cr, 0, -8, 0, 8, {{0.0, rgb(255, 120, 110)}, {0.4, kRedInk}, {1.0, rgb(120, 10, 16)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  roundRect(cr, -12, -5, 18, 10, 2);
  setRgba(cr, rgb(250, 248, 240), 1.0);
  cairo_fill(cr);
  lettering(cr, "RED", -3, 3.2, 7, kRedInk);
  // The cap on the back end and its clip.
  roundRect(cr, 14, -9, 16, 18, 4);
  lgrad(cr, 0, -9, 0, 9, {{0.0, rgb(255, 130, 120)}, {1.0, rgb(140, 14, 20)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  cairo_rectangle(cr, 16, -12, 12, 3);
  setRgba(cr, rgb(220, 224, 232), 1.0);
  cairo_fill(cr);
  // The nib.
  cairo_move_to(cr, -24, -5);
  cairo_line_to(cr, -32, -2);
  cairo_line_to(cr, -32, 2);
  cairo_line_to(cr, -24, 5);
  cairo_close_path(cr);
  setRgba(cr, rgb(250, 240, 230), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.2);
  cairo_move_to(cr, -32, -2);
  cairo_line_to(cr, -36, 0);
  cairo_line_to(cr, -32, 2);
  cairo_close_path(cr);
  setRgba(cr, kRedInk, 1.0);
  cairo_fill(cr);
  cairo_restore(cr);
}

// A Breakout brick (136 x 72, its top-left 4 px out from its 4 x 2 cells):
// a block of typed paper with the letter big in the middle; red ink ones
// edged in red, `cracked` torn across; a gem tucked in its corner.
void paintBrick(cairo_t* cr, char ch, bool red, bool cracked, bool gem)
{
  roundRect(cr, 8, 8, 128, 64, 4);
  setRgba(cr, kInk, 0.35);
  cairo_fill(cr);
  roundRect(cr, 4, 4, 128, 64, 4);
  lgrad(cr, 0, 4, 0, 68, {{0.0, rgb(255, 254, 246)}, {0.8, kPaper}, {1.0, kPaperLo}}, 1.0, true);
  stroke(cr, red ? kRedInk : kInk, red ? 3.0 : 1.8);
  cairo_move_to(cr, 4, 16);
  cairo_line_to(cr, 132, 16);
  stroke(cr, rgb(150, 180, 220), 0.9, 0.7);
  const char s[2] = {ch, 0};
  selectGameFont(cr);
  cairo_set_font_size(cr, 42);
  cairo_text_extents_t e;
  cairo_text_extents(cr, s, &e);
  cairo_move_to(cr, 68 - e.width * 0.5 - e.x_bearing, 57);
  setRgba(cr, red ? kRedInk : kInk, 1.0);
  cairo_show_text(cr, s);
  if (cracked)
  {
    cairo_move_to(cr, 30, 4);
    cairo_line_to(cr, 44, 26);
    cairo_line_to(cr, 36, 40);
    cairo_line_to(cr, 56, 68);
    cairo_move_to(cr, 44, 26);
    cairo_line_to(cr, 70, 30);
    stroke(cr, kInk, 2.0);
    cairo_move_to(cr, 110, 4);
    cairo_line_to(cr, 132, 4);
    cairo_line_to(cr, 132, 22);
    cairo_close_path(cr);
    setRgba(cr, rgb(60, 40, 30), 0.8);
    cairo_fill(cr);
  }
  if (gem)
  {
    cairo_move_to(cr, 118, 48);
    cairo_line_to(cr, 126, 56);
    cairo_line_to(cr, 118, 64);
    cairo_line_to(cr, 110, 56);
    cairo_close_path(cr);
    lgrad(cr, 110, 48, 126, 64, {{0.0, rgb(200, 255, 255)}, {1.0, rgb(40, 160, 230)}}, 1.0, true);
    stroke(cr, kInk, 1.0);
  }
}

// The ball: a crumpled-up page (40 x 40, centred).
void paintPaperBall(cairo_t* cr)
{
  cairo_translate(cr, 5, 5);
  paintCrumple(cr, 7);
}

// The ruler the runner holds up as the paddle (200 x 26, centred): yellow
// wood, inch marks, numbers.
void paintRuler(cairo_t* cr)
{
  roundRect(cr, 2, 4, 196, 18, 3);
  lgrad(cr, 0, 4, 0, 22, {{0.0, rgb(255, 236, 150)}, {0.5, rgb(240, 200, 80)}, {1.0, rgb(170, 120, 30)}}, 1.0, true);
  stroke(cr, kInk, 1.8);
  for (int k = 0; k <= 32; ++k)
  {
    cairo_move_to(cr, 6 + k * 5.8, 4);
    cairo_line_to(cr, 6 + k * 5.8, 4 + (k % 4 == 0 ? 8 : 4));
  }
  stroke(cr, kInk, 0.9, 0.9);
  for (int k = 1; k < 8; ++k)
  {
    char b[4];
    std::snprintf(b, sizeof b, "%d", k);
    lettering(cr, b, 6 + k * 23.2, 20, 7, kInk);
  }
}

// --- Live helpers --------------------------------------------------------------------------------

// A strike-through in red marker from (x0, y0) to (x1, y1), `p` 0..1 of it
// drawn, wobbling by `seed`.
void drawStrike(Renderer& r, float x0, float y0, float x1, float y1, float p, int seed, float alpha)
{
  if (p <= 0.0f)
    return;
  constexpr int kSeg = 6;
  float px = x0, py = y0;
  for (int k = 1; k <= kSeg; ++k)
  {
    const float u = float(k) / float(kSeg);
    if (u - 1.0f / float(kSeg) >= p)
      break;
    const float uu = std::min(u, p);
    const float wob = k < kSeg ? (unit(hash2(seed, k)) - 0.5f) * 6.0f : 0.0f;
    const float nx = x0 + (x1 - x0) * uu, ny = y0 + (y1 - y0) * uu + (uu < u ? 0.0f : wob);
    r.drawLine(px, py, nx, ny, 9.0f, withAlpha(rgb(150, 10, 20), int(200.0f * alpha)));
    r.drawLine(px, py, nx, ny, 6.0f, withAlpha(kRedInk, int(255.0f * alpha)));
    r.drawLine(px, py - 1.5f, nx, ny - 1.5f, 2.0f, withAlpha(kRedHi, int(200.0f * alpha)));
    px = nx;
    py = ny;
  }
}

const Texture& glyph(const Art& art, const Renderer& r, char c, int style)
{
  const std::string key = std::string("glyph/") + c + "/" + std::to_string(style);
  return baked(art, r, key, 56, 68, 28.0f, 34.0f, [c, style](cairo_t* cr) { paintGlyph(cr, c, style); });
}

// A word in marker across (x, y, w x 64 px): the first `shown` letters of
// it, each a little tilted and off the line like handwriting.
void drawMarkerWord(Renderer& r, const Art& art, const std::string& text, float x, float y, float w, int shown, float alpha, int style,
  int seed)
{
  const int n = int(text.size());
  if (n == 0 || alpha <= 0.0f)
    return;
  const float slot = w / float(n);
  for (int i = 0; i < std::min(n, shown); ++i)
  {
    const char c = text[std::size_t(i)];
    if (c == ' ')
      continue;
    const std::uint32_t h = hash2(seed * 31 + i, 3811);
    DrawOpts o;
    o.alpha = alpha;
    o.angle = (unit(h) - 0.5f) * 14.0f;
    o.scale = std::min(1.0f, slot / 30.0f);
    r.draw(glyph(art, r, c, style), x + slot * (float(i) + 0.5f), y + 32.0f + (unit(h >> 7) - 0.5f) * 6.0f, o);
  }
}

// Ink scribbled at (x, y): a few quick loops, new every frame.
void drawScribble(Renderer& r, float x, float y, int frame, Color c)
{
  float px = x, py = y;
  for (int k = 0; k < 5; ++k)
  {
    const std::uint32_t h = hash2(frame, k + 3813);
    const float nx = x + (unit(h) - 0.5f) * 26.0f, ny = y + (unit(h >> 8) - 0.5f) * 20.0f;
    r.drawLine(px, py, nx, ny, 3.0f, c);
    px = nx;
    py = ny;
  }
}

// A dashed pencil outline round (x, y, w, h): where something is about to be.
void drawGhostBox(Renderer& r, float x, float y, float w, float h, float a)
{
  const Color c = withAlpha(kGraphite, int(220.0f * a));
  for (float k = 0; k < w; k += 20.0f)
  {
    r.drawLine(x + k, y, x + std::min(w, k + 11.0f), y, 2.5f, c);
    r.drawLine(x + k, y + h, x + std::min(w, k + 11.0f), y + h, 2.5f, c);
  }
  for (float k = 0; k < h; k += 20.0f)
  {
    r.drawLine(x, y + k, x, y + std::min(h, k + 11.0f), 2.5f, c);
    r.drawLine(x + w, y + k, x + w, y + std::min(h, k + 11.0f), 2.5f, c);
  }
}

// Eraser crumbs falling off something being struck out.
void drawCrumbs(Renderer& r, float x, float y, float w, float h, int t, int seed)
{
  for (int k = 0; k < 10; ++k)
  {
    const std::uint32_t hh = hash2(seed, k + 3817);
    const float cx = x + unit(hh) * w, cy = y + unit(hh >> 9) * h + float(t) * (3.0f + float(k % 3));
    r.fillRect(cx, cy, 5.0f, 4.0f, withAlpha(rgb(230, 150, 160), std::max(0, 220 - t * 14)));
  }
}

// The typed sentence on the paper: a character every `step` px from x, the
// typed ones in ink (a little off their line, as a typewriter types), the
// rest faint; the next one marked.
void drawSentence(Renderer& r, const std::string& text, int typed, float x, float y, float step, float size, int frame, bool wrong)
{
  for (int i = 0; i < int(text.size()); ++i)
  {
    const char c[2] = {text[std::size_t(i)], 0};
    const float cx = x + step * (float(i) + 0.5f);
    if (i < typed)
    {
      const float jy = (unit(hash2(i, 3821)) - 0.5f) * 5.0f;
      r.drawText(c, cx, y + jy, {size, rgb(20, 18, 26), 0}, Align::Center);
    }
    else
      r.drawText(c, cx, y, {size, rgb(214, 206, 188), 0}, Align::Center);
    if (i == typed && (frame / 12) % 2 == 0)
      r.fillRect(cx - step * 0.38f, y + size * 1.12f, step * 0.76f, 6.0f, wrong ? kRedInk : rgb(40, 40, 60));
  }
}

} // namespace

// --- Blocks the level draws itself ---------------------------------------------------------------

namespace
{

// The desks: blocks x0..x1 on rows 55-57.
struct DeskSpan
{
  int x0, x1;
};
constexpr DeskSpan kDesks[3] = {{41, 52}, {59, 73}, {87, 106}};
constexpr int kDeskTop = 55, kDeskBottom = 57;

bool inCabinet(int tx, int ty) { return tx >= 2 && tx <= 6 && ty >= 35 && ty <= 39; }

bool onShelf(int tx, int ty)
{
  return (ty == 10 && tx >= 7 && tx <= 9) || (ty == 14 && tx >= 5 && tx <= 6) || (ty == 18 && tx >= 2 && tx <= 3);
}

bool stickyNote(int tx, int ty) { return ty == 56 && tx >= 5 && tx <= 7; }

} // namespace

bool World::writersDrawsBlock(int tx, int ty) const
{
  const auto& s = mWriters;
  if (s.breakout)
    return false;
  const Tile t = mMap.block(tx, ty);
  for (const auto& k : s.keys)
    if (ty == k.y && (tx == k.x || tx == k.x + 1))
      return true;
  for (const auto& b : s.blocks)
    if (b.stampX >= 0 && tx >= b.stampX && tx < b.stampX + 2 && ty >= b.stampY && ty < b.stampY + 2)
      return true;
  if (ty == s.bridge.row && tx >= s.bridge.x0 && tx <= s.bridge.x1 && t == Tile::Solid)
    return true;
  for (const auto& w : s.words)
    if (w.target == WordTarget::Rect && tx >= w.x0 && tx <= w.x1 && ty >= w.y0 && ty <= w.y1)
    {
      // Walls and doors always; the plank always; a ladder or the pencils
      // only while they fade.
      if (t == Tile::Solid || (t == Tile::Platform && w.load))
        return true;
      if (w.state == WordState::Striking && (t == Tile::Ladder || t == Tile::Spikes || t == Tile::Platform))
        return true;
    }
  if (mLevel->themeKey != "writers_room")
    return false;
  if (t == Tile::Solid)
  {
    for (const auto& d : kDesks)
      if (tx >= d.x0 && tx <= d.x1 && ty >= kDeskTop && ty <= kDeskBottom)
        return true;
    if (inCabinet(tx, ty) || onShelf(tx, ty))
      return true;
  }
  if (t == Tile::Platform && stickyNote(tx, ty))
    return true;
  return false;
}

// --- The set, behind the tiles ---------------------------------------------------------------------

void drawWritersSet(Renderer& r, const World& w, const Art& art, float camX, float camY, int frame, float alpha)
{
  (void)alpha;
  const auto& s = w.writers();
  auto bx = [&](float tx) { return tx * kTilePx - camX; };
  auto by = [&](float ty) { return ty * kTilePx - camY; };
  auto put = [&](const Texture& t, float tx, float ty, const DrawOpts& o = {}) {
    const float x = bx(tx), y = by(ty);
    if (!offScreen(x - t.anchorX() * o.scale, y - t.anchorY() * o.scale, float(t.w()) * o.scale, float(t.h()) * o.scale))
      r.draw(t, x, y, o);
  };
  const Texture& cork = baked(art, r, "cork", 256, 256, 0.0f, 0.0f, paintCork);
  // Cork tiles over the block rect (x0, y0) .. (x1, y1), the screen's part.
  auto corkWall = [&](float x0, float y0, float x1, float y1) {
    const float sx0 = std::max(bx(x0), -256.0f), sx1 = std::min(bx(x1), float(kScreenW) + 256.0f);
    const float sy0 = std::max(by(y0), -256.0f), sy1 = std::min(by(y1), float(kScreenH) + 256.0f);
    if (sx0 >= sx1 || sy0 >= sy1)
      return;
    const float ox = bx(x0), oy = by(y0);
    for (float y = oy + std::floor((sy0 - oy) / 256.0f) * 256.0f; y < sy1; y += 256.0f)
      for (float x = ox + std::floor((sx0 - ox) / 256.0f) * 256.0f; x < sx1; x += 256.0f)
        r.draw(cork, x, y);
    // Clip the tiles' overhang with the frame: wooden bars round the board.
    const float L = bx(x0), T = by(y0), R = bx(x1), B = by(y1);
    r.fillRect(L - 18.0f, T - 18.0f, R - L + 36.0f, 22.0f, kWoodLo);
    r.fillRect(L - 18.0f, B - 4.0f, R - L + 36.0f, 22.0f, kWoodLo);
    r.fillRect(L - 18.0f, T - 18.0f, 22.0f, B - T + 36.0f, kWoodLo);
    r.fillRect(R - 4.0f, T - 18.0f, 22.0f, B - T + 36.0f, kWoodLo);
    r.fillRect(L - 18.0f, T - 18.0f, R - L + 36.0f, 5.0f, kWoodHi);
    r.fillRect(L - 18.0f, T - 18.0f, 5.0f, B - T + 36.0f, kWoodHi);
  };

  if (s.breakout)
  {
    // The Breakout bonus: a corkboard wall of rejected pages, the motto
    // pinned up over it.
    corkWall(1.0f, 1.0f, 39.0f, 22.0f);
    for (int k = 0; k < 14; ++k)
    {
      const std::uint32_t h = hash2(k, 3831);
      DrawOpts o;
      o.angle = (unit(h) - 0.5f) * 16.0f;
      o.alpha = 0.55f;
      put(baked(art, r, "card/" + std::to_string(k % 12), 120, 84, 60.0f, 42.0f, [k](cairo_t* cr) { paintIndexCard(cr, k); }),
        2.5f + float(k % 7) * 5.3f, 12.5f + float(k / 7) * 4.0f + unit(h >> 6) * 1.5f, o);
    }
    return;
  }
  const auto& map = w.map();

  // --- The doorway (F1, 0-11): the door in, a coat rack, the sign, the note.
  {
    const float x = bx(0.6f), y = by(52.4f);
    if (!offScreen(x, y, 200.0f, 360.0f))
    {
      r.fillRect(x, y, 150.0f, 360.0f, rgb(60, 40, 30));
      r.fillRect(x + 10.0f, y + 10.0f, 130.0f, 350.0f, rgb(110, 76, 50));
      r.fillRect(x + 22.0f, y + 26.0f, 106.0f, 120.0f, rgb(200, 220, 230));
      r.fillRect(x + 22.0f, y + 26.0f, 106.0f, 6.0f, rgb(240, 250, 255));
      r.drawText("WRITERS", x + 75.0f, y + 70.0f, {18.0f, rgb(40, 40, 50), 0}, Align::Center);
      r.drawText("ROOM", x + 75.0f, y + 92.0f, {18.0f, rgb(40, 40, 50), 0}, Align::Center);
      r.fillRect(x + 116.0f, y + 190.0f, 14.0f, 30.0f, rgb(220, 180, 90));
    }
    const float sx = bx(3.4f), sy = by(46.0f);
    if (!offScreen(sx, sy, 420.0f, 80.0f))
    {
      r.fillRect(sx, sy, 420.0f, 70.0f, rgb(30, 30, 40));
      r.fillRect(sx + 6.0f, sy + 6.0f, 408.0f, 58.0f, rgb(240, 236, 220));
      r.drawText("WRITERS AT WORK", sx + 210.0f, sy + 10.0f, {24.0f, kRedInk, 0}, Align::Center);
      r.drawText("QUIET PLEASE - THE SHOW IS BEING WRITTEN", sx + 210.0f, sy + 40.0f, {13.0f, rgb(40, 40, 50), 0}, Align::Center);
    }
    // The coat rack: a pole, hooks, a jacket and a scarf.
    const float cx = bx(9.2f), floorY = by(58.0f);
    if (!offScreen(cx - 80.0f, floorY - 320.0f, 160.0f, 330.0f))
    {
      r.fillRect(cx - 6.0f, floorY - 300.0f, 12.0f, 300.0f, rgb(90, 60, 36));
      r.fillRect(cx - 40.0f, floorY - 10.0f, 80.0f, 10.0f, rgb(70, 46, 28));
      r.drawLine(cx, floorY - 296.0f, cx - 34.0f, floorY - 316.0f, 6.0f, rgb(90, 60, 36));
      r.drawLine(cx, floorY - 296.0f, cx + 34.0f, floorY - 316.0f, 6.0f, rgb(90, 60, 36));
      r.fillRect(cx - 56.0f, floorY - 290.0f, 50.0f, 140.0f, rgb(110, 60, 50));
      r.fillRect(cx - 52.0f, floorY - 290.0f, 8.0f, 140.0f, rgb(150, 90, 70));
      r.fillRect(cx + 10.0f, floorY - 290.0f, 16.0f, 170.0f, rgb(60, 110, 160));
      r.fillRect(cx + 10.0f, floorY - 140.0f, 16.0f, 6.0f, rgb(230, 230, 230));
    }
    put(baked(art, r, "sticky", 204, 152, 4.0f, 0.0f, paintStickyNote), 5.0f, 56.0f);
  }

  // --- The desks (F1): oak, their tops cluttered.
  for (int d = 0; d < 3; ++d)
  {
    const auto& dk = kDesks[d];
    const int n = dk.x1 - dk.x0 + 1;
    const float x = bx(float(dk.x0)), y = by(float(kDeskTop));
    if (offScreen(x - 200.0f, y - 260.0f, float(n) * kTilePx + 400.0f, 460.0f))
      continue;
    // Clutter at the back of the top, behind the runner.
    struct Thing
    {
      float at;
      int kind, seed;
    };
    static constexpr Thing kThings[3][7] = {
      {{1.4f, 2, 0}, {3.6f, 0, 1}, {5.2f, 1, 2}, {7.0f, 3, 3}, {8.6f, 0, 4}, {10.4f, 4, 5}, {11.2f, 4, 6}},
      {{1.2f, 0, 7}, {2.8f, 0, 8}, {4.6f, 1, 9}, {6.4f, 3, 10}, {8.4f, 2, 11}, {11.0f, 0, 12}, {13.6f, 1, 13}},
      {{1.0f, 3, 14}, {2.6f, 0, 15}, {5.4f, 2, 16}, {9.0f, 0, 17}, {12.6f, 1, 18}, {14.8f, 4, 19}, {18.2f, 2, 20}}};
    for (const auto& t : kThings[d])
    {
      const float tx = float(dk.x0) + t.at, ty = float(kDeskTop) + 0.08f;
      switch (t.kind)
      {
        case 0:
          put(baked(art, r, "stack/" + std::to_string(t.seed % 6), 92, 72, 46.0f, 70.0f, [t](cairo_t* cr) { paintPaperStack(cr, t.seed); }), tx,
            ty);
          break;
        case 1:
          put(baked(art, r, "mug/" + std::to_string(t.seed % 6), 48, 58, 22.0f, 56.0f, [t](cairo_t* cr) { paintMug(cr, t.seed); }), tx, ty);
          break;
        case 2:
        {
          // The lamp and its pool of warm light on the desk and the wall.
          const float lx = bx(tx), ly = by(ty);
          drawGlow(r, art, lx + 10.0f, ly - 30.0f, 170.0f, kLamp, 0.35f);
          drawGlow(r, art, lx + 10.0f, ly - 4.0f, 90.0f, rgb(255, 230, 170), 0.35f);
          put(baked(art, r, "lamp", 120, 122, 60.0f, 120.0f, paintLamp), tx, ty);
          break;
        }
        case 3:
          put(baked(art, r, "cup/" + std::to_string(t.seed % 4), 40, 70, 20.0f, 68.0f, [t](cairo_t* cr) { paintPencilCup(cr, t.seed); }), tx,
            ty);
          break;
        default:
          put(baked(art, r, "crumple/" + std::to_string(t.seed % 4), 30, 30, 15.0f, 27.0f, [t](cairo_t* cr) { paintCrumple(cr, t.seed); }),
            tx, ty);
          break;
      }
    }
    r.draw(baked(art, r, "desk/" + std::to_string(d), n * 64, 192, 0.0f, 0.0f, [n, d](cairo_t* cr) { paintDesk(cr, n, d); }), x, y);
  }
  // The FLOOR pit: the dark under the floorboards, joists.
  {
    const float x = bx(74.0f), y = by(56.0f);
    if (!offScreen(x, y, 12.0f * kTilePx, 6.0f * kTilePx))
    {
      // The gap between the desks darkening down into it.
      for (int k = 0; k < 8; ++k)
        r.fillRect(std::floor(x), std::floor(by(56.0f)) + float(k) * 16.0f, 12.0f * kTilePx, 16.0f, rgba(20, 16, 20, 60 + k * 24));
      r.fillRect(x, by(58.0f), 12.0f * kTilePx, 4.0f * kTilePx, rgb(20, 16, 20));
      for (int k = 0; k < 4; ++k)
        r.fillRect(x + float(k) * 3.0f * kTilePx + 40.0f, by(58.0f), 26.0f, 4.0f * kTilePx, rgb(46, 34, 26));
      r.fillRect(x, by(59.5f), 12.0f * kTilePx, 14.0f, rgb(60, 50, 40));
      r.drawText("MIND THE GAP", x + 6.0f * kTilePx, by(60.4f), {22.0f, rgb(240, 200, 60), kHudInk}, Align::Center);
    }
  }

  // --- The corkboard (F2, 30-119): cork in a frame, index cards and loose
  // red yarn between them, the rejected pitches.
  corkWall(29.6f, 24.0f, 120.4f, 41.9f);
  {
    const float x0 = bx(29.6f), x1 = bx(120.4f);
    if (x1 > -200.0f && x0 < float(kScreenW) + 200.0f)
    {
      // Index cards on a loose grid, a pin each; red yarn linking some.
      std::array<std::array<float, 2>, 64> at{};
      int n = 0;
      for (int k = 0; k < 64; ++k)
      {
        const std::uint32_t h = hash2(k, 3841);
        const float cx = 31.0f + float(k % 16) * 5.6f + unit(h) * 2.0f, cy = 24.8f + float(k / 16) * 2.6f + unit(h >> 8) * 1.1f;
        at[std::size_t(n++)] = {cx, cy};
        const float sx = bx(cx), sy = by(cy);
        if (offScreen(sx - 70.0f, sy - 50.0f, 140.0f, 100.0f))
          continue;
        DrawOpts o;
        o.angle = (unit(h >> 16) - 0.5f) * 14.0f;
        r.draw(baked(art, r, "card/" + std::to_string(k % 12), 120, 84, 60.0f, 42.0f, [k](cairo_t* cr) { paintIndexCard(cr, k); }), sx, sy, o);
      }
      for (int k = 0; k + 1 < n; ++k)
      {
        if (hash2(k, 3843) % 3u != 0)
          continue;
        const int j = std::min(n - 1, k + 1 + int(hash2(k, 3845) % 17u));
        const float ax = bx(at[std::size_t(k)][0]), ay = by(at[std::size_t(k)][1]) - 34.0f;
        const float cx = bx(at[std::size_t(j)][0]), cy = by(at[std::size_t(j)][1]) - 34.0f;
        if (std::max(ax, cx) < -50.0f || std::min(ax, cx) > float(kScreenW) + 50.0f)
          continue;
        r.drawLine(ax, ay, cx, cy, 3.0f, rgba(150, 20, 30, 200));
        r.drawLine(ax, ay - 1.0f, cx, cy - 1.0f, 1.2f, rgba(255, 90, 90, 160));
      }
      for (int k = 0; k < n; ++k)
      {
        const float sx = bx(at[std::size_t(k)][0]), sy = by(at[std::size_t(k)][1]) - 34.0f;
        if (offScreen(sx - 20.0f, sy - 20.0f, 40.0f, 40.0f))
          continue;
        static constexpr Color kPins[4] = {rgb(230, 40, 50), rgb(250, 200, 40), rgb(60, 140, 230), rgb(80, 200, 110)};
        DrawOpts po;
        po.scale = 0.6f;
        r.draw(baked(art, r, "pin/" + std::to_string(k % 4), 30, 30, 15.0f, 15.0f, [k](cairo_t* cr) { paintPin(cr, kPins[k % 4]); }), sx, sy, po);
      }
      // A polaroid of a suspect under each of the strings' pins.
      for (std::size_t i = 0; i + 1 < s.pins.size(); i += 2)
      {
        const float px = float(s.pins[i] + 1) * kCellPx - camX, py = float(s.pins[i + 1]) * kCellPx - camY;
        if (offScreen(px - 60.0f, py - 20.0f, 120.0f, 140.0f))
          continue;
        DrawOpts o;
        o.angle = (unit(hash2(int(i), 3847)) - 0.5f) * 12.0f;
        const int who = int(i / 2);
        r.draw(baked(art, r, "polaroid/" + std::to_string(who % 7), 100, 116, 48.0f, 8.0f, [who](cairo_t* cr) { paintPolaroid(cr, who); }), px,
          py + 8.0f, o);
      }
    }
  }
  // The rejected pitches (shots spin them).
  for (std::size_t i = 0; i < s.cards.size(); ++i)
  {
    const auto& c = s.cards[i];
    const float x = (float(c.x) + 2.0f) * kCellPx - camX, y = (float(c.y) + 1.5f) * kCellPx - camY;
    if (offScreen(x - 120.0f, y - 80.0f, 240.0f, 160.0f))
      continue;
    const float turn = std::cos(float(c.spin) * 0.52f);
    const float wide = std::abs(turn);
    const int phase = wide > 0.85f ? 0 : wide > 0.55f ? 1 : wide > 0.25f ? 2 : 3;
    const bool back = turn < 0.0f;
    DrawOpts o;
    o.angle = c.spin > 0 ? 0.0f : (float(int(i) % 3) - 1.0f) * 4.0f;
    const std::string key = "pitch/" + std::to_string(i) + "/" + std::to_string(phase) + (back ? "b" : "");
    const std::string text = c.text;
    r.draw(baked(art, r, key, 216, 140, 108.0f, 70.0f, [text, phase, back](cairo_t* cr) { paintPitchCard(cr, text, phase, back); }), x, y, o);
    DrawOpts po;
    po.scale = 0.8f;
    r.draw(baked(art, r, "pin/0", 30, 30, 15.0f, 15.0f, [](cairo_t* cr) { paintPin(cr, rgb(230, 40, 50)); }), x, y - 62.0f, po);
  }
  // The filing cabinet's shell and its snack drawer.
  put(baked(art, r, "cabinet_in", 320, 320, 0.0f, 0.0f, paintCabinetInside), 2.0f, 35.0f);
  // The top shelf's binders (Nova's way up to the original pitch).
  put(baked(art, r, "binders/3", 192, 64, 0.0f, 0.0f, [](cairo_t* cr) { paintBinderStack(cr, 3, 0); }), 7.0f, 10.0f);
  put(baked(art, r, "binders/2a", 128, 64, 0.0f, 0.0f, [](cairo_t* cr) { paintBinderStack(cr, 2, 1); }), 5.0f, 14.0f);
  put(baked(art, r, "binders/2b", 128, 64, 0.0f, 0.0f, [](cairo_t* cr) { paintBinderStack(cr, 2, 2); }), 2.0f, 18.0f);

  // --- The typewriter (F3): the paper with the sentence at its line, the
  // roller, the body behind the big keys.
  if (!s.keys.empty())
  {
    const float kx0 = float(s.keys.front().x) - 3.0f, kx1 = float(s.keys.back().x) + 5.0f;
    const float x0 = bx(kx0), x1 = bx(kx1);
    if (x1 > -200.0f && x0 < float(kScreenW) + 200.0f)
    {
      const float bodyTop = by(15.3f);
      // The paper, rising out of the roller; the line being typed sits
      // right over the roller, where the runner can see it.
      const float px0 = bx(kx0 + 4.0f), px1 = bx(kx1 - 4.0f), pTop = by(3.0f), pBot = by(14.7f);
      r.fillRect(px0 + 10.0f, pTop + 10.0f, px1 - px0, pBot - pTop, rgba(0, 0, 0, 70));
      r.fillRect(px0, pTop, px1 - px0, pBot - pTop, kPaper);
      r.fillRect(px0, pTop, px1 - px0, 8.0f, rgb(255, 255, 250));
      r.fillRect(px1 - 30.0f, pTop, 30.0f, pBot - pTop, rgb(236, 230, 214));
      r.fillRect(px0 + 80.0f, pTop, 3.0f, pBot - pTop, rgba(220, 90, 100, 120));
      r.drawText("INT. WRITERS' ROOM - NIGHT", px0 + 120.0f, pTop + 40.0f, {30.0f, rgb(60, 58, 64), 0});
      r.drawText("The runners stand at the gap. There is no bridge. Somebody has to write one.", px0 + 120.0f, pTop + 100.0f,
        {22.0f, rgb(90, 88, 94), 0});
      r.drawText("MAXINE (V.O.)", (px0 + px1) * 0.5f, pTop + 170.0f, {24.0f, rgb(60, 58, 64), 0}, Align::Center);
      r.drawText("Type it. Every letter.", (px0 + px1) * 0.5f, pTop + 206.0f, {22.0f, rgb(90, 88, 94), 0}, Align::Center);
      const float lineY = by(13.15f);
      if (!s.bridge.text.empty())
      {
        const float step = std::min(96.0f, (px1 - px0 - 200.0f) / float(s.bridge.text.size()));
        const float sx = (px0 + px1) * 0.5f - step * float(s.bridge.text.size()) * 0.5f;
        drawSentence(r, s.bridge.text, s.bridge.typed, sx, lineY, step, 64.0f, frame, s.bridge.wrong > 0);
        if (s.bridge.wrong > 0)
          r.drawText("CLACK!", sx + step * (float(s.bridge.typed) + 0.5f), lineY - 34.0f, {28.0f, kRedInk, kHudInk, true}, Align::Center);
        // The type guide at the next letter, over the roller.
        const float gx = sx + step * (float(std::min<int>(s.bridge.typed, int(s.bridge.text.size()) - 1)) + 0.5f);
        r.fillRect(gx - 30.0f, pBot - 14.0f, 60.0f, 16.0f, rgb(170, 176, 190));
        r.fillRect(gx - 30.0f, pBot - 14.0f, 60.0f, 3.0f, rgb(236, 240, 248));
        r.fillRect(gx - 3.0f, pBot - 24.0f, 6.0f, 12.0f, rgb(120, 126, 140));
      }
      // The body.
      const int bw = int((kx1 - kx0) * kTilePx), bh = int((22.0f - 15.3f) * kTilePx) + 2;
      r.draw(baked(art, r, "twbody/" + std::to_string(bw) + "x" + std::to_string(bh), bw, bh, 0.0f, 0.0f,
               [bw, bh](cairo_t* cr) { paintTypewriterBody(cr, bw, bh); }),
        x0, bodyTop);
      // The roller (platen) and its knobs.
      const float rTop = by(14.65f), rBot = by(15.55f);
      static constexpr int kBands[6] = {70, 46, 30, 20, 14, 10};
      for (int b = 0; b < 6; ++b)
      {
        const float t0 = rTop + (rBot - rTop) * float(b) / 6.0f;
        r.fillRect(x0 + 60.0f, t0, x1 - x0 - 120.0f, (rBot - rTop) / 6.0f + 1.0f, rgb(kBands[b], kBands[b], kBands[b] + 6));
      }
      r.fillRect(x0 + 60.0f, rTop + 6.0f, x1 - x0 - 120.0f, 3.0f, rgba(255, 255, 255, 90));
      for (const float kx : {x0 + 10.0f, x1 - 70.0f})
      {
        r.fillRect(kx, rTop - 16.0f, 60.0f, rBot - rTop + 32.0f, rgb(40, 40, 48));
        r.fillRect(kx + 6.0f, rTop - 10.0f, 48.0f, rBot - rTop + 20.0f, rgb(190, 194, 206));
        r.fillRect(kx + 6.0f, rTop - 10.0f, 48.0f, 6.0f, rgb(250, 252, 255));
      }
      // The carriage return lever.
      r.drawLine(x0 + 40.0f, rTop - 10.0f, x0 - 60.0f, rTop - 80.0f, 12.0f, rgb(40, 40, 48));
      r.drawLine(x0 + 40.0f, rTop - 12.0f, x0 - 58.0f, rTop - 82.0f, 5.0f, rgb(210, 214, 226));
    }
  }

  // --- The final draft (F3, 86-149): the script page the lines come off,
  // the bound FINAL DRAFT with PAGE 42 on its cover, the press's housing
  // and its warning sign, the line over the exit.
  {
    const float x0 = bx(93.0f), x1 = bx(115.5f), y0 = by(3.2f), y1 = by(22.0f);
    if (x1 > -60.0f && x0 < float(kScreenW) + 60.0f)
    {
      r.fillRect(x0 + 14.0f, y0 + 14.0f, x1 - x0, y1 - y0, rgba(0, 0, 0, 80));
      r.fillRect(x0, y0, x1 - x0, y1 - y0, rgb(232, 226, 208));
      r.fillRect(x0, y0, x1 - x0, 10.0f, rgb(250, 248, 238));
      // The lamplight falls off down the page and toward its edges.
      for (int k = 0; k < 24; ++k)
      {
        const float t = float(k) / 24.0f;
        const float ya = std::floor(y0 + (y1 - y0) * (0.5f + t * 0.5f)), yb = std::floor(y0 + (y1 - y0) * (0.5f + (t + 1.0f / 24.0f) * 0.5f));
        r.fillRect(std::floor(x0), ya, std::floor(x1) - std::floor(x0), yb - ya, rgba(60, 50, 40, int(4.0f + t * 44.0f)));
      }
      for (int k = 0; k < 10; ++k)
      {
        r.fillRect(std::floor(x0) + float(k) * 6.0f, std::floor(y0), 6.0f, std::floor(y1) - std::floor(y0), rgba(60, 50, 40, 50 - k * 5));
        r.fillRect(std::floor(x1) - float(k + 1) * 6.0f, std::floor(y0), 6.0f, std::floor(y1) - std::floor(y0), rgba(60, 50, 40, 50 - k * 5));
      }
      // The punch holes and the margin rule.
      const Texture& hole = baked(art, r, "hole", 32, 32, 16.0f, 16.0f, [](cairo_t* cr) {
        circle(cr, 16, 16, 13);
        lgrad(cr, 0, 3, 0, 29, {{0.0, rgb(20, 18, 24)}, {1.0, rgb(70, 66, 74)}}, 1.0, true);
        stroke(cr, rgb(150, 144, 130), 1.5);
      });
      for (int h = 0; h < 3; ++h)
        r.draw(hole, x0 + 44.0f, y0 + 140.0f + float(h) * 420.0f);
      r.fillRect(x0 + 84.0f, y0, 2.0f, y1 - y0, rgba(220, 90, 100, 110));
      // The scene, in screenplay layout: action lines from the margin, names
      // and dialogue centred, the transition on the right.
      static const char* const kLines[] = {"38.", "INT. THE FINAL DRAFT - LATER", "", "The runners climb the last page.",
        "Lance's door blocks the way out.", "", "LANCE", "(smug)", "I always win. It's in the script.", "", "DASH",
        "Then we edit the script.", "", "Dash uncaps the red pen. Above the", "page, a press waits for its cue.", "", "MAXINE (V.O.)",
        "Mind the press. It stamps on time.", "", "CUT TO:"};
      const float lh = (y1 - y0 - 80.0f) / 20.0f;
      for (int k = 0; k < 20; ++k)
      {
        const char* l = kLines[k];
        if (!l[0])
          continue;
        const bool centred = k == 6 || k == 7 || k == 8 || k == 10 || k == 11 || k == 16 || k == 17;
        const bool right = k == 19 || k == 0;
        const float tx = centred ? (x0 + x1) * 0.5f : right ? x1 - 60.0f : x0 + 120.0f;
        const float ty = y0 + 40.0f + float(k) * lh;
        if (ty < -40.0f || ty > float(kScreenH) + 10.0f || tx < -1000.0f || tx > float(kScreenW) + 1000.0f)
          continue;
        r.drawText(l, tx, ty, {k == 1 ? 30.0f : 26.0f, rgb(52, 50, 58), 0}, centred ? Align::Center : right ? Align::Right : Align::Left);
      }
    }
    // The bound final draft, standing on the floor: its cover takes the
    // level's PAGE 42.
    const float bx0 = bx(124.6f), btop = by(12.4f), bw = 8.0f * kTilePx, bh = by(22.0f) - btop;
    if (!offScreen(bx0, btop, bw, bh))
    {
      r.fillRect(bx0 + 14.0f, btop + 10.0f, bw, bh, rgba(0, 0, 0, 90));
      for (int k = 0; k < 6; ++k)
        r.fillRect(bx0 + bw - 12.0f + float(k) * 2.0f, btop + 10.0f + float(k) * 2.0f, 12.0f, bh - 14.0f, k % 2 ? kPaper : kPaperLo);
      r.fillRect(bx0, btop, bw, bh, rgb(24, 26, 40));
      r.fillRect(bx0, btop, 36.0f, bh, rgb(14, 14, 24));
      for (int k = 0; k < 3; ++k)
        r.fillRect(bx0 + 10.0f, btop + 80.0f + float(k) * (bh - 180.0f) / 2.0f, 18.0f, 18.0f, rgb(200, 170, 80));
      r.fillRect(bx0 + 60.0f, btop + 30.0f, bw - 100.0f, 4.0f, rgb(210, 170, 70));
      r.fillRect(bx0 + 60.0f, btop + 150.0f, bw - 100.0f, 4.0f, rgb(210, 170, 70));
      r.drawText("FINAL DRAFT", bx0 + bw * 0.5f + 16.0f, btop + 220.0f, {48.0f, rgb(250, 246, 236), 0}, Align::Center);
      r.drawText("GUNRUNNERS", bx0 + bw * 0.5f + 16.0f, btop + 300.0f, {34.0f, rgb(240, 200, 80), 0, true}, Align::Center);
      r.drawText("\"THE LAST EPISODE\"", bx0 + bw * 0.5f + 16.0f, btop + 350.0f, {20.0f, rgb(200, 200, 216), 0}, Align::Center);
      r.drawText("DO NOT CHANGE", bx0 + bw * 0.5f + 16.0f, btop + 430.0f, {22.0f, kRedInk, 0}, Align::Center);
    }
    // The press's housing in the ceiling.
    for (const auto& c : s.crushers)
    {
      const float hx = bx(float(c.x0)) + float(c.w) * kTilePx * 0.5f;
      const Texture& t = baked(art, r, "housing", 300, 120, 150.0f, 0.0f, paintPressHousing);
      if (!offScreen(hx - 150.0f, by(1.8f), 300.0f, 120.0f))
        r.draw(t, hx, by(1.8f));
    }
    // The warning sign by the press (its word CRUSHED goes on it).
    for (const auto& wd : s.words)
      if (wd.target == WordTarget::Crusher)
      {
        const float sx = (float(wd.x) + float(wd.w) * 0.5f) * kCellPx - camX, sy = float(wd.y) * kCellPx - camY - 26.0f;
        if (!offScreen(sx - 170.0f, sy, 340.0f, 140.0f))
          r.draw(baked(art, r, "warnsign", 340, 140, 170.0f, 0.0f, paintWarningSign), sx, sy);
      }
    // The line over the exit.
    const float ex = bx(141.6f), ey = by(13.6f);
    if (!offScreen(ex, ey, 520.0f, 70.0f))
    {
      r.fillRect(ex + 8.0f, ey + 8.0f, 500.0f, 60.0f, rgba(0, 0, 0, 80));
      r.fillRect(ex, ey, 500.0f, 60.0f, kPaper);
      r.drawText("THE RUNNERS EXIT.", ex + 250.0f, ey + 14.0f, {32.0f, rgb(30, 28, 36), 0}, Align::Center);
    }
  }
  (void)map;
}

// --- Behind the runner -------------------------------------------------------------------------------

void World::drawWritersBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& s = mWriters;
  if (s.breakout)
    return;
  auto bx = [&](float tx) { return tx * kTilePx - camX; };
  auto by = [&](float ty) { return ty * kTilePx - camY; };
  // The carpet runs on under the desks.
  for (const auto& d : kDesks)
    for (int tx = d.x0; tx <= d.x1; ++tx)
    {
      const float x = bx(float(tx)), y = by(float(kDeskBottom + 1));
      if (!offScreen(x, y - 40.0f, kTilePx, 100.0f))
        r.draw(mArt.solidTop, x, y);
    }

  // What the words name: drawn in full, fading as they are struck, sketched
  // back in pencil, or a dashed ghost while they wait for the runner.
  for (std::size_t wi = 0; wi < s.words.size(); ++wi)
  {
    const auto& w = s.words[wi];
    if (w.target != WordTarget::Rect)
      continue;
    float a = 1.0f;
    if (w.state == WordState::Striking && w.t >= kWrStrikeLine)
      a = std::max(0.0f, 1.0f - (float(w.t - kWrStrikeLine) + alpha) / float(kWrStrikeFade));
    else if (w.gone())
      a = 0.0f;
    const float x = bx(float(w.x0)), y = by(float(w.y0));
    const float rw = float(w.x1 - w.x0 + 1) * kTilePx, rh = float(w.y1 - w.y0 + 1) * kTilePx;
    if (offScreen(x, y, rw, rh))
      continue;
    const Tile first = w.tiles.empty() ? Tile::Empty : w.tiles.front();
    if (a > 0.0f)
    {
      DrawOpts o;
      o.alpha = a;
      if (w.id == "w7")
      {
        // The filing cabinet's front: drawn in front, over its secret.
      }
      else if (first == Tile::Solid && w.text == "LANCE WINS")
        r.draw(baked(mArt, r, "lancedoor", 136, 392, 4.0f, 4.0f, paintLanceDoor), x, y, o);
      else if (first == Tile::Solid)
      {
        const int rows = w.y1 - w.y0 + 1;
        r.draw(baked(mArt, r, "inkwall/" + std::to_string(rows), 136, rows * 64, 4.0f, 0.0f,
                 [rows](cairo_t* cr) { paintInkWall(cr, rows, rows); }),
          x, y, o);
      }
      else if (first == Tile::Platform && w.load)
        r.draw(baked(mArt, r, "plank", 768, 40, 0.0f, 0.0f, paintPlank), x, y, o);
      else if (w.state == WordState::Striking)
      {
        // A ladder or the pencils fading out: the tiles, see-through.
        for (int ty = w.y0; ty <= w.y1; ++ty)
          for (int tx = w.x0; tx <= w.x1; ++tx)
          {
            const Tile t = mMap.block(tx, ty);
            if (t == Tile::Ladder)
              r.draw(mArt.ladder, bx(float(tx)), by(float(ty)), o);
            else if (t == Tile::Spikes)
              r.draw(mArt.spikes, bx(float(tx)), by(float(ty)), o);
            else if (t == Tile::Platform)
              r.draw(mArt.platform, bx(float(tx)), by(float(ty)), o);
          }
      }
      if (w.state == WordState::Striking && w.t >= kWrStrikeLine && w.id != "w7")
        drawCrumbs(r, x, y, rw, std::min(rh, 4.0f * kTilePx), w.t - kWrStrikeLine, int(wi));
      continue;
    }
    // Gone: the pencil puts a load-bearing one back...
    if (w.state == WordState::Sketching)
    {
      const float p = std::min(1.0f, (float(w.t) + alpha) / float(kWrResketch - 4));
      const Color pc = withAlpha(kGraphite, 230);
      if (first == Tile::Platform)
      {
        // The plank: its outline drawn left to right, hatched in.
        const float len = rw * p;
        r.drawLine(x, y + 1.0f, x + len, y + 1.0f, 3.0f, pc);
        r.drawLine(x, y + 23.0f, x + len, y + 23.0f, 3.0f, pc);
        for (float k = 0; k < len; k += 14.0f)
          r.drawLine(x + k, y + 22.0f, x + k + 12.0f, y + 3.0f, 1.5f, withAlpha(kGraphite, 160));
        if (p > 0.8f)
        {
          DrawOpts o;
          o.alpha = (p - 0.8f) * 5.0f;
          r.draw(baked(mArt, r, "plank", 768, 40, 0.0f, 0.0f, paintPlank), x, y, o);
        }
        r.drawLine(x + len, y - 30.0f, x + len - 10.0f, y + 10.0f, 7.0f, kPencil);
      }
      else
      {
        // A ladder: the rails drawn up from the bottom, the rungs as they
        // are reached.
        const float top = y + rh * (1.0f - p);
        for (int tx = w.x0; tx <= w.x1; ++tx)
        {
          const float lx = bx(float(tx));
          r.drawLine(lx - 4.0f, y + rh, lx - 4.0f, top, 3.0f, pc);
          r.drawLine(lx + 36.0f, y + rh, lx + 36.0f, top, 3.0f, pc);
          for (float ry = y + rh - 18.0f; ry > top; ry -= 32.0f)
            r.drawLine(lx - 4.0f, ry, lx + 36.0f, ry + 2.0f, 2.5f, pc);
          if (p > 0.8f)
          {
            DrawOpts o;
            o.alpha = (p - 0.8f) * 5.0f;
            for (int ty = w.y0; ty <= w.y1; ++ty)
              r.draw(mArt.ladder, lx, by(float(ty)), o);
          }
          r.drawLine(lx + 16.0f, top - 40.0f, lx + 10.0f, top, 7.0f, kPencil);
        }
      }
    }
    // ...a Typo's work comes back once the letters are all written...
    else if (w.state == WordState::Rewriting && w.t >= kWrRewrite - kWrRewriteShow)
      drawGhostBox(r, x, y, rw, rh, 0.4f + 0.4f * float((frame / 6) % 2));
    // ...and waits for the runner to get out of the way.
    else if (w.state == WordState::Waiting)
      drawGhostBox(r, x, y, rw, rh, 0.5f + 0.5f * std::sin(float(frame) * 0.3f));
  }

  // The keys: the next one the sentence wants lit and glowing.
  const bool done = s.bridge.typed >= int(s.bridge.text.size());
  const char want = done ? 0 : s.bridge.text[std::size_t(std::max(0, s.bridge.typed))];
  for (const auto& k : s.keys)
  {
    const float x = bx(float(k.x)), y = by(float(k.y)) + (k.press > 0 ? 9.0f : 0.0f);
    if (offScreen(x, y, 2.0f * kTilePx, kTilePx))
      continue;
    const bool lit = !done && k.ch == want;
    if (lit)
      drawGlow(r, mArt, x + kTilePx, y + 24.0f, 120.0f + 12.0f * std::sin(float(frame) * 0.2f), rgb(255, 210, 90), 0.55f);
    const std::string key = std::string("key/") + (k.ch == ' ' ? '_' : k.ch) + (lit ? "l" : "");
    const char ch = k.ch;
    r.draw(baked(mArt, r, key, 136, 76, 4.0f, 0.0f, [ch, lit](cairo_t* cr) { paintKey(cr, ch, lit); }), x, y);
    if (lit)
    {
      // A bouncing arrow over it.
      const float ay = y - 54.0f - std::abs(std::sin(float(frame) * 0.15f)) * 14.0f;
      r.drawLine(x + kTilePx, ay, x + kTilePx, ay + 30.0f, 6.0f, rgb(255, 210, 90));
      r.drawLine(x + kTilePx - 14.0f, ay + 16.0f, x + kTilePx, ay + 32.0f, 6.0f, rgb(255, 210, 90));
      r.drawLine(x + kTilePx + 14.0f, ay + 16.0f, x + kTilePx, ay + 32.0f, 6.0f, rgb(255, 210, 90));
    }
  }
  // The bridge of typed paper, a block per letter.
  for (int i = 0; i <= s.bridge.x1 - s.bridge.x0; ++i)
  {
    const int tx = s.bridge.x0 + i;
    if (mMap.block(tx, s.bridge.row) != Tile::Solid)
      continue;
    const float x = bx(float(tx)), y = by(float(s.bridge.row));
    if (offScreen(x, y, kTilePx, kTilePx))
      continue;
    const char ch = i < int(s.bridge.text.size()) ? s.bridge.text[std::size_t(i)] : ' ';
    const std::string key = std::string("tape/") + (ch == ' ' ? '_' : ch) + std::to_string(i % 3);
    const Texture& t = baked(mArt, r, key, 64, 64, 0.0f, 0.0f, [ch, i](cairo_t* cr) { paintTape(cr, ch, i); });
    // The newest block drops into place.
    const bool newest = i == s.bridge.typed - 1 && i < int(s.bridge.text.size());
    DrawOpts o;
    o.alpha = 1.0f;
    r.draw(t, x, y - (newest && s.keys.front().press > 0 ? 6.0f : 0.0f), o);
  }

  // The red strings: twisted yarn between the pins, the pins on top.
  for (std::size_t i = 0; i + 3 < s.pins.size(); i += 2)
  {
    const float x0 = float(s.pins[i] + 1) * kCellPx - camX, y0 = float(s.pins[i + 1]) * kCellPx - camY;
    const float x1 = float(s.pins[i + 2] + 1) * kCellPx - camX, y1 = float(s.pins[i + 3]) * kCellPx - camY;
    if (std::max(x0, x1) < -40.0f || std::min(x0, x1) > float(kScreenW) + 40.0f)
      continue;
    r.drawLine(x0, y0 + 3.0f, x1, y1 + 3.0f, 10.0f, rgb(110, 10, 18));
    r.drawLine(x0, y0, x1, y1, 7.0f, rgb(210, 30, 40));
    // The twist: little light dashes along it.
    const float len = std::hypot(x1 - x0, y1 - y0);
    const float ux = (x1 - x0) / std::max(1.0f, len), uy = (y1 - y0) / std::max(1.0f, len);
    for (float d = 6.0f; d < len - 6.0f; d += 14.0f)
      r.drawLine(x0 + ux * d, y0 + uy * d - 2.0f, x0 + ux * (d + 6.0f), y0 + uy * (d + 6.0f) + 1.0f, 2.0f, rgb(255, 110, 110));
  }
  static constexpr Color kPinTone[4] = {rgb(250, 200, 40), rgb(60, 140, 230), rgb(230, 40, 50), rgb(80, 200, 110)};
  for (std::size_t i = 0; i + 1 < s.pins.size(); i += 2)
  {
    const float x = float(s.pins[i] + 1) * kCellPx - camX, y = float(s.pins[i + 1]) * kCellPx - camY;
    if (offScreen(x - 20.0f, y - 20.0f, 40.0f, 40.0f))
      continue;
    const int k = int(i / 2) % 4;
    r.draw(baked(mArt, r, "pin/b" + std::to_string(k), 30, 30, 15.0f, 15.0f, [k](cairo_t* cr) { paintPin(cr, kPinTone[k]); }), x, y);
  }

  // Inkwells: the ink bubbles as a Typo gets ready to crawl out.
  for (const auto& k : s.inkwells)
  {
    const float x = float(k.x) * kCellPx - camX, y = float(k.y + 1) * kCellPx - camY;
    if (offScreen(x - 20.0f, y - 120.0f, 160.0f, 130.0f))
      continue;
    r.draw(baked(mArt, r, "inkwell", 136, 112, 0.0f, 112.0f, paintInkwell), x, y);
    const int soon = kWrTypoEvery - k.timer;
    const int bubbles = soon < 45 ? 3 : 1;
    for (int b = 0; b < bubbles; ++b)
    {
      const float ph = std::fmod(float(frame) * 0.04f + float(b) * 0.37f, 1.0f);
      const float bxp = x + 56.0f + float(b) * 12.0f, byp = y - 86.0f - ph * 30.0f;
      r.fillRect(bxp - 4.0f, byp - 4.0f, 8.0f + float(b) * 2.0f, 8.0f + float(b) * 2.0f, rgba(20, 20, 50, int(255.0f * (1.0f - ph))));
    }
  }

  // The original pitch on the top shelf.
  if (s.pitchX >= 0 && !s.pitchTaken)
  {
    const float x = (float(s.pitchX) + 2.0f) * kCellPx - camX, y = float(s.pitchY + 1) * kCellPx - camY;
    if (!offScreen(x - 80.0f, y - 120.0f, 160.0f, 130.0f))
    {
      drawGlow(r, mArt, x, y - 50.0f, 110.0f + 10.0f * std::sin(float(frame) * 0.1f), rgb(255, 220, 120), 0.45f);
      r.draw(baked(mArt, r, "pitchscroll", 150, 110, 75.0f, 104.0f, paintPitchScroll), x, y - std::sin(float(frame) * 0.08f) * 4.0f);
      if ((frame / 8) % 6 == 0)
        drawGlow(r, mArt, x + 40.0f, y - 70.0f, 26.0f, rgb(255, 255, 255), 0.9f);
    }
  }
}

// --- In front --------------------------------------------------------------------------------------

void World::drawWritersFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& s = mWriters;
  const auto& p = mPlayer;
  if (s.breakout)
  {
    // The runner holds a ruler up for the paddle.
    const CellBox pb = p.box();
    const float rx = (lerpCells(p.prevX, p.x, alpha) + float(pb.w) * kCellPx * 0.5f) - camX, ry = float(pb.y) * kCellPx - camY - 4.0f;
    r.draw(baked(mArt, r, "ruler", 200, 26, 100.0f, 13.0f, paintRuler), rx, ry);
    if (s.ball)
    {
      DrawOpts o;
      o.angle = float(frame * 14 % 360) * (s.bvx < 0 ? -1.0f : 1.0f);
      const float x = (s.bx + s.bvx * (alpha - 1.0f)) * kCellPx - camX, y = (s.by + s.bvy * (alpha - 1.0f)) * kCellPx - camY;
      for (int k = 1; k <= 3; ++k)
        r.fillRect(x - s.bvx * float(k) * 10.0f - 4.0f, y - s.bvy * float(k) * 10.0f - 4.0f, 8.0f, 8.0f, rgba(255, 255, 240, 120 - k * 30));
      r.draw(baked(mArt, r, "paperball", 40, 40, 20.0f, 20.0f, paintPaperBall), x, y, o);
    }
    else if (s.balls > 0)
    {
      // The next ball waiting on the ruler.
      r.draw(baked(mArt, r, "paperball", 40, 40, 20.0f, 20.0f, paintPaperBall), rx, ry - 22.0f);
    }
    // The bricks, here because the ball flies in front of them.
    for (const auto& b : s.bricks)
    {
      if (!b.alive)
        continue;
      const float x = float(b.x) * kCellPx - camX, y = float(b.y) * kCellPx - camY;
      const bool cracked = b.red && b.hp < 2;
      const std::string key = std::string("brick/") + b.ch + (b.red ? (cracked ? "c" : "r") : "n") + (b.gem ? "g" : "");
      const char ch = b.ch;
      const bool red = b.red, gem = b.gem;
      r.draw(baked(mArt, r, key, 136, 72, 4.0f, 4.0f, [ch, red, cracked, gem](cairo_t* cr) { paintBrick(cr, ch, red, cracked, gem); }), x, y);
    }
    return;
  }
  auto bx = [&](float tx) { return tx * kTilePx - camX; };
  auto by = [&](float ty) { return ty * kTilePx - camY; };

  // How far a word on an enemy has faded it (the BLOCK, the GUARD).
  auto enemyFade = [&](int enemy) {
    for (const auto& w : s.words)
      if (w.target == WordTarget::Enemy && w.enemy == enemy && w.state == WordState::Striking && w.t >= kWrStrikeLine)
        return std::max(0.0f, 1.0f - (float(w.t - kWrStrikeLine) + alpha) / float(kWrStrikeFade));
    return 1.0f;
  };

  // Enemies.
  for (std::size_t i = 0; i < mEnemies.size(); ++i)
  {
    const Enemy& e = mEnemies[i];
    if (!e.alive || e.hidden)
      continue;
    if (e.kind != EnemyKind::Typo && e.kind != EnemyKind::Deadline && e.kind != EnemyKind::WritersBlock && e.kind != EnemyKind::SketchGuard)
      continue;
    const float cx = (e.drawX + float(e.w) * 0.5f) * kCellPx - camX, foot = (e.drawY + 1.0f) * kCellPx - camY;
    const float h = float(e.h) * kCellPx;
    if (offScreen(cx - 140.0f, foot - h - 120.0f, 280.0f, h + 160.0f))
      continue;
    const int dir = e.dir < 0 ? -1 : 1;
    const Sprite* sp = nullptr;
    float ox = 0.0f, oy = 0.0f;
    DrawOpts so;
    so.alpha = enemyFade(int(i));
    if (e.kind == EnemyKind::Typo)
    {
      const Typo* t = nullptr;
      for (const auto& k : s.typos)
        if (k.enemy == int(i))
          t = &k;
      const bool tell = t && t->tell > 0;
      bool busy = false;
      for (const auto& w : s.words)
        busy = busy || (w.state == WordState::Rewriting && w.typo == int(i));
      const int pose = tell ? 2 : int(e.drawX * 0.5f) % 2;
      sp = &styledEnemySprite(mArt, r, mTheme, "typo", pose + (e.carrier ? 3 : 0), 0, e.w, e.h);
      if (tell)
        oy = -10.0f - std::abs(std::sin(float(frame) * 0.9f)) * 8.0f;
      if (busy)
        ox = float((frame / 2) % 3 - 1) * 2.0f;
      r.draw(sp->get(dir), cx + ox, foot + oy, so);
      // Its letter: a typo of one, jumbling while it springs.
      static constexpr char kLetters[7] = {'Q', 'Z', 'X', 'J', 'K', 'W', 'Y'};
      const int li = tell ? (frame / 2) % 7 : int(i) % 7;
      const char c[2] = {kLetters[li], 0};
      r.drawText(c, cx + ox - float(dir) * 6.0f, foot + oy - 50.0f, {34.0f, rgb(250, 248, 236), kInk, tell}, Align::Center, so.alpha);
      if (tell && (frame / 3) % 2 == 0)
        r.drawText("squeak!", cx, foot - 96.0f, {15.0f, rgb(220, 255, 200), kHudInk, true}, Align::Center);
    }
    else if (e.kind == EnemyKind::Deadline)
    {
      const Deadline* d = nullptr;
      for (const auto& k : s.deadlines)
        if (k.enemy == int(i))
          d = &k;
      const int step = d ? std::clamp(d->step, 0, 4) : 0;
      const bool ring = e.tell > 0;
      const int stride = int(e.drawX * 0.5f) % 2;
      sp = &styledEnemySprite(mArt, r, mTheme, "deadline", step + (ring ? 5 : 0), stride, e.w, e.h);
      if (ring)
        ox = float((frame / 2) % 2 ? 3 : -3);
      // Speed lines behind a fast one.
      if (step >= 2)
        for (int q = 0; q < step; ++q)
          r.drawLine(cx - float(dir) * (70.0f + float(q) * 6.0f), foot - 40.0f - float(q) * 26.0f, cx - float(dir) * (110.0f + float(q) * 14.0f),
            foot - 40.0f - float(q) * 26.0f, 3.0f, rgba(255, 240, 220, 150));
      r.draw(sp->get(dir), cx + ox, foot, so);
      // The hands: the minute hand laps (spinning wild once it is flat out),
      // the hour hand creeps on a step a lap.
      const float hx = cx + ox, hy = foot - 90.0f;
      const float lap = d && d->step < 4 ? (float(d->lap) + alpha) / float(kWrDeadlineLap) : float(frame) * 0.09f;
      const float am = lap * 6.2832f, ah = 3.1416f * 0.5f + float(step) * 0.9f;
      r.drawLine(hx, hy, hx + std::sin(am) * 34.0f, hy - std::cos(am) * 34.0f, 4.0f, kInk);
      r.drawLine(hx, hy, hx + std::sin(ah) * 22.0f, hy - std::cos(ah) * 22.0f, 6.0f, kRedInk);
      r.fillRect(hx - 4.0f, hy - 4.0f, 8.0f, 8.0f, rgb(220, 178, 80));
      if (ring && (frame / 3) % 2 == 0)
        r.drawText("RRRING!", cx, foot - h - 50.0f, {20.0f, rgb(255, 230, 120), kHudInk, true}, Align::Center);
    }
    else if (e.kind == EnemyKind::WritersBlock)
    {
      const WritersBlock* b = nullptr;
      for (const auto& k : s.blocks)
        if (k.enemy == int(i))
          b = &k;
      const bool sliding = b && b->slide != 0;
      const int v = e.tell > 0 ? 1 : sliding ? 2 : 0;
      sp = &styledEnemySprite(mArt, r, mTheme, "writersblock", v, 0, e.w, e.h);
      if (v == 1)
        ox = float((frame / 2) % 2 ? 2 : -2);
      r.draw(sp->get(dir), cx + ox, foot, so);
      // Scraping: grit and dust at its foot; sliding: a dust trail.
      if (v >= 1)
        for (int q = 0; q < 6; ++q)
        {
          const std::uint32_t hh = hash2(frame / 2, q + int(i) * 7);
          const float dx = (v == 2 ? -float(dir) : (unit(hh) < 0.5f ? -1.0f : 1.0f)) * (64.0f + unit(hh) * 40.0f);
          const float dy = -unit(hh >> 8) * 40.0f;
          drawGlow(r, mArt, cx + dx, foot - 6.0f + dy, 18.0f + unit(hh >> 4) * 16.0f, rgb(200, 190, 170), 0.45f * so.alpha);
        }
      if (v == 1 && (frame / 4) % 2 == 0)
        r.drawText("scrrrape", cx, foot - h - 24.0f, {15.0f, rgb(230, 224, 210), kHudInk, true}, Align::Center);
    }
    else
    {
      // The sketch guard: the rifle comes up for the tell, then fires.
      const EnemyDef& def = enemyDef(e.def);
      const int v = e.tell > 0 ? 1 : (e.cool > def.cooldown - 4 ? 2 : 0);
      sp = &styledEnemySprite(mArt, r, mTheme, "sketch_guard", v, 0, e.w, e.h);
      r.draw(sp->get(dir), cx, foot, so);
      if (v == 1 && (frame / 4) % 2 == 0)
        r.drawText("!", cx + float(dir) * 30.0f, foot - h - 30.0f, {30.0f, rgb(255, 230, 120), kHudInk}, Align::Center);
    }
    if (e.flash > 0 && sp)
    {
      DrawOpts fo;
      fo.blend = Blend::Add;
      fo.alpha = float(e.flash) / 8.0f;
      r.draw(sp->get(dir), cx + ox, foot + oy, fo);
    }
  }

  // The filing cabinet's front over its secret, fading when SECRET is struck.
  for (const auto& w : s.words)
  {
    if (w.id != "w7")
      continue;
    float a = 1.0f;
    if (w.state == WordState::Striking && w.t >= kWrStrikeLine)
      a = std::max(0.0f, 1.0f - (float(w.t - kWrStrikeLine) + alpha) / float(kWrStrikeFade));
    else if (w.gone())
      a = 0.0f;
    if (a <= 0.0f)
      continue;
    const float x = bx(2.0f), y = by(35.0f);
    if (offScreen(x, y, 320.0f, 320.0f))
      continue;
    DrawOpts o;
    o.alpha = a;
    r.draw(baked(mArt, r, "cabinet_face", 320, 320, 0.0f, 0.0f, paintCabinetFace), x, y, o);
    if (w.state == WordState::Striking)
      drawCrumbs(r, x, y + 64.0f, 320.0f, 256.0f, std::max(0, w.t - kWrStrikeLine), 77);
  }

  // The rubber-stamp press: its piston down from the housing, the stamp; it
  // lifts and creaks before each slam, and leaves REJECTED on the floor.
  for (const auto& c : s.crushers)
  {
    const float x = bx(float(c.x0)), w = float(c.w) * kTilePx;
    const float face = float(c.face + 1) * kCellPx - camY;
    if (offScreen(x, by(1.0f), w, face - by(1.0f) + 40.0f))
      continue;
    const int up = (c.rest + 1) * kCellsPerTile - 1, down = (c.to + 1) * kCellsPerTile - 1;
    const int tellAt = kWrCrusherPeriod - 30 - kWrCrusherTell;
    const bool tell = !c.gone && c.t >= tellAt && c.t < tellAt + kWrCrusherTell;
    const float shake = tell ? float((frame / 2) % 2 ? 2 : -2) : 0.0f;
    // The imprint, fading after the slam.
    if (!c.gone && c.t >= tellAt + kWrCrusherTell + 3)
    {
      const float a = std::max(0.0f, 1.0f - float(c.t - (tellAt + kWrCrusherTell + 3)) / 26.0f);
      const float fy = float(down + 1) * kCellPx - camY;
      r.fillRect(x + 20.0f, fy + 4.0f, w - 40.0f, 4.0f, withAlpha(kRedInk, int(200.0f * a)));
      r.drawText("REJECTED", x + w * 0.5f, fy + 10.0f, {30.0f, withAlpha(kRedInk, int(230.0f * a)), 0}, Align::Center);
    }
    const float headTop = face - 128.0f;
    const float pistonTop = by(1.8f) + 100.0f;
    if (headTop > pistonTop)
    {
      r.fillRect(x + w * 0.5f - 26.0f + shake, pistonTop, 52.0f, headTop - pistonTop + 4.0f, rgb(70, 72, 84));
      r.fillRect(x + w * 0.5f - 18.0f + shake, pistonTop, 14.0f, headTop - pistonTop + 4.0f, rgb(210, 214, 226));
    }
    DrawOpts o;
    if (c.gone)
      o.tint = rgb(170, 170, 170);
    r.draw(baked(mArt, r, "stamp", 256, 128, 0.0f, 0.0f, paintStamp), x + shake, headTop, o);
    if (c.gone)
    {
      // Struck: it hangs out of order, a note taped on.
      r.fillRect(x + w * 0.5f - 90.0f, headTop + 30.0f, 180.0f, 50.0f, kNote);
      r.drawText("OUT OF ORDER", x + w * 0.5f, headTop + 44.0f, {20.0f, kRedInk, 0}, Align::Center);
    }
    else if (tell)
    {
      // Creak marks at its collar, grit trickling.
      for (int k = 0; k < 2; ++k)
      {
        const float sx = x + (k ? w - 30.0f : 30.0f), sy = headTop + 10.0f;
        r.drawLine(sx, sy, sx + (k ? 22.0f : -22.0f), sy - 14.0f, 3.0f, rgb(255, 240, 200));
        r.drawLine(sx, sy + 12.0f, sx + (k ? 26.0f : -26.0f), sy + 6.0f, 3.0f, rgb(255, 240, 200));
      }
      if ((frame / 4) % 2 == 0)
        r.drawText("creak", x + w * 0.5f, headTop - 34.0f, {16.0f, rgb(255, 230, 190), kHudInk, true}, Align::Center);
    }
    if (!c.gone && c.face == down && c.face > up)
      r.fillRect(x - 20.0f, face - 6.0f, w + 40.0f, 6.0f, rgba(255, 255, 255, 120));
  }

  // The words, over everything they name.
  for (std::size_t wi = 0; wi < s.words.size(); ++wi)
  {
    const auto& w = s.words[wi];
    float wx = float(w.x) * kCellPx - camX, wy = float(w.y) * kCellPx - camY;
    const float ww = float(w.w) * kCellPx;
    float a = 1.0f;
    if (w.target == WordTarget::Enemy && w.enemy >= 0)
    {
      const Enemy& e = mEnemies[std::size_t(w.enemy)];
      if (!e.alive && w.state != WordState::Striking)
        continue;
      // Riding on its chest, smoothly.
      wx = (e.drawX + float(e.w) * 0.5f) * kCellPx - ww * 0.5f - camX;
      wy = (e.drawY - 3.0f) * kCellPx - camY;
      a = enemyFade(w.enemy);
    }
    if (w.target == WordTarget::Egg && s.egg.state != 0)
      continue; // the note is an egg now
    if (offScreen(wx - 40.0f, wy - 40.0f, ww + 80.0f, 140.0f))
      continue;
    const int n = int(w.text.size());
    const int style = w.load ? 1 : 0;
    if (w.target == WordTarget::Egg)
    {
      // The EASTER EGG note: a sticky note pinned to the corkboard.
      r.fillRect(wx - 26.0f, wy - 30.0f, ww + 52.0f, 124.0f, rgba(0, 0, 0, 60));
      r.fillRect(wx - 32.0f, wy - 36.0f, ww + 52.0f, 124.0f, kNote);
      r.fillRect(wx - 32.0f, wy - 36.0f, ww + 52.0f, 14.0f, rgb(255, 246, 170));
      r.drawText("(do not open)", wx + ww * 0.5f - 6.0f, wy + 62.0f, {14.0f, rgb(80, 70, 40), 0, true}, Align::Center);
      DrawOpts po;
      po.scale = 0.8f;
      r.draw(baked(mArt, r, "pin/0", 30, 30, 15.0f, 15.0f, [](cairo_t* cr) { paintPin(cr, rgb(230, 40, 50)); }), wx + ww * 0.5f - 6.0f,
        wy - 26.0f, po);
    }
    const float lx0 = wx - 10.0f, ly0 = wy + 38.0f, lx1 = wx + ww + 10.0f, ly1 = wy + 30.0f;
    switch (w.state)
    {
      case WordState::Intact:
        drawMarkerWord(r, mArt, w.text, wx, wy, ww, n, a, style, int(wi));
        break;
      case WordState::Waiting:
        drawMarkerWord(r, mArt, w.text, wx, wy, ww, n, a, style, int(wi));
        drawGhostBox(r, wx - 6.0f, wy, ww + 12.0f, 64.0f, 0.5f + 0.5f * std::sin(float(frame) * 0.3f));
        break;
      case WordState::Striking:
      {
        drawMarkerWord(r, mArt, w.text, wx, wy, ww, n, a, style, int(wi));
        const float pl = std::min(1.0f, (float(w.t) + alpha) / float(kWrStrikeLine));
        drawStrike(r, lx0, ly0, lx1, ly1, pl, int(wi), a);
        if (pl < 1.0f)
        {
          // The pen's nib at the head of the line.
          const float nx = lx0 + (lx1 - lx0) * pl, ny = ly0 + (ly1 - ly0) * pl;
          r.drawLine(nx, ny, nx + 26.0f, ny - 40.0f, 10.0f, kRedInk);
          r.drawLine(nx + 2.0f, ny - 4.0f, nx + 8.0f, ny - 12.0f, 4.0f, rgb(250, 240, 230));
        }
        break;
      }
      case WordState::Struck:
        drawMarkerWord(r, mArt, w.text, wx, wy, ww, n, 0.5f, style, int(wi));
        drawStrike(r, lx0, ly0, lx1, ly1, 1.0f, int(wi), 1.0f);
        break;
      case WordState::Rewriting:
      {
        // Letter by letter, all back kWrRewriteShow frames before the target;
        // the red line fades as the letters come back.
        const int span = std::max(1, kWrRewrite - kWrRewriteShow);
        const int shown = std::min(n, n * w.t / span);
        drawMarkerWord(r, mArt, w.text, wx, wy, ww, n, 0.25f, style, int(wi));
        drawMarkerWord(r, mArt, w.text, wx, wy, ww, shown, 1.0f, style, int(wi));
        drawStrike(r, lx0, ly0, lx1, ly1, 1.0f, int(wi), std::max(0.0f, 1.0f - float(shown) / float(std::max(1, n))));
        if (shown < n)
          drawScribble(r, wx + ww / float(n) * (float(shown) + 0.5f), wy + 32.0f, frame, kInk);
        break;
      }
      case WordState::Sketching:
      {
        const int shown = std::min(n, n * w.t / std::max(1, kWrResketch - 4));
        drawMarkerWord(r, mArt, w.text, wx, wy, ww, shown, 1.0f, 1, int(wi));
        if (shown < n)
          drawScribble(r, wx + ww / float(n) * (float(shown) + 0.5f), wy + 32.0f, frame, kGraphite);
        break;
      }
    }
  }

  // The egg: it falls, sits and wobbles as it cracks, and out comes a chick
  // that follows the runner, cheeping when they jump.
  if (s.egg.state >= 1 && s.egg.state <= 3)
  {
    const float x = (s.egg.x + 1.0f) * kCellPx - camX, y = (s.egg.y + 1.0f) * kCellPx - camY;
    if (s.egg.state < 3)
    {
      const int cracks = s.egg.state == 2 ? std::min(3, s.egg.t * 4 / kWrEggHatch) : 0;
      DrawOpts o;
      o.angle = s.egg.state == 2 ? std::sin(float(s.egg.t) * (0.6f + 0.02f * float(s.egg.t))) * float(4 + cracks * 4) : float(frame * 8 % 360);
      r.draw(baked(mArt, r, "egg/" + std::to_string(cracks), 48, 56, 24.0f, 54.0f, [cracks](cairo_t* cr) { paintEgg(cr, cracks); }), x, y, o);
    }
    else
    {
      const int hop = std::abs(p.x - int(s.egg.x)) > 3 ? (frame / 4) % 2 : 0;
      const int face = p.x + 1 >= int(s.egg.x) ? 1 : 0;
      r.draw(bakedSprite(mArt, r, "chick/" + std::to_string(hop), 52, 52, 26.0f, 50.0f, [hop](cairo_t* cr) { paintChick(cr, hop); }).get(face), x,
        y);
      if (s.egg.cheep > 0)
        r.drawText("cheep!", x, y - 80.0f, {18.0f, rgb(255, 240, 140), kHudInk, true}, Align::Center);
    }
  }
}

// --- The HUD -------------------------------------------------------------------------------------------

void World::drawWritersHud(Renderer& r, int frame) const
{
  const auto& s = mWriters;
  if (s.breakout)
  {
    // Balls left as paper balls, the bricks broken as a typed tally.
    const float x = float(kScreenW) - 12.0f - 400.0f, y = 84.0f; // under the score panel, clear of the timer
    r.fillRect(x, y, 400.0f, 46.0f, rgba(20, 16, 24, 190));
    r.fillRect(x, y, 400.0f, 3.0f, rgb(222, 28, 40));
    for (int k = 0; k < 3; ++k)
    {
      DrawOpts o;
      o.alpha = k < s.balls ? 1.0f : 0.25f;
      o.scale = 0.8f;
      r.draw(baked(mArt, r, "paperball", 40, 40, 20.0f, 20.0f, paintPaperBall), x + 30.0f + float(k) * 36.0f, y + 23.0f, o);
    }
    char buf[48];
    std::snprintf(buf, sizeof buf, "BRICKS %d / %d", s.broken, std::max(1, int(s.bricks.size()) - 4));
    r.drawText(buf, x + 270.0f, y + 9.0f, {24.0f, rgb(250, 246, 236), kHudInk}, Align::Center);
    return;
  }
  if (s.scramble > 0)
  {
    // A Typo's joke: the score panel's label jumbles itself.
    static const char* const kJumbles[5] = {"SCROE", "OCSRE", "SCOER", "CSORE", "SROCE"};
    const float x = 12.0f + float(kHudHealthW + kHudWeaponW + kHudInventoryW + kHudLettersW) + 40.0f + 16.0f;
    r.fillRect(x - 4.0f, 14.0f, 70.0f, 20.0f, rgba(8, 6, 20, 235));
    const char* word = kJumbles[(frame / 6) % 5];
    for (int k = 0; k < 5; ++k)
    {
      const char c[2] = {word[k], 0};
      const float jy = std::sin(float(frame) * 0.5f + float(k) * 1.3f) * 2.5f;
      r.drawText(c, x + float(k) * 12.0f + 5.0f, 16.0f + jy, {15.0f, rgb(150, 255, 120), kHudInk}, Align::Center);
    }
  }
}

// --- Shots and the icon ------------------------------------------------------------------------------

bool drawWritersShot(Renderer& r, const World& w, const Art& art, const Projectile& pr, float cx, float cy, int frame)
{
  const auto& s = w.writers();
  if (!s.on || s.breakout)
    return false;
  const float d = float(pr.dx < 0 ? -1 : 1);
  if (pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::RedPen))
  {
    // A streak of red ink behind a felt nib, drips flying off.
    drawGlow(r, art, cx, cy, 34.0f, rgb(255, 60, 70), 0.35f);
    r.drawLine(cx - d * 60.0f, cy + 2.0f, cx, cy, 9.0f, rgb(140, 10, 20));
    r.drawLine(cx - d * 56.0f, cy + 1.0f, cx, cy, 6.0f, kRedInk);
    r.drawLine(cx - d * 40.0f, cy - 1.5f, cx - d * 4.0f, cy - 1.5f, 2.0f, kRedHi);
    for (int k = 0; k < 3; ++k)
    {
      const float off = float((frame + k * 5) % 15) * 4.0f + 20.0f;
      r.fillRect(cx - d * off - 3.0f, cy + 6.0f + float(k) * 3.0f + off * 0.1f, 6.0f, 6.0f, withAlpha(kRedInk, 220 - k * 50));
    }
    // The nib.
    r.drawLine(cx, cy, cx + d * 14.0f, cy, 8.0f, rgb(250, 240, 230));
    r.drawLine(cx + d * 10.0f, cy, cx + d * 18.0f, cy, 5.0f, kRedInk);
    return true;
  }
  if (pr.kind == ShotKind::Enemy)
  {
    // The sketch guard's shot: a pencil lead, spinning, a graphite smudge
    // after it.
    const float a = float(frame) * 0.7f * d;
    r.drawLine(cx - d * 30.0f, cy, cx, cy, 6.0f, rgba(70, 70, 84, 90));
    r.drawLine(cx - std::cos(a) * 14.0f, cy - std::sin(a) * 14.0f, cx + std::cos(a) * 14.0f, cy + std::sin(a) * 14.0f, 6.0f, kInk);
    r.drawLine(cx - std::cos(a) * 12.0f, cy - std::sin(a) * 12.0f, cx + std::cos(a) * 12.0f, cy + std::sin(a) * 12.0f, 3.0f, rgb(120, 120, 136));
    return true;
  }
  return false;
}

void drawRedPenIcon(Renderer& r, const Art& art, float x, float y, float scale, int frame)
{
  DrawOpts o;
  o.scale = scale;
  r.draw(baked(art, r, "redpen_icon", 64, 64, 0.0f, 0.0f, paintRedPenIcon), x, y + std::sin(float(frame) * 0.1f) * 1.5f * scale, o);
}

} // namespace gr
