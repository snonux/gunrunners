// Level 32, Dish Array: drawing. See world_dish.cpp for the logic.
//
// Behind the tiles (drawDishSet): the satellite, a moving star, crossing
// the deep blue sky past its three markers (east, overhead, west; the one
// the big dishes turn to next glows); the dishes' lattice pedestals, their
// bearings and hydraulic yokes; the white bowls hung from the rims (and the
// feed arm's boom and horn) that follow the platforms smoothly through each
// turn and shake on the groan before it, D4 with "42" on its back; the
// rims' catwalks; the struts, solid white trusses in their poses and faint
// ghosts out of them (blinking amber as they swing in); the receiver
// cabin's racks and its stilts; the hangar's and the bunker's back walls and
// the bunker's consoles and tracking screen; the uplink tower's mast;
// Heatstroke's fixed dishes. Behind the runner (drawDishBack): the red mesa,
// the survey truck, the bunker's concrete, the tower's cladding, D4's
// footing and struts, the cabin's corrugated shell, the sand sea with its
// ripples, the teaching dish's sign, the hangar door with the Halcyon target
// painted on it (or the open hangar and the target board), the Halcyon
// crate, the minidishes in their poses, the cactus and its hat, the
// headphones. In front (drawDishFront): Sand Sharks (a fin cutting the sand,
// the bulge, the breach and the bite), Scorpion Mechs (their aim lines
// turning, locking, the beam), Vulture Drones and their shadows, falling
// scrap and its shrinking shadow, the jammed robots' static and confused
// glyph, the Jammer's noise cone, the heat shimmer. The HUD (drawDishHud):
// Heatstroke's swap sign.

#include "game/world.hpp"

#include "assets/art.hpp"
#include "assets/art_dish.hpp"
#include "assets/enemy_art.hpp"
#include "base/math.hpp"
#include "game/dish_draw.hpp"
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
constexpr Color kInk = rgb(34, 24, 20);
constexpr Color kJam = rgb(255, 220, 120);
constexpr Color kSandC = rgb(236, 206, 156);
// The big dishes' angles in their three poses (radians, clockwise): east
// tips the bowl's axis to the right.
constexpr std::array<float, 3> kPoseAngle{0.42f, 0.0f, -0.42f};

bool offScreen(float x, float y, float w, float h, float m = 96.0f)
{
  return x > float(kScreenW) + m || x + w < -m || y > float(kScreenH) + m || y + h < -m;
}

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
  const std::string id = "~dish/" + key;
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

// A thick line with a dark outline, for booms and yokes drawn live.
void beamLine(Renderer& r, float x0, float y0, float x1, float y1, float w, Color c)
{
  r.drawLine(x0, y0, x1, y1, w + 4.0f, kInk);
  r.drawLine(x0, y0, x1, y1, w, c);
}

// --- Painters: the dishes --------------------------------------------------------------------------------

// A big dish seen from the side, its aperture level at the anchor (220,
// 150): the white back shell with its ribs and panel seams, the shaded
// inside showing past the rim, the feed's three legs up to the horn at the
// focus, the yoke's mounting plate on the back. With "42" on its back (D4).
void paintBowl(cairo_t* cr, bool n42)
{
  // The back shell.
  cairo_move_to(cr, 20, 150);
  cairo_curve_to(cr, 60, 266, 380, 266, 420, 150);
  cairo_close_path(cr);
  lgrad(cr, 60, 140, 380, 270, {{0.0, rgb(255, 255, 255)}, {0.45, rgb(226, 230, 238)}, {1.0, rgb(150, 156, 174)}}, 1.0, true);
  stroke(cr, kInk, 3.0);
  // Panel seams following the shell, and ribs down to the vertex.
  for (int i = 1; i <= 2; ++i)
  {
    cairo_move_to(cr, 20 + i * 44, 150);
    cairo_curve_to(cr, 60 + i * 34, 266 - i * 30, 380 - i * 34, 266 - i * 30, 420 - i * 44, 150);
    stroke(cr, rgb(130, 136, 154), 1.4, 0.45);
  }
  for (int k = -3; k <= 3; ++k)
  {
    cairo_move_to(cr, 220 + k * 6, 236);
    cairo_line_to(cr, 220 + k * 62, 154);
    stroke(cr, rgb(130, 136, 154), 1.2, 0.4);
  }
  // A sunlit edge along the shell's upper left.
  cairo_move_to(cr, 26, 156);
  cairo_curve_to(cr, 50, 210, 110, 246, 190, 252);
  stroke(cr, rgb(255, 255, 255), 2.0, 0.7);
  if (n42)
  {
    // D4's number, stencilled big on its back.
    lettering(cr, "42", 222, 238, 70, rgb(44, 44, 56), 0.9);
    cairo_move_to(cr, 168, 248);
    cairo_line_to(cr, 276, 248);
    stroke(cr, rgb(220, 60, 50), 4.0, 0.85);
  }
  // The yoke's mounting plate at the vertex.
  ellipse(cr, 220, 252, 28, 10);
  lgrad(cr, 0, 242, 0, 262, {{0.0, rgb(200, 204, 214)}, {1.0, rgb(110, 114, 128)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  for (const double x : {200.0, 220.0, 240.0})
  {
    circle(cr, x, 252, 2.0);
    setRgba(cr, rgb(60, 62, 72), 1.0);
    cairo_fill(cr);
  }
  // The inside, past the rim: shaded panels, brighter at the far side.
  ellipse(cr, 220, 150, 200, 17);
  lgrad(cr, 0, 133, 0, 167, {{0.0, rgb(236, 240, 248)}, {0.5, rgb(196, 204, 220)}, {1.0, rgb(160, 168, 188)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  for (int k = -4; k <= 4; ++k)
  {
    cairo_move_to(cr, 220 + k * 6, 152);
    cairo_line_to(cr, 220 + k * 44, 136 + std::abs(k) * 0.8);
    stroke(cr, rgb(150, 158, 178), 1.0, 0.5);
  }
  // The rim: a bright band.
  ellipse(cr, 220, 150, 200, 17);
  stroke(cr, rgb(255, 255, 255), 3.0, 0.9);
  // The feed: three legs up to the horn at the focus.
  for (const auto& leg : {std::array<double, 2>{42, 152}, std::array<double, 2>{398, 152}, std::array<double, 2>{220, 166}})
  {
    cairo_move_to(cr, leg[0], leg[1]);
    cairo_line_to(cr, 220, 58);
    stroke(cr, kInk, 6.0);
    cairo_move_to(cr, leg[0], leg[1]);
    cairo_line_to(cr, 220, 58);
    stroke(cr, rgb(236, 238, 244), 3.4);
  }
  // The horn: a white housing, the cone pointing down into the dish.
  cairo_rectangle(cr, 204, 26, 32, 30);
  lgrad(cr, 204, 0, 236, 0, {{0.0, rgb(255, 255, 255)}, {1.0, rgb(160, 166, 182)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  cairo_move_to(cr, 206, 56);
  cairo_line_to(cr, 234, 56);
  cairo_line_to(cr, 226, 72);
  cairo_line_to(cr, 214, 72);
  cairo_close_path(cr);
  lgrad(cr, 206, 0, 234, 0, {{0.0, rgb(240, 200, 110)}, {1.0, rgb(150, 100, 40)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
}

// One section of a pedestal: white steel lattice, 96 x 64, stacking.
void paintLattice(cairo_t* cr)
{
  for (const double x : {14.0, 82.0})
  {
    cairo_move_to(cr, x, 0);
    cairo_line_to(cr, x, 64);
    stroke(cr, kInk, 8.0);
  }
  cairo_move_to(cr, 14, 2);
  cairo_line_to(cr, 82, 62);
  cairo_move_to(cr, 82, 2);
  cairo_line_to(cr, 14, 62);
  stroke(cr, kInk, 5.0);
  cairo_move_to(cr, 14, 2);
  cairo_line_to(cr, 82, 62);
  cairo_move_to(cr, 82, 2);
  cairo_line_to(cr, 14, 62);
  stroke(cr, rgb(214, 218, 228), 2.6);
  cairo_move_to(cr, 14, 1);
  cairo_line_to(cr, 82, 1);
  stroke(cr, kInk, 5.0);
  cairo_move_to(cr, 14, 1);
  cairo_line_to(cr, 82, 1);
  stroke(cr, rgb(236, 238, 244), 2.6);
  for (const double x : {14.0, 82.0})
  {
    cairo_rectangle(cr, x - 3, -1, 6, 66);
    lgrad(cr, x - 3, 0, x + 3, 0, {{0.0, rgb(255, 255, 255)}, {1.0, rgb(160, 166, 182)}});
  }
}

// The bearing housing on top of a pedestal (120 x 90, anchored mid).
void paintHub(cairo_t* cr)
{
  cairo_rectangle(cr, 10, 22, 100, 58);
  lgrad(cr, 0, 22, 0, 80, {{0.0, rgb(250, 250, 252)}, {1.0, rgb(150, 156, 172)}}, 1.0, true);
  stroke(cr, kInk, 2.4);
  circle(cr, 60, 45, 30);
  lgrad(cr, 0, 15, 0, 75, {{0.0, rgb(230, 232, 240)}, {1.0, rgb(120, 126, 142)}}, 1.0, true);
  stroke(cr, kInk, 2.2);
  for (int k = 0; k < 10; ++k)
  {
    const double a = k * 2 * kPi / 10;
    circle(cr, 60 + std::cos(a) * 23, 45 + std::sin(a) * 23, 2.2);
    setRgba(cr, rgb(70, 72, 82), 1.0);
    cairo_fill(cr);
  }
  circle(cr, 60, 45, 11);
  setRgba(cr, rgb(70, 72, 84), 1.0);
  cairo_fill(cr);
  // The drive motor on its side, a warning stripe.
  cairo_rectangle(cr, 92, 54, 24, 22);
  lgrad(cr, 0, 54, 0, 76, {{0.0, rgb(210, 214, 222)}, {1.0, rgb(110, 114, 128)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  for (double x = 12; x < 108; x += 12)
  {
    cairo_move_to(cr, x, 80);
    cairo_line_to(cr, x + 6, 74);
    stroke(cr, rgb(240, 190, 40), 3.0, 0.9);
  }
}

// A concrete plinth at a pedestal's foot (160 x 40, anchored bottom mid).
void paintPlinth(cairo_t* cr)
{
  cairo_move_to(cr, 10, 40);
  cairo_line_to(cr, 26, 8);
  cairo_line_to(cr, 134, 8);
  cairo_line_to(cr, 150, 40);
  cairo_close_path(cr);
  lgrad(cr, 0, 8, 0, 40, {{0.0, rgb(214, 206, 192)}, {1.0, rgb(150, 140, 126)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  for (const double x : {44.0, 116.0})
  {
    circle(cr, x, 18, 3.0);
    setRgba(cr, rgb(90, 86, 80), 1.0);
    cairo_fill(cr);
  }
}

// A strut block: a white truss square with an X, 64 x 64.
void paintStrut(cairo_t* cr)
{
  cairo_rectangle(cr, 4, 4, 56, 56);
  setRgba(cr, rgb(190, 196, 210), 0.25);
  cairo_fill(cr);
  for (int pass = 0; pass < 2; ++pass)
  {
    const Color c = pass == 0 ? kInk : rgb(238, 240, 246);
    const double w = pass == 0 ? 6.0 : 3.2;
    cairo_rectangle(cr, 4, 4, 56, 56);
    cairo_move_to(cr, 6, 6);
    cairo_line_to(cr, 58, 58);
    cairo_move_to(cr, 58, 6);
    cairo_line_to(cr, 6, 58);
    stroke(cr, c, w);
  }
  for (const double x : {4.0, 60.0})
    for (const double y : {4.0, 60.0})
    {
      circle(cr, x, y, 3.0);
      setRgba(cr, rgb(120, 126, 140), 1.0);
      cairo_fill(cr);
    }
}

// The horn cabin at a feed arm's tip (120 x 110, anchored at the arm's top
// right corner (24, 30)): a white box hanging under the arm, the horn
// pointing down and back toward its dish.
void paintHornCabin(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 30, 70, 54);
  lgrad(cr, 0, 30, 70, 84, {{0.0, rgb(255, 255, 255)}, {1.0, rgb(150, 156, 172)}}, 1.0, true);
  stroke(cr, kInk, 2.4);
  cairo_rectangle(cr, 10, 40, 22, 10);
  setRgba(cr, rgb(60, 80, 110), 1.0);
  cairo_fill(cr);
  cairo_move_to(cr, 8, 84);
  cairo_line_to(cr, 46, 84);
  cairo_line_to(cr, 14, 106);
  cairo_line_to(cr, 2, 98);
  cairo_close_path(cr);
  lgrad(cr, 0, 84, 46, 106, {{0.0, rgb(240, 200, 110)}, {1.0, rgb(140, 92, 36)}}, 1.0, true);
  stroke(cr, kInk, 1.8);
  for (double x = 4; x < 68; x += 12)
  {
    cairo_move_to(cr, x, 80);
    cairo_line_to(cr, x + 6, 74);
    stroke(cr, rgb(240, 190, 40), 3.0, 0.9);
  }
}

// The teaching dish's sign (120 x 70, anchored bottom mid): its name and
// the three poses it shows.
void paintPlacard(cairo_t* cr)
{
  for (const double x : {30.0, 90.0})
  {
    cairo_move_to(cr, x, 40);
    cairo_line_to(cr, x, 70);
    stroke(cr, rgb(80, 70, 60), 4.0);
  }
  cairo_rectangle(cr, 4, 2, 112, 42);
  lgrad(cr, 0, 2, 0, 44, {{0.0, rgb(255, 255, 255)}, {1.0, rgb(214, 218, 228)}}, 1.0, true);
  stroke(cr, kInk, 2.2);
  lettering(cr, "DISH 0", 60, 19, 13, rgb(40, 50, 90));
  lettering(cr, "E   O   W", 60, 38, 12, rgb(200, 60, 40));
}

// --- Painters: rock and buildings ------------------------------------------------------------------------

// The mesa's red sandstone, 64 x 64, its beds banded.
void paintRedRock(cairo_t* cr, int v)
{
  cairo_rectangle(cr, 0, 0, 64, 64);
  lgrad(cr, 0, 0, 0, 64, {{0.0, rgb(200, 102, 64)}, {1.0, rgb(166, 76, 50)}});
  constexpr std::array<double, 4> kBeds{6, 22, 37, 52};
  for (std::size_t i = 0; i < kBeds.size(); ++i)
    for (const double oy : {-64.0, 0.0, 64.0})
    {
      const double y = kBeds[i] + oy;
      auto at = [&](double x) { return y + 1.4 * std::sin(x / 64.0 * 2 * kPi + double(i) * 1.7); };
      cairo_move_to(cr, 0, at(0));
      for (double x = 4; x <= 64; x += 4)
        cairo_line_to(cr, x, at(x));
      cairo_line_to(cr, 64, at(64) + 7);
      for (double x = 64; x >= 0; x -= 4)
        cairo_line_to(cr, x, at(x) + 7);
      cairo_close_path(cr);
      setRgba(cr, i % 2 ? rgb(226, 132, 84) : rgb(150, 64, 42), 0.35);
      cairo_fill(cr);
      cairo_move_to(cr, 0, at(0));
      for (double x = 4; x <= 64; x += 4)
        cairo_line_to(cr, x, at(x));
      stroke(cr, rgb(100, 40, 26), 1.2, 0.4);
    }
  Rng rng(32211u + std::uint32_t(v));
  for (int i = 0; i < 70; ++i)
  {
    circle(cr, rng.range(0, 64), rng.range(0, 64), rng.range(0.4f, 0.9f));
    setRgba(cr, i % 3 ? rgb(90, 36, 22) : rgb(255, 210, 170), rng.range(0.12f, 0.3f));
    cairo_fill(cr);
  }
  if (v == 1)
  {
    // A crack with desert varnish streaked down from it.
    cairo_move_to(cr, 40, 0);
    cairo_curve_to(cr, 36, 20, 44, 30, 38, 64);
    stroke(cr, rgb(70, 30, 22), 5.0, 0.3);
    cairo_move_to(cr, 40, 0);
    cairo_curve_to(cr, 37, 14, 42, 22, 39, 34);
    stroke(cr, rgb(60, 22, 16), 1.4, 0.7);
  }
  else if (v == 2)
  {
    // A pale lens of bleached rock, tapering out at both ends.
    cairo_move_to(cr, 10, 31);
    cairo_curve_to(cr, 24, 26, 40, 26, 54, 31);
    cairo_curve_to(cr, 40, 35, 24, 35, 10, 31);
    setRgba(cr, rgb(236, 170, 124), 0.45);
    cairo_fill(cr);
  }
}

// The mesa's lip where you stand: hard cap rock, a sunlit edge and a dusting
// of red sand (64 x 24, anchored 6 px down).
void paintRedLip(cairo_t* cr)
{
  cairo_move_to(cr, 0, 5);
  for (double x = 4; x <= 64; x += 4)
    cairo_line_to(cr, x, 5 + 0.8 * std::sin(x / 64.0 * 4 * kPi));
  cairo_line_to(cr, 64, 18);
  for (double x = 64; x >= 0; x -= 4)
    cairo_line_to(cr, x, 16 + 1.5 * std::sin(x / 64.0 * 2 * kPi + 1.0));
  cairo_close_path(cr);
  lgrad(cr, 0, 4, 0, 18, {{0.0, rgb(240, 160, 110)}, {0.4, rgb(196, 98, 62)}, {1.0, rgb(140, 60, 40)}});
  cairo_move_to(cr, 0, 5);
  for (double x = 4; x <= 64; x += 4)
    cairo_line_to(cr, x, 5 + 0.8 * std::sin(x / 64.0 * 4 * kPi));
  stroke(cr, rgb(255, 222, 190), 1.6, 0.8);
  for (const double x : {12.0, 40.0, 54.0})
  {
    ellipse(cr, x, 8, 2.0, 1.2);
    setRgba(cr, rgb(120, 60, 40), 0.8);
    cairo_fill(cr);
  }
}

// The survey truck, 360 x 170, anchored so its roof's left end sits at the
// block's top-left (20, 30): a white box truck, its cab to the right, an
// orange stripe, GEO SURVEY on its side, a mast on the cab.
void paintTruck(cairo_t* cr)
{
  // Under it: the shade (the ground in its shadow) and the chassis.
  cairo_rectangle(cr, 14, 120, 336, 38);
  lgrad(cr, 0, 120, 0, 158, {{0.0, rgb(52, 40, 34)}, {1.0, rgb(110, 80, 56)}}, 1.0);
  cairo_rectangle(cr, 30, 128, 304, 14);
  setRgba(cr, rgb(40, 36, 36), 1.0);
  cairo_fill(cr);
  // The box.
  cairo_rectangle(cr, 20, 30, 236, 102);
  lgrad(cr, 0, 30, 0, 132, {{0.0, rgb(255, 255, 255)}, {0.7, rgb(226, 228, 234)}, {1.0, rgb(170, 172, 182)}}, 1.0, true);
  stroke(cr, kInk, 3.0);
  for (double x = 60; x < 256; x += 40)
  {
    cairo_move_to(cr, x, 32);
    cairo_line_to(cr, x, 130);
    stroke(cr, rgb(170, 174, 186), 1.2, 0.6);
  }
  cairo_rectangle(cr, 20, 92, 236, 10);
  setRgba(cr, rgb(240, 120, 40), 1.0);
  cairo_fill(cr);
  lettering(cr, "GEO SURVEY", 138, 80, 22, rgb(40, 50, 90));
  // Dust up its sides.
  {
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 104, 0, 132);
    stop(g, 0.0, rgb(214, 170, 120), 0.0);
    stop(g, 1.0, rgb(200, 150, 100), 0.7);
    cairo_rectangle(cr, 22, 104, 232, 27);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  // The cab: its roof level with the box's, a sloping windscreen.
  cairo_move_to(cr, 258, 30);
  cairo_line_to(cr, 314, 30);
  cairo_line_to(cr, 340, 80);
  cairo_line_to(cr, 340, 132);
  cairo_line_to(cr, 258, 132);
  cairo_close_path(cr);
  lgrad(cr, 0, 30, 0, 132, {{0.0, rgb(255, 255, 255)}, {1.0, rgb(184, 186, 196)}}, 1.0, true);
  stroke(cr, kInk, 3.0);
  cairo_move_to(cr, 280, 38);
  cairo_line_to(cr, 310, 38);
  cairo_line_to(cr, 330, 76);
  cairo_line_to(cr, 280, 76);
  cairo_close_path(cr);
  lgrad(cr, 280, 38, 330, 76, {{0.0, rgb(150, 190, 230)}, {1.0, rgb(40, 60, 90)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  cairo_move_to(cr, 288, 44);
  cairo_line_to(cr, 300, 66);
  stroke(cr, rgb(255, 255, 255), 2.0, 0.6);
  cairo_rectangle(cr, 258, 92, 82, 10);
  setRgba(cr, rgb(240, 120, 40), 1.0);
  cairo_fill(cr);
  cairo_rectangle(cr, 330, 104, 12, 8);
  setRgba(cr, rgb(255, 230, 150), 1.0);
  cairo_fill(cr);
  // The bumper.
  cairo_rectangle(cr, 330, 118, 18, 12);
  setRgba(cr, rgb(70, 72, 80), 1.0);
  cairo_fill(cr);
  // Wheels.
  for (const double x : {76.0, 140.0, 300.0})
  {
    circle(cr, x, 136, 23);
    setRgba(cr, rgb(30, 28, 30), 1.0);
    cairo_fill(cr);
    circle(cr, x, 136, 11);
    lgrad(cr, 0, 125, 0, 147, {{0.0, rgb(220, 220, 226)}, {1.0, rgb(120, 120, 130)}});
    circle(cr, x, 136, 4);
    setRgba(cr, rgb(60, 60, 66), 1.0);
    cairo_fill(cr);
    // Dust on the tyre.
    cairo_arc(cr, x, 136, 21, kPi * 0.1, kPi * 0.9);
    stroke(cr, rgb(200, 160, 110), 3.0, 0.5);
  }
  // A whip antenna on the cab.
  cairo_move_to(cr, 300, 30);
  cairo_line_to(cr, 308, 0);
  stroke(cr, rgb(60, 60, 66), 2.0);
}

// Concrete: grey-beige, formwork seams and tie holes; the roof variant has a
// darker parapet band (64 x 64).
void paintConcrete(cairo_t* cr, bool roof)
{
  cairo_rectangle(cr, 0, 0, 64, 64);
  lgrad(cr, 0, 0, 0, 64, {{0.0, rgb(206, 196, 180)}, {1.0, rgb(176, 166, 150)}});
  cairo_move_to(cr, 0, 32);
  cairo_line_to(cr, 64, 32);
  cairo_move_to(cr, 32, 0);
  cairo_line_to(cr, 32, 32);
  stroke(cr, rgb(130, 122, 110), 1.0, 0.6);
  for (const double x : {12.0, 52.0})
    for (const double y : {16.0, 48.0})
    {
      circle(cr, x, y, 1.8);
      setRgba(cr, rgb(110, 100, 90), 0.9);
      cairo_fill(cr);
    }
  Rng rng(32231u);
  for (int i = 0; i < 40; ++i)
  {
    circle(cr, rng.range(0, 64), rng.range(0, 64), rng.range(0.4f, 1.0f));
    setRgba(cr, i % 3 ? rgb(120, 110, 100) : rgb(240, 236, 228), rng.range(0.15f, 0.35f));
    cairo_fill(cr);
  }
  if (roof)
  {
    cairo_rectangle(cr, 0, 0, 64, 10);
    lgrad(cr, 0, 0, 0, 10, {{0.0, rgb(236, 230, 218)}, {1.0, rgb(150, 140, 126)}});
    // Sand drifted against the parapet.
    ellipse(cr, 20, 1, 16, 2.0);
    setRgba(cr, kSandC, 0.8);
    cairo_fill(cr);
  }
}

// The back walls inside (64 x 64): dark concrete, a pipe along it.
void paintInsideWall(cairo_t* cr, bool pipe)
{
  cairo_rectangle(cr, 0, 0, 64, 64);
  lgrad(cr, 0, 0, 0, 64, {{0.0, rgb(92, 86, 80)}, {1.0, rgb(64, 60, 56)}});
  cairo_move_to(cr, 0, 0);
  cairo_line_to(cr, 64, 0);
  cairo_move_to(cr, 0, 0);
  cairo_line_to(cr, 0, 64);
  stroke(cr, rgb(40, 36, 34), 1.6, 0.7);
  if (pipe)
  {
    cairo_rectangle(cr, 0, 12, 64, 8);
    lgrad(cr, 0, 12, 0, 20, {{0.0, rgb(170, 170, 176)}, {1.0, rgb(70, 70, 78)}});
  }
}

// A bank of consoles (300 x 110, anchored at its bottom-left): screens with
// green traces, knobs, a keyboard shelf.
void paintConsole(cairo_t* cr)
{
  cairo_move_to(cr, 0, 110);
  cairo_line_to(cr, 0, 40);
  cairo_line_to(cr, 20, 10);
  cairo_line_to(cr, 300, 10);
  cairo_line_to(cr, 300, 110);
  cairo_close_path(cr);
  lgrad(cr, 0, 10, 0, 110, {{0.0, rgb(200, 198, 190)}, {1.0, rgb(120, 116, 110)}}, 1.0, true);
  stroke(cr, kInk, 2.4);
  for (int k = 0; k < 4; ++k)
  {
    const double x = 30 + k * 66;
    cairo_rectangle(cr, x, 18, 52, 34);
    setRgba(cr, rgb(16, 30, 22), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.6);
    cairo_move_to(cr, x + 4, 36);
    for (double u = 0; u <= 44; u += 4)
      cairo_line_to(cr, x + 4 + u, 36 - 9 * std::sin(u * 0.3 + k) * std::exp(-u * 0.02));
    stroke(cr, rgb(110, 255, 150), 1.4, 0.9);
  }
  cairo_rectangle(cr, 12, 60, 280, 14);
  setRgba(cr, rgb(70, 68, 66), 1.0);
  cairo_fill(cr);
  for (int k = 0; k < 18; ++k)
  {
    cairo_rectangle(cr, 18 + k * 15, 62, 11, 8);
    setRgba(cr, rgb(220, 216, 206), 1.0);
    cairo_fill(cr);
  }
  for (int k = 0; k < 8; ++k)
  {
    circle(cr, 30 + k * 34, 90, 5);
    setRgba(cr, rgb(50, 50, 56), 1.0);
    cairo_fill(cr);
  }
}

// The tracking screen's bezel (260 x 150, anchored top-left); the arc and
// the satellite on it are drawn live.
void paintTrackScreen(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 0, 260, 150);
  lgrad(cr, 0, 0, 0, 150, {{0.0, rgb(190, 188, 180)}, {1.0, rgb(110, 106, 100)}}, 1.0, true);
  stroke(cr, kInk, 2.4);
  cairo_rectangle(cr, 12, 12, 236, 112);
  setRgba(cr, rgb(10, 22, 30), 1.0);
  cairo_fill(cr);
  // The arc of the sky with its three marks.
  cairo_arc(cr, 130, 120, 96, kPi, 2 * kPi);
  stroke(cr, rgb(80, 200, 255), 1.4, 0.6);
  for (int k = 0; k < 3; ++k)
  {
    const double a = kPi + (k + 1) * kPi / 4;
    cairo_move_to(cr, 130 + std::cos(a) * 88, 120 + std::sin(a) * 88);
    cairo_line_to(cr, 130 + std::cos(a) * 104, 120 + std::sin(a) * 104);
    stroke(cr, rgb(80, 200, 255), 2.0, 0.8);
  }
  lettering(cr, "TRACKING", 130, 142, 11, rgb(40, 40, 46));
}

// The uplink tower's cladding (64 x 64): white panels, a seam, small dark
// windows (v 1) or the red aviation band (v 2).
void paintTowerBlock(cairo_t* cr, int v)
{
  cairo_rectangle(cr, 0, 0, 64, 64);
  lgrad(cr, 0, 0, 64, 0, {{0.0, rgb(246, 246, 248)}, {1.0, rgb(206, 208, 216)}});
  cairo_move_to(cr, 0, 63);
  cairo_line_to(cr, 64, 63);
  cairo_move_to(cr, 63, 0);
  cairo_line_to(cr, 63, 64);
  stroke(cr, rgb(150, 154, 166), 1.4, 0.8);
  if (v == 1)
  {
    cairo_rectangle(cr, 22, 20, 20, 24);
    lgrad(cr, 22, 20, 42, 44, {{0.0, rgb(110, 150, 190)}, {1.0, rgb(30, 40, 60)}}, 1.0, true);
    stroke(cr, kInk, 1.6);
  }
  else if (v == 2)
  {
    cairo_rectangle(cr, 0, 16, 64, 30);
    setRgba(cr, rgb(214, 60, 46), 1.0);
    cairo_fill(cr);
  }
  // A streak of dust down from the seam.
  cairo_move_to(cr, 10, 0);
  cairo_line_to(cr, 12, 30);
  stroke(cr, rgb(200, 170, 130), 3.0, 0.2);
}

// The uplink mast on the tower (120 x 460, anchored at its foot): a
// lattice mast, horn antennas, a small dish.
void paintMast(cairo_t* cr)
{
  for (int pass = 0; pass < 2; ++pass)
  {
    const Color c = pass == 0 ? kInk : rgb(236, 238, 244);
    const double w = pass == 0 ? 6.0 : 3.0;
    cairo_move_to(cr, 40, 460);
    cairo_line_to(cr, 54, 20);
    cairo_move_to(cr, 80, 460);
    cairo_line_to(cr, 66, 20);
    for (double y = 440; y > 40; y -= 40)
    {
      const double u = (460 - y) / 440, l = 40 + 14 * u, rr = 80 - 14 * u;
      const double u2 = (460 - (y - 40)) / 440, l2 = 40 + 14 * u2, r2 = 80 - 14 * u2;
      cairo_move_to(cr, l, y);
      cairo_line_to(cr, r2, y - 40);
      cairo_move_to(cr, rr, y);
      cairo_line_to(cr, l2, y - 40);
    }
    stroke(cr, c, w);
  }
  // Horn antennas and a small dish.
  for (const auto& h : {std::array<double, 3>{50, 120, -1}, std::array<double, 3>{70, 180, 1}, std::array<double, 3>{50, 260, -1}})
  {
    const double x = h[0], y = h[1], d = h[2];
    cairo_move_to(cr, x, y);
    cairo_line_to(cr, x + d * 36, y - 12);
    cairo_line_to(cr, x + d * 36, y + 16);
    cairo_line_to(cr, x, y + 6);
    cairo_close_path(cr);
    lgrad(cr, x, 0, x + d * 36, 0, {{0.0, rgb(250, 250, 252)}, {1.0, rgb(170, 176, 190)}}, 1.0, true);
    stroke(cr, kInk, 2.0);
  }
  cairo_save(cr);
  cairo_translate(cr, 60, 340);
  cairo_rotate(cr, 0.5);
  cairo_move_to(cr, -26, 0);
  cairo_curve_to(cr, -18, 16, 18, 16, 26, 0);
  cairo_close_path(cr);
  lgrad(cr, 0, 0, 0, 14, {{0.0, rgb(255, 255, 255)}, {1.0, rgb(160, 166, 182)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  cairo_restore(cr);
  cairo_rectangle(cr, 54, 6, 12, 16);
  setRgba(cr, rgb(70, 70, 80), 1.0);
  cairo_fill(cr);
}

// The receiver cabin's shell: corrugated white sheet (64 x 64).
void paintCabinShell(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 0, 64, 64);
  lgrad(cr, 0, 0, 0, 64, {{0.0, rgb(244, 244, 246)}, {1.0, rgb(200, 202, 210)}});
  for (double x = 4; x < 64; x += 8)
  {
    cairo_rectangle(cr, x, 0, 3, 64);
    setRgba(cr, rgb(160, 164, 176), 0.6);
    cairo_fill(cr);
    cairo_move_to(cr, x + 4, 0);
    cairo_line_to(cr, x + 4, 64);
    stroke(cr, rgb(255, 255, 255), 1.0, 0.8);
  }
  cairo_move_to(cr, 0, 63);
  cairo_line_to(cr, 64, 63);
  stroke(cr, rgb(120, 110, 100), 1.6, 0.6);
}

// Inside the cabin (64 x 64): pale green insulation panels.
void paintCabinInside(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 0, 64, 64);
  lgrad(cr, 0, 0, 0, 64, {{0.0, rgb(150, 170, 150)}, {1.0, rgb(110, 128, 112)}});
  cairo_rectangle(cr, 2, 2, 60, 60);
  stroke(cr, rgb(80, 96, 84), 1.4, 0.7);
  for (const double x : {8.0, 56.0})
    for (const double y : {8.0, 56.0})
    {
      circle(cr, x, y, 1.6);
      setRgba(cr, rgb(70, 80, 72), 1.0);
      cairo_fill(cr);
    }
}

// A receiver rack (110 x 150, anchored at its bottom-left): meters, dials, a
// reel-to-reel, rows of lights (lit live).
void paintRack(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 0, 110, 150);
  lgrad(cr, 0, 0, 110, 0, {{0.0, rgb(120, 120, 128)}, {1.0, rgb(60, 60, 68)}}, 1.0, true);
  stroke(cr, kInk, 2.4);
  for (const double y : {8.0, 52.0, 100.0})
  {
    cairo_rectangle(cr, 6, y, 98, 40);
    setRgba(cr, rgb(196, 192, 180), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.2);
  }
  // Two VU meters.
  for (const double x : {12.0, 58.0})
  {
    cairo_rectangle(cr, x, 12, 40, 30);
    setRgba(cr, rgb(250, 236, 190), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.0);
    cairo_arc(cr, x + 20, 40, 22, kPi * 1.2, kPi * 1.8);
    stroke(cr, rgb(60, 50, 40), 1.0);
  }
  // Reels.
  for (const double x : {30.0, 80.0})
  {
    circle(cr, x, 72, 15);
    setRgba(cr, rgb(40, 40, 46), 1.0);
    cairo_fill(cr);
    circle(cr, x, 72, 4);
    setRgba(cr, rgb(200, 200, 210), 1.0);
    cairo_fill(cr);
  }
  for (int k = 0; k < 6; ++k)
  {
    circle(cr, 18 + k * 15, 124, 4.5);
    setRgba(cr, rgb(40, 40, 46), 1.0);
    cairo_fill(cr);
  }
}

// The headphones on their hook (110 x 90, anchored at the hook).
void paintHeadphones(cairo_t* cr)
{
  cairo_move_to(cr, 55, 0);
  cairo_line_to(cr, 55, 10);
  stroke(cr, rgb(60, 60, 66), 3.0);
  cairo_arc(cr, 55, 46, 34, kPi * 1.05, kPi * 1.95);
  stroke(cr, kInk, 9.0);
  cairo_arc(cr, 55, 46, 34, kPi * 1.05, kPi * 1.95);
  stroke(cr, rgb(70, 70, 84), 5.0);
  for (const double x : {20.0, 90.0})
  {
    ellipse(cr, x, 58, 14, 20);
    lgrad(cr, 0, 38, 0, 78, {{0.0, rgb(230, 80, 60)}, {1.0, rgb(120, 30, 24)}}, 1.0, true);
    stroke(cr, kInk, 2.2);
    ellipse(cr, x, 58, 7, 12);
    setRgba(cr, rgb(40, 30, 30), 1.0);
    cairo_fill(cr);
  }
}

// The hangar's rolling door (64 x 256): steel slats, rust, a stencil.
void paintShutter(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 0, 64, 256);
  lgrad(cr, 0, 0, 64, 0, {{0.0, rgb(150, 146, 140)}, {0.5, rgb(184, 180, 172)}, {1.0, rgb(110, 106, 100)}});
  for (double y = 8; y < 256; y += 12)
  {
    cairo_move_to(cr, 0, y);
    cairo_line_to(cr, 64, y);
    stroke(cr, rgb(80, 76, 72), 1.4, 0.7);
    cairo_move_to(cr, 0, y + 2);
    cairo_line_to(cr, 64, y + 2);
    stroke(cr, rgb(230, 226, 220), 0.8, 0.5);
  }
  Rng rng(32251u);
  for (int i = 0; i < 9; ++i)
  {
    const double x = rng.range(4, 60), y = rng.range(10, 240);
    glow(cr, x, y, rng.range(5, 12), rgb(150, 70, 36), 0.6);
    cairo_move_to(cr, x, y);
    cairo_line_to(cr, x + rng.range(-1, 1), y + rng.range(10, 30));
    stroke(cr, rgb(120, 56, 28), 1.4, 0.5);
  }
  cairo_rectangle(cr, 0, 0, 64, 256);
  stroke(cr, kInk, 3.0);
  // The hazard band at its foot.
  for (double x = -8; x < 64; x += 16)
  {
    cairo_move_to(cr, x, 256);
    cairo_line_to(cr, x + 8, 256);
    cairo_line_to(cr, x + 20, 240);
    cairo_line_to(cr, x + 12, 240);
    cairo_close_path(cr);
    setRgba(cr, rgb(240, 190, 40), 1.0);
    cairo_fill(cr);
  }
}

// The Halcyon target (120 x 120, anchored mid): rings on a white board, the
// company's hexagon in the bull.
void paintTargetBoard(cairo_t* cr)
{
  cairo_rectangle(cr, 6, 6, 108, 108);
  setRgba(cr, rgb(250, 250, 248), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 2.4);
  for (int k = 0; k < 4; ++k)
  {
    circle(cr, 60, 60, 48 - k * 12);
    setRgba(cr, k % 2 ? rgb(250, 250, 248) : rgb(220, 50, 40), 1.0);
    cairo_fill(cr);
  }
  cairo_move_to(cr, 60 + 9, 60);
  for (int k = 1; k <= 6; ++k)
    cairo_line_to(cr, 60 + 9 * std::cos(k * kPi / 3), 60 + 9 * std::sin(k * kPi / 3));
  cairo_close_path(cr);
  setRgba(cr, rgb(30, 160, 170), 1.0);
  cairo_fill(cr);
  lettering(cr, "HALCYON", 60, 110, 11, rgb(30, 60, 80));
}

// Halcyon's crate (72 x 64, anchored at the block's top-left (4, 0)).
void paintCrate(cairo_t* cr)
{
  cairo_rectangle(cr, 4, 8, 64, 56);
  lgrad(cr, 0, 8, 0, 64, {{0.0, rgb(250, 250, 252)}, {1.0, rgb(190, 194, 204)}}, 1.0, true);
  stroke(cr, kInk, 2.4);
  cairo_rectangle(cr, 4, 30, 64, 10);
  setRgba(cr, rgb(30, 170, 180), 1.0);
  cairo_fill(cr);
  cairo_move_to(cr, 36 + 8, 20);
  for (int k = 1; k <= 6; ++k)
    cairo_line_to(cr, 36 + 8 * std::cos(k * kPi / 3), 20 + 8 * std::sin(k * kPi / 3));
  cairo_close_path(cr);
  setRgba(cr, rgb(30, 170, 180), 1.0);
  cairo_fill(cr);
  lettering(cr, "HALCYON", 36, 56, 10, rgb(30, 60, 80));
  for (const double x : {8.0, 64.0})
    for (const double y : {12.0, 60.0})
    {
      circle(cr, x, y, 1.6);
      setRgba(cr, rgb(110, 116, 130), 1.0);
      cairo_fill(cr);
    }
}

// A saguaro (170 x 240, anchored at its foot): a ribbed trunk, two arms, a
// flower on top.
void paintCactus(cairo_t* cr)
{
  const Color hi = rgb(120, 180, 96), mid = rgb(70, 136, 70), lo = rgb(40, 90, 50);
  auto limb = [&](std::initializer_list<std::array<double, 2>> pts, double w) {
    bool first = true;
    for (const auto& p : pts)
    {
      if (first)
        cairo_move_to(cr, p[0], p[1]);
      else
        cairo_line_to(cr, p[0], p[1]);
      first = false;
    }
    stroke(cr, kInk, w + 5);
  };
  auto limbFill = [&](std::initializer_list<std::array<double, 2>> pts, double w) {
    bool first = true;
    for (const auto& p : pts)
    {
      if (first)
        cairo_move_to(cr, p[0], p[1]);
      else
        cairo_line_to(cr, p[0], p[1]);
      first = false;
    }
    stroke(cr, mid, w);
    first = true;
    for (const auto& p : pts)
    {
      if (first)
        cairo_move_to(cr, p[0] - w * 0.22, p[1]);
      else
        cairo_line_to(cr, p[0] - w * 0.22, p[1]);
      first = false;
    }
    stroke(cr, hi, w * 0.3, 0.8);
  };
  limb({{85, 150}, {40, 150}, {40, 96}}, 34);
  limb({{85, 118}, {132, 118}, {132, 62}}, 30);
  limbFill({{85, 150}, {40, 150}, {40, 96}}, 34);
  limbFill({{85, 118}, {132, 118}, {132, 62}}, 30);
  // The trunk.
  cairo_move_to(cr, 57, 240);
  cairo_line_to(cr, 57, 62);
  cairo_curve_to(cr, 57, 30, 113, 30, 113, 62);
  cairo_line_to(cr, 113, 240);
  cairo_close_path(cr);
  lgrad(cr, 57, 0, 113, 0, {{0.0, hi}, {0.45, mid}, {1.0, lo}}, 1.0, true);
  stroke(cr, kInk, 3.0);
  for (const double x : {70.0, 85.0, 100.0})
  {
    cairo_move_to(cr, x, 52);
    cairo_line_to(cr, x, 238);
    stroke(cr, lo, 1.6, 0.6);
  }
  // Spines.
  Rng rng(32261u);
  for (int i = 0; i < 40; ++i)
  {
    const double x = 60 + rng.range(0, 50), y = rng.range(50, 236);
    cairo_move_to(cr, x, y);
    cairo_line_to(cr, x + rng.range(-3, 3), y - 3);
    stroke(cr, rgb(250, 240, 210), 0.8, 0.8);
  }
  // A white flower on top.
  for (int k = 0; k < 5; ++k)
  {
    const double a = k * 2 * kPi / 5;
    ellipse(cr, 85 + std::cos(a) * 6, 38 + std::sin(a) * 3, 6, 3.5, a);
    setRgba(cr, rgb(255, 250, 236), 1.0);
    cairo_fill(cr);
  }
  circle(cr, 85, 38, 3);
  setRgba(cr, rgb(250, 200, 60), 1.0);
  cairo_fill(cr);
}

// A tiny cowboy hat (96 x 48, anchored at its crown's middle (48, 30)).
void paintHat(cairo_t* cr)
{
  cairo_move_to(cr, 6, 34);
  cairo_curve_to(cr, 20, 44, 76, 44, 90, 34);
  cairo_curve_to(cr, 80, 30, 70, 32, 62, 32);
  cairo_line_to(cr, 34, 32);
  cairo_curve_to(cr, 26, 32, 16, 30, 6, 34);
  cairo_close_path(cr);
  lgrad(cr, 0, 30, 0, 44, {{0.0, rgb(196, 140, 80)}, {1.0, rgb(110, 70, 36)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  cairo_move_to(cr, 32, 33);
  cairo_curve_to(cr, 30, 14, 36, 6, 44, 10);
  cairo_curve_to(cr, 48, 12, 50, 12, 54, 10);
  cairo_curve_to(cr, 62, 6, 68, 14, 64, 33);
  cairo_close_path(cr);
  lgrad(cr, 0, 6, 0, 33, {{0.0, rgb(214, 160, 96)}, {1.0, rgb(140, 92, 48)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  cairo_rectangle(cr, 32, 26, 32, 5);
  setRgba(cr, rgb(170, 40, 34), 1.0);
  cairo_fill(cr);
}

// A minidish's post (48 x 100, anchored at its foot): a white pole on a
// base plate, a little motor box at the top.
void paintMiniPost(cairo_t* cr)
{
  cairo_rectangle(cr, 6, 92, 36, 8);
  setRgba(cr, rgb(130, 126, 120), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.6);
  cairo_rectangle(cr, 20, 30, 8, 64);
  lgrad(cr, 20, 0, 28, 0, {{0.0, rgb(255, 255, 255)}, {1.0, rgb(150, 156, 170)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  cairo_rectangle(cr, 12, 22, 24, 14);
  lgrad(cr, 0, 22, 0, 36, {{0.0, rgb(240, 240, 244)}, {1.0, rgb(150, 154, 166)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
}

// A minidish's bowl (100 x 70, anchored at its pivot (50, 40)): the bowl
// above the pivot, its feed stick out along the axis.
void paintMiniBowl(cairo_t* cr)
{
  cairo_move_to(cr, 50, 40);
  cairo_line_to(cr, 50, 28);
  stroke(cr, kInk, 5.0);
  cairo_move_to(cr, 14, 18);
  cairo_curve_to(cr, 24, 36, 76, 36, 86, 18);
  cairo_close_path(cr);
  lgrad(cr, 14, 14, 86, 36, {{0.0, rgb(255, 255, 255)}, {1.0, rgb(160, 166, 182)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  ellipse(cr, 50, 18, 36, 5);
  lgrad(cr, 0, 13, 0, 23, {{0.0, rgb(210, 216, 230)}, {1.0, rgb(250, 252, 255)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  cairo_move_to(cr, 50, 20);
  cairo_line_to(cr, 50, 2);
  stroke(cr, rgb(90, 92, 104), 2.0);
  circle(cr, 50, 3, 3.0);
  setRgba(cr, rgb(240, 190, 80), 1.0);
  cairo_fill(cr);
}

// The sand sea's body (64 x 128): pale loose sand, rippled, darker deeper.
void paintSandSea(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 2, 64, 126);
  lgrad(cr, 0, 2, 0, 128, {{0.0, rgb(250, 230, 190)}, {0.12, kSandC}, {1.0, rgb(214, 172, 118)}});
  for (int k = 0; k < 9; ++k)
  {
    const double y = 12 + k * 13.0;
    cairo_move_to(cr, 0, y);
    for (double x = 4; x <= 64; x += 4)
      cairo_line_to(cr, x, y + 2.0 * std::sin(x / 64.0 * 4 * kPi + k * 1.3));
    stroke(cr, rgb(196, 150, 100), 1.0, 0.35);
    cairo_move_to(cr, 0, y - 1.5);
    for (double x = 4; x <= 64; x += 4)
      cairo_line_to(cr, x, y - 1.5 + 2.0 * std::sin(x / 64.0 * 4 * kPi + k * 1.3));
    stroke(cr, rgb(255, 246, 222), 0.8, 0.4);
  }
  Rng rng(32271u);
  for (int i = 0; i < 50; ++i)
  {
    circle(cr, rng.range(0, 64), rng.range(4, 128), rng.range(0.4f, 0.9f));
    setRgba(cr, i % 2 ? rgb(170, 120, 80) : rgb(255, 250, 236), 0.3);
    cairo_fill(cr);
  }
  cairo_move_to(cr, 0, 2);
  cairo_line_to(cr, 64, 2);
  stroke(cr, rgb(255, 250, 236), 1.6, 0.8);
}

// The Jammer's icon (64 x 64): a pistol grip under a boxy body, a grille,
// a little dish on the muzzle with its antenna, amber trim.
void paintJammer(cairo_t* cr)
{
  glow(cr, 32, 32, 30, kJam, 0.25);
  // The grip.
  cairo_move_to(cr, 18, 36);
  cairo_line_to(cr, 30, 36);
  cairo_line_to(cr, 26, 58);
  cairo_line_to(cr, 14, 58);
  cairo_close_path(cr);
  lgrad(cr, 14, 0, 30, 0, {{0.0, rgb(90, 90, 100)}, {1.0, rgb(40, 40, 48)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  // The body.
  cairo_rectangle(cr, 8, 20, 36, 18);
  lgrad(cr, 0, 20, 0, 38, {{0.0, rgb(150, 150, 160)}, {1.0, rgb(70, 70, 80)}}, 1.0, true);
  stroke(cr, kInk, 2.2);
  for (double x = 14; x < 40; x += 5)
  {
    cairo_move_to(cr, x, 24);
    cairo_line_to(cr, x, 34);
    stroke(cr, rgb(30, 30, 36), 1.6);
  }
  cairo_rectangle(cr, 8, 36, 36, 3);
  setRgba(cr, kJam, 1.0);
  cairo_fill(cr);
  // The dish on the muzzle and its feed.
  cairo_move_to(cr, 44, 29);
  cairo_line_to(cr, 48, 29);
  stroke(cr, kInk, 4.0);
  cairo_move_to(cr, 50, 14);
  cairo_curve_to(cr, 44, 22, 44, 36, 50, 44);
  cairo_curve_to(cr, 54, 36, 54, 22, 50, 14);
  cairo_close_path(cr);
  lgrad(cr, 44, 0, 54, 0, {{0.0, rgb(255, 255, 255)}, {1.0, rgb(160, 166, 182)}}, 1.0, true);
  stroke(cr, kInk, 1.8);
  cairo_move_to(cr, 50, 29);
  cairo_line_to(cr, 60, 29);
  stroke(cr, rgb(90, 90, 100), 1.6);
  circle(cr, 60, 29, 2.0);
  setRgba(cr, kJam, 1.0);
  cairo_fill(cr);
  // The antenna.
  cairo_move_to(cr, 14, 20);
  cairo_line_to(cr, 10, 4);
  stroke(cr, kInk, 2.2);
  circle(cr, 10, 4, 2.4);
  setRgba(cr, rgb(255, 80, 60), 1.0);
  cairo_fill(cr);
}

// Heatstroke's swap sign (140 x 140, anchored mid): an orange diamond, two
// arrows passing each other.
void paintSwapSign(cairo_t* cr)
{
  cairo_move_to(cr, 70, 4);
  cairo_line_to(cr, 136, 70);
  cairo_line_to(cr, 70, 136);
  cairo_line_to(cr, 4, 70);
  cairo_close_path(cr);
  lgrad(cr, 0, 4, 0, 136, {{0.0, rgb(255, 170, 60)}, {1.0, rgb(230, 110, 30)}}, 1.0, true);
  stroke(cr, kInk, 5.0);
  cairo_move_to(cr, 70, 14);
  cairo_line_to(cr, 126, 70);
  cairo_line_to(cr, 70, 126);
  cairo_line_to(cr, 14, 70);
  cairo_close_path(cr);
  stroke(cr, kInk, 2.0);
  for (const double dir : {1.0, -1.0})
  {
    const double y = dir > 0 ? 56 : 84;
    const double x0 = 70 - dir * 34, x1 = 70 + dir * 34;
    cairo_move_to(cr, x0, y);
    cairo_line_to(cr, x1 - dir * 10, y);
    stroke(cr, kInk, 7.0);
    cairo_move_to(cr, x1, y);
    cairo_line_to(cr, x1 - dir * 16, y - 10);
    cairo_line_to(cr, x1 - dir * 16, y + 10);
    cairo_close_path(cr);
    setRgba(cr, kInk, 1.0);
    cairo_fill(cr);
  }
}

// --- Level 32's layout (in blocks) ---------------------------------------------------------------------

struct BlockRect
{
  int x0, y0, x1, y1;
  bool has(int bx, int by) const { return bx >= x0 && bx <= x1 && by >= y0 && by <= y1; }
};

constexpr BlockRect kTruck{7, 36, 11, 37};
constexpr BlockRect kMesa{118, 24, 140, 43};
constexpr BlockRect kHangar{132, 33, 139, 37};
constexpr BlockRect kCabin{117, 14, 122, 20};
constexpr BlockRect kCabinInside{117, 15, 121, 19};
constexpr BlockRect kBunker{155, 32, 170, 37};
constexpr BlockRect kBunkerInside{155, 33, 170, 37};
constexpr BlockRect kTower{205, 18, 212, 43};
constexpr BlockRect kFooting{185, 34, 204, 37};
constexpr BlockRect kD4Struts{185, 34, 192, 37};

// The Dish Array itself (not the Heatstroke bonus).
bool isArray(const World& w)
{
  const auto& s = w.dish();
  return s.on && !s.swapLr && !s.rides.empty() && w.level().width >= 213;
}

bool solidBlock(const World& w, int bx, int by)
{
  return bx >= 0 && by >= 0 && bx < w.level().width && by < w.level().height && w.map().block(bx, by) == Tile::Solid;
}

// Where a ride is now, drawn: its catwalk's middle and top (screen px), its
// width, the bowl's angle, the groan's shake.
struct RideView
{
  float cx = 0.0f, top = 0.0f, w = 0.0f;
  float angle = 0.0f;
  float shake = 0.0f;
  bool groan = false;
  std::string id;
};

DishPhase phaseOf(const DishState& s, int dish)
{
  if (dish >= 0 && std::size_t(dish) < s.small.size())
    return dishPhase(s.small[std::size_t(dish)].t, true);
  return dishPhase(s.t, false);
}

RideView viewOf(const World& w, const DishRide& ride, float camX, float camY, int frame, float alpha)
{
  RideView v;
  const auto& pls = w.platforms();
  if (ride.platform < 0 || std::size_t(ride.platform) >= pls.size())
    return v;
  const Platform& pl = pls[std::size_t(ride.platform)];
  const float x = float(pl.prevX) + float(pl.x - pl.prevX) * alpha, y = float(pl.prevY) + float(pl.y - pl.prevY) * alpha;
  v.id = pl.id;
  v.w = float(pl.w) * kCellPx;
  v.cx = x * kCellPx + v.w * 0.5f - camX;
  v.top = y * kCellPx - camY;
  const DishPhase ph = phaseOf(w.dish(), ride.dish);
  const int a = int(ph.from), b = int(ph.to);
  float t = 0.0f;
  if (ph.n > 0)
  {
    const float dx = float(ride.px[b] - ride.px[a]), dy = float(ride.py[b] - ride.py[a]);
    const float len2 = dx * dx + dy * dy;
    t = len2 > 0.0f ? std::clamp(((x - float(ride.px[a])) * dx + (y - float(ride.py[a])) * dy) / len2, 0.0f, 1.0f)
                    : float(ph.k) / float(ph.n);
  }
  v.angle = kPoseAngle[std::size_t(a)] + (kPoseAngle[std::size_t(b)] - kPoseAngle[std::size_t(a)]) * smooth(t);
  v.groan = ph.groan;
  if (ph.groan)
    v.shake = (frame / 2) % 2 ? 4.0f : -4.0f;
  return v;
}

bool isArm(const std::string& id) { return !id.empty() && id.back() == 'a'; }

float bowlFactor(const std::string& id)
{
  if (id == "D4r")
    return 1.45f;
  if (id == "D0r")
    return 1.1f;
  return 1.12f;
}

// A point on a bowl drawn at (cx, top) at angle a and scale sc: (bx, by)
// in the bowl texture's units from its anchor.
std::array<float, 2> bowlPoint(float cx, float top, float a, float sc, float bx, float by)
{
  const float c = std::cos(a), s = std::sin(a);
  return {cx + (bx * c - by * s) * sc, top + (bx * s + by * c) * sc};
}

constexpr float kBowlDrop = 10.0f; // the aperture sits this far under the catwalk's top

// A pedestal from the ground up to a hub, the hydraulic yoke from the hub to
// the bowl's back.
void drawPedestal(Renderer& r, const Art& art, float hubX, float hubY, float groundY, float sc)
{
  const Texture& lat = baked(art, r, "lattice", 96, 64, 48, 64, paintLattice);
  DrawOpts o;
  o.scale = std::max(0.6f, sc);
  const float step = 64.0f * o.scale;
  for (float y = groundY; y > hubY + 10.0f; y -= step)
    r.draw(lat, hubX, y, o);
  r.draw(baked(art, r, "plinth", 160, 40, 80, 40, paintPlinth), hubX, groundY + 4.0f, o);
}

void drawHub(Renderer& r, const Art& art, float hubX, float hubY, float sc)
{
  DrawOpts o;
  o.scale = std::max(0.6f, sc);
  r.draw(baked(art, r, "hub", 120, 90, 60, 45, paintHub), hubX, hubY, o);
}

// A bowl at its view, with its yoke from the hub.
void drawBowl(Renderer& r, const Art& art, const RideView& v, float hubX, float hubY, bool hub, int frame)
{
  (void)frame;
  const float sc = v.w * bowlFactor(v.id) / 400.0f;
  const float cx = v.cx + v.shake, top = v.top + kBowlDrop;
  if (offScreen(cx - 320.0f * sc, top - 160.0f * sc, 640.0f * sc, 300.0f * sc, 200.0f))
    return;
  if (hub)
  {
    // The yoke: a hydraulic ram from the hub to the mounting plate.
    const auto back = bowlPoint(cx, top, v.angle, sc, 0.0f, 102.0f);
    const float dx = back[0] - hubX, dy = back[1] - hubY;
    const float mx = hubX + dx * 0.55f, my = hubY + dy * 0.55f;
    beamLine(r, hubX, hubY, mx, my, 22.0f * std::max(0.7f, sc), rgb(236, 238, 244));
    beamLine(r, mx, my, back[0], back[1], 12.0f * std::max(0.7f, sc), rgb(176, 180, 192));
    r.drawLine(hubX, hubY, mx, my, 4.0f, rgba(255, 255, 255, 160));
    drawHub(r, art, hubX, hubY, sc * 0.9f);
  }
  DrawOpts o;
  o.scale = sc;
  o.angle = v.angle * 57.2958f;
  o.cull = false;
  r.draw(baked(art, r, v.id == "D4r" ? "bowl42" : "bowl", 440, 290, 220, 150, [n42 = v.id == "D4r"](cairo_t* cr) { paintBowl(cr, n42); }),
    cx, top, o);
  // The groan: a shiver of light along the rim.
  if (v.groan)
  {
    const auto l = bowlPoint(cx, top, v.angle, sc, -200.0f, 0.0f), rr = bowlPoint(cx, top, v.angle, sc, 200.0f, 0.0f);
    r.drawLine(l[0], l[1], rr[0], rr[1], 3.0f, rgba(255, 220, 160, 90 + ((frame / 2) % 2) * 60), Blend::Add);
  }
}

// A catwalk along a rim or an arm.
void drawCatwalk(Renderer& r, const Art& art, const RideView& v)
{
  const float x0 = v.cx - v.w * 0.5f + v.shake;
  if (offScreen(x0, v.top, v.w, 40.0f))
    return;
  for (float tx = 0.0f; tx < v.w - 1.0f; tx += kTilePx)
    r.draw(art.platform, x0 + tx, v.top);
  // Handrail posts at its ends.
  for (const float x : {x0 + 4.0f, x0 + v.w - 4.0f})
  {
    r.drawLine(x, v.top, x, v.top - 34.0f, 5.0f, kInk);
    r.drawLine(x, v.top, x, v.top - 34.0f, 2.6f, rgb(240, 240, 244));
  }
  r.drawLine(x0 + 4.0f, v.top - 34.0f, x0 + v.w - 4.0f, v.top - 34.0f, 2.0f, rgba(240, 240, 244, 150));
}

// The ground under a point (screen px), looking down at most `maxBlocks`.
float groundBelow(const World& w, float worldX, float worldY, int maxBlocks)
{
  const int bx = int(std::floor(worldX / kTilePx));
  int by = int(std::floor(worldY / kTilePx));
  for (int k = 0; k < maxBlocks && by < w.level().height; ++k, ++by)
    if (solidBlock(w, bx, by))
      return float(by) * kTilePx;
  return float(by) * kTilePx;
}

void drawSatellite(Renderer& r, const World& w, const Art& art, int frame, float alpha)
{
  const auto& s = w.dish();
  const float period = float(std::max(60, s.satellitePeriod));
  const float t = s.satellitePeriod == kDishCycle ? float(s.t) + alpha : float(w.stats().frames) + alpha;
  auto at = [&](float tt) {
    const float u = std::fmod(tt + 240.0f * period / float(kDishCycle) + period * 4.0f, period) / period;
    const float x = 1360.0f - u * 1440.0f;
    const float k = std::clamp((1360.0f - x) / 1440.0f, 0.0f, 1.0f);
    return std::array<float, 2>{x, 150.0f - 80.0f * std::sin(k * float(kPi))};
  };
  // The three markers; the one the dishes turn to next glows.
  const DishPhase ph = dishPhase(s.t, false);
  const int next = ph.n > 0 ? int(ph.to) : (ph.from == DishPose::East ? 1 : (ph.from == DishPose::Over ? 2 : 0));
  constexpr std::array<int, 3> kAt{780, 210, 495}; // the satellite at E, O, W (the groans start)
  constexpr std::array<const char*, 3> kName{"E", "O", "W"};
  for (int i = 0; i < 3; ++i)
  {
    const auto p = at(float(kAt[std::size_t(i)]) * period / float(kDishCycle));
    const bool hot = i == next;
    const float pulse = hot ? 0.5f + 0.5f * std::sin(float(frame) * 0.15f) : 0.0f;
    const Color c = hot ? rgb(255, 230, 160) : rgb(170, 200, 240);
    const int a = hot ? 150 + int(80.0f * pulse) : 90;
    for (int k = 0; k < 8; ++k)
    {
      // A dashed ring.
      const float a0 = float(k) * float(kPi) / 4.0f, a1 = a0 + float(kPi) / 8.0f;
      r.drawLine(p[0] + std::cos(a0) * 16.0f, p[1] + std::sin(a0) * 16.0f, p[0] + std::cos(a1) * 16.0f, p[1] + std::sin(a1) * 16.0f,
        2.0f, withAlpha(c, a));
    }
    r.drawLine(p[0] - 24.0f, p[1], p[0] - 18.0f, p[1], 2.0f, withAlpha(c, a));
    r.drawLine(p[0] + 18.0f, p[1], p[0] + 24.0f, p[1], 2.0f, withAlpha(c, a));
    r.drawText(kName[std::size_t(i)], p[0], p[1] + 20.0f, {14.0f, withAlpha(c, a), 0}, Align::Center);
    if (hot)
      drawGlow(r, art, p[0], p[1], 30.0f, c, 0.25f * pulse);
  }
  // The satellite: a moving star, its panels catching the sun, a faint
  // trail behind it.
  const auto p = at(t);
  for (int k = 1; k <= 6; ++k)
  {
    const auto q = at(t - float(k) * 1.5f);
    if (std::abs(q[0] - p[0]) < 100.0f)
      r.fillRect(q[0] - 1.5f, q[1] - 1.5f, 3.0f, 3.0f, rgba(255, 255, 240, 120 - k * 18), Blend::Add);
  }
  drawGlow(r, art, p[0], p[1], 26.0f, rgb(255, 250, 220), 0.7f);
  const float tw = 0.7f + 0.3f * std::sin(float(frame) * 0.5f);
  r.drawLine(p[0] - 14.0f * tw, p[1], p[0] + 14.0f * tw, p[1], 1.5f, rgba(255, 255, 255, 200), Blend::Add);
  r.drawLine(p[0], p[1] - 10.0f * tw, p[0], p[1] + 10.0f * tw, 1.5f, rgba(255, 255, 255, 200), Blend::Add);
  r.fillRect(p[0] - 9.0f, p[1] - 1.5f, 5.0f, 3.0f, rgb(120, 150, 220));
  r.fillRect(p[0] + 4.0f, p[1] - 1.5f, 5.0f, 3.0f, rgb(120, 150, 220));
  r.fillRect(p[0] - 2.5f, p[1] - 2.5f, 5.0f, 5.0f, rgb(255, 255, 245));
}

// Heatstroke's fixed dishes: a bowl under each long run of rim, a lattice
// tower under each short one, the mast's antennas.
void drawHeatstrokeSet(Renderer& r, const World& w, const Art& art, float camX, float camY, int frame)
{
  const auto& lv = w.level();
  int dishIndex = 0;
  for (int by = 0; by < lv.height; ++by)
  {
    int bx = 0;
    while (bx < lv.width)
    {
      if (w.map().block(bx, by) != Tile::Platform)
      {
        ++bx;
        continue;
      }
      int end = bx;
      while (end + 1 < lv.width && w.map().block(end + 1, by) == Tile::Platform)
        ++end;
      const int run = end - bx + 1;
      const float top = float(by) * kTilePx - camY;
      if (run >= 6)
      {
        // A dish: its bowl under the first eight blocks of the run.
        const int span = std::min(run, 8);
        RideView v;
        v.w = float(span) * kTilePx * 0.95f;
        v.cx = (float(bx) + float(span) * 0.5f) * kTilePx - camX;
        v.top = top;
        v.id = "fixed";
        v.angle = kPoseAngle[std::size_t(dishIndex % 3)] * 0.6f;
        const float ground = groundBelow(w, (float(bx) + float(span) * 0.5f) * kTilePx, float(by + 1) * kTilePx, 20) - camY;
        const float sc = v.w * bowlFactor(v.id) / 400.0f;
        const float hubY = std::min(ground - 40.0f, top + 150.0f * sc);
        if (!offScreen(v.cx - v.w, top - 200.0f, v.w * 2.0f, ground - top + 240.0f))
        {
          drawPedestal(r, art, v.cx, hubY, ground, sc * 0.8f);
          drawBowl(r, art, v, v.cx, hubY, true, frame);
        }
        ++dishIndex;
      }
      else
      {
        // A strut tower under a short run.
        const float ground = groundBelow(w, (float(bx) + float(run) * 0.5f) * kTilePx, float(by + 1) * kTilePx, 20) - camY;
        const Texture& st = baked(art, r, "strut", 64, 64, 0, 0, paintStrut);
        for (int k = 0; k < run; ++k)
          for (float y = top + 16.0f; y < ground; y += 64.0f)
            r.draw(st, float(bx + k) * kTilePx - camX, y);
      }
      bx = end + 1;
    }
  }
}

} // namespace

// --- Behind the tiles ------------------------------------------------------------------------------------

void drawDishSet(Renderer& r, const World& w, const Art& art, float camX, float camY, int frame, float alpha)
{
  const auto& s = w.dish();
  drawSatellite(r, w, art, frame, alpha);
  if (s.swapLr)
  {
    drawHeatstrokeSet(r, w, art, camX, camY, frame);
    return;
  }
  const bool array = isArray(w);
  // The struts: solid white trusses in their poses, faint out of them,
  // blinking amber as they come in.
  for (const auto& st : s.struts)
  {
    if (st.layer < 0 || std::size_t(st.layer) >= w.layers().size())
      continue;
    const Layer& l = w.layers()[std::size_t(st.layer)];
    const float x = float(l.x0) * kTilePx - camX, y = float(l.y0) * kTilePx - camY;
    const float bw = float(l.x1 - l.x0 + 1) * kTilePx, bh = float(l.y1 - l.y0 + 1) * kTilePx;
    if (offScreen(x, y, bw, bh))
      continue;
    const DishPhase ph = phaseOf(s, st.dish);
    const bool coming = !l.solid && ph.n > 0 && st.solidIn[int(ph.to)];
    float a = l.solid ? 1.0f : 0.22f;
    if (coming)
      a = 0.22f + 0.5f * float(ph.k) / float(ph.n);
    DrawOpts o;
    o.alpha = a;
    const Texture& tex = baked(art, r, "strut", 64, 64, 0, 0, paintStrut);
    for (float ty = 0.0f; ty < bh - 1.0f; ty += kTilePx)
      for (float tx = 0.0f; tx < bw - 1.0f; tx += kTilePx)
        r.draw(tex, x + tx, y + ty, o);
    if (l.solid)
    {
      // Its deck.
      for (float tx = 0.0f; tx < bw - 1.0f; tx += kTilePx)
        r.draw(art.platform, x + tx, y);
    }
    else
    {
      // A dotted outline where it will be.
      const Color c = coming && (frame / 4) % 2 ? rgba(255, 190, 60, 220) : rgba(255, 255, 255, 90);
      for (float d = 0.0f; d < bw; d += 16.0f)
      {
        r.fillRect(x + d, y, 8.0f, 3.0f, c);
        r.fillRect(x + d, y + bh - 3.0f, 8.0f, 3.0f, c);
      }
      for (float d = 0.0f; d < bh; d += 16.0f)
      {
        r.fillRect(x, y + d, 3.0f, 8.0f, c);
        r.fillRect(x + bw - 3.0f, y + d, 3.0f, 8.0f, c);
      }
    }
  }
  // The dishes: pedestals first, then the bowls, then the catwalks.
  std::vector<RideView> views;
  views.reserve(s.rides.size());
  for (const auto& ride : s.rides)
    views.push_back(viewOf(w, ride, camX, camY, frame, alpha));
  for (std::size_t i = 0; i < s.rides.size(); ++i)
  {
    const auto& ride = s.rides[i];
    const RideView& v = views[i];
    if (v.w <= 0.0f || isArm(v.id))
      continue;
    // The hub: under the middle of the rim's three poses.
    const float sc = v.w * bowlFactor(v.id) / 400.0f;
    float minX = 1e9f, maxX = -1e9f, maxY = -1e9f;
    for (int k = 0; k < 3; ++k)
    {
      minX = std::min(minX, float(ride.px[k]));
      maxX = std::max(maxX, float(ride.px[k]));
      maxY = std::max(maxY, float(ride.py[k]));
    }
    const float hubWX = (minX + maxX) * 0.5f * kCellPx + v.w * 0.5f;
    const float lowest = maxY * kCellPx + kBowlDrop + 110.0f * sc;
    const float groundW = groundBelow(w, hubWX, lowest, 30);
    const float hubWY = std::min(groundW - 50.0f, lowest + 30.0f);
    const float hubX = hubWX - camX, hubY = hubWY - camY, groundY = groundW - camY;
    if (!offScreen(hubX - 400.0f, std::min(hubY, v.top) - 300.0f, 800.0f, groundY - std::min(hubY, v.top) + 400.0f))
      drawPedestal(r, art, hubX, hubY, groundY, sc);
    drawBowl(r, art, v, hubX, hubY, true, frame);
    if (v.id == "D0r" && array)
    {
      // The teaching dish's sign by its pedestal.
      DrawOpts o;
      o.scale = 0.8f;
      r.draw(baked(art, r, "placard", 120, 70, 60, 70, paintPlacard), hubX + 120.0f, groundY, o);
    }
  }
  // Feed arms: a lattice boom from their dish's rim to the arm, the horn
  // cabin at the arm's tip.
  for (std::size_t i = 0; i < s.rides.size(); ++i)
  {
    const RideView& v = views[i];
    if (v.w <= 0.0f || !isArm(v.id))
      continue;
    const RideView* dish = nullptr;
    for (const auto& o : views)
      if (o.id.size() == v.id.size() && o.id.compare(0, o.id.size() - 1, v.id, 0, v.id.size() - 1) == 0 && !isArm(o.id))
        dish = &o;
    const float ax = v.cx - v.w * 0.5f + v.shake, ay = v.top + 14.0f;
    if (dish)
    {
      const float sc = dish->w * bowlFactor(dish->id) / 400.0f;
      const auto lip = bowlPoint(dish->cx + dish->shake, dish->top + kBowlDrop, dish->angle, sc, 190.0f, 0.0f);
      const auto focus = bowlPoint(dish->cx + dish->shake, dish->top + kBowlDrop, dish->angle, sc, 0.0f, -92.0f);
      beamLine(r, lip[0], lip[1], ax + 10.0f, ay + 8.0f, 9.0f, rgb(230, 232, 240));
      beamLine(r, focus[0], focus[1], ax + 10.0f, ay, 7.0f, rgb(230, 232, 240));
      // Bracing between the two.
      for (int k = 1; k < 5; ++k)
      {
        const float u = float(k) / 5.0f, u2 = float(k + 1) / 5.0f;
        const float x0 = lip[0] + (ax + 10.0f - lip[0]) * u, y0 = lip[1] + (ay + 8.0f - lip[1]) * u;
        const float x1 = focus[0] + (ax + 10.0f - focus[0]) * u2, y1 = focus[1] + (ay - focus[1]) * u2;
        r.drawLine(x0, y0, x1, y1, 3.0f, rgb(200, 204, 214));
      }
    }
    drawCatwalk(r, art, v);
    r.draw(baked(art, r, "horncabin", 120, 110, 24, 30, paintHornCabin), v.cx + v.w * 0.5f + v.shake - 24.0f, v.top + 2.0f);
  }
  for (const auto& v : views)
    if (v.w > 0.0f && !isArm(v.id))
      drawCatwalk(r, art, v);
  if (!array)
    return;
  // The receiver cabin: inside, its racks and the window; the stilts under it.
  {
    const float x = float(kCabinInside.x0) * kTilePx - camX, y = float(kCabinInside.y0) * kTilePx - camY;
    if (!offScreen(x, y, 6.0f * kTilePx, 10.0f * kTilePx))
    {
      const Texture& in = baked(art, r, "cabininside", 64, 64, 0, 0, paintCabinInside);
      for (int by = kCabinInside.y0; by <= kCabinInside.y1; ++by)
        for (int bx = kCabinInside.x0; bx <= kCabinInside.x1; ++bx)
          r.draw(in, float(bx) * kTilePx - camX, float(by) * kTilePx - camY);
      // A window on the sky.
      const float wx = x + 2.5f * kTilePx, wy = y + 0.4f * kTilePx;
      r.fillRect(wx, wy, 120.0f, 60.0f, rgb(40, 40, 46));
      r.fillRect(wx + 4.0f, wy + 4.0f, 112.0f, 52.0f, rgb(110, 150, 210));
      r.fillRect(wx + 4.0f, wy + 40.0f, 112.0f, 16.0f, rgb(220, 160, 110));
      r.drawLine(wx + 60.0f, wy + 4.0f, wx + 60.0f, wy + 56.0f, 3.0f, rgb(40, 40, 46));
      // The racks along the back.
      const float floor = float(kCabin.y1) * kTilePx - camY;
      r.draw(baked(art, r, "rack", 110, 150, 0, 150, paintRack), x + 3.4f * kTilePx, floor);
      for (int k = 0; k < 6; ++k)
      {
        const bool on = hash2(frame / 6, k) % 3u != 0;
        r.fillRect(x + 3.4f * kTilePx + 13.0f + float(k) * 15.0f, floor - 30.0f, 8.0f, 8.0f, on ? rgb(120, 255, 140) : rgb(40, 70, 50));
      }
      // The meters' needles, twitching to the satellite's signal.
      for (int k = 0; k < 2; ++k)
      {
        const float mx = x + 3.4f * kTilePx + 32.0f + float(k) * 46.0f, my = floor - 150.0f + 40.0f;
        const float a = -2.2f + 0.6f * unit(hash2(frame / 3, k)) + 0.6f;
        r.drawLine(mx, my, mx + std::cos(a) * 20.0f, my + std::sin(a) * 20.0f, 1.5f, rgb(60, 30, 20));
      }
    }
    // The stilts down to the mesa.
    const float sy0 = float(kCabin.y1 + 1) * kTilePx - camY, sy1 = float(kMesa.y0) * kTilePx - camY;
    for (const float bx : {float(kCabin.x0) + 0.3f, float(kCabin.x1) - 0.3f})
    {
      const float sx = bx * kTilePx - camX;
      if (offScreen(sx - 20.0f, sy0, 40.0f, sy1 - sy0))
        continue;
      beamLine(r, sx, sy0 - 4.0f, sx, sy1 + 4.0f, 10.0f, rgb(210, 212, 220));
    }
    const float bx0 = (float(kCabin.x0) + 0.3f) * kTilePx - camX, bx1 = (float(kCabin.x1) - 0.3f) * kTilePx - camX;
    r.drawLine(bx0, sy0, bx1, sy1, 4.0f, rgb(180, 184, 196));
    r.drawLine(bx1, sy0, bx0, sy1, 4.0f, rgb(180, 184, 196));
  }
  // The hangar's back wall, a lamp and a stripe.
  {
    const float x = float(kHangar.x0) * kTilePx - camX, y = float(kHangar.y0) * kTilePx - camY;
    const float bw = float(kHangar.x1 - kHangar.x0 + 1) * kTilePx, bh = float(kHangar.y1 - kHangar.y0 + 1) * kTilePx;
    if (!offScreen(x, y, bw, bh))
    {
      const Texture& wall = baked(art, r, "inside0", 64, 64, 0, 0, [](cairo_t* cr) { paintInsideWall(cr, false); });
      for (int by = kHangar.y0; by <= kHangar.y1; ++by)
        for (int bx = kHangar.x0; bx <= kHangar.x1; ++bx)
          if (!solidBlock(w, bx, by))
            r.draw(wall, float(bx) * kTilePx - camX, float(by) * kTilePx - camY);
      for (float d = 0.0f; d < bw; d += 32.0f)
        r.fillRect(x + d, y + bh - 70.0f, 16.0f, 8.0f, rgb(240, 190, 40));
      const float lx = x + bw * 0.5f, ly = y + 30.0f;
      r.drawLine(lx, y, lx, ly, 2.0f, rgb(30, 30, 30));
      r.fillRect(lx - 16.0f, ly, 32.0f, 8.0f, rgb(60, 60, 64));
      drawGlow(r, art, lx, ly + 30.0f, 140.0f, rgb(255, 220, 160), 0.3f);
    }
  }
  // The bunker: its back wall, consoles, the tracking screen, an alarm light.
  {
    const float x = float(kBunkerInside.x0) * kTilePx - camX, y = float(kBunkerInside.y0) * kTilePx - camY;
    const float bw = float(kBunkerInside.x1 - kBunkerInside.x0 + 1) * kTilePx, bh = float(kBunkerInside.y1 - kBunkerInside.y0 + 1) * kTilePx;
    if (!offScreen(x, y, bw, bh))
    {
      DrawOpts dim;
      dim.tint = rgb(200, 196, 190);
      const Texture& wall0 = baked(art, r, "inside0", 64, 64, 0, 0, [](cairo_t* cr) { paintInsideWall(cr, false); });
      const Texture& wall1 = baked(art, r, "inside1", 64, 64, 0, 0, [](cairo_t* cr) { paintInsideWall(cr, true); });
      for (int by = kBunkerInside.y0; by <= kBunkerInside.y1; ++by)
        for (int bx = kBunkerInside.x0; bx <= kBunkerInside.x1; ++bx)
          if (!solidBlock(w, bx, by))
            r.draw(by == kBunkerInside.y0 ? wall1 : wall0, float(bx) * kTilePx - camX, float(by) * kTilePx - camY, dim);
      const float floor = float(kBunkerInside.y1 + 1) * kTilePx - camY;
      r.draw(baked(art, r, "console", 300, 110, 0, 110, paintConsole), x + 2.0f * kTilePx, floor);
      r.draw(baked(art, r, "console", 300, 110, 0, 110, paintConsole), x + 8.6f * kTilePx, floor);
      // The tracking screen: the satellite on its arc, the dishes' pose.
      const float sx = x + 5.0f * kTilePx, sy = y + 8.0f;
      r.draw(baked(art, r, "trackscreen", 260, 150, 0, 0, paintTrackScreen), sx, sy);
      const float u = std::fmod(float(s.t) + alpha + 240.0f, float(kDishCycle)) / float(kDishCycle);
      const float ang = float(kPi) * (2.0f - std::clamp(u * 1.2f, 0.0f, 1.0f));
      const float px = sx + 130.0f + std::cos(ang) * 96.0f, py = sy + 120.0f + std::sin(ang) * 96.0f;
      if (u < 0.83f)
      {
        drawGlow(r, art, px, py, 18.0f, rgb(120, 255, 170), 0.8f);
        r.fillRect(px - 3.0f, py - 3.0f, 6.0f, 6.0f, rgb(220, 255, 230));
      }
      // The dishes' axis on the screen.
      const DishPhase ph = dishPhase(s.t, false);
      const float da = kPoseAngle[std::size_t(ph.from)] +
        (ph.n > 0 ? (kPoseAngle[std::size_t(ph.to)] - kPoseAngle[std::size_t(ph.from)]) * float(ph.k) / float(ph.n) : 0.0f);
      r.drawLine(sx + 130.0f, sy + 120.0f, sx + 130.0f + std::sin(da) * 70.0f, sy + 120.0f - std::cos(da) * 70.0f, 2.0f,
        rgba(255, 200, 120, 200));
      if (ph.groan && (frame / 4) % 2)
        r.drawText("SLEW", sx + 130.0f, sy + 20.0f, {14.0f, rgb(255, 120, 90), 0}, Align::Center);
      // The alarm light over the door, turning.
      const float lx = x + 0.6f * kTilePx, ly = y + 14.0f;
      r.fillRect(lx - 10.0f, ly - 8.0f, 20.0f, 14.0f, rgb(60, 20, 20));
      drawGlow(r, art, lx, ly, 60.0f, rgb(255, 60, 40), 0.25f + 0.25f * std::max(0.0f, std::sin(float(frame) * 0.2f)));
    }
  }
  // The uplink tower's mast, its light blinking.
  {
    const float x = (float(kTower.x0) + 0.6f) * kTilePx - camX, y = float(kTower.y0) * kTilePx - camY;
    if (!offScreen(x - 60.0f, y - 460.0f, 120.0f, 470.0f))
    {
      r.draw(baked(art, r, "mast", 120, 460, 60, 460, paintMast), x, y + 2.0f);
      if ((frame / 20) % 2 == 0)
        drawGlow(r, art, x, y - 446.0f, 40.0f, rgb(255, 60, 40), 0.85f);
    }
  }
}

void drawJammerIcon(Renderer& r, const Art& art, float x, float y, float scale, int frame)
{
  DrawOpts o;
  o.scale = scale;
  r.draw(baked(art, r, "jammer", 64, 64, 0, 0, paintJammer), x, y, o);
  // Its LED blinks, the little dish's feed glows.
  if ((frame / 8) % 2 == 0)
    drawGlow(r, art, x + 10.0f * scale, y + 4.0f * scale, 10.0f * scale, rgb(255, 80, 60), 0.8f);
  drawGlow(r, art, x + 60.0f * scale, y + 29.0f * scale, 12.0f * scale, kJam, 0.4f + 0.2f * std::sin(float(frame) * 0.2f));
}

float dishWobble(const World& w, int frame)
{
  const auto& s = w.dish();
  if (!s.swapLr || s.swapT < kSwapEvery - kSwapWobble)
    return 0.0f;
  return 2.0f * kPixelScale * std::sin(float(frame) * 0.9f);
}

// --- Behind the runner -----------------------------------------------------------------------------------

void World::drawDishBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)alpha;
  const auto& s = mDish;
  const bool array = isArray(*this);
  const int bx0 = std::max(0, int(camX / kTilePx) - 1), bx1 = std::min(mLevel->width - 1, int((camX + float(kScreenW)) / kTilePx) + 1);
  const int by0 = std::max(0, int(camY / kTilePx) - 1), by1 = std::min(mLevel->height - 1, int((camY + float(kScreenH)) / kTilePx) + 1);
  auto solid = [&](int bx, int by) { return solidBlock(*this, bx, by); };
  auto depthTint = [&](int bx, int by) {
    int depth = 0;
    while (depth < 3 && solid(bx, by - depth - 1))
      ++depth;
    static constexpr int kShade[4] = {255, 210, 175, 145};
    return rgb(kShade[depth], kShade[depth], kShade[depth]);
  };
  // The sand sea: loose sand over the ground, rippling slowly in the wind.
  for (const auto& b : s.sand)
  {
    const float x = float(b.x) * kCellPx - camX, y = float(b.y) * kCellPx - camY;
    const float bw = float(b.w) * kCellPx, bh = float(b.h) * kCellPx;
    if (offScreen(x, y, bw, bh))
      continue;
    const Texture& sea = baked(mArt, r, "sandsea", 64, 128, 0, 0, paintSandSea);
    for (float tx = std::max(0.0f, std::floor((-x) / kTilePx) * kTilePx); tx < bw - 1.0f && x + tx < float(kScreenW) + 64.0f; tx += kTilePx)
      for (float ty = 0.0f; ty < bh - 1.0f; ty += 128.0f)
        r.draw(sea, x + tx, y + ty);
    // Ripple crests drifting along the surface.
    for (int i = 0; i < 40; ++i)
    {
      const std::uint32_t h = hash2(i, 3201);
      const float rx = std::fmod(float(h % 4000u) + float(frame) * 0.25f, bw);
      const float ry = y + 6.0f + float((h >> 12) % 40u);
      if (x + rx < -60.0f || x + rx > float(kScreenW) + 60.0f)
        continue;
      r.drawLine(x + rx, ry, x + rx + 26.0f, ry - 2.0f, 2.0f, rgba(255, 248, 228, 90));
      r.drawLine(x + rx + 2.0f, ry + 3.0f, x + rx + 24.0f, ry + 2.0f, 1.5f, rgba(190, 140, 90, 70));
    }
  }
  if (s.swapLr)
  {
    // Heatstroke: the dunes (sand over the ground's top), the mast.
    for (int by = by0; by <= by1; ++by)
      for (int bx = bx0; bx <= bx1; ++bx)
      {
        if (!solid(bx, by))
          continue;
        const float x = float(bx) * kTilePx - camX, y = float(by) * kTilePx - camY;
        const bool top = !solid(bx, by - 1);
        if (by < mLevel->height - 4 && !(top && by >= mLevel->height - 4))
        {
          // Above the dunes: the mast's base.
          DrawOpts o;
          o.tint = depthTint(bx, by);
          r.draw(baked(mArt, r, by % 3 == 1 ? "tower2" : "tower0", 64, 64, 0, 0,
                   [v = by % 3 == 1 ? 2 : 0](cairo_t* cr) { paintTowerBlock(cr, v); }),
            x, y, o);
          continue;
        }
        if (top)
          r.draw(baked(mArt, r, "sandsea", 64, 128, 0, 0, paintSandSea), x, y);
      }
    return;
  }
  if (!array)
    return;
  for (int by = by0; by <= by1; ++by)
    for (int bx = bx0; bx <= bx1; ++bx)
    {
      if (!solid(bx, by) || kTruck.has(bx, by))
        continue;
      const float x = float(bx) * kTilePx - camX, y = float(by) * kTilePx - camY;
      const std::uint32_t h = hash2(bx, by);
      DrawOpts o;
      o.tint = depthTint(bx, by);
      if (kCabin.has(bx, by))
        r.draw(baked(mArt, r, "cabinshell", 64, 64, 0, 0, paintCabinShell), x, y);
      else if (kMesa.has(bx, by))
      {
        const int v = (h >> 8) % 10u < 7u ? 0 : ((h >> 8) % 10u < 9u ? 1 : 2);
        r.draw(baked(mArt, r, "redrock" + std::to_string(v), 64, 64, 0, 0, [v](cairo_t* cr) { paintRedRock(cr, v); }), x, y, o);
        if (!solid(bx, by - 1))
          r.draw(baked(mArt, r, "redlip", 64, 24, 0, 6, paintRedLip), x, y);
      }
      else if (kTower.has(bx, by))
      {
        const int v = (by - kTower.y0) % 6 == 2 ? 2 : ((bx - kTower.x0) % 3 == 1 && (by - kTower.y0) % 3 == 1 ? 1 : 0);
        r.draw(baked(mArt, r, "tower" + std::to_string(v), 64, 64, 0, 0, [v](cairo_t* cr) { paintTowerBlock(cr, v); }), x, y, o);
      }
      else if (kBunker.has(bx, by))
        r.draw(baked(mArt, r, by == kBunker.y0 ? "concrete1" : "concrete0", 64, 64, 0, 0,
                 [roof = by == kBunker.y0](cairo_t* cr) { paintConcrete(cr, roof); }),
          x, y, o);
      else if (kFooting.has(bx, by))
      {
        r.draw(baked(mArt, r, !solid(bx, by - 1) ? "concrete1" : "concrete0", 64, 64, 0, 0,
                 [roof = !solid(bx, by - 1)](cairo_t* cr) { paintConcrete(cr, roof); }),
          x, y, o);
        if (kD4Struts.has(bx, by))
        {
          // D4's fixed struts: white steel over the concrete.
          DrawOpts so;
          so.alpha = 0.9f;
          r.draw(baked(mArt, r, "strut", 64, 64, 0, 0, paintStrut), x, y, so);
        }
      }
    }
  // The survey truck.
  {
    const float x = float(kTruck.x0) * kTilePx - camX, y = float(kTruck.y0) * kTilePx - camY;
    if (!offScreen(x - 20.0f, y - 30.0f, 360.0f, 170.0f))
    {
      // The ground under it, shaded as the surface (not as deep rock).
      for (int bx = kTruck.x0; bx <= kTruck.x1; ++bx)
        for (int by = kTruck.y1 + 1, d = 0; by <= kTruck.y1 + 3; ++by, ++d)
        {
          static constexpr int kShade[3] = {255, 210, 175};
          const auto h = (hash2(bx, by) >> 8) % 10u;
          DrawOpts o;
          o.tint = rgb(kShade[d], kShade[d], kShade[d]);
          const float gx = float(bx) * kTilePx - camX, gy = float(by) * kTilePx - camY;
          r.draw(mArt.solid[h < 7u ? 0u : (h < 9u ? 1u : 2u)], gx, gy, o);
          if (d == 0)
            r.draw(mArt.solidTop, gx, gy);
        }
      r.draw(baked(mArt, r, "truck", 360, 170, 20, 30, paintTruck), x, y);
    }
  }
  // The hangar door with the Halcyon target painted on it, or the open
  // hangar and the target board standing inside.
  for (const auto& b : mBreakables)
  {
    if (b.look != 7 || b.x0 != kMesa.x1)
      continue;
    const float x = float(b.x0) * kTilePx - camX, y = float(b.y0) * kTilePx - camY;
    const float h = float(b.y1 - b.y0 + 1) * kTilePx;
    if (offScreen(x - 200.0f, y, 300.0f, h))
      continue;
    if (!b.broken)
    {
      DrawOpts o;
      o.scale = h / 256.0f;
      r.draw(baked(mArt, r, "shutter", 64, 256, 0, 0, paintShutter), x, y, o);
      if (s.dummyX >= 0)
      {
        DrawOpts t;
        t.scale = 0.5f;
        r.draw(baked(mArt, r, "target", 120, 120, 60, 60, paintTargetBoard), x + 32.0f, float(s.dummyY) * kCellPx - camY, t);
      }
    }
    else
    {
      // The frame left standing, scorched where the beam went in.
      r.fillRect(x, y - 6.0f, 64.0f, 10.0f, rgb(70, 66, 62));
      drawGlow(r, mArt, x + 32.0f, float(s.dummyY) * kCellPx - camY, 60.0f, rgb(40, 20, 10), 0.4f);
    }
  }
  if (s.dummyX >= 0)
  {
    bool open = true;
    for (const auto& b : mBreakables)
      if (b.look == 7 && b.x0 == kMesa.x1 && !b.broken)
        open = false;
    if (open)
    {
      const float x = float(s.dummyX) * kCellPx - camX, y = float(s.dummyY) * kCellPx - camY;
      r.drawLine(x, y + 20.0f, x, y + 64.0f + 32.0f, 6.0f, rgb(90, 70, 50));
      DrawOpts t;
      t.scale = 0.55f;
      r.draw(baked(mArt, r, "target", 120, 120, 60, 60, paintTargetBoard), x, y, t);
    }
  }
  // Halcyon's crate.
  for (const auto& tr : mSignal.treasures)
  {
    if (tr.taken || tr.art != "halcyon_crate")
      continue;
    const float x = float(tr.x) * kTilePx - camX, y = float(tr.y) * kTilePx - camY;
    if (offScreen(x, y, kTilePx, kTilePx))
      continue;
    drawGlow(r, mArt, x + 32.0f, y + 34.0f, 70.0f + 6.0f * std::sin(float(frame) * 0.1f), rgb(120, 240, 240), 0.4f);
    r.draw(baked(mArt, r, "crate", 72, 64, 4, 0, paintCrate), x, y);
    if (frame % 90 < 16)
    {
      const float k = std::sin(float(frame % 90) / 16.0f * float(kPi));
      const float sx = x + 14.0f + float(hash2(frame / 90, tr.x) % 40u), sy = y + 12.0f;
      r.drawLine(sx - 9.0f * k, sy, sx + 9.0f * k, sy, 2.0f, rgb(220, 255, 255), Blend::Add);
      r.drawLine(sx, sy - 9.0f * k, sx, sy + 9.0f * k, 2.0f, rgb(220, 255, 255), Blend::Add);
    }
  }
  // The headphones on the cabin wall, hissing.
  for (const auto& pr : mProps)
  {
    if (pr.kind != PropKind::BonusDoor || pr.dormant)
      continue;
    const float x = (float(pr.x) + float(pr.w) * 0.5f) * kCellPx - camX, y = float(pr.y) * kCellPx - camY - 40.0f;
    if (offScreen(x - 60.0f, y, 120.0f, 100.0f))
      continue;
    r.draw(baked(mArt, r, "headphones", 110, 90, 55, 0, paintHeadphones), x, y);
    for (int k = 0; k < 3; ++k)
    {
      const float u = std::fmod(float(frame) * 0.04f + float(k) / 3.0f, 1.0f);
      r.drawLine(x - 40.0f - u * 24.0f, y + 40.0f - u * 10.0f, x - 40.0f - u * 24.0f, y + 70.0f + u * 10.0f, 2.0f,
        rgba(255, 255, 255, int(120.0f * (1.0f - u))));
    }
  }
  // The minidishes in their poses, wobbling through a turn.
  for (const auto& m : s.minis)
  {
    const float x = (float(m.x) + 1.0f) * kCellPx - camX, y = float(m.y + 1) * kCellPx - camY;
    if (offScreen(x - 60.0f, y - 120.0f, 120.0f, 130.0f))
      continue;
    r.draw(baked(mArt, r, "minipost", 48, 100, 24, 100, paintMiniPost), x, y);
    const int prev = (m.pose + 2) % 3;
    const float u = 1.0f - float(m.turn) / 8.0f;
    float ang = (kPoseAngle[std::size_t(prev)] + (kPoseAngle[std::size_t(m.pose)] - kPoseAngle[std::size_t(prev)]) * smooth(u)) * 1.6f;
    if (m.turn > 0)
      ang += std::sin(float(frame) * 1.3f) * 0.08f * float(m.turn) / 8.0f;
    DrawOpts o;
    o.angle = ang * 57.2958f;
    r.draw(baked(mArt, r, "minibowl", 100, 70, 50, 40, paintMiniBowl), x, y - 70.0f, o);
    if (m.pose == 1)
      drawGlow(r, mArt, x + std::sin(ang) * 37.0f, y - 70.0f - std::cos(ang) * 37.0f, 14.0f, rgb(255, 220, 120), 0.5f);
  }
  // The cactus and its hat, popping up and spinning back on.
  if (s.cactusX >= 0)
  {
    const float x = (float(s.cactusX) + 1.0f) * kCellPx - camX, y = float(s.cactusY + 1) * kCellPx - camY;
    if (!offScreen(x - 100.0f, y - 300.0f, 200.0f, 300.0f))
    {
      r.draw(baked(mArt, r, "cactus", 170, 240, 85, 240, paintCactus), x, y);
      const float lift = s.hatUp > 0 ? float(std::min(s.hatUp, 20 - s.hatUp)) * 9.0f : 0.0f;
      DrawOpts o;
      o.angle = s.hatUp > 0 ? float(20 - s.hatUp) * 36.0f : 0.0f;
      o.scale = 0.7f;
      r.draw(baked(mArt, r, "hat", 96, 48, 48, 30, paintHat), x, y - 196.0f - lift, o);
    }
  }
}

// --- In front --------------------------------------------------------------------------------------------

void World::drawDishFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)alpha;
  const auto& s = mDish;
  const auto& p = mPlayer;
  auto robotFor = [&](std::size_t i) -> const Robot* {
    for (const auto& ro : s.robots)
      if (ro.enemy == int(i))
        return &ro;
    return nullptr;
  };
  // A jammed robot: static crawling over it, a confused glyph over its head.
  auto jammedFx = [&](float cx, float top, float w, float h, std::uint32_t seed) {
    for (int i = 0; i < 9; ++i)
    {
      const std::uint32_t hh = hash2(frame * 3 + i, int(seed));
      const float y = top + unit(hh) * h, x0 = cx - w * 0.5f + unit(hash2(int(hh), 1)) * w * 0.8f;
      const float len = 12.0f + unit(hash2(int(hh), 2)) * w * 0.4f;
      r.drawLine(x0, y, x0 + len, y, 2.0f, hh % 3u ? rgba(230, 230, 240, 150) : rgba(255, 220, 120, 170), Blend::Add);
    }
    const float bob = std::sin(float(frame) * 0.25f) * 5.0f;
    r.drawText("?", cx + std::sin(float(frame) * 0.17f) * 6.0f, top - 56.0f + bob, {36.0f, kJam, kInk, true}, Align::Center);
    for (int k = 0; k < 3; ++k)
    {
      const float a = float(frame) * 0.2f + float(k) * 2.094f;
      const float ox = cx + std::cos(a) * 34.0f, oy = top - 24.0f + std::sin(a) * 9.0f;
      drawGlow(r, mArt, ox, oy, 10.0f, kJam, 0.7f);
      r.fillRect(ox - 2.0f, oy - 2.0f, 4.0f, 4.0f, rgb(255, 255, 230));
    }
  };
  // Sand Sharks.
  for (const auto& k : s.sharks)
  {
    if (std::size_t(k.enemy) >= mEnemies.size())
      continue;
    const Enemy& e = mEnemies[std::size_t(k.enemy)];
    if (!e.alive)
      continue;
    const float w = float(e.w) * kCellPx;
    const float cx = (e.drawX + float(e.w) * 0.5f) * kCellPx - camX;
    const float surf = float(k.surface) * kCellPx - camY;
    if (offScreen(cx - 200.0f, surf - 200.0f, 400.0f, 260.0f))
      continue;
    const int move = e.x > e.prevX ? 1 : (e.x < e.prevX ? -1 : 0);
    if (k.state == 0 || k.state == 1)
    {
      const bool tell = k.state == 1;
      const int dir = move != 0 ? move : (e.dir < 0 ? -1 : 1);
      const Sprite& sp = styledEnemySprite(mArt, r, mTheme, "sand_shark", tell ? 3 : 2, (frame / 2) % 2, e.w, e.h);
      r.draw(sp.get(dir), cx, surf);
      if (!tell && move != 0)
      {
        // Sand sprayed off the fin's foot, a wake behind it.
        for (int i = 0; i < 8; ++i)
        {
          const std::uint32_t h = hash2(frame + i * 7, k.enemy);
          const float u = unit(h);
          const float sx = cx - float(dir) * (10.0f + u * 60.0f), sy = surf - 4.0f - unit(hash2(int(h), 3)) * 22.0f * (1.0f - u);
          r.fillRect(sx, sy, 4.0f, 4.0f, rgba(240, 210, 160, int(220.0f * (1.0f - u))));
        }
        r.drawLine(cx - float(dir) * 30.0f, surf + 2.0f, cx - float(dir) * 140.0f, surf + 4.0f, 3.0f, rgba(190, 140, 90, 120));
      }
      if (tell)
      {
        // The bulge: sand fountaining up round the stopped fin.
        for (int i = 0; i < 10; ++i)
        {
          const std::uint32_t h = hash2(frame / 2 + i * 11, k.enemy + 5);
          const float u = unit(hash2(int(h), 9));
          const float sx = cx + (unit(h) - 0.5f) * w * 0.8f, sy = surf - 10.0f - u * 40.0f;
          r.fillRect(sx, sy, 5.0f, 5.0f, rgba(244, 216, 168, int(230.0f * (1.0f - u))));
        }
      }
      continue;
    }
    // Breached: rising out of the sand, jaws wide in the bite, sinking back.
    const int t = k.t;
    const float rise = std::min(1.0f, float(t + 1) / 3.0f) * std::min(1.0f, float(kSharkBreach - t) / 4.0f);
    const bool bite = t >= 2 && t < 12;
    const int dir = p.x + 1 >= e.x + e.w / 2 ? 1 : -1;
    const float h = float(e.h) * kCellPx;
    const Sprite& sp = styledEnemySprite(mArt, r, mTheme, "sand_shark", bite ? 1 : 0, (frame / 3) % 2, e.w, e.h);
    const float y = surf + (1.0f - rise) * h * 0.9f;
    r.draw(sp.get(dir), cx, y);
    if (e.flash > 0 && (frame / 2) % 2 == 0)
    {
      DrawOpts wo;
      wo.blend = Blend::Add;
      wo.alpha = 0.6f;
      r.draw(sp.get(dir), cx, y, wo);
    }
    // The sand closing over what is still under (the sand sea's own tiles
    // drawn again over it), heaped up round its body.
    {
      const Texture& sea = baked(mArt, r, "sandsea", 64, 128, 0, 0, paintSandSea);
      const float wx0 = std::floor((cx + camX - w * 0.7f) / kTilePx) * kTilePx;
      for (float wx = wx0; wx < cx + camX + w * 0.7f; wx += kTilePx)
        r.draw(sea, wx - camX, surf);
      for (int i = 0; i < 9; ++i)
      {
        const float u = (float(i) + 0.5f) / 9.0f, mound = std::sin(u * float(kPi)) * 10.0f * rise;
        r.fillRect(cx - w * 0.55f + u * w * 1.1f - w * 0.07f, surf - mound, w * 0.14f, mound + 2.0f, rgba(236, 206, 158, 230));
      }
    }
    // The burst: sand flung up and out.
    if (t < 8)
      for (int i = 0; i < 16; ++i)
      {
        const std::uint32_t hh = hash2(i * 13, k.enemy + 17);
        const float a = float(kPi) * (0.1f + 0.8f * unit(hh));
        const float d = (20.0f + 120.0f * unit(hash2(int(hh), 2))) * (float(t) + alpha) / 8.0f;
        const float sx = cx + std::cos(a) * d * 1.3f, sy = surf - std::sin(a) * d + float(t * t) * 1.5f;
        r.fillRect(sx, sy, 5.0f, 5.0f, rgba(244, 214, 164, int(255.0f * (1.0f - float(t) / 8.0f))));
      }
    if (e.stun > 0)
      jammedFx(cx, y - h, w, h, std::uint32_t(k.enemy));
  }
  // Scorpion Mechs: aim lines turning, then locked, then the beam.
  for (std::size_t i = 0; i < mEnemies.size(); ++i)
  {
    const Enemy& e = mEnemies[i];
    if (!e.alive || e.kind != EnemyKind::ScorpionMech)
      continue;
    const Robot* rb = robotFor(i);
    const float w = float(e.w) * kCellPx, h = float(e.h) * kCellPx;
    const float cx = (e.drawX + float(e.w) * 0.5f) * kCellPx - camX, foot = (e.drawY + 1.0f) * kCellPx - camY;
    const bool firing = rb && !s.beams.empty() && e.cool > enemyDef(e.def).cooldown - kBeamFrames;
    if (offScreen(cx - 1100.0f, foot - 600.0f, 2200.0f, 1200.0f))
      continue;
    const int dir = e.dir < 0 ? -1 : 1;
    const int v = firing ? 2 : (rb && rb->aimState > 0 ? 1 : 0);
    const bool walking = rb && rb->aimState == 0 && e.x != e.prevX;
    const int f = walking || (rb && rb->aimState == 0) ? (e.x / 2) % 2 : 0;
    const Sprite& sp = styledEnemySprite(mArt, r, mTheme, "scorpion_mech", v, f, e.w, e.h);
    if (rb && rb->aimState > 0)
    {
      // The aim line, from the stinger toward its end until a wall.
      const float ox = cx, oy = foot - h + 16.0f;
      const float tx = (float(rb->ax) + 0.5f) * kCellPx - camX, ty = (float(rb->ay) + 0.5f) * kCellPx - camY;
      float dx = tx - ox, dy = ty - oy;
      const float len = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
      dx /= len;
      dy /= len;
      float ex = ox, ey = oy;
      for (int k = 0; k < kBeamLength * 2; ++k)
      {
        const float nx = ex + dx * kCellPx * 0.5f, ny = ey + dy * kCellPx * 0.5f;
        if (mMap.solid(int(std::floor((nx + camX) / kCellPx)), int(std::floor((ny + camY) / kCellPx))))
          break;
        ex = nx;
        ey = ny;
      }
      const bool locked = rb->aimState == 2;
      if (!locked || (frame / 2) % 2 == 0)
      {
        r.drawLine(ox, oy, ex, ey, locked ? 6.0f : 3.0f, rgba(255, 40, 30, locked ? 90 : 50), Blend::Add);
        r.drawLine(ox, oy, ex, ey, locked ? 2.0f : 1.2f, rgba(255, 90, 80, locked ? 240 : 170));
      }
      // The dot where it lands, and the lock's brackets.
      drawGlow(r, mArt, ex, ey, locked ? 22.0f : 14.0f, rgb(255, 50, 40), locked ? 0.8f : 0.5f);
      if (locked)
        for (int k = 0; k < 4; ++k)
        {
          const float a = float(k) * float(kPi) * 0.5f + float(kPi) * 0.25f;
          const float bx = ex + std::cos(a) * 20.0f, by = ey + std::sin(a) * 20.0f;
          r.drawLine(bx, by, bx - std::cos(a) * 8.0f, by, 2.0f, rgb(255, 80, 70));
          r.drawLine(bx, by, bx, by - std::sin(a) * 8.0f, 2.0f, rgb(255, 80, 70));
        }
    }
    r.draw(sp.get(dir), cx, foot);
    if (e.flash > 0 && (frame / 2) % 2 == 0)
    {
      DrawOpts wo;
      wo.blend = Blend::Add;
      wo.alpha = 0.55f;
      r.draw(sp.get(dir), cx, foot, wo);
    }
    if (rb && rb->aimState > 0)
      drawGlow(r, mArt, cx, foot - h + 20.0f, rb->aimState == 2 ? 34.0f : 22.0f, rgb(255, 50, 40), 0.6f);
    if (rb && rb->jam > 0)
      jammedFx(cx, foot - h, w, h, std::uint32_t(i));
  }
  // Beams: a hard red laser with a white core, sparks where it ends.
  for (const auto& b : s.beams)
  {
    const float x0 = (float(b.x0) + 0.5f) * kCellPx - camX, y0 = (float(b.y0) + 0.5f) * kCellPx - camY;
    const float x1 = (float(b.x1) + 0.5f) * kCellPx - camX, y1 = (float(b.y1) + 0.5f) * kCellPx - camY;
    const float f = float(b.frames) / float(kBeamFrames);
    r.drawLine(x0, y0, x1, y1, 22.0f * f, rgba(255, 40, 30, 90), Blend::Add);
    r.drawLine(x0, y0, x1, y1, 9.0f * f + 2.0f, rgba(255, 90, 70, 230), Blend::Add);
    r.drawLine(x0, y0, x1, y1, 3.0f, rgba(255, 245, 235, 255));
    drawGlow(r, mArt, x0, y0, 40.0f, rgb(255, 120, 100), 0.8f * f);
    drawGlow(r, mArt, x1, y1, 50.0f, rgb(255, 120, 90), 0.9f * f);
    for (int k = 0; k < 8; ++k)
    {
      const std::uint32_t h = hash2(frame * 9 + k, b.x1 + b.y1);
      const float a = unit(h) * 2.0f * float(kPi), d = 10.0f + unit(hash2(int(h), 1)) * 40.0f;
      r.fillRect(x1 + std::cos(a) * d, y1 + std::sin(a) * d, 4.0f, 4.0f, rgb(255, 220, 160), Blend::Add);
    }
  }
  // Vulture Drones, their shadows on the ground under them.
  for (std::size_t i = 0; i < mEnemies.size(); ++i)
  {
    const Enemy& e = mEnemies[i];
    if (!e.alive || e.kind != EnemyKind::VultureDrone)
      continue;
    const Robot* rb = robotFor(i);
    const float w = float(e.w) * kCellPx, h = float(e.h) * kCellPx;
    const float cx = (e.drawX + float(e.w) * 0.5f) * kCellPx - camX, foot = (e.drawY + 1.0f) * kCellPx - camY;
    if (offScreen(cx - 150.0f, foot - 150.0f, 300.0f, 600.0f))
      continue;
    {
      const float gy = groundBelow(*this, cx + camX, foot + camY, 16) - camY;
      const float high = std::clamp((gy - foot) / 500.0f, 0.0f, 1.0f);
      r.fillRect(cx - w * (0.3f + 0.15f * high), gy - 6.0f, w * (0.6f + 0.3f * high), 8.0f, rgba(60, 30, 10, int(90.0f * (1.0f - high * 0.6f))));
    }
    const int dir = e.dir < 0 ? -1 : 1;
    const int v = (e.cool > 8 ? 1 : 0) + (e.carrier ? 2 : 0);
    DrawOpts o;
    o.angle = float(e.x - e.prevX) * float(dir) * 6.0f + std::sin(float(frame) * 0.07f + float(i)) * 3.0f;
    const float bob = std::sin(float(frame) * 0.1f + float(i)) * 3.0f;
    const Sprite& sp = styledEnemySprite(mArt, r, mTheme, "vulture_drone", v, frame % 2, e.w, e.h);
    r.draw(sp.get(dir), cx, foot + bob, o);
    if (e.flash > 0 && (frame / 2) % 2 == 0)
    {
      DrawOpts wo = o;
      wo.blend = Blend::Add;
      wo.alpha = 0.55f;
      r.draw(sp.get(dir), cx, foot + bob, wo);
    }
    if (rb && rb->jam > 0)
      jammedFx(cx, foot - h + bob, w, h, std::uint32_t(i));
  }
  // Scrap falling, turning, its shadow tightening on the ground below.
  for (const auto& sc : s.scraps)
  {
    const float x = (float(sc.x) + 1.0f) * kCellPx - camX, y = (float(sc.y) + 2.0f) * kCellPx - camY;
    const float gy = (float(sc.groundY) + 2.0f) * kCellPx - camY;
    if (offScreen(x - 60.0f, y - 60.0f, 120.0f, gy - y + 120.0f))
      continue;
    const float left = std::clamp((gy - y) / 400.0f, 0.0f, 1.0f);
    const float sw = 40.0f + 90.0f * left;
    const int a = int(150.0f * (1.0f - left * 0.75f));
    r.fillRect(x - sw * 0.5f, gy - 5.0f, sw, 7.0f, rgba(70, 30, 10, a));
    r.fillRect(x - sw * 0.3f, gy - 4.0f, sw * 0.6f, 5.0f, rgba(70, 30, 10, a));
    if (sc.carrier)
      drawGlow(r, mArt, x, y - 32.0f, 40.0f, rgb(130, 255, 100), 0.4f);
    const Sprite& sp = styledEnemySprite(mArt, r, mTheme, "scrap", sc.carrier ? 1 : 0, (sc.fall + sc.x) % 4, 2, 2);
    r.draw(sp.get(1), x, y);
    // A whistle of air streaming off it.
    r.drawLine(x - 10.0f, y - 70.0f, x - 10.0f, y - 70.0f - float(std::min(sc.fall, 6)) * 8.0f, 2.0f, rgba(255, 255, 255, 80));
    r.drawLine(x + 12.0f, y - 64.0f, x + 12.0f, y - 64.0f - float(std::min(sc.fall, 6)) * 6.0f, 2.0f, rgba(255, 255, 255, 60));
  }
  // The Jammer's cone: rings of noise rolling out, static speckling it.
  for (const auto& c : s.cones)
  {
    const float mx = float(c.x) * kCellPx - camX + (c.dir < 0 ? kCellPx : 0.0f), my = (float(c.y) + 0.5f) * kCellPx - camY;
    const float len = float(kJamLength) * kCellPx, half = float(kJamHeight) * kCellPx * 0.5f;
    const float life = float(c.life) / float(kJamLife);
    const float spread = std::atan2(half, len);
    for (int k = 0; k < 4; ++k)
    {
      const float u = std::fmod((1.0f - life) * 1.4f + float(k) * 0.25f, 1.0f);
      const float rad = 20.0f + u * (len - 20.0f);
      const int a = int(220.0f * life * (1.0f - u * 0.6f));
      float px = 0.0f, py = 0.0f;
      for (int j = 0; j <= 8; ++j)
      {
        const float th = -spread * 1.2f + 2.4f * spread * float(j) / 8.0f;
        const float x = mx + float(c.dir) * std::cos(th) * rad, y = my + std::sin(th) * rad * 1.35f;
        if (j > 0)
          r.drawLine(px, py, x, y, 3.0f, withAlpha(kJam, a), Blend::Add);
        px = x;
        py = y;
      }
    }
    for (int k = 0; k < 24; ++k)
    {
      const std::uint32_t h = hash2(frame * 5 + k, c.x + c.y);
      const float d = unit(h) * len, th = (unit(hash2(int(h), 1)) - 0.5f) * 2.0f * spread;
      r.fillRect(mx + float(c.dir) * std::cos(th) * d, my + std::sin(th) * d * 1.35f, 3.0f, 3.0f,
        h % 3u ? rgba(255, 255, 255, int(200.0f * life)) : withAlpha(kJam, int(220.0f * life)), Blend::Add);
    }
    drawGlow(r, mArt, mx, my, 30.0f, kJam, 0.6f * life);
  }
  // The heat: shimmer rising off the sand, stronger in Heatstroke.
  {
    const bool hot = s.swapLr;
    const int n = hot ? 26 : 12;
    for (int i = 0; i < n; ++i)
    {
      const std::uint32_t h = hash2(i, 3301);
      const float x = std::fmod(float(h % 1600u) - camX * 0.6f + 16000.0f, 1600.0f) - 160.0f;
      const float u = std::fmod(float(frame) * 0.008f + unit(hash2(int(h), 1)), 1.0f);
      const float y = float(kScreenH) * (0.85f - 0.55f * u);
      const int a = int((hot ? 34.0f : 18.0f) * std::sin(u * float(kPi)));
      float px = x, py = y;
      for (int j = 1; j <= 6; ++j)
      {
        const float xx = x + float(j) * 26.0f, yy = y + std::sin(float(frame) * 0.2f + float(j) * 1.3f + float(i)) * 3.0f;
        r.drawLine(px, py, xx, yy, 2.0f, rgba(255, 250, 235, a), Blend::Add);
        px = xx;
        py = yy;
      }
    }
    if (hot)
    {
      // A hot cast over everything, swelling as a swap nears.
      const float warn = s.swapT >= kSwapEvery - kSwapWobble ? float(s.swapT - (kSwapEvery - kSwapWobble)) / float(kSwapWobble) : 0.0f;
      r.fillRect(0.0f, 0.0f, float(kScreenW), float(kScreenH), rgba(255, 140, 60, int(18.0f + 40.0f * warn)), Blend::Add);
    }
  }
}

void World::drawDishHud(Renderer& r, int frame) const
{
  const auto& s = mDish;
  if (!s.swapLr)
    return;
  const Texture& sign = baked(mArt, r, "swapsign", 140, 140, 70, 70, paintSwapSign);
  const bool warn = s.swapT >= kSwapEvery - kSwapWobble;
  if (warn)
  {
    // The sign blinks big in the middle as the swap comes.
    if ((frame / 4) % 2 == 0)
    {
      DrawOpts o;
      o.scale = 1.0f + 0.08f * std::sin(float(frame) * 0.6f);
      r.draw(sign, float(kScreenW) * 0.5f, 190.0f, o);
      drawGlow(r, mArt, float(kScreenW) * 0.5f, 190.0f, 120.0f, rgb(255, 160, 60), 0.35f);
    }
    r.drawText("LEFT <> RIGHT", float(kScreenW) * 0.5f, 268.0f, {26.0f, rgb(255, 210, 120), kInk, true}, Align::Center);
  }
  else if (s.swapped)
  {
    // Swapped: a small sign stays lit.
    DrawOpts o;
    o.scale = 0.42f;
    r.draw(sign, float(kScreenW) * 0.5f, 120.0f, o);
    r.drawText("SWAPPED", float(kScreenW) * 0.5f, 152.0f, {16.0f, rgb(255, 200, 120), kInk}, Align::Center);
  }
}

} // namespace gr
