// Level 36, Prop Warehouse (theme key prop_warehouse, look "warehouse"):
// MaxTV's prop store at night. The masses are dark cinder block laid in
// running bond, each block lit a little on its top edge, the mortar sunk
// in shadow, flecks of aggregate in the face; now and then a grey conduit
// runs across with a junction box, or the wall is cracked and wears a faded
// stencilled bay number. Where you stand, the concrete floor's edge: a worn
// yellow safety line, a steel nosing and the shadow under it. The one-way
// ledges are pallet-rack shelves (a chipboard deck on an orange step beam
// with a strip of stock labels), the ladders fixed steel with yellow rails.
// The backdrop is the roof space's dark; far off the corrugated back wall
// under the trusses with high windows and the moon, bare work lights on
// their flex; nearer, tall dark racks crammed with old props.

#include "assets/art_warehouse36.hpp"

#include "base/math.hpp"
#include "render/vector.hpp"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <string_view>

namespace gr
{

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr Color kBlock = rgb(78, 74, 72);
constexpr Color kBlockHi = rgb(112, 106, 100);
constexpr Color kMortar = rgb(30, 28, 30);
constexpr Color kInk = rgb(14, 12, 16);
constexpr Color kSafety = rgb(240, 196, 40);
constexpr Color kRackOrange = rgb(232, 112, 34);
constexpr Color kSteel = rgb(140, 146, 156);

Color darken(Color c, float t) { return lerpColor(c, rgb(0, 0, 0), t); }
Color lighten(Color c, float t) { return lerpColor(c, rgb(255, 255, 255), t); }

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
  stop(g, 0.5, c, a * 0.35);
  stop(g, 1.0, c, 0.0);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
  cairo_restore(cr);
}

void stencil(cairo_t* cr, const char* text, double x, double baseline, double size, Color c, double a)
{
  selectGameFont(cr);
  cairo_set_font_size(cr, size);
  cairo_move_to(cr, x, baseline);
  setRgba(cr, c, a);
  cairo_show_text(cr, text);
}

// One cinder block, (x, y) its top-left, w x h: a face a touch lighter at
// the top, lit on its top and left edges, flecked with aggregate.
void cinderBlock(cairo_t* cr, double x, double y, double w, double h, Rng& rng)
{
  const Color face = lerpColor(kBlock, kBlockHi, rng.range(0.0f, 0.25f));
  cairo_rectangle(cr, x + 1, y + 1, w - 2, h - 2);
  fillLinear(cr, 0, y, 0, y + h, {{0.0, lighten(face, 0.1f)}, {0.3, face}, {1.0, darken(face, 0.18f)}});
  cairo_move_to(cr, x + 1.5, y + h - 2);
  cairo_line_to(cr, x + 1.5, y + 1.5);
  cairo_line_to(cr, x + w - 2, y + 1.5);
  stroke(cr, rgb(150, 142, 132), 1.0, 0.5);
  cairo_move_to(cr, x + w - 1.5, y + 2);
  cairo_line_to(cr, x + w - 1.5, y + h - 1.5);
  cairo_line_to(cr, x + 2, y + h - 1.5);
  stroke(cr, kInk, 1.0, 0.45);
  for (int i = 0; i < 18; ++i)
  {
    cairo_rectangle(cr, x + 2 + rng.range(0, float(w - 5)), y + 2 + rng.range(0, float(h - 5)), rng.range(1.0f, 2.2f), rng.range(1.0f, 2.0f));
    setRgba(cr, i % 3 ? rgb(40, 36, 36) : rgb(150, 144, 136), rng.range(0.2f, 0.45f));
    cairo_fill(cr);
  }
}

// The wall: two courses of blocks a tile, running bond (it tiles both ways).
void blockWall(cairo_t* cr, std::uint32_t seed)
{
  cairo_rectangle(cr, 0, 0, 64, 64);
  setRgba(cr, kMortar, 1.0);
  cairo_fill(cr);
  Rng rng(seed);
  cinderBlock(cr, 0, 0, 64, 32, rng);
  cinderBlock(cr, -32, 32, 64, 32, rng);
  cinderBlock(cr, 32, 32, 64, 32, rng);
  // Grime gathering in the mortar's lower lip.
  for (const double y : {31.0, 63.0})
  {
    cairo_rectangle(cr, 0, y - 3, 64, 3);
    fillLinear(cr, 0, y - 3, 0, y, {{0.0, kInk}, {1.0, kInk}}, 0.18);
  }
}

// Variant 1: a grey conduit across the lower course, with a junction box
// and its saddle clips.
void conduit(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 44, 64, 6);
  fillLinear(cr, 0, 44, 0, 50, {{0.0, rgb(190, 194, 200)}, {0.5, rgb(120, 124, 132)}, {1.0, rgb(56, 58, 66)}});
  for (const double x : {6.0, 54.0})
  {
    cairo_rectangle(cr, x, 42, 4, 10);
    setRgba(cr, rgb(90, 92, 100), 1.0);
    cairo_fill(cr);
  }
  cairo_rectangle(cr, 22, 36, 20, 22);
  fillLinear(cr, 22, 0, 42, 0, {{0.0, rgb(160, 164, 172)}, {1.0, rgb(84, 86, 96)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  cairo_rectangle(cr, 25, 39, 14, 16);
  stroke(cr, rgb(200, 204, 212), 0.8, 0.6);
  // A little yellow lightning sticker.
  cairo_move_to(cr, 33, 41);
  cairo_line_to(cr, 29, 48);
  cairo_line_to(cr, 32, 48);
  cairo_line_to(cr, 30, 54);
  cairo_line_to(cr, 36, 46);
  cairo_line_to(cr, 33, 46);
  cairo_close_path(cr);
  setRgba(cr, kSafety, 0.95);
  cairo_fill(cr);
}

// Variant 2: a crack wandering down the blocks and a faded stencilled bay
// number.
void crackAndBay(cairo_t* cr)
{
  stencil(cr, "B7", 8, 26, 20, rgb(220, 210, 190), 0.22);
  cairo_move_to(cr, 44, 0);
  cairo_line_to(cr, 40, 10);
  cairo_line_to(cr, 46, 19);
  cairo_line_to(cr, 41, 30);
  cairo_line_to(cr, 47, 41);
  cairo_line_to(cr, 43, 52);
  stroke(cr, kInk, 1.6, 0.8);
  cairo_move_to(cr, 46, 19);
  cairo_line_to(cr, 54, 24);
  stroke(cr, kInk, 1.0, 0.6);
  cairo_move_to(cr, 45, 0);
  cairo_line_to(cr, 41, 10);
  cairo_line_to(cr, 47, 19);
  stroke(cr, rgb(150, 142, 132), 0.6, 0.4);
}

// A bare work light hanging on its flex: the cable from the top, a cage
// round the bulb, the bulb's glow and a soft cone of light down.
void workLight(cairo_t* cr, double x, double top, double y, double scale, double alpha)
{
  cairo_move_to(cr, x, top);
  cairo_line_to(cr, x, y - 10 * scale);
  stroke(cr, rgb(20, 18, 20), 1.6 * scale, alpha);
  softDot(cr, x, y + 120 * scale, 90 * scale, 160 * scale, rgb(255, 200, 120), 0.07 * alpha);
  softDot(cr, x, y, 46 * scale, 46 * scale, rgb(255, 210, 140), 0.35 * alpha);
  cairo_arc(cr, x, y, 6 * scale, 0, 2 * kPi);
  setRgba(cr, rgb(255, 244, 210), alpha);
  cairo_fill(cr);
  cairo_arc(cr, x, y, 8 * scale, kPi, 2 * kPi);
  stroke(cr, rgb(40, 36, 34), 1.4 * scale, alpha);
}

} // namespace

bool isPropWarehouse(const Theme& t) { return std::string_view(t.look) == "warehouse"; }

Texture bakeWarehouseSolid(const Renderer& r, const Theme& t, int variant)
{
  (void)t;
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  blockWall(cr, 36011u + std::uint32_t(variant) * 7u);
  if (variant == 1)
    conduit(cr);
  else if (variant == 2)
    crackAndBay(cr);
  return img.toTexture(r, 0.0f, 0.0f);
}

// Where you stand: the concrete floor's edge, seen a little from above, a
// worn yellow safety line along it, a steel nosing on the lip, and the
// shadow it throws on the block below.
Texture bakeWarehouseSolidTop(const Renderer& r, const Theme& t, float topOffset, int height)
{
  (void)t;
  VectorImage img(64, height);
  cairo_t* cr = img.cr();
  const double y = topOffset;
  {
    cairo_pattern_t* g = cairo_pattern_create_linear(0, y + 6, 0, y + 18);
    stop(g, 0.0, kInk, 0.55);
    stop(g, 1.0, kInk, 0.0);
    cairo_rectangle(cr, 0, y + 6, 64, 12);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  // The floor's top, a sliver of it in perspective.
  cairo_rectangle(cr, 0, y - 5, 64, 7);
  fillLinear(cr, 0, y - 5, 0, y + 2, {{0.0, rgb(120, 114, 108)}, {1.0, rgb(92, 88, 84)}});
  // The safety line, worn through in places.
  Rng rng(36021u);
  cairo_rectangle(cr, 0, y - 3, 64, 3);
  setRgba(cr, kSafety, 0.9);
  cairo_fill(cr);
  for (int i = 0; i < 4; ++i)
  {
    cairo_rectangle(cr, rng.range(0, 60), y - 3, rng.range(2, 7), 3);
    setRgba(cr, rgb(110, 104, 98), 0.8);
    cairo_fill(cr);
  }
  cairo_rectangle(cr, 0, y - 6, 64, 1.4);
  setRgba(cr, rgb(170, 162, 150), 0.9);
  cairo_fill(cr);
  // The steel nosing: an angle along the lip, scuffed bright.
  cairo_rectangle(cr, 0, y + 1, 64, 5);
  fillLinear(cr, 0, y + 1, 0, y + 6, {{0.0, rgb(210, 214, 220)}, {0.4, kSteel}, {1.0, rgb(60, 62, 70)}});
  for (int i = 0; i < 5; ++i)
  {
    cairo_move_to(cr, rng.range(0, 60), y + 2.5);
    cairo_rel_line_to(cr, rng.range(3, 9), 0);
    stroke(cr, rgb(250, 250, 255), 0.8, 0.6);
  }
  cairo_rectangle(cr, 0, y + 6, 64, 1);
  setRgba(cr, kInk, 0.9);
  cairo_fill(cr);
  for (const double x : {12.0, 44.0})
  {
    cairo_arc(cr, x, y + 3.5, 1.2, 0, 2 * kPi);
    setRgba(cr, rgb(50, 50, 56), 1.0);
    cairo_fill(cr);
  }
  return img.toTexture(r, 0.0f, topOffset);
}

// A pallet-rack shelf: a chipboard deck on an orange step beam with a strip
// of stock labels, the beam's shadow under it. 64 x 44, the walking surface
// at the top; it tiles side by side.
Texture bakeWarehousePlatform(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(64, 44);
  cairo_t* cr = img.cr();
  // The shadow under the beam.
  {
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 19, 0, 32);
    stop(g, 0.0, kInk, 0.5);
    stop(g, 1.0, kInk, 0.0);
    cairo_rectangle(cr, 0, 19, 64, 13);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  // The chipboard deck, its edge flecked.
  cairo_rectangle(cr, 0, 0, 64, 6);
  fillLinear(cr, 0, 0, 0, 6, {{0.0, rgb(214, 180, 128)}, {1.0, rgb(150, 116, 74)}});
  Rng rng(36031u);
  for (int i = 0; i < 26; ++i)
  {
    cairo_rectangle(cr, rng.range(0, 63), rng.range(0.5f, 5.0f), rng.range(1.0f, 2.5f), 1);
    setRgba(cr, i % 2 ? rgb(110, 80, 50) : rgb(240, 214, 160), 0.6);
    cairo_fill(cr);
  }
  cairo_rectangle(cr, 0, 0, 64, 1.2);
  setRgba(cr, rgb(250, 230, 190), 0.8);
  cairo_fill(cr);
  // The step beam.
  cairo_rectangle(cr, 0, 6, 64, 13);
  fillLinear(cr, 0, 6, 0, 19, {{0.0, lighten(kRackOrange, 0.3f)}, {0.25, kRackOrange}, {0.8, darken(kRackOrange, 0.2f)},
                                  {1.0, darken(kRackOrange, 0.5f)}});
  cairo_rectangle(cr, 0, 6, 64, 13);
  stroke(cr, kInk, 1.0, 0.7);
  // The label strip: white tags with a barcode and a number.
  for (int k = 0; k < 2; ++k)
  {
    const double x = 6 + k * 32;
    cairo_rectangle(cr, x, 9, 20, 8);
    setRgba(cr, rgb(244, 240, 230), 0.92);
    cairo_fill(cr);
    for (int b = 0; b < 8; ++b)
    {
      cairo_rectangle(cr, x + 2 + b * 1.6, 10.5, (b % 3) ? 0.8 : 1.3, 5);
      setRgba(cr, kInk, 0.85);
      cairo_fill(cr);
    }
    cairo_rectangle(cr, x + 15, 11, 3, 4);
    setRgba(cr, rgb(200, 40, 40), 0.8);
    cairo_fill(cr);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// A fixed steel ladder: safety-yellow rails, grey rungs with a non-slip
// tread, the rail ends bolted to the wall.
Texture bakeWarehouseLadder(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  // Wall brackets behind.
  for (const double x : {12.0, 52.0})
  {
    cairo_rectangle(cr, x - 4, 28, 8, 8);
    setRgba(cr, rgb(40, 38, 40), 1.0);
    cairo_fill(cr);
  }
  for (const double y : {14.0, 46.0})
  {
    cairo_rectangle(cr, 13, y - 3, 38, 6);
    fillLinear(cr, 0, y - 3, 0, y + 3, {{0.0, rgb(214, 218, 226)}, {0.5, rgb(130, 134, 144)}, {1.0, rgb(54, 56, 66)}});
    cairo_rectangle(cr, 13, y - 3, 38, 6);
    stroke(cr, kInk, 1.0, 0.9);
    for (double x = 16; x < 50; x += 4)
    {
      cairo_move_to(cr, x, y - 2.4);
      cairo_line_to(cr, x + 2, y + 2.4);
      stroke(cr, rgb(50, 52, 62), 0.8, 0.6);
    }
  }
  for (const double x : {9.0, 49.0})
  {
    cairo_rectangle(cr, x, -2, 6, 68);
    fillLinear(cr, x, 0, x + 6, 0, {{0.0, lighten(kSafety, 0.5f)}, {0.4, kSafety}, {1.0, darken(kSafety, 0.45f)}});
    cairo_rectangle(cr, x, -2, 6, 68);
    stroke(cr, kInk, 1.2, 0.9);
  }
  return img.toTexture(r, 16.0f, 0.0f);
}

// The roof space at night: near black under the roof, a dim warm haze low
// down from the work lights, dust hanging in it.
Texture bakeWarehouseSky(const Renderer& r, const Theme& t)
{
  VectorImage img(kScreenW, kScreenH);
  cairo_t* cr = img.cr();
  cairo_rectangle(cr, 0, 0, kScreenW, kScreenH);
  fillLinear(cr, 0, 0, 0, kScreenH,
    {{0.0, darken(t.skyTop, 0.4f)}, {0.45, t.skyTop}, {0.8, t.skyMid}, {1.0, lerpColor(t.skyMid, t.skyBottom, 0.5f)}});
  Rng rng(36041u);
  for (int i = 0; i < 70; ++i)
  {
    cairo_arc(cr, rng.range(0, float(kScreenW)), rng.range(0, float(kScreenH)), rng.range(0.6f, 1.4f), 0, 2 * kPi);
    setRgba(cr, rgb(255, 220, 170), rng.range(0.04f, 0.14f));
    cairo_fill(cr);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// The far back wall: corrugated sheeting under the roof trusses, a row of
// high windows with the night outside (the moon in one, the studio lot's
// water tower in another), bare work lights on long flexes, and the
// silhouettes of tall racks along the bottom. Taller than the screen,
// anchored kWarehouseFarRise px down.
Texture bakeWarehouseFar(const Renderer& r, const Theme& t, int layerW)
{
  (void)t;
  const int h = kScreenH + kWarehouseFarRise;
  VectorImage img(layerW, h);
  cairo_t* cr = img.cr();
  const double W = layerW;
  // Corrugated sheeting: ribs every 18 px.
  cairo_rectangle(cr, 0, 160, W, h - 160);
  fillLinear(cr, 0, 160, 0, h, {{0.0, rgb(22, 24, 32)}, {0.5, rgb(30, 30, 38)}, {1.0, rgb(26, 24, 28)}});
  for (double x = 0; x < W; x += 18)
  {
    cairo_rectangle(cr, x, 160, 6, h - 160);
    setRgba(cr, rgb(44, 46, 56), 0.5);
    cairo_fill(cr);
    cairo_rectangle(cr, x + 6, 160, 2, h - 160);
    setRgba(cr, rgb(10, 10, 14), 0.4);
    cairo_fill(cr);
  }
  // The high windows: a band of wired glass panes, the night through them.
  for (double x = 60; x < W - 200; x += 320)
  {
    const double wx = x, wy = 250, ww = 220, wh = 110;
    cairo_rectangle(cr, wx - 6, wy - 6, ww + 12, wh + 12);
    setRgba(cr, rgb(14, 14, 18), 1.0);
    cairo_fill(cr);
    cairo_rectangle(cr, wx, wy, ww, wh);
    fillLinear(cr, 0, wy, 0, wy + wh, {{0.0, rgb(30, 44, 86)}, {1.0, rgb(54, 60, 96)}});
    const int idx = int(x / 320);
    if (idx % 4 == 1)
    {
      // The moon.
      softDot(cr, wx + 150, wy + 40, 50, 50, rgb(200, 220, 255), 0.4);
      cairo_arc(cr, wx + 150, wy + 40, 16, 0, 2 * kPi);
      setRgba(cr, rgb(236, 240, 250), 0.95);
      cairo_fill(cr);
      cairo_arc(cr, wx + 144, wy + 36, 4, 0, 2 * kPi);
      setRgba(cr, rgb(190, 196, 214), 0.7);
      cairo_fill(cr);
    }
    else if (idx % 4 == 3)
    {
      // The studio lot's water tower, MAXTV on its tank.
      const double tx = wx + 70, ty = wy + 30;
      for (const double lx : {-22.0, 22.0})
      {
        cairo_move_to(cr, tx + lx * 0.6, ty + 30);
        cairo_line_to(cr, tx + lx, wy + wh);
        stroke(cr, rgb(16, 16, 26), 3.0);
      }
      cairo_rectangle(cr, tx - 28, ty, 56, 32);
      setRgba(cr, rgb(18, 18, 30), 1.0);
      cairo_fill(cr);
      cairo_move_to(cr, tx - 30, ty);
      cairo_line_to(cr, tx, ty - 16);
      cairo_line_to(cr, tx + 30, ty);
      cairo_close_path(cr);
      cairo_fill(cr);
      stencil(cr, "MAXTV", tx - 22, ty + 22, 12, rgb(255, 120, 200), 0.45);
      softDot(cr, tx, ty - 18, 6, 6, rgb(255, 60, 40), 0.8);
    }
    // Stars.
    Rng rs(36051u + std::uint32_t(idx));
    for (int i = 0; i < 8; ++i)
    {
      cairo_arc(cr, wx + rs.range(4, float(ww - 4)), wy + rs.range(4, float(wh - 30)), 0.9, 0, 2 * kPi);
      setRgba(cr, rgb(220, 230, 255), rs.range(0.3f, 0.7f));
      cairo_fill(cr);
    }
    // The glazing bars and the wire mesh.
    for (int k = 1; k < 4; ++k)
    {
      cairo_move_to(cr, wx + ww * k / 4.0, wy);
      cairo_line_to(cr, wx + ww * k / 4.0, wy + wh);
    }
    cairo_move_to(cr, wx, wy + wh * 0.5);
    cairo_line_to(cr, wx + ww, wy + wh * 0.5);
    stroke(cr, rgb(14, 14, 18), 4.0);
    for (double k = 0; k < ww + wh; k += 9)
    {
      cairo_move_to(cr, wx + k, wy);
      cairo_line_to(cr, wx + k - wh, wy + wh);
    }
    cairo_save(cr);
    cairo_rectangle(cr, wx, wy, ww, wh);
    cairo_clip(cr);
    stroke(cr, rgb(120, 140, 190), 0.5, 0.18);
    cairo_restore(cr);
    // Moonlight falling in, a long pale shaft.
    cairo_move_to(cr, wx, wy + wh);
    cairo_line_to(cr, wx + ww, wy + wh);
    cairo_line_to(cr, wx + ww + 160, wy + wh + 420);
    cairo_line_to(cr, wx + 120, wy + wh + 420);
    cairo_close_path(cr);
    {
      cairo_pattern_t* g = cairo_pattern_create_linear(0, wy + wh, 0, wy + wh + 420);
      stop(g, 0.0, rgb(150, 176, 255), 0.09);
      stop(g, 1.0, rgb(150, 176, 255), 0.0);
      cairo_set_source(cr, g);
      cairo_fill(cr);
      cairo_pattern_destroy(g);
    }
  }
  // The roof trusses: a bottom chord, a top chord, the web between.
  cairo_rectangle(cr, 0, 0, W, 160);
  setRgba(cr, rgb(10, 10, 14), 1.0);
  cairo_fill(cr);
  for (double x = 0; x < W; x += 80)
  {
    cairo_move_to(cr, x, 150);
    cairo_line_to(cr, x + 40, 40);
    cairo_line_to(cr, x + 80, 150);
  }
  stroke(cr, rgb(30, 30, 38), 5.0);
  cairo_move_to(cr, 0, 150);
  cairo_line_to(cr, W, 150);
  cairo_move_to(cr, 0, 40);
  cairo_line_to(cr, W, 40);
  stroke(cr, rgb(36, 36, 46), 9.0);
  cairo_move_to(cr, 0, 146);
  cairo_line_to(cr, W, 146);
  stroke(cr, rgb(70, 70, 84), 1.5, 0.6);
  // Work lights hanging off the bottom chord.
  for (double x = 200; x < W; x += 400)
    workLight(cr, x, 150, 420 + 40 * std::sin(x * 0.01), 0.8, 0.85);
  // The far racks: tall silhouettes with boxes on their shelves.
  Rng rng(36061u);
  for (double x = 0; x < W; x += 140)
  {
    const double top = 520 + rng.range(-40, 60);
    cairo_rectangle(cr, x + 6, top, 128, h - top);
    setRgba(cr, rgb(16, 16, 22), 1.0);
    cairo_fill(cr);
    for (double y = top + 60; y < h; y += 90)
    {
      cairo_rectangle(cr, x + 4, y, 132, 5);
      setRgba(cr, rgb(70, 50, 34), 0.7);
      cairo_fill(cr);
      for (double bx = x + 10; bx < x + 124;)
      {
        const double bw = rng.range(18, 40), bh = rng.range(20, 70);
        if (bx + bw > x + 130)
          break;
        cairo_rectangle(cr, bx, y - bh, bw, bh);
        setRgba(cr, rng.uniform() < 0.5f ? rgb(36, 30, 28) : rgb(28, 28, 36), 1.0);
        cairo_fill(cr);
        bx += bw + rng.range(2, 8);
      }
    }
    for (const double ux : {x + 4, x + 132})
    {
      cairo_rectangle(cr, ux, top, 4, h - top);
      setRgba(cr, rgb(70, 44, 26), 0.8);
      cairo_fill(cr);
    }
  }
  return img.toTexture(r, 0.0f, float(kWarehouseFarRise));
}

// The near racks: tall dark pallet racking crammed with the props of old
// shows (crates, a rolled carpet, a suit of armour, a dish, a palm, a
// mannequin, a stack of TVs), caught at their edges by the work lights.
// Taller than the screen, anchored kWarehouseNearRise px down.
Texture bakeWarehouseNear(const Renderer& r, const Theme& t, int layerW)
{
  (void)t;
  const int h = kScreenH + kWarehouseNearRise;
  VectorImage img(layerW, h);
  cairo_t* cr = img.cr();
  Rng rng(36071u);
  const Color body = rgb(30, 28, 34), edge = rgb(84, 70, 60);
  for (double x = 40; x < layerW - 300; x += rng.range(560, 760))
  {
    const double rw = rng.range(260, 380), top = rng.range(80, 220);
    // The uprights and the bracing between them.
    for (const double ux : {x, x + rw - 10})
    {
      cairo_rectangle(cr, ux, top, 10, h - top);
      setRgba(cr, rgb(40, 40, 60), 1.0);
      cairo_fill(cr);
      for (double y = top + 10; y < h; y += 14)
      {
        cairo_rectangle(cr, ux + 3, y, 4, 6);
        setRgba(cr, rgb(18, 18, 26), 1.0);
        cairo_fill(cr);
      }
    }
    // Each shelf: a beam and what is on it.
    for (double y = top + 150; y < h; y += rng.range(160, 210))
    {
      cairo_rectangle(cr, x, y, rw, 12);
      setRgba(cr, rgb(110, 54, 20), 0.85);
      cairo_fill(cr);
      cairo_rectangle(cr, x, y, rw, 2);
      setRgba(cr, rgb(200, 120, 60), 0.4);
      cairo_fill(cr);
      for (double bx = x + 16; bx < x + rw - 30;)
      {
        const int kind = rng.irange(0, 6);
        double bw = 0;
        cairo_new_path(cr);
        if (kind <= 2)
        {
          // A crate.
          bw = rng.range(40, 90);
          const double bh = rng.range(40, 100);
          cairo_rectangle(cr, bx, y - bh, bw, bh);
          setRgba(cr, kind == 0 ? rgb(44, 36, 30) : rgb(36, 34, 40), 1.0);
          cairo_fill(cr);
          cairo_move_to(cr, bx, y - bh);
          cairo_line_to(cr, bx + bw, y);
          cairo_move_to(cr, bx + bw, y - bh);
          cairo_line_to(cr, bx, y);
          stroke(cr, rgb(24, 20, 20), 2.0, 0.8);
          cairo_move_to(cr, bx, y - bh + 1);
          cairo_line_to(cr, bx + bw, y - bh + 1);
          stroke(cr, edge, 1.5, 0.6);
        }
        else if (kind == 3)
        {
          // A rolled carpet lying along the shelf.
          bw = rng.range(90, 130);
          cairo_rectangle(cr, bx, y - 26, bw, 26);
          setRgba(cr, rgb(60, 28, 34), 1.0);
          cairo_fill(cr);
          cairo_arc(cr, bx + bw, y - 13, 13, 0, 2 * kPi);
          setRgba(cr, rgb(80, 40, 44), 1.0);
          cairo_fill(cr);
          cairo_arc(cr, bx + bw, y - 13, 6, 0, 2 * kPi);
          stroke(cr, rgb(40, 20, 24), 1.5);
        }
        else if (kind == 4)
        {
          // A suit of armour (or a mannequin): a figure standing.
          bw = 40;
          const double cx = bx + 20;
          cairo_arc(cr, cx, y - 108, 11, 0, 2 * kPi);
          cairo_rectangle(cr, cx - 15, y - 96, 30, 50);
          cairo_rectangle(cr, cx - 12, y - 46, 9, 46);
          cairo_rectangle(cr, cx + 3, y - 46, 9, 46);
          setRgba(cr, body, 1.0);
          cairo_fill(cr);
          cairo_arc(cr, cx - 3, y - 112, 8, kPi, kPi * 1.6);
          stroke(cr, edge, 1.6, 0.7);
        }
        else if (kind == 5)
        {
          // A stack of old TVs, one still faintly glowing.
          bw = 60;
          for (int k = 0; k < 2; ++k)
          {
            const double ty = y - 44 - k * 44;
            cairo_rectangle(cr, bx, ty, 58, 42);
            setRgba(cr, rgb(34, 32, 36), 1.0);
            cairo_fill(cr);
            cairo_rectangle(cr, bx + 6, ty + 6, 36, 28);
            setRgba(cr, k == 0 ? rgb(40, 70, 80) : rgb(22, 24, 28), 1.0);
            cairo_fill(cr);
          }
        }
        else
        {
          // A potted palm.
          bw = 60;
          const double cx = bx + 30;
          cairo_rectangle(cr, cx - 12, y - 24, 24, 24);
          setRgba(cr, rgb(50, 32, 26), 1.0);
          cairo_fill(cr);
          for (int k = 0; k < 6; ++k)
          {
            const double a = -kPi * 0.95 + k * kPi * 0.18;
            cairo_move_to(cr, cx, y - 24);
            cairo_curve_to(cr, cx + std::cos(a) * 20, y - 60, cx + std::cos(a) * 34, y - 70 + std::sin(a) * 10,
              cx + std::cos(a) * 46, y - 50);
            stroke(cr, rgb(28, 38, 30), 5.0);
          }
        }
        bx += bw + rng.range(10, 30);
      }
    }
    // A work light's warm catch on the rack's near edge.
    cairo_rectangle(cr, x, top, 3, h - top);
    setRgba(cr, rgb(255, 190, 120), 0.12);
    cairo_fill(cr);
  }
  return img.toTexture(r, 0.0f, float(kWarehouseNearRise));
}

Texture bakeWarehouseExitBase(const Renderer& r, const Theme& t)
{
  (void)t;
  // 96 wide, 144 tall; (0, 0) is the top-left of the 64 x 128 exit area.
  VectorImage img(96, 144);
  cairo_t* cr = img.cr();
  cairo_translate(cr, 16, 8);
  // The frame and the door, a gold star on it, a round brass knob.
  cairo_rectangle(cr, -6, 14, 76, 114);
  setRgba(cr, rgb(24, 20, 22), 1.0);
  cairo_fill(cr);
  cairo_rectangle(cr, 0, 20, 64, 108);
  fillLinear(cr, 0, 0, 64, 0, {{0.0, rgb(130, 40, 52)}, {0.5, rgb(170, 60, 72)}, {1.0, rgb(90, 26, 36)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  for (const double y : {28.0, 76.0})
  {
    cairo_rectangle(cr, 8, y, 48, 40);
    stroke(cr, rgb(70, 16, 26), 2.0);
  }
  cairo_save(cr);
  cairo_translate(cr, 32, 52);
  cairo_move_to(cr, 0, -14);
  for (int k = 1; k < 10; ++k)
  {
    const double a = -kPi / 2 + k * kPi / 5, rad = k % 2 ? 6.0 : 14.0;
    cairo_line_to(cr, std::cos(a) * rad, std::sin(a) * rad);
  }
  cairo_close_path(cr);
  fillLinear(cr, 0, -14, 0, 14, {{0.0, rgb(255, 240, 150)}, {1.0, rgb(210, 150, 40)}}, 1.0, true);
  stroke(cr, rgb(80, 50, 10), 1.2);
  cairo_restore(cr);
  cairo_arc(cr, 52, 84, 4, 0, 2 * kPi);
  fillLinear(cr, 48, 80, 56, 88, {{0.0, rgb(255, 230, 150)}, {1.0, rgb(150, 100, 30)}});
  // The sign over it: DRESSING ROOMS in a frame of bulbs.
  cairo_rectangle(cr, -8, -6, 80, 20);
  setRgba(cr, rgb(30, 24, 30), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, rgb(200, 160, 80), 1.6);
  selectGameFont(cr);
  cairo_set_font_size(cr, 8.5);
  cairo_text_extents_t e;
  cairo_text_extents(cr, "DRESSING ROOMS", &e);
  cairo_move_to(cr, 32 - e.width * 0.5 - e.x_bearing, 7.5);
  setRgba(cr, rgb(255, 236, 200), 1.0);
  cairo_show_text(cr, "DRESSING ROOMS");
  for (double x = -6; x <= 70; x += 7.6)
    for (const double y : {-6.0, 14.0})
    {
      softDot(cr, x, y, 4, 4, rgb(255, 210, 120), 0.8);
      cairo_arc(cr, x, y, 1.6, 0, 2 * kPi);
      setRgba(cr, rgb(255, 250, 220), 1.0);
      cairo_fill(cr);
    }
  return img.toTexture(r, 16.0f, 8.0f);
}

} // namespace gr
