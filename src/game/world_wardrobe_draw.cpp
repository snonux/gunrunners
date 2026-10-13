// Level 37, Wardrobe: drawing. See world_wardrobe.cpp for the logic.
//
// Behind the tiles (drawWardrobeSet, wardrobe_draw.hpp): each room's
// furniture against the wall. The fitting room's long mirrors, coat stand
// and chaise; the costume racks' long rails of costumes and the hat shelf
// (the SIZE 42 jacket among them); the makeup hall's bulb-ringed mirrors
// under their gilt frame tops and its counter of wig heads; the sewing
// room's machines, fabric bolts, thread rack, dress form, the pulley beam
// and the needle pit's walls; the archive's tall shelves and the vault's
// steel walls with its round door; the hanging room signs and the battens
// the ceiling needles hang from. In the Totem bonus, footlights and
// painted flats.
//
// Behind the runner (drawWardrobeBack): the changing booths (cabinet,
// curtain, the runner's name on the sign; the OUT OF ORDER booth, the
// stage-door booth and the vault's triple booth with static in its
// curtain), heavy plates, the cutting tables on their chains, spring
// racks, shutters, the vault doors and the push rack, the Gorgazilla suit
// on its rack, the bulb mirror, and the Totem's other runners. The runner
// (drawWardrobeRunner, from World::drawPlayer): in the suit, on the stool,
// or as a cartoon. In front (drawWardrobeFront): decoys, Mannequins,
// Powder Puffers and their clouds, Rack Runners, the fog inside a cloud,
// the costume curtain over the secret booth, the booth curtain drawn shut,
// the mustache and the powder puff. The HUD (drawWardrobeHud): each
// runner's hearts.

#include "game/world.hpp"

#include "assets/art.hpp"
#include "assets/enemy_art.hpp"
#include "base/math.hpp"
#include "game/wardrobe_draw.hpp"
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
constexpr Color kInk = rgb(26, 12, 16);
constexpr Color kWood = rgb(104, 50, 34);
constexpr Color kWoodHi = rgb(156, 84, 52);
constexpr Color kWoodLo = rgb(54, 22, 16);
constexpr Color kBrass = rgb(214, 170, 80);
constexpr Color kBrassHi = rgb(255, 232, 150);
constexpr Color kBrassLo = rgb(124, 84, 30);
constexpr Color kVelvet = rgb(150, 20, 40);
constexpr Color kVelvetHi = rgb(214, 62, 76);
constexpr Color kVelvetLo = rgb(66, 6, 18);
constexpr Color kCream = rgb(236, 220, 192);
constexpr Color kSteel = rgb(150, 154, 166);
constexpr Color kSteelLo = rgb(70, 72, 84);
constexpr Color kFog = rgb(255, 226, 238);
constexpr Color kSickFog = rgb(170, 236, 130);

float lerpCells(int prev, int cur, float alpha) { return (float(prev) + float(cur - prev) * alpha) * kCellPx; }

// Each runner's own colour (the booth signs, the HUD).
Color runnerTone(int runner)
{
  switch (runner)
  {
    case 1:
      return rgb(232, 120, 48); // Rocco
    case 2:
      return rgb(150, 100, 236); // Nova
    default:
      return rgb(56, 156, 236); // Dash
  }
}

const char* runnerWord(int index) { return index == 1 ? "ROCCO" : index == 2 ? "NOVA" : "DASH"; }

bool offScreen(float x, float y, float w, float h, float m = 96.0f)
{
  return x > float(kScreenW) + m || x + w < -m || y > float(kScreenH) + m || y + h < -m;
}

float unit(std::uint32_t h) { return float(h % 10000u) / 10000.0f; }

const Sprite& bakedSprite(const Art& art, const Renderer& r, const std::string& key, int w, int h, float ax, float ay,
  const std::function<void(cairo_t*)>& paint)
{
  const std::string id = "~wardrobe/" + key;
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

// Lettering that shrinks to fit `maxW`.
void fitLettering(cairo_t* cr, const char* text, double cx, double baseline, double size, double maxW, Color c)
{
  selectGameFont(cr);
  cairo_set_font_size(cr, size);
  cairo_text_extents_t e;
  cairo_text_extents(cr, text, &e);
  if (e.width > maxW)
    size *= maxW / e.width;
  lettering(cr, text, cx, baseline, size, c);
}

// A panel of mahogany with a gilt bead, (x, y, w, h).
void woodPanel(cairo_t* cr, double x, double y, double w, double h, bool bead = true)
{
  cairo_rectangle(cr, x, y, w, h);
  lgrad(cr, x, 0, x + w, 0, {{0.0, kWoodLo}, {0.25, kWoodHi}, {0.6, kWood}, {1.0, kWoodLo}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  if (bead && w > 12 && h > 12)
  {
    cairo_rectangle(cr, x + 4, y + 4, w - 8, h - 8);
    stroke(cr, kBrass, 1.2, 0.7);
  }
}

// A brass bulb, lit.
void bulb(cairo_t* cr, double x, double y, double rad)
{
  glow(cr, x, y, rad * 3.0, rgb(255, 214, 140), 0.5);
  circle(cr, x, y, rad + 1.4);
  setRgba(cr, kBrassLo, 1.0);
  cairo_fill(cr);
  cairo_pattern_t* g = cairo_pattern_create_radial(x - rad * 0.3, y - rad * 0.3, 0.5, x, y, rad);
  stop(g, 0.0, rgb(255, 255, 240));
  stop(g, 0.6, rgb(255, 236, 170));
  stop(g, 1.0, rgb(250, 190, 90));
  circle(cr, x, y, rad);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
}

// A garment on a hanger hanging from (x, y) on a rail: 0 a long coat, 1 a
// gown, 2 a cape, 3 a doublet.
void garment(cairo_t* cr, double x, double y, int kind, Color c, double len)
{
  cairo_move_to(cr, x, y - 2);
  cairo_curve_to(cr, x + 5, y - 2, x + 4, y - 9, x, y - 8);
  stroke(cr, rgb(70, 60, 50), 1.6);
  cairo_move_to(cr, x - 15, y + 8);
  cairo_line_to(cr, x, y);
  cairo_line_to(cr, x + 15, y + 8);
  stroke(cr, kWoodLo, 2.4);
  const Color lo = lerpColor(c, rgb(0, 0, 0), 0.45f), hi = lerpColor(c, rgb(255, 255, 255), 0.18f);
  switch (kind % 4)
  {
    case 0:
      cairo_move_to(cr, x - 15, y + 7);
      cairo_line_to(cr, x + 15, y + 7);
      cairo_line_to(cr, x + 19, y + len);
      cairo_line_to(cr, x - 19, y + len);
      cairo_close_path(cr);
      break;
    case 1:
      cairo_move_to(cr, x - 9, y + 7);
      cairo_line_to(cr, x + 9, y + 7);
      cairo_curve_to(cr, x + 8, y + len * 0.4, x + 25, y + len * 0.8, x + 27, y + len);
      cairo_line_to(cr, x - 27, y + len);
      cairo_curve_to(cr, x - 25, y + len * 0.8, x - 8, y + len * 0.4, x - 9, y + 7);
      cairo_close_path(cr);
      break;
    case 2:
      cairo_move_to(cr, x - 6, y + 6);
      cairo_line_to(cr, x + 6, y + 6);
      cairo_line_to(cr, x + 24, y + len * 0.8);
      cairo_line_to(cr, x - 24, y + len * 0.8);
      cairo_close_path(cr);
      break;
    default:
      cairo_move_to(cr, x - 14, y + 7);
      cairo_line_to(cr, x + 14, y + 7);
      cairo_line_to(cr, x + 12, y + len * 0.55);
      cairo_line_to(cr, x - 12, y + len * 0.55);
      cairo_close_path(cr);
      cairo_new_sub_path(cr);
      cairo_arc(cr, x - 16, y + 15, 7.5, 0, 2 * kPi);
      cairo_new_sub_path(cr);
      cairo_arc(cr, x + 16, y + 15, 7.5, 0, 2 * kPi);
      break;
  }
  lgrad(cr, x - 26, 0, x + 26, 0, {{0.0, lo}, {0.35, hi}, {0.6, c}, {1.0, lo}}, 1.0, true);
  stroke(cr, kInk, 1.4, 0.8);
  cairo_move_to(cr, x, y + 9);
  cairo_line_to(cr, x, y + len * 0.5);
  stroke(cr, lo, 1.2, 0.7);
  if (kind % 4 == 0 || kind % 4 == 3)
    for (int b = 0; b < 3; ++b)
    {
      circle(cr, x + 3, y + 18 + b * 12, 1.6);
      setRgba(cr, kBrassHi, 0.9);
      cairo_fill(cr);
    }
}

constexpr Color kCloth[10] = {rgb(170, 30, 50), rgb(40, 70, 140), rgb(226, 176, 60), rgb(70, 120, 70), rgb(120, 60, 140),
  rgb(232, 222, 204), rgb(40, 40, 52), rgb(200, 96, 46), rgb(90, 160, 180), rgb(240, 140, 170)};

// --- Painters: the booths ----------------------------------------------------------------------------

// A changing booth's cabinet (160 x 248; the booth's 128 x 192 at x 16,
// its feet on y 240): mahogany posts and a cornice, the dark crimson inside
// with an oval mirror and a coat hook, a brass curtain rail, and the sign
// on top. kind 0-2 a runner's booth (the name in their colour), 3 OUT OF
// ORDER, 4 the stage door, 5 the vault's triple booth.
void paintBooth(cairo_t* cr, int kind)
{
  const bool ooo = kind == 3, stage = kind == 4;
  // The inside.
  cairo_rectangle(cr, 28, 60, 104, 178);
  lgrad(cr, 0, 60, 0, 238,
    {{0.0, ooo ? rgb(40, 34, 40) : rgb(50, 8, 18)}, {0.5, ooo ? rgb(60, 54, 58) : rgb(90, 20, 34)}, {1.0, rgb(30, 6, 12)}});
  if (!ooo)
  {
    ellipse(cr, 80, 128, 16, 24);
    lgrad(cr, 64, 104, 96, 152, {{0.0, rgb(220, 236, 240)}, {0.5, rgb(140, 160, 176)}, {1.0, rgb(70, 80, 96)}}, 1.0, true);
    stroke(cr, kBrass, 3.0);
    cairo_move_to(cr, 70, 116);
    cairo_line_to(cr, 76, 108);
    stroke(cr, rgb(255, 255, 255), 2.0, 0.7);
    cairo_move_to(cr, 116, 84);
    cairo_curve_to(cr, 116, 92, 108, 92, 108, 86);
    stroke(cr, kBrass, 2.4);
  }
  else
  {
    // Cobwebs in the corner.
    for (int k = 0; k < 4; ++k)
    {
      cairo_move_to(cr, 30, 62);
      cairo_line_to(cr, 30 + 28 * std::cos(k * 0.45), 62 + 28 * std::sin(k * 0.45));
      stroke(cr, rgb(200, 200, 210), 0.8, 0.5);
    }
    for (const double rr : {10.0, 18.0, 26.0})
    {
      cairo_arc(cr, 30, 62, rr, 0, kPi * 0.5 * 0.9 * 1.5);
      stroke(cr, rgb(200, 200, 210), 0.7, 0.4);
    }
  }
  // Posts.
  for (const double x : {14.0, 130.0})
    woodPanel(cr, x, 52, 16, 190);
  // Plinth.
  cairo_rectangle(cr, 10, 232, 140, 10);
  lgrad(cr, 0, 232, 0, 242, {{0.0, kWoodHi}, {1.0, kWoodLo}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  // The cornice.
  cairo_rectangle(cr, 6, 44, 148, 16);
  lgrad(cr, 0, 44, 0, 60, {{0.0, kWoodHi}, {0.5, kWood}, {1.0, kWoodLo}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  cairo_rectangle(cr, 4, 42, 152, 4);
  lgrad(cr, 0, 42, 0, 46, {{0.0, kBrassHi}, {1.0, kBrassLo}});
  // The rail.
  cairo_move_to(cr, 28, 66);
  cairo_line_to(cr, 132, 66);
  stroke(cr, kBrassLo, 5.0);
  cairo_move_to(cr, 28, 65);
  cairo_line_to(cr, 132, 65);
  stroke(cr, kBrassHi, 1.6);
  // The sign.
  Color plate = kind <= 2 ? runnerTone(kind) : ooo ? rgb(110, 110, 120) : rgb(214, 170, 70);
  if (kind == 5)
    plate = rgb(90, 60, 140);
  roundRect(cr, 26, 6, 108, 38, 8);
  lgrad(cr, 0, 6, 0, 44, {{0.0, lerpColor(plate, rgb(255, 255, 255), 0.3f)}, {0.5, plate}, {1.0, lerpColor(plate, rgb(0, 0, 0), 0.4f)}},
    1.0, true);
  stroke(cr, kBrass, 3.0);
  roundRect(cr, 30, 10, 100, 30, 6);
  stroke(cr, kBrassHi, 1.0, 0.6);
  if (stage)
  {
    cairo_save(cr);
    cairo_translate(cr, 44, 25);
    cairo_move_to(cr, 0, -9);
    for (int k = 1; k < 10; ++k)
    {
      const double a = -kPi / 2 + k * kPi / 5, rr = k % 2 ? 3.8 : 9.0;
      cairo_line_to(cr, std::cos(a) * rr, std::sin(a) * rr);
    }
    cairo_close_path(cr);
    setRgba(cr, rgb(255, 250, 220), 1.0);
    cairo_fill(cr);
    cairo_restore(cr);
    fitLettering(cr, "STAGE", 90, 32, 18, 60, rgb(60, 20, 10));
  }
  else if (ooo)
    fitLettering(cr, "BOOTH 4", 80, 32, 17, 90, rgb(40, 40, 46));
  else if (kind == 5)
    fitLettering(cr, "ALL THREE", 80, 31, 16, 92, rgb(255, 236, 200));
  else
    fitLettering(cr, runnerWord(kind), 80, 32, 20, 90, rgb(255, 250, 236));
  if (stage)
  {
    // Bulbs up the posts.
    for (int k = 0; k < 6; ++k)
      for (const double x : {22.0, 138.0})
        bulb(cr, x, 70 + k * 30, 3.2);
  }
}

// A booth's curtain on its rail (108 x 176, its top-left on the rail):
// pleats gathered toward the left, closed 0.15 (bunched) .. 1 (drawn
// right across); `sway` -1..1 swings the hem. style 0 crimson velvet, 1
// a dingy mauve, 2 gold lamé.
void paintCurtain(cairo_t* cr, double closed, double sway, int style)
{
  const Color base = style == 1 ? rgb(110, 90, 110) : style == 2 ? rgb(220, 170, 70) : kVelvet;
  const Color hi = lerpColor(base, rgb(255, 255, 255), style == 2 ? 0.45f : 0.3f), lo = lerpColor(base, rgb(0, 0, 0), 0.55f);
  const double span = 10 + 94 * std::clamp(closed, 0.0, 1.0);
  const int pleats = 7;
  const double pw = span / pleats;
  for (int k = 0; k < pleats; ++k)
  {
    const double x0 = 2 + k * pw, x1 = x0 + pw + 1.5;
    const double hem = sway * (6 + k * 1.5);
    cairo_move_to(cr, x0, 4);
    cairo_line_to(cr, x1, 4);
    cairo_curve_to(cr, x1, 80, x1 + hem * 0.6, 140, x1 + hem, 172);
    cairo_curve_to(cr, x1 + hem - pw * 0.3, 175, x0 + hem + pw * 0.3, 169, x0 + hem, 172);
    cairo_curve_to(cr, x0 + hem * 0.6, 140, x0, 80, x0, 4);
    cairo_close_path(cr);
    lgrad(cr, x0, 0, x1, 0, {{0.0, lo}, {0.35, hi}, {0.6, base}, {1.0, lo}}, 1.0, true);
    stroke(cr, kInk, 0.8, 0.5);
  }
  // The hem's gold braid, the rings.
  for (int k = 0; k < pleats; ++k)
  {
    const double x0 = 2 + k * pw, hem = sway * (6 + k * 1.5);
    cairo_move_to(cr, x0 + hem, 170);
    cairo_line_to(cr, x0 + pw + hem, 170);
    stroke(cr, style == 2 ? rgb(255, 250, 220) : kBrass, 2.4);
    circle(cr, x0 + pw * 0.5, 3, 3.2);
    stroke(cr, kBrassHi, 1.4);
  }
}

// The OUT OF ORDER card hung across a booth (120 x 60, its middle at 60, 0
// where the string's knot is): a card on a string, tilted, scrawled.
void paintOutOfOrder(cairo_t* cr)
{
  cairo_move_to(cr, 20, 26);
  cairo_line_to(cr, 60, 2);
  cairo_line_to(cr, 100, 20);
  stroke(cr, rgb(220, 210, 190), 1.2);
  cairo_save(cr);
  cairo_translate(cr, 60, 36);
  cairo_rotate(cr, 0.12);
  cairo_rectangle(cr, -54, -16, 108, 34);
  lgrad(cr, 0, -16, 0, 18, {{0.0, rgb(255, 250, 230)}, {1.0, rgb(226, 214, 186)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  lettering(cr, "OUT OF", 0, -2, 12, rgb(200, 30, 40));
  lettering(cr, "ORDER", 0, 13, 14, rgb(200, 30, 40));
  cairo_restore(cr);
  // Tape across.
  cairo_move_to(cr, 8, 50);
  cairo_line_to(cr, 112, 10);
  stroke(cr, rgb(240, 200, 60), 6.0, 0.85);
}

// --- Painters: the machinery -------------------------------------------------------------------------

// A spring rack (200 x 180, the floor line at y 160, the middle at x 100):
// a crimson springboard flush with the floor, two chrome uprights on big
// coil springs, a top rail with a couple of costumes on it. `sq` 0..2 how
// far it is squashed.
void paintSpringRack(cairo_t* cr, int sq)
{
  const double drop = sq * 9.0;
  // The coils at each upright's foot.
  for (const double x : {40.0, 160.0})
  {
    const double top = 112 + drop, bottom = 160;
    const int turns = 6;
    for (int k = 0; k <= turns; ++k)
    {
      const double y = top + (bottom - top) * k / turns;
      cairo_save(cr);
      cairo_translate(cr, x, y);
      cairo_scale(cr, 13, 3.5);
      cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
      cairo_restore(cr);
      stroke(cr, kInk, 3.6);
      cairo_save(cr);
      cairo_translate(cr, x, y);
      cairo_scale(cr, 13, 3.5);
      cairo_arc(cr, 0, 0, 1, kPi * 0.05, kPi * 0.95);
      cairo_restore(cr);
      stroke(cr, rgb(230, 234, 244), 2.0);
    }
    // The upright.
    cairo_move_to(cr, x, top);
    cairo_line_to(cr, x, 46 + drop);
    stroke(cr, kInk, 8.0);
    cairo_move_to(cr, x, top);
    cairo_line_to(cr, x, 46 + drop);
    stroke(cr, rgb(214, 218, 228), 5.0);
  }
  // Costumes on the top rail.
  garment(cr, 80, 52 + drop, 0, rgb(46, 100, 170), 54);
  garment(cr, 118, 52 + drop, 2, rgb(200, 40, 60), 50);
  cairo_move_to(cr, 32, 48 + drop);
  cairo_line_to(cr, 168, 48 + drop);
  stroke(cr, kInk, 8.0);
  cairo_move_to(cr, 32, 48 + drop);
  cairo_line_to(cr, 168, 48 + drop);
  stroke(cr, rgb(230, 234, 244), 5.0);
  for (const double x : {32.0, 168.0})
  {
    circle(cr, x, 48 + drop, 5);
    setRgba(cr, kBrass, 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.4);
  }
  // A "BOING" tag.
  roundRect(cr, 128, 74 + drop, 40, 16, 3);
  setRgba(cr, rgb(255, 230, 120), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.2);
  lettering(cr, "BOING", 148, 86 + drop, 9, rgb(150, 30, 30));
  // The springboard.
  roundRect(cr, 34, 156 + sq * 2.0, 132, 12, 4);
  lgrad(cr, 0, 156, 0, 168, {{0.0, kVelvetHi}, {0.5, kVelvet}, {1.0, kVelvetLo}}, 1.0, true);
  stroke(cr, kBrass, 2.0);
  for (double x = 44; x < 160; x += 12)
  {
    cairo_move_to(cr, x, 159 + sq * 2.0);
    cairo_line_to(cr, x + 4, 165 + sq * 2.0);
    stroke(cr, kVelvetLo, 1.2, 0.6);
  }
}

// A heavy plate (`wpx` wide + 16, 40 tall; the floor line at y 24): a
// brass plate in a recess, stamped with a kettlebell and its weight; down,
// flush and its lamp green.
void paintPlate(cairo_t* cr, int wpx, bool down, int weight)
{
  const double x0 = 8, x1 = 8 + wpx;
  cairo_rectangle(cr, x0 - 4, 22, wpx + 8, 10);
  setRgba(cr, rgb(30, 14, 12), 1.0);
  cairo_fill(cr);
  const double top = down ? 22 : 13;
  cairo_rectangle(cr, x0, top, wpx, 9);
  lgrad(cr, 0, top, 0, top + 9, {{0.0, kBrassHi}, {0.4, kBrass}, {1.0, kBrassLo}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  if (!down)
  {
    cairo_rectangle(cr, x0, top + 9, wpx, 22 - top - 9 + 1);
    setRgba(cr, kBrassLo, 1.0);
    cairo_fill(cr);
  }
  // The stamp: a kettlebell and the weight.
  const double cx = (x0 + x1) * 0.5 - (wpx > 70 ? 10 : 6);
  circle(cr, cx, top + 5.5, 3.2);
  setRgba(cr, kBrassLo, 1.0);
  cairo_fill(cr);
  cairo_arc(cr, cx, top + 2, 2.2, kPi, 2 * kPi);
  stroke(cr, kBrassLo, 1.2);
  char num[8];
  std::snprintf(num, sizeof num, "%d", weight);
  lettering(cr, num, cx + (wpx > 70 ? 14 : 10), top + 8, 9, kBrassLo);
  // The lamp.
  circle(cr, x1 - 6, top + 4.5, 2.6);
  setRgba(cr, down ? rgb(120, 255, 140) : rgb(255, 80, 60), 1.0);
  cairo_fill(cr);
}

// A cutting table (n blocks wide, 80 tall; its top surface at y 12): a
// thick maple top with a tape measure along its front edge, a sheet of
// fabric spread on it with a paper pattern chalked on, shears lying on it,
// a frame under it with the chains' eyes at both ends.
void paintTable(cairo_t* cr, int blocks, int seed)
{
  const double W = blocks * 64.0;
  // The frame under it.
  cairo_rectangle(cr, 8, 30, W - 16, 10);
  setRgba(cr, kWoodLo, 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.4);
  for (double x = 20; x < W - 10; x += 60)
  {
    cairo_move_to(cr, x, 40);
    cairo_line_to(cr, x + 30, 30);
    stroke(cr, kWood, 3.0);
  }
  // The top.
  cairo_rectangle(cr, 0, 12, W, 12);
  lgrad(cr, 0, 12, 0, 24, {{0.0, rgb(250, 220, 170)}, {0.5, rgb(214, 170, 110)}, {1.0, rgb(150, 104, 60)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  // The tape measure.
  cairo_rectangle(cr, 0, 24, W, 7);
  setRgba(cr, rgb(250, 214, 60), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.0);
  for (double x = 4; x < W; x += 6)
  {
    cairo_move_to(cr, x, 24);
    cairo_line_to(cr, x, std::fmod(x, 30.0) < 6 ? 30 : 27);
    stroke(cr, kInk, 0.8, 0.9);
  }
  // The fabric, its edge hanging over the front.
  Rng rng(std::uint32_t(37300 + seed));
  const Color cloth = kCloth[std::size_t(seed * 3 + 1) % 10u];
  cairo_move_to(cr, 14, 12);
  cairo_line_to(cr, W - 30, 10);
  cairo_curve_to(cr, W - 24, 14, W - 26, 22, W - 34, 30);
  cairo_curve_to(cr, W * 0.6, 26, W * 0.4, 32, 24, 26);
  cairo_close_path(cr);
  lgrad(cr, 0, 10, 0, 32, {{0.0, lerpColor(cloth, rgb(255, 255, 255), 0.25f)}, {1.0, lerpColor(cloth, rgb(0, 0, 0), 0.3f)}}, 1.0, true);
  stroke(cr, kInk, 1.0, 0.7);
  // The pattern paper and its chalk line.
  cairo_move_to(cr, W * 0.3, 11);
  cairo_line_to(cr, W * 0.55, 10.5);
  cairo_line_to(cr, W * 0.5, 14);
  cairo_line_to(cr, W * 0.28, 14.5);
  cairo_close_path(cr);
  setRgba(cr, rgb(240, 232, 200), 0.95);
  cairo_fill(cr);
  // Shears.
  const double sx = W * 0.72;
  cairo_move_to(cr, sx, 9);
  cairo_line_to(cr, sx + 28, 7);
  cairo_move_to(cr, sx, 11);
  cairo_line_to(cr, sx + 26, 11);
  stroke(cr, rgb(210, 214, 224), 2.4);
  circle(cr, sx - 4, 8, 3.5);
  circle(cr, sx - 4, 12.5, 3.5);
  stroke(cr, rgb(30, 30, 36), 2.0);
  // The chains' eyes.
  for (const double x : {14.0, W - 14})
  {
    circle(cr, x, 8, 5);
    stroke(cr, kSteelLo, 3.0);
  }
  (void)rng;
}

// The vault door (80 x 176; the door 64 x 160 at x 8, its foot on y 168):
// riveted steel, a wheel, a stencil. `down`: it goes down into the vault.
void paintVaultDoor(cairo_t* cr, bool down)
{
  cairo_rectangle(cr, 4, 4, 72, 168);
  setRgba(cr, rgb(40, 40, 48), 1.0);
  cairo_fill(cr);
  roundRect(cr, 8, 8, 64, 160, 6);
  lgrad(cr, 8, 0, 72, 0, {{0.0, kSteelLo}, {0.3, rgb(196, 200, 210)}, {0.7, kSteel}, {1.0, kSteelLo}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  for (double y = 16; y < 166; y += 18)
    for (const double x : {14.0, 66.0})
    {
      circle(cr, x, y, 1.8);
      setRgba(cr, rgb(230, 234, 240), 1.0);
      cairo_fill(cr);
    }
  // The wheel.
  circle(cr, 40, 80, 18);
  stroke(cr, kInk, 6.0);
  circle(cr, 40, 80, 18);
  stroke(cr, rgb(210, 180, 90), 3.6);
  for (int k = 0; k < 3; ++k)
  {
    const double a = k * kPi / 3;
    cairo_move_to(cr, 40 - std::cos(a) * 18, 80 - std::sin(a) * 18);
    cairo_line_to(cr, 40 + std::cos(a) * 18, 80 + std::sin(a) * 18);
    stroke(cr, rgb(210, 180, 90), 3.0);
  }
  circle(cr, 40, 80, 5);
  setRgba(cr, kBrassLo, 1.0);
  cairo_fill(cr);
  // The stencil.
  lettering(cr, down ? "VAULT" : "OUT", 40, 128, 12, rgb(250, 200, 50));
  cairo_move_to(cr, 32, down ? 136 : 150);
  cairo_line_to(cr, 48, down ? 136 : 150);
  cairo_line_to(cr, 40, down ? 150 : 136);
  cairo_close_path(cr);
  setRgba(cr, rgb(250, 200, 50), 1.0);
  cairo_fill(cr);
}

// The archive's push rack (144 x 208, its box's top-left at 8, 8; 128 x
// 192): a tall filing cabinet of drawers on casters, labelled.
void paintPushRack(cairo_t* cr)
{
  woodPanel(cr, 8, 8, 128, 178, false);
  cairo_rectangle(cr, 4, 4, 136, 10);
  lgrad(cr, 0, 4, 0, 14, {{0.0, kWoodHi}, {1.0, kWoodLo}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  for (int row = 0; row < 5; ++row)
    for (int col = 0; col < 2; ++col)
    {
      const double x = 16 + col * 58, y = 22 + row * 32;
      roundRect(cr, x, y, 54, 28, 3);
      lgrad(cr, 0, y, 0, y + 28, {{0.0, kWoodHi}, {1.0, kWood}}, 1.0, true);
      stroke(cr, kInk, 1.2);
      cairo_rectangle(cr, x + 15, y + 4, 24, 9);
      setRgba(cr, rgb(244, 236, 214), 1.0);
      cairo_fill_preserve(cr);
      stroke(cr, kBrass, 1.0);
      static const char* kLabels[10] = {"1971", "1978", "1983", "1986", "1990", "1994", "WIGS", "HATS", "CAPES", "MISC"};
      lettering(cr, kLabels[row * 2 + col], x + 27, y + 11.5, 6.5, rgb(40, 30, 30));
      cairo_move_to(cr, x + 19, y + 20);
      cairo_line_to(cr, x + 35, y + 20);
      stroke(cr, kBrass, 3.0);
    }
  roundRect(cr, 34, 186, 76, 10, 3);
  setRgba(cr, kWoodLo, 1.0);
  cairo_fill(cr);
  for (const double x : {22.0, 122.0})
  {
    circle(cr, x, 196, 7);
    setRgba(cr, rgb(30, 30, 34), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.2);
    circle(cr, x, 196, 2.4);
    setRgba(cr, kSteel, 1.0);
    cairo_fill(cr);
  }
}

// The Gorgazilla suit's rack (160 x 220, the floor at y 212, the middle at
// 80): chrome uprights on feet, a top rail, a "DO NOT TOUCH" tag.
void paintSuitRack(cairo_t* cr)
{
  for (const double x : {24.0, 136.0})
  {
    cairo_move_to(cr, x, 210);
    cairo_line_to(cr, x, 22);
    stroke(cr, kInk, 8.0);
    cairo_move_to(cr, x, 210);
    cairo_line_to(cr, x, 22);
    stroke(cr, rgb(214, 218, 228), 5.0);
    cairo_move_to(cr, x - 18, 210);
    cairo_line_to(cr, x + 18, 210);
    stroke(cr, kInk, 6.0);
  }
  cairo_move_to(cr, 16, 22);
  cairo_line_to(cr, 144, 22);
  stroke(cr, kInk, 8.0);
  cairo_move_to(cr, 16, 22);
  cairo_line_to(cr, 144, 22);
  stroke(cr, rgb(230, 234, 244), 5.0);
  roundRect(cr, 92, 140, 58, 34, 3);
  setRgba(cr, rgb(250, 244, 226), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.2);
  lettering(cr, "GORGAZILLA", 121, 153, 8, rgb(150, 30, 30));
  lettering(cr, "DO NOT", 121, 163, 7, rgb(40, 40, 40));
  lettering(cr, "TOUCH", 121, 171, 7, rgb(40, 40, 40));
}

// The bulb mirror (170 x 172, its feet on y 160, the middle at 85): a
// dressing table with cabriole legs and a drawer, a mirror on it ringed with
// bulb sockets (the bulbs are lit live), pots and brushes.
void paintVanity(cairo_t* cr)
{
  // The legs.
  for (const double x : {30.0, 140.0})
  {
    cairo_move_to(cr, x, 124);
    cairo_curve_to(cr, x + (x < 85 ? -8 : 8), 138, x + (x < 85 ? 6 : -6), 150, x + (x < 85 ? -4 : 4), 160);
    stroke(cr, kInk, 7.0);
    cairo_move_to(cr, x, 124);
    cairo_curve_to(cr, x + (x < 85 ? -8 : 8), 138, x + (x < 85 ? 6 : -6), 150, x + (x < 85 ? -4 : 4), 160);
    stroke(cr, kWoodHi, 4.0);
  }
  // The table.
  cairo_rectangle(cr, 18, 112, 134, 14);
  lgrad(cr, 0, 112, 0, 126, {{0.0, kWoodHi}, {1.0, kWoodLo}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  circle(cr, 85, 120, 2.6);
  setRgba(cr, kBrass, 1.0);
  cairo_fill(cr);
  // The mirror.
  cairo_move_to(cr, 32, 112);
  cairo_line_to(cr, 32, 34);
  cairo_curve_to(cr, 32, 10, 138, 10, 138, 34);
  cairo_line_to(cr, 138, 112);
  cairo_close_path(cr);
  lgrad(cr, 32, 0, 138, 0, {{0.0, kBrassLo}, {0.3, kBrassHi}, {0.7, kBrass}, {1.0, kBrassLo}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  cairo_move_to(cr, 44, 110);
  cairo_line_to(cr, 44, 38);
  cairo_curve_to(cr, 44, 22, 126, 22, 126, 38);
  cairo_line_to(cr, 126, 110);
  cairo_close_path(cr);
  lgrad(cr, 44, 22, 126, 110, {{0.0, rgb(230, 240, 246)}, {0.45, rgb(160, 176, 196)}, {1.0, rgb(90, 100, 120)}}, 1.0, true);
  stroke(cr, kInk, 1.2);
  for (const double off : {0.0, 18.0})
  {
    cairo_move_to(cr, 56 + off, 100);
    cairo_line_to(cr, 84 + off, 40);
    stroke(cr, rgb(255, 255, 255), 4.0, 0.25);
  }
  // Pots and a brush on the table.
  cairo_rectangle(cr, 26, 102, 12, 10);
  setRgba(cr, rgb(250, 190, 210), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.0);
  cairo_rectangle(cr, 132, 98, 10, 14);
  setRgba(cr, rgb(200, 30, 50), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.0);
  cairo_move_to(cr, 140, 110);
  cairo_line_to(cr, 156, 96);
  stroke(cr, kWoodLo, 2.4);
  ellipse(cr, 158, 94, 4, 3);
  setRgba(cr, rgb(250, 220, 200), 1.0);
  cairo_fill(cr);
}

// Where the bulb mirror's bulbs are (in its 170 x 172 texture).
constexpr std::array<std::array<float, 2>, 10> kVanityBulbs = {{{38, 104}, {38, 80}, {38, 56}, {48, 28}, {72, 16}, {98, 16},
  {122, 28}, {132, 56}, {132, 80}, {132, 104}}};

// A velvet stool (64 x 44, its feet on y 42).
void paintStool(cairo_t* cr)
{
  for (const double x : {14.0, 50.0})
  {
    cairo_move_to(cr, 32, 16);
    cairo_line_to(cr, x, 42);
    stroke(cr, kBrassLo, 3.0);
  }
  ellipse(cr, 32, 14, 26, 9);
  lgrad(cr, 0, 5, 0, 23, {{0.0, kVelvetHi}, {1.0, kVelvetLo}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  ellipse(cr, 32, 11, 18, 4);
  setRgba(cr, rgb(255, 160, 170), 0.35);
  cairo_fill(cr);
}

// A handlebar mustache (60 x 24, its middle at 30, 8).
void paintMustache(cairo_t* cr)
{
  for (const double s : {-1.0, 1.0})
  {
    cairo_move_to(cr, 30, 6);
    cairo_curve_to(cr, 30 + s * 6, 2, 30 + s * 16, 4, 30 + s * 20, 10);
    cairo_curve_to(cr, 30 + s * 24, 16, 30 + s * 28, 12, 30 + s * 27, 7);
    cairo_curve_to(cr, 30 + s * 29, 16, 30 + s * 20, 20, 30 + s * 14, 14);
    cairo_curve_to(cr, 30 + s * 9, 11, 30 + s * 4, 12, 30, 12);
    cairo_close_path(cr);
    lgrad(cr, 0, 2, 0, 20, {{0.0, rgb(90, 56, 36)}, {1.0, rgb(36, 20, 14)}}, 1.0, true);
    stroke(cr, kInk, 1.2);
  }
}

// --- Painters: the rooms -----------------------------------------------------------------------------

// A full-length mirror on the wall (100 x 260).
void paintLongMirror(cairo_t* cr)
{
  roundRect(cr, 4, 4, 92, 252, 40);
  lgrad(cr, 4, 0, 96, 0, {{0.0, kBrassLo}, {0.3, kBrassHi}, {0.7, kBrass}, {1.0, kBrassLo}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  roundRect(cr, 14, 14, 72, 232, 32);
  lgrad(cr, 14, 14, 86, 246, {{0.0, rgb(214, 226, 236)}, {0.5, rgb(150, 164, 184)}, {1.0, rgb(80, 90, 110)}}, 1.0, true);
  stroke(cr, kInk, 1.0);
  for (const double off : {0.0, 22.0})
  {
    cairo_move_to(cr, 24 + off, 200);
    cairo_line_to(cr, 60 + off, 60);
    stroke(cr, rgb(255, 255, 255), 6.0, 0.2);
  }
}

// A bentwood coat stand with hats and a scarf (120 x 270, its foot's
// middle at 60, 266).
void paintCoatStand(cairo_t* cr)
{
  for (const double s : {-1.0, 1.0})
  {
    cairo_move_to(cr, 60, 230);
    cairo_curve_to(cr, 60 + s * 20, 246, 60 + s * 34, 256, 60 + s * 40, 266);
    stroke(cr, kWoodLo, 6.0);
  }
  cairo_move_to(cr, 60, 266);
  cairo_line_to(cr, 60, 20);
  stroke(cr, kInk, 9.0);
  cairo_move_to(cr, 60, 266);
  cairo_line_to(cr, 60, 20);
  stroke(cr, kWoodHi, 6.0);
  for (const double s : {-1.0, 1.0})
  {
    cairo_move_to(cr, 60, 40);
    cairo_curve_to(cr, 60 + s * 20, 40, 60 + s * 30, 30, 60 + s * 28, 20);
    stroke(cr, kWoodHi, 4.0);
  }
  // A bowler on the left hook, a boater on the right.
  ellipse(cr, 32, 22, 18, 4);
  setRgba(cr, rgb(30, 26, 30), 1.0);
  cairo_fill(cr);
  cairo_arc(cr, 32, 20, 12, kPi, 2 * kPi);
  setRgba(cr, rgb(40, 34, 40), 1.0);
  cairo_fill(cr);
  ellipse(cr, 90, 22, 20, 4);
  setRgba(cr, rgb(230, 200, 120), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.0);
  cairo_rectangle(cr, 78, 10, 24, 12);
  setRgba(cr, rgb(236, 210, 130), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.0);
  cairo_rectangle(cr, 78, 15, 24, 4);
  setRgba(cr, rgb(170, 30, 50), 1.0);
  cairo_fill(cr);
  // A long striped scarf.
  cairo_move_to(cr, 56, 44);
  cairo_curve_to(cr, 40, 70, 46, 110, 38, 150);
  stroke(cr, kInk, 12.0);
  cairo_move_to(cr, 56, 44);
  cairo_curve_to(cr, 40, 70, 46, 110, 38, 150);
  stroke(cr, rgb(60, 110, 190), 9.0);
  for (double t = 0.15; t < 1.0; t += 0.18)
  {
    const double y = 44 + t * 106, x = 56 - t * 16;
    cairo_move_to(cr, x - 5, y);
    cairo_line_to(cr, x + 5, y + 2);
    stroke(cr, rgb(250, 240, 220), 3.0);
  }
}

// A velvet chaise longue (260 x 120, its feet on y 116).
void paintChaise(cairo_t* cr)
{
  for (const double x : {30.0, 230.0})
  {
    cairo_move_to(cr, x, 96);
    cairo_line_to(cr, x + (x < 100 ? -4 : 4), 116);
    stroke(cr, kWoodLo, 6.0);
  }
  cairo_move_to(cr, 10, 96);
  cairo_curve_to(cr, 0, 60, 10, 20, 40, 24);
  cairo_curve_to(cr, 60, 28, 50, 60, 70, 64);
  cairo_line_to(cr, 250, 64);
  cairo_curve_to(cr, 258, 70, 258, 90, 250, 98);
  cairo_line_to(cr, 14, 98);
  cairo_close_path(cr);
  lgrad(cr, 0, 20, 0, 98, {{0.0, kVelvetHi}, {0.5, kVelvet}, {1.0, kVelvetLo}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  for (double x = 80; x < 250; x += 30)
  {
    circle(cr, x, 76, 2.2);
    setRgba(cr, kBrass, 1.0);
    cairo_fill(cr);
  }
  cairo_move_to(cr, 14, 98);
  cairo_line_to(cr, 250, 98);
  stroke(cr, kBrass, 2.4);
}

// A long rack of costumes (W x 250, its rail at y 30, its feet on y 246).
void paintBigRack(cairo_t* cr, int W, int seed)
{
  Rng rng(std::uint32_t(37400 + seed));
  for (const double x : {10.0, double(W) - 10})
  {
    cairo_move_to(cr, x, 26);
    cairo_line_to(cr, x, 244);
    stroke(cr, kInk, 8.0);
    cairo_move_to(cr, x, 26);
    cairo_line_to(cr, x, 244);
    stroke(cr, rgb(200, 160, 80), 5.0);
    cairo_move_to(cr, x - 22, 244);
    cairo_line_to(cr, x + 22, 244);
    stroke(cr, kWoodLo, 6.0);
  }
  for (double gx = 28; gx < W - 20; gx += rng.range(22, 30))
    garment(cr, gx, 30, int(rng.next() % 4u), kCloth[rng.next() % 10u], rng.range(140, 200));
  cairo_move_to(cr, 2, 30);
  cairo_line_to(cr, W - 2, 30);
  stroke(cr, kInk, 8.0);
  cairo_move_to(cr, 2, 30);
  cairo_line_to(cr, W - 2, 30);
  stroke(cr, kBrass, 5.0);
  // Shoes lined up under it.
  for (double sx = 30; sx < W - 40; sx += rng.range(40, 80))
  {
    const Color c = kCloth[rng.next() % 10u];
    ellipse(cr, sx, 240, 12, 5);
    setRgba(cr, lerpColor(c, rgb(0, 0, 0), 0.3f), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.0);
    ellipse(cr, sx + 22, 240, 12, 5);
    setRgba(cr, lerpColor(c, rgb(0, 0, 0), 0.3f), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.0);
  }
}

// A wall shelf of hat boxes and wigs on stands (W x 180, its board at y 170).
void paintHatShelf(cairo_t* cr, int W, int seed)
{
  Rng rng(std::uint32_t(37500 + seed));
  double x = 10;
  while (x < W - 50)
  {
    if (rng.next() % 3u == 0)
    {
      // A wig on a stand: a polystyrene head under a big hairdo.
      cairo_move_to(cr, x + 20, 170);
      cairo_line_to(cr, x + 20, 128);
      stroke(cr, kWoodLo, 4.0);
      ellipse(cr, x + 20, 116, 12, 15);
      setRgba(cr, rgb(240, 236, 228), 1.0);
      cairo_fill_preserve(cr);
      stroke(cr, kInk, 1.2);
      const Color hair = rng.next() % 2u ? rgb(240, 200, 80) : rgb(200, 60, 40);
      ellipse(cr, x + 20, 98, 22, 20);
      setRgba(cr, hair, 1.0);
      cairo_fill_preserve(cr);
      stroke(cr, kInk, 1.2);
      for (int k = 0; k < 5; ++k)
      {
        circle(cr, x + 6 + k * 7, 106 + (k % 2) * 4, 6);
        setRgba(cr, lerpColor(hair, rgb(255, 255, 255), 0.2f), 1.0);
        cairo_fill(cr);
      }
      x += 46;
    }
    else
    {
      double y = 170;
      const int stack = 1 + int(rng.next() % 3u);
      for (int s = 0; s < stack; ++s)
      {
        const double bw = rng.range(44, 60), bh = rng.range(28, 38);
        const Color c = kCloth[rng.next() % 10u];
        cairo_rectangle(cr, x, y - bh, bw, bh);
        lgrad(cr, x, 0, x + bw, 0, {{0.0, lerpColor(c, rgb(0, 0, 0), 0.4f)}, {0.4, lerpColor(c, rgb(255, 255, 255), 0.15f)},
                                     {1.0, lerpColor(c, rgb(0, 0, 0), 0.5f)}}, 1.0, true);
        stroke(cr, kInk, 1.2);
        for (double sx = x + 6; sx < x + bw - 2; sx += 10)
        {
          cairo_rectangle(cr, sx, y - bh + 6, 4, bh - 6);
          setRgba(cr, rgb(255, 245, 225), 0.3);
          cairo_fill(cr);
        }
        cairo_rectangle(cr, x - 3, y - bh - 5, bw + 6, 8);
        lgrad(cr, 0, y - bh - 5, 0, y - bh + 3, {{0.0, lerpColor(c, rgb(255, 255, 255), 0.25f)}, {1.0, lerpColor(c, rgb(0, 0, 0), 0.4f)}},
          1.0, true);
        stroke(cr, kInk, 1.0);
        y -= bh + 5;
      }
      x += 66;
    }
  }
  // The shelf board and its brackets.
  cairo_rectangle(cr, 0, 170, W, 10);
  lgrad(cr, 0, 170, 0, 180, {{0.0, kWoodHi}, {1.0, kWoodLo}}, 1.0, true);
  stroke(cr, kInk, 1.4);
}

// The SIZE 42 jacket on a hanger (110 x 170, its hook at 55, 8): a tweed
// jacket with a big swing tag.
void paintJacket42(cairo_t* cr)
{
  garment(cr, 55, 14, 0, rgb(120, 96, 70), 140);
  cairo_move_to(cr, 70, 30);
  cairo_curve_to(cr, 80, 40, 84, 50, 86, 60);
  stroke(cr, kInk, 1.0);
  cairo_save(cr);
  cairo_translate(cr, 88, 76);
  cairo_rotate(cr, 0.2);
  roundRect(cr, -18, -16, 36, 40, 4);
  setRgba(cr, rgb(250, 244, 226), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.2);
  circle(cr, 0, -11, 2.4);
  stroke(cr, kInk, 1.0);
  lettering(cr, "SIZE", 0, 2, 9, rgb(60, 40, 40));
  lettering(cr, "42", 0, 20, 16, rgb(190, 30, 40));
  cairo_restore(cr);
}

// A makeup mirror's frame sides, glass and bottom bar under its top bar
// (the platform tiles): W x 200, the glass from y 0 (the frame top's
// underside) down to 150, bulbs down both sides.
void paintVanityMirror(cairo_t* cr, int W)
{
  cairo_rectangle(cr, 14, 0, W - 28, 150);
  lgrad(cr, 0, 0, W, 150, {{0.0, rgb(214, 228, 238)}, {0.45, rgb(150, 166, 188)}, {1.0, rgb(84, 94, 116)}}, 1.0, true);
  for (const double off : {0.0, 30.0, 46.0})
  {
    cairo_move_to(cr, W * 0.25 + off, 140);
    cairo_line_to(cr, W * 0.25 + off + 60, 10);
    stroke(cr, rgb(255, 255, 255), off == 46.0 ? 3.0 : 9.0, 0.16);
  }
  for (const double x : {0.0, double(W) - 18})
  {
    cairo_rectangle(cr, x, -4, 18, 158);
    lgrad(cr, x, 0, x + 18, 0, {{0.0, kBrassLo}, {0.4, kBrassHi}, {1.0, kBrassLo}}, 1.0, true);
    stroke(cr, kInk, 1.4);
    for (double y = 20; y < 150; y += 34)
      bulb(cr, x + 9, y, 5.0);
  }
  cairo_rectangle(cr, 0, 150, W, 16);
  lgrad(cr, 0, 150, 0, 166, {{0.0, kBrassHi}, {0.5, kBrass}, {1.0, kBrassLo}}, 1.0, true);
  stroke(cr, kInk, 1.4);
}

// The makeup counter (W x 150, the floor at y 146): a long counter with
// wig heads on stands, powder pots, brushes in jars.
void paintCounter(cairo_t* cr, int W)
{
  Rng rng(37601u);
  cairo_rectangle(cr, 0, 70, W, 76);
  lgrad(cr, 0, 70, 0, 146, {{0.0, kWoodHi}, {0.2, kWood}, {1.0, kWoodLo}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  for (double x = 10; x < W - 60; x += 96)
  {
    cairo_rectangle(cr, x, 84, 80, 52);
    stroke(cr, kBrass, 1.2, 0.6);
    circle(cr, x + 40, 110, 3);
    setRgba(cr, kBrass, 1.0);
    cairo_fill(cr);
  }
  cairo_rectangle(cr, -4, 64, W + 8, 8);
  lgrad(cr, 0, 64, 0, 72, {{0.0, rgb(250, 240, 220)}, {1.0, rgb(170, 150, 130)}}, 1.0, true);
  stroke(cr, kInk, 1.2);
  for (double x = 20; x < W - 30; x += rng.range(40, 70))
  {
    const int k = int(rng.next() % 3u);
    if (k == 0)
    {
      // A wig head.
      cairo_move_to(cr, x, 64);
      cairo_line_to(cr, x, 40);
      stroke(cr, kWoodLo, 3.0);
      ellipse(cr, x, 28, 10, 13);
      setRgba(cr, rgb(240, 236, 228), 1.0);
      cairo_fill_preserve(cr);
      stroke(cr, kInk, 1.0);
      const Color hair = kCloth[rng.next() % 10u];
      ellipse(cr, x, 16, 16, 12);
      setRgba(cr, hair, 1.0);
      cairo_fill_preserve(cr);
      stroke(cr, kInk, 1.0);
    }
    else if (k == 1)
    {
      // A powder pot.
      cairo_rectangle(cr, x - 9, 52, 18, 12);
      setRgba(cr, rgb(250, 200, 220), 1.0);
      cairo_fill_preserve(cr);
      stroke(cr, kInk, 1.0);
      ellipse(cr, x, 52, 10, 3);
      setRgba(cr, rgb(255, 240, 246), 1.0);
      cairo_fill(cr);
    }
    else
    {
      // Brushes in a jar.
      for (int b = -1; b <= 1; ++b)
      {
        cairo_move_to(cr, x, 58);
        cairo_line_to(cr, x + b * 6, 36);
        stroke(cr, kWoodLo, 2.0);
        ellipse(cr, x + b * 6, 34, 3, 4);
        setRgba(cr, rgb(60, 40, 30), 1.0);
        cairo_fill(cr);
      }
      cairo_rectangle(cr, x - 7, 50, 14, 14);
      setRgba(cr, rgb(180, 220, 230), 0.8);
      cairo_fill_preserve(cr);
      stroke(cr, kInk, 1.0);
    }
  }
}

// A sewing machine on its table (220 x 180, the floor at y 176): an old
// black and gold machine on a treadle table, a spool on top.
void paintSewingMachine(cairo_t* cr, int seed)
{
  // The treadle table.
  for (const double x : {30.0, 190.0})
  {
    cairo_move_to(cr, x, 100);
    cairo_line_to(cr, x, 176);
    stroke(cr, rgb(30, 28, 32), 6.0);
  }
  cairo_move_to(cr, 30, 150);
  cairo_line_to(cr, 190, 150);
  stroke(cr, rgb(30, 28, 32), 4.0);
  circle(cr, 150, 140, 18);
  stroke(cr, rgb(40, 38, 44), 4.0);
  cairo_rectangle(cr, 14, 92, 192, 12);
  lgrad(cr, 0, 92, 0, 104, {{0.0, kWoodHi}, {1.0, kWoodLo}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  // The machine.
  cairo_move_to(cr, 50, 92);
  cairo_line_to(cr, 50, 46);
  cairo_curve_to(cr, 50, 34, 60, 30, 72, 30);
  cairo_line_to(cr, 160, 30);
  cairo_curve_to(cr, 172, 30, 176, 38, 176, 48);
  cairo_line_to(cr, 176, 70);
  cairo_line_to(cr, 162, 70);
  cairo_line_to(cr, 162, 50);
  cairo_line_to(cr, 70, 50);
  cairo_line_to(cr, 70, 92);
  cairo_close_path(cr);
  lgrad(cr, 0, 30, 0, 92, {{0.0, rgb(70, 66, 76)}, {0.3, rgb(30, 28, 34)}, {1.0, rgb(12, 10, 14)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  // Gold decals.
  cairo_move_to(cr, 80, 40);
  cairo_curve_to(cr, 100, 34, 120, 46, 140, 38);
  stroke(cr, kBrass, 1.6);
  lettering(cr, seed % 2 ? "MAXTV" : "SINGALONG", 110, 46, 7, kBrassHi);
  // The needle bar, the foot.
  cairo_rectangle(cr, 164, 70, 6, 16);
  setRgba(cr, rgb(200, 204, 214), 1.0);
  cairo_fill(cr);
  // The hand wheel.
  circle(cr, 50, 56, 12);
  setRgba(cr, rgb(40, 38, 44), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kBrass, 2.0);
  // A spool on top and the fabric under the foot.
  cairo_rectangle(cr, 110, 18, 12, 12);
  setRgba(cr, kCloth[std::size_t(seed * 3) % 10u], 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.0);
  cairo_move_to(cr, 120, 92);
  cairo_line_to(cr, 200, 92);
  cairo_curve_to(cr, 204, 100, 200, 110, 196, 116);
  cairo_line_to(cr, 140, 108);
  cairo_close_path(cr);
  setRgba(cr, kCloth[std::size_t(seed * 7 + 2) % 10u], 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.0);
}

// Shelves of fabric bolts (W x 300, the floor at y 296).
void paintFabricShelf(cairo_t* cr, int W)
{
  Rng rng(37701u);
  woodPanel(cr, 0, 0, 12, 296, false);
  woodPanel(cr, W - 12, 0, 12, 296, false);
  for (int s = 0; s < 4; ++s)
  {
    const double y = 70 + s * 74;
    for (double x = 16; x < W - 30; x += rng.range(22, 30))
    {
      const Color c = kCloth[rng.next() % 10u];
      const double h = rng.range(48, 62);
      roundRect(cr, x, y - h, 20, h, 3);
      lgrad(cr, x, 0, x + 20, 0, {{0.0, lerpColor(c, rgb(0, 0, 0), 0.4f)}, {0.4, lerpColor(c, rgb(255, 255, 255), 0.2f)},
                                    {1.0, lerpColor(c, rgb(0, 0, 0), 0.5f)}}, 1.0, true);
      stroke(cr, kInk, 1.0);
    }
    cairo_rectangle(cr, 0, y, W, 8);
    lgrad(cr, 0, y, 0, y + 8, {{0.0, kWoodHi}, {1.0, kWoodLo}}, 1.0, true);
    stroke(cr, kInk, 1.2);
  }
}

// A pegboard of thread spools (220 x 200).
void paintSpoolRack(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 0, 220, 200);
  lgrad(cr, 0, 0, 0, 200, {{0.0, rgb(200, 170, 130)}, {1.0, rgb(150, 120, 90)}}, 1.0, true);
  stroke(cr, kWoodLo, 4.0);
  Rng rng(37711u);
  for (int row = 0; row < 6; ++row)
    for (int col = 0; col < 8; ++col)
    {
      const double x = 18 + col * 26, y = 20 + row * 30;
      const Color c = kCloth[rng.next() % 10u];
      cairo_rectangle(cr, x - 7, y - 10, 14, 20);
      lgrad(cr, x - 7, 0, x + 7, 0, {{0.0, lerpColor(c, rgb(0, 0, 0), 0.4f)}, {0.4, lerpColor(c, rgb(255, 255, 255), 0.25f)},
                                       {1.0, lerpColor(c, rgb(0, 0, 0), 0.5f)}}, 1.0, true);
      stroke(cr, kInk, 0.8);
      for (const double yy : {y - 11.0, y + 10.0})
      {
        cairo_rectangle(cr, x - 9, yy - 1.5, 18, 3);
        setRgba(cr, rgb(230, 200, 150), 1.0);
        cairo_fill(cr);
      }
    }
}

// A dress form (100 x 230, its foot at 50, 226) with a half-made gown pinned
// on it.
void paintDressForm(cairo_t* cr)
{
  cairo_move_to(cr, 50, 226);
  cairo_line_to(cr, 50, 150);
  stroke(cr, kWoodLo, 5.0);
  for (const double s : {-1.0, 0.0, 1.0})
  {
    cairo_move_to(cr, 50, 214);
    cairo_line_to(cr, 50 + s * 26, 226);
    stroke(cr, kWoodLo, 4.0);
  }
  cairo_move_to(cr, 30, 30);
  cairo_curve_to(cr, 40, 22, 60, 22, 70, 30);
  cairo_curve_to(cr, 76, 60, 66, 80, 62, 96);
  cairo_curve_to(cr, 72, 110, 76, 140, 72, 154);
  cairo_line_to(cr, 28, 154);
  cairo_curve_to(cr, 24, 140, 28, 110, 38, 96);
  cairo_curve_to(cr, 34, 80, 24, 60, 30, 30);
  cairo_close_path(cr);
  lgrad(cr, 24, 0, 76, 0, {{0.0, rgb(150, 130, 110)}, {0.4, rgb(230, 214, 190)}, {1.0, rgb(140, 120, 100)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  // The gown, pinned.
  cairo_move_to(cr, 34, 60);
  cairo_curve_to(cr, 44, 66, 56, 66, 66, 60);
  cairo_curve_to(cr, 64, 80, 62, 92, 64, 100);
  cairo_curve_to(cr, 80, 130, 86, 160, 90, 180);
  cairo_line_to(cr, 10, 180);
  cairo_curve_to(cr, 14, 160, 20, 130, 36, 100);
  cairo_curve_to(cr, 38, 92, 36, 80, 34, 60);
  cairo_close_path(cr);
  lgrad(cr, 10, 0, 90, 0, {{0.0, rgb(40, 90, 120)}, {0.4, rgb(90, 170, 200)}, {1.0, rgb(30, 70, 100)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  for (const auto& p : {std::array<double, 2>{40, 70}, {58, 72}, {30, 150}, {70, 156}})
  {
    circle(cr, p[0], p[1], 2.0);
    setRgba(cr, rgb(250, 60, 60), 1.0);
    cairo_fill(cr);
  }
  ellipse(cr, 50, 24, 10, 5);
  setRgba(cr, kWoodLo, 1.0);
  cairo_fill(cr);
}

// A tall unit of archive shelves (256 x 640, the floor at y 636): labelled
// boxes, ledger books, a rolled poster or two.
void paintArchiveShelf(cairo_t* cr, int seed)
{
  Rng rng(std::uint32_t(37800 + seed));
  woodPanel(cr, 0, 0, 14, 636, false);
  woodPanel(cr, 242, 0, 14, 636, false);
  cairo_rectangle(cr, 14, 0, 228, 636);
  setRgba(cr, rgb(40, 20, 16), 1.0);
  cairo_fill(cr);
  static const char* kLabels[12] = {"1979", "1982", "1985", "1988", "1991", "1997", "PROPS", "WIGS", "MASKS", "HATS", "SCRIPTS", "MISC"};
  for (int s = 0; s < 8; ++s)
  {
    const double y = 78 + s * 80;
    double x = 18;
    while (x < 230)
    {
      const int k = int(rng.next() % 5u);
      if (k < 3)
      {
        const double bw = rng.range(52, 70);
        if (x + bw > 238)
          break;
        cairo_rectangle(cr, x, y - 54, bw, 54);
        lgrad(cr, x, 0, x + bw, 0, {{0.0, rgb(150, 116, 76)}, {0.4, rgb(200, 164, 116)}, {1.0, rgb(130, 96, 60)}}, 1.0, true);
        stroke(cr, kInk, 1.2);
        cairo_rectangle(cr, x + 8, y - 44, bw - 16, 16);
        setRgba(cr, rgb(246, 240, 224), 1.0);
        cairo_fill_preserve(cr);
        stroke(cr, kInk, 0.8);
        fitLettering(cr, kLabels[rng.next() % 12u], x + bw * 0.5, y - 32, 10, bw - 20, rgb(40, 30, 30));
        x += bw + 3;
      }
      else
      {
        for (int b = 0; b < 4 && x < 226; ++b)
        {
          const double h = rng.range(44, 60);
          cairo_rectangle(cr, x, y - h, 10, h);
          setRgba(cr, kCloth[rng.next() % 10u], 1.0);
          cairo_fill_preserve(cr);
          stroke(cr, kInk, 0.8);
          x += 11;
        }
        x += 4;
      }
    }
    cairo_rectangle(cr, 14, y, 228, 8);
    lgrad(cr, 0, y, 0, y + 8, {{0.0, kWoodHi}, {1.0, kWoodLo}}, 1.0, true);
    stroke(cr, kInk, 1.0);
  }
}

// One block of the vault's steel wall (128 x 128, tiles both ways).
void paintVaultWall(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 0, 128, 128);
  setRgba(cr, rgb(60, 64, 76), 1.0);
  cairo_fill(cr);
  for (const double y : {0.0, 64.0})
    for (const double x : {0.0, 64.0})
    {
      cairo_rectangle(cr, x + 2, y + 2, 60, 60);
      lgrad(cr, x, y, x + 64, y + 64, {{0.0, rgb(120, 126, 140)}, {0.5, rgb(90, 94, 108)}, {1.0, rgb(60, 62, 74)}});
      for (const double rx : {x + 7, x + 57})
        for (const double ry : {y + 7, y + 57})
        {
          circle(cr, rx, ry, 2.2);
          setRgba(cr, rgb(180, 186, 200), 1.0);
          cairo_fill(cr);
        }
    }
}

// The vault's round door, swung open against its back wall (240 x 240).
void paintRoundDoor(cairo_t* cr)
{
  circle(cr, 120, 120, 112);
  lgrad(cr, 0, 8, 0, 232, {{0.0, rgb(200, 204, 214)}, {0.5, rgb(140, 144, 156)}, {1.0, rgb(70, 72, 84)}}, 1.0, true);
  stroke(cr, kInk, 3.0);
  circle(cr, 120, 120, 90);
  stroke(cr, rgb(90, 94, 108), 4.0);
  for (int k = 0; k < 12; ++k)
  {
    const double a = k * kPi / 6;
    circle(cr, 120 + std::cos(a) * 101, 120 + std::sin(a) * 101, 6);
    lgrad(cr, 0, 0, 0, 240, {{0.0, rgb(240, 240, 250)}, {1.0, rgb(110, 114, 126)}}, 1.0, true);
    stroke(cr, kInk, 1.0);
  }
  for (int k = 0; k < 3; ++k)
  {
    const double a = k * kPi / 3 + 0.3;
    cairo_move_to(cr, 120 - std::cos(a) * 46, 120 - std::sin(a) * 46);
    cairo_line_to(cr, 120 + std::cos(a) * 46, 120 + std::sin(a) * 46);
    stroke(cr, kInk, 9.0);
    cairo_move_to(cr, 120 - std::cos(a) * 46, 120 - std::sin(a) * 46);
    cairo_line_to(cr, 120 + std::cos(a) * 46, 120 + std::sin(a) * 46);
    stroke(cr, kBrass, 5.0);
  }
  circle(cr, 120, 120, 14);
  setRgba(cr, kBrassLo, 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.6);
  lettering(cr, "MAXTV COSTUME VAULT", 120, 206, 11, rgb(40, 40, 50));
}

// A rail of gold lamé and sequins (300 x 170, its rail at y 14): the
// vault's treasures.
void paintGoldRack(cairo_t* cr)
{
  Rng rng(37901u);
  for (double gx = 20; gx < 290; gx += 26)
  {
    const Color c = rng.next() % 3u == 0 ? rgb(230, 230, 240) : rgb(236, 190, 70);
    garment(cr, gx, 14, int(rng.next() % 4u), c, rng.range(120, 150));
    for (int k = 0; k < 6; ++k)
    {
      circle(cr, gx + rng.range(-14, 14), 30 + rng.range(0, 100), 1.2);
      setRgba(cr, rgb(255, 255, 255), 0.9);
      cairo_fill(cr);
    }
  }
  cairo_move_to(cr, 4, 14);
  cairo_line_to(cr, 296, 14);
  stroke(cr, kInk, 7.0);
  cairo_move_to(cr, 4, 14);
  cairo_line_to(cr, 296, 14);
  stroke(cr, kBrassHi, 4.0);
}

// A hanging room sign (300 x 70, hung from its top middle): a mahogany
// board, gilt edge and lettering.
void paintSign(cairo_t* cr, const char* text)
{
  roundRect(cr, 6, 10, 288, 54, 10);
  lgrad(cr, 0, 10, 0, 64, {{0.0, kWoodHi}, {0.5, kWood}, {1.0, kWoodLo}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  roundRect(cr, 12, 16, 276, 42, 7);
  stroke(cr, kBrass, 2.4);
  fitLettering(cr, text, 150, 46, 26, 250, kBrassHi);
  for (const double x : {40.0, 260.0})
  {
    circle(cr, x, 10, 5);
    stroke(cr, kBrass, 2.4);
  }
}

// A batten the ceiling needles hang from (W x 40): a timber beam with brass
// brackets.
void paintBatten(cairo_t* cr, int W)
{
  cairo_rectangle(cr, 0, 8, W, 24);
  lgrad(cr, 0, 8, 0, 32, {{0.0, kWoodHi}, {0.5, kWood}, {1.0, kWoodLo}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  for (double x = 20; x < W; x += 80)
  {
    cairo_rectangle(cr, x - 8, 4, 16, 32);
    lgrad(cr, 0, 4, 0, 36, {{0.0, kBrassHi}, {1.0, kBrassLo}}, 1.0, true);
    stroke(cr, kInk, 1.0);
  }
}

// A pulley (48 x 48, centred).
void paintPulley(cairo_t* cr)
{
  circle(cr, 24, 24, 20);
  lgrad(cr, 0, 4, 0, 44, {{0.0, rgb(210, 214, 224)}, {1.0, rgb(80, 84, 96)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  circle(cr, 24, 24, 12);
  stroke(cr, rgb(60, 62, 72), 3.0);
  circle(cr, 24, 24, 4);
  setRgba(cr, kBrass, 1.0);
  cairo_fill(cr);
}

// The "MIND THE PINS" board for the needle pit (200 x 64).
void paintPinSign(cairo_t* cr)
{
  roundRect(cr, 4, 4, 192, 56, 6);
  lgrad(cr, 0, 4, 0, 60, {{0.0, rgb(255, 236, 120)}, {1.0, rgb(220, 180, 50)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  for (double x = 8; x < 196; x += 18)
  {
    cairo_move_to(cr, x, 8);
    cairo_line_to(cr, x + 8, 8);
    cairo_line_to(cr, x, 16);
    cairo_close_path(cr);
    setRgba(cr, kInk, 0.8);
    cairo_fill(cr);
  }
  lettering(cr, "MIND THE PINS", 100, 46, 20, rgb(160, 20, 20));
}

// The Decoy Launcher's icon (64 x 64).
void paintDecoyIcon(cairo_t* cr)
{
  // The stock.
  cairo_move_to(cr, 6, 40);
  cairo_curve_to(cr, 6, 32, 18, 30, 26, 32);
  cairo_line_to(cr, 26, 44);
  cairo_curve_to(cr, 18, 50, 8, 52, 6, 40);
  cairo_close_path(cr);
  lgrad(cr, 0, 30, 0, 52, {{0.0, kWoodHi}, {1.0, kWoodLo}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  // The barrel, flaring to a bell.
  cairo_move_to(cr, 24, 32);
  cairo_line_to(cr, 44, 30);
  cairo_curve_to(cr, 50, 24, 56, 20, 60, 18);
  cairo_line_to(cr, 60, 50);
  cairo_curve_to(cr, 56, 48, 50, 44, 44, 40);
  cairo_line_to(cr, 24, 42);
  cairo_close_path(cr);
  lgrad(cr, 0, 18, 0, 50, {{0.0, kBrassHi}, {0.5, kBrass}, {1.0, kBrassLo}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  ellipse(cr, 60, 34, 4, 16);
  setRgba(cr, rgb(50, 26, 16), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.4);
  // The mannequin's head peeping out of the bell.
  ellipse(cr, 58, 22, 8, 10);
  lgrad(cr, 50, 12, 66, 32, {{0.0, rgb(255, 240, 220)}, {1.0, rgb(200, 160, 120)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  // The trigger guard, a band.
  cairo_arc(cr, 24, 48, 5, 0, kPi);
  stroke(cr, kBrassLo, 2.0);
  cairo_rectangle(cr, 34, 30, 4, 12);
  setRgba(cr, kVelvet, 1.0);
  cairo_fill(cr);
}

// --- Live helpers ------------------------------------------------------------------------------------

// A chain hanging from (x, y0) down to (x, y1), its links alternating.
void drawChain(Renderer& r, float x, float y0, float y1)
{
  int k = 0;
  for (float y = y0; y < y1; y += 9.0f, ++k)
  {
    const float h = std::min(11.0f, y1 - y);
    if (k % 2 == 0)
    {
      r.fillRect(x - 4.0f, y, 8.0f, h, rgb(40, 40, 48));
      r.fillRect(x - 2.0f, y + 2.0f, 4.0f, std::max(0.0f, h - 4.0f), rgb(20, 14, 16));
      r.fillRect(x - 4.0f, y, 2.0f, h, rgb(170, 176, 190));
    }
    else
      r.fillRect(x - 1.5f, y, 3.0f, h, rgb(150, 156, 170));
  }
}

// TV static over a rectangle.
void drawStatic(Renderer& r, float x, float y, float w, float h, int frame, int seed, float alpha)
{
  for (int row = 0; row < int(h / 8.0f); ++row)
  {
    const unsigned hsh = hash2(row * 7 + frame * 131, seed + frame);
    const int v = int(60 + (hsh % 160u));
    r.fillRect(x, y + float(row) * 8.0f, w, 8.0f, rgba(v, v, v + 10, int(255.0f * alpha)));
    if (hsh % 5u == 0)
      r.fillRect(x + float(hsh % 100u) * (w - 18.0f) / 100.0f, y + float(row) * 8.0f, 18.0f, 8.0f, rgba(240, 240, 255, int(255.0f * alpha)));
  }
}

// The sprite the runner shows (for the fog's outline and the suit), as
// World::drawPlayer picks it.
const Sprite& runnerSprite(const Art& art, const World& w, int frame)
{
  const auto& p = w.player();
  const auto& ca = art.runner(w.character());
  switch (p.visual)
  {
    case PlayerVisual::Walking:
      return ca.run[std::size_t(p.walkFrame % kRunFrames)];
    case PlayerVisual::LookingUp:
      return ca.lookUp;
    case PlayerVisual::Crouching:
      return ca.crouch;
    case PlayerVisual::Coiling:
      return ca.coil;
    case PlayerVisual::Jumping:
      return ca.jump;
    case PlayerVisual::Falling:
      return ca.fall;
    case PlayerVisual::FallingFull:
      return ca.fallFast;
    case PlayerVisual::ClimbingLadder:
      return ca.climb[std::size_t(p.climbFrame % 2)];
    case PlayerVisual::Dying:
      return ca.hurt;
    default:
      return ca.idle[std::size_t((frame / 30) % 2)];
  }
}

// The Gorgazilla suit's pose for what the runner is doing.
int suitPose(const Player& p)
{
  switch (p.visual)
  {
    case PlayerVisual::Walking:
      return 1 + (p.walkFrame / 2) % 2;
    case PlayerVisual::Jumping:
    case PlayerVisual::Falling:
    case PlayerVisual::FallingFull:
    case PlayerVisual::Somersault:
      return 3;
    default:
      return 0;
  }
}

// Where the runner's face is, from the sprite's anchor (the feet's middle),
// facing right: for the mustache.
std::array<float, 2> faceAt(int runner, PlayerVisual v)
{
  float x = 22.0f, y = -122.0f;
  if (runner == 1)
  {
    x = 24.0f;
    y = -130.0f;
  }
  else if (runner == 2)
  {
    x = 20.0f;
    y = -124.0f;
  }
  if (v == PlayerVisual::Crouching || v == PlayerVisual::Coiling)
    y += 40.0f;
  return {x, y};
}

// Fog puffs over a cell rectangle, `a` 0..1, seeded per cloud.
void drawPuffs(Renderer& r, const Art& art, float x, float y, float w, float h, Color c, float a, int seed, int frame)
{
  for (int k = 0; k < 9; ++k)
  {
    const std::uint32_t hs = hash2(seed, k);
    const float px = x + w * (0.12f + 0.76f * unit(hs)), py = y + h * (0.2f + 0.6f * unit(hs >> 7));
    const float rad = (0.28f + 0.16f * unit(hs >> 13)) * std::min(w, h);
    const float drift = std::sin(float(frame) * 0.03f + float(k)) * 8.0f;
    DrawOpts o;
    o.tint = c;
    o.alpha = std::min(1.0f, a * (0.55f + 0.3f * unit(hs >> 3)));
    o.scale = rad / 16.0f;
    r.draw(art.dot, px + drift, py, o);
  }
}

} // namespace

// --- Behind the props and tiles ------------------------------------------------------------------------

void drawWardrobeSet(Renderer& r, const World& w, const Art& art, float camX, float camY, int frame, float alpha)
{
  (void)alpha;
  const auto& s = w.wardrobe();
  const auto& map = w.map();
  // Block coordinates to the screen.
  auto bx = [&](float tx) { return tx * kTilePx - camX; };
  auto by = [&](float ty) { return ty * kTilePx - camY; };
  auto put = [&](const Texture& t, float tx, float ty) {
    const float x = bx(tx), y = by(ty);
    if (!offScreen(x - t.anchorX(), y - t.anchorY(), float(t.w()), float(t.h())))
      r.draw(t, x, y);
  };
  // The battens the ceiling needles hang from, and their chains up.
  for (int ty = 1; ty < map.height() / kCellsPerTile; ++ty)
    for (int tx = 0; tx < map.width() / kCellsPerTile; ++tx)
    {
      if (map.block(tx, ty) != Tile::SpikesDown || map.block(tx - 1, ty) == Tile::SpikesDown)
        continue;
      if (map.block(tx, ty - 1) == Tile::Solid)
        continue;
      int n = 1;
      while (map.block(tx + n, ty) == Tile::SpikesDown)
        ++n;
      const float x = bx(float(tx)), y = by(float(ty));
      if (offScreen(x, y - 600.0f, float(n) * kTilePx, 640.0f))
        continue;
      for (const float cx : {x + 24.0f, x + float(n) * kTilePx - 24.0f})
        drawChain(r, cx, std::max(-10.0f, y - 700.0f), y - 4.0f);
      const int bw = n * 64 + 16;
      r.draw(baked(art, r, "batten/" + std::to_string(n), bw, 40, 8.0f, 36.0f, [bw](cairo_t* cr) { paintBatten(cr, bw); }), x, y + 2.0f);
    }

  if (s.totem)
  {
    // The Totem's stage: painted flats of a castle and trees, the
    // footlights along the front.
    const float floorY = by(22.0f);
    for (int k = 0; k < 4; ++k)
    {
      const float x = bx(4.0f + float(k) * 16.0f);
      const Texture& flat = baked(art, r, "flat/" + std::to_string(k % 2), 360, 320, 180.0f, 320.0f, [k](cairo_t* cr) {
        if (k % 2 == 0)
        {
          // A painted castle.
          cairo_rectangle(cr, 60, 120, 240, 200);
          lgrad(cr, 0, 120, 0, 320, {{0.0, rgb(170, 160, 200)}, {1.0, rgb(110, 100, 150)}}, 1.0, true);
          stroke(cr, kInk, 3.0);
          for (const double tx : {40.0, 280.0})
          {
            cairo_rectangle(cr, tx, 60, 60, 260);
            lgrad(cr, 0, 60, 0, 320, {{0.0, rgb(190, 180, 220)}, {1.0, rgb(110, 100, 150)}}, 1.0, true);
            stroke(cr, kInk, 3.0);
            cairo_move_to(cr, tx - 6, 60);
            cairo_line_to(cr, tx + 30, 10);
            cairo_line_to(cr, tx + 66, 60);
            cairo_close_path(cr);
            setRgba(cr, rgb(200, 40, 60), 1.0);
            cairo_fill_preserve(cr);
            stroke(cr, kInk, 3.0);
          }
          cairo_move_to(cr, 150, 320);
          cairo_line_to(cr, 150, 250);
          cairo_curve_to(cr, 150, 220, 210, 220, 210, 250);
          cairo_line_to(cr, 210, 320);
          cairo_close_path(cr);
          setRgba(cr, rgb(60, 40, 50), 1.0);
          cairo_fill(cr);
        }
        else
        {
          // Painted trees.
          for (int t = 0; t < 3; ++t)
          {
            const double tx = 60 + t * 110;
            cairo_rectangle(cr, tx - 10, 200, 20, 120);
            setRgba(cr, rgb(110, 70, 40), 1.0);
            cairo_fill_preserve(cr);
            stroke(cr, kInk, 2.4);
            ellipse(cr, tx, 160, 70, 80);
            lgrad(cr, 0, 80, 0, 240, {{0.0, rgb(110, 190, 90)}, {1.0, rgb(40, 110, 50)}}, 1.0, true);
            stroke(cr, kInk, 3.0);
          }
        }
        // The flat's wooden edge and brace.
        cairo_rectangle(cr, 2, 2, 356, 316);
        stroke(cr, rgb(140, 110, 80), 3.0, 0.5);
      });
      DrawOpts o;
      o.tint = rgb(210, 190, 190);
      if (!offScreen(x - 180.0f, floorY - 320.0f, 360.0f, 320.0f))
        r.draw(flat, x, floorY, o);
    }
    for (int tx = 1; tx < 63; tx += 2)
    {
      const float x = bx(float(tx)) + 32.0f, y = floorY - 6.0f;
      if (x < -60.0f || x > float(kScreenW) + 60.0f)
        continue;
      r.fillRect(x - 12.0f, y - 2.0f, 24.0f, 8.0f, kBrassLo);
      drawGlow(r, art, x, y - 6.0f, 46.0f, rgb(255, 210, 140), 0.45f + 0.08f * std::sin(float(frame) * 0.05f + float(tx)));
    }
    return;
  }

  // --- 1. The fitting room (blocks 0-36) -----------------------------------------------------------
  put(baked(art, r, "chaise", 260, 120, 0.0f, 116.0f, paintChaise), 0.4f, 48.0f);
  put(baked(art, r, "coatstand", 120, 270, 60.0f, 266.0f, paintCoatStand), 8.4f, 48.0f);
  // The stalls' back walls: cream, panelled, a long mirror in each.
  {
    const float x0 = bx(15.0f), x1 = bx(29.0f), y0 = by(34.0f), y1 = by(44.0f);
    if (!offScreen(x0, y0, x1 - x0, y1 - y0))
    {
      r.fillRect(x0, y0, x1 - x0, y1 - y0, rgb(206, 186, 156));
      for (float x = x0; x < x1; x += 128.0f)
      {
        r.fillRect(x + 10.0f, y0 + 14.0f, 108.0f, y1 - y0 - 80.0f, rgb(222, 204, 176));
        r.fillRect(x + 10.0f, y0 + 14.0f, 108.0f, 3.0f, rgb(240, 226, 200));
        r.fillRect(x, y0, 4.0f, y1 - y0, rgb(150, 126, 100));
      }
      r.fillRect(x0, y1 - 64.0f, x1 - x0, 64.0f, rgb(120, 66, 44));
      r.fillRect(x0, y1 - 68.0f, x1 - x0, 6.0f, kBrass);
      for (const char* c : {"B", "C"})
      {
        const float sx = c[0] == 'B' ? bx(18.0f) : bx(25.5f);
        r.drawText(c, sx, y0 + 30.0f, {40.0f, rgb(150, 110, 70), 0}, Align::Center);
      }
    }
    put(baked(art, r, "longmirror", 100, 260, 50.0f, 260.0f, paintLongMirror), 17.0f, 43.4f);
  }
  put(baked(art, r, "sign/FITTING ROOMS", 300, 70, 150.0f, 0.0f, [](cairo_t* cr) { paintSign(cr, "FITTING ROOMS"); }), 6.0f, 40.5f);

  // --- 2. The costume racks (37-68) ----------------------------------------------------------------
  for (int k = 0; k < 3; ++k)
  {
    const float tx = 46.0f + float(k) * 7.4f;
    const Texture& rack = baked(art, r, "bigrack/" + std::to_string(k), 470, 250, 0.0f, 246.0f, [k](cairo_t* cr) { paintBigRack(cr, 470, k); });
    DrawOpts o;
    o.tint = rgb(200, 180, 170);
    const float x = bx(tx), y = by(44.0f);
    if (!offScreen(x, y - 250.0f, 470.0f, 250.0f))
      r.draw(rack, x, y, o);
  }
  for (int k = 0; k < 2; ++k)
  {
    const Texture& shelf = baked(art, r, "hatshelf/" + std::to_string(k), 400, 182, 0.0f, 180.0f, [k](cairo_t* cr) { paintHatShelf(cr, 400, k); });
    put(shelf, 47.5f + float(k) * 6.6f, 36.4f);
  }
  // The SIZE 42 jacket (the level's `@ deco kind=42`), on the back rack.
  for (const auto& pr : w.props())
  {
    if (pr.kind != PropKind::Deco42)
      continue;
    const float x = (float(pr.x) + float(pr.w) * 0.5f) * kCellPx - camX, y = float(pr.y) * kCellPx - camY - 40.0f;
    if (!offScreen(x - 60.0f, y, 120.0f, 180.0f))
      r.draw(baked(art, r, "jacket42", 110, 170, 55.0f, 8.0f, paintJacket42), x, y);
  }
  put(baked(art, r, "sign/COSTUMES", 300, 70, 150.0f, 0.0f, [](cairo_t* cr) { paintSign(cr, "COSTUMES A - Z"); }), 40.0f, 34.5f);

  // --- 3. The makeup hall (69-100) -----------------------------------------------------------------
  put(baked(art, r, "counter", 640, 150, 0.0f, 146.0f, [](cairo_t* cr) { paintCounter(cr, 640); }), 78.6f, 48.0f);
  // Under each run of mirror-frame tops (the platforms): the mirror.
  for (int ty = 20; ty < 48; ++ty)
    for (int tx = 69; tx <= 100; ++tx)
    {
      if (map.block(tx, ty) != Tile::Platform || map.block(tx - 1, ty) == Tile::Platform)
        continue;
      int n = 1;
      while (map.block(tx + n, ty) == Tile::Platform)
        ++n;
      const int W = n * 64;
      put(baked(art, r, "vanitymirror/" + std::to_string(n), W, 170, 0.0f, 0.0f, [W](cairo_t* cr) { paintVanityMirror(cr, W); }), float(tx),
        float(ty) + 0.5f);
      // The bulbs' glow down the sides.
      const float x = bx(float(tx)), y = by(float(ty) + 0.5f);
      if (!offScreen(x, y, float(W), 170.0f))
        for (float yy = 20.0f; yy < 150.0f; yy += 34.0f)
          for (const float xx : {9.0f, float(W) - 9.0f})
            drawGlow(r, art, x + xx, y + yy, 26.0f, rgb(255, 214, 150), 0.35f + 0.05f * std::sin(float(frame) * 0.07f + yy));
    }
  put(baked(art, r, "sign/MAKEUP", 300, 70, 150.0f, 0.0f, [](cairo_t* cr) { paintSign(cr, "MAKEUP"); }), 79.0f, 31.0f);
  {
    // A marquee of bulbs round the sign.
    const float x = bx(79.0f), y = by(31.0f);
    if (!offScreen(x - 160.0f, y, 320.0f, 80.0f))
      for (int k = 0; k < 12; ++k)
      {
        const bool on = (k + frame / 10) % 3 != 0;
        const float bx2 = x - 140.0f + float(k) * 25.5f;
        drawGlow(r, art, bx2, y + 4.0f, on ? 16.0f : 8.0f, rgb(255, 220, 140), on ? 0.8f : 0.3f);
        drawGlow(r, art, bx2, y + 70.0f, on ? 16.0f : 8.0f, rgb(255, 220, 140), on ? 0.3f : 0.8f);
      }
  }

  // --- 4. The sewing room (101-130) ----------------------------------------------------------------
  put(baked(art, r, "fabricshelf", 380, 300, 0.0f, 296.0f, [](cairo_t* cr) { paintFabricShelf(cr, 380); }), 103.6f, 44.6f);
  put(baked(art, r, "spoolrack", 220, 200, 0.0f, 0.0f, paintSpoolRack), 124.0f, 41.0f);
  for (int k = 0; k < 2; ++k)
    put(baked(art, r, "sewing/" + std::to_string(k), 220, 180, 110.0f, 176.0f, [k](cairo_t* cr) { paintSewingMachine(cr, k); }),
      k == 0 ? 107.4f : 128.0f, 48.0f);
  put(baked(art, r, "dressform", 100, 230, 50.0f, 226.0f, paintDressForm), 111.6f, 48.0f);
  {
    // The needle pit's walls, its warning board, and the pulley beam
    // over it that the cutting tables hang from.
    const float x0 = bx(113.0f), x1 = bx(123.0f), y0 = by(48.0f), y1 = by(52.0f);
    if (!offScreen(x0, y0, x1 - x0, y1 - y0))
    {
      r.fillRect(x0, y0, x1 - x0, y1 - y0, rgb(60, 28, 22));
      for (float x = x0; x < x1; x += 32.0f)
        r.fillRect(x, y0, 2.0f, y1 - y0, rgb(40, 18, 14));
      r.fillRect(x0, y0, x1 - x0, 24.0f, rgba(0, 0, 0, 90));
    }
    put(baked(art, r, "pinsign", 200, 64, 100.0f, 0.0f, paintPinSign), 117.0f, 49.0f);
    const float beamY = by(33.0f);
    if (!offScreen(x0, beamY - 40.0f, x1 - x0, 80.0f))
    {
      for (const float cx : {x0 + 40.0f, x1 - 40.0f})
        drawChain(r, cx, std::max(-10.0f, beamY - 700.0f), beamY);
      r.fillRect(x0 - 10.0f, beamY, x1 - x0 + 20.0f, 22.0f, rgb(110, 60, 38));
      r.fillRect(x0 - 10.0f, beamY, x1 - x0 + 20.0f, 4.0f, rgb(170, 100, 60));
      r.fillRect(x0 - 10.0f, beamY + 18.0f, x1 - x0 + 20.0f, 4.0f, rgb(50, 22, 14));
    }
    for (const auto& t : s.tables)
      for (const float px : {float(t.x0) + 0.22f, float(t.x1) + 0.78f})
        put(baked(art, r, "pulley", 48, 48, 24.0f, 24.0f, paintPulley), px, 33.7f);
  }
  put(baked(art, r, "sign/SEWING", 300, 70, 150.0f, 0.0f, [](cairo_t* cr) { paintSign(cr, "SEWING ROOM"); }), 106.0f, 36.0f);

  // --- 5. The archive (131-159) --------------------------------------------------------------------
  for (int k = 0; k < 7; ++k)
  {
    const Texture& sh = baked(art, r, "archiveshelf/" + std::to_string(k % 4), 256, 640, 0.0f, 636.0f,
      [k](cairo_t* cr) { paintArchiveShelf(cr, k % 4); });
    DrawOpts o;
    o.tint = rgb(190, 170, 160);
    const float x = bx(131.0f + float(k) * 4.0f), y = by(48.0f);
    if (!offScreen(x, y - 640.0f, 256.0f, 640.0f))
      r.draw(sh, x, y, o);
  }
  put(baked(art, r, "sign/ARCHIVE", 300, 70, 150.0f, 0.0f, [](cairo_t* cr) { paintSign(cr, "ARCHIVE"); }), 150.0f, 30.0f);
  {
    // The vault: steel walls, its round door, the gold rack.
    const float x0 = bx(143.0f), x1 = bx(159.0f), y0 = by(50.0f), y1 = by(55.0f);
    if (!offScreen(x0, y0, x1 - x0, y1 - y0))
    {
      const Texture& wall = baked(art, r, "vaultwall", 128, 128, 0.0f, 0.0f, paintVaultWall);
      for (float y = y0; y < y1; y += 128.0f)
        for (float x = x0; x < x1; x += 128.0f)
          r.draw(wall, x, y);
      r.fillRect(x0, y0, x1 - x0, 30.0f, rgba(0, 0, 0, 110));
      put(baked(art, r, "rounddoor", 240, 240, 120.0f, 240.0f, paintRoundDoor), 147.0f, 55.0f);
      put(baked(art, r, "goldrack", 300, 170, 0.0f, 0.0f, paintGoldRack), 149.6f, 51.3f);
      drawGlow(r, art, bx(152.0f), by(52.5f), 220.0f, rgb(255, 210, 120), 0.25f);
    }
  }
}

// --- Behind the runner ---------------------------------------------------------------------------------

bool World::wardrobeDrawsBlock(int tx, int ty) const
{
  const auto& s = mWardrobe;
  for (const auto& sh : s.shutters)
    if (!sh.open && tx >= sh.x0 && tx <= sh.x1 && ty >= sh.y0 && ty <= sh.y1)
      return true;
  for (const auto& r : s.pushRacks)
    if (tx >= r.x && tx < r.x + r.w && ty >= r.y && ty < r.y + r.h)
      return true;
  for (const auto& t : s.tables)
    if (ty == t.row && tx >= t.x0 && tx <= t.x1)
      return true; // a cutting table, not a mirror frame
  return false;
}

void World::drawWardrobeBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)alpha;
  const auto& s = mWardrobe;
  // Booths: a cabinet, its curtain (open, or swinging back open after a
  // swap), the name on the sign.
  for (std::size_t i = 0; i < s.booths.size(); ++i)
  {
    const auto& b = s.booths[i];
    const float left = float(b.x) * kCellPx - camX, foot = float(b.y + 1) * kCellPx - camY;
    if (offScreen(left - 20.0f, foot - 250.0f, 170.0f, 260.0f))
      continue;
    const int kind = b.exit ? 4 : b.runner == -2 ? 3 : std::clamp(b.runner, 0, 2);
    r.draw(baked(mArt, r, "booth/" + std::to_string(kind), 160, 248, 80.0f, 240.0f, [kind](cairo_t* cr) { paintBooth(cr, kind); }),
      left + 64.0f, foot);
    if (b.exit)
      for (int k = 0; k < 6; ++k)
        for (const float x : {6.0f, 122.0f})
          drawGlow(r, mArt, left + x, foot - 170.0f + float(k) * 30.0f, 14.0f, rgb(255, 220, 150),
            (k + frame / 8) % 2 ? 0.75f : 0.35f);
    if (int(i) == s.booth && s.boothT > 0)
      continue; // drawn shut in front of the runner (drawWardrobeFront)
    // Open, bunched to the left; swinging back open just after a swap.
    float closed = 0.18f, sway = 0.0f;
    if (b.curtain > 0)
    {
      closed = 0.18f + 0.82f * std::min(1.0f, float(b.curtain) / 6.0f);
      sway = std::sin(float(frame) * 0.6f) * std::min(1.0f, float(b.curtain) / 6.0f);
    }
    if (b.exit)
      closed = 1.0f;
    const int style = b.exit ? 2 : b.runner == -2 ? 1 : 0;
    const int cq = int(closed * 10.0f + 0.5f), sq = int(std::lround(sway * 2.0f));
    r.draw(baked(mArt, r, "curtain/" + std::to_string(style) + "/" + std::to_string(cq) + "/" + std::to_string(sq), 112, 180, 0.0f, 0.0f,
             [cq, sq, style](cairo_t* cr) { paintCurtain(cr, cq / 10.0, sq / 2.0, style); }),
      left + 12.0f, foot - 176.0f);
    if (b.runner == -2)
      r.draw(baked(mArt, r, "outoforder", 120, 64, 60.0f, 0.0f, paintOutOfOrder), left + 64.0f, foot - 130.0f);
  }
  // The vault's triple booth round the bonus static: three runners'
  // colours on its sign, static flickering in its curtain.
  for (const auto& pr : mProps)
  {
    if (pr.kind != PropKind::BonusDoor || pr.dormant)
      continue;
    const float midX = (float(pr.x) + float(pr.w) * 0.5f) * kCellPx - camX;
    const float foot = (float(pr.y) + 2.0f) * kCellPx - camY;
    if (offScreen(midX - 120.0f, foot - 260.0f, 240.0f, 270.0f))
      continue;
    DrawOpts wide;
    wide.scale = 1.0f;
    r.draw(baked(mArt, r, "booth/5", 160, 248, 80.0f, 240.0f, [](cairo_t* cr) { paintBooth(cr, 5); }), midX, foot, wide);
    const bool near = mPlayer.box().intersects({pr.x - 12, pr.y - 12, pr.w + 24, pr.h + 24});
    drawStatic(r, midX - 52.0f, foot - 172.0f, 104.0f, 168.0f, near && !pr.used ? (frame / 2) % 8 : (frame / 12) % 8, pr.x,
      pr.used ? 0.4f : 0.95f);
    if (near && !pr.used)
      drawGlow(r, mArt, midX, foot - 90.0f, 120.0f, rgb(200, 220, 255), 0.35f);
    for (int k = 0; k < 3; ++k)
      drawGlow(r, mArt, midX - 30.0f + float(k) * 30.0f, foot - 236.0f, 10.0f, runnerTone(k), 0.9f);
  }
  // Spring racks: squashed and wobbling when they throw.
  for (const auto& k : s.racks)
  {
    const float mid = (float(k.x) + float(k.w) * 0.5f) * kCellPx - camX, floorY = float(k.y + 1) * kCellPx - camY;
    if (offScreen(mid - 100.0f, floorY - 170.0f, 200.0f, 190.0f))
      continue;
    const int sq = k.bounce > 5 ? 2 : k.bounce > 2 ? 1 : 0;
    const float wob = k.bounce > 0 ? std::sin(float(frame) * 1.3f) * float(k.bounce) * 0.6f : 0.0f;
    DrawOpts o;
    o.angle = wob;
    r.draw(baked(mArt, r, "spring/" + std::to_string(sq), 200, 180, 100.0f, 160.0f, [sq](cairo_t* cr) { paintSpringRack(cr, sq); }), mid, floorY, o);
  }
  // The cutting tables on their chains (drawn instead of the mirror-frame
  // platform), then the plates (one rides a table).
  for (const auto& t : s.tables)
  {
    const int n = t.x1 - t.x0 + 1;
    float row = float(t.row);
    if (t.lower && !t.down())
      row += float(t.t % kTableStep) / float(kTableStep) * 0.0f; // whole blocks, as the logic moves it
    const float x = float(t.x0) * kTilePx - camX, top = row * kTilePx - camY;
    if (offScreen(x, top - 800.0f, float(n) * kTilePx, 900.0f))
      continue;
    const float beamY = 33.0f * kTilePx + 22.0f - camY;
    for (const float cx : {x + 14.0f, x + float(n) * kTilePx - 14.0f})
      drawChain(r, cx, beamY, top + 6.0f);
    const float shake = t.lower && !t.down() ? std::sin(float(frame) * 1.7f) * 1.5f : 0.0f;
    r.draw(baked(mArt, r, "table/" + std::to_string(n), n * 64, 80, 0.0f, 12.0f,
             [n](cairo_t* cr) { paintTable(cr, n, n); }),
      x + shake, top);
  }
  for (const auto& pl : s.plates)
  {
    const CutTable* t = nullptr;
    for (const auto& tb : s.tables)
      if (tb.id == pl.onTable)
        t = &tb;
    const float floorY = t ? float(t->row) * kTilePx - camY : float(pl.y + 1) * kCellPx - camY;
    const float x = float(pl.x) * kCellPx - camX;
    if (offScreen(x - 10.0f, floorY - 40.0f, float(pl.w) * kCellPx + 20.0f, 60.0f))
      continue;
    const int wpx = pl.w * int(kCellPx);
    const bool down = pl.down;
    const int weight = pl.weight;
    r.draw(baked(mArt, r, "plate/" + std::to_string(wpx) + "/" + std::to_string(down ? 1 : 0) + "/" + std::to_string(weight), wpx + 16, 40,
             8.0f, 24.0f, [wpx, down, weight](cairo_t* cr) { paintPlate(cr, wpx, down, weight); }),
      x, floorY);
    if (pl.hint > 30)
    {
      // "Too light": the plate's lamp blinks red.
      drawGlow(r, mArt, x + float(wpx) - 6.0f, floorY - 6.0f, 18.0f, rgb(255, 60, 40), (frame / 4) % 2 ? 0.9f : 0.2f);
    }
    else if (down)
      drawGlow(r, mArt, x + float(wpx) - 6.0f, floorY - 2.0f, 14.0f, rgb(120, 255, 140), 0.5f);
  }
  // Shutters: corrugated steel under a roller box, rolling up when opened.
  for (const auto& sh : s.shutters)
  {
    const float x = float(sh.x0) * kTilePx - camX, y = float(sh.y0) * kTilePx - camY;
    const float w = float(sh.x1 - sh.x0 + 1) * kTilePx, h = float(sh.y1 - sh.y0 + 1) * kTilePx;
    if (offScreen(x - 10.0f, y - 40.0f, w + 20.0f, h + 50.0f))
      continue;
    const float shown = sh.open ? h * float(12 - sh.t) / 12.0f : h;
    if (shown > 2.0f)
    {
      r.fillRect(x + 4.0f, y, w - 8.0f, shown, rgb(120, 116, 112));
      for (float yy = 0.0f; yy < shown - 2.0f; yy += 10.0f)
      {
        r.fillRect(x + 4.0f, y + yy, w - 8.0f, 3.0f, rgb(170, 166, 160));
        r.fillRect(x + 4.0f, y + yy + 7.0f, w - 8.0f, 2.0f, rgb(70, 66, 64));
      }
      r.fillRect(x + 2.0f, y + shown - 10.0f, w - 4.0f, 10.0f, rgb(60, 56, 54));
      r.fillRect(x + w * 0.5f - 10.0f, y + shown - 7.0f, 20.0f, 4.0f, kBrass);
    }
    // The guide rails and the roller box.
    for (const float gx : {x, x + w - 6.0f})
    {
      r.fillRect(gx, y, 6.0f, h, rgb(66, 68, 76));
      r.fillRect(gx + 1.0f, y, 2.0f, h, rgb(146, 148, 158));
    }
    r.fillRect(x - 4.0f, y - 22.0f, w + 8.0f, 24.0f, rgb(80, 78, 82));
    r.fillRect(x - 4.0f, y - 22.0f, w + 8.0f, 4.0f, rgb(150, 148, 150));
    if (sh.open && sh.t < 12)
      r.fillRect(x - 4.0f, y - 22.0f, w + 8.0f, 24.0f, rgba(255, 255, 255, 30 * ((frame / 2) % 2)));
    else if (!sh.open)
      drawGlow(r, mArt, x + w * 0.5f, y - 10.0f, 10.0f, rgb(255, 60, 40), 0.7f);
  }
  // The vault doors and the push rack in front of the upper one.
  for (const auto& d : s.doors)
  {
    const float x = float(d.x) * kCellPx - camX, foot = float(d.y + 1) * kCellPx - camY;
    if (offScreen(x - 10.0f, foot - 180.0f, 90.0f, 190.0f))
      continue;
    const bool down = d.toY > d.y;
    r.draw(baked(mArt, r, down ? "vaultdoor/1" : "vaultdoor/0", 80, 176, 8.0f, 168.0f, [down](cairo_t* cr) { paintVaultDoor(cr, down); }), x,
      foot);
  }
  for (const auto& k : s.pushRacks)
  {
    const float wob = k.slide > 0 ? ((frame / 2) % 2 ? 2.0f : -2.0f) : 0.0f;
    const float x = float(k.x) * kTilePx - camX + wob, y = float(k.y) * kTilePx - camY;
    if (offScreen(x - 10.0f, y - 10.0f, float(k.w) * kTilePx + 20.0f, float(k.h) * kTilePx + 20.0f))
      continue;
    r.draw(baked(mArt, r, "pushrack", 144, 208, 8.0f, 8.0f, paintPushRack), x, y);
    if (k.slide > 0)
      for (int q = 0; q < 3; ++q)
        r.fillRect(x + (q % 2 ? -14.0f : float(k.w) * kTilePx + 6.0f), y + float(k.h) * kTilePx - 10.0f - float(q) * 4.0f, 8.0f, 2.0f,
          rgba(230, 210, 180, 160));
  }
  // The Gorgazilla suit on its rack (or the empty hanger, swinging).
  if (s.suitX >= 0)
  {
    const float mid = float(s.suitX + 2) * kCellPx - camX, floorY = float(s.suitY + 1) * kCellPx - camY;
    if (!offScreen(mid - 90.0f, floorY - 230.0f, 180.0f, 240.0f))
    {
      r.draw(baked(mArt, r, "suitrack", 160, 220, 80.0f, 212.0f, paintSuitRack), mid, floorY);
      const float railY = floorY - 190.0f;
      if (!s.suitTouched && !s.suit)
        r.draw(styledEnemySprite(mArt, r, mTheme, "gorgazilla_suit", 0, 0, 3, 5).get(1), mid - 8.0f, railY + 170.0f);
      else
      {
        const float a = std::sin(float(frame) * 0.08f) * 0.3f;
        const float hx = mid - 8.0f + std::sin(a) * 20.0f, hy = railY + 14.0f;
        r.drawLine(mid - 8.0f, railY, hx, hy, 3.0f, rgb(90, 90, 100));
        r.drawLine(hx - 18.0f, hy + 10.0f, hx, hy, 3.0f, kWoodLo);
        r.drawLine(hx + 18.0f, hy + 10.0f, hx, hy, 3.0f, kWoodLo);
      }
    }
  }
  // The bulb mirror: its bulbs chase round it.
  if (s.mirrorX >= 0)
  {
    const float mid = float(s.mirrorX + 2) * kCellPx - camX, floorY = float(s.mirrorY + 1) * kCellPx - camY;
    if (!offScreen(mid - 90.0f, floorY - 180.0f, 180.0f, 190.0f))
    {
      r.draw(baked(mArt, r, "vanity", 170, 172, 85.0f, 160.0f, paintVanity), mid, floorY);
      for (std::size_t k = 0; k < kVanityBulbs.size(); ++k)
      {
        const bool on = (int(k) + frame / 8) % 3 != 0 || s.sit > 0;
        const float x = mid - 85.0f + kVanityBulbs[k][0], y = floorY - 160.0f + kVanityBulbs[k][1];
        r.fillRect(x - 4.0f, y - 4.0f, 8.0f, 8.0f, on ? rgb(255, 250, 220) : rgb(170, 140, 90));
        if (on)
          drawGlow(r, mArt, x, y, 18.0f, rgb(255, 220, 150), 0.7f);
      }
    }
  }
  // The Totem's runners who aren't the player, standing on each other.
  for (const auto& b : s.bodies)
  {
    if (b.active)
      continue;
    const auto& ca = mArt.runner(characterByIndex(b.runner));
    const float x = (float(b.x) + 1.5f) * kCellPx - camX, y = float(b.y + 1) * kCellPx - camY;
    DrawOpts o;
    if (b.on >= 0)
      o.angle = std::sin(float(frame) * 0.1f + float(b.runner)) * 3.0f; // keeping their balance
    else
    {
      // On their own: a shadow under them.
      DrawOpts sh;
      sh.tint = rgb(0, 0, 0);
      sh.alpha = 0.35f;
      sh.scale = 2.4f;
      r.draw(mArt.dot, x, y - 2.0f, sh);
    }
    const Sprite& spr = b.on >= 0 ? ca.lookUp : ca.idle[std::size_t((frame / 30 + b.runner) % 2)];
    r.draw(spr.get(1), x, y, o);
  }
}

// --- The runner ----------------------------------------------------------------------------------------

bool drawWardrobeRunner(Renderer& r, const World& w, const Art& art, const Texture& tex, float x, float y, const DrawOpts& o, int frame)
{
  const auto& s = w.wardrobe();
  const auto& p = w.player();
  if (s.totem || w.player().state == PlayerState::Teleporting)
    return false;
  if (s.boothT > 0 && s.booth >= 0 && kBoothCurtain - s.boothT >= 1)
    return true; // changing behind the booth's curtain
  if (s.sit > 0)
  {
    // On the stool at the bulb mirror, facing it.
    r.draw(baked(art, r, "stool", 64, 44, 32.0f, 42.0f, paintStool), x, y);
    const auto& ca = art.runner(w.character());
    r.draw(ca.crouch.get(p.facing), x, y - 26.0f, o);
    return true;
  }
  if (s.suit)
  {
    const int v = suitPose(p);
    const int ti = themeIndexForKey(w.level().themeKey.c_str());
    const Theme& theme = themeByIndex(ti >= 0 ? ti : 0);
    DrawOpts so = o;
    so.angle = 0.0f;
    r.draw(styledEnemySprite(art, r, theme, "gorgazilla_worn", v, 0, 4, 6).get(p.facing), x, y, so);
    if (p.mercy > 0 && p.mercy <= 10)
    {
      DrawOpts f = so;
      f.blend = Blend::Add;
      f.alpha = 0.4f;
      r.draw(styledEnemySprite(art, r, theme, "gorgazilla_worn", v, 0, 4, 6).get(p.facing), x, y, f);
    }
    return true;
  }
  if (s.toon)
  {
    // The rubber-hose cartoon: inked round, bouncing as it goes, a wobble.
    const bool moving = p.visual == PlayerVisual::Walking;
    const float bounce = moving ? std::fabs(std::sin(float(frame) * 0.35f)) * 10.0f : std::fabs(std::sin(float(frame) * 0.12f)) * 3.0f;
    DrawOpts ink = o;
    ink.tint = rgb(16, 10, 16);
    ink.angle = o.angle + (moving ? std::sin(float(frame) * 0.35f) * 5.0f : 0.0f);
    for (const auto& d : {std::array<float, 2>{-6, 0}, {6, 0}, {0, -6}, {0, 6}, {-4, -4}, {4, -4}, {-4, 4}, {4, 4}})
      r.draw(tex, x + d[0], y - bounce + d[1], ink);
    DrawOpts body = o;
    body.angle = ink.angle;
    body.tint = rgb(255, 246, 232);
    r.draw(tex, x, y - bounce, body);
    // Flat, bright cartoon paint over the shading.
    DrawOpts flat = body;
    flat.blend = Blend::Add;
    flat.tint = rgb(70, 64, 56);
    flat.alpha = 0.8f * o.alpha;
    r.draw(tex, x, y - bounce, flat);
    if (moving)
    {
      // Speed lines behind, and puffs of dust kicked up at each bounce.
      const float back = -float(p.facing);
      for (int k = 0; k < 3; ++k)
      {
        const float ly = y - 60.0f - bounce - float(k) * 30.0f, lx = x + back * (46.0f + float((frame + k * 3) % 6) * 2.0f);
        r.drawLine(lx, ly, lx + back * (26.0f - float(k) * 4.0f), ly, 3.0f, rgb(16, 10, 16));
      }
      if (bounce < 3.0f)
        for (int k = 0; k < 2; ++k)
        {
          const float px = x + back * (24.0f + float(k) * 18.0f), py = y - 6.0f - float(k) * 4.0f;
          drawGlow(r, art, px, py, 14.0f - float(k) * 3.0f, rgb(255, 255, 255), 0.9f);
        }
    }
    // Running on air: the legs a blur in a dust wheel.
    if (s.toonAir > 0 && !w.map().onSolidGround(p.box()))
    {
      for (int k = 0; k < 6; ++k)
      {
        const float a = float(frame) * 0.9f + float(k) * 1.047f;
        r.drawLine(x + std::cos(a) * 18.0f, y - 12.0f + std::sin(a) * 10.0f, x + std::cos(a + 0.5f) * 18.0f,
          y - 12.0f + std::sin(a + 0.5f) * 10.0f, 3.0f, rgba(255, 255, 255, 180));
      }
      if (s.toonAir >= kToonAirRun)
      {
        // Uh-oh: a little sign held up.
        const float sx = x - float(p.facing) * 50.0f, sy = y - 200.0f;
        r.drawLine(sx, sy + 20.0f, sx, sy + 60.0f, 3.0f, rgb(110, 70, 40));
        r.fillRect(sx - 34.0f, sy - 6.0f, 68.0f, 28.0f, rgb(250, 244, 226));
        r.drawText("UH-OH", sx, sy - 2.0f, {16.0f, rgb(30, 20, 20), 0}, Align::Center);
      }
    }
    if (p.mercy > 0 && p.mercy <= 10)
    {
      DrawOpts f = body;
      f.blend = Blend::Add;
      f.alpha = 0.4f;
      r.draw(tex, x, y - bounce, f);
    }
    return true;
  }
  return false;
}

// --- In front ------------------------------------------------------------------------------------------

void World::drawWardrobeFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)alpha;
  const auto& s = mWardrobe;
  const auto& p = mPlayer;
  // Decoys: a mannequin dressed as the runner, on its stand; lobbed end
  // over end; landed, in a spotlight (that is what everyone goes for).
  for (std::size_t i = 0; i < s.decoys.size(); ++i)
  {
    const auto& d = s.decoys[i];
    const auto& ca = mArt.runner(characterByIndex(d.look));
    const float x = (d.x + 1.5f) * kCellPx - camX, y = (d.y + 1.0f) * kCellPx - camY;
    if (offScreen(x - 100.0f, y - 200.0f, 200.0f, 220.0f))
      continue;
    DrawOpts o;
    o.tint = rgb(255, 232, 206);
    o.alpha = d.life < 15 && (frame / 3) % 2 ? 0.45f : 1.0f;
    if (!d.landed)
    {
      o.angle = float(frame * 18 % 360) * float(d.facing);
      r.draw(ca.jump.get(d.facing), x, y - 70.0f, o);
      for (int k = 1; k <= 3; ++k)
        r.drawLine(x - d.vx * float(k) * 14.0f, y - 70.0f - d.vy * float(k) * 10.0f, x - d.vx * float(k) * 22.0f,
          y - 70.0f - d.vy * float(k) * 16.0f, 3.0f, rgba(255, 255, 255, 120 - k * 30));
      continue;
    }
    // The spotlight from the flies: a cone widening to a pool on the floor.
    const float pulse = 0.85f + 0.15f * std::sin(float(frame) * 0.2f);
    for (int k = 0; k < 12; ++k)
    {
      const float ya = y - 700.0f + float(k) * 58.0f, wk = 50.0f + float(k) * 11.0f;
      r.drawLine(x, ya, x, ya + 58.0f, wk, rgba(255, 236, 190, int(34.0f * pulse)), Blend::Add);
    }
    drawGlow(r, mArt, x, y - 4.0f, 96.0f, rgb(255, 236, 190), 0.55f * pulse);
    // The stand: a round brass base and a short pole, the figure on it.
    r.fillRect(x - 30.0f, y - 8.0f, 60.0f, 8.0f, kBrass);
    r.fillRect(x - 30.0f, y - 8.0f, 60.0f, 2.0f, kBrassHi);
    r.fillRect(x - 30.0f, y - 2.0f, 60.0f, 2.0f, kBrassLo);
    r.fillRect(x - 4.0f, y - 24.0f, 8.0f, 16.0f, kBrassLo);
    const Texture& fig = ca.idle[0].get(d.facing);
    r.draw(fig, x, y - 22.0f, o);
    // A shop-window sheen: paler than the real runner.
    DrawOpts sheen = o;
    sheen.tint = rgb(90, 70, 50);
    sheen.blend = Blend::Add;
    sheen.alpha = 0.7f * o.alpha;
    r.draw(fig, x, y - 22.0f, sheen);
    // The price tag on a string from its hand.
    const float tx = x - float(d.facing) * 40.0f;
    r.drawLine(x - float(d.facing) * 22.0f, y - 92.0f, tx, y - 66.0f, 1.5f, rgb(240, 236, 220));
    r.fillRect(tx - 18.0f, y - 66.0f, 36.0f, 18.0f, rgb(250, 244, 226));
    r.fillRect(tx - 18.0f, y - 66.0f, 36.0f, 2.0f, rgb(200, 60, 60));
    r.drawText("SALE", tx, y - 63.0f, {12.0f, rgb(170, 30, 40), 0}, Align::Center);
    if (d.hp < kDecoyHp)
      for (int k = 0; k < kDecoyHp - d.hp; ++k)
        r.drawLine(x - 16.0f + float(k) * 9.0f, y - 120.0f + float(k % 2) * 30.0f, x - 8.0f + float(k) * 9.0f, y - 104.0f + float(k % 2) * 30.0f,
          2.0f, rgb(60, 30, 20));
  }
  // Enemies.
  for (std::size_t i = 0; i < mEnemies.size(); ++i)
  {
    const Enemy& e = mEnemies[i];
    if (!e.alive || e.hidden)
      continue;
    const float cx = (e.drawX + float(e.w) * 0.5f) * kCellPx - camX, foot = (e.drawY + 1.0f) * kCellPx - camY;
    const float h = float(e.h) * kCellPx;
    if (offScreen(cx - 140.0f, foot - h - 120.0f, 280.0f, h + 160.0f))
      continue;
    const int dir = e.dir < 0 ? -1 : 1;
    const Sprite* sp = nullptr;
    float ox = 0.0f, oy = 0.0f;
    if (e.kind == EnemyKind::Mannequin)
    {
      const Mannequin* m = nullptr;
      for (const auto& k : s.mannequins)
        if (k.enemy == int(i))
          m = &k;
      const bool twitch = m && m->twitch > 0;
      const int pose = m ? m->pose % 4 : 0;
      sp = &styledEnemySprite(mArt, r, mTheme, "mannequin", pose + (twitch ? 4 : 0), int(i) % 3, e.w, e.h);
      if (twitch)
      {
        ox = (frame / 2) % 2 ? 3.0f : -3.0f;
        // Creak marks at its joints.
        for (int k = 0; k < 3; ++k)
        {
          const float jx = cx + float(dir) * (k == 0 ? 18.0f : -14.0f), jy = foot - (k == 2 ? 60.0f : 118.0f);
          const float a = float(frame + k * 5) * 0.8f;
          r.drawLine(jx + std::cos(a) * 10.0f, jy + std::sin(a) * 6.0f, jx + std::cos(a) * 18.0f, jy + std::sin(a) * 10.0f, 2.5f,
            rgb(255, 240, 200));
        }
        if ((frame / 4) % 2 == 0)
          r.drawText("creak", cx + float(dir) * 40.0f, foot - h - 26.0f, {16.0f, rgb(255, 230, 190), kHudInk, true}, Align::Center);
      }
      else if (m && !m->frozen)
        oy = -std::fabs(std::sin(float(e.drawX) * 1.5f)) * 3.0f; // a stiff hop each step
    }
    else if (e.kind == EnemyKind::PowderPuffer)
    {
      const Puffer* f = nullptr;
      for (const auto& k : s.puffers)
        if (k.enemy == int(i))
          f = &k;
      int v = 0;
      if (e.tell > 0)
        v = 1;
      else if (f && f->cloud > kPuffCloud - 10)
        v = 2;
      if (e.carrier)
        v += 3;
      const int blink = (frame + int(i) * 37) % 150 < 6 ? 1 : 0;
      sp = &styledEnemySprite(mArt, r, mTheme, "powder_puffer", v, blink, e.w, e.h);
      drawGlow(r, mArt, cx, foot - h * 0.5f, 90.0f, e.carrier ? rgb(150, 255, 110) : rgb(255, 170, 210), e.carrier ? 0.3f : 0.2f);
      if (e.tell > 0)
        ox = (frame / 2) % 2 ? 2.0f : -2.0f;
    }
    else if (e.kind == EnemyKind::RackRunner)
    {
      const RackRunner* k = nullptr;
      for (const auto& q : s.rackRunners)
        if (q.enemy == int(i))
          k = &q;
      int v = 0;
      if (e.tell > 0)
        v = 1;
      else if (k && k->roll != 0)
        v = 2;
      const int f = v > 0 ? (frame / 3) % 2 : 0;
      sp = &styledEnemySprite(mArt, r, mTheme, "rack_runner", v, f, e.w, e.h);
      if (v == 1)
        ox = (frame / 2) % 2 ? 2.0f : -2.0f;
      if (v == 2)
        for (int q = 0; q < 4; ++q)
          r.drawLine(cx - float(dir) * (110.0f + float(q) * 16.0f), foot - 30.0f - float(q) * 36.0f, cx - float(dir) * (150.0f + float(q) * 16.0f),
            foot - 30.0f - float(q) * 36.0f, 4.0f, rgba(255, 240, 220, 140));
      if (k && k->stopped > 0)
      {
        // Held by a decoy: steaming.
        for (int q = 0; q < 2; ++q)
          drawGlow(r, mArt, cx + float(q * 2 - 1) * 30.0f, foot - h - 10.0f - float((frame + q * 10) % 20), 20.0f, rgb(255, 255, 255), 0.4f);
      }
    }
    else
      continue;
    r.draw(sp->get(dir), cx + ox, foot + oy);
    if (e.kind == EnemyKind::PowderPuffer && e.carrier)
      r.draw(styledEnemySprite(mArt, r, mTheme, "powder_puffer", 6, dir < 0 ? 1 : 0, e.w, e.h).get(1), cx + ox, foot + oy);
    if (e.flash > 0)
    {
      DrawOpts fo;
      fo.blend = Blend::Add;
      fo.alpha = float(e.flash) / 8.0f;
      r.draw(sp->get(dir), cx + ox, foot + oy, fo);
    }
  }
  // Powder clouds: pink-white (sickly green from the carrier), billowing out
  // and thinning away.
  for (std::size_t i = 0; i < s.puffers.size(); ++i)
  {
    const auto& f = s.puffers[i];
    if (f.cloud <= 0)
      continue;
    const bool carrier = mEnemies[std::size_t(f.enemy)].carrier;
    const float grow = std::min(1.0f, float(kPuffCloud - f.cloud + 1) / 8.0f);
    const float a = std::min(1.0f, float(f.cloud) / 20.0f) * 0.85f;
    const float x = float(f.cloudX) * kCellPx - camX, y = float(f.cloudY) * kCellPx - camY;
    const float w = float(kPuffW) * kCellPx, hh = float(kPuffH) * kCellPx;
    if (offScreen(x, y, w, hh))
      continue;
    const float gx = f.cloudDir > 0 ? x : x + w * (1.0f - grow);
    drawPuffs(r, mArt, gx, y + hh * (1.0f - grow) * 0.5f, w * grow, hh * grow, carrier ? kSickFog : kFog, a, int(i) * 17 + 3, frame);
    if (carrier)
      for (int k = 0; k < 6; ++k)
      {
        const std::uint32_t hs = hash2(int(i), k + frame / 8);
        drawGlow(r, mArt, gx + w * grow * unit(hs), y + hh * unit(hs >> 9), 6.0f, rgb(120, 255, 80), a);
      }
  }
  // Inside a cloud: the room goes white-pink and only the runner's outline
  // shows.
  if (s.puffed > 0)
  {
    bool sick = false;
    for (const auto& f : s.puffers)
      if (f.cloud > 0 && mEnemies[std::size_t(f.enemy)].carrier && CellBox{f.cloudX, f.cloudY, kPuffW, kPuffH}.intersects(p.box()))
        sick = true;
    const Color fog = sick ? kSickFog : kFog;
    r.fillRect(0, 0, float(kScreenW), float(kScreenH), rgba(redOf(fog), greenOf(fog), blueOf(fog), 225));
    drawPuffs(r, mArt, 0.0f, 0.0f, float(kScreenW), float(kScreenH), lerpColor(fog, rgb(255, 255, 255), 0.4f), 0.5f, 99, frame);
    const float x = lerpCells(p.prevX, p.x, alpha) + 1.5f * kCellPx - camX, y = lerpCells(p.prevY + 1, p.y + 1, alpha) - camY;
    const Texture& tex = s.suit ? styledEnemySprite(mArt, r, mTheme, "gorgazilla_worn", suitPose(p), 0, 4, 6).get(p.facing)
                                : runnerSprite(mArt, *this, frame).get(p.facing);
    // Only the runner's shape shows, a shadow in the powder.
    DrawOpts sil;
    sil.tint = rgb(0, 0, 0);
    sil.alpha = 0.5f;
    r.draw(tex, x, y, sil);
  }
  // The costume curtain over the OUT OF ORDER booth; see-through when the
  // runner is behind it.
  for (const auto& c : s.curtains)
  {
    const bool inside = p.box().intersects(c);
    const float x = float(c.x) * kCellPx - camX, y = float(c.y) * kCellPx - camY, w = float(c.w) * kCellPx, h = float(c.h) * kCellPx;
    if (offScreen(x, y, w, h))
      continue;
    const int pleats = int(w / 32.0f);
    const int a = inside ? 90 : 255;
    for (int k = 0; k < pleats; ++k)
    {
      const float px = x + float(k) * w / float(pleats), pw = w / float(pleats) + 1.0f;
      const float sway = std::sin(float(frame) * 0.04f + float(k)) * 2.0f;
      r.fillRect(px, y, pw, h, rgba(150, 22, 42, a));
      r.fillRect(px + pw * 0.3f + sway, y, pw * 0.25f, h, rgba(210, 64, 78, a));
      r.fillRect(px + pw * 0.8f, y, pw * 0.2f, h, rgba(70, 6, 20, a));
    }
    r.fillRect(x - 6.0f, y - 10.0f, w + 12.0f, 12.0f, rgba(214, 170, 80, a));
    r.fillRect(x, y + h - 8.0f, w, 8.0f, rgba(214, 170, 80, a));
  }
  // The booth curtain drawn shut in front of the runner while they change.
  if (s.boothT > 0 && s.booth >= 0)
  {
    const auto& b = s.booths[std::size_t(s.booth)];
    const float left = float(b.x) * kCellPx - camX, foot = float(b.y + 1) * kCellPx - camY;
    const int done = kBoothCurtain - s.boothT;
    const float closed = std::min(1.0f, 0.18f + 0.82f * float(done + 1) / 3.0f);
    const float sway = std::sin(float(frame) * 0.9f) * (s.boothT > 3 ? 0.6f : 1.0f);
    const int style = b.runner == -2 ? 1 : 0;
    const int cq = int(closed * 10.0f + 0.5f), sq = int(std::lround(sway * 2.0f));
    r.draw(baked(mArt, r, "curtain/" + std::to_string(style) + "/" + std::to_string(cq) + "/" + std::to_string(sq), 112, 180, 0.0f, 0.0f,
             [cq, sq, style](cairo_t* cr) { paintCurtain(cr, cq / 10.0, sq / 2.0, style); }),
      left + 12.0f, foot - 176.0f);
    // Clothes flying over the top.
    if (done > 3 && done < 10)
    {
      const float u = float(done - 3) / 6.0f;
      r.drawLine(left + 40.0f + u * 30.0f, foot - 190.0f - u * 40.0f, left + 70.0f + u * 30.0f, foot - 200.0f - u * 40.0f, 8.0f,
        runnerTone(std::max(0, s.boothTo)));
    }
  }
  // The mustache, and the powder puff dabbing while they sit.
  if ((s.mustache || s.sit > 0) && p.state != PlayerState::Dying && !s.suit)
  {
    const float x = lerpCells(p.prevX, p.x, alpha) + 1.5f * kCellPx - camX, y = lerpCells(p.prevY + 1, p.y + 1, alpha) - camY;
    auto face = faceAt(characterIndex(), s.sit > 0 ? PlayerVisual::Crouching : p.visual);
    const float fx = x + float(p.facing) * face[0], fy = y + face[1] - (s.sit > 0 ? 26.0f : 0.0f);
    if (s.sit > 0)
    {
      const float u = float(kSitFrames - s.sit) / float(kSitFrames);
      const float dab = std::fabs(std::sin(u * 18.0f)) * 14.0f;
      DrawOpts po;
      po.tint = rgb(255, 190, 210);
      po.scale = 1.1f;
      r.draw(mArt.dot, fx + float(p.facing) * (16.0f + dab), fy - 10.0f, po);
      drawGlow(r, mArt, fx + float(p.facing) * 10.0f, fy - 6.0f, 30.0f, rgb(255, 230, 240), 0.4f * (1.0f - u));
    }
    else
    {
      DrawOpts mo;
      mo.angle = p.visual == PlayerVisual::Walking ? std::sin(float(frame) * 0.4f) * 4.0f : 0.0f;
      r.draw(bakedSprite(mArt, r, "mustache", 60, 24, 30.0f, 8.0f, paintMustache).get(p.facing), fx, fy, mo);
    }
  }
}

// --- The HUD -------------------------------------------------------------------------------------------

void World::drawWardrobeHud(Renderer& r, int frame) const
{
  const auto& s = mWardrobe;
  if (s.totem)
    return;
  // Each runner's hearts under the health panel: a portrait, the name in
  // their colour, a row of little hearts; the runner you are lit up.
  const float top = 84.0f;
  r.fillRect(12.0f, top, 430.0f, 48.0f, rgba(20, 8, 14, 170));
  r.fillRect(12.0f, top, 430.0f, 2.0f, rgba(214, 170, 80, 200));
  for (int i : {0, 1, 2})
  {
    const float x = 18.0f + float(i) * 142.0f;
    const bool me = i == mCharacterIndex;
    const auto& def = characterByIndex(i);
    const int pool = me ? mPlayer.hp : s.pools[std::size_t(i)];
    const int hp = pool < 0 ? def.maxHp : pool;
    DrawOpts po;
    po.scale = 0.2f;
    po.alpha = me ? 1.0f : 0.55f;
    r.draw(mArt.portrait(def), x + 22.0f, top + 1.0f, po);
    if (me)
      r.fillRect(x, top + 44.0f, 136.0f, 3.0f, runnerTone(i));
    r.drawText(runnerWord(i), x + 46.0f, top + 4.0f, {13.0f, me ? lerpColor(runnerTone(i), rgb(255, 255, 255), 0.4f) : rgb(150, 140, 140), kHudInk});
    for (int k = 0; k < def.maxHp; ++k)
    {
      DrawOpts ho;
      ho.scale = 0.36f;
      ho.alpha = me ? 1.0f : 0.6f;
      if (me && hp <= 2 && k < hp && (frame / 12) % 2 == 0)
        ho.tint = rgb(255, 255, 255);
      r.draw(k < hp ? mArt.heartFull : mArt.heartEmpty, x + 46.0f + float(k) * 10.0f, top + 22.0f, ho);
    }
  }
}

// --- The Decoy Launcher's icon -------------------------------------------------------------------------

void drawDecoyIcon(Renderer& r, const Art& art, float x, float y, float scale, int frame)
{
  DrawOpts o;
  o.scale = scale;
  const Texture& t = baked(art, r, "decoyicon", 64, 64, 0.0f, 0.0f, paintDecoyIcon);
  // The little head bobs in and out of the bell.
  r.draw(t, x, y + std::sin(float(frame) * 0.1f) * 1.5f * scale, o);
}

} // namespace gr
