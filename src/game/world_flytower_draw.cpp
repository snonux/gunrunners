// Level 40, The Fly Tower: drawing. See world_flytower.cpp for the logic.
//
// Behind the tiles (drawFlyTowerSet, flytower_draw.hpp): the stage house's
// back wall of sooty brick with its steel columns, counterweight arbors and
// purchase lines; battens hanging in the haze with black borders and lamps;
// moving-head beams sweeping up out of the show; keyed to the map, the
// roof's trusses and the loft blocks on the grid deck, the hanger rods and
// handrails of every catwalk, the scorched steel behind the pyro wall, the
// wings at the foot (black legs, the lit cyc beyond them, the stage
// manager's prompt desk with its cue lights). In Trigger Happy, the
// scaffold's poles and braces.
//
// Over the tiles, behind the runner (drawFlyTowerBack): the pyro nozzles,
// glowing and spitting sparks before they burn; the Q99 cannon and its fuse
// box; the trusses (silver box truss, lamps under them, lift lines up to
// the grid, chase lights running before a move, red while one waits); the
// confetti cannons on their stands, rattling before a blast; the torn ends
// of a catwalk a sandbag broke; the "42 LB" sandbag, Lance's smoke machine,
// the towel; Trigger Happy's discs.
//
// In front (drawFlyTowerFront): the fly crew (followspots on their stands,
// the barrel turning to its beam; Riggers; Sandbag Drops on their lines),
// the followspot beams and the pool of light they put on the runner (white,
// amber once locked), the Riggers' laser sights, the bags hanging and their
// growing shadows, the bags falling, the flame columns, the confetti
// blasts, the Pyro Rig's flame stream and its jetpack plume. The HUD
// (drawFlyTowerHud): the stage manager's cue sheet, and Trigger Happy's
// disc count.

#include "game/world.hpp"

#include "assets/art.hpp"
#include "assets/enemy_art.hpp"
#include "base/math.hpp"
#include "data/enemies.hpp"
#include "data/weapons.hpp"
#include "game/flytower_draw.hpp"
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
constexpr Color kInk = rgb(8, 8, 12);
constexpr Color kHudInk = rgb(6, 6, 12);
constexpr Color kSteel = rgb(150, 152, 162);
constexpr Color kSteelHi = rgb(214, 216, 226);
constexpr Color kSteelLo = rgb(64, 66, 76);
constexpr Color kBlackSteel = rgb(36, 36, 42);
constexpr Color kAmber = rgb(255, 170, 60);
constexpr Color kMagenta = rgb(230, 70, 170);
constexpr Color kCyan = rgb(80, 190, 255);
constexpr Color kFire = rgb(255, 120, 30);
constexpr Color kFireHot = rgb(255, 230, 140);
constexpr Color kGlowTape = rgb(170, 255, 190);
constexpr Color kRope = rgb(214, 190, 140);
constexpr Color kCanvas = rgb(170, 150, 110);
constexpr Color kRed = rgb(255, 50, 40);

Color darken(Color c, float t) { return lerpColor(c, rgb(0, 0, 0), t); }
Color lighten(Color c, float t) { return lerpColor(c, rgb(255, 255, 255), t); }

float lerpCells(int prev, int cur, float alpha) { return (float(prev) + float(cur - prev) * alpha) * kCellPx; }

bool offScreen(float x, float y, float w, float h, float m = 96.0f)
{
  return x > float(kScreenW) + m || x + w < -m || y > float(kScreenH) + m || y + h < -m;
}

float unit(std::uint32_t h) { return float(h % 10000u) / 10000.0f; }

float wrapFrom(float v, float period, float base)
{
  float u = std::fmod(v - base, period);
  if (u < 0.0f)
    u += period;
  return u;
}

const Sprite& bakedSprite(const Art& art, const Renderer& r, const std::string& key, int w, int h, float ax, float ay,
  const std::function<void(cairo_t*)>& paint)
{
  const std::string id = "~flytower/" + key;
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

void bolt(cairo_t* cr, double x, double y, double r = 2.0)
{
  circle(cr, x, y, r);
  setRgba(cr, kSteelLo, 1.0);
  cairo_fill(cr);
  circle(cr, x - r * 0.3, y - r * 0.3, r * 0.5);
  setRgba(cr, kSteelHi, 0.8);
  cairo_fill(cr);
}

// --- Renderer helpers ------------------------------------------------------------------------

// Clips everything drawn while it lives to a rectangle (inside any clip
// already set), and puts the old clip back.
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
  SDL_RenderCopyExF(r.sdl(), r.use(t), nullptr, &dst, double(angle), &pivot, SDL_FLIP_NONE);
  SDL_SetTextureColorMod(t.get(), 255, 255, 255);
  SDL_SetTextureAlphaMod(t.get(), 255);
}

// A texture tiled over a rectangle from (ox, oy), only the part on screen.
void tile(Renderer& r, const Texture& t, float x0, float y0, float x1, float y1, float ox, float oy, const DrawOpts& o = {})
{
  const float tw = float(t.w()), th = float(t.h());
  const float sx0 = std::max(x0, 0.0f), sx1 = std::min(x1, float(kScreenW));
  const float sy0 = std::max(y0, 0.0f), sy1 = std::min(y1, float(kScreenH));
  if (sx0 >= sx1 || sy0 >= sy1)
    return;
  Clip clip(r, sx0, sy0, sx1 - sx0, sy1 - sy0);
  const float fx = sx0 - wrapFrom(sx0 - ox, tw, 0.0f), fy = sy0 - wrapFrom(sy0 - oy, th, 0.0f);
  for (float y = fy; y < sy1; y += th)
    for (float x = fx; x < sx1; x += tw)
      r.draw(t, x, y, o);
}

// --- Painters: the stage house ---------------------------------------------------------------

constexpr int kWallTex = 1024;

// The back wall (1024 x 1024, seamless both ways): courses of sooty
// black-painted brick, two steel columns with bolted plates, a run of
// counterweight arbors in their T-tracks with the purchase lines up past
// them, caged worklights. Dark: it sits deep in the haze.
void paintWall(cairo_t* cr)
{
  const double S = kWallTex;
  box(cr, 0, 0, S, S, rgb(20, 15, 18));
  Rng rng(40101u);
  for (int row = 0; row < 32; ++row)
  {
    const double y = row * 32.0, off = row % 2 ? 32.0 : 0.0;
    for (int c = -1; c < 17; ++c)
    {
      const double x = c * 64.0 + off;
      const float v = rng.range(-0.1f, 0.12f);
      const Color b = lerpColor(rgb(38, 28, 28), v < 0 ? rgb(0, 0, 0) : rgb(96, 70, 60), std::abs(v));
      box(cr, x + 2, y + 2, 60, 28, b);
      box(cr, x + 2, y + 2, 60, 2, rgb(80, 60, 56), 0.2);
    }
  }
  // Soot running down in streaks.
  for (int i = 0; i < 18; ++i)
  {
    const double x = rng.range(0, float(S)), w = rng.range(10, 46), y = rng.range(0, float(S));
    for (const double yy : {y, y - S})
    {
      cairo_rectangle(cr, x, yy, w, rng.range(200, 600));
      vgrad(cr, yy, yy + 500, {{0.0, kInk}, {1.0, rgb(0, 0, 0)}}, 0.0);
      cairo_rectangle(cr, x, yy, w, 420);
      cairo_pattern_t* g = cairo_pattern_create_linear(0, yy, 0, yy + 420);
      stop(g, 0.0, kInk, 0.0);
      stop(g, 0.3, kInk, 0.35);
      stop(g, 1.0, kInk, 0.0);
      cairo_set_source(cr, g);
      cairo_fill(cr);
      cairo_pattern_destroy(g);
    }
  }
  // Counterweight arbors in their tracks (x 560 .. 900).
  for (int k = 0; k < 7; ++k)
  {
    const double x = 560 + k * 50.0;
    box(cr, x - 3, 0, 6, S, rgb(44, 44, 52));
    box(cr, x + 26, 0, 6, S, rgb(44, 44, 52));
    cairo_move_to(cr, x + 14, 0);
    cairo_line_to(cr, x + 14, S);
    stroke(cr, rgb(120, 104, 80), 1.6, 0.55);
    const double ay = std::fmod(140.0 + k * 337.0, S - 200.0);
    cairo_rectangle(cr, x - 2, ay, 32, 150);
    setRgba(cr, rgb(30, 30, 36), 1.0);
    cairo_fill(cr);
    for (double y = ay + 12; y < ay + 138; y += 10)
      box(cr, x + 2, y, 24, 8, rgb(66, 62, 68));
    box(cr, x - 2, ay, 32, 5, rgb(100, 100, 110));
    box(cr, x - 2, ay + 145, 32, 5, rgb(100, 100, 110));
  }
  // The columns.
  for (const double x : {200.0, 1000.0})
  {
    for (const double xx : {x, x - S})
    {
      cairo_rectangle(cr, xx - 26, 0, 52, S);
      hgrad(cr, xx - 26, xx + 26, {{0.0, rgb(20, 20, 24)}, {0.25, rgb(58, 58, 66)}, {0.55, rgb(34, 34, 40)}, {1.0, rgb(14, 14, 18)}});
      for (double y = 60; y < S; y += 256)
      {
        box(cr, xx - 30, y, 60, 22, rgb(46, 46, 54));
        for (const double bx : {-18.0, 0.0, 18.0})
          bolt(cr, xx + bx, y + 11, 2.6);
      }
    }
  }
  // Worklights in their cages.
  for (const auto& p : {std::array<double, 2>{420, 260}, std::array<double, 2>{120, 780}, std::array<double, 2>{780, 640}})
  {
    softDot(cr, p[0], p[1], 90, 70, rgb(120, 150, 230), 0.16);
    circle(cr, p[0], p[1], 7);
    setRgba(cr, rgb(220, 230, 255), 0.85);
    cairo_fill(cr);
    for (int k = -1; k <= 1; ++k)
    {
      cairo_move_to(cr, p[0] + k * 5, p[1] - 10);
      cairo_line_to(cr, p[0] + k * 5, p[1] + 10);
    }
    stroke(cr, rgb(40, 40, 46), 1.4);
    box(cr, p[0] - 9, p[1] - 15, 18, 6, rgb(44, 44, 50));
  }
}

constexpr int kBattenW = 1280, kBattenH = 1024;

// Battens hanging across the tower (1280 x 1024, seamless both ways,
// see-through): two pipes with black velour borders hanging from them in
// folds, PAR cans and moving heads clamped on, lift lines running up.
void paintBattens(cairo_t* cr)
{
  Rng rng(40111u);
  for (const double y : {140.0, 660.0})
  {
    // Lift lines up from the pipe.
    for (double x = 160; x < kBattenW; x += 480)
    {
      cairo_move_to(cr, x, y - kBattenH);
      cairo_line_to(cr, x, y);
      cairo_move_to(cr, x, y);
      cairo_line_to(cr, x, y - 2000);
      stroke(cr, rgb(90, 90, 100), 1.4, 0.6);
    }
    const double drop = 130;
    for (double x = 0; x < kBattenW; x += 20)
    {
      cairo_rectangle(cr, x, y, 20, drop);
      hgrad(cr, x, x + 20, {{0.0, rgb(8, 6, 10)}, {0.5, rgb(28, 22, 30)}, {1.0, rgb(8, 6, 10)}});
    }
    cairo_move_to(cr, 0, y + drop - 8);
    for (double x = 0; x <= kBattenW; x += 20)
      cairo_line_to(cr, x, y + drop + (std::fmod(x, 40.0) < 20 ? 4 : 0));
    cairo_line_to(cr, kBattenW, y + drop - 8);
    cairo_close_path(cr);
    setRgba(cr, rgb(18, 14, 20), 1.0);
    cairo_fill(cr);
    cairo_rectangle(cr, 0, y - 7, kBattenW, 9);
    vgrad(cr, y - 7, y + 2, {{0.0, rgb(120, 120, 130)}, {0.5, rgb(54, 54, 62)}, {1.0, rgb(20, 20, 24)}});
    for (double x = rng.range(50, 120); x < kBattenW - 50; x += rng.range(120, 200))
    {
      cairo_save(cr);
      cairo_translate(cr, x, y - 6);
      if (rng.uniform() < 0.35f)
      {
        box(cr, -16, -26, 32, 12, rgb(30, 30, 36));
        circle(cr, 0, -40, 15);
        setRgba(cr, rgb(36, 36, 42), 1.0);
        cairo_fill(cr);
        circle(cr, 3, -40, 7);
        setRgba(cr, rgb(255, 220, 200), 0.3);
        cairo_fill(cr);
      }
      else
      {
        box(cr, -3, -12, 6, 12, rgb(30, 30, 36));
        cairo_rotate(cr, rng.range(-0.5f, 0.5f));
        cairo_rectangle(cr, -12, -44, 24, 34);
        hgrad(cr, -12, 12, {{0.0, rgb(30, 30, 36)}, {0.4, rgb(68, 68, 78)}, {1.0, rgb(20, 20, 24)}});
        box(cr, -11, -46, 22, 3, rgb(255, 200, 150), 0.35);
      }
      cairo_restore(cr);
    }
  }
}

// A beam of light (128 x 1024): a cone from a point at the top widening to
// the bottom, bright at the source and fading out.
void paintBeam(cairo_t* cr)
{
  cairo_move_to(cr, 60, 0);
  cairo_line_to(cr, 68, 0);
  cairo_line_to(cr, 128, 1024);
  cairo_line_to(cr, 0, 1024);
  cairo_close_path(cr);
  cairo_pattern_t* g = cairo_pattern_create_linear(0, 0, 0, 1024);
  stop(g, 0.0, rgb(255, 255, 255), 0.9);
  stop(g, 0.25, rgb(255, 255, 255), 0.45);
  stop(g, 1.0, rgb(255, 255, 255), 0.0);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
  // Softer edges: a brighter core down the middle.
  cairo_move_to(cr, 62, 0);
  cairo_line_to(cr, 66, 0);
  cairo_line_to(cr, 84, 1024);
  cairo_line_to(cr, 44, 1024);
  cairo_close_path(cr);
  g = cairo_pattern_create_linear(0, 0, 0, 1024);
  stop(g, 0.0, rgb(255, 255, 255), 0.6);
  stop(g, 1.0, rgb(255, 255, 255), 0.0);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
}

// A soft pool of light (256 x 64), white.
void paintPool(cairo_t* cr)
{
  softDot(cr, 128, 32, 126, 30, rgb(255, 255, 255), 0.9);
  softDot(cr, 128, 32, 70, 16, rgb(255, 255, 255), 0.6);
}

// A soft round shadow (128 x 32).
void paintShadow(cairo_t* cr) { softDot(cr, 64, 16, 63, 15, rgb(0, 0, 0), 0.85); }

// A roof truss segment (1024 x 300, tiles side by side): a deep Warren
// truss of black steel, its top chord sloping to the ridge, a skylight's
// moonlit glass between two of its bays.
void paintRoofTruss(cairo_t* cr)
{
  const double W = 1024;
  // The roof deck behind.
  cairo_rectangle(cr, 0, 0, W, 90);
  vgrad(cr, 0, 90, {{0.0, rgb(6, 6, 10)}, {1.0, rgb(18, 16, 22)}});
  for (double x = 0; x < W; x += 32)
  {
    cairo_move_to(cr, x, 0);
    cairo_line_to(cr, x, 90);
  }
  stroke(cr, rgb(30, 28, 34), 2.0);
  // A smoke vent's glass, the moon through the haze.
  cairo_rectangle(cr, 380, 8, 260, 70);
  vgrad(cr, 8, 78, {{0.0, rgb(40, 50, 90)}, {1.0, rgb(20, 24, 44)}});
  softDot(cr, 560, 30, 30, 30, rgb(220, 230, 255), 0.6);
  for (double x = 380; x <= 640; x += 65)
  {
    cairo_move_to(cr, x, 8);
    cairo_line_to(cr, x, 78);
  }
  stroke(cr, rgb(20, 20, 26), 4.0);
  // The truss: top and bottom chords, verticals, diagonals.
  auto chord = [&](double y, double h) {
    cairo_rectangle(cr, 0, y, W, h);
    vgrad(cr, y, y + h, {{0.0, rgb(84, 84, 94)}, {0.4, rgb(44, 44, 52)}, {1.0, rgb(18, 18, 22)}});
  };
  for (double x = 0; x < W; x += 128)
  {
    cairo_move_to(cr, x, 100);
    cairo_line_to(cr, x + 64, 250);
    cairo_line_to(cr, x + 128, 100);
  }
  stroke(cr, rgb(16, 16, 20), 12.0);
  for (double x = 0; x < W; x += 128)
  {
    cairo_move_to(cr, x, 100);
    cairo_line_to(cr, x + 64, 250);
    cairo_line_to(cr, x + 128, 100);
  }
  stroke(cr, rgb(48, 48, 56), 7.0);
  chord(90, 18);
  chord(246, 16);
  for (double x = 0; x < W; x += 128)
  {
    box(cr, x - 10, 100, 20, 12, rgb(60, 60, 70));
    bolt(cr, x - 4, 106, 2.0);
    bolt(cr, x + 4, 106, 2.0);
  }
  // Rigging points hanging from the bottom chord: shackles and chain.
  for (double x = 96; x < W; x += 256)
  {
    cairo_move_to(cr, x, 262);
    cairo_line_to(cr, x, 296);
    stroke(cr, rgb(110, 110, 120), 2.4);
    circle(cr, x, 270, 5);
    stroke(cr, rgb(150, 150, 160), 2.0);
  }
}

// A loft block (64 x 56): a sheave in its steel housing bolted to the grid,
// the steel lines over it.
void paintLoftBlock(cairo_t* cr)
{
  box(cr, 6, 44, 52, 12, rgb(40, 40, 48));
  cairo_move_to(cr, 10, 44);
  cairo_line_to(cr, 14, 10);
  cairo_line_to(cr, 50, 10);
  cairo_line_to(cr, 54, 44);
  cairo_close_path(cr);
  hgrad(cr, 10, 54, {{0.0, rgb(30, 30, 36)}, {0.4, rgb(70, 70, 80)}, {1.0, rgb(24, 24, 28)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  circle(cr, 32, 26, 15);
  setRgba(cr, rgb(20, 20, 24), 1.0);
  cairo_fill(cr);
  circle(cr, 32, 26, 12);
  stroke(cr, rgb(130, 130, 140), 2.0);
  circle(cr, 32, 26, 3.5);
  setRgba(cr, kSteelHi, 1.0);
  cairo_fill(cr);
  for (const double x : {14.0, 50.0})
    bolt(cr, x, 50, 2.2);
}

// A black velour leg (240 x 900): a soft curtain hanging in deep folds,
// its edge catching the cyc's glow.
void paintLeg(cairo_t* cr)
{
  for (double x = 0; x < 240; x += 30)
  {
    cairo_rectangle(cr, x, 0, 30, 900);
    hgrad(cr, x, x + 30, {{0.0, rgb(6, 4, 8)}, {0.45, rgb(32, 24, 34)}, {0.6, rgb(26, 20, 28)}, {1.0, rgb(6, 4, 8)}});
  }
  cairo_rectangle(cr, 226, 0, 14, 900);
  hgrad(cr, 226, 240, {{0.0, rgb(30, 20, 40)}, {1.0, rgb(90, 50, 120)}}, 0.6);
  cairo_rectangle(cr, 0, 860, 240, 40);
  vgrad(cr, 860, 900, {{0.0, rgb(16, 12, 18)}, {1.0, rgb(4, 2, 6)}});
}

// The stage manager's prompt desk (240 x 200, its feet on the floor at the
// bottom): a black desk with a script on a lit lectern, a show-relay
// monitor, the cue-light panel (red standby, green GO), a headset hung on
// its hook, a gooseneck lamp in blue.
void paintPromptDesk(cairo_t* cr)
{
  // The legs and the desk body.
  box(cr, 20, 120, 12, 80, rgb(26, 26, 30));
  box(cr, 200, 120, 12, 80, rgb(26, 26, 30));
  cairo_rectangle(cr, 10, 100, 220, 26);
  vgrad(cr, 100, 126, {{0.0, rgb(60, 58, 66)}, {0.2, rgb(34, 32, 38)}, {1.0, rgb(16, 16, 20)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  // The monitor showing the stage: a lit stage and tiny figures.
  cairo_rectangle(cr, 128, 26, 92, 66);
  setRgba(cr, rgb(24, 24, 28), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.6);
  cairo_rectangle(cr, 134, 32, 80, 50);
  vgrad(cr, 32, 82, {{0.0, rgb(30, 40, 90)}, {0.6, rgb(120, 60, 120)}, {1.0, rgb(200, 140, 90)}});
  box(cr, 134, 70, 80, 12, rgb(70, 40, 40));
  for (int k = 0; k < 3; ++k)
    box(cr, 150 + k * 18, 56, 5, 14, rgb(20, 16, 24));
  box(cr, 168, 92, 8, 8, rgb(30, 30, 34));
  // The script on its lectern under the blue lamp.
  cairo_move_to(cr, 30, 98);
  cairo_line_to(cr, 40, 70);
  cairo_line_to(cr, 112, 70);
  cairo_line_to(cr, 108, 98);
  cairo_close_path(cr);
  setRgba(cr, rgb(200, 210, 240), 1.0);
  cairo_fill(cr);
  for (int k = 0; k < 6; ++k)
  {
    cairo_move_to(cr, 44 + k * 0.6, 76 + k * 3.6);
    cairo_line_to(cr, 102 - (k % 3) * 8, 76 + k * 3.6);
  }
  stroke(cr, rgb(60, 70, 110), 0.9, 0.8);
  box(cr, 72, 82, 30, 2, rgb(255, 230, 40), 0.8);
  cairo_move_to(cr, 26, 100);
  cairo_curve_to(cr, 20, 60, 60, 40, 72, 50);
  stroke(cr, rgb(40, 40, 48), 3.0);
  cairo_move_to(cr, 64, 46);
  cairo_line_to(cr, 82, 46);
  cairo_line_to(cr, 78, 56);
  cairo_line_to(cr, 66, 56);
  cairo_close_path(cr);
  setRgba(cr, rgb(40, 40, 48), 1.0);
  cairo_fill(cr);
  softDot(cr, 72, 76, 50, 26, rgb(110, 140, 255), 0.4);
  // The cue lights.
  for (int k = 0; k < 4; ++k)
  {
    const double x = 22 + k * 22.0;
    box(cr, x, 108, 16, 12, rgb(16, 16, 20));
    circle(cr, x + 5, 114, 3.2);
    setRgba(cr, k == 1 ? rgb(255, 50, 40) : rgb(70, 20, 20), 1.0);
    cairo_fill(cr);
    circle(cr, x + 12, 114, 3.2);
    setRgba(cr, k == 2 ? rgb(60, 255, 120) : rgb(20, 60, 30), 1.0);
    cairo_fill(cr);
  }
  softDot(cr, 49, 114, 10, 10, rgb(255, 60, 40), 0.6);
  softDot(cr, 78, 114, 10, 10, rgb(60, 255, 120), 0.6);
  // The headset on its hook.
  cairo_arc(cr, 218, 112, 10, kPi, 2 * kPi);
  stroke(cr, rgb(50, 50, 58), 2.6);
  ellipse(cr, 208, 114, 4, 6);
  setRgba(cr, rgb(30, 30, 36), 1.0);
  cairo_fill(cr);
  ellipse(cr, 228, 114, 4, 6);
  cairo_fill(cr);
  // A strip of glow tape on its corner, PROMPT on a scrap of tape.
  box(cr, 12, 100, 16, 3, kGlowTape, 0.8);
  cairo_save(cr);
  cairo_translate(cr, 160, 113);
  cairo_rotate(cr, -0.04);
  box(cr, -22, -5, 44, 10, rgb(230, 230, 220), 0.9);
  text(cr, "PROMPT", 0, 3.2, 7.5, rgb(30, 30, 40));
  cairo_restore(cr);
}

// The scorched steel behind the pyro wall (256 x 256, tiles): fireproof
// sheet steel bolted in panels, blued and blackened by the burns.
void paintPyroBack(cairo_t* cr)
{
  box(cr, 0, 0, 256, 256, rgb(34, 30, 32));
  Rng rng(40121u);
  for (int k = 0; k < 4; ++k)
  {
    const double x = (k % 2) * 128.0, y = (k / 2) * 128.0;
    cairo_rectangle(cr, x + 2, y + 2, 124, 124);
    vgrad(cr, y, y + 128, {{0.0, rgb(56, 50, 52)}, {0.5, rgb(42, 38, 42)}, {1.0, rgb(30, 26, 30)}});
    for (int i = 0; i < 3; ++i)
      softDot(cr, x + rng.range(20, 108), y + rng.range(20, 108), rng.range(20, 50), rng.range(30, 60),
        i % 2 ? rgb(80, 60, 110) : rgb(10, 6, 8), 0.45);
    for (const double bx : {x + 8, x + 120})
      for (const double by : {y + 8, y + 120})
        bolt(cr, bx, by, 2.2);
  }
}

// --- Painters: pyro --------------------------------------------------------------------------

// A pyro flame projector (128 x 44, its top at the nozzle cell's top): a
// black steel box, the nozzle's slot along the top, a red ARMED lamp, its
// cable snaking off; `hot` the slot glows.
void paintNozzle(cairo_t* cr)
{
  cairo_move_to(cr, 8, 44);
  cairo_line_to(cr, 14, 10);
  cairo_line_to(cr, 114, 10);
  cairo_line_to(cr, 120, 44);
  cairo_close_path(cr);
  hgrad(cr, 8, 120, {{0.0, rgb(22, 22, 26)}, {0.3, rgb(58, 58, 66)}, {1.0, rgb(18, 18, 22)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  roundRect(cr, 30, 2, 68, 12, 3);
  vgrad(cr, 2, 14, {{0.0, rgb(110, 110, 120)}, {1.0, rgb(40, 40, 46)}}, 1.0, true);
  stroke(cr, kInk, 1.2);
  box(cr, 36, 4, 56, 4, rgb(16, 12, 12));
  for (int k = 0; k < 6; ++k)
    box(cr, 18 + k * 16, 30, 10, 3, rgb(20, 20, 24));
  // Hazard tape and the stencil.
  cairo_save(cr);
  cairo_rectangle(cr, 14, 36, 100, 6);
  cairo_clip(cr);
  box(cr, 14, 36, 100, 6, rgb(240, 190, 40));
  for (double x = 6; x < 120; x += 10)
  {
    cairo_move_to(cr, x, 42);
    cairo_line_to(cr, x + 5, 42);
    cairo_line_to(cr, x + 11, 36);
    cairo_line_to(cr, x + 6, 36);
    cairo_close_path(cr);
    setRgba(cr, rgb(20, 20, 24), 1.0);
    cairo_fill(cr);
  }
  cairo_restore(cr);
  text(cr, "PYRO", 64, 27, 8, rgb(230, 230, 230), 0.6);
  box(cr, 100, 16, 8, 6, rgb(30, 10, 10));
}

// The Q99 cannon (112 x 80, the barrel's mouth at the left, its foot at
// the bottom): a stubby flame cannon on a black base, pointed along the
// catwalk, the struck cue number on its side.
void paintCannon(cairo_t* cr)
{
  box(cr, 30, 64, 70, 16, rgb(26, 26, 30));
  box(cr, 30, 64, 70, 3, rgb(80, 80, 90));
  cairo_move_to(cr, 50, 64);
  cairo_line_to(cr, 60, 44);
  cairo_line_to(cr, 80, 44);
  cairo_line_to(cr, 90, 64);
  cairo_close_path(cr);
  setRgba(cr, rgb(40, 40, 46), 1.0);
  cairo_fill(cr);
  roundRect(cr, 4, 22, 100, 30, 8);
  vgrad(cr, 22, 52, {{0.0, rgb(96, 96, 106)}, {0.35, rgb(50, 50, 58)}, {1.0, rgb(18, 18, 22)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  ellipse(cr, 6, 37, 6, 14);
  setRgba(cr, rgb(30, 22, 20), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.4);
  ellipse(cr, 6, 37, 3, 9);
  setRgba(cr, rgb(8, 6, 6), 1.0);
  cairo_fill(cr);
  for (const double x : {30.0, 56.0, 82.0})
    box(cr, x, 24, 4, 26, rgb(30, 30, 36));
  text(cr, "Q99", 66, 42, 11, rgb(230, 230, 230), 0.75);
  cairo_move_to(cr, 50, 38);
  cairo_line_to(cr, 82, 38);
  stroke(cr, rgb(230, 230, 230), 1.4, 0.75);
}

// The cannon's fuse box (48 x 76): a grey steel box on a stalk, two
// breakers behind a cracked window.
void paintFuseBox(cairo_t* cr)
{
  box(cr, 20, 50, 8, 26, rgb(40, 40, 46));
  roundRect(cr, 4, 6, 40, 46, 3);
  vgrad(cr, 6, 52, {{0.0, rgb(150, 150, 160)}, {0.3, rgb(100, 100, 110)}, {1.0, rgb(50, 50, 58)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  box(cr, 10, 12, 28, 22, rgb(20, 24, 30));
  for (int k = 0; k < 2; ++k)
    box(cr, 14 + k * 12, 16, 8, 14, rgb(60, 60, 70));
  text(cr, "FUSE", 24, 46, 7, rgb(30, 30, 36));
  cairo_move_to(cr, 26, 0);
  cairo_line_to(cr, 26, 6);
  stroke(cr, kInk, 2.0);
}

// Fire: a flame column's body (128 x 256, seamless top to bottom): tongues
// of flame licking up, white-yellow at the heart, orange, red at the
// ragged edges. `k` one of four frames.
void paintFlameBody(cairo_t* cr, int k)
{
  Rng rng(40131u + std::uint32_t(k) * 977u);
  for (int i = 0; i < 46; ++i)
  {
    const double u = rng.range(-1.0f, 1.0f);
    const double x = 64 + u * std::abs(u) * 50;
    const double y = rng.range(0, 256);
    const double rx = rng.range(14, 30) * (1.0 - std::abs(u) * 0.4), ry = rng.range(30, 60);
    const double heat = 1.0 - std::abs(u);
    for (const double yy : {y, y - 256, y + 256})
    {
      cairo_save(cr);
      cairo_translate(cr, x, yy);
      cairo_scale(cr, rx, ry);
      cairo_move_to(cr, 0, -1.4);
      cairo_curve_to(cr, 0.5, -0.6, 1.0, 0.1, 0.0, 1.0);
      cairo_curve_to(cr, -1.0, 0.1, -0.5, -0.6, 0, -1.4);
      cairo_close_path(cr);
      cairo_restore(cr);
      cairo_pattern_t* g = cairo_pattern_create_radial(x, yy + ry * 0.3, 0, x, yy, ry * 1.2);
      stop(g, 0.0, lerpColor(rgb(255, 120, 30), rgb(255, 250, 210), float(heat)), 0.55 + heat * 0.35);
      stop(g, 0.5, lerpColor(rgb(220, 40, 10), rgb(255, 150, 40), float(heat)), 0.35 + heat * 0.2);
      stop(g, 1.0, rgb(160, 20, 10), 0.0);
      cairo_set_source(cr, g);
      cairo_fill(cr);
      cairo_pattern_destroy(g);
    }
  }
}

// The top of a flame column (128 x 192, its base at the bottom): the
// tongues thinning and breaking off into the air, smoke over them.
void paintFlameTip(cairo_t* cr, int k)
{
  Rng rng(40141u + std::uint32_t(k) * 613u);
  for (int i = 0; i < 26; ++i)
  {
    const double t = rng.uniform();
    const double u = rng.range(-1.0f, 1.0f) * (1.0 - t * 0.6);
    const double x = 64 + u * 46, y = 192 - t * 170;
    const double rx = (1.0 - t) * 22 + 5, ry = (1.0 - t) * 34 + 10;
    cairo_save(cr);
    cairo_translate(cr, x, y);
    cairo_scale(cr, rx, ry);
    cairo_move_to(cr, 0, -1.5);
    cairo_curve_to(cr, 0.5, -0.6, 1.0, 0.1, 0.0, 1.0);
    cairo_curve_to(cr, -1.0, 0.1, -0.5, -0.6, 0, -1.5);
    cairo_close_path(cr);
    cairo_restore(cr);
    cairo_pattern_t* g = cairo_pattern_create_radial(x, y + ry * 0.3, 0, x, y, ry * 1.2);
    stop(g, 0.0, lerpColor(rgb(255, 240, 180), rgb(255, 120, 30), float(t)), 0.8 - t * 0.4);
    stop(g, 0.6, rgb(230, 60, 10), 0.4 - t * 0.2);
    stop(g, 1.0, rgb(140, 20, 10), 0.0);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
}

// A jet of flame (256 x 72, from its root at the left): the Pyro Rig's
// stream and the Q99 cannon's blast.
void paintFlameJet(cairo_t* cr, int k)
{
  Rng rng(40151u + std::uint32_t(k) * 389u);
  for (int i = 0; i < 40; ++i)
  {
    const double t = rng.uniform();
    const double x = 8 + t * 236, spread = 8 + t * 26;
    const double y = 36 + rng.range(-1.0f, 1.0f) * spread * 0.7;
    const double rx = 14 + t * 22, ry = 6 + t * 12;
    cairo_save(cr);
    cairo_translate(cr, x, y);
    cairo_scale(cr, rx, ry);
    cairo_move_to(cr, 1.4, 0);
    cairo_curve_to(cr, 0.6, 0.5, -0.1, 1.0, -1.0, 0);
    cairo_curve_to(cr, -0.1, -1.0, 0.6, -0.5, 1.4, 0);
    cairo_close_path(cr);
    cairo_restore(cr);
    cairo_pattern_t* g = cairo_pattern_create_radial(x, y, 0, x, y, rx * 1.2);
    stop(g, 0.0, lerpColor(rgb(255, 250, 220), rgb(255, 120, 30), float(t)), 0.85 - t * 0.35);
    stop(g, 0.6, lerpColor(rgb(255, 150, 40), rgb(200, 40, 10), float(t)), 0.45);
    stop(g, 1.0, rgb(140, 20, 10), 0.0);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
}

// --- Painters: rigging -----------------------------------------------------------------------

// A flying truss `wCells` wide (its walking deck at y 8, 112 tall in all):
// a silver box truss with zigzag lacing, a grated deck on its top chord,
// PAR cans hanging under it; the chase-light sockets along its face are
// left for the level to light.
void paintTruss(cairo_t* cr, int wCells)
{
  const double W = wCells * 32.0;
  // The lamps under it.
  for (double x = 40; x < W - 20; x += 96)
  {
    box(cr, x - 3, 64, 6, 12, rgb(30, 30, 36));
    cairo_save(cr);
    cairo_translate(cr, x, 76);
    cairo_rotate(cr, (std::fmod(x, 192.0) < 96 ? -0.3 : 0.25));
    cairo_rectangle(cr, -13, 0, 26, 30);
    hgrad(cr, -13, 13, {{0.0, rgb(26, 26, 32)}, {0.4, rgb(70, 70, 80)}, {1.0, rgb(20, 20, 24)}}, 1.0, true);
    stroke(cr, kInk, 1.2);
    box(cr, -12, 29, 24, 4, rgb(255, 210, 160), 0.7);
    cairo_restore(cr);
  }
  // The chords.
  auto chord = [&](double y) {
    cairo_rectangle(cr, 0, y - 4, W, 8);
    vgrad(cr, y - 4, y + 4, {{0.0, rgb(240, 242, 248)}, {0.4, rgb(170, 172, 182)}, {1.0, rgb(80, 82, 92)}}, 1.0, true);
    stroke(cr, kInk, 1.0);
  };
  // The lacing between them.
  for (double x = 0; x < W; x += 32)
  {
    cairo_move_to(cr, x, 14);
    cairo_line_to(cr, x + 16, 60);
    cairo_line_to(cr, x + 32, 14);
  }
  stroke(cr, rgb(40, 40, 46), 4.4);
  for (double x = 0; x < W; x += 32)
  {
    cairo_move_to(cr, x, 14);
    cairo_line_to(cr, x + 16, 60);
    cairo_line_to(cr, x + 32, 14);
  }
  stroke(cr, rgb(190, 192, 200), 2.2);
  chord(14);
  chord(60);
  // The deck on top, grated, with a toe board.
  box(cr, 0, 0, W, 10, rgb(24, 24, 28));
  for (double x = 1; x < W; x += 4)
    box(cr, x, 1, 1.6, 6, rgb(110, 110, 120), 0.9);
  box(cr, 0, 0, W, 1.2, rgb(230, 220, 200), 0.6);
  box(cr, 0, 7, W, 3, rgb(230, 190, 40), 0.8);
  // End plates.
  for (const double x : {0.0, W - 8})
  {
    cairo_rectangle(cr, x, 6, 8, 60);
    hgrad(cr, x, x + 8, {{0.0, rgb(200, 200, 210)}, {1.0, rgb(90, 90, 100)}}, 1.0, true);
    stroke(cr, kInk, 1.0);
  }
}

// A CO2 confetti cannon on its stand (128 x 80, facing right, its feet at
// the bottom): a fat tube angled up, a gas bottle strapped behind, the
// stand's legs, its number on a tag.
void paintConfettiCannon(cairo_t* cr, const std::string& label)
{
  // The stand.
  cairo_move_to(cr, 30, 80);
  cairo_line_to(cr, 56, 46);
  cairo_line_to(cr, 82, 80);
  cairo_move_to(cr, 56, 46);
  cairo_line_to(cr, 56, 80);
  stroke(cr, rgb(30, 30, 36), 5.0);
  cairo_move_to(cr, 30, 80);
  cairo_line_to(cr, 56, 46);
  cairo_line_to(cr, 82, 80);
  stroke(cr, rgb(90, 90, 100), 1.6, 0.8);
  // The gas bottle.
  roundRect(cr, 12, 40, 18, 38, 7);
  hgrad(cr, 12, 30, {{0.0, rgb(140, 160, 180)}, {0.4, rgb(80, 100, 120)}, {1.0, rgb(40, 50, 60)}}, 1.0, true);
  stroke(cr, kInk, 1.2);
  box(cr, 17, 34, 8, 7, rgb(60, 60, 66));
  cairo_move_to(cr, 21, 34);
  cairo_curve_to(cr, 22, 24, 34, 28, 40, 34);
  stroke(cr, kInk, 2.2);
  // The tube, angled up.
  cairo_save(cr);
  cairo_translate(cr, 56, 44);
  cairo_rotate(cr, -0.35);
  roundRect(cr, -34, -13, 96, 26, 6);
  hgrad(cr, -34, 62, {{0.0, rgb(50, 50, 58)}, {1.0, rgb(30, 30, 34)}}, 1.0, false);
  roundRect(cr, -34, -13, 96, 26, 6);
  vgrad(cr, -13, 13, {{0.0, rgb(250, 90, 170)}, {0.35, rgb(200, 50, 140)}, {1.0, rgb(90, 20, 60)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  for (const double x : {-20.0, 20.0})
    box(cr, x, -13, 6, 26, rgb(40, 40, 46));
  ellipse(cr, 62, 0, 5, 13);
  setRgba(cr, rgb(30, 10, 24), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.2);
  // Confetti peeking out of the mouth.
  for (int k = 0; k < 6; ++k)
  {
    static const Color kBits[] = {rgb(255, 230, 60), rgb(80, 220, 255), rgb(255, 255, 255)};
    box(cr, 59, -9 + k * 3.2, 4, 2, kBits[k % 3]);
  }
  cairo_restore(cr);
  // The tag.
  roundRect(cr, 64, 58, 30, 14, 2);
  setRgba(cr, rgb(240, 240, 230), 0.9);
  cairo_fill(cr);
  text(cr, label.c_str(), 79, 68.5, 9, rgb(30, 30, 40));
}

// A sandbag (128 x 128, its top at the top): a fat canvas sack tied off
// with hemp, a steel ring at the neck, stitched seams, stencilled `label`.
void paintBag(cairo_t* cr, const char* label, bool rope)
{
  if (rope)
  {
    cairo_move_to(cr, 64, 0);
    cairo_line_to(cr, 64, 22);
    stroke(cr, kInk, 5.0);
    cairo_move_to(cr, 64, 0);
    cairo_line_to(cr, 64, 22);
    stroke(cr, kRope, 3.0);
  }
  circle(cr, 64, 24, 7);
  stroke(cr, kInk, 4.0);
  circle(cr, 64, 24, 7);
  stroke(cr, rgb(170, 170, 180), 2.2);
  // The neck, gathered.
  cairo_move_to(cr, 52, 44);
  cairo_curve_to(cr, 54, 34, 58, 30, 64, 30);
  cairo_curve_to(cr, 70, 30, 74, 34, 76, 44);
  cairo_close_path(cr);
  setRgba(cr, darken(kCanvas, 0.1f), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.6);
  for (int k = 0; k < 3; ++k)
  {
    cairo_move_to(cr, 54, 38 + k * 2.4);
    cairo_line_to(cr, 74, 38 + k * 2.4);
  }
  stroke(cr, kRope, 2.0);
  // The sack.
  cairo_move_to(cr, 50, 42);
  cairo_curve_to(cr, 18, 52, 10, 90, 18, 118);
  cairo_curve_to(cr, 40, 128, 88, 128, 110, 118);
  cairo_curve_to(cr, 118, 90, 110, 52, 78, 42);
  cairo_close_path(cr);
  cairo_pattern_t* g = cairo_pattern_create_linear(18, 42, 110, 126);
  stop(g, 0.0, lighten(kCanvas, 0.2f));
  stop(g, 0.5, kCanvas);
  stop(g, 1.0, darken(kCanvas, 0.45f));
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  stroke(cr, kInk, 2.2);
  // Seams and stitching, a patch, the stencil.
  cairo_move_to(cr, 64, 46);
  cairo_curve_to(cr, 60, 80, 62, 104, 64, 124);
  stroke(cr, darken(kCanvas, 0.35f), 1.4);
  for (double y = 52; y < 120; y += 7)
  {
    cairo_move_to(cr, 60, y);
    cairo_line_to(cr, 66, y + 2);
  }
  stroke(cr, darken(kCanvas, 0.5f), 0.8, 0.8);
  text(cr, label, 64, 96, 22, rgb(40, 30, 20), 0.75);
}

// A towel (144 x 56): striped, a fringe, DON'T PANIC on it; `used`
// rumpled and grey.
void paintTowel(cairo_t* cr, bool used)
{
  const Color base = used ? rgb(150, 150, 160) : rgb(250, 250, 255);
  cairo_move_to(cr, 6, 12);
  cairo_curve_to(cr, 40, 4, 100, 18, 138, 8);
  cairo_line_to(cr, 140, 46);
  cairo_curve_to(cr, 100, 54, 40, 42, 4, 50);
  cairo_close_path(cr);
  vgrad(cr, 4, 54, {{0.0, base}, {1.0, darken(base, 0.25f)}}, 1.0, true);
  stroke(cr, kInk, 1.8);
  for (const double x : {22.0, 30.0, 114.0, 122.0})
  {
    cairo_move_to(cr, x, 10 - (x > 60 ? -2 : 0));
    cairo_line_to(cr, x, 48);
    stroke(cr, used ? rgb(110, 110, 130) : rgb(60, 100, 220), 4.0, 0.9);
  }
  for (double x = 8; x < 140; x += 5)
  {
    cairo_move_to(cr, x, 49);
    cairo_line_to(cr, x - 1, 55);
  }
  stroke(cr, base, 1.4);
  text(cr, "DON'T", 72, 26, 11, used ? rgb(80, 80, 90) : rgb(40, 60, 160));
  text(cr, "PANIC", 72, 40, 11, used ? rgb(80, 80, 90) : rgb(40, 60, 160));
}

// Lance's smoke machine (128 x 72, its feet at the bottom): a black box
// with a fog nozzle, a remote on a coiled lead, LANCE on a gold sticker.
void paintSmokeMachine(cairo_t* cr)
{
  roundRect(cr, 14, 24, 92, 44, 5);
  vgrad(cr, 24, 68, {{0.0, rgb(70, 70, 80)}, {0.25, rgb(36, 36, 42)}, {1.0, rgb(16, 16, 20)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  for (int k = 0; k < 5; ++k)
    box(cr, 22 + k * 8, 34, 4, 24, rgb(20, 20, 24));
  roundRect(cr, 100, 38, 22, 14, 3);
  vgrad(cr, 38, 52, {{0.0, rgb(150, 150, 160)}, {1.0, rgb(60, 60, 70)}}, 1.0, true);
  stroke(cr, kInk, 1.2);
  ellipse(cr, 122, 45, 3, 6);
  setRgba(cr, rgb(20, 20, 24), 1.0);
  cairo_fill(cr);
  roundRect(cr, 64, 30, 34, 12, 2);
  setRgba(cr, rgb(240, 200, 70), 1.0);
  cairo_fill(cr);
  text(cr, "LANCE", 81, 39.5, 8.5, rgb(60, 30, 10));
  box(cr, 66, 48, 8, 4, rgb(255, 60, 40));
  for (const double x : {22.0, 96.0})
    box(cr, x, 66, 10, 6, rgb(20, 20, 24));
  // The remote on its coil.
  cairo_move_to(cr, 16, 60);
  for (int k = 0; k < 6; ++k)
    cairo_curve_to(cr, 10 - k * 1.5, 62 + k * 1.5, 8 - k * 1.5, 66 + k * 1.2, 12 - k * 1.5, 64 + k * 1.4);
  stroke(cr, kInk, 1.6);
  roundRect(cr, 0, 62, 10, 10, 2);
  setRgba(cr, rgb(30, 30, 36), 1.0);
  cairo_fill(cr);
}

// A Trigger Happy disc (64 x 128, on its stand): a round target in red and
// white rings clamped to a short pole, a scaffold clamp at its foot.
void paintDisc(cairo_t* cr)
{
  box(cr, 29, 60, 6, 64, rgb(150, 150, 160));
  box(cr, 29, 60, 2, 64, rgb(220, 220, 230));
  roundRect(cr, 20, 112, 24, 12, 3);
  vgrad(cr, 112, 124, {{0.0, rgb(180, 180, 190)}, {1.0, rgb(80, 80, 90)}}, 1.0, true);
  stroke(cr, kInk, 1.2);
  for (int k = 5; k >= 1; --k)
  {
    circle(cr, 32, 34, k * 5.6);
    setRgba(cr, k % 2 ? rgb(230, 40, 40) : rgb(250, 250, 245), 1.0);
    cairo_fill(cr);
  }
  circle(cr, 32, 34, 28);
  stroke(cr, kInk, 2.0);
  circle(cr, 32, 34, 3);
  setRgba(cr, rgb(255, 230, 80), 1.0);
  cairo_fill(cr);
  cairo_arc(cr, 32, 34, 24, kPi * 1.1, kPi * 1.5);
  stroke(cr, rgb(255, 255, 255), 2.0, 0.6);
}

// A followspot's stand (128 x 100, its feet at the bottom, the yoke's
// pivot at (64, 18)): a heavy tripod on casters, a column, the yoke.
void paintSpotStand(cairo_t* cr)
{
  cairo_move_to(cr, 16, 94);
  cairo_line_to(cr, 64, 58);
  cairo_line_to(cr, 112, 94);
  cairo_move_to(cr, 64, 58);
  cairo_line_to(cr, 64, 96);
  stroke(cr, kInk, 9.0);
  cairo_move_to(cr, 16, 94);
  cairo_line_to(cr, 64, 58);
  cairo_line_to(cr, 112, 94);
  cairo_move_to(cr, 64, 58);
  cairo_line_to(cr, 64, 96);
  stroke(cr, rgb(60, 60, 70), 5.0);
  for (const double x : {16.0, 64.0, 112.0})
  {
    circle(cr, x, 94, 5);
    setRgba(cr, rgb(20, 20, 24), 1.0);
    cairo_fill(cr);
  }
  cairo_rectangle(cr, 56, 24, 16, 38);
  hgrad(cr, 56, 72, {{0.0, rgb(30, 30, 36)}, {0.4, rgb(80, 80, 90)}, {1.0, rgb(24, 24, 28)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  // The yoke.
  cairo_move_to(cr, 40, 8);
  cairo_line_to(cr, 40, 26);
  cairo_line_to(cr, 88, 26);
  cairo_line_to(cr, 88, 8);
  stroke(cr, kInk, 8.0);
  cairo_move_to(cr, 40, 8);
  cairo_line_to(cr, 40, 26);
  cairo_line_to(cr, 88, 26);
  cairo_line_to(cr, 88, 8);
  stroke(cr, rgb(70, 70, 80), 4.5);
}

// A followspot's lamp house (168 x 64, pointing right, its trunnion at
// (60, 32)): a long black barrel, the iris lever and the colour
// boomerang on top, the handle at the back, the lens at the front:
// `state` 0 dark, 1 lit white, 2 lit amber (locked), 3 smashed.
void paintSpotBarrel(cairo_t* cr, int state)
{
  // The handle at the back.
  cairo_move_to(cr, 14, 32);
  cairo_line_to(cr, 0, 46);
  stroke(cr, kInk, 6.0);
  cairo_move_to(cr, 14, 32);
  cairo_line_to(cr, 0, 46);
  stroke(cr, rgb(90, 90, 100), 3.0);
  // The body: a long barrel, fatter at the lamp house.
  cairo_move_to(cr, 12, 12);
  cairo_line_to(cr, 70, 14);
  cairo_line_to(cr, 150, 18);
  cairo_line_to(cr, 150, 46);
  cairo_line_to(cr, 70, 50);
  cairo_line_to(cr, 12, 52);
  cairo_close_path(cr);
  vgrad(cr, 12, 52, {{0.0, rgb(96, 96, 108)}, {0.3, rgb(44, 44, 52)}, {1.0, rgb(14, 14, 18)}}, 1.0, true);
  stroke(cr, kInk, 1.8);
  for (int k = 0; k < 5; ++k)
    box(cr, 18 + k * 7, 16, 3, 32, rgb(20, 20, 24));
  // The colour boomerang and the iris lever.
  box(cr, 96, 10, 26, 8, rgb(40, 40, 48));
  for (int k = 0; k < 4; ++k)
  {
    static const Color kGels[] = {rgb(255, 170, 60), rgb(230, 70, 170), rgb(80, 190, 255), rgb(255, 255, 255)};
    box(cr, 98 + k * 6, 4, 4, 8, kGels[k]);
  }
  cairo_move_to(cr, 80, 46);
  cairo_line_to(cr, 84, 58);
  stroke(cr, kInk, 4.0);
  // The trunnion.
  circle(cr, 60, 32, 7);
  setRgba(cr, rgb(70, 70, 80), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.4);
  // The lens.
  ellipse(cr, 152, 32, 9, 16);
  setRgba(cr, rgb(24, 24, 28), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.6);
  if (state == 3)
  {
    ellipse(cr, 153, 32, 6, 12);
    setRgba(cr, rgb(10, 10, 12), 1.0);
    cairo_fill(cr);
    cairo_move_to(cr, 148, 22);
    cairo_line_to(cr, 155, 32);
    cairo_line_to(cr, 150, 42);
    cairo_move_to(cr, 155, 32);
    cairo_line_to(cr, 158, 26);
    stroke(cr, rgb(170, 170, 180), 1.0, 0.8);
  }
  else
  {
    const Color c = state == 2 ? rgb(255, 190, 80) : state == 1 ? rgb(255, 255, 240) : rgb(50, 54, 64);
    ellipse(cr, 153, 32, 6, 12);
    setRgba(cr, c, 1.0);
    cairo_fill(cr);
    if (state != 0)
      softDot(cr, 156, 32, 14, 22, c, 0.8);
  }
}

// --- Painters: the scaffold (Trigger Happy) --------------------------------------------------

// A scaffold pole (24 x 256, tiles top to bottom): galvanised tube with a
// coupler now and then.
void paintPole(cairo_t* cr)
{
  cairo_rectangle(cr, 6, 0, 12, 256);
  hgrad(cr, 6, 18, {{0.0, rgb(110, 112, 120)}, {0.35, rgb(200, 202, 210)}, {1.0, rgb(60, 62, 70)}});
  for (const double y : {40.0, 168.0})
  {
    roundRect(cr, 2, y, 20, 14, 3);
    vgrad(cr, y, y + 14, {{0.0, rgb(190, 190, 196)}, {1.0, rgb(80, 80, 88)}}, 1.0, true);
    stroke(cr, kInk, 1.0);
  }
}

// --- Draw helpers ----------------------------------------------------------------------------

const FtActor* actorOf(const World& w, const Enemy& e)
{
  const int i = int(&e - w.enemies().data());
  for (const auto& a : w.flyTower().actors)
    if (a.enemy == i)
      return &a;
  return nullptr;
}

bool crew(const Enemy& e)
{
  return e.kind == EnemyKind::Followspot || e.kind == EnemyKind::Rigger || e.kind == EnemyKind::SandbagDrop;
}

// A flame column from the screen rect's top (top) down to its foot (foot),
// x the left edge, w wide: the body scrolling up, the tip, the glow.
void flameColumn(Renderer& r, const Art& art, float x, float top, float foot, float w, int frame, float strength, int seed)
{
  if (foot - top < 4.0f)
    return;
  const float tipH = std::min(192.0f, (foot - top) * 0.5f);
  const Texture& body = baked(art, r, "flamebody" + std::to_string((frame / 3 + seed) % 4), 128, 256, 0.0f, 0.0f,
    [k = (frame / 3 + seed) % 4](cairo_t* cr) { paintFlameBody(cr, k); });
  const Texture& body2 = baked(art, r, "flamebody" + std::to_string((frame / 3 + seed + 2) % 4), 128, 256, 0.0f, 0.0f,
    [k = (frame / 3 + seed + 2) % 4](cairo_t* cr) { paintFlameBody(cr, k); });
  const float sx = w / 128.0f;
  {
    Clip clip(r, x - 8.0f, top + tipH * 0.6f, w + 16.0f, foot - top - tipH * 0.6f);
    DrawOpts o;
    o.blend = Blend::Add;
    o.alpha = strength;
    o.scale = sx;
    const float scroll = std::fmod(float(frame) * 14.0f, 256.0f * sx);
    for (float y = foot - 256.0f * sx - scroll + 256.0f * sx; y > top - 256.0f * sx; y -= 256.0f * sx)
      r.draw(body, x, y, o);
    o.alpha = strength * 0.7f;
    const float scroll2 = std::fmod(float(frame) * 9.0f + 90.0f, 256.0f * sx);
    for (float y = foot - scroll2 + 256.0f * sx; y > top - 256.0f * sx; y -= 256.0f * sx)
      r.draw(body2, x, y - 256.0f * sx, o);
  }
  const int tk = (frame / 2 + seed) % 4;
  const Texture& tip = baked(art, r, "flametip" + std::to_string(tk), 128, 192, 0.0f, 192.0f, [tk](cairo_t* cr) { paintFlameTip(cr, tk); });
  blit(r, tip, x - w * 0.1f, top - 30.0f, w * 1.2f, tipH + 30.0f, 0.0f, 0.0f, 0.0f, strength, Blend::Add);
  // Light thrown round it.
  r.fillRect(x - 60.0f, top, w + 120.0f, foot - top, rgba(255, 110, 30, int(26 * strength)), Blend::Add);
  drawGlow(r, art, x + w * 0.5f, foot - 20.0f, 120.0f, kFire, 0.6f * strength);
}

} // namespace

// --- The set -----------------------------------------------------------------------------------

void drawFlyTowerSet(Renderer& r, const World& w, const Art& art, float camX, float camY, int frame, float alpha)
{
  (void)alpha;
  const auto& s = w.flyTower();
  const auto& map = w.map();
  const int bw = map.widthBlocks(), bh = map.heightBlocks();
  const float levelH = float(bh) * kTilePx;
  if (s.trigger)
  {
    // Trigger Happy: the scaffold's poles from the floor to the top, its
    // ledger tubes under each floor and the braces between.
    const Texture& pole = baked(art, r, "pole", 24, 256, 12.0f, 0.0f, paintPole);
    for (int bx = 2; bx < bw - 1; bx += 6)
    {
      const float x = float(bx) * kTilePx - camX;
      if (x < -40.0f || x > float(kScreenW) + 40.0f)
        continue;
      tile(r, pole, x - 12.0f, 64.0f - camY, x + 12.0f, levelH - 64.0f - camY, x - 12.0f, -camY);
    }
    for (int by = 1; by < bh - 1; ++by)
    {
      bool any = false;
      for (int bx = 0; bx < bw && !any; ++bx)
        any = map.block(bx, by) == Tile::Platform;
      if (!any)
        continue;
      const float y = float(by) * kTilePx - camY + 22.0f;
      r.fillRect(64.0f - camX, y, float(bw - 2) * kTilePx, 10.0f, rgb(120, 122, 130));
      r.fillRect(64.0f - camX, y, float(bw - 2) * kTilePx, 3.0f, rgb(210, 212, 220));
      for (int bx = 2; bx + 6 < bw; bx += 6)
      {
        const float x0 = float(bx) * kTilePx - camX, x1 = float(bx + 6) * kTilePx - camX;
        r.drawLine(x0, y + 8.0f, x1, y + 8.0f + 6.0f * kTilePx * 0.5f, 5.0f, rgba(150, 152, 160, 150));
      }
    }
    return;
  }

  // The back wall, deep in the haze.
  const Texture& wall = baked(art, r, "wall", kWallTex, kWallTex, 0.0f, 0.0f, paintWall);
  tile(r, wall, 0.0f, 0.0f, float(kScreenW), float(kScreenH), -camX * 0.4f, -camY * 0.4f);
  // The air: cold and dark up in the grid, warm with the show's light near
  // the stage.
  {
    const float warm = std::clamp((camY + float(kScreenH) - (levelH - 2600.0f)) / 2600.0f, 0.0f, 1.0f);
    r.fillRect(0, 0, float(kScreenW), float(kScreenH), lerpColor(rgba(14, 16, 34, 110), rgba(70, 30, 30, 70), warm));
  }
  // Moving-head beams sweeping up through the haze out of the show.
  {
    const Texture& beam = baked(art, r, "beam", 128, 1024, 64.0f, 0.0f, paintBeam);
    static const Color kBeamCols[] = {kAmber, kMagenta, kCyan, kAmber, kMagenta};
    for (int k = 0; k < 5; ++k)
    {
      const float wx = 500.0f + float(k) * 1050.0f;
      const float baseY = levelH - 200.0f - float(k % 3) * 1700.0f;
      const float sx = wx - camX * 0.85f, sy = baseY - camY * 0.85f - (levelH * 0.15f - 600.0f);
      const float ang = 180.0f + 24.0f * std::sin(float(frame) * 0.006f + float(k) * 1.7f) + (k % 2 ? 10.0f : -10.0f);
      if (sy < -400.0f || sy > float(kScreenH) + 2600.0f)
        continue;
      blit(r, beam, sx - 140.0f, sy, 280.0f, 2600.0f, ang, 140.0f, 0.0f, 0.13f, Blend::Add, kBeamCols[k]);
    }
  }
  // Battens hanging in the haze.
  {
    const Texture& battens = baked(art, r, "battens", kBattenW, kBattenH, 0.0f, 0.0f, paintBattens);
    DrawOpts o;
    o.alpha = 0.6f;
    o.tint = rgb(170, 160, 190);
    const float top = std::max(0.0f, 900.0f - camY * 0.7f);
    tile(r, battens, 0.0f, top, float(kScreenW), float(kScreenH), -camX * 0.7f, -camY * 0.7f + 40.0f, o);
  }

  // The roof over the grid: its trusses, the smoke vent, the lines down.
  {
    const Texture& roof = baked(art, r, "roof", 1024, 300, 0.0f, 0.0f, paintRoofTruss);
    const float y = -camY - 40.0f;
    if (y + 300.0f > 0.0f)
      for (float x = -wrapFrom(camX, 1024.0f, 0.0f); x < float(kScreenW); x += 1024.0f)
        r.draw(roof, x, y);
  }
  // The scorched steel behind the pyro wall (x 44-75, rows 21-51).
  {
    const float x0 = 44.0f * kTilePx - camX, x1 = 76.0f * kTilePx - camX;
    const float y0 = 21.0f * kTilePx - camY, y1 = 52.0f * kTilePx - camY;
    if (!offScreen(x0, y0, x1 - x0, y1 - y0, 0.0f))
    {
      const Texture& steel = baked(art, r, "pyroback", 256, 256, 0.0f, 0.0f, paintPyroBack);
      DrawOpts o;
      o.alpha = 0.85f;
      tile(r, steel, x0, y0, x1, y1, x0, y0, o);
      for (int k = 0; k < 4; ++k)
      {
        const float sx = x0 + float(k) * 8.0f * kTilePx + 4.0f * kTilePx;
        r.drawText("DANGER - PYRO", sx, y1 - 220.0f, {22.0f, rgba(255, 200, 60, 150), 0}, Align::Center);
      }
      // The shaft's walls: steel uprights at its edges.
      r.fillRect(x0 - 14.0f, y0, 14.0f, y1 - y0, rgb(30, 30, 36));
      r.fillRect(x1, y0, 14.0f, y1 - y0, rgb(30, 30, 36));
      r.fillRect(x0 - 14.0f, y0, 4.0f, y1 - y0, rgb(90, 90, 100));
    }
  }

  // The wings at the foot of the tower: the lit cyc beyond, black legs, the
  // prompt desk by the start.
  {
    const float stageY = float(bh - 2) * kTilePx - camY; // the stage floor's top
    if (stageY > -100.0f && stageY - 1100.0f < float(kScreenH))
    {
      // The cyc's glow beyond the legs.
      r.fillRect(0.0f, stageY - 700.0f, float(kScreenW), 700.0f, rgba(60, 30, 90, 70), Blend::Add);
      drawGlow(r, art, float(kScreenW) * 0.5f - (camX - float(bw) * kTilePx * 0.5f) * 0.6f, stageY - 120.0f, 128.0f * 5.0f,
        rgb(120, 60, 200), 0.35f);
      const Texture& leg = baked(art, r, "leg", 240, 900, 0.0f, 900.0f, paintLeg);
      for (int k = 0; k < 6; ++k)
      {
        const float wx = 300.0f + float(k) * 900.0f;
        const float x = wx - camX * 0.9f + float(kScreenW) * 0.05f;
        if (x < -260.0f || x > float(kScreenW) + 20.0f)
          continue;
        DrawOpts o;
        o.alpha = 0.92f;
        r.draw(leg, x, stageY - (camY - (levelH - float(kScreenH))) * 0.1f, o);
      }
      const Texture& desk = baked(art, r, "promptdesk", 240, 200, 0.0f, 200.0f, paintPromptDesk);
      r.draw(desk, 6.0f * kTilePx - camX, stageY);
      // The SM's standby light blinking on the desk with the cue list.
      int next = 1 << 20;
      for (const auto& c : s.cues)
        if (c.list == 0)
        {
          const int in = w.cueIn(c);
          if (in >= 0)
            next = std::min(next, in);
        }
      const float lx = 6.0f * kTilePx - camX;
      if (next <= 45 && (frame / 8) % 2 == 0)
        drawGlow(r, art, lx + 49.0f, stageY - 86.0f, 24.0f, kRed, 0.9f);
      if (next <= 2)
        drawGlow(r, art, lx + 78.0f, stageY - 86.0f, 24.0f, rgb(60, 255, 120), 1.0f);
    }
  }

  // Keyed to the map: the loft blocks on the grid deck, the catwalks'
  // hanger rods and handrails.
  const int bx0 = std::max(0, int(camX / kTilePx) - 2), bx1 = std::min(bw - 1, int((camX + float(kScreenW)) / kTilePx) + 2);
  const int by0 = std::max(0, int(camY / kTilePx) - 2), by1 = std::min(bh - 1, int((camY + float(kScreenH)) / kTilePx) + 14);
  const Texture& loft = baked(art, r, "loftblock", 64, 56, 32.0f, 56.0f, paintLoftBlock);
  for (int by = by0; by <= by1; ++by)
  {
    int bx = std::max(0, bx0 - 30);
    while (bx <= bx1)
    {
      const Tile t = map.block(bx, by);
      if (t != Tile::Platform && t != Tile::Solid)
      {
        ++bx;
        continue;
      }
      // A run of the same kind along the row, with open air above it.
      int end = bx;
      while (end + 1 < bw && map.block(end + 1, by) == t)
        ++end;
      const bool open = by > 0 && map.block(bx, by - 1) == Tile::Empty && map.block(end, by - 1) == Tile::Empty;
      const int len = end - bx + 1;
      const float x0 = float(bx) * kTilePx - camX, x1 = float(end + 1) * kTilePx - camX, y = float(by) * kTilePx - camY;
      if (x1 > -64.0f && x0 < float(kScreenW) + 64.0f && open)
      {
        if (t == Tile::Solid && by <= 13 && len >= 10)
        {
          // The grid deck: loft blocks on it every few blocks, their lines
          // running up to the roof's head blocks.
          for (int k = bx + 3; k <= end - 1; k += 7)
          {
            const float lx = float(k) * kTilePx + 32.0f - camX;
            r.drawLine(lx - 10.0f, y - 30.0f, lx - 10.0f, -camY + 260.0f, 2.0f, rgba(120, 120, 130, 160));
            r.drawLine(lx + 10.0f, y - 30.0f, lx + 10.0f, -camY + 260.0f, 2.0f, rgba(120, 120, 130, 160));
            r.draw(loft, lx, y + 2.0f);
          }
        }
        else if (t == Tile::Platform)
        {
          // Hanger rods up from the catwalk, out of sight into the flies.
          const float rodTop = std::max(-10.0f, y - 1400.0f);
          for (int k = bx; k <= end + 1; k += (len >= 6 ? 6 : std::max(1, len)))
          {
            const float rx = float(std::min(k, end + 1)) * kTilePx - camX + (k > end ? -6.0f : 6.0f);
            if (rx < -20.0f || rx > float(kScreenW) + 20.0f)
              continue;
            r.drawLine(rx, rodTop, rx, y + 20.0f, 3.0f, rgba(70, 70, 80, 220));
            r.fillRect(rx - 3.0f, y - 90.0f, 6.0f, 14.0f, rgb(90, 90, 100)); // a turnbuckle
          }
          if (len >= 6)
          {
            // Handrails: posts every two blocks, a top rail and a mid rail.
            for (int k = bx; k <= end + 1; k += 2)
            {
              const float px = float(std::min(k, end + 1)) * kTilePx - camX + (k > end ? -4.0f : 2.0f);
              r.fillRect(px, y - 74.0f, 5.0f, 74.0f, rgb(40, 40, 48));
              r.fillRect(px, y - 74.0f, 2.0f, 74.0f, rgb(100, 100, 112));
            }
            r.fillRect(x0, y - 76.0f, x1 - x0, 6.0f, rgb(48, 48, 56));
            r.fillRect(x0, y - 76.0f, x1 - x0, 2.0f, rgb(140, 136, 140));
            r.fillRect(x0, y - 40.0f, x1 - x0, 4.0f, rgb(40, 40, 48));
            r.fillRect(x0, y - 40.0f, x1 - x0, 1.0f, rgb(110, 106, 110));
          }
        }
      }
      bx = end + 1;
    }
  }
}

// --- Behind the runner ---------------------------------------------------------------------------

void World::drawFlyTowerBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& s = mFly;
  // Pyro nozzles (and the Q99 cannon with its fuse box), glowing before
  // they burn.
  const Texture& nozzle = baked(mArt, r, "nozzle", 128, 44, 0.0f, 0.0f, paintNozzle);
  for (const auto& py : s.pyros)
  {
    const int heat = pyroHeat(py, kFtGlow);
    const float nx = float(py.x) * kTilePx - camX, ny = (float(py.y) + 0.5f) * kTilePx - camY;
    if (offScreen(nx - 200.0f, ny - 200.0f, 400.0f + float(py.w) * kTilePx, 400.0f))
    {
      if (heat != 2)
        continue;
    }
    if (py.dir == 0)
    {
      const float w = float(py.w) * kTilePx;
      r.draw(nozzle, nx + (w - 128.0f) * 0.5f, ny - 12.0f);
      // The ARMED lamp.
      const bool armed = heat >= 1 || (frame / 20) % 2 == 0;
      r.fillRect(nx + (w - 128.0f) * 0.5f + 100.0f, ny + 4.0f, 8.0f, 6.0f, armed ? kRed : rgb(70, 16, 16));
      if (heat == 1)
      {
        // The slot glows white-hot, pilot flames lick and sparks spit.
        const float hot = pyroHeat(py, 5) == 1 ? 1.0f : (pyroHeat(py, 10) == 1 ? 0.75f : 0.45f);
        const float cx = nx + w * 0.5f, cy = ny - 8.0f;
        drawGlow(r, mArt, cx, cy, 70.0f + 30.0f * hot, kFire, 0.5f + 0.4f * hot * float((frame / 2) % 2));
        r.fillRect(cx - 28.0f, cy - 4.0f, 56.0f, 4.0f, withAlpha(kFireHot, int(140 + 100 * hot)), Blend::Add);
        for (int k = 0; k < 8; ++k)
        {
          const std::uint32_t hh = hash2(k + py.x * 7, frame / 2);
          const float ph = float((frame + k * 5) % 16) / 16.0f;
          const float sx = cx + (unit(hh) - 0.5f) * 60.0f, sy = cy - ph * (60.0f + 70.0f * hot);
          r.fillRect(sx, sy, 3.0f, 3.0f, withAlpha(k % 2 ? kFireHot : kFire, int(255 * (1.0f - ph))), Blend::Add);
        }
        // Pilot flames.
        for (int k = 0; k < 3; ++k)
        {
          const float fx = cx - 20.0f + float(k) * 20.0f, fh = 12.0f + 10.0f * hot + float((frame + k) % 3) * 4.0f;
          r.fillRect(fx - 3.0f, cy - fh, 6.0f, fh, withAlpha(rgb(120, 170, 255), 120), Blend::Add);
        }
      }
    }
    else
    {
      // The Q99 cannon, on C5, aimed along it; its fuse box behind.
      const float bx = (float(py.x) + 0.5f) * kTilePx - camX, foot = (float(py.y) + 1.0f) * kTilePx - camY;
      const Texture& cannon = baked(mArt, r, "q99cannon", 112, 80, 8.0f, 80.0f, paintCannon);
      DrawOpts co;
      if (py.spent)
        co.tint = rgb(120, 100, 90);
      const bool shake = py.fuseT >= 0 && py.fuseT < kFtGlow;
      r.draw(cannon, bx - 40.0f + (shake ? float((frame / 2) % 2) * 3.0f : 0.0f), foot, co);
      const CellBox fb = py.fuseBox();
      const float fx = float(fb.x) * kCellPx - camX, fy = float(fb.y) * kCellPx - camY;
      const Texture& fuse = baked(mArt, r, "fusebox", 48, 76, 24.0f, 76.0f, paintFuseBox);
      r.draw(fuse, fx + 32.0f, fy + float(fb.h) * kCellPx);
      // Its breakers: lit while there are hits left to take.
      for (int k = 0; k < 2; ++k)
      {
        const bool on = k < py.fuse;
        r.fillRect(fx + 32.0f - 10.0f + float(k) * 12.0f, fy + float(fb.h) * kCellPx - 60.0f, 8.0f, 6.0f,
          on ? rgb(80, 255, 120) : rgb(255, 60, 40));
      }
      if (py.fuseT >= 0 && py.fuseT < kFtGlow)
      {
        drawGlow(r, mArt, bx - 40.0f, foot - 46.0f, 60.0f, kFire, 0.5f + 0.4f * float((frame / 2) % 2));
        for (int k = 0; k < 5; ++k)
        {
          const float ph = float((frame + k * 7) % 12) / 12.0f;
          r.fillRect(fx + 30.0f + (unit(hash2(k, frame / 3)) - 0.5f) * 30.0f, fy + 30.0f - ph * 50.0f, 3.0f, 3.0f,
            withAlpha(kFireHot, int(255 * (1.0f - ph))), Blend::Add);
        }
      }
      if (py.spent)
        for (int k = 0; k < 4; ++k)
        {
          const float ph = float((frame + k * 30) % 120) / 120.0f;
          drawGlow(r, mArt, bx - 40.0f + ph * 20.0f, foot - 50.0f - ph * 120.0f, 30.0f + ph * 40.0f, rgb(90, 90, 100), 0.25f * (1.0f - ph));
        }
    }
  }

  // The trusses: chase lights run along before a move, red while one waits.
  for (std::size_t ti = 0; ti < s.trusses.size(); ++ti)
  {
    const auto& t = s.trusses[ti];
    const auto& pl = mPlatforms[std::size_t(t.platform)];
    const float x = lerpCells(pl.prevX, pl.x, alpha) - camX, y = lerpCells(pl.prevY, pl.y, alpha) - camY;
    const float wpx = float(pl.w) * kCellPx;
    if (offScreen(x, y - 2000.0f, wpx, 2200.0f))
      continue;
    // The lift lines up to the grid.
    const float gridY = 14.0f * kTilePx - camY;
    for (const float lx : {x + 20.0f, x + wpx - 20.0f})
    {
      r.drawLine(lx, std::max(-10.0f, gridY), lx, y + 10.0f, 2.4f, rgb(150, 150, 160));
      r.drawLine(lx + 1.0f, std::max(-10.0f, gridY), lx + 1.0f, y + 10.0f, 1.0f, rgb(230, 230, 240));
    }
    r.draw(baked(mArt, r, "truss" + std::to_string(pl.w), int(wpx), 112, 0.0f, 0.0f, [wc = pl.w](cairo_t* cr) { paintTruss(cr, wc); }),
      x, y);
    bool chase = false;
    for (const auto& c : s.cues)
      if (c.kind == FtCueKind::Truss && std::find(c.targets.begin(), c.targets.end(), int(ti)) != c.targets.end())
      {
        const int in = cueIn(c);
        chase = chase || (in > 0 && in <= kFtChase);
      }
    const bool moving = pl.y != t.stops[std::size_t(t.goal)];
    chase = chase || moving;
    const int bulbs = int(wpx / 32.0f);
    for (int k = 0; k < bulbs; ++k)
    {
      const float bx = x + 16.0f + float(k) * 32.0f, by = y + 37.0f;
      const bool lit = t.waiting ? (frame / 6) % 2 == 0 : chase ? (k + frame / 3) % 3 == 0 : false;
      const Color c = t.waiting ? kRed : kAmber;
      r.fillRect(bx - 4.0f, by - 4.0f, 8.0f, 8.0f, lit ? lighten(c, 0.4f) : rgb(70, 50, 30));
      if (lit)
        drawGlow(r, mArt, bx, by, 22.0f, c, 0.9f);
    }
    // The lamps under it shine down.
    for (float lx = 40.0f; lx < wpx - 20.0f; lx += 96.0f)
      drawGlow(r, mArt, x + lx, y + 112.0f, 40.0f, rgb(255, 210, 160), 0.35f);
  }

  // Confetti cannons: they tilt and rattle before a blast.
  for (std::size_t ci = 0; ci < s.confetti.size(); ++ci)
  {
    const auto& cf = s.confetti[ci];
    int rattle = 0;
    for (const auto& c : s.cues)
      if (c.kind == FtCueKind::Confetti && std::find(c.targets.begin(), c.targets.end(), int(ci)) != c.targets.end())
      {
        const int in = cueIn(c);
        if (in > 0 && in <= kFtConfettiTell)
          rattle = (frame / 2) % 2 ? 1 : -1;
      }
    const float x = (float(cf.x) + 1.0f) * kTilePx - camX + float(rattle) * 3.0f;
    const float foot = (float(cf.y) + 1.0f) * kTilePx - camY;
    if (offScreen(x - 80.0f, foot - 100.0f, 160.0f, 100.0f))
      continue;
    const Sprite& sp = bakedSprite(mArt, r, "confetti_" + cf.id, 128, 80, 64.0f, 80.0f,
      [id = cf.id](cairo_t* cr) { paintConfettiCannon(cr, id); });
    DrawOpts o;
    o.angle = float(rattle) * 2.0f;
    r.draw(sp.get(cf.dir), x, foot, o);
    if (rattle != 0)
      for (int k = 0; k < 3; ++k)
      {
        const float ph = float((frame + k * 4) % 12) / 12.0f;
        drawGlow(r, mArt, x + float(cf.dir) * (60.0f + ph * 30.0f), foot - 50.0f - ph * 20.0f, 14.0f + ph * 16.0f,
          rgb(230, 240, 255), 0.4f * (1.0f - ph));
      }
  }

  // The torn ends of a catwalk a sandbag broke: grating bent down at each
  // side, sparks still dropping now and then.
  for (const auto& g : s.gaps)
  {
    const float x0 = float(g.bx) * kTilePx - camX, x1 = float(g.bx + 2) * kTilePx - camX, y = float(g.by) * kTilePx - camY;
    if (offScreen(x0, y - 20.0f, x1 - x0, 80.0f))
      continue;
    for (int k = 0; k < 4; ++k)
    {
      const float dx = float(k) * 7.0f;
      r.drawLine(x0 + dx, y + 2.0f, x0 + dx + 8.0f, y + 26.0f + float(k % 2) * 8.0f, 3.0f, rgb(90, 90, 100));
      r.drawLine(x1 - dx, y + 2.0f, x1 - dx - 8.0f, y + 24.0f + float(k % 2) * 10.0f, 3.0f, rgb(90, 90, 100));
    }
    if (g.t > kFtRehang - 40 && (frame / 3) % 2 == 0)
      for (int k = 0; k < 3; ++k)
      {
        const float ph = float((frame + k * 9) % 20) / 20.0f;
        r.fillRect(x0 + 8.0f + float(k) * 40.0f, y + 10.0f + ph * 90.0f, 3.0f, 3.0f, withAlpha(kFireHot, int(255 * (1.0f - ph))),
          Blend::Add);
      }
  }

  // The 42 LB sandbag, hanging on its line over L1.
  for (const auto& pr : mProps)
  {
    if (pr.kind != PropKind::Deco42)
      continue;
    const float cx = (float(pr.x) + float(pr.w) * 0.5f) * kCellPx - camX, y = float(pr.y) * kCellPx - camY;
    if (offScreen(cx - 40.0f, y - 400.0f, 80.0f, 500.0f))
      continue;
    const float sway = std::sin(float(frame) * 0.03f) * 4.0f;
    r.drawLine(cx, y - 800.0f, cx + sway, y - 10.0f, 3.0f, kRope);
    DrawOpts o;
    o.scale = 0.62f;
    o.angle = sway * 0.6f;
    r.draw(baked(mArt, r, "bag42", 128, 128, 64.0f, 0.0f, [](cairo_t* cr) { paintBag(cr, "42 LB", true); }), cx + sway, y - 14.0f, o);
  }
  // Lance's smoke machine on its perch, puffing haze.
  for (const auto& t : mSignal.treasures)
  {
    if (t.taken || t.art != "smoke_machine")
      continue;
    const float x = float(t.x) * kTilePx - camX, foot = float(t.y + 1) * kTilePx - camY;
    if (offScreen(x - 100.0f, foot - 200.0f, 260.0f, 220.0f))
      continue;
    for (int k = 0; k < 5; ++k)
    {
      const float ph = float((frame + k * 24) % 120) / 120.0f;
      drawGlow(r, mArt, x + 4.0f - ph * 90.0f, foot - 30.0f - ph * 60.0f, 20.0f + ph * 60.0f, rgb(200, 200, 220), 0.35f * (1.0f - ph));
    }
    drawGlow(r, mArt, x + 32.0f, foot - 30.0f, 60.0f, rgb(255, 210, 90), 0.35f + 0.1f * std::sin(float(frame) * 0.1f));
    DrawOpts o;
    o.scale = 0.6f;
    r.draw(baked(mArt, r, "smokemachine", 128, 72, 64.0f, 72.0f, paintSmokeMachine), x + 32.0f, foot, o);
  }
  // The towel (THE ANSWER): falling, it flutters; on the deck it lies
  // folded, grey once used.
  if (s.towelY >= 0)
  {
    const bool falling = s.towelY < s.towelLand;
    const float x = (float(s.towelX) + 1.0f) * kCellPx - camX, y = float(s.towelY) * kCellPx - camY;
    DrawOpts o;
    o.scale = falling ? 0.7f : 0.55f;
    if (falling)
      o.angle = std::sin(float(frame) * 0.3f) * 18.0f;
    r.draw(baked(mArt, r, s.towelUsed ? "towel_used" : "towel", 144, 56, 72.0f, 56.0f,
             [u = s.towelUsed](cairo_t* cr) { paintTowel(cr, u); }),
      x + (falling ? std::sin(float(frame) * 0.13f) * 20.0f : 0.0f), y + 2.0f, o);
    if (!s.towelUsed && !falling && (frame / 10) % 3 == 0)
      drawGlow(r, mArt, x, y - 12.0f, 40.0f, rgb(150, 200, 255), 0.4f);
  }
  // Trigger Happy's discs.
  const Texture& disc = baked(mArt, r, "disc", 64, 128, 0.0f, 0.0f, paintDisc);
  for (const auto& t : s.targets)
  {
    if (t.hit)
      continue;
    const CellBox b = t.box();
    const float x = float(b.x) * kCellPx - camX, y = float(b.y) * kCellPx - camY;
    if (offScreen(x, y, 64.0f, 128.0f))
      continue;
    DrawOpts o;
    o.angle = std::sin(float(frame) * 0.05f + float(t.x)) * 3.0f;
    r.draw(disc, x, y, o);
  }
}

// --- In front --------------------------------------------------------------------------------------

void World::drawFlyTowerFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& s = mFly;
  const auto& p = mPlayer;
  const float px = lerpCells(p.prevX, p.x, alpha) + 1.5f * kCellPx - camX;
  const float pfoot = lerpCells(p.prevY, p.y, alpha) + kCellPx - camY;

  // The fly crew.
  for (const auto& e : mEnemies)
  {
    if (e.hidden || !crew(e))
      continue;
    const float cx = (e.drawX + float(e.w) * 0.5f) * kCellPx - camX, foot = (e.drawY + 1.0f) * kCellPx - camY;
    if (offScreen(cx - 200.0f, foot - 400.0f, 400.0f, 420.0f))
      continue;
    const FtActor* a = actorOf(*this, e);
    const int dir = e.dir < 0 ? -1 : 1;
    if (e.kind == EnemyKind::Followspot)
    {
      // The stand; the barrel turned to where its beam lies; a dead one
      // stays dark and smoking.
      const Texture& stand = baked(mArt, r, "spotstand", 128, 100, 64.0f, 100.0f, paintSpotStand);
      r.draw(stand, cx, foot);
      const float pvx = cx, pvy = foot - 82.0f;
      const bool lamp = e.alive && a && a->lamp;
      const int state = !e.alive ? 3 : lamp ? (a->lock >= kFtSpotLock ? 2 : 1) : 0;
      float tx = a ? float(a->beamX) * kCellPx - camX : cx - 200.0f, ty = pfoot;
      if (!lamp)
        ty = foot + 40.0f;
      const float ang = std::atan2(ty - pvy, tx - pvx) * 180.0f / float(kPi);
      const Sprite& barrel = bakedSprite(mArt, r, "spotbarrel" + std::to_string(state), 168, 64, 60.0f, 32.0f,
        [state](cairo_t* cr) { paintSpotBarrel(cr, state); });
      DrawOpts o;
      o.angle = ang;
      o.cull = false;
      r.draw(barrel.facing[0], pvx, pvy, o);
      if (e.flash > 0)
      {
        DrawOpts fo = o;
        fo.blend = Blend::Add;
        fo.alpha = float(e.flash) / 8.0f;
        r.draw(barrel.facing[0], pvx, pvy, fo);
      }
      if (!e.alive)
        for (int k = 0; k < 3; ++k)
        {
          const float ph = float((frame + k * 40) % 120) / 120.0f;
          drawGlow(r, mArt, pvx + ph * 16.0f, pvy - 20.0f - ph * 110.0f, 16.0f + ph * 30.0f, rgb(110, 110, 120), 0.3f * (1.0f - ph));
        }
      continue;
    }
    if (!e.alive)
      continue;
    int variant = 0;
    if (e.kind == EnemyKind::Rigger)
      variant = a && a->aim > 0 ? 1 : 0;
    else
      variant = a && a->tell > 0 ? 1 : (e.cool > enemyDef(e.def).cooldown - 6 ? 2 : 0);
    const Sprite& sp = styledEnemySprite(mArt, r, mTheme, e.kind == EnemyKind::Rigger ? "rigger" : "sandbag", variant,
      (frame / 50) % 9 == 0 ? 1 : 0, e.w, e.h);
    if (e.kind == EnemyKind::SandbagDrop && variant != 2)
    {
      // His line, on up into the flies.
      const float lx = cx + float(dir) * (66.0f - 48.0f), top = foot - 160.0f;
      r.drawLine(lx, std::min(top, -10.0f), lx, top + 2.0f, 3.0f, kRope);
    }
    r.draw(sp.get(dir), cx, foot);
    if (e.flash > 0)
    {
      DrawOpts fo;
      fo.blend = Blend::Add;
      fo.alpha = float(e.flash) / 8.0f;
      r.draw(sp.get(dir), cx, foot, fo);
    }
    if (e.kind == EnemyKind::Rigger)
      drawGlow(r, mArt, cx + float(dir) * 18.0f, foot - 140.0f, 14.0f, kRed, 0.7f + 0.3f * float((frame / 6) % 2)); // the headlamp
  }

  // Followspot beams: from the lens to a pool of light on the runner's
  // floor, white, amber once it has them.
  const Texture& beam = baked(mArt, r, "beam", 128, 1024, 64.0f, 0.0f, paintBeam);
  const Texture& pool = baked(mArt, r, "pool", 256, 64, 128.0f, 32.0f, paintPool);
  for (const auto& a : s.actors)
  {
    const Enemy& e = mEnemies[std::size_t(a.enemy)];
    if (e.kind != EnemyKind::Followspot || !e.alive || !a.lamp)
      continue;
    const float cx = (e.drawX + float(e.w) * 0.5f) * kCellPx - camX, foot = (e.drawY + 1.0f) * kCellPx - camY;
    const float pvx = cx, pvy = foot - 82.0f;
    const float tx = float(a.beamX) * kCellPx - camX, ty = pfoot;
    const float dx = tx - pvx, dy = ty - pvy, d = std::sqrt(dx * dx + dy * dy);
    if (d < 40.0f)
      continue;
    const float lx = pvx + dx / d * 96.0f, ly = pvy + dy / d * 96.0f, len = d - 96.0f;
    const bool locked = a.lock >= kFtSpotLock;
    const Color c = locked ? rgb(255, 180, 70) : rgb(255, 250, 230);
    const float ang = std::atan2(-dx, dy) * 180.0f / float(kPi);
    const float spread = std::max(60.0f, len * 0.16f);
    blit(r, beam, lx - spread * 0.5f, ly, spread, len + 30.0f, ang, spread * 0.5f, 0.0f, locked ? 0.42f : 0.3f, Blend::Add, c);
    DrawOpts po;
    po.blend = Blend::Add;
    po.tint = c;
    po.alpha = locked ? 0.75f : 0.5f;
    po.scale = 0.9f;
    r.draw(pool, tx, ty - 4.0f, po);
    drawGlow(r, mArt, lx, ly, 26.0f, c, 0.8f);
  }

  // Riggers' laser sights, the whole aim long; brighter for the last
  // half-second, the beep.
  for (const auto& a : s.actors)
  {
    const Enemy& e = mEnemies[std::size_t(a.enemy)];
    if (e.kind != EnemyKind::Rigger || !e.alive || a.aim <= 0)
      continue;
    const int dir = e.dir < 0 ? -1 : 1;
    const float cx = (e.drawX + float(e.w) * 0.5f) * kCellPx - camX, foot = (e.drawY + 1.0f) * kCellPx - camY;
    const float fx = cx + float(dir) * 46.0f, fy = foot - 126.0f;
    const float ax = (float(a.aimX) + 0.5f) * kCellPx - camX, ay = (float(a.aimY) + 0.5f) * kCellPx - camY;
    const bool late = a.aim >= kFtRiggerAim - kFtRiggerBeep;
    const bool blink = late && (frame / 3) % 2 == 0;
    r.drawLine(fx, fy, ax, ay, late ? 5.0f : 3.0f, rgba(255, 30, 30, blink ? 120 : 60), Blend::Add);
    r.drawLine(fx, fy, ax, ay, 1.5f, rgba(255, 120, 110, blink ? 255 : 200), Blend::Add);
    drawGlow(r, mArt, ax, ay, late ? 24.0f : 16.0f, kRed, blink ? 1.0f : 0.7f);
    drawGlow(r, mArt, fx, fy, 12.0f, kRed, 0.8f);
  }

  // Sandbags: hanging on their lines while the shadow grows, then falling.
  const Texture& shadow = baked(mArt, r, "shadow", 128, 32, 64.0f, 16.0f, paintShadow);
  const Texture& bag = baked(mArt, r, "bag", 128, 128, 64.0f, 0.0f, [](cairo_t* cr) { paintBag(cr, "25 LB", false); });
  for (const auto& a : s.actors)
  {
    const Enemy& e = mEnemies[std::size_t(a.enemy)];
    if (e.kind != EnemyKind::SandbagDrop || !e.alive || a.tell <= 0)
      continue;
    const float bx = (float(a.bagX) + 2.0f) * kCellPx - camX, by = float(a.bagY) * kCellPx - camY;
    int floor = a.bagY + 4;
    while (floor < mMap.height() && !mMap.solidTop(a.bagX + 1, floor) && !mMap.solid(a.bagX + 1, floor))
      ++floor;
    const float u = float(a.tell) / float(kFtBagTell);
    DrawOpts so;
    so.scale = 0.5f + 0.8f * u;
    so.alpha = 0.3f + 0.5f * u;
    r.draw(shadow, bx, float(floor) * kCellPx - camY - 2.0f, so);
    const float sway = std::sin(float(frame) * 0.2f) * 3.0f * u;
    r.drawLine(bx, std::min(by - 20.0f, -10.0f), bx + sway, by + 4.0f, 3.0f, kRope);
    DrawOpts bo;
    bo.angle = sway;
    r.draw(bag, bx + sway, by, bo);
    if (u > 0.6f && (frame / 3) % 2 == 0)
      r.drawText("!", bx, by - 40.0f, {30.0f, rgb(255, 230, 120), kHudInk, true}, Align::Center);
  }
  for (const auto& b : s.bags)
  {
    const float bx = (float(b.x) + 2.0f) * kCellPx - camX, by = float(b.y) * kCellPx - camY;
    if (offScreen(bx - 64.0f, by, 128.0f, 128.0f))
      continue;
    // A shadow on the floor under it, the cut rope trailing.
    int floor = b.y + 4;
    while (floor < mMap.height() && floor < b.y + 60 && !mMap.solidTop(b.x + 1, floor) && !mMap.solid(b.x + 1, floor))
      ++floor;
    DrawOpts so;
    so.scale = 1.4f;
    so.alpha = 0.7f;
    r.draw(shadow, bx, float(floor) * kCellPx - camY - 2.0f, so);
    r.drawLine(bx, by - 60.0f, bx + 6.0f, by + 8.0f, 3.0f, kRope);
    r.drawLine(bx - 20.0f, by - 50.0f, bx - 20.0f, by - 10.0f, 2.0f, rgba(255, 255, 255, 90));
    r.drawLine(bx + 20.0f, by - 50.0f, bx + 20.0f, by - 10.0f, 2.0f, rgba(255, 255, 255, 90));
    r.draw(bag, bx, by);
  }

  // The flames: columns, the wave, PYRO FULL STAGE, and the Q99 cannon's
  // blast along C5.
  for (std::size_t i = 0; i < s.pyros.size(); ++i)
  {
    const auto& py = s.pyros[i];
    if (pyroHeat(py, 0) != 2)
      continue;
    const CellBox f = py.flame();
    const float x = float(f.x) * kCellPx - camX, y = float(f.y) * kCellPx - camY;
    const float w = float(f.w) * kCellPx, h = float(f.h) * kCellPx;
    if (offScreen(x, y, w, h))
      continue;
    if (py.dir == 0)
    {
      const float foot = (float(py.y) + 0.5f) * kTilePx - camY - 10.0f;
      flameColumn(r, mArt, x, y, foot, w, frame, 1.0f, int(i) * 3);
    }
    else
    {
      const int k = (frame / 2 + int(i)) % 4;
      const Texture& jet = baked(mArt, r, "jet" + std::to_string(k), 256, 72, 0.0f, 36.0f, [k](cairo_t* cr) { paintFlameJet(cr, k); });
      const float mouth = py.dir < 0 ? x + w : x;
      const float cy = y + h * 0.5f + 8.0f;
      blit(r, jet, mouth - (py.dir < 0 ? w + 40.0f : 0.0f), cy - 70.0f, w + 40.0f, 140.0f, py.dir < 0 ? 180.0f : 0.0f,
        py.dir < 0 ? w + 40.0f : 0.0f, 70.0f, 1.0f, Blend::Add);
      r.fillRect(x, y - 40.0f, w, h + 80.0f, rgba(255, 110, 30, 24), Blend::Add);
      drawGlow(r, mArt, mouth, cy, 80.0f, kFire, 0.8f);
    }
  }

  // The confetti blast: a CO2 plume at the mouth, paper streaming along
  // the lane and fluttering down.
  const Texture& bit = baked(mArt, r, "confettibit", 14, 8, 7.0f, 4.0f, [](cairo_t* cr) {
    roundRect(cr, 0.5, 0.5, 13, 7, 1.5);
    setRgba(cr, rgb(255, 255, 255), 1.0);
    cairo_fill(cr);
  });
  for (const auto& c : s.cues)
  {
    if (c.kind != FtCueKind::Confetti || c.list < 0)
      continue;
    const auto& l = s.lists[std::size_t(c.list)];
    if (!l.started || l.t < c.t || l.t - c.t >= kFtConfettiPush + 20)
      continue;
    const float u = (float(l.t - c.t) + alpha) / float(kFtConfettiPush);
    for (int k : c.targets)
    {
      const auto& cf = s.confetti[std::size_t(k)];
      const CellBox lane = cf.lane();
      const float lx0 = float(lane.x) * kCellPx - camX, lx1 = float(lane.x + lane.w) * kCellPx - camX;
      const float ly = float(lane.y) * kCellPx - camY, lh = float(lane.h) * kCellPx;
      if (offScreen(lx0, ly, lx1 - lx0, lh))
        continue;
      const float mouthX = (float(cf.x) + 1.0f) * kTilePx - camX + float(cf.dir) * 60.0f;
      const float mouthY = (float(cf.y) + 1.0f) * kTilePx - camY - 62.0f;
      if (u < 1.0f)
        for (int j = 0; j < 4; ++j)
          drawGlow(r, mArt, mouthX + float(cf.dir) * (30.0f + float(j) * 60.0f * u), mouthY - float(j) * 10.0f * u,
            30.0f + float(j) * 20.0f, rgb(240, 245, 255), 0.5f * (1.0f - u));
      static const Color kBits[] = {rgb(255, 80, 120), rgb(80, 220, 255), rgb(255, 230, 60), rgb(160, 255, 120), rgb(255, 255, 255),
        rgb(200, 120, 255)};
      const float span = lx1 - lx0;
      for (int i = 0; i < 70; ++i)
      {
        const float r0 = unit(hash2(i, k * 31 + 7)), r1 = unit(hash2(i, k * 31 + 8)), r2 = unit(hash2(i, k * 31 + 9));
        const float t = u * (0.7f + r0 * 0.6f) - r1 * 0.3f;
        if (t <= 0.0f)
          continue;
        const float along = std::min(1.2f, t) * span * (0.6f + 0.5f * r2);
        const float bxp = cf.dir < 0 ? (mouthX - along) : (mouthX + along);
        const float fall = std::max(0.0f, t - 0.5f) * 220.0f;
        const float byp = mouthY - 30.0f * std::sin(std::min(1.0f, t) * float(kPi)) + (r1 - 0.5f) * lh * 0.9f + fall +
          std::sin(float(frame) * 0.3f + float(i)) * 6.0f;
        DrawOpts o;
        o.tint = kBits[i % 6];
        o.angle = float(frame * (8 + i % 7) + i * 40) * (i % 2 ? 1.0f : -1.0f);
        o.alpha = std::clamp(2.0f - t * 1.2f, 0.0f, 1.0f);
        o.scale = 0.8f + r2 * 0.6f;
        r.draw(bit, bxp, byp, o);
      }
    }
  }

  // The Pyro Rig: its flame stream, and the jetpack's plume (bigger and
  // brighter riding a burning column).
  if (s.stream > 0 && p.weapon == Weapon::Proto && p.proto == int(ProtoId::PyroRig))
  {
    const CellBox cone = pyroStream();
    const float x = float(cone.x) * kCellPx - camX, y = float(cone.y) * kCellPx - camY;
    const float w = float(cone.w) * kCellPx, h = float(cone.h) * kCellPx;
    const int k = (frame / 2) % 4;
    const Texture& jet = baked(mArt, r, "jet" + std::to_string(k), 256, 72, 0.0f, 36.0f, [k](cairo_t* cr) { paintFlameJet(cr, k); });
    if (p.stance == Stance::Up)
      blit(r, jet, x + w * 0.5f, y + h, h + 20.0f, 96.0f, -90.0f, 0.0f, 48.0f, 1.0f, Blend::Add);
    else if (p.facing > 0)
      blit(r, jet, x - 10.0f, y + h * 0.5f - 46.0f, w + 30.0f, 92.0f, 0.0f, 0.0f, 46.0f, 1.0f, Blend::Add);
    else
      blit(r, jet, x + w + 10.0f, y + h * 0.5f - 46.0f, w + 30.0f, 92.0f, 180.0f, 0.0f, 46.0f, 1.0f, Blend::Add);
    drawGlow(r, mArt, x + w * 0.5f, y + h * 0.5f, std::max(w, h) * 0.6f, kFire, 0.45f);
  }
  if (p.state == PlayerState::Jetpack)
  {
    bool free = p.turbo > 0;
    for (const auto& py : s.pyros)
      free = free || (pyroHeat(py, 0) == 2 && py.flame().intersects(p.hitBox()));
    const int k = (frame / 2) % 4;
    const Texture& jet = baked(mArt, r, "jet" + std::to_string(k), 256, 72, 0.0f, 36.0f, [k](cairo_t* cr) { paintFlameJet(cr, k); });
    const float len = free ? 150.0f : 90.0f;
    blit(r, jet, px - 4.0f, pfoot - 20.0f, len, 44.0f, 90.0f, 0.0f, 22.0f, 0.9f, Blend::Add);
    if (free)
      drawGlow(r, mArt, px, pfoot + 30.0f, 70.0f, rgb(120, 170, 255), 0.5f);
  }
}

// --- HUD -------------------------------------------------------------------------------------------

void World::drawFlyTowerHud(Renderer& r, int frame) const
{
  const auto& s = mFly;
  char buf[96];
  if (s.trigger)
  {
    // Trigger Happy: a panel under the score, a disc for every target, lit
    // as it falls.
    const int n = int(s.targets.size());
    const float x = float(kScreenW) - 300.0f, y = 92.0f;
    r.fillRect(x, y, 284.0f, 74.0f, rgba(10, 8, 20, 190));
    r.fillRect(x, y, 284.0f, 3.0f, kAmber);
    std::snprintf(buf, sizeof(buf), "DISCS %d / %d", s.hits, n);
    r.drawText(buf, x + 142.0f, y + 6.0f, {20.0f, rgb(255, 230, 140), kHudInk}, Align::Center);
    for (int k = 0; k < n; ++k)
    {
      const float dx = x + 18.0f + float(k % 12) * 22.0f, dy = y + 42.0f + float(k / 12) * 18.0f;
      const bool hit = k < s.hits;
      r.fillRect(dx - 7.0f, dy - 7.0f, 14.0f, 14.0f, hit ? rgb(255, 230, 120) : rgb(60, 30, 30));
      r.fillRect(dx - 3.0f, dy - 3.0f, 6.0f, 6.0f, hit ? rgb(255, 255, 255) : rgb(160, 40, 40));
    }
    return;
  }
  if (s.active < 0)
    return;
  // The stage manager's cue sheet: the zone's next three cues, a standby
  // lamp that goes red within 3 s and green on GO.
  struct Line
  {
    const FtCue* cue;
    int in;
  };
  std::vector<Line> lines;
  for (const auto& c : s.cues)
  {
    if (c.list != s.active)
      continue;
    if (c.kind == FtCueKind::Cut)
    {
      lines.push_back({&c, 1 << 20});
      continue;
    }
    const int in = cueIn(c);
    if (in < 0 || (c.warn > 0 && in > c.warn))
      continue;
    lines.push_back({&c, c.kind == FtCueKind::Wave ? -1 : in});
  }
  std::stable_sort(lines.begin(), lines.end(), [](const Line& a, const Line& b) { return a.in < b.in; });
  if (lines.size() > 3)
    lines.resize(3);
  if (lines.empty())
    return;
  const float x = 16.0f, y = 140.0f, rowH = 30.0f, w = 400.0f; // under the messages and the bonus clock
  const float h = 30.0f + rowH * float(lines.size());
  r.fillRect(x, y, w, h, rgba(10, 8, 16, 205));
  r.fillRect(x, y, w, 3.0f, kAmber);
  r.fillRect(x, y + h - 2.0f, w, 2.0f, rgba(255, 170, 60, 90));
  std::snprintf(buf, sizeof(buf), "CUE SHEET - %s", s.lists[std::size_t(s.active)].id == "cl1"   ? "WINGS"
                                                     : s.lists[std::size_t(s.active)].id == "cl2" ? "LOWER CATWALKS"
                                                     : s.lists[std::size_t(s.active)].id == "cl3" ? "FLY GALLERY"
                                                     : s.lists[std::size_t(s.active)].id == "cl4" ? "PYRO WALL"
                                                     : s.lists[std::size_t(s.active)].id == "cl5" ? "GRID"
                                                                                                    : "ZONE");
  r.drawText(buf, x + 12.0f, y + 7.0f, {15.0f, rgb(255, 200, 120), 0});
  float ly = y + 28.0f;
  for (const auto& ln : lines)
  {
    const FtCue& c = *ln.cue;
    bool struck = c.kind == FtCueKind::Cut;
    if (struck)
      for (int k : c.targets)
        if (s.pyros[std::size_t(k)].faced > 0)
          struck = false; // it flickers back while the runner faces its cannon
    const bool standby = ln.in >= 0 && ln.in <= 45;
    const bool go = ln.in >= 0 && ln.in <= 8;
    // The cue light.
    const Color lamp = struck ? rgb(50, 50, 56) : go ? rgb(60, 255, 120) : standby ? rgb(255, 50, 40) : rgb(70, 30, 30);
    const bool lampOn = go || (standby && (frame / 8) % 2 == 0) || c.kind == FtCueKind::Wave;
    r.fillRect(x + 12.0f, ly + 6.0f, 14.0f, 14.0f, lampOn ? lamp : darken(lamp, 0.55f));
    if (lampOn && !struck)
      r.fillRect(x + 8.0f, ly + 2.0f, 22.0f, 22.0f, withAlpha(lamp, 60), Blend::Add);
    // Number, label, count.
    const Color col = struck ? rgb(130, 130, 140) : go && (frame / 3) % 2 ? rgb(255, 255, 255) : rgb(255, 222, 150);
    r.fillRect(x + 36.0f, ly + 3.0f, 52.0f, 22.0f, struck ? rgba(40, 40, 48, 200) : rgba(255, 170, 60, 50));
    r.drawText(c.id, x + 62.0f, ly + 4.0f, {18.0f, col, 0}, Align::Center);
    const float lw = r.drawText(c.label, x + 98.0f, ly + 4.0f, {18.0f, col, 0}, Align::Left);
    if (struck)
      r.fillRect(x + 36.0f, ly + 14.0f, 62.0f + lw, 2.0f, col);
    if (c.kind == FtCueKind::Wave)
      r.drawText("LIVE", x + w - 14.0f, ly + 4.0f, {18.0f, (frame / 6) % 2 ? kFire : kFireHot, 0}, Align::Right);
    else if (c.kind == FtCueKind::Cut)
    {
      if (!struck)
        r.drawText("?!", x + w - 14.0f, ly + 4.0f, {18.0f, (frame / 3) % 2 ? kRed : rgb(255, 222, 150), 0}, Align::Right);
    }
    else if (!struck)
    {
      std::snprintf(buf, sizeof(buf), "%d.%d", ln.in / 15, (ln.in % 15) * 10 / 15);
      r.drawText(go ? "GO" : buf, x + w - 14.0f, ly + 4.0f, {18.0f, go ? rgb(80, 255, 140) : col, 0}, Align::Right);
    }
    ly += rowH;
  }
}

// --- Shots and the icon ----------------------------------------------------------------------------

bool drawFlyTowerShot(Renderer& r, const World& w, const Art& art, const Projectile& pr, float cx, float cy, int frame)
{
  if (!w.flyTower().on || pr.kind != ShotKind::Enemy)
    return false;
  // A Rigger's dart: red-hot, its line trailing back.
  float vx = float(pr.dx), vy = float(pr.dy);
  if (pr.precise)
  {
    vx = pr.vx;
    vy = pr.vy;
  }
  const float len = std::max(0.01f, std::sqrt(vx * vx + vy * vy));
  vx /= len;
  vy /= len;
  r.drawLine(cx - vx * 70.0f, cy - vy * 70.0f, cx, cy, 2.0f, rgba(214, 190, 140, 160));
  r.drawLine(cx - vx * 18.0f, cy - vy * 18.0f, cx + vx * 6.0f, cy + vy * 6.0f, 6.0f, rgb(60, 20, 20));
  r.drawLine(cx - vx * 16.0f, cy - vy * 16.0f, cx + vx * 8.0f, cy + vy * 8.0f, 3.0f, rgb(255, 120, 80), Blend::Add);
  drawGlow(r, art, cx, cy, 22.0f + float(frame % 3) * 2.0f, kRed, 0.8f);
  return true;
}

void drawPyroRigIcon(Renderer& r, const Art& art, float x, float y, float scale, int frame)
{
  const Texture& icon = baked(art, r, "pyroicon", 64, 64, 0.0f, 0.0f, [](cairo_t* cr) {
    // The fuel tank.
    roundRect(cr, 8, 34, 30, 16, 7);
    vgrad(cr, 34, 50, {{0.0, rgb(255, 170, 80)}, {0.4, rgb(230, 100, 30)}, {1.0, rgb(120, 40, 10)}}, 1.0, true);
    stroke(cr, kInk, 1.8);
    box(cr, 16, 34, 3, 16, rgb(60, 30, 20));
    box(cr, 28, 34, 3, 16, rgb(60, 30, 20));
    // The hose.
    cairo_move_to(cr, 10, 40);
    cairo_curve_to(cr, 2, 30, 8, 22, 18, 26);
    stroke(cr, kInk, 4.0);
    cairo_move_to(cr, 10, 40);
    cairo_curve_to(cr, 2, 30, 8, 22, 18, 26);
    stroke(cr, rgb(60, 60, 70), 2.2);
    // The wand: grip, body, a long nozzle.
    roundRect(cr, 16, 20, 30, 10, 3);
    vgrad(cr, 20, 30, {{0.0, rgb(110, 110, 120)}, {1.0, rgb(30, 30, 36)}}, 1.0, true);
    stroke(cr, kInk, 1.6);
    roundRect(cr, 22, 28, 7, 12, 2);
    setRgba(cr, rgb(30, 30, 36), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.2);
    cairo_rectangle(cr, 44, 22, 12, 6);
    vgrad(cr, 22, 28, {{0.0, rgb(220, 220, 230)}, {1.0, rgb(90, 90, 100)}}, 1.0, true);
    stroke(cr, kInk, 1.2);
  });
  DrawOpts o;
  o.scale = scale;
  r.draw(icon, x, y, o);
  // The pilot flame.
  const float fx = x + 57.0f * scale, fy = y + 25.0f * scale;
  drawGlow(r, art, fx + 2.0f * scale, fy, 10.0f * scale + float(frame % 4), rgb(120, 170, 255), 0.9f);
  drawGlow(r, art, fx + 6.0f * scale, fy, 8.0f * scale, kFire, 0.6f + 0.2f * float((frame / 3) % 2));
}

} // namespace gr
