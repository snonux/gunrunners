// Level 24, Too Many Cooks: drawing. See world_kitchen.cpp for the logic.
//
// Over the tiles (drawKitchenBack): the conveyor belts (a steel housing,
// rollers turning, a black rubber top with chevrons one every two blocks
// scrolling its way, flashing amber when a bell rings), the order rail and
// its tickets, the chef's bells on their brackets, the pantry flaps, the
// sauce pans, the sink, the meat grinder that drops the meatballs, the
// toaster and its toast, the set dressing (`@ deco kind=pot|basket|oven`,
// the TABLE 42 ticket, the pots hanging under their rims), the dishwasher
// racks, the golden whisk and cleavers stuck in walls. In front
// (drawKitchenFront): the chef's hand (the show-off lift, and the
// thumbs-up at the exit) and the sneezes. drawKitchenHud: the Pepper
// Grinder's spin-up. The Turntable bonus (drawTurntableBack/Front): a
// microwave's inside, turning, seen through its door.

#include "game/world.hpp"

#include "assets/art.hpp"
#include "assets/enemy_art.hpp"
#include "assets/enemy_art_kitchen.hpp"
#include "base/math.hpp"
#include "render/vector.hpp"

#include <algorithm>
#include <array>
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
constexpr float kCellPx = float(kCellSize) * kPixelScale; // 32
constexpr float kTilePx = float(kTileSize) * kPixelScale; // 64
constexpr Color kInk = rgb(30, 18, 14);
constexpr Color kAmber = rgb(255, 190, 60);
constexpr Color kGold = rgb(255, 210, 90);

bool offScreen(float x, float y, float w, float h)
{
  return x > float(kScreenW) + 96.0f || x + w < -96.0f || y > float(kScreenH) + 96.0f || y + h < -96.0f;
}

// A texture baked once and kept in the Art's sprite cache under `key`.
const Texture& baked(const Art& art, const Renderer& r, const std::string& key, int w, int h, float ax, float ay,
  const std::function<void(cairo_t*)>& paint)
{
  const std::string id = "~kitchen/" + key;
  auto it = art.styled.find(id);
  if (it != art.styled.end())
    return it->second.facing[0];
  VectorImage img(w, h);
  paint(img.cr());
  Sprite s;
  s.facing[0] = img.toTexture(r, ax, ay);
  return art.styled.emplace(id, std::move(s)).first->second.facing[0];
}

void stop(cairo_pattern_t* g, double at, Color c, double a = 1.0)
{
  cairo_pattern_add_color_stop_rgba(g, at, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, a);
}

void linear(cairo_t* cr, double x0, double y0, double x1, double y1, std::initializer_list<std::pair<double, Color>> stops,
  double line = 0.0)
{
  cairo_pattern_t* g = cairo_pattern_create_linear(x0, y0, x1, y1);
  for (const auto& s : stops)
    stop(g, s.first, s.second, alphaOf(s.second) / 255.0);
  cairo_set_source(cr, g);
  if (line > 0)
  {
    cairo_fill_preserve(cr);
    setColor(cr, kInk);
    cairo_set_line_width(cr, line);
    cairo_stroke(cr);
  }
  else
    cairo_fill(cr);
  cairo_pattern_destroy(g);
}

// Brushed steel across x0..x1.
void steel(cairo_t* cr, double x0, double x1, double line = 0.0)
{
  linear(cr, x0, 0, x1, 0,
    {{0, rgb(150, 160, 174)}, {0.25, rgb(236, 240, 246)}, {0.6, rgb(186, 194, 206)}, {1, rgb(120, 128, 142)}}, line);
}

void steelV(cairo_t* cr, double y0, double y1, double line = 0.0)
{
  linear(cr, 0, y0, 0, y1,
    {{0, rgb(246, 248, 252)}, {0.35, rgb(196, 204, 216)}, {0.75, rgb(130, 138, 152)}, {1, rgb(90, 96, 110)}}, line);
}

void circlePath(cairo_t* cr, double x, double y, double r)
{
  cairo_new_sub_path(cr);
  cairo_arc(cr, x, y, r, 0, 2 * kPi);
}

void fillInk(cairo_t* cr, Color c, double line)
{
  setColor(cr, c);
  cairo_fill_preserve(cr);
  setColor(cr, kInk);
  cairo_set_line_width(cr, line);
  cairo_stroke(cr);
}

// --- Painters ---------------------------------------------------------------------

// A belt's chevron, 34 x 20, pointing right, the tip at (30, 10).
void paintChevron(cairo_t* cr)
{
  cairo_move_to(cr, 4, 2);
  cairo_line_to(cr, 16, 2);
  cairo_line_to(cr, 30, 10);
  cairo_line_to(cr, 16, 18);
  cairo_line_to(cr, 4, 18);
  cairo_line_to(cr, 18, 10);
  cairo_close_path(cr);
  setColor(cr, rgb(255, 255, 255));
  cairo_fill(cr);
}

// One block of belt housing, 64 x 64: the rubber top (16 px, drawn dark
// grey; the chevrons go over it), a steel side skirt with a slot for the
// rollers, bolts, a shadow at the foot.
void paintBeltBlock(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 0, 64, 16);
  linear(cr, 0, 0, 0, 16, {{0, rgb(70, 70, 76)}, {0.2, rgb(44, 44, 50)}, {1, rgb(22, 22, 26)}});
  // Rubber cleats.
  for (int i = 0; i < 4; ++i)
  {
    cairo_rectangle(cr, i * 16 + 7, 1.5, 2, 12);
    setColor(cr, rgba(0, 0, 0, 90));
    cairo_fill(cr);
  }
  cairo_rectangle(cr, 0, 0, 64, 1.5);
  setColor(cr, rgba(255, 255, 255, 70));
  cairo_fill(cr);
  // The skirt.
  cairo_rectangle(cr, 0, 16, 64, 48);
  steelV(cr, 16, 64);
  cairo_rectangle(cr, 0, 22, 64, 30);
  setColor(cr, rgb(40, 42, 50));
  cairo_fill(cr);
  cairo_rectangle(cr, 0, 22, 64, 3);
  setColor(cr, rgba(0, 0, 0, 120));
  cairo_fill(cr);
  for (const double bx : {6.0, 58.0})
  {
    circlePath(cr, bx, 58, 2.2);
    setColor(cr, rgb(250, 250, 255));
    cairo_fill(cr);
  }
  cairo_rectangle(cr, 0, 16, 64, 1.2);
  setColor(cr, rgba(0, 0, 0, 160));
  cairo_fill(cr);
}

// A roller seen end-on, 28 x 28, centred: a steel drum with spokes.
void paintRoller(cairo_t* cr)
{
  circlePath(cr, 14, 14, 12.5);
  cairo_pattern_t* g = cairo_pattern_create_radial(10, 9, 2, 14, 14, 13);
  stop(g, 0, rgb(250, 250, 255));
  stop(g, 1, rgb(110, 118, 132));
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  setColor(cr, kInk);
  cairo_set_line_width(cr, 1.6);
  cairo_stroke(cr);
  for (int i = 0; i < 3; ++i)
  {
    const double a = i * kPi / 3;
    cairo_move_to(cr, 14 + std::cos(a) * 10, 14 + std::sin(a) * 10);
    cairo_line_to(cr, 14 - std::cos(a) * 10, 14 - std::sin(a) * 10);
  }
  setColor(cr, rgb(80, 86, 100));
  cairo_set_line_width(cr, 2.2);
  cairo_stroke(cr);
  circlePath(cr, 14, 14, 3);
  setColor(cr, rgb(40, 42, 50));
  cairo_fill(cr);
}

// The drum at a belt's end, 40 x 64: the belt wrapping round it.
void paintBeltEnd(cairo_t* cr)
{
  cairo_move_to(cr, 0, 0);
  cairo_line_to(cr, 8, 0);
  cairo_arc(cr, 8, 26, 26, -kPi / 2, kPi / 2);
  cairo_line_to(cr, 0, 52);
  cairo_close_path(cr);
  linear(cr, 0, 0, 34, 0, {{0, rgb(44, 44, 50)}, {0.7, rgb(30, 30, 34)}, {1, rgb(14, 14, 18)}});
  circlePath(cr, 8, 26, 17);
  steel(cr, -9, 25, 1.6);
  circlePath(cr, 8, 26, 5);
  setColor(cr, rgb(40, 42, 50));
  cairo_fill(cr);
  // The leg under it.
  cairo_rectangle(cr, 0, 50, 10, 14);
  steel(cr, 0, 10);
}

// A paper order ticket, 40 x 60, clipped at its top middle.
void paintTicket(cairo_t* cr, int seed, const char* title)
{
  cairo_move_to(cr, 4, 6);
  cairo_line_to(cr, 36, 6);
  cairo_line_to(cr, 36, 54);
  for (int i = 0; i < 8; ++i)
    cairo_line_to(cr, 36 - (i + 0.5) * 4, i % 2 ? 54 : 58);
  cairo_line_to(cr, 4, 54);
  cairo_close_path(cr);
  linear(cr, 0, 6, 0, 58, {{0, rgb(255, 254, 246)}, {1, rgb(232, 226, 210)}}, 1.2);
  Rng rng(std::uint32_t(seed * 977 + 11));
  if (title)
  {
    selectGameFont(cr);
    cairo_set_font_size(cr, 8);
    cairo_move_to(cr, 7, 18);
    setColor(cr, rgb(200, 30, 30));
    cairo_show_text(cr, title);
  }
  for (int i = 0; i < 5; ++i)
  {
    cairo_rectangle(cr, 8, 22 + i * 6, rng.range(10, 24), 2);
    setColor(cr, rgba(40, 40, 60, 160));
    cairo_fill(cr);
  }
  // The clip.
  cairo_rectangle(cr, 14, 0, 12, 10);
  steel(cr, 14, 26, 1.0);
}

// The TABLE 42 ticket, bigger, 180 x 130, clipped at its bottom middle to
// the rail and leaning: the number circled.
void paintTicket42(cairo_t* cr, const std::string& text)
{
  cairo_translate(cr, 90, 124);
  cairo_rotate(cr, -0.08);
  cairo_move_to(cr, -78, -6);
  cairo_line_to(cr, -78, -112);
  for (int i = 0; i < 13; ++i)
    cairo_line_to(cr, -78 + (i + 0.5) * 12, i % 2 ? -112 : -118);
  cairo_line_to(cr, 78, -112);
  cairo_line_to(cr, 78, -6);
  cairo_close_path(cr);
  linear(cr, 0, -118, 0, -6, {{0, rgb(255, 252, 238)}, {1, rgb(236, 228, 206)}}, 1.6);
  selectGameFont(cr);
  cairo_set_font_size(cr, 15);
  cairo_text_extents_t te;
  cairo_text_extents(cr, "ORDER", &te);
  cairo_move_to(cr, -te.x_advance / 2, -94);
  setColor(cr, rgb(120, 110, 100));
  cairo_show_text(cr, "ORDER");
  cairo_set_font_size(cr, 26);
  cairo_text_extents(cr, text.c_str(), &te);
  if (te.x_advance > 140)
  {
    cairo_set_font_size(cr, 26 * 140 / te.x_advance);
    cairo_text_extents(cr, text.c_str(), &te);
  }
  cairo_move_to(cr, -te.x_advance / 2, -62);
  setColor(cr, rgb(30, 30, 40));
  cairo_show_text(cr, text.c_str());
  // "42" circled in red pen, and a scribbled order under it.
  cairo_new_path(cr);
  cairo_save(cr);
  cairo_translate(cr, te.x_advance / 2 - 22, -71);
  cairo_scale(cr, 30, 19);
  cairo_arc(cr, 0, 0, 1, 0.2, 2 * kPi + 0.5);
  cairo_restore(cr);
  setColor(cr, rgb(220, 30, 30));
  cairo_set_line_width(cr, 2.2);
  cairo_stroke(cr);
  cairo_set_font_size(cr, 12);
  cairo_move_to(cr, -62, -38);
  setColor(cr, rgb(60, 60, 90));
  cairo_show_text(cr, "1x SPAGHETTI + MEATBALLS");
  cairo_move_to(cr, -62, -22);
  cairo_show_text(cr, "   (THE ANSWER)");
  // The clip.
  cairo_rectangle(cr, -9, -12, 18, 14);
  steel(cr, -9, 9, 1.2);
}

// A sauce pan, 3 blocks wide (192) + its handle, 64 tall, the rim at y 14.
void paintSaucePan(cairo_t* cr, double w)
{
  // The handle out to the right.
  cairo_move_to(cr, w - 6, 22);
  cairo_line_to(cr, w + 60, 14);
  cairo_line_to(cr, w + 62, 24);
  cairo_line_to(cr, w - 6, 34);
  cairo_close_path(cr);
  linear(cr, 0, 14, 0, 34, {{0, rgb(70, 60, 56)}, {1, rgb(20, 16, 14)}}, 1.6);
  // The pan: chrome walls, a heavy base.
  cairo_move_to(cr, 6, 14);
  cairo_line_to(cr, w - 6, 14);
  cairo_line_to(cr, w - 12, 58);
  cairo_curve_to(cr, w - 14, 62, w - 16, 64, w - 22, 64);
  cairo_line_to(cr, 22, 64);
  cairo_curve_to(cr, 16, 64, 14, 62, 12, 58);
  cairo_close_path(cr);
  steel(cr, 6, w - 6, 2.0);
  cairo_rectangle(cr, 10, 54, w - 20, 8);
  setColor(cr, rgba(60, 50, 50, 120));
  cairo_fill(cr);
  // The rim, and the sauce inside it, seen just over the lip.
  cairo_new_path(cr);
  cairo_save(cr);
  cairo_translate(cr, w / 2, 14);
  cairo_scale(cr, w / 2 - 4, 7);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_restore(cr);
  linear(cr, 0, 7, 0, 21, {{0, rgb(160, 30, 18)}, {1, rgb(230, 60, 36)}}, 2.0);
  cairo_new_path(cr);
  cairo_save(cr);
  cairo_translate(cr, w / 2 - 20, 12);
  cairo_scale(cr, 30, 3);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_restore(cr);
  setColor(cr, rgba(255, 170, 140, 140));
  cairo_fill(cr);
  // Sauce slopped down the side.
  cairo_move_to(cr, 30, 16);
  cairo_curve_to(cr, 32, 26, 28, 30, 31, 34);
  cairo_curve_to(cr, 34, 30, 36, 24, 40, 18);
  cairo_close_path(cr);
  setColor(cr, rgb(206, 42, 26));
  cairo_fill(cr);
}

// The sink, w x h px: a steel basin's back and sides, the drain, and its
// mixer tap arching over from the right.
void paintSink(cairo_t* cr, double w, double h)
{
  cairo_rectangle(cr, 0, 0, w, h);
  linear(cr, 0, 0, 0, h, {{0, rgb(120, 128, 140)}, {0.4, rgb(176, 184, 196)}, {1, rgb(96, 104, 118)}});
  // Brushed lines.
  Rng rng(2453u);
  for (int i = 0; i < 40; ++i)
  {
    const double y = rng.range(0, float(h));
    cairo_move_to(cr, 0, y);
    cairo_line_to(cr, w, y + rng.range(-2, 2));
    setColor(cr, rgba(255, 255, 255, 24));
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);
  }
  // The side walls slanting in.
  for (const int side : {0, 1})
  {
    const double x = side ? w : 0, d = side ? -1 : 1;
    cairo_move_to(cr, x, 0);
    cairo_line_to(cr, x + d * 22, 18);
    cairo_line_to(cr, x + d * 22, h);
    cairo_line_to(cr, x, h);
    cairo_close_path(cr);
    setColor(cr, side ? rgba(40, 44, 54, 140) : rgba(255, 255, 255, 60));
    cairo_fill(cr);
  }
  cairo_new_path(cr);
  cairo_save(cr);
  cairo_translate(cr, w / 2, h - 18);
  cairo_scale(cr, 26, 7);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_restore(cr);
  fillInk(cr, rgb(40, 44, 52), 1.6);
  for (int i = -2; i <= 2; ++i)
  {
    cairo_rectangle(cr, w / 2 + i * 8 - 1.5, h - 22, 3, 8);
    setColor(cr, rgb(120, 126, 140));
    cairo_fill(cr);
  }
}

// The tap, 150 x 170: a gooseneck from its base at (120, 170) arching
// over to the spout at (20, 50), a lever on top.
void paintTap(cairo_t* cr)
{
  cairo_move_to(cr, 120, 170);
  cairo_line_to(cr, 120, 60);
  cairo_curve_to(cr, 120, 0, 20, 0, 20, 50);
  setColor(cr, kInk);
  cairo_set_line_width(cr, 18);
  cairo_stroke_preserve(cr);
  setColor(cr, rgb(200, 208, 220));
  cairo_set_line_width(cr, 13);
  cairo_stroke_preserve(cr);
  setColor(cr, rgba(255, 255, 255, 200));
  cairo_set_line_width(cr, 3);
  cairo_stroke(cr);
  cairo_rectangle(cr, 12, 46, 16, 12);
  steel(cr, 12, 28, 1.6);
  cairo_rectangle(cr, 104, 150, 32, 20);
  steel(cr, 104, 136, 1.6);
  cairo_move_to(cr, 128, 104);
  cairo_line_to(cr, 148, 84);
  setColor(cr, kInk);
  cairo_set_line_width(cr, 9);
  cairo_stroke_preserve(cr);
  setColor(cr, rgb(220, 226, 236));
  cairo_set_line_width(cr, 5);
  cairo_stroke(cr);
}

// The meat grinder the meatballs come out of, 200 x 190: the hopper, the
// barrel, the crank, the cutting plate facing down, its mouth at (100, 190).
void paintGrinder(cairo_t* cr)
{
  // The hopper.
  cairo_move_to(cr, 40, 0);
  cairo_line_to(cr, 160, 0);
  cairo_line_to(cr, 124, 56);
  cairo_line_to(cr, 76, 56);
  cairo_close_path(cr);
  steel(cr, 40, 160, 2.2);
  cairo_new_path(cr);
  cairo_save(cr);
  cairo_translate(cr, 100, 2);
  cairo_scale(cr, 58, 7);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_restore(cr);
  linear(cr, 0, -5, 0, 9, {{0, rgb(160, 60, 40)}, {1, rgb(110, 30, 20)}}, 1.6);
  // The barrel, down to the plate.
  roundedRect(cr, 66, 50, 68, 110, 10);
  steel(cr, 66, 134, 2.2);
  for (int i = 0; i < 4; ++i)
  {
    cairo_rectangle(cr, 66, 70 + i * 20, 68, 3);
    setColor(cr, rgba(60, 64, 76, 150));
    cairo_fill(cr);
  }
  // The crank off the side.
  cairo_move_to(cr, 134, 90);
  cairo_line_to(cr, 180, 90);
  cairo_line_to(cr, 180, 130);
  setColor(cr, kInk);
  cairo_set_line_width(cr, 10);
  cairo_stroke_preserve(cr);
  setColor(cr, rgb(210, 216, 226));
  cairo_set_line_width(cr, 6);
  cairo_stroke(cr);
  roundedRect(cr, 172, 126, 18, 34, 8);
  linear(cr, 172, 0, 190, 0, {{0, rgb(200, 40, 30)}, {1, rgb(120, 20, 14)}}, 1.6);
  // The ring nut and the cutting plate.
  roundedRect(cr, 56, 156, 88, 22, 6);
  steel(cr, 56, 144, 2.2);
  cairo_rectangle(cr, 64, 176, 72, 14);
  setColor(cr, rgb(60, 64, 74));
  cairo_fill(cr);
  for (int i = 0; i < 6; ++i)
  {
    circlePath(cr, 70 + i * 12, 183, 3);
    setColor(cr, rgb(150, 50, 36));
    cairo_fill(cr);
  }
}

// The toaster, w x h: a chrome loaf on a black base, two slots on top, a
// browning dial and the lever's groove on the right.
void paintToaster(cairo_t* cr, double w, double h)
{
  // The base.
  roundedRect(cr, 4, h - 24, w - 8, 24, 8);
  linear(cr, 0, h - 24, 0, h, {{0, rgb(50, 46, 50)}, {1, rgb(14, 12, 14)}}, 2.0);
  // The body: rounded shoulders.
  cairo_move_to(cr, 10, h - 20);
  cairo_line_to(cr, 10, 40);
  cairo_curve_to(cr, 10, 6, 30, 4, 60, 4);
  cairo_line_to(cr, w - 60, 4);
  cairo_curve_to(cr, w - 30, 4, w - 10, 6, w - 10, 40);
  cairo_line_to(cr, w - 10, h - 20);
  cairo_close_path(cr);
  linear(cr, 10, 0, w - 10, 0,
    {{0, rgb(150, 158, 170)}, {0.18, rgb(250, 252, 255)}, {0.35, rgb(196, 204, 214)}, {0.62, rgb(232, 236, 244)},
      {0.85, rgb(130, 138, 150)}, {1, rgb(90, 96, 110)}},
    2.4);
  // Reflections: the studio lights in the chrome.
  for (const double rx : {0.22, 0.27})
  {
    cairo_rectangle(cr, w * rx, 18, 4, h - 50);
    setColor(cr, rgba(255, 255, 255, 140));
    cairo_fill(cr);
  }
  // Ribs along the side.
  for (int i = 0; i < 3; ++i)
  {
    cairo_rectangle(cr, 14, h * 0.45 + i * 12, w - 28, 2.5);
    setColor(cr, rgba(80, 88, 100, 140));
    cairo_fill(cr);
  }
  // The dial.
  circlePath(cr, w * 0.72, h * 0.32, 13);
  linear(cr, 0, h * 0.32 - 13, 0, h * 0.32 + 13, {{0, rgb(70, 66, 70)}, {1, rgb(16, 14, 16)}}, 1.6);
  cairo_move_to(cr, w * 0.72, h * 0.32);
  cairo_line_to(cr, w * 0.72 + 8, h * 0.32 - 8);
  setColor(cr, rgb(255, 200, 80));
  cairo_set_line_width(cr, 2.4);
  cairo_stroke(cr);
  // The lever's groove at the right end.
  roundedRect(cr, w - 22, 26, 8, h - 70, 4);
  setColor(cr, rgb(30, 30, 36));
  cairo_fill(cr);
  // The slots.
  for (const double sx : {0.3, 0.62})
  {
    roundedRect(cr, w * sx - 40, 0, 80, 10, 4);
    setColor(cr, rgb(24, 22, 24));
    cairo_fill(cr);
  }
}

// A slice of toast, 70 x 80, its bottom at y 80.
void paintToast(cairo_t* cr)
{
  cairo_move_to(cr, 6, 80);
  cairo_line_to(cr, 6, 26);
  cairo_curve_to(cr, 2, 6, 22, 2, 35, 10);
  cairo_curve_to(cr, 48, 2, 68, 6, 64, 26);
  cairo_line_to(cr, 64, 80);
  cairo_close_path(cr);
  linear(cr, 0, 2, 0, 80, {{0, rgb(150, 86, 30)}, {1, rgb(120, 62, 20)}}, 2.0);
  cairo_move_to(cr, 13, 76);
  cairo_line_to(cr, 13, 28);
  cairo_curve_to(cr, 11, 14, 26, 10, 35, 17);
  cairo_curve_to(cr, 44, 10, 59, 14, 57, 28);
  cairo_line_to(cr, 57, 76);
  cairo_close_path(cr);
  linear(cr, 0, 10, 0, 76, {{0, rgb(236, 176, 90)}, {1, rgb(214, 150, 66)}});
  Rng rng(2459u);
  for (int i = 0; i < 18; ++i)
  {
    circlePath(cr, rng.range(16, 54), rng.range(22, 72), rng.range(0.6f, 1.4f));
    setColor(cr, rgba(150, 90, 30, 160));
    cairo_fill(cr);
  }
}

// A louvred pantry door, 56 x hpx: slats, a brass push plate, a little
// PANTRY plaque. open: drawn narrow, swung away.
void paintFlap(cairo_t* cr, double w, double h, const Theme& t)
{
  roundedRect(cr, 0, 0, w, h, 4);
  linear(cr, 0, 0, w, 0, {{0, lerpColor(t.platform, rgb(255, 255, 255), 0.15f)}, {1, t.platformDark}}, 2.2);
  if (w < 30)
    return;
  for (double y = 18; y < h - 14; y += 12)
  {
    cairo_move_to(cr, 7, y);
    cairo_line_to(cr, w - 7, y + 5);
    setColor(cr, lerpColor(t.platformDark, rgb(0, 0, 0), 0.35f));
    cairo_set_line_width(cr, 3);
    cairo_stroke(cr);
    cairo_move_to(cr, 7, y + 2);
    cairo_line_to(cr, w - 7, y + 7);
    setColor(cr, lerpColor(t.platform, rgb(255, 255, 255), 0.3f));
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);
  }
  roundedRect(cr, w * 0.5 - 10, h * 0.42, 20, 30, 3);
  linear(cr, 0, h * 0.42, 0, h * 0.42 + 30, {{0, rgb(255, 230, 140)}, {1, rgb(170, 120, 30)}}, 1.4);
  roundedRect(cr, 5, 4, w - 10, 12, 2);
  fillInk(cr, rgb(250, 240, 210), 1.0);
  selectGameFont(cr);
  cairo_set_font_size(cr, 7.5);
  cairo_text_extents_t te;
  cairo_text_extents(cr, "PANTRY", &te);
  cairo_move_to(cr, w / 2 - te.x_advance / 2, 13);
  setColor(cr, rgb(90, 50, 20));
  cairo_show_text(cr, "PANTRY");
}

// The stock pot on the stove, w x h: chrome, two handles, a rolled rim,
// soup glimpsed over it, a riveted band.
void paintStockPot(cairo_t* cr, double w, double h)
{
  for (const int side : {0, 1})
  {
    const double x = side ? w - 6 : 6;
    cairo_new_path(cr);
    cairo_save(cr);
    cairo_translate(cr, x, 40);
    cairo_scale(cr, 18, 12);
    cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
    cairo_restore(cr);
    setColor(cr, kInk);
    cairo_set_line_width(cr, 9);
    cairo_stroke_preserve(cr);
    setColor(cr, rgb(210, 216, 226));
    cairo_set_line_width(cr, 5);
    cairo_stroke(cr);
  }
  cairo_rectangle(cr, 16, 8, w - 32, h - 12);
  steel(cr, 16, w - 16, 2.4);
  cairo_rectangle(cr, 16, h - 16, w - 32, 12);
  setColor(cr, rgba(40, 30, 30, 90));
  cairo_fill(cr);
  cairo_rectangle(cr, 16, 60, w - 32, 10);
  setColor(cr, rgba(90, 96, 110, 160));
  cairo_fill(cr);
  for (double x = 28; x < w - 24; x += 26)
  {
    circlePath(cr, x, 65, 2.4);
    setColor(cr, rgb(250, 252, 255));
    cairo_fill(cr);
  }
  // The rolled rim, and the soup just showing.
  roundedRect(cr, 8, 0, w - 16, 14, 7);
  steelV(cr, 0, 14, 2.4);
  // A label stuck on the side.
  roundedRect(cr, w * 0.5 - 50, h * 0.55, 100, 44, 6);
  fillInk(cr, rgb(250, 244, 228), 1.6);
  selectGameFont(cr);
  cairo_set_font_size(cr, 14);
  cairo_text_extents_t te;
  cairo_text_extents(cr, "STOCK", &te);
  cairo_move_to(cr, w * 0.5 - te.x_advance / 2, h * 0.55 + 28);
  setColor(cr, rgb(180, 40, 30));
  cairo_show_text(cr, "STOCK");
}

// A pot hung under its rim, w x h: rim at top, belly, a chain up from each
// handle.
void paintHangingPot(cairo_t* cr, double w, double h)
{
  cairo_move_to(cr, 14, 8);
  cairo_line_to(cr, w - 14, 8);
  cairo_line_to(cr, w - 20, h - 22);
  cairo_curve_to(cr, w - 24, h - 2, w - 40, h, w / 2, h);
  cairo_curve_to(cr, 40, h, 24, h - 2, 20, h - 22);
  cairo_close_path(cr);
  linear(cr, 14, 0, w - 14, 0,
    {{0, rgb(160, 80, 40)}, {0.3, rgb(250, 170, 110)}, {0.6, rgb(200, 110, 60)}, {1, rgb(110, 52, 26)}}, 2.2);
  roundedRect(cr, 6, 0, w - 12, 13, 6);
  linear(cr, 0, 0, 0, 13, {{0, rgb(255, 210, 160)}, {1, rgb(140, 70, 36)}}, 2.0);
  for (const double hx : {10.0, w - 10.0})
  {
    circlePath(cr, hx, 6, 5);
    setColor(cr, kInk);
    cairo_set_line_width(cr, 2.0);
    cairo_stroke(cr);
  }
}

// The cutlery basket, w x h: a coated-wire grid with compartments, a top
// rail, and forks, spoons and knives standing in it (their handles up).
void paintBasket(cairo_t* cr, double w, double h)
{
  Rng rng(2467u);
  // Cutlery first (behind the front wires): handles poking up out of it.
  for (double x = 30; x < w - 30; x += rng.range(24, 40))
  {
    const int kind = int(rng.uniform() * 3.0f);
    const double top = rng.range(-90, -40), lean = rng.range(-0.12f, 0.12f);
    cairo_save(cr);
    cairo_translate(cr, x, h * 0.6);
    cairo_rotate(cr, lean);
    roundedRect(cr, -6, top, 12, h * 0.6 - top, 5);
    linear(cr, -6, 0, 6, 0, {{0, rgb(250, 252, 255)}, {0.5, rgb(190, 198, 210)}, {1, rgb(120, 128, 142)}}, 1.6);
    if (kind == 0)
    {
      // A fork's tines.
      for (int k = -1; k <= 1; ++k)
      {
        cairo_rectangle(cr, k * 5 - 1.2, top - 26, 2.4, 28);
        setColor(cr, rgb(200, 206, 218));
        cairo_fill_preserve(cr);
        setColor(cr, kInk);
        cairo_set_line_width(cr, 0.8);
        cairo_stroke(cr);
      }
    }
    else if (kind == 1)
    {
      cairo_new_path(cr);
      cairo_save(cr);
      cairo_translate(cr, 0, top - 14);
      cairo_scale(cr, 11, 16);
      cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
      cairo_restore(cr);
      linear(cr, -11, 0, 11, 0, {{0, rgb(250, 252, 255)}, {1, rgb(130, 138, 150)}}, 1.6);
    }
    else
    {
      cairo_move_to(cr, -6, top);
      cairo_line_to(cr, -6, top - 40);
      cairo_curve_to(cr, 0, top - 46, 6, top - 30, 6, top);
      cairo_close_path(cr);
      linear(cr, -6, 0, 6, 0, {{0, rgb(250, 252, 255)}, {1, rgb(150, 158, 170)}}, 1.4);
    }
    cairo_restore(cr);
  }
  // The wire grid.
  for (double x = 6; x <= w - 6; x += 16)
  {
    cairo_move_to(cr, x, 10);
    cairo_line_to(cr, x, h - 6);
  }
  for (double y = 10; y <= h - 6; y += 16)
  {
    cairo_move_to(cr, 6, y);
    cairo_line_to(cr, w - 6, y);
  }
  setColor(cr, rgba(220, 226, 236, 200));
  cairo_set_line_width(cr, 2.2);
  cairo_stroke(cr);
  // Compartment walls and the frame.
  for (double x = 6; x <= w - 6; x += (w - 12) / 4)
  {
    cairo_move_to(cr, x, 6);
    cairo_line_to(cr, x, h - 4);
  }
  roundedRect(cr, 4, 4, w - 8, h - 8, 8);
  setColor(cr, kInk);
  cairo_set_line_width(cr, 6);
  cairo_stroke_preserve(cr);
  setColor(cr, rgb(236, 240, 246));
  cairo_set_line_width(cr, 3.5);
  cairo_stroke(cr);
  // The handle over the top.
  cairo_move_to(cr, w * 0.4, 6);
  cairo_curve_to(cr, w * 0.42, -40, w * 0.58, -40, w * 0.6, 6);
  setColor(cr, kInk);
  cairo_set_line_width(cr, 7);
  cairo_stroke_preserve(cr);
  setColor(cr, rgb(236, 240, 246));
  cairo_set_line_width(cr, 4);
  cairo_stroke(cr);
}

// The countertop oven, w x h, with its door's window cut out (wx0..wx1,
// wy0..wy1 left transparent but tinted): enamel body, a chrome handle over
// the door, dials and a clock on the right, little feet.
void paintOven(cairo_t* cr, double w, double h, double wx0, double wy0, double wx1, double wy1)
{
  roundedRect(cr, 4, 4, w - 8, h - 14, 14);
  linear(cr, 0, 4, 0, h - 10, {{0, rgb(232, 70, 56)}, {0.5, rgb(206, 44, 36)}, {1, rgb(140, 24, 20)}}, 2.6);
  // The window cut out: the inside is dark glass, tinted.
  cairo_save(cr);
  cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
  roundedRect(cr, wx0, wy0, wx1 - wx0, wy1 - wy0, 10);
  cairo_fill(cr);
  cairo_restore(cr);
  roundedRect(cr, wx0, wy0, wx1 - wx0, wy1 - wy0, 10);
  setColor(cr, rgba(60, 20, 10, 110));
  cairo_fill_preserve(cr);
  setColor(cr, kInk);
  cairo_set_line_width(cr, 3);
  cairo_stroke(cr);
  // The door's frame round the window, and its handle.
  roundedRect(cr, wx0 - 10, wy0 - 30, wx1 - wx0 + 20, wy1 - wy0 + 40, 14);
  setColor(cr, rgba(255, 255, 255, 50));
  cairo_set_line_width(cr, 4);
  cairo_stroke(cr);
  roundedRect(cr, wx0 + 10, wy0 - 24, wx1 - wx0 - 20, 12, 6);
  steelV(cr, wy0 - 24, wy0 - 12, 2.0);
  // The glass: a reflection streak.
  cairo_move_to(cr, wx0 + 20, wy1 - 10);
  cairo_line_to(cr, wx0 + 60, wy0 + 10);
  cairo_line_to(cr, wx0 + 76, wy0 + 10);
  cairo_line_to(cr, wx0 + 36, wy1 - 10);
  cairo_close_path(cr);
  setColor(cr, rgba(255, 255, 255, 40));
  cairo_fill(cr);
  // The heating elements, glowing.
  for (const double ey : {wy0 + 12, wy1 - 12})
  {
    cairo_move_to(cr, wx0 + 12, ey);
    for (double x = wx0 + 12; x < wx1 - 12; x += 12)
      cairo_line_to(cr, x + 6, ey + ((int(x) / 12) % 2 ? 3 : -3));
    setColor(cr, rgba(255, 110, 40, 230));
    cairo_set_line_width(cr, 3);
    cairo_stroke(cr);
  }
  // Dials and the clock on the right strip.
  const double px = (wx1 + w) * 0.5;
  for (int i = 0; i < 3; ++i)
  {
    const double dy = wy0 + 10 + i * 50;
    circlePath(cr, px, dy + 14, 14);
    linear(cr, 0, dy, 0, dy + 28, {{0, rgb(250, 250, 250)}, {1, rgb(170, 170, 176)}}, 1.8);
    cairo_move_to(cr, px, dy + 14);
    cairo_line_to(cr, px + std::cos(i * 1.7 - 1.0) * 10, dy + 14 + std::sin(i * 1.7 - 1.0) * 10);
    setColor(cr, kInk);
    cairo_set_line_width(cr, 2.4);
    cairo_stroke(cr);
  }
  // The brand.
  selectGameFont(cr, true);
  cairo_set_font_size(cr, 15);
  cairo_text_extents_t te;
  cairo_text_extents(cr, "BAKE-O-MATIC", &te);
  cairo_move_to(cr, (wx0 + wx1) / 2 - te.x_advance / 2, h - 18);
  setColor(cr, rgb(255, 230, 210));
  cairo_show_text(cr, "BAKE-O-MATIC");
  // Feet.
  for (const double fx : {20.0, w - 44})
  {
    roundedRect(cr, fx, h - 14, 24, 14, 4);
    fillInk(cr, rgb(30, 28, 30), 1.6);
  }
}

// A cleaver stuck in a wall: the blade's tip buried (cut off at the wall),
// the handle out, 130 x 64, the wall face at x 112, the handle's top at y 8.
void paintStuckCleaver(cairo_t* cr)
{
  cairo_save(cr);
  cairo_rectangle(cr, 0, 0, 112, 64);
  cairo_clip(cr);
  cairo_translate(cr, 128 - 120, 26);
  paintCleaver(cr, 120);
  cairo_restore(cr);
  // The slit it cut in the wall.
  cairo_rectangle(cr, 110, 0, 5, 50);
  setColor(cr, rgba(20, 10, 6, 200));
  cairo_fill(cr);
  for (const double cy : {8.0, 44.0})
  {
    cairo_move_to(cr, 113, cy);
    cairo_line_to(cr, 122, cy + (cy < 20 ? -6 : 6));
  }
  setColor(cr, rgba(60, 40, 30, 200));
  cairo_set_line_width(cr, 1.4);
  cairo_stroke(cr);
}

// A dishwasher rack, w x 36: coated wire, the top rail at y 0, tines,
// wheels at the ends; the inside filled dark (the tub behind).
void paintRack(cairo_t* cr, double w)
{
  cairo_rectangle(cr, 2, 4, w - 4, 26);
  setColor(cr, rgba(70, 78, 92, 230));
  cairo_fill(cr);
  for (double x = 6; x < w - 4; x += 8)
  {
    cairo_move_to(cr, x, 4);
    cairo_line_to(cr, x, 30);
  }
  setColor(cr, rgb(196, 204, 216));
  cairo_set_line_width(cr, 1.6);
  cairo_stroke(cr);
  for (const double y : {2.0, 16.0, 30.0})
  {
    cairo_rectangle(cr, 0, y - 2, w, 4);
    linear(cr, 0, y - 2, 0, y + 2, {{0, rgb(255, 255, 255)}, {1, rgb(150, 158, 172)}});
  }
  cairo_rectangle(cr, 0, 0, w, 30);
  setColor(cr, kInk);
  cairo_set_line_width(cr, 1.6);
  cairo_stroke(cr);
  for (const double wx : {14.0, w - 14})
  {
    circlePath(cr, wx, 32, 5);
    fillInk(cr, rgb(60, 60, 70), 1.4);
  }
}

// One wall block of the microwave's cavity, 64 x 64: off-white enamel,
// a faint seam.
void paintCavityWall(cairo_t* cr, int variant)
{
  cairo_rectangle(cr, 0, 0, 64, 64);
  linear(cr, 0, 0, 64, 64, {{0, rgb(255, 254, 250)}, {0.5, rgb(236, 234, 228)}, {1, rgb(196, 192, 184)}});
  // A bevel: light on two edges, shade on the others.
  cairo_rectangle(cr, 0, 0, 64, 4);
  cairo_rectangle(cr, 0, 0, 4, 64);
  setColor(cr, rgba(255, 255, 255, 200));
  cairo_fill(cr);
  cairo_rectangle(cr, 0, 60, 64, 4);
  cairo_rectangle(cr, 60, 0, 4, 64);
  setColor(cr, rgba(90, 84, 76, 150));
  cairo_fill(cr);
  cairo_rectangle(cr, 0, 0, 64, 64);
  setColor(cr, rgba(60, 54, 48, 160));
  cairo_set_line_width(cr, 1.5);
  cairo_stroke(cr);
  if (variant == 1)
  {
    // A splat of something that exploded in here once.
    for (int i = 0; i < 6; ++i)
    {
      circlePath(cr, 30 + std::cos(i * 1.1) * (i ? 9 : 0), 34 + std::sin(i * 1.1) * (i ? 7 : 0), i ? 3.0 : 7.0);
      setColor(cr, rgba(220, 120, 40, 160));
      cairo_fill(cr);
    }
  }
  else if (variant == 2)
  {
    // Vent holes.
    for (int y = 0; y < 4; ++y)
      for (int x = 0; x < 4; ++x)
      {
        circlePath(cr, 14 + x * 12, 14 + y * 12, 2.4);
        setColor(cr, rgba(80, 70, 60, 150));
        cairo_fill(cr);
      }
  }
}

// The back of the cavity, 64 x 64: darker enamel with a soft pattern.
void paintCavityBack(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 0, 64, 64);
  setColor(cr, rgb(170, 156, 134));
  cairo_fill(cr);
  for (int y = 0; y < 4; ++y)
    for (int x = 0; x < 4; ++x)
    {
      circlePath(cr, 8 + x * 16, 8 + y * 16, 1.6);
      setColor(cr, rgba(150, 140, 122, 160));
      cairo_fill(cr);
    }
}

// The door's mesh, 96 x 96: a tile of perforations, and its glass sheen.
void paintDoorMesh(cairo_t* cr)
{
  for (int y = 0; y < 8; ++y)
    for (int x = 0; x < 8; ++x)
    {
      const double ox = (y % 2) * 6.0;
      circlePath(cr, ox + x * 12 + 3, y * 12 + 6, 2.2);
    }
  setColor(cr, rgba(10, 10, 14, 40));
  cairo_fill(cr);
}

} // namespace

// --- Behind the runner ---------------------------------------------------------------

void World::drawKitchenBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& k = mKitchen;
  auto sx = [&](float bx) { return bx * kTilePx - camX; };
  auto sy = [&](float by) { return by * kTilePx - camY; };
  const bool kitchenSet = mLevel->themeKey == "tv_kitchen";

  // Pots hung under the one-way rims three blocks wide.
  if (kitchenSet)
  {
    const int tx0 = std::max(0, int(camX / kTilePx) - 2), tx1 = std::min(mLevel->width - 1, int((camX + float(kScreenW)) / kTilePx) + 2);
    const int ty0 = std::max(0, int(camY / kTilePx) - 1), ty1 = std::min(mLevel->height - 1, int((camY + float(kScreenH)) / kTilePx) + 1);
    const Texture& pot = baked(mArt, r, "hangpot", 3 * 64, 110, 0.0f, 0.0f, [](cairo_t* cr) { paintHangingPot(cr, 192, 110); });
    for (int by = ty0; by <= ty1; ++by)
      for (int bx = tx0; bx <= tx1; ++bx)
      {
        if (mMap.block(bx, by) != Tile::Platform || mMap.block(bx - 1, by) == Tile::Platform)
          continue;
        int len = 0;
        while (mMap.block(bx + len, by) == Tile::Platform)
          ++len;
        if (len != 3)
          continue;
        bool cleaverHere = false;
        for (const auto& c : k.cleavers)
          cleaverHere = cleaverHere || (c.by == by && c.bx >= bx && c.bx < bx + 3);
        if (cleaverHere)
          continue;
        const float x = sx(float(bx)), y = sy(float(by));
        r.drawLine(x + 10.0f, y + 4.0f, x + 96.0f, y - 140.0f, 3.0f, rgba(60, 56, 60, 220));
        r.drawLine(x + 182.0f, y + 4.0f, x + 96.0f, y - 140.0f, 3.0f, rgba(60, 56, 60, 220));
        r.draw(pot, x, y);
      }
  }

  // The set dressing.
  for (const auto& d : k.decos)
  {
    const float x = sx(float(d.r.x0)), y = sy(float(d.r.y0));
    const float w = float(d.r.x1 - d.r.x0 + 1) * kTilePx, h = float(d.r.y1 - d.r.y0 + 1) * kTilePx;
    if (offScreen(x - 64.0f, y - 160.0f, w + 128.0f, h + 200.0f))
      continue;
    const std::string size = std::to_string(int(w)) + "x" + std::to_string(int(h));
    if (d.kind == "pot")
    {
      // The burner's flames under it, the pot, steam off the top.
      for (int i = 0; i < 9; ++i)
      {
        const float fx = x + 14.0f + float(i) * (w - 28.0f) / 8.0f;
        const float fl = 14.0f + 6.0f * std::sin(float(frame) * 0.5f + float(i) * 1.7f);
        r.drawLine(fx, y + h, fx, y + h - fl, 7.0f, rgba(80, 140, 255, 200), Blend::Add);
        r.drawLine(fx, y + h, fx, y + h - fl * 0.5f, 4.0f, rgba(200, 230, 255, 220), Blend::Add);
      }
      drawGlow(r, mArt, x + w * 0.5f, y + h, w * 0.6f, rgb(90, 140, 255), 0.35f);
      r.draw(baked(mArt, r, "pot/" + size, int(w), int(h) - 4, 0.0f, 0.0f,
               [&](cairo_t* cr) { paintStockPot(cr, double(w), double(h) - 4); }),
        x, y);
      for (int i = 0; i < 4; ++i)
      {
        const float u = std::fmod(float(frame) * 0.006f + float(i) * 0.25f, 1.0f);
        drawGlow(r, mArt, x + w * (0.3f + 0.15f * float(i)) + std::sin(u * 6.0f + float(i)) * 14.0f, y - u * 150.0f,
          30.0f + u * 40.0f, rgb(255, 255, 255), 0.22f * (1.0f - u));
      }
    }
    else if (d.kind == "basket")
      r.draw(baked(mArt, r, "basket/" + size, int(w), int(h) + 110, 0.0f, 110.0f,
               [&](cairo_t* cr) {
                 cairo_translate(cr, 0, 110);
                 paintBasket(cr, double(w), double(h));
               }),
        x, y);
    else if (d.kind == "oven")
    {
      // The window round the exit (the oven light): centred on it.
      const float ex = float(mLevel->exitTx) * kTilePx + 32.0f - x;
      const float wx0 = std::clamp(ex - 104.0f, 16.0f, w - 120.0f), wx1 = std::min(ex + 104.0f, w - 70.0f);
      const float wy0 = 44.0f, wy1 = h - 26.0f;
      drawGlow(r, mArt, x + (wx0 + wx1) * 0.5f, y + (wy0 + wy1) * 0.5f, 170.0f, rgb(255, 150, 60),
        0.35f + 0.08f * std::sin(float(frame) * 0.08f));
      r.draw(baked(mArt, r, "oven/" + size, int(w), int(h), 0.0f, 0.0f,
               [&](cairo_t* cr) { paintOven(cr, double(w), double(h), wx0, wy0, wx1, wy1); }),
        x, y);
      drawGlow(r, mArt, x + (wx0 + wx1) * 0.5f, y + wy1 - 30.0f, 120.0f, rgb(255, 170, 80), 0.3f);
    }
  }

  // The TABLE 42 ticket, clipped to the order rail under it.
  for (const auto& pr : mProps)
  {
    if (pr.kind != PropKind::Deco42 || !kitchenSet)
      continue;
    const float x = float(pr.x) * kCellPx - camX + float(pr.w) * kCellPx * 0.5f, y = float(pr.y + pr.h) * kCellPx - camY + 10.0f;
    if (offScreen(x - 90.0f, y - 130.0f, 180.0f, 140.0f))
      continue;
    DrawOpts o;
    o.angle = std::sin(float(frame) * 0.05f) * 2.0f;
    r.draw(baked(mArt, r, "ticket42/" + pr.text, 180, 130, 90.0f, 124.0f, [&](cairo_t* cr) { paintTicket42(cr, pr.text); }),
      x, y, o);
  }

  // The meat grinder over each hatch; it shudders as it is about to drop one.
  for (const auto& h : k.hatches)
  {
    const float mx = (float(h.x) + 1.5f) * kCellPx - camX, my = float(h.y - 2) * kCellPx - camY;
    if (offScreen(mx - 100.0f, my - 190.0f, 200.0f, 200.0f))
      continue;
    const bool soon = h.t > h.every - 8 && int(h.balls.size()) < h.max;
    const float shake = soon ? ((frame / 2) % 2 ? 2.0f : -2.0f) : 0.0f;
    r.draw(baked(mArt, r, "grinder", 200, 190, 100.0f, 190.0f, [](cairo_t* cr) { paintGrinder(cr); }), mx + shake, my);
    if (soon)
      drawGlow(r, mArt, mx, my - 6.0f, 40.0f, rgb(255, 120, 80), 0.4f);
  }

  // Sauce pans: bubbling, steaming.
  const Texture* pan = nullptr;
  for (const auto& s : k.sauces)
  {
    const float x = sx(float(s.x0)), y = sy(float(s.y0));
    const float w = float(s.x1 - s.x0 + 1) * kTilePx;
    if (offScreen(x, y - 100.0f, w + 70.0f, 170.0f))
      continue;
    pan = &baked(mArt, r, "saucepan/" + std::to_string(int(w)), int(w) + 66, 66, 0.0f, 0.0f,
      [&](cairo_t* cr) { paintSaucePan(cr, double(w)); });
    r.draw(*pan, x, y);
    for (int i = 0; i < 5; ++i)
    {
      const int t = (frame + i * 13) % 40;
      const float bx = x + 24.0f + float((i * 37) % int(std::max(1.0f, w - 48.0f)));
      const float rr = 3.0f + float(t) * 0.18f;
      if (t < 30)
        drawGlow(r, mArt, bx, y + 14.0f, rr * 2.0f, rgb(255, 120, 90), 0.5f);
      else
        r.fillRect(bx - rr, y + 12.0f, rr * 2.0f, 3.0f, rgba(255, 170, 140, 160));
    }
    for (int i = 0; i < 3; ++i)
    {
      const float u = std::fmod(float(frame) * 0.01f + float(i) * 0.33f, 1.0f);
      drawGlow(r, mArt, x + w * (0.25f + 0.25f * float(i)), y + 6.0f - u * 90.0f, 24.0f + u * 30.0f, rgb(255, 255, 255),
        0.2f * (1.0f - u));
    }
  }

  // The sink: the basin, the water with its ripples, the tap dripping.
  for (const auto& s : k.sinks)
  {
    const float x = sx(float(s.x0)), y = sy(float(s.y0));
    const float w = float(s.x1 - s.x0 + 1) * kTilePx, h = float(s.y1 - s.y0 + 1) * kTilePx;
    if (offScreen(x, y - 300.0f, w, h + 300.0f))
      continue;
    r.draw(baked(mArt, r, "sink/" + std::to_string(int(w)) + "x" + std::to_string(int(h)), int(w), int(h), 0.0f, 0.0f,
             [&](cairo_t* cr) { paintSink(cr, double(w), double(h)); }),
      x, y);
    const float wy = y + h * 0.45f;
    r.fillRect(x, wy, w, y + h - wy, rgba(110, 180, 220, 120));
    for (int i = 0; i < 4; ++i)
    {
      const float ph = std::fmod(float(frame) * 0.02f + float(i) * 0.25f, 1.0f);
      const float rw = 20.0f + ph * (w * 0.4f);
      r.fillRect(x + w * 0.5f - rw, wy - 1.0f, rw * 2.0f, 2.0f, rgba(230, 250, 255, int(160 * (1.0f - ph))));
    }
    // Suds.
    for (int i = 0; i < 9; ++i)
      drawGlow(r, mArt, x + 14.0f + float((i * 53) % int(w - 20.0f)), wy + std::sin(float(frame) * 0.07f + float(i)) * 3.0f,
        16.0f, rgb(255, 255, 255), 0.35f);
    const float tapX = x + w - 150.0f, tapY = y - 230.0f;
    r.draw(baked(mArt, r, "tap", 150, 170, 0.0f, 0.0f, [](cairo_t* cr) { paintTap(cr); }), tapX + 60.0f, tapY);
    const float drip = std::fmod(float(frame) * 4.0f, 260.0f);
    if (drip < 200.0f)
      drawGlow(r, mArt, tapX + 80.0f, tapY + 62.0f + drip, 8.0f, rgb(200, 240, 255), 0.9f);
  }

  // The toaster: its lever, the toast in its slots (warming, then popped).
  if (k.toaster.x0 >= 0)
  {
    const float x = sx(float(k.toaster.x0)), y = sy(float(k.toaster.y0));
    const float w = float(k.toaster.x1 - k.toaster.x0 + 1) * kTilePx, h = float(k.toaster.y1 - k.toaster.y0 + 1) * kTilePx;
    if (!offScreen(x, y - 300.0f, w + 64.0f, h + 300.0f))
    {
      const Texture& toast = baked(mArt, r, "toast", 70, 80, 35.0f, 80.0f, [](cairo_t* cr) { paintToast(cr); });
      float lift = 22.0f; // the slices just peeking out
      if (k.toast > 0)
        lift = 0.0f;
      if (k.popped > 0)
      {
        const float u = float(20 - k.popped) + alpha;
        lift = std::max(18.0f, 22.0f + u * 46.0f - u * u * 2.4f);
      }
      for (const float fx : {0.3f, 0.62f})
      {
        DrawOpts o;
        o.angle = k.popped > 0 ? (fx < 0.5f ? -1.0f : 1.0f) * float(20 - k.popped) * 1.5f : 0.0f;
        r.draw(toast, x + w * fx, y + 70.0f - lift, o);
      }
      r.draw(baked(mArt, r, "toaster/" + std::to_string(int(w)) + "x" + std::to_string(int(h)), int(w), int(h), 0.0f, 0.0f,
               [&](cairo_t* cr) { paintToaster(cr, double(w), double(h)); }),
        x, y);
      if (k.toast > 0)
        for (const float fx : {0.3f, 0.62f})
          drawGlow(r, mArt, x + w * fx, y + 4.0f, 50.0f, rgb(255, 110, 30), 0.5f + 0.2f * std::sin(float(frame) * 0.4f));
      if (k.leverX >= 0)
      {
        // The lever's knob in the groove at the toaster's end.
        const float lx = x + w - 18.0f, ly = y + (k.toast > 0 ? h - 56.0f : 36.0f);
        r.fillRect(lx - 4.0f, ly - 4.0f, 30.0f, 16.0f, kInk);
        r.fillRect(lx - 2.0f, ly - 2.0f, 26.0f, 12.0f, rgb(50, 46, 50));
        r.fillRect(lx, ly - 1.0f, 22.0f, 3.0f, rgba(255, 255, 255, 90));
      }
    }
  }

  // Belts: housing, rollers, the rubber top and its chevrons; the order
  // rail, a chrome bar with tickets riding it.
  const Texture& block = baked(mArt, r, "beltblock", 64, 64, 0.0f, 0.0f, [](cairo_t* cr) { paintBeltBlock(cr); });
  const Texture& roller = baked(mArt, r, "roller", 28, 28, 14.0f, 14.0f, [](cairo_t* cr) { paintRoller(cr); });
  const Texture& endR = baked(mArt, r, "beltend_r", 40, 64, 0.0f, 0.0f, [](cairo_t* cr) { paintBeltEnd(cr); });
  const Texture& endL = baked(mArt, r, "beltend_l", 40, 64, 40.0f, 0.0f, [](cairo_t* cr) {
    cairo_translate(cr, 40, 0);
    cairo_scale(cr, -1, 1);
    paintBeltEnd(cr);
  });
  const Texture& chevR = baked(mArt, r, "chev_r", 34, 20, 17.0f, 10.0f, [](cairo_t* cr) { paintChevron(cr); });
  const Texture& chevL = baked(mArt, r, "chev_l", 34, 20, 17.0f, 10.0f, [](cairo_t* cr) {
    cairo_translate(cr, 34, 0);
    cairo_scale(cr, -1, 1);
    paintChevron(cr);
  });
  for (std::size_t bi = 0; bi < k.belts.size(); ++bi)
  {
    const auto& b = k.belts[bi];
    const float x = sx(float(b.x0)), y = sy(float(b.y));
    const float w = float(b.x1 - b.x0 + 1) * kTilePx;
    if (offScreen(x - 40.0f, y - 64.0f, w + 80.0f, 128.0f))
      continue;
    const float sub = (float(mStats.frames % std::max(1, b.period)) + alpha) / float(std::max(1, b.period));
    const float pos = (float(b.phase) + float(b.dir) * sub) * kCellPx; // px the belt has run
    const bool flash = b.flash > 0;
    const bool on = flash && (frame / 3) % 2 == 0;
    if (b.hang)
    {
      // Brackets up into the dark, the bar, the tickets on it.
      for (float bx = x + 32.0f; bx < x + w; bx += 3.0f * kTilePx)
        r.drawLine(bx, y + 8.0f, bx, y - 200.0f, 4.0f, rgba(70, 70, 80, 220));
      for (int i = 0; i < 7; ++i)
      {
        float tx = std::fmod(float(i) * 3.0f * kTilePx + pos * 1.0f, w);
        if (tx < 0.0f)
          tx += w;
        DrawOpts o;
        o.angle = std::sin(float(frame) * 0.12f + float(i)) * 6.0f;
        r.draw(baked(mArt, r, "ticket/" + std::to_string(i % 3), 40, 60, 20.0f, 0.0f,
                 [i](cairo_t* cr) { paintTicket(cr, i, i % 3 == 0 ? "TABLE 7" : (i % 3 == 1 ? "TABLE 12" : "TO GO")); }),
          x + tx, y + 10.0f, o);
      }
      r.fillRect(x - 6.0f, y + 2.0f, w + 12.0f, 14.0f, kInk);
      r.fillRect(x - 4.0f, y + 4.0f, w + 8.0f, 10.0f, rgb(196, 204, 216));
      r.fillRect(x - 4.0f, y + 5.0f, w + 8.0f, 3.0f, rgb(250, 252, 255));
      r.fillRect(x - 4.0f, y + 11.0f, w + 8.0f, 3.0f, rgb(120, 128, 142));
      if (flash)
        r.fillRect(x, y + 6.0f, w, 4.0f, rgba(255, 210, 90, on ? 200 : 80), Blend::Add);
      continue;
    }
    for (float tx = 0.0f; tx < w - 1.0f; tx += kTilePx)
      r.draw(block, x + tx, y);
    // Rollers in the skirt, turning with the belt.
    DrawOpts ro;
    ro.angle = pos / 13.0f * 57.2958f;
    for (float rx = 16.0f; rx < w; rx += 32.0f)
      r.draw(roller, x + rx, y + 37.0f, ro);
    r.draw(endL, x, y);
    r.draw(endR, x + w, y);
    // Chevrons: one every two blocks, scrolling the belt's way.
    float shift = std::fmod(pos, 2.0f * kTilePx);
    if (shift < 0.0f)
      shift += 2.0f * kTilePx;
    DrawOpts co;
    co.tint = on ? kAmber : (flash ? rgb(255, 240, 200) : rgb(236, 226, 160));
    co.alpha = 0.9f;
    for (float ax = shift - 2.0f * kTilePx + kTilePx; ax < w; ax += 2.0f * kTilePx)
      if (ax > 14.0f && ax < w - 14.0f)
        r.draw(b.dir > 0 ? chevR : chevL, x + ax, y + 8.0f, co);
    // The lamps along the skirt: amber blinking while a bell has it flipping.
    if (flash)
      for (float lx = 32.0f; lx < w; lx += 2.0f * kTilePx)
      {
        r.fillRect(x + lx - 6.0f, y + 54.0f, 12.0f, 6.0f, on ? kAmber : rgb(120, 80, 20));
        if (on)
          drawGlow(r, mArt, x + lx, y + 57.0f, 30.0f, kAmber, 0.7f);
      }
  }

  // The dishwasher racks (moving platforms here): coated wire on wheels.
  if (kitchenSet)
    for (const auto& pl : mPlatforms)
    {
      if (pl.once || pl.mode == PlatformMode::Pulley)
        continue;
      const float x = (float(pl.prevX) + (float(pl.x) - float(pl.prevX)) * alpha) * kCellPx - camX;
      const float y = (float(pl.prevY) + (float(pl.y) - float(pl.prevY)) * alpha) * kCellPx - camY;
      const float w = float(pl.w) * kCellPx;
      if (offScreen(x, y, w, 64.0f))
        continue;
      r.draw(baked(mArt, r, "rack/" + std::to_string(int(w)), int(w), 40, 0.0f, 0.0f, [&](cairo_t* cr) { paintRack(cr, double(w)); }),
        x, y);
    }

  // Bells: brass, hung from brackets; they swing and flare when rung, and
  // pulse gently while they can be rung.
  for (const auto& bell : k.bells)
  {
    const float x = sx(float(bell.x)) + 32.0f, y = sy(float(bell.y));
    if (offScreen(x - 64.0f, y - 140.0f, 128.0f, 220.0f))
      continue;
    const float amp = bell.ding > 0 ? 0.45f : 0.35f * float(bell.rest) / float(kBellRest);
    const float swing = amp * std::sin(float(frame) * 0.55f);
    const int sw = int(std::lround(swing * 20.0f));
    r.drawLine(x, y - 130.0f, x, y - 4.0f, 3.0f, rgba(60, 56, 60, 200));
    r.drawLine(x - 20.0f, y - 4.0f, x + 20.0f, y - 4.0f, 5.0f, rgb(60, 56, 60));
    if (bell.rest == 0)
      drawGlow(r, mArt, x, y + 34.0f, 46.0f, kGold, 0.18f + 0.1f * std::sin(float(frame) * 0.1f));
    if (bell.ding > 0)
      drawGlow(r, mArt, x, y + 34.0f, 90.0f, kGold, 0.6f);
    r.draw(baked(mArt, r, "bell/" + std::to_string(sw) + (bell.ding > 0 ? "r" : ""), 128, 96, 64.0f, 8.0f,
             [&](cairo_t* cr) { paintChefBell(cr, 64, 8, 62, double(sw) / 20.0, bell.ding > 0 ? 1.0 : 0.0); }),
      x, y);
    if (bell.ding > 0)
    {
      const float u = float(kBellDing - bell.ding) + alpha;
      for (int i = 0; i < 2; ++i)
      {
        const float rr = 30.0f + (u + float(i) * 3.0f) * 9.0f;
        const int a = std::max(0, 200 - int((u + float(i) * 3.0f) * 22.0f));
        for (int s = 0; s < 3; ++s)
        {
          const float a0 = -0.6f + float(s) * 0.25f;
          r.drawLine(x + std::cos(a0) * rr, y + 34.0f + std::sin(a0) * rr, x + std::cos(a0 + 0.2f) * rr,
            y + 34.0f + std::sin(a0 + 0.2f) * rr, 3.0f, rgba(255, 220, 120, a));
          r.drawLine(x - std::cos(a0) * rr, y + 34.0f + std::sin(a0) * rr, x - std::cos(a0 + 0.2f) * rr,
            y + 34.0f + std::sin(a0 + 0.2f) * rr, 3.0f, rgba(255, 220, 120, a));
        }
      }
      r.drawText("DING!", x, y - 34.0f - u * 3.0f, {26.0f, kGold, kInk}, Align::Center);
    }
  }

  // Pantry flaps: closed, a louvred door; open, swung aside.
  for (const auto& f : k.flaps)
  {
    const float x = sx(float(f.x)), y = sy(float(f.y));
    const float h = float(f.h) * kTilePx;
    if (offScreen(x, y, kTilePx, h))
      continue;
    if (f.open > 0)
      r.draw(baked(mArt, r, "flap_open/" + std::to_string(int(h)), 16, int(h), 0.0f, 0.0f,
               [&](cairo_t* cr) { paintFlap(cr, 16, double(h), mTheme); }),
        x + 2.0f, y);
    else
      r.draw(baked(mArt, r, "flap/" + std::to_string(int(h)), 56, int(h), 0.0f, 0.0f,
               [&](cairo_t* cr) { paintFlap(cr, 56, double(h), mTheme); }),
        x + 4.0f, y);
  }

  // Cleavers stuck in walls, the handles out; they wobble working loose.
  const Texture& stuckR = baked(mArt, r, "stuck_r", 130, 64, 112.0f, 0.0f, [](cairo_t* cr) { paintStuckCleaver(cr); });
  const Texture& stuckL = baked(mArt, r, "stuck_l", 130, 64, 18.0f, 0.0f, [](cairo_t* cr) {
    cairo_translate(cr, 130, 0);
    cairo_scale(cr, -1, 1);
    paintStuckCleaver(cr);
  });
  for (const auto& c : k.cleavers)
  {
    const float wallX = c.dir > 0 ? sx(float(c.bx + 1)) : sx(float(c.bx));
    const float y = sy(float(c.by)) - 4.0f;
    if (offScreen(wallX - 140.0f, y, 280.0f, 64.0f))
      continue;
    DrawOpts o;
    if (c.t < 20)
      o.angle = std::sin(float(frame) * 1.3f) * 4.0f * float(20 - c.t) / 20.0f;
    r.draw(c.dir > 0 ? stuckR : stuckL, wallX, y, o);
    if (c.t > kCleaverStick - 4)
      drawGlow(r, mArt, wallX, y + 24.0f, 40.0f, rgb(255, 255, 255), 0.5f);
  }

  // The golden whisk on its shelf.
  for (const auto& pz : mWest.prizes)
  {
    if (pz.taken || pz.kind != "golden_whisk")
      continue;
    const float x = sx(float(pz.x) + 0.5f), y = sy(float(pz.y + 1));
    if (offScreen(x - 80.0f, y - 140.0f, 160.0f, 160.0f))
      continue;
    drawGlow(r, mArt, x, y - 56.0f, 90.0f, kGold, 0.3f + 0.12f * std::sin(float(frame) * 0.1f));
    DrawOpts o;
    o.angle = std::sin(float(frame) * 0.06f) * 6.0f;
    r.draw(baked(mArt, r, "golden_whisk", 80, 130, 40.0f, 126.0f, [](cairo_t* cr) {
      cairo_translate(cr, 40, 126);
      paintGoldenWhisk(cr, 120);
    }),
      x, y - 2.0f, o);
    for (int i = 0; i < 3; ++i)
    {
      const int g = (frame + i * 23) % 70;
      if (g < 12)
      {
        const float gx = x + float(int(hash2(i, frame / 70) % 60u) - 30), gy = y - 30.0f - float(hash2(i + 7, frame / 70) % 90u);
        const float a = 1.0f - float(g) / 12.0f;
        r.drawLine(gx - 9.0f * a, gy, gx + 9.0f * a, gy, 3.0f, rgba(255, 250, 210, int(255 * a)), Blend::Add);
        r.drawLine(gx, gy - 9.0f * a, gx, gy + 9.0f * a, 3.0f, rgba(255, 250, 210, int(255 * a)), Blend::Add);
      }
    }
  }
}

// --- In front --------------------------------------------------------------------------

void World::drawKitchenFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& k = mKitchen;
  const auto& p = mPlayer;
  // The chef's hand.
  if (k.hand > 0)
  {
    const float px = (float(p.prevX) + float(p.x - p.prevX) * alpha + 1.5f) * kCellPx - camX;
    const float feet = (float(p.prevY) + float(p.y - p.prevY) * alpha + 1.0f) * kCellPx - camY;
    if (k.thumbs)
    {
      // Thumbs-up: in from the right, a little shake of the thumb, back out.
      const int f = kThumbsFrames - k.hand;
      const float in = std::min(1.0f, (float(f) + alpha) / 5.0f), out = std::min(1.0f, float(k.hand) / 5.0f);
      const float slide = (1.0f - std::min(in, out)) * 700.0f;
      const float hx = px + 190.0f + slide, hy = feet - 170.0f;
      DrawOpts o;
      o.angle = std::sin(float(frame) * 0.4f) * 5.0f;
      drawGlow(r, mArt, hx - 40.0f, hy - 30.0f, 160.0f, rgb(255, 240, 200), 0.25f);
      r.draw(baked(mArt, r, "hand_thumbs", 1200, 220, 100.0f, 120.0f, [](cairo_t* cr) {
        cairo_translate(cr, 100, 120);
        paintChefHand(cr, 1.15, 1, 900);
      }),
        hx, hy, o);
      if (f >= 6 && f < 24)
        r.drawText("GREAT JOB!", hx - 30.0f, hy - 150.0f, {30.0f, rgb(255, 230, 120), kInk}, Align::Center);
    }
    else
    {
      // The lift: palm up under the feet, slid in from the right at first and
      // out at the end; a spotlight on the runner while shown off.
      const int f = kHandFrames - k.hand;
      const float in = std::min(1.0f, (float(f) + alpha) / 4.0f), out = std::min(1.0f, float(k.hand) / 4.0f);
      const float slide = (1.0f - std::min(in, out)) * 500.0f;
      if (f >= 8 && f < 48)
      {
        drawGlow(r, mArt, px, feet - 80.0f, 200.0f, rgb(255, 250, 220), 0.35f);
        r.drawLine(px, feet - 90.0f, px - 120.0f, -40.0f, 160.0f, rgba(255, 250, 220, 30), Blend::Add);
      }
      r.draw(baked(mArt, r, "hand_lift", 340, 1100, 260.0f, 1066.0f, [](cairo_t* cr) {
        cairo_translate(cr, 260, 1066);
        paintChefHand(cr, 1.5, 0, 700);
      }),
        px + 120.0f + slide, feet + 6.0f);
    }
  }

  // ACHOO: whoever the Pepper Grinder just made sneeze.
  if (k.sneezing > 0)
    for (const auto& z : k.sneezes)
    {
      if (z.enemy < 0 || std::size_t(z.enemy) >= mEnemies.size())
        continue;
      const Enemy& e = mEnemies[std::size_t(z.enemy)];
      if (!e.alive || e.stun <= 0)
        continue;
      const float ex = (e.drawX + float(e.w) * 0.5f) * kCellPx - camX, ey = e.drawY * kCellPx - camY;
      const float u = float(20 - k.sneezing) + alpha;
      const float pop = 1.0f + 0.3f * std::max(0.0f, 1.0f - u / 3.0f);
      r.drawText("ACHOO!", ex + float(e.dir) * 30.0f, ey - 40.0f - u * 2.0f, {30.0f * pop, rgb(255, 250, 240), kInk, true},
        Align::Center);
      for (int i = 0; i < 8; ++i)
      {
        const float a = float(i) * 0.8f - 0.4f;
        const float d = 20.0f + u * 9.0f;
        r.fillRect(ex + float(e.dir) * (std::cos(a) * d + 20.0f), ey + 30.0f + std::sin(a) * d * 0.6f, 5.0f, 5.0f,
          rgba(40, 30, 26, std::max(0, 220 - int(u * 11.0f))));
      }
    }
}

void World::drawKitchenHud(Renderer& r, int frame) const
{
  const auto& p = mPlayer;
  // The Pepper Grinder's spin-up, under the weapon panel: a little grinder
  // turning and a bar filling; it whirs (shakes) until it is up to speed.
  if (p.weapon == Weapon::Proto && p.proto == int(ProtoId::PepperGrinder) && p.charge > 0)
  {
    const float t = std::min(1.0f, float(p.charge) / float(kGrinderSpin));
    const float x = 452.0f + 16.0f, y = 80.0f, w = 200.0f;
    const bool ready = p.charge >= kGrinderSpin;
    const float shake = ready ? 0.0f : ((frame / 2) % 2 ? 1.5f : -1.5f);
    r.fillRect(x - 2.0f, y - 2.0f, w + 40.0f, 18.0f, rgba(20, 14, 12, 200));
    r.fillRect(x + 30.0f, y + 3.0f, w, 8.0f, rgba(255, 255, 255, 40));
    r.fillRect(x + 30.0f, y + 3.0f, w * t, 8.0f, ready ? rgb(120, 230, 120) : lerpColor(rgb(255, 200, 80), rgb(240, 70, 40), t));
    r.draw(baked(mArt, r, "hud_grinder", 26, 26, 13.0f, 13.0f, [](cairo_t* cr) {
      roundedRect(cr, 8, 2, 10, 22, 4);
      linear(cr, 8, 0, 18, 0, {{0, rgb(160, 100, 50)}, {1, rgb(80, 44, 20)}}, 1.2);
      circlePath(cr, 13, 3, 3);
      fillInk(cr, rgb(220, 226, 236), 1.0);
    }),
      x + 14.0f + shake, y + 7.0f);
    if (!ready)
      r.drawText("SPIN", x + 30.0f + w * 0.5f, y - 3.0f, {13.0f, rgb(255, 240, 220), kInk}, Align::Center);
  }
}

// --- The Turntable bonus: a microwave's inside -------------------------------------------

// The cavity: enamel walls and shelves, its back wall, the grilles (vent
// plates the gems slip out through), the glass turntable plate turning and
// the lamp, all turned about the room's centre.
void World::drawTurntableBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& t = mTurn;
  float da = t.ang - t.prevAng;
  if (da < -3.0f)
    da += 6.2831853f;
  const float a = t.prevAng + da * alpha;
  const float c = std::cos(a), s = std::sin(a);
  const float cellPx = float(kCellSize) * kPixelScale;
  auto at = [&](float qx, float qy, float& ox, float& oy) {
    ox = (t.cx + c * qx - s * qy) * cellPx - camX;
    oy = (t.cy + s * qx + c * qy) * cellPx - camY;
  };
  const float ccx = t.cx * cellPx - camX, ccy = t.cy * cellPx - camY;
  // Outside the cavity: the microwave's dark casing.
  r.fillRect(0.0f, 0.0f, float(kScreenW), float(kScreenH), rgb(36, 32, 34));
  // The warm light of the oven lamp filling the box.
  drawGlow(r, mArt, ccx, ccy, float(t.w) * kTilePx * 0.75f, rgb(255, 214, 140), 0.3f);
  DrawOpts o;
  o.angle = a * 57.29578f;
  o.scale = 1.03f;
  const Texture& back = baked(mArt, r, "cavity_back", 64, 64, 0.0f, 0.0f, [](cairo_t* cr) { paintCavityBack(cr); });
  std::array<const Texture*, 3> walls{};
  for (int v = 0; v < 3; ++v)
    walls[std::size_t(v)] = &baked(mArt, r, "cavity_wall/" + std::to_string(v), 64, 64, 0.0f, 0.0f,
      [v](cairo_t* cr) { paintCavityWall(cr, v); });
  // Which solid blocks hang off the walls (shelves) and which stand free
  // (the turntable plate): flood from the border.
  std::vector<unsigned char> joined(t.solid.size(), 0);
  std::vector<int> todo;
  for (int by = 0; by < t.h; ++by)
    for (int bx = 0; bx < t.w; ++bx)
      if ((bx == 0 || by == 0 || bx == t.w - 1 || by == t.h - 1) && t.solid[std::size_t(by * t.w + bx)] != 0)
      {
        joined[std::size_t(by * t.w + bx)] = 1;
        todo.push_back(by * t.w + bx);
      }
  while (!todo.empty())
  {
    const int i = todo.back();
    todo.pop_back();
    const int bx = i % t.w, by = i / t.w;
    const int nb[4][2] = {{bx - 1, by}, {bx + 1, by}, {bx, by - 1}, {bx, by + 1}};
    for (const auto& n : nb)
    {
      if (n[0] < 0 || n[1] < 0 || n[0] >= t.w || n[1] >= t.h)
        continue;
      const std::size_t j = std::size_t(n[1] * t.w + n[0]);
      if (t.solid[j] != 0 && !joined[j])
      {
        joined[j] = 1;
        todo.push_back(int(j));
      }
    }
  }
  // The back wall over the inside.
  for (int by = 1; by < t.h - 1; ++by)
    for (int bx = 1; bx < t.w - 1; ++bx)
    {
      float ox = 0.0f, oy = 0.0f;
      at(float(bx * kCellsPerTile) - t.cx, float(by * kCellsPerTile) - t.cy, ox, oy);
      r.draw(back, ox, oy, o);
    }
  int plateX0 = t.w, plateX1 = -1, plateY = -1;
  for (int by = 0; by < t.h; ++by)
    for (int bx = 0; bx < t.w; ++bx)
    {
      const std::size_t i = std::size_t(by * t.w + bx);
      const unsigned char kind = t.solid[i];
      if (kind == 0)
        continue;
      float ox = 0.0f, oy = 0.0f;
      if (kind == 1 && !joined[i])
      {
        plateX0 = std::min(plateX0, bx);
        plateX1 = std::max(plateX1, bx);
        plateY = by;
        continue;
      }
      at(float(bx * kCellsPerTile) - t.cx, float(by * kCellsPerTile) - t.cy, ox, oy);
      if (kind == 1)
      {
        const bool border = bx == 0 || by == 0 || bx == t.w - 1 || by == t.h - 1;
        const int v = border ? (int(hash2(bx, by) % 9u) == 0 ? 1 : 0) : 0;
        r.draw(*walls[std::size_t(v)], ox, oy, o);
        if (!border)
        {
          // A shelf: a chrome lip along it.
          DrawOpts lo = o;
          lo.alpha = 0.9f;
          r.draw(*walls[2], ox, oy, lo);
        }
      }
      else
      {
        // A grille: a chrome vent plate, slotted.
        r.draw(baked(mArt, r, "grille", 64, 64, 0.0f, 0.0f, [](cairo_t* cr) {
          cairo_rectangle(cr, 0, 0, 64, 64);
          linear(cr, 0, 0, 64, 64, {{0, rgb(236, 240, 246)}, {1, rgb(140, 148, 162)}});
          for (int k = 0; k < 5; ++k)
          {
            roundedRect(cr, 10, 8 + k * 11, 44, 5, 2.5);
            setColor(cr, rgb(30, 30, 36));
            cairo_fill(cr);
          }
          cairo_rectangle(cr, 1, 1, 62, 62);
          setColor(cr, kInk);
          cairo_set_line_width(cr, 2);
          cairo_stroke(cr);
        }),
          ox, oy, o);
      }
    }
  // The turntable plate: glass, the light running round it as it turns.
  if (plateY >= 0)
  {
    const float len = float(plateX1 - plateX0 + 1) * float(kCellsPerTile);
    const float qx0 = float(plateX0 * kCellsPerTile) - t.cx, qy = float(plateY * kCellsPerTile) - t.cy;
    for (int i = 0; i <= 16; ++i)
    {
      const float u = float(i) / 16.0f;
      float ox = 0.0f, oy = 0.0f;
      at(qx0 + u * len, qy + 0.5f, ox, oy);
      float ox2 = 0.0f, oy2 = 0.0f;
      at(qx0 + u * len, qy + float(kCellsPerTile) - 0.5f, ox2, oy2);
      if (i < 16)
      {
        float nx = 0.0f, ny = 0.0f, nx2 = 0.0f, ny2 = 0.0f;
        at(qx0 + (u + 1.0f / 16.0f) * len, qy + 0.5f, nx, ny);
        at(qx0 + (u + 1.0f / 16.0f) * len, qy + float(kCellsPerTile) - 0.5f, nx2, ny2);
        const float edge = std::abs(u - 0.5f) * 2.0f;
        const float shine = 0.5f + 0.5f * std::sin(u * 9.0f - float(frame) * 0.15f);
        const Color glass = lerpColor(rgb(170, 220, 210), rgb(240, 255, 250), shine * (1.0f - edge));
        r.drawLine((ox + ox2) * 0.5f, (oy + oy2) * 0.5f, (nx + nx2) * 0.5f, (ny + ny2) * 0.5f, kTilePx * 0.55f,
          withAlpha(glass, 210));
        r.drawLine(ox, oy, nx, ny, 4.0f, rgba(255, 255, 255, 220));
        r.drawLine(ox2, oy2, nx2, ny2, 5.0f, rgba(60, 100, 96, 200));
      }
    }
    // The roller ring and the drive under its middle.
    float hx = 0.0f, hy = 0.0f;
    at(qx0 + len * 0.5f, qy + float(kCellsPerTile) + 1.0f, hx, hy);
    drawGlow(r, mArt, hx, hy, 30.0f, rgb(255, 230, 180), 0.4f);
  }
  // The lamp in the top right corner of the cavity.
  float lx = 0.0f, ly = 0.0f;
  at(float(t.w * kCellsPerTile) * 0.5f - 4.0f, -float(t.h * kCellsPerTile) * 0.5f + 3.0f, lx, ly);
  drawGlow(r, mArt, lx, ly, 120.0f, rgb(255, 230, 160), 0.6f);
  drawGlow(r, mArt, lx, ly, 30.0f, rgb(255, 255, 230), 0.9f);
}

// The gems, and over everything the door: its glass glowing warm, the mesh
// of perforations, the frame round the view and the reflections.
void World::drawTurntableFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& t = mTurn;
  float da = t.ang - t.prevAng;
  if (da < -3.0f)
    da += 6.2831853f;
  const float a = t.prevAng + da * alpha;
  const float c = std::cos(a), s = std::sin(a);
  const float cellPx = float(kCellSize) * kPixelScale;
  for (std::size_t i = 0; i < t.gems.size(); ++i)
  {
    const auto& g = t.gems[i];
    if (g.gone)
      continue;
    const float qx = g.x - t.cx, qy = g.y - t.cy;
    const float x = (t.cx + c * qx - s * qy) * cellPx - camX, y = (t.cy + s * qx + c * qy) * cellPx - camY;
    drawGlow(r, mArt, x, y, 50, mArt.gemColor[std::size_t(g.variant % 4)], 0.45f + 0.2f * std::sin(float(frame) * 0.15f + float(i)));
    r.draw(mArt.items[std::size_t(kIconGem0 + g.variant % 4)], x - 32.0f, y - 32.0f);
  }
  // The door: a warm cast, the mesh, the frame.
  r.fillRect(0.0f, 0.0f, float(kScreenW), float(kScreenH), rgba(255, 200, 120, 26));
  const Texture& mesh = baked(mArt, r, "door_mesh", 96, 96, 0.0f, 0.0f, [](cairo_t* cr) { paintDoorMesh(cr); });
  DrawOpts mo;
  mo.cull = false;
  for (float y = 0.0f; y < float(kScreenH); y += 96.0f)
    for (float x = 0.0f; x < float(kScreenW); x += 96.0f)
      r.draw(mesh, x, y, mo);
  // Reflections sliding slowly across the glass.
  const float rx = std::fmod(float(frame) * 0.6f, float(kScreenW) + 600.0f) - 300.0f;
  r.drawLine(rx, float(kScreenH) + 40.0f, rx + 260.0f, -40.0f, 70.0f, rgba(255, 255, 255, 14), Blend::Add);
  r.drawLine(rx + 120.0f, float(kScreenH) + 40.0f, rx + 380.0f, -40.0f, 24.0f, rgba(255, 255, 255, 18), Blend::Add);
  // The door's frame: dark bands round the view.
  const Color frameC = rgba(30, 28, 32, 235);
  r.fillRect(0.0f, 0.0f, float(kScreenW), 18.0f, frameC);
  r.fillRect(0.0f, float(kScreenH) - 18.0f, float(kScreenW), 18.0f, frameC);
  r.fillRect(0.0f, 0.0f, 18.0f, float(kScreenH), frameC);
  r.fillRect(float(kScreenW) - 18.0f, 0.0f, 18.0f, float(kScreenH), frameC);
  r.fillRect(18.0f, 18.0f, float(kScreenW) - 36.0f, 3.0f, rgba(200, 200, 210, 120));
}

} // namespace gr
