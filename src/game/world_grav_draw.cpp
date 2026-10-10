// Level 19's drawing (world_grav.cpp has the rules): the test chambers'
// lights and stencils, the steel lips under their ceilings, the orange wall
// arrows that turn with their chamber, the switch panels, the test gates,
// the walls that are not there, the scenery (the STATION MANAGER mug on its
// desk, the chains a ledge hangs from, the observation window, CHAMBER 42),
// the Grav Grenade and its vortex, the runner's dizzy stars and the gravity
// badge. The pieces are baked with Cairo the first time they are drawn and
// kept in the Art's sprite cache.

#include "game/world.hpp"

#include "assets/art.hpp"
#include "base/math.hpp"
#include "render/vector.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
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
constexpr float kTilePx = kCellPx * float(kCellsPerTile);  // 64
constexpr Color kInk = rgb(18, 20, 32);
constexpr Color kOrange = rgb(255, 136, 36);
constexpr Color kOrangeLight = rgb(255, 200, 120);
constexpr Color kCharcoal = rgb(44, 46, 56);
constexpr Color kSteel = rgb(176, 186, 202);
constexpr Color kSteelLight = rgb(240, 244, 250);
constexpr Color kSteelDark = rgb(70, 78, 96);
constexpr Color kVortex = rgb(170, 80, 255);

bool visible(float x, float y, float w, float h)
{
  return x + w > -96.0f && x < float(kScreenW) + 96.0f && y + h > -96.0f && y < float(kScreenH) + 96.0f;
}

// A texture baked once and kept in the Art's sprite cache under `key`.
const Texture& baked(const Art& art, const Renderer& r, const std::string& key, int w, int h, float ax, float ay,
  const std::function<void(cairo_t*)>& paint)
{
  const std::string id = "~grav/" + key;
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
  cairo_pattern_add_color_stop_rgb(p, 0, 1, 1, 1);
  cairo_pattern_add_color_stop_rgb(p, 1, 0.45, 0.5, 0.6);
  cairo_set_source(cr, p);
  cairo_fill(cr);
  cairo_pattern_destroy(p);
}

// Orange and charcoal hazard stripes filling a rect.
void hazard(cairo_t* cr, double x, double y, double w, double h, double step)
{
  cairo_save(cr);
  cairo_rectangle(cr, x, y, w, h);
  cairo_clip(cr);
  setColor(cr, kOrange);
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

// --- Wall signage --------------------------------------------------------------

// The backing plate an arrow turns on: a white enamel disc on a charcoal
// square, bolted at its corners. 64 x 64, the middle at (32, 32).
void paintArrowPlate(cairo_t* cr)
{
  roundedRect(cr, 3, 3, 58, 58, 8);
  linearFill(cr, 0, 3, 0, 61, rgb(70, 74, 88), rgb(36, 38, 48), true);
  outline(cr, 2.0);
  cairo_arc(cr, 32, 32, 24, 0, 2 * kPi);
  linearFill(cr, 0, 8, 0, 56, rgb(255, 255, 255), rgb(206, 212, 224), true);
  outline(cr, 1.6, rgb(30, 32, 40));
  for (double x : {9.0, 55.0})
    for (double y : {9.0, 55.0})
      bolt(cr, x, y, 2.4);
}

// The arrow itself, pointing down, its middle at (24, 24) of 48 x 48.
void paintArrow(cairo_t* cr)
{
  cairo_move_to(cr, 17, 5);
  cairo_line_to(cr, 31, 5);
  cairo_line_to(cr, 31, 24);
  cairo_line_to(cr, 41, 24);
  cairo_line_to(cr, 24, 43);
  cairo_line_to(cr, 7, 24);
  cairo_line_to(cr, 17, 24);
  cairo_close_path(cr);
  linearFill(cr, 7, 0, 41, 0, kOrangeLight, rgb(236, 100, 20), true);
  outline(cr, 2.2);
  cairo_move_to(cr, 19, 8);
  cairo_line_to(cr, 19, 24);
  setRgba(cr, rgb(255, 255, 255), 0.7);
  cairo_set_line_width(cr, 2.0);
  cairo_stroke(cr);
}

// A switch's wall panel: a steel box with a slot the lever swings in, a
// stencilled GRAVITY and a status lamp housing, 56 x 104, the pivot at
// (28, 56).
void paintSwitchPanel(cairo_t* cr)
{
  roundedRect(cr, 2, 2, 52, 100, 7);
  linearFill(cr, 0, 2, 0, 102, kSteelLight, kSteel, true);
  outline(cr, 2.2);
  hazard(cr, 6, 6, 44, 8, 10);
  cairo_rectangle(cr, 6, 6, 44, 8);
  outline(cr, 1.2);
  // The lever's slot.
  roundedRect(cr, 22, 22, 12, 68, 6);
  setColor(cr, rgb(30, 32, 40));
  cairo_fill(cr);
  // Up and down marks either side of the slot.
  for (int k = -1; k <= 1; k += 2)
  {
    const double y = 56 + k * 26;
    cairo_move_to(cr, 10, y - k * 5);
    cairo_line_to(cr, 15, y + k * 1);
    cairo_line_to(cr, 20, y - k * 5);
    setColor(cr, kSteelDark);
    cairo_set_line_width(cr, 2.0);
    cairo_stroke(cr);
  }
  setColor(cr, kSteelDark);
  centredText(cr, "GRAV", 28, 100, 9);
  for (double x : {8.0, 48.0})
    bolt(cr, x, 94, 2.2);
}

// The lever: a red-knobbed handle on its pivot, pointing down, the pivot
// at (12, 12) of 24 x 56.
void paintLever(cairo_t* cr)
{
  roundedRect(cr, 9, 10, 6, 34, 3);
  linearFill(cr, 9, 0, 15, 0, kSteelLight, kSteelDark, true);
  outline(cr, 1.6);
  cairo_arc(cr, 12, 46, 8, 0, 2 * kPi);
  cairo_pattern_t* p = cairo_pattern_create_radial(9, 43, 1, 12, 46, 8);
  cairo_pattern_add_color_stop_rgb(p, 0, 1.0, 0.75, 0.6);
  cairo_pattern_add_color_stop_rgb(p, 1, 0.8, 0.12, 0.1);
  cairo_set_source(cr, p);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(p);
  outline(cr, 1.8);
  cairo_arc(cr, 12, 12, 7, 0, 2 * kPi);
  linearFill(cr, 0, 5, 0, 19, kSteelLight, kSteelDark, true);
  outline(cr, 1.6);
  cairo_arc(cr, 12, 12, 2.2, 0, 2 * kPi);
  setColor(cr, kInk);
  cairo_fill(cr);
}

// --- Test gate -------------------------------------------------------------------

// A gate's slab, `w` x `full` px, of which the bottom `vis` px are drawn
// (the rest has slid up into the ceiling): brushed white steel, a narrow
// window, the hazard band along its foot.
void paintGate(cairo_t* cr, double w, double full, double vis)
{
  cairo_translate(cr, 0, vis - full);
  roundedRect(cr, 4, 0, w - 8, full, 4);
  linearFill(cr, 4, 0, w - 4, 0, rgb(250, 252, 255), rgb(188, 198, 214), true);
  outline(cr, 2.2);
  for (double y = 36; y < full - 30; y += 48)
  {
    cairo_rectangle(cr, 10, y, w - 20, 2);
    setColor(cr, rgba(90, 100, 120, 110));
    cairo_fill(cr);
  }
  // The window.
  roundedRect(cr, w * 0.5 - 7, full * 0.18, 14, full * 0.3, 5);
  linearFill(cr, 0, full * 0.18, 0, full * 0.48, rgb(140, 210, 240), rgb(40, 80, 110), true);
  outline(cr, 1.6);
  cairo_move_to(cr, w * 0.5 - 3, full * 0.2 + 4);
  cairo_line_to(cr, w * 0.5 - 3, full * 0.3);
  setRgba(cr, rgb(255, 255, 255), 0.8);
  cairo_set_line_width(cr, 2.0);
  cairo_stroke(cr);
  hazard(cr, 6, full - 22, w - 12, 18, 12);
  cairo_rectangle(cr, 6, full - 22, w - 12, 18);
  outline(cr, 1.4);
  for (double y : {12.0, full * 0.62})
    for (double x : {12.0, w - 12})
      bolt(cr, x, y, 2.4);
}

// --- Scenery -----------------------------------------------------------------------

// A lab desk, `w` px wide, standing on y = h, with a mug (the STATION
// MANAGER's, which never falls) or a monitor and papers.
void paintDesk(cairo_t* cr, double w, double h, bool mug)
{
  const double top = h - 60;
  // Legs and a modesty panel.
  for (double x : {10.0, w - 22})
  {
    cairo_rectangle(cr, x, top + 8, 12, 52);
    linearFill(cr, x, 0, x + 12, 0, kSteel, kSteelDark, true);
    outline(cr, 1.8);
  }
  cairo_rectangle(cr, 22, top + 10, w - 44, 26);
  linearFill(cr, 0, top + 10, 0, top + 36, rgb(214, 220, 232), rgb(160, 170, 188), true);
  outline(cr, 1.6);
  // A drawer with a handle.
  roundedRect(cr, w - 60, top + 14, 30, 18, 3);
  linearFill(cr, 0, top + 14, 0, top + 32, kSteelLight, kSteel, true);
  outline(cr, 1.4);
  cairo_rectangle(cr, w - 50, top + 22, 10, 3);
  setColor(cr, kSteelDark);
  cairo_fill(cr);
  // The top: white laminate, an orange edge strip, clamped down with bolts
  // (it does not fall either).
  roundedRect(cr, 2, top, w - 4, 11, 3);
  linearFill(cr, 0, top, 0, top + 11, rgb(255, 255, 255), rgb(206, 214, 226), true);
  outline(cr, 2.0);
  cairo_rectangle(cr, 4, top + 7, w - 8, 3);
  setColor(cr, kOrange);
  cairo_fill(cr);
  for (double x : {14.0, w - 14})
    bolt(cr, x, top + 4, 2.2);
  if (mug)
  {
    // Papers held down by a stapler.
    cairo_save(cr);
    cairo_translate(cr, 22, top - 2);
    cairo_rotate(cr, -0.06);
    cairo_rectangle(cr, 0, -3, 34, 3);
    setColor(cr, rgb(250, 250, 244));
    cairo_fill_preserve(cr);
    outline(cr, 1.0);
    cairo_restore(cr);
    // The mug: big, white, chipped, its slogan in navy.
    const double mx = w - 62, mw = 40, mh = 44, my = top - mh;
    cairo_arc(cr, mx + mw + 2, my + mh * 0.48, 11, -kPi * 0.5, kPi * 0.5);
    setColor(cr, kInk);
    cairo_set_line_width(cr, 8.5);
    cairo_stroke(cr);
    cairo_arc(cr, mx + mw + 2, my + mh * 0.48, 11, -kPi * 0.5, kPi * 0.5);
    setColor(cr, rgb(240, 242, 248));
    cairo_set_line_width(cr, 4.5);
    cairo_stroke(cr);
    roundedRect(cr, mx, my, mw, mh, 5);
    linearFill(cr, mx, 0, mx + mw, 0, rgb(255, 255, 255), rgb(196, 204, 218), true);
    outline(cr, 2.0);
    cairo_save(cr);
    cairo_scale(cr, 1.0, 0.35);
    cairo_arc(cr, mx + mw * 0.5, (my + 1) / 0.35, mw * 0.46, 0, 2 * kPi);
    cairo_restore(cr);
    setColor(cr, rgb(90, 52, 30));
    cairo_fill(cr);
    cairo_rectangle(cr, mx + 3, my + 8, mw - 6, 3);
    setColor(cr, kOrange);
    cairo_fill(cr);
    setColor(cr, rgb(30, 44, 96));
    centredText(cr, "STATION", mx + mw * 0.5, my + 22, 7.8);
    centredText(cr, "MANAGER", mx + mw * 0.5, my + 31, 7.8);
    // A star for the manager, and a chip out of the rim.
    cairo_arc(cr, mx + mw * 0.5, my + 38, 2.4, 0, 2 * kPi);
    setColor(cr, rgb(255, 196, 40));
    cairo_fill(cr);
    cairo_move_to(cr, mx + 9, my);
    cairo_line_to(cr, mx + 12, my + 4);
    cairo_line_to(cr, mx + 15, my);
    setColor(cr, rgb(170, 176, 190));
    cairo_fill(cr);
  }
  else
  {
    // A monitor showing a graph of something falling up.
    const double mx = w * 0.5 - 30, my = top - 50;
    cairo_rectangle(cr, w * 0.5 - 4, top - 10, 8, 10);
    setColor(cr, kSteelDark);
    cairo_fill(cr);
    roundedRect(cr, mx, my, 60, 42, 4);
    linearFill(cr, 0, my, 0, my + 42, rgb(70, 76, 92), rgb(34, 38, 50), true);
    outline(cr, 2.0);
    cairo_rectangle(cr, mx + 5, my + 5, 50, 32);
    setColor(cr, rgb(16, 40, 50));
    cairo_fill(cr);
    cairo_move_to(cr, mx + 8, my + 14);
    cairo_curve_to(cr, mx + 22, my + 14, mx + 26, my + 34, mx + 52, my + 32);
    setColor(cr, rgb(110, 240, 200));
    cairo_set_line_width(cr, 2.0);
    cairo_stroke(cr);
  }
}

// One length of chain, 24 x 64: links alternating face-on and edge-on.
void paintChain(cairo_t* cr)
{
  for (int k = 0; k < 4; ++k)
  {
    const double y = k * 16.0;
    if (k % 2 == 0)
    {
      roundedRect(cr, 5, y - 3, 14, 22, 7);
      setColor(cr, kInk);
      cairo_set_line_width(cr, 6.0);
      cairo_stroke_preserve(cr);
      setColor(cr, rgb(176, 184, 200));
      cairo_set_line_width(cr, 3.2);
      cairo_stroke(cr);
    }
    else
    {
      roundedRect(cr, 9.5, y - 4, 5, 24, 2.5);
      linearFill(cr, 9.5, 0, 14.5, 0, rgb(226, 232, 242), rgb(90, 98, 116), true);
      outline(cr, 1.6);
    }
  }
}

// The observation window, w x h: a thick steel frame, the dark control room
// behind it with two scientists and their screens, reflections on the glass.
void paintWindow(cairo_t* cr, double w, double h)
{
  roundedRect(cr, 0, 0, w, h, 14);
  linearFill(cr, 0, 0, 0, h, kSteelLight, kSteel, true);
  outline(cr, 2.4);
  const double gx = 14, gy = 14, gw = w - 28, gh = h - 28;
  roundedRect(cr, gx, gy, gw, gh, 8);
  linearFill(cr, 0, gy, 0, gy + gh, rgb(28, 48, 72), rgb(10, 18, 30), true);
  outline(cr, 2.0);
  cairo_save(cr);
  roundedRect(cr, gx, gy, gw, gh, 8);
  cairo_clip(cr);
  // Screens glowing along the back of the room.
  for (double x = gx + 12; x < gx + gw - 40; x += 54)
  {
    cairo_rectangle(cr, x, gy + gh * 0.25, 40, 26);
    setColor(cr, rgba(90, 220, 255, 150));
    cairo_fill(cr);
  }
  // Two scientists watching, one with a clipboard.
  const int count = std::max(1, std::min(3, int(gw / 110)));
  for (int k = 0; k < count; ++k)
  {
    const double cx = gx + gw * (k + 0.5) / count, base = gy + gh;
    cairo_move_to(cr, cx - 30, base);
    cairo_curve_to(cr, cx - 28, base - 34, cx + 28, base - 34, cx + 30, base);
    cairo_close_path(cr);
    setColor(cr, rgb(200, 210, 224));
    cairo_fill(cr);
    cairo_arc(cr, cx, base - 44, 14, 0, 2 * kPi);
    setColor(cr, rgb(30, 38, 54));
    cairo_fill(cr);
    // Goggles catching the chamber's light.
    cairo_rectangle(cr, cx - 10, base - 48, 20, 6);
    setColor(cr, rgba(255, 190, 110, 220));
    cairo_fill(cr);
    if (k == 0)
    {
      cairo_rectangle(cr, cx + 8, base - 30, 16, 20);
      setColor(cr, rgb(170, 130, 80));
      cairo_fill(cr);
      cairo_rectangle(cr, cx + 10, base - 27, 12, 15);
      setColor(cr, rgb(250, 250, 244));
      cairo_fill(cr);
    }
  }
  // Reflections.
  for (double x = gx - 40; x < gx + gw; x += 120)
  {
    cairo_move_to(cr, x, gy + gh);
    cairo_line_to(cr, x + 26, gy + gh);
    cairo_line_to(cr, x + 26 + gh * 0.6, gy);
    cairo_line_to(cr, x + gh * 0.6, gy);
    cairo_close_path(cr);
    setColor(cr, rgba(255, 255, 255, 36));
    cairo_fill(cr);
  }
  cairo_restore(cr);
  for (double x : {7.0, w - 7})
    for (double y : {7.0, h - 7})
      bolt(cr, x, y, 2.8);
  hazard(cr, w * 0.5 - 30, h - 9, 60, 6, 8);
}

// CHAMBER 42, stencilled on an enamel plate, w x h.
void paintChamberSign(cairo_t* cr, double w, double h)
{
  roundedRect(cr, 2, 2, w - 4, h - 4, 6);
  linearFill(cr, 0, 2, 0, h - 2, rgb(255, 255, 255), rgb(214, 220, 232), true);
  outline(cr, 2.0);
  cairo_rectangle(cr, 6, 6, 10, h - 12);
  cairo_rectangle(cr, w - 16, 6, 10, h - 12);
  setColor(cr, kOrange);
  cairo_fill(cr);
  setColor(cr, kCharcoal);
  centredText(cr, "CHAMBER 42", w * 0.5, h * 0.5 + h * 0.18, std::min(h * 0.46, w * 0.11));
}

// --- The Grav Grenade ------------------------------------------------------------

// The grenade in flight: a small purple sphere with a ring, 40 x 40, the
// middle at (20, 20).
void paintGrenade(cairo_t* cr)
{
  cairo_arc(cr, 20, 20, 11, 0, 2 * kPi);
  cairo_pattern_t* p = cairo_pattern_create_radial(16, 15, 1, 20, 20, 11);
  cairo_pattern_add_color_stop_rgb(p, 0, 0.95, 0.8, 1.0);
  cairo_pattern_add_color_stop_rgb(p, 0.5, 0.62, 0.3, 0.95);
  cairo_pattern_add_color_stop_rgb(p, 1, 0.25, 0.06, 0.45);
  cairo_set_source(cr, p);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(p);
  outline(cr, 2.0);
  cairo_save(cr);
  cairo_translate(cr, 20, 20);
  cairo_scale(cr, 1.0, 0.35);
  cairo_arc(cr, 0, 0, 16, 0, 2 * kPi);
  cairo_restore(cr);
  setColor(cr, kInk);
  cairo_set_line_width(cr, 4.0);
  cairo_stroke(cr);
  cairo_save(cr);
  cairo_translate(cr, 20, 20);
  cairo_scale(cr, 1.0, 0.35);
  cairo_arc(cr, 0, 0, 16, 0, 2 * kPi);
  cairo_restore(cr);
  setColor(cr, kOrange);
  cairo_set_line_width(cr, 1.8);
  cairo_stroke(cr);
  cairo_arc(cr, 16, 15, 3, 0, 2 * kPi);
  setColor(cr, rgb(255, 255, 255));
  cairo_fill(cr);
}

// The vortex's disc: a black hole in a purple whirl, 240 x 240, the middle
// at (120, 120). `arms`: the spiral arms alone (turned faster on top).
void paintVortex(cairo_t* cr, bool arms)
{
  const double c = 120, rad = 110;
  if (!arms)
  {
    cairo_arc(cr, c, c, rad, 0, 2 * kPi);
    cairo_pattern_t* p = cairo_pattern_create_radial(c, c, 0, c, c, rad);
    cairo_pattern_add_color_stop_rgba(p, 0.0, 0.0, 0.0, 0.02, 1.0);
    cairo_pattern_add_color_stop_rgba(p, 0.28, 0.06, 0.0, 0.12, 1.0);
    cairo_pattern_add_color_stop_rgba(p, 0.55, 0.35, 0.1, 0.62, 0.9);
    cairo_pattern_add_color_stop_rgba(p, 0.8, 0.55, 0.25, 0.95, 0.45);
    cairo_pattern_add_color_stop_rgba(p, 1.0, 0.6, 0.3, 1.0, 0.0);
    cairo_set_source(cr, p);
    cairo_fill(cr);
    cairo_pattern_destroy(p);
    // The event horizon's bright rim.
    cairo_arc(cr, c, c, rad * 0.3, 0, 2 * kPi);
    setRgba(cr, rgb(230, 190, 255), 0.85);
    cairo_set_line_width(cr, 2.5);
    cairo_stroke(cr);
    return;
  }
  for (int arm = 0; arm < 4; ++arm)
  {
    const double a0 = arm * kPi * 0.5;
    cairo_new_path(cr);
    for (int i = 0; i <= 40; ++i)
    {
      const double t = i / 40.0;
      const double rr = rad * (0.32 + 0.66 * t);
      const double a = a0 + t * 2.6;
      if (i == 0)
        cairo_move_to(cr, c + std::cos(a) * rr, c + std::sin(a) * rr);
      else
        cairo_line_to(cr, c + std::cos(a) * rr, c + std::sin(a) * rr);
    }
    setRgba(cr, rgb(210, 150, 255), 0.75);
    cairo_set_line_width(cr, 7.0);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_stroke_preserve(cr);
    setRgba(cr, rgb(255, 240, 255), 0.8);
    cairo_set_line_width(cr, 2.0);
    cairo_stroke(cr);
  }
}

// A cartoon star for the dizzy runner, 24 x 24, the middle at (12, 12).
void paintStar(cairo_t* cr)
{
  for (int i = 0; i < 10; ++i)
  {
    const double a = -kPi * 0.5 + i * kPi / 5.0, rr = i % 2 ? 4.6 : 10.5;
    if (i == 0)
      cairo_move_to(cr, 12 + std::cos(a) * rr, 12 + std::sin(a) * rr);
    else
      cairo_line_to(cr, 12 + std::cos(a) * rr, 12 + std::sin(a) * rr);
  }
  cairo_close_path(cr);
  linearFill(cr, 0, 2, 0, 22, rgb(255, 250, 190), rgb(255, 190, 40), true);
  outline(cr, 1.8);
}

float dirAngle(Grav g)
{
  switch (g)
  {
    case Grav::Up:
      return 180.0f;
    case Grav::Left:
      return 90.0f;
    case Grav::Right:
      return -90.0f;
    default:
      return 0.0f;
  }
}

float ease(float t)
{
  t = std::clamp(t, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

} // namespace

void World::drawGravBack(Renderer& r, float camX, float camY, int frame) const
{
  const auto& g = mGrav;
  const int tx0 = std::max(0, int(camX / kTilePx) - 1);
  const int ty0 = std::max(0, int(camY / kTilePx) - 1);
  const int tx1 = std::min(mLevel->width - 1, int((camX + float(kScreenW)) / kTilePx) + 1);
  const int ty1 = std::min(mLevel->height - 1, int((camY + float(kScreenH)) / kTilePx) + 1);
  auto fake = [&](int bx, int by) {
    for (const auto& f : g.fakes)
      if (bx == f.bx && by >= f.by && by < f.by + f.h)
        return true;
    return false;
  };
  auto wall = [&](int bx, int by) { return mMap.block(bx, by) == Tile::Solid || fake(bx, by); };
  auto inZone = [&](int bx, int by) {
    for (const auto& z : g.zones)
      if (bx >= z.x0 && bx <= z.x1 && by >= z.y0 && by <= z.y1)
        return true;
    return false;
  };

  // The chambers: faint panel seams, strip lights under the ceiling (they
  // blink orange for a moment after the chamber turns over) and the
  // chamber's name stencilled on its back wall.
  for (const auto& z : g.zones)
  {
    const float x = float(z.x0) * kTilePx - camX, y = float(z.y0) * kTilePx - camY;
    const float w = float(z.x1 - z.x0 + 1) * kTilePx, h = float(z.y1 - z.y0 + 1) * kTilePx;
    if (!visible(x, y, w, h))
      continue;
    for (int bx = std::max(z.x0 + 2, tx0 - (tx0 - z.x0) % 4); bx <= std::min(z.x1 - 1, tx1); bx += 4)
      if (bx > z.x0)
        r.fillRect(float(bx) * kTilePx - camX - 1.0f, std::max(y, -2.0f), 2.0f, std::min(h, float(kScreenH) + 4.0f),
          rgba(120, 132, 156, 34));
    for (int by = z.y0 + 3; by <= z.y1; by += 3)
      r.fillRect(std::max(x, -2.0f), float(by) * kTilePx - camY - 1.0f, std::min(w, float(kScreenW) + 4.0f), 2.0f,
        rgba(120, 132, 156, 34));
    const bool alarm = z.lastFlip >= 0 && z.flip < 30 && (z.flip / 4) % 2 == 0;
    const float ly = y + 6.0f;
    for (int bx = z.x0 + 3; bx + 2 <= z.x1; bx += 8)
    {
      const float lx = float(bx) * kTilePx - camX;
      if (!visible(lx, ly - 40.0f, 128.0f, 200.0f))
        continue;
      r.fillRect(lx, ly - 6.0f, 128.0f, 12.0f, rgb(90, 98, 116));
      r.fillRect(lx + 4.0f, ly - 3.0f, 120.0f, 7.0f, alarm ? rgb(255, 170, 70) : rgb(255, 255, 255));
      drawGlow(r, mArt, lx + 64.0f, ly + 10.0f, alarm ? 110.0f : 150.0f, alarm ? kOrange : rgb(255, 255, 255),
        alarm ? 0.55f : 0.18f);
    }
    if (!z.id.empty() && z.id.size() <= 4 && z.x1 - z.x0 >= 10 && z.y1 - z.y0 >= 6)
      r.drawText(z.id, x + 2.2f * kTilePx, y + 1.2f * kTilePx, {72.0f, rgb(150, 160, 180), 0}, Align::Left, 0.35f);
  }

  // The walls that are not there: drawn exactly like the wall round them
  // (and the wall under them shaded as if they were real).
  for (const auto& f : g.fakes)
  {
    const float x = float(f.bx) * kTilePx - camX;
    if (!visible(x, float(f.by) * kTilePx - camY, kTilePx, float(f.h + 3) * kTilePx))
      continue;
    for (int by = f.by; by < f.by + f.h + 3; ++by)
    {
      if (by >= f.by + f.h && mMap.block(f.bx, by) != Tile::Solid)
        break;
      int depth = 0;
      while (depth < 3 && wall(f.bx, by - depth - 1))
        ++depth;
      static constexpr int kShade[4] = {255, 210, 175, 145};
      const auto hs = (hash2(f.bx, by) >> 8) % 10u;
      DrawOpts o;
      o.tint = rgb(kShade[depth], kShade[depth], kShade[depth]);
      const float y = float(by) * kTilePx - camY;
      r.draw(mArt.solid[hs < 7u ? 0u : (hs < 9u ? 1u : 2u)], x, y, o);
      if (by == f.by && !wall(f.bx, by - 1) && mMap.block(f.bx, by - 1) != Tile::Spikes)
        r.draw(mArt.solidTop, x, y);
    }
  }

  // Ceilings get the floors' lip and safety band too, turned over (they are
  // floors half the time here).
  for (int ty = ty0; ty <= ty1; ++ty)
    for (int tx = tx0; tx <= tx1; ++tx)
    {
      if (!wall(tx, ty) || wall(tx, ty + 1) || mMap.block(tx, ty + 1) == Tile::SpikesDown ||
          (!g.gun && !inZone(tx, ty + 1)))
        continue;
      DrawOpts o;
      o.angle = 180.0f;
      r.draw(mArt.solidTop, float(tx + 1) * kTilePx - camX, float(ty + 1) * kTilePx - camY, o);
    }

  // The orange wall arrows, turning 180 degrees as their chamber flips.
  for (const auto& a : g.arrows)
  {
    const float cx = (float(a.bx) + 0.5f) * kTilePx - camX, cy = (float(a.by) + 0.5f) * kTilePx - camY;
    if (!visible(cx - 40.0f, cy - 40.0f, 80.0f, 80.0f))
      continue;
    const Texture& plate = baked(mArt, r, "arrowplate", 64, 64, 32.0f, 32.0f, paintArrowPlate);
    const Texture& arrow = baked(mArt, r, "arrow", 48, 48, 24.0f, 24.0f, paintArrow);
    r.draw(plate, cx, cy);
    DrawOpts o;
    if (a.zone >= 0 && std::size_t(a.zone) < g.zones.size())
    {
      const auto& z = g.zones[std::size_t(a.zone)];
      o.angle = dirAngle(z.dir);
      if (z.lastFlip >= 0 && z.flip < 6)
        o.angle -= 180.0f * (1.0f - ease(float(z.flip) / 6.0f));
      if (z.lastFlip >= 0 && z.flip < 12)
        drawGlow(r, mArt, cx, cy, 44.0f, kOrange, 0.6f * (1.0f - float(z.flip) / 12.0f));
    }
    r.draw(arrow, cx, cy, o);
  }

  // The switch panels: the lever points the chamber's way and swings over
  // when thrown, the lamp flashes and then rests amber for 10 frames.
  for (const auto& s : g.switches)
  {
    const float cx = (float(s.bx) + 0.5f) * kTilePx - camX, cy = (float(s.by) + 0.5f) * kTilePx - camY;
    if (!visible(cx - 40.0f, cy - 60.0f, 80.0f, 120.0f))
      continue;
    const Texture& panel = baked(mArt, r, "switch", 56, 104, 28.0f, 56.0f, paintSwitchPanel);
    const Texture& lever = baked(mArt, r, "lever", 24, 56, 12.0f, 12.0f, paintLever);
    r.draw(panel, cx, cy);
    Grav dir = Grav::Down;
    bool thrown = false;
    if (s.zone >= 0 && std::size_t(s.zone) < g.zones.size())
    {
      const auto& z = g.zones[std::size_t(s.zone)];
      dir = z.dir;
      thrown = z.lastFlip >= 0 || s.lastHit > -1000;
    }
    DrawOpts o;
    o.angle = dir == Grav::Up ? 180.0f : 0.0f;
    if (thrown && s.flash < 6)
      o.angle -= 180.0f * (1.0f - ease(float(s.flash) / 6.0f));
    r.draw(lever, cx, cy, o);
    const bool resting = thrown && s.flash < 10;
    const Color lamp = resting ? rgb(255, 170, 40) : rgb(90, 255, 140);
    drawGlow(r, mArt, cx, cy - 36.0f, 14.0f, lamp, 0.9f);
    r.fillRect(cx - 3.0f, cy - 39.0f, 6.0f, 6.0f, lerpColor(lamp, rgb(255, 255, 255), 0.5f));
    if (thrown && s.flash < 12)
    {
      const float t = 1.0f - float(s.flash) / 12.0f;
      drawGlow(r, mArt, cx, cy, 70.0f * t + 20.0f, rgb(255, 255, 255), 0.7f * t);
    }
  }

  // Test gates: a white slab in a steel frame, sliding up into the ceiling
  // when it opens.
  for (const auto& gt : g.gates)
  {
    const float x = float(gt.bx) * kTilePx - camX, y = float(gt.by) * kTilePx - camY;
    const float full = float(gt.h) * kTilePx;
    if (!visible(x - 16.0f, y - 16.0f, kTilePx + 32.0f, full + 32.0f))
      continue;
    // The rails either side.
    for (const float rx : {x - 6.0f, x + kTilePx - 2.0f})
    {
      r.fillRect(rx, y, 8.0f, full, rgb(70, 78, 96));
      r.fillRect(rx + 2.0f, y, 2.0f, full, rgb(170, 180, 198));
    }
    const float t = gt.open ? ease(float(gt.opened) / 24.0f) : 0.0f;
    const int vis = int(std::round(full * (1.0f - t) / 4.0f)) * 4;
    if (vis > 0)
    {
      const std::string key = "gate/" + std::to_string(gt.h) + "/" + std::to_string(vis);
      const Texture& slab = baked(mArt, r, key, int(kTilePx), vis, 0.0f, 0.0f,
        [&](cairo_t* cr) { paintGate(cr, double(kTilePx), double(full), double(vis)); });
      r.draw(slab, x, y);
    }
    // The status lamp over it: red while shut, green once open.
    const Color lamp = gt.open ? rgb(90, 255, 140) : rgb(255, 60, 50);
    drawGlow(r, mArt, x + kTilePx * 0.5f, y + 8.0f, 18.0f, lamp, gt.open || (frame / 20) % 2 ? 0.9f : 0.5f);
  }

  // Scenery.
  for (const auto& d : g.decos)
  {
    const float x = float(d.x0) * kTilePx - camX, y = float(d.y0) * kTilePx - camY;
    const int wpx = (d.x1 - d.x0 + 1) * int(kTilePx), hpx = (d.y1 - d.y0 + 1) * int(kTilePx);
    const float w = float(wpx), h = float(hpx);
    if (!visible(x - 128.0f, y - 128.0f, w + 256.0f, h + 256.0f))
      continue;
    if (d.kind == "mug" || d.kind == "desk")
    {
      // At least two blocks wide, standing on the rect's floor.
      const bool mug = d.kind == "mug";
      const int dw = std::max(wpx, 2 * int(kTilePx)), dh = 120;
      const Texture& tex = baked(mArt, r, std::string(mug ? "mugdesk/" : "desk/") + std::to_string(dw), dw, dh,
        float(dw) * 0.5f, float(dh), [&](cairo_t* cr) { paintDesk(cr, double(dw), double(dh), mug); });
      const float fx = x + w * 0.5f, fy = y + h;
      r.draw(tex, fx, fy);
      if (mug)
      {
        // Steam curling up off the coffee, whichever way down is.
        const float mx = fx + float(dw) * 0.5f - 42.0f, my = fy - 60.0f - 46.0f;
        for (int k = 0; k < 3; ++k)
        {
          const float ph = std::fmod(float(frame) * 0.012f + float(k) / 3.0f, 1.0f);
          drawGlow(r, mArt, mx + std::sin(ph * 9.0f + float(k)) * 6.0f, my - ph * 46.0f, 10.0f + ph * 8.0f,
            rgb(255, 255, 255), 0.35f * (1.0f - ph));
        }
      }
    }
    else if (d.kind == "chain")
    {
      const Texture& link = baked(mArt, r, "chain", 24, 64, 12.0f, 0.0f, paintChain);
      std::vector<float> cols;
      if (d.x1 - d.x0 < 2)
        cols.push_back(x + w * 0.5f);
      else
      {
        cols.push_back(x + kTilePx * 0.5f);
        cols.push_back(x + w - kTilePx * 0.5f);
      }
      for (const float cx : cols)
      {
        // A bracket on the ceiling, the chain, a shackle on the ledge.
        r.fillRect(cx - 14.0f, y - 2.0f, 28.0f, 8.0f, rgb(70, 78, 96));
        for (float cy = y + 4.0f; cy < y + h; cy += 64.0f)
          r.draw(link, cx, cy);
        r.fillRect(cx - 10.0f, y + h - 6.0f, 20.0f, 6.0f, rgb(90, 98, 116));
      }
    }
    else if (d.kind == "window")
    {
      const Texture& tex = baked(mArt, r, "window/" + std::to_string(wpx) + "x" + std::to_string(hpx), wpx, hpx, 0.0f,
        0.0f, [&](cairo_t* cr) { paintWindow(cr, w, h); });
      r.draw(tex, x, y);
      if ((frame / 30) % 3 == 0)
        drawGlow(r, mArt, x + w - 24.0f, y + 24.0f, 12.0f, rgb(255, 60, 50), 0.9f);
    }
    else if (d.kind == "42" || d.kind == "sign42")
    {
      const int sw = std::max(wpx, 3 * int(kTilePx)), sh = 48;
      const Texture& tex = baked(mArt, r, "sign42/" + std::to_string(sw), sw, sh, float(sw) * 0.5f, float(sh) * 0.5f,
        [&](cairo_t* cr) { paintChamberSign(cr, double(sw), double(sh)); });
      r.draw(tex, x + w * 0.5f, y + h * 0.5f);
    }
  }
}

void World::drawGravFront(Renderer& r, float camX, float camY, int frame) const
{
  const auto& g = mGrav;

  // The Grav Grenade: a purple sphere in flight, then a whirling vortex
  // with sparks being sucked in, popping at the end.
  for (std::size_t i = 0; i < g.vortices.size(); ++i)
  {
    const auto& v = g.vortices[i];
    const float cx = v.x * kCellPx - camX, cy = v.y * kCellPx - camY;
    if (!visible(cx - 200.0f, cy - 200.0f, 400.0f, 400.0f))
      continue;
    if (v.flight >= 0)
    {
      // A short purple trail behind it.
      const float sp = std::max(0.001f, std::sqrt(v.vx * v.vx + v.vy * v.vy));
      for (int k = 1; k <= 4; ++k)
        drawGlow(r, mArt, cx - v.vx / sp * float(k) * 12.0f, cy - v.vy / sp * float(k) * 12.0f, 16.0f - float(k) * 2.0f,
          kVortex, 0.5f - float(k) * 0.1f);
      drawGlow(r, mArt, cx, cy, 30.0f, kVortex, 0.6f);
      const Texture& tex = baked(mArt, r, "grenade", 40, 40, 20.0f, 20.0f, paintGrenade);
      DrawOpts o;
      o.angle = float(frame * 24 % 360);
      r.draw(tex, cx, cy, o);
      continue;
    }
    const int age = 45 - v.life;
    float scale = std::min(1.0f, 0.3f + float(age) * 0.12f);
    float alpha = 1.0f;
    if (v.life <= 5)
    {
      // Popping: a white flash and a ring blown outward.
      const float t = 1.0f - float(v.life) / 5.0f;
      scale = 1.0f + t * 0.5f;
      alpha = 1.0f - t;
      drawGlow(r, mArt, cx, cy, 80.0f + 120.0f * t, rgb(240, 210, 255), 0.9f * (1.0f - t * 0.5f));
      const float rr = 60.0f + t * 130.0f;
      for (int k = 0; k < 24; ++k)
      {
        const float a = float(k) * 0.2618f;
        r.drawLine(cx + std::cos(a) * rr * 0.85f, cy + std::sin(a) * rr * 0.85f, cx + std::cos(a) * rr,
          cy + std::sin(a) * rr, 4.0f, rgba(230, 190, 255, int(230 * (1.0f - t))), Blend::Add);
      }
    }
    drawGlow(r, mArt, cx, cy, 150.0f * scale, kVortex, 0.35f * alpha);
    const Texture& disc = baked(mArt, r, "vortex", 240, 240, 120.0f, 120.0f, [](cairo_t* cr) { paintVortex(cr, false); });
    const Texture& arms = baked(mArt, r, "vortexarms", 240, 240, 120.0f, 120.0f, [](cairo_t* cr) { paintVortex(cr, true); });
    DrawOpts o;
    o.scale = scale * 0.87f; // about 6 cells across
    o.alpha = alpha;
    o.angle = -float(frame * 4 % 360);
    r.draw(disc, cx, cy, o);
    o.angle = -float(frame * 11 % 360);
    r.draw(arms, cx, cy, o);
    // Sparks spiralling in from the pull's edge.
    for (int k = 0; k < 18; ++k)
    {
      const float ph = std::fmod(float(frame) * 0.03f + float(k) * 0.381966f, 1.0f);
      const float rr = (190.0f * (1.0f - ph) + 18.0f) * scale;
      const float a = float(k) * 2.39996f - ph * 5.0f;
      const float px = cx + std::cos(a) * rr, py = cy + std::sin(a) * rr;
      const float qx = cx + std::cos(a + 0.18f) * (rr + 16.0f), qy = cy + std::sin(a + 0.18f) * (rr + 16.0f);
      r.drawLine(qx, qy, px, py, 2.5f, rgba(235, 200, 255, int(220 * ph * alpha)), Blend::Add);
    }
    drawGlow(r, mArt, cx, cy, 20.0f * scale, rgb(255, 255, 255), 0.25f * alpha);
  }

  // Dizzy stars circling the runner's head (the end of the box away from
  // the feet).
  if (g.dizzy > 0 && !mPlayer.hidden)
  {
    const CellBox b = mPlayer.box();
    float hx = (float(b.x) + float(b.w) * 0.5f) * kCellPx - camX;
    float hy = (float(b.y) + float(b.h) * 0.5f) * kCellPx - camY;
    switch (mPlayer.grav)
    {
      case Grav::Up:
        hy = float(b.y + b.h) * kCellPx - camY - 6.0f;
        break;
      case Grav::Left:
        hx = float(b.x + b.w) * kCellPx - camX - 6.0f;
        break;
      case Grav::Right:
        hx = float(b.x) * kCellPx - camX + 6.0f;
        break;
      default:
        hy = float(b.y) * kCellPx - camY + 6.0f;
        break;
    }
    const Texture& star = baked(mArt, r, "star", 24, 24, 12.0f, 12.0f, paintStar);
    const float fade = std::min(1.0f, float(g.dizzy) / 8.0f);
    const bool sideways = mPlayer.grav == Grav::Left || mPlayer.grav == Grav::Right;
    for (int k = 0; k < 3; ++k)
    {
      const float a = float(frame) * 0.16f + float(k) * 2.0944f;
      const float ox = std::cos(a) * 34.0f, oy = std::sin(a) * 10.0f;
      DrawOpts o;
      o.alpha = fade;
      o.angle = float(frame * 6 % 360);
      o.scale = 0.85f + 0.2f * std::sin(a);
      r.draw(star, hx + (sideways ? oy : ox), hy + (sideways ? ox : oy), o);
    }
  }
}

void World::drawGravHud(Renderer& r, int frame) const
{
  // A small badge under the score panel: which way down is for the runner,
  // the arrow swinging round after a flip.
  const float w = 150.0f, hgt = 30.0f;
  const float x = float(kScreenW) - 12.0f - w, y = 10.0f + float(kHudPanelH) + 8.0f;
  r.fillRect(x, y, w, hgt, rgba(8, 6, 22, 170));
  r.fillRect(x, y, w, 2.0f, withAlpha(mTheme.accentA, 170));
  r.drawText(mGrav.gun ? "GUN GRAV" : "GRAVITY", x + 12.0f, y + 6.0f, {16.0f, mTheme.accentA, rgb(10, 8, 20)});
  float a = dirAngle(mPlayer.grav) * 0.0174533f;
  if (mGrav.lastFlip >= 0 && clock() - mGrav.lastFlip < 8)
    a -= 3.14159f * (1.0f - ease(float(clock() - mGrav.lastFlip) / 8.0f));
  // The arrow, pointing (0, 1) turned by a.
  const float cx = x + w - 22.0f, cy = y + hgt * 0.5f;
  const float dx = -std::sin(a), dy = std::cos(a);
  const float px = -dy, py = dx;
  const Color c = (mGrav.dizzy > 0 && (frame / 4) % 2) ? rgb(255, 240, 140) : mTheme.accentA;
  r.drawLine(cx - dx * 9.0f, cy - dy * 9.0f, cx + dx * 7.0f, cy + dy * 7.0f, 4.0f, c);
  r.drawLine(cx + dx * 10.0f, cy + dy * 10.0f, cx + dx * 2.0f + px * 7.0f, cy + dy * 2.0f + py * 7.0f, 4.0f, c);
  r.drawLine(cx + dx * 10.0f, cy + dy * 10.0f, cx + dx * 2.0f - px * 7.0f, cy + dy * 2.0f - py * 7.0f, 4.0f, c);
}

} // namespace gr
