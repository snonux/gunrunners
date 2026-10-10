// Level 26, Kaiju Tonight: drawing. See world_city.cpp for the logic and
// world_topdown.cpp for the Top-Down Rampage bonus.
//
// Behind the runner (drawCityBack): the Toy Jet formations growing out of
// the painted sky, the studio's underside (the subway's tiled walls, its
// rails, the storeroom's TV set), the power lines on their pylons (snapped
// and sparking once touched), the stuntman's trailer with his EMPLOYEE OF
// THE MONTH photo, the model bus on route 42, the model buildings (houses,
// the chrome diner and its three figurines, the water tower, the office
// blocks, the cooling towers, the hangars, the station, the Halcyon tower)
// standing, cracking under a runner's feet, scorched by the breath,
// crumbling to rubble; the towers creaking, falling and lying as bridges;
// the ocean liner and the harbor maser dish on its deck, Gorgazilla, the
// model water of the canal and the bay, the arena's gate, the full heal and
// the blacked-out sectors. In front (drawCityFront): the Atomic Breath, the
// Maser beams and their rings, the tail swipe, the dust of a fall.
// drawCityHud: the DAMAGE meter and Gorgazilla's bar. In the Top-Down
// Rampage the camera looks straight down: the grid city's streets, lots and
// parks, the roofs, the tanks and the runner from above (drawTopRunner).

#include "game/world.hpp"

#include "assets/art.hpp"
#include "assets/enemy_art.hpp"
#include "assets/enemy_art_city.hpp"
#include "base/math.hpp"
#include "render/vector.hpp"

#include <SDL.h>

#include <algorithm>
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
constexpr Color kInk = rgb(20, 18, 28);

bool offScreen(float x, float y, float w, float h)
{
  return x > float(kScreenW) + 96.0f || x + w < -96.0f || y > float(kScreenH) + 96.0f || y + h < -96.0f;
}

// A texture baked once and kept in the Art's sprite cache under `key`.
const Texture& baked(const Art& art, const Renderer& r, const std::string& key, int w, int h, float ax, float ay,
  const std::function<void(cairo_t*)>& paint)
{
  const std::string id = "~city/" + key;
  auto it = art.styled.find(id);
  if (it != art.styled.end())
    return it->second.facing[0];
  VectorImage img(std::max(1, w), std::max(1, h));
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
  cairo_scale(cr, rx, ry);
  cairo_new_sub_path(cr);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_restore(cr);
}

void ink(cairo_t* cr, double line = 1.6, double a = 1.0)
{
  setRgba(cr, kInk, a);
  cairo_set_line_width(cr, line);
  cairo_stroke(cr);
}

void stroke(cairo_t* cr, Color c, double w, double a = 1.0)
{
  setRgba(cr, c, a);
  cairo_set_line_width(cr, w);
  cairo_stroke(cr);
}

// Fills the path with a linear gradient through `stops`; inks it if line > 0.
void fillLinear(cairo_t* cr, double x0, double y0, double x1, double y1, std::initializer_list<std::pair<double, Color>> stops,
  double line = 0.0)
{
  cairo_pattern_t* g = cairo_pattern_create_linear(x0, y0, x1, y1);
  for (const auto& s : stops)
    stop(g, s.first, s.second);
  cairo_set_source(cr, g);
  cairo_pattern_destroy(g);
  if (line > 0.0)
  {
    cairo_fill_preserve(cr);
    ink(cr, line);
  }
  else
    cairo_fill(cr);
}

void glow(cairo_t* cr, double x, double y, double r, Color c, double a)
{
  circle(cr, x, y, r);
  cairo_pattern_t* g = cairo_pattern_create_radial(x, y, 0, x, y, r);
  stop(g, 0, c, a);
  stop(g, 0.45, c, a * 0.4);
  stop(g, 1, c, 0.0);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
}

void text(cairo_t* cr, const char* s, double x, double y, double size, Color c, bool centre = true, bool italic = false)
{
  selectGameFont(cr, italic);
  cairo_set_font_size(cr, size);
  cairo_text_extents_t te;
  cairo_text_extents(cr, s, &te);
  cairo_move_to(cr, centre ? x - te.x_advance / 2 : x, y);
  cairo_text_path(cr, s);
  setColor(cr, c);
  cairo_fill(cr);
}

std::uint32_t seedOf(const std::string& s)
{
  std::uint32_t h = 2166136261u;
  for (const char ch : s)
    h = (h ^ std::uint32_t(static_cast<unsigned char>(ch))) * 16777619u;
  h ^= h >> 15; // mixed, so the low bits vary too
  h *= 0x2c1b3c6du;
  h ^= h >> 12;
  return h ? h : 1u;
}

// --- The model buildings -------------------------------------------------------------

// What a building looks like, by its id (falling back on its kind).
enum class Look
{
  House,
  Diner,
  WaterTower,
  Office,
  Cooling,
  Hangar,
  Station,
  Halcyon,
  DecoTower,
  Lighthouse,
  Control,
};

Look lookOf(const Building& b)
{
  const auto has = [&](const char* p) { return b.id.find(p) != std::string::npos; };
  switch (b.kind)
  {
    case BuildingKind::House:
      return has("diner") ? Look::Diner : Look::House;
    case BuildingKind::Roof:
      return Look::Station;
    case BuildingKind::Prize:
      return Look::Halcyon;
    case BuildingKind::Tower:
      return has("harbor") || has("light") ? Look::Lighthouse : (has("control") ? Look::Control : Look::DecoTower);
    case BuildingKind::Block:
      break;
  }
  if (has("water"))
    return Look::WaterTower;
  if (has("cool"))
    return Look::Cooling;
  if (has("hangar"))
    return Look::Hangar;
  return Look::Office;
}

constexpr Color kWarmLit = rgb(255, 214, 120), kDarkPane = rgb(40, 46, 66);

// A window pane: lit warm (with a soft spill), or dark with a sheen.
void pane(cairo_t* cr, double x, double y, double w, double h, bool lit, double frame = 1.6)
{
  cairo_rectangle(cr, x, y, w, h);
  if (lit)
  {
    fillLinear(cr, x, y, x, y + h, {{0.0, lighten(kWarmLit, 0.3f)}, {1.0, rgb(240, 170, 80)}});
    glow(cr, x + w * 0.5, y + h * 0.5, std::max(w, h) * 0.9, kWarmLit, 0.25);
  }
  else
  {
    fillLinear(cr, x, y, x + w, y + h, {{0.0, rgb(80, 96, 130)}, {0.5, kDarkPane}, {1.0, rgb(24, 26, 40)}});
    cairo_move_to(cr, x + w * 0.2, y + h);
    cairo_line_to(cr, x + w * 0.7, y);
    stroke(cr, rgb(200, 210, 255), 1.0, 0.2);
  }
  cairo_rectangle(cr, x, y, w, h);
  stroke(cr, rgb(240, 236, 226), frame, 0.9);
}

bool litWindow(std::uint32_t seed, int i, bool lit)
{
  return lit && ((seed >> (i % 23)) ^ std::uint32_t(i * 2654435761u >> 7)) % 5u < 3u;
}

void paintHouse(cairo_t* cr, double w, double h, std::uint32_t seed, bool lit)
{
  static const Color kWalls[5] = {rgb(176, 222, 196), rgb(242, 196, 196), rgb(246, 226, 156), rgb(176, 208, 238),
    rgb(212, 192, 234)};
  static const Color kRoofs[3] = {rgb(184, 72, 62), rgb(90, 98, 124), rgb(86, 126, 86)};
  const Color wall = kWalls[seed % 5u], roof = kRoofs[(seed / 7u) % 3u];
  // The chimney behind the roof.
  cairo_rectangle(cr, w * 0.66, h * 0.08, w * 0.1, h * 0.26);
  fillLinear(cr, 0, 0, w, 0, {{0.6, rgb(180, 90, 70)}, {0.8, rgb(120, 56, 44)}}, 1.4);
  // The walls.
  cairo_rectangle(cr, w * 0.08, h * 0.4, w * 0.84, h * 0.6);
  fillLinear(cr, 0, h * 0.4, 0, h, {{0.0, lighten(wall, 0.15f)}, {1.0, darken(wall, 0.15f)}}, 1.6);
  // Clapboard lines.
  for (double y = h * 0.48; y < h - 2; y += 5)
  {
    cairo_move_to(cr, w * 0.08, y);
    cairo_line_to(cr, w * 0.92, y);
  }
  stroke(cr, darken(wall, 0.25f), 0.7, 0.6);
  // The roof: a gable with overhang, shingles.
  cairo_move_to(cr, w * 0.0, h * 0.44);
  cairo_line_to(cr, w * 0.5, h * 0.04);
  cairo_line_to(cr, w * 1.0, h * 0.44);
  cairo_close_path(cr);
  fillLinear(cr, 0, h * 0.04, 0, h * 0.44, {{0.0, lighten(roof, 0.2f)}, {1.0, darken(roof, 0.25f)}}, 1.6);
  for (int i = 1; i < 4; ++i)
  {
    const double yy = h * (0.04 + 0.1 * i), half = w * 0.5 * (yy - h * 0.04) / (h * 0.4);
    cairo_move_to(cr, w * 0.5 - half, yy);
    cairo_line_to(cr, w * 0.5 + half, yy);
  }
  stroke(cr, darken(roof, 0.4f), 0.8, 0.6);
  // The door and a window.
  cairo_rectangle(cr, w * 0.18, h * 0.66, w * 0.18, h * 0.34);
  fillLinear(cr, 0, h * 0.66, 0, h, {{0.0, rgb(150, 80, 60)}, {1.0, rgb(90, 46, 34)}}, 1.2);
  circle(cr, w * 0.32, h * 0.84, 1.2);
  setColor(cr, rgb(250, 220, 120));
  cairo_fill(cr);
  pane(cr, w * 0.5, h * 0.58, w * 0.3, h * 0.2, litWindow(seed, 1, lit));
  cairo_move_to(cr, w * 0.65, h * 0.58);
  cairo_line_to(cr, w * 0.65, h * 0.78);
  cairo_move_to(cr, w * 0.5, h * 0.68);
  cairo_line_to(cr, w * 0.8, h * 0.68);
  stroke(cr, rgb(240, 236, 226), 1.2);
  // A round window in the gable.
  circle(cr, w * 0.5, h * 0.27, w * 0.06);
  setColor(cr, litWindow(seed, 3, lit) ? kWarmLit : kDarkPane);
  cairo_fill_preserve(cr);
  stroke(cr, rgb(240, 236, 226), 1.2);
  // A shrub.
  for (int i = 0; i < 3; ++i)
    circle(cr, w * (0.82 + i * 0.05), h * 0.94 - i % 2 * 2, w * 0.07);
  setColor(cr, rgb(60, 110, 60));
  cairo_fill(cr);
}

// The chrome diner: ribbed stainless steel, a red enamel stripe, a row of
// windows with booths (the figurines are drawn over, live), the DINER sign.
void paintDiner(cairo_t* cr, double w, double h, bool lit)
{
  // The sign on its frame.
  cairo_rectangle(cr, w * 0.28, 1, w * 0.36, h * 0.2);
  fillLinear(cr, 0, 1, 0, h * 0.2, {{0.0, rgb(40, 36, 50)}, {1.0, rgb(20, 18, 26)}}, 1.4);
  if (lit)
    glow(cr, w * 0.46, h * 0.12, w * 0.2, rgb(255, 80, 90), 0.35);
  text(cr, "DINER", w * 0.46, h * 0.18, h * 0.15, lit ? rgb(255, 120, 130) : rgb(110, 60, 70));
  // The body: rounded ends.
  roundedRect(cr, w * 0.02, h * 0.22, w * 0.96, h * 0.78, h * 0.18);
  fillLinear(cr, 0, h * 0.22, 0, h, {{0.0, rgb(236, 240, 246)}, {0.35, rgb(170, 178, 192)}, {0.5, rgb(240, 244, 250)}, {1.0, rgb(120, 126, 140)}}, 1.8);
  for (double y = h * 0.74; y < h - 3; y += 3.2)
  {
    cairo_move_to(cr, w * 0.04, y);
    cairo_line_to(cr, w * 0.96, y);
  }
  stroke(cr, rgb(110, 116, 130), 0.7, 0.6);
  // The red stripe.
  cairo_rectangle(cr, w * 0.03, h * 0.64, w * 0.94, h * 0.08);
  fillLinear(cr, 0, h * 0.64, 0, h * 0.72, {{0.0, rgb(240, 70, 70)}, {1.0, rgb(170, 30, 40)}});
  // The windows: one long run, mullioned; the booths' red seat backs.
  cairo_rectangle(cr, w * 0.08, h * 0.3, w * 0.66, h * 0.3);
  fillLinear(cr, 0, h * 0.3, 0, h * 0.6, {{0.0, lit ? rgb(255, 236, 180) : rgb(70, 80, 100)}, {1.0, lit ? rgb(240, 180, 100) : rgb(36, 40, 56)}});
  for (int i = 0; i < 4; ++i)
  {
    roundedRect(cr, w * (0.12 + i * 0.16), h * 0.46, w * 0.03, h * 0.14, 1.5);
    roundedRect(cr, w * (0.2 + i * 0.16), h * 0.46, w * 0.03, h * 0.14, 1.5);
  }
  setColor(cr, rgb(200, 40, 50));
  cairo_fill(cr);
  for (int i = 0; i <= 6; ++i)
  {
    cairo_move_to(cr, w * (0.08 + i * 0.11), h * 0.3);
    cairo_line_to(cr, w * (0.08 + i * 0.11), h * 0.6);
  }
  stroke(cr, rgb(220, 226, 236), 1.6);
  cairo_rectangle(cr, w * 0.08, h * 0.3, w * 0.66, h * 0.3);
  stroke(cr, rgb(220, 226, 236), 1.8);
  // The door with its porthole, the steps.
  roundedRect(cr, w * 0.8, h * 0.3, w * 0.09, h * 0.62, 3);
  fillLinear(cr, 0, h * 0.3, 0, h, {{0.0, rgb(220, 226, 236)}, {1.0, rgb(130, 136, 150)}}, 1.4);
  circle(cr, w * 0.845, h * 0.44, h * 0.07);
  setColor(cr, lit ? kWarmLit : kDarkPane);
  cairo_fill_preserve(cr);
  ink(cr, 1.0);
  cairo_rectangle(cr, w * 0.78, h * 0.92, w * 0.13, h * 0.08);
  fillLinear(cr, 0, h * 0.92, 0, h, {{0.0, rgb(200, 200, 200)}, {1.0, rgb(120, 120, 120)}}, 1.0);
}

void paintWaterTower(cairo_t* cr, double w, double h)
{
  const Color wood = rgb(176, 120, 76), iron = rgb(70, 72, 80);
  // The legs, the back pair first, and their cross-bracing.
  for (const double lx : {0.3, 0.7})
  {
    cairo_move_to(cr, w * lx, h * 0.48);
    cairo_line_to(cr, w * (lx + (lx < 0.5 ? -0.06 : 0.06)), h);
  }
  stroke(cr, darken(iron, 0.3f), 5.0);
  for (int k = 0; k < 2; ++k)
  {
    const double y0 = h * (0.52 + k * 0.24), y1 = y0 + h * 0.24;
    cairo_move_to(cr, w * 0.2, y0);
    cairo_line_to(cr, w * 0.8, y1);
    cairo_move_to(cr, w * 0.8, y0);
    cairo_line_to(cr, w * 0.2, y1);
  }
  stroke(cr, iron, 2.2);
  for (const double lx : {0.18, 0.82})
  {
    cairo_move_to(cr, w * lx, h * 0.48);
    cairo_line_to(cr, w * (lx + (lx < 0.5 ? -0.1 : 0.1)), h);
  }
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  stroke(cr, kInk, 9.0);
  for (const double lx : {0.18, 0.82})
  {
    cairo_move_to(cr, w * lx, h * 0.48);
    cairo_line_to(cr, w * (lx + (lx < 0.5 ? -0.1 : 0.1)), h);
  }
  stroke(cr, rgb(110, 112, 124), 6.0);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
  // A ladder up the side.
  for (double y = h * 0.5; y < h; y += 8)
  {
    cairo_move_to(cr, w * 0.62, y);
    cairo_line_to(cr, w * 0.68, y);
  }
  cairo_move_to(cr, w * 0.62, h * 0.48);
  cairo_line_to(cr, w * 0.62, h);
  cairo_move_to(cr, w * 0.68, h * 0.48);
  cairo_line_to(cr, w * 0.68, h);
  stroke(cr, rgb(150, 150, 160), 1.4);
  // The tank: wooden staves, iron hoops, the conical roof.
  cairo_rectangle(cr, w * 0.12, h * 0.16, w * 0.76, h * 0.34);
  fillLinear(cr, w * 0.12, 0, w * 0.88, 0, {{0.0, lighten(wood, 0.25f)}, {0.4, wood}, {1.0, darken(wood, 0.45f)}}, 1.8);
  for (int i = 1; i < 12; ++i)
  {
    cairo_move_to(cr, w * (0.12 + i * 0.0633), h * 0.16);
    cairo_line_to(cr, w * (0.12 + i * 0.0633), h * 0.5);
  }
  stroke(cr, darken(wood, 0.4f), 0.8, 0.6);
  for (const double y : {0.22, 0.33, 0.44})
  {
    cairo_rectangle(cr, w * 0.11, h * y, w * 0.78, 3);
    fillLinear(cr, 0, h * y, 0, h * y + 3, {{0.0, rgb(130, 130, 140)}, {1.0, rgb(50, 50, 58)}});
  }
  text(cr, "MODELVILLE", w * 0.5, h * 0.3, h * 0.055, rgb(250, 244, 220));
  cairo_move_to(cr, w * 0.08, h * 0.17);
  cairo_line_to(cr, w * 0.5, h * 0.0 + 2);
  cairo_line_to(cr, w * 0.92, h * 0.17);
  cairo_close_path(cr);
  fillLinear(cr, 0, 0, 0, h * 0.17, {{0.0, rgb(150, 70, 56)}, {1.0, rgb(96, 40, 32)}}, 1.8);
}

// An office block, one of four styles: 0 Art Deco limestone, 1 a glass
// curtain wall, 2 red brick with a HOTEL sign, 3 a concrete bank with strip
// windows. A shop front along the bottom, a parapet on top.
void paintOffice(cairo_t* cr, double w, double h, int style, std::uint32_t seed, bool lit)
{
  static const Color kFace[4] = {rgb(222, 208, 176), rgb(110, 160, 170), rgb(170, 84, 66), rgb(156, 164, 150)};
  const Color face = kFace[style & 3];
  cairo_rectangle(cr, 0, 0, w, h);
  fillLinear(cr, 0, 0, w, 0, {{0.0, lighten(face, 0.18f)}, {0.7, face}, {1.0, darken(face, 0.3f)}}, 1.8);
  // The windows.
  const double top = h * 0.1, bottom = h * 0.78;
  int n = 0;
  if (style == 1)
  {
    // Glass: a grid of tinted panes, aluminium mullions.
    for (double y = top; y < bottom - 2; y += 18)
      for (double x = 8; x < w - 12; x += 22)
      {
        cairo_rectangle(cr, x, y, 20, 16);
        const bool on = litWindow(seed, n++, lit);
        fillLinear(cr, x, y, x + 20, y + 16,
          {{0.0, on ? lighten(kWarmLit, 0.2f) : rgb(120, 190, 200)}, {1.0, on ? rgb(240, 170, 80) : rgb(50, 90, 110)}});
        if (on)
          glow(cr, x + 10, y + 8, 18, kWarmLit, 0.2);
      }
    for (double x = 7; x < w - 10; x += 22)
    {
      cairo_move_to(cr, x, top - 2);
      cairo_line_to(cr, x, bottom);
    }
    stroke(cr, rgb(220, 230, 236), 2.0);
  }
  else if (style == 3)
  {
    for (double y = top + 4; y < bottom - 8; y += 26)
    {
      cairo_rectangle(cr, 8, y, w - 16, 14);
      fillLinear(cr, 0, y, 0, y + 14, {{0.0, rgb(70, 86, 110)}, {1.0, rgb(30, 34, 50)}});
      for (double x = 12; x < w - 20; x += 16)
        if (litWindow(seed, n++, lit))
        {
          cairo_rectangle(cr, x, y + 2, 12, 10);
          setColor(cr, kWarmLit);
          cairo_fill(cr);
          glow(cr, x + 6, y + 7, 14, kWarmLit, 0.2);
        }
      cairo_rectangle(cr, 8, y, w - 16, 14);
      stroke(cr, rgb(230, 230, 220), 1.6);
    }
    text(cr, "BANK", w * 0.5, h * 0.09, h * 0.07, rgb(250, 240, 200));
  }
  else
  {
    // Piers and punched windows.
    const double cols = style == 0 ? 5 : 4;
    const double cw = (w - 20) / cols;
    for (double y = top + (style == 2 ? 18 : 0); y < bottom - 14; y += 24)
      for (int c = 0; c < int(cols); ++c)
        pane(cr, 12 + c * cw + cw * 0.18, y, cw * 0.64, 16, litWindow(seed, n++, lit), 1.4);
    if (style == 0)
    {
      // Deco piers running up, a gold chevron band at the top.
      for (int c = 0; c <= int(cols); ++c)
      {
        cairo_rectangle(cr, 10 + c * cw - 3, top - 6, 6, bottom - top + 6);
        fillLinear(cr, 0, 0, 6, 0, {{0.0, lighten(face, 0.35f)}, {1.0, darken(face, 0.2f)}});
      }
      for (double x = 6; x < w - 6; x += 12)
      {
        cairo_move_to(cr, x, 14);
        cairo_line_to(cr, x + 6, 8);
        cairo_line_to(cr, x + 12, 14);
      }
      stroke(cr, rgb(214, 170, 60), 2.2);
    }
    else
    {
      // The brick: courses, and a neon HOTEL sign across the top.
      for (double y = 4; y < h * 0.8; y += 6)
      {
        cairo_move_to(cr, 2, y);
        cairo_line_to(cr, w - 2, y);
      }
      stroke(cr, darken(face, 0.3f), 0.6, 0.35);
      roundedRect(cr, w * 0.18, 6, w * 0.64, 22, 3);
      fillLinear(cr, 0, 6, 0, 28, {{0.0, rgb(40, 30, 40)}, {1.0, rgb(20, 16, 22)}}, 1.4);
      if (lit)
        glow(cr, w * 0.5, 17, w * 0.35, rgb(120, 230, 255), 0.3);
      text(cr, "HOTEL", w * 0.5, 24, 17, lit ? rgb(160, 240, 255) : rgb(70, 90, 100));
    }
  }
  // The shop front: an awning, the glass, a door.
  const double sy = h * 0.8;
  cairo_rectangle(cr, 4, sy + 8, w - 8, h - sy - 8);
  fillLinear(cr, 0, sy + 8, 0, h, {{0.0, lit ? rgb(255, 230, 170) : rgb(70, 80, 100)}, {1.0, lit ? rgb(220, 150, 80) : rgb(30, 34, 46)}}, 1.4);
  cairo_rectangle(cr, w * 0.42, sy + 12, w * 0.16, h - sy - 12);
  fillLinear(cr, 0, sy, 0, h, {{0.0, rgb(120, 70, 50)}, {1.0, rgb(70, 40, 30)}}, 1.2);
  static const Color kAwning[4] = {rgb(220, 60, 60), rgb(60, 140, 200), rgb(60, 150, 90), rgb(230, 170, 50)};
  const Color aw = kAwning[(seed / 3u) % 4u];
  for (int i = 0; i < 10; ++i)
  {
    cairo_rectangle(cr, 2 + i * (w - 4) / 10, sy, (w - 4) / 10, 12);
    setColor(cr, i % 2 ? rgb(250, 248, 236) : aw);
    cairo_fill(cr);
  }
  cairo_rectangle(cr, 2, sy, w - 4, 12);
  ink(cr, 1.2);
  // The parapet.
  cairo_rectangle(cr, 0, 0, w, 5);
  fillLinear(cr, 0, 0, 0, 5, {{0.0, lighten(face, 0.4f)}, {1.0, darken(face, 0.35f)}});
}

void paintCooling(cairo_t* cr, double w, double h)
{
  const Color con = rgb(184, 184, 178);
  cairo_move_to(cr, w * 0.16, 0);
  cairo_curve_to(cr, w * 0.28, h * 0.4, w * 0.28, h * 0.5, w * 0.02, h);
  cairo_line_to(cr, w * 0.98, h);
  cairo_curve_to(cr, w * 0.72, h * 0.5, w * 0.72, h * 0.4, w * 0.84, 0);
  cairo_close_path(cr);
  fillLinear(cr, 0, 0, w, 0, {{0.0, lighten(con, 0.3f)}, {0.45, con}, {1.0, darken(con, 0.45f)}}, 1.8);
  // Streaks, the red and white band, the rim.
  for (int i = 0; i < 9; ++i)
  {
    const double x = w * (0.22 + i * 0.07);
    cairo_move_to(cr, x, 8);
    cairo_line_to(cr, x + (i - 4) * 2.5, h * 0.95);
  }
  stroke(cr, darken(con, 0.25f), 1.0, 0.35);
  for (int i = 0; i < 6; ++i)
  {
    cairo_rectangle(cr, w * (0.18 + i * 0.11), h * 0.06, w * 0.11, h * 0.05);
    setColor(cr, i % 2 ? rgb(240, 240, 236) : rgb(210, 60, 50));
    cairo_fill(cr);
  }
  cairo_rectangle(cr, w * 0.14, 0, w * 0.72, 5);
  fillLinear(cr, 0, 0, 0, 5, {{0.0, rgb(230, 230, 226)}, {1.0, rgb(120, 120, 116)}}, 1.0);
  text(cr, "AEC", w * 0.5, h * 0.62, h * 0.06, rgb(90, 90, 96));
}

void paintHangar(cairo_t* cr, double w, double h, const char* name, bool lit)
{
  const Color tin = rgb(170, 176, 170);
  cairo_move_to(cr, 0, h);
  cairo_line_to(cr, 0, h * 0.2);
  cairo_curve_to(cr, w * 0.2, 0, w * 0.8, 0, w, h * 0.2);
  cairo_line_to(cr, w, h);
  cairo_close_path(cr);
  fillLinear(cr, 0, 0, 0, h, {{0.0, lighten(tin, 0.3f)}, {0.5, tin}, {1.0, darken(tin, 0.35f)}}, 1.8);
  for (double x = 6; x < w; x += 8)
  {
    cairo_move_to(cr, x, h * 0.2 * (1.0 - std::sin(x / w * kPi)) + 2);
    cairo_line_to(cr, x, h * 0.36);
  }
  stroke(cr, darken(tin, 0.3f), 0.8, 0.5);
  // A checker band, the name.
  for (int i = 0; i < 16; ++i)
    for (int j = 0; j < 2; ++j)
    {
      cairo_rectangle(cr, i * w / 16, h * 0.32 + j * 6, w / 16, 6);
      setColor(cr, (i + j) % 2 ? rgb(240, 240, 236) : rgb(214, 70, 50));
      cairo_fill(cr);
    }
  text(cr, name, w * 0.5, h * 0.26, h * 0.09, rgb(40, 50, 70));
  // The doors: big, sliding, a row of windows; a light over them.
  cairo_rectangle(cr, w * 0.08, h * 0.44, w * 0.84, h * 0.56);
  fillLinear(cr, 0, h * 0.44, 0, h, {{0.0, rgb(120, 130, 120)}, {1.0, rgb(70, 76, 70)}}, 1.6);
  for (int i = 1; i < 4; ++i)
  {
    cairo_move_to(cr, w * (0.08 + i * 0.21), h * 0.44);
    cairo_line_to(cr, w * (0.08 + i * 0.21), h);
  }
  ink(cr, 1.4);
  for (int i = 0; i < 8; ++i)
    pane(cr, w * (0.1 + i * 0.105), h * 0.5, w * 0.08, h * 0.08, lit && i % 3 != 1, 1.0);
  if (lit)
    glow(cr, w * 0.5, h * 0.44, w * 0.2, rgb(255, 230, 170), 0.35);
}

void paintStation(cairo_t* cr, double w, double h, bool lit)
{
  const Color stone = rgb(214, 204, 182);
  cairo_rectangle(cr, 0, 0, w, h);
  fillLinear(cr, 0, 0, w, 0, {{0.0, lighten(stone, 0.15f)}, {1.0, darken(stone, 0.25f)}}, 1.8);
  // The frieze: UNION STATION and a clock.
  cairo_rectangle(cr, 0, 0, w, h * 0.2);
  fillLinear(cr, 0, 0, 0, h * 0.2, {{0.0, lighten(stone, 0.3f)}, {1.0, darken(stone, 0.2f)}}, 1.6);
  text(cr, "UNION STATION", w * 0.36, h * 0.15, h * 0.1, rgb(80, 70, 56));
  circle(cr, w * 0.82, h * 0.1, h * 0.075);
  setColor(cr, lit ? rgb(255, 246, 210) : rgb(150, 150, 140));
  cairo_fill_preserve(cr);
  ink(cr, 1.6);
  cairo_move_to(cr, w * 0.82, h * 0.1);
  cairo_line_to(cr, w * 0.82, h * 0.05);
  cairo_move_to(cr, w * 0.82, h * 0.1);
  cairo_line_to(cr, w * 0.84, h * 0.12);
  ink(cr, 1.4);
  // Columns, arched windows between.
  const int cols = 7;
  for (int i = 0; i < cols; ++i)
  {
    const double cx = w * (0.07 + i * 0.86 / (cols - 1));
    if (i < cols - 1)
    {
      const double ax = cx + w * 0.03, aw = w * 0.86 / (cols - 1) - w * 0.06, ay = h * 0.32, ah = h * 0.42;
      cairo_move_to(cr, ax, ay + ah);
      cairo_line_to(cr, ax, ay + aw * 0.5);
      cairo_arc(cr, ax + aw * 0.5, ay + aw * 0.5, aw * 0.5, kPi, 2 * kPi);
      cairo_line_to(cr, ax + aw, ay + ah);
      cairo_close_path(cr);
      const bool on = lit && i != 2;
      fillLinear(cr, 0, ay, 0, ay + ah, {{0.0, on ? lighten(kWarmLit, 0.2f) : rgb(70, 80, 110)}, {1.0, on ? rgb(230, 160, 80) : rgb(30, 34, 50)}}, 1.4);
      cairo_move_to(cr, ax + aw * 0.5, ay);
      cairo_line_to(cr, ax + aw * 0.5, ay + ah);
      cairo_move_to(cr, ax, ay + ah * 0.55);
      cairo_line_to(cr, ax + aw, ay + ah * 0.55);
      stroke(cr, rgb(240, 236, 226), 1.2);
    }
    cairo_rectangle(cr, cx - 6, h * 0.24, 12, h * 0.62);
    fillLinear(cr, cx - 6, 0, cx + 6, 0, {{0.0, rgb(250, 246, 236)}, {1.0, rgb(170, 160, 140)}}, 1.2);
  }
  // The steps.
  for (int i = 0; i < 3; ++i)
  {
    cairo_rectangle(cr, w * 0.02 + i * 6, h * 0.86 + i * 0.045 * h, w * 0.96 - i * 12, h * 0.05);
    fillLinear(cr, 0, h * 0.86, 0, h, {{0.0, rgb(230, 226, 214)}, {1.0, rgb(160, 156, 140)}}, 1.0);
  }
  text(cr, "SUBWAY", w * 0.5, h * 0.82, h * 0.06, rgb(40, 60, 120));
}

// The Halcyon tower: a gleaming Art Deco shaft, gold fins, the name lit up
// its face, a stepped crown.
void paintHalcyon(cairo_t* cr, double w, double h, bool lit)
{
  const Color body = rgb(236, 226, 200), gold = rgb(226, 180, 70);
  // The crown: steps in to the top.
  cairo_move_to(cr, 0, h);
  cairo_line_to(cr, 0, h * 0.14);
  cairo_line_to(cr, w * 0.12, h * 0.14);
  cairo_line_to(cr, w * 0.12, h * 0.07);
  cairo_line_to(cr, w * 0.26, h * 0.07);
  cairo_line_to(cr, w * 0.26, h * 0.02);
  cairo_line_to(cr, w * 0.4, h * 0.02);
  cairo_line_to(cr, w * 0.5, 0);
  cairo_line_to(cr, w * 0.6, h * 0.02);
  cairo_line_to(cr, w * 0.74, h * 0.02);
  cairo_line_to(cr, w * 0.74, h * 0.07);
  cairo_line_to(cr, w * 0.88, h * 0.07);
  cairo_line_to(cr, w * 0.88, h * 0.14);
  cairo_line_to(cr, w, h * 0.14);
  cairo_line_to(cr, w, h);
  cairo_close_path(cr);
  fillLinear(cr, 0, 0, w, 0, {{0.0, lighten(body, 0.4f)}, {0.5, body}, {1.0, darken(body, 0.3f)}}, 1.8);
  // Gold fins up the face, the windows between.
  for (int i = 0; i < 5; ++i)
  {
    const double x = w * (0.1 + i * 0.2);
    cairo_rectangle(cr, x - 3, h * 0.16, 6, h * 0.7);
    fillLinear(cr, x - 3, 0, x + 3, 0, {{0.0, lighten(gold, 0.4f)}, {1.0, darken(gold, 0.35f)}});
  }
  int n = 0;
  for (double y = h * 0.18; y < h * 0.84; y += 18)
    for (int i = 0; i < 4; ++i)
    {
      const double x = w * (0.13 + i * 0.2);
      cairo_rectangle(cr, x, y, w * 0.14, 11);
      const bool on = lit && (n++ * 7 % 5) < 3;
      setColor(cr, on ? kWarmLit : rgb(60, 70, 96));
      cairo_fill(cr);
    }
  // The name, vertical, in a neon panel.
  roundedRect(cr, w * 0.36, h * 0.3, w * 0.28, h * 0.42, 4);
  fillLinear(cr, 0, 0, 0, h, {{0.0, rgb(30, 26, 40)}, {1.0, rgb(14, 12, 20)}}, 1.4);
  if (lit)
    glow(cr, w * 0.5, h * 0.51, w * 0.5, rgb(255, 200, 90), 0.25);
  const char* name[] = {"H", "A", "L", "C", "Y", "O", "N"};
  for (int i = 0; i < 7; ++i)
    text(cr, name[i], w * 0.5, h * (0.345 + i * 0.056), h * 0.045, lit ? rgb(255, 220, 120) : rgb(120, 100, 60));
  // The lobby.
  cairo_rectangle(cr, w * 0.1, h * 0.88, w * 0.8, h * 0.12);
  fillLinear(cr, 0, h * 0.88, 0, h, {{0.0, lit ? rgb(255, 236, 180) : rgb(70, 80, 100)}, {1.0, lit ? rgb(230, 160, 80) : rgb(30, 34, 46)}}, 1.4);
  cairo_rectangle(cr, 0, h * 0.86, w, h * 0.02);
  setColor(cr, gold);
  cairo_fill(cr);
  // Glints.
  for (const double gy : {0.04, 0.3, 0.6})
    glow(cr, w * 0.2, h * gy, 10, rgb(255, 250, 220), 0.7);
}

// A tower that topples: 0 Art Deco (terracotta setbacks, an antenna),
// 1 a lighthouse (red and white bands, the lantern), 2 an airport control
// tower (a slim shaft, the glass cab on top, a checker band).
void paintTower(cairo_t* cr, double w, double h, int style, bool lit)
{
  if (style == 1)
  {
    // The lighthouse, tapering.
    const double t0 = w * 0.2, t1 = w * 0.8;
    for (int i = 0; i < 6; ++i)
    {
      const double ya = h * (0.24 + i * 0.127), yb = ya + h * 0.127;
      const double ia = (ya - h * 0.24) / (h * 0.76), ib = (yb - h * 0.24) / (h * 0.76);
      cairo_move_to(cr, t0 - ia * w * 0.12, ya);
      cairo_line_to(cr, t1 + ia * w * 0.12, ya);
      cairo_line_to(cr, t1 + ib * w * 0.12, yb);
      cairo_line_to(cr, t0 - ib * w * 0.12, yb);
      cairo_close_path(cr);
      const Color band = i % 2 ? rgb(244, 240, 232) : rgb(210, 50, 44);
      fillLinear(cr, 0, 0, w, 0, {{0.1, lighten(band, 0.3f)}, {0.5, band}, {0.9, darken(band, 0.35f)}});
    }
    cairo_move_to(cr, t0, h * 0.24);
    cairo_line_to(cr, t1, h * 0.24);
    cairo_line_to(cr, t1 + w * 0.12, h);
    cairo_line_to(cr, t0 - w * 0.12, h);
    cairo_close_path(cr);
    ink(cr, 1.8);
    // A door and two little windows.
    roundedRect(cr, w * 0.42, h * 0.9, w * 0.16, h * 0.1, 3);
    fillLinear(cr, 0, h * 0.9, 0, h, {{0.0, rgb(90, 60, 40)}, {1.0, rgb(50, 30, 20)}}, 1.0);
    for (const double y : {0.45, 0.7})
      pane(cr, w * 0.44, h * y, w * 0.12, h * 0.04, lit, 1.0);
    // The gallery and its rail.
    cairo_rectangle(cr, w * 0.08, h * 0.2, w * 0.84, h * 0.04);
    fillLinear(cr, 0, h * 0.2, 0, h * 0.24, {{0.0, rgb(60, 60, 66)}, {1.0, rgb(24, 24, 28)}}, 1.2);
    for (double x = w * 0.1; x < w * 0.92; x += 8)
    {
      cairo_move_to(cr, x, h * 0.17);
      cairo_line_to(cr, x, h * 0.2);
    }
    cairo_move_to(cr, w * 0.08, h * 0.17);
    cairo_line_to(cr, w * 0.92, h * 0.17);
    stroke(cr, rgb(40, 40, 46), 1.6);
    // The lantern: glass, the lamp, a red dome.
    cairo_rectangle(cr, w * 0.26, h * 0.08, w * 0.48, h * 0.1);
    fillLinear(cr, 0, h * 0.08, 0, h * 0.18, {{0.0, rgb(255, 250, 220)}, {1.0, rgb(255, 200, 90)}}, 1.4);
    glow(cr, w * 0.5, h * 0.13, w * 0.45, rgb(255, 240, 180), 0.5);
    for (int i = 1; i < 4; ++i)
    {
      cairo_move_to(cr, w * (0.26 + i * 0.12), h * 0.08);
      cairo_line_to(cr, w * (0.26 + i * 0.12), h * 0.18);
    }
    ink(cr, 1.2);
    cairo_move_to(cr, w * 0.22, h * 0.08);
    cairo_curve_to(cr, w * 0.24, h * 0.0, w * 0.76, h * 0.0, w * 0.78, h * 0.08);
    cairo_close_path(cr);
    fillLinear(cr, 0, 0, 0, h * 0.08, {{0.0, rgb(230, 80, 70)}, {1.0, rgb(140, 30, 30)}}, 1.4);
    return;
  }
  if (style == 2)
  {
    // The control tower.
    const Color con = rgb(214, 210, 198);
    cairo_rectangle(cr, w * 0.3, h * 0.22, w * 0.4, h * 0.78);
    fillLinear(cr, w * 0.3, 0, w * 0.7, 0, {{0.0, lighten(con, 0.3f)}, {1.0, darken(con, 0.35f)}}, 1.8);
    for (double y = h * 0.3; y < h * 0.92; y += 30)
      pane(cr, w * 0.43, y, w * 0.14, 12, lit, 1.0);
    // The cab: tilted glass, a roof, an antenna and a beacon.
    cairo_move_to(cr, w * 0.04, h * 0.07);
    cairo_line_to(cr, w * 0.96, h * 0.07);
    cairo_line_to(cr, w * 0.88, h * 0.18);
    cairo_line_to(cr, w * 0.12, h * 0.18);
    cairo_close_path(cr);
    fillLinear(cr, 0, h * 0.07, 0, h * 0.18, {{0.0, lit ? rgb(200, 240, 220) : rgb(90, 120, 130)}, {1.0, lit ? rgb(110, 190, 170) : rgb(40, 56, 66)}}, 1.6);
    for (int i = 1; i < 6; ++i)
    {
      cairo_move_to(cr, w * (0.04 + i * 0.153), h * 0.07);
      cairo_line_to(cr, w * (0.12 + i * 0.127), h * 0.18);
    }
    ink(cr, 1.0);
    cairo_rectangle(cr, w * 0.0, h * 0.04, w, h * 0.035);
    fillLinear(cr, 0, h * 0.04, 0, h * 0.075, {{0.0, rgb(90, 90, 100)}, {1.0, rgb(40, 40, 46)}}, 1.2);
    for (int i = 0; i < 8; ++i)
    {
      cairo_rectangle(cr, w * (0.12 + i * 0.095), h * 0.18, w * 0.095, h * 0.03);
      setColor(cr, i % 2 ? rgb(240, 240, 236) : rgb(210, 60, 50));
      cairo_fill(cr);
    }
    cairo_move_to(cr, w * 0.5, h * 0.04);
    cairo_line_to(cr, w * 0.5, 2);
    stroke(cr, rgb(60, 60, 70), 2.0);
    circle(cr, w * 0.5, 3, 3);
    setColor(cr, rgb(255, 80, 70));
    cairo_fill(cr);
    return;
  }
  // Art Deco: terracotta, setbacks, cream piers, an antenna.
  const Color terra = rgb(196, 112, 82);
  cairo_move_to(cr, 0, h);
  cairo_line_to(cr, 0, h * 0.22);
  cairo_line_to(cr, w * 0.1, h * 0.22);
  cairo_line_to(cr, w * 0.1, h * 0.12);
  cairo_line_to(cr, w * 0.24, h * 0.12);
  cairo_line_to(cr, w * 0.24, h * 0.05);
  cairo_line_to(cr, w * 0.46, h * 0.05);
  cairo_line_to(cr, w * 0.48, 0);
  cairo_line_to(cr, w * 0.52, 0);
  cairo_line_to(cr, w * 0.54, h * 0.05);
  cairo_line_to(cr, w * 0.76, h * 0.05);
  cairo_line_to(cr, w * 0.76, h * 0.12);
  cairo_line_to(cr, w * 0.9, h * 0.12);
  cairo_line_to(cr, w * 0.9, h * 0.22);
  cairo_line_to(cr, w, h * 0.22);
  cairo_line_to(cr, w, h);
  cairo_close_path(cr);
  fillLinear(cr, 0, 0, w, 0, {{0.0, lighten(terra, 0.3f)}, {0.6, terra}, {1.0, darken(terra, 0.35f)}}, 1.8);
  for (int c = 0; c < 4; ++c)
  {
    const double x = w * (0.08 + c * 0.28);
    cairo_rectangle(cr, x - 2, h * 0.24, 4, h * 0.66);
    setColor(cr, rgb(244, 230, 200));
    cairo_fill(cr);
  }
  int n = 0;
  for (double y = h * 0.26; y < h * 0.86; y += 22)
    for (int c = 0; c < 3; ++c)
      pane(cr, w * (0.13 + c * 0.28), y, w * 0.18, 13, lit && (n++ * 5 % 7) < 4, 1.0);
  for (double y = h * 0.14; y < h * 0.2; y += 12)
    for (int c = 0; c < 3; ++c)
      pane(cr, w * (0.18 + c * 0.22), y, w * 0.14, 8, lit, 0.8);
  cairo_rectangle(cr, w * 0.1, h * 0.9, w * 0.8, h * 0.1);
  fillLinear(cr, 0, h * 0.9, 0, h, {{0.0, rgb(250, 240, 210)}, {1.0, rgb(170, 150, 120)}}, 1.4);
  roundedRect(cr, w * 0.4, h * 0.92, w * 0.2, h * 0.08, 2);
  setColor(cr, lit ? kWarmLit : rgb(60, 50, 40));
  cairo_fill(cr);
}

// A pile of model rubble `w` wide (its base along the bottom, `h` tall at
// most): chunks of the walls and roof in their colours, splinters of balsa,
// a broken window frame, a curl of cardboard.
void paintRubble(cairo_t* cr, double w, double h, Color wall, Color roof, std::uint32_t seed)
{
  Rng rng(seed);
  const double top = h * 0.25;
  // The mound's dust.
  cairo_move_to(cr, 0, h);
  cairo_curve_to(cr, w * 0.1, h * 0.5, w * 0.3, top, w * 0.5, top);
  cairo_curve_to(cr, w * 0.7, top, w * 0.9, h * 0.5, w, h);
  cairo_close_path(cr);
  fillLinear(cr, 0, top, 0, h, {{0.0, rgb(150, 140, 130)}, {1.0, rgb(90, 84, 80)}});
  const int n = int(w / 7) + 6;
  for (int i = 0; i < n; ++i)
  {
    const double u = rng.range(0.04f, 0.96f);
    const double peak = h - (h - top) * std::sin(u * kPi);
    const double x = u * w, y = rng.range(float(peak), float(h)) - 2;
    const double s = rng.range(4, 11);
    const Color c = i % 3 == 0 ? roof : (i % 3 == 1 ? wall : lighten(wall, 0.2f));
    cairo_save(cr);
    cairo_translate(cr, x, y);
    cairo_rotate(cr, rng.range(0, 6.28f));
    cairo_move_to(cr, -s, -s * 0.4);
    cairo_line_to(cr, s * 0.3, -s * 0.7);
    cairo_line_to(cr, s, s * 0.2);
    cairo_line_to(cr, -s * 0.2, s * 0.6);
    cairo_close_path(cr);
    fillLinear(cr, 0, -s, 0, s, {{0.0, lighten(c, 0.2f)}, {1.0, darken(c, 0.3f)}}, 1.0);
    cairo_restore(cr);
  }
  // Splinters and a window frame.
  for (int i = 0; i < 4; ++i)
  {
    const double x = rng.range(float(w * 0.15), float(w * 0.85)), y = rng.range(float(top + 4), float(h - 4));
    const double a = rng.range(-0.8f, 0.8f), l = rng.range(10, 22);
    cairo_move_to(cr, x - std::cos(a) * l * 0.5, y - std::sin(a) * l * 0.5);
    cairo_line_to(cr, x + std::cos(a) * l * 0.5, y + std::sin(a) * l * 0.5);
    stroke(cr, i % 2 ? rgb(230, 206, 160) : rgb(240, 236, 226), 2.0);
  }
  cairo_save(cr);
  cairo_translate(cr, w * 0.4, h * 0.62);
  cairo_rotate(cr, -0.3);
  cairo_rectangle(cr, -7, -6, 14, 12);
  stroke(cr, rgb(240, 236, 226), 2.0);
  cairo_move_to(cr, 0, -6);
  cairo_line_to(cr, 0, 6);
  stroke(cr, rgb(240, 236, 226), 1.4);
  cairo_restore(cr);
}

// --- From above (the Top-Down Rampage) ------------------------------------------------

void paintRoofTop(cairo_t* cr, double w, double h, bool house, std::uint32_t seed)
{
  // The shadow to the lower right.
  cairo_rectangle(cr, 8, 8, w - 4, h - 4);
  setRgba(cr, rgb(0, 0, 0), 0.32);
  cairo_fill(cr);
  if (house)
  {
    static const Color kRoofs[4] = {rgb(184, 72, 62), rgb(90, 98, 124), rgb(86, 126, 86), rgb(170, 120, 70)};
    const Color roof = kRoofs[seed % 4u];
    cairo_rectangle(cr, 3, 3, w - 6, h * 0.5 - 3);
    fillLinear(cr, 0, 3, 0, h * 0.5, {{0.0, lighten(roof, 0.35f)}, {1.0, roof}}, 1.4);
    cairo_rectangle(cr, 3, h * 0.5, w - 6, h * 0.5 - 3);
    fillLinear(cr, 0, h * 0.5, 0, h - 3, {{0.0, darken(roof, 0.3f)}, {1.0, darken(roof, 0.45f)}}, 1.4);
    for (double x = 8; x < w - 6; x += 7)
    {
      cairo_move_to(cr, x, 4);
      cairo_line_to(cr, x, h - 4);
    }
    stroke(cr, darken(roof, 0.5f), 0.7, 0.4);
    cairo_move_to(cr, 3, h * 0.5);
    cairo_line_to(cr, w - 3, h * 0.5);
    stroke(cr, lighten(roof, 0.5f), 2.0);
    cairo_rectangle(cr, w * 0.62, h * 0.18, w * 0.14, h * 0.16);
    fillLinear(cr, 0, 0, 0, h, {{0.0, rgb(190, 100, 80)}, {1.0, rgb(110, 50, 40)}}, 1.2);
    return;
  }
  static const Color kGravel[4] = {rgb(170, 164, 150), rgb(150, 154, 160), rgb(180, 170, 146), rgb(140, 136, 130)};
  const Color gravel = kGravel[seed % 4u];
  cairo_rectangle(cr, 2, 2, w - 4, h - 4);
  fillLinear(cr, 0, 0, w, h, {{0.0, lighten(gravel, 0.15f)}, {1.0, darken(gravel, 0.15f)}}, 1.6);
  Rng rng(seed);
  for (int i = 0; i < 120; ++i)
  {
    cairo_rectangle(cr, rng.range(5, float(w - 5)), rng.range(5, float(h - 5)), 1.2, 1.2);
    setRgba(cr, i % 2 ? rgb(220, 214, 200) : rgb(110, 104, 96), 0.6);
    cairo_fill(cr);
  }
  // The parapet.
  cairo_rectangle(cr, 2, 2, w - 4, h - 4);
  stroke(cr, rgb(230, 226, 214), 5.0);
  cairo_rectangle(cr, 5, 5, w - 10, h - 10);
  stroke(cr, rgb(100, 96, 90), 1.2, 0.8);
  const auto airHandler = [&](double x, double y, double s) {
    roundedRect(cr, x, y, w * 0.34 * s, h * 0.26 * s, 3);
    fillLinear(cr, 0, y, 0, y + h * 0.26 * s, {{0.0, rgb(214, 218, 226)}, {1.0, rgb(140, 146, 156)}}, 1.4);
    const double fx = x + w * 0.17 * s, fy = y + h * 0.13 * s, fr = h * 0.09 * s;
    circle(cr, fx, fy, fr);
    setColor(cr, rgb(60, 64, 72));
    cairo_fill(cr);
    for (int k = 0; k < 4; ++k)
    {
      const double a = k * kPi / 2 + 0.4;
      cairo_move_to(cr, fx, fy);
      cairo_line_to(cr, fx + std::cos(a) * fr * 0.9, fy + std::sin(a) * fr * 0.9);
    }
    stroke(cr, rgb(170, 176, 186), 2.0);
  };
  const auto tank = [&](double x, double y, double r) {
    circle(cr, x, y, r);
    fillLinear(cr, x - r, 0, x + r, 0, {{0.0, rgb(196, 140, 96)}, {1.0, rgb(120, 76, 48)}}, 1.4);
    circle(cr, x, y, r * 0.4);
    stroke(cr, rgb(90, 56, 36), 1.4);
  };
  const auto hatch = [&](double x, double y) {
    cairo_rectangle(cr, x, y, w * 0.16, h * 0.16);
    fillLinear(cr, 0, y, 0, y + h * 0.16, {{0.0, rgb(120, 126, 136)}, {1.0, rgb(70, 74, 84)}}, 1.2);
  };
  switch (seed % 4u)
  {
    case 0: // an air handler, a water tank, a hatch
      airHandler(w * 0.14, h * 0.16, 1.0);
      tank(w * 0.7, h * 0.66, h * 0.15);
      hatch(w * 0.2, h * 0.64);
      break;
    case 1: // a helipad
    {
      circle(cr, w * 0.5, h * 0.5, std::min(w, h) * 0.34);
      setColor(cr, rgb(70, 74, 82));
      cairo_fill_preserve(cr);
      stroke(cr, rgb(240, 210, 90), 2.4);
      const double c = w * 0.5, m = h * 0.5, k = std::min(w, h) * 0.14;
      cairo_move_to(cr, c - k * 0.7, m - k);
      cairo_line_to(cr, c - k * 0.7, m + k);
      cairo_move_to(cr, c + k * 0.7, m - k);
      cairo_line_to(cr, c + k * 0.7, m + k);
      cairo_move_to(cr, c - k * 0.7, m);
      cairo_line_to(cr, c + k * 0.7, m);
      stroke(cr, rgb(244, 244, 236), 3.4);
      hatch(w * 0.76, h * 0.1);
      break;
    }
    case 2: // two air handlers and skylights
      airHandler(w * 0.1, h * 0.12, 0.8);
      airHandler(w * 0.56, h * 0.12, 0.8);
      for (int k = 0; k < 3; ++k)
      {
        cairo_rectangle(cr, w * (0.14 + 0.25 * k), h * 0.56, w * 0.18, h * 0.28);
        fillLinear(cr, 0, h * 0.56, w, h * 0.84, {{0.0, rgb(150, 190, 220)}, {1.0, rgb(60, 80, 110)}}, 1.2);
      }
      break;
    default: // a roof garden and a tank
    {
      roundedRect(cr, w * 0.1, h * 0.1, w * 0.5, h * 0.8, 4);
      fillLinear(cr, 0, 0, 0, h, {{0.0, rgb(112, 160, 84)}, {1.0, rgb(70, 112, 56)}}, 1.2);
      for (int k = 0; k < 6; ++k)
      {
        circle(cr, w * (0.2 + 0.15 * (k % 2)) + rng.range(-3, 3), h * (0.2 + 0.12 * k), h * 0.07);
        setColor(cr, k % 3 == 0 ? rgb(230, 120, 140) : rgb(60, 120, 60));
        cairo_fill(cr);
      }
      tank(w * 0.78, h * 0.3, h * 0.14);
      hatch(w * 0.7, h * 0.66);
      break;
    }
  }
}

void paintRubbleTop(cairo_t* cr, double w, double h, std::uint32_t seed)
{
  Rng rng(seed);
  ellipse(cr, w * 0.5, h * 0.5, w * 0.48, h * 0.48);
  setRgba(cr, rgb(120, 112, 104), 0.7);
  cairo_fill(cr);
  for (int i = 0; i < int(w / 3); ++i)
  {
    const double a = rng.range(0, 6.28f), d = rng.range(0, 0.45f);
    const double x = w * 0.5 + std::cos(a) * w * d, y = h * 0.5 + std::sin(a) * h * d, s = rng.range(3, 8);
    cairo_save(cr);
    cairo_translate(cr, x, y);
    cairo_rotate(cr, rng.range(0, 6.28f));
    cairo_rectangle(cr, -s, -s * 0.6, s * 2, s * 1.2);
    static const Color kBits[4] = {rgb(184, 72, 62), rgb(210, 206, 196), rgb(150, 150, 140), rgb(230, 206, 160)};
    fillLinear(cr, 0, -s, 0, s, {{0.0, lighten(kBits[i % 4], 0.2f)}, {1.0, darken(kBits[i % 4], 0.3f)}}, 0.8);
    cairo_restore(cr);
  }
}

// A street tile from above: asphalt, speckled.
void paintAsphalt(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 0, 64, 64);
  setColor(cr, rgb(70, 72, 82));
  cairo_fill(cr);
  Rng rng(2711u);
  for (int i = 0; i < 70; ++i)
  {
    cairo_rectangle(cr, rng.range(0, 63), rng.range(0, 63), 1.2, 1.0);
    setRgba(cr, i % 2 ? rgb(120, 122, 134) : rgb(40, 42, 50), 0.6);
    cairo_fill(cr);
  }
}

// A lot tile from above: the sidewalk's concrete slabs.
void paintSidewalk(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 0, 64, 64);
  setColor(cr, rgb(184, 180, 170));
  cairo_fill(cr);
  for (const double v : {0.5, 32.5})
  {
    cairo_move_to(cr, v, 0);
    cairo_line_to(cr, v, 64);
    cairo_move_to(cr, 0, v);
    cairo_line_to(cr, 64, v);
  }
  stroke(cr, rgb(130, 126, 118), 1.0, 0.8);
}

// A park square from above: flock grass, a tree, a path's corner.
void paintPark(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 0, 64, 64);
  fillLinear(cr, 0, 0, 64, 64, {{0.0, rgb(110, 170, 86)}, {1.0, rgb(70, 130, 64)}});
  Rng rng(2719u);
  for (int i = 0; i < 90; ++i)
  {
    cairo_rectangle(cr, rng.range(0, 63), rng.range(0, 63), 1.0, 1.0);
    setRgba(cr, i % 2 ? rgb(170, 220, 120) : rgb(50, 96, 44), 0.7);
    cairo_fill(cr);
  }
  ellipse(cr, 36, 38, 17, 15);
  setRgba(cr, rgb(0, 0, 0), 0.3);
  cairo_fill(cr);
  for (int i = 0; i < 5; ++i)
  {
    const double a = i * 1.26;
    circle(cr, 30 + std::cos(a) * 7, 31 + std::sin(a) * 7, 9);
  }
  cairo_pattern_t* g = cairo_pattern_create_radial(26, 26, 2, 30, 31, 18);
  stop(g, 0, rgb(130, 200, 100));
  stop(g, 1, rgb(40, 90, 44));
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  ink(cr, 1.2);
  for (int i = 0; i < 4; ++i)
  {
    circle(cr, 8 + i * 4, 54 - i * 3, 2.0);
    setColor(cr, i % 2 ? rgb(255, 120, 150) : rgb(255, 230, 90));
    cairo_fill(cr);
  }
}

// The edge of the tabletop, from above: the plywood's top, a dark rim.
void paintTableEdge(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 0, 64, 64);
  fillLinear(cr, 0, 0, 64, 64, {{0.0, rgb(150, 110, 70)}, {1.0, rgb(110, 76, 46)}});
  for (int i = 0; i < 8; ++i)
  {
    cairo_move_to(cr, 0, 4 + i * 8);
    cairo_curve_to(cr, 20, 6 + i * 8, 44, 2 + i * 8, 64, 4 + i * 8);
  }
  stroke(cr, rgb(90, 60, 36), 0.8, 0.5);
}

// --- Props -------------------------------------------------------------------------------

// The model water of the canal and the bay: a glossy resin sheet, deep in
// the middle, painted ripples, its meniscus light along the top.
void paintPond(cairo_t* cr, double w, double h)
{
  cairo_rectangle(cr, 0, 0, w, h);
  fillLinear(cr, 0, 0, 0, h, {{0.0, rgb(60, 140, 170)}, {0.3, rgb(30, 90, 130)}, {1.0, rgb(12, 36, 70)}});
  Rng rng(std::uint32_t(w * 7 + h));
  for (int i = 0; i < int(w / 24); ++i)
  {
    const double x = rng.range(0, float(w)), y = rng.range(10, float(h) * 0.8), l = rng.range(14, 40);
    cairo_move_to(cr, x, y);
    cairo_curve_to(cr, x + l * 0.3, y - 3, x + l * 0.7, y + 3, x + l, y);
    stroke(cr, rgb(150, 220, 240), 1.4, 0.25);
  }
  cairo_rectangle(cr, 0, 0, w, 5);
  fillLinear(cr, 0, 0, 0, 5, {{0.0, rgb(220, 250, 255)}, {1.0, rgb(90, 170, 200)}});
  cairo_move_to(cr, 0, 7);
  cairo_line_to(cr, w, 7);
  stroke(cr, rgb(200, 240, 255), 1.0, 0.4);
}

void paintRipple(cairo_t* cr)
{
  ellipse(cr, 80, 20, 70, 12);
  stroke(cr, rgb(220, 246, 255), 2.4, 0.8);
  ellipse(cr, 80, 20, 46, 7);
  stroke(cr, rgb(220, 246, 255), 1.4, 0.5);
}

// A lattice pylon `h` tall, its middle at (60, h): splayed legs, cross
// bracing, the crossarm with insulators near the top.
void paintPylon(cairo_t* cr, double h)
{
  const Color steel = rgb(176, 182, 196);
  const double top = 24, arm = 36;
  auto legs = [&](double lw, Color c) {
    cairo_move_to(cr, 20, h);
    cairo_line_to(cr, 50, top);
    cairo_move_to(cr, 100, h);
    cairo_line_to(cr, 70, top);
    stroke(cr, c, lw);
  };
  legs(7, kInk);
  legs(4, steel);
  for (double y = top + 14; y < h - 10; y += 34)
  {
    const double u0 = (y - top) / (h - top), u1 = (y + 34 - top) / (h - top);
    const double l0 = 50 - 30 * u0, r0 = 70 + 30 * u0, l1 = 50 - 30 * std::min(1.0, u1), r1 = 70 + 30 * std::min(1.0, u1);
    cairo_move_to(cr, l0, y);
    cairo_line_to(cr, r1, std::min(h, y + 34));
    cairo_move_to(cr, r0, y);
    cairo_line_to(cr, l1, std::min(h, y + 34));
    cairo_move_to(cr, l0, y);
    cairo_line_to(cr, r0, y);
  }
  stroke(cr, darken(steel, 0.2f), 1.6);
  // The crossarm and the insulators.
  cairo_rectangle(cr, 2, arm - 4, 116, 6);
  fillLinear(cr, 0, arm - 4, 0, arm + 2, {{0.0, lighten(steel, 0.3f)}, {1.0, darken(steel, 0.4f)}}, 1.4);
  for (const double ix : {8.0, 112.0})
  {
    for (int k = 0; k < 3; ++k)
    {
      ellipse(cr, ix, arm + 5 + k * 4, 4.5, 1.8);
      setColor(cr, rgb(130, 200, 160));
      cairo_fill_preserve(cr);
      ink(cr, 0.8);
    }
  }
  cairo_move_to(cr, 60, top);
  cairo_line_to(cr, 60, 2);
  stroke(cr, steel, 2.0);
}

// The model bus on route 42: two-tone, rounded, the route sign lit.
void paintBus(cairo_t* cr, double w, double h, const std::string& route)
{
  roundedRect(cr, 2, h * 0.1, w - 4, h * 0.72, h * 0.22);
  fillLinear(cr, 0, h * 0.1, 0, h * 0.82, {{0.0, rgb(244, 236, 206)}, {0.48, rgb(230, 220, 186)}, {0.5, rgb(70, 130, 96)}, {1.0, rgb(40, 84, 60)}}, 1.8);
  for (int i = 0; i < 6; ++i)
    pane(cr, w * (0.1 + i * 0.12), h * 0.22, w * 0.1, h * 0.22, true, 1.2);
  // The windshield and the route sign.
  cairo_move_to(cr, w * 0.84, h * 0.2);
  cairo_line_to(cr, w * 0.95, h * 0.24);
  cairo_line_to(cr, w * 0.97, h * 0.48);
  cairo_line_to(cr, w * 0.84, h * 0.48);
  cairo_close_path(cr);
  fillLinear(cr, 0, h * 0.2, 0, h * 0.5, {{0.0, rgb(200, 236, 255)}, {1.0, rgb(80, 120, 150)}}, 1.4);
  roundedRect(cr, w * 0.83, h * 0.03, w * 0.15, h * 0.14, 2);
  fillLinear(cr, 0, 0, 0, h * 0.17, {{0.0, rgb(30, 26, 20)}, {1.0, rgb(10, 8, 6)}}, 1.2);
  glow(cr, w * 0.905, h * 0.1, h * 0.2, rgb(255, 200, 90), 0.4);
  text(cr, route.c_str(), w * 0.905, h * 0.155, h * 0.14, rgb(255, 220, 110));
  // The wheels, a headlight.
  for (const double wx : {0.2, 0.78})
  {
    circle(cr, w * wx, h * 0.84, h * 0.14);
    fillLinear(cr, 0, h * 0.7, 0, h, {{0.0, rgb(70, 70, 70)}, {1.0, rgb(20, 20, 20)}}, 1.6);
    circle(cr, w * wx, h * 0.84, h * 0.06);
    setColor(cr, rgb(190, 190, 196));
    cairo_fill(cr);
  }
  circle(cr, w * 0.975, h * 0.64, h * 0.05);
  setColor(cr, rgb(255, 246, 200));
  cairo_fill(cr);
}

// The model ocean liner: a black hull with a red boot top, white upper
// works with portholes and two raked funnels, `w` x `h`, its deck at `deck`.
void paintLiner(cairo_t* cr, double w, double h, double deck)
{
  // The funnels (behind the exit's spot, at the far end).
  for (const double fx : {0.72, 0.88})
  {
    cairo_move_to(cr, w * fx, deck - 30);
    cairo_line_to(cr, w * (fx + 0.02), deck - 120);
    cairo_line_to(cr, w * (fx + 0.1), deck - 120);
    cairo_line_to(cr, w * (fx + 0.08), deck - 30);
    cairo_close_path(cr);
    fillLinear(cr, w * fx, 0, w * (fx + 0.1), 0, {{0.0, rgb(250, 180, 80)}, {1.0, rgb(190, 110, 40)}}, 1.8);
    cairo_rectangle(cr, w * (fx + 0.02), deck - 120, w * 0.08, 18);
    setColor(cr, rgb(30, 30, 34));
    cairo_fill(cr);
  }
  // The upper works.
  cairo_rectangle(cr, w * 0.62, deck - 34, w * 0.36, 34);
  fillLinear(cr, 0, deck - 34, 0, deck, {{0.0, rgb(250, 250, 246)}, {1.0, rgb(190, 190, 186)}}, 1.6);
  for (int i = 0; i < 8; ++i)
    pane(cr, w * (0.64 + i * 0.042), deck - 26, w * 0.025, 10, true, 0.8);
  // The hull, its bow raked to the left.
  cairo_move_to(cr, 0, deck);
  cairo_line_to(cr, w, deck);
  cairo_line_to(cr, w, h);
  cairo_line_to(cr, w * 0.1, h);
  cairo_curve_to(cr, w * 0.05, h * 0.7, w * 0.02, deck + 30, 0, deck);
  cairo_close_path(cr);
  fillLinear(cr, 0, deck, 0, h, {{0.0, rgb(50, 50, 60)}, {0.62, rgb(20, 20, 26)}, {0.64, rgb(190, 40, 40)}, {1.0, rgb(110, 20, 24)}}, 2.0);
  cairo_move_to(cr, 0, deck + 2);
  cairo_line_to(cr, w, deck + 2);
  stroke(cr, rgb(250, 250, 246), 3.0);
  for (int i = 0; i < 14; ++i)
  {
    circle(cr, w * (0.12 + i * 0.062), deck + 26, 4);
    setColor(cr, (i * 7) % 3 ? kWarmLit : rgb(60, 70, 90));
    cairo_fill_preserve(cr);
    stroke(cr, rgb(200, 200, 210), 1.2);
  }
  text(cr, "S.S. MAXIMA", w * 0.32, deck + 54, 15, rgb(250, 240, 220));
  // The rail along the deck.
  for (double x = 4; x < w; x += 10)
  {
    cairo_move_to(cr, x, deck);
    cairo_line_to(cr, x, deck - 10);
  }
  cairo_move_to(cr, 0, deck - 10);
  cairo_line_to(cr, w * 0.62, deck - 10);
  stroke(cr, rgb(240, 240, 236), 1.4, 0.9);
}

// The harbor maser dish on its pedestal, facing left (the arena), 120 x
// 140, the pedestal's foot at (60, 140): the dish's centre at (40, 46).
void paintHarborDish(cairo_t* cr)
{
  cairo_rectangle(cr, 46, 70, 28, 70);
  fillLinear(cr, 46, 0, 74, 0, {{0.0, rgb(150, 156, 170)}, {1.0, rgb(70, 74, 86)}}, 1.8);
  cairo_rectangle(cr, 34, 126, 52, 14);
  fillLinear(cr, 0, 126, 0, 140, {{0.0, rgb(120, 126, 140)}, {1.0, rgb(50, 54, 64)}}, 1.6);
  cairo_move_to(cr, 60, 74);
  cairo_line_to(cr, 48, 50);
  stroke(cr, kInk, 9);
  cairo_move_to(cr, 60, 74);
  cairo_line_to(cr, 48, 50);
  stroke(cr, rgb(130, 136, 150), 6);
  ellipse(cr, 52, 46, 16, 42);
  fillLinear(cr, 40, 0, 70, 0, {{0.0, rgb(90, 96, 110)}, {1.0, rgb(150, 156, 170)}}, 2.0);
  ellipse(cr, 42, 46, 11, 40);
  cairo_pattern_t* g = cairo_pattern_create_radial(42, 46, 2, 42, 46, 40);
  stop(g, 0, rgb(230, 236, 246));
  stop(g, 0.8, rgb(170, 176, 190));
  stop(g, 1, rgb(110, 114, 126));
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  ink(cr, 2.0);
  cairo_move_to(cr, 42, 46);
  cairo_line_to(cr, 8, 46);
  stroke(cr, kInk, 6);
  cairo_move_to(cr, 42, 46);
  cairo_line_to(cr, 8, 46);
  stroke(cr, rgb(190, 196, 206), 3.4);
  for (int i = 0; i < 3; ++i)
  {
    ellipse(cr, 30 - i * 7, 46, 2.4, 8 - i);
    setColor(cr, rgb(190, 150, 90));
    cairo_fill_preserve(cr);
    ink(cr, 1.0);
  }
  circle(cr, 7, 46, 5.5);
  setColor(cr, rgb(170, 230, 250));
  cairo_fill_preserve(cr);
  ink(cr, 1.2);
}

// The arena's gate: a tall chain-link harbor gate in a steel frame,
// hazard stripes and a sign. w x h.
void paintGate(cairo_t* cr, double w, double h)
{
  cairo_rectangle(cr, 4, 0, w - 8, h);
  setRgba(cr, rgb(30, 30, 40), 0.35);
  cairo_fill(cr);
  for (double y = -w; y < h; y += 10)
  {
    cairo_move_to(cr, 4, y);
    cairo_line_to(cr, w - 4, y + (w - 8));
    cairo_move_to(cr, w - 4, y);
    cairo_line_to(cr, 4, y + (w - 8));
  }
  stroke(cr, rgb(180, 186, 196), 1.0, 0.8);
  for (const double x : {3.0, w - 3.0})
  {
    cairo_move_to(cr, x, 0);
    cairo_line_to(cr, x, h);
  }
  cairo_move_to(cr, 0, 3);
  cairo_line_to(cr, w, 3);
  stroke(cr, kInk, 8);
  for (const double x : {3.0, w - 3.0})
  {
    cairo_move_to(cr, x, 0);
    cairo_line_to(cr, x, h);
  }
  stroke(cr, rgb(150, 156, 170), 5);
  for (double y = 60; y < h; y += 140)
  {
    cairo_rectangle(cr, 2, y, w - 4, 14);
    setColor(cr, rgb(240, 200, 40));
    cairo_fill(cr);
    for (double x = -10; x < w; x += 14)
    {
      cairo_move_to(cr, x, y + 14);
      cairo_line_to(cr, x + 14, y);
      cairo_line_to(cr, x + 21, y);
      cairo_line_to(cr, x + 7, y + 14);
      cairo_close_path(cr);
    }
    setColor(cr, rgb(30, 30, 36));
    cairo_fill(cr);
  }
}

// The full heal: a craft-services first-aid cabinet on legs, a red cross on
// its door, a thermos and a doughnut on top. 64 x 96, its feet at (32, 96).
void paintFullHeal(cairo_t* cr)
{
  for (const double x : {14.0, 50.0})
  {
    cairo_move_to(cr, x, 60);
    cairo_line_to(cr, x, 96);
  }
  stroke(cr, rgb(90, 90, 100), 3.0);
  roundedRect(cr, 6, 26, 52, 40, 4);
  fillLinear(cr, 0, 26, 0, 66, {{0.0, rgb(255, 255, 255)}, {1.0, rgb(200, 204, 214)}}, 1.8);
  cairo_rectangle(cr, 26, 32, 12, 28);
  cairo_rectangle(cr, 18, 40, 28, 12);
  setColor(cr, rgb(226, 40, 56));
  cairo_fill(cr);
  // A thermos and a doughnut.
  roundedRect(cr, 12, 4, 10, 22, 3);
  fillLinear(cr, 12, 0, 22, 0, {{0.0, rgb(120, 190, 120)}, {1.0, rgb(50, 110, 60)}}, 1.4);
  ellipse(cr, 42, 22, 11, 5);
  fillLinear(cr, 0, 17, 0, 27, {{0.0, rgb(250, 160, 200)}, {1.0, rgb(190, 110, 70)}}, 1.4);
  ellipse(cr, 42, 21, 3.5, 1.6);
  setColor(cr, rgb(120, 70, 40));
  cairo_fill(cr);
}

// The stuntman's trailer: a rounded aluminium caravan, a star on its door,
// his name on a card. w x h, its wheels on the bottom.
void paintTrailer(cairo_t* cr, double w, double h)
{
  roundedRect(cr, 2, h * 0.08, w - 4, h * 0.78, h * 0.26);
  fillLinear(cr, 0, h * 0.08, 0, h * 0.86, {{0.0, rgb(230, 234, 240)}, {0.5, rgb(170, 176, 188)}, {1.0, rgb(120, 126, 138)}}, 1.8);
  for (double y = h * 0.2; y < h * 0.8; y += 10)
  {
    cairo_move_to(cr, 10, y);
    cairo_line_to(cr, w - 10, y);
  }
  stroke(cr, rgb(130, 136, 150), 0.8, 0.5);
  // The door and its star.
  roundedRect(cr, w * 0.08, h * 0.28, w * 0.2, h * 0.58, 4);
  fillLinear(cr, 0, h * 0.28, 0, h * 0.86, {{0.0, rgb(214, 220, 228)}, {1.0, rgb(150, 156, 168)}}, 1.4);
  cairo_save(cr);
  cairo_translate(cr, w * 0.18, h * 0.42);
  for (int i = 0; i < 10; ++i)
  {
    const double a = -kPi / 2 + i * kPi / 5, rr = i % 2 ? 6 : 15;
    if (i == 0)
      cairo_move_to(cr, std::cos(a) * rr, std::sin(a) * rr);
    else
      cairo_line_to(cr, std::cos(a) * rr, std::sin(a) * rr);
  }
  cairo_close_path(cr);
  cairo_restore(cr);
  fillLinear(cr, 0, h * 0.3, 0, h * 0.55, {{0.0, rgb(255, 230, 120)}, {1.0, rgb(220, 160, 40)}}, 1.4);
  roundedRect(cr, w * 0.07, h * 0.6, w * 0.22, h * 0.08, 2);
  setColor(cr, rgb(250, 248, 240));
  cairo_fill(cr);
  text(cr, "MR. G", w * 0.18, h * 0.665, h * 0.05, rgb(40, 40, 50));
  // A window with its awning; the wheels.
  pane(cr, w * 0.74, h * 0.3, w * 0.18, h * 0.18, true, 1.4);
  for (int i = 0; i < 5; ++i)
  {
    cairo_rectangle(cr, w * 0.72 + i * w * 0.044, h * 0.24, w * 0.044, h * 0.05);
    setColor(cr, i % 2 ? rgb(250, 248, 236) : rgb(60, 140, 200));
    cairo_fill(cr);
  }
  circle(cr, w * 0.6, h * 0.88, h * 0.1);
  fillLinear(cr, 0, h * 0.78, 0, h, {{0.0, rgb(70, 70, 70)}, {1.0, rgb(20, 20, 20)}}, 1.6);
}

// EMPLOYEE OF THE MONTH: a framed photo of him in the suit, thumbs up; 80 x
// 110, hung from its nail at (40, 0).
void paintPhoto(cairo_t* cr)
{
  cairo_move_to(cr, 40, 2);
  cairo_line_to(cr, 14, 16);
  cairo_move_to(cr, 40, 2);
  cairo_line_to(cr, 66, 16);
  stroke(cr, rgb(60, 50, 40), 1.2);
  circle(cr, 40, 2, 2.2);
  setColor(cr, rgb(200, 200, 210));
  cairo_fill(cr);
  cairo_rectangle(cr, 6, 14, 68, 92);
  fillLinear(cr, 0, 14, 0, 106, {{0.0, rgb(214, 170, 70)}, {1.0, rgb(150, 100, 30)}}, 1.6);
  cairo_rectangle(cr, 12, 20, 56, 62);
  fillLinear(cr, 0, 20, 0, 82, {{0.0, rgb(200, 220, 240)}, {1.0, rgb(150, 170, 200)}});
  // Him: the suit, the head, a thumb up.
  cairo_save(cr);
  cairo_rectangle(cr, 12, 20, 56, 62);
  cairo_clip(cr);
  paintGorgazilla(cr, 36, 112, 0.42, 0, 0);
  circle(cr, 56, 52, 4.5);
  setColor(cr, rgb(130, 168, 110));
  cairo_fill_preserve(cr);
  ink(cr, 1.0);
  cairo_move_to(cr, 56, 48);
  cairo_line_to(cr, 56, 40);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  stroke(cr, kInk, 4.0);
  cairo_move_to(cr, 56, 48);
  cairo_line_to(cr, 56, 40);
  stroke(cr, rgb(130, 168, 110), 2.4);
  cairo_restore(cr);
  cairo_rectangle(cr, 12, 20, 56, 62);
  ink(cr, 1.2);
  cairo_rectangle(cr, 10, 86, 60, 16);
  setColor(cr, rgb(250, 246, 230));
  cairo_fill(cr);
  text(cr, "EMPLOYEE OF", 40, 93, 6.5, rgb(40, 30, 20));
  text(cr, "THE MONTH", 40, 100.5, 6.5, rgb(40, 30, 20));
}

// A 1950s console TV round the bonus patch: a walnut cabinet on splayed
// legs, the screen (the static shows through it), dials. w x h; the screen
// is the hole (sx, sy, sw, sh).
void paintTvSet(cairo_t* cr, double w, double h, double sx, double sy, double sw, double sh)
{
  roundedRect(cr, 2, 2, w - 4, h * 0.86, 10);
  roundedRect(cr, sx, sy, sw, sh, 12);
  cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
  fillLinear(cr, 0, 0, w, 0, {{0.0, rgb(150, 96, 56)}, {0.5, rgb(120, 72, 40)}, {1.0, rgb(80, 46, 24)}}, 2.0);
  cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
  roundedRect(cr, sx - 3, sy - 3, sw + 6, sh + 6, 14);
  stroke(cr, rgb(200, 190, 160), 3.0);
  for (int i = 0; i < 2; ++i)
  {
    circle(cr, w - 16, sy + 14 + i * 26, 7);
    fillLinear(cr, 0, sy, 0, sy + 50, {{0.0, rgb(240, 230, 200)}, {1.0, rgb(150, 140, 110)}}, 1.4);
  }
  for (int i = 0; i < 6; ++i)
  {
    cairo_move_to(cr, w - 24, sy + 50 + i * 5);
    cairo_line_to(cr, w - 8, sy + 50 + i * 5);
  }
  stroke(cr, rgb(60, 36, 20), 1.4);
  for (const double x : {14.0, w - 14.0})
  {
    cairo_move_to(cr, x, h * 0.86);
    cairo_line_to(cr, x + (x < w * 0.5 ? -6 : 6), h);
  }
  stroke(cr, rgb(60, 36, 20), 4.0);
  // The rabbit-ear aerial.
  cairo_move_to(cr, w * 0.5, 2);
  cairo_line_to(cr, w * 0.3, -40);
  cairo_move_to(cr, w * 0.5, 2);
  cairo_line_to(cr, w * 0.72, -36);
  stroke(cr, rgb(200, 200, 210), 1.8);
}

// The subway's wall (and anything else under the table): glazed cream
// tiles in a running bond, a maroon band, grime; one block.
void paintSubwayWall(cairo_t* cr, bool band)
{
  // Glazed cream tiles in half-bond, a little grimy; the lowest row of
  // blocks carries a maroon band at the height of a train's windows.
  cairo_rectangle(cr, 0, 0, 64, 64);
  fillLinear(cr, 0, 0, 0, 64, {{0.0, rgb(150, 142, 122)}, {1.0, rgb(126, 118, 100)}});
  for (int row = 0; row < 8; ++row)
    for (int c = -1; c < 4; ++c)
    {
      const double x = c * 21.33 + (row % 2 ? 10.66 : 0.0), y = row * 8.0;
      roundedRect(cr, x + 0.6, y + 0.6, 20.1, 6.8, 1.0);
      const bool b = band && (row == 1 || row == 2);
      fillLinear(cr, 0, y, 0, y + 8,
        {{0.0, b ? rgb(134, 62, 58) : rgb(200, 192, 166)}, {1.0, b ? rgb(108, 46, 46) : rgb(178, 170, 146)}});
    }
  if (band)
  {
    cairo_rectangle(cr, 0, 7.4, 64, 1.2);
    setRgba(cr, rgb(232, 200, 120), 0.7);
    cairo_fill(cr);
    cairo_rectangle(cr, 0, 23.4, 64, 1.2);
    setRgba(cr, rgb(232, 200, 120), 0.7);
    cairo_fill(cr);
  }
  // Grime, darker low down.
  cairo_rectangle(cr, 0, 0, 64, 64);
  cairo_pattern_t* g = cairo_pattern_create_linear(0, 0, 0, 64);
  stop(g, 0, rgb(30, 26, 34), 0.18);
  stop(g, 1, rgb(30, 26, 34), band ? 0.5 : 0.3);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
}

// The back wall of a model canal or the harbour basin: dressed concrete
// blocks, drip stains, a green scum line low down. 64 x 64.
void paintQuay(cairo_t* cr, bool low)
{
  cairo_rectangle(cr, 0, 0, 64, 64);
  fillLinear(cr, 0, 0, 0, 64, {{0.0, rgb(92, 96, 108)}, {1.0, rgb(70, 74, 86)}});
  for (int row = 0; row < 4; ++row)
    for (int c = -1; c < 3; ++c)
    {
      const double x = c * 32.0 + (row % 2 ? 16.0 : 0.0), y = row * 16.0;
      roundedRect(cr, x + 1.0, y + 1.0, 30.0, 14.0, 1.6);
      fillLinear(cr, 0, y, 0, y + 16, {{0.0, rgb(116, 120, 130)}, {1.0, rgb(86, 90, 102)}});
    }
  for (int k = 0; k < 3; ++k)
  {
    const double x = 9.0 + k * 21.0;
    cairo_move_to(cr, x, 0);
    cairo_curve_to(cr, x + 2, 20, x - 2, 40, x + 1, 64);
    stroke(cr, rgb(40, 44, 50), 3.0 + k, 0.18);
  }
  if (low)
  {
    cairo_rectangle(cr, 0, 40, 64, 24);
    fillLinear(cr, 0, 40, 0, 64, {{0.0, rgb(70, 110, 70)}, {1.0, rgb(30, 60, 50)}}, 0.0);
    cairo_rectangle(cr, 0, 40, 64, 24);
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 40, 0, 64);
    stop(g, 0, rgb(60, 110, 70), 0.0);
    stop(g, 0.4, rgb(60, 110, 70), 0.55);
    stop(g, 1, rgb(20, 50, 44), 0.7);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  cairo_rectangle(cr, 0, 0, 64, 64);
  setRgba(cr, rgb(10, 12, 30), 0.25);
  cairo_fill(cr);
}

// The rails along the subway's floor, seen from the side, laid over the
// floor tile's street top: a bed of ballast, sleepers, a rail.
// 64 x 44, the floor's top at y 14.
void paintTrack(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 6, 64, 38);
  fillLinear(cr, 0, 6, 0, 44, {{0.0, rgb(92, 88, 84)}, {0.5, rgb(70, 66, 62)}, {1.0, rgb(46, 42, 40)}});
  // Gravel.
  std::uint32_t h = 2166136261u;
  for (int i = 0; i < 70; ++i)
  {
    h = (h ^ std::uint32_t(i * 7919)) * 16777619u;
    const double x = double(h % 64u), y = 10.0 + double((h >> 8) % 32u);
    const int v = 60 + int((h >> 16) % 70u);
    circle(cr, x, y, 1.2 + double((h >> 24) % 3u) * 0.5);
    setRgba(cr, rgb(v + 10, v + 6, v), 0.9);
    cairo_fill(cr);
  }
  for (int i = 0; i < 4; ++i)
  {
    cairo_rectangle(cr, i * 16 + 2, 8, 12, 7);
    fillLinear(cr, 0, 8, 0, 15, {{0.0, rgb(126, 92, 60)}, {1.0, rgb(74, 50, 30)}}, 0.8);
  }
  cairo_rectangle(cr, 0, 1, 64, 7);
  fillLinear(cr, 0, 1, 0, 8, {{0.0, rgb(236, 238, 246)}, {0.35, rgb(160, 164, 176)}, {1.0, rgb(64, 66, 76)}});
  cairo_rectangle(cr, 0, 40, 64, 4);
  setRgba(cr, rgb(20, 18, 16), 0.5);
  cairo_fill(cr);
}

// A road sign on its post: a yellow diamond with an arrow pointing right.
// 60 x 110, the post's foot at (30, 110).
void paintArrowSign(cairo_t* cr)
{
  cairo_rectangle(cr, 27, 40, 6, 70);
  fillLinear(cr, 27, 0, 33, 0, {{0.0, rgb(200, 204, 214)}, {1.0, rgb(110, 114, 126)}}, 1.4);
  cairo_move_to(cr, 30, 2);
  cairo_line_to(cr, 58, 30);
  cairo_line_to(cr, 30, 58);
  cairo_line_to(cr, 2, 30);
  cairo_close_path(cr);
  fillLinear(cr, 0, 2, 0, 58, {{0.0, rgb(255, 226, 90)}, {1.0, rgb(230, 170, 30)}}, 2.2);
  cairo_move_to(cr, 14, 26);
  cairo_line_to(cr, 32, 26);
  cairo_line_to(cr, 32, 18);
  cairo_line_to(cr, 46, 30);
  cairo_line_to(cr, 32, 42);
  cairo_line_to(cr, 32, 34);
  cairo_line_to(cr, 14, 34);
  cairo_close_path(cr);
  setColor(cr, rgb(20, 20, 26));
  cairo_fill(cr);
}

// A cartoon motion arrow curving the way a tower falls (right), 90 x 70.
void paintCreak(cairo_t* cr)
{
  cairo_move_to(cr, 10, 60);
  cairo_curve_to(cr, 20, 20, 50, 8, 74, 14);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  stroke(cr, kInk, 9);
  cairo_move_to(cr, 10, 60);
  cairo_curve_to(cr, 20, 20, 50, 8, 74, 14);
  stroke(cr, rgb(255, 240, 200), 5);
  cairo_move_to(cr, 66, 2);
  cairo_line_to(cr, 88, 16);
  cairo_line_to(cr, 64, 28);
  cairo_close_path(cr);
  setColor(cr, rgb(255, 240, 200));
  cairo_fill_preserve(cr);
  ink(cr, 2.4);
}

void paintDust(cairo_t* cr)
{
  for (int i = 0; i < 5; ++i)
  {
    const double a = i * 1.256;
    circle(cr, 40 + std::cos(a) * 12, 40 + std::sin(a) * 9, 18);
  }
  cairo_pattern_t* g = cairo_pattern_create_radial(34, 32, 4, 40, 40, 34);
  stop(g, 0, rgb(236, 226, 210), 0.95);
  stop(g, 0.7, rgb(190, 180, 166), 0.8);
  stop(g, 1, rgb(160, 150, 140), 0.0);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
}

void paintSpark(cairo_t* cr)
{
  glow(cr, 24, 24, 24, rgb(160, 220, 255), 0.9);
  for (int i = 0; i < 8; ++i)
  {
    const double a = i * kPi / 4 + 0.2, l = i % 2 ? 10 : 20;
    cairo_move_to(cr, 24, 24);
    cairo_line_to(cr, 24 + std::cos(a) * l, 24 + std::sin(a) * l);
  }
  stroke(cr, rgb(255, 255, 230), 2.0);
  circle(cr, 24, 24, 4);
  setColor(cr, rgb(255, 255, 255));
  cairo_fill(cr);
}

void paintFlame(cairo_t* cr)
{
  cairo_move_to(cr, 20, 60);
  cairo_curve_to(cr, 2, 50, 6, 30, 16, 16);
  cairo_curve_to(cr, 16, 26, 22, 28, 22, 20);
  cairo_curve_to(cr, 22, 10, 28, 4, 30, 0);
  cairo_curve_to(cr, 34, 14, 42, 26, 38, 44);
  cairo_curve_to(cr, 36, 54, 30, 60, 20, 60);
  cairo_close_path(cr);
  cairo_pattern_t* g = cairo_pattern_create_linear(0, 60, 0, 0);
  stop(g, 0, rgb(255, 250, 200));
  stop(g, 0.4, rgb(255, 170, 60));
  stop(g, 1, rgb(220, 50, 30), 0.0);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
}

// The DAMAGE meter's panel: a 1950s seismograph dial, 220 x 64.
void paintMeterPanel(cairo_t* cr)
{
  roundedRect(cr, 1, 1, 218, 62, 10);
  fillLinear(cr, 0, 0, 0, 64, {{0.0, rgb(40, 30, 40)}, {1.0, rgb(16, 12, 20)}});
  roundedRect(cr, 1, 1, 218, 62, 10);
  stroke(cr, rgb(220, 120, 70), 2.0, 0.9);
  // The dial's arc on the left.
  cairo_arc(cr, 34, 50, 26, kPi, 2 * kPi);
  stroke(cr, rgb(250, 230, 200), 2.0, 0.8);
  for (int i = 0; i <= 8; ++i)
  {
    const double a = kPi + i * kPi / 8;
    cairo_move_to(cr, 34 + std::cos(a) * 20, 50 + std::sin(a) * 20);
    cairo_line_to(cr, 34 + std::cos(a) * 26, 50 + std::sin(a) * 26);
  }
  stroke(cr, rgb(250, 230, 200), 1.4, 0.7);
  cairo_arc(cr, 34, 50, 26, kPi * 1.75, 2 * kPi);
  stroke(cr, rgb(255, 80, 60), 3.0, 0.9);
}

// Gorgazilla's head, for his bar: 44 x 40.
void paintLizardIcon(cairo_t* cr)
{
  cairo_save(cr);
  cairo_rectangle(cr, 0, 0, 44, 40);
  cairo_clip(cr);
  paintGorgazilla(cr, 8, 112, 0.6, 4, 0);
  cairo_restore(cr);
}

// --- The runner from above (the Top-Down Rampage) ----------------------------------------

struct TopColors
{
  Color skin, hair, top, pants;
};

TopColors topColors(const CharacterDef& c)
{
  switch (c.art)
  {
    case 0:
      return {rgb(246, 200, 160), rgb(255, 214, 72), rgb(232, 64, 52), rgb(54, 76, 150)};
    case 1:
      return {rgb(170, 108, 70), rgb(240, 70, 70), rgb(104, 146, 64), rgb(110, 110, 124)};
    case 2:
      return {rgb(252, 214, 186), rgb(255, 86, 184), rgb(132, 66, 216), rgb(132, 66, 216)};
    default:
      break;
  }
  const auto p = c.parts;
  return {skinTone(p.body, p.skin).first, paletteColor(p.hairColor), paletteColor(p.top), paletteColor(p.pants)};
}

// Seen from straight above, facing +x: the shoulders, the arms (the gun
// out front), the head and its hair; `step` swings the feet. 96 x 96, the
// middle at (48, 48).
void paintTopRunner(cairo_t* cr, const TopColors& c, int step)
{
  const double sw = step == 0 ? 0.0 : (step == 1 ? 9.0 : -9.0);
  ellipse(cr, 52, 52, 26, 22);
  setRgba(cr, rgb(0, 0, 0), 0.3);
  cairo_fill(cr);
  // The feet, stepping.
  for (const int side : {-1, 1})
  {
    ellipse(cr, 46 + sw * side, 48 + side * 9, 8, 5);
    fillLinear(cr, 0, 40, 0, 56, {{0.0, rgb(90, 70, 60)}, {1.0, rgb(40, 30, 26)}}, 1.4);
  }
  // The gun out front, the arms reaching for it.
  cairo_rectangle(cr, 58, 44, 30, 8);
  fillLinear(cr, 0, 44, 0, 52, {{0.0, rgb(200, 210, 230)}, {1.0, rgb(90, 96, 120)}}, 1.4);
  for (const int side : {-1, 1})
  {
    cairo_move_to(cr, 46, 48 + side * 18);
    cairo_line_to(cr, 62, 48 + side * 5);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    stroke(cr, kInk, 10);
    cairo_move_to(cr, 46, 48 + side * 18);
    cairo_line_to(cr, 62, 48 + side * 5);
    stroke(cr, c.top, 7);
    circle(cr, 62, 48 + side * 5, 4);
    setColor(cr, c.skin);
    cairo_fill(cr);
  }
  // The shoulders.
  ellipse(cr, 44, 48, 13, 22);
  fillLinear(cr, 30, 0, 58, 0, {{0.0, darken(c.top, 0.3f)}, {1.0, lighten(c.top, 0.2f)}}, 1.8);
  // The head and its hair.
  circle(cr, 46, 48, 12);
  fillLinear(cr, 34, 36, 58, 60, {{0.0, lighten(c.hair, 0.3f)}, {1.0, darken(c.hair, 0.3f)}}, 1.8);
  circle(cr, 55, 48, 4);
  setColor(cr, c.skin);
  cairo_fill(cr);
  ellipse(cr, 42, 44, 4, 2.4, -0.6);
  setRgba(cr, rgb(255, 255, 255), 0.5);
  cairo_fill(cr);
}

} // namespace

// --- Behind the runner ---------------------------------------------------------------------

void World::drawCityBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& c = mCity;
  const bool top = mTop.on;
  const int W = mLevel->width, H = mLevel->height;
  const int tx0 = std::max(0, int(camX / kTilePx) - 1), tx1 = std::min(W - 1, int((camX + float(kScreenW)) / kTilePx) + 1);
  const int ty0 = std::max(0, int(camY / kTilePx) - 1), ty1 = std::min(H - 1, int((camY + float(kScreenH)) / kTilePx) + 1);
  const auto sectorAt = [&](int bx) {
    for (const auto& s : c.sectors)
      if (bx >= s.x0 && bx <= s.x1)
        return s.dark;
    return false;
  };

  if (top)
  {
    // The grid city from above. Which tiles are lots (a building's) and
    // which parks (an empty square between buildings).
    const int vw = tx1 - tx0 + 3, vh = ty1 - ty0 + 3;
    std::vector<unsigned char> lot(std::size_t(vw * vh), 0);
    const auto at = [&](int bx, int by) -> unsigned char {
      const int i = bx - tx0 + 1, j = by - ty0 + 1;
      return i < 0 || j < 0 || i >= vw || j >= vh ? 0 : lot[std::size_t(j * vw + i)];
    };
    for (const auto& b : c.buildings)
      for (int by = std::max(b.y0, ty0 - 1); by <= std::min(b.y1, ty1 + 1); ++by)
        for (int bx = std::max(b.x0, tx0 - 1); bx <= std::min(b.x1, tx1 + 1); ++bx)
          lot[std::size_t((by - ty0 + 1) * vw + (bx - tx0 + 1))] = 1;
    const auto park = [&](int bx, int by) {
      if (at(bx, by) || mLevel->isSolid(bx, by))
        return false;
      return int(at(bx - 1, by)) + at(bx + 1, by) + at(bx, by - 1) + at(bx, by + 1) >= 2;
    };
    const auto street = [&](int bx, int by) {
      return bx >= 0 && by >= 0 && bx < W && by < H && !mLevel->isSolid(bx, by) && !at(bx, by) && !park(bx, by);
    };
    const Texture& asphalt = baked(mArt, r, "top_asphalt", 64, 64, 0, 0, [](cairo_t* cr) { paintAsphalt(cr); });
    const Texture& walk = baked(mArt, r, "top_walk", 64, 64, 0, 0, [](cairo_t* cr) { paintSidewalk(cr); });
    const Texture& grass = baked(mArt, r, "top_park", 64, 64, 0, 0, [](cairo_t* cr) { paintPark(cr); });
    const Texture& edge = baked(mArt, r, "top_edge", 64, 64, 0, 0, [](cairo_t* cr) { paintTableEdge(cr); });
    for (int by = ty0; by <= ty1; ++by)
      for (int bx = tx0; bx <= tx1; ++bx)
      {
        const float x = float(bx) * kTilePx - camX, y = float(by) * kTilePx - camY;
        if (mLevel->isSolid(bx, by) && !at(bx, by))
        {
          r.draw(edge, x, y);
          continue;
        }
        if (at(bx, by) || park(bx, by))
        {
          r.draw(walk, x, y);
          if (park(bx, by))
            r.draw(grass, x + 0.0f, y + 0.0f);
          // The curb along any side facing the street.
          if (street(bx, by - 1))
            r.fillRect(x, y, kTilePx, 4.0f, rgb(220, 216, 206));
          if (street(bx, by + 1))
            r.fillRect(x, y + kTilePx - 4.0f, kTilePx, 4.0f, rgb(110, 106, 98));
          if (street(bx - 1, by))
            r.fillRect(x, y, 4.0f, kTilePx, rgb(220, 216, 206));
          if (street(bx + 1, by))
            r.fillRect(x + kTilePx - 4.0f, y, 4.0f, kTilePx, rgb(110, 106, 98));
          continue;
        }
        r.draw(asphalt, x, y);
        // Lane lines down the middle of a street: where its run across is
        // three to five blocks (a crossing's runs are longer).
        const auto run = [&](int dx, int dy, int& lo, int& hi) {
          lo = 0;
          hi = 0;
          while (lo < 8 && street(bx - dx * (lo + 1), by - dy * (lo + 1)))
            ++lo;
          while (hi < 8 && street(bx + dx * (hi + 1), by + dy * (hi + 1)))
            ++hi;
          return lo + hi + 1;
        };
        int lo = 0, hi = 0;
        const int vRun = run(0, 1, lo, hi);
        if (vRun >= 3 && vRun <= 5)
        {
          const float mid = (float(by - lo) + float(by + hi + 1)) * 0.5f * kTilePx - camY;
          if (mid >= y - 1.0f && mid <= y + kTilePx + 1.0f)
            for (int k = 0; k < 2; ++k)
              r.fillRect(x + 6.0f + float(k) * 32.0f, mid - 2.0f, 20.0f, 4.0f, rgb(244, 208, 90));
        }
        const int hRun = run(1, 0, lo, hi);
        if (hRun >= 3 && hRun <= 5)
        {
          const float mid = (float(bx - lo) + float(bx + hi + 1)) * 0.5f * kTilePx - camX;
          if (mid >= x - 1.0f && mid <= x + kTilePx + 1.0f)
            for (int k = 0; k < 2; ++k)
              r.fillRect(mid - 2.0f, y + 6.0f + float(k) * 32.0f, 4.0f, 20.0f, rgb(244, 208, 90));
        }
        if (vRun > 5 && hRun > 5)
        {
          // A crossing: zebra stripes along its edges.
          if (!street(bx, by - 2))
            for (int k = 0; k < 4; ++k)
              r.fillRect(x + 4.0f + float(k) * 16.0f, y + 6.0f, 8.0f, 22.0f, withAlpha(rgb(240, 240, 236), 200));
        }
      }
    // The buildings from above.
    for (std::size_t i = 0; i < c.buildings.size(); ++i)
    {
      const auto& b = c.buildings[i];
      const float x = float(b.x0) * kTilePx - camX, y = float(b.y0) * kTilePx - camY;
      const int w = (b.x1 - b.x0 + 1) * int(kTilePx), h = (b.y1 - b.y0 + 1) * int(kTilePx);
      if (offScreen(x, y, float(w), float(h)))
        continue;
      const bool house = b.kind == BuildingKind::House || w <= int(kTilePx);
      const std::uint32_t seed = seedOf(b.id);
      if (b.state == BuildingState::Standing || b.state == BuildingState::Crumbling)
      {
        const float t = b.state == BuildingState::Crumbling ? std::min(1.0f, (float(b.t) + alpha) / float(kCrumbleFrames)) : 0.0f;
        if (t > 0.0f)
          r.draw(baked(mArt, r, "rtop/" + std::to_string(w) + "/" + std::to_string(seed % 97u), w, h, 0, 0,
                   [&](cairo_t* cr) { paintRubbleTop(cr, w, h, seed); }),
            x, y);
        DrawOpts o;
        o.scale = 1.0f - 0.5f * t;
        o.alpha = 1.0f - t;
        o.angle = t * 25.0f;
        r.draw(baked(mArt, r, std::string(house ? "roof_h/" : "roof_o/") + std::to_string(w) + "/" + std::to_string(seed % 4u), w,
                 h, float(w) * 0.5f, float(h) * 0.5f, [&](cairo_t* cr) { paintRoofTop(cr, w, h, house, seed % 4u); }),
          x + float(w) * 0.5f, y + float(h) * 0.5f, o);
        if (t > 0.0f)
          drawGlow(r, mArt, x + float(w) * 0.5f, y + float(h) * 0.5f, float(w) * (0.6f + t), rgb(230, 220, 200), 0.5f * (1.0f - t));
      }
      else
        r.draw(baked(mArt, r, "rtop/" + std::to_string(w) + "/" + std::to_string(seed % 97u), w, h, 0, 0,
                 [&](cairo_t* cr) { paintRubbleTop(cr, w, h, seed); }),
          x, y);
    }
    // The tanks from above, turned the way they drive.
    for (const auto& e : mEnemies)
    {
      if (!e.alive || e.hidden || e.kind != EnemyKind::ModelTank)
        continue;
      const float cx = (e.drawX + float(e.w) * 0.5f) * kCellPx - camX;
      const float cy = (e.drawY - float(e.h) * 0.5f + 1.0f) * kCellPx - camY;
      if (offScreen(cx - 80.0f, cy - 80.0f, 160.0f, 160.0f))
        continue;
      const int ax = e.aimX == 0 && e.aimY == 0 ? e.dir : e.aimX;
      const int ay = e.aimY;
      const bool tell = e.tell > 0;
      const int fr = ((e.x + e.y) % 2 + 2) % 2;
      const std::string key = std::string("tanktop/") + (tell ? "t" : "n") + std::to_string(fr);
      const Texture& tex = baked(mArt, r, key, 192, 128, 80.0f, 64.0f, [&](cairo_t* cr) {
        drawCityArt(cr, mTheme, "model_tank", 96.0, 64.0, tell ? 3 : 2, fr);
      });
      DrawOpts o;
      o.angle = float(std::atan2(double(ay), double(ax)) * 180.0 / kPi);
      r.draw(tex, cx, cy, o);
      if (e.flash > 0)
      {
        o.blend = Blend::Add;
        o.alpha = float(e.flash) / 8.0f;
        r.draw(tex, cx, cy, o);
      }
    }
    return;
  }

  // Toy Jet formations small in the painted sky, growing as they come.
  for (const auto& f : c.formations)
    if (f.state == 1)
    {
      const float u = std::min(1.0f, (float(f.t) + alpha) / float(kJetIntro));
      const float s = 0.18f + 0.72f * u * u;
      const float cx = float(kScreenW) * (0.5f + 0.25f * float(f.dir) * (1.0f - u));
      const float cy = (float(f.row) + 1.0f) * kCellPx - camY - 220.0f * (1.0f - u);
      for (int i = 0; i < 3; ++i)
      {
        DrawOpts o;
        o.scale = s;
        o.tint = lerpColor(rgb(150, 150, 200), rgb(255, 255, 255), u);
        o.alpha = 0.5f + 0.5f * u;
        const float jx = cx + float(i - 1) * 110.0f * s, jy = cy + float(i % 2) * 40.0f * s;
        r.draw(styledEnemySprite(mArt, r, mTheme, "toy_jet", 0, (frame / 3) % 2, 3, 2).get(f.dir), jx, jy, o);
        r.drawLine(jx, jy - 64.0f * s, jx, jy - 64.0f * s - 400.0f, std::max(1.0f, 2.0f * s), withAlpha(rgb(200, 200, 220), int(90 * u)));
      }
    }

  // Under the table: the subway's walls (and the manhole's shaft), rails
  // along its floor, the storeroom's TV set.
  {
    const Texture& wall = baked(mArt, r, "subway_wall", 64, 64, 0, 0, [](cairo_t* cr) { paintSubwayWall(cr, false); });
    const Texture& wallBand = baked(mArt, r, "subway_band", 64, 64, 0, 0, [](cairo_t* cr) { paintSubwayWall(cr, true); });
    const Texture& track = baked(mArt, r, "track", 64, 44, 0, 0, [](cairo_t* cr) { paintTrack(cr); });
    const auto underground = [&](int bx, int by) {
      if (mLevel->isSolid(bx, by))
        return mMap.block(bx, by) == Tile::Empty; // a slab that broke
      for (int k = 1; k <= 8; ++k)
        if (by - k >= 0 && mLevel->at(bx, by - k) != Tile::Empty)
          return true;
      bool left = false, right = false;
      for (int k = 1; k <= 3; ++k)
      {
        left = left || mLevel->isSolid(bx - k, by);
        right = right || mLevel->isSolid(bx + k, by);
      }
      return left && right;
    };
    for (int by = ty0; by <= ty1; ++by)
      for (int bx = tx0; bx <= tx1; ++bx)
      {
        const Tile t = mMap.block(bx, by);
        if ((t != Tile::Empty && t != Tile::Ladder && t != Tile::Platform) || !underground(bx, by))
          continue;
        const float x = float(bx) * kTilePx - camX, y = float(by) * kTilePx - camY;
        DrawOpts o;
        o.tint = sectorAt(bx) ? rgb(110, 110, 140) : rgb(255, 255, 255);
        r.draw(mMap.block(bx, by + 1) == Tile::Solid ? wallBand : wall, x, y, o);
        if (by % 4 == 0 && bx % 6 == 0)
        {
          // A bulb in a cage.
          drawGlow(r, mArt, x + 32.0f, y + 10.0f, 70.0f, rgb(255, 230, 170), sectorAt(bx) ? 0.06f : 0.22f);
          r.fillRect(x + 28.0f, y + 4.0f, 8.0f, 10.0f, sectorAt(bx) ? rgb(90, 90, 80) : rgb(255, 246, 210));
        }
        if (t == Tile::Ladder)
          r.draw(mArt.ladder, x, y);
        else if (t == Tile::Platform)
          r.draw(mArt.platform, x, y);
        if (mMap.block(bx, by + 1) == Tile::Solid)
          r.draw(track, x, y + kTilePx - 14.0f);
      }
    for (const auto& pr : mProps)
    {
      if (pr.kind != PropKind::BonusDoor || pr.dormant || !underground(pr.x / kCellsPerTile, pr.y / kCellsPerTile))
        continue;
      // The patch's top block is its screen; the console stands round it.
      const float x = float(pr.x) * kCellPx - camX, y = float(pr.y) * kCellPx - camY;
      const float sw = float(pr.w) * kCellPx, sh = kTilePx;
      if (offScreen(x - 40.0f, y - 60.0f, sw + 80.0f, 200.0f))
        continue;
      const CellBox near{pr.x - 12, pr.y - 12, pr.w + 24, pr.h + 24};
      const bool flicker = near.intersects(mPlayer.box()) && !pr.used;
      for (int row = 0; row < int(sh / 6.0f); ++row)
      {
        const unsigned hsh = hash2(row * 7 + (flicker ? (frame / 2) % 8 : 0) * 131, pr.x + (frame / 3) % 5);
        const int v = int(70 + (hsh % 150u));
        r.fillRect(x, y + float(row) * 6.0f, sw, 6.0f, rgb(v, v, v + 12));
        if (hsh % 4u == 0)
          r.fillRect(x + float(hsh % 100u) * sw / 100.0f, y + float(row) * 6.0f, 16.0f, 6.0f, rgb(240, 240, 255));
      }
      if (pr.used)
        r.fillRect(x, y, sw, sh, rgba(0, 0, 0, 170));
      const float tw = sw + 52.0f, th = sh + 70.0f;
      r.draw(baked(mArt, r, "tvset", int(tw), int(th), 18.0f, 14.0f,
               [&](cairo_t* cr) { paintTvSet(cr, tw, th, 18.0, 14.0, sw, sh); }),
        x, y);
      if (flicker)
        drawGlow(r, mArt, x + sw * 0.5f, y + sh * 0.5f, 120.0f, rgb(200, 220, 255), 0.35f);
      r.drawText("STOREROOM - KEEP CLEAR", x + sw * 0.5f, y - 54.0f, {14.0f, rgb(250, 240, 210), kInk}, Align::Center, 0.8f);
    }
  }

  // The canal's and the harbour's back walls, down to the water.
  for (const auto& f : c.ponds)
  {
    const int bx0 = f.left() / kCellsPerTile, bx1 = f.right() / kCellsPerTile, byBottom = f.bottom() / kCellsPerTile;
    int top = 0;
    while (top < H && !mLevel->isSolid(bx0 - 1, top))
      ++top;
    if (top >= H || top > byBottom)
      continue;
    const Texture& quay = baked(mArt, r, "quay", 64, 64, 0, 0, [](cairo_t* cr) { paintQuay(cr, false); });
    const Texture& quayLow = baked(mArt, r, "quay_low", 64, 64, 0, 0, [](cairo_t* cr) { paintQuay(cr, true); });
    const int surfaceRow = f.top() / kCellsPerTile;
    for (int by = top; by <= byBottom; ++by)
      for (int bx = bx0; bx <= bx1; ++bx)
      {
        if (mMap.block(bx, by) != Tile::Empty && mMap.block(bx, by) != Tile::Platform)
          continue;
        const float x = float(bx) * kTilePx - camX, y = float(by) * kTilePx - camY;
        if (offScreen(x, y, kTilePx, kTilePx))
          continue;
        r.draw(by >= surfaceRow - 1 ? quayLow : quay, x, y);
      }
  }

  // The power lines on their pylons, sagging; snapped, the ends hang down
  // and spark.
  for (const auto& l : c.lines)
  {
    const float x0 = (float(l.x0) + 0.5f) * kCellPx - camX, x1 = (float(l.x1) + 0.5f) * kCellPx - camX;
    const float wy = (float(l.y) + 0.5f) * kCellPx - camY;
    if (offScreen(std::min(x0, x1) - 80.0f, wy - 60.0f, std::abs(x1 - x0) + 160.0f, 500.0f))
      continue;
    const int span = std::max(1, int(std::ceil(float(l.x1 - l.x0) / float(12 * kCellsPerTile))));
    // The pylons, standing on whatever is under the wire.
    for (int k = 0; k <= span; ++k)
    {
      const int cx = l.x0 + (l.x1 - l.x0) * k / span;
      int gy = l.y / kCellsPerTile + 1;
      while (gy < H && !mLevel->isSolid(cx / kCellsPerTile, gy))
        ++gy;
      const float px = (float(cx) + 0.5f) * kCellPx - camX, ground = float(gy) * kTilePx - camY;
      const int ph = int(ground - wy) + 44;
      if (ph < 60)
        continue;
      DrawOpts o;
      o.tint = sectorAt(cx / kCellsPerTile) ? rgb(130, 130, 160) : rgb(255, 255, 255);
      r.draw(baked(mArt, r, "pylon/" + std::to_string(ph), 120, ph, 60.0f, float(ph), [ph](cairo_t* cr) { paintPylon(cr, ph); }),
        px, ground + 6.0f, o);
    }
    // The wires: one each side of the crossarm.
    for (int k = 0; k < span; ++k)
    {
      const float a = x0 + (x1 - x0) * float(k) / float(span), b = x0 + (x1 - x0) * float(k + 1) / float(span);
      for (const float off : {-52.0f, 52.0f})
      {
        const float ya = wy + 2.0f, ax = a + off * 0.0f;
        (void)ax;
        const int n = 14;
        if (!l.snapped)
        {
          for (int s = 0; s < n; ++s)
          {
            const float u0 = float(s) / float(n), u1 = float(s + 1) / float(n);
            const float s0 = std::sin(u0 * 3.14159f) * 12.0f, s1 = std::sin(u1 * 3.14159f) * 12.0f;
            const float yo = off < 0.0f ? 0.0f : 6.0f;
            r.drawLine(a + (b - a) * u0, ya + yo + s0, a + (b - a) * u1, ya + yo + s1, 2.5f, rgb(20, 18, 26));
          }
          continue;
        }
        // Broken mid-span: each half hangs from its pylon.
        const float mid = (a + b) * 0.5f, yo = off < 0.0f ? 0.0f : 6.0f;
        for (const float from : {a, b})
        {
          const float dir = from < mid ? 1.0f : -1.0f;
          const float len = std::abs(mid - from) * 0.55f;
          float px = from, py = ya + yo;
          for (int s = 1; s <= n; ++s)
          {
            const float u = float(s) / float(n);
            const float nx = from + dir * len * std::sin(u * 1.5708f) * 0.6f;
            const float ny = ya + yo + len * 0.8f * (1.0f - std::cos(u * 1.5708f)) +
              std::sin(float(frame) * 0.15f + from) * 3.0f * u;
            r.drawLine(px, py, nx, ny, 2.5f, rgb(20, 18, 26));
            px = nx;
            py = ny;
          }
          const bool spark = l.sparks > 0 || hash2(int(from), frame / 5) % 6u == 0;
          if (spark)
          {
            DrawOpts so;
            so.blend = Blend::Add;
            so.scale = l.sparks > 0 ? 1.4f : 0.8f;
            so.angle = float((frame * 37) % 360);
            r.draw(baked(mArt, r, "spark", 48, 48, 24.0f, 24.0f, [](cairo_t* cr) { paintSpark(cr); }), px, py, so);
          }
        }
      }
    }
  }

  // The stuntman's trailer and his photo.
  if (c.photoX >= 0)
  {
    const float px = float(c.photoX) * kCellPx - camX, py = float(c.photoY - 2) * kCellPx - camY;
    const float tw = 4.0f * kTilePx, th = 4.0f * kTilePx;
    // The trailer stands on the ground under the photo, the photo on its side.
    int gy = c.photoY / kCellsPerTile + 1;
    while (gy < H && !mLevel->isSolid(c.photoX / kCellsPerTile, gy))
      ++gy;
    const float trX = px - 2.0f * kTilePx, trY = float(gy) * kTilePx - camY - th;
    if (!offScreen(trX, trY, tw, th))
    {
      r.draw(baked(mArt, r, "trailer", int(tw), int(th), 0, 0, [tw, th](cairo_t* cr) { paintTrailer(cr, tw, th); }), trX, trY);
      DrawOpts o;
      o.angle = c.photoSwing > 0 ? std::sin((float(c.photoSwing) - alpha) * 0.45f) * float(c.photoSwing) * 0.9f : 0.0f;
      r.draw(baked(mArt, r, "photo", 80, 110, 40.0f, 0.0f, [](cairo_t* cr) { paintPhoto(cr); }), px + 32.0f, py - 14.0f, o);
    }
  }

  // The model bus on route 42.
  for (const auto& pr : mProps)
  {
    if (pr.kind != PropKind::Deco42)
      continue;
    const float x = float(pr.x) * kCellPx - camX, y = float(pr.y) * kCellPx - camY;
    const float w = float(pr.w) * kCellPx, h = float(pr.h) * kCellPx + 8.0f;
    if (offScreen(x, y, w, h))
      continue;
    const std::string route = pr.text;
    r.draw(baked(mArt, r, "bus/" + route + "/" + std::to_string(int(w)), int(w), int(h), 0, 0,
             [&](cairo_t* cr) { paintBus(cr, w, h, route); }),
      x, y);
  }

  // The buildings.
  for (std::size_t i = 0; i < c.buildings.size(); ++i)
  {
    const auto& b = c.buildings[i];
    const Look look = lookOf(b);
    const int w = (b.x1 - b.x0 + 1) * int(kTilePx), h = (b.y1 - b.y0 + 1) * int(kTilePx);
    const float x = float(b.x0) * kTilePx - camX, y = float(b.y0) * kTilePx - camY;
    const std::uint32_t seed = seedOf(b.id);
    const bool lit = !sectorAt((b.x0 + b.x1) / 2);
    const std::string key = b.id + "/" + std::to_string(w) + "x" + std::to_string(h) + (lit ? "" : "/dark");
    int officeIndex = 0;
    for (std::size_t k = 0; k < i; ++k)
      officeIndex += lookOf(c.buildings[k]) == Look::Office ? 1 : 0;
    const auto paint = [&](cairo_t* cr) {
      switch (look)
      {
        case Look::House:
          paintHouse(cr, w, h, seed, lit);
          break;
        case Look::Diner:
          paintDiner(cr, w, h, lit);
          break;
        case Look::WaterTower:
          paintWaterTower(cr, w, h);
          break;
        case Look::Office:
          paintOffice(cr, w, h, officeIndex % 4, seed, lit);
          break;
        case Look::Cooling:
          paintCooling(cr, w, h);
          break;
        case Look::Hangar:
          paintHangar(cr, w, h, b.id.find('2') != std::string::npos ? "HANGAR 2" : "HANGAR 1", lit);
          break;
        case Look::Station:
          paintStation(cr, w, h, lit);
          break;
        case Look::Halcyon:
          paintHalcyon(cr, w, h, lit);
          break;
        case Look::DecoTower:
          paintTower(cr, w, h, 0, lit);
          break;
        case Look::Lighthouse:
          paintTower(cr, w, h, 1, lit);
          break;
        case Look::Control:
          paintTower(cr, w, h, 2, lit);
          break;
      }
    };
    static const Color kWall[11] = {rgb(240, 210, 190), rgb(210, 214, 226), rgb(176, 120, 76), rgb(200, 190, 170),
      rgb(184, 184, 178), rgb(170, 176, 170), rgb(214, 204, 182), rgb(236, 226, 200), rgb(196, 112, 82), rgb(244, 240, 232),
      rgb(214, 210, 198)};
    const Color wallC = kWall[int(look)];
    const Color roofC = look == Look::House ? rgb(184, 72, 62) : darken(wallC, 0.3f);
    const auto rubble = [&](float alphaR) {
      const int rh = std::min(h, std::max(40, std::min(90, w / 3)));
      DrawOpts o;
      o.alpha = alphaR;
      r.draw(baked(mArt, r, "rubble/" + b.id + "/" + std::to_string(w), w + 24, rh, 12.0f, float(rh),
               [&](cairo_t* cr) {
                 cairo_translate(cr, 2, 0);
                 paintRubble(cr, w + 20, rh, wallC, roofC, seed);
               }),
        x, y + float(h) + 4.0f, o);
    };
    const bool tower = b.kind == BuildingKind::Tower;
    const float ax = b.fall > 0 ? float(w) : 0.0f;
    const auto texFor = [&]() -> const Texture& {
      return baked(mArt, r, key + (tower ? (b.fall > 0 ? "/r" : "/l") : ""), w, h, tower ? ax : 0.0f, tower ? float(h) : 0.0f, paint);
    };
    switch (b.state)
    {
      case BuildingState::Standing:
      {
        if (offScreen(x - 80.0f, y, float(w) + 160.0f, float(h)))
          break;
        // Cracks: the last few frames before it gives, it shudders.
        const float crack = float(b.cracks) / float(kRoofCrack);
        const float shake = crack > 0.6f ? ((frame / 2) % 2 ? 1.5f : -1.5f) : 0.0f;
        if (tower)
          r.draw(texFor(), x + ax + shake, y + float(h));
        else
          r.draw(texFor(), x + shake, y);
        if (look == Look::Lighthouse && lit)
        {
          // The lamp turning.
          const float a = float(frame) * 0.05f;
          const float lx = x + float(w) * 0.5f, ly = y + float(h) * 0.13f;
          r.drawLine(lx, ly, lx + std::cos(a) * 340.0f, ly + std::sin(a) * 40.0f, 30.0f * std::abs(std::cos(a)),
            withAlpha(rgb(255, 240, 180), 50), Blend::Add);
          drawGlow(r, mArt, lx, ly, 60.0f, rgb(255, 240, 180), 0.5f + 0.3f * std::abs(std::cos(a)));
        }
        if (look == Look::Cooling)
          for (int k = 0; k < 4; ++k)
          {
            const float u = std::fmod(float(frame) * 0.006f + float(k) * 0.25f + float(seed % 7u) * 0.1f, 1.0f);
            drawGlow(r, mArt, x + float(w) * 0.5f + std::sin(u * 5.0f + float(k)) * 14.0f, y - u * 160.0f, 40.0f + u * 70.0f,
              rgb(230, 230, 240), 0.35f * std::sin(u * 3.14159f));
          }
        if (int(i) == c.diner)
        {
          // The three figurines in their booth: they turn to watch you go
          // by, and wave when you leave the diner standing.
          static const Color kHair[3] = {rgb(255, 214, 72), rgb(42, 30, 24), rgb(255, 86, 184)};
          static const Color kTop[3] = {rgb(232, 64, 52), rgb(104, 146, 64), rgb(132, 66, 216)};
          const float px = (float(mPlayer.x) + 1.5f) * kCellPx - camX;
          for (int k = 0; k < 3; ++k)
          {
            const float fx = x + float(w) * (0.17f + 0.12f * float(k)), fy = y + float(h) * 0.5f;
            const float look2 = std::clamp((px - fx) / 200.0f, -1.0f, 1.0f);
            const bool waving = c.wave > 0;
            r.fillRect(fx - 5.0f, fy - 2.0f, 10.0f, 10.0f, kTop[k]);
            if (waving)
            {
              const float wa = std::sin(float(frame) * 0.5f + float(k)) * 4.0f;
              r.drawLine(fx + 4.0f, fy, fx + 9.0f + wa, fy - 12.0f, 3.0f, kTop[k]);
              r.fillRect(fx + 7.0f + wa, fy - 15.0f, 4.0f, 4.0f, rgb(246, 200, 160));
            }
            r.fillRect(fx - 4.0f, fy - 11.0f, 8.0f, 9.0f, rgb(246, 200, 160));
            r.fillRect(fx - 4.0f + (look2 > 0.0f ? 1.0f : -1.0f) * 1.0f, fy - 13.0f, 8.0f, 4.0f, kHair[k]);
            r.fillRect(fx - 1.0f + look2 * 2.5f, fy - 8.0f, 2.0f, 2.0f, kInk);
          }
          if (c.wave > 0)
            drawGlow(r, mArt, x + float(w) * 0.3f, y + float(h) * 0.45f, 90.0f, rgb(255, 230, 160), 0.3f);
        }
        if (crack > 0.0f)
        {
          // Cracks spreading from three points along the roof.
          for (int k = 0; k < 3; ++k)
          {
            const unsigned hh = hash2(int(i) * 17 + k, 26);
            float cx = x + float(w) * (0.2f + 0.3f * float(k)) + float(hh % 20u) - 10.0f, cy = y + 2.0f;
            const int segs = 1 + int(crack * 6.0f);
            for (int s = 0; s < segs; ++s)
            {
              const unsigned hs = hash2(int(i) * 31 + k * 7 + s, 9);
              const float nx = cx + float(int(hs % 21u) - 10), ny = cy + 6.0f + float(hs % 9u);
              r.drawLine(cx, cy, nx, ny, 3.0f, rgb(30, 22, 20));
              r.drawLine(cx + 1.0f, cy, nx + 1.0f, ny, 1.0f, withAlpha(rgb(255, 240, 220), 120));
              cx = nx;
              cy = ny;
            }
          }
          r.fillRect(x, y - 3.0f, float(w), 3.0f, withAlpha(rgb(255, 220, 160), int(140.0f * crack * float((frame / 3) % 2))));
          for (int k = 0; k < 3; ++k)
          {
            const float u = std::fmod(float(frame) * 0.05f + float(k) * 0.33f, 1.0f);
            r.fillRect(x + float(w) * (0.25f + 0.25f * float(k)), y + u * float(h) * 0.6f, 3.0f, 3.0f,
              withAlpha(rgb(200, 190, 170), int(200.0f * (1.0f - u))));
          }
        }
        if (b.burn > 0)
        {
          // Scorched where the breath plays on it; flames lick up.
          const float u = float(b.burn) / float(kBurnFrames);
          r.fillRect(x, y, float(w), float(h), withAlpha(rgb(20, 10, 8), int(90.0f * u)));
          const float hx = std::clamp(c.beamX1 * kCellPx - camX, x, x + float(w));
          const float hy = std::clamp(c.beamY1 * kCellPx - camY, y, y + float(h));
          drawGlow(r, mArt, hx, hy, 70.0f + 40.0f * u, rgb(255, 140, 50), 0.5f + 0.4f * u);
          for (int k = 0; k < 3; ++k)
          {
            DrawOpts fo;
            fo.blend = Blend::Add;
            fo.scale = 0.6f + 0.6f * u + 0.15f * std::sin(float(frame) * 0.7f + float(k));
            r.draw(baked(mArt, r, "flame", 44, 60, 22.0f, 60.0f, [](cairo_t* cr) { paintFlame(cr); }), hx + float(k - 1) * 16.0f,
              hy + 10.0f, fo);
          }
        }
        if (tower)
        {
          // The road sign by its foot, pointing at it.
          const float sx = x - 70.0f, sy = y + float(h) + 4.0f;
          r.draw(baked(mArt, r, "arrowsign", 60, 110, 30.0f, 110.0f, [](cairo_t* cr) { paintArrowSign(cr); }), sx, sy);
        }
        break;
      }
      case BuildingState::Crumbling:
      {
        if (offScreen(x - 60.0f, y - 60.0f, float(w) + 120.0f, float(h) + 120.0f))
          break;
        const float t = std::min(1.0f, (float(b.t) + alpha) / float(kCrumbleFrames));
        rubble(t);
        // It sinks into its own dust, tilting; clipped at the street.
        SDL_Rect clip{int(x) - 40, int(y) - 200, w + 80, h + 200};
        SDL_RenderSetClipRect(r.sdl(), &clip);
        DrawOpts o;
        o.angle = (b.x0 % 2 ? 1.0f : -1.0f) * 6.0f * t;
        o.alpha = 1.0f - 0.4f * t;
        r.draw(texFor(), x + (tower ? ax : 0.0f), y + float(h) * t * t + (tower ? float(h) : 0.0f), o);
        SDL_RenderSetClipRect(r.sdl(), nullptr);
        const Texture& dust = baked(mArt, r, "dust", 80, 80, 40.0f, 40.0f, [](cairo_t* cr) { paintDust(cr); });
        for (int k = 0; k < 2 + w / 48; ++k)
        {
          DrawOpts d;
          d.scale = 0.8f + 1.4f * t + float(k % 3) * 0.2f;
          d.alpha = 0.9f * (1.0f - t * t);
          r.draw(dust, x + float(w) * (float(k) + 0.5f) / float(2 + w / 48), y + float(h) - 20.0f - t * 40.0f - float(k % 2) * 30.0f, d);
        }
        break;
      }
      case BuildingState::Down:
        if (!offScreen(x - 20.0f, y, float(w) + 40.0f, float(h) + 20.0f))
          rubble(1.0f);
        break;
      case BuildingState::Leaning:
      case BuildingState::Falling:
      {
        // Swinging down about its foot on the falling side.
        const float t = b.state == BuildingState::Leaning
          ? 0.035f + 0.03f * std::sin((float(b.t) + alpha) * 2.2f)
          : 0.06f + 0.94f * std::pow(std::min(1.0f, (float(b.t) + alpha) / float(kTowerFall)), 2.0f);
        DrawOpts o;
        o.angle = 90.0f * t * float(b.fall);
        const float fx = x + ax, fy = y + float(h);
        r.draw(texFor(), fx, fy, o);
        if (b.state == BuildingState::Leaning)
        {
          // The creak: motion arrows the way it will go, and the word.
          DrawOpts ao;
          ao.alpha = 0.6f + 0.4f * float((frame / 4) % 2);
          const Texture& arrow = baked(mArt, r, b.fall > 0 ? "creak_r" : "creak_l", 90, 70, 45.0f, 35.0f, [&](cairo_t* cr) {
            if (b.fall < 0)
            {
              cairo_translate(cr, 90, 0);
              cairo_scale(cr, -1, 1);
            }
            paintCreak(cr);
          });
          r.draw(arrow, fx + float(b.fall) * 60.0f, fy - float(h) * 0.85f, ao);
          r.draw(arrow, fx + float(b.fall) * 40.0f, fy - float(h) * 0.6f, ao);
          r.drawText("CREAK!", fx - float(b.fall) * 30.0f + std::sin(float(frame) * 0.9f) * 3.0f, fy - float(h) - 40.0f,
            {26.0f, rgb(255, 240, 200), kInk, true}, Align::Center);
        }
        else
        {
          // Speed lines off its top.
          const float a = (90.0f * t * float(b.fall)) * 0.01745f;
          const float tx = fx + std::sin(a) * float(h) - std::cos(a) * float(w) * 0.5f * float(b.fall);
          const float ty = fy - std::cos(a) * float(h);
          for (int k = 0; k < 3; ++k)
            r.drawLine(tx - float(b.fall) * float(30 + k * 20), ty - 20.0f + float(k) * 22.0f, tx - float(b.fall) * float(90 + k * 20),
              ty - 40.0f + float(k) * 22.0f, 3.0f, withAlpha(rgb(255, 255, 255), 150));
        }
        break;
      }
      case BuildingState::Lying:
      {
        // Lying as a bridge: the tower on its side, rubble at its foot.
        const float bx = float(b.bx0) * kTilePx - camX, by = float(b.by0) * kTilePx - camY;
        const float bw = float(b.bx1 - b.bx0 + 1) * kTilePx, bh = float(b.by1 - b.by0 + 1) * kTilePx;
        if (offScreen(bx - 64.0f, by - 64.0f, bw + 128.0f, bh + 128.0f))
          break;
        DrawOpts o;
        o.angle = 90.0f * float(b.fall);
        r.draw(texFor(), x + ax, y + float(h), o);
        const float settle = std::min(1.0f, float(b.t) / 1.0f);
        (void)settle;
        break;
      }
    }
  }

  // The ocean liner, the harbor dish on its deck, the exit over them.
  const auto& z = c.lizard;
  if (z.on)
  {
    // The liner: from the dish to the level's end, the deck under the dish.
    const int deckRow = z.dishY / kCellsPerTile + 1;
    int bx0 = z.dishX / kCellsPerTile;
    while (bx0 > 0 && mLevel->isSolid(bx0 - 1, deckRow))
      --bx0;
    const float lx = float(bx0) * kTilePx - camX, ly = float(deckRow) * kTilePx - camY;
    const float lw = float(W - bx0) * kTilePx, lh = 3.0f * kTilePx + 40.0f;
    if (!offScreen(lx, ly - 140.0f, lw, lh + 140.0f))
    {
      r.draw(baked(mArt, r, "liner/" + std::to_string(int(lw)), int(lw), int(lh + 140.0f), 0.0f, 140.0f,
               [lw, lh](cairo_t* cr) { paintLiner(cr, lw, lh + 140.0, 140.0); }),
        lx, ly);
      drawExit(r, mArt, mTheme, float(mLevel->exitTx) * kTilePx - 32.0f - camX, float(mLevel->exitTy + 1) * kTilePx - camY, frame);
    }
    // The dish.
    const float dx = (float(z.dishX) + 0.5f) * kCellPx - camX, dy = float(z.dishY + 1) * kCellPx - camY;
    if (!offScreen(dx - 120.0f, dy - 160.0f, 240.0f, 200.0f))
    {
      DrawOpts o;
      if (z.state == LizardState::Roar && z.beamRow >= 0)
      {
        // Tilted onto the row it will fire along.
        const float ty = (float(z.beamRow) + 0.5f) * kCellPx - camY;
        o.angle = std::clamp((ty - (dy - 94.0f)) * 0.08f, -25.0f, 25.0f) * -1.0f;
      }
      r.draw(baked(mArt, r, "harbordish", 120, 140, 60.0f, 140.0f, [](cairo_t* cr) { paintHarborDish(cr); }), dx, dy, o);
    }
    // Gorgazilla.
    if (z.state != LizardState::Under)
    {
      const float gx = (z.prevX + (z.x - z.prevX) * alpha + float(Lizard::kW) * 0.5f) * kCellPx - camX;
      const float gy = float(z.floor + 1) * kCellPx - camY;
      int pose = (frame / 10) % 4;
      int fr = (frame / 10) % 4;
      float sink = 0.0f, lean = 0.0f;
      switch (z.state)
      {
        case LizardState::Roar:
          pose = z.t < 4 ? (frame / 10) % 4 : 4;
          break;
        case LizardState::Swipe:
          pose = z.t < kSwipeTell ? 5 : 6;
          break;
        case LizardState::Stagger:
          pose = 7 + (frame / 6) % 2;
          lean = std::sin(float(frame) * 0.3f) * 6.0f;
          break;
        case LizardState::Dive:
        {
          // Wading in: the further out over the bay, the deeper.
          const float bay = float(z.piers[0][1] + Lizard::kW + z.piers[1][0]) * 0.5f - float(Lizard::kW) * 0.5f;
          const float from = std::abs(float(z.startX) - bay) > 1.0f ? std::abs(z.x - bay) : 0.0f;
          sink = std::clamp(1.0f - from / 6.0f, 0.0f, 1.0f) * float(Lizard::kH) * kCellPx * 0.9f;
          break;
        }
        case LizardState::Sit:
          pose = 9 + std::clamp(z.t * 5 / 45, 0, 4);
          break;
        case LizardState::Leave:
        case LizardState::Done:
          pose = 14;
          fr = 0;
          break;
        default:
          break;
      }
      const bool show = z.state != LizardState::Waiting || true;
      if (show)
      {
        const float wade = sink;
        SDL_Rect clip{0, 0, kScreenW, int(gy + 4.0f)};
        if (wade > 0.0f)
          SDL_RenderSetClipRect(r.sdl(), &clip);
        DrawOpts o;
        if (z.flash > 0 && (frame / 2) % 2)
          o.tint = rgb(255, 255, 255);
        const float sx = z.state >= LizardState::Leave ? float(z.x + float(Lizard::kW) * 0.5f) * kCellPx - camX : gx;
        const Texture& tex = styledEnemySprite(mArt, r, mTheme, "gorgazilla", pose, fr, Lizard::kW, Lizard::kH).get(z.dir);
        r.draw(tex, sx + lean, gy + wade, o);
        if (z.flash > 0)
        {
          DrawOpts f;
          f.blend = Blend::Add;
          f.alpha = float(z.flash) / 4.0f;
          r.draw(tex, sx + lean, gy + wade, f);
        }
        if (wade > 0.0f)
          SDL_RenderSetClipRect(r.sdl(), nullptr);
        if (z.state == LizardState::Stagger)
          for (int k = 0; k < 3; ++k)
          {
            // Stars round his head.
            const float a = float(frame) * 0.15f + float(k) * 2.094f;
            const float hx = sx + float(z.dir) * 20.0f + std::cos(a) * 40.0f, hy = gy - float(Lizard::kH) * kCellPx - 6.0f + std::sin(a) * 10.0f;
            drawGlow(r, mArt, hx, hy, 14.0f, rgb(255, 240, 120), 0.9f);
            r.fillRect(hx - 2.0f, hy - 2.0f, 4.0f, 4.0f, rgb(255, 255, 220));
          }
        if (z.state == LizardState::Roar && z.t >= 4)
        {
          // The roar's rings off his mouth.
          for (int k = 0; k < 3; ++k)
          {
            const float u = std::fmod(float(frame) * 0.06f + float(k) * 0.33f, 1.0f);
            const float rx = sx + float(z.dir) * (50.0f + u * 120.0f), ry = gy - float(Lizard::kH) * kCellPx + 10.0f - u * 30.0f;
            for (int s = 0; s < 6; ++s)
            {
              const float a0 = -0.9f + float(s) * 0.3f, a1 = a0 + 0.22f;
              const float rr = 20.0f + u * 50.0f;
              r.drawLine(rx + float(z.dir) * std::cos(a0) * rr, ry + std::sin(a0) * rr, rx + float(z.dir) * std::cos(a1) * rr,
                ry + std::sin(a1) * rr, 3.0f, withAlpha(rgb(255, 255, 255), int(200.0f * (1.0f - u))));
            }
          }
        }
      }
      if (z.state == LizardState::Leave)
      {
        // The tired man walking off set, waving at first.
        const float mx = (z.manX + float(Lizard::kW) * 0.5f) * kCellPx - camX;
        const bool waving = z.t < 24;
        r.draw(styledEnemySprite(mArt, r, mTheme, "tired_man", waving ? 1 : 0, (frame / 8) % 4, 3, 5).get(waving ? 1 : -1), mx, gy);
      }
    }
  }

  // The model water of the canal and the bay (over a wading Gorgazilla).
  for (const auto& w : c.ponds)
  {
    const float x = float(w.x) * kCellPx - camX, y = float(w.y) * kCellPx - camY;
    const int ww = w.w * int(kCellPx), h = w.h * int(kCellPx);
    if (offScreen(x, y - 30.0f, float(ww), float(h) + 30.0f))
      continue;
    DrawOpts wo;
    wo.alpha = 0.92f;
    r.draw(baked(mArt, r, "pond/" + std::to_string(ww) + "x" + std::to_string(h), ww, h, 0, 0,
             [&](cairo_t* cr) { paintPond(cr, ww, h); }),
      x, y, wo);
    const Texture& ripple = baked(mArt, r, "ripple", 160, 40, 80.0f, 20.0f, [](cairo_t* cr) { paintRipple(cr); });
    for (int i = 0; i < std::max(1, ww / 180); ++i)
    {
      const float period = 120.0f + float(i % 3) * 31.0f;
      const float t = std::fmod(float(frame) + alpha + float(i) * 47.0f, period) / period;
      DrawOpts o;
      o.scale = 0.25f + t * 0.8f;
      o.alpha = 0.5f * (1.0f - t);
      r.draw(ripple, x + 40.0f + float(hash2(i, w.x) % unsigned(std::max(1, ww - 80))), y + 10.0f, o);
    }
    for (int i = 0; i < ww / 70; ++i)
    {
      const float sx = x + float(hash2(i, 77) % unsigned(ww)) + std::sin(float(frame) * 0.03f + float(i)) * 16.0f;
      const float a = 0.5f + 0.5f * std::sin(float(frame) * 0.07f + float(i) * 2.3f);
      r.fillRect(sx, y + 4.0f + float(i % 3) * 3.0f, 22.0f, 2.0f, withAlpha(rgb(230, 250, 255), int(150.0f * a)));
    }
  }

  // The arena's gate, shut behind you.
  if (z.on && z.doorShut)
  {
    const float x = float(z.doorX) * kTilePx - camX, y = float(z.doorY0) * kTilePx - camY;
    const float h = float(z.doorY1 - z.doorY0 + 1) * kTilePx;
    if (!offScreen(x, y, kTilePx, h))
      r.draw(baked(mArt, r, "gate/" + std::to_string(int(h)), 64, int(h), 0, 0, [h](cairo_t* cr) { paintGate(cr, 64, h); }), x, y);
  }

  // The full heal.
  if (c.fullhealX >= 0 && !c.fullhealUsed)
  {
    const float x = float(c.fullhealX) * kTilePx - camX + 32.0f, y = float(c.fullhealY + 1) * kTilePx - camY;
    if (!offScreen(x - 40.0f, y - 120.0f, 80.0f, 120.0f))
    {
      drawGlow(r, mArt, x, y - 50.0f, 80.0f, rgb(255, 120, 140), 0.35f + 0.15f * std::sin(float(frame) * 0.1f));
      r.draw(baked(mArt, r, "fullheal", 64, 96, 32.0f, 96.0f, [](cairo_t* cr) { paintFullHeal(cr); }), x, y);
      for (int k = 0; k < 2; ++k)
      {
        const float u = std::fmod(float(frame) * 0.012f + float(k) * 0.5f, 1.0f);
        r.drawText("+", x - 14.0f + float(k) * 28.0f, y - 110.0f - u * 50.0f, {22.0f, rgb(255, 140, 160), kInk}, Align::Center,
          std::sin(u * 3.14159f));
      }
    }
  }

  // The blacked-out sectors.
  for (const auto& s : c.sectors)
  {
    if (!s.dark)
      continue;
    const float x0 = std::max(0.0f, float(s.x0) * kTilePx - camX), x1 = std::min(float(kScreenW), float(s.x1 + 1) * kTilePx - camX);
    if (x1 <= x0)
      continue;
    r.fillRect(x0, 0.0f, x1 - x0, float(kScreenH), rgba(6, 8, 30, 120));
  }

  // The Toy Jets' wires, up out of the top of the frame.
  for (const auto& e : mEnemies)
    if (e.alive && !e.hidden && e.kind == EnemyKind::ToyJet && e.tell == 0)
    {
      const float jx = (e.drawX + float(e.w) * 0.5f) * kCellPx - camX, jy = (e.drawY - float(e.h) + 1.4f) * kCellPx - camY;
      if (jx > -40.0f && jx < float(kScreenW) + 40.0f)
        r.drawLine(jx, jy, jx - float(e.dir) * 20.0f, -10.0f, 1.5f, withAlpha(rgb(210, 210, 230), 140));
    }
}

// --- In front of the runner ------------------------------------------------------------------

void World::drawCityFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& c = mCity;
  if (mTop.on)
    return;
  // The Maser Trucks: the ring growing on the dish, the beam.
  for (const auto& e : mEnemies)
  {
    if (!e.alive || e.hidden || e.kind != EnemyKind::MaserTruck)
      continue;
    const float ex = e.drawX * kCellPx - camX, ey = (e.drawY - float(e.h) + 1.0f) * kCellPx - camY;
    const float w = float(e.w) * kCellPx;
    // The emitter's tip (maser_truck art: the dish at 0.5 w, the bulb 0.16 w ahead, at 0.28 h).
    const float tipX = e.dir > 0 ? ex + w * 0.66f + 0.0f : ex + w * 0.34f, tipY = ey + float(e.h) * kCellPx * 0.28f;
    if (e.tell > 0 && !sectorDark(e.variant))
    {
      const float u = 1.0f - float(e.tell) / float(std::max(1, enemyDef(e.def).tell));
      const float rr = 10.0f + 46.0f * u;
      for (int s = 0; s < 24; ++s)
      {
        const float a0 = float(s) * 0.2618f, a1 = a0 + 0.2618f;
        r.drawLine(tipX + std::cos(a0) * rr * 0.45f, tipY + std::sin(a0) * rr, tipX + std::cos(a1) * rr * 0.45f, tipY + std::sin(a1) * rr,
          3.0f + 3.0f * u, withAlpha(rgb(140, 235, 255), int(120 + 120 * u)), Blend::Add);
      }
      drawGlow(r, mArt, tipX, tipY, 40.0f + 50.0f * u, rgb(120, 230, 255), 0.4f + 0.5f * u);
    }
    if (e.dive > 0)
    {
      const float x1 = (float(e.aimX) + 0.5f) * kCellPx - camX, y = (float(e.aimY) + 0.5f) * kCellPx - camY;
      const float x0 = tipX;
      const float fade = float(e.dive) / float(kMaserShow);
      r.drawLine(x0, tipY, x0 + float(e.dir) * 30.0f, y, 10.0f, withAlpha(rgb(140, 235, 255), int(200 * fade)), Blend::Add);
      r.drawLine(x0, y, x1, y, 26.0f, withAlpha(rgb(90, 200, 255), int(130 * fade)), Blend::Add);
      r.drawLine(x0, y, x1, y, 10.0f, withAlpha(rgb(200, 250, 255), int(220 * fade)), Blend::Add);
      r.drawLine(x0, y, x1, y, 3.0f, rgb(255, 255, 255));
      // Its rings travelling along.
      for (int k = 0; k < 8; ++k)
      {
        const float u = std::fmod(float(k) / 8.0f + float(frame) * 0.04f, 1.0f);
        const float rx = x0 + (x1 - x0) * u;
        r.drawLine(rx, y - 18.0f, rx + float(e.dir) * 6.0f, y + 18.0f, 3.0f, withAlpha(rgb(200, 250, 255), int(160 * fade)), Blend::Add);
      }
      drawGlow(r, mArt, x1, y, 50.0f, rgb(160, 240, 255), 0.7f * fade);
    }
  }
  // The Atomic Breath: a blue-green beam, a white-hot core, flickering; a
  // flare where it leaves the gun and where it strikes.
  if (c.breath > 0)
  {
    const float x0 = c.beamX0 * kCellPx - camX, y0 = c.beamY0 * kCellPx - camY;
    const float x1 = c.beamX1 * kCellPx - camX, y1 = c.beamY1 * kCellPx - camY;
    const float flick = 0.5f + 0.5f * std::sin(float(frame) * 1.3f);
    r.drawLine(x0, y0, x1, y1, 44.0f + 8.0f * flick, withAlpha(rgb(40, 200, 170), 70), Blend::Add);
    r.drawLine(x0, y0, x1, y1, 26.0f + 4.0f * flick, withAlpha(rgb(90, 255, 210), 150), Blend::Add);
    r.drawLine(x0, y0, x1, y1, 12.0f, withAlpha(rgb(190, 255, 240), 220), Blend::Add);
    r.drawLine(x0, y0, x1, y1, 5.0f, rgb(255, 255, 255));
    const float len = std::hypot(x1 - x0, y1 - y0);
    const float dx = len > 0.0f ? (x1 - x0) / len : 1.0f, dy = len > 0.0f ? (y1 - y0) / len : 0.0f;
    // Crackles of energy spiralling along it.
    for (int k = 0; k < 10; ++k)
    {
      const float u = std::fmod(float(k) * 0.1f + float(frame) * 0.05f, 1.0f);
      const float px = x0 + dx * len * u, py = y0 + dy * len * u;
      const float o = std::sin(u * 18.0f + float(frame) * 0.4f) * 14.0f;
      r.fillRect(px - dy * o - 2.0f, py + dx * o - 2.0f, 4.0f, 4.0f, withAlpha(rgb(220, 255, 240), 200));
    }
    drawGlow(r, mArt, x0, y0, 46.0f, rgb(120, 255, 210), 0.8f);
    drawGlow(r, mArt, x1, y1, 70.0f + 14.0f * flick, rgb(150, 255, 220), 0.85f);
    for (int k = 0; k < 6; ++k)
    {
      const unsigned hh = hash2(k, frame);
      const float a = float(hh % 628u) / 100.0f, d = float((hh >> 10) % 40u);
      r.fillRect(x1 + std::cos(a) * d, y1 + std::sin(a) * d, 3.0f, 3.0f, rgb(230, 255, 245));
    }
  }
  // Gorgazilla's tail swipe; the ripples where he will come up; his splash;
  // the harbor dish's ring and its beam.
  const auto& z = c.lizard;
  if (!z.on)
    return;
  const float floorY = float(z.floor + 1) * kCellPx - camY;
  if (z.state == LizardState::Swipe && z.t >= kSwipeTell)
  {
    const float u = std::min(1.0f, (float(z.t - kSwipeTell) + alpha) / 4.0f);
    const float cx = (z.x + float(Lizard::kW) * 0.5f) * kCellPx - camX;
    const float reach = (8.0f + float(Lizard::kW) * 0.5f) * kCellPx;
    for (const int side : {-1, 1})
    {
      // An arc of motion and a skirt of dust along the floor.
      for (int s = 0; s < 10; ++s)
      {
        const float a0 = float(s) / 10.0f, a1 = float(s + 1) / 10.0f;
        if (a1 > u)
          break;
        r.drawLine(cx + float(side) * reach * a0, floorY - 26.0f + a0 * 14.0f, cx + float(side) * reach * a1, floorY - 26.0f + a1 * 14.0f,
          14.0f * (1.0f - a0 * 0.6f), withAlpha(rgb(150, 200, 130), int(200 * (1.0f - u * 0.5f))));
      }
      drawGlow(r, mArt, cx + float(side) * reach * u, floorY - 14.0f, 50.0f, rgb(230, 220, 200), 0.6f * (1.0f - u * 0.4f));
    }
  }
  if (z.state == LizardState::Under && z.t >= kUnder - 20 && z.splashX >= 0)
  {
    const float sx = (float(z.splashX) + float(Lizard::kW) * 0.5f) * kCellPx - camX;
    const Texture& ripple = baked(mArt, r, "ripple", 160, 40, 80.0f, 20.0f, [](cairo_t* cr) { paintRipple(cr); });
    for (int k = 0; k < 3; ++k)
    {
      const float u = std::fmod(float(frame) * 0.025f + float(k) * 0.33f, 1.0f);
      DrawOpts o;
      o.scale = 0.4f + u;
      o.alpha = 0.9f * (1.0f - u);
      r.draw(ripple, sx, floorY - 4.0f, o);
    }
    // The pier's boards quiver.
    r.fillRect(sx - 60.0f, floorY - 3.0f + float((frame / 2) % 2) * 2.0f, 120.0f, 3.0f, withAlpha(rgb(220, 240, 255), 160));
  }
  if (z.surfaced > 0)
  {
    const float sx = (z.x + float(Lizard::kW) * 0.5f) * kCellPx - camX;
    const float u = 1.0f - float(z.surfaced) / 10.0f;
    for (int k = -3; k <= 3; ++k)
    {
      const float hx = sx + float(k) * 40.0f, hh = (80.0f - float(std::abs(k)) * 18.0f) * std::sin(u * 3.14159f);
      r.drawLine(hx, floorY, hx + float(k) * 10.0f * u, floorY - hh, 10.0f, withAlpha(rgb(200, 236, 255), int(200 * (1.0f - u))));
    }
  }
  const float dx = (float(z.dishX) + 0.5f) * kCellPx - camX - 52.0f, dy = float(z.dishY + 1) * kCellPx - camY - 94.0f;
  if (z.state == LizardState::Roar && z.t >= kRoarLen - kRoarLock)
  {
    const float u = std::min(1.0f, (float(z.t - (kRoarLen - kRoarLock)) + alpha) / float(kRoarLock));
    const float rr = 12.0f + 60.0f * u;
    for (int s = 0; s < 24; ++s)
    {
      const float a0 = float(s) * 0.2618f, a1 = a0 + 0.2618f;
      r.drawLine(dx + std::cos(a0) * rr * 0.4f, dy + std::sin(a0) * rr, dx + std::cos(a1) * rr * 0.4f, dy + std::sin(a1) * rr,
        3.0f + 4.0f * u, withAlpha(rgb(140, 235, 255), int(110 + 140 * u)), Blend::Add);
    }
    drawGlow(r, mArt, dx, dy, 40.0f + 60.0f * u, rgb(120, 230, 255), 0.4f + 0.5f * u);
    if (z.beamRow >= 0)
    {
      // The lock: a thin sight line along the row it will fire down.
      const float y = (float(z.beamRow) + 0.5f) * kCellPx - camY;
      r.drawLine(float(z.x0) * kTilePx - camX, y, dx, y, 2.0f, withAlpha(rgb(140, 235, 255), int(80 + 80 * ((frame / 3) % 2))), Blend::Add);
    }
  }
  if (z.beam > 0 && z.beamRow >= 0)
  {
    const float y = (float(z.beamRow) + 0.5f) * kCellPx - camY;
    const float x0 = float(z.x0) * kTilePx - camX;
    const float fade = float(z.beam) / float(kMaserShow);
    r.drawLine(dx, dy, dx - 30.0f, y, 12.0f, withAlpha(rgb(140, 235, 255), int(200 * fade)), Blend::Add);
    r.drawLine(x0, y, dx - 30.0f, y, 34.0f, withAlpha(rgb(90, 200, 255), int(120 * fade)), Blend::Add);
    r.drawLine(x0, y, dx - 30.0f, y, 14.0f, withAlpha(rgb(200, 250, 255), int(220 * fade)), Blend::Add);
    r.drawLine(x0, y, dx - 30.0f, y, 4.0f, rgb(255, 255, 255));
    for (int k = 0; k < 12; ++k)
    {
      const float u = std::fmod(float(k) / 12.0f + float(frame) * 0.05f, 1.0f);
      const float rx = dx - 30.0f + (x0 - dx + 30.0f) * u;
      r.drawLine(rx, y - 22.0f, rx - 6.0f, y + 22.0f, 3.0f, withAlpha(rgb(200, 250, 255), int(160 * fade)), Blend::Add);
    }
  }
}

// --- The HUD ---------------------------------------------------------------------------------

void World::drawCityHud(Renderer& r, int frame) const
{
  const auto& c = mCity;
  if (mTop.on || c.buildings.empty())
    return;
  // The DAMAGE meter: a seismograph dial under the score, the needle
  // kicking with each break.
  const float px = float(kScreenW) - 232.0f, py = 84.0f;
  r.draw(baked(mArt, r, "meter", 220, 64, 0, 0, [](cairo_t* cr) { paintMeterPanel(cr); }), px, py);
  const float kick = c.ratingFlash > 0 ? float(c.ratingFlash) / 20.0f : 0.0f;
  const float a = 3.14159f * (1.15f + 0.7f * std::min(1.0f, float(c.rating) / 60000.0f) + 0.15f * kick * std::sin(float(frame) * 1.7f));
  r.drawLine(px + 34.0f, py + 50.0f, px + 34.0f + std::cos(a) * 22.0f, py + 50.0f + std::sin(a) * 22.0f, 3.0f, rgb(255, 90, 60));
  r.fillRect(px + 31.0f, py + 47.0f, 6.0f, 6.0f, rgb(250, 230, 200));
  r.drawText("DAMAGE", px + 70.0f, py + 6.0f, {14.0f, rgb(255, 170, 110), kInk});
  const Color col = c.ratingFlash > 0 && (frame / 3) % 2 ? rgb(255, 255, 255) : rgb(255, 214, 90);
  r.drawText("$" + std::to_string(c.rating), px + 70.0f, py + 24.0f, {26.0f + 6.0f * kick, col, kInk});
  if (lizardFight())
  {
    // Gorgazilla's bar along the bottom, a notch at each phase.
    const float bw = 600.0f, bx = (float(kScreenW) - bw) * 0.5f, by = float(kScreenH) - 54.0f;
    r.fillRect(bx - 62.0f, by - 30.0f, bw + 72.0f, 52.0f, rgba(8, 6, 22, 200));
    r.draw(baked(mArt, r, "lizicon", 44, 40, 0, 0, [](cairo_t* cr) { paintLizardIcon(cr); }), bx - 56.0f, by - 26.0f);
    r.drawText("GORGAZILLA", bx, by - 26.0f, {17.0f, rgb(140, 230, 140), kInk});
    const auto& z = c.lizard;
    r.drawText(std::string("TAKE ") + std::to_string(z.phase), bx + bw, by - 26.0f, {15.0f, rgb(250, 230, 200), kInk}, Align::Right);
    const float frac = float(lizardHp()) / float(3 * Lizard::kHp);
    r.fillRect(bx, by, bw, 12.0f, rgba(255, 255, 255, 40));
    r.fillRect(bx, by, bw * frac, 12.0f, z.flash > 0 && (frame / 2) % 2 ? rgb(255, 255, 255) : rgb(90, 220, 90));
    for (const float cut : {1.0f / 3.0f, 2.0f / 3.0f})
      r.fillRect(bx + bw * cut - 1.0f, by - 3.0f, 3.0f, 18.0f, rgb(20, 16, 30));
  }
}

// The runner from straight above, in the Top-Down Rampage.
void World::drawTopRunner(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& p = mPlayer;
  const float cx = (float(p.prevX) + (float(p.x) - float(p.prevX)) * alpha + float(Player::kWidth) * 0.5f) * kCellPx - camX;
  const float cy = (float(p.prevY) + (float(p.y) - float(p.prevY)) * alpha - float(p.height()) * 0.5f + 1.0f) * kCellPx - camY;
  const TopColors tc = topColors(mCharacter);
  const bool moving = p.x != p.prevX || p.y != p.prevY;
  const int step = moving ? 1 + (frame / 6) % 2 : 0;
  const std::string key = "toprunner/" + mCharacter.id + "/" + std::to_string(step);
  DrawOpts o;
  o.angle = float(std::atan2(double(mTop.fy), double(mTop.fx)) * 180.0 / kPi);
  if (p.mercy > 10)
    o.alpha = 0.45f + 0.25f * std::sin(float(frame) * 0.35f);
  const Texture& tex = baked(mArt, r, key, 96, 96, 48.0f, 48.0f, [&](cairo_t* cr) { paintTopRunner(cr, tc, step); });
  r.draw(tex, cx, cy, o);
  if (p.mercy > 0 && p.mercy <= 10)
  {
    o.blend = Blend::Add;
    o.alpha = 0.6f;
    r.draw(tex, cx, cy, o);
  }
  if (p.muzzleTicks > 0)
  {
    const float a = o.angle * 0.01745f;
    drawGlow(r, mArt, cx + std::cos(a) * 44.0f, cy + std::sin(a) * 44.0f, 26.0f, rgb(255, 230, 150), 0.8f);
  }
}

bool World::cityDrawsBlock(int bx, int by) const
{
  if (mTop.on)
    return false;
  const auto& z = mCity.lizard;
  if (z.on && z.doorShut && bx == z.doorX && by >= z.doorY0 && by <= z.doorY1)
    return true; // the arena's gate
  for (const auto& b : mCity.buildings)
  {
    if (b.state == BuildingState::Lying)
    {
      if (bx >= b.bx0 && bx <= b.bx1 && by >= b.by0 && by <= b.by1)
        return true;
    }
    else if ((b.state == BuildingState::Standing || b.state == BuildingState::Leaning || b.state == BuildingState::Falling) &&
      b.covers(bx, by))
      return true;
  }
  return false;
}

} // namespace gr
