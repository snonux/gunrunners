// Level 18's drawing (world_hull.cpp has the rules): the hull's scenery (the
// antenna mast, the dish, truss towers, solar wings, airlocks, the crashed
// UFO, girders), the rivet plates and the mites working them loose, the
// drifting camera's thruster frame, planted flags, the UFO's alien, the
// Recoil Cannon's blast and the LOW G badge; and the Planetoids bonus
// (world_orbit.cpp): the little worlds, their fields and the ship parts.
// The pieces are baked with Cairo the first time they are drawn and kept in
// the Art's sprite cache.

#include "game/world.hpp"

#include "assets/art.hpp"
#include "assets/enemy_art.hpp"
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
constexpr Color kInk = rgb(18, 20, 32);
constexpr Color kSteel = rgb(176, 188, 204);
constexpr Color kSteelLight = rgb(236, 242, 250);
constexpr Color kSteelDark = rgb(64, 74, 94);
constexpr Color kSpace = rgb(6, 10, 26);
constexpr Color kSun = rgb(255, 170, 70);
constexpr Color kAlienGreen = rgb(120, 255, 170);

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
  const std::string id = "~hull/" + key;
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

void line(cairo_t* cr, double x0, double y0, double x1, double y1, double width, Color c, double a = 1.0)
{
  cairo_move_to(cr, x0, y0);
  cairo_line_to(cr, x1, y1);
  setRgba(cr, c, a);
  cairo_set_line_width(cr, width);
  cairo_stroke(cr);
}

void bolt(cairo_t* cr, double x, double y, double rad)
{
  cairo_arc(cr, x, y, rad, 0, 2 * kPi);
  cairo_pattern_t* p = cairo_pattern_create_radial(x - rad * 0.4, y - rad * 0.4, 0.2, x, y, rad);
  cairo_pattern_add_color_stop_rgb(p, 0, 1, 1, 1);
  cairo_pattern_add_color_stop_rgb(p, 1, 0.45, 0.5, 0.6);
  cairo_set_source(cr, p);
  cairo_fill(cr);
  cairo_pattern_destroy(p);
}

// A stretch of steel lattice: two chords and X bracing, `w` wide from y0 to
// y1, in px.
void lattice(cairo_t* cr, double x, double y0, double w, double y1, double bay, double chord)
{
  for (int pass = 0; pass < 2; ++pass)
  {
    const Color c = pass == 0 ? kSteelDark : kSteel;
    const double k = pass == 0 ? 1.0 : 0.45;
    for (double y = y0; y < y1 - 1; y += bay)
    {
      const double yb = std::min(y1, y + bay);
      line(cr, x + chord * 0.5, y, x + w - chord * 0.5, yb, chord * 0.55 * k + (pass == 0 ? 2 : 0), c);
      line(cr, x + w - chord * 0.5, y, x + chord * 0.5, yb, chord * 0.55 * k + (pass == 0 ? 2 : 0), c);
      line(cr, x, y, x + w, y, chord * 0.7 * k + (pass == 0 ? 2 : 0), c);
    }
    for (const double cx : {x + chord * 0.5, x + w - chord * 0.5})
      line(cr, cx, y0, cx, y1, chord * k + (pass == 0 ? 2 : 0), c);
  }
  // Light catching the chords' sunward edge.
  line(cr, x + w - chord * 0.2, y0, x + w - chord * 0.2, y1, 1.2, kSun, 0.6);
}

// --- Scenery (anchored at the rect's top-left) ---------------------------------

// The antenna mast: a lattice pole, a crossarm with whip antennas, and the
// lamp housing on top (its blink is drawn live), w x h px. Where the mast's
// rect is solid (from `deck` down) it stands on a full-width lattice tower.
void paintMast(cairo_t* cr, double w, double h, double deck)
{
  const double cx = w * 0.5, pw = 22;
  if (deck < h)
  {
    cairo_rectangle(cr, 0, deck, w, h - deck);
    linearFill(cr, 0, 0, w, 0, rgb(30, 36, 54), rgb(46, 54, 76));
    lattice(cr, 0, deck + 10, w, h, 48, 8);
    roundedRect(cr, -1, deck, w + 2, 12, 3);
    linearFill(cr, 0, deck, 0, deck + 12, kSteelLight, kSteel, true);
    outline(cr, 1.8);
    line(cr, 0, deck + 0.8, w, deck + 0.8, 1.6, kSun, 0.8);
  }
  const double foot = std::min(h, deck);
  if (foot > 40)
    lattice(cr, cx - pw * 0.5, 34, pw, foot - 6, 22, 4);
  // A foot plate bolted down.
  roundedRect(cr, cx - 18, foot - 8, 36, 8, 3);
  linearFill(cr, 0, foot - 8, 0, foot, kSteelLight, kSteelDark, true);
  outline(cr, 1.6);
  // The crossarm and whips.
  roundedRect(cr, cx - 30, 36, 60, 7, 2);
  linearFill(cr, 0, 36, 0, 43, kSteelLight, kSteelDark, true);
  outline(cr, 1.4);
  for (const double ax : {cx - 26, cx + 26})
  {
    line(cr, ax, 36, ax, 8, 2.0, kSteelDark);
    cairo_arc(cr, ax, 8, 2.4, 0, 2 * kPi);
    setColor(cr, kSteelLight);
    cairo_fill(cr);
  }
  // The lamp housing.
  roundedRect(cr, cx - 9, 14, 18, 22, 4);
  linearFill(cr, 0, 14, 0, 36, kSteelLight, kSteelDark, true);
  outline(cr, 1.6);
  cairo_arc(cr, cx, 14, 6, kPi, 2 * kPi);
  setColor(cr, rgb(120, 30, 30));
  cairo_fill(cr);
}

// The dish: an equipment housing filling the solid rows (from `deck` down,
// walkable on top) and the dish above it, tilted up toward the rising sun,
// with its feed on three struts. w x h px.
void paintDish(cairo_t* cr, double w, double h, double deck)
{
  if (deck >= h - 8)
    deck = h * 0.5;
  const double cx = w * 0.5;
  // The bowl's mount: a yoke rising from the housing.
  cairo_move_to(cr, cx - 40, deck + 2);
  cairo_line_to(cr, cx - 14, deck * 0.55);
  cairo_line_to(cr, cx + 14, deck * 0.55);
  cairo_line_to(cr, cx + 40, deck + 2);
  cairo_close_path(cr);
  linearFill(cr, 0, deck * 0.55, 0, deck, kSteel, kSteelDark, true);
  outline(cr, 2);
  // The bowl: an ellipse tilted to the upper right, its back shell below.
  const double by = deck * 0.46, rx = w * 0.44, ry = std::min(deck * 0.36, rx * 0.5);
  cairo_save(cr);
  cairo_translate(cr, cx, by);
  cairo_rotate(cr, -0.22);
  cairo_move_to(cr, -rx, 0);
  cairo_curve_to(cr, -rx * 0.8, ry * 1.6, rx * 0.8, ry * 1.6, rx, 0);
  cairo_close_path(cr);
  linearFill(cr, 0, 0, 0, ry * 1.4, rgb(214, 220, 232), rgb(110, 120, 140), true);
  outline(cr, 2.4);
  cairo_save(cr);
  cairo_scale(cr, rx, ry);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_restore(cr);
  cairo_pattern_t* g = cairo_pattern_create_linear(-rx, -ry, rx, ry);
  cairo_pattern_add_color_stop_rgb(g, 0, 0.6, 0.65, 0.76);
  cairo_pattern_add_color_stop_rgb(g, 0.6, 0.95, 0.96, 0.99);
  cairo_pattern_add_color_stop_rgb(g, 1, 1.0, 0.85, 0.66);
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  outline(cr, 2.4);
  for (int k = 1; k <= 3; ++k)
  {
    cairo_save(cr);
    cairo_scale(cr, rx * k / 4.0, ry * k / 4.0);
    cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
    cairo_restore(cr);
    setRgba(cr, kSteelDark, 0.4);
    cairo_set_line_width(cr, 1.4);
    cairo_stroke(cr);
  }
  // The feed on its struts.
  const double fy = -ry * 1.7;
  for (const double sx : {-rx * 0.55, rx * 0.55, 0.0})
    line(cr, sx, sx == 0.0 ? ry * 0.6 : 0.0, 0, fy, 2.2, kSteelDark);
  roundedRect(cr, -6, fy - 8, 12, 14, 3);
  linearFill(cr, 0, fy - 8, 0, fy + 6, kSteelLight, kSteelDark, true);
  outline(cr, 1.6);
  cairo_restore(cr);
  // The housing, filling the solid rows.
  roundedRect(cr, 0, deck, w, h - deck, 6);
  linearFill(cr, 0, deck, 0, h, rgb(220, 226, 236), rgb(120, 130, 148), true);
  outline(cr, 2.2);
  line(cr, 2, deck + 1, w - 2, deck + 1, 1.6, kSun, 0.8);
  for (double x = 34; x < w - 20; x += 64)
  {
    roundedRect(cr, x - 22, deck + 14, 44, std::max(8.0, h - deck - 40), 4);
    setRgba(cr, kSteelDark, 0.35);
    cairo_set_line_width(cr, 2);
    cairo_stroke(cr);
  }
  if (h - deck > 40)
  {
    cairo_save(cr);
    cairo_rectangle(cr, 2, h - 20, w - 4, 12);
    cairo_clip(cr);
    setColor(cr, rgb(255, 196, 40));
    cairo_paint(cr);
    for (double x = -20; x < w + 20; x += 20)
    {
      cairo_move_to(cr, x, h - 8);
      cairo_line_to(cr, x + 10, h - 8);
      cairo_line_to(cr, x + 22, h - 20);
      cairo_line_to(cr, x + 12, h - 20);
      cairo_close_path(cr);
    }
    setColor(cr, rgb(34, 34, 40));
    cairo_fill(cr);
    cairo_restore(cr);
  }
}

// A truss tower: a dark box frame with lattice faces and a capped top.
void paintTruss(cairo_t* cr, double w, double h)
{
  cairo_rectangle(cr, 0, 0, w, h);
  linearFill(cr, 0, 0, w, 0, rgb(30, 36, 54), rgb(46, 54, 76));
  // The far face's lattice, dim, then the near face.
  cairo_save(cr);
  cairo_translate(cr, 10, 12);
  for (double y = 0; y < h; y += 64)
  {
    line(cr, 0, y, w - 20, y + 64, 3, rgb(60, 70, 96));
    line(cr, w - 20, y, 0, y + 64, 3, rgb(60, 70, 96));
  }
  cairo_restore(cr);
  lattice(cr, 0, 10, w, h, 64, 10);
  // The cap: a deck plate with a hazard edge.
  roundedRect(cr, -2, 0, w + 4, 12, 3);
  linearFill(cr, 0, 0, 0, 12, kSteelLight, kSteel, true);
  outline(cr, 1.8);
  line(cr, 0, 0.8, w, 0.8, 1.6, kSun, 0.8);
  cairo_save(cr);
  cairo_rectangle(cr, 0, 12, w, 6);
  cairo_clip(cr);
  setColor(cr, rgb(255, 196, 40));
  cairo_paint(cr);
  for (double x = -12; x < w + 12; x += 12)
  {
    cairo_move_to(cr, x, 18);
    cairo_line_to(cr, x + 6, 18);
    cairo_line_to(cr, x + 12, 12);
    cairo_line_to(cr, x + 6, 12);
    cairo_close_path(cr);
  }
  setColor(cr, rgb(34, 34, 40));
  cairo_fill(cr);
  cairo_restore(cr);
}

// A solar wing: blue cells in a thin gold-edged frame on its boom, filling
// its block (it is solid to stand on). w x h px.
void paintWing(cairo_t* cr, double w, double h)
{
  const double face = h - 16;
  // The boom under the cells, with hinge brackets.
  roundedRect(cr, 0, face + 2, w, h - face - 3, 3);
  linearFill(cr, 0, face + 2, 0, h, kSteel, kSteelDark, true);
  outline(cr, 1.4);
  for (double x = 32; x < w; x += 64)
  {
    roundedRect(cr, x - 6, face + 4, 12, h - face - 7, 2);
    setColor(cr, kSteelLight);
    cairo_fill(cr);
  }
  // The cell face.
  cairo_rectangle(cr, 1, 2, w - 2, face - 2);
  linearFill(cr, 0, 2, 0, face, rgb(70, 120, 220), rgb(20, 40, 110), true);
  outline(cr, 2.0, rgb(30, 30, 50));
  const int cols = std::max(1, int(w / 16)), rows = 3;
  for (int c = 1; c < cols; ++c)
    line(cr, 1 + (w - 2) * c / cols, 2, 1 + (w - 2) * c / cols, face, c % 4 == 0 ? 2.0 : 1.0, rgb(200, 210, 240), 0.55);
  for (int k = 1; k < rows; ++k)
    line(cr, 1, 2 + (face - 2) * k / rows, w - 1, 2 + (face - 2) * k / rows, 1.0, rgb(200, 210, 240), 0.55);
  // Glints of the sunrise slanting across the cells.
  for (double x = 30; x < w; x += 190)
  {
    cairo_move_to(cr, x, 2);
    cairo_line_to(cr, x + 26, 2);
    cairo_line_to(cr, x + 6, face);
    cairo_line_to(cr, x - 20, face);
    cairo_close_path(cr);
    setRgba(cr, rgb(255, 220, 190), 0.18);
    cairo_fill(cr);
  }
  // The frame's edge.
  cairo_rectangle(cr, 0, 0, w, 3);
  setColor(cr, rgb(230, 190, 90));
  cairo_fill(cr);
  cairo_rectangle(cr, 0, face - 1, w, 3);
  setColor(cr, rgb(150, 110, 40));
  cairo_fill(cr);
}

// An airlock: a heavy hazard-banded frame round a door with a round window.
// sign42: the stencil AIRLOCK 42 on a painted plate.
void paintAirlock(cairo_t* cr, double w, double h)
{
  roundedRect(cr, 0, 0, w, h, 14);
  linearFill(cr, 0, 0, w, 0, kSteelLight, rgb(120, 130, 150), true);
  outline(cr, 2.4);
  // Hazard band round the opening.
  const double in = 14;
  cairo_save(cr);
  roundedRect(cr, in, in, w - 2 * in, h - in, 10);
  cairo_clip(cr);
  setColor(cr, rgb(255, 196, 40));
  cairo_paint(cr);
  for (double x = -h; x < w + h; x += 22)
  {
    cairo_move_to(cr, x, h);
    cairo_line_to(cr, x + 11, h);
    cairo_line_to(cr, x + 11 + h, 0);
    cairo_line_to(cr, x + h, 0);
    cairo_close_path(cr);
  }
  setColor(cr, rgb(34, 34, 40));
  cairo_fill(cr);
  cairo_restore(cr);
  // The door.
  const double dx = in + 10, dy = in + 10, dw = w - 2 * dx, dh = h - dy;
  roundedRect(cr, dx, dy, dw, dh, 8);
  linearFill(cr, 0, dy, 0, h, rgb(206, 214, 226), rgb(120, 130, 150), true);
  outline(cr, 2.2);
  line(cr, dx + dw * 0.5, dy + 4, dx + dw * 0.5, h - 2, 2.0, kSteelDark, 0.6);
  // The window: warm light from inside.
  const double wx = dx + dw * 0.5, wy = dy + std::min(dh * 0.3, 60.0), wr = std::min(dw * 0.26, 26.0);
  radialGlow(cr, wx, wy, wr * 2.2, rgb(255, 210, 140), 0.5);
  cairo_arc(cr, wx, wy, wr + 5, 0, 2 * kPi);
  setColor(cr, kSteelDark);
  cairo_fill(cr);
  cairo_arc(cr, wx, wy, wr, 0, 2 * kPi);
  linearFill(cr, wx - wr, wy - wr, wx + wr, wy + wr, rgb(255, 236, 190), rgb(220, 140, 70), true);
  outline(cr, 1.6);
  line(cr, wx - wr * 0.5, wy - wr * 0.3, wx - wr * 0.1, wy - wr * 0.7, 3, rgb(255, 255, 255), 0.7);
  // The handwheel.
  const double hx = wx, hy = wy + wr + std::min(dh * 0.25, 46.0), hr = std::min(dw * 0.2, 18.0);
  cairo_arc(cr, hx, hy, hr, 0, 2 * kPi);
  outline(cr, 4, kSteelDark);
  for (int k = 0; k < 3; ++k)
    line(cr, hx + std::cos(k * kPi / 3) * hr, hy + std::sin(k * kPi / 3) * hr, hx - std::cos(k * kPi / 3) * hr,
      hy - std::sin(k * kPi / 3) * hr, 3, kSteelDark);
  bolt(cr, hx, hy, 4);
}

void paintSign42(cairo_t* cr, double w, double h)
{
  roundedRect(cr, 2, 2, w - 4, h - 4, 6);
  linearFill(cr, 0, 2, 0, h - 2, rgb(250, 210, 70), rgb(220, 160, 30), true);
  outline(cr, 2.2);
  for (const double bx : {10.0, w - 10})
    for (const double by : {10.0, h - 10})
      bolt(cr, bx, by, 2.4);
  // The stencil: blocky letters with the bridges a stencil leaves.
  selectGameFont(cr);
  const char* text = "AIRLOCK 42";
  double size = std::min(h * 0.55, (w - 30) / 6.4);
  cairo_set_font_size(cr, size);
  cairo_text_extents_t ext;
  cairo_text_extents(cr, text, &ext);
  const double tx = (w - ext.width) * 0.5 - ext.x_bearing, ty = (h - ext.height) * 0.5 - ext.y_bearing;
  cairo_move_to(cr, tx, ty);
  cairo_text_path(cr, text);
  setColor(cr, rgb(30, 30, 36));
  cairo_fill(cr);
  for (int k = 0; k < 10; ++k)
  {
    const double x = tx + ext.width * (k + 0.5) / 10.0;
    cairo_rectangle(cr, x - 1, ty + ext.y_bearing + ext.height * 0.42, 2.4, ext.height * 0.14);
  }
  setColor(cr, rgb(240, 196, 60));
  cairo_fill(cr);
  // Scuffs.
  Rng rng(42u);
  for (int i = 0; i < 6; ++i)
  {
    const double x = rng.range(8, float(w) - 8), y = rng.range(6, float(h) - 6);
    line(cr, x, y, x + rng.range(4, 14), y + rng.range(-2, 2), 1.2, rgb(255, 245, 210), 0.5);
  }
}

// The crashed UFO's dome: a cracked glass bubble over the saucer's rim,
// which sticks out of the hull at a tilt. `hole` (px, -1 if none) is the
// open hatch shaft cut out of it.
void paintUfo(cairo_t* cr, double w, double h, double holeX0, double holeX1, double holeY0, int seed)
{
  const double cx = w * 0.5, rimY = h - 26;
  // The dome, full to its rect's corners (they are solid).
  cairo_move_to(cr, 2, rimY);
  cairo_curve_to(cr, 0, h * 0.12, w * 0.08, 0, w * 0.25, 0);
  cairo_line_to(cr, w * 0.75, 0);
  cairo_curve_to(cr, w * 0.92, 0, w, h * 0.12, w - 2, rimY);
  cairo_close_path(cr);
  cairo_pattern_t* g = cairo_pattern_create_radial(w * 0.62, h * 0.3, 4, cx, h * 0.6, w * 0.6);
  cairo_pattern_add_color_stop_rgba(g, 0, 0.75, 1.0, 0.86, 0.95);
  cairo_pattern_add_color_stop_rgba(g, 0.5, 0.2, 0.55, 0.5, 0.95);
  cairo_pattern_add_color_stop_rgba(g, 1, 0.05, 0.16, 0.2, 1.0);
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  outline(cr, 2.6);
  // Inside: the little pilot's empty seat and console.
  roundedRect(cr, cx - 70, rimY - 46, 26, 40, 8);
  setRgba(cr, rgb(10, 40, 40), 0.6);
  cairo_fill(cr);
  roundedRect(cr, cx + 30, rimY - 30, 60, 24, 6);
  setRgba(cr, rgb(10, 40, 40), 0.6);
  cairo_fill(cr);
  for (int k = 0; k < 4; ++k)
  {
    cairo_arc(cr, cx + 40 + k * 13, rimY - 20, 3, 0, 2 * kPi);
    setColor(cr, k % 2 ? rgb(255, 120, 200) : kAlienGreen);
    cairo_fill(cr);
  }
  // Cracks in the glass.
  Rng rng(std::uint32_t(seed * 3 + 1));
  for (int c = 0; c < 2; ++c)
  {
    double x = c ? w * 0.78 : w * 0.18, y = c ? h * 0.3 : h * 0.4;
    cairo_move_to(cr, x, y);
    for (int k = 0; k < 5; ++k)
    {
      x += rng.range(-14, 14);
      y += rng.range(6, 16);
      cairo_line_to(cr, x, y);
    }
    setRgba(cr, rgb(230, 255, 245), 0.8);
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);
  }
  // Reflections.
  cairo_move_to(cr, w * 0.2, h * 0.36);
  cairo_curve_to(cr, w * 0.24, h * 0.16, w * 0.34, h * 0.07, w * 0.44, h * 0.05);
  setRgba(cr, rgb(255, 255, 255), 0.75);
  cairo_set_line_width(cr, 6);
  cairo_stroke(cr);
  cairo_move_to(cr, w * 0.74, h * 0.12);
  cairo_curve_to(cr, w * 0.82, h * 0.2, w * 0.86, h * 0.3, w * 0.87, h * 0.4);
  setRgba(cr, rgb(255, 230, 190), 0.5);
  cairo_set_line_width(cr, 3);
  cairo_stroke(cr);
  // The saucer's rim, tilted, half in the hull, its lights mostly dead.
  cairo_save(cr);
  cairo_translate(cr, cx, rimY + 8);
  cairo_rotate(cr, -0.05);
  cairo_save(cr);
  cairo_scale(cr, w * 0.5 + 4, 18);
  cairo_arc(cr, 0, 0, 1, kPi, 2 * kPi);
  cairo_restore(cr);
  cairo_line_to(cr, w * 0.5 + 4, 12);
  cairo_line_to(cr, -w * 0.5 - 4, 12);
  cairo_close_path(cr);
  linearFill(cr, 0, -18, 0, 12, rgb(226, 236, 240), rgb(100, 116, 130), true);
  outline(cr, 2.4);
  for (int k = 0; k < 9; ++k)
  {
    const double lx = -w * 0.4 + k * w * 0.1;
    cairo_arc(cr, lx, -2, 4, 0, 2 * kPi);
    setColor(cr, k == 2 || k == 6 ? rgb(255, 240, 140) : rgb(60, 70, 70));
    cairo_fill(cr);
  }
  cairo_restore(cr);
  // Dents and scorch where it hit.
  radialGlow(cr, w * 0.15, h - 10, 40, rgb(20, 16, 10), 0.6);
  radialGlow(cr, w * 0.9, h - 8, 30, rgb(20, 16, 10), 0.5);
  // The open hatch shaft.
  if (holeX0 >= 0)
  {
    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_rectangle(cr, holeX0, holeY0, holeX1 - holeX0, h - holeY0);
    cairo_fill(cr);
    cairo_restore(cr);
    line(cr, holeX0 - 2, holeY0, holeX0 - 2, h, 4, kInk);
    line(cr, holeX1 + 2, holeY0, holeX1 + 2, h, 4, kInk);
    cairo_save(cr);
    cairo_translate(cr, (holeX0 + holeX1) * 0.5, holeY0 + 4);
    cairo_scale(cr, (holeX1 - holeX0) * 0.5 + 6, 10);
    cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
    cairo_restore(cr);
    outline(cr, 4, rgb(140, 156, 160));
  }
}

// A one-block girder: flanges and a lattice web, w x h px.
void paintGirder(cairo_t* cr, double w, double h)
{
  lattice(cr, 6, 10, w - 12, h - 10, h - 20, 5);
  for (const double y : {0.0, h - 12})
  {
    roundedRect(cr, 0, y, w, 12, 2);
    linearFill(cr, 0, y, 0, y + 12, kSteelLight, kSteelDark, true);
    outline(cr, 1.6);
    bolt(cr, 8, y + 6, 2.2);
    bolt(cr, w - 8, y + 6, 2.2);
  }
  line(cr, 0, 0.8, w, 0.8, 1.4, kSun, 0.8);
}

// A loose hull plate drifting off: w x 64 px, its rivet holes empty.
void paintPlate(cairo_t* cr, double w)
{
  roundedRect(cr, 1, 1, w - 2, 62, 3);
  linearFill(cr, 0, 0, 0, 64, rgb(238, 242, 248), rgb(150, 158, 174), true);
  outline(cr, 2.2);
  line(cr, 3, 3, w - 3, 3, 2, rgb(255, 255, 255), 0.7);
  line(cr, 3, 4, w - 3, 4, 2, kSun, 0.5);
  // Panel seams within.
  for (double x = 64; x < w - 4; x += 64)
    line(cr, x, 4, x, 60, 1.6, kSteelDark, 0.5);
  // The empty rivet holes.
  for (int k = 0; k < 4; ++k)
  {
    const double x = w * (k + 0.5) / 4.0;
    cairo_arc(cr, x, 10, 4.5, 0, 2 * kPi);
    setColor(cr, rgb(30, 34, 46));
    cairo_fill_preserve(cr);
    outline(cr, 1.2, rgb(240, 244, 250));
  }
  // A bent corner where it tore free.
  cairo_move_to(cr, w - 22, 63);
  cairo_line_to(cr, w - 1, 63);
  cairo_line_to(cr, w - 1, 44);
  cairo_close_path(cr);
  setColor(cr, rgb(110, 118, 134));
  cairo_fill(cr);
}

// A rivet's hex head, 24 x 14, anchored at its base centre; turn 0..2.
void paintRivetHead(cairo_t* cr, int turn)
{
  cairo_move_to(cr, 1, 14);
  cairo_line_to(cr, 3, 4);
  cairo_line_to(cr, 21, 4);
  cairo_line_to(cr, 23, 14);
  cairo_close_path(cr);
  linearFill(cr, 0, 4, 0, 14, rgb(240, 244, 250), rgb(110, 118, 134), true);
  outline(cr, 1.6);
  // The hex flats, turning.
  for (int k = 0; k < 2; ++k)
  {
    const double x = 6 + ((turn + k * 1.5) - std::floor((turn + k * 1.5) / 3.0) * 3.0) * 6.0;
    line(cr, x, 5, x, 13, 1.4, kSteelDark, 0.8);
  }
  roundedRect(cr, 5, 1, 14, 4, 2);
  linearFill(cr, 0, 1, 0, 5, rgb(255, 255, 255), kSteel);
}

// The tiny green alien from the UFO, giving a thumbs-up: 64 x 84, anchored
// at the middle of its base. It shows `up` (0..6 sixths) of itself above
// the hatch line (the texture's bottom). pose 0/1 pumps the thumb.
void paintAlien(cairo_t* cr, int up, int pose)
{
  const double H = 84;
  cairo_save(cr);
  cairo_rectangle(cr, 0, 0, 64, H);
  cairo_clip(cr);
  cairo_translate(cr, 0, (6 - up) / 6.0 * 70.0);
  const Color skin = rgb(130, 230, 110), skinDark = rgb(40, 120, 50);
  // Body.
  cairo_move_to(cr, 22, 84);
  cairo_curve_to(cr, 20, 64, 24, 54, 32, 54);
  cairo_curve_to(cr, 40, 54, 44, 64, 42, 84);
  cairo_close_path(cr);
  linearFill(cr, 22, 0, 44, 0, skin, skinDark, true);
  outline(cr, 2);
  // The arm with the thumb up.
  const double lift = pose ? 4 : 0;
  cairo_move_to(cr, 40, 62);
  cairo_curve_to(cr, 50, 60, 52, 52 - lift, 52, 46 - lift);
  setColor(cr, kInk);
  cairo_set_line_width(cr, 6);
  cairo_stroke_preserve(cr);
  setColor(cr, skin);
  cairo_set_line_width(cr, 3.5);
  cairo_stroke(cr);
  cairo_arc(cr, 52, 45 - lift, 5, 0, 2 * kPi);
  setColor(cr, skin);
  cairo_fill_preserve(cr);
  outline(cr, 1.6);
  roundedRect(cr, 50, 34 - lift, 5, 10, 2.5);
  setColor(cr, skin);
  cairo_fill_preserve(cr);
  outline(cr, 1.4);
  // The other arm, on its hip.
  cairo_move_to(cr, 24, 62);
  cairo_curve_to(cr, 16, 64, 16, 70, 22, 72);
  setColor(cr, kInk);
  cairo_set_line_width(cr, 6);
  cairo_stroke_preserve(cr);
  setColor(cr, skin);
  cairo_set_line_width(cr, 3.5);
  cairo_stroke(cr);
  // The big head, the eyes, the antennae.
  for (const double side : {-1.0, 1.0})
  {
    cairo_move_to(cr, 32 + side * 6, 22);
    cairo_curve_to(cr, 32 + side * 10, 12, 32 + side * 14, 8, 32 + side * 16, 4);
    setColor(cr, kInk);
    cairo_set_line_width(cr, 3.4);
    cairo_stroke_preserve(cr);
    setColor(cr, skin);
    cairo_set_line_width(cr, 1.6);
    cairo_stroke(cr);
    cairo_arc(cr, 32 + side * 16, 4, 3.4, 0, 2 * kPi);
    setColor(cr, rgb(255, 120, 200));
    cairo_fill_preserve(cr);
    outline(cr, 1.2);
  }
  cairo_save(cr);
  cairo_translate(cr, 32, 38);
  cairo_scale(cr, 17, 16);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_restore(cr);
  linearFill(cr, 15, 22, 49, 54, rgb(190, 255, 170), skinDark, true);
  outline(cr, 2.2);
  for (const double side : {-1.0, 1.0})
  {
    cairo_save(cr);
    cairo_translate(cr, 32 + side * 7, 38);
    cairo_rotate(cr, side * 0.4);
    cairo_scale(cr, 5, 7.5);
    cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
    cairo_restore(cr);
    setColor(cr, rgb(16, 16, 26));
    cairo_fill(cr);
    cairo_arc(cr, 32 + side * 7 - 1.5, 35, 1.8, 0, 2 * kPi);
    setColor(cr, rgb(255, 255, 255));
    cairo_fill(cr);
  }
  cairo_arc(cr, 32, 46, 4, 0.2, kPi - 0.2);
  outline(cr, 1.6);
  cairo_restore(cr);
}

// The UFO's round hatch, seen on top of the dome: w x h, cracked after a hit.
void paintHatch(cairo_t* cr, double w, double h, bool cracked)
{
  const double cx = w * 0.5, cy = h * 0.55, rx = w * 0.46, ry = h * 0.36;
  cairo_save(cr);
  cairo_translate(cr, cx, cy + 4);
  cairo_scale(cr, rx + 4, ry + 4);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_restore(cr);
  setColor(cr, rgb(40, 60, 64));
  cairo_fill(cr);
  cairo_save(cr);
  cairo_translate(cr, cx, cy);
  cairo_scale(cr, rx, ry);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_restore(cr);
  linearFill(cr, 0, cy - ry, 0, cy + ry, rgb(236, 250, 244), rgb(120, 150, 150), true);
  outline(cr, 2.4);
  for (int k = 1; k <= 2; ++k)
  {
    cairo_save(cr);
    cairo_translate(cr, cx, cy);
    cairo_scale(cr, rx * (1.0 - k * 0.28), ry * (1.0 - k * 0.28));
    cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
    cairo_restore(cr);
    setRgba(cr, rgb(60, 90, 96), 0.7);
    cairo_set_line_width(cr, 2);
    cairo_stroke(cr);
  }
  // Clamps round the rim, and a glowing glyph in the middle.
  for (int k = 0; k < 6; ++k)
  {
    const double a = k * kPi / 3 + 0.3;
    const double x = cx + std::cos(a) * rx * 0.86, y = cy + std::sin(a) * ry * 0.86;
    roundedRect(cr, x - 5, y - 3, 10, 6, 2);
    setColor(cr, rgb(90, 110, 116));
    cairo_fill(cr);
  }
  radialGlow(cr, cx, cy, ry * 0.9, kAlienGreen, 0.6);
  cairo_move_to(cr, cx - 8, cy + 3);
  cairo_line_to(cr, cx, cy - 6);
  cairo_line_to(cr, cx + 8, cy + 3);
  cairo_move_to(cr, cx - 4, cy + 1);
  cairo_line_to(cr, cx + 4, cy + 1);
  setColor(cr, rgb(30, 120, 70));
  cairo_set_line_width(cr, 2.4);
  cairo_stroke(cr);
  if (cracked)
  {
    const double pts[][2] = {{-0.1, -0.9}, {0.05, -0.4}, {-0.15, -0.1}, {0.12, 0.3}, {-0.02, 0.85}};
    for (int pass = 0; pass < 2; ++pass)
    {
      for (int k = 0; k < 5; ++k)
      {
        const double x = cx + pts[k][0] * rx, y = cy + pts[k][1] * ry;
        if (k == 0)
          cairo_move_to(cr, x, y);
        else
          cairo_line_to(cr, x, y);
      }
      cairo_move_to(cr, cx + 0.05 * rx, cy - 0.4 * ry);
      cairo_line_to(cr, cx + 0.5 * rx, cy - 0.55 * ry);
      cairo_move_to(cr, cx + 0.12 * rx, cy + 0.3 * ry);
      cairo_line_to(cr, cx - 0.45 * rx, cy + 0.5 * ry);
      setColor(cr, pass == 0 ? rgb(20, 40, 30) : kAlienGreen);
      cairo_set_line_width(cr, pass == 0 ? 4.0 : 1.6);
      cairo_stroke(cr);
    }
  }
}

// A flag on its pole planted by an idle runner: 80 x 52, anchored at the
// pole's top. unfurl 0..4 (fifths), wave 0/1.
void paintFlag(cairo_t* cr, int unfurl, int wave)
{
  const double f = (unfurl + 1) / 5.0, fw = 66 * f, fh = 40;
  auto edge = [&](double t) { return std::sin(t * kPi * 2.0 + wave * kPi) * 3.0 * t; };
  cairo_move_to(cr, 4, 4);
  for (int k = 1; k <= 12; ++k)
    cairo_line_to(cr, 4 + fw * k / 12.0, 4 + edge(k / 12.0));
  for (int k = 12; k >= 0; --k)
    cairo_line_to(cr, 4 + fw * k / 12.0, 4 + fh * (0.6 + 0.4 * f) + edge(k / 12.0));
  cairo_close_path(cr);
  linearFill(cr, 4, 0, 4 + fw, 0, rgb(255, 80, 70), rgb(200, 30, 50), true);
  outline(cr, 2);
  if (unfurl >= 3)
  {
    selectGameFont(cr);
    cairo_set_font_size(cr, 26);
    cairo_move_to(cr, 4 + fw * 0.5 - 9, 4 + fh * 0.5 + 10 + edge(0.5));
    cairo_text_path(cr, "G");
    setColor(cr, rgb(255, 240, 200));
    cairo_fill_preserve(cr);
    outline(cr, 1.2, rgb(120, 20, 30));
  }
}

// The drifting camera's frame: a ring with four little thrusters, 128 x 128,
// anchored at its centre.
void paintDrifterFrame(cairo_t* cr)
{
  const double c = 64;
  for (int k = 0; k < 4; ++k)
  {
    const double a0 = k * kPi * 0.5 + 0.25, a1 = a0 + kPi * 0.5 - 0.5;
    cairo_new_path(cr);
    cairo_arc(cr, c, c, 50, a0, a1);
    setColor(cr, kInk);
    cairo_set_line_width(cr, 8);
    cairo_stroke_preserve(cr);
    setColor(cr, kSteel);
    cairo_set_line_width(cr, 4);
    cairo_stroke(cr);
  }
  for (int k = 0; k < 4; ++k)
  {
    const double a = k * kPi * 0.5;
    cairo_save(cr);
    cairo_translate(cr, c + std::cos(a) * 50, c + std::sin(a) * 50);
    cairo_rotate(cr, a);
    roundedRect(cr, -6, -7, 14, 14, 3);
    linearFill(cr, 0, -7, 0, 7, kSteelLight, kSteelDark, true);
    outline(cr, 1.6);
    cairo_move_to(cr, 8, -4);
    cairo_line_to(cr, 13, -6);
    cairo_line_to(cr, 13, 6);
    cairo_line_to(cr, 8, 4);
    cairo_close_path(cr);
    setColor(cr, kSteelDark);
    cairo_fill(cr);
    cairo_arc(cr, 0, 0, 2.4, 0, 2 * kPi);
    setColor(cr, k % 2 ? rgb(255, 80, 60) : rgb(90, 255, 140));
    cairo_fill(cr);
    cairo_restore(cr);
  }
}

// --- Planetoids ------------------------------------------------------------------

struct Rock
{
  Color light, mid, dark, crater;
};

Rock rockLook(int look)
{
  switch (((look % 3) + 3) % 3)
  {
    case 1:
      return {rgb(240, 160, 120), rgb(186, 92, 62), rgb(90, 36, 30), rgb(130, 56, 40)};
    case 2:
      return {rgb(220, 226, 255), rgb(140, 150, 214), rgb(60, 56, 120), rgb(100, 104, 170)};
    default:
      return {rgb(226, 214, 190), rgb(156, 140, 116), rgb(70, 60, 52), rgb(110, 96, 80)};
  }
}

// A rocky little world of radius `rad` px, lit from the upper right (the
// sunrise), craters and boulders on it. (2 rad + 48) square, centred.
void paintPlanetoid(cairo_t* cr, double rad, int look, int seed)
{
  const Rock k = rockLook(look);
  const double c = rad + 24;
  radialGlow(cr, c, c, rad + 22, k.light, 0.35);
  // A lumpy outline.
  Rng rng(std::uint32_t(seed * 977 + 13));
  constexpr int n = 36;
  double bumps[n];
  for (int i = 0; i < n; ++i)
    bumps[i] = rng.range(-1.5f, 1.5f);
  for (int i = 0; i <= n; ++i)
  {
    const double a = i * 2 * kPi / n, rr = rad + bumps[i % n];
    if (i == 0)
      cairo_move_to(cr, c + std::cos(a) * rr, c + std::sin(a) * rr);
    else
      cairo_line_to(cr, c + std::cos(a) * rr, c + std::sin(a) * rr);
  }
  cairo_close_path(cr);
  cairo_pattern_t* g = cairo_pattern_create_radial(c + rad * 0.35, c - rad * 0.4, rad * 0.1, c, c, rad * 1.1);
  cairo_pattern_add_color_stop_rgb(g, 0, redOf(k.light) / 255.0, greenOf(k.light) / 255.0, blueOf(k.light) / 255.0);
  cairo_pattern_add_color_stop_rgb(g, 0.55, redOf(k.mid) / 255.0, greenOf(k.mid) / 255.0, blueOf(k.mid) / 255.0);
  cairo_pattern_add_color_stop_rgb(g, 1, redOf(k.dark) / 255.0, greenOf(k.dark) / 255.0, blueOf(k.dark) / 255.0);
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  outline(cr, 2.6);
  // Craters: a dark bowl with a lit lower rim.
  cairo_save(cr);
  cairo_arc(cr, c, c, rad - 2, 0, 2 * kPi);
  cairo_clip(cr);
  const int craters = 3 + int(rad / 24);
  for (int i = 0; i < craters; ++i)
  {
    const double a = rng.range(0, float(2 * kPi)), d = rng.range(0.1f, 0.8f) * rad;
    const double x = c + std::cos(a) * d, y = c + std::sin(a) * d, cr0 = rng.range(0.08f, 0.2f) * rad;
    cairo_arc(cr, x, y, cr0, 0, 2 * kPi);
    setRgba(cr, k.crater, 0.8);
    cairo_fill(cr);
    cairo_arc(cr, x, y, cr0, kPi * 0.85, kPi * 1.9);
    setRgba(cr, k.dark, 0.7);
    cairo_set_line_width(cr, 2);
    cairo_stroke(cr);
    cairo_arc(cr, x, y, cr0, -kPi * 0.1, kPi * 0.85);
    setRgba(cr, k.light, 0.7);
    cairo_set_line_width(cr, 1.6);
    cairo_stroke(cr);
  }
  // The terminator: the side away from the sun in shadow.
  cairo_arc(cr, c - rad * 0.55, c + rad * 0.55, rad * 1.05, 0, 2 * kPi);
  setRgba(cr, rgb(10, 8, 30), 0.28);
  cairo_fill(cr);
  cairo_restore(cr);
  // A rim of sunlight.
  cairo_new_path(cr);
  cairo_arc(cr, c, c, rad - 1.5, -kPi * 0.85, kPi * 0.05);
  setRgba(cr, rgb(255, 230, 190), 0.75);
  cairo_set_line_width(cr, 2.5);
  cairo_stroke(cr);
  // Boulders on the surface.
  for (int i = 0; i < 4; ++i)
  {
    const double a = rng.range(0, float(2 * kPi));
    cairo_arc(cr, c + std::cos(a) * (rad + 1), c + std::sin(a) * (rad + 1), rng.range(2.5f, 4.5f), 0, 2 * kPi);
    setColor(cr, k.mid);
    cairo_fill_preserve(cr);
    outline(cr, 1.4);
  }
}

// The alien's ship parts, 72 x 72 centred: 0 fin, 1 dish, 2 engine, 3
// cockpit, 4 landing leg. Mint alloy with green running lights.
void paintPart(cairo_t* cr, int kind)
{
  const Color alloyL = rgb(230, 250, 244), alloyD = rgb(110, 150, 150);
  auto lightDot = [&](double x, double y) {
    radialGlow(cr, x, y, 8, kAlienGreen, 0.8);
    cairo_arc(cr, x, y, 2.6, 0, 2 * kPi);
    setColor(cr, rgb(220, 255, 230));
    cairo_fill(cr);
  };
  switch (kind)
  {
    case 0: // fin
      cairo_move_to(cr, 14, 60);
      cairo_curve_to(cr, 20, 40, 34, 18, 56, 8);
      cairo_curve_to(cr, 52, 26, 52, 44, 58, 60);
      cairo_close_path(cr);
      linearFill(cr, 14, 8, 58, 60, alloyL, alloyD, true);
      outline(cr, 2.2);
      line(cr, 26, 52, 50, 18, 2, rgb(255, 120, 200), 0.8);
      lightDot(54, 12);
      break;
    case 1: // dish
      line(cr, 36, 44, 36, 62, 5, kInk);
      line(cr, 36, 44, 36, 62, 2.5, alloyD);
      cairo_save(cr);
      cairo_translate(cr, 36, 34);
      cairo_rotate(cr, -0.4);
      cairo_scale(cr, 24, 12);
      cairo_arc(cr, 0, 0, 1, 0, kPi);
      cairo_close_path(cr);
      cairo_restore(cr);
      linearFill(cr, 12, 30, 60, 46, alloyL, alloyD, true);
      outline(cr, 2.2);
      line(cr, 36, 36, 44, 18, 2, alloyD);
      lightDot(44, 18);
      break;
    case 2: // engine
      roundedRect(cr, 18, 16, 30, 40, 8);
      linearFill(cr, 18, 0, 48, 0, alloyL, alloyD, true);
      outline(cr, 2.2);
      cairo_move_to(cr, 22, 56);
      cairo_line_to(cr, 14, 66);
      cairo_line_to(cr, 52, 66);
      cairo_line_to(cr, 44, 56);
      cairo_close_path(cr);
      linearFill(cr, 0, 56, 0, 66, alloyD, rgb(50, 70, 76), true);
      outline(cr, 2);
      radialGlow(cr, 33, 66, 14, rgb(255, 120, 200), 0.8);
      for (int k = 0; k < 3; ++k)
        line(cr, 20, 26 + k * 10, 46, 26 + k * 10, 1.6, alloyD, 0.8);
      lightDot(33, 22);
      break;
    case 3: // cockpit
      cairo_arc(cr, 36, 42, 22, kPi, 2 * kPi);
      cairo_close_path(cr);
      linearFill(cr, 14, 20, 58, 42, rgba(200, 255, 230, 230), rgba(40, 130, 110, 230), true);
      outline(cr, 2.2);
      roundedRect(cr, 10, 40, 52, 10, 4);
      linearFill(cr, 0, 40, 0, 50, alloyL, alloyD, true);
      outline(cr, 2);
      line(cr, 24, 32, 30, 24, 3, rgb(255, 255, 255), 0.8);
      lightDot(16, 45);
      lightDot(56, 45);
      break;
    default: // landing leg
      line(cr, 24, 12, 44, 50, 7, kInk);
      line(cr, 24, 12, 44, 50, 4, alloyD);
      line(cr, 30, 30, 22, 50, 5, kInk);
      line(cr, 30, 30, 22, 50, 2.5, alloyL);
      roundedRect(cr, 32, 50, 24, 8, 3);
      linearFill(cr, 0, 50, 0, 58, alloyL, alloyD, true);
      outline(cr, 2);
      cairo_arc(cr, 24, 12, 6, 0, 2 * kPi);
      linearFill(cr, 18, 6, 30, 18, alloyL, alloyD, true);
      outline(cr, 1.8);
      lightDot(44, 54);
      break;
  }
}

} // namespace

void World::drawHullBack(Renderer& r, float camX, float camY, int frame) const
{
  const auto& h = mHull;
  for (std::size_t di = 0; di < h.decos.size(); ++di)
  {
    const auto& d = h.decos[di];
    const float x = float(d.x0) * kTilePx - camX, y = float(d.y0) * kTilePx - camY;
    const int wpx = (d.x1 - d.x0 + 1) * int(kTilePx), hpx = (d.y1 - d.y0 + 1) * int(kTilePx);
    const float w = float(wpx), hh = float(hpx);
    if (!visible(x - 128.0f, y - 128.0f, w + 256.0f, hh + 256.0f))
      continue;
    const std::string size = std::to_string(wpx) + "x" + std::to_string(hpx);
    const int seed = d.x0 * 31 + d.y0;
    // Where the rect turns solid (its middle column), in px from its top.
    int deckRow = d.y1 + 1;
    for (int by = d.y0; by <= d.y1 && deckRow > d.y1; ++by)
      if (mMap.block((d.x0 + d.x1) / 2, by) == Tile::Solid)
        deckRow = by;
    const int deck = (deckRow - d.y0) * int(kTilePx);
    if (d.kind == "mast")
    {
      const Texture& tex = baked(mArt, r, "mast/" + size + "/" + std::to_string(deck), wpx, hpx, 0.0f, 0.0f,
        [&](cairo_t* cr) { paintMast(cr, w, hh, double(deck)); });
      r.draw(tex, x, y);
      // The blinking light, and the duck's tether to the nearest duck.
      const bool on = (frame / 20) % 3 != 2;
      if (on)
      {
        drawGlow(r, mArt, x + w * 0.5f, y + 14.0f, 34.0f, rgb(255, 50, 40), 0.85f);
        r.fillRect(x + w * 0.5f - 3.0f, y + 9.0f, 6.0f, 5.0f, rgb(255, 210, 200));
      }
      for (std::size_t i = 0; i < mItems.size(); ++i)
      {
        const auto& it = mItems[i];
        if (it.kind != ItemKind::Duck || it.taken || std::abs(it.x - d.x0 * kCellsPerTile) > 16)
          continue;
        float dy = float(it.y - 1) * kCellPx - camY + 32.0f;
        if (it.floating)
          dy += std::sin(float(frame + int(i) * 9) * 0.08f) * 6.0f;
        const float dx = float(it.x) * kCellPx - camX + 32.0f;
        const float ax = x + w * 0.5f + 26.0f, ay = y + 40.0f;
        float px = ax, py = ay;
        for (int k = 1; k <= 12; ++k)
        {
          const float t = float(k) / 12.0f;
          const float qx = ax + (dx - ax) * t;
          const float qy = ay + (dy - ay) * t + std::sin(t * 3.14159f) * (14.0f + 4.0f * std::sin(float(frame) * 0.05f));
          r.drawLine(px, py, qx, qy, 2.0f, rgb(230, 230, 210));
          px = qx;
          py = qy;
        }
      }
    }
    else if (d.kind == "dish")
    {
      const Texture& tex = baked(mArt, r, "dish/" + size + "/" + std::to_string(deck), wpx, hpx, 0.0f, 0.0f,
        [&](cairo_t* cr) { paintDish(cr, w, hh, double(deck)); });
      r.draw(tex, x, y);
    }
    else if (d.kind == "truss")
    {
      const Texture& tex =
        baked(mArt, r, "truss/" + size, wpx, hpx, 0.0f, 0.0f, [&](cairo_t* cr) { paintTruss(cr, w, hh); });
      r.draw(tex, x, y);
      if ((frame / 24 + d.x0) % 4 == 0)
        drawGlow(r, mArt, x + w - 8.0f, y + 6.0f, 14.0f, rgb(90, 255, 140), 0.8f);
    }
    else if (d.kind == "wing")
    {
      const Texture& tex =
        baked(mArt, r, "wing/" + size, wpx, hpx, 0.0f, 0.0f, [&](cairo_t* cr) { paintWing(cr, w, hh); });
      r.draw(tex, x, y);
      // A glint running along the cells.
      const float gx = x + std::fmod(float(frame) * 3.0f + float(d.x0) * 50.0f, w + 200.0f) - 100.0f;
      if (gx > x && gx < x + w)
        drawGlow(r, mArt, gx, y + hh * 0.3f, 30.0f, rgb(200, 230, 255), 0.25f);
    }
    else if (d.kind == "airlock")
    {
      const Texture& tex =
        baked(mArt, r, "airlock/" + size, wpx, hpx, 0.0f, 0.0f, [&](cairo_t* cr) { paintAirlock(cr, w, hh); });
      r.draw(tex, x, y);
      const bool on = (frame / 30) % 2 == 0;
      drawGlow(r, mArt, x + w - 22.0f, y + 26.0f, 16.0f, on ? rgb(90, 255, 140) : rgb(40, 120, 70), 0.9f);
    }
    else if (d.kind == "sign42" || d.kind == "42")
    {
      // At least three blocks wide, so the stencil reads.
      const int sw = std::max(wpx, 3 * int(kTilePx)), sh = std::min(std::max(hpx, 40), 64);
      const Texture& tex = baked(mArt, r, "sign42/" + std::to_string(sw) + "x" + std::to_string(sh), sw, sh,
        float(sw) * 0.5f, float(sh) * 0.5f, [&](cairo_t* cr) { paintSign42(cr, float(sw), float(sh)); });
      r.draw(tex, x + w * 0.5f, y + hh * 0.5f);
    }
    else if (d.kind == "ufo")
    {
      // The open hatch (a broken look-13 breakable on it) cuts a shaft.
      int hx0 = -1, hx1 = -1, hy0 = 0;
      for (const auto& b : mBreakables)
        if (b.look == 13 && b.broken && b.x1 >= d.x0 && b.x0 <= d.x1 && b.y1 >= d.y0 && b.y0 <= d.y1)
        {
          hx0 = (b.x0 - d.x0) * int(kTilePx);
          hx1 = (b.x1 - d.x0 + 1) * int(kTilePx);
          hy0 = (b.y0 - d.y0) * int(kTilePx);
        }
      const std::string key = "ufo/" + size + "/" + std::to_string(hx0) + "," + std::to_string(hx1);
      const Texture& tex = baked(mArt, r, key, wpx, hpx, 0.0f, 0.0f,
        [&](cairo_t* cr) { paintUfo(cr, w, hh, double(hx0), double(hx1), double(hy0), seed); });
      r.draw(tex, x, y);
      if (hx0 >= 0)
        drawGlow(r, mArt, x + float(hx0 + hx1) * 0.5f, y + float(hy0) + 10.0f, 50.0f, kAlienGreen,
          0.35f + 0.1f * std::sin(float(frame) * 0.2f));
      // The hatch sits on the dome: draw it over it.
      for (const auto& b : mBreakables)
        if (b.look == 13 && !b.broken && b.x1 >= d.x0 && b.x0 <= d.x1 && b.y1 >= d.y0 && b.y0 <= d.y1)
          drawHullBreakable(r, b, camX, camY, frame);
    }
    else if (d.kind == "girder")
    {
      const Texture& tex =
        baked(mArt, r, "girder/" + size, wpx, hpx, 0.0f, 0.0f, [&](cairo_t* cr) { paintGirder(cr, w, hh); });
      r.draw(tex, x, y);
    }
  }

  // The bolted plates: seams at their ends, so the loose ones read apart
  // from the hull.
  for (const auto& p : h.plates)
  {
    if (p.drifting)
      continue;
    const float x = float(p.x0) * kTilePx - camX, y = float(p.y) * kTilePx - camY;
    const float w = float(p.x1 - p.x0 + 1) * kTilePx;
    if (!visible(x, y, w, kTilePx))
      continue;
    r.fillRect(x - 1.0f, y, 3.0f, kTilePx, rgb(30, 34, 46));
    r.fillRect(x + w - 2.0f, y, 3.0f, kTilePx, rgb(30, 34, 46));
    r.fillRect(x + 2.0f, y + kTilePx - 4.0f, w - 4.0f, 4.0f, rgba(30, 34, 46, 160));
  }
}

void World::drawHullFront(Renderer& r, float camX, float camY, int frame) const
{
  const auto& h = mHull;

  // Rivets: bolted in, or turning loose and rising as a mite works them.
  for (const auto& p : h.plates)
  {
    if (p.drifting)
      continue;
    const float surface = float(p.y) * kTilePx - camY;
    for (std::size_t i = 0; i < p.rivets.size(); ++i)
    {
      const float cx = float(p.rivets[i]) * kCellPx + kCellPx * 0.5f - camX;
      if (!visible(cx - 20.0f, surface - 40.0f, 40.0f, 40.0f))
        continue;
      const int done = i < p.unbolt.size() ? p.unbolt[i] : 0;
      const float u = std::clamp(float(done) / float(kRivetFrames), 0.0f, 1.0f);
      const float rise = u * 18.0f;
      const int turn = u > 0.0f ? (frame / 2) % 3 : 0;
      if (rise > 0.5f)
      {
        // The threaded shaft showing under the head.
        r.fillRect(cx - 3.5f, surface - rise, 7.0f, rise, rgb(150, 158, 174));
        for (float ty = surface - rise + float((frame / 2) % 3); ty < surface; ty += 3.0f)
          r.fillRect(cx - 3.5f, ty, 7.0f, 1.2f, rgb(80, 88, 104));
        if (u < 1.0f && frame % 3 == 0)
          drawGlow(r, mArt, cx + float(int(hashCell(frame, int(i)) % 13u) - 6), surface - 2.0f, 10.0f,
            rgb(255, 220, 120), 0.8f);
      }
      const Texture& head = baked(mArt, r, "rivet/" + std::to_string(turn), 24, 14, 12.0f, 14.0f,
        [&](cairo_t* cr) { paintRivetHead(cr, turn); });
      r.draw(head, cx, surface + 3.0f - rise);
    }
  }

  // Plates that came loose, drifting off with their rivet holes empty.
  for (const auto& p : h.plates)
  {
    if (!p.drifting || p.platform < 0 || std::size_t(p.platform) >= mPlatforms.size())
      continue;
    const auto& pl = mPlatforms[std::size_t(p.platform)];
    const float w = float(pl.w) * kCellPx;
    const float x = float(pl.x) * kCellPx - camX, y = float(pl.y) * kCellPx - camY;
    if (!visible(x, y, w, kTilePx))
      continue;
    const int wpx = int(w);
    const Texture& tex = baked(mArt, r, "plate/" + std::to_string(wpx), wpx, 64, w * 0.5f, 32.0f,
      [&](cairo_t* cr) { paintPlate(cr, w); });
    DrawOpts o;
    o.angle = std::sin(float(p.life) * 0.08f) * 1.5f;
    o.alpha = std::clamp(float(120 - p.life) / 30.0f, 0.0f, 1.0f);
    r.draw(tex, x + w * 0.5f, y + 32.0f, o);
    // A few flakes of paint shed as it goes.
    for (int k = 0; k < 3; ++k)
    {
      const unsigned hs = hashCell(p.x0 + k, p.life / 8);
      r.fillRect(x + float(hs % unsigned(std::max(1, wpx))), y + 64.0f + float(p.life % 8) * 3.0f + float(k) * 9.0f,
        3.0f, 3.0f, rgba(220, 226, 236, int(200 * o.alpha)));
    }
  }

  // Rivet Mites.
  for (std::size_t mi = 0; mi < h.mites.size(); ++mi)
  {
    const auto& m = h.mites[mi];
    if (m.state == MiteState::Dead)
      continue;
    float cx = (m.x + 1.0f) * kCellPx - camX;
    const float by = (m.y + 1.0f) * kCellPx - camY;
    if (!visible(cx - 64.0f, by - 64.0f, 128.0f, 96.0f))
      continue;
    const bool carrier = m.swarm >= 0 && std::size_t(m.swarm) < mEnemies.size() &&
      (mEnemies[std::size_t(m.swarm)].flags() & kEnemyCarrier) != 0;
    int variant = 0;
    int anim = (frame / 3 + int(mi)) % 2;
    DrawOpts o;
    switch (m.state)
    {
      case MiteState::Unbolt:
        anim = (frame / 2) % 2;
        cx += float((frame + int(mi)) % 2) * 1.5f;
        if (frame % 4 == int(mi) % 4)
          drawGlow(r, mArt, cx + float(m.dir) * 26.0f, by - 6.0f, 12.0f, rgb(255, 220, 120), 0.8f);
        break;
      case MiteState::Tell:
        variant = 1;
        cx += float((frame / 2) % 2) * 2.0f - 1.0f;
        drawGlow(r, mArt, cx, by - 16.0f, 34.0f, rgb(120, 255, 90), 0.5f + 0.3f * float(frame % 2));
        break;
      case MiteState::Hop:
        anim = 0;
        o.angle = -25.0f * float(m.dir);
        break;
      case MiteState::Ride:
        o.angle = std::sin(float(frame) * 0.9f + float(mi)) * 18.0f;
        variant = carrier ? 1 : 0;
        break;
      case MiteState::Fall:
        anim = 0;
        o.angle = float(m.timer) * 24.0f * float(m.dir);
        break;
      default:
        break;
    }
    if (carrier && variant == 0)
      drawGlow(r, mArt, cx, by - 14.0f, 22.0f, rgb(120, 255, 90), 0.22f);
    const Texture& tex = styledEnemySprite(mArt, r, mTheme, "rivet_mites", variant, anim, 2, 1).get(m.dir);
    // Turned about its middle, not its feet.
    const float c = std::cos(o.angle * 0.0174533f), s = std::sin(o.angle * 0.0174533f);
    const float ax = cx - 16.0f * s, ay = by - 16.0f + 16.0f * c;
    r.draw(tex, ax, ay, o);
    if (m.flash > 0)
    {
      DrawOpts f = o;
      f.blend = Blend::Add;
      f.alpha = float(m.flash) / 8.0f;
      r.draw(tex, ax, ay, f);
    }
  }

  // The drifting camera's thruster frame, tumbling round it.
  for (const auto& d : h.drifters)
  {
    float cx = d.fx * kCellPx - camX, cy = d.fy * kCellPx - camY;
    if (d.enemy >= 0 && std::size_t(d.enemy) < mEnemies.size())
    {
      const auto& e = mEnemies[std::size_t(d.enemy)];
      if (!e.alive)
        continue;
      cx = e.drawX * kCellPx + float(e.w) * kCellPx * 0.5f - camX;
      cy = (e.drawY + 1.0f) * kCellPx - 32.0f - camY;
    }
    if (!visible(cx - 80.0f, cy - 80.0f, 160.0f, 160.0f))
      continue;
    const Texture& ring = baked(mArt, r, "drifter", 128, 128, 64.0f, 64.0f, paintDrifterFrame);
    DrawOpts o;
    o.angle = d.spin * 57.2958f;
    r.draw(ring, cx, cy, o);
    // A puff from one thruster now and then.
    const int beat = frame % 40;
    if (beat < 8)
    {
      const float a = d.spin + float((frame / 40) % 4) * 1.5708f;
      const float out = 62.0f + float(beat) * 3.0f;
      drawGlow(r, mArt, cx + std::cos(a) * out, cy + std::sin(a) * out, 14.0f + float(beat) * 2.0f,
        rgb(220, 236, 255), 0.6f * (1.0f - float(beat) / 8.0f));
    }
  }

  // Flags the runner planted.
  for (const auto& f : h.flags)
  {
    const float fx = float(f.x) * kCellPx - camX, fy = float(f.y + 1) * kCellPx - camY; // f.y is the row stood in
    if (!visible(fx - 20.0f, fy - 140.0f, 120.0f, 160.0f))
      continue;
    const float pole = 112.0f;
    r.fillRect(fx - 10.0f, fy - 5.0f, 20.0f, 5.0f, rgb(80, 86, 100));
    r.fillRect(fx - 2.5f, fy - pole, 5.0f, pole, rgb(200, 206, 220));
    r.fillRect(fx - 2.5f, fy - pole, 2.0f, pole, rgb(250, 252, 255));
    r.fillRect(fx - 4.0f, fy - pole - 6.0f, 8.0f, 8.0f, rgb(255, 210, 80));
    const int unfurl = std::clamp(f.age / 4, 0, 4);
    const int wave = (frame / 8) % 2;
    const Texture& flag = baked(mArt, r, "flag/" + std::to_string(unfurl) + "/" + std::to_string(wave), 80, 52, 4.0f,
      4.0f, [&](cairo_t* cr) { paintFlag(cr, unfurl, wave); });
    r.draw(flag, fx + 2.0f, fy - pole + 2.0f);
  }

  // The UFO's alien popping out of its hatch with a thumbs-up.
  if (h.alien > 0)
  {
    float lineY = float(h.alienY - kCellsPerTile) * kCellPx - camY;
    for (const auto& b : mBreakables)
      if (b.look == 13 && std::abs(b.x0 * kCellsPerTile - h.alienX) <= 6)
        lineY = float(b.y0) * kTilePx - camY;
    const float ax = float(h.alienX) * kCellPx - camX;
    const int t = 30 - h.alien;
    const int up = std::clamp(std::min(t + 1, h.alien), 0, 6);
    const int pose = (frame / 5) % 2;
    if (up > 0)
    {
      const Texture& tex = baked(mArt, r, "alien/" + std::to_string(up) + "/" + std::to_string(pose), 64, 84, 32.0f,
        84.0f, [&](cairo_t* cr) { paintAlien(cr, up, pose); });
      r.draw(tex, ax, lineY + 8.0f);
      if (up == 6 && pose == 1)
        drawGlow(r, mArt, ax + 20.0f, lineY - 48.0f, 18.0f, rgb(255, 255, 200), 0.7f);
    }
  }

  // The Recoil Cannon's blast at the runner's hands (or under the feet).
  if (h.kick > 0)
  {
    const auto& p = mPlayer;
    const float x = float(p.x) * kCellPx + 1.5f * kCellPx - camX, foot = float(p.y + 1) * kCellPx - camY;
    const float t = std::clamp(float(h.kick) / 6.0f, 0.0f, 1.0f);
    float mx = x, my = foot + 6.0f, dx = 0.0f, dy = 1.0f;
    if (h.kickDir == 1 || h.kickDir == -1)
    {
      dx = float(h.kickDir);
      dy = 0.0f;
      mx = x + dx * 1.95f * kCellPx;
      my = foot - 2.75f * kCellPx;
    }
    else if (h.kickDir == 0)
    {
      // Shot straight up: the blast leaves over the runner's head.
      dy = -1.0f;
      my = foot - 5.3f * kCellPx;
    }
    drawGlow(r, mArt, mx + dx * 24.0f, my + dy * 24.0f, 40.0f + 50.0f * t, rgb(255, 150, 60), 0.9f * t);
    drawGlow(r, mArt, mx + dx * 10.0f, my + dy * 10.0f, 22.0f, rgb(255, 255, 230), t);
    // Flame tongues in a cone, and the smoke ring blown out past them.
    for (int k = -2; k <= 2; ++k)
    {
      const float a = std::atan2(dy, dx) + float(k) * 0.22f;
      const float len = (34.0f + 16.0f * float(2 - std::abs(k))) * (0.4f + 0.6f * t);
      r.drawLine(mx, my, mx + std::cos(a) * len, my + std::sin(a) * len, 6.0f * t + 2.0f, rgba(255, 200, 90, int(230 * t)),
        Blend::Add);
    }
    const float ring = 40.0f + (1.0f - t) * 50.0f;
    for (int k = 0; k < 8; ++k)
    {
      const float a = float(k) * 0.785f;
      const float px = mx + dx * ring + std::cos(a) * 18.0f * (1.0f - dx * dx * 0.6f) * (2.0f - t);
      const float py = my + dy * ring + std::sin(a) * 18.0f * (1.0f - dy * dy * 0.6f) * (2.0f - t);
      drawGlow(r, mArt, px, py, 12.0f, rgb(200, 206, 220), 0.5f * t);
    }
  }
}

void World::drawHullHud(Renderer& r, int frame) const
{
  if (!mHull.lowgrav)
    return;
  // A small badge under the score panel: LOW G, a dot bobbing over its line.
  const float w = 118.0f, hgt = 30.0f;
  const float x = float(kScreenW) - 12.0f - w, y = 10.0f + float(kHudPanelH) + 8.0f;
  r.fillRect(x, y, w, hgt, rgba(8, 6, 22, 170));
  r.fillRect(x, y, w, 2.0f, withAlpha(mTheme.accentB, 150));
  r.drawText("LOW G", x + 14.0f, y + 6.0f, {16.0f, mTheme.accentB, rgb(10, 8, 20)});
  const float bob = std::sin(float(frame) * 0.07f) * 5.0f;
  r.fillRect(x + w - 30.0f, y + hgt - 7.0f, 18.0f, 2.0f, withAlpha(mTheme.accentB, 120));
  r.fillRect(x + w - 24.0f, y + 11.0f + bob, 6.0f, 6.0f, mTheme.accentB);
}

void World::drawHullBreakable(Renderer& r, const Breakable& b, float camX, float camY, int frame) const
{
  (void)frame;
  const float x = float(b.x0) * kTilePx - camX, y = float(b.y0) * kTilePx - camY;
  // Look 13: the crashed UFO's round hatch (hp 2), cracked after a hit. It
  // is the top block of its rect; the shaft under it is the dome's.
  const int wpx = (b.x1 - b.x0 + 1) * int(kTilePx), hpx = int(kTilePx);
  if (!visible(x, y, float(wpx), float(hpx)))
    return;
  const bool cracked = b.hp <= 1;
  const std::string key = "hatch/" + std::to_string(wpx) + "x" + std::to_string(hpx) + (cracked ? "c" : "");
  const Texture& tex = baked(mArt, r, key, wpx, hpx, 0.0f, 0.0f,
    [&](cairo_t* cr) { paintHatch(cr, float(wpx), float(hpx), cracked); });
  r.draw(tex, x, y);
}

void World::drawOrbitBack(Renderer& r, float camX, float camY, int frame) const
{
  const auto& o = mOrbit;
  for (std::size_t i = 0; i < o.planets.size(); ++i)
  {
    const auto& p = o.planets[i];
    const float cx = p.cx * kCellPx - camX, cy = p.cy * kCellPx - camY;
    const float rad = p.r * kCellPx, reach = (p.r + 8.0f) * kCellPx;
    if (!visible(cx - reach, cy - reach, reach * 2.0f, reach * 2.0f))
      continue;
    // The field's reach: a faint dotted ring, turning slowly, brighter while
    // it holds you.
    const bool held = o.planet == int(i);
    const Color ring = held ? rgba(160, 225, 255, 150) : rgba(150, 205, 255, 80);
    const int dots = std::max(48, int(reach * 6.28318f / 18.0f));
    const float spin = float(frame) * 0.003f * (i % 2 ? 1.0f : -1.0f);
    const float dot = held ? 4.0f : 3.0f;
    for (int k = 0; k < dots; ++k)
    {
      const float a = spin + float(k) * 6.28318f / float(dots);
      r.fillRect(cx + std::cos(a) * reach - dot * 0.5f, cy + std::sin(a) * reach - dot * 0.5f, dot, dot, ring);
    }
    drawGlow(r, mArt, cx, cy, reach * 0.9f, rgb(110, 170, 255), held ? 0.12f : 0.06f);
    const int rpx = int(rad);
    const int size = 2 * rpx + 48;
    const Texture& tex = baked(mArt, r, "planetoid/" + std::to_string(rpx) + "/" + std::to_string(p.look) + "/" +
        std::to_string(i % 4), size, size, float(size) * 0.5f, float(size) * 0.5f,
      [&](cairo_t* cr) { paintPlanetoid(cr, rad, p.look, int(i)); });
    r.draw(tex, cx, cy);
  }
}

void World::drawOrbitFront(Renderer& r, float camX, float camY, int frame) const
{
  const auto& o = mOrbit;
  for (std::size_t i = 0; i < o.parts.size(); ++i)
  {
    const auto& p = o.parts[i];
    if (p.taken && p.flash <= 0)
      continue;
    float cx = p.x * kCellPx - camX, cy = p.y * kCellPx - camY;
    if (!visible(cx - 80.0f, cy - 80.0f, 160.0f, 160.0f))
      continue;
    const int kind = std::clamp(p.kind, 0, 4);
    const Texture& tex = baked(mArt, r, "part/" + std::to_string(kind), 72, 72, 36.0f, 36.0f,
      [&](cairo_t* cr) { paintPart(cr, kind); });
    if (p.taken)
    {
      // Snapped up: a burst and a ring, the part swelling away.
      const float t = std::clamp(float(p.flash) / 20.0f, 0.0f, 1.0f);
      drawGlow(r, mArt, cx, cy, 40.0f + (1.0f - t) * 80.0f, kAlienGreen, t);
      drawGlow(r, mArt, cx, cy, 24.0f, rgb(255, 255, 255), t);
      const float rr = 30.0f + (1.0f - t) * 70.0f;
      for (int k = 0; k < 12; ++k)
      {
        const float a = float(k) * 0.5236f;
        r.drawLine(cx + std::cos(a) * rr * 0.8f, cy + std::sin(a) * rr * 0.8f, cx + std::cos(a) * rr,
          cy + std::sin(a) * rr, 3.0f, rgba(200, 255, 220, int(255 * t)), Blend::Add);
      }
      DrawOpts d;
      d.alpha = t;
      d.scale = 1.0f + (1.0f - t) * 0.8f;
      r.draw(tex, cx, cy, d);
      continue;
    }
    cy += std::sin(float(frame) * 0.09f + float(i)) * 5.0f;
    drawGlow(r, mArt, cx, cy, 64.0f, kAlienGreen, 0.55f + 0.2f * std::sin(float(frame) * 0.15f + float(i)));
    DrawOpts d;
    d.angle = std::sin(float(frame) * 0.05f + float(i) * 2.0f) * 10.0f;
    r.draw(tex, cx, cy, d);
  }
}

} // namespace gr
