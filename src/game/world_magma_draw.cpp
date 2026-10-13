// Level 33, Magma Mainframe: drawing. See world_magma.cpp for the logic.
//
// Behind the tiles (drawMagmaSet): the near layer of hanging cable bundles
// and pipes, repeating up the climb; the lava's glow welling up behind
// everything; the back aisles of dim server racks on the server floors and
// the cable trays under each floor; the cooling shaft's pipes, frost and
// stencils; the coolant stations at the checkpoints (a tank, its pipe to a
// nozzle over the beacon, the warning light that turns on for the siren);
// the crater's night sky with stars and drifting ash; the mainframe on the
// crater floor, its lights and tape reels; the mooring mast and the airship
// over it; Overclock's speed lines. Behind the runner (drawMagmaBack): the
// floors' steel decks over the rock and the racks standing on them (their
// blue lights blinking), the corrupted rack and its green Virus dust, the
// DO NOT REBOOT rack (its uptime, or its blue reboot screen), the RATINGS
// server, the Fan Bots' gusts, the magma gutter. In front (drawMagmaFront):
// Firewalls (the flicker before a dip, the dip), Lava Daemons (the arm
// winding up, the leap, the marshmallow), Fan Bots (the spin-up), the
// coolant purges' steam, the rising lava (its glowing surface, crust,
// bubbles and heat shimmer; crusted over while a purge holds it), the pop's
// arc out of it, the Heat Sink's cone, Overclock's motion streaks. The HUD
// (drawMagmaHud): the lava gauge, the coolant hold, the reboot screen,
// Overclock's sign.

#include "game/world.hpp"

#include "assets/art.hpp"
#include "assets/enemy_art.hpp"
#include "base/math.hpp"
#include "game/magma_draw.hpp"
#include "render/vector.hpp"

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
constexpr Color kInk = rgb(16, 10, 10);
constexpr Color kHudInk = rgb(10, 8, 20);
constexpr Color kLed = rgb(80, 170, 255);
constexpr Color kLava = rgb(255, 110, 24);
constexpr Color kLavaHot = rgb(255, 214, 110);
constexpr Color kVirus = rgb(120, 255, 80);
constexpr Color kCoolant = rgb(120, 210, 255);

bool offScreen(float x, float y, float w, float h, float m = 96.0f)
{
  return x > float(kScreenW) + m || x + w < -m || y > float(kScreenH) + m || y + h < -m;
}

float unit(std::uint32_t h) { return float(h % 10000u) / 10000.0f; }

float lerpF(int a, int b, float t) { return float(a) + (float(b) - float(a)) * t; }

// A texture baked once and kept in the Art's sprite cache under `key`; the
// mirrored copy too, its anchor mirrored with it.
const Sprite& bakedSprite(const Art& art, const Renderer& r, const std::string& key, int w, int h, float ax, float ay,
  const std::function<void(cairo_t*)>& paint)
{
  const std::string id = "~magma/" + key;
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
  cairo_scale(cr, rx, ry);
  cairo_new_sub_path(cr);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_restore(cr);
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

// A thick line with a dark outline.
void beamLine(Renderer& r, float x0, float y0, float x1, float y1, float w, Color c)
{
  r.drawLine(x0, y0, x1, y1, w + 4.0f, kInk);
  r.drawLine(x0, y0, x1, y1, w, c);
}

// --- Painters: the data centre ---------------------------------------------------------------------

// A floor's steel deck (64 x 24, its top at the block's top): a bright
// edge, raised-floor tiles with their perforations, a gunmetal fascia beam
// with rivets.
void paintDeckTop(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 0, 64, 24);
  lgrad(cr, 0, 0, 0, 24, {{0.0, rgb(150, 156, 170)}, {0.12, rgb(104, 110, 124)}, {0.45, rgb(84, 88, 100)}, {0.5, rgb(40, 42, 52)},
    {1.0, rgb(28, 30, 38)}});
  cairo_rectangle(cr, 0, 0, 64, 2);
  setRgba(cr, rgb(232, 236, 244), 1.0);
  cairo_fill(cr);
  // The floor tiles: seams and perforation rows.
  for (const double x : {0.5, 32.5})
  {
    cairo_move_to(cr, x, 2);
    cairo_line_to(cr, x, 11);
    stroke(cr, rgb(30, 32, 40), 1.0, 0.8);
  }
  for (double x = 4; x < 64; x += 4)
    for (const double y : {5.0, 8.5})
    {
      circle(cr, x + (y > 6 ? 2 : 0), y, 0.8);
      setRgba(cr, rgb(30, 32, 40), 0.8);
      cairo_fill(cr);
    }
  // The fascia, riveted, a hazard tick at each seam.
  cairo_move_to(cr, 0, 12);
  cairo_line_to(cr, 64, 12);
  stroke(cr, rgb(10, 10, 14), 1.4);
  for (double x = 8; x < 64; x += 16)
  {
    circle(cr, x, 17, 1.3);
    setRgba(cr, rgb(170, 176, 190), 0.9);
    cairo_fill(cr);
  }
  cairo_rectangle(cr, 30, 13, 4, 9);
  setRgba(cr, rgb(240, 190, 40), 0.85);
  cairo_fill(cr);
}

// Under a deck with open air below (64 x 40, from 24 down): the I-beam's
// web, a cable tray hanging from it, a strip of blue lights, the bottom
// flange lit by the glow from below.
void paintDeckUnder(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 0, 64, 40);
  lgrad(cr, 0, 0, 0, 40, {{0.0, rgb(24, 24, 32)}, {0.8, rgb(36, 32, 38)}, {1.0, rgb(70, 46, 40)}});
  // Web stiffeners.
  for (const double x : {16.0, 48.0})
  {
    cairo_move_to(cr, x, 0);
    cairo_line_to(cr, x, 34);
    stroke(cr, rgb(60, 62, 74), 2.0, 0.9);
  }
  // The cable tray: a shallow channel, cables sagging in it.
  cairo_rectangle(cr, 0, 14, 64, 8);
  setRgba(cr, rgb(14, 14, 20), 1.0);
  cairo_fill(cr);
  for (int k = 0; k < 3; ++k)
  {
    cairo_move_to(cr, 0, 16 + k * 2);
    cairo_curve_to(cr, 20, 19 + k * 2, 44, 19 + k * 2, 64, 16 + k * 2);
    stroke(cr, k == 1 ? rgb(40, 70, 120) : rgb(50, 50, 60), 1.6);
  }
  cairo_move_to(cr, 0, 22);
  cairo_line_to(cr, 64, 22);
  stroke(cr, rgb(110, 116, 130), 1.4);
  // The light strip's housing (the lights are drawn live).
  cairo_rectangle(cr, 0, 27, 64, 4);
  setRgba(cr, rgb(20, 30, 50), 1.0);
  cairo_fill(cr);
  // The bottom flange, warm with glow.
  cairo_rectangle(cr, 0, 34, 64, 6);
  lgrad(cr, 0, 34, 0, 40, {{0.0, rgb(90, 80, 84)}, {1.0, rgb(220, 110, 50)}});
}

// A server rack's face (64 x 64): four 1U servers, each a bezel with a
// vent grille on the left and drive bays on the right; the lights are drawn
// live. Edges (posts, cap, plinth) are drawn live too.
void paintRackFace(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 0, 64, 64);
  setRgba(cr, rgb(18, 18, 24), 1.0);
  cairo_fill(cr);
  for (int u = 0; u < 4; ++u)
  {
    const double y = u * 16.0;
    cairo_rectangle(cr, 2, y + 1, 60, 14);
    lgrad(cr, 0, y + 1, 0, y + 15, {{0.0, rgb(70, 74, 88)}, {0.15, rgb(46, 48, 60)}, {1.0, rgb(30, 32, 40)}}, 1.0, true);
    stroke(cr, rgb(8, 8, 12), 1.0);
    // The vent grille.
    for (double x = 6; x < 28; x += 3)
    {
      cairo_move_to(cr, x, y + 4);
      cairo_line_to(cr, x, y + 12);
      stroke(cr, rgb(10, 10, 14), 1.4);
    }
    // Drive bays.
    for (double x = 32; x < 58; x += 6.5)
    {
      cairo_rectangle(cr, x, y + 4, 5, 8);
      lgrad(cr, 0, y + 4, 0, y + 12, {{0.0, rgb(90, 96, 110)}, {1.0, rgb(50, 54, 64)}}, 1.0, true);
      stroke(cr, rgb(10, 10, 14), 0.8);
    }
  }
}

// A rack in a back aisle (64 x 64): the same, dim and blued by distance.
void paintRackBack(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 0, 64, 64);
  setRgba(cr, rgb(12, 12, 20), 0.95);
  cairo_fill(cr);
  for (int u = 0; u < 4; ++u)
  {
    const double y = u * 16.0;
    cairo_rectangle(cr, 3, y + 2, 58, 12);
    setRgba(cr, rgb(26, 28, 40), 1.0);
    cairo_fill(cr);
    for (double x = 7; x < 30; x += 4)
    {
      cairo_move_to(cr, x, y + 5);
      cairo_line_to(cr, x, y + 11);
      stroke(cr, rgb(8, 8, 14), 1.2);
    }
  }
  cairo_move_to(cr, 0.5, 0);
  cairo_line_to(cr, 0.5, 64);
  stroke(cr, rgb(4, 4, 8), 1.0);
}

// The near layer (1280 x 1280, wraps both ways): hanging cable bundles in
// silhouette, their loops sagging, a few vertical pipes with flanges, tiny
// amber and blue lights on junction boxes; a warm rim on their lower edges.
void paintNear(cairo_t* cr)
{
  Rng rng(33201u);
  const Color sil = rgb(14, 8, 10);
  // Vertical pipes and conduits.
  for (int i = 0; i < 5; ++i)
  {
    const double x = rng.range(40, 1240), w = rng.range(10, 26);
    cairo_rectangle(cr, x, 0, w, 1280);
    setRgba(cr, sil, 0.92);
    cairo_fill(cr);
    cairo_rectangle(cr, x + w - 2, 0, 2, 1280);
    setRgba(cr, rgb(120, 50, 24), 0.5);
    cairo_fill(cr);
    for (double y = rng.range(0, 200); y < 1280; y += rng.range(180, 320))
    {
      cairo_rectangle(cr, x - 4, y, w + 8, 8);
      setRgba(cr, sil, 1.0);
      cairo_fill(cr);
      if (rng.uniform() < 0.4f)
      {
        // A junction box with a light.
        cairo_rectangle(cr, x - 10, y + 20, w + 20, 26);
        setRgba(cr, sil, 1.0);
        cairo_fill(cr);
        const bool amber = rng.uniform() < 0.3f;
        glow(cr, x + w * 0.5, y + 33, 8, amber ? rgb(255, 170, 60) : kLed, 0.7);
      }
    }
  }
  // Cable bundles hanging in loops, tied off at the top and bottom of the
  // tile so they wrap.
  for (int i = 0; i < 9; ++i)
  {
    const double x = rng.range(0, 1280), sag = rng.range(60, 220), y = rng.range(0, 1280), span = rng.range(160, 380);
    for (int k = 0; k < 3; ++k)
      for (const double ox : {-1280.0, 0.0, 1280.0})
        for (const double oy : {-1280.0, 0.0, 1280.0})
        {
          const double x0 = x + ox, y0 = y + oy + k * 5;
          cairo_move_to(cr, x0, y0);
          cairo_curve_to(cr, x0 + span * 0.25, y0 + sag, x0 + span * 0.75, y0 + sag, x0 + span, y0 + rng.range(-20, 20) * 0.0);
          stroke(cr, sil, 4.0 - k, 0.9);
        }
  }
}

// The heat welling up from the lava (1280 x 256): additive, bright at the
// bottom edge.
void paintHeatGlow(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 0, 1280, 256);
  lgrad(cr, 0, 0, 0, 256, {{0.0, rgb(255, 80, 20)}, {1.0, rgb(255, 120, 40)}}, 1.0);
  // Fade the top: redraw with an alpha mask via a gradient operator.
  cairo_set_operator(cr, CAIRO_OPERATOR_DEST_IN);
  cairo_pattern_t* m = cairo_pattern_create_linear(0, 0, 0, 256);
  cairo_pattern_add_color_stop_rgba(m, 0.0, 0, 0, 0, 0.0);
  cairo_pattern_add_color_stop_rgba(m, 0.6, 0, 0, 0, 0.35);
  cairo_pattern_add_color_stop_rgba(m, 1.0, 0, 0, 0, 1.0);
  cairo_rectangle(cr, 0, 0, 1280, 256);
  cairo_set_source(cr, m);
  cairo_fill(cr);
  cairo_pattern_destroy(m);
  cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
}

// Molten lava (256 x 256, wraps): orange-yellow cells of melt between
// darker cooling veins, a few dark crust flecks.
void paintLavaBody(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 0, 256, 256);
  setRgba(cr, rgb(232, 80, 18), 1.0);
  cairo_fill(cr);
  Rng rng(33211u);
  for (int i = 0; i < 26; ++i)
  {
    const double x = rng.range(0, 256), y = rng.range(0, 256), rx = rng.range(20, 46), ry = rx * rng.range(0.5f, 0.9f);
    for (const double ox : {-256.0, 0.0, 256.0})
      for (const double oy : {-256.0, 0.0, 256.0})
      {
        cairo_save(cr);
        cairo_translate(cr, x + ox, y + oy);
        cairo_scale(cr, rx, ry);
        cairo_pattern_t* g = cairo_pattern_create_radial(0, 0, 0, 0, 0, 1);
        stop(g, 0.0, rgb(255, 220, 120), 0.9);
        stop(g, 0.5, rgb(255, 150, 40), 0.6);
        stop(g, 1.0, rgb(255, 110, 20), 0.0);
        cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
        cairo_set_source(cr, g);
        cairo_fill(cr);
        cairo_pattern_destroy(g);
        cairo_restore(cr);
      }
  }
  // Cooling veins.
  for (int i = 0; i < 14; ++i)
  {
    double x = rng.range(0, 256), y = rng.range(0, 256);
    cairo_move_to(cr, x, y);
    for (int k = 0; k < 5; ++k)
    {
      x += rng.range(-30, 30);
      y += rng.range(-30, 30);
      cairo_line_to(cr, x, y);
    }
    stroke(cr, rgb(150, 30, 10), rng.range(1.5f, 4.0f), 0.5);
  }
}

// A floating plate of crust (96 x 40): black, cracked, glowing at its rim.
void paintCrust(cairo_t* cr)
{
  cairo_move_to(cr, 6, 22);
  cairo_curve_to(cr, 10, 8, 40, 4, 60, 8);
  cairo_curve_to(cr, 84, 10, 94, 18, 88, 28);
  cairo_curve_to(cr, 70, 36, 30, 36, 12, 32);
  cairo_close_path(cr);
  lgrad(cr, 0, 4, 0, 36, {{0.0, rgb(80, 56, 50)}, {0.5, rgb(40, 28, 28)}, {1.0, rgb(20, 12, 12)}}, 1.0, true);
  stroke(cr, rgb(255, 170, 70), 2.0, 0.8);
  cairo_move_to(cr, 30, 10);
  cairo_line_to(cr, 40, 20);
  cairo_line_to(cr, 36, 32);
  cairo_move_to(cr, 40, 20);
  cairo_line_to(cr, 62, 22);
  stroke(cr, rgb(255, 140, 50), 1.4, 0.8);
}

// The Heat Sink's cone (256 x 352, anchor its muzzle at (0, 176)): a
// widening fan of warm, rippling air.
void paintSinkCone(cairo_t* cr)
{
  cairo_move_to(cr, 0, 160);
  cairo_line_to(cr, 256, 0);
  cairo_line_to(cr, 256, 352);
  cairo_line_to(cr, 0, 192);
  cairo_close_path(cr);
  cairo_pattern_t* g = cairo_pattern_create_linear(0, 0, 256, 0);
  stop(g, 0.0, rgb(255, 210, 120), 0.45);
  stop(g, 0.6, rgb(255, 150, 60), 0.18);
  stop(g, 1.0, rgb(255, 120, 40), 0.0);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
  for (int k = 1; k < 6; ++k)
  {
    const double x = k * 44.0, half = 16 + x * 0.62;
    cairo_move_to(cr, x, 176 - half);
    cairo_curve_to(cr, x - 12, 176 - half * 0.4, x - 12, 176 + half * 0.4, x, 176 + half);
    stroke(cr, rgb(255, 230, 170), 2.0, 0.28 - k * 0.04);
  }
}

// The coolant station (240 x 320, anchored at its nozzle's x and the
// floor): a frosty blue tank with a sight glass, a red valve wheel and a
// snowflake placard, its pipe up to the ceiling and across to a nozzle
// pointing down over the beacon. The warning light on the tank is drawn live.
void paintStation(cairo_t* cr)
{
  // The pipe.
  cairo_move_to(cr, 55, 150);
  cairo_line_to(cr, 55, 18);
  cairo_line_to(cr, 200, 18);
  cairo_line_to(cr, 200, 36);
  stroke(cr, kInk, 16.0);
  cairo_move_to(cr, 55, 150);
  cairo_line_to(cr, 55, 18);
  cairo_line_to(cr, 200, 18);
  cairo_line_to(cr, 200, 36);
  stroke(cr, rgb(150, 190, 220), 11.0);
  cairo_move_to(cr, 51, 150);
  cairo_line_to(cr, 51, 14);
  cairo_line_to(cr, 200, 14);
  stroke(cr, rgb(230, 245, 255), 2.4, 0.7);
  for (const double y : {60.0, 110.0})
  {
    cairo_rectangle(cr, 45, y, 20, 6);
    setRgba(cr, rgb(70, 90, 110), 1.0);
    cairo_fill(cr);
  }
  for (const double x : {100.0, 150.0})
  {
    cairo_rectangle(cr, x, 8, 6, 20);
    setRgba(cr, rgb(70, 90, 110), 1.0);
    cairo_fill(cr);
  }
  // The nozzle: a flared cone pointing down.
  cairo_move_to(cr, 190, 34);
  cairo_line_to(cr, 210, 34);
  cairo_line_to(cr, 222, 62);
  cairo_line_to(cr, 178, 62);
  cairo_close_path(cr);
  lgrad(cr, 178, 0, 222, 0, {{0.0, rgb(200, 220, 236)}, {1.0, rgb(80, 100, 124)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  ellipse(cr, 200, 62, 22, 4);
  setRgba(cr, rgb(20, 40, 60), 1.0);
  cairo_fill(cr);
  // The tank.
  cairo_move_to(cr, 18, 170);
  cairo_curve_to(cr, 18, 140, 92, 140, 92, 170);
  cairo_line_to(cr, 92, 308);
  cairo_line_to(cr, 18, 308);
  cairo_close_path(cr);
  lgrad(cr, 18, 0, 92, 0, {{0.0, rgb(150, 200, 236)}, {0.3, rgb(220, 240, 255)}, {0.6, rgb(110, 160, 200)}, {1.0, rgb(40, 70, 100)}}, 1.0,
    true);
  stroke(cr, kInk, 2.6);
  // Frost on it.
  Rng rng(33221u);
  for (int i = 0; i < 18; ++i)
  {
    const double x = rng.range(22, 88), y = rng.range(150, 300);
    circle(cr, x, y, rng.range(1.0f, 2.6f));
    setRgba(cr, rgb(255, 255, 255), 0.5);
    cairo_fill(cr);
  }
  // Bands, the sight glass with coolant in it, the valve wheel.
  for (const double y : {182.0, 290.0})
  {
    cairo_rectangle(cr, 16, y, 78, 6);
    lgrad(cr, 0, y, 0, y + 6, {{0.0, rgb(120, 126, 140)}, {1.0, rgb(50, 54, 66)}}, 1.0, true);
    stroke(cr, kInk, 1.2);
  }
  cairo_rectangle(cr, 26, 200, 10, 80);
  setRgba(cr, rgb(20, 30, 44), 1.0);
  cairo_fill(cr);
  cairo_rectangle(cr, 27, 226, 8, 53);
  lgrad(cr, 27, 0, 35, 0, {{0.0, rgb(140, 230, 255)}, {1.0, rgb(40, 140, 220)}});
  cairo_rectangle(cr, 26, 200, 10, 80);
  stroke(cr, kInk, 1.4);
  // The snowflake placard.
  cairo_move_to(cr, 66, 206);
  cairo_line_to(cr, 82, 222);
  cairo_line_to(cr, 66, 238);
  cairo_line_to(cr, 50, 222);
  cairo_close_path(cr);
  setRgba(cr, rgb(250, 250, 255), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, rgb(40, 110, 200), 1.6);
  for (int k = 0; k < 3; ++k)
  {
    const double a = k * kPi / 3.0;
    cairo_move_to(cr, 66 - std::cos(a) * 9, 222 - std::sin(a) * 9);
    cairo_line_to(cr, 66 + std::cos(a) * 9, 222 + std::sin(a) * 9);
  }
  stroke(cr, rgb(40, 110, 200), 1.8);
  lettering(cr, "COOLANT", 55, 266, 11, rgb(20, 40, 70));
  // The valve wheel on the pipe.
  circle(cr, 55, 132, 11);
  stroke(cr, kInk, 6.0);
  circle(cr, 55, 132, 11);
  stroke(cr, rgb(220, 50, 40), 3.6);
  for (int k = 0; k < 3; ++k)
  {
    const double a = k * kPi / 3.0;
    cairo_move_to(cr, 55 - std::cos(a) * 11, 132 - std::sin(a) * 11);
    cairo_line_to(cr, 55 + std::cos(a) * 11, 132 + std::sin(a) * 11);
  }
  stroke(cr, rgb(180, 40, 30), 2.0);
  // The warning light's housing on top.
  cairo_rectangle(cr, 46, 140, 18, 6);
  setRgba(cr, rgb(40, 42, 52), 1.0);
  cairo_fill(cr);
}

// The cooling shaft's back wall (64 x 256, wraps vertically): a pair of
// big frosty coolant pipes with flanges and a stencilled arrow.
void paintShaftPipes(cairo_t* cr)
{
  for (const double x : {8.0, 36.0})
  {
    cairo_rectangle(cr, x, 0, 20, 256);
    lgrad(cr, x, 0, x + 20, 0, {{0.0, rgb(60, 90, 120)}, {0.35, rgb(130, 170, 200)}, {1.0, rgb(30, 44, 60)}});
    for (const double y : {40.0, 168.0})
    {
      cairo_rectangle(cr, x - 3, y, 26, 10);
      lgrad(cr, 0, y, 0, y + 10, {{0.0, rgb(130, 140, 156)}, {1.0, rgb(50, 54, 66)}}, 1.0, true);
      stroke(cr, kInk, 1.0);
    }
    Rng rng(33231u + std::uint32_t(x));
    for (int i = 0; i < 10; ++i)
    {
      circle(cr, x + rng.range(3, 17), rng.range(0, 256), rng.range(0.8f, 2.0f));
      setRgba(cr, rgb(240, 250, 255), 0.45);
      cairo_fill(cr);
    }
  }
  // An arrow stencil pointing up.
  cairo_move_to(cr, 18, 120);
  cairo_line_to(cr, 18, 96);
  cairo_move_to(cr, 12, 104);
  cairo_line_to(cr, 18, 96);
  cairo_line_to(cr, 24, 104);
  stroke(cr, rgb(240, 200, 60), 2.4, 0.8);
}

// The crater's night sky (1280 x 720): deep blue-black, a red underglow
// from the vent at the bottom edge, an ash plume across it, stars (the
// brightest twinkle live).
void paintCraterSky(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 0, 1280, 720);
  lgrad(cr, 0, 0, 0, 720, {{0.0, rgb(6, 8, 22)}, {0.55, rgb(20, 18, 42)}, {0.85, rgb(70, 30, 34)}, {1.0, rgb(150, 50, 24)}});
  Rng rng(33241u);
  for (int i = 0; i < 160; ++i)
  {
    circle(cr, rng.range(0, 1280), rng.range(0, 480), rng.range(0.5f, 1.4f));
    setRgba(cr, rgb(230, 236, 255), rng.range(0.3f, 0.9f));
    cairo_fill(cr);
  }
  // The ash plume, rising and leaning away to the right.
  for (int i = 0; i < 26; ++i)
  {
    const double u = double(i) / 26.0;
    const double x = 520 + u * 760 + rng.range(-40, 40), y = 640 - u * 520 + rng.range(-30, 30), rad = 90 + u * 120;
    cairo_save(cr);
    cairo_translate(cr, x, y);
    cairo_scale(cr, rad, rad * 0.6);
    cairo_pattern_t* g = cairo_pattern_create_radial(0, 0, 0, 0, 0, 1);
    stop(g, 0.0, rgb(40, 32, 36), 0.45 - u * 0.2);
    stop(g, 1.0, rgb(40, 32, 36), 0.0);
    cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
    cairo_restore(cr);
  }
  // A thin moon.
  circle(cr, 220, 120, 34);
  setRgba(cr, rgb(250, 240, 220), 0.9);
  cairo_fill(cr);
  circle(cr, 234, 112, 32);
  setRgba(cr, rgb(6, 8, 22), 1.0);
  cairo_fill(cr);
}

// The mainframe (1344 x 640, anchored bottom-left on the crater floor at
// block x 20): seven black cabinets in a row under a cornice lettered
// MAGMA MAINFRAME, LED arrays down each, two with tape reels (the reels
// spin live), the middle one a big amber screen.
void paintMainframe(cairo_t* cr)
{
  const double W = 1344, H = 640;
  // The plinth and the cornice.
  cairo_rectangle(cr, 0, H - 24, W, 24);
  lgrad(cr, 0, H - 24, 0, H, {{0.0, rgb(60, 62, 74)}, {1.0, rgb(20, 20, 26)}});
  cairo_rectangle(cr, 0, 40, W, 70);
  lgrad(cr, 0, 40, 0, 110, {{0.0, rgb(80, 84, 98)}, {0.3, rgb(40, 42, 52)}, {1.0, rgb(18, 18, 24)}}, 1.0, true);
  stroke(cr, kInk, 3.0);
  lettering(cr, "MAGMA  MAINFRAME", W * 0.5, 92, 40, rgb(150, 210, 255));
  lettering(cr, "MAGMA  MAINFRAME", W * 0.5, 92, 40, rgb(230, 245, 255), 0.4);
  // The cabinets.
  for (int c = 0; c < 7; ++c)
  {
    const double x = 8 + c * 190.0, w = 176;
    cairo_rectangle(cr, x, 110, w, H - 134);
    lgrad(cr, x, 0, x + w, 0, {{0.0, rgb(44, 46, 58)}, {0.1, rgb(26, 26, 34)}, {0.9, rgb(18, 18, 24)}, {1.0, rgb(8, 8, 12)}}, 1.0, true);
    stroke(cr, kInk, 3.0);
    if (c == 3)
    {
      // The big screen.
      cairo_rectangle(cr, x + 14, 140, w - 28, 180);
      setRgba(cr, rgb(30, 18, 6), 1.0);
      cairo_fill_preserve(cr);
      stroke(cr, rgb(90, 94, 108), 4.0);
      glow(cr, x + w * 0.5, 230, 110, rgb(255, 170, 60), 0.25);
      for (int l = 0; l < 9; ++l)
      {
        cairo_rectangle(cr, x + 26, 156 + l * 18, (l * 37 % 90) + 40, 6);
        setRgba(cr, rgb(255, 190, 80), 0.75);
        cairo_fill(cr);
      }
      // Big switches under it.
      for (int k = 0; k < 6; ++k)
      {
        cairo_rectangle(cr, x + 20 + k * 24, 350, 14, 30);
        lgrad(cr, 0, 350, 0, 380, {{0.0, rgb(220, 60, 40)}, {1.0, rgb(120, 20, 14)}}, 1.0, true);
        stroke(cr, kInk, 1.4);
      }
    }
    else if (c == 1 || c == 5)
    {
      // Tape reel windows (the reels are drawn live).
      for (const double rx : {x + 48, x + 128})
      {
        circle(cr, rx, 200, 38);
        setRgba(cr, rgb(10, 12, 18), 1.0);
        cairo_fill_preserve(cr);
        stroke(cr, rgb(110, 116, 130), 3.0);
      }
      cairo_rectangle(cr, x + 24, 260, w - 48, 30);
      setRgba(cr, rgb(10, 12, 18), 1.0);
      cairo_fill(cr);
    }
    // The LED panel (the lights are drawn live) and vents.
    cairo_rectangle(cr, x + 18, c == 3 ? 400 : 320, w - 36, c == 3 ? 120 : 200);
    setRgba(cr, rgb(10, 12, 20), 1.0);
    cairo_fill(cr);
    for (double y = H - 80; y < H - 30; y += 7)
    {
      cairo_move_to(cr, x + 20, y);
      cairo_line_to(cr, x + w - 20, y);
      stroke(cr, rgb(6, 6, 10), 2.4);
    }
    // A hot rim from the vent behind the floor.
    cairo_move_to(cr, x + 2, H - 26);
    cairo_line_to(cr, x + w - 2, H - 26);
    stroke(cr, rgb(255, 130, 60), 2.0, 0.6);
  }
}

// A tape reel (72 x 72, anchored centre): three holes in a hub, tape wound
// on it.
void paintReel(cairo_t* cr)
{
  circle(cr, 36, 36, 34);
  setRgba(cr, rgb(60, 44, 34), 1.0);
  cairo_fill(cr);
  circle(cr, 36, 36, 34);
  stroke(cr, rgb(200, 204, 214), 2.0);
  circle(cr, 36, 36, 14);
  setRgba(cr, rgb(200, 204, 214), 1.0);
  cairo_fill(cr);
  for (int k = 0; k < 3; ++k)
  {
    const double a = k * 2.0 * kPi / 3.0;
    circle(cr, 36 + std::cos(a) * 8, 36 + std::sin(a) * 8, 3.4);
    setRgba(cr, rgb(20, 20, 26), 1.0);
    cairo_fill(cr);
  }
}

// One section of the mooring mast (192 x 256, wraps vertically): a steel
// lattice tower, red-painted, its cross-bracing lit from below.
void paintMast(cairo_t* cr)
{
  for (const double x : {20.0, 172.0})
  {
    cairo_rectangle(cr, x - 5, 0, 10, 256);
    lgrad(cr, x - 5, 0, x + 5, 0, {{0.0, rgb(220, 90, 70)}, {1.0, rgb(110, 30, 24)}});
  }
  for (double y = 0; y < 256; y += 64)
  {
    cairo_move_to(cr, 20, y);
    cairo_line_to(cr, 172, y + 64);
    cairo_move_to(cr, 172, y);
    cairo_line_to(cr, 20, y + 64);
    stroke(cr, rgb(150, 50, 40), 5.0);
    cairo_move_to(cr, 15, y);
    cairo_line_to(cr, 177, y);
    stroke(cr, rgb(170, 60, 46), 6.0);
    cairo_move_to(cr, 15, y + 3);
    cairo_line_to(cr, 177, y + 3);
    stroke(cr, rgb(255, 150, 90), 1.4, 0.5);
  }
}

// The airship (1600 x 560, anchored at its gondola's hatch, bottom
// middle): a long silver envelope with panel seams and a red band, fins,
// the gondola below it with lit windows.
void paintAirship(cairo_t* cr)
{
  const double cx = 900, cy = 250, rx = 760, ry = 220;
  // Fins at the tail (left).
  cairo_move_to(cr, 280, 200);
  cairo_line_to(cr, 120, 60);
  cairo_line_to(cr, 60, 70);
  cairo_line_to(cr, 200, 250);
  cairo_close_path(cr);
  cairo_move_to(cr, 280, 300);
  cairo_line_to(cr, 120, 440);
  cairo_line_to(cr, 60, 430);
  cairo_line_to(cr, 200, 250);
  cairo_close_path(cr);
  lgrad(cr, 0, 60, 0, 440, {{0.0, rgb(200, 70, 56)}, {1.0, rgb(110, 30, 26)}}, 1.0, true);
  stroke(cr, kInk, 3.0);
  // The envelope.
  ellipse(cr, cx, cy, rx, ry);
  lgrad(cr, 0, cy - ry, 0, cy + ry, {{0.0, rgb(230, 232, 240)}, {0.35, rgb(180, 184, 196)}, {0.8, rgb(90, 80, 86)}, {1.0, rgb(150, 70, 50)}}, 1.0,
    true);
  stroke(cr, kInk, 4.0);
  cairo_save(cr);
  ellipse(cr, cx, cy, rx, ry);
  cairo_clip(cr);
  for (int k = 1; k < 12; ++k)
  {
    const double x = cx - rx + k * rx * 2 / 12;
    cairo_move_to(cr, x, cy - ry);
    cairo_curve_to(cr, x + (x - cx) * 0.15, cy - ry * 0.3, x + (x - cx) * 0.15, cy + ry * 0.3, x, cy + ry);
    stroke(cr, rgb(110, 110, 124), 2.0, 0.5);
  }
  cairo_rectangle(cr, cx - rx, cy + 30, rx * 2, 34);
  setRgba(cr, rgb(200, 50, 40), 0.9);
  cairo_fill(cr);
  lettering(cr, "GUNRUNNER ONE", cx + 60, cy + 58, 30, rgb(255, 236, 200));
  cairo_restore(cr);
  // The gondola.
  cairo_move_to(cr, 1000, 450);
  cairo_line_to(cr, 1240, 450);
  cairo_curve_to(cr, 1250, 500, 1230, 540, 1200, 560);
  cairo_line_to(cr, 1030, 560);
  cairo_curve_to(cr, 1004, 540, 994, 500, 1000, 450);
  cairo_close_path(cr);
  lgrad(cr, 0, 450, 0, 560, {{0.0, rgb(150, 50, 40)}, {1.0, rgb(70, 20, 18)}}, 1.0, true);
  stroke(cr, kInk, 3.0);
  for (int k = 0; k < 6; ++k)
  {
    cairo_rectangle(cr, 1020 + k * 34, 476, 20, 22);
    setRgba(cr, rgb(255, 214, 130), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, rgb(230, 190, 90), 1.6);
  }
  for (const double x : {1010.0, 1230.0})
  {
    cairo_move_to(cr, x, 450);
    cairo_line_to(cr, x + (x < 1100 ? -40 : 40), 420);
    stroke(cr, rgb(40, 30, 30), 3.0);
  }
}

// Painters: the props --------------------------------------------------------------------------------

// The corrupted rack (128 x 192, facing left, anchored bottom middle): a
// cabinet split down its side, a cracked screen glitching green, a fan
// grille on its left side, green ooze dripping. Dead: dark, scorched.
void paintVirusRack(cairo_t* cr, bool dead)
{
  cairo_rectangle(cr, 18, 6, 100, 184);
  lgrad(cr, 18, 0, 118, 0, {{0.0, rgb(40, 50, 44)}, {1.0, rgb(14, 18, 16)}}, 1.0, true);
  stroke(cr, kInk, 3.0);
  // The crack.
  cairo_move_to(cr, 80, 6);
  cairo_line_to(cr, 70, 40);
  cairo_line_to(cr, 86, 70);
  cairo_line_to(cr, 72, 110);
  stroke(cr, dead ? rgb(10, 10, 10) : kVirus, 3.0, dead ? 1.0 : 0.8);
  // The screen.
  cairo_rectangle(cr, 40, 24, 64, 46);
  setRgba(cr, dead ? rgb(10, 12, 10) : rgb(10, 40, 14), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, rgb(90, 96, 100), 2.4);
  if (!dead)
  {
    for (int l = 0; l < 5; ++l)
    {
      cairo_rectangle(cr, 46 + (l * 13) % 20, 30 + l * 8, 24 + (l * 17) % 30, 4);
      setRgba(cr, kVirus, 0.8);
      cairo_fill(cr);
    }
    lettering(cr, "ERR", 72, 66, 11, rgb(200, 255, 170));
  }
  // Servers below.
  for (int u = 0; u < 6; ++u)
  {
    const double y = 84 + u * 16.0;
    cairo_rectangle(cr, 30, y, 80, 13);
    setRgba(cr, dead ? rgb(24, 24, 24) : rgb(34, 44, 38), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.0);
  }
  // The fan grille on its left side.
  circle(cr, 20, 120, 22);
  setRgba(cr, rgb(20, 24, 22), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 2.4);
  // Ooze.
  if (!dead)
    for (const double x : {44.0, 70.0, 96.0})
    {
      cairo_move_to(cr, x - 4, 84);
      cairo_curve_to(cr, x - 4, 96, x + 4, 96, x + 4, 84);
      setRgba(cr, kVirus, 0.85);
      cairo_fill(cr);
    }
  else
    for (int k = 0; k < 4; ++k)
      glow(cr, 40 + k * 22, 60 + k * 30, 18, rgb(0, 0, 0), 0.6);
}

// The DO NOT REBOOT rack (128 x 192, anchored bottom middle): an old beige
// cabinet, a hand-written sign taped across it, a small screen (drawn live:
// its uptime, or the blue reboot screen) and a brass plaque.
void paintRebootRack(cairo_t* cr)
{
  cairo_rectangle(cr, 10, 6, 108, 184);
  lgrad(cr, 10, 0, 118, 0, {{0.0, rgb(220, 210, 180)}, {0.6, rgb(190, 178, 146)}, {1.0, rgb(130, 120, 96)}}, 1.0, true);
  stroke(cr, kInk, 3.0);
  // Drive slots and a ventilation grille.
  for (int u = 0; u < 4; ++u)
  {
    cairo_rectangle(cr, 22, 104 + u * 18, 84, 12);
    setRgba(cr, rgb(150, 140, 112), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, rgb(90, 80, 60), 1.0);
  }
  // The screen's bezel.
  cairo_rectangle(cr, 24, 18, 80, 50);
  setRgba(cr, rgb(40, 38, 34), 1.0);
  cairo_fill(cr);
  // The sign, taped on at a slant.
  cairo_save(cr);
  cairo_translate(cr, 64, 86);
  cairo_rotate(cr, -0.06);
  cairo_rectangle(cr, -54, -14, 108, 28);
  setRgba(cr, rgb(252, 250, 240), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, rgb(120, 110, 90), 1.0);
  lettering(cr, "DO NOT REBOOT", 0, 6, 14, rgb(210, 30, 30));
  for (const double x : {-54.0, 54.0})
  {
    cairo_rectangle(cr, x - 6, -18, 12, 8);
    setRgba(cr, rgb(240, 230, 170), 0.8);
    cairo_fill(cr);
  }
  cairo_restore(cr);
  // A power switch with a guard over it.
  cairo_rectangle(cr, 96, 172, 12, 10);
  setRgba(cr, rgb(200, 40, 30), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.0);
}

// The RATINGS server (128 x 192, anchored bottom middle): a black server
// with gold trim, RATINGS lettered on it, a bar-graph display (live) and
// a "(DO NOT TOUCH)" plate. Shot: cracked, its door hanging open.
void paintRatings(cairo_t* cr, bool shot)
{
  cairo_rectangle(cr, 14, 10, 100, 180);
  lgrad(cr, 14, 0, 114, 0, {{0.0, rgb(40, 36, 30)}, {1.0, rgb(12, 10, 8)}}, 1.0, true);
  stroke(cr, kInk, 3.0);
  cairo_rectangle(cr, 18, 14, 92, 172);
  stroke(cr, rgb(230, 180, 60), 2.4);
  lettering(cr, "RATINGS", 64, 40, 18, rgb(255, 210, 90));
  // The display.
  cairo_rectangle(cr, 28, 52, 72, 56);
  setRgba(cr, rgb(6, 10, 20), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, rgb(120, 110, 80), 2.0);
  // The plate.
  cairo_rectangle(cr, 26, 150, 76, 20);
  lgrad(cr, 0, 150, 0, 170, {{0.0, rgb(255, 226, 130)}, {1.0, rgb(180, 120, 30)}}, 1.0, true);
  stroke(cr, kInk, 1.2);
  lettering(cr, "(DO NOT TOUCH)", 64, 164, 9.5, rgb(60, 30, 10));
  if (shot)
  {
    cairo_move_to(cr, 40, 56);
    cairo_line_to(cr, 64, 80);
    cairo_line_to(cr, 58, 104);
    cairo_move_to(cr, 64, 80);
    cairo_line_to(cr, 92, 70);
    stroke(cr, rgb(200, 220, 255), 1.6);
    // The door hanging open.
    cairo_move_to(cr, 14, 116);
    cairo_line_to(cr, -6, 124);
    cairo_line_to(cr, -6, 188);
    cairo_line_to(cr, 14, 186);
    cairo_close_path(cr);
    setRgba(cr, rgb(30, 26, 22), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, rgb(230, 180, 60), 1.6);
    cairo_rectangle(cr, 18, 116, 92, 30);
    setRgba(cr, rgb(4, 4, 4), 1.0);
    cairo_fill(cr);
  }
  else
    for (int u = 0; u < 2; ++u)
    {
      cairo_rectangle(cr, 26, 116 + u * 15, 76, 11);
      setRgba(cr, rgb(30, 28, 24), 1.0);
      cairo_fill_preserve(cr);
      stroke(cr, rgb(120, 100, 50), 0.8);
    }
}

// The Heat Sink (64 x 64): a block of aluminium fins on a grip, an intake
// funnel at the muzzle, a hot core window in its side.
void paintSink(cairo_t* cr)
{
  glow(cr, 34, 30, 30, rgb(255, 150, 60), 0.25);
  // The grip.
  cairo_move_to(cr, 16, 38);
  cairo_line_to(cr, 28, 38);
  cairo_line_to(cr, 24, 58);
  cairo_line_to(cr, 12, 58);
  cairo_close_path(cr);
  lgrad(cr, 12, 0, 28, 0, {{0.0, rgb(70, 60, 56)}, {1.0, rgb(30, 26, 26)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  // The fin block.
  cairo_rectangle(cr, 6, 18, 36, 22);
  lgrad(cr, 0, 18, 0, 40, {{0.0, rgb(210, 214, 224)}, {1.0, rgb(110, 116, 130)}}, 1.0, true);
  stroke(cr, kInk, 2.2);
  for (double x = 10; x < 40; x += 4)
  {
    cairo_move_to(cr, x, 10);
    cairo_line_to(cr, x, 18);
    stroke(cr, kInk, 3.2);
    cairo_move_to(cr, x, 10.5);
    cairo_line_to(cr, x, 18);
    stroke(cr, rgb(200, 206, 218), 1.6);
  }
  // The core window.
  cairo_rectangle(cr, 14, 24, 20, 10);
  lgrad(cr, 0, 24, 0, 34, {{0.0, rgb(255, 220, 120)}, {1.0, rgb(255, 100, 30)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  // The intake funnel.
  cairo_move_to(cr, 42, 22);
  cairo_line_to(cr, 60, 12);
  cairo_line_to(cr, 60, 48);
  cairo_line_to(cr, 42, 36);
  cairo_close_path(cr);
  lgrad(cr, 42, 0, 60, 0, {{0.0, rgb(180, 120, 60)}, {1.0, rgb(240, 190, 110)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  ellipse(cr, 60, 30, 3, 18);
  setRgba(cr, rgb(40, 16, 8), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.4);
}

// Overclock's sign (360 x 84, anchored mid): a chip, pins down both long
// sides, OVERCLOCK on it.
void paintOverclockSign(cairo_t* cr)
{
  for (double x = 24; x < 340; x += 16)
  {
    cairo_rectangle(cr, x, 2, 6, 12);
    cairo_rectangle(cr, x, 70, 6, 12);
  }
  setRgba(cr, rgb(220, 200, 120), 1.0);
  cairo_fill(cr);
  cairo_rectangle(cr, 10, 12, 340, 60);
  lgrad(cr, 0, 12, 0, 72, {{0.0, rgb(50, 54, 66)}, {1.0, rgb(16, 16, 22)}}, 1.0, true);
  stroke(cr, kInk, 3.0);
  cairo_rectangle(cr, 16, 18, 328, 48);
  stroke(cr, rgb(255, 90, 40), 2.0);
  lettering(cr, "OVERCLOCK", 180, 56, 38, rgb(255, 230, 140));
}

// The lava gauge's tube (64 x 300, anchored top-left): a glass tube in a
// steel frame, a scale down its side.
void paintGauge(cairo_t* cr)
{
  cairo_rectangle(cr, 4, 4, 56, 292);
  lgrad(cr, 4, 0, 60, 0, {{0.0, rgb(110, 116, 130)}, {0.5, rgb(60, 64, 76)}, {1.0, rgb(30, 32, 40)}}, 1.0, true);
  stroke(cr, kHudInk, 3.0);
  cairo_rectangle(cr, 16, 14, 24, 272);
  setRgba(cr, rgb(10, 10, 16), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, rgb(200, 206, 220), 1.6);
  for (int k = 0; k <= 10; ++k)
  {
    const double y = 14 + k * 27.2;
    cairo_move_to(cr, 42, y);
    cairo_line_to(cr, k % 5 == 0 ? 54 : 49, y);
    stroke(cr, rgb(220, 224, 236), 1.4);
  }
}

// --- Level 33's layout -------------------------------------------------------------------------------

// What a solid block is, for drawing: rock (left as basalt), a floor's
// steel deck, or a server rack.
enum class Mass
{
  None,
  Rock,
  Deck,
  Rack,
};

bool solid(const CollisionMap& m, int bx, int by) { return m.block(bx, by) == Tile::Solid; }

// A rack: a short run of solid (4 blocks or fewer) standing on solid.
bool rackAt(const CollisionMap& m, int lw, int bx, int by)
{
  if (!solid(m, bx, by) || bx <= 1 || bx >= lw - 2 || !solid(m, bx, by + 1))
    return false;
  int l = bx, r = bx;
  while (l > 0 && solid(m, l - 1, by) && r - l < 5)
    --l;
  while (r < lw - 1 && solid(m, r + 1, by) && r - l < 5)
    ++r;
  return r - l + 1 <= 4;
}

Mass massAt(const CollisionMap& m, int lw, int lh, int bx, int by)
{
  if (bx < 0 || by < 0 || bx >= lw || by >= lh || !solid(m, bx, by))
    return Mass::None;
  if (bx <= 1 || bx >= lw - 2)
    return Mass::Rock;
  if (rackAt(m, lw, bx, by))
    return Mass::Rack;
  if (by == 0 || !solid(m, bx, by - 1) || rackAt(m, lw, bx, by - 1))
    return Mass::Deck;
  return Mass::Rock;
}

// The main level (not the Overclock bonus): its regions, in blocks.
bool isMainframe(const World& w) { return w.magma().rising && w.level().height >= 140; }
constexpr int kCraterFloor = 28;  // the crater floor's row
constexpr int kShaftTop = 34, kShaftBottom = 73; // the cooling shaft's rows
constexpr int kShaftX0 = 14, kShaftX1 = 45;
constexpr int kServerTop = 74; // the server floors below this row

// A rack's light (k 0..7 in block bx, by): blue mostly, now and then green
// or amber; blinking at its own rate.
Color ledColor(int bx, int by, int k, int t, bool& on)
{
  const std::uint32_t h = hash2(bx * 13 + k, by * 7 + 3);
  const int rate = 6 + int(h % 23u);
  on = (hash2(int(h % 977u), (t + int(h % 61u)) / rate) % 5u) != 0u;
  const std::uint32_t c = h % 17u;
  return c == 0u ? rgb(255, 180, 60) : (c < 3u ? rgb(90, 255, 140) : kLed);
}

int animClock(const World& w, int frame) { return w.magma().overclock ? frame * 3 : frame; }

} // namespace

// --- Behind the tiles --------------------------------------------------------------------------------

void drawMagmaSet(Renderer& r, const World& w, const Art& art, float camX, float camY, int frame, float alpha)
{
  (void)alpha;
  const auto& s = w.magma();
  const auto& m = w.map();
  const int lw = w.level().width, lh = w.level().height;
  const bool main = isMainframe(w);
  const int t = animClock(w, frame);
  const int bx0 = std::max(0, int(camX / kTilePx) - 1), bx1 = std::min(lw - 1, int((camX + float(kScreenW)) / kTilePx) + 1);
  const int by0 = std::max(0, int(camY / kTilePx) - 1), by1 = std::min(lh - 1, int((camY + float(kScreenH)) / kTilePx) + 1);
  const float craterY = float(kCraterFloor) * kTilePx - camY;

  // The crater's night sky, above the crater floor.
  if (main && craterY > -80.0f)
  {
    const float skyBottom = craterY + 40.0f;
    const float top = skyBottom - 720.0f;
    if (top > 0.0f)
      r.fillRect(0, 0, float(kScreenW), top + 2.0f, rgb(6, 8, 22));
    r.draw(baked(art, r, "crater_sky", 1280, 720, 0, 0, paintCraterSky), -std::fmod(camX * 0.05f, 1.0f), top);
    // The brightest stars twinkle.
    for (int i = 0; i < 18; ++i)
    {
      const float sx = unit(hash2(i, 3301)) * 1280.0f, sy = top + unit(hash2(i, 3302)) * 420.0f;
      if ((hash2(i, frame / 9) % 4u) == 0u)
        drawGlow(r, art, sx, sy, 7.0f, rgb(220, 230, 255), 0.8f);
    }
    // Ash drifting across the crater.
    for (int i = 0; i < 40; ++i)
    {
      const float u = unit(hash2(i, 3303));
      const float ax = std::fmod(u * 1800.0f + float(t) * (0.4f + u * 0.6f), 1400.0f) - 60.0f;
      const float ay = top + std::fmod(unit(hash2(i, 3304)) * 760.0f - float(t) * 0.25f + 2000.0f, 760.0f);
      r.fillRect(ax, ay, 2.0f + u * 2.0f, 2.0f + u * 2.0f, rgba(150, 140, 140, 90 + int(u * 80)));
    }
  }

  // The near layer: hanging cables and pipes, repeating up the climb.
  {
    const Texture& nearTex = baked(art, r, "near", 1280, 1280, 0, 0, paintNear);
    float ox = std::fmod(camX * 0.3f, 1280.0f), oy = std::fmod(camY * 0.3f, 1280.0f);
    if (ox < 0.0f)
      ox += 1280.0f;
    if (oy < 0.0f)
      oy += 1280.0f;
    DrawOpts no;
    no.alpha = main && craterY > 0.0f ? std::clamp(1.0f - craterY / 900.0f, 0.0f, 1.0f) : 1.0f;
    if (no.alpha > 0.02f)
      for (float x = -ox; x < float(kScreenW); x += 1280.0f)
        for (float y = -oy; y < float(kScreenH); y += 1280.0f)
          r.draw(nearTex, x, y, no);
  }

  // The lava's glow welling up behind everything.
  if (s.rising)
  {
    const float surf = float(s.surface) * kCellPx - camY;
    if (surf < float(kScreenH) + 300.0f)
    {
      const float crust = s.pause > 0 && s.delay == 0 ? 0.45f : 1.0f;
      DrawOpts go;
      go.blend = Blend::Add;
      go.alpha = (0.55f + 0.08f * std::sin(float(t) * 0.07f)) * crust;
      const Texture& g = baked(art, r, "heat_glow", 1280, 256, 0, 256, paintHeatGlow);
      DrawOpts big = go;
      big.scale = 2.0f;
      r.draw(g, -float(kScreenW) * 0.5f, surf + 20.0f, big);
    }
  }

  // The back aisles: dim racks along each server floor, their lights.
  {
    const Texture& back = baked(art, r, "rack_back", 64, 64, 0, 0, paintRackBack);
    for (int by = by0; by <= by1; ++by)
    {
      if (main && by < kServerTop)
        continue;
      for (int bx = std::max(2, bx0); bx <= std::min(lw - 3, bx1); ++bx)
      {
        if (massAt(m, lw, lh, bx, by) != Mass::Deck)
          continue;
        // Only under open air: the three blocks above.
        bool open = true;
        for (int k = 1; k <= 4; ++k)
          open = open && by - k >= 0 && m.block(bx, by - k) != Tile::Solid;
        if (!open)
          continue;
        const float x = float(bx) * kTilePx - camX;
        for (int k = 1; k <= 3; ++k)
        {
          const float y = float(by - k) * kTilePx - camY;
          r.draw(back, x, y);
          for (int l = 0; l < 4; ++l)
          {
            bool on = false;
            const Color c = ledColor(bx, by - k, l, t / 4, on);
            if (on)
              r.fillRect(x + 36.0f + float(l % 2) * 12.0f, y + 6.0f + float(l) * 16.0f - float(l % 2) * 2.0f, 3.0f, 2.0f,
                lerpColor(c, rgb(10, 10, 20), 0.35f));
          }
        }
        // The aisle's ceiling: a light fitting under the deck above.
        if ((bx % 4) == 1)
        {
          const float ly = float(by - 5) * kTilePx - camY + 4.0f;
          drawGlow(r, art, x + 32.0f, ly + 70.0f, 70.0f, kLed, 0.12f);
        }
      }
    }
  }

  // The cooling shaft's back wall: coolant pipes, a stencil.
  if (main)
  {
    const float top = float(kShaftTop) * kTilePx - camY, bottom = float(kShaftBottom + 1) * kTilePx - camY;
    if (bottom > 0.0f && top < float(kScreenH))
    {
      const Texture& pipes = baked(art, r, "shaft_pipes", 64, 256, 0, 0, paintShaftPipes);
      for (const int bx : {kShaftX0 + 3, kShaftX0 + 15, kShaftX1 - 5})
      {
        const float x = float(bx) * kTilePx - camX;
        if (offScreen(x, 0, 64, 10))
          continue;
        for (float y = top; y < bottom; y += 256.0f)
        {
          if (y > float(kScreenH) || y + 256.0f < 0.0f)
            continue;
          DrawOpts po;
          po.alpha = 0.8f;
          r.draw(pipes, x, y, po);
        }
      }
      const float sx = float(kShaftX0 + 9) * kTilePx - camX;
      for (int k = 0; k < 3; ++k)
      {
        const float sy = float(kShaftBottom - 6 - k * 14) * kTilePx - camY;
        if (sy > -60.0f && sy < float(kScreenH) + 20.0f)
          r.drawText("COOLING SHAFT " + std::to_string(3 - k), sx, sy, {22.0f, rgba(240, 200, 60, 120), 0}, Align::Left);
      }
    }
  }

  // The coolant stations at the checkpoints.
  for (const auto& pu : s.purges)
  {
    const float nx = float(pu.x + 1) * kCellPx - camX, fy = float(pu.y + 1) * kCellPx - camY;
    if (offScreen(nx - 200.0f, fy - 320.0f, 240.0f, 320.0f))
      continue;
    r.draw(baked(art, r, "station", 240, 320, 200, 320, paintStation), nx, fy);
    // The warning light: amber at rest, red and turning for the siren,
    // blue while the purge holds the lava.
    const float lx = nx - 145.0f, ly = fy - 184.0f;
    const bool siren = s.rising && s.pause > 0 && s.pause <= kSirenFrames && s.delay == 0;
    const bool holding = s.rising && s.pause > kSirenFrames && s.delay == 0;
    Color lc = rgb(255, 170, 60);
    if (siren)
      lc = rgb(255, 40, 30);
    else if (holding)
      lc = kCoolant;
    r.fillRect(lx - 7.0f, ly - 8.0f, 14.0f, 10.0f, lc);
    if (siren)
    {
      const float a = float(frame) * 0.35f;
      drawGlow(r, art, lx + std::cos(a) * 30.0f, ly - 4.0f, 60.0f, lc, 0.6f);
      r.drawLine(lx, ly - 4.0f, lx + std::cos(a) * 160.0f, ly - 4.0f + std::sin(a) * 30.0f, 10.0f, rgba(255, 60, 40, 60), Blend::Add);
    }
    drawGlow(r, art, lx, ly - 4.0f, 22.0f, lc, siren ? 0.9f : 0.5f);
  }

  // The crater: the mainframe, the mast, the airship.
  if (main && craterY > -40.0f)
  {
    const float fx = 20.0f * kTilePx - camX;
    if (!offScreen(fx, craterY - 640.0f, 1344.0f, 640.0f))
    {
      r.draw(baked(art, r, "mainframe", 1344, 640, 0, 640, paintMainframe), fx, craterY);
      // The reels turning (one fast, one slow, by turns).
      const Texture& reel = baked(art, r, "reel", 72, 72, 36, 36, paintReel);
      for (const int c : {1, 5})
        for (int k = 0; k < 2; ++k)
        {
          DrawOpts ro;
          ro.angle = float((t * (k == 0 ? 3 : 5) + c * 40) % 360) * ((c + k) % 2 ? 1.0f : -1.0f);
          r.draw(reel, fx + 8.0f + float(c) * 190.0f + (k == 0 ? 48.0f : 128.0f), craterY - 640.0f + 200.0f, ro);
        }
      // The LED panels.
      for (int c = 0; c < 7; ++c)
      {
        const float px = fx + 8.0f + float(c) * 190.0f + 22.0f, py = craterY - 640.0f + (c == 3 ? 404.0f : 324.0f);
        const int rows = c == 3 ? 14 : 24;
        for (int row = 0; row < rows; ++row)
          for (int col = 0; col < 16; ++col)
          {
            bool on = false;
            const Color lc = ledColor(c * 31 + col, row + 500, 0, t / 3, on);
            if (on)
              r.fillRect(px + float(col) * 8.5f, py + float(row) * 8.0f, 4.0f, 3.0f, lc);
          }
      }
      // The screen's scrolling text cursor.
      if ((frame / 15) % 2 == 0)
        r.fillRect(fx + 8.0f + 3.0f * 190.0f + 30.0f, craterY - 640.0f + 156.0f + float((t / 20) % 9) * 18.0f, 10.0f, 6.0f,
          rgb(255, 230, 150));
    }
    // The mooring mast up to the mast top (rows 12 to 28, around x 45).
    {
      const float mx = 44.0f * kTilePx - camX - 32.0f, top = 12.0f * kTilePx - camY;
      const Texture& mast = baked(art, r, "mast", 192, 256, 0, 0, paintMast);
      for (float y = top; y < craterY; y += 256.0f)
        if (y < float(kScreenH) && y + 256.0f > 0.0f)
          r.draw(mast, mx, y);
      if ((frame / 20) % 2 == 0)
        for (const float ax : {mx + 20.0f, mx + 172.0f})
          drawGlow(r, art, ax, top - 6.0f, 26.0f, rgb(255, 50, 40), 0.9f);
    }
    // The airship, moored over the mast top, bobbing a little.
    {
      const float hx = 54.5f * kTilePx - camX, hy = 9.6f * kTilePx - camY + std::sin(float(t) * 0.03f) * 4.0f;
      if (!offScreen(hx - 1200.0f, hy - 560.0f, 1600.0f, 600.0f))
      {
        r.draw(baked(art, r, "airship", 1600, 560, 1120, 560, paintAirship), hx, hy);
        // Mooring lines to the mast top.
        r.drawLine(hx - 420.0f, hy - 80.0f, 46.0f * kTilePx - camX, 12.0f * kTilePx - camY, 2.0f, rgb(40, 30, 30));
        r.drawLine(hx - 300.0f, hy - 40.0f, 46.0f * kTilePx - camX, 12.0f * kTilePx - camY, 2.0f, rgb(40, 30, 30));
        if ((frame / 10) % 3 == 0)
          drawGlow(r, art, hx - 1050.0f, hy - 300.0f, 30.0f, rgb(255, 60, 50), 0.8f);
      }
    }
  }
}

// --- Behind the runner -------------------------------------------------------------------------------

void World::drawMagmaBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)alpha;
  const auto& s = mMagma;
  const int lw = mLevel->width, lh = mLevel->height;
  const int t = animClock(*this, frame);
  const int bx0 = std::max(0, int(camX / kTilePx) - 1), bx1 = std::min(lw - 1, int((camX + float(kScreenW)) / kTilePx) + 1);
  const int by0 = std::max(0, int(camY / kTilePx) - 1), by1 = std::min(lh - 1, int((camY + float(kScreenH)) / kTilePx) + 1);

  // The floors' steel decks and the racks standing on them.
  const Texture& deckTop = baked(mArt, r, "deck_top", 64, 24, 0, 0, paintDeckTop);
  const Texture& deckUnder = baked(mArt, r, "deck_under", 64, 40, 0, 0, paintDeckUnder);
  const Texture& rackFace = baked(mArt, r, "rack_face", 64, 64, 0, 0, paintRackFace);
  for (int by = by0; by <= by1; ++by)
    for (int bx = bx0; bx <= bx1; ++bx)
    {
      const Mass k = massAt(mMap, lw, lh, bx, by);
      if (k != Mass::Deck && k != Mass::Rack)
        continue;
      const float x = float(bx) * kTilePx - camX, y = float(by) * kTilePx - camY;
      if (k == Mass::Deck)
      {
        r.draw(deckTop, x, y);
        if (by + 1 < lh && !solid(mMap, bx, by + 1))
        {
          r.draw(deckUnder, x, y + 24.0f);
          // The light strip under it.
          for (int l = 0; l < 4; ++l)
          {
            const bool on = ((t / 6 + bx * 3 + l) % 16) != 0;
            r.fillRect(x + 4.0f + float(l) * 16.0f, y + 52.0f, 8.0f, 2.0f, on ? rgb(170, 220, 255) : rgb(40, 60, 90));
          }
        }
        continue;
      }
      // A rack block: its face, then its frame where it meets the air.
      r.draw(rackFace, x, y);
      const bool left = massAt(mMap, lw, lh, bx - 1, by) == Mass::Rack, right = massAt(mMap, lw, lh, bx + 1, by) == Mass::Rack;
      const bool up = massAt(mMap, lw, lh, bx, by - 1) == Mass::Rack;
      if (!left)
      {
        r.fillRect(x, y, 5.0f, 64.0f, rgb(70, 74, 88));
        r.fillRect(x, y, 1.5f, 64.0f, rgb(150, 156, 170));
      }
      if (!right)
        r.fillRect(x + 59.0f, y, 5.0f, 64.0f, rgb(40, 42, 52));
      if (!up)
      {
        // The cap you stand on: steel, a strip of blue light along its front.
        r.fillRect(x, y, 64.0f, 7.0f, rgb(110, 116, 130));
        r.fillRect(x, y, 64.0f, 2.0f, rgb(220, 226, 236));
        r.fillRect(x + 2.0f, y + 5.0f, 60.0f, 1.5f, kLed);
        r.fillRect(x, y + 7.0f, 64.0f, 1.0f, rgb(10, 10, 14));
      }
      // Its lights: two per server, blinking.
      for (int l = 0; l < 8; ++l)
      {
        bool on = false;
        const Color c = ledColor(bx, by, l, t / 4, on);
        const float lx = x + 33.0f + float(l % 2) * 26.0f - (l % 2 ? 4.0f : 0.0f), ly = y + 3.0f + float(l / 2) * 16.0f;
        if (!on)
          continue;
        r.fillRect(lx, ly, 3.0f, 2.0f, c);
        if (l % 3 == 0)
          r.fillRect(lx - 2.0f, ly - 2.0f, 7.0f, 6.0f, withAlpha(c, 48), Blend::Add);
      }
    }

  // The corrupted rack and its Virus dust.
  for (const auto& rk : s.racks)
  {
    const CellBox b = rk.box();
    const float cx = (float(b.x) + float(b.w) * 0.5f) * kCellPx - camX, foot = float(b.bottom() + 1) * kCellPx - camY;
    const CellBox d = rk.dust;
    const float dx0 = float(d.x) * kCellPx - camX, dy0 = float(d.y) * kCellPx - camY;
    if (offScreen(std::min(cx - 70.0f, dx0), foot - 200.0f, 600.0f, 260.0f))
      continue;
    const bool dead = rk.hp <= 0;
    const Sprite& sp = bakedSprite(mArt, r, dead ? "virus_rack_dead" : "virus_rack", 128, 192, 64, 192,
      [dead](cairo_t* cr) { paintVirusRack(cr, dead); });
    r.draw(sp.get(rk.dir > 0 ? -1 : 1), cx, foot);
    if (dead)
    {
      // Smoke curling off it.
      for (int k = 0; k < 4; ++k)
      {
        const float u = std::fmod(float(t) * 0.01f + float(k) * 0.25f, 1.0f);
        drawGlow(r, mArt, cx + std::sin(u * 6.0f + float(k)) * 14.0f, foot - 190.0f - u * 120.0f, 18.0f + u * 30.0f, rgb(40, 40, 44),
          0.5f * (1.0f - u));
      }
      continue;
    }
    // The screen glitches; the fan on its side turns, faster as it spins up.
    const float sx = cx + float(rk.dir > 0 ? 8 : -8);
    if ((hash2(t / 3, 77) % 3u) == 0u)
      r.fillRect(sx - 24.0f + float(hash2(t, 78) % 20u) - 10.0f, foot - 160.0f + float(hash2(t, 79) % 40u), 48.0f, 3.0f,
        rgba(160, 255, 120, 200));
    const float fanX = cx + float(rk.dir > 0 ? 44 : -44), fanY = foot - 72.0f;
    const float speed = rk.blowing() ? 0.9f : (rk.spinning() ? 0.15f + 0.6f * float(rk.t - (rk.on + rk.off - kRackSpin)) / float(kRackSpin) : 0.05f);
    const float fa = float(t) * speed;
    for (int k = 0; k < 3; ++k)
    {
      const float a = fa + float(k) * 2.094f;
      beamLine(r, fanX, fanY, fanX + std::cos(a) * 18.0f, fanY + std::sin(a) * 18.0f, 4.0f, rgb(90, 110, 96));
    }
    if (rk.spinning())
      drawGlow(r, mArt, fanX, fanY, 40.0f, kVirus, 0.25f + 0.25f * float((frame / 2) % 2));
    // The dust: green motes streaming across the rect, swirling.
    if (rk.blowing() || rk.spinning())
    {
      const float dw = float(d.w) * kCellPx, dh = float(d.h) * kCellPx;
      const int n = rk.blowing() ? 70 : 12;
      for (int i = 0; i < n; ++i)
      {
        const float u = std::fmod(unit(hash2(i, 3401)) + float(t) * (0.012f + 0.01f * unit(hash2(i, 3402))), 1.0f);
        const float px = rk.dir < 0 ? dx0 + dw * (1.0f - u) : dx0 + dw * u;
        const float py = dy0 + dh * unit(hash2(i, 3403)) + std::sin(u * 9.0f + float(i)) * 10.0f;
        const float sz = 3.0f + 4.0f * unit(hash2(i, 3404));
        r.fillRect(px, py, sz, sz, rgba(130, 255, 90, int(200.0f * std::sin(u * float(kPi)))));
      }
      if (rk.blowing())
        r.fillRect(dx0, dy0, dw, dh, rgba(120, 255, 80, 22 + int(8.0f * std::sin(float(t) * 0.3f))));
    }
  }

  // The DO NOT REBOOT rack, its screen.
  if (s.rebootX >= 0)
  {
    const CellBox b = s.rebootBox();
    const float cx = (float(b.x) + float(b.w) * 0.5f) * kCellPx - camX, foot = float(b.bottom() + 1) * kCellPx - camY;
    if (!offScreen(cx - 64.0f, foot - 192.0f, 128.0f, 192.0f))
    {
      r.draw(baked(mArt, r, "reboot_rack", 128, 192, 64, 192, paintRebootRack), cx, foot);
      const float sx = cx - 40.0f, sy = foot - 174.0f;
      if (s.reboot > 0)
      {
        r.fillRect(sx, sy, 80.0f, 50.0f, rgb(30, 70, 200));
        r.drawText("REBOOTING", sx + 40.0f, sy + 8.0f, {11.0f, rgb(255, 255, 255), 0}, Align::Center);
        const float done = 1.0f - float(s.reboot) / float(kRebootFrames);
        r.fillRect(sx + 8.0f, sy + 32.0f, 64.0f, 6.0f, rgb(20, 40, 120));
        r.fillRect(sx + 8.0f, sy + 32.0f, 64.0f * done, 6.0f, rgb(230, 240, 255));
        drawGlow(r, mArt, cx, sy + 25.0f, 80.0f, rgb(60, 110, 255), 0.35f);
      }
      else
      {
        r.fillRect(sx, sy, 80.0f, 50.0f, rgb(8, 24, 12));
        r.drawText("UPTIME", sx + 40.0f, sy + 6.0f, {10.0f, rgb(120, 255, 140), 0}, Align::Center);
        r.drawText(s.rebooted ? "0 DAYS" : "4,242 DAYS", sx + 40.0f, sy + 22.0f, {12.0f, rgb(160, 255, 170), 0}, Align::Center);
        if ((frame / 20) % 2 == 0)
          r.fillRect(sx + 66.0f, sy + 40.0f, 6.0f, 3.0f, rgb(160, 255, 170));
      }
    }
  }

  // The RATINGS server: its bar graph climbing (or dead).
  if (s.serverX >= 0)
  {
    const CellBox b = s.serverBox();
    const float cx = (float(b.x) + float(b.w) * 0.5f) * kCellPx - camX, foot = float(b.bottom() + 1) * kCellPx - camY;
    if (!offScreen(cx - 80.0f, foot - 192.0f, 160.0f, 192.0f))
    {
      const bool shot = s.serverShot;
      r.draw(baked(mArt, r, shot ? "ratings_shot" : "ratings", 128, 192, 64, 192, [shot](cairo_t* cr) { paintRatings(cr, shot); }), cx,
        foot);
      const float gx = cx - 30.0f, gy = foot - 84.0f;
      if (!shot)
      {
        drawGlow(r, mArt, cx, foot - 110.0f, 70.0f, rgb(255, 210, 90), 0.2f + 0.1f * std::sin(float(t) * 0.1f));
        for (int k = 0; k < 6; ++k)
        {
          const float hgt = 8.0f + 38.0f * (0.5f + 0.5f * std::sin(float(t) * 0.05f + float(k) * 0.9f)) * (0.4f + 0.12f * float(k));
          r.fillRect(gx + float(k) * 10.0f, gy - hgt, 7.0f, hgt, k == 5 ? rgb(255, 210, 90) : rgb(90, 200, 255));
        }
      }
      else if ((hash2(t / 4, 81) % 4u) == 0u)
        drawGlow(r, mArt, gx + 30.0f, gy - 20.0f, 20.0f, rgb(200, 220, 255), 0.6f);
    }
  }

  // The Fan Bots' gusts: streaks of air across the shaft.
  for (const auto& f : s.fans)
  {
    if (f.zone < 0)
      continue;
    const Enemy& e = mEnemies[std::size_t(f.enemy)];
    if (!e.alive)
      continue;
    const Zone& z = mZones[std::size_t(f.zone)];
    const float zx = float(z.box.x) * kCellPx - camX, zy = float(z.box.y) * kCellPx - camY;
    const float zw = float(z.box.w) * kCellPx, zh = float(z.box.h) * kCellPx;
    if (offScreen(zx, zy, zw, zh))
      continue;
    const int ph = mStats.frames % kFanPeriod;
    const bool blowing = ph < kFanOn;
    const int n = blowing ? 34 : (e.tell > 0 ? 6 : 0);
    const float fade = blowing ? std::min(1.0f, float(kFanOn - ph) / 10.0f) : 0.5f;
    for (int i = 0; i < n; ++i)
    {
      const float sp = 0.012f + 0.01f * unit(hash2(i, 3501));
      const float u = std::fmod(unit(hash2(i, 3502)) + float(t) * sp, 1.0f);
      const float reach = blowing ? 1.0f : 0.25f;
      const float px = z.dx > 0 ? zx + zw * u * reach : zx + zw - zw * u * reach;
      const float py = zy + zh * unit(hash2(i, 3503)) + std::sin(u * 7.0f + float(i)) * 6.0f;
      const float len = 30.0f + 60.0f * unit(hash2(i, 3504));
      const int a = int(fade * 110.0f * std::sin(u * float(kPi)));
      r.drawLine(px, py, px - float(z.dx) * len, py, 2.0f, rgba(220, 235, 255, a), Blend::Add);
    }
  }

  // The magma gutter: a channel of melt with a steel lip.
  if (s.gutter.w > 0)
  {
    const float gx = float(s.gutter.x) * kCellPx - camX, gy = float(s.gutter.y) * kCellPx - camY;
    const float gw = float(s.gutter.w) * kCellPx, gh = float(s.gutter.h) * kCellPx;
    if (!offScreen(gx, gy, gw, gh))
    {
      drawGlow(r, mArt, gx + gw * 0.5f, gy + 10.0f, gw * 0.9f, kLava, 0.45f + 0.1f * std::sin(float(t) * 0.1f));
      r.fillRect(gx, gy + 8.0f, gw, gh - 8.0f, rgb(200, 60, 14));
      for (int k = 0; k < int(gw / 8.0f); ++k)
      {
        const float x = gx + float(k) * 8.0f, wave = std::sin(float(t) * 0.12f + float(k) * 0.7f) * 3.0f;
        r.fillRect(x, gy + 6.0f + wave, 8.0f, gh - 6.0f - wave, rgb(240, 100, 24));
        r.fillRect(x, gy + 6.0f + wave, 8.0f, 4.0f, kLavaHot);
      }
      // Bubbles.
      for (int i = 0; i < 3; ++i)
      {
        const int slot = (t / 18 + i * 7);
        const float bx = gx + gw * unit(hash2(slot, 3601 + i)), life = float((t + i * 6) % 18) / 18.0f;
        drawGlow(r, mArt, bx, gy + 10.0f, 6.0f + life * 10.0f, kLavaHot, 0.8f * (1.0f - life));
      }
      // Steel lips at its ends.
      r.fillRect(gx - 4.0f, gy, 6.0f, gh, rgb(90, 94, 108));
      r.fillRect(gx + gw - 2.0f, gy, 6.0f, gh, rgb(60, 64, 76));
      // Heat rising off it.
      for (int k = 0; k < 4; ++k)
      {
        const float u = std::fmod(float(t) * 0.02f + float(k) * 0.25f, 1.0f);
        r.fillRect(gx + 6.0f + float(k) * gw / 4.0f + std::sin(u * 8.0f) * 6.0f, gy - u * 90.0f, 2.0f, 16.0f,
          rgba(255, 200, 140, int(70.0f * (1.0f - u))));
      }
    }
  }
}

// --- In front ------------------------------------------------------------------------------------

namespace
{

// A soft round puff of steam or smoke (alpha-blended, unlike a glow).
void paintPuff(cairo_t* cr)
{
  cairo_pattern_t* g = cairo_pattern_create_radial(64, 64, 0, 64, 64, 64);
  cairo_pattern_add_color_stop_rgba(g, 0.0, 1, 1, 1, 0.9);
  cairo_pattern_add_color_stop_rgba(g, 0.5, 1, 1, 1, 0.5);
  cairo_pattern_add_color_stop_rgba(g, 1.0, 1, 1, 1, 0.0);
  cairo_arc(cr, 64, 64, 64, 0, 2 * kPi);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
}

void puff(Renderer& r, const Art& art, float x, float y, float rad, Color c, float a)
{
  if (a <= 0.01f || rad <= 1.0f)
    return;
  DrawOpts o;
  o.tint = c;
  o.alpha = std::min(1.0f, a);
  o.scale = rad / 64.0f;
  r.draw(baked(art, r, "puff", 128, 128, 64, 64, paintPuff), x, y, o);
}

// A ball of fire (96 x 96, anchored mid): a white-hot heart, orange
// licks of flame round it.
void paintFireball(cairo_t* cr)
{
  glow(cr, 48, 48, 46, rgb(255, 100, 20), 0.6);
  for (int k = 0; k < 7; ++k)
  {
    const double a = k * 2.0 * kPi / 7.0;
    cairo_move_to(cr, 48 + std::cos(a - 0.3) * 18, 48 + std::sin(a - 0.3) * 18);
    cairo_curve_to(cr, 48 + std::cos(a) * 30, 48 + std::sin(a) * 30, 48 + std::cos(a + 0.4) * 40, 48 + std::sin(a + 0.4) * 40,
      48 + std::cos(a + 0.5) * 42, 48 + std::sin(a + 0.5) * 42);
    cairo_curve_to(cr, 48 + std::cos(a + 0.5) * 30, 48 + std::sin(a + 0.5) * 30, 48 + std::cos(a + 0.4) * 22,
      48 + std::sin(a + 0.4) * 22, 48 + std::cos(a + 0.3) * 18, 48 + std::sin(a + 0.3) * 18);
    cairo_close_path(cr);
    setRgba(cr, rgb(255, 120, 30), 0.85);
    cairo_fill(cr);
  }
  cairo_pattern_t* g = cairo_pattern_create_radial(44, 44, 0, 48, 48, 24);
  stop(g, 0.0, rgb(255, 255, 240));
  stop(g, 0.45, rgb(255, 220, 120));
  stop(g, 1.0, rgb(255, 130, 30));
  circle(cr, 48, 48, 24);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
}

} // namespace

void World::drawMagmaFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& s = mMagma;
  const int t = animClock(*this, frame);
  const auto& p = mPlayer;

  // Overclock: everything leaves streaks; speed lines across the screen.
  if (s.overclock)
  {
    for (int i = 0; i < 14; ++i)
    {
      const float u = std::fmod(unit(hash2(i, 3701)) + float(frame) * (0.03f + 0.02f * unit(hash2(i, 3702))), 1.0f);
      const float y = unit(hash2(i, 3703)) * float(kScreenH);
      const float x = float(kScreenW) * (1.0f - u);
      r.drawLine(x, y, x + 120.0f + 80.0f * unit(hash2(i, 3704)), y, 2.0f, rgba(140, 220, 255, 50), Blend::Add);
    }
    auto streak = [&](float cx, float cy, float hgt, int dir) {
      for (int k = 0; k < 5; ++k)
      {
        const float y = cy - hgt * 0.5f + hgt * (float(k) + 0.5f) / 5.0f;
        const float len = 50.0f + 40.0f * float((k * 7 + frame) % 3);
        r.drawLine(cx - float(dir) * 30.0f, y, cx - float(dir) * (30.0f + len), y, 3.0f, rgba(170, 230, 255, 90), Blend::Add);
      }
    };
    if (p.x != p.prevX)
      streak((lerpF(p.prevX, p.x, alpha) + 1.5f) * kCellPx - camX, (lerpF(p.prevY, p.y, alpha) - 2.0f) * kCellPx - camY,
        float(Player::kHeight) * kCellPx, p.x > p.prevX ? 1 : -1);
    for (const auto& e : mEnemies)
      if (e.alive && e.kind == EnemyKind::Firewall)
        streak((e.drawX + float(e.w) * 0.5f) * kCellPx - camX, (e.drawY - float(e.h) * 0.5f + 1.0f) * kCellPx - camY,
          float(e.h) * kCellPx, e.dir < 0 ? -1 : 1);
  }

  // Firewalls, Lava Daemons, Fan Bots.
  for (std::size_t i = 0; i < mEnemies.size(); ++i)
  {
    const Enemy& e = mEnemies[i];
    if (!e.alive)
      continue;
    const float cx = (e.drawX + float(e.w) * 0.5f) * kCellPx - camX, foot = (e.drawY + 1.0f) * kCellPx - camY;
    const float h = float(e.h) * kCellPx;
    if (offScreen(cx - 120.0f, foot - h - 120.0f, 240.0f, h + 160.0f))
      continue;
    const int dir = e.dir < 0 ? -1 : 1;
    const Sprite* sp = nullptr;
    if (e.kind == EnemyKind::Firewall)
    {
      const int v = e.dive ? 2 : (e.tell > 0 ? 1 : 0);
      sp = &styledEnemySprite(mArt, r, mTheme, "firewall", v, (t / 4) % 4, e.w, e.h);
      drawGlow(r, mArt, cx, foot - (v == 2 ? 30.0f : 100.0f), v == 2 ? 100.0f : 170.0f, rgb(255, 110, 30), 0.5f);
      // Heat shimmer over the flames.
      const float top = foot - (v == 2 ? 64.0f : h);
      for (int k = 0; k < 4; ++k)
      {
        const float u = std::fmod(float(t) * 0.03f + float(k) * 0.25f, 1.0f);
        const float y = top - u * 90.0f;
        const float wob = std::sin(float(t) * 0.2f + float(k)) * 8.0f;
        r.drawLine(cx - 50.0f + wob, y, cx + 50.0f + wob, y, 2.0f, rgba(255, 220, 170, int(60.0f * (1.0f - u))), Blend::Add);
      }
      r.draw(sp->get(dir), cx, foot);
      if (v == 1 && (frame / 2) % 2 == 0)
      {
        DrawOpts wo;
        wo.blend = Blend::Add;
        wo.alpha = 0.5f;
        r.draw(sp->get(dir), cx, foot, wo);
      }
    }
    else if (e.kind == EnemyKind::LavaDaemon)
    {
      const Daemon* d = nullptr;
      for (const auto& dd : s.daemons)
        if (dd.enemy == int(i))
          d = &dd;
      int v = 0;
      if (d && d->leapT > 0)
        v = 3;
      else if (d && d->marshmallow && !d->roused)
        v = 4;
      else if (e.tell > 0)
        v = 1;
      else if (e.cool > enemyDef(e.def).cooldown - 4)
        v = 2;
      sp = &styledEnemySprite(mArt, r, mTheme, "lava_daemon", v, (t / 8) % 2, e.w, e.h);
      drawGlow(r, mArt, cx, foot - h * 0.5f, 90.0f, rgb(255, 100, 30), 0.35f);
      if (v == 4)
      {
        // The vent it roasts its marshmallow over.
        const float vx = cx + float(dir) * 44.0f;
        r.fillRect(vx - 14.0f, foot - 4.0f, 28.0f, 4.0f, rgb(40, 30, 30));
        drawGlow(r, mArt, vx, foot - 6.0f, 30.0f, kLava, 0.7f + 0.2f * std::sin(float(t) * 0.3f));
        for (int k = 0; k < 3; ++k)
        {
          const float u = std::fmod(float(t) * 0.04f + float(k) / 3.0f, 1.0f);
          r.fillRect(vx - 2.0f + std::sin(u * 9.0f + float(k)) * 5.0f, foot - 8.0f - u * 70.0f, 3.0f, 8.0f,
            rgba(255, 200, 120, int(120.0f * (1.0f - u))));
        }
      }
      r.draw(sp->get(dir), cx, foot);
      if (v == 1)
      {
        // The wind-up: the fist's fireball swelling.
        const float k = 1.0f - float(e.tell) / float(std::max(1, enemyDef(e.def).tell));
        drawGlow(r, mArt, cx - float(dir) * 20.0f, foot - 110.0f, 20.0f + 40.0f * k, rgb(255, 160, 60), 0.5f + 0.4f * k);
      }
      if (v == 3)
        for (int k = 0; k < 3; ++k)
          r.fillRect(cx - 12.0f + float(k) * 10.0f, foot + 4.0f + float(k % 2) * 6.0f, 4.0f, 4.0f, rgba(255, 170, 60, 180));
    }
    else if (e.kind == EnemyKind::FanBot)
    {
      const int v = e.dive ? 2 : (e.tell > 0 ? 1 : 0);
      int f = (t / 14) % 4;
      if (v == 2)
        f = t % 4;
      else if (v == 1)
        f = (t * (1 + (kFanSpin - e.tell) / 4) / 4) % 4;
      sp = &styledEnemySprite(mArt, r, mTheme, "fan_bot", v, f, e.w, e.h);
      r.draw(sp->get(dir), cx, foot);
      if (v == 1)
      {
        // Spinning up: whine rings off the rotor.
        const float k = 1.0f - float(e.tell) / float(kFanSpin);
        const float rx = cx + float(dir) * 34.0f;
        for (int q = 0; q < 2; ++q)
        {
          const float rad = 30.0f + std::fmod(float(frame) * 3.0f + float(q) * 25.0f, 50.0f);
          drawGlow(r, mArt, rx, foot - h * 0.5f, rad, rgb(255, 220, 120), 0.12f * k);
        }
      }
    }
    if (sp && e.flash > 0 && (frame / 2) % 2 == 0)
    {
      DrawOpts wo;
      wo.blend = Blend::Add;
      wo.alpha = 0.55f;
      r.draw(sp->get(dir), cx, foot, wo);
    }
  }

  // The coolant purges' steam: a burst down out of the nozzle, billowing
  // out and up.
  for (const auto& pu : s.purges)
  {
    if (pu.steam <= 0)
      continue;
    const float nx = float(pu.x + 1) * kCellPx - camX, ny = float(pu.y + 1) * kCellPx - camY - 258.0f;
    if (offScreen(nx - 300.0f, ny - 200.0f, 600.0f, 500.0f))
      continue;
    const float age = float(30 - pu.steam) + alpha;
    for (int i = 0; i < 26; ++i)
    {
      const float a = age - float(i) * 0.4f;
      if (a <= 0.0f)
        continue;
      const float side = (unit(hash2(i, 3801)) - 0.5f) * 2.0f;
      const float jet = std::min(a, 5.0f) * 44.0f;
      const float px = nx + side * (20.0f + a * 9.0f);
      const float py = ny + jet - std::max(0.0f, a - 5.0f) * (3.0f + 3.0f * unit(hash2(i, 3802)));
      const float rad = 26.0f + a * 4.0f + 20.0f * unit(hash2(i, 3803));
      puff(r, mArt, px, py, rad, rgb(236, 244, 255), 0.75f * (1.0f - age / 30.0f));
    }
    drawGlow(r, mArt, nx, ny + 80.0f, 160.0f, kCoolant, 0.35f * (1.0f - age / 30.0f));
  }

  // The rising lava.
  if (s.rising)
  {
    const float surf = float(s.surface) * kCellPx - camY;
    const float lx0 = float(s.x0) * kCellPx - camX, lx1 = float(s.x1 + 1) * kCellPx - camX;
    const bool held = s.pause > 0 && s.delay == 0;
    const bool siren = held && s.pause <= kSirenFrames;
    if (surf < float(kScreenH) + 40.0f)
    {
      const float x0 = std::max(lx0, -8.0f), x1 = std::min(lx1, float(kScreenW) + 8.0f);
      // Heat shimmer: wavering bands of hot air rising off it.
      if (!held)
        for (int k = 0; k < 9; ++k)
        {
          const float u = std::fmod(float(t) * 0.012f + float(k) / 9.0f, 1.0f);
          const float y = surf - 10.0f - u * 260.0f;
          if (y < -10.0f || y > float(kScreenH))
            continue;
          const int a = int(46.0f * (1.0f - u));
          for (float x = x0; x < x1; x += 40.0f)
          {
            const float yy = y + std::sin(x * 0.02f + float(t) * 0.15f + float(k)) * 5.0f;
            const float y2 = y + std::sin((x + 40.0f) * 0.02f + float(t) * 0.15f + float(k)) * 5.0f;
            r.drawLine(x, yy, x + 40.0f, y2, 3.0f, rgba(255, 190, 130, a), Blend::Add);
          }
        }
      // The melt, flowing slowly.
      const Texture& body = baked(mArt, r, "lava_body", 256, 256, 0, 0, paintLavaBody);
      const float flow = std::fmod(float(t) * 0.4f + camX, 256.0f);
      for (float y = surf; y < float(kScreenH); y += 256.0f)
        for (float x = x0 - flow; x < x1; x += 256.0f)
          r.draw(body, x, y);
      // Darker with depth.
      for (int k = 0; k < 6; ++k)
      {
        const float y = surf + 40.0f + float(k) * 40.0f;
        if (y > float(kScreenH))
          break;
        r.fillRect(x0, y, x1 - x0, k == 5 ? float(kScreenH) - y : 40.0f, rgba(90, 14, 4, 30 + k * 26));
      }
      // Crust plates drifting on it.
      const Texture& crust = baked(mArt, r, "crust", 96, 40, 48, 14, paintCrust);
      for (int i = 0; i < 7; ++i)
      {
        const float dirc = (i % 2) ? 1.0f : -1.0f;
        const float span = x1 - x0 + 200.0f;
        float cxp = std::fmod(unit(hash2(i, 3901)) * span + float(t) * 0.25f * dirc + span * 4.0f, span) + x0 - 100.0f;
        DrawOpts co;
        co.scale = 0.7f + 0.6f * unit(hash2(i, 3902));
        r.draw(crust, cxp, surf + 6.0f + std::sin(float(t) * 0.05f + float(i)) * 2.0f, co);
      }
      // The glowing surface, rolling.
      for (float x = x0; x < x1; x += 16.0f)
      {
        auto wave = [&](float xx) { return surf + std::sin(xx * 0.015f + float(t) * 0.06f) * 4.0f + std::sin(xx * 0.05f - float(t) * 0.1f) * 2.0f; };
        r.drawLine(x, wave(x + camX), x + 16.0f, wave(x + 16.0f + camX), 7.0f, held ? rgb(150, 70, 40) : kLavaHot);
        r.drawLine(x, wave(x + camX) - 2.0f, x + 16.0f, wave(x + 16.0f + camX) - 2.0f, 10.0f, rgba(255, 150, 60, held ? 30 : 90), Blend::Add);
      }
      // Bubbles swelling and bursting.
      for (int i = 0; i < 6; ++i)
      {
        const int life = 24;
        const int slot = (t + i * 11) / life;
        const float u = float((t + i * 11) % life) / float(life);
        const float bx = x0 + (x1 - x0) * unit(hash2(slot, 4001 + i));
        const float rad = 6.0f + 16.0f * u;
        if (u < 0.85f)
        {
          drawGlow(r, mArt, bx, surf + 2.0f, rad * 1.6f, kLavaHot, 0.5f);
          r.drawLine(bx - rad, surf + 2.0f - rad * 0.6f, bx + rad, surf + 2.0f - rad * 0.6f, 2.0f, rgba(255, 240, 200, 120));
        }
        else
          for (int k = 0; k < 4; ++k)
          {
            const float a = float(k) * 0.9f + 0.5f;
            r.fillRect(bx + std::cos(a) * 20.0f * (u - 0.6f) * 4.0f, surf - std::sin(a) * 26.0f * (u - 0.6f) * 4.0f, 4.0f, 4.0f, kLavaHot);
          }
      }
      // Embers rising off it.
      for (int i = 0; i < 22; ++i)
      {
        const float u = std::fmod(unit(hash2(i, 4101)) + float(t) * (0.006f + 0.006f * unit(hash2(i, 4102))), 1.0f);
        const float ex = x0 + (x1 - x0) * unit(hash2(i, 4103)) + std::sin(u * 10.0f + float(i)) * 14.0f;
        const float ey = surf - u * 360.0f;
        r.fillRect(ex, ey, 3.0f, 3.0f, rgba(255, 190, 90, int(220.0f * (1.0f - u))));
      }
      // A purge holding it: cooled to a dark crust, steam curling off,
      // cracking and glowing again as the siren sounds.
      if (held)
      {
        r.fillRect(x0, surf - 2.0f, x1 - x0, 34.0f, rgba(46, 40, 44, 230));
        r.fillRect(x0, surf + 32.0f, x1 - x0, 30.0f, rgba(46, 40, 44, 120));
        const float glowK = siren ? 0.5f + 0.5f * float((frame / 3) % 2) : 0.25f;
        for (int i = 0; i < 18; ++i)
        {
          const float cxk = x0 + (x1 - x0) * unit(hash2(i, 4201));
          const float len = 30.0f + 50.0f * unit(hash2(i, 4202));
          r.drawLine(cxk, surf + 4.0f, cxk + len, surf + 10.0f + 10.0f * unit(hash2(i, 4203)), 3.0f,
            withAlpha(kLava, int(255.0f * glowK)));
        }
        for (int i = 0; i < 8; ++i)
        {
          const float u = std::fmod(float(t) * 0.01f + float(i) / 8.0f, 1.0f);
          puff(r, mArt, x0 + (x1 - x0) * unit(hash2(i, 4204)) + std::sin(u * 5.0f) * 20.0f, surf - u * 140.0f, 24.0f + u * 40.0f,
            rgb(220, 226, 236), 0.35f * (1.0f - u));
        }
      }
    }
  }

  // The pop: a fiery arc from where the runner went in to where they came
  // down, a splash in the lava, smoke on the runner (20 frames).
  {
    const float age = float(mStats.frames - s.popFrame) + alpha;
    if (age >= 0.0f && age < 20.0f)
    {
      const float fx = (float(s.popFromX) + 1.5f) * kCellPx - camX, fy = (float(s.popFromY) - 1.0f) * kCellPx - camY;
      const float tx = (float(s.popToX) + 1.5f) * kCellPx - camX, ty = (float(s.popToY) - 2.0f) * kCellPx - camY;
      const float apex = std::min(fy, ty) - 140.0f;
      auto at = [&](float u, float& x, float& y) {
        x = fx + (tx - fx) * u;
        const float lin = fy + (ty - fy) * u;
        y = lin - 4.0f * (lin - apex) * u * (1.0f - u) * 0.9f;
      };
      const float head = std::min(1.0f, age / 7.0f), tail = std::clamp((age - 5.0f) / 13.0f, 0.0f, 1.0f);
      const float fade = 1.0f - std::max(0.0f, (age - 12.0f) / 8.0f);
      float px = 0.0f, py = 0.0f;
      at(tail, px, py);
      for (int k = 1; k <= 20; ++k)
      {
        const float u = tail + (head - tail) * float(k) / 20.0f;
        float x = 0.0f, y = 0.0f;
        at(u, x, y);
        const float wgt = 0.45f + 0.55f * float(k) / 20.0f;
        r.drawLine(px, py, x, y, 34.0f * wgt, rgba(255, 100, 20, int(255.0f * fade * wgt)), Blend::Add);
        r.drawLine(px, py, x, y, 16.0f * wgt, rgba(255, 180, 70, int(255.0f * fade * wgt)), Blend::Add);
        r.drawLine(px, py, x, y, 6.0f + 4.0f * wgt, rgba(255, 246, 210, int(255.0f * fade)));
        px = x;
        py = y;
      }
      drawGlow(r, mArt, px, py, 56.0f, kLavaHot, 0.9f * fade);
      // Embers falling off the arc.
      for (int i = 0; i < 10; ++i)
      {
        float x = 0.0f, y = 0.0f;
        at(unit(hash2(i, 4301)) * head, x, y);
        r.fillRect(x + float(i % 3 - 1) * age * 2.0f, y + age * age * 0.25f, 4.0f, 4.0f, rgba(255, 180, 70, int(220.0f * fade)));
      }
      // The splash where they went in.
      drawGlow(r, mArt, fx, fy + 40.0f, 50.0f + age * 8.0f, kLava, 0.9f * (1.0f - age / 20.0f));
      for (int i = 0; i < 8; ++i)
      {
        // Lava thrown up by the splash.
        const float a = -float(kPi) * (0.2f + 0.6f * unit(hash2(i, 4307))), v = 6.0f + 6.0f * unit(hash2(i, 4309));
        const float t = std::min(age, 14.0f);
        r.fillRect(fx + std::cos(a) * v * t - 4.0f, fy + 30.0f + std::sin(a) * v * t + 0.9f * t * t - 4.0f, 9.0f, 9.0f,
          rgba(255, 150, 50, int(230.0f * (1.0f - age / 20.0f))), Blend::Add);
      }
      // Smoke on the runner.
      const float rx = (lerpF(p.prevX, p.x, alpha) + 1.5f) * kCellPx - camX, ry = (lerpF(p.prevY, p.y, alpha) - 4.0f) * kCellPx - camY;
      for (int k = 0; k < 4; ++k)
      {
        const float u = std::fmod(age / 20.0f + float(k) * 0.25f, 1.0f);
        puff(r, mArt, rx + std::sin(u * 6.0f + float(k)) * 14.0f, ry - u * 90.0f, 14.0f + u * 26.0f, rgb(80, 74, 76), 0.6f * (1.0f - u) * fade);
      }
    }
  }

  // The Heat Sink's cone: warm air pulled in, embers streaming to the
  // muzzle.
  if (s.sucking)
  {
    const int row = p.stance == Stance::Crouched ? p.y - 1 : p.y - 2;
    const float px = lerpF(p.prevX, p.x, alpha), py = lerpF(p.prevY, p.y, alpha) - float(p.y - row);
    const float mx = (p.facing > 0 ? px + float(Player::kWidth) : px) * kCellPx - camX, my = (py + 0.5f) * kCellPx - camY;
    const int f = p.facing > 0 ? 1 : -1;
    const Sprite& cone = bakedSprite(mArt, r, "sink_cone", 256, 352, 0, 176, paintSinkCone);
    DrawOpts co;
    co.blend = Blend::Add;
    co.alpha = 0.6f + 0.2f * std::sin(float(frame) * 0.5f);
    r.draw(cone.get(f), mx, my, co);
    for (int i = 0; i < 16; ++i)
    {
      const float u = std::fmod(unit(hash2(i, 4401)) + float(frame) * 0.035f, 1.0f);
      const float d = (1.0f - u) * 256.0f;
      const float lat = (unit(hash2(i, 4402)) - 0.5f) * 2.0f * (16.0f + d * 0.62f);
      const float x = mx + float(f) * d, y = my + lat;
      r.drawLine(x, y, x - float(f) * 14.0f, y - lat * 0.06f, 3.0f, rgba(255, 200, 110, int(200.0f * u)), Blend::Add);
    }
    drawGlow(r, mArt, mx, my, 30.0f + 6.0f * float(p.ammo), rgb(255, 160, 60), 0.6f);
  }
}

// --- The HUD -------------------------------------------------------------------------------------

void World::drawMagmaHud(Renderer& r, int frame) const
{
  const auto& s = mMagma;
  // Overclock's sign, pulsing.
  if (s.overclock)
  {
    DrawOpts so;
    so.scale = 0.8f + 0.04f * std::sin(float(frame) * 0.5f);
    so.alpha = (frame / 3) % 9 == 0 ? 0.6f : 1.0f;
    const float x = float(kScreenW) * 0.5f, y = 184.0f;
    drawGlow(r, mArt, x, y, 200.0f, rgb(255, 90, 40), 0.25f + 0.15f * std::sin(float(frame) * 0.5f));
    r.draw(baked(mArt, r, "overclock_sign", 360, 84, 180, 42, paintOverclockSign), x, y, so);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "CLOCK x3  %d.%02d GHz", 4 + (frame / 7) % 3, (frame * 37) % 100);
    r.drawText(buf, x, y + 40.0f, {18.0f, rgb(150, 220, 255), kHudInk}, Align::Center);
  }
  // The DO NOT REBOOT rack's reboot screen, blue over the middle.
  if (s.reboot > 0)
  {
    const float k = std::min(1.0f, float(kRebootFrames - s.reboot) / 6.0f) * std::min(1.0f, float(s.reboot) / 6.0f);
    r.fillRect(0, 0, float(kScreenW), float(kScreenH), rgba(20, 40, 160, int(60.0f * k)));
    const float pw = 620.0f, ph = 260.0f, px = (float(kScreenW) - pw) * 0.5f, py = 200.0f;
    r.fillRect(px - 6.0f, py - 6.0f, pw + 12.0f, ph + 12.0f, rgba(200, 210, 230, int(230.0f * k)));
    r.fillRect(px, py, pw, ph, rgba(28, 60, 190, int(240.0f * k)));
    const TextStyle big{34.0f, rgb(255, 255, 255), kHudInk};
    const TextStyle small{20.0f, rgb(200, 220, 255), 0};
    r.drawText("MAINFRAME REBOOTING", px + pw * 0.5f, py + 24.0f, big, Align::Center, k);
    r.drawText("UPTIME 4,242 DAYS  >  0 DAYS", px + pw * 0.5f, py + 80.0f, small, Align::Center, k);
    r.drawText("PLEASE DO NOT TURN OFF THE VOLCANO", px + pw * 0.5f, py + 112.0f, small, Align::Center, k);
    const float done = 1.0f - float(s.reboot) / float(kRebootFrames);
    r.fillRect(px + 60.0f, py + 170.0f, pw - 120.0f, 22.0f, rgba(10, 24, 90, int(255.0f * k)));
    r.fillRect(px + 64.0f, py + 174.0f, (pw - 128.0f) * done, 14.0f, rgba(240, 246, 255, int(255.0f * k)));
    for (int d = 0; d < 6; ++d)
    {
      const float a = float(frame) * 0.25f + float(d) * 0.6f;
      r.fillRect(px + pw * 0.5f + std::cos(a) * 16.0f - 2.0f, py + 222.0f + std::sin(a) * 16.0f - 2.0f, 5.0f, 5.0f,
        rgba(255, 255, 255, int(float(40 + d * 36) * k)));
    }
  }
  if (!s.rising)
    return;
  // The lava gauge: a tube on the right, the lava climbing it as it closes
  // in (empty at 30 blocks below you, full at you).
  const float gx = float(kScreenW) - 92.0f, gy = 110.0f;
  r.draw(baked(mArt, r, "gauge", 64, 300, 0, 0, paintGauge), gx, gy);
  const int gap = lavaGap();
  const float frac = 1.0f - std::clamp(float(gap) / 30.0f, 0.0f, 1.0f);
  const float tubeTop = gy + 14.0f, tubeH = 272.0f;
  const float fillH = tubeH * frac;
  const bool held = s.pause > 0 && s.delay == 0;
  if (fillH > 0.5f)
  {
    r.fillRect(gx + 17.0f, tubeTop + tubeH - fillH, 22.0f, fillH, held ? rgb(110, 60, 50) : rgb(230, 80, 20));
    r.fillRect(gx + 17.0f, tubeTop + tubeH - fillH, 22.0f, 4.0f, held ? rgb(140, 180, 220) : kLavaHot);
    if (!held)
      drawGlow(r, mArt, gx + 28.0f, tubeTop + tubeH - fillH, 26.0f, kLava, 0.6f);
  }
  // The danger mark (6 blocks) and you at the top.
  const float danger = tubeTop + tubeH * (6.0f / 30.0f);
  r.fillRect(gx + 10.0f, danger - 1.0f, 36.0f, 3.0f, rgb(255, 60, 40));
  r.fillRect(gx + 22.0f, tubeTop - 10.0f, 12.0f, 12.0f, rgb(255, 255, 255));
  r.fillRect(gx + 24.0f, tubeTop - 8.0f, 8.0f, 8.0f, rgb(60, 140, 255));
  const bool close = gap < 8 && (frame / 8) % 2 == 0;
  r.drawText("LAVA", gx + 32.0f, gy - 28.0f, {20.0f, rgb(255, 190, 80), kHudInk}, Align::Center);
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%d", gap);
  r.drawText(buf, gx + 32.0f, gy + 302.0f, {30.0f, close ? rgb(255, 70, 40) : rgb(255, 210, 120), kHudInk}, Align::Center);
  r.drawText("BLOCKS", gx + 32.0f, gy + 336.0f, {13.0f, rgb(220, 200, 180), kHudInk}, Align::Center);
  // Before it starts; while a purge holds it; the siren.
  if (s.delay > 0)
  {
    std::snprintf(buf, sizeof(buf), "LAVA RISING IN %d", (s.delay + 14) / 15);
    r.drawText(buf, gx - 14.0f, gy + 60.0f, {22.0f, rgb(255, 190, 80), kHudInk}, Align::Right);
  }
  else if (held)
  {
    const bool siren = s.pause <= kSirenFrames;
    const float left = float(s.pause) / float(kPurgeFrames);
    const float bx = gx - 180.0f, by = gy + 60.0f;
    r.fillRect(bx, by + 24.0f, 160.0f, 12.0f, rgb(16, 24, 40));
    r.fillRect(bx + 2.0f, by + 26.0f, 156.0f * left, 8.0f, siren ? rgb(255, 60, 40) : kCoolant);
    if (!siren || (frame / 4) % 2 == 0)
      r.drawText(siren ? "LAVA RISING!" : "COOLANT HOLD", bx + 160.0f, by, {20.0f, siren ? rgb(255, 80, 60) : kCoolant, kHudInk},
        Align::Right);
  }
}

// --- Shots and icons -----------------------------------------------------------------------------

bool drawMagmaShot(Renderer& r, const World& w, const Art& art, const Projectile& pr, float cx, float cy, int frame)
{
  const auto& s = w.magma();
  const bool sink = pr.kind == ShotKind::Rocket && pr.proto == int(ProtoId::HeatSink);
  const bool lob = pr.kind == ShotKind::Enemy && s.on && pr.precise && pr.gy > 0.0f;
  if (!sink && !lob)
    return false;
  const Texture& ball = baked(art, r, "fireball", 96, 96, 48, 48, paintFireball);
  if (sink)
  {
    // The Heat Sink's fireball: bigger the more it holds, a trail of fire.
    const float rad = 20.0f + 5.0f * float(std::max(1, pr.damage));
    const float d = float(pr.dx < 0 ? -1 : 1);
    for (int k = 6; k >= 1; --k)
    {
      const float wob = std::sin(float(frame) * 0.6f + float(k)) * 6.0f;
      drawGlow(r, art, cx - d * float(k) * rad * 0.45f, cy + wob, rad * (1.1f - float(k) * 0.12f), rgb(255, 110 + k * 10, 30),
        0.55f - float(k) * 0.06f);
    }
    drawGlow(r, art, cx, cy, rad * 2.6f, rgb(255, 120, 40), 0.6f);
    DrawOpts o;
    o.scale = rad / 30.0f;
    o.angle = float(frame * 14 % 360);
    r.draw(ball, cx, cy, o);
    return true;
  }
  // A Lava Daemon's fireball: small, sparks trailing behind it.
  float vx = pr.vx, vy = pr.vy;
  const float len = std::max(0.001f, std::sqrt(vx * vx + vy * vy));
  vx /= len;
  vy /= len;
  for (int k = 1; k <= 5; ++k)
    r.fillRect(cx - vx * float(k) * 9.0f + float((frame + k) % 3 - 1) * 2.0f, cy - vy * float(k) * 9.0f, 5.0f - float(k) * 0.6f,
      5.0f - float(k) * 0.6f, rgba(255, 170, 60, 230 - k * 35));
  drawGlow(r, art, cx, cy, 40.0f, rgb(255, 110, 30), 0.7f);
  DrawOpts o;
  o.scale = 0.38f;
  o.angle = float(frame * 20 % 360);
  r.draw(ball, cx, cy, o);
  return true;
}

void drawSinkIcon(Renderer& r, const Art& art, float x, float y, float scale, int frame)
{
  DrawOpts o;
  o.scale = scale;
  r.draw(baked(art, r, "sink", 64, 64, 0, 0, paintSink), x, y, o);
  // Its core glows; heat rising off the fins.
  drawGlow(r, art, x + 24.0f * scale, y + 29.0f * scale, 16.0f * scale, rgb(255, 150, 50), 0.5f + 0.25f * std::sin(float(frame) * 0.15f));
  for (int k = 0; k < 3; ++k)
  {
    const float u = std::fmod(float(frame) * 0.02f + float(k) / 3.0f, 1.0f);
    r.fillRect(x + (14.0f + float(k) * 10.0f + std::sin(u * 6.0f) * 3.0f) * scale, y + (8.0f - u * 14.0f) * scale, 2.0f * scale,
      5.0f * scale, rgba(255, 200, 140, int(140.0f * (1.0f - u))));
  }
}

} // namespace gr
