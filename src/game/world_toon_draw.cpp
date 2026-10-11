// Level 28, Toon Town Throwdown: drawing. See world_toon.cpp for the logic.
//
// Behind the tiles (drawToonSet): backstage walls in the enclosed rooms
// (the cartoon studio's wallpaper, the basement road's painted dark, Buddy
// Boom's dressing room with its stars), the big top's striped canvas with
// bunting and spotlights, and the Inkwell bonus's ruled page. Behind the
// runner (drawToonBack): ink outlines on the painted earth's open edges,
// the big top's roof and side flaps and its scalloped valance, the stage's
// eight erasable boards, the ink pond, the painted tunnels (a real one is a
// dark passage with its shadow; a fake one is the same painting, flat) and
// the programmer art behind a fake one, the canyon sign, the inkwell on the
// studio desk, Buddy's original sketch on its easel, the full heal's tonic,
// the Portable Hole open in a wall or a floor, and the pianos (their
// shadows, hanging, falling, standing). In front (drawToonFront): the hole
// in flight, a Piano Bird's piano on its ropes, a Rubber Goon's stretched
// punch, the runner's cartoon business (the "uh-oh" sign, bonk stars, the
// hat tipped), Buddy Boom in every state (the mallet's shockwaves, the red
// nose, the giant pencil, his drawn line, the eraser and its shadow, cream
// pies, dizzy stars, his fall into the ink), and the Inkwell's lines, pen
// and refills. drawToonHud: the cream over half the screen after a pie,
// Buddy's bar and the INK meter.

#include "game/world.hpp"

#include "assets/art.hpp"
#include "assets/enemy_art.hpp"
#include "assets/enemy_art_toon.hpp"
#include "base/math.hpp"
#include "game/toon_draw.hpp"
#include "render/vector.hpp"

#include <SDL.h>

#include <algorithm>
#include <cmath>
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
constexpr float kTilePx = float(kTileSize) * kPixelScale; // 64
constexpr float kCellPx = float(kCellSize) * kPixelScale; // 32
constexpr Color kInk = rgb(30, 22, 20);
constexpr Color kPaper = rgb(252, 244, 222);
constexpr Color kTentRed = rgb(214, 66, 58);
constexpr Color kTentCream = rgb(250, 238, 212);
// world_toon.cpp's timings that the drawing follows.
constexpr int kOutlineT = 30; // frames the drawn line is an outline
constexpr int kPieWhiteT = 45;

bool offScreen(float x, float y, float w, float h)
{
  return x > float(kScreenW) + 96.0f || x + w < -96.0f || y > float(kScreenH) + 96.0f || y + h < -96.0f;
}

// A texture baked once and kept in the Art's sprite cache under `key`.
const Texture& baked(const Art& art, const Renderer& r, const std::string& key, int w, int h, float ax, float ay,
  const std::function<void(cairo_t*)>& paint)
{
  const std::string id = "~toon/" + key;
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

void ellipse(cairo_t* cr, double x, double y, double rx, double ry, double angle = 0.0)
{
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_rotate(cr, angle);
  cairo_scale(cr, rx, ry);
  cairo_new_sub_path(cr);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_restore(cr);
}

void ink(cairo_t* cr, double line = 3.0, double a = 1.0)
{
  setRgba(cr, kInk, a);
  cairo_set_line_width(cr, line);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
  cairo_stroke(cr);
}

void fillInk(cairo_t* cr, Color fill, double line = 3.0)
{
  setRgba(cr, fill, 1.0);
  cairo_fill_preserve(cr);
  ink(cr, line);
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

void lettering(cairo_t* cr, const char* text, double cx, double baseline, double size, Color fill, double line)
{
  selectGameFont(cr);
  cairo_set_font_size(cr, size);
  cairo_text_extents_t te;
  cairo_text_extents(cr, text, &te);
  cairo_move_to(cr, cx - te.width * 0.5 - te.x_bearing, baseline);
  cairo_text_path(cr, text);
  setRgba(cr, fill, 1.0);
  cairo_fill_preserve(cr);
  if (line > 0)
    ink(cr, line);
  else
    cairo_new_path(cr);
}

// A five-pointed cartoon star, inked.
void starPath(cairo_t* cr, double x, double y, double r)
{
  for (int i = 0; i < 10; ++i)
  {
    const double a = -kPi / 2 + i * kPi / 5, rr = i % 2 ? r * 0.45 : r;
    (i ? cairo_line_to : cairo_move_to)(cr, x + std::cos(a) * rr, y + std::sin(a) * rr);
  }
  cairo_close_path(cr);
}

// Dry-brush strokes over a 64 px tile, wrapped so it tiles.
void brushTile(cairo_t* cr, Color base, std::uint32_t seed, int count)
{
  Rng rng(seed);
  for (int i = 0; i < count; ++i)
  {
    const double x = rng.range(0, 64), y = rng.range(0, 64), l = rng.range(12, 30);
    for (const double ox : {-64.0, 0.0, 64.0})
      for (const double oy : {-64.0, 0.0, 64.0})
      {
        cairo_move_to(cr, x + ox, y + oy);
        cairo_curve_to(cr, x + ox + l * 0.3, y + oy + 2, x + ox + l * 0.7, y + oy - 2, x + ox + l, y + oy);
      }
    setRgba(cr, i % 2 ? lighten(base, 0.2f) : darken(base, 0.2f), rng.range(0.14f, 0.26f));
    cairo_set_line_width(cr, rng.range(2.5f, 5.0f));
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_stroke(cr);
  }
}

// --- Baked pieces ------------------------------------------------------------------------------

// The painted dark behind an enclosed room: a deep plum-brown, brushed.
const Texture& backstageTile(const Art& art, const Renderer& r, int v)
{
  return baked(art, r, "backstage" + std::to_string(v), 64, 64, 0, 0, [v](cairo_t* cr) {
    const Color base = rgb(92, 62, 66);
    cairo_rectangle(cr, 0, 0, 64, 64);
    setRgba(cr, base, 1.0);
    cairo_fill(cr);
    brushTile(cr, base, 5101u + std::uint32_t(v) * 13u, 9);
  });
}

// The cartoon studio's wallpaper: cream with pastel stripes and dots; the
// bottom row a wooden wainscot.
const Texture& studioTile(const Art& art, const Renderer& r, bool wainscot)
{
  return baked(art, r, wainscot ? "studio_wainscot" : "studio_wall", 64, 64, 0, 0, [wainscot](cairo_t* cr) {
    if (wainscot)
    {
      cairo_rectangle(cr, 0, 0, 64, 64);
      vgrad(cr, 0, 64, rgb(196, 140, 90), rgb(150, 100, 64));
      for (const double x : {0.5, 32.5})
      {
        cairo_rectangle(cr, x + 4, 12, 24, 44);
        setRgba(cr, rgb(120, 76, 48), 0.5);
        cairo_set_line_width(cr, 2);
        cairo_stroke(cr);
      }
      cairo_rectangle(cr, 0, 0, 64, 6);
      setRgba(cr, rgb(110, 70, 44), 1.0);
      cairo_fill(cr);
      return;
    }
    cairo_rectangle(cr, 0, 0, 64, 64);
    setRgba(cr, rgb(246, 232, 204), 1.0);
    cairo_fill(cr);
    for (const double x : {8.0, 40.0})
    {
      cairo_rectangle(cr, x, 0, 12, 64);
      setRgba(cr, rgb(186, 214, 206), 0.7);
      cairo_fill(cr);
    }
    for (const double y : {16.0, 48.0})
    {
      circle(cr, 30, y, 2.4);
      circle(cr, 62, y + 16, 2.4);
      setRgba(cr, rgb(230, 140, 130), 0.8);
      cairo_fill(cr);
    }
  });
}

// Buddy Boom's dressing room: pink wallpaper with little gold stars.
const Texture& dressingTile(const Art& art, const Renderer& r)
{
  return baked(art, r, "dressing", 64, 64, 0, 0, [](cairo_t* cr) {
    cairo_rectangle(cr, 0, 0, 64, 64);
    setRgba(cr, rgb(236, 170, 180), 1.0);
    cairo_fill(cr);
    brushTile(cr, rgb(236, 170, 180), 5113u, 6);
    for (const auto& s : {std::array<double, 2>{16, 18}, std::array<double, 2>{48, 46}})
    {
      starPath(cr, s[0], s[1], 7);
      fillInk(cr, rgb(250, 214, 90), 1.4);
    }
  });
}

// The big top's canvas: a red and a cream stripe, their seams stitched and
// the folds shaded; darker for the roof.
const Texture& tentTile(const Art& art, const Renderer& r, bool roof)
{
  return baked(art, r, roof ? "tent_roof" : "tent", 64, 64, 0, 0, [roof](cairo_t* cr) {
    const float d = roof ? 0.08f : 0.0f;
    cairo_rectangle(cr, 0, 0, 32, 64);
    setRgba(cr, darken(kTentRed, d), 1.0);
    cairo_fill(cr);
    cairo_rectangle(cr, 32, 0, 32, 64);
    setRgba(cr, darken(kTentCream, d), 1.0);
    cairo_fill(cr);
    // Folds: a soft shade down one side of each stripe.
    for (const double x : {0.0, 32.0})
    {
      cairo_rectangle(cr, x + 22, 0, 10, 64);
      cairo_pattern_t* g = cairo_pattern_create_linear(x + 22, 0, x + 32, 0);
      cairo_pattern_add_color_stop_rgba(g, 0, 0.2, 0.05, 0.05, 0.0);
      cairo_pattern_add_color_stop_rgba(g, 1, 0.2, 0.05, 0.05, 0.22);
      cairo_set_source(cr, g);
      cairo_fill(cr);
      cairo_pattern_destroy(g);
    }
    for (const double x : {0.5, 32.5})
    {
      cairo_move_to(cr, x, 0);
      cairo_line_to(cr, x, 64);
      ink(cr, 1.6, 0.8);
      for (int k = 0; k < 4; ++k)
      {
        cairo_move_to(cr, x + 3, 6 + k * 16);
        cairo_line_to(cr, x + 3, 12 + k * 16);
      }
      ink(cr, 1.0, 0.35);
    }
  });
}

// The valance along the bottom of the big top's roof: scallops in red and
// cream, a gold cord along the top and tassels.
const Texture& valance(const Art& art, const Renderer& r)
{
  return baked(art, r, "valance", 128, 64, 0, 0, [](cairo_t* cr) {
    for (int i = 0; i < 4; ++i)
    {
      const double x = i * 32;
      cairo_move_to(cr, x, 6);
      cairo_line_to(cr, x, 26);
      cairo_curve_to(cr, x + 4, 46, x + 28, 46, x + 32, 26);
      cairo_line_to(cr, x + 32, 6);
      cairo_close_path(cr);
      fillInk(cr, i % 2 ? kTentCream : kTentRed, 2.4);
      circle(cr, x + 32, 30, 4.5);
      fillInk(cr, rgb(250, 200, 70), 1.6);
    }
    cairo_rectangle(cr, -2, 0, 132, 9);
    fillInk(cr, rgb(250, 200, 70), 2.4);
  });
}

// Bunting: pennants on a sagging string, one 256 px span.
const Texture& bunting(const Art& art, const Renderer& r)
{
  return baked(art, r, "bunting", 256, 72, 0, 0, [](cairo_t* cr) {
    auto sag = [](double x) { return 8 + 26 * std::sin(kPi * x / 256.0); };
    const Color flags[] = {rgb(240, 200, 70), rgb(90, 150, 220), rgb(214, 66, 58), rgb(120, 190, 110)};
    for (int i = 0; i < 7; ++i)
    {
      const double x = 14 + i * 34;
      cairo_move_to(cr, x - 12, sag(x - 12));
      cairo_line_to(cr, x + 12, sag(x + 12));
      cairo_line_to(cr, x, sag(x) + 32);
      cairo_close_path(cr);
      fillInk(cr, flags[i % 4], 2.0);
    }
    cairo_move_to(cr, 0, sag(0));
    for (double x = 0; x <= 256; x += 8)
      cairo_line_to(cr, x, sag(x));
    ink(cr, 2.2);
  });
}

// The stage: five blocks of boards, a top face, a red skirt with gold trim.
const Texture& stageBoards(const Art& art, const Renderer& r, int blocks)
{
  const int w = blocks * 64;
  return baked(art, r, "stage" + std::to_string(blocks), w, 80, 0, 14, [w](cairo_t* cr) {
    cairo_rectangle(cr, 1, 2, w - 2, 14);
    vgrad(cr, 2, 16, rgb(240, 200, 140), rgb(214, 160, 100), true);
    ink(cr, 2.4);
    for (int x = 40; x < w; x += 40)
    {
      cairo_move_to(cr, x, 3);
      cairo_line_to(cr, x - 4, 15);
    }
    ink(cr, 1.2, 0.6);
    cairo_rectangle(cr, 1, 16, w - 2, 62);
    vgrad(cr, 16, 78, rgb(204, 56, 50), rgb(150, 34, 34), true);
    ink(cr, 2.6);
    for (int x = 16; x < w; x += 32)
    {
      cairo_move_to(cr, x, 22);
      cairo_curve_to(cr, x - 3, 40, x + 3, 56, x, 76);
    }
    ink(cr, 1.2, 0.35);
    cairo_rectangle(cr, 1, 16, w - 2, 6);
    fillInk(cr, rgb(250, 200, 70), 1.6);
  });
}

// The ink's surface: a wavy black edge, white highlights riding it. 256 px.
const Texture& inkSurface(const Art& art, const Renderer& r)
{
  return baked(art, r, "ink_surface", 256, 48, 0, 0, [](cairo_t* cr) {
    auto top = [](double x) { return 14 + 5 * std::sin(2 * kPi * x / 128.0) + 2 * std::sin(2 * kPi * x / 64.0 + 1.0); };
    cairo_move_to(cr, 0, 48);
    for (double x = 0; x <= 256; x += 4)
      cairo_line_to(cr, x, top(x));
    cairo_line_to(cr, 256, 48);
    cairo_close_path(cr);
    setRgba(cr, rgb(20, 14, 22), 1.0);
    cairo_fill(cr);
    for (double x = 0; x <= 256; x += 4)
      (x == 0 ? cairo_move_to : cairo_line_to)(cr, x, top(x));
    setRgba(cr, rgb(110, 90, 130), 0.9);
    cairo_set_line_width(cr, 2.0);
    cairo_stroke(cr);
    for (const double hx : {30.0, 96.0, 158.0, 222.0})
    {
      cairo_move_to(cr, hx - 10, top(hx) + 8);
      cairo_curve_to(cr, hx - 4, top(hx) + 4, hx + 4, top(hx) + 4, hx + 10, top(hx) + 8);
      setRgba(cr, rgb(255, 255, 255), 0.8);
      cairo_set_line_width(cr, 2.6);
      cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
      cairo_stroke(cr);
    }
  });
}

// A painted tunnel (2 x 3 blocks): a stone arch round a dark opening. A
// real one is deep (shaded to black) with its shadow along the bottom; a
// fake one is the same painting done flat, a drip at its foot.
const Texture& tunnelArt(const Art& art, const Renderer& r, bool real)
{
  return baked(art, r, real ? "tunnel_real" : "tunnel_fake", 128, 192, 0, 0, [real](cairo_t* cr) {
    auto arch = [cr](double inset) {
      cairo_move_to(cr, 10 + inset, 192);
      cairo_line_to(cr, 10 + inset, 76);
      cairo_arc(cr, 64, 76, 54 - inset, kPi, 0);
      cairo_line_to(cr, 118 - inset, 192);
      cairo_close_path(cr);
    };
    // The stones round the arch.
    arch(-8);
    fillInk(cr, rgb(214, 170, 120), 3.0);
    for (int i = 0; i <= 8; ++i)
    {
      const double a = kPi + i * kPi / 8;
      cairo_move_to(cr, 64 + std::cos(a) * 46, 76 + std::sin(a) * 46);
      cairo_line_to(cr, 64 + std::cos(a) * 62, 76 + std::sin(a) * 62);
    }
    for (int i = 0; i < 4; ++i)
    {
      cairo_move_to(cr, 2, 96 + i * 26);
      cairo_line_to(cr, 18, 96 + i * 26);
      cairo_move_to(cr, 110, 96 + i * 26);
      cairo_line_to(cr, 126, 96 + i * 26);
    }
    ink(cr, 2.0);
    arch(8);
    if (real)
    {
      cairo_pattern_t* g = cairo_pattern_create_radial(64, 140, 6, 64, 120, 90);
      cairo_pattern_add_color_stop_rgb(g, 0, 0.0, 0.0, 0.0);
      cairo_pattern_add_color_stop_rgb(g, 0.6, 0.08, 0.05, 0.08);
      cairo_pattern_add_color_stop_rgb(g, 1, 0.24, 0.16, 0.18);
      cairo_set_source(cr, g);
      cairo_fill_preserve(cr);
      cairo_pattern_destroy(g);
      ink(cr, 3.4);
      // The passage's floor going in, and its shadow on the ground.
      cairo_move_to(cr, 22, 192);
      cairo_line_to(cr, 44, 160);
      cairo_line_to(cr, 84, 160);
      cairo_line_to(cr, 106, 192);
      cairo_close_path(cr);
      setRgba(cr, rgb(60, 40, 40), 0.7);
      cairo_fill(cr);
      cairo_rectangle(cr, 0, 160, 128, 32);
      cairo_pattern_t* s = cairo_pattern_create_linear(0, 160, 0, 192);
      cairo_pattern_add_color_stop_rgba(s, 0, 0, 0, 0, 0.0);
      cairo_pattern_add_color_stop_rgba(s, 1, 0, 0, 0, 0.6);
      cairo_set_source(cr, s);
      cairo_fill(cr);
      cairo_pattern_destroy(s);
    }
    else
    {
      setRgba(cr, rgb(34, 26, 34), 1.0);
      cairo_fill_preserve(cr);
      ink(cr, 3.4);
      // Brush marks: it is paint.
      Rng rng(5119u);
      for (int i = 0; i < 9; ++i)
      {
        const double x = rng.range(26, 96), y = rng.range(40, 176);
        cairo_move_to(cr, x, y);
        cairo_line_to(cr, x + rng.range(10, 22), y + rng.range(-3, 3));
      }
      setRgba(cr, rgb(80, 66, 80), 0.6);
      cairo_set_line_width(cr, 3);
      cairo_stroke(cr);
      cairo_move_to(cr, 86, 186);
      cairo_curve_to(cr, 84, 192, 92, 192, 90, 186);
      cairo_line_to(cr, 88, 176);
      cairo_close_path(cr);
      setRgba(cr, rgb(34, 26, 34), 1.0);
      cairo_fill(cr);
    }
  });
}

// Behind the fake tunnel's paint: the PoC's first level, crude on purpose.
// Flat squares without anti-aliasing, a magenta placeholder runner.
const Texture& retroArt(const Art& art, const Renderer& r, int f)
{
  return baked(art, r, "retro" + std::to_string(f), 128, 192, 0, 0, [f](cairo_t* cr) {
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
    for (int y = 0; y < 12; ++y)
      for (int x = 0; x < 8; ++x)
      {
        const bool ground = y >= 8;
        cairo_rectangle(cr, x * 16, y * 16, 16, 16);
        setRgba(cr, ground ? ((x + y) % 2 ? rgb(0, 168, 0) : rgb(0, 136, 0)) : rgb(0, 120, 248), 1.0);
        cairo_fill(cr);
        if (ground)
        {
          cairo_rectangle(cr, x * 16, y * 16, 16, 2);
          setRgba(cr, rgb(120, 255, 120), 1.0);
          cairo_fill(cr);
        }
      }
    cairo_rectangle(cr, 16, 32, 32, 16);
    setRgba(cr, rgb(255, 255, 255), 1.0);
    cairo_fill(cr);
    // The placeholder runner, a magenta box with eyes.
    const int bob = f % 2 ? 0 : 4;
    cairo_rectangle(cr, 72, 80 + bob, 32, 48);
    setRgba(cr, rgb(255, 0, 255), 1.0);
    cairo_fill(cr);
    cairo_rectangle(cr, 88, 88 + bob, 4, 4);
    cairo_rectangle(cr, 96, 88 + bob, 4, 4);
    setRgba(cr, rgb(0, 0, 0), 1.0);
    cairo_fill(cr);
    cairo_rectangle(cr, 0, 176, 128, 16);
    setRgba(cr, rgb(0, 0, 0), 1.0);
    cairo_fill(cr);
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);
  });
}

// The canyon's sign on its post, hand lettered.
const Texture& canyonSign(const Art& art, const Renderer& r)
{
  return baked(art, r, "sign", 320, 220, 160, 216, [](cairo_t* cr) {
    cairo_rectangle(cr, 150, 80, 20, 136);
    fillInk(cr, rgb(170, 120, 74), 2.6);
    cairo_save(cr);
    cairo_translate(cr, 160, 64);
    cairo_rotate(cr, -0.04);
    cairo_move_to(cr, -146, -50);
    cairo_line_to(cr, 140, -54);
    cairo_line_to(cr, 150, 44);
    cairo_line_to(cr, -150, 50);
    cairo_close_path(cr);
    vgrad(cr, -54, 50, rgb(246, 222, 170), rgb(222, 186, 128), true);
    ink(cr, 3.4);
    for (const double y : {-18.0, 16.0})
    {
      cairo_move_to(cr, -140, y);
      cairo_curve_to(cr, -60, y - 2, 60, y + 3, 142, y);
    }
    ink(cr, 1.0, 0.3);
    lettering(cr, "GRAVITY HASN'T", 0, -10, 26, kInk, 0);
    lettering(cr, "BEEN INVENTED YET", 0, 26, 22, rgb(190, 50, 40), 0);
    for (const double nx : {-132.0, 130.0})
    {
      circle(cr, nx, -38, 3);
      setRgba(cr, kInk, 1.0);
      cairo_fill(cr);
    }
    cairo_restore(cr);
  });
}

// Buddy Boom's original sketch, pinned to an easel.
const Texture& sketchArt(const Art& art, const Renderer& r)
{
  return baked(art, r, "sketch", 140, 170, 70, 166, [](cairo_t* cr) {
    for (const auto& l : {std::array<double, 4>{40, 20, 20, 166}, std::array<double, 4>{100, 20, 120, 166},
           std::array<double, 4>{70, 30, 70, 160}})
    {
      cairo_move_to(cr, l[0], l[1]);
      cairo_line_to(cr, l[2], l[3]);
      setRgba(cr, kInk, 1.0);
      cairo_set_line_width(cr, 8);
      cairo_stroke_preserve(cr);
      setRgba(cr, rgb(190, 140, 90), 1.0);
      cairo_set_line_width(cr, 4.5);
      cairo_stroke(cr);
    }
    cairo_save(cr);
    cairo_translate(cr, 70, 70);
    cairo_rotate(cr, 0.04);
    cairo_rectangle(cr, -50, -62, 100, 120);
    fillInk(cr, rgb(250, 246, 232), 2.6);
    // The sketch itself, faded like pencil.
    cairo_push_group(cr);
    paintBuddy(cr, 0, 46, 0.2, 15, 0);
    cairo_pop_group_to_source(cr);
    cairo_paint_with_alpha(cr, 0.55);
    cairo_rectangle(cr, -50, -62, 100, 120);
    setRgba(cr, rgb(150, 140, 130), 0.25);
    cairo_fill(cr);
    // Pencil scribbles and the date.
    cairo_move_to(cr, -40, 50);
    cairo_curve_to(cr, -30, 46, -20, 54, -8, 48);
    ink(cr, 1.2, 0.6);
    circle(cr, 0, -56, 5);
    fillInk(cr, rgb(220, 50, 50), 1.6);
    cairo_restore(cr);
    cairo_rectangle(cr, 18, 128, 104, 10);
    fillInk(cr, rgb(170, 120, 74), 2.2);
  });
}

// The full heal: a big bottle of cartoon tonic with a red cross.
const Texture& tonicArt(const Art& art, const Renderer& r)
{
  return baked(art, r, "tonic", 96, 140, 48, 136, [](cairo_t* cr) {
    cairo_move_to(cr, 36, 12);
    cairo_line_to(cr, 60, 12);
    cairo_line_to(cr, 60, 40);
    cairo_curve_to(cr, 90, 50, 88, 80, 88, 120);
    cairo_curve_to(cr, 88, 134, 8, 134, 8, 120);
    cairo_curve_to(cr, 8, 80, 6, 50, 36, 40);
    cairo_close_path(cr);
    vgrad(cr, 10, 134, rgb(250, 140, 150), rgb(214, 60, 80), true);
    ink(cr, 3.2);
    cairo_rectangle(cr, 32, 2, 32, 14);
    fillInk(cr, rgb(170, 120, 74), 2.4);
    cairo_rectangle(cr, 20, 70, 56, 44);
    fillInk(cr, rgb(252, 248, 236), 2.2);
    cairo_rectangle(cr, 42, 76, 12, 32);
    cairo_rectangle(cr, 32, 86, 32, 12);
    setRgba(cr, rgb(220, 40, 50), 1.0);
    cairo_fill(cr);
    ellipse(cr, 26, 56, 6, 12, 0.3);
    setRgba(cr, rgb(255, 255, 255), 0.7);
    cairo_fill(cr);
    selectGameFont(cr);
    cairo_set_font_size(cr, 11);
    cairo_move_to(cr, 24, 66);
    cairo_show_text(cr, "TONIC");
  });
}

// The inkwell on the studio desk: a squat glass bottle, its label, the
// ink's shine.
const Texture& inkwellArt(const Art& art, const Renderer& r)
{
  return baked(art, r, "inkwell", 160, 140, 80, 136, [](cairo_t* cr) {
    cairo_move_to(cr, 50, 30);
    cairo_line_to(cr, 110, 30);
    cairo_line_to(cr, 110, 48);
    cairo_curve_to(cr, 150, 56, 152, 80, 150, 126);
    cairo_curve_to(cr, 150, 138, 10, 138, 10, 126);
    cairo_curve_to(cr, 8, 80, 10, 56, 50, 48);
    cairo_close_path(cr);
    setRgba(cr, rgb(40, 30, 70), 0.82);
    cairo_fill_preserve(cr);
    ink(cr, 3.4);
    cairo_rectangle(cr, 44, 18, 72, 16);
    fillInk(cr, rgb(30, 22, 28), 2.6);
    cairo_rectangle(cr, 34, 76, 92, 36);
    fillInk(cr, rgb(250, 240, 214), 2.2);
    lettering(cr, "INK", 80, 104, 26, kInk, 0);
    ellipse(cr, 32, 70, 7, 22, 0.15);
    setRgba(cr, rgb(255, 255, 255), 0.5);
    cairo_fill(cr);
    // A quill standing in it.
    cairo_move_to(cr, 96, 24);
    cairo_curve_to(cr, 110, -10, 140, -10, 150, 4);
    cairo_curve_to(cr, 136, 6, 116, 12, 96, 24);
    cairo_close_path(cr);
    fillInk(cr, rgb(250, 250, 244), 2.0);
  });
}

// A piano (variant 0 hanging or falling, 1 landed) with a 16 px margin.
const Texture& pianoArt(const Art& art, const Renderer& r, int v)
{
  return baked(art, r, "piano" + std::to_string(v), 224, 160, 16, 16, [v](cairo_t* cr) { paintPiano(cr, 16, 16, 1.0, v); });
}

// A soft round shadow, 256 x 64.
const Texture& shadowArt(const Art& art, const Renderer& r)
{
  return baked(art, r, "shadow", 256, 64, 128, 32, [](cairo_t* cr) {
    cairo_save(cr);
    cairo_translate(cr, 128, 32);
    cairo_scale(cr, 4.0, 1.0);
    cairo_pattern_t* g = cairo_pattern_create_radial(0, 0, 0, 0, 0, 31);
    cairo_pattern_add_color_stop_rgba(g, 0, 0.1, 0.05, 0.05, 0.6);
    cairo_pattern_add_color_stop_rgba(g, 0.7, 0.1, 0.05, 0.05, 0.45);
    cairo_pattern_add_color_stop_rgba(g, 1, 0.1, 0.05, 0.05, 0.0);
    cairo_arc(cr, 0, 0, 31, 0, 2 * kPi);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
    cairo_restore(cr);
  });
}

// The Portable Hole open: a black hole with a white rim, w x h px (anchor
// at its middle).
const Texture& holeArt(const Art& art, const Renderer& r, int w, int h)
{
  return baked(art, r, "hole" + std::to_string(w) + "x" + std::to_string(h), w + 24, h + 24, float(w + 24) * 0.5f,
    float(h + 24) * 0.5f, [w, h](cairo_t* cr) {
      const double cx = (w + 24) * 0.5, cy = (h + 24) * 0.5, rx = w * 0.5 + 4, ry = h * 0.5 + 4;
      ellipse(cr, cx, cy, rx + 6, ry + 6);
      fillInk(cr, rgb(255, 255, 255), 3.0);
      ellipse(cr, cx, cy, rx - 2, ry - 2);
      cairo_pattern_t* g = cairo_pattern_create_radial(cx, cy, 2, cx, cy, std::max(rx, ry));
      cairo_pattern_add_color_stop_rgb(g, 0, 0.0, 0.0, 0.0);
      cairo_pattern_add_color_stop_rgb(g, 0.7, 0.04, 0.02, 0.06);
      cairo_pattern_add_color_stop_rgb(g, 1, 0.16, 0.1, 0.2);
      cairo_set_source(cr, g);
      cairo_fill_preserve(cr);
      cairo_pattern_destroy(g);
      ink(cr, 2.4);
      // A swirl in the dark.
      cairo_save(cr);
      cairo_translate(cr, cx, cy);
      cairo_scale(cr, rx / 60.0, ry / 60.0);
      for (int i = 0; i < 3; ++i)
      {
        cairo_new_sub_path(cr);
        cairo_arc(cr, 0, 0, 14 + i * 14, i * 1.4, i * 1.4 + 2.2);
      }
      cairo_restore(cr);
      setRgba(cr, rgb(120, 90, 160), 0.5);
      cairo_set_line_width(cr, 2.0);
      cairo_stroke(cr);
    });
}

// The hole in flight: a spinning disc, rim and all.
const Texture& tossArt(const Art& art, const Renderer& r)
{
  return baked(art, r, "toss", 64, 64, 32, 32, [](cairo_t* cr) {
    ellipse(cr, 32, 32, 26, 12);
    fillInk(cr, rgb(255, 255, 255), 2.6);
    ellipse(cr, 32, 32, 20, 8);
    setRgba(cr, rgb(10, 6, 14), 1.0);
    cairo_fill(cr);
  });
}

// The "uh-oh" sign on its stick (anchor: the stick's foot, in the hand).
const Texture& uhOhArt(const Art& art, const Renderer& r)
{
  return baked(art, r, "uhoh", 200, 170, 100, 166, [](cairo_t* cr) {
    cairo_rectangle(cr, 94, 70, 12, 96);
    fillInk(cr, rgb(190, 140, 90), 2.4);
    cairo_save(cr);
    cairo_translate(cr, 100, 46);
    cairo_rotate(cr, -0.06);
    roundedRect(cr, -88, -38, 176, 72, 8);
    vgrad(cr, -38, 34, rgb(255, 250, 232), rgb(240, 222, 186), true);
    ink(cr, 3.4);
    lettering(cr, "UH-OH!", 0, 14, 36, rgb(214, 50, 44), 2.0);
    cairo_restore(cr);
  });
}

const Texture& starArt(const Art& art, const Renderer& r)
{
  return baked(art, r, "star", 40, 40, 20, 20, [](cairo_t* cr) {
    starPath(cr, 20, 21, 16);
    fillInk(cr, rgb(255, 222, 70), 2.4);
    circle(cr, 16, 16, 2.4);
    setRgba(cr, rgb(255, 255, 230), 1.0);
    cairo_fill(cr);
  });
}

// A straw boater, tipped.
const Texture& hatArt(const Art& art, const Renderer& r)
{
  return baked(art, r, "hat", 96, 56, 48, 48, [](cairo_t* cr) {
    ellipse(cr, 48, 40, 44, 9);
    fillInk(cr, rgb(244, 214, 130), 2.6);
    cairo_rectangle(cr, 22, 10, 52, 30);
    fillInk(cr, rgb(244, 214, 130), 2.6);
    cairo_rectangle(cr, 22, 24, 52, 10);
    fillInk(cr, rgb(214, 60, 52), 2.0);
    ellipse(cr, 48, 10, 26, 5);
    fillInk(cr, rgb(250, 226, 150), 2.2);
  });
}

// A mallet shockwave: a white crescent of dust with speed lines (runs +x).
const Texture& waveArt(const Art& art, const Renderer& r)
{
  return baked(art, r, "wave", 96, 64, 48, 60, [](cairo_t* cr) {
    cairo_move_to(cr, 14, 60);
    cairo_curve_to(cr, 30, 14, 64, 6, 84, 24);
    cairo_curve_to(cr, 70, 26, 58, 40, 56, 60);
    cairo_close_path(cr);
    fillInk(cr, rgb(255, 252, 240), 2.6);
    for (int i = 0; i < 3; ++i)
    {
      cairo_move_to(cr, 2, 30 + i * 10);
      cairo_line_to(cr, 18 - i * 4, 30 + i * 10);
    }
    ink(cr, 2.2);
    for (const auto& p : {std::array<double, 3>{30, 52, 7}, std::array<double, 3>{46, 54, 6}, std::array<double, 3>{70, 56, 5}})
    {
      circle(cr, p[0], p[1], p[2]);
      fillInk(cr, rgb(236, 220, 196), 1.8);
    }
  });
}

const Texture& pieArt(const Art& art, const Renderer& r)
{
  return baked(art, r, "pie", 96, 80, 48, 48, [](cairo_t* cr) { paintCreamPie(cr, 48, 46, 1.0); });
}

// His drawing: a solid ink line ten blocks long, wobbly, a drip or two.
const Texture& lineArt(const Art& art, const Renderer& r, int blocks)
{
  const int w = blocks * 64;
  return baked(art, r, "line" + std::to_string(blocks), w + 16, 48, 8, 4, [w](cairo_t* cr) {
    cairo_move_to(cr, 8, 8);
    for (int x = 0; x <= w; x += 8)
      cairo_line_to(cr, 8 + x, 6 + 1.6 * std::sin(x * 0.05));
    for (int x = w; x >= 0; x -= 8)
      cairo_line_to(cr, 8 + x, 22 + 1.8 * std::sin(x * 0.037 + 1.0));
    cairo_close_path(cr);
    setRgba(cr, rgb(24, 18, 22), 1.0);
    cairo_fill(cr);
    for (const double dx : {0.23, 0.61, 0.86})
    {
      const double x = 8 + w * dx;
      cairo_move_to(cr, x - 4, 20);
      cairo_curve_to(cr, x - 4, 34, x + 4, 34, x + 4, 20);
      cairo_close_path(cr);
      cairo_fill(cr);
      circle(cr, x, 38, 3.4);
      cairo_fill(cr);
    }
    cairo_move_to(cr, 16, 10);
    cairo_line_to(cr, 8 + w * 0.4, 10);
    setRgba(cr, rgb(130, 120, 150), 0.6);
    cairo_set_line_width(cr, 2);
    cairo_stroke(cr);
  });
}

// The giant eraser: a pink rubber block in its paper sleeve.
const Texture& eraserArt(const Art& art, const Renderer& r)
{
  return baked(art, r, "eraser", 240, 120, 120, 112, [](cairo_t* cr) {
    cairo_move_to(cr, 24, 30);
    cairo_line_to(cr, 216, 30);
    cairo_line_to(cr, 226, 108);
    cairo_line_to(cr, 14, 108);
    cairo_close_path(cr);
    vgrad(cr, 30, 108, rgb(250, 170, 180), rgb(226, 120, 140), true);
    ink(cr, 3.4);
    cairo_rectangle(cr, 20, 14, 200, 44);
    fillInk(cr, rgb(70, 110, 200), 3.0);
    lettering(cr, "ERASER", 120, 46, 26, rgb(252, 244, 222), 1.4);
    ellipse(cr, 50, 84, 16, 5);
    setRgba(cr, rgb(255, 230, 236), 0.7);
    cairo_fill(cr);
  });
}

// The giant pencil standing on its point, its point at the bottom.
const Texture& pencilArt(const Art& art, const Renderer& r)
{
  return baked(art, r, "pencil_stand", 80, 640, 40, 636,
    [](cairo_t* cr) { paintToonPencil(cr, 40, 8, 40, 636, 48); });
}

// The red flashing nose: a shiny bulb.
const Texture& noseArt(const Art& art, const Renderer& r, bool bright)
{
  return baked(art, r, bright ? "nose_on" : "nose_dim", 64, 56, 32, 28, [bright](cairo_t* cr) {
    ellipse(cr, 32, 28, 23, 20);
    cairo_pattern_t* g = cairo_pattern_create_radial(24, 20, 2, 32, 28, 23);
    cairo_pattern_add_color_stop_rgb(g, 0, 1.0, bright ? 0.7 : 0.5, bright ? 0.6 : 0.5);
    cairo_pattern_add_color_stop_rgb(g, 1, bright ? 0.95 : 0.7, 0.05, 0.08);
    cairo_set_source(cr, g);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(g);
    ink(cr, 2.6);
    ellipse(cr, 25, 20, 6, 3.5, -0.5);
    setRgba(cr, rgb(255, 255, 255), 0.85);
    cairo_fill(cr);
  });
}

// An ink refill: a little bottle, corked.
const Texture& refillArt(const Art& art, const Renderer& r)
{
  return baked(art, r, "refill", 64, 96, 32, 92, [](cairo_t* cr) {
    cairo_move_to(cr, 24, 22);
    cairo_line_to(cr, 40, 22);
    cairo_line_to(cr, 40, 36);
    cairo_curve_to(cr, 58, 42, 56, 60, 56, 82);
    cairo_curve_to(cr, 56, 92, 8, 92, 8, 82);
    cairo_curve_to(cr, 8, 60, 6, 42, 24, 36);
    cairo_close_path(cr);
    setRgba(cr, rgb(40, 30, 70), 0.9);
    cairo_fill_preserve(cr);
    ink(cr, 2.8);
    cairo_rectangle(cr, 22, 8, 20, 16);
    fillInk(cr, rgb(200, 150, 100), 2.2);
    cairo_rectangle(cr, 14, 56, 36, 20);
    fillInk(cr, rgb(250, 240, 214), 1.8);
    lettering(cr, "+80", 32, 72, 13, kInk, 0);
    ellipse(cr, 18, 50, 3, 9, 0.2);
    setRgba(cr, rgb(255, 255, 255), 0.6);
    cairo_fill(cr);
  });
}

// The Inkwell's pen: a steel nib, its point at the anchor, pointing +x.
const Texture& penArt(const Art& art, const Renderer& r)
{
  return baked(art, r, "pen", 120, 48, 112, 24, [](cairo_t* cr) {
    cairo_rectangle(cr, 2, 16, 56, 16);
    fillInk(cr, rgb(40, 34, 60), 2.4);
    cairo_rectangle(cr, 56, 14, 12, 20);
    fillInk(cr, rgb(250, 210, 90), 2.0);
    cairo_move_to(cr, 68, 12);
    cairo_line_to(cr, 112, 24);
    cairo_line_to(cr, 68, 36);
    cairo_close_path(cr);
    fillInk(cr, rgb(200, 206, 220), 2.4);
    cairo_move_to(cr, 80, 24);
    cairo_line_to(cr, 110, 24);
    ink(cr, 1.4);
    circle(cr, 80, 24, 3);
    setRgba(cr, kInk, 1.0);
    cairo_fill(cr);
  });
}

// The cream after a pie: a drippy edge for the inner side of the splat
// (mirrored for the right half).
const Texture& splatEdge(const Art& art, const Renderer& r, bool mirror)
{
  return baked(art, r, mirror ? "splat_edge_r" : "splat_edge", 140, kScreenH, 0, 0, [mirror](cairo_t* cr) {
    if (mirror)
    {
      cairo_translate(cr, 140, 0);
      cairo_scale(cr, -1, 1);
    }
    cairo_move_to(cr, 0, 0);
    double y = 0;
    Rng rng(5147u);
    cairo_line_to(cr, 40, 0);
    while (y < kScreenH)
    {
      const double bump = rng.range(30, 70), out = rng.range(20, 110);
      cairo_curve_to(cr, out, y + bump * 0.2, out, y + bump * 0.8, rng.range(10, 40), y + bump);
      y += bump;
    }
    cairo_line_to(cr, 0, kScreenH);
    cairo_close_path(cr);
    setRgba(cr, rgb(255, 252, 240), 1.0);
    cairo_fill_preserve(cr);
    setRgba(cr, rgb(220, 200, 170), 1.0);
    cairo_set_line_width(cr, 4);
    cairo_stroke(cr);
  });
}

// Buddy's head for his bar.
const Texture& buddyIcon(const Art& art, const Renderer& r)
{
  return baked(art, r, "buddy_icon", 96, 96, 48, 48, [](cairo_t* cr) {
    circle(cr, 48, 48, 44);
    fillInk(cr, rgb(250, 200, 70), 3.0);
    cairo_save(cr);
    circle(cr, 48, 48, 42);
    cairo_clip(cr);
    paintBuddy(cr, 30, 128, 0.24, 9, 0);
    cairo_restore(cr);
  });
}

// The INK meter's inkwell icon.
const Texture& inkIcon(const Art& art, const Renderer& r)
{
  return baked(art, r, "ink_icon", 80, 70, 40, 35, [](cairo_t* cr) {
    cairo_save(cr);
    cairo_translate(cr, 0, 2);
    cairo_scale(cr, 0.5, 0.5);
    cairo_translate(cr, 0, -6);
    cairo_move_to(cr, 50, 30);
    cairo_line_to(cr, 110, 30);
    cairo_line_to(cr, 110, 48);
    cairo_curve_to(cr, 150, 56, 152, 80, 150, 126);
    cairo_curve_to(cr, 150, 138, 10, 138, 10, 126);
    cairo_curve_to(cr, 8, 80, 10, 56, 50, 48);
    cairo_close_path(cr);
    setRgba(cr, rgb(40, 30, 70), 1.0);
    cairo_fill_preserve(cr);
    ink(cr, 5);
    cairo_rectangle(cr, 44, 18, 72, 16);
    fillInk(cr, rgb(30, 22, 28), 4);
    cairo_restore(cr);
  });
}

// Wobbly ink edge segments on a block's open side: an inked line with a
// little jitter from the block's hash.
void inkEdge(Renderer& r, float x0, float y0, float x1, float y1, int seed)
{
  const float j = float(hash2(seed, 7) % 5u) - 2.0f;
  const float mx = (x0 + x1) * 0.5f + (y0 == y1 ? 0.0f : j), my = (y0 + y1) * 0.5f + (y0 == y1 ? j * 0.6f : 0.0f);
  r.drawLine(x0, y0, mx, my, 4.0f, kInk);
  r.drawLine(mx, my, x1, y1, 4.0f, kInk);
}

} // namespace

// --- The set, behind the tiles -------------------------------------------------------------------

bool toonDrawsBlock(const World& w, int tx, int ty)
{
  for (const auto& pn : w.toon().pianos)
    if (pn.state == 2 && tx >= pn.bx && tx < pn.bx + 3 && ty >= pn.by && ty < pn.by + 2)
      return true;
  if (w.ink().on)
    for (const auto& ib : w.ink().blocks)
      if (ib.x == tx && ib.y == ty)
        return true;
  return false;
}

void drawToonSet(Renderer& r, const World& w, const Art& art, float camX, float camY, int frame)
{
  const auto& t = w.toon();
  const auto& map = w.map();
  if (w.ink().on)
  {
    // The Inkwell: a blank page, ruled, its margin in red.
    r.fillRect(0.0f, 0.0f, float(kScreenW), float(kScreenH), rgb(250, 246, 234));
    const float y0 = std::fmod(-camY, 48.0f);
    for (float y = y0; y < float(kScreenH); y += 48.0f)
      r.fillRect(0.0f, y, float(kScreenW), 2.0f, rgba(120, 160, 210, 70));
    const float mx = 3.0f * kTilePx - camX;
    if (mx > -4.0f && mx < float(kScreenW))
      r.fillRect(mx, 0.0f, 3.0f, float(kScreenH), rgba(220, 80, 80, 110));
    return;
  }
  if (!t.on)
    return;
  const auto& b = t.buddy;
  const int tx0 = std::max(0, int(camX / kTilePx) - 1);
  const int ty0 = std::max(0, int(camY / kTilePx) - 1);
  const int tx1 = std::min(map.widthBlocks() - 1, int((camX + float(kScreenW)) / kTilePx) + 1);
  const int ty1 = std::min(map.heightBlocks() - 1, int((camY + float(kScreenH)) / kTilePx) + 1);
  auto inArena = [&](int bx, int by) { return b.on && bx > b.x0 && bx < b.x1 && by >= b.y0 && by <= b.y1; };
  auto inBox = [](const CellBox& c, int bx, int by) {
    return bx * kCellsPerTile >= c.x && bx * kCellsPerTile < c.x + c.w && by * kCellsPerTile >= c.y && by * kCellsPerTile < c.y + c.h;
  };
  for (int tx = tx0; tx <= tx1; ++tx)
  {
    // A column is roofed from the first solid block down.
    bool roofed = false;
    for (int y = 0; y < ty0 && !roofed; ++y)
      roofed = map.block(tx, y) == Tile::Solid;
    for (int ty = ty0; ty <= ty1; ++ty)
    {
      const Tile tile = map.block(tx, ty);
      if (tile == Tile::Solid)
      {
        roofed = true;
        continue;
      }
      const float x = float(tx) * kTilePx - camX, y = float(ty) * kTilePx - camY;
      if (inArena(tx, ty))
      {
        // Inside the big top the canvas is in shadow, so the show stands out.
        DrawOpts o;
        o.tint = rgb(178, 160, 156);
        r.draw(tentTile(art, r, false), x, y, o);
      }
      else if (inBox(t.dressing, tx, ty))
        r.draw(dressingTile(art, r), x, y);
      else if (inBox(t.studio, tx, ty))
        r.draw(studioTile(art, r, !inBox(t.studio, tx, ty + 1)), x, y);
      else if (roofed)
        r.draw(backstageTile(art, r, int(hash2(tx, ty) % 2u)), x, y);
    }
  }
  if (b.on)
  {
    // Bunting across the big top and two spotlights from the roof.
    const float y = float(b.y0 + 1) * kTilePx - camY;
    for (int bx = b.x0 + 1; bx + 4 <= b.x1; bx += 4)
    {
      const float x = float(bx) * kTilePx - camX;
      if (!offScreen(x, y, 256.0f, 72.0f))
        r.draw(bunting(art, r), x, y);
    }
    for (int k = 0; k < 2; ++k)
    {
      const float sx = float(b.x0 + 10 + k * 22) * kTilePx - camX + std::sin(float(frame) * 0.03f + float(k) * 2.0f) * 90.0f;
      const float sy = float(b.floorY) * kTilePx - camY;
      if (sx > -400.0f && sx < float(kScreenW) + 400.0f)
        drawGlow(r, art, sx, sy - 80.0f, 300.0f, rgb(255, 236, 190), 0.4f);
    }
  }
}

// --- Behind the runner ---------------------------------------------------------------------------

void World::drawToonBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)alpha;
  const auto& t = mToon;
  const auto& b = t.buddy;
  if (mInk.on)
  {
    // The Inkwell's lines: wobbly strokes of ink, thinning and greying
    // as they fade.
    for (const auto& ib : mInk.blocks)
    {
      const float x = float(ib.x) * kTilePx - camX, y = float(ib.y) * kTilePx - camY;
      if (offScreen(x, y, 64.0f, 64.0f))
        continue;
      const int left = kInkFade - ib.age;
      const float k = left < 90 ? float(left) / 90.0f : 1.0f;
      const std::uint8_t a = std::uint8_t(80 + 175 * k);
      const float j0 = float(hash2(ib.x, ib.y) % 5u) - 2.0f, j1 = float(hash2(ib.x + 1, ib.y) % 5u) - 2.0f;
      const float th = 8.0f + 6.0f * k;
      r.drawLine(x - 2.0f, y + 9.0f + j0, x + 32.0f, y + 9.0f + (j0 + j1) * 0.5f, th, rgba(24, 18, 34, a));
      r.drawLine(x + 32.0f, y + 9.0f + (j0 + j1) * 0.5f, x + 66.0f, y + 9.0f + j1, th, rgba(24, 18, 34, a));
      r.drawLine(x + 6.0f, y + 6.0f + j0, x + 30.0f, y + 6.0f + j0, 2.0f, rgba(140, 120, 190, std::uint8_t(a / 2)));
      if ((ib.x * 7 + ib.y) % 5 == 0)
        r.fillRect(x + 30.0f, y + 12.0f, 5.0f, 10.0f, rgba(24, 18, 34, a)); // a drip
    }
    for (const auto& rf : mInk.refills)
      if (!rf.taken)
      {
        const float x = float(rf.x) * kTilePx + 32.0f - camX, y = float(rf.y + 1) * kTilePx - camY;
        r.draw(refillArt(mArt, r), x, y - 6.0f + std::sin(float(frame) * 0.12f + float(rf.x)) * 6.0f);
      }
    return;
  }
  const int tx0 = std::max(0, int(camX / kTilePx) - 1);
  const int ty0 = std::max(0, int(camY / kTilePx) - 1);
  const int tx1 = std::min(mMap.widthBlocks() - 1, int((camX + float(kScreenW)) / kTilePx) + 1);
  const int ty1 = std::min(mMap.heightBlocks() - 1, int((camY + float(kScreenH)) / kTilePx) + 1);
  auto canvas = [&](int bx, int by) { return b.on && bx >= b.x0 && bx <= b.x1 && by < b.floorY; };
  // The big top's roof and side flaps, painted over its solid blocks, and
  // ink along the earth's open edges.
  for (int ty = ty0; ty <= ty1; ++ty)
    for (int tx = tx0; tx <= tx1; ++tx)
    {
      if (mMap.block(tx, ty) != Tile::Solid)
        continue;
      const float x = float(tx) * kTilePx - camX, y = float(ty) * kTilePx - camY;
      if (canvas(tx, ty))
      {
        r.draw(tentTile(mArt, r, true), x, y);
        continue;
      }
      if (b.on && ty == b.floorY && tx >= b.segX0 && tx < b.segX0 + b.segs * b.segW)
        continue; // the stage's boards
      if (toonDrawsBlock(*this, tx, ty))
        continue; // a piano
      const int seed = tx * 131 + ty;
      if (mMap.block(tx - 1, ty) != Tile::Solid && tx > 0)
        inkEdge(r, x, y - (mMap.block(tx, ty - 1) == Tile::Solid ? 0.0f : 6.0f), x, y + 64.0f, seed);
      if (mMap.block(tx + 1, ty) != Tile::Solid && tx + 1 < mMap.widthBlocks())
        inkEdge(r, x + 64.0f, y - (mMap.block(tx, ty - 1) == Tile::Solid ? 0.0f : 6.0f), x + 64.0f, y + 64.0f, seed + 1);
      if (mMap.block(tx, ty + 1) != Tile::Solid && ty + 1 < mMap.heightBlocks())
        inkEdge(r, x, y + 64.0f, x + 64.0f, y + 64.0f, seed + 2);
    }
  if (b.on)
  {
    // The valance along the roof's edge, inside the tent.
    const float vy = float(b.y0) * kTilePx - camY - 6.0f;
    for (int bx = b.x0; bx <= b.x1; bx += 2)
    {
      const float x = float(bx) * kTilePx - camX;
      if (!offScreen(x, vy, 128.0f, 64.0f))
        r.draw(valance(mArt, r), x, vy);
    }
    // The stage: a board per segment still there.
    for (int s = 0; s < b.segs; ++s)
    {
      if (b.erased[std::size_t(s)] > 0)
        continue;
      const float x = float(b.segX0 + s * b.segW) * kTilePx - camX, y = float(b.floorY) * kTilePx - camY;
      if (!offScreen(x, y - 20.0f, float(b.segW) * kTilePx, 90.0f))
        r.draw(stageBoards(mArt, r, b.segW), x, y);
    }
  }
  // The ink: a black body under a wavy surface, highlights riding it.
  for (const auto& p : t.ponds)
  {
    const float x = float(p.x) * kCellPx - camX, y = float(p.y) * kCellPx - camY;
    const float pw = float(p.w) * kCellPx, ph = float(p.h) * kCellPx;
    if (offScreen(x, y, pw, ph))
      continue;
    r.fillRect(x, y + 30.0f, pw, ph - 30.0f, rgb(20, 14, 22));
    SDL_Rect clip{int(std::floor(x)), int(std::floor(y)) - 4, int(std::ceil(pw)) + 1, int(ph) + 8};
    SDL_RenderSetClipRect(r.sdl(), &clip);
    const float off = std::fmod(float(frame) * 1.5f, 256.0f);
    for (float sx = x - off; sx < x + pw; sx += 256.0f)
      r.draw(inkSurface(mArt, r), sx, y);
    SDL_RenderSetClipRect(r.sdl(), nullptr);
    for (int k = 0; k < p.w / 10; ++k)
    {
      // A slow bubble now and then.
      const int ph2 = (frame + k * 37) % 90;
      if (ph2 > 30)
        continue;
      const float bx = x + float((k * 97 + (frame / 90) * 53) % std::max(1, int(pw) - 40)) + 20.0f;
      const float rr = 4.0f + float(ph2) * 0.3f;
      r.drawLine(bx - rr, y + 22.0f - float(ph2) * 0.2f, bx + rr, y + 22.0f - float(ph2) * 0.2f, 3.0f, rgb(110, 90, 130));
    }
  }
  // Painted tunnels.
  for (std::size_t i = 0; i < t.tunnels.size(); ++i)
  {
    const auto& tn = t.tunnels[i];
    const float x = float(tn.x) * kTilePx - camX, y = float(tn.y) * kTilePx - camY;
    if (offScreen(x, y, 128.0f, 192.0f))
      continue;
    if (t.hole.open && t.hole.tunnel == int(i))
    {
      // Behind the paint: the original programmer art.
      r.draw(retroArt(mArt, r, (frame / 6) % 2), x, y);
      continue;
    }
    if (tn.real)
      for (int k = 0; k < 6; ++k)
        r.draw(mArt.solid[0], x + float(k % 2) * 64.0f, y + float(k / 2) * 64.0f);
    r.draw(tunnelArt(mArt, r, tn.real), x, y);
  }
  // The canyon sign.
  if (t.signX >= 0)
  {
    const float x = float(t.signX) * kTilePx + 32.0f - camX, y = float(t.signY + 1) * kTilePx - camY;
    if (!offScreen(x - 160.0f, y - 220.0f, 320.0f, 220.0f))
      r.draw(canyonSign(mArt, r), x, y);
  }
  // The inkwell on the studio desk (the bonus entrance).
  for (const auto& pr : mProps)
    if (pr.kind == PropKind::BonusDoor)
    {
      const float x = float(pr.x + 2) * kCellPx - camX, y = float(pr.y + 2) * kCellPx - camY;
      if (!offScreen(x - 80.0f, y - 140.0f, 160.0f, 140.0f))
      {
        r.draw(inkwellArt(mArt, r), x, y);
        drawGlow(r, mArt, x, y - 70.0f, 70.0f + 8.0f * std::sin(float(frame) * 0.2f), rgb(160, 140, 255), 0.35f);
      }
    }
  // Buddy's sketch on its easel, the full heal's tonic.
  if (t.prizeX >= 0 && !t.prizeTaken)
  {
    const float x = float(t.prizeX) * kTilePx + 32.0f - camX, y = float(t.prizeY + 1) * kTilePx - camY;
    if (!offScreen(x - 70.0f, y - 170.0f, 140.0f, 170.0f))
    {
      r.draw(sketchArt(mArt, r), x, y);
      if ((frame / 20) % 4 == 0)
        drawGlow(r, mArt, x + 30.0f, y - 140.0f, 30.0f, rgb(255, 250, 200), 0.8f);
    }
  }
  if (t.fullhealX >= 0 && !t.fullhealUsed)
  {
    const float x = float(t.fullhealX) * kTilePx + 32.0f - camX, y = float(t.fullhealY + 1) * kTilePx - camY;
    if (!offScreen(x - 50.0f, y - 140.0f, 100.0f, 140.0f))
    {
      drawGlow(r, mArt, x, y - 60.0f, 80.0f, rgb(255, 140, 160), 0.4f);
      r.draw(tonicArt(mArt, r), x, y - 4.0f - std::abs(std::sin(float(frame) * 0.15f)) * 10.0f);
      r.drawText("FULL HEAL", x, y - 170.0f, {18.0f, kPaper, kInk, true}, Align::Center);
    }
  }
  // The Portable Hole, open in a wall or a floor.
  if (t.hole.open)
  {
    const float x = float(t.hole.x0) * kTilePx - camX, y = float(t.hole.y0) * kTilePx - camY;
    const int w = (t.hole.x1 - t.hole.x0 + 1) * 64, h = (t.hole.y1 - t.hole.y0 + 1) * 64;
    DrawOpts o;
    o.alpha = t.hole.t < 20 && (frame / 3) % 2 ? 0.55f : 1.0f;
    if (t.hole.tunnel < 0)
    {
      if (t.hole.wall)
        r.draw(holeArt(mArt, r, w, h), x + float(w) * 0.5f, y + float(h) * 0.5f, o);
      else
        r.draw(holeArt(mArt, r, w, 40), x + float(w) * 0.5f, y + 20.0f, o);
    }
  }
  // Pianos: the shadow grows, then the piano.
  for (const auto& pn : t.pianos)
  {
    const float x = float(pn.x) * kCellPx - camX;
    const float fy = float(pn.floor) * kCellPx - camY;
    if (offScreen(x, fy - 900.0f, 192.0f, 900.0f))
      continue;
    DrawOpts so;
    if (pn.state == 0)
    {
      const float k = 1.0f - float(pn.t) / float(kPianoTell);
      so.scale = 0.3f + 0.5f * k;
      so.alpha = 0.4f + 0.6f * k;
      r.draw(shadowArt(mArt, r), x + 96.0f, fy, so);
      // Let go of: it hangs a moment in the air where the bird dropped it.
      const float hx = std::min(float(pn.x), float(pn.x) - 10.0f + float(kPianoTell - pn.t)) * kCellPx - camX;
      r.draw(pianoArt(mArt, r, 0), hx, pn.y * kCellPx - camY);
      continue;
    }
    if (pn.state == 1)
    {
      so.scale = 0.85f;
      r.draw(shadowArt(mArt, r), x + 96.0f, fy, so);
      const float y = pn.y * kCellPx - camY;
      for (int k = 0; k < 4; ++k)
        r.drawLine(x + 30.0f + float(k) * 44.0f, y - 70.0f, x + 30.0f + float(k) * 44.0f, y - 18.0f, 3.0f, kInk);
      r.draw(pianoArt(mArt, r, 0), x, y);
      continue;
    }
    DrawOpts po;
    if (pn.t < 24 && (frame / 3) % 2)
      po.alpha = 0.5f;
    r.draw(pianoArt(mArt, r, 1), x, pn.y * kCellPx - camY, po);
  }
}

// --- In front --------------------------------------------------------------------------------------

void World::drawToonFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& t = mToon;
  const auto& p = mPlayer;
  const float px = (float(p.prevX) + float(p.x - p.prevX) * alpha) * kCellPx - camX;
  const float py = (float(p.prevY) + float(p.y - p.prevY) * alpha) * kCellPx - camY;
  // The hole in flight.
  if (t.toss.on)
  {
    DrawOpts o;
    o.angle = float(frame * 40 % 360);
    r.draw(tossArt(mArt, r), t.toss.fx * kCellPx - camX, t.toss.fy * kCellPx - camY, o);
  }
  // Piano Birds' pianos on their ropes; Rubber Goons' punches.
  for (const auto& e : mEnemies)
  {
    if (!e.alive || e.hidden)
      continue;
    const float ex = e.drawX * kCellPx + float(e.w) * kCellPx * 0.5f - camX;
    const float ey = (e.drawY + 1.0f) * kCellPx - camY;
    if (ex < -300.0f || ex > float(kScreenW) + 300.0f)
      continue;
    if (e.kind == EnemyKind::PianoBird && e.cool <= 20)
    {
      const float top = ey + 70.0f;
      r.drawLine(ex - 14.0f, ey - 2.0f, ex - 70.0f, top, 2.5f, kInk);
      r.drawLine(ex + 20.0f, ey - 2.0f, ex + 76.0f, top, 2.5f, kInk);
      r.draw(pianoArt(mArt, r, 0), ex - 96.0f, top);
    }
    if (e.kind == EnemyKind::RubberGoon && e.attach > 0 && e.dive == 0)
    {
      const CellBox b = e.box();
      const float ax = (e.dir > 0 ? float(b.right() + 1) + 3.0f : float(b.left() - 6) + 3.0f) * kCellPx - camX;
      const float ay = float(b.top() + 3) * kCellPx - camY;
      r.draw(styledEnemySprite(mArt, r, mTheme, "goon_arm", 0, 0, 6, 2).get(e.dir), ax, ay);
    }
  }
  // The runner's cartoon business: the "uh-oh" sign, bonk stars, the hat.
  if (p.state != PlayerState::Dying && !p.hidden && !mInk.on)
  {
    const float cx = px + 1.5f * kCellPx, top = py - 4.0f * kCellPx;
    if (t.uhOh > 0)
    {
      DrawOpts o;
      o.scale = std::min(1.0f, 0.4f + float((mCharacterIndex == 2 ? kUhOhNova : kUhOh) - t.uhOh + 1) * 0.2f);
      o.angle = std::sin(float(frame) * 0.9f) * 4.0f;
      r.draw(uhOhArt(mArt, r), cx + float(p.facing) * 22.0f, top + 40.0f, o);
    }
    if (t.bonk > 0)
      for (int i = 0; i < 4; ++i)
      {
        const float a = float(frame) * 0.35f + float(i) * 1.571f;
        DrawOpts o;
        o.scale = 0.7f + 0.3f * std::sin(a);
        o.angle = float(frame * 12 % 360);
        r.draw(starArt(mArt, r), cx + std::cos(a) * 44.0f, top - 6.0f + std::sin(a) * 12.0f, o);
      }
    if (t.tip > 0)
    {
      const float lift = float(std::min(t.tip, 30 - t.tip)) * 2.4f;
      DrawOpts o;
      o.angle = -float(p.facing) * std::min(20.0f, lift);
      r.draw(hatArt(mArt, r), cx + float(p.facing) * 6.0f, top + 6.0f - lift, o);
    }
  }
  // Buddy Boom.
  const auto& b = t.buddy;
  if (b.on)
  {
    // His drawing: an outline being drawn, then solid ink.
    if (b.lineY >= 0)
    {
      const float x = float(b.lineX0) * kTilePx - camX, y = float(b.lineY) * kTilePx - camY;
      const float w = float(b.lineX1 - b.lineX0 + 1) * kTilePx;
      if (b.lineSolid)
      {
        r.draw(lineArt(mArt, r, b.lineX1 - b.lineX0 + 1), x, y);
        for (int k = 0; k < b.dizzyHits; ++k)
        {
          // Cracks where the shots land.
          const float cxk = x + w * 0.5f + float((k * 53) % 300 - 150);
          r.drawLine(cxk, y + 2.0f, cxk + 8.0f, y + 10.0f, 3.0f, kPaper);
          r.drawLine(cxk + 8.0f, y + 10.0f, cxk + 2.0f, y + 20.0f, 3.0f, kPaper);
        }
      }
      else
      {
        const float k = b.state == BuddyState::Draw ? std::min(1.0f, float(b.t + 1) / float(kOutlineT) * 1.3f) : 1.0f;
        const float mid = x + w * 0.5f, half = w * 0.5f * k;
        for (float sx = mid - half; sx < mid + half; sx += 22.0f)
        {
          const float ex = std::min(mid + half, sx + 12.0f);
          r.drawLine(sx, y + 12.0f, ex, y + 12.0f, 4.0f, rgba(60, 56, 70, 220));
        }
        r.drawLine(mid - half, y + 4.0f, mid - half, y + 20.0f, 3.0f, rgba(60, 56, 70, 200));
        r.drawLine(mid + half, y + 4.0f, mid + half, y + 20.0f, 3.0f, rgba(60, 56, 70, 200));
      }
    }
    // The eraser coming down on a floor segment, its shadow growing.
    if (b.state == BuddyState::Erase && b.eraseSeg >= 0)
    {
      const float x = float(b.segX0 + b.eraseSeg * b.segW) * kTilePx - camX, y = float(b.floorY) * kTilePx - camY;
      const float sw = float(b.segW) * kTilePx;
      const float k = std::min(1.0f, float(b.t) / float(kEraseTell));
      DrawOpts so;
      so.scale = 0.6f + 0.6f * k;
      so.alpha = 0.5f + 0.5f * k;
      r.draw(shadowArt(mArt, r), x + sw * 0.5f, y - 2.0f, so);
      const float ey = y - 12.0f - (1.0f - k) * 260.0f + (b.t % 4 < 2 ? 0.0f : 4.0f);
      r.draw(eraserArt(mArt, r), x + sw * 0.5f + std::sin(float(frame) * 0.9f) * 20.0f * k, ey);
      if (k > 0.8f)
        for (int c = 0; c < 6; ++c)
        {
          // Crumbs of rubber.
          const float cx = x + float((c * 47 + frame * 5) % int(sw));
          r.fillRect(cx, y - 8.0f - float((c * 13 + frame * 3) % 20), 6.0f, 4.0f, rgb(240, 150, 166));
        }
    }
    // Shockwaves along the floor.
    for (const auto& wv : b.waves)
    {
      const float x = wv.x * kCellPx - camX, y = float(wv.y + 1) * kCellPx - camY;
      r.draw(waveArt(mArt, r), x, y);
      (void)wv.dir;
    }
    // Cream pies in flight.
    for (const auto& pie : b.pies)
    {
      DrawOpts o;
      o.angle = std::sin(float(frame) * 0.3f) * 10.0f;
      r.draw(pieArt(mArt, r), (pie.x + 1.0f) * kCellPx - camX, (pie.y + 1.0f) * kCellPx - camY, o);
    }
  }
  if (b.on && b.state != BuddyState::Waiting)
  {
    const float bx = (b.prevX + (b.x - b.prevX) * alpha) * kCellPx - camX;
    const float by = (b.prevY + (b.y - b.prevY) * alpha + 1.0f) * kCellPx - camY; // his feet
    const float cx = bx + float(Buddy::kW) * kCellPx * 0.5f;
    int pose = 9, pf = (frame / 8) % 2;
    switch (b.state)
    {
      case BuddyState::Walk:
        pose = b.nose > 0 ? 3 : (int(b.x) / 2) % 2;
        break;
      case BuddyState::Slam:
        pose = b.t < kSlamTell ? 2 : 3;
        break;
      case BuddyState::Spin:
        pose = 4;
        pf = (frame / 2) % 4;
        break;
      case BuddyState::Hop:
        pose = 5;
        break;
      case BuddyState::Pie:
        pose = b.cool > 0 ? 9 : (b.t < 12 ? 6 : 7);
        break;
      case BuddyState::Pencil:
        pose = 8;
        pf = (frame / 3) % 4;
        break;
      case BuddyState::Draw:
        pose = 10;
        break;
      case BuddyState::Stand:
        pose = 9;
        break;
      case BuddyState::Erase:
        pose = 11;
        break;
      case BuddyState::Dizzy:
        pose = 12;
        pf = (frame / 3) % 4;
        break;
      case BuddyState::Fall:
        pose = b.deflate > 0.0f ? 14 : 13;
        pf = b.deflate > 0.0f ? std::min(4, int(b.deflate * 5.0f)) : 0;
        break;
      case BuddyState::Done:
        pose = 14;
        pf = 4;
        break;
      default:
        break;
    }
    // The pencil he rides up on, standing on its point on the stage.
    if (b.state == BuddyState::Pencil)
    {
      const float floorY = float(b.floorY) * kTilePx - camY;
      SDL_Rect clip{0, int(by) - 4, kScreenW, std::max(0, int(floorY - by) + 6)};
      SDL_RenderSetClipRect(r.sdl(), &clip);
      r.draw(pencilArt(mArt, r), cx, floorY);
      SDL_RenderSetClipRect(r.sdl(), nullptr);
    }
    const Texture& tex = styledEnemySprite(mArt, r, mTheme, "buddy_boom", pose, pf, Buddy::kW, Buddy::kH).get(b.dir);
    const bool inInk = b.state == BuddyState::Fall || b.state == BuddyState::Done;
    float sink = 0.0f;
    if (b.state == BuddyState::Done)
      sink = -18.0f; // the empty skin floats
    r.draw(tex, cx, by + sink);
    if (b.flash > 0)
    {
      DrawOpts fo;
      fo.blend = Blend::Add;
      fo.alpha = 0.55f;
      r.draw(tex, cx, by + sink, fo);
    }
    // The nose, red and flashing after a slam.
    if (b.nose > 0)
    {
      const CellBox nose = buddyNose();
      const float nx = (float(nose.x) + 2.0f) * kCellPx - camX, ny = (float(nose.y) + 2.0f) * kCellPx - camY;
      const bool on = (frame / 3) % 2 == 0;
      drawGlow(r, mArt, nx, ny, on ? 60.0f : 40.0f, rgb(255, 60, 60), on ? 0.8f : 0.4f);
      r.draw(noseArt(mArt, r, on), nx, ny);
    }
    if (b.state == BuddyState::Dizzy)
      for (int i = 0; i < 4; ++i)
      {
        const float a = float(frame) * 0.3f + float(i) * 1.571f;
        DrawOpts o;
        o.scale = 0.8f + 0.3f * std::sin(a);
        r.draw(starArt(mArt, r), cx + float(b.dir) * 4.0f + std::cos(a) * 80.0f, by - 380.0f + std::sin(a) * 18.0f, o);
      }
    if (b.state == BuddyState::Pie && b.cool == 0 && b.t < 12)
      drawGlow(r, mArt, cx - float(b.dir) * 70.0f, by - 440.0f, 50.0f, rgb(255, 255, 230), 0.3f);
    // Sinking into the ink: the ink closes over his legs.
    if (inInk)
      for (const auto& pd : t.ponds)
      {
        const float x = float(pd.x) * kCellPx - camX, y = float(pd.y) * kCellPx - camY;
        if (cx > x && cx < x + float(pd.w) * kCellPx)
        {
          const float x0 = std::max(x, cx - 200.0f), x1 = std::min(x + float(pd.w) * kCellPx, cx + 200.0f);
          r.fillRect(x0, y + 30.0f, x1 - x0, float(pd.h) * kCellPx - 30.0f, rgb(20, 14, 22));
          SDL_Rect clip{int(x0), int(y) - 4, int(x1 - x0), 60};
          SDL_RenderSetClipRect(r.sdl(), &clip);
          const float off = std::fmod(float(frame) * 1.5f, 256.0f);
          for (float sx = x - off; sx < x + float(pd.w) * kCellPx; sx += 256.0f)
            r.draw(inkSurface(mArt, r), sx, y);
          SDL_RenderSetClipRect(r.sdl(), nullptr);
        }
      }
  }
  // The Inkwell: the pen at work.
  if (mInk.on && mInk.drawing)
  {
    const float x = (mInk.penX + 0.5f) * kCellPx - camX, y = (mInk.penY + 0.5f) * kCellPx - camY;
    DrawOpts o;
    o.angle = float(std::atan2(float(mInk.penDy), float(mInk.penDx)) * 180.0 / kPi) + 30.0f * float(mInk.penDx);
    r.draw(penArt(mArt, r), x, y, o);
    drawGlow(r, mArt, x, y, 20.0f, rgb(160, 140, 255), 0.4f);
  }
}

// --- HUD ---------------------------------------------------------------------------------------------

void World::drawToonHud(Renderer& r, int frame) const
{
  (void)frame;
  const auto& b = mToon.buddy;
  // A pie in the face: half the screen covered in cream, fading at the end.
  for (int side = 0; side < 2; ++side)
  {
    const int f = side == 0 ? b.whiteL : b.whiteR;
    if (f <= 0)
      continue;
    const std::uint8_t a = std::uint8_t(std::min(255, f * 255 / 12));
    const float half = float(kScreenW) * 0.5f;
    const float drip = float(kPieWhiteT - f) * 3.0f; // the cream sliding down
    DrawOpts o;
    o.alpha = float(a) / 255.0f;
    if (side == 0)
    {
      r.fillRect(0.0f, drip, half, float(kScreenH), rgba(255, 252, 240, a));
      r.draw(splatEdge(mArt, r, false), half, drip, o);
    }
    else
    {
      r.fillRect(half, drip, half, float(kScreenH), rgba(255, 252, 240, a));
      r.draw(splatEdge(mArt, r, true), half - 140.0f, drip, o);
    }
  }
  if (buddyFight())
  {
    // Buddy's bar under the HUD: his head, three thirds (a phase each).
    const float w = 520.0f, x = float(kScreenW) * 0.5f - w * 0.5f + 30.0f, y = 128.0f;
    r.fillRect(x - 64.0f, y - 10.0f, w + 74.0f, 38.0f, kInk);
    r.fillRect(x - 60.0f, y - 6.0f, w + 66.0f, 30.0f, kPaper);
    r.fillRect(x - 2.0f, y - 2.0f, w + 4.0f, 22.0f, kInk);
    const float fill = w * float(buddyHp()) / float(3 * kBuddyHp);
    r.fillRect(x, y, fill, 18.0f, rgb(222, 64, 56));
    r.fillRect(x, y + 2.0f, fill, 4.0f, rgb(250, 140, 120));
    for (int k = 1; k < 3; ++k)
      r.fillRect(x + w * float(k) / 3.0f - 2.0f, y, 4.0f, 18.0f, kInk);
    DrawOpts io;
    io.scale = 0.75f;
    r.draw(buddyIcon(mArt, r), x - 32.0f, y + 9.0f, io);
    r.drawText("BUDDY BOOM", x + w * 0.5f, y + 26.0f, {18.0f, kPaper, kInk, true}, Align::Center);
  }
  if (mInk.on)
  {
    // The INK meter: an inkwell and a glass tube filling with ink.
    const float x = 90.0f, y = float(kScreenH) - 70.0f, w = 300.0f;
    r.draw(inkIcon(mArt, r), 50.0f, y + 10.0f);
    r.fillRect(x - 4.0f, y - 4.0f, w + 8.0f, 32.0f, kInk);
    r.fillRect(x, y, w, 24.0f, kPaper);
    const float fill = w * float(mInk.ink) / float(kInkMax);
    r.fillRect(x, y, fill, 24.0f, rgb(40, 30, 70));
    r.fillRect(x, y + 3.0f, fill, 4.0f, rgb(120, 100, 180));
    r.drawText("INK", x, y - 34.0f, {22.0f, kPaper, kInk, true}, Align::Left);
  }
}

} // namespace gr
