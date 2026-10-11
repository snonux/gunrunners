// Level 30, Fiber Trench: drawing. See world_trench.cpp for the logic.
//
// Behind the tiles (drawTrenchSet): the water going dark with depth, the
// sun's shafts slanting down and the motes drifting in them, the abyss's
// deepening black and the whale crossing it every 600 frames, the sunken
// ship's hold (its ribbed back wall and the porthole, the level 18 alien
// waving behind the glass), the cable station's lit walls, and the fiber
// cable glowing along the whole route with light pulses running home along
// it. Behind the runner (drawTrenchBack): the ship's rusted plating and the
// station's painted panels over their tiles, kelp swaying on the shelf and
// the slope, tube worms, anemones and sea pens deeper down, coral in Bubble
// Up, the station's 42-fathom depth gauge, the waterproof tape, anchor
// rings on their posts, the Cable Eels (a chain of cable-black sections and
// a head) and their junction boxes (humming and sparking before a lunge),
// the quiet junction box, the harpoons and their taut line. In front
// (drawTrenchFront): the Box Jellies and the Anglerfish, the water's haze
// (clear in the air pockets, whose surfaces shimmer), the currents' bubble
// streams, the dark hold (a pool of light round the runner, the cable and
// the waving alien lit through it), the lures pulsing and the eyes glinting
// in the dark, real gems sparkling there, the trench's rock fading into the
// abyss, the abyss's return fade, and Bubble Up's rings and bubble.
// drawTrenchHud: the STROKES panel's bubble pips under the score.

#include "game/world.hpp"

#include "assets/art.hpp"
#include "assets/enemy_art.hpp"
#include "assets/enemy_art_trench.hpp"
#include "base/math.hpp"
#include "game/trench_draw.hpp"
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
constexpr float kCellPx = float(kCellSize) * kPixelScale; // 32
constexpr float kTilePx = float(kTileSize) * kPixelScale; // 64
constexpr Color kFiber = rgb(90, 240, 255);
constexpr Color kFiberCore = rgb(210, 255, 255);
constexpr Color kDeep = rgb(1, 6, 16);
constexpr Color kHaze = rgb(4, 34, 64);
constexpr Color kInk = rgb(8, 12, 22);
constexpr Color kHudLabel = rgb(190, 188, 214);
constexpr Color kHudInk = rgb(10, 8, 20);
constexpr Color kAlienGreen = rgb(130, 230, 110);

bool offScreen(float x, float y, float w, float h, float m = 96.0f)
{
  return x > float(kScreenW) + m || x + w < -m || y > float(kScreenH) + m || y + h < -m;
}

Color lighten(Color c, float t) { return lerpColor(c, rgb(255, 255, 255), t); }
Color darken(Color c, float t) { return lerpColor(c, rgb(0, 0, 0), t); }

float lerpCellsF(int a, int b, float t) { return float(a) + (float(b) - float(a)) * t; }

float smooth(float t)
{
  t = std::clamp(t, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

// A texture baked once and kept in the Art's sprite cache under `key`; the
// mirrored copy too, its anchor mirrored with it.
const Sprite& bakedSprite(const Art& art, const Renderer& r, const std::string& key, int w, int h, float ax, float ay,
  const std::function<void(cairo_t*)>& paint)
{
  const std::string id = "~trench/" + key;
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

void lettering(cairo_t* cr, const char* text, double cx, double baseline, double size, Color c, double a = 1.0)
{
  selectGameFont(cr);
  cairo_set_font_size(cr, size);
  cairo_text_extents_t e;
  cairo_text_extents(cr, text, &e);
  cairo_move_to(cr, cx - e.width * 0.5 - e.x_bearing, baseline);
  setRgba(cr, c, a);
  cairo_show_text(cr, text);
}

// --- Painters: the water ------------------------------------------------------------------------

// A shaft of sunlight: a soft beam, brightest down its middle, fading out
// along its length. 240 x 1000, hung from the middle of its top.
void paintShaft(cairo_t* cr)
{
  cairo_push_group(cr);
  cairo_rectangle(cr, 0, 0, 240, 1000);
  cairo_pattern_t* g = cairo_pattern_create_linear(0, 0, 240, 0);
  cairo_pattern_add_color_stop_rgba(g, 0.0, 1, 1, 1, 0.0);
  cairo_pattern_add_color_stop_rgba(g, 0.3, 1, 1, 1, 0.3);
  cairo_pattern_add_color_stop_rgba(g, 0.5, 1, 1, 1, 1.0);
  cairo_pattern_add_color_stop_rgba(g, 0.7, 1, 1, 1, 0.3);
  cairo_pattern_add_color_stop_rgba(g, 1.0, 1, 1, 1, 0.0);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
  cairo_pop_group_to_source(cr);
  cairo_pattern_t* m = cairo_pattern_create_linear(0, 0, 0, 1000);
  cairo_pattern_add_color_stop_rgba(m, 0.0, 0, 0, 0, 1.0);
  cairo_pattern_add_color_stop_rgba(m, 0.5, 0, 0, 0, 0.45);
  cairo_pattern_add_color_stop_rgba(m, 1.0, 0, 0, 0, 0.0);
  cairo_mask(cr, m);
  cairo_pattern_destroy(m);
}

// A bubble: a thin bright skin, a faint fill, a highlight. 24 x 24 centred.
void paintBubble(cairo_t* cr)
{
  circle(cr, 12, 12, 8.5);
  setRgba(cr, rgb(190, 235, 255), 0.18);
  cairo_fill_preserve(cr);
  stroke(cr, rgb(225, 250, 255), 1.6, 0.9);
  cairo_arc(cr, 12, 12, 5.6, kPi * 1.1, kPi * 1.45);
  stroke(cr, rgb(255, 255, 255), 1.8, 0.95);
  circle(cr, 15.5, 15.5, 1.2);
  setRgba(cr, rgb(255, 255, 255), 0.6);
  cairo_fill(cr);
}

// One bead of the fiber cable: the black jacket, soft at the edge, and its
// glowing core. 24 x 24 centred; a chain of them is the cable.
void paintBead(cairo_t* cr)
{
  circle(cr, 12, 12, 8.6);
  setRgba(cr, rgb(12, 28, 40), 1.0);
  cairo_fill(cr);
  glow(cr, 12, 12, 6.5, kFiber, 0.9);
  circle(cr, 12, 12, 2.4);
  setRgba(cr, kFiberCore, 1.0);
  cairo_fill(cr);
}

// The whale, a humpback in silhouette with a faint blue rim, swimming right.
// 700 x 260 centred; `tail` 0/1 the flukes up or down.
void paintWhale(cairo_t* cr, int tail)
{
  const double fl = tail ? -22.0 : 22.0;
  const Color body = rgb(22, 50, 74);
  // The long pectoral fin, behind.
  cairo_move_to(cr, 470, 160);
  cairo_curve_to(cr, 440, 190, 380, 228, 330, 246);
  cairo_curve_to(cr, 360, 222, 420, 182, 440, 150);
  cairo_close_path(cr);
  setRgba(cr, darken(body, 0.25f), 0.85);
  cairo_fill(cr);
  // The flukes.
  cairo_move_to(cr, 92, 128);
  cairo_curve_to(cr, 60, 116 + fl * 0.4, 30, 96 + fl, 6, 92 + fl * 1.3);
  cairo_curve_to(cr, 30, 120 + fl * 0.6, 40, 134 + fl * 0.4, 30, 166 + fl * 1.3);
  cairo_curve_to(cr, 56, 160 + fl, 76, 146 + fl * 0.4, 96, 136);
  cairo_close_path(cr);
  setRgba(cr, body, 0.9);
  cairo_fill(cr);
  // The body.
  cairo_move_to(cr, 90, 130);
  cairo_curve_to(cr, 180, 108, 300, 80, 440, 82);
  cairo_curve_to(cr, 560, 84, 640, 96, 676, 122);
  cairo_curve_to(cr, 688, 134, 680, 150, 650, 160);
  cairo_curve_to(cr, 560, 186, 400, 184, 280, 166);
  cairo_curve_to(cr, 190, 154, 130, 142, 90, 130);
  cairo_close_path(cr);
  lgrad(cr, 0, 82, 0, 186, {{0.0, lighten(body, 0.12f)}, {0.6, body}, {1.0, darken(body, 0.3f)}}, 0.92, true);
  cairo_new_path(cr);
  // The throat's grooves and the eye.
  for (int k = 0; k < 6; ++k)
  {
    cairo_move_to(cr, 640 - k * 10, 150 + k * 3);
    cairo_curve_to(cr, 590 - k * 10, 168 + k * 2, 520 - k * 8, 172 + k, 470 - k * 6, 170);
    stroke(cr, darken(body, 0.35f), 1.6, 0.6);
  }
  circle(cr, 590, 128, 3.4);
  setRgba(cr, rgb(4, 10, 18), 0.9);
  cairo_fill(cr);
  // A cold rim along its back and a few barnacle knobs on its head.
  cairo_move_to(cr, 100, 128);
  cairo_curve_to(cr, 190, 106, 300, 80, 440, 82);
  cairo_curve_to(cr, 560, 84, 640, 96, 676, 122);
  stroke(cr, rgb(120, 200, 235), 3.0, 0.45);
  for (int k = 0; k < 5; ++k)
  {
    circle(cr, 610 + k * 12, 100 + k * 4, 3.0);
    setRgba(cr, lighten(body, 0.3f), 0.7);
    cairo_fill(cr);
  }
}

// --- Painters: life on the rock ---------------------------------------------------------------

// Giant kelp: a wavy stem with ruffled blades on little floats. 120 x 340,
// anchored at its root (60, 340).
void paintKelp(cairo_t* cr, int v)
{
  Rng rng(std::uint32_t(30201 + v * 17));
  const double h = 300 - v * 40, sway = v == 1 ? -14 : 12;
  const Color stem = rgb(96, 112, 44), blade = rgb(132, 150, 58), bladeDark = rgb(70, 96, 40);
  auto at = [&](double u, double& x, double& y) {
    const double s = 1 - u;
    x = s * s * s * 60 + 3 * s * s * u * (60 + sway) + 3 * s * u * u * (60 - sway) + u * u * u * (60 + sway * 0.5);
    y = 340 - h * u;
  };
  // Blades behind the stem first, then the stem, then blades in front.
  for (int pass = 0; pass < 2; ++pass)
  {
    if (pass == 1)
    {
      cairo_move_to(cr, 60, 340);
      cairo_curve_to(cr, 60 + sway, 340 - h * 0.35, 60 - sway, 340 - h * 0.7, 60 + sway * 0.5, 340 - h);
      stroke(cr, darken(stem, 0.3f), 6.5);
      cairo_move_to(cr, 60, 340);
      cairo_curve_to(cr, 60 + sway, 340 - h * 0.35, 60 - sway, 340 - h * 0.7, 60 + sway * 0.5, 340 - h);
      stroke(cr, stem, 4.0);
    }
    const int blades = 7;
    for (int l = 0; l < blades; ++l)
    {
      if ((l % 2) != pass)
        continue;
      double px = 0, py = 0;
      at(0.16 + l * 0.8 / blades, px, py);
      const double side = l % 2 ? 1.0 : -1.0, len = rng.range(40, 58) * (1.0 - l * 0.05), lift = rng.range(10, 22);
      // The blade: a long ruffled leaf.
      cairo_move_to(cr, px, py);
      cairo_curve_to(cr, px + side * len * 0.3, py - lift - 8, px + side * len * 0.8, py - lift - 6, px + side * len, py - lift);
      for (int k = 6; k >= 0; --k)
      {
        const double u = k / 6.0;
        cairo_line_to(cr, px + side * len * u, py - lift * u + 7 * std::sin(u * kPi) + (k % 2 ? 1.5 : -1.0));
      }
      cairo_close_path(cr);
      lgrad(cr, px, py, px + side * len, py - lift, {{0.0, bladeDark}, {1.0, blade}}, 0.95, true);
      stroke(cr, lighten(blade, 0.3f), 1.0, 0.5);
      // Its float.
      circle(cr, px + side * 3, py - 2, 3.4);
      setRgba(cr, rgb(170, 160, 70), 1.0);
      cairo_fill_preserve(cr);
      stroke(cr, darken(stem, 0.4f), 0.8);
    }
  }
  // Light catching the top.
  double tx = 0, ty = 0;
  at(1.0, tx, ty);
  glow(cr, tx, ty + 10, 26, rgb(220, 240, 140), 0.2);
}

// The deep's life, 96 x 112, anchored at the root (48, 110): 0 tube worms
// with red plumes, 1 an anemone, 2 a glowing sea pen, 3 a barrel sponge.
void paintFlora(cairo_t* cr, int v)
{
  if (v == 0)
  {
    for (const auto& tw : {std::array<double, 3>{30, 70, -0.15}, std::array<double, 3>{46, 86, 0.05}, std::array<double, 3>{60, 64, 0.2},
           std::array<double, 3>{38, 50, -0.3}})
    {
      const double x = tw[0], h = tw[1], lean = tw[2];
      const double tx = x + lean * h, ty = 110 - h;
      cairo_move_to(cr, x, 110);
      cairo_line_to(cr, tx, ty);
      stroke(cr, kInk, 8.0);
      cairo_move_to(cr, x, 110);
      cairo_line_to(cr, tx, ty);
      stroke(cr, rgb(232, 230, 214), 5.6);
      for (int k = 0; k < 7; ++k)
      {
        const double a = -kPi * 0.5 + (k - 3) * 0.32;
        cairo_move_to(cr, tx, ty);
        cairo_curve_to(cr, tx + std::cos(a) * 8, ty + std::sin(a) * 8 - 2, tx + std::cos(a) * 14, ty + std::sin(a) * 14,
          tx + std::cos(a) * 16, ty + std::sin(a) * 15 + 4);
        stroke(cr, rgb(230, 40, 60), 2.2);
      }
      glow(cr, tx, ty - 6, 12, rgb(255, 90, 100), 0.35);
    }
  }
  else if (v == 1)
  {
    // The column, then the tentacles in a crown.
    cairo_move_to(cr, 34, 110);
    cairo_curve_to(cr, 32, 90, 36, 78, 34, 70);
    cairo_line_to(cr, 62, 70);
    cairo_curve_to(cr, 60, 78, 64, 90, 62, 110);
    cairo_close_path(cr);
    lgrad(cr, 34, 0, 62, 0, {{0.0, rgb(150, 70, 140)}, {0.5, rgb(200, 110, 180)}, {1.0, rgb(110, 50, 110)}}, 1.0, true);
    stroke(cr, kInk, 1.6);
    for (int k = 0; k < 13; ++k)
    {
      const double a = -kPi + k * kPi / 12, len = 26 + (k % 3) * 5;
      const double bx = 48 + std::cos(a) * 12, by = 70;
      const double ex = 48 + std::cos(a) * (12 + len * 0.9), ey = 66 + std::sin(a) * len - 4;
      cairo_move_to(cr, bx, by);
      cairo_curve_to(cr, bx + std::cos(a) * 8, by - 12, ex - std::cos(a) * 6, ey + 6, ex, ey);
      stroke(cr, kInk, 5.0);
      cairo_move_to(cr, bx, by);
      cairo_curve_to(cr, bx + std::cos(a) * 8, by - 12, ex - std::cos(a) * 6, ey + 6, ex, ey);
      stroke(cr, rgb(240, 150, 220), 3.0);
      circle(cr, ex, ey, 2.4);
      setRgba(cr, rgb(255, 230, 250), 1.0);
      cairo_fill(cr);
    }
  }
  else if (v == 2)
  {
    // A sea pen: a feather of polyps on a stalk, glowing green-blue.
    glow(cr, 48, 46, 40, rgb(90, 255, 200), 0.35);
    cairo_move_to(cr, 48, 110);
    cairo_curve_to(cr, 46, 80, 50, 40, 47, 10);
    stroke(cr, kInk, 5.0);
    cairo_move_to(cr, 48, 110);
    cairo_curve_to(cr, 46, 80, 50, 40, 47, 10);
    stroke(cr, rgb(230, 200, 150), 3.0);
    for (int k = 0; k < 10; ++k)
    {
      const double y = 22 + k * 6.5, w = 16 - std::abs(k - 4.5) * 1.6;
      for (const double s : {-1.0, 1.0})
      {
        cairo_move_to(cr, 48, y);
        cairo_curve_to(cr, 48 + s * w * 0.4, y - 6, 48 + s * w, y - 6, 48 + s * w, y - 2);
        stroke(cr, rgb(160, 255, 220), 2.4, 0.9);
      }
    }
  }
  else
  {
    // A barrel sponge.
    cairo_move_to(cr, 26, 110);
    cairo_curve_to(cr, 20, 80, 26, 50, 30, 40);
    cairo_line_to(cr, 66, 40);
    cairo_curve_to(cr, 70, 50, 76, 80, 70, 110);
    cairo_close_path(cr);
    lgrad(cr, 22, 0, 74, 0, {{0.0, rgb(150, 80, 50)}, {0.45, rgb(210, 130, 80)}, {1.0, rgb(110, 54, 40)}}, 1.0, true);
    stroke(cr, kInk, 2.0);
    for (int k = 1; k < 5; ++k)
    {
      cairo_move_to(cr, 26 + k * 2, 110 - k * 16);
      cairo_curve_to(cr, 40, 112 - k * 16, 56, 112 - k * 16, 70 - k * 2, 110 - k * 16);
      stroke(cr, rgb(90, 40, 30), 1.4, 0.6);
    }
    ellipse(cr, 48, 41, 18, 5);
    setRgba(cr, rgb(50, 20, 20), 1.0);
    cairo_fill(cr);
  }
}

// Bubble Up's coral, 112 x 112, anchored at its root (56, 110): 0 staghorn
// (orange), 1 a sea fan (violet), 2 brain coral (yellow green), 3 tube coral
// (pink).
void paintCoral(cairo_t* cr, int v)
{
  if (v == 0)
  {
    std::function<void(double, double, double, double, int)> branch = [&](double x, double y, double a, double len, int depth) {
      const double ex = x + std::cos(a) * len, ey = y + std::sin(a) * len;
      cairo_move_to(cr, x, y);
      cairo_line_to(cr, ex, ey);
      stroke(cr, kInk, 9.0 - depth * 2.0);
      cairo_move_to(cr, x, y);
      cairo_line_to(cr, ex, ey);
      stroke(cr, rgb(255, 150, 70), 6.0 - depth * 1.6);
      if (depth < 2)
      {
        branch(ex, ey, a - 0.45, len * 0.7, depth + 1);
        branch(ex, ey, a + 0.4, len * 0.68, depth + 1);
      }
      else
      {
        circle(cr, ex, ey, 2.2);
        setRgba(cr, rgb(255, 230, 170), 1.0);
        cairo_fill(cr);
      }
    };
    branch(56, 110, -kPi * 0.5 - 0.35, 34, 0);
    branch(56, 110, -kPi * 0.5 + 0.4, 30, 0);
  }
  else if (v == 1)
  {
    // A sea fan: a lattice in a fan shape.
    cairo_move_to(cr, 56, 110);
    cairo_line_to(cr, 56, 96);
    stroke(cr, rgb(110, 50, 130), 5.0);
    cairo_save(cr);
    cairo_move_to(cr, 56, 98);
    cairo_curve_to(cr, 6, 80, 6, 22, 30, 10);
    cairo_curve_to(cr, 50, 2, 70, 2, 86, 12);
    cairo_curve_to(cr, 108, 26, 104, 80, 56, 98);
    cairo_close_path(cr);
    setRgba(cr, rgb(180, 90, 220), 0.25);
    cairo_fill_preserve(cr);
    cairo_clip(cr);
    for (int k = -6; k <= 6; ++k)
    {
      cairo_move_to(cr, 56, 100);
      cairo_line_to(cr, 56 + k * 9, 0);
      stroke(cr, rgb(200, 110, 240), 2.0, 0.9);
    }
    for (int k = 1; k < 7; ++k)
    {
      cairo_arc(cr, 56, 100, k * 14, kPi * 1.05, kPi * 1.95);
      stroke(cr, rgb(170, 80, 210), 1.4, 0.8);
    }
    cairo_restore(cr);
  }
  else if (v == 2)
  {
    ellipse(cr, 56, 96, 40, 28);
    lgrad(cr, 0, 68, 0, 124, {{0.0, rgb(220, 230, 110)}, {1.0, rgb(110, 140, 50)}}, 1.0, true);
    stroke(cr, kInk, 2.2);
    cairo_save(cr);
    ellipse(cr, 56, 96, 38, 26);
    cairo_clip(cr);
    for (int k = 0; k < 7; ++k)
    {
      cairo_move_to(cr, 18, 72 + k * 7);
      for (double x = 18; x <= 96; x += 6)
        cairo_line_to(cr, x, 72 + k * 7 + 3 * std::sin(x * 0.25 + k * 1.3));
      stroke(cr, rgb(90, 110, 40), 1.6, 0.8);
    }
    cairo_restore(cr);
  }
  else
  {
    for (const auto& tb : {std::array<double, 3>{38, 46, -0.2}, std::array<double, 3>{56, 66, 0.0}, std::array<double, 3>{74, 40, 0.25},
           std::array<double, 3>{48, 30, -0.1}, std::array<double, 3>{66, 30, 0.12}})
    {
      const double x = tb[0], h = tb[1], tx = x + tb[2] * h, ty = 110 - h;
      cairo_move_to(cr, x - 6, 110);
      cairo_line_to(cr, tx - 6, ty);
      cairo_line_to(cr, tx + 6, ty);
      cairo_line_to(cr, x + 6, 110);
      cairo_close_path(cr);
      lgrad(cr, x - 6, 0, x + 6, 0, {{0.0, rgb(220, 100, 150)}, {0.5, rgb(255, 160, 200)}, {1.0, rgb(160, 60, 110)}}, 1.0, true);
      stroke(cr, kInk, 1.6);
      ellipse(cr, tx, ty, 6, 2.4);
      setRgba(cr, rgb(90, 20, 50), 1.0);
      cairo_fill(cr);
    }
  }
}

// --- Painters: the wreck and the station ------------------------------------------------------

// A rusted steel plate of the ship, 64 x 64: plate seams with rivets, rust
// blooms and streaks, a crust of growth. v 0 plain, 1 with a frame (rib),
// 2 crusted.
void paintHull(cairo_t* cr, int v)
{
  cairo_rectangle(cr, 0, 0, 64, 64);
  lgrad(cr, 0, 0, 64, 64, {{0.0, rgb(96, 84, 76)}, {0.5, rgb(74, 64, 60)}, {1.0, rgb(52, 46, 46)}});
  Rng rng(std::uint32_t(30301 + v * 7));
  for (int i = 0; i < 5; ++i)
    glow(cr, rng.range(6, 58), rng.range(6, 58), rng.range(6, 14), rgb(170, 90, 40), 0.45);
  for (int i = 0; i < 3; ++i)
  {
    const double x = rng.range(8, 56);
    cairo_move_to(cr, x, rng.range(8, 30));
    cairo_line_to(cr, x + rng.range(-2, 2), rng.range(40, 64));
    stroke(cr, rgb(150, 70, 30), rng.range(1.5f, 3.0f), 0.35);
  }
  // Seams and rivets top and bottom.
  for (const double y : {1.0, 63.0})
  {
    cairo_move_to(cr, 0, y);
    cairo_line_to(cr, 64, y);
    stroke(cr, rgb(30, 24, 24), 1.6, 0.8);
    for (double x = 6; x < 64; x += 13)
    {
      circle(cr, x, y < 32 ? 6 : 58, 2.0);
      lgrad(cr, 0, y < 32 ? 4 : 56, 0, y < 32 ? 8 : 60, {{0.0, rgb(170, 150, 130)}, {1.0, rgb(60, 50, 46)}});
    }
  }
  if (v == 1)
  {
    cairo_rectangle(cr, 26, 0, 12, 64);
    lgrad(cr, 26, 0, 38, 0, {{0.0, rgb(120, 104, 92)}, {0.5, rgb(84, 72, 66)}, {1.0, rgb(40, 34, 34)}});
    for (double y = 10; y < 64; y += 14)
    {
      circle(cr, 32, y, 1.8);
      setRgba(cr, rgb(170, 150, 130), 0.9);
      cairo_fill(cr);
    }
  }
  else if (v == 2)
  {
    for (int i = 0; i < 9; ++i)
    {
      circle(cr, rng.range(8, 56), rng.range(36, 58), rng.range(2.0f, 4.0f));
      setRgba(cr, i % 3 ? rgb(70, 130, 100) : rgb(210, 210, 196), 0.75);
      cairo_fill(cr);
    }
  }
}

// A painted station panel, 64 x 64: off-white steel, panel seams, rivets;
// v 1 a vent grille, 2 a cable tray with coloured cables.
void paintStation(cairo_t* cr, int v)
{
  cairo_rectangle(cr, 0, 0, 64, 64);
  lgrad(cr, 0, 0, 0, 64, {{0.0, rgb(196, 210, 214)}, {1.0, rgb(150, 166, 172)}});
  roundedRect(cr, 3, 3, 58, 58, 4);
  stroke(cr, rgb(100, 116, 124), 1.4, 0.8);
  for (const double x : {7.0, 57.0})
    for (const double y : {7.0, 57.0})
    {
      circle(cr, x, y, 1.6);
      setRgba(cr, rgb(110, 124, 130), 1.0);
      cairo_fill(cr);
    }
  if (v == 1)
  {
    for (int k = 0; k < 6; ++k)
    {
      roundedRect(cr, 16, 16 + k * 6, 32, 3, 1.5);
      setRgba(cr, rgb(60, 72, 80), 1.0);
      cairo_fill(cr);
    }
  }
  else if (v == 2)
  {
    cairo_rectangle(cr, 0, 22, 64, 20);
    setRgba(cr, rgb(110, 124, 132), 1.0);
    cairo_fill(cr);
    const std::array<Color, 4> cables{rgb(250, 200, 40), rgb(40, 140, 230), rgb(230, 70, 60), rgb(30, 30, 36)};
    for (std::size_t k = 0; k < cables.size(); ++k)
    {
      cairo_move_to(cr, 0, 26 + k * 4);
      cairo_line_to(cr, 64, 26 + k * 4);
      stroke(cr, cables[k], 3.2);
    }
  }
}

// The porthole: a brass ring with its bolts over a pane of dark sea. 120 x
// 120 centred. `front` the ring and the glass's sheen (drawn over the alien),
// else the sea behind the glass.
void paintPorthole(cairo_t* cr, bool front)
{
  if (!front)
  {
    circle(cr, 60, 60, 44);
    cairo_pattern_t* g = cairo_pattern_create_radial(52, 50, 4, 60, 60, 46);
    stop(g, 0.0, rgb(40, 120, 150));
    stop(g, 1.0, rgb(4, 24, 40));
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
    return;
  }
  cairo_new_path(cr);
  cairo_arc(cr, 60, 60, 56, 0, 2 * kPi);
  cairo_arc_negative(cr, 60, 60, 44, 2 * kPi, 0);
  lgrad(cr, 0, 4, 0, 116, {{0.0, rgb(250, 220, 130)}, {0.5, rgb(190, 140, 60)}, {1.0, rgb(110, 76, 30)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  for (int k = 0; k < 8; ++k)
  {
    const double a = k * kPi / 4 + kPi / 8;
    circle(cr, 60 + std::cos(a) * 50, 60 + std::sin(a) * 50, 3.2);
    lgrad(cr, 0, 0, 0, 120, {{0.0, rgb(255, 240, 190)}, {1.0, rgb(130, 90, 40)}}, 1.0, true);
    stroke(cr, kInk, 1.0);
  }
  // Verdigris.
  glow(cr, 30, 92, 12, rgb(80, 180, 150), 0.5);
  // The glass's sheen.
  cairo_arc(cr, 60, 60, 36, kPi * 1.05, kPi * 1.45);
  stroke(cr, rgb(255, 255, 255), 5.0, 0.35);
  cairo_arc(cr, 60, 60, 30, kPi * 1.1, kPi * 1.3);
  stroke(cr, rgb(255, 255, 255), 2.4, 0.4);
}

// The level 18 alien in a fishbowl helmet, waving: 96 x 96 centred. pose 0/1
// the hand up or across.
void paintAlien(cairo_t* cr, int pose)
{
  const Color skin = kAlienGreen, skinDark = rgb(40, 120, 50);
  // Shoulders.
  cairo_move_to(cr, 22, 96);
  cairo_curve_to(cr, 24, 76, 36, 70, 48, 70);
  cairo_curve_to(cr, 60, 70, 72, 76, 74, 96);
  cairo_close_path(cr);
  lgrad(cr, 22, 0, 74, 0, {{0.0, skin}, {1.0, skinDark}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  // The waving arm.
  const double hx = pose ? 82.0 : 76.0, hy = pose ? 34.0 : 44.0;
  cairo_move_to(cr, 66, 78);
  cairo_curve_to(cr, 78, 70, 80, 56, hx, hy + 6);
  stroke(cr, kInk, 7.0);
  cairo_move_to(cr, 66, 78);
  cairo_curve_to(cr, 78, 70, 80, 56, hx, hy + 6);
  stroke(cr, skin, 4.4);
  circle(cr, hx, hy, 6.5);
  setRgba(cr, skin, 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.6);
  for (int k = -1; k <= 1; ++k)
  {
    cairo_move_to(cr, hx + k * 3, hy - 4);
    cairo_line_to(cr, hx + k * 4.5, hy - 10);
    stroke(cr, kInk, 3.4);
    cairo_move_to(cr, hx + k * 3, hy - 4);
    cairo_line_to(cr, hx + k * 4.5, hy - 10);
    stroke(cr, skin, 1.8);
  }
  // The head, the eyes, the antennae.
  for (const double side : {-1.0, 1.0})
  {
    cairo_move_to(cr, 48 + side * 6, 32);
    cairo_curve_to(cr, 48 + side * 10, 22, 48 + side * 13, 18, 48 + side * 15, 14);
    stroke(cr, kInk, 3.0);
    cairo_move_to(cr, 48 + side * 6, 32);
    cairo_curve_to(cr, 48 + side * 10, 22, 48 + side * 13, 18, 48 + side * 15, 14);
    stroke(cr, skin, 1.4);
    circle(cr, 48 + side * 15, 14, 3.2);
    setRgba(cr, rgb(255, 120, 200), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.0);
  }
  ellipse(cr, 48, 50, 18, 17);
  lgrad(cr, 30, 33, 66, 67, {{0.0, rgb(190, 255, 170)}, {1.0, skinDark}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  for (const double side : {-1.0, 1.0})
  {
    ellipse(cr, 48 + side * 7, 49, 5, 7.5, side * 0.4);
    setRgba(cr, rgb(16, 16, 26), 1.0);
    cairo_fill(cr);
    circle(cr, 48 + side * 7 - 1.5, 46, 1.8);
    setRgba(cr, rgb(255, 255, 255), 1.0);
    cairo_fill(cr);
  }
  cairo_arc(cr, 48, 58, 4, 0.2, kPi - 0.2);
  stroke(cr, kInk, 1.6);
  // The fishbowl.
  circle(cr, 48, 46, 30);
  setRgba(cr, rgb(200, 240, 255), 0.12);
  cairo_fill_preserve(cr);
  stroke(cr, rgb(220, 250, 255), 2.0, 0.7);
  cairo_arc(cr, 48, 46, 24, kPi * 1.1, kPi * 1.4);
  stroke(cr, rgb(255, 255, 255), 2.4, 0.7);
}

// --- Painters: the line, the gauge, the treasure -----------------------------------------------

// An anchor ring on its bracket: a heavy steel torus, 80 x 80 centred.
void paintAnchor(cairo_t* cr)
{
  // The bracket under it.
  roundedRect(cr, 30, 58, 20, 14, 3);
  lgrad(cr, 30, 0, 50, 0, {{0.0, rgb(150, 160, 168)}, {1.0, rgb(60, 68, 76)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  cairo_new_path(cr);
  cairo_arc(cr, 40, 36, 26, 0, 2 * kPi);
  cairo_arc_negative(cr, 40, 36, 17, 2 * kPi, 0);
  cairo_pattern_t* g = cairo_pattern_create_radial(34, 28, 4, 40, 36, 30);
  stop(g, 0.0, rgb(230, 236, 240));
  stop(g, 0.6, rgb(140, 150, 160));
  stop(g, 1.0, rgb(50, 58, 66));
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  stroke(cr, kInk, 2.0);
  cairo_arc(cr, 40, 36, 21.5, kPi * 1.1, kPi * 1.55);
  stroke(cr, rgb(255, 255, 255), 2.0, 0.6);
  // Barnacles and rust.
  glow(cr, 58, 48, 7, rgb(170, 90, 40), 0.6);
  for (const auto& b : {std::array<double, 2>{18, 46}, std::array<double, 2>{22, 52}})
  {
    circle(cr, b[0], b[1], 3);
    setRgba(cr, rgb(220, 220, 206), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 0.8);
  }
}

// One section of an anchor's post: 24 x 32, hung from the middle of its top.
void paintPost(cairo_t* cr)
{
  cairo_rectangle(cr, 5, 0, 14, 32);
  lgrad(cr, 5, 0, 19, 0, {{0.0, rgb(140, 150, 156)}, {0.5, rgb(96, 106, 114)}, {1.0, rgb(40, 46, 54)}}, 1.0, true);
  cairo_new_path(cr);
  cairo_move_to(cr, 5, 0);
  cairo_line_to(cr, 5, 32);
  cairo_move_to(cr, 19, 0);
  cairo_line_to(cr, 19, 32);
  stroke(cr, kInk, 1.6);
  roundedRect(cr, 3, 26, 18, 4, 2);
  setRgba(cr, rgb(70, 78, 86), 1.0);
  cairo_fill(cr);
}

// A harpoon: barbed head on the right, a steel shaft, fins and the line's
// eye at the tail. 128 x 30, anchored where the barbs bite (106, 15).
void paintHarpoon(cairo_t* cr)
{
  // Fins.
  for (const double s : {-1.0, 1.0})
  {
    cairo_move_to(cr, 14, 15);
    cairo_line_to(cr, 8, 15 + s * 10);
    cairo_line_to(cr, 30, 15 + s * 2);
    cairo_close_path(cr);
    setRgba(cr, rgb(250, 120, 40), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.2);
  }
  // The shaft.
  roundedRect(cr, 8, 12.5, 98, 5, 2.5);
  lgrad(cr, 0, 12.5, 0, 17.5, {{0.0, rgb(240, 244, 248)}, {0.5, rgb(170, 178, 186)}, {1.0, rgb(80, 88, 96)}}, 1.0, true);
  stroke(cr, kInk, 1.2);
  // The eye at the tail.
  circle(cr, 6, 15, 4);
  stroke(cr, kInk, 3.4);
  circle(cr, 6, 15, 4);
  stroke(cr, rgb(200, 206, 212), 1.8);
  // The head and its barbs.
  cairo_move_to(cr, 102, 10);
  cairo_line_to(cr, 126, 15);
  cairo_line_to(cr, 102, 20);
  cairo_close_path(cr);
  lgrad(cr, 0, 10, 0, 20, {{0.0, rgb(250, 252, 255)}, {1.0, rgb(110, 120, 130)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  for (const double s : {-1.0, 1.0})
  {
    cairo_move_to(cr, 108, 15 + s * 3);
    cairo_line_to(cr, 98, 15 + s * 10);
    cairo_line_to(cr, 104, 15 + s * 3);
    cairo_close_path(cr);
    setRgba(cr, rgb(200, 206, 214), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.0);
  }
}

// The Harpoon Line as an icon: a speargun with its harpoon loaded, the line
// coiled on a reel. 64 x 64, pointing right.
void paintGun(cairo_t* cr)
{
  // The harpoon on top.
  cairo_move_to(cr, 8, 24);
  cairo_line_to(cr, 54, 24);
  stroke(cr, kInk, 4.4);
  cairo_move_to(cr, 8, 24);
  cairo_line_to(cr, 54, 24);
  stroke(cr, rgb(220, 226, 232), 2.2);
  cairo_move_to(cr, 52, 20);
  cairo_line_to(cr, 62, 24);
  cairo_line_to(cr, 52, 28);
  cairo_close_path(cr);
  setRgba(cr, rgb(240, 244, 250), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.4);
  // The barrel and the stock.
  roundedRect(cr, 10, 27, 40, 8, 3);
  lgrad(cr, 0, 27, 0, 35, {{0.0, rgb(90, 220, 220)}, {1.0, rgb(20, 110, 120)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  cairo_move_to(cr, 12, 33);
  cairo_line_to(cr, 4, 46);
  cairo_line_to(cr, 12, 48);
  cairo_line_to(cr, 20, 35);
  cairo_close_path(cr);
  lgrad(cr, 0, 33, 0, 48, {{0.0, rgb(70, 190, 196)}, {1.0, rgb(16, 90, 100)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  // The grip and trigger.
  cairo_move_to(cr, 24, 35);
  cairo_line_to(cr, 22, 50);
  cairo_line_to(cr, 30, 50);
  cairo_line_to(cr, 31, 35);
  cairo_close_path(cr);
  setRgba(cr, rgb(40, 50, 60), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.4);
  // The bands and the reel.
  for (const double x : {40.0, 46.0})
  {
    cairo_move_to(cr, x, 22);
    cairo_line_to(cr, x - 2, 36);
    stroke(cr, rgb(250, 140, 50), 2.2);
  }
  circle(cr, 38, 42, 6);
  lgrad(cr, 0, 36, 0, 48, {{0.0, rgb(230, 230, 220)}, {1.0, rgb(150, 140, 120)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  circle(cr, 38, 42, 2);
  setRgba(cr, kInk, 1.0);
  cairo_fill(cr);
}

// The station's depth gauge on a brass plate: a round dial (the needle is
// drawn live), the number and its unit beside it. 240 x 130 centred.
void paintGauge(cairo_t* cr, const std::string& text)
{
  roundedRect(cr, 4, 4, 232, 122, 12);
  lgrad(cr, 0, 4, 0, 126, {{0.0, rgb(236, 200, 110)}, {0.5, rgb(186, 140, 60)}, {1.0, rgb(120, 84, 34)}}, 1.0, true);
  stroke(cr, kInk, 2.4);
  for (const double x : {16.0, 224.0})
    for (const double y : {16.0, 114.0})
    {
      circle(cr, x, y, 3.4);
      setRgba(cr, rgb(100, 70, 30), 1.0);
      cairo_fill(cr);
      cairo_move_to(cr, x - 2.4, y);
      cairo_line_to(cr, x + 2.4, y);
      stroke(cr, rgb(250, 230, 170), 1.0);
    }
  // The dial: a bezel, a cream face, ticks from 0 to 60 over 270 degrees,
  // red past 50.
  const double cx = 70, cy = 65;
  circle(cr, cx, cy, 50);
  lgrad(cr, 0, 15, 0, 115, {{0.0, rgb(250, 240, 200)}, {1.0, rgb(130, 96, 40)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  circle(cr, cx, cy, 42);
  setRgba(cr, rgb(244, 238, 220), 1.0);
  cairo_fill(cr);
  cairo_arc(cr, cx, cy, 36, kPi * 0.75 + 1.5 * kPi * 50 / 60, kPi * 2.25);
  stroke(cr, rgb(220, 50, 40), 5.0, 0.85);
  for (int k = 0; k <= 30; ++k)
  {
    const double a = kPi * 0.75 + 1.5 * kPi * k / 30.0, r0 = k % 5 ? 36 : 31;
    cairo_move_to(cr, cx + std::cos(a) * r0, cy + std::sin(a) * r0);
    cairo_line_to(cr, cx + std::cos(a) * 40, cy + std::sin(a) * 40);
    stroke(cr, kInk, k % 5 ? 1.0 : 2.0);
  }
  for (int k = 0; k <= 3; ++k)
  {
    const double a = kPi * 0.75 + 1.5 * kPi * k / 3.0;
    const std::string n = std::to_string(k * 20);
    lettering(cr, n.c_str(), cx + std::cos(a) * 22, cy + std::sin(a) * 22 + 4, 11, kInk);
  }
  // The glass.
  cairo_arc(cr, cx, cy, 38, kPi * 1.1, kPi * 1.45);
  stroke(cr, rgb(255, 255, 255), 3.0, 0.5);
  // The number and its unit, engraved.
  const auto sp = text.find(' ');
  const std::string num = sp == std::string::npos ? text : text.substr(0, sp);
  const std::string unit = sp == std::string::npos ? std::string() : text.substr(sp + 1);
  lettering(cr, num.c_str(), 176, 74, 46, rgb(70, 44, 14));
  lettering(cr, num.c_str(), 175, 72, 46, rgb(255, 236, 170));
  if (!unit.empty())
  {
    lettering(cr, unit.c_str(), 176, 102, 16, rgb(70, 44, 14));
    lettering(cr, unit.c_str(), 175, 101, 16, rgb(255, 236, 170));
  }
}

// The waterproof tape: a MaxTV cassette sealed in a clear dry bag, its top
// rolled and clipped in yellow, an air bubble caught on it. 96 x 96, its
// 64 px block at (16, 16).
void paintWaterproofTape(cairo_t* cr)
{
  // The bag behind.
  roundedRect(cr, 8, 18, 80, 66, 14);
  setRgba(cr, rgb(180, 230, 255), 0.22);
  cairo_fill_preserve(cr);
  stroke(cr, rgb(220, 248, 255), 2.0, 0.8);
  // The tape inside.
  roundedRect(cr, 16, 36, 64, 38, 5);
  lgrad(cr, 0, 36, 0, 74, {{0.0, rgb(52, 52, 60)}, {1.0, rgb(16, 16, 20)}}, 1.0, true);
  stroke(cr, rgb(6, 6, 8), 2.0);
  roundedRect(cr, 22, 40, 52, 12, 2);
  setRgba(cr, rgb(250, 244, 226), 1.0);
  cairo_fill(cr);
  lettering(cr, "MAXTV", 48, 50, 9, rgb(220, 40, 120));
  for (const double rx : {38.0, 58.0})
  {
    circle(cr, rx, 62, 4.6);
    setRgba(cr, rgb(230, 230, 236), 1.0);
    cairo_fill(cr);
    circle(cr, rx, 62, 1.8);
    setRgba(cr, rgb(30, 30, 36), 1.0);
    cairo_fill(cr);
  }
  // The bag over it: crinkles and sheen.
  roundedRect(cr, 8, 18, 80, 66, 14);
  setRgba(cr, rgb(200, 240, 255), 0.12);
  cairo_fill(cr);
  for (const auto& c : {std::array<double, 4>{14, 30, 26, 80}, std::array<double, 4>{70, 26, 82, 60}, std::array<double, 4>{40, 80, 60, 78}})
  {
    cairo_move_to(cr, c[0], c[1]);
    cairo_line_to(cr, c[2], c[3]);
    stroke(cr, rgb(255, 255, 255), 1.2, 0.35);
  }
  cairo_move_to(cr, 16, 28);
  cairo_curve_to(cr, 14, 40, 14, 56, 16, 70);
  stroke(cr, rgb(255, 255, 255), 3.0, 0.45);
  // The rolled top and its clip.
  roundedRect(cr, 10, 10, 76, 12, 6);
  setRgba(cr, rgb(200, 236, 250), 0.6);
  cairo_fill_preserve(cr);
  stroke(cr, rgb(220, 248, 255), 1.4, 0.8);
  roundedRect(cr, 6, 12, 84, 8, 3);
  lgrad(cr, 0, 12, 0, 20, {{0.0, rgb(255, 230, 80)}, {1.0, rgb(210, 160, 20)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  for (const double x : {6.0, 90.0})
  {
    circle(cr, x, 16, 3);
    setRgba(cr, rgb(40, 40, 46), 1.0);
    cairo_fill(cr);
  }
  // A bubble caught on the bag.
  circle(cr, 78, 74, 5);
  setRgba(cr, rgb(220, 250, 255), 0.25);
  cairo_fill_preserve(cr);
  stroke(cr, rgb(240, 255, 255), 1.2, 0.9);
}

// --- Painters: Bubble Up and the HUD ----------------------------------------------------------

// A bubble ring: an ellipse of light seen at a slant, little bubbles on it.
// 400 x 120 centred, white (tinted live).
void paintHoop(cairo_t* cr)
{
  ellipse(cr, 200, 60, 180, 34);
  stroke(cr, rgb(255, 255, 255), 14.0, 0.18);
  ellipse(cr, 200, 60, 180, 34);
  stroke(cr, rgb(255, 255, 255), 5.0, 0.8);
  ellipse(cr, 200, 58, 172, 28);
  stroke(cr, rgb(255, 255, 255), 1.4, 0.5);
  for (int k = 0; k < 16; ++k)
  {
    const double a = k * 2 * kPi / 16 + 0.2;
    circle(cr, 200 + std::cos(a) * 180, 60 + std::sin(a) * 34, 3 + (k % 3));
    stroke(cr, rgb(255, 255, 255), 1.4, 0.8);
  }
}

// The runner's bubble: a thin skin, its rim brighter, a rainbow sheen and a
// window of light. 240 x 240 centred.
void paintRunnerBubble(cairo_t* cr)
{
  const double c = 120, R = 108;
  circle(cr, c, c, R);
  cairo_pattern_t* g = cairo_pattern_create_radial(c, c, R * 0.6, c, c, R);
  cairo_pattern_add_color_stop_rgba(g, 0.0, 0.8, 0.95, 1.0, 0.04);
  cairo_pattern_add_color_stop_rgba(g, 0.85, 0.8, 0.95, 1.0, 0.16);
  cairo_pattern_add_color_stop_rgba(g, 1.0, 0.9, 1.0, 1.0, 0.5);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
  circle(cr, c, c, R);
  stroke(cr, rgb(230, 250, 255), 2.4, 0.85);
  // The sheen: arcs of rainbow along the rim.
  const std::array<Color, 3> sheen{rgb(255, 120, 220), rgb(120, 230, 255), rgb(255, 230, 120)};
  for (std::size_t k = 0; k < sheen.size(); ++k)
  {
    cairo_arc(cr, c, c, R - 6 - k * 4, kPi * (0.15 + k * 0.08), kPi * (0.6 + k * 0.06));
    stroke(cr, sheen[k], 3.0, 0.35);
  }
  // A window of light top left, a glint bottom right.
  cairo_save(cr);
  cairo_translate(cr, c - R * 0.45, c - R * 0.5);
  cairo_rotate(cr, -0.7);
  roundedRect(cr, -22, -10, 44, 20, 9);
  cairo_restore(cr);
  setRgba(cr, rgb(255, 255, 255), 0.6);
  cairo_fill(cr);
  cairo_arc(cr, c, c, R - 10, kPi * 0.2, kPi * 0.38);
  stroke(cr, rgb(255, 255, 255), 3.0, 0.5);
}

// A stroke pip: a glossy bubble (full) or the ring it leaves (spent). 34 x
// 34 centred.
void paintPip(cairo_t* cr, bool full)
{
  if (!full)
  {
    circle(cr, 17, 17, 11);
    stroke(cr, rgb(150, 220, 255), 2.0, 0.45);
    return;
  }
  glow(cr, 17, 17, 16, rgb(120, 220, 255), 0.5);
  circle(cr, 17, 17, 11);
  cairo_pattern_t* g = cairo_pattern_create_radial(13, 12, 1, 17, 17, 12);
  stop(g, 0.0, rgb(255, 255, 255), 0.95);
  stop(g, 0.5, rgb(140, 230, 255), 0.7);
  stop(g, 1.0, rgb(40, 140, 220), 0.9);
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  stroke(cr, rgb(230, 250, 255), 1.6);
  ellipse(cr, 13, 11, 4, 2.6, -0.6);
  setRgba(cr, rgb(255, 255, 255), 0.9);
  cairo_fill(cr);
}

// An Anglerfish's lure: a gem-shaped light, cut like a jewel to fool you.
// 48 x 48 centred, white (tinted and added live).
void paintLure(cairo_t* cr)
{
  glow(cr, 24, 24, 22, rgb(255, 255, 255), 0.6);
  cairo_move_to(cr, 24, 10);
  cairo_line_to(cr, 34, 19);
  cairo_line_to(cr, 24, 38);
  cairo_line_to(cr, 14, 19);
  cairo_close_path(cr);
  setRgba(cr, rgb(255, 255, 255), 0.95);
  cairo_fill(cr);
  cairo_move_to(cr, 14, 19);
  cairo_line_to(cr, 34, 19);
  cairo_move_to(cr, 24, 10);
  cairo_line_to(cr, 24, 38);
  stroke(cr, rgb(160, 160, 160), 0.8, 0.6);
}

// A four-pointed glint: 48 x 48 centred, white.
void paintStar(cairo_t* cr)
{
  glow(cr, 24, 24, 10, rgb(255, 255, 255), 0.8);
  for (const double a : {0.0, kPi * 0.5})
  {
    cairo_move_to(cr, 24 + std::cos(a) * 22, 24 + std::sin(a) * 22);
    cairo_line_to(cr, 24 + std::cos(a + kPi * 0.5) * 2.2, 24 + std::sin(a + kPi * 0.5) * 2.2);
    cairo_line_to(cr, 24 - std::cos(a) * 22, 24 - std::sin(a) * 22);
    cairo_line_to(cr, 24 - std::cos(a + kPi * 0.5) * 2.2, 24 - std::sin(a + kPi * 0.5) * 2.2);
    cairo_close_path(cr);
    setRgba(cr, rgb(255, 255, 255), 1.0);
    cairo_fill(cr);
  }
}

// --- Cached textures -----------------------------------------------------------------------------

const Texture& shaftTex(const Art& a, const Renderer& r) { return baked(a, r, "shaft", 240, 1000, 120, 0, paintShaft); }
const Texture& bubbleTex(const Art& a, const Renderer& r) { return baked(a, r, "bubble", 24, 24, 12, 12, paintBubble); }
const Texture& beadTex(const Art& a, const Renderer& r) { return baked(a, r, "bead", 24, 24, 12, 12, paintBead); }
const Texture& starTex(const Art& a, const Renderer& r) { return baked(a, r, "star", 48, 48, 24, 24, paintStar); }
const Texture& lureTex(const Art& a, const Renderer& r) { return baked(a, r, "lure", 48, 48, 24, 24, paintLure); }

const Texture& whaleTex(const Art& a, const Renderer& r, int tail)
{
  return baked(a, r, "whale" + std::to_string(tail), 700, 260, 350, 130, [=](cairo_t* cr) { paintWhale(cr, tail); });
}

const Texture& kelpTex(const Art& a, const Renderer& r, int v)
{
  return baked(a, r, "kelp" + std::to_string(v), 120, 340, 60, 340, [=](cairo_t* cr) { paintKelp(cr, v); });
}

const Texture& floraTex(const Art& a, const Renderer& r, int v)
{
  return baked(a, r, "flora" + std::to_string(v), 96, 112, 48, 110, [=](cairo_t* cr) { paintFlora(cr, v); });
}

const Texture& coralTex(const Art& a, const Renderer& r, int v)
{
  return baked(a, r, "coral" + std::to_string(v), 112, 112, 56, 110, [=](cairo_t* cr) { paintCoral(cr, v); });
}

const Texture& hullTex(const Art& a, const Renderer& r, int v)
{
  return baked(a, r, "hull" + std::to_string(v), 64, 64, 0, 0, [=](cairo_t* cr) { paintHull(cr, v); });
}

const Texture& stationTex(const Art& a, const Renderer& r, int v)
{
  return baked(a, r, "station" + std::to_string(v), 64, 64, 0, 0, [=](cairo_t* cr) { paintStation(cr, v); });
}

const Texture& pipTex(const Art& a, const Renderer& r, bool full)
{
  return baked(a, r, full ? "pip1" : "pip0", 34, 34, 17, 17, [=](cairo_t* cr) { paintPip(cr, full); });
}

// The fiber cable's route through Fiber Trench, in blocks: along the floors
// (just above each), down the step faces, into the ship's hold and out of
// its stern, down the trench's right wall, sagging across the trench's
// floor under both crossings, up and over the pillar, along the ledge and
// down to the cable station's relay. Only for the level it was laid in
// (checked against its size and its exit).
constexpr std::array<std::array<float, 2>, 69> kCableRoute{{
  {-1.0f, 11.8f}, {26.6f, 11.8f}, {27.25f, 12.3f}, {27.25f, 15.4f}, {27.7f, 15.8f},
  {34.6f, 15.8f}, {35.25f, 16.3f}, {35.25f, 19.4f}, {35.7f, 19.8f},
  {42.6f, 19.8f}, {43.25f, 20.3f}, {43.25f, 23.4f}, {43.7f, 23.8f},
  {50.6f, 23.8f}, {51.25f, 24.3f}, {51.25f, 27.4f}, {51.7f, 27.8f},
  {56.6f, 27.8f}, {57.25f, 28.3f}, {57.25f, 33.4f}, {57.7f, 33.8f},
  {74.0f, 33.8f}, {90.6f, 33.8f}, {91.25f, 34.3f}, {91.25f, 37.4f}, {91.7f, 37.8f},
  {98.6f, 37.8f}, {99.25f, 38.3f}, {99.3f, 41.4f}, {99.7f, 41.8f},
  {102.6f, 41.8f}, {103.25f, 42.3f}, {103.3f, 47.0f}, {103.5f, 50.0f}, {103.75f, 52.6f},
  {103.75f, 64.0f}, {103.6f, 72.0f}, {102.0f, 75.6f}, {99.0f, 76.4f}, {96.0f, 75.6f}, {94.4f, 72.0f},
  {94.25f, 64.0f}, {94.25f, 54.6f}, {93.7f, 53.8f},
  {88.4f, 53.8f}, {87.75f, 54.6f}, {87.75f, 64.0f}, {87.6f, 72.0f}, {85.6f, 75.8f}, {82.0f, 76.6f},
  {78.4f, 75.8f}, {76.4f, 72.0f}, {76.25f, 64.0f}, {76.25f, 56.6f}, {75.7f, 55.8f},
  {60.4f, 55.8f}, {59.75f, 56.3f}, {59.75f, 59.4f}, {59.4f, 59.8f},
  {40.4f, 59.8f}, {39.75f, 60.3f}, {39.75f, 63.4f}, {39.4f, 63.8f},
  {30.0f, 63.8f}, {20.0f, 63.8f}, {14.0f, 63.8f}, {12.6f, 63.8f}, {12.3f, 63.8f}, {12.2f, 63.8f},
}};

struct CablePt
{
  float x, y; // px
  float s;    // px along the cable
};

// The route as a smooth curve (Catmull-Rom through the points), sampled
// every few px; empty if this isn't Fiber Trench.
const std::vector<CablePt>& cablePath(const World& w)
{
  static std::vector<CablePt> path;
  static bool built = false;
  const auto& lv = w.level();
  if (!(lv.width == 120 && lv.height == 80 && lv.exitTx == 12 && lv.exitTy == 63 && w.trench().water))
  {
    static const std::vector<CablePt> none;
    return none;
  }
  if (built)
    return path;
  built = true;
  const std::size_t n = kCableRoute.size();
  auto pt = [&](std::size_t i) {
    const auto& p = kCableRoute[std::min(i, n - 1)];
    return std::array<float, 2>{p[0] * kTilePx, p[1] * kTilePx};
  };
  float s = 0.0f;
  float px = pt(0)[0], py = pt(0)[1];
  path.push_back({px, py, 0.0f});
  for (std::size_t i = 0; i + 1 < n; ++i)
  {
    const auto p0 = pt(i == 0 ? 0 : i - 1), p1 = pt(i), p2 = pt(i + 1), p3 = pt(i + 2);
    const float len = std::hypot(p2[0] - p1[0], p2[1] - p1[1]);
    const int steps = std::max(1, int(len / 6.0f));
    for (int k = 1; k <= steps; ++k)
    {
      const float t = float(k) / float(steps), t2 = t * t, t3 = t2 * t;
      const float x = 0.5f * (2 * p1[0] + (-p0[0] + p2[0]) * t + (2 * p0[0] - 5 * p1[0] + 4 * p2[0] - p3[0]) * t2 +
        (-p0[0] + 3 * p1[0] - 3 * p2[0] + p3[0]) * t3);
      const float y = 0.5f * (2 * p1[1] + (-p0[1] + p2[1]) * t + (2 * p0[1] - 5 * p1[1] + 4 * p2[1] - p3[1]) * t2 +
        (-p0[1] + 3 * p1[1] - 3 * p2[1] + p3[1]) * t3);
      s += std::hypot(x - px, y - py);
      path.push_back({x, y, s});
      px = x;
      py = y;
    }
  }
  return path;
}

// The cable, glowing: a soft halo, the jacket in beads with its bright core,
// and light pulses running along it toward the station.
void drawCable(Renderer& r, const Art& art, const std::vector<CablePt>& path, float camX, float camY, int frame)
{
  if (path.size() < 2)
    return;
  const float total = path.back().s;
  const Texture& bead = beadTex(art, r);
  float lastGlow = -1000.0f, lastBead = -1000.0f;
  for (const auto& p : path)
  {
    const float x = p.x - camX, y = p.y - camY;
    if (x < -80.0f || x > float(kScreenW) + 80.0f || y < -80.0f || y > float(kScreenH) + 80.0f)
      continue;
    if (p.s - lastGlow >= 40.0f)
    {
      drawGlow(r, art, x, y, 34.0f, kFiber, 0.16f + 0.05f * std::sin(p.s * 0.01f + float(frame) * 0.05f));
      lastGlow = p.s;
    }
    if (p.s - lastBead >= 3.0f)
    {
      r.draw(bead, x, y);
      lastBead = p.s;
    }
  }
  // The pulses: one every 1100 px, 7 px a frame.
  const float spacing = 1100.0f, head = std::fmod(float(frame) * 7.0f, spacing);
  for (float s0 = head; s0 < total; s0 += spacing)
  {
    // Find the point at s0 (the samples are about 6 px apart).
    const auto it = std::lower_bound(path.begin(), path.end(), s0, [](const CablePt& p, float s) { return p.s < s; });
    if (it == path.end())
      break;
    const float x = it->x - camX, y = it->y - camY;
    if (x < -120.0f || x > float(kScreenW) + 120.0f || y < -120.0f || y > float(kScreenH) + 120.0f)
      continue;
    drawGlow(r, art, x, y, 70.0f, kFiber, 0.55f);
    drawGlow(r, art, x, y, 22.0f, kFiberCore, 0.9f);
    // A short bright tail behind it.
    for (int k = 1; k <= 6; ++k)
    {
      const auto jt = it - std::min<std::ptrdiff_t>(it - path.begin(), k * 3);
      drawGlow(r, art, jt->x - camX, jt->y - camY, 20.0f - float(k) * 2.0f, kFiberCore, 0.5f - float(k) * 0.07f);
    }
  }
}

// The shape of an alpha ramp down a column of rows (row centres in blocks).
std::vector<float> rowRamp(int row0, int rows, int cols, const std::function<float(float)>& f)
{
  std::vector<float> a(std::size_t(rows * cols));
  for (int j = 0; j < rows; ++j)
  {
    const float v = f(float(row0 + j) + 0.5f);
    for (int i = 0; i < cols; ++i)
      a[std::size_t(j * cols + i)] = v;
  }
  return a;
}

// The water's darkening below the surface, 0..1 at block row `row`.
float depthShade(float row, float height) { return smooth((row / height - 0.08f) / 0.9f) * 0.86f; }

} // namespace

// --- Behind the tiles ------------------------------------------------------------------------------

void drawTrenchSet(Renderer& r, const World& w, const Art& art, float camX, float camY, int frame)
{
  const auto& t = w.trench();
  const auto& lv = w.level();
  const auto& map = w.map();
  if (!t.water && !t.bubble)
    return;
  const float height = float(lv.height);
  const int row0 = int(std::floor(camY / kTilePx)) - 1;
  const int rows = int(float(kScreenH) / kTilePx) + 4;
  const float midRow = (camY + float(kScreenH) * 0.5f) / kTilePx;
  // The water darkens as you go down.
  {
    const float deep = t.bubble ? 0.45f : 1.0f; // Bubble Up's shaft is lit all the way down
    const auto a = rowRamp(row0, rows, 2, [&](float row) { return depthShade(row, height) * deep; });
    r.drawAlphaGrid(a, 2, rows, 0.0f, float(row0) * kTilePx - camY, float(kScreenW) * 0.5f, kTilePx, kDeep);
  }
  // The sun's shafts, fading out with depth.
  const float sun = 1.0f - smooth((midRow - 6.0f) / (t.bubble ? 120.0f : 30.0f));
  if (sun > 0.01f)
  {
    const Texture& shaft = shaftTex(art, r);
    for (int k = 0; k < 5; ++k)
    {
      const float x = std::fmod(float(k) * 430.0f + 200.0f - camX * 0.35f + 4300.0f, 2150.0f) - 300.0f;
      DrawOpts o;
      o.blend = Blend::Add;
      o.tint = rgb(170, 240, 255);
      o.angle = 16.0f + 4.0f * std::sin(float(frame) * 0.004f + float(k) * 1.3f);
      o.alpha = sun * (0.09f + 0.05f * std::sin(float(frame) * 0.012f + float(k) * 1.7f));
      o.scale = 1.0f + 0.25f * float(k % 2);
      r.draw(shaft, x, -camY * 0.4f - 40.0f, o);
    }
  }
  // Motes: marine snow drifting slowly down (up in Bubble Up's rush).
  {
    const float fall = t.bubble ? -0.35f : 0.12f;
    for (int i = 0; i < 64; ++i)
    {
      const std::uint32_t h = hash2(i * 31 + 7, i * 17 + 3);
      const float par = 0.45f + float(h % 7u) * 0.05f;
      const float x = std::fmod(float(h % 1700u) - camX * par + float(frame) * 0.06f * std::sin(float(i)) + 17000.0f, 1700.0f) - 200.0f;
      const float y = std::fmod(float((h >> 11) % 1100u) - camY * par + float(frame) * fall * (0.6f + par) + 11000.0f, 1100.0f) - 190.0f;
      DrawOpts o;
      o.blend = Blend::Add;
      o.tint = rgb(190, 235, 255);
      o.scale = 0.25f + float((h >> 5) % 5u) * 0.07f;
      o.alpha = (0.18f + float((h >> 9) % 5u) * 0.06f) * (1.0f - 0.6f * depthShade((camY + y) / kTilePx, height));
      r.draw(art.dot, x, y, o);
    }
  }
  // The abyss: the deep black, the whale crossing it, its few lights.
  for (const auto& a : t.abyss)
  {
    const float x = float(a.x) * kCellPx - camX - kTilePx * 2.0f, y = float(a.y) * kCellPx - camY;
    const float wd = float(a.w) * kCellPx + kTilePx * 4.0f, h = float(a.h) * kCellPx;
    if (offScreen(x, y - 8.0f * kTilePx, wd, h + 8.0f * kTilePx))
      continue;
    const int top = a.y / kCellsPerTile - 8, n = a.h / kCellsPerTile + 9;
    const auto ramp = rowRamp(top, n, 2, [&](float row) { return smooth((row - float(top)) / float(n - 1)) * 0.92f; });
    r.drawAlphaGrid(ramp, 2, n, x, float(top) * kTilePx - camY, wd * 0.5f, kTilePx, kDeep);
    // Specks of living light down there.
    for (int k = 0; k < 14; ++k)
    {
      const std::uint32_t hh = hash2(k * 13, a.x + k);
      const float sx = x + float(hh % 1000u) / 1000.0f * wd, sy = y + float((hh >> 10) % 1000u) / 1000.0f * h * 0.9f;
      const float tw = 0.5f + 0.5f * std::sin(float(frame) * 0.05f + float(k) * 2.1f);
      drawGlow(r, art, sx + std::sin(float(frame) * 0.01f + float(k)) * 12.0f, sy, 10.0f, k % 3 ? kFiber : rgb(160, 255, 140),
        0.25f * tw);
    }
    const int ph = frame % kWhaleEvery;
    if (ph < 360)
    {
      const float u = float(ph) / 360.0f;
      const float wx = x - 400.0f + (wd + 800.0f) * u, wy = y + h * 0.42f + std::sin(u * 6.0f) * 14.0f;
      DrawOpts o;
      o.alpha = 0.75f * smooth(u * 6.0f) * smooth((1.0f - u) * 6.0f);
      o.angle = std::sin(u * 6.0f + 1.0f) * 2.0f;
      r.draw(whaleTex(art, r, (frame / 24) % 2), wx, wy, o);
    }
  }
  // The sunken ship's hold: its back wall, ribbed, behind the open water.
  for (const auto& d : t.darks)
  {
    const int bx0 = d.x / kCellsPerTile, by0 = d.y / kCellsPerTile;
    const int bx1 = (d.x + d.w) / kCellsPerTile - 1, by1 = (d.y + d.h) / kCellsPerTile - 1;
    for (int by = std::max(by0, row0); by <= std::min(by1, row0 + rows); ++by)
      for (int bx = bx0; bx <= bx1; ++bx)
      {
        const float x = float(bx) * kTilePx - camX, y = float(by) * kTilePx - camY;
        if (offScreen(x, y, kTilePx, kTilePx, 0.0f) || map.block(bx, by) == Tile::Solid)
          continue;
        const int v = (bx - bx0) % 5 == 2 ? 1 : (hash2(bx, by) % 7u == 0 ? 2 : 0);
        DrawOpts o;
        o.tint = rgb(118, 124, 138);
        r.draw(hullTex(art, r, v), x, y, o);
      }
  }
  // The station's walls, lit: every air pocket outside the hold.
  for (const auto& pk : t.pockets)
  {
    bool hold = false;
    for (const auto& d : t.darks)
      hold = hold || d.intersects(pk);
    if (hold)
      continue;
    const int bx0 = pk.x / kCellsPerTile, by0 = pk.y / kCellsPerTile;
    const int bx1 = (pk.x + pk.w) / kCellsPerTile - 1, by1 = (pk.y + pk.h) / kCellsPerTile - 1;
    for (int by = by0; by <= by1; ++by)
      for (int bx = bx0; bx <= bx1; ++bx)
      {
        const float x = float(bx) * kTilePx - camX, y = float(by) * kTilePx - camY;
        if (offScreen(x, y, kTilePx, kTilePx, 0.0f) || map.block(bx, by) == Tile::Solid)
          continue;
        const int v = by == by0 + 2 ? 2 : (hash2(bx, by) % 6u == 0 ? 1 : 0);
        DrawOpts o;
        o.tint = rgb(150, 172, 182);
        r.draw(stationTex(art, r, v), x, y, o);
      }
    // Its lamps along the ceiling.
    for (int bx = bx0 + 2; bx <= bx1; bx += 6)
    {
      const float x = float(bx) * kTilePx + 32.0f - camX, y = float(by0) * kTilePx + 10.0f - camY;
      if (offScreen(x - 100.0f, y, 200.0f, 200.0f))
        continue;
      r.fillRect(x - 22.0f, y - 6.0f, 44.0f, 10.0f, rgb(60, 70, 80));
      r.fillRect(x - 18.0f, y + 2.0f, 36.0f, 5.0f, rgb(255, 236, 190));
      drawGlow(r, art, x, y + 30.0f, 120.0f, rgb(255, 220, 160), 0.22f);
    }
  }
  // The porthole, and the alien waving behind it.
  if (t.portX >= 0)
  {
    const float cx = float(t.portX + 1) * kCellPx - camX, cy = float(t.portY) * kCellPx - camY;
    if (!offScreen(cx - 60.0f, cy - 60.0f, 120.0f, 120.0f))
    {
      r.draw(baked(art, r, "port_back", 120, 120, 60, 60, [](cairo_t* cr) { paintPorthole(cr, false); }), cx, cy);
      if (t.wave > 0)
      {
        const float rise = smooth(float(std::min(t.wave, 24 - t.wave + 1)) / 5.0f);
        const int pose = (t.wave / 6) % 2;
        DrawOpts o;
        o.alpha = rise;
        drawGlow(r, art, cx, cy, 50.0f, kAlienGreen, 0.35f * rise);
        r.draw(baked(art, r, "alien" + std::to_string(pose), 96, 96, 48, 48, [=](cairo_t* cr) { paintAlien(cr, pose); }), cx,
          cy + (1.0f - rise) * 30.0f, o);
      }
      r.draw(baked(art, r, "port_front", 120, 120, 60, 60, [](cairo_t* cr) { paintPorthole(cr, true); }), cx, cy);
    }
  }
  // The cable along the route.
  drawCable(r, art, cablePath(w), camX, camY, frame);
}

bool drawTrenchShot(Renderer& r, const Art& art, const Projectile& pr, float cx, float cy, int frame)
{
  (void)frame;
  if (pr.kind != ShotKind::Proto || pr.proto != int(ProtoId::HarpoonLine))
    return false;
  float dx = float(pr.dx), dy = float(pr.dy);
  if (dx == 0.0f && dy == 0.0f)
    dx = 1.0f;
  const float len = std::sqrt(dx * dx + dy * dy);
  dx /= len;
  dy /= len;
  // The line paying out behind it, a wave running down it as it goes.
  const float tail = std::min(float(pr.age) * 64.0f + 40.0f, 420.0f);
  float px = cx - dx * 40.0f, py = cy - dy * 40.0f;
  for (float s = 12.0f; s <= tail; s += 12.0f)
  {
    const float amp = 7.0f * (s / tail) * std::max(0.2f, 1.0f - float(pr.age) * 0.06f);
    const float wv = std::sin(s * 0.05f - float(pr.age) * 1.3f) * amp;
    const float x = cx - dx * (40.0f + s) - dy * wv, y = cy - dy * (40.0f + s) + dx * wv;
    const float fade = 1.0f - s / tail;
    r.drawLine(px, py, x, y, 4.0f, rgba(40, 34, 24, int(200 * fade)));
    r.drawLine(px, py, x, y, 2.4f, rgba(232, 220, 180, int(255 * fade)));
    px = x;
    py = y;
  }
  DrawOpts o;
  o.angle = std::atan2(dy, dx) * 57.2958f;
  drawGlow(r, art, cx + dx * 30.0f, cy + dy * 30.0f, 26.0f, rgb(200, 240, 255), 0.35f);
  r.draw(baked(art, r, "harpoon", 128, 30, 106, 15, paintHarpoon), cx + dx * 30.0f, cy + dy * 30.0f, o);
  return true;
}

bool drawHarpoonIcon(Renderer& r, const Art& art, float x, float y)
{
  DrawOpts o;
  o.scale = 0.62f;
  r.draw(baked(art, r, "gun", 64, 64, 0, 0, paintGun), x, y, o);
  return true;
}

// --- Behind the runner -------------------------------------------------------------------------------

void World::drawTrenchBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)alpha;
  const auto& t = mTrench;
  const int tx0 = std::max(0, int(camX / kTilePx) - 2), tx1 = std::min(mLevel->width - 1, int((camX + float(kScreenW)) / kTilePx) + 2);
  const int ty0 = std::max(0, int(camY / kTilePx) - 2), ty1 = std::min(mLevel->height - 1, int((camY + float(kScreenH)) / kTilePx) + 6);
  auto solid = [&](int bx, int by) { return mMap.block(bx, by) == Tile::Solid; };
  auto pocketBlock = [&](int bx, int by) { return inPocket(bx * kCellsPerTile + 1, by * kCellsPerTile + 1); };
  // The ship's plating over its tiles (the hold, its deck and keel), and
  // the wreck on the shelf; the station's panels over its walls.
  std::vector<CellBox> hulls, stations;
  for (const auto& d : t.darks)
    hulls.push_back({d.x / kCellsPerTile - 1, d.y / kCellsPerTile - 2, d.w / kCellsPerTile + 2, d.h / kCellsPerTile + 3});
  if (!cablePath(*this).empty())
  {
    // Fiber Trench's shelf wreck, if it is still there.
    bool wreck = true;
    for (int bx = 14; bx <= 22; ++bx)
      wreck = wreck && solid(bx, 10) && solid(bx, 11);
    if (wreck)
      hulls.push_back({14, 10, 9, 2});
  }
  for (const auto& pk : t.pockets)
  {
    bool hold = false;
    for (const auto& d : t.darks)
      hold = hold || d.intersects(pk);
    if (!hold)
      stations.push_back({pk.x / kCellsPerTile - 1, pk.y / kCellsPerTile - 1, pk.w / kCellsPerTile + 2, pk.h / kCellsPerTile + 2});
  }
  auto inAny = [](const std::vector<CellBox>& v, int bx, int by) {
    for (const auto& b : v)
      if (b.contains(bx, by))
        return true;
    return false;
  };
  for (int by = ty0; by <= ty1; ++by)
    for (int bx = tx0; bx <= tx1; ++bx)
    {
      if (!solid(bx, by))
        continue;
      bool eelBox = false;
      for (const auto& eb : t.eels)
        eelBox = eelBox || (eb.bx == bx && eb.by == by);
      if (eelBox)
        continue;
      const float x = float(bx) * kTilePx - camX, y = float(by) * kTilePx - camY;
      const bool top = !solid(bx, by - 1);
      if (inAny(hulls, bx, by))
      {
        r.draw(hullTex(mArt, r, hash2(bx, by) % 9u == 0 ? 2 : ((bx % 6) == 3 ? 1 : 0)), x, y);
        if (top)
        {
          // The deck's edge: a rail of worn steel.
          r.fillRect(x, y, kTilePx, 5.0f, rgb(150, 130, 112));
          r.fillRect(x, y + 5.0f, kTilePx, 3.0f, rgb(40, 32, 30));
        }
      }
      else if (inAny(stations, bx, by))
      {
        r.draw(stationTex(mArt, r, (bx + by) % 5 == 0 ? 1 : 0), x, y);
        if (top)
        {
          // The roof's edge, striped like a safety line.
          for (float sx = 0.0f; sx < kTilePx; sx += 16.0f)
          {
            r.fillRect(x + sx, y, 8.0f, 6.0f, rgb(250, 200, 40));
            r.fillRect(x + sx + 8.0f, y, 8.0f, 6.0f, rgb(30, 30, 36));
          }
        }
      }
    }
  // Life on the rock: kelp in the shallows, worms, anemones and sea pens in
  // the deep, coral in Bubble Up.
  for (int by = std::max(1, ty0); by <= ty1; ++by)
    for (int bx = tx0; bx <= tx1; ++bx)
    {
      if (!solid(bx, by) || inAny(hulls, bx, by) || inAny(stations, bx, by))
        continue;
      const std::uint32_t h = hash2(bx * 7 + 3, by * 13 + 5);
      const float x = float(bx) * kTilePx + 32.0f - camX, y = float(by) * kTilePx - camY;
      const bool openTop = mMap.block(bx, by - 1) == Tile::Empty && !pocketBlock(bx, by - 1);
      if (t.bubble)
      {
        if (openTop && h % 100u < 55u)
        {
          DrawOpts o;
          o.scale = 0.6f + float(h % 5u) * 0.08f;
          o.angle = std::sin(float(frame) * 0.03f + float(bx)) * 2.0f;
          r.draw(coralTex(mArt, r, int((h >> 8) % 4u)), x + float(int(h % 21u) - 10), y + 4.0f, o);
        }
        for (const int side : {-1, 1})
          if (mMap.block(bx + side, by) == Tile::Empty && (h >> (side > 0 ? 3 : 6)) % 100u < 30u)
          {
            DrawOpts o;
            o.scale = 0.5f;
            o.angle = float(side) * 90.0f;
            r.draw(coralTex(mArt, r, int((h >> 12) % 4u)), x + float(side) * 32.0f - float(side) * 3.0f, y + 32.0f, o);
          }
        continue;
      }
      if (!openTop || offScreen(x - 60.0f, y - 340.0f, 120.0f, 340.0f))
        continue;
      bool lined = false;
      for (const auto& a : t.anchors)
        lined = lined || std::abs(bx - a.x / kCellsPerTile) <= 1;
      if (by < 30)
      {
        if (h % 100u < 30u && !lined)
        {
          // Kelp, swaying (and leaning with a current).
          float lean = 0.0f;
          for (const auto& c : t.currents)
            if (c.dx != 0 && c.rect.contains(bx * kCellsPerTile, by * kCellsPerTile - 4))
              lean = float(c.dx) * 7.0f;
          DrawOpts o;
          o.scale = 0.75f + float((h >> 4) % 5u) * 0.1f;
          o.angle = lean + std::sin(float(frame) * 0.022f + float(bx) * 0.7f) * 4.5f;
          o.alpha = 0.96f;
          r.draw(kelpTex(mArt, r, int((h >> 8) % 3u)), x + float(int((h >> 2) % 25u) - 12), y + 6.0f, o);
        }
        else if (h % 100u >= 86u)
        {
          DrawOpts o;
          o.scale = 0.45f;
          r.draw(coralTex(mArt, r, int((h >> 8) % 4u)), x, y + 4.0f, o);
        }
      }
      else if (h % 100u < 24u && !lined)
      {
        const int v = int((h >> 8) % 4u);
        DrawOpts o;
        o.scale = 0.6f + float((h >> 3) % 4u) * 0.08f;
        o.angle = std::sin(float(frame) * 0.03f + float(bx)) * 3.0f;
        if (v == 2)
          drawGlow(r, mArt, x, y - 30.0f, 50.0f, rgb(90, 255, 200), 0.2f + 0.1f * std::sin(float(frame) * 0.07f + float(bx)));
        r.draw(floraTex(mArt, r, v), x + float(int((h >> 2) % 21u) - 10), y + 4.0f, o);
      }
    }
  // The station's depth gauge (the 42).
  for (const auto& pr : mProps)
  {
    if (pr.kind != PropKind::Deco42)
      continue;
    const float cx = float(pr.x) * kCellPx + float(pr.w) * kCellPx * 0.5f - camX;
    const float cy = float(pr.y) * kCellPx + float(pr.h) * kCellPx * 0.5f - camY;
    if (offScreen(cx - 120.0f, cy - 65.0f, 240.0f, 130.0f))
      continue;
    const std::string text = pr.text.empty() ? "42" : pr.text;
    r.draw(baked(mArt, r, "gauge/" + text, 240, 130, 120, 65, [=](cairo_t* cr) { paintGauge(cr, text); }), cx, cy);
    // The needle on 42 of 60, trembling.
    const float a = float(kPi) * 0.75f + 1.5f * float(kPi) * 42.0f / 60.0f + std::sin(float(frame) * 0.9f) * 0.012f;
    const float nx = cx - 120.0f + 70.0f, ny = cy - 65.0f + 65.0f;
    r.drawLine(nx, ny, nx + std::cos(a) * 34.0f, ny + std::sin(a) * 34.0f, 3.0f, rgb(200, 30, 30));
    r.fillRect(nx - 4.0f, ny - 4.0f, 8.0f, 8.0f, rgb(40, 30, 20));
  }
  // The waterproof tape, buoyant against its crevice's ceiling.
  for (const auto& tr : mSignal.treasures)
  {
    if (tr.taken || tr.art != "maxtv_tape_waterproof")
      continue;
    const float x = float(tr.x) * kTilePx - camX, y = float(tr.y) * kTilePx - camY;
    if (offScreen(x, y, kTilePx, kTilePx))
      continue;
    const float bob = std::sin(float(frame) * 0.06f) * 3.0f;
    drawGlow(r, mArt, x + 32.0f, y + 32.0f + bob, 72.0f + 6.0f * std::sin(float(frame) * 0.1f), rgb(255, 210, 90), 0.5f);
    DrawOpts o;
    o.angle = std::sin(float(frame) * 0.045f) * 4.0f;
    r.draw(baked(mArt, r, "tape_waterproof", 96, 96, 16, 16, paintWaterproofTape), x, y + bob, o);
    const int ph = frame % 90;
    if (ph < 16)
    {
      DrawOpts so;
      so.blend = Blend::Add;
      so.tint = rgb(255, 250, 220);
      so.scale = 0.6f * std::sin(float(ph) / 16.0f * float(kPi));
      r.draw(starTex(mArt, r), x + 10.0f + float(hash2(frame / 90, tr.x) % 44u), y + bob + 8.0f + float(hash2(tr.y, frame / 90) % 30u), so);
    }
  }
  // Anchor rings on their posts; one with a harpoon in it glows.
  for (std::size_t i = 0; i < t.anchors.size(); ++i)
  {
    const auto& a = t.anchors[i];
    const float cx = float(a.x + 1) * kCellPx - camX, cy = float(a.y + 1) * kCellPx - camY;
    if (offScreen(cx - 40.0f, cy - 40.0f, 80.0f, 400.0f))
      continue;
    int ground = a.y + 2;
    while (ground < a.y + 16 && !mMap.solid(a.x, ground) && !mMap.solid(a.x + 1, ground))
      ++ground;
    const float gy = float(ground) * kCellPx - camY;
    const Texture& post = baked(mArt, r, "post", 24, 32, 12, 0, paintPost);
    for (float py = cy + 30.0f; py < gy; py += 30.0f)
      r.draw(post, cx, std::min(py, gy - 32.0f));
    bool held = false;
    for (const auto& h : t.harpoons)
      held = held || h.anchor == int(i);
    if (held)
      drawGlow(r, mArt, cx, cy, 60.0f, rgb(200, 240, 255), 0.35f);
    else
      drawGlow(r, mArt, cx, cy, 44.0f, rgb(200, 240, 255), 0.12f + 0.08f * std::sin(float(frame) * 0.08f));
    r.draw(baked(mArt, r, "anchor", 80, 80, 40, 36, paintAnchor), cx, cy);
  }
  // The Cable Eels: a chain of sections from the box's mouth to the head.
  for (const auto& eb : t.eels)
  {
    if (eb.enemy < 0 || std::size_t(eb.enemy) >= mEnemies.size())
      continue;
    const auto& e = mEnemies[std::size_t(eb.enemy)];
    if (!e.alive || eb.ext <= 0)
      continue;
    const float mx = float(eb.bx) * kTilePx + 32.0f + float(eb.fx) * 30.0f - camX;
    const float my = float(eb.by) * kTilePx + 32.0f + float(eb.fy) * 30.0f - camY;
    const float reach = float(eb.ext) * kCellPx + 30.0f;
    if (offScreen(mx - reach, my - reach, reach * 2.0f, reach * 2.0f))
      continue;
    const float fx = float(eb.fx), fy = float(eb.fy);
    const bool hit = e.flash > 0 && (frame / 2) % 2 == 0;
    DrawOpts bo;
    if (hit)
      bo.tint = rgb(255, 200, 200);
    const Sprite& body = styledEnemySprite(mArt, r, mTheme, "cable_eel_body", 0, 0, 2, 2);
    const float headBack = 70.0f;
    for (float s = 0.0f; s <= std::max(0.0f, reach - headBack); s += 10.0f)
    {
      const float wv = std::sin(s * 0.045f - float(frame) * 0.5f) * 7.0f * std::min(1.0f, s / 60.0f);
      r.draw(body.get(1), mx + fx * s - fy * wv, my + fy * s + fx * wv + 32.0f, bo);
    }
    // The head, its snout at the reach.
    const bool open = eb.state == 2;
    const float sx = mx + fx * reach, sy = my + fy * reach;
    const std::string hk = std::string("eel_head") + (open ? "1" : "0");
    const Sprite& head = bakedSprite(mArt, r, hk, 160, 160, 126, 80, [&](cairo_t* cr) {
      drawTrenchArt(cr, mTheme, "cable_eel_head", 96, 96, open ? 1 : 0, 0);
    });
    DrawOpts ho = bo;
    if (eb.fx < 0)
      r.draw(head.get(-1), sx, sy, ho);
    else
    {
      ho.angle = eb.fy < 0 ? -90.0f : (eb.fy > 0 ? 90.0f : 0.0f);
      r.draw(head.get(1), sx, sy, ho);
    }
    if (open)
      drawGlow(r, mArt, sx - fx * 20.0f, sy - fy * 20.0f, 40.0f, rgb(170, 245, 255), 0.5f);
  }
  // Their junction boxes, humming and sparking before a lunge; and the
  // quiet one, which only talks.
  for (const auto& eb : t.eels)
  {
    const float cx = float(eb.bx) * kTilePx + 32.0f - camX, cy = float(eb.by) * kTilePx + 32.0f - camY;
    if (offScreen(cx - 32.0f, cy - 32.0f, 64.0f, 64.0f))
      continue;
    const bool hum = eb.state == 1;
    const bool dead = eb.enemy < 0 || std::size_t(eb.enemy) >= mEnemies.size() || !mEnemies[std::size_t(eb.enemy)].alive;
    const float shake = hum ? float((frame % 3) - 1) * 2.0f : 0.0f;
    const Texture& box = baked(mArt, r, std::string("jbox0") + (hum ? "1" : "0"), 128, 128, 64, 64, [&](cairo_t* cr) {
      drawTrenchArt(cr, mTheme, "junction_box", 64, 64, 0, hum ? 1 : 0);
    });
    DrawOpts o;
    o.angle = eb.fx < 0 ? 180.0f : (eb.fy < 0 ? -90.0f : (eb.fy > 0 ? 90.0f : 0.0f));
    if (dead)
      o.tint = rgb(150, 150, 160);
    r.draw(box, cx + shake, cy, o);
    const float mx = cx + float(eb.fx) * 30.0f, my = cy + float(eb.fy) * 30.0f;
    if (hum)
    {
      drawGlow(r, mArt, cx, cy, 70.0f, rgb(255, 200, 90), 0.25f + 0.15f * float(frame % 4) / 3.0f);
      for (int k = 0; k < 3; ++k)
      {
        // A jagged spark from the mouth.
        float px = mx, py = my;
        for (int s = 0; s < 4; ++s)
        {
          const std::uint32_t hh = hash2(frame * 3 + k, s * 7 + eb.bx);
          const float nx = px + float(eb.fx) * 10.0f + float(int(hh % 25u) - 12) * (eb.fx != 0 ? 0.6f : 1.0f);
          const float ny = py + float(eb.fy) * 10.0f + float(int((hh >> 8) % 25u) - 12) * (eb.fy != 0 ? 0.6f : 1.0f);
          r.drawLine(px, py, nx, ny, 2.5f, rgb(210, 250, 255), Blend::Add);
          px = nx;
          py = ny;
        }
      }
      drawGlow(r, mArt, mx, my, 30.0f, rgb(200, 250, 255), 0.6f);
    }
    else if (eb.state == 2 || eb.state == 3)
      drawGlow(r, mArt, mx, my, 26.0f, rgb(200, 250, 255), 0.3f);
  }
  if (t.quietX >= 0)
  {
    const float cx = float(t.quietX + 1) * kCellPx - camX, cy = float(t.quietY) * kCellPx - camY;
    if (!offScreen(cx - 32.0f, cy - 32.0f, 64.0f, 64.0f))
    {
      const bool talk = t.quietT > 0 && !t.quietSpoke;
      const Texture& box = baked(mArt, r, std::string("jbox1") + (talk ? "1" : "0"), 128, 128, 64, 64, [&](cairo_t* cr) {
        drawTrenchArt(cr, mTheme, "junction_box", 64, 64, 1, talk ? 1 : 0);
      });
      r.draw(box, cx, cy);
      if (talk)
      {
        // Rings of sound from its grille.
        for (int k = 0; k < 3; ++k)
        {
          const float u = std::fmod(float(frame) * 0.03f + float(k) / 3.0f, 1.0f);
          const float rad = 10.0f + u * 40.0f;
          for (int s = -3; s <= 3; ++s)
          {
            const float a0 = float(s) * 0.18f, a1 = float(s + 1) * 0.18f;
            r.drawLine(cx + 18.0f + std::cos(a0) * rad, cy + std::sin(a0) * rad, cx + 18.0f + std::cos(a1) * rad,
              cy + std::sin(a1) * rad, 2.0f, rgba(200, 220, 230, int(160 * (1.0f - u))));
          }
        }
      }
    }
  }
  // The line between the two harpoons, and the harpoons.
  if (t.line)
  {
    const float y = float(t.lineY) * kCellPx - camY + 4.0f;
    const float x0 = float(t.lineX0) * kCellPx - camX, x1 = float(t.lineX1 + 1) * kCellPx - camX;
    // A dip where the runner stands on it.
    float dipX = -1.0f, dip = 0.0f;
    if (lineUnder())
    {
      dipX = (float(mPlayer.x) + 1.5f) * kCellPx - camX;
      dip = 5.0f;
    }
    auto ropeY = [&](float x) {
      if (dipX < 0.0f)
        return y;
      const float u = x < dipX ? (x - x0) / std::max(1.0f, dipX - x0) : (x1 - x) / std::max(1.0f, x1 - dipX);
      return y + dip * std::clamp(u, 0.0f, 1.0f);
    };
    drawGlow(r, mArt, (x0 + x1) * 0.5f, y, (x1 - x0) * 0.5f, rgb(200, 230, 255), 0.06f);
    float px = x0, py = ropeY(x0);
    for (float x = x0 + 16.0f; x < x1 + 15.0f; x += 16.0f)
    {
      const float nx = std::min(x, x1), ny = ropeY(nx);
      r.drawLine(px, py + 2.0f, nx, ny + 2.0f, 8.0f, rgba(20, 16, 12, 150));
      r.drawLine(px, py, nx, ny, 6.0f, rgb(222, 206, 160));
      px = nx;
      py = ny;
    }
    // Its twist.
    for (float x = x0 + 4.0f; x < x1 - 4.0f; x += 9.0f)
    {
      const float ty = ropeY(x);
      r.drawLine(x, ty + 2.5f, x + 5.0f, ty - 2.5f, 1.6f, rgb(150, 132, 96));
    }
    r.drawLine(x0, y - 2.0f, x1, y - 2.0f, 1.4f, rgba(255, 250, 230, 160));
  }
  const Texture& harpoon = baked(mArt, r, "harpoon", 128, 30, 106, 15, paintHarpoon);
  for (std::size_t i = 0; i < t.harpoons.size(); ++i)
  {
    const auto& h = t.harpoons[i];
    float bx = float(h.x) * kCellPx + 16.0f - camX, by = float(h.y) * kCellPx + 16.0f - camY;
    if (offScreen(bx - 140.0f, by - 140.0f, 280.0f, 280.0f))
      continue;
    float angle = 0.0f;
    if (h.anchor >= 0 && std::size_t(h.anchor) < t.anchors.size())
    {
      // Through the ring, from the side the line comes from.
      const auto& a = t.anchors[std::size_t(h.anchor)];
      bx = float(a.x + 1) * kCellPx - camX;
      by = float(a.y + 1) * kCellPx - camY;
      const bool fromRight = t.harpoons.size() == 2 && t.harpoons[1 - i].x > a.x;
      angle = fromRight ? 180.0f : 0.0f;
      bx += fromRight ? -14.0f : 14.0f;
    }
    else if (mMap.solid(h.x, h.y) && !mMap.solid(h.x, h.y - 1) && mMap.solid(h.x - 1, h.y) && mMap.solid(h.x + 1, h.y))
    {
      // Planted at the feet: standing up out of the floor.
      by = float(h.y) * kCellPx + 10.0f - camY;
      angle = 80.0f;
    }
    else if (!mMap.solid(h.x - 1, h.y))
    {
      bx = float(h.x) * kCellPx + 10.0f - camX; // in a wall on the right
      angle = 0.0f;
    }
    else
    {
      bx = float(h.x + 1) * kCellPx - 10.0f - camX; // in a wall on the left
      angle = 180.0f;
    }
    DrawOpts o;
    o.angle = angle;
    r.draw(harpoon, bx, by, o);
  }
}

// --- In front ----------------------------------------------------------------------------------

void World::drawTrenchFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& t = mTrench;
  const auto& p = mPlayer;
  // Box Jellies: glassy bells over a glow, contracting before they rise.
  for (const auto& e : mEnemies)
  {
    if (!e.alive || e.hidden || e.kind != EnemyKind::BoxJelly)
      continue;
    const float x = e.drawX * kCellPx + float(e.w) * kCellPx * 0.5f - camX, y = (e.drawY + 1.0f) * kCellPx - camY;
    if (offScreen(x - 80.0f, y - 160.0f, 160.0f, 160.0f))
      continue;
    int f = 0;
    if (e.tell > 0)
      f = e.tell > 4 ? 1 : 2;
    else if (e.y < e.prevY)
    {
      static constexpr int kPulse[4] = {0, 1, 2, 1};
      f = kPulse[(frame / 5) % 4];
    }
    else
      f = (frame / 24) % 2 ? 3 : 0;
    const Color c = e.carrier ? rgb(120, 255, 120) : rgb(190, 150, 255);
    drawGlow(r, mArt, x, y - float(e.h) * kCellPx * 0.7f, 80.0f, c, e.carrier ? 0.4f : 0.28f);
    DrawOpts o;
    o.alpha = 0.95f;
    if (e.flash > 0 && (frame / 2) % 2 == 0)
      o.tint = rgb(255, 255, 255);
    r.draw(styledEnemySprite(mArt, r, mTheme, "box_jelly", e.carrier ? 1 : 0, f, e.w, e.h).get(1), x, y, o);
    if (e.flash > 0 && (frame / 2) % 2 == 0)
    {
      DrawOpts w;
      w.blend = Blend::Add;
      w.alpha = 0.5f;
      r.draw(styledEnemySprite(mArt, r, mTheme, "box_jelly", e.carrier ? 1 : 0, f, e.w, e.h).get(1), x, y, w);
    }
  }
  // Anglerfish (their lures come after the dark).
  for (const auto& e : mEnemies)
  {
    if (!e.alive || e.hidden || e.kind != EnemyKind::Anglerfish)
      continue;
    const float x = e.drawX * kCellPx + float(e.w) * kCellPx * 0.5f - camX, y = (e.drawY + 1.0f) * kCellPx - camY;
    if (offScreen(x - 140.0f, y - 180.0f, 280.0f, 200.0f))
      continue;
    const int v = e.variant == 1 ? 3 : (e.dive > 0 ? 2 : (e.tell > 0 ? 1 : 0));
    const Sprite& s = styledEnemySprite(mArt, r, mTheme, "anglerfish", v, (frame / 14) % 2, e.w, e.h);
    const float bob = e.variant == 1 ? std::sin(float(frame) * 0.03f) * 4.0f : std::sin(float(frame) * 0.05f + float(e.ox)) * 3.0f;
    r.draw(s.get(e.dir), x, y + bob);
    if (e.flash > 0 && (frame / 2) % 2 == 0)
    {
      DrawOpts w;
      w.blend = Blend::Add;
      w.alpha = 0.55f;
      r.draw(s.get(e.dir), x, y + bob, w);
    }
  }
  // The water's haze, deeper the further down; clear in the air pockets.
  if (t.water)
  {
    const int bx0 = int(std::floor(camX / kTilePx)) - 1, by0 = int(std::floor(camY / kTilePx)) - 1;
    const int cols = int(float(kScreenW) / kTilePx) + 3, rows = int(float(kScreenH) / kTilePx) + 3;
    std::vector<float> haze(std::size_t(cols * rows));
    for (int j = 0; j < rows; ++j)
      for (int i = 0; i < cols; ++i)
      {
        const int bx = bx0 + i, by = by0 + j;
        const bool air = inPocket(bx * kCellsPerTile + 1, by * kCellsPerTile + 1) ||
          !t.waterBox.contains(bx * kCellsPerTile + 1, by * kCellsPerTile + 1);
        haze[std::size_t(j * cols + i)] = air ? 0.0f : 0.07f + 0.2f * depthShade(float(by) + 0.5f, float(mLevel->height));
      }
    r.drawAlphaGrid(haze, cols, rows, float(bx0) * kTilePx - camX, float(by0) * kTilePx - camY, kTilePx, kTilePx, kHaze);
    // The air pockets: their surfaces shimmering where they meet the water.
    for (const auto& pk : t.pockets)
    {
      const float px = float(pk.x) * kCellPx - camX, py = float(pk.y) * kCellPx - camY;
      const float pw = float(pk.w) * kCellPx, ph = float(pk.h) * kCellPx;
      if (offScreen(px, py, pw, ph))
        continue;
      if (!mMap.solid(pk.x + pk.w / 2, pk.y - 1))
      {
        // The surface above: a wobbling line, its sheen under it.
        float lx = px, ly = py + std::sin(float(frame) * 0.1f) * 3.0f;
        r.fillRect(px, py, pw, 14.0f, rgba(220, 245, 255, 26), Blend::Add);
        for (float sx = 12.0f; sx <= pw; sx += 12.0f)
        {
          const float wy = py + std::sin((sx + float(frame) * 5.0f) * 0.045f) * 3.0f + std::sin((sx - float(frame) * 3.0f) * 0.11f) * 1.5f;
          r.drawLine(lx, ly, px + sx, wy, 3.0f, rgba(220, 248, 255, 200));
          lx = px + sx;
          ly = wy;
        }
        for (int k = 0; k < 4; ++k)
        {
          const float sx = std::fmod(float(k) * 173.0f + float(frame) * 1.3f, pw);
          drawGlow(r, mArt, px + sx, py + 2.0f, 16.0f, rgb(255, 255, 255), 0.35f * (0.5f + 0.5f * std::sin(float(frame) * 0.2f + float(k))));
        }
      }
      // Its open sides: a shimmering wall of water.
      for (const int side : {0, 1})
      {
        const int cx = side == 0 ? pk.x - 1 : pk.x + pk.w;
        const float sx = side == 0 ? px : px + pw;
        float y0 = -1.0f;
        for (int cy = pk.y; cy <= pk.y + pk.h; ++cy)
        {
          const bool open = cy < pk.y + pk.h && !mMap.solid(cx, cy) && !inPocket(cx, cy);
          if (open && y0 < 0.0f)
            y0 = float(cy) * kCellPx - camY;
          if (!open && y0 >= 0.0f)
          {
            const float y1 = float(cy) * kCellPx - camY;
            float lx = sx + std::sin(float(frame) * 0.1f) * 2.0f, ly = y0;
            for (float yy = y0 + 12.0f; yy <= y1; yy += 12.0f)
            {
              const float wx = sx + std::sin((yy + float(frame) * 4.0f) * 0.05f) * 2.5f;
              r.drawLine(lx, ly, wx, yy, 2.0f, rgba(210, 245, 255, 120));
              lx = wx;
              ly = yy;
            }
            y0 = -1.0f;
          }
        }
      }
    }
    // Currents: streams of bubbles drifting their way, and faint flow lines.
    const Texture& bub = bubbleTex(mArt, r);
    for (std::size_t i = 0; i < t.currents.size(); ++i)
    {
      const auto& c = t.currents[i];
      const float x0 = float(c.rect.x) * kCellPx - camX, y0 = float(c.rect.y) * kCellPx - camY;
      const float w = float(c.rect.w) * kCellPx, h = float(c.rect.h) * kCellPx;
      if (offScreen(x0, y0, w, h))
        continue;
      const float speed = (c.every == 1 ? 7.0f : 3.5f);
      const int count = int(w * h / 11000.0f) + 6;
      for (int k = 0; k < count; ++k)
      {
        const std::uint32_t s = hash2(k * 7 + 1, int(i) * 31 + 5);
        float bx = float(s % 1000u) / 1000.0f * w, by = float((s >> 10) % 1000u) / 1000.0f * h;
        const float sp = speed * (0.7f + float((s >> 20) % 5u) * 0.12f);
        bx = std::fmod(bx + float(c.dx) * sp * float(frame) + w * 64.0f, w);
        by = std::fmod(by + float(c.dy) * sp * float(frame) + h * 64.0f, h);
        // Wobbling across the flow.
        const float wob = std::sin(float(frame) * 0.15f + float(k)) * 5.0f;
        const float x = x0 + bx + (c.dx == 0 ? wob : 0.0f), y = y0 + by + (c.dy == 0 ? wob : 0.0f);
        // Fading in and out at the edges of the stream.
        const float ex = std::min(bx, w - bx) / 80.0f, ey = std::min(by, h - by) / 80.0f;
        DrawOpts o;
        o.alpha = std::clamp(std::min(ex, ey), 0.0f, 1.0f) * 0.85f;
        o.scale = 0.5f + float((s >> 24) % 5u) * 0.18f;
        r.draw(bub, x, y, o);
      }
      for (int k = 0; k < count / 2; ++k)
      {
        const std::uint32_t s = hash2(k * 13 + 9, int(i) * 17 + 2);
        const float len = 70.0f + float(s % 90u);
        float bx = float((s >> 4) % 1000u) / 1000.0f * w, by = float((s >> 14) % 1000u) / 1000.0f * h;
        bx = std::fmod(bx + float(c.dx) * speed * 1.6f * float(frame) + w * 64.0f, w);
        by = std::fmod(by + float(c.dy) * speed * 1.6f * float(frame) + h * 64.0f, h);
        const float ex = std::min(bx, w - bx) / 120.0f, ey = std::min(by, h - by) / 120.0f;
        const int a = int(std::clamp(std::min(ex, ey), 0.0f, 1.0f) * 34.0f);
        r.drawLine(x0 + bx, y0 + by, x0 + bx - float(c.dx) * len, y0 + by - float(c.dy) * len, 2.0f, rgba(200, 240, 255, a), Blend::Add);
      }
    }
  }
  // The dark: deep shadow, a pool of light round the runner; the cable
  // shows through it, and the alien's glow when it waves.
  const auto& cable = cablePath(*this);
  for (const auto& d : t.darks)
  {
    const int cx0 = std::max(d.x, int(std::floor(camX / kCellPx)) - 1);
    const int cy0 = std::max(d.y, int(std::floor(camY / kCellPx)) - 1);
    const int cx1 = std::min(d.x + d.w - 1, int((camX + float(kScreenW)) / kCellPx) + 1);
    // The shade runs on over the hold's floor (4 cells), fading out.
    const int cy1 = std::min(d.y + d.h + 3, int((camY + float(kScreenH)) / kCellPx) + 1);
    if (cx1 < cx0 || cy1 < cy0)
      continue;
    const int cols = cx1 - cx0 + 1, rows = cy1 - cy0 + 1;
    // The cable's samples near this patch of dark, in cells.
    std::vector<std::array<float, 2>> lit;
    for (std::size_t k = 0; k < cable.size(); k += 3)
    {
      const float ccx = cable[k].x / kCellPx, ccy = cable[k].y / kCellPx;
      if (ccx >= float(cx0 - 4) && ccx <= float(cx1 + 4) && ccy >= float(cy0 - 4) && ccy <= float(cy1 + 4))
        lit.push_back({ccx, ccy});
    }
    const float plx = lerpCellsF(p.prevX, p.x, alpha) + 1.5f, ply = lerpCellsF(p.prevY, p.y, alpha) - 2.0f;
    std::vector<float> shade(std::size_t(cols * rows));
    for (int j = 0; j < rows; ++j)
      for (int i = 0; i < cols; ++i)
      {
        const float gx = float(cx0 + i) + 0.5f, gy = float(cy0 + j) + 0.5f;
        const float dd = std::hypot(gx - plx, gy - ply);
        float s = smooth((dd - 3.0f) / 6.5f) * 0.95f;
        for (const auto& c : lit)
        {
          const float cd = std::hypot(gx - c[0], gy - c[1]);
          if (cd < 3.5f)
            s = std::min(s, 0.94f * (0.45f + 0.55f * smooth(cd / 3.5f)));
        }
        if (t.wave > 0 && t.portX >= 0)
        {
          const float ad = std::hypot(gx - float(t.portX + 1), gy - float(t.portY));
          s = std::min(s, 0.94f * (0.4f + 0.6f * smooth(ad / 5.0f)));
        }
        const int below = cy0 + j - (d.y + d.h - 1);
        if (below > 0)
          s *= below <= 2 ? 0.9f : 0.5f;
        shade[std::size_t(j * cols + i)] = s;
      }
    r.drawAlphaGrid(shade, cols, rows, float(cx0) * kCellPx - camX, float(cy0) * kCellPx - camY, kCellPx, kCellPx, rgb(1, 3, 10));
    // A faint warm light on the runner, as if from a head lamp.
    const float sx = plx * kCellPx - camX, sy = ply * kCellPx - camY;
    if (d.contains(int(plx), int(ply)))
      drawGlow(r, mArt, sx + float(p.facing) * 30.0f, sy - 10.0f, 110.0f, rgb(255, 230, 190), 0.08f);
  }
  // In the dark only the lure shows: a jewel of light pulsing (60 to 100%
  // over 30 frames); on the tell it holds still and two eyes glint.
  for (const auto& e : mEnemies)
  {
    if (!e.alive || e.hidden || e.kind != EnemyKind::Anglerfish)
      continue;
    const float x = e.drawX * kCellPx + float(e.w) * kCellPx * 0.5f - camX, y = (e.drawY + 1.0f) * kCellPx - camY;
    const float bob = e.variant == 1 ? std::sin(float(frame) * 0.03f) * 4.0f : std::sin(float(frame) * 0.05f + float(e.ox)) * 3.0f;
    const float lx = x + kAnglerLureX * float(e.dir), ly = y + kAnglerLureY + bob;
    if (offScreen(lx - 60.0f, ly - 60.0f, 120.0f, 120.0f))
      continue;
    const bool pulsing = e.tell == 0 && e.dive == 0;
    const float a = pulsing ? 0.6f + 0.4f * float(frame % 30) / 30.0f : 1.0f;
    const Color c = e.variant == 1 ? rgb(170, 255, 200) : rgb(120, 255, 230);
    drawGlow(r, mArt, lx, ly, 90.0f, c, 0.3f * a);
    DrawOpts o;
    o.blend = Blend::Add;
    o.tint = c;
    o.alpha = a;
    o.angle = std::sin(float(frame) * 0.05f + float(e.ox)) * 10.0f;
    r.draw(lureTex(mArt, r), lx, ly, o);
    if (!pulsing && e.variant != 1)
    {
      const float ex = x + kAnglerEyeX * float(e.dir), ey = y + kAnglerEyeY + bob;
      for (int k = 0; k < 2; ++k)
      {
        DrawOpts g;
        g.blend = Blend::Add;
        g.tint = rgb(255, 240, 170);
        g.scale = 0.75f + 0.25f * float((frame / 2) % 2);
        g.angle = float(frame * 6 % 90);
        r.draw(starTex(mArt, r), ex - float(k) * 16.0f * float(e.dir), ey - float(k) * 3.0f, g);
      }
    }
  }
  // Real gems sparkle in the dark (a 4-frame star every 20 frames).
  for (std::size_t i = 0; i < mItems.size(); ++i)
  {
    const auto& it = mItems[i];
    if (it.kind != ItemKind::Gem)
      continue;
    bool dark = false;
    for (const auto& d : t.darks)
      dark = dark || d.contains(it.x + 1, it.y - 1);
    const int ph = (frame + int(i) * 7) % 20;
    if (!dark || ph >= 4)
      continue;
    const float x = float(it.x) * kCellPx + 32.0f - camX, y = float(it.y - 1) * kCellPx + 8.0f - camY;
    DrawOpts g;
    g.blend = Blend::Add;
    g.tint = mArt.gemColor[std::size_t(it.variant % 4)];
    g.scale = 0.4f + 0.3f * std::sin(float(ph + 1) / 5.0f * float(kPi));
    r.draw(starTex(mArt, r), x, y, g);
    drawGlow(r, mArt, x, y + 20.0f, 30.0f, g.tint, 0.3f);
  }
  // The trench's rock fading into the abyss.
  for (const auto& a : t.abyss)
  {
    const int bx0 = a.x / kCellsPerTile - 4, bx1 = (a.x + a.w) / kCellsPerTile + 4;
    const int by0 = a.y / kCellsPerTile - 8, by1 = (a.y + a.h) / kCellsPerTile;
    const int cols = bx1 - bx0 + 1, rows = by1 - by0 + 1;
    if (offScreen(float(bx0) * kTilePx - camX, float(by0) * kTilePx - camY, float(cols) * kTilePx, float(rows) * kTilePx))
      continue;
    std::vector<float> fadeRock(std::size_t(cols * rows));
    for (int j = 0; j < rows; ++j)
      for (int i = 0; i < cols; ++i)
        fadeRock[std::size_t(j * cols + i)] =
          mMap.block(bx0 + i, by0 + j) == Tile::Solid ? smooth(float(j) / float(rows - 1)) * 0.85f : 0.0f;
    r.drawAlphaGrid(fadeRock, cols, rows, float(bx0) * kTilePx - camX, float(by0) * kTilePx - camY, kTilePx, kTilePx, kDeep);
  }
  // The abyss's return: a fade from black.
  if (t.fade > 0)
    r.fillRect(0.0f, 0.0f, float(kScreenW), float(kScreenH), rgba(0, 4, 12, t.fade * 255 / kAbyssFade));
  // Bubble Up: the rings, and the bubble round the runner.
  if (t.bubble)
  {
    const Texture& hoop = baked(mArt, r, "hoop", 400, 120, 200, 60, paintHoop);
    for (int row : t.rings)
    {
      const float y = float(row) * kCellPx - camY;
      if (y < -80.0f || y > float(kScreenH) + 80.0f)
        continue;
      const bool passed = row >= t.ringY;
      const float cx = float(mLevel->width) * kTilePx * 0.5f - camX;
      // A curtain of little bubbles right across the shaft.
      int left = row / kCellsPerTile, x0 = mLevel->width / 2, x1 = mLevel->width / 2;
      while (x0 > 0 && mMap.block(x0 - 1, left) != Tile::Solid)
        --x0;
      while (x1 < mLevel->width - 1 && mMap.block(x1 + 1, left) != Tile::Solid)
        ++x1;
      const Texture& bub = bubbleTex(mArt, r);
      for (float bx = float(x0) * kTilePx + 8.0f; bx < float(x1 + 1) * kTilePx - 8.0f; bx += 22.0f)
      {
        DrawOpts o;
        o.alpha = passed ? 0.5f : 0.8f;
        o.scale = 0.6f + 0.2f * std::sin(bx * 0.3f);
        o.tint = passed ? rgb(150, 255, 210) : rgb(255, 255, 255);
        r.draw(bub, bx - camX, y + std::sin(bx * 0.05f + float(frame) * 0.1f) * 4.0f, o);
      }
      DrawOpts o;
      o.blend = Blend::Add;
      o.tint = passed ? rgb(120, 255, 200) : rgb(220, 240, 255);
      o.alpha = passed ? 0.75f : 0.55f + 0.2f * std::sin(float(frame) * 0.1f);
      o.scale = 0.55f;
      r.draw(hoop, cx, y, o);
    }
    if (p.state != PlayerState::Dying)
    {
      const float cx = (lerpCellsF(p.prevX, p.x, alpha) + 1.5f) * kCellPx - camX;
      const float cy = (lerpCellsF(p.prevY, p.y, alpha) - 1.8f) * kCellPx - camY;
      const Texture& bubble = baked(mArt, r, "runner_bubble", 240, 240, 120, 120, paintRunnerBubble);
      DrawOpts o;
      if (t.pop > 0)
      {
        // Popped: it forms again round the runner at the ring.
        const float u = 1.0f - float(t.pop) / 15.0f;
        o.scale = 0.3f + 0.6f * smooth(u);
        o.alpha = u;
      }
      else
      {
        o.scale = 0.92f + std::sin(float(frame) * 0.25f) * 0.02f;
        o.angle = std::sin(float(frame) * 0.05f) * 8.0f;
      }
      r.draw(bubble, cx, cy, o);
    }
  }
  // A few motes close to the glass, drifting past faster.
  if (t.water || t.bubble)
    for (int i = 0; i < 10; ++i)
    {
      const std::uint32_t h = hash2(i * 53 + 11, i * 29 + 7);
      const float x = std::fmod(float(h % 1800u) - camX * 1.25f + 18000.0f, 1800.0f) - 260.0f;
      const float y = std::fmod(float((h >> 11) % 1200u) - camY * 1.25f + float(frame) * (t.bubble ? -0.9f : 0.3f) + 12000.0f, 1200.0f) - 240.0f;
      DrawOpts o;
      o.blend = Blend::Add;
      o.tint = rgb(190, 235, 255);
      o.scale = 0.7f + float((h >> 5) % 4u) * 0.15f;
      o.alpha = 0.1f + float((h >> 9) % 4u) * 0.04f;
      r.draw(mArt.dot, x, y, o);
    }
}

void World::drawTrenchHud(Renderer& r, int frame) const
{
  const auto& t = mTrench;
  const auto& p = mPlayer;
  // The STROKES panel under the score while swimming (not in Turbo, when
  // strokes are unlimited): a bubble for each stroke left.
  if (!t.water || !underwater() || p.turbo > 0)
    return;
  const int max = strokesMax();
  const int left = std::max(0, max - t.strokes);
  const float x = float(12 + kHudHealthW + 10 + kHudWeaponW + 10 + kHudInventoryW + 10 + kHudLettersW + 10);
  const float y = 10.0f + float(kHudPanelH) + 10.0f;
  r.draw(mArt.hudPanels[4], x, y);
  const bool out = left == 0;
  r.drawText("STROKES", x + 16, y + 6, {15.0f, out && (frame / 8) % 2 ? rgb(255, 200, 90) : kHudLabel, kHudInk});
  const float gap = 40.0f, px0 = x + float(kHudScoreW) * 0.5f - gap * float(max - 1) * 0.5f;
  for (int k = 0; k < max; ++k)
  {
    const bool full = k < left;
    DrawOpts o;
    if (full && k == left - 1)
      o.scale = 1.0f + 0.06f * std::sin(float(frame) * 0.2f); // the next to go
    if (!full && out)
      o.alpha = 0.5f + 0.5f * float((frame / 8) % 2);
    r.draw(pipTex(mArt, r, full), px0 + float(k) * gap, y + 42.0f, o);
  }
}

} // namespace gr
