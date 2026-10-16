// Level 41, Sweeps Week: drawing. See world_sweeps.cpp for the logic.
//
// Behind the tiles (drawSweepsSet, sweeps_draw.hpp): the back rows of the
// audience climbing into the dark behind the stands, cameras flashing; the
// black understage under the stage floor (the orchestra pit, the audience
// pit, the hollow under the stands).
//
// Over the tiles (drawSweepsBack): the seats along the tiers and G's plush
// seat over the hollow; the host's desk; the arenas' brass stanchions, gold
// floor tape and marquee signs; the jumbotron over every vote, its bulbs
// chasing, the applause dial resting at 42 dB or swinging over the ballot's
// three options, the words; the turntables' rims with their chevrons; the
// studio audience clapping, the cameos turning their heads after the
// runner. Paintball's spring pad.
//
// In front (drawSweepsFront): the Warm-up Comics, the Audience Plants in
// their seats, the Cue Card Drones and their cards; party hats, beaks and
// BIG HEADS (the runner's too); the ??? party poppers; rubber chickens; the
// Virus's glow. Paintball's splats. On top (drawSweepsTop): LIGHTS OUT,
// MIRROR MODE. Under MIRROR MODE, or BIG HEADS with the runner's lens on,
// the scene is drawn into the renderer's offscreen capture
// (beginSweepsScene): BIG HEADS grabs the runner's head from it,
// drawSweepsTop puts it back on the screen, mirrored for MIRROR MODE;
// nothing is read back from the GPU. Only when the renderer refuses the
// capture are the head and the mirrored picture read back from the screen,
// and the head always without SDL_RenderGeometry (SDL older than 2.0.18),
// which the lens is faded with on the GPU.
// The HUD (drawSweepsHud): the Applause Cannon's meter, the rule in force,
// the ballot; Paintball's share painted.

#include "game/world.hpp"

#include "assets/art.hpp"
#include "assets/enemy_art.hpp"
#include "base/math.hpp"
#include "data/enemies.hpp"
#include "data/weapons.hpp"
#include "game/sweeps_draw.hpp"
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
constexpr Color kInk = rgb(12, 6, 10);
constexpr Color kHudInk = rgb(8, 4, 8);
constexpr Color kVel = rgb(122, 22, 44);
constexpr Color kVelHi = rgb(196, 56, 80);
constexpr Color kVelLo = rgb(46, 6, 18);
constexpr Color kGold = rgb(226, 176, 70);
constexpr Color kGoldHi = rgb(255, 238, 170);
constexpr Color kGoldLo = rgb(120, 80, 26);
constexpr Color kBulb = rgb(255, 232, 160);
constexpr Color kLacquer = rgb(22, 14, 20);

Color lighten(Color c, float t) { return lerpColor(c, rgb(255, 255, 255), t); }

float lerpCells(int prev, int cur, float alpha) { return (float(prev) + float(cur - prev) * alpha) * kCellPx; }

bool offScreen(float x, float y, float w, float h, float m = 96.0f)
{
  return x > float(kScreenW) + m || x + w < -m || y > float(kScreenH) + m || y + h < -m;
}

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
  const std::string id = "~sweeps/" + key;
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

// --- Painters --------------------------------------------------------------------------------

void bulbPaint(cairo_t* cr, double x, double y, double r, bool lit)
{
  if (lit)
    softDot(cr, x, y, r * 3.0, r * 3.0, kBulb, 0.35);
  circle(cr, x, y, r);
  setRgba(cr, lit ? kBulb : rgb(110, 80, 50), 1.0);
  cairo_fill(cr);
  circle(cr, x - r * 0.3, y - r * 0.35, r * 0.4);
  setRgba(cr, rgb(255, 255, 255), lit ? 0.8 : 0.3);
  cairo_fill(cr);
}

void starPath(cairo_t* cr, double cx, double cy, double ro, double ri)
{
  for (int i = 0; i < 10; ++i)
  {
    const double a = -kPi / 2 + i * kPi / 5, rr = i % 2 ? ri : ro;
    if (i == 0)
      cairo_move_to(cr, cx + std::cos(a) * rr, cy + std::sin(a) * rr);
    else
      cairo_line_to(cr, cx + std::cos(a) * rr, cy + std::sin(a) * rr);
  }
  cairo_close_path(cr);
}

// A soft round glow for a lit bulb, 24 x 24 (drawn additive).
void paintBulbGlow(cairo_t* cr)
{
  softDot(cr, 12, 12, 12, 12, kBulb, 0.9);
  circle(cr, 12, 12, 3.2);
  setRgba(cr, rgb(255, 250, 235), 1.0);
  cairo_fill(cr);
}

// The jumbotron (1088 x 320, the 1024 x 256 screen at (32, 32)): a black
// lacquer housing with gold trim and a row of bulb sockets all round it,
// the LED face dark with its pixel grid showing.
void paintJumbo(cairo_t* cr)
{
  roundRect(cr, 2, 2, 1084, 316, 18);
  vgrad(cr, 0, 320, {{0.0, rgb(50, 30, 40)}, {0.5, kLacquer}, {1.0, rgb(12, 6, 10)}}, 1.0, true);
  stroke(cr, kGold, 4.0);
  roundRect(cr, 10, 10, 1068, 300, 12);
  stroke(cr, kGoldLo, 1.5);
  for (double x = 16; x < 1080; x += 32)
  {
    bulbPaint(cr, x, 16, 4.5, false);
    bulbPaint(cr, x, 304, 4.5, false);
  }
  for (double y = 48; y < 290; y += 32)
  {
    bulbPaint(cr, 16, y, 4.5, false);
    bulbPaint(cr, 1072, y, 4.5, false);
  }
  cairo_rectangle(cr, 32, 32, 1024, 256);
  vgrad(cr, 32, 288, {{0.0, rgb(16, 10, 30)}, {1.0, rgb(6, 4, 12)}});
  for (double x = 32; x < 1056; x += 4)
    box(cr, x, 32, 1, 256, rgb(0, 0, 0), 0.35);
  for (double y = 32; y < 288; y += 4)
    box(cr, 32, y, 1024, 1, rgb(0, 0, 0), 0.35);
  // The glass's sheen.
  cairo_move_to(cr, 32, 32);
  cairo_line_to(cr, 420, 32);
  cairo_line_to(cr, 300, 288);
  cairo_line_to(cr, 32, 288);
  cairo_close_path(cr);
  setRgba(cr, rgb(255, 255, 255), 0.03);
  cairo_fill(cr);
}

// The applause dial (400 x 230, its pivot at (200, 200)): a cream face with
// a gold bezel, ticks every 10 dB to 120, APPLAUSE across it.
void paintDial(cairo_t* cr)
{
  cairo_arc(cr, 200, 200, 190, kPi, 2 * kPi);
  cairo_close_path(cr);
  vgrad(cr, 10, 200, {{0.0, rgb(255, 248, 225)}, {1.0, rgb(225, 205, 160)}}, 1.0, true);
  stroke(cr, kGold, 8.0);
  for (int k = 0; k <= 24; ++k)
  {
    const double a = kPi + kPi * k / 24.0, r0 = k % 2 ? 160 : 148;
    cairo_move_to(cr, 200 + std::cos(a) * r0, 200 + std::sin(a) * r0);
    cairo_line_to(cr, 200 + std::cos(a) * 180, 200 + std::sin(a) * 180);
    stroke(cr, kInk, k % 2 ? 1.6 : 3.0);
  }
  for (int k = 0; k <= 12; k += 3)
  {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%d", k * 10);
    const double a = kPi + kPi * k / 12.0;
    text(cr, buf, 200 + std::cos(a) * 128, 206 + std::sin(a) * 128, 18, kInk);
  }
  text(cr, "APPLAUSE", 200, 150, 26, rgb(150, 20, 40));
  text(cr, "dB", 200, 178, 16, kInk, 0.7);
  box(cr, 0, 200, 400, 30, kLacquer);
  box(cr, 0, 200, 400, 3, kGold);
}

// A zone of the dial (400 x 210, pivot (200, 200)): a white band of the arc
// 60 degrees wide pointing up, tinted by the option's colour.
void paintWedge(cairo_t* cr)
{
  cairo_arc(cr, 200, 200, 184, -kPi / 2 - kPi / 6, -kPi / 2 + kPi / 6);
  cairo_arc_negative(cr, 200, 200, 120, -kPi / 2 + kPi / 6, -kPi / 2 - kPi / 6);
  cairo_close_path(cr);
  setRgba(cr, rgb(255, 255, 255), 0.75);
  cairo_fill(cr);
}

// The needle (24 x 190, pivot (12, 178)), pointing up: black with a red tip
// and a brass boss.
void paintNeedle(cairo_t* cr)
{
  cairo_move_to(cr, 8, 178);
  cairo_line_to(cr, 12, 4);
  cairo_line_to(cr, 16, 178);
  cairo_close_path(cr);
  setRgba(cr, kInk, 1.0);
  cairo_fill(cr);
  cairo_move_to(cr, 10, 40);
  cairo_line_to(cr, 12, 4);
  cairo_line_to(cr, 14, 40);
  cairo_close_path(cr);
  setRgba(cr, rgb(230, 30, 50), 1.0);
  cairo_fill(cr);
  circle(cr, 12, 178, 10);
  vgrad(cr, 168, 188, {{0.0, kGoldHi}, {1.0, kGoldLo}}, 1.0, true);
  stroke(cr, kInk, 1.4);
}

// A theatre seat seen from the front (96 x 96, anchored bottom middle): a
// red velvet back with a gold number plate, the cushion, the armrests.
void paintSeat(cairo_t* cr)
{
  roundRect(cr, 14, 8, 68, 58, 14);
  vgrad(cr, 8, 66, {{0.0, kVelHi}, {0.5, kVel}, {1.0, kVelLo}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  for (double x = 30; x < 80; x += 16)
  {
    cairo_move_to(cr, x, 14);
    cairo_line_to(cr, x, 60);
    stroke(cr, kVelLo, 1.4, 0.6);
  }
  roundRect(cr, 40, 12, 16, 7, 2);
  vgrad(cr, 12, 19, {{0.0, kGoldHi}, {1.0, kGoldLo}}, 1.0, true);
  stroke(cr, kInk, 0.8);
  roundRect(cr, 12, 62, 72, 16, 6);
  vgrad(cr, 62, 78, {{0.0, kVelHi}, {1.0, kVelLo}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  for (const double ax : {6.0, 80.0})
  {
    roundRect(cr, ax, 48, 10, 46, 4);
    hgrad(cr, ax, ax + 10, {{0.0, rgb(60, 40, 40)}, {1.0, rgb(24, 14, 16)}}, 1.0, true);
    stroke(cr, kInk, 1.2);
    box(cr, ax, 48, 10, 3, kGold);
  }
}

// The back rows (512 x 192, tiles across): rows of seat backs climbing away
// into the dark, heads over them, faint.
void paintBackRows(cairo_t* cr)
{
  Rng rng(41201u);
  for (int row = 0; row < 4; ++row)
  {
    const double y = 20 + row * 46, s = 0.6 + row * 0.1;
    for (double x = (row % 2) * 16; x < 512; x += 32)
    {
      if (rng.uniform() < 0.8f)
      {
        // A head and shoulders.
        circle(cr, x + 16, y + 8, 9 * s);
        setRgba(cr, rgb(40, 22, 30), 1.0);
        cairo_fill(cr);
        ellipse(cr, x + 16, y + 26, 15 * s, 10 * s);
        setRgba(cr, rgb(34, 18, 26), 1.0);
        cairo_fill(cr);
      }
      roundRect(cr, x + 3, y + 22, 26, 22, 6);
      setRgba(cr, rgb(70, 16, 30), 1.0);
      cairo_fill(cr);
      box(cr, x + 3, y + 22, 26, 2, rgb(140, 40, 60), 0.6);
    }
  }
}

// The understage (64 x 64, tiles): black-painted walls with the stage's
// steel framing showing, a little light leaking through the boards above.
void paintUnderstage(cairo_t* cr)
{
  box(cr, 0, 0, 64, 64, rgb(14, 7, 11));
  for (double x = 0; x < 64; x += 8)
    box(cr, x, 0, 1, 64, rgb(24, 12, 18), 0.8);
  cairo_move_to(cr, 0, 0);
  cairo_line_to(cr, 64, 64);
  cairo_move_to(cr, 64, 0);
  cairo_line_to(cr, 0, 64);
  stroke(cr, rgb(26, 18, 22), 2.0);
  box(cr, 0, 0, 3, 64, rgb(30, 22, 26));
  softDot(cr, 32, 0, 30, 6, rgb(255, 200, 140), 0.06);
}

// The host's desk (352 x 136, its top surface at y = 8): a curved front of
// red lacquer with gold bands and the show's star, a glossy black top with
// a coffee mug, the host's desk mic and a stack of cue cards.
void paintDesk(cairo_t* cr)
{
  // The front.
  cairo_move_to(cr, 8, 14);
  cairo_line_to(cr, 344, 14);
  cairo_curve_to(cr, 340, 80, 330, 120, 320, 136);
  cairo_line_to(cr, 32, 136);
  cairo_curve_to(cr, 22, 120, 12, 80, 8, 14);
  cairo_close_path(cr);
  hgrad(cr, 8, 344, {{0.0, rgb(80, 10, 24)}, {0.3, rgb(200, 34, 56)}, {0.55, rgb(150, 20, 40)}, {1.0, rgb(70, 8, 20)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  for (const double y : {34.0, 112.0})
  {
    cairo_rectangle(cr, 14, y, 324, 5);
    vgrad(cr, y, y + 5, {{0.0, kGoldHi}, {1.0, kGoldLo}});
  }
  starPath(cr, 176, 74, 30, 13);
  vgrad(cr, 44, 104, {{0.0, kGoldHi}, {0.5, kGold}, {1.0, kGoldLo}}, 1.0, true);
  stroke(cr, kGoldLo, 1.5);
  text(cr, "SWEEPS WEEK", 176, 130, 13, kGoldHi);
  // The top.
  roundRect(cr, 0, 2, 352, 14, 6);
  vgrad(cr, 2, 16, {{0.0, rgb(90, 70, 80)}, {0.3, rgb(30, 20, 26)}, {1.0, rgb(14, 8, 12)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  box(cr, 6, 4, 340, 1.4, rgb(255, 220, 220), 0.5);
  // A mug, a desk mic, the cue cards.
  roundRect(cr, 40, -18, 20, 22, 3);
  vgrad(cr, -18, 4, {{0.0, rgb(255, 255, 255)}, {1.0, rgb(200, 200, 210)}}, 1.0, true);
  stroke(cr, kInk, 1.2);
  text(cr, "#1", 50, -4, 9, rgb(220, 30, 50));
  cairo_arc(cr, 62, -8, 5, -kPi / 2, kPi / 2);
  stroke(cr, kInk, 2.4);
  cairo_move_to(cr, 280, 4);
  cairo_curve_to(cr, 282, -10, 270, -20, 256, -26);
  stroke(cr, rgb(40, 40, 46), 3.0);
  ellipse(cr, 252, -28, 7, 9);
  vgrad(cr, -37, -19, {{0.0, rgb(220, 220, 230)}, {1.0, rgb(90, 90, 100)}}, 1.0, true);
  stroke(cr, kInk, 1.2);
  for (int k = 0; k < 3; ++k)
  {
    cairo_save(cr);
    cairo_translate(cr, 130 + k * 3, -2 - k * 2);
    cairo_rotate(cr, -0.08 + k * 0.06);
    roundRect(cr, -22, -6, 44, 7, 1.5);
    setRgba(cr, rgb(250, 248, 236), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 0.8);
    cairo_restore(cr);
  }
}

// A marquee sign (w x 72): a red board in gold, bulbs round it, the words.
void paintSign(cairo_t* cr, int w, const std::string& words)
{
  roundRect(cr, 4, 4, w - 8, 64, 10);
  vgrad(cr, 4, 68, {{0.0, rgb(160, 24, 50)}, {1.0, rgb(70, 8, 20)}}, 1.0, true);
  stroke(cr, kGold, 4.0);
  for (double x = 20; x < w - 10; x += 24)
  {
    bulbPaint(cr, x, 12, 3.4, true);
    bulbPaint(cr, x, 60, 3.4, true);
  }
  selectGameFont(cr);
  cairo_set_font_size(cr, 26);
  cairo_text_extents_t e;
  cairo_text_extents(cr, words.c_str(), &e);
  cairo_move_to(cr, w * 0.5 - e.width * 0.5 - e.x_bearing, 46);
  cairo_text_path(cr, words.c_str());
  cairo_set_line_width(cr, 5);
  setRgba(cr, kInk, 0.8);
  cairo_stroke_preserve(cr);
  vgrad(cr, 22, 46, {{0.0, kGoldHi}, {1.0, rgb(255, 160, 60)}});
}

// A brass stanchion (24 x 80, anchored bottom middle) with a little light.
void paintStanchion(cairo_t* cr)
{
  ellipse(cr, 12, 76, 11, 4);
  vgrad(cr, 72, 80, {{0.0, kGoldHi}, {1.0, kGoldLo}}, 1.0, true);
  stroke(cr, kInk, 1.0);
  cairo_rectangle(cr, 9, 12, 6, 64);
  hgrad(cr, 9, 15, {{0.0, kGoldHi}, {0.5, kGold}, {1.0, kGoldLo}}, 1.0, true);
  stroke(cr, kInk, 0.8);
  circle(cr, 12, 9, 6);
  vgrad(cr, 3, 15, {{0.0, kGoldHi}, {1.0, kGoldLo}}, 1.0, true);
  stroke(cr, kInk, 1.0);
}

// A turntable's rim seen edge on (w x 40, its surface at y = 8): a black
// lacquer disc edge with a gold band and the LED strip the arrows run in,
// rounded at both ends.
void paintTurntable(cairo_t* cr, int w)
{
  cairo_move_to(cr, 16, 4);
  cairo_line_to(cr, w - 16, 4);
  cairo_curve_to(cr, w - 2, 4, w - 2, 30, w - 16, 30);
  cairo_line_to(cr, 16, 30);
  cairo_curve_to(cr, 2, 30, 2, 4, 16, 4);
  cairo_close_path(cr);
  vgrad(cr, 4, 30, {{0.0, rgb(80, 60, 70)}, {0.2, rgb(30, 20, 26)}, {1.0, rgb(10, 6, 8)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  cairo_rectangle(cr, 14, 8, w - 28, 3);
  vgrad(cr, 8, 11, {{0.0, kGoldHi}, {1.0, kGoldLo}});
  cairo_rectangle(cr, 14, 14, w - 28, 12);
  setRgba(cr, rgb(20, 10, 16), 1.0);
  cairo_fill(cr);
  for (double x = 18; x < w - 18; x += 12)
    box(cr, x, 16, 8, 8, rgb(70, 40, 30), 0.8);
}

// A chevron (24 x 16) for the turntable's LED strip, pointing right.
void paintChevron(cairo_t* cr)
{
  cairo_move_to(cr, 2, 2);
  cairo_line_to(cr, 12, 8);
  cairo_line_to(cr, 2, 14);
  cairo_move_to(cr, 11, 2);
  cairo_line_to(cr, 21, 8);
  cairo_line_to(cr, 11, 14);
  stroke(cr, rgb(255, 255, 255), 3.0);
}

// A party hat (48 x 64, anchored bottom middle): a striped cone, a pompom,
// the elastic.
void paintHat(cairo_t* cr, Color a, Color b)
{
  cairo_move_to(cr, 6, 60);
  cairo_line_to(cr, 24, 8);
  cairo_line_to(cr, 42, 60);
  cairo_close_path(cr);
  cairo_save(cr);
  cairo_clip_preserve(cr);
  setRgba(cr, a, 1.0);
  cairo_fill_preserve(cr);
  for (double y = 0; y < 70; y += 14)
  {
    cairo_move_to(cr, 0, y + 10);
    cairo_line_to(cr, 48, y);
    cairo_line_to(cr, 48, y + 6);
    cairo_line_to(cr, 0, y + 16);
    cairo_close_path(cr);
  }
  setRgba(cr, b, 1.0);
  cairo_fill(cr);
  cairo_restore(cr);
  stroke(cr, kInk, 1.6);
  for (int k = 0; k < 8; ++k)
  {
    const double ang = k * kPi / 4;
    cairo_move_to(cr, 24, 8);
    cairo_line_to(cr, 24 + std::cos(ang) * 8, 8 + std::sin(ang) * 8);
  }
  stroke(cr, rgb(255, 230, 80), 2.4);
  circle(cr, 24, 8, 4);
  setRgba(cr, rgb(255, 250, 200), 1.0);
  cairo_fill(cr);
}

// A duck's bill for QUACK (40 x 28, anchored at its back middle (2, 14)).
void paintBeak(cairo_t* cr)
{
  cairo_move_to(cr, 2, 6);
  cairo_curve_to(cr, 20, 0, 36, 4, 38, 12);
  cairo_curve_to(cr, 36, 16, 20, 16, 2, 14);
  cairo_close_path(cr);
  vgrad(cr, 0, 16, {{0.0, rgb(255, 200, 60)}, {1.0, rgb(240, 140, 20)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  cairo_move_to(cr, 2, 14);
  cairo_curve_to(cr, 18, 16, 32, 18, 34, 20);
  cairo_curve_to(cr, 26, 26, 12, 24, 2, 20);
  cairo_close_path(cr);
  vgrad(cr, 14, 26, {{0.0, rgb(240, 150, 30)}, {1.0, rgb(200, 100, 10)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  circle(cr, 12, 8, 1.4);
  setRgba(cr, kInk, 0.8);
  cairo_fill(cr);
}

// ???: a party popper the size of a man (96 x 160, anchored bottom middle):
// a gold-and-pink striped cone on a stand, its string hanging, POP! on a tag.
void paintPopper(cairo_t* cr)
{
  cairo_move_to(cr, 30, 158);
  cairo_line_to(cr, 48, 120);
  cairo_line_to(cr, 66, 158);
  stroke(cr, rgb(40, 30, 34), 5.0);
  cairo_save(cr);
  cairo_translate(cr, 48, 116);
  cairo_rotate(cr, -0.3);
  cairo_move_to(cr, -14, 6);
  cairo_line_to(cr, 0, -100);
  cairo_line_to(cr, 14, 6);
  cairo_close_path(cr);
  cairo_save(cr);
  cairo_clip_preserve(cr);
  setRgba(cr, rgb(255, 210, 70), 1.0);
  cairo_fill_preserve(cr);
  for (double y = -100; y < 10; y += 18)
    box(cr, -20, y, 40, 9, rgb(255, 80, 160));
  cairo_restore(cr);
  stroke(cr, kInk, 1.8);
  ellipse(cr, 0, -100, 16, 5);
  setRgba(cr, rgb(255, 255, 240), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.2);
  cairo_move_to(cr, 0, 6);
  cairo_curve_to(cr, 6, 20, -6, 26, 4, 34);
  stroke(cr, rgb(250, 250, 250), 1.6);
  cairo_restore(cr);
  roundRect(cr, 52, 120, 34, 18, 3);
  setRgba(cr, rgb(255, 255, 255), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.2);
  text(cr, "POP!", 69, 134, 12, rgb(230, 30, 80));
}

// A rubber chicken (72 x 48, centred): limp yellow, red comb, honking beak.
void paintChicken(cairo_t* cr)
{
  cairo_translate(cr, 36, 26);
  cairo_move_to(cr, -24, 0);
  cairo_curve_to(cr, -22, -12, 2, -14, 10, -7);
  cairo_curve_to(cr, 14, -16, 18, -22, 24, -22);
  cairo_curve_to(cr, 30, -22, 30, -14, 26, -12);
  cairo_curve_to(cr, 22, -6, 20, 6, 8, 10);
  cairo_curve_to(cr, -8, 14, -24, 10, -24, 0);
  cairo_close_path(cr);
  vgrad(cr, -22, 12, {{0.0, rgb(255, 245, 170)}, {1.0, rgb(230, 190, 70)}}, 1.0, true);
  stroke(cr, kInk, 1.8);
  for (int k = 0; k < 7; ++k)
  {
    circle(cr, -16 + k * 3.6, -1 + (k % 2) * 3, 1.0);
    setRgba(cr, rgb(210, 170, 70), 1.0);
    cairo_fill(cr);
  }
  cairo_move_to(cr, 22, -22);
  cairo_curve_to(cr, 21, -29, 25, -29, 26, -25);
  cairo_curve_to(cr, 27, -30, 31, -29, 29, -22);
  setRgba(cr, rgb(230, 30, 50), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.0);
  cairo_move_to(cr, 29, -18);
  cairo_line_to(cr, 36, -20);
  cairo_line_to(cr, 30, -15);
  cairo_line_to(cr, 35, -12);
  cairo_line_to(cr, 28, -13);
  setRgba(cr, rgb(255, 150, 30), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.0);
  cairo_move_to(cr, 22, -19);
  cairo_line_to(cr, 25, -16);
  cairo_move_to(cr, 25, -19);
  cairo_line_to(cr, 22, -16);
  stroke(cr, kInk, 1.2);
  for (const double lx : {-16.0, -6.0})
  {
    cairo_move_to(cr, lx, 10);
    cairo_line_to(cr, lx - 3, 20);
    stroke(cr, kInk, 3.6);
    cairo_move_to(cr, lx, 10);
    cairo_line_to(cr, lx - 3, 20);
    stroke(cr, rgb(255, 150, 30), 2.0);
  }
}

// A cue card (160 x 60): white board, a thick marker word.
void paintCard(cairo_t* cr, const char* words)
{
  roundRect(cr, 3, 3, 154, 54, 4);
  vgrad(cr, 3, 57, {{0.0, rgb(255, 255, 250)}, {1.0, rgb(225, 222, 210)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  selectGameFont(cr);
  cairo_set_font_size(cr, 22);
  cairo_text_extents_t e;
  cairo_text_extents(cr, words, &e);
  const double sc = std::min(1.0, 140.0 / std::max(1.0, e.width));
  cairo_save(cr);
  cairo_translate(cr, 80, 39);
  cairo_scale(cr, sc, 1.0);
  cairo_move_to(cr, -e.width * 0.5 - e.x_bearing, 0);
  setRgba(cr, rgb(220, 20, 40), 1.0);
  cairo_show_text(cr, words);
  cairo_restore(cr);
}

// A tomato (32 x 32, centred), its green top.
void paintTomato(cairo_t* cr)
{
  circle(cr, 16, 17, 12);
  vgrad(cr, 5, 29, {{0.0, rgb(255, 120, 90)}, {0.5, rgb(230, 40, 30)}, {1.0, rgb(140, 20, 14)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  ellipse(cr, 11, 12, 4, 2.6);
  setRgba(cr, rgb(255, 255, 255), 0.6);
  cairo_fill(cr);
  for (int k = 0; k < 5; ++k)
  {
    const double a = -kPi / 2 + (k - 2) * 0.6;
    cairo_move_to(cr, 16, 6);
    cairo_line_to(cr, 16 + std::cos(a) * 7, 6 + std::sin(a) * 3 + 3);
  }
  stroke(cr, rgb(50, 150, 50), 2.2);
}

// An Applause Cannon burst (96 x 96, centred): a gold star with a ring of
// smaller stars and motion lines, white at its heart.
void paintClapBurst(cairo_t* cr)
{
  softDot(cr, 48, 48, 44, 44, rgb(255, 210, 90), 0.5);
  starPath(cr, 48, 48, 30, 13);
  vgrad(cr, 18, 78, {{0.0, rgb(255, 255, 240)}, {0.5, kGoldHi}, {1.0, kGold}}, 1.0, true);
  stroke(cr, kGoldLo, 2.0);
  for (int k = 0; k < 6; ++k)
  {
    const double a = k * kPi / 3 + 0.3;
    starPath(cr, 48 + std::cos(a) * 38, 48 + std::sin(a) * 38, 6, 2.6);
    setRgba(cr, rgb(255, 250, 220), 0.9);
    cairo_fill(cr);
  }
}

// A splat of paint (64 x 64, white and greys, tinted by the runner's
// colour): a smooth blob with droplets round it. On a floor top (`drip`)
// it is a wide puddle along the top (y 4 .. 18) running down the front in
// drips; on a wall a round splat.
void paintSplat(cairo_t* cr, int variant, bool drip)
{
  Rng rng(41301u + std::uint32_t(variant) * 17u);
  const double cx = 32, cy = drip ? 11 : 32, rx = drip ? 34 : 24, ry = drip ? 9 : 22;
  constexpr int n = 9;
  std::array<double, n> rad{};
  for (auto& v : rad)
    v = rng.range(0.75f, 1.15f);
  auto pt = [&](int i, double& x, double& y) {
    const double a = i * 2 * kPi / n;
    x = cx + std::cos(a) * rx * rad[std::size_t((i % n + n) % n)];
    y = cy + std::sin(a) * ry * rad[std::size_t((i % n + n) % n)];
  };
  double x0 = 0, y0 = 0;
  pt(0, x0, y0);
  cairo_move_to(cr, x0, y0);
  for (int i = 0; i < n; ++i)
  {
    double ax = 0, ay = 0, bx = 0, by = 0, px = 0, py = 0, qx = 0, qy = 0;
    pt(i - 1, px, py);
    pt(i, ax, ay);
    pt(i + 1, bx, by);
    pt(i + 2, qx, qy);
    cairo_curve_to(cr, ax + (bx - px) / 6, ay + (by - py) / 6, bx - (qx - ax) / 6, by - (qy - ay) / 6, bx, by);
  }
  cairo_close_path(cr);
  if (drip)
    for (int k = 0; k < 3; ++k)
    {
      const double x = 12 + k * 20 + rng.range(-4, 4), len = rng.range(8, 30);
      roundRect(cr, x - 3.5, 12, 7, len + 6, 3.5);
      circle(cr, x, 16 + len, 4.6);
    }
  cairo_pattern_t* g = cairo_pattern_create_radial(cx - 4, cy - 3, 2, cx, cy, std::max(rx, ry) * 1.2);
  stop(g, 0.0, rgb(255, 255, 255));
  stop(g, 0.7, rgb(225, 225, 225));
  stop(g, 1.0, rgb(170, 170, 170));
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
  for (int k = 0; k < 6; ++k)
  {
    const double a = rng.range(0, 6.28f), d = rng.range(1.15f, 1.5f);
    circle(cr, cx + std::cos(a) * rx * d, cy + std::sin(a) * ry * d, rng.range(1.5f, 3.5f));
    setRgba(cr, rgb(230, 230, 230), 1.0);
    cairo_fill(cr);
  }
  ellipse(cr, cx - rx * 0.35, cy - ry * 0.35, rx * 0.25, ry * 0.18);
  setRgba(cr, rgb(255, 255, 255), 0.9);
  cairo_fill(cr);
}

// Paintball's spring pad (128 x 40, its top at y = 6): a red pad on a gold
// coil in a black housing.
void paintPad(cairo_t* cr)
{
  roundRect(cr, 4, 4, 120, 10, 4);
  vgrad(cr, 4, 14, {{0.0, rgb(255, 120, 120)}, {1.0, rgb(180, 20, 30)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  for (double x = 14; x < 120; x += 14)
  {
    ellipse(cr, x, 22, 6, 6);
    stroke(cr, kGold, 2.4);
  }
  box(cr, 2, 30, 124, 10, kLacquer);
  box(cr, 2, 30, 124, 2, kGold);
}

// The Applause Cannon icon (64 x 64).
void paintCannonIcon(cairo_t* cr)
{
  // The barrel, a bell mouth to the right.
  cairo_move_to(cr, 8, 34);
  cairo_line_to(cr, 40, 30);
  cairo_curve_to(cr, 50, 26, 56, 22, 60, 18);
  cairo_line_to(cr, 60, 50);
  cairo_curve_to(cr, 56, 46, 50, 44, 40, 42);
  cairo_line_to(cr, 8, 40);
  cairo_close_path(cr);
  vgrad(cr, 18, 50, {{0.0, kGoldHi}, {0.45, kGold}, {1.0, kGoldLo}}, 1.0, true);
  stroke(cr, kInk, 1.8);
  ellipse(cr, 60, 34, 3.5, 16);
  setRgba(cr, rgb(60, 20, 20), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.4);
  box(cr, 16, 38, 8, 14, rgb(120, 24, 40));
  // The foam hand on top, clapping.
  roundRect(cr, 14, 12, 18, 18, 6);
  vgrad(cr, 12, 30, {{0.0, rgb(255, 120, 170)}, {1.0, rgb(200, 40, 100)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  for (int k = 0; k < 3; ++k)
  {
    roundRect(cr, 15 + k * 5.5, 2, 5, 12, 2.4);
    vgrad(cr, 2, 14, {{0.0, rgb(255, 140, 180)}, {1.0, rgb(220, 60, 120)}}, 1.0, true);
    stroke(cr, kInk, 1.0);
  }
  // Stars out of the mouth.
  starPath(cr, 54, 10, 6, 2.6);
  setRgba(cr, kGoldHi, 1.0);
  cairo_fill(cr);
  starPath(cr, 44, 6, 4, 1.8);
  cairo_fill(cr);
}

const char* const kCards[] = {"BOO!", "THROW THINGS!", "VOTE NASTY"};

Color catColor(SwCat c)
{
  switch (c)
  {
    case SwCat::Friendly:
      return rgb(90, 220, 120);
    case SwCat::Neutral:
      return rgb(110, 180, 255);
    case SwCat::Nasty:
      return rgb(255, 80, 80);
    case SwCat::Secret:
      return rgb(230, 120, 255);
    case SwCat::Fanfare:
      return rgb(255, 210, 60);
  }
  return rgb(255, 255, 255);
}

const char* catName(SwCat c)
{
  switch (c)
  {
    case SwCat::Friendly:
      return "FRIENDLY";
    case SwCat::Neutral:
      return "NEUTRAL";
    case SwCat::Nasty:
      return "NASTY";
    case SwCat::Secret:
      return "???";
    case SwCat::Fanfare:
      return "FANFARE";
  }
  return "";
}

Color runnerPaint(int character)
{
  static const Color kPaints[] = {rgb(60, 140, 255), rgb(255, 140, 40), rgb(255, 90, 190),
    rgb(80, 220, 120), rgb(80, 230, 240), rgb(200, 200, 215)};
  return character >= 0 && character < 6 ? kPaints[character] : rgb(255, 230, 80);
}

// The needle's position over the three options (0..2) during the swing.
float needleAt(const SweepsState& s)
{
  if (s.ballotT >= kSwSwing)
    return float(s.winner);
  const float t = float(s.ballotT) / float(kSwSwing);
  const float wobble = std::sin(float(s.ballotT) * 0.35f) * (1.0f - t) * 1.2f;
  return std::clamp(1.0f + (float(s.winner) - 1.0f) * t + wobble, 0.0f, 2.0f);
}

// BIG HEADS on the runner: the picture round their head (already drawn) put
// back `scale` times the size on the same neck, faded out to an oval: a
// caricature's head through a fisheye. Copied on the GPU from the scene
// capture, or read back from the screen when the renderer refused the
// capture (Renderer::drawLens). The head too near the picture's edge: no
// lens.
void bigHeadLens(Renderer& r, float cx, float headTop, float scale)
{
  constexpr float lw = 112.0f, lh = 92.0f;
  const float x0 = cx - lw * 0.5f, y0 = headTop - 14.0f;
  // The neck (the piece's lower middle) stays put.
  const float neckY = y0 + lh * 0.82f;
  r.drawLens({x0, y0, lw, lh}, {cx - lw * scale * 0.5f, neckY - lh * 0.82f * scale, lw * scale, lh * scale}, 0.75f);
}

// The stands' and the pit's spans (cells) from where the audience sits.
struct Spans
{
  int standX0 = 1 << 30, standX1 = -1, pitX0 = 1 << 30, pitX1 = -1, floorY = 0;
};

Spans spansOf(const SweepsState& s)
{
  Spans sp;
  int lowest = 0;
  for (const auto& c : s.crowd)
    lowest = std::max(lowest, c.y);
  for (const auto& c : s.crowd)
  {
    if (c.y == lowest)
    {
      sp.pitX0 = std::min(sp.pitX0, c.x);
      sp.pitX1 = std::max(sp.pitX1, c.x + 3);
    }
    else
    {
      sp.standX0 = std::min(sp.standX0, c.x);
      sp.standX1 = std::max(sp.standX1, c.x + 3);
    }
  }
  return sp;
}

} // namespace

void drawSweepsSet(Renderer& r, const World& w, const Art& art, float camX, float camY, int frame, float alpha)
{
  (void)alpha;
  const auto& s = w.sweeps();
  if (!s.on)
    return;
  const auto& map = w.map();
  const int bw = map.widthBlocks(), bh = map.heightBlocks();
  // The understage: under the stage floor, and in the hollow under the
  // stands, black framing rather than the curtain.
  const Texture& under = baked(art, r, "understage", 64, 64, 0.0f, 0.0f, paintUnderstage);
  const int bx0 = std::max(0, int(camX / kTilePx) - 1), bx1 = std::min(bw - 1, int((camX + float(kScreenW)) / kTilePx) + 1);
  const int by0 = std::max(0, int(camY / kTilePx) - 1), by1 = std::min(bh - 1, int((camY + float(kScreenH)) / kTilePx) + 1);
  int stage = bh;
  if (!s.paint)
  {
    // The stage floor: the most common top over the level's columns.
    std::vector<int> count(std::size_t(bh) + 1, 0);
    for (int bx = 1; bx < bw - 1; bx += 3)
      for (int by = 0; by < bh; ++by)
        if (map.block(bx, by) == Tile::Solid)
        {
          ++count[std::size_t(by)];
          break;
        }
    stage = int(std::max_element(count.begin(), count.end()) - count.begin());
  }
  for (int by = by0; by <= by1; ++by)
    for (int bx = bx0; bx <= bx1; ++bx)
    {
      if (map.block(bx, by) == Tile::Solid)
        continue;
      bool roofed = by >= stage;
      for (int k = 1; k <= 6 && !roofed && by - k >= 0; ++k)
        roofed = map.block(bx, by - k) == Tile::Solid;
      if (!roofed || s.paint)
        continue;
      const float x = float(bx) * kTilePx - camX, y = float(by) * kTilePx - camY;
      r.draw(under, x, y);
    }
  if (s.paint)
    return;
  const Spans sp = spansOf(s);
  // The back rows behind the stands, climbing into the dark, a camera
  // flash going off now and then.
  if (sp.standX1 > sp.standX0)
  {
    const float x0 = float(sp.standX0 - 4) * kCellPx - camX, x1 = float(sp.standX1 + 4) * kCellPx - camX;
    const float yb = float(stage) * kTilePx - camY, yt = yb - 15.0f * kTilePx;
    if (x1 > 0.0f && x0 < float(kScreenW))
    {
      const Texture& rows = baked(art, r, "backrows", 512, 192, 0.0f, 0.0f, paintBackRows);
      DrawOpts o;
      o.alpha = 0.85f;
      tile(r, rows, x0, yt, x1, yb, x0, yb, o);
      // Fading up into the dark.
      for (int k = 0; k < 8; ++k)
        r.fillRect(x0, yt + float(k) * 48.0f, x1 - x0, 48.0f, rgba(10, 4, 8, 230 - k * 28));
      for (int k = 0; k < 3; ++k)
      {
        const std::uint32_t h = hash2(frame / 20 + k * 7, 4141);
        if (h % 5u == 0)
        {
          const float fx = x0 + float(h % 997u) / 997.0f * (x1 - x0), fy = yt + 6.0f * kTilePx + float((h >> 10) % 400u);
          drawGlow(r, art, fx, fy, 30.0f, rgb(255, 255, 255), 0.6f);
        }
      }
    }
  }
}

void World::drawSweepsBack(Renderer& r, float camX, float camY, int frame, float /*alpha*/) const
{
  const auto& s = mSweeps;
  const auto& p = mPlayer;
  if (s.paint)
  {
    // Paintball: the spring pad.
    const Texture& pad = baked(mArt, r, "pad", 128, 40, 64.0f, 6.0f, paintPad);
    for (const auto& pd : s.pads)
    {
      const float cx = (float(pd.x0) + float(pd.x1 - pd.x0 + 1) * 0.5f) * kTilePx - camX, top = float(pd.row) * kTilePx - camY;
      DrawOpts o;
      o.scale = float(pd.x1 - pd.x0 + 1) * 64.0f / 128.0f;
      r.draw(pad, cx, top - 4.0f, o);
    }
    return;
  }
  const int bw = mLevel->width;
  const Spans sp = spansOf(s);
  int stage = 0;
  if (!s.votes.empty())
    stage = (s.votes[0].y + 1) / kCellsPerTile;
  // Seats along the tiers, behind the audience.
  if (sp.standX1 > sp.standX0)
  {
    const Texture& seat = baked(mArt, r, "seat", 96, 96, 48.0f, 96.0f, paintSeat);
    for (int cx = sp.standX0 - 3; cx <= sp.standX1 + 3; cx += 3)
    {
      const int bx = (cx + 1) / kCellsPerTile;
      if (bx < 0 || bx >= bw)
        continue;
      int top = -1;
      for (int by = 0; by < mLevel->height; ++by)
        if (mMap.block(bx, by) == Tile::Solid)
        {
          top = by;
          break;
        }
      if (top < 0 || top >= stage)
        continue;
      const float x = (float(cx) + 1.5f) * kCellPx - camX, y = float(top) * kTilePx - camY;
      if (x < -60.0f || x > float(kScreenW) + 60.0f)
        continue;
      bool broken = false; // the hollow's seat: drawn below, or gone
      for (const auto& b : mBreakables)
        broken = broken || (bx >= b.x0 && bx <= b.x1 && top >= b.y0 - 1 && top <= b.y1);
      if (!broken)
        r.draw(seat, x, y + 2.0f);
    }
  }
  // G's seat, over the hollow: a big plush one (it breaks).
  for (const auto& b : mBreakables)
  {
    if (b.broken)
      continue;
    const float x = float(b.x0) * kTilePx - camX, y = float(b.y0) * kTilePx - camY;
    const float wpx = float(b.x1 - b.x0 + 1) * kTilePx, hpx = float(b.y1 - b.y0 + 1) * kTilePx;
    if (offScreen(x, y, wpx, hpx))
      continue;
    const Texture& seat = baked(mArt, r, "seat", 96, 96, 48.0f, 96.0f, paintSeat);
    DrawOpts o;
    o.scale = wpx / 96.0f;
    r.draw(seat, x + wpx * 0.5f, y + hpx, o);
  }
  // The host's desk: the short run of ledge before the first vote.
  if (!s.votes.empty())
  {
    const int limit = s.votes[0].x / kCellsPerTile;
    for (int by = std::max(0, stage - 3); by < stage; ++by)
    {
      int run0 = -1;
      for (int bx = 0; bx <= limit; ++bx)
      {
        const bool plat = mMap.block(bx, by) == Tile::Platform;
        if (plat && run0 < 0)
          run0 = bx;
        if ((!plat || bx == limit) && run0 >= 0)
        {
          const int run1 = plat ? bx : bx - 1;
          const float x = float(run0) * kTilePx - camX, y = float(by) * kTilePx - camY;
          const float wpx = float(run1 - run0 + 1) * kTilePx;
          if (!offScreen(x, y, wpx, 200.0f))
          {
            DrawOpts o;
            o.scale = (wpx + 32.0f) / 352.0f;
            r.draw(baked(mArt, r, "desk", 352, 160, 176.0f, 24.0f, [](cairo_t* cr) {
              cairo_translate(cr, 0, 22);
              paintDesk(cr);
            }),
              x + wpx * 0.5f, y - 2.0f, o);
          }
          run0 = -1;
        }
      }
    }
  }
  // The arenas: brass stanchions at the corners, gold tape across the
  // floor, a marquee sign over each.
  for (const auto& ar : s.arenas)
  {
    const float x0 = float(ar.x) * kCellPx - camX, x1 = float(ar.x + ar.w) * kCellPx - camX;
    const float floorY = float(ar.y + ar.h) * kCellPx - camY;
    if (x1 < -200.0f || x0 > float(kScreenW) + 200.0f)
      continue;
    for (float x = std::max(x0, -40.0f); x < std::min(x1, float(kScreenW) + 40.0f); x += 48.0f)
      if ((int(std::floor((x - x0) / 48.0f)) % 2) == 0)
        r.fillRect(x, floorY - 3.0f, 28.0f, 3.0f, rgba(255, 210, 90, 200), Blend::Add);
    const Texture& post = baked(mArt, r, "stanchion", 24, 80, 12.0f, 80.0f, paintStanchion);
    for (const float px : {x0 + 12.0f, x1 - 12.0f})
    {
      r.draw(post, px, floorY);
      drawGlow(r, mArt, px, floorY - 72.0f, 16.0f, rgb(255, 220, 140), 0.6f + 0.3f * float((frame / 10) % 2));
    }
    bool turn = false;
    for (const auto& t : s.turntables)
      turn = turn || (t.x0 * kCellsPerTile >= ar.x && t.x0 * kCellsPerTile < ar.x + ar.w);
    const std::string words = turn ? "THE TURNTABLE STAGE" : "THE CHICKEN RUN";
    const int sw = turn ? 420 : 340;
    const Texture& sign = baked(mArt, r, turn ? "sign_turn" : "sign_chicken", sw, 72, float(sw) * 0.5f, 0.0f,
      [sw, words](cairo_t* cr) { paintSign(cr, sw, words); });
    const float sx = (x0 + x1) * 0.5f, sy = float(ar.y) * kCellPx - camY - 8.0f;
    r.drawLine(sx - float(sw) * 0.35f, sy - 400.0f, sx - float(sw) * 0.35f, sy + 4.0f, 2.0f, rgba(40, 30, 34, 255));
    r.drawLine(sx + float(sw) * 0.35f, sy - 400.0f, sx + float(sw) * 0.35f, sy + 4.0f, 2.0f, rgba(40, 30, 34, 255));
    r.draw(sign, sx, sy);
  }
  // The vote screens over the checkpoints: idle (the meter resting at its
  // 42 dB), the ballot (the needle swinging over the three options), the
  // winner, then THANK YOU.
  const Texture& jumbo = baked(mArt, r, "jumbo", 1088, 320, 0.0f, 0.0f, paintJumbo);
  const Texture& bulbGlow = baked(mArt, r, "bulbglow", 24, 24, 12.0f, 12.0f, paintBulbGlow);
  const Texture& dial = baked(mArt, r, "dial", 400, 230, 200.0f, 200.0f, paintDial);
  const Texture& wedge = baked(mArt, r, "wedge", 400, 210, 200.0f, 200.0f, paintWedge);
  const Texture& needle = baked(mArt, r, "needle", 24, 190, 12.0f, 178.0f, paintNeedle);
  for (std::size_t i = 0; i < s.votes.size(); ++i)
  {
    const auto& v = s.votes[i];
    const CellBox scr{v.x - 14, v.y - 16, 32, 8};
    const float sx = float(scr.x) * kCellPx - camX, sy = float(scr.y) * kCellPx - camY;
    if (offScreen(sx - 32.0f, sy - 400.0f, 1088.0f, 720.0f))
      continue;
    // Its hanging cables and the jumbotron.
    r.drawLine(sx + 120.0f, sy - 32.0f, sx + 120.0f, -20.0f, 3.0f, rgb(30, 22, 26));
    r.drawLine(sx + 904.0f, sy - 32.0f, sx + 904.0f, -20.0f, 3.0f, rgb(30, 22, 26));
    r.draw(jumbo, sx - 32.0f, sy - 32.0f);
    const bool up = s.ballot == int(i);
    const bool settled = up && s.ballotT >= kSwSwing;
    // The bulbs chase round (faster during a ballot; all on for the winner).
    const int speed = up ? 2 : 6;
    int n = 0;
    auto bulbAt = [&](float bx, float by) {
      const bool on = settled ? (frame / 4) % 2 == 0 : (n + frame / speed) % 3 != 0;
      ++n;
      if (on)
      {
        DrawOpts o;
        o.blend = Blend::Add;
        r.draw(bulbGlow, bx, by, o);
      }
    };
    for (float x = 16.0f; x < 1080.0f; x += 32.0f)
      bulbAt(sx - 32.0f + x, sy - 16.0f);
    for (float y = 48.0f; y < 290.0f; y += 32.0f)
      bulbAt(sx + 1040.0f, sy - 32.0f + y);
    for (float x = 1072.0f; x > 8.0f; x -= 32.0f)
      bulbAt(sx - 32.0f + x, sy + 272.0f);
    for (float y = 272.0f; y > 40.0f; y -= 32.0f)
      bulbAt(sx - 16.0f, sy - 32.0f + y);
    // The dial on the left of the screen.
    const float dx = sx + 230.0f, dy = sy + 222.0f;
    DrawOpts dop;
    dop.scale = 0.95f;
    r.draw(dial, dx, dy, dop);
    float needleAngle = -90.0f + 180.0f * 42.0f / 120.0f; // 42 dB at rest
    if (up)
    {
      for (int k = 0; k < 3; ++k)
      {
        DrawOpts wo;
        wo.scale = 0.95f;
        wo.angle = float(k - 1) * 60.0f;
        wo.tint = catColor(s.options[k].cat);
        wo.alpha = settled && k != s.winner ? 0.35f : 0.9f;
        r.draw(wedge, dx, dy, wo);
      }
      needleAngle = (needleAt(s) - 1.0f) * 60.0f;
    }
    else if (v.done)
      needleAngle = 60.0f + 6.0f * std::sin(float(frame) * 0.2f); // still riding the applause
    DrawOpts no;
    no.angle = needleAngle;
    no.scale = 0.95f;
    no.cull = false;
    r.draw(needle, dx, dy, no);
    if (!up && !v.done)
      r.drawText("42 dB", dx, dy - 70.0f, {22.0f, rgb(150, 20, 40), 0}, Align::Center); // the resting reading
    // The words on the right.
    const float tx = sx + 680.0f, ty = sy + 30.0f;
    const bool blink = (frame / 10) % 2 == 0;
    if (up)
    {
      // The three options, a row each, the one the needle is over lit;
      // once it settles, the winner glowing and the others dimmed.
      r.drawText(settled ? "THE AUDIENCE HAS SPOKEN" : "VOTE NOW!", tx, ty - 8.0f,
        {settled ? 30.0f : 40.0f, settled || blink ? kGoldHi : rgb(255, 140, 60), kHudInk}, Align::Center);
      const int over = std::clamp(int(std::lround(needleAt(s))), 0, 2);
      for (int k = 0; k < 3; ++k)
      {
        const SwOption& o = s.options[k];
        const Color c = catColor(o.cat);
        const float ry = ty + 48.0f + float(k) * 58.0f, rx = tx - 255.0f;
        const bool lit = settled ? k == s.winner : k == over;
        r.fillRect(rx, ry, 520.0f, 52.0f, lit ? withAlpha(c, settled ? 200 : 120) : rgba(40, 24, 40, 200));
        r.fillRect(rx, ry, 10.0f, 52.0f, c);
        r.drawText(catName(o.cat), rx + 24.0f, ry + 4.0f, {14.0f, lit ? rgb(255, 255, 255) : c, 0}, Align::Left);
        r.drawText(sweepsOptionName(o), rx + 24.0f, ry + 20.0f, {24.0f, settled && !lit ? rgb(140, 120, 130) : rgb(255, 255, 255), kHudInk},
          Align::Left);
      }
    }
    else if (v.done)
    {
      r.drawText("THANK YOU!", tx, ty + 30.0f, {64.0f, kGoldHi, kHudInk}, Align::Center);
      r.drawText("ENJOY THE SEGMENT", tx, ty + 120.0f, {24.0f, rgb(255, 210, 160), kHudInk}, Align::Center);
    }
    else
    {
      r.drawText(v.fanfare ? "ENCORE VOTE" : "AUDIENCE VOTE", tx, ty + 30.0f,
        {60.0f, blink ? kGoldHi : rgb(255, 150, 70), kHudInk}, Align::Center);
      r.drawText(v.fanfare ? "PICK THE EXIT FANFARE" : "THIS WAY, RUNNER!", tx, ty + 120.0f, {24.0f, rgb(255, 210, 160), kHudInk},
        Align::Center);
    }
  }
  // Turntables: the disc's rim in the floor, chevrons running the way it
  // turns, red before each turn.
  const Texture& chevron = baked(mArt, r, "chevron", 24, 16, 12.0f, 8.0f, paintChevron);
  for (const auto& t : s.turntables)
  {
    const int wpx = (t.x1 - t.x0 + 1) * 64;
    const float x = float(t.x0) * kTilePx - camX, y = float(t.row) * kTilePx - camY;
    if (offScreen(x, y, float(wpx), 40.0f))
      continue;
    const Texture& rim = baked(mArt, r, "turntable" + std::to_string(wpx), wpx, 40, 0.0f, 8.0f,
      [wpx](cairo_t* cr) { paintTurntable(cr, wpx); });
    r.draw(rim, x, y + 2.0f);
    const int dir = t.dirAt(s.clock);
    const bool warn = kSwTurnFlip - s.clock % kSwTurnFlip <= kSwTurnWarn;
    const Color c = warn ? ((frame / 3) % 2 ? rgb(255, 60, 50) : rgb(120, 20, 20)) : rgb(255, 210, 90);
    const float run = float(frame % 32) * 0.75f * float(dir);
    Clip clip(r, x + 16.0f, y, float(wpx) - 32.0f, 40.0f);
    for (float ax = x - 24.0f; ax < x + float(wpx) + 24.0f; ax += 24.0f)
    {
      DrawOpts o;
      o.tint = c;
      o.blend = Blend::Add;
      o.angle = dir < 0 ? 180.0f : 0.0f;
      r.draw(chevron, ax + std::fmod(run + 240.0f, 24.0f), y + 14.0f, o);
    }
  }
  // The audience: clapping, the cameos turning their heads after the runner.
  const int clapRate = s.ballot >= 0 ? 3 : 6;
  for (std::size_t i = 0; i < s.crowd.size(); ++i)
  {
    const auto& c = s.crowd[i];
    const float x = (float(c.x) + 1.5f) * kCellPx - camX, foot = float(c.y + 1) * kCellPx - camY;
    if (offScreen(x - 60.0f, foot - 200.0f, 120.0f, 200.0f))
      continue;
    const int rel = p.x + 1 - (c.x + 1);
    int side = 0, dir = 1;
    if (c.look != 0)
    {
      side = std::abs(rel) > 2 ? 1 : 0;
      dir = rel < 0 ? -1 : 1;
    }
    else
    {
      const std::uint32_t h = hash2(int(i) * 31 + frame / 90, 41);
      side = h % 3u == 0 ? 1 : 0;
      dir = (h >> 4) % 2u ? 1 : -1;
    }
    const int variant = c.look * 16 + int(hash2(c.x, c.y) % 4u) * 2 + side;
    const int clap = (frame / clapRate + int(i)) % 2;
    r.draw(styledEnemySprite(mArt, r, mTheme, "crowd", variant, clap, 3, 5).get(dir), x, foot);
  }
}

void World::drawSweepsFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& s = mSweeps;
  const auto& p = mPlayer;
  if (s.paint)
  {
    // Paint on every block the runner (or a shot) touched: splats on the
    // floor tops dripping down their fronts, splats on the walls; the tops
    // still to paint outlined, pulsing.
    const int w = mLevel->width;
    const Color paint = runnerPaint(mCharacterIndex);
    const int bx0 = std::max(0, int(camX / kTilePx) - 1), by0 = std::max(0, int(camY / kTilePx) - 1);
    for (int by = by0; by < std::min(mLevel->height, by0 + 14); ++by)
      for (int bx = bx0; bx < std::min(w, bx0 + 23); ++bx)
      {
        const int c = s.paints[std::size_t(by * w + bx)];
        if (c == 0 || c == 3)
          continue;
        const float x = float(bx) * kTilePx - camX, y = float(by) * kTilePx - camY;
        const std::uint32_t h = hash2(bx, by);
        if (c == 2)
        {
          const int v = int(h % 3u);
          const Texture& splat = baked(mArt, r, "splat_top" + std::to_string(v), 64, 64, 32.0f, 11.0f,
            [v](cairo_t* cr) { paintSplat(cr, v, true); });
          DrawOpts o;
          o.tint = paint;
          r.draw(splat, x + 32.0f, y + 2.0f, o);
        }
        else if (c == 4)
        {
          const int v = 3 + int(h % 3u);
          const Texture& splat = baked(mArt, r, "splat_wall" + std::to_string(v), 64, 64, 32.0f, 32.0f,
            [v](cairo_t* cr) { paintSplat(cr, v, false); });
          DrawOpts o;
          o.tint = paint;
          o.angle = float(h % 360u);
          r.draw(splat, x + 32.0f, y + 32.0f, o);
        }
        else if (c == 1)
        {
          const int a = 70 + int(50.0f * (0.5f + 0.5f * std::sin(float(frame) * 0.12f + float(bx) * 0.5f)));
          for (float dx = 4.0f; dx < 60.0f; dx += 16.0f)
            r.fillRect(x + dx, y - 2.0f, 10.0f, 3.0f, rgba(255, 255, 255, a), Blend::Add);
        }
      }
    return;
  }
  const bool big = (s.rules & kSwBigHeads) != 0, quack = (s.rules & kSwQuack) != 0;
  const Texture& beak = baked(mArt, r, "beak", 40, 28, 2.0f, 14.0f, paintBeak);
  for (const auto& a : s.actors)
  {
    const Enemy& e = mEnemies[std::size_t(a.enemy)];
    if (!e.alive || e.hidden)
      continue;
    const float cx = (e.drawX + float(e.w) * 0.5f) * kCellPx - camX, foot = (e.drawY + 1.0f) * kCellPx - camY;
    if (offScreen(cx - 140.0f, foot - 300.0f, 280.0f, 320.0f))
      continue;
    const int dir = e.dir < 0 ? -1 : 1;
    DrawOpts fo;
    fo.blend = Blend::Add;
    fo.alpha = float(e.flash) / 8.0f;
    if (a.popper)
    {
      // ???: a party popper where the enemy stood, waiting to go off.
      const Texture& pop = baked(mArt, r, "popper", 96, 160, 48.0f, 160.0f, paintPopper);
      const float wob = std::sin(float(frame) * 0.2f + float(a.enemy)) * 3.0f;
      DrawOpts o;
      o.angle = wob;
      r.draw(pop, cx, foot, o);
      if (e.flash > 0)
        r.draw(pop, cx, foot, fo);
      continue;
    }
    float headX = cx + float(dir) * 2.0f, headTop = foot - 156.0f, beakX = cx + float(dir) * 13.0f, beakY = foot - 130.0f;
    float headScale = 1.0f;
    if (e.kind == EnemyKind::WarmupComic)
    {
      const int variant = (e.tell > 0 ? 1 : 0) | (big ? 2 : 0);
      const Sprite& sp = styledEnemySprite(mArt, r, mTheme, "comic", variant, (frame / 8) % 2, e.w, e.h);
      r.draw(sp.get(dir), cx, foot);
      if (e.flash > 0)
        r.draw(sp.get(dir), cx, foot, fo);
      if (e.tell > 0)
      {
        // Ba-dum-tss: the rimshot's words popping up over him.
        const float pop = 1.0f + 0.15f * std::sin(float(frame) * 0.6f);
        r.drawText("BA-DUM-TSS!", cx, foot - (big ? 250.0f : 206.0f), {24.0f * pop, kGoldHi, kHudInk, true}, Align::Center);
      }
    }
    else if (e.kind == EnemyKind::AudiencePlant)
    {
      const int pose = a.stand == 0 ? 0 : (e.tell > 0 ? 1 : 2);
      const Texture& seat = baked(mArt, r, "seat", 96, 96, 48.0f, 96.0f, paintSeat);
      if (pose < 2)
        r.draw(seat, cx, foot + 2.0f);
      const Sprite& sp = styledEnemySprite(mArt, r, mTheme, "audience_plant", pose | (big ? 4 : 0), (frame / 20) % 2, e.w, e.h);
      r.draw(sp.get(dir), cx, foot);
      if (e.flash > 0)
        r.draw(sp.get(dir), cx, foot, fo);
      if (pose == 0)
      {
        // The armrests and the cushion's front edge in front of him.
        Clip clip(r, cx - 60.0f, foot - 48.0f, 120.0f, 50.0f);
        r.draw(seat, cx, foot + 2.0f);
      }
      const float drop = pose == 0 ? 34.0f : pose == 1 ? 16.0f : 0.0f;
      headTop += drop;
      beakY += drop;
      if (e.tell > 0)
        r.drawText("!", cx, headTop - (big ? 70.0f : 44.0f), {44.0f, rgb(255, 80, 60), kHudInk}, Align::Center);
    }
    else if (e.kind == EnemyKind::CueDrone)
    {
      const Sprite& sp = styledEnemySprite(mArt, r, mTheme, "cuedrone", a.swoop == 1 ? 1 : 0, (frame / 2) % 2, e.w, e.h);
      const float bob = std::sin(float(frame) * 0.15f + float(a.enemy)) * 4.0f;
      r.draw(sp.get(dir), cx, foot + bob);
      if (e.flash > 0)
        r.draw(sp.get(dir), cx, foot + bob, fo);
      // The card, hung from its arms; flipping over before a swoop.
      const int card = a.card % 3;
      const Texture& tex = baked(mArt, r, std::string("card") + std::to_string(card), 160, 60, 80.0f, 0.0f,
        [card](cairo_t* cr) { paintCard(cr, kCards[card]); });
      float sx = 1.0f;
      if (a.flip > 0)
        sx = std::abs(std::cos(float(a.flip * 4 + frame % 4) * 0.4f));
      const float cw = 150.0f * std::max(0.08f, sx), top = foot + bob - 30.0f;
      blit(r, tex, cx - cw * 0.5f, top, cw, 56.0f, 0.0f, 0.0f, 0.0f, 1.0f, Blend::Alpha);
      headX = cx;
      headTop = foot + bob - 84.0f;
      beakX = cx + float(dir) * 8.0f;
      beakY = foot + bob - 56.0f;
    }
    if (big && e.kind != EnemyKind::CueDrone)
    {
      // The head scaled 1.75 about the neck (42 px under its top).
      headScale = 1.75f;
      const float neck = headTop + 42.0f;
      headTop = neck - 42.0f * headScale;
      beakX = cx + float(dir) * 23.0f;
      beakY = neck - 28.0f;
    }
    if (a.hat)
    {
      const int hv = a.enemy % 2;
      const Texture& hat = baked(mArt, r, "hat" + std::to_string(hv), 48, 64, 24.0f, 60.0f, [hv](cairo_t* cr) {
        paintHat(cr, hv ? rgb(255, 70, 160) : rgb(70, 200, 255), rgb(255, 230, 80));
      });
      DrawOpts o;
      o.scale = headScale;
      o.angle = float(dir) * 12.0f;
      r.draw(hat, headX, headTop + 8.0f * headScale, o);
    }
    if (quack)
    {
      DrawOpts o;
      o.scale = headScale;
      if (dir < 0)
        r.draw(baked(mArt, r, "beak_l", 40, 28, 38.0f, 14.0f, [](cairo_t* cr) {
          cairo_translate(cr, 40, 0);
          cairo_scale(cr, -1, 1);
          paintBeak(cr);
        }),
          beakX, beakY, o);
      else
        r.draw(beak, beakX, beakY, o);
    }
  }
  // BIG HEADS on the runner too.
  if (sweepsHeadLens())
  {
    const float px = lerpCells(p.prevX, p.x, alpha) + 1.5f * kCellPx - camX;
    const float foot = lerpCells(p.prevY + 1, p.y + 1, alpha) - camY;
    bigHeadLens(r, px, foot - float(p.height()) * kCellPx, 1.55f);
  }
  // Rubber chickens: spinning end over end in flight, limp on the floor.
  const Texture& chick = baked(mArt, r, "chicken", 72, 48, 36.0f, 26.0f, paintChicken);
  for (std::size_t i = 0; i < s.chickens.size(); ++i)
  {
    const auto& c = s.chickens[i];
    const float x = (c.x + 1.0f) * kCellPx - camX, y = (c.y + 1.0f) * kCellPx - camY;
    if (offScreen(x - 40.0f, y - 40.0f, 80.0f, 80.0f))
      continue;
    DrawOpts o;
    o.cull = false;
    if (c.still > 0)
      o.angle = c.vx < 0.0f ? -10.0f : 10.0f;
    else
      o.angle = float(frame * 14 + int(i) * 60) * (c.vx < 0.0f ? -1.0f : 1.0f);
    r.draw(chick, x, y + (c.still > 0 ? 14.0f : 0.0f), o);
  }
  // The V of VIRUS FOR THE RUNNER, glowing sick green.
  for (const auto& it : mItems)
    if (!it.taken && it.kind == ItemKind::Virus && it.variant == 7)
    {
      const float x = (float(it.x) + 1.0f) * kCellPx - camX, y = (float(it.y) - 0.5f) * kCellPx - camY;
      drawGlow(r, mArt, x, y, 90.0f + 10.0f * std::sin(float(frame) * 0.2f), rgb(120, 255, 60), 0.6f);
    }
}

// BIG HEADS puts its lens on the runner this frame (drawSweepsFront): alive,
// in sight, upright and not mid-somersault. Asked again before the scene
// (beginSweepsScene); nothing it reads changes while the world is drawn.
bool World::sweepsHeadLens() const
{
  const auto& p = mPlayer;
  return (mSweeps.rules & kSwBigHeads) != 0 && !mSweeps.paint && !p.hidden && p.state != PlayerState::Dying &&
    p.visual != PlayerVisual::Dying && p.visual != PlayerVisual::Somersault && p.grav == Grav::Down;
}

// Before the scene: MIRROR MODE flips all of it and BIG HEADS' lens
// magnifies a piece of it, so it is drawn into the renderer's offscreen
// capture instead of reading the screen back from the GPU (a pipeline stall
// every frame). Only on frames that need it: BIG HEADS is Level 41's rule
// most of the time, and its lens is off while the runner dies, hides,
// somersaults or walks the ceiling. (A lens refused at the picture's edge
// still costs the capture: that is known only when the head is placed.)
// Without SDL_RenderGeometry (SDL older than 2.0.18) the lens is read back
// whatever the target, so it begins no capture of its own.
void World::beginSweepsScene(Renderer& r) const
{
#if SDL_VERSION_ATLEAST(2, 0, 18)
  const bool lensFromCapture = sweepsHeadLens();
#else
  const bool lensFromCapture = false;
#endif
  if (sweepsMirrored() || lensFromCapture)
    r.beginCapture();
}

void World::drawSweepsTop(Renderer& r, float camX, float camY, int frame) const
{
  const auto& s = mSweeps;
  const auto& p = mPlayer;
  if ((s.rules & kSwLightsOut) != 0)
  {
    // LIGHTS OUT: dark but for a followspot on the runner, warm at its edge.
    constexpr int cols = 41, rows = 24;
    std::vector<float> a(std::size_t(cols * rows));
    const float px = (float(p.x) + 1.5f) * kCellPx - camX, py = (float(p.y) - 2.0f) * kCellPx - camY;
    const float rad = float(kSwLight) * kCellPx;
    for (int y = 0; y < rows; ++y)
      for (int x = 0; x < cols; ++x)
      {
        const float dx = float(x) * kCellPx - px, dy = float(y) * kCellPx - py;
        const float d = std::sqrt(dx * dx + dy * dy) / rad;
        a[std::size_t(y * cols + x)] = std::clamp((d - 0.7f) / 0.3f, 0.0f, 1.0f) * 0.94f;
      }
    r.drawAlphaGrid(a, cols, rows, 0.0f, 0.0f, kCellPx, kCellPx, rgb(0, 0, 0));
    drawGlow(r, mArt, px, py, rad * 0.9f, rgb(255, 220, 160), 0.12f + 0.02f * std::sin(float(frame) * 0.1f));
  }
  // The scene back on the screen, flipped left to right for MIRROR MODE; the
  // HUD goes over it the right way round. Without a capture (the renderer
  // refused it) the scene is on the screen already, and MIRROR MODE flips it
  // there, read back from the GPU: the controls are swapped, so the picture
  // must be too.
  const bool captured = r.capturing();
  r.endCapture(sweepsMirrored());
  if (!captured && sweepsMirrored())
    r.mirrorReadBack();
}

void World::drawSweepsHud(Renderer& r, int frame) const
{
  const auto& s = mSweeps;
  char buf[96];
  const float W = float(kScreenW);
  if (s.paint)
  {
    // Paintball: the share painted, a bar in the runner's colour, the goal
    // marked in gold.
    const int pct = s.tops > 0 ? s.painted * 100 / s.tops : 0;
    const Color paint = runnerPaint(mCharacterIndex);
    const float x = W - 300.0f, y = 92.0f;
    r.fillRect(x, y, 284.0f, 64.0f, rgba(10, 6, 12, 200));
    r.fillRect(x, y, 284.0f, 3.0f, kGold);
    std::snprintf(buf, sizeof(buf), "PAINTED %d%%", pct);
    r.drawText(buf, x + 14.0f, y + 8.0f, {20.0f, lighten(paint, 0.3f), kHudInk}, Align::Left);
    std::snprintf(buf, sizeof(buf), "GOAL %d%%", s.goal);
    r.drawText(buf, x + 270.0f, y + 10.0f, {16.0f, kGoldHi, 0}, Align::Right);
    const float bx = x + 14.0f, by = y + 38.0f, bwid = 256.0f;
    r.fillRect(bx, by, bwid, 14.0f, rgb(30, 22, 30));
    r.fillRect(bx, by, bwid * float(std::min(pct, 100)) / 100.0f, 14.0f, paint);
    r.fillRect(bx, by, bwid * float(std::min(pct, 100)) / 100.0f, 4.0f, lighten(paint, 0.4f));
    r.fillRect(bx + bwid * float(s.goal) / 100.0f - 1.5f, by - 4.0f, 3.0f, 22.0f, kGoldHi);
    return;
  }
  // The Applause Cannon's meter: four VU steps, the streak's hits toward
  // the next one as pips under them.
  {
    const float x = W - 300.0f, y = 92.0f;
    r.fillRect(x, y, 284.0f, 64.0f, rgba(14, 6, 12, 205));
    r.fillRect(x, y, 284.0f, 3.0f, kGold);
    const bool hot = s.meter >= kSwMeterMax;
    r.drawText("APPLAUSE", x + 14.0f, y + 8.0f, {18.0f, hot && (frame / 4) % 2 ? rgb(255, 255, 255) : kGoldHi, 0}, Align::Left);
    std::snprintf(buf, sizeof(buf), "x%d", s.meter);
    r.drawText(buf, x + 14.0f, y + 30.0f, {24.0f, hot ? rgb(255, 120, 160) : kGold, kHudInk}, Align::Left);
    static constexpr Color kSteps[kSwMeterMax] = {rgb(90, 220, 120), rgb(220, 230, 80), rgb(255, 160, 50), rgb(255, 70, 110)};
    for (int k = 0; k < kSwMeterMax; ++k)
    {
      const float sx = x + 150.0f + float(k) * 32.0f, h = 14.0f + float(k) * 8.0f;
      const bool on = k < s.meter;
      r.fillRect(sx, y + 50.0f - h, 24.0f, h, on ? kSteps[k] : rgb(50, 36, 40));
      if (on)
        r.fillRect(sx, y + 50.0f - h, 24.0f, 3.0f, rgb(255, 255, 255));
    }
    if (s.meter < kSwMeterMax)
      for (int k = 0; k < kSwHitsPerStep; ++k)
        r.fillRect(x + 150.0f + float(k) * 32.0f + 8.0f, y + 55.0f, 8.0f, 5.0f,
          k < s.streak % kSwHitsPerStep ? kGoldHi : rgb(70, 50, 50));
  }
  // The rule in force, on a lower-left tag.
  if (s.ballot < 0 && s.rules != 0)
  {
    SwOption o;
    o.rules = s.rules;
    const std::string name = sweepsOptionName(o);
    const float x = 16.0f, y = 140.0f;
    const float tw = std::max(220.0f, 26.0f + 11.0f * float(name.size()));
    r.fillRect(x, y, tw, 52.0f, rgba(14, 6, 12, 205));
    r.fillRect(x, y, 5.0f, 52.0f, rgb(255, 60, 90));
    r.drawText("TONIGHT'S RULE", x + 16.0f, y + 6.0f, {14.0f, rgb(255, 120, 140), 0}, Align::Left);
    r.drawText(name, x + 16.0f, y + 24.0f, {20.0f, kGoldHi, 0}, Align::Left);
  }
  if (s.ballot < 0)
    return;
  {
    // The ballot is on the vote's big screen while that is in view.
    const auto& v = s.votes[std::size_t(s.ballot)];
    if (isOnScreen({v.x - 6, v.y - 16, 16, 8}, 0))
      return;
  }
  // Out of its sight: the ballot on the HUD, three option cards, the needle's pointer sliding over them.
  const float cx = W * 0.5f, y = 168.0f;
  const bool settled = s.ballotT >= kSwSwing;
  r.fillRect(cx - 490.0f, y, 980.0f, 128.0f, rgba(14, 6, 16, 215));
  r.fillRect(cx - 490.0f, y, 980.0f, 3.0f, kGold);
  r.fillRect(cx - 490.0f, y + 125.0f, 980.0f, 3.0f, kGold);
  if (s.options[0].cat == SwCat::Fanfare)
    std::snprintf(buf, sizeof(buf), "ENCORE! THE AUDIENCE PICKS YOUR EXIT FANFARE");
  else
    std::snprintf(buf, sizeof(buf), "THE AUDIENCE VOTES  -  STYLE %d", s.style);
  r.drawText(buf, cx, y + 8.0f, {20.0f, rgb(255, 245, 225), 0}, Align::Center);
  for (int k = 0; k < 3; ++k)
  {
    const float ox = cx + float(k - 1) * 324.0f;
    const bool won = settled && k == s.winner;
    const Color c = catColor(s.options[k].cat);
    const float lift = won ? 4.0f * std::sin(float(frame) * 0.3f) : 0.0f;
    r.fillRect(ox - 150.0f, y + 40.0f - lift, 300.0f, 72.0f, won ? withAlpha(c, 230) : rgba(40, 24, 36, 230));
    r.fillRect(ox - 150.0f, y + 40.0f - lift, 300.0f, 4.0f, c);
    if (settled && !won)
      r.fillRect(ox - 150.0f, y + 40.0f, 300.0f, 72.0f, rgba(0, 0, 0, 120));
    r.drawText(catName(s.options[k].cat), ox, y + 48.0f - lift, {14.0f, won ? kHudInk : c, 0}, Align::Center);
    const std::string name = sweepsOptionName(s.options[k]);
    const float size = name.size() > 20 ? 17.0f : 21.0f;
    r.drawText(name, ox, y + 70.0f - lift, {size, rgb(255, 255, 255), kHudInk}, Align::Center);
    if (won)
      for (int j = 0; j < 6; ++j)
      {
        const float a = float(j) * 1.047f + float(frame) * 0.1f;
        drawGlow(r, mArt, ox + std::cos(a) * 160.0f, y + 76.0f + std::sin(a) * 44.0f, 22.0f, c, 0.6f);
      }
  }
  const float nx = cx + (needleAt(s) - 1.0f) * 324.0f;
  r.drawLine(nx - 12.0f, y + 30.0f, nx, y + 42.0f, 4.0f, kGoldHi);
  r.drawLine(nx + 12.0f, y + 30.0f, nx, y + 42.0f, 4.0f, kGoldHi);
  r.fillRect(nx - 2.0f, y + 24.0f, 4.0f, 10.0f, kGoldHi);
}

// --- Shots and the icon ----------------------------------------------------------------------------

bool drawSweepsShot(Renderer& r, const World& w, const Art& art, const Projectile& pr, float cx, float cy, int frame)
{
  const auto& s = w.sweeps();
  if (!s.on)
    return false;
  if (pr.kind == ShotKind::Enemy)
  {
    // An Audience Plant's tomato, tumbling.
    const Texture& tom = baked(art, r, "tomato", 32, 32, 16.0f, 16.0f, paintTomato);
    DrawOpts o;
    o.angle = float(frame * 18 + pr.age * 30);
    o.scale = 1.4f;
    r.draw(tom, cx, cy, o);
    return true;
  }
  if (pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::ApplauseCannon))
  {
    // An Applause Cannon burst: bigger and hotter as the meter climbs.
    const Texture& burst = baked(art, r, "clapburst", 96, 96, 48.0f, 48.0f, paintClapBurst);
    const float m = float(std::clamp(pr.damage, 1, 4));
    DrawOpts o;
    o.scale = 0.42f + 0.1f * m;
    o.angle = float(frame * 9);
    if (m >= 4.0f)
      o.tint = rgb(255, 170, 210);
    r.draw(burst, cx, cy, o);
    const float back = pr.dx < 0 ? 1.0f : -1.0f;
    for (int k = 1; k <= 3; ++k)
      r.fillRect(cx + back * (14.0f + float(k) * 12.0f) - 6.0f, cy - 9.0f + float(k * 5 % 18), 12.0f, 3.0f,
        rgba(255, 230, 150, 200 - k * 50), Blend::Add);
    return true;
  }
  if (s.paint && pr.kind != ShotKind::Proto)
  {
    // Paintball: the runner's shots are paintballs.
    const Color c = runnerPaint(w.characterIndex());
    drawGlow(r, art, cx, cy, 18.0f, c, 0.6f);
    const Texture& ball = baked(art, r, "paintball", 24, 24, 12.0f, 12.0f, [](cairo_t* cr) {
      circle(cr, 12, 12, 9);
      vgrad(cr, 3, 21, {{0.0, rgb(255, 255, 255)}, {1.0, rgb(170, 170, 170)}}, 1.0, true);
      stroke(cr, kInk, 1.4);
      ellipse(cr, 9, 8, 3, 2);
      setRgba(cr, rgb(255, 255, 255), 0.9);
      cairo_fill(cr);
    });
    DrawOpts o;
    o.tint = c;
    r.draw(ball, cx, cy, o);
    return true;
  }
  return false;
}

void drawApplauseCannonIcon(Renderer& r, const Art& art, float x, float y, float scale, int frame)
{
  const Texture& icon = baked(art, r, "cannonicon", 64, 64, 0.0f, 0.0f, paintCannonIcon);
  DrawOpts o;
  o.scale = scale;
  r.draw(icon, x, y, o);
  // A sparkle popping at the mouth.
  const float ph = float(frame % 30) / 30.0f;
  drawGlow(r, art, x + (58.0f + ph * 6.0f) * scale, y + (18.0f - ph * 10.0f) * scale, (8.0f + ph * 6.0f) * scale, rgb(255, 230, 140),
    0.8f * (1.0f - ph));
}

} // namespace gr
