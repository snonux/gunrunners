// Level 29, Dead Air: drawing. See world_signal.cpp for the logic.
//
// Behind the tiles (drawSignalSet): the live snow over the void (and, on
// frame 4242, its thanks), the Test Card's arena painted on the dark (a
// great test-card circle with its grid, a band of bars and a step wedge),
// and by the exit the seven bars that hang in the void and, once the
// runner tunes in, bend into a sea-floor cable. Behind the runner
// (drawSignalBack): the colour bars (solid and glowing, a dashed 30%
// ghost, flashing before a change), the black bars with their red, green
// and blue hairline, the RGB dials (a pointer on the signal's colour, a
// timed dial's ring filling), the CH 42 sign, the rubber duck's bars and
// Episode 5's treasures. In front (drawSignalFront): the Ghost Images
// (their grey after-images, the tinted halo), the Static Swarms (brighter
// as they hiss, shaking while latched), the Test Card (its plate, the four
// turning quarters, the face and its "please stand by"), its aim line and
// beam, the Tri-Tone's glow in its tint, and the snow over a quarter of the
// screen from a carrier. drawSignalHud: the SIGNAL panel (the swatch and
// the three tint pips) under the score, the Test Card's bar and the flash
// of tuning in.

#include "game/world.hpp"

#include "assets/art.hpp"
#include "assets/enemy_art.hpp"
#include "assets/enemy_art_signal.hpp"
#include "base/math.hpp"
#include "game/signal_draw.hpp"
#include "render/vector.hpp"

#include <SDL.h>

#include <algorithm>
#include <array>
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

Color signalColor(int c);

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr float kTilePx = float(kTileSize) * kPixelScale; // 64
constexpr float kCellPx = float(kCellSize) * kPixelScale; // 32
constexpr int kSnowTile = 512;
constexpr float kBarGlow = 14.0f; // px of glow round a solid bar's texture
constexpr Color kYellow = rgb(245, 214, 40);
constexpr Color kHudLabel = rgb(190, 188, 214);
constexpr Color kHudInk = rgb(10, 8, 20);
// The bars that hang by the exit, left to right.
constexpr std::array<Color, 7> kHangBars{rgb(200, 200, 200), rgb(220, 205, 30), rgb(30, 205, 215), rgb(40, 200, 70),
  rgb(205, 50, 200), rgb(225, 45, 45), rgb(45, 70, 225)};

bool offScreen(float x, float y, float w, float h)
{
  return x > float(kScreenW) + 96.0f || x + w < -96.0f || y > float(kScreenH) + 96.0f || y + h < -96.0f;
}

// A texture baked once and kept in the Art's sprite cache under `key`.
const Texture& baked(const Art& art, const Renderer& r, const std::string& key, int w, int h, float ax, float ay,
  const std::function<void(cairo_t*)>& paint)
{
  const std::string id = "~signal/" + key;
  auto it = art.styled.find(id);
  if (it != art.styled.end())
    return it->second.facing[0];
  VectorImage img(std::max(1, w), std::max(1, h));
  paint(img.cr());
  Sprite s;
  s.facing[0] = img.toTexture(r, ax, ay);
  return art.styled.emplace(id, std::move(s)).first->second.facing[0];
}

Color lighten(Color c, float t) { return lerpColor(c, rgb(255, 255, 255), t); }
Color darken(Color c, float t) { return lerpColor(c, rgb(0, 0, 0), t); }

void setRgba(cairo_t* cr, Color c, double a)
{
  cairo_set_source_rgba(cr, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, a);
}

void circle(cairo_t* cr, double x, double y, double r)
{
  cairo_new_sub_path(cr);
  cairo_arc(cr, x, y, r, 0, 2 * kPi);
}

void vgrad(cairo_t* cr, double y0, double y1, Color a, Color b, bool preserve = false)
{
  cairo_pattern_t* g = cairo_pattern_create_linear(0, y0, 0, y1);
  cairo_pattern_add_color_stop_rgb(g, 0, redOf(a) / 255.0, greenOf(a) / 255.0, blueOf(a) / 255.0);
  cairo_pattern_add_color_stop_rgb(g, 1, redOf(b) / 255.0, greenOf(b) / 255.0, blueOf(b) / 255.0);
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

Color quarterColor(int c) { return c == 3 ? kYellow : signalColor(c); }

float smooth(float t)
{
  t = std::clamp(t, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

// --- Snow ----------------------------------------------------------------------------------------

// A tile of TV snow, 512 px square: sparse white flecks smeared a few px
// along the line, weaker on some lines than others, and a faint grain
// between them; transparent elsewhere. Three of them, turned over fast.
const Texture& snowTile(const Art& art, const Renderer& r, int f)
{
  return baked(art, r, "snow" + std::to_string(f), kSnowTile, kSnowTile, 0, 0, [f](cairo_t* cr) {
    cairo_surface_t* s = cairo_get_target(cr);
    cairo_surface_flush(s);
    unsigned char* px = cairo_image_surface_get_data(s);
    const int stride = cairo_image_surface_get_stride(s);
    std::uint32_t sd = 0x9E3779B9u * std::uint32_t(f + 7);
    auto next = [&sd] {
      sd ^= sd << 13;
      sd ^= sd >> 17;
      sd ^= sd << 5;
      return sd;
    };
    for (int y = 0; y < kSnowTile; ++y)
    {
      const double row = 0.72 + 0.28 * std::sin(double(y + f * 53) * 0.061) - ((hash2(y / 2, f) % 11u) == 0 ? 0.35 : 0.0);
      int run = 0;
      double val = 0.0;
      for (int x = 0; x < kSnowTile; ++x)
      {
        if (run == 0 && (next() & 1023u) < 120u)
        {
          run = 1 + int(next() % 4u);
          val = double(110 + next() % 146u) * row;
        }
        double a = 0.0;
        if (run > 0)
        {
          a = val;
          --run;
        }
        else if ((next() & 255u) < 40u)
          a = double(next() % 30u) * row;
        const auto v = static_cast<unsigned char>(std::clamp(a, 0.0, 255.0));
        unsigned char* p = px + y * stride + x * 4;
        p[0] = v;                                       // B
        p[1] = static_cast<unsigned char>(v * 0.98);   // G
        p[2] = static_cast<unsigned char>(v * 0.95);   // R
        p[3] = v;
      }
    }
    cairo_surface_mark_dirty(s);
  });
}

// Snow over a rect of the screen (clipped to it), a new pattern each frame.
void drawSnow(Renderer& r, const Art& art, float x, float y, float w, float h, int frame, float alpha, int salt)
{
  SDL_Rect prev{};
  const bool hadClip = SDL_RenderIsClipEnabled(r.sdl()) == SDL_TRUE;
  if (hadClip)
    SDL_RenderGetClipRect(r.sdl(), &prev);
  const SDL_Rect clip{int(std::floor(x)), int(std::floor(y)), int(std::ceil(w)), int(std::ceil(h))};
  SDL_RenderSetClipRect(r.sdl(), &clip);
  const Texture& t = snowTile(art, r, (frame + salt) % 3);
  const std::uint32_t hsh = hash2(frame, salt * 977 + 13);
  const float ox = float(hsh % unsigned(kSnowTile)), oy = float((hsh >> 11) % unsigned(kSnowTile));
  DrawOpts o;
  o.alpha = alpha;
  o.cull = false;
  for (float ty = y - oy; ty < y + h; ty += float(kSnowTile))
    for (float tx = x - ox; tx < x + w; tx += float(kSnowTile))
      r.draw(t, tx, ty, o);
  SDL_RenderSetClipRect(r.sdl(), hadClip ? &prev : nullptr);
}

// A strip that fades from opaque to clear left to right, for feathering an
// edge of the snow (turned for the other edges).
const Texture& featherArt(const Art& art, const Renderer& r)
{
  return baked(art, r, "feather", 64, 8, 0, 0, [](cairo_t* cr) {
    cairo_rectangle(cr, 0, 0, 64, 8);
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 0, 64, 0);
    cairo_pattern_add_color_stop_rgba(g, 0, 0.27, 0.27, 0.3, 0.0);
    cairo_pattern_add_color_stop_rgba(g, 1, 0.27, 0.27, 0.3, 0.85);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  });
}

// --- The colour bars ---------------------------------------------------------------------------------

// A bar `bw` x `bh` blocks. Solid: glossy, scanned, a light inner edge and a
// soft glow of its colour round it (the texture is kBarGlow px bigger on
// every side). A black bar is gloss black with a red, green and blue
// hairline along its top. Ghost: a dashed outline at 30% and the faintest
// wash of its colour.
const Texture& barArt(const Art& art, const Renderer& r, int color, int bw, int bh, bool solid)
{
  const int w = bw * int(kTilePx), h = bh * int(kTilePx), m = int(kBarGlow);
  const std::string key = "bar" + std::to_string(color) + (solid ? "s" : "g") + std::to_string(bw) + "x" + std::to_string(bh);
  return baked(art, r, key, w + 2 * m, h + 2 * m, float(m), float(m), [=](cairo_t* cr) {
    const bool black = color == kSignalBlack;
    const Color c = black ? rgb(30, 30, 36) : signalColor(color);
    const double x = m, y = m;
    if (!solid)
    {
      roundedRect(cr, x + 3, y + 3, w - 6, h - 6, 8);
      setRgba(cr, black ? rgb(170, 170, 180) : c, 0.07);
      cairo_fill(cr);
      for (double sy = y + 6; sy < y + h - 6; sy += 4)
      {
        cairo_rectangle(cr, x + 6, sy, w - 12, 1);
        setRgba(cr, black ? rgb(170, 170, 180) : c, 0.05);
        cairo_fill(cr);
      }
      const double dash[2] = {12.0, 7.0};
      cairo_set_dash(cr, dash, 2, 3.0);
      roundedRect(cr, x + 4, y + 4, w - 8, h - 8, 8);
      stroke(cr, black ? rgb(180, 180, 190) : lighten(c, 0.15f), 2.6, 0.34);
      cairo_set_dash(cr, nullptr, 0, 0.0);
      return;
    }
    // The glow round it.
    for (int k = 6; k >= 1; --k)
    {
      roundedRect(cr, x - k * 2.2, y - k * 2.2, w + k * 4.4, h + k * 4.4, 8 + k * 2.2);
      setRgba(cr, black ? rgb(200, 200, 215) : c, black ? 0.02 : 0.055);
      cairo_fill(cr);
    }
    // The body.
    roundedRect(cr, x + 1, y + 1, w - 2, h - 2, 7);
    if (black)
      vgrad(cr, y, y + h, rgb(52, 52, 60), rgb(10, 10, 14), true);
    else
      vgrad(cr, y, y + std::min(h, 128), lighten(c, 0.28f), h > 128 ? c : darken(c, 0.12f), true);
    cairo_save(cr);
    cairo_clip(cr);
    if (h > 128 && !black)
    {
      cairo_rectangle(cr, x, y + 128, w, h - 128);
      vgrad(cr, y + 128, y + h, c, darken(c, 0.2f));
    }
    // Scan lines.
    for (double sy = y + 2; sy < y + h; sy += 4)
    {
      cairo_rectangle(cr, x, sy, w, 1.3);
      setRgba(cr, rgb(0, 0, 0), black ? 0.25 : 0.1);
      cairo_fill(cr);
    }
    // A sheen across the top and a soft shade at the bottom.
    cairo_rectangle(cr, x, y, w, 26);
    {
      cairo_pattern_t* g = cairo_pattern_create_linear(0, y, 0, y + 26);
      cairo_pattern_add_color_stop_rgba(g, 0, 1, 1, 1, black ? 0.18 : 0.3);
      cairo_pattern_add_color_stop_rgba(g, 1, 1, 1, 1, 0.0);
      cairo_set_source(cr, g);
      cairo_fill(cr);
      cairo_pattern_destroy(g);
    }
    cairo_restore(cr);
    if (black)
    {
      // The three hairlines: all three tints to stand on it.
      const std::array<Color, 3> rgbLine{signalColor(0), signalColor(1), signalColor(2)};
      for (int k = 0; k < 3; ++k)
      {
        cairo_rectangle(cr, x + 8, y + 6 + k * 3.2, w - 16, 2);
        setRgba(cr, rgbLine[std::size_t(k)], 0.9);
        cairo_fill(cr);
      }
    }
    roundedRect(cr, x + 4, y + 4, w - 8, h - 8, 5);
    stroke(cr, black ? rgb(150, 150, 165) : lighten(c, 0.55f), 1.4, black ? 0.35 : 0.5);
    cairo_rectangle(cr, x + 8, y + 3, w - 16, 2.2);
    setRgba(cr, rgb(255, 255, 255), black ? 0.45 : 0.75);
    cairo_fill(cr);
    roundedRect(cr, x + 1, y + 1, w - 2, h - 2, 7);
    stroke(cr, black ? rgb(6, 6, 8) : darken(c, 0.5f), 1.6);
  });
}

// --- The RGB dials -------------------------------------------------------------------------------------

// The plate a dial sits on: a graphite square with a bevel and four screws.
const Texture& dialPlate(const Art& art, const Renderer& r)
{
  return baked(art, r, "dial_plate", 64, 64, 0, 0, [](cairo_t* cr) {
    roundedRect(cr, 3, 3, 58, 58, 9);
    vgrad(cr, 3, 61, rgb(78, 80, 92), rgb(34, 35, 42), true);
    stroke(cr, rgb(12, 12, 16), 2.0);
    roundedRect(cr, 5.5, 5.5, 53, 53, 7);
    stroke(cr, rgb(255, 255, 255), 1.0, 0.22);
    for (const double sx : {10.0, 54.0})
      for (const double sy : {10.0, 54.0})
      {
        circle(cr, sx, sy, 2.4);
        vgrad(cr, sy - 2.4, sy + 2.4, rgb(200, 202, 212), rgb(90, 92, 104));
        cairo_move_to(cr, sx - 1.6, sy - 1.6);
        cairo_line_to(cr, sx + 1.6, sy + 1.6);
        stroke(cr, rgb(40, 40, 48), 0.8);
      }
  });
}

// The dial's face: three sectors (red at the top, then green, then blue
// round clockwise), glossy, in a chrome ring. Anchored at its middle.
const Texture& dialFace(const Art& art, const Renderer& r)
{
  return baked(art, r, "dial_face", 64, 64, 32, 32, [](cairo_t* cr) {
    const double cx = 32, cy = 32, R = 21;
    circle(cr, cx, cy, R + 4);
    vgrad(cr, cy - R - 4, cy + R + 4, rgb(236, 238, 246), rgb(120, 122, 136));
    for (int k = 0; k < 3; ++k)
    {
      const double a = -kPi * 0.5 + k * 2 * kPi / 3, half = kPi / 3 - 0.06;
      cairo_move_to(cr, cx, cy);
      cairo_arc(cr, cx, cy, R, a - half, a + half);
      cairo_close_path(cr);
      const Color c = signalColor(k);
      cairo_pattern_t* g = cairo_pattern_create_radial(cx, cy, 4, cx, cy, R);
      cairo_pattern_add_color_stop_rgb(g, 0, redOf(darken(c, 0.3f)) / 255.0, greenOf(darken(c, 0.3f)) / 255.0,
        blueOf(darken(c, 0.3f)) / 255.0);
      cairo_pattern_add_color_stop_rgb(g, 1, redOf(lighten(c, 0.15f)) / 255.0, greenOf(lighten(c, 0.15f)) / 255.0,
        blueOf(lighten(c, 0.15f)) / 255.0);
      cairo_set_source(cr, g);
      cairo_fill(cr);
      cairo_pattern_destroy(g);
    }
    circle(cr, cx, cy, R);
    stroke(cr, rgb(20, 20, 26), 1.6);
    circle(cr, cx, cy, R + 4);
    stroke(cr, rgb(20, 20, 26), 1.6);
  });
}

// The knob and its pointer (pointing up; turned to the colour). Anchored at
// its middle.
const Texture& dialKnob(const Art& art, const Renderer& r)
{
  return baked(art, r, "dial_knob", 64, 64, 32, 32, [](cairo_t* cr) {
    const double cx = 32, cy = 32;
    cairo_move_to(cr, cx, cy - 4);
    cairo_line_to(cr, cx, cy - 19);
    stroke(cr, rgb(16, 16, 20), 5.4);
    cairo_move_to(cr, cx, cy - 4);
    cairo_line_to(cr, cx, cy - 18);
    stroke(cr, rgb(255, 255, 255), 2.6);
    circle(cr, cx, cy, 9);
    cairo_pattern_t* g = cairo_pattern_create_radial(cx - 3, cy - 4, 1, cx, cy, 9);
    cairo_pattern_add_color_stop_rgb(g, 0, 1, 1, 1);
    cairo_pattern_add_color_stop_rgb(g, 0.6, 0.72, 0.73, 0.78);
    cairo_pattern_add_color_stop_rgb(g, 1, 0.36, 0.37, 0.42);
    cairo_set_source(cr, g);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(g);
    stroke(cr, rgb(20, 20, 26), 1.6);
    // Knurls round the knob.
    for (int k = 0; k < 12; ++k)
    {
      const double a = k * kPi / 6;
      cairo_move_to(cr, cx + std::cos(a) * 6.5, cy + std::sin(a) * 6.5);
      cairo_line_to(cr, cx + std::cos(a) * 8.5, cy + std::sin(a) * 8.5);
    }
    stroke(cr, rgb(60, 60, 70), 0.9, 0.6);
  });
}

// The glass over the dial: a curved highlight.
const Texture& dialGlass(const Art& art, const Renderer& r)
{
  return baked(art, r, "dial_glass", 64, 64, 32, 32, [](cairo_t* cr) {
    cairo_move_to(cr, 16, 26);
    cairo_curve_to(cr, 18, 14, 30, 9, 42, 12);
    cairo_curve_to(cr, 32, 13, 22, 18, 16, 26);
    cairo_close_path(cr);
    setRgba(cr, rgb(255, 255, 255), 0.55);
    cairo_fill(cr);
  });
}

// A thin white ring with a soft edge (a dial's flash, a halo, the swatch's
// pips), anchored at its middle.
const Texture& ringArt(const Art& art, const Renderer& r)
{
  return baked(art, r, "ring", 128, 128, 64, 64, [](cairo_t* cr) {
    for (int k = 5; k >= 0; --k)
    {
      circle(cr, 64, 64, 50);
      stroke(cr, rgb(255, 255, 255), 2.0 + k * 3.0, k == 0 ? 0.95 : 0.09);
    }
  });
}

// --- The Test Card ---------------------------------------------------------------------------------------

constexpr int kCardTex = 288; // the Test Card's pieces: 256 px and a margin, anchored at the middle

const Texture& cardBack(const Art& art, const Renderer& r)
{
  return baked(art, r, "card_back", kCardTex, kCardTex, kCardTex / 2.0f, kCardTex / 2.0f,
    [](cairo_t* cr) { paintTestCardBack(cr, kCardTex / 2.0, kCardTex / 2.0, 1.0); });
}

const Texture& cardWedge(const Art& art, const Renderer& r, int c, bool dead)
{
  return baked(art, r, "card_wedge" + std::to_string(c) + (dead ? "d" : ""), kCardTex, kCardTex, kCardTex / 2.0f,
    kCardTex / 2.0f, [=](cairo_t* cr) { paintTestCardWedge(cr, kCardTex / 2.0, kCardTex / 2.0, 1.0, quarterColor(c), dead); });
}

const Texture& cardMask(const Art& art, const Renderer& r)
{
  return baked(art, r, "card_mask", kCardTex, kCardTex, kCardTex / 2.0f, kCardTex / 2.0f,
    [](cairo_t* cr) { paintTestCardWedgeMask(cr, kCardTex / 2.0, kCardTex / 2.0, 1.0); });
}

const Texture& cardFace(const Art& art, const Renderer& r, int mouth)
{
  return baked(art, r, "card_face" + std::to_string(mouth), 96, 96, 48, 48,
    [=](cairo_t* cr) { paintTestCardFace(cr, 48, 48, 1.0, mouth); });
}

// --- Set pieces ------------------------------------------------------------------------------------------

// MaxTV's ident, the treasure: a little round-shouldered set on rabbit ears,
// the logo on its screen (MAX in white on magenta, TV in cyan) and its two
// knobs. 96 px square, anchored so its 64 px block sits at (16, 16).
const Texture& identArt(const Art& art, const Renderer& r)
{
  return baked(art, r, "ident", 96, 96, 16, 16, [](cairo_t* cr) {
    // Rabbit ears.
    for (const auto& e : {std::array<double, 2>{30, 6}, std::array<double, 2>{66, 2}})
    {
      cairo_move_to(cr, 48, 28);
      cairo_line_to(cr, e[0], e[1]);
      stroke(cr, rgb(30, 30, 40), 3.2);
      cairo_move_to(cr, 48, 28);
      cairo_line_to(cr, e[0], e[1]);
      stroke(cr, rgb(210, 214, 226), 1.4);
      circle(cr, e[0], e[1], 3);
      setRgba(cr, rgb(255, 80, 200), 1.0);
      cairo_fill(cr);
    }
    // The cabinet.
    roundedRect(cr, 18, 26, 62, 50, 12);
    vgrad(cr, 26, 76, rgb(120, 60, 150), rgb(52, 22, 72), true);
    stroke(cr, rgb(20, 10, 30), 2.6);
    roundedRect(cr, 20.5, 28.5, 57, 6, 3);
    setRgba(cr, rgb(255, 160, 240), 0.35);
    cairo_fill(cr);
    // The screen and the logo on it.
    roundedRect(cr, 24, 33, 40, 34, 8);
    setRgba(cr, rgb(16, 10, 28), 1.0);
    cairo_fill(cr);
    roundedRect(cr, 27, 38, 34, 15, 3);
    setRgba(cr, rgb(255, 60, 200), 1.0);
    cairo_fill(cr);
    lettering(cr, "MAX", 44, 50, 12.5, rgb(255, 255, 255));
    lettering(cr, "TV", 44, 64, 12, rgb(0, 236, 255));
    for (double y = 35; y < 66; y += 3)
    {
      cairo_rectangle(cr, 25, y, 38, 1);
      setRgba(cr, rgb(0, 0, 0), 0.25);
      cairo_fill(cr);
    }
    roundedRect(cr, 24, 33, 40, 34, 8);
    stroke(cr, rgb(10, 6, 16), 2.0);
    cairo_move_to(cr, 28, 40);
    cairo_curve_to(cr, 30, 36, 36, 35, 42, 35);
    stroke(cr, rgb(255, 255, 255), 1.6, 0.5);
    // The knobs.
    for (const double ky : {42.0, 56.0})
    {
      circle(cr, 71, ky, 4.2);
      vgrad(cr, ky - 4, ky + 4, rgb(250, 230, 120), rgb(170, 120, 30), true);
      stroke(cr, rgb(20, 10, 30), 1.4);
    }
    // Feet.
    for (const double fx : {28.0, 66.0})
    {
      cairo_move_to(cr, fx, 75);
      cairo_line_to(cr, fx + (fx < 48 ? -3 : 3), 80);
      stroke(cr, rgb(20, 10, 30), 3.4);
    }
  });
}

// A lost tape, the generic treasure for a name with "tape" in it: a VHS
// cassette with a MaxTV label and its two reels.
const Texture& tapeArt(const Art& art, const Renderer& r)
{
  return baked(art, r, "tape", 96, 96, 16, 16, [](cairo_t* cr) {
    roundedRect(cr, 12, 30, 72, 42, 5);
    vgrad(cr, 30, 72, rgb(52, 52, 60), rgb(16, 16, 20), true);
    stroke(cr, rgb(6, 6, 8), 2.2);
    roundedRect(cr, 20, 34, 56, 14, 2);
    setRgba(cr, rgb(250, 244, 226), 1.0);
    cairo_fill(cr);
    lettering(cr, "MAXTV", 48, 45, 10, rgb(220, 40, 120));
    roundedRect(cr, 30, 52, 36, 14, 3);
    setRgba(cr, rgb(40, 40, 48), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, rgb(120, 120, 130), 1.0, 0.6);
    for (const double rx : {38.0, 58.0})
    {
      circle(cr, rx, 59, 5);
      setRgba(cr, rgb(230, 230, 236), 1.0);
      cairo_fill(cr);
      circle(cr, rx, 59, 2);
      setRgba(cr, rgb(30, 30, 36), 1.0);
      cairo_fill(cr);
    }
    cairo_move_to(cr, 16, 33);
    cairo_line_to(cr, 80, 33);
    stroke(cr, rgb(255, 255, 255), 1.2, 0.3);
  });
}

// Any other treasure: a crate banded in gold with a star stencilled on it.
const Texture& crateArt(const Art& art, const Renderer& r)
{
  return baked(art, r, "crate", 96, 96, 16, 16, [](cairo_t* cr) {
    roundedRect(cr, 18, 24, 60, 52, 5);
    vgrad(cr, 24, 76, rgb(120, 92, 60), rgb(70, 50, 30), true);
    stroke(cr, rgb(30, 20, 10), 2.4);
    for (const double by : {30.0, 64.0})
    {
      cairo_rectangle(cr, 18, by, 60, 6);
      vgrad(cr, by, by + 6, rgb(255, 230, 120), rgb(190, 140, 30));
    }
    cairo_move_to(cr, 48, 38);
    for (int k = 1; k <= 10; ++k)
    {
      const double a = -kPi * 0.5 + k * kPi / 5, rr = k % 2 ? 5.0 : 12.0;
      cairo_line_to(cr, 48 + std::cos(a) * rr, 50 + std::sin(a) * rr);
    }
    cairo_close_path(cr);
    setRgba(cr, rgb(255, 220, 90), 1.0);
    cairo_fill(cr);
  });
}

// The CH 42 sign: a channel number on screen, the green of an old set's
// on-screen display on a smoked panel, with its tuning bar.
const Texture& ch42Art(const Art& art, const Renderer& r, const std::string& text, int w, int h)
{
  return baked(art, r, "ch42/" + text + "/" + std::to_string(w) + "x" + std::to_string(h), w + 32, h + 32, 16, 16,
    [=](cairo_t* cr) {
      const Color osd = rgb(80, 255, 120);
      roundedRect(cr, 16, 16, w, h, 8);
      setRgba(cr, rgb(6, 10, 8), 0.72);
      cairo_fill_preserve(cr);
      stroke(cr, osd, 1.4, 0.35);
      // The glow, then the figures.
      for (int k = 4; k >= 1; --k)
      {
        selectGameFont(cr);
        cairo_set_font_size(cr, h * 0.5);
        cairo_text_extents_t e;
        cairo_text_extents(cr, text.c_str(), &e);
        cairo_move_to(cr, 16 + w * 0.5 - e.width * 0.5 - e.x_bearing, 16 + h * 0.62);
        cairo_text_path(cr, text.c_str());
        stroke(cr, osd, k * 3.0, 0.08);
      }
      lettering(cr, text.c_str(), 16 + w * 0.5, 16 + h * 0.62, h * 0.5, lighten(osd, 0.35f));
      // The tuning bar under it.
      for (int k = 0; k < 10; ++k)
      {
        cairo_rectangle(cr, 16 + w * 0.18 + k * w * 0.064, 16 + h * 0.76, w * 0.05, h * 0.08);
        setRgba(cr, osd, k < 7 ? 0.85 : 0.25);
        cairo_fill(cr);
      }
      for (double y = 18; y < 16 + h; y += 3)
      {
        cairo_rectangle(cr, 18, y, w - 4, 1);
        setRgba(cr, rgb(0, 0, 0), 0.22);
        cairo_fill(cr);
      }
    });
}

// The SIGNAL panel's swatch: a little screen in a bezel, the glass over it
// (its colour is filled in under it each frame). 92 x 34.
const Texture& swatchGlass(const Art& art, const Renderer& r)
{
  return baked(art, r, "swatch", 92, 34, 0, 0, [](cairo_t* cr) {
    // The bezel round a hole the colour shows through.
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
    roundedRect(cr, 0, 0, 92, 34, 8);
    roundedRect(cr, 4, 4, 84, 26, 6);
    vgrad(cr, 0, 34, rgb(70, 72, 84), rgb(24, 24, 30));
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
    for (double y = 5; y < 30; y += 3)
    {
      cairo_rectangle(cr, 4, y, 84, 1);
      setRgba(cr, rgb(0, 0, 0), 0.22);
      cairo_fill(cr);
    }
    cairo_move_to(cr, 8, 15);
    cairo_curve_to(cr, 10, 8, 20, 6, 44, 6);
    cairo_line_to(cr, 84, 6);
    cairo_line_to(cr, 84, 10);
    cairo_curve_to(cr, 50, 10, 20, 10, 8, 15);
    cairo_close_path(cr);
    setRgba(cr, rgb(255, 255, 255), 0.3);
    cairo_fill(cr);
    roundedRect(cr, 4, 4, 84, 26, 6);
    stroke(cr, rgb(8, 8, 12), 1.6);
    roundedRect(cr, 0.8, 0.8, 90.4, 32.4, 8);
    stroke(cr, rgb(255, 255, 255), 1.0, 0.25);
  });
}

// A pip: a round lamp in its colour (lit or dark), 32 px, anchored at its
// middle.
const Texture& pipArt(const Art& art, const Renderer& r, int c, bool lit)
{
  return baked(art, r, "pip" + std::to_string(c) + (lit ? "l" : "d"), 32, 32, 16, 16, [=](cairo_t* cr) {
    const Color col = signalColor(c);
    circle(cr, 16, 16, 9.5);
    setRgba(cr, rgb(8, 8, 12), 1.0);
    cairo_fill(cr);
    circle(cr, 16, 16, 8);
    cairo_pattern_t* g = cairo_pattern_create_radial(13, 12, 1, 16, 16, 8);
    const Color a = lit ? lighten(col, 0.6f) : darken(col, 0.45f), b = lit ? col : darken(col, 0.7f);
    cairo_pattern_add_color_stop_rgb(g, 0, redOf(a) / 255.0, greenOf(a) / 255.0, blueOf(a) / 255.0);
    cairo_pattern_add_color_stop_rgb(g, 1, redOf(b) / 255.0, greenOf(b) / 255.0, blueOf(b) / 255.0);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
    circle(cr, 13.5, 12.5, 2.4);
    setRgba(cr, rgb(255, 255, 255), lit ? 0.85 : 0.25);
    cairo_fill(cr);
  });
}

// A Tri-Tone shot: a white bolt with a soft head (tinted when drawn),
// pointing right, anchored at its head.
const Texture& boltArt(const Art& art, const Renderer& r)
{
  return baked(art, r, "bolt", 80, 32, 60, 16, [](cairo_t* cr) {
    for (int k = 4; k >= 0; --k)
    {
      cairo_move_to(cr, 8, 16);
      cairo_line_to(cr, 60, 16);
      stroke(cr, rgb(255, 255, 255), 4.0 + k * 4.0, k == 0 ? 1.0 : 0.12);
    }
    cairo_pattern_t* g = cairo_pattern_create_radial(60, 16, 0, 60, 16, 14);
    cairo_pattern_add_color_stop_rgba(g, 0, 1, 1, 1, 1);
    cairo_pattern_add_color_stop_rgba(g, 1, 1, 1, 1, 0);
    circle(cr, 60, 16, 14);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  });
}

// A line of `n` segments round an arc (a ring on the backdrop, a timer).
void arc(Renderer& r, float cx, float cy, float rad, float a0, float a1, int n, float width, Color c, Blend b = Blend::Alpha)
{
  float px = cx + std::cos(a0) * rad, py = cy + std::sin(a0) * rad;
  for (int i = 1; i <= n; ++i)
  {
    const float a = a0 + (a1 - a0) * float(i) / float(n);
    const float x = cx + std::cos(a) * rad, y = cy + std::sin(a) * rad;
    if (!offScreen(std::min(px, x), std::min(py, y), std::abs(x - px), std::abs(y - py)))
      r.drawLine(px, py, x, y, width, c, b);
    px = x;
    py = y;
  }
}

// The Test Card's arena on the backdrop: its grid, a great circle round the
// card, a band of bars across the top of it, a step wedge under the card
// and the crosshair. Fainter once the card is down.
void drawArena(Renderer& r, const SignalState& s, const Enemy& card, float camX, float camY, int frame)
{
  const CellBox b = card.box();
  const float cx = (float(b.x) + float(b.w) * 0.5f) * kCellPx - camX, cy = (float(b.y) + float(b.h) * 0.5f) * kCellPx - camY;
  const float R = 15.0f * kTilePx;
  if (cx + R < -64.0f || cx - R > float(kScreenW) + 64.0f || cy + R < -64.0f || cy - R > float(kScreenH) + 64.0f)
    return;
  const float fade = s.cardDead ? 0.55f : 1.0f;
  const auto a = [&](float v) { return int(std::clamp(v * fade, 0.0f, 1.0f) * 255.0f); };
  // The grid over the arena, two blocks a square.
  const float ax0 = float(s.arena.x) * kCellPx - camX, ay0 = float(s.arena.y) * kCellPx - camY;
  const float ax1 = ax0 + float(s.arena.w) * kCellPx, ay1 = ay0 + float(s.arena.h) * kCellPx;
  for (float x = ax0; x <= ax1 + 1.0f; x += 2.0f * kTilePx)
    if (x > -4.0f && x < float(kScreenW) + 4.0f)
      r.fillRect(x - 1.0f, std::max(ay0, -4.0f), 2.0f, std::min(ay1, float(kScreenH) + 4.0f) - std::max(ay0, -4.0f),
        rgba(220, 220, 235, a(0.05f)));
  for (float y = ay0; y <= ay1 + 1.0f; y += 2.0f * kTilePx)
    if (y > -4.0f && y < float(kScreenH) + 4.0f)
      r.fillRect(std::max(ax0, -4.0f), y - 1.0f, std::min(ax1, float(kScreenW) + 4.0f) - std::max(ax0, -4.0f), 2.0f,
        rgba(220, 220, 235, a(0.05f)));
  // A band of the seven bars across the circle above the card.
  const float bandY0 = cy - 7.0f * kTilePx, bandY1 = cy - 4.5f * kTilePx;
  const float half = std::sqrt(std::max(0.0f, R * R - (cy - bandY0) * (cy - bandY0))) - 24.0f;
  for (int i = 0; i < 7; ++i)
  {
    const float x0 = cx - half + 2.0f * half * float(i) / 7.0f;
    r.fillRect(x0, bandY0, 2.0f * half / 7.0f + 0.5f, bandY1 - bandY0, withAlpha(kHangBars[std::size_t(i)], a(0.1f)));
  }
  // A step wedge under the card.
  const float wedgeY = cy + 2.6f * kTilePx, wedgeW = 10.0f * kTilePx;
  for (int i = 0; i < 6; ++i)
  {
    const int v = 250 - i * 42;
    r.fillRect(cx - wedgeW * 0.5f + wedgeW * float(i) / 6.0f, wedgeY, wedgeW / 6.0f + 0.5f, 0.9f * kTilePx,
      rgba(v, v, v + 4, a(0.09f)));
  }
  // The crosshair, the inner ring and the great circle, breathing slowly.
  const float breathe = 0.85f + 0.15f * std::sin(float(frame) * 0.04f);
  r.fillRect(cx - R, cy - 1.0f, 2.0f * R, 2.0f, rgba(230, 230, 245, a(0.08f)));
  r.fillRect(cx - 1.0f, cy - R, 2.0f, 2.0f * R, rgba(230, 230, 245, a(0.08f)));
  arc(r, cx, cy, 6.0f * kTilePx, 0.0f, float(2 * kPi), 64, 2.0f, rgba(230, 230, 245, a(0.1f)));
  arc(r, cx, cy, R - 22.0f, 0.0f, float(2 * kPi), 160, 2.0f, rgba(230, 230, 245, a(0.1f)));
  arc(r, cx, cy, R, 0.0f, float(2 * kPi), 160, 14.0f, rgba(230, 230, 245, a(0.05f * breathe)));
  arc(r, cx, cy, R, 0.0f, float(2 * kPi), 160, 5.0f, rgba(240, 240, 255, a(0.26f * breathe)));
}

// The bars that hang in the void by the exit; tuned in (bend 0 to 1) they
// bend into a sea-floor cable running off to the right, light running
// along it.
void drawCable(Renderer& r, const Art& art, float baseX, float bend, bool tuned, int frame)
{
  constexpr float bw = 34.0f, r0 = 70.0f;
  const float yb = 780.0f - 530.0f * smooth(bend); // where the bars turn
  const float bright = tuned ? 0.42f : 0.22f;
  const float glitch = !tuned && (hash2(frame / 6, 29) % 23u) == 0 ? 10.0f : 0.0f;
  // Each bar's centre line: down, a quarter turn, then off to the right,
  // the bundle drawing in tight and sinking a little as it goes.
  const float joinX = baseX + r0 + 6.5f * bw;
  const float midY = yb + r0 + 3.0f * bw;
  auto point = [&](int i, float u) {
    // u in px along the bar after the turn's start (negative: above it).
    const float x = baseX + (float(i) + 0.5f) * bw, R = r0 + float(6 - i) * bw;
    if (u <= 0.0f)
      return std::array<float, 2>{x + (i % 2 ? glitch : 0.0f), yb + u};
    const float turn = R * float(kPi) * 0.5f;
    if (u <= turn)
    {
      const float phi = u / R;
      return std::array<float, 2>{x + R - R * std::cos(phi), yb + R * std::sin(phi)};
    }
    const float d = u - turn, squeeze = 1.0f - 0.72f * smooth(d / 320.0f);
    const float y = yb + R;
    return std::array<float, 2>{joinX + d, midY + (y - midY) * squeeze + d * 0.07f};
  };
  // Nothing turns before the tune: straight bars down the screen.
  const float len = bend > 0.0f ? 1600.0f : 0.0f;
  if (bend > 0.0f)
  {
    // The cable's dark sheath round the bundle.
    float px = joinX, py = midY;
    for (float d = 20.0f; d <= len; d += 20.0f)
    {
      const float squeeze = 1.0f - 0.72f * smooth(d / 320.0f);
      const float x = joinX + d, y = midY + d * 0.07f;
      if (x > -100.0f && px < float(kScreenW) + 100.0f)
        r.drawLine(px, py, x, y, 7.0f * bw * squeeze + 10.0f, rgb(34, 34, 42));
      px = x;
      py = y;
    }
  }
  for (int i = 0; i < 7; ++i)
  {
    const Color c = lerpColor(rgb(0, 0, 0), kHangBars[std::size_t(i)], bright);
    const auto top = point(i, -yb - 20.0f), knee = point(i, 0.0f);
    r.drawLine(top[0], top[1], knee[0], knee[1], bw, c);
    if (bend <= 0.0f)
      continue;
    auto prev = knee;
    const float R = r0 + float(6 - i) * bw, turn = R * float(kPi) * 0.5f;
    for (float u = 6.0f; u <= turn + len; u += u < turn ? 6.0f : 20.0f)
    {
      const auto p = point(i, u);
      const float d = std::max(0.0f, u - turn);
      const float width = bw * (1.0f - 0.72f * smooth(d / 320.0f)) + 1.0f;
      if (p[0] > -100.0f && prev[0] < float(kScreenW) + 100.0f)
        r.drawLine(prev[0], prev[1], p[0], p[1], width, c);
      prev = p;
    }
  }
  // A scan line's worth of light running down each bar, and pulses along
  // the cable once it carries the signal.
  for (int i = 0; i < 7; ++i)
  {
    const float y = std::fmod(float(frame) * 5.0f + float(i) * 97.0f, yb + 60.0f) - 30.0f;
    const auto p = point(i, y - yb);
    drawGlow(r, art, p[0], p[1], 26.0f, kHangBars[std::size_t(i)], 0.25f * bright);
  }
  if (tuned && bend >= 1.0f)
    for (int k = 0; k < 4; ++k)
    {
      const float d = std::fmod(float(frame) * 9.0f + float(k) * 400.0f, len);
      const float x = joinX + d, y = midY + d * 0.07f;
      drawGlow(r, art, x, y, 60.0f, rgb(200, 240, 255), 0.5f);
      drawGlow(r, art, x, y, 16.0f, rgb(255, 255, 255), 0.8f);
    }
}

} // namespace

// --- The backdrop ---------------------------------------------------------------------------------------

void drawSignalSet(Renderer& r, const World& w, const Art& art, float camX, float camY, int frame)
{
  const auto& s = w.signal();
  if (s.card >= 0 && std::size_t(s.card) < w.enemies().size())
    drawArena(r, s, w.enemies()[std::size_t(s.card)], camX, camY, frame);
  if (s.tuneX >= 0)
  {
    // The hanging bars, half way back between the backdrop and the tiles.
    const float anchor = float(s.tuneX + 3) * kTilePx;
    const float x = (anchor - camX - 640.0f) * 0.5f + 640.0f - 120.0f;
    if (x > -1700.0f && x < float(kScreenW) + 300.0f)
    {
      const float bend = s.tuned ? 1.0f - float(s.shake) / float(kTuneShake) : 0.0f;
      drawCable(r, art, x, bend, s.tuned, frame);
    }
  }
  // The live snow, and now and then a brighter band rolling down.
  drawSnow(r, art, 0.0f, 0.0f, float(kScreenW), float(kScreenH), frame, 0.17f, 0);
  const float roll = std::fmod(float(frame) * 2.2f, float(kScreenH) + 900.0f) - 120.0f;
  if (roll < float(kScreenH))
    for (int k = 0; k < 6; ++k)
      r.fillRect(0.0f, roll + float(k) * 8.0f, float(kScreenW), 8.0f, rgba(220, 220, 235, 4 + (k < 3 ? k : 5 - k) * 3),
        Blend::Add);
  // Frame 4242: for exactly one frame the static thanks RigelEngine.
  if (s.egg)
  {
    drawSnow(r, art, 0.0f, float(kScreenH) * 0.3f, float(kScreenW), 120.0f, frame + 1, 0.6f, 5);
    r.drawText("THANK YOU, RIGELENGINE", float(kScreenW) * 0.5f, float(kScreenH) * 0.33f,
      {54.0f, rgb(236, 236, 242), rgb(20, 20, 24), true}, Align::Center);
  }
}

bool drawSignalShot(Renderer& r, const Art& art, const Projectile& pr, float cx, float cy, int frame)
{
  if (pr.kind != ShotKind::Proto || pr.proto != int(ProtoId::TriToneBlaster) || pr.tint < 0)
    return false;
  const Color c = signalColor(pr.tint);
  DrawOpts o;
  o.tint = lighten(c, 0.25f);
  o.blend = Blend::Add;
  o.angle = pr.dx < 0 ? 180.0f : (pr.dy < 0 ? -90.0f : (pr.dy > 0 ? 90.0f : 0.0f));
  o.scale = 1.0f + 0.06f * std::sin(float(frame) * 1.3f);
  drawGlow(r, art, cx, cy, 44.0f, c, 0.55f);
  r.draw(boltArt(art, r), cx, cy, o);
  o.tint = rgb(255, 255, 255);
  o.alpha = 0.7f;
  o.scale *= 0.55f;
  r.draw(boltArt(art, r), cx, cy, o);
  return true;
}

// --- The level -----------------------------------------------------------------------------------------

void World::drawSignalBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)alpha;
  const auto& s = mSignal;
  if (s.air)
  {
    const int next = s.pending >= 0 ? s.pending : (s.color + 1) % 3;
    bool warn = false;
    for (const auto& d : s.dials)
      warn = warn || (d.timed && d.rect.intersects(mPlayer.box()) && !s.cardDead &&
                       d.period - (d.t + d.phase) % d.period <= d.warn);
    const bool blinkOn = (frame / 3) % 2 == 0;
    // The colour bars.
    for (const auto& b : s.bars)
    {
      const auto& l = mLayers[std::size_t(b.layer)];
      const float x = float(l.x0) * kTilePx - camX, y = float(l.y0) * kTilePx - camY;
      const int bw = l.x1 - l.x0 + 1, bh = l.y1 - l.y0 + 1;
      if (offScreen(x, y, float(bw) * kTilePx, float(bh) * kTilePx))
        continue;
      // The incoming colour flashes (before a change, and before a timed step).
      const bool flashing = b.color == next && b.color != kSignalBlack && ((s.pending >= 0 && s.flash > 0) || warn);
      if (l.solid)
      {
        r.draw(barArt(mArt, r, b.color, bw, bh, true), x, y);
        continue;
      }
      r.draw(barArt(mArt, r, b.color, bw, bh, false), x, y);
      if (flashing && blinkOn)
      {
        DrawOpts o;
        o.alpha = 0.55f;
        r.draw(barArt(mArt, r, b.color, bw, bh, true), x, y, o);
      }
    }
    // The dials: the pointer on the signal's colour, a flash when hit, a
    // timed dial's ring filling round it.
    for (const auto& d : s.dials)
    {
      const float x = float(d.x) * kTilePx - camX, y = float(d.y) * kTilePx - camY;
      if (offScreen(x, y, kTilePx, kTilePx))
        continue;
      const float cx = x + 32.0f, cy = y + 32.0f;
      r.draw(dialPlate(mArt, r), x, y);
      drawGlow(r, mArt, cx, cy, 40.0f, signalColor(signalShown()), 0.35f);
      r.draw(dialFace(mArt, r), cx, cy);
      if ((s.pending >= 0 && s.flash > 0) || (d.timed && warn))
        if (blinkOn)
        {
          // The incoming sector lights up.
          const float a = -float(kPi) * 0.5f + float(next) * 2.0f * float(kPi) / 3.0f;
          drawGlow(r, mArt, cx + std::cos(a) * 12.0f, cy + std::sin(a) * 12.0f, 20.0f, rgb(255, 255, 255), 0.6f);
        }
      DrawOpts ko;
      ko.angle = float(signalShown()) * 120.0f;
      r.draw(dialKnob(mArt, r), cx, cy, ko);
      r.draw(dialGlass(mArt, r), cx, cy);
      if (d.timed && !s.cardDead)
      {
        const float f = float((d.t + d.phase) % d.period) / float(d.period);
        arc(r, cx, cy, 29.0f, -float(kPi) * 0.5f, -float(kPi) * 0.5f + 2.0f * float(kPi), 36, 3.0f, rgba(0, 0, 0, 150));
        arc(r, cx, cy, 29.0f, -float(kPi) * 0.5f, -float(kPi) * 0.5f + 2.0f * float(kPi) * f, 36, 3.0f,
          warn && blinkOn ? signalColor(next) : rgb(235, 235, 245));
      }
      else if (!d.timed && d.lock == 0 && (frame / 50) % 3 == 0)
      {
        // A shoot dial breathes now and then: shoot me.
        DrawOpts o;
        o.tint = signalColor(signalShown());
        o.blend = Blend::Add;
        o.scale = 0.5f + 0.12f * float(frame % 50) / 50.0f;
        o.alpha = 0.5f * (1.0f - float(frame % 50) / 50.0f);
        r.draw(ringArt(mArt, r), cx, cy, o);
      }
      if (d.flash > 0)
      {
        DrawOpts o;
        o.blend = Blend::Add;
        o.scale = 0.55f + 0.08f * float(6 - d.flash);
        o.alpha = float(d.flash) / 6.0f;
        r.draw(ringArt(mArt, r), cx, cy, o);
        drawGlow(r, mArt, cx, cy, 48.0f, rgb(255, 255, 255), 0.5f * float(d.flash) / 6.0f);
      }
    }
    // The CH 42 sign.
    for (const auto& pr : mProps)
    {
      if (pr.kind != PropKind::Deco42)
        continue;
      const float x = float(pr.x) * kCellPx - camX, y = float(pr.y) * kCellPx - camY;
      const int w = pr.w * int(kCellPx), h = pr.h * int(kCellPx);
      if (offScreen(x, y, float(w), float(h)))
        continue;
      DrawOpts o;
      o.alpha = (hash2(frame / 4, pr.x) % 17u) == 0 ? 0.6f : 1.0f; // a flicker now and then
      r.draw(ch42Art(mArt, r, pr.text, w, h), x, y, o);
    }
  }
  // Treasures.
  for (const auto& t : s.treasures)
  {
    if (t.taken)
      continue;
    if (mTrench.on && t.art == "maxtv_tape_waterproof")
      continue; // Level 30 draws its own (world_trench_draw.cpp)
    const float x = float(t.x) * kTilePx - camX, y = float(t.y) * kTilePx - camY;
    if (offScreen(x, y, kTilePx, kTilePx))
      continue;
    const float bob = std::sin(float(frame) * 0.08f) * 4.0f;
    drawGlow(r, mArt, x + 32.0f, y + 32.0f + bob, 70.0f + 6.0f * std::sin(float(frame) * 0.1f), rgb(255, 210, 90), 0.5f);
    const bool ident = t.art == "maxtv_ident", tape = t.art.find("tape") != std::string::npos;
    r.draw(ident ? identArt(mArt, r) : tape ? tapeArt(mArt, r) : crateArt(mArt, r), x, y + bob);
    if (ident)
    {
      // A bright line rolling down the little screen.
      const float sy = y + bob + 17.0f + float((frame / 2) % 34);
      r.fillRect(x + 9.0f, sy, 38.0f, 2.0f, rgba(255, 255, 255, 60), Blend::Add);
    }
    // A sparkle now and then.
    const int ph = frame % 90;
    if (ph < 16)
    {
      const float sx = x + 10.0f + float(hash2(frame / 90, t.x) % 44u), sy = y + bob + 4.0f + float(hash2(t.y, frame / 90) % 30u);
      const float k = std::sin(float(ph) / 16.0f * float(kPi));
      r.drawLine(sx - 9.0f * k, sy, sx + 9.0f * k, sy, 2.0f, rgb(255, 250, 220), Blend::Add);
      r.drawLine(sx, sy - 9.0f * k, sx, sy + 9.0f * k, 2.0f, rgb(255, 250, 220), Blend::Add);
    }
  }
}

void World::drawSignalFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& s = mSignal;
  if (!s.air)
    return;
  for (const auto& e : mEnemies)
  {
    if (!e.alive)
      continue;
    const float ex = (float(e.prevX) + float(e.x - e.prevX) * alpha) * kCellPx - camX;
    const float ey = (float(e.prevY) + float(e.y - e.prevY) * alpha + 1.0f) * kCellPx - camY;
    const float w = float(e.w) * kCellPx, h = float(e.h) * kCellPx;
    if (offScreen(ex, ey - h, w, h))
      continue;
    const float mx = ex + w * 0.5f; // the box's bottom middle, where the sprites anchor
    if (e.kind == EnemyKind::TestCard)
    {
      const float cx = mx, cy = ey - h * 0.5f + std::sin(float(frame) * 0.05f) * 3.0f;
      drawGlow(r, mArt, cx, cy, 190.0f, rgb(200, 205, 230), 0.16f);
      r.draw(cardBack(mArt, r), cx, cy);
      // The quarters, turning a quarter every second (eased in over the
      // first frames after a turn).
      const float since = cardFight() ? (float(e.timer % kCardTurn) + alpha) : 99.0f;
      const float lag = 1.0f - smooth(since / 10.0f);
      for (int c = 0; c < 4; ++c)
      {
        const bool dead = c < 3 && s.cardHp[c] == 0;
        const int pos = (c + s.cardTurn) % 4;
        DrawOpts o;
        o.angle = (float(pos) - lag) * 90.0f;
        r.draw(cardWedge(mArt, r, c, dead), cx, cy, o);
        // The quarter in signal pulses: that's the one to hit.
        DrawOpts m = o;
        m.blend = Blend::Add;
        if (!dead && c == s.color && !s.cardDead)
        {
          m.alpha = 0.18f + 0.14f * std::sin(float(frame) * 0.25f);
          m.tint = lighten(quarterColor(c), 0.5f);
          r.draw(cardMask(mArt, r), cx, cy, m);
        }
        // The quarter about to fire flashes; a hit flashes white.
        const bool aiming = s.beam.on && !s.beam.burning && s.beam.quadrant == c && (frame / 2) % 2 == 0;
        if (aiming || (e.flash > 0 && c == s.color))
        {
          m.alpha = aiming ? 0.6f : 0.8f;
          m.tint = rgb(255, 255, 255);
          r.draw(cardMask(mArt, r), cx, cy, m);
        }
      }
      // The face: "please stand by" after a hit (its mouth going), else
      // straight-faced.
      const int mouth = s.cardStandBy > 0 ? (s.cardStandBy % 6 < 3 ? 1 : 0) : 0;
      r.draw(cardFace(mArt, r, mouth), cx, cy);
      if (e.flash > 0)
        drawGlow(r, mArt, cx, cy, 60.0f, rgb(255, 255, 255), 0.5f);
      if (s.cardStandBy > 0)
      {
        r.fillRect(cx - 86.0f, cy + 50.0f, 172.0f, 26.0f, rgba(8, 8, 12, 230));
        r.fillRect(cx - 86.0f, cy + 50.0f, 172.0f, 2.0f, rgba(255, 255, 255, 120));
        r.drawText("PLEASE STAND BY", cx, cy + 53.0f, {17.0f, rgb(240, 240, 245), 0}, Align::Center);
      }
      continue;
    }
    if (e.kind == EnemyKind::GhostImage)
    {
      // Two grey after-images behind it (snapping in during the tell), the
      // halo in its tint (gone for a while after it fires), then the body.
      const float lag = e.tell > 0 ? float(e.tell) / float(kImageTell) : 1.0f;
      const int frameNo = (frame / 10) % 2;
      for (int k = 2; k >= 1; --k)
      {
        DrawOpts o;
        o.alpha = 0.5f - 0.12f * float(k - 1);
        r.draw(styledEnemySprite(mArt, r, mTheme, "ghost_image", 2, frameNo, e.w, e.h).get(e.dir),
          mx - float(e.dir) * float(k) * 2.0f * kCellPx * lag, ey, o);
      }
      const bool open = e.dive > 0;
      if (!open)
      {
        const int halo = (mStats.frames / kImageCycle) % 3;
        const float fresh = float(mStats.frames % kImageCycle) / 8.0f; // a new tint swells in
        const Color hc = signalColor(halo);
        drawGlow(r, mArt, mx, ey - h * 0.5f, 120.0f, hc, 0.45f);
        DrawOpts o;
        o.tint = lighten(hc, 0.2f);
        o.blend = Blend::Add;
        o.scale = 1.55f + 0.15f * std::max(0.0f, 1.0f - fresh) + 0.03f * std::sin(float(frame) * 0.2f);
        o.alpha = 0.8f;
        r.draw(ringArt(mArt, r), mx, ey - h * 0.52f, o);
      }
      else
      {
        // No halo: open to anything, a crackle of white round it.
        for (int k = 0; k < 6; ++k)
        {
          const std::uint32_t hs = hash2(frame / 2 + k * 13, e.id);
          const float sx = mx + float(int(hs % 120u) - 60), sy = ey - h * 0.5f + float(int((hs >> 8) % 170u) - 85);
          r.fillRect(sx, sy, 6.0f, 2.0f, rgba(255, 255, 255, 200), Blend::Add);
        }
      }
      const int variant = e.tell > 0 || e.cool > enemyDef(e.def).cooldown - 6 ? 1 : 0;
      DrawOpts o;
      if (e.flash > 0)
        o.tint = rgb(255, 255, 255);
      r.draw(styledEnemySprite(mArt, r, mTheme, "ghost_image", variant, frameNo, e.w, e.h).get(e.dir), mx, ey, o);
      if (e.flash > 0)
      {
        o.blend = Blend::Add;
        o.alpha = 0.6f;
        r.draw(styledEnemySprite(mArt, r, mTheme, "ghost_image", variant, frameNo, e.w, e.h).get(e.dir), mx, ey, o);
      }
      continue;
    }
    if (e.kind == EnemyKind::StaticSwarm)
    {
      // A churning cloud; it brightens as it hisses and shakes while
      // latched on.
      float jx = 0.0f, jy = 0.0f;
      if (e.attach != 0 || e.tell > 0)
      {
        jx = float(int(hash2(frame, e.id) % 7u) - 3);
        jy = float(int(hash2(e.id, frame) % 5u) - 2);
      }
      const int f = (frame / 3 + e.id) % 4;
      const auto& spr = styledEnemySprite(mArt, r, mTheme, "static_swarm", e.carrier ? 1 : 0, f, e.w, e.h).get(e.dir);
      if (e.tell > 0 || e.attach != 0)
        drawGlow(r, mArt, mx + jx, ey - h * 0.5f + jy, 110.0f, e.carrier ? rgb(130, 255, 90) : rgb(230, 230, 245),
          e.tell > 0 ? 0.55f : 0.3f);
      r.draw(spr, mx + jx, ey + jy);
      if (e.tell > 0 || e.flash > 0)
      {
        DrawOpts o;
        o.blend = Blend::Add;
        o.alpha = e.flash > 0 ? 0.8f : 0.3f + 0.5f * float(kSwarmHiss - e.tell) / float(kSwarmHiss);
        r.draw(spr, mx + jx, ey + jy, o);
      }
    }
  }
  // The beam: a thin aim line during the tell, then two cells of light to
  // the first wall.
  if (s.beam.on)
  {
    const auto& b = s.beam;
    const Color c = quarterColor(b.quadrant);
    const float dx = b.x1 - b.x0, dy = b.y1 - b.y0;
    const float len = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
    float reach = len;
    for (float k = 6.0f; k <= len; k += 1.0f)
      if (mMap.solid(int(std::floor(b.x0 + dx * k / len)), int(std::floor(b.y0 + dy * k / len))))
      {
        reach = k;
        break;
      }
    const float x0 = b.x0 * kCellPx - camX, y0 = b.y0 * kCellPx - camY;
    const float x1 = (b.x0 + dx * reach / len) * kCellPx - camX, y1 = (b.y0 + dy * reach / len) * kCellPx - camY;
    if (b.burning)
    {
      const float flick = 0.9f + 0.1f * std::sin(float(frame) * 2.1f);
      r.drawLine(x0, y0, x1, y1, 2.6f * kCellPx * flick, withAlpha(c, 70), Blend::Add);
      r.drawLine(x0, y0, x1, y1, 2.0f * kCellPx * flick, withAlpha(c, 210));
      r.drawLine(x0, y0, x1, y1, 1.1f * kCellPx, withAlpha(lighten(c, 0.5f), 230));
      r.drawLine(x0, y0, x1, y1, 0.4f * kCellPx, rgb(255, 255, 255));
      // Scan lines running along it.
      const float ux = (x1 - x0) / std::max(1.0f, reach * kCellPx), uy = (y1 - y0) / std::max(1.0f, reach * kCellPx);
      for (float t = std::fmod(float(frame) * 24.0f, 48.0f); t < reach * kCellPx; t += 48.0f)
        r.drawLine(x0 + ux * t - uy * kCellPx, y0 + uy * t + ux * kCellPx, x0 + ux * t + uy * kCellPx,
          y0 + uy * t - ux * kCellPx, 3.0f, withAlpha(rgb(255, 255, 255), 90), Blend::Add);
      drawGlow(r, mArt, x0, y0, 90.0f, c, 0.8f);
      drawGlow(r, mArt, x1, y1, 80.0f, c, 0.7f);
      drawGlow(r, mArt, x1, y1, 30.0f, rgb(255, 255, 255), 0.8f);
    }
    else
    {
      // The tell: a dashed aim line marching out, the quarter charging.
      const float u = float(b.t) / float(kCardTell);
      const float ux = (x1 - x0) / std::max(1.0f, reach * kCellPx), uy = (y1 - y0) / std::max(1.0f, reach * kCellPx);
      const float total = reach * kCellPx;
      for (float t = std::fmod(float(frame) * 3.0f, 28.0f); t < total; t += 28.0f)
      {
        const float t1 = std::min(total, t + 16.0f);
        r.drawLine(x0 + ux * t, y0 + uy * t, x0 + ux * t1, y0 + uy * t1, 3.0f, withAlpha(c, 220));
      }
      drawGlow(r, mArt, x0, y0, 30.0f + 50.0f * u, c, 0.5f + 0.4f * u);
    }
  }
  // The Tri-Tone glows in its tint.
  const auto& p = mPlayer;
  if (p.weapon == Weapon::Proto && p.proto == int(ProtoId::TriToneBlaster) && !p.hidden && p.state != PlayerState::Dying)
  {
    static constexpr float kGun[5][2] = {{1.55f, 2.75f}, {1.55f, 1.75f}, {0.45f, 5.0f}, {-0.1f, -0.2f}, {1.2f, 2.8f}};
    const auto& g = kGun[std::min(4, int(p.stance))];
    const float px = (float(p.prevX) + float(p.x - p.prevX) * alpha + 1.5f) * kCellPx - camX;
    const float py = (float(p.prevY) + float(p.y - p.prevY) * alpha + 1.0f) * kCellPx - camY;
    const float gx = px + float(p.facing) * g[0] * kCellPx, gy = py - g[1] * kCellPx;
    const Color c = signalColor(s.tint);
    const float pulse = 0.5f + 0.5f * std::sin(float(frame) * 0.18f);
    drawGlow(r, mArt, gx, gy, 30.0f + 6.0f * pulse, c, 0.55f + 0.2f * pulse);
    drawGlow(r, mArt, gx, gy, 10.0f, lighten(c, 0.6f), 0.7f);
    if (s.tintCool > 0)
    {
      // Just turned: a ring snaps out round the gun.
      DrawOpts o;
      o.tint = c;
      o.blend = Blend::Add;
      o.scale = 0.2f + 0.5f * float(kTintCool - s.tintCool) / float(kTintCool);
      o.alpha = float(s.tintCool) / float(kTintCool);
      r.draw(ringArt(mArt, r), gx, gy, o);
    }
  }
  // A carrier latched on fills a quarter of the screen (its side) with snow,
  // feathered at the edges, under the HUD.
  if (s.snow > 0)
  {
    const float w = float(kScreenW) * 0.5f, h = float(kScreenH) * 0.5f;
    const float x = s.snowQuarter % 2 ? w : 0.0f, y = s.snowQuarter >= 2 ? h : 0.0f;
    const float a = std::min(1.0f, float(s.snow) / 20.0f);
    r.fillRect(x, y, w, h, rgba(70, 70, 78, int(210 * a)));
    drawSnow(r, mArt, x, y, w, h, frame, 0.95f * a, 1);
    drawSnow(r, mArt, x, y, w, h, frame + 1, 0.7f * a, 2);
    // The feathered inner edges.
    const Texture& f = featherArt(mArt, r);
    DrawOpts o;
    o.alpha = a;
    o.cull = false;
    const bool right = s.snowQuarter % 2 != 0, bottom = s.snowQuarter >= 2;
    for (float k = 0.0f; k < h; k += 8.0f)
    {
      o.angle = right ? 0.0f : 180.0f;
      r.draw(f, right ? x - 64.0f : x + w + 64.0f, right ? y + k : y + k + 8.0f, o);
    }
    for (float k = 0.0f; k < w; k += 8.0f)
    {
      o.angle = bottom ? 90.0f : -90.0f;
      r.draw(f, bottom ? x + k + 8.0f : x + k, bottom ? y - 64.0f : y + h + 64.0f, o);
    }
  }
}

void World::drawSignalHud(Renderer& r, int frame) const
{
  const auto& s = mSignal;
  // Tuning in: a bright band sweeps down the picture as it locks.
  if (s.shake > 0 && mState == WorldState::Playing)
  {
    const float u = 1.0f - float(s.shake) / float(kTuneShake);
    const float y = u * float(kScreenH + 120) - 60.0f;
    r.fillRect(0.0f, y, float(kScreenW), 40.0f, rgba(255, 255, 255, 50), Blend::Add);
    r.fillRect(0.0f, y + 12.0f, float(kScreenW), 16.0f, rgba(255, 255, 255, 60), Blend::Add);
  }
  if (s.wrap)
    return;
  // The SIGNAL panel under the score: the swatch (blinking between the two
  // colours while a change waits) and the three tint pips, the Tri-Tone's
  // tint ringed, joined in white while all three are lit.
  const float x = float(12 + kHudHealthW + 10 + kHudWeaponW + 10 + kHudInventoryW + 10 + kHudLettersW + 10);
  const float y = 10.0f + float(kHudPanelH) + 10.0f;
  r.draw(mArt.hudPanels[4], x, y);
  r.drawText("SIGNAL", x + 16, y + 6, {15.0f, kHudLabel, kHudInk});
  r.drawText("TINT", x + 168, y + 6, {15.0f, kHudLabel, kHudInk}, Align::Center);
  const int shown = s.pending >= 0 && (frame / 4) % 2 == 0 ? s.pending : s.color;
  const float sx = x + 16.0f, sy = y + 24.0f;
  r.fillRect(sx + 4.0f, sy + 4.0f, 84.0f, 26.0f, signalColor(shown));
  r.draw(swatchGlass(mArt, r), sx, sy);
  if (s.pending >= 0)
    drawGlow(r, mArt, sx + 46.0f, sy + 17.0f, 50.0f, signalColor(s.pending), 0.3f);
  const bool tri = mPlayer.weapon == Weapon::Proto && mPlayer.proto == int(ProtoId::TriToneBlaster);
  if (blackLit())
  {
    r.fillRect(x + 138.0f, y + 40.0f, 60.0f, 3.0f, rgb(240, 240, 250));
    drawGlow(r, mArt, x + 168.0f, y + 41.0f, 44.0f, rgb(255, 255, 255), 0.35f);
  }
  for (int k = 0; k < 3; ++k)
  {
    const float px = x + 138.0f + float(k) * 30.0f, py = y + 41.0f;
    const bool lit = s.pips[k] > 0;
    if (lit)
      drawGlow(r, mArt, px, py, 26.0f, signalColor(k), 0.45f + 0.35f * float(s.pips[k]) / float(kPipFrames));
    r.draw(pipArt(mArt, r, k, lit), px, py);
    if (lit)
      arc(r, px, py, 12.0f, -float(kPi) * 0.5f, -float(kPi) * 0.5f + 2.0f * float(kPi) * float(s.pips[k]) / float(kPipFrames), 16,
        2.0f, rgb(240, 240, 250));
    if (tri && k == s.tint)
    {
      // The tint the gun is on: a white marker under its pip.
      r.drawLine(px - 6.0f, py + 19.0f, px, py + 14.0f, 2.5f, rgb(255, 255, 255));
      r.drawLine(px, py + 14.0f, px + 6.0f, py + 19.0f, 2.5f, rgb(255, 255, 255));
    }
  }
  // The Test Card's bar: its three colours, three hearts each, the one in
  // signal outlined.
  if (cardFight())
  {
    const float bw = 150.0f, gap = 14.0f, total = 3.0f * bw + 2.0f * gap;
    const float bx = float(kScreenW) * 0.5f - total * 0.5f, by = float(kScreenH) - 40.0f;
    r.fillRect(bx - 12.0f, by - 30.0f, total + 24.0f, 58.0f, rgba(8, 6, 22, 200));
    r.drawText("TEST CARD", bx, by - 27.0f, {17.0f, rgb(240, 240, 245), kHudInk});
    r.drawText("HIT THE QUARTER IN SIGNAL", bx + total, by - 25.0f, {14.0f, kHudLabel, kHudInk}, Align::Right);
    for (int c = 0; c < 3; ++c)
    {
      const float cx = bx + float(c) * (bw + gap);
      r.fillRect(cx, by, bw, 16.0f, rgba(255, 255, 255, 30));
      for (int k = 0; k < 3; ++k)
        if (k < s.cardHp[c])
        {
          const Color col = signalColor(c);
          r.fillRect(cx + float(k) * bw / 3.0f + 1.0f, by, bw / 3.0f - 2.0f, 16.0f, col);
          r.fillRect(cx + float(k) * bw / 3.0f + 1.0f, by + 2.0f, bw / 3.0f - 2.0f, 4.0f, lighten(col, 0.5f));
        }
      if (c == s.color && s.cardHp[c] > 0 && (frame / 8) % 2 == 0)
      {
        r.fillRect(cx - 3.0f, by - 3.0f, bw + 6.0f, 2.0f, rgb(255, 255, 255));
        r.fillRect(cx - 3.0f, by + 17.0f, bw + 6.0f, 2.0f, rgb(255, 255, 255));
        r.fillRect(cx - 3.0f, by - 3.0f, 2.0f, 22.0f, rgb(255, 255, 255));
        r.fillRect(cx + bw + 1.0f, by - 3.0f, 2.0f, 22.0f, rgb(255, 255, 255));
      }
    }
  }
}

} // namespace gr
