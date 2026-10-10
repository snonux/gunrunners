// Level 22, Dry Gulch: drawing (fuses, barrels, the drawbridge, metal,
// prizes, posters, the piano, the tonic's label, the duel's bell and the
// High Noon HUD). See world_west.cpp for the logic.
//
// Behind everything (drawWestBack): the dark rock behind the mine and the
// vault, the town's buildings (false fronts with their signs; the saloon is
// cut away to its wallpapered inside; the tiles inside a building are drawn
// again over it), the steeple and its bell, telegraph poles and the wire,
// the winch, the headframes, the water tower, the stagecoach, the hitching
// post, the windows the bandits pop up in, the piano, the bar counter and
// the balcony, the wanted posters, the gold revolver, the drawbridge, the
// fuses and the barrels, and High Noon's chalk marks. In front
// (drawWestFront): the window sills, the sparks, the Duelists' bells, the
// tonic's label and the piano's notes. Pieces are baked with Cairo the first
// time they are drawn and kept in the Art's sprite cache.

#include "game/world.hpp"

#include "assets/art.hpp"
#include "assets/enemy_art.hpp"
#include "assets/enemy_art_west.hpp"
#include "base/math.hpp"
#include "data/weapons.hpp"
#include "render/vector.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
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
constexpr Color kInk = rgb(34, 20, 12);
constexpr Color kGold = rgb(255, 206, 80);
constexpr Color kSpark = rgb(255, 190, 70);

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
  const std::string id = "~west/" + key;
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

void centredText(cairo_t* cr, const char* s, double cx, double baseline, double size)
{
  selectGameFont(cr);
  cairo_set_font_size(cr, size);
  cairo_text_extents_t e;
  cairo_text_extents(cr, s, &e);
  cairo_move_to(cr, cx - e.width * 0.5 - e.x_bearing, baseline);
  cairo_show_text(cr, s);
}

double textWidth(cairo_t* cr, const char* s, double size)
{
  selectGameFont(cr);
  cairo_set_font_size(cr, size);
  cairo_text_extents_t e;
  cairo_text_extents(cr, s, &e);
  return e.x_advance;
}

void bolt(cairo_t* cr, double x, double y, double rad)
{
  cairo_arc(cr, x, y, rad, 0, 2 * kPi);
  cairo_pattern_t* p = cairo_pattern_create_radial(x - rad * 0.4, y - rad * 0.4, 0.2, x, y, rad);
  cairo_pattern_add_color_stop_rgb(p, 0, 0.9, 0.88, 0.84);
  cairo_pattern_add_color_stop_rgb(p, 1, 0.25, 0.22, 0.2);
  cairo_set_source(cr, p);
  cairo_fill(cr);
  cairo_pattern_destroy(p);
}

float frac(std::uint32_t h) { return float(h % 1000u) / 1000.0f; }

void clipTo(Renderer& r, float x, float y, float w, float h)
{
  const SDL_Rect rc{int(std::floor(x)), int(std::floor(y)), std::max(0, int(std::ceil(w))), std::max(0, int(std::ceil(h)))};
  SDL_RenderSetClipRect(r.sdl(), &rc);
}

void unclip(Renderer& r) { SDL_RenderSetClipRect(r.sdl(), nullptr); }

// A sparkle: four thin rays and a hot middle.
void sparkle(Renderer& r, const Art& art, float x, float y, float size, Color c, float a)
{
  drawGlow(r, art, x, y, size * 1.4f, c, 0.6f * a);
  r.drawLine(x - size, y, x + size, y, 2.0f, withAlpha(rgb(255, 255, 255), int(230 * a)), Blend::Add);
  r.drawLine(x, y - size, x, y + size, 2.0f, withAlpha(rgb(255, 255, 255), int(230 * a)), Blend::Add);
  r.drawLine(x - size * 0.4f, y - size * 0.4f, x + size * 0.4f, y + size * 0.4f, 1.5f, withAlpha(c, int(200 * a)), Blend::Add);
  r.drawLine(x - size * 0.4f, y + size * 0.4f, x + size * 0.4f, y - size * 0.4f, 1.5f, withAlpha(c, int(200 * a)), Blend::Add);
}

// Weathered planks filling a rect, running across (horizontal) or down.
void planks(cairo_t* cr, double x, double y, double w, double h, Color c, double step, bool across, unsigned seed)
{
  std::uint32_t sd = seed * 747796405u + 1u;
  auto rnd = [&]() {
    sd ^= sd << 13;
    sd ^= sd >> 17;
    sd ^= sd << 5;
    return double(sd & 0xFFFF) / 65535.0;
  };
  cairo_save(cr);
  cairo_rectangle(cr, x, y, w, h);
  cairo_clip(cr);
  const double len = across ? h : w;
  for (double p = 0; p < len; p += step)
  {
    const Color pc = lerpColor(c, rnd() < 0.5 ? lighten(c, 0.2f) : darken(c, 0.18f), float(rnd()));
    if (across)
      cairo_rectangle(cr, x, y + p, w, step);
    else
      cairo_rectangle(cr, x + p, y, step, h);
    setColor(cr, pc);
    cairo_fill(cr);
    // The seam and a few streaks of grain.
    if (across)
      cairo_rectangle(cr, x, y + p + step - 1.4, w, 1.4);
    else
      cairo_rectangle(cr, x + p + step - 1.4, y, 1.4, h);
    setRgba(cr, darken(c, 0.55f), 0.6);
    cairo_fill(cr);
    for (int k = 0; k < 3; ++k)
    {
      if (across)
      {
        const double gx = x + rnd() * w, gy = y + p + 2 + rnd() * (step - 5);
        cairo_move_to(cr, gx, gy);
        cairo_line_to(cr, gx + 10 + rnd() * 40, gy + rnd() - 0.5);
      }
      else
      {
        const double gx = x + p + 2 + rnd() * (step - 5), gy = y + rnd() * h;
        cairo_move_to(cr, gx, gy);
        cairo_line_to(cr, gx + rnd() - 0.5, gy + 10 + rnd() * 40);
      }
      setRgba(cr, darken(c, 0.4f), 0.3);
      cairo_set_line_width(cr, 1.0);
      cairo_stroke(cr);
    }
  }
  cairo_restore(cr);
}

// --- Painters ----------------------------------------------------------------------------

struct BuildingStyle
{
  Color wall, front, board, letters;
  bool interior; // cut away to its inside (the saloon)
  bool gable;    // a pitched gable instead of a false front (the church)
  bool brick;    // adobe blocks (the bank)
};

BuildingStyle styleFor(const std::string& text, const std::string& kind)
{
  if (text == "SALOON")
    return {rgb(120, 34, 40), rgb(196, 150, 100), rgb(122, 36, 28), rgb(255, 214, 110), true, false, false};
  if (text == "CHURCH")
    return {rgb(232, 224, 206), rgb(238, 232, 216), rgb(70, 60, 54), rgb(255, 240, 200), false, true, false};
  if (text == "BANK")
    return {rgb(214, 176, 128), rgb(206, 166, 120), rgb(36, 74, 52), rgb(255, 214, 110), false, false, true};
  if (text == "ASSAY")
    return {rgb(162, 142, 120), rgb(170, 148, 124), rgb(60, 50, 46), rgb(240, 226, 196), false, false, false};
  if (kind == "sheriff" || text == "SHERIFF")
    return {rgb(176, 128, 84), rgb(188, 140, 94), rgb(70, 44, 30), rgb(255, 214, 110), false, false, false};
  return {rgb(170, 126, 84), rgb(186, 140, 96), rgb(80, 50, 34), rgb(250, 230, 190), false, false, false};
}

// A sheriff's star, five points, centred at (cx, cy).
void star(cairo_t* cr, double cx, double cy, double rad)
{
  for (int k = 0; k < 10; ++k)
  {
    const double a = -kPi * 0.5 + k * kPi / 5.0, rr = k % 2 ? rad * 0.45 : rad;
    if (k == 0)
      cairo_move_to(cr, cx + std::cos(a) * rr, cy + std::sin(a) * rr);
    else
      cairo_line_to(cr, cx + std::cos(a) * rr, cy + std::sin(a) * rr);
  }
  cairo_close_path(cr);
  linearFill(cr, cx, cy - rad, cx, cy + rad, rgb(255, 240, 160), rgb(200, 140, 30), true);
  outline(cr, 1.6);
  for (int k = 0; k < 5; ++k)
  {
    const double a = -kPi * 0.5 + k * 2 * kPi / 5.0;
    cairo_arc(cr, cx + std::cos(a) * rad, cy + std::sin(a) * rad, rad * 0.14, 0, 2 * kPi);
    setColor(cr, rgb(255, 230, 140));
    cairo_fill(cr);
  }
}

// A window with a sash, w x h at (x, y), glowing warm (lit) or showing the
// evening sky.
void sashWindow(cairo_t* cr, double x, double y, double w, double h, bool lit, Color trim)
{
  cairo_rectangle(cr, x - 5, y - 5, w + 10, h + 10);
  setColor(cr, trim);
  cairo_fill(cr);
  cairo_rectangle(cr, x, y, w, h);
  if (lit)
    linearFill(cr, 0, y, 0, y + h, rgb(255, 216, 140), rgb(214, 120, 60), true);
  else
    linearFill(cr, 0, y, 0, y + h, rgb(110, 70, 110), rgb(230, 130, 90), true);
  outline(cr, 1.6);
  cairo_move_to(cr, x + w * 0.5, y);
  cairo_line_to(cr, x + w * 0.5, y + h);
  cairo_move_to(cr, x, y + h * 0.5);
  cairo_line_to(cr, x + w, y + h * 0.5);
  outline(cr, 2.4, darken(trim, 0.4f));
  // A sill.
  cairo_rectangle(cr, x - 8, y + h + 4, w + 16, 5);
  setColor(cr, darken(trim, 0.2f));
  cairo_fill(cr);
}

// A building, the rect (0, 0) .. (W, H) translated by (8, 56): the false
// front (or gable) over its first row with the sign, then the wall below.
void paintBuilding(cairo_t* cr, double W, double H, const std::string& text, const std::string& kind)
{
  const BuildingStyle st = styleFor(text, kind);
  cairo_translate(cr, 8, 56);
  // The wall.
  cairo_rectangle(cr, 0, 56, W, H - 56);
  if (st.interior)
  {
    // Wallpaper: deep red with gold damask, a dark wainscot along the
    // bottom of each storey, gas lamps.
    setColor(cr, st.wall);
    cairo_fill(cr);
    for (double y = 70; y < H; y += 26)
      for (double x = 14 + std::fmod(y, 52.0) * 0.5; x < W - 8; x += 26)
      {
        cairo_save(cr);
        cairo_translate(cr, x, y);
        cairo_scale(cr, 1.0, 1.5);
        cairo_arc(cr, 0, 0, 3.2, 0, 2 * kPi);
        cairo_restore(cr);
        setRgba(cr, rgb(230, 170, 90), 0.35);
        cairo_fill(cr);
      }
    for (double x = 0; x < W; x += 52)
    {
      cairo_rectangle(cr, x, 56, 1.5, H - 56);
      setRgba(cr, rgb(60, 10, 14), 0.4);
      cairo_fill(cr);
    }
    planks(cr, 0, H - 90, W, 90, rgb(96, 58, 34), 30, false, 11u);
    cairo_rectangle(cr, 0, H - 94, W, 6);
    linearFill(cr, 0, H - 94, 0, H - 88, rgb(200, 150, 90), rgb(110, 70, 40));
    // Framed pictures and lamps.
    for (double x = 150; x < W - 100; x += 300)
    {
      cairo_rectangle(cr, x - 30, H - 220, 60, 46);
      linearFill(cr, 0, H - 220, 0, H - 174, rgb(230, 190, 110), rgb(150, 100, 40), true);
      outline(cr, 2.0);
      cairo_rectangle(cr, x - 24, H - 214, 48, 34);
      linearFill(cr, 0, H - 214, 0, H - 180, rgb(120, 140, 120), rgb(200, 150, 100));
      // Mountains in the painting.
      cairo_move_to(cr, x - 24, H - 184);
      cairo_line_to(cr, x - 8, H - 204);
      cairo_line_to(cr, x + 4, H - 192);
      cairo_line_to(cr, x + 14, H - 200);
      cairo_line_to(cr, x + 24, H - 184);
      cairo_close_path(cr);
      setColor(cr, rgb(110, 70, 60));
      cairo_fill(cr);
    }
    for (double x = 64; x < W - 40; x += 300)
      for (const double y : {H - 140.0, std::max(130.0, H - 360.0)})
      {
        radialGlow(cr, x, y, 46, rgb(255, 200, 110), 0.45);
        cairo_move_to(cr, x - 6, y + 14);
        cairo_line_to(cr, x + 6, y + 14);
        cairo_line_to(cr, x + 9, y - 2);
        cairo_line_to(cr, x - 9, y - 2);
        cairo_close_path(cr);
        linearFill(cr, 0, y - 2, 0, y + 14, rgb(255, 240, 190), rgb(230, 160, 70), true);
        outline(cr, 1.4);
        cairo_rectangle(cr, x - 2, y + 14, 4, 10);
        setColor(cr, rgb(180, 140, 60));
        cairo_fill(cr);
      }
  }
  else if (st.brick)
  {
    setColor(cr, st.wall);
    cairo_fill(cr);
    int row = 0;
    for (double y = 56; y < H; y += 22, ++row)
      for (double x = (row % 2) ? -22 : 0; x < W; x += 44)
      {
        cairo_rectangle(cr, x + 1.5, y + 1.5, 41, 19);
        setColor(cr, lerpColor(st.wall, (int(x + y) / 22) % 3 ? lighten(st.wall, 0.12f) : darken(st.wall, 0.1f), 0.6f));
        cairo_fill(cr);
      }
    // Columns either side of the door and the windows.
    for (const double x : {W * 0.5 - 70, W * 0.5 + 54})
    {
      cairo_rectangle(cr, x, 70, 16, H - 70);
      linearFill(cr, x, 0, x + 16, 0, rgb(250, 240, 220), rgb(170, 150, 120), true);
      outline(cr, 1.6);
    }
    for (const double x : {W * 0.18, W * 0.82})
    {
      sashWindow(cr, x - 30, H * 0.45, 60, 80, true, rgb(110, 80, 50));
      for (int k = 1; k < 5; ++k)
      {
        cairo_move_to(cr, x - 30 + k * 12, H * 0.45);
        cairo_line_to(cr, x - 30 + k * 12, H * 0.45 + 80);
      }
      outline(cr, 2.4, rgb(40, 36, 34));
    }
  }
  else
  {
    planks(cr, 0, 56, W, H - 56, st.wall, 18, true, unsigned(W + H));
    // Windows upstairs and down.
    const int n = std::max(1, int(W / 180));
    for (int i = 0; i < n; ++i)
    {
      const double x = W * (i + 0.5) / n;
      if (st.gable)
      {
        // An arched window of coloured glass, high over the door.
        const double wt = std::max(150.0, H - 340), wb = H - 150;
        cairo_move_to(cr, x - 30, wb);
        cairo_line_to(cr, x - 30, wt + 30);
        cairo_arc(cr, x, wt + 30, 30, kPi, 2 * kPi);
        cairo_line_to(cr, x + 30, wb);
        cairo_close_path(cr);
        cairo_save(cr);
        cairo_clip_preserve(cr);
        static const Color kGlass[] = {rgb(220, 60, 60), rgb(60, 120, 220), rgb(250, 200, 60), rgb(80, 180, 90)};
        for (int k = 0; k < 24; ++k)
        {
          cairo_move_to(cr, x + ((k % 3) - 1.5) * 20, wt + (k / 3) * 26);
          cairo_rel_line_to(cr, 20, 0);
          cairo_rel_line_to(cr, 0, 26);
          cairo_rel_line_to(cr, -20, 0);
          cairo_close_path(cr);
          setColor(cr, kGlass[(k * 3 + k / 3) % 4]);
          cairo_fill(cr);
        }
        radialGlow(cr, x, wt + 50, 40, rgb(255, 250, 220), 0.35);
        cairo_restore(cr);
        outline(cr, 5.0, rgb(60, 50, 44));
        cairo_move_to(cr, x - 10, wt + 6);
        cairo_line_to(cr, x - 10, wb);
        cairo_move_to(cr, x + 10, wt + 6);
        cairo_line_to(cr, x + 10, wb);
        cairo_move_to(cr, x - 30, (wt + wb) * 0.5);
        cairo_line_to(cr, x + 30, (wt + wb) * 0.5);
        outline(cr, 2.0, rgb(60, 50, 44));
      }
      else if (H > 260)
        sashWindow(cr, x - 30, 96, 60, 70, (i + int(W)) % 2 == 0, darken(st.front, 0.35f));
    }
    // The door.
    const double dx = st.gable ? W * 0.5 : W * 0.5 + (n % 2 ? 0.0 : W * 0.25 / n);
    cairo_rectangle(cr, dx - 30, H - 110, 60, 110);
    linearFill(cr, 0, H - 110, 0, H, darken(st.front, 0.35f), darken(st.front, 0.65f), true);
    outline(cr, 2.0);
    for (const double py : {H - 100.0, H - 50.0})
    {
      cairo_rectangle(cr, dx - 22, py, 18, 36);
      cairo_rectangle(cr, dx + 4, py, 18, 36);
      outline(cr, 1.4, darken(st.front, 0.75f));
    }
    cairo_arc(cr, dx + 20, H - 55, 3, 0, 2 * kPi);
    setColor(cr, kGold);
    cairo_fill(cr);
    if (kind == "sheriff" || text == "SHERIFF")
    {
      // A notice board of curling bills and the jail's barred window.
      cairo_rectangle(cr, W * 0.12, H - 150, 70, 54);
      setColor(cr, rgb(96, 64, 40));
      cairo_fill(cr);
      for (int k = 0; k < 3; ++k)
      {
        cairo_rectangle(cr, W * 0.12 + 6 + k * 21, H - 144 + (k % 2) * 6, 18, 24);
        setColor(cr, rgb(236, 222, 186));
        cairo_fill(cr);
      }
      sashWindow(cr, W * 0.8 - 30, H - 170, 60, 56, false, darken(st.front, 0.4f));
      for (int k = 1; k < 5; ++k)
      {
        cairo_move_to(cr, W * 0.8 - 30 + k * 12, H - 170);
        cairo_line_to(cr, W * 0.8 - 30 + k * 12, H - 114);
      }
      outline(cr, 3.0, rgb(40, 40, 46));
    }
  }
  // Corner posts.
  for (const double x : {0.0, W - 10})
  {
    cairo_rectangle(cr, x, 56, 10, H - 56);
    linearFill(cr, x, 0, x + 10, 0, lighten(st.front, 0.2f), darken(st.front, 0.35f), true);
    outline(cr, 1.4);
  }
  // The false front (or the church's gable).
  if (st.gable)
  {
    cairo_move_to(cr, -6, 70);
    cairo_line_to(cr, W * 0.5, -30);
    cairo_line_to(cr, W + 6, 70);
    cairo_close_path(cr);
    cairo_save(cr);
    cairo_clip_preserve(cr);
    planks(cr, -6, -30, W + 12, 100, st.front, 14, true, 5u);
    cairo_restore(cr);
    outline(cr, 2.4);
    cairo_move_to(cr, -10, 72);
    cairo_line_to(cr, W * 0.5, -36);
    cairo_line_to(cr, W + 10, 72);
    outline(cr, 6.0, rgb(150, 70, 50));
    cairo_arc(cr, W * 0.5, 30, 14, 0, 2 * kPi);
    linearFill(cr, 0, 16, 0, 44, rgb(255, 240, 190), rgb(220, 180, 100), true);
    outline(cr, 2.0);
  }
  else
  {
    const double step = std::min(36.0, W * 0.12);
    cairo_move_to(cr, -4, 72);
    cairo_line_to(cr, -4, 0);
    cairo_line_to(cr, step, 0);
    cairo_line_to(cr, step, -22);
    if (st.brick)
    {
      cairo_line_to(cr, W - step, -22);
    }
    else
    {
      cairo_line_to(cr, W * 0.5 - 50, -22);
      cairo_curve_to(cr, W * 0.5 - 30, -46, W * 0.5 + 30, -46, W * 0.5 + 50, -22);
      cairo_line_to(cr, W - step, -22);
    }
    cairo_line_to(cr, W - step, 0);
    cairo_line_to(cr, W + 4, 0);
    cairo_line_to(cr, W + 4, 72);
    cairo_close_path(cr);
    cairo_save(cr);
    cairo_clip_preserve(cr);
    if (st.brick)
      linearFill(cr, 0, -40, 0, 72, lighten(st.front, 0.1f), darken(st.front, 0.15f));
    else
      planks(cr, -4, -46, W + 8, 118, st.front, 14, true, 7u);
    cairo_restore(cr);
    outline(cr, 2.4);
    // The cornice.
    cairo_rectangle(cr, -8, 64, W + 16, 10);
    linearFill(cr, 0, 64, 0, 74, lighten(st.front, 0.25f), darken(st.front, 0.4f), true);
    outline(cr, 1.6);
  }
  // The sign board.
  const double size = 30;
  const double tw = textWidth(cr, text.c_str(), size);
  const double bw = std::min(W - 24, tw + 44), by = st.gable ? 56 : 8;
  if (!text.empty() && !st.gable)
  {
    roundedRect(cr, W * 0.5 - bw * 0.5, by, bw, 46, 5);
    linearFill(cr, 0, by, 0, by + 46, lighten(st.board, 0.15f), darken(st.board, 0.3f), true);
    outline(cr, 2.4);
    roundedRect(cr, W * 0.5 - bw * 0.5 + 5, by + 5, bw - 10, 36, 3);
    outline(cr, 1.4, withAlpha(st.letters, 200));
    setRgba(cr, rgb(0, 0, 0), 0.45);
    centredText(cr, text.c_str(), W * 0.5 + 2, by + 36, std::min(size, size * (bw - 20) / std::max(1.0, tw)));
    setColor(cr, st.letters);
    centredText(cr, text.c_str(), W * 0.5, by + 34, std::min(size, size * (bw - 20) / std::max(1.0, tw)));
    if (kind == "sheriff" || text == "SHERIFF")
      for (const double x : {W * 0.5 - bw * 0.5 - 22, W * 0.5 + bw * 0.5 + 22})
        if (x > 20 && x < W - 20)
          star(cr, x, by + 23, 16);
  }
  else if (!text.empty())
  {
    // The church's name over its door.
    setColor(cr, st.board);
    centredText(cr, text.c_str(), W * 0.5, H - 122, 16);
  }
}

// The steeple: a clapboard tower, the belfry round the bell's row (bellRow
// blocks from its top), a spire and a cross. (0, 0) .. (W, H) translated by
// (30, 190).
void paintSteeple(cairo_t* cr, double W, double H, int bellRow)
{
  cairo_translate(cr, 30, 190);
  const double tw = W * 0.8, tx = (W - tw) * 0.5;
  const double belfryTop = bellRow * 64.0 - 36, belfryBottom = bellRow * 64.0 + 84;
  const double cornice = belfryTop - 10;
  const Color white = rgb(236, 230, 214);
  // The tower body.
  cairo_rectangle(cr, tx, cornice, tw, H - cornice);
  cairo_save(cr);
  cairo_clip_preserve(cr);
  planks(cr, tx, cornice, tw, H - cornice, white, 14, true, 9u);
  linearFill(cr, tx, 0, tx + tw, 0, rgba(0, 0, 0, 0), rgba(60, 30, 40, 90));
  cairo_restore(cr);
  outline(cr, 2.4);
  // The belfry's opening: dark, arched, louvres at the bottom.
  const double ow = tw * 0.62, ox = W * 0.5 - ow * 0.5;
  cairo_move_to(cr, ox, belfryBottom);
  cairo_line_to(cr, ox, belfryTop + ow * 0.5);
  cairo_arc(cr, W * 0.5, belfryTop + ow * 0.5, ow * 0.5, kPi, 2 * kPi);
  cairo_line_to(cr, ox + ow, belfryBottom);
  cairo_close_path(cr);
  linearFill(cr, 0, belfryTop, 0, belfryBottom, rgb(40, 26, 30), rgb(80, 46, 40), true);
  outline(cr, 3.0);
  for (double y = belfryBottom - 22; y < belfryBottom; y += 7)
  {
    cairo_rectangle(cr, ox, y, ow, 3);
    setColor(cr, rgb(180, 170, 150));
    cairo_fill(cr);
  }
  // The clock under the belfry.
  const double cy = belfryBottom + 54;
  if (cy + 30 < H)
  {
    cairo_arc(cr, W * 0.5, cy, 26, 0, 2 * kPi);
    linearFill(cr, 0, cy - 26, 0, cy + 26, rgb(255, 250, 230), rgb(210, 200, 170), true);
    outline(cr, 2.4);
    for (int k = 0; k < 12; ++k)
    {
      const double a = k * kPi / 6.0;
      cairo_move_to(cr, W * 0.5 + std::cos(a) * 20, cy + std::sin(a) * 20);
      cairo_line_to(cr, W * 0.5 + std::cos(a) * 24, cy + std::sin(a) * 24);
    }
    outline(cr, 1.4);
    // High noon, near enough.
    cairo_move_to(cr, W * 0.5, cy);
    cairo_line_to(cr, W * 0.5 + 1, cy - 18);
    cairo_move_to(cr, W * 0.5, cy);
    cairo_line_to(cr, W * 0.5 + 4, cy - 12);
    outline(cr, 2.4, rgb(30, 26, 24));
  }
  // The cornice and the spire.
  cairo_rectangle(cr, tx - 10, cornice - 8, tw + 20, 12);
  linearFill(cr, 0, cornice - 8, 0, cornice + 4, rgb(250, 246, 236), rgb(170, 160, 140), true);
  outline(cr, 1.8);
  const double apex = cornice - 170;
  cairo_move_to(cr, tx - 4, cornice - 8);
  cairo_line_to(cr, W * 0.5, apex);
  cairo_line_to(cr, tx + tw + 4, cornice - 8);
  cairo_close_path(cr);
  linearFill(cr, tx, 0, tx + tw, 0, rgb(70, 60, 80), rgb(190, 110, 90), true);
  outline(cr, 2.4);
  for (int k = 1; k < 6; ++k)
  {
    const double y = apex + (cornice - 8 - apex) * k / 6.0;
    const double half = (tw * 0.5 + 4) * k / 6.0;
    cairo_move_to(cr, W * 0.5 - half, y);
    cairo_line_to(cr, W * 0.5 + half, y);
  }
  outline(cr, 1.0, rgba(30, 20, 30, 140));
  // The cross.
  cairo_rectangle(cr, W * 0.5 - 2.5, apex - 34, 5, 36);
  cairo_rectangle(cr, W * 0.5 - 12, apex - 26, 24, 5);
  linearFill(cr, 0, apex - 34, 0, apex, rgb(255, 230, 140), rgb(190, 130, 40), true);
  outline(cr, 1.2);
}

// The church bell, 64 x 72, hung from its yoke at the top middle.
void paintBell(cairo_t* cr)
{
  cairo_rectangle(cr, 10, 2, 44, 8);
  linearFill(cr, 0, 2, 0, 10, rgb(110, 76, 50), rgb(60, 40, 24), true);
  outline(cr, 1.6);
  cairo_move_to(cr, 32 - 10, 12);
  cairo_curve_to(cr, 32 - 12, 10, 32 + 12, 10, 32 + 10, 12);
  cairo_curve_to(cr, 32 + 14, 30, 32 + 16, 46, 32 + 26, 56);
  cairo_curve_to(cr, 32 + 28, 60, 32 + 22, 62, 32, 62);
  cairo_curve_to(cr, 32 - 22, 62, 32 - 28, 60, 32 - 26, 56);
  cairo_curve_to(cr, 32 - 16, 46, 32 - 14, 30, 32 - 10, 12);
  cairo_close_path(cr);
  linearFill(cr, 8, 0, 58, 0, rgb(255, 220, 130), rgb(140, 84, 30), true);
  outline(cr, 2.2);
  cairo_move_to(cr, 32 - 22, 54);
  cairo_curve_to(cr, 32 - 8, 57, 32 + 8, 57, 32 + 22, 54);
  outline(cr, 1.4, rgb(120, 70, 20));
  cairo_move_to(cr, 26, 18);
  cairo_curve_to(cr, 24, 30, 20, 44, 14, 54);
  outline(cr, 2.0, rgba(255, 250, 220, 180));
  // The clapper.
  cairo_arc(cr, 32, 66, 5, 0, 2 * kPi);
  linearFill(cr, 0, 61, 0, 71, rgb(140, 120, 100), rgb(60, 50, 40), true);
  outline(cr, 1.4);
}

// A bank's alarm bell on its ceiling plate, 64 x 56, anchored top middle.
void paintAlarmBell(cairo_t* cr)
{
  cairo_rectangle(cr, 14, 0, 36, 8);
  linearFill(cr, 0, 0, 0, 8, rgb(170, 170, 180), rgb(70, 70, 80), true);
  outline(cr, 1.4);
  cairo_rectangle(cr, 29, 8, 6, 8);
  setColor(cr, rgb(60, 60, 70));
  cairo_fill(cr);
  cairo_arc(cr, 32, 36, 20, kPi, 2 * kPi);
  cairo_line_to(cr, 52, 40);
  cairo_line_to(cr, 12, 40);
  cairo_close_path(cr);
  linearFill(cr, 12, 0, 52, 0, rgb(255, 230, 140), rgb(170, 110, 30), true);
  outline(cr, 2.0);
  cairo_arc(cr, 32, 26, 6, 0, 2 * kPi);
  setRgba(cr, rgb(255, 255, 240), 0.5);
  cairo_fill(cr);
  // The striker.
  cairo_move_to(cr, 32, 40);
  cairo_line_to(cr, 40, 50);
  outline(cr, 2.4, rgb(50, 50, 60));
  cairo_arc(cr, 41, 51, 3.5, 0, 2 * kPi);
  setColor(cr, rgb(90, 90, 100));
  cairo_fill(cr);
}

// A red TNT barrel, 64 x 72, standing on its bottom middle (32, 70).
void paintBarrel(cairo_t* cr)
{
  const Color red = rgb(206, 44, 34);
  cairo_move_to(cr, 12, 14);
  cairo_curve_to(cr, 6, 30, 6, 52, 12, 66);
  cairo_curve_to(cr, 24, 72, 40, 72, 52, 66);
  cairo_curve_to(cr, 58, 52, 58, 30, 52, 14);
  cairo_curve_to(cr, 40, 8, 24, 8, 12, 14);
  cairo_close_path(cr);
  cairo_save(cr);
  cairo_clip_preserve(cr);
  cairo_pattern_t* g = cairo_pattern_create_linear(6, 0, 58, 0);
  cairo_pattern_add_color_stop_rgb(g, 0, 0.42, 0.06, 0.05);
  cairo_pattern_add_color_stop_rgb(g, 0.35, redOf(lighten(red, 0.25f)) / 255.0, greenOf(lighten(red, 0.25f)) / 255.0,
    blueOf(lighten(red, 0.25f)) / 255.0);
  cairo_pattern_add_color_stop_rgb(g, 1, 0.36, 0.05, 0.04);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
  // Staves.
  for (const double x : {18.0, 26.0, 34.0, 42.0, 50.0})
  {
    cairo_move_to(cr, x, 10);
    cairo_curve_to(cr, x + (x - 32) * 0.12, 30, x + (x - 32) * 0.12, 50, x, 70);
  }
  setRgba(cr, rgb(60, 10, 8), 0.5);
  cairo_set_line_width(cr, 1.2);
  cairo_stroke(cr);
  cairo_restore(cr);
  outline(cr, 2.4);
  // Iron hoops.
  for (const double y : {20.0, 58.0})
  {
    cairo_move_to(cr, 9, y);
    cairo_curve_to(cr, 22, y + 4, 42, y + 4, 55, y);
    outline(cr, 4.6, rgb(40, 34, 32));
    cairo_move_to(cr, 10, y - 1);
    cairo_curve_to(cr, 22, y + 3, 42, y + 3, 54, y - 1);
    outline(cr, 1.2, rgb(150, 140, 130));
  }
  // The lid.
  cairo_save(cr);
  cairo_translate(cr, 32, 13);
  cairo_scale(cr, 20, 4.5);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_restore(cr);
  linearFill(cr, 0, 8, 0, 18, rgb(170, 120, 80), rgb(100, 64, 40), true);
  outline(cr, 1.6);
  // The stencil.
  setColor(cr, rgb(255, 244, 220));
  centredText(cr, "TNT", 32, 46, 15);
  cairo_move_to(cr, 16, 50);
  cairo_line_to(cr, 48, 50);
  outline(cr, 1.4, rgba(255, 244, 220, 200));
}

// A wanted poster, 116 x 150, its nail at (58, 9).
void paintPoster(cairo_t* cr, const std::string& text)
{
  // Torn, curling parchment.
  cairo_move_to(cr, 6, 4);
  cairo_line_to(cr, 110, 6);
  cairo_line_to(cr, 108, 70);
  cairo_line_to(cr, 112, 146);
  cairo_line_to(cr, 70, 142);
  cairo_line_to(cr, 62, 148);
  cairo_line_to(cr, 8, 145);
  cairo_line_to(cr, 4, 80);
  cairo_close_path(cr);
  cairo_pattern_t* g = cairo_pattern_create_radial(58, 70, 10, 58, 76, 90);
  cairo_pattern_add_color_stop_rgb(g, 0, 0.98, 0.92, 0.76);
  cairo_pattern_add_color_stop_rgb(g, 1, 0.8, 0.64, 0.42);
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  outline(cr, 1.6, rgb(110, 76, 40));
  setColor(cr, rgb(70, 36, 18));
  centredText(cr, "WANTED", 58, 30, 21);
  cairo_move_to(cr, 16, 36);
  cairo_line_to(cr, 100, 36);
  outline(cr, 1.2, rgb(110, 60, 30));
  // The portrait in its frame.
  cairo_rectangle(cr, 26, 42, 64, 62);
  setColor(cr, rgb(232, 210, 166));
  cairo_fill_preserve(cr);
  outline(cr, 1.6, rgb(90, 50, 24));
  cairo_save(cr);
  cairo_rectangle(cr, 26, 42, 64, 62);
  cairo_clip(cr);
  paintOutlawFace(cr, 58, 82, 38, true);
  cairo_restore(cr);
  setColor(cr, rgb(70, 36, 18));
  centredText(cr, "DEAD OR ALIVE", 58, 117, 9);
  // The bounty, split over two lines at the colon.
  const auto colon = text.find(':');
  const std::string a = colon == std::string::npos ? text : text.substr(0, colon + 1);
  std::string b = colon == std::string::npos ? std::string() : text.substr(colon + 1);
  while (!b.empty() && b[0] == ' ')
    b.erase(0, 1);
  setColor(cr, rgb(150, 30, 20));
  centredText(cr, a.c_str(), 58, b.empty() ? 136 : 129, 11);
  if (!b.empty())
    centredText(cr, b.c_str(), 58, 142, 12);
  // The nail.
  cairo_arc(cr, 58, 9, 3.5, 0, 2 * kPi);
  setColor(cr, rgb(80, 76, 72));
  cairo_fill(cr);
  cairo_arc(cr, 57, 8, 1.2, 0, 2 * kPi);
  setColor(cr, rgb(220, 220, 220));
  cairo_fill(cr);
}

// The gold-plated revolver on its hook, 80 x 80, the hook at (40, 8).
void paintGoldRevolver(cairo_t* cr)
{
  cairo_rectangle(cr, 36, 2, 8, 10);
  linearFill(cr, 36, 0, 44, 0, rgb(140, 100, 60), rgb(70, 46, 26), true);
  outline(cr, 1.2);
  cairo_move_to(cr, 40, 10);
  cairo_curve_to(cr, 40, 20, 48, 20, 48, 14);
  outline(cr, 2.4, rgb(70, 70, 76));
  // Hung by its trigger guard, barrel down.
  cairo_save(cr);
  cairo_translate(cr, 46, 22);
  cairo_rotate(cr, 1.2);
  paintRevolver(cr, 2.0, rgb(255, 200, 70));
  cairo_restore(cr);
}

// The upright piano, 272 x 176, standing on its bottom left (8, 176); the
// keyboard ledge runs at y 96 (the level draws the keys).
void paintPiano(cairo_t* cr)
{
  const Color wood = rgb(110, 54, 34);
  // The case.
  cairo_rectangle(cr, 8, 10, 256, 166);
  linearFill(cr, 8, 0, 264, 0, lighten(wood, 0.15f), darken(wood, 0.3f), true);
  outline(cr, 2.4);
  // The lid and its moulding.
  cairo_rectangle(cr, 2, 4, 268, 14);
  linearFill(cr, 0, 4, 0, 18, lighten(wood, 0.3f), darken(wood, 0.2f), true);
  outline(cr, 2.0);
  // Panels and the music desk.
  for (const double x : {24.0, 148.0})
  {
    roundedRect(cr, x, 26, 100, 56, 6);
    linearFill(cr, 0, 26, 0, 82, darken(wood, 0.1f), darken(wood, 0.35f), true);
    outline(cr, 1.4, darken(wood, 0.6f));
    cairo_move_to(cr, x + 20, 54);
    cairo_curve_to(cr, x + 40, 36, x + 60, 72, x + 80, 54);
    outline(cr, 1.6, rgba(230, 180, 100, 120));
  }
  cairo_rectangle(cr, 90, 64, 92, 20);
  linearFill(cr, 0, 64, 0, 84, rgb(250, 244, 226), rgb(210, 200, 170), true);
  outline(cr, 1.2);
  for (int k = 0; k < 4; ++k)
  {
    cairo_move_to(cr, 96, 68 + k * 4);
    cairo_line_to(cr, 176, 68 + k * 4);
  }
  outline(cr, 0.6, rgb(80, 70, 60));
  // The ledge under the keys and the key well.
  cairo_rectangle(cr, 4, 86, 264, 12);
  setColor(cr, rgb(30, 14, 10));
  cairo_fill(cr);
  cairo_rectangle(cr, 0, 112, 272, 10);
  linearFill(cr, 0, 112, 0, 122, lighten(wood, 0.3f), darken(wood, 0.3f), true);
  outline(cr, 1.6);
  // Legs and the pedals.
  for (const double x : {20.0, 244.0})
  {
    cairo_rectangle(cr, x, 122, 10, 50);
    linearFill(cr, x, 0, x + 10, 0, lighten(wood, 0.2f), darken(wood, 0.4f), true);
    outline(cr, 1.4);
  }
  for (const double x : {124.0, 142.0})
  {
    cairo_rectangle(cr, x, 168, 8, 5);
    setColor(cr, kGold);
    cairo_fill(cr);
  }
  // A whisky glass on top.
  cairo_rectangle(cr, 220, -8, 14, 14);
  linearFill(cr, 0, -8, 0, 6, rgba(255, 255, 255, 120), rgba(200, 120, 40, 220), true);
  outline(cr, 1.0);
}

// The window a bandit pops up in: frame, curtains, the dark inside;
// (0, 0) .. (w, h).
void paintBanditWindow(cairo_t* cr, double w, double h)
{
  cairo_rectangle(cr, 0, 0, w, h);
  linearFill(cr, 0, 0, 0, h, rgb(70, 44, 30), rgb(150, 90, 50), true);
  outline(cr, 2.0);
  cairo_rectangle(cr, 10, 10, w - 20, h - 10);
  linearFill(cr, 0, 10, 0, h, rgb(24, 14, 12), rgb(60, 34, 24), true);
  outline(cr, 1.6);
  // Curtains gathered at the sides.
  for (const int side : {0, 1})
  {
    const double x = side ? w - 10 : 10;
    const double dir = side ? -1 : 1;
    cairo_move_to(cr, x, 10);
    cairo_line_to(cr, x + dir * 26, 10);
    cairo_curve_to(cr, x + dir * 14, h * 0.4, x + dir * 18, h * 0.6, x + dir * 8, h);
    cairo_line_to(cr, x, h);
    cairo_close_path(cr);
    linearFill(cr, x, 0, x + dir * 26, 0, rgb(200, 50, 50), rgb(110, 20, 26), true);
    outline(cr, 1.2);
  }
  cairo_rectangle(cr, 0, 0, w, 10);
  linearFill(cr, 0, 0, 0, 10, rgb(200, 150, 100), rgb(110, 70, 40), true);
  outline(cr, 1.4);
}

// The sill in front of the bandit and the wall under it, w x h.
void paintSill(cairo_t* cr, double w, double h)
{
  cairo_rectangle(cr, 8, 10, w - 16, h - 10);
  cairo_save(cr);
  cairo_clip_preserve(cr);
  planks(cr, 8, 10, w - 16, h - 10, rgb(150, 104, 66), 12, true, 3u);
  cairo_restore(cr);
  outline(cr, 1.6);
  cairo_rectangle(cr, 0, 0, w, 12);
  linearFill(cr, 0, 0, 0, 12, rgb(214, 170, 120), rgb(120, 76, 44), true);
  outline(cr, 1.8);
}

// The drawbridge's plank, 64 wide and len blocks long, upright, with
// iron straps and a ring at its top; (8, 8) .. (72, 8 + len * 64).
void paintBridge(cairo_t* cr, int len)
{
  const double H = len * 64.0;
  cairo_translate(cr, 8, 8);
  cairo_rectangle(cr, 0, 0, 64, H);
  cairo_save(cr);
  cairo_clip_preserve(cr);
  planks(cr, 0, 0, 64, H, rgb(168, 116, 70), 16, true, 13u);
  cairo_restore(cr);
  outline(cr, 2.4);
  for (const double x : {8.0, 50.0})
  {
    cairo_rectangle(cr, x, 0, 6, H);
    linearFill(cr, x, 0, x + 6, 0, rgb(120, 116, 112), rgb(40, 38, 36), true);
    outline(cr, 1.0);
    for (double y = 10; y < H; y += 32)
      bolt(cr, x + 3, y, 2.0);
  }
  cairo_arc(cr, 32, -2, 7, 0, 2 * kPi);
  outline(cr, 3.0, rgb(70, 68, 66));
}

// A telegraph pole with its crossarm and glass insulators, 100 x (h + 12):
// the pole's middle at x 50, its top at y 6.
void paintPole(cairo_t* cr, double h)
{
  cairo_move_to(cr, 44, 6);
  cairo_line_to(cr, 56, 6);
  cairo_line_to(cr, 59, h + 12);
  cairo_line_to(cr, 41, h + 12);
  cairo_close_path(cr);
  cairo_save(cr);
  cairo_clip_preserve(cr);
  linearFill(cr, 41, 0, 59, 0, rgb(170, 128, 90), rgb(80, 54, 34), true);
  for (double y = 20; y < h; y += 23)
  {
    cairo_move_to(cr, 45, y);
    cairo_line_to(cr, 52, y + 14);
  }
  setRgba(cr, rgb(60, 36, 20), 0.35);
  cairo_set_line_width(cr, 1.0);
  cairo_stroke(cr);
  cairo_restore(cr);
  outline(cr, 1.8);
  cairo_rectangle(cr, 8, 18, 84, 9);
  linearFill(cr, 0, 18, 0, 27, rgb(170, 128, 90), rgb(90, 60, 36), true);
  outline(cr, 1.6);
  for (const double x : {16.0, 50.0, 84.0})
  {
    cairo_move_to(cr, x - 4, 18);
    cairo_line_to(cr, x - 3, 8);
    cairo_curve_to(cr, x - 3, 3, x + 3, 3, x + 3, 8);
    cairo_line_to(cr, x + 4, 18);
    cairo_close_path(cr);
    linearFill(cr, x - 4, 0, x + 4, 0, rgba(150, 230, 170, 230), rgba(40, 120, 70, 230), true);
    outline(cr, 1.0);
  }
  // Steps for the linesman.
  for (double y = 60; y < h - 40; y += 40)
  {
    cairo_rectangle(cr, (int(y) / 40) % 2 ? 56 : 34, y, 10, 4);
    setColor(cr, rgb(60, 60, 64));
    cairo_fill(cr);
  }
}

// The winch: a post with a rope drum and its crank, 100 x (h + 10), the
// post's top middle at (50, 0).
void paintWinch(cairo_t* cr, double h)
{
  cairo_rectangle(cr, 38, 0, 24, h + 10);
  cairo_save(cr);
  cairo_clip_preserve(cr);
  linearFill(cr, 38, 0, 62, 0, rgb(176, 130, 86), rgb(90, 60, 36), true);
  cairo_restore(cr);
  outline(cr, 1.8);
  // The drum.
  cairo_rectangle(cr, 14, 18, 72, 30);
  linearFill(cr, 0, 18, 0, 48, rgb(190, 150, 100), rgb(100, 66, 40), true);
  outline(cr, 1.8);
  for (double x = 18; x < 84; x += 5)
  {
    cairo_move_to(cr, x, 20);
    cairo_line_to(cr, x + 3, 46);
  }
  outline(cr, 1.4, rgb(210, 180, 120));
  for (const double x : {14.0, 86.0})
  {
    cairo_save(cr);
    cairo_translate(cr, x, 33);
    cairo_scale(cr, 5, 19);
    cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
    cairo_restore(cr);
    linearFill(cr, 0, 14, 0, 52, rgb(120, 116, 112), rgb(50, 48, 46), true);
    outline(cr, 1.4);
  }
  // The crank.
  cairo_move_to(cr, 90, 33);
  cairo_line_to(cr, 96, 60);
  cairo_line_to(cr, 92, 64);
  outline(cr, 4.0, rgb(60, 58, 56));
  // A pawl and a bolt.
  bolt(cr, 50, 70, 4);
}

// A mine headframe: two splayed timber legs, braces and the sheave wheel
// over the shaft; (0, 0) .. (W, H) translated by (10, 50).
void paintHeadframe(cairo_t* cr, double W, double H)
{
  cairo_translate(cr, 10, 50);
  const Color timber = rgb(130, 88, 56);
  auto beam = [&](double x0, double y0, double x1, double y1, double wd) {
    cairo_move_to(cr, x0, y0);
    cairo_line_to(cr, x1, y1);
    setColor(cr, kInk);
    cairo_set_line_width(cr, wd + 3);
    cairo_stroke(cr);
    cairo_move_to(cr, x0, y0);
    cairo_line_to(cr, x1, y1);
    setColor(cr, timber);
    cairo_set_line_width(cr, wd);
    cairo_stroke(cr);
  };
  const double cx = W * 0.5;
  beam(14, H, cx - 26, 6, 14);
  beam(W - 14, H, cx + 26, 6, 14);
  for (const double f : {0.3, 0.62})
  {
    const double y = 6 + (H - 6) * f;
    const double xl = 14 + (cx - 26 - 14) * (1 - f), xr = W - 14 - (W - 14 - cx - 26) * (1 - f);
    beam(xl, y, xr, y, 9);
  }
  beam(26, H * 0.95, cx + 18, H * 0.34, 7);
  beam(W - 26, H * 0.95, cx - 18, H * 0.34, 7);
  beam(cx - 40, 8, cx + 40, 8, 10);
  // The sheave wheel and its cable.
  cairo_move_to(cr, cx + 26, -24);
  cairo_line_to(cr, cx + 26, H + 40);
  outline(cr, 2.0, rgb(40, 40, 44));
  cairo_arc(cr, cx, -24, 28, 0, 2 * kPi);
  outline(cr, 6.0, rgb(50, 48, 50));
  cairo_arc(cr, cx, -24, 28, 0, 2 * kPi);
  outline(cr, 2.0, rgb(150, 146, 140));
  for (int k = 0; k < 6; ++k)
  {
    const double a = k * kPi / 3.0;
    cairo_move_to(cr, cx, -24);
    cairo_line_to(cr, cx + std::cos(a) * 26, -24 + std::sin(a) * 26);
  }
  outline(cr, 2.4, rgb(70, 68, 70));
  cairo_arc(cr, cx, -24, 5, 0, 2 * kPi);
  setColor(cr, rgb(120, 116, 112));
  cairo_fill(cr);
}

// The water tower's trestle legs, (0, 0) .. (W, H) translated by (10, 0).
void paintTowerLegs(cairo_t* cr, double W, double H)
{
  cairo_translate(cr, 10, 0);
  const Color timber = rgb(140, 96, 60);
  const double xs[4] = {14, W * 0.36, W * 0.64, W - 14};
  auto beam = [&](double x0, double y0, double x1, double y1, double wd) {
    cairo_move_to(cr, x0, y0);
    cairo_line_to(cr, x1, y1);
    setColor(cr, kInk);
    cairo_set_line_width(cr, wd + 3);
    cairo_stroke(cr);
    cairo_move_to(cr, x0, y0);
    cairo_line_to(cr, x1, y1);
    setColor(cr, timber);
    cairo_set_line_width(cr, wd);
    cairo_stroke(cr);
  };
  for (int i = 0; i < 3; ++i)
    for (const double f : {0.0, 0.5})
    {
      const double y0 = H * f + 4, y1 = H * (f + 0.5) - 4;
      beam(xs[i] + 2, y0, xs[i + 1] - 2, y1, 4);
      beam(xs[i + 1] - 2, y0, xs[i] + 2, y1, 4);
    }
  beam(4, H * 0.5, W - 4, H * 0.5, 7);
  beam(4, 6, W - 4, 6, 9);
  for (int i = 0; i < 4; ++i)
    beam(xs[i] + (i < 2 ? 6 : -6), 0, xs[i] + (i < 2 ? -4 : 4), H, 12);
}

// The wooden water tank over the legs, (0, 0) .. (W, H) translated by
// (12, 30): staves, iron hoops, a shallow conical cap.
void paintTank(cairo_t* cr, double W, double H)
{
  cairo_translate(cr, 12, 30);
  cairo_move_to(cr, 4, 6);
  cairo_line_to(cr, W - 4, 6);
  cairo_line_to(cr, W, H);
  cairo_line_to(cr, 0, H);
  cairo_close_path(cr);
  cairo_save(cr);
  cairo_clip_preserve(cr);
  planks(cr, 0, 6, W, H, rgb(150, 104, 66), 22, false, 17u);
  linearFill(cr, 0, 0, W, 0, rgba(255, 220, 160, 40), rgba(40, 10, 0, 110));
  cairo_restore(cr);
  outline(cr, 2.4);
  for (int k = 0; k < 4; ++k)
  {
    const double y = 18 + (H - 30) * k / 3.0;
    cairo_move_to(cr, 1, y);
    cairo_line_to(cr, W - 1, y);
    outline(cr, 5.0, rgb(40, 36, 34));
    cairo_move_to(cr, 2, y - 1.5);
    cairo_line_to(cr, W - 2, y - 1.5);
    outline(cr, 1.2, rgb(160, 150, 140));
  }
  // The cap.
  cairo_move_to(cr, -8, 8);
  cairo_line_to(cr, W * 0.5, -22);
  cairo_line_to(cr, W + 8, 8);
  cairo_close_path(cr);
  linearFill(cr, 0, -22, W, 8, rgb(170, 120, 80), rgb(90, 56, 34), true);
  outline(cr, 2.2);
  // Its spout.
  cairo_rectangle(cr, W - 10, H * 0.7, 30, 10);
  linearFill(cr, 0, H * 0.7, 0, H * 0.7 + 10, rgb(110, 104, 100), rgb(50, 48, 46), true);
  outline(cr, 1.4);
}

// A stagecoach, (0, 0) .. (W, H) translated by (10, 30), facing right.
void paintStagecoach(cairo_t* cr, double W, double H)
{
  cairo_translate(cr, 10, 30);
  const double s = W / 320.0;
  auto wheel = [&](double cx, double cy, double rr) {
    cairo_arc(cr, cx, cy, rr, 0, 2 * kPi);
    outline(cr, 7 * s, rgb(40, 34, 32));
    cairo_arc(cr, cx, cy, rr - 3 * s, 0, 2 * kPi);
    outline(cr, 4 * s, rgb(200, 150, 70));
    for (int k = 0; k < 12; ++k)
    {
      const double a = k * kPi / 6.0;
      cairo_move_to(cr, cx + std::cos(a) * 7 * s, cy + std::sin(a) * 7 * s);
      cairo_line_to(cr, cx + std::cos(a) * (rr - 4 * s), cy + std::sin(a) * (rr - 4 * s));
    }
    outline(cr, 3 * s, rgb(190, 130, 60));
    cairo_arc(cr, cx, cy, 9 * s, 0, 2 * kPi);
    linearFill(cr, cx, cy - 9 * s, cx, cy + 9 * s, rgb(230, 190, 110), rgb(130, 80, 30), true);
    outline(cr, 1.6);
  };
  // The far wheels, darker.
  wheel(98 * s, H - 46 * s, 44 * s);
  wheel(262 * s, H - 36 * s, 34 * s);
  cairo_rectangle(cr, 0, 0, W, H);
  cairo_set_source_rgba(cr, 0.1, 0.05, 0.05, 0.0);
  cairo_new_path(cr);
  // Leaf springs.
  for (const double x : {98.0, 262.0})
  {
    cairo_move_to(cr, (x - 30) * s, H - 96 * s);
    cairo_curve_to(cr, (x - 10) * s, H - 80 * s, (x + 10) * s, H - 80 * s, (x + 30) * s, H - 96 * s);
    outline(cr, 5 * s, rgb(50, 46, 44));
  }
  // The body: a rounded box swung low between the wheels.
  cairo_move_to(cr, 56 * s, 70 * s);
  cairo_line_to(cr, 250 * s, 70 * s);
  cairo_curve_to(cr, 262 * s, 110 * s, 258 * s, 150 * s, 240 * s, H - 92 * s);
  cairo_line_to(cr, 70 * s, H - 92 * s);
  cairo_curve_to(cr, 48 * s, 150 * s, 44 * s, 110 * s, 56 * s, 70 * s);
  cairo_close_path(cr);
  linearFill(cr, 0, 70 * s, 0, H - 92 * s, rgb(190, 40, 36), rgb(100, 16, 18), true);
  outline(cr, 2.6);
  cairo_move_to(cr, 58 * s, 80 * s);
  cairo_line_to(cr, 248 * s, 80 * s);
  cairo_move_to(cr, 66 * s, H - 100 * s);
  cairo_line_to(cr, 242 * s, H - 100 * s);
  outline(cr, 2.4 * s, kGold);
  // The door and its window.
  cairo_rectangle(cr, 118 * s, 88 * s, 76 * s, H - 196 * s);
  outline(cr, 2.0 * s, rgb(230, 180, 80));
  cairo_rectangle(cr, 126 * s, 94 * s, 60 * s, 44 * s);
  linearFill(cr, 0, 94 * s, 0, 138 * s, rgb(60, 30, 30), rgb(150, 80, 50), true);
  outline(cr, 1.6);
  cairo_move_to(cr, 126 * s, 94 * s);
  cairo_curve_to(cr, 140 * s, 110 * s, 136 * s, 124 * s, 128 * s, 138 * s);
  cairo_line_to(cr, 126 * s, 138 * s);
  cairo_close_path(cr);
  setColor(cr, rgb(220, 190, 120));
  cairo_fill(cr);
  setColor(cr, kGold);
  centredText(cr, "OVERLAND STAGE", 156 * s, 160 * s, 13 * s);
  // The roof, luggage lashed on top.
  cairo_rectangle(cr, 50 * s, 60 * s, 208 * s, 12 * s);
  linearFill(cr, 0, 60 * s, 0, 72 * s, rgb(70, 40, 30), rgb(30, 16, 12), true);
  outline(cr, 1.6);
  roundedRect(cr, 80 * s, 26 * s, 60 * s, 34 * s, 4 * s);
  linearFill(cr, 0, 26 * s, 0, 60 * s, rgb(150, 100, 60), rgb(80, 50, 30), true);
  outline(cr, 1.6);
  roundedRect(cr, 146 * s, 36 * s, 50 * s, 24 * s, 4 * s);
  linearFill(cr, 0, 36 * s, 0, 60 * s, rgb(100, 110, 80), rgb(50, 56, 40), true);
  outline(cr, 1.6);
  for (const double x : {96.0, 124.0, 170.0})
  {
    cairo_rectangle(cr, x * s, 26 * s, 4 * s, 34 * s);
    setColor(cr, rgb(60, 36, 20));
    cairo_fill(cr);
  }
  // The driver's box and footboard.
  cairo_move_to(cr, 250 * s, 70 * s);
  cairo_line_to(cr, 300 * s, 92 * s);
  cairo_line_to(cr, 300 * s, 100 * s);
  cairo_line_to(cr, 252 * s, 100 * s);
  cairo_close_path(cr);
  linearFill(cr, 0, 70 * s, 0, 100 * s, rgb(120, 70, 40), rgb(60, 34, 18), true);
  outline(cr, 1.8);
  cairo_rectangle(cr, 236 * s, 50 * s, 30 * s, 20 * s);
  linearFill(cr, 0, 50 * s, 0, 70 * s, rgb(130, 80, 46), rgb(70, 40, 22), true);
  outline(cr, 1.6);
  // The near wheels.
  wheel(98 * s, H - 46 * s, 46 * s);
  wheel(262 * s, H - 36 * s, 36 * s);
}

// A hitching post, 72 x 64 on its block: two posts, a rail and a ring.
void paintHitchingPost(cairo_t* cr)
{
  for (const double x : {10.0, 56.0})
  {
    cairo_rectangle(cr, x, 18, 9, 46);
    linearFill(cr, x, 0, x + 9, 0, rgb(180, 134, 90), rgb(90, 60, 36), true);
    outline(cr, 1.6);
  }
  cairo_rectangle(cr, 4, 16, 64, 9);
  linearFill(cr, 0, 16, 0, 25, rgb(196, 150, 104), rgb(110, 74, 44), true);
  outline(cr, 1.6);
  cairo_arc(cr, 36, 31, 6, 0, 2 * kPi);
  outline(cr, 2.2, rgb(70, 66, 64));
}

// The rock behind the mine's tunnels and the vault, 64 x 64.
void paintMineWall(cairo_t* cr, int variant)
{
  cairo_rectangle(cr, 0, 0, 64, 64);
  linearFill(cr, 0, 0, 0, 64, rgb(70, 40, 30), rgb(52, 30, 24));
  std::uint32_t sd = 2237u + unsigned(variant) * 31u;
  auto rnd = [&]() {
    sd ^= sd << 13;
    sd ^= sd >> 17;
    sd ^= sd << 5;
    return double(sd & 0xFFFF) / 65535.0;
  };
  for (int k = 0; k < 7; ++k)
  {
    const double x = rnd() * 64, y = rnd() * 64, rr = 6 + rnd() * 12;
    cairo_save(cr);
    cairo_translate(cr, x, y);
    cairo_scale(cr, 1.3, 0.8);
    cairo_arc(cr, 0, 0, rr, 0, 2 * kPi);
    cairo_restore(cr);
    setRgba(cr, rnd() < 0.5 ? rgb(96, 58, 42) : rgb(40, 22, 18), 0.4);
    cairo_fill(cr);
  }
  for (int k = 0; k < 3; ++k)
  {
    const double x = rnd() * 60, y = rnd() * 60;
    cairo_move_to(cr, x, y);
    cairo_line_to(cr, x + 6 + rnd() * 8, y + 4 + rnd() * 6);
    setRgba(cr, rgb(24, 12, 10), 0.6);
    cairo_set_line_width(cr, 1.2);
    cairo_stroke(cr);
  }
}

// The brass cap at the end of a fuse, 22 x 16, anchored at its middle.
void paintCap(cairo_t* cr)
{
  roundedRect(cr, 2, 3, 18, 10, 3);
  linearFill(cr, 0, 3, 0, 13, rgb(255, 230, 140), rgb(150, 100, 30), true);
  outline(cr, 1.4);
  cairo_rectangle(cr, 5, 3, 2, 10);
  setRgba(cr, rgb(255, 255, 230), 0.6);
  cairo_fill(cr);
}

// The six-shooter's cylinder, 64 x 64: the drum with six bores round the
// pin, flutes between them. The rounds are drawn over it.
void paintCylinder(cairo_t* cr)
{
  cairo_arc(cr, 32, 32, 29, 0, 2 * kPi);
  cairo_pattern_t* g = cairo_pattern_create_radial(24, 22, 4, 32, 32, 30);
  cairo_pattern_add_color_stop_rgb(g, 0, 0.86, 0.87, 0.9);
  cairo_pattern_add_color_stop_rgb(g, 1, 0.3, 0.31, 0.36);
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  outline(cr, 2.0, rgb(16, 14, 20));
  for (int k = 0; k < 6; ++k)
  {
    const double a = -kPi * 0.5 + (k + 0.5) * kPi / 3.0;
    cairo_arc(cr, 32 + std::cos(a) * 25, 32 + std::sin(a) * 25, 4, 0, 2 * kPi);
    setColor(cr, rgb(60, 62, 72));
    cairo_fill(cr);
  }
  for (int k = 0; k < 6; ++k)
  {
    const double a = -kPi * 0.5 + k * kPi / 3.0;
    cairo_arc(cr, 32 + std::cos(a) * 16, 32 + std::sin(a) * 16, 7, 0, 2 * kPi);
    setColor(cr, rgb(20, 20, 26));
    cairo_fill(cr);
  }
  cairo_arc(cr, 32, 32, 4, 0, 2 * kPi);
  setColor(cr, rgb(140, 142, 150));
  cairo_fill(cr);
}

// A round seen from behind in its bore, 16 x 16.
void paintRound(cairo_t* cr)
{
  cairo_arc(cr, 8, 8, 6.5, 0, 2 * kPi);
  linearFill(cr, 2, 2, 14, 14, rgb(255, 230, 140), rgb(170, 110, 30), true);
  outline(cr, 1.0, rgb(60, 40, 10));
  cairo_arc(cr, 8, 8, 2.2, 0, 2 * kPi);
  setColor(cr, rgb(200, 190, 180));
  cairo_fill(cr);
}

// A small bell icon, 32 x 32, hung from its top middle.
void paintBellIcon(cairo_t* cr)
{
  cairo_move_to(cr, 10, 24);
  cairo_curve_to(cr, 10, 8, 22, 8, 22, 24);
  cairo_line_to(cr, 26, 27);
  cairo_line_to(cr, 6, 27);
  cairo_close_path(cr);
  linearFill(cr, 6, 0, 26, 0, rgb(255, 230, 140), rgb(170, 110, 30), true);
  outline(cr, 1.6);
  cairo_arc(cr, 16, 29, 2.5, 0, 2 * kPi);
  setColor(cr, rgb(120, 80, 30));
  cairo_fill(cr);
  cairo_move_to(cr, 16, 2);
  cairo_line_to(cr, 16, 8);
  outline(cr, 2.0);
}

// A cowboy hat on its own (a bandit's, rising behind a sill), 72 x 44.
void paintHat(cairo_t* cr) { paintCowboyHat(cr, 36, 34, 60, rgb(34, 28, 28), -0.05); }

// --- Geometry -------------------------------------------------------------------------------

const WestDeco* decoAt(const WestState& w, const char* kind, int bx, int by)
{
  for (const auto& d : w.decos)
    if (d.kind == kind && bx >= d.x0 && bx <= d.x1 && by >= d.y0 && by <= d.y1)
      return &d;
  return nullptr;
}

// The wire's height (world px) at world x, sagging between its supports
// (its ends and any pole under it). -1 if x is not under a wire.
float wireY(const WestState& w, float x, int row)
{
  for (const auto& d : w.decos)
  {
    if (d.kind != "wire" || d.y0 != row)
      continue;
    const float x0 = float(d.x0) * kTilePx + 32.0f, x1 = float(d.x1) * kTilePx + 32.0f;
    if (x < x0 - 0.5f || x > x1 + 0.5f)
      continue;
    float a = x0, b = x1;
    for (const auto& p : w.decos)
      if (p.kind == "pole" && p.y0 <= row && p.y1 >= row)
      {
        const float px = float(p.x0) * kTilePx + 32.0f;
        if (px <= x && px > a)
          a = px;
        if (px >= x && px < b)
          b = px;
      }
    const float u = b > a ? (x - a) / (b - a) : 0.0f;
    const float sag = std::min(26.0f, (b - a) * 0.025f);
    return float(row) * kTilePx + 10.0f + sag * 4.0f * u * (1.0f - u);
  }
  return -1.0f;
}

} // namespace

// --- Behind everything --------------------------------------------------------------------

void World::drawWestBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& w = mWest;
  const bool western = mLevel->themeKey == "western_gulch";
  auto sx = [&](float bx) { return bx * kTilePx - camX; };
  auto sy = [&](float by) { return by * kTilePx - camY; };

  // The dark rock behind the mine, the gully and the vault: everything
  // empty under the street.
  if (western)
  {
    const int ground = mLevel->startTy + 1;
    const int tx0 = std::max(0, int(camX / kTilePx) - 1), ty0 = std::max(ground, int(camY / kTilePx) - 1);
    const int tx1 = std::min(mLevel->width - 1, int((camX + float(kScreenW)) / kTilePx) + 1);
    const int ty1 = std::min(mLevel->height - 1, int((camY + float(kScreenH)) / kTilePx) + 1);
    const Texture* walls[2] = {
      &baked(mArt, r, "minewall0", 64, 64, 0, 0, [](cairo_t* cr) { paintMineWall(cr, 0); }),
      &baked(mArt, r, "minewall1", 64, 64, 0, 0, [](cairo_t* cr) { paintMineWall(cr, 1); })};
    for (int ty = ty0; ty <= ty1; ++ty)
      for (int tx = tx0; tx <= tx1; ++tx)
      {
        const Tile t = mMap.block(tx, ty);
        if (t != Tile::Empty && t != Tile::Ladder && t != Tile::Spikes && t != Tile::Platform)
          continue;
        const float x = sx(float(tx)), y = sy(float(ty));
        r.draw(*walls[hash2(tx, ty) % 2u], x, y);
        if (t == Tile::Ladder)
          r.draw(mArt.ladder, x, y);
        else if (t == Tile::Platform)
          r.draw(mArt.platform, x, y);
        else if (t == Tile::Spikes)
          r.draw(mArt.spikes, x, y);
        // A lantern under the timbers every so often.
        if (t == Tile::Empty && mMap.block(tx, ty - 1) == Tile::Solid && tx % 9 == 4)
        {
          r.drawLine(x + 32, y, x + 32, y + 14, 2.0f, rgb(40, 30, 26));
          r.fillRect(x + 25, y + 14, 14, 18, rgb(60, 44, 30));
          r.fillRect(x + 28, y + 17, 8, 12, rgb(255, 210, 120));
          const float flick = 0.45f + 0.15f * frac(hash2(tx, frame / 4));
          drawGlow(r, mArt, x + 32, y + 24, 110.0f, rgb(255, 170, 80), flick);
        }
      }
    // What sits in front of that rock (the bonus door, the wind's streaks)
    // goes back over it.
    const float under = sy(float(ground));
    if (under < float(kScreenH))
    {
      clipTo(r, 0.0f, std::max(0.0f, under), float(kScreenW), float(kScreenH) - std::max(0.0f, under));
      drawProps(r, camX, camY, frame, false);
      unclip(r);
    }
  }

  // The town's buildings. The tiles inside a building are drawn again over
  // it, so its floors, roof and counter stay in front.
  for (const auto& d : w.decos)
  {
    if (d.kind != "facade" && d.kind != "sheriff")
      continue;
    // A roof on its top row: the false front stands a row higher, over it.
    bool roofTop = false;
    for (int tx = d.x0; tx <= d.x1; ++tx)
      roofTop = roofTop || mMap.block(tx, d.y0) == Tile::Platform;
    const int top = d.y0 - (roofTop ? 1 : 0);
    const int W = (d.x1 - d.x0 + 1) * int(kTilePx), H = (d.y1 - top + 1) * int(kTilePx);
    const float x = sx(float(d.x0)), y = sy(float(top));
    if (!visible(x - 8, y - 56, float(W + 16), float(H + 56)))
      continue;
    const std::string key = "bld/" + d.kind + "/" + d.text + "/" + std::to_string(W) + "x" + std::to_string(H);
    const std::string text = d.text, kind = d.kind;
    r.draw(baked(mArt, r, key, W + 16, H + 56, 8.0f, 56.0f,
             [&](cairo_t* cr) { paintBuilding(cr, double(W), double(H), text, kind); }),
      x, y);
    clipTo(r, x, y + 60.0f, float(W), float(H) - 60.0f);
    drawTiles(r, camX, camY, frame);
    drawPlatforms(r, camX, camY, frame, alpha);
    unclip(r);
    // The street under a counter that stands on it is still the street: the
    // tile shading counted the counter as earth above it.
    for (int tx = d.x0; tx <= d.x1; ++tx)
    {
      if (mMap.block(tx, d.y1) != Tile::Solid || mMap.block(tx, d.y1 + 1) != Tile::Solid)
        continue;
      static constexpr int kShade[4] = {255, 210, 175, 145};
      for (int k = 0, ty = d.y1 + 1; k < 4 && mMap.block(tx, ty) == Tile::Solid; ++k, ++ty)
      {
        const auto h = (hash2(tx, ty) >> 8) % 10u;
        DrawOpts o;
        o.tint = rgb(kShade[k], kShade[k], kShade[k]);
        r.draw(mArt.solid[h < 7u ? 0u : (h < 9u ? 1u : 2u)], sx(float(tx)), sy(float(ty)), o);
      }
      r.draw(mArt.solidTop, sx(float(tx)), sy(float(d.y1 + 1)));
    }
    // Solid tiles inside it are furniture: the bar counter, the balcony.
    for (int ty = d.y0 + 1; ty <= d.y1; ++ty)
      for (int tx = d.x0; tx <= d.x1; ++tx)
      {
        if (mMap.block(tx, ty) != Tile::Solid)
          continue;
        const float bx = sx(float(tx)), by = sy(float(ty));
        const bool top = mMap.block(tx, ty - 1) != Tile::Solid;
        const bool bar = d.text == "SALOON";
        r.fillRect(bx, by - (top ? 6.0f : 0.0f), kTilePx, kTilePx + (top ? 6.0f : 0.0f), bar ? rgb(110, 56, 32) : rgb(150, 104, 66));
        for (float py = by + 14.0f; py < by + kTilePx; py += 16.0f)
          r.fillRect(bx, py, kTilePx, 1.5f, rgba(50, 26, 14, 150));
        r.fillRect(bx + (tx % 2 ? 40.0f : 18.0f), by, 1.5f, kTilePx, rgba(50, 26, 14, 110));
        if (top)
        {
          r.fillRect(bx - 1.0f, by - 10.0f, kTilePx + 2.0f, 10.0f, bar ? rgb(70, 34, 20) : rgb(196, 150, 104));
          r.fillRect(bx - 1.0f, by - 10.0f, kTilePx + 2.0f, 2.5f, rgba(255, 230, 190, 200));
          if (bar)
          {
            // A brass foot rail, glasses and a bottle on the bar.
            r.fillRect(bx, by + 50.0f, kTilePx, 3.0f, kGold);
            if (tx % 2 == 0)
            {
              r.fillRect(bx + 14.0f, by - 26.0f, 9.0f, 16.0f, rgba(220, 240, 230, 150));
              r.fillRect(bx + 15.0f, by - 18.0f, 7.0f, 8.0f, rgba(220, 150, 50, 220));
            }
            else
            {
              r.fillRect(bx + 30.0f, by - 40.0f, 10.0f, 30.0f, rgb(60, 100, 50));
              r.fillRect(bx + 33.0f, by - 50.0f, 4.0f, 12.0f, rgb(60, 100, 50));
              r.fillRect(bx + 31.0f, by - 30.0f, 8.0f, 9.0f, rgb(240, 230, 200));
            }
          }
          else
          {
            // A railing along a balcony's edge.
            r.fillRect(bx, by - 46.0f, kTilePx, 5.0f, rgb(170, 124, 80));
            for (float px = bx + 6.0f; px < bx + kTilePx; px += 16.0f)
              r.fillRect(px, by - 44.0f, 4.0f, 34.0f, rgb(150, 104, 66));
          }
        }
      }
  }

  // The steeple and its bell.
  for (const auto& d : w.decos)
  {
    if (d.kind != "steeple")
      continue;
    const int W = (d.x1 - d.x0 + 1) * int(kTilePx), H = (d.y1 - d.y0 + 1) * int(kTilePx);
    const int bellRow = w.bellX >= d.x0 && w.bellX <= d.x1 && w.bellY >= d.y0 && w.bellY <= d.y1 ? w.bellY - d.y0 : 1;
    const float x = sx(float(d.x0)), y = sy(float(d.y0));
    if (!visible(x - 30, y - 190, float(W + 60), float(H + 190)))
      continue;
    r.draw(baked(mArt, r, "steeple/" + std::to_string(W) + "x" + std::to_string(H) + "/" + std::to_string(bellRow), W + 60,
             H + 200, 30.0f, 190.0f, [&](cairo_t* cr) { paintSteeple(cr, double(W), double(H), bellRow); }),
      x, y);
  }
  if (w.bellX >= 0)
  {
    int ring = w.bellRing * 30 / 20;
    for (const auto& m : w.metals)
      if (m.x0 == w.bellX && m.y0 == w.bellY)
        ring = std::max(ring, m.ring);
    const float bx = sx(float(w.bellX) + 0.5f), by = sy(float(w.bellY)) - 8.0f;
    if (visible(bx - 40, by, 80, 80))
    {
      DrawOpts o;
      o.angle = ring > 0 ? std::sin(float(frame) * 0.45f) * 30.0f * float(ring) / 30.0f : 0.0f;
      r.draw(baked(mArt, r, "bell", 64, 76, 32.0f, 4.0f, [](cairo_t* cr) { paintBell(cr); }), bx, by, o);
      if (ring > 0)
        drawGlow(r, mArt, bx, by + 40, 70.0f, rgb(255, 220, 140), 0.3f * float(ring) / 30.0f);
      // A glint every two seconds: something to shoot at.
      const int g = frame % 120;
      if (g < 10 && western)
        sparkle(r, mArt, bx + 18.0f, by + 46.0f, 10.0f + float(5 - std::abs(g - 5)) * 2.0f, kGold,
          1.0f - float(std::abs(g - 5)) / 6.0f);
    }
  }

  // Poles, the wire, the winch, headframes, the water tower's legs, the
  // stagecoach, the hitching post.
  for (const auto& d : w.decos)
  {
    const float x = sx(float(d.x0)), y = sy(float(d.y0));
    const float W = float(d.x1 - d.x0 + 1) * kTilePx, H = float(d.y1 - d.y0 + 1) * kTilePx;
    if (!visible(x - 60, y - 80, W + 120, H + 160))
      continue;
    if (d.kind == "pole")
      r.draw(baked(mArt, r, "pole/" + std::to_string(int(H)), 100, int(H) + 12, 50.0f, 6.0f,
               [&](cairo_t* cr) { paintPole(cr, double(H)); }),
        x + 32.0f, y + 4.0f);
    else if (d.kind == "winch")
      r.draw(baked(mArt, r, "winch/" + std::to_string(int(H)), 100, int(H) + 10, 50.0f, 0.0f,
               [&](cairo_t* cr) { paintWinch(cr, double(H)); }),
        x + 32.0f, y);
    else if (d.kind == "headframe")
      r.draw(baked(mArt, r, "headframe/" + std::to_string(int(W)) + "x" + std::to_string(int(H)), int(W) + 20, int(H) + 60,
               10.0f, 50.0f, [&](cairo_t* cr) { paintHeadframe(cr, double(W), double(H)); }),
        x, y);
    else if (d.kind == "legs")
      r.draw(baked(mArt, r, "legs/" + std::to_string(int(W)) + "x" + std::to_string(int(H)), int(W) + 20, int(H), 10.0f, 0.0f,
               [&](cairo_t* cr) { paintTowerLegs(cr, double(W), double(H)); }),
        x, y);
    else if (d.kind == "stagecoach")
      r.draw(baked(mArt, r, "coach/" + std::to_string(int(W)) + "x" + std::to_string(int(H)), int(W) + 20, int(H) + 30, 10.0f,
               30.0f, [&](cairo_t* cr) { paintStagecoach(cr, double(W), double(H)); }),
        x, y);
    else if (d.kind == "post")
      r.draw(baked(mArt, r, "post", 72, 64, 4.0f, 0.0f, [](cairo_t* cr) { paintHitchingPost(cr); }), x, y);
    else if (d.kind == "wire")
    {
      const float wx0 = float(d.x0) * kTilePx + 32.0f, wx1 = float(d.x1) * kTilePx + 32.0f;
      float px = wx0, py = wireY(w, wx0, d.y0);
      for (float wx = wx0 + 16.0f; wx <= wx1 + 0.5f; wx += 16.0f)
      {
        const float qy = wireY(w, std::min(wx, wx1), d.y0);
        r.drawLine(px - camX, py - camY, std::min(wx, wx1) - camX, qy - camY, 2.0f, rgb(40, 32, 30));
        px = std::min(wx, wx1);
        py = qy;
      }
    }
  }

  // The water tank (a metal that rings), the vault's alarm bell, and any
  // other metal plate.
  for (const auto& m : w.metals)
  {
    if (m.x0 == w.bellX && m.y0 == w.bellY && m.x1 == m.x0 && m.y1 == m.y0)
      continue; // the church bell, drawn above
    const float x = sx(float(m.x0)), y = sy(float(m.y0));
    const float W = float(m.x1 - m.x0 + 1) * kTilePx, H = float(m.y1 - m.y0 + 1) * kTilePx;
    if (!visible(x - 40, y - 60, W + 80, H + 100))
      continue;
    const float shake = m.ring > 0 ? std::sin(float(frame) * 2.1f) * 2.0f * float(m.ring) / 30.0f : 0.0f;
    if (m.x1 > m.x0 || m.y1 > m.y0)
    {
      r.draw(baked(mArt, r, "tank/" + std::to_string(int(W)) + "x" + std::to_string(int(H)), int(W) + 24, int(H) + 30, 12.0f,
               30.0f, [&](cairo_t* cr) { paintTank(cr, double(W), double(H)); }),
        x + shake, y);
      if (m.ring > 0)
        for (int k = 0; k < 4; ++k)
        {
          // The hoops sing: a glint running round each one.
          const float hy = y + 18.0f + (H - 30.0f) * float(k) / 3.0f;
          const float gx = x + W * std::fmod(float(frame) * 0.04f + float(k) * 0.3f, 1.0f);
          drawGlow(r, mArt, gx, hy, 22.0f, rgb(255, 240, 200), 0.6f * float(m.ring) / 30.0f);
        }
    }
    else if (m.reveal)
    {
      r.draw(baked(mArt, r, "alarmbell", 64, 56, 32.0f, 0.0f, [](cairo_t* cr) { paintAlarmBell(cr); }), x + 32.0f + shake,
        y);
      if (m.ring > 0)
        drawGlow(r, mArt, x + 32.0f, y + 30.0f, 60.0f, rgb(255, 220, 120), 0.5f * float(m.ring) / 30.0f);
    }
    else
    {
      r.fillRect(x + 6 + shake, y + 6, W - 12, H - 12, rgb(120, 124, 134));
      r.fillRect(x + 6 + shake, y + 6, W - 12, 4, rgb(220, 224, 232));
      for (const float bx : {x + 12.0f, x + W - 16.0f})
        for (const float by : {y + 14.0f, y + H - 16.0f})
          r.fillRect(bx + shake, by, 4, 4, rgb(60, 62, 70));
    }
  }

  // The windows the bandits pop up in, and a hat rising while he gets up.
  for (const auto& e : mEnemies)
  {
    if (e.kind != EnemyKind::WindowBandit)
      continue;
    const float L = float(e.x) * kCellPx - camX, T = float(e.y - e.h + 1) * kCellPx - camY;
    const float bw = float(e.w) * kCellPx, bh = float(e.h) * kCellPx;
    if (!visible(L - 30, T - 30, bw + 60, bh + 60))
      continue;
    const int ww = int(bw) + 36, wh = int(bh) + 6;
    r.draw(baked(mArt, r, "bwindow/" + std::to_string(ww) + "x" + std::to_string(wh), ww, wh, 0.0f, 0.0f,
             [&](cairo_t* cr) { paintBanditWindow(cr, double(ww), double(wh)); }),
      L - 18.0f, T - 6.0f);
    if (e.alive && e.hidden && e.tell > 0)
    {
      const float rise = 1.0f - float(e.tell) / float(kBanditTell);
      const float sill = T + bh * 0.62f;
      r.draw(baked(mArt, r, "bandit_hat", 72, 44, 36.0f, 34.0f, [](cairo_t* cr) { paintHat(cr); }), L + bw * 0.46f,
        sill + 18.0f - rise * 30.0f);
    }
  }

  // The piano.
  if (w.pianoX >= 0)
  {
    int floorRow = w.pianoY + 1;
    while (floorRow < mLevel->height - 1 && mMap.block(w.pianoX, floorRow) != Tile::Solid &&
           mMap.block(w.pianoX, floorRow) != Tile::Platform)
      ++floorRow;
    const float px = sx(float(w.pianoX)), floorY = sy(float(floorRow));
    const float keyY = sy(float(w.pianoY)) + 30.0f;
    if (visible(px - 20, floorY - 200, 300, 220))
    {
      // The case stands on the floor; its keyboard sits at the keys' row.
      r.draw(baked(mArt, r, "piano", 272, 186, 8.0f, 106.0f, [](cairo_t* cr) {
        cairo_translate(cr, 0, 10);
        paintPiano(cr);
      }),
        px, keyY);
      if (floorY > keyY + 66.0f)
        for (const float lx : {px + 12.0f, px + 236.0f})
          r.fillRect(lx, keyY + 66.0f, 10.0f, floorY - keyY - 66.0f, rgb(80, 38, 24));
      for (int k = 0; k < 4; ++k)
      {
        const bool pressed = w.pianoKey == k && w.pianoFlash > 0;
        float dip = pressed ? 4.0f : 0.0f;
        if (w.tune > 0)
          dip = std::max(dip, std::max(0.0f, std::sin(float(frame) * 0.7f + float(k) * 1.6f)) * 4.0f);
        const float kx = px + float(k) * kTilePx;
        for (int i = 0; i < 8; ++i)
        {
          r.fillRect(kx + float(i) * 8.0f + 0.5f, keyY - 14.0f + dip, 7.0f, 22.0f, pressed ? rgb(255, 236, 160) : rgb(250, 244, 226));
          r.fillRect(kx + float(i) * 8.0f + 0.5f, keyY + 5.0f + dip, 7.0f, 3.0f, rgb(200, 190, 170));
        }
        for (const int b : {0, 1, 3, 4, 5})
          r.fillRect(kx + float(b) * 8.0f + 5.0f, keyY - 14.0f + dip, 5.0f, 13.0f, rgb(30, 22, 20));
        if (pressed)
          drawGlow(r, mArt, kx + 32.0f, keyY - 4.0f, 34.0f, rgb(255, 220, 120), 0.5f * float(w.pianoFlash) / 10.0f);
      }
    }
  }

  // Wanted posters, spinning on their nails when shot.
  for (std::size_t i = 0; i < w.posters.size(); ++i)
  {
    const Poster& po = w.posters[i];
    const float x = sx(float(po.x) + 1.0f), y = sy(float(po.y)) + 4.0f;
    if (!visible(x - 80, y - 20, 160, 180))
      continue;
    std::string text = "BOUNTY: 42 GEMS";
    for (const auto& pr : mProps)
      if (pr.kind == PropKind::Deco42 && pr.x == po.x * kCellsPerTile && pr.y == po.y * kCellsPerTile && !pr.text.empty())
        text = pr.text;
    DrawOpts o;
    if (po.spin > 0)
    {
      const float u = 1.0f - float(po.spin) / 40.0f;
      o.angle = 720.0f * (2.0f * u - u * u);
    }
    r.draw(baked(mArt, r, "poster/" + text, 116, 150, 58.0f, 9.0f, [&](cairo_t* cr) { paintPoster(cr, text); }), x, y, o);
  }

  // The gold-plated revolver on its hook.
  for (const auto& pz : w.prizes)
  {
    if (pz.taken)
      continue;
    const float x = sx(float(pz.x) + 0.5f), y = sy(float(pz.y));
    if (!visible(x - 50, y - 10, 100, 100))
      continue;
    drawGlow(r, mArt, x + 4, y + 40, 46.0f, kGold, 0.25f + 0.1f * std::sin(float(frame) * 0.1f));
    r.draw(baked(mArt, r, "gold_revolver", 80, 80, 40.0f, 8.0f, [](cairo_t* cr) { paintGoldRevolver(cr); }), x, y);
    const int g = frame % 50;
    if (g < 8)
      sparkle(r, mArt, x + 10.0f - float(g) * 2.0f, y + 26.0f + float(g) * 4.0f, 9.0f, kGold, 1.0f - float(g) / 8.0f);
  }

  // The drawbridge: the plank tipping over on its hinge, then lying flat.
  for (const auto& d : w.bridges)
  {
    const int len = d.len;
    const float pivotX = float(d.hx + (d.dir > 0 ? 1 : 0)) * kTilePx - camX;
    float f = d.down ? 1.0f : (d.t >= 0 ? std::clamp((float(d.t) + alpha) / float(kBridgeFall), 0.0f, 1.0f) : 0.0f);
    f = f * f;
    const float pivotY = float(d.hy + 1) * kTilePx + kTilePx * f - camY;
    if (!visible(pivotX - float(len + 1) * kTilePx, pivotY - float(len + 1) * kTilePx, float(2 * len + 2) * kTilePx,
          float(len + 2) * kTilePx))
      continue;
    const float ax = d.dir > 0 ? 72.0f : 8.0f;
    const Texture& plank = baked(mArt, r, "bridge/" + std::to_string(len) + "/" + std::to_string(d.dir), 80, len * 64 + 16, ax,
      float(len * 64 + 8), [&](cairo_t* cr) { paintBridge(cr, len); });
    // Its chain to the winch while it stands.
    if (!d.down && d.t < 0)
      for (const auto& wd : w.decos)
        if (wd.kind == "winch" && std::abs(wd.x0 - d.hx) <= 2)
        {
          const float x0 = pivotX + (d.dir > 0 ? -32.0f : 32.0f), y0 = pivotY - float(len) * kTilePx + 4.0f;
          const float x1 = sx(float(wd.x0) + 0.5f), y1 = sy(float(wd.y0)) + 30.0f;
          for (int k = 0; k < 8; ++k)
          {
            const float a = float(k) / 8.0f, b = float(k + 1) / 8.0f;
            r.drawLine(x0 + (x1 - x0) * a, y0 + (y1 - y0) * a + 8.0f * std::sin(a * 3.14f),
              x0 + (x1 - x0) * b, y0 + (y1 - y0) * b + 8.0f * std::sin(b * 3.14f), k % 2 ? 3.0f : 5.0f, rgb(70, 66, 64));
          }
        }
    DrawOpts o;
    o.angle = -90.0f * float(d.dir < 0 ? 1 : -1) * f;
    r.draw(plank, pivotX, pivotY, o);
  }

  // Fuses: a twisted cord along their blocks (on the ground, up the poles,
  // along the wire), a brass cap at the end you light, burnt to ash behind
  // the spark.
  const Texture& cap = baked(mArt, r, "cap", 22, 16, 11.0f, 8.0f, [](cairo_t* cr) { paintCap(cr); });
  for (const auto& fu : w.fuses)
  {
    if (fu.hidden && !fu.lit())
      continue;
    const std::size_t n = fu.cells.size();
    std::vector<float> ax(n), ay(n);
    for (std::size_t i = 0; i < n; ++i)
    {
      const auto& c = fu.cells[i];
      float x = float(c.x) * kTilePx + 32.0f, y = float(c.y) * kTilePx + 32.0f;
      const bool horizontal = (i > 0 && fu.cells[i - 1].y == c.y) || (i + 1 < n && fu.cells[i + 1].y == c.y);
      const float wy = wireY(w, x, c.y);
      const Tile below = mMap.block(c.x, c.y + 1);
      if (wy >= 0.0f && horizontal)
        y = wy + 5.0f;
      else if (horizontal && (below == Tile::Solid || below == Tile::Platform))
        y = float(c.y + 1) * kTilePx - 4.0f;
      if (decoAt(w, "pole", c.x, c.y))
        x += 9.0f;
      ax[i] = x - camX;
      ay[i] = y - camY;
    }
    auto segment = [&](float x0, float y0, float x1, float y1, int state) {
      if (state == 2)
      {
        // Ash: a broken black line.
        r.drawLine(x0, y0, x1, y1, 3.0f, rgba(26, 22, 20, 220));
        return;
      }
      r.drawLine(x0, y0, x1, y1, 6.0f, rgb(70, 50, 30));
      r.drawLine(x0, y0, x1, y1, 3.5f, rgb(214, 186, 130));
      // The twist.
      const float dx = x1 - x0, dy = y1 - y0, len = std::sqrt(dx * dx + dy * dy);
      if (len < 1.0f)
        return;
      const float ux = dx / len, uy = dy / len;
      for (float t = 3.0f; t < len; t += 7.0f)
      {
        const float cx = x0 + ux * t, cy = y0 + uy * t;
        r.drawLine(cx - ux * 2.0f - uy * 2.0f, cy - uy * 2.0f + ux * 2.0f, cx + ux * 2.0f + uy * 2.0f,
          cy + uy * 2.0f - ux * 2.0f, 1.4f, rgb(120, 90, 54));
      }
    };
    if (!visible(std::min(ax.front(), ax.back()) - 2000.0f, -100.0f, 4000.0f, 900.0f))
      continue;
    for (std::size_t i = 0; i + 1 < n; ++i)
    {
      const float mx = (ax[i] + ax[i + 1]) * 0.5f, my = (ay[i] + ay[i + 1]) * 0.5f;
      if (!visible(std::min(ax[i], ax[i + 1]), std::min(ay[i], ay[i + 1]), std::abs(ax[i + 1] - ax[i]) + 1.0f,
            std::abs(ay[i + 1] - ay[i]) + 1.0f))
        continue;
      segment(ax[i], ay[i], mx, my, fu.cells[i].state);
      segment(mx, my, ax[i + 1], ay[i + 1], fu.cells[i + 1].state);
    }
    if (fu.fromId.empty() && fu.cells[0].state != 2)
      r.draw(cap, ax[0], ay[0]);
  }

  // Barrels: standing where they sit (or hung on a rope under a ceiling),
  // shaking and glowing faster while they hiss.
  const Texture& barrel = baked(mArt, r, "barrel", 64, 72, 32.0f, 70.0f, [](cairo_t* cr) { paintBarrel(cr); });
  for (const auto& b : w.barrels)
  {
    if (b.blown)
      continue;
    float x = sx(float(b.x) + 0.5f), y = sy(float(b.y + 1));
    if (!visible(x - 40, y - 80, 80, 90))
      continue;
    const Tile below = mMap.block(b.x, b.y + 1);
    if (below != Tile::Solid && below != Tile::Platform)
      for (int k = 1; k <= 3; ++k)
        if (mMap.block(b.x, b.y - k) == Tile::Solid)
        {
          r.drawLine(x, sy(float(b.y - k + 1)), x, y - 64.0f, 3.0f, rgb(150, 120, 80));
          y -= 2.0f;
          break;
        }
    if (b.t >= 0)
    {
      const float urgency = 1.0f - float(b.t) / float(std::max(1, b.flash));
      x += std::sin(float(frame) * 2.3f) * (1.5f + 3.5f * urgency);
      y += std::cos(float(frame) * 3.1f) * (0.5f + 1.5f * urgency);
    }
    r.draw(barrel, x, y);
    if (b.t >= 0)
    {
      const float urgency = 1.0f - float(b.t) / float(std::max(1, b.flash));
      const float pulse = 0.5f + 0.5f * std::sin(float(frame) * (0.6f + urgency * 1.6f));
      DrawOpts o;
      o.blend = Blend::Add;
      o.alpha = (0.25f + 0.55f * urgency) * pulse;
      o.tint = rgb(255, 200, 120);
      r.draw(barrel, x, y, o);
      drawGlow(r, mArt, x, y - 34.0f, 60.0f + 40.0f * urgency, rgb(255, 140, 50), 0.35f + 0.4f * pulse * urgency);
    }
  }

  // High Noon's chalk marks on the street.
  if (w.noon.on)
  {
    auto chalk = [&](float cx, float gy) {
      const float x = cx - camX, y = gy - camY;
      r.drawLine(x - 18, y - 2, x + 18, y + 6, 4.0f, rgba(250, 246, 236, 200));
      r.drawLine(x - 18, y + 6, x + 18, y - 2, 4.0f, rgba(250, 246, 236, 200));
    };
    chalk(float(w.noon.runnerX + 1) * kCellPx, float(w.noon.runnerY + 1) * kCellPx + 6.0f);
    chalk(float(w.noon.markX) * kCellPx + 48.0f, float(w.noon.markY + 1) * kCellPx + 6.0f);
  }
}

// --- In front -------------------------------------------------------------------------------

void World::drawWestFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)alpha;
  const auto& w = mWest;

  // The sills in front of the window bandits.
  for (const auto& e : mEnemies)
  {
    if (e.kind != EnemyKind::WindowBandit)
      continue;
    const float L = float(e.x) * kCellPx - camX, T = float(e.y - e.h + 1) * kCellPx - camY;
    const float bw = float(e.w) * kCellPx, bh = float(e.h) * kCellPx;
    if (!visible(L - 30, T - 30, bw + 60, bh + 60))
      continue;
    const float sill = T + bh * 0.62f;
    const int sw = int(bw) + 44, sh = int(T + bh - sill) + 2;
    r.draw(baked(mArt, r, "sill/" + std::to_string(sw) + "x" + std::to_string(sh), sw, sh, 0.0f, 0.0f,
             [&](cairo_t* cr) { paintSill(cr, double(sw), double(sh)); }),
      L - 22.0f, sill);
  }

  // Sparks on the burning fuses.
  for (const auto& fu : w.fuses)
    for (std::size_t i = 0; i < fu.cells.size(); ++i)
    {
      const auto& c = fu.cells[i];
      if (c.state != 1)
        continue;
      float x = float(c.x) * kTilePx + 32.0f, y = float(c.y) * kTilePx + 32.0f;
      const bool horizontal = (i > 0 && fu.cells[i - 1].y == c.y) || (i + 1 < fu.cells.size() && fu.cells[i + 1].y == c.y);
      const float wy = wireY(w, x, c.y);
      const Tile below = mMap.block(c.x, c.y + 1);
      if (wy >= 0.0f && horizontal)
        y = wy + 5.0f;
      else if (horizontal && (below == Tile::Solid || below == Tile::Platform))
        y = float(c.y + 1) * kTilePx - 4.0f;
      if (decoAt(w, "pole", c.x, c.y))
        x += 9.0f;
      x -= camX;
      y -= camY;
      if (!visible(x - 40, y - 40, 80, 80))
        continue;
      const float flick = 0.7f + 0.3f * frac(hash2(c.x * 7 + c.y, frame));
      drawGlow(r, mArt, x, y, 46.0f * flick, kSpark, 0.8f);
      drawGlow(r, mArt, x, y, 14.0f * flick, rgb(255, 255, 230), 1.0f);
      for (int k = 0; k < 7; ++k)
      {
        const std::uint32_t h = hash2(c.x * 31 + c.y * 17 + k, frame / 2);
        const float a = -1.57f + (frac(h) - 0.5f) * 3.4f;
        const float life = float((frame + k * 5) % 10) / 10.0f;
        const float dist = 6.0f + life * (18.0f + frac(h >> 8) * 22.0f);
        const float px = x + std::cos(a) * dist, py = y + std::sin(a) * dist + life * life * 14.0f;
        r.drawLine(px, py, px - std::cos(a) * 5.0f, py - std::sin(a) * 5.0f, 2.0f,
          withAlpha(lerpColor(rgb(255, 250, 200), kSpark, life), int(255 * (1.0f - life))), Blend::Add);
      }
      // A wisp of smoke.
      const float sm = float(frame % 24) / 24.0f;
      drawGlow(r, mArt, x + std::sin(float(frame) * 0.2f) * 4.0f, y - 10.0f - sm * 30.0f, 12.0f + sm * 10.0f,
        rgb(120, 110, 100), 0.25f * (1.0f - sm));
    }

  // The Duelists: a little bell over each one waiting on the church bell,
  // the glint as he draws.
  for (int i : w.duelists)
  {
    if (i < 0 || std::size_t(i) >= mEnemies.size())
      continue;
    const Enemy& e = mEnemies[std::size_t(i)];
    if (!e.alive)
      continue;
    const float cx = (e.drawX + float(e.w) * 0.5f) * kCellPx - camX;
    const float top = (e.drawY - float(e.h) + 1.0f) * kCellPx - camY;
    if (!visible(cx - 60, top - 80, 120, 260))
      continue;
    if (e.attach == 1 || e.attach == 2)
    {
      DrawOpts o;
      o.angle = w.bellRing > 0 ? std::sin(float(frame) * 0.6f) * 28.0f * float(w.bellRing) / 20.0f : 0.0f;
      const float by = top - 40.0f + std::sin(float(frame) * 0.08f) * 3.0f;
      r.draw(baked(mArt, r, "bellicon", 32, 34, 16.0f, 2.0f, [](cairo_t* cr) { paintBellIcon(cr); }), cx, by, o);
      // One tick for the first ring, two once it has rung twice.
      for (int k = 0; k < (e.attach == 2 ? 1 : 0); ++k)
        r.fillRect(cx + 20.0f, by + 12.0f, 6.0f, 6.0f, rgb(255, 220, 120));
    }
    if (e.tell > 0)
    {
      const float gx = cx + float(e.dir) * 46.0f, gy = top + 76.0f;
      sparkle(r, mArt, gx, gy, 8.0f + float(e.tell) * 1.5f, rgb(255, 250, 220), std::min(1.0f, float(e.tell) / 4.0f));
    }
  }
  if (w.noon.on && w.noon.duelist >= 0 && std::size_t(w.noon.duelist) < mEnemies.size())
  {
    const Enemy& e = mEnemies[std::size_t(w.noon.duelist)];
    if (e.alive && e.tell > 0)
    {
      const float cx = (e.drawX + float(e.w) * 0.5f) * kCellPx - camX, top = (e.drawY - float(e.h) + 1.0f) * kCellPx - camY;
      sparkle(r, mArt, cx + float(e.dir) * 46.0f, top + 76.0f, 8.0f + float(e.tell) * 1.5f, rgb(255, 250, 220),
        std::min(1.0f, float(e.tell) / 4.0f));
    }
  }

  // The tonic's label.
  if (w.tonic >= 0 && std::size_t(w.tonic) < mItems.size() && w.tonicNear && !mItems[std::size_t(w.tonic)].taken)
  {
    const Item& it = mItems[std::size_t(w.tonic)];
    const float cx = float(it.x) * kCellPx + 32.0f - camX, top = float(it.y - 1) * kCellPx - camY;
    const float bw = 330.0f, bh = 52.0f;
    const float bx = std::clamp(cx - bw * 0.5f, 8.0f, float(kScreenW) - bw - 8.0f), by = top - bh - 18.0f;
    r.fillRect(bx, by, bw, bh, rgba(246, 232, 196, 240));
    r.fillRect(bx, by, bw, 3.0f, rgb(150, 40, 30));
    r.fillRect(bx, by + bh - 3.0f, bw, 3.0f, rgb(150, 40, 30));
    r.fillRect(bx, by, 3.0f, bh, rgb(150, 40, 30));
    r.fillRect(bx + bw - 3.0f, by, 3.0f, bh, rgb(150, 40, 30));
    r.drawText("DR. FIZZ'S MIRACLE TONIC", bx + bw * 0.5f, by + 6.0f, {18.0f, rgb(90, 30, 20), 0}, Align::Center);
    r.drawText("(CONTAINS VIRUS)", bx + bw * 0.5f, by + 29.0f, {14.0f, rgb(40, 130, 30), 0}, Align::Center);
    r.drawLine(cx, by + bh, cx, top - 4.0f, 2.0f, rgb(150, 40, 30));
  }

  // The piano's notes floating up while it plays itself.
  if (w.pianoX >= 0 && (w.tune > 0 || w.pianoFlash > 0))
  {
    const float px = (float(w.pianoX) + 2.0f) * kTilePx - camX, py = float(w.pianoY) * kTilePx - camY - 20.0f;
    const int count = w.tune > 0 ? 6 : 1;
    for (int k = 0; k < count; ++k)
    {
      const float life = w.tune > 0 ? std::fmod(float(frame) * 0.012f + float(k) / float(count), 1.0f)
                                    : 1.0f - float(w.pianoFlash) / 10.0f;
      const float nx = px + (w.tune > 0 ? (float(k) - 2.5f) * 40.0f : (float(w.pianoKey) - 1.5f) * kTilePx) +
        std::sin(life * 6.0f + float(k)) * 12.0f;
      const float ny = py - life * 120.0f;
      const int a = int(255 * (1.0f - life));
      const Color c = withAlpha(k % 2 ? rgb(255, 220, 120) : rgb(255, 250, 230), a);
      r.fillRect(nx - 6.0f, ny - 4.0f, 10.0f, 8.0f, c);
      r.fillRect(nx + 2.0f, ny - 24.0f, 2.5f, 22.0f, c);
      r.fillRect(nx + 2.0f, ny - 24.0f, 10.0f, 4.0f, c);
    }
  }
}

// --- HUD ------------------------------------------------------------------------------------

void World::drawWestHud(Renderer& r, int frame) const
{
  const auto& w = mWest;
  const auto& p = mPlayer;

  // The Six-Shooter's cylinder, under the weapon panel.
  if (p.weapon == Weapon::Proto && p.proto == int(ProtoId::SixShooter))
  {
    // It takes the weapon icon's place in the panel.
    const float cx = 12.0f + float(kHudHealthW) + 10.0f + 38.0f, cy = 10.0f + 41.0f;
    r.draw(baked(mArt, r, "cylback", 56, 56, 28.0f, 28.0f, [](cairo_t* cr) {
      cairo_arc(cr, 28, 28, 27, 0, 2 * kPi);
      setRgba(cr, rgb(16, 12, 30), 0.95);
      cairo_fill(cr);
    }),
      cx, cy);
    const float spin = w.reload > 0 ? float(frame) * 40.0f : 0.0f;
    DrawOpts o;
    o.angle = spin;
    o.scale = 0.72f;
    r.draw(baked(mArt, r, "cylinder", 64, 64, 32.0f, 32.0f, [](cairo_t* cr) { paintCylinder(cr); }), cx, cy, o);
    const Texture& round = baked(mArt, r, "round", 16, 16, 8.0f, 8.0f, [](cairo_t* cr) { paintRound(cr); });
    const int loaded = w.reload > 0 ? 0 : w.cylinder;
    for (int k = 0; k < kSixCylinder; ++k)
    {
      if (k >= loaded)
        continue;
      const float a = (-90.0f + float(k) * 60.0f + spin) * 3.14159f / 180.0f;
      DrawOpts ro;
      ro.scale = 0.72f;
      r.draw(round, cx + std::cos(a) * 16.0f * 0.72f, cy + std::sin(a) * 16.0f * 0.72f, ro);
    }
    if (w.reload > 0)
      drawGlow(r, mArt, cx, cy, 34.0f, rgb(255, 220, 140), 0.25f + 0.15f * float((frame / 3) % 2));
  }

  // QUICK DRAW!
  if (w.quickDraw > 0)
  {
    const float t = float(60 - w.quickDraw) / 60.0f;
    const float pop = t < 0.15f ? 0.6f + t / 0.15f * 0.5f : 1.1f - std::min(0.1f, (t - 0.15f));
    const float a = w.quickDraw < 15 ? float(w.quickDraw) / 15.0f : 1.0f;
    drawGlow(r, mArt, float(kScreenW) * 0.5f, 250.0f, 260.0f, kGold, 0.35f * a);
    r.drawText("QUICK DRAW!", float(kScreenW) * 0.5f, 220.0f, {60.0f * pop, rgb(255, 220, 90), rgb(90, 30, 10), true},
      Align::Center, a);
    r.drawText("+500", float(kScreenW) * 0.5f, 296.0f, {26.0f, rgb(255, 250, 230), rgb(90, 30, 10)}, Align::Center, a);
  }

  // High Noon: the duel count, DRAW! and what went wrong.
  if (w.noon.on)
  {
    const auto& n = w.noon;
    const float rx = float(kScreenW) - 24.0f, ry = 92.0f;
    r.fillRect(rx - 190.0f, ry - 6.0f, 190.0f, 44.0f, rgba(8, 6, 22, 190));
    char buf[32];
    std::snprintf(buf, sizeof(buf), "DUEL %d/%d", std::min(n.duel, kHighNoonDuels), kHighNoonDuels);
    r.drawText(buf, rx - 12.0f, ry, {26.0f, rgb(255, 214, 110), rgb(30, 14, 6)}, Align::Right);
    for (int k = 0; k < kHighNoonDuels; ++k)
      r.fillRect(rx - 182.0f + float(k) * 8.5f, ry + 32.0f, 6.0f, 3.0f,
        k < n.duel - (n.phase == DuelPhase::Won || n.phase == DuelPhase::Done ? 0 : 1) ? rgb(255, 214, 110)
                                                                                      : rgba(255, 255, 255, 50));
    // The steeple's bell is up out of sight: a bell here, with a pip for
    // each ring of this duel.
    if (n.phase != DuelPhase::Done)
    {
      const int rings = n.phase == DuelPhase::Gap ? 1 : (n.phase == DuelPhase::Draw || n.phase == DuelPhase::Fired ? 2 : 0);
      const float bx = rx - 190.0f, by = ry + 44.0f;
      r.fillRect(bx, by, 96.0f, 40.0f, rgba(8, 6, 22, 190));
      DrawOpts o;
      o.scale = 1.25f;
      o.angle = w.bellRing > 0 ? std::sin(float(frame) * 0.6f) * 28.0f * float(w.bellRing) / 20.0f : 0.0f;
      r.draw(baked(mArt, r, "bellicon", 32, 34, 16.0f, 2.0f, [](cairo_t* cr) { paintBellIcon(cr); }), bx + 26.0f, by + 1.0f, o);
      for (int k = 0; k < 2; ++k)
      {
        const float px = bx + 52.0f + float(k) * 20.0f, py = by + 14.0f;
        r.fillRect(px, py, 12.0f, 12.0f, rgba(255, 255, 255, 40));
        if (k < rings)
        {
          drawGlow(r, mArt, px + 6.0f, py + 6.0f, 18.0f, rgb(255, 200, 90), 0.6f);
          r.fillRect(px + 1.0f, py + 1.0f, 10.0f, 10.0f, k == 1 ? rgb(255, 120, 70) : rgb(255, 220, 120));
        }
      }
    }
    if (n.phase == DuelPhase::Draw)
    {
      const float t = float(n.t) / float(kDuelDraw);
      drawGlow(r, mArt, float(kScreenW) * 0.5f, 300.0f, 300.0f, rgb(255, 120, 50), 0.45f * (1.0f - t));
      r.drawText("DRAW!", float(kScreenW) * 0.5f + std::sin(float(frame) * 3.0f) * 3.0f, 250.0f,
        {110.0f * (1.15f - 0.15f * t), rgb(255, 240, 200), rgb(140, 20, 10), true}, Align::Center);
    }
    else if (n.message > 0 && !n.text.empty() && n.text != "DRAW!")
    {
      const float a = std::min(1.0f, float(n.message) / 12.0f);
      r.drawText(n.text, float(kScreenW) * 0.5f, 270.0f, {44.0f, rgb(255, 120, 90), rgb(40, 8, 4)}, Align::Center, a);
    }
    else if (n.phase == DuelPhase::WalkIn || n.phase == DuelPhase::Ring1 || n.phase == DuelPhase::Gap)
      r.drawText("WAIT FOR THE SECOND BELL", float(kScreenW) * 0.5f, 140.0f, {20.0f, rgb(255, 230, 190), rgb(30, 14, 6)},
        Align::Center, 0.8f);
  }
}

} // namespace gr
