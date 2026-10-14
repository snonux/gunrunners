// Level 38, Writers' Room (theme key writers_room, look "writers_room"): the
// show's writers' room at night. The masses are pencil-grey plaster, a
// faint graph ruling and pencil hatching on it, now and then a pencilled
// tally or an old coffee ring. Where you stand, worn blue-grey office carpet
// on a wooden edge. The one-way ledges are script lines: a strip of typed
// script page with its shadow. The spikes are sharpened pencils standing
// points up in a pencil tray; the ladder two long yellow pencils with
// wooden rulers for rungs. Behind: a dark ceiling of acoustic tiles and
// strip lights; the back wall with tall night windows over the city (the
// blinds half down), whiteboards of episode plans and framed posters;
// nearer, bookcases of script binders, filing cabinets, a water cooler,
// plants and desk lamps. The furniture is drawn by the level
// (world_writers_draw.cpp).

#include "assets/art_writers38.hpp"

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
constexpr Color kGrey = rgb(118, 116, 124);
constexpr Color kGreyHi = rgb(150, 148, 156);
constexpr Color kGreyLo = rgb(76, 74, 84);
constexpr Color kGraphite = rgb(58, 58, 68);
constexpr Color kPaper = rgb(246, 240, 222);
constexpr Color kPaperLo = rgb(208, 198, 176);
constexpr Color kCarpet = rgb(70, 88, 106);
constexpr Color kCarpetHi = rgb(108, 128, 146);
constexpr Color kCarpetLo = rgb(36, 46, 60);
constexpr Color kWood = rgb(152, 104, 62);
constexpr Color kWoodHi = rgb(204, 156, 100);
constexpr Color kWoodLo = rgb(90, 58, 32);
constexpr Color kPencil = rgb(244, 186, 36);
constexpr Color kPencilHi = rgb(255, 228, 120);
constexpr Color kPencilLo = rgb(190, 124, 16);
constexpr Color kRed = rgb(214, 34, 44);
constexpr Color kInk = rgb(24, 22, 30);
constexpr Color kLamp = rgb(255, 214, 140);

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
  stop(g, 0.6, c, a * 0.4);
  stop(g, 1.0, c, 0.0);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
  cairo_restore(cr);
}

void text(cairo_t* cr, const char* s, double cx, double baseline, double size, Color c, double a = 1.0, bool italic = false)
{
  selectGameFont(cr, italic);
  cairo_set_font_size(cr, size);
  cairo_text_extents_t e;
  cairo_text_extents(cr, s, &e);
  cairo_move_to(cr, cx - e.width * 0.5 - e.x_bearing, baseline);
  setRgba(cr, c, a);
  cairo_show_text(cr, s);
}

// Pencil hatching: short parallel strokes in graphite over a rectangle.
void hatch(cairo_t* cr, double x, double y, double w, double h, Rng& rng, int n, double a)
{
  cairo_save(cr);
  cairo_rectangle(cr, x, y, w, h);
  cairo_clip(cr);
  for (int i = 0; i < n; ++i)
  {
    const double cx = x + rng.range(0, float(w)), cy = y + rng.range(0, float(h));
    const double len = rng.range(5, 12);
    for (int k = 0; k < 3; ++k)
    {
      cairo_move_to(cr, cx + k * 2.2, cy + len * 0.5);
      cairo_line_to(cr, cx + k * 2.2 + len * 0.5, cy - len * 0.5);
    }
    stroke(cr, kGraphite, 0.7, a * rng.range(0.5f, 1.0f));
  }
  cairo_restore(cr);
}

// The plaster: pencil grey, a little mottled, a faint graph ruling every 16
// px and a scatter of pencil hatching; a soft joint at the block's edges.
void plaster(cairo_t* cr)
{
  Rng rng(38011u);
  cairo_rectangle(cr, 0, 0, 64, 64);
  fillLinear(cr, 0, 0, 0, 64, {{0.0, kGreyHi}, {0.2, kGrey}, {1.0, darken(kGrey, 0.1f)}});
  for (int i = 0; i < 70; ++i)
  {
    cairo_rectangle(cr, rng.range(0, 64), rng.range(0, 64), 1.2, 1.2);
    setRgba(cr, i % 2 ? kGreyHi : kGreyLo, rng.range(0.25f, 0.5f));
    cairo_fill(cr);
  }
  for (double k = 8; k < 64; k += 16)
  {
    cairo_move_to(cr, 0, k);
    cairo_line_to(cr, 64, k);
    cairo_move_to(cr, k, 0);
    cairo_line_to(cr, k, 64);
  }
  stroke(cr, rgb(160, 176, 200), 0.6, 0.22);
  hatch(cr, 0, 0, 64, 64, rng, 3, 0.18);
  cairo_rectangle(cr, 0.5, 0.5, 63, 63);
  stroke(cr, kGreyLo, 1.0, 0.45);
  cairo_move_to(cr, 1.5, 62);
  cairo_line_to(cr, 1.5, 1.5);
  cairo_line_to(cr, 62, 1.5);
  stroke(cr, lighten(kGreyHi, 0.2f), 0.8, 0.35);
}

// Variant 1: a writer's tally scratched into the plaster in pencil, the
// days the episode has been late, one bundle struck through.
void pencilTally(cairo_t* cr)
{
  cairo_save(cr);
  cairo_translate(cr, 18, 22);
  cairo_rotate(cr, -0.06);
  for (int b = 0; b < 2; ++b)
  {
    const double x0 = b * 20.0;
    for (int k = 0; k < 4; ++k)
    {
      cairo_move_to(cr, x0 + k * 4.0, 0.0 + (k % 2) * 0.6);
      cairo_line_to(cr, x0 + k * 4.0 + 0.6, 14.0);
    }
    cairo_move_to(cr, x0 - 2.0, 11.0);
    cairo_line_to(cr, x0 + 15.0, 3.0);
  }
  stroke(cr, kGraphite, 1.1, 0.32);
  cairo_restore(cr);
}

// Variant 2: an old coffee ring where a mug was put down on the wrong
// side of the desk, and a drip under it.
void coffeeRing(cairo_t* cr)
{
  cairo_arc(cr, 36, 38, 11, 0.3, 2 * kPi - 0.4);
  stroke(cr, rgb(120, 88, 56), 2.2, 0.22);
  cairo_arc(cr, 36, 38, 9.5, 1.2, 3.6);
  stroke(cr, rgb(120, 88, 56), 1.0, 0.16);
  cairo_move_to(cr, 44, 47);
  cairo_curve_to(cr, 45, 52, 44, 56, 45, 60);
  stroke(cr, rgb(120, 88, 56), 1.2, 0.16);
}

} // namespace

bool isWritersRoom(const Theme& t) { return std::string_view(t.look) == "writers_room"; }

Texture bakeWritersSolid(const Renderer& r, const Theme& t, int variant)
{
  (void)t;
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  plaster(cr);
  if (variant == 1)
    pencilTally(cr);
  else if (variant == 2)
    coffeeRing(cr);
  return img.toTexture(r, 0.0f, 0.0f);
}

// Where you stand: worn blue-grey office carpet over the edge, its loops
// catching the lamp light, a wooden edging strip along its front and the
// shadow it throws on the plaster below.
Texture bakeWritersSolidTop(const Renderer& r, const Theme& t, float topOffset, int height)
{
  (void)t;
  VectorImage img(64, height);
  cairo_t* cr = img.cr();
  const double y = topOffset;
  {
    cairo_pattern_t* g = cairo_pattern_create_linear(0, y + 12, 0, y + 24);
    stop(g, 0.0, kInk, 0.45);
    stop(g, 1.0, kInk, 0.0);
    cairo_rectangle(cr, 0, y + 12, 64, 12);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  // The carpet.
  cairo_rectangle(cr, 0, y - 4, 64, 11);
  fillLinear(cr, 0, y - 4, 0, y + 7, {{0.0, kCarpetHi}, {0.35, kCarpet}, {1.0, kCarpetLo}});
  Rng rng(38021u);
  for (int i = 0; i < 90; ++i)
  {
    cairo_rectangle(cr, rng.range(0, 64), rng.range(float(y) - 3.5f, float(y) + 6.0f), 1, 1);
    setRgba(cr, i % 3 ? rgb(150, 170, 190) : rgb(20, 26, 36), rng.range(0.15f, 0.4f));
    cairo_fill(cr);
  }
  // A worn path down the middle, a few loose fibres on top.
  softDot(cr, 32, y, 40, 2.5, rgb(160, 176, 190), 0.25);
  for (int i = 0; i < 9; ++i)
  {
    const double x = rng.range(2, 62);
    cairo_move_to(cr, x, y - 3.5);
    cairo_line_to(cr, x + rng.range(-1.5f, 1.5f), y - 6.0);
    stroke(cr, kCarpetHi, 0.8, 0.6);
  }
  // The wooden edging.
  cairo_rectangle(cr, 0, y + 7, 64, 6);
  fillLinear(cr, 0, y + 7, 0, y + 13, {{0.0, kWoodHi}, {0.4, kWood}, {1.0, kWoodLo}});
  for (double x = 6; x < 64; x += 21)
  {
    cairo_move_to(cr, x, y + 8.5);
    cairo_curve_to(cr, x + 6, y + 9.5, x + 10, y + 8, x + 16, y + 9);
    stroke(cr, kWoodLo, 0.6, 0.5);
  }
  cairo_rectangle(cr, 0, y + 13, 64, 1);
  setRgba(cr, kInk, 0.8);
  cairo_fill(cr);
  return img.toTexture(r, 0.0f, topOffset);
}

// A script line: a strip of white script page, typed words along it in
// Courier grey, the paper's torn bottom edge curling a little, its shadow
// under it. 64 x 44, the walking surface at the top; it tiles side by side.
Texture bakeWritersPlatform(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(64, 44);
  cairo_t* cr = img.cr();
  // The shadow.
  {
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 14, 0, 26);
    stop(g, 0.0, kInk, 0.45);
    stop(g, 1.0, kInk, 0.0);
    cairo_rectangle(cr, 0, 14, 64, 12);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  // The strip, its bottom edge a little ragged.
  cairo_move_to(cr, 0, 0);
  cairo_line_to(cr, 64, 0);
  cairo_line_to(cr, 64, 15);
  Rng rng(38031u);
  for (double x = 60; x > 0; x -= 4)
    cairo_line_to(cr, x, 15 + rng.range(-1.2f, 1.2f));
  cairo_line_to(cr, 0, 15);
  cairo_close_path(cr);
  fillLinear(cr, 0, 0, 0, 16, {{0.0, rgb(255, 252, 240)}, {0.7, kPaper}, {1.0, kPaperLo}}, 1.0, true);
  stroke(cr, kInk, 0.9, 0.55);
  cairo_rectangle(cr, 0, 0, 64, 1.5);
  setRgba(cr, rgb(255, 255, 255), 0.9);
  cairo_fill(cr);
  // Typed words: little grey runs of letters along the line.
  static constexpr double kWords[][2] = {{3, 9}, {14, 6}, {22, 12}, {37, 5}, {45, 10}, {58, 6}};
  for (const auto& w : kWords)
    for (double x = w[0]; x < w[0] + w[1] && x < 63; x += 2.2)
    {
      cairo_rectangle(cr, x, 6 + (int(x * 7) % 3) * 0.3, 1.6, 3.6);
      setRgba(cr, rgb(70, 66, 72), 0.75);
      cairo_fill(cr);
    }
  return img.toTexture(r, 0.0f, 0.0f);
}

// The ladder: two long yellow pencils for rails (hexagonal, a lit facet and
// a dark one, the maker's gilt lettering now and then), wooden rulers with
// their tick marks for rungs.
Texture bakeWritersLadder(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  for (const double y : {14.0, 46.0})
  {
    cairo_rectangle(cr, 12, y - 4, 40, 8);
    fillLinear(cr, 0, y - 4, 0, y + 4, {{0.0, rgb(232, 206, 150)}, {0.5, rgb(206, 172, 110)}, {1.0, rgb(150, 116, 64)}}, 1.0, true);
    stroke(cr, kInk, 0.9, 0.9);
    for (double x = 14; x < 52; x += 3)
    {
      cairo_move_to(cr, x, y - 4);
      cairo_line_to(cr, x, y - 4 + (int(x) % 15 == 2 ? 4.5 : 2.2));
    }
    stroke(cr, rgb(70, 50, 30), 0.6, 0.8);
  }
  for (const double x : {9.0, 49.0})
  {
    cairo_rectangle(cr, x, -2, 7, 68);
    fillLinear(cr, x, 0, x + 7, 0, {{0.0, kPencilHi}, {0.35, kPencil}, {0.65, kPencil}, {1.0, kPencilLo}}, 1.0, true);
    stroke(cr, kInk, 1.1, 0.9);
    cairo_move_to(cr, x + 2.4, 0);
    cairo_line_to(cr, x + 2.4, 64);
    cairo_move_to(cr, x + 4.8, 0);
    cairo_line_to(cr, x + 4.8, 64);
    stroke(cr, kPencilLo, 0.6, 0.5);
    // The maker's gilt mark once a block.
    for (int k = 0; k < 4; ++k)
    {
      cairo_rectangle(cr, x + 2.6, 26 + k * 3.2, 2.0, 1.6);
      setRgba(cr, rgb(120, 80, 10), 0.7);
      cairo_fill(cr);
    }
  }
  return img.toTexture(r, 16.0f, 0.0f);
}

// Pencils standing points up in a long wooden pencil tray: yellow ones and
// a couple of coloured ones, each sharpened to a wooden cone and a graphite
// point; a stub of an eraser here and there.
Texture bakeWritersSpikes(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  struct Pencil
  {
    double x, top;
    Color body;
  };
  static constexpr Pencil kPencils[5] = {{6, 14, kPencil}, {18, 22, rgb(200, 40, 50)}, {31, 10, kPencil}, {44, 20, rgb(60, 120, 210)},
    {57, 16, kPencil}};
  for (const auto& p : kPencils)
  {
    const double hw = 5.0, cone = 12.0, bottom = 56;
    // The body.
    cairo_rectangle(cr, p.x - hw, p.top + cone, hw * 2, bottom - p.top - cone);
    fillLinear(cr, p.x - hw, 0, p.x + hw, 0, {{0.0, lighten(p.body, 0.4f)}, {0.4, p.body}, {1.0, darken(p.body, 0.35f)}}, 1.0, true);
    stroke(cr, kInk, 1.0, 0.9);
    cairo_move_to(cr, p.x - 1.5, p.top + cone + 1);
    cairo_line_to(cr, p.x - 1.5, bottom);
    stroke(cr, darken(p.body, 0.2f), 0.6, 0.6);
    // The sharpened cone of bare wood, scalloped where the paint stops.
    cairo_move_to(cr, p.x - hw, p.top + cone);
    cairo_curve_to(cr, p.x - hw * 0.6, p.top + cone - 2, p.x - hw * 0.3, p.top + cone + 1, p.x, p.top + cone - 1);
    cairo_curve_to(cr, p.x + hw * 0.3, p.top + cone + 1, p.x + hw * 0.6, p.top + cone - 2, p.x + hw, p.top + cone);
    cairo_line_to(cr, p.x + 1.3, p.top + 3.5);
    cairo_line_to(cr, p.x - 1.3, p.top + 3.5);
    cairo_close_path(cr);
    fillLinear(cr, p.x - hw, 0, p.x + hw, 0, {{0.0, rgb(250, 222, 180)}, {0.5, rgb(226, 184, 132)}, {1.0, rgb(170, 128, 80)}}, 1.0, true);
    stroke(cr, kInk, 0.9, 0.9);
    // The point.
    cairo_move_to(cr, p.x - 1.4, p.top + 4);
    cairo_line_to(cr, p.x, p.top);
    cairo_line_to(cr, p.x + 1.4, p.top + 4);
    cairo_close_path(cr);
    setRgba(cr, p.body == kPencil ? kGraphite : darken(p.body, 0.2f), 1.0);
    cairo_fill(cr);
    cairo_move_to(cr, p.x - 0.4, p.top + 1);
    cairo_line_to(cr, p.x - 0.8, p.top + 8);
    stroke(cr, rgb(255, 255, 255), 0.7, 0.6);
  }
  // The tray.
  cairo_rectangle(cr, 0, 50, 64, 14);
  fillLinear(cr, 0, 50, 0, 64, {{0.0, kWoodHi}, {0.3, kWood}, {1.0, kWoodLo}}, 1.0, true);
  stroke(cr, kInk, 1.2, 0.9);
  cairo_rectangle(cr, 0, 50, 64, 3);
  setRgba(cr, lighten(kWoodHi, 0.3f), 0.6);
  cairo_fill(cr);
  for (double x = 10; x < 64; x += 26)
  {
    cairo_move_to(cr, x, 56);
    cairo_curve_to(cr, x + 4, 57, x + 8, 55.5, x + 14, 56.5);
    stroke(cr, kWoodLo, 0.7, 0.6);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// The room's height lost in the dark: acoustic ceiling tiles in their grid,
// a few strip lights glowing (most switched off for the night), the dark
// going a warmer grey lower down.
Texture bakeWritersSky(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(kScreenW, kScreenH);
  cairo_t* cr = img.cr();
  cairo_rectangle(cr, 0, 0, kScreenW, kScreenH);
  fillLinear(cr, 0, 0, 0, kScreenH, {{0.0, rgb(26, 26, 34)}, {0.5, rgb(40, 40, 50)}, {1.0, rgb(62, 58, 62)}});
  // The tile grid, in perspective-ish rows that get taller lower down.
  double y = 0;
  for (int row = 0; y < kScreenH; ++row)
  {
    const double hgt = 46 + row * 12;
    cairo_move_to(cr, 0, y);
    cairo_line_to(cr, kScreenW, y);
    stroke(cr, rgb(70, 70, 82), 2.0, 0.5);
    for (double x = std::fmod(row * 37.0, 120.0); x < kScreenW; x += 120)
    {
      cairo_move_to(cr, x, y);
      cairo_line_to(cr, x, y + hgt);
      stroke(cr, rgb(70, 70, 82), 1.5, 0.45);
    }
    // Pinholes in the tiles.
    Rng rng(38041u + std::uint32_t(row));
    for (int i = 0; i < 70; ++i)
    {
      cairo_rectangle(cr, rng.range(0, float(kScreenW)), float(y) + rng.range(4, float(hgt) - 4), 1.2, 1.2);
      setRgba(cr, rgb(16, 16, 22), 0.45);
      cairo_fill(cr);
    }
    // A strip light now and then: the lit ones glowing.
    if (row % 2 == 1)
      for (int k = 0; k < 4; ++k)
      {
        const double lx = 80 + k * 320 + std::fmod(row * 53.0, 90.0);
        const bool lit = (k + row) % 3 == 0;
        if (lit)
          softDot(cr, lx + 60, y + hgt * 0.5, 180, 70, rgb(220, 230, 255), 0.18);
        cairo_rectangle(cr, lx, y + hgt * 0.5 - 7, 120, 14);
        const Color c0 = lit ? rgb(250, 252, 255) : rgb(96, 98, 110), c1 = lit ? rgb(200, 210, 230) : rgb(60, 62, 72);
        fillLinear(cr, 0, y + hgt * 0.5 - 7, 0, y + hgt * 0.5 + 7, {{0.0, c0}, {1.0, c1}}, 1.0, true);
        stroke(cr, rgb(20, 20, 26), 1.2, 0.8);
      }
    y += hgt;
  }
  // Dust in the lamp light.
  Rng rng(38043u);
  for (int i = 0; i < 70; ++i)
  {
    cairo_arc(cr, rng.range(0, float(kScreenW)), rng.range(0, float(kScreenH)), rng.range(0.5f, 1.2f), 0, 2 * kPi);
    setRgba(cr, rgb(255, 230, 190), rng.range(0.05f, 0.18f));
    cairo_fill(cr);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

namespace
{

constexpr int kFarH = kScreenH + 360;
constexpr int kNearH = kScreenH + 420;

// A tall night window at (x, y, w, h): the city outside (towers with lit
// windows, a moon in some), the blinds half down, the frame and sill.
void nightWindow(cairo_t* cr, double x, double y, double w, double h, int seed)
{
  Rng rng(38051u + std::uint32_t(seed));
  cairo_save(cr);
  cairo_rectangle(cr, x, y, w, h);
  cairo_clip(cr);
  cairo_rectangle(cr, x, y, w, h);
  fillLinear(cr, 0, y, 0, y + h, {{0.0, rgb(10, 14, 40)}, {0.6, rgb(30, 36, 80)}, {1.0, rgb(70, 50, 90)}});
  for (int i = 0; i < 26; ++i)
  {
    cairo_arc(cr, x + rng.range(0, float(w)), y + rng.range(0, float(h) * 0.5f), rng.range(0.6f, 1.3f), 0, 2 * kPi);
    setRgba(cr, rgb(255, 255, 240), rng.range(0.3f, 0.8f));
    cairo_fill(cr);
  }
  if (seed % 3 == 0)
  {
    softDot(cr, x + w * 0.7, y + h * 0.42, 40, 40, rgb(255, 250, 220), 0.35);
    cairo_arc(cr, x + w * 0.7, y + h * 0.42, 14, 0, 2 * kPi);
    setRgba(cr, rgb(250, 246, 220), 1.0);
    cairo_fill(cr);
  }
  // The towers.
  for (double bx = x - 10; bx < x + w;)
  {
    const double bw = rng.range(26, 60), bh = rng.range(float(h) * 0.25f, float(h) * 0.6f);
    const double top = y + h - bh;
    cairo_rectangle(cr, bx, top, bw, bh);
    setRgba(cr, rgb(16, 18, 34), 1.0);
    cairo_fill(cr);
    for (double wy = top + 8; wy < y + h - 6; wy += 9)
      for (double wx = bx + 5; wx < bx + bw - 5; wx += 8)
        if (rng.uniform() < 0.35f)
        {
          cairo_rectangle(cr, wx, wy, 4, 5);
          setRgba(cr, rng.uniform() < 0.8f ? rgb(255, 214, 120) : rgb(150, 220, 255), rng.range(0.5f, 0.95f));
          cairo_fill(cr);
        }
    bx += bw + rng.range(2, 10);
  }
  // The blinds, half down: slats with a cord.
  const double drop = h * rng.range(0.3f, 0.5f);
  for (double sy = y; sy < y + drop; sy += 9)
  {
    cairo_rectangle(cr, x, sy, w, 7);
    fillLinear(cr, 0, sy, 0, sy + 7, {{0.0, rgb(220, 216, 204)}, {1.0, rgb(150, 146, 136)}});
  }
  cairo_rectangle(cr, x, y + drop - 2, w, 6);
  setRgba(cr, rgb(120, 116, 106), 1.0);
  cairo_fill(cr);
  cairo_move_to(cr, x + w - 12, y);
  cairo_line_to(cr, x + w - 12, y + drop + 60);
  stroke(cr, rgb(200, 196, 186), 1.4, 0.9);
  cairo_restore(cr);
  // The frame, its mullion and the sill.
  cairo_rectangle(cr, x - 6, y - 6, w + 12, h + 12);
  setRgba(cr, rgb(196, 194, 200), 1.0);
  cairo_set_line_width(cr, 10);
  cairo_stroke(cr);
  cairo_move_to(cr, x + w * 0.5, y + drop);
  cairo_line_to(cr, x + w * 0.5, y + h);
  stroke(cr, rgb(190, 188, 196), 5.0, 1.0);
  cairo_rectangle(cr, x - 16, y + h + 4, w + 32, 12);
  fillLinear(cr, 0, y + h + 4, 0, y + h + 16, {{0.0, rgb(230, 228, 232)}, {1.0, rgb(150, 148, 156)}});
  // A mug and a plant on the sill.
  cairo_rectangle(cr, x + 14, y + h - 14, 14, 18);
  setRgba(cr, seed % 2 ? rgb(200, 60, 60) : rgb(60, 110, 170), 1.0);
  cairo_fill(cr);
  if (seed % 2 == 0)
  {
    cairo_move_to(cr, x + w - 40, y + h + 4);
    cairo_line_to(cr, x + w - 36, y + h - 16);
    cairo_line_to(cr, x + w - 20, y + h - 16);
    cairo_line_to(cr, x + w - 16, y + h + 4);
    cairo_close_path(cr);
    setRgba(cr, rgb(170, 90, 50), 1.0);
    cairo_fill(cr);
    for (int k = 0; k < 6; ++k)
    {
      const double a = -kPi * 0.5 + (k - 2.5) * 0.35;
      cairo_move_to(cr, x + w - 28, y + h - 16);
      cairo_curve_to(cr, x + w - 28 + std::cos(a) * 10, y + h - 26, x + w - 28 + std::cos(a) * 22, y + h - 30 + std::sin(a) * 10,
        x + w - 28 + std::cos(a) * 26, y + h - 22 + std::sin(a) * 24);
      stroke(cr, rgb(60, 120, 60), 3.0, 1.0);
    }
  }
}

// A whiteboard with the season's plan in marker: a grid of episode boxes,
// arrows, a circled "ACT 3??", magnets.
void whiteboard(cairo_t* cr, double x, double y, double w, double h, int seed)
{
  Rng rng(38061u + std::uint32_t(seed));
  cairo_rectangle(cr, x - 6, y - 6, w + 12, h + 12);
  setRgba(cr, rgb(170, 172, 182), 1.0);
  cairo_fill(cr);
  cairo_rectangle(cr, x, y, w, h);
  fillLinear(cr, x, y, x + w, y + h, {{0.0, rgb(250, 252, 255)}, {1.0, rgb(214, 220, 228)}});
  // Ghosts of old wiped plans.
  for (int i = 0; i < 10; ++i)
  {
    cairo_move_to(cr, x + rng.range(10, float(w) - 60), y + rng.range(10, float(h) - 10));
    cairo_rel_curve_to(cr, 15, -5, 30, 5, 50, 0);
    stroke(cr, rgb(160, 170, 190), 2.0, 0.25);
  }
  const Color pens[3] = {rgb(30, 60, 160), rgb(200, 40, 40), rgb(30, 130, 70)};
  const int cols = 4, rows = 2;
  for (int c = 0; c < cols; ++c)
    for (int rr = 0; rr < rows; ++rr)
    {
      const double bx = x + 16 + c * (w - 32) / cols, by = y + 30 + rr * (h - 50) / rows;
      const double bw = (w - 32) / cols - 10, bh = (h - 50) / rows - 12;
      cairo_rectangle(cr, bx, by, bw, bh);
      stroke(cr, pens[0], 1.6, 0.8);
      char ep[8] = {'E', 'P', ' ', char('1' + c + rr * cols), 0, 0, 0, 0};
      text(cr, ep, bx + bw * 0.5, by + 14, 11, pens[0], 0.85);
      for (int l = 0; l < 3; ++l)
      {
        cairo_move_to(cr, bx + 6, by + 22 + l * 9);
        cairo_rel_line_to(cr, rng.range(10, float(bw) - 14), 0);
        stroke(cr, pens[(c + l) % 3], 1.4, 0.7);
      }
    }
  text(cr, seed % 2 ? "SEASON 6" : "WHO RUNS?", x + w * 0.5, y + 20, 16, pens[1], 0.9);
  // An arrow across, a circled question.
  cairo_move_to(cr, x + 40, y + h - 16);
  cairo_curve_to(cr, x + w * 0.4, y + h - 2, x + w * 0.6, y + h - 30, x + w - 50, y + h - 18);
  stroke(cr, pens[1], 2.0, 0.8);
  cairo_arc(cr, x + w - 36, y + h - 18, 16, 0, 2 * kPi);
  stroke(cr, pens[1], 1.6, 0.8);
  text(cr, "??", x + w - 36, y + h - 13, 13, pens[1], 0.9);
  for (int m = 0; m < 3; ++m)
  {
    cairo_arc(cr, x + 20 + m * (w - 40) / 2, y + 8, 5, 0, 2 * kPi);
    setRgba(cr, pens[m], 1.0);
    cairo_fill(cr);
  }
  cairo_rectangle(cr, x - 6, y + h + 6, w + 12, 8);
  setRgba(cr, rgb(150, 152, 162), 1.0);
  cairo_fill(cr);
}

// A framed poster of the show: a black field, the logo in yellow, three
// runner silhouettes, "TONIGHT".
void poster(cairo_t* cr, double x, double y, double w, double h, int seed)
{
  cairo_rectangle(cr, x - 8, y - 8, w + 16, h + 16);
  fillLinear(cr, x - 8, y - 8, x + w, y + h, {{0.0, rgb(60, 58, 64)}, {1.0, rgb(24, 22, 28)}});
  cairo_rectangle(cr, x, y, w, h);
  fillLinear(cr, 0, y, 0, y + h, {{0.0, seed % 2 ? rgb(30, 20, 60) : rgb(70, 16, 20)}, {1.0, rgb(10, 8, 16)}});
  for (int k = 0; k < 3; ++k)
  {
    const double cx = x + w * (0.28 + k * 0.22), base = y + h - 20;
    cairo_arc(cr, cx, base - 70, 9, 0, 2 * kPi);
    cairo_move_to(cr, cx - 12, base);
    cairo_line_to(cr, cx - 9, base - 56);
    cairo_line_to(cr, cx + 9, base - 56);
    cairo_line_to(cr, cx + 12, base);
    cairo_close_path(cr);
    setRgba(cr, rgb(10, 8, 14), 1.0);
    cairo_fill(cr);
  }
  softDot(cr, x + w * 0.5, y + h * 0.62, w * 0.5, h * 0.2, rgb(255, 200, 80), 0.25);
  text(cr, "GUNRUNNERS", x + w * 0.5, y + 34, 16, rgb(255, 214, 60), 1.0, true);
  text(cr, seed % 2 ? "SEASON 6" : "TONIGHT 9/8c", x + w * 0.5, y + h - 6, 10, rgb(240, 236, 226), 0.9);
}

// A round wall clock reading a quarter to three in the morning.
void wallClock(cairo_t* cr, double x, double y)
{
  cairo_arc(cr, x, y, 30, 0, 2 * kPi);
  setRgba(cr, rgb(40, 38, 44), 1.0);
  cairo_fill(cr);
  cairo_arc(cr, x, y, 25, 0, 2 * kPi);
  setRgba(cr, rgb(244, 240, 228), 1.0);
  cairo_fill(cr);
  for (int k = 0; k < 12; ++k)
  {
    const double a = k * kPi / 6;
    cairo_move_to(cr, x + std::cos(a) * 20, y + std::sin(a) * 20);
    cairo_line_to(cr, x + std::cos(a) * 23, y + std::sin(a) * 23);
  }
  stroke(cr, kInk, 1.5, 0.9);
  cairo_move_to(cr, x, y);
  cairo_line_to(cr, x + 12 * std::cos(-kPi * 0.5 + 2.75 * kPi / 6), y + 12 * std::sin(-kPi * 0.5 + 2.75 * kPi / 6));
  stroke(cr, kInk, 3.0);
  cairo_move_to(cr, x, y);
  cairo_line_to(cr, x - 19, y);
  stroke(cr, kInk, 2.0);
  cairo_move_to(cr, x, y);
  cairo_line_to(cr, x + 4, y - 20);
  stroke(cr, kRed, 1.0);
}

} // namespace

// The back wall (layerW x kFarH, it tiles side by side): pencil-grey
// plaster above a dark skirting, tall night windows with the blinds half
// down, whiteboards of the season's plan, framed posters of the show and a
// clock reading 2:45. Hazy with distance.
Texture bakeWritersFar(const Renderer& r, const Theme& t, int layerW)
{
  (void)t;
  const int h = kFarH;
  VectorImage img(layerW, h);
  cairo_t* cr = img.cr();
  const double W = layerW;
  cairo_rectangle(cr, 0, 0, W, h);
  fillLinear(cr, 0, 0, 0, h, {{0.0, rgb(44, 44, 52)}, {0.15, rgb(104, 102, 110)}, {0.6, rgb(132, 128, 134)}, {1.0, rgb(96, 92, 98)}});
  // A cornice under the ceiling.
  cairo_rectangle(cr, 0, 60, W, 18);
  fillLinear(cr, 0, 60, 0, 78, {{0.0, rgb(200, 198, 204)}, {1.0, rgb(110, 108, 116)}});
  // Pencil hatching drifting over the plaster.
  Rng rng(38071u);
  hatch(cr, 0, 90, W, 560, rng, 260, 0.12);
  const double bay = W / 8.0;
  for (int b = 0; b < 8; ++b)
  {
    const double x0 = b * bay;
    // A pilaster between bays.
    cairo_rectangle(cr, x0 - 14, 78, 28, 640);
    fillLinear(cr, x0 - 14, 0, x0 + 14, 0, {{0.0, rgb(100, 98, 106)}, {0.4, rgb(150, 148, 156)}, {1.0, rgb(90, 88, 96)}});
    switch (b % 4)
    {
      case 0:
      case 2:
        nightWindow(cr, x0 + 50, 130, bay - 100, 400, b);
        break;
      case 1:
        whiteboard(cr, x0 + 34, 210, bay - 68, 190, b);
        wallClock(cr, x0 + bay * 0.5, 140);
        break;
      default:
        poster(cr, x0 + 50, 180, 100, 150, b);
        poster(cr, x0 + bay - 150, 220, 100, 150, b + 1);
        break;
    }
  }
  // The skirting and a strip of carpet.
  cairo_rectangle(cr, 0, 700, W, h - 700);
  fillLinear(cr, 0, 700, 0, h, {{0.0, rgb(60, 70, 84)}, {1.0, rgb(30, 36, 46)}});
  cairo_rectangle(cr, 0, 680, W, 22);
  fillLinear(cr, 0, 680, 0, 702, {{0.0, rgb(150, 110, 70)}, {1.0, rgb(80, 56, 34)}});
  // Haze: the far wall sits back in the dim room.
  cairo_rectangle(cr, 0, 0, W, h);
  setRgba(cr, rgb(40, 38, 50), 0.28);
  cairo_fill(cr);
  return img.toTexture(r, 0.0f, 0.0f);
}

namespace
{

// A bookcase of script binders (x, floor, w, h): shelves of coloured
// binders with white spine labels, a stack lying flat, a trophy.
void bookcase(cairo_t* cr, double x, double floor, double w, double h, Rng& rng)
{
  static constexpr Color kBinders[6] = {rgb(200, 50, 50), rgb(50, 90, 170), rgb(240, 200, 60), rgb(40, 40, 46), rgb(60, 140, 90),
    rgb(230, 230, 220)};
  const double top = floor - h;
  cairo_rectangle(cr, x, top, w, h);
  fillLinear(cr, x, 0, x + w, 0, {{0.0, kWoodLo}, {0.3, kWood}, {1.0, kWoodLo}});
  const int shelves = int(h / 80);
  for (int s = 0; s < shelves; ++s)
  {
    const double sy = top + 10 + s * (h - 20) / shelves, sh = (h - 20) / shelves;
    cairo_rectangle(cr, x + 8, sy, w - 16, sh - 8);
    setRgba(cr, rgb(30, 22, 16), 1.0);
    cairo_fill(cr);
    double bx = x + 10;
    while (bx < x + w - 24)
    {
      if (rng.uniform() < 0.1f)
      {
        // A stack lying flat.
        for (int k = 0; k < 4; ++k)
        {
          cairo_rectangle(cr, bx, sy + sh - 14 - k * 7, 40, 6);
          setRgba(cr, kBinders[rng.next() % 6u], 1.0);
          cairo_fill(cr);
        }
        bx += 44;
        continue;
      }
      const double bw = rng.range(9, 14), bh = sh - 8 - rng.range(4, 16);
      const Color c = kBinders[rng.next() % 6u];
      cairo_rectangle(cr, bx, sy + sh - 8 - bh, bw, bh);
      setRgba(cr, c, 1.0);
      cairo_fill(cr);
      cairo_rectangle(cr, bx + 2, sy + sh - 8 - bh + 8, bw - 4, 10);
      setRgba(cr, rgb(250, 246, 236), 0.85);
      cairo_fill(cr);
      bx += bw + 1;
    }
    cairo_rectangle(cr, x + 4, sy + sh - 8, w - 8, 8);
    fillLinear(cr, 0, sy + sh - 8, 0, sy + sh, {{0.0, kWoodHi}, {1.0, kWoodLo}});
  }
}

// A filing cabinet (x, floor): four steel drawers with handles and label
// slots, a plant or a stack of scripts on top.
void cabinet(cairo_t* cr, double x, double floor, Rng& rng)
{
  const double w = 90, h = 240, top = floor - h;
  cairo_rectangle(cr, x, top, w, h);
  fillLinear(cr, x, 0, x + w, 0, {{0.0, rgb(110, 116, 126)}, {0.4, rgb(170, 176, 186)}, {1.0, rgb(90, 96, 106)}}, 1.0, true);
  stroke(cr, kInk, 1.5, 0.8);
  for (int d = 0; d < 4; ++d)
  {
    const double dy = top + 6 + d * 58;
    cairo_rectangle(cr, x + 6, dy, w - 12, 52);
    stroke(cr, rgb(60, 64, 72), 1.4, 0.8);
    cairo_rectangle(cr, x + 30, dy + 10, 30, 10);
    setRgba(cr, rgb(250, 246, 236), 0.9);
    cairo_fill(cr);
    cairo_rectangle(cr, x + 32, dy + 28, 26, 6);
    setRgba(cr, rgb(60, 64, 72), 1.0);
    cairo_fill(cr);
  }
  if (rng.uniform() < 0.5f)
    for (int k = 0; k < 5; ++k)
    {
      cairo_rectangle(cr, x + 10 + rng.range(-2, 2), top - 8 - k * 7, 66, 6);
      setRgba(cr, k % 2 ? kPaper : kPaperLo, 1.0);
      cairo_fill(cr);
    }
  else
  {
    cairo_rectangle(cr, x + 30, top - 30, 30, 30);
    setRgba(cr, rgb(170, 90, 50), 1.0);
    cairo_fill(cr);
    for (int k = 0; k < 7; ++k)
    {
      const double a = -kPi * 0.5 + (k - 3) * 0.32;
      cairo_move_to(cr, x + 45, top - 30);
      cairo_line_to(cr, x + 45 + std::cos(a) * 46, top - 30 + std::sin(a) * 50);
      stroke(cr, rgb(60, 130, 60), 5.0, 1.0);
    }
  }
}

// A water cooler (x, floor): a blue bottle upside down on a white stand.
void cooler(cairo_t* cr, double x, double floor)
{
  cairo_rectangle(cr, x, floor - 170, 60, 170);
  fillLinear(cr, x, 0, x + 60, 0, {{0.0, rgb(180, 180, 186)}, {0.4, rgb(240, 240, 244)}, {1.0, rgb(160, 160, 168)}}, 1.0, true);
  stroke(cr, kInk, 1.4, 0.8);
  cairo_move_to(cr, x + 8, floor - 170);
  cairo_curve_to(cr, x + 4, floor - 230, x + 6, floor - 270, x + 30, floor - 270);
  cairo_curve_to(cr, x + 54, floor - 270, x + 56, floor - 230, x + 52, floor - 170);
  cairo_close_path(cr);
  fillLinear(cr, x, 0, x + 60, 0, {{0.0, rgb(70, 130, 200)}, {0.4, rgb(160, 210, 255)}, {1.0, rgb(50, 100, 170)}}, 0.85, true);
  stroke(cr, kInk, 1.4, 0.7);
  cairo_rectangle(cr, x + 22, floor - 130, 16, 10);
  setRgba(cr, rgb(200, 40, 40), 1.0);
  cairo_fill(cr);
}

// A desk lamp's pool of warm light, the lamp itself (x, floor).
void lamp(cairo_t* cr, double x, double floor)
{
  softDot(cr, x + 30, floor - 60, 140, 90, kLamp, 0.35);
  cairo_move_to(cr, x, floor);
  cairo_line_to(cr, x + 30, floor - 80);
  cairo_line_to(cr, x + 60, floor - 110);
  stroke(cr, rgb(40, 60, 50), 5.0, 1.0);
  cairo_move_to(cr, x + 46, floor - 128);
  cairo_line_to(cr, x + 90, floor - 100);
  cairo_line_to(cr, x + 74, floor - 82);
  cairo_close_path(cr);
  setRgba(cr, rgb(40, 100, 70), 1.0);
  cairo_fill(cr);
  softDot(cr, x + 82, floor - 86, 30, 20, rgb(255, 240, 200), 0.8);
}

} // namespace

// The near layer (layerW x kNearH, tiles side by side; see-through above):
// bookcases of script binders, filing cabinets with plants and scripts on
// top, a water cooler, desk lamps glowing; dimmed by the dark of the room.
Texture bakeWritersNear(const Renderer& r, const Theme& t, int layerW)
{
  (void)t;
  VectorImage img(layerW, kNearH);
  cairo_t* cr = img.cr();
  Rng rng(38081u);
  const double W = layerW, floor = 780;
  double x = rng.range(10, 80);
  int k = 0;
  while (x < W - 200)
  {
    switch (k++ % 5)
    {
      case 0:
      {
        const double w = rng.range(220, 300);
        bookcase(cr, x, floor, w, rng.range(330, 420), rng);
        x += w;
        break;
      }
      case 1:
        cabinet(cr, x, floor, rng);
        cabinet(cr, x + 94, floor, rng);
        x += 190;
        break;
      case 2:
        cooler(cr, x, floor);
        x += 70;
        break;
      case 3:
        lamp(cr, x, floor - 150);
        cairo_rectangle(cr, x - 30, floor - 150, 180, 150);
        fillLinear(cr, 0, floor - 150, 0, floor, {{0.0, kWood}, {1.0, kWoodLo}});
        x += 160;
        break;
      default:
      {
        const double w = rng.range(180, 240);
        bookcase(cr, x, floor, w, rng.range(260, 340), rng);
        x += w;
        break;
      }
    }
    x += rng.range(80, 240);
  }
  // Dimmed with distance and the dark.
  cairo_set_operator(cr, CAIRO_OPERATOR_ATOP);
  cairo_rectangle(cr, 0, 0, W, kNearH);
  setRgba(cr, rgb(30, 30, 44), 0.55);
  cairo_fill(cr);
  cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
  return img.toTexture(r, 0.0f, 0.0f);
}

// The back door (96 x 144; (0, 0) the top-left of the 64 x 128 exit area):
// a grey office door open into a warm corridor, light pouring round it,
// THE END in black on its frosted glass, a brass push plate and a little
// lamp over the frame.
Texture bakeWritersExitBase(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(96, 144);
  cairo_t* cr = img.cr();
  cairo_translate(cr, 16, 8);
  // The frame.
  cairo_rectangle(cr, -6, 6, 76, 122);
  fillLinear(cr, -6, 0, 70, 0, {{0.0, rgb(150, 148, 156)}, {0.3, rgb(220, 218, 224)}, {1.0, rgb(120, 118, 126)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  // The corridor's light through the gap.
  cairo_rectangle(cr, 2, 16, 60, 112);
  fillLinear(cr, 0, 16, 0, 128, {{0.0, rgb(255, 250, 226)}, {1.0, rgb(255, 214, 140)}});
  softDot(cr, 32, 70, 50, 70, rgb(255, 255, 230), 0.6);
  // The door, swung in a little.
  cairo_move_to(cr, 2, 16);
  cairo_line_to(cr, 46, 22);
  cairo_line_to(cr, 46, 124);
  cairo_line_to(cr, 2, 128);
  cairo_close_path(cr);
  fillLinear(cr, 2, 0, 46, 0, {{0.0, rgb(90, 96, 110)}, {0.5, rgb(130, 136, 150)}, {1.0, rgb(80, 84, 98)}}, 1.0, true);
  stroke(cr, kInk, 1.8);
  // The frosted glass and THE END on it.
  cairo_move_to(cr, 8, 28);
  cairo_line_to(cr, 40, 32);
  cairo_line_to(cr, 40, 72);
  cairo_line_to(cr, 8, 74);
  cairo_close_path(cr);
  fillLinear(cr, 0, 28, 0, 74, {{0.0, rgb(236, 240, 236)}, {1.0, rgb(200, 206, 204)}}, 1.0, true);
  stroke(cr, kInk, 1.0, 0.8);
  selectGameFont(cr, true);
  cairo_set_font_size(cr, 8.0);
  cairo_move_to(cr, 12, 48);
  setRgba(cr, kInk, 0.9);
  cairo_show_text(cr, "THE");
  cairo_move_to(cr, 12, 60);
  cairo_show_text(cr, "END");
  // The push plate.
  cairo_rectangle(cr, 34, 82, 6, 16);
  setRgba(cr, rgb(214, 180, 90), 1.0);
  cairo_fill(cr);
  // The lamp over the frame.
  softDot(cr, 32, 0, 26, 12, kLamp, 0.8);
  cairo_rectangle(cr, 24, -2, 16, 6);
  setRgba(cr, rgb(250, 240, 210), 1.0);
  cairo_fill(cr);
  return img.toTexture(r, 16.0f, 8.0f);
}

} // namespace gr
