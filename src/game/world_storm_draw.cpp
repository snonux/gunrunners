// Level 31, Storm Relay: drawing. See world_storm.cpp for the logic.
//
// Behind the tiles (drawStormSet): sheet lightning flickering in the cloud
// and now and then a distant bolt down to the sea, rain far off, the sea's
// whitecaps coming and going, the surf rolling in at the foot of the beach,
// the ship on the horizon flashing G-U-N once the lamp is lit, the back
// walls of the fisherman's hut (a window on the storm), the sea caves (their
// arches open to the night), the smugglers' room and the lighthouse (stone,
// slit windows, the lamp room's glass), the lamp's turning beam, and
// Bungee's lighthouse. Behind the runner (drawStormBack): wet sand over the
// beach and the boardwalk's planks, the hut's shingled roof, rusted metal
// over every `@ metal` piece (the cliff stairs' riveted steps, the wet
// grate, the antenna masts with their rods and lights), the lighthouse's
// masonry, gallery and roof rod, the wooden landings, the stair flights'
// stringers and rails, planks over the one-way rock ends, the hatch, the
// lift's rails, gantry, cable and cage, the winch house, the generator and
// the lamp, the Kite Drones' tethers (a spark running down them), the fry
// gull on its post, buoy 42, the keeper's desk and log, stuck Tesla rods
// (their tips glowing), Bungee's rings. In front (drawStormFront): Kite
// Drones, Storm Gulls and Foghorns (the inhale and the blast's rippling
// air, green spray on the carrier), the blue-white glow and crackle on the
// metal before a strike, the bolt and the live crackle after it, the rods'
// arcs, Bungee's rope, the rain, and the lightning's flash over it all.

#include "game/world.hpp"

#include "assets/art.hpp"
#include "assets/art_storm.hpp"
#include "assets/enemy_art.hpp"
#include "assets/enemy_art_storm.hpp"
#include "base/math.hpp"
#include "game/storm_draw.hpp"
#include "render/vector.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
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
constexpr Color kInk = rgb(8, 10, 16);
constexpr Color kBolt = rgb(160, 196, 255);
constexpr Color kRustC = rgb(140, 70, 36);
constexpr Color kCopper = rgb(214, 120, 64);

bool offScreen(float x, float y, float w, float h, float m = 96.0f)
{
  return x > float(kScreenW) + m || x + w < -m || y > float(kScreenH) + m || y + h < -m;
}

Color lighten(Color c, float t) { return lerpColor(c, rgb(255, 255, 255), t); }
Color darken(Color c, float t) { return lerpColor(c, rgb(0, 0, 0), t); }

float smooth(float t)
{
  t = std::clamp(t, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

float unit(std::uint32_t h) { return float(h % 10000u) / 10000.0f; }

// A texture baked once and kept in the Art's sprite cache under `key`; the
// mirrored copy too, its anchor mirrored with it.
const Sprite& bakedSprite(const Art& art, const Renderer& r, const std::string& key, int w, int h, float ax, float ay,
  const std::function<void(cairo_t*)>& paint)
{
  const std::string id = "~storm/" + key;
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

// Rust: blotches and runs over a steel face.
void rustOver(cairo_t* cr, double x, double y, double w, double h, std::uint32_t seed, int n)
{
  Rng rng(seed);
  for (int i = 0; i < n; ++i)
  {
    const double rx = x + rng.range(0, float(w)), ry = y + rng.range(0, float(h));
    glow(cr, rx, ry, rng.range(4, 10), kRustC, 0.55);
    cairo_move_to(cr, rx, ry);
    cairo_line_to(cr, rx + rng.range(-1, 1), std::min(y + h, ry + rng.range(6, 18)));
    stroke(cr, rgb(110, 50, 26), 1.2, 0.5);
  }
}

// --- Painters: the beach and the hut ---------------------------------------------------------------

// Wet sand: a dark, fine body with faint strata; the top tile carries the
// surface (8 px down its texture), a glossy line of wet and a shell or two.
void paintSand(cairo_t* cr, bool top)
{
  const double y0 = top ? 8.0 : 0.0;
  if (top)
  {
    cairo_move_to(cr, 0, y0 - 2);
    for (double x = 0; x <= 64; x += 4)
      cairo_line_to(cr, x, y0 - 2 + 1.2 * std::sin(x / 64.0 * 2 * kPi));
    cairo_line_to(cr, 64, 72);
    cairo_line_to(cr, 0, 72);
    cairo_close_path(cr);
  }
  else
    cairo_rectangle(cr, 0, 0, 64, 64);
  setRgba(cr, rgb(70, 62, 52), 1.0);
  cairo_fill(cr);
  // Strata.
  for (int k = 0; k < 3; ++k)
  {
    const double y = y0 + 12 + k * 18;
    cairo_move_to(cr, 0, y);
    cairo_curve_to(cr, 20, y - 2, 44, y + 2, 64, y);
    stroke(cr, k % 2 ? rgb(90, 80, 66) : rgb(52, 46, 40), 2.0, 0.5);
  }
  Rng rng(top ? 31201u : 31203u);
  for (int i = 0; i < 120; ++i)
  {
    cairo_arc(cr, rng.range(0, 64), y0 + rng.range(0, 62), rng.range(0.4f, 1.0f), 0, 2 * kPi);
    setRgba(cr, i % 3 ? rgb(40, 36, 30) : rgb(150, 138, 116), rng.range(0.2f, 0.5f));
    cairo_fill(cr);
  }
  if (!top)
    return;
  // The surface: a lighter skin of wet sand and the sky's sheen on it.
  cairo_rectangle(cr, 0, y0 - 2, 64, 8);
  lgrad(cr, 0, y0 - 2, 0, y0 + 6, {{0.0, rgb(132, 120, 100)}, {1.0, rgb(70, 62, 52)}});
  cairo_move_to(cr, 0, y0 - 1.5);
  for (double x = 0; x <= 64; x += 4)
    cairo_line_to(cr, x, y0 - 1.5 + 1.2 * std::sin(x / 64.0 * 2 * kPi));
  stroke(cr, rgb(190, 205, 225), 1.4, 0.6);
  // A cockle shell and a pebble.
  cairo_move_to(cr, 40, y0 + 1);
  cairo_curve_to(cr, 40, y0 - 4, 48, y0 - 4, 48, y0 + 1);
  cairo_close_path(cr);
  setRgba(cr, rgb(226, 214, 196), 1.0);
  cairo_fill(cr);
  ellipse(cr, 14, y0 + 1, 2.4, 1.4);
  setRgba(cr, rgb(110, 116, 126), 1.0);
  cairo_fill(cr);
}

// The boardwalk: grey weathered planks on a joist, nail heads, rain on the
// top. 64 x 30, the walking surface 4 px down.
void paintBoardwalk(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 2, 64, 12);
  lgrad(cr, 0, 2, 0, 14, {{0.0, rgb(150, 136, 116)}, {0.5, rgb(112, 98, 82)}, {1.0, rgb(70, 60, 50)}});
  for (const double x : {0.0, 32.0})
  {
    cairo_move_to(cr, x + 0.5, 2);
    cairo_line_to(cr, x + 0.5, 14);
    stroke(cr, rgb(30, 24, 20), 1.6);
  }
  for (int k = 0; k < 3; ++k)
  {
    cairo_move_to(cr, 2, 6 + k * 3);
    cairo_curve_to(cr, 20, 5 + k * 3, 40, 7 + k * 3, 62, 6 + k * 3);
    stroke(cr, rgb(80, 68, 56), 0.7, 0.6);
  }
  for (const double x : {4.0, 28.0, 36.0, 60.0})
  {
    circle(cr, x, 8, 1.1);
    setRgba(cr, rgb(40, 34, 30), 1.0);
    cairo_fill(cr);
  }
  cairo_move_to(cr, 0, 2.6);
  cairo_line_to(cr, 64, 2.6);
  stroke(cr, rgb(210, 220, 236), 1.2, 0.6);
  // The joist and a post under it.
  cairo_rectangle(cr, 0, 14, 64, 6);
  setRgba(cr, rgb(48, 40, 34), 1.0);
  cairo_fill(cr);
  cairo_rectangle(cr, 26, 20, 10, 10);
  lgrad(cr, 26, 0, 36, 0, {{0.0, rgb(100, 86, 70)}, {1.0, rgb(44, 36, 30)}});
}

// The hut's roof: tarred shingles in rows, moss in the joints, rain on them;
// `v` 1 the left eave, 2 the right.
void paintRoof(cairo_t* cr, int v)
{
  cairo_save(cr);
  if (v == 1)
  {
    cairo_move_to(cr, 24, 0);
    cairo_line_to(cr, 64, 0);
    cairo_line_to(cr, 64, 64);
    cairo_line_to(cr, 0, 64);
    cairo_close_path(cr);
    cairo_clip(cr);
  }
  else if (v == 2)
  {
    cairo_move_to(cr, 0, 0);
    cairo_line_to(cr, 40, 0);
    cairo_line_to(cr, 64, 64);
    cairo_line_to(cr, 0, 64);
    cairo_close_path(cr);
    cairo_clip(cr);
  }
  cairo_rectangle(cr, 0, 0, 64, 64);
  setRgba(cr, rgb(36, 30, 28), 1.0);
  cairo_fill(cr);
  for (int row = 0; row < 4; ++row)
    for (int k = -1; k < 5; ++k)
    {
      const double x = k * 16.0 + (row % 2) * 8.0, y = row * 16.0;
      cairo_move_to(cr, x + 1, y);
      cairo_line_to(cr, x + 15, y);
      cairo_line_to(cr, x + 15, y + 15);
      cairo_curve_to(cr, x + 11, y + 17, x + 5, y + 17, x + 1, y + 15);
      cairo_close_path(cr);
      const Color c = (k + row) % 3 == 0 ? rgb(78, 66, 58) : rgb(62, 54, 50);
      lgrad(cr, 0, y, 0, y + 16, {{0.0, lighten(c, 0.15f)}, {1.0, darken(c, 0.3f)}}, 1.0, true);
      stroke(cr, kInk, 1.0, 0.8);
      cairo_move_to(cr, x + 2, y + 1.2);
      cairo_line_to(cr, x + 14, y + 1.2);
      stroke(cr, rgb(190, 205, 226), 0.8, 0.45);
    }
  Rng rng(31211u);
  for (int i = 0; i < 6; ++i)
    glow(cr, rng.range(4, 60), rng.range(6, 60), rng.range(3, 6), rgb(70, 110, 60), 0.6);
  cairo_restore(cr);
}

// The hut's back wall: weathered upright boards.
void paintHutWall(cairo_t* cr)
{
  for (int k = 0; k < 4; ++k)
  {
    const double x = k * 16.0;
    cairo_rectangle(cr, x, 0, 16, 64);
    const Color c = k % 2 ? rgb(92, 82, 70) : rgb(104, 92, 78);
    lgrad(cr, x, 0, x + 16, 0, {{0.0, lighten(c, 0.1f)}, {1.0, darken(c, 0.2f)}});
    cairo_move_to(cr, x + 0.5, 0);
    cairo_line_to(cr, x + 0.5, 64);
    stroke(cr, rgb(30, 26, 22), 1.4);
    cairo_move_to(cr, x + 6, 4);
    cairo_curve_to(cr, x + 8, 20, x + 5, 40, x + 7, 60);
    stroke(cr, rgb(70, 60, 50), 0.7, 0.6);
  }
}

// The hut's window on the storm: four panes, a sill, rain on the glass.
void paintHutWindow(cairo_t* cr)
{
  cairo_rectangle(cr, 6, 6, 84, 64);
  setRgba(cr, rgb(70, 56, 44), 1.0);
  cairo_fill(cr);
  cairo_rectangle(cr, 12, 12, 72, 52);
  lgrad(cr, 0, 12, 0, 64, {{0.0, rgb(30, 40, 62)}, {0.6, rgb(52, 64, 88)}, {1.0, rgb(16, 22, 34)}}, 0.85);
  Rng rng(31221u);
  for (int i = 0; i < 14; ++i)
  {
    const double x = rng.range(14, 82), y = rng.range(14, 50);
    cairo_move_to(cr, x, y);
    cairo_line_to(cr, x + 1, y + rng.range(5, 12));
    stroke(cr, rgb(200, 220, 245), 1.0, 0.55);
  }
  cairo_move_to(cr, 48, 12);
  cairo_line_to(cr, 48, 64);
  cairo_move_to(cr, 12, 38);
  cairo_line_to(cr, 84, 38);
  stroke(cr, rgb(70, 56, 44), 4.0);
  cairo_rectangle(cr, 12, 12, 72, 52);
  stroke(cr, kInk, 1.4, 0.8);
  cairo_rectangle(cr, 2, 68, 92, 8);
  lgrad(cr, 0, 68, 0, 76, {{0.0, rgb(140, 120, 96)}, {1.0, rgb(70, 56, 44)}});
}

// A wooden post (a pole), tileable down its length. 40 x 64, centred.
void paintPole(cairo_t* cr)
{
  cairo_rectangle(cr, 12, 0, 16, 64);
  lgrad(cr, 12, 0, 28, 0, {{0.0, rgb(120, 104, 86)}, {0.4, rgb(96, 82, 68)}, {1.0, rgb(44, 36, 30)}});
  for (int k = 0; k < 3; ++k)
  {
    cairo_move_to(cr, 15 + k * 4, 0);
    cairo_curve_to(cr, 16 + k * 4, 20, 14 + k * 4, 44, 15 + k * 4, 64);
    stroke(cr, rgb(60, 50, 42), 0.7, 0.6);
  }
  cairo_move_to(cr, 13, 0);
  cairo_line_to(cr, 13, 64);
  stroke(cr, rgb(200, 214, 236), 1.0, 0.35);
}

// The buoy numbered 42 hanging on its chain from a beach post's arm. 220 x
// 320; the buoy's middle at (100, 120), the sand at y 280.
void paintBuoy(cairo_t* cr)
{
  // The post and its arm.
  cairo_rectangle(cr, 158, 26, 16, 266);
  lgrad(cr, 158, 0, 174, 0, {{0.0, rgb(120, 104, 86)}, {1.0, rgb(44, 36, 30)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  cairo_rectangle(cr, 90, 24, 86, 12);
  lgrad(cr, 0, 24, 0, 36, {{0.0, rgb(130, 112, 92)}, {1.0, rgb(60, 50, 40)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  cairo_move_to(cr, 140, 36);
  cairo_line_to(cr, 164, 60);
  stroke(cr, rgb(70, 58, 46), 6.0);
  // The chain.
  for (int k = 0; k < 5; ++k)
  {
    ellipse(cr, 100, 42 + k * 7, 2.6, 4.2);
    stroke(cr, rgb(120, 110, 100), 1.8);
  }
  // The buoy: red top and bottom, a white band with the number.
  cairo_save(cr);
  cairo_translate(cr, 100, 120);
  cairo_move_to(cr, -8, -46);
  cairo_curve_to(cr, -40, -40, -46, -6, -44, 14);
  cairo_curve_to(cr, -40, 40, -20, 48, 0, 48);
  cairo_curve_to(cr, 20, 48, 40, 40, 44, 14);
  cairo_curve_to(cr, 46, -6, 40, -40, 8, -46);
  cairo_close_path(cr);
  cairo_save(cr);
  cairo_clip_preserve(cr);
  lgrad(cr, -44, 0, 44, 0, {{0.0, rgb(150, 24, 26)}, {0.35, rgb(220, 52, 44)}, {1.0, rgb(110, 16, 20)}}, 1.0, true);
  cairo_new_path(cr);
  cairo_rectangle(cr, -50, -12, 100, 28);
  lgrad(cr, -44, 0, 44, 0, {{0.0, rgb(170, 176, 184)}, {0.35, rgb(244, 244, 240)}, {1.0, rgb(140, 146, 156)}});
  cairo_restore(cr);
  cairo_move_to(cr, -8, -46);
  cairo_curve_to(cr, -40, -40, -46, -6, -44, 14);
  cairo_curve_to(cr, -40, 40, -20, 48, 0, 48);
  cairo_curve_to(cr, 20, 48, 40, 40, 44, 14);
  cairo_curve_to(cr, 46, -6, 40, -40, 8, -46);
  cairo_close_path(cr);
  stroke(cr, kInk, 2.4);
  lettering(cr, "42", 0, 11, 24, rgb(20, 20, 26));
  // Its lifting eye, rust and the rain's sheen.
  circle(cr, 0, -50, 5);
  stroke(cr, rgb(120, 110, 100), 2.4);
  rustOver(cr, -30, 18, 60, 24, 31231u, 4);
  cairo_arc(cr, 0, 0, 40, -kPi * 0.85, -kPi * 0.55);
  stroke(cr, rgb(255, 240, 240), 2.0, 0.55);
  cairo_restore(cr);
}

// --- Painters: rock, stone and metal -------------------------------------------------------------

// Lighthouse masonry: whitewashed granite in two courses, weathered and
// rain-streaked; `band` paints it red (the tower's stripe).
void paintMasonry(cairo_t* cr, bool band)
{
  const Color base = band ? rgb(178, 46, 40) : rgb(206, 206, 200);
  cairo_rectangle(cr, 0, 0, 64, 64);
  setRgba(cr, darken(base, 0.45f), 1.0);
  cairo_fill(cr);
  for (int row = 0; row < 2; ++row)
    for (int k = -1; k < 2; ++k)
    {
      const double x = k * 40.0 + (row % 2) * 20.0, y = row * 32.0;
      cairo_rectangle(cr, x + 1.5, y + 1.5, 37, 29);
      const Color c = lerpColor(base, darken(base, 0.2f), float((k + 2 + row) % 3) / 3.0f);
      lgrad(cr, 0, y, 0, y + 32, {{0.0, lighten(c, 0.08f)}, {0.7, c}, {1.0, darken(c, 0.22f)}}, 1.0, true);
      setRgba(cr, kInk, 0.25);
      cairo_set_line_width(cr, 1.0);
      cairo_stroke(cr);
    }
  // Streaks of rain and grime down it.
  Rng rng(band ? 31301u : 31303u);
  for (int i = 0; i < 7; ++i)
  {
    const double x = rng.range(2, 62), y = rng.range(0, 30);
    cairo_move_to(cr, x, y);
    cairo_line_to(cr, x + rng.range(-1, 1), y + rng.range(16, 34));
    stroke(cr, darken(base, 0.4f), 1.4, 0.35);
  }
}

// A cliff stair's step: a riveted steel box gone to rust, a tread plate on
// top with a bright worn nosing (`top`).
void paintMetalBlock(cairo_t* cr, bool top)
{
  cairo_rectangle(cr, 0, 0, 64, 64);
  lgrad(cr, 0, 0, 0, 64, {{0.0, rgb(98, 92, 88)}, {1.0, rgb(52, 46, 44)}});
  // Panels and rivets.
  cairo_rectangle(cr, 3, top ? 10 : 3, 58, top ? 51 : 58);
  stroke(cr, rgb(30, 26, 26), 1.4, 0.8);
  for (double x = 7; x < 64; x += 12.5)
    for (const double y : {top ? 14.0 : 7.0, 57.0})
    {
      circle(cr, x, y, 1.6);
      setRgba(cr, rgb(170, 160, 150), 0.9);
      cairo_fill(cr);
    }
  rustOver(cr, 2, top ? 12 : 2, 60, top ? 50 : 60, top ? 31311u : 31313u, 8);
  if (!top)
    return;
  // The tread: diamond plate seen at a slant, and the nosing.
  cairo_rectangle(cr, 0, 0, 64, 9);
  lgrad(cr, 0, 0, 0, 9, {{0.0, rgb(190, 192, 196)}, {1.0, rgb(96, 92, 90)}});
  for (double x = 2; x < 64; x += 6)
  {
    cairo_move_to(cr, x, 6);
    cairo_line_to(cr, x + 3, 3);
    stroke(cr, rgb(60, 58, 58), 1.0, 0.7);
  }
  cairo_move_to(cr, 0, 0.8);
  cairo_line_to(cr, 64, 0.8);
  stroke(cr, rgb(235, 240, 248), 1.6, 0.8);
  cairo_rectangle(cr, 0, 9, 64, 2);
  setRgba(cr, kInk, 0.8);
  cairo_fill(cr);
}

// The wet grate's top (over its rock): steel grating, the dark showing
// through, drops hanging under it. 64 x 24, the surface 2 px down.
void paintGrate(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 2, 64, 16);
  setRgba(cr, rgb(10, 10, 14), 1.0);
  cairo_fill(cr);
  for (double x = 1; x < 64; x += 8)
  {
    cairo_rectangle(cr, x, 2, 3, 16);
    lgrad(cr, x, 0, x + 3, 0, {{0.0, rgb(150, 146, 140)}, {1.0, rgb(70, 64, 60)}});
  }
  cairo_rectangle(cr, 0, 2, 64, 3);
  lgrad(cr, 0, 2, 0, 5, {{0.0, rgb(210, 214, 220)}, {1.0, rgb(110, 104, 100)}});
  cairo_rectangle(cr, 0, 16, 64, 3);
  setRgba(cr, rgb(70, 62, 58), 1.0);
  cairo_fill(cr);
  rustOver(cr, 0, 4, 64, 12, 31321u, 5);
  for (const double x : {9.0, 33.0, 52.0})
  {
    ellipse(cr, x, 21, 1.4, 2.0);
    setRgba(cr, rgb(200, 220, 245), 0.7);
    cairo_fill(cr);
  }
}

// One block of an antenna mast: a narrow lattice of rusted angle iron;
// `top` adds the lightning rod and a red light (the texture runs 40 px up).
void paintMast(cairo_t* cr, bool top)
{
  const double y0 = top ? 40.0 : 0.0;
  for (const double x : {24.0, 40.0})
  {
    cairo_move_to(cr, x, y0);
    cairo_line_to(cr, x, y0 + 64);
    stroke(cr, kInk, 5.0);
    cairo_move_to(cr, x, y0);
    cairo_line_to(cr, x, y0 + 64);
    stroke(cr, rgb(130, 110, 96), 2.8);
  }
  for (int k = 0; k < 4; ++k)
  {
    const double y = y0 + k * 16.0;
    cairo_move_to(cr, 24, y);
    cairo_line_to(cr, 40, y + 16);
    cairo_move_to(cr, 24, y);
    cairo_line_to(cr, 40, y);
    stroke(cr, rgb(110, 90, 76), 1.6);
  }
  rustOver(cr, 22, y0, 20, 64, top ? 31331u : 31333u, 3);
  if (!top)
    return;
  // The rod and its ball, the cap, the red light.
  cairo_move_to(cr, 32, y0);
  cairo_line_to(cr, 32, 4);
  stroke(cr, kInk, 4.0);
  cairo_move_to(cr, 32, y0);
  cairo_line_to(cr, 32, 4);
  stroke(cr, kCopper, 2.2);
  circle(cr, 32, 6, 3.5);
  lgrad(cr, 0, 2, 0, 10, {{0.0, rgb(255, 210, 160)}, {1.0, rgb(150, 70, 30)}}, 1.0, true);
  stroke(cr, kInk, 1.0);
  cairo_rectangle(cr, 20, y0 - 3, 24, 6);
  lgrad(cr, 0, y0 - 3, 0, y0 + 3, {{0.0, rgb(150, 140, 132)}, {1.0, rgb(60, 54, 50)}}, 1.0, true);
  stroke(cr, kInk, 1.0);
}

// --- Painters: the lighthouse ---------------------------------------------------------------------

// A slit window in the tower: a stone surround, iron mullion, glass with
// rain on it. 64 x 128.
void paintTowerWindow(cairo_t* cr)
{
  cairo_move_to(cr, 8, 124);
  cairo_line_to(cr, 8, 26);
  cairo_curve_to(cr, 8, 8, 56, 8, 56, 26);
  cairo_line_to(cr, 56, 124);
  cairo_close_path(cr);
  setRgba(cr, rgb(150, 150, 146), 1.0);
  cairo_fill(cr);
  cairo_move_to(cr, 16, 118);
  cairo_line_to(cr, 16, 28);
  cairo_curve_to(cr, 16, 16, 48, 16, 48, 28);
  cairo_line_to(cr, 48, 118);
  cairo_close_path(cr);
  lgrad(cr, 0, 16, 0, 118, {{0.0, rgb(70, 84, 112)}, {1.0, rgb(30, 38, 56)}}, 0.35, true);
  stroke(cr, kInk, 1.6);
  cairo_move_to(cr, 32, 16);
  cairo_line_to(cr, 32, 118);
  cairo_move_to(cr, 16, 66);
  cairo_line_to(cr, 48, 66);
  stroke(cr, rgb(40, 40, 46), 3.0);
  Rng rng(31341u);
  for (int i = 0; i < 8; ++i)
  {
    const double x = rng.range(18, 46), y = rng.range(26, 104);
    cairo_move_to(cr, x, y);
    cairo_line_to(cr, x, y + rng.range(4, 10));
    stroke(cr, rgb(210, 226, 250), 1.0, 0.5);
  }
  cairo_rectangle(cr, 4, 116, 56, 8);
  lgrad(cr, 0, 116, 0, 124, {{0.0, rgb(200, 200, 196)}, {1.0, rgb(110, 110, 108)}});
}

// A pane of the lamp room's glass: diagonal iron astragals, faint glass.
void paintLanternPane(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 0, 64, 64);
  lgrad(cr, 0, 0, 64, 64, {{0.0, rgb(150, 180, 220)}, {0.5, rgb(60, 80, 110)}, {1.0, rgb(150, 180, 220)}}, 0.16);
  cairo_move_to(cr, 0, 64);
  cairo_line_to(cr, 64, 0);
  cairo_move_to(cr, 0, 0);
  cairo_line_to(cr, 64, 64);
  stroke(cr, rgb(30, 30, 36), 3.0, 0.9);
  cairo_rectangle(cr, 0, 0, 64, 64);
  stroke(cr, rgb(40, 40, 46), 2.4, 0.9);
  cairo_move_to(cr, 6, 18);
  cairo_line_to(cr, 18, 6);
  stroke(cr, rgb(230, 240, 255), 1.4, 0.35);
}

// A wooden landing block: planks laid along it, a lighter edge on top.
void paintWood(cairo_t* cr, bool top)
{
  for (int k = 0; k < 4; ++k)
  {
    const double y = k * 16.0;
    cairo_rectangle(cr, 0, y, 64, 16);
    const Color c = k % 2 ? rgb(120, 82, 52) : rgb(136, 94, 60);
    lgrad(cr, 0, y, 0, y + 16, {{0.0, lighten(c, 0.1f)}, {1.0, darken(c, 0.25f)}});
    cairo_move_to(cr, 0, y + 0.5);
    cairo_line_to(cr, 64, y + 0.5);
    stroke(cr, rgb(40, 26, 18), 1.2);
    cairo_move_to(cr, 4, y + 7);
    cairo_curve_to(cr, 24, y + 5, 40, y + 9, 60, y + 7);
    stroke(cr, rgb(90, 60, 38), 0.7, 0.6);
    for (const double x : {6.0, 58.0})
    {
      circle(cr, x, y + 8, 1.1);
      setRgba(cr, rgb(40, 30, 24), 1.0);
      cairo_fill(cr);
    }
  }
  cairo_move_to(cr, 17, 16);
  cairo_line_to(cr, 17, 32);
  cairo_move_to(cr, 45, 48);
  cairo_line_to(cr, 45, 64);
  stroke(cr, rgb(40, 26, 18), 1.2);
  if (top)
  {
    cairo_rectangle(cr, 0, 0, 64, 4);
    lgrad(cr, 0, 0, 0, 4, {{0.0, rgb(200, 160, 116)}, {1.0, rgb(130, 90, 58)}});
  }
}

// The planks over a one-way rock end: a short wooden walkway. 64 x 24.
void paintPlankTop(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 0, 64, 14);
  lgrad(cr, 0, 0, 0, 14, {{0.0, rgb(150, 112, 76)}, {0.4, rgb(118, 84, 54)}, {1.0, rgb(66, 44, 28)}});
  for (const double x : {0.5, 21.5, 42.5})
  {
    cairo_move_to(cr, x, 0);
    cairo_line_to(cr, x, 14);
    stroke(cr, rgb(30, 20, 14), 1.4);
  }
  cairo_move_to(cr, 0, 0.8);
  cairo_line_to(cr, 64, 0.8);
  stroke(cr, rgb(214, 220, 236), 1.2, 0.55);
  cairo_rectangle(cr, 0, 14, 64, 4);
  setRgba(cr, rgb(40, 28, 20), 1.0);
  cairo_fill(cr);
  for (const double x : {4.0, 48.0})
  {
    cairo_move_to(cr, x, 18);
    cairo_line_to(cr, x + 14, 18);
    cairo_line_to(cr, x + 7, 28);
    cairo_close_path(cr);
    setRgba(cr, rgb(50, 36, 26), 1.0);
    cairo_fill(cr);
  }
}

// The lighthouse's roof deck block: dark iron plates (the gallery floor).
void paintIronDeck(cairo_t* cr, bool top)
{
  cairo_rectangle(cr, 0, 0, 64, 64);
  lgrad(cr, 0, 0, 0, 64, {{0.0, rgb(60, 60, 66)}, {1.0, rgb(26, 26, 30)}});
  cairo_rectangle(cr, 2, 2, 60, 60);
  stroke(cr, rgb(16, 16, 20), 1.4);
  for (double x = 6; x < 64; x += 13)
  {
    circle(cr, x, 6, 1.4);
    circle(cr, x, 58, 1.4);
  }
  setRgba(cr, rgb(140, 140, 150), 0.8);
  cairo_fill(cr);
  rustOver(cr, 2, 4, 60, 56, 31351u, 4);
  if (top)
  {
    cairo_rectangle(cr, 0, 0, 64, 5);
    lgrad(cr, 0, 0, 0, 5, {{0.0, rgb(170, 174, 184)}, {1.0, rgb(60, 60, 66)}});
  }
}

// A section of the gallery's railing on the roof. 64 x 40, its foot at the
// bottom.
void paintRailing(cairo_t* cr)
{
  cairo_move_to(cr, 0, 6);
  cairo_line_to(cr, 64, 6);
  stroke(cr, kInk, 5.0);
  cairo_move_to(cr, 0, 6);
  cairo_line_to(cr, 64, 6);
  stroke(cr, rgb(110, 110, 120), 2.6);
  for (const double x : {8.0, 24.0, 40.0, 56.0})
  {
    cairo_move_to(cr, x, 6);
    cairo_line_to(cr, x, 40);
    stroke(cr, rgb(70, 70, 80), 2.4);
  }
  cairo_move_to(cr, 0, 22);
  cairo_line_to(cr, 64, 22);
  stroke(cr, rgb(80, 80, 90), 1.6);
}

// --- Painters: machines -----------------------------------------------------------------------------

// The lift's cage: a steel floor, two posts and a top beam with the cable
// eye, mesh behind. 200 x 150; the floor's top at (4, 120).
void paintLiftCar(cairo_t* cr)
{
  cairo_rectangle(cr, 12, 24, 176, 96);
  setRgba(cr, rgb(30, 32, 38), 0.35);
  cairo_fill(cr);
  for (double x = 16; x < 188; x += 12)
  {
    cairo_move_to(cr, x, 24);
    cairo_line_to(cr, x + 12, 120);
    cairo_move_to(cr, x + 12, 24);
    cairo_line_to(cr, x, 120);
  }
  stroke(cr, rgb(110, 110, 116), 0.8, 0.35);
  for (const double x : {8.0, 184.0})
  {
    cairo_rectangle(cr, x, 18, 8, 104);
    lgrad(cr, x, 0, x + 8, 0, {{0.0, rgb(170, 160, 150)}, {1.0, rgb(60, 54, 50)}}, 1.0, true);
    stroke(cr, kInk, 1.4);
  }
  cairo_rectangle(cr, 4, 12, 192, 10);
  lgrad(cr, 0, 12, 0, 22, {{0.0, rgb(240, 196, 60)}, {1.0, rgb(150, 110, 30)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  for (double x = 10; x < 196; x += 20)
  {
    cairo_move_to(cr, x, 22);
    cairo_line_to(cr, x + 8, 12);
    stroke(cr, rgb(30, 30, 30), 3.0, 0.8);
  }
  circle(cr, 100, 6, 6);
  stroke(cr, rgb(150, 150, 156), 3.0);
  // The floor.
  cairo_rectangle(cr, 0, 120, 200, 18);
  lgrad(cr, 0, 120, 0, 138, {{0.0, rgb(160, 156, 152)}, {0.2, rgb(100, 96, 92)}, {1.0, rgb(40, 36, 34)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  rustOver(cr, 2, 122, 196, 14, 31401u, 6);
  cairo_move_to(cr, 0, 120.8);
  cairo_line_to(cr, 200, 120.8);
  stroke(cr, rgb(230, 236, 246), 1.4, 0.7);
}

// The winch house beside the lift shaft: a steel cabinet with a motor and a
// cable drum on top, its lamp dark or lit. 80 x 210; the cabinet's top-left
// at (8, 18) for a 64 x 192 box.
void paintWinch(cairo_t* cr, bool lit)
{
  cairo_rectangle(cr, 8, 40, 64, 170);
  lgrad(cr, 8, 0, 72, 0, {{0.0, rgb(120, 126, 120)}, {0.5, rgb(84, 92, 86)}, {1.0, rgb(40, 44, 42)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  cairo_rectangle(cr, 16, 70, 48, 110);
  stroke(cr, rgb(30, 34, 32), 1.4);
  rustOver(cr, 10, 44, 60, 160, 31411u, 9);
  // Hazard stripes along the foot.
  cairo_save(cr);
  cairo_rectangle(cr, 10, 192, 60, 12);
  cairo_clip(cr);
  for (double x = 0; x < 80; x += 12)
  {
    cairo_move_to(cr, x, 204);
    cairo_line_to(cr, x + 6, 192);
    cairo_line_to(cr, x + 12, 192);
    cairo_line_to(cr, x + 6, 204);
    cairo_close_path(cr);
    setRgba(cr, rgb(240, 196, 40), 1.0);
    cairo_fill(cr);
  }
  cairo_restore(cr);
  // The drum and the motor on top.
  ellipse(cr, 40, 30, 26, 14);
  lgrad(cr, 0, 16, 0, 44, {{0.0, rgb(150, 146, 140)}, {1.0, rgb(50, 46, 44)}}, 1.0, true);
  stroke(cr, kInk, 1.8);
  for (int k = -2; k <= 2; ++k)
  {
    cairo_move_to(cr, 40 + k * 8, 18);
    cairo_line_to(cr, 40 + k * 8, 42);
    stroke(cr, rgb(60, 56, 54), 1.2, 0.8);
  }
  // A gauge and the lamp.
  circle(cr, 40, 96, 10);
  setRgba(cr, rgb(230, 226, 210), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.6);
  cairo_move_to(cr, 40, 96);
  cairo_line_to(cr, lit ? 47 : 33, lit ? 90 : 101);
  stroke(cr, rgb(200, 30, 30), 1.8);
  const Color lamp = lit ? rgb(110, 255, 140) : rgb(70, 30, 30);
  if (lit)
    glow(cr, 40, 136, 18, lamp, 0.8);
  circle(cr, 40, 136, 6);
  setRgba(cr, lamp, 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.4);
  lettering(cr, "LIFT", 40, 166, 12, rgb(230, 230, 220), 0.85);
}

// The generator: a dynamo on a skid, a flywheel, coils, a gauge. 144 x 136;
// the box's top-left at (8, 8) for 128 x 128.
void paintGenerator(cairo_t* cr, bool lit)
{
  // The skid.
  cairo_rectangle(cr, 10, 122, 124, 14);
  lgrad(cr, 0, 122, 0, 136, {{0.0, rgb(110, 100, 90)}, {1.0, rgb(40, 36, 32)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  // The body: a fat drum painted green, rusting.
  cairo_rectangle(cr, 34, 52, 84, 66);
  lgrad(cr, 0, 52, 0, 118, {{0.0, rgb(110, 140, 110)}, {0.4, rgb(60, 92, 66)}, {1.0, rgb(24, 40, 30)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  rustOver(cr, 36, 54, 80, 60, 31421u, 8);
  // The coil end, copper windings.
  ellipse(cr, 118, 85, 12, 33);
  lgrad(cr, 106, 0, 130, 0, {{0.0, rgb(240, 160, 90)}, {1.0, rgb(130, 60, 30)}}, 1.0, true);
  stroke(cr, kInk, 1.8);
  for (int k = -3; k <= 3; ++k)
  {
    cairo_move_to(cr, 110, 85 + k * 8);
    cairo_line_to(cr, 128, 85 + k * 8);
    stroke(cr, rgb(120, 56, 26), 1.2, 0.8);
  }
  // The flywheel on the left.
  circle(cr, 28, 84, 26);
  lgrad(cr, 0, 58, 0, 110, {{0.0, rgb(150, 150, 150)}, {1.0, rgb(50, 50, 54)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  for (int k = 0; k < 4; ++k)
  {
    const double a = k * kPi / 4.0;
    cairo_move_to(cr, 28 - std::cos(a) * 20, 84 - std::sin(a) * 20);
    cairo_line_to(cr, 28 + std::cos(a) * 20, 84 + std::sin(a) * 20);
    stroke(cr, rgb(60, 60, 66), 3.0);
  }
  circle(cr, 28, 84, 5);
  setRgba(cr, rgb(170, 170, 176), 1.0);
  cairo_fill(cr);
  // The gauge box and its lamp on top.
  cairo_rectangle(cr, 60, 20, 44, 32);
  lgrad(cr, 0, 20, 0, 52, {{0.0, rgb(150, 150, 140)}, {1.0, rgb(70, 70, 66)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  circle(cr, 74, 36, 9);
  setRgba(cr, rgb(232, 228, 210), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.2);
  cairo_move_to(cr, 74, 36);
  cairo_line_to(cr, lit ? 80 : 68, lit ? 30 : 41);
  stroke(cr, rgb(200, 30, 30), 1.6);
  const Color lamp = lit ? rgb(120, 255, 150) : rgb(70, 30, 30);
  if (lit)
    glow(cr, 94, 36, 14, lamp, 0.8);
  circle(cr, 94, 36, 5);
  setRgba(cr, lamp, 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.2);
  // The cable out to the hatch.
  cairo_move_to(cr, 34, 110);
  cairo_curve_to(cr, 20, 126, 6, 118, 0, 128);
  stroke(cr, rgb(16, 16, 20), 5.0);
}

// The lamp: a beehive Fresnel lens of glass rings in a brass frame on a
// pedestal, dark or lit. 208 x 272; the box's top-left at (8, 8) for 192 x
// 256.
void paintLamp(cairo_t* cr, bool lit)
{
  // The pedestal.
  cairo_move_to(cr, 70, 264);
  cairo_line_to(cr, 80, 196);
  cairo_line_to(cr, 128, 196);
  cairo_line_to(cr, 138, 264);
  cairo_close_path(cr);
  lgrad(cr, 70, 0, 138, 0, {{0.0, rgb(90, 90, 98)}, {0.5, rgb(150, 150, 160)}, {1.0, rgb(50, 50, 56)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  // The lens: rings of prisms, wider in the middle.
  const double cx = 104, top = 28, bot = 196;
  const int rings = 9;
  for (int i = 0; i < rings; ++i)
  {
    const double y0 = top + (bot - top) * i / rings, y1 = top + (bot - top) * (i + 1) / rings;
    const double u = (i + 0.5) / rings, half = 44 + 34 * std::sin(u * kPi);
    cairo_rectangle(cr, cx - half, y0 + 1, half * 2, y1 - y0 - 2);
    if (lit)
      lgrad(cr, cx - half, 0, cx + half, 0,
        {{0.0, rgb(200, 140, 50)}, {0.35, rgb(255, 230, 150)}, {0.5, rgb(255, 252, 230)}, {0.65, rgb(255, 230, 150)}, {1.0, rgb(200, 140, 50)}},
        0.95, true);
    else
      lgrad(cr, cx - half, 0, cx + half, 0,
        {{0.0, rgb(40, 50, 66)}, {0.3, rgb(120, 140, 170)}, {0.5, rgb(190, 205, 225)}, {0.7, rgb(90, 104, 130)}, {1.0, rgb(30, 36, 50)}}, 0.9,
        true);
    stroke(cr, rgb(150, 110, 50), 2.0);
  }
  // The brass frame: bars up the lens, a cap and a base ring.
  for (const double x : {-60.0, -20.0, 20.0, 60.0})
  {
    cairo_move_to(cr, cx + x * 0.7, top);
    cairo_curve_to(cr, cx + x * 1.1, top + 60, cx + x * 1.1, bot - 60, cx + x * 0.7, bot);
    stroke(cr, rgb(190, 150, 70), 2.4, 0.9);
  }
  ellipse(cr, cx, top, 52, 12);
  lgrad(cr, 0, top - 12, 0, top + 12, {{0.0, rgb(240, 200, 110)}, {1.0, rgb(120, 80, 30)}}, 1.0, true);
  stroke(cr, kInk, 1.8);
  cairo_rectangle(cr, cx - 60, bot - 6, 120, 12);
  lgrad(cr, 0, bot - 6, 0, bot + 6, {{0.0, rgb(240, 200, 110)}, {1.0, rgb(120, 80, 30)}}, 1.0, true);
  stroke(cr, kInk, 1.8);
  // The vent on its cap.
  cairo_move_to(cr, cx - 14, top - 10);
  cairo_line_to(cr, cx, top - 24);
  cairo_line_to(cr, cx + 14, top - 10);
  cairo_close_path(cr);
  setRgba(cr, rgb(150, 110, 50), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.4);
}

// The ship on the horizon: a small freighter in silhouette. 240 x 80; its
// waterline's middle at (120, 64).
void paintShip(cairo_t* cr)
{
  const Color c = rgb(12, 16, 24);
  cairo_move_to(cr, 20, 50);
  cairo_line_to(cr, 220, 50);
  cairo_line_to(cr, 206, 66);
  cairo_line_to(cr, 34, 66);
  cairo_close_path(cr);
  setRgba(cr, c, 1.0);
  cairo_fill(cr);
  cairo_rectangle(cr, 150, 30, 44, 20);
  cairo_rectangle(cr, 160, 18, 10, 14);
  cairo_rectangle(cr, 40, 44, 90, 6);
  setRgba(cr, c, 1.0);
  cairo_fill(cr);
  cairo_move_to(cr, 60, 50);
  cairo_line_to(cr, 60, 10);
  cairo_move_to(cr, 120, 50);
  cairo_line_to(cr, 120, 16);
  stroke(cr, c, 2.4);
  // Portholes, dimly lit.
  for (int k = 0; k < 5; ++k)
  {
    circle(cr, 154 + k * 8, 38, 1.4);
    setRgba(cr, rgb(255, 220, 150), 0.6);
    cairo_fill(cr);
  }
}

// The keeper's log: a leather logbook with brass corners, a ribbon, LOG on
// its cover. 96 x 96 over a 64 x 64 tile at (16, 16).
void paintKeepersLog(cairo_t* cr)
{
  cairo_translate(cr, 48, 50);
  cairo_rotate(cr, -0.12);
  // The page block (seen at the edge) and the cover.
  cairo_rectangle(cr, -26, -30, 54, 62);
  setRgba(cr, rgb(236, 226, 196), 1.0);
  cairo_fill(cr);
  cairo_rectangle(cr, -30, -34, 54, 64);
  lgrad(cr, -30, -34, 24, 30, {{0.0, rgb(150, 70, 44)}, {0.5, rgb(110, 46, 30)}, {1.0, rgb(60, 24, 16)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  // The spine.
  cairo_rectangle(cr, -30, -34, 9, 64);
  setRgba(cr, rgb(70, 28, 18), 1.0);
  cairo_fill(cr);
  for (const double y : {-24.0, 0.0, 22.0})
  {
    cairo_move_to(cr, -30, y);
    cairo_line_to(cr, -21, y);
    stroke(cr, rgb(220, 180, 90), 1.6);
  }
  // Brass corners.
  for (const auto& c : {std::array<double, 2>{24, -34}, std::array<double, 2>{24, 30}})
  {
    cairo_move_to(cr, c[0], c[1]);
    cairo_line_to(cr, c[0] - 10, c[1]);
    cairo_line_to(cr, c[0], c[1] + (c[1] < 0 ? 10 : -10));
    cairo_close_path(cr);
    lgrad(cr, 0, c[1] - 10, 0, c[1] + 10, {{0.0, rgb(250, 220, 130)}, {1.0, rgb(150, 110, 40)}}, 1.0);
  }
  // The label.
  cairo_rectangle(cr, -14, -18, 32, 16);
  setRgba(cr, rgb(236, 222, 186), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, rgb(90, 60, 30), 1.0);
  lettering(cr, "LOG", 2, -5.5, 11, rgb(40, 24, 16));
  // The ribbon.
  cairo_move_to(cr, 10, 30);
  cairo_curve_to(cr, 12, 36, 8, 40, 12, 44);
  stroke(cr, rgb(200, 30, 40), 3.0);
  // A gleam.
  cairo_move_to(cr, -18, -30);
  cairo_line_to(cr, 18, -30);
  stroke(cr, rgb(255, 230, 200), 1.2, 0.45);
}

// The keeper's desk and his oil lamp. 160 x 120, its feet's middle at (80, 120).
void paintDesk(cairo_t* cr)
{
  cairo_rectangle(cr, 16, 56, 128, 10);
  lgrad(cr, 0, 56, 0, 66, {{0.0, rgb(150, 104, 66)}, {1.0, rgb(80, 52, 32)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  for (const double x : {22.0, 130.0})
  {
    cairo_rectangle(cr, x, 66, 8, 54);
    lgrad(cr, x, 0, x + 8, 0, {{0.0, rgb(120, 84, 52)}, {1.0, rgb(60, 40, 24)}}, 1.0, true);
    stroke(cr, kInk, 1.2);
  }
  cairo_rectangle(cr, 34, 66, 50, 22);
  lgrad(cr, 0, 66, 0, 88, {{0.0, rgb(120, 84, 52)}, {1.0, rgb(70, 46, 28)}}, 1.0, true);
  stroke(cr, kInk, 1.2);
  circle(cr, 59, 77, 2.0);
  setRgba(cr, rgb(220, 190, 110), 1.0);
  cairo_fill(cr);
  // The oil lamp: a brass base, a glass chimney.
  cairo_move_to(cr, 108, 56);
  cairo_line_to(cr, 112, 44);
  cairo_line_to(cr, 128, 44);
  cairo_line_to(cr, 132, 56);
  cairo_close_path(cr);
  lgrad(cr, 0, 44, 0, 56, {{0.0, rgb(240, 200, 110)}, {1.0, rgb(130, 90, 30)}}, 1.0, true);
  stroke(cr, kInk, 1.2);
  ellipse(cr, 120, 32, 8, 13);
  setRgba(cr, rgb(255, 230, 170), 0.55);
  cairo_fill_preserve(cr);
  stroke(cr, rgb(120, 100, 70), 1.0);
  ellipse(cr, 120, 34, 2.4, 4.2);
  setRgba(cr, rgb(255, 210, 120), 1.0);
  cairo_fill(cr);
}

// --- Painters: the Tesla Rod, rings, Bungee's lighthouse -----------------------------------------------

// A Tesla rod: a copper rod, a coil wound near its back end and a glass
// bulb tip there, the sharp end on the right. 104 x 32, centred.
void paintRod(cairo_t* cr)
{
  cairo_move_to(cr, 22, 13);
  cairo_line_to(cr, 88, 13);
  cairo_line_to(cr, 102, 16);
  cairo_line_to(cr, 88, 19);
  cairo_line_to(cr, 22, 19);
  cairo_close_path(cr);
  lgrad(cr, 0, 13, 0, 19, {{0.0, rgb(255, 200, 150)}, {0.4, kCopper}, {1.0, rgb(110, 50, 24)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  for (int k = 0; k < 7; ++k)
  {
    const double x = 26 + k * 3.4;
    cairo_move_to(cr, x, 10);
    cairo_line_to(cr, x + 2, 22);
    stroke(cr, rgb(200, 110, 50), 2.0);
  }
  cairo_rectangle(cr, 25, 10, 25, 12);
  stroke(cr, kInk, 0.8, 0.6);
  // The bulb.
  circle(cr, 14, 16, 8);
  lgrad(cr, 0, 8, 0, 24, {{0.0, rgb(220, 240, 255)}, {1.0, rgb(120, 160, 220)}}, 0.75, true);
  stroke(cr, kInk, 1.4);
  circle(cr, 14, 16, 3);
  setRgba(cr, rgb(255, 255, 255), 0.95);
  cairo_fill(cr);
  circle(cr, 11, 13, 1.4);
  setRgba(cr, rgb(255, 255, 255), 0.9);
  cairo_fill(cr);
}

// The Tesla Rod's icon: the rod on a slant, its bulb lit, two sparks. 64 x 64.
void paintRodIcon(cairo_t* cr)
{
  glow(cr, 12, 52, 18, rgb(170, 210, 255), 0.7);
  cairo_save(cr);
  cairo_translate(cr, 32, 32);
  cairo_rotate(cr, -kPi / 4.0);
  cairo_scale(cr, 0.75, 0.75);
  cairo_translate(cr, -52, -16);
  paintRod(cr);
  cairo_restore(cr);
  for (const auto& s : {std::array<double, 4>{8, 36, 2, 30}, std::array<double, 4>{24, 56, 30, 60}})
  {
    cairo_move_to(cr, s[0], s[1]);
    cairo_line_to(cr, (s[0] + s[2]) * 0.5 + 3, (s[1] + s[3]) * 0.5 - 3);
    cairo_line_to(cr, s[2], s[3]);
    stroke(cr, rgb(220, 240, 255), 1.6);
  }
}

// A storm ring: a hoop of blue-white light with a hot core (drawn additive).
// 128 x 128, centred.
void paintRing(cairo_t* cr)
{
  for (int k = 0; k < 3; ++k)
  {
    cairo_arc(cr, 64, 64, 44, 0, 2 * kPi);
    stroke(cr, k == 2 ? rgb(255, 255, 255) : kBolt, k == 0 ? 16.0 : (k == 1 ? 8.0 : 3.0), k == 0 ? 0.25 : (k == 1 ? 0.55 : 0.95));
  }
  // Little zigzags round it.
  for (int k = 0; k < 8; ++k)
  {
    const double a = k * kPi / 4.0 + 0.2;
    const double x0 = 64 + std::cos(a) * 40, y0 = 64 + std::sin(a) * 40;
    const double x1 = 64 + std::cos(a + 0.12) * 54, y1 = 64 + std::sin(a + 0.12) * 54;
    cairo_move_to(cr, x0, y0);
    cairo_line_to(cr, (x0 + x1) * 0.5 + 4 * std::sin(a), (y0 + y1) * 0.5 - 4 * std::cos(a));
    cairo_line_to(cr, x1, y1);
    stroke(cr, rgb(200, 225, 255), 1.4, 0.8);
  }
}

// Bungee's lighthouse: a tapering tower in red and white bands, a gallery,
// the lantern and its dome. 480 x 760; its foot's middle at (240, 740),
// the gallery at y 160, the lantern's glass 70..150.
void paintTower(cairo_t* cr)
{
  const double cx = 240, foot = 740, top = 170;
  auto half = [&](double y) { return 110.0 + (y - top) / (foot - top) * 110.0; };
  // The tower body, banded.
  cairo_save(cr);
  cairo_move_to(cr, cx - half(top), top);
  cairo_line_to(cr, cx + half(top), top);
  cairo_line_to(cr, cx + half(foot), foot);
  cairo_line_to(cr, cx - half(foot), foot);
  cairo_close_path(cr);
  cairo_clip(cr);
  for (int b = 0; b < 6; ++b)
  {
    const double y0 = top + (foot - top) * b / 6.0;
    cairo_rectangle(cr, 0, y0, 480, (foot - top) / 6.0 + 1);
    lgrad(cr, cx - 220, 0, cx + 220, 0,
      b % 2 ? std::initializer_list<std::pair<double, Color>>{{0.0, rgb(120, 24, 24)}, {0.4, rgb(196, 50, 44)}, {1.0, rgb(70, 14, 16)}}
            : std::initializer_list<std::pair<double, Color>>{{0.0, rgb(150, 150, 150)}, {0.4, rgb(232, 232, 228)}, {1.0, rgb(90, 92, 98)}});
  }
  // Rain streaks.
  Rng rng(31501u);
  for (int i = 0; i < 40; ++i)
  {
    const double x = rng.range(20, 460), y = rng.range(float(top), float(foot) - 40);
    cairo_move_to(cr, x, y);
    cairo_line_to(cr, x, y + rng.range(20, 60));
    stroke(cr, rgb(20, 20, 30), 2.0, 0.18);
  }
  cairo_restore(cr);
  cairo_move_to(cr, cx - half(top), top);
  cairo_line_to(cr, cx - half(foot), foot);
  cairo_move_to(cr, cx + half(top), top);
  cairo_line_to(cr, cx + half(foot), foot);
  stroke(cr, kInk, 3.0);
  // The door and two windows.
  cairo_move_to(cr, cx - 30, foot);
  cairo_line_to(cr, cx - 30, foot - 80);
  cairo_curve_to(cr, cx - 30, foot - 110, cx + 30, foot - 110, cx + 30, foot - 80);
  cairo_line_to(cr, cx + 30, foot);
  cairo_close_path(cr);
  setRgba(cr, rgb(40, 30, 26), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 2.0);
  for (const double y : {top + 150.0, top + 330.0})
  {
    cairo_rectangle(cr, cx - 14, y, 28, 44);
    setRgba(cr, rgb(255, 214, 130), 0.85);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 2.0);
  }
  // The gallery and its rail.
  cairo_rectangle(cr, cx - 150, top - 14, 300, 16);
  lgrad(cr, 0, top - 14, 0, top + 2, {{0.0, rgb(80, 80, 90)}, {1.0, rgb(24, 24, 30)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  cairo_move_to(cr, cx - 150, top - 44);
  cairo_line_to(cr, cx + 150, top - 44);
  stroke(cr, rgb(60, 60, 70), 4.0);
  for (double x = cx - 146; x <= cx + 146; x += 24)
  {
    cairo_move_to(cr, x, top - 44);
    cairo_line_to(cr, x, top - 14);
    stroke(cr, rgb(60, 60, 70), 2.4);
  }
  // The lantern: glass panes in iron, the lamp inside.
  cairo_rectangle(cr, cx - 80, 70, 160, 86);
  lgrad(cr, 0, 70, 0, 156, {{0.0, rgb(255, 236, 170)}, {1.0, rgb(230, 170, 70)}}, 0.75, true);
  stroke(cr, kInk, 2.4);
  for (double x = cx - 40; x <= cx + 40; x += 40)
  {
    cairo_move_to(cr, x, 70);
    cairo_line_to(cr, x, 156);
    stroke(cr, rgb(30, 30, 36), 3.0);
  }
  glow(cr, cx, 112, 50, rgb(255, 250, 220), 0.85);
  // The dome and its finial (the rope's post is drawn by the level).
  cairo_move_to(cr, cx - 96, 72);
  cairo_curve_to(cr, cx - 90, 20, cx + 90, 20, cx + 96, 72);
  cairo_close_path(cr);
  lgrad(cr, 0, 22, 0, 72, {{0.0, rgb(200, 70, 60)}, {1.0, rgb(90, 20, 20)}}, 1.0, true);
  stroke(cr, kInk, 2.4);
}

// The rope's post on the dome: an iron mast with a ring. 48 x 80; the ring
// at (24, 14).
void paintBungeePost(cairo_t* cr)
{
  cairo_move_to(cr, 24, 20);
  cairo_line_to(cr, 24, 80);
  stroke(cr, kInk, 8.0);
  cairo_move_to(cr, 24, 20);
  cairo_line_to(cr, 24, 80);
  stroke(cr, rgb(120, 120, 130), 4.5);
  circle(cr, 24, 14, 8);
  stroke(cr, kInk, 5.0);
  circle(cr, 24, 14, 8);
  stroke(cr, rgb(200, 200, 210), 2.6);
}

// The cave's arch rim: rock filling the window's top corners (so the hole
// reads as an arch) with a wet lit edge round the opening. 192 x 256 over a
// 3 x 4 block window.
void paintArch(cairo_t* cr)
{
  cairo_move_to(cr, 0, 0);
  cairo_line_to(cr, 192, 0);
  cairo_line_to(cr, 192, 256);
  cairo_line_to(cr, 176, 256);
  cairo_line_to(cr, 176, 100);
  cairo_curve_to(cr, 176, 30, 16, 30, 16, 100);
  cairo_line_to(cr, 16, 256);
  cairo_line_to(cr, 0, 256);
  cairo_close_path(cr);
  lgrad(cr, 0, 0, 0, 256, {{0.0, rgb(26, 28, 36)}, {1.0, rgb(14, 16, 22)}});
  cairo_move_to(cr, 16, 250);
  cairo_line_to(cr, 16, 100);
  cairo_curve_to(cr, 16, 30, 176, 30, 176, 100);
  cairo_line_to(cr, 176, 250);
  stroke(cr, rgb(150, 170, 200), 2.0, 0.55);
  // The sill.
  cairo_rectangle(cr, 8, 240, 176, 16);
  lgrad(cr, 0, 240, 0, 256, {{0.0, rgb(70, 76, 90)}, {1.0, rgb(20, 22, 30)}});
}

// The blast's air: a soft band, brightest at the mouth, fading out along
// its length. 512 x 192, the mouth's middle at (0, 96).
void paintBlast(cairo_t* cr)
{
  cairo_move_to(cr, 0, 46);
  cairo_curve_to(cr, 180, 10, 360, 0, 512, 0);
  cairo_line_to(cr, 512, 192);
  cairo_curve_to(cr, 360, 192, 180, 182, 0, 146);
  cairo_close_path(cr);
  cairo_pattern_t* g = cairo_pattern_create_linear(0, 0, 512, 0);
  cairo_pattern_add_color_stop_rgba(g, 0.0, 1, 1, 1, 0.5);
  cairo_pattern_add_color_stop_rgba(g, 0.4, 1, 1, 1, 0.2);
  cairo_pattern_add_color_stop_rgba(g, 1.0, 1, 1, 1, 0.0);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
}

// The lamp's beam: a long soft wedge from its tip on the left. 900 x 200,
// the tip at (0, 100).
void paintBeam(cairo_t* cr)
{
  cairo_move_to(cr, 0, 94);
  cairo_line_to(cr, 900, 0);
  cairo_line_to(cr, 900, 200);
  cairo_line_to(cr, 0, 106);
  cairo_close_path(cr);
  cairo_pattern_t* g = cairo_pattern_create_linear(0, 0, 900, 0);
  cairo_pattern_add_color_stop_rgba(g, 0.0, 1, 1, 1, 0.9);
  cairo_pattern_add_color_stop_rgba(g, 0.3, 1, 1, 1, 0.35);
  cairo_pattern_add_color_stop_rgba(g, 1.0, 1, 1, 1, 0.0);
  cairo_set_source(cr, g);
  cairo_fill(cr);
}

// --- Live drawing helpers ------------------------------------------------------------------------

struct Pt
{
  float x, y;
};

// A jagged path from a to b in `n` steps, kicked sideways up to `jag` px.
std::vector<Pt> jagged(Pt a, Pt b, int n, float jag, std::uint32_t seed)
{
  std::vector<Pt> p;
  p.reserve(std::size_t(n + 1));
  const float dx = b.x - a.x, dy = b.y - a.y, len = std::sqrt(dx * dx + dy * dy) + 0.001f;
  for (int i = 0; i <= n; ++i)
  {
    const float u = float(i) / float(n);
    float x = a.x + dx * u, y = a.y + dy * u;
    if (i > 0 && i < n)
    {
      const float j = (unit(hash2(int(seed), i)) * 2.0f - 1.0f) * jag;
      x += -dy / len * j;
      y += dx / len * j;
    }
    p.push_back({x, y});
  }
  return p;
}

// Lightning along a path: a wide blue glow, a brighter channel, a white core.
void drawBolt(Renderer& r, const std::vector<Pt>& p, float width, float a)
{
  if (a <= 0.01f)
    return;
  for (std::size_t i = 1; i < p.size(); ++i)
    r.drawLine(p[i - 1].x, p[i - 1].y, p[i].x, p[i].y, width * 4.0f, rgba(120, 160, 255, int(50 * a)), Blend::Add);
  for (std::size_t i = 1; i < p.size(); ++i)
    r.drawLine(p[i - 1].x, p[i - 1].y, p[i].x, p[i].y, width * 1.8f, rgba(170, 200, 255, int(170 * a)), Blend::Add);
  for (std::size_t i = 1; i < p.size(); ++i)
    r.drawLine(p[i - 1].x, p[i - 1].y, p[i].x, p[i].y, width * 0.7f, rgba(255, 255, 255, int(255 * a)), Blend::Add);
}

// Crackle over a box of metal: little arcs jumping off it, more as `amount`
// grows, re-rolled every other frame.
void crackle(Renderer& r, float x, float y, float w, float h, int frame, float amount, std::uint32_t seed)
{
  const int n = 2 + int(amount * (w + h) / 40.0f);
  for (int k = 0; k < n; ++k)
  {
    const std::uint32_t hh = hash2(int(seed) + k * 7, frame / 2);
    if (unit(hh) > 0.25f + 0.75f * amount)
      continue;
    // A start on the edge of the box, outward and a bit up.
    const float side = unit(hash2(int(hh), 1));
    Pt a{x + side * w, y + (unit(hash2(int(hh), 2)) < 0.6f ? 0.0f : h * unit(hash2(int(hh), 3)))};
    const float ang = -kPi * (0.15f + 0.7f * unit(hash2(int(hh), 4)));
    const float len = 14.0f + 26.0f * amount * unit(hash2(int(hh), 5));
    Pt b{a.x + std::cos(ang) * len, a.y + std::sin(ang) * len};
    drawBolt(r, jagged(a, b, 3, 6.0f, hh), 1.6f, 0.5f + 0.5f * amount);
  }
}

// Sheet lightning: how bright the cloud is lit this frame (0..1) and where.
float sheetFlash(int frame, float& sx, float& sy, bool& bolt)
{
  constexpr int kWindow = 170;
  const int k = frame / kWindow, t = frame % kWindow;
  const std::uint32_t h = hash2(k, 3131);
  sx = float(h % 1280u);
  sy = 60.0f + float((h >> 11) % 200u);
  bolt = (h >> 5) % 3u == 0;
  if ((h >> 8) % 4u == 0)
    return 0.0f; // a quiet spell
  const int at = int((h >> 14) % 120u);
  static constexpr std::array<float, 16> kCurve{{1.0f, 0.4f, 0.85f, 0.55f, 0.3f, 0.7f, 0.45f, 0.3f, 0.2f, 0.14f, 0.1f, 0.07f, 0.05f, 0.03f,
    0.02f, 0.01f}};
  const int u = t - at;
  return u >= 0 && u < int(kCurve.size()) ? kCurve[std::size_t(u)] : 0.0f;
}

// --- Level 31's layout (in blocks) --------------------------------------------------------------------

bool isRelay(const StormState& s)
{
  if (s.lamp < 0)
    return false;
  for (const auto& m : s.metals)
    if (m.id == "flight1")
      return true;
  return false;
}

struct BlockRect
{
  int x0, y0, x1, y1;
  bool has(int bx, int by) const { return bx >= x0 && bx <= x1 && by >= y0 && by <= y1; }
};

constexpr BlockRect kBeach{0, 92, 40, 99};
constexpr BlockRect kBoardwalk{20, 92, 32, 92};
constexpr BlockRect kHut{6, 88, 12, 91};
constexpr BlockRect kHutRoof{6, 87, 12, 87};
constexpr BlockRect kCaves{29, 52, 75, 67};
constexpr BlockRect kSmugglers{30, 68, 44, 75};
constexpr BlockRect kTower{64, 4, 91, 35};
constexpr BlockRect kTowerInside{65, 6, 90, 34};
constexpr BlockRect kLampRoom{78, 6, 90, 11};
constexpr BlockRect kHatch{32, 68, 33, 69};
constexpr std::array<BlockRect, 3> kArches{{{33, 55, 35, 58}, {48, 55, 50, 58}, {62, 55, 64, 58}}};
constexpr std::array<BlockRect, 4> kSlits{{{70, 21, 70, 22}, {87, 19, 87, 20}, {75, 29, 75, 30}, {70, 8, 70, 9}}};
constexpr std::array<BlockRect, 3> kLandings{{{84, 26, 90, 26}, {65, 17, 67, 17}, {78, 12, 90, 12}}};

bool inside(const StormState& s, int bx, int by)
{
  if (!isRelay(s))
    return false;
  return kCaves.has(bx, by) || kSmugglers.has(bx, by) || kTowerInside.has(bx, by) || kHut.has(bx, by);
}

// Is a metal group a stair flight (one-way grating)?
bool isFlight(const MetalGroup& g) { return g.id.rfind("flight", 0) == 0; }

} // namespace

// --- Behind the tiles ------------------------------------------------------------------------------------

void drawStormSet(Renderer& r, const World& w, const Art& art, float camX, float camY, int frame)
{
  const auto& s = w.storm();
  const auto& map = w.map();
  const auto& lv = w.level();
  const float hz = kStormHorizonY;
  const bool relay = isRelay(s);
  // Sheet lightning in the cloud, and now and then a bolt far out at sea.
  {
    float sx = 0.0f, sy = 0.0f;
    bool bolt = false;
    const float f = sheetFlash(frame, sx, sy, bolt);
    if (f > 0.0f)
    {
      drawGlow(r, art, sx, sy, 560.0f, rgb(130, 150, 220), 0.55f * f);
      drawGlow(r, art, sx + 140.0f, sy + 60.0f, 360.0f, rgb(190, 205, 255), 0.4f * f);
      r.fillRect(0.0f, 0.0f, float(kScreenW), hz, rgba(110, 130, 190, int(60.0f * f)), Blend::Add);
      if (bolt && f > 0.35f)
      {
        const std::uint32_t seed = hash2(frame / 170, 77);
        const float bx = sx + float(int(seed % 200u) - 100);
        drawBolt(r, jagged({sx, sy + 40.0f}, {bx, hz}, 9, 22.0f, seed), 1.4f, f * 0.8f);
      }
    }
  }
  // The sea: whitecaps coming and going, drifting with the swell.
  for (int i = 0; i < 46; ++i)
  {
    const std::uint32_t h = hash2(i, 3141);
    const float u = unit(h), d = u * u;
    const float y = hz + 6.0f + (float(kScreenH) - hz) * d;
    const float x = std::fmod(float(h % 1600u) - camX * (0.02f + 0.1f * d) + float(frame) * (0.15f + 0.6f * d) + 16000.0f, 1600.0f) - 160.0f;
    const float life = std::sin(float(frame) * 0.02f + float(i) * 1.7f);
    if (life <= 0.0f)
      continue;
    const float wv = 6.0f + 60.0f * d;
    r.drawLine(x, y, x + wv, y - 1.0f - 2.0f * d, 1.5f + 3.0f * d, rgba(210, 222, 236, int(110.0f * life * (0.4f + 0.6f * d))));
    r.drawLine(x + wv * 0.2f, y + 2.0f + 3.0f * d, x + wv * 0.9f, y + 2.0f + 3.0f * d, 1.0f + 2.0f * d,
      rgba(150, 170, 200, int(60.0f * life)));
  }
  // The ship on the horizon answers the lamp: G-U-N.
  if (s.morse >= 0)
  {
    const float x = 1060.0f - std::fmod(camX * 0.03f, 900.0f), y = hz + 2.0f;
    r.draw(baked(art, r, "ship", 240, 80, 120, 64, paintShip), x, y);
    if (morseLight(s.morse))
    {
      drawGlow(r, art, x - 60.0f, y - 56.0f, 40.0f, rgb(255, 250, 210), 0.9f);
      r.fillRect(x - 63.0f, y - 59.0f, 6.0f, 6.0f, rgb(255, 255, 240));
      drawGlow(r, art, x - 60.0f, y + 4.0f, 30.0f, rgb(255, 250, 210), 0.3f); // on the water
    }
  }
  // Rain far off: fine and faint.
  for (int i = 0; i < 70; ++i)
  {
    const std::uint32_t h = hash2(i, 2718);
    const float x = std::fmod(float(h % 1400u) - camX * 0.2f + float(frame) * 3.0f + 14000.0f, 1400.0f) - 60.0f;
    const float y = std::fmod(float((h >> 11) % 800u) + float(frame) * 11.0f - camY * 0.2f + 80000.0f, 800.0f) - 40.0f;
    r.drawLine(x, y, x + 3.0f, y + 14.0f, 1.0f, rgba(150, 170, 210, 40));
  }
  // The surf rolling in at the foot of the beach.
  if (relay)
  {
    const float shore = float(kBeach.y0) * kTilePx - camY;
    const float x0 = std::max(0.0f, -camX), x1 = std::min(float(kScreenW), float(kBeach.x1 + 1) * kTilePx - camX);
    if (x1 > x0 && shore > hz && shore < float(kScreenH) + 40.0f)
    {
      const float span = std::min(110.0f, shore - hz - 4.0f);
      for (int k = 0; k < 4; ++k)
      {
        const float u = std::fmod(float(frame) / 260.0f + float(k) * 0.25f, 1.0f);
        const float y = shore - span * (1.0f - u) * (1.0f - u) - 3.0f;
        const float a = std::sin(u * float(kPi)) * (0.35f + 0.5f * u);
        float px = x0, py = y;
        for (float x = x0; x <= x1 + 24.0f; x += 24.0f)
        {
          const float yy = y + std::sin((x + camX) * 0.013f + float(k) * 2.0f + float(frame) * 0.02f) * (2.0f + 3.0f * u);
          if (x > x0)
          {
            r.drawLine(px, py, x, yy, 2.0f + 5.0f * u, rgba(220, 230, 240, int(150.0f * a)));
            r.drawLine(px, py + 4.0f + 4.0f * u, x, yy + 4.0f + 4.0f * u, 2.0f, rgba(140, 160, 190, int(80.0f * a)));
          }
          px = x;
          py = yy;
        }
      }
      // The wash on the sand's edge.
      r.fillRect(x0, shore - 6.0f, x1 - x0, 6.0f, rgba(200, 215, 235, 70 + int(40.0f * std::sin(float(frame) * 0.03f))));
    }
  }
  // Bungee: the lighthouse behind the post.
  if (s.bungee && s.tower.w > 0)
  {
    const float x = (float(s.tower.x) + float(s.tower.w) * 0.5f) * kCellPx - camX;
    const float y = float(s.tower.y + s.tower.h) * kCellPx - camY;
    const float sc = float(s.tower.w) * kCellPx / 440.0f;
    DrawOpts o;
    o.scale = sc;
    r.draw(baked(art, r, "tower", 480, 760, 240, 740, paintTower), x, y, o);
    // The lamp turning in it.
    const float ly = y - (740.0f - 112.0f) * sc;
    const float ang = float(frame) * 0.025f, c = std::cos(ang);
    DrawOpts b;
    b.blend = Blend::Add;
    b.tint = rgb(255, 240, 190);
    b.alpha = 0.22f;
    b.scale = 1.0f;
    const Texture& beam = baked(art, r, "beam", 900, 200, 0, 100, paintBeam);
    DrawOpts bo = b;
    bo.angle = c >= 0.0f ? 0.0f : 180.0f;
    bo.scale = std::max(0.05f, std::abs(c));
    r.draw(beam, x, ly, bo);
    if (std::sin(ang) > 0.0f && std::abs(c) < 0.3f)
      drawGlow(r, art, x, ly, 160.0f, rgb(255, 245, 210), 0.6f * (1.0f - std::abs(c) / 0.3f));
    drawGlow(r, art, x, ly, 70.0f, rgb(255, 230, 160), 0.5f);
  }
  if (!relay)
    return;
  // The back walls: the hut, the caves, the smugglers' room, the tower.
  const int bx0 = std::max(0, int(camX / kTilePx) - 1), bx1 = std::min(lv.width - 1, int((camX + float(kScreenW)) / kTilePx) + 1);
  const int by0 = std::max(0, int(camY / kTilePx) - 1), by1 = std::min(lv.height - 1, int((camY + float(kScreenH)) / kTilePx) + 1);
  auto inArch = [](int bx, int by) {
    for (const auto& a : kArches)
      if (a.has(bx, by))
        return true;
    return false;
  };
  auto inSlit = [](int bx, int by) {
    for (const auto& a : kSlits)
      if (a.has(bx, by))
        return true;
    return false;
  };
  for (int by = by0; by <= by1; ++by)
    for (int bx = bx0; bx <= bx1; ++bx)
    {
      if (map.block(bx, by) == Tile::Solid)
        continue;
      const float x = float(bx) * kTilePx - camX, y = float(by) * kTilePx - camY;
      const std::uint32_t h = hash2(bx, by);
      if (kCaves.has(bx, by))
      {
        if (inArch(bx, by))
          continue;
        DrawOpts o;
        o.tint = rgb(54, 58, 72);
        r.draw(art.solid[h % 9u == 0 ? 2u : (h % 5u == 0 ? 1u : 0u)], x, y, o);
      }
      else if (kSmugglers.has(bx, by))
      {
        DrawOpts o;
        o.tint = rgb(66, 62, 64);
        r.draw(art.solid[h % 7u == 0 ? 2u : 0u], x, y, o);
      }
      else if (kTowerInside.has(bx, by))
      {
        if (kLampRoom.has(bx, by))
        {
          r.draw(baked(art, r, "pane", 64, 64, 0, 0, paintLanternPane), x, y);
          continue;
        }
        if (inSlit(bx, by))
          continue;
        DrawOpts o;
        o.tint = rgb(60, 62, 74);
        r.draw(baked(art, r, "masonry0", 64, 64, 0, 0, [](cairo_t* cr) { paintMasonry(cr, false); }), x, y, o);
      }
      else if (kHut.has(bx, by))
      {
        DrawOpts o;
        o.tint = rgb(150, 150, 160);
        r.draw(baked(art, r, "hutwall", 64, 64, 0, 0, paintHutWall), x, y, o);
      }
    }
  // The caves' arches, open on the storm, their rims wet.
  for (const auto& a : kArches)
  {
    const float x = float(a.x0) * kTilePx - camX, y = float(a.y0) * kTilePx - camY;
    if (!offScreen(x, y, 192.0f, 256.0f))
      r.draw(baked(art, r, "arch", 192, 256, 0, 0, paintArch), x, y);
  }
  // The tower's slit windows and the hut's window.
  for (const auto& sl : kSlits)
  {
    const float x = float(sl.x0) * kTilePx - camX, y = float(sl.y0) * kTilePx - camY;
    if (!offScreen(x, y, 64.0f, 128.0f))
      r.draw(baked(art, r, "slit", 64, 128, 0, 0, paintTowerWindow), x, y);
  }
  {
    const float x = float(kHut.x0 + 2) * kTilePx + 16.0f - camX, y = float(kHut.y0) * kTilePx + 30.0f - camY;
    if (!offScreen(x, y, 96.0f, 80.0f))
    {
      r.draw(baked(art, r, "hutwindow", 96, 80, 0, 0, paintHutWindow), x, y);
      // A lantern hanging in the hut, its warm light.
      drawGlow(r, art, x + 230.0f, y - 6.0f, 120.0f, rgb(255, 190, 110), 0.35f);
      r.drawLine(x + 230.0f, y - 30.0f, x + 230.0f, y - 14.0f, 2.0f, rgb(40, 34, 30));
      r.fillRect(x + 222.0f, y - 14.0f, 16.0f, 20.0f, rgb(60, 50, 40));
      r.fillRect(x + 225.0f, y - 11.0f, 10.0f, 14.0f, rgb(255, 210, 130));
    }
  }
  // A lantern in the smugglers' room.
  {
    const float x = 37.5f * kTilePx - camX, y = 71.0f * kTilePx - camY;
    if (!offScreen(x - 150.0f, y - 150.0f, 300.0f, 300.0f))
    {
      drawGlow(r, art, x, y + 20.0f, 220.0f, rgb(255, 170, 90), 0.3f + 0.04f * std::sin(float(frame) * 0.3f));
      r.drawLine(x, y - 64.0f, x, y, 2.0f, rgb(30, 26, 24));
      r.fillRect(x - 9.0f, y, 18.0f, 24.0f, rgb(50, 40, 32));
      r.fillRect(x - 6.0f, y + 4.0f, 12.0f, 16.0f, rgb(255, 200, 120));
    }
  }
  // The lamp: its room aglow and its beam turning out over the sea.
  if (s.lamp >= 0 && s.machines[std::size_t(s.lamp)].powered)
  {
    const auto& m = s.machines[std::size_t(s.lamp)];
    const float cx = (float(m.box.x) + float(m.box.w) * 0.5f) * kCellPx - camX;
    const float cy = (float(m.box.y) + float(m.box.h) * 0.42f) * kCellPx - camY;
    if (!offScreen(cx - 1200.0f, cy - 300.0f, 2400.0f, 600.0f))
    {
      drawGlow(r, art, cx, cy, 380.0f, rgb(255, 210, 130), 0.35f);
      const float ang = float(frame) * 0.03f, c = std::cos(ang);
      DrawOpts b;
      b.blend = Blend::Add;
      b.tint = rgb(255, 240, 190);
      b.alpha = 0.3f;
      b.angle = c >= 0.0f ? 0.0f : 180.0f;
      b.scale = std::max(0.05f, std::abs(c)) * 1.6f;
      r.draw(baked(art, r, "beam", 900, 200, 0, 100, paintBeam), cx, cy, b);
      if (std::sin(ang) > 0.0f && std::abs(c) < 0.3f)
        drawGlow(r, art, cx, cy, 260.0f, rgb(255, 245, 220), 0.7f * (1.0f - std::abs(c) / 0.3f));
    }
  }
}

bool drawStormShot(Renderer& r, const Art& art, const Projectile& pr, float cx, float cy, int frame)
{
  if (pr.kind != ShotKind::Proto || pr.proto != int(ProtoId::TeslaRod))
    return false;
  const int d = pr.dx < 0 ? -1 : 1;
  // Sparks streaming off it.
  for (int k = 0; k < 6; ++k)
  {
    const std::uint32_t h = hash2(pr.age * 7 + k, frame);
    const float back = 30.0f + float(k) * 12.0f + unit(h) * 10.0f;
    const float sy = (unit(hash2(int(h), 3)) - 0.5f) * 14.0f;
    r.fillRect(cx - float(d) * back, cy + sy, 3.0f, 3.0f, rgba(190, 220, 255, 220 - k * 30), Blend::Add);
  }
  drawGlow(r, art, cx - float(d) * 38.0f, cy, 34.0f, rgb(170, 210, 255), 0.5f + 0.2f * std::sin(float(frame) * 0.8f));
  r.draw(bakedSprite(art, r, "rod", 104, 32, 52, 16, paintRod).get(d), cx, cy);
  return true;
}

void drawTeslaIcon(Renderer& r, const Art& art, float x, float y, float scale, int frame)
{
  DrawOpts o;
  o.scale = scale;
  r.draw(baked(art, r, "rodicon", 64, 64, 0, 0, paintRodIcon), x, y, o);
  // The bulb pulses.
  drawGlow(r, art, x + 12.0f * scale, y + 52.0f * scale, 16.0f * scale, rgb(170, 210, 255), 0.4f + 0.2f * std::sin(float(frame) * 0.15f));
}

// --- Behind the runner -----------------------------------------------------------------------------------

void World::drawStormBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& s = mStorm;
  const bool relay = isRelay(s);
  const int bx0 = std::max(0, int(camX / kTilePx) - 2), bx1 = std::min(mLevel->width - 1, int((camX + float(kScreenW)) / kTilePx) + 2);
  const int by0 = std::max(0, int(camY / kTilePx) - 2), by1 = std::min(mLevel->height - 1, int((camY + float(kScreenH)) / kTilePx) + 2);
  auto solid = [&](int bx, int by) { return mMap.block(bx, by) == Tile::Solid; };
  // Metal: which blocks belong to which piece.
  auto metalAt = [&](int bx, int by) -> const MetalGroup* {
    for (const auto& g : s.metals)
      for (const auto& b : g.boxes)
        if (b.contains(bx * kCellsPerTile, by * kCellsPerTile))
          return &g;
    return nullptr;
  };
  if (relay)
  {
    for (int by = by0; by <= by1; ++by)
      for (int bx = bx0; bx <= bx1; ++bx)
      {
        const float x = float(bx) * kTilePx - camX, y = float(by) * kTilePx - camY;
        const Tile t = mMap.block(bx, by);
        const bool top = t == Tile::Solid && !solid(bx, by - 1);
        const MetalGroup* g = metalAt(bx, by);
        if (t == Tile::Solid && kBeach.has(bx, by))
        {
          int depth = 0;
          while (depth < 3 && solid(bx, by - depth - 1))
            ++depth;
          static constexpr int kShade[4] = {255, 214, 180, 150};
          DrawOpts o;
          o.tint = rgb(kShade[depth], kShade[depth], kShade[depth]);
          if (top)
            r.draw(baked(mArt, r, "sandtop", 64, 72, 0, 8, [](cairo_t* cr) { paintSand(cr, true); }), x, y, o);
          else
            r.draw(baked(mArt, r, "sand", 64, 64, 0, 0, [](cairo_t* cr) { paintSand(cr, false); }), x, y, o);
          if (top && kBoardwalk.has(bx, by))
            r.draw(baked(mArt, r, "boardwalk", 64, 30, 0, 4, paintBoardwalk), x, y - 10.0f);
        }
        else if (t == Tile::Solid && kHutRoof.has(bx, by))
        {
          const int v = bx == kHutRoof.x0 ? 1 : (bx == kHutRoof.x1 ? 2 : 0);
          r.draw(baked(mArt, r, "roof" + std::to_string(v), 64, 64, 0, 0, [v](cairo_t* cr) { paintRoof(cr, v); }), x, y);
        }
        else if (g && t == Tile::Solid && g->id == "grate")
          r.draw(baked(mArt, r, "grate", 64, 24, 0, 2, paintGrate), x, y);
        else if (g && t == Tile::Solid)
          r.draw(baked(mArt, r, top ? "metaltop" : "metal", 64, 64, 0, 0, [top](cairo_t* cr) { paintMetalBlock(cr, top); }), x, y);
        else if (g && t != Tile::Solid && !isFlight(*g))
        {
          // An antenna mast (or the roof's rod): the top block carries the rod.
          const bool mastTop = metalAt(bx, by - 1) != g;
          if (mastTop)
            r.draw(baked(mArt, r, "masttop", 64, 104, 0, 40, [](cairo_t* cr) { paintMast(cr, true); }), x, y);
          else
            r.draw(baked(mArt, r, "mast", 64, 64, 0, 0, [](cairo_t* cr) { paintMast(cr, false); }), x, y);
          if (mastTop)
          {
            const bool on = (frame / 40 + bx) % 2 == 0;
            drawGlow(r, mArt, x + 32.0f, y - 2.0f, 26.0f, rgb(255, 60, 50), on ? 0.7f : 0.15f);
            r.fillRect(x + 28.0f, y - 6.0f, 8.0f, 6.0f, on ? rgb(255, 120, 100) : rgb(110, 30, 30));
          }
        }
        else if (t == Tile::Platform && !g)
          r.draw(baked(mArt, r, "planktop", 64, 28, 0, 0, paintPlankTop), x, y);
        else if (t == Tile::Solid && kTower.has(bx, by))
        {
          bool landing = false;
          for (const auto& l : kLandings)
            landing = landing || l.has(bx, by);
          if (landing && by != kLandings[2].y0)
            r.draw(baked(mArt, r, top ? "woodtop" : "wood", 64, 64, 0, 0, [top](cairo_t* cr) { paintWood(cr, top); }), x, y);
          else if (landing || by < kTowerInside.y0)
            r.draw(baked(mArt, r, top ? "decktop" : "deck", 64, 64, 0, 0, [top](cairo_t* cr) { paintIronDeck(cr, top); }), x, y);
          else
          {
            const bool band = (by >= 14 && by <= 17) || (by >= 26 && by <= 29);
            r.draw(baked(mArt, r, band ? "masonry1" : "masonry0", 64, 64, 0, 0, [band](cairo_t* cr) { paintMasonry(cr, band); }), x,
              y);
          }
        }
      }
    // The gallery's railing along the roof.
    for (int bx = std::max(bx0, kTower.x0); bx <= std::min(bx1, kTower.x1); ++bx)
    {
      const float x = float(bx) * kTilePx - camX, y = float(kTower.y0) * kTilePx - 40.0f - camY;
      if (!offScreen(x, y, 64.0f, 40.0f) && !solid(bx, kTower.y0 - 1))
        r.draw(baked(mArt, r, "railing", 64, 40, 0, 0, paintRailing), x, y);
    }
    // The stair flights: a steel stringer under each, a handrail over it.
    for (const auto& g : s.metals)
    {
      if (!isFlight(g) || g.boxes.size() < 2)
        continue;
      const CellBox& a = g.boxes.front();
      const CellBox& b = g.boxes.back();
      const float ax = float(a.x) * kCellPx - camX, ay = float(a.y) * kCellPx + 18.0f - camY;
      const float bxp = float(b.x + b.w) * kCellPx - camX, byp = float(b.y) * kCellPx + 18.0f - camY;
      const bool left = b.x < a.x;
      const float sx0 = left ? float(a.x + a.w) * kCellPx - camX : ax, sx1 = left ? float(b.x) * kCellPx - camX : bxp;
      if (offScreen(std::min(sx0, sx1), std::min(ay, byp) - 120.0f, std::abs(sx1 - sx0), std::abs(byp - ay) + 160.0f))
        continue;
      r.drawLine(sx0, ay + 10.0f, sx1, byp + 10.0f, 14.0f, rgb(20, 18, 20));
      r.drawLine(sx0, ay + 10.0f, sx1, byp + 10.0f, 9.0f, rgb(96, 70, 54));
      r.drawLine(sx0, ay + 7.0f, sx1, byp + 7.0f, 2.0f, rgb(160, 130, 110));
      // The handrail on its posts.
      for (const auto& st : g.boxes)
      {
        const float px = float(st.x + (left ? st.w : 0)) * kCellPx - camX + (left ? -8.0f : 8.0f), py = float(st.y) * kCellPx - camY;
        r.drawLine(px, py, px, py - 64.0f, 3.0f, rgb(70, 60, 56));
      }
      const float hx0 = sx0 + (left ? -8.0f : 8.0f), hx1 = sx1 + (left ? 8.0f : -8.0f);
      r.drawLine(hx0, ay - 18.0f - 64.0f, hx1, byp - 18.0f - 64.0f, 4.0f, rgb(30, 26, 26));
      r.drawLine(hx0, ay - 18.0f - 65.0f, hx1, byp - 18.0f - 65.0f, 2.0f, rgb(150, 120, 100));
    }
    // The hatch: a rusted plate over the smugglers' way down, or its frame.
    {
      const float x = float(kHatch.x0) * kTilePx - camX, y = float(kHatch.y0) * kTilePx - camY;
      if (!offScreen(x, y, 128.0f, 128.0f))
      {
        if (solid(kHatch.x0, kHatch.y0))
        {
          r.fillRect(x + 6.0f, y - 2.0f, 116.0f, 10.0f, rgb(70, 60, 54));
          r.fillRect(x + 6.0f, y - 2.0f, 116.0f, 3.0f, rgb(150, 140, 130));
          r.fillRect(x + 14.0f, y + 1.0f, 18.0f, 4.0f, rgb(40, 34, 30));
          r.fillRect(x + 96.0f, y + 1.0f, 18.0f, 4.0f, rgb(40, 34, 30));
        }
        else
        {
          r.fillRect(x - 4.0f, y - 4.0f, 8.0f, 136.0f, rgb(60, 52, 48));
          r.fillRect(x + 124.0f, y - 4.0f, 8.0f, 136.0f, rgb(60, 52, 48));
        }
      }
    }
    // The keeper's desk on the upper landing.
    {
      const float x = 88.5f * kTilePx - camX, y = 26.0f * kTilePx - camY;
      if (!offScreen(x - 80.0f, y - 120.0f, 160.0f, 120.0f))
      {
        r.draw(baked(mArt, r, "desk", 160, 120, 80, 120, paintDesk), x, y);
        drawGlow(r, mArt, x + 40.0f, y - 86.0f, 90.0f, rgb(255, 200, 120), 0.4f + 0.05f * std::sin(float(frame) * 0.4f));
      }
    }
  }
  // The lift: its rails up the shaft, the gantry, the cable and the cage.
  for (const auto& m : s.machines)
  {
    if (m.kind != MachineKind::Lift || m.platform < 0)
      continue;
    const Platform& pl = mPlatforms[std::size_t(m.platform)];
    int topY = pl.y, botY = pl.y;
    for (const auto& p : pl.path)
    {
      topY = std::min(topY, p.second);
      botY = std::max(botY, p.second);
    }
    const float lx = float(pl.x) * kCellPx - camX, w = float(pl.w) * kCellPx;
    const float ty = float(topY) * kCellPx - camY - 2.0f * kTilePx, by = float(botY) * kCellPx - camY + 20.0f;
    if (!offScreen(lx - 80.0f, ty, w + 160.0f, by - ty))
    {
      for (const float rx : {lx - 6.0f, lx + w})
      {
        r.fillRect(rx, ty, 6.0f, by - ty, rgb(40, 36, 36));
        r.fillRect(rx + 1.0f, ty, 2.0f, by - ty, rgb(130, 110, 96));
      }
      // The gantry over the top stop.
      r.fillRect(lx - 20.0f, ty - 12.0f, w + 40.0f, 14.0f, rgb(60, 54, 50));
      r.fillRect(lx - 20.0f, ty - 12.0f, w + 40.0f, 3.0f, rgb(160, 150, 140));
      drawGlow(r, mArt, lx + w * 0.5f, ty, 20.0f, rgb(150, 140, 130), 0.3f);
    }
    const float px = (float(pl.prevX) + (float(pl.x) - float(pl.prevX)) * alpha) * kCellPx - camX;
    const float py = (float(pl.prevY) + (float(pl.y) - float(pl.prevY)) * alpha) * kCellPx - camY;
    if (!offScreen(px - 20.0f, py - 160.0f, w + 40.0f, 200.0f))
    {
      r.drawLine(px + w * 0.5f, ty, px + w * 0.5f, py - 110.0f, 3.0f, rgb(30, 30, 34));
      DrawOpts o;
      o.scale = w / 192.0f;
      if (pl.dead)
        o.tint = rgb(170, 170, 176);
      r.draw(baked(mArt, r, "liftcar", 200, 150, 4, 120, paintLiftCar), px, py, o);
    }
  }
  // Machines: the winch house, the generator, the lamp.
  for (std::size_t i = 0; i < s.machines.size(); ++i)
  {
    const auto& m = s.machines[i];
    const float x = float(m.box.x) * kCellPx - camX, y = float(m.box.y) * kCellPx - camY;
    const float w = float(m.box.w) * kCellPx, h = float(m.box.h) * kCellPx;
    if (offScreen(x - 100.0f, y - 100.0f, w + 200.0f, h + 200.0f))
      continue;
    const bool hum = m.powered && (frame / 6) % 2 == 0;
    switch (m.kind)
    {
      case MachineKind::Lift:
      {
        DrawOpts o;
        o.scale = h / 192.0f;
        r.draw(baked(mArt, r, m.powered ? "winch1" : "winch0", 80, 210, 8, 18, [p = m.powered](cairo_t* cr) { paintWinch(cr, p); }),
          x + (w - 64.0f * o.scale) * 0.5f, y, o);
        if (m.powered)
          drawGlow(r, mArt, x + w * 0.5f, y + 136.0f * o.scale - 18.0f * o.scale, 30.0f, rgb(110, 255, 140), hum ? 0.5f : 0.3f);
        break;
      }
      case MachineKind::Generator:
      case MachineKind::Door:
      {
        DrawOpts o;
        o.scale = w / 128.0f;
        r.draw(baked(mArt, r, m.powered ? "gen1" : "gen0", 144, 136, 8, 8, [p = m.powered](cairo_t* cr) { paintGenerator(cr, p); }),
          x, y, o);
        if (m.powered)
        {
          drawGlow(r, mArt, x + w * 0.86f, y + h * 0.6f, 50.0f, rgb(255, 170, 90), hum ? 0.45f : 0.3f);
          // A spark now and then at the coil.
          if (hash2(frame / 3, int(i)) % 7u == 0)
            drawBolt(r, jagged({x + w * 0.86f, y + h * 0.45f}, {x + w * 0.98f, y + h * 0.3f}, 3, 5.0f, hash2(frame, 9)), 1.2f, 0.8f);
        }
        break;
      }
      case MachineKind::Lamp:
      {
        DrawOpts o;
        o.scale = w / 192.0f;
        if (m.powered)
          drawGlow(r, mArt, x + w * 0.5f, y + h * 0.42f, 200.0f, rgb(255, 220, 140), 0.5f + 0.06f * std::sin(float(frame) * 0.2f));
        r.draw(baked(mArt, r, m.powered ? "lamp1" : "lamp0", 208, 272, 8, 8, [p = m.powered](cairo_t* cr) { paintLamp(cr, p); }), x, y,
          o);
        if (m.powered)
          drawGlow(r, mArt, x + w * 0.5f, y + h * 0.42f, 70.0f, rgb(255, 250, 230), 0.8f);
        break;
      }
    }
  }
  // The Kite Drones' tethers, and a spark running down them.
  for (const auto& k : s.kites)
  {
    if (std::size_t(k.enemy) >= mEnemies.size() || !mEnemies[std::size_t(k.enemy)].alive)
      continue;
    const Enemy& e = mEnemies[std::size_t(k.enemy)];
    const float ax = float(k.ax) * kCellPx - camX, ay = float(k.ay) * kCellPx - camY;
    const float ex = e.drawX * kCellPx + float(e.w) * kCellPx * 0.5f - camX;
    const float ey = (e.drawY + 1.0f) * kCellPx - camY - float(e.h) * kCellPx * 0.14f;
    if (offScreen(std::min(ax, ex), std::min(ay, ey), std::abs(ex - ax), std::abs(ey - ay)))
      continue;
    // A slight sag in the line.
    const float sag = 18.0f;
    float lx = ax, ly = ay;
    for (int i = 1; i <= 10; ++i)
    {
      const float u = float(i) / 10.0f;
      const float x = ax + (ex - ax) * u, y = ay + (ey - ay) * u + std::sin(u * float(kPi)) * sag;
      r.drawLine(lx, ly, x, y, 3.0f, rgb(20, 20, 26));
      r.drawLine(lx, ly, x, y, 1.4f, rgb(150, 150, 160));
      if (k.spark > 0)
        r.drawLine(lx, ly, x, y, 6.0f, rgba(170, 210, 255, 90), Blend::Add);
      lx = x;
      ly = y;
    }
    if (k.spark > 0)
    {
      const float u = 1.0f - float(k.spark) / float(kSparkFrames);
      const float x = ex + (ax - ex) * u, y = ey + (ay - ey) * u + std::sin(u * float(kPi)) * sag;
      drawGlow(r, mArt, x, y, 40.0f, rgb(200, 230, 255), 0.9f);
      r.fillRect(x - 3.0f, y - 3.0f, 6.0f, 6.0f, rgb(255, 255, 255), Blend::Add);
      crackle(r, x - 8.0f, y - 8.0f, 16.0f, 16.0f, frame, 0.8f, hash2(k.enemy, 5));
    }
    // The anchor: a ring bolt in the rock.
    r.fillRect(ax - 10.0f, ay - 6.0f, 20.0f, 12.0f, rgb(60, 56, 54));
    r.fillRect(ax - 10.0f, ay - 6.0f, 20.0f, 3.0f, rgb(150, 140, 130));
    drawGlow(r, mArt, ax, ay, 14.0f, rgb(150, 150, 160), 0.25f);
  }
  // The fry gull on its post (gone once it flaps off).
  if (s.fryX >= 0)
  {
    const float x = (float(s.fryX) + 1.0f) * kCellPx - camX, foot = float(s.fryY + 1) * kCellPx - camY;
    int ground = (s.fryY + 1) / kCellsPerTile;
    while (ground < mLevel->height && !solid(int(s.fryX) / kCellsPerTile, ground))
      ++ground;
    const float gy = float(ground) * kTilePx - camY;
    if (!offScreen(x - 100.0f, foot - 200.0f, 200.0f, gy - foot + 200.0f))
    {
      for (float y = foot; y < gy; y += 64.0f)
        r.draw(baked(mArt, r, "pole", 40, 64, 20, 0, paintPole), x, y);
      r.fillRect(x - 12.0f, foot - 2.0f, 24.0f, 6.0f, rgb(80, 68, 56));
      if (s.fryFlown == 0)
      {
        static constexpr int kLoop[4] = {0, 1, 2, 3};
        const int f = kLoop[std::size_t((frame % 24) / 6)];
        r.draw(styledEnemySprite(mArt, r, mTheme, "fry_gull", 0, f, 3, 3).get(1), x, foot);
      }
      else if (s.fryFlown < 120)
      {
        const float u = float(s.fryFlown) / 120.0f;
        DrawOpts o;
        o.alpha = 1.0f - smooth((u - 0.7f) / 0.3f);
        r.draw(styledEnemySprite(mArt, r, mTheme, "fry_gull", 1, (frame / 4) % 2, 3, 3).get(1), x + u * 700.0f,
          foot - u * 420.0f - std::sin(u * 9.0f) * 10.0f, o);
      }
    }
  }
  // Buoy 42 on its post.
  for (const auto& pr : mProps)
  {
    if (pr.kind != PropKind::Deco42 || !relay)
      continue;
    const float cx = (float(pr.x) + float(pr.w) * 0.5f) * kCellPx - camX, cy = (float(pr.y) + float(pr.h) * 0.5f) * kCellPx - camY;
    if (offScreen(cx - 110.0f, cy - 120.0f, 220.0f, 320.0f))
      continue;
    DrawOpts o;
    o.angle = std::sin(float(frame) * 0.035f) * 5.0f;
    r.draw(baked(mArt, r, "buoy", 220, 320, 100, 120, paintBuoy), cx, cy, o);
  }
  // The keeper's log.
  for (const auto& tr : mSignal.treasures)
  {
    if (tr.taken || tr.art != "keepers_log")
      continue;
    const float x = float(tr.x) * kTilePx - camX, y = float(tr.y) * kTilePx - camY;
    if (offScreen(x, y, kTilePx, kTilePx))
      continue;
    const float bob = std::sin(float(frame) * 0.07f) * 4.0f;
    drawGlow(r, mArt, x + 32.0f, y + 32.0f + bob, 72.0f + 6.0f * std::sin(float(frame) * 0.1f), rgb(255, 210, 90), 0.5f);
    r.draw(baked(mArt, r, "log", 96, 96, 16, 16, paintKeepersLog), x, y + bob);
    const int ph = frame % 90;
    if (ph < 16)
    {
      const float k = std::sin(float(ph) / 16.0f * float(kPi));
      const float sx = x + 10.0f + float(hash2(frame / 90, tr.x) % 44u), sy = y + bob + 4.0f + float(hash2(tr.y, frame / 90) % 30u);
      r.drawLine(sx - 9.0f * k, sy, sx + 9.0f * k, sy, 2.0f, rgb(255, 250, 220), Blend::Add);
      r.drawLine(sx, sy - 9.0f * k, sx, sy + 9.0f * k, 2.0f, rgb(255, 250, 220), Blend::Add);
    }
  }
  // Stuck Tesla rods, their bulbs glowing (brighter when their strike is
  // coming), blinking before they fall out.
  for (std::size_t i = 0; i < s.rods.size(); ++i)
  {
    const Rod& rd = s.rods[i];
    const float cx = (float(rd.x) + 1.0f) * kCellPx - camX, cy = (float(rd.y) + 0.5f) * kCellPx - camY;
    if (offScreen(cx - 60.0f, cy - 60.0f, 120.0f, 120.0f))
      continue;
    float charge = 0.0f;
    for (const auto& z : s.zones)
      if (z.target.kind == TargetKind::Rod && z.target.index == int(i) && z.glowing())
        charge = std::max(charge, float(z.t - (z.period - z.glow)) / float(z.glow));
    if (rd.life < 60 && (frame / 4) % 2 == 0)
      continue;
    const float bulbX = cx - float(rd.dir) * 38.0f;
    drawGlow(r, mArt, bulbX, cy, 30.0f + 40.0f * charge, rgb(170, 210, 255), 0.45f + 0.5f * charge + 0.1f * std::sin(float(frame) * 0.3f));
    r.draw(bakedSprite(mArt, r, "rod", 104, 32, 52, 16, paintRod).get(rd.dir), cx, cy);
    if (charge > 0.0f)
      crackle(r, bulbX - 10.0f, cy - 10.0f, 20.0f, 20.0f, frame, charge, hash2(int(i), 11));
  }
  // Bungee: the rope's post on the lighthouse, and the storm rings.
  if (s.bungee)
  {
    const float px = s.postX * kCellPx - camX, py = s.postY * kCellPx - camY;
    r.draw(baked(mArt, r, "bpost", 48, 80, 24, 14, paintBungeePost), px, py);
    const Texture& ring = baked(mArt, r, "ring", 128, 128, 64, 64, paintRing);
    for (std::size_t i = 0; i < s.rings.size(); ++i)
    {
      if (s.ringTaken[i])
        continue;
      const float x = float(s.rings[i].first) * kCellPx - camX, y = float(s.rings[i].second) * kCellPx - camY;
      if (offScreen(x - 64.0f, y - 64.0f, 128.0f, 128.0f))
        continue;
      drawGlow(r, mArt, x, y, 70.0f, kBolt, 0.25f);
      DrawOpts o;
      o.blend = Blend::Add;
      o.angle = float((frame * 3 + int(i) * 40) % 360);
      o.scale = 0.62f + 0.04f * std::sin(float(frame) * 0.15f + float(i));
      o.alpha = 0.85f;
      r.draw(ring, x, y, o);
    }
  }
}

// --- In front --------------------------------------------------------------------------------------------

void World::drawStormFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)alpha;
  const auto& s = mStorm;
  const auto& p = mPlayer;
  // Kite Drones.
  for (const auto& k : s.kites)
  {
    if (std::size_t(k.enemy) >= mEnemies.size())
      continue;
    const Enemy& e = mEnemies[std::size_t(k.enemy)];
    if (!e.alive || e.hidden)
      continue;
    const float x = e.drawX * kCellPx + float(e.w) * kCellPx * 0.5f - camX, y = (e.drawY + 1.0f) * kCellPx - camY;
    if (offScreen(x - 100.0f, y - 160.0f, 200.0f, 200.0f))
      continue;
    const int v = k.spark > 0 ? 2 : (e.tell > 0 || k.state == 1 || k.state == 2 ? 1 : 0);
    const int dir = p.x + 1 >= e.x + e.w / 2 ? 1 : -1;
    DrawOpts o;
    o.angle = float(e.x - e.prevX) * 8.0f + std::sin(float(frame) * 0.07f + float(k.enemy)) * 4.0f;
    if (v == 1)
      o.angle += ((frame / 2) % 2 ? 2.0f : -2.0f);
    const float bob = std::sin(float(frame) * 0.09f + float(k.enemy)) * 3.0f;
    const Sprite& sp = styledEnemySprite(mArt, r, mTheme, "kite_drone", v, (frame / 2) % 2, e.w, e.h);
    if (v == 1)
      drawGlow(r, mArt, x, y - float(e.h) * kCellPx * 0.55f, 70.0f, rgb(255, 80, 60), 0.25f);
    r.draw(sp.get(dir), x, y + bob, o);
    if (e.flash > 0 && (frame / 2) % 2 == 0)
    {
      DrawOpts w = o;
      w.blend = Blend::Add;
      w.alpha = 0.55f;
      r.draw(sp.get(dir), x, y + bob, w);
    }
    if (v == 2)
    {
      // Sparks off the struck drone.
      crackle(r, x - 60.0f, y - 120.0f + bob, 120.0f, 110.0f, frame, 0.9f, hash2(k.enemy, 17));
      for (int i = 0; i < 6; ++i)
      {
        const std::uint32_t h = hash2(frame + i * 13, k.enemy);
        r.fillRect(x - 60.0f + unit(h) * 120.0f, y - 110.0f + unit(hash2(int(h), 2)) * 100.0f, 3.0f, 3.0f, rgb(255, 240, 180), Blend::Add);
      }
    }
  }
  // Storm Gulls: wheeling, crying before the dive, diving along it.
  for (const auto& e : mEnemies)
  {
    if (!e.alive || e.hidden || e.kind != EnemyKind::StormGull)
      continue;
    const float w = float(e.w) * kCellPx, h = float(e.h) * kCellPx;
    const float x = e.drawX * kCellPx + w * 0.5f - camX, y = (e.drawY + 1.0f) * kCellPx - camY;
    if (offScreen(x - 120.0f, y - 140.0f, 240.0f, 200.0f))
      continue;
    int v = 0, f = (frame / 6) % 4;
    float ang = 0.0f;
    const int dir = e.dir < 0 ? -1 : 1;
    switch (e.dive)
    {
      case 1:
        v = 1;
        f = (frame / 3) % 2;
        break;
      case 2:
      {
        v = 2;
        const float dx = std::abs(float(e.aimX - e.ox)) + 0.01f, dy = float(e.aimY - e.oy);
        ang = std::atan2(dy, dx) * 57.2958f;
        break;
      }
      case 3:
        f = (frame / 3) % 4;
        ang = -25.0f;
        break;
      case 4:
        f = (frame / 3) % 4;
        ang = -35.0f;
        break;
      default:
        ang = float(e.y - e.prevY) * 6.0f;
        break;
    }
    if (dir < 0)
      ang = -ang;
    // Turn it round its middle, not its feet.
    const float rad = ang / 57.2958f;
    const float dxp = -h * 0.5f * std::sin(rad), dyp = -h * 0.5f + h * 0.5f * std::cos(rad);
    DrawOpts o;
    o.angle = ang;
    const Sprite& sp = styledEnemySprite(mArt, r, mTheme, "storm_gull", v, f, e.w, e.h);
    if (v == 1)
    {
      // The cry: rings of it off the beak.
      const float bx = x + float(dir) * w * 0.48f, by = y - h * 0.72f;
      for (int k = 0; k < 2; ++k)
      {
        const float rr = 14.0f + float((frame + k * 6) % 12) * 3.0f;
        const float a = 1.0f - float((frame + k * 6) % 12) / 12.0f;
        r.drawLine(bx + float(dir) * rr, by - rr * 0.6f, bx + float(dir) * rr * 1.1f, by + rr * 0.6f, 2.0f,
          rgba(255, 240, 220, int(200.0f * a)));
      }
    }
    r.draw(sp.get(dir), x + dxp, y + dyp, o);
    if (e.flash > 0 && (frame / 2) % 2 == 0)
    {
      DrawOpts wo = o;
      wo.blend = Blend::Add;
      wo.alpha = 0.6f;
      r.draw(sp.get(dir), x + dxp, y + dyp, wo);
    }
  }
  // Foghorns: the inhale on the tell, the blast's rippling air.
  for (const auto& e : mEnemies)
  {
    if (!e.alive || e.hidden || e.kind != EnemyKind::Foghorn)
      continue;
    const float w = float(e.w) * kCellPx;
    const float x = e.drawX * kCellPx + w * 0.5f - camX, y = (e.drawY + 1.0f) * kCellPx - camY;
    const int dir = e.dir < 0 ? -1 : 1;
    if (offScreen(x - 700.0f, y - 260.0f, 1400.0f, 300.0f))
      continue;
    const float sc = w / 192.0f;
    const float mx = x + float(dir) * kFoghornMouthX * sc, my = y + kFoghornMouthY * sc;
    const int v = (e.dive > 0 ? 3 : (e.tell > 0 ? (e.tell > 11 ? 1 : 2) : 0)) + (e.carrier ? 4 : 0);
    const Sprite& sp = styledEnemySprite(mArt, r, mTheme, "foghorn", v, (frame / 2) % 2, e.w, e.h);
    if (e.tell > 0)
    {
      // The inhale: streaks of air drawn into the bell.
      for (int k = 0; k < 7; ++k)
      {
        const float u = std::fmod(float(frame) * 0.06f + float(k) / 7.0f, 1.0f);
        const float d = (1.0f - u) * 200.0f;
        const float a = (float(k) - 3.0f) * 0.22f;
        const float sx = mx + float(dir) * std::cos(a) * d, sy = my + std::sin(a) * d;
        const float ex = mx + float(dir) * std::cos(a) * (d - 30.0f), ey = my + std::sin(a) * (d - 30.0f);
        r.drawLine(sx, sy, ex, ey, 2.0f, e.carrier ? rgba(150, 255, 140, int(120.0f * u)) : rgba(220, 230, 245, int(120.0f * u)));
      }
    }
    r.draw(sp.get(dir), x, y);
    if (e.flash > 0 && (frame / 2) % 2 == 0)
    {
      DrawOpts wo;
      wo.blend = Blend::Add;
      wo.alpha = 0.55f;
      r.draw(sp.get(dir), x, y, wo);
    }
    if (e.dive > 0)
    {
      // The blast: a band of shaken air the length of the cone, rings of
      // sound rolling out through it.
      const float len = float(kFogLength) * kCellPx, ch = float(kFogHeight) * kCellPx;
      const float coneMid = float(e.y - kFogHeight + 1) * kCellPx + ch * 0.5f - camY + (e.drawY - float(e.y)) * kCellPx;
      const float fade = std::min(1.0f, float(e.dive) / 3.0f);
      DrawOpts b;
      b.blend = Blend::Add;
      b.tint = e.carrier ? rgb(130, 255, 120) : rgb(200, 215, 235);
      b.alpha = 0.35f * fade;
      b.angle = dir < 0 ? 180.0f : 0.0f;
      b.scale = len / 512.0f;
      r.draw(baked(mArt, r, "blast", 512, 192, 0, 96, paintBlast), mx, coneMid, b);
      const float since = float(kFogPush - e.dive) + alpha;
      for (int k = 0; k < 6; ++k)
      {
        const float d = std::fmod(since * 40.0f + float(k) * 90.0f, len);
        const float a = (1.0f - d / len) * fade;
        const float hh = ch * (0.35f + 0.65f * d / len) * 0.5f;
        float px = mx + float(dir) * d, py = coneMid - hh;
        for (int j = 1; j <= 8; ++j)
        {
          const float u = float(j) / 8.0f;
          const float yy = coneMid - hh + 2.0f * hh * u;
          const float bulge = std::sin(u * float(kPi)) * (14.0f + d * 0.06f);
          const float wob = std::sin(float(frame) * 0.9f + u * 7.0f + float(k)) * 3.0f;
          const float xx = mx + float(dir) * (d + bulge + wob);
          r.drawLine(px, py, xx, yy, 3.0f, e.carrier ? rgba(150, 255, 140, int(170.0f * a)) : rgba(235, 240, 250, int(150.0f * a)),
            Blend::Add);
          px = xx;
          py = yy;
        }
      }
      if (e.carrier)
        for (int i = 0; i < 18; ++i)
        {
          // Green salt spray flung along it.
          const std::uint32_t hsh = hash2(i, int(e.id));
          const float d = std::fmod(since * (30.0f + float(hsh % 20u)) + float(hsh % 300u), len);
          const float yy = coneMid + (unit(hash2(int(hsh), 4)) - 0.5f) * ch * (0.4f + 0.6f * d / len);
          drawGlow(r, mArt, mx + float(dir) * d, yy, 8.0f, rgb(130, 255, 110), 0.7f * fade);
        }
    }
  }
  // The metal before a strike: a blue-white glow rising over the rust,
  // crackling harder as it nears; then the bolt and the live metal.
  for (std::size_t zi = 0; zi < s.zones.size(); ++zi)
  {
    const auto& z = s.zones[zi];
    if (z.glowing())
    {
      const float u = float(z.t - (z.period - z.glow)) / float(z.glow);
      const float flick = 0.75f + 0.25f * std::sin(float(frame) * 1.3f);
      for (const auto& b : struckMetal(z, z.target))
      {
        const float x = float(b.x) * kCellPx - camX, y = float(b.y) * kCellPx - camY;
        const float bw = float(b.w) * kCellPx, bh = float(b.h) * kCellPx;
        if (offScreen(x, y, bw, bh))
          continue;
        r.fillRect(x, y, bw, bh, rgba(120, 160, 255, int((30.0f + 90.0f * u) * flick)), Blend::Add);
        for (float gx = x + 32.0f; gx < x + bw; gx += 64.0f)
          drawGlow(r, mArt, gx, y + 4.0f, 46.0f + 30.0f * u, rgb(170, 205, 255), (0.25f + 0.5f * u) * flick);
        crackle(r, x, y, bw, bh, frame, u, hash2(b.x, b.y));
      }
    }
    if (z.hurt > 0)
    {
      // The bolt: from the cloud to the struck metal, branching.
      const float a = z.hurt == kStrikeHurt ? 1.0f : (z.hurt == 3 ? 0.55f : (z.hurt == 2 ? 0.85f : 0.4f));
      const float bx = (float(z.boltX)) * kCellPx - camX, by = float(z.boltY) * kCellPx - camY;
      const std::uint32_t seed = hash2(z.boltX * 31 + z.boltY, frame + z.hurt);
      // From the cloud, or from the top of its zone when that is in view (a
      // zone under an overhang: the bolt comes in under the rock).
      const float zoneTop = float(z.rect.y) * kCellPx - camY;
      const Pt top{bx + (unit(seed) - 0.5f) * 240.0f, std::min(by - 40.0f, std::max(-60.0f, zoneTop))};
      const auto main = jagged(top, {bx, by}, 16, 34.0f, seed);
      drawBolt(r, main, 3.6f, a);
      for (int k = 0; k < 3; ++k)
      {
        const std::size_t at = std::size_t(3 + int(hash2(int(seed), k) % 9u));
        if (at >= main.size())
          continue;
        const Pt from = main[at];
        const float side = unit(hash2(int(seed), k + 10)) < 0.5f ? -1.0f : 1.0f;
        const Pt to{from.x + side * (80.0f + 120.0f * unit(hash2(int(seed), k + 20))), from.y + 90.0f + 140.0f * unit(hash2(int(seed), k + 30))};
        drawBolt(r, jagged(from, to, 6, 18.0f, hash2(int(seed), k + 40)), 1.6f, a * 0.7f);
      }
      drawGlow(r, mArt, bx, by, 150.0f, rgb(200, 220, 255), 0.8f * a);
      // The metal alive with it.
      for (const auto& b : z.live)
      {
        const float x = float(b.x) * kCellPx - camX, y = float(b.y) * kCellPx - camY;
        const float bw = float(b.w) * kCellPx, bh = float(b.h) * kCellPx;
        if (offScreen(x, y, bw, bh))
          continue;
        r.fillRect(x, y, bw, bh, rgba(200, 220, 255, int(140.0f * a)), Blend::Add);
        crackle(r, x - 6.0f, y - 6.0f, bw + 12.0f, bh + 12.0f, frame, 1.0f, hash2(b.x + 5, b.y));
        for (int i = 0; i < 5; ++i)
        {
          const std::uint32_t h = hash2(frame * 5 + i, b.x + b.y);
          r.fillRect(x + unit(h) * bw, y - unit(hash2(int(h), 1)) * 30.0f, 3.0f, 3.0f, rgb(255, 255, 255), Blend::Add);
        }
      }
    }
  }
  // A struck rod's arcs to the enemies round it.
  for (const auto& a : s.arcs)
  {
    const Pt from{(float(a.x0)) * kCellPx - camX, (float(a.y0) + 0.5f) * kCellPx - camY};
    const Pt to{float(a.x1) * kCellPx - camX, float(a.y1) * kCellPx - camY};
    drawBolt(r, jagged(from, to, 8, 16.0f, hash2(a.x1 * 13 + a.y1, frame)), 2.0f, float(a.frames) / float(kArcFrames));
    drawGlow(r, mArt, to.x, to.y, 40.0f, rgb(200, 225, 255), 0.6f);
  }
  // Bungee: the elastic rope from the post to the runner, taut or slack.
  if (s.bungee)
  {
    const float x0 = s.postX * kCellPx - camX, y0 = s.postY * kCellPx - camY;
    const float rx = (float(p.prevX) + float(p.x - p.prevX) * alpha + 1.5f) * kCellPx - camX;
    const float ry = (float(p.prevY) + float(p.y - p.prevY) * alpha - 2.0f) * kCellPx - camY;
    const float dx = (rx - x0) / kCellPx, dy = (ry - y0) / kCellPx;
    const float dist = std::sqrt(dx * dx + dy * dy);
    const bool taut = dist > kBungeeRest;
    const float stretch = std::clamp((dist - kBungeeRest) / (kBungeeMax - kBungeeRest), 0.0f, 1.0f);
    const float sag = taut ? 0.0f : (kBungeeRest - dist) * kCellPx * 0.45f;
    const float hum = s.stretching ? std::sin(float(frame) * 1.7f) * 3.0f * stretch : 0.0f;
    const float wid = taut ? 6.0f - 2.5f * stretch : 6.5f;
    float lx = x0, ly = y0;
    for (int i = 1; i <= 16; ++i)
    {
      const float u = float(i) / 16.0f;
      const float x = x0 + (rx - x0) * u, y = y0 + (ry - y0) * u + std::sin(u * float(kPi)) * (sag + hum);
      r.drawLine(lx, ly, x, y, wid + 3.0f, rgb(20, 16, 20));
      r.drawLine(lx, ly, x, y, wid, i % 2 ? rgb(220, 60, 50) : rgb(240, 236, 228));
      lx = x;
      ly = y;
    }
    if (taut)
      r.drawLine(x0, y0, rx, ry, 1.5f, rgba(255, 255, 255, int(60.0f + 120.0f * stretch)));
    drawGlow(r, mArt, x0, y0, 22.0f, rgb(255, 220, 180), 0.3f);
  }
  // The rain, slanting in on the wind (not under rock or a roof).
  {
    auto sheltered = [&](float sx, float sy) {
      const int bx = int(std::floor((sx + camX) / kTilePx)), by = int(std::floor((sy + camY) / kTilePx));
      if (inside(s, bx, by))
        return true;
      for (int k = 0; k <= 8; ++k)
        if (by - k >= 0 && by - k < mLevel->height && bx >= 0 && bx < mLevel->width && mMap.block(bx, by - k) == Tile::Solid)
          return true;
      return false;
    };
    for (int i = 0; i < 130; ++i)
    {
      const std::uint32_t h = hash2(i, 1618);
      const float par = 0.8f + 0.4f * unit(hash2(int(h), 1));
      const float len = 22.0f + 22.0f * unit(hash2(int(h), 2));
      const float x = std::fmod(float(h % 1500u) - camX * par + float(frame) * 6.0f * par + 150000.0f, 1500.0f) - 110.0f;
      const float y = std::fmod(float((h >> 11) % 820u) + float(frame) * 24.0f * par - camY * par + 820000.0f, 820.0f) - 60.0f;
      if (s.bungee == false && sheltered(x, y + len))
        continue;
      r.drawLine(x, y, x + 6.0f * par, y + len, 1.4f * par, rgba(185, 205, 240, int(55.0f + 45.0f * unit(hash2(int(h), 3)))));
    }
  }
  // The lightning's flash over everything: the sheet lightning faintly, a
  // strike hard.
  {
    float sx = 0.0f, sy = 0.0f;
    bool bolt = false;
    float f = sheetFlash(frame, sx, sy, bolt) * 0.12f;
    for (const auto& z : s.zones)
      if (z.hurt > 0)
        f = std::max(f, z.hurt == kStrikeHurt ? 0.38f : (z.hurt == 2 ? 0.16f : 0.08f));
    if (f > 0.0f)
      r.fillRect(0.0f, 0.0f, float(kScreenW), float(kScreenH), rgba(170, 190, 240, int(255.0f * f)), Blend::Add);
  }
}

void World::drawStormHud(Renderer& r, int frame) const
{
  (void)r;
  (void)frame;
}

} // namespace gr
