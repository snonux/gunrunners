// Level 36, Prop Warehouse: drawing. See world_warehouse.cpp for the logic.
//
// Behind the tiles (drawWarehouseSet): the loading dock's back wall and its
// half-open roll-up door onto the studio lot at night, the pallet racks'
// blue uprights behind the aisle shelves, hanging aisle signs, the black
// velvet curtains of the prop maze, the merch mezzanine's sign, the crane
// yard's yellow gantry legs and its pit wall, the freight lift shafts and
// the dressing-room landing, and the props of Levels 1-35 on their little
// shelves with their tags. Behind the runner (drawWarehouseBack): the foam
// in the pit, CRATE 42, the EP. 1 BLOOPERS crate, the TURBODUDES crate and
// its old title screen, the dock bumpers, the cranes (rail, trolley,
// cable, magnet and its bell) and the ride crates, the metal crates
// (resting, held, sliding, blinking), the levers, the freight lift cars,
// and the bonus's loose crates and wall pieces. In front
// (drawWarehouseFront): the scenery flats, Forklift Bots and their forks,
// Stagehands, Prop Mimics, the Magnet Gun's beam and muzzle, the bonus's
// lifted and flying things. The HUD (drawWarehouseHud): a prop's tag.

#include "game/world.hpp"

#include "assets/art.hpp"
#include "assets/enemy_art.hpp"
#include "base/math.hpp"
#include "game/warehouse_draw.hpp"
#include "render/vector.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <initializer_list>
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
constexpr float kTilePx = float(kTileSize) * kPixelScale; // 64
constexpr Color kInk = rgb(18, 14, 20);
constexpr Color kHudInk = rgb(10, 8, 20);
constexpr Color kCraneYellow = rgb(246, 192, 36);
constexpr Color kCraneYellowLo = rgb(160, 104, 18);
constexpr Color kSteel = rgb(150, 156, 168);
constexpr Color kSteelLo = rgb(62, 66, 78);
constexpr Color kWood = rgb(170, 122, 70);
constexpr Color kWoodLo = rgb(100, 66, 36);
constexpr Color kField = rgb(110, 170, 255);
constexpr Color kFieldRed = rgb(255, 80, 80);

bool offScreen(float x, float y, float w, float h, float m = 96.0f)
{
  return x > float(kScreenW) + m || x + w < -m || y > float(kScreenH) + m || y + h < -m;
}

float sx(int cell, float camX) { return float(cell) * kCellPx - camX; }
float sy(int cell, float camY) { return float(cell) * kCellPx - camY; }

const Sprite& bakedSprite(const Art& art, const Renderer& r, const std::string& key, int w, int h, float ax, float ay,
  const std::function<void(cairo_t*)>& paint)
{
  const std::string id = "~warehouse36/" + key;
  auto it = art.styled.find(id);
  if (it != art.styled.end())
    return it->second;
  VectorImage img(std::max(1, w), std::max(1, h));
  paint(img.cr());
  Sprite s;
  s.facing[0] = img.toTexture(r, ax, ay);
  s.facing[1] = img.toTexture(r, float(w) - ax, ay, true);
  return art.styled.emplace(id, std::move(s)).first->second;
}

const Texture& baked(const Art& art, const Renderer& r, const std::string& key, int w, int h, float ax, float ay,
  const std::function<void(cairo_t*)>& paint)
{
  return bakedSprite(art, r, key, w, h, ax, ay, paint).facing[0];
}

void setRgba(cairo_t* cr, Color c, double a = 1.0)
{
  cairo_set_source_rgba(cr, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, a);
}

void stop(cairo_pattern_t* g, double at, Color c, double a = 1.0)
{
  cairo_pattern_add_color_stop_rgba(g, at, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, a);
}

void circle(cairo_t* cr, double x, double y, double r)
{
  cairo_new_sub_path(cr);
  cairo_arc(cr, x, y, r, 0, 2 * kPi);
}

void ellipse(cairo_t* cr, double x, double y, double rx, double ry)
{
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_scale(cr, std::max(0.01, rx), std::max(0.01, ry));
  cairo_new_sub_path(cr);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_restore(cr);
}

void roundRect(cairo_t* cr, double x, double y, double w, double h, double r)
{
  r = std::min(r, std::min(w, h) * 0.5);
  cairo_new_sub_path(cr);
  cairo_arc(cr, x + w - r, y + r, r, -kPi * 0.5, 0);
  cairo_arc(cr, x + w - r, y + h - r, r, 0, kPi * 0.5);
  cairo_arc(cr, x + r, y + h - r, r, kPi * 0.5, kPi);
  cairo_arc(cr, x + r, y + r, r, kPi, kPi * 1.5);
  cairo_close_path(cr);
}

void lgrad(cairo_t* cr, double x0, double y0, double x1, double y1, std::initializer_list<std::pair<double, Color>> stops,
  double a = 1.0, bool preserve = false)
{
  cairo_pattern_t* g = cairo_pattern_create_linear(x0, y0, x1, y1);
  for (const auto& s : stops)
    stop(g, s.first, s.second, a);
  cairo_set_source(cr, g);
  if (preserve)
    cairo_fill_preserve(cr);
  else
    cairo_fill(cr);
  cairo_pattern_destroy(g);
}

void fill(cairo_t* cr, Color c, double a = 1.0)
{
  setRgba(cr, c, a);
  cairo_fill(cr);
}

void fillKeep(cairo_t* cr, Color c, double a = 1.0)
{
  setRgba(cr, c, a);
  cairo_fill_preserve(cr);
}

void stroke(cairo_t* cr, Color c, double w, double a = 1.0, bool preserve = false)
{
  setRgba(cr, c, a);
  cairo_set_line_width(cr, w);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
  if (preserve)
    cairo_stroke_preserve(cr);
  else
    cairo_stroke(cr);
}

// Fill with a colour and outline in ink.
void inked(cairo_t* cr, Color c, double w = 2.0)
{
  fillKeep(cr, c);
  stroke(cr, kInk, w);
}

void glow(cairo_t* cr, double x, double y, double rad, Color c, double a)
{
  cairo_pattern_t* g = cairo_pattern_create_radial(x, y, 0, x, y, rad);
  stop(g, 0.0, c, a);
  stop(g, 0.5, c, a * 0.35);
  stop(g, 1.0, c, 0.0);
  circle(cr, x, y, rad);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
}

void lettering(cairo_t* cr, const char* text, double cx, double baseline, double size, Color c, double a = 1.0)
{
  selectGameFont(cr);
  cairo_set_font_size(cr, size);
  cairo_text_extents_t e;
  cairo_text_extents(cr, text, &e);
  cairo_move_to(cr, cx - e.width * 0.5 - e.x_bearing, baseline);
  setRgba(cr, c, a);
  cairo_show_text(cr, text);
}

void line(cairo_t* cr, double x0, double y0, double x1, double y1)
{
  cairo_move_to(cr, x0, y0);
  cairo_line_to(cr, x1, y1);
}

// A wooden crate face (x, y, w, h): planks, a frame round it and a brace.
void woodCrate(cairo_t* cr, double x, double y, double w, double h, Color c)
{
  cairo_rectangle(cr, x, y, w, h);
  lgrad(cr, 0, y, 0, y + h, {{0.0, lerpColor(c, rgb(255, 230, 190), 0.25f)}, {1.0, lerpColor(c, rgb(0, 0, 0), 0.3f)}}, 1.0, true);
  stroke(cr, kInk, 2.4);
  for (double py = y + 16; py < y + h - 8; py += 16)
  {
    line(cr, x + 2, py, x + w - 2, py);
    stroke(cr, lerpColor(c, rgb(0, 0, 0), 0.45f), 1.2, 0.8);
  }
  const double f = std::min(w, h) * 0.12;
  cairo_rectangle(cr, x + f * 0.5, y + f * 0.5, w - f, h - f);
  stroke(cr, lerpColor(c, rgb(0, 0, 0), 0.25f), f, 1.0);
  cairo_rectangle(cr, x, y, w, h);
  stroke(cr, kInk, 2.4);
  for (const double px : {x + f * 0.5, x + w - f * 0.5})
    for (const double py : {y + f * 0.5, y + h - f * 0.5})
    {
      circle(cr, px, py, 2.0);
      fill(cr, rgb(70, 66, 70));
    }
}

// --- Painters: the warehouse's set ---------------------------------------------------------------------

// The props of levels 1-35: a word or two for each label.
const char* propLabel(int level)
{
  static const char* kLabels[] = {"",
    "EP 1 - ROOFTOP SIGN", "EP 1 - GLASS PANE", "EP 1 - HOLO AD", "EP 1 - CLUB SPEAKER", "EP 1 - SLUDGE VALVE",
    "EP 1 - MAGLEV SEAT", "EP 1 - BLACK HALO DECAL", "EP 2 - PULLEY HOOK", "EP 2 - TEMPLE MASK", "EP 2 - LIGHT MIRROR",
    "EP 2 - MINE LANTERN", "EP 2 - LAVA ROCK", "EP 2 - SURFBOARD", "EP 2 - GOLEM HAND", "EP 3 - DOCK CLAMP",
    "EP 3 - CRYO POD", "EP 3 - GROW LAMP", "EP 3 - HULL RIVET", "EP 3 - GRAV SWITCH", "EP 3 - REACTOR ROD",
    "EP 3 - ZERO'S EYE", "EP 4 - WANTED POSTER", "EP 4 - GHOST SHEET", "EP 4 - GIANT WHISK", "EP 4 - GARDEN GNOME",
    "EP 4 - GORGAZILLA FOOT", "EP 4 - FEDORA", "EP 4 - INK BOTTLE", "EP 5 - TEST CARD", "EP 5 - ANCHOR RING",
    "EP 5 - TESLA ROD", "EP 5 - MINIDISH", "EP 5 - SERVER BLADE", "EP 5 - PROPELLER", "EP 5 - TITAN BROCHURE"};
  return level >= 1 && level <= 35 ? kLabels[level] : "";
}

// One prop of an earlier level, 128 x 128, standing on the bottom edge.
void paintProp(cairo_t* cr, int n)
{
  const Color gold = rgb(240, 196, 70), grey = rgb(150, 150, 160), dark = rgb(50, 48, 56);
  switch (n)
  {
    case 1: // the rooftop sign: neon letters on a frame with legs
      for (const double x : {30.0, 98.0})
      {
        line(cr, x, 128, x, 70);
        stroke(cr, dark, 5);
      }
      roundRect(cr, 12, 30, 104, 44, 6);
      inked(cr, rgb(30, 24, 44));
      lettering(cr, "GR", 64, 66, 34, rgb(255, 90, 200));
      glow(cr, 64, 52, 50, rgb(255, 90, 200), 0.35);
      break;
    case 2: // a pane of glass in a frame, a crack and a glare
      cairo_rectangle(cr, 26, 20, 76, 108);
      inked(cr, rgb(150, 200, 230));
      cairo_rectangle(cr, 26, 20, 76, 108);
      stroke(cr, grey, 6);
      line(cr, 40, 40, 60, 70);
      line(cr, 60, 70, 52, 100);
      line(cr, 60, 70, 88, 84);
      stroke(cr, rgb(240, 250, 255), 1.4);
      line(cr, 36, 112, 56, 30);
      stroke(cr, rgb(255, 255, 255), 4, 0.5);
      break;
    case 3: // a holo ad projector, a pink figure flickering over it
      roundRect(cr, 34, 104, 60, 24, 4);
      inked(cr, dark);
      glow(cr, 64, 62, 46, rgb(255, 120, 220), 0.45);
      ellipse(cr, 64, 40, 10, 12);
      cairo_rectangle(cr, 52, 52, 24, 40);
      stroke(cr, rgb(255, 150, 230), 2.0);
      for (int k = 0; k < 6; ++k)
      {
        line(cr, 46, 50 + k * 8, 82, 50 + k * 8);
        stroke(cr, rgb(255, 170, 240), 1.0, 0.5);
      }
      break;
    case 4: // a club speaker cabinet
      roundRect(cr, 28, 22, 72, 106, 6);
      inked(cr, rgb(30, 28, 34));
      for (const auto& c : {std::array<double, 2>{64, 52}, {64, 98}})
      {
        circle(cr, c[0], c[1], c[1] < 60 ? 16 : 24);
        inked(cr, rgb(70, 70, 80));
        circle(cr, c[0], c[1], 6);
        fill(cr, rgb(130, 130, 140));
      }
      break;
    case 5: // the sludge valve: a red wheel on a pipe, a green drip
      cairo_rectangle(cr, 56, 60, 16, 68);
      inked(cr, rgb(120, 120, 110));
      circle(cr, 64, 52, 30);
      stroke(cr, kInk, 9);
      circle(cr, 64, 52, 30);
      stroke(cr, rgb(210, 50, 40), 6);
      line(cr, 34, 52, 94, 52);
      line(cr, 64, 22, 64, 82);
      stroke(cr, rgb(210, 50, 40), 4);
      ellipse(cr, 80, 108, 4, 6);
      fill(cr, rgb(120, 240, 80));
      break;
    case 6: // a maglev seat
      roundRect(cr, 30, 22, 30, 80, 8);
      inked(cr, rgb(60, 120, 200));
      roundRect(cr, 30, 84, 74, 22, 6);
      inked(cr, rgb(70, 140, 220));
      line(cr, 50, 106, 50, 128);
      line(cr, 90, 106, 90, 128);
      stroke(cr, grey, 5);
      break;
    case 7: // the Black Halo decal: a black ring on a white panel
      cairo_rectangle(cr, 18, 24, 92, 104);
      inked(cr, rgb(230, 230, 236));
      circle(cr, 64, 72, 28);
      stroke(cr, rgb(20, 20, 24), 10);
      circle(cr, 64, 72, 28);
      stroke(cr, rgb(255, 200, 80), 1.5, 0.8);
      break;
    case 8: // a pulley block with a hook
      line(cr, 64, 0, 64, 40);
      stroke(cr, rgb(200, 180, 130), 3);
      roundRect(cr, 44, 34, 40, 44, 8);
      inked(cr, rgb(170, 110, 50));
      circle(cr, 64, 52, 12);
      inked(cr, grey);
      cairo_move_to(cr, 64, 78);
      cairo_line_to(cr, 64, 96);
      cairo_arc(cr, 54, 104, 10, -kPi * 0.2, kPi);
      stroke(cr, kInk, 8);
      cairo_move_to(cr, 64, 78);
      cairo_line_to(cr, 64, 96);
      cairo_arc(cr, 54, 104, 10, -kPi * 0.2, kPi);
      stroke(cr, grey, 5);
      cairo_rectangle(cr, 34, 120, 60, 8);
      fill(cr, dark);
      break;
    case 9: // a temple mask
      cairo_move_to(cr, 64, 18);
      cairo_curve_to(cr, 108, 20, 108, 110, 64, 124);
      cairo_curve_to(cr, 20, 110, 20, 20, 64, 18);
      lgrad(cr, 0, 18, 0, 124, {{0.0, rgb(250, 220, 110)}, {1.0, rgb(170, 110, 30)}}, 1.0, true);
      stroke(cr, kInk, 2.4);
      for (const double x : {48.0, 80.0})
      {
        ellipse(cr, x, 58, 10, 6);
        fill(cr, kInk);
      }
      cairo_rectangle(cr, 46, 88, 36, 12);
      inked(cr, rgb(110, 40, 30));
      break;
    case 10: // a light mirror on a stand
      line(cr, 64, 80, 64, 128);
      stroke(cr, dark, 6);
      ellipse(cr, 64, 50, 34, 40);
      inked(cr, rgb(200, 220, 240));
      line(cr, 48, 30, 60, 70);
      stroke(cr, rgb(255, 255, 255), 5, 0.6);
      glow(cr, 70, 40, 30, rgb(255, 250, 220), 0.4);
      break;
    case 11: // a mine lantern
      glow(cr, 64, 76, 44, rgb(255, 190, 90), 0.5);
      cairo_arc(cr, 64, 34, 14, kPi, 2 * kPi);
      stroke(cr, dark, 4);
      roundRect(cr, 42, 40, 44, 70, 8);
      inked(cr, rgb(255, 210, 120));
      for (const double x : {50.0, 64.0, 78.0})
      {
        line(cr, x, 42, x, 108);
        stroke(cr, dark, 3);
      }
      cairo_rectangle(cr, 38, 108, 52, 20);
      inked(cr, dark);
      break;
    case 12: // a lava rock, glowing in its cracks
      cairo_move_to(cr, 20, 128);
      cairo_line_to(cr, 26, 70);
      cairo_line_to(cr, 56, 46);
      cairo_line_to(cr, 96, 56);
      cairo_line_to(cr, 110, 128);
      cairo_close_path(cr);
      inked(cr, rgb(50, 40, 40));
      line(cr, 40, 90, 64, 80);
      line(cr, 64, 80, 90, 100);
      line(cr, 64, 80, 70, 60);
      stroke(cr, rgb(255, 120, 30), 3);
      glow(cr, 64, 84, 34, rgb(255, 120, 40), 0.4);
      break;
    case 13: // a surfboard standing up
      ellipse(cr, 64, 66, 22, 62);
      lgrad(cr, 42, 0, 86, 0, {{0.0, rgb(80, 200, 220)}, {1.0, rgb(30, 120, 170)}}, 1.0, true);
      stroke(cr, kInk, 2.4);
      line(cr, 64, 8, 64, 126);
      stroke(cr, rgb(255, 230, 120), 4);
      break;
    case 14: // a golem's stone hand
      roundRect(cr, 30, 60, 68, 68, 14);
      inked(cr, rgb(120, 116, 106));
      for (int k = 0; k < 4; ++k)
      {
        roundRect(cr, 32 + k * 16, 20 + (k == 0 || k == 3 ? 14 : 0), 16, 50, 7);
        inked(cr, rgb(136, 130, 120));
      }
      line(cr, 50, 90, 70, 100);
      stroke(cr, rgb(70, 66, 60), 2);
      break;
    case 15: // a dock clamp: a big steel C
      cairo_move_to(cr, 96, 30);
      cairo_line_to(cr, 40, 30);
      cairo_line_to(cr, 40, 110);
      cairo_line_to(cr, 96, 110);
      stroke(cr, kInk, 20);
      cairo_move_to(cr, 96, 30);
      cairo_line_to(cr, 40, 30);
      cairo_line_to(cr, 40, 110);
      cairo_line_to(cr, 96, 110);
      stroke(cr, rgb(110, 130, 160), 15);
      line(cr, 88, 40, 88, 80);
      stroke(cr, kCraneYellow, 6);
      break;
    case 16: // a cryo pod with frosty glass
      roundRect(cr, 34, 8, 60, 120, 28);
      inked(cr, rgb(200, 210, 224));
      roundRect(cr, 44, 22, 40, 82, 18);
      inked(cr, rgb(160, 220, 255), 1.4);
      glow(cr, 64, 60, 30, rgb(220, 250, 255), 0.6);
      break;
    case 17: // a grow lamp, purple light down on a sprout
      glow(cr, 64, 90, 48, rgb(200, 90, 255), 0.4);
      line(cr, 20, 128, 20, 30);
      line(cr, 20, 30, 64, 30);
      stroke(cr, dark, 5);
      cairo_move_to(cr, 44, 50);
      cairo_line_to(cr, 84, 50);
      cairo_line_to(cr, 74, 32);
      cairo_line_to(cr, 54, 32);
      cairo_close_path(cr);
      inked(cr, rgb(90, 80, 110));
      cairo_rectangle(cr, 50, 108, 28, 20);
      inked(cr, rgb(170, 90, 50));
      line(cr, 64, 108, 64, 88);
      stroke(cr, rgb(90, 200, 90), 3);
      ellipse(cr, 72, 90, 8, 4);
      fill(cr, rgb(90, 200, 90));
      break;
    case 18: // a giant hull rivet
      cairo_rectangle(cr, 54, 64, 20, 64);
      inked(cr, rgb(130, 130, 140));
      cairo_move_to(cr, 24, 66);
      cairo_curve_to(cr, 24, 22, 104, 22, 104, 66);
      cairo_close_path(cr);
      lgrad(cr, 0, 24, 0, 66, {{0.0, rgb(220, 224, 232)}, {1.0, rgb(100, 104, 116)}}, 1.0, true);
      stroke(cr, kInk, 2.4);
      break;
    case 19: // a grav switch: an arrow on a panel and a lever
      cairo_rectangle(cr, 28, 34, 72, 94);
      inked(cr, rgb(60, 70, 96));
      cairo_move_to(cr, 64, 46);
      cairo_line_to(cr, 82, 70);
      cairo_line_to(cr, 70, 70);
      cairo_line_to(cr, 70, 96);
      cairo_line_to(cr, 58, 96);
      cairo_line_to(cr, 58, 70);
      cairo_line_to(cr, 46, 70);
      cairo_close_path(cr);
      fill(cr, rgb(150, 255, 200));
      line(cr, 64, 112, 90, 20);
      stroke(cr, dark, 5);
      circle(cr, 90, 20, 8);
      inked(cr, rgb(230, 60, 50));
      break;
    case 20: // a reactor rod, glowing green-blue
      glow(cr, 64, 70, 46, rgb(110, 255, 210), 0.45);
      roundRect(cr, 52, 10, 24, 118, 10);
      lgrad(cr, 52, 0, 76, 0, {{0.0, rgb(160, 255, 220)}, {1.0, rgb(40, 160, 130)}}, 1.0, true);
      stroke(cr, kInk, 2.4);
      for (const double y : {30.0, 60.0, 90.0})
      {
        cairo_rectangle(cr, 50, y, 28, 6);
        inked(cr, grey, 1.2);
      }
      break;
    case 21: // ZERO's eye: a red lens in a steel housing
      circle(cr, 64, 70, 46);
      inked(cr, rgb(70, 72, 84));
      circle(cr, 64, 70, 28);
      inked(cr, rgb(170, 20, 30));
      glow(cr, 64, 70, 28, rgb(255, 60, 60), 0.8);
      circle(cr, 64, 70, 8);
      fill(cr, rgb(255, 220, 200));
      break;
    case 22: // a wanted poster on a board
      cairo_rectangle(cr, 26, 14, 76, 100);
      inked(cr, rgb(232, 210, 160));
      lettering(cr, "WANTED", 64, 34, 13, rgb(80, 40, 20));
      cairo_rectangle(cr, 42, 40, 44, 40);
      inked(cr, rgb(190, 160, 110), 1.2);
      circle(cr, 64, 58, 10);
      fill(cr, rgb(110, 70, 40));
      lettering(cr, "$500", 64, 100, 14, rgb(80, 40, 20));
      line(cr, 64, 114, 64, 128);
      stroke(cr, dark, 4);
      break;
    case 23: // a ghost sheet on a stand
      cairo_move_to(cr, 64, 14);
      cairo_curve_to(cr, 100, 14, 100, 60, 104, 124);
      cairo_line_to(cr, 88, 116);
      cairo_line_to(cr, 76, 126);
      cairo_line_to(cr, 64, 116);
      cairo_line_to(cr, 52, 126);
      cairo_line_to(cr, 40, 116);
      cairo_line_to(cr, 24, 124);
      cairo_curve_to(cr, 28, 60, 28, 14, 64, 14);
      cairo_close_path(cr);
      inked(cr, rgb(236, 236, 244));
      for (const double x : {52.0, 76.0})
      {
        ellipse(cr, x, 46, 6, 9);
        fill(cr, kInk);
      }
      break;
    case 24: // a giant whisk
      cairo_rectangle(cr, 58, 82, 12, 46);
      inked(cr, rgb(200, 60, 60));
      for (int k = -2; k <= 2; ++k)
      {
        cairo_move_to(cr, 64, 84);
        cairo_curve_to(cr, 64 + k * 22, 60, 64 + k * 16, 14, 64, 14);
        stroke(cr, rgb(210, 214, 224), 3);
      }
      break;
    case 25: // a garden gnome
      cairo_move_to(cr, 64, 6);
      cairo_line_to(cr, 84, 56);
      cairo_line_to(cr, 44, 56);
      cairo_close_path(cr);
      inked(cr, rgb(220, 50, 50));
      circle(cr, 64, 62, 12);
      inked(cr, rgb(250, 200, 170));
      cairo_move_to(cr, 50, 66);
      cairo_curve_to(cr, 52, 96, 76, 96, 78, 66);
      inked(cr, rgb(250, 250, 250));
      roundRect(cr, 44, 86, 40, 42, 10);
      inked(cr, rgb(60, 110, 220));
      break;
    case 26: // Gorgazilla's foot: a huge green clawed toe end
      cairo_move_to(cr, 10, 128);
      cairo_curve_to(cr, 10, 60, 118, 60, 118, 128);
      cairo_close_path(cr);
      inked(cr, rgb(70, 150, 70));
      for (const double x : {30.0, 64.0, 98.0})
      {
        cairo_move_to(cr, x - 9, 100);
        cairo_line_to(cr, x, 82);
        cairo_line_to(cr, x + 9, 100);
        cairo_close_path(cr);
        inked(cr, rgb(240, 236, 220), 1.4);
      }
      break;
    case 27: // a fedora on a hat stand
      line(cr, 64, 70, 64, 128);
      stroke(cr, rgb(120, 80, 50), 6);
      ellipse(cr, 64, 70, 40, 8);
      inked(cr, rgb(70, 60, 50));
      cairo_move_to(cr, 40, 68);
      cairo_curve_to(cr, 40, 30, 88, 30, 88, 68);
      cairo_close_path(cr);
      inked(cr, rgb(84, 72, 60));
      cairo_rectangle(cr, 40, 58, 48, 6);
      fill(cr, rgb(30, 24, 20));
      break;
    case 28: // an ink bottle
      cairo_move_to(cr, 50, 40);
      cairo_line_to(cr, 78, 40);
      cairo_line_to(cr, 100, 70);
      cairo_line_to(cr, 100, 128);
      cairo_line_to(cr, 28, 128);
      cairo_line_to(cr, 28, 70);
      cairo_close_path(cr);
      inked(cr, rgb(30, 30, 50));
      cairo_rectangle(cr, 50, 24, 28, 18);
      inked(cr, rgb(120, 80, 40));
      cairo_rectangle(cr, 38, 84, 52, 28);
      inked(cr, rgb(236, 226, 200), 1.2);
      lettering(cr, "INK", 64, 104, 14, kInk);
      break;
    case 29: // a test card in a frame
      cairo_rectangle(cr, 14, 30, 100, 76);
      inked(cr, rgb(60, 60, 70));
      {
        static const Color bars[7] = {rgb(230, 230, 230), rgb(230, 230, 40), rgb(40, 230, 230), rgb(40, 230, 40),
          rgb(230, 40, 230), rgb(230, 40, 40), rgb(40, 40, 230)};
        for (int k = 0; k < 7; ++k)
        {
          cairo_rectangle(cr, 20 + k * 12.6, 36, 12.6, 64);
          fill(cr, bars[k]);
        }
      }
      circle(cr, 64, 68, 20);
      stroke(cr, kInk, 2);
      line(cr, 64, 106, 64, 128);
      stroke(cr, dark, 5);
      break;
    case 30: // an anchor
      cairo_move_to(cr, 64, 30);
      cairo_line_to(cr, 64, 116);
      cairo_move_to(cr, 40, 46);
      cairo_line_to(cr, 88, 46);
      cairo_move_to(cr, 24, 92);
      cairo_curve_to(cr, 30, 120, 98, 120, 104, 92);
      stroke(cr, kInk, 12);
      cairo_move_to(cr, 64, 30);
      cairo_line_to(cr, 64, 116);
      cairo_move_to(cr, 40, 46);
      cairo_line_to(cr, 88, 46);
      cairo_move_to(cr, 24, 92);
      cairo_curve_to(cr, 30, 120, 98, 120, 104, 92);
      stroke(cr, rgb(90, 96, 110), 8);
      circle(cr, 64, 22, 10);
      stroke(cr, rgb(90, 96, 110), 5);
      break;
    case 31: // a Tesla rod: a coil and a ball, sparking
      cairo_rectangle(cr, 50, 50, 28, 78);
      inked(cr, rgb(180, 110, 60));
      for (double y = 54; y < 124; y += 6)
      {
        line(cr, 50, y, 78, y + 3);
        stroke(cr, rgb(230, 160, 90), 2);
      }
      circle(cr, 64, 34, 18);
      inked(cr, rgb(200, 204, 214));
      line(cr, 80, 26, 96, 14);
      line(cr, 96, 14, 104, 24);
      stroke(cr, rgb(180, 220, 255), 2);
      break;
    case 32: // a mini dish on a tripod
      line(cr, 64, 70, 40, 128);
      line(cr, 64, 70, 88, 128);
      stroke(cr, dark, 4);
      cairo_save(cr);
      cairo_translate(cr, 64, 56);
      cairo_rotate(cr, -0.5);
      cairo_scale(cr, 1.0, 0.45);
      cairo_arc(cr, 0, 0, 40, 0, kPi);
      cairo_restore(cr);
      inked(cr, rgb(232, 232, 238));
      line(cr, 64, 60, 84, 30);
      stroke(cr, dark, 3);
      break;
    case 33: // a server blade, its lights
      cairo_rectangle(cr, 14, 70, 100, 30);
      inked(cr, rgb(40, 42, 52));
      for (int k = 0; k < 8; ++k)
      {
        cairo_rectangle(cr, 22 + k * 10, 80, 5, 4);
        fill(cr, k % 3 == 0 ? rgb(255, 80, 60) : rgb(80, 220, 255));
      }
      cairo_rectangle(cr, 92, 78, 16, 14);
      inked(cr, gold, 1.2);
      cairo_rectangle(cr, 40, 100, 48, 28);
      inked(cr, dark);
      break;
    case 34: // a propeller
      cairo_save(cr);
      cairo_translate(cr, 64, 64);
      for (int k = 0; k < 3; ++k)
      {
        cairo_rotate(cr, 2.0 * kPi / 3.0);
        ellipse(cr, 0, -28, 11, 30);
        inked(cr, rgb(170, 120, 70));
      }
      cairo_restore(cr);
      circle(cr, 64, 64, 10);
      inked(cr, gold);
      line(cr, 64, 74, 64, 128);
      stroke(cr, dark, 6);
      break;
    default: // the Titan's sales brochure on a little stand
      cairo_move_to(cr, 30, 128);
      cairo_line_to(cr, 64, 100);
      cairo_line_to(cr, 98, 128);
      stroke(cr, dark, 4);
      cairo_rectangle(cr, 30, 26, 68, 90);
      inked(cr, rgb(240, 244, 250));
      cairo_rectangle(cr, 30, 26, 68, 20);
      fill(cr, rgb(60, 140, 255));
      lettering(cr, "HALCYON", 64, 40, 10, rgb(255, 255, 255));
      roundRect(cr, 50, 56, 28, 44, 6);
      inked(cr, rgb(200, 206, 220), 1.4);
      circle(cr, 64, 70, 5);
      fill(cr, rgb(60, 140, 255));
      break;
  }
}

// A metal crate, 128 x 128: a ribbed steel box in a blue-grey frame, corner
// castings, a lifting plate on top for the magnet, a red magnet stencil and
// its number, a hazard band along its foot.
void paintMetalCrate(cairo_t* cr, const std::string& id, bool recess)
{
  cairo_rectangle(cr, 2, 2, 124, 124);
  lgrad(cr, 0, 2, 0, 126, {{0.0, rgb(170, 180, 196)}, {0.5, rgb(118, 128, 146)}, {1.0, rgb(70, 76, 92)}}, 1.0, true);
  stroke(cr, kInk, 3.0);
  // The ribs.
  for (double x = 16; x < 116; x += 12)
  {
    line(cr, x, 18, x, 106);
    stroke(cr, rgb(200, 208, 222), 1.4, 0.6);
    line(cr, x + 3, 18, x + 3, 106);
    stroke(cr, rgb(40, 44, 56), 1.4, 0.5);
  }
  // The frame rails top and bottom.
  for (const double y : {2.0, 108.0})
  {
    cairo_rectangle(cr, 2, y, 124, 18);
    lgrad(cr, 0, y, 0, y + 18, {{0.0, rgb(150, 160, 180)}, {1.0, rgb(60, 66, 82)}}, 1.0, true);
    stroke(cr, kInk, 2.0);
  }
  // A hazard band on the foot rail.
  cairo_save(cr);
  cairo_rectangle(cr, 20, 112, 88, 10);
  cairo_clip_preserve(cr);
  fill(cr, rgb(30, 28, 32));
  for (double k = 0; k < 120; k += 14)
  {
    cairo_move_to(cr, 20 + k, 112);
    cairo_line_to(cr, 27 + k, 112);
    cairo_line_to(cr, 17 + k, 122);
    cairo_line_to(cr, 10 + k, 122);
    cairo_close_path(cr);
  }
  fill(cr, kCraneYellow);
  cairo_restore(cr);
  // Corner castings.
  for (const double x : {2.0, 108.0})
    for (const double y : {2.0, 108.0})
    {
      cairo_rectangle(cr, x, y, 18, 18);
      lgrad(cr, 0, y, 0, y + 18, {{0.0, rgb(130, 136, 150)}, {1.0, rgb(50, 54, 66)}}, 1.0, true);
      stroke(cr, kInk, 1.6);
      ellipse(cr, x + 9, y + 9, 5, 3);
      fill(cr, kInk);
    }
  // The lifting plate (or the ride crate's recess for the magnet).
  if (recess)
  {
    cairo_rectangle(cr, 22, 2, 84, 10);
    fill(cr, rgb(20, 20, 26));
  }
  else
  {
    cairo_rectangle(cr, 38, 4, 52, 8);
    lgrad(cr, 0, 4, 0, 12, {{0.0, rgb(230, 234, 240)}, {1.0, rgb(110, 114, 126)}}, 1.0, true);
    stroke(cr, kInk, 1.2);
  }
  // The magnet stencil on a white disc, the crate's number under it.
  circle(cr, 64, 56, 22);
  fill(cr, rgb(236, 236, 230), 0.9);
  cairo_new_path(cr);
  cairo_arc_negative(cr, 64, 58, 12, kPi, 0);
  stroke(cr, rgb(200, 40, 40), 8);
  for (const double x : {52.0, 76.0})
  {
    cairo_rectangle(cr, x - 4, 44, 8, 8);
    fill(cr, rgb(230, 230, 236));
    cairo_rectangle(cr, x - 4, 44, 8, 8);
    stroke(cr, rgb(200, 40, 40), 1.4);
  }
  lettering(cr, id.c_str(), 64, 98, 15, rgb(240, 240, 236), 0.85);
}

// The rail: a yellow I-beam `len` px long, 30 tall, red buffer stops at its
// ends.
void paintRail(cairo_t* cr, double len)
{
  cairo_rectangle(cr, 0, 2, len, 26);
  fill(cr, kCraneYellowLo);
  cairo_rectangle(cr, 0, 10, len, 10);
  lgrad(cr, 0, 10, 0, 20, {{0.0, rgb(120, 80, 14)}, {1.0, rgb(90, 56, 10)}});
  for (const double y : {2.0, 20.0})
  {
    cairo_rectangle(cr, 0, y, len, 8);
    lgrad(cr, 0, y, 0, y + 8, {{0.0, rgb(255, 230, 120)}, {0.4, kCraneYellow}, {1.0, kCraneYellowLo}}, 1.0, true);
    stroke(cr, kInk, 1.4);
  }
  for (double x = 20; x < len - 10; x += 40)
  {
    circle(cr, x, 6, 1.6);
    circle(cr, x, 24, 1.6);
    fill(cr, rgb(110, 70, 10));
  }
  for (const double x : {0.0, len - 12})
  {
    cairo_rectangle(cr, x, 0, 12, 30);
    inked(cr, rgb(210, 50, 40), 1.6);
  }
}

// The trolley, 136 x 64: a yellow carriage straddling the rail (the rail
// runs through its top), the hoist drum, a capacity plate.
void paintTrolley(cairo_t* cr)
{
  roundRect(cr, 4, 6, 128, 54, 6);
  lgrad(cr, 0, 6, 0, 60, {{0.0, rgb(255, 230, 120)}, {0.3, kCraneYellow}, {1.0, kCraneYellowLo}}, 1.0, true);
  stroke(cr, kInk, 2.2);
  // The wheels on the rail's lower flange, behind the side plate's window.
  cairo_rectangle(cr, 12, 12, 112, 14);
  fill(cr, rgb(40, 34, 30));
  for (const double x : {30.0, 106.0})
  {
    circle(cr, x, 19, 7);
    inked(cr, rgb(120, 124, 134), 1.4);
  }
  // The hoist drum with the cable wound on it.
  ellipse(cr, 68, 42, 30, 12);
  inked(cr, rgb(80, 82, 92), 1.6);
  for (double x = 44; x < 94; x += 4)
  {
    line(cr, x, 32, x + 2, 52);
    stroke(cr, rgb(170, 174, 184), 1.2, 0.8);
  }
  cairo_rectangle(cr, 100, 34, 22, 14);
  inked(cr, rgb(240, 240, 236), 1.2);
  lettering(cr, "5T", 111, 45, 10, kInk);
  for (const double x : {10.0, 126.0})
  {
    circle(cr, x, 54, 2.2);
    fill(cr, rgb(110, 70, 10));
  }
}

// The magnet head, 136 x 48: a wide shallow drum with a domed cap and the
// cable's shackle, a band of chevrons round its side, the dark pole face
// underneath. Its bottom edge is the magnet's bottom.
void paintMagnet(cairo_t* cr)
{
  cairo_move_to(cr, 30, 18);
  cairo_curve_to(cr, 40, 2, 96, 2, 106, 18);
  cairo_close_path(cr);
  lgrad(cr, 0, 4, 0, 18, {{0.0, rgb(150, 154, 166)}, {1.0, rgb(70, 72, 84)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  circle(cr, 68, 4, 5);
  stroke(cr, kInk, 4.0);
  circle(cr, 68, 4, 5);
  stroke(cr, kSteel, 2.0);
  roundRect(cr, 4, 16, 128, 28, 6);
  lgrad(cr, 0, 16, 0, 44, {{0.0, rgb(110, 114, 126)}, {0.5, rgb(70, 72, 84)}, {1.0, rgb(36, 36, 44)}}, 1.0, true);
  stroke(cr, kInk, 2.2);
  cairo_save(cr);
  cairo_rectangle(cr, 6, 22, 124, 9);
  cairo_clip_preserve(cr);
  fill(cr, rgb(28, 26, 30));
  for (double k = -10; k < 140; k += 16)
  {
    cairo_move_to(cr, k, 22);
    cairo_line_to(cr, k + 8, 22);
    cairo_line_to(cr, k + 2, 31);
    cairo_line_to(cr, k - 6, 31);
    cairo_close_path(cr);
  }
  fill(cr, kCraneYellow);
  cairo_restore(cr);
  cairo_rectangle(cr, 8, 40, 120, 6);
  fill(cr, rgb(20, 20, 26));
  for (double x = 14; x < 126; x += 10)
  {
    line(cr, x, 40, x, 46);
    stroke(cr, rgb(90, 94, 110), 1.0, 0.8);
  }
}

// The crane's warning bell, 28 x 28, hung from its bracket.
void paintBell(cairo_t* cr)
{
  line(cr, 14, 0, 14, 6);
  stroke(cr, kInk, 3);
  cairo_move_to(cr, 4, 22);
  cairo_curve_to(cr, 4, 6, 24, 6, 24, 22);
  cairo_close_path(cr);
  lgrad(cr, 0, 6, 0, 22, {{0.0, rgb(255, 120, 100)}, {1.0, rgb(160, 30, 24)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  ellipse(cr, 14, 22, 10, 3);
  inked(cr, rgb(120, 20, 20), 1.2);
  circle(cr, 14, 26, 2.5);
  fill(cr, rgb(70, 70, 76));
}

// A lever, 64 x 112 (bottom on the floor): a grey switch box on its post,
// a hazard-striped plate, the handle (drawn live) pivoting on its front.
void paintLeverBox(cairo_t* cr)
{
  cairo_rectangle(cr, 26, 64, 12, 48);
  lgrad(cr, 26, 0, 38, 0, {{0.0, rgb(130, 134, 146)}, {1.0, rgb(50, 52, 62)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  cairo_rectangle(cr, 16, 104, 32, 8);
  inked(cr, rgb(70, 72, 84), 1.4);
  roundRect(cr, 8, 18, 48, 52, 5);
  lgrad(cr, 0, 18, 0, 70, {{0.0, rgb(150, 154, 166)}, {1.0, rgb(70, 72, 84)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  cairo_save(cr);
  cairo_rectangle(cr, 12, 56, 40, 10);
  cairo_clip_preserve(cr);
  fill(cr, rgb(30, 28, 32));
  for (double k = 0; k < 60; k += 10)
  {
    cairo_move_to(cr, 12 + k, 56);
    cairo_line_to(cr, 17 + k, 56);
    cairo_line_to(cr, 10 + k, 66);
    cairo_line_to(cr, 5 + k, 66);
    cairo_close_path(cr);
  }
  fill(cr, kCraneYellow);
  cairo_restore(cr);
  // The slot the handle swings in.
  roundRect(cr, 28, 22, 8, 32, 3);
  fill(cr, rgb(20, 20, 24));
}

// The black velvet curtain, one block wide and 8 tall: deep folds catching
// a little light on their crowns, the rod and gathered top, a hem pooling
// at the foot.
void paintCurtain(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 0, 64, 512);
  cairo_pattern_t* g = cairo_pattern_create_linear(0, 0, 64, 0);
  stop(g, 0.0, rgb(8, 6, 10));
  stop(g, 0.25, rgb(30, 26, 34));
  stop(g, 0.45, rgb(14, 12, 18));
  stop(g, 0.7, rgb(4, 4, 6));
  stop(g, 0.85, rgb(22, 20, 26));
  stop(g, 1.0, rgb(8, 6, 10));
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
  // Highlights on the crowns of the folds, brighter toward the top.
  for (const double x : {16.0, 54.0})
  {
    line(cr, x, 20, x + 2, 500);
    cairo_pattern_t* h = cairo_pattern_create_linear(0, 20, 0, 500);
    stop(h, 0.0, rgb(90, 80, 100), 0.35);
    stop(h, 1.0, rgb(90, 80, 100), 0.05);
    cairo_set_source(cr, h);
    cairo_set_line_width(cr, 2.0);
    cairo_stroke(cr);
    cairo_pattern_destroy(h);
  }
  // The rod and the gathered top.
  cairo_rectangle(cr, 0, 0, 64, 8);
  lgrad(cr, 0, 0, 0, 8, {{0.0, rgb(130, 120, 100)}, {1.0, rgb(50, 44, 40)}});
  for (const double x : {8.0, 40.0})
  {
    ellipse(cr, x + 8, 12, 9, 5);
    fill(cr, rgb(24, 20, 28));
  }
  // The hem.
  cairo_rectangle(cr, 0, 500, 64, 12);
  fill(cr, rgb(4, 4, 6));
}

// A scenery flat, 128 x 192: a painted face in a timber frame (look 0 a
// brick wall with a window, 1 a tree, 2 a castle wall), a stage weight on
// its brace; `damage` 0-2 tears in the canvas.
void paintFlat(cairo_t* cr, int look, int damage)
{
  // The brace and its sandbag behind.
  line(cr, 20, 40, 4, 190);
  stroke(cr, kInk, 7);
  line(cr, 20, 40, 4, 190);
  stroke(cr, rgb(150, 110, 70), 4);
  roundRect(cr, -2, 176, 26, 16, 5);
  inked(cr, rgb(150, 130, 90), 1.4);
  cairo_rectangle(cr, 10, 4, 112, 188);
  stroke(cr, kInk, 10);
  cairo_rectangle(cr, 14, 8, 104, 180);
  if (look == 0)
  {
    fill(cr, rgb(170, 80, 60));
    for (double y = 8; y < 188; y += 14)
      for (double x = 14 + (int(y / 14) % 2) * 12; x < 118; x += 24)
      {
        cairo_rectangle(cr, x + 1, y + 1, 22, 12);
        fill(cr, rgb(190, 96, 72));
      }
    cairo_rectangle(cr, 40, 46, 52, 60);
    inked(cr, rgb(40, 50, 90));
    line(cr, 66, 46, 66, 106);
    line(cr, 40, 76, 92, 76);
    stroke(cr, rgb(230, 220, 200), 4);
  }
  else if (look == 1)
  {
    fill(cr, rgb(110, 160, 210));
    cairo_rectangle(cr, 58, 110, 18, 78);
    inked(cr, rgb(110, 70, 40), 1.4);
    for (const auto& c : {std::array<double, 3>{66, 70, 36}, {44, 90, 24}, {90, 92, 24}, {66, 40, 26}})
    {
      circle(cr, c[0], c[1], c[2]);
      inked(cr, rgb(70, 150, 70), 1.4);
    }
    cairo_rectangle(cr, 14, 170, 104, 18);
    fill(cr, rgb(80, 140, 60));
  }
  else
  {
    fill(cr, rgb(140, 140, 150));
    for (double y = 30; y < 188; y += 22)
      for (double x = 14 + (int(y / 22) % 2) * 18; x < 118; x += 36)
      {
        cairo_rectangle(cr, x + 1.5, y + 1.5, 33, 19);
        fill(cr, rgb(160, 160, 170));
      }
    for (double x = 14; x < 118; x += 26)
    {
      cairo_rectangle(cr, x, 8, 14, 22);
      fill(cr, rgb(150, 150, 160));
    }
    cairo_move_to(cr, 50, 188);
    cairo_line_to(cr, 50, 140);
    cairo_curve_to(cr, 50, 116, 82, 116, 82, 140);
    cairo_line_to(cr, 82, 188);
    cairo_close_path(cr);
    inked(cr, rgb(50, 36, 30));
  }
  cairo_rectangle(cr, 14, 8, 104, 180);
  stroke(cr, rgb(170, 130, 80), 4);
  // Tears in the canvas, the timber showing through.
  for (int k = 0; k < damage; ++k)
  {
    const double x = k == 0 ? 80 : 40, y = k == 0 ? 60 : 130;
    cairo_move_to(cr, x, y);
    cairo_line_to(cr, x + 10, y + 6);
    cairo_line_to(cr, x + 4, y + 16);
    cairo_line_to(cr, x + 16, y + 26);
    cairo_line_to(cr, x - 6, y + 20);
    cairo_line_to(cr, x - 2, y + 8);
    cairo_close_path(cr);
    inked(cr, rgb(30, 22, 18), 1.2);
  }
}

// A roll of gaffer tape, 30 x 30, its middle at (15, 15).
void paintTape(cairo_t* cr)
{
  circle(cr, 15, 15, 13);
  lgrad(cr, 0, 2, 0, 28, {{0.0, rgb(200, 204, 212)}, {1.0, rgb(90, 94, 106)}}, 1.0, true);
  stroke(cr, kInk, 1.8);
  circle(cr, 15, 15, 6);
  inked(cr, rgb(170, 130, 80), 1.4);
  circle(cr, 15, 15, 3.5);
  fill(cr, rgb(20, 18, 22));
  line(cr, 27, 18, 29, 26);
  stroke(cr, rgb(170, 174, 184), 3);
}

// The Magnet Gun's icon, 64 x 64.
void paintMagnetIcon(cairo_t* cr)
{
  // The grip.
  cairo_move_to(cr, 14, 34);
  cairo_line_to(cr, 26, 34);
  cairo_line_to(cr, 22, 56);
  cairo_line_to(cr, 10, 56);
  cairo_close_path(cr);
  inked(cr, rgb(50, 50, 60), 1.6);
  // The body and its coil.
  roundRect(cr, 6, 22, 34, 16, 4);
  inked(cr, rgb(120, 124, 140), 1.6);
  for (double x = 12; x < 36; x += 4)
  {
    line(cr, x, 23, x, 37);
    stroke(cr, rgb(210, 140, 60), 2.0);
  }
  // The horseshoe magnet on the front, its poles pointing ahead.
  cairo_new_path(cr);
  cairo_arc(cr, 44, 30, 13, kPi * 0.5, kPi * 1.5);
  cairo_line_to(cr, 60, 17);
  cairo_line_to(cr, 60, 25);
  cairo_line_to(cr, 44, 25);
  cairo_arc_negative(cr, 44, 30, 5, kPi * 1.5, kPi * 0.5);
  cairo_line_to(cr, 60, 35);
  cairo_line_to(cr, 60, 43);
  cairo_close_path(cr);
  lgrad(cr, 0, 17, 0, 43, {{0.0, rgb(255, 110, 100)}, {1.0, rgb(170, 20, 20)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  for (const double y : {17.0, 35.0})
  {
    cairo_rectangle(cr, 53, y, 7, 8);
    inked(cr, rgb(230, 232, 240), 1.2);
  }
}

} // namespace

// --- Map blocks the level draws itself -------------------------------------------------------------------

// A Prop Mimic's stencil (PROPS, FRAGILE - THIS WAY UP and an arrow), 128
// x 60, PROPS's baseline at y 22: drawn over its sprite, not in it, so it
// reads the right way round whichever way the mimic faces.
void paintMimicLabel(cairo_t* cr)
{
  lettering(cr, "PROPS", 64, 22, 20, rgb(40, 24, 14), 0.75);
  lettering(cr, "FRAGILE - THIS WAY UP", 64, 38, 8, rgb(150, 30, 30), 0.8);
  line(cr, 64, 44, 64, 58);
  line(cr, 58, 50, 64, 44);
  line(cr, 64, 44, 70, 50);
  stroke(cr, rgb(150, 30, 30), 2.0, 0.8);
}

bool World::warehouseDrawsBlock(int tx, int ty) const
{
  const auto& s = mWarehouse;
  for (const auto& c : s.crates)
    if (c.stamped && tx >= c.x / kCellsPerTile && tx < c.x / kCellsPerTile + 2 && ty >= c.y / kCellsPerTile &&
        ty < c.y / kCellsPerTile + 2)
      return true;
  if (s.throwAll)
    for (const auto& [lx, ly] : s.loose)
      if (lx == tx && ly == ty)
        return mMap.block(tx, ty) == Tile::Solid;
  return false;
}

// --- Behind the tiles ------------------------------------------------------------------------------------

void drawWarehouseSet(Renderer& r, const World& w, const Art& art, float camX, float camY, int frame, float alpha)
{
  (void)alpha;
  const auto& s = w.warehouse();
  if (!s.on)
    return;
  auto bx = [&](float block) { return block * kTilePx - camX; };
  auto by = [&](float block) { return block * kTilePx - camY; };
  if (s.throwAll)
  {
    // Throw Everything: a practice room, a banner over the floor, a roof
    // hatch over the exit.
    const Level& lv = w.level();
    const float hx = float(lv.exitTx) * kTilePx - camX, hy = float(lv.exitTy - 3) * kTilePx - camY;
    if (!offScreen(hx - 60.0f, hy - 40.0f, 180.0f, 80.0f))
    {
      r.fillRect(hx - 40.0f, hy - 16.0f, 144.0f, 20.0f, rgb(60, 62, 72));
      r.fillRect(hx - 36.0f, hy - 12.0f, 136.0f, 12.0f, rgb(30, 50, 90));
      r.drawText("ROOF HATCH", hx + 32.0f, hy - 40.0f, {18.0f, rgb(255, 230, 150), kHudInk}, Align::Center);
    }
    const float tx = bx(20.0f), ty = by(13.5f);
    if (!offScreen(tx - 300.0f, ty - 30.0f, 600.0f, 60.0f))
    {
      r.fillRect(tx - 260.0f, ty - 26.0f, 520.0f, 52.0f, rgb(150, 30, 40));
      r.fillRect(tx - 260.0f, ty - 26.0f, 520.0f, 5.0f, rgb(220, 70, 70));
      r.drawText("STAGE COMBAT PRACTICE - THROW EVERYTHING", tx, ty - 14.0f, {20.0f, rgb(255, 240, 210), kHudInk}, Align::Center);
    }
    return;
  }
  // 1. The loading dock's back wall and its roll-up door, half open on the
  // studio lot at night.
  {
    const float x0 = bx(0.0f), y0 = by(38.0f), x1 = bx(32.0f), y1 = by(56.0f);
    if (!offScreen(x0, y0, x1 - x0, y1 - y0))
    {
      r.fillRect(x0, y0, x1 - x0, y1 - y0, rgb(34, 32, 36));
      for (float y = y0; y < y1; y += 32.0f)
        r.fillRect(x0, y, x1 - x0, 2.0f, rgb(26, 24, 28));
      for (int k = 0; k < 16; ++k)
        r.fillRect(x0 + float(k) * 128.0f + float((k * 37) % 50), y0, 2.0f, y1 - y0, rgb(28, 26, 30));
      // The doorway, the lot outside.
      const float dx = bx(3.0f), dw = 9.0f * kTilePx, dy = by(46.0f), dh = 10.0f * kTilePx;
      r.draw(baked(art, r, "lot", int(dw), int(dh), 0, 0, [dw, dh](cairo_t* cr) {
        cairo_rectangle(cr, 0, 0, dw, dh);
        lgrad(cr, 0, 0, 0, dh, {{0.0, rgb(14, 18, 40)}, {0.6, rgb(36, 40, 74)}, {1.0, rgb(60, 54, 70)}});
        Rng rng(36101u);
        for (int i = 0; i < 40; ++i)
        {
          circle(cr, rng.range(0, float(dw)), rng.range(0, float(dh * 0.6)), rng.range(0.6f, 1.4f));
          fill(cr, rgb(230, 236, 255), rng.range(0.3f, 0.8f));
        }
        // The lot's far stage buildings and the water tower.
        cairo_rectangle(cr, 0, dh - 260, dw, 140);
        fill(cr, rgb(20, 20, 32));
        for (const double bx0 : {40.0, 300.0})
        {
          cairo_rectangle(cr, bx0, dh - 330, 200, 210);
          fill(cr, rgb(24, 24, 38));
          lettering(cr, bx0 < 100 ? "STAGE 4" : "STAGE 5", bx0 + 100, dh - 290, 18, rgb(120, 120, 150), 0.6);
        }
        const double tx = 470, ty = dh - 470;
        for (const double lx : {-34.0, 34.0})
        {
          line(cr, tx + lx * 0.6, ty + 50, tx + lx, dh - 300);
          stroke(cr, rgb(16, 16, 26), 5);
        }
        cairo_rectangle(cr, tx - 46, ty, 92, 54);
        fill(cr, rgb(30, 30, 46));
        cairo_move_to(cr, tx - 50, ty);
        cairo_line_to(cr, tx, ty - 26);
        cairo_line_to(cr, tx + 50, ty);
        cairo_close_path(cr);
        fill(cr, rgb(30, 30, 46));
        lettering(cr, "MAXTV", tx, ty + 36, 22, rgb(255, 110, 200), 0.7);
        glow(cr, tx, ty - 30, 12, rgb(255, 60, 40), 0.9);
        // A street lamp's sodium pool on the tarmac.
        line(cr, 120, dh - 120, 120, dh - 300);
        line(cr, 120, dh - 300, 150, dh - 300);
        stroke(cr, rgb(20, 20, 26), 5);
        glow(cr, 150, dh - 296, 90, rgb(255, 170, 70), 0.55);
        cairo_rectangle(cr, 0, dh - 120, dw, 120);
        fill(cr, rgb(40, 38, 46));
        glow(cr, 150, dh - 60, 160, rgb(255, 160, 70), 0.25);
        for (double x = 20; x < dw; x += 140)
        {
          cairo_rectangle(cr, x, dh - 70, 70, 6);
          fill(cr, rgb(220, 200, 120), 0.5);
        }
      }),
        dx, dy);
      // The roll-up door, rolled half way up, its slats and its drum.
      const float open = dh * 0.5f;
      for (float y = dy; y < dy + dh - open; y += 18.0f)
      {
        r.fillRect(dx, y, dw, 18.0f, rgb(110, 116, 128));
        r.fillRect(dx, y, dw, 3.0f, rgb(170, 176, 188));
        r.fillRect(dx, y + 15.0f, dw, 3.0f, rgb(60, 64, 76));
      }
      r.fillRect(dx, dy + dh - open - 6.0f, dw, 10.0f, rgb(40, 40, 46));
      r.fillRect(dx - 24.0f, dy - 40.0f, dw + 48.0f, 40.0f, rgb(70, 72, 84));
      r.fillRect(dx - 24.0f, dy - 40.0f, dw + 48.0f, 5.0f, rgb(130, 134, 146));
      for (const float gx : {dx - 18.0f, dx + dw + 6.0f})
        r.fillRect(gx, dy - 4.0f, 12.0f, dh + 4.0f, rgb(90, 94, 106));
      // DOCK 1 and its amber beacon.
      r.fillRect(dx + dw * 0.5f - 90.0f, dy - 110.0f, 180.0f, 56.0f, rgb(240, 196, 40));
      r.fillRect(dx + dw * 0.5f - 84.0f, dy - 104.0f, 168.0f, 44.0f, rgb(30, 28, 32));
      r.drawText("DOCK 1", dx + dw * 0.5f, dy - 102.0f, {32.0f, rgb(255, 214, 80), 0}, Align::Center);
      const bool beacon = (frame / 12) % 2 == 0;
      r.fillRect(dx + dw + 40.0f, dy - 60.0f, 20.0f, 16.0f, beacon ? rgb(255, 180, 40) : rgb(140, 80, 20));
      if (beacon)
        drawGlow(r, art, dx + dw + 50.0f, dy - 52.0f, 60.0f, rgb(255, 170, 50), 0.6f);
      // A sign over the dock ledge.
      const float sx0 = bx(21.0f), sy0 = by(44.0f);
      r.fillRect(sx0, sy0, 8.0f * kTilePx, 90.0f, rgb(230, 228, 220));
      r.fillRect(sx0 + 6.0f, sy0 + 6.0f, 8.0f * kTilePx - 12.0f, 78.0f, rgb(40, 60, 120));
      r.drawText("MAXTV PROP STORE", sx0 + 4.0f * kTilePx, sy0 + 12.0f, {30.0f, rgb(255, 255, 255), 0}, Align::Center);
      r.drawText("DELIVERIES - NO STEP - KEEP AISLES CLEAR", sx0 + 4.0f * kTilePx, sy0 + 52.0f, {17.0f, rgb(255, 214, 80), 0},
        Align::Center);
    }
  }
  // 2. The aisles: pallet-rack uprights behind the shelf boards, a hanging
  // aisle sign.
  for (const float ux : {33.8f, 46.4f, 59.0f})
  {
    const float x = bx(ux), top = by(42.5f), bottom = by(56.0f);
    if (offScreen(x, top, 30.0f, bottom - top))
      continue;
    r.fillRect(x, top, 14.0f, bottom - top, rgb(36, 70, 150));
    r.fillRect(x, top, 4.0f, bottom - top, rgb(80, 120, 210));
    for (float y = top + 8.0f; y < bottom; y += 18.0f)
      r.fillRect(x + 6.0f, y, 4.0f, 8.0f, rgb(14, 20, 40));
    r.fillRect(x - 4.0f, bottom - 8.0f, 22.0f, 8.0f, rgb(60, 62, 72));
  }
  for (const auto& sign : {std::pair<float, const char*>{40.0f, "AISLE 7"}, {54.0f, "AISLE 8"}, {72.0f, "PROP MAZE"},
         {112.0f, "CRANE YARD"}, {139.0f, "FREIGHT"}})
  {
    const float x = bx(sign.first), y = by(sign.first > 100.0f ? 31.0f : (sign.first > 60.0f ? 34.5f : 38.0f));
    if (offScreen(x - 120.0f, y - 300.0f, 240.0f, 400.0f))
      continue;
    r.drawLine(x - 60.0f, y - 600.0f, x - 60.0f, y, 2.0f, rgb(20, 20, 24));
    r.drawLine(x + 60.0f, y - 600.0f, x + 60.0f, y, 2.0f, rgb(20, 20, 24));
    r.fillRect(x - 84.0f, y, 168.0f, 44.0f, rgb(40, 110, 60));
    r.fillRect(x - 80.0f, y + 4.0f, 160.0f, 36.0f, rgb(30, 80, 46));
    r.drawText(sign.second, x, y + 8.0f, {22.0f, rgb(240, 250, 240), 0}, Align::Center);
  }
  // 3. The prop maze's black curtains, and the merch mezzanine's sign.
  {
    const Texture& curtain = baked(art, r, "curtain", 64, 512, 0, 0, paintCurtain);
    for (int k = 65; k <= 96; ++k)
    {
      const float x = bx(float(k)), y = by(42.0f);
      if (offScreen(x, y, 64.0f, 512.0f))
        continue;
      r.draw(curtain, x, y);
    }
    const float mx = bx(72.5f), my = by(29.0f);
    if (!offScreen(mx - 200.0f, my, 400.0f, 80.0f))
    {
      r.fillRect(mx - 170.0f, my, 340.0f, 70.0f, rgb(30, 24, 40));
      for (int k = 0; k < 18; ++k)
      {
        const bool on = (k + frame / 8) % 3 != 0;
        drawGlow(r, art, mx - 160.0f + float(k) * 18.8f, my + (k % 2 ? 66.0f : 4.0f), 8.0f, rgb(255, 210, 120), on ? 0.9f : 0.3f);
      }
      r.drawText("MAXTV MERCH", mx, my + 6.0f, {28.0f, rgb(255, 120, 210), kHudInk}, Align::Center);
      r.drawText("ONE FOR EVERY EPISODE", mx, my + 40.0f, {16.0f, rgb(255, 230, 180), 0}, Align::Center);
    }
  }
  // 4. The crane yard: the pit's back wall with its hazard band, and the
  // gantry's yellow legs up to the ride cranes' rails.
  {
    const float x0 = bx(99.0f), x1 = bx(129.0f), y0 = by(50.0f), y1 = by(62.0f);
    if (!offScreen(x0, y0, x1 - x0, y1 - y0))
    {
      r.fillRect(x0, y0, x1 - x0, y1 - y0, rgb(30, 30, 36));
      for (float y = y0 + 64.0f; y < y1; y += 64.0f)
        r.fillRect(x0, y, x1 - x0, 2.0f, rgb(22, 22, 26));
      for (float x = x0; x < x1; x += 40.0f)
      {
        r.fillRect(x, y0 + 4.0f, 20.0f, 18.0f, rgb(240, 196, 40));
        r.fillRect(x + 20.0f, y0 + 4.0f, 20.0f, 18.0f, rgb(26, 24, 28));
      }
      const float sx0 = bx(103.0f), sy0 = by(53.0f);
      r.fillRect(sx0, sy0, 240.0f, 64.0f, rgb(240, 196, 40));
      r.fillRect(sx0 + 5.0f, sy0 + 5.0f, 230.0f, 54.0f, rgb(30, 28, 32));
      r.drawText("DANGER", sx0 + 120.0f, sy0 + 8.0f, {22.0f, rgb(255, 210, 60), 0}, Align::Center);
      r.drawText("FOAM PIT - DO NOT DIVE", sx0 + 120.0f, sy0 + 34.0f, {15.0f, rgb(255, 255, 255), 0}, Align::Center);
    }
    for (const float lx : {100.1f, 110.6f, 120.6f, 128.6f})
    {
      const float x = bx(lx), top = by(36.0f), bottom = by(50.0f);
      if (offScreen(x - 40.0f, top, 80.0f, bottom - top))
        continue;
      r.fillRect(x, top, 22.0f, bottom - top, rgb(200, 150, 26));
      r.fillRect(x, top, 6.0f, bottom - top, rgb(255, 220, 110));
      r.fillRect(x + 18.0f, top, 4.0f, bottom - top, rgb(120, 80, 14));
      for (float y = top + 40.0f; y + 100.0f < bottom; y += 120.0f)
      {
        r.drawLine(x + 11.0f, y, x + 11.0f - 60.0f, y + 100.0f, 6.0f, rgb(170, 120, 20));
        r.drawLine(x + 11.0f, y, x + 11.0f + 60.0f, y + 100.0f, 6.0f, rgb(170, 120, 20));
      }
      r.fillRect(x - 8.0f, bottom - 10.0f, 38.0f, 10.0f, rgb(70, 72, 84));
    }
  }
  // 5. The freight lift shafts: guide rails, a cage of mesh behind, the
  // floor numbers; the counterweight's cables.
  for (const auto& pl : w.platforms())
  {
    if (pl.crane || pl.path.size() < 2 || !pl.waitRider)
      continue;
    int lo = pl.path[0].second, hi = pl.path[0].second;
    for (const auto& pt : pl.path)
    {
      lo = std::min(lo, pt.second);
      hi = std::max(hi, pt.second);
    }
    const float x = sx(pl.path[0].first, camX), wpx = float(pl.w) * kCellPx;
    const float top = sy(lo - 12, camY), bottom = sy(hi + 2, camY);
    if (offScreen(x - 40.0f, top, wpx + 80.0f, bottom - top))
      continue;
    r.fillRect(x, top, wpx, bottom - top, rgb(22, 22, 28));
    for (float k = 0.0f; k < bottom - top; k += 24.0f)
    {
      r.drawLine(x, top + k, x + wpx, top + k + 24.0f, 1.0f, rgba(120, 120, 140, 60));
      r.drawLine(x + wpx, top + k, x, top + k + 24.0f, 1.0f, rgba(120, 120, 140, 60));
    }
    for (const float gx : {x - 10.0f, x + wpx})
    {
      r.fillRect(gx, top, 10.0f, bottom - top, rgb(90, 96, 110));
      r.fillRect(gx, top, 3.0f, bottom - top, rgb(150, 156, 170));
    }
    for (int y = lo; y <= hi; y += 18)
    {
      char buf[16];
      std::snprintf(buf, sizeof buf, "%d", y == hi ? 1 : 2);
      r.drawText(buf, x + wpx * 0.5f, sy(y - 9, camY), {40.0f, rgba(240, 196, 40, 120), 0}, Align::Center);
    }
  }
  // 6. The dressing-room landing: a cream wall, a mirror ringed with bulbs,
  // a rail of costumes, a star.
  {
    const float x0 = bx(133.0f), x1 = bx(149.0f), y0 = by(6.0f), y1 = by(14.0f);
    if (!offScreen(x0, y0, x1 - x0, y1 - y0))
    {
      r.fillRect(x0, y0, x1 - x0, y1 - y0, rgb(70, 54, 50));
      r.fillRect(x0, y1 - 80.0f, x1 - x0, 80.0f, rgb(90, 30, 40));
      r.fillRect(x0, y1 - 84.0f, x1 - x0, 6.0f, rgb(200, 160, 80));
      const float mx = bx(137.0f), my = by(9.0f);
      r.fillRect(mx, my, 160.0f, 110.0f, rgb(200, 210, 220));
      r.fillRect(mx + 8.0f, my + 8.0f, 144.0f, 94.0f, rgb(90, 110, 130));
      for (int k = 0; k < 6; ++k)
      {
        drawGlow(r, art, mx + float(k) * 32.0f, my - 4.0f, 14.0f, rgb(255, 220, 140), 0.9f);
        drawGlow(r, art, mx + float(k) * 32.0f, my + 114.0f, 14.0f, rgb(255, 220, 140), 0.9f);
      }
      const float rx = bx(141.5f), ry = by(10.0f);
      r.drawLine(rx, ry, rx + 160.0f, ry, 4.0f, rgb(200, 200, 210));
      static const Color kCostume[5] = {rgb(200, 40, 60), rgb(40, 120, 200), rgb(240, 200, 60), rgb(120, 60, 160), rgb(40, 160, 90)};
      for (int k = 0; k < 5; ++k)
        r.fillRect(rx + 10.0f + float(k) * 30.0f, ry + 6.0f, 24.0f, 120.0f + float(k % 2) * 30.0f, kCostume[k]);
      r.drawText("DRESSING ROOMS", bx(141.0f), by(7.0f), {26.0f, rgb(255, 230, 180), kHudInk}, Align::Center);
    }
  }
  // 7. The props of Levels 1-35, each on its own little shelf with its tag.
  for (const auto& d : s.props)
  {
    const float x = sx(d.x, camX), foot = sy(d.y + 1, camY);
    if (offScreen(x - 20.0f, foot - 140.0f, 170.0f, 170.0f))
      continue;
    r.fillRect(x - 10.0f, foot, 148.0f, 10.0f, rgb(110, 76, 44));
    r.fillRect(x - 10.0f, foot, 148.0f, 3.0f, rgb(170, 126, 80));
    for (const float bxk : {x + 4.0f, x + 116.0f})
      r.drawLine(bxk, foot + 10.0f, bxk + 14.0f, foot + 34.0f, 4.0f, rgb(60, 60, 68));
    if (d.label > 0)
      drawGlow(r, art, x + 64.0f, foot - 64.0f, 110.0f, rgb(255, 220, 160), 0.35f * float(d.label) / float(kPropLabel) + 0.1f);
    r.draw(baked(art, r, "prop/" + std::to_string(d.level), 128, 128, 0, 128, [&d](cairo_t* cr) { paintProp(cr, d.level); }), x,
      foot);
    // The tag on its string.
    const float swing = d.label > 0 ? std::sin(float(frame) * 0.25f) * 4.0f : 0.0f;
    r.drawLine(x + 120.0f, foot + 4.0f, x + 124.0f + swing, foot + 22.0f, 1.5f, rgb(230, 220, 200));
    r.fillRect(x + 112.0f + swing, foot + 22.0f, 26.0f, 16.0f, rgb(240, 230, 200));
    char buf[8];
    std::snprintf(buf, sizeof buf, "%d", d.level);
    r.drawText(buf, x + 125.0f + swing, foot + 22.0f, {13.0f, rgb(60, 40, 30), 0}, Align::Center);
  }
}

// --- Behind the runner ----------------------------------------------------------------------------------

void World::drawWarehouseBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& s = mWarehouse;
  // Foam in the crane yard's pit: soft cubes of stage foam piled up.
  for (const auto& f : s.foam)
  {
    const float x0 = sx(f.x, camX), y0 = sy(f.y, camY), wpx = float(f.w) * kCellPx, hpx = float(f.h) * kCellPx;
    if (offScreen(x0, y0, wpx, hpx))
      continue;
    static const Color kFoam[4] = {rgb(70, 130, 220), rgb(240, 196, 70), rgb(220, 80, 80), rgb(80, 180, 120)};
    for (int row = 0; row < f.h; ++row)
      for (int i = -1; i <= f.w; ++i)
      {
        const float cx = x0 + float(i) * kCellPx + float(row % 2) * 16.0f;
        if (cx < x0 || cx + kCellPx > x0 + wpx || cx > float(kScreenW) + 40.0f || cx < -40.0f)
          continue;
        const std::uint32_t h = hash2(f.x + i, f.y + row);
        const float cy = y0 + float(row) * kCellPx + float(h % 5u) - 2.0f;
        const Color c = kFoam[h % 4u];
        r.fillRect(cx + 1.0f, cy + 1.0f, kCellPx - 2.0f, kCellPx - 2.0f, c);
        r.fillRect(cx + 1.0f, cy + 1.0f, kCellPx - 2.0f, 6.0f, lerpColor(c, rgb(255, 255, 255), 0.35f));
        r.fillRect(cx + kCellPx - 7.0f, cy + 1.0f, 6.0f, kCellPx - 2.0f, lerpColor(c, rgb(0, 0, 0), 0.25f));
        for (int k = 0; k < 3; ++k)
          r.fillRect(cx + 6.0f + float(k) * 8.0f, cy + 12.0f + float((h >> (k + 3)) % 10u), 3.0f, 3.0f,
            lerpColor(c, rgb(0, 0, 0), 0.2f));
      }
  }
  if (!s.throwAll)
  {
    // CRATE 42, stencilled on a long wooden crate on the aisle floor.
    for (const auto& pr : mProps)
    {
      if (pr.kind != PropKind::Deco42)
        continue;
      const float cx = (float(pr.x) + float(pr.w) * 0.5f) * kCellPx - camX, foot = float(pr.y + pr.h) * kCellPx - camY;
      if (offScreen(cx - 100.0f, foot - 100.0f, 200.0f, 100.0f))
        continue;
      r.draw(baked(mArt, r, "crate42", 180, 96, 90, 96, [](cairo_t* cr) {
        woodCrate(cr, 2, 2, 176, 92, kWood);
        lettering(cr, "CRATE 42", 90, 62, 30, rgb(30, 20, 14), 0.85);
        lettering(cr, "THIS WAY UP", 90, 82, 11, rgb(160, 30, 30), 0.8);
      }),
        cx, foot);
    }
    // The EP. 1 BLOOPERS crate: a big old crate, cracking with each hit.
    for (const auto& b : mBreakables)
    {
      if (b.look != 0)
        continue;
      const float x = float(b.x0) * kTilePx - camX, y = float(b.y0) * kTilePx - camY;
      const float wpx = float(b.x1 - b.x0 + 1) * kTilePx, hpx = float(b.y1 - b.y0 + 1) * kTilePx;
      if (offScreen(x, y, wpx, hpx))
        continue;
      if (b.broken)
      {
        // Splintered planks either side of the opening.
        for (const float px : {x + 6.0f, x + wpx - 30.0f})
        {
          r.fillRect(px, y + hpx - 90.0f, 24.0f, 90.0f, kWoodLo);
          r.drawLine(px, y + hpx - 90.0f, px + 12.0f, y + hpx - 110.0f, 6.0f, kWood);
        }
        continue;
      }
      const int hits = std::clamp(3 - b.hp, 0, 2);
      r.draw(baked(mArt, r, "bloopers/" + std::to_string(hits), int(wpx), int(hpx), 0, 0,
               [wpx, hpx, hits](cairo_t* cr) {
                 woodCrate(cr, 2, 2, wpx - 4, hpx - 4, rgb(150, 110, 66));
                 lettering(cr, "EP. 1", wpx * 0.5, hpx * 0.38, 40, rgb(30, 20, 14), 0.85);
                 lettering(cr, "BLOOPERS", wpx * 0.5, hpx * 0.56, 42, rgb(30, 20, 14), 0.85);
                 cairo_save(cr);
                 cairo_translate(cr, wpx * 0.5, hpx * 0.76);
                 cairo_rotate(cr, -0.12);
                 cairo_rectangle(cr, -84, -22, 168, 34);
                 stroke(cr, rgb(200, 30, 30), 4, 0.85);
                 lettering(cr, "DO NOT AIR", 0, 6, 24, rgb(200, 30, 30), 0.85);
                 cairo_restore(cr);
                 for (int k = 0; k < hits; ++k)
                 {
                   const double cx = k == 0 ? wpx * 0.3 : wpx * 0.7, cy = k == 0 ? hpx * 0.3 : hpx * 0.65;
                   cairo_move_to(cr, cx, cy);
                   for (int q = 0; q < 6; ++q)
                   {
                     const double a = q * kPi / 3.0 + k;
                     cairo_move_to(cr, cx, cy);
                     cairo_line_to(cr, cx + std::cos(a) * 34, cy + std::sin(a) * 30);
                     cairo_line_to(cr, cx + std::cos(a + 0.2) * 52, cy + std::sin(a + 0.2) * 46);
                   }
                   stroke(cr, rgb(30, 18, 10), 2.4);
                   circle(cr, cx, cy, 8);
                   fill(cr, rgb(20, 12, 8));
                 }
               }),
        x, y);
    }
    // The TURBODUDES crate; shot open, its side panel plays the old title.
    if (s.tdX >= 0)
    {
      const CellBox b = boxAt(s.tdX, s.tdY, 4, 4);
      const float x = sx(b.x, camX), y = sy(b.y, camY);
      if (!offScreen(x, y, 128.0f, 128.0f))
      {
        const int hits = std::min(s.tdHits, 2);
        r.draw(baked(mArt, r, "turbodudes/" + std::to_string(hits), 128, 128, 0, 0, [hits](cairo_t* cr) {
          woodCrate(cr, 2, 2, 124, 124, rgb(140, 104, 60));
          if (hits < 2)
          {
            lettering(cr, "TURBO", 64, 52, 26, rgb(30, 20, 14), 0.85);
            lettering(cr, "DUDES", 64, 80, 26, rgb(30, 20, 14), 0.85);
            lettering(cr, "PROTOTYPE - 1 OF 1", 64, 104, 9, rgb(150, 30, 30), 0.8);
            if (hits == 1)
            {
              line(cr, 90, 20, 70, 60);
              line(cr, 70, 60, 84, 96);
              stroke(cr, rgb(30, 18, 10), 2.4);
            }
          }
          else
          {
            // The side panel pried open: an old monitor inside.
            cairo_rectangle(cr, 14, 14, 100, 92);
            inked(cr, rgb(60, 56, 54));
            roundRect(cr, 22, 22, 84, 64, 10);
            fill(cr, rgb(10, 12, 14));
            for (const double px : {30.0, 96.0})
            {
              circle(cr, px, 96, 3);
              fill(cr, rgb(120, 120, 120));
            }
          }
        }),
          x, y);
        if (hits >= 2)
        {
          const float mx = x + 22.0f, my = y + 22.0f;
          if (s.tdShow > 0)
          {
            // The PoC's old title screen: a starfield, chunky bouncing
            // letters in rainbow bands, PRESS START blinking.
            r.fillRect(mx, my, 84.0f, 64.0f, rgb(10, 10, 40));
            for (int k = 0; k < 12; ++k)
            {
              const float stx = std::fmod(float(hash2(k, 361) % 84u) + float(frame) * float(1 + k % 3) * 0.5f, 84.0f);
              r.fillRect(mx + 84.0f - stx, my + float(hash2(k, 362) % 60u), 2.0f, 2.0f, rgb(220, 220, 255));
            }
            static const char* kTitle = "TURBODUDES";
            static const Color kBands[5] = {rgb(255, 70, 70), rgb(255, 180, 40), rgb(250, 250, 80), rgb(80, 240, 120), rgb(80, 180, 255)};
            for (int k = 0; k < 10; ++k)
            {
              const char ch[2] = {kTitle[k], 0};
              const float bob = std::sin(float(frame) * 0.3f + float(k) * 0.6f) * 3.0f;
              r.drawText(ch, mx + 6.0f + float(k) * 7.8f, my + 14.0f + bob, {13.0f, kBands[(k + frame / 6) % 5], rgb(0, 0, 0)},
                Align::Left);
            }
            if ((frame / 15) % 2 == 0)
              r.drawText("PRESS START", mx + 42.0f, my + 42.0f, {10.0f, rgb(255, 255, 255), 0}, Align::Center);
            for (float yy = my; yy < my + 64.0f; yy += 3.0f)
              r.fillRect(mx, yy, 84.0f, 1.0f, rgba(0, 0, 0, 70));
            drawGlow(r, mArt, mx + 42.0f, my + 32.0f, 70.0f, rgb(120, 160, 255), 0.35f);
          }
          else
            r.fillRect(mx + 30.0f, my + 31.0f, 24.0f, 2.0f, rgba(200, 220, 255, 90));
        }
      }
    }
    // The dock ledge's rubber bumpers and its hazard-striped lip.
    {
      const float x = 18.0f * kTilePx - camX, y = 51.0f * kTilePx - camY;
      if (!offScreen(x - 40.0f, y, 80.0f, 320.0f))
        for (const float by0 : {y + 60.0f, y + 190.0f})
        {
          r.fillRect(x - 18.0f, by0, 22.0f, 70.0f, rgb(24, 22, 26));
          r.fillRect(x - 18.0f, by0, 6.0f, 70.0f, rgb(60, 58, 64));
          r.fillRect(x - 22.0f, by0 + 10.0f, 4.0f, 10.0f, rgb(150, 150, 160));
          r.fillRect(x - 22.0f, by0 + 50.0f, 4.0f, 10.0f, rgb(150, 150, 160));
        }
    }
  }
  // Cranes: the rail, the trolley, its bell, the cable and the magnet; a
  // ride crane's crate riding flush with the floor.
  const Texture& magnet = baked(mArt, r, "magnet", 136, 48, 4, 46, paintMagnet);
  const Texture& trolley = baked(mArt, r, "trolley", 136, 64, 4, 10, paintTrolley);
  const Texture& bell = baked(mArt, r, "bell", 28, 28, 14, 0, paintBell);
  for (const auto& c : s.cranes)
  {
    const float railY = sy(c.rail - 1, camY);
    const float x0 = sx(c.x0, camX), len = float(c.x1 - c.x0 + kCraneHead) * kCellPx;
    const float hookBottom = sy(c.hook + 1, camY);
    if (offScreen(x0 - 40.0f, railY - 60.0f, len + 80.0f, hookBottom - railY + 300.0f))
      continue;
    // Hangers up to the roof every few blocks.
    for (float hx = x0 + 40.0f; hx < x0 + len; hx += 4.0f * kTilePx)
      r.drawLine(hx, railY, hx, railY - 900.0f, 3.0f, rgb(20, 20, 26));
    r.draw(baked(mArt, r, "rail/" + std::to_string(int(len)), int(len), 30, 0, 0, [len](cairo_t* cr) { paintRail(cr, len); }), x0,
      railY);
    const float tx = sx(c.x, camX);
    const bool ringing = c.phase == CranePhase::Bell || c.phase == CranePhase::RideBell ||
      (c.ride && c.phase == CranePhase::RideHold && c.t >= 110);
    const bool busy = c.phase != CranePhase::Idle;
    // The cable: two falls from the drum to the magnet's shackle.
    const float cableTop = railY + 52.0f, cableBottom = hookBottom - 48.0f;
    if (cableBottom > cableTop)
    {
      const float mid = tx + 2.0f * kCellPx;
      r.drawLine(mid - 5.0f, cableTop, mid - 5.0f, cableBottom + 4.0f, 3.0f, rgb(40, 40, 44));
      r.drawLine(mid + 5.0f, cableTop, mid + 5.0f, cableBottom + 4.0f, 3.0f, rgb(40, 40, 44));
      r.drawLine(mid - 6.0f, cableTop, mid - 6.0f, cableBottom + 4.0f, 1.0f, rgb(150, 150, 160));
    }
    // The bell rings on the trolley: it swings and the beacon flashes.
    DrawOpts bo;
    bo.angle = ringing ? ((frame / 2) % 2 ? 18.0f : -18.0f) : 0.0f;
    r.draw(bell, tx + 116.0f, railY + 50.0f, bo);
    r.draw(trolley, tx, railY);
    const bool lamp = busy && (frame / 6) % 2 == 0;
    r.fillRect(tx + 8.0f, railY + 24.0f, 12.0f, 10.0f, lamp ? rgb(255, 170, 40) : rgb(110, 70, 20));
    if (lamp)
      drawGlow(r, mArt, tx + 14.0f, railY + 29.0f, 34.0f, rgb(255, 170, 50), 0.6f);
    if (ringing)
      for (int k = 0; k < 2; ++k)
      {
        const float rad = 14.0f + float((frame + k * 4) % 8) * 3.0f;
        r.drawLine(tx + 130.0f + rad * 0.7f, railY + 60.0f - rad * 0.7f, tx + 130.0f + rad, railY + 62.0f, 2.0f,
          rgba(255, 230, 150, 200 - k * 60));
      }
    if (c.ride && c.platform >= 0)
    {
      // The ride crate, its top flush with the floor, the magnet sitting
      // in its recess.
      const auto& pl = mPlatforms[std::size_t(c.platform)];
      const float x = (float(pl.prevX) + float(pl.x - pl.prevX) * alpha) * kCellPx - camX, y = float(pl.y) * kCellPx - camY;
      const float midC = x + 2.0f * kCellPx;
      r.drawLine(midC - 5.0f, cableTop, midC - 5.0f, y + 6.0f, 3.0f, rgb(40, 40, 44));
      r.drawLine(midC + 5.0f, cableTop, midC + 5.0f, y + 6.0f, 3.0f, rgb(40, 40, 44));
      r.draw(bakedSprite(mArt, r, "ridecrate/" + c.id, 128, 128, 0, 0, [&c](cairo_t* cr) {
        std::string label = c.id;
        for (auto& ch : label)
          ch = char(std::toupper(static_cast<unsigned char>(ch)));
        paintMetalCrate(cr, label, true);
      }).facing[0],
        x, y);
      r.fillRect(x + 24.0f, y, 80.0f, 6.0f, rgb(60, 62, 72));
      r.fillRect(x + 24.0f, y, 80.0f, 2.0f, rgb(150, 154, 166));
      if (pl.parked && c.phase == CranePhase::Idle)
      {
        // Waiting: a green go lamp on its side when its lever is thrown.
        const bool awake = c.lever < 0 || s.levers[std::size_t(c.lever)].throws > 0;
        r.fillRect(x + 54.0f, y + 26.0f, 20.0f, 8.0f, awake ? rgb(90, 255, 120) : rgb(120, 40, 40));
        if (awake)
          drawGlow(r, mArt, x + 64.0f, y + 30.0f, 24.0f, rgb(90, 255, 120), 0.4f + 0.2f * float((frame / 10) % 2));
      }
      continue;
    }
    // The magnet head.
    r.draw(magnet, tx, hookBottom);
    const bool energized = c.load != -1 && (c.phase == CranePhase::Lift || c.phase == CranePhase::Move || c.phase == CranePhase::Drop);
    if (energized || c.phase == CranePhase::Lower)
    {
      const float k = energized ? 0.7f : 0.3f;
      drawGlow(r, mArt, tx + 64.0f, hookBottom, 70.0f, kField, k * (0.7f + 0.3f * std::sin(float(frame) * 0.6f)));
      for (int q = 0; q < 3; ++q)
      {
        const float ax = tx + 20.0f + float(hash2(q, frame / 2) % 90u);
        r.drawLine(ax, hookBottom - 2.0f, ax + float(int(hash2(q + 5, frame / 2) % 13u) - 6), hookBottom + 8.0f, 2.0f,
          rgba(190, 220, 255, 200), Blend::Add);
      }
    }
  }
  // Metal crates: resting, on a magnet, sliding, falling, blinking above the
  // runner.
  for (const auto& c : s.crates)
  {
    if (c.state == CrateState::Gone)
      continue;
    const float x = sx(c.x, camX), y = sy(c.y, camY);
    if (offScreen(x, y, 128.0f, 128.0f))
      continue;
    if (c.blink > 0 && (frame / 3) % 2)
    {
      r.fillRect(x, y, 128.0f, 128.0f, rgba(255, 60, 40, 70));
      continue;
    }
    std::string label = c.id;
    r.draw(baked(mArt, r, "crate/" + c.id, 128, 128, 0, 0, [label](cairo_t* cr) { paintMetalCrate(cr, label, false); }), x, y);
    if (c.state == CrateState::Held)
      drawGlow(r, mArt, x + 64.0f, y + 4.0f, 60.0f, kField, 0.35f);
    else if (c.state == CrateState::Slide)
    {
      // Sparks off its foot and streaks behind it.
      const int d = c.slide > 0 ? 1 : -1;
      for (int k = 0; k < 4; ++k)
      {
        const float px = d > 0 ? x - 6.0f : x + 134.0f;
        r.drawLine(px, y + 30.0f + float(k) * 24.0f, px - float(d) * 40.0f, y + 30.0f + float(k) * 24.0f, 2.0f,
          rgba(255, 255, 255, 80));
      }
      for (int k = 0; k < 5; ++k)
      {
        const std::uint32_t h = hash2(k, frame);
        const float px = (d > 0 ? x + 10.0f : x + 118.0f) - float(d) * float(h % 30u);
        r.fillRect(px, y + 124.0f - float(h % 12u), 3.0f, 3.0f, rgb(255, 210, 120));
      }
    }
    else if (c.state == CrateState::Fall)
      for (int k = 0; k < 3; ++k)
        r.drawLine(x + 30.0f + float(k) * 34.0f, y - 10.0f, x + 30.0f + float(k) * 34.0f, y - 50.0f, 2.0f, rgba(255, 255, 255, 70));
  }
  // Levers: the switch box, the handle up or down, its crane's lamp.
  const Texture& leverBox = baked(mArt, r, "lever", 64, 112, 0, 112, paintLeverBox);
  for (std::size_t li = 0; li < s.levers.size(); ++li)
  {
    const auto& l = s.levers[li];
    const float x = sx(l.x, camX), foot = sy(l.y + 1, camY);
    if (offScreen(x, foot - 140.0f, 64.0f, 140.0f))
      continue;
    const Crane* cr = nullptr;
    for (const auto& c : s.cranes)
      if (c.lever == int(li))
        cr = &c;
    r.draw(leverBox, x, foot);
    const float px = x + 32.0f, py = foot - 74.0f;
    const float a = l.on ? -0.55f : 2.6f;
    const float hx = px + std::sin(a) * 36.0f, hy = py - std::cos(a) * 36.0f;
    r.drawLine(px, py, hx, hy, 9.0f, kInk);
    r.drawLine(px, py, hx, hy, 5.0f, rgb(190, 194, 206));
    r.fillRect(hx - 8.0f, hy - 8.0f, 16.0f, 16.0f, kInk);
    r.fillRect(hx - 6.0f, hy - 6.0f, 12.0f, 12.0f, rgb(230, 50, 40));
    r.fillRect(hx - 4.0f, hy - 5.0f, 4.0f, 3.0f, rgb(255, 160, 150));
    r.fillRect(px - 5.0f, py - 5.0f, 10.0f, 10.0f, rgb(60, 62, 72));
    if (l.flash > 0)
    {
      drawGlow(r, mArt, px, py, 40.0f, rgb(255, 240, 180), float(l.flash) / 10.0f);
      for (int k = 0; k < 4; ++k)
      {
        const std::uint32_t h = hash2(int(li) * 7 + k, l.flash);
        r.fillRect(px + float(int(h % 40u) - 20), py - float(h % 30u), 3.0f, 3.0f, rgb(255, 230, 140));
      }
    }
    // The lamp on top: amber while its crane works, green when it is ready.
    const bool busy = cr && cr->phase != CranePhase::Idle;
    const bool ready = cr && cr->ride && l.throws > 0;
    const Color lc = busy ? ((frame / 5) % 2 ? rgb(255, 170, 40) : rgb(120, 70, 20)) : (ready ? rgb(90, 255, 120) : rgb(230, 60, 50));
    r.fillRect(x + 26.0f, foot - 104.0f, 12.0f, 8.0f, lc);
    drawGlow(r, mArt, x + 32.0f, foot - 100.0f, 18.0f, lc, 0.5f);
    if (cr)
    {
      std::string label = cr->id;
      for (auto& ch : label)
        ch = char(std::toupper(static_cast<unsigned char>(ch)));
      r.drawText(label, x + 32.0f, foot - 92.0f, {13.0f, rgb(240, 240, 236), 0}, Align::Center);
    }
    // A hint over it when the runner is beside it and it has not been thrown.
    const auto& p = mPlayer;
    if (l.throws == 0 && std::abs(p.x + 1 - (l.x + 1)) <= 3 && std::abs(p.y - l.y) <= 2)
    {
      const float bob = std::sin(float(frame) * 0.15f) * 5.0f;
      r.drawText("UP", x + 32.0f, foot - 150.0f + bob, {22.0f, rgb(255, 230, 120), kHudInk}, Align::Center);
    }
  }
  // The freight lift cars: a diamond-plate deck, posts, a crossbar with the
  // sheave and the cable, a lamp and the bell that rings before it goes.
  if (!s.throwAll)
    for (const auto& pl : mPlatforms)
    {
      if (pl.crane || !pl.waitRider || pl.path.size() < 2)
        continue;
      const float x = (float(pl.prevX) + float(pl.x - pl.prevX) * alpha) * kCellPx - camX;
      const float y = (float(pl.prevY) + float(pl.y - pl.prevY) * alpha) * kCellPx - camY;
      const float wpx = float(pl.w) * kCellPx;
      if (offScreen(x, y - 220.0f, wpx, 260.0f))
        continue;
      r.drawLine(x + wpx * 0.5f, y - 196.0f, x + wpx * 0.5f, y - 2000.0f, 3.0f, rgb(30, 30, 36));
      r.fillRect(x + 6.0f, y - 190.0f, wpx - 12.0f, 190.0f, rgba(40, 44, 56, 120));
      for (float k = 0.0f; k < 190.0f; k += 16.0f)
        r.fillRect(x + 6.0f, y - 190.0f + k, wpx - 12.0f, 1.0f, rgba(120, 124, 140, 70));
      for (const float px : {x, x + wpx - 8.0f})
      {
        r.fillRect(px, y - 196.0f, 8.0f, 196.0f, rgb(100, 106, 120));
        r.fillRect(px, y - 196.0f, 2.0f, 196.0f, rgb(170, 176, 190));
      }
      r.fillRect(x - 4.0f, y - 204.0f, wpx + 8.0f, 12.0f, rgb(240, 196, 40));
      r.fillRect(x + wpx * 0.5f - 12.0f, y - 214.0f, 24.0f, 12.0f, rgb(70, 72, 84));
      r.fillRect(x, y - 2.0f, wpx, 18.0f, rgb(120, 126, 140));
      r.fillRect(x, y - 2.0f, wpx, 3.0f, rgb(200, 206, 220));
      for (float k = 6.0f; k < wpx - 6.0f; k += 12.0f)
        r.fillRect(x + k, y + 4.0f + float(int(k) % 24 == 6 ? 0 : 5), 6.0f, 2.0f, rgb(170, 176, 190));
      for (float k = 0.0f; k < wpx; k += 20.0f)
      {
        r.fillRect(x + k, y + 12.0f, 10.0f, 4.0f, rgb(240, 196, 40));
        r.fillRect(x + k + 10.0f, y + 12.0f, 10.0f, 4.0f, rgb(30, 28, 32));
      }
      const bool ring = pl.bell > 0;
      DrawOpts bo;
      bo.angle = ring ? ((frame / 2) % 2 ? 20.0f : -20.0f) : 0.0f;
      r.draw(bell, x + wpx - 20.0f, y - 192.0f, bo);
      const bool lit = ring ? (frame / 4) % 2 == 0 : true;
      r.fillRect(x + 12.0f, y - 188.0f, 14.0f, 8.0f, lit ? (ring ? rgb(255, 180, 50) : rgb(90, 255, 120)) : rgb(90, 60, 20));
      drawGlow(r, mArt, x + 19.0f, y - 184.0f, 22.0f, ring ? rgb(255, 180, 50) : rgb(90, 255, 120), lit ? 0.5f : 0.1f);
      r.drawText(pl.id, x + wpx * 0.5f, y - 186.0f, {13.0f, rgb(240, 240, 236), 0}, Align::Center);
    }
  // Throw Everything: the loose crates and the loose pieces in the walls.
  if (s.throwAll)
  {
    const Texture& lc = baked(mArt, r, "loose_crate", 64, 64, 0, 0, [](cairo_t* cr) {
      woodCrate(cr, 1, 1, 62, 62, kWood);
      line(cr, 6, 6, 58, 58);
      stroke(cr, kWoodLo, 5);
    });
    const Texture& lw = baked(mArt, r, "loose_wall", 64, 64, 0, 0, [](cairo_t* cr) {
      cairo_rectangle(cr, 1, 1, 62, 62);
      lgrad(cr, 0, 1, 0, 63, {{0.0, rgb(120, 114, 108)}, {1.0, rgb(70, 66, 64)}}, 1.0, true);
      stroke(cr, kInk, 2);
      line(cr, 1, 32, 63, 32);
      line(cr, 32, 1, 32, 32);
      line(cr, 16, 32, 16, 63);
      line(cr, 48, 32, 48, 63);
      stroke(cr, rgb(40, 38, 40), 2);
      // A chalk mark: this one comes out.
      circle(cr, 32, 32, 18);
      stroke(cr, rgb(240, 240, 230), 2.4, 0.8);
    });
    const int mw = mMap.width() / kCellsPerTile;
    for (const auto& [tx, ty] : s.loose)
    {
      if (mMap.block(tx, ty) != Tile::Solid)
        continue;
      const float x = float(tx) * kTilePx - camX, y = float(ty) * kTilePx - camY;
      if (offScreen(x, y, 64.0f, 64.0f))
        continue;
      r.draw(tx <= 1 || tx >= mw - 2 ? lw : lc, x, y);
    }
  }
}

// --- In front --------------------------------------------------------------------------------------------

void World::drawWarehouseFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)alpha;
  const auto& s = mWarehouse;
  const auto& p = mPlayer;
  // The scenery flats: carried up beside their Stagehand, or set down.
  for (std::size_t fi = 0; fi < s.flats.size(); ++fi)
  {
    const auto& f = s.flats[fi];
    if (!f.alive)
      continue;
    const CellBox b = f.box();
    float x = sx(b.x, camX), y = sy(b.y, camY);
    if (!f.down && f.enemy >= 0)
    {
      const Enemy& e = mEnemies[std::size_t(f.enemy)];
      x = (e.dir > 0 ? e.drawX + float(e.w) : e.drawX - float(kFlatW)) * kCellPx - camX;
      y = (e.drawY + 1.0f - float(kFlatH)) * kCellPx - camY - 14.0f;
    }
    if (offScreen(x, y, 128.0f, 192.0f))
      continue;
    const int look = int(fi % 3), damage = f.hp >= kFlatHp ? 0 : (f.hp > kFlatHp / 2 ? 1 : 2);
    const int dir = f.enemy >= 0 ? mEnemies[std::size_t(f.enemy)].dir : 1;
    const Sprite& sp = bakedSprite(mArt, r, "flat/" + std::to_string(look) + "/" + std::to_string(damage), 128, 192, 0, 0,
      [look, damage](cairo_t* cr) { paintFlat(cr, look, damage); });
    // The brace is on the Stagehand's side.
    r.draw(sp.get(dir > 0 ? -1 : 1), x, y);
  }
  // Forklift Bots, Stagehands, Prop Mimics.
  for (std::size_t i = 0; i < mEnemies.size(); ++i)
  {
    const Enemy& e = mEnemies[i];
    if (!e.alive || e.hidden)
      continue;
    if (e.kind != EnemyKind::Forklift && e.kind != EnemyKind::Stagehand && e.kind != EnemyKind::Mimic)
      continue;
    float cx = (e.drawX + float(e.w) * 0.5f) * kCellPx - camX;
    const float foot = (e.drawY + 1.0f) * kCellPx - camY;
    const float h = float(e.h) * kCellPx, wpx = float(e.w) * kCellPx;
    if (offScreen(cx - 260.0f, foot - h - 120.0f, 520.0f, h + 160.0f))
      continue;
    const int dir = e.dir < 0 ? -1 : 1;
    const EnemyDef& def = enemyDef(e.def);
    DrawOpts flash;
    if (e.flash > 0 && (e.flash / 2) % 2)
      flash.tint = rgb(255, 150, 150);
    if (e.kind == EnemyKind::Forklift)
    {
      const Forklift* f = nullptr;
      for (const auto& k : s.forklifts)
        if (k.enemy == int(i))
          f = &k;
      int v = 0;
      if (f && f->held >= 0)
        v = 4;
      else if (f && f->carrying)
        v = 3;
      else if (e.tell > 0)
        v = 1;
      else if (f && f->charge > 0)
        v = 2;
      if (v == 1)
        cx += (frame / 2) % 2 ? 2.0f : -2.0f; // revving
      // The forks and their carriage, moving on the mast.
      const float mastX = cx + float(dir) * (wpx * 0.5f - 24.0f);
      float forkY = foot - 26.0f; // a cell up, travelling
      if (v == 1 || v == 2)
        forkY = foot - 8.0f; // lowered to scoop
      else if (v == 3) // up under the runner's feet
        forkY = std::clamp(float(mPlayer.y + 1) * kCellPx - camY, foot - 72.0f, foot - 8.0f);
      else if (v == 4)
        forkY = foot - 40.0f;
      const float tipX = mastX + float(dir) * (24.0f + 3.0f * kCellPx);
      r.fillRect(std::min(mastX, mastX + float(dir) * 22.0f), forkY - 46.0f, 22.0f, 50.0f, kInk);
      r.fillRect(std::min(mastX, mastX + float(dir) * 22.0f) + 2.0f, forkY - 44.0f, 18.0f, 46.0f, rgb(70, 72, 84));
      for (const float dz : {0.0f, 7.0f})
      {
        const float fy = forkY - dz;
        r.drawLine(mastX, fy, tipX, fy + 2.0f, 9.0f, kInk);
        r.drawLine(mastX, fy, tipX, fy + 2.0f, 5.0f, dz > 0.0f ? rgb(120, 124, 136) : rgb(160, 166, 178));
      }
      if ((v == 1 || v == 2) && (frame / 2) % 2)
        for (int k = 0; k < 3; ++k)
        {
          const std::uint32_t hh = hash2(int(i) * 5 + k, frame);
          r.fillRect(tipX - float(dir) * float(hh % 40u), foot - 4.0f - float(hh % 8u), 3.0f, 3.0f, rgb(255, 210, 120));
        }
      const Sprite& sp = styledEnemySprite(mArt, r, mTheme, "forklift_bot", v, (int(e.drawX * 2.0f) % 2 + 2) % 2, e.w, e.h);
      r.draw(sp.get(dir), cx, foot, flash);
      // Its fleet number, stencilled on the cab (drawn here so it reads the
      // right way round both ways).
      const Texture& fleet = baked(mArt, r, "fleet_number", 60, 20, 30, 16,
        [](cairo_t* cr) { lettering(cr, "FB-36", 30, 16, 13, rgb(60, 40, 10), 0.75); });
      r.draw(fleet, cx + float(dir) * 4.0f, foot - 36.0f + (v == 1 ? 5.0f : (v == 4 ? -8.0f : 0.0f)), flash);
      if (v == 1)
      {
        // Revving: exhaust puffing from its stack, a tremble.
        for (int k = 0; k < 3; ++k)
        {
          const float u = std::fmod(float(frame) * 0.08f + float(k) / 3.0f, 1.0f);
          const float ex = cx - float(dir) * (wpx * 0.5f - 18.0f) - float(dir) * u * 30.0f;
          drawGlow(r, mArt, ex, foot - 92.0f - u * 50.0f, 10.0f + u * 18.0f, rgb(120, 120, 130), 0.5f * (1.0f - u));
        }
        if ((frame / 4) % 2 == 0)
          r.drawText("!", cx, foot - h - 34.0f, {30.0f, rgb(255, 210, 60), kHudInk}, Align::Center);
      }
      else if (v == 2)
        for (int k = 0; k < 3; ++k)
          r.drawLine(cx - float(dir) * (wpx * 0.5f + 10.0f), foot - 30.0f - float(k) * 30.0f,
            cx - float(dir) * (wpx * 0.5f + 60.0f), foot - 30.0f - float(k) * 30.0f, 3.0f, rgba(255, 255, 255, 90));
      if (v == 4 && f)
        r.fillRect(cx - 40.0f, foot - h - 6.0f, 80.0f, 8.0f, rgba(110, 170, 255, 90));
    }
    else if (e.kind == EnemyKind::Stagehand)
    {
      const Stagehand* sh = nullptr;
      for (const auto& k : s.stagehands)
        if (k.enemy == int(i))
          sh = &k;
      const Flat* fl = sh && sh->flat >= 0 && s.flats[std::size_t(sh->flat)].alive ? &s.flats[std::size_t(sh->flat)] : nullptr;
      int v = 1;
      if (e.tell > 0)
        v = 2;
      else if (sh && sh->lean > 0)
        v = 3;
      else if (fl && !fl->down)
        v = 0;
      const Sprite& sp = styledEnemySprite(mArt, r, mTheme, "stagehand", v, ((e.x / 2) % 2 + 2) % 2, e.w, e.h);
      r.draw(sp.get(dir), cx, foot, flash);
      if (v == 2)
      {
        // The raised glove flashing white as it leans out.
        const double a = 0.32, gx = 82.0 - 47.0, gy = 22.0 - 92.0;
        const float lx = float(47.0 + gx * std::cos(a) - gy * std::sin(a)), ly = float(92.0 + gx * std::sin(a) + gy * std::cos(a));
        const float px = cx + float(dir) * (lx - 48.0f) * wpx / 96.0f, py = foot - h + ly * h / 160.0f;
        const float k = 1.0f - float(e.tell) / float(std::max(1, def.tell));
        if ((frame / 2) % 2 == 0)
          drawGlow(r, mArt, px, py, 30.0f + 30.0f * k, rgb(255, 255, 255), 0.9f);
        drawGlow(r, mArt, px, py, 16.0f, rgb(255, 255, 255), 0.6f);
      }
    }
    else
    {
      const Mimic* m = nullptr;
      for (const auto& k : s.mimics)
        if (k.enemy == int(i))
          m = &k;
      int v = 0;
      if (m && m->lunge != 0)
        v = 3;
      else if (e.tell > 0)
        v = 2;
      else if (e.cool > def.cooldown - 6)
        v = 3;
      else if (m && m->breath < kMimicBreathe)
        v = 1;
      const Sprite& sp = styledEnemySprite(mArt, r, mTheme, "prop_mimic", v, e.carrier ? 1 : 0, e.w, e.h);
      float shake = 0.0f;
      if (v == 2)
        shake = (frame / 2) % 2 ? 2.0f : -2.0f;
      r.draw(sp.get(dir), cx + shake, foot, flash);
      r.draw(baked(mArt, r, "mimic_label", 128, 60, 64, 22, paintMimicLabel), cx + shake, foot - 44.0f, flash);
      if (v == 2 && e.carrier)
        drawGlow(r, mArt, cx, foot - h * 0.7f, 60.0f, rgb(120, 255, 60), 0.4f);
      if (v == 2 && (frame / 4) % 2 == 0)
        r.drawText("!", cx, foot - h - 50.0f, {30.0f, rgb(255, 210, 60), kHudInk}, Align::Center);
    }
  }
  // The Magnet Gun: the pull beam's rings of field rolling in toward the
  // runner, a glow where it has hold of something; the push's muzzle burst.
  const float gunY = float(p.stance == Stance::Crouched ? p.y - 1 : p.y - 2) * kCellPx + 16.0f - camY;
  const float gunX = float(p.facing > 0 ? p.x + Player::kWidth : p.x) * kCellPx - camX;
  if (s.pulling && s.beam > 0)
  {
    const float len = float(s.beam) * kCellPx;
    const float x1 = gunX + float(p.facing) * len;
    r.drawLine(gunX, gunY, x1, gunY, 14.0f, rgba(110, 170, 255, 50), Blend::Add);
    r.drawLine(gunX, gunY, x1, gunY, 4.0f, rgba(200, 230, 255, 140), Blend::Add);
    for (float d = std::fmod(float(frame) * 6.0f, 24.0f); d < len; d += 24.0f)
    {
      const float x = x1 - float(p.facing) * d;
      const float k = 1.0f - d / std::max(1.0f, len);
      const Color c = (int(d / 24.0f) + frame / 4) % 2 ? kFieldRed : kField;
      const float hgt = 10.0f + 12.0f * k;
      r.drawLine(x, gunY - hgt, x - float(p.facing) * 6.0f, gunY, 3.0f, withAlpha(c, 200), Blend::Add);
      r.drawLine(x - float(p.facing) * 6.0f, gunY, x, gunY + hgt, 3.0f, withAlpha(c, 200), Blend::Add);
    }
    if (s.pullCrate >= 0 || s.pullEnemy >= 0)
    {
      drawGlow(r, mArt, x1, gunY, 50.0f, kField, 0.6f);
      for (int q = 0; q < 4; ++q)
      {
        const std::uint32_t hh = hash2(q, frame);
        r.drawLine(x1, gunY, x1 + float(int(hh % 40u) - 20), gunY + float(int((hh >> 8) % 40u) - 20), 2.0f,
          rgba(220, 240, 255, 220), Blend::Add);
      }
    }
    drawGlow(r, mArt, gunX, gunY, 26.0f, kFieldRed, 0.6f);
  }
  if (s.pulse > 0)
  {
    const float k = float(s.pulse) / 6.0f;
    drawGlow(r, mArt, gunX + float(p.facing) * 10.0f, gunY, 30.0f + (1.0f - k) * 30.0f, kField, 0.8f * k);
    for (int q = 0; q < 3; ++q)
    {
      const float rad = 10.0f + (1.0f - k) * 40.0f + float(q) * 8.0f;
      r.drawLine(gunX + float(p.facing) * rad, gunY - rad * 0.8f, gunX + float(p.facing) * (rad + 6.0f), gunY, 3.0f,
        rgba(200, 220, 255, int(200.0f * k)), Blend::Add);
      r.drawLine(gunX + float(p.facing) * (rad + 6.0f), gunY, gunX + float(p.facing) * rad, gunY + rad * 0.8f, 3.0f,
        rgba(200, 220, 255, int(200.0f * k)), Blend::Add);
    }
  }
  // On a Forklift Bot's forks: jump!
  if (s.grabbedBy >= 0 && (frame / 6) % 2 == 0)
    r.drawText("JUMP!", float(p.x + 1) * kCellPx - camX, float(p.y - p.height() - 2) * kCellPx - camY, {26.0f, rgb(255, 230, 90), kHudInk},
      Align::Center);
  // Throw Everything: what the runner holds overhead, what is in the air.
  const Texture& crateTex = baked(mArt, r, "loose_crate", 64, 64, 0, 0, [](cairo_t* cr) {
    woodCrate(cr, 1, 1, 62, 62, kWood);
    line(cr, 6, 6, 58, 58);
    stroke(cr, kWoodLo, 5);
  });
  if (s.carry >= 0)
  {
    const float lift = s.liftT > 0 ? float(s.liftT) * 4.0f : 0.0f;
    const float x = float(p.x) * kCellPx - camX + 16.0f, y = float(p.y - p.height() - 1) * kCellPx - camY + lift;
    if (s.carry == 1)
    {
      DrawOpts o;
      o.angle = 90.0f;
      o.scale = 0.7f;
      r.draw(styledEnemySprite(mArt, r, mTheme, "practice_dummy", 1, 0, 3, 5).get(1), x - 40.0f, y + 26.0f, o);
    }
    else
      r.draw(crateTex, x - 16.0f, y - 32.0f);
  }
  for (const auto& t : s.thrown)
  {
    const float x = t.x * kCellPx - camX, y = t.y * kCellPx - camY;
    DrawOpts o;
    o.angle = float(t.t) * 40.0f;
    if (t.kind == 1)
    {
      o.scale = 0.7f;
      r.draw(styledEnemySprite(mArt, r, mTheme, "practice_dummy", 1, 0, 3, 5).get(1), x + 32.0f, y + 64.0f, o);
    }
    else
    {
      const Texture& lc = bakedSprite(mArt, r, "loose_crate_c", 64, 64, 32, 32, [](cairo_t* cr) {
        woodCrate(cr, 1, 1, 62, 62, kWood);
        line(cr, 6, 6, 58, 58);
        stroke(cr, kWoodLo, 5);
      }).facing[0];
      r.draw(lc, x + 32.0f, y + 32.0f, o);
    }
    r.drawLine(x + 32.0f, y + 32.0f, x + 32.0f - float(t.toX - t.fromX) * 2.0f, y + 40.0f, 3.0f, rgba(255, 255, 255, 50));
  }
}

// --- HUD -------------------------------------------------------------------------------------------------

void World::drawWarehouseHud(Renderer& r, int frame) const
{
  (void)frame;
  const auto& s = mWarehouse;
  for (const auto& d : s.props)
    if (d.label > 0)
    {
      // The prop's tag, hand-written on a luggage label, fading out.
      const float a = std::min(1.0f, float(d.label) / 10.0f);
      const char* text = propLabel(d.level);
      const float tw = 24.0f + float(std::string(text).size()) * 14.0f;
      const float x = float(kScreenW) * 0.5f - tw * 0.5f, y = 150.0f;
      r.fillRect(x, y, tw, 46.0f, rgba(240, 228, 196, int(235.0f * a)));
      r.fillRect(x, y, tw, 4.0f, rgba(200, 180, 140, int(235.0f * a)));
      r.fillRect(x + 10.0f, y + 17.0f, 10.0f, 10.0f, rgba(60, 50, 40, int(200.0f * a)));
      r.drawLine(x + 15.0f, y + 22.0f, x - 30.0f, y - 30.0f, 2.0f, rgba(230, 220, 200, int(200.0f * a)));
      r.drawText(text, x + tw * 0.5f + 8.0f, y + 8.0f, {24.0f, rgb(50, 36, 30), 0, true}, Align::Center, a);
      break;
    }
}

// --- Shots and the icon ----------------------------------------------------------------------------------

bool drawWarehouseShot(Renderer& r, const World& w, const Art& art, const Projectile& pr, float cx, float cy, int frame)
{
  const auto& s = w.warehouse();
  if (!s.on)
    return false;
  const float d = float(pr.dx < 0 ? -1 : 1);
  if (pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::MagnetGun))
  {
    // The push pulse: rings of field rolling out ahead.
    drawGlow(r, art, cx, cy, 40.0f, kField, 0.6f);
    for (int k = 0; k < 3; ++k)
    {
      const float off = float((frame + k * 3) % 9) * 3.0f + float(k) * 8.0f;
      const float rad = 18.0f - float(k) * 3.0f;
      const Color c = k % 2 ? kFieldRed : kField;
      r.drawLine(cx - d * off, cy - rad, cx - d * off + d * 8.0f, cy, 4.0f, withAlpha(c, 220 - k * 50), Blend::Add);
      r.drawLine(cx - d * off + d * 8.0f, cy, cx - d * off, cy + rad, 4.0f, withAlpha(c, 220 - k * 50), Blend::Add);
    }
    return true;
  }
  if (pr.kind == ShotKind::Enemy && !s.throwAll)
  {
    // A Stagehand's roll of gaffer tape, spinning.
    DrawOpts o;
    o.angle = float(frame) * 24.0f * d;
    r.draw(bakedSprite(art, r, "tape", 30, 30, 15, 15, paintTape).facing[0], cx, cy, o);
    return true;
  }
  return false;
}

void drawMagnetIcon(Renderer& r, const Art& art, float x, float y, float scale, int frame)
{
  DrawOpts o;
  o.scale = scale;
  r.draw(baked(art, r, "magnet_icon", 64, 64, 0, 0, paintMagnetIcon), x, y, o);
  // A spark jumping between the poles.
  if ((frame / 3) % 4 != 0)
  {
    const float px = x + 60.0f * scale, a = y + 21.0f * scale, b = y + 39.0f * scale;
    r.drawLine(px, a, px + 4.0f * scale, (a + b) * 0.5f, 2.0f * scale, rgba(170, 220, 255, 230), Blend::Add);
    r.drawLine(px + 4.0f * scale, (a + b) * 0.5f, px, b, 2.0f * scale, rgba(170, 220, 255, 230), Blend::Add);
    drawGlow(r, art, px + 2.0f * scale, (a + b) * 0.5f, 10.0f * scale, kField, 0.7f);
  }
}

} // namespace gr
