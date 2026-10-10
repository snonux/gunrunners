// Level 21's drawing (world_zero.cpp has the rules): the server racks'
// blinking lights, the bulkheads sliding in and out of the ceiling and
// floor, the monitors and their schematics, laser grids, the kill switch,
// Echo pads, terminals, RACK 42 and the fake wall, the rack door and the
// floor grating, the wrecks a Repair Swarm is rebuilding, the Lattice
// Turrets' rails, the Echoes themselves (holograms of a runner), the Phase
// Rifle's shots, ZERO (the red eye in its ring of racks) with the arena's
// drop-away floor over the live grille, the reveal (the wall falls on a TV
// studio, its lights, the audience and Lance Marquee), the Wireframe
// bonus's debug view, and ZERO's bar. Pieces are baked with Cairo the first
// time they are drawn and kept in the Art's sprite cache.

#include "game/world.hpp"

#include "assets/art.hpp"
#include "assets/enemy_art.hpp"
#include "assets/enemy_art_zero.hpp"
#include "base/math.hpp"
#include "data/characters.hpp"
#include "data/weapons.hpp"
#include "game/zero_draw.hpp"
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

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr float kCellPx = float(kCellSize) * kPixelScale; // 32
constexpr float kTilePx = kCellPx * float(kCellsPerTile);  // 64
constexpr Color kInk = rgb(14, 12, 20);
constexpr Color kRed = rgb(255, 46, 56);
constexpr Color kRedHot = rgb(255, 170, 160);
constexpr Color kGreen = rgb(90, 255, 130);
constexpr Color kCyan = rgb(90, 240, 255);
constexpr Color kViolet = rgb(200, 120, 255);
constexpr Color kSteel = rgb(150, 156, 172);
constexpr Color kSteelLight = rgb(214, 220, 232);
constexpr Color kSteelDark = rgb(48, 50, 62);
constexpr Color kMagenta = rgb(255, 70, 220);
constexpr Color kWire = rgb(80, 255, 140);

Color lighten(Color c, float t) { return lerpColor(c, rgb(255, 255, 255), t); }
Color darken(Color c, float t) { return lerpColor(c, rgb(0, 0, 0), t); }

bool visible(float x, float y, float w, float h)
{
  return x + w > -96.0f && x < float(kScreenW) + 96.0f && y + h > -96.0f && y < float(kScreenH) + 96.0f;
}

// A texture baked once and kept in the Art's sprite cache under `key`.
const Texture& baked(const Art& art, const Renderer& r, const std::string& key, int w, int h, float ax, float ay,
  const std::function<void(cairo_t*)>& paint)
{
  const std::string id = "~zero/" + key;
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
  cairo_pattern_add_color_stop_rgb(p, 1, 0.3, 0.32, 0.4);
  cairo_set_source(cr, p);
  cairo_fill(cr);
  cairo_pattern_destroy(p);
}

// Red and black warning stripes filling a rect.
void stripes(cairo_t* cr, double x, double y, double w, double h, double step)
{
  cairo_save(cr);
  cairo_rectangle(cr, x, y, w, h);
  cairo_clip(cr);
  setColor(cr, rgb(200, 30, 40));
  cairo_paint(cr);
  for (double sx = x - h - step; sx < x + w + h; sx += step)
  {
    cairo_move_to(cr, sx, y + h);
    cairo_line_to(cr, sx + step * 0.5, y + h);
    cairo_line_to(cr, sx + step * 0.5 + h, y);
    cairo_line_to(cr, sx + h, y);
    cairo_close_path(cr);
  }
  setColor(cr, rgb(20, 18, 24));
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

float ease(float t)
{
  t = std::clamp(t, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

float frac(std::uint32_t h) { return float(h % 1000u) / 1000.0f; }

// A jagged bolt of electricity from (x0, y0) to (x1, y1): a wide coloured
// stroke under a thin white one.
void zap(Renderer& r, float x0, float y0, float x1, float y1, int seed, Color c, float width, int steps = 8)
{
  const float dx = x1 - x0, dy = y1 - y0, len = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
  const float nx = -dy / len, ny = dx / len;
  float px = x0, py = y0;
  for (int i = 1; i <= steps; ++i)
  {
    const float t = float(i) / float(steps);
    const float off = i == steps ? 0.0f : (frac(hash2(seed, i)) * 2.0f - 1.0f) * len * 0.08f;
    const float qx = x0 + dx * t + nx * off, qy = y0 + dy * t + ny * off;
    r.drawLine(px, py, qx, qy, width, withAlpha(c, 200), Blend::Add);
    r.drawLine(px, py, qx, qy, std::max(1.0f, width * 0.35f), rgba(255, 255, 255, 230), Blend::Add);
    px = qx;
    py = qy;
  }
}

// A sparkle: four thin rays and a hot middle.
void sparkle(Renderer& r, const Art& art, float x, float y, float size, Color c, float a)
{
  drawGlow(r, art, x, y, size * 1.4f, c, 0.6f * a);
  r.drawLine(x - size, y, x + size, y, 2.0f, withAlpha(rgb(255, 255, 255), int(230 * a)), Blend::Add);
  r.drawLine(x, y - size, x, y + size, 2.0f, withAlpha(rgb(255, 255, 255), int(230 * a)), Blend::Add);
  r.drawLine(x - size * 0.4f, y - size * 0.4f, x + size * 0.4f, y + size * 0.4f, 1.5f, withAlpha(c, int(200 * a)), Blend::Add);
  r.drawLine(x - size * 0.4f, y + size * 0.4f, x + size * 0.4f, y - size * 0.4f, 1.5f, withAlpha(c, int(200 * a)), Blend::Add);
}

// A rounded speech-bubble panel with lines of text, its bottom middle at
// (cx, bottom).
void bubble(Renderer& r, float cx, float bottom, const std::vector<std::string>& lines, Color border, Color text, float a)
{
  if (a <= 0.01f || lines.empty())
    return;
  std::size_t longest = 0;
  for (const auto& l : lines)
    longest = std::max(longest, l.size());
  const float w = std::max(120.0f, float(longest) * 10.4f + 30.0f), lh = 22.0f;
  const float h = float(lines.size()) * lh + 18.0f;
  const float x = std::clamp(cx - w * 0.5f, 8.0f, float(kScreenW) - w - 8.0f), y = bottom - h - 12.0f;
  r.fillRect(x, y, w, h, rgba(6, 12, 10, int(220 * a)));
  r.fillRect(x, y, w, 2.0f, withAlpha(border, int(230 * a)));
  r.fillRect(x, y + h - 2.0f, w, 2.0f, withAlpha(border, int(230 * a)));
  r.fillRect(x, y, 2.0f, h, withAlpha(border, int(230 * a)));
  r.fillRect(x + w - 2.0f, y, 2.0f, h, withAlpha(border, int(230 * a)));
  for (int k = 0; k < 4; ++k)
    r.fillRect(cx - 8.0f + float(k) * 2.0f, y + h + float(k) * 3.0f, 16.0f - float(k) * 4.0f, 3.0f,
      withAlpha(border, int(200 * a)));
  for (std::size_t i = 0; i < lines.size(); ++i)
    r.drawText(lines[i], x + w * 0.5f, y + 9.0f + float(i) * lh, {17.0f, text, rgb(0, 0, 0)}, Align::Center, a);
}

void clipTo(Renderer& r, float x, float y, float w, float h)
{
  const SDL_Rect rc{int(std::floor(x)), int(std::floor(y)), std::max(0, int(std::ceil(w))), std::max(0, int(std::ceil(h)))};
  SDL_RenderSetClipRect(r.sdl(), &rc);
}

void unclip(Renderer& r) { SDL_RenderSetClipRect(r.sdl(), nullptr); }

// A texture stretched into any rect (the flat tipping back is squashed).
void stretch(Renderer& r, const Texture& t, float x, float y, float w, float h, Color tint, float alpha)
{
  if (!t || w <= 0.5f || h <= 0.5f)
    return;
  const SDL_FRect dst{x, y, w, h};
  SDL_SetTextureBlendMode(t.get(), SDL_BLENDMODE_BLEND);
  SDL_SetTextureAlphaMod(t.get(), Uint8(std::lround(std::clamp(alpha, 0.0f, 1.0f) * 255.0f)));
  SDL_SetTextureColorMod(t.get(), Uint8(redOf(tint)), Uint8(greenOf(tint)), Uint8(blueOf(tint)));
  SDL_RenderCopyF(r.sdl(), t.get(), nullptr, &dst);
}

// --- Painters ----------------------------------------------------------------------------

// A bulkhead: a server rack slab with warning stripes along its leading
// edge (the bottom of one that comes down from the ceiling).
void paintBulkhead(cairo_t* cr, double w, double h, unsigned seed, bool fromCeiling)
{
  paintServerRack(cr, w, h, seed, true);
  const double sy = fromCeiling ? h - 16 : 2;
  stripes(cr, 3, sy, w - 6, 14, 18);
  cairo_rectangle(cr, 3, sy, w - 6, 14);
  outline(cr, 2.0);
  // Hydraulic rams' heads.
  for (const double x : {w * 0.25, w * 0.75})
  {
    cairo_rectangle(cr, x - 6, fromCeiling ? 0 : h - 10, 12, 10);
    linearFill(cr, x - 6, 0, x + 6, 0, kSteelLight, kSteelDark, true);
    outline(cr, 1.4);
  }
}

// One arena floor segment, w px wide, 64 tall: a heavy steel plate with a
// tread, a light channel along its face (lit in-engine) and its number.
void paintSegment(cairo_t* cr, double w, int number)
{
  roundedRect(cr, 2, 2, w - 4, 60, 4);
  linearFill(cr, 0, 2, 0, 62, rgb(118, 122, 138), rgb(30, 32, 40), true);
  outline(cr, 2.4);
  cairo_rectangle(cr, 4, 4, w - 8, 10);
  linearFill(cr, 0, 4, 0, 14, rgb(210, 214, 226), rgb(110, 114, 130));
  for (double x = 8; x < w - 8; x += 10)
  {
    cairo_move_to(cr, x, 12);
    cairo_line_to(cr, x + 5, 6);
  }
  setRgba(cr, rgb(40, 40, 50), 0.6);
  cairo_set_line_width(cr, 1.3);
  cairo_stroke(cr);
  // The light channel.
  roundedRect(cr, 12, 24, w - 24, 8, 3);
  setColor(cr, rgb(10, 10, 14));
  cairo_fill(cr);
  for (const double x : {10.0, w - 10})
    for (const double y : {44.0, 54.0})
      bolt(cr, x, y, 2.6);
  // Its number, stencilled.
  setColor(cr, rgb(200, 190, 120));
  centredText(cr, ("SEG " + std::to_string(number)).c_str(), w * 0.5, 52, 13);
  // Undersides: girders.
  cairo_rectangle(cr, 2, 58, w - 4, 4);
  setColor(cr, rgb(12, 12, 16));
  cairo_fill(cr);
}

// A CRT monitor 128 x 104 with the screen at (14, 12, 100, 72) cut out.
void paintMonitor(cairo_t* cr)
{
  cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
  roundedRect(cr, 2, 2, 124, 92, 12);
  roundedRect(cr, 14, 12, 100, 72, 8);
  linearFill(cr, 0, 2, 0, 94, rgb(70, 72, 84), rgb(18, 18, 24), true);
  outline(cr, 2.4);
  cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
  roundedRect(cr, 14, 12, 100, 72, 8);
  outline(cr, 3.0, rgb(6, 6, 8));
  // Glass sheen.
  cairo_move_to(cr, 20, 16);
  cairo_line_to(cr, 60, 16);
  cairo_line_to(cr, 24, 46);
  cairo_close_path(cr);
  setRgba(cr, rgb(255, 255, 255), 0.07);
  cairo_fill(cr);
  // Knobs and the stand.
  for (const double x : {104.0, 116.0})
  {
    cairo_arc(cr, x, 90, 3, 0, 2 * kPi);
    setColor(cr, rgb(120, 124, 136));
    cairo_fill(cr);
  }
  cairo_rectangle(cr, 50, 94, 28, 8);
  linearFill(cr, 0, 94, 0, 102, rgb(60, 62, 74), rgb(20, 20, 26), true);
  outline(cr, 1.4);
}

// The kill switch, 64 x 128: a red panel hanging on a cable, a big lever
// in its slot, KILL stencilled over it. Off (shot) it is green, the lever
// down.
void paintSwitch(cairo_t* cr, bool hit)
{
  const Color body = hit ? rgb(40, 150, 70) : rgb(190, 30, 40);
  cairo_rectangle(cr, 30, 0, 4, 22);
  setColor(cr, rgb(20, 20, 26));
  cairo_fill(cr);
  roundedRect(cr, 6, 20, 52, 100, 6);
  linearFill(cr, 0, 20, 0, 120, lighten(body, 0.25f), darken(body, 0.45f), true);
  outline(cr, 2.4);
  stripes(cr, 10, 24, 44, 8, 10);
  roundedRect(cr, 24, 46, 16, 52, 4);
  setColor(cr, rgb(16, 10, 12));
  cairo_fill(cr);
  // The lever.
  const double ly = hit ? 88 : 54;
  roundedRect(cr, 18, ly - 6, 28, 12, 5);
  linearFill(cr, 0, ly - 6, 0, ly + 6, kSteelLight, kSteelDark, true);
  outline(cr, 1.6);
  setColor(cr, rgb(255, 240, 230));
  centredText(cr, hit ? "OFF" : "KILL", 32, 114, 11);
  // The target ring.
  cairo_arc(cr, 32, 72, 22, 0, 2 * kPi);
  setRgba(cr, rgb(255, 255, 255), 0.35);
  cairo_set_line_width(cr, 2);
  cairo_stroke(cr);
}

// An Echo pad, 128 x 32: a ring of emitters set in the floor.
void paintPad(cairo_t* cr, bool armed)
{
  cairo_save(cr);
  cairo_translate(cr, 64, 16);
  cairo_scale(cr, 60, 13);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_restore(cr);
  linearFill(cr, 0, 3, 0, 29, rgb(100, 104, 120), rgb(24, 24, 30), true);
  outline(cr, 2.0);
  cairo_save(cr);
  cairo_translate(cr, 64, 15);
  cairo_scale(cr, 46, 8.5);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_restore(cr);
  setColor(cr, armed ? rgb(20, 90, 110) : rgb(26, 28, 34));
  cairo_fill(cr);
  for (int k = 0; k < 10; ++k)
  {
    const double a = k * 2 * kPi / 10;
    cairo_arc(cr, 64 + std::cos(a) * 53, 16 + std::sin(a) * 11, 2.2, 0, 2 * kPi);
    setColor(cr, armed ? rgb(150, 250, 255) : rgb(70, 74, 86));
    cairo_fill(cr);
  }
}

// A terminal, 72 x 96 on (36, 96): a slim console with a screen and a
// keyboard.
void paintTerminal(cairo_t* cr)
{
  cairo_move_to(cr, 22, 96);
  cairo_line_to(cr, 26, 50);
  cairo_line_to(cr, 46, 50);
  cairo_line_to(cr, 50, 96);
  cairo_close_path(cr);
  linearFill(cr, 22, 0, 50, 0, rgb(70, 72, 86), rgb(20, 20, 26), true);
  outline(cr, 2.0);
  cairo_move_to(cr, 8, 58);
  cairo_line_to(cr, 64, 58);
  cairo_line_to(cr, 60, 48);
  cairo_line_to(cr, 12, 48);
  cairo_close_path(cr);
  linearFill(cr, 0, 48, 0, 58, rgb(90, 92, 106), rgb(40, 40, 50), true);
  outline(cr, 1.6);
  for (int i = 0; i < 6; ++i)
  {
    cairo_rectangle(cr, 15 + i * 7.5, 51, 5, 2.5);
    setColor(cr, rgb(20, 20, 26));
    cairo_fill(cr);
  }
  roundedRect(cr, 6, 4, 60, 42, 5);
  linearFill(cr, 0, 4, 0, 46, rgb(76, 78, 92), rgb(24, 24, 30), true);
  outline(cr, 2.0);
  roundedRect(cr, 11, 9, 50, 32, 3);
  setColor(cr, rgb(4, 12, 6));
  cairo_fill(cr);
}

// DOOR1: a rack-faced door to the ceiling with its handle, stencil and a
// hazard band at the foot. `dmg` 0..3 adds dents and cracks.
void paintRackDoor(cairo_t* cr, double w, double h, int dmg)
{
  paintServerRack(cr, w, h, 77u, true, "DOOR 1");
  stripes(cr, 4, h - 34, w - 8, 18, 16);
  cairo_rectangle(cr, 4, h - 34, w - 8, 18);
  outline(cr, 1.6);
  roundedRect(cr, w - 26, h * 0.5 - 30, 12, 60, 5);
  linearFill(cr, w - 26, 0, w - 14, 0, kSteelLight, kSteelDark, true);
  outline(cr, 1.6);
  for (int k = 0; k < dmg; ++k)
  {
    const double cx = w * (0.3 + 0.2 * k), cy = h * (0.35 + 0.15 * k);
    for (int j = 0; j < 4; ++j)
    {
      const double a = j * 1.7 + k;
      cairo_move_to(cr, cx, cy);
      cairo_line_to(cr, cx + std::cos(a) * 22, cy + std::sin(a) * 22);
    }
    setColor(cr, rgb(230, 230, 240));
    cairo_set_line_width(cr, 1.6);
    cairo_stroke(cr);
    cairo_arc(cr, cx, cy, 5, 0, 2 * kPi);
    setColor(cr, rgb(10, 10, 12));
    cairo_fill(cr);
  }
}

// A floor grating, w x 64: slats across a dark pocket, a frame round it.
void paintGrate(cairo_t* cr, double w)
{
  cairo_rectangle(cr, 0, 0, w, 64);
  setColor(cr, rgb(6, 6, 8));
  cairo_fill(cr);
  for (double x = 6; x < w - 4; x += 12)
  {
    cairo_rectangle(cr, x, 4, 6, 56);
    linearFill(cr, x, 0, x + 6, 0, rgb(150, 154, 170), rgb(50, 52, 62), true);
    outline(cr, 1.0);
  }
  cairo_rectangle(cr, 0, 0, w, 64);
  setColor(cr, rgb(40, 42, 52));
  cairo_set_line_width(cr, 6);
  cairo_stroke(cr);
  cairo_rectangle(cr, 0, 0, w, 6);
  linearFill(cr, 0, 0, 0, 6, rgb(200, 204, 216), rgb(90, 94, 110));
  for (const double x : {6.0, w - 6})
    bolt(cr, x, 32, 2.6);
}

// A ceiling rail, w px long and 24 tall: an I-beam with a channel the
// trolley's wheels ride in, bracket stubs on top.
void paintRail(cairo_t* cr, double w)
{
  for (double x = 20; x < w; x += 128)
  {
    cairo_rectangle(cr, x - 4, 0, 8, 8);
    setColor(cr, rgb(30, 32, 40));
    cairo_fill(cr);
  }
  cairo_rectangle(cr, 0, 6, w, 16);
  linearFill(cr, 0, 6, 0, 22, rgb(150, 156, 170), rgb(40, 42, 52), true);
  outline(cr, 2.0);
  cairo_rectangle(cr, 4, 12, w - 8, 4);
  setColor(cr, rgb(12, 12, 16));
  cairo_fill(cr);
  for (const double x : {3.0, w - 9})
  {
    cairo_rectangle(cr, x, 4, 6, 20);
    setColor(cr, rgb(200, 40, 50));
    cairo_fill(cr);
  }
}

// The zoom ring alone (an annulus with its knurling and focal lengths), to
// turn over the baked eye.
void paintZoomRing(cairo_t* cr, double c, double rad)
{
  const double r0 = rad * 0.8, r1 = rad * 0.88;
  cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
  cairo_arc(cr, c, c, r1, 0, 2 * kPi);
  cairo_new_sub_path(cr);
  cairo_arc(cr, c, c, r0 - rad * 0.12, 0, 2 * kPi);
  setColor(cr, rgb(18, 18, 22));
  cairo_fill(cr);
  cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
  for (int k = 0; k < 120; ++k)
  {
    const double a = k * 2 * kPi / 120;
    cairo_move_to(cr, c + std::cos(a) * r0, c + std::sin(a) * r0);
    cairo_line_to(cr, c + std::cos(a) * r1, c + std::sin(a) * r1);
  }
  setColor(cr, rgb(70, 72, 84));
  cairo_set_line_width(cr, 1.6);
  cairo_stroke(cr);
  static const char* kMarks[] = {"24", "35", "50", "85", "135", "200", "300"};
  selectGameFont(cr);
  cairo_set_font_size(cr, rad * 0.055);
  for (int k = 0; k < 7; ++k)
  {
    const double a = -kPi * 0.5 + (k - 3) * 0.2;
    cairo_save(cr);
    cairo_translate(cr, c + std::cos(a) * rad * 0.74, c + std::sin(a) * rad * 0.74);
    cairo_rotate(cr, a + kPi * 0.5);
    cairo_text_extents_t e;
    cairo_text_extents(cr, kMarks[k], &e);
    cairo_move_to(cr, -e.width * 0.5, e.height * 0.5);
    setColor(cr, k == 3 ? rgb(255, 210, 60) : rgb(220, 222, 230));
    cairo_show_text(cr, kMarks[k]);
    cairo_restore(cr);
  }
}

// A bank of studio lights, 360 x 90: a truss and four fresnel lamps
// pointing down.
void paintLightBank(cairo_t* cr, bool lit)
{
  cairo_rectangle(cr, 0, 6, 360, 18);
  linearFill(cr, 0, 6, 0, 24, rgb(70, 72, 84), rgb(20, 20, 26), true);
  outline(cr, 2.0);
  for (double x = 0; x < 360; x += 20)
  {
    cairo_move_to(cr, x, 24);
    cairo_line_to(cr, x + 10, 6);
    cairo_line_to(cr, x + 20, 24);
  }
  setColor(cr, rgb(110, 114, 128));
  cairo_set_line_width(cr, 1.6);
  cairo_stroke(cr);
  for (int k = 0; k < 4; ++k)
  {
    const double cx = 45 + k * 90;
    cairo_rectangle(cr, cx - 3, 24, 6, 12);
    setColor(cr, rgb(30, 30, 36));
    cairo_fill(cr);
    roundedRect(cr, cx - 26, 34, 52, 40, 8);
    linearFill(cr, cx - 26, 0, cx + 26, 0, rgb(80, 82, 96), rgb(16, 16, 22), true);
    outline(cr, 2.0);
    // Barn doors.
    for (const int side : {-1, 1})
    {
      cairo_move_to(cr, cx + side * 26, 70);
      cairo_line_to(cr, cx + side * 36, 86);
      setColor(cr, rgb(40, 40, 48));
      cairo_set_line_width(cr, 4);
      cairo_stroke(cr);
    }
    cairo_save(cr);
    cairo_translate(cr, cx, 74);
    cairo_scale(cr, 22, 6);
    cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
    cairo_restore(cr);
    if (lit)
      linearFill(cr, 0, 68, 0, 80, rgb(255, 255, 240), rgb(255, 220, 140));
    else
    {
      setColor(cr, rgb(40, 36, 30));
      cairo_fill(cr);
    }
  }
}

// A cone of light, 320 x 640, its top middle at (160, 0), for additive
// drawing.
void paintCone(cairo_t* cr)
{
  cairo_move_to(cr, 140, 0);
  cairo_line_to(cr, 180, 0);
  cairo_line_to(cr, 320, 640);
  cairo_line_to(cr, 0, 640);
  cairo_close_path(cr);
  cairo_pattern_t* g = cairo_pattern_create_linear(0, 0, 0, 640);
  cairo_pattern_add_color_stop_rgba(g, 0, 1, 0.96, 0.85, 0.55);
  cairo_pattern_add_color_stop_rgba(g, 1, 1, 0.96, 0.85, 0.0);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
}

// The studio behind the set, w x h: black drapes, a scaffold, the show's
// neon sign (lit in-engine), bleachers for the audience.
void paintStudio(cairo_t* cr, double w, double h)
{
  cairo_rectangle(cr, 0, 0, w, h);
  linearFill(cr, 0, 0, 0, h, rgb(10, 10, 22), rgb(24, 18, 40));
  // Drapes.
  for (double x = 0; x < w; x += 48)
  {
    cairo_rectangle(cr, x, 0, 24, h * 0.62);
    linearFill(cr, x, 0, x + 24, 0, rgba(60, 30, 80, 90), rgba(0, 0, 0, 0));
  }
  // The scaffold across the top.
  cairo_rectangle(cr, 0, 40, w, 10);
  setColor(cr, rgb(40, 40, 52));
  cairo_fill(cr);
  for (double x = 0; x < w; x += 40)
  {
    cairo_move_to(cr, x, 50);
    cairo_line_to(cr, x + 20, 40);
    cairo_line_to(cr, x + 40, 50);
  }
  setColor(cr, rgb(60, 60, 76));
  cairo_set_line_width(cr, 2);
  cairo_stroke(cr);
  // Bleachers: three tiers stepping up at the back.
  for (int k = 0; k < 3; ++k)
  {
    const double y = h - 150 - k * 110;
    cairo_rectangle(cr, 0, y, w, 26);
    linearFill(cr, 0, y, 0, y + 26, rgb(70, 60, 90), rgb(30, 24, 44), true);
    outline(cr, 1.6);
    cairo_rectangle(cr, 0, y + 26, w, 84);
    setColor(cr, rgb(18, 14, 28));
    cairo_fill(cr);
  }
  // The neon sign's tubes, unlit.
  setColor(cr, rgb(60, 30, 50));
  centredText(cr, "GUNRUNNERS", w * 0.5, 150, 90);
}

// A TV camera on a pedestal, 160 x 260 standing on (80, 260), facing left.
void paintTvCamera(cairo_t* cr)
{
  cairo_rectangle(cr, 74, 120, 12, 120);
  linearFill(cr, 74, 0, 86, 0, kSteelLight, kSteelDark, true);
  outline(cr, 1.6);
  cairo_move_to(cr, 30, 260);
  cairo_line_to(cr, 80, 230);
  cairo_line_to(cr, 130, 260);
  setColor(cr, rgb(30, 30, 38));
  cairo_set_line_width(cr, 8);
  cairo_stroke(cr);
  roundedRect(cr, 40, 50, 100, 70, 8);
  linearFill(cr, 0, 50, 0, 120, rgb(80, 82, 96), rgb(20, 20, 26), true);
  outline(cr, 2.2);
  cairo_rectangle(cr, 6, 66, 38, 40);
  linearFill(cr, 0, 66, 0, 106, rgb(60, 62, 74), rgb(10, 10, 14), true);
  outline(cr, 2.0);
  cairo_arc(cr, 8, 86, 16, 0, 2 * kPi);
  setColor(cr, rgb(20, 10, 30));
  cairo_fill_preserve(cr);
  outline(cr, 2.0);
  cairo_arc(cr, 4, 82, 4, 0, 2 * kPi);
  setRgba(cr, rgb(200, 160, 255), 0.7);
  cairo_fill(cr);
  setColor(cr, rgb(220, 220, 230));
  centredText(cr, "MAXTV", 92, 92, 14);
  cairo_rectangle(cr, 120, 40, 14, 10);
  setColor(cr, rgb(90, 10, 14));
  cairo_fill(cr);
}

// The painted flat (the east wall): a column of rack fronts, w x h.
void paintFlat(cairo_t* cr, double w, double h)
{
  for (double y = 0; y < h; y += 256)
  {
    cairo_save(cr);
    cairo_translate(cr, 0, y);
    paintServerRack(cr, w, std::min(256.0, h - y), unsigned(y) + 5u, false);
    cairo_restore(cr);
  }
}

const CharacterArt& echoArt(const Art& art, const CharacterDef& player, int look)
{
  if (look >= 0 && look <= 2)
    return art.runner(characterByIndex(look));
  return art.runner(player);
}

const Sprite& poseSprite(const CharacterArt& ca, PlayerVisual v, int frame)
{
  switch (v)
  {
    case PlayerVisual::Walking:
      return ca.run[std::size_t((frame / 3) % kRunFrames)];
    case PlayerVisual::LookingUp:
      return ca.lookUp;
    case PlayerVisual::Crouching:
      return ca.crouch;
    case PlayerVisual::Coiling:
      return ca.coil;
    case PlayerVisual::Jumping:
    case PlayerVisual::Somersault:
      return ca.jump;
    case PlayerVisual::Falling:
      return ca.fall;
    case PlayerVisual::FallingFull:
      return ca.fallFast;
    case PlayerVisual::ClimbingLadder:
      return ca.climb[std::size_t((frame / 8) % 2)];
    case PlayerVisual::Hanging:
    case PlayerVisual::MovingOnPipe:
      return ca.hang;
    case PlayerVisual::AimingDownOnPipe:
      return ca.hangAimDown;
    case PlayerVisual::PullingLegsUp:
      return ca.hangLegsUp;
    case PlayerVisual::Jetpack:
      return ca.jetpack;
    case PlayerVisual::Dying:
      return ca.hurt;
    case PlayerVisual::Clinging:
      return ca.cling;
    case PlayerVisual::Standing:
    default:
      return ca.idle[std::size_t((frame / 30) % 2)];
  }
}

// A runner as a hologram: cyan, see-through, scan lines over it, now and
// then a slice of it jumping sideways.
void hologram(Renderer& r, const Art& art, const Sprite& spr, int facing, float x, float y, float a, int seed, int frame)
{
  const float flick = 0.75f + 0.25f * frac(hash2(seed, frame / 2));
  const bool glitch = hash2(seed * 7, frame / 3) % 9u == 0u;
  const float jx = glitch ? (hash2(seed, frame) % 2u ? 8.0f : -8.0f) : 0.0f;
  drawGlow(r, art, x, y - 80.0f, 110.0f, kCyan, 0.18f * a * flick);
  DrawOpts o;
  o.tint = rgb(120, 236, 255);
  o.alpha = 0.5f * a * flick;
  r.draw(spr.get(facing), x + jx, y, o);
  o.blend = Blend::Add;
  o.alpha = 0.4f * a * flick;
  r.draw(spr.get(facing), x + jx, y, o);
  for (float sy = y - 170.0f + float(frame % 6); sy < y; sy += 6.0f)
    r.fillRect(x - 46.0f + jx, sy, 92.0f, 2.0f, rgba(170, 255, 255, int(26 * a)), Blend::Add);
  if (glitch)
    r.fillRect(x - 50.0f, y - 60.0f - float(hash2(seed, frame) % 80u), 100.0f, 6.0f, rgba(170, 255, 255, int(90 * a)), Blend::Add);
  // The base it is projected from.
  r.fillRect(x - 36.0f, y - 3.0f, 72.0f, 4.0f, rgba(120, 240, 255, int(120 * a)), Blend::Add);
}

} // namespace

// --- Hooks outside the back and front passes ---------------------------------------------------

bool drawZeroShot(Renderer& r, const Art& art, const Projectile& pr, float cx, float cy, int frame)
{
  const bool phase = pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::PhaseRifle);
  if (!pr.echo && !phase)
    return false;
  float vx = pr.vx, vy = pr.vy;
  if (vx == 0.0f && vy == 0.0f)
  {
    vx = float(pr.dx);
    vy = float(pr.dy);
  }
  const float len = std::max(0.001f, std::sqrt(vx * vx + vy * vy));
  const float ux = vx / len, uy = vy / len;
  if (pr.echo)
  {
    // A hologram's copy of a blaster shot: a cyan dash with a flicker.
    const float f = 0.7f + 0.3f * float((frame / 2) % 2);
    drawGlow(r, art, cx, cy, 34.0f, kCyan, 0.55f * f);
    r.drawLine(cx - ux * 22.0f, cy - uy * 22.0f, cx + ux * 12.0f, cy + uy * 12.0f, 8.0f, rgba(90, 240, 255, int(170 * f)), Blend::Add);
    r.drawLine(cx - ux * 16.0f, cy - uy * 16.0f, cx + ux * 10.0f, cy + uy * 10.0f, 3.0f, rgba(230, 255, 255, int(230 * f)), Blend::Add);
    return true;
  }
  // The Phase Rifle: a violet streak with a long tail; inside a wall it goes
  // hollow, an outline flickering through the rack.
  const bool inside = pr.phase >= 0;
  if (inside)
  {
    const float f = 0.5f + 0.5f * float((frame / 2) % 2);
    for (const float side : {-1.0f, 1.0f})
      r.drawLine(cx - ux * 40.0f - uy * side * 6.0f, cy - uy * 40.0f + ux * side * 6.0f, cx + ux * 14.0f - uy * side * 6.0f,
        cy + uy * 14.0f + ux * side * 6.0f, 2.0f, rgba(220, 170, 255, int(200 * f)), Blend::Add);
    r.drawLine(cx + ux * 14.0f - uy * 6.0f, cy + uy * 14.0f + ux * 6.0f, cx + ux * 14.0f + uy * 6.0f,
      cy + uy * 14.0f - ux * 6.0f, 2.0f, rgba(220, 170, 255, int(200 * f)), Blend::Add);
    drawGlow(r, art, cx, cy, 40.0f, kViolet, 0.35f * f);
    return true;
  }
  drawGlow(r, art, cx, cy, 44.0f, kViolet, 0.6f);
  r.drawLine(cx - ux * 70.0f, cy - uy * 70.0f, cx + ux * 16.0f, cy + uy * 16.0f, 10.0f, rgba(170, 90, 255, 120), Blend::Add);
  r.drawLine(cx - ux * 44.0f, cy - uy * 44.0f, cx + ux * 16.0f, cy + uy * 16.0f, 6.0f, rgba(210, 150, 255, 220), Blend::Add);
  r.drawLine(cx - ux * 24.0f, cy - uy * 24.0f, cx + ux * 14.0f, cy + uy * 14.0f, 2.5f, rgba(255, 245, 255, 255), Blend::Add);
  // Little phase rings riding along it.
  for (int k = 0; k < 2; ++k)
  {
    const float t = std::fmod(float(frame) * 0.15f + float(k) * 0.5f, 1.0f);
    const float px = cx - ux * 50.0f * t, py = cy - uy * 50.0f * t;
    const float rr = 6.0f + 6.0f * t;
    r.drawLine(px - uy * rr, py + ux * rr, px + uy * rr, py - ux * rr, 2.0f, rgba(230, 200, 255, int(200 * (1.0f - t))), Blend::Add);
  }
  return true;
}

void drawWireframeBackdrop(Renderer& r, float camX, float camY, int frame)
{
  r.fillRect(0.0f, 0.0f, float(kScreenW), float(kScreenH), rgb(2, 4, 6));
  const float ox = -std::fmod(camX, kTilePx), oy = -std::fmod(camY, kTilePx);
  for (float x = ox; x < float(kScreenW); x += kTilePx)
    r.fillRect(x, 0.0f, 1.0f, float(kScreenH), rgba(40, 140, 80, 40));
  for (float y = oy; y < float(kScreenH); y += kTilePx)
    r.fillRect(0.0f, y, float(kScreenW), 1.0f, rgba(40, 140, 80, 40));
  // Every fourth line brighter, like a debug grid.
  const float ox4 = -std::fmod(camX, kTilePx * 4.0f), oy4 = -std::fmod(camY, kTilePx * 4.0f);
  for (float x = ox4; x < float(kScreenW); x += kTilePx * 4.0f)
    r.fillRect(x, 0.0f, 1.5f, float(kScreenH), rgba(60, 200, 110, 60));
  for (float y = oy4; y < float(kScreenH); y += kTilePx * 4.0f)
    r.fillRect(0.0f, y, float(kScreenW), 1.5f, rgba(60, 200, 110, 60));
  // A debug readout in the corner.
  const int fps = 60 - int(hash2(frame / 30, 3) % 2u);
  r.drawText("DEBUG VIEW  FPS " + std::to_string(fps), 16.0f, float(kScreenH) - 30.0f, {14.0f, rgb(80, 255, 140), rgb(0, 0, 0)});
}

// --- The back pass ------------------------------------------------------------------------

void World::drawZeroBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)alpha;
  const auto& z = mZero;
  const auto& b = z.boss;
  const int bx0 = std::max(0, int(camX / kTilePx) - 1), by0 = std::max(0, int(camY / kTilePx) - 1);
  const int bx1 = std::min(mLevel->width - 1, int((camX + float(kScreenW)) / kTilePx) + 1);
  const int by1 = std::min(mLevel->height - 1, int((camY + float(kScreenH)) / kTilePx) + 1);

  // The Wireframe bonus: what was solid is a glowing green wireframe; the
  // hitboxes are the ground now, outlined in pink.
  if (z.wireframe)
  {
    for (const auto& [wx, wy] : z.wires)
    {
      if (wx < bx0 || wx > bx1 || wy < by0 || wy > by1)
        continue;
      const float x = float(wx) * kTilePx - camX, y = float(wy) * kTilePx - camY;
      const float d = 12.0f;
      const int a = 150 + int(60.0f * std::sin(float(frame) * 0.08f + float(wx + wy) * 0.6f));
      const Color c = withAlpha(kWire, a), back = withAlpha(kWire, a / 3);
      // The back face, the edges to it, the front face.
      r.drawLine(x + d, y - d, x + kTilePx + d, y - d, 1.5f, back, Blend::Add);
      r.drawLine(x + kTilePx + d, y - d, x + kTilePx + d, y + kTilePx - d, 1.5f, back, Blend::Add);
      for (const auto& [px, py] : {std::pair{0.0f, 0.0f}, {kTilePx, 0.0f}, {kTilePx, kTilePx}})
        r.drawLine(x + px, y + py, x + px + d, y + py - d, 1.5f, back, Blend::Add);
      r.drawLine(x, y, x + kTilePx, y, 2.0f, c, Blend::Add);
      r.drawLine(x, y + kTilePx, x + kTilePx, y + kTilePx, 2.0f, c, Blend::Add);
      r.drawLine(x, y, x, y + kTilePx, 2.0f, c, Blend::Add);
      r.drawLine(x + kTilePx, y, x + kTilePx, y + kTilePx, 2.0f, c, Blend::Add);
      r.drawLine(x, y, x + kTilePx, y + kTilePx, 1.0f, withAlpha(kWire, a / 5), Blend::Add);
    }
    for (std::size_t i = 0; i < z.hitboxes.size(); ++i)
    {
      const auto& h = z.hitboxes[i];
      const float x = float(h[0]) * kTilePx - camX, y = float(h[1]) * kTilePx - camY;
      const float w = float(h[2] - h[0] + 1) * kTilePx, hh = float(h[3] - h[1] + 1) * kTilePx;
      if (!visible(x, y, w, hh))
        continue;
      r.fillRect(x, y, w, hh, rgba(255, 60, 210, 46));
      // Dashed outline.
      const int phase = (frame / 4) % 12;
      for (float sx = x - float(phase); sx < x + w; sx += 12.0f)
      {
        const float s0 = std::max(sx, x), s1 = std::min(sx + 7.0f, x + w);
        if (s1 > s0)
        {
          r.fillRect(s0, y, s1 - s0, 2.5f, kMagenta);
          r.fillRect(s0, y + hh - 2.5f, s1 - s0, 2.5f, kMagenta);
        }
      }
      for (float sy = y + float(phase); sy < y + hh; sy += 12.0f)
      {
        const float s0 = std::max(sy, y), s1 = std::min(sy + 7.0f, y + hh);
        if (s1 > s0)
        {
          r.fillRect(x, s0, 2.5f, s1 - s0, kMagenta);
          r.fillRect(x + w - 2.5f, s0, 2.5f, s1 - s0, kMagenta);
        }
      }
      for (const float cxp : {x, x + w})
        for (const float cyp : {y, y + hh})
          r.fillRect(cxp - 4.0f, cyp - 4.0f, 8.0f, 8.0f, rgb(255, 200, 250));
      r.drawText("HITBOX " + std::to_string(i), x + 6.0f, y + 6.0f, {12.0f, rgb(255, 170, 240), rgb(30, 0, 30)});
      if (w >= 192.0f)
        r.drawText(std::to_string(h[2] - h[0] + 1) + "x" + std::to_string(h[3] - h[1] + 1), x + w - 6.0f, y + 6.0f,
          {12.0f, rgb(255, 170, 240), rgb(30, 0, 30)}, Align::Right);
    }
    // The world's bounds.
    const float lw = float(mLevel->width) * kTilePx, lh = float(mLevel->height) * kTilePx;
    r.fillRect(kTilePx - camX, kTilePx - camY, lw - 2.0f * kTilePx, 2.0f, rgba(255, 255, 255, 90));
    r.fillRect(kTilePx - camX, lh - kTilePx - camY, lw - 2.0f * kTilePx, 2.0f, rgba(255, 255, 255, 90));
    r.fillRect(kTilePx - camX, kTilePx - camY, 2.0f, lh - 2.0f * kTilePx, rgba(255, 255, 255, 90));
    r.fillRect(lw - kTilePx - camX, kTilePx - camY, 2.0f, lh - 2.0f * kTilePx, rgba(255, 255, 255, 90));
    return;
  }

  // The racks' status lights blinking: a few lights on each rack block in
  // view, on their own rhythms.
  for (int by = by0; by <= by1; ++by)
    for (int bx = bx0; bx <= bx1; ++bx)
    {
      if (mMap.block(bx, by) != Tile::Solid || mLayerMask[std::size_t(by * mLevel->width + bx)])
        continue;
      const float x = float(bx) * kTilePx - camX, y = float(by) * kTilePx - camY;
      for (int u = 0; u < 4; ++u)
      {
        const std::uint32_t h = hash2(bx * 4 + u, by * 7);
        if (h % 3u == 0u)
          continue;
        const int i = int((h >> 4) % 3u);
        const int period = 20 + int((h >> 8) % 70u);
        const bool on = ((frame + int(h >> 12)) % period) < period / 2 + int((h >> 20) % 5u);
        if (!on)
          continue;
        const std::uint32_t pick = (h >> 16) % 10u;
        const Color c = pick < 6 ? rgb(255, 60, 70) : (pick < 9 ? rgb(90, 255, 130) : rgb(255, 190, 80));
        r.fillRect(x + 45.5f + float(i) * 4.2f, y + float(u) * 16.0f + 6.5f, 3.0f, 3.0f, c, Blend::Add);
        if (pick == 0)
          drawGlow(r, mArt, x + 47.0f + float(i) * 4.2f, y + float(u) * 16.0f + 8.0f, 10.0f, c, 0.6f);
      }
    }

  // Turret rails along the ceiling.
  for (const auto& e : mEnemies)
  {
    if (e.kind != EnemyKind::LatticeTurret)
      continue;
    const float x0 = float(e.railX0) * kCellPx - camX, x1 = float(e.railX1 + e.w) * kCellPx - camX;
    const float y = float(e.aimY - e.h + 1) * kCellPx - camY;
    if (!visible(x0, y - 24.0f, x1 - x0, 30.0f))
      continue;
    const int wpx = int(x1 - x0);
    const Texture& rail = baked(mArt, r, "rail/" + std::to_string(wpx), wpx, 24, 0.0f, 24.0f,
      [&](cairo_t* cr) { paintRail(cr, double(wpx)); });
    r.draw(rail, x0, y + 6.0f);
  }

  // RACK 42's insides: the back of the cabinet, cables, a dim red light.
  for (const auto& d : z.decos)
  {
    if (d.kind != "rack42")
      continue;
    const float x = float(d.x0 + 1) * kTilePx - camX, y = float(d.y0 + 1) * kTilePx - camY;
    const float w = float(d.x1 - d.x0 - 1) * kTilePx, h = float(d.y1 - d.y0) * kTilePx;
    if (!visible(x, y, w, h))
      continue;
    r.fillRect(x, y, w, h, rgb(12, 10, 14));
    for (float cx = x + 20.0f; cx < x + w; cx += 34.0f)
      r.drawLine(cx, y, cx + 10.0f * std::sin(cx), y + h, 5.0f, rgb(30, 28, 36));
    for (float yy = y + 30.0f; yy < y + h; yy += 48.0f)
      r.fillRect(x, yy, w, 3.0f, rgb(26, 26, 32));
    drawGlow(r, mArt, x + w * 0.5f, y + 20.0f, w * 0.5f, kRed, 0.25f);
  }

  // The rack door (DOOR1) and the floor grating, while they stand.
  for (const auto& br : mBreakables)
  {
    if (br.broken || (br.look != 14 && br.look != 15))
      continue;
    const float x = float(br.x0) * kTilePx - camX, y = float(br.y0) * kTilePx - camY;
    const int wpx = (br.x1 - br.x0 + 1) * int(kTilePx), hpx = (br.y1 - br.y0 + 1) * int(kTilePx);
    if (!visible(x, y, float(wpx), float(hpx)))
      continue;
    if (br.look == 14)
    {
      const int dmg = std::clamp(3 - br.hp / 2, 0, 3);
      const Texture& tex = baked(mArt, r, "rackdoor/" + std::to_string(wpx) + "x" + std::to_string(hpx) + "/" + std::to_string(dmg),
        wpx, hpx, 0.0f, 0.0f, [&](cairo_t* cr) { paintRackDoor(cr, double(wpx), double(hpx), dmg); });
      r.draw(tex, x, y);
      // Its lock light.
      r.fillRect(x + float(wpx) - 24.0f, y + float(hpx) * 0.5f - 46.0f, 8.0f, 8.0f, (frame / 20) % 2 ? kRed : rgb(110, 20, 24));
    }
    else
    {
      const Texture& tex = baked(mArt, r, "grate/" + std::to_string(wpx), wpx, 64, 0.0f, 0.0f,
        [&](cairo_t* cr) { paintGrate(cr, double(wpx)); });
      r.draw(tex, x, y);
    }
  }

  // Echo pads.
  for (const auto& p : z.pads)
  {
    const float cx = float(p.x) * kTilePx + 32.0f - camX, fy = float(p.y + 1) * kTilePx - camY;
    if (!visible(cx - 80.0f, fy - 200.0f, 160.0f, 220.0f))
      continue;
    const Texture& tex = p.armed ? baked(mArt, r, "pad/on", 128, 32, 64.0f, 16.0f, [](cairo_t* cr) { paintPad(cr, true); })
                                 : baked(mArt, r, "pad/off", 128, 32, 64.0f, 16.0f, [](cairo_t* cr) { paintPad(cr, false); });
    r.draw(tex, cx, fy - 4.0f);
    if (p.armed)
    {
      // A holographic shimmer rising off it, quicker while an Echo is
      // coming.
      const bool coming = p.passedAt >= 0;
      const float speed = coming ? 3.0f : 1.2f;
      drawGlow(r, mArt, cx, fy - 10.0f, 70.0f, kCyan, coming ? 0.5f : 0.25f);
      for (int k = 0; k < 7; ++k)
      {
        const float px = cx - 48.0f + float(k) * 16.0f;
        const float t = std::fmod(float(frame) * speed + float(k) * 37.0f, 120.0f) / 120.0f;
        const float top = fy - 8.0f - 150.0f * t;
        const int a = int(110.0f * (1.0f - t));
        r.fillRect(px, top, 2.0f, 40.0f * (1.0f - t) + 6.0f, rgba(140, 250, 255, a), Blend::Add);
      }
      for (int k = 0; k < 3; ++k)
      {
        const float t = std::fmod(float(frame) * 0.02f * speed + float(k) / 3.0f, 1.0f);
        r.fillRect(cx - 56.0f + 6.0f * t, fy - 12.0f - 140.0f * t, 112.0f - 12.0f * t, 2.0f, rgba(140, 250, 255, int(70 * (1.0f - t))),
          Blend::Add);
      }
    }
  }

  // Terminals.
  for (const auto& t : z.terminals)
  {
    const float cx = float(t.x) * kTilePx + 32.0f - camX, fy = float(t.y + 1) * kTilePx - camY;
    if (!visible(cx - 40.0f, fy - 100.0f, 80.0f, 100.0f))
      continue;
    r.draw(baked(mArt, r, "terminal", 72, 96, 36.0f, 96.0f, paintTerminal), cx, fy);
    // Its screen: a cursor, or lit text lines.
    const bool lit = t.shown > 0;
    for (int k = 0; k < 4; ++k)
    {
      const float lw = lit ? 18.0f + float(hash2(t.x + k, frame / 10) % 22u) : (k == 0 ? 10.0f : 0.0f);
      if (lw > 0.0f)
        r.fillRect(cx - 21.0f, fy - 84.0f + float(k) * 7.0f, lw, 3.0f, lit ? rgb(110, 255, 150) : rgb(40, 120, 60));
    }
    if (!lit && (frame / 20) % 2 == 0)
      r.fillRect(cx - 9.0f, fy - 84.0f, 5.0f, 4.0f, rgb(110, 255, 150));
    if (lit)
      drawGlow(r, mArt, cx, fy - 76.0f, 40.0f, rgb(110, 255, 150), 0.35f);
  }

  // Bulkheads: server-rack slabs sliding out of the ceiling or the floor.
  for (std::size_t i = 0; i < z.doors.size(); ++i)
  {
    const auto& d = z.doors[i];
    if (d.segment || d.layer < 0 || d.layer >= int(mLayers.size()) || d.pos <= 0.001f)
      continue;
    const Layer& l = mLayers[std::size_t(d.layer)];
    const float x = float(l.x0) * kTilePx - camX, y = float(l.y0) * kTilePx - camY;
    const int wpx = (l.x1 - l.x0 + 1) * int(kTilePx), hpx = (l.y1 - l.y0 + 1) * int(kTilePx);
    if (!visible(x, y, float(wpx), float(hpx)))
      continue;
    const Texture& tex = baked(mArt, r,
      "bulkhead/" + std::to_string(wpx) + "x" + std::to_string(hpx) + "/" + std::to_string(i % 4) + (d.fromCeiling ? "c" : "f"),
      wpx, hpx, 0.0f, 0.0f, [&](cairo_t* cr) { paintBulkhead(cr, double(wpx), double(hpx), unsigned(i * 13 + 5), d.fromCeiling); });
    const float shown = float(hpx) * std::clamp(d.pos, 0.0f, 1.0f);
    const bool moving = d.t < d.slide && d.from != d.to;
    if (d.fromCeiling)
    {
      clipTo(r, x, y, float(wpx), shown);
      r.draw(tex, x, y - (float(hpx) - shown));
    }
    else
    {
      clipTo(r, x, y + float(hpx) - shown, float(wpx), shown);
      r.draw(tex, x, y + float(hpx) - shown);
    }
    unclip(r);
    if (moving)
    {
      // Warning lights on its leading edge, and sparks off the guides.
      const float ey = d.fromCeiling ? y + shown : y + float(hpx) - shown;
      const bool blink = (frame / 5) % 2 == 0;
      drawGlow(r, mArt, x + float(wpx) * 0.5f, ey, 60.0f, kRed, blink ? 0.7f : 0.3f);
      for (const float sx : {x + 2.0f, x + float(wpx) - 2.0f})
        if (hash2(int(sx), frame) % 3u == 0u)
          sparkle(r, mArt, sx, ey + (d.fromCeiling ? -6.0f : 6.0f), 8.0f, rgb(255, 210, 120), 0.8f);
    }
  }
  // Bulkheads about to move (the monitors are previewing their shift): their
  // slots blink red where they will be.
  if (z.preview >= 0 && z.preview < int(z.shifts.size()) && (frame / 6) % 2 == 0)
  {
    const auto& s = z.shifts[std::size_t(z.preview)];
    for (const auto* list : {&s.open, &s.close})
      for (const int li : *list)
      {
        if (li < 0 || li >= int(mLayers.size()))
          continue;
        const Layer& l = mLayers[std::size_t(li)];
        const float x = float(l.x0) * kTilePx - camX, y = float(l.y0) * kTilePx - camY;
        const float w = float(l.x1 - l.x0 + 1) * kTilePx, h = float(l.y1 - l.y0 + 1) * kTilePx;
        if (!visible(x, y, w, h))
          continue;
        r.fillRect(x, y, w, 3.0f, withAlpha(kRed, 200));
        r.fillRect(x, y + h - 3.0f, w, 3.0f, withAlpha(kRed, 200));
        r.fillRect(x, y, 3.0f, h, withAlpha(kRed, 200));
        r.fillRect(x + w - 3.0f, y, 3.0f, h, withAlpha(kRed, 200));
      }
  }

  // Monitors hanging from the ceiling.
  const bool overloadSchematic = b.on && b.phase == ZeroPhase::Overload && b.cycle < 45;
  for (std::size_t mi = 0; mi < z.monitors.size(); ++mi)
  {
    const auto& m = z.monitors[mi];
    const float cx = float(m.x) * kTilePx + 32.0f - camX, top = float(m.y) * kTilePx - camY;
    const float mx = cx - 64.0f, my = top + 34.0f;
    if (!visible(mx, top, 128.0f, 150.0f))
      continue;
    // Cables up into the ceiling, the monitor swaying a touch.
    const float sway = std::sin(float(frame) * 0.03f + float(mi)) * 1.5f;
    r.drawLine(cx - 30.0f, top, cx - 30.0f + sway, my + 4.0f, 3.0f, rgb(26, 26, 32));
    r.drawLine(cx + 30.0f, top, cx + 30.0f + sway, my + 4.0f, 3.0f, rgb(26, 26, 32));
    const float sx = mx + 14.0f + sway, sy = my + 12.0f, sw = 100.0f, sh = 72.0f;
    r.fillRect(sx, sy, sw, sh, rgb(4, 6, 8));
    if (z.preview >= 0 && z.preview < int(z.shifts.size()))
    {
      // The next state's schematic: the corridor's outline, its bulkheads,
      // the ones that will move in red.
      const auto& s = z.shifts[std::size_t(z.preview)];
      const bool fuzz = z.previewT < 6;
      r.fillRect(sx, sy, sw, sh, rgb(6, 16, 34));
      int lo = 1 << 30, hi = -1;
      for (const auto* list : {&s.open, &s.close})
        for (const int li : *list)
          if (li >= 0 && li < int(mLayers.size()))
          {
            lo = std::min(lo, mLayers[std::size_t(li)].x0);
            hi = std::max(hi, mLayers[std::size_t(li)].x1);
          }
      if (hi < 0)
      {
        lo = m.x - 10;
        hi = m.x + 10;
      }
      const int span = std::max(24, hi - lo + 12);
      const int left = (lo + hi) / 2 - span / 2;
      const float scale = (sw - 8.0f) / float(span);
      const float rowTop = 24.0f, rows = 12.0f;
      const float vs = (sh - 22.0f) / rows;
      auto sxOf = [&](float bx) { return sx + 4.0f + (bx - float(left)) * scale; };
      auto syOf = [&](float by) { return sy + 14.0f + (by - rowTop) * vs; };
      // Ceiling and floor lines.
      r.fillRect(sx + 4.0f, syOf(24.0f), sw - 8.0f, 1.5f, rgb(110, 170, 255));
      r.fillRect(sx + 4.0f, syOf(36.0f), sw - 8.0f, 1.5f, rgb(110, 170, 255));
      for (std::size_t di = 0; di < z.doors.size(); ++di)
      {
        const auto& d = z.doors[di];
        if (d.segment || d.layer < 0 || d.layer >= int(mLayers.size()))
          continue;
        const Layer& l = mLayers[std::size_t(d.layer)];
        if (l.x1 < left || l.x0 > left + span)
          continue;
        bool opens = false, closes = false;
        for (const int li : s.open)
          opens = opens || li == d.layer;
        for (const int li : s.close)
          closes = closes || li == d.layer;
        const float x0 = std::max(sx + 4.0f, sxOf(float(l.x0))), x1 = std::min(sx + sw - 4.0f, sxOf(float(l.x1 + 1)));
        const float y0 = syOf(float(l.y0)), y1 = syOf(float(l.y1 + 1));
        if (closes)
          r.fillRect(x0, y0, std::max(2.0f, x1 - x0), y1 - y0, (z.previewT / 4) % 2 ? kRed : rgb(255, 120, 120));
        else if (opens)
        {
          r.fillRect(x0, y0, std::max(2.0f, x1 - x0), 1.5f, kRed);
          r.fillRect(x0, y1 - 1.5f, std::max(2.0f, x1 - x0), 1.5f, kRed);
          r.fillRect(x0, y0, 1.5f, y1 - y0, kRed);
          r.fillRect(x1 - 1.5f, y0, 1.5f, y1 - y0, kRed);
        }
        else if (d.pos > 0.5f)
          r.fillRect(x0, y0, std::max(2.0f, x1 - x0), y1 - y0, rgb(110, 170, 255));
      }
      // You are here.
      const float px = sxOf(float(mPlayer.x) / float(kCellsPerTile));
      if (px > sx + 2.0f && px < sx + sw - 2.0f && (frame / 8) % 2 == 0)
        r.fillRect(px - 1.5f, syOf(33.0f), 3.0f, syOf(36.0f) - syOf(33.0f), rgb(255, 255, 255));
      r.drawText("NEXT: " + s.id, sx + 5.0f, sy + 2.0f, {10.0f, rgb(255, 200, 200), rgb(0, 0, 0)});
      r.fillRect(sx + 4.0f, sy + sh - 5.0f, (sw - 8.0f) * float(std::min(z.previewT, kShiftPreview)) / float(kShiftPreview), 2.0f, kRed);
      if (fuzz)
        for (float yy = sy; yy < sy + sh; yy += 3.0f)
        {
          const int v = 60 + int(hash2(int(yy), frame) % 150u);
          r.fillRect(sx, yy, sw, 3.0f, rgba(v, v, v, 200));
        }
    }
    else if (overloadSchematic)
    {
      // The arena floor: twelve segments, the four that stay lit green.
      r.fillRect(sx, sy, sw, sh, rgb(6, 20, 12));
      r.drawText("FLOOR", sx + 5.0f, sy + 2.0f, {10.0f, rgb(170, 255, 190), rgb(0, 0, 0)});
      const float segW = (sw - 10.0f) / float(kZeroSegments);
      for (int k = 0; k < kZeroSegments; ++k)
      {
        const bool safe = (b.safe >> k) & 1u;
        const float x = sx + 5.0f + float(k) * segW;
        r.fillRect(x + 0.5f, sy + 38.0f, segW - 1.5f, 12.0f, safe ? kGreen : rgb(110, 30, 34));
        if (safe)
          r.fillRect(x + 0.5f, sy + 52.0f, segW - 1.5f, 2.0f, rgba(90, 255, 130, 120));
      }
      // The eye over it.
      r.fillRect(sx + sw * 0.5f - 6.0f, sy + 18.0f, 12.0f, 10.0f, kRed);
    }
    else
    {
      // ZERO's red eye, or scrolling green code, switching every few
      // seconds.
      const int show = int((hash2(int(mi), 11) + std::uint32_t(frame / 240)) % 3u);
      if (show == 0)
      {
        r.fillRect(sx, sy, sw, sh, rgb(20, 2, 4));
        const float ecx = sx + sw * 0.5f, ecy = sy + sh * 0.5f;
        drawGlow(r, mArt, ecx, ecy, 40.0f, kRed, 0.7f);
        r.fillRect(ecx - 16.0f, ecy - 16.0f, 32.0f, 32.0f, rgba(180, 20, 30, 255));
        r.fillRect(ecx - 12.0f, ecy - 20.0f, 24.0f, 40.0f, rgba(180, 20, 30, 255));
        r.fillRect(ecx - 20.0f, ecy - 12.0f, 40.0f, 24.0f, rgba(180, 20, 30, 255));
        const float pr = 6.0f + 2.0f * std::sin(float(frame) * 0.05f + float(mi));
        r.fillRect(ecx - pr, ecy - pr, pr * 2.0f, pr * 2.0f, rgb(10, 0, 2));
        r.fillRect(ecx - 10.0f, ecy - 13.0f, 4.0f, 4.0f, rgba(255, 220, 220, 200));
      }
      else
      {
        r.fillRect(sx, sy, sw, sh, rgb(2, 12, 4));
        const int scroll = frame / 2 + int(mi) * 17;
        for (int k = 0; k < 9; ++k)
        {
          const int line = scroll / 8 + k;
          const float ly = sy + 4.0f + float(k) * 8.0f - float(scroll % 8);
          if (ly < sy + 1.0f || ly > sy + sh - 6.0f)
            continue;
          const std::uint32_t h = hash2(line, int(mi));
          const float indent = float(h % 3u) * 8.0f;
          const float lw = 14.0f + float((h >> 3) % 60u);
          r.fillRect(sx + 5.0f + indent, ly, std::min(lw, sw - 10.0f - indent), 3.0f, h % 7u == 0u ? kRed : rgb(80, 220, 110));
        }
      }
    }
    // Scan lines and the glow off the glass.
    for (float yy = sy; yy < sy + sh; yy += 3.0f)
      r.fillRect(sx, yy, sw, 1.0f, rgba(0, 0, 0, 70));
    r.draw(baked(mArt, r, "monitor", 128, 104, 0.0f, 0.0f, paintMonitor), mx + sway, my);
    drawGlow(r, mArt, sx + sw * 0.5f, sy + sh * 0.5f, 80.0f, z.preview >= 0 ? kRed : (overloadSchematic ? kGreen : rgb(255, 80, 80)),
      0.12f);
  }

  // Laser grids' emitter posts.
  for (const auto& g : z.grids)
  {
    const float x0 = float(g.x0) * kTilePx - camX, x1 = float(g.x1 + 1) * kTilePx - camX;
    const float y0 = float(g.y0) * kTilePx - camY, y1 = float(g.y1 + 1) * kTilePx - camY;
    if (!visible(x0 - 20.0f, y0, x1 - x0 + 40.0f, y1 - y0))
      continue;
    for (const float px : {x0 - 4.0f, x1 - 10.0f})
    {
      r.fillRect(px, y0, 14.0f, y1 - y0, rgb(30, 30, 38));
      r.fillRect(px + 2.0f, y0, 3.0f, y1 - y0, rgb(80, 84, 96));
      for (float yy = y0 + 16.0f; yy < y1; yy += 32.0f)
        r.fillRect(px + 3.0f, yy - 3.0f, 8.0f, 6.0f, rgb(120, 20, 26));
    }
  }

  // Kill switches.
  for (const auto& s : z.switches)
  {
    const float x = float(s.x) * kTilePx - camX, y = float(s.y) * kTilePx - camY;
    if (!visible(x, y, 64.0f, 128.0f))
      continue;
    const Texture& tex = s.hit ? baked(mArt, r, "switch/off", 64, 128, 0.0f, 0.0f, [](cairo_t* cr) { paintSwitch(cr, true); })
                               : baked(mArt, r, "switch/on", 64, 128, 0.0f, 0.0f, [](cairo_t* cr) { paintSwitch(cr, false); });
    r.draw(tex, x, y);
    if (!s.hit)
      drawGlow(r, mArt, x + 32.0f, y + 72.0f, 50.0f, kRed, (frame / 10) % 2 ? 0.6f : 0.25f);
    else
      drawGlow(r, mArt, x + 32.0f, y + 72.0f, 40.0f, kGreen, 0.3f);
  }

  // The arena: the pit under the floor and its live grille.
  if (b.on)
  {
    for (const auto& l : mLavas)
    {
      const float x = float(l.x0) * kCellPx - camX, y = float(l.y0) * kCellPx - camY;
      const float w = float(l.x1 - l.x0 + 1) * kCellPx, h = float(l.y1 - l.y0 + 1) * kCellPx;
      if (!visible(x, y - 80.0f, w, h + 80.0f))
        continue;
      const float x0 = std::max(x, -40.0f), x1 = std::min(x + w, float(kScreenW) + 40.0f);
      // The pit's dark and the grille's bars.
      r.fillRect(x0, y - 2.0f * kTilePx, x1 - x0, 2.0f * kTilePx + h, rgb(6, 4, 8));
      r.fillRect(x0, y + 10.0f, x1 - x0, h - 10.0f, rgb(20, 8, 14));
      for (float gx = x0 - std::fmod(x0 + camX, 16.0f); gx < x1; gx += 16.0f)
        r.fillRect(gx, y + 6.0f, 4.0f, h - 6.0f, rgb(60, 50, 64));
      r.fillRect(x0, y + 6.0f, x1 - x0, 5.0f, rgb(110, 100, 120));
      r.fillRect(x0, y + 26.0f, x1 - x0, 4.0f, rgb(70, 60, 80));
      // It hums: a red glow and small arcs crawling along it.
      for (float gx = x0 + 40.0f; gx < x1; gx += 180.0f)
        drawGlow(r, mArt, gx, y + 12.0f, 90.0f, kRed, 0.18f + 0.06f * std::sin(float(frame) * 0.1f + gx));
      for (int k = 0; k < 4; ++k)
      {
        const std::uint32_t h2 = hash2(k, frame / 3);
        if (h2 % 3u == 0u)
          continue;
        const float ax = x0 + frac(h2) * (x1 - x0);
        zap(r, ax, y + 8.0f, ax + 40.0f, y + 8.0f, int(h2), rgb(255, 80, 90), 3.0f, 4);
      }
      // The beam: crackling up through the gaps where the floor dropped.
      if (b.beam > 0)
        for (const auto& d : z.doors)
        {
          if (!d.segment || d.pos > 0.5f || d.layer < 0)
            continue;
          const Layer& sl = mLayers[std::size_t(d.layer)];
          const float sx0 = float(sl.x0) * kTilePx - camX, sx1 = float(sl.x1 + 1) * kTilePx - camX;
          drawGlow(r, mArt, (sx0 + sx1) * 0.5f, y + 10.0f, 150.0f, rgb(255, 120, 120), 0.55f);
          for (int k = 0; k < 5; ++k)
          {
            const std::uint32_t h2 = hash2(sl.x0 * 13 + k, frame / 2);
            const float ax = sx0 + frac(h2) * (sx1 - sx0);
            zap(r, ax, y + 8.0f, ax + (frac(h2 >> 8) - 0.5f) * 60.0f, y - 60.0f - frac(h2 >> 16) * 70.0f, int(h2), rgb(255, 90, 110), 5.0f, 6);
          }
          r.fillRect(sx0, y + 4.0f, sx1 - sx0, 8.0f, rgba(255, 220, 220, 200), Blend::Add);
        }
    }
  }

  // ZERO, the studio behind it, the eye, its racks, the floor.
  if (!b.on)
    return;
  const float ax0 = float(b.ax0) * kTilePx - camX, ay0 = float(b.ay0) * kTilePx - camY;
  const float aw = float(b.ax1 - b.ax0 + 1) * kTilePx, ah = float(b.ay1 - b.ay0 + 1) * kTilePx;
  const bool arenaVisible = visible(ax0 - 200.0f, ay0 - 200.0f, aw + 400.0f, ah + 400.0f);
  const int reveal = b.reveal;
  const bool revealing = reveal >= 0 || b.phase == ZeroPhase::Reveal || b.phase == ZeroPhase::Done;
  const int rv = reveal >= 0 ? reveal : (b.phase == ZeroPhase::Done ? kRevealFrames : 0);

  if (arenaVisible && revealing && rv >= 90)
  {
    // The studio coming out from behind the set: it fades in as the flat
    // falls.
    const float studio = ease(float(rv - 90) / 30.0f);
    const float wallX = b.wallX0 >= 0 ? float(b.wallX1 + 1) * kTilePx - camX : ax0 + aw;
    const float sx0 = ax0, sw = wallX - ax0;
    const int swpx = int(sw), shpx = int(ah + kTilePx);
    const Texture& back = baked(mArt, r, "studio/" + std::to_string(swpx) + "x" + std::to_string(shpx), swpx, shpx, 0.0f, 0.0f,
      [&](cairo_t* cr) { paintStudio(cr, double(swpx), double(shpx)); });
    DrawOpts so;
    so.alpha = studio;
    r.draw(back, sx0, ay0, so);
    // The sign lights with the last bank.
    if (b.lights >= 3)
    {
      const float flick = (rv / 3) % 7 == 0 ? 0.6f : 1.0f;
      drawGlow(r, mArt, sx0 + sw * 0.5f, ay0 + 120.0f, 340.0f, rgb(255, 60, 200), 0.35f * flick * studio);
      r.drawText("GUNRUNNERS", sx0 + sw * 0.5f, ay0 + 70.0f, {90.0f, rgb(255, 120, 230), rgb(120, 0, 90)}, Align::Center, flick * studio);
    }
    // TV cameras on pedestals either side, tally lights on.
    const Texture& cam = baked(mArt, r, "tvcam", 160, 260, 80.0f, 260.0f, paintTvCamera);
    for (const float cxp : {sx0 + 160.0f, sx0 + sw - 200.0f})
    {
      DrawOpts co;
      co.alpha = studio;
      r.draw(cam, cxp, ay0 + ah, co);
      if (b.lights >= 1 && (frame / 30) % 2 == 0)
        drawGlow(r, mArt, cxp + 47.0f, ay0 + ah - 215.0f, 16.0f, kRed, 0.9f * studio);
    }
    // The audience on the bleachers: dark until the lights come on.
    const float lit = std::min(1.0f, float(b.lights) / 2.0f);
    const Color tint = lerpColor(rgb(40, 34, 60), rgb(255, 255, 255), lit);
    const int afr = (frame / 6) % 6;
    for (int tier = 0; tier < 3; ++tier)
    {
      const float rowY = ay0 + float(shpx) - 150.0f - float(tier) * 110.0f + 4.0f;
      int k = 0;
      for (float px = sx0 + 40.0f + float(tier % 2) * 34.0f; px < sx0 + sw - 30.0f; px += 68.0f, ++k)
      {
        if (px + camX < -200.0f || px > float(kScreenW) + 80.0f)
          continue;
        int kind = 0;
        if (tier == 1 && k == 4)
          kind = 1; // Black Halo's pilot
        else if (tier == 0 && k == 9)
          kind = 2; // the spare host from the cryo pod
        const unsigned seed = unsigned(tier * 31 + k * 7) % 12u;
        const std::string key = "aud/" + std::to_string(kind) + "/" + std::to_string(kind ? 0u : seed) + "/" + std::to_string(afr);
        const Texture& tex = baked(mArt, r, key, 110, 140, 55.0f, 90.0f,
          [&](cairo_t* cr) { paintAudienceMember(cr, 55, 90, 34, kind, afr, kind ? 99u : seed); });
        DrawOpts ao;
        ao.tint = tint;
        ao.alpha = studio;
        const float hop = lit > 0.5f ? std::abs(std::sin(float(frame + k * 11 + tier * 5) * 0.2f)) * 6.0f : 0.0f;
        r.draw(tex, px, rowY - hop, ao);
      }
    }
  }

  if (arenaVisible && revealing && b.wallX0 >= 0)
  {
    // The east wall: a crack runs up it (45-90), then the painted flat tips
    // over backwards and lands flat (90-115), and dust rolls out.
    const float wx = float(b.wallX0) * kTilePx - camX, wy = float(b.wallY0) * kTilePx - camY;
    const int wwpx = (b.wallX1 - b.wallX0 + 1) * int(kTilePx), whpx = (b.wallY1 - b.wallY0 + 1) * int(kTilePx);
    const float floorY = float(b.floorRow) * kTilePx - camY;
    if (rv >= 90)
    {
      // Cover the wall's own blocks with the dark behind the set.
      r.fillRect(wx, wy, float(wwpx), floorY - wy, rgb(10, 10, 22));
      drawGlow(r, mArt, wx + float(wwpx) * 0.5f, floorY - 200.0f, 260.0f, rgb(255, 240, 200), 0.25f * ease(float(rv - 95) / 25.0f));
      const float t = std::clamp(float(rv - 90) / 25.0f, 0.0f, 1.0f);
      const float tip = t * t; // falls faster and faster
      const float fall = std::cos(tip * 1.5707963f);
      const float shownH = (floorY - wy) * fall;
      if (shownH > 6.0f)
      {
        const Texture& flat = baked(mArt, r, "flat/" + std::to_string(wwpx) + "x" + std::to_string(whpx), wwpx, whpx, 0.0f, 0.0f,
          [&](cairo_t* cr) { paintFlat(cr, double(wwpx), double(whpx)); });
        const float shade = 1.0f - 0.6f * tip;
        stretch(r, flat, wx, floorY - shownH, float(wwpx), shownH, lerpColor(rgb(0, 0, 0), rgb(255, 255, 255), shade), 1.0f);
      }
      else
      {
        // Flat on the floor: its back, a thin plywood strip.
        r.fillRect(wx - 6.0f, floorY - 10.0f, float(wwpx) + 12.0f, 10.0f, rgb(170, 132, 84));
        r.fillRect(wx - 6.0f, floorY - 10.0f, float(wwpx) + 12.0f, 2.0f, rgb(220, 186, 130));
      }
      // Dust where it lands.
      if (rv >= 112 && rv < 160)
      {
        const float d = float(rv - 112) / 48.0f;
        for (int k = 0; k < 10; ++k)
        {
          const float dx = (float(k) - 4.5f) * 26.0f * (0.4f + d);
          const float dy = -20.0f * d - float(k % 3) * 12.0f * d;
          drawGlow(r, mArt, wx + float(wwpx) * 0.5f + dx, floorY + dy - 10.0f, 40.0f + 50.0f * d, rgb(200, 190, 170),
            0.45f * (1.0f - d));
        }
      }
    }
    else if (rv >= 45)
    {
      // The crack, climbing.
      const float p = ease(float(rv - 45) / 45.0f);
      const float x0 = wx + float(wwpx) * 0.45f;
      float px = x0, py = floorY;
      const float top = floorY - (floorY - wy) * p;
      int i = 0;
      while (py > top)
      {
        const float ny = std::max(top, py - 26.0f);
        const float nx = x0 + (frac(hash2(i, 21)) - 0.5f) * float(wwpx) * 0.7f;
        r.drawLine(px, py, nx, ny, 5.0f, rgb(4, 2, 6));
        r.drawLine(px, py, nx, ny, 1.5f, rgba(255, 240, 210, 200), Blend::Add);
        if (i % 3 == 1)
          r.drawLine(nx, ny, nx + (frac(hash2(i, 5)) - 0.5f) * 40.0f, ny + 14.0f, 2.0f, rgb(4, 2, 6));
        px = nx;
        py = ny;
        ++i;
      }
      drawGlow(r, mArt, px, py, 30.0f, rgb(255, 240, 210), 0.6f);
    }
  }

  if (arenaVisible && revealing && rv >= 90)
  {
    // The studio lights: three banks along the top that clunk on one by one,
    // throwing cones of light down onto the set.
    for (int k = 0; k < 3; ++k)
    {
      const float cx = ax0 + aw * (float(k) + 0.5f) / 3.0f, ty = ay0 + 6.0f;
      const bool on = b.lights > k;
      r.draw(on ? baked(mArt, r, "bank/on", 360, 90, 180.0f, 0.0f, [](cairo_t* cr) { paintLightBank(cr, true); })
                : baked(mArt, r, "bank/off", 360, 90, 180.0f, 0.0f, [](cairo_t* cr) { paintLightBank(cr, false); }),
        cx, ty);
      if (on)
      {
        const Texture& cone = baked(mArt, r, "cone", 320, 640, 160.0f, 0.0f, paintCone);
        DrawOpts co;
        co.blend = Blend::Add;
        co.alpha = 0.32f;
        co.scale = 1.4f;
        for (int j = 0; j < 4; ++j)
        {
          const float lx = cx - 135.0f + float(j) * 90.0f;
          r.draw(cone, lx, ty + 78.0f, co);
          drawGlow(r, mArt, lx, ty + 76.0f, 40.0f, rgb(255, 250, 220), 0.9f);
        }
      }
    }
  }

  // The floor segments over the grille.
  const bool cycleLive = b.phase == ZeroPhase::Overload && b.cycle < 60;
  for (std::size_t i = 0; i < z.doors.size(); ++i)
  {
    const auto& d = z.doors[i];
    if (!d.segment || d.layer < 0 || d.layer >= int(mLayers.size()) || d.pos <= 0.02f)
      continue;
    const Layer& l = mLayers[std::size_t(d.layer)];
    float x = float(l.x0) * kTilePx - camX, y = float(l.y0) * kTilePx - camY;
    const int wpx = (l.x1 - l.x0 + 1) * int(kTilePx);
    if (!visible(x, y, float(wpx), 200.0f))
      continue;
    int k = 0;
    if (l.id.size() > 3)
      k = std::atoi(l.id.c_str() + 3) - 1;
    const bool safe = cycleLive && k >= 0 && k < 16 && ((b.safe >> k) & 1u);
    if (d.shake > 0)
    {
      x += float(int(hash2(int(i), frame) % 7u) - 3);
      y += float(int(hash2(int(i) + 5, frame) % 5u) - 2);
    }
    const float drop = (1.0f - d.pos);
    y += drop * drop * 220.0f;
    const Texture& tex = baked(mArt, r, "seg/" + std::to_string(wpx) + "/" + std::to_string(k + 1), wpx, 64, 0.0f, 0.0f,
      [&](cairo_t* cr) { paintSegment(cr, double(wpx), k + 1); });
    DrawOpts so;
    so.alpha = std::clamp(d.pos * 1.2f, 0.0f, 1.0f);
    if (drop > 0.0f)
      so.angle = (k % 2 ? 1.0f : -1.0f) * drop * 14.0f;
    r.draw(tex, x, y, so);
    // The light channel: green on the safe ones, red flickering on the
    // ones about to drop, dim otherwise.
    Color lc = rgb(80, 30, 34);
    float glowA = 0.0f;
    if (safe)
    {
      lc = kGreen;
      glowA = 0.45f;
    }
    else if (b.phase == ZeroPhase::Overload && b.cycle >= 45 && b.cycle < 60)
    {
      const bool f = hash2(int(i), frame / 2) % 3u != 0u;
      lc = f ? kRed : rgb(120, 20, 26);
      glowA = f ? 0.4f : 0.1f;
    }
    else if (b.phase == ZeroPhase::Racks || b.phase == ZeroPhase::Echoes)
      lc = rgb(150, 30, 40);
    if (drop <= 0.0f)
    {
      r.fillRect(x + 12.0f, y + 24.0f, float(wpx) - 24.0f, 8.0f, lc);
      if (glowA > 0.0f)
        for (float gx = x + 40.0f; gx < x + float(wpx); gx += 80.0f)
          drawGlow(r, mArt, gx, y + 28.0f, 70.0f, lc, glowA);
    }
  }

  // The eye.
  const float ecx = b.cx * kCellPx - camX, ecy = b.cy * kCellPx - camY;
  const float erad = float(b.w) * kTilePx * 0.5f;
  if (!visible(ecx - erad * 3.0f, ecy - erad * 3.0f, erad * 6.0f, erad * 6.0f))
    return;
  float light = 1.0f;
  switch (b.phase)
  {
    case ZeroPhase::Idle:
      light = 0.7f + 0.05f * std::sin(float(frame) * 0.05f);
      break;
    case ZeroPhase::Dying:
      light = std::max(0.15f, 1.0f - float(b.t) / 50.0f) * (hash2(b.t, 3) % 5u == 0u ? 0.5f : 1.0f);
      break;
    case ZeroPhase::Reveal:
    case ZeroPhase::Done:
      light = rv < 45 ? 0.15f * (1.0f - float(rv) / 45.0f) : 0.0f;
      break;
    default:
      break;
  }
  if (b.reveal >= 0)
    light = std::min(light, rv < 45 ? 0.15f * (1.0f - float(rv) / 45.0f) : 0.0f);
  float pupil = 0.34f + 0.03f * std::sin(float(frame) * 0.04f);
  if (b.iris > 0)
    pupil = 0.15f;
  const bool charging = b.phase == ZeroPhase::Overload && b.cycle >= 60 && b.cycle < 82;
  if (charging)
    pupil = 0.34f + 0.2f * float(b.cycle - 60) / 22.0f;
  // The shutter: it closes over 10 frames as the phase starts, opens over
  // 10 when a wave is down and closes again in the window's last 10.
  float shutterShown = 0.0f;
  if (b.phase == ZeroPhase::Echoes)
  {
    if (b.shutter)
      shutterShown = std::min(1.0f, float(b.t) / 10.0f);
    else if (b.open > 140)
      shutterShown = float(b.open - 140) / 10.0f;
    else if (b.open < 10)
      shutterShown = 1.0f - float(b.open) / 10.0f;
  }
  const int pq = std::clamp(int(std::lround(pupil * 40.0f)), 4, 26);
  const int sq = std::clamp(int(std::lround(shutterShown * 10.0f)), 0, 10);
  const int lq = std::clamp(int(std::lround(light * 10.0f)), 0, 10);
  const int texSize = int(erad * 2.0f) + 8;
  const float half = float(texSize) * 0.5f;

  // Phase 1's racks: ten slots round the eye, two of them gaps, each rack
  // long side along the ring; they slide back out to the walls as they
  // retract.
  if (b.racks > 0.01f && (b.phase == ZeroPhase::Racks || b.phase == ZeroPhase::Retract || b.phase == ZeroPhase::Idle))
  {
    const float out = 1.0f - std::clamp(b.racks, 0.0f, 1.0f);
    const float rad = 9.0f * kCellPx + out * 22.0f * kCellPx;
    // The ring's track and the cables to the eye.
    if (out < 0.05f)
      for (int k = 0; k < 60; k += 2)
      {
        const float a0 = float(k) * 6.2831853f / 60.0f, a1 = float(k + 1) * 6.2831853f / 60.0f;
        r.drawLine(ecx + std::cos(a0) * rad, ecy + std::sin(a0) * rad, ecx + std::cos(a1) * rad, ecy + std::sin(a1) * rad, 2.0f,
          rgba(255, 60, 70, 60));
      }
    for (int k = 0; k < 10; ++k)
    {
      if (k == 0 || k == 5)
        continue;
      // Centred in its slot's sector (world_zero.cpp's rackSlot), long side
      // along the ring.
      const float a = b.orbit + (float(k) + 0.5f) * 0.62831853f;
      const float rx = ecx + std::cos(a) * rad, ry = ecy + std::sin(a) * rad;
      if (out < 0.05f)
      {
        // A cable from the eye's bezel to the rack, a pulse running along it.
        const float ex = ecx + std::cos(a) * erad * 0.95f, ey = ecy + std::sin(a) * erad * 0.95f;
        r.drawLine(ex, ey, rx - std::cos(a) * 62.0f, ry - std::sin(a) * 62.0f, 5.0f, rgb(20, 18, 24));
        const float t = std::fmod(float(frame) * 0.04f + float(k) * 0.3f, 1.0f);
        const float px = ex + (rx - std::cos(a) * 62.0f - ex) * t, py = ey + (ry - std::sin(a) * 62.0f - ey) * t;
        r.fillRect(px - 3.0f, py - 3.0f, 6.0f, 6.0f, kRed, Blend::Add);
      }
      const Texture& rack = baked(mArt, r, "orbit/" + std::to_string(k), 128, 168, 64.0f, 84.0f,
        [k](cairo_t* cr) { paintServerRack(cr, 128, 168, unsigned(k * 17 + 3), true); });
      DrawOpts ro;
      ro.angle = a * 57.29578f;
      ro.alpha = std::clamp(b.racks * 1.5f, 0.0f, 1.0f);
      r.draw(rack, rx, ry, ro);
      // A red light on its outer end.
      drawGlow(r, mArt, rx + std::cos(a) * 20.0f, ry + std::sin(a) * 20.0f, 24.0f, kRed, (frame / 8 + k) % 3 ? 0.25f : 0.6f);
    }
  }

  // Charging: the red glow swelling round it.
  if (charging)
  {
    const float c = float(b.cycle - 60) / 22.0f;
    drawGlow(r, mArt, ecx, ecy, erad * (1.2f + 1.3f * c), kRed, 0.25f + 0.5f * c);
  }
  else if (light > 0.3f)
    drawGlow(r, mArt, ecx, ecy, erad * 1.5f, kRed, 0.18f * light);
  const Texture& eye = baked(mArt, r,
    "eye/" + std::to_string(texSize) + "/" + std::to_string(pq) + "/" + std::to_string(sq) + "/" + std::to_string(lq), texSize, texSize,
    half, half, [&](cairo_t* cr) {
      paintZeroEye(cr, double(half), double(half), double(erad), double(pq) / 40.0, double(sq) / 10.0, double(lq) / 10.0, 0.0);
    });
  r.draw(eye, ecx, ecy);
  // The zoom ring turns as it focuses: while it fans, tells and charges.
  {
    const Texture& ringTex = baked(mArt, r, "ring/" + std::to_string(texSize), texSize, texSize, half, half,
      [&](cairo_t* cr) { paintZoomRing(cr, double(half), double(erad)); });
    DrawOpts ro;
    ro.angle = float(b.fan) * 0.6f + (b.iris > 0 ? float(12 - b.iris) * 2.0f : 0.0f) + (charging ? float(b.cycle - 60) * 4.0f : 0.0f);
    ro.alpha = 0.92f;
    r.draw(ringTex, ecx, ecy, ro);
  }
  // The tally light: red while it is on air.
  if (light > 0.3f && (frame / 30) % 4 != 3)
    drawGlow(r, mArt, ecx + erad * 0.78f, ecy - erad * 0.64f, 26.0f, kRed, 0.9f);
  if (charging)
  {
    const float c = float(b.cycle - 60) / 22.0f;
    drawGlow(r, mArt, ecx, ecy, erad * 0.5f * (0.5f + c), rgb(255, 230, 220), 0.4f + 0.5f * c);
  }
  // The beam: a column of red light straight down from the eye into the
  // pit, where it runs along the grille under every gap in the floor.
  if (b.beam > 0)
  {
    // Into the gap nearest under it.
    float gx = ecx, best = 1e9f;
    for (const auto& d : z.doors)
    {
      if (!d.segment || d.pos > 0.5f || d.layer < 0)
        continue;
      const Layer& sl = mLayers[std::size_t(d.layer)];
      const float mx = (float(sl.x0) + float(sl.x1 - sl.x0 + 1) * 0.5f) * kTilePx - camX;
      if (std::abs(mx - ecx) < best)
      {
        best = std::abs(mx - ecx);
        gx = mx;
      }
    }
    const float gy = float(b.floorRow + 2) * kTilePx - camY + 8.0f;
    const float top = ecy + erad * 0.3f;
    const float wob = 1.0f + 0.15f * std::sin(float(frame) * 1.3f);
    r.drawLine(ecx, top, gx, gy, 46.0f * wob, rgba(255, 40, 50, 90), Blend::Add);
    r.drawLine(ecx, top, gx, gy, 22.0f * wob, rgba(255, 90, 100, 170), Blend::Add);
    r.drawLine(ecx, top, gx, gy, 8.0f, rgba(255, 235, 230, 240), Blend::Add);
    for (int k = 0; k < 3; ++k)
      zap(r, ecx, top + 40.0f, gx + (frac(hash2(k, frame / 2)) - 0.5f) * 90.0f, gy, int(hash2(k + 7, frame / 2)),
        rgb(255, 80, 90), 4.0f, 7);
    drawGlow(r, mArt, ecx, top, 90.0f, rgb(255, 200, 190), 0.8f);
    drawGlow(r, mArt, gx, gy, 120.0f, kRed, 0.7f);
  }
  if (b.flash > 0)
  {
    DrawOpts fo;
    fo.blend = Blend::Add;
    fo.alpha = std::min(1.0f, float(b.flash) / 8.0f);
    r.draw(eye, ecx, ecy, fo);
    drawGlow(r, mArt, ecx, ecy, erad * 1.2f, rgb(255, 255, 255), 0.5f * fo.alpha);
  }
  if (b.glance > 0)
  {
    // A shot glancing off the bezel, on the side facing the runner.
    const float px = (float(mPlayer.x) + 1.5f) * kCellPx - camX, py = (float(mPlayer.y) - 2.0f) * kCellPx - camY;
    const float a = std::atan2(py - ecy, px - ecx);
    const float gx = ecx + std::cos(a) * erad * 0.9f, gy = ecy + std::sin(a) * erad * 0.9f;
    sparkle(r, mArt, gx, gy, 18.0f + float(b.glance) * 2.0f, rgb(255, 230, 160), std::min(1.0f, float(b.glance) / 6.0f));
    for (int k = 0; k < 4; ++k)
    {
      const float sa = a + (frac(hash2(k, b.glance)) - 0.5f) * 2.0f;
      r.drawLine(gx, gy, gx + std::cos(sa) * 30.0f, gy + std::sin(sa) * 30.0f, 2.0f, rgba(255, 220, 140, 220), Blend::Add);
    }
  }

  // Lance Marquee strides on and throws his arms wide.
  if (revealing && rv >= 130)
  {
    const float exitX = (float(mLevel->exitTx) + 0.5f) * kTilePx - camX;
    const float floorY = float(b.floorRow) * kTilePx - camY;
    const float startX = b.wallX0 >= 0 ? float(b.wallX1 + 1) * kTilePx - camX + 40.0f : exitX + 400.0f;
    const float standX = exitX - 3.0f * kTilePx;
    const float t = std::clamp(float(rv - 130) / 42.0f, 0.0f, 1.0f);
    const float lx = startX + (standX - startX) * t;
    const bool arrived = rv >= 172 || b.phase == ZeroPhase::Done;
    const int pose = arrived ? 8 : (frame / 5) % 8;
    const bool mouth = arrived && (frame / 7) % 2 == 0;
    const Texture& lance = baked(mArt, r, "lance/" + std::to_string(pose) + (mouth ? "o" : "c"), 130, 250, 60.0f, 240.0f,
      [&](cairo_t* cr) {
        cairo_translate(cr, 0, 0);
        cairo_scale(cr, 0.5, 0.5);
        paintLance(cr, pose, mouth);
      });
    // He walks in from the right, so his art (facing right) is flipped
    // while he walks; then he turns to the audience.
    drawGlow(r, mArt, lx, floorY - 100.0f, 150.0f, rgb(255, 240, 200), 0.25f);
    DrawOpts lo;
    if (!arrived)
    {
      // Facing left: draw the mirrored copy.
      const Texture& mirrored = baked(mArt, r, "lance/m" + std::to_string(pose), 130, 250, 70.0f, 240.0f, [&](cairo_t* cr) {
        cairo_translate(cr, 130, 0);
        cairo_scale(cr, -0.5, 0.5);
        paintLance(cr, pose, false);
      });
      r.draw(mirrored, lx, floorY, lo);
    }
    else
      r.draw(lance, lx, floorY, lo);
  }
}

// --- The front pass -----------------------------------------------------------------------

void World::drawZeroFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& z = mZero;
  const auto& b = z.boss;
  if (z.wireframe)
    return;
  const float px = float(mPlayer.prevX) + float(mPlayer.x - mPlayer.prevX) * alpha, py = float(mPlayer.prevY) + float(mPlayer.y - mPlayer.prevY) * alpha;

  // Laser grids: red beams across, on for half of every cycle.
  for (const auto& g : z.grids)
  {
    const float x0 = float(g.x0) * kTilePx - camX, x1 = float(g.x1 + 1) * kTilePx - camX;
    const float y0 = float(g.y0) * kTilePx - camY, y1 = float(g.y1 + 1) * kTilePx - camY;
    if (!visible(x0, y0, x1 - x0, y1 - y0))
      continue;
    const bool on = (mStats.frames % g.every) < g.every / 2;
    for (float yy = y0 + 16.0f; yy < y1; yy += 32.0f)
    {
      if (on)
      {
        const float wob = std::sin(float(frame) * 0.9f + yy) * 0.8f;
        r.drawLine(x0, yy + wob, x1, yy - wob, 6.0f, rgba(255, 40, 50, 120), Blend::Add);
        r.drawLine(x0, yy, x1, yy, 2.0f, rgba(255, 200, 200, 240), Blend::Add);
      }
      else
        for (float sx = x0; sx < x1; sx += 16.0f)
          r.fillRect(sx, yy - 1.0f, 6.0f, 2.0f, rgba(255, 60, 70, 60));
    }
    if (on)
      for (float yy = y0 + 16.0f; yy < y1; yy += 64.0f)
      {
        drawGlow(r, mArt, x0, yy, 22.0f, kRed, 0.7f);
        drawGlow(r, mArt, x1, yy, 22.0f, kRed, 0.7f);
      }
  }

  // RACK 42's front: a rack like any other, a fake wall you can walk
  // through; see-through while you are inside.
  for (const auto& d : z.decos)
  {
    const float x = float(d.x0) * kTilePx - camX, y = float(d.y0) * kTilePx - camY;
    const float w = float(d.x1 - d.x0 + 1) * kTilePx, h = float(d.y1 - d.y0 + 1) * kTilePx;
    if (!visible(x, y, w, h))
      continue;
    if (d.kind == "rack42" || d.kind == "rack")
    {
      const bool inside = px + 3.0f > float(d.x0 * kCellsPerTile) && px < float((d.x1 + 1) * kCellsPerTile) &&
        py >= float(d.y0 * kCellsPerTile) && py - 5.0f < float((d.y1 + 1) * kCellsPerTile);
      const int hpx = int(h);
      const Texture& front = baked(mArt, r, "rackfront/" + std::to_string(hpx) + (d.kind == "rack42" ? "/42" : ""), 64, hpx, 0.0f,
        0.0f, [&](cairo_t* cr) { paintServerRack(cr, 64, double(hpx), 42u, true, d.kind == "rack42" ? "42" : ""); });
      DrawOpts fo;
      fo.alpha = inside ? 0.35f : 1.0f;
      if (d.kind == "rack42")
      {
        // Its roof and back are blocks; the front is this rack face.
        r.draw(front, x, y, fo);
        r.drawText("RACK 42", x + w * 0.5f, y - 22.0f, {14.0f, rgb(255, 200, 200), rgb(30, 0, 0)}, Align::Center, 0.8f);
      }
      else
        for (float sx = x; sx < x + w - 1.0f; sx += 64.0f)
          r.draw(front, sx, y, fo);
    }
    else if (d.kind == "sign" && !d.text.empty())
    {
      r.fillRect(x, y + 10.0f, w, 34.0f, rgba(20, 6, 10, 220));
      r.fillRect(x, y + 10.0f, w, 2.0f, kRed);
      r.drawText(d.text, x + w * 0.5f, y + 16.0f, {18.0f, rgb(255, 210, 210), rgb(0, 0, 0)}, Align::Center);
    }
    if (d.glint > 0)
    {
      // A phase shot went through: a glint sweeps across it.
      const float t = 1.0f - float(d.glint) / float(kGlintFrames);
      const float gx = x + w * t, a = std::sin(t * 3.14159f);
      r.drawLine(gx - 20.0f, y + h, gx + 20.0f, y, 10.0f, rgba(255, 255, 255, int(110 * a)), Blend::Add);
      sparkle(r, mArt, x + w * 0.5f, y + h * 0.5f, 22.0f, kViolet, a);
      sparkle(r, mArt, gx, y + h * 0.3f, 12.0f, rgb(255, 255, 255), a);
    }
  }

  // Wrecks: sparking, and a ring filling while a swarm rebuilds one.
  for (const auto& w : z.wrecks)
  {
    const float cx = float(w.x) * kCellPx + 16.0f - camX, cy = float(w.y) * kCellPx + 16.0f - camY;
    if (!visible(cx - 80.0f, cy - 80.0f, 160.0f, 160.0f))
      continue;
    if (w.turret)
    {
      // The twisted lattice hanging off its rail.
      r.drawLine(cx - 40.0f, cy - 40.0f, cx - 10.0f, cy + 6.0f, 6.0f, rgb(40, 42, 52));
      r.drawLine(cx + 36.0f, cy - 40.0f, cx + 14.0f, cy - 4.0f, 6.0f, rgb(40, 42, 52));
      r.drawLine(cx - 10.0f, cy + 6.0f, cx + 8.0f, cy + 20.0f, 9.0f, rgb(60, 62, 74));
      drawGlow(r, mArt, cx, cy + 10.0f, 30.0f, rgb(255, 120, 40), 0.3f);
    }
    if (hash2(int(cx + camX), frame / 3) % 3u == 0u)
    {
      const std::uint32_t h = hash2(w.x, frame);
      sparkle(r, mArt, cx + (frac(h) - 0.5f) * 40.0f, cy + (frac(h >> 9) - 0.5f) * 30.0f, 10.0f, rgb(255, 210, 120), 0.9f);
    }
    if (w.tended > 0)
    {
      const float p = std::clamp(float(w.t) / float(kRebuildFrames), 0.0f, 1.0f);
      const float rr = 52.0f;
      const int segs = 40;
      for (int k = 0; k < segs; ++k)
      {
        const float a0 = -1.5707963f + float(k) * 6.2831853f / float(segs), a1 = a0 + 6.2831853f / float(segs);
        const bool filled = float(k) / float(segs) < p;
        r.drawLine(cx + std::cos(a0) * rr, cy + std::sin(a0) * rr, cx + std::cos(a1) * rr, cy + std::sin(a1) * rr, filled ? 6.0f : 3.0f,
          filled ? rgba(120, 230, 255, 230) : rgba(60, 80, 100, 140), filled ? Blend::Add : Blend::Alpha);
      }
      drawGlow(r, mArt, cx, cy, 70.0f, kCyan, 0.15f + 0.2f * p);
      r.drawText(std::to_string(int(p * 100.0f)) + "%", cx, cy - 10.0f, {16.0f, rgb(170, 240, 255), rgb(0, 10, 20)}, Align::Center);
    }
  }

  // Echoes: holograms of a runner replaying the runner's moves; where one
  // is about to fire, a ghost of it flickers ahead of time.
  for (std::size_t i = 0; i < mEnemies.size(); ++i)
  {
    const auto& e = mEnemies[i];
    if (!e.alive || e.kind != EnemyKind::Echo || e.hidden)
      continue;
    const CharacterArt& ca = echoArt(mArt, mCharacter, e.aimX);
    const float ex = (e.drawX + 1.5f) * kCellPx - camX, ey = (e.drawY + 1.0f) * kCellPx - camY;
    if (!visible(ex - 100.0f, ey - 200.0f, 200.0f, 220.0f))
      continue;
    const EchoFrame& f = z.ago(std::max(0, e.attach));
    const int facing = f.facing < 0 ? -1 : 1;
    hologram(r, mArt, poseSprite(ca, PlayerVisual(f.visual), frame), facing, ex, ey, 1.0f, int(i) * 31 + 7, frame);
    if (e.flash > 0)
    {
      DrawOpts fo;
      fo.blend = Blend::Add;
      fo.alpha = float(e.flash) / 8.0f;
      r.draw(poseSprite(ca, PlayerVisual(f.visual), frame).get(facing), ex, ey, fo);
    }
    if (e.attach - kEchoTell >= 0)
    {
      const EchoFrame& g = z.ago(e.attach - kEchoTell);
      if ((g.shotDx != 0 || g.shotDy != 0) && (frame / 2) % 2 == 0)
      {
        const float gx = (float(g.x) + 1.5f) * kCellPx - camX, gy = (float(g.y) + 1.0f) * kCellPx - camY;
        hologram(r, mArt, poseSprite(ca, PlayerVisual(g.visual), frame), g.facing < 0 ? -1 : 1, gx, gy, 0.35f, int(i) * 31 + 9, frame);
        const float mx = (float(g.x + g.shotOx) + 0.5f) * kCellPx - camX, my = (float(g.y + g.shotOy) + 0.5f) * kCellPx - camY;
        drawGlow(r, mArt, mx, my, 30.0f, kCyan, 0.8f);
        const float len = std::max(1.0f, std::sqrt(float(g.shotDx * g.shotDx + g.shotDy * g.shotDy)));
        for (int k = 1; k <= 4; ++k)
        {
          const float t = float(k) * 30.0f;
          r.fillRect(mx + float(g.shotDx) / len * t - 3.0f, my + float(g.shotDy) / len * t - 3.0f, 6.0f, 6.0f,
            rgba(150, 250, 255, 200 - k * 40), Blend::Add);
        }
      }
    }
  }

  // Terminals' screens, read out; ZERO's joke at its public terminal.
  for (const auto& t : z.terminals)
  {
    const float cx = float(t.x) * kTilePx + 32.0f - camX, fy = float(t.y + 1) * kTilePx - camY;
    if (!visible(cx - 300.0f, fy - 300.0f, 600.0f, 300.0f))
      continue;
    if (t.shown > 0)
      bubble(r, cx, fy - 100.0f, {t.text}, rgb(110, 255, 150), rgb(200, 255, 210), std::min(1.0f, float(t.shown) / 8.0f));
    if (t.code && z.joke > 0)
      bubble(r, cx, fy - (t.shown > 0 ? 170.0f : 100.0f),
        {"ZERO: WHY DON'T AIS WATCH TELEVISION?", "TOO MANY RERUNS OF THE SAME LOOP."}, kRed, rgb(255, 220, 220),
        std::min(1.0f, float(z.joke) / 8.0f));
  }

  // The stage door's sign once the exit is open.
  if (b.on && b.exitOpen)
  {
    const float cx = (float(mLevel->exitTx) + 0.5f) * kTilePx - camX;
    const float top = float(mLevel->exitTy + 1) * kTilePx - 6.0f * kCellPx - camY;
    if (visible(cx - 100.0f, top - 80.0f, 200.0f, 100.0f))
    {
      r.fillRect(cx - 86.0f, top - 64.0f, 172.0f, 40.0f, rgb(20, 16, 30));
      r.fillRect(cx - 82.0f, top - 60.0f, 164.0f, 32.0f, rgb(240, 230, 200));
      r.drawText("TO SET B", cx, top - 56.0f, {22.0f, rgb(30, 24, 20), 0}, Align::Center);
      if ((frame / 20) % 2 == 0)
        drawGlow(r, mArt, cx, top - 44.0f, 100.0f, rgb(255, 240, 200), 0.25f);
      r.drawLine(cx - 60.0f, top - 24.0f, cx - 60.0f, top, 2.0f, rgb(60, 60, 70));
      r.drawLine(cx + 60.0f, top - 24.0f, cx + 60.0f, top, 2.0f, rgb(60, 60, 70));
    }
  }
}

// --- The HUD ------------------------------------------------------------------------------

void World::drawZeroHud(Renderer& r, int frame) const
{
  if (!zeroFight())
    return;
  const auto& b = mZero.boss;
  const float bw = 600.0f, bx = (float(kScreenW) - bw) * 0.5f, by = 158.0f; // under the HUD and its messages: the floor segments fill the bottom of the view
  const float total = 90.0f;
  r.fillRect(bx - 10, by - 26, bw + 20, 42, rgba(8, 6, 12, 200));
  r.drawText("ZERO", bx, by - 23, {15.0f, kRed, rgb(30, 4, 6)});
  const char* phase = b.phase == ZeroPhase::Racks ? "THE RACKS"
    : b.phase == ZeroPhase::Echoes                ? (b.shutter ? "SHUTTERED" : "OPEN")
    : b.phase == ZeroPhase::Overload              ? "OVERLOAD"
                                                  : "";
  r.drawText(phase, bx + bw, by - 23, {13.0f, rgb(255, 170, 170), rgb(30, 4, 6)}, Align::Right);
  r.fillRect(bx, by, bw, 10, rgba(255, 255, 255, 40));
  const float fill = std::clamp(float(zeroHp()) / total, 0.0f, 1.0f);
  const bool shut = b.phase == ZeroPhase::Echoes && b.shutter;
  r.fillRect(bx, by, bw * fill, 10,
    (b.flash > 0 && (frame / 2) % 2) ? rgb(255, 255, 255) : (shut ? rgb(140, 140, 160) : kRed));
  for (const float cut : {30.0f / total, 60.0f / total})
    r.fillRect(bx + bw * cut - 1.0f, by - 2, 3, 14, rgb(20, 16, 30));
}

} // namespace gr
