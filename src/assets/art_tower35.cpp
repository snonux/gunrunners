// Level 35, The Transmitter (theme key broadcast_mast, look "tower"): a red
// and white lattice mast above a dawn cloud sea. The masses are red-painted
// steel plate in riveted panels, a lighter bevel on each panel's top and
// left, the paint chipped back to grey steel along the seams and a little
// rust weeping from the rivets; now and then a louvred vent, or a yellow RF
// hazard placard on an access hatch. Where you stand, a galvanized grating
// lip with a white-painted edge. The one-way ledges are grating walkways on
// white girders with a red and white toe board and a knee brace; the
// ladders galvanized steel with a hoop of safety cage. The sky is dawn,
// deep blue overhead, peach and gold low down. The cloud sea, the lattice,
// the dishes and lights and the spire are drawn live
// (world_tower_draw.cpp).

#include "assets/art_tower35.hpp"

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
constexpr Color kPaint = rgb(178, 52, 44);
constexpr Color kPaintHi = rgb(222, 92, 78);
constexpr Color kPaintLo = rgb(104, 28, 28);
constexpr Color kWhite = rgb(236, 234, 228);
constexpr Color kSteel = rgb(150, 156, 166);
constexpr Color kSteelDark = rgb(72, 76, 88);
constexpr Color kInk = rgb(22, 14, 18);

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

void rivet(cairo_t* cr, double x, double y)
{
  cairo_arc(cr, x + 0.4, y + 0.5, 1.6, 0, 2 * kPi);
  setRgba(cr, kInk, 0.5);
  cairo_fill(cr);
  cairo_arc(cr, x, y, 1.4, 0, 2 * kPi);
  setRgba(cr, kPaintHi, 1.0);
  cairo_fill(cr);
  cairo_arc(cr, x - 0.4, y - 0.4, 0.6, 0, 2 * kPi);
  setRgba(cr, rgb(255, 200, 190), 0.9);
  cairo_fill(cr);
}

// Two riveted plates per block (it tiles in both directions): each lit on
// its top and left, shadowed on its bottom and right, a seam between them
// where the paint has chipped back to the steel.
void plates(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 0, 64, 64);
  setRgba(cr, kPaintLo, 1.0);
  cairo_fill(cr);
  for (const double y0 : {0.0, 32.0})
  {
    cairo_rectangle(cr, 1, y0 + 1, 62, 30);
    fillLinear(cr, 0, y0, 0, y0 + 32, {{0.0, kPaintHi}, {0.12, kPaint}, {0.8, darken(kPaint, 0.12f)}, {1.0, kPaintLo}});
    // Bevels.
    cairo_move_to(cr, 1.5, y0 + 30.5);
    cairo_line_to(cr, 1.5, y0 + 1.5);
    cairo_line_to(cr, 62.5, y0 + 1.5);
    stroke(cr, rgb(255, 150, 130), 1.0, 0.55);
    cairo_move_to(cr, 62.5, y0 + 1.5);
    cairo_line_to(cr, 62.5, y0 + 30.5);
    cairo_line_to(cr, 1.5, y0 + 30.5);
    stroke(cr, kInk, 1.2, 0.5);
    for (double x = 5; x < 64; x += 9)
    {
      rivet(cr, x, y0 + 4.5);
      rivet(cr, x, y0 + 27.5);
    }
  }
  // Chipped paint along the seams: grey steel showing through.
  Rng rng(35011u);
  for (int i = 0; i < 8; ++i)
  {
    const double x = rng.range(2, 60), y = i % 2 ? 31.0 + rng.range(-2, 2) : rng.range(0, 3);
    cairo_move_to(cr, x, y);
    cairo_curve_to(cr, x + 2, y - 1.5, x + 5, y + 1.5, x + rng.range(4, 9), y + rng.range(-1, 1));
    stroke(cr, kSteel, rng.range(1.0f, 2.2f), 0.75);
  }
  // Rust weeping from a rivet or two.
  for (int i = 0; i < 2; ++i)
  {
    const double x = 5 + 9 * double(rng.next() % 7u), y = i == 0 ? 4.5 : 36.5;
    cairo_move_to(cr, x, y + 2);
    cairo_curve_to(cr, x + 0.6, y + 8, x - 0.6, y + 12, x + 0.3, y + 18);
    stroke(cr, rgb(120, 60, 30), 1.4, 0.35);
  }
  // A fine grain in the paint.
  for (int i = 0; i < 60; ++i)
  {
    cairo_rectangle(cr, rng.range(0, 64), rng.range(0, 64), 1, 1);
    setRgba(cr, i % 2 ? rgb(255, 170, 150) : rgb(60, 14, 14), rng.range(0.1f, 0.25f));
    cairo_fill(cr);
  }
}

// Variant 1: a louvred vent in the lower plate.
void vent(cairo_t* cr)
{
  cairo_rectangle(cr, 14, 38, 36, 20);
  setRgba(cr, rgb(30, 18, 20), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.4);
  for (int k = 0; k < 5; ++k)
  {
    const double y = 40.5 + k * 3.7;
    cairo_rectangle(cr, 15, y, 34, 2.2);
    fillLinear(cr, 0, y, 0, y + 2.2, {{0.0, kPaintHi}, {1.0, kPaintLo}});
  }
  for (const double x : {16.5, 47.5})
    for (const double y : {39.5, 56.5})
      rivet(cr, x, y);
}

// Variant 2: an access hatch with a yellow RF hazard placard (the
// radiating-waves triangle).
void placard(cairo_t* cr)
{
  cairo_rectangle(cr, 10, 7, 44, 50);
  stroke(cr, kInk, 1.6, 0.7);
  cairo_rectangle(cr, 11, 8, 42, 48);
  stroke(cr, rgb(255, 150, 130), 0.8, 0.4);
  // The hinge and the handle.
  cairo_rectangle(cr, 8, 14, 4, 8);
  cairo_rectangle(cr, 8, 42, 4, 8);
  setRgba(cr, kSteel, 1.0);
  cairo_fill(cr);
  cairo_rectangle(cr, 46, 28, 4, 10);
  fillLinear(cr, 46, 0, 50, 0, {{0.0, rgb(220, 224, 232)}, {1.0, kSteelDark}});
  // The placard.
  cairo_move_to(cr, 30, 14);
  cairo_line_to(cr, 42, 34);
  cairo_line_to(cr, 18, 34);
  cairo_close_path(cr);
  setRgba(cr, rgb(250, 210, 40), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.6);
  cairo_arc(cr, 30, 29, 1.6, 0, 2 * kPi);
  setRgba(cr, kInk, 1.0);
  cairo_fill(cr);
  for (int k = 1; k <= 2; ++k)
  {
    cairo_arc(cr, 30, 29, 2.5 + k * 2.4, -kPi * 0.85, -kPi * 0.15);
    stroke(cr, kInk, 1.1);
  }
  cairo_rectangle(cr, 18, 37, 24, 6);
  setRgba(cr, rgb(250, 246, 236), 1.0);
  cairo_fill(cr);
  for (double x = 20; x < 40; x += 3.5)
  {
    cairo_rectangle(cr, x, 39, 2.2, 2);
    setRgba(cr, kInk, 0.75);
    cairo_fill(cr);
  }
}

} // namespace

bool isTransmitter(const Theme& t) { return std::string_view(t.look) == "tower"; }

Texture bakeTowerSolid(const Renderer& r, const Theme& t, int variant)
{
  (void)t;
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  plates(cr);
  if (variant == 1)
    vent(cr);
  else if (variant == 2)
    placard(cr);
  return img.toTexture(r, 0.0f, 0.0f);
}

// Where you stand on the plate: a galvanized grating lip, its white-painted
// front edge, the shadow it throws on the plate below.
Texture bakeTowerSolidTop(const Renderer& r, const Theme& t, float topOffset, int height)
{
  (void)t;
  VectorImage img(64, height);
  cairo_t* cr = img.cr();
  const double y = topOffset;
  {
    cairo_pattern_t* g = cairo_pattern_create_linear(0, y + 7, 0, y + 16);
    stop(g, 0.0, kInk, 0.45);
    stop(g, 1.0, kInk, 0.0);
    cairo_rectangle(cr, 0, y + 7, 64, 9);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  // The grating, seen a little from above.
  cairo_rectangle(cr, 0, y - 3, 64, 6);
  setRgba(cr, rgb(40, 42, 50), 1.0);
  cairo_fill(cr);
  cairo_save(cr);
  cairo_rectangle(cr, 0, y - 3, 64, 6);
  cairo_clip(cr);
  for (double x = -4; x < 68; x += 4)
  {
    cairo_move_to(cr, x, y - 3);
    cairo_line_to(cr, x + 3, y + 3);
  }
  stroke(cr, rgb(176, 182, 192), 1.1);
  cairo_restore(cr);
  cairo_rectangle(cr, 0, y - 4, 64, 1.6);
  setRgba(cr, rgb(235, 240, 246), 1.0);
  cairo_fill(cr);
  // The white edge.
  cairo_rectangle(cr, 0, y + 3, 64, 4);
  fillLinear(cr, 0, y + 3, 0, y + 7, {{0.0, rgb(255, 255, 250)}, {1.0, rgb(170, 168, 164)}});
  cairo_rectangle(cr, 0, y + 7, 64, 1);
  setRgba(cr, kInk, 0.8);
  cairo_fill(cr);
  return img.toTexture(r, 0.0f, topOffset);
}

// One-way ledges: a grating walkway on a white girder, a red and white toe
// board along its front, a knee brace under it back to the mast. 64 x 44,
// the walking surface at the top; it tiles side by side.
Texture bakeTowerPlatform(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(64, 44);
  cairo_t* cr = img.cr();
  // The knee brace.
  for (int pass = 0; pass < 2; ++pass)
  {
    cairo_move_to(cr, 6, 18);
    cairo_line_to(cr, 30, 40);
    cairo_move_to(cr, 58, 18);
    cairo_line_to(cr, 34, 40);
    stroke(cr, pass == 0 ? kInk : rgb(200, 200, 196), pass == 0 ? 5.5 : 3.0);
  }
  cairo_rectangle(cr, 28, 37, 8, 6);
  setRgba(cr, kSteelDark, 1.0);
  cairo_fill(cr);
  // The grating.
  cairo_rectangle(cr, 0, 2, 64, 6);
  setRgba(cr, rgb(30, 32, 40), 1.0);
  cairo_fill(cr);
  cairo_save(cr);
  cairo_rectangle(cr, 0, 2, 64, 6);
  cairo_clip(cr);
  for (double x = -8; x < 72; x += 5)
  {
    cairo_move_to(cr, x, 2);
    cairo_line_to(cr, x + 6, 8);
    cairo_move_to(cr, x + 6, 2);
    cairo_line_to(cr, x, 8);
  }
  stroke(cr, rgb(170, 176, 188), 1.0);
  cairo_restore(cr);
  cairo_rectangle(cr, 0, 0, 64, 2.5);
  fillLinear(cr, 0, 0, 0, 2.5, {{0.0, rgb(250, 252, 255)}, {1.0, rgb(150, 156, 168)}});
  // The white girder.
  cairo_rectangle(cr, 0, 8, 64, 6);
  fillLinear(cr, 0, 8, 0, 14, {{0.0, rgb(255, 255, 252)}, {0.5, kWhite}, {1.0, rgb(150, 148, 146)}});
  // The toe board's red and white blocks.
  for (int k = 0; k < 4; ++k)
  {
    cairo_rectangle(cr, k * 16, 14, 16, 6);
    if (k % 2 == 0)
      fillLinear(cr, 0, 14, 0, 20, {{0.0, kPaintHi}, {1.0, kPaintLo}});
    else
      fillLinear(cr, 0, 14, 0, 20, {{0.0, rgb(255, 255, 250)}, {1.0, rgb(170, 168, 164)}});
  }
  cairo_rectangle(cr, 0, 8, 64, 12);
  stroke(cr, kInk, 1.2, 0.8);
  for (double x = 4; x < 64; x += 8)
  {
    cairo_arc(cr, x, 11, 0.9, 0, 2 * kPi);
    setRgba(cr, rgb(120, 118, 116), 1.0);
    cairo_fill(cr);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// A galvanized ladder: two rails, rungs with a tread, a hoop of safety cage
// round it once a block.
Texture bakeTowerLadder(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  // The cage's hoop (behind the climber, so faint) and its back straps.
  cairo_save(cr);
  cairo_translate(cr, 32, 6);
  cairo_scale(cr, 30, 7);
  cairo_arc(cr, 0, 0, 1, 0, kPi);
  cairo_restore(cr);
  stroke(cr, rgb(200, 206, 214), 2.2, 0.55);
  for (const double x : {6.0, 32.0, 58.0})
  {
    cairo_move_to(cr, x, -2);
    cairo_line_to(cr, x, 66);
    stroke(cr, rgb(160, 166, 176), 1.6, x == 32.0 ? 0.25 : 0.4);
  }
  for (const double y : {14.0, 46.0})
  {
    cairo_rectangle(cr, 13, y - 2.5, 38, 5);
    fillLinear(cr, 0, y - 2.5, 0, y + 2.5, {{0.0, rgb(220, 226, 234)}, {0.5, rgb(140, 146, 158)}, {1.0, rgb(60, 64, 76)}});
    cairo_rectangle(cr, 13, y - 2.5, 38, 5);
    stroke(cr, kInk, 1.0, 0.9);
    for (double x = 16; x < 50; x += 4)
    {
      cairo_move_to(cr, x, y - 2);
      cairo_line_to(cr, x + 2, y + 2);
      stroke(cr, rgb(50, 54, 64), 0.8, 0.6);
    }
  }
  for (const double x : {10.0, 48.0})
  {
    cairo_rectangle(cr, x, -2, 6, 68);
    fillLinear(cr, x, 0, x + 6, 0, {{0.0, rgb(250, 252, 255)}, {0.45, rgb(196, 202, 212)}, {1.0, rgb(96, 100, 112)}});
    cairo_rectangle(cr, x, -2, 6, 68);
    stroke(cr, kInk, 1.2, 0.9);
    // A red band at the top of each block.
    cairo_rectangle(cr, x + 0.6, 0, 4.8, 6);
    setRgba(cr, kPaint, 0.95);
    cairo_fill(cr);
  }
  return img.toTexture(r, 16.0f, 0.0f);
}

// Dawn: deep blue overhead, through lavender and peach to a gold haze low
// down, a few high streaks of cirrus catching the light.
Texture bakeTowerSky(const Renderer& r, const Theme& t)
{
  VectorImage img(kScreenW, kScreenH);
  cairo_t* cr = img.cr();
  cairo_rectangle(cr, 0, 0, kScreenW, kScreenH);
  fillLinear(cr, 0, 0, 0, kScreenH,
    {{0.0, lerpColor(t.skyTop, rgb(20, 30, 80), 0.35f)}, {0.35, t.skyTop}, {0.62, lerpColor(t.skyTop, t.skyMid, 0.6f)},
      {0.82, t.skyMid}, {1.0, t.skyBottom}});
  Rng rng(35041u);
  for (int i = 0; i < 9; ++i)
  {
    const double y = rng.range(40, 300), x = rng.range(-200, float(kScreenW)), w = rng.range(240, 520);
    softDot(cr, x + w * 0.5, y, w * 0.5, rng.range(4, 10), rgb(255, 200, 190), rng.range(0.12f, 0.3f));
  }
  // The last stars, high up.
  for (int i = 0; i < 40; ++i)
  {
    cairo_arc(cr, rng.range(0, float(kScreenW)), rng.range(0, 160), rng.range(0.6f, 1.2f), 0, 2 * kPi);
    setRgba(cr, rgb(230, 236, 255), rng.range(0.2f, 0.55f));
    cairo_fill(cr);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

Texture bakeTowerFar(const Renderer& r, const Theme& t, int layerW)
{
  (void)t;
  VectorImage img(layerW, 4);
  return img.toTexture(r, 0.0f, 0.0f);
}

Texture bakeTowerNear(const Renderer& r, const Theme& t, int layerW)
{
  (void)t;
  VectorImage img(layerW, 4);
  return img.toTexture(r, 0.0f, 0.0f);
}

Texture bakeTowerExitBase(const Renderer& r, const Theme& t)
{
  (void)t;
  // 96 wide, 144 tall; (0, 0) is the top-left of the 64 x 128 exit area.
  VectorImage img(96, 144);
  cairo_t* cr = img.cr();
  cairo_translate(cr, 16, 8);
  // The plinth: a red steel box, a white band, a status lamp.
  cairo_rectangle(cr, 4, 96, 56, 32);
  fillLinear(cr, 0, 96, 0, 128, {{0.0, kPaintHi}, {0.3, kPaint}, {1.0, kPaintLo}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  cairo_rectangle(cr, 4, 104, 56, 6);
  setRgba(cr, kWhite, 1.0);
  cairo_fill(cr);
  for (const double x : {9.0, 55.0})
    for (const double y : {100.0, 123.0})
      rivet(cr, x, y);
  softDot(cr, 50, 117, 6, 6, rgb(120, 220, 255), 0.8);
  cairo_arc(cr, 50, 117, 2, 0, 2 * kPi);
  setRgba(cr, rgb(230, 250, 255), 1.0);
  cairo_fill(cr);
  // The horn: a flared feed horn pointing up, in white with a red rim.
  cairo_move_to(cr, 22, 96);
  cairo_line_to(cr, 42, 96);
  cairo_line_to(cr, 40, 72);
  cairo_curve_to(cr, 48, 58, 58, 52, 60, 44);
  cairo_line_to(cr, 4, 44);
  cairo_curve_to(cr, 6, 52, 16, 58, 24, 72);
  cairo_close_path(cr);
  fillLinear(cr, 4, 0, 60, 0, {{0.0, rgb(160, 160, 160)}, {0.35, rgb(255, 255, 252)}, {0.7, kWhite}, {1.0, rgb(130, 130, 134)}}, 1.0,
    true);
  stroke(cr, kInk, 2.0);
  cairo_save(cr);
  cairo_translate(cr, 32, 44);
  cairo_scale(cr, 28, 6);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_restore(cr);
  setRgba(cr, rgb(40, 60, 90), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kPaint, 2.4);
  softDot(cr, 32, 44, 24, 6, rgb(150, 230, 255), 0.8);
  // Its waveguide down into the plinth.
  cairo_rectangle(cr, 28, 82, 8, 14);
  setRgba(cr, kSteel, 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.2);
  return img.toTexture(r, 16.0f, 8.0f);
}

} // namespace gr
