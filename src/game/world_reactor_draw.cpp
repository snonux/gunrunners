// Level 20's drawing (world_reactor.cpp has the rules): the glowing core on
// the back wall, the lead booths, the wires the Conduit Sparks ride and the
// glow running ahead of each spark, the leaking coolant pipe and its drops,
// the valves, the lift cage, the unlit ladder that only shows in a flash,
// the DO NOT PRESS console, the scenery (dosimeter, crane, lead glass
// windows, pipes, signs), the pulse rings and their flash, the Shield
// Drones' bubbles, the Deflector Bracer's shield and its arcs, the pulse
// counter, and the Stop Motion bonus's film look. The pieces are baked with
// Cairo the first time they are drawn and kept in the Art's sprite cache.

#include "game/world.hpp"

#include "assets/art.hpp"
#include "base/math.hpp"
#include "game/reactor_draw.hpp"
#include "render/vector.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <string>

// baked() returns a reference into the Art's cache, not into its key or
// painter arguments; GCC's heuristic flags every call that passes temporaries.
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 13
#pragma GCC diagnostic ignored "-Wdangling-reference"
#endif

namespace gr
{

float& reactorRenderAlpha()
{
  static float alpha = 1.0f;
  return alpha;
}

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr float kCellPx = float(kCellSize) * kPixelScale; // 32
constexpr float kTilePx = kCellPx * float(kCellsPerTile);  // 64
constexpr Color kInk = rgb(16, 18, 28);
constexpr Color kYellow = rgb(255, 204, 40);
constexpr Color kCharcoal = rgb(28, 30, 38);
constexpr Color kLead = rgb(112, 118, 130);
constexpr Color kLeadLight = rgb(168, 174, 188);
constexpr Color kLeadDark = rgb(58, 62, 74);
constexpr Color kSteel = rgb(150, 158, 174);
constexpr Color kSteelLight = rgb(220, 226, 236);
constexpr Color kSteelDark = rgb(60, 66, 82);
constexpr Color kCore = rgb(90, 180, 255);
constexpr Color kCoreWhite = rgb(225, 242, 255);
constexpr Color kCoolant = rgb(140, 255, 80);

Color darken(Color c, float t) { return lerpColor(c, rgb(0, 0, 0), t); }

bool visible(float x, float y, float w, float h)
{
  return x + w > -96.0f && x < float(kScreenW) + 96.0f && y + h > -96.0f && y < float(kScreenH) + 96.0f;
}

// A texture baked once and kept in the Art's sprite cache under `key`.
const Texture& baked(const Art& art, const Renderer& r, const std::string& key, int w, int h, float ax, float ay,
  const std::function<void(cairo_t*)>& paint)
{
  const std::string id = "~reactor/" + key;
  auto it = art.styled.find(id);
  if (it != art.styled.end())
    return it->second.facing[0];
  VectorImage img(w, h);
  paint(img.cr());
  Sprite s;
  s.facing[0] = img.toTexture(r, ax, ay);
  return art.styled.emplace(id, std::move(s)).first->second.facing[0];
}

void setRgba(cairo_t* cr, Color c, double a)
{
  cairo_set_source_rgba(cr, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, a);
}

void linearFill(cairo_t* cr, double x0, double y0, double x1, double y1, Color a, Color b, bool preserve = false)
{
  cairo_pattern_t* g = cairo_pattern_create_linear(x0, y0, x1, y1);
  cairo_pattern_add_color_stop_rgba(g, 0, redOf(a) / 255.0, greenOf(a) / 255.0, blueOf(a) / 255.0, alphaOf(a) / 255.0);
  cairo_pattern_add_color_stop_rgba(g, 1, redOf(b) / 255.0, greenOf(b) / 255.0, blueOf(b) / 255.0, alphaOf(b) / 255.0);
  cairo_set_source(cr, g);
  if (preserve)
    cairo_fill_preserve(cr);
  else
    cairo_fill(cr);
  cairo_pattern_destroy(g);
}

void outline(cairo_t* cr, double width = 2.0, Color c = kInk)
{
  setColor(cr, c);
  cairo_set_line_width(cr, width);
  cairo_stroke(cr);
}

void bolt(cairo_t* cr, double x, double y, double rad)
{
  cairo_arc(cr, x, y, rad, 0, 2 * kPi);
  cairo_pattern_t* p = cairo_pattern_create_radial(x - rad * 0.4, y - rad * 0.4, 0.2, x, y, rad);
  cairo_pattern_add_color_stop_rgb(p, 0, 0.95, 0.96, 1.0);
  cairo_pattern_add_color_stop_rgb(p, 1, 0.36, 0.4, 0.48);
  cairo_set_source(cr, p);
  cairo_fill(cr);
  cairo_pattern_destroy(p);
}

// Yellow and black hazard stripes filling a rect.
void hazard(cairo_t* cr, double x, double y, double w, double h, double step)
{
  cairo_save(cr);
  cairo_rectangle(cr, x, y, w, h);
  cairo_clip(cr);
  setColor(cr, kYellow);
  cairo_paint(cr);
  for (double sx = x - h - step; sx < x + w + h; sx += step)
  {
    cairo_move_to(cr, sx, y + h);
    cairo_line_to(cr, sx + step * 0.5, y + h);
    cairo_line_to(cr, sx + step * 0.5 + h, y);
    cairo_line_to(cr, sx + h, y);
    cairo_close_path(cr);
  }
  setColor(cr, kCharcoal);
  cairo_fill(cr);
  cairo_restore(cr);
}

void centredText(cairo_t* cr, const char* s, double cx, double baseline, double size)
{
  selectGameFont(cr);
  cairo_set_font_size(cr, size);
  cairo_text_extents_t e;
  cairo_text_extents(cr, s, &e);
  cairo_move_to(cr, cx - e.width * 0.5 - e.x_bearing, baseline);
  cairo_show_text(cr, s);
}

// The radiation trefoil: three blades and a hub, `rad` px out.
void trefoil(cairo_t* cr, double cx, double cy, double rad)
{
  for (int k = 0; k < 3; ++k)
  {
    const double a = -kPi * 0.5 + k * kPi * 2.0 / 3.0;
    cairo_new_sub_path(cr);
    cairo_arc(cr, cx, cy, rad, a - kPi / 6.0, a + kPi / 6.0);
    cairo_arc_negative(cr, cx, cy, rad * 0.3, a + kPi / 6.0, a - kPi / 6.0);
    cairo_close_path(cr);
  }
  cairo_new_sub_path(cr);
  cairo_arc(cr, cx, cy, rad * 0.18, 0, 2 * kPi);
}

float ease(float t)
{
  t = std::clamp(t, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

// --- The core ------------------------------------------------------------------------

// One slice of the core column, `w` px wide and four blocks (256 px) tall:
// steel pillars either side, the glass tube with the deep blue glow in it
// and its white-hot middle, a dark containment ring across the top.
void paintCoreSlice(cairo_t* cr, double w)
{
  constexpr double H = 256;
  cairo_rectangle(cr, 0, 0, w, H);
  setColor(cr, rgb(16, 20, 32));
  cairo_fill(cr);
  const double gx = 30, gw = w - 60;
  // The glass tube.
  cairo_rectangle(cr, gx, 0, gw, H);
  cairo_pattern_t* g = cairo_pattern_create_linear(gx, 0, gx + gw, 0);
  cairo_pattern_add_color_stop_rgb(g, 0.0, 0.02, 0.04, 0.12);
  cairo_pattern_add_color_stop_rgb(g, 0.2, 0.05, 0.14, 0.38);
  cairo_pattern_add_color_stop_rgb(g, 0.4, 0.14, 0.38, 0.82);
  cairo_pattern_add_color_stop_rgb(g, 0.47, 0.55, 0.8, 1.0);
  cairo_pattern_add_color_stop_rgb(g, 0.5, 0.92, 0.97, 1.0);
  cairo_pattern_add_color_stop_rgb(g, 0.53, 0.55, 0.8, 1.0);
  cairo_pattern_add_color_stop_rgb(g, 0.6, 0.14, 0.38, 0.82);
  cairo_pattern_add_color_stop_rgb(g, 0.8, 0.05, 0.14, 0.38);
  cairo_pattern_add_color_stop_rgb(g, 1.0, 0.02, 0.04, 0.12);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
  // Faint streaks of energy rising up the tube, and the fuel rods' shadows.
  for (int k = 0; k < 8; ++k)
  {
    const double x = gx + gw * (0.08 + 0.12 * k);
    if (std::abs(x - (gx + gw * 0.5)) < gw * 0.06)
      continue;
    cairo_rectangle(cr, x, 0, 3, H);
    setRgba(cr, rgb(150, 210, 255), k % 2 ? 0.08 : 0.14);
    cairo_fill(cr);
    cairo_rectangle(cr, x + 10, 0, 7, H);
    setRgba(cr, rgb(0, 6, 24), 0.25);
    cairo_fill(cr);
  }
  // Glass shine either side.
  for (const double x : {gx + 10.0, gx + gw - 18.0})
  {
    cairo_rectangle(cr, x, 0, 6, H);
    setRgba(cr, rgb(200, 230, 255), 0.12);
    cairo_fill(cr);
  }
  // The steel pillars.
  for (const double x : {0.0, w - 30})
  {
    cairo_rectangle(cr, x, 0, 30, H);
    linearFill(cr, x, 0, x + 30, 0, rgb(110, 118, 134), rgb(40, 44, 58), true);
    cairo_new_path(cr);
    cairo_rectangle(cr, x + (x > 0 ? 0 : 26), 0, 4, H);
    setColor(cr, kInk);
    cairo_fill(cr);
    for (double y = 44; y < H; y += 64)
      bolt(cr, x + 15, y, 3.0);
  }
  // The containment ring: dark steel with little blue lamps.
  cairo_rectangle(cr, 0, 0, w, 16);
  linearFill(cr, 0, 0, 0, 16, rgb(96, 104, 120), rgb(30, 34, 46), true);
  outline(cr, 2.0);
  for (double x = 40; x < w - 30; x += 56)
  {
    cairo_arc(cr, x, 8, 2.6, 0, 2 * kPi);
    setColor(cr, rgb(150, 220, 255));
    cairo_fill(cr);
  }
  for (const double x : {14.0, w - 14})
    bolt(cr, x, 8, 2.6);
}

// The heavy cap over the column's top, w + 40 px wide, 56 tall.
void paintCoreCap(cairo_t* cr, double w)
{
  const double W = w + 40;
  // Pipes rising into the ceiling.
  for (const double x : {W * 0.25, W * 0.5, W * 0.75})
  {
    cairo_rectangle(cr, x - 9, 0, 18, 20);
    linearFill(cr, x - 9, 0, x + 9, 0, kSteelLight, kSteelDark, true);
    outline(cr, 1.6);
  }
  roundedRect(cr, 2, 16, W - 4, 38, 8);
  linearFill(cr, 0, 16, 0, 54, rgb(180, 188, 204), rgb(70, 76, 94), true);
  outline(cr, 2.4);
  hazard(cr, 12, 40, W - 24, 9, 14);
  cairo_rectangle(cr, 12, 40, W - 24, 9);
  outline(cr, 1.2);
  for (double x = 14; x < W - 8; x += 40)
    bolt(cr, x, 26, 3.0);
  setColor(cr, rgb(30, 34, 46));
  centredText(cr, "CORE 01", W * 0.5, 36, 13);
}

// The base flange at the floor, w + 60 px wide, 70 tall.
void paintCoreBase(cairo_t* cr, double w)
{
  const double W = w + 60;
  cairo_move_to(cr, 12, 70);
  cairo_line_to(cr, 26, 16);
  cairo_line_to(cr, W - 26, 16);
  cairo_line_to(cr, W - 12, 70);
  cairo_close_path(cr);
  linearFill(cr, 0, 16, 0, 70, rgb(170, 178, 196), rgb(52, 58, 74), true);
  outline(cr, 2.4);
  roundedRect(cr, 18, 6, W - 36, 16, 5);
  linearFill(cr, 0, 6, 0, 22, kSteelLight, kSteel, true);
  outline(cr, 2.0);
  hazard(cr, 30, 48, W - 60, 12, 16);
  cairo_rectangle(cr, 30, 48, W - 60, 12);
  outline(cr, 1.2);
  for (double x = 34; x < W - 30; x += 44)
    bolt(cr, x, 32, 3.2);
}

// --- Lead booths ---------------------------------------------------------------------

// One block of a lead booth's back wall: `kind` 0 the top (in the roof's
// shadow), 1 plain, 2 the hazard band, 3 the foot (a kick plate).
void paintBoothBlock(cairo_t* cr, int kind)
{
  cairo_rectangle(cr, 0, 0, 64, 64);
  linearFill(cr, 0, 0, 64, 64, rgb(104, 110, 124), rgb(78, 82, 96));
  // The seam between plates, and rivets.
  cairo_rectangle(cr, 0, 0, 2, 64);
  setColor(cr, rgb(44, 48, 58));
  cairo_fill(cr);
  cairo_rectangle(cr, 2, 0, 1.5, 64);
  setColor(cr, rgb(150, 156, 170));
  cairo_fill(cr);
  for (double y : {10.0, 54.0})
    bolt(cr, 10, y, 2.2);
  // Faint mottling of the lead.
  for (int k = 0; k < 5; ++k)
  {
    cairo_arc(cr, 16 + (k * 23) % 44, 12 + (k * 17) % 40, 5 + k % 3, 0, 2 * kPi);
    setRgba(cr, k % 2 ? rgb(130, 136, 150) : rgb(70, 74, 88), 0.18);
    cairo_fill(cr);
  }
  if (kind == 0)
  {
    cairo_rectangle(cr, 0, 0, 64, 22);
    linearFill(cr, 0, 0, 0, 22, rgba(0, 0, 0, 150), rgba(0, 0, 0, 0));
  }
  if (kind == 2)
  {
    hazard(cr, 0, 22, 64, 18, 16);
    cairo_rectangle(cr, -2, 22, 68, 18);
    outline(cr, 1.6);
  }
  if (kind == 3)
  {
    cairo_rectangle(cr, 0, 44, 64, 20);
    linearFill(cr, 0, 44, 0, 64, rgb(70, 74, 86), rgb(40, 42, 52));
    cairo_rectangle(cr, 0, 44, 64, 2);
    setColor(cr, rgb(140, 146, 160));
    cairo_fill(cr);
  }
}

// A booth's side post, 22 x h px: a thick lead pillar with a yellow and
// black band at the top and the foot, bolted.
void paintBoothPost(cairo_t* cr, double h)
{
  roundedRect(cr, 1, 0, 20, h, 3);
  linearFill(cr, 1, 0, 21, 0, kLeadLight, kLeadDark, true);
  outline(cr, 2.0);
  for (const double y : {6.0, h - 40})
  {
    hazard(cr, 3, y, 16, 32, 12);
    cairo_rectangle(cr, 3, y, 16, 32);
    outline(cr, 1.2);
  }
  for (double y = 54; y < h - 50; y += 48)
    bolt(cr, 11, y, 2.6);
}

// The SHIELDED lamp: a small dark plate, a green lamp, green lettering,
// 96 x 30 px.
void paintShieldSign(cairo_t* cr)
{
  roundedRect(cr, 2, 2, 92, 26, 5);
  linearFill(cr, 0, 2, 0, 28, rgb(40, 44, 56), rgb(20, 22, 30), true);
  outline(cr, 2.0);
  cairo_arc(cr, 15, 15, 6, 0, 2 * kPi);
  setColor(cr, rgb(120, 255, 140));
  cairo_fill(cr);
  cairo_arc(cr, 13, 13, 2, 0, 2 * kPi);
  setColor(cr, rgb(240, 255, 240));
  cairo_fill(cr);
  setColor(cr, rgb(120, 255, 140));
  centredText(cr, "SHIELDED", 56, 20, 11.5);
}

// --- Scenery --------------------------------------------------------------------------

// Thick lead glass, w x h: a heavy lead frame, bolted, around a deep window
// on the reactor hall: the core's blue light below, gantries in silhouette.
void paintWindow(cairo_t* cr, double w, double h)
{
  roundedRect(cr, 0, 0, w, h, 10);
  linearFill(cr, 0, 0, 0, h, kLeadLight, kLeadDark, true);
  outline(cr, 2.4);
  const double gx = 18, gy = 18, gw = w - 36, gh = h - 36;
  roundedRect(cr, gx - 4, gy - 4, gw + 8, gh + 8, 7);
  setColor(cr, rgb(36, 40, 50));
  cairo_fill(cr);
  cairo_save(cr);
  roundedRect(cr, gx, gy, gw, gh, 5);
  cairo_clip(cr);
  cairo_rectangle(cr, gx, gy, gw, gh);
  linearFill(cr, 0, gy, 0, gy + gh, rgb(10, 24, 50), rgb(30, 90, 170));
  // The core's light from below.
  cairo_pattern_t* p = cairo_pattern_create_radial(gx + gw * 0.5, gy + gh * 1.1, 4, gx + gw * 0.5, gy + gh * 1.1, gh * 1.1);
  cairo_pattern_add_color_stop_rgba(p, 0, 0.7, 0.9, 1.0, 0.8);
  cairo_pattern_add_color_stop_rgba(p, 1, 0.2, 0.5, 1.0, 0.0);
  cairo_set_source(cr, p);
  cairo_paint(cr);
  cairo_pattern_destroy(p);
  // A gantry far off.
  cairo_rectangle(cr, gx, gy + gh * 0.32, gw, 6);
  setRgba(cr, rgb(6, 12, 26), 0.8);
  cairo_fill(cr);
  for (double x = gx + 20; x < gx + gw; x += 60)
  {
    cairo_rectangle(cr, x, gy + gh * 0.32, 5, gh);
    cairo_fill(cr);
  }
  // Thick glass: a green tint and a bevel, and reflections.
  cairo_rectangle(cr, gx, gy, gw, gh);
  setRgba(cr, rgb(120, 200, 170), 0.12);
  cairo_fill(cr);
  for (double x = gx - 30; x < gx + gw; x += 110)
  {
    cairo_move_to(cr, x, gy + gh);
    cairo_line_to(cr, x + 22, gy + gh);
    cairo_line_to(cr, x + 22 + gh * 0.5, gy);
    cairo_line_to(cr, x + gh * 0.5, gy);
    cairo_close_path(cr);
    setRgba(cr, rgb(255, 255, 255), 0.12);
    cairo_fill(cr);
  }
  cairo_restore(cr);
  roundedRect(cr, gx, gy, gw, gh, 5);
  setRgba(cr, rgb(200, 240, 230), 0.5);
  cairo_set_line_width(cr, 3);
  cairo_stroke(cr);
  for (double x = 9; x < w; x += std::max(40.0, (w - 18) / std::max(1.0, std::floor((w - 18) / 64))))
    for (double y : {9.0, h - 9})
      bolt(cr, std::min(x, w - 9), y, 3.0);
  hazard(cr, w * 0.5 - 40, h - 14, 80, 8, 10);
  setColor(cr, rgb(30, 34, 44));
  centredText(cr, "LEAD GLASS 40CM", w * 0.5, 13, 9);
}

// The dosimeter, 120 x 132: a steel box with a round dial (its needle on 42
// in the amber), a red zone, a little readout that says 42, the trefoil.
void paintDosimeter(cairo_t* cr)
{
  roundedRect(cr, 4, 4, 112, 124, 10);
  linearFill(cr, 0, 4, 0, 128, rgb(226, 214, 120), rgb(186, 152, 40), true);
  outline(cr, 2.4);
  // The dial.
  const double cx = 60, cy = 62, rad = 40;
  cairo_arc(cr, cx, cy, rad + 5, 0, 2 * kPi);
  linearFill(cr, 0, cy - rad, 0, cy + rad, kSteelLight, kSteelDark, true);
  outline(cr, 2.0);
  cairo_arc(cr, cx, cy, rad, 0, 2 * kPi);
  setColor(cr, rgb(246, 244, 232));
  cairo_fill(cr);
  // Scale from 0 (at 225 degrees) to 100 (at -45), red past 70.
  auto angleOf = [](double v) { return (225.0 - 270.0 * v / 100.0) * kPi / 180.0; };
  cairo_arc_negative(cr, cx, cy, rad - 6, -angleOf(70), -angleOf(100));
  setColor(cr, rgb(230, 50, 40));
  cairo_set_line_width(cr, 6);
  cairo_stroke(cr);
  for (int v = 0; v <= 100; v += 10)
  {
    const double a = angleOf(v);
    const double r0 = v % 50 == 0 ? rad - 14 : rad - 9;
    cairo_move_to(cr, cx + std::cos(a) * r0, cy - std::sin(a) * r0);
    cairo_line_to(cr, cx + std::cos(a) * (rad - 2), cy - std::sin(a) * (rad - 2));
  }
  setColor(cr, kInk);
  cairo_set_line_width(cr, 1.8);
  cairo_stroke(cr);
  setColor(cr, kInk);
  trefoil(cr, cx, cy + 14, 8);
  setColor(cr, rgb(40, 40, 40));
  cairo_fill(cr);
  // The needle, on 42.
  const double a = angleOf(42);
  cairo_move_to(cr, cx - std::cos(a) * 8, cy + std::sin(a) * 8);
  cairo_line_to(cr, cx + std::cos(a) * (rad - 6), cy - std::sin(a) * (rad - 6));
  setColor(cr, rgb(200, 30, 30));
  cairo_set_line_width(cr, 3);
  cairo_stroke(cr);
  cairo_arc(cr, cx, cy, 4, 0, 2 * kPi);
  setColor(cr, kInk);
  cairo_fill(cr);
  // The readout.
  roundedRect(cr, 34, 106, 52, 18, 3);
  setColor(cr, rgb(30, 22, 10));
  cairo_fill(cr);
  setColor(cr, rgb(255, 170, 40));
  centredText(cr, "42", 60, 121, 15);
  setColor(cr, rgb(60, 46, 10));
  centredText(cr, "DOSE mSv", 60, 18, 9);
}

// A gantry crane over the core, w x h: an I-beam across the top on two
// wheeled trucks, a trolley with its drum, cables down to a striped hook
// block.
void paintCrane(cairo_t* cr, double w, double h)
{
  // The beam.
  cairo_rectangle(cr, 0, 6, w, 30);
  linearFill(cr, 0, 6, 0, 36, rgb(255, 214, 70), rgb(200, 140, 20), true);
  outline(cr, 2.4);
  cairo_rectangle(cr, 0, 6, w, 5);
  cairo_rectangle(cr, 0, 31, w, 5);
  setColor(cr, rgb(150, 100, 10));
  cairo_fill(cr);
  for (double x = 10; x < w - 10; x += 34)
  {
    cairo_move_to(cr, x, 31);
    cairo_line_to(cr, x + 17, 11);
    cairo_line_to(cr, x + 34, 31);
  }
  setColor(cr, rgb(150, 100, 10));
  cairo_set_line_width(cr, 3);
  cairo_stroke(cr);
  // The trolley.
  const double tx = w * 0.5;
  roundedRect(cr, tx - 34, 36, 68, 28, 5);
  linearFill(cr, 0, 36, 0, 64, kSteelLight, kSteelDark, true);
  outline(cr, 2.2);
  cairo_arc(cr, tx, 50, 9, 0, 2 * kPi);
  linearFill(cr, 0, 41, 0, 59, kSteel, kSteelDark, true);
  outline(cr, 1.6);
  // The cables and the hook block.
  const double by = std::max(90.0, h - 70);
  for (const double cx : {tx - 8.0, tx + 8.0})
  {
    cairo_move_to(cr, cx, 64);
    cairo_line_to(cr, cx, by);
  }
  setColor(cr, rgb(40, 42, 50));
  cairo_set_line_width(cr, 3);
  cairo_stroke(cr);
  roundedRect(cr, tx - 20, by, 40, 34, 5);
  setColor(cr, kYellow);
  cairo_fill_preserve(cr);
  outline(cr, 2.2);
  hazard(cr, tx - 18, by + 12, 36, 10, 10);
  cairo_arc(cr, tx, by + 50, 12, -kPi * 0.1, kPi * 1.2);
  setColor(cr, kInk);
  cairo_set_line_width(cr, 7);
  cairo_stroke(cr);
  cairo_arc(cr, tx, by + 50, 12, -kPi * 0.1, kPi * 1.2);
  setColor(cr, kSteel);
  cairo_set_line_width(cr, 3.5);
  cairo_stroke(cr);
}

// A coolant pipe along the ceiling, w px long, 64 tall: brackets up into
// the ceiling, flanges, green arrows and stains where it has been leaking.
void paintPipe(cairo_t* cr, double w)
{
  for (double x = 30; x < w; x += 128)
  {
    cairo_rectangle(cr, x - 4, 0, 8, 14);
    setColor(cr, kSteelDark);
    cairo_fill_preserve(cr);
    outline(cr, 1.4);
  }
  cairo_rectangle(cr, 0, 12, w, 34);
  linearFill(cr, 0, 12, 0, 46, rgb(196, 204, 216), rgb(74, 82, 98), true);
  outline(cr, 2.2);
  cairo_rectangle(cr, 0, 17, w, 4);
  setRgba(cr, rgb(255, 255, 255), 0.5);
  cairo_fill(cr);
  for (double x = 62; x < w; x += 128)
  {
    roundedRect(cr, x - 6, 8, 12, 42, 3);
    linearFill(cr, x - 6, 0, x + 6, 0, kSteelLight, kSteelDark, true);
    outline(cr, 1.6);
    for (const double y : {14.0, 44.0})
      bolt(cr, x, y, 2.0);
  }
  // Green band and flow arrows.
  for (double x = 96; x < w - 30; x += 256)
  {
    cairo_rectangle(cr, x, 12, 40, 34);
    setRgba(cr, rgb(80, 200, 60), 0.85);
    cairo_fill(cr);
    cairo_move_to(cr, x + 8, 22);
    cairo_line_to(cr, x + 26, 22);
    cairo_line_to(cr, x + 26, 16);
    cairo_line_to(cr, x + 36, 29);
    cairo_line_to(cr, x + 26, 42);
    cairo_line_to(cr, x + 26, 36);
    cairo_line_to(cr, x + 8, 36);
    cairo_close_path(cr);
    setColor(cr, rgb(240, 255, 230));
    cairo_fill(cr);
  }
  // Stains of old leaks.
  for (double x = 180; x < w; x += 300)
  {
    cairo_move_to(cr, x, 46);
    cairo_curve_to(cr, x + 4, 52, x - 3, 58, x + 2, 64);
    setRgba(cr, kCoolant, 0.45);
    cairo_set_line_width(cr, 4);
    cairo_stroke(cr);
  }
}

// A stencilled sign, w x h, its text in black on yellow, chevrons at the
// ends.
void paintSign(cairo_t* cr, double w, double h, const std::string& text)
{
  roundedRect(cr, 2, 2, w - 4, h - 4, 4);
  linearFill(cr, 0, 2, 0, h - 2, rgb(255, 222, 80), rgb(226, 170, 30), true);
  outline(cr, 2.0);
  hazard(cr, 6, 6, 18, h - 12, 12);
  hazard(cr, w - 24, 6, 18, h - 12, 12);
  setColor(cr, rgb(24, 22, 20));
  selectGameFont(cr);
  double size = h * 0.5;
  cairo_set_font_size(cr, size);
  cairo_text_extents_t e;
  cairo_text_extents(cr, text.c_str(), &e);
  if (e.width > w - 64)
    size *= (w - 64) / e.width;
  centredText(cr, text.c_str(), w * 0.5, h * 0.5 + size * 0.36, size);
}

// --- Machines -------------------------------------------------------------------------

// A valve wheel, 60 x 60, the middle at (30, 30): a rim on five spokes.
void paintValveWheel(cairo_t* cr, bool shut)
{
  const Color light = shut ? rgb(140, 255, 150) : rgb(255, 120, 100);
  const Color dark = shut ? rgb(30, 140, 60) : rgb(170, 20, 24);
  for (int k = 0; k < 5; ++k)
  {
    const double a = k * kPi * 2.0 / 5.0;
    cairo_move_to(cr, 30, 30);
    cairo_line_to(cr, 30 + std::cos(a) * 22, 30 + std::sin(a) * 22);
  }
  setColor(cr, kInk);
  cairo_set_line_width(cr, 7);
  cairo_stroke(cr);
  for (int k = 0; k < 5; ++k)
  {
    const double a = k * kPi * 2.0 / 5.0;
    cairo_move_to(cr, 30, 30);
    cairo_line_to(cr, 30 + std::cos(a) * 22, 30 + std::sin(a) * 22);
  }
  setColor(cr, dark);
  cairo_set_line_width(cr, 3.5);
  cairo_stroke(cr);
  cairo_arc(cr, 30, 30, 24, 0, 2 * kPi);
  setColor(cr, kInk);
  cairo_set_line_width(cr, 9);
  cairo_stroke(cr);
  cairo_arc(cr, 30, 30, 24, 0, 2 * kPi);
  setColor(cr, dark);
  cairo_set_line_width(cr, 5.5);
  cairo_stroke(cr);
  cairo_arc(cr, 30, 30, 24, kPi * 1.1, kPi * 1.6);
  setColor(cr, light);
  cairo_set_line_width(cr, 2.5);
  cairo_stroke(cr);
  // Grips round the rim.
  for (int k = 0; k < 10; ++k)
  {
    const double a = k * kPi / 5.0 + 0.3;
    cairo_arc(cr, 30 + std::cos(a) * 24, 30 + std::sin(a) * 24, 2.2, 0, 2 * kPi);
    setColor(cr, darken(dark, 0.3f));
    cairo_fill(cr);
  }
  cairo_arc(cr, 30, 30, 7, 0, 2 * kPi);
  linearFill(cr, 0, 23, 0, 37, kSteelLight, kSteelDark, true);
  outline(cr, 1.6);
}

// The valve's stand, 80 x 110, standing on (40, 110): a pipe out of the
// floor turning into the wall, a flange, a pressure gauge.
void paintValveStand(cairo_t* cr)
{
  // The pipe up out of the floor and into the wall.
  cairo_rectangle(cr, 30, 40, 20, 70);
  linearFill(cr, 30, 0, 50, 0, kSteelLight, kSteelDark, true);
  outline(cr, 2.0);
  cairo_rectangle(cr, 20, 96, 40, 14);
  linearFill(cr, 0, 96, 0, 110, kSteel, kSteelDark, true);
  outline(cr, 2.0);
  roundedRect(cr, 22, 34, 36, 12, 3);
  linearFill(cr, 0, 34, 0, 46, kSteelLight, kSteel, true);
  outline(cr, 1.8);
  for (const double x : {27.0, 53.0})
    bolt(cr, x, 40, 2.0);
  // The gauge.
  cairo_arc(cr, 64, 70, 11, 0, 2 * kPi);
  linearFill(cr, 0, 59, 0, 81, kSteelLight, kSteelDark, true);
  outline(cr, 1.6);
  cairo_arc(cr, 64, 70, 8, 0, 2 * kPi);
  setColor(cr, rgb(246, 244, 236));
  cairo_fill(cr);
  cairo_arc(cr, 64, 70, 6, kPi * 1.75, kPi * 2.25);
  setColor(cr, rgb(230, 50, 40));
  cairo_set_line_width(cr, 2.5);
  cairo_stroke(cr);
  cairo_rectangle(cr, 50, 68, 6, 4);
  setColor(cr, kSteelDark);
  cairo_fill(cr);
}

// The DO NOT PRESS console, 120 x 96, standing on (60, 96): a sloped desk
// with a few lamps and a yellow-ringed housing for the big button, and the
// label.
void paintConsole(cairo_t* cr)
{
  // The desk.
  cairo_move_to(cr, 10, 96);
  cairo_line_to(cr, 10, 44);
  cairo_line_to(cr, 24, 24);
  cairo_line_to(cr, 110, 24);
  cairo_line_to(cr, 110, 96);
  cairo_close_path(cr);
  linearFill(cr, 0, 24, 0, 96, rgb(90, 98, 116), rgb(40, 44, 56), true);
  outline(cr, 2.4);
  cairo_move_to(cr, 10, 44);
  cairo_line_to(cr, 24, 24);
  cairo_line_to(cr, 110, 24);
  cairo_line_to(cr, 110, 44);
  cairo_close_path(cr);
  linearFill(cr, 0, 24, 0, 44, rgb(150, 158, 176), rgb(90, 98, 116), true);
  outline(cr, 2.0);
  // Lamps.
  const Color lamps[] = {rgb(90, 255, 140), rgb(255, 200, 60), rgb(90, 200, 255), rgb(90, 255, 140)};
  for (int k = 0; k < 4; ++k)
  {
    cairo_arc(cr, 28 + k * 10, 36, 3, 0, 2 * kPi);
    setColor(cr, lamps[k]);
    cairo_fill(cr);
  }
  // The button's housing.
  cairo_save(cr);
  cairo_translate(cr, 86, 30);
  cairo_scale(cr, 1.0, 0.4);
  cairo_arc(cr, 0, 0, 20, 0, 2 * kPi);
  cairo_restore(cr);
  hazard(cr, 64, 20, 44, 18, 8);
  cairo_save(cr);
  cairo_translate(cr, 86, 30);
  cairo_scale(cr, 1.0, 0.4);
  cairo_arc(cr, 0, 0, 20, 0, 2 * kPi);
  cairo_restore(cr);
  setColor(cr, kInk);
  cairo_set_line_width(cr, 2);
  cairo_stroke(cr);
  // The label.
  roundedRect(cr, 18, 56, 84, 22, 3);
  setColor(cr, rgb(240, 236, 226));
  cairo_fill_preserve(cr);
  outline(cr, 1.8);
  setColor(cr, rgb(200, 20, 20));
  centredText(cr, "DO NOT PRESS", 60, 71.5, 9.5);
  for (const double x : {20.0, 100.0})
    bolt(cr, x, 88, 2.4);
}

// The big red button, 40 x 30, its base middle at (20, 26); pressed: down
// and lit.
void paintButton(cairo_t* cr, bool pressed)
{
  const double top = pressed ? 16 : 6;
  cairo_new_sub_path(cr);
  cairo_arc(cr, 20, top + 12, 15, kPi, 2 * kPi);
  cairo_line_to(cr, 35, 26);
  cairo_line_to(cr, 5, 26);
  cairo_close_path(cr);
  cairo_pattern_t* g = cairo_pattern_create_radial(15, top + 4, 1, 20, top + 12, 18);
  cairo_pattern_add_color_stop_rgb(g, 0, 1.0, pressed ? 0.85 : 0.65, pressed ? 0.75 : 0.55);
  cairo_pattern_add_color_stop_rgb(g, 1, pressed ? 1.0 : 0.75, pressed ? 0.25 : 0.06, pressed ? 0.2 : 0.06);
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  outline(cr, 2.0);
  cairo_arc(cr, 15, top + 6, 3.5, 0, 2 * kPi);
  setRgba(cr, rgb(255, 255, 255), 0.8);
  cairo_fill(cr);
}

// A cassette, 48 x 32, the middle at (24, 16), without its reels' spokes.
void paintCassette(cairo_t* cr)
{
  roundedRect(cr, 2, 2, 44, 28, 3);
  linearFill(cr, 0, 2, 0, 30, rgb(60, 62, 72), rgb(26, 28, 36), true);
  outline(cr, 1.8);
  roundedRect(cr, 7, 6, 34, 9, 2);
  setColor(cr, rgb(250, 236, 200));
  cairo_fill(cr);
  setColor(cr, rgb(200, 30, 30));
  centredText(cr, "VICTORY", 24, 13, 6);
  roundedRect(cr, 10, 17, 28, 10, 4);
  setColor(cr, rgb(16, 16, 20));
  cairo_fill(cr);
  for (const double x : {16.0, 32.0})
  {
    cairo_arc(cr, x, 22, 4, 0, 2 * kPi);
    setColor(cr, rgb(236, 236, 240));
    cairo_fill(cr);
  }
}

// The lift cage's header, w x 40: a striped beam, the LIFT plate.
void paintCageHeader(cairo_t* cr, double w)
{
  cairo_rectangle(cr, 0, 2, w, 34);
  linearFill(cr, 0, 2, 0, 36, kSteelLight, kSteelDark, true);
  outline(cr, 2.2);
  hazard(cr, 4, 24, w - 8, 9, 14);
  roundedRect(cr, w * 0.5 - 34, 5, 68, 17, 3);
  setColor(cr, rgb(24, 26, 34));
  cairo_fill(cr);
  setColor(cr, kYellow);
  centredText(cr, "LIFT", w * 0.5, 19, 13);
  for (double x = 10; x < w; x += 48)
    bolt(cr, x, 12, 2.4);
}

// A cage post, 16 x h.
void paintCagePost(cairo_t* cr, double h)
{
  cairo_rectangle(cr, 2, 0, 12, h);
  linearFill(cr, 2, 0, 14, 0, kSteelLight, kSteelDark, true);
  outline(cr, 1.8);
  for (double y = 20; y < h; y += 40)
    bolt(cr, 8, y, 2.0);
}

// --- The Bracer ------------------------------------------------------------------------

// The raised shield, 64 x h, a lens of blue energy filled with a hex grid,
// its rim brightest on the outside (right).
void paintShield(cairo_t* cr, double h)
{
  const double w = 64, cx = 30, cy = h * 0.5, half = h * 0.5 - 4;
  auto lens = [&]() {
    cairo_new_path(cr);
    cairo_move_to(cr, cx, cy - half);
    cairo_curve_to(cr, cx + 34, cy - half * 0.6, cx + 34, cy + half * 0.6, cx, cy + half);
    cairo_curve_to(cr, cx - 18, cy + half * 0.6, cx - 18, cy - half * 0.6, cx, cy - half);
    cairo_close_path(cr);
  };
  lens();
  cairo_pattern_t* g = cairo_pattern_create_linear(cx - 18, 0, cx + 30, 0);
  cairo_pattern_add_color_stop_rgba(g, 0, 0.3, 0.6, 1.0, 0.15);
  cairo_pattern_add_color_stop_rgba(g, 1, 0.55, 0.85, 1.0, 0.55);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
  cairo_save(cr);
  lens();
  cairo_clip(cr);
  // The hex grid.
  const double hs = 9;
  for (int row = -1; row < int(h / (hs * 1.5)) + 2; ++row)
    for (int col = -1; col < int(w / (hs * 1.732)) + 2; ++col)
    {
      const double hx = col * hs * 1.732 + (row % 2 ? hs * 0.866 : 0), hy = row * hs * 1.5;
      cairo_new_sub_path(cr);
      for (int k = 0; k < 6; ++k)
      {
        const double a = kPi / 6.0 + k * kPi / 3.0;
        const double px = hx + std::cos(a) * hs * 0.92, py = hy + std::sin(a) * hs * 0.92;
        if (k == 0)
          cairo_move_to(cr, px, py);
        else
          cairo_line_to(cr, px, py);
      }
      cairo_close_path(cr);
    }
  setRgba(cr, rgb(190, 235, 255), 0.55);
  cairo_set_line_width(cr, 1.3);
  cairo_stroke(cr);
  cairo_restore(cr);
  // The rim.
  cairo_new_path(cr);
  cairo_move_to(cr, cx, cy - half);
  cairo_curve_to(cr, cx + 34, cy - half * 0.6, cx + 34, cy + half * 0.6, cx, cy + half);
  setRgba(cr, rgb(120, 200, 255), 0.7);
  cairo_set_line_width(cr, 7);
  cairo_stroke_preserve(cr);
  setRgba(cr, rgb(240, 252, 255), 1.0);
  cairo_set_line_width(cr, 2.5);
  cairo_stroke(cr);
  cairo_move_to(cr, cx, cy + half);
  cairo_curve_to(cr, cx - 18, cy + half * 0.6, cx - 18, cy - half * 0.6, cx, cy - half);
  setRgba(cr, rgb(150, 215, 255), 0.5);
  cairo_set_line_width(cr, 2);
  cairo_stroke(cr);
}

// The thrown-back pulse: a crescent of blue energy, 140 x 420, its middle
// at (70, 210), bulging right (the way it travels).
void paintArc(cairo_t* cr)
{
  const double cy = 210, half = 196;
  for (int pass = 0; pass < 3; ++pass)
  {
    const double thick = pass == 0 ? 46 : (pass == 1 ? 26 : 10);
    cairo_new_path(cr);
    cairo_move_to(cr, 40, cy - half);
    cairo_curve_to(cr, 40 + 90, cy - half * 0.55, 40 + 90, cy + half * 0.55, 40, cy + half);
    cairo_curve_to(cr, 40 + 90 - thick, cy + half * 0.5, 40 + 90 - thick, cy - half * 0.5, 40, cy - half);
    cairo_close_path(cr);
    if (pass == 0)
      setRgba(cr, rgb(60, 140, 255), 0.35);
    else if (pass == 1)
      setRgba(cr, rgb(130, 200, 255), 0.6);
    else
      setRgba(cr, rgb(240, 250, 255), 0.95);
    cairo_fill(cr);
  }
}

// A Shield Drone's bubble, 560 x 560, the middle at (280, 280): a faint
// sphere with a bright rim, a few hexes and a highlight.
void paintBubble(cairo_t* cr)
{
  const double c = 280, rad = 256;
  cairo_arc(cr, c, c, rad, 0, 2 * kPi);
  cairo_pattern_t* p = cairo_pattern_create_radial(c, c, rad * 0.4, c, c, rad);
  cairo_pattern_add_color_stop_rgba(p, 0, 0.3, 0.6, 1.0, 0.0);
  cairo_pattern_add_color_stop_rgba(p, 0.8, 0.35, 0.7, 1.0, 0.12);
  cairo_pattern_add_color_stop_rgba(p, 1, 0.6, 0.85, 1.0, 0.45);
  cairo_set_source(cr, p);
  cairo_fill(cr);
  cairo_pattern_destroy(p);
  cairo_arc(cr, c, c, rad - 2, 0, 2 * kPi);
  setRgba(cr, rgb(200, 240, 255), 0.75);
  cairo_set_line_width(cr, 3);
  cairo_stroke(cr);
  // Hexes round the rim.
  for (int k = 0; k < 18; ++k)
  {
    const double a = k * kPi / 9.0, rr = rad * 0.86;
    const double hx = c + std::cos(a) * rr, hy = c + std::sin(a) * rr;
    cairo_new_sub_path(cr);
    for (int s = 0; s < 6; ++s)
    {
      const double b = a + s * kPi / 3.0;
      if (s == 0)
        cairo_move_to(cr, hx + std::cos(b) * 20, hy + std::sin(b) * 20);
      else
        cairo_line_to(cr, hx + std::cos(b) * 20, hy + std::sin(b) * 20);
    }
    cairo_close_path(cr);
  }
  setRgba(cr, rgb(170, 225, 255), 0.25);
  cairo_set_line_width(cr, 1.5);
  cairo_stroke(cr);
  // The highlight.
  cairo_save(cr);
  cairo_translate(cr, c - rad * 0.45, c - rad * 0.55);
  cairo_rotate(cr, -0.6);
  cairo_scale(cr, rad * 0.32, rad * 0.12);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_restore(cr);
  setRgba(cr, rgb(255, 255, 255), 0.3);
  cairo_fill(cr);
}

// A soft white frame, 1280 x 720: clear in the middle, brighter toward the
// edges (tinted when drawn: the hum's blue pulse, the film's dark edges).
void paintEdges(cairo_t* cr)
{
  const double W = kScreenW, H = kScreenH;
  cairo_pattern_t* p = cairo_pattern_create_radial(W * 0.5, H * 0.5, H * 0.35, W * 0.5, H * 0.5, W * 0.62);
  cairo_pattern_add_color_stop_rgba(p, 0, 1, 1, 1, 0.0);
  cairo_pattern_add_color_stop_rgba(p, 0.7, 1, 1, 1, 0.35);
  cairo_pattern_add_color_stop_rgba(p, 1, 1, 1, 1, 1.0);
  cairo_rectangle(cr, 0, 0, W, H);
  cairo_set_source(cr, p);
  cairo_fill(cr);
  cairo_pattern_destroy(p);
}

// The pulse counter's badge, 64 x 64, the middle at (32, 32): a trefoil on
// a round plate.
void paintBadge(cairo_t* cr, Color plate, Color mark)
{
  cairo_arc(cr, 32, 32, 29, 0, 2 * kPi);
  cairo_pattern_t* g = cairo_pattern_create_radial(26, 24, 2, 32, 32, 30);
  cairo_pattern_add_color_stop_rgb(g, 0, std::min(1.0, redOf(plate) / 255.0 + 0.2), std::min(1.0, greenOf(plate) / 255.0 + 0.2),
    std::min(1.0, blueOf(plate) / 255.0 + 0.2));
  cairo_pattern_add_color_stop_rgb(g, 1, redOf(plate) / 255.0 * 0.7, greenOf(plate) / 255.0 * 0.7, blueOf(plate) / 255.0 * 0.7);
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  outline(cr, 3.0, rgb(10, 8, 20));
  trefoil(cr, 32, 32, 22);
  setColor(cr, mark);
  cairo_fill(cr);
}

// The Stop Motion icon, 44 x 44, the middle at (22, 22): a film reel.
void paintReel(cairo_t* cr)
{
  cairo_arc(cr, 22, 22, 19, 0, 2 * kPi);
  setColor(cr, rgb(230, 220, 196));
  cairo_fill_preserve(cr);
  outline(cr, 2.4, rgb(30, 24, 16));
  for (int k = 0; k < 5; ++k)
  {
    const double a = -kPi * 0.5 + k * kPi * 2.0 / 5.0;
    cairo_arc(cr, 22 + std::cos(a) * 10, 22 + std::sin(a) * 10, 4.6, 0, 2 * kPi);
    setColor(cr, rgb(60, 46, 30));
    cairo_fill(cr);
  }
  cairo_arc(cr, 22, 22, 3, 0, 2 * kPi);
  setColor(cr, rgb(60, 46, 30));
  cairo_fill(cr);
}

// A ring of `rad` px round (cx, cy), drawn as short lines, skipping the
// pulse's gap arcs and anything off screen.
void drawRing(Renderer& r, const CorePulse* c, float cx, float cy, float rad, float width, Color col, int alpha)
{
  if (rad <= 1.0f || alpha <= 0)
    return;
  // Off screen altogether: the screen wholly inside the ring, or the ring
  // wholly off the screen.
  const float nx = std::clamp(cx, 0.0f, float(kScreenW)), ny = std::clamp(cy, 0.0f, float(kScreenH));
  const float nearest = std::hypot(nx - cx, ny - cy);
  const float fx = std::max(std::abs(cx), std::abs(cx - float(kScreenW)));
  const float fy = std::max(std::abs(cy), std::abs(cy - float(kScreenH)));
  const float farthest = std::hypot(fx, fy);
  if (nearest > rad + width || farthest < rad - width)
    return;
  const int n = std::clamp(int(rad * 6.2832f / 18.0f), 32, 2400);
  const float step = 6.2832f / float(n);
  for (int i = 0; i < n; ++i)
  {
    const float a0 = float(i) * step, a1 = a0 + step * 1.08f;
    const float x0 = cx + std::cos(a0) * rad, y0 = cy + std::sin(a0) * rad;
    if (x0 < -width - 24.0f || x0 > float(kScreenW) + width + 24.0f || y0 < -width - 24.0f ||
        y0 > float(kScreenH) + width + 24.0f)
      continue;
    if (c && !c->gaps.empty())
    {
      float deg = (a0 + step * 0.5f) * 57.29578f;
      bool gap = false;
      for (const auto& [g0, g1] : c->gaps)
        if (g0 <= g1 ? (deg >= float(g0) && deg <= float(g1)) : (deg >= float(g0) || deg <= float(g1)))
          gap = true;
      if (gap)
        continue;
    }
    r.drawLine(x0, y0, cx + std::cos(a1) * rad, cy + std::sin(a1) * rad, width, withAlpha(col, alpha), Blend::Add);
  }
}

} // namespace

// --- Back: scenery, machines, wires ----------------------------------------------------

void World::drawReactorBack(Renderer& r, float camX, float camY, int frame) const
{
  const auto& rs = mReactor;
  const float hum = pulseHum();
  const float flash = float(rs.flash) / float(kPulseFlash);
  const float dim = rs.stopped < 0 ? 0.0f : std::min(1.0f, float(rs.stopped) / float(kPulseWindDown));
  auto empty = [&](int bx, int by) {
    return bx >= 0 && by >= 0 && bx < mLevel->width && by < mLevel->height && mMap.block(bx, by) == Tile::Empty;
  };

  // The core: the glowing column on the back wall, block by block where
  // nothing is in front of it; its glow swells with the hum.
  for (const auto& d : rs.decos)
  {
    if (d.kind != "core")
      continue;
    const int wpx = (d.x1 - d.x0 + 1) * int(kTilePx);
    const float x = float(d.x0) * kTilePx - camX, y = float(d.y0) * kTilePx - camY;
    const float w = float(wpx), h = float(d.y1 - d.y0 + 1) * kTilePx;
    if (!visible(x - 80.0f, y - 80.0f, w + 160.0f, h + 160.0f))
      continue;
    const int cols = d.x1 - d.x0 + 1;
    const int by0 = std::max(d.y0, int(camY / kTilePx) - 1), by1 = std::min(d.y1, int((camY + float(kScreenH)) / kTilePx) + 1);
    for (int by = by0; by <= by1; ++by)
      for (int i = 0; i < cols; ++i)
      {
        const int bx = d.x0 + i;
        if (!empty(bx, by))
          continue;
        const int phase = (by - d.y0) % 4;
        const std::string key = "core/" + std::to_string(cols) + "/" + std::to_string(i) + "/" + std::to_string(phase);
        const Texture& tex = baked(mArt, r, key, int(kTilePx), int(kTilePx), 0.0f, 0.0f, [&](cairo_t* cr) {
          cairo_translate(cr, -double(i) * kTilePx, -double(phase) * kTilePx);
          paintCoreSlice(cr, double(wpx));
        });
        const float bxp = float(bx) * kTilePx - camX, byp = float(by) * kTilePx - camY;
        r.draw(tex, bxp, byp);
        if (dim > 0.0f)
          r.fillRect(bxp, byp, kTilePx, kTilePx, rgba(4, 10, 34, int(190.0f * dim)));
      }
    // Its light: brighter in the hum and the flash, dark once powered down.
    const float cx = x + w * 0.5f;
    const float power = (1.0f - dim) * (0.3f + 0.08f * std::sin(float(frame) * 0.07f) + 0.6f * hum + 0.4f * flash);
    for (int by = by0; by <= by1; by += 2)
    {
      const float gy = (float(by) + 0.5f) * kTilePx - camY;
      if (gy < y || gy > y + h)
        continue;
      drawGlow(r, mArt, cx, gy, w * (0.7f + 0.4f * hum), kCore, 0.18f * power);
      drawGlow(r, mArt, cx, gy, w * 0.16f, kCoreWhite, 0.22f * power);
    }
    // Energy rising up the tube.
    if (dim < 1.0f)
      for (int k = 0; k < 10; ++k)
      {
        const float speed = 4.0f + 6.0f * hum;
        const float travel = std::fmod(float(frame) * speed + float(k) * 397.0f, h);
        const float py = y + h - travel, px = cx + std::sin(float(k) * 1.7f + float(frame) * 0.02f) * w * 0.12f;
        if (!empty(int((px + camX) / kTilePx), int((py + camY) / kTilePx)))
          continue;
        drawGlow(r, mArt, px, py, 26.0f + 10.0f * hum, kCoreWhite, (0.55f + 0.4f * hum) * (1.0f - dim));
      }
    // The cap and the base.
    const Texture& cap = baked(mArt, r, "corecap/" + std::to_string(wpx), wpx + 40, 56, float(wpx + 40) * 0.5f, 0.0f,
      [&](cairo_t* cr) { paintCoreCap(cr, double(wpx)); });
    r.draw(cap, cx, y - 16.0f);
    const Texture& base = baked(mArt, r, "corebase/" + std::to_string(wpx), wpx + 60, 70, float(wpx + 60) * 0.5f, 70.0f,
      [&](cairo_t* cr) { paintCoreBase(cr, double(wpx)); });
    r.draw(base, cx, y + h);
    if (dim < 1.0f)
      drawGlow(r, mArt, cx, y + h - 20.0f, w * 0.6f, kCore, (0.4f + 0.6f * hum + flash) * (1.0f - dim));
  }

  // Lead booths: the back wall, the posts at each open side, the green
  // SHIELDED lamp.
  for (const auto& b : rs.booths)
  {
    const float x = float(b.x0) * kTilePx - camX, y = float(b.y0) * kTilePx - camY;
    const float w = float(b.x1 - b.x0 + 1) * kTilePx, h = float(b.y1 - b.y0 + 1) * kTilePx;
    if (!visible(x, y, w, h))
      continue;
    const int rows = b.y1 - b.y0 + 1;
    const int bx0 = std::max(b.x0, int(camX / kTilePx) - 1), bx1 = std::min(b.x1, int((camX + float(kScreenW)) / kTilePx) + 1);
    for (int by = b.y0; by <= b.y1; ++by)
    {
      const int row = by - b.y0;
      const int kind = row == 0 ? 0 : (row == rows - 1 ? 3 : (row == rows - 2 ? 2 : 1));
      const Texture& tex = baked(mArt, r, "booth/" + std::to_string(kind), int(kTilePx), int(kTilePx), 0.0f, 0.0f,
        [kind](cairo_t* cr) { paintBoothBlock(cr, kind); });
      for (int bx = bx0; bx <= bx1; ++bx)
        if (empty(bx, by))
          r.draw(tex, float(bx) * kTilePx - camX, float(by) * kTilePx - camY);
    }
    const Texture& post = baked(mArt, r, "boothpost/" + std::to_string(rows), 22, rows * int(kTilePx), 0.0f, 0.0f,
      [h](cairo_t* cr) { paintBoothPost(cr, double(h)); });
    r.draw(post, x, y);
    r.draw(post, x + w - 22.0f, y);
    const Texture& sign = baked(mArt, r, "shieldsign", 96, 30, 48.0f, 0.0f, paintShieldSign);
    const int signs = std::max(1, (b.x1 - b.x0 + 1) / 14);
    for (int k = 0; k < signs; ++k)
    {
      const float sx = x + w * (float(k) + 0.5f) / float(signs);
      r.draw(sign, sx, y + 8.0f);
      drawGlow(r, mArt, sx - 33.0f, y + 23.0f, 18.0f, rgb(110, 255, 140), 0.7f + 0.2f * std::sin(float(frame) * 0.1f));
    }
    // The ring's flash glances off the lead.
    if (flash > 0.0f)
      r.fillRect(x, y, w, h, rgba(160, 200, 255, int(26.0f * flash)), Blend::Add);
  }

  // Checkpoint beacons standing in front of a booth or the core (the back
  // wall went over them).
  for (const auto& cp : mCheckpoints)
  {
    const int bx = cp.x / kCellsPerTile, by = cp.y / kCellsPerTile;
    bool covered = false;
    for (const auto& b : rs.booths)
      covered = covered || (bx >= b.x0 && bx <= b.x1 && by >= b.y0 && by <= b.y1);
    for (const auto& d : rs.decos)
      covered = covered || (d.kind == "core" && bx >= d.x0 && bx <= d.x1 && by >= d.y0 && by <= d.y1);
    if (!covered)
      continue;
    const float x = float(cp.x) * kCellPx - camX, y = float(cp.y - 3) * kCellPx - camY;
    if (cp.active)
    {
      drawGlow(r, mArt, x + 32, y + 26, 70 + 10 * std::sin(float(frame) * 0.15f), mTheme.accentB, 0.6f);
      r.draw(mArt.beaconOn, x, y);
    }
    else
      r.draw(mArt.beaconOff, x, y);
  }

  // Scenery.
  for (const auto& d : rs.decos)
  {
    const float x = float(d.x0) * kTilePx - camX, y = float(d.y0) * kTilePx - camY;
    const int wpx = (d.x1 - d.x0 + 1) * int(kTilePx), hpx = (d.y1 - d.y0 + 1) * int(kTilePx);
    const float w = float(wpx), h = float(hpx);
    if (!visible(x - 128.0f, y - 128.0f, w + 256.0f, h + 256.0f))
      continue;
    if (d.kind == "window")
    {
      const Texture& tex = baked(mArt, r, "window/" + std::to_string(wpx) + "x" + std::to_string(hpx), wpx, hpx, 0.0f,
        0.0f, [&](cairo_t* cr) { paintWindow(cr, w, h); });
      r.draw(tex, x, y);
      // The ring seen through the glass: the window lights up.
      if (flash > 0.0f || hum > 0.0f)
        r.fillRect(x + 18.0f, y + 18.0f, w - 36.0f, h - 36.0f,
          rgba(150, 210, 255, int(150.0f * flash + 40.0f * hum * (0.6f + 0.4f * std::sin(float(frame) * 0.6f)))), Blend::Add);
    }
    else if (d.kind == "dosimeter" || d.kind == "42")
    {
      const Texture& tex = baked(mArt, r, "dosimeter", 120, 132, 60.0f, 66.0f, paintDosimeter);
      const float cx = x + w * 0.5f, cy = y + h * 0.5f;
      r.draw(tex, cx, cy);
      if ((frame / 20) % 2 == 0)
        drawGlow(r, mArt, cx, cy + 49.0f, 26.0f, rgb(255, 170, 40), 0.35f);
    }
    else if (d.kind == "crane")
    {
      const int ch = std::max(hpx, 2 * int(kTilePx));
      const Texture& tex = baked(mArt, r, "crane/" + std::to_string(wpx) + "x" + std::to_string(ch), wpx, ch, 0.0f, 0.0f,
        [&](cairo_t* cr) { paintCrane(cr, w, double(ch)); });
      DrawOpts o;
      if (flash > 0.0f)
        o.tint = lerpColor(rgb(255, 255, 255), rgb(200, 230, 255), flash);
      r.draw(tex, x, y, o);
      // The warning lamp on the trolley.
      if ((frame / 15) % 2 == 0)
        drawGlow(r, mArt, x + w * 0.5f, y + 40.0f, 16.0f, rgb(255, 120, 40), 0.8f);
    }
    else if (d.kind == "pipe")
    {
      const Texture& tex = baked(mArt, r, "pipe/" + std::to_string(wpx), wpx, 64, 0.0f, 0.0f,
        [&](cairo_t* cr) { paintPipe(cr, w); });
      r.draw(tex, x, y);
    }
    else if (d.kind == "sign")
    {
      const int sw = std::max(wpx, 3 * int(kTilePx)), sh = std::min(hpx, 56);
      const Texture& tex = baked(mArt, r, "sign/" + std::to_string(sw) + "x" + std::to_string(sh) + "/" + d.text, sw, sh,
        float(sw) * 0.5f, float(sh) * 0.5f, [&](cairo_t* cr) { paintSign(cr, double(sw), double(sh), d.text); });
      r.draw(tex, x + w * 0.5f, y + h * 0.5f);
    }
  }

  // The wires: thick insulated cable on clamps, glowing white ahead of the
  // spark that rides it.
  for (std::size_t wi = 0; wi < rs.wires.size(); ++wi)
  {
    const auto& wire = rs.wires[wi];
    for (std::size_t i = 1; i < wire.path.size(); ++i)
    {
      const float ax = float(wire.path[i - 1].first + 1) * kCellPx - camX, ay = float(wire.path[i - 1].second) * kCellPx - camY;
      const float bx = float(wire.path[i].first + 1) * kCellPx - camX, by = float(wire.path[i].second) * kCellPx - camY;
      if (!visible(std::min(ax, bx) - 20.0f, std::min(ay, by) - 20.0f, std::abs(bx - ax) + 40.0f, std::abs(by - ay) + 40.0f))
        continue;
      const bool horiz = std::abs(bx - ax) >= std::abs(by - ay);
      r.drawLine(ax, ay, bx, by, 15.0f, kInk);
      r.fillRect(ax - 7.5f, ay - 7.5f, 15.0f, 15.0f, kInk);
      r.fillRect(bx - 7.5f, by - 7.5f, 15.0f, 15.0f, kInk);
      r.drawLine(ax, ay, bx, by, 10.0f, rgb(46, 50, 62));
      r.fillRect(bx - 5.0f, by - 5.0f, 10.0f, 10.0f, rgb(46, 50, 62));
      if (horiz)
        r.drawLine(ax, ay - 2.5f, bx, by - 2.5f, 2.0f, rgb(112, 120, 140));
      else
        r.drawLine(ax - 2.5f, ay, bx - 2.5f, by, 2.0f, rgb(112, 120, 140));
      // Clamps every block, a yellow HV tag every fourth.
      const float len = std::hypot(bx - ax, by - ay);
      const float ux = (bx - ax) / std::max(1.0f, len), uy = (by - ay) / std::max(1.0f, len);
      int n = 0;
      for (float t = 32.0f; t < len - 16.0f; t += 64.0f, ++n)
      {
        const float px = ax + ux * t, py = ay + uy * t;
        if (horiz)
          r.fillRect(px - 4.0f, py - 11.0f, 8.0f, 22.0f, rgb(140, 148, 166));
        else
          r.fillRect(px - 11.0f, py - 4.0f, 22.0f, 8.0f, rgb(140, 148, 166));
        r.fillRect(px - 1.5f, py - 1.5f, 3.0f, 3.0f, kInk);
        if (n % 4 == 2)
        {
          if (horiz)
            r.fillRect(px + 14.0f, py - 6.0f, 14.0f, 12.0f, kYellow);
          else
            r.fillRect(px - 6.0f, py + 14.0f, 12.0f, 14.0f, kYellow);
        }
      }
    }
    // The glow running 8 frames (12 cells) ahead of its spark.
    for (const auto& sr : rs.sparks)
    {
      if (sr.wire != int(wi) || sr.enemy < 0 || !mEnemies[std::size_t(sr.enemy)].alive)
        continue;
      auto ahead = [&](float k, float& px, float& py) {
        float s = sr.s + float(sr.dir) * k;
        if (!wire.loop)
        {
          if (s > wire.len)
            s = 2.0f * wire.len - s;
          if (s < 0.0f)
            s = -s;
        }
        float cx = 0.0f, cy = 0.0f;
        wirePoint(wire, s, cx, cy);
        px = (cx + 1.0f) * kCellPx - camX;
        py = cy * kCellPx - camY;
      };
      constexpr int kAhead = 12;
      float px0 = 0.0f, py0 = 0.0f;
      ahead(0.0f, px0, py0);
      const float flick = 0.85f + 0.15f * std::sin(float(frame) * 1.3f);
      for (int k = 1; k <= kAhead; ++k)
      {
        float px1 = 0.0f, py1 = 0.0f;
        ahead(float(k), px1, py1);
        const float t = 1.0f - float(k - 1) / float(kAhead);
        r.drawLine(px0, py0, px1, py1, 14.0f, rgba(90, 170, 255, int(110.0f * t * flick)), Blend::Add);
        r.drawLine(px0, py0, px1, py1, 5.0f, rgba(240, 250, 255, int(235.0f * t * flick)), Blend::Add);
        px0 = px1;
        py0 = py1;
      }
      drawGlow(r, mArt, px0, py0, 18.0f, rgb(200, 235, 255), 0.4f * flick);
    }
  }

  // The valves: a red wheel on a pipe stand, spinning as it is turned and
  // green once shut; a ring round it fills while up is held.
  for (const auto& v : rs.valves)
  {
    const float cx = (float(v.x) + 0.5f) * kTilePx - camX, fy = float(v.y + 1) * kTilePx - camY;
    if (!visible(cx - 60.0f, fy - 130.0f, 120.0f, 140.0f))
      continue;
    const Texture& stand = baked(mArt, r, "valvestand", 80, 110, 40.0f, 110.0f, paintValveStand);
    r.draw(stand, cx, fy);
    const Texture& wheel = baked(mArt, r, v.shut ? "wheel1" : "wheel0", 60, 60, 30.0f, 30.0f,
      [shut = v.shut](cairo_t* cr) { paintValveWheel(cr, shut); });
    DrawOpts o;
    o.angle = v.shut ? 36.0f : float(v.held) * 14.0f;
    const float wy = fy - 76.0f;
    r.draw(wheel, cx, wy, o);
    if (v.shut)
      drawGlow(r, mArt, cx, wy, 40.0f, rgb(90, 255, 120), 0.25f + 0.1f * std::sin(float(frame) * 0.08f));
    else if (v.held > 0)
    {
      const float t = float(v.held) / float(kValveHold);
      const int segs = int(32.0f * t);
      for (int k = 0; k < segs; ++k)
      {
        const float a0 = -1.5708f + float(k) * 0.19635f, a1 = a0 + 0.19635f;
        r.drawLine(cx + std::cos(a0) * 38.0f, wy + std::sin(a0) * 38.0f, cx + std::cos(a1) * 38.0f, wy + std::sin(a1) * 38.0f,
          5.0f, rgba(255, 220, 90, 230), Blend::Add);
      }
    }
    else
    {
      // A blinking hint arrow while the runner is close.
      const CellBox pb = mPlayer.box();
      const float dx = (float(pb.x) + float(pb.w) * 0.5f) * kCellPx - camX - cx;
      if (std::abs(dx) < 160.0f && std::abs(float(pb.y + pb.h) * kCellPx - camY - fy) < 80.0f && (frame / 12) % 2 == 0)
      {
        r.drawLine(cx, wy - 40.0f, cx, wy - 62.0f, 5.0f, rgba(255, 220, 90, 220));
        r.drawLine(cx - 10.0f, wy - 52.0f, cx, wy - 63.0f, 5.0f, rgba(255, 220, 90, 220));
        r.drawLine(cx + 10.0f, wy - 52.0f, cx, wy - 63.0f, 5.0f, rgba(255, 220, 90, 220));
      }
    }
  }

  // The lift cage: its frame, and the bars (the solid column while it is
  // shut), sliding up into the header once the core is down.
  if (rs.cageX0 >= 0)
  {
    const float x = float(rs.cageX0) * kTilePx - camX, y = float(rs.cageY0) * kTilePx - camY;
    const float w = float(rs.cageX1 - rs.cageX0 + 1) * kTilePx, h = float(rs.cageY1 - rs.cageY0 + 1) * kTilePx;
    if (visible(x - 40.0f, y - 60.0f, w + 80.0f, h + 80.0f))
    {
      // Rails up the back of the shaft.
      for (const float rx : {x + 26.0f, x + w - kTilePx - 34.0f})
      {
        r.fillRect(rx, y, 8.0f, h, rgba(30, 34, 46, 200));
        r.fillRect(rx + 2.0f, y, 2.0f, h, rgba(150, 160, 180, 120));
      }
      float open = rs.cageOpen ? 1.0f : 0.0f;
      if (rs.cageOpen && rs.stopped >= 0)
        open = ease(float(rs.stopped - kPulseWindDown / 2) / 30.0f);
      const float gx = x + w - kTilePx;
      if (!rs.cageOpen)
      {
        // The gate in front of the dark shaft (over the solid tiles).
        for (int by = rs.cageY0; by <= rs.cageY1; ++by)
          r.fillRect(gx, float(by) * kTilePx - camY, kTilePx, kTilePx, rgb(18, 22, 32));
      }
      const float barLen = h * (1.0f - open);
      if (barLen > 2.0f)
      {
        for (int k = 0; k < 4; ++k)
        {
          const float bx = gx + 8.0f + float(k) * 16.0f;
          r.drawLine(bx, y, bx, y + barLen, 9.0f, kInk);
          r.drawLine(bx, y, bx, y + barLen, 5.0f, rgb(150, 160, 178));
          r.drawLine(bx - 1.5f, y, bx - 1.5f, y + barLen, 1.5f, rgb(230, 236, 246));
        }
        for (float cy = y + 40.0f; cy < y + barLen - 10.0f; cy += 96.0f)
        {
          r.fillRect(gx + 2.0f, cy, kTilePx - 4.0f, 8.0f, kInk);
          r.fillRect(gx + 3.0f, cy + 1.5f, kTilePx - 6.0f, 5.0f, rgb(130, 140, 160));
        }
        r.fillRect(gx + 2.0f, y + barLen - 10.0f, kTilePx - 4.0f, 10.0f, kInk);
        r.fillRect(gx + 4.0f, y + barLen - 8.0f, kTilePx - 8.0f, 6.0f, kYellow);
      }
      const Texture& post = baked(mArt, r, "cagepost/" + std::to_string(int(h)), 16, int(h), 8.0f, 0.0f,
        [h](cairo_t* cr) { paintCagePost(cr, double(h)); });
      r.draw(post, x + 2.0f, y);
      r.draw(post, x + w + 2.0f, y);
      const Texture& head = baked(mArt, r, "cagehead/" + std::to_string(int(w)), int(w) + 24, 40, 12.0f, 40.0f,
        [w](cairo_t* cr) { paintCageHeader(cr, double(w) + 24.0); });
      r.draw(head, x, y + 6.0f);
      const Color lamp = rs.cageOpen ? rgb(90, 255, 120) : rgb(255, 60, 50);
      drawGlow(r, mArt, x + w * 0.5f + 52.0f, y - 20.0f, 16.0f, lamp, rs.cageOpen || (frame / 15) % 2 ? 0.9f : 0.4f);
      drawGlow(r, mArt, x + w * 0.5f - 52.0f, y - 20.0f, 16.0f, lamp, rs.cageOpen || (frame / 15) % 2 ? 0.9f : 0.4f);
    }
  }

  // The unlit ladder: only there while a ring's flash lights it.
  if (rs.unlitX0 >= 0 && rs.flash > 0)
  {
    DrawOpts o;
    o.alpha = std::min(1.0f, flash * 1.4f);
    for (int ty = rs.unlitY0; ty <= rs.unlitY1; ++ty)
      for (int tx = rs.unlitX0; tx <= rs.unlitX1; ++tx)
      {
        const float x = float(tx) * kTilePx - camX, y = float(ty) * kTilePx - camY;
        if (!visible(x, y, kTilePx, kTilePx))
          continue;
        const Tile t = mMap.block(tx, ty);
        if (t == Tile::Ladder)
          r.draw(mArt.ladder, x, y, o);
        else if (t == Tile::Pipe)
          r.draw(mArt.pipe, x, y, o);
      }
  }

  // DO NOT PRESS: after a press the button stays down and lit and a little
  // cassette spins its reels the wrong way.
  if (rs.consoleX >= 0)
  {
    // It stands on the floor under its block.
    int foot = rs.consoleY;
    while (foot + 1 < mLevel->height && foot < rs.consoleY + 3 && mMap.block(rs.consoleX, foot + 1) != Tile::Solid)
      ++foot;
    const float cx = (float(rs.consoleX) + 0.5f) * kTilePx - camX, fy = float(foot + 1) * kTilePx - camY;
    if (visible(cx - 80.0f, fy - 200.0f, 160.0f, 210.0f))
    {
      const Texture& desk = baked(mArt, r, "console", 120, 96, 60.0f, 96.0f, paintConsole);
      r.draw(desk, cx, fy);
      const bool pressed = rs.fanfare > 0;
      const Texture& button = baked(mArt, r, pressed ? "button1" : "button0", 40, 30, 20.0f, 26.0f,
        [pressed](cairo_t* cr) { paintButton(cr, pressed); });
      r.draw(button, cx + 26.0f, fy - 62.0f);
      if (pressed)
      {
        drawGlow(r, mArt, cx + 26.0f, fy - 72.0f, 40.0f, rgb(255, 80, 60), 0.6f + 0.3f * float((frame / 4) % 2));
        const float t = std::min(1.0f, float(90 - rs.fanfare) / 8.0f);
        const float ky = fy - 110.0f - 30.0f * ease(t) + 4.0f * std::sin(float(frame) * 0.12f);
        const Texture& tape = baked(mArt, r, "cassette", 48, 32, 24.0f, 16.0f, paintCassette);
        DrawOpts o;
        o.alpha = std::min(1.0f, float(rs.fanfare) / 10.0f);
        o.scale = 1.4f;
        r.draw(tape, cx, ky, o);
        // The reels turning backwards.
        for (const float rx : {-11.2f, 11.2f})
          for (int k = 0; k < 3; ++k)
          {
            const float a = -float(frame) * 0.35f + float(k) * 2.0944f;
            r.drawLine(cx + rx, ky + 8.4f, cx + rx + std::cos(a) * 5.0f, ky + 8.4f + std::sin(a) * 5.0f, 2.0f,
              withAlpha(rgb(40, 40, 50), int(255.0f * o.alpha)));
          }
        // Notes drifting down into it (it plays backwards).
        for (int k = 0; k < 3; ++k)
        {
          const float ph = std::fmod(float(frame) * 0.02f + float(k) / 3.0f, 1.0f);
          r.drawText(k % 2 ? "\xE2\x99\xAA" : "\xE2\x99\xAB", cx - 40.0f + float(k) * 36.0f + std::sin(ph * 6.0f) * 8.0f,
            ky - 110.0f + ph * 90.0f, {26.0f, rgb(140, 210, 255), kInk}, Align::Center, (1.0f - ph) * o.alpha);
        }
      }
    }
  }
}

// --- Front: rings, drops, bubbles, the Bracer ---------------------------------------------

void World::drawReactorFront(Renderer& r, float camX, float camY, int frame) const
{
  const auto& rs = mReactor;
  const float alpha = rs.stopMotion && !rs.moving ? 1.0f : reactorRenderAlpha();
  const float flash = float(rs.flash) / float(kPulseFlash);
  const float hum = pulseHum();

  // The coolant drip: a drop swelling under the pipe, then falling.
  for (const auto& d : rs.drips)
  {
    const float x = (float(d.x) + 0.5f) * kCellPx - camX, y = float(d.y) * kCellPx - camY;
    if (!visible(x - 40.0f, y - 40.0f, 80.0f, 900.0f))
      continue;
    // The crack it leaks from.
    drawGlow(r, mArt, x, y - 4.0f, 22.0f, kCoolant, 0.35f + 0.15f * std::sin(float(frame) * 0.2f));
    if (d.t < 8)
    {
      const float s = (float(d.t) + alpha) / 8.0f;
      const float rad = 3.0f + 7.0f * s;
      drawGlow(r, mArt, x, y + rad, 14.0f + 14.0f * s, kCoolant, 0.5f);
      r.fillRect(x - rad * 0.55f, y, rad * 1.1f, rad * 1.2f, rgb(110, 230, 60));
      r.fillRect(x - rad * 0.8f, y + rad * 0.6f, rad * 1.6f, rad * 1.2f, rgb(130, 245, 70));
      r.fillRect(x - rad * 0.4f, y + rad * 0.8f, rad * 0.35f, rad * 0.5f, rgb(230, 255, 210));
    }
    for (const float dy : d.drops)
    {
      const float by = (dy + alpha) * kCellPx - camY; // the bottom of its bottom row, between frames
      drawGlow(r, mArt, x, by - 14.0f, 26.0f, kCoolant, 0.55f);
      r.drawLine(x, by - 34.0f, x, by - 14.0f, 4.0f, rgba(140, 255, 90, 120));
      r.fillRect(x - 5.0f, by - 22.0f, 10.0f, 16.0f, rgb(120, 240, 60));
      r.fillRect(x - 3.0f, by - 28.0f, 6.0f, 8.0f, rgb(120, 240, 60));
      r.fillRect(x - 3.0f, by - 19.0f, 3.0f, 6.0f, rgb(235, 255, 220));
    }
  }

  // Shield Drones' bubbles: a faint sphere round each live drone, flaring
  // and rippling when it takes a hit for its group.
  for (const auto& g : rs.drones)
  {
    if (g.enemy < 0)
      continue;
    const Enemy& e = mEnemies[std::size_t(g.enemy)];
    if (!e.alive || e.hidden)
      continue;
    const float cx = (e.drawX + float(e.w) * 0.5f) * kCellPx - camX;
    const float cy = (e.drawY + 1.0f - float(e.h) * 0.5f) * kCellPx - camY;
    const float rad = float(kBubble) * kCellPx;
    if (!visible(cx - rad, cy - rad, rad * 2.0f, rad * 2.0f))
      continue;
    const Texture& bubble = baked(mArt, r, "bubble", 560, 560, 280.0f, 280.0f, paintBubble);
    const float sh = float(g.shimmer) / 8.0f;
    DrawOpts o;
    o.blend = Blend::Add;
    o.alpha = std::min(1.0f, 0.5f + 0.08f * std::sin(float(frame) * 0.09f) + 0.5f * sh);
    o.scale = 1.0f + 0.015f * std::sin(float(frame) * 0.13f) + 0.03f * sh;
    o.angle = float(frame % 720) * 0.5f;
    r.draw(bubble, cx, cy, o);
    // A shimmer running round its rim.
    for (int k = 0; k < 3; ++k)
    {
      const float a = float(frame) * 0.05f + float(k) * 2.0944f;
      drawGlow(r, mArt, cx + std::cos(a) * rad * 0.98f, cy + std::sin(a) * rad * 0.98f, 30.0f, rgb(170, 225, 255), 0.35f);
    }
    if (g.shimmer > 0)
    {
      drawRing(r, nullptr, cx, cy, rad * (1.0f - 0.04f * sh), 10.0f, rgb(150, 220, 255), int(200.0f * sh));
      drawRing(r, nullptr, cx, cy, rad * (0.92f + 0.08f * (1.0f - sh)), 4.0f, rgb(240, 250, 255), int(220.0f * sh));
    }
  }

  // The Bracer's thrown-back arcs.
  for (const auto& a : rs.arcs)
  {
    const float x = (a.x - float(a.dir) * 3.0f * (1.0f - alpha)) * kCellPx - camX, y = a.y * kCellPx - camY;
    if (!visible(x - 140.0f, y - 220.0f, 280.0f, 440.0f))
      continue;
    const Texture& tex = baked(mArt, r, "arc", 140, 420, 70.0f, 210.0f, paintArc);
    const float fade = 1.0f - std::clamp(float(a.life) / 17.0f, 0.0f, 1.0f) * 0.7f;
    DrawOpts o;
    o.blend = Blend::Add;
    o.alpha = fade;
    if (a.dir < 0)
      o.angle = 180.0f;
    for (int k = 2; k >= 1; --k)
    {
      DrawOpts t = o;
      t.alpha = fade * (0.35f - 0.12f * float(k));
      r.draw(tex, x - float(a.dir * k) * 34.0f, y, t);
    }
    r.draw(tex, x, y, o);
    drawGlow(r, mArt, x + float(a.dir) * 20.0f, y, 130.0f, kCore, 0.3f * fade);
  }

  // The Deflector Bracer's shield: a lens of hex-patterned blue energy held
  // up in front of the runner, a ripple where a shot bounced off it, a big
  // flare where it soaked a pulse.
  const auto& p = mPlayer;
  const float ox = (float(p.prevX) - float(p.x)) * (1.0f - alpha), oy = (float(p.prevY) - float(p.y)) * (1.0f - alpha);
  if (bracerUp() && !p.hidden)
  {
    const CellBox sb = bracerBox();
    const float sx = (float(sb.x) + ox + 1.0f) * kCellPx - camX;
    const float sy = (float(sb.y) + oy + float(sb.h) * 0.5f) * kCellPx - camY;
    const int hpx = sb.h * int(kCellPx) + 24;
    const Texture& tex = baked(mArt, r, "shield/" + std::to_string(hpx), 64, hpx, 30.0f, float(hpx) * 0.5f,
      [hpx](cairo_t* cr) { paintShield(cr, double(hpx)); });
    const float grow = ease(float(rs.raise) / 3.0f);
    DrawOpts o;
    o.blend = Blend::Add;
    o.alpha = (0.75f + 0.2f * std::sin(float(frame) * 0.9f)) * (rs.raise > kBracerRaise - 8 && (frame / 2) % 2 ? 0.5f : 1.0f);
    o.scale = 0.55f + 0.45f * grow;
    if (p.facing < 0)
      o.angle = 180.0f;
    drawGlow(r, mArt, sx, sy, 70.0f, kCore, 0.35f * grow);
    r.draw(tex, sx, sy, o);
    // The emitter on the forearm.
    const CellBox b = p.box();
    const float hx = (p.facing > 0 ? float(b.x + b.w) - 0.6f : float(b.x) + 0.6f) + ox;
    drawGlow(r, mArt, hx * kCellPx - camX, (float(b.y) + oy + float(b.h) * 0.48f) * kCellPx - camY, 16.0f,
      rgb(200, 240, 255), 0.8f);
  }
  if (rs.reflect > 0)
  {
    const CellBox sb = bracerBox();
    const float sx = (float(sb.x) + ox + 1.0f) * kCellPx - camX, sy = (float(sb.y) + oy + float(sb.h) * 0.5f) * kCellPx - camY;
    const float t = 1.0f - float(rs.reflect) / 6.0f;
    drawGlow(r, mArt, sx, sy, 60.0f + 40.0f * t, rgb(220, 245, 255), 0.8f * (1.0f - t));
    drawRing(r, nullptr, sx, sy, 16.0f + 60.0f * t, 5.0f, rgb(200, 240, 255), int(230.0f * (1.0f - t)));
  }
  if (rs.soakFlash > 0)
  {
    const CellBox b = p.box();
    const float cx = (float(b.x) + ox + float(b.w) * 0.5f + float(p.facing) * 2.0f) * kCellPx - camX;
    const float cy = (float(b.y) + oy + float(b.h) * 0.5f) * kCellPx - camY;
    const float t = 1.0f - float(rs.soakFlash) / 10.0f;
    drawGlow(r, mArt, cx, cy, 160.0f + 160.0f * t, kCoreWhite, 0.9f * (1.0f - t));
    drawRing(r, nullptr, cx, cy, 30.0f + 180.0f * t, 12.0f, rgb(120, 200, 255), int(220.0f * (1.0f - t)));
  }
  if (rs.soakCool > 0 && p.weapon == Weapon::Proto && !p.hidden)
  {
    // Recharging: a small arc over the runner's head fills back up.
    const CellBox b = p.box();
    const float cx = (float(b.x) + ox + float(b.w) * 0.5f) * kCellPx - camX, cy = (float(b.y) + oy) * kCellPx - camY - 26.0f;
    const float t = 1.0f - float(rs.soakCool) / float(kBracerSoak);
    const int segs = int(24.0f * t);
    for (int k = 0; k < 24; ++k)
    {
      const float a0 = -1.5708f + float(k) * 0.2618f, a1 = a0 + 0.2618f;
      r.drawLine(cx + std::cos(a0) * 12.0f, cy + std::sin(a0) * 12.0f, cx + std::cos(a1) * 12.0f, cy + std::sin(a1) * 12.0f,
        3.0f, k < segs ? rgba(130, 210, 255, 220) : rgba(60, 80, 110, 120), k < segs ? Blend::Add : Blend::Alpha);
    }
  }

  // The pulse rings: a thick glowing ring of blue-white racing out from the
  // core, a fading wake behind it, its gaps (the bonus) left open.
  for (const auto& c : rs.pulses)
  {
    const float cx = c.x * kCellPx - camX, cy = c.y * kCellPx - camY;
    for (const auto& ring : c.rings)
    {
      const float rad = std::max(0.0f, ring.r - c.speed * (1.0f - alpha)) * kCellPx;
      const float spacing = std::clamp(c.speed * kCellPx * 0.12f, 10.0f, 60.0f);
      for (int k = 3; k >= 1; --k)
        drawRing(r, &c, cx, cy, rad - spacing * float(k), 18.0f - float(k) * 3.0f, rgb(70, 150, 255), 70 - k * 18);
      drawRing(r, &c, cx, cy, rad, 44.0f, rgb(60, 140, 255), 70);
      drawRing(r, &c, cx, cy, rad, 18.0f, rgb(140, 205, 255), 170);
      drawRing(r, &c, cx, cy, rad, 6.0f, rgb(240, 250, 255), 255);
    }
  }

  // The hum: the screen's edges breathe blue as the core winds up.
  if (hum > 0.0f && rs.stopped < 0)
  {
    const Texture& edges = baked(mArt, r, "edges", kScreenW, kScreenH, 0.0f, 0.0f, paintEdges);
    DrawOpts o;
    o.blend = Blend::Add;
    o.tint = kCore;
    o.cull = false;
    o.alpha = hum * (0.55f + 0.3f * std::sin(float(frame) * (0.25f + 0.5f * hum)));
    r.draw(edges, 0.0f, 0.0f, o);
  }
  // The flash: the whole level lit blue-white for a moment.
  if (flash > 0.0f)
    r.fillRect(0.0f, 0.0f, float(kScreenW), float(kScreenH), rgba(150, 200, 255, int(96.0f * flash * flash)), Blend::Add);
}

// --- HUD: the pulse counter, Stop Motion -------------------------------------------------

void World::drawReactorHud(Renderer& r, int frame) const
{
  const auto& rs = mReactor;
  // Stop Motion: while the world stands still the screen turns to old film:
  // a sepia wash, dark edges, grain and the odd scratch.
  if (rs.stopMotion && !rs.moving)
  {
    r.fillRect(0.0f, 0.0f, float(kScreenW), float(kScreenH), rgba(112, 84, 46, 92));
    const Texture& edges = baked(mArt, r, "edges", kScreenW, kScreenH, 0.0f, 0.0f, paintEdges);
    DrawOpts o;
    o.tint = rgb(30, 20, 8);
    o.cull = false;
    o.alpha = 0.55f;
    r.draw(edges, 0.0f, 0.0f, o);
    const int shot = frame / 5; // grain holds for a few frames, like film
    for (int k = 0; k < 90; ++k)
    {
      const std::uint32_t h = hash2(shot * 131 + k, k * 7 + 3);
      const float gx = float(h % std::uint32_t(kScreenW)), gy = float((h >> 11) % std::uint32_t(kScreenH));
      const float sz = 1.5f + float((h >> 21) % 3u);
      r.fillRect(gx, gy, sz, sz, (h >> 25) % 2u ? rgba(255, 244, 220, 90) : rgba(30, 20, 10, 120));
    }
    for (int k = 0; k < 2; ++k)
    {
      const std::uint32_t h = hash2(shot, 977 + k);
      if (h % 3u == 0u)
        continue;
      const float sx = float(h % std::uint32_t(kScreenW));
      r.drawLine(sx, 0.0f, sx + float(int((h >> 12) % 9u) - 4), float(kScreenH), 1.5f, rgba(255, 240, 210, 70));
    }
    if (hash2(shot, 5) % 4u == 0u)
      r.fillRect(0.0f, 0.0f, float(kScreenW), float(kScreenH), rgba(0, 0, 0, 22));
  }

  // The pulse counter: a radiation badge and the seconds to the next ring,
  // red in the last three; dark once the core is down.
  const float w = 168.0f, hgt = 64.0f;
  const float x = float(kScreenW) - 12.0f - w, y = 10.0f + float(kHudPanelH) + 8.0f;
  if (!rs.pulses.empty())
  {
    const int count = pulseCountdown();
    const bool dark = count < 0;
    const bool hurry = !dark && count <= 3;
    const bool blink = hurry && (frame / 6) % 2 == 0;
    r.fillRect(x, y, w, hgt, rgba(8, 6, 22, 180));
    r.fillRect(x, y, w, 2.0f, dark ? rgba(90, 96, 110, 170) : (hurry ? rgba(255, 70, 50, 220) : withAlpha(mTheme.accentA, 190)));
    const Texture& badge = dark ? baked(mArt, r, "badge_dark", 64, 64, 32.0f, 32.0f, [](cairo_t* cr) { paintBadge(cr, rgb(70, 74, 86), rgb(30, 32, 40)); })
                                : (blink ? baked(mArt, r, "badge_red", 64, 64, 32.0f, 32.0f,
                                             [](cairo_t* cr) { paintBadge(cr, rgb(255, 70, 50), rgb(40, 8, 8)); })
                                         : baked(mArt, r, "badge", 64, 64, 32.0f, 32.0f,
                                             [](cairo_t* cr) { paintBadge(cr, kYellow, rgb(24, 20, 16)); }));
    const float bx = x + 36.0f, bcy = y + hgt * 0.5f;
    DrawOpts bo;
    bo.scale = 0.84f;
    if (!dark)
    {
      const float hum = pulseHum();
      bo.scale += 0.08f * hum * (0.5f + 0.5f * std::sin(float(frame) * 0.8f));
      if (hurry)
        drawGlow(r, mArt, bx, bcy, 44.0f, rgb(255, 60, 40), blink ? 0.7f : 0.3f);
      else if (hum > 0.0f)
        drawGlow(r, mArt, bx, bcy, 44.0f, kCore, 0.5f * hum);
    }
    bo.angle = dark ? 0.0f : float(frame % 360) * 0.25f;
    r.draw(badge, bx, bcy, bo);
    r.drawText("PULSE", x + 74.0f, y + 6.0f, {14.0f, dark ? rgb(110, 114, 126) : rgb(190, 188, 214), rgb(10, 8, 20)});
    if (dark)
      r.drawText("OFF", x + 74.0f, y + 24.0f, {30.0f, rgb(110, 114, 126), rgb(10, 8, 20)});
    else
    {
      const Color c = hurry ? (blink ? rgb(255, 90, 70) : rgb(255, 170, 150)) : mTheme.hudText;
      r.drawText(std::to_string(count), x + 74.0f, y + 20.0f, {38.0f, c, rgb(10, 8, 20)});
    }
  }

  // Stop Motion's film reel: turning while the world moves, still (and the
  // word PAUSED blinking) while it stands.
  if (rs.stopMotion)
  {
    const float ry = rs.pulses.empty() ? y : y + hgt + 8.0f;
    r.fillRect(x, ry, w, 44.0f, rgba(20, 14, 8, 190));
    r.fillRect(x, ry, w, 2.0f, rgba(230, 200, 150, 200));
    const Texture& reel = baked(mArt, r, "reel", 44, 44, 22.0f, 22.0f, paintReel);
    static int spin = 0;
    static int lastFrame = 0;
    if (rs.moving && frame != lastFrame)
      spin += 6;
    lastFrame = frame;
    DrawOpts o;
    o.angle = float(spin % 360);
    o.scale = 0.8f;
    r.draw(reel, x + 26.0f, ry + 22.0f, o);
    r.drawText("STOP MOTION", x + 50.0f, ry + 6.0f, {15.0f, rgb(240, 222, 186), rgb(10, 8, 20)});
    if (!rs.moving && (frame / 15) % 2 == 0)
      r.drawText("FROZEN", x + 50.0f, ry + 23.0f, {13.0f, rgb(255, 180, 120), rgb(10, 8, 20)});
    else if (rs.moving)
      r.drawText("ROLLING", x + 50.0f, ry + 23.0f, {13.0f, rgb(170, 230, 170), rgb(10, 8, 20)});
  }
}

} // namespace gr
