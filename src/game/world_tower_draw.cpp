// Level 35, The Transmitter: drawing. See world_tower.cpp for the logic.
//
// The map's x wraps and the world turns the tower round the runner, moving
// the camera with it (Camera::shiftX). Everything painted on the tower here
// is placed by the tower's own x: the camera's x with the turn taken off
// (towerCamX), wrapped once round the lap (4096 px). The layers behind
// move by whole fractions of a lap, so they come back round seamlessly too.
//
// Behind the tiles (drawTowerSet): the dawn cloud sea far below with the
// sun and the studios on the horizon, wisps drifting past, the far side of
// the lattice turning the other way, the near side's red and white bays
// with the dishes, panel antennas and warning lights, the deck's guard rail,
// the spire with its big dish and the alien's flag, the crawl space's
// walls. Behind the runner (drawTowerBack): the hatches, the Copycat pits,
// the swarm drones on the far side of their orbit. In front
// (drawTowerFront): Mast Climbers, Relay Drones, Feedback Nodes and their
// rings, the swarm drones on the near side, the brochure, the Halcyon
// Titan and its laser, flames, crosshairs and missiles, steam, shockwave,
// the cargo airship and its banner, the lattice shot's glow, a push's
// streaks, and the round tower's sides falling away into shadow. The HUD
// (drawTowerHud): the holo banners, the Titan's bar and where it is when it
// is round the back, the Copycat count.

#include "game/world.hpp"

#include "assets/art.hpp"
#include "assets/enemy_art.hpp"
#include "assets/enemy_art_tower35.hpp"
#include "base/math.hpp"
#include "game/tower_draw.hpp"
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
constexpr Color kInk = rgb(20, 16, 26);
constexpr Color kHudInk = rgb(10, 8, 20);
constexpr Color kRed = rgb(196, 58, 48);
constexpr Color kWhite = rgb(236, 234, 228);
constexpr Color kHalcyon = rgb(80, 170, 255);
constexpr Color kSwarm = rgb(160, 255, 255);
constexpr float kLapPx = 4096.0f;  // once round the tower (64 blocks)
constexpr float kBayPx = 256.0f;   // the lattice's bays: 4 x 4 blocks
constexpr float kPanoW = 1536.0f;  // the cloud sea: 0.375 of a lap's turn
constexpr float kWispW = 2048.0f;  // the wisps: half a lap's turn

bool offScreen(float x, float y, float w, float h, float m = 96.0f)
{
  return x > float(kScreenW) + m || x + w < -m || y > float(kScreenH) + m || y + h < -m;
}

float unit(std::uint32_t h) { return float(h % 10000u) / 10000.0f; }

float lerpF(int a, int b, float t) { return float(a) + (float(b) - float(a)) * t; }

// x wrapped into [lo, lo + period).
float wrapFrom(float x, float period, float lo) { return x - period * std::floor((x - lo) / period); }

const Sprite& bakedSprite(const Art& art, const Renderer& r, const std::string& key, int w, int h, float ax, float ay,
  const std::function<void(cairo_t*)>& paint)
{
  const std::string id = "~tower/" + key;
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
  cairo_scale(cr, std::max(0.01, rx), std::max(0.01, ry));
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

// A soft elliptical blob (clouds, smoke).
void blob(cairo_t* cr, double x, double y, double rx, double ry, Color c, double a)
{
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_scale(cr, std::max(0.1, rx), std::max(0.1, ry));
  cairo_pattern_t* g = cairo_pattern_create_radial(0, 0, 0, 0, 0, 1);
  stop(g, 0.0, c, a);
  stop(g, 0.65, c, a * 0.75);
  stop(g, 1.0, c, 0.0);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
  cairo_restore(cr);
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

void puff(Renderer& r, const Art& art, float x, float y, float rad, Color c, float a)
{
  if (a <= 0.01f)
    return;
  DrawOpts o;
  o.tint = c;
  o.alpha = std::min(1.0f, a);
  o.scale = rad / 16.0f;
  r.draw(art.dot, x, y, o);
}

// --- Painters: the sky and the mast -----------------------------------------------------------------

// The cloud sea far below (kPanoW x 900, it tiles side by side; the
// horizon at y 420): the sun just up over it, a range of far peaks
// standing out of it, the MaxTV studios' mast tiny on the horizon, and
// banks of cloud lit gold and pink on top, lavender in their shadows,
// thickening toward the bottom.
void paintPanorama(cairo_t* cr)
{
  const double W = kPanoW;
  // The sun and its glow.
  glow(cr, 1060, 400, 420, rgb(255, 210, 140), 0.55);
  glow(cr, 1060, 400, 120, rgb(255, 240, 200), 0.8);
  circle(cr, 1060, 404, 34);
  setRgba(cr, rgb(255, 250, 225), 1.0);
  cairo_fill(cr);
  // Far peaks, pale with distance, their sunward faces pink.
  Rng rng(35501u);
  for (int pass = 0; pass < 2; ++pass)
    for (double x = -120; x < W + 120; x += rng.range(110, 220))
    {
      const double h = rng.range(40, pass == 0 ? 110 : 70), wd = rng.range(120, 240);
      for (const double off : {0.0, -W, W})
      {
        const double px = x + off;
        cairo_move_to(cr, px - wd * 0.5, 430);
        cairo_line_to(cr, px - wd * 0.08, 430 - h);
        cairo_line_to(cr, px + wd * 0.05, 430 - h + 6);
        cairo_line_to(cr, px + wd * 0.5, 430);
        cairo_close_path(cr);
        setRgba(cr, pass == 0 ? rgb(170, 150, 190) : rgb(150, 130, 176), 0.85);
        cairo_fill(cr);
        cairo_move_to(cr, px - wd * 0.08, 430 - h);
        cairo_line_to(cr, px - wd * 0.5 + 10, 430);
        stroke(cr, rgb(255, 196, 190), 2.0, 0.5);
      }
    }
  // The studios: a far city's towers and its broadcast mast, at x 300.
  for (const double off : {0.0, -W, W})
  {
    const double sx = 300 + off;
    for (int k = 0; k < 9; ++k)
    {
      const double bw = 6 + (k * 7) % 9, bh = 10 + (k * 13) % 22;
      cairo_rectangle(cr, sx - 40 + k * 10, 428 - bh, bw, bh);
      setRgba(cr, rgb(120, 104, 150), 0.9);
      cairo_fill(cr);
    }
    cairo_move_to(cr, sx + 6, 428);
    cairo_line_to(cr, sx + 10, 360);
    cairo_line_to(cr, sx + 14, 428);
    setRgba(cr, rgb(110, 96, 140), 0.95);
    cairo_fill(cr);
    circle(cr, sx + 10, 360, 2.5);
    setRgba(cr, rgb(255, 80, 70), 0.9);
    cairo_fill(cr);
  }
  // The cloud sea: rows of billows, each a lavender body with a lit top.
  for (int row = 0; row < 9; ++row)
  {
    const double y = 432 + row * row * 6.5 + row * 12;
    const double rad = 26 + row * 11;
    const Color body = lerpColor(rgb(186, 170, 214), rgb(214, 196, 226), float(row) / 9.0f);
    const Color lit = lerpColor(rgb(255, 214, 186), rgb(255, 236, 214), float(row) / 9.0f);
    for (double x = rng.range(-30, 0); x < W; x += rad * rng.range(1.1f, 1.7f))
      for (const double off : {0.0, -W, W})
      {
        const double px = x + off, py = y + rng.range(-6, 6) * (1 + row * 0.3);
        if (px < -rad * 2 || px > W + rad * 2)
          continue;
        blob(cr, px, py + rad * 0.5, rad * 1.5, rad * 0.9, body, 0.95);
        blob(cr, px - rad * 0.25, py, rad * 1.1, rad * 0.55, lit, 0.85);
      }
  }
  // Haze over the horizon.
  cairo_pattern_t* haze = cairo_pattern_create_linear(0, 380, 0, 450);
  cairo_pattern_add_color_stop_rgba(haze, 0.0, 1.0, 0.86, 0.7, 0.0);
  cairo_pattern_add_color_stop_rgba(haze, 0.5, 1.0, 0.84, 0.67, 0.3);
  cairo_pattern_add_color_stop_rgba(haze, 1.0, 1.0, 0.86, 0.7, 0.0);
  cairo_rectangle(cr, 0, 380, W, 70);
  cairo_set_source(cr, haze);
  cairo_fill(cr);
  cairo_pattern_destroy(haze);
}

// Wisps of cloud drifting past at the mast's height (kWispW x 360, tiles
// side by side): long thin streaks, pink-lit, mostly see-through.
void paintWisps(cairo_t* cr)
{
  const double W = kWispW;
  Rng rng(35511u);
  for (int i = 0; i < 16; ++i)
  {
    const double x = rng.range(0, float(W)), y = rng.range(40, 320), len = rng.range(160, 420);
    for (int k = 0; k < 6; ++k)
    {
      const double px = x + (double(k) - 2.5) * len * 0.16, py = y + std::sin(double(k) + x) * 6;
      for (const double off : {0.0, -W, W})
        blob(cr, px + off, py, len * 0.2, rng.range(8, 18), k % 2 ? rgb(255, 236, 230) : rgb(255, 214, 204), 0.22);
    }
  }
}

// One bay of the far side of the mast, seen through the near side (128 x
// 128, tiles both ways): thin girders in hazy lavender.
void paintBackBay(cairo_t* cr)
{
  const Color c = rgb(190, 170, 200);
  for (const double x : {0.0, 128.0})
  {
    cairo_move_to(cr, x, 0);
    cairo_line_to(cr, x, 128);
  }
  stroke(cr, c, 7.0);
  for (const double y : {0.0, 128.0})
  {
    cairo_move_to(cr, 0, y);
    cairo_line_to(cr, 128, y);
  }
  stroke(cr, c, 5.0);
  cairo_move_to(cr, 0, 0);
  cairo_line_to(cr, 128, 128);
  cairo_move_to(cr, 128, 0);
  cairo_line_to(cr, 0, 128);
  stroke(cr, c, 3.0);
  cairo_move_to(cr, 0, 64);
  cairo_line_to(cr, 128, 64);
  stroke(cr, c, 2.0, 0.6);
}

// A girder from (x0, y0) to (x1, y1), `w` wide: an angle section lit on one
// face, shadowed on the other.
void girder(cairo_t* cr, double x0, double y0, double x1, double y1, double w, Color c)
{
  const double dx = x1 - x0, dy = y1 - y0, len = std::max(0.01, std::sqrt(dx * dx + dy * dy));
  const double nx = -dy / len, ny = dx / len;
  cairo_move_to(cr, x0 + nx * w * 0.5, y0 + ny * w * 0.5);
  cairo_line_to(cr, x1 + nx * w * 0.5, y1 + ny * w * 0.5);
  cairo_line_to(cr, x1 - nx * w * 0.5, y1 - ny * w * 0.5);
  cairo_line_to(cr, x0 - nx * w * 0.5, y0 - ny * w * 0.5);
  cairo_close_path(cr);
  setRgba(cr, kInk, 1.0);
  cairo_set_line_width(cr, 2.0);
  cairo_stroke_preserve(cr);
  cairo_pattern_t* g = cairo_pattern_create_linear(x0 + nx * w * 0.5, y0 + ny * w * 0.5, x0 - nx * w * 0.5, y0 - ny * w * 0.5);
  stop(g, 0.0, lerpColor(c, rgb(255, 255, 255), 0.35f));
  stop(g, 0.45, c);
  stop(g, 0.55, lerpColor(c, rgb(0, 0, 0), 0.3f));
  stop(g, 1.0, lerpColor(c, rgb(0, 0, 0), 0.45f));
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
}

// One bay of the near side (256 x 256, tiles both ways): a leg down each
// side (half of it in this bay, half in the next), a horizontal at top and
// bottom, an X of bracing and a light strut across the middle, gusset
// plates and bolts where they meet. Red or white, in the mast's bands; a
// shade dimmer than the walkways in front of it.
void paintBay(cairo_t* cr, bool red)
{
  const Color c = red ? lerpColor(kRed, rgb(60, 30, 50), 0.12f) : lerpColor(kWhite, rgb(150, 140, 170), 0.2f);
  girder(cr, 12, 12, 244, 244, 11, c);
  girder(cr, 244, 12, 12, 244, 11, c);
  girder(cr, 0, 128, 256, 128, 6, c);
  for (const double y : {0.0, 256.0})
    girder(cr, -4, y, 260, y, 18, c);
  for (const double x : {0.0, 256.0})
    girder(cr, x, -4, x, 260, 26, c);
  // Gussets and bolts.
  for (const double x : {0.0, 256.0})
    for (const double y : {0.0, 128.0, 256.0})
    {
      cairo_rectangle(cr, x - 16, y - 12, 32, 24);
      setRgba(cr, lerpColor(c, rgb(0, 0, 0), 0.15f), 1.0);
      cairo_fill_preserve(cr);
      stroke(cr, kInk, 1.4);
      for (int k = 0; k < 4; ++k)
      {
        circle(cr, x - 9 + (k % 2) * 18, y - 6 + (k / 2) * 12, 2.2);
        setRgba(cr, lerpColor(c, rgb(255, 255, 255), 0.4f), 1.0);
        cairo_fill(cr);
      }
    }
  circle(cr, 128, 128, 9);
  setRgba(cr, lerpColor(c, rgb(0, 0, 0), 0.2f), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.4);
}

// A drum dish face on (150 x 150, centred): a grey rim, a white radome
// cover with a sheen, the mount behind.
void paintDrumDish(cairo_t* cr)
{
  cairo_move_to(cr, 75, 75);
  cairo_line_to(cr, 10, 140);
  cairo_move_to(cr, 75, 75);
  cairo_line_to(cr, 140, 140);
  stroke(cr, kInk, 6.0);
  cairo_move_to(cr, 75, 75);
  cairo_line_to(cr, 10, 140);
  cairo_move_to(cr, 75, 75);
  cairo_line_to(cr, 140, 140);
  stroke(cr, rgb(150, 156, 170), 3.0);
  circle(cr, 75, 75, 62);
  lgrad(cr, 0, 13, 0, 137, {{0.0, rgb(150, 156, 170)}, {1.0, rgb(60, 64, 80)}}, 1.0, true);
  stroke(cr, kInk, 2.4);
  circle(cr, 75, 75, 52);
  {
    cairo_pattern_t* g = cairo_pattern_create_radial(58, 56, 4, 75, 75, 54);
    stop(g, 0.0, rgb(255, 255, 255));
    stop(g, 0.7, rgb(230, 228, 236));
    stop(g, 1.0, rgb(170, 168, 186));
    cairo_set_source(cr, g);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(g);
  }
  stroke(cr, kInk, 1.6);
  for (int k = 0; k < 8; ++k)
  {
    const double a = k * kPi / 4.0;
    cairo_move_to(cr, 75 + std::cos(a) * 54, 75 + std::sin(a) * 54);
    cairo_line_to(cr, 75 + std::cos(a) * 62, 75 + std::sin(a) * 62);
    stroke(cr, rgb(40, 44, 56), 2.0);
  }
  lettering(cr, "MAXTV", 75, 80, 13, rgb(190, 60, 50), 0.7);
}

// A panel antenna (40 x 180): a tall white radome on two brackets, its
// cable looping down.
void paintPanelAntenna(cairo_t* cr)
{
  for (const double y : {40.0, 140.0})
  {
    cairo_rectangle(cr, 0, y - 4, 40, 8);
    setRgba(cr, rgb(110, 116, 130), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.2);
  }
  cairo_move_to(cr, 8, 10);
  cairo_curve_to(cr, 8, 2, 32, 2, 32, 10);
  cairo_line_to(cr, 32, 168);
  cairo_curve_to(cr, 32, 176, 8, 176, 8, 168);
  cairo_close_path(cr);
  lgrad(cr, 8, 0, 32, 0, {{0.0, rgb(255, 255, 255)}, {0.6, rgb(222, 222, 228)}, {1.0, rgb(150, 150, 166)}}, 1.0, true);
  stroke(cr, kInk, 1.8);
  cairo_move_to(cr, 20, 172);
  cairo_curve_to(cr, 22, 182, 34, 178, 36, 180);
  stroke(cr, kInk, 2.4);
}

// The spire on the top deck (160 x 448, its foot's middle at the bottom):
// a tapering lattice in red and white bands, a ladder up its middle, a
// beacon on top; the dish is drawn on it live.
void paintSpire(cairo_t* cr)
{
  const double base = 448, top = 24;
  auto halfAt = [&](double y) { return 18 + (y - top) / (base - top) * 52; };
  for (int band = 0; band < 7; ++band)
  {
    const double y0 = top + band * 60, y1 = std::min(base, y0 + 60);
    const Color c = band % 2 ? kWhite : kRed;
    girder(cr, 80 - halfAt(y0), y0, 80 + halfAt(y1), y1, 6, c);
    girder(cr, 80 + halfAt(y0), y0, 80 - halfAt(y1), y1, 6, c);
    girder(cr, 80 - halfAt(y1) - 4, y1, 80 + halfAt(y1) + 4, y1, 7, c);
  }
  for (int side = -1; side <= 1; side += 2)
    for (int band = 0; band < 7; ++band)
    {
      const double y0 = top + band * 60, y1 = std::min(base, y0 + 60);
      girder(cr, 80 + side * halfAt(y0), y0 - 2, 80 + side * halfAt(y1), y1 + 2, 10, band % 2 ? kWhite : kRed);
    }
  // The beacon housing on top.
  cairo_rectangle(cr, 70, 6, 20, 18);
  lgrad(cr, 70, 0, 90, 0, {{0.0, rgb(200, 60, 50)}, {1.0, rgb(110, 30, 30)}}, 1.0, true);
  stroke(cr, kInk, 1.8);
}

// The big dish on the spire (200 x 200, its pivot at the middle, facing up
// and right at angle 0): a parabolic dish in side view, its feed on struts,
// a back frame.
void paintBigDish(cairo_t* cr)
{
  cairo_translate(cr, 100, 100);
  // The back frame and the pivot.
  cairo_move_to(cr, -10, 0);
  cairo_line_to(cr, -40, -50);
  cairo_move_to(cr, -10, 0);
  cairo_line_to(cr, -40, 50);
  stroke(cr, kInk, 7.0);
  cairo_move_to(cr, -10, 0);
  cairo_line_to(cr, -40, -50);
  cairo_move_to(cr, -10, 0);
  cairo_line_to(cr, -40, 50);
  stroke(cr, rgb(150, 156, 170), 4.0);
  // The bowl, open to the right.
  cairo_move_to(cr, -30, -90);
  cairo_curve_to(cr, -70, -40, -70, 40, -30, 90);
  cairo_curve_to(cr, -50, 40, -50, -40, -30, -90);
  cairo_close_path(cr);
  lgrad(cr, -70, 0, -30, 0, {{0.0, rgb(150, 150, 166)}, {1.0, rgb(255, 255, 255)}}, 1.0, true);
  stroke(cr, kInk, 2.4);
  cairo_move_to(cr, -30, -90);
  cairo_curve_to(cr, -50, -40, -50, 40, -30, 90);
  stroke(cr, kRed, 4.0);
  // The feed on its struts.
  for (const double y : {-84.0, 84.0})
  {
    cairo_move_to(cr, -32, y);
    cairo_line_to(cr, 40, 0);
  }
  stroke(cr, kInk, 4.0);
  for (const double y : {-84.0, 84.0})
  {
    cairo_move_to(cr, -32, y);
    cairo_line_to(cr, 40, 0);
  }
  stroke(cr, rgb(200, 204, 214), 2.0);
  cairo_rectangle(cr, 36, -8, 18, 16);
  lgrad(cr, 0, -8, 0, 8, {{0.0, rgb(240, 240, 246)}, {1.0, rgb(120, 126, 140)}}, 1.0, true);
  stroke(cr, kInk, 1.8);
}

// The alien's flag (64 x 40, its hoist at the left): green, a white alien
// head with two big black eyes.
void paintAlienFlag(cairo_t* cr, int wave)
{
  cairo_move_to(cr, 0, 2);
  for (double x = 0; x <= 60; x += 6)
    cairo_line_to(cr, x, 2 + std::sin(x * 0.12 + wave * 1.6) * 3.0);
  for (double x = 60; x >= 0; x -= 6)
    cairo_line_to(cr, x, 34 + std::sin(x * 0.12 + wave * 1.6) * 3.0);
  cairo_close_path(cr);
  lgrad(cr, 0, 0, 60, 0, {{0.0, rgb(120, 230, 90)}, {0.5, rgb(90, 200, 70)}, {1.0, rgb(60, 160, 50)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  const double dy = std::sin(30 * 0.12 + wave * 1.6) * 3.0;
  ellipse(cr, 30, 17 + dy, 9, 11);
  setRgba(cr, rgb(245, 250, 240), 1.0);
  cairo_fill(cr);
  for (const double ex : {26.0, 34.0})
  {
    ellipse(cr, ex, 16 + dy, 2.6, 4.2);
    setRgba(cr, kInk, 1.0);
    cairo_fill(cr);
  }
}

// The crawl space's back wall (256 x 256, tiles both ways): cast concrete
// in lifts with form-tie holes, a cable tray along the top, a pipe.
void paintCrawlWall(cairo_t* cr)
{
  cairo_rectangle(cr, 0, 0, 256, 256);
  lgrad(cr, 0, 0, 0, 256, {{0.0, rgb(84, 82, 92)}, {1.0, rgb(58, 56, 66)}});
  for (const double y : {64.0, 128.0, 192.0})
  {
    cairo_move_to(cr, 0, y);
    cairo_line_to(cr, 256, y);
    stroke(cr, rgb(40, 38, 46), 2.0);
  }
  for (double x = 32; x < 256; x += 64)
    for (double y = 32; y < 256; y += 64)
    {
      circle(cr, x, y, 3);
      setRgba(cr, rgb(36, 34, 42), 1.0);
      cairo_fill(cr);
    }
  Rng rng(35521u);
  for (int i = 0; i < 90; ++i)
  {
    cairo_rectangle(cr, rng.range(0, 256), rng.range(0, 256), 2, 2);
    setRgba(cr, i % 2 ? rgb(110, 108, 118) : rgb(40, 38, 46), 0.5);
    cairo_fill(cr);
  }
  // The pipe and its brackets.
  cairo_rectangle(cr, 0, 40, 256, 14);
  lgrad(cr, 0, 40, 0, 54, {{0.0, rgb(200, 120, 60)}, {1.0, rgb(110, 60, 30)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  for (const double x : {40.0, 168.0})
  {
    cairo_rectangle(cr, x, 36, 8, 22);
    setRgba(cr, rgb(70, 72, 84), 1.0);
    cairo_fill(cr);
  }
}

// The cargo airship (960 x 380, anchored at the cargo bay's middle under
// its gondola): a long grey-white envelope with a Halcyon blue stripe and
// lettering, tail fins, engine pods with propeller disks, the gondola with
// its open cargo bay and winch.
void paintAirship(cairo_t* cr)
{
  // The envelope.
  ellipse(cr, 480, 140, 440, 120);
  {
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 20, 0, 260);
    stop(g, 0.0, rgb(255, 255, 255));
    stop(g, 0.35, rgb(226, 230, 238));
    stop(g, 0.8, rgb(150, 158, 178));
    stop(g, 1.0, rgb(96, 104, 126));
    cairo_set_source(cr, g);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(g);
  }
  stroke(cr, kInk, 3.0);
  // Its ribs.
  for (int k = 1; k < 9; ++k)
  {
    const double x = 40 + k * 98.0;
    const double h = 120 * std::sqrt(std::max(0.0, 1.0 - std::pow((x - 480) / 440, 2.0)));
    cairo_move_to(cr, x, 140 - h);
    cairo_curve_to(cr, x + 10, 140 - h * 0.4, x + 10, 140 + h * 0.4, x, 140 + h);
    stroke(cr, rgb(160, 168, 186), 1.6, 0.6);
  }
  // The stripe and the lettering.
  cairo_save(cr);
  ellipse(cr, 480, 140, 438, 118);
  cairo_clip(cr);
  cairo_rectangle(cr, 0, 150, 960, 26);
  setRgba(cr, kHalcyon, 1.0);
  cairo_fill(cr);
  cairo_restore(cr);
  lettering(cr, "HALCYON ARMS", 470, 132, 54, rgb(40, 70, 140));
  lettering(cr, "DEFENCE  FOR  A  BRIGHTER  TOMORROW", 470, 200, 16, rgb(60, 70, 100), 0.8);
  // Tail fins.
  for (const double y : {40.0, 240.0})
  {
    cairo_move_to(cr, 120, 140 + (y - 140) * 0.55);
    cairo_line_to(cr, 10, y);
    cairo_line_to(cr, 30, 140 + (y - 140) * 0.2);
    cairo_close_path(cr);
    lgrad(cr, 0, 40, 0, 240, {{0.0, rgb(230, 234, 240)}, {1.0, rgb(130, 138, 156)}}, 1.0, true);
    stroke(cr, kInk, 2.4);
  }
  // Engine pods and their propeller disks.
  for (const double x : {250.0, 700.0})
  {
    cairo_move_to(cr, x, 230);
    cairo_line_to(cr, x + 20, 262);
    stroke(cr, kInk, 6.0);
    ellipse(cr, x + 26, 272, 40, 14);
    lgrad(cr, 0, 258, 0, 286, {{0.0, rgb(150, 156, 170)}, {1.0, rgb(60, 64, 80)}}, 1.0, true);
    stroke(cr, kInk, 2.0);
    ellipse(cr, x - 18, 272, 4, 30);
    setRgba(cr, rgb(200, 210, 230), 0.4);
    cairo_fill(cr);
  }
  // The gondola and the cargo bay, open underneath.
  cairo_move_to(cr, 330, 240);
  cairo_line_to(cr, 630, 240);
  cairo_line_to(cr, 610, 340);
  cairo_line_to(cr, 350, 340);
  cairo_close_path(cr);
  lgrad(cr, 0, 240, 0, 340, {{0.0, rgb(200, 206, 220)}, {1.0, rgb(100, 108, 128)}}, 1.0, true);
  stroke(cr, kInk, 2.6);
  for (int k = 0; k < 6; ++k)
  {
    cairo_rectangle(cr, 350 + k * 42, 258, 28, 18);
    setRgba(cr, rgb(255, 220, 150), 0.9);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.2);
  }
  cairo_rectangle(cr, 400, 330, 160, 30);
  setRgba(cr, rgb(20, 22, 30), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 2.0);
  // The open bay doors hanging down.
  cairo_move_to(cr, 400, 360);
  cairo_line_to(cr, 380, 378);
  cairo_move_to(cr, 560, 360);
  cairo_line_to(cr, 580, 378);
  stroke(cr, rgb(120, 126, 140), 6.0);
  // The winch drum.
  cairo_rectangle(cr, 450, 334, 60, 14);
  setRgba(cr, rgb(250, 200, 40), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.2);
}

// The banner hanging off the airship's stern (240 x 600, anchored at its
// top's middle): a long cloth with the slogan down it.
void paintBanner(cairo_t* cr)
{
  cairo_rectangle(cr, 10, 0, 220, 600);
  lgrad(cr, 10, 0, 230, 0, {{0.0, rgb(230, 236, 250)}, {0.5, rgb(255, 255, 255)}, {1.0, rgb(200, 208, 226)}}, 1.0, true);
  stroke(cr, kInk, 2.4);
  cairo_rectangle(cr, 10, 0, 220, 26);
  setRgba(cr, kHalcyon, 1.0);
  cairo_fill(cr);
  cairo_rectangle(cr, 10, 574, 220, 26);
  setRgba(cr, kHalcyon, 1.0);
  cairo_fill(cr);
  static const char* kLines[] = {"BROUGHT", "TO YOU", "BY", "HALCYON", "ARMS!"};
  for (int k = 0; k < 5; ++k)
    lettering(cr, kLines[k], 120, 110 + k * 100, k == 3 || k == 4 ? 44 : 36, k >= 3 ? rgb(40, 90, 190) : rgb(40, 40, 60));
  cairo_rectangle(cr, 0, 0, 240, 8);
  setRgba(cr, rgb(110, 80, 50), 1.0);
  cairo_fill(cr);
}

// The sales brochure (72 x 56, anchored at its bottom's middle): a glossy
// tri-fold, half open, the Titan striding across its cover.
void paintBrochure(cairo_t* cr)
{
  cairo_move_to(cr, 4, 50);
  cairo_line_to(cr, 24, 8);
  cairo_line_to(cr, 48, 10);
  cairo_line_to(cr, 68, 52);
  cairo_line_to(cr, 46, 54);
  cairo_line_to(cr, 26, 52);
  cairo_close_path(cr);
  setRgba(cr, rgb(250, 252, 255), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.6);
  cairo_move_to(cr, 24, 8);
  cairo_line_to(cr, 26, 52);
  cairo_move_to(cr, 48, 10);
  cairo_line_to(cr, 46, 54);
  stroke(cr, rgb(150, 160, 180), 1.2);
  // The cover: a blue sky band, the Titan in white.
  cairo_move_to(cr, 26, 14);
  cairo_line_to(cr, 47, 15);
  cairo_line_to(cr, 46, 34);
  cairo_line_to(cr, 26, 33);
  cairo_close_path(cr);
  setRgba(cr, kHalcyon, 1.0);
  cairo_fill(cr);
  ellipse(cr, 37, 25, 5, 7);
  setRgba(cr, rgb(255, 255, 255), 1.0);
  cairo_fill(cr);
  circle(cr, 37, 18, 2.6);
  cairo_fill(cr);
  for (int k = 0; k < 3; ++k)
  {
    cairo_move_to(cr, 28, 38 + k * 4);
    cairo_line_to(cr, 44, 38.5 + k * 4);
    stroke(cr, rgb(120, 130, 150), 1.0);
  }
}

// A satellite for the swarm (40 x 28, centred): a gold-foil body, two blue
// solar wings, a tiny dish; grey and dead when offline.
void paintSatellite(cairo_t* cr, bool dead)
{
  const Color panel = dead ? rgb(90, 92, 100) : rgb(40, 90, 200);
  for (const double x : {2.0, 26.0})
  {
    cairo_rectangle(cr, x, 8, 12, 12);
    setRgba(cr, panel, 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.0);
    cairo_move_to(cr, x + 6, 8);
    cairo_line_to(cr, x + 6, 20);
    cairo_move_to(cr, x, 14);
    cairo_line_to(cr, x + 12, 14);
    stroke(cr, dead ? rgb(60, 60, 66) : rgb(140, 200, 255), 0.8);
  }
  cairo_rectangle(cr, 14, 7, 12, 14);
  lgrad(cr, 0, 7, 0, 21, {{0.0, dead ? rgb(170, 170, 170) : rgb(255, 230, 130)}, {1.0, dead ? rgb(90, 90, 96) : rgb(190, 130, 30)}}, 1.0, true);
  stroke(cr, kInk, 1.2);
  cairo_move_to(cr, 20, 7);
  cairo_line_to(cr, 20, 2);
  stroke(cr, kInk, 1.2);
  ellipse(cr, 20, 2, 4, 1.6);
  setRgba(cr, rgb(240, 240, 246), 1.0);
  cairo_fill(cr);
}

// The Satellite Swarm's icon (64 x 64): a chunky handheld uplink with a
// little dish, three satellites round it on an orbit line.
void paintSwarmIcon(cairo_t* cr)
{
  ellipse(cr, 32, 34, 27, 12);
  stroke(cr, kSwarm, 2.0, 0.7);
  // The uplink: a grip and a body with a dish.
  cairo_move_to(cr, 24, 40);
  cairo_line_to(cr, 20, 58);
  cairo_line_to(cr, 30, 58);
  cairo_line_to(cr, 32, 42);
  cairo_close_path(cr);
  setRgba(cr, rgb(60, 64, 80), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.6);
  cairo_rectangle(cr, 16, 28, 30, 16);
  lgrad(cr, 0, 28, 0, 44, {{0.0, rgb(240, 244, 250)}, {1.0, rgb(140, 146, 162)}}, 1.0, true);
  stroke(cr, kInk, 1.8);
  cairo_rectangle(cr, 20, 32, 10, 6);
  setRgba(cr, kSwarm, 1.0);
  cairo_fill(cr);
  ellipse(cr, 46, 24, 5, 10);
  lgrad(cr, 0, 14, 0, 34, {{0.0, rgb(255, 255, 255)}, {1.0, rgb(170, 176, 190)}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  cairo_move_to(cr, 42, 30);
  cairo_line_to(cr, 46, 24);
  stroke(cr, kInk, 1.4);
  for (const auto& p : {std::array<double, 2>{8, 30}, {54, 40}, {30, 10}})
  {
    cairo_save(cr);
    cairo_translate(cr, p[0] - 10, p[1] - 7);
    cairo_scale(cr, 0.5, 0.5);
    paintSatellite(cr, false);
    cairo_restore(cr);
  }
}

// The round tower's sides falling into shadow (1280 x 720): a cool haze
// deepening toward both edges.
void paintSides(cairo_t* cr)
{
  for (int side = 0; side < 2; ++side)
  {
    const double x0 = side == 0 ? 0.0 : double(kScreenW), x1 = side == 0 ? 330.0 : double(kScreenW) - 330.0;
    cairo_pattern_t* g = cairo_pattern_create_linear(x0, 0, x1, 0);
    stop(g, 0.0, rgb(16, 12, 40), 0.72);
    stop(g, 0.35, rgb(24, 18, 50), 0.4);
    stop(g, 0.7, rgb(30, 24, 60), 0.12);
    stop(g, 1.0, rgb(30, 24, 60), 0.0);
    cairo_rectangle(cr, std::min(x0, x1), 0, 330, kScreenH);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
}

// --- Level 35's layout ---------------------------------------------------------------------------------

// The camera's x with the tower's turn taken off: the tower's own frame.
float towerCamX(const World& w, float camX)
{
  return w.tower().wrap ? camX - float(w.tower().rot) * kCellPx : camX;
}

// A spot in the tower's own frame (px) on the screen, the nearest way
// round.
float towerScreenX(const World& w, float ux, float camXu)
{
  const float x = ux - camXu;
  return w.tower().wrap ? wrapFrom(x, kLapPx, float(kScreenW) * 0.5f - kLapPx * 0.5f) : x;
}

// The main level's rows: the top deck's surface, the base ring's.
int deckRow(const World& w)
{
  const auto& t = w.tower().titan;
  return t.on ? (t.deckY) / kCellsPerTile : -1;
}

bool isMain(const World& w) { return w.tower().wrap; }
constexpr int kBaseRow = 102;
constexpr int kCrawlX0 = 20, kCrawlX1 = 43, kCrawlY0 = 106, kCrawlY1 = 109; // blocks, the tower's own x

// Where something on the screen is relative to the runner (px), through
// the shortest way round.
float cellScreenX(const World& w, int cellX, float camX)
{
  const auto& p = w.player();
  return float(p.x + w.towerDx(p.x, cellX)) * kCellPx - camX;
}

// Where the Titan is drawn while the airship lowers it (its feet's row):
// from 8 rows over the deck down onto it (the logic starts it higher, out
// of sight; this keeps the drop on the screen).
float introY(const Titan& t, float alpha)
{
  const float u = std::clamp((float(t.t) - 1.0f + alpha) / float(kTitanIntro - 10), 0.0f, 1.0f);
  return float(t.deckY) - 8.0f * (1.0f - u) * (1.0f - u * 0.4f);
}

// The Halcyon Titan while it is round the back of the mast, seen through
// the lattice (smaller, hazed, moving the far side's way), and in the
// intro the cargo airship lowering it there on its cables.
void drawFarTitan(Renderer& r, const World& w, const Art& art, float camX, float camY, float alpha)
{
  const auto& t = w.tower().titan;
  if (!t.on || t.phase == TitanPhase::Waiting || t.phase == TitanPhase::Done)
    return;
  const auto& p = w.player();
  const int lap = w.level().width * kCellsPerTile;
  const int d = w.towerDx(p.x + 1, t.x + kTitanW / 2);
  if (std::abs(d) <= lap / 4)
    return; // on the near side: drawTowerFront has it
  // Straight behind the runner is the screen's middle; the far side slides
  // the other way at half speed, like its lattice.
  const bool falling = t.phase == TitanPhase::Topple && t.fallT > 0;
  const float fx = float(t.x) - (falling ? float(t.facing) * (1.0f - alpha) : 0.0f);
  const float mid = (fx + float(kTitanW) * 0.5f) * kCellPx - camX;
  const float x = float(kScreenW) * 0.5f - 0.5f * wrapFrom(mid - kLapPx * 0.5f - float(kScreenW) * 0.5f, kLapPx, -kLapPx * 0.5f);
  const float ty = t.phase == TitanPhase::Intro ? introY(t, alpha)
                                                : float(t.y) - (falling ? float(1 + t.fallT / 4) * (1.0f - alpha) : 0.0f);
  const float s = 0.7f;
  const float foot = (ty + 1.0f) * kCellPx - camY;
  if (offScreen(x - 600.0f, foot - 600.0f, 1200.0f, 640.0f))
    return;
  const int f = t.facing < 0 ? 1 : -1; // seen from behind
  const std::string themeKey = w.level().themeKey;
  const int ti = themeIndexForKey(themeKey.c_str());
  const Theme& theme = themeByIndex(ti >= 0 ? ti : 0);
  DrawOpts o;
  o.scale = s;
  o.tint = rgb(176, 184, 214);
  o.alpha = 0.92f;
  const Sprite& body = styledEnemySprite(art, r, theme, "halcyon_titan", 0, 0, kTitanW, kTitanH);
  if (t.phase == TitanPhase::Intro)
  {
    // Lowered on long cables from the cargo airship, which crosses over
    // the top of the screen (nearer than the Titan: it hangs off the far
    // side of the mast) and leaves once it has set it down.
    const float k = float(t.ship) + alpha;
    const float in = std::clamp(k / 24.0f, 0.0f, 1.0f), out = std::clamp((k - 80.0f) / 10.0f, 0.0f, 1.0f);
    const float ease = 1.0f - (1.0f - in) * (1.0f - in);
    for (const float cx : {-50.0f, 50.0f})
      beamLine(r, x + cx * s * 1.6f, -40.0f, x + cx * s, foot - 330.0f * s, 1.5f, rgb(70, 70, 86));
    r.draw(body.get(f), x, foot, o);
    DrawOpts so;
    so.scale = 0.8f;
    const float sx = x + (1.0f - ease) * 1500.0f - out * out * 1500.0f;
    const float sy = 200.0f + std::sin(k * 0.08f) * 4.0f - out * 120.0f;
    r.draw(baked(art, r, "airship", 960, 380, 480, 360, paintAirship), sx, sy, so);
    if ((int(k) / 3) % 2 == 0)
      drawGlow(r, art, sx - 440.0f * so.scale, sy - 220.0f * so.scale, 22.0f, rgb(255, 60, 40), 0.8f);
    return;
  }
  if (t.phase == TitanPhase::Topple)
  {
    // Teetering, then over the edge on the far side.
    const float tt = float(t.t) + alpha;
    if (tt <= float(kTeeter))
      o.angle = std::sin(tt * 0.9f) * 7.0f * (tt / float(kTeeter)) * float(f);
    else
      o.angle = float(f) * std::min(100.0f, (float(t.fallT) + alpha) * 3.5f);
    r.draw(body.get(f), x, foot, o);
    return;
  }
  r.draw(body.get(f), x, foot, o);
  // Its reactor pack, glowing on its back (which is this side of it).
  drawGlow(r, art, x + float(f) * (kTitanPackX - 160.0f) * s, foot - (384.0f - kTitanPackY) * s, 40.0f, rgb(255, 150, 60),
    t.phase == TitanPhase::Orbit ? 0.6f : 0.25f);
}

} // namespace

// --- Behind the tiles --------------------------------------------------------------------------------

void drawTowerSet(Renderer& r, const World& w, const Art& art, float camX, float camY, int frame, float alpha)
{
  (void)alpha;
  const auto& s = w.tower();
  const bool main = isMain(w);
  const float camXu = towerCamX(w, camX);
  const int lh = w.level().height;
  const float maxCamY = std::max(1.0f, float(lh) * kTilePx - float(kScreenH));
  const int deck = deckRow(w);
  const float deckY = deck >= 0 ? float(deck) * kTilePx - camY : -1e6f;
  const float baseY = (main ? float(kBaseRow) : float(lh)) * kTilePx - camY;

  // The cloud sea and the sun, sinking as you climb.
  {
    const float horizon = 520.0f - std::min(camY, maxCamY) * 0.035f - (main ? 0.0f : 150.0f);
    const Texture& pano = baked(art, r, "pano", int(kPanoW), 900, 0, 0, paintPanorama);
    const float ox = wrapFrom(camXu * 0.375f, kPanoW, 0.0f);
    for (float x = -ox; x < float(kScreenW); x += kPanoW)
      r.draw(pano, x, horizon - 420.0f);
    const float below = horizon - 420.0f + 900.0f;
    if (below < float(kScreenH))
      r.fillRect(0, below, float(kScreenW), float(kScreenH) - below, rgb(214, 196, 226));
  }
  // Wisps drifting past.
  {
    const Texture& wisps = baked(art, r, "wisps", int(kWispW), 360, 0, 0, paintWisps);
    const float ox = wrapFrom(camXu * 0.5f + float(frame) * 0.35f, kWispW, 0.0f);
    const float period = 1500.0f;
    const float oy = wrapFrom(-camY * 0.18f, period, 0.0f);
    DrawOpts wo;
    wo.alpha = 0.9f;
    for (float y = oy - period; y < float(kScreenH); y += period)
      if (y + 360.0f > 0.0f)
        for (float x = -ox; x < float(kScreenW); x += kWispW)
          r.draw(wisps, x, y, wo);
  }
  // Wind: thin streaks blowing across.
  for (int i = 0; i < 9; ++i)
  {
    const float u = unit(hash2(i, 3551));
    const float x = wrapFrom(float(kScreenW) + 200.0f - float(frame) * (5.0f + 4.0f * u) - u * 3000.0f, float(kScreenW) + 400.0f, -200.0f);
    const float y = unit(hash2(i, 3552)) * float(kScreenH);
    r.drawLine(x, y, x + 90.0f + 80.0f * u, y - 3.0f, 2.0f, rgba(255, 255, 255, 40));
  }

  if (main)
    drawFarTitan(r, w, art, camX, camY, alpha);

  // The tower's body: from the top deck (or the top of the bonus room)
  // down to the base ring.
  const float bodyTop = main ? std::max(0.0f, deckY) : 0.0f;
  const float bodyBottom = std::min(float(kScreenH), baseY);
  if (bodyBottom > bodyTop)
  {
    // The far side's lattice, turning the other way.
    const Texture& back = baked(art, r, "back_bay", 128, 128, 0, 0, paintBackBay);
    DrawOpts bo;
    bo.alpha = 0.45f;
    const float ox = wrapFrom(-camXu * 0.5f, 128.0f, 0.0f), oy = wrapFrom(camY * 0.9f, 128.0f, 0.0f);
    for (float y = -oy; y < bodyBottom; y += 128.0f)
      if (y + 128.0f > bodyTop)
        for (float x = -ox; x < float(kScreenW); x += 128.0f)
          r.draw(back, x, y, bo);
    // A darker core of the tower behind the near lattice.
    r.fillRect(0, bodyTop, float(kScreenW), bodyBottom - bodyTop, rgba(40, 30, 70, 46));

    // The near side's bays, red and white bands from the deck down.
    const Texture& redBay = baked(art, r, "bay_red", 256, 256, 0, 0, [](cairo_t* cr) { paintBay(cr, true); });
    const Texture& whiteBay = baked(art, r, "bay_white", 256, 256, 0, 0, [](cairo_t* cr) { paintBay(cr, false); });
    const float originY = main ? float(deck) * kTilePx : 0.0f;
    const int row0 = int(std::floor((camY - originY) / kBayPx)) - 1;
    const float xs = towerScreenX(w, 0.0f, camXu);
    const float x0 = wrapFrom(xs, kBayPx, -kBayPx);
    for (int row = std::max(0, row0); row <= row0 + 5; ++row)
    {
      const float y = originY + float(row) * kBayPx - camY;
      if (y > bodyBottom || y + kBayPx < bodyTop)
        continue;
      const Texture& bay = row % 2 == 0 ? redBay : whiteBay;
      for (float x = x0; x < float(kScreenW) + kBayPx; x += kBayPx)
        r.draw(bay, x, y);
      // The warning lights on the legs at each red band's top, blinking.
      if (row % 2 == 0 && (frame / 30) % 2 == 0)
        for (float x = x0; x < float(kScreenW) + kBayPx; x += kBayPx)
        {
          const int leg = int(std::lround((x - xs) / kBayPx));
          if (((leg % 2) + 2) % 2 != 0)
            continue;
          r.fillRect(x - 7.0f, y + 14.0f, 14.0f, 10.0f, rgb(255, 60, 40));
          drawGlow(r, art, x, y + 19.0f, 40.0f, rgb(255, 50, 40), 0.75f);
        }
    }

    // Dishes and panel antennas on the legs, all round the dish level (and
    // a few elsewhere).
    if (main)
    {
      const Texture& drum = baked(art, r, "drum_dish", 150, 150, 75, 75, paintDrumDish);
      const Texture& panel = baked(art, r, "panel_antenna", 40, 180, 20, 90, paintPanelAntenna);
      struct Mount
      {
        float ux, row;
        int kind; // 0 drum face on, 1 drum turned, 2 a cluster of panels
      };
      static const std::array<Mount, 22> kMounts = [] {
        std::array<Mount, 22> m{};
        int n = 0;
        for (int i = 0; i < 8; ++i)
          m[std::size_t(n++)] = {float(i) * 512.0f + 128.0f, 69.5f, i % 2};
        for (int i = 0; i < 8; ++i)
          m[std::size_t(n++)] = {float(i) * 512.0f + 384.0f, 57.5f, 2};
        for (int i = 0; i < 3; ++i)
          m[std::size_t(n++)] = {float(i) * 1365.0f + 640.0f, 91.0f, 1};
        for (int i = 0; i < 3; ++i)
          m[std::size_t(n++)] = {float(i) * 1365.0f + 300.0f, 33.0f, 0};
        return m;
      }();
      for (const auto& mt : kMounts)
      {
        const float x = towerScreenX(w, mt.ux, camXu), y = mt.row * kTilePx - camY;
        if (offScreen(x - 100.0f, y - 120.0f, 200.0f, 240.0f))
          continue;
        if (mt.kind == 2)
        {
          for (int k = -1; k <= 1; ++k)
          {
            DrawOpts po;
            po.scale = k == 0 ? 1.0f : 0.85f;
            po.tint = k == 0 ? rgb(255, 255, 255) : rgb(200, 196, 214);
            r.draw(panel, x + float(k) * 34.0f, y, po);
          }
        }
        else
        {
          DrawOpts d;
          d.scale = mt.kind == 0 ? 1.0f : 0.8f;
          d.tint = mt.kind == 0 ? rgb(255, 255, 255) : rgb(214, 210, 226);
          r.draw(drum, x, y, d);
        }
      }
    }
  }

  // The top deck: its guard rail along the back, the spire, the dish, the
  // alien's flag.
  if (main && deck >= 0 && deckY > -200.0f && deckY < float(kScreenH) + 520.0f)
  {
    const auto& m = w.map();
    const int bx0 = std::max(0, int(camX / kTilePx) - 1), bx1 = std::min(w.level().width - 1, int((camX + float(kScreenW)) / kTilePx) + 1);
    for (int bx = bx0; bx <= bx1; ++bx)
    {
      if (m.block(bx, deck) != Tile::Solid || m.block(bx, deck - 1) != Tile::Empty)
        continue;
      const float x = float(bx) * kTilePx - camX;
      r.fillRect(x, deckY - 56.0f, 64.0f, 6.0f, rgb(236, 234, 228));
      r.fillRect(x, deckY - 30.0f, 64.0f, 4.0f, rgb(200, 196, 196));
      r.fillRect(x + 4.0f, deckY - 56.0f, 6.0f, 56.0f, rgb(190, 60, 50));
      r.fillRect(x + 4.0f, deckY - 56.0f, 2.0f, 56.0f, rgb(240, 120, 100));
    }
    if (s.flagX >= 0)
    {
      const float fx = cellScreenX(w, s.flagX, camX) + 16.0f;
      if (!offScreen(fx - 200.0f, deckY - 520.0f, 400.0f, 520.0f))
      {
        r.draw(baked(art, r, "spire", 160, 448, 80, 448, paintSpire), fx, deckY);
        // The beacon on top.
        if ((frame / 30) % 2 == 0)
          drawGlow(r, art, fx, deckY - 438.0f, 50.0f, rgb(255, 60, 40), 0.9f);
        // The big dish: idle, nodding a little; after the Titan falls it
        // swings round to the studios.
        const auto& t = s.titan;
        float swing = 0.0f;
        if (t.on && t.phase == TitanPhase::Done)
          swing = t.t < 81 ? 1.0f : std::clamp(float(t.t - 81) / 30.0f, 0.0f, 1.0f);
        const float ease = swing * swing * (3.0f - 2.0f * swing);
        DrawOpts dop;
        dop.angle = (-120.0f + 2.0f * std::sin(float(frame) * 0.02f)) * (1.0f - ease) + (-8.0f) * ease;
        const float dx = fx + 6.0f, dy = deckY - 260.0f;
        r.draw(baked(art, r, "big_dish", 200, 200, 100, 100, paintBigDish), dx, dy, dop);
        if (swing >= 1.0f)
        {
          // The signal home: a beam off the dish to the horizon.
          const float a = dop.angle * float(kPi) / 180.0f;
          const float bx = dx + std::cos(a) * 50.0f, by = dy + std::sin(a) * 50.0f;
          const float pulse = 0.6f + 0.4f * std::sin(float(frame) * 0.2f);
          for (int k = 0; k < 3; ++k)
            r.drawLine(bx, by, bx + std::cos(a) * 2000.0f, by + std::sin(a) * 2000.0f, 26.0f - float(k) * 8.0f,
              rgba(140, 230, 255, int(50.0f * pulse) + k * 40), Blend::Add);
        }
        // The flag on its pole.
        const float top = deckY - 448.0f;
        beamLine(r, fx, top + 10.0f, fx, top - 70.0f, 3.0f, rgb(220, 220, 230));
        const int wave = (frame / 6) % 4;
        r.draw(baked(art, r, "flag" + std::to_string(wave), 64, 40, 0, 0, [wave](cairo_t* cr) { paintAlienFlag(cr, wave); }), fx + 2.0f,
          top - 70.0f);
        if (s.anthem || s.flagT > 0)
          for (int k = 0; k < 3; ++k)
          {
            const float u = std::fmod(float(frame) * 0.01f + float(k) / 3.0f, 1.0f);
            r.drawText(k % 2 ? "~" : "*", fx + 40.0f + std::sin(u * 7.0f) * 14.0f, top - 50.0f - u * 80.0f,
              {22.0f, rgba(140, 255, 120, int(255.0f * (1.0f - u))), kHudInk}, Align::Center);
          }
      }
    }
  }

  // The crawl space's walls, under the base ring.
  if (main)
  {
    const float top = float(kCrawlY0) * kTilePx - camY;
    if (top < float(kScreenH) && top + float(kCrawlY1 - kCrawlY0 + 1) * kTilePx > 0.0f)
    {
      const Texture& wall = baked(art, r, "crawl_wall", 256, 256, 0, 0, paintCrawlWall);
      const float x0 = towerScreenX(w, float(kCrawlX0) * kTilePx, camXu);
      const float wpx = float(kCrawlX1 - kCrawlX0 + 1) * kTilePx, hpx = float(kCrawlY1 - kCrawlY0 + 1) * kTilePx;
      for (float y = top; y < top + hpx; y += 256.0f)
        for (float x = x0; x < x0 + wpx; x += 256.0f)
          r.draw(wall, x, y);
      // Bare bulbs on the ceiling, their light pooled on the wall.
      for (int k = 0; k < 4; ++k)
      {
        const float bx = x0 + 160.0f + float(k) * 400.0f;
        r.drawLine(bx, top, bx, top + 30.0f, 2.0f, rgb(30, 30, 36));
        r.fillRect(bx - 5.0f, top + 30.0f, 10.0f, 12.0f, rgb(255, 240, 190));
        drawGlow(r, art, bx, top + 50.0f, 160.0f, rgb(255, 210, 140), 0.35f + 0.03f * float(hash2(k, frame / 5) % 3u));
      }
    }
  }
}

// --- Behind the runner -----------------------------------------------------------------------------------

void World::drawTowerBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& s = mTower;
  const auto& p = mPlayer;
  // The hatches: a steel door in a yellow and black frame, a porthole, a
  // lamp that lights when you are at it, an arrow up.
  for (const auto& d : s.doors)
  {
    const float x = cellScreenX(*this, d.x, camX), foot = float(d.y + 1) * kCellPx - camY;
    if (offScreen(x - 20.0f, foot - 200.0f, 110.0f, 200.0f))
      continue;
    r.draw(baked(mArt, r, "hatch", 96, 208, 16, 208,
             [](cairo_t* cr) {
               cairo_rectangle(cr, 4, 6, 88, 202);
               setRgba(cr, rgb(30, 28, 36), 1.0);
               cairo_fill(cr);
               cairo_save(cr);
               cairo_rectangle(cr, 4, 6, 88, 202);
               cairo_rectangle(cr, 14, 16, 68, 192);
               cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
               cairo_clip(cr);
               for (double k = -200; k < 220; k += 16)
               {
                 cairo_move_to(cr, 0, k);
                 cairo_line_to(cr, 96, k + 96);
                 cairo_line_to(cr, 96, k + 104);
                 cairo_line_to(cr, 0, k + 8);
                 cairo_close_path(cr);
               }
               setRgba(cr, rgb(250, 200, 30), 1.0);
               cairo_fill(cr);
               cairo_restore(cr);
               cairo_rectangle(cr, 4, 6, 88, 202);
               stroke(cr, kInk, 2.0);
               cairo_rectangle(cr, 16, 18, 64, 190);
               lgrad(cr, 16, 0, 80, 0, {{0.0, rgb(170, 176, 190)}, {0.5, rgb(130, 136, 150)}, {1.0, rgb(80, 84, 98)}}, 1.0, true);
               stroke(cr, kInk, 2.0);
               circle(cr, 48, 70, 14);
               setRgba(cr, rgb(40, 60, 90), 1.0);
               cairo_fill_preserve(cr);
               stroke(cr, kInk, 3.0);
               cairo_move_to(cr, 42, 64);
               cairo_line_to(cr, 46, 60);
               stroke(cr, rgb(200, 230, 255), 1.6, 0.8);
               cairo_rectangle(cr, 66, 120, 8, 26);
               setRgba(cr, rgb(60, 64, 76), 1.0);
               cairo_fill_preserve(cr);
               stroke(cr, kInk, 1.2);
               for (const double y : {40.0, 180.0})
               {
                 cairo_rectangle(cr, 14, y - 8, 6, 16);
                 setRgba(cr, rgb(90, 94, 108), 1.0);
                 cairo_fill(cr);
               }
             }),
      x, foot);
    const bool near = std::abs(towerDx(d.x, p.x)) <= 2 && std::abs(p.y - d.y) <= 2;
    const Color lamp = near ? rgb(120, 255, 140) : rgb(255, 180, 60);
    r.fillRect(x + 24.0f, foot - 216.0f, 16.0f, 8.0f, lamp);
    drawGlow(r, mArt, x + 32.0f, foot - 212.0f, near ? 50.0f : 26.0f, lamp, near ? 0.9f : 0.5f);
    if (near)
    {
      const float bob = std::sin(float(frame) * 0.15f) * 6.0f;
      r.drawText("UP", x + 32.0f, foot - 270.0f + bob, {22.0f, rgb(140, 255, 160), kHudInk}, Align::Center);
    }
  }
  // Copycat pits: a gap in the girder with hazard paint on its lips and a
  // drop-zone arrow glowing down into it.
  for (const auto& pit : s.pits)
  {
    const float x = float(pit.x) * kCellPx - camX, y = float(pit.y) * kCellPx - camY;
    const float w = float(pit.w) * kCellPx;
    if (offScreen(x - 40.0f, y - 120.0f, w + 80.0f, 240.0f))
      continue;
    for (const float lx : {x - 14.0f, x + w})
    {
      r.fillRect(lx, y, 14.0f, 18.0f, rgb(250, 200, 30));
      for (int k = 0; k < 3; ++k)
        r.fillRect(lx, y + float(k) * 6.0f, 14.0f, 3.0f, kInk);
    }
    const float pulse = 0.5f + 0.5f * std::sin(float(frame) * 0.15f);
    drawGlow(r, mArt, x + w * 0.5f, y + 20.0f, 70.0f, rgb(255, 210, 80), 0.25f + 0.2f * pulse);
    const float ay = y - 70.0f + pulse * 10.0f;
    r.fillRect(x + w * 0.5f - 6.0f, ay, 12.0f, 26.0f, rgba(255, 220, 90, 200));
    for (int k = 0; k < 10; ++k)
      r.fillRect(x + w * 0.5f - 18.0f + float(k) * 1.8f, ay + 26.0f + float(k) * 1.8f, 36.0f - float(k) * 3.6f, 2.0f,
        rgba(255, 220, 90, 200));
  }
  // The swarm's drones on the far side of their orbit, smaller and dimmer.
  if (p.weapon == Weapon::Proto && ProtoId(p.proto) == ProtoId::SatelliteSwarm)
  {
    const float cx = (lerpF(p.prevX, p.x, alpha) + 1.5f) * kCellPx - camX, cy = (lerpF(p.prevY, p.y, alpha) - 2.0f) * kCellPx - camY;
    for (int i = 0; i < kSwarmDrones; ++i)
    {
      const float a = 6.2831853f * (std::fmod(float(s.swarmT) - 1.0f + alpha, float(kSwarmLap)) / float(kSwarmLap) + float(i) / float(kSwarmDrones));
      if (std::sin(a) >= 0.0f)
        continue;
      const bool dead = s.swarm[std::size_t(i)].offline > 0;
      DrawOpts o;
      o.scale = 0.8f + 0.2f * (1.0f + std::sin(a)) * 0.5f;
      o.tint = rgb(190, 190, 210);
      const float dx = cx + float(kSwarmRadius) * kCellPx * std::cos(a), dy = cy + float(kSwarmRadius) * 0.6f * kCellPx * std::sin(a);
      r.draw(baked(mArt, r, dead ? "sat_dead" : "sat", 40, 28, 20, 14, [dead](cairo_t* cr) { paintSatellite(cr, dead); }), dx, dy, o);
    }
  }
}

// --- In front ------------------------------------------------------------------------------------------

void World::drawTowerFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& s = mTower;
  const auto& p = mPlayer;

  // Mast Climbers (and the Copycat's dummies), Relay Drones, Feedback Nodes.
  for (std::size_t i = 0; i < mEnemies.size(); ++i)
  {
    const Enemy& e = mEnemies[i];
    if (!e.alive || e.hidden)
      continue;
    const float cx = (e.drawX + float(e.w) * 0.5f) * kCellPx - camX, foot = (e.drawY + 1.0f) * kCellPx - camY;
    const float h = float(e.h) * kCellPx;
    if (offScreen(cx - 120.0f, foot - h - 120.0f, 240.0f, h + 160.0f))
      continue;
    const int dir = e.dir < 0 ? -1 : 1;
    const Sprite* sp = nullptr;
    float lift = 0.0f;
    if (e.kind == EnemyKind::MastClimber)
    {
      int v = 0, f = ((e.x % 2) + 2) % 2;
      if (s.copycat)
      {
        const bool air = i < s.dummyFall.size() && (s.dummyFall[i] > 0 || s.dummyVy[i] > 0);
        v = air ? 5 : 4;
      }
      else
      {
        const Climber* c = nullptr;
        for (const auto& cc : s.climbers)
          if (cc.enemy == int(i))
            c = &cc;
        if (c && c->climb != 0)
        {
          v = 3;
          f = ((e.y / 2) % 2 + 2) % 2;
        }
        else if (c && c->fall >= 2)
          v = 6;
        else if (e.tell > 0)
          v = 1;
        else if (e.cool > enemyDef(e.def).cooldown - 4)
          v = 2;
      }
      sp = &styledEnemySprite(mArt, r, mTheme, "mast_climber", v, f, e.w, e.h);
      r.draw(sp->get(dir), cx, foot);
      if (v == 2)
      {
        // The swing's arc.
        const float k = float(e.cool - (enemyDef(e.def).cooldown - 4)) / 4.0f;
        for (int q = 0; q < 6; ++q)
        {
          const float a0 = -1.4f + float(q) * 0.38f, a1 = a0 + 0.38f;
          r.drawLine(cx + float(dir) * (20.0f + std::cos(a0) * 90.0f), foot - 90.0f + std::sin(a0) * 90.0f,
            cx + float(dir) * (20.0f + std::cos(a1) * 90.0f), foot - 90.0f + std::sin(a1) * 90.0f, 6.0f,
            rgba(255, 255, 255, int(150.0f * k)), Blend::Add);
        }
      }
      if (v == 1 && (frame / 4) % 2 == 0)
        r.drawText("!", cx, foot - h - 30.0f, {30.0f, rgb(255, 230, 90), kHudInk}, Align::Center);
    }
    else if (e.kind == EnemyKind::RelayDrone)
    {
      const int v = e.tell > 0 ? 1 : 0;
      lift = std::sin(float(frame) * 0.12f + float(i) * 1.7f) * 5.0f;
      sp = &styledEnemySprite(mArt, r, mTheme, "relay_drone", v, (frame / 2) % 4, e.w, e.h);
      r.draw(sp->get(dir), cx, foot + lift);
      if (v == 1)
      {
        // The muzzle charging, brighter toward the shot.
        const float k = 1.0f - float(e.tell) / float(kRelayTell);
        const float mx = cx + float(dir) * 60.0f, my = foot + lift - 30.0f;
        drawGlow(r, mArt, mx, my, 20.0f + 40.0f * k, rgb(255, 120, 80), 0.5f + 0.4f * k);
        if ((frame / 2) % 2 == 0)
          drawGlow(r, mArt, mx, my, 12.0f, rgb(255, 255, 230), 0.9f);
      }
      // The downwash under it.
      for (int k = 0; k < 3; ++k)
      {
        const float u = std::fmod(float(frame) * 0.05f + float(k) / 3.0f, 1.0f);
        r.fillRect(cx - 40.0f + u * 4.0f, foot + lift + 10.0f + u * 60.0f, 80.0f - u * 8.0f, 2.0f, rgba(255, 255, 255, int(60.0f * (1.0f - u))));
      }
    }
    else if (e.kind == EnemyKind::FeedbackNode)
    {
      const FeedbackNode* n = nullptr;
      for (const auto& nn : s.nodes)
        if (nn.enemy == int(i))
          n = &nn;
      const int state = n ? n->state : 0, t = n ? n->t : 0;
      int v = 0;
      if (state == 1)
        v = 1;
      else if (state == 2)
        v = 2;
      else if (state == 3 && t < kNodeCool / 2)
        v = 3;
      sp = &styledEnemySprite(mArt, r, mTheme, "feedback_node", v, (frame / 3) % 2, e.w, e.h);
      // Its hum: the air round the sphere shimmering.
      const float sy = foot - h + 30.0f;
      if (state == 1)
      {
        const float k = std::min(1.0f, (float(t) + alpha) / float(kNodeTell));
        drawGlow(r, mArt, cx, sy, 60.0f + 120.0f * k, rgb(220, 240, 255), 0.25f + 0.5f * k);
        // The coming ring's reach, faint.
        const float rad = float(kNodeRadius) * kCellPx;
        const int segs = 40;
        for (int q = 0; q < segs; q += 2)
        {
          const float a0 = 6.2831853f * float(q) / float(segs) + float(frame) * 0.01f, a1 = a0 + 6.2831853f / float(segs);
          r.drawLine(cx + rad * std::cos(a0), sy + rad * std::sin(a0), cx + rad * std::cos(a1), sy + rad * std::sin(a1), 2.0f,
            rgba(220, 240, 255, int(90.0f * k)), Blend::Add);
        }
      }
      r.draw(sp->get(1), cx, foot);
      if (state == 1)
      {
        DrawOpts wo;
        wo.blend = Blend::Add;
        wo.alpha = 0.4f * std::min(1.0f, float(t) / float(kNodeTell)) * ((frame / 2) % 2 ? 1.0f : 0.6f);
        r.draw(sp->get(1), cx, foot, wo);
      }
      if (state == 2)
      {
        // The ring going out: a bright shock front, a wake behind it.
        const float k = std::min(1.0f, (float(t) + alpha + 1.0f) / 8.0f);
        const float rad = float(kNodeRadius) * kCellPx * k;
        for (int ring = 0; ring < 3; ++ring)
        {
          const float rr = rad - float(ring) * 22.0f;
          if (rr <= 4.0f)
            continue;
          const int segs = 48;
          for (int q = 0; q < segs; ++q)
          {
            const float a0 = 6.2831853f * float(q) / float(segs), a1 = 6.2831853f * float(q + 1) / float(segs);
            r.drawLine(cx + rr * std::cos(a0), sy + rr * std::sin(a0), cx + rr * std::cos(a1), sy + rr * std::sin(a1),
              ring == 0 ? 8.0f : 4.0f, rgba(200, 240, 255, int((ring == 0 ? 220.0f : 110.0f) * (1.0f - k * 0.6f))), Blend::Add);
          }
        }
        drawGlow(r, mArt, cx, sy, rad * 0.8f, rgb(180, 230, 255), 0.25f * (1.0f - k));
      }
    }
    if (sp && e.flash > 0 && (frame / 2) % 2 == 0)
    {
      DrawOpts wo;
      wo.blend = Blend::Add;
      wo.alpha = 0.55f;
      r.draw(sp->get(e.kind == EnemyKind::FeedbackNode ? 1 : dir), cx, foot + lift, wo);
    }
  }

  // A Feedback Node's push on the runner: streaks blowing past.
  if (s.pushT > 0)
  {
    const float px = (lerpF(p.prevX, p.x, alpha) + 1.5f) * kCellPx - camX, py = (lerpF(p.prevY, p.y, alpha) - 2.0f) * kCellPx - camY;
    for (int k = 0; k < 6; ++k)
    {
      const float y = py - 70.0f + float(k) * 28.0f;
      const float len = 60.0f + 30.0f * float((k * 5 + frame) % 3);
      r.drawLine(px - float(s.pushDir) * 30.0f, y, px - float(s.pushDir) * (30.0f + len), y, 3.0f, rgba(220, 240, 255, 140), Blend::Add);
    }
  }

  // The swarm's drones on the near side of their orbit.
  if (p.weapon == Weapon::Proto && ProtoId(p.proto) == ProtoId::SatelliteSwarm)
  {
    const float cx = (lerpF(p.prevX, p.x, alpha) + 1.5f) * kCellPx - camX, cy = (lerpF(p.prevY, p.y, alpha) - 2.0f) * kCellPx - camY;
    for (int i = 0; i < kSwarmDrones; ++i)
    {
      const float a = 6.2831853f * (std::fmod(float(s.swarmT) - 1.0f + alpha, float(kSwarmLap)) / float(kSwarmLap) + float(i) / float(kSwarmDrones));
      if (std::sin(a) < 0.0f)
        continue;
      const bool dead = s.swarm[std::size_t(i)].offline > 0;
      const float dx = cx + float(kSwarmRadius) * kCellPx * std::cos(a), dy = cy + float(kSwarmRadius) * 0.6f * kCellPx * std::sin(a);
      if (!dead)
        drawGlow(r, mArt, dx, dy, 34.0f, kSwarm, 0.45f);
      DrawOpts o;
      o.scale = 1.0f + 0.15f * std::sin(a);
      r.draw(baked(mArt, r, dead ? "sat_dead" : "sat", 40, 28, 20, 14, [dead](cairo_t* cr) { paintSatellite(cr, dead); }), dx, dy, o);
      if (dead && (hash2(i, frame / 3) % 3u) == 0u)
        r.fillRect(dx - 10.0f + float(hash2(i, frame) % 20u), dy - 12.0f, 3.0f, 3.0f, rgb(255, 230, 140));
    }
  }

  // The brochure, once the airship has dropped it.
  const auto& t = s.titan;
  if (s.brochureX >= 0 && !s.brochureTaken && (!t.on || t.phase != TitanPhase::Waiting))
  {
    const float x = cellScreenX(*this, s.brochureX, camX) + 32.0f, foot = float(s.brochureY + 1) * kCellPx - camY;
    if (!offScreen(x - 40.0f, foot - 60.0f, 80.0f, 60.0f))
    {
      drawGlow(r, mArt, x, foot - 24.0f, 50.0f, rgb(150, 210, 255), 0.35f + 0.15f * std::sin(float(frame) * 0.15f));
      DrawOpts o;
      o.angle = std::sin(float(frame) * 0.09f) * 4.0f;
      r.draw(baked(mArt, r, "brochure", 72, 56, 36, 56, paintBrochure), x, foot, o);
    }
  }

  // The Halcyon Titan.
  if (t.on && t.phase != TitanPhase::Waiting && t.phase != TitanPhase::Done)
  {
    // Its prevX and prevY stay where the intro put them, so it is smoothed
    // between logic frames from how it moves instead: lowered in the
    // intro, half a cell a frame round the ring in Orbit (on even frames,
    // while the runner is not on the far side), over the edge in Topple.
    float bx = float(t.x), ty = float(t.y);
    bool striding = false;
    if (t.phase == TitanPhase::Intro)
      ty = introY(t, alpha);
    else if (t.phase == TitanPhase::Orbit)
    {
      const int dx = towerDx(t.x + kTitanW / 2, mPlayer.x + 1);
      striding = std::abs(dx) < mMap.width() / 2 - 4;
      if (striding)
        bx += float(dx < 0 ? 1 : -1) * ((float(t.t % 2) + alpha) * 0.5f - 0.5f);
    }
    else if (t.phase == TitanPhase::Topple && t.fallT > 0)
    {
      bx -= float(t.facing) * (1.0f - alpha);
      ty -= float(1 + t.fallT / 4) * (1.0f - alpha);
    }
    const float cx = (bx + float(kTitanW) * 0.5f) * kCellPx - camX, foot = (ty + 1.0f) * kCellPx - camY;
    const int f = t.facing < 0 ? -1 : 1;
    const bool crouch = t.phase == TitanPhase::Overheat && t.stompT >= kTitanStompEvery - kTitanStompTell;
    const float drop = crouch ? kTitanCrouch : 0.0f;
    // A part of its body (in its box's px, facing right) on the screen.
    auto at = [&](float px, float py, bool upper = true) {
      return std::array<float, 2>{cx + float(f) * (px - 160.0f), foot - (384.0f - py) + (upper ? drop : 0.0f)};
    };
    const bool onScreen = !offScreen(cx - 700.0f, foot - 500.0f, 1400.0f, 560.0f);
    int v = crouch ? 1 : 0;
    if (striding)
      v = 2 + (t.t / 4) % 2;
    // The cargo airship lowering it, its banner unrolling.
    if (t.phase == TitanPhase::Intro)
    {
      const float k = float(t.ship) + alpha;
      const float in = std::clamp(k / 24.0f, 0.0f, 1.0f), out = std::clamp((k - 80.0f) / 10.0f, 0.0f, 1.0f);
      const float ease = 1.0f - (1.0f - in) * (1.0f - in);
      const float sx = cx + (1.0f - ease) * 1500.0f - out * out * 900.0f;
      const float sy = 150.0f + std::sin(k * 0.08f) * 4.0f - out * 260.0f;
      // The banner off the stern.
      const float unroll = std::clamp((k - 14.0f) / 26.0f, 0.0f, 1.0f);
      if (unroll > 0.0f)
      {
        const Texture& ban = baked(mArt, r, "banner", 240, 600, 120, 0, paintBanner);
        DrawOpts bo;
        bo.angle = std::sin(k * 0.1f) * 3.0f;
        bo.scale = 0.75f;
        const float bxp = sx + 330.0f, byp = sy - 20.0f;
        // Unrolling: only the top part shows, the roll at its foot.
        const float shown = 600.0f * 0.75f * unroll;
        const float fullH = 600.0f * 0.75f;
        r.draw(ban, bxp, byp - (fullH - shown), bo);
        r.fillRect(bxp - 92.0f, byp + shown - 8.0f, 184.0f, 16.0f, rgb(230, 236, 250));
      }
      // The Titan on its cables, under the ship.
      if (onScreen)
      {
        const auto l = at(110.0f, 60.0f), rr = at(230.0f, 60.0f);
        for (const auto& pt : {l, rr})
          if (pt[1] > sy + 10.0f)
            beamLine(r, sx + (pt[0] - cx) * 0.3f, sy + 8.0f, pt[0], pt[1], 2.0f, rgb(60, 60, 70));
        const Sprite& body = styledEnemySprite(mArt, r, mTheme, "halcyon_titan", 0, 0, kTitanW, kTitanH);
        r.draw(body.get(f), cx, foot);
      }
      r.draw(baked(mArt, r, "airship", 960, 380, 480, 360, paintAirship), sx, sy);
      // Its propellers turning.
      for (const float ex : {-248.0f, 202.0f})
      {
        const float wob = std::fabs(std::sin(k * 1.3f + ex));
        r.fillRect(sx + ex - 2.0f, sy - 118.0f - 30.0f * wob, 4.0f, 60.0f * wob, rgba(220, 230, 255, 120));
      }
      if ((frame / 10) % 2 == 0)
        drawGlow(r, mArt, sx - 440.0f, sy - 220.0f, 26.0f, rgb(255, 60, 40), 0.9f);
      // Its shadow sliding over the deck.
      const float dk = float(deckRow(*this)) * kTilePx - camY;
      r.fillRect(sx - 420.0f, dk - 6.0f, 840.0f, 6.0f, rgba(10, 8, 30, 70));
    }
    else if (onScreen)
    {
      DrawOpts o;
      if (t.phase == TitanPhase::Topple)
      {
        const float tt = float(t.t) + alpha;
        if (tt <= float(kTeeter))
          o.angle = std::sin(tt * 0.9f) * 7.0f * (tt / float(kTeeter)) * float(f);
        else
          o.angle = float(f) * std::min(100.0f, (float(t.fallT) + alpha) * 3.5f);
      }
      const Sprite& body = styledEnemySprite(mArt, r, mTheme, "halcyon_titan", v, 0, kTitanW, kTitanH);
      // The stomp's crouch: dust puffing from its feet.
      if (crouch)
        for (int k = 0; k < 4; ++k)
          puff(r, mArt, cx + float(k - 2) * 60.0f + 30.0f, foot - 10.0f, 30.0f, rgb(220, 210, 200), 0.4f);
      r.draw(body.get(f), cx, foot, o);
      if (t.flash > 0)
      {
        DrawOpts wo = o;
        wo.blend = Blend::Add;
        wo.alpha = 0.6f;
        r.draw(body.get(f), cx, foot, wo);
      }
      if (t.phase != TitanPhase::Topple)
      {
        // The visor's eye.
        const auto eye = at(kTitanEyeX, kTitanEyeY);
        drawGlow(r, mArt, eye[0], eye[1], 24.0f, t.phase == TitanPhase::Overheat ? rgb(255, 80, 60) : rgb(120, 210, 255), 0.7f);
        // The power core, open after each demo attack.
        if (t.phase == TitanPhase::Demo && t.core > 0)
        {
          const auto c = at(kTitanCoreX, kTitanCoreY);
          const float pulse = 0.75f + 0.25f * std::sin(float(frame) * 0.4f);
          drawGlow(r, mArt, c[0], c[1], 90.0f * pulse, rgb(60, 150, 255), 0.7f);
          drawGlow(r, mArt, c[0], c[1], 30.0f, rgb(220, 240, 255), 1.0f);
          if (t.core < 10 && (frame / 2) % 2 == 0)
            drawGlow(r, mArt, c[0], c[1], 50.0f, rgb(255, 255, 255), 0.4f);
        }
        // Its reactor pack, exposed all through the orbit.
        if (t.phase == TitanPhase::Orbit)
        {
          const auto pk = at(kTitanPackX, kTitanPackY);
          drawGlow(r, mArt, pk[0], pk[1], 70.0f + 10.0f * std::sin(float(frame) * 0.3f), rgb(255, 170, 60), 0.8f);
          drawGlow(r, mArt, pk[0], pk[1], 22.0f, rgb(255, 250, 220), 1.0f);
        }
      }
      // The vent panels.
      if (t.phase == TitanPhase::Overheat || t.phase == TitanPhase::Topple)
        for (int i = 0; i < 4; ++i)
        {
          const auto& pn = kTitanPanel[i];
          const bool upper = pn[1] < 236.0f;
          const auto c = at(pn[0] + pn[2] * 0.5f, pn[1] + pn[3] * 0.5f, upper);
          if (t.panel[std::size_t(i)] <= 0)
          {
            // Blown out: a black hole, smoke curling off it, sparks.
            r.fillRect(c[0] - pn[2] * 0.5f, c[1] - pn[3] * 0.5f, pn[2], pn[3], rgb(26, 20, 22));
            for (int k = 0; k < 3; ++k)
            {
              const float u = std::fmod(float(frame) * 0.012f + float(k) / 3.0f + float(i) * 0.2f, 1.0f);
              puff(r, mArt, c[0] + std::sin(u * 6.0f + float(i)) * 12.0f, c[1] - u * 140.0f, 14.0f + u * 26.0f, rgb(60, 58, 64),
                0.55f * (1.0f - u));
            }
            if ((hash2(i, frame / 4) % 4u) == 0u)
              drawGlow(r, mArt, c[0], c[1], 20.0f, rgb(255, 200, 100), 0.8f);
            continue;
          }
          if (t.open != i || t.phase != TitanPhase::Overheat)
            continue;
          const bool back = i >= 2;
          const int side = back ? -f : f;
          if (t.openT <= kPanelTell)
          {
            // The tell: glowing red, a hiss of vapour round its edges.
            const float k = float(t.openT) / float(kPanelTell);
            drawGlow(r, mArt, c[0], c[1], 40.0f + 30.0f * k, rgb(255, 60, 30), 0.4f + 0.5f * k * ((frame / 2) % 2 ? 1.0f : 0.7f));
            for (int q = 0; q < 3; ++q)
              puff(r, mArt, c[0] + float(side) * (pn[2] * 0.5f + float(q) * 6.0f), c[1] - pn[3] * 0.5f + float(q) * pn[3] * 0.5f, 10.0f,
                rgb(240, 240, 250), 0.4f * k);
          }
          else
          {
            // Open: the hatch swung out, the hot vanes inside, steam
            // jetting out of that side.
            r.fillRect(c[0] - pn[2] * 0.5f, c[1] - pn[3] * 0.5f, pn[2], pn[3], rgb(60, 10, 8));
            for (float y = c[1] - pn[3] * 0.5f + 4.0f; y < c[1] + pn[3] * 0.5f - 2.0f; y += 7.0f)
              r.fillRect(c[0] - pn[2] * 0.5f + 3.0f, y, pn[2] - 6.0f, 3.0f, rgb(255, 120 + int(60.0f * std::sin(float(frame) * 0.5f + y)), 40));
            drawGlow(r, mArt, c[0], c[1], 50.0f, rgb(255, 90, 40), 0.6f);
            const float hx = c[0] + float(side) * (pn[2] * 0.5f);
            beamLine(r, hx, c[1] - pn[3] * 0.5f, hx + float(side) * pn[2] * 0.8f, c[1] - pn[3] * 0.5f - 10.0f, 4.0f, rgb(200, 206, 220));
            const float edge = cx + float(side) * 160.0f;
            const float reach = float(kSteamReach + 2) * kCellPx;
            for (int q = 0; q < 14; ++q)
            {
              const float u = std::fmod(float(frame) * 0.06f + float(q) / 14.0f, 1.0f);
              puff(r, mArt, edge + float(side) * u * reach, c[1] + std::sin(float(q) * 2.1f) * u * 24.0f - u * 20.0f, 14.0f + u * 30.0f,
                rgb(236, 240, 250), 0.75f * (1.0f - u * 0.8f));
            }
          }
        }
      // The demo reel's attacks.
      if (t.phase == TitanPhase::Demo)
      {
        if (t.move == TitanMove::Laser)
        {
          const auto mz = at(kTitanLaserX, kTitanLaserY, false);
          if (t.moveT <= kLaserTell)
          {
            const float k = (float(t.moveT) + alpha) / float(kLaserTell);
            drawGlow(r, mArt, mz[0], mz[1], 16.0f + 50.0f * k, rgb(255, 60, 60), 0.4f + 0.5f * k);
            // Its sight flickering along the beam's line.
            if ((frame / 2) % 2 == 0)
              r.drawLine(mz[0], mz[1], mz[0] + float(f) * float(kLaserReach) * kCellPx, mz[1], 2.0f, rgba(255, 60, 60, int(80.0f + 100.0f * k)));
          }
          else if (t.moveT <= kLaserTell + kLaserFrames)
          {
            const float len = float(kLaserReach) * kCellPx - (kTitanLaserX - 160.0f);
            const float x1 = mz[0] + float(f) * len;
            const float wob = std::sin(float(frame) * 1.3f) * 2.0f;
            r.drawLine(mz[0], mz[1], x1, mz[1], 30.0f + wob, rgba(255, 40, 40, 120), Blend::Add);
            r.drawLine(mz[0], mz[1], x1, mz[1], 14.0f, rgb(255, 120, 110), Blend::Add);
            r.drawLine(mz[0], mz[1], x1, mz[1], 5.0f, rgb(255, 255, 240));
            drawGlow(r, mArt, mz[0], mz[1], 60.0f, rgb(255, 120, 100), 0.9f);
            for (int q = 0; q < 6; ++q)
              drawGlow(r, mArt, mz[0] + float(f) * len * unit(hash2(q, frame)), mz[1] + (unit(hash2(q, frame + 7)) - 0.5f) * 14.0f, 14.0f,
                rgb(255, 230, 200), 0.7f);
          }
        }
        if (t.move == TitanMove::Flame)
        {
          const auto nz = at(kTitanFlameX, kTitanFlameY, false);
          if (t.moveT <= kFlameTell)
          {
            // The pilot light.
            const float fl = 0.7f + 0.3f * float(hash2(frame, 35) % 10u) / 10.0f;
            drawGlow(r, mArt, nz[0] + float(f) * 8.0f, nz[1], 18.0f * fl, rgb(90, 160, 255), 0.9f);
            drawGlow(r, mArt, nz[0] + float(f) * 14.0f, nz[1] - 4.0f, 10.0f * fl, rgb(255, 220, 120), 0.8f);
          }
          else if (t.moveT <= kFlameTell + kFlameFrames)
          {
            // The cone: tongues of flame rolling out, widening, going to smoke.
            const float reach = float(kTitanW / 2 + kFlameReach) * kCellPx - (kTitanFlameX - 160.0f);
            const float age = float(t.moveT - kFlameTell) + alpha;
            for (int q = 0; q < 36; ++q)
            {
              const float u = std::fmod(unit(hash2(q, 3561)) + age * 0.09f, 1.0f);
              const float spread = 20.0f + u * 160.0f;
              const float x = nz[0] + float(f) * u * reach;
              const float y = nz[1] - u * 60.0f + (unit(hash2(q, 3562)) - 0.5f) * spread;
              const Color c = u < 0.25f ? rgb(255, 240, 170) : (u < 0.6f ? rgb(255, 150, 40) : rgb(220, 70, 30));
              drawGlow(r, mArt, x, y, 26.0f + u * 50.0f, c, 0.75f * (1.0f - u * 0.6f));
            }
            for (int q = 0; q < 8; ++q)
            {
              const float u = std::fmod(unit(hash2(q, 3563)) + age * 0.05f, 1.0f);
              puff(r, mArt, nz[0] + float(f) * reach * (0.6f + 0.4f * u), nz[1] - 120.0f - u * 120.0f, 30.0f + u * 40.0f, rgb(60, 54, 60),
                0.4f * (1.0f - u));
            }
          }
        }
        if (t.move == TitanMove::Missiles && t.moveT < kMissileTell)
        {
          const float k = (float(t.moveT) + alpha) / float(kMissileTell);
          // Launched off its shoulder pods.
          if (t.moveT < 6)
          {
            const auto pod = at(kTitanPodX, kTitanPodY);
            for (int q = 0; q < 4; ++q)
            {
              const float u = (float(t.moveT) + alpha) / 6.0f;
              const float mx = pod[0] + float(q - 2) * 10.0f, my = pod[1] - u * 500.0f - float(q) * 30.0f;
              beamLine(r, mx, my, mx, my + 22.0f, 4.0f, rgb(240, 240, 246));
              drawGlow(r, mArt, mx, my + 26.0f, 16.0f, rgb(255, 180, 80), 0.9f);
              puff(r, mArt, mx, my + 50.0f + u * 40.0f, 18.0f, rgb(230, 226, 230), 0.5f * (1.0f - u));
            }
          }
          for (int q = 0; q < 4; ++q)
          {
            const float x = cellScreenX(*this, t.cross[std::size_t(q)], camX) + 16.0f;
            const float y = float(t.deckY) * kCellPx - camY + 28.0f;
            const float rad = 70.0f - 40.0f * k;
            const bool blink = (frame / std::max(1, int(6.0f - 5.0f * k))) % 2 == 0;
            const Color c = blink ? rgb(255, 50, 40) : rgb(255, 210, 200);
            // A ring painted on the deck, foreshortened, its four ticks turning.
            for (int seg = 0; seg < 24; ++seg)
            {
              const float a0 = 6.2831853f * float(seg) / 24.0f, a1 = a0 + 6.2831853f / 24.0f;
              r.drawLine(x + rad * std::cos(a0), y + rad * 0.3f * std::sin(a0), x + rad * std::cos(a1), y + rad * 0.3f * std::sin(a1), 4.0f, c);
            }
            for (int tick = 0; tick < 4; ++tick)
            {
              const float a = float(tick) * 1.5708f + k * 3.0f;
              r.drawLine(x + std::cos(a) * rad * 0.6f, y + std::sin(a) * rad * 0.18f, x + std::cos(a) * rad * 1.25f, y + std::sin(a) * rad * 0.38f,
                4.0f, c);
            }
            drawGlow(r, mArt, x, y, rad * 0.8f, rgb(255, 60, 40), 0.25f);
            // The missile coming down on it, the last few frames.
            const float fallK = (float(t.moveT) + alpha - float(kMissileTell - 6)) / 6.0f;
            if (fallK > 0.0f)
            {
              const float my = -60.0f + (y + 60.0f) * fallK;
              beamLine(r, x, my - 30.0f, x, my, 6.0f, rgb(240, 240, 246));
              r.fillRect(x - 4.0f, my - 4.0f, 8.0f, 8.0f, rgb(220, 50, 40));
              drawGlow(r, mArt, x, my - 36.0f, 20.0f, rgb(255, 170, 80), 0.9f);
              r.drawLine(x, my - 40.0f, x, my - 200.0f, 6.0f, rgba(240, 236, 240, 90));
            }
          }
        }
      }
      // The stomp's shockwave rolling along the deck both ways.
      if (t.wave >= 0)
      {
        const float k = float(t.wave) / float(kTitanWaveReach);
        const float dk = float(t.deckY + 1) * kCellPx - camY;
        for (int sd = -1; sd <= 1; sd += 2)
        {
          const float wx = cx + float(sd) * (float(t.wave) + alpha * float(kTitanWaveSpeed)) * kCellPx;
          for (int q = 0; q < 5; ++q)
          {
            const float back = float(q) * 14.0f;
            const float hgt = (46.0f - float(q) * 8.0f) * (1.0f - k * 0.5f);
            r.drawLine(wx - float(sd) * back, dk, wx - float(sd) * (back + 16.0f), dk - hgt, 6.0f - float(q),
              rgba(255, 240, 210, int(220.0f * (1.0f - k) * (1.0f - float(q) * 0.18f))), Blend::Add);
          }
          puff(r, mArt, wx, dk - 14.0f, 24.0f, rgb(210, 200, 190), 0.5f * (1.0f - k));
        }
      }
    }
    // The lattice shot's tell: its muzzle glowing through the mast at the
    // edge of the screen it will come from.
    if (t.phase == TitanPhase::Orbit && t.lattice >= kLatticeCool)
    {
      const int side = towerDx(p.x + 1, t.x + kTitanW / 2) < 0 ? -1 : 1;
      const float k = float(t.lattice - kLatticeCool) / float(kLatticeTell);
      const float ex = side < 0 ? 0.0f : float(kScreenW), ey = float(t.deckY) * kCellPx - camY;
      drawGlow(r, mArt, ex, ey, 80.0f + 120.0f * k, rgb(255, 120, 60), 0.4f + 0.5f * k);
      if ((frame / 2) % 2 == 0)
        drawGlow(r, mArt, ex, ey, 40.0f, rgb(255, 240, 200), 0.8f * k);
    }
  }

  // The round tower's sides falling away into shadow.
  if (s.wrap)
  {
    float sway = 0.0f;
    if (s.dizzy > 0)
      sway = std::sin(float(frame) * 0.3f) * 8.0f;
    r.draw(baked(mArt, r, "sides", kScreenW, kScreenH, 0, 0, paintSides), sway, 0.0f);
  }
}

// --- The HUD ---------------------------------------------------------------------------------------------

void World::drawTowerHud(Renderer& r, int frame) const
{
  const auto& s = mTower;
  const auto& t = s.titan;
  // A holo banner: a cyan projection that unrolls across, flickering, with
  // scan lines.
  if (t.banner > 0 && !t.bannerText.empty())
  {
    int total = 30;
    if (t.bannerText.rfind("BROUGHT", 0) == 0)
      total = kTitanIntro;
    else if (t.bannerText == "PRODUCT RECALL")
      total = 60;
    else if (t.bannerText.rfind("NOW", 0) == 0 || t.bannerText.rfind("WARNING", 0) == 0)
      total = 45;
    const int age = std::max(0, total - t.banner);
    const float open = std::min(1.0f, float(age + 1) / 6.0f);
    const float fade = std::min(1.0f, float(t.banner) / 5.0f);
    const float w = 640.0f * open, cx = float(kScreenW) * 0.5f, y = 140.0f; // (under the game's own message line)
    const bool flick = (hash2(frame / 2, 3571) % 9u) == 0u;
    const float a = fade * (flick ? 0.5f : 1.0f);
    r.fillRect(cx - w * 0.5f, y, w, 64.0f, rgba(40, 140, 220, int(110.0f * a)));
    for (float sy = y; sy < y + 64.0f; sy += 4.0f)
      r.fillRect(cx - w * 0.5f, sy, w, 1.0f, rgba(160, 230, 255, int(60.0f * a)));
    r.fillRect(cx - w * 0.5f, y, w, 3.0f, rgba(200, 245, 255, int(230.0f * a)));
    r.fillRect(cx - w * 0.5f, y + 61.0f, w, 3.0f, rgba(200, 245, 255, int(230.0f * a)));
    // The projector's beam fanning up from below the panel.
    r.drawLine(cx, y + 64.0f, cx - w * 0.5f, y + 64.0f, 2.0f, rgba(160, 230, 255, int(80.0f * a)), Blend::Add);
    if (open >= 1.0f)
    {
      const Color red = t.bannerText == "PRODUCT RECALL" || t.bannerText.rfind("WARNING", 0) == 0 ? rgb(255, 120, 100) : rgb(220, 250, 255);
      r.drawText(t.bannerText, cx + (flick ? 3.0f : 0.0f), y + 14.0f, {30.0f, red, kHudInk}, Align::Center, a);
      r.drawText("HALCYON ARMS", cx - w * 0.5f + 12.0f, y + 4.0f, {11.0f, rgb(200, 245, 255), 0}, Align::Left, a);
    }
  }
  if (titanFight() && t.phase != TitanPhase::Intro)
  {
    const float bw = 600.0f, bx = (float(kScreenW) - bw) * 0.5f, by = float(kScreenH) - 54.0f;
    r.fillRect(bx - 12, by - 32, bw + 24, 56, rgba(8, 10, 30, 210));
    r.fillRect(bx - 12, by - 32, bw + 24, 3, kHalcyon);
    r.drawText("HALCYON TITAN", bx, by - 27, {17.0f, rgb(220, 240, 255), kHudInk});
    static const char* kPhase[] = {"DEMO REEL", "ORBITAL MODE", "OVERHEATING"};
    const int ph = t.phase == TitanPhase::Demo ? 0 : (t.phase == TitanPhase::Orbit ? 1 : 2);
    r.drawText(kPhase[ph], bx + bw, by - 27, {15.0f, ph == 2 ? rgb(255, 130, 100) : rgb(150, 210, 255), kHudInk}, Align::Right);
    const float frac = float(t.total()) / 92.0f;
    r.fillRect(bx, by, bw, 14, rgba(255, 255, 255, 40));
    const Color fill = (t.flash > 0 && (frame / 2) % 2) ? rgb(255, 255, 255) : (ph == 2 ? rgb(255, 120, 80) : kHalcyon);
    r.fillRect(bx, by, bw * frac, 14, fill);
    r.fillRect(bx, by, bw * frac, 4, lerpColor(fill, rgb(255, 255, 255), 0.5f));
    for (const float cut : {32.0f / 92.0f, 62.0f / 92.0f})
      r.fillRect(bx + bw * cut - 1.0f, by - 3, 3, 20, rgb(20, 16, 30));
    // Round the back of the mast: an arrow at the edge pointing the way.
    if (t.phase == TitanPhase::Orbit)
    {
      const auto& p = mPlayer;
      const int d = towerDx(p.x + 1, t.x + kTitanW / 2);
      const float px = (float(p.x) + 1.5f) * kCellPx - mCamera.renderX() * kPixelScale;
      const float sx = px + float(d) * kCellPx;
      if (sx < 0.0f || sx > float(kScreenW))
      {
        const int side = d < 0 ? -1 : 1;
        const float ax = side < 0 ? 40.0f : float(kScreenW) - 40.0f, ay = float(kScreenH) * 0.5f;
        const float bob = std::sin(float(frame) * 0.2f) * 6.0f * float(side);
        for (int k = 0; k < 12; ++k)
          r.fillRect(ax + bob - float(side) * float(k) * 2.0f, ay - 24.0f + float(k) * 2.0f, 4.0f, 48.0f - float(k) * 4.0f,
            rgba(255, 140, 80, 220));
        char buf[24];
        std::snprintf(buf, sizeof(buf), "%d", std::abs(d) / kCellsPerTile);
        r.drawText(buf, ax - float(side) * 30.0f, ay + 30.0f, {18.0f, rgb(255, 200, 160), kHudInk}, Align::Center);
      }
    }
  }
  if (s.copycat)
  {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "CAUGHT %d / 8", s.caught);
    const float x = float(kScreenW) - 30.0f;
    r.fillRect(x - 210.0f, 140.0f, 220.0f, 44.0f, rgba(8, 10, 30, 170));
    r.fillRect(x - 210.0f, 140.0f, 220.0f, 3.0f, rgb(255, 130, 30));
    r.drawText(buf, x - 4.0f, 148.0f, {26.0f, rgb(255, 220, 140), kHudInk}, Align::Right);
  }
}

// --- Shots and icons -------------------------------------------------------------------------------------

bool drawTowerShot(Renderer& r, const World& w, const Art& art, const Projectile& pr, float cx, float cy, int frame)
{
  const auto& s = w.tower();
  if (!s.on)
    return false;
  const float d = float(pr.dx < 0 ? -1 : (pr.dx > 0 ? 1 : 0)), dv = float(pr.dy < 0 ? -1 : (pr.dy > 0 ? 1 : 0));
  if (pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::SatelliteSwarm))
  {
    // A swarm drone's pellet: a cyan bead with a short trail.
    for (int k = 1; k <= 4; ++k)
      r.fillRect(cx - d * float(k) * 9.0f - 3.0f, cy - dv * float(k) * 9.0f - 3.0f, 6.0f, 6.0f, rgba(160, 255, 255, 200 - k * 40));
    drawGlow(r, art, cx, cy, 26.0f, kSwarm, 0.8f);
    r.fillRect(cx - 4.0f, cy - 4.0f, 8.0f, 8.0f, rgb(240, 255, 255));
    return true;
  }
  if (pr.kind != ShotKind::Enemy || !s.wrap)
    return false;
  if (pr.w == 2 && s.titan.on && s.titan.phase == TitanPhase::Orbit)
  {
    // The Titan's lattice shot: a white-hot bolt crackling along the deck,
    // sparks off the girders.
    drawGlow(r, art, cx, cy, 70.0f, rgb(255, 140, 60), 0.7f);
    for (int k = 0; k < 5; ++k)
      r.drawLine(cx - d * float(k) * 18.0f, cy + float(int(hash2(k, frame) % 9u) - 4), cx - d * float(k + 1) * 18.0f,
        cy + float(int(hash2(k + 9, frame) % 9u) - 4), 6.0f - float(k), rgba(255, 220, 160, 230 - k * 40), Blend::Add);
    r.fillRect(cx - 10.0f, cy - 5.0f, 20.0f, 10.0f, rgb(255, 250, 230));
    if ((frame / 2) % 2 == 0)
      r.fillRect(cx + float(int(hash2(frame, 3581) % 30u) - 15), cy - 20.0f - float(hash2(frame, 3582) % 20u), 3.0f, 3.0f, rgb(255, 230, 140));
    return true;
  }
  // A Relay Drone's shot: a red packet of signal, rings pulsing round it.
  const float ph = std::fmod(float(frame) * 0.15f, 1.0f);
  drawGlow(r, art, cx, cy, 34.0f, rgb(255, 80, 60), 0.7f);
  for (int k = 0; k < 2; ++k)
  {
    const float rad = 8.0f + std::fmod(ph + float(k) * 0.5f, 1.0f) * 16.0f;
    const int segs = 12;
    for (int q = 0; q < segs; ++q)
    {
      const float a0 = 6.2831853f * float(q) / float(segs), a1 = a0 + 6.2831853f / float(segs);
      r.drawLine(cx + rad * std::cos(a0), cy + rad * std::sin(a0), cx + rad * std::cos(a1), cy + rad * std::sin(a1), 2.5f,
        rgba(255, 160, 140, int(200.0f * (1.0f - (rad - 8.0f) / 16.0f))));
    }
  }
  r.fillRect(cx - 5.0f, cy - 5.0f, 10.0f, 10.0f, rgb(255, 240, 220));
  return true;
}

void drawSwarmIcon(Renderer& r, const Art& art, float x, float y, float scale, int frame)
{
  DrawOpts o;
  o.scale = scale;
  r.draw(baked(art, r, "swarm_icon", 64, 64, 0, 0, paintSwarmIcon), x, y, o);
  // A glint running round the orbit.
  const float a = float(frame) * 0.12f;
  drawGlow(r, art, x + (32.0f + std::cos(a) * 27.0f) * scale, y + (34.0f + std::sin(a) * 12.0f) * scale, 10.0f * scale, kSwarm, 0.8f);
}

} // namespace gr
