// Level 34, Zeppelin Relay: drawing. See world_fleet.cpp for the logic.
//
// Behind the tiles (drawFleetSet): every ship of the fleet, its cigar
// balloon (fabric gores, a painted band, tail fins, the sunset along its
// back) on rigging lines over the deck, its hull planked like the hull
// blocks with lit portholes and copper sheathing, a pusher propeller at the
// stern, brass deck rails and a lantern at the bow, the deck itself where
// the ship moves (the steady ones' decks are tiles); the Looter's rotors,
// its Jolly Roger and the smoke when it goes down; the bomb bay's inside.
// Human Cannonball's circus bunting and banner. Behind the runner
// (drawFleetBack): the hulls' brass name boards (the easter egg's glint),
// hull number 42, the bomb bay's hatch and keel, the brass rings on their
// tether balloons (or chains), the captain's log, Human Cannonball's nets
// and their score boards, its cannon and the flight's smoke trail. In front
// (drawFleetFront): the Grapple Gun's line (taut to a ring, out to the hook,
// or reeling in a yanked enemy), Sky Pirates (the cutlass and pistol tells,
// the jet pack), Peck Parrots (pecking, the squawk, the dive) and the green
// carrier, Balloon Mines (the lit fuse and the blast ring), the shrapnel,
// the cloud sea below everything. The HUD (drawFleetHud): Human
// Cannonball's angle dial.

#include "game/world.hpp"

#include "assets/art.hpp"
#include "assets/art_fleet34.hpp"
#include "assets/enemy_art.hpp"
#include "base/math.hpp"
#include "game/fleet_draw.hpp"
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
constexpr Color kInk = rgb(34, 18, 22);
constexpr Color kHudInk = rgb(10, 8, 20);
constexpr Color kBrass = rgb(220, 178, 88);
constexpr Color kBrassHi = rgb(255, 236, 160);
constexpr Color kBrassLo = rgb(130, 90, 36);
constexpr Color kRim = rgb(255, 200, 150);
constexpr Color kRope = rgb(206, 176, 124);
constexpr Color kLamp = rgb(255, 210, 130);
constexpr float kEnvRise = 16.0f; // cells from a deck up to its balloon's top
constexpr float kEnvH = 248.0f;   // px: the balloon's height
constexpr float kRailH = 26.0f;   // px: the deck rails

bool offScreen(float x, float y, float w, float h, float m = 96.0f)
{
  return x > float(kScreenW) + m || x + w < -m || y > float(kScreenH) + m || y + h < -m;
}

float unit(std::uint32_t h) { return float(h % 10000u) / 10000.0f; }

float lerpF(int a, int b, float t) { return float(a) + (float(b) - float(a)) * t; }

// A texture baked once and kept in the Art's sprite cache under `key`; the
// mirrored copy too, its anchor mirrored with it.
const Sprite& bakedSprite(const Art& art, const Renderer& r, const std::string& key, int w, int h, float ax, float ay,
  const std::function<void(cairo_t*)>& paint)
{
  const std::string id = "~fleet/" + key;
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

void rivet(cairo_t* cr, double x, double y, double r)
{
  circle(cr, x, y, r);
  setRgba(cr, kBrassLo, 1.0);
  cairo_fill(cr);
  circle(cr, x - r * 0.25, y - r * 0.25, r * 0.6);
  setRgba(cr, kBrassHi, 0.9);
  cairo_fill(cr);
}

// A rope: a dark line under a hemp one.
void rope(Renderer& r, float x0, float y0, float x1, float y1, float w = 4.0f)
{
  r.drawLine(x0, y0, x1, y1, w + 3.0f, rgba(40, 24, 20, 220));
  r.drawLine(x0, y0, x1, y1, w, kRope);
}

// --- Painters: the ships ---------------------------------------------------------------------------

struct Palette
{
  Color body, accent;
};

constexpr std::array<Palette, 6> kPalettes{{
  {rgb(240, 228, 206), rgb(196, 56, 56)},
  {rgb(132, 196, 206), rgb(250, 240, 220)},
  {rgb(228, 182, 84), rgb(130, 40, 50)},
  {rgb(236, 146, 124), rgb(52, 62, 112)},
  {rgb(176, 156, 214), rgb(242, 202, 92)},
  {rgb(156, 208, 172), rgb(172, 72, 52)},
}};

// The balloon's half height at u (0 the tail, 1 the nose): a cigar, blunt
// at the nose and tapering to the fins.
double envHalf(double u, double ry)
{
  const double d = std::fabs(2.0 * u - 1.0);
  const double p = u > 0.5 ? 2.6 : 1.7;
  return ry * std::pow(std::max(0.0, 1.0 - std::pow(std::min(1.0, d), p)), 1.0 / p);
}

// The balloon, `len` px long, kEnvH tall, its tail's tip at x 120 (the
// fins reach past it), its top at y 26.
void paintEnvelope(cairo_t* cr, double len, const Palette& pal)
{
  const double x0 = 120, ry = kEnvH * 0.5, cy = 26 + ry;
  auto xu = [&](double u) { return x0 + u * len; };
  auto outline = [&] {
    cairo_move_to(cr, xu(0), cy);
    for (int i = 1; i <= 90; ++i)
    {
      const double u = double(i) / 90.0;
      cairo_line_to(cr, xu(u), cy - envHalf(u, ry));
    }
    for (int i = 89; i >= 0; --i)
    {
      const double u = double(i) / 90.0;
      cairo_line_to(cr, xu(u), cy + envHalf(u, ry));
    }
    cairo_close_path(cr);
  };
  const double fin = std::min(0.22, 160.0 / len);
  // The top and bottom fins, behind the body.
  for (const double s : {-1.0, 1.0})
  {
    cairo_move_to(cr, xu(fin), cy + s * envHalf(fin, ry) * 0.92);
    cairo_line_to(cr, xu(fin * 0.25), cy + s * ry * 0.98);
    cairo_line_to(cr, xu(-fin * 0.25), cy + s * ry * 0.96);
    cairo_line_to(cr, xu(-fin * 0.05), cy + s * 6);
    cairo_close_path(cr);
    lgrad(cr, 0, cy - ry, 0, cy + ry, {{0.0, lerpColor(pal.accent, rgb(255, 220, 190), 0.25f)}, {1.0, lerpColor(pal.accent, rgb(60, 40, 80), 0.4f)}},
      1.0, true);
    stroke(cr, kInk, 2.0);
    // The rudder's stripes.
    cairo_move_to(cr, xu(fin * 0.5), cy + s * ry * 0.6);
    cairo_line_to(cr, xu(-fin * 0.1), cy + s * ry * 0.6);
    stroke(cr, pal.body, 4.0, 0.8);
  }
  // The body: lit warm along the top, dusky lavender under it.
  outline();
  lgrad(cr, 0, cy - ry, 0, cy + ry,
    {{0.0, lerpColor(pal.body, rgb(255, 232, 200), 0.45f)}, {0.4, pal.body}, {0.8, lerpColor(pal.body, rgb(110, 84, 140), 0.45f)},
      {1.0, lerpColor(pal.body, rgb(60, 44, 80), 0.6f)}});
  cairo_save(cr);
  outline();
  cairo_clip(cr);
  // The sun's sheen toward the nose.
  glow(cr, xu(0.78), cy - ry * 0.55, ry * 1.3, rgb(255, 236, 200), 0.35);
  // The painted band.
  cairo_move_to(cr, xu(0), cy + 2);
  for (int i = 0; i <= 90; ++i)
  {
    const double u = double(i) / 90.0;
    cairo_line_to(cr, xu(u), cy + envHalf(u, ry) * 0.12);
  }
  for (int i = 90; i >= 0; --i)
  {
    const double u = double(i) / 90.0;
    cairo_line_to(cr, xu(u), cy + envHalf(u, ry) * 0.36);
  }
  cairo_close_path(cr);
  setRgba(cr, pal.accent, 0.92);
  cairo_fill(cr);
  // The gores along it and the ribs across it.
  for (const double k : {-0.72, -0.42, -0.14, 0.12, 0.36, 0.64})
  {
    cairo_move_to(cr, xu(0), cy);
    for (int i = 1; i <= 60; ++i)
    {
      const double u = double(i) / 60.0;
      cairo_line_to(cr, xu(u), cy + k * envHalf(u, ry));
    }
    stroke(cr, rgb(60, 40, 50), 1.0, 0.28);
  }
  for (double x = xu(0) + 48; x < xu(1) - 10; x += 64)
  {
    const double u = (x - x0) / len;
    const double hh = envHalf(u, ry);
    cairo_move_to(cr, x, cy - hh);
    cairo_curve_to(cr, x + 6, cy - hh * 0.3, x + 6, cy + hh * 0.3, x, cy + hh);
    stroke(cr, rgb(60, 40, 50), 1.0, 0.14);
  }
  cairo_restore(cr);
  outline();
  stroke(cr, kInk, 2.6);
  // The sunset along its back.
  cairo_move_to(cr, xu(0.08), cy - envHalf(0.08, ry) + 3);
  for (int i = 9; i <= 88; ++i)
  {
    const double u = double(i) / 90.0;
    cairo_line_to(cr, xu(u), cy - envHalf(u, ry) + 3);
  }
  stroke(cr, kRim, 3.0, 0.8);
  // The side fin, in front.
  cairo_move_to(cr, xu(fin * 0.55), cy - 3);
  cairo_line_to(cr, xu(-fin * 0.12), cy - 15);
  cairo_line_to(cr, xu(-fin * 0.16), cy + 17);
  cairo_line_to(cr, xu(fin * 0.55), cy + 7);
  cairo_close_path(cr);
  setRgba(cr, lerpColor(pal.accent, rgb(40, 30, 50), 0.35f), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.8);
  // The nose cap.
  circle(cr, xu(1) - 3, cy, 9);
  lgrad(cr, 0, cy - 9, 0, cy + 9, {{0.0, kBrassHi}, {1.0, kBrassLo}}, 1.0, true);
  stroke(cr, kInk, 1.5);
}

// The hull under a deck, `w` x `d` px: planked like the hull blocks, its
// lower corners rounded (`rs` at the stern on the left, `rb` at the bow),
// shaded toward the keel, copper sheathing along the bottom, a row of lit
// portholes. The Looter's is patched, darker, a skull on its bow.
void paintHull(cairo_t* cr, double w, double d, double rs, double rb, bool portholes, bool looter)
{
  auto shape = [&] {
    cairo_move_to(cr, 0, 0);
    cairo_line_to(cr, w, 0);
    cairo_line_to(cr, w, d - rb);
    cairo_curve_to(cr, w, d - rb * 0.3, w - rb * 0.3, d, w - rb, d);
    cairo_line_to(cr, rs, d);
    cairo_curve_to(cr, rs * 0.3, d, 0, d - rs * 0.3, 0, d - rs);
    cairo_close_path(cr);
  };
  shape();
  cairo_pattern_t* planks = fleetHullPattern();
  cairo_set_source(cr, planks);
  cairo_fill(cr);
  cairo_pattern_destroy(planks);
  // Round it: warm along the top, shaded toward the keel.
  cairo_save(cr);
  shape();
  cairo_clip(cr);
  {
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 0, 0, d);
    stop(g, 0.0, rgb(255, 190, 140), 0.12);
    stop(g, 0.15, rgb(0, 0, 0), 0.0);
    stop(g, 0.7, rgb(30, 10, 30), 0.25);
    stop(g, 1.0, rgb(30, 10, 40), 0.55);
    cairo_rectangle(cr, 0, 0, w, d);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  if (looter)
  {
    cairo_rectangle(cr, 0, 0, w, d);
    setRgba(cr, rgb(30, 20, 26), 0.35);
    cairo_fill(cr);
    // Patches nailed over holes.
    Rng rng(34201u);
    for (int i = 0; i < 5; ++i)
    {
      const double px = rng.range(20.0f, float(w) - 70.0f), py = rng.range(30.0f, float(d) - 70.0f);
      cairo_rectangle(cr, px, py, rng.range(30.0f, 50.0f), rng.range(16.0f, 26.0f));
      setRgba(cr, i % 2 ? rgb(150, 110, 70) : rgb(110, 90, 80), 1.0);
      cairo_fill_preserve(cr);
      stroke(cr, kInk, 1.2);
    }
    // The skull and crossbones on the bow.
    const double sx = w - 70, sy = d * 0.4;
    for (const double s : {-1.0, 1.0})
    {
      cairo_move_to(cr, sx - 22, sy + s * 16);
      cairo_line_to(cr, sx + 22, sy - s * 16);
      stroke(cr, rgb(240, 232, 214), 6.0);
    }
    circle(cr, sx, sy - 6, 15);
    setRgba(cr, rgb(240, 232, 214), 1.0);
    cairo_fill(cr);
    cairo_rectangle(cr, sx - 8, sy + 4, 16, 10);
    cairo_fill(cr);
    for (const double ex : {sx - 6, sx + 6})
    {
      circle(cr, ex, sy - 6, 4);
      setRgba(cr, kInk, 1.0);
      cairo_fill(cr);
    }
  }
  cairo_restore(cr);
  // The brass strake under the deck.
  cairo_rectangle(cr, 0, 30, w, 5);
  lgrad(cr, 0, 30, 0, 35, {{0.0, kBrassHi}, {1.0, kBrassLo}});
  // Copper sheathing along the bottom, gone green in places.
  cairo_move_to(cr, w, d - rb - 8);
  cairo_curve_to(cr, w, d - rb * 0.3, w - rb * 0.3, d, w - rb, d);
  cairo_line_to(cr, rs, d);
  cairo_curve_to(cr, rs * 0.3, d, 0, d - rs * 0.3, 0, d - rs - 8);
  stroke(cr, kInk, 16.0);
  cairo_move_to(cr, w, d - rb - 8);
  cairo_curve_to(cr, w, d - rb * 0.3, w - rb * 0.3, d, w - rb, d);
  cairo_line_to(cr, rs, d);
  cairo_curve_to(cr, rs * 0.3, d, 0, d - rs * 0.3, 0, d - rs - 8);
  setRgba(cr, looter ? rgb(110, 80, 60) : rgb(190, 112, 60), 1.0);
  cairo_set_line_width(cr, 11.0);
  cairo_stroke(cr);
  if (!looter)
    for (double x = rs + 30; x < w - rb - 20; x += 90)
    {
      ellipse(cr, x, d - 2, 22, 3.5);
      setRgba(cr, rgb(110, 170, 140), 0.6);
      cairo_fill(cr);
    }
  shape();
  stroke(cr, kInk, 2.4);
  if (portholes)
    for (double x = 70; x < w - 50; x += 112)
    {
      const double y = d * 0.58;
      glow(cr, x, y, 30, kLamp, 0.35);
      circle(cr, x, y, 13);
      lgrad(cr, x - 13, y - 13, x + 13, y + 13, {{0.0, kBrassHi}, {0.5, kBrass}, {1.0, kBrassLo}}, 1.0, true);
      stroke(cr, kInk, 1.4);
      circle(cr, x, y, 8.5);
      lgrad(cr, 0, y - 9, 0, y + 9, {{0.0, rgb(255, 246, 210)}, {1.0, rgb(255, 180, 90)}}, 1.0, true);
      stroke(cr, kInk, 1.0);
      cairo_move_to(cr, x - 4, y - 3);
      cairo_line_to(cr, x - 1, y - 6);
      stroke(cr, rgb(255, 255, 255), 1.4, 0.9);
    }
}

// A brass name board `w` px wide, its lettering drawn live.
void paintNameBoard(cairo_t* cr, double w)
{
  const double h = 34;
  cairo_move_to(cr, 8, 2);
  cairo_line_to(cr, w - 8, 2);
  cairo_curve_to(cr, w - 2, 2, w - 2, h - 2, w - 8, h - 2);
  cairo_line_to(cr, 8, h - 2);
  cairo_curve_to(cr, 2, h - 2, 2, 2, 8, 2);
  cairo_close_path(cr);
  lgrad(cr, 0, 2, 0, h - 2, {{0.0, kBrassHi}, {0.35, kBrass}, {1.0, kBrassLo}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  cairo_rectangle(cr, 10, 6, w - 20, h - 12);
  stroke(cr, kBrassLo, 1.0, 0.8);
  for (const double x : {7.0, w - 7.0})
    rivet(cr, x, h * 0.5, 2.0);
}

// The engine at the stern: a brass nacelle on a strut, 90 x 60, the hub at
// (14, 30) (the propeller is drawn live).
void paintNacelle(cairo_t* cr)
{
  cairo_move_to(cr, 40, 30);
  cairo_line_to(cr, 88, 22);
  cairo_move_to(cr, 40, 34);
  cairo_line_to(cr, 88, 48);
  stroke(cr, kInk, 6.0);
  cairo_move_to(cr, 40, 30);
  cairo_line_to(cr, 88, 22);
  cairo_move_to(cr, 40, 34);
  cairo_line_to(cr, 88, 48);
  stroke(cr, rgb(110, 100, 96), 3.0);
  cairo_move_to(cr, 14, 30);
  cairo_curve_to(cr, 18, 18, 30, 16, 52, 18);
  cairo_curve_to(cr, 62, 20, 62, 40, 52, 42);
  cairo_curve_to(cr, 30, 44, 18, 42, 14, 30);
  cairo_close_path(cr);
  lgrad(cr, 0, 16, 0, 44, {{0.0, kBrassHi}, {0.4, kBrass}, {1.0, kBrassLo}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  for (const double x : {28.0, 40.0})
  {
    cairo_move_to(cr, x, 18);
    cairo_line_to(cr, x, 42);
    stroke(cr, kBrassLo, 1.4);
  }
  // Exhaust stubs.
  cairo_rectangle(cr, 34, 10, 6, 8);
  setRgba(cr, rgb(60, 54, 60), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.0);
  circle(cr, 14, 30, 5);
  setRgba(cr, rgb(70, 64, 70), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.2);
}

// A propeller seen edge on at (x, y): a blur disc and two blades turning.
void propeller(Renderer& r, float x, float y, float rad, int frame, float speed)
{
  const float a = float(frame) * speed;
  r.fillRect(x - 3.0f, y - rad, 6.0f, rad * 2.0f, rgba(230, 220, 210, 50));
  for (int k = 0; k < 2; ++k)
  {
    const float c = std::cos(a + float(k) * 3.14159f);
    const float tip = y + c * rad;
    r.drawLine(x, y, x + std::sin(a + float(k) * 3.14159f) * 4.0f, tip, 7.0f, rgba(40, 24, 20, 230));
    r.drawLine(x, y, x + std::sin(a + float(k) * 3.14159f) * 4.0f, tip, 4.0f, rgb(150, 100, 60));
  }
  r.fillRect(x - 4.0f, y - 4.0f, 8.0f, 8.0f, kBrass);
}

// The Looter's Jolly Roger, 70 x 50, its luff at x 0; `frame` 0/1 flaps.
void paintFlag(cairo_t* cr, int frame)
{
  const double wv = frame ? 5.0 : -5.0;
  cairo_move_to(cr, 0, 4);
  cairo_curve_to(cr, 20, 4 + wv, 44, 4 - wv, 66, 6 + wv * 0.5);
  cairo_line_to(cr, 64, 44 + wv * 0.5);
  cairo_curve_to(cr, 44, 42 - wv, 20, 42 + wv, 0, 42);
  cairo_close_path(cr);
  setRgba(cr, rgb(24, 20, 24), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.5);
  circle(cr, 30, 19, 8);
  setRgba(cr, rgb(240, 232, 214), 1.0);
  cairo_fill(cr);
  for (const double s : {-1.0, 1.0})
  {
    cairo_move_to(cr, 18, 30 + s * 6);
    cairo_line_to(cr, 42, 30 - s * 6);
    stroke(cr, rgb(240, 232, 214), 3.0);
  }
  for (const double ex : {27.0, 33.0})
  {
    circle(cr, ex, 19, 2.2);
    setRgba(cr, rgb(24, 20, 24), 1.0);
    cairo_fill(cr);
  }
}

// The bomb bay's inside: a dark hold, its ribs, a lantern, two fat bombs in
// their rack and a crate.
void paintBay(cairo_t* cr, double w, double h)
{
  cairo_rectangle(cr, 0, 0, w, h);
  lgrad(cr, 0, 0, 0, h, {{0.0, rgb(70, 40, 30)}, {1.0, rgb(30, 16, 14)}});
  for (double x = 0; x < w; x += 22)
  {
    cairo_move_to(cr, x, 0);
    cairo_line_to(cr, x, h);
    stroke(cr, rgb(20, 10, 10), 1.2, 0.7);
  }
  for (double x = 40; x < w; x += 128)
  {
    cairo_rectangle(cr, x, 0, 14, h);
    lgrad(cr, x, 0, x + 14, 0, {{0.0, rgb(110, 64, 40)}, {1.0, rgb(50, 28, 20)}});
  }
  glow(cr, w * 0.5, 26, 120, kLamp, 0.35);
  // The lantern.
  cairo_move_to(cr, w * 0.5, 0);
  cairo_line_to(cr, w * 0.5, 16);
  stroke(cr, kInk, 2.0);
  cairo_rectangle(cr, w * 0.5 - 7, 16, 14, 18);
  setRgba(cr, kLamp, 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kBrassLo, 2.0);
  // The bomb rack: a beam, two bombs on shackles.
  cairo_rectangle(cr, w - 170, 30, 140, 8);
  setRgba(cr, rgb(90, 90, 100), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.2);
  for (const double bx : {w - 140, w - 70})
  {
    cairo_move_to(cr, bx, 38);
    cairo_line_to(cr, bx, 48);
    stroke(cr, kInk, 2.0);
    ellipse(cr, bx, 76, 22, 28);
    lgrad(cr, bx - 22, 48, bx + 22, 104, {{0.0, rgb(120, 126, 140)}, {0.5, rgb(50, 54, 64)}, {1.0, rgb(20, 20, 26)}}, 1.0, true);
    stroke(cr, kInk, 1.6);
    cairo_move_to(cr, bx - 16, 108);
    cairo_line_to(cr, bx, 98);
    cairo_line_to(cr, bx + 16, 108);
    stroke(cr, kInk, 4.0);
    cairo_rectangle(cr, bx - 22, 70, 44, 6);
    setRgba(cr, rgb(220, 60, 50), 0.9);
    cairo_fill(cr);
  }
  // A crate in the corner.
  cairo_rectangle(cr, 18, h - 64, 60, 60);
  lgrad(cr, 0, h - 64, 0, h, {{0.0, rgb(170, 120, 70)}, {1.0, rgb(100, 66, 36)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  cairo_move_to(cr, 18, h - 64);
  cairo_line_to(cr, 78, h - 4);
  cairo_move_to(cr, 78, h - 64);
  cairo_line_to(cr, 18, h - 4);
  stroke(cr, rgb(80, 50, 26), 3.0);
}

// A brass ring, 80 x 80, its middle at (40, 40).
void paintRing(cairo_t* cr)
{
  glow(cr, 40, 40, 38, rgb(255, 220, 140), 0.3);
  circle(cr, 40, 40, 24);
  stroke(cr, kInk, 11.0);
  circle(cr, 40, 40, 24);
  cairo_pattern_t* g = cairo_pattern_create_linear(16, 16, 64, 64);
  stop(g, 0.0, kBrassHi);
  stop(g, 0.45, kBrass);
  stop(g, 1.0, kBrassLo);
  cairo_set_source(cr, g);
  cairo_set_line_width(cr, 7.0);
  cairo_stroke(cr);
  cairo_pattern_destroy(g);
  cairo_arc(cr, 40, 40, 24, kPi * 1.1, kPi * 1.6);
  stroke(cr, rgb(255, 255, 230), 2.2, 0.9);
}

// A small tether balloon, 60 x 70, red and white, its knot at (30, 64).
void paintTether(cairo_t* cr)
{
  ellipse(cr, 30, 30, 24, 28);
  cairo_save(cr);
  cairo_clip_preserve(cr);
  setRgba(cr, rgb(240, 236, 226), 1.0);
  cairo_fill(cr);
  for (int k = 0; k < 3; ++k)
  {
    cairo_move_to(cr, 30, 0);
    cairo_curve_to(cr, 6 + k * 18.0, 14, 6 + k * 18.0, 46, 30, 60);
    cairo_curve_to(cr, 14 + k * 18.0, 46, 14 + k * 18.0, 14, 30, 0);
    setRgba(cr, rgb(210, 60, 60), 1.0);
    cairo_fill(cr);
  }
  glow(cr, 22, 18, 20, rgb(255, 240, 220), 0.6);
  cairo_restore(cr);
  ellipse(cr, 30, 30, 24, 28);
  stroke(cr, kInk, 2.0);
  cairo_move_to(cr, 26, 58);
  cairo_line_to(cr, 34, 58);
  cairo_line_to(cr, 30, 64);
  cairo_close_path(cr);
  setRgba(cr, rgb(170, 50, 50), 1.0);
  cairo_fill(cr);
}

// The captain's log: a leather-bound book with brass corners and a red
// ribbon, 96 x 96, its middle at (48, 48).
void paintLog(cairo_t* cr)
{
  cairo_save(cr);
  cairo_translate(cr, 48, 50);
  cairo_rotate(cr, -0.12);
  cairo_rectangle(cr, -28, -32, 56, 62);
  setRgba(cr, rgb(250, 240, 210), 1.0);
  cairo_fill(cr);
  cairo_rectangle(cr, -30, -34, 56, 62);
  lgrad(cr, -30, -34, 26, 28, {{0.0, rgb(150, 70, 40)}, {1.0, rgb(80, 30, 20)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  cairo_rectangle(cr, -30, -34, 8, 62);
  setRgba(cr, rgb(60, 24, 16), 1.0);
  cairo_fill(cr);
  for (const auto& c : {std::array<double, 2>{26, -34}, {26, 28}})
  {
    cairo_move_to(cr, c[0], c[1]);
    cairo_line_to(cr, c[0] - 10, c[1]);
    cairo_line_to(cr, c[0], c[1] + (c[1] < 0 ? 10 : -10));
    cairo_close_path(cr);
    setRgba(cr, kBrass, 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.0);
  }
  // A compass rose stamped in gold on its cover.
  circle(cr, 2, -4, 13);
  stroke(cr, kBrass, 2.0);
  for (int k = 0; k < 4; ++k)
  {
    const double a = k * kPi / 2.0;
    cairo_move_to(cr, 2 + std::cos(a) * 15, -4 + std::sin(a) * 15);
    cairo_line_to(cr, 2 + std::cos(a + 0.4) * 4, -4 + std::sin(a + 0.4) * 4);
    cairo_line_to(cr, 2 + std::cos(a - 0.4) * 4, -4 + std::sin(a - 0.4) * 4);
    cairo_close_path(cr);
    setRgba(cr, kBrassHi, 1.0);
    cairo_fill(cr);
  }
  // The ribbon.
  cairo_move_to(cr, 12, 28);
  cairo_line_to(cr, 14, 42);
  cairo_line_to(cr, 18, 38);
  cairo_line_to(cr, 21, 44);
  cairo_line_to(cr, 19, 28);
  cairo_close_path(cr);
  setRgba(cr, rgb(200, 30, 40), 1.0);
  cairo_fill(cr);
  cairo_restore(cr);
}

// A soft heap of cloud, 260 x 150, lit warm on top.
void paintPuff(cairo_t* cr)
{
  static constexpr std::array<std::array<double, 3>, 6> kP{
    {{60, 90, 46}, {110, 70, 58}, {170, 76, 52}, {210, 98, 40}, {130, 104, 50}, {80, 110, 40}}};
  for (const auto& p : kP)
  {
    cairo_pattern_t* g = cairo_pattern_create_radial(p[0], p[1] - p[2] * 0.3, 0, p[0], p[1], p[2]);
    stop(g, 0.0, rgb(255, 250, 246), 1.0);
    stop(g, 0.7, rgb(250, 226, 230), 0.95);
    stop(g, 1.0, rgb(230, 200, 220), 0.0);
    circle(cr, p[0], p[1], p[2]);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
}

// --- Painters: Human Cannonball --------------------------------------------------------------------

// The cannon's barrel: 190 x 70, the trunnion (the pivot) at (40, 35),
// pointing right: red, gold bands and stars, a flared muzzle.
void paintBarrel(cairo_t* cr)
{
  cairo_move_to(cr, 14, 18);
  cairo_line_to(cr, 168, 22);
  cairo_line_to(cr, 184, 12);
  cairo_line_to(cr, 188, 12);
  cairo_line_to(cr, 188, 58);
  cairo_line_to(cr, 184, 58);
  cairo_line_to(cr, 168, 48);
  cairo_line_to(cr, 14, 52);
  cairo_curve_to(cr, 2, 50, 2, 20, 14, 18);
  cairo_close_path(cr);
  lgrad(cr, 0, 12, 0, 58, {{0.0, rgb(255, 120, 110)}, {0.35, rgb(214, 40, 44)}, {1.0, rgb(110, 14, 24)}}, 1.0, true);
  stroke(cr, kInk, 2.4);
  for (const double x : {22.0, 70.0, 120.0, 166.0})
  {
    cairo_rectangle(cr, x, 15 + (x - 14) * 0.026, 7, 40 - (x - 14) * 0.052);
    lgrad(cr, 0, 16, 0, 54, {{0.0, kBrassHi}, {1.0, kBrassLo}}, 1.0, true);
    stroke(cr, kInk, 1.0);
  }
  // Stars.
  for (const double x : {46.0, 96.0, 144.0})
  {
    cairo_move_to(cr, x + 8 * std::cos(-kPi / 2), 33 + 8 * std::sin(-kPi / 2));
    for (int k = 1; k < 10; ++k)
    {
      const double a = -kPi / 2 + k * kPi / 5.0, rr = k % 2 ? 3.4 : 8.0;
      cairo_line_to(cr, x + rr * std::cos(a), 33 + rr * std::sin(a));
    }
    cairo_close_path(cr);
    setRgba(cr, rgb(255, 230, 120), 1.0);
    cairo_fill(cr);
  }
  ellipse(cr, 186, 35, 4, 22);
  setRgba(cr, rgb(30, 10, 14), 1.0);
  cairo_fill(cr);
  cairo_move_to(cr, 16, 22);
  cairo_line_to(cr, 166, 25);
  stroke(cr, rgb(255, 210, 200), 2.0, 0.7);
}

// The carriage: 200 x 130, the trunnion at (100, 40), the floor at 130:
// a gold-trimmed red wagon on two big spoked wheels.
void paintCarriage(cairo_t* cr)
{
  cairo_move_to(cr, 40, 100);
  cairo_line_to(cr, 76, 34);
  cairo_line_to(cr, 124, 34);
  cairo_line_to(cr, 168, 100);
  cairo_close_path(cr);
  lgrad(cr, 0, 34, 0, 100, {{0.0, rgb(60, 80, 170)}, {1.0, rgb(24, 30, 80)}}, 1.0, true);
  stroke(cr, kInk, 2.2);
  cairo_move_to(cr, 48, 96);
  cairo_line_to(cr, 80, 40);
  cairo_line_to(cr, 120, 40);
  cairo_line_to(cr, 160, 96);
  stroke(cr, kBrass, 2.0);
  circle(cr, 100, 40, 9);
  lgrad(cr, 0, 31, 0, 49, {{0.0, kBrassHi}, {1.0, kBrassLo}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  for (const double wx : {58.0, 150.0})
  {
    circle(cr, wx, 100, 29);
    setRgba(cr, kInk, 1.0);
    cairo_fill(cr);
    circle(cr, wx, 100, 25);
    setRgba(cr, rgb(240, 200, 80), 1.0);
    cairo_set_line_width(cr, 6.0);
    cairo_stroke(cr);
    for (int k = 0; k < 8; ++k)
    {
      const double a = k * kPi / 4.0;
      cairo_move_to(cr, wx, 100);
      cairo_line_to(cr, wx + std::cos(a) * 23, 100 + std::sin(a) * 23);
    }
    stroke(cr, rgb(200, 40, 44), 3.0);
    circle(cr, wx, 100, 6);
    setRgba(cr, kBrass, 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.2);
  }
}

// A safety net, `w` px wide, on two poles down to the floor `h` px below
// its rim, the mesh sagging.
void paintNet(cairo_t* cr, double w, double h)
{
  const double x0 = 16, x1 = 16 + w, sag = 26;
  for (const double x : {x0, x1})
  {
    cairo_move_to(cr, x, 2);
    cairo_line_to(cr, x, h + 6);
    stroke(cr, kInk, 8.0);
    cairo_move_to(cr, x, 2);
    cairo_line_to(cr, x, h + 6);
    stroke(cr, rgb(230, 230, 236), 4.0);
    for (double y = 14; y < h; y += 26)
    {
      cairo_move_to(cr, x - 2, y);
      cairo_line_to(cr, x + 2, y + 10);
      stroke(cr, rgb(214, 40, 44), 3.0);
    }
    circle(cr, x, 4, 5);
    setRgba(cr, kBrass, 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.2);
  }
  // The mesh: lines along it and across it, sagging in the middle.
  auto sagAt = [&](double x) { return 8 + sag * std::sin((x - x0) / w * kPi); };
  cairo_move_to(cr, x0, 8);
  for (double x = x0; x <= x1; x += 4)
    cairo_line_to(cr, x, sagAt(x));
  stroke(cr, kInk, 4.0);
  for (int k = 0; k < 4; ++k)
  {
    cairo_move_to(cr, x0, 8 + k * 2.0);
    for (double x = x0; x <= x1; x += 4)
      cairo_line_to(cr, x, sagAt(x) * (0.25 + k * 0.25));
    stroke(cr, rgb(250, 248, 240), 1.2, 0.85);
  }
  for (double x = x0 + 10; x < x1; x += 12)
  {
    cairo_move_to(cr, x, sagAt(x) * 0.25);
    cairo_line_to(cr, x + 6, sagAt(x + 6));
    cairo_move_to(cr, x + 6, sagAt(x + 6) * 0.25);
    cairo_line_to(cr, x, sagAt(x));
    stroke(cr, rgb(250, 248, 240), 1.0, 0.75);
  }
  cairo_move_to(cr, x0, 8);
  for (double x = x0; x <= x1; x += 4)
    cairo_line_to(cr, x, sagAt(x));
  stroke(cr, rgb(214, 40, 44), 2.4);
  cairo_move_to(cr, x0, 6);
  cairo_line_to(cr, x1, 6);
  stroke(cr, rgb(214, 40, 44), 3.0);
}

// A score board for a net: a circus placard, the bullseye's with a target
// painted on it. 170 x 110, its foot at (85, 110).
void paintBoard(cairo_t* cr, int score)
{
  const bool bull = score >= 5000;
  cairo_move_to(cr, 16, 8);
  cairo_line_to(cr, 154, 8);
  cairo_curve_to(cr, 166, 30, 166, 50, 154, 72);
  cairo_line_to(cr, 16, 72);
  cairo_curve_to(cr, 4, 50, 4, 30, 16, 8);
  cairo_close_path(cr);
  lgrad(cr, 0, 8, 0, 72, {{0.0, bull ? rgb(255, 90, 80) : rgb(250, 240, 220)}, {1.0, bull ? rgb(170, 20, 30) : rgb(220, 200, 170)}},
    1.0, true);
  stroke(cr, kInk, 2.4);
  cairo_move_to(cr, 18, 13);
  cairo_line_to(cr, 152, 13);
  cairo_curve_to(cr, 161, 30, 161, 50, 152, 67);
  cairo_line_to(cr, 18, 67);
  cairo_curve_to(cr, 9, 50, 9, 30, 18, 13);
  cairo_close_path(cr);
  stroke(cr, kBrass, 2.0);
  // Bulbs round it.
  for (int k = 0; k < 9; ++k)
  {
    circle(cr, 20 + k * 16.25, 8, 3);
    setRgba(cr, rgb(255, 240, 180), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 0.8);
  }
  if (bull)
  {
    for (int k = 3; k >= 1; --k)
    {
      circle(cr, 36, 40, k * 7.0);
      setRgba(cr, k % 2 ? rgb(255, 255, 255) : rgb(214, 30, 40), 1.0);
      cairo_fill(cr);
    }
  }
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%d", score);
  lettering(cr, buf, bull ? 100 : 85, 52, 30, bull ? rgb(255, 250, 220) : rgb(150, 30, 40));
}

// A string of pennants between (x0, y0) and (x1, y1), `n` flags.
void bunting(Renderer& r, float x0, float y0, float x1, float y1, int n, int frame)
{
  r.drawLine(x0, y0, x1, y1, 2.0f, rgb(60, 40, 40));
  static constexpr std::array<Color, 4> kFlag{rgb(214, 40, 44), rgb(250, 220, 90), rgb(60, 110, 200), rgb(250, 248, 240)};
  for (int k = 0; k < n; ++k)
  {
    const float u = (float(k) + 0.5f) / float(n);
    const float sag = std::sin(u * 3.14159f) * 40.0f;
    const float x = x0 + (x1 - x0) * u, y = y0 + (y1 - y0) * u + sag;
    const float wv = std::sin(float(frame) * 0.08f + float(k)) * 3.0f;
    for (int s = 0; s < 6; ++s)
    {
      const float t = float(s) / 6.0f;
      r.fillRect(x - 12.0f + t * 12.0f + wv * t, y + t * 26.0f, 24.0f * (1.0f - t), 26.0f / 6.0f + 0.5f, kFlag[std::size_t(k % 4)]);
    }
  }
}

// --- The Grapple Gun -------------------------------------------------------------------------------

// The hook: three prongs on a shank, 56 x 56, its eye at (12, 28), pointing
// right.
void paintHook(cairo_t* cr)
{
  for (int pass = 0; pass < 2; ++pass)
  {
    const Color c = pass == 0 ? kInk : rgb(170, 176, 190);
    const double wd = pass == 0 ? 7.0 : 4.0;
    cairo_move_to(cr, 14, 28);
    cairo_line_to(cr, 44, 28);
    stroke(cr, c, wd);
    for (const double s : {-1.0, 1.0})
    {
      cairo_move_to(cr, 44, 28);
      cairo_curve_to(cr, 48, 28 + s * 14, 40, 28 + s * 22, 32, 28 + s * 16);
      stroke(cr, c, wd);
    }
    cairo_move_to(cr, 44, 28);
    cairo_line_to(cr, 52, 28);
    stroke(cr, c, wd);
  }
  circle(cr, 11, 28, 5);
  stroke(cr, kInk, 4.0);
  circle(cr, 11, 28, 5);
  stroke(cr, kBrass, 2.4);
  cairo_move_to(cr, 16, 26);
  cairo_line_to(cr, 42, 26);
  stroke(cr, rgb(255, 255, 255), 1.0, 0.7);
}

// The Grapple Gun's icon, 64 x 64: a brass line gun on a wooden grip, the
// rope drum on top, the hook at its muzzle.
void paintGrapple(cairo_t* cr)
{
  // The grip.
  cairo_move_to(cr, 14, 34);
  cairo_line_to(cr, 26, 34);
  cairo_line_to(cr, 22, 56);
  cairo_line_to(cr, 10, 54);
  cairo_close_path(cr);
  lgrad(cr, 10, 0, 26, 0, {{0.0, rgb(150, 86, 46)}, {1.0, rgb(90, 50, 26)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  // The barrel.
  cairo_rectangle(cr, 8, 26, 40, 11);
  lgrad(cr, 0, 26, 0, 37, {{0.0, kBrassHi}, {0.4, kBrass}, {1.0, kBrassLo}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  cairo_rectangle(cr, 44, 24, 6, 15);
  setRgba(cr, kBrassLo, 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.2);
  // The trigger guard.
  cairo_arc(cr, 30, 40, 5, 0, kPi);
  stroke(cr, kInk, 1.6);
  // The rope drum.
  circle(cr, 24, 20, 9);
  lgrad(cr, 0, 11, 0, 29, {{0.0, rgb(230, 206, 150)}, {1.0, rgb(150, 120, 70)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  for (int k = -2; k <= 2; ++k)
  {
    cairo_move_to(cr, 17, 20 + k * 3.0);
    cairo_line_to(cr, 31, 20 + k * 3.0 + 1.0);
    stroke(cr, rgb(120, 90, 50), 0.8);
  }
  circle(cr, 24, 20, 2.4);
  setRgba(cr, kBrass, 1.0);
  cairo_fill(cr);
  // The line out to the hook at the muzzle.
  cairo_move_to(cr, 30, 16);
  cairo_curve_to(cr, 40, 12, 48, 18, 52, 30);
  stroke(cr, kRope, 1.6);
  cairo_save(cr);
  cairo_translate(cr, 48, 31);
  cairo_scale(cr, 0.3, 0.3);
  cairo_translate(cr, -12, -28);
  paintHook(cr);
  cairo_restore(cr);
}

// --- Geometry --------------------------------------------------------------------------------------

constexpr float kHullD = 11.0f * kCellPx; // px: a hull's depth under its deck

bool isBayShip(const FleetState& s, const Ship& sh)
{
  return s.bayX0 >= 0 && s.bayX0 >= sh.baseX && s.bayX1 < sh.baseX + sh.deckW();
}

float hullDepth(const FleetState& s, const Ship& sh)
{
  // The bomb bay's ship is its hold: its hull ends with the bay's floor.
  if (isBayShip(s, sh))
    return float(s.bayY + 2 - sh.baseY) * kCellPx;
  return kHullD;
}

// The deck's top-left now (screen px), at the in-between time; false if
// the ship is gone (the Looter under the clouds).
bool shipAt(const World& w, const Ship& sh, float alpha, float camX, float camY, float& x, float& y)
{
  float dx = float(sh.baseX), dy = float(sh.baseY);
  if (sh.platform >= 0)
  {
    const auto& pl = w.platforms()[std::size_t(sh.platform)];
    if (pl.hidden)
      return false;
    dx = lerpF(pl.prevX, pl.x, alpha);
    dy = lerpF(pl.prevY, pl.y, alpha);
  }
  x = dx * kCellPx - camX;
  y = dy * kCellPx - camY;
  return true;
}

// The ships take the palettes in turn, so neighbours never match.
int paletteOf(std::size_t index) { return int(index % kPalettes.size()); }

} // namespace

// --- Behind the tiles ------------------------------------------------------------------------------

void drawFleetSet(Renderer& r, const World& w, const Art& art, float camX, float camY, int frame, float alpha)
{
  const auto& s = w.fleet();
  if (s.cannonball)
  {
    // Human Cannonball: bunting from the masts and the banner over it all.
    const float top = 2.0f * kTilePx - camY;
    const float left = kTilePx - camX, right = float(w.level().width - 1) * kTilePx - camX;
    for (int k = 0; k < 4; ++k)
    {
      const float x0 = left + (right - left) * float(k) / 4.0f;
      bunting(r, x0, top + float(k % 2) * 30.0f, x0 + (right - left) / 4.0f, top + float((k + 1) % 2) * 30.0f, 9, frame);
    }
    const float bx = (left + right) * 0.5f, by = top + 160.0f;
    if (!offScreen(bx - 300.0f, by - 60.0f, 600.0f, 120.0f))
    {
      r.drawLine(bx - 260.0f, top + 20.0f, bx - 240.0f, by - 40.0f, 3.0f, rgb(60, 40, 40));
      r.drawLine(bx + 260.0f, top + 20.0f, bx + 240.0f, by - 40.0f, 3.0f, rgb(60, 40, 40));
      r.draw(baked(art, r, "banner", 560, 100, 280, 50, [](cairo_t* cr) {
        cairo_move_to(cr, 10, 8);
        cairo_curve_to(cr, 200, 20, 360, 20, 550, 8);
        cairo_line_to(cr, 530, 50);
        cairo_line_to(cr, 550, 92);
        cairo_curve_to(cr, 360, 80, 200, 80, 10, 92);
        cairo_line_to(cr, 30, 50);
        cairo_close_path(cr);
        lgrad(cr, 0, 8, 0, 92, {{0.0, rgb(240, 70, 70)}, {1.0, rgb(150, 20, 30)}}, 1.0, true);
        stroke(cr, kInk, 3.0);
        lettering(cr, "THE HUMAN CANNONBALL", 280, 62, 34, rgb(255, 230, 140));
      }),
        bx, by);
    }
    return;
  }
  for (std::size_t i = 0; i < s.ships.size(); ++i)
  {
    const Ship& sh = s.ships[i];
    float x = 0.0f, y = 0.0f;
    if (!shipAt(w, sh, alpha, camX, camY, x, y))
      continue;
    const float W = float(sh.deckW()) * kCellPx;
    const float D = hullDepth(s, sh);
    if (offScreen(x - 160.0f, y - kEnvRise * kCellPx - 40.0f, W + 320.0f, kEnvRise * kCellPx + D + 120.0f))
      continue;
    const Palette& pal = kPalettes[std::size_t(paletteOf(i))];
    const bool sinking = sh.looter && sh.looterT >= 0;
    // The balloon on its rigging.
    if (sh.balloon)
    {
      const float len = W + 64.0f;
      const float ex = x - 32.0f, ey = y - kEnvRise * kCellPx;
      const double ry = kEnvH * 0.5;
      const int n = std::max(3, int(W / 150.0f));
      for (int k = 0; k <= n; ++k)
      {
        const float u = 0.16f + 0.7f * float(k) / float(n);
        const float tx = ex + u * len, ty = ey + float(ry + envHalf(u, ry)) - 6.0f;
        const float bx = x + 20.0f + (W - 40.0f) * float(k) / float(n);
        r.drawLine(tx, ty, bx, y - kRailH, 2.0f, rgba(60, 36, 36, 220));
        if (k < n)
          r.drawLine(tx, ty, x + 20.0f + (W - 40.0f) * float(k + 1) / float(n), y - kRailH, 1.0f, rgba(60, 36, 36, 140));
      }
      const std::string key = "env/" + std::to_string(int(len)) + "/" + std::to_string(paletteOf(i));
      r.draw(baked(art, r, key, int(len) + 160, int(kEnvH) + 52, 120.0f, 26.0f,
               [len, pal](cairo_t* cr) { paintEnvelope(cr, len, pal); }),
        ex, ey);
      // Pecked: wrinkles and the air hissing out where the parrot pecks.
      if (sh.sinkMax > 0 && sh.sink > 0)
      {
        const float k = float(sh.sink) / float(sh.sinkMax);
        for (int q = 0; q < 4; ++q)
        {
          const float wx = ex + len * (0.35f + 0.1f * float(q));
          r.drawLine(wx, ey + 30.0f, wx + 14.0f, ey + 100.0f * k + 40.0f, 2.0f, rgba(60, 40, 60, int(90.0f * k)));
        }
      }
    }
    // The hull, its engine and propeller at the stern.
    const bool bay = isBayShip(s, sh), looter = sh.looter;
    const std::string hkey = "hull/" + std::to_string(int(W)) + "x" + std::to_string(int(D)) + (looter ? "L" : "") + (bay ? "B" : "");
    const float rs = bay ? 12.0f : std::min(110.0f, D * 0.4f), rb = bay ? 12.0f : std::min(170.0f, D * 0.6f);
    r.draw(baked(art, r, hkey, int(W), int(D) + 4, 0.0f, 0.0f,
             [W, D, rs, rb, bay, looter](cairo_t* cr) { paintHull(cr, W, D, rs, rb, !bay && !looter, looter); }),
      x, y);
    if (bay)
    {
      // The bomb bay's hold, inside the hull (its walls and floor are tiles).
      const float bx = float(s.bayX0) * kCellPx - camX, by = float((s.hatchTy + 1) * kCellsPerTile) * kCellPx - camY;
      const float bw = float(s.bayX1 + 1 - s.bayX0) * kCellPx, bh = float(s.bayY) * kCellPx - camY - by;
      r.draw(baked(art, r, "bay/" + std::to_string(int(bw)) + "x" + std::to_string(int(bh)), int(bw), int(bh), 0, 0,
               [bw, bh](cairo_t* cr) { paintBay(cr, bw, bh); }),
        bx, by);
      drawGlow(r, art, bx + bw * 0.5f, by + 34.0f, 60.0f + 4.0f * std::sin(float(frame) * 0.2f), kLamp, 0.35f);
      if (s.hatchOpen)
        for (int k = 0; k < 4; ++k)
          r.fillRect(float(s.hatchTx) * kTilePx - camX + 6.0f + float(k) * 12.0f, by, 10.0f, bh, rgba(255, 220, 160, 22), Blend::Add);
    }
    if (!sh.looter)
    {
      const float ny = y + D * 0.42f;
      r.draw(baked(art, r, "nacelle", 92, 60, 88.0f, 30.0f, paintNacelle), x + 4.0f, ny);
      propeller(r, x - 84.0f + 14.0f, ny, 44.0f, frame, 0.9f);
    }
    // The deck where it moves (the steady ones' decks are tiles).
    if (sh.platform >= 0)
      for (float dx = 0.0f; dx < W - 1.0f; dx += kTilePx)
        r.draw(art.platform, x + dx, y);
    // Rails along the deck, a lantern at the bow.
    for (float rx = x + 12.0f; rx < x + W - 8.0f; rx += kTilePx)
    {
      r.fillRect(rx - 2.0f, y - kRailH, 5.0f, kRailH, rgba(40, 24, 20, 230));
      r.fillRect(rx - 1.0f, y - kRailH, 3.0f, kRailH, kBrass);
    }
    r.drawLine(x + 10.0f, y - kRailH, x + W - 10.0f, y - kRailH, 4.0f, rgba(40, 24, 20, 230));
    r.drawLine(x + 10.0f, y - kRailH, x + W - 10.0f, y - kRailH, 2.0f, kBrassHi);
    r.drawLine(x + 10.0f, y - kRailH * 0.5f, x + W - 10.0f, y - kRailH * 0.5f, 2.0f, kRope);
    if (!sh.looter)
    {
      const float lx = x + W - 14.0f;
      r.fillRect(lx - 2.0f, y - 64.0f, 4.0f, 64.0f, rgb(60, 40, 30));
      r.fillRect(lx - 7.0f, y - 78.0f, 14.0f, 16.0f, kLamp);
      r.fillRect(lx - 9.0f, y - 82.0f, 18.0f, 5.0f, kBrassLo);
      drawGlow(r, art, lx, y - 70.0f, 44.0f + 4.0f * std::sin(float(frame) * 0.13f + float(i)), kLamp, 0.55f);
    }
    else
    {
      // The Looter: two rotor masts and its Jolly Roger.
      const float spin = sinking ? std::max(0.15f, 0.9f - float(sh.looterT) * 0.004f) : 0.9f;
      for (const float mx : {x + 40.0f, x + W - 40.0f})
      {
        r.fillRect(mx - 3.0f, y - 140.0f, 6.0f, 140.0f, rgb(70, 54, 46));
        const float a = float(frame) * spin;
        const float span = 110.0f * std::fabs(std::cos(a));
        r.fillRect(mx - 110.0f, y - 146.0f, 220.0f, 6.0f, rgba(220, 210, 200, 40));
        r.drawLine(mx - span, y - 143.0f, mx + span, y - 143.0f, 7.0f, rgba(40, 24, 20, 230));
        r.drawLine(mx - span, y - 143.0f, mx + span, y - 143.0f, 4.0f, rgb(120, 110, 100));
        r.fillRect(mx - 6.0f, y - 150.0f, 12.0f, 12.0f, rgb(90, 80, 80));
      }
      const float fx = x + W * 0.5f;
      r.fillRect(fx - 2.0f, y - 120.0f, 4.0f, 120.0f, rgb(70, 54, 46));
      r.draw(baked(art, r, "flag" + std::to_string((frame / 10) % 2), 72, 50, 0, 0,
               [f = (frame / 10) % 2](cairo_t* cr) { paintFlag(cr, f); }),
        fx + 2.0f, y - 120.0f);
      if (sinking)
      {
        // Going down: black smoke pouring up off it.
        for (int k = 0; k < 10; ++k)
        {
          const float u = std::fmod(float(frame) * 0.012f + float(k) * 0.1f, 1.0f);
          const float px = x + W * (0.2f + 0.6f * unit(hash2(k, 3411))) + std::sin(u * 6.0f + float(k)) * 20.0f;
          const float py = y - 20.0f - u * 260.0f;
          drawGlow(r, art, px, py, 30.0f + u * 50.0f, rgb(40, 30, 34), 0.5f * (1.0f - u));
        }
      }
    }
  }
}

// --- Behind the runner -----------------------------------------------------------------------------

void World::drawFleetBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& s = mFleet;
  const auto& p = mPlayer;
  if (s.cannonball)
  {
    // The nets, their score boards.
    for (const auto& n : s.nets)
    {
      const float nx = float(n.x0) * kCellPx - camX, ny = float(n.y) * kCellPx - camY;
      const float nw = float(n.x1 + 1 - n.x0) * kCellPx;
      if (offScreen(nx - 40.0f, ny - 200.0f, nw + 80.0f, 400.0f))
        continue;
      const float floorY = float(mMap.height()) * kCellPx;
      int fy = n.y;
      while (fy < mMap.height() && !mMap.solidTop(n.x0, fy))
        ++fy;
      const float h = std::min(floorY, float(fy) * kCellPx) - float(n.y) * kCellPx;
      r.draw(baked(mArt, r, "net/" + std::to_string(int(nw)) + "x" + std::to_string(int(h)), int(nw) + 32, int(h) + 12, 16.0f, 6.0f,
               [nw, h](cairo_t* cr) { paintNet(cr, nw, h); }),
        nx, ny);
      const bool bull = n.score >= 5000;
      const float bx = nx + nw * 0.5f, by = ny - 170.0f; // high over the net, clear of the landing
      // Two striped poles hold it up off the net's posts.
      for (const float side : {-1.0f, 1.0f})
      {
        const float px = side < 0.0f ? nx : nx + nw, qx = bx + side * 58.0f, qy = by - 40.0f;
        r.drawLine(px, ny, qx, qy, 6.0f, kInk);
        r.drawLine(px, ny, qx, qy, 3.0f, rgb(236, 232, 236));
        for (int k = 1; k < 6; ++k)
        {
          const float u = float(k) / 6.0f;
          const float sx = px + (qx - px) * u, sy = ny + (qy - ny) * u;
          r.drawLine(sx, sy, sx + (qx - px) * 0.05f, sy + (qy - ny) * 0.05f, 3.0f, rgb(214, 40, 44));
        }
      }
      if (bull)
        drawGlow(r, mArt, bx, by - 60.0f, 110.0f + 10.0f * std::sin(float(frame) * 0.15f), rgb(255, 120, 90), 0.4f);
      r.draw(baked(mArt, r, "board/" + std::to_string(n.score), 170, 110, 85.0f, 110.0f,
               [sc = n.score](cairo_t* cr) { paintBoard(cr, sc); }),
        bx, by);
      // Its bulbs chase round.
      for (int k = 0; k < 9; ++k)
        if ((k + frame / 6) % 3 == 0)
          drawGlow(r, mArt, bx - 65.0f + float(k) * 16.25f, by - 102.0f, 12.0f, rgb(255, 240, 180), 0.8f);
    }
    // The flight's smoke trail and sparkle.
    if (s.flying)
    {
      const float px = (lerpF(p.prevX, p.x, alpha) + 1.5f) * kCellPx - camX;
      const float py = (lerpF(p.prevY, p.y, alpha) - 2.0f) * kCellPx - camY;
      const float len = std::max(0.01f, std::sqrt(s.vx * s.vx + s.vy * s.vy));
      for (int k = 1; k <= 8; ++k)
      {
        const float d = float(k) * 26.0f;
        const float wob = std::sin(float(frame) * 0.3f + float(k)) * 4.0f;
        drawGlow(r, mArt, px - s.vx / len * d + wob, py - s.vy / len * d, 18.0f + float(k) * 3.0f, rgb(240, 236, 230),
          0.5f - float(k) * 0.05f);
      }
      if ((frame / 3) % 2 == 0)
        drawGlow(r, mArt, px, py, 30.0f, rgb(255, 230, 140), 0.4f);
    }
    // The cannon: its barrel at the angle, the carriage over the trunnion.
    const float cx = (float(s.cannonX) + 1.5f) * kCellPx - camX;
    int floorRow = s.cannonY + 1;
    while (floorRow < mMap.height() && !mMap.solidTop(s.cannonX + 1, floorRow))
      ++floorRow;
    const float floorY = float(floorRow) * kCellPx - camY;
    const float tx = cx, ty = floorY - 90.0f;
    DrawOpts bo;
    bo.angle = -float(s.angle);
    // The recoil: the barrel kicks back as it fires.
    const float kick = s.flying && s.fy > float(s.cannonY) - 6.0f ? 10.0f : 0.0f;
    const float ca = std::cos(float(s.angle) * 3.14159f / 180.0f), sa = std::sin(float(s.angle) * 3.14159f / 180.0f);
    r.draw(baked(mArt, r, "barrel", 192, 70, 40.0f, 35.0f, paintBarrel), tx - ca * kick, ty + sa * kick, bo);
    if (!s.flying && s.reload == 0)
    {
      // Loaded: the runner's helmet in the muzzle, a fuse fizzing.
      const float mx = tx + ca * 176.0f, my = ty - sa * 176.0f;
      drawGlow(r, mArt, mx, my, 16.0f, rgb(255, 230, 160), 0.5f + 0.3f * float((frame / 4) % 2));
      for (int k = 0; k < 3; ++k)
      {
        const float u = std::fmod(float(frame) * 0.07f + float(k) / 3.0f, 1.0f);
        r.fillRect(tx - 30.0f + std::sin(u * 9.0f + float(k)) * 8.0f, ty - 30.0f - u * 30.0f, 3.0f, 3.0f,
          rgba(255, 220, 120, int(255.0f * (1.0f - u))));
      }
    }
    r.draw(baked(mArt, r, "carriage", 200, 130, 100.0f, 40.0f, paintCarriage), tx, ty);
    return;
  }
  // The hulls' name boards (the easter egg glints along a named one).
  for (const auto& sh : s.ships)
  {
    if (sh.name.empty())
      continue;
    float x = 0.0f, y = 0.0f;
    if (!shipAt(*this, sh, alpha, camX, camY, x, y))
      continue;
    const float W = float(sh.deckW()) * kCellPx;
    float cx = x + W * 0.5f;
    if (isBayShip(s, sh))
      cx = x + (float(s.bayX0) * kCellPx - camX - x) * 0.5f; // on the stern's wall, clear of the bay
    const float bw = float(sh.name.size()) * 15.0f + 40.0f, by = y + 54.0f;
    if (offScreen(cx - bw, by - 20.0f, bw * 2.0f, 60.0f))
      continue;
    r.draw(baked(mArt, r, "board/" + std::to_string(int(bw)), int(bw), 36, bw * 0.5f, 0.0f,
             [bw](cairo_t* cr) { paintNameBoard(cr, bw); }),
      cx, by);
    r.drawText(sh.name, cx, by + 6.0f, {20.0f, rgb(60, 30, 20), 0}, Align::Center);
    if (sh.named || (sh.still > 0 && sh.still < kNameStill))
    {
      const float k = sh.named ? std::fmod(float(frame) * 0.01f, 1.0f) : float(sh.still) / float(kNameStill);
      const float gx = cx - bw * 0.5f + bw * k;
      r.drawLine(gx - 8.0f, by + 32.0f, gx + 8.0f, by + 2.0f, 6.0f, rgba(255, 255, 230, 120), Blend::Add);
    }
  }
  // Hull number 42, in brass numerals over its painted ground.
  for (const auto& pr : mProps)
  {
    if (pr.kind != PropKind::Deco42)
      continue;
    const float cx = (float(pr.x) + float(pr.w) * 0.5f) * kCellPx - camX, cy = (float(pr.y) + float(pr.h) * 0.5f) * kCellPx - camY;
    if (offScreen(cx - 60.0f, cy - 60.0f, 120.0f, 120.0f))
      continue;
    r.draw(baked(mArt, r, "hull42", 88, 64, 44.0f, 32.0f, [](cairo_t* cr) {
      cairo_rectangle(cr, 4, 4, 80, 56);
      lgrad(cr, 0, 4, 0, 60, {{0.0, rgb(70, 40, 30)}, {1.0, rgb(40, 22, 18)}}, 1.0, true);
      stroke(cr, kBrass, 3.0);
      for (const auto& c : {std::array<double, 2>{10, 10}, {78, 10}, {10, 54}, {78, 54}})
        rivet(cr, c[0], c[1], 2.2);
      lettering(cr, "42", 44, 48, 40, kBrassHi);
    }),
      cx, cy);
  }
  // The bomb bay: its hatch in the deck (shut, or swung up open), the
  // copper keel and doors under the hold.
  if (s.hatchTx >= 0)
  {
    const float hx = float(s.hatchTx) * kTilePx - camX, hy = float(s.hatchTy) * kTilePx - camY;
    if (!offScreen(hx - 40.0f, hy - 80.0f, 160.0f, 200.0f))
    {
      if (!s.hatchOpen)
      {
        r.fillRect(hx + 4.0f, hy - 1.0f, kTilePx - 8.0f, 11.0f, rgb(90, 52, 30));
        r.fillRect(hx + 4.0f, hy - 1.0f, kTilePx - 8.0f, 2.0f, kBrassHi);
        r.fillRect(hx + 4.0f, hy + 8.0f, kTilePx - 8.0f, 2.0f, kBrassLo);
        r.fillRect(hx + 12.0f, hy + 1.0f, 4.0f, 7.0f, kBrass);
        r.fillRect(hx + kTilePx - 16.0f, hy + 1.0f, 4.0f, 7.0f, kBrass);
        drawGlow(r, mArt, hx + kTilePx * 0.5f, hy + 4.0f, 10.0f, kBrassHi, 0.5f);
      }
      else
      {
        // The lid stood up on its hinge.
        r.fillRect(hx - 2.0f, hy - kTilePx + 8.0f, 11.0f, kTilePx - 8.0f, rgb(90, 52, 30));
        r.fillRect(hx - 2.0f, hy - kTilePx + 8.0f, 2.0f, kTilePx - 8.0f, kBrassHi);
        drawGlow(r, mArt, hx + kTilePx * 0.5f, hy + 20.0f, 50.0f, kLamp, 0.3f);
      }
    }
    const float bx0 = float(s.bayX0) * kCellPx - camX, bx1 = float(s.bayX1 + 1) * kCellPx - camX;
    const float ky = float(s.bayY + 2) * kCellPx - camY;
    if (!offScreen(bx0 - 300.0f, ky - 20.0f, bx1 - bx0 + 600.0f, 40.0f))
    {
      for (const auto& sh : s.ships)
        if (isBayShip(s, sh))
        {
          const float sx0 = float(sh.baseX) * kCellPx - camX, sx1 = sx0 + float(sh.deckW()) * kCellPx;
          r.fillRect(sx0, ky - 8.0f, sx1 - sx0, 10.0f, rgba(40, 24, 20, 240));
          r.fillRect(sx0 + 2.0f, ky - 7.0f, sx1 - sx0 - 4.0f, 6.0f, rgb(190, 112, 60));
        }
      r.fillRect(bx0, ky - 6.0f, bx1 - bx0, 4.0f, rgb(60, 60, 70));
      r.fillRect((bx0 + bx1) * 0.5f - 1.0f, ky - 7.0f, 3.0f, 6.0f, kInk);
    }
  }
  // The rings, each on its tether balloon (or a chain up to a hull).
  for (std::size_t i = 0; i < s.rings.size(); ++i)
  {
    const Ring& rg = s.rings[i];
    const float cx = (float(rg.x) + 0.5f) * kCellPx - camX, cy = (float(rg.y) + 0.5f) * kCellPx - camY;
    if (offScreen(cx - 60.0f, cy - 160.0f, 120.0f, 220.0f))
      continue;
    // A hull just over it?
    float hullBottom = -1.0f;
    for (const auto& sh : s.ships)
    {
      float x = 0.0f, y = 0.0f;
      if (!shipAt(*this, sh, alpha, camX, camY, x, y))
        continue;
      const float bottom = y + hullDepth(s, sh);
      if (cx > x && cx < x + float(sh.deckW()) * kCellPx && bottom < cy && bottom > cy - 6.0f * kCellPx)
        hullBottom = bottom;
    }
    if (hullBottom >= 0.0f)
    {
      for (float yy = hullBottom; yy < cy - 24.0f; yy += 8.0f)
        r.fillRect(cx - 3.0f, yy, 6.0f, 6.0f, rgb(110, 110, 120));
    }
    else
    {
      const float bob = std::sin(float(frame) * 0.05f + float(i) * 1.7f) * 5.0f;
      const float sway = std::sin(float(frame) * 0.03f + float(i)) * 6.0f;
      r.drawLine(cx, cy - 24.0f, cx + sway, cy - 82.0f + bob, 2.0f, rgb(80, 60, 50));
      r.draw(baked(mArt, r, "tether", 60, 70, 30.0f, 64.0f, paintTether), cx + sway, cy - 80.0f + bob);
    }
    const bool held = s.ring == int(i);
    const float pulse = held ? 0.0f : 0.25f + 0.2f * std::sin(float(frame) * 0.12f + float(i));
    drawGlow(r, mArt, cx, cy, 46.0f, rgb(255, 220, 140), pulse);
    r.draw(baked(mArt, r, "ring", 80, 80, 40.0f, 40.0f, paintRing), cx, cy);
  }
  // The captain's log on the Looter.
  if (s.logX >= 0 && !s.logTaken)
  {
    const float cx = float(s.logX + 1) * kCellPx - camX, cy = float(s.logY + 1) * kCellPx - camY;
    if (!offScreen(cx - 60.0f, cy - 60.0f, 120.0f, 120.0f))
    {
      const float bob = std::sin(float(frame) * 0.07f) * 4.0f;
      drawGlow(r, mArt, cx, cy + bob, 70.0f + 6.0f * std::sin(float(frame) * 0.1f), rgb(255, 210, 90), 0.5f);
      r.draw(baked(mArt, r, "log", 96, 96, 48.0f, 48.0f, paintLog), cx, cy + bob);
      const int ph = frame % 90;
      if (ph < 16)
      {
        const float k = std::sin(float(ph) / 16.0f * 3.14159f);
        const float sx = cx - 20.0f + float(hash2(frame / 90, 34) % 40u), sy = cy - 24.0f + bob + float(hash2(34, frame / 90) % 30u);
        r.drawLine(sx - 9.0f * k, sy, sx + 9.0f * k, sy, 2.0f, rgb(255, 250, 220), Blend::Add);
        r.drawLine(sx, sy - 9.0f * k, sx, sy + 9.0f * k, 2.0f, rgb(255, 250, 220), Blend::Add);
      }
    }
  }
}

// --- In front --------------------------------------------------------------------------------------

void World::drawFleetFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& s = mFleet;
  const auto& p = mPlayer;
  // Where the line leaves the gun: the runner's hands.
  const float hx = (lerpF(p.prevX, p.x, alpha) + 1.5f) * kCellPx - camX;
  const float hy = (lerpF(p.prevY, p.y, alpha) - float(Player::kHeight) + 1.0f) * kCellPx - camY;
  if (s.ring >= 0 && std::size_t(s.ring) < s.rings.size())
  {
    // Taut to the ring.
    const Ring& rg = s.rings[std::size_t(s.ring)];
    rope(r, (float(rg.x) + 0.5f) * kCellPx - camX, (float(rg.y) + 0.5f) * kCellPx - camY + 18.0f, hx, hy);
  }
  for (const auto& pr : mProjectiles)
    if (pr.alive && pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::GrappleGun))
    {
      // Paying out to the hook: a little slack in it.
      const float ex = lerpF(pr.prevX, pr.x, alpha) * kCellPx + 16.0f - camX, ey = lerpF(pr.prevY, pr.y, alpha) * kCellPx + 16.0f - camY;
      const float mx = (hx + ex) * 0.5f, my = (hy + ey) * 0.5f + 14.0f;
      rope(r, hx, hy, mx, my, 3.0f);
      rope(r, mx, my, ex, ey, 3.0f);
    }
  // Yanked enemies come in on the line.
  for (const auto& y : s.yanks)
  {
    const Enemy& e = mEnemies[std::size_t(y.enemy)];
    if (!e.alive)
      continue;
    const float ex = (e.drawX + float(e.w) * 0.5f) * kCellPx - camX, ey = (e.drawY + 1.0f - float(e.h) * 0.5f) * kCellPx - camY;
    rope(r, hx, hy, ex, ey, 3.0f);
    r.draw(baked(mArt, r, "hook", 56, 56, 12.0f, 28.0f, paintHook), ex, ey);
  }
  // Sky Pirates, Peck Parrots, Balloon Mines.
  for (std::size_t i = 0; i < mEnemies.size(); ++i)
  {
    const Enemy& e = mEnemies[i];
    if (!e.alive || e.hidden)
      continue;
    if (e.kind != EnemyKind::SkyPirate && e.kind != EnemyKind::PeckParrot && e.kind != EnemyKind::BalloonMine)
      continue;
    float cx = (e.drawX + float(e.w) * 0.5f) * kCellPx - camX;
    const float foot = (e.drawY + 1.0f) * kCellPx - camY;
    const float h = float(e.h) * kCellPx;
    if (offScreen(cx - 140.0f, foot - h - 140.0f, 280.0f, h + 200.0f))
      continue;
    const int dir = e.dir < 0 ? -1 : 1;
    const Sprite* sp = nullptr;
    DrawOpts o;
    float dx = cx, dy = foot;
    if (e.kind == EnemyKind::SkyPirate)
    {
      const Pirate* pi = nullptr;
      for (const auto& f : s.pirates)
        if (f.enemy == int(i))
          pi = &f;
      int v = 0, f = (e.x >> 1) & 1;
      if (pi && (pi->state == 1 || pi->fall > 0))
      {
        v = 3;
        f = (frame / 3) % 2;
      }
      else if (e.tell > 0 && pi && pi->attack == 1)
        v = 1;
      else if (e.tell > 0 && pi && pi->attack == 2)
        v = 2;
      sp = &styledEnemySprite(mArt, r, mTheme, "sky_pirate", v, f, e.w, e.h);
      if (v == 3)
      {
        // Jet smoke behind the pack.
        for (int k = 0; k < 5; ++k)
        {
          const float u = std::fmod(float(frame) * 0.08f + float(k) * 0.2f, 1.0f);
          drawGlow(r, mArt, cx - float(dir) * (30.0f + u * 30.0f) + u * 40.0f, foot - 40.0f + u * 50.0f, 16.0f + u * 22.0f,
            rgb(220, 210, 220), 0.4f * (1.0f - u));
        }
      }
      r.draw(sp->get(dir), cx, foot);
      if (v == 1)
      {
        // The blade flashing over his head.
        const float k = 1.0f - float(e.tell) / float(std::max(1, enemyDef(e.def).tell));
        const float tx = cx + float(dir) * (20.0f - 48.0f), ty = foot - 160.0f - 2.0f;
        const float sz = 8.0f + 10.0f * k * float((frame / 2) % 2);
        r.drawLine(tx - sz, ty, tx + sz, ty, 3.0f, rgb(255, 255, 240), Blend::Add);
        r.drawLine(tx, ty - sz, tx, ty + sz, 3.0f, rgb(255, 255, 240), Blend::Add);
        drawGlow(r, mArt, tx, ty, 26.0f + 20.0f * k, rgb(255, 250, 220), 0.6f);
      }
      else if (v == 2)
      {
        // The pan smoking, the muzzle glinting.
        const float mx = cx + float(dir) * (126.0f - 48.0f), my = foot - 160.0f + 62.0f;
        drawGlow(r, mArt, mx, my, 14.0f + 6.0f * float((frame / 3) % 2), rgb(255, 120, 80), 0.7f);
        drawGlow(r, mArt, cx + float(dir) * (100.0f - 48.0f), my - 14.0f - float(frame % 12), 10.0f, rgb(220, 220, 220), 0.4f);
      }
    }
    else if (e.kind == EnemyKind::PeckParrot)
    {
      const Parrot* pa = nullptr;
      for (const auto& f : s.parrots)
        if (f.enemy == int(i))
          pa = &f;
      const int st = pa ? pa->state : 0;
      const bool carrier = e.carrier;
      int v = 0, f = 0;
      float angle = 0.0f;
      if (st == 1)
      {
        v = carrier ? 2 : 1;
        dx += ((frame / 2) % 2 ? 3.0f : -3.0f); // shaking with rage
      }
      else if (st == 2)
      {
        v = carrier ? 3 : 2;
        if (pa)
        {
          const float vx = dir > 0 ? pa->vx : -pa->vx;
          angle = std::clamp(std::atan2(pa->vy, std::max(0.01f, vx)) * 57.2958f, -50.0f, 50.0f) * float(dir);
        }
      }
      else if (st == 3)
      {
        v = carrier ? 4 : 3;
        f = (frame / 4) % 2;
      }
      else if (st == 4)
      {
        v = e.dive ? 1 : 0;
        if (e.dive)
          dy -= 10.0f;
      }
      else
        f = carrier ? 0 : ((frame / 9) % 3 == 2 ? 1 : 0); // pecking at the balloon
      sp = &styledEnemySprite(mArt, r, mTheme, carrier ? "carrier_parrot" : "peck_parrot", v, f, e.w, e.h);
      if (angle != 0.0f)
      {
        // Rotate about the middle, not the feet.
        const float th = angle * 3.14159f / 180.0f;
        o.angle = angle;
        dx = cx - h * 0.5f * std::sin(th);
        dy = foot - h * 0.5f + h * 0.5f * std::cos(th);
      }
      r.draw(sp->get(dir), dx, dy, o);
      if (st == 1)
      {
        // The squawk: a burst over its head, feathers flying.
        const float k = pa ? 1.0f - float(pa->t) / float(std::max(1, enemyDef(e.def).tell)) : 0.5f;
        r.drawText("!", dx + float(dir) * 20.0f, foot - h - 34.0f - 6.0f * k, {34.0f, rgb(255, 80, 60), kHudInk}, Align::Center);
        for (int q = 0; q < 4; ++q)
        {
          const float a = float(q) * 1.57f + float(frame) * 0.1f;
          const float rr = 30.0f + 20.0f * k;
          r.drawLine(cx + std::cos(a) * rr, foot - h * 0.5f + std::sin(a) * rr, cx + std::cos(a) * (rr + 10.0f),
            foot - h * 0.5f + std::sin(a) * (rr + 10.0f), 3.0f, carrier ? rgb(120, 200, 90) : rgb(240, 70, 60));
        }
      }
      else if (st == 0 && !carrier && pa && pa->ship >= 0)
      {
        // Pecking: puffs of air out of the balloon at its beak.
        const Ship& sh = s.ships[std::size_t(pa->ship)];
        if (sh.sink < sh.sinkMax)
          for (int q = 0; q < 3; ++q)
          {
            const float u = std::fmod(float(frame) * 0.04f + float(q) / 3.0f, 1.0f);
            drawGlow(r, mArt, cx + float(dir) * (24.0f + u * 30.0f), foot + 6.0f - u * 24.0f, 6.0f + u * 10.0f, rgb(250, 248, 255),
              0.5f * (1.0f - u));
          }
      }
    }
    else
    {
      const bool lit = e.tell > 0;
      const int f = lit ? (frame / 2) % 2 : (frame / 24) % 2;
      sp = &styledEnemySprite(mArt, r, mTheme, "balloon_mine", lit ? 1 : 0, f, e.w, e.h);
      dx += std::sin(float(frame) * 0.04f + float(i)) * 4.0f;
      const float my = (float(e.y) - float(e.h) / 2.0f + 0.5f) * kCellPx - camY;
      if (lit)
      {
        // The blast's reach, closing in on it as the fuse runs down.
        const float reach = (float(kMineRadius) + 1.5f) * kCellPx;
        const float k = float(e.tell) / float(kMineFuse);
        drawGlow(r, mArt, dx, my, reach * (1.0f + 0.3f * k), rgb(255, 60, 40), 0.18f + 0.12f * float(f));
        for (int q = 0; q < 24; ++q)
        {
          const float a0 = float(q) * 0.2618f, a1 = a0 + 0.17f;
          const float rr = reach * (0.9f + 0.3f * k);
          r.drawLine(dx + std::cos(a0) * rr, my + std::sin(a0) * rr, dx + std::cos(a1) * rr, my + std::sin(a1) * rr, 3.0f,
            rgba(255, 70, 50, f ? 220 : 140));
        }
      }
      r.draw(sp->get(1), dx, dy);
    }
    if (sp && e.flash > 0 && (frame / 2) % 2 == 0)
    {
      DrawOpts wo = o;
      wo.blend = Blend::Add;
      wo.alpha = 0.55f;
      r.draw(sp->get(e.kind == EnemyKind::BalloonMine ? 1 : dir), dx, dy, wo);
    }
  }
  // Shrapnel: hot iron shards tumbling.
  for (const auto& sh : s.shards)
  {
    const float x = sh.x * kCellPx - camX, y = sh.y * kCellPx - camY;
    const float k = float(sh.life) / 8.0f;
    drawGlow(r, mArt, x, y, 14.0f, rgb(255, 150, 70), 0.6f * k);
    const float a = float(frame) * 0.7f + sh.x;
    r.drawLine(x - std::cos(a) * 7.0f, y - std::sin(a) * 7.0f, x + std::cos(a) * 7.0f, y + std::sin(a) * 7.0f, 6.0f, rgb(50, 50, 60));
    r.drawLine(x - sh.vx * 14.0f, y - sh.vy * 14.0f, x, y, 2.0f, rgba(255, 200, 120, int(200.0f * k)));
  }
  // The cloud sea: heaped tops drifting, then nothing but cloud.
  if (s.killY < (1 << 19))
  {
    const float top = float(s.killY - 6) * kCellPx - camY;
    if (top < float(kScreenH) + 120.0f)
    {
      const Texture& puff = baked(mArt, r, "puff", 260, 150, 130.0f, 100.0f, paintPuff);
      const float drift = float(frame) * 0.3f;
      for (int row = 0; row < 2; ++row)
      {
        const float step = row == 0 ? 170.0f : 210.0f;
        const float off = std::fmod(camX * 1.0f - drift * (row == 0 ? 1.0f : 0.6f) + float(row) * 90.0f, step);
        for (float x = -off - step; x < float(kScreenW) + step; x += step)
        {
          const int k = int(std::floor((x + camX) / step));
          DrawOpts po;
          po.scale = 1.0f + 0.35f * unit(hash2(k, 3451 + row));
          po.tint = row == 0 ? rgb(255, 236, 240) : rgb(240, 224, 246);
          r.draw(puff, x, top + float(row) * 60.0f + 20.0f * unit(hash2(k, 3461 + row)), po);
        }
      }
      r.fillRect(0, top + 110.0f, float(kScreenW), std::max(0.0f, float(kScreenH) - top - 110.0f), rgba(244, 226, 236, 245));
    }
  }
}

void World::drawFleetHud(Renderer& r, int frame) const
{
  const auto& s = mFleet;
  if (!s.cannonball || s.flying || s.reload > 0)
    return;
  // Human Cannonball's angle dial: a brass gauge, its needle at the aim.
  const float gx = float(kScreenW) - 120.0f, gy = 230.0f;
  r.draw(baked(mArt, r, "dial", 180, 110, 90.0f, 95.0f, [](cairo_t* cr) {
    cairo_arc(cr, 90, 95, 80, kPi, 2 * kPi);
    cairo_close_path(cr);
    lgrad(cr, 0, 15, 0, 95, {{0.0, rgb(250, 240, 220)}, {1.0, rgb(220, 200, 170)}}, 1.0, true);
    stroke(cr, kBrassLo, 6.0);
    for (const int a : {30, 45, 60})
    {
      const double t = a * kPi / 180.0;
      cairo_move_to(cr, 90 + std::cos(t) * 60, 95 - std::sin(t) * 60);
      cairo_line_to(cr, 90 + std::cos(t) * 76, 95 - std::sin(t) * 76);
      stroke(cr, rgb(180, 30, 40), 4.0);
    }
    for (int a = 0; a <= 90; a += 15)
    {
      const double t = a * kPi / 180.0;
      cairo_move_to(cr, 90 + std::cos(t) * 70, 95 - std::sin(t) * 70);
      cairo_line_to(cr, 90 + std::cos(t) * 76, 95 - std::sin(t) * 76);
      stroke(cr, kInk, 1.5);
    }
  }),
    gx, gy);
  const float t = float(s.angle) * 3.14159f / 180.0f;
  const float wob = std::sin(float(frame) * 0.3f) * 0.01f;
  r.drawLine(gx, gy, gx + std::cos(t + wob) * 64.0f, gy - std::sin(t + wob) * 64.0f, 5.0f, rgb(40, 24, 20));
  r.fillRect(gx - 6.0f, gy - 6.0f, 12.0f, 12.0f, kBrass);
  char buf[32];
  std::snprintf(buf, sizeof(buf), "ANGLE %d", s.angle);
  r.drawText(buf, gx, gy + 12.0f, {24.0f, rgb(255, 220, 140), kHudInk}, Align::Center);
  r.drawText("UP / DOWN TO AIM", gx, gy + 42.0f, {14.0f, rgb(240, 220, 200), kHudInk}, Align::Center);
}

// --- Shots and icons -------------------------------------------------------------------------------

bool drawFleetShot(Renderer& r, const World& w, const Art& art, const Projectile& pr, float cx, float cy, int frame)
{
  const auto& s = w.fleet();
  if (pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::GrappleGun))
  {
    DrawOpts o;
    o.angle = std::atan2(pr.vy, pr.vx) * 57.2958f;
    r.draw(baked(art, r, "hook", 56, 56, 12.0f, 28.0f, paintHook), cx, cy, o);
    if ((frame / 2) % 2 == 0)
      drawGlow(r, art, cx + pr.vx * 30.0f, cy + pr.vy * 30.0f, 12.0f, rgb(255, 255, 240), 0.5f);
    return true;
  }
  if (pr.kind == ShotKind::Enemy && s.on && !s.cannonball)
  {
    // A Sky Pirate's pistol ball, a puff of smoke behind it.
    for (int k = 1; k <= 4; ++k)
      drawGlow(r, art, cx - float(pr.dx) * float(k) * 14.0f, cy, 8.0f + float(k) * 3.0f, rgb(220, 220, 226), 0.35f - float(k) * 0.07f);
    drawGlow(r, art, cx, cy, 16.0f, rgb(255, 160, 90), 0.5f);
    r.fillRect(cx - 6.0f, cy - 6.0f, 12.0f, 12.0f, rgb(30, 30, 36));
    r.fillRect(cx - 4.0f, cy - 4.0f, 4.0f, 4.0f, rgb(170, 170, 180));
    return true;
  }
  return false;
}

void drawGrappleIcon(Renderer& r, const Art& art, float x, float y, float scale, int frame)
{
  DrawOpts o;
  o.scale = scale;
  r.draw(baked(art, r, "grapple", 64, 64, 0, 0, paintGrapple), x, y, o);
  // A glint running along the barrel.
  const float u = std::fmod(float(frame) * 0.015f, 1.0f);
  if (u < 0.3f)
    drawGlow(r, art, x + (10.0f + u / 0.3f * 36.0f) * scale, y + 28.0f * scale, 8.0f * scale, rgb(255, 250, 220), 0.7f);
}

} // namespace gr
