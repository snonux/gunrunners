// Level 42, The Live Finale: drawing. See world_finale.cpp and
// world_finale_boss.cpp for the logic.
//
// Behind the tiles (drawFinaleSet, finale_draw.hpp): each set wagon's
// painted flat in its slot, framed in gold with its name over it: Neon
// City's skyline at sunset, the temple's carved stones and torches, the
// station's viewport on a ringed planet, the western's saloon, the toon's
// inked hills, the studio's cyc and cameras. A flat waits dark in the
// wings, slides in lit as the turntable turns it, plays full while its
// trick runs and goes purple for a RERUN. The stage door's spotlit
// pedestal; backstage's dressing-room wall with Lance's star door; the
// arena's main stage, its LED wall, the gantry Lance runs along and the
// broadcast desk.
//
// Over the tiles (drawFinaleBack): the turntable rims and their chasing
// lights, the floor marks at the triggers, the gravity plates, the studio
// monitors on their stands, the backstage refill, the ON AIR door and its
// STAGE 42 plate, the trapdoors (hazard edges, an outline before they
// drop, the static flicker while open), the race decks, the master
// switch's glass case; the Ad Breaks dropping from the flies, their
// samples, the Censor Bars and the Stunt Doubles; the Encore Engine and
// Off the Air's run along the gantry.
//
// In front (drawFinaleFront): the Encore's shots, the Censor Bars' black
// bars, the duck's host jacket; Lance behind his hover podium with his
// prize wheel and cue cards; the rain of gold, the laugh track, the lamps;
// the Engine's move board, coins, ring, lightning, flame and magnet beam;
// the toon's "uh-oh". On top (drawFinaleTop): PREVIOUSLY ON, the monitor's
// static, CUT TO COMMERCIAL, the BLOOPER! clapper. The HUD
// (drawFinaleHud): the ratings ticker, the Encore's wheel, the boss's bar
// and the race to the desk.

#include "game/world.hpp"

#include "assets/art.hpp"
#include "assets/enemy_art.hpp"
#include "assets/enemy_art_zero.hpp"
#include "base/math.hpp"
#include "data/enemies.hpp"
#include "data/weapons.hpp"
#include "game/finale_draw.hpp"
#include "render/vector.hpp"

#include <SDL.h>

#include <algorithm>
#include <array>
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
constexpr Color kInk = rgb(8, 8, 18);
constexpr Color kHudInk = rgb(4, 4, 12);
constexpr Color kNavy = rgb(20, 24, 66);
constexpr Color kNavyLo = rgb(8, 10, 30);
constexpr Color kGold = rgb(236, 186, 74);
constexpr Color kGoldHi = rgb(255, 242, 180);
constexpr Color kGoldLo = rgb(124, 84, 24);
constexpr Color kLed = rgb(90, 200, 255);
constexpr Color kChrome = rgb(196, 204, 222);
constexpr Color kChromeLo = rgb(70, 76, 98);
constexpr Color kOnAir = rgb(255, 50, 70);
constexpr int kFlatTop = 16; // blocks: the top of a wagon's flat (its foot is the stage floor, row 40)
constexpr int kStage = 40;   // blocks: the stage floor

Color lighten(Color c, float t) { return lerpColor(c, rgb(255, 255, 255), t); }
Color darken(Color c, float t) { return lerpColor(c, rgb(0, 0, 0), t); }

float lerpCells(int prev, int cur, float alpha) { return (float(prev) + float(cur - prev) * alpha) * kCellPx; }

bool offScreen(float x, float y, float w, float h, float m = 96.0f)
{
  return x > float(kScreenW) + m || x + w < -m || y > float(kScreenH) + m || y + h < -m;
}

const Sprite& bakedSprite(const Art& art, const Renderer& r, const std::string& key, int w, int h, float ax, float ay,
  const std::function<void(cairo_t*)>& paint)
{
  const std::string id = "~finale/" + key;
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

// --- Cairo helpers ---------------------------------------------------------------------------

void setRgba(cairo_t* cr, Color c, double a)
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

void vgrad(cairo_t* cr, double y0, double y1, std::initializer_list<std::pair<double, Color>> stops, double a = 1.0,
  bool preserve = false)
{
  cairo_pattern_t* g = cairo_pattern_create_linear(0, y0, 0, y1);
  for (const auto& s : stops)
    stop(g, s.first, s.second, a);
  cairo_set_source(cr, g);
  if (preserve)
    cairo_fill_preserve(cr);
  else
    cairo_fill(cr);
  cairo_pattern_destroy(g);
}

void hgrad(cairo_t* cr, double x0, double x1, std::initializer_list<std::pair<double, Color>> stops, double a = 1.0,
  bool preserve = false)
{
  cairo_pattern_t* g = cairo_pattern_create_linear(x0, 0, x1, 0);
  for (const auto& s : stops)
    stop(g, s.first, s.second, a);
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

void box(cairo_t* cr, double x, double y, double w, double h, Color c, double a = 1.0)
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

// Centred text with a soft halo stroked under it.
void glowText(cairo_t* cr, const char* s, double cx, double baseline, double size, Color c, Color halo, double width)
{
  selectGameFont(cr);
  cairo_set_font_size(cr, size);
  cairo_text_extents_t e;
  cairo_text_extents(cr, s, &e);
  const double x = cx - e.width * 0.5 - e.x_bearing;
  for (const double k : {1.0, 0.5})
  {
    cairo_move_to(cr, x, baseline);
    cairo_text_path(cr, s);
    stroke(cr, halo, width * k, 0.22);
  }
  cairo_move_to(cr, x, baseline);
  setRgba(cr, c, 1.0);
  cairo_show_text(cr, s);
}

void starPath(cairo_t* cr, double cx, double cy, double ro, double ri, int points = 5)
{
  for (int i = 0; i < points * 2; ++i)
  {
    const double a = -kPi / 2 + i * kPi / points, rr = i % 2 ? ri : ro;
    if (i == 0)
      cairo_move_to(cr, cx + std::cos(a) * rr, cy + std::sin(a) * rr);
    else
      cairo_line_to(cr, cx + std::cos(a) * rr, cy + std::sin(a) * rr);
  }
  cairo_close_path(cr);
}

void hazard(cairo_t* cr, double x, double y, double w, double h, double pitch)
{
  cairo_save(cr);
  cairo_rectangle(cr, x, y, w, h);
  cairo_clip(cr);
  box(cr, x, y, w, h, rgb(20, 20, 28));
  for (double sx = x - h; sx < x + w + h; sx += pitch)
  {
    cairo_move_to(cr, sx, y + h);
    cairo_line_to(cr, sx + h, y);
    cairo_line_to(cr, sx + h + pitch * 0.5, y);
    cairo_line_to(cr, sx + pitch * 0.5, y + h);
    cairo_close_path(cr);
    setRgba(cr, rgb(240, 200, 40), 0.95);
    cairo_fill(cr);
  }
  cairo_restore(cr);
}

void bulb(cairo_t* cr, double x, double y, double r, bool lit)
{
  if (lit)
    softDot(cr, x, y, r * 3.0, r * 3.0, kGoldHi, 0.35);
  circle(cr, x, y, r);
  setRgba(cr, lit ? kGoldHi : rgb(110, 90, 60), 1.0);
  cairo_fill(cr);
  circle(cr, x - r * 0.3, y - r * 0.35, r * 0.4);
  setRgba(cr, rgb(255, 255, 255), lit ? 0.8 : 0.3);
  cairo_fill(cr);
}

// A run of buildings from the bottom edge, windows lit by `win`.
void skylineRow(cairo_t* cr, double W, double bottom, double minH, double maxH, Color body, Color win, double winOdds,
  std::uint32_t seed)
{
  Rng rng(seed);
  double x = -20;
  while (x < W + 20)
  {
    const double bw = rng.range(90.0f, 220.0f), bh = rng.range(float(minH), float(maxH));
    box(cr, x, bottom - bh, bw - 8, bh, body);
    if (rng.uniform() < 0.35f)
      box(cr, x + bw * 0.45, bottom - bh - 40, 5, 40, body);
    for (double wy = bottom - bh + 18; wy < bottom - 16; wy += 26)
      for (double wx = x + 12; wx < x + bw - 26; wx += 20)
        if (rng.uniform() < float(winOdds))
          box(cr, wx, wy, 9, 12, win, rng.uniform() < 0.5f ? 0.95 : 0.55);
    x += bw;
  }
}

// --- Renderer helpers ------------------------------------------------------------------------

class Clip
{
public:
  Clip(Renderer& r, float x, float y, float w, float h) : mSdl(r.sdl())
  {
    mHad = SDL_RenderIsClipEnabled(mSdl) == SDL_TRUE;
    if (mHad)
      SDL_RenderGetClipRect(mSdl, &mPrev);
    const int x0 = int(std::floor(x)), y0 = int(std::floor(y));
    SDL_Rect rc{x0, y0, std::max(0, int(std::ceil(x + w)) - x0), std::max(0, int(std::ceil(y + h)) - y0)};
    if (mHad && !SDL_IntersectRect(&rc, &mPrev, &rc))
      rc = {x0, y0, 0, 0};
    SDL_RenderSetClipRect(mSdl, &rc);
  }
  ~Clip() { SDL_RenderSetClipRect(mSdl, mHad ? &mPrev : nullptr); }
  Clip(const Clip&) = delete;
  Clip& operator=(const Clip&) = delete;

private:
  SDL_Renderer* mSdl;
  SDL_Rect mPrev{};
  bool mHad = false;
};

// A texture stretched over (x, y, w, h), turned `angle` degrees clockwise
// round (px, py) inside that rectangle.
void blit(Renderer& r, const Texture& t, float x, float y, float w, float h, float angle, float px, float py, float alpha,
  Blend b, Color tint = rgb(255, 255, 255))
{
  if (!t || w <= 0.5f || h <= 0.5f || alpha <= 0.0f)
    return;
  const SDL_FRect dst{x, y, w, h};
  const SDL_FPoint pivot{px, py};
  SDL_SetTextureBlendMode(t.get(), b == Blend::Add ? SDL_BLENDMODE_ADD : SDL_BLENDMODE_BLEND);
  SDL_SetTextureAlphaMod(t.get(), Uint8(std::lround(std::clamp(alpha, 0.0f, 1.0f) * 255.0f)));
  SDL_SetTextureColorMod(t.get(), Uint8(redOf(tint)), Uint8(greenOf(tint)), Uint8(blueOf(tint)));
  SDL_RenderCopyExF(r.sdl(), t.get(), nullptr, &dst, double(angle), &pivot, SDL_FLIP_NONE);
  SDL_SetTextureColorMod(t.get(), 255, 255, 255);
  SDL_SetTextureAlphaMod(t.get(), 255);
}

// --- The wagons' flats -------------------------------------------------------------------------
//
// Painted in full-size units (W x H, the slot's width by the 24 blocks from
// kFlatTop to the stage) into a half-size texture, drawn back up at 2 x.

void paintNeonFlat(cairo_t* cr, double W, double H)
{
  cairo_rectangle(cr, 0, 0, W, H);
  vgrad(cr, 0, H, {{0.0, rgb(10, 4, 36)}, {0.45, rgb(70, 16, 100)}, {0.75, rgb(210, 70, 120)}, {1.0, rgb(255, 150, 90)}});
  Rng rng(42101u);
  for (int i = 0; i < 160; ++i)
  {
    circle(cr, rng.range(0, float(W)), rng.range(0, float(H * 0.5)), rng.range(0.8f, 2.2f));
    setRgba(cr, rgb(255, 255, 255), rng.range(0.3f, 0.9f));
    cairo_fill(cr);
  }
  // The sun, its stripes cut out.
  const double sx = W * 0.62, sy = H * 0.7, sr = std::min(W * 0.28, 300.0);
  cairo_push_group(cr);
  circle(cr, sx, sy, sr);
  vgrad(cr, sy - sr, sy + sr, {{0.0, rgb(255, 236, 110)}, {1.0, rgb(255, 60, 150)}});
  cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
  for (int i = 0; i < 9; ++i)
    cairo_rectangle(cr, sx - sr, sy + i * 22, sr * 2, 4 + i * 2);
  cairo_fill(cr);
  cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
  cairo_pop_group_to_source(cr);
  cairo_paint(cr);
  softDot(cr, sx, sy, sr * 1.6, sr * 1.2, rgb(255, 120, 160), 0.25);
  skylineRow(cr, W, H, 260, 640, rgb(60, 20, 90), rgb(255, 120, 200), 0.18, 42102u);
  // The maglev's rail and a train.
  const double ry = H - 420;
  box(cr, 0, ry, W, 10, rgb(30, 10, 50));
  for (double x = 60; x < W; x += 240)
    box(cr, x, ry + 10, 14, 420, rgb(30, 10, 50));
  roundRect(cr, W * 0.12, ry - 46, 420, 44, 20);
  hgrad(cr, W * 0.12, W * 0.12 + 420, {{0.0, rgb(200, 210, 240)}, {1.0, rgb(120, 130, 170)}}, 1.0, true);
  stroke(cr, rgb(30, 10, 50), 3.0);
  for (double x = W * 0.12 + 40; x < W * 0.12 + 400; x += 34)
    box(cr, x, ry - 36, 22, 12, rgb(90, 230, 255), 0.95);
  skylineRow(cr, W, H, 160, 460, rgb(22, 8, 40), rgb(90, 230, 255), 0.28, 42103u);
  // Neon signs on the near buildings.
  const char* const kSigns[] = {"NEON", "CITY", "24H", "BAR"};
  const Color kTubes[] = {rgb(255, 80, 190), rgb(90, 230, 255), rgb(255, 230, 90), rgb(140, 255, 120)};
  for (int i = 0; i < 4; ++i)
  {
    const double x = W * (0.14 + i * 0.24), y = H - 300 + (i % 2) * 90;
    roundRect(cr, x - 80, y - 52, 160, 70, 10);
    setRgba(cr, rgb(16, 4, 26), 0.9);
    cairo_fill_preserve(cr);
    stroke(cr, kTubes[i], 3.0);
    softDot(cr, x, y - 16, 120, 60, kTubes[i], 0.25);
    glowText(cr, kSigns[i], x, y, 40, lighten(kTubes[i], 0.5f), kTubes[i], 16);
  }
}

void paintTempleFlat(cairo_t* cr, double W, double H)
{
  cairo_rectangle(cr, 0, 0, W, H);
  vgrad(cr, 0, H, {{0.0, rgb(6, 20, 24)}, {0.5, rgb(24, 64, 56)}, {1.0, rgb(90, 120, 76)}});
  Rng rng(42111u);
  // The jungle's canopy behind.
  for (int i = 0; i < 40; ++i)
  {
    const double x = rng.range(0, float(W)), y = H * 0.36 + rng.range(-60, 80), rr = rng.range(70, 150);
    circle(cr, x, y, rr);
    setRgba(cr, rgb(14, 40, 30), 0.9);
    cairo_fill(cr);
  }
  // The wall of carved stones.
  const double top = H * 0.42;
  cairo_rectangle(cr, 0, top, W, H - top);
  vgrad(cr, top, H, {{0.0, rgb(150, 126, 84)}, {1.0, rgb(96, 76, 50)}});
  for (double y = top; y < H; y += 72)
  {
    const double off = int((y - top) / 72) % 2 ? 80 : 0;
    for (double x = -off; x < W; x += 160)
    {
      roundRect(cr, x + 3, y + 3, 154, 66, 6);
      vgrad(cr, y, y + 72, {{0.0, rgb(176, 150, 100)}, {1.0, rgb(120, 96, 62)}}, 1.0, true);
      stroke(cr, rgb(60, 44, 28), 2.5);
      if (rng.uniform() < 0.25f)
      {
        // A glyph.
        const double gx = x + 80, gy = y + 36;
        circle(cr, gx, gy, 14);
        stroke(cr, rgb(80, 60, 36), 3.0);
        cairo_move_to(cr, gx - 20, gy);
        cairo_line_to(cr, gx + 20, gy);
        stroke(cr, rgb(80, 60, 36), 3.0);
      }
    }
  }
  // The sun disc over the middle.
  const double cx = W * 0.5, cy = top + 200;
  circle(cr, cx, cy, 130);
  vgrad(cr, cy - 130, cy + 130, {{0.0, rgb(255, 214, 100)}, {1.0, rgb(200, 130, 40)}}, 1.0, true);
  stroke(cr, rgb(90, 60, 20), 5.0);
  for (int k = 0; k < 16; ++k)
  {
    const double a = k * kPi / 8;
    cairo_move_to(cr, cx + std::cos(a) * 92, cy + std::sin(a) * 92);
    cairo_line_to(cr, cx + std::cos(a) * 122, cy + std::sin(a) * 122);
    stroke(cr, rgb(110, 70, 20), 6.0);
  }
  circle(cr, cx, cy, 50);
  stroke(cr, rgb(110, 70, 20), 6.0);
  // Columns at the edges.
  for (const double x : {60.0, W - 60.0})
  {
    cairo_rectangle(cr, x - 46, top - 60, 92, H - top + 60);
    hgrad(cr, x - 46, x + 46, {{0.0, rgb(90, 70, 46)}, {0.4, rgb(196, 170, 120)}, {1.0, rgb(80, 60, 40)}}, 1.0, true);
    stroke(cr, rgb(50, 36, 22), 3.0);
    for (double y = top; y < H; y += 60)
    {
      cairo_move_to(cr, x - 46, y);
      cairo_line_to(cr, x + 46, y);
      stroke(cr, rgb(60, 44, 28), 1.5, 0.6);
    }
  }
  // Vines hanging down the wall.
  for (int i = 0; i < 9; ++i)
  {
    const double x = rng.range(0, float(W)), len = rng.range(180, 520);
    cairo_move_to(cr, x, top - 20);
    cairo_curve_to(cr, x + 30, top + len * 0.3, x - 30, top + len * 0.7, x + 10, top + len);
    stroke(cr, rgb(40, 110, 50), 7.0);
    for (double t = 0.1; t < 1.0; t += 0.15)
      ellipse(cr, x + std::sin(t * 9) * 16, top + len * t, 12, 6);
    setRgba(cr, rgb(70, 150, 70), 0.9);
    cairo_fill(cr);
  }
  // Torches.
  for (const double x : {W * 0.25, W * 0.75})
  {
    const double y = H - 380;
    softDot(cr, x, y - 30, 160, 140, rgb(255, 160, 60), 0.35);
    box(cr, x - 8, y, 16, 60, rgb(70, 46, 26));
    cairo_move_to(cr, x - 20, y);
    cairo_curve_to(cr, x - 24, y - 40, x - 4, y - 60, x, y - 90);
    cairo_curve_to(cr, x + 4, y - 60, x + 24, y - 40, x + 20, y);
    cairo_close_path(cr);
    vgrad(cr, y - 90, y, {{0.0, rgb(255, 250, 200)}, {0.5, rgb(255, 170, 50)}, {1.0, rgb(220, 70, 20)}});
  }
}

void paintStationFlat(cairo_t* cr, double W, double H)
{
  cairo_rectangle(cr, 0, 0, W, H);
  vgrad(cr, 0, H, {{0.0, rgb(30, 34, 48)}, {1.0, rgb(16, 18, 28)}});
  // The wall's panels.
  for (double y = 0; y < H; y += 192)
    for (double x = 0; x < W; x += 256)
    {
      roundRect(cr, x + 6, y + 6, 244, 180, 8);
      vgrad(cr, y, y + 192, {{0.0, rgb(70, 76, 96)}, {1.0, rgb(40, 44, 60)}}, 1.0, true);
      stroke(cr, rgb(14, 16, 24), 3.0);
      circle(cr, x + 18, y + 18, 3);
      circle(cr, x + 238, y + 18, 3);
      setRgba(cr, rgb(120, 126, 150), 1.0);
      cairo_fill(cr);
    }
  // The viewport: space, a ringed planet.
  const double cx = W * 0.5, cy = H * 0.6, rr = std::min(W * 0.34, 420.0);
  cairo_save(cr);
  circle(cr, cx, cy, rr);
  cairo_clip(cr);
  cairo_rectangle(cr, cx - rr, cy - rr, rr * 2, rr * 2);
  vgrad(cr, cy - rr, cy + rr, {{0.0, rgb(2, 2, 12)}, {1.0, rgb(10, 14, 40)}});
  Rng rng(42121u);
  for (int i = 0; i < 140; ++i)
  {
    circle(cr, cx + rng.range(-float(rr), float(rr)), cy + rng.range(-float(rr), float(rr)), rng.range(0.8f, 2.4f));
    setRgba(cr, rgb(255, 255, 255), rng.range(0.4f, 1.0f));
    cairo_fill(cr);
  }
  const double px = cx + rr * 0.35, py = cy + rr * 0.3, pr = rr * 0.55;
  softDot(cr, px, py, pr * 1.3, pr * 1.3, rgb(90, 160, 255), 0.35);
  circle(cr, px, py, pr);
  cairo_pattern_t* g = cairo_pattern_create_radial(px - pr * 0.4, py - pr * 0.4, pr * 0.1, px, py, pr);
  stop(g, 0.0, rgb(255, 210, 150));
  stop(g, 0.6, rgb(220, 110, 70));
  stop(g, 1.0, rgb(60, 20, 30));
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
  for (int k = 0; k < 3; ++k)
  {
    cairo_save(cr);
    cairo_translate(cr, px, py);
    cairo_rotate(cr, -0.3);
    cairo_scale(cr, pr * (1.5 + k * 0.12), pr * (0.32 + k * 0.03));
    cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
    cairo_restore(cr);
    stroke(cr, rgb(255, 230, 190), 5.0 - k, 0.6);
  }
  cairo_restore(cr);
  circle(cr, cx, cy, rr);
  stroke(cr, rgb(10, 12, 18), 30.0);
  circle(cr, cx, cy, rr);
  stroke(cr, kChrome, 16.0);
  circle(cr, cx, cy, rr - 6);
  stroke(cr, rgb(255, 255, 255), 2.0, 0.5);
  for (int k = 0; k < 12; ++k)
  {
    const double a = k * kPi / 6;
    circle(cr, cx + std::cos(a) * rr, cy + std::sin(a) * rr, 5);
    setRgba(cr, kChromeLo, 1.0);
    cairo_fill(cr);
  }
  // Ribs and lights.
  for (double x = 0; x <= W; x += 512)
  {
    cairo_rectangle(cr, x - 22, 0, 44, H);
    hgrad(cr, x - 22, x + 22, {{0.0, rgb(30, 32, 44)}, {0.5, rgb(110, 116, 140)}, {1.0, rgb(24, 26, 36)}});
    for (double y = 80; y < H; y += 220)
    {
      softDot(cr, x, y, 18, 18, kLed, 0.6);
      circle(cr, x, y, 5);
      setRgba(cr, rgb(200, 240, 255), 1.0);
      cairo_fill(cr);
    }
  }
  hazard(cr, 0, H - 56, W, 56, 48);
  roundRect(cr, cx - 170, H * 0.2, 340, 64, 8);
  setRgba(cr, rgb(10, 12, 20), 0.9);
  cairo_fill_preserve(cr);
  stroke(cr, kLed, 3.0);
  glowText(cr, "GRAVITY LAB", cx, H * 0.2 + 46, 38, rgb(210, 245, 255), kLed, 12);
}

void paintWesternFlat(cairo_t* cr, double W, double H)
{
  cairo_rectangle(cr, 0, 0, W, H);
  vgrad(cr, 0, H, {{0.0, rgb(36, 16, 50)}, {0.5, rgb(190, 80, 70)}, {0.8, rgb(255, 170, 90)}, {1.0, rgb(255, 210, 140)}});
  softDot(cr, W * 0.3, H * 0.68, 260, 260, rgb(255, 230, 150), 0.6);
  circle(cr, W * 0.3, H * 0.68, 110);
  setRgba(cr, rgb(255, 236, 180), 1.0);
  cairo_fill(cr);
  // Mesas.
  cairo_move_to(cr, 0, H);
  cairo_line_to(cr, 0, H - 420);
  cairo_line_to(cr, W * 0.1, H - 440);
  cairo_line_to(cr, W * 0.16, H - 300);
  cairo_line_to(cr, W * 0.6, H - 280);
  cairo_line_to(cr, W * 0.66, H - 500);
  cairo_line_to(cr, W * 0.86, H - 510);
  cairo_line_to(cr, W * 0.9, H - 330);
  cairo_line_to(cr, W, H - 320);
  cairo_line_to(cr, W, H);
  cairo_close_path(cr);
  vgrad(cr, H - 520, H, {{0.0, rgb(150, 60, 50)}, {1.0, rgb(80, 30, 34)}});
  box(cr, 0, H - 140, W, 140, rgb(170, 100, 60));
  // The saloon.
  const double x0 = W * 0.2, x1 = W * 0.8, top = H - 640;
  cairo_move_to(cr, x0, H);
  cairo_line_to(cr, x0, top + 120);
  cairo_line_to(cr, x0 + 60, top + 120);
  cairo_line_to(cr, x0 + 60, top);
  cairo_line_to(cr, x1 - 60, top);
  cairo_line_to(cr, x1 - 60, top + 120);
  cairo_line_to(cr, x1, top + 120);
  cairo_line_to(cr, x1, H);
  cairo_close_path(cr);
  cairo_save(cr);
  cairo_clip_preserve(cr);
  hgrad(cr, x0, x1, {{0.0, rgb(110, 64, 34)}, {0.5, rgb(150, 92, 50)}, {1.0, rgb(90, 50, 26)}});
  for (double x = x0; x < x1; x += 28)
  {
    cairo_move_to(cr, x, top);
    cairo_line_to(cr, x, H);
    stroke(cr, rgb(60, 34, 18), 2.0, 0.8);
  }
  cairo_restore(cr);
  stroke(cr, rgb(40, 22, 12), 4.0);
  roundRect(cr, W * 0.5 - 200, top + 26, 400, 80, 8);
  vgrad(cr, top + 26, top + 106, {{0.0, rgb(240, 220, 170)}, {1.0, rgb(200, 170, 110)}}, 1.0, true);
  stroke(cr, rgb(60, 34, 18), 4.0);
  text(cr, "SALOON", W * 0.5, top + 86, 56, rgb(120, 30, 20));
  // The porch roof and its posts.
  box(cr, x0 - 20, H - 360, x1 - x0 + 40, 26, rgb(70, 40, 22));
  for (double x = x0; x <= x1; x += (x1 - x0) / 4)
    box(cr, x - 7, H - 334, 14, 334, rgb(80, 46, 24));
  // Windows, warm.
  for (const double wx : {W * 0.32, W * 0.68})
  {
    cairo_rectangle(cr, wx - 60, top + 170, 120, 110);
    vgrad(cr, top + 170, top + 280, {{0.0, rgb(255, 220, 140)}, {1.0, rgb(230, 140, 60)}}, 1.0, true);
    stroke(cr, rgb(50, 28, 14), 6.0);
    cairo_move_to(cr, wx, top + 170);
    cairo_line_to(cr, wx, top + 280);
    stroke(cr, rgb(50, 28, 14), 4.0);
  }
  // The batwing doors.
  for (const double d : {-1.0, 1.0})
  {
    cairo_rectangle(cr, W * 0.5 + (d < 0 ? -76 : 4), H - 250, 72, 130);
    vgrad(cr, H - 250, H - 120, {{0.0, rgb(170, 110, 60)}, {1.0, rgb(110, 64, 32)}}, 1.0, true);
    stroke(cr, rgb(50, 28, 14), 4.0);
  }
  // A wanted poster.
  roundRect(cr, W * 0.36 - 50, H - 300, 100, 130, 4);
  setRgba(cr, rgb(240, 225, 180), 1.0);
  cairo_fill(cr);
  text(cr, "WANTED", W * 0.36, H - 272, 20, rgb(80, 30, 20));
  circle(cr, W * 0.36, H - 228, 26);
  setRgba(cr, rgb(120, 90, 60), 1.0);
  cairo_fill(cr);
  // Cacti.
  for (const double x : {W * 0.07, W * 0.93})
  {
    cairo_move_to(cr, x, H);
    cairo_line_to(cr, x, H - 260);
    stroke(cr, rgb(30, 90, 50), 34.0);
    cairo_move_to(cr, x - 40, H - 170);
    cairo_line_to(cr, x - 40, H - 120);
    cairo_line_to(cr, x, H - 120);
    stroke(cr, rgb(30, 90, 50), 22.0);
    cairo_move_to(cr, x + 40, H - 210);
    cairo_line_to(cr, x + 40, H - 150);
    cairo_line_to(cr, x, H - 150);
    stroke(cr, rgb(30, 90, 50), 22.0);
  }
}

void paintToonFlat(cairo_t* cr, double W, double H)
{
  const Color ink = rgb(20, 20, 40);
  cairo_rectangle(cr, 0, 0, W, H);
  vgrad(cr, 0, H, {{0.0, rgb(40, 120, 240)}, {1.0, rgb(170, 225, 255)}});
  // The sun, beaming.
  const double sx = W * 0.82, sy = H * 0.42;
  for (int k = 0; k < 12; ++k)
  {
    const double a = k * kPi / 6;
    cairo_move_to(cr, sx + std::cos(a) * 90, sy + std::sin(a) * 90);
    cairo_line_to(cr, sx + std::cos(a + 0.12) * 150, sy + std::sin(a + 0.12) * 150);
    cairo_line_to(cr, sx + std::cos(a - 0.12) * 150, sy + std::sin(a - 0.12) * 150);
    cairo_close_path(cr);
    setRgba(cr, rgb(255, 230, 90), 0.9);
    cairo_fill(cr);
  }
  circle(cr, sx, sy, 80);
  setRgba(cr, rgb(255, 220, 60), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, ink, 6.0);
  // Puffy clouds.
  for (int i = 0; i < 4; ++i)
  {
    const double cx = W * (0.12 + i * 0.25), cy = H * (0.36 + (i % 2) * 0.12);
    for (const double d : {-70.0, -20.0, 30.0, 80.0})
      circle(cr, cx + d, cy - (std::abs(d) < 50 ? 30 : 0), std::abs(d) < 50 ? 60 : 44);
    setRgba(cr, rgb(255, 255, 255), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, ink, 5.0);
    for (const double d : {-70.0, -20.0, 30.0, 80.0})
      circle(cr, cx + d, cy - (std::abs(d) < 50 ? 30 : 0), (std::abs(d) < 50 ? 60 : 44) - 4);
    setRgba(cr, rgb(255, 255, 255), 1.0);
    cairo_fill(cr);
  }
  // Painted mountains, then green hills.
  for (double x = -100; x < W + 200; x += 320)
  {
    const double peak = H - 470 - std::fmod(x * 0.37, 120.0);
    cairo_move_to(cr, x - 200, H - 200);
    cairo_line_to(cr, x, peak);
    cairo_line_to(cr, x + 200, H - 200);
    cairo_close_path(cr);
    vgrad(cr, peak, H - 200, {{0.0, rgb(160, 130, 230)}, {1.0, rgb(100, 80, 180)}}, 1.0, true);
    stroke(cr, ink, 5.0);
    // A snowcap.
    cairo_move_to(cr, x - 50, peak + 60);
    cairo_line_to(cr, x, peak);
    cairo_line_to(cr, x + 50, peak + 60);
    cairo_line_to(cr, x + 20, peak + 48);
    cairo_line_to(cr, x, peak + 64);
    cairo_line_to(cr, x - 22, peak + 46);
    cairo_close_path(cr);
    setRgba(cr, rgb(255, 255, 255), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, ink, 4.0);
  }
  cairo_move_to(cr, 0, H - 200);
  for (double x = 0; x <= W; x += 40)
    cairo_line_to(cr, x, H - 220 - 50 * std::sin(x * 0.008 + 1.0));
  cairo_line_to(cr, W, H);
  cairo_line_to(cr, 0, H);
  cairo_close_path(cr);
  vgrad(cr, H - 280, H, {{0.0, rgb(120, 220, 90)}, {1.0, rgb(50, 150, 60)}}, 1.0, true);
  stroke(cr, ink, 5.0);
  // A signpost: THIS WAY.
  const double px = W * 0.2, py = H - 160;
  box(cr, px - 8, py - 200, 16, 200, rgb(140, 90, 50));
  cairo_move_to(cr, px - 90, py - 200);
  cairo_line_to(cr, px + 90, py - 200);
  cairo_line_to(cr, px + 120, py - 170);
  cairo_line_to(cr, px + 90, py - 140);
  cairo_line_to(cr, px - 90, py - 140);
  cairo_close_path(cr);
  setRgba(cr, rgb(230, 190, 120), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, ink, 5.0);
  text(cr, "THIS WAY", px + 10, py - 158, 30, ink);
}

void paintStudioFlat(cairo_t* cr, double W, double H)
{
  cairo_rectangle(cr, 0, 0, W, H);
  vgrad(cr, 0, H, {{0.0, rgb(14, 14, 22)}, {0.6, rgb(46, 48, 66)}, {1.0, rgb(26, 28, 40)}});
  softDot(cr, W * 0.5, H * 0.72, W * 0.5, 260, rgb(110, 120, 170), 0.35);
  // The lighting grid and its lamps' pools of light.
  const double gy = H * 0.34;
  for (const double y : {gy, gy + 30})
  {
    cairo_move_to(cr, 0, y);
    cairo_line_to(cr, W, y);
    stroke(cr, rgb(70, 72, 90), 8.0);
  }
  for (double x = 40; x < W; x += 80)
  {
    cairo_move_to(cr, x, gy);
    cairo_line_to(cr, x, gy + 30);
    stroke(cr, rgb(60, 62, 80), 4.0);
  }
  for (double x = 120; x < W; x += 260)
  {
    cairo_move_to(cr, x - 22, gy + 34);
    cairo_line_to(cr, x + 22, gy + 34);
    cairo_line_to(cr, x + 70, H);
    cairo_line_to(cr, x - 70, H);
    cairo_close_path(cr);
    vgrad(cr, gy, H, {{0.0, rgb(255, 245, 220)}, {1.0, rgb(255, 245, 220)}}, 0.05);
    roundRect(cr, x - 24, gy + 30, 48, 50, 8);
    vgrad(cr, gy + 30, gy + 80, {{0.0, rgb(80, 80, 96)}, {1.0, rgb(20, 20, 28)}});
    softDot(cr, x, gy + 82, 28, 10, rgb(255, 245, 220), 0.9);
  }
  // STUDIO 42 on the cyc.
  glowText(cr, "STUDIO 42", W * 0.5, H * 0.56, 70, rgb(230, 235, 255), kLed, 18);
  // Cameras on their pedestals.
  for (int side = 0; side < 2; ++side)
  {
    const double x = side ? W * 0.86 : W * 0.14, dir = side ? -1.0 : 1.0, y = H - 40;
    box(cr, x - 60, y, 120, 20, rgb(20, 20, 26));
    box(cr, x - 10, y - 200, 20, 200, rgb(60, 62, 76));
    roundRect(cr, x - 70, y - 290, 140, 90, 10);
    vgrad(cr, y - 290, y - 200, {{0.0, rgb(90, 92, 110)}, {1.0, rgb(30, 30, 40)}}, 1.0, true);
    stroke(cr, rgb(10, 10, 16), 3.0);
    roundRect(cr, x + dir * 70 - (dir < 0 ? 50 : 0), y - 270, 50, 50, 6);
    setRgba(cr, rgb(20, 20, 26), 1.0);
    cairo_fill(cr);
    circle(cr, x - dir * 40, y - 270, 8);
    setRgba(cr, kOnAir, 1.0);
    cairo_fill(cr);
    text(cr, side ? "CAM 2" : "CAM 1", x, y - 230, 22, rgb(220, 220, 230));
  }
}

// The frame round a flat: gold-edged navy posts with bulbs, a header with
// the wagon's name; drawn in full-size units.
void paintFlatFrame(cairo_t* cr, double W, double H, const char* name)
{
  for (const double x : {0.0, W - 36})
  {
    cairo_rectangle(cr, x, 0, 36, H);
    hgrad(cr, x, x + 36, {{0.0, kNavyLo}, {0.5, kNavy}, {1.0, kNavyLo}}, 1.0, true);
    stroke(cr, kGold, 4.0);
    for (double y = 120; y < H; y += 64)
      bulb(cr, x + 18, y, 6, true);
  }
  cairo_rectangle(cr, 0, 0, W, 96);
  vgrad(cr, 0, 96, {{0.0, rgb(30, 34, 80)}, {1.0, kNavyLo}}, 1.0, true);
  stroke(cr, kGold, 5.0);
  glowText(cr, name, W * 0.5, 66, 52, kGoldHi, kGold, 14);
}

void paintFlat(cairo_t* cr, FnWagonKind kind, double W, double H)
{
  cairo_save(cr);
  cairo_scale(cr, 0.5, 0.5);
  switch (kind)
  {
    case FnWagonKind::Neon:
      paintNeonFlat(cr, W, H);
      break;
    case FnWagonKind::Temple:
      paintTempleFlat(cr, W, H);
      break;
    case FnWagonKind::Station:
      paintStationFlat(cr, W, H);
      break;
    case FnWagonKind::Western:
      paintWesternFlat(cr, W, H);
      break;
    case FnWagonKind::Toon:
      paintToonFlat(cr, W, H);
      break;
    case FnWagonKind::Studio:
      paintStudioFlat(cr, W, H);
      break;
  }
  paintFlatFrame(cr, W, H, finaleWagonName(kind));
  cairo_restore(cr);
}

// --- Set pieces ----------------------------------------------------------------------------------

// The stage door's pedestal (224 x 160, anchored bottom middle): a round
// gold-and-navy plinth on a lit riser, a star on its front.
void paintPedestal(cairo_t* cr)
{
  ellipse(cr, 112, 150, 108, 10);
  setRgba(cr, rgb(0, 0, 0), 0.4);
  cairo_fill(cr);
  cairo_move_to(cr, 24, 150);
  cairo_line_to(cr, 40, 70);
  cairo_line_to(cr, 184, 70);
  cairo_line_to(cr, 200, 150);
  cairo_close_path(cr);
  hgrad(cr, 24, 200, {{0.0, kNavyLo}, {0.4, rgb(50, 60, 140)}, {1.0, kNavyLo}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  ellipse(cr, 112, 70, 74, 12);
  vgrad(cr, 58, 82, {{0.0, kGoldHi}, {1.0, kGold}}, 1.0, true);
  stroke(cr, kGoldLo, 2.0);
  box(cr, 30, 122, 164, 4, kGold);
  starPath(cr, 112, 104, 16, 7);
  vgrad(cr, 88, 120, {{0.0, kGoldHi}, {1.0, kGold}}, 1.0, true);
  stroke(cr, kGoldLo, 1.2);
}

// A gravity plate (96 x 40, anchored at its middle on the floor): a steel
// pad with a cyan arrow, pointing up (it flips gravity up).
void paintPlate(cairo_t* cr, bool lit)
{
  roundRect(cr, 4, 14, 88, 22, 6);
  vgrad(cr, 14, 36, {{0.0, rgb(150, 156, 180)}, {1.0, rgb(50, 54, 70)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  roundRect(cr, 12, 18, 72, 12, 4);
  setRgba(cr, lit ? rgb(200, 245, 255) : rgb(40, 90, 130), 1.0);
  cairo_fill(cr);
  if (lit)
    softDot(cr, 48, 24, 50, 12, kLed, 0.6);
  cairo_move_to(cr, 48, 0);
  cairo_line_to(cr, 64, 14);
  cairo_line_to(cr, 54, 14);
  cairo_line_to(cr, 54, 20);
  cairo_line_to(cr, 42, 20);
  cairo_line_to(cr, 42, 14);
  cairo_line_to(cr, 32, 14);
  cairo_close_path(cr);
  setRgba(cr, lit ? rgb(255, 255, 255) : kLed, 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.4);
}

// A studio monitor on its stand (128 x 192, (0, 0) the portal's top-left):
// the housing, its screen (drawn live over it), CAM n on a plate, a tally
// lamp, the rolling stand.
void paintMonitor(cairo_t* cr, const std::string& label)
{
  // The stand.
  box(cr, 58, 132, 12, 46, kChromeLo);
  box(cr, 20, 176, 88, 8, rgb(30, 30, 40));
  for (const double x : {26.0, 102.0})
  {
    circle(cr, x, 186, 6);
    setRgba(cr, rgb(20, 20, 26), 1.0);
    cairo_fill(cr);
  }
  // The housing.
  roundRect(cr, 2, 6, 124, 128, 10);
  vgrad(cr, 6, 134, {{0.0, rgb(80, 84, 104)}, {0.3, rgb(40, 42, 56)}, {1.0, rgb(18, 18, 26)}}, 1.0, true);
  stroke(cr, kInk, 2.4);
  roundRect(cr, 12, 16, 104, 86, 6);
  setRgba(cr, rgb(6, 6, 12), 1.0);
  cairo_fill(cr);
  roundRect(cr, 30, 108, 68, 20, 4);
  setRgba(cr, rgb(14, 14, 20), 1.0);
  cairo_fill(cr);
  text(cr, label.c_str(), 64, 123, 13, rgb(230, 230, 240));
}

// The backstage refill (192 x 192, anchored bottom middle): a make-up
// station, a mirror ringed with bulbs, a red heart sticker and a cooler of
// water bottles; `lit` its bulbs.
void paintRefill(cairo_t* cr, bool lit)
{
  // The counter.
  roundRect(cr, 10, 110, 172, 82, 6);
  vgrad(cr, 110, 192, {{0.0, rgb(90, 60, 70)}, {1.0, rgb(40, 24, 34)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  box(cr, 10, 110, 172, 8, rgb(150, 110, 120));
  // The mirror and its bulbs.
  roundRect(cr, 36, 8, 120, 96, 8);
  vgrad(cr, 8, 104, {{0.0, rgb(150, 170, 200)}, {1.0, rgb(70, 80, 110)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  cairo_move_to(cr, 50, 20);
  cairo_line_to(cr, 90, 20);
  cairo_line_to(cr, 60, 70);
  cairo_close_path(cr);
  setRgba(cr, rgb(255, 255, 255), 0.18);
  cairo_fill(cr);
  for (double y = 16; y < 104; y += 22)
  {
    bulb(cr, 30, y, 5, lit);
    bulb(cr, 162, y, 5, lit);
  }
  for (double x = 52; x < 150; x += 24)
    bulb(cr, x, 4, 5, lit);
  // The heart.
  cairo_move_to(cr, 96, 76);
  cairo_curve_to(cr, 70, 58, 76, 32, 96, 44);
  cairo_curve_to(cr, 116, 32, 122, 58, 96, 76);
  setRgba(cr, lit ? rgb(255, 60, 90) : rgb(110, 50, 60), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.6);
  // The bottles.
  for (int i = 0; i < 4; ++i)
  {
    const double x = 30 + i * 18;
    roundRect(cr, x, 124, 12, 30, 4);
    setRgba(cr, rgb(170, 220, 255), 0.85);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.0);
    box(cr, x + 3, 120, 6, 5, rgb(40, 120, 220));
  }
  roundRect(cr, 116, 130, 54, 40, 6);
  setRgba(cr, lit ? rgb(255, 90, 90) : rgb(120, 60, 60), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.4);
  box(cr, 136, 140, 14, 20, rgb(255, 255, 255));
  box(cr, 128, 146, 30, 8, rgb(255, 255, 255));
}

// The ON AIR door (128 x 352, (32, 96) the top-left of its 64 x 256 cell
// column): a steel studio door in a heavy frame, a porthole, a push bar;
// the ON AIR box over it (lit separately) and STAGE 42 on a plate.
void paintStageDoor(cairo_t* cr)
{
  cairo_translate(cr, 32, 96);
  // The frame.
  roundRect(cr, -24, -20, 112, 276, 6);
  hgrad(cr, -24, 88, {{0.0, rgb(40, 42, 56)}, {0.5, rgb(90, 94, 116)}, {1.0, rgb(30, 32, 44)}}, 1.0, true);
  stroke(cr, kInk, 2.4);
  // The door.
  cairo_rectangle(cr, -8, 0, 80, 256);
  hgrad(cr, -8, 72, {{0.0, rgb(70, 20, 30)}, {0.4, rgb(140, 36, 50)}, {1.0, rgb(60, 14, 24)}}, 1.0, true);
  stroke(cr, kInk, 1.8);
  for (const double y : {40.0, 210.0})
  {
    cairo_rectangle(cr, 0, y, 64, 3);
    setRgba(cr, rgb(30, 6, 12), 0.6);
    cairo_fill(cr);
  }
  circle(cr, 32, 80, 18);
  vgrad(cr, 62, 98, {{0.0, rgb(130, 160, 200)}, {1.0, rgb(40, 50, 80)}}, 1.0, true);
  stroke(cr, kChrome, 4.0);
  roundRect(cr, -2, 140, 68, 10, 4);
  vgrad(cr, 140, 150, {{0.0, rgb(250, 250, 255)}, {1.0, kChromeLo}}, 1.0, true);
  stroke(cr, kInk, 1.2);
  hazard(cr, -8, 240, 80, 16, 16);
  // STAGE 42.
  roundRect(cr, -10, 16, 84, 26, 4);
  vgrad(cr, 16, 42, {{0.0, kGoldHi}, {0.5, kGold}, {1.0, kGoldLo}}, 1.0, true);
  stroke(cr, kInk, 1.2);
  text(cr, "STAGE 42", 32, 35, 14, kInk);
  // The ON AIR box's housing.
  roundRect(cr, -22, -84, 108, 52, 8);
  vgrad(cr, -84, -32, {{0.0, rgb(50, 50, 60)}, {1.0, rgb(16, 16, 22)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
}

// The ON AIR box's face (96 x 40): red glass and the words, lit or dark.
void paintOnAir(cairo_t* cr, bool lit)
{
  roundRect(cr, 2, 2, 92, 36, 5);
  vgrad(cr, 2, 38, lit ? std::initializer_list<std::pair<double, Color>>{{0.0, rgb(255, 120, 120)}, {1.0, rgb(200, 20, 40)}}
                       : std::initializer_list<std::pair<double, Color>>{{0.0, rgb(80, 30, 34)}, {1.0, rgb(40, 10, 16)}});
  text(cr, "ON AIR", 48, 28, 20, lit ? rgb(255, 250, 240) : rgb(120, 70, 70));
}

// A trapdoor (192 x 64, its top on the stage floor): a steel hatch with a
// diamond tread, hinges and a hazard border.
void paintTrapdoor(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 0, 192, 64);
  vgrad(cr, 0, 64, {{0.0, rgb(110, 116, 140)}, {0.2, rgb(70, 74, 96)}, {1.0, rgb(30, 32, 44)}});
  for (double y = 8; y < 60; y += 10)
    for (double x = 6 + (int(y) / 10 % 2) * 8; x < 190; x += 16)
    {
      cairo_move_to(cr, x, y);
      cairo_line_to(cr, x + 6, y - 3);
      stroke(cr, rgb(140, 146, 170), 1.6, 0.6);
    }
  hazard(cr, 0, 0, 192, 8, 18);
  for (const double x : {0.0, 186.0})
    box(cr, x, 0, 6, 64, rgb(20, 20, 28));
  for (const double x : {40.0, 152.0})
  {
    roundRect(cr, x - 12, 12, 24, 10, 3);
    setRgba(cr, kChromeLo, 1.0);
    cairo_fill(cr);
  }
  cairo_move_to(cr, 96, 8);
  cairo_line_to(cr, 96, 64);
  stroke(cr, rgb(10, 10, 16), 2.0);
}

// A race deck's block (64 x 40): a brushed plate over a truss, a light line.
void paintDeck(cairo_t* cr)
{
  for (double x = 0; x < 64; x += 32)
  {
    cairo_move_to(cr, x, 32);
    cairo_line_to(cr, x + 16, 12);
    cairo_line_to(cr, x + 32, 32);
    stroke(cr, kChromeLo, 3.0);
  }
  box(cr, 0, 30, 64, 4, kChromeLo);
  cairo_rectangle(cr, 0, 0, 64, 10);
  vgrad(cr, 0, 10, {{0.0, rgb(250, 250, 255)}, {0.4, kChrome}, {1.0, kChromeLo}});
  box(cr, 0, 10, 64, 3, rgb(200, 240, 255));
}

// The master switch's glass case (96 x 96, (16, 16) the top-left of its
// 64 x 64 cells): a glass dome on a chrome rim; `cracks` 0..4.
void paintGlass(cairo_t* cr, int cracks)
{
  cairo_translate(cr, 16, 16);
  roundRect(cr, -6, -10, 76, 82, 14);
  setRgba(cr, rgb(170, 220, 255), 0.22);
  cairo_fill_preserve(cr);
  stroke(cr, rgb(220, 245, 255), 2.4, 0.9);
  cairo_move_to(cr, 4, 4);
  cairo_curve_to(cr, 6, 30, 6, 48, 8, 60);
  stroke(cr, rgb(255, 255, 255), 4.0, 0.6);
  box(cr, -10, 66, 84, 10, kChrome);
  box(cr, -10, 72, 84, 4, kChromeLo);
  Rng rng(42131u);
  for (int c = 0; c < cracks; ++c)
  {
    double x = rng.range(10, 54), y = rng.range(6, 50);
    cairo_move_to(cr, x, y);
    for (int k = 0; k < 4; ++k)
    {
      x += rng.range(-14, 14);
      y += rng.range(-14, 14);
      cairo_line_to(cr, x, y);
    }
    stroke(cr, rgb(255, 255, 255), 1.6, 0.9);
  }
}

// --- The show's pieces ---------------------------------------------------------------------------

// Lance's hover podium (288 x 192; (16, 16) the top-left of its 256 x 128
// cells): a navy lectern with gold trim, the LANCE marquee panel on its
// front (open on the cue cards: the glowing weak point), two hover jets.
void paintPodium(cairo_t* cr, bool open)
{
  cairo_translate(cr, 16, 16);
  // The jets.
  for (const double x : {64.0, 192.0})
  {
    cairo_move_to(cr, x - 22, 112);
    cairo_line_to(cr, x + 22, 112);
    cairo_line_to(cr, x + 16, 140);
    cairo_line_to(cr, x - 16, 140);
    cairo_close_path(cr);
    hgrad(cr, x - 22, x + 22, {{0.0, rgb(40, 42, 56)}, {0.5, kChrome}, {1.0, rgb(30, 30, 40)}}, 1.0, true);
    stroke(cr, kInk, 1.8);
  }
  // The lectern: wider at the top.
  cairo_move_to(cr, 0, 10);
  cairo_line_to(cr, 256, 10);
  cairo_line_to(cr, 232, 120);
  cairo_line_to(cr, 24, 120);
  cairo_close_path(cr);
  hgrad(cr, 0, 256, {{0.0, kNavyLo}, {0.35, rgb(46, 56, 140)}, {1.0, kNavyLo}}, 1.0, true);
  stroke(cr, kInk, 2.4);
  roundRect(cr, -6, 0, 268, 16, 6);
  vgrad(cr, 0, 16, {{0.0, kGoldHi}, {0.5, kGold}, {1.0, kGoldLo}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  box(cr, 26, 112, 204, 6, kGold);
  // The panel.
  roundRect(cr, 44, 30, 168, 66, 8);
  if (open)
  {
    vgrad(cr, 30, 96, {{0.0, rgb(255, 240, 180)}, {1.0, rgb(255, 120, 40)}}, 1.0, true);
    stroke(cr, kInk, 2.0);
    softDot(cr, 128, 63, 80, 30, rgb(255, 255, 220), 0.8);
    circle(cr, 128, 63, 18);
    setRgba(cr, rgb(255, 255, 255), 1.0);
    cairo_fill(cr);
  }
  else
  {
    vgrad(cr, 30, 96, {{0.0, rgb(30, 10, 20)}, {1.0, rgb(10, 4, 10)}}, 1.0, true);
    stroke(cr, kGold, 2.4);
    glowText(cr, "LANCE", 128, 76, 34, kGoldHi, kGold, 10);
    for (double x = 52; x < 210; x += 16)
    {
      bulb(cr, x, 36, 3, true);
      bulb(cr, x, 90, 3, true);
    }
  }
}

// The prize wheel (176 x 176, centre (88, 88)): four wedges with their
// attacks' icons (a gold bar, a trapdoor, HA!, a lamp), a gold rim of
// bulbs; it turns as a whole.
void paintWheel(cairo_t* cr)
{
  static const Color kWedge[] = {rgb(255, 200, 60), rgb(80, 90, 120), rgb(255, 90, 140), rgb(90, 200, 255)};
  const double cx = 88, cy = 88, rr = 80;
  for (int k = 0; k < 8; ++k)
  {
    const double a0 = -kPi / 2 + k * kPi / 4, a1 = a0 + kPi / 4;
    cairo_move_to(cr, cx, cy);
    cairo_arc(cr, cx, cy, rr, a0, a1);
    cairo_close_path(cr);
    setRgba(cr, k % 2 ? darken(kWedge[k / 2], 0.25f) : kWedge[k / 2], 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.6);
  }
  // The icons, one per attack's pair of wedges.
  for (int k = 0; k < 4; ++k)
  {
    const double a = -kPi / 2 + k * kPi / 2 + kPi / 4;
    const double ix = cx + std::cos(a) * 50, iy = cy + std::sin(a) * 50;
    switch (k)
    {
      case 0:
        cairo_move_to(cr, ix - 14, iy + 6);
        cairo_line_to(cr, ix + 14, iy + 6);
        cairo_line_to(cr, ix + 9, iy - 6);
        cairo_line_to(cr, ix - 9, iy - 6);
        cairo_close_path(cr);
        setRgba(cr, kGoldHi, 1.0);
        cairo_fill_preserve(cr);
        stroke(cr, kInk, 1.4);
        break;
      case 1:
        box(cr, ix - 13, iy - 6, 26, 12, rgb(10, 10, 14));
        hazard(cr, ix - 13, iy - 9, 26, 4, 6);
        break;
      case 2:
        text(cr, "HA!", ix, iy + 6, 16, rgb(255, 255, 255));
        break;
      default:
        cairo_move_to(cr, ix - 8, iy - 8);
        cairo_line_to(cr, ix + 8, iy - 8);
        cairo_line_to(cr, ix + 13, iy + 8);
        cairo_line_to(cr, ix - 13, iy + 8);
        cairo_close_path(cr);
        setRgba(cr, rgb(40, 40, 50), 1.0);
        cairo_fill(cr);
        ellipse(cr, ix, iy + 8, 12, 4);
        setRgba(cr, rgb(255, 250, 220), 1.0);
        cairo_fill(cr);
        break;
    }
  }
  circle(cr, cx, cy, rr);
  stroke(cr, kGold, 8.0);
  for (int k = 0; k < 16; ++k)
  {
    const double a = k * kPi / 8;
    bulb(cr, cx + std::cos(a) * rr, cy + std::sin(a) * rr, 3.2, k % 2 == 0);
  }
  circle(cr, cx, cy, 12);
  vgrad(cr, cy - 12, cy + 12, {{0.0, kGoldHi}, {1.0, kGoldLo}}, 1.0, true);
  stroke(cr, kInk, 1.4);
}

// The wheel's pointer (32 x 40, its tip at (16, 36)).
void paintPointer(cairo_t* cr)
{
  cairo_move_to(cr, 4, 4);
  cairo_line_to(cr, 28, 4);
  cairo_line_to(cr, 16, 36);
  cairo_close_path(cr);
  vgrad(cr, 4, 36, {{0.0, rgb(255, 120, 120)}, {1.0, rgb(200, 20, 40)}}, 1.0, true);
  stroke(cr, kInk, 1.8);
}

// A fan of cue cards in a hand (90 x 70): white cards, scrawled lines.
void paintCueCards(cairo_t* cr)
{
  for (int k = 0; k < 3; ++k)
  {
    cairo_save(cr);
    cairo_translate(cr, 45, 60);
    cairo_rotate(cr, -0.3 + k * 0.25);
    roundRect(cr, -24, -56, 48, 40, 3);
    setRgba(cr, rgb(250, 250, 240), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.4);
    for (int l = 0; l < 3; ++l)
    {
      cairo_move_to(cr, -18, -48 + l * 10);
      cairo_line_to(cr, 14 - l * 6, -48 + l * 10);
      stroke(cr, rgb(40, 60, 160), 2.0);
    }
    cairo_restore(cr);
  }
  ellipse(cr, 45, 60, 12, 9);
  setRgba(cr, rgb(244, 204, 168), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.4);
}

// A column of gold bars falling (192 x 128): three ingots stacked, gleaming.
void paintGold(cairo_t* cr)
{
  for (int k = 0; k < 3; ++k)
  {
    const double x = 26 + (k % 2) * 40 + k * 10, y = 16 + k * 36;
    cairo_move_to(cr, x, y + 30);
    cairo_line_to(cr, x + 110, y + 30);
    cairo_line_to(cr, x + 94, y);
    cairo_line_to(cr, x + 16, y);
    cairo_close_path(cr);
    vgrad(cr, y, y + 30, {{0.0, kGoldHi}, {0.4, kGold}, {1.0, kGoldLo}}, 1.0, true);
    stroke(cr, rgb(90, 56, 10), 2.0);
    text(cr, "999", x + 55, y + 22, 13, kGoldLo);
  }
  softDot(cr, 60, 20, 16, 16, rgb(255, 255, 255), 0.8);
}

// A studio lamp falling (192 x 160, anchored bottom middle): a big fresnel
// can on its yoke, its lens blazing.
void paintLamp(cairo_t* cr)
{
  cairo_move_to(cr, 40, 30);
  cairo_curve_to(cr, 40, 0, 152, 0, 152, 30);
  stroke(cr, rgb(40, 40, 50), 8.0);
  roundRect(cr, 30, 24, 132, 100, 18);
  hgrad(cr, 30, 162, {{0.0, rgb(30, 30, 40)}, {0.4, rgb(90, 92, 110)}, {1.0, rgb(20, 20, 28)}}, 1.0, true);
  stroke(cr, kInk, 2.4);
  for (double x = 50; x < 150; x += 16)
  {
    cairo_move_to(cr, x, 30);
    cairo_line_to(cr, x, 60);
    stroke(cr, rgb(16, 16, 22), 3.0);
  }
  ellipse(cr, 96, 132, 64, 20);
  vgrad(cr, 112, 152, {{0.0, rgb(255, 255, 240)}, {1.0, rgb(255, 210, 120)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  softDot(cr, 96, 140, 90, 26, rgb(255, 250, 220), 0.6);
}

// A gold coin on its edge (64 x 64), rolling (frame turns it).
void paintCoin(cairo_t* cr, int f)
{
  const double sx = std::abs(std::cos(f * kPi / 4)) * 26 + 6;
  ellipse(cr, 32, 32, sx, 28);
  vgrad(cr, 4, 60, {{0.0, kGoldHi}, {0.5, kGold}, {1.0, kGoldLo}}, 1.0, true);
  stroke(cr, rgb(90, 56, 10), 2.0);
  if (sx > 16)
    text(cr, "$", 32, 42, 28, kGoldLo);
}

// The Encore Engine (576 x 576; (32, 32) the top-left of its 512 x 512
// cells), facing left: a giant built of the season's kit. A CRT head with
// a jaw (`jaw` open), copper Tesla rods on its shoulders, a speaker stack
// for a chest, a gold fist (`fist` raised over its head) and a horseshoe
// magnet for the other hand, road-case legs; its core housing low at the
// front (the core's state is drawn over it).
void paintEngine(cairo_t* cr, bool jaw, bool fist)
{
  cairo_translate(cr, 32, 32);
  const Color steel = rgb(84, 90, 118), steelLo = rgb(34, 36, 52), ink = kInk;
  auto plate = [&](double x, double y, double w, double h, double rad, Color c) {
    roundRect(cr, x, y, w, h, rad);
    cairo_pattern_t* g = cairo_pattern_create_linear(x, y, x + w * 0.4, y + h);
    stop(g, 0.0, lighten(c, 0.25f));
    stop(g, 0.5, c);
    stop(g, 1.0, darken(c, 0.45f));
    cairo_set_source(cr, g);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(g);
    stroke(cr, ink, 3.0);
  };
  // The back arm: a horseshoe magnet.
  cairo_move_to(cr, 420, 170);
  cairo_line_to(cr, 470, 280);
  stroke(cr, ink, 34.0);
  cairo_move_to(cr, 420, 170);
  cairo_line_to(cr, 470, 280);
  stroke(cr, steelLo, 26.0);
  cairo_arc(cr, 470, 330, 40, kPi, 2 * kPi);
  stroke(cr, ink, 34.0);
  cairo_arc(cr, 470, 330, 40, kPi, 2 * kPi);
  stroke(cr, rgb(220, 40, 50), 26.0);
  for (const double x : {430.0, 510.0})
  {
    cairo_move_to(cr, x, 330);
    cairo_line_to(cr, x, 372);
    stroke(cr, ink, 34.0);
    cairo_move_to(cr, x, 330);
    cairo_line_to(cr, x, 372);
    stroke(cr, rgb(220, 40, 50), 26.0);
    cairo_move_to(cr, x, 356);
    cairo_line_to(cr, x, 372);
    stroke(cr, kChrome, 26.0);
  }
  // The legs: road cases with hazard bands, wheels for feet.
  for (const double x : {150.0, 330.0})
  {
    plate(x, 380, 110, 120, 10, rgb(40, 40, 52));
    hazard(cr, x + 6, 420, 98, 16, 18);
    for (const double wx : {x + 22, x + 88})
    {
      circle(cr, wx, 500, 12);
      setRgba(cr, rgb(16, 16, 20), 1.0);
      cairo_fill(cr);
    }
  }
  // The pelvis.
  plate(100, 330, 360, 70, 16, steelLo);
  // The torso: a speaker stack.
  plate(110, 120, 330, 230, 20, steel);
  for (const auto& sp : {std::array<double, 3>{200, 190, 52}, std::array<double, 3>{340, 190, 52},
         std::array<double, 3>{270, 290, 40}})
  {
    circle(cr, sp[0], sp[1], sp[2]);
    setRgba(cr, rgb(20, 20, 28), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kChrome, 5.0);
    circle(cr, sp[0], sp[1], sp[2] * 0.62);
    cairo_pattern_t* g = cairo_pattern_create_radial(sp[0] - 8, sp[1] - 8, 2, sp[0], sp[1], sp[2] * 0.62);
    stop(g, 0.0, rgb(110, 110, 130));
    stop(g, 1.0, rgb(30, 30, 40));
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
    circle(cr, sp[0], sp[1], sp[2] * 0.2);
    setRgba(cr, rgb(60, 60, 76), 1.0);
    cairo_fill(cr);
  }
  // A gold trim and a 42 on the chest.
  roundRect(cr, 110, 120, 330, 230, 20);
  stroke(cr, kGold, 3.0);
  text(cr, "42", 400, 340, 34, kGold);
  // The shoulders and their Tesla rods.
  for (const double x : {170.0, 380.0})
  {
    plate(x - 50, 92, 100, 52, 22, steel);
    box(cr, x - 6, 0, 12, 96, rgb(150, 90, 40));
    for (double y = 20; y < 90; y += 10)
    {
      ellipse(cr, x, y, 14, 4);
      setRgba(cr, rgb(210, 130, 60), 1.0);
      cairo_fill_preserve(cr);
      stroke(cr, rgb(90, 50, 20), 1.0);
    }
    circle(cr, x, 0, 12);
    vgrad(cr, -12, 12, {{0.0, rgb(240, 240, 255)}, {1.0, kChromeLo}}, 1.0, true);
    stroke(cr, ink, 2.0);
  }
  // The head: a CRT with an angry face, toward the stage (left).
  plate(20, 40, 170, 130, 18, rgb(60, 60, 80));
  roundRect(cr, 36, 56, 138, 92, 14);
  setRgba(cr, rgb(10, 20, 14), 1.0);
  cairo_fill(cr);
  for (const double ex : {72.0, 138.0})
  {
    cairo_move_to(cr, ex - 22, 86);
    cairo_line_to(cr, ex + 22, 96);
    cairo_line_to(cr, ex + 18, 108);
    cairo_line_to(cr, ex - 22, 104);
    cairo_close_path(cr);
    setRgba(cr, rgb(255, 60, 60), 1.0);
    cairo_fill(cr);
    softDot(cr, ex, 100, 30, 18, rgb(255, 60, 60), 0.45);
  }
  for (double y = 58; y < 148; y += 6)
    box(cr, 36, y, 138, 1.5, rgb(0, 0, 0), 0.4);
  // The jaw: a hinged grille under the screen, dropped when open.
  const double jy = jaw ? 196 : 168;
  if (jaw)
  {
    cairo_move_to(cr, 30, 168);
    cairo_line_to(cr, 180, 168);
    cairo_line_to(cr, 170, jy);
    cairo_line_to(cr, 40, jy);
    cairo_close_path(cr);
    setRgba(cr, rgb(40, 255, 200), 0.9);
    cairo_fill(cr);
    softDot(cr, 60, 182, 60, 30, rgb(140, 255, 220), 0.8);
  }
  plate(28, jy, 156, 32, 8, rgb(70, 70, 92));
  for (double x = 44; x < 176; x += 14)
  {
    cairo_move_to(cr, x, jy + 6);
    cairo_line_to(cr, x, jy + 26);
    stroke(cr, rgb(20, 20, 28), 3.0);
  }
  // The front arm: down at its side or up over its head, ending in a gold fist.
  const double fx = fist ? 90 : 60, fy = fist ? -10 : 330;
  cairo_move_to(cr, 150, 150);
  cairo_line_to(cr, fist ? 110 : 80, fist ? 60 : 250);
  cairo_line_to(cr, fx, fy);
  stroke(cr, ink, 46.0);
  cairo_move_to(cr, 150, 150);
  cairo_line_to(cr, fist ? 110 : 80, fist ? 60 : 250);
  cairo_line_to(cr, fx, fy);
  stroke(cr, steel, 36.0);
  plate(fx - 46, fy - 38, 92, 80, 24, kGold);
  for (int k = 0; k < 3; ++k)
  {
    cairo_move_to(cr, fx - 30 + k * 22, fy - 34);
    cairo_line_to(cr, fx - 30 + k * 22, fy - 6);
    stroke(cr, kGoldLo, 3.0);
  }
  // The core's housing, low at the front.
  plate(-4, 316, 136, 136, 30, rgb(50, 54, 72));
}

// The core's face (128 x 128): shuttered or open and blazing.
void paintCore(cairo_t* cr, bool open)
{
  circle(cr, 64, 64, 52);
  if (open)
  {
    cairo_pattern_t* g = cairo_pattern_create_radial(64, 64, 4, 64, 64, 52);
    stop(g, 0.0, rgb(255, 255, 255));
    stop(g, 0.35, rgb(255, 240, 140));
    stop(g, 1.0, rgb(255, 120, 30));
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  else
  {
    setRgba(cr, rgb(24, 26, 36), 1.0);
    cairo_fill(cr);
    for (int k = 0; k < 6; ++k)
    {
      const double a = k * kPi / 3;
      cairo_move_to(cr, 64, 64);
      cairo_line_to(cr, 64 + std::cos(a) * 52, 64 + std::sin(a) * 52);
      stroke(cr, rgb(90, 94, 120), 3.0);
    }
    circle(cr, 64, 64, 10);
    setRgba(cr, rgb(255, 140, 40), 0.7);
    cairo_fill(cr);
  }
  circle(cr, 64, 64, 52);
  stroke(cr, kChrome, 7.0);
  circle(cr, 64, 64, 58);
  stroke(cr, kInk, 2.0);
}

// The broadcast desk (224 x 120, anchored bottom middle): a curved gold
// desk with GUNRUNNERS LIVE on its front, two monitors on it.
void paintDesk(cairo_t* cr)
{
  for (const double x : {50.0, 150.0})
  {
    roundRect(cr, x - 30, 6, 60, 42, 4);
    vgrad(cr, 6, 48, {{0.0, rgb(70, 74, 96)}, {1.0, rgb(20, 20, 28)}}, 1.0, true);
    stroke(cr, kInk, 1.6);
    roundRect(cr, x - 25, 10, 50, 32, 3);
    vgrad(cr, 10, 42, {{0.0, rgb(90, 180, 255)}, {1.0, rgb(30, 60, 140)}});
  }
  cairo_move_to(cr, 4, 56);
  cairo_curve_to(cr, 60, 46, 164, 46, 220, 56);
  cairo_line_to(cr, 204, 120);
  cairo_line_to(cr, 20, 120);
  cairo_close_path(cr);
  hgrad(cr, 4, 220, {{0.0, kNavyLo}, {0.5, rgb(46, 56, 140)}, {1.0, kNavyLo}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  cairo_move_to(cr, 4, 56);
  cairo_curve_to(cr, 60, 46, 164, 46, 220, 56);
  stroke(cr, kGold, 6.0);
  text(cr, "GUNRUNNERS LIVE", 112, 96, 15, kGoldHi);
}

// Backstage's wall (640 x 704, the ten blocks x 110-119 from row 29 to the
// floor): plum-painted block wall, a costume rack, a QUIET PLEASE lamp,
// Lance's dressing-room door with its gold star.
void paintBackstage(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 0, 640, 704);
  vgrad(cr, 0, 704, {{0.0, rgb(30, 16, 34)}, {1.0, rgb(56, 30, 52)}});
  for (double y = 0; y < 704; y += 32)
    for (double x = (int(y) / 32 % 2) * 32 - 32; x < 640; x += 64)
    {
      roundRect(cr, x + 2, y + 2, 60, 28, 3);
      setRgba(cr, rgb(70, 40, 64), 0.6);
      cairo_fill(cr);
    }
  // A costume rack.
  cairo_move_to(cr, 200, 470);
  cairo_line_to(cr, 380, 470);
  stroke(cr, kChrome, 5.0);
  for (const double x : {200.0, 380.0})
  {
    cairo_move_to(cr, x, 470);
    cairo_line_to(cr, x, 704);
    stroke(cr, kChromeLo, 5.0);
  }
  static const Color kCoats[] = {rgb(230, 60, 90), rgb(60, 140, 255), rgb(255, 200, 60), rgb(150, 90, 230), rgb(240, 240, 240)};
  for (int i = 0; i < 5; ++i)
  {
    const double x = 220 + i * 34;
    cairo_move_to(cr, x, 474);
    cairo_line_to(cr, x - 18, 500);
    cairo_line_to(cr, x - 14, 600);
    cairo_line_to(cr, x + 14, 600);
    cairo_line_to(cr, x + 18, 500);
    cairo_close_path(cr);
    setRgba(cr, kCoats[i], 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.6);
  }
  // QUIET PLEASE.
  roundRect(cr, 210, 330, 180, 52, 6);
  setRgba(cr, rgb(30, 10, 14), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 2.0);
  text(cr, "QUIET PLEASE", 300, 364, 20, rgb(255, 120, 110));
  // Lance's door (blocks 117-118: x 448-576).
  roundRect(cr, 452, 420, 120, 284, 4);
  hgrad(cr, 452, 572, {{0.0, rgb(200, 190, 210)}, {0.5, rgb(250, 245, 255)}, {1.0, rgb(170, 160, 180)}}, 1.0, true);
  stroke(cr, kInk, 2.4);
  starPath(cr, 512, 500, 40, 17);
  vgrad(cr, 460, 540, {{0.0, kGoldHi}, {1.0, kGold}}, 1.0, true);
  stroke(cr, kGoldLo, 2.0);
  text(cr, "LANCE", 512, 510, 16, kInk);
  circle(cr, 556, 580, 6);
  setRgba(cr, kGold, 1.0);
  cairo_fill(cr);
}

// The arena's back wall (2560 x 1024, the forty blocks x 120-159 from row
// 24 to the floor): a giant LED wall in modules, THE LIVE FINALE over it,
// the gantry along it at the cell row Lance runs on, a pair of big ON AIR
// boxes, the light towers at the sides.
void paintArena(cairo_t* cr, double gantryY)
{
  cairo_rectangle(cr, 0, 0, 2560, 1024);
  vgrad(cr, 0, 1024, {{0.0, rgb(4, 4, 16)}, {1.0, rgb(16, 16, 44)}});
  // The LED wall.
  const double wx0 = 220, wx1 = 2340, wy0 = 60, wy1 = 900;
  cairo_rectangle(cr, wx0, wy0, wx1 - wx0, wy1 - wy0);
  vgrad(cr, wy0, wy1, {{0.0, rgb(10, 18, 70)}, {0.5, rgb(30, 50, 150)}, {1.0, rgb(8, 12, 50)}});
  for (double x = wx0; x < wx1; x += 6)
  {
    const double v = 0.5 + 0.5 * std::sin((x - wx0) / 260.0);
    box(cr, x, wy0 + 100 + v * 260, 6, 300, rgb(80, 150, 255), 0.06 + 0.06 * v);
  }
  for (double x = wx0; x <= wx1; x += 106)
    box(cr, x, wy0, 2, wy1 - wy0, rgb(4, 6, 20), 0.85);
  for (double y = wy0; y <= wy1; y += 106)
    box(cr, wx0, y, wx1 - wx0, 2, rgb(4, 6, 20), 0.85);
  softDot(cr, 1280, 340, 700, 200, kGold, 0.18);
  glowText(cr, "THE LIVE FINALE", 1280, 330, 150, kGoldHi, kGold, 34);
  glowText(cr, "SEASON RECAP  -  42 JOBS  -  ONE SHOW", 1280, 430, 44, rgb(220, 240, 255), kLed, 12);
  cairo_rectangle(cr, wx0 - 10, wy0 - 10, wx1 - wx0 + 20, wy1 - wy0 + 20);
  stroke(cr, rgb(40, 42, 60), 20.0);
  cairo_rectangle(cr, wx0 - 10, wy0 - 10, wx1 - wx0 + 20, wy1 - wy0 + 20);
  stroke(cr, kGold, 4.0);
  // The light towers.
  for (const double x : {100.0, 2460.0})
  {
    for (double y = 40; y < 1024; y += 60)
    {
      cairo_move_to(cr, x - 40, y);
      cairo_line_to(cr, x + 40, y + 60);
      cairo_move_to(cr, x + 40, y);
      cairo_line_to(cr, x - 40, y + 60);
      stroke(cr, rgb(70, 72, 96), 3.0);
    }
    for (const double dx : {-40.0, 40.0})
    {
      cairo_move_to(cr, x + dx, 0);
      cairo_line_to(cr, x + dx, 1024);
      stroke(cr, rgb(90, 92, 116), 6.0);
    }
    for (double y = 120; y < 800; y += 160)
    {
      softDot(cr, x, y, 40, 40, kGoldHi, 0.5);
      circle(cr, x, y, 16);
      setRgba(cr, rgb(255, 250, 220), 1.0);
      cairo_fill(cr);
    }
  }
  // The gantry along the wall.
  box(cr, 0, gantryY, 2560, 18, rgb(60, 62, 84));
  box(cr, 0, gantryY, 2560, 4, kChrome);
  for (double x = 0; x < 2560; x += 64)
  {
    cairo_move_to(cr, x, gantryY + 18);
    cairo_line_to(cr, x + 32, gantryY + 50);
    cairo_line_to(cr, x + 64, gantryY + 18);
    stroke(cr, rgb(50, 52, 70), 3.0);
  }
  box(cr, 0, gantryY + 50, 2560, 6, rgb(50, 52, 70));
  for (double x = 32; x < 2560; x += 128)
  {
    cairo_move_to(cr, x, gantryY - 50);
    cairo_line_to(cr, x, gantryY);
    stroke(cr, kChromeLo, 3.0);
  }
  box(cr, 0, gantryY - 52, 2560, 4, kChromeLo);
}

// One frame of snow (320 x 180, drawn 4 x).
void paintSnow(cairo_t* cr, int seed)
{
  Rng rng(42141u + std::uint32_t(seed) * 977u);
  for (double y = 0; y < 180; y += 2)
    for (double x = 0; x < 320; x += 2)
    {
      const float v = rng.uniform();
      box(cr, x, y, 2, 2, rgb(int(v * 255), int(v * 255), int(v * 260 > 255 ? 255 : v * 260)));
    }
  for (int k = 0; k < 4; ++k)
    box(cr, 0, rng.range(0, 176), 320, rng.range(2, 6), rgb(255, 255, 255), 0.4);
}

// CUT TO COMMERCIAL (1280 x 720): the network's card, colour bars, the
// Halcyon Arms shield.
void paintCommercial(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 0, 1280, 720);
  vgrad(cr, 0, 720, {{0.0, rgb(16, 20, 60)}, {1.0, rgb(4, 4, 20)}});
  static const Color kBars[] = {rgb(200, 200, 200), rgb(200, 200, 30), rgb(30, 200, 200), rgb(30, 200, 30),
    rgb(200, 30, 200), rgb(200, 30, 30), rgb(30, 30, 200)};
  for (int i = 0; i < 7; ++i)
    box(cr, i * 1280.0 / 7, 560, 1280.0 / 7 + 1, 160, kBars[i]);
  box(cr, 0, 556, 1280, 4, rgb(0, 0, 0));
  // The shield.
  const double cx = 640, cy = 190, s = 80;
  softDot(cr, cx, cy, 240, 160, rgb(80, 130, 255), 0.35);
  cairo_move_to(cr, cx - s, cy - s);
  cairo_line_to(cr, cx + s, cy - s);
  cairo_curve_to(cr, cx + s, cy + s * 0.4, cx + s * 0.4, cy + s * 0.9, cx, cy + s * 1.2);
  cairo_curve_to(cr, cx - s * 0.4, cy + s * 0.9, cx - s, cy + s * 0.4, cx - s, cy - s);
  cairo_close_path(cr);
  vgrad(cr, cy - s, cy + s * 1.2, {{0.0, rgb(60, 110, 230)}, {1.0, rgb(20, 40, 120)}}, 1.0, true);
  stroke(cr, kGold, 10.0);
  text(cr, "H", cx, cy + 36, 100, kGoldHi);
  glowText(cr, "CUT TO COMMERCIAL", 640, 400, 66, kGoldHi, kGold, 18);
  text(cr, "HALCYON ARMS  -  WE'LL BE RIGHT BACK", 640, 470, 28, rgb(230, 235, 255));
}

// The BLOOPER! clapperboard (520 x 300).
void paintClapper(cairo_t* cr)
{
  // The sticks, the top one open.
  cairo_save(cr);
  cairo_translate(cr, 20, 70);
  cairo_rotate(cr, -0.18);
  cairo_rectangle(cr, 0, -40, 480, 40);
  setRgba(cr, rgb(20, 20, 24), 1.0);
  cairo_fill(cr);
  for (double x = 0; x < 480; x += 80)
  {
    cairo_move_to(cr, x + 10, -40);
    cairo_line_to(cr, x + 50, -40);
    cairo_line_to(cr, x + 30, 0);
    cairo_line_to(cr, x - 10, 0);
    cairo_close_path(cr);
    setRgba(cr, rgb(250, 250, 250), 1.0);
    cairo_fill(cr);
  }
  cairo_restore(cr);
  cairo_rectangle(cr, 20, 72, 480, 36);
  setRgba(cr, rgb(20, 20, 24), 1.0);
  cairo_fill(cr);
  for (double x = 20; x < 500; x += 80)
  {
    cairo_move_to(cr, x + 10, 72);
    cairo_line_to(cr, x + 50, 72);
    cairo_line_to(cr, x + 30, 108);
    cairo_line_to(cr, x - 10, 108);
    cairo_close_path(cr);
    setRgba(cr, rgb(250, 250, 250), 1.0);
    cairo_fill(cr);
  }
  // The slate.
  roundRect(cr, 20, 108, 480, 186, 8);
  vgrad(cr, 108, 294, {{0.0, rgb(40, 40, 48)}, {1.0, rgb(16, 16, 20)}}, 1.0, true);
  stroke(cr, rgb(240, 240, 240), 3.0);
  for (const double y : {170.0, 232.0})
  {
    cairo_move_to(cr, 30, y);
    cairo_line_to(cr, 490, y);
    stroke(cr, rgb(200, 200, 200), 2.0, 0.6);
  }
  cairo_move_to(cr, 260, 170);
  cairo_line_to(cr, 260, 294);
  stroke(cr, rgb(200, 200, 200), 2.0, 0.6);
  text(cr, "SCENE 42", 145, 210, 24, rgb(240, 240, 240));
  text(cr, "TAKE 99", 375, 210, 24, rgb(240, 240, 240));
}

// A HUD plate (w x h): navy glass with a gold edge.
void paintPlateHud(cairo_t* cr, double w, double h)
{
  roundRect(cr, 2, 2, w - 4, h - 4, 8);
  vgrad(cr, 2, h - 2, {{0.0, rgba(30, 34, 80, 235)}, {1.0, rgba(8, 10, 28, 235)}}, 1.0, true);
  stroke(cr, kGold, 2.0);
  roundRect(cr, 5, 5, w - 10, h * 0.4, 6);
  setRgba(cr, rgb(255, 255, 255), 0.06);
  cairo_fill(cr);
}

// A small TV (28 x 24) for the ticker.
void paintTv(cairo_t* cr)
{
  cairo_move_to(cr, 8, 2);
  cairo_line_to(cr, 14, 7);
  cairo_line_to(cr, 20, 2);
  stroke(cr, kChrome, 1.6);
  roundRect(cr, 2, 7, 24, 16, 3);
  setRgba(cr, kGold, 1.0);
  cairo_fill(cr);
  roundRect(cr, 5, 10, 15, 10, 2);
  setRgba(cr, rgb(20, 40, 90), 1.0);
  cairo_fill(cr);
  circle(cr, 23, 13, 1.4);
  setRgba(cr, kOnAir, 1.0);
  cairo_fill(cr);
}

// The Encore's icon (64 x 64): a revolver cylinder in gold on a
// microphone's grip (the live chamber lit over it).
void paintEncoreIcon(cairo_t* cr)
{
  // The grip, a chrome mic handle.
  cairo_move_to(cr, 16, 60);
  cairo_line_to(cr, 26, 36);
  stroke(cr, kInk, 12.0);
  cairo_move_to(cr, 16, 60);
  cairo_line_to(cr, 26, 36);
  stroke(cr, kChrome, 8.0);
  // The barrel.
  roundRect(cr, 36, 22, 26, 10, 3);
  vgrad(cr, 22, 32, {{0.0, rgb(240, 240, 255)}, {1.0, kChromeLo}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  // The cylinder.
  circle(cr, 30, 30, 18);
  vgrad(cr, 12, 48, {{0.0, kGoldHi}, {0.5, kGold}, {1.0, kGoldLo}}, 1.0, true);
  stroke(cr, kInk, 1.8);
  for (int k = 0; k < 6; ++k)
  {
    const double a = k * kPi / 3;
    circle(cr, 30 + std::cos(a) * 10, 30 + std::sin(a) * 10, 4);
    setRgba(cr, rgb(40, 30, 10), 1.0);
    cairo_fill(cr);
  }
  circle(cr, 30, 30, 3);
  setRgba(cr, kGoldLo, 1.0);
  cairo_fill(cr);
  // A star at the muzzle.
  starPath(cr, 60, 14, 6, 2.6);
  setRgba(cr, kGoldHi, 1.0);
  cairo_fill(cr);
}

// A note (40 x 40): the Engine's beat shot.
void paintNote(cairo_t* cr)
{
  ellipse(cr, 14, 30, 9, 7);
  setRgba(cr, kGoldHi, 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.6);
  cairo_move_to(cr, 22, 29);
  cairo_line_to(cr, 22, 4);
  cairo_curve_to(cr, 30, 8, 34, 12, 34, 20);
  stroke(cr, kInk, 4.4);
  cairo_move_to(cr, 22, 29);
  cairo_line_to(cr, 22, 4);
  cairo_curve_to(cr, 30, 8, 34, 12, 34, 20);
  stroke(cr, kGoldHi, 2.2);
}

// Lance at `scale`, feet at the anchor, facing right (mirror for left).
const Sprite& lanceSprite(const Art& art, const Renderer& r, int pose, bool mouth, float scale)
{
  const int w = int(240.0f * scale), h = int(500.0f * scale);
  return bakedSprite(art, r, "lance" + std::to_string(pose) + (mouth ? "o" : "c") + std::to_string(int(scale * 100.0f)), w, h,
    float(w) * 0.5f, 480.0f * scale, [scale, pose, mouth](cairo_t* cr) {
      cairo_scale(cr, scale, scale);
      paintLance(cr, pose, mouth);
    });
}

CellBox layerCells(const Layer& l)
{
  return {l.x0 * kCellsPerTile, l.y0 * kCellsPerTile, (l.x1 - l.x0 + 1) * kCellsPerTile, (l.y1 - l.y0 + 1) * kCellsPerTile};
}

// The rim lights' colour for a wagon's state.
Color rimColor(FnWagonState s)
{
  switch (s)
  {
    case FnWagonState::Running:
      return kGoldHi;
    case FnWagonState::Rerun:
      return rgb(210, 140, 255);
    case FnWagonState::Rolling:
      return rgb(255, 150, 70);
    default:
      return rgb(120, 110, 90);
  }
}

const char* moveName(FnEngineMove m)
{
  static const char* const kNames[] = {"BEAT BARRAGE", "GOLD FIST", "CORE PULSE", "ARC STORM", "MAGNET GRAB", "ATOMIC BREATH"};
  return kNames[std::clamp(int(m), 0, 5)];
}

const char* attackName(FnAttack a)
{
  switch (a)
  {
    case FnAttack::Gold:
      return "RAIN OF GOLD";
    case FnAttack::Trapdoor:
      return "TRAPDOOR";
    case FnAttack::Laugh:
      return "LAUGH TRACK";
    case FnAttack::Light:
      return "LIGHTS!";
    default:
      return "";
  }
}

// The wheel's angle (degrees) for an attack on its pointer: each attack's
// pair of wedges is centred 45 degrees past its quarter.
float wheelAngleFor(FnAttack a)
{
  int k = 0;
  switch (a)
  {
    case FnAttack::Trapdoor:
      k = 1;
      break;
    case FnAttack::Laugh:
      k = 2;
      break;
    case FnAttack::Light:
      k = 3;
      break;
    default:
      break;
  }
  return -(float(k) * 90.0f + 45.0f);
}

} // namespace

// --- Behind the tiles ----------------------------------------------------------------------------

void drawFinaleSet(Renderer& r, const World& w, const Art& art, float camX, float camY, int frame, float alpha)
{
  (void)alpha;
  const auto& f = w.finale();
  if (!f.on || f.reel)
    return;
  // The wagons' flats.
  for (const auto& wg : f.wagons)
  {
    const float fw = float(wg.x1 - wg.x0 + 1) * kTilePx, fh = float(kStage - kFlatTop) * kTilePx;
    float x = float(wg.x0) * kTilePx - camX;
    const float y = float(kFlatTop) * kTilePx - camY;
    if (offScreen(x, y, fw, fh, 0.0f))
      continue;
    const int iw = int(fw * 0.5f), ih = int(fh * 0.5f);
    const FnWagonKind kind = wg.kind;
    const Texture& flat = baked(art, r, "flat" + std::to_string(int(kind)) + "_" + std::to_string(iw), iw, ih, 0.0f, 0.0f,
      [kind, fw, fh](cairo_t* cr) { paintFlat(cr, kind, fw, fh); });
    Color tint = rgb(255, 255, 255);
    float a = 1.0f;
    switch (wg.state)
    {
      case FnWagonState::Idle:
      case FnWagonState::Done:
        tint = rgb(70, 70, 96); // in the wings, the lights down
        break;
      case FnWagonState::Rolling:
      {
        // The turntable turns it in from the right, the lights coming up.
        const float t = std::clamp(float(wg.t) / float(kFnRoll), 0.0f, 1.0f);
        const float ease = 1.0f - (1.0f - t) * (1.0f - t);
        x += (1.0f - ease) * fw * 0.6f;
        a = 0.35f + 0.65f * ease;
        tint = lerpColor(rgb(70, 70, 96), rgb(255, 255, 255), ease);
        break;
      }
      case FnWagonState::Rerun:
        tint = (frame / 6) % 2 ? rgb(210, 160, 255) : rgb(170, 130, 230);
        break;
      default:
        break;
    }
    if (wg.state == FnWagonState::Rolling)
    {
      Clip clip(r, float(wg.x0) * kTilePx - camX, y, fw, fh);
      blit(r, flat, x, y, fw, fh, 0.0f, 0.0f, 0.0f, a, Blend::Alpha, tint);
    }
    else
      blit(r, flat, x, y, fw, fh, 0.0f, 0.0f, 0.0f, a, Blend::Alpha, tint);
  }
  // The stage door's pedestal, a spot on it.
  if (f.pedX >= 0)
  {
    const float cx = (float(f.pedX) + 3.0f) * kCellPx - camX, foot = float(kStage) * kTilePx - camY;
    if (!offScreen(cx - 160.0f, foot - 400.0f, 320.0f, 400.0f))
    {
      drawGlow(r, art, cx, foot - 120.0f, 220.0f, rgb(255, 240, 200), 0.3f);
      r.draw(baked(art, r, "pedestal", 224, 160, 112.0f, 150.0f, paintPedestal), cx, foot);
    }
  }
  // Backstage's wall (blocks 110-119, from row 29).
  {
    const float x = 110.0f * kTilePx - camX, y = 29.0f * kTilePx - camY;
    if (!offScreen(x, y, 640.0f, 704.0f, 0.0f))
      r.draw(baked(art, r, "backstage", 640, 704, 0.0f, 0.0f, paintBackstage), x, y);
  }
  // The arena: the LED wall, the gantry, the desk.
  const auto& b = f.boss;
  if (b.on)
  {
    const float x = float(b.arenaX0) * kCellPx - camX, y = 24.0f * kTilePx - camY;
    const float gantry = float(b.arenaY0 + 6) * kCellPx - 24.0f * kTilePx;
    if (!offScreen(x, y, 2560.0f, 1024.0f, 0.0f))
    {
      const Texture& wall = baked(art, r, "arena", 1280, 512, 0.0f, 0.0f, [gantry](cairo_t* cr) {
        cairo_scale(cr, 0.5, 0.5);
        paintArena(cr, gantry);
      });
      blit(r, wall, x, y, 2560.0f, 1024.0f, 0.0f, 0.0f, 0.0f, 1.0f, Blend::Alpha);
      // The wall's colour wash, slowly moving.
      const float sweep = std::fmod(float(frame) * 3.0f, 2560.0f);
      drawGlow(r, art, x + sweep, y + 400.0f, 420.0f, rgb(90, 140, 255), 0.18f);
      const float dx = (float(b.deskX) - 1.0f) * kCellPx - camX, dy = float(b.arenaY0 + 6) * kCellPx - camY;
      r.draw(baked(art, r, "desk", 224, 120, 112.0f, 120.0f, paintDesk), dx, dy);
    }
  }
}

// --- Over the tiles ----------------------------------------------------------------------------

void World::drawFinaleBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& f = mFinale;
  const auto& b = f.boss;
  if (f.reel)
    return;
  const float stageY = float(kStage) * kTilePx - camY;
  // The turntable's rims: a band of lights along each slot's floor, chasing
  // while the trick runs; the floor mark at the trigger.
  for (const auto& w : f.wagons)
  {
    const float x0 = float(w.x0) * kTilePx - camX, x1 = float(w.x1 + 1) * kTilePx - camX;
    if (x1 < -64.0f || x0 > float(kScreenW) + 64.0f)
      continue;
    const Color c = rimColor(w.state);
    const bool live = w.state == FnWagonState::Running || w.state == FnWagonState::Rolling || w.state == FnWagonState::Rerun;
    r.fillRect(x0, stageY - 4.0f, x1 - x0, 4.0f, withAlpha(c, live ? 200 : 90));
    const int chase = w.state == FnWagonState::Rolling ? frame : frame / 3;
    for (float x = x0 + 16.0f; x < x1; x += 32.0f)
    {
      const int k = int((x - x0) / 32.0f);
      const bool on = live && (k + chase) % 4 == 0;
      r.fillRect(x - 3.0f, stageY - 6.0f, 6.0f, 6.0f, on ? c : withAlpha(c, 110));
      if (on)
        drawGlow(r, mArt, x, stageY - 3.0f, 18.0f, c, 0.7f);
    }
    // The floor mark.
    const float mx = (float(w.trigger) + 0.5f) * kTilePx - camX;
    r.fillRect(mx - 22.0f, stageY - 3.0f, 44.0f, 3.0f, withAlpha(kGold, 220));
    r.fillRect(mx - 2.0f, stageY - 3.0f, 4.0f, 3.0f, kOnAir);
  }
  // Gravity plates: the arrow lights while the station runs and flashes
  // before the flip.
  bool station = false, studio = false;
  for (const auto& w : f.wagons)
  {
    station = station || (w.kind == FnWagonKind::Station && w.state == FnWagonState::Running);
    studio = studio || (w.kind == FnWagonKind::Studio && w.state == FnWagonState::Running);
  }
  for (const auto& pl : f.plates)
  {
    const bool up = pl.dir == Grav::Up;
    const float cx = (float(pl.bx) + 0.5f) * kTilePx - camX;
    const float fy = (up ? float(pl.by + 1) : float(pl.by)) * kTilePx - camY;
    if (offScreen(cx - 60.0f, fy - 60.0f, 120.0f, 120.0f))
      continue;
    const bool flash = pl.flip > 0 && (frame / 2) % 2;
    const bool lit = station && (flash || pl.flip == 0);
    const Sprite& sp = bakedSprite(mArt, r, std::string("plate") + (lit ? "1" : "0"), 96, 40, 48.0f, 36.0f,
      [lit](cairo_t* cr) { paintPlate(cr, lit); });
    DrawOpts o;
    if (!up)
      o.angle = 180.0f; // on the ceiling, its arrow pointing down
    r.draw(sp.facing[0], cx, up ? fy : fy, o);
    if (flash)
      drawGlow(r, mArt, cx, fy + (up ? -20.0f : 20.0f), 50.0f, kLed, 0.8f);
  }
  // The studio monitors.
  for (const auto& po : f.portals)
  {
    const float x = float(po.bx) * kTilePx - camX, y = float(po.by) * kTilePx - camY;
    if (offScreen(x, y, 128.0f, 192.0f))
      continue;
    const std::string label = po.label;
    r.draw(baked(mArt, r, "monitor_" + label, 128, 192, 0.0f, 0.0f, [label](cairo_t* cr) { paintMonitor(cr, label); }), x, y);
    // The screen: the other camera's feed when live, a blue standby card when not.
    const float sx = x + 12.0f, sy = y + 16.0f, sw = 104.0f, sh = 86.0f;
    if (studio)
    {
      const Texture& snow = baked(mArt, r, "snow" + std::to_string((frame / 2) % 3), 320, 180, 0.0f, 0.0f,
        [s = (frame / 2) % 3](cairo_t* cr) { paintSnow(cr, s); });
      blit(r, snow, sx, sy, sw, sh, 0.0f, 0.0f, 0.0f, 0.35f, Blend::Alpha);
      r.fillRect(sx, sy, sw, sh, rgba(60, 160, 255, 90), Blend::Add);
      r.drawText("LIVE", sx + sw * 0.5f, sy + 30.0f, {22.0f, rgb(255, 255, 255), kHudInk}, Align::Center);
      r.drawText("UP: HOP", sx + sw * 0.5f, sy + 56.0f, {14.0f, kGoldHi, kHudInk}, Align::Center);
      const bool on = (frame / 8) % 2 == 0;
      r.fillRect(x + 100.0f, y + 110.0f, 12.0f, 12.0f, on ? kOnAir : rgb(90, 30, 30));
      if (on)
        drawGlow(r, mArt, x + 106.0f, y + 116.0f, 20.0f, kOnAir, 0.8f);
    }
    else
    {
      r.fillRect(sx, sy, sw, sh, rgb(20, 40, 110));
      r.drawText("STANDBY", sx + sw * 0.5f, sy + 36.0f, {14.0f, rgb(200, 220, 255), 0}, Align::Center);
    }
  }
  // The backstage refill.
  if (f.refillX >= 0)
  {
    const float cx = (float(f.refillX) + 2.0f) * kCellPx - camX, foot = float(f.refillY + 1) * kCellPx - camY;
    if (!offScreen(cx - 96.0f, foot - 192.0f, 192.0f, 192.0f))
    {
      const bool lit = !f.refillUsed;
      r.draw(baked(mArt, r, std::string("refill") + (lit ? "1" : "0"), 192, 192, 96.0f, 192.0f,
               [lit](cairo_t* cr) { paintRefill(cr, lit); }),
        cx, foot);
      if (lit)
        r.drawText("REFILL", cx, foot - 228.0f, {18.0f, kGoldHi, kHudInk}, Align::Center);
    }
  }
  // The ON AIR door.
  if (f.door >= 0)
  {
    const CellBox d = layerCells(mLayers[std::size_t(f.door)]);
    const float x = float(d.x) * kCellPx - camX, y = float(d.y) * kCellPx - camY;
    if (!offScreen(x - 32.0f, y - 96.0f, 128.0f, 352.0f))
    {
      const Layer& l = mLayers[std::size_t(f.door)];
      if (l.solid)
        r.draw(baked(mArt, r, "stagedoor", 128, 352, 32.0f, 96.0f, paintStageDoor), x, y);
      else
      {
        // Open: the frame and the box only, the dark stage beyond.
        r.fillRect(x - 8.0f, y, 80.0f, 256.0f, rgba(0, 0, 0, 120));
        Clip clip(r, x - 40.0f, y - 100.0f, 144.0f, 100.0f);
        r.draw(baked(mArt, r, "stagedoor", 128, 352, 32.0f, 96.0f, paintStageDoor), x, y);
      }
      const bool lit = b.phase != FnPhase::Switched ? b.phase != FnPhase::Waiting || (frame / 30) % 2 == 0
                                                    : b.signDead < 30 && (frame / 2) % 2;
      r.draw(baked(mArt, r, std::string("onair") + (lit ? "1" : "0"), 96, 40, 48.0f, 20.0f,
               [lit](cairo_t* cr) { paintOnAir(cr, lit); }),
        x + 32.0f, y - 58.0f);
      if (lit)
        drawGlow(r, mArt, x + 32.0f, y - 58.0f, 90.0f, kOnAir, 0.6f);
    }
  }
  // The trapdoors.
  for (int k = 0; k < 3; ++k)
  {
    const int li = f.traps[std::size_t(k)];
    if (li < 0)
      continue;
    const Layer& l = mLayers[std::size_t(li)];
    const CellBox t = layerCells(l);
    const float x = float(t.x) * kCellPx - camX, y = float(t.y) * kCellPx - camY, w = float(t.w) * kCellPx;
    if (offScreen(x, y, w, 64.0f))
      continue;
    if (l.solid)
      r.draw(baked(mArt, r, "trapdoor", 192, 64, 0.0f, 0.0f, paintTrapdoor), x, y);
    else
    {
      // Open: a black drop with the static flicker.
      r.fillRect(x, y, w, 64.0f, rgb(0, 0, 0));
      const Texture& snow = baked(mArt, r, "snow" + std::to_string((frame / 2) % 3), 320, 180, 0.0f, 0.0f,
        [s = (frame / 2) % 3](cairo_t* cr) { paintSnow(cr, s); });
      blit(r, snow, x + 4.0f, y + 6.0f, w - 8.0f, 54.0f, 0.0f, 0.0f, 0.0f, 0.45f, Blend::Alpha);
      r.fillRect(x - 6.0f, y - 4.0f, 6.0f, 68.0f, rgb(20, 20, 28));
      r.fillRect(x + w, y - 4.0f, 6.0f, 68.0f, rgb(20, 20, 28));
    }
    const bool warn = b.trap == k && b.openTrap != k &&
      ((b.phase == FnPhase::Lance && b.attack == FnAttack::Trapdoor) || b.phase == FnPhase::OffAir);
    if (warn && (frame / 2) % 2)
    {
      r.fillRect(x, y - 6.0f, w, 6.0f, rgb(255, 230, 80));
      r.fillRect(x, y + 58.0f, w, 6.0f, rgb(255, 230, 80));
      r.fillRect(x, y - 6.0f, 6.0f, 70.0f, rgb(255, 230, 80));
      r.fillRect(x + w - 6.0f, y - 6.0f, 6.0f, 70.0f, rgb(255, 230, 80));
      drawGlow(r, mArt, x + w * 0.5f, y, 90.0f, rgb(255, 220, 80), 0.5f);
      r.drawText("!", x + w * 0.5f, y - 50.0f, {34.0f, rgb(255, 230, 80), kHudInk}, Align::Center);
    }
  }
  // The race decks: chrome truss, ghosted until Off the Air rolls them in.
  for (int li : {f.deckStep, f.deckMid, f.deckHigh})
  {
    if (li < 0)
      continue;
    const Layer& l = mLayers[std::size_t(li)];
    const float x0 = float(l.x0) * kTilePx - camX, y = float(l.y0) * kTilePx - camY;
    const float w = float(l.x1 - l.x0 + 1) * kTilePx;
    if (offScreen(x0, y, w, 40.0f))
      continue;
    DrawOpts o;
    o.alpha = l.solid ? 1.0f : 0.16f;
    const Texture& deck = baked(mArt, r, "deck", 64, 40, 0.0f, 0.0f, paintDeck);
    for (float x = x0; x < x0 + w - 1.0f; x += kTilePx)
      r.draw(deck, x, y, o);
    if (l.solid)
      for (float x = x0 + 32.0f; x < x0 + w; x += 128.0f)
        drawGlow(r, mArt, x, y + 11.0f, 30.0f, kLed, 0.4f);
  }
  // The master switch's glass case.
  if (f.cover >= 0 && b.coverHp > 0)
  {
    const CellBox c = layerCells(mLayers[std::size_t(f.cover)]);
    const float x = float(c.x) * kCellPx - camX, y = float(c.y) * kCellPx - camY;
    const int cracks = std::clamp((kFnCoverHp - b.coverHp + 1) / 2, 0, 5);
    r.draw(baked(mArt, r, "glass" + std::to_string(cracks), 96, 112, 16.0f, 16.0f, [cracks](cairo_t* cr) { paintGlass(cr, cracks); }),
      x, y);
    if (b.hitFlash > 0)
      drawGlow(r, mArt, x + 32.0f, y + 32.0f, 60.0f, rgb(220, 245, 255), 0.6f);
  }
  // The finale's enemies.
  for (const auto& a : f.actors)
  {
    const Enemy& e = mEnemies[std::size_t(a.enemy)];
    if (!e.alive || e.hidden)
      continue;
    const float cx = (e.drawX + float(e.w) * 0.5f) * kCellPx - camX, foot = (e.drawY + 1.0f) * kCellPx - camY;
    if (offScreen(cx - 160.0f, foot - 320.0f, 320.0f, 340.0f))
      continue;
    const int dir = e.dir < 0 ? -1 : 1;
    DrawOpts fo;
    fo.blend = Blend::Add;
    fo.alpha = float(e.flash) / 8.0f;
    if (a.kind == FnActorKind::AdBreak)
    {
      // Dropping in from the flies on a cable, then parked; its ad loops.
      const float drop = a.roll < kFnAdRoll ? float(kFnAdRoll - a.roll) * kCellPx * 1.5f : 0.0f;
      const float y = foot - drop;
      const Sprite& sp = styledEnemySprite(mArt, r, mTheme, "adbreak", e.tell > 0 ? 1 : 0, (frame / 20) % 4, e.w, e.h);
      if (drop > 0.0f)
        r.drawLine(cx, y - float(e.h) * kCellPx - 400.0f, cx, y - float(e.h) * kCellPx, 3.0f, rgb(60, 60, 76));
      r.draw(sp.get(1), cx, y);
      if (e.flash > 0)
        r.draw(sp.get(1), cx, y, fo);
      drawGlow(r, mArt, cx, y - float(e.h) * kCellPx * 0.6f, 160.0f, rgb(120, 170, 255), 0.18f);
      if (e.tell > 0)
        r.drawText("~ HALCYON ARMS! ~", cx, y - float(e.h) * kCellPx - 40.0f,
          {22.0f, (frame / 3) % 2 ? kGoldHi : rgb(255, 120, 150), kHudInk, true}, Align::Center);
    }
    else if (a.kind == FnActorKind::Double)
    {
      const Sprite& sp = styledEnemySprite(mArt, r, mTheme, "double", a.runner | (e.tell > 0 ? 4 : 0),
        a.jumpT >= 0 ? 1 : (e.x / 2) % 2, e.w, e.h);
      r.draw(sp.get(dir), cx, foot);
      if (e.flash > 0)
        r.draw(sp.get(dir), cx, foot, fo);
      if (e.tell > 0)
        drawGlow(r, mArt, cx + float(dir) * 70.0f, foot - 110.0f, 40.0f, rgb(255, 230, 140), 0.8f);
    }
    else if (a.kind == FnActorKind::Censor)
    {
      const float bob = std::sin(float(frame) * 0.12f + float(a.enemy)) * 5.0f;
      const Sprite& sp = styledEnemySprite(mArt, r, mTheme, "censorbar", 0, (frame / 2) % 2, e.w, e.h);
      r.draw(sp.get(dir), cx, foot + bob);
      if (e.flash > 0)
        r.draw(sp.get(dir), cx, foot + bob, fo);
    }
  }
  for (const auto& e : mEnemies)
  {
    if (!e.alive || e.hidden || e.kind != EnemyKind::SampleBot)
      continue;
    const float cx = (e.drawX + float(e.w) * 0.5f) * kCellPx - camX, foot = (e.drawY + 1.0f) * kCellPx - camY;
    if (offScreen(cx - 60.0f, foot - 120.0f, 120.0f, 130.0f))
      continue;
    const Sprite& sp = styledEnemySprite(mArt, r, mTheme, "sample_bot", 0, (e.x / 2) % 2, e.w, e.h);
    r.draw(sp.get(e.dir < 0 ? -1 : 1), cx, foot);
    if (e.flash > 0)
    {
      DrawOpts fo;
      fo.blend = Blend::Add;
      fo.alpha = float(e.flash) / 8.0f;
      r.draw(sp.get(e.dir < 0 ? -1 : 1), cx, foot, fo);
    }
  }
  if (!b.on || !isOnScreen({b.arenaX0, b.arenaY0 - 8, b.arenaX1 - b.arenaX0 + 1, b.floorY - b.arenaY0 + 9}, 4))
    return;
  // The Encore Engine stands at the back.
  if (b.phase == FnPhase::Engine || b.phase == FnPhase::Collapse)
  {
    const CellBox eb = finaleEngine(b);
    float x = float(eb.x) * kCellPx - camX, y = float(eb.y) * kCellPx - camY;
    const int tell = b.moveT;
    const bool jaw = b.move == FnEngineMove::Breath && tell >= 0;
    const bool fist = b.move == FnEngineMove::Fist && tell >= 0 && tell <= 15;
    const Texture& body = baked(mArt, r, std::string("engine") + (jaw ? "j" : "") + (fist ? "f" : ""), 576, 576, 32.0f, 32.0f,
      [jaw, fist](cairo_t* cr) { paintEngine(cr, jaw, fist); });
    float angle = 0.0f, sink = 0.0f;
    if (b.phase == FnPhase::Collapse)
    {
      const float t = std::clamp(float(b.t) / float(kFnCollapse), 0.0f, 1.0f);
      sink = t * t * 300.0f;
      angle = -t * 12.0f;
      x += std::sin(float(frame) * 1.3f) * 6.0f * (1.0f - t);
    }
    {
      Clip clip(r, x - 200.0f, 48.0f, 900.0f, stageY - 48.0f); // the stage floor, the HUD's strip
      DrawOpts o;
      o.angle = angle;
      r.draw(body, x, y + sink, o);
      if (b.hitFlash > 0)
      {
        DrawOpts fo;
        fo.blend = Blend::Add;
        fo.alpha = float(b.hitFlash) / 6.0f * 0.6f;
        fo.angle = angle;
        r.draw(body, x, y + sink, fo);
      }
    }
    if (b.phase == FnPhase::Engine)
    {
      // The tells: the speakers pulse, the chest glows, the rods crackle,
      // the magnet hums.
      const bool telling = tell >= 0;
      if (telling && b.move == FnEngineMove::Barrage)
        for (const auto& sp : {std::array<float, 2>{200, 190}, std::array<float, 2>{340, 190}})
          drawGlow(r, mArt, x + sp[0], y + sp[1], 70.0f + float(tell % 8) * 6.0f, kGoldHi, 0.5f);
      if (telling && b.move == FnEngineMove::Pulse)
        drawGlow(r, mArt, x + 270.0f, y + 240.0f, 200.0f, kLed, 0.25f + 0.5f * std::min(1.0f, float(tell) / 22.0f));
      if (telling && b.move == FnEngineMove::Arc)
        for (const float rx : {170.0f, 380.0f})
          drawGlow(r, mArt, x + rx, y, 40.0f + float(frame % 3) * 10.0f, rgb(200, 220, 255), 0.8f);
      if (telling && b.move == FnEngineMove::Magnet)
        drawGlow(r, mArt, x + 470.0f, y + 350.0f, 60.0f + float(frame % 4) * 6.0f, rgb(255, 80, 80), 0.6f);
      if (telling && b.move == FnEngineMove::Fist && tell <= 15)
        drawGlow(r, mArt, x + 90.0f, y - 10.0f, 80.0f, kGoldHi, 0.5f);
      // The core.
      const CellBox cb = finaleCore(b);
      const bool open = b.core > 0 || b.stagger > 0;
      const float ccx = (float(cb.x) + 2.0f) * kCellPx - camX, ccy = (float(cb.y) + 2.0f) * kCellPx - camY;
      r.draw(baked(mArt, r, std::string("core") + (open ? "1" : "0"), 128, 128, 64.0f, 64.0f,
               [open](cairo_t* cr) { paintCore(cr, open); }),
        ccx, ccy);
      if (open)
        drawGlow(r, mArt, ccx, ccy, 110.0f + float(frame % 4) * 8.0f, rgb(255, 210, 90), b.stagger > 0 ? 0.9f : 0.6f);
      if (b.stagger > 0)
        r.drawText("STAGGERED  x2", ccx + 120.0f, ccy - 90.0f, {24.0f, (frame / 3) % 2 ? kGoldHi : rgb(255, 140, 60), kHudInk, true},
          Align::Center);
    }
  }
  // Off the Air: Lance running along the gantry for the desk.
  if (b.phase == FnPhase::OffAir)
  {
    const float lx = (float(b.runX) + 1.5f) * kCellPx - camX, foot = float(b.arenaY0 + 6) * kCellPx - camY;
    const int pose = (frame / 4) % 8;
    r.draw(lanceSprite(mArt, r, pose, (frame / 10) % 2 == 0, 0.34f).get(1), lx, foot);
    drawGlow(r, mArt, lx, foot - 80.0f, 90.0f, rgb(255, 240, 200), 0.25f);
  }
  (void)alpha;
}

// --- In front ------------------------------------------------------------------------------------

void World::drawFinaleFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& f = mFinale;
  const auto& b = f.boss;
  // Encore shots glow in their prototype's colour.
  for (const auto& pr : mProjectiles)
    if (pr.alive && pr.encore >= 0)
    {
      const float x = (lerpCells(pr.prevX, pr.x, alpha) / kCellPx + float(pr.w) * 0.5f) * kCellPx - camX;
      const float y = (lerpCells(pr.prevY, pr.y, alpha) / kCellPx + 0.5f) * kCellPx - camY;
      drawGlow(r, mArt, x, y, 26.0f, protoDef(pr.encore).color, 0.7f);
    }
  for (const auto& a : f.actors)
  {
    const Enemy& e = mEnemies[std::size_t(a.enemy)];
    // A Censor Bar's black bar, over everything below it until it is shot down.
    if (a.kind == FnActorKind::Censor && a.bar >= 0 && e.alive)
    {
      const float t = float(std::min(a.bar, kFnBarUnroll)) / float(kFnBarUnroll);
      const float fullW = float(kFnBarW) * kCellPx, h = float(kFnBarH) * kCellPx;
      const float w = std::max(kCellPx, fullW * t);
      const float x = (float(a.barX) + float(kFnBarW) * 0.5f) * kCellPx - camX - w * 0.5f, y = float(a.barY) * kCellPx - camY;
      r.fillRect(x, y, w, h, rgb(0, 0, 0));
      // A pixelated fringe along its edges.
      for (float px = x; px < x + w; px += 16.0f)
      {
        const unsigned hsh = hash2(int(px - x) / 16, a.enemy * 7 + frame / 4);
        r.fillRect(px, y - 8.0f, 16.0f, 8.0f, rgba(0, 0, 0, 120 + int(hsh % 100u)));
        r.fillRect(px, y + h, 16.0f, 8.0f, rgba(0, 0, 0, 120 + int((hsh >> 8) % 100u)));
      }
      if (t >= 1.0f)
        r.drawText("CENSORED", x + w * 0.5f, y + h * 0.5f - 14.0f, {26.0f, rgb(52, 52, 60), 0}, Align::Center);
      else if ((frame / 2) % 2)
        r.drawText("BLEEP!", x + w * 0.5f, y + h * 0.5f - 14.0f, {26.0f, rgb(255, 80, 90), 0}, Align::Center);
    }
  }
  // The toon's air-run: dust off the runner's heels; looking down, "uh-oh".
  if (!f.reel && (f.air != 0 || f.uhOh > 0))
  {
    const auto& p = mPlayer;
    const float px = lerpCells(p.prevX, p.x, alpha) + 1.5f * kCellPx - camX, foot = lerpCells(p.prevY + 1, p.y + 1, alpha) - camY;
    if (f.air != 0)
      for (int k = 0; k < 4; ++k)
      {
        const float ph = float((frame + k * 3) % 12) / 12.0f;
        drawGlow(r, mArt, px - float(f.air) * (20.0f + ph * 60.0f), foot - 6.0f - ph * 10.0f, 10.0f + ph * 12.0f, rgb(255, 255, 255),
          0.6f * (1.0f - ph));
      }
    if (f.uhOh > 0)
    {
      const float y = foot - float(p.height()) * kCellPx - 60.0f;
      r.fillRect(px - 54.0f, y - 22.0f, 108.0f, 40.0f, rgb(250, 250, 240));
      r.drawLine(px, y + 18.0f, px, y + 50.0f, 4.0f, rgb(140, 90, 50));
      r.drawText("UH-OH", px, y - 16.0f, {24.0f, rgb(20, 20, 40), 0}, Align::Center);
    }
  }
  if (!b.on || b.phase == FnPhase::Waiting || b.phase == FnPhase::Switched)
    return;
  const float tall = float(b.floorY - b.arenaY0 + 1) * kCellPx;
  const float floorPx = float(b.floorY + 1) * kCellPx - camY;
  if (b.phase == FnPhase::Lance || b.phase == FnPhase::Intro)
  {
    // Lance behind his hover podium; the prize wheel at his side.
    const float px = lerpCells(b.prevX, b.x, alpha) - camX, py = lerpCells(b.prevY, b.y, alpha) - camY;
    const float bob = std::sin(float(frame) * 0.1f) * 4.0f;
    const bool cue = b.cue > 0;
    const bool open = cue && b.y >= b.pathY + kFnSinkRows * kCellsPerTile - 1;
    const int dir = mPlayer.x + 1 < b.x + kFnPodiumW / 2 ? -1 : 1;
    // Lance, from the waist up over the lectern (kept out of the HUD's strip).
    Clip clip(r, 0.0f, 48.0f, float(kScreenW), float(kScreenH) - 48.0f);
    const int pose = b.wheel >= 0 ? 8 : 9;
    r.draw(lanceSprite(mArt, r, pose, (frame / 6) % 2 == 0 && !cue, 0.36f).get(dir), px + 128.0f, py + bob + 64.0f);
    if (cue)
      r.draw(baked(mArt, r, "cuecards", 90, 70, 45.0f, 60.0f, paintCueCards), px + 128.0f + float(dir) * 30.0f, py + bob - 10.0f);
    // The jets' flames under it.
    for (const float jx : {80.0f, 208.0f})
    {
      const float flick = 0.7f + 0.3f * float(hash2(int(jx), frame / 2) % 100u) / 100.0f;
      drawGlow(r, mArt, px + jx, py + bob + 150.0f, 34.0f * flick, rgb(120, 200, 255), 0.9f);
      r.fillRect(px + jx - 8.0f, py + bob + 140.0f, 16.0f, 20.0f * flick, rgba(200, 240, 255, 200), Blend::Add);
    }
    const Texture& pod = baked(mArt, r, std::string("podium") + (open ? "1" : "0"), 288, 192, 16.0f, 16.0f,
      [open](cairo_t* cr) { paintPodium(cr, open); });
    r.draw(pod, px, py + bob);
    if (b.hitFlash > 0)
    {
      DrawOpts fo;
      fo.blend = Blend::Add;
      fo.alpha = float(b.hitFlash) / 6.0f;
      r.draw(pod, px, py + bob, fo);
    }
    if (open && (frame / 4) % 2)
      r.drawText("HIT THE PANEL!", px + 128.0f, py + bob + 140.0f, {18.0f, kGoldHi, kHudInk}, Align::Center);
    // The prize wheel, hung under the podium between its jets while it hovers.
    const float wx = px + 128.0f, wy = py + bob + 196.0f;
    float ang = wheelAngleFor(b.pick);
    if (b.wheel >= 0 && b.wheel < kFnWheelFrames - kFnWheelHold)
    {
      const float left = float(kFnWheelFrames - kFnWheelHold - b.wheel) / float(kFnWheelFrames - kFnWheelHold);
      ang += left * left * 720.0f;
    }
    if (b.y <= b.pathY + 1)
    {
      r.drawLine(wx, py + bob + 120.0f, wx, wy, 5.0f, kChromeLo);
      DrawOpts wo;
      wo.scale = 0.5f;
      wo.angle = ang;
      r.draw(baked(mArt, r, "wheel", 176, 176, 88.0f, 88.0f, paintWheel), wx, wy, wo);
      DrawOpts po;
      po.scale = 0.6f;
      r.draw(baked(mArt, r, "pointer", 32, 40, 16.0f, 36.0f, paintPointer), wx, wy - 36.0f, po);
      if (b.wheel >= kFnWheelFrames - kFnWheelHold)
        r.drawText(attackName(b.pick), wx, wy + 52.0f, {22.0f, kGoldHi, kHudInk, true}, Align::Center);
    }
    if (b.attack == FnAttack::Laugh && b.waveR < 0)
      r.drawText("LAUGH", px + 128.0f, py + bob - 130.0f,
        {32.0f, (frame / 2) % 2 ? rgb(255, 60, 60) : rgb(255, 230, 80), kHudInk}, Align::Center);
  }
  const float arenaTop = float(b.arenaY0) * kCellPx - camY;
  // Rain of gold: shadows, then the bars.
  if (b.phase == FnPhase::Lance && b.attack == FnAttack::Gold)
    for (int g : b.gold)
    {
      const float gx = (float(g) + 3.0f) * kCellPx - camX;
      if (b.attackT < kFnGoldTell)
      {
        const float t = float(b.attackT) / float(kFnGoldTell);
        r.fillRect(gx - 40.0f - t * 56.0f, floorPx - 6.0f, 80.0f + t * 112.0f, 6.0f, rgba(0, 0, 0, 150));
        drawGlow(r, mArt, gx, floorPx - 4.0f, 40.0f + t * 60.0f, rgb(255, 210, 80), 0.25f + 0.3f * t);
      }
      else
      {
        const float fall = std::min(1.0f, float(b.attackT - kFnGoldTell) / 4.0f);
        const float gy = arenaTop - 128.0f + (tall - 0.0f) * fall;
        r.draw(baked(mArt, r, "gold", 192, 128, 96.0f, 128.0f, paintGold), gx, std::min(gy, floorPx));
        if (fall >= 1.0f)
          drawGlow(r, mArt, gx, floorPx - 30.0f, 120.0f, kGoldHi, 0.5f);
      }
    }
  // The laugh-track shockwave: HA HA HA running along the floor.
  if (b.waveR >= 0)
    for (int wx : {b.waveL, b.waveR})
    {
      const float x = (float(wx) + 1.0f) * kCellPx - camX;
      drawGlow(r, mArt, x, floorPx - 32.0f, 60.0f, rgb(255, 220, 120), 0.6f);
      r.drawText("HA!", x, floorPx - 70.0f + float((frame + wx) % 4) * 3.0f, {28.0f, kGoldHi, kHudInk, true}, Align::Center);
      r.fillRect(x - 32.0f, floorPx - 64.0f, 64.0f, 64.0f, rgba(255, 230, 140, 60), Blend::Add);
    }
  // A studio lamp: its shadow grows, then it falls.
  if (b.lampX >= 0)
  {
    const float lx = float(b.lampX) * kCellPx - camX;
    if (b.attackT < kFnLampTell)
    {
      const float t = float(b.attackT) / float(kFnLampTell);
      r.fillRect(lx - 20.0f - t * 76.0f, floorPx - 6.0f, 40.0f + t * 152.0f, 6.0f, rgba(0, 0, 0, 170));
      r.drawLine(lx, arenaTop - 200.0f, lx, arenaTop + 40.0f, 3.0f, rgb(60, 60, 76));
      DrawOpts lo;
      lo.angle = std::sin(float(frame) * 0.8f) * 4.0f * t;
      r.draw(baked(mArt, r, "lamp", 192, 160, 96.0f, 160.0f, paintLamp), lx, arenaTop + 200.0f, lo);
    }
    else
    {
      const float fall = std::min(1.0f, float(b.attackT - kFnLampTell + 1) / 3.0f);
      r.draw(baked(mArt, r, "lamp", 192, 160, 96.0f, 160.0f, paintLamp), lx, arenaTop + 200.0f + (floorPx - arenaTop - 200.0f) * fall);
      if (fall >= 1.0f)
        drawGlow(r, mArt, lx, floorPx - 20.0f, 160.0f, rgb(255, 250, 220), 0.5f);
    }
  }
  if (b.phase == FnPhase::Engine)
  {
    // The board over the Engine: its next move, the counter blinking.
    const CellBox eb = finaleEngine(b);
    const float ix = std::clamp((float(eb.x) + 8.0f) * kCellPx - camX, 170.0f, float(kScreenW) - 170.0f);
    const float iy = std::max(float(eb.y) * kCellPx - camY - 120.0f, 150.0f); // under the messages
    if (b.moveT >= 0)
    {
      const int c = finaleCounter(b.move);
      const bool logged = std::find(f.wheel.begin(), f.wheel.end(), c) != f.wheel.end();
      r.draw(baked(mArt, r, "moveplate", 300, 76, 150.0f, 0.0f, [](cairo_t* cr) { paintPlateHud(cr, 300, 76); }), ix, iy);
      r.drawText(moveName(b.move), ix, iy + 10.0f, {22.0f, rgb(255, 120, 90), kHudInk}, Align::Center);
      if ((frame / 4) % 2)
        r.drawText(std::string("COUNTER: ") + (logged ? protoDef(c).name : "BLASTER"), ix, iy + 42.0f,
          {16.0f, logged ? protoDef(c).color : rgb(220, 220, 230), kHudInk}, Align::Center);
    }
    const float ey = float(b.ey + 1) * kCellPx - camY;
    for (int cx : {b.coinL, b.coinR})
      if (cx >= 0)
      {
        const float x = (float(cx) + 1.0f) * kCellPx - camX;
        r.draw(baked(mArt, r, "coin" + std::to_string((frame / 2) % 4), 64, 64, 32.0f, 64.0f,
                 [k = (frame / 2) % 4](cairo_t* cr) { paintCoin(cr, k); }),
          x, ey);
      }
    if (b.ring > 0)
    {
      // The Core Pulse: a ring of light rolling out at chest height.
      const float rx = float(b.ex - b.ring) * kCellPx - camX, ry = float(b.ey - 8) * kCellPx - camY;
      r.fillRect(rx, ry + 40.0f, float(b.ring) * kCellPx, 80.0f, rgba(90, 200, 255, 40), Blend::Add);
      drawGlow(r, mArt, rx, ry + 80.0f, 90.0f, kLed, 0.9f);
      r.drawLine(rx, ry, rx, ry + 160.0f, 10.0f, rgba(200, 240, 255, 220), Blend::Add);
    }
    if (b.move == FnEngineMove::Arc && b.moveT >= 0)
      for (int ac : b.arcs)
      {
        const float ax = (float(ac) + 2.0f) * kCellPx - camX;
        const bool strike = b.moveT > 22;
        if (!strike)
        {
          r.fillRect(ax - 64.0f, floorPx - 6.0f, 128.0f, 6.0f, rgba(120, 160, 255, 120 + 80 * ((frame / 2) % 2)));
          drawGlow(r, mArt, ax, floorPx, 60.0f, rgb(120, 160, 255), 0.4f);
          continue;
        }
        // A jagged bolt down from the rig.
        float x0 = ax, y0 = arenaTop;
        for (int k = 0; k < 10; ++k)
        {
          const float y1 = arenaTop + (floorPx - arenaTop) * float(k + 1) / 10.0f;
          const float x1 = ax + float(int(hash2(k + ac, frame) % 41u) - 20);
          r.drawLine(x0, y0, x1, y1, 10.0f, rgba(140, 170, 255, 160), Blend::Add);
          r.drawLine(x0, y0, x1, y1, 4.0f, rgb(240, 245, 255));
          x0 = x1;
          y0 = y1;
        }
        drawGlow(r, mArt, ax, floorPx - 20.0f, 120.0f, rgb(200, 220, 255), 0.8f);
      }
    if (b.breath > 0)
    {
      // Atomic Breath: a cone of green-white fire from the jaw.
      const float jx = float(b.ex) * kCellPx - camX + 40.0f, jy = float(eb.y) * kCellPx - camY + 200.0f;
      const float reach = float(kFnBreathReach) * kCellPx;
      for (int k = 0; k < 14; ++k)
      {
        const float t = float((k * 7 + frame * 3) % 40) / 40.0f;
        drawGlow(r, mArt, jx - reach * t, jy + 40.0f * t + std::sin(float(k + frame)) * 20.0f * t, 30.0f + 90.0f * t,
          lerpColor(rgb(220, 255, 240), rgb(40, 255, 180), t), 0.75f * (1.0f - t * 0.6f));
      }
    }
    if (b.move == FnEngineMove::Magnet && b.moveT > 15)
    {
      // The pull: red arcs of force between the magnet and the runner.
      const float mx = float(b.ex) * kCellPx - camX + 470.0f - 0.0f, my = float(eb.y) * kCellPx - camY + 350.0f;
      const float tx = lerpCells(mPlayer.prevX, mPlayer.x, alpha) + 1.5f * kCellPx - camX;
      const float ty = lerpCells(mPlayer.prevY, mPlayer.y, alpha) - 1.5f * kCellPx - camY;
      for (int k = 0; k < 5; ++k)
      {
        const float t = std::fmod(float(k) / 5.0f + float(frame) * 0.05f, 1.0f);
        const float x = tx + (mx - tx) * t, y = ty + (my - ty) * t;
        r.drawLine(x, y - 26.0f, x, y + 26.0f, 5.0f, rgba(255, 70, 70, int(200 * (1.0f - t))), Blend::Add);
      }
    }
  }
}

// --- On top --------------------------------------------------------------------------------------

void World::drawFinaleTop(Renderer& r, float /*camX*/, float /*camY*/, int frame) const
{
  const auto& f = mFinale;
  const float W = float(kScreenW), H = float(kScreenH);
  const FnWagon* rolling = nullptr;
  for (const auto& w : f.wagons)
    if (w.state == FnWagonState::Rolling)
      rolling = &w;
  if (rolling)
  {
    // PREVIOUSLY ON: a lower third sliding in, the turntable's arc sweeping.
    const float t = std::clamp(float(rolling->t) / float(kFnRoll), 0.0f, 1.0f);
    const float in = std::min(1.0f, t * 4.0f), out = t > 0.85f ? (t - 0.85f) / 0.15f : 0.0f;
    const float slide = (1.0f - in) * -700.0f - out * 700.0f;
    for (int k = 0; k < 10; ++k)
    {
      const float x = std::fmod(float(frame * 30 + k * 160), W + 200.0f) - 100.0f;
      r.fillRect(x, H * 0.66f, 60.0f, 8.0f, rgba(255, 215, 90, 120), Blend::Add);
    }
    const float y = H * 0.24f;
    r.draw(baked(mArt, r, "lowerthird", 720, 120, 0.0f, 0.0f, [](cairo_t* cr) { paintPlateHud(cr, 720, 120); }), W * 0.5f - 360.0f + slide,
      y);
    r.fillRect(W * 0.5f - 356.0f + slide, y + 4.0f, 12.0f, 112.0f, kOnAir);
    r.drawText("PREVIOUSLY ON GUNRUNNERS", W * 0.5f + slide, y + 14.0f, {22.0f, kLed, kHudInk}, Align::Center);
    r.drawText(finaleWagonName(rolling->kind), W * 0.5f + slide, y + 46.0f, {46.0f, kGoldHi, kHudInk}, Align::Center);
  }
  if (f.wipe > 0)
  {
    // A monitor's static wipe.
    const Texture& snow = baked(mArt, r, "snow" + std::to_string(frame % 3), 320, 180, 0.0f, 0.0f,
      [s = frame % 3](cairo_t* cr) { paintSnow(cr, s); });
    blit(r, snow, 0.0f, 0.0f, W, H, 0.0f, 0.0f, 0.0f, 0.85f, Blend::Alpha);
    r.fillRect(0.0f, float((frame * 97) % 720), W, 30.0f, rgba(255, 255, 255, 80));
  }
  if (f.boss.commercial > 0)
  {
    r.draw(baked(mArt, r, "commercial", 1280, 720, 0.0f, 0.0f, paintCommercial), 0.0f, 0.0f);
    if (f.boss.commercial > kFnCommercial - 4)
      r.fillRect(0.0f, 0.0f, W, H, rgba(255, 255, 255, 140));
  }
  if (f.reel && f.card > 0)
  {
    // BLOOPER!: the clapper snaps, the tape warps.
    for (int k = 0; k < 10; ++k)
    {
      const float y = float((k * 73 + frame * 37) % 720);
      r.fillRect(0.0f, y, W, 4.0f + float(k % 3) * 2.0f, rgba(255, 255, 255, 70));
      r.fillRect(float((k * 151 + frame * 11) % 1280), y, 160.0f, 4.0f, rgba(255, 80, 120, 90));
    }
    const float shake = float((frame % 3) - 1) * 6.0f;
    r.draw(baked(mArt, r, "clapper", 520, 300, 260.0f, 150.0f, paintClapper), W * 0.5f + shake, H * 0.42f);
    r.drawText("BLOOPER!", W * 0.5f + shake, H * 0.42f - 16.0f, {54.0f, rgb(255, 70, 100), kHudInk}, Align::Center);
    r.drawText(finaleReelName(f.rule), W * 0.5f, H * 0.42f + 104.0f, {30.0f, rgb(255, 255, 255), kHudInk}, Align::Center);
  }
}

// --- The HUD -------------------------------------------------------------------------------------

void World::drawFinaleHud(Renderer& r, int frame) const
{
  const auto& f = mFinale;
  const auto& b = f.boss;
  const auto& p = mPlayer;
  char buf[96];
  if (f.reel)
  {
    std::snprintf(buf, sizeof(buf), "THE BLOOPER REEL: %s - NEXT IN %d", finaleReelName(f.rule),
      std::max(0, (kFnReelEvery - f.reelT + 14) / 15));
    // Under the bonus clock and the messages.
    r.draw(baked(mArt, r, "reelplate", 560, 40, 280.0f, 0.0f, [](cairo_t* cr) { paintPlateHud(cr, 560, 40); }), float(kScreenW) * 0.5f,
      144.0f);
    r.drawText(buf, float(kScreenW) * 0.5f, 150.0f, {20.0f, rgb(255, 140, 170), kHudInk}, Align::Center);
    return;
  }
  // The ratings ticker: each wagon's trick counting down, RERUN.
  float y = 96.0f;
  const Texture& plate = baked(mArt, r, "tickerplate", 320, 40, 0.0f, 0.0f, [](cairo_t* cr) { paintPlateHud(cr, 320, 40); });
  const Texture& tv = baked(mArt, r, "tv", 28, 24, 0.0f, 0.0f, paintTv);
  for (const auto& w : f.wagons)
  {
    const bool run = w.state == FnWagonState::Running;
    if (!run && w.state != FnWagonState::Rerun)
      continue;
    r.draw(plate, 12.0f, y);
    r.draw(tv, 22.0f, y + 8.0f);
    if (run)
      std::snprintf(buf, sizeof(buf), "%s  %d", finaleWagonName(w.kind), std::max(0, (kFnTrick - w.t + 14) / 15));
    else
      std::snprintf(buf, sizeof(buf), "%s  RERUN", finaleWagonName(w.kind));
    r.drawText(buf, 58.0f, y + 8.0f,
      {18.0f, !run && (frame / 6) % 2 ? rgb(210, 140, 255) : kGoldHi, kHudInk});
    const float left = run ? 1.0f - float(w.t) / float(kFnTrick) : float(w.t) / float(kFnRerun);
    r.fillRect(58.0f, y + 31.0f, 250.0f * std::clamp(left, 0.0f, 1.0f), 3.0f, run ? kLed : rgb(210, 140, 255));
    y += 44.0f;
  }
  // The Encore's wheel.
  if (p.weapon == Weapon::Proto && p.proto == int(ProtoId::Encore))
  {
    const int id = f.wheel[std::size_t(std::clamp(f.sel, 0, int(f.wheel.size()) - 1))];
    const Color c = id < 0 ? rgb(220, 220, 230) : protoDef(id).color;
    const float x = float(kScreenW) - 352.0f, py = 118.0f;
    r.draw(baked(mArt, r, "encoreplate", 340, 44, 0.0f, 0.0f, [](cairo_t* cr) { paintPlateHud(cr, 340, 44); }), x, py);
    drawEncoreIcon(r, mArt, x + 8.0f, py + 4.0f, 0.56f, frame, c);
    std::snprintf(buf, sizeof(buf), "%s", id < 0 ? "BLASTER" : protoDef(id).name);
    r.drawText(buf, x + 52.0f, py + 6.0f, {17.0f, f.spin > 0 && (frame / 2) % 2 ? rgb(255, 255, 255) : c, kHudInk});
    // The pool: thirty pips.
    for (int k = 0; k < kFnPool; ++k)
      r.fillRect(x + 52.0f + float(k) * 9.4f, py + 30.0f, 7.0f, 6.0f, k < p.ammo ? kGold : rgba(255, 255, 255, 40));
  }
  if (!finaleFight())
    return;
  // The boss: its health, Off the Air's race.
  const float bx = float(kScreenW) * 0.5f - 250.0f, by = float(kScreenH) - 60.0f;
  int hp = 0, max = 1;
  const char* who = "";
  switch (b.phase)
  {
    case FnPhase::Lance:
    case FnPhase::Intro:
      hp = b.lanceHp;
      max = kFnLanceHp;
      who = "LANCE MARQUEE";
      break;
    case FnPhase::Engine:
    case FnPhase::Collapse:
      hp = b.engineHp;
      max = kFnEngineHp;
      who = "THE ENCORE ENGINE";
      break;
    case FnPhase::OffAir:
      hp = b.coverHp;
      max = kFnCoverHp;
      who = "THE GLASS CASE";
      break;
    default:
      break;
  }
  r.draw(baked(mArt, r, "bossplate", 524, 48, 0.0f, 0.0f, [](cairo_t* cr) { paintPlateHud(cr, 524, 48); }), bx - 12.0f, by - 10.0f);
  r.fillRect(bx, by, 500.0f, 26.0f, rgba(0, 0, 0, 160));
  r.fillRect(bx, by, 500.0f * float(std::max(0, hp)) / float(max), 26.0f, kOnAir);
  r.fillRect(bx, by, 500.0f * float(std::max(0, hp)) / float(max), 8.0f, rgba(255, 200, 200, 120));
  r.drawText(who, bx + 250.0f, by + 2.0f, {18.0f, rgb(255, 255, 255), kHudInk}, Align::Center);
  if (b.phase == FnPhase::OffAir)
  {
    const float t = float(b.runX - b.runStartX) / float(std::max(1, b.deskX - kFnPodiumW / 2 - b.runStartX));
    const float ry = by - 40.0f;
    r.fillRect(bx, ry, 500.0f, 12.0f, rgba(20, 20, 50, 220));
    r.fillRect(bx, ry, 500.0f * std::clamp(t, 0.0f, 1.0f), 12.0f, kGold);
    r.fillRect(bx + 494.0f, ry - 6.0f, 10.0f, 24.0f, kOnAir);
    r.drawText("LANCE TO THE DESK", bx + 250.0f, ry - 28.0f, {16.0f, kGoldHi, kHudInk}, Align::Center);
    // Lance on the bar, running.
    r.draw(lanceSprite(mArt, r, (frame / 4) % 8, false, 0.12f).get(1), bx + 500.0f * std::clamp(t, 0.0f, 1.0f), ry + 10.0f);
  }
}

// --- Free functions ------------------------------------------------------------------------------

bool drawFinaleShot(Renderer& r, const World& w, const Art& art, const Projectile& pr, float cx, float cy, int frame)
{
  const auto& f = w.finale();
  if (!f.on || f.reel || pr.kind != ShotKind::Enemy)
    return false;
  if (f.boss.on && pr.x >= f.boss.arenaX0)
  {
    // The Engine's beat: a gold note bobbing on its way.
    drawGlow(r, art, cx, cy, 30.0f, kGold, 0.6f);
    DrawOpts o;
    o.angle = std::sin(float(frame) * 0.5f) * 12.0f;
    r.draw(baked(art, r, "note", 40, 40, 20.0f, 22.0f, paintNote), cx, cy, o);
    return true;
  }
  // A Stunt Double's bolt: hot magenta and gold.
  const float dir = pr.dx < 0 ? -1.0f : 1.0f;
  drawGlow(r, art, cx, cy, 26.0f, rgb(255, 90, 180), 0.7f);
  r.drawLine(cx - dir * 30.0f, cy, cx + dir * 6.0f, cy, 8.0f, rgba(255, 120, 200, 200), Blend::Add);
  r.drawLine(cx - dir * 20.0f, cy, cx + dir * 6.0f, cy, 3.0f, rgb(255, 250, 220));
  return true;
}

void drawEncoreIcon(Renderer& r, const Art& art, float x, float y, float scale, int frame, Color sel)
{
  const Texture& icon = baked(art, r, "encoreicon", 64, 64, 0.0f, 0.0f, paintEncoreIcon);
  DrawOpts o;
  o.scale = scale;
  r.draw(icon, x, y, o);
  // The live chamber, lit in the selected prototype's colour.
  const float a = float(frame) * 0.05f;
  drawGlow(r, art, x + (30.0f + std::cos(a) * 10.0f) * scale, y + (30.0f + std::sin(a) * 10.0f) * scale, 10.0f * scale, sel, 0.95f);
}

} // namespace gr
