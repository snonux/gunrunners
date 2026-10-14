// Level 39, The Gallery (theme key gallery, look "gallery"): the studio's
// control rooms and stages after hours, lit by the monitors. The masses are
// dark acoustic panels: slate fabric stretched in bevelled frames, a block
// of grey foam wedges now and then, or a wall box of microphone sockets.
// Where you stand, a black studio floor with a sheen, an aluminium nosing
// and a taped-down cable channel. The one-way ledges are cable trays. The
// spikes are shards of broken screen glass in a hazard-striped tray.
// Behind: the lighting grid in the dark, the control room's wall of
// screens, and nearer the studio's kit (cameras on pedestals, light
// stands, flight cases, monitor carts, cable drums). The rooms' furniture
// is drawn by the level (world_gallery_draw.cpp).

#include "assets/art_gallery39.hpp"

#include "base/math.hpp"
#include "render/vector.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <string_view>

namespace gr
{

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr Color kPanel = rgb(42, 46, 58);
constexpr Color kPanelHi = rgb(66, 72, 88);
constexpr Color kPanelLo = rgb(24, 26, 34);
constexpr Color kSeam = rgb(12, 13, 18);
constexpr Color kFoam = rgb(70, 74, 84);
constexpr Color kFloor = rgb(30, 32, 38);
constexpr Color kFloorHi = rgb(96, 108, 130);
constexpr Color kSteel = rgb(150, 156, 168);
constexpr Color kSteelHi = rgb(214, 220, 230);
constexpr Color kSteelLo = rgb(78, 82, 94);
constexpr Color kInk = rgb(10, 10, 16);
constexpr Color kScreen = rgb(120, 190, 255);
constexpr Color kTally = rgb(240, 40, 40);
constexpr Color kTape = rgb(236, 196, 40);

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

void rect(cairo_t* cr, double x, double y, double w, double h, Color c, double a = 1.0)
{
  cairo_rectangle(cr, x, y, w, h);
  setRgba(cr, c, a);
  cairo_fill(cr);
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

void text(cairo_t* cr, const char* s, double cx, double baseline, double size, Color c, double a = 1.0)
{
  selectGameFont(cr);
  cairo_set_font_size(cr, size);
  cairo_text_extents_t e;
  cairo_text_extents(cr, s, &e);
  cairo_move_to(cr, cx - e.width * 0.5 - e.x_bearing, baseline);
  setRgba(cr, c, a);
  cairo_show_text(cr, s);
}

// The acoustic panel: slate fabric with a fine weave, stretched in a frame
// that catches the monitor light along its top and left.
void panel(cairo_t* cr, Rng& rng)
{
  cairo_rectangle(cr, 0, 0, 64, 64);
  setRgba(cr, kSeam, 1.0);
  cairo_fill(cr);
  cairo_rectangle(cr, 2, 2, 60, 60);
  fillLinear(cr, 0, 2, 0, 62, {{0.0, lighten(kPanel, 0.06f)}, {0.5, kPanel}, {1.0, darken(kPanel, 0.18f)}});
  // The weave.
  for (double y = 3; y < 62; y += 2)
  {
    cairo_move_to(cr, 2, y);
    cairo_line_to(cr, 62, y);
  }
  stroke(cr, kPanelLo, 0.5, 0.35);
  for (double x = 3; x < 62; x += 3)
  {
    cairo_move_to(cr, x, 2);
    cairo_line_to(cr, x, 62);
  }
  stroke(cr, kPanelHi, 0.4, 0.12);
  for (int i = 0; i < 40; ++i)
    rect(cr, rng.range(3, 60), rng.range(3, 60), 1.2, 1.2, i % 2 ? kPanelHi : kPanelLo, rng.range(0.2f, 0.45f));
  // The frame's bevel.
  cairo_move_to(cr, 2.5, 61.5);
  cairo_line_to(cr, 2.5, 2.5);
  cairo_line_to(cr, 61.5, 2.5);
  stroke(cr, kPanelHi, 1.2, 0.8);
  cairo_move_to(cr, 61.5, 2.5);
  cairo_line_to(cr, 61.5, 61.5);
  cairo_line_to(cr, 2.5, 61.5);
  stroke(cr, kPanelLo, 1.2, 0.9);
  // A sliver of blue from the monitors on the top edge.
  cairo_rectangle(cr, 3, 3, 58, 1.2);
  setRgba(cr, kScreen, 0.12);
  cairo_fill(cr);
}

// Variant 1: grey foam wedges, four rows of them turned alternately.
void foam(cairo_t* cr)
{
  rect(cr, 0, 0, 64, 64, darken(kFoam, 0.5f));
  for (int by = 0; by < 4; ++by)
    for (int bx = 0; bx < 4; ++bx)
    {
      const double x = bx * 16.0, y = by * 16.0;
      const bool across = (bx + by) % 2 == 0;
      for (int k = 0; k < 3; ++k)
      {
        if (across)
        {
          const double yy = y + k * 5.33;
          cairo_move_to(cr, x + 1, yy + 5.33);
          cairo_line_to(cr, x + 8, yy + 0.8);
          cairo_line_to(cr, x + 15, yy + 5.33);
          cairo_close_path(cr);
          fillLinear(cr, x, yy, x, yy + 5.33, {{0.0, lighten(kFoam, 0.25f)}, {1.0, darken(kFoam, 0.35f)}});
        }
        else
        {
          const double xx = x + k * 5.33;
          cairo_move_to(cr, xx + 5.33, y + 1);
          cairo_line_to(cr, xx + 0.8, y + 8);
          cairo_line_to(cr, xx + 5.33, y + 15);
          cairo_close_path(cr);
          fillLinear(cr, xx, y, xx + 5.33, y, {{0.0, lighten(kFoam, 0.2f)}, {1.0, darken(kFoam, 0.4f)}});
        }
      }
    }
  cairo_rectangle(cr, 0.5, 0.5, 63, 63);
  stroke(cr, kSeam, 1.0, 0.8);
}

// Variant 2: a wall box of microphone sockets on the panel, MIC 1 and 2,
// a stencilled number and a strip of gaffer tape with a name on it.
void wallBox(cairo_t* cr)
{
  cairo_rectangle(cr, 14, 18, 36, 28);
  fillLinear(cr, 0, 18, 0, 46, {{0.0, kSteelHi}, {0.3, kSteel}, {1.0, kSteelLo}}, 1.0, true);
  stroke(cr, kInk, 1.2);
  for (int k = 0; k < 2; ++k)
  {
    const double cx = 24 + k * 16.0, cy = 30;
    cairo_arc(cr, cx, cy, 5.5, 0, 2 * kPi);
    setRgba(cr, rgb(30, 30, 36), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kSteelHi, 0.8, 0.8);
    for (int p = 0; p < 3; ++p)
    {
      const double a = -kPi * 0.5 + p * 2.0 * kPi / 3.0;
      cairo_arc(cr, cx + std::cos(a) * 2.6, cy + std::sin(a) * 2.6, 0.9, 0, 2 * kPi);
      setRgba(cr, rgb(200, 200, 210), 1.0);
      cairo_fill(cr);
    }
    text(cr, k == 0 ? "1" : "2", cx, 43, 6, kInk, 0.9);
  }
  // Gaffer tape label.
  cairo_save(cr);
  cairo_translate(cr, 32, 52);
  cairo_rotate(cr, -0.04);
  rect(cr, -16, -3.5, 32, 7, rgb(220, 220, 210), 0.95);
  text(cr, "STUDIO", 0, 2.4, 5, rgb(40, 40, 60), 0.9);
  cairo_restore(cr);
}

} // namespace

bool isGallery(const Theme& t) { return std::string_view(t.look) == "gallery"; }

Texture bakeGallerySolid(const Renderer& r, const Theme& t, int variant)
{
  (void)t;
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  Rng rng(39011u + std::uint32_t(variant));
  if (variant == 1)
    foam(cr);
  else
  {
    panel(cr, rng);
    if (variant == 2)
      wallBox(cr);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// Where you stand: the studio floor, black paint polished to a sheen that
// takes the monitors' blue, an aluminium nosing along its front edge and a
// cable channel under it, its shadow on the panel below.
Texture bakeGallerySolidTop(const Renderer& r, const Theme& t, float topOffset, int height)
{
  (void)t;
  VectorImage img(64, height);
  cairo_t* cr = img.cr();
  const double y = topOffset;
  {
    cairo_pattern_t* g = cairo_pattern_create_linear(0, y + 14, 0, y + 28);
    stop(g, 0.0, kInk, 0.55);
    stop(g, 1.0, kInk, 0.0);
    cairo_rectangle(cr, 0, y + 14, 64, 14);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  // The floor's top: black paint and its sheen.
  cairo_rectangle(cr, 0, y - 4, 64, 9);
  fillLinear(cr, 0, y - 4, 0, y + 5, {{0.0, rgb(58, 62, 74)}, {0.3, kFloor}, {1.0, darken(kFloor, 0.3f)}});
  cairo_rectangle(cr, 0, y - 4, 64, 1.5);
  setRgba(cr, kFloorHi, 0.85);
  cairo_fill(cr);
  softDot(cr, 20, y - 1, 26, 1.6, kScreen, 0.22);
  Rng rng(39021u);
  for (int i = 0; i < 26; ++i)
    rect(cr, rng.range(0, 63), rng.range(float(y) - 2.5f, float(y) + 4.0f), 1.0, 1.0, i % 3 ? rgb(90, 96, 110) : kInk,
      rng.range(0.2f, 0.5f));
  // The aluminium nosing.
  cairo_rectangle(cr, 0, y + 5, 64, 5);
  fillLinear(cr, 0, y + 5, 0, y + 10, {{0.0, kSteelHi}, {0.45, kSteel}, {1.0, kSteelLo}});
  for (double x = 6; x < 64; x += 32)
  {
    cairo_arc(cr, x, y + 7.5, 1.1, 0, 2 * kPi);
    setRgba(cr, kSteelLo, 1.0);
    cairo_fill(cr);
  }
  // The cable channel under it: a dark slot, a cable showing in it.
  rect(cr, 0, y + 10, 64, 5, rgb(16, 16, 22));
  cairo_move_to(cr, 0, y + 12.6);
  cairo_curve_to(cr, 20, y + 11.8, 40, y + 13.4, 64, y + 12.6);
  stroke(cr, rgb(40, 40, 48), 2.6);
  cairo_move_to(cr, 0, y + 12.0);
  cairo_curve_to(cr, 20, y + 11.2, 40, y + 12.8, 64, y + 12.0);
  stroke(cr, rgb(110, 116, 130), 0.6, 0.5);
  rect(cr, 0, y + 15, 64, 1, kInk, 0.9);
  return img.toTexture(r, 0.0f, topOffset);
}

// A cable tray: a galvanised ladder tray seen from the front, its side
// rail punched with slots, black cable bundled in it with a red and a blue
// run, cable ties, the shadow under it. 64 x 40, the walking surface at
// the top; it tiles side by side.
Texture bakeGalleryPlatform(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(64, 40);
  cairo_t* cr = img.cr();
  {
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 16, 0, 30);
    stop(g, 0.0, kInk, 0.5);
    stop(g, 1.0, kInk, 0.0);
    cairo_rectangle(cr, 0, 16, 64, 14);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  // The cables, bulging over the tray's lip.
  struct Run
  {
    double y;
    Color c;
    double w;
  };
  static constexpr Run kRuns[] = {{2.5, rgb(30, 30, 36), 4.0}, {4.0, rgb(200, 40, 40), 2.6}, {3.0, rgb(40, 40, 48), 4.4},
    {5.0, rgb(50, 110, 210), 2.4}, {4.6, rgb(24, 24, 30), 3.4}};
  for (std::size_t i = 0; i < std::size(kRuns); ++i)
  {
    const auto& k = kRuns[i];
    cairo_move_to(cr, -2, k.y);
    cairo_curve_to(cr, 18, k.y - 1.4 + double(i % 2), 44, k.y + 1.2 - double(i % 2), 66, k.y);
    stroke(cr, k.c, k.w);
    cairo_move_to(cr, -2, k.y - k.w * 0.3);
    cairo_curve_to(cr, 18, k.y - 1.4 + double(i % 2) - k.w * 0.3, 44, k.y + 1.2 - double(i % 2) - k.w * 0.3, 66, k.y - k.w * 0.3);
    stroke(cr, lighten(k.c, 0.45f), 0.6, 0.6);
  }
  // A cable tie.
  rect(cr, 30, 0.5, 2.2, 7, rgb(230, 230, 220), 0.9);
  // The side rail.
  cairo_rectangle(cr, 0, 6, 64, 11);
  fillLinear(cr, 0, 6, 0, 17, {{0.0, kSteelHi}, {0.25, kSteel}, {1.0, kSteelLo}}, 1.0, true);
  stroke(cr, kInk, 0.9, 0.8);
  for (double x = 4; x < 64; x += 10)
  {
    cairo_rectangle(cr, x, 9.5, 6, 3.4);
    setRgba(cr, rgb(20, 20, 26), 0.9);
    cairo_fill(cr);
  }
  cairo_rectangle(cr, 0, 6, 64, 1.2);
  setRgba(cr, rgb(255, 255, 255), 0.6);
  cairo_fill(cr);
  // The walking line along the top.
  cairo_rectangle(cr, 0, 0, 64, 1.0);
  setRgba(cr, kSteelHi, 0.5);
  cairo_fill(cr);
  return img.toTexture(r, 0.0f, 0.0f);
}

// Broken screen glass standing up in a floor tray: jagged shards catching
// blue and white, the tray's yellow-and-black hazard stripes.
Texture bakeGallerySpikes(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  struct Shard
  {
    double x, w, top, lean;
  };
  static constexpr Shard kShards[] = {{6, 9, 16, 3}, {17, 11, 8, -2}, {29, 8, 20, 4}, {38, 12, 10, -3}, {51, 10, 18, 2}};
  for (const auto& s : kShards)
  {
    cairo_move_to(cr, s.x - s.w * 0.5, 52);
    cairo_line_to(cr, s.x - s.w * 0.2, s.top + 14);
    cairo_line_to(cr, s.x + s.lean, s.top);
    cairo_line_to(cr, s.x + s.w * 0.3, s.top + 9);
    cairo_line_to(cr, s.x + s.w * 0.5, 52);
    cairo_close_path(cr);
    fillLinear(cr, s.x - s.w * 0.5, s.top, s.x + s.w * 0.5, 52,
      {{0.0, rgb(220, 240, 255)}, {0.35, rgb(110, 170, 230)}, {0.7, rgb(40, 70, 120)}, {1.0, rgb(20, 30, 50)}}, 0.92, true);
    stroke(cr, rgb(230, 245, 255), 0.9, 0.9);
    cairo_move_to(cr, s.x + s.lean * 0.6, s.top + 4);
    cairo_line_to(cr, s.x - s.w * 0.15, s.top + 20);
    stroke(cr, rgb(255, 255, 255), 1.0, 0.8);
  }
  // The tray, striped.
  cairo_rectangle(cr, 0, 50, 64, 14);
  setRgba(cr, rgb(20, 20, 24), 1.0);
  cairo_fill(cr);
  cairo_save(cr);
  cairo_rectangle(cr, 0, 52, 64, 9);
  cairo_clip(cr);
  rect(cr, 0, 52, 64, 9, kTape);
  for (double x = -12; x < 70; x += 12)
  {
    cairo_move_to(cr, x, 61);
    cairo_line_to(cr, x + 6, 61);
    cairo_line_to(cr, x + 15, 52);
    cairo_line_to(cr, x + 9, 52);
    cairo_close_path(cr);
    setRgba(cr, rgb(20, 20, 24), 1.0);
    cairo_fill(cr);
  }
  cairo_restore(cr);
  cairo_rectangle(cr, 0, 50, 64, 2);
  setRgba(cr, rgb(120, 124, 136), 1.0);
  cairo_fill(cr);
  return img.toTexture(r, 0.0f, 0.0f);
}

namespace
{

// A lamp hanging from the grid: a Fresnel's barrel on its yoke, barn doors
// open (x, y the clamp on the pipe), lit or dark.
void hangingLamp(cairo_t* cr, double x, double y, bool lit, double tilt)
{
  cairo_save(cr);
  cairo_translate(cr, x, y);
  rect(cr, -3, 0, 6, 12, rgb(30, 30, 36));
  cairo_rotate(cr, tilt);
  cairo_move_to(cr, -16, 12);
  cairo_curve_to(cr, -18, 30, 18, 30, 16, 12);
  stroke(cr, rgb(50, 50, 58), 3.0);
  cairo_rectangle(cr, -13, 14, 26, 30);
  fillLinear(cr, -13, 0, 13, 0, {{0.0, rgb(40, 40, 46)}, {0.4, rgb(70, 72, 82)}, {1.0, rgb(26, 26, 32)}}, 1.0, true);
  stroke(cr, kInk, 1.2);
  // The barn doors.
  cairo_move_to(cr, -13, 44);
  cairo_line_to(cr, -22, 56);
  cairo_move_to(cr, 13, 44);
  cairo_line_to(cr, 22, 56);
  stroke(cr, rgb(36, 36, 42), 3.0);
  cairo_rectangle(cr, -11, 42, 22, 4);
  setRgba(cr, lit ? rgb(255, 244, 210) : rgb(30, 32, 40), 1.0);
  cairo_fill(cr);
  cairo_restore(cr);
}

} // namespace

// The studio's height lost in the dark: the lighting grid's pipes, lamps
// hanging off them (two lit, their beams slanting down through the haze),
// cable looped between, the dark going a cold blue lower down.
Texture bakeGallerySky(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(kScreenW, kScreenH);
  cairo_t* cr = img.cr();
  cairo_rectangle(cr, 0, 0, kScreenW, kScreenH);
  fillLinear(cr, 0, 0, 0, kScreenH, {{0.0, rgb(8, 9, 14)}, {0.45, rgb(16, 19, 28)}, {1.0, rgb(28, 34, 48)}});
  // The beams of two lit lamps, through the haze.
  for (const double lx : {300.0, 940.0})
  {
    cairo_move_to(cr, lx - 14, 150);
    cairo_line_to(cr, lx + 14, 150);
    cairo_line_to(cr, lx + 260 * (lx < 600 ? 1 : -1) + 160, kScreenH);
    cairo_line_to(cr, lx + 260 * (lx < 600 ? 1 : -1) - 160, kScreenH);
    cairo_close_path(cr);
    fillLinear(cr, 0, 150, 0, kScreenH, {{0.0, rgb(200, 210, 240)}, {1.0, rgb(120, 140, 200)}}, 0.07);
  }
  // Haze.
  Rng rng(39041u);
  for (int i = 0; i < 18; ++i)
    softDot(cr, rng.range(0, float(kScreenW)), rng.range(200, 700), rng.range(120, 280), rng.range(40, 90), rgb(70, 90, 130), 0.06);
  // The grid: two runs of pipe across, drops down at the joints.
  for (const double py : {60.0, 118.0})
  {
    cairo_rectangle(cr, 0, py - 4, kScreenW, 8);
    fillLinear(cr, 0, py - 4, 0, py + 4, {{0.0, rgb(70, 74, 86)}, {0.5, rgb(44, 46, 56)}, {1.0, rgb(20, 22, 28)}});
  }
  for (double x = 80; x < kScreenW; x += 240)
  {
    rect(cr, x - 3, 0, 6, 120, rgb(30, 32, 40));
    rect(cr, x - 7, 56, 14, 10, rgb(56, 58, 68));
    rect(cr, x - 7, 114, 14, 10, rgb(56, 58, 68));
  }
  // Cable looping between the pipes.
  for (double x = 0; x < kScreenW; x += 160)
  {
    cairo_move_to(cr, x, 122);
    cairo_curve_to(cr, x + 40, 160 + std::fmod(x, 3.0) * 10, x + 120, 160, x + 160, 122);
    stroke(cr, rgb(16, 16, 22), 2.4);
  }
  // Lamps along the lower pipe, two of them lit.
  int k = 0;
  for (double x = 140; x < kScreenW; x += 160, ++k)
  {
    const bool lit = std::abs(x - 300) < 1 || std::abs(x - 940) < 1;
    hangingLamp(cr, x, 122, lit, (k % 2 ? 0.18 : -0.15) + (lit ? (x < 600 ? -0.3 : 0.3) : 0.0));
    if (lit)
      softDot(cr, x, 175, 60, 30, rgb(255, 240, 200), 0.35);
  }
  // Dust in the beams.
  for (int i = 0; i < 80; ++i)
  {
    cairo_arc(cr, rng.range(0, float(kScreenW)), rng.range(150, float(kScreenH)), rng.range(0.5f, 1.3f), 0, 2 * kPi);
    setRgba(cr, rgb(220, 230, 255), rng.range(0.05f, 0.2f));
    cairo_fill(cr);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

namespace
{

constexpr int kFarH = kScreenH + 360;
constexpr int kNearH = kScreenH + 420;

// One screen of the wall (x, y, w, h), showing picture `kind`: 0 colour
// bars, 1 a studio wide shot, 2 a face in close-up, 3 a graphic with a
// headline bar, 4 a weather map, 5 blue with a clock, 6 a dark screen.
void wallScreen(cairo_t* cr, double x, double y, double w, double h, int kind, Rng& rng)
{
  cairo_rectangle(cr, x - 5, y - 5, w + 10, h + 14);
  fillLinear(cr, 0, y - 5, 0, y + h + 9, {{0.0, rgb(44, 46, 54)}, {1.0, rgb(20, 20, 26)}});
  rect(cr, x - 5, y + h + 3, w + 10, 6, rgb(14, 14, 18));
  // The UMD strip under the screen: the source's name in green.
  rect(cr, x + w * 0.2, y + h + 4, w * 0.6, 4, rgb(40, 160, 90), 0.7);
  cairo_save(cr);
  cairo_rectangle(cr, x, y, w, h);
  cairo_clip(cr);
  switch (kind)
  {
    case 0:
    {
      static constexpr Color kBars[7] = {rgb(192, 192, 192), rgb(192, 192, 0), rgb(0, 192, 192), rgb(0, 192, 0),
        rgb(192, 0, 192), rgb(192, 0, 0), rgb(0, 0, 192)};
      for (int i = 0; i < 7; ++i)
        rect(cr, x + w * i / 7.0, y, w / 7.0 + 0.5, h * 0.72, kBars[i]);
      rect(cr, x, y + h * 0.72, w, h * 0.28, rgb(20, 20, 40));
      break;
    }
    case 1:
      cairo_rectangle(cr, x, y, w, h);
      fillLinear(cr, 0, y, 0, y + h, {{0.0, rgb(20, 30, 70)}, {1.0, rgb(60, 50, 110)}});
      rect(cr, x, y + h * 0.7, w, h * 0.3, rgb(40, 30, 60));
      rect(cr, x + w * 0.3, y + h * 0.5, w * 0.4, h * 0.22, rgb(120, 90, 160));
      for (int i = 0; i < 3; ++i)
        softDot(cr, x + w * (0.2 + 0.3 * i), y + h * 0.15, w * 0.12, h * 0.1, rgb(255, 230, 180), 0.8);
      break;
    case 2:
      cairo_rectangle(cr, x, y, w, h);
      fillLinear(cr, 0, y, 0, y + h, {{0.0, rgb(40, 30, 50)}, {1.0, rgb(20, 16, 30)}});
      cairo_arc(cr, x + w * 0.5, y + h * 0.48, h * 0.3, 0, 2 * kPi);
      setRgba(cr, rgb(220, 170, 140), 1.0);
      cairo_fill(cr);
      cairo_arc(cr, x + w * 0.5, y + h * 0.36, h * 0.3, kPi, 2 * kPi);
      setRgba(cr, rgb(70, 40, 30), 1.0);
      cairo_fill(cr);
      rect(cr, x + w * 0.28, y + h * 0.82, w * 0.44, h * 0.2, rgb(60, 60, 90));
      break;
    case 3:
      cairo_rectangle(cr, x, y, w, h);
      fillLinear(cr, x, 0, x + w, 0, {{0.0, rgb(10, 40, 90)}, {1.0, rgb(30, 90, 160)}});
      rect(cr, x, y + h * 0.66, w, h * 0.16, rgb(220, 30, 40));
      rect(cr, x + 4, y + h * 0.7, w * 0.6, h * 0.08, rgb(255, 255, 255), 0.9);
      rect(cr, x + 4, y + h * 0.84, w * 0.8, h * 0.08, rgb(240, 240, 250), 0.6);
      break;
    case 4:
      rect(cr, x, y, w, h, rgb(30, 80, 140));
      for (int i = 0; i < 4; ++i)
        softDot(cr, x + rng.range(0, float(w)), y + rng.range(0, float(h)), w * 0.3, h * 0.3, rgb(60, 160, 70), 0.9);
      cairo_arc(cr, x + w * 0.7, y + h * 0.35, h * 0.12, 0, 2 * kPi);
      setRgba(cr, rgb(255, 220, 60), 1.0);
      cairo_fill(cr);
      break;
    case 5:
      rect(cr, x, y, w, h, rgb(20, 40, 120));
      cairo_arc(cr, x + w * 0.5, y + h * 0.5, h * 0.36, 0, 2 * kPi);
      stroke(cr, rgb(240, 240, 255), 1.6);
      cairo_move_to(cr, x + w * 0.5, y + h * 0.5);
      cairo_line_to(cr, x + w * 0.5, y + h * 0.24);
      cairo_move_to(cr, x + w * 0.5, y + h * 0.5);
      cairo_line_to(cr, x + w * 0.66, y + h * 0.56);
      stroke(cr, rgb(240, 240, 255), 1.6);
      break;
    default:
      rect(cr, x, y, w, h, rgb(14, 18, 26));
      break;
  }
  // Scanlines and the glass's sheen.
  for (double sy = y; sy < y + h; sy += 3)
    rect(cr, x, sy, w, 1, kInk, 0.18);
  cairo_move_to(cr, x, y);
  cairo_line_to(cr, x + w * 0.5, y);
  cairo_line_to(cr, x + w * 0.2, y + h);
  cairo_line_to(cr, x, y + h);
  cairo_close_path(cr);
  setRgba(cr, rgb(255, 255, 255), 0.05);
  cairo_fill(cr);
  cairo_restore(cr);
  softDot(cr, x + w * 0.5, y + h * 0.5, w * 0.9, h * 0.9, kind == 6 ? kInk : kScreen, kind == 6 ? 0.0 : 0.08);
}

// A studio clock: a ring of red second dots round a red LED time.
void studioClock(cairo_t* cr, double cx, double cy)
{
  cairo_arc(cr, cx, cy, 44, 0, 2 * kPi);
  setRgba(cr, rgb(12, 12, 16), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, rgb(60, 62, 72), 3.0);
  for (int i = 0; i < 60; ++i)
  {
    const double a = -kPi * 0.5 + i * 2.0 * kPi / 60.0;
    cairo_arc(cr, cx + std::cos(a) * 36, cy + std::sin(a) * 36, i % 5 == 0 ? 2.2 : 1.4, 0, 2 * kPi);
    setRgba(cr, i < 41 ? kTally : rgb(70, 16, 16), 1.0);
    cairo_fill(cr);
  }
  softDot(cr, cx, cy, 30, 16, kTally, 0.35);
  text(cr, "21:59", cx, cy + 7, 18, rgb(255, 80, 70));
}

} // namespace

// The control room's back wall (layerW x kFarH, it tiles side by side):
// slate acoustic panels in a grid under a dark soffit, a wall of screens
// across it in two rows (bars, studio shots, faces, headlines, weather),
// studio clocks between, a QUIET PLEASE light, a skirting of cable trunking.
// Hazy with distance.
Texture bakeGalleryFar(const Renderer& r, const Theme& t, int layerW)
{
  (void)t;
  const int h = kFarH;
  VectorImage img(layerW, h);
  cairo_t* cr = img.cr();
  const double W = layerW;
  cairo_rectangle(cr, 0, 0, W, h);
  fillLinear(cr, 0, 0, 0, h, {{0.0, rgb(14, 16, 22)}, {0.2, rgb(34, 38, 50)}, {0.7, rgb(40, 44, 58)}, {1.0, rgb(24, 26, 34)}});
  // The panel grid.
  for (double x = 0; x < W; x += 96)
  {
    cairo_move_to(cr, x, 90);
    cairo_line_to(cr, x, 690);
  }
  for (double y = 90; y < 700; y += 96)
  {
    cairo_move_to(cr, 0, y);
    cairo_line_to(cr, W, y);
  }
  stroke(cr, rgb(16, 18, 24), 3.0, 0.9);
  // The soffit.
  cairo_rectangle(cr, 0, 0, W, 90);
  fillLinear(cr, 0, 0, 0, 90, {{0.0, rgb(8, 8, 12)}, {1.0, rgb(22, 24, 30)}});
  for (double x = 60; x < W; x += 200)
    softDot(cr, x, 92, 40, 10, rgb(180, 200, 255), 0.25);
  Rng rng(39071u);
  const double bay = W / 4.0;
  for (int b = 0; b < 4; ++b)
  {
    const double x0 = b * bay + 40;
    // A wall of screens: two rows.
    for (int row = 0; row < 2; ++row)
      for (int c = 0; c < 4; ++c)
      {
        const double sw = 108, sh = 70;
        wallScreen(cr, x0 + c * (sw + 14), 200 + row * (sh + 26), sw, sh, (b * 7 + row * 4 + c * 3) % 7, rng);
      }
    if (b % 2 == 0)
      studioClock(cr, x0 + 4 * 122 + 60, 260);
    else
    {
      // QUIET PLEASE.
      cairo_rectangle(cr, x0 + 4 * 122 + 8, 220, 110, 40);
      setRgba(cr, rgb(60, 14, 14), 1.0);
      cairo_fill_preserve(cr);
      stroke(cr, rgb(20, 20, 24), 3.0);
      text(cr, "QUIET", x0 + 4 * 122 + 63, 238, 13, rgb(255, 120, 110), 0.85);
      text(cr, "PLEASE", x0 + 4 * 122 + 63, 254, 13, rgb(255, 120, 110), 0.85);
    }
    // A light strip under the screens, and the glow down the wall.
    softDot(cr, x0 + 240, 330, 300, 120, kScreen, 0.07);
  }
  // Cable trunking along the skirting.
  cairo_rectangle(cr, 0, 640, W, 26);
  fillLinear(cr, 0, 640, 0, 666, {{0.0, rgb(70, 74, 86)}, {1.0, rgb(30, 32, 40)}});
  cairo_rectangle(cr, 0, 666, W, h - 666);
  fillLinear(cr, 0, 666, 0, h, {{0.0, rgb(22, 24, 30)}, {1.0, rgb(10, 10, 14)}});
  // Haze: the far wall sits back in the dark.
  cairo_rectangle(cr, 0, 0, W, h);
  setRgba(cr, rgb(14, 18, 30), 0.35);
  cairo_fill(cr);
  return img.toTexture(r, 0.0f, 0.0f);
}

namespace
{

// A studio camera on its pedestal (x the column, floor), aimed `dir`, a
// red tally on top.
void pedestalCamera(cairo_t* cr, double x, double floor, int dir)
{
  // The base and its wheels.
  cairo_move_to(cr, x - 60, floor - 16);
  cairo_line_to(cr, x + 60, floor - 16);
  cairo_line_to(cr, x + 44, floor - 40);
  cairo_line_to(cr, x - 44, floor - 40);
  cairo_close_path(cr);
  setRgba(cr, rgb(40, 42, 50), 1.0);
  cairo_fill(cr);
  for (const double wx : {-48.0, 0.0, 48.0})
  {
    cairo_arc(cr, x + wx, floor - 9, 9, 0, 2 * kPi);
    setRgba(cr, rgb(20, 20, 24), 1.0);
    cairo_fill(cr);
  }
  // The column, the steering ring.
  rect(cr, x - 12, floor - 210, 24, 172, rgb(60, 62, 72));
  cairo_save(cr);
  cairo_translate(cr, x, floor - 120);
  cairo_scale(cr, 1.0, 0.22);
  cairo_arc(cr, 0, 0, 52, 0, 2 * kPi);
  cairo_restore(cr);
  stroke(cr, rgb(90, 92, 104), 4.0);
  // The head: camera body, lens hood, viewfinder, pan bars.
  cairo_save(cr);
  cairo_translate(cr, x, floor - 222);
  cairo_scale(cr, dir, 1);
  rect(cr, -46, -50, 92, 52, rgb(54, 56, 66));
  rect(cr, 46, -42, 40, 36, rgb(36, 38, 46));
  rect(cr, 86, -48, 10, 48, rgb(20, 20, 26));
  rect(cr, -40, -84, 54, 34, rgb(44, 46, 54));
  rect(cr, -36, -80, 46, 26, rgb(50, 80, 120), 0.8);
  cairo_move_to(cr, -46, -10);
  cairo_line_to(cr, -110, 30);
  stroke(cr, rgb(70, 72, 82), 5.0);
  cairo_arc(cr, 4, -92, 6, 0, 2 * kPi);
  setRgba(cr, kTally, 1.0);
  cairo_fill(cr);
  softDot(cr, 4, -92, 20, 20, kTally, 0.5);
  cairo_restore(cr);
}

// A light stand (x, floor): a tripod, a telescoping pole, a Fresnel on top.
void lightStand(cairo_t* cr, double x, double floor, double hgt)
{
  cairo_move_to(cr, x - 46, floor);
  cairo_line_to(cr, x, floor - 60);
  cairo_line_to(cr, x + 46, floor);
  cairo_move_to(cr, x, floor - 60);
  cairo_line_to(cr, x, floor - hgt);
  stroke(cr, rgb(48, 50, 58), 5.0);
  cairo_save(cr);
  cairo_translate(cr, x, floor - hgt);
  cairo_rotate(cr, 0.4);
  rect(cr, -24, -30, 48, 40, rgb(44, 46, 54));
  rect(cr, -20, 10, 40, 6, rgb(240, 230, 200), 0.6);
  cairo_restore(cr);
}

// A flight case (x, floor, w, h): black, aluminium edges, a stencil.
void flightCase(cairo_t* cr, double x, double floor, double w, double h, const char* label)
{
  rect(cr, x, floor - h, w, h, rgb(26, 26, 32));
  cairo_rectangle(cr, x, floor - h, w, h);
  stroke(cr, rgb(130, 134, 146), 3.0);
  for (const double cx : {x, x + w})
    for (const double cy : {floor - h, floor})
      rect(cr, cx - 6, cy - 6, 12, 12, rgb(150, 154, 166));
  text(cr, label, x + w * 0.5, floor - h * 0.45, 16, rgb(220, 220, 210), 0.7);
}

// A monitor cart (x, floor): a stack of two monitors on a trolley.
void monitorCart(cairo_t* cr, double x, double floor)
{
  rect(cr, x, floor - 120, 110, 8, rgb(60, 62, 72));
  rect(cr, x, floor - 30, 110, 8, rgb(60, 62, 72));
  rect(cr, x + 4, floor - 120, 6, 104, rgb(50, 52, 60));
  rect(cr, x + 100, floor - 120, 6, 104, rgb(50, 52, 60));
  for (const double wx : {x + 10, x + 100})
  {
    cairo_arc(cr, wx, floor - 8, 8, 0, 2 * kPi);
    setRgba(cr, rgb(18, 18, 22), 1.0);
    cairo_fill(cr);
  }
  for (int k = 0; k < 2; ++k)
  {
    const double y = floor - 120 - 74 * (k + 1);
    rect(cr, x + 6, y, 98, 70, rgb(34, 36, 44));
    rect(cr, x + 14, y + 8, 82, 50, k ? rgb(60, 100, 160) : rgb(40, 70, 120));
    softDot(cr, x + 55, y + 33, 60, 40, kScreen, 0.2);
  }
}

// A cable drum (x, floor): a wooden reel wound with black cable.
void cableDrum(cairo_t* cr, double x, double floor)
{
  cairo_arc(cr, x, floor - 50, 50, 0, 2 * kPi);
  setRgba(cr, rgb(70, 52, 36), 1.0);
  cairo_fill(cr);
  cairo_arc(cr, x, floor - 50, 38, 0, 2 * kPi);
  setRgba(cr, rgb(24, 24, 28), 1.0);
  cairo_fill(cr);
  for (double rr = 14; rr < 38; rr += 5)
  {
    cairo_arc(cr, x, floor - 50, rr, 0, 2 * kPi);
    stroke(cr, rgb(50, 50, 58), 1.2, 0.8);
  }
  cairo_arc(cr, x, floor - 50, 10, 0, 2 * kPi);
  setRgba(cr, rgb(90, 70, 50), 1.0);
  cairo_fill(cr);
  cairo_move_to(cr, x + 30, floor - 30);
  cairo_curve_to(cr, x + 70, floor - 10, x + 90, floor - 2, x + 140, floor - 4);
  stroke(cr, rgb(24, 24, 28), 5.0);
}

} // namespace

// The near layer (layerW x kNearH, tiles side by side; see-through above):
// the studio's kit standing about in the dark, studio cameras on
// pedestals with their tallies lit, light stands, flight cases, monitor
// carts glowing, cable drums; dimmed by the dark.
Texture bakeGalleryNear(const Renderer& r, const Theme& t, int layerW)
{
  (void)t;
  VectorImage img(layerW, kNearH);
  cairo_t* cr = img.cr();
  Rng rng(39081u);
  const double W = layerW, floor = 600;
  double x = rng.range(60, 140);
  int k = 0;
  while (x < W - 260)
  {
    switch (k++ % 6)
    {
      case 0:
        pedestalCamera(cr, x + 70, floor, k % 4 == 1 ? 1 : -1);
        x += 180;
        break;
      case 1:
        lightStand(cr, x, floor, rng.range(260, 340));
        x += 60;
        break;
      case 2:
        flightCase(cr, x, floor, 150, 90, "STUDIO B");
        flightCase(cr, x + 20, floor - 90, 110, 60, "CAMS");
        x += 160;
        break;
      case 3:
        monitorCart(cr, x, floor);
        x += 120;
        break;
      case 4:
        cableDrum(cr, x + 50, floor);
        x += 200;
        break;
      default:
        lightStand(cr, x, floor, rng.range(200, 280));
        flightCase(cr, x + 60, floor, 120, 70, "VT");
        x += 190;
        break;
    }
    x += rng.range(120, 320);
  }
  // The floor's edge in the dark under it.
  cairo_rectangle(cr, 0, floor, W, kNearH - floor);
  fillLinear(cr, 0, floor, 0, floor + 60, {{0.0, rgb(30, 32, 40)}, {1.0, rgb(14, 14, 20)}});
  // Dimmed with distance and the dark.
  cairo_set_operator(cr, CAIRO_OPERATOR_ATOP);
  cairo_rectangle(cr, 0, 0, W, kNearH);
  setRgba(cr, rgb(16, 20, 32), 0.5);
  cairo_fill(cr);
  cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
  return img.toTexture(r, 0.0f, 0.0f);
}

// CAM 1 (96 x 144; (0, 0) the top-left of the 64 x 128 exit area): the big
// studio monitor on its cart, its screen gone white with the live picture,
// a red tally lamp on top, ON AIR on its label strip.
Texture bakeGalleryExitBase(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(96, 144);
  cairo_t* cr = img.cr();
  cairo_translate(cr, 16, 8);
  // The cart.
  rect(cr, 2, 74, 60, 4, rgb(70, 72, 84));
  rect(cr, 2, 112, 60, 4, rgb(70, 72, 84));
  rect(cr, 5, 74, 3, 44, rgb(56, 58, 68));
  rect(cr, 56, 74, 3, 44, rgb(56, 58, 68));
  rect(cr, 12, 96, 40, 14, rgb(30, 30, 36));
  rect(cr, 15, 100, 3, 3, rgb(80, 255, 120));
  rect(cr, 21, 100, 3, 3, kTally);
  for (const double wx : {8.0, 56.0})
  {
    cairo_arc(cr, wx, 123, 5, 0, 2 * kPi);
    setRgba(cr, rgb(16, 16, 20), 1.0);
    cairo_fill(cr);
  }
  // The monitor: a deep casing, the bezel, the screen lit white.
  cairo_rectangle(cr, -4, 10, 72, 62);
  fillLinear(cr, 0, 10, 0, 72, {{0.0, rgb(78, 80, 92)}, {0.2, rgb(52, 54, 64)}, {1.0, rgb(26, 26, 32)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  cairo_rectangle(cr, 2, 15, 60, 42);
  fillLinear(cr, 0, 15, 0, 57, {{0.0, rgb(255, 255, 255)}, {0.6, rgb(220, 240, 255)}, {1.0, rgb(170, 210, 255)}});
  softDot(cr, 32, 36, 34, 24, rgb(255, 255, 255), 0.8);
  text(cr, "LIVE", 32, 41, 12, rgb(220, 30, 40));
  // The label strip: ON AIR.
  rect(cr, 8, 60, 48, 9, rgb(16, 16, 20));
  rect(cr, 10, 61.5, 44, 6, kTally);
  text(cr, "ON AIR  CAM 1", 32, 66.6, 5, rgb(255, 255, 255));
  // The tally lamp.
  rect(cr, 26, 4, 12, 7, rgb(40, 40, 46));
  cairo_arc(cr, 32, 4, 6, kPi, 2 * kPi);
  setRgba(cr, kTally, 1.0);
  cairo_fill(cr);
  softDot(cr, 32, 2, 18, 12, kTally, 0.7);
  return img.toTexture(r, 16.0f, 8.0f);
}

} // namespace gr
