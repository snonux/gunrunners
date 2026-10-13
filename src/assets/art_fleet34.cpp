// Level 34, Zeppelin Relay (theme key sunset_airships, look "fleet"): a
// fleet of airships at sunset over a sea of clouds. The masses are hull:
// varnished mahogany strakes, each plank lit along its top edge and shaded
// along its bottom, a butt joint on every other strake, on the plain
// blocks a narrow brass rivet strip across the middle; now and then a
// brass porthole with the sunset in its glass, or a louvred brass vent.
// Where you stand on hull, a brass-capped gunwale. The one-way floors are
// honey teak deck planks with a brass rub rail and its rivets along the
// edge, a shadow under it. Ladders are rope ladders with
// wooden rungs. Behind: lavender overhead going to rose, peach and gold at
// the horizon, the sun low and huge, stratus bands lit pink and gold, the
// first stars; far off, a level of cloud tops with the rest of the fleet
// as small dusky silhouettes, their lamps lit; near, the cumulus banks of
// the cloud sea, their tops rimmed with gold. Everything at the fleet is
// drawn live (world_fleet_draw.cpp).

#include "assets/art_fleet34.hpp"

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
constexpr Color kWood = rgb(122, 62, 38);
constexpr Color kWoodHi = rgb(176, 104, 62);
constexpr Color kWoodLo = rgb(70, 34, 22);
constexpr Color kTeak = rgb(206, 150, 92);
constexpr Color kTeakLo = rgb(150, 100, 58);
constexpr Color kBrass = rgb(220, 178, 88);
constexpr Color kBrassHi = rgb(255, 236, 160);
constexpr Color kBrassLo = rgb(130, 90, 36);
constexpr Color kInk = rgb(34, 18, 22);
constexpr Color kSun = rgb(255, 214, 150);

Color lighten(Color c, float t) { return lerpColor(c, rgb(255, 255, 255), t); }
Color darken(Color c, float t) { return lerpColor(c, rgb(0, 0, 0), t); }

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

void stroke(cairo_t* cr, Color c, double w, double a = 1.0)
{
  setRgba(cr, c, a);
  cairo_set_line_width(cr, w);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
  cairo_stroke(cr);
}

void softDot(cairo_t* cr, double x, double y, double rx, double ry, Color c, double a)
{
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_scale(cr, std::max(0.1, rx), std::max(0.1, ry));
  cairo_pattern_t* g = cairo_pattern_create_radial(0, 0, 0, 0, 0, 1);
  stop(g, 0.0, c, a);
  stop(g, 0.6, c, a * 0.4);
  stop(g, 1.0, c, 0.0);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
  cairo_restore(cr);
}

void rivet(cairo_t* cr, double x, double y, double r)
{
  cairo_arc(cr, x, y, r, 0, 2 * kPi);
  setRgba(cr, kBrassLo, 1.0);
  cairo_fill(cr);
  cairo_arc(cr, x - r * 0.25, y - r * 0.25, r * 0.6, 0, 2 * kPi);
  setRgba(cr, kBrassHi, 0.9);
  cairo_fill(cr);
}

// The hull under every variant: four mahogany strakes, 16 px each, lit
// along the top, shaded along the bottom, a grain, a butt joint on every
// other strake (staggered).
void hullBase(cairo_t* cr)
{
  constexpr std::array<double, 4> kButt{0.0, 44.0, 0.0, 14.0};
  for (int i = 0; i < 4; ++i)
  {
    const double y = i * 16.0;
    const Color c = i % 2 ? darken(kWood, 0.06f) : kWood;
    cairo_rectangle(cr, 0, y, 64, 16);
    fillLinear(cr, 0, y, 0, y + 16, {{0.0, lighten(c, 0.16f)}, {0.25, c}, {0.85, darken(c, 0.15f)}, {1.0, kWoodLo}});
    // The grain, long and wavy.
    for (int g = 0; g < 3; ++g)
    {
      const double gy = y + 4.0 + g * 4.0;
      cairo_move_to(cr, 0, gy);
      for (double x = 8; x <= 64; x += 8)
        cairo_line_to(cr, x, gy + 0.7 * std::sin(x * 0.19 + i * 1.7 + g));
      stroke(cr, kWoodLo, 0.6, 0.35);
    }
    // The seam under the plank.
    cairo_move_to(cr, 0, y + 15.6);
    cairo_line_to(cr, 64, y + 15.6);
    stroke(cr, kInk, 1.0, 0.85);
    // A butt joint on every other strake only, so the planks run long.
    if (i % 2 == 0)
      continue;
    const double bx = kButt[std::size_t(i)];
    cairo_move_to(cr, bx, y + 0.5);
    cairo_line_to(cr, bx, y + 15.5);
    stroke(cr, kInk, 1.0, 0.55);
    cairo_move_to(cr, bx + 1.2, y + 1.0);
    cairo_line_to(cr, bx + 1.2, y + 15.0);
    stroke(cr, kWoodHi, 0.6, 0.35);
    // Treenails at the joint.
    for (const double ny : {y + 5.0, y + 11.0})
    {
      cairo_arc(cr, bx + 4.0, ny, 0.9, 0, 2 * kPi);
      setRgba(cr, kWoodLo, 0.9);
      cairo_fill(cr);
    }
  }
}

// A narrow brass strip with a rivet every 16 px.
void rivetStrip(cairo_t* cr, double y)
{
  cairo_rectangle(cr, 0, y - 2, 64, 4);
  fillLinear(cr, 0, y - 2, 0, y + 2, {{0.0, kBrassHi}, {0.4, kBrass}, {1.0, kBrassLo}});
  cairo_move_to(cr, 0, y + 2.2);
  cairo_line_to(cr, 64, y + 2.2);
  stroke(cr, kInk, 1.0, 0.5);
  for (double x = 8; x < 64; x += 16)
    rivet(cr, x, y, 1.2);
}

// Variant 1: a brass porthole, the sunset glowing in its glass.
void porthole(cairo_t* cr)
{
  const double cx = 32, cy = 30;
  cairo_arc(cr, cx + 1.5, cy + 2, 15, 0, 2 * kPi);
  setRgba(cr, kInk, 0.45);
  cairo_fill(cr);
  cairo_arc(cr, cx, cy, 15, 0, 2 * kPi);
  fillLinear(cr, cx - 15, cy - 15, cx + 15, cy + 15, {{0.0, kBrassHi}, {0.45, kBrass}, {1.0, kBrassLo}});
  cairo_arc(cr, cx, cy, 10.5, 0, 2 * kPi);
  fillLinear(cr, 0, cy - 10, 0, cy + 10,
    {{0.0, rgb(120, 104, 180)}, {0.5, rgb(240, 150, 160)}, {0.8, rgb(255, 200, 140)}, {1.0, rgb(255, 230, 170)}});
  // The glint.
  cairo_move_to(cr, cx - 6, cy - 3);
  cairo_curve_to(cr, cx - 5, cy - 7, cx - 2, cy - 8, cx + 1, cy - 8);
  stroke(cr, rgb(255, 255, 255), 1.8, 0.75);
  cairo_arc(cr, cx, cy, 10.5, 0, 2 * kPi);
  stroke(cr, kInk, 1.4, 0.9);
  for (int k = 0; k < 8; ++k)
  {
    const double a = k * kPi / 4.0 + kPi / 8.0;
    rivet(cr, cx + std::cos(a) * 12.8, cy + std::sin(a) * 12.8, 1.1);
  }
  cairo_arc(cr, cx, cy, 15, 0, 2 * kPi);
  stroke(cr, kInk, 1.2, 0.9);
}

// Variant 2: a louvred brass vent plate held by four rivets.
void ventPlate(cairo_t* cr)
{
  cairo_rectangle(cr, 15, 15, 34, 26);
  setRgba(cr, kInk, 0.4);
  cairo_fill(cr);
  cairo_rectangle(cr, 13, 13, 34, 26);
  fillLinear(cr, 13, 13, 47, 39, {{0.0, kBrassHi}, {0.5, kBrass}, {1.0, kBrassLo}}, 1.0, true);
  stroke(cr, kInk, 1.2);
  for (int k = 0; k < 4; ++k)
  {
    const double y = 18 + k * 5.0;
    cairo_rectangle(cr, 18, y, 24, 3);
    setRgba(cr, rgb(50, 30, 20), 1.0);
    cairo_fill(cr);
    cairo_move_to(cr, 18, y + 3.4);
    cairo_line_to(cr, 42, y + 3.4);
    stroke(cr, kBrassHi, 0.8, 0.8);
  }
  for (const auto& p : {std::array<double, 2>{16, 16}, {44, 16}, {16, 36}, {44, 36}})
    rivet(cr, p[0], p[1], 1.4);
}

} // namespace

bool isZeppelinFleet(const Theme& t) { return std::string_view(t.look) == "fleet"; }

Texture bakeFleetSolid(const Renderer& r, const Theme& t, int variant)
{
  (void)t;
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  hullBase(cr);
  if (variant == 1)
    porthole(cr);
  else if (variant == 2)
    ventPlate(cr);
  else
    rivetStrip(cr, 32);
  return img.toTexture(r, 0.0f, 0.0f);
}

cairo_pattern_t* fleetHullPattern()
{
  cairo_surface_t* tile = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 64, 64);
  cairo_t* cr = cairo_create(tile);
  hullBase(cr);
  cairo_destroy(cr);
  cairo_pattern_t* p = cairo_pattern_create_for_surface(tile);
  cairo_pattern_set_extend(p, CAIRO_EXTEND_REPEAT);
  cairo_surface_destroy(tile);
  return p;
}

// Where you stand on hull: a brass-capped gunwale, its top polished bright
// with the sunset on it, rivets along it, a shadow under it.
Texture bakeFleetSolidTop(const Renderer& r, const Theme& t, float topOffset, int height)
{
  (void)t;
  VectorImage img(64, height);
  cairo_t* cr = img.cr();
  const double y = topOffset;
  {
    cairo_pattern_t* g = cairo_pattern_create_linear(0, y + 7, 0, y + 18);
    stop(g, 0.0, kInk, 0.45);
    stop(g, 1.0, kInk, 0.0);
    cairo_rectangle(cr, 0, y + 7, 64, 11);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  cairo_rectangle(cr, 0, y - 2, 64, 9);
  fillLinear(cr, 0, y - 2, 0, y + 7, {{0.0, kBrassHi}, {0.3, rgb(255, 200, 130)}, {0.6, kBrass}, {1.0, kBrassLo}});
  cairo_move_to(cr, 0, y + 7);
  cairo_line_to(cr, 64, y + 7);
  stroke(cr, kInk, 1.2, 0.8);
  cairo_move_to(cr, 0, y - 1.5);
  cairo_line_to(cr, 64, y - 1.5);
  stroke(cr, rgb(255, 250, 220), 1.0, 0.8);
  for (double x = 8; x < 64; x += 16)
    rivet(cr, x, y + 3.5, 1.3);
  return img.toTexture(r, 0.0f, topOffset);
}

// One-way floors: honey teak deck planks seen a little from above (three
// strakes, butt joints and caulking), a brass rub rail with rivets along
// the edge, the dark of the hull's top strake and a shadow under it. 64 x
// 40, the walking surface at the top; it tiles side by side.
Texture bakeFleetPlatform(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(64, 40);
  cairo_t* cr = img.cr();
  // The shadow it throws.
  {
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 22, 0, 34);
    stop(g, 0.0, kInk, 0.5);
    stop(g, 1.0, kInk, 0.0);
    cairo_rectangle(cr, 0, 22, 64, 12);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  // The planks.
  constexpr std::array<double, 3> kButt{20.0, 50.0, 6.0};
  for (int i = 0; i < 3; ++i)
  {
    const double y = i * 3.6;
    const Color c = i == 1 ? darken(kTeak, 0.05f) : kTeak;
    cairo_rectangle(cr, 0, y, 64, 3.6);
    fillLinear(cr, 0, y, 0, y + 3.6, {{0.0, lighten(c, 0.25f)}, {1.0, darken(c, 0.08f)}});
    cairo_move_to(cr, 0, y + 3.5);
    cairo_line_to(cr, 64, y + 3.5);
    stroke(cr, rgb(70, 40, 24), 0.7, 0.8);
    cairo_move_to(cr, kButt[std::size_t(i)], y + 0.3);
    cairo_line_to(cr, kButt[std::size_t(i)], y + 3.3);
    stroke(cr, rgb(70, 40, 24), 0.7, 0.7);
  }
  // The sunset's sheen on the top edge.
  cairo_move_to(cr, 0, 0.6);
  cairo_line_to(cr, 64, 0.6);
  stroke(cr, rgb(255, 236, 200), 1.0, 0.9);
  // The rub rail.
  cairo_rectangle(cr, 0, 11, 64, 6);
  fillLinear(cr, 0, 11, 0, 17, {{0.0, kBrassHi}, {0.35, kBrass}, {1.0, kBrassLo}});
  cairo_move_to(cr, 0, 11);
  cairo_line_to(cr, 64, 11);
  stroke(cr, kInk, 1.0, 0.8);
  for (double x = 8; x < 64; x += 16)
    rivet(cr, x, 14, 1.2);
  // The hull's top strake under it.
  cairo_rectangle(cr, 0, 17, 64, 6);
  fillLinear(cr, 0, 17, 0, 23, {{0.0, kWood}, {1.0, kWoodLo}});
  cairo_move_to(cr, 0, 23);
  cairo_line_to(cr, 64, 23);
  stroke(cr, kInk, 1.0, 0.7);
  return img.toTexture(r, 0.0f, 0.0f);
}

// A rope ladder: two hemp side ropes, wooden rungs lashed to them.
Texture bakeFleetLadder(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  for (const double y : {14.0, 46.0})
  {
    cairo_rectangle(cr, 10, y - 3, 44, 6);
    fillLinear(cr, 0, y - 3, 0, y + 3, {{0.0, rgb(214, 160, 100)}, {0.5, rgb(160, 104, 60)}, {1.0, rgb(90, 54, 30)}});
    cairo_rectangle(cr, 10, y - 3, 44, 6);
    stroke(cr, kInk, 1.0, 0.85);
  }
  for (const double x : {12.0, 52.0})
  {
    for (int pass = 0; pass < 2; ++pass)
    {
      cairo_move_to(cr, x, -2);
      cairo_curve_to(cr, x + 1.2, 20, x - 1.2, 44, x, 66);
      stroke(cr, pass == 0 ? kInk : rgb(214, 190, 140), pass == 0 ? 6.0 : 3.6);
    }
    // The twist of the hemp.
    for (double y = 0; y < 64; y += 5)
    {
      cairo_move_to(cr, x - 1.6, y);
      cairo_line_to(cr, x + 1.6, y + 3);
      stroke(cr, rgb(130, 100, 60), 0.8, 0.8);
    }
    // The lashings round the rungs.
    for (const double y : {14.0, 46.0})
    {
      cairo_rectangle(cr, x - 3.5, y - 4, 7, 8);
      setRgba(cr, rgb(196, 170, 120), 1.0);
      cairo_fill_preserve(cr);
      stroke(cr, kInk, 0.8, 0.8);
      for (double k = -2; k <= 2; k += 2)
      {
        cairo_move_to(cr, x - 3.5, y + k);
        cairo_line_to(cr, x + 3.5, y + k + 1);
        stroke(cr, rgb(120, 90, 50), 0.7, 0.8);
      }
    }
  }
  return img.toTexture(r, 16.0f, 0.0f);
}

namespace
{

// A long band of stratus: a flattened soft ellipse, lit on top, dusky
// lavender under it.
void stratus(cairo_t* cr, double x, double y, double w, double h, Color lit, Color shade, double a)
{
  softDot(cr, x, y + h * 0.35, w, h, shade, a * 0.7);
  softDot(cr, x, y, w * 0.92, h * 0.8, lit, a);
  softDot(cr, x - w * 0.2, y - h * 0.25, w * 0.5, h * 0.35, lighten(lit, 0.35f), a * 0.8);
}

} // namespace

// The sky: lavender overhead going to rose, peach and gold at the horizon,
// the sun low and huge over the right, stratus bands lit pink and gold, the
// first stars up top.
Texture bakeFleetSky(const Renderer& r, const Theme& t)
{
  VectorImage img(kScreenW, kScreenH);
  cairo_t* cr = img.cr();
  cairo_rectangle(cr, 0, 0, kScreenW, kScreenH);
  fillLinear(cr, 0, 0, 0, kScreenH,
    {{0.0, darken(t.skyTop, 0.3f)}, {0.25, t.skyTop}, {0.55, t.skyMid}, {0.8, t.skyBottom}, {1.0, rgb(255, 226, 170)}});
  Rng rng(34001u);
  // Stars.
  for (int i = 0; i < 60; ++i)
  {
    const double x = rng.range(0.0f, float(kScreenW)), y = rng.range(0.0f, 170.0f);
    cairo_arc(cr, x, y, i % 9 ? 0.9 : 1.5, 0, 2 * kPi);
    setRgba(cr, rgb(255, 250, 240), (1.0 - y / 170.0) * rng.range(0.3f, 0.8f));
    cairo_fill(cr);
  }
  // The sun's glow, then the sun.
  const double sx = 930, sy = 560;
  softDot(cr, sx, sy, 760, 420, rgb(255, 170, 110), 0.35);
  softDot(cr, sx, sy, 300, 220, rgb(255, 210, 150), 0.55);
  cairo_arc(cr, sx, sy, 70, 0, 2 * kPi);
  fillLinear(cr, 0, sy - 70, 0, sy + 70, {{0.0, rgb(255, 250, 220)}, {0.6, kSun}, {1.0, rgb(255, 160, 100)}});
  // Stratus bands across it, lit pink and gold.
  for (int i = 0; i < 12; ++i)
  {
    const double y = rng.range(150.0f, 560.0f);
    const double k = (y - 150.0) / 410.0;
    const Color lit = lerpColor(rgb(250, 180, 200), rgb(255, 214, 150), float(k));
    const Color shade = lerpColor(rgb(120, 100, 170), rgb(200, 130, 150), float(k));
    stratus(cr, rng.range(-100.0f, float(kScreenW) + 100.0f), y, rng.range(160.0f, 420.0f), rng.range(8.0f, 20.0f), lit, shade,
      rng.range(0.35f, 0.65f));
  }
  // Two thin bars across the sun.
  for (const double y : {528.0, 576.0})
  {
    softDot(cr, sx + 20, y, 260, 6, rgb(200, 110, 130), 0.55);
    softDot(cr, sx + 10, y - 3, 220, 3, rgb(255, 230, 190), 0.5);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

namespace
{

// A cumulus heap: overlapping puffs, shaded lavender below, lit warm on
// top, a gold rim where the low sun catches it. `wrapW` > 0 draws it again
// a layer's width left and right so the layer tiles.
void cumulus(cairo_t* cr, double x, double base, double w, double h, Rng& rng, Color lit, Color shade, Color rim, double wrapW)
{
  const int n = 6 + int(w / 60.0);
  struct Puff
  {
    double x, y, r;
  };
  std::array<Puff, 40> puffs{};
  const int count = std::min(n, int(puffs.size()));
  for (int i = 0; i < count; ++i)
  {
    const double u = (double(i) + 0.5) / double(count);
    const double hump = std::sin(u * kPi);
    const double rad = h * (0.35 + 0.45 * hump) * rng.range(0.8f, 1.1f);
    puffs[std::size_t(i)] = {x - w * 0.5 + u * w + rng.range(-10.0f, 10.0f), base - rad * 0.7 - hump * h * 0.25, rad};
  }
  for (const double off : {0.0, -wrapW, wrapW})
  {
    if (off != 0.0 && wrapW <= 0.0)
      continue;
    // The shaded body.
    for (int i = 0; i < count; ++i)
    {
      const auto& p = puffs[std::size_t(i)];
      cairo_arc(cr, p.x + off, p.y + p.r * 0.25, p.r, 0, 2 * kPi);
      setRgba(cr, shade, 1.0);
      cairo_fill(cr);
    }
    cairo_rectangle(cr, x - w * 0.5 + off, base - h * 0.3, w, h * 0.3 + 400);
    setRgba(cr, shade, 1.0);
    cairo_fill(cr);
    // The lit tops, the rim.
    for (int i = 0; i < count; ++i)
    {
      const auto& p = puffs[std::size_t(i)];
      cairo_save(cr);
      cairo_arc(cr, p.x + off, p.y, p.r * 0.92, 0, 2 * kPi);
      cairo_clip(cr);
      cairo_arc(cr, p.x + off, p.y + p.r * 0.25, p.r, 0, 2 * kPi);
      fillLinear(cr, 0, p.y - p.r, 0, p.y + p.r * 0.6, {{0.0, lighten(lit, 0.3f)}, {0.5, lit}, {1.0, shade}});
      cairo_restore(cr);
      cairo_arc(cr, p.x + off, p.y, p.r * 0.92, kPi * 1.1, kPi * 1.75);
      stroke(cr, rim, 2.0, 0.6);
    }
  }
}

// A small airship far off: a dusky silhouette of an envelope and gondola,
// its lamps lit. `s` is its length.
void farShip(cairo_t* cr, double x, double y, double s, Color c, double wrapW)
{
  for (const double off : {0.0, -wrapW, wrapW})
  {
    const double cx = x + off;
    cairo_save(cr);
    cairo_translate(cr, cx, y);
    cairo_scale(cr, s * 0.5, s * 0.16);
    cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
    cairo_restore(cr);
    setRgba(cr, c, 0.9);
    cairo_fill(cr);
    // Fins.
    cairo_move_to(cr, cx - s * 0.42, y - s * 0.04);
    cairo_line_to(cr, cx - s * 0.56, y - s * 0.17);
    cairo_line_to(cr, cx - s * 0.5, y);
    cairo_line_to(cr, cx - s * 0.56, y + s * 0.15);
    cairo_line_to(cr, cx - s * 0.42, y + s * 0.05);
    cairo_close_path(cr);
    cairo_fill(cr);
    // The gondola, hung on lines.
    cairo_rectangle(cr, cx - s * 0.12, y + s * 0.2, s * 0.24, s * 0.07);
    cairo_fill(cr);
    cairo_move_to(cr, cx - s * 0.1, y + s * 0.13);
    cairo_line_to(cr, cx - s * 0.1, y + s * 0.2);
    cairo_move_to(cr, cx + s * 0.1, y + s * 0.13);
    cairo_line_to(cr, cx + s * 0.1, y + s * 0.2);
    stroke(cr, c, 1.0, 0.9);
    // The sunlit top edge, the lamps.
    cairo_save(cr);
    cairo_translate(cr, cx, y);
    cairo_scale(cr, s * 0.5, s * 0.16);
    cairo_arc(cr, 0, 0, 1, kPi * 1.15, kPi * 1.85);
    cairo_restore(cr);
    stroke(cr, rgb(255, 190, 150), 1.2, 0.7);
    for (const double lx : {-0.08, 0.0, 0.08})
    {
      cairo_arc(cr, cx + s * lx, y + s * 0.235, std::max(0.8, s * 0.012), 0, 2 * kPi);
      setRgba(cr, rgb(255, 230, 150), 0.95);
      cairo_fill(cr);
    }
  }
}

} // namespace

// Far: towering cumulus lit orange by the sun at the back, a level of
// cloud tops in rose and lavender across the lower screen, and the rest of
// the fleet as small dusky silhouettes, their lamps lit.
Texture bakeFleetFar(const Renderer& r, const Theme& t, int layerW)
{
  (void)t;
  VectorImage img(layerW, kScreenH + 200);
  cairo_t* cr = img.cr();
  const double W = layerW;
  Rng rng(34051u);
  // Towers far back.
  for (double x = rng.range(100.0f, 400.0f); x < W - 200; x += rng.range(700.0f, 1100.0f))
    cumulus(cr, x, 520, rng.range(260.0f, 420.0f), rng.range(200.0f, 280.0f), rng, rgb(255, 196, 160), rgb(186, 140, 180),
      rgb(255, 230, 170), W);
  // Silhouettes of the fleet, smallest farthest.
  for (int i = 0; i < 9; ++i)
  {
    const double s = rng.range(40.0f, 120.0f);
    farShip(cr, rng.range(0.0f, float(W)), rng.range(150.0f, 400.0f) + (120.0 - s) * 0.6, s,
      lerpColor(rgb(120, 96, 150), rgb(90, 70, 110), float(s / 120.0)), W);
  }
  // The level of cloud tops.
  for (double x = 0; x < W; x += rng.range(160.0f, 260.0f))
    cumulus(cr, x, 600, rng.range(240.0f, 420.0f), rng.range(60.0f, 110.0f), rng, rgb(250, 190, 186), rgb(170, 140, 190),
      rgb(255, 220, 180), W);
  return img.toTexture(r, 0.0f, 0.0f);
}

// Near: the cumulus banks of the cloud sea along the bottom, their tops
// lit warm and rimmed with gold, lavender in their shadows.
Texture bakeFleetNear(const Renderer& r, const Theme& t, int layerW)
{
  (void)t;
  VectorImage img(layerW, kScreenH + 300);
  cairo_t* cr = img.cr();
  const double W = layerW;
  Rng rng(34061u);
  for (double x = 0; x < W; x += rng.range(220.0f, 360.0f))
    cumulus(cr, x, 700 + rng.range(-30.0f, 20.0f), rng.range(320.0f, 520.0f), rng.range(110.0f, 170.0f), rng, rgb(255, 214, 200),
      rgb(196, 160, 204), rgb(255, 236, 190), W);
  for (double x = 120; x < W; x += rng.range(260.0f, 420.0f))
    cumulus(cr, x, 790, rng.range(360.0f, 560.0f), rng.range(110.0f, 150.0f), rng, rgb(255, 228, 220), rgb(214, 180, 214),
      rgb(255, 244, 210), W);
  return img.toTexture(r, 0.0f, 0.0f);
}

Texture bakeFleetExitBase(const Renderer& r, const Theme& t)
{
  (void)t;
  // 96 wide, 144 tall; (0, 0) is the top-left of the 64 x 128 exit area.
  VectorImage img(96, 144);
  cairo_t* cr = img.cr();
  cairo_translate(cr, 16, 8);
  // The relay aerial over the roof: a brass mast, its cross arms, a lamp.
  cairo_move_to(cr, 32, 48);
  cairo_line_to(cr, 32, -6);
  stroke(cr, kInk, 4.0);
  cairo_move_to(cr, 32, 48);
  cairo_line_to(cr, 32, -6);
  stroke(cr, kBrass, 2.2);
  for (const double y : {4.0, 16.0, 28.0})
  {
    const double half = 6.0 + (y - 4.0) * 0.5;
    cairo_move_to(cr, 32 - half, y);
    cairo_line_to(cr, 32 + half, y);
    stroke(cr, kInk, 3.0);
    cairo_move_to(cr, 32 - half, y);
    cairo_line_to(cr, 32 + half, y);
    stroke(cr, kBrassHi, 1.4);
  }
  // The guy wires.
  for (const double side : {-1.0, 1.0})
  {
    cairo_move_to(cr, 32, 0);
    cairo_line_to(cr, 32 + side * 34, 52);
    stroke(cr, rgb(60, 40, 40), 0.9, 0.8);
  }
  softDot(cr, 32, -6, 12, 12, rgb(255, 120, 90), 0.7);
  cairo_arc(cr, 32, -6, 2.6, 0, 2 * kPi);
  setRgba(cr, rgb(255, 240, 220), 1.0);
  cairo_fill(cr);
  // The shack: a mahogany box with a curved brass roof.
  cairo_rectangle(cr, -6, 52, 76, 76);
  fillLinear(cr, -6, 0, 70, 0, {{0.0, kWoodLo}, {0.2, kWood}, {0.8, kWoodHi}, {1.0, kWood}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  for (double y = 64; y < 128; y += 12)
  {
    cairo_move_to(cr, -6, y);
    cairo_line_to(cr, 70, y);
    stroke(cr, kWoodLo, 1.0, 0.7);
  }
  cairo_move_to(cr, -12, 56);
  cairo_curve_to(cr, 4, 38, 60, 38, 76, 56);
  cairo_line_to(cr, 76, 60);
  cairo_line_to(cr, -12, 60);
  cairo_close_path(cr);
  fillLinear(cr, 0, 40, 0, 60, {{0.0, kBrassHi}, {0.5, kBrass}, {1.0, kBrassLo}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  // The open door, warm light inside, a sign over it.
  cairo_rectangle(cr, 12, 70, 40, 58);
  fillLinear(cr, 0, 70, 0, 128, {{0.0, rgb(255, 236, 180)}, {1.0, rgb(255, 170, 90)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  softDot(cr, 32, 98, 34, 30, rgb(255, 230, 160), 0.5);
  cairo_rectangle(cr, 12, 70, 40, 58);
  stroke(cr, kBrass, 1.2);
  cairo_rectangle(cr, 14, 61, 36, 7);
  setRgba(cr, rgb(40, 24, 24), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kBrass, 1.0);
  for (int k = 0; k < 5; ++k)
  {
    cairo_rectangle(cr, 17 + k * 6.5, 63, 4, 3);
    setRgba(cr, rgb(255, 210, 120), 0.9);
    cairo_fill(cr);
  }
  // A porthole each side.
  for (const double x : {2.0, 62.0})
  {
    cairo_arc(cr, x, 88, 5, 0, 2 * kPi);
    setRgba(cr, rgb(255, 214, 150), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kBrass, 2.0);
  }
  return img.toTexture(r, 16.0f, 8.0f);
}

} // namespace gr
