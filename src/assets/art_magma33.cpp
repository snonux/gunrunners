// Level 33, Magma Mainframe (theme key volcano_datacenter, look "magma"): a
// data centre inside a volcano. The masses are black basalt in columns (a
// prism face lit on its left, a dark joint on its right), cut across by
// fractures whose upper lips catch the orange glow from below; vesicles
// pock the rock. Now and then a seam of magma glows through a crack, or a
// thin cable conduit runs down a joint past a small blue status light. Where you
// stand, a rough rock lip warmed by reflected glow. The one-way floors are
// steel grating catwalks with a strip of blue lights, the ladders steel with
// yellow safety rails. Behind: the cone's smoky dark lit orange from below,
// ash and embers in it; far off, the cavern wall cut into terraces, each
// lined with server racks and their blue lights, cable runs hanging between.
// The floors' steel decks, the racks and everything else at the data centre
// are drawn live (world_magma_draw.cpp).

#include "assets/art_magma33.hpp"

#include "base/math.hpp"
#include "render/vector.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <initializer_list>

namespace gr
{

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr Color kBasalt = rgb(48, 42, 46);
constexpr Color kBasaltLo = rgb(24, 20, 24);
constexpr Color kBasaltHi = rgb(78, 70, 74);
constexpr Color kGlow = rgb(255, 130, 40);
constexpr Color kHot = rgb(255, 214, 120);
constexpr Color kLed = rgb(80, 170, 255);
constexpr Color kSteel = rgb(120, 126, 138);
constexpr Color kInk = rgb(16, 12, 14);

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

// The columns' joints across the 64 px tile (it wraps: 64 is 0).
constexpr std::array<double, 3> kJoint{0.0, 22.0, 43.0};
// Each column's fractures (y), two per column, tilted a little.
constexpr std::array<std::array<double, 2>, 3> kFracture{{{18.0, 47.0}, {6.0, 36.0}, {27.0, 58.0}}};
constexpr std::array<double, 3> kTilt{2.5, -2.0, 1.5};

// The basalt under every variant: three prism columns, each face lit on its
// left and falling into shadow at its right-hand joint, cut by fractures
// whose upper lips glow orange; vesicles and a fine grain.
void basaltBase(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 0, 64, 64);
  setRgba(cr, kBasaltLo, 1.0);
  cairo_fill(cr);
  for (std::size_t i = 0; i < kJoint.size(); ++i)
  {
    const double x0 = kJoint[i], x1 = i + 1 < kJoint.size() ? kJoint[i + 1] : 64.0;
    // The face: a prism, lit on the left.
    cairo_rectangle(cr, x0 + 0.8, 0, x1 - x0 - 1.6, 64);
    fillLinear(cr, x0, 0, x1, 0,
      {{0.0, kBasaltHi}, {0.22, lighten(kBasalt, 0.06f)}, {0.7, kBasalt}, {1.0, darken(kBasalt, 0.35f)}});
    // A second facet down the middle of the face.
    const double mid = x0 + (x1 - x0) * 0.42;
    cairo_move_to(cr, mid, 0);
    cairo_line_to(cr, mid, 64);
    stroke(cr, kBasaltHi, 0.8, 0.35);
    // The fractures: a dark crack, its upper lip catching the glow.
    for (const double fy : kFracture[i])
    {
      const double tilt = kTilt[i];
      cairo_move_to(cr, x0 + 0.5, fy);
      cairo_line_to(cr, x0 + (x1 - x0) * 0.5, fy + tilt * 0.6);
      cairo_line_to(cr, x1 - 0.5, fy + tilt);
      stroke(cr, kInk, 1.6, 0.9);
      cairo_move_to(cr, x0 + 1.5, fy - 1.4);
      cairo_line_to(cr, x0 + (x1 - x0) * 0.5, fy + tilt * 0.6 - 1.4);
      cairo_line_to(cr, x1 - 1.5, fy + tilt - 1.4);
      stroke(cr, kGlow, 0.9, 0.42);
      // The lower lip's top edge, lit faintly.
      cairo_move_to(cr, x0 + 1.5, fy + 1.4);
      cairo_line_to(cr, x1 - 1.5, fy + tilt + 1.4);
      stroke(cr, kBasaltHi, 0.6, 0.35);
    }
  }
  // The joints between the columns: deep shadow, a rim of glow along the
  // bottom of each.
  for (const double x : {0.0, 22.0, 43.0, 64.0})
  {
    cairo_move_to(cr, x, -1);
    cairo_line_to(cr, x, 65);
    stroke(cr, kInk, 2.0, 0.95);
  }
  {
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 40, 0, 64);
    stop(g, 0.0, kGlow, 0.0);
    stop(g, 1.0, kGlow, 0.12);
    cairo_rectangle(cr, 0, 40, 64, 24);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  // Vesicles: gas holes frozen in the rock.
  Rng rng(33011u);
  for (int i = 0; i < 9; ++i)
  {
    const double x = rng.range(3, 61), y = rng.range(3, 61), rad = rng.range(0.7f, 1.6f);
    cairo_arc(cr, x, y, rad, 0, 2 * kPi);
    setRgba(cr, kInk, 0.85);
    cairo_fill(cr);
    cairo_arc(cr, x + 0.3, y + 0.4, rad * 0.7, 0.2, kPi - 0.2);
    stroke(cr, kBasaltHi, 0.5, 0.6);
  }
  for (int i = 0; i < 70; ++i)
  {
    cairo_arc(cr, rng.range(0, 64), rng.range(0, 64), rng.range(0.3f, 0.7f), 0, 2 * kPi);
    setRgba(cr, i % 3 ? rgb(10, 8, 10) : rgb(120, 110, 112), rng.range(0.15f, 0.35f));
    cairo_fill(cr);
  }
}

// Variant 1: a seam of magma glowing through a jagged crack.
void magmaSeam(cairo_t* cr)
{
  static constexpr std::array<std::array<double, 2>, 7> kPts{
    {{-2, 40}, {10, 35}, {19, 39}, {28, 30}, {39, 33}, {50, 26}, {66, 29}}};
  auto path = [&](double dy) {
    cairo_move_to(cr, kPts[0][0], kPts[0][1] + dy);
    for (std::size_t i = 1; i < kPts.size(); ++i)
      cairo_line_to(cr, kPts[i][0], kPts[i][1] + dy);
  };
  for (std::size_t i = 0; i < kPts.size(); i += 2)
    softDot(cr, kPts[i][0], kPts[i][1], 14, 9, kGlow, 0.35);
  path(0);
  stroke(cr, kInk, 5.0, 0.95);
  path(0);
  stroke(cr, rgb(220, 70, 20), 3.0);
  path(-0.3);
  stroke(cr, kHot, 1.2);
  // A branch.
  cairo_move_to(cr, 28, 30);
  cairo_line_to(cr, 31, 22);
  cairo_line_to(cr, 29, 15);
  stroke(cr, kInk, 3.0, 0.9);
  cairo_move_to(cr, 28, 30);
  cairo_line_to(cr, 31, 22);
  cairo_line_to(cr, 29, 15);
  stroke(cr, rgb(240, 110, 30), 1.2);
}

// Variant 2: a thin cable conduit run down a joint in the rock, held by two
// clips, with one small blue status light on a junction box.
void rockConduit(cairo_t* cr)
{
  auto run = [&] {
    cairo_move_to(cr, 41, -2);
    cairo_curve_to(cr, 42, 18, 39, 30, 40, 44);
    cairo_curve_to(cr, 41, 54, 44, 60, 45, 66);
  };
  run();
  stroke(cr, kInk, 4.2, 0.9);
  run();
  stroke(cr, rgb(84, 90, 104), 2.2);
  cairo_move_to(cr, 40.2, 0);
  cairo_curve_to(cr, 41.2, 18, 38.2, 30, 39.2, 44);
  stroke(cr, rgb(150, 156, 170), 0.7, 0.6);
  for (const double cy : {12.0, 52.0})
  {
    const double cx = cy < 30 ? 41.3 : 41.6;
    cairo_rectangle(cr, cx - 3.5, cy - 1.2, 7, 2.4);
    setRgba(cr, rgb(120, 126, 140), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 0.7);
  }
  // The junction box.
  cairo_rectangle(cr, 36.5, 28, 7, 6);
  fillLinear(cr, 0, 28, 0, 34, {{0.0, rgb(130, 136, 150)}, {1.0, rgb(60, 64, 76)}}, 1.0, true);
  stroke(cr, kInk, 0.9);
  softDot(cr, 40, 31, 6, 6, kLed, 0.5);
  cairo_arc(cr, 40, 31, 1.3, 0, 2 * kPi);
  setRgba(cr, rgb(210, 240, 255), 1.0);
  cairo_fill(cr);
}

} // namespace

bool isMagmaMainframe(const Theme& t)
{
  return std::string_view(t.look) == "magma" && t.name != nullptr && std::strcmp(t.name, "MAGMA MAINFRAME") == 0;
}

Texture bakeMainframeSolid(const Renderer& r, const Theme& t, int variant)
{
  (void)t;
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  basaltBase(cr);
  if (variant == 1)
    magmaSeam(cr);
  else if (variant == 2)
    rockConduit(cr);
  return img.toTexture(r, 0.0f, 0.0f);
}

// Where you stand on bare rock: a rough lip, its top edge warm with glow,
// grit on it, a shadow under it. Kept low (2 px over the surface at most).
Texture bakeMainframeSolidTop(const Renderer& r, const Theme& t, float topOffset, int height)
{
  (void)t;
  VectorImage img(64, height);
  cairo_t* cr = img.cr();
  const double y = topOffset;
  auto edge = [&](double x) { return y - 1.0 + 0.8 * std::sin(x / 64.0 * 2 * kPi * 3 + 0.7) + 0.3 * std::sin(x / 8.0 * 2 * kPi); };
  {
    cairo_pattern_t* g = cairo_pattern_create_linear(0, y + 6, 0, y + 18);
    stop(g, 0.0, kInk, 0.5);
    stop(g, 1.0, kInk, 0.0);
    cairo_rectangle(cr, 0, y + 6, 64, 12);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  cairo_move_to(cr, 0, edge(0));
  for (double x = 2; x <= 64; x += 2)
    cairo_line_to(cr, x, edge(x));
  cairo_line_to(cr, 64, y + 7);
  for (double x = 64; x >= 0; x -= 4)
    cairo_line_to(cr, x, y + 7 + 0.8 * std::sin(x * 0.4));
  cairo_close_path(cr);
  fillLinear(cr, 0, y - 2, 0, y + 8, {{0.0, rgb(110, 92, 86)}, {0.4, rgb(66, 56, 58)}, {1.0, kBasaltLo}});
  cairo_move_to(cr, 0, edge(0));
  for (double x = 2; x <= 64; x += 2)
    cairo_line_to(cr, x, edge(x));
  stroke(cr, rgb(255, 170, 100), 1.2, 0.7);
  Rng rng(33021u);
  for (int i = 0; i < 6; ++i)
  {
    const double x = rng.range(3, 61), gy = y + rng.range(0.5f, 3.0f);
    cairo_arc(cr, x, gy, rng.range(0.8f, 1.5f), 0, 2 * kPi);
    setRgba(cr, i % 2 ? rgb(90, 80, 82) : rgb(150, 110, 90), 1.0);
    cairo_fill(cr);
  }
  return img.toTexture(r, 0.0f, topOffset);
}

// One-way floors: a steel grating catwalk: a bright top bar, the grating's
// diamond mesh, a fascia with blue lights, a bracket under it. 64 x 40, the
// walking surface at the top; it tiles side by side.
Texture bakeMainframePlatform(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(64, 40);
  cairo_t* cr = img.cr();
  // The bracket under the deck.
  cairo_move_to(cr, 8, 14);
  cairo_line_to(cr, 32, 30);
  cairo_line_to(cr, 56, 14);
  stroke(cr, kInk, 5.0);
  cairo_move_to(cr, 8, 14);
  cairo_line_to(cr, 32, 30);
  cairo_line_to(cr, 56, 14);
  stroke(cr, rgb(84, 88, 100), 3.0);
  // The grating, seen a little from above: a diamond mesh.
  cairo_rectangle(cr, 0, 2, 64, 8);
  setRgba(cr, rgb(20, 20, 26), 1.0);
  cairo_fill(cr);
  cairo_save(cr);
  cairo_rectangle(cr, 0, 2, 64, 8);
  cairo_clip(cr);
  for (double x = -16; x < 72; x += 6)
  {
    cairo_move_to(cr, x, 2);
    cairo_line_to(cr, x + 8, 10);
    cairo_move_to(cr, x + 8, 2);
    cairo_line_to(cr, x, 10);
  }
  stroke(cr, rgb(130, 136, 150), 1.1);
  cairo_restore(cr);
  // The top bar.
  cairo_rectangle(cr, 0, 0, 64, 3);
  fillLinear(cr, 0, 0, 0, 3, {{0.0, rgb(230, 234, 242)}, {1.0, rgb(120, 126, 140)}});
  // The fascia, its blue lights.
  cairo_rectangle(cr, 0, 10, 64, 6);
  fillLinear(cr, 0, 10, 0, 16, {{0.0, rgb(100, 106, 120)}, {1.0, rgb(44, 46, 56)}}, 1.0, true);
  stroke(cr, kInk, 1.0);
  for (double x = 8; x < 64; x += 16)
  {
    softDot(cr, x, 13, 5, 3, kLed, 0.5);
    cairo_rectangle(cr, x - 2, 12, 4, 2);
    setRgba(cr, rgb(190, 230, 255), 1.0);
    cairo_fill(cr);
  }
  // The glow from below on its underside.
  cairo_rectangle(cr, 0, 15, 64, 1.5);
  setRgba(cr, kGlow, 0.5);
  cairo_fill(cr);
  return img.toTexture(r, 0.0f, 0.0f);
}

// Steel ladder: dark rungs with a tread, yellow safety rails, a warm rim
// on the rails' outer edges.
Texture bakeMainframeLadder(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  for (const double y : {12.0, 44.0})
  {
    cairo_rectangle(cr, 12, y - 2.5, 40, 5);
    fillLinear(cr, 0, y - 2.5, 0, y + 2.5, {{0.0, rgb(170, 176, 190)}, {0.5, rgb(100, 104, 118)}, {1.0, rgb(40, 42, 52)}});
    cairo_rectangle(cr, 12, y - 2.5, 40, 5);
    stroke(cr, kInk, 1.1, 0.9);
    for (double x = 16; x < 50; x += 4)
    {
      cairo_move_to(cr, x, y - 2);
      cairo_line_to(cr, x + 2, y + 2);
      stroke(cr, rgb(30, 30, 38), 0.8, 0.7);
    }
  }
  for (const double x : {10.0, 48.0})
  {
    cairo_rectangle(cr, x, -2, 6, 68);
    fillLinear(cr, x, 0, x + 6, 0, {{0.0, rgb(255, 226, 120)}, {0.45, rgb(232, 180, 40)}, {1.0, rgb(140, 96, 20)}});
    cairo_rectangle(cr, x, -2, 6, 68);
    stroke(cr, kInk, 1.2, 0.9);
    // Black hazard bands.
    for (int k = 0; k < 4; ++k)
    {
      cairo_move_to(cr, x, 4 + k * 16);
      cairo_line_to(cr, x + 6, 10 + k * 16);
      cairo_line_to(cr, x + 6, 14 + k * 16);
      cairo_line_to(cr, x, 8 + k * 16);
      cairo_close_path(cr);
      setRgba(cr, kInk, 0.75);
      cairo_fill(cr);
    }
    cairo_move_to(cr, x + 0.8, -2);
    cairo_line_to(cr, x + 0.8, 66);
    stroke(cr, rgb(255, 170, 90), 0.8, 0.6);
  }
  return img.toTexture(r, 16.0f, 0.0f);
}

// The cone's dark: smoke-black overhead, deep red lower down, the glow of
// the magma below welling up from the bottom edge; smoke banks; ash and
// embers drifting.
Texture bakeMainframeSky(const Renderer& r, const Theme& t)
{
  VectorImage img(kScreenW, kScreenH);
  cairo_t* cr = img.cr();
  cairo_rectangle(cr, 0, 0, kScreenW, kScreenH);
  fillLinear(cr, 0, 0, 0, kScreenH,
    {{0.0, rgb(10, 6, 10)}, {0.45, t.skyTop}, {0.8, lerpColor(t.skyMid, t.skyTop, 0.3f)}, {1.0, t.skyMid}});
  Rng rng(33041u);
  for (int i = 0; i < 5; ++i)
    softDot(cr, rng.range(0, float(kScreenW)), kScreenH + 40, rng.range(260, 460), rng.range(160, 260), rgb(220, 70, 20), 0.28);
  // Smoke banks.
  for (int i = 0; i < 14; ++i)
    softDot(cr, rng.range(-100, float(kScreenW) + 100), rng.range(0, float(kScreenH) * 0.6f), rng.range(120, 260),
      rng.range(50, 110), rgb(4, 2, 4), rng.range(0.25f, 0.5f));
  // Ash (grey flecks) and embers (orange sparks).
  for (int i = 0; i < 110; ++i)
  {
    const bool ember = i % 5 == 0;
    cairo_arc(cr, rng.range(0, float(kScreenW)), rng.range(0, float(kScreenH)),
      ember ? rng.range(1.0f, 2.0f) : rng.range(0.6f, 1.3f), 0, 2 * kPi);
    if (ember)
      setRgba(cr, rgb(255, 160, 70), rng.range(0.4f, 0.9f));
    else
      setRgba(cr, rgb(150, 140, 140), rng.range(0.15f, 0.35f));
    cairo_fill(cr);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

namespace
{

// A run of server racks on a terrace far off: black cabinets, their blue
// lights in rows, now and then an amber one.
void farRacks(cairo_t* cr, double x0, double x1, double floorY, double h, Rng& rng)
{
  for (double x = x0; x + 14 < x1; x += 17)
  {
    cairo_rectangle(cr, x, floorY - h, 15, h);
    setRgba(cr, rgb(14, 14, 22), 1.0);
    cairo_fill(cr);
    cairo_rectangle(cr, x, floorY - h, 15, 1.5);
    setRgba(cr, rgb(70, 60, 70), 0.8);
    cairo_fill(cr);
    for (double y = floorY - h + 4; y < floorY - 3; y += 4)
      for (double lx = x + 3; lx < x + 13; lx += 4)
      {
        const std::uint32_t k = rng.next();
        if (k % 3 == 0)
          continue;
        const bool amber = k % 29 == 0;
        cairo_rectangle(cr, lx, y, 1.6, 1.2);
        setRgba(cr, amber ? rgb(255, 180, 60) : kLed, amber ? 0.9 : 0.55 + double(k % 40) / 100.0);
        cairo_fill(cr);
      }
  }
  // The aisle's light pooled on the terrace's floor.
  softDot(cr, (x0 + x1) * 0.5, floorY, (x1 - x0) * 0.55, 10, kLed, 0.18);
}

} // namespace

// Far: the cavern wall, black basalt cut into terraces, each lined with
// racks whose blue lights read as rows of tiny dots, cable runs sagging
// between them; the rock between is columnar, lit orange from below. It is
// kMainframeFarRise px taller than the screen, anchored that far down, so
// the climb never shows its top.
Texture bakeMainframeFar(const Renderer& r, const Theme& t, int layerW)
{
  const int h = kScreenH + kMainframeFarRise;
  VectorImage img(layerW, h);
  cairo_t* cr = img.cr();
  const Color wall = lerpColor(t.farLayer, kBasaltLo, 0.4f);
  cairo_rectangle(cr, 0, 0, layerW, h);
  fillLinear(cr, 0, 0, 0, h, {{0.0, darken(wall, 0.4f)}, {0.6, wall}, {1.0, lerpColor(wall, rgb(120, 40, 16), 0.5f)}}, 0.92);
  Rng rng(33051u);
  // Columns in the rock face.
  for (double x = 0; x < layerW; x += rng.range(26, 44))
  {
    cairo_move_to(cr, x, 0);
    cairo_line_to(cr, x + rng.range(-6, 6), h);
    stroke(cr, rgb(10, 6, 8), rng.range(1.5f, 3.0f), 0.6);
    cairo_move_to(cr, x + 3, 0);
    cairo_line_to(cr, x + 3 + rng.range(-6, 6), h);
    stroke(cr, rgb(110, 60, 40), 1.0, 0.18);
  }
  // Terraces, every ~170 px, each a ledge with racks on it.
  for (double ty = 150; ty < h - 30; ty += rng.range(150, 200))
  {
    double x = rng.range(-60, 40);
    while (x < layerW)
    {
      const double w = rng.range(260, 520);
      const double x1 = std::min(double(layerW) - 10, x + w);
      if (x1 - x > 80)
      {
        // The cut: a dark recess with the racks in it.
        cairo_rectangle(cr, x, ty - 52, x1 - x, 52);
        setRgba(cr, rgb(6, 6, 10), 0.9);
        cairo_fill(cr);
        farRacks(cr, x + 6, x1 - 6, ty, 40, rng);
        // The ledge's lip, lit from below.
        cairo_rectangle(cr, x - 4, ty, x1 - x + 8, 6);
        fillLinear(cr, 0, ty, 0, ty + 6, {{0.0, rgb(70, 56, 56)}, {1.0, rgb(200, 90, 40)}});
        // A cable run sagging along under the ledge.
        cairo_move_to(cr, x, ty + 12);
        cairo_curve_to(cr, x + (x1 - x) * 0.3, ty + 30, x + (x1 - x) * 0.7, ty + 30, x1, ty + 12);
        stroke(cr, rgb(8, 8, 12), 3.0, 0.9);
      }
      x = x1 + rng.range(60, 240);
    }
  }
  // Glow welling up from below.
  {
    cairo_pattern_t* g = cairo_pattern_create_linear(0, h - 260, 0, h);
    stop(g, 0.0, rgb(255, 90, 30), 0.0);
    stop(g, 1.0, rgb(255, 90, 30), 0.35);
    cairo_rectangle(cr, 0, h - 260, layerW, 260);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  return img.toTexture(r, 0.0f, float(kMainframeFarRise));
}

Texture bakeMainframeNear(const Renderer& r, const Theme& t, int layerW)
{
  (void)t;
  VectorImage img(layerW, 4);
  return img.toTexture(r, 0.0f, 0.0f);
}

Texture bakeMainframeExitBase(const Renderer& r, const Theme& t)
{
  (void)t;
  // 96 wide, 144 tall; (0, 0) is the top-left of the 64 x 128 exit area.
  VectorImage img(96, 144);
  cairo_t* cr = img.cr();
  cairo_translate(cr, 16, 8);
  // The gondola's belly overhead: riveted brass-trimmed red panels.
  cairo_move_to(cr, -16, -8);
  cairo_line_to(cr, 80, -8);
  cairo_line_to(cr, 80, 30);
  cairo_curve_to(cr, 60, 40, 4, 40, -16, 30);
  cairo_close_path(cr);
  fillLinear(cr, 0, -8, 0, 40, {{0.0, rgb(150, 50, 40)}, {0.6, rgb(110, 34, 30)}, {1.0, rgb(60, 18, 18)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  cairo_move_to(cr, -16, 26);
  cairo_curve_to(cr, 4, 35, 60, 35, 80, 26);
  stroke(cr, rgb(230, 190, 90), 2.2);
  for (double x = -10; x < 80; x += 10)
  {
    cairo_arc(cr, x, 2, 1.1, 0, 2 * kPi);
    setRgba(cr, rgb(240, 200, 110), 0.9);
    cairo_fill(cr);
  }
  // The open hatch, warm light inside.
  cairo_rectangle(cr, 18, 2, 30, 30);
  fillLinear(cr, 0, 2, 0, 32, {{0.0, rgb(255, 230, 160)}, {1.0, rgb(255, 160, 70)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  cairo_rectangle(cr, 18, 2, 30, 30);
  stroke(cr, rgb(230, 190, 90), 1.0);
  softDot(cr, 33, 20, 30, 22, rgb(255, 220, 140), 0.4);
  // The boarding ramp: a steel gangway sloping down to the mast top, its
  // treads, rope handrails on stanchions.
  cairo_move_to(cr, 18, 32);
  cairo_line_to(cr, 48, 32);
  cairo_line_to(cr, 60, 128);
  cairo_line_to(cr, 4, 128);
  cairo_close_path(cr);
  fillLinear(cr, 0, 32, 0, 128, {{0.0, rgb(90, 94, 106)}, {1.0, rgb(150, 156, 170)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  for (int k = 1; k < 9; ++k)
  {
    const double u = double(k) / 9.0, y = 32 + u * 96;
    cairo_move_to(cr, 18 - u * 14 + 2, y);
    cairo_line_to(cr, 48 + u * 12 - 2, y);
    stroke(cr, rgb(40, 42, 52), 1.6, 0.8);
  }
  for (const int side : {0, 1})
  {
    const double xa = side == 0 ? 16.0 : 50.0, xb = side == 0 ? 2.0 : 62.0;
    for (int k = 0; k < 3; ++k)
    {
      const double u = double(k) / 2.0;
      const double x = xa + (xb - xa) * u, y = 34 + u * 92;
      cairo_move_to(cr, x, y);
      cairo_line_to(cr, x, y - 26);
      stroke(cr, rgb(200, 170, 90), 1.8);
    }
    cairo_move_to(cr, xa, 8);
    cairo_curve_to(cr, xa + (xb - xa) * 0.5, 70, xb, 96, xb, 100);
    stroke(cr, rgb(220, 196, 140), 1.4, 0.9);
  }
  // A lantern by the hatch.
  softDot(cr, 54, 10, 12, 12, rgb(255, 210, 120), 0.8);
  cairo_arc(cr, 54, 10, 2.4, 0, 2 * kPi);
  setRgba(cr, rgb(255, 250, 220), 1.0);
  cairo_fill(cr);
  return img.toTexture(r, 16.0f, 8.0f);
}

} // namespace gr
