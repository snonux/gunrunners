// Level 39, The Gallery: drawing. See world_gallery.cpp for the logic.
//
// Behind the tiles (drawGallerySet, gallery_draw.hpp), keyed to the map:
// each room's furniture (the edit suite's edit monitors, the hallway's
// studio doors and posters, studio B's news set and cameras, the cable
// corridor's wall trays and catwalk railings, the panning room's jib, the
// tape library's shelving, the main gallery's vision mixer and its walls
// of screens, the forgotten studio's dusty sitcom set, the virus room's
// corrupted screen).
//
// Over the tiles, behind the runner (drawGalleryBack): the mixing desk
// under its one-way top, the library's shelf planks, the shutter, the
// locked door, the sealed booth, the dolly track, the light rig's truss,
// the CAM 0 display with its living room (and the live game on that
// room's TV), the ON AIR sign, the candid tapes, and the studio monitors:
// a bezel and a cart (or a wall bracket), the label on the strip under the
// screen, the tally lamps blinking R-U-N in Morse while an enemy is awake
// where it looks, and on the screen a live view of where it leads (a 12 x
// 7 block window round the exit spot: the tiles, the other monitors, the
// items, the enemies, the runner and the shots); a panning one's view
// moves with its camera; dead ones show snow and a cracked screen, the
// NO SIGNAL one snow with a glimpse now and then, the VIRUS WARNING one
// flashes. In the Rewind bonus, the dark studio's tape counter.
//
// In front (drawGalleryFront): the crew (the Floor Manager pointing and
// counting 3-2-1 on his fingers, the Cable Wrangler and his cable whipping
// up or dropped, the Boom Op with his pole and the mic sweeping its arc,
// its shadow on the floor first), the runner's stars after a trip, the
// Rewind bonus's presses. The HUD (drawGalleryHud): the 6-frame static
// wipe of a monitor hop and the VHS rewind.

#include "game/world.hpp"

#include "assets/art.hpp"
#include "assets/enemy_art.hpp"
#include "base/math.hpp"
#include "data/enemies.hpp"
#include "data/weapons.hpp"
#include "game/gallery_draw.hpp"
#include "render/vector.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
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
constexpr Color kInk = rgb(10, 10, 16);
constexpr Color kHudInk = rgb(8, 8, 16);
constexpr Color kSteel = rgb(150, 156, 168);
constexpr Color kSteelHi = rgb(214, 220, 230);
constexpr Color kSteelLo = rgb(70, 74, 86);
constexpr Color kCasing = rgb(54, 56, 66);
constexpr Color kScreen = rgb(120, 190, 255);
constexpr Color kTally = rgb(240, 40, 40);
constexpr Color kTallyOff = rgb(70, 18, 18);
constexpr Color kUmd = rgb(110, 255, 150);
constexpr Color kWood = rgb(120, 82, 50);
constexpr Color kWoodHi = rgb(170, 124, 80);
constexpr Color kWoodLo = rgb(70, 46, 26);
constexpr Color kTape = rgb(236, 196, 40);
constexpr Color kMicGold = rgb(255, 230, 120);
constexpr Color kEchoBlue = rgb(150, 200, 255);

// Where a monitor's screen is in its 128 x 192 box.
constexpr float kScrX = 9.0f, kScrY = 9.0f, kScrW = 110.0f, kScrH = 64.0f;

float lerpCells(int prev, int cur, float alpha) { return (float(prev) + float(cur - prev) * alpha) * kCellPx; }

bool offScreen(float x, float y, float w, float h, float m = 96.0f)
{
  return x > float(kScreenW) + m || x + w < -m || y > float(kScreenH) + m || y + h < -m;
}

float unit(std::uint32_t h) { return float(h % 10000u) / 10000.0f; }

const Sprite& bakedSprite(const Art& art, const Renderer& r, const std::string& key, int w, int h, float ax, float ay,
  const std::function<void(cairo_t*)>& paint)
{
  const std::string id = "~gallery/" + key;
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

// Four 160 x 90 frames of grey TV snow, a brighter band rolling in each.
const Texture& noiseTex(const Art& art, const Renderer& r, int k)
{
  k &= 3;
  return baked(art, r, "noise" + std::to_string(k), 160, 90, 0.0f, 0.0f, [k](cairo_t* cr) {
    cairo_surface_t* s = cairo_get_target(cr);
    cairo_surface_flush(s);
    unsigned char* d = cairo_image_surface_get_data(s);
    const int stride = cairo_image_surface_get_stride(s);
    std::uint32_t h = 39101u + std::uint32_t(k) * 7919u;
    for (int y = 0; y < 90; ++y)
    {
      const int band = (y + k * 23) % 30 < 4 ? 45 : 0;
      for (int x = 0; x < 160; ++x)
      {
        h ^= h << 13;
        h ^= h >> 17;
        h ^= h << 5;
        const std::uint32_t v = std::uint32_t(std::min(255, int(18 + h % 200u) + band));
        const std::uint32_t px = 0xFF000000u | (v << 16) | (v << 8) | v;
        std::memcpy(d + y * stride + x * 4, &px, 4);
      }
    }
    cairo_surface_mark_dirty(s);
  });
}

// Snow stretched over a rectangle: a random part of a random frame.
void snow(Renderer& r, const Art& art, float x, float y, float w, float h, int seed, float alpha, Color tint = rgb(255, 255, 255))
{
  if (w <= 0.5f || h <= 0.5f || alpha <= 0.0f)
    return;
  const std::uint32_t hh = hash2(seed, 3907);
  const Texture& t = noiseTex(art, r, int(hh % 4u));
  const int sw = std::clamp(int(w / 3.0f), 24, 150), sh = std::clamp(int(h / 3.0f), 14, 84);
  const SDL_Rect src{int((hh >> 4) % unsigned(160 - sw + 1)), int((hh >> 12) % unsigned(90 - sh + 1)), sw, sh};
  const SDL_FRect dst{x, y, w, h};
  SDL_SetTextureBlendMode(t.get(), SDL_BLENDMODE_BLEND);
  SDL_SetTextureAlphaMod(t.get(), Uint8(std::lround(std::clamp(alpha, 0.0f, 1.0f) * 255.0f)));
  SDL_SetTextureColorMod(t.get(), Uint8(redOf(tint)), Uint8(greenOf(tint)), Uint8(blueOf(tint)));
  SDL_RenderCopyF(r.sdl(), r.use(t), &src, &dst);
  SDL_SetTextureColorMod(t.get(), 255, 255, 255);
  SDL_SetTextureAlphaMod(t.get(), 255);
}

// A rectangle filled with a texture tiled from (ox, oy), only the part on
// screen.
void tile(Renderer& r, const Texture& t, float x0, float y0, float x1, float y1, const DrawOpts& o = {})
{
  const float tw = float(t.w()), th = float(t.h());
  const float sx0 = std::max(x0, -tw), sx1 = std::min(x1, float(kScreenW) + tw);
  const float sy0 = std::max(y0, -th), sy1 = std::min(y1, float(kScreenH) + th);
  if (sx0 >= sx1 || sy0 >= sy1)
    return;
  Clip clip(r, x0, y0, x1 - x0, y1 - y0);
  for (float y = y0 + std::floor((sy0 - y0) / th) * th; y < sy1; y += th)
    for (float x = x0 + std::floor((sx0 - x0) / tw) * tw; x < sx1; x += tw)
      r.draw(t, x, y, o);
}

// R-U-N in Morse on a tally light: dot 4, dash 12, gap 4, letter gap 12, 28 dark.
bool tallyOn(int t)
{
  static const int kRun[] = {4, -4, 12, -4, 4, -12, 4, -4, 4, -4, 12, -12, 12, -4, 4, -28};
  int sum = 0;
  for (int v : kRun)
    sum += std::abs(v);
  int u = ((t % sum) + sum) % sum;
  for (int v : kRun)
  {
    if (u < std::abs(v))
      return v > 0;
    u -= std::abs(v);
  }
  return false;
}

// A camspot's spot at a fractional frame (World::camX at whole ones).
float camAt(const CamSpot& c, float t)
{
  const float len = float(std::abs(c.x1 - c.x0));
  if (len <= 0.0f)
    return float(c.x0);
  const float u = std::fmod(std::max(0.0f, t) * float(c.n) / float(c.d), 2.0f * len);
  return float(c.x0) + float((c.x1 > c.x0) - (c.x1 < c.x0)) * (u < len ? u : 2.0f * len - u);
}

const GalleryActor* actorOf(const World& w, const Enemy& e)
{
  const int i = int(&e - w.enemies().data());
  for (const auto& a : w.gallery().actors)
    if (a.enemy == i)
      return &a;
  return nullptr;
}

bool crew(const Enemy& e)
{
  return e.kind == EnemyKind::FloorManager || e.kind == EnemyKind::CableWrangler || e.kind == EnemyKind::BoomOp;
}

// Where a Floor Manager is in his count: 0 walking, 1 pointing, 2-4 the
// fingers (three, two, one), 5 the cue after "one".
int managerPose(const World& w, const Enemy& e, const GalleryActor* a)
{
  if (!a || a->count == 0)
    return e.cool > enemyDef(e.def).cooldown - 6 ? 5 : 0;
  const int total = w.player().turbo > 0 ? kGlCountTurbo : kGlCount;
  const int point = total * 7 / 22, per = std::max(1, (total - point) / 3);
  if (a->count <= point)
    return 1;
  return 2 + std::min(2, (a->count - point - 1) / per);
}

// The crew's sprites: key, variant and stride for this moment.
const Sprite& crewSprite(Renderer& r, const World& w, const Art& art, const Theme& th, const Enemy& e)
{
  const GalleryActor* a = actorOf(w, e);
  const int stride = int(e.drawX * 0.5f) % 2;
  if (e.kind == EnemyKind::FloorManager)
  {
    const int v = managerPose(w, e, a);
    return styledEnemySprite(art, r, th, "floormgr", v, v == 0 ? stride : 0, e.w, e.h);
  }
  if (e.kind == EnemyKind::CableWrangler)
  {
    const int v = a && a->whip > 0 && !a->dropped ? 1 : (a && a->dropped && e.flash > 0 ? 2 : 0);
    return styledEnemySprite(art, r, th, "wrangler", v, v == 0 ? stride : 0, e.w, e.h);
  }
  const int v = a && a->pass > 0 ? (a->pass <= kGlBoomPass ? 1 : 2) : 0;
  return styledEnemySprite(art, r, th, "boomop", v, 0, e.w, e.h);
}

// --- Painters: the monitors ------------------------------------------------------------------

// A studio monitor in its 128 x 192 box (the texture is 160 x 228, the box
// at (16, 24)): a deep grey casing, a black bezel round the screen hole, the
// UMD strip under it with its two tally lamps, the tally lamp's housing on
// top; on a cart (a VTR deck on the top shelf, a coil of cable on the
// bottom one, casters) or, up a wall, on a bracket with its lead hanging.
void paintMonitor(cairo_t* cr, bool cart)
{
  cairo_translate(cr, 16, 24);
  // The tally lamp's housing.
  box(cr, 52, -6, 24, 7, rgb(30, 30, 36));
  if (cart)
  {
    // The cart.
    box(cr, 54, 104, 20, 10, rgb(36, 36, 42));
    for (const double y : {114.0, 166.0})
    {
      cairo_rectangle(cr, 0, y, 128, 6);
      vgrad(cr, y, y + 6, {{0.0, kSteelHi}, {0.4, kSteel}, {1.0, kSteelLo}}, 1.0, true);
      stroke(cr, kInk, 1.0);
    }
    for (const double x : {6.0, 116.0})
    {
      cairo_rectangle(cr, x, 120, 6, 62);
      hgrad(cr, x, x + 6, {{0.0, kSteelHi}, {1.0, kSteelLo}}, 1.0, true);
      stroke(cr, kInk, 0.8);
    }
    // The VTR deck.
    roundRect(cr, 16, 122, 96, 22, 2);
    vgrad(cr, 122, 144, {{0.0, rgb(50, 52, 60)}, {1.0, rgb(22, 22, 28)}}, 1.0, true);
    stroke(cr, kInk, 1.2);
    box(cr, 26, 129, 46, 5, rgb(8, 8, 10));
    box(cr, 80, 128, 22, 7, rgb(20, 40, 30));
    text(cr, "00:42", 91, 134, 6, rgb(110, 255, 150));
    for (int k = 0; k < 4; ++k)
      box(cr, 28 + k * 9, 138, 6, 3, rgb(120, 124, 136));
    // A coil of cable on the bottom shelf.
    for (int k = 0; k < 3; ++k)
    {
      ellipse(cr, 46 + k * 2, 160, 22 - k * 2, 6 - k * 0.5);
      stroke(cr, rgb(20, 20, 24), 3.0);
    }
    // Casters.
    for (const double x : {9.0, 119.0})
    {
      box(cr, x - 2, 172, 4, 8, rgb(60, 62, 70));
      circle(cr, x, 185, 6.5);
      setRgba(cr, rgb(18, 18, 22), 1.0);
      cairo_fill_preserve(cr);
      stroke(cr, rgb(80, 82, 92), 1.2);
    }
  }
  else
  {
    // A wall bracket: an arm back to a plate, the lead hanging in a loop.
    box(cr, 56, 104, 16, 22, rgb(40, 40, 46));
    cairo_rectangle(cr, 40, 124, 48, 10);
    vgrad(cr, 124, 134, {{0.0, kSteelHi}, {1.0, kSteelLo}}, 1.0, true);
    stroke(cr, kInk, 1.0);
    for (const double x : {46.0, 82.0})
    {
      circle(cr, x, 129, 1.8);
      setRgba(cr, kInk, 1.0);
      cairo_fill(cr);
    }
    cairo_move_to(cr, 96, 104);
    cairo_curve_to(cr, 100, 150, 120, 160, 112, 186);
    stroke(cr, rgb(16, 16, 20), 3.4);
  }
  // The casing, deep, catching the light along its top.
  roundRect(cr, 0, 0, 128, 104, 7);
  vgrad(cr, 0, 104, {{0.0, rgb(96, 100, 114)}, {0.08, rgb(66, 68, 80)}, {0.6, kCasing}, {1.0, rgb(28, 28, 34)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  roundRect(cr, 4, 4, 120, 72, 4);
  setRgba(cr, rgb(18, 18, 22), 1.0);
  cairo_fill(cr);
  box(cr, kScrX, kScrY, kScrW, kScrH, rgb(6, 8, 12));
  // The UMD strip, its lamps' sockets, a brand and a power LED.
  roundRect(cr, 12, 79, 104, 15, 2);
  setRgba(cr, rgb(10, 10, 14), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, rgb(80, 84, 96), 0.8);
  for (const double x : {15.0, 105.0})
    box(cr, x, 82, 8, 9, kTallyOff);
  text(cr, "MAXVISION", 52, 101.5, 5.5, rgb(150, 156, 170), 0.8);
  circle(cr, 112, 99, 1.6);
  setRgba(cr, rgb(80, 255, 120), 1.0);
  cairo_fill(cr);
  for (const double x : {92.0, 100.0})
  {
    circle(cr, x, 99, 2.2);
    setRgba(cr, rgb(30, 30, 36), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, rgb(110, 114, 126), 0.6);
  }
}

// The screen's glass over whatever it shows (110 x 64): scanlines, a dark
// vignette into the corners, a sheen across it.
void paintGlass(cairo_t* cr)
{
  for (double y = 0; y < 64; y += 2)
    box(cr, 0, y, 110, 1, kInk, 0.22);
  cairo_pattern_t* g = cairo_pattern_create_radial(55, 32, 20, 55, 32, 70);
  stop(g, 0.0, kInk, 0.0);
  stop(g, 1.0, kInk, 0.65);
  cairo_rectangle(cr, 0, 0, 110, 64);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
  cairo_move_to(cr, 8, 0);
  cairo_line_to(cr, 50, 0);
  cairo_line_to(cr, 26, 64);
  cairo_line_to(cr, 0, 64);
  cairo_line_to(cr, 0, 18);
  cairo_close_path(cr);
  setRgba(cr, rgb(220, 235, 255), 0.07);
  cairo_fill(cr);
}

// A crack across a dead screen (110 x 64): a star of breaks from a hit.
void paintCrack(cairo_t* cr)
{
  const double cx = 70, cy = 24;
  static constexpr double kRays[][3] = {{-2.6, 60, 0.3}, {-1.6, 40, -0.2}, {-0.4, 50, 0.2}, {0.6, 34, -0.3}, {1.4, 46, 0.25},
    {2.4, 56, -0.2}, {3.1, 30, 0.1}};
  for (const auto& rr : kRays)
  {
    cairo_move_to(cr, cx, cy);
    const double mx = cx + std::cos(rr[0]) * rr[1] * 0.5, my = cy + std::sin(rr[0]) * rr[1] * 0.5;
    cairo_line_to(cr, mx + std::cos(rr[0] + rr[2]) * 4, my + std::sin(rr[0] + rr[2]) * 4);
    cairo_line_to(cr, cx + std::cos(rr[0]) * rr[1], cy + std::sin(rr[0]) * rr[1]);
  }
  stroke(cr, rgb(230, 240, 255), 1.0, 0.75);
  for (const double rr : {6.0, 13.0})
  {
    cairo_arc(cr, cx, cy, rr, 0.4, 2.6);
    stroke(cr, rgb(230, 240, 255), 0.7, 0.5);
  }
}

// --- The live view ---------------------------------------------------------------------------

// What a camera sees: the world rectangle (wx, wy, ww, wh) in px drawn
// small into the screen rectangle (sx, sy, sw, sh), clipped to it.
struct View
{
  float wx, wy, ww, wh;
  float sx, sy, sw, sh;
};

void drawLiveView(Renderer& r, const World& w, const Art& art, const Theme& th, const View& v, int frame, float alpha)
{
  Clip clip(r, v.sx, v.sy, v.sw, v.sh);
  const float k = v.sw / v.ww;
  auto X = [&](float px) { return v.sx + (px - v.wx) * k; };
  auto Y = [&](float py) { return v.sy + (py - v.wy) * k; };
  // The room's dark, lit from below by its own monitors.
  r.fillRect(v.sx, v.sy, v.sw, v.sh, rgb(26, 30, 42));
  r.fillRect(v.sx, v.sy + v.sh * 0.45f, v.sw, v.sh * 0.55f, rgba(44, 52, 72, 160));
  const CollisionMap& map = w.map();
  const int bx0 = std::max(0, int(std::floor(v.wx / kTilePx))), bx1 = std::min(map.widthBlocks() - 1, int((v.wx + v.ww) / kTilePx));
  const int by0 = std::max(0, int(std::floor(v.wy / kTilePx))), by1 = std::min(map.heightBlocks() - 1, int((v.wy + v.wh) / kTilePx));
  const float b = kTilePx * k;
  for (int by = by0; by <= by1; ++by)
    for (int bx = bx0; bx <= bx1; ++bx)
    {
      const Tile t = map.block(bx, by);
      const float x = X(float(bx) * kTilePx), y = Y(float(by) * kTilePx);
      if (t == Tile::Solid)
      {
        r.fillRect(x, y, b + 0.5f, b + 0.5f, by > 0 && map.block(bx, by - 1) == Tile::Solid ? rgb(44, 48, 60) : rgb(58, 64, 80));
        if (by > 0 && map.block(bx, by - 1) != Tile::Solid)
          r.fillRect(x, y, b + 0.5f, 2.0f, rgb(150, 160, 186));
      }
      else if (t == Tile::Platform)
        r.fillRect(x, y, b + 0.5f, std::max(2.0f, b * 0.2f), kSteel);
      else if (t == Tile::Spikes)
        r.fillRect(x, y + b * 0.5f, b, b * 0.5f, rgb(160, 200, 240));
    }
  // The exit, glowing.
  {
    const Level& lv = w.level();
    const float ex = X(float(lv.exitTx) * kTilePx - 32.0f), ey = Y(float(lv.exitTy + 1) * kTilePx);
    r.fillRect(ex + 4.0f * k * 4.0f, ey - 160.0f * k, 96.0f * k, 120.0f * k, rgb(230, 245, 255));
  }
  // The other monitors: little sets with blue screens.
  for (const auto& m : w.gallery().monitors)
  {
    const float x = X(float(m.x) * kTilePx), y = Y(float(m.y) * kTilePx);
    if (x > v.sx + v.sw || x + 128.0f * k < v.sx || y > v.sy + v.sh || y + 192.0f * k < v.sy)
      continue;
    r.fillRect(x, y, 128.0f * k, 104.0f * k, kCasing);
    r.fillRect(x + kScrX * k, y + kScrY * k, kScrW * k, kScrH * k, m.dead ? rgb(120, 120, 130) : m.virus ? rgb(200, 40, 40) : kScreen);
    r.fillRect(x + 50.0f * k, y + 104.0f * k, 28.0f * k, 88.0f * k, rgb(40, 42, 50));
  }
  // Items: gems glinting, the rest as little badges.
  for (const auto& it : w.items())
  {
    if (it.taken || it.heldBy >= 0)
      continue;
    const float x = X((float(it.x) + 1.0f) * kCellPx), y = Y(float(it.y) * kCellPx);
    if (x < v.sx - 8.0f || x > v.sx + v.sw + 8.0f || y < v.sy - 8.0f || y > v.sy + v.sh + 8.0f)
      continue;
    const float s = std::max(3.0f, 36.0f * k);
    Color c = rgb(255, 230, 120);
    if (it.kind == ItemKind::Gem)
      c = art.gemColor[std::size_t(std::clamp(it.variant, 0, 3))];
    else if (it.kind == ItemKind::Health)
      c = rgb(255, 90, 110);
    r.fillRect(x - s * 0.5f, y - s, s, s, c);
    if ((frame / 6 + it.x) % 5 == 0)
      r.fillRect(x - s * 0.5f, y - s, s * 0.5f, s * 0.5f, rgb(255, 255, 255), Blend::Add);
  }
  for (const auto& bx : w.boxes())
    if (bx.alive)
    {
      const float x = X(float(bx.x) * kCellPx), y = Y(float(bx.y - 1) * kCellPx);
      r.fillRect(x, y, 64.0f * k, 64.0f * k, art.boxColor[std::size_t(std::clamp(bx.variant, 0, 2))]);
    }
  // The enemies, small.
  for (const auto& e : w.enemies())
  {
    if (!e.alive || e.hidden)
      continue;
    const float cx = X((e.drawX + float(e.w) * 0.5f) * kCellPx), foot = Y((e.drawY + 1.0f) * kCellPx);
    if (cx < v.sx - 40.0f || cx > v.sx + v.sw + 40.0f || foot < v.sy - 10.0f || foot > v.sy + v.sh + 60.0f)
      continue;
    if (crew(e))
    {
      if (e.kind == EnemyKind::CableWrangler)
        if (const GalleryActor* a = actorOf(w, e))
          r.fillRect(X(float(a->cableX0) * kCellPx), foot - 2.0f, float(a->cableX1 - a->cableX0 + 1) * kCellPx * k, 2.0f, kInk);
      DrawOpts o;
      o.scale = k;
      r.draw(crewSprite(r, w, art, th, e).get(e.dir), cx, foot, o);
    }
    else
      r.fillRect(cx - 6.0f, foot - 14.0f, 12.0f, 14.0f, rgb(230, 70, 70));
  }
  // The runner, if they're where it looks.
  {
    const Player& p = w.player();
    if (p.state != PlayerState::Teleporting && !p.hidden)
    {
      const float x = X(lerpCells(p.prevX, p.x, alpha) + 1.5f * kCellPx), y = Y(lerpCells(p.prevY + 1, p.y + 1, alpha));
      if (x > v.sx - 40.0f && x < v.sx + v.sw + 40.0f && y > v.sy - 10.0f && y < v.sy + v.sh + 80.0f)
      {
        DrawOpts o;
        o.scale = k;
        r.draw(art.runner(w.character()).idle[std::size_t((frame / 30) % 2)].get(p.facing), x, y, o);
      }
    }
  }
  // Shots.
  for (const auto& pr : w.projectiles())
  {
    if (!pr.alive)
      continue;
    const float x = X((float(pr.x) + float(pr.w) * 0.5f) * kCellPx), y = Y((float(pr.y) + float(pr.h) * 0.5f) * kCellPx);
    const Color c = pr.kind == ShotKind::Enemy ? rgb(255, 120, 100) : pr.echo ? kEchoBlue : kMicGold;
    r.fillRect(x - 2.0f, y - 2.0f, 4.0f, 4.0f, c, Blend::Add);
  }
}

// --- Painters: props over the tiles ----------------------------------------------------------

// The mixing desk (5 blocks of desk top, 344 x 210; its top-left block's
// corner at (12, 8)): the desk top a sloped panel of faders, knobs and
// meters over the one-way top; the body under it, a vented front, a
// keyboard drawer and a footrest.
void paintMixDesk(cairo_t* cr, int blocks)
{
  const double W = blocks * 64.0;
  cairo_translate(cr, 12, 8);
  // The body.
  cairo_rectangle(cr, 6, 30, W - 12, 162);
  vgrad(cr, 30, 192, {{0.0, rgb(58, 60, 70)}, {0.3, rgb(42, 44, 52)}, {1.0, rgb(20, 20, 26)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  for (double y = 70; y < 170; y += 8)
  {
    cairo_move_to(cr, 30, y);
    cairo_line_to(cr, W - 30, y);
  }
  stroke(cr, rgb(16, 16, 20), 2.0, 0.8);
  // The keyboard drawer, pulled out a little.
  cairo_rectangle(cr, W * 0.25, 34, W * 0.5, 22);
  vgrad(cr, 34, 56, {{0.0, rgb(70, 72, 84)}, {1.0, rgb(34, 34, 40)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  for (int k = 0; k < 16; ++k)
    box(cr, W * 0.25 + 10 + k * (W * 0.5 - 20) / 16.0, 40, (W * 0.5 - 20) / 16.0 - 2, 6, rgb(200, 200, 206), 0.85);
  // The footrest bar and the legs.
  box(cr, 20, 176, W - 40, 6, kSteelLo);
  // The desk top: a deep sloped panel.
  cairo_move_to(cr, -8, 0);
  cairo_line_to(cr, W + 8, 0);
  cairo_line_to(cr, W + 4, 30);
  cairo_line_to(cr, -4, 30);
  cairo_close_path(cr);
  vgrad(cr, 0, 30, {{0.0, rgb(90, 94, 108)}, {0.15, rgb(56, 58, 68)}, {1.0, rgb(30, 30, 36)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  // Channel strips: faders, knobs, mute buttons.
  const int strips = int(W / 14.0);
  for (int i = 0; i < strips; ++i)
  {
    const double x = 8 + i * (W - 16) / strips;
    for (int k = 0; k < 2; ++k)
    {
      circle(cr, x + 4, 5 + k * 6, 2.0);
      setRgba(cr, k ? rgb(200, 200, 210) : rgb(220, 60, 60), 1.0);
      cairo_fill(cr);
    }
    box(cr, x + 3.3, 16, 1.4, 11, rgb(10, 10, 12));
    box(cr, x + 1, 17 + double((i * 37) % 8), 6, 3.4, rgb(230, 230, 236));
  }
  // The meter bridge at the back of the desk, its needles lit amber.
  for (int k = 0; k < 4; ++k)
  {
    const double x = 30 + k * (W - 60) / 3.0 - 20;
    roundRect(cr, x, -16, 40, 15, 2);
    setRgba(cr, rgb(250, 200, 110), 0.9);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.2);
    cairo_move_to(cr, x + 20, -2);
    cairo_line_to(cr, x + 12 + k * 4, -13);
    stroke(cr, kInk, 1.0);
  }
}

// One block of library shelf (64 x 30): an oak plank, its front lip
// labelled with the year.
void paintPlank(cairo_t* cr, int year)
{
  cairo_rectangle(cr, 0, 0, 64, 12);
  vgrad(cr, 0, 12, {{0.0, kWoodHi}, {0.3, kWood}, {1.0, kWoodLo}}, 1.0, true);
  stroke(cr, kInk, 1.0);
  box(cr, 0, 12, 64, 6, rgb(30, 20, 12), 0.6);
  box(cr, 20, 3, 24, 7, rgb(240, 236, 220));
  char buf[8];
  std::snprintf(buf, sizeof(buf), "%d", year);
  text(cr, buf, 32, 9, 6.5, rgb(40, 30, 20));
}

// A block of steel roller shutter (64 x 64): horizontal ribs.
void paintShutter(cairo_t* cr)
{
  box(cr, 0, 0, 64, 64, rgb(96, 100, 110));
  for (double y = 0; y < 64; y += 8)
  {
    cairo_rectangle(cr, 0, y, 64, 8);
    vgrad(cr, y, y + 8, {{0.0, rgb(170, 176, 188)}, {0.5, rgb(110, 114, 126)}, {1.0, rgb(60, 62, 72)}});
  }
  box(cr, 0, 0, 3, 64, rgb(40, 42, 50));
  box(cr, 61, 0, 3, 64, rgb(40, 42, 50));
}

// The shutter's bottom bar with its padlock and STAFF ONLY (64 x 48).
void paintShutterFoot(cairo_t* cr)
{
  box(cr, 0, 0, 64, 10, rgb(60, 62, 72));
  cairo_arc(cr, 32, 18, 5, kPi, 2 * kPi);
  stroke(cr, kSteelHi, 2.0);
  roundRect(cr, 25, 18, 14, 13, 2);
  vgrad(cr, 18, 31, {{0.0, rgb(240, 200, 80)}, {1.0, rgb(150, 110, 30)}}, 1.0, true);
  stroke(cr, kInk, 1.0);
  box(cr, 6, 36, 52, 10, rgb(200, 30, 36));
  text(cr, "STAFF ONLY", 32, 44, 6.4, rgb(255, 255, 255));
}

// The edit suite's locked door (64 x 256), set in the wall: steel, a
// keypad with its red LED, the sign.
void paintLockedDoor(cairo_t* cr)
{
  box(cr, 0, 0, 64, 256, rgb(20, 20, 26));
  cairo_rectangle(cr, 6, 8, 52, 248);
  hgrad(cr, 6, 58, {{0.0, rgb(110, 114, 128)}, {0.5, rgb(80, 84, 96)}, {1.0, rgb(46, 48, 58)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  cairo_rectangle(cr, 14, 30, 36, 70);
  stroke(cr, rgb(50, 52, 62), 1.4);
  cairo_rectangle(cr, 14, 120, 36, 110);
  stroke(cr, rgb(50, 52, 62), 1.4);
  box(cr, 10, 108, 44, 18, rgb(230, 230, 220));
  text(cr, "EDIT 1", 32, 116, 6.5, rgb(30, 30, 40));
  text(cr, "KEEP LOCKED", 32, 123, 5.0, rgb(200, 30, 40));
  roundRect(cr, 44, 136, 10, 18, 2);
  setRgba(cr, rgb(30, 30, 36), 1.0);
  cairo_fill(cr);
  for (int k = 0; k < 6; ++k)
    box(cr, 45.5 + (k % 2) * 4, 140 + (k / 2) * 4, 3, 3, rgb(160, 164, 176));
  box(cr, 4, 152, 6, 22, rgb(170, 174, 186));
}

// A strip of dolly track (64 x 18): two steel rails on dark sleepers.
void paintTrack(cairo_t* cr)
{
  for (double x = 6; x < 64; x += 16)
    box(cr, x, 9, 9, 7, rgb(30, 28, 26));
  for (const double y : {6.0, 12.0})
  {
    cairo_rectangle(cr, 0, y - 2, 64, 4);
    vgrad(cr, y - 2, y + 2, {{0.0, kSteelHi}, {1.0, kSteelLo}});
  }
}

// A block of lighting truss (64 x 44): a silver box truss over the one-way
// top, its zigzag lacing.
void paintTruss(cairo_t* cr)
{
  for (const double y : {2.0, 36.0})
  {
    cairo_rectangle(cr, 0, y - 3, 64, 6);
    vgrad(cr, y - 3, y + 3, {{0.0, rgb(250, 250, 255)}, {0.5, rgb(170, 176, 188)}, {1.0, rgb(90, 94, 106)}});
  }
  cairo_move_to(cr, 0, 36);
  for (int k = 0; k < 4; ++k)
  {
    cairo_line_to(cr, k * 16 + 8, 2);
    cairo_line_to(cr, k * 16 + 16, 36);
  }
  stroke(cr, rgb(140, 146, 158), 2.4);
}

// The CAM 0 display's frame (264 x 184; its screen at (14, 14), 236 x 144):
// a wall-mounted monitor with its UMD reading CAM 0.
void paintCam0Frame(cairo_t* cr)
{
  box(cr, 120, 0, 24, 6, rgb(40, 40, 46));
  roundRect(cr, 0, 4, 264, 180, 8);
  vgrad(cr, 4, 184, {{0.0, rgb(90, 94, 108)}, {0.1, rgb(60, 62, 74)}, {1.0, rgb(26, 26, 32)}}, 1.0, true);
  stroke(cr, kInk, 2.4);
  box(cr, 10, 10, 244, 152, rgb(14, 14, 18));
  roundRect(cr, 92, 165, 80, 14, 2);
  setRgba(cr, rgb(8, 8, 12), 1.0);
  cairo_fill(cr);
}

// The living room on CAM 0 (236 x 144): wallpaper, a lamp, a window on the
// night, the TV on its stand (its screen at (38, 14), 160 x 90, left for the
// live game).
void paintLivingRoom(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 0, 236, 144);
  vgrad(cr, 0, 144, {{0.0, rgb(70, 54, 60)}, {1.0, rgb(40, 30, 38)}});
  for (double x = 6; x < 236; x += 14)
  {
    cairo_move_to(cr, x, 0);
    cairo_line_to(cr, x, 120);
  }
  stroke(cr, rgb(110, 84, 80), 2.0, 0.35);
  // The window and the lamp.
  box(cr, 206, 12, 26, 44, rgb(20, 26, 60));
  for (int k = 0; k < 5; ++k)
    box(cr, 210 + (k * 7) % 18, 18 + (k * 11) % 30, 2, 2, rgb(255, 240, 200), 0.8);
  cairo_rectangle(cr, 206, 12, 26, 44);
  stroke(cr, rgb(200, 180, 150), 2.0);
  softDot(cr, 16, 40, 34, 40, rgb(255, 210, 140), 0.45);
  cairo_move_to(cr, 8, 34);
  cairo_line_to(cr, 24, 34);
  cairo_line_to(cr, 20, 20);
  cairo_line_to(cr, 12, 20);
  cairo_close_path(cr);
  setRgba(cr, rgb(255, 220, 160), 1.0);
  cairo_fill(cr);
  box(cr, 15, 34, 2, 60, rgb(60, 50, 40));
  // The floor.
  box(cr, 0, 118, 236, 26, rgb(60, 40, 30));
  // The TV, its stand and the glow it throws.
  softDot(cr, 118, 70, 120, 70, rgb(140, 190, 255), 0.25);
  roundRect(cr, 32, 8, 172, 102, 4);
  setRgba(cr, rgb(16, 16, 20), 1.0);
  cairo_fill(cr);
  box(cr, 100, 110, 36, 6, rgb(30, 30, 36));
  box(cr, 60, 116, 116, 6, rgb(80, 56, 40));
}

// The sofa's back and the heads of two people on it watching the TV, one
// with a gamepad held up, the cable to the console (236 x 144).
void paintSofa(cairo_t* cr)
{
  roundRect(cr, 40, 112, 160, 40, 10);
  vgrad(cr, 112, 152, {{0.0, rgb(120, 70, 50)}, {1.0, rgb(60, 32, 24)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  // Heads, from behind: hair and ears lit by the TV.
  for (int k = 0; k < 2; ++k)
  {
    const double x = k ? 150 : 88, y = k ? 104 : 100;
    ellipse(cr, x, y, 14, 16);
    setRgba(cr, k ? rgb(40, 26, 20) : rgb(200, 140, 70), 1.0);
    cairo_fill(cr);
    softDot(cr, x, y - 10, 12, 6, rgb(170, 210, 255), 0.4);
  }
  // The gamepad held up.
  roundRect(cr, 104, 92, 22, 10, 4);
  setRgba(cr, rgb(30, 30, 36), 1.0);
  cairo_fill(cr);
  box(cr, 120, 95, 2, 2, kTally);
}

// A wall camera on a bracket (100 x 80), aimed down into the room, its red
// tally lamp left dark (lit by the level).
void paintWallCam(cairo_t* cr)
{
  box(cr, 74, 10, 20, 34, rgb(50, 52, 60));
  cairo_move_to(cr, 80, 30);
  cairo_line_to(cr, 60, 44);
  stroke(cr, rgb(70, 72, 84), 6.0);
  cairo_save(cr);
  cairo_translate(cr, 46, 48);
  cairo_rotate(cr, 0.4);
  roundRect(cr, -36, -14, 64, 28, 4);
  vgrad(cr, -14, 14, {{0.0, rgb(220, 222, 228)}, {1.0, rgb(130, 134, 146)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  cairo_rectangle(cr, -44, -12, 10, 24);
  setRgba(cr, rgb(30, 30, 36), 1.0);
  cairo_fill(cr);
  circle(cr, -44, 0, 8);
  setRgba(cr, rgb(20, 40, 70), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.2);
  text(cr, "CAM 0", -4, 4, 9, rgb(40, 40, 50));
  cairo_restore(cr);
}

// The ON AIR sign (144 x 80): a red light box lettered white, on a bracket.
void paintOnAir(cairo_t* cr, bool lit)
{
  box(cr, 66, 0, 12, 10, rgb(40, 40, 46));
  roundRect(cr, 4, 10, 136, 62, 6);
  vgrad(cr, 10, 72, {{0.0, rgb(70, 70, 80)}, {1.0, rgb(24, 24, 30)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  roundRect(cr, 12, 18, 120, 46, 3);
  if (lit)
    vgrad(cr, 18, 64, {{0.0, rgb(255, 90, 80)}, {1.0, rgb(200, 20, 30)}});
  else
    vgrad(cr, 18, 64, {{0.0, rgb(90, 30, 30)}, {1.0, rgb(50, 14, 16)}});
  text(cr, "ON AIR", 72, 52, 26, lit ? rgb(255, 250, 240) : rgb(140, 70, 70));
}

// A picture on one of the walls of screens (200 x 120): 0 colour bars,
// 1 the news (an anchor at the desk, the red strap), 2 the weather map,
// 3 a game show's stage (spotlights, a big question mark), 4 the test card,
// 5 the football, 6 the channel ident (MAXTV on its swirl).
void paintWallPic(cairo_t* cr, int k)
{
  const double W = 200, H = 120;
  switch (k)
  {
    case 0:
    {
      static constexpr Color bars[7] = {rgb(192, 192, 192), rgb(192, 192, 0), rgb(0, 192, 192), rgb(0, 192, 0), rgb(192, 0, 192),
        rgb(192, 0, 0), rgb(0, 0, 192)};
      for (int i = 0; i < 7; ++i)
        box(cr, double(i) * W / 7.0, 0, W / 7.0 + 0.5, H * 0.7, bars[i]);
      box(cr, 0, H * 0.7, W, H * 0.3, rgb(16, 16, 40));
      box(cr, W * 0.1, H * 0.76, W * 0.2, H * 0.2, rgb(240, 240, 240));
      break;
    }
    case 1:
      cairo_rectangle(cr, 0, 0, W, H);
      vgrad(cr, 0, H, {{0.0, rgb(20, 50, 120)}, {1.0, rgb(10, 20, 60)}});
      for (int i = 0; i < 5; ++i)
        box(cr, 120 + i * 14, 14 + (i % 2) * 8, 8, 40, rgb(60, 110, 200), 0.6);
      ellipse(cr, 70, 92, 34, 26);
      setRgba(cr, rgb(40, 40, 60), 1.0);
      cairo_fill(cr);
      box(cr, 66, 66, 8, 12, rgb(230, 230, 236));
      circle(cr, 70, 48, 16);
      setRgba(cr, rgb(214, 170, 130), 1.0);
      cairo_fill(cr);
      ellipse(cr, 70, 36, 17, 9);
      setRgba(cr, rgb(70, 46, 30), 1.0);
      cairo_fill(cr);
      box(cr, 0, 94, W, 16, rgb(200, 20, 30));
      box(cr, 0, 110, W, 10, rgb(250, 250, 250));
      text(cr, "NEWS 9", 30, 106, 10, rgb(255, 255, 255));
      break;
    case 2:
      cairo_rectangle(cr, 0, 0, W, H);
      vgrad(cr, 0, H, {{0.0, rgb(30, 90, 160)}, {1.0, rgb(20, 60, 120)}});
      cairo_move_to(cr, 30, 120);
      cairo_curve_to(cr, 20, 70, 60, 50, 80, 20);
      cairo_curve_to(cr, 110, 10, 150, 30, 140, 60);
      cairo_curve_to(cr, 130, 90, 170, 100, 160, 120);
      cairo_close_path(cr);
      setRgba(cr, rgb(70, 160, 80), 1.0);
      cairo_fill(cr);
      for (int i = 0; i < 3; ++i)
      {
        softDot(cr, 60 + i * 40, 40 + (i % 2) * 30, 22, 10, rgb(255, 255, 255), 0.8);
      }
      circle(cr, 160, 30, 12);
      setRgba(cr, rgb(255, 210, 60), 1.0);
      cairo_fill(cr);
      text(cr, "21", 100, 70, 16, rgb(255, 255, 255));
      break;
    case 3:
      cairo_rectangle(cr, 0, 0, W, H);
      vgrad(cr, 0, H, {{0.0, rgb(60, 10, 90)}, {1.0, rgb(140, 30, 120)}});
      for (int i = 0; i < 4; ++i)
      {
        cairo_move_to(cr, 20 + i * 54, 0);
        cairo_line_to(cr, 5 + i * 54, H);
        cairo_line_to(cr, 45 + i * 54, H);
        cairo_close_path(cr);
        setRgba(cr, rgb(255, 230, 255), 0.18);
        cairo_fill(cr);
      }
      for (int i = 0; i < 12; ++i)
      {
        circle(cr, 20 + i * 15, 104, 3);
        setRgba(cr, rgb(255, 220, 80), 1.0);
        cairo_fill(cr);
      }
      text(cr, "?", 100, 84, 64, rgb(255, 220, 80));
      break;
    case 4:
      box(cr, 0, 0, W, H, rgb(90, 90, 96));
      for (double x = 0; x < W; x += 20)
        box(cr, x, 0, 1.5, H, rgb(220, 220, 220));
      for (double y = 0; y < H; y += 20)
        box(cr, 0, y, W, 1.5, rgb(220, 220, 220));
      circle(cr, 100, 60, 46);
      setRgba(cr, rgb(30, 30, 34), 1.0);
      cairo_fill(cr);
      for (int i = 0; i < 6; ++i)
        box(cr, 62 + i * 13, 40, 13, 22, i % 2 ? rgb(250, 250, 250) : rgb(0, 0, 0));
      box(cr, 62, 62, 76, 18, rgb(220, 60, 50));
      text(cr, "MAXTV", 100, 76, 13, rgb(255, 255, 255));
      break;
    case 5:
      cairo_rectangle(cr, 0, 0, W, H);
      vgrad(cr, 0, H, {{0.0, rgb(40, 130, 50)}, {1.0, rgb(60, 170, 70)}});
      for (int i = 0; i < 5; ++i)
        box(cr, double(i) * 44, 0, 22, H, rgb(30, 110, 40), 0.35);
      cairo_set_line_width(cr, 2.0);
      setRgba(cr, rgb(240, 240, 240), 0.9);
      cairo_move_to(cr, 100, 0);
      cairo_line_to(cr, 100, H);
      cairo_stroke(cr);
      circle(cr, 100, 60, 22);
      cairo_stroke(cr);
      for (int i = 0; i < 4; ++i)
      {
        circle(cr, 40 + i * 40, 30 + (i * 37) % 60, 4);
        setRgba(cr, i % 2 ? rgb(240, 40, 40) : rgb(40, 80, 240), 1.0);
        cairo_fill(cr);
      }
      box(cr, 6, 6, 70, 14, rgb(10, 10, 20), 0.8);
      text(cr, "2 - 1", 41, 17, 10, rgb(255, 255, 255));
      break;
    default:
      cairo_rectangle(cr, 0, 0, W, H);
      vgrad(cr, 0, H, {{0.0, rgb(10, 10, 30)}, {1.0, rgb(30, 10, 50)}});
      for (int i = 0; i < 6; ++i)
      {
        cairo_arc(cr, 100, 60, 14 + i * 12, 0.3 * i, 0.3 * i + 3.6);
        cairo_set_line_width(cr, 5.0);
        setRgba(cr, i % 2 ? rgb(255, 60, 120) : rgb(60, 200, 255), 0.6);
        cairo_stroke(cr);
      }
      text(cr, "MAXTV", 100, 68, 24, rgb(255, 255, 255));
      break;
  }
}

// The note taped up over CAM 42's dead monitor (132 x 72): a sheet of
// printer paper, a little crooked, OUT OF ORDER in marker, two strips of
// gaffer tape across its corners.
void paintOutOfOrder(cairo_t* cr)
{
  cairo_save(cr);
  cairo_translate(cr, 66, 36);
  cairo_rotate(cr, -0.05);
  cairo_rectangle(cr, -56, -28, 112, 56);
  vgrad(cr, -28, 28, {{0.0, rgb(250, 248, 240)}, {1.0, rgb(214, 210, 198)}}, 1.0, true);
  stroke(cr, rgb(120, 116, 106), 1.0);
  text(cr, "OUT OF", 0, -4, 15, rgb(30, 30, 40));
  text(cr, "ORDER", 0, 16, 15, rgb(200, 30, 40));
  for (const double sx : {-1.0, 1.0})
  {
    cairo_save(cr);
    cairo_translate(cr, sx * 52, -26);
    cairo_rotate(cr, sx * 0.7);
    cairo_rectangle(cr, -14, -5, 28, 10);
    setRgba(cr, rgb(70, 70, 76), 0.92);
    cairo_fill(cr);
    cairo_restore(cr);
  }
  cairo_restore(cr);
}

// A candid camera tape (64 x 64): a VHS cassette in a white sleeve, its
// spine hand-lettered CANDID and a red dot sticker.
void paintCandidTape(cairo_t* cr, int n)
{
  cairo_save(cr);
  cairo_translate(cr, 32, 40);
  cairo_rotate(cr, n % 2 ? -0.06 : 0.05);
  roundRect(cr, -24, -16, 48, 30, 3);
  vgrad(cr, -16, 14, {{0.0, rgb(252, 250, 242)}, {1.0, rgb(200, 196, 184)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  roundRect(cr, -20, -12, 40, 11, 1.5);
  setRgba(cr, rgb(40, 40, 46), 1.0);
  cairo_fill(cr);
  for (const double x : {-10.0, 10.0})
  {
    circle(cr, x, -6.5, 3.4);
    setRgba(cr, rgb(220, 220, 228), 1.0);
    cairo_fill(cr);
    circle(cr, x, -6.5, 1.2);
    setRgba(cr, kInk, 1.0);
    cairo_fill(cr);
  }
  text(cr, "CANDID", 0, 7, 7.5, rgb(30, 40, 120));
  char buf[8];
  std::snprintf(buf, sizeof(buf), "#%d", 1 + n % 20);
  text(cr, buf, 14, 12.5, 5.5, rgb(90, 90, 100));
  circle(cr, -17, 8, 3.2);
  setRgba(cr, kTally, 1.0);
  cairo_fill(cr);
  cairo_restore(cr);
}

// The press head (w x 60): a steel platen with bolts, hazard stripes along
// its bottom edge.
void paintPressHead(cairo_t* cr, double w)
{
  roundRect(cr, 2, 2, w - 4, 44, 4);
  vgrad(cr, 2, 46, {{0.0, rgb(170, 176, 190)}, {0.4, rgb(110, 114, 128)}, {1.0, rgb(56, 58, 68)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  for (double x = 12; x < w - 6; x += 22)
  {
    circle(cr, x, 12, 2.4);
    setRgba(cr, rgb(60, 62, 72), 1.0);
    cairo_fill(cr);
  }
  cairo_save(cr);
  cairo_rectangle(cr, 2, 44, w - 4, 14);
  cairo_clip(cr);
  box(cr, 2, 44, w - 4, 14, kTape);
  for (double x = -14; x < w + 14; x += 16)
  {
    cairo_move_to(cr, x, 58);
    cairo_line_to(cr, x + 8, 58);
    cairo_line_to(cr, x + 22, 44);
    cairo_line_to(cr, x + 14, 44);
    cairo_close_path(cr);
    setRgba(cr, rgb(20, 20, 24), 1.0);
    cairo_fill(cr);
  }
  cairo_restore(cr);
  cairo_rectangle(cr, 2, 44, w - 4, 14);
  stroke(cr, kInk, 1.6);
}

// The boom's mic in its fluffy windshield (64 x 28).
void paintDeadCat(cairo_t* cr)
{
  ellipse(cr, 32, 14, 28, 10);
  setRgba(cr, rgb(150, 146, 140), 1.0);
  cairo_fill(cr);
  for (int k = 0; k < 70; ++k)
  {
    const double a = k * 0.37, rr = 0.75 + 0.25 * std::sin(k * 1.7);
    const double x = 32 + std::cos(a) * 27 * rr, y = 14 + std::sin(a) * 9.5 * rr;
    cairo_move_to(cr, x, y);
    cairo_line_to(cr, x + std::cos(a) * 3.5, y + std::sin(a) * 3.0);
  }
  stroke(cr, rgb(210, 206, 200), 1.0, 0.8);
  ellipse(cr, 26, 10, 14, 4);
  setRgba(cr, rgb(230, 228, 222), 0.5);
  cairo_fill(cr);
}

// A cue card (44 x 30): white, scrawled lines in marker.
void paintCueCard(cairo_t* cr)
{
  roundRect(cr, 2, 2, 40, 26, 2);
  setRgba(cr, rgb(252, 250, 240), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.6);
  for (int k = 0; k < 3; ++k)
  {
    cairo_move_to(cr, 8, 9 + k * 6);
    cairo_curve_to(cr, 16, 7 + k * 6, 24, 11 + k * 6, 34 - k * 5, 9 + k * 6);
  }
  stroke(cr, rgb(30, 30, 160), 2.0);
}

// One ring of sound (24 x 56): an arc bulging forward (to the right).
void paintEchoArc(cairo_t* cr, bool dotted)
{
  cairo_arc(cr, -6, 28, 26, -0.95, 0.95);
  if (dotted)
  {
    const double dash[] = {4.0, 4.0};
    cairo_set_dash(cr, dash, 2, 0);
  }
  stroke(cr, rgb(255, 255, 255), 5.0);
}

// The Echo Mic's icon (64 x 64): a silver handheld mic at a slant, its
// mesh head lit, a red ON switch, a lanyard loop at its end.
void paintEchoMic(cairo_t* cr)
{
  cairo_save(cr);
  cairo_translate(cr, 26, 36);
  cairo_rotate(cr, -0.75);
  // The handle.
  cairo_move_to(cr, -5, -6);
  cairo_line_to(cr, 5, -6);
  cairo_line_to(cr, 3.5, 26);
  cairo_line_to(cr, -3.5, 26);
  cairo_close_path(cr);
  hgrad(cr, -5, 5, {{0.0, rgb(90, 90, 100)}, {0.4, rgb(40, 40, 46)}, {1.0, rgb(16, 16, 20)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  box(cr, -2.2, 6, 4.4, 7, rgb(220, 40, 40));
  box(cr, -1.2, 7, 2.4, 2.4, rgb(255, 200, 200));
  cairo_arc(cr, 0, 29, 3, 0, 2 * kPi);
  stroke(cr, rgb(150, 150, 160), 1.2);
  // The collar and the mesh head.
  cairo_rectangle(cr, -6.5, -10, 13, 5);
  hgrad(cr, -6.5, 6.5, {{0.0, kSteelHi}, {1.0, kSteelLo}}, 1.0, true);
  stroke(cr, kInk, 1.2);
  circle(cr, 0, -19, 10.5);
  cairo_pattern_t* g = cairo_pattern_create_radial(-4, -23, 1, 0, -19, 11);
  stop(g, 0.0, rgb(255, 255, 255));
  stop(g, 0.5, rgb(190, 196, 208));
  stop(g, 1.0, rgb(90, 94, 106));
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  stroke(cr, kInk, 1.6);
  cairo_save(cr);
  circle(cr, 0, -19, 10);
  cairo_clip(cr);
  for (double k = -12; k <= 12; k += 3)
  {
    cairo_move_to(cr, k, -31);
    cairo_line_to(cr, k + 12, -7);
    cairo_move_to(cr, k, -7);
    cairo_line_to(cr, k + 12, -31);
  }
  stroke(cr, rgb(70, 74, 86), 0.6, 0.6);
  cairo_restore(cr);
  cairo_restore(cr);
}

} // namespace

// --- Behind the tiles: the rooms -------------------------------------------------------------

namespace
{

// A pedestal camera seen side on (220 x 320; anchor the base's middle on
// the floor at (110, 316)), aimed right, its tally lamp left dark.
void paintPedCam(cairo_t* cr)
{
  cairo_translate(cr, 110, 316);
  cairo_move_to(cr, -70, -14);
  cairo_line_to(cr, 70, -14);
  cairo_line_to(cr, 52, -40);
  cairo_line_to(cr, -52, -40);
  cairo_close_path(cr);
  vgrad(cr, -40, -14, {{0.0, rgb(80, 84, 96)}, {1.0, rgb(36, 38, 46)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  for (const double x : {-56.0, 0.0, 56.0})
  {
    circle(cr, x, -8, 8);
    setRgba(cr, rgb(16, 16, 20), 1.0);
    cairo_fill(cr);
  }
  cairo_rectangle(cr, -11, -200, 22, 162);
  hgrad(cr, -11, 11, {{0.0, rgb(120, 124, 136)}, {0.4, rgb(80, 84, 96)}, {1.0, rgb(40, 42, 50)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  cairo_save(cr);
  cairo_translate(cr, 0, -120);
  cairo_scale(cr, 1.0, 0.24);
  circle(cr, 0, 0, 54);
  cairo_restore(cr);
  stroke(cr, rgb(110, 114, 128), 4.0);
  // The head.
  cairo_translate(cr, 0, -210);
  roundRect(cr, -50, -54, 100, 56, 5);
  vgrad(cr, -54, 2, {{0.0, rgb(110, 114, 128)}, {0.2, rgb(70, 74, 86)}, {1.0, rgb(34, 36, 44)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  cairo_rectangle(cr, 50, -46, 42, 40);
  vgrad(cr, -46, -6, {{0.0, rgb(60, 62, 72)}, {1.0, rgb(24, 24, 30)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  roundRect(cr, 90, -52, 12, 52, 2);
  setRgba(cr, rgb(14, 14, 18), 1.0);
  cairo_fill(cr);
  roundRect(cr, -44, -92, 58, 38, 3);
  setRgba(cr, rgb(40, 42, 50), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.2);
  box(cr, -40, -88, 50, 30, rgb(60, 100, 150), 0.9);
  box(cr, 12, -60, 10, 6, rgb(30, 30, 36));
  // The pan bars.
  cairo_move_to(cr, -50, -12);
  cairo_line_to(cr, -118, 30);
  stroke(cr, rgb(20, 20, 24), 7.0);
  cairo_move_to(cr, -50, -12);
  cairo_line_to(cr, -118, 30);
  stroke(cr, rgb(100, 104, 118), 3.0);
  text(cr, "MAXTV", 0, -20, 11, rgb(220, 224, 236), 0.8);
}

// The edit suite's bank of edit monitors on their shelf (600 x 170; its
// bottom-left at (0, 170)): a timeline, the cut's picture, the audio
// meters; studio speakers either side.
void paintEditBank(cairo_t* cr)
{
  box(cr, 40, 150, 520, 10, kWood);
  box(cr, 40, 160, 520, 6, kWoodLo);
  for (int k = 0; k < 3; ++k)
  {
    const double x = 60 + k * 166;
    roundRect(cr, x, 40, 150, 104, 4);
    vgrad(cr, 40, 144, {{0.0, rgb(80, 84, 96)}, {1.0, rgb(30, 30, 36)}}, 1.0, true);
    stroke(cr, kInk, 1.6);
    box(cr, x + 70, 144, 10, 6, rgb(30, 30, 36));
    const double sx = x + 7, sy = 47, sw = 136, sh = 86;
    box(cr, sx, sy, sw, sh, rgb(16, 18, 26));
    if (k == 0)
    {
      // The timeline: tracks of clips.
      static constexpr Color kClip[4] = {rgb(90, 140, 230), rgb(230, 140, 70), rgb(120, 210, 120), rgb(200, 100, 200)};
      for (int t = 0; t < 5; ++t)
        for (double cxp = sx + 4 + (t * 13) % 20; cxp < sx + sw - 10; cxp += 30 + (t * 7) % 13)
          box(cr, cxp, sy + 12 + t * 14, 22 + (t * 5) % 9, 10, kClip[t % 4], 0.85);
      box(cr, sx, sy, sw, 8, rgb(40, 44, 56));
    }
    else if (k == 1)
    {
      // The cut: a wide shot of the hallway with a tiny runner in it.
      cairo_rectangle(cr, sx, sy, sw, sh);
      vgrad(cr, sy, sy + sh, {{0.0, rgb(40, 50, 80)}, {1.0, rgb(70, 60, 90)}});
      box(cr, sx, sy + sh * 0.72, sw, sh * 0.28, rgb(36, 36, 46));
      box(cr, sx + 60, sy + 36, 8, 26, rgb(220, 50, 50));
      circle(cr, sx + 64, sy + 32, 5);
      setRgba(cr, rgb(250, 210, 80), 1.0);
      cairo_fill(cr);
      text(cr, "TAKE 42", sx + sw * 0.5, sy + 12, 9, rgb(255, 255, 255), 0.85);
    }
    else
    {
      // Audio: a waveform and bar meters.
      cairo_move_to(cr, sx + 4, sy + 30);
      for (int i = 0; i < 64; ++i)
        cairo_line_to(cr, sx + 4 + i * 2, sy + 30 + std::sin(i * 0.9) * (6 + (i * 7) % 11));
      stroke(cr, rgb(110, 255, 150), 1.2);
      for (int i = 0; i < 8; ++i)
      {
        const double hgt = 10 + (i * 13) % 30;
        box(cr, sx + 10 + i * 15, sy + sh - 6 - hgt, 10, hgt, hgt > 30 ? rgb(255, 90, 70) : rgb(110, 255, 150));
      }
    }
    softDot(cr, sx + sw * 0.5, sy + sh * 0.5, sw * 0.7, sh * 0.7, kScreen, 0.15);
  }
  // Speakers.
  for (const double x : {0.0, 560.0})
  {
    roundRect(cr, x, 70, 40, 74, 3);
    vgrad(cr, 70, 144, {{0.0, rgb(50, 50, 58)}, {1.0, rgb(20, 20, 24)}}, 1.0, true);
    stroke(cr, kInk, 1.4);
    circle(cr, x + 20, 86, 7);
    setRgba(cr, rgb(30, 30, 34), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, rgb(90, 90, 100), 1.0);
    circle(cr, x + 20, 118, 14);
    setRgba(cr, rgb(24, 24, 28), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, rgb(90, 90, 100), 1.4);
  }
}

// An office chair (110 x 160; anchor the base's middle on the floor).
void paintChair(cairo_t* cr)
{
  cairo_translate(cr, 55, 160);
  for (const double x : {-40.0, -14.0, 14.0, 40.0})
  {
    cairo_move_to(cr, 0, -26);
    cairo_line_to(cr, x, -6);
    stroke(cr, rgb(40, 40, 46), 5.0);
    circle(cr, x, -4, 4);
    setRgba(cr, rgb(20, 20, 24), 1.0);
    cairo_fill(cr);
  }
  box(cr, -4, -66, 8, 42, kSteelLo);
  roundRect(cr, -36, -78, 72, 16, 6);
  vgrad(cr, -78, -62, {{0.0, rgb(70, 70, 80)}, {1.0, rgb(30, 30, 36)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  roundRect(cr, -32, -156, 22, 82, 8);
  vgrad(cr, -156, -74, {{0.0, rgb(70, 70, 80)}, {1.0, rgb(30, 30, 36)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
}

// A studio door (150 x 260; bottom-left on the floor): grey steel, a
// porthole with light behind, a push plate, the studio's sign over it and
// the ON AIR lamp's housing (lit by the level).
void paintStudioDoor(cairo_t* cr, const char* name)
{
  box(cr, 0, 40, 150, 220, rgb(28, 28, 34));
  cairo_rectangle(cr, 10, 50, 130, 210);
  hgrad(cr, 10, 140, {{0.0, rgb(110, 114, 128)}, {0.5, rgb(84, 88, 100)}, {1.0, rgb(54, 56, 66)}}, 1.0, true);
  stroke(cr, kInk, 1.8);
  cairo_move_to(cr, 75, 50);
  cairo_line_to(cr, 75, 260);
  stroke(cr, kInk, 1.6);
  for (const double x : {42.0, 108.0})
  {
    circle(cr, x, 100, 18);
    setRgba(cr, rgb(30, 30, 36), 1.0);
    cairo_fill(cr);
    circle(cr, x, 100, 14);
    setRgba(cr, rgb(120, 160, 220), 0.7);
    cairo_fill(cr);
    softDot(cr, x - 4, 96, 10, 10, rgb(255, 255, 255), 0.4);
    box(cr, x - 14, 150, 28, 40, rgb(170, 174, 186), 0.8);
  }
  roundRect(cr, 20, 10, 110, 26, 3);
  setRgba(cr, rgb(20, 20, 26), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, rgb(120, 124, 136), 1.0);
  text(cr, name, 75, 29, 14, rgb(230, 232, 240));
  box(cr, 62, 0, 26, 9, rgb(60, 20, 20));
}

// A poster for the show (110 x 150).
void paintPoster(cairo_t* cr, int k)
{
  box(cr, 0, 0, 110, 150, rgb(16, 14, 20));
  cairo_rectangle(cr, 6, 6, 98, 138);
  if (k == 0)
    vgrad(cr, 6, 144, {{0.0, rgb(40, 20, 80)}, {1.0, rgb(200, 40, 100)}});
  else
    vgrad(cr, 6, 144, {{0.0, rgb(10, 40, 60)}, {1.0, rgb(40, 160, 170)}});
  softDot(cr, 55, 80, 40, 40, rgb(255, 220, 120), 0.5);
  for (int f = 0; f < 3; ++f)
  {
    circle(cr, 32 + f * 23, 86, 9);
    box(cr, 24 + f * 23, 96, 16, 30, kInk);
    setRgba(cr, kInk, 1.0);
    cairo_fill(cr);
  }
  text(cr, k == 0 ? "GUNRUNNERS" : "MAXTV", 55, 30, k == 0 ? 12 : 17, rgb(255, 220, 80));
  text(cr, k == 0 ? "SEASON 6" : "LIVE TONIGHT", 55, 136, 9, rgb(240, 240, 250));
}

// A fire extinguisher on its bracket (40 x 100).
void paintExtinguisher(cairo_t* cr)
{
  roundRect(cr, 8, 24, 24, 70, 8);
  hgrad(cr, 8, 32, {{0.0, rgb(255, 110, 100)}, {0.4, rgb(210, 30, 40)}, {1.0, rgb(120, 10, 16)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  box(cr, 14, 14, 12, 10, rgb(40, 40, 46));
  cairo_move_to(cr, 24, 18);
  cairo_curve_to(cr, 38, 22, 38, 60, 30, 70);
  stroke(cr, kInk, 3.0);
  box(cr, 10, 50, 20, 14, rgb(240, 240, 230));
}

// Studio B's news set (520 x 330; bottom-left where the desk stands on the
// riser): the big screen with the NEWS 9 ident and a world map, the curved
// desk in front.
void paintNewsSet(cairo_t* cr)
{
  roundRect(cr, 30, 0, 460, 240, 8);
  setRgba(cr, rgb(20, 20, 26), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, rgb(80, 84, 96), 2.0);
  cairo_rectangle(cr, 42, 12, 436, 216);
  vgrad(cr, 12, 228, {{0.0, rgb(20, 50, 120)}, {1.0, rgb(10, 20, 60)}});
  for (int k = 0; k < 9; ++k)
    softDot(cr, 80 + (k * 53) % 380, 60 + (k * 37) % 140, 40 + k % 3 * 10, 24, rgb(60, 140, 200), 0.6);
  for (double x = 42; x < 478; x += 24)
  {
    cairo_move_to(cr, x, 12);
    cairo_line_to(cr, x, 228);
  }
  for (double y = 12; y < 228; y += 24)
  {
    cairo_move_to(cr, 42, y);
    cairo_line_to(cr, 478, y);
  }
  stroke(cr, rgb(120, 180, 255), 0.6, 0.25);
  box(cr, 42, 170, 436, 34, rgb(200, 20, 30), 0.9);
  text(cr, "NEWS 9", 140, 196, 24, rgb(255, 255, 255));
  text(cr, "BREAKING: RUNNERS LOOSE IN STUDIO", 330, 192, 11, rgb(255, 240, 220));
  // The desk.
  cairo_move_to(cr, 70, 330);
  cairo_line_to(cr, 90, 260);
  cairo_curve_to(cr, 200, 248, 320, 248, 430, 260);
  cairo_line_to(cr, 450, 330);
  cairo_close_path(cr);
  vgrad(cr, 248, 330, {{0.0, rgb(200, 210, 230)}, {0.1, rgb(80, 100, 150)}, {1.0, rgb(24, 30, 50)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  box(cr, 160, 286, 200, 20, rgb(200, 30, 40));
  text(cr, "NEWS 9", 260, 302, 14, rgb(255, 255, 255));
}

// A light stand (120 x 380; anchor the tripod's middle on the floor).
void paintLightStand(cairo_t* cr)
{
  cairo_translate(cr, 60, 380);
  cairo_move_to(cr, -50, 0);
  cairo_line_to(cr, 0, -70);
  cairo_line_to(cr, 50, 0);
  cairo_move_to(cr, 0, -70);
  cairo_line_to(cr, 0, -320);
  stroke(cr, rgb(50, 52, 60), 5.0);
  cairo_save(cr);
  cairo_translate(cr, 0, -330);
  cairo_rotate(cr, 0.5);
  roundRect(cr, -26, -34, 52, 44, 4);
  vgrad(cr, -34, 10, {{0.0, rgb(80, 82, 94)}, {1.0, rgb(30, 30, 36)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  box(cr, -22, 10, 44, 6, rgb(255, 240, 210));
  cairo_restore(cr);
}

// A block of wall cable tray (64 x 48): a steel tray bolted to the wall,
// cables drooping out of it in loops.
void paintWallTray(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 6, 64, 14);
  vgrad(cr, 6, 20, {{0.0, kSteelHi}, {0.3, kSteel}, {1.0, kSteelLo}}, 1.0, true);
  stroke(cr, kInk, 1.0);
  for (double x = 4; x < 64; x += 12)
    box(cr, x, 10, 6, 5, rgb(30, 30, 36));
  static constexpr Color kRun[3] = {rgb(26, 26, 32), rgb(180, 40, 40), rgb(40, 90, 180)};
  for (int k = 0; k < 3; ++k)
  {
    cairo_move_to(cr, -2, 16 + k);
    cairo_curve_to(cr, 16, 30 + k * 6, 46, 30 + k * 5, 66, 16 + k);
    stroke(cr, kRun[k], 3.0);
  }
  box(cr, 30, 0, 4, 8, rgb(60, 62, 72));
}

// A block of catwalk railing (64 x 70; its foot on the walkway): a post
// and the rails.
void paintRail(cairo_t* cr)
{
  box(cr, 2, 4, 6, 66, rgb(200, 170, 40));
  for (const double y : {6.0, 36.0})
  {
    cairo_rectangle(cr, 0, y, 64, 5);
    vgrad(cr, y, y + 5, {{0.0, rgb(250, 220, 90)}, {1.0, rgb(150, 120, 20)}});
  }
  box(cr, 0, 62, 64, 8, rgb(60, 62, 72));
}

// The catwalk's leg (40 x 180; top-middle under the walkway).
void paintCatLeg(cairo_t* cr)
{
  box(cr, 14, 0, 12, 180, rgb(70, 74, 86));
  cairo_move_to(cr, 20, 10);
  cairo_line_to(cr, 0, 60);
  cairo_move_to(cr, 20, 10);
  cairo_line_to(cr, 40, 60);
  stroke(cr, rgb(60, 62, 72), 4.0);
  box(cr, 6, 172, 28, 8, rgb(40, 42, 50));
}

// A cable drum (170 x 110; anchor its middle on the floor).
void paintDrum(cairo_t* cr)
{
  cairo_translate(cr, 70, 110);
  circle(cr, 0, -52, 52);
  setRgba(cr, rgb(110, 76, 44), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 2.0);
  circle(cr, 0, -52, 40);
  setRgba(cr, rgb(24, 24, 28), 1.0);
  cairo_fill(cr);
  for (double rr = 14; rr < 40; rr += 5)
  {
    circle(cr, 0, -52, rr);
    stroke(cr, rgb(60, 60, 70), 1.0, 0.8);
  }
  circle(cr, 0, -52, 11);
  setRgba(cr, rgb(150, 110, 70), 1.0);
  cairo_fill(cr);
  cairo_move_to(cr, 30, -30);
  cairo_curve_to(cr, 60, -8, 80, -2, 100, -4);
  stroke(cr, rgb(24, 24, 28), 5.0);
}

// A sign: a yellow triangle and a line (100 x 80).
void paintTripSign(cairo_t* cr)
{
  box(cr, 0, 0, 100, 80, rgb(250, 250, 240));
  cairo_rectangle(cr, 0, 0, 100, 80);
  stroke(cr, kInk, 2.0);
  cairo_move_to(cr, 50, 6);
  cairo_line_to(cr, 76, 50);
  cairo_line_to(cr, 24, 50);
  cairo_close_path(cr);
  setRgba(cr, kTape, 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 2.4);
  text(cr, "!", 50, 46, 26, kInk);
  text(cr, "TRIP HAZARD", 50, 68, 9.5, kInk);
  text(cr, "CABLES", 50, 77, 7, rgb(60, 60, 70));
}

// A jib arm on its tripod with a camera on its end (460 x 400; anchor the
// tripod's middle on the floor at (300, 400)).
void paintJib(cairo_t* cr)
{
  cairo_translate(cr, 300, 400);
  cairo_move_to(cr, -60, 0);
  cairo_line_to(cr, 0, -150);
  cairo_line_to(cr, 60, 0);
  cairo_move_to(cr, 0, -150);
  cairo_line_to(cr, 0, 0);
  stroke(cr, rgb(50, 52, 62), 6.0);
  // The arm: counterweights on the back, the camera out front.
  cairo_move_to(cr, 140, -110);
  cairo_line_to(cr, -280, -300);
  stroke(cr, rgb(30, 30, 36), 12.0);
  cairo_move_to(cr, 140, -110);
  cairo_line_to(cr, -280, -300);
  stroke(cr, rgb(120, 124, 136), 5.0);
  for (int k = 0; k < 3; ++k)
    box(cr, 110 + k * 12, -118 + k * 5, 10, 34, rgb(40, 40, 46));
  circle(cr, 0, -164, 12);
  setRgba(cr, rgb(70, 72, 84), 1.0);
  cairo_fill(cr);
  roundRect(cr, -300, -320, 60, 36, 4);
  vgrad(cr, -320, -284, {{0.0, rgb(80, 84, 96)}, {1.0, rgb(30, 30, 36)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  roundRect(cr, -330, -314, 32, 24, 3);
  setRgba(cr, rgb(20, 20, 24), 1.0);
  cairo_fill(cr);
}

// The tape library's shelving (624 x 680; its bottom-left on the floor at
// the library's left wall): oak uprights, a dark back panel, the top shelf
// stacked with tape boxes, a sign.
void paintLibrary(cairo_t* cr)
{
  box(cr, 30, 60, 590, 620, rgb(40, 28, 20));
  for (double x = 30; x < 620; x += 28)
    box(cr, x, 60, 2, 620, rgb(30, 20, 14), 0.8);
  for (const double x : {20.0, 600.0})
  {
    cairo_rectangle(cr, x, 40, 20, 640);
    hgrad(cr, x, x + 20, {{0.0, kWoodHi}, {1.0, kWoodLo}}, 1.0, true);
    stroke(cr, kInk, 1.2);
  }
  // The top shelf and its tape boxes.
  box(cr, 30, 150, 580, 12, kWood);
  static constexpr Color kSpine[5] = {rgb(230, 228, 216), rgb(40, 40, 46), rgb(200, 40, 50), rgb(230, 228, 216), rgb(60, 90, 160)};
  for (int i = 0; i < 40; ++i)
  {
    const double x = 40 + i * 14.0;
    const double hgt = 54 + (i * 7) % 12;
    box(cr, x, 150 - hgt, 12, hgt, kSpine[i % 5]);
    box(cr, x + 2, 150 - hgt + 8, 8, 14, rgb(250, 248, 236), 0.9);
  }
  // The sign.
  roundRect(cr, 170, 14, 300, 38, 4);
  setRgba(cr, rgb(20, 20, 26), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kWoodHi, 1.6);
  text(cr, "TAPE LIBRARY  1979-1994", 320, 40, 15, rgb(240, 220, 170));
}

// The gallery's vision mixer (700 x 140; bottom-left on the floor): a long
// console, rows of lit buttons, a T-bar; preview monitors on its hood.
void paintVisionMixer(cairo_t* cr)
{
  for (int k = 0; k < 5; ++k)
  {
    const double x = 60 + k * 120;
    roundRect(cr, x, 0, 108, 56, 3);
    setRgba(cr, rgb(30, 30, 36), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.2);
    cairo_rectangle(cr, x + 5, 5, 98, 46);
    if (k == 2)
      vgrad(cr, 5, 51, {{0.0, rgb(200, 40, 50)}, {1.0, rgb(90, 20, 30)}});
    else
      vgrad(cr, 5, 51, {{0.0, rgb(40, 70, 130)}, {1.0, rgb(20, 30, 60)}});
    text(cr, k == 2 ? "PGM" : "PVW", x + 54, 33, 13, rgb(255, 255, 255), 0.7);
  }
  cairo_move_to(cr, 0, 140);
  cairo_line_to(cr, 20, 70);
  cairo_line_to(cr, 680, 70);
  cairo_line_to(cr, 700, 140);
  cairo_close_path(cr);
  vgrad(cr, 70, 140, {{0.0, rgb(110, 114, 128)}, {0.15, rgb(60, 62, 74)}, {1.0, rgb(24, 24, 30)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  static constexpr Color kBtn[4] = {rgb(255, 60, 60), rgb(80, 255, 120), rgb(255, 220, 80), rgb(120, 190, 255)};
  for (int row = 0; row < 3; ++row)
    for (int i = 0; i < 30; ++i)
      box(cr, 40 + i * 20 + row * 3, 78 + row * 14, 12, 8, i % 7 == row ? kBtn[(i + row) % 4] : rgb(70, 72, 84));
  box(cr, 640, 76, 8, 40, rgb(20, 20, 24));
  box(cr, 630, 86, 28, 8, kSteelHi);
}

// The forgotten studio's set (1280 x 470; bottom-left on the floor): the
// living room of an old sitcom, its wallpaper faded and peeling, a painted
// window, a door flat; a sofa under a dust sheet, a standard lamp, the
// STUDIO D sign; cobwebs in the corners.
void paintSitcom(cairo_t* cr)
{
  // The flats.
  cairo_rectangle(cr, 40, 40, 1200, 430);
  vgrad(cr, 40, 470, {{0.0, rgb(120, 104, 80)}, {1.0, rgb(84, 70, 54)}});
  for (double x = 50; x < 1240; x += 40)
  {
    cairo_move_to(cr, x, 40);
    cairo_line_to(cr, x, 470);
  }
  stroke(cr, rgb(150, 130, 96), 6.0, 0.25);
  for (double x = 70; x < 1240; x += 80)
    for (double y = 80; y < 440; y += 90)
    {
      circle(cr, x, y, 7);
      setRgba(cr, rgb(150, 120, 80), 0.35);
      cairo_fill(cr);
    }
  // Peeling corners and water stains.
  softDot(cr, 300, 120, 80, 50, rgb(60, 50, 34), 0.35);
  softDot(cr, 900, 300, 100, 60, rgb(60, 50, 34), 0.3);
  cairo_move_to(cr, 1240, 40);
  cairo_line_to(cr, 1180, 40);
  cairo_curve_to(cr, 1200, 70, 1220, 80, 1240, 110);
  cairo_close_path(cr);
  setRgba(cr, rgb(60, 50, 40), 1.0);
  cairo_fill(cr);
  // The seams between flats, the struts showing at the top.
  for (const double x : {440.0, 840.0})
  {
    cairo_move_to(cr, x, 40);
    cairo_line_to(cr, x, 470);
    stroke(cr, rgb(40, 32, 24), 3.0);
  }
  box(cr, 40, 30, 1200, 12, rgb(90, 70, 50));
  // The painted window: a sunny day long gone dim.
  roundRect(cr, 160, 110, 220, 180, 4);
  vgrad(cr, 110, 290, {{0.0, rgb(110, 150, 170)}, {1.0, rgb(150, 160, 140)}}, 1.0, true);
  stroke(cr, rgb(220, 210, 180), 10.0);
  cairo_move_to(cr, 270, 110);
  cairo_line_to(cr, 270, 290);
  cairo_move_to(cr, 160, 200);
  cairo_line_to(cr, 380, 200);
  stroke(cr, rgb(220, 210, 180), 6.0);
  softDot(cr, 220, 160, 40, 30, rgb(255, 255, 220), 0.4);
  // The door flat.
  cairo_rectangle(cr, 960, 170, 140, 300);
  vgrad(cr, 170, 470, {{0.0, rgb(130, 90, 60)}, {1.0, rgb(80, 54, 34)}}, 1.0, true);
  stroke(cr, rgb(60, 40, 26), 4.0);
  circle(cr, 1080, 330, 7);
  setRgba(cr, rgb(200, 170, 90), 1.0);
  cairo_fill(cr);
  // The sign.
  roundRect(cr, 560, 70, 220, 60, 4);
  setRgba(cr, rgb(30, 26, 22), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, rgb(160, 140, 110), 2.0);
  text(cr, "STUDIO D", 670, 100, 22, rgb(230, 210, 170));
  text(cr, "CLOSED 1987", 670, 122, 12, rgb(200, 60, 50));
  // The sofa under its dust sheet.
  cairo_move_to(cr, 480, 470);
  cairo_curve_to(cr, 470, 400, 500, 360, 540, 350);
  cairo_curve_to(cr, 640, 330, 760, 330, 840, 350);
  cairo_curve_to(cr, 880, 360, 900, 400, 890, 470);
  cairo_close_path(cr);
  vgrad(cr, 330, 470, {{0.0, rgb(230, 224, 206)}, {1.0, rgb(150, 144, 128)}}, 1.0, true);
  stroke(cr, rgb(90, 84, 70), 2.0);
  for (int k = 0; k < 5; ++k)
  {
    cairo_move_to(cr, 520 + k * 80, 470);
    cairo_curve_to(cr, 540 + k * 80, 420, 520 + k * 80, 390, 560 + k * 76, 360);
    stroke(cr, rgb(170, 164, 146), 2.0, 0.7);
  }
  // A standard lamp, shade askew.
  box(cr, 1140, 230, 6, 240, rgb(60, 50, 40));
  cairo_move_to(cr, 1104, 240);
  cairo_line_to(cr, 1180, 230);
  cairo_line_to(cr, 1166, 186);
  cairo_line_to(cr, 1118, 192);
  cairo_close_path(cr);
  setRgba(cr, rgb(190, 160, 110), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, rgb(90, 70, 50), 2.0);
  // Cobwebs in the top corners.
  for (const double cx : {40.0, 1240.0})
  {
    const double s = cx < 600 ? 1.0 : -1.0;
    for (int k = 1; k <= 4; ++k)
    {
      cairo_move_to(cr, cx + s * 22 * k, 40);
      cairo_curve_to(cr, cx + s * 18 * k, 40 + 10 * k, cx + s * 8 * k, 40 + 18 * k, cx, 40 + 22 * k);
    }
    for (int k = 0; k < 4; ++k)
    {
      cairo_move_to(cr, cx, 40);
      cairo_line_to(cr, cx + s * 100 * std::cos(k * 0.45), 40 + 100 * std::sin(k * 0.45));
    }
    stroke(cr, rgb(230, 230, 230), 1.0, 0.5);
  }
}

// An old tube camera on a wooden tripod, cobwebbed (240 x 320; anchor the
// tripod's middle on the floor at (120, 320)).
void paintOldCam(cairo_t* cr)
{
  cairo_translate(cr, 120, 320);
  cairo_move_to(cr, -70, 0);
  cairo_line_to(cr, 0, -170);
  cairo_line_to(cr, 70, 0);
  cairo_move_to(cr, 0, -170);
  cairo_line_to(cr, 10, 0);
  stroke(cr, rgb(110, 80, 50), 7.0);
  roundRect(cr, -70, -270, 130, 90, 8);
  vgrad(cr, -270, -180, {{0.0, rgb(150, 150, 140)}, {1.0, rgb(80, 80, 74)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  // The lens turret: four lenses.
  for (int k = 0; k < 4; ++k)
  {
    const double a = k * kPi * 0.5 + 0.4;
    circle(cr, 74 + std::cos(a) * 18, -226 + std::sin(a) * 18, 10);
    setRgba(cr, rgb(30, 30, 34), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, rgb(120, 120, 120), 1.2);
  }
  text(cr, "MAXTV", -6, -220, 14, rgb(60, 60, 60), 0.8);
  // Dust and cobwebs.
  box(cr, -70, -272, 130, 6, rgb(200, 196, 180), 0.6);
  for (int k = 1; k < 4; ++k)
  {
    cairo_move_to(cr, -70, -180 - k * 12);
    cairo_curve_to(cr, -60, -186 - k * 10, -40, -170 + k * 4, -20 * k, -150 + k * 6);
  }
  stroke(cr, rgb(230, 230, 230), 0.8, 0.6);
}

// The virus room's corrupted screen (780 x 420; bottom-left on the floor):
// a big monitor wall gone red, a pixel skull on it, ERROR codes.
void paintVirusScreen(cairo_t* cr)
{
  roundRect(cr, 0, 0, 780, 300, 6);
  setRgba(cr, rgb(20, 14, 16), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, rgb(90, 40, 40), 2.0);
  cairo_rectangle(cr, 14, 14, 752, 272);
  vgrad(cr, 14, 286, {{0.0, rgb(120, 10, 20)}, {1.0, rgb(40, 4, 8)}});
  // The skull, in big pixels.
  static const char* const kSkull[] = {"..XXXXX..", ".XXXXXXX.", "XXXXXXXXX", "XX..X..XX", "XX..X..XX", "XXXXXXXXX",
    ".XXX.XXX.", "..X.X.X..", "..XXXXX.."};
  for (int y = 0; y < 9; ++y)
    for (int x = 0; x < 9; ++x)
      if (kSkull[y][x] == 'X')
        box(cr, 300 + x * 20, 40 + y * 20, 19, 19, rgb(255, 220, 220));
  text(cr, "VIRUS DETECTED", 390, 250, 26, rgb(255, 230, 230));
  for (int k = 0; k < 6; ++k)
  {
    char buf[24];
    std::snprintf(buf, sizeof(buf), "ERR 0x%04X", 0x3900 + k * 0x113);
    text(cr, buf, 110, 50 + k * 34, 13, rgb(255, 120, 120), 0.8);
    text(cr, buf, 670, 50 + k * 34, 13, rgb(255, 120, 120), 0.8);
  }
  box(cr, 360, 300, 60, 120, rgb(30, 20, 22));
}

} // namespace

void drawGallerySet(Renderer& r, const World& w, const Art& art, float camX, float camY, int frame, float alpha)
{
  (void)alpha;
  const auto& s = w.gallery();
  const Level& lv = w.level();
  if (s.rewind || lv.width != 140 || lv.height != 60)
    return; // the rooms are Level 39's own
  auto bx = [&](float tx) { return tx * kTilePx - camX; };
  auto by = [&](float ty) { return ty * kTilePx - camY; };
  auto put = [&](const Texture& t, float tx, float ty, const DrawOpts& o = {}) {
    const float x = bx(tx), y = by(ty);
    if (!offScreen(x - t.anchorX() * o.scale, y - t.anchorY() * o.scale, float(t.w()) * o.scale, float(t.h()) * o.scale))
      r.draw(t, x, y, o);
  };
  const Texture& pedcam = baked(art, r, "pedcam", 220, 320, 110.0f, 316.0f, paintPedCam);
  const Texture& pedcamL = bakedSprite(art, r, "pedcam", 220, 320, 110.0f, 316.0f, paintPedCam).facing[1];
  const Texture& stand = baked(art, r, "lightstand", 120, 380, 60.0f, 380.0f, paintLightStand);

  // R1, the edit suite: the bank of edit monitors, the producer's chair.
  put(baked(art, r, "editbank", 600, 170, 0.0f, 170.0f, paintEditBank), 5.4f, 51.9f);
  {
    // The timeline's playhead creeping along.
    const float x = bx(5.4f) + 67.0f + float((frame / 3) % 136), y = by(51.9f) - 170.0f + 47.0f;
    if (!offScreen(x, y, 4.0f, 86.0f))
      r.fillRect(x, y, 2.0f, 86.0f, rgb(255, 80, 70));
  }
  put(baked(art, r, "chair", 110, 160, 55.0f, 160.0f, paintChair), 14.6f, 56.0f);

  // R2, the hallway: studio doors with their ON AIR lamps, posters, an
  // extinguisher.
  static constexpr struct
  {
    float x;
    const char* name;
    bool live;
  } kDoors[] = {{28.6f, "STUDIO A", true}, {36.4f, "STUDIO C", false}, {47.0f, "GREEN ROOM", false}};
  for (const auto& d : kDoors)
  {
    const std::string key = std::string("door/") + d.name;
    const char* name = d.name;
    put(baked(art, r, key, 150, 260, 0.0f, 260.0f, [name](cairo_t* cr) { paintStudioDoor(cr, name); }), d.x, 56.0f);
    if (d.live)
    {
      const float x = bx(d.x) + 62.0f, y = by(56.0f) - 260.0f;
      if (!offScreen(x, y, 26.0f, 9.0f))
      {
        r.fillRect(x, y, 26.0f, 9.0f, kTally);
        drawGlow(r, art, x + 13.0f, y + 4.0f, 40.0f, kTally, 0.55f);
      }
    }
  }
  put(baked(art, r, "poster0", 110, 150, 0.0f, 0.0f, [](cairo_t* cr) { paintPoster(cr, 0); }), 32.6f, 50.7f);
  put(baked(art, r, "poster1", 110, 150, 0.0f, 0.0f, [](cairo_t* cr) { paintPoster(cr, 1); }), 40.8f, 50.9f);
  put(baked(art, r, "extinguisher", 40, 100, 20.0f, 100.0f, paintExtinguisher), 55.6f, 55.4f);

  // R3, studio B: the cyclorama lit blue, the news set behind the riser,
  // cameras on their pedestals aimed at it, light stands.
  {
    const float x0 = bx(63.0f), x1 = bx(96.0f), y0 = by(47.0f), y1 = by(56.0f);
    if (!offScreen(x0, y0, x1 - x0, y1 - y0))
    {
      r.fillRect(x0, y0, x1 - x0, y1 - y0, rgba(40, 56, 96, 150));
      r.fillRect(x0, y1 - 48.0f, x1 - x0, 48.0f, rgba(60, 80, 130, 120));
      drawGlow(r, art, bx(78.0f), by(51.0f), 520.0f, rgb(80, 120, 220), 0.25f);
    }
  }
  put(baked(art, r, "newsset", 520, 330, 0.0f, 330.0f, paintNewsSet), 74.0f, 54.0f);
  put(pedcam, 69.0f, 56.0f);
  put(pedcamL, 91.3f, 56.0f);
  put(stand, 63.6f, 56.0f);
  put(stand, 84.0f, 56.0f);

  // R4, the cable corridor: wall trays of cable, the TRIP HAZARD sign,
  // the catwalk's railing and legs, cable drums under it.
  {
    const Texture& tray = baked(art, r, "walltray", 64, 48, 0.0f, 0.0f, paintWallTray);
    tile(r, tray, bx(99.0f), by(47.6f), bx(139.0f), by(47.6f) + 48.0f);
    tile(r, tray, bx(99.0f), by(49.4f), bx(139.0f), by(49.4f) + 48.0f);
  }
  put(baked(art, r, "tripsign", 100, 80, 0.0f, 0.0f, paintTripSign), 100.6f, 51.4f);
  {
    const Texture& rail = baked(art, r, "rail", 64, 70, 0.0f, 70.0f, paintRail);
    for (int x = 108; x <= 136; ++x)
      put(rail, float(x), 53.0f);
    const Texture& leg = baked(art, r, "catleg", 40, 180, 20.0f, 0.0f, paintCatLeg);
    for (int x = 109; x <= 136; x += 5)
      put(leg, float(x) + 0.5f, 53.3f);
  }
  const Texture& drum = baked(art, r, "drum", 170, 110, 70.0f, 110.0f, paintDrum);
  put(drum, 121.0f, 56.0f);
  put(drum, 131.0f, 56.0f);

  // R5, the panning room: a jib swung out over the stage, light stands.
  put(baked(art, r, "jib", 460, 400, 300.0f, 400.0f, paintJib), 33.0f, 36.0f);
  put(stand, 17.0f, 36.0f);
  put(stand, 27.0f, 36.0f);

  // R6, the tape library's shelving.
  put(baked(art, r, "library", 624, 680, 0.0f, 680.0f, paintLibrary), 49.2f, 36.0f);

  // R7, the main gallery: the vision mixer in G1, the walls of screens in
  // G2 and G3.
  put(baked(art, r, "visionmixer", 700, 140, 0.0f, 140.0f, paintVisionMixer), 73.2f, 36.0f);
  {
    // Its buttons blinking.
    const float x = bx(73.2f), y = by(36.0f) - 140.0f;
    if (!offScreen(x, y, 700.0f, 140.0f))
      for (int k = 0; k < 6; ++k)
      {
        const std::uint32_t h = hash2(frame / 20, k);
        r.fillRect(x + 40.0f + float(h % 30u) * 20.0f, y + 78.0f + float((h >> 5) % 3u) * 14.0f, 12.0f, 8.0f,
          (h >> 9) % 2u ? rgb(255, 80, 70) : rgb(110, 255, 150), Blend::Add);
      }
  }
  // (The walls of screens are drawn with drawGalleryBack's live feeds.)

  // R8, the forgotten studio: the old sitcom's living room, the old camera.
  put(baked(art, r, "sitcom", 1280, 470, 0.0f, 470.0f, paintSitcom), 2.0f, 16.0f);
  put(baked(art, r, "oldcam", 240, 320, 120.0f, 320.0f, paintOldCam), 29.0f, 16.0f);

  // R10, the virus room's corrupted screen, glitching.
  put(baked(art, r, "virusscreen", 780, 420, 0.0f, 420.0f, paintVirusScreen), 79.0f, 16.0f);
  {
    const float x = bx(79.0f), y = by(16.0f) - 420.0f;
    if (!offScreen(x, y, 780.0f, 300.0f))
    {
      drawGlow(r, art, x + 390.0f, y + 150.0f, 500.0f, rgb(255, 30, 40), 0.25f + 0.1f * float((frame / 7) % 2));
      for (int k = 0; k < 7; ++k)
      {
        const std::uint32_t h = hash2(frame / 3, k + 71);
        r.fillRect(x + 14.0f + float(h % 700u), y + 14.0f + float((h >> 10) % 260u), 20.0f + float((h >> 3) % 80u),
          4.0f + float((h >> 7) % 10u), (h >> 12) % 2u ? rgba(255, 255, 255, 120) : rgba(0, 0, 0, 160));
      }
    }
  }
}

// --- Over the tiles, behind the runner -------------------------------------------------------

void World::drawGalleryBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& s = mGallery;
  const bool level39 = !s.rewind && mLevel->width == 140 && mLevel->height == 60;
  auto bx = [&](float tx) { return tx * kTilePx - camX; };
  auto by = [&](float ty) { return ty * kTilePx - camY; };
  auto put = [&](const Texture& t, float tx, float ty, const DrawOpts& o = {}) {
    const float x = bx(tx), y = by(ty);
    if (!offScreen(x - t.anchorX() * o.scale, y - t.anchorY() * o.scale, float(t.w()) * o.scale, float(t.h()) * o.scale))
      r.draw(t, x, y, o);
  };

  if (level39)
  {
    // The mixing desk under its one-way top (the edit suite).
    int d0 = -1, d1 = -1;
    for (int x = 2; x < 20; ++x)
      if (mMap.block(x, 53) == Tile::Platform)
      {
        if (d0 < 0)
          d0 = x;
        d1 = x;
      }
    if (d0 >= 0)
    {
      const int n = d1 - d0 + 1;
      put(baked(mArt, r, "mixdesk" + std::to_string(n), n * 64 + 24, 210, 12.0f, 8.0f, [n](cairo_t* cr) { paintMixDesk(cr, n); }),
        float(d0), 53.0f);
      // The meters' needles twitching.
      for (int k = 0; k < 4; ++k)
      {
        const float mx = bx(float(d0)) + 30.0f + float(k) * float(n * 64 - 60) / 3.0f, my = by(53.0f) - 3.0f;
        if (offScreen(mx - 30.0f, my - 20.0f, 60.0f, 20.0f))
          continue;
        const float a = -0.9f + 0.5f * std::sin(float(frame) * 0.21f + float(k) * 1.7f) + 0.2f * unit(hash2(frame / 2, k));
        r.drawLine(mx, my, mx + std::sin(a) * 11.0f, my - std::cos(a) * 11.0f, 1.5f, kInk);
      }
    }
    // The library's shelf planks over their one-way tops, a year on each.
    for (int y : {30, 33})
      for (int x = 47; x < 60; ++x)
        if (mMap.block(x, y) == Tile::Platform)
        {
          const int year = 1979 + ((x - 47) + (33 - y) * 3) % 16;
          put(baked(mArt, r, "plank" + std::to_string(year), 64, 30, 0.0f, 0.0f, [year](cairo_t* cr) { paintPlank(cr, year); }),
            float(x), float(y));
        }
    // The shutter down between the library and its landing.
    {
      const Texture& sh = baked(mArt, r, "shutter", 64, 64, 0.0f, 0.0f, paintShutter);
      for (int y = 20; y < 36; ++y)
        if (mMap.block(60, y) == Tile::Solid)
          put(sh, 60.0f, float(y));
      put(baked(mArt, r, "shutterfoot", 64, 48, 0.0f, 48.0f, paintShutterFoot), 60.0f, 36.0f);
    }
    // The edit suite's locked door in its wall.
    if (mMap.block(21, 52) == Tile::Solid)
      put(baked(mArt, r, "lockdoor", 64, 256, 0.0f, 0.0f, paintLockedDoor), 21.0f, 52.0f);
    // The producer's booth sealed off at x 83: its glass door, a force
    // field shimmering on it.
    {
      const float x = bx(83.0f), y = by(32.0f);
      if (!offScreen(x, y, 64.0f, 256.0f) && mMap.block(83, 33) == Tile::Solid)
      {
        r.fillRect(x + 4.0f, y, 56.0f, 256.0f, rgb(30, 40, 60));
        r.fillRect(x + 8.0f, y + 8.0f, 48.0f, 248.0f, rgba(90, 160, 255, 70));
        for (int k = 0; k < 8; ++k)
        {
          const float yy = y + std::fmod(float(frame * 3 + k * 37), 256.0f);
          r.fillRect(x + 8.0f, yy, 48.0f, 3.0f, rgba(160, 220, 255, 120), Blend::Add);
        }
        drawGlow(r, mArt, x + 32.0f, y + 128.0f, 120.0f, rgb(90, 160, 255), 0.35f + 0.1f * std::sin(float(frame) * 0.2f));
        r.drawText("SEALED", x + 32.0f, y - 22.0f, {14.0f, rgb(200, 230, 255), kHudInk}, Align::Center);
      }
    }
    // The panning room's dolly track along its floor.
    {
      const Texture& tr = baked(mArt, r, "track", 64, 18, 0.0f, 0.0f, paintTrack);
      tile(r, tr, bx(6.0f), by(36.0f) - 14.0f, bx(41.0f), by(36.0f) + 4.0f);
    }
    // The light rig over its one-way top in G3, its lamps lighting the floor.
    {
      const Texture& tr = baked(mArt, r, "truss", 64, 44, 0.0f, 4.0f, paintTruss);
      for (int x = 120; x < 139; ++x)
        if (mMap.block(x, 30) == Tile::Platform)
        {
          put(tr, float(x), 30.0f);
          const float lx = bx(float(x) + 0.5f), ly = by(31.0f);
          if (offScreen(lx - 160.0f, ly, 320.0f, 320.0f))
            continue;
          r.fillRect(lx - 9.0f, ly - 18.0f, 18.0f, 20.0f, rgb(40, 40, 46));
          r.fillRect(lx - 7.0f, ly + 1.0f, 14.0f, 4.0f, rgb(255, 240, 200));
          for (int k = 0; k < 6; ++k)
          {
            const float t0 = float(k) / 6.0f;
            r.fillRect(lx - 8.0f - t0 * 70.0f, ly + 6.0f + t0 * 300.0f, 16.0f + t0 * 140.0f, 50.0f, rgba(255, 236, 190, 10), Blend::Add);
          }
        }
    }
  }

  // The CAM 0 display and its wall camera, the ON AIR sign (over their
  // deco boxes).
  for (const auto& pr : mProps)
  {
    if (pr.kind != PropKind::TextSign)
      continue;
    const float px = float(pr.x) * kCellPx - camX, py = float(pr.y) * kCellPx - camY;
    if (pr.text == "ON AIR")
    {
      const float x = px + float(pr.w) * kCellPx * 0.5f - 72.0f, y = py - 8.0f;
      if (offScreen(x, y, 144.0f, 80.0f))
        continue;
      const bool lit = (frame / 45) % 8 != 7;
      if (lit)
        drawGlow(r, mArt, x + 72.0f, y + 40.0f, 140.0f, kTally, 0.45f);
      r.draw(baked(mArt, r, lit ? "onair1" : "onair0", 144, 80, 0.0f, 0.0f, [lit](cairo_t* cr) { paintOnAir(cr, lit); }), x, y);
    }
    else if (pr.text == "CAM 0")
    {
      // The wall camera over the deco box, aimed down at the room.
      const float wx = px + float(pr.w) * kCellPx * 0.5f - 50.0f, wy = py + 10.0f;
      r.fillRect(px, py, float(pr.w) * kCellPx, float(pr.h) * kCellPx, rgb(36, 40, 52));
      r.draw(baked(mArt, r, "wallcam", 100, 80, 0.0f, 0.0f, paintWallCam), wx, wy);
      const bool on = tallyOn(frame / 4);
      r.fillRect(wx + 52.0f, wy + 32.0f, 7.0f, 7.0f, on ? kTally : kTallyOff);
      if (on)
        drawGlow(r, mArt, wx + 55.0f, wy + 35.0f, 22.0f, kTally, 0.6f);
      // The display under it, low on the wall where the runner sees it:
      // CAM 0's picture of a living room where someone plays this game,
      // and on that room's TV the game itself, live.
      const float dx = float(pr.x + pr.w) * kCellPx - 264.0f - camX + 4.0f, dy = 49.0f * kTilePx - camY;
      if (offScreen(dx, dy, 264.0f, 184.0f))
        continue;
      r.draw(baked(mArt, r, "cam0frame", 264, 184, 0.0f, 0.0f, paintCam0Frame), dx, dy);
      const float sx = dx + 14.0f, sy = dy + 14.0f;
      r.draw(baked(mArt, r, "livingroom", 236, 144, 0.0f, 0.0f, paintLivingRoom), sx, sy);
      const View v{camX, camY, float(kScreenW), float(kScreenH), sx + 38.0f, sy + 14.0f, 160.0f, 90.0f};
      drawLiveView(r, *this, mArt, mTheme, v, frame, alpha);
      r.fillRect(v.sx, v.sy, v.sw, v.sh, rgba(140, 180, 255, 30), Blend::Add);
      for (float yy = v.sy; yy < v.sy + v.sh; yy += 2.0f)
        r.fillRect(v.sx, yy, v.sw, 1.0f, rgba(0, 0, 0, 40));
      r.draw(baked(mArt, r, "sofa", 236, 160, 0.0f, 0.0f, paintSofa), sx, sy);
      // The camera's own scanlines and REC.
      for (float yy = sy; yy < sy + 144.0f; yy += 3.0f)
        r.fillRect(sx, yy, 236.0f, 1.0f, rgba(0, 0, 0, 50));
      if ((frame / 30) % 2 == 0)
        r.fillRect(sx + 8.0f, sy + 8.0f, 8.0f, 8.0f, kTally);
      r.drawText("CAM 0", dx + 132.0f, dy + 164.0f, {11.0f, kUmd, 0}, Align::Center);
    }
  }

  // CAM 42: a note taped up on the wall over its dead monitor (over the
  // level's `@ deco kind=42`).
  for (const auto& pr : mProps)
  {
    if (pr.kind != PropKind::Deco42)
      continue;
    const float x = (float(pr.x) + float(pr.w) * 0.5f) * kCellPx - camX - 66.0f, y = (float(pr.y) + float(pr.h) * 0.5f) * kCellPx - camY - 36.0f;
    if (!offScreen(x, y, 132.0f, 72.0f))
      r.draw(baked(mArt, r, "outoforder", 132, 72, 0.0f, 0.0f, paintOutOfOrder), x, y);
  }

  // The candid tapes, on their shelves.
  for (const auto& t : mSignal.treasures)
  {
    if (t.taken || t.art != "candid_tape")
      continue;
    const float x = float(t.x) * kTilePx - camX, y = float(t.y) * kTilePx - camY;
    if (offScreen(x, y, kTilePx, kTilePx))
      continue;
    const int n = t.x * 3 + t.y;
    drawGlow(r, mArt, x + 32.0f, y + 40.0f, 46.0f + 4.0f * std::sin(float(frame) * 0.1f + float(n)), rgb(255, 220, 140), 0.35f);
    r.draw(baked(mArt, r, "tape" + std::to_string(n % 6), 64, 64, 0.0f, 0.0f, [n](cairo_t* cr) { paintCandidTape(cr, n % 6); }), x, y);
    const int ph = (frame + n * 17) % 120;
    if (ph < 14)
    {
      const float k = std::sin(float(ph) / 14.0f * float(kPi));
      const float sx = x + 14.0f + float(n * 7 % 36), sy = y + 26.0f;
      r.drawLine(sx - 8.0f * k, sy, sx + 8.0f * k, sy, 2.0f, rgb(255, 250, 220), Blend::Add);
      r.drawLine(sx, sy - 8.0f * k, sx, sy + 8.0f * k, 2.0f, rgb(255, 250, 220), Blend::Add);
    }
  }

  if (level39)
  {
    // The walls of screens in G2 and G3: pictures, snow, and in the middle
    // of each the programme feed, live.
    for (const float x0 : {96.6f, 118.6f})
    {
      const float wx = bx(x0), wy = by(27.4f), ww = 19.8f * kTilePx, wh = 4.1f * kTilePx;
      if (offScreen(wx, wy, ww, wh))
        continue;
      r.fillRect(wx - 10.0f, wy - 10.0f, ww + 20.0f, wh + 20.0f, rgb(16, 16, 22));
      const int cols = 6, rows = 2;
      const float gap = 10.0f, cw = (ww - gap * float(cols - 1)) / float(cols), ch = (wh - gap * float(rows - 1)) / float(rows);
      for (int row = 0; row < rows; ++row)
        for (int col = 0; col < cols; ++col)
        {
          const float x = wx + float(col) * (cw + gap), y = wy + float(row) * (ch + gap);
          r.fillRect(x - 3.0f, y - 3.0f, cw + 6.0f, ch + 6.0f, rgb(40, 42, 52));
          const int k = (col * 5 + row * 3 + int(x0)) % 7;
          if ((col == 2 || col == 3) && row == 0)
          {
            if (col == 3)
              continue;
            // The programme feed: what the studio's on-air camera sees.
            const float fw = cw * 2.0f + gap;
            const View v{camX + 160.0f, camY + 90.0f, float(kScreenW) - 320.0f, float(kScreenH) - 180.0f, x, y, fw,
              fw * (float(kScreenH) - 180.0f) / (float(kScreenW) - 320.0f)};
            drawLiveView(r, *this, mArt, mTheme, View{v.wx, v.wy, v.ww, v.wh, x, y, fw, std::min(ch, v.sh)}, frame, alpha);
            r.fillRect(x + 6.0f, y + 6.0f, 52.0f, 18.0f, kTally);
            r.drawText("PGM", x + 32.0f, y + 6.0f, {14.0f, rgb(255, 255, 255), 0}, Align::Center);
          }
          else if (k == 0 || k == 4)
            snow(r, mArt, x, y, cw, ch, frame / 2 + col * 7 + row * 13, 0.85f);
          else
          {
            // A picture, cutting to another now and then; a roll of interference.
            const int pic = (k + col + int(hash2((frame + col * 97 + row * 41) / 300, col + row * 7 + int(x0)) % 3u)) % 7;
            DrawOpts po;
            po.scale = std::max(cw / 200.0f, ch / 120.0f);
            po.alpha = (hash2(frame / 5, col * 3 + row) % 23u) == 0 ? 0.7f : 1.0f;
            Clip clip(r, x, y, cw, ch);
            r.draw(baked(mArt, r, "wallpic" + std::to_string(pic), 200, 120, 0.0f, 0.0f, [pic](cairo_t* cr) { paintWallPic(cr, pic); }),
              x + (cw - 200.0f * po.scale) * 0.5f, y + (ch - 120.0f * po.scale) * 0.5f, po);
            const float bar = std::fmod(float(frame) * 0.7f + float(col * 31 + row * 17), ch + 20.0f) - 10.0f;
            r.fillRect(x, y + std::max(0.0f, bar), cw, 6.0f, rgba(255, 255, 255, 22), Blend::Add);
          }
          for (float yy = y; yy < y + ch; yy += 3.0f)
            r.fillRect(x, yy, cw, 1.0f, rgba(0, 0, 0, 36));
        }
      drawGlow(r, mArt, wx + ww * 0.5f, wy + wh * 0.5f, ww * 0.5f, kScreen, 0.12f);
    }
    // The forgotten studio: dust drifting in the dark.
    {
      const float x0 = bx(1.0f), y0 = by(2.0f), x1 = bx(41.0f), y1 = by(16.0f);
      if (!offScreen(x0, y0, x1 - x0, y1 - y0))
        for (int k = 0; k < 60; ++k)
        {
          const std::uint32_t h = hash2(k, 3911);
          const float px = x0 + std::fmod(float(h % 2560u) + float(frame) * (0.15f + unit(h >> 3) * 0.2f), x1 - x0);
          const float py = y0 + 300.0f + std::fmod(float((h >> 11) % 600u) + std::sin(float(frame) * 0.01f + float(k)) * 30.0f, y1 - y0 - 300.0f);
          r.fillRect(px, py, 2.0f, 2.0f, rgba(255, 240, 210, 40 + int((h >> 5) % 60u)));
        }
    }
  }

  // The studio monitors.
  for (std::size_t i = 0; i < s.monitors.size(); ++i)
  {
    const Monitor& m = s.monitors[i];
    const float X = float(m.x) * kTilePx - camX, Y = float(m.y) * kTilePx - camY;
    if (offScreen(X, Y - 30.0f, 128.0f, 222.0f))
      continue;
    const Tile under = mMap.block(m.x, m.y + 3), under2 = mMap.block(m.x + 1, m.y + 3);
    const bool cart = under == Tile::Solid || under == Tile::Platform || under2 == Tile::Solid || under2 == Tile::Platform;
    r.draw(baked(mArt, r, cart ? "monitor_cart" : "monitor_wall", 160, 228, 16.0f, 24.0f, [cart](cairo_t* cr) { paintMonitor(cr, cart); }),
      X, Y);
    const float sx = X + kScrX, sy = Y + kScrY;
    int ex = 0, ey = 0;
    const bool leads = monitorExit(m, ex, ey);
    const bool entering = s.hop > 0 && s.hopTo == int(i);
    if (entering)
    {
      // Going in: the screen floods white with snow.
      snow(r, mArt, sx, sy, kScrW, kScrH, frame + int(i) * 31, 1.0f);
      r.fillRect(sx, sy, kScrW, kScrH, rgba(255, 255, 255, 60 + 30 * (kGlHop - s.hop)), Blend::Add);
    }
    else if (m.virus)
    {
      // VIRUS WARNING, flashing.
      const bool red = (frame / 6) % 2 == 0;
      r.fillRect(sx, sy, kScrW, kScrH, red ? rgb(210, 20, 30) : rgb(20, 4, 6));
      for (int k = 0; k < 4; ++k)
      {
        const std::uint32_t h = hash2(frame / 2, k + int(i) * 5);
        r.fillRect(sx, sy + float(h % 60u), kScrW, 2.0f + float((h >> 6) % 4u), rgba(255, 255, 255, 60));
      }
      const Color ink = red ? rgb(255, 255, 255) : rgb(255, 50, 50);
      r.drawText("VIRUS", sx + kScrW * 0.5f, sy + 8.0f, {20.0f, ink, 0}, Align::Center);
      r.drawText("WARNING", sx + kScrW * 0.5f, sy + 34.0f, {15.0f, ink, 0}, Align::Center);
      drawGlow(r, mArt, sx + kScrW * 0.5f, sy + kScrH * 0.5f, 110.0f, rgb(255, 30, 40), red ? 0.4f : 0.15f);
    }
    else if (m.dead || !leads)
    {
      // Dead: faint snow behind a cracked screen.
      r.fillRect(sx, sy, kScrW, kScrH, rgb(14, 16, 20));
      snow(r, mArt, sx, sy, kScrW, kScrH, frame / 3 + int(i) * 17, 0.28f);
      r.draw(baked(mArt, r, "crack", 110, 64, 0.0f, 0.0f, paintCrack), sx, sy);
    }
    else
    {
      const bool noSignal = m.label == "NO SIGNAL";
      const bool glimpse = noSignal && (frame % 200) < 14;
      if (noSignal && !glimpse)
      {
        snow(r, mArt, sx, sy, kScrW, kScrH, frame / 2 + int(i) * 13, 0.95f);
        r.fillRect(sx + 14.0f, sy + 22.0f, kScrW - 28.0f, 20.0f, rgba(0, 0, 0, 170));
        r.drawText("NO SIGNAL", sx + kScrW * 0.5f, sy + 24.0f, {13.0f, rgb(255, 255, 255), 0}, Align::Center);
      }
      else
      {
        // The live view round the exit spot; a panning camera's slides.
        float cxp = float(ex) * kCellPx;
        if (m.cam >= 0)
          cxp = (camAt(s.cams[std::size_t(m.cam)], float(s.t) - 1.0f + alpha) + 0.5f) * kCellPx;
        const float ww = 12.0f * kTilePx, wh = 7.0f * kTilePx;
        const View v{cxp - ww * 0.5f, float(ey) * kCellPx - 10.0f * kCellPx, ww, wh, sx, sy, kScrW, kScrH};
        drawLiveView(r, *this, mArt, mTheme, v, frame, alpha);
        if (glimpse)
          snow(r, mArt, sx, sy, kScrW, kScrH, frame + int(i), 0.5f);
        if (m.cam >= 0)
        {
          // The camera's crosshair and which way it pans.
          const CamSpot& c = s.cams[std::size_t(m.cam)];
          const float ccx = sx + kScrW * 0.5f, ccy = sy + kScrH * 0.62f;
          r.fillRect(ccx - 8.0f, ccy, 16.0f, 1.0f, rgba(255, 255, 255, 160));
          r.fillRect(ccx, ccy - 8.0f, 1.0f, 16.0f, rgba(255, 255, 255, 160));
          if (c.x1 != c.x0)
          {
            const bool right = camAt(c, float(s.t) + 1.0f) > camAt(c, float(s.t));
            r.drawText(right ? ">>" : "<<", right ? sx + kScrW - 6.0f : sx + 6.0f, sy + kScrH - 15.0f, {10.0f, kUmd, 0},
              right ? Align::Right : Align::Left);
          }
        }
        if ((frame / 30) % 2 == 0)
          r.fillRect(sx + 5.0f, sy + 5.0f, 5.0f, 5.0f, kTally);
      }
    }
    r.draw(baked(mArt, r, "glass", 110, 64, 0.0f, 0.0f, paintGlass), sx, sy);
    // The label on the UMD.
    {
      // (between its two tally lamps: a long name in smaller letters)
      const std::string& umd = m.label.empty() ? m.id : m.label;
      const float size = std::min(11.0f, 72.0f / (0.62f * float(std::max<std::size_t>(1, umd.size()))));
      r.drawText(umd, X + 64.0f, Y + 80.0f + (11.0f - size) * 0.5f, {size, m.virus ? rgb(255, 90, 90) : kUmd, 0}, Align::Center);
    }
    // The tally: red while an enemy is awake where it looks, R-U-N in Morse.
    bool awake = false;
    if (leads && !m.dead)
      for (const auto& e : mEnemies)
        if (e.alive && e.active && std::abs(e.x - ex) < 24 && std::abs(e.y - ey) < 14)
          awake = true;
    const bool lit = awake && tallyOn(frame / 4);
    for (const float lx : {15.0f, 105.0f})
      r.fillRect(X + lx, Y + 82.0f, 8.0f, 9.0f, lit ? kTally : kTallyOff);
    r.fillRect(X + 54.0f, Y - 10.0f, 20.0f, 6.0f, lit ? rgb(255, 80, 70) : rgb(90, 24, 24));
    r.fillRect(X + 57.0f, Y - 13.0f, 14.0f, 3.0f, lit ? rgb(255, 140, 130) : rgb(90, 24, 24));
    if (lit)
    {
      drawGlow(r, mArt, X + 64.0f, Y - 8.0f, 46.0f, kTally, 0.7f);
      drawGlow(r, mArt, X + 19.0f, Y + 86.0f, 18.0f, kTally, 0.5f);
      drawGlow(r, mArt, X + 109.0f, Y + 86.0f, 18.0f, kTally, 0.5f);
    }
    // The screen's own light on the wall round it.
    drawGlow(r, mArt, sx + kScrW * 0.5f, sy + kScrH * 0.5f, 120.0f, m.virus ? rgb(255, 40, 50) : kScreen, 0.12f);
  }

}

// --- In front ------------------------------------------------------------------------------

void World::drawGalleryFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& s = mGallery;
  const auto& p = mPlayer;
  // Cables first: they lie on the floor under everyone's feet.
  for (const auto& a : s.actors)
  {
    const Enemy& e = mEnemies[std::size_t(a.enemy)];
    if (e.kind != EnemyKind::CableWrangler)
      continue;
    const bool loose = a.dropped || !e.alive;
    const float floorY = float(a.cableY + 1) * kCellPx - camY - 5.0f;
    const float cx = (e.drawX + float(e.w) * 0.5f) * kCellPx - camX, foot = (e.drawY + 1.0f) * kCellPx - camY;
    const int dir = e.dir < 0 ? -1 : 1;
    const float tailX = float(dir > 0 ? a.cableX0 : a.cableX1 + 1) * kCellPx - camX;
    const float x0 = float(a.cableX0) * kCellPx - camX, x1 = float(a.cableX1 + 1) * kCellPx - camX;
    if (offScreen(std::min(x0, cx) - 60.0f, floorY - 120.0f, std::abs(x1 - x0) + 200.0f, 140.0f))
      continue;
    // The cable as a run of points.
    std::array<std::array<float, 2>, 24> pts{};
    int n = 0;
    if (loose)
    {
      for (int k = 0; k < 16; ++k)
      {
        const float u = float(k) / 15.0f;
        pts[std::size_t(n++)] = {x0 + (x1 - x0) * u, floorY - std::abs(std::sin(u * 9.0f + float(a.enemy))) * 4.0f};
      }
    }
    else
    {
      const float hx = cx - float(dir) * 30.0f, hy = foot - 56.0f;
      pts[std::size_t(n++)] = {hx, hy};
      pts[std::size_t(n++)] = {hx - float(dir) * 10.0f, hy + 30.0f};
      const float sx0 = hx - float(dir) * 22.0f;
      const float whip = a.whip > 0 ? float(a.whip) / float(kGlWhip) : 0.0f;
      for (int k = 0; k <= 16; ++k)
      {
        const float u = float(k) / 16.0f;
        const float x = sx0 + (tailX - sx0) * u;
        float y = floorY;
        if (whip > 0.0f)
          y -= 60.0f * whip * std::sin(u * float(kPi)) * std::abs(std::sin(u * 7.0f - float(frame) * 0.9f));
        else
          y -= std::abs(std::sin(u * 11.0f + float(e.x) * 0.3f)) * 3.0f;
        pts[std::size_t(n++)] = {x, y};
      }
    }
    for (int pass = 0; pass < 3; ++pass)
      for (int k = 1; k < n; ++k)
      {
        const auto& a0 = pts[std::size_t(k - 1)];
        const auto& a1 = pts[std::size_t(k)];
        if (pass == 0)
          r.drawLine(a0[0], a0[1], a1[0], a1[1], 9.0f, kInk);
        else if (pass == 1)
          r.drawLine(a0[0], a0[1], a1[0], a1[1], 5.0f, loose ? rgb(60, 60, 68) : rgb(36, 36, 42));
        else
          r.drawLine(a0[0], a0[1] - 1.5f, a1[0], a1[1] - 1.5f, 1.5f, rgba(150, 160, 190, 140));
      }
    // The plug at its tail.
    const auto& tl = pts[std::size_t(n - 1)];
    r.fillRect(tl[0] - 8.0f, tl[1] - 6.0f, 16.0f, 12.0f, kSteel);
    r.fillRect(tl[0] - 8.0f, tl[1] - 6.0f, 16.0f, 3.0f, kSteelHi);
    if (!loose && a.whip > 0)
      for (int k = 0; k < 3; ++k)
      {
        const float u = 0.3f + 0.2f * float(k);
        const auto& q = pts[std::size_t(2 + int(u * 16.0f))];
        r.drawLine(q[0] - 10.0f, q[1] - 14.0f, q[0] + 10.0f, q[1] - 20.0f, 2.0f, rgba(255, 255, 255, 150));
      }
  }

  // The crew.
  for (const auto& e : mEnemies)
  {
    if (!e.alive || e.hidden || !crew(e))
      continue;
    const float cx = (e.drawX + float(e.w) * 0.5f) * kCellPx - camX, foot = (e.drawY + 1.0f) * kCellPx - camY;
    const float h = float(e.h) * kCellPx;
    if (offScreen(cx - 400.0f, foot - h - 200.0f, 800.0f, h + 260.0f))
      continue;
    const int dir = e.dir < 0 ? -1 : 1;
    const GalleryActor* a = actorOf(*this, e);
    const Sprite& sp = crewSprite(r, *this, mArt, mTheme, e);
    if (e.kind == EnemyKind::BoomOp && a && a->pass > 0 && a->pass <= kGlBoomPass)
    {
      // The tell: a whoosh, and the mic's shadow sliding over the floor.
      const float u = float(a->pass) / float(kGlBoomPass);
      const float shx = cx - float(dir) * 300.0f * u;
      r.fillRect(shx - 34.0f, foot - 8.0f, 68.0f, 8.0f, rgba(0, 0, 0, int(60 + 120 * u)));
      r.fillRect(shx - 24.0f, foot - 10.0f, 48.0f, 4.0f, rgba(0, 0, 0, int(40 + 80 * u)));
      r.fillRect(cx - 320.0f, foot - 3.0f, 640.0f, 3.0f, rgba(0, 0, 0, int(70 * u)));
      if ((frame / 3) % 2 == 0)
        r.drawText("whoosh", cx - float(dir) * 120.0f, foot - h - 70.0f, {16.0f, rgb(220, 230, 255), kHudInk, true}, Align::Center);
    }
    if (e.kind == EnemyKind::FloorManager && a && a->count > 0 && managerPose(*this, e, a) == 1)
    {
      // Pointing: a sight line from his finger to the runner.
      const float fx = cx + float(dir) * 52.0f, fy = foot - 110.0f;
      const float tx = lerpCells(p.prevX, p.x, alpha) + 1.5f * kCellPx - camX, ty = lerpCells(p.prevY, p.y, alpha) - 1.5f * kCellPx - camY;
      const float len = std::hypot(tx - fx, ty - fy);
      for (float d = 10.0f; d < len - 10.0f; d += 18.0f)
      {
        const float u = d / len;
        r.fillRect(fx + (tx - fx) * u - 2.0f, fy + (ty - fy) * u - 2.0f, 5.0f, 5.0f, rgba(255, 80, 70, 170));
      }
    }
    r.draw(sp.get(dir), cx, foot);
    if (e.flash > 0)
    {
      DrawOpts fo;
      fo.blend = Blend::Add;
      fo.alpha = float(e.flash) / 8.0f;
      r.draw(sp.get(dir), cx, foot, fo);
    }
    if (e.kind == EnemyKind::BoomOp)
    {
      // The pole from his hands to the mic: resting up and back, swung
      // back for the tell, then swept through its arc.
      const int v = a && a->pass > 0 ? (a->pass <= kGlBoomPass ? 1 : 2) : 0;
      const float lean = v == 1 ? -6.0f : v == 2 ? 6.0f : 0.0f, crouch = v == 1 ? 8.0f : 0.0f;
      const float hx = cx + float(dir) * (6.0f + lean), hy = foot - 134.0f + crouch;
      float mx = hx - float(dir) * 70.0f, my = hy - 140.0f;
      std::array<std::array<float, 2>, 4> trail{};
      int nt = 0;
      if (a && a->pass > 0 && a->pass <= kGlBoomPass)
      {
        const float u = std::min(1.0f, (float(a->pass) + alpha) / float(kGlBoomPass));
        const float ex = cx - float(dir) * 300.0f, ey = foot - 200.0f;
        mx = mx + (ex - mx) * u * u * (3.0f - 2.0f * u);
        my = my + (ey - my) * u * u * (3.0f - 2.0f * u);
      }
      else if (a && a->pass > kGlBoomPass)
      {
        auto at = [&](float vv, float& x, float& y) {
          const float ph = std::clamp(vv, 0.0f, 1.0f) * 2.0f - 1.0f;
          x = cx + float(dir) * 300.0f * ph;
          y = foot - 200.0f - 30.0f * (1.0f - ph * ph);
        };
        const float vv = (float(a->pass - kGlBoomPass) - 1.0f + alpha) / float(kGlBoomSweep);
        at(vv, mx, my);
        for (int k = 1; k <= 3; ++k)
        {
          at(vv - float(k) * 0.07f, trail[std::size_t(nt)][0], trail[std::size_t(nt)][1]);
          ++nt;
        }
      }
      const float bx0 = hx - (mx - hx) * 0.12f, by0 = hy - (my - hy) * 0.12f;
      for (int k = 0; k < nt; ++k)
      {
        DrawOpts to;
        to.alpha = 0.35f - 0.1f * float(k);
        r.drawLine(hx, hy, trail[std::size_t(k)][0], trail[std::size_t(k)][1], 3.0f, rgba(200, 210, 230, int(90 - 25 * k)));
        r.draw(baked(mArt, r, "deadcat", 64, 28, 32.0f, 6.0f, paintDeadCat), trail[std::size_t(k)][0], trail[std::size_t(k)][1], to);
      }
      r.drawLine(bx0, by0, mx, my, 7.0f, kInk);
      r.drawLine(bx0, by0, mx, my, 4.0f, rgb(90, 94, 108));
      r.drawLine(hx + (mx - hx) * 0.4f, hy + (my - hy) * 0.4f, mx, my, 2.0f, kSteelHi);
      r.draw(baked(mArt, r, "deadcat", 64, 28, 32.0f, 6.0f, paintDeadCat), mx, my);
      // His cue-light hands back on the pole.
      r.fillRect(hx - 5.0f, hy - 5.0f, 10.0f, 10.0f, rgb(110, 70, 50));
    }
    if (e.kind == EnemyKind::FloorManager && a)
    {
      const int pose = managerPose(*this, e, a);
      if (pose >= 1 && pose <= 4)
      {
        // The count over his head: "!" while pointing, then 3, 2, 1.
        const char* label = pose == 1 ? "!" : pose == 2 ? "3" : pose == 3 ? "2" : "1";
        const float bxp = cx + float(dir) * 22.0f, byp = foot - h - 54.0f;
        const float pop = pose >= 2 ? 1.0f + 0.25f * std::max(0.0f, 1.0f - float(frame % 20) / 6.0f) : 1.0f;
        r.fillRect(bxp - 22.0f * pop, byp - 22.0f * pop, 44.0f * pop, 44.0f * pop, rgb(250, 250, 244));
        r.fillRect(bxp - 22.0f * pop, byp + 22.0f * pop - 3.0f, 44.0f * pop, 3.0f, rgb(200, 200, 196));
        r.drawLine(bxp - float(dir) * 6.0f, byp + 22.0f * pop, bxp - float(dir) * 16.0f, byp + 34.0f, 4.0f, rgb(250, 250, 244));
        r.drawText(label, bxp, byp - 20.0f * pop, {34.0f * pop, pose == 4 ? kTally : kInk, 0}, Align::Center);
      }
      else if (pose == 5)
        r.drawText("YOU'RE ON!", cx, foot - h - 60.0f, {18.0f, rgb(255, 230, 120), kHudInk, true}, Align::Center);
    }
  }

  // Tripped: stars circling over the runner lying on the floor.
  if (s.trip > 0)
  {
    const float x = lerpCells(p.prevX, p.x, alpha) + 1.5f * kCellPx - camX, y = lerpCells(p.prevY, p.y, alpha) - camY;
    for (int k = 0; k < 4; ++k)
    {
      const float a = float(frame) * 0.2f + float(k) * 1.5708f;
      const float sx = x + std::cos(a) * 34.0f, sy = y - 30.0f + std::sin(a) * 9.0f;
      r.drawLine(sx - 7.0f, sy, sx + 7.0f, sy, 3.0f, rgb(255, 230, 90));
      r.drawLine(sx, sy - 7.0f, sx, sy + 7.0f, 3.0f, rgb(255, 230, 90));
    }
    r.drawText("TRIP!", x, y - 110.0f, {20.0f, rgb(255, 230, 120), kHudInk, true}, Align::Center);
  }

  // The Rewind bonus's presses: rods out of the housing, the platen; a
  // creak (shaking, steam, the amber lamp) before the slam, dust after.
  for (const auto& c : s.crushers)
  {
    const float x = float(c.x0) * kTilePx - camX, w = float(c.w) * kTilePx;
    const float top = float(c.rest) * kTilePx - camY, bottom = float(c.face + 1) * kCellPx - camY;
    if (offScreen(x - 40.0f, top - 80.0f, w + 80.0f, bottom - top + 200.0f))
      continue;
    const int up = (c.rest + 1) * kCellsPerTile - 1;
    const int slam = c.period - 10;
    const bool creak = c.t >= slam - c.tell && c.t < slam;
    const float shake = creak ? float((frame / 2) % 2 ? 2 : -2) : 0.0f;
    const float headTop = bottom - 60.0f;
    for (const float rx : {0.25f, 0.75f})
    {
      const float rxp = x + w * rx + shake;
      r.fillRect(rxp - 8.0f, top, 16.0f, std::max(0.0f, headTop - top + 4.0f), rgb(50, 52, 60));
      r.fillRect(rxp - 5.0f, top, 6.0f, std::max(0.0f, headTop - top + 4.0f), kSteelHi);
    }
    r.draw(baked(mArt, r, "presshead" + std::to_string(c.w), int(w), 60, 0.0f, 0.0f, [w](cairo_t* cr) { paintPressHead(cr, double(w)); }),
      x + shake, headTop);
    // The amber lamp on the housing.
    const bool lamp = creak && (frame / 4) % 2 == 0;
    r.fillRect(x + w * 0.5f - 8.0f, top - 18.0f, 16.0f, 10.0f, lamp ? rgb(255, 180, 40) : rgb(90, 60, 20));
    if (lamp)
      drawGlow(r, mArt, x + w * 0.5f, top - 13.0f, 50.0f, rgb(255, 180, 40), 0.7f);
    if (creak)
      for (const float side : {-1.0f, 1.0f})
        drawGlow(r, mArt, x + w * 0.5f + side * (w * 0.5f + 10.0f), top + 10.0f + float(frame % 8), 26.0f, rgb(230, 235, 245), 0.5f);
    if (c.face > up && c.t >= slam && c.t < slam + 8)
      for (int k = 0; k < 6; ++k)
      {
        const std::uint32_t h = hash2(c.x0 * 7 + k, frame / 3);
        const float u = float(c.t - slam) / 8.0f;
        drawGlow(r, mArt, x + w * 0.5f + (unit(h) - 0.5f) * w * (1.0f + u * 2.0f), bottom - 6.0f - unit(h >> 8) * 30.0f * u, 22.0f + 20.0f * u,
          rgb(200, 196, 186), 0.5f * (1.0f - u));
      }
  }
}

// --- The HUD -------------------------------------------------------------------------------

void World::drawGalleryHud(Renderer& r, int frame) const
{
  const auto& s = mGallery;
  const float W = float(kScreenW), H = float(kScreenH);
  if (s.hop > 0)
  {
    // The static wipe: the picture switches off to a line, snow fills the
    // screen with the channel's name up in the corner, then rolls away.
    const int ph = kGlHop - s.hop; // 0..5
    if (ph == 0)
    {
      r.fillRect(0.0f, 0.0f, W, H * 0.4f, rgba(0, 0, 0, 230));
      r.fillRect(0.0f, H * 0.6f, W, H * 0.4f, rgba(0, 0, 0, 230));
      snow(r, mArt, 0.0f, H * 0.4f, W, H * 0.2f, frame, 0.9f);
      r.fillRect(0.0f, H * 0.5f - 3.0f, W, 6.0f, rgb(255, 255, 255), Blend::Add);
    }
    else
    {
      const float a = ph == 5 ? 0.55f : 1.0f;
      snow(r, mArt, 0.0f, 0.0f, W, H, frame * 7 + ph, a);
      const float roll = std::fmod(float(frame) * 37.0f, H + 120.0f) - 60.0f;
      r.fillRect(0.0f, roll, W, 60.0f, rgba(255, 255, 255, 50), Blend::Add);
      r.fillRect(0.0f, roll + 70.0f, W, 8.0f, rgba(0, 0, 0, 90));
      if (s.hopTo >= 0 && std::size_t(s.hopTo) < s.monitors.size() && ph < 5)
      {
        const Monitor& m = s.monitors[std::size_t(s.hopTo)];
        r.drawText(m.label.empty() ? m.id : m.label, W - 60.0f, 90.0f, {46.0f, kUmd, kHudInk}, Align::Right);
      }
    }
  }
  if (s.rewind)
  {
    if (s.rewinding > 0)
    {
      // The VHS rewind: the picture washed blue-grey, tracking bands
      // tearing up through it, << REWIND and the counter running back.
      r.fillRect(0.0f, 0.0f, W, H, rgba(30, 36, 70, 90));
      for (int k = 0; k < 4; ++k)
      {
        const float bh = 14.0f + float(k % 3) * 12.0f;
        const float y = H - std::fmod(float(frame) * (26.0f + float(k) * 7.0f) + float(k) * 211.0f, H + bh);
        snow(r, mArt, 0.0f, y, W, bh, frame + k * 19, 0.75f);
        r.fillRect(0.0f, y - 2.0f, W, 2.0f, rgba(255, 60, 220, 140));
        r.fillRect(0.0f, y + bh, W, 2.0f, rgba(60, 230, 255, 140));
      }
      for (int k = 0; k < 10; ++k)
      {
        const std::uint32_t h = hash2(frame, k + 3939);
        r.fillRect(float(h % 900u), float((h >> 10) % 720u), 80.0f + float((h >> 4) % 300u), 2.0f, rgba(255, 255, 255, 90));
      }
      if ((frame / 8) % 4 != 3)
      {
        r.drawText("<< REWIND", 64.0f, 96.0f, {54.0f, rgb(255, 255, 255), kHudInk}, Align::Left);
        r.drawText("<< REWIND", 67.0f, 96.0f, {54.0f, rgba(255, 60, 220, 70), 0}, Align::Left);
      }
      char buf[32];
      std::snprintf(buf, sizeof(buf), "SP -0:00:%02d", (s.rewinding * 3 + 19) / 20);
      r.drawText(buf, W - 64.0f, H - 96.0f, {30.0f, rgb(255, 255, 255), kHudInk}, Align::Right);
    }
    else
    {
      // Otherwise the tape just plays: PLAY in the corner, the rewinds so far.
      r.drawText("PLAY >", 64.0f, H - 70.0f, {24.0f, rgba(255, 255, 255, 170), kHudInk}, Align::Left);
      if (s.rewinds > 0)
      {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "REW x%d", s.rewinds);
        r.drawText(buf, W - 64.0f, H - 70.0f, {24.0f, rgba(255, 255, 255, 170), kHudInk}, Align::Right);
      }
    }
  }
}

// --- Shots and the icon ----------------------------------------------------------------------

bool drawGalleryShot(Renderer& r, const World& w, const Art& art, const Projectile& pr, float cx, float cy, int frame)
{
  if (!w.gallery().on)
    return false;
  if (pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::EchoMic))
  {
    // Rings of sound going out behind a bright core; an echo is fainter,
    // bluer and dotted.
    const bool echo = pr.echo;
    const Color c = echo ? kEchoBlue : kMicGold;
    const float fade = echo ? 0.55f : 1.0f;
    float ang = 0.0f;
    if (pr.dy != 0 && pr.dx == 0)
      ang = pr.dy > 0 ? 90.0f : -90.0f;
    else if (pr.dx < 0)
      ang = 180.0f;
    const float ux = std::cos(ang * float(kPi) / 180.0f), uy = std::sin(ang * float(kPi) / 180.0f);
    drawGlow(r, art, cx, cy, 40.0f, c, 0.4f * fade);
    const Texture& ring = baked(art, r, echo ? "echoarc_dot" : "echoarc", 24, 56, 0.0f, 28.0f, [echo](cairo_t* cr) { paintEchoArc(cr, echo); });
    for (int k = 0; k < 3; ++k)
    {
      const float ph = float((frame + k * 3) % 9) / 9.0f;
      const float d = 8.0f + (float(k) + ph) * 13.0f;
      DrawOpts o;
      o.angle = ang;
      o.tint = c;
      o.scale = 0.55f + (float(k) + ph) * 0.22f;
      o.alpha = fade * (1.0f - (float(k) + ph) / 3.2f);
      o.blend = Blend::Add;
      r.draw(ring, cx - ux * d, cy - uy * d, o);
    }
    r.fillRect(cx - 6.0f, cy - 6.0f, 12.0f, 12.0f, withAlpha(c, int(200 * fade)));
    r.fillRect(cx - 3.0f, cy - 3.0f, 6.0f, 6.0f, withAlpha(rgb(255, 255, 255), int(230 * fade)));
    return true;
  }
  if (pr.kind == ShotKind::Enemy)
  {
    // On the Floor Manager's cue, the crew let fly with cue cards.
    DrawOpts o;
    o.angle = float(frame * 24 % 360) * (pr.dx < 0 ? -1.0f : 1.0f);
    r.draw(baked(art, r, "cuecard", 44, 30, 22.0f, 15.0f, paintCueCard), cx, cy, o);
    return true;
  }
  return false;
}

void drawEchoMicIcon(Renderer& r, const Art& art, float x, float y, float scale, int frame)
{
  DrawOpts o;
  o.scale = scale;
  const float bob = std::sin(float(frame) * 0.1f) * 1.5f * scale;
  // Rings of sound off the head, and their echo a beat behind, fainter.
  const Texture& ring = baked(art, r, "echoarc", 24, 56, 0.0f, 28.0f, [](cairo_t* cr) { paintEchoArc(cr, false); });
  for (int k = 0; k < 2; ++k)
    for (int e = 0; e < 2; ++e)
    {
      const float ph = std::fmod(float(frame) * 0.03f + float(k) * 0.5f + float(e) * 0.25f, 1.0f);
      DrawOpts ro;
      ro.scale = scale * (0.35f + ph * 0.4f);
      ro.angle = -40.0f;
      ro.tint = e ? kEchoBlue : kMicGold;
      ro.alpha = (1.0f - ph) * (e ? 0.45f : 0.9f);
      ro.blend = Blend::Add;
      r.draw(ring, x + (40.0f + ph * 16.0f) * scale, y + (20.0f - ph * 12.0f) * scale + bob, ro);
    }
  r.draw(baked(art, r, "echomic_icon", 64, 64, 0.0f, 0.0f, paintEchoMic), x, y + bob, o);
}

} // namespace gr
