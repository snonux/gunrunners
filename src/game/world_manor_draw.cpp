// Level 23, Fright Night Manor: drawing. See world_manor.cpp for the logic.
//
// Behind everything (drawManorWall, before the tiles): the inside walls,
// chosen by where a block sits: damask wallpaper over oak wainscot in the
// rooms (three papers, purple in the real manor and green in its
// reflection), dressed stone up the stair hall, rubble in the cellar, bare
// boards in the attic, lancet windows onto the storm, and the library's
// bookcases.
//
// Over the tiles (drawManorBack): the reflection's green cast, the walls
// and floors that are there on one side only (stone or oak on their side,
// a faint ghost outline on the other), the ballroom doors, the front door,
// chandeliers, mirrors (gilt, broken or the crypt's; the glass shows the
// other side, the runner's reflection, the shimmer of a step through and
// the backwards reflection), levers, the Portrait Ghosts' portraits with
// their sconces, Lance's portrait, the coffin and its sign, ghost
// platforms and the refill box coming back. In front (drawManorFront): the
// shimmer of a step through a mirror and the lightning. drawManorHud: the
// side and what Up does here. drawSplit: Both Sides.

#include "game/world.hpp"

#include "assets/art.hpp"
#include "assets/enemy_art.hpp"
#include "assets/enemy_art_manor.hpp"
#include "assets/enemy_art_zero.hpp"
#include "base/math.hpp"
#include "render/vector.hpp"

#include <SDL.h>

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
constexpr float kTilePx = float(kTileSize) * kPixelScale; // 64
constexpr Color kInk = rgb(16, 12, 24);
constexpr Color kGilt = rgb(214, 172, 80);
constexpr Color kGiltDark = rgb(110, 76, 26);
constexpr Color kGreenGlow = rgb(120, 255, 150);
constexpr Color kPurpleGlow = rgb(190, 130, 255);

Color lighten(Color c, float t) { return lerpColor(c, rgb(255, 255, 255), t); }
Color darken(Color c, float t) { return lerpColor(c, rgb(0, 0, 0), t); }

float lerpCells(int prev, int cur, float alpha)
{
  return (float(prev) + float(cur - prev) * alpha) * kCellPx;
}

bool visible(float x, float y, float w, float h)
{
  return x + w > -96.0f && x < float(kScreenW) + 96.0f && y + h > -96.0f && y < float(kScreenH) + 96.0f;
}

// A texture baked once and kept in the Art's sprite cache under `key`.
const Texture& baked(const Art& art, const Renderer& r, const std::string& key, int w, int h, float ax, float ay,
  const std::function<void(cairo_t*)>& paint)
{
  const std::string id = "~manor/" + key;
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

void circle(cairo_t* cr, double x, double y, double rad)
{
  cairo_new_sub_path(cr);
  cairo_arc(cr, x, y, rad, 0, 2 * kPi);
}

std::uint32_t xs(std::uint32_t& s)
{
  s ^= s << 13;
  s ^= s >> 17;
  s ^= s << 5;
  return s;
}

double rnd(std::uint32_t& s) { return double(xs(s) & 0xFFFF) / 65535.0; }

void clipTo(Renderer& r, float x, float y, float w, float h)
{
  const SDL_Rect rc{int(std::floor(x)), int(std::floor(y)), std::max(0, int(std::ceil(w))), std::max(0, int(std::ceil(h)))};
  SDL_RenderSetClipRect(r.sdl(), &rc);
}

void unclip(Renderer& r) { SDL_RenderSetClipRect(r.sdl(), nullptr); }

// The whole view multiplied by a colour (the reflection's green cast).
void modulate(Renderer& r, Color c)
{
  SDL_Renderer* sdl = r.sdl();
  SDL_SetRenderDrawBlendMode(sdl, SDL_BLENDMODE_MOD);
  SDL_SetRenderDrawColor(sdl, Uint8(redOf(c)), Uint8(greenOf(c)), Uint8(blueOf(c)), 255);
  SDL_RenderFillRect(sdl, nullptr);
  SDL_SetRenderDrawBlendMode(sdl, SDL_BLENDMODE_BLEND);
}

// --- Wall paintings ------------------------------------------------------------------

// The papers: 0 damask (the ground floor), 1 stripes (the galleries), 2 a
// gilt trellis (the ballroom). side 1: the reflection's green.
struct Paper
{
  Color base, motif, shade;
};

Paper paperFor(int pattern, int side)
{
  static const Paper kReal[3] = {{rgb(66, 38, 88), rgb(112, 76, 140), rgb(40, 22, 54)},
                                 {rgb(80, 34, 58), rgb(118, 62, 88), rgb(50, 20, 36)},
                                 {rgb(96, 24, 38), rgb(196, 146, 74), rgb(60, 14, 24)}};
  static const Paper kMirror[3] = {{rgb(26, 64, 48), rgb(74, 146, 100), rgb(14, 40, 30)},
                                   {rgb(22, 60, 62), rgb(62, 124, 116), rgb(12, 36, 38)},
                                   {rgb(26, 76, 36), rgb(168, 214, 116), rgb(14, 46, 22)}};
  const int k = pattern < 0 ? 0 : (pattern > 2 ? 2 : pattern);
  return side == kSideMirror ? kMirror[k] : kReal[k];
}

// A damask medallion round (cx, cy): four leaves round a bud.
void damask(cairo_t* cr, double cx, double cy, double s, Color c, double a)
{
  for (int k = 0; k < 4; ++k)
  {
    const double ang = k * kPi * 0.5;
    cairo_save(cr);
    cairo_translate(cr, cx, cy);
    cairo_rotate(cr, ang);
    cairo_move_to(cr, 0, -2 * s);
    cairo_curve_to(cr, 5 * s, -6 * s, 6 * s, -11 * s, 0, -15 * s);
    cairo_curve_to(cr, -6 * s, -11 * s, -5 * s, -6 * s, 0, -2 * s);
    cairo_restore(cr);
  }
  circle(cr, cx, cy, 2.2 * s);
  setRgba(cr, c, a);
  cairo_fill(cr);
  for (int k = 0; k < 4; ++k)
  {
    const double ang = kPi * 0.25 + k * kPi * 0.5;
    circle(cr, cx + std::cos(ang) * 9 * s, cy + std::sin(ang) * 9 * s, 1.3 * s);
  }
  setRgba(cr, c, a * 0.8);
  cairo_fill(cr);
}

void paintPaper(cairo_t* cr, int pattern, int side, int variant)
{
  const Paper p = paperFor(pattern, side);
  cairo_rectangle(cr, 0, 0, 64, 64);
  linearFill(cr, 0, 0, 0, 64, lighten(p.base, 0.04f), darken(p.base, 0.06f));
  if (pattern == 0)
  {
    // Medallions in a half-drop, cut at the edges so the tiles meet.
    for (const auto& c : {std::pair<double, double>{32, 32}, {0, 0}, {64, 0}, {0, 64}, {64, 64}})
      damask(cr, c.first, c.second, 1.15, p.motif, 0.75);
    for (const auto& c : {std::pair<double, double>{0, 32}, {64, 32}, {32, 0}, {32, 64}})
    {
      circle(cr, c.first, c.second, 2.0);
      setRgba(cr, p.motif, 0.45);
      cairo_fill(cr);
    }
  }
  else if (pattern == 1)
  {
    // Broad stripes with a pinstripe and a run of small fleurons.
    for (const double x : {0.0, 32.0})
    {
      cairo_rectangle(cr, x + 4, 0, 14, 64);
      setRgba(cr, p.motif, 0.45);
      cairo_fill(cr);
      cairo_rectangle(cr, x + 24, 0, 1.4, 64);
      setRgba(cr, p.motif, 0.7);
      cairo_fill(cr);
      for (const double y : {16.0, 48.0})
        damask(cr, x + 11, y, 0.42, p.shade, 0.7);
    }
  }
  else
  {
    // A gilt trellis, a dot at each crossing.
    cairo_move_to(cr, 0, 0);
    cairo_line_to(cr, 64, 64);
    cairo_move_to(cr, 64, 0);
    cairo_line_to(cr, 0, 64);
    cairo_move_to(cr, 32, -32);
    cairo_line_to(cr, 96, 32);
    cairo_move_to(cr, -32, 32);
    cairo_line_to(cr, 32, 96);
    cairo_move_to(cr, 32, -32);
    cairo_line_to(cr, -32, 32);
    cairo_move_to(cr, 96, 32);
    cairo_line_to(cr, 32, 96);
    setRgba(cr, p.motif, 0.5);
    cairo_set_line_width(cr, 1.4);
    cairo_stroke(cr);
    for (const auto& c : {std::pair<double, double>{0, 0}, {64, 0}, {0, 64}, {64, 64}, {32, 32}})
    {
      circle(cr, c.first, c.second, 2.6);
      setRgba(cr, p.motif, 0.85);
      cairo_fill(cr);
    }
    for (const auto& c : {std::pair<double, double>{32, 0}, {0, 32}, {64, 32}, {32, 64}})
      damask(cr, c.first, c.second, 0.5, p.motif, 0.5);
  }
  // Age: a damp stain or a curling seam.
  std::uint32_t sd = std::uint32_t(pattern * 31 + variant * 7 + side * 3 + 11);
  if (variant == 1)
  {
    const double cx = 18 + rnd(sd) * 28, cy = 14 + rnd(sd) * 30;
    cairo_pattern_t* g = cairo_pattern_create_radial(cx, cy, 2, cx, cy, 20);
    cairo_pattern_add_color_stop_rgba(g, 0, 0.1, 0.08, 0.04, 0.0);
    cairo_pattern_add_color_stop_rgba(g, 0.8, 0.1, 0.08, 0.04, 0.22);
    cairo_pattern_add_color_stop_rgba(g, 1, 0.1, 0.08, 0.04, 0.0);
    cairo_set_source(cr, g);
    cairo_arc(cr, cx, cy, 20, 0, 2 * kPi);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  else if (variant == 2)
  {
    cairo_move_to(cr, 63, 30);
    cairo_line_to(cr, 52, 36);
    cairo_line_to(cr, 63, 46);
    cairo_close_path(cr);
    setRgba(cr, p.shade, 0.9);
    cairo_fill(cr);
    cairo_move_to(cr, 63, 30);
    cairo_line_to(cr, 52, 36);
    setRgba(cr, lighten(p.base, 0.3f), 0.6);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);
  }
  // The seam between drops.
  cairo_rectangle(cr, 63, 0, 1, 64);
  setRgba(cr, p.shade, 0.5);
  cairo_fill(cr);
}

// Oak wainscot, two rows high: row 0 the dado rail and the top of a raised
// panel, row 1 its bottom and the skirting board.
void paintWainscot(cairo_t* cr, int row, int side)
{
  const Color oak = side == kSideMirror ? rgb(56, 70, 50) : rgb(84, 50, 34);
  const Color oakDark = darken(oak, 0.45f), oakLight = lighten(oak, 0.25f);
  cairo_rectangle(cr, 0, 0, 64, 64);
  linearFill(cr, 0, 0, 64, 0, oak, darken(oak, 0.12f));
  // The raised panel: a bevelled field inside the stiles and rails.
  const double top = row == 0 ? 16 : -10, bottom = row == 0 ? 74 : 40;
  cairo_rectangle(cr, 8, top, 48, bottom - top);
  linearFill(cr, 8, top, 56, bottom, oakLight, darken(oak, 0.1f));
  cairo_rectangle(cr, 14, top + 6, 36, bottom - top - 12);
  linearFill(cr, 14, top, 50, bottom, oak, darken(oak, 0.2f));
  cairo_rectangle(cr, 8, top, 48, bottom - top);
  setRgba(cr, oakDark, 0.9);
  cairo_set_line_width(cr, 1.4);
  cairo_stroke(cr);
  // Grain.
  std::uint32_t sd = std::uint32_t(row * 13 + side * 5 + 7);
  for (int k = 0; k < 5; ++k)
  {
    const double gx = 16 + rnd(sd) * 32;
    cairo_move_to(cr, gx, std::max(0.0, top + 8));
    cairo_curve_to(cr, gx + 2, 24, gx - 2, 40, gx + 1, std::min(64.0, bottom - 8));
  }
  setRgba(cr, oakDark, 0.35);
  cairo_set_line_width(cr, 0.8);
  cairo_stroke(cr);
  if (row == 0)
  {
    // The dado rail.
    cairo_rectangle(cr, 0, 0, 64, 9);
    linearFill(cr, 0, 0, 0, 9, oakLight, oakDark);
    cairo_rectangle(cr, 0, 9, 64, 2);
    setRgba(cr, rgb(0, 0, 0), 0.35);
    cairo_fill(cr);
  }
  else
  {
    // The skirting board.
    cairo_rectangle(cr, 0, 46, 64, 18);
    linearFill(cr, 0, 46, 0, 64, oakLight, oakDark);
    cairo_rectangle(cr, 0, 46, 64, 2);
    setRgba(cr, rgb(255, 240, 220), 0.25);
    cairo_fill(cr);
  }
}

// Dressed stone up the stair hall: big pale blocks in courses, one with a
// carved rose now and then.
void paintHallStone(cairo_t* cr, int variant)
{
  const Color st = rgb(92, 88, 104), mortar = rgb(44, 40, 54);
  cairo_rectangle(cr, 0, 0, 64, 64);
  setColor(cr, mortar);
  cairo_fill(cr);
  for (int row = 0; row < 2; ++row)
  {
    const double off = row == 0 ? 0 : -32;
    for (double x = off; x < 64; x += 64)
    {
      cairo_rectangle(cr, x + 1.5, row * 32 + 1.5, 61, 29);
      const Color c = lerpColor(st, rgb(120, 116, 132), float(((variant + row) % 3)) * 0.15f);
      linearFill(cr, 0, row * 32, 0, row * 32 + 32, lighten(c, 0.06f), darken(c, 0.12f));
    }
  }
  if (variant == 3)
  {
    // a hairline crack running down through two courses
    cairo_move_to(cr, 38, 2);
    cairo_line_to(cr, 34, 12);
    cairo_line_to(cr, 37, 20);
    cairo_line_to(cr, 31, 31);
    cairo_line_to(cr, 33, 42);
    cairo_line_to(cr, 28, 52);
    setRgba(cr, rgb(30, 26, 38), 0.8);
    cairo_set_line_width(cr, 1.6);
    cairo_stroke(cr);
    cairo_move_to(cr, 37, 20);
    cairo_line_to(cr, 44, 25);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);
  }
  else if (variant == 1)
  {
    // a little moss in the bottom joint
    for (int k = 0; k < 7; ++k)
    {
      circle(cr, 8 + k * 7.5, 62 - (k % 2) * 1.5, 2.6);
    }
    setRgba(cr, rgb(70, 96, 58), 0.55);
    cairo_fill(cr);
  }
}

// The cellar's rubble wall: rough stones of every size in thick mortar.
void paintRubble(cairo_t* cr, int variant)
{
  cairo_rectangle(cr, 0, 0, 64, 64);
  setColor(cr, rgb(30, 28, 32));
  cairo_fill(cr);
  std::uint32_t sd = std::uint32_t(variant * 977 + 13);
  for (int k = 0; k < 9; ++k)
  {
    const double cx = (k % 3) * 22 + 10 + rnd(sd) * 6, cy = (k / 3) * 22 + 10 + rnd(sd) * 6;
    const double rx = 8 + rnd(sd) * 4, ry = 6 + rnd(sd) * 4;
    cairo_save(cr);
    cairo_translate(cr, cx, cy);
    cairo_scale(cr, rx, ry);
    cairo_new_sub_path(cr);
    cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
    cairo_restore(cr);
    const Color c = lerpColor(rgb(62, 58, 64), rgb(86, 80, 82), float(rnd(sd)));
    linearFill(cr, cx - rx, cy - ry, cx + rx, cy + ry, lighten(c, 0.1f), darken(c, 0.2f));
  }
  if (variant == 2)
  {
    // A trickle of damp.
    cairo_move_to(cr, 40, 0);
    cairo_curve_to(cr, 42, 20, 38, 40, 41, 64);
    setRgba(cr, rgb(80, 120, 90), 0.3);
    cairo_set_line_width(cr, 3);
    cairo_stroke(cr);
  }
}

// The attic: bare boards, nail heads, a knot or two.
void paintBoards(cairo_t* cr, int variant)
{
  std::uint32_t sd = std::uint32_t(variant * 331 + 5);
  for (int k = 0; k < 4; ++k)
  {
    const Color c = lerpColor(rgb(60, 44, 34), rgb(78, 58, 42), float(rnd(sd)));
    cairo_rectangle(cr, k * 16, 0, 16, 64);
    linearFill(cr, k * 16, 0, k * 16 + 16, 0, lighten(c, 0.08f), darken(c, 0.2f));
    cairo_rectangle(cr, k * 16 + 15, 0, 1, 64);
    setRgba(cr, rgb(20, 12, 8), 0.8);
    cairo_fill(cr);
    for (const double ny : {6.0, 58.0})
    {
      circle(cr, k * 16 + 8, ny, 1.1);
      setRgba(cr, rgb(30, 26, 26), 0.9);
      cairo_fill(cr);
    }
  }
  if (variant == 1)
  {
    cairo_save(cr);
    cairo_translate(cr, 24, 30);
    cairo_scale(cr, 3, 5);
    cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
    cairo_restore(cr);
    setRgba(cr, rgb(30, 20, 14), 0.8);
    cairo_fill(cr);
  }
}

// A lancet window, 2 x 3 blocks: a stone surround with a pointed arch,
// a mullion and leading over clear glass (the storm shows through), a sill,
// and tattered velvet curtains gathered at the sides.
void paintWindow(cairo_t* cr, int side)
{
  const double W = 128, H = 192;
  const Color stone = rgb(96, 92, 110);
  auto archPath = [&](double inset) {
    const double l = 16 + inset, rr = W - 16 - inset, spring = 64 + inset * 0.5;
    cairo_move_to(cr, l, H - 20);
    cairo_line_to(cr, l, spring);
    cairo_curve_to(cr, l, spring - 34, W * 0.5 - 6, 18 + inset, W * 0.5, 14 + inset);
    cairo_curve_to(cr, W * 0.5 + 6, 18 + inset, rr, spring - 34, rr, spring);
    cairo_line_to(cr, rr, H - 20);
    cairo_close_path(cr);
  };
  // The surround: everything but the opening.
  cairo_rectangle(cr, 0, 0, W, H);
  archPath(0);
  cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
  linearFill(cr, 0, 0, W, 0, lighten(stone, 0.1f), darken(stone, 0.25f));
  cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
  // Voussoirs round the arch.
  archPath(-6);
  setRgba(cr, rgb(40, 36, 50), 0.8);
  cairo_set_line_width(cr, 2);
  cairo_stroke(cr);
  archPath(0);
  setRgba(cr, rgb(20, 16, 26), 0.9);
  cairo_set_line_width(cr, 3);
  cairo_stroke(cr);
  // A faint film of grime on the glass and the leading.
  archPath(0);
  setRgba(cr, side == kSideMirror ? rgb(40, 90, 60) : rgb(60, 50, 90), 0.18);
  cairo_fill(cr);
  cairo_save(cr);
  archPath(0);
  cairo_clip(cr);
  for (double d = -200; d < 240; d += 18)
  {
    cairo_move_to(cr, d, H);
    cairo_line_to(cr, d + H, 0);
    cairo_move_to(cr, d, 0);
    cairo_line_to(cr, d + H, H);
  }
  setRgba(cr, rgb(20, 18, 28), 0.45);
  cairo_set_line_width(cr, 1.0);
  cairo_stroke(cr);
  // A broken pane.
  cairo_move_to(cr, 92, 120);
  cairo_line_to(cr, 100, 108);
  cairo_line_to(cr, 104, 124);
  cairo_line_to(cr, 110, 114);
  cairo_line_to(cr, 112, 132);
  cairo_close_path(cr);
  setRgba(cr, rgb(10, 8, 16), 0.85);
  cairo_fill(cr);
  cairo_restore(cr);
  // The mullion and transom.
  cairo_rectangle(cr, W * 0.5 - 4, 16, 8, H - 36);
  linearFill(cr, W * 0.5 - 4, 0, W * 0.5 + 4, 0, lighten(stone, 0.1f), darken(stone, 0.3f));
  cairo_rectangle(cr, 16, 98, W - 32, 6);
  linearFill(cr, 0, 98, 0, 104, lighten(stone, 0.1f), darken(stone, 0.3f));
  // The sill.
  cairo_rectangle(cr, 6, H - 22, W - 12, 12);
  linearFill(cr, 0, H - 22, 0, H - 10, lighten(stone, 0.25f), darken(stone, 0.3f));
  // Curtains: heavy velvet gathered with a tie, ragged at the hem.
  const Color velvet = side == kSideMirror ? rgb(30, 110, 70) : rgb(120, 20, 40);
  for (const int s : {-1, 1})
  {
    const double edge = s < 0 ? 0 : W;
    cairo_move_to(cr, edge, 0);
    cairo_line_to(cr, edge - s * 34, 0);
    cairo_curve_to(cr, edge - s * 30, 50, edge - s * 10, 90, edge - s * 16, 112);
    cairo_curve_to(cr, edge - s * 24, 140, edge - s * 30, 170, edge - s * 26, H);
    cairo_line_to(cr, edge - s * 20, H - 8);
    cairo_line_to(cr, edge - s * 14, H);
    cairo_line_to(cr, edge - s * 8, H - 6);
    cairo_line_to(cr, edge, H);
    cairo_close_path(cr);
    linearFill(cr, edge, 0, edge - s * 34, 0, darken(velvet, 0.5f), velvet, true);
    setRgba(cr, rgb(10, 4, 8), 0.8);
    cairo_set_line_width(cr, 1.4);
    cairo_stroke(cr);
    for (int k = 1; k < 3; ++k)
    {
      cairo_move_to(cr, edge - s * k * 9, 4);
      cairo_curve_to(cr, edge - s * (k * 9 - 2), 60, edge - s * (k * 6), 100, edge - s * (k * 4 + 6), 112);
    }
    setRgba(cr, darken(velvet, 0.6f), 0.6);
    cairo_set_line_width(cr, 2);
    cairo_stroke(cr);
    cairo_rectangle(cr, edge - s * 22 - 4, 108, 8, 8);
    setColor(cr, kGilt);
    cairo_fill(cr);
  }
  // The rod across the top.
  cairo_rectangle(cr, 0, 2, W, 5);
  linearFill(cr, 0, 2, 0, 7, rgb(240, 200, 110), kGiltDark);
}

// A library wall of bookcases, W x H px, shelves on the block rows.
void paintBookcases(cairo_t* cr, double W, double H, int side)
{
  const Color oak = side == kSideMirror ? rgb(40, 62, 44) : rgb(66, 38, 24);
  cairo_rectangle(cr, 0, 0, W, H);
  linearFill(cr, 0, 0, 0, H, darken(oak, 0.3f), darken(oak, 0.55f));
  std::uint32_t sd = 23023u;
  static const Color kSpines[] = {rgb(120, 30, 40), rgb(40, 60, 110), rgb(50, 90, 50), rgb(130, 90, 40),
                                  rgb(90, 40, 100), rgb(150, 130, 90), rgb(30, 30, 36), rgb(160, 60, 30)};
  for (double y = 0; y < H; y += 64)
  {
    // Books standing on the shelf at y + 64.
    double x = 6;
    while (x < W - 10)
    {
      const double r0 = rnd(sd);
      if (r0 < 0.06)
      {
        x += 18 + rnd(sd) * 20; // a gap
        continue;
      }
      const double bw = 7 + rnd(sd) * 9, bh = 34 + rnd(sd) * 22;
      const Color c = kSpines[xs(sd) % 8u];
      const double lean = r0 > 0.94 ? 0.25 : 0.0;
      cairo_save(cr);
      cairo_translate(cr, x, y + 60);
      cairo_rotate(cr, lean);
      cairo_rectangle(cr, 0, -bh, bw, bh);
      linearFill(cr, 0, 0, bw, 0, lighten(c, 0.2f), darken(c, 0.4f), true);
      setRgba(cr, rgb(10, 6, 6), 0.7);
      cairo_set_line_width(cr, 0.8);
      cairo_stroke(cr);
      for (const double by : {0.18, 0.8})
      {
        cairo_rectangle(cr, 1, -bh * (1 - by), bw - 2, 2);
        setRgba(cr, rgb(230, 190, 90), 0.75);
        cairo_fill(cr);
      }
      cairo_restore(cr);
      x += bw + (lean > 0 ? 8 : 0.6);
    }
    // A skull or a candle stub now and then.
    if (rnd(sd) < 0.5)
    {
      const double sx = 20 + rnd(sd) * (W - 40);
      circle(cr, sx, y + 50, 8);
      setColor(cr, rgb(220, 214, 196));
      cairo_fill(cr);
      cairo_rectangle(cr, sx - 5, y + 54, 10, 6);
      cairo_fill(cr);
      circle(cr, sx - 3, y + 49, 2.2);
      circle(cr, sx + 3, y + 49, 2.2);
      setColor(cr, rgb(20, 16, 16));
      cairo_fill(cr);
    }
    // The shelf board.
    cairo_rectangle(cr, 0, y + 58, W, 6);
    linearFill(cr, 0, y + 58, 0, y + 64, lighten(oak, 0.25f), darken(oak, 0.4f));
  }
  // Uprights every three blocks.
  for (double x = 0; x <= W; x += 192)
  {
    cairo_rectangle(cr, std::min(x, W - 10), 0, 10, H);
    linearFill(cr, x, 0, x + 10, 0, lighten(oak, 0.2f), darken(oak, 0.4f));
  }
  // The cornice.
  cairo_rectangle(cr, 0, 0, W, 10);
  linearFill(cr, 0, 0, 0, 10, lighten(oak, 0.35f), darken(oak, 0.3f));
}

// --- Furniture -------------------------------------------------------------------

// A gilt frame round a w x h opening at (x, y) with a moulded profile.
void giltFrame(cairo_t* cr, double x, double y, double w, double h, double t, bool tarnished)
{
  const Color hi = tarnished ? rgb(150, 140, 120) : rgb(255, 226, 140);
  const Color mid = tarnished ? rgb(100, 92, 80) : kGilt;
  const Color lo = tarnished ? rgb(50, 46, 40) : kGiltDark;
  cairo_rectangle(cr, x - t, y - t, w + 2 * t, h + 2 * t);
  cairo_rectangle(cr, x, y, w, h);
  cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
  cairo_pattern_t* g = cairo_pattern_create_linear(x - t, y - t, x + w + t, y + h + t);
  cairo_pattern_add_color_stop_rgb(g, 0, redOf(hi) / 255.0, greenOf(hi) / 255.0, blueOf(hi) / 255.0);
  cairo_pattern_add_color_stop_rgb(g, 0.5, redOf(mid) / 255.0, greenOf(mid) / 255.0, blueOf(mid) / 255.0);
  cairo_pattern_add_color_stop_rgb(g, 1, redOf(lo) / 255.0, greenOf(lo) / 255.0, blueOf(lo) / 255.0);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
  cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
  // The beads along the moulding and the inner lip.
  cairo_rectangle(cr, x - t * 0.5, y - t * 0.5, w + t, h + t);
  setRgba(cr, lo, 0.8);
  cairo_set_line_width(cr, 1.4);
  cairo_stroke(cr);
  cairo_rectangle(cr, x, y, w, h);
  setRgba(cr, rgb(20, 12, 4), 0.9);
  cairo_set_line_width(cr, 2.0);
  cairo_stroke(cr);
  cairo_rectangle(cr, x - t, y - t, w + 2 * t, h + 2 * t);
  outline(cr, 1.6);
  // Corner rosettes.
  for (const double cx : {x - t * 0.5, x + w + t * 0.5})
    for (const double cy : {y - t * 0.5, y + h + t * 0.5})
    {
      circle(cr, cx, cy, t * 0.7);
      cairo_pattern_t* rg = cairo_pattern_create_radial(cx - 2, cy - 2, 1, cx, cy, t * 0.7);
      cairo_pattern_add_color_stop_rgb(rg, 0, redOf(hi) / 255.0, greenOf(hi) / 255.0, blueOf(hi) / 255.0);
      cairo_pattern_add_color_stop_rgb(rg, 1, redOf(lo) / 255.0, greenOf(lo) / 255.0, blueOf(lo) / 255.0);
      cairo_set_source(cr, rg);
      cairo_fill_preserve(cr);
      cairo_pattern_destroy(rg);
      outline(cr, 1.2);
    }
}

// A mirror's frame, 128 x 192 (the glass's opening left clear): kind 0 a
// tall gilt frame with a crest, 1 the same gone black with a crack-starred
// glass, 2 the crypt's: a slab of carved stone.
void paintMirrorFrame(cairo_t* cr, int kind)
{
  const double x = 18, y = 22, w = 92, h = 160;
  if (kind == 2)
  {
    const Color st = rgb(70, 74, 72);
    cairo_rectangle(cr, x - 14, y - 16, w + 28, h + 26);
    cairo_rectangle(cr, x, y, w, h);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
    linearFill(cr, 0, 0, 128, 192, lighten(st, 0.2f), darken(st, 0.4f));
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
    // A skull carved on the lintel.
    circle(cr, 64, y - 8, 7);
    setColor(cr, rgb(120, 124, 118));
    cairo_fill(cr);
    circle(cr, 61, y - 9, 1.8);
    circle(cr, 67, y - 9, 1.8);
    setColor(cr, rgb(20, 20, 20));
    cairo_fill(cr);
    cairo_rectangle(cr, x, y, w, h);
    outline(cr, 2.5);
    return;
  }
  giltFrame(cr, x, y, w, h, 11, kind == 1);
  // The crest: a scrolled cartouche with a mask.
  const Color hi = kind == 1 ? rgb(150, 140, 120) : rgb(255, 226, 140);
  const Color lo = kind == 1 ? rgb(50, 46, 40) : kGiltDark;
  cairo_move_to(cr, 30, y - 8);
  cairo_curve_to(cr, 36, 0, 54, 2, 64, 4);
  cairo_curve_to(cr, 74, 2, 92, 0, 98, y - 8);
  cairo_close_path(cr);
  linearFill(cr, 0, 0, 0, y, hi, lo, true);
  outline(cr, 1.4);
  circle(cr, 64, 10, 6);
  linearFill(cr, 58, 4, 70, 16, hi, lo, true);
  outline(cr, 1.2);
  if (kind == 1)
  {
    // The crack star, from where something hit it.
    cairo_save(cr);
    cairo_rectangle(cr, x, y, w, h);
    cairo_clip(cr);
    const double cx = 70, cy = 84;
    std::uint32_t sd = 9157u;
    for (int k = 0; k < 9; ++k)
    {
      const double a = k * 2 * kPi / 9 + rnd(sd) * 0.4;
      double px = cx, py = cy;
      cairo_move_to(cr, px, py);
      for (int seg = 0; seg < 4; ++seg)
      {
        px += std::cos(a + (rnd(sd) - 0.5) * 0.6) * (14 + rnd(sd) * 14);
        py += std::sin(a + (rnd(sd) - 0.5) * 0.6) * (14 + rnd(sd) * 14);
        cairo_line_to(cr, px, py);
      }
    }
    for (const double rr : {10.0, 22.0})
    {
      cairo_new_sub_path(cr);
      for (int k = 0; k <= 9; ++k)
      {
        const double a = k * 2 * kPi / 9;
        const double rj = rr * (0.8 + 0.4 * rnd(sd));
        if (k == 0)
          cairo_move_to(cr, cx + std::cos(a) * rj, cy + std::sin(a) * rj);
        else
          cairo_line_to(cr, cx + std::cos(a) * rj, cy + std::sin(a) * rj);
      }
    }
    setRgba(cr, rgb(230, 240, 255), 0.75);
    cairo_set_line_width(cr, 1.2);
    cairo_stroke(cr);
    // A shard fallen out.
    cairo_move_to(cr, 40, 140);
    cairo_line_to(cr, 56, 128);
    cairo_line_to(cr, 52, 158);
    cairo_close_path(cr);
    setRgba(cr, rgb(10, 10, 14), 0.9);
    cairo_fill(cr);
    cairo_restore(cr);
  }
}

// The front door, 128 x 256: an arched oak double door studded with iron,
// a lion's-head knocker, a fanlight. open: the leaves swung back against
// the jambs and the doorway left clear.
void paintFrontDoor(cairo_t* cr, bool open)
{
  const double W = 128, H = 256;
  const Color stone = rgb(100, 96, 112), oak = rgb(78, 44, 28);
  auto doorway = [&]() {
    cairo_move_to(cr, 14, H);
    cairo_line_to(cr, 14, 64);
    cairo_arc(cr, W * 0.5, 64, W * 0.5 - 14, kPi, 2 * kPi);
    cairo_line_to(cr, W - 14, H);
    cairo_close_path(cr);
  };
  // The stone surround.
  cairo_rectangle(cr, 0, 0, W, H);
  doorway();
  cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
  linearFill(cr, 0, 0, W, 0, lighten(stone, 0.12f), darken(stone, 0.3f));
  cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
  for (int k = 0; k < 9; ++k)
  {
    const double a = kPi + k * kPi / 8;
    cairo_move_to(cr, W * 0.5 + std::cos(a) * (W * 0.5 - 14), 64 + std::sin(a) * (W * 0.5 - 14));
    cairo_line_to(cr, W * 0.5 + std::cos(a) * (W * 0.5), 64 + std::sin(a) * (W * 0.5));
  }
  setRgba(cr, rgb(30, 26, 36), 0.7);
  cairo_set_line_width(cr, 1.6);
  cairo_stroke(cr);
  if (open)
  {
    // The leaves folded back, edge on, against the jambs.
    for (const double x : {14.0, W - 24.0})
    {
      cairo_rectangle(cr, x, 70, 10, H - 70);
      linearFill(cr, x, 0, x + 10, 0, lighten(oak, 0.2f), darken(oak, 0.4f), true);
      outline(cr, 1.4);
    }
    doorway();
    setRgba(cr, rgb(10, 6, 14), 0.9);
    cairo_set_line_width(cr, 3);
    cairo_stroke(cr);
    return;
  }
  doorway();
  linearFill(cr, 14, 0, W - 14, 0, lighten(oak, 0.12f), darken(oak, 0.3f), true);
  outline(cr, 2.5);
  // Planks and the split between the leaves.
  for (double x = 30; x < W - 14; x += 16)
  {
    cairo_move_to(cr, x, 30);
    cairo_line_to(cr, x, H);
  }
  setRgba(cr, darken(oak, 0.6f), 0.7);
  cairo_set_line_width(cr, 1.2);
  cairo_stroke(cr);
  cairo_move_to(cr, W * 0.5, 14);
  cairo_line_to(cr, W * 0.5, H);
  outline(cr, 2.2);
  // Iron straps and studs.
  for (const double y : {96.0, 180.0, 236.0})
  {
    cairo_rectangle(cr, 16, y, W - 32, 7);
    linearFill(cr, 0, y, 0, y + 7, rgb(90, 90, 100), rgb(30, 30, 36));
    for (double x = 22; x < W - 16; x += 14)
    {
      circle(cr, x, y + 3.5, 2.2);
      setColor(cr, rgb(150, 150, 160));
      cairo_fill(cr);
    }
  }
  // The fanlight, lit faintly from inside.
  cairo_move_to(cr, 30, 64);
  cairo_arc(cr, W * 0.5, 64, W * 0.5 - 30, kPi, 2 * kPi);
  cairo_close_path(cr);
  setRgba(cr, rgb(255, 210, 120), 0.5);
  cairo_fill_preserve(cr);
  outline(cr, 1.6);
  for (int k = 1; k < 4; ++k)
  {
    const double a = kPi + k * kPi / 4;
    cairo_move_to(cr, W * 0.5, 64);
    cairo_line_to(cr, W * 0.5 + std::cos(a) * (W * 0.5 - 30), 64 + std::sin(a) * (W * 0.5 - 30));
  }
  outline(cr, 1.4);
  // The knocker: a lion's head holding a ring.
  for (const double kx : {W * 0.5 - 18, W * 0.5 + 18})
  {
    circle(cr, kx, 140, 8);
    linearFill(cr, kx - 8, 132, kx + 8, 148, rgb(240, 200, 110), kGiltDark, true);
    outline(cr, 1.2);
    cairo_new_path(cr);
    cairo_arc(cr, kx, 156, 9, 0, 2 * kPi);
    setColor(cr, kInk);
    cairo_set_line_width(cr, 4.4);
    cairo_stroke_preserve(cr);
    setColor(cr, kGilt);
    cairo_set_line_width(cr, 2.6);
    cairo_stroke(cr);
  }
}

// A chandelier, 320 x 192, hung from the middle of the top edge: chain,
// stem, two tiers of scrolled arms with candle cups, crystal drops. The
// flames are drawn live.
void paintChandelier(cairo_t* cr, double W, double H)
{
  const double cx = W * 0.5;
  // The chain.
  for (double y = 0; y < H * 0.3; y += 9)
  {
    cairo_save(cr);
    cairo_translate(cr, cx, y + 4.5);
    cairo_scale(cr, (int(y / 9) % 2) ? 1.6 : 3.4, 4.5);
    cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
    cairo_restore(cr);
    setColor(cr, kInk);
    cairo_set_line_width(cr, 3.4);
    cairo_stroke_preserve(cr);
    setColor(cr, kGilt);
    cairo_set_line_width(cr, 1.8);
    cairo_stroke(cr);
  }
  // The stem: a stack of turned brass.
  const double sy = H * 0.3;
  for (int k = 0; k < 4; ++k)
  {
    const double y = sy + k * 18, rw = k % 2 ? 7 : 12;
    cairo_save(cr);
    cairo_translate(cr, cx, y + 9);
    cairo_scale(cr, rw, 9);
    cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
    cairo_restore(cr);
    linearFill(cr, cx - rw, 0, cx + rw, 0, rgb(255, 226, 140), kGiltDark, true);
    outline(cr, 1.4);
  }
  // The arms, two tiers.
  for (int tier = 0; tier < 2; ++tier)
  {
    const double ay = sy + 30 + tier * 34, span = tier == 0 ? W * 0.44 : W * 0.3;
    for (const int s : {-1, 1})
    {
      for (int k = 0; k < 2; ++k)
      {
        const double ex = cx + s * span * (k == 0 ? 1.0 : 0.55), ey = ay - 18;
        cairo_move_to(cr, cx, ay + 6);
        cairo_curve_to(cr, cx + s * span * 0.3, ay + 30, ex, ay + 24, ex, ey);
        setColor(cr, kInk);
        cairo_set_line_width(cr, 6.4);
        cairo_stroke_preserve(cr);
        setColor(cr, kGilt);
        cairo_set_line_width(cr, 4);
        cairo_stroke(cr);
        // The cup and a stub of candle.
        cairo_rectangle(cr, ex - 9, ey - 4, 18, 6);
        linearFill(cr, ex - 9, 0, ex + 9, 0, rgb(255, 226, 140), kGiltDark, true);
        outline(cr, 1.2);
        cairo_rectangle(cr, ex - 3.5, ey - 22, 7, 18);
        linearFill(cr, ex - 3.5, 0, ex + 3.5, 0, rgb(250, 246, 232), rgb(190, 180, 160), true);
        outline(cr, 1.0);
        // A crystal drop under the arm.
        cairo_move_to(cr, ex, ay + 10);
        cairo_line_to(cr, ex - 4, ay + 20);
        cairo_line_to(cr, ex, ay + 30);
        cairo_line_to(cr, ex + 4, ay + 20);
        cairo_close_path(cr);
        setRgba(cr, rgb(220, 240, 255), 0.85);
        cairo_fill(cr);
      }
    }
  }
  // Cobwebs strung from the arms.
  cairo_move_to(cr, cx - W * 0.44, sy + 12);
  cairo_curve_to(cr, cx - W * 0.3, sy + 40, cx - W * 0.2, sy + 40, cx - W * 0.1, sy + 26);
  setRgba(cr, rgb(230, 230, 240), 0.35);
  cairo_set_line_width(cr, 0.8);
  cairo_stroke(cr);
}

// The candle flames of a chandelier (rest position, relative to the hook).
std::vector<std::pair<float, float>> chandelierCandles(float W, float H)
{
  std::vector<std::pair<float, float>> v;
  const float cx = W * 0.5f, sy = H * 0.3f;
  for (int tier = 0; tier < 2; ++tier)
  {
    const float ay = sy + 30.0f + float(tier) * 34.0f, span = tier == 0 ? W * 0.44f : W * 0.3f;
    for (const float s : {-1.0f, 1.0f})
      for (int k = 0; k < 2; ++k)
        v.push_back({cx + s * span * (k == 0 ? 1.0f : 0.55f) - cx, ay - 18.0f - 22.0f});
  }
  return v;
}

// A candle flame, 16 x 30, its foot at (8, 28).
void paintFlame(cairo_t* cr)
{
  cairo_move_to(cr, 8, 29);
  cairo_curve_to(cr, 1, 26, 2, 16, 8, 2);
  cairo_curve_to(cr, 14, 16, 15, 26, 8, 29);
  cairo_close_path(cr);
  cairo_pattern_t* g = cairo_pattern_create_linear(0, 2, 0, 29);
  cairo_pattern_add_color_stop_rgba(g, 0, 1, 1, 1, 0.2);
  cairo_pattern_add_color_stop_rgba(g, 0.5, 1, 1, 1, 0.95);
  cairo_pattern_add_color_stop_rgba(g, 1, 1, 1, 1, 0.8);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
  cairo_save(cr);
  cairo_translate(cr, 8, 22);
  cairo_scale(cr, 2, 4);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_restore(cr);
  cairo_set_source_rgba(cr, 1, 1, 1, 1);
  cairo_fill(cr);
}

// A wall sconce: a brass back plate and a cup with a candle, 32 x 48, the
// candle's top at (16, 10).
void paintSconce(cairo_t* cr)
{
  cairo_move_to(cr, 10, 46);
  cairo_curve_to(cr, 4, 36, 6, 26, 16, 22);
  cairo_curve_to(cr, 26, 26, 28, 36, 22, 46);
  cairo_close_path(cr);
  linearFill(cr, 4, 0, 28, 0, rgb(255, 226, 140), kGiltDark, true);
  outline(cr, 1.4);
  cairo_rectangle(cr, 8, 26, 16, 5);
  linearFill(cr, 8, 0, 24, 0, rgb(255, 226, 140), kGiltDark, true);
  outline(cr, 1.0);
  cairo_rectangle(cr, 12.5, 10, 7, 16);
  linearFill(cr, 12.5, 0, 19.5, 0, rgb(250, 246, 232), rgb(190, 180, 160), true);
  outline(cr, 1.0);
}

// A lever: an iron wall plate, 64 x 64, with the pivot at (32, 40).
void paintLeverPlate(cairo_t* cr)
{
  cairo_rectangle(cr, 14, 18, 36, 44);
  linearFill(cr, 14, 0, 50, 0, rgb(110, 110, 124), rgb(46, 46, 56), true);
  outline(cr, 1.8);
  // The slot the lever travels in.
  cairo_arc(cr, 32, 40, 18, kPi * 1.15, kPi * 1.85);
  setColor(cr, rgb(16, 14, 20));
  cairo_set_line_width(cr, 5);
  cairo_stroke(cr);
  for (const double y : {22.0, 58.0})
    for (const double x : {18.0, 46.0})
    {
      circle(cr, x, y, 2);
      setColor(cr, rgb(170, 170, 184));
      cairo_fill(cr);
    }
}

// A portrait's frame, 160 x 168, round a 112 x 116 canvas at (24, 26).
// canvas: 0 the ghost painted in it (eyes `flare`), 1 left empty (a smear
// of ectoplasm where it climbed out).
void paintPortrait(cairo_t* cr, int canvas, double flare)
{
  const double x = 24, y = 26, w = 112, h = 116;
  // The canvas: a dark varnished ground with a vignette.
  cairo_rectangle(cr, x, y, w, h);
  cairo_pattern_t* g = cairo_pattern_create_radial(x + w * 0.5, y + h * 0.45, 8, x + w * 0.5, y + h * 0.5, w * 0.7);
  cairo_pattern_add_color_stop_rgb(g, 0, 0.28, 0.22, 0.2);
  cairo_pattern_add_color_stop_rgb(g, 1, 0.07, 0.05, 0.06);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
  cairo_save(cr);
  cairo_rectangle(cr, x, y, w, h);
  cairo_clip(cr);
  if (canvas == 0)
    paintGhostFace(cr, x + w * 0.52, y + h * 0.42, 58, flare);
  else
  {
    // An empty chair back, and the green smear where he climbed out.
    cairo_rectangle(cr, x + 26, y + 60, 60, 60);
    setRgba(cr, rgb(70, 30, 30), 0.9);
    cairo_fill(cr);
    cairo_move_to(cr, x + 40, y + 30);
    cairo_curve_to(cr, x + 60, y + 50, x + 50, y + 80, x + 90, y + 120);
    setRgba(cr, rgb(120, 255, 150), 0.35);
    cairo_set_line_width(cr, 14);
    cairo_stroke(cr);
  }
  // Craquelure on the varnish.
  std::uint32_t sd = 4711u;
  for (int k = 0; k < 14; ++k)
  {
    const double px = x + rnd(sd) * w, py = y + rnd(sd) * h;
    cairo_move_to(cr, px, py);
    cairo_line_to(cr, px + (rnd(sd) - 0.5) * 18, py + (rnd(sd) - 0.5) * 18);
  }
  setRgba(cr, rgb(0, 0, 0), 0.25);
  cairo_set_line_width(cr, 0.7);
  cairo_stroke(cr);
  cairo_restore(cr);
  giltFrame(cr, x, y, w, h, 14, false);
  // A brass name plate.
  cairo_rectangle(cr, 58, 150, 44, 10);
  linearFill(cr, 0, 150, 0, 160, rgb(255, 226, 140), kGiltDark, true);
  outline(cr, 1.0);
}

// Lance's portrait (Secret 2): Lance Marquee as a vampire, an oval in a
// gilt frame, 160 x 168. shot: fangs out; wink: one eye shut.
void paintLancePortrait(cairo_t* cr, bool shot, bool wink)
{
  const double cx = 80, cy = 84, rx = 56, ry = 66;
  auto oval = [&](double grow) {
    cairo_save(cr);
    cairo_translate(cr, cx, cy);
    cairo_scale(cr, rx + grow, ry + grow);
    cairo_new_sub_path(cr);
    cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
    cairo_restore(cr);
  };
  // The ground: a blood-red curtain behind him.
  oval(0);
  cairo_pattern_t* g = cairo_pattern_create_radial(cx, cy - 20, 6, cx, cy, ry);
  cairo_pattern_add_color_stop_rgb(g, 0, 0.55, 0.08, 0.14);
  cairo_pattern_add_color_stop_rgb(g, 1, 0.12, 0.02, 0.05);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
  cairo_save(cr);
  oval(0);
  cairo_clip(cr);
  // Lance, head and shoulders, pale as the grave, in his cape.
  const double s = 0.58;
  const double hx = 126, hy = 112; // his head's middle in paintLance's units
  cairo_push_group(cr);
  paintCapeCollar(cr, cx, cy + 38, 120, 70);
  cairo_save(cr);
  cairo_translate(cr, cx - hx * s, cy - 6 - hy * s);
  cairo_scale(cr, s, s);
  paintLance(cr, 9, false);
  cairo_restore(cr);
  // The cape over his shoulders.
  for (const int side : {-1, 1})
  {
    cairo_move_to(cr, cx + side * 8, cy + 30);
    cairo_curve_to(cr, cx + side * 30, cy + 34, cx + side * 50, cy + 50, cx + side * 60, cy + 80);
    cairo_line_to(cr, cx + side * 6, cy + 80);
    cairo_close_path(cr);
    linearFill(cr, cx, 0, cx + side * 60, 0, rgb(50, 44, 60), rgb(10, 8, 14), true);
    outline(cr, 1.4);
  }
  // Pale: a cold green-white over everything painted so far.
  cairo_set_operator(cr, CAIRO_OPERATOR_ATOP);
  cairo_rectangle(cr, 0, 0, 160, 168);
  cairo_set_source_rgba(cr, 0.75, 0.95, 0.85, 0.32);
  cairo_fill(cr);
  cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
  cairo_pop_group_to_source(cr);
  cairo_paint(cr);
  const double fx = cx - hx * s, fy = cy - 6 - hy * s;
  auto P = [&](double x, double y) { return std::pair<double, double>{fx + x * s, fy + y * s}; };
  if (shot)
  {
    // Fangs over the grin.
    for (const double tx : {hx + 12.0, hx + 30.0})
    {
      const auto a = P(tx - 3, hy + 24), b = P(tx, hy + 36), c = P(tx + 3, hy + 24);
      cairo_move_to(cr, a.first, a.second);
      cairo_line_to(cr, b.first, b.second);
      cairo_line_to(cr, c.first, c.second);
      cairo_close_path(cr);
      setColor(cr, rgb(255, 255, 255));
      cairo_fill_preserve(cr);
      outline(cr, 0.8);
    }
    const auto drop = P(hx + 30, hy + 40);
    circle(cr, drop.first, drop.second, 1.6);
    setColor(cr, rgb(200, 0, 20));
    cairo_fill(cr);
  }
  if (wink)
  {
    // The eye shut in a wink.
    const auto e = P(hx + 20, hy - 4);
    cairo_save(cr);
    cairo_translate(cr, e.first, e.second);
    cairo_scale(cr, 6 * s * 1.3, 7 * s * 1.3);
    cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
    cairo_restore(cr);
    setColor(cr, rgb(214, 214, 200));
    cairo_fill(cr);
    cairo_move_to(cr, e.first - 4, e.second);
    cairo_curve_to(cr, e.first - 1, e.second + 3, e.first + 2, e.second + 3, e.first + 5, e.second - 1);
    outline(cr, 1.4);
  }
  cairo_restore(cr);
  // The oval frame.
  oval(9);
  oval(0);
  cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
  cairo_pattern_t* fg = cairo_pattern_create_linear(cx - rx, cy - ry, cx + rx, cy + ry);
  cairo_pattern_add_color_stop_rgb(fg, 0, 1.0, 0.89, 0.55);
  cairo_pattern_add_color_stop_rgb(fg, 0.5, 0.84, 0.67, 0.31);
  cairo_pattern_add_color_stop_rgb(fg, 1, 0.43, 0.3, 0.1);
  cairo_set_source(cr, fg);
  cairo_fill(cr);
  cairo_pattern_destroy(fg);
  cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
  oval(9);
  outline(cr, 1.6);
  oval(0);
  outline(cr, 2.0);
  // A bat on the crest.
  cairo_move_to(cr, cx, cy - ry - 4);
  for (const int side : {-1, 1})
  {
    cairo_move_to(cr, cx, cy - ry - 6);
    cairo_curve_to(cr, cx + side * 8, cy - ry - 18, cx + side * 18, cy - ry - 14, cx + side * 24, cy - ry - 8);
    cairo_line_to(cr, cx + side * 18, cy - ry - 4);
    cairo_line_to(cr, cx + side * 12, cy - ry);
    cairo_line_to(cr, cx + side * 6, cy - ry - 2);
    cairo_close_path(cr);
  }
  setColor(cr, rgb(20, 16, 24));
  cairo_fill(cr);
  // The plate.
  cairo_rectangle(cr, cx - 30, cy + ry + 2, 60, 12);
  linearFill(cr, 0, cy + ry + 2, 0, cy + ry + 14, rgb(255, 226, 140), kGiltDark, true);
  outline(cr, 1.0);
}

// The coffin lying on its side, w px long and 128 tall: body (the box with
// its handles) or lid (the top slab, 26 px thick).
void paintCoffin(cairo_t* cr, double w, bool lid)
{
  const Color wood = rgb(64, 28, 30), woodDark = rgb(26, 10, 14);
  if (lid)
  {
    cairo_move_to(cr, 4, 26);
    cairo_line_to(cr, 14, 4);
    cairo_line_to(cr, w * 0.3, 0);
    cairo_line_to(cr, w - 10, 6);
    cairo_line_to(cr, w - 4, 26);
    cairo_close_path(cr);
    linearFill(cr, 0, 0, 0, 26, lighten(wood, 0.3f), woodDark, true);
    outline(cr, 2.2);
    // A brass cross on the lid.
    cairo_rectangle(cr, w * 0.3 - 18, 8, 36, 5);
    cairo_rectangle(cr, w * 0.3 - 2.5, 4, 5, 14);
    setColor(cr, kGilt);
    cairo_fill(cr);
    return;
  }
  cairo_move_to(cr, 0, 26);
  cairo_line_to(cr, w, 26);
  cairo_line_to(cr, w - 14, 126);
  cairo_line_to(cr, w * 0.3 + 6, 128);
  cairo_line_to(cr, 10, 120);
  cairo_close_path(cr);
  linearFill(cr, 0, 26, 0, 128, wood, woodDark, true);
  outline(cr, 2.4);
  // Panels and grain.
  cairo_move_to(cr, 14, 40);
  cairo_line_to(cr, w - 16, 40);
  cairo_move_to(cr, 16, 112);
  cairo_line_to(cr, w - 22, 112);
  setRgba(cr, rgb(110, 60, 60), 0.4);
  cairo_set_line_width(cr, 1.6);
  cairo_stroke(cr);
  // The handles.
  for (const double hx : {w * 0.18, w * 0.5, w * 0.8})
  {
    cairo_rectangle(cr, hx - 14, 70, 28, 6);
    setColor(cr, kGilt);
    cairo_fill_preserve(cr);
    outline(cr, 1.2);
    for (const double bx : {hx - 12, hx + 12})
    {
      circle(cr, bx, 66, 3);
      setColor(cr, kGiltDark);
      cairo_fill(cr);
    }
  }
}

// The sign on the coffin, 168 x 70: a board on a stake, the text added live.
void paintSign(cairo_t* cr)
{
  cairo_rectangle(cr, 80, 46, 8, 24);
  linearFill(cr, 80, 0, 88, 0, rgb(120, 90, 60), rgb(60, 40, 24), true);
  outline(cr, 1.2);
  cairo_move_to(cr, 4, 6);
  cairo_line_to(cr, 164, 2);
  cairo_line_to(cr, 162, 50);
  cairo_line_to(cr, 6, 52);
  cairo_close_path(cr);
  linearFill(cr, 0, 0, 0, 52, rgb(206, 184, 140), rgb(150, 124, 86), true);
  outline(cr, 2.0);
  for (const double nx : {12.0, 156.0})
  {
    circle(cr, nx, 10, 2);
    setColor(cr, rgb(60, 50, 40));
    cairo_fill(cr);
  }
}

// A crossbow bolt texture: 64 x 16, the tip at (0, 8).
void paintBoltTex(cairo_t* cr)
{
  cairo_translate(cr, 2, 8);
  paintBolt(cr, 58);
}

} // namespace

// --- The walls -------------------------------------------------------------------

void World::drawManorWall(Renderer& r, float camX, float camY, int frame) const
{
  (void)frame;
  const auto& m = mManor;
  const bool split = m.split.on;
  const int side = mSplitPass == 2 || (!split && m.side == kSideMirror) ? kSideMirror : kSideReal;
  const int W = mLevel->width, Hh = mLevel->height;
  const int tx0 = std::max(0, int(camX / kTilePx) - 1), ty0 = std::max(0, int(camY / kTilePx) - 1);
  const int tx1 = std::min(W - 1, int((camX + float(kScreenW)) / kTilePx) + 1);
  const int ty1 = std::min(Hh - 1, int((camY + float(kScreenH)) / kTilePx) + 1);
  const int cellarRow = split ? Hh + 1 : mLevel->startTy + 2;
  auto solid = [&](int tx, int ty) { return tx < 0 || ty < 0 || tx >= W || ty >= Hh || mMap.block(tx, ty) == Tile::Solid; };
  auto floorDist = [&](int tx, int ty) {
    int d = 1;
    while (d < 24 && !solid(tx, ty + d))
      ++d;
    return d;
  };
  auto ceilDist = [&](int tx, int ty) {
    int d = 1;
    while (d < 24 && !solid(tx, ty - d))
      ++d;
    return d;
  };
  // Zones: 0 a room, 1 the stair hall, 2 the cellar, 3 the attic.
  auto zone = [&](int tx, int ty) {
    if (ty >= cellarRow)
      return 2;
    const int fd = floorDist(tx, ty), cd = ceilDist(tx, ty);
    if (fd + cd > 17)
      return 1;
    if (!split && ty - cd <= 5 && ty + fd <= 14)
      return 3;
    return 0;
  };
  auto plain = [&](int tx, int ty) {
    const Tile t = mMap.block(tx, ty);
    return t == Tile::Empty && !mLayerMask[std::size_t(ty * W + tx)];
  };
  // A window's top-left block if (tx, ty) is part of one.
  auto windowAt = [&](int tx, int ty, int& ax, int& ay) {
    const int col = ((tx % 16) + 16) % 16;
    if (col != 6 && col != 7)
      return false;
    ax = tx - (col - 6);
    const int fd = floorDist(tx, ty);
    if (fd < 3 || fd > 5)
      return false;
    ay = ty - (5 - fd);
    for (int j = 0; j < 3; ++j)
      for (int i = 0; i < 2; ++i)
      {
        const int x = ax + i, y = ay + j;
        if (!plain(x, y) || floorDist(x, y) != 5 - j || zone(x, y) != 0)
          return false;
      }
    if (ceilDist(ax, ay) < 2 || ceilDist(ax + 1, ay) < 2)
      return false;
    for (const auto& d : m.decos)
      if (ax + 1 >= d.x0 && ax <= d.x1 && ay + 2 >= d.y0 && ay <= d.y1)
        return false;
    return true;
  };
  const std::string sk = side == kSideMirror ? "m" : "r";
  for (int ty = ty0; ty <= ty1; ++ty)
    for (int tx = tx0; tx <= tx1; ++tx)
    {
      if (mMap.block(tx, ty) == Tile::Solid && !mLayerMask[std::size_t(ty * W + tx)])
        continue;
      const float x = float(tx) * kTilePx - camX, y = float(ty) * kTilePx - camY;
      const std::uint32_t h = hash2(tx * 7 + 3, ty * 13 + 1);
      const int z = zone(tx, ty);
      if (z == 2)
      {
        const int v = int(h % 7u) == 0 ? 2 : int(h % 2u);
        r.draw(baked(mArt, r, "rubble" + std::to_string(v), 64, 64, 0, 0, [v](cairo_t* cr) { paintRubble(cr, v); }), x, y);
        continue;
      }
      if (z == 1)
      {
        const int v = int(h % 11u) == 0 ? 3 : int(h % 3u);
        r.draw(baked(mArt, r, "hall" + std::to_string(v), 64, 64, 0, 0, [v](cairo_t* cr) { paintHallStone(cr, v); }), x, y);
        continue;
      }
      if (z == 3)
      {
        const int v = int(h % 5u) == 0 ? 1 : 0;
        r.draw(baked(mArt, r, "boards" + std::to_string(v), 64, 64, 0, 0, [v](cairo_t* cr) { paintBoards(cr, v); }), x, y);
        continue;
      }
      int ax = 0, ay = 0;
      if (plain(tx, ty) && windowAt(tx, ty, ax, ay))
        continue; // the storm shows through (the frame goes on below)
      const int fd = floorDist(tx, ty);
      if (fd <= 2)
      {
        const int row = fd == 2 ? 0 : 1;
        r.draw(baked(mArt, r, "wains" + std::to_string(row) + sk, 64, 64, 0, 0,
                 [row, side](cairo_t* cr) { paintWainscot(cr, row, side); }),
          x, y);
        continue;
      }
      // The paper: by the storey's floor row.
      const int pattern = int(std::uint32_t(ty + fd) * 2654435761u >> 16) % 3;
      const int v = h % 9u == 0 ? 1 : (h % 13u == 0 ? 2 : 0);
      r.draw(baked(mArt, r, "paper" + std::to_string(pattern) + std::to_string(v) + sk, 64, 64, 0, 0,
               [pattern, side, v](cairo_t* cr) { paintPaper(cr, pattern, side, v); }),
        x, y);
      // A picture rail just under the ceiling.
      if (ceilDist(tx, ty) == 1)
      {
        r.fillRect(x, y + 6.0f, kTilePx, 5.0f, side == kSideMirror ? rgb(40, 70, 50) : rgb(70, 42, 28));
        r.fillRect(x, y + 6.0f, kTilePx, 1.5f, rgba(255, 230, 200, 60));
      }
    }
  // Window frames over the openings, lit by the lightning.
  for (int ty = ty0; ty <= ty1; ++ty)
    for (int tx = tx0; tx <= tx1; ++tx)
    {
      int ax = 0, ay = 0;
      if (((tx % 16) + 16) % 16 != 6 || !plain(tx, ty) || !windowAt(tx, ty, ax, ay) || ay != ty || zone(tx, ty) != 0)
        continue;
      const float x = float(ax) * kTilePx - camX, y = float(ay) * kTilePx - camY;
      if (m.flash > 0)
        r.fillRect(x + 16.0f, y + 14.0f, 96.0f, 158.0f, rgba(235, 235, 255, 50 * m.flash));
      r.draw(baked(mArt, r, "window" + sk, 128, 192, 0, 0, [side](cairo_t* cr) { paintWindow(cr, side); }), x, y);
    }
  // The library's bookcases.
  for (const auto& d : m.decos)
  {
    if (d.kind != "bookshelf")
      continue;
    const int bw = (d.x1 - d.x0 + 1) * int(kTilePx), bh = (d.y1 - d.y0 + 1) * int(kTilePx);
    const float x = float(d.x0) * kTilePx - camX, y = float(d.y0) * kTilePx - camY;
    if (!visible(x, y, float(bw), float(bh)))
      continue;
    r.draw(baked(mArt, r, "books" + std::to_string(bw) + "x" + std::to_string(bh) + sk, bw, bh, 0, 0,
             [bw, bh, side](cairo_t* cr) { paintBookcases(cr, double(bw), double(bh), side); }),
      x, y);
  }
}

// --- Over the tiles ------------------------------------------------------------------

void World::drawManorBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)alpha;
  const auto& m = mManor;
  const bool mirrorView = mSplitPass == 2 || (!m.split.on && m.side == kSideMirror);
  const Color otherGlow = mirrorView ? kPurpleGlow : kGreenGlow;
  const Color flameC = mirrorView ? rgb(150, 255, 160) : rgb(255, 196, 90);
  auto sx = [&](float bx) { return bx * kTilePx - camX; };
  auto sy = [&](float by) { return by * kTilePx - camY; };
  const Texture& flame = baked(mArt, r, "flame", 16, 30, 8.0f, 28.0f, [](cairo_t* cr) { paintFlame(cr); });

  // Side-only walls and floors: stone or oak on their side, a ghost outline
  // on the other (in the other side's colour, shimmering).
  for (const auto& sl : m.layers)
  {
    const auto& l = mLayers[std::size_t(sl.layer)];
    const float x = sx(float(l.x0)), y = sy(float(l.y0));
    const float w = float(l.x1 - l.x0 + 1) * kTilePx, h = float(l.y1 - l.y0 + 1) * kTilePx;
    if (!visible(x, y, w, h))
      continue;
    const bool plat = l.tile == Tile::Platform;
    if (l.solid)
    {
      for (int ty = l.y0; ty <= l.y1; ++ty)
        for (int tx = l.x0; tx <= l.x1; ++tx)
        {
          const float bx = sx(float(tx)), by = sy(float(ty));
          if (plat)
          {
            r.draw(mArt.platform, bx, by);
            continue;
          }
          int depth = 0;
          while (depth < 3 && mMap.block(tx, ty - depth - 1) == Tile::Solid)
            ++depth;
          static constexpr int kShade[4] = {255, 210, 175, 145};
          const auto hh = (hash2(tx, ty) >> 8) % 10u;
          DrawOpts o;
          o.tint = rgb(kShade[depth], kShade[depth], kShade[depth]);
          r.draw(mArt.solid[hh < 7u ? 0u : (hh < 9u ? 1u : 2u)], bx, by, o);
        }
      if (!plat)
        for (int tx = l.x0; tx <= l.x1; ++tx)
          if (mMap.block(tx, l.y0 - 1) != Tile::Solid)
            r.draw(mArt.solidTop, sx(float(tx)), y);
      // The edge that comes and goes with the side: a faint seam of light.
      r.fillRect(x, y, w, 2.0f, withAlpha(mirrorView ? kGreenGlow : kPurpleGlow, 50));
    }
    else
    {
      const float hh = plat ? 14.0f : h;
      const float shimmer = 0.6f + 0.4f * std::sin(float(frame) * 0.12f + float(l.x0 + l.y0));
      r.fillRect(x, y, w, hh, withAlpha(otherGlow, int(14 * shimmer)));
      const Color g = withAlpha(otherGlow, int(90 * shimmer));
      // Dashed edges.
      for (float dx = 0.0f; dx < w; dx += 16.0f)
      {
        const float len = std::min(9.0f, w - dx);
        r.fillRect(x + dx, y, len, 2.0f, g);
        r.fillRect(x + dx, y + hh - 2.0f, len, 2.0f, g);
      }
      for (float dy = 0.0f; dy < hh; dy += 16.0f)
      {
        const float len = std::min(9.0f, hh - dy);
        r.fillRect(x, y + dy, 2.0f, len, g);
        r.fillRect(x + w - 2.0f, y + dy, 2.0f, len, g);
      }
    }
  }

  // The ballroom doors: tall oak double doors until both levers are
  // thrown, then standing open.
  for (const auto& d : m.doors)
  {
    const auto& l = mLayers[std::size_t(d.layer)];
    const float x = sx(float(l.x0)), y = sy(float(l.y0));
    const float w = float(l.x1 - l.x0 + 1) * kTilePx, h = float(l.y1 - l.y0 + 1) * kTilePx;
    if (!visible(x, y, w, h))
      continue;
    if (l.solid)
    {
      r.fillRect(x - 6.0f, y, w + 12.0f, h, rgb(40, 22, 14));
      r.fillRect(x, y, w, h, rgb(84, 48, 30));
      for (float px = x + 8.0f; px < x + w - 4.0f; px += 14.0f)
        r.fillRect(px, y, 2.0f, h, rgba(30, 14, 8, 160));
      for (float py = y + 40.0f; py < y + h; py += 150.0f)
      {
        r.fillRect(x, py, w, 8.0f, rgb(60, 60, 70));
        for (float px = x + 6.0f; px < x + w; px += 14.0f)
          r.fillRect(px, py + 2.0f, 4.0f, 4.0f, rgb(160, 160, 170));
      }
      // The padlocks, one for each lever: red, or green once thrown.
      for (std::size_t k = 0; k < d.levers.size(); ++k)
      {
        const bool on = m.levers[std::size_t(d.levers[k])].state == 1;
        const float ly = y + h * 0.42f + float(k) * 70.0f;
        r.fillRect(x + w * 0.5f - 14.0f, ly, 28.0f, 24.0f, kGilt);
        r.drawLine(x + w * 0.5f - 8.0f, ly, x + w * 0.5f - 8.0f, ly - (on ? 18.0f : 10.0f), 4.0f, kGiltDark);
        r.drawLine(x + w * 0.5f + 8.0f, ly, x + w * 0.5f + 8.0f, ly - 10.0f, 4.0f, kGiltDark);
        r.drawLine(x + w * 0.5f - 8.0f, ly - (on ? 18.0f : 10.0f), x + w * 0.5f + 8.0f, ly - 10.0f, 4.0f, kGiltDark);
        drawGlow(r, mArt, x + w * 0.5f, ly + 12.0f, 26.0f, on ? kGreenGlow : rgb(255, 60, 60), 0.6f);
      }
    }
    else
    {
      // Open: the jambs, the lintel and a threshold.
      r.fillRect(x - 6.0f, y, 16.0f, h, rgb(70, 40, 26));
      r.fillRect(x + w - 10.0f, y, 16.0f, h, rgb(70, 40, 26));
      r.fillRect(x - 6.0f, y, w + 12.0f, 14.0f, rgb(60, 34, 22));
      r.fillRect(x - 6.0f, y + h - 8.0f, w + 12.0f, 8.0f, rgb(90, 70, 50));
    }
  }

  // Decorations: the front door, chandeliers.
  for (const auto& d : m.decos)
  {
    if (offSide(d.side))
      continue;
    const float x = sx(float(d.x0)), y = sy(float(d.y0));
    const float w = float(d.x1 - d.x0 + 1) * kTilePx, h = float(d.y1 - d.y0 + 1) * kTilePx;
    if (!visible(x - 64, y - 64, w + 128, h + 128))
      continue;
    if (d.kind == "frontdoor")
    {
      // In the reflection the front door is the way out: open, and the
      // night beyond it glowing green.
      const bool open = !m.split.on && !offSide(m.exitSide) && m.exitSide >= 0;
      if (open)
        drawGlow(r, mArt, x + w * 0.5f, y + h * 0.6f, 160.0f, kGreenGlow, 0.35f + 0.1f * std::sin(float(frame) * 0.1f));
      r.draw(baked(mArt, r, open ? "door_open" : "door", 128, 256, 0, 0, [open](cairo_t* cr) { paintFrontDoor(cr, open); }),
        x, y + h - 256.0f);
    }
    else if (d.kind == "chandelier")
    {
      const float cw = w, ch = h;
      const int iw = int(cw), ih = int(ch);
      const float sway = std::sin(float(frame) * 0.03f + float(d.x0)) * 1.6f + (m.flash > 0 ? 2.0f : 0.0f);
      DrawOpts o;
      o.angle = sway;
      const float hookX = x + cw * 0.5f, hookY = y - 4.0f;
      r.draw(baked(mArt, r, "chandelier" + std::to_string(iw) + "x" + std::to_string(ih), iw, ih, cw * 0.5f, 0.0f,
               [cw, ch](cairo_t* cr) { paintChandelier(cr, cw, ch); }),
        hookX, hookY, o);
      const float a = sway * 3.14159f / 180.0f;
      for (const auto& c : chandelierCandles(cw, ch))
      {
        const float fx = hookX + c.first * std::cos(a) - c.second * std::sin(a);
        const float fy = hookY + c.first * std::sin(a) + c.second * std::cos(a);
        const float flick = 0.8f + 0.2f * float(hash2(int(c.first), frame / 3) % 100u) / 100.0f;
        drawGlow(r, mArt, fx, fy - 8.0f, 60.0f * flick, flameC, 0.35f);
        DrawOpts fo;
        fo.tint = flameC;
        fo.scale = 0.7f * flick;
        fo.angle = -sway * 2.0f;
        r.draw(flame, fx, fy + 2.0f, fo);
      }
      drawGlow(r, mArt, hookX, hookY + ch * 0.55f, cw * 0.7f, flameC, 0.18f);
    }
  }

  // Mirrors: the glass shows the other side's colour, sheens drifting over
  // it and the runner's reflection; a broken one goes green when shot.
  const auto& ca = mArt.runner(mCharacter);
  const float px = lerpCells(mPlayer.prevX, mPlayer.x, alpha) + 1.5f * kCellPx - camX;
  const float py = lerpCells(mPlayer.prevY + 1, mPlayer.y + 1, alpha) - camY;
  for (std::size_t i = 0; i < m.mirrors.size(); ++i)
  {
    const auto& mi = m.mirrors[i];
    const float x = sx(float(mi.x)), y = sy(float(mi.y));
    if (!visible(x, y, 128.0f, 192.0f))
      continue;
    const float gx = x + 18.0f, gy = y + 22.0f, gw = 92.0f, gh = 160.0f;
    const int kind = mi.kind == MirrorKind::Normal ? 0 : (mi.kind == MirrorKind::Broken ? 1 : 2);
    const bool third = mi.kind == MirrorKind::Broken && mi.third > 0;
    Color top, bottom;
    if (kind == 0)
    {
      top = mirrorView ? rgb(80, 40, 120) : rgb(30, 110, 70);
      bottom = mirrorView ? rgb(30, 14, 50) : rgb(10, 44, 30);
    }
    else if (third || kind == 2)
    {
      top = rgb(10, 40, 18);
      bottom = rgb(0, 6, 2);
    }
    else
    {
      top = rgb(40, 42, 52);
      bottom = rgb(14, 14, 20);
    }
    for (int k = 0; k < 8; ++k)
      r.fillRect(gx, gy + float(k) * gh / 8.0f, gw, gh / 8.0f + 1.0f, lerpColor(top, bottom, float(k) / 7.0f));
    clipTo(r, gx, gy, gw, gh);
    if (third || kind == 2)
    {
      // The third world: green lines on black, the crypt's arches.
      const float t = third ? float(mi.third) / float(kThirdWorld) : 0.5f;
      for (int k = 0; k < 5; ++k)
      {
        const float ax = gx + 10.0f + float(k) * 20.0f;
        r.drawLine(ax, gy + gh, ax, gy + 60.0f, 2.0f, withAlpha(kGreenGlow, int(140 * t + 40)));
        r.drawLine(ax, gy + 60.0f, ax + 10.0f, gy + 40.0f, 2.0f, withAlpha(kGreenGlow, int(140 * t + 40)));
        r.drawLine(ax + 10.0f, gy + 40.0f, ax + 20.0f, gy + 60.0f, 2.0f, withAlpha(kGreenGlow, int(140 * t + 40)));
      }
      for (int k = 0; k < 6; ++k)
        r.fillRect(gx, gy + gh - 20.0f - float(k) * 24.0f, gw, 1.5f, withAlpha(kGreenGlow, int(60 * t + 20)));
      drawGlow(r, mArt, gx + gw * 0.5f, gy + gh * 0.5f, 90.0f, kGreenGlow, 0.25f + 0.5f * t);
    }
    // Sheens drifting down the glass.
    for (int k = 0; k < 2; ++k)
    {
      const float ph = std::fmod(float(frame) * 0.6f + float(k) * 110.0f + float(mi.x * 37), 260.0f) - 60.0f;
      r.drawLine(gx + ph, gy, gx + ph - 50.0f, gy + gh, 10.0f - float(k) * 4.0f, rgba(255, 255, 255, 26));
    }
    // The runner's reflection when standing near.
    if (kind == 0 && std::abs(px - (x + 64.0f)) < 220.0f && std::abs(py - (y + 192.0f)) < 200.0f && !mPlayer.hidden &&
        m.split.on == false)
    {
      DrawOpts ro;
      ro.alpha = 0.45f;
      ro.tint = mirrorView ? rgb(200, 160, 255) : rgb(160, 255, 190);
      const float rx = x + 64.0f + (px - (x + 64.0f)) * 0.6f;
      r.draw(ca.idle[std::size_t((frame / 30) % 2)].get(-mPlayer.facing), rx, py - 4.0f, ro);
    }
    // The backwards egg: the reflection that stands still, then turns and
    // follows.
    if (m.reflection > 0 && int(i) == m.lastFlip)
    {
      DrawOpts ro;
      ro.alpha = std::min(1.0f, float(m.reflection) / 8.0f) * 0.75f;
      ro.tint = mirrorView ? rgb(210, 180, 255) : rgb(170, 255, 200);
      const float rx = float(m.reflX) * kCellPx + 1.5f * kCellPx - camX;
      const float ry = float(m.reflY + 1) * kCellPx - camY;
      const bool turned = m.reflection < 10;
      r.draw((turned ? ca.run[std::size_t((frame / 3) % kRunFrames)] : ca.idle[0]).get(turned ? -mPlayer.facing : mPlayer.facing),
        rx + (turned ? float(10 - m.reflection) * 6.0f * float(-mPlayer.facing) : 0.0f), ry, ro);
    }
    // Stepping through this one: it ripples bright.
    if (m.flip > 0 && m.flipTo == int(i))
    {
      const float t = 1.0f - std::abs(float(m.flip) - kMirrorShimmer / 2.0f) / (kMirrorShimmer / 2.0f);
      for (int k = 0; k < 6; ++k)
      {
        const float ry = gy + std::fmod(float(frame * 9 + k * 30), gh);
        r.fillRect(gx, ry, gw, 6.0f, withAlpha(rgb(230, 245, 255), int(160 * t)));
      }
    }
    unclip(r);
    r.draw(baked(mArt, r, "mirror" + std::to_string(kind), 128, 192, 0, 0, [kind](cairo_t* cr) { paintMirrorFrame(cr, kind); }),
      x, y);
    if (third)
      drawGlow(r, mArt, x + 64.0f, y + 100.0f, 140.0f, kGreenGlow, 0.3f + 0.3f * float((frame / 2) % 2));
  }

  // Levers: an iron handle in its slot, red until thrown, then green.
  const Texture& bolt = baked(mArt, r, "bolt", 64, 16, 2.0f, 8.0f, [](cairo_t* cr) { paintBoltTex(cr); });
  for (const auto& l : m.levers)
  {
    const float x = sx(float(l.x)), y = sy(float(l.y));
    if (!visible(x, y, 64.0f, 64.0f))
      continue;
    r.draw(baked(mArt, r, "leverplate", 64, 64, 0, 0, [](cairo_t* cr) { paintLeverPlate(cr); }), x, y);
    const float t = l.state == 1 ? 1.0f - float(l.swing) / 20.0f : 0.0f;
    const float a = (-150.0f + 120.0f * std::min(1.0f, t * 1.3f)) * 3.14159f / 180.0f;
    const float ex = x + 32.0f + std::cos(a) * 30.0f, ey = y + 40.0f + std::sin(a) * 30.0f;
    r.drawLine(x + 32.0f, y + 40.0f, ex, ey, 8.0f, kInk);
    r.drawLine(x + 32.0f, y + 40.0f, ex, ey, 5.0f, rgb(120, 120, 134));
    r.fillRect(ex - 6.0f, ey - 6.0f, 12.0f, 12.0f, l.state ? rgb(70, 200, 110) : rgb(200, 50, 50));
    r.fillRect(x + 28.0f, y + 36.0f, 8.0f, 8.0f, rgb(40, 40, 48));
    drawGlow(r, mArt, x + 32.0f, y + 10.0f, 24.0f, l.state ? kGreenGlow : rgb(255, 60, 60), 0.5f + 0.2f * std::sin(float(frame) * 0.2f));
  }

  // Portraits: the ghost painted in its frame while it is home; its eyes
  // follow you as you come close. Two sconces flank each one; in the real
  // manor their flames lean towards a ghost about to lunge.
  for (const auto& pt : m.portraits)
  {
    const float x = sx(float(pt.x)) - 16.0f, y = sy(float(pt.y)) - 20.0f;
    if (!visible(x - 40.0f, y, 240.0f, 170.0f))
      continue;
    const Enemy& g = mEnemies[std::size_t(pt.ghost)];
    const bool home = g.alive && g.attach == 0;
    const int d = std::max(std::abs(mPlayer.x / kCellsPerTile - pt.x), std::abs(mPlayer.y / kCellsPerTile - pt.y));
    const int flare = home && d <= kGhostWake + 3 ? 1 : 0;
    const int canvas = home ? 0 : 1;
    r.draw(baked(mArt, r, "portrait" + std::to_string(canvas) + std::to_string(flare), 160, 168, 0, 0,
             [canvas, flare](cairo_t* cr) { paintPortrait(cr, canvas, flare ? 1.0 : 0.0); }),
      x, y);
    if (flare)
      drawGlow(r, mArt, x + 80.0f, y + 66.0f, 50.0f, kGreenGlow, 0.25f + 0.15f * std::sin(float(frame) * 0.3f));
    // The sconces.
    float lean = 0.0f;
    float gxw = 0.0f;
    if (g.alive && g.attach != 0)
    {
      gxw = g.drawX * kCellPx + float(g.w) * kCellPx * 0.5f - camX;
      lean = (g.attach == 2 || g.attach == 3) ? 40.0f : 12.0f;
    }
    for (const float ox : {-26.0f, 170.0f})
    {
      const float cx = x + ox, cy = y + 70.0f;
      r.draw(baked(mArt, r, "sconce", 32, 48, 16.0f, 10.0f, [](cairo_t* cr) { paintSconce(cr); }), cx, cy);
      const float dir = lean > 0.0f ? (gxw < cx ? -1.0f : 1.0f) : 0.0f;
      const float flick = 0.85f + 0.15f * float(hash2(int(ox), frame / 3) % 100u) / 100.0f;
      DrawOpts fo;
      fo.tint = flameC;
      fo.angle = dir * lean + std::sin(float(frame) * 0.2f + ox) * 4.0f;
      fo.scale = flick;
      drawGlow(r, mArt, cx, cy - 10.0f, 46.0f, flameC, 0.3f);
      r.draw(flame, cx, cy, fo);
    }
  }

  // Lance's portrait (Secret 2), in the reflection: shot, it grows fangs and
  // winks.
  if (m.lanceX >= 0 && !offSide(m.lanceSide))
  {
    const float x = sx(float(m.lanceX)) - 16.0f, y = sy(float(m.lanceY)) - 20.0f;
    if (visible(x, y, 160.0f, 168.0f))
    {
      const bool wink = m.lanceShot && m.lanceWink > 0 && (m.lanceWink / 8) % 2 == 0;
      const int v = (m.lanceShot ? 1 : 0) + (wink ? 2 : 0);
      r.draw(baked(mArt, r, "lance" + std::to_string(v), 160, 168, 0, 0,
               [v](cairo_t* cr) { paintLancePortrait(cr, v & 1, (v & 2) != 0); }),
        x, y);
      if (m.lanceShot && m.lanceWink > 0)
        drawGlow(r, mArt, x + 80.0f, y + 80.0f, 110.0f, rgb(255, 60, 80), 0.2f * float(m.lanceWink) / 60.0f);
    }
  }

  // The coffin: the lid lifts a cell, two red eyes blink in the gap, it
  // slams; the sign on it changes its tune.
  if (m.coffinX >= 0)
  {
    const float cw = float(m.coffinW) * kTilePx;
    const float x = sx(float(m.coffinX)), y = sy(float(m.coffinY));
    if (visible(x - 40.0f, y - 120.0f, cw + 80.0f, 260.0f))
    {
      const int icw = int(cw);
      const int t = m.coffin > 0 ? 40 - m.coffin : -1;
      float lift = 0.0f;
      if (t >= 0)
        lift = t < 6 ? float(t) / 6.0f * 32.0f : (t < 34 ? 32.0f : std::max(0.0f, 32.0f * float(37 - t) / 3.0f));
      const float shake = (t >= 37) ? ((frame % 2) ? 3.0f : -3.0f) : 0.0f;
      r.draw(baked(mArt, r, "coffin" + std::to_string(icw), icw, 130, 0, 0, [cw](cairo_t* cr) { paintCoffin(cr, cw, false); }),
        x + shake, y);
      if (lift > 6.0f)
      {
        // The dark inside and the eyes.
        r.fillRect(x + 8.0f, y + 26.0f - lift, cw - 16.0f, lift, rgb(8, 2, 6));
        const bool blink = (t >= 14 && t < 16) || (t >= 24 && t < 26);
        if (!blink)
          for (const float ex : {cw * 0.28f, cw * 0.36f})
          {
            drawGlow(r, mArt, x + ex, y + 26.0f - lift * 0.5f, 18.0f, rgb(255, 30, 30), 0.8f);
            r.fillRect(x + ex - 5.0f, y + 24.0f - lift * 0.5f, 10.0f, 5.0f, rgb(255, 80, 70));
          }
      }
      r.draw(baked(mArt, r, "coffinlid" + std::to_string(icw), icw, 28, 0, 0, [cw](cairo_t* cr) { paintCoffin(cr, cw, true); }),
        x + shake, y - lift);
      if (t >= 34 && t < 38)
        for (int k = 0; k < 6; ++k)
          drawGlow(r, mArt, x + cw * float(k) / 5.0f, y + 26.0f, 26.0f, rgb(160, 150, 140), 0.3f);
      // The sign, propped on the lid.
      if (m.signX >= 0)
      {
        const float gx = sx(float(m.signX)) - 52.0f, gy = sy(float(m.signY)) + 62.0f - 70.0f - lift;
        r.draw(baked(mArt, r, "sign", 168, 70, 0, 0, [](cairo_t* cr) { paintSign(cr); }), gx + shake, gy);
        const bool angry = m.coffinWoke && m.coffin == 0;
        const Color ink = angry ? rgb(170, 10, 20) : rgb(40, 24, 16);
        if (angry)
        {
          r.drawText("I SAID", gx + 84.0f, gy + 6.0f, {17.0f, ink}, Align::Center);
          r.drawText("NIGHT SHIFT", gx + 84.0f, gy + 26.0f, {19.0f, ink}, Align::Center);
        }
        else
        {
          r.drawText("DO NOT DISTURB,", gx + 84.0f, gy + 8.0f, {14.0f, ink}, Align::Center);
          r.drawText("NIGHT SHIFT", gx + 84.0f, gy + 26.0f, {16.0f, ink}, Align::Center);
        }
      }
    }
  }

  // Ghost platforms: a pinned ghost's top glows where you can stand.
  for (const auto& pl : m.platforms)
  {
    const float x = sx(float(pl.bx)), y = sy(float(pl.by));
    if (!visible(x, y, 128.0f, 64.0f))
      continue;
    drawGlow(r, mArt, x + 64.0f, y + 6.0f, 90.0f, kGreenGlow, 0.3f);
    r.fillRect(x + 4.0f, y, 120.0f, 4.0f, rgba(220, 255, 230, 160));
  }

  // The refill box gathering itself back together.
  if (m.boltBox >= 0 && m.boltT > 0)
  {
    const auto& b = mBoxes[std::size_t(m.boltBox)];
    if (!b.alive)
    {
      const float cx = float(b.x) * kCellPx + 32.0f - camX, cy = float(b.y - 1) * kCellPx + 32.0f - camY;
      const float t = float(m.boltT) / float(kBoltRespawn);
      const int n = 24;
      for (int k = 0; k < int(t * float(n)); ++k)
      {
        const float a = float(k) / float(n) * 6.28318f - 1.5708f;
        r.fillRect(cx + std::cos(a) * 26.0f - 2.0f, cy + std::sin(a) * 26.0f - 2.0f, 4.0f, 4.0f, rgba(220, 230, 255, 160));
      }
      DrawOpts bo;
      bo.alpha = 0.25f + 0.5f * t;
      r.draw(bolt, cx - 30.0f, cy, bo);
    }
  }

  // Both Sides: each half's exit, lit while its runner stands on it.
  if (m.split.on)
  {
    for (int k = 0; k < 2; ++k)
    {
      const int ex = k == 0 ? m.split.topExitX : m.split.botExitX, ey = k == 0 ? m.split.topExitY : m.split.botExitY;
      const bool on = k == 0 ? m.split.topOn : m.split.botOn;
      const float x = sx(float(ex)), y = sy(float(ey + 1));
      if (!visible(x - 64.0f, y - 200.0f, 192.0f, 220.0f))
        continue;
      if (ex != mLevel->exitTx || ey != mLevel->exitTy)
        drawExit(r, mArt, mTheme, x - 32.0f, y, frame);
      drawGlow(r, mArt, x + 32.0f, y - 60.0f, on ? 150.0f : 90.0f, k == 0 ? kPurpleGlow : kGreenGlow,
        on ? 0.6f + 0.2f * std::sin(float(frame) * 0.4f) : 0.2f);
    }
  }

  // The reflection's green cast over everything behind the runner.
  if (mirrorView)
    modulate(r, rgb(170, 240, 195));
}

// The shimmer of a step through a mirror (ripples in the colour of the side
// you are going to, peaking as the side flips) and the lightning.
void World::drawManorFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)camX;
  (void)camY;
  (void)alpha;
  const auto& m = mManor;
  if (m.flip > 0)
  {
    const float t = 1.0f - std::abs(float(m.flip) - kMirrorShimmer / 2.0f) / (kMirrorShimmer / 2.0f);
    const bool toMirror = (m.flip > kMirrorShimmer / 2) == (m.side == kSideReal);
    const Color c = toMirror ? rgb(150, 255, 190) : rgb(200, 160, 255);
    r.fillRect(0.0f, 0.0f, float(kScreenW), float(kScreenH), withAlpha(c, int(80 * t)));
    for (int k = 0; k < 10; ++k)
    {
      const float y = std::fmod(float(k) * 53.0f + float(frame) * 7.0f, float(kScreenH));
      const float wob = std::sin(float(frame) * 0.5f + float(k)) * 40.0f * t;
      r.fillRect(wob, y, float(kScreenW), 6.0f + 8.0f * t, withAlpha(rgb(255, 255, 255), int(60 * t)));
    }
    if (t > 0.8f)
      r.fillRect(0.0f, 0.0f, float(kScreenW), float(kScreenH), withAlpha(rgb(255, 255, 255), int(90 * (t - 0.8f) * 5.0f)));
  }
  if (m.flash > 0)
    r.fillRect(0.0f, 0.0f, float(kScreenW), float(kScreenH), withAlpha(rgb(235, 235, 255), 22 * m.flash));
}

// The side you are on, and what Up does where you stand.
void World::drawManorHud(Renderer& r, int frame) const
{
  const auto& m = mManor;
  if (m.split.on)
    return; // drawSplit labels the halves
  const bool mirror = m.side == kSideMirror;
  const float x = float(kScreenW) - 24.0f, y = 92.0f;
  r.fillRect(x - 200.0f, y - 6.0f, 200.0f, 38.0f, rgba(8, 6, 22, 190));
  r.fillRect(x - 200.0f, y - 6.0f, 4.0f, 38.0f, mirror ? kGreenGlow : kPurpleGlow);
  r.drawText(mirror ? "REFLECTION" : "REAL MANOR", x - 12.0f, y, {22.0f, mirror ? kGreenGlow : kPurpleGlow, kInk},
    Align::Right);
  const Player& p = mPlayer;
  const bool standing = p.state == PlayerState::OnGround || p.state == PlayerState::Recovering;
  if (!standing || m.flip > 0)
    return;
  std::string hint;
  const int mi = manorMirrorAt(p);
  if (mi >= 0)
  {
    const ManorMirror& mr = m.mirrors[std::size_t(mi)];
    if (mr.kind == MirrorKind::Normal)
      hint = "UP: STEP THROUGH";
    else if (mr.kind == MirrorKind::Crypt)
      hint = "UP: BACK THROUGH";
    else if (mr.third > 0)
      hint = "UP: NOW!";
  }
  const int lv = manorLeverAt(p);
  if (lv >= 0 && m.levers[std::size_t(lv)].state == 0)
    hint = "UP: PULL THE LEVER";
  if (!hint.empty())
  {
    const float a = 0.7f + 0.3f * std::sin(float(frame) * 0.15f);
    r.drawText(hint, float(kScreenW) * 0.5f, float(kScreenH) - 120.0f, {24.0f, rgb(236, 230, 255), kInk},
      Align::Center, a);
  }
}

// Both Sides: the top runner in the top half of the screen, the bottom one
// in the bottom half, each with its own camera; one HUD over both.
void World::drawSplit(Renderer& r, int frame, float alpha) const
{
  const auto& s = mManor.split;
  const float halfH = float(kScreenH) / 2.0f;
  auto camFor = [&](const Player& p, int rowTop, int rowBottom, float& cx, float& cy) {
    const float mapW = float(mMap.width()) * kCellPx;
    cx = std::clamp(lerpCells(p.prevX, p.x, alpha) + 48.0f - float(kScreenW) / 2.0f, 0.0f, std::max(0.0f, mapW - float(kScreenW)));
    const float top = float(rowTop) * kTilePx, bottom = float(rowBottom + 1) * kTilePx;
    cy = std::clamp(lerpCells(p.prevY, p.y, alpha) - halfH * 0.6f, top, std::max(top, bottom - halfH));
  };
  SDL_Renderer* sdl = r.sdl();
  // Top half.
  SDL_Rect vp{0, 0, kScreenW, int(halfH)};
  SDL_RenderSetViewport(sdl, &vp);
  mSplitPass = 1;
  camFor(mPlayer, 0, s.divider - 1, mSplitCamX, mSplitCamY);
  draw(r, frame, alpha);
  // Bottom half: the twin drawn as the runner.
  vp = {0, int(halfH), kScreenW, int(halfH)};
  SDL_RenderSetViewport(sdl, &vp);
  mSplitPass = 2;
  auto& self = const_cast<World&>(*this);
  std::swap(self.mPlayer, self.mTwin);
  camFor(mPlayer, s.divider + 2, mLevel->height - 1, mSplitCamX, mSplitCamY);
  draw(r, frame, alpha);
  std::swap(self.mPlayer, self.mTwin);
  mSplitPass = 0;
  SDL_RenderSetViewport(sdl, nullptr);
  // The divider: the mirror's silvered edge, a shimmer running along it.
  r.fillRect(0.0f, halfH - 5.0f, float(kScreenW), 10.0f, rgb(30, 26, 40));
  r.fillRect(0.0f, halfH - 3.0f, float(kScreenW), 6.0f, rgb(200, 210, 240));
  const float run = std::fmod(float(frame) * 9.0f, float(kScreenW) + 300.0f) - 150.0f;
  drawGlow(r, mArt, run, halfH, 70.0f, rgb(255, 255, 255), 0.5f);
  r.fillRect(0.0f, halfH - 1.0f, float(kScreenW), 2.0f, rgba(255, 255, 255, 200));
  // The halves' labels and their exit lamps.
  auto label = [&](const char* text, float y, Color c, bool on) {
    r.fillRect(float(kScreenW) - 236.0f, y, 212.0f, 30.0f, rgba(8, 6, 22, 190));
    r.drawText(text, float(kScreenW) - 66.0f, y + 4.0f, {18.0f, c, kInk}, Align::Right);
    r.fillRect(float(kScreenW) - 52.0f, y + 7.0f, 16.0f, 16.0f, on ? c : rgb(50, 46, 60));
    if (on)
      drawGlow(r, mArt, float(kScreenW) - 44.0f, y + 15.0f, 24.0f, c, 0.7f);
  };
  label("MANOR", halfH - 40.0f, kPurpleGlow, s.topOn);
  label("REFLECTION", halfH + 10.0f, kGreenGlow, s.botOn);
  drawHud(r, frame);
}

} // namespace gr
