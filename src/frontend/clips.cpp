// The cutscene clips: scenes drawn with the game's vector art. A clip is a
// function of its frame (and the shot's progress); shared pieces (skylines,
// the MAX hologram, the runners, TV static, cards) are baked with Cairo the
// first time they are used and cached in the ClipKit.

#include "assets/art.hpp"
#include "assets/enemy_art.hpp"
#include "assets/enemy_art_city.hpp"
#include "assets/enemy_art_kitchen.hpp"
#include "assets/enemy_art_manor.hpp"
#include "assets/enemy_art_toon.hpp"
#include "assets/enemy_art_west.hpp"
#include "assets/enemy_art_zero.hpp"
#include "base/math.hpp"
#include "data/theme.hpp"
#include "frontend/cutscene.hpp"
#include "render/vector.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

// cached() returns a reference into ClipKit::cache, not into its painter
// argument; GCC's heuristic flags every call that passes a lambda.
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 13
#pragma GCC diagnostic ignored "-Wdangling-reference"
#endif

namespace gr
{

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr Color kInk = rgb(10, 8, 20);
constexpr float W = float(kScreenW);
constexpr float H = float(kScreenH);

Color lighten(Color c, float t) { return lerpColor(c, rgb(255, 255, 255), t); }
Color darken(Color c, float t) { return lerpColor(c, rgb(0, 0, 0), t); }

const Texture& cached(ClipKit& k, const std::string& key, int w, int h, float ax, float ay,
  const std::function<void(cairo_t*)>& paint)
{
  auto it = k.cache.find(key);
  if (it != k.cache.end())
    return it->second;
  VectorImage img(w, h);
  paint(img.cr());
  return k.cache.emplace(key, img.toTexture(k.r, ax, ay)).first->second;
}

const Texture& runner(ClipKit& k, int kind, int pose, float scale, bool mirror = false)
{
  const std::string key = "runner" + std::to_string(kind) + "_" + std::to_string(pose) + "_" +
    std::to_string(int(scale * 100)) + (mirror ? "m" : "");
  auto it = k.cache.find(key);
  if (it != k.cache.end())
    return it->second;
  return k.cache.emplace(key, bakeCharacterPose(k.r, kind, pose, scale, mirror)).first->second;
}

void gradient(cairo_t* cr, double w, double h, Color top, Color mid, Color bottom)
{
  cairo_pattern_t* g = cairo_pattern_create_linear(0, 0, 0, h);
  auto stop = [&](double at, Color c) {
    cairo_pattern_add_color_stop_rgb(g, at, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0);
  };
  stop(0.0, top);
  stop(0.55, mid);
  stop(1.0, bottom);
  cairo_rectangle(cr, 0, 0, w, h);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
}

// A skyline strip 2560 px wide (tiles horizontally), with lit windows.
const Texture& skyline(ClipKit& k, int layer, Color body, Color window)
{
  return cached(k, "skyline" + std::to_string(layer), 2560, 420, 0, 0, [&](cairo_t* cr) {
    Rng rng(std::uint32_t(77 + layer * 13));
    double x = 0;
    while (x < 2560)
    {
      const double bw = rng.range(70.0f, 170.0f);
      const double bh = layer == 0 ? rng.range(140.0f, 300.0f) : rng.range(90.0f, 230.0f);
      const double top = 420 - bh;
      cairo_rectangle(cr, x, top, bw - 6, bh);
      setColor(cr, body);
      cairo_fill(cr);
      if (rng.uniform() < 0.3f)
      {
        cairo_rectangle(cr, x + bw * 0.4, top - 30, 4, 30);
        cairo_fill(cr);
      }
      for (double wy = top + 14; wy < 410; wy += 18)
        for (double wx = x + 8; wx < x + bw - 14; wx += 14)
          if (rng.uniform() < (layer == 0 ? 0.25f : 0.4f))
          {
            cairo_rectangle(cr, wx, wy, 6, 8);
            setColor(cr, withAlpha(window, rng.uniform() < 0.5f ? 230 : 140));
            cairo_fill(cr);
          }
      x += bw;
    }
  });
}

void neonSky(ClipKit& k, float sun)
{
  const Texture& sky = cached(k, "neon_sky", 1280, 720, 0, 0, [](cairo_t* cr) {
    gradient(cr, 1280, 720, rgb(18, 6, 40), rgb(110, 20, 110), rgb(255, 120, 70));
    for (int i = 0; i < 90; ++i)
    {
      const double x = double(hash2(i, 3) % 1280u), y = double(hash2(i, 9) % 300u);
      cairo_arc(cr, x, y, (i % 3) ? 1.0 : 1.8, 0, 2 * kPi);
      cairo_set_source_rgba(cr, 1, 1, 1, 0.7);
      cairo_fill(cr);
    }
  });
  k.r.draw(sky, 0, 0);
  const Texture& sunTex = cached(k, "neon_sun", 520, 520, 260, 260, [](cairo_t* cr) {
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 0, 0, 520);
    cairo_pattern_add_color_stop_rgb(g, 0.0, 1.0, 0.92, 0.35);
    cairo_pattern_add_color_stop_rgb(g, 1.0, 1.0, 0.25, 0.55);
    cairo_arc(cr, 260, 260, 250, 0, 2 * kPi);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    for (int i = 0; i < 8; ++i)
    {
      const double y = 300 + i * 26;
      cairo_rectangle(cr, 0, y, 520, 4 + i * 1.6);
      cairo_fill(cr);
    }
  });
  k.r.draw(sunTex, 640, 470 + sun);
}

void panLayer(ClipKit& k, const Texture& t, float offset, float y, float alpha = 1.0f)
{
  const float w = float(t.w());
  float ox = std::fmod(offset, w);
  if (ox < 0.0f)
    ox += w;
  DrawOpts o;
  o.alpha = alpha;
  for (float x = -ox; x < W; x += w)
    k.r.draw(t, x, y, o);
}

void staticNoise(ClipKit& k, int seed, float alpha = 1.0f, float x0 = 0, float y0 = 0, float w = W, float h = H)
{
  for (float y = y0; y < y0 + h; y += 6)
  {
    const unsigned hsh = hash2(int(y) * 7 + seed * 131, seed);
    const int v = int(40 + (hsh % 180u));
    k.r.fillRect(x0, y, w, 6, rgba(v, v, v + 12, int(255 * alpha)));
    if (hsh % 4u == 0)
      k.r.fillRect(x0 + float(hsh % 997u) / 997.0f * w, y, 40, 6, rgba(240, 240, 255, int(255 * alpha)));
  }
}

// MAX, the job broker: a slick TV-host hologram.
const Texture& maxHead(ClipKit& k)
{
  return cached(k, "max_head", 360, 420, 180, 210, [](cairo_t* cr) {
    const Color c = rgb(90, 240, 255);
    cairo_set_line_width(cr, 4.0);
    // Shoulders and lapels.
    cairo_move_to(cr, 40, 420);
    cairo_curve_to(cr, 60, 330, 120, 310, 180, 310);
    cairo_curve_to(cr, 240, 310, 300, 330, 320, 420);
    cairo_close_path(cr);
    setColor(cr, withAlpha(c, 60));
    cairo_fill_preserve(cr);
    setColor(cr, c);
    cairo_stroke(cr);
    cairo_move_to(cr, 150, 312);
    cairo_line_to(cr, 180, 380);
    cairo_line_to(cr, 210, 312);
    cairo_stroke(cr);
    // Bow tie.
    cairo_move_to(cr, 180, 330);
    cairo_line_to(cr, 150, 315);
    cairo_line_to(cr, 150, 345);
    cairo_close_path(cr);
    cairo_move_to(cr, 180, 330);
    cairo_line_to(cr, 210, 315);
    cairo_line_to(cr, 210, 345);
    cairo_close_path(cr);
    setColor(cr, rgb(255, 80, 200));
    cairo_fill(cr);
    // Head.
    cairo_save(cr);
    cairo_translate(cr, 180, 190);
    cairo_scale(cr, 1.0, 1.2);
    cairo_arc(cr, 0, 0, 95, 0, 2 * kPi);
    cairo_restore(cr);
    setColor(cr, withAlpha(c, 70));
    cairo_fill_preserve(cr);
    setColor(cr, c);
    cairo_stroke(cr);
    // The quiff.
    cairo_move_to(cr, 90, 140);
    cairo_curve_to(cr, 80, 40, 220, 20, 290, 70);
    cairo_curve_to(cr, 250, 60, 240, 90, 270, 130);
    cairo_curve_to(cr, 200, 90, 140, 100, 90, 140);
    setColor(cr, withAlpha(c, 160));
    cairo_fill_preserve(cr);
    setColor(cr, c);
    cairo_stroke(cr);
    // Shades.
    roundedRect(cr, 100, 160, 72, 40, 10);
    roundedRect(cr, 188, 160, 72, 40, 10);
    setColor(cr, rgb(20, 40, 60));
    cairo_fill_preserve(cr);
    setColor(cr, c);
    cairo_stroke(cr);
    cairo_move_to(cr, 172, 172);
    cairo_line_to(cr, 188, 172);
    cairo_stroke(cr);
    cairo_move_to(cr, 112, 168);
    cairo_line_to(cr, 140, 168);
    setColor(cr, rgba(255, 255, 255, 200));
    cairo_stroke(cr);
    // The grin.
    cairo_move_to(cr, 120, 250);
    cairo_curve_to(cr, 150, 290, 210, 290, 240, 250);
    cairo_close_path(cr);
    setColor(cr, rgba(255, 255, 255, 220));
    cairo_fill_preserve(cr);
    setColor(cr, c);
    cairo_stroke(cr);
    for (int i = 0; i < 5; ++i)
    {
      cairo_move_to(cr, 135 + i * 22, 254);
      cairo_line_to(cr, 135 + i * 22, 268);
    }
    setColor(cr, withAlpha(c, 160));
    cairo_set_line_width(cr, 2.0);
    cairo_stroke(cr);
  });
}

// The hologram with scanlines and a scramble that settles.
void drawMax(ClipKit& k, float cx, float cy, float scale, int ticks, float scramble)
{
  const Texture& head = maxHead(k);
  DrawOpts o;
  o.scale = scale;
  o.blend = Blend::Add;
  o.alpha = 0.75f + 0.2f * std::sin(float(ticks) * 0.5f);
  const float jx = scramble > 0.0f ? (float(hash2(ticks, 1) % 41u) - 20.0f) * scramble : 0.0f;
  drawGlow(k.r, k.art, cx, cy, 260 * scale, rgb(80, 220, 255), 0.35f);
  k.r.draw(head, cx + jx, cy, o);
  if (scramble > 0.0f)
  {
    o.alpha = 0.4f * scramble;
    o.tint = rgb(255, 80, 200);
    k.r.draw(head, cx - jx * 1.5f, cy + 6, o);
  }
  // Scanlines.
  const float top = cy - 210 * scale, bottom = cy + 210 * scale;
  for (float y = top; y < bottom; y += 6)
    k.r.fillRect(cx - 190 * scale, y, 380 * scale, 2, rgba(0, 0, 0, 70));
  const float band = top + std::fmod(float(ticks) * 4.0f, bottom - top);
  k.r.fillRect(cx - 190 * scale, band, 380 * scale, 10, rgba(120, 255, 255, 40));
  // The projector beam from the table.
  k.r.fillRect(cx - 4, bottom, 8, 30, rgba(120, 255, 255, 80));
}

void card(ClipKit& k, const std::string& text, Color c, float size, float y, float alpha = 1.0f)
{
  k.r.drawText(text, W / 2, y, {size, c, kInk, true}, Align::Center, alpha);
}

void neonCity(ClipKit& k, int ticks, float ox, float oy, float t)
{
  neonSky(k, -t * 40.0f);
  panLayer(k, skyline(k, 0, rgb(52, 20, 80), rgb(255, 120, 200)), float(ticks) * 0.25f + ox, 240 + oy);
  panLayer(k, skyline(k, 1, rgb(26, 10, 44), rgb(0, 240, 255)), float(ticks) * 0.5f + ox, 320 + oy);
  // The Halcyon tower in the middle.
  k.r.fillRect(590 + ox, 150 + oy, 100, 560, rgb(20, 8, 36));
  k.r.fillRect(615 + ox, 120 + oy, 50, 40, rgb(20, 8, 36));
  for (int i = 0; i < 18; ++i)
    k.r.fillRect(600 + ox, 170 + float(i) * 28 + oy, 80, 4, (i + ticks / 20) % 5 == 0 ? rgb(255, 230, 90) : rgb(90, 40, 130));
  drawGlow(k.r, k.art, 640 + ox, 120 + oy, 60, rgb(255, 60, 120), 0.5f + 0.4f * float((ticks / 15) % 2));
  // A maglev crossing.
  const float mx = std::fmod(float(ticks) * 6.0f, W + 600.0f) - 500.0f;
  k.r.fillRect(0, 560 + oy, W, 8, rgb(70, 40, 110));
  k.r.fillRect(mx + ox, 528 + oy, 420, 30, rgb(220, 230, 250));
  k.r.fillRect(mx + ox, 538 + oy, 420, 6, rgb(0, 240, 255));
  drawGlow(k.r, k.art, mx + 420 + ox, 543 + oy, 70, rgb(0, 240, 255), 0.6f);
}

void diner(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  const Texture& room = cached(k, "diner_room", 1280, 720, 0, 0, [](cairo_t* cr) {
    gradient(cr, 1280, 720, rgb(40, 20, 50), rgb(60, 26, 60), rgb(30, 14, 30));
    // Window with the city outside.
    cairo_rectangle(cr, 160, 80, 960, 330);
    setColor(cr, rgb(20, 10, 40));
    cairo_fill(cr);
    for (int i = 0; i < 40; ++i)
    {
      const double x = 160 + double(hash2(i, 5) % 900u), h = 60 + double(hash2(i, 6) % 200u);
      cairo_rectangle(cr, x, 410 - h, 40, h);
      setColor(cr, rgb(50, 24, 80));
      cairo_fill(cr);
    }
    cairo_rectangle(cr, 150, 70, 980, 350);
    cairo_set_line_width(cr, 18);
    setColor(cr, rgb(200, 200, 215));
    cairo_stroke(cr);
    cairo_move_to(cr, 640, 70);
    cairo_line_to(cr, 640, 420);
    cairo_stroke(cr);
    // Booth and table.
    roundedRect(cr, 80, 430, 1120, 300, 30);
    setColor(cr, rgb(190, 30, 50));
    cairo_fill(cr);
    roundedRect(cr, 300, 520, 680, 40, 10);
    setColor(cr, rgb(230, 230, 240));
    cairo_fill(cr);
    cairo_rectangle(cr, 620, 560, 40, 160);
    setColor(cr, rgb(170, 170, 185));
    cairo_fill(cr);
  });
  k.r.draw(room, ox, oy);
  // Rain on the window.
  for (int i = 0; i < 70; ++i)
  {
    const float x = 170.0f + float(hash2(i, 11) % 940u);
    const float y = 80.0f + std::fmod(float(hash2(i, 12) % 330u) + float(frame * 40 + ticks * 3), 330.0f);
    k.r.fillRect(x + ox, y + oy, 2, 14, rgba(180, 200, 255, 120));
  }
  // The neon sign outside blinks.
  if ((frame / 2) % 2 == 0)
  {
    card(k, "DINER", rgb(255, 60, 200), 40, 110 + oy);
    drawGlow(k.r, k.art, 640 + ox, 130 + oy, 120, rgb(255, 60, 200), 0.4f);
  }
  // The three runners in the booth.
  k.r.draw(runner(k, 0, 0, 2.0f), 420 + ox, 600 + oy);
  k.r.draw(runner(k, 1, 0, 2.0f), 640 + ox, 610 + oy);
  k.r.draw(runner(k, 2, 0, 2.0f, true), 860 + ox, 600 + oy);
  k.r.fillRect(300 + ox, 520 + oy, 680, 40, rgb(230, 230, 240));
}

void nameCard(ClipKit& k, int who, int frame, float ox, float oy)
{
  static const char* const kNames[3] = {"DASH", "ROCCO", "NOVA"};
  static const char* const kRoles[3] = {"THE ALL-ROUNDER", "THE HEAVY", "THE ACROBAT"};
  static const Color kCol[3] = {rgb(232, 64, 52), rgb(104, 146, 64), rgb(132, 66, 216)};
  const Texture& bg = cached(k, "namebg" + std::to_string(who), 1280, 720, 0, 0, [&](cairo_t* cr) {
    gradient(cr, 1280, 720, darken(kCol[who], 0.6f), darken(kCol[who], 0.3f), rgb(10, 8, 20));
    for (int i = 0; i < 24; ++i)
    {
      cairo_move_to(cr, 640, 360);
      const double a = i * 2 * kPi / 24;
      cairo_line_to(cr, 640 + std::cos(a) * 1200, 360 + std::sin(a) * 1200);
      cairo_line_to(cr, 640 + std::cos(a + 0.12) * 1200, 360 + std::sin(a + 0.12) * 1200);
      cairo_close_path(cr);
      cairo_set_source_rgba(cr, 1, 1, 1, 0.05);
      cairo_fill(cr);
    }
  });
  k.r.draw(bg, ox, oy);
  // The runner's move on frames 0-4.
  const int pose = who == 0 ? (frame < 3 ? 6 : 0) : (who == 1 ? (frame % 2 ? 4 : 0) : (frame < 4 ? 2 : 0));
  k.r.draw(runner(k, who, pose, 4.0f), 420 + ox, 700 + oy);
  if (frame >= 5)
  {
    k.r.fillRect(640 + ox, 260 + oy, 560, 200, rgba(8, 6, 22, 220));
    k.r.fillRect(640 + ox, 260 + oy, 560, 8, kCol[who]);
    k.r.drawText(kNames[who], 920 + ox, 290 + oy, {92.0f, rgb(255, 255, 255), kInk, true}, Align::Center);
    k.r.drawText(kRoles[who], 920 + ox, 400 + oy, {28.0f, lighten(kCol[who], 0.4f), kInk}, Align::Center);
  }
}

void logo(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  k.r.fillRect(0, 0, W, H, rgb(12, 6, 24));
  drawGlow(k.r, k.art, 640, 360, 600, rgb(255, 60, 200), 0.25f);
  const std::string word = "GUNRUNNERS";
  const int slid = std::min(10, frame + 1);
  for (int i = 0; i < int(word.size()); ++i)
  {
    if (i >= slid)
      break;
    const float x = 190.0f + float(i) * 100.0f;
    const float settle = frame < 10 ? float(10 - frame) * 30.0f * float(i == slid - 1) : 0.0f;
    k.r.drawText(std::string(1, word[std::size_t(i)]), x + settle + ox, 280 + oy, {120.0f, rgb(255, 230, 90), kInk, true},
      Align::Center);
  }
  if (frame >= 10)
  {
    drawGlow(k.r, k.art, 640, 340, 400, rgb(255, 200, 80), frame >= 14 ? 0.5f : 0.3f);
    // Debris from the smashes.
    for (int i = 0; i < 30; ++i)
    {
      const float a = float(hash2(i, 7) % 628u) / 100.0f;
      const float d = float((ticks % 90) * 6);
      k.r.fillRect(640 + std::cos(a) * d + ox, 340 + std::sin(a) * d + oy, 8, 8, rgb(255, 160, 60));
    }
  }
  if (frame >= 17)
  {
    k.r.fillRect(0, 0, W, H, rgba(12, 6, 24, 160));
    if ((ticks / 20) % 2 == 0)
      drawGlow(k.r, k.art, 640, 80, 30, rgb(255, 30, 40), 0.9f);
  }
}

void tvCard(ClipKit& k, const std::string& text, int ticks)
{
  k.r.fillRect(0, 0, W, H, rgb(20, 16, 60));
  for (int i = 0; i < 8; ++i)
    k.r.fillRect(float(i) * 160.0f, 0, 160, H, (i % 2) ? rgb(30, 24, 80) : rgb(24, 20, 70));
  // The MaxTV logo, spinning.
  const float a = float(ticks) * 0.08f;
  const float sx = std::cos(a);
  k.r.fillRect(640 - 120 * std::abs(sx), 140, 240 * std::abs(sx), 120, sx > 0 ? rgb(255, 60, 200) : rgb(0, 240, 255));
  card(k, "MAX TV", rgb(255, 255, 255), 56, 168);
  card(k, text, rgb(255, 230, 90), 72, 380);
}

void bumper(ClipKit& k, const std::string& spec, int frame, int ticks)
{
  // "bumper:CH:GENRE": 4 frames of static, then the channel card.
  const auto a = spec.find(':', 7);
  const std::string ch = spec.substr(7, a == std::string::npos ? std::string::npos : a - 7);
  const std::string genre = a == std::string::npos ? std::string() : spec.substr(a + 1);
  if (frame < 4)
  {
    staticNoise(k, ticks);
    return;
  }
  k.r.fillRect(0, 0, W, H, rgb(6, 6, 16));
  k.r.drawText("CH " + ch, 120, 90, {64.0f, rgb(120, 255, 120), kInk, true});
  card(k, genre, rgb(255, 255, 255), 64, 320);
}

void credits(ClipKit& k, int ticks)
{
  k.r.fillRect(0, 0, W, H, rgb(6, 4, 14));
  static const char* const kLines[] = {
    "GUNRUNNERS", "", "A JUMP-N-SHOOT IN THE SPIRIT OF DUKE NUKEM II", "", "STARRING", "DASH", "ROCCO", "NOVA", "",
    "AND", "MAX", "", "ENGINE LOGIC PORTED FROM RIGELENGINE", "BY NIKOLAI WUTTKE (GPL-2.0-OR-LATER)", "",
    "ALL ART AND SOUND MADE BY CODE", "", "THANKS FOR PLAYING", "", "STAY TUNED"};
  float y = H - float(ticks) * 1.2f;
  for (const char* l : kLines)
  {
    if (y > -60 && y < H + 60)
      card(k, l, rgb(255, 255, 255), 34, y);
    y += 60;
  }
}

// Briefings: MAX over the level's own backdrop.
void briefing(ClipKit& k, const std::string& clip, int frame, int ticks, float ox, float oy, float t)
{
  drawBackdrop(k.r, k.art, float(ticks) * 2.0f + ox, oy, 0.0f);
  k.r.fillRect(0, 0, W, H, rgba(10, 6, 24, 90));
  // The diner table the hologram rises from.
  k.r.fillRect(200 + ox, 528 + oy, 880, 18, rgb(205, 208, 228));
  k.r.fillRect(200 + ox, 546 + oy, 880, 6, rgb(110, 112, 134));
  k.r.fillRect(200 + ox, 552 + oy, 880, 168, rgb(36, 22, 52));
  // The projector puck under the hologram.
  k.r.fillRect(596 + ox, 518 + oy, 88, 10, rgb(70, 210, 255));
  const float rise = std::min(1.0f, float(ticks) / 30.0f);
  const float scramble = frame < 4 ? 1.0f - float(frame) / 4.0f : 0.0f;
  drawMax(k, 640 + ox, 330 + oy + (1.0f - rise) * 200.0f, 0.95f, ticks, scramble);
  // A small sign inside the hologram keeps the beat on Episode 1 briefings.
  if (clip == "max_holo_rooftop")
  {
    const bool lit = frame % 16 < 12;
    k.r.fillRect(880 + ox, 200 + oy, 140, 50, lit ? rgba(255, 60, 200, 200) : rgba(80, 40, 80, 120));
    k.r.drawText("OPEN", 950 + ox, 210 + oy, {26.0f, lit ? rgb(255, 255, 255) : rgb(120, 100, 130), kInk, true}, Align::Center);
  }
  // Level 2: a thin glass tower turning 15 degrees a frame beside MAX.
  if (clip == "max_holo_tower")
  {
    const float a = float(frame) * float(kPi) / 12.0f;
    const float cx = 950 + ox, top = 110 + oy, bottom = 500 + oy;
    const float f1 = 70.0f * std::fabs(std::cos(a)), f2 = 70.0f * std::fabs(std::sin(a));
    const float left = cx - (f1 + f2) * 0.5f;
    k.r.fillRect(left, top, f1, bottom - top, rgba(120, 230, 255, 110), Blend::Add);
    k.r.fillRect(left + f1, top, f2, bottom - top, rgba(60, 150, 230, 90), Blend::Add);
    for (float y = top + 12; y < bottom; y += 18)
      k.r.fillRect(left, y, f1 + f2, 2, rgba(180, 250, 255, 90), Blend::Add);
    k.r.fillRect(cx - 1, top - 40, 2, 40, rgba(180, 250, 255, 160), Blend::Add);
    drawGlow(k.r, k.art, cx, top - 40, 16, rgb(255, 80, 80), 0.6f + 0.3f * float(frame % 2));
  }
  // Level 3: a tiny club, its dance floor changing colour on every beat.
  if (clip == "max_holo_club")
  {
    const float x0 = 860 + ox, y0 = 170 + oy, w = 220, h = 250;
    k.r.fillRect(x0, y0, w, 4, rgba(120, 230, 255, 160), Blend::Add);
    k.r.fillRect(x0, y0, 4, h, rgba(120, 230, 255, 160), Blend::Add);
    k.r.fillRect(x0 + w - 4, y0, 4, h, rgba(120, 230, 255, 160), Blend::Add);
    static const Color kTiles[4] = {rgb(255, 60, 200), rgb(0, 230, 255), rgb(255, 230, 60), rgb(160, 90, 255)};
    for (int i = 0; i < 5; ++i)
      for (int j = 0; j < 2; ++j)
      {
        const unsigned hsh = hash2(i * 3 + j, frame / 4);
        k.r.fillRect(x0 + 10 + float(i) * 41.0f, y0 + h - 40 + float(j) * 18.0f, 37, 15, withAlpha(kTiles[hsh % 4u], 170),
          Blend::Add);
      }
    // Dancers hop on the beat; a mirror ball turns above them.
    for (int d = 0; d < 4; ++d)
    {
      const float hop = (frame / 4 + d) % 2 ? 8.0f : 0.0f;
      const float dx = x0 + 34 + float(d) * 48.0f, dy = y0 + h - 82 - hop;
      k.r.fillRect(dx, dy, 14, 34, rgba(150, 240, 255, 130), Blend::Add);
      k.r.fillRect(dx + 2, dy - 14, 10, 10, rgba(150, 240, 255, 150), Blend::Add);
    }
    drawGlow(k.r, k.art, x0 + w * 0.5f, y0 + 50, 26, rgb(255, 255, 255), 0.5f + 0.3f * float(frame % 2));
    k.r.fillRect(x0 + w * 0.5f - 1, y0, 2, 30, rgba(150, 240, 255, 160), Blend::Add);
  }
  // Level 4: a street map in four sectors, each pulsing in turn.
  if (clip == "max_holo_district")
  {
    const float x0 = 850 + ox, y0 = 190 + oy;
    for (int i = 0; i < 4; ++i)
    {
      const float x = x0 + float(i) * 58.0f;
      const bool hot = (frame / 4) % 4 == i;
      k.r.fillRect(x, y0, 52, 220, rgba(120, 230, 255, hot ? 120 : 50), Blend::Add);
      k.r.fillRect(x, y0, 52, 3, rgba(180, 250, 255, 200), Blend::Add);
      k.r.drawText(std::string(1, char('A' + i)), x + 26, y0 + 90, {28.0f, rgb(200, 250, 255), kInk, true}, Align::Center);
    }
    k.r.fillRect(x0 - 10, y0 + 230, 252, 4, rgba(180, 250, 255, 180), Blend::Add); // the street
  }
  // Level 5: a manhole cover sliding aside over frames 4-9.
  if (clip == "max_holo_manhole")
  {
    const float cx = 970 + ox, cy = 320 + oy;
    const float slide = float(std::clamp(frame - 4, 0, 5)) * 18.0f;
    drawGlow(k.r, k.art, cx, cy, 80, rgb(140, 255, 90), 0.25f + 0.05f * float(std::clamp(frame - 4, 0, 5)));
    k.r.fillRect(cx - 70, cy - 22, 140, 44, rgba(20, 60, 20, 160)); // the hole
    k.r.fillRect(cx - 70 + slide, cy - 26, 140, 52, rgba(120, 230, 255, 110), Blend::Add);
    for (int i = 0; i < 5; ++i)
      k.r.fillRect(cx - 60 + slide + float(i) * 26.0f, cy - 20, 4, 40, rgba(200, 250, 255, 170), Blend::Add);
    k.r.fillRect(cx - 120, cy + 34, 240, 3, rgba(180, 250, 255, 180), Blend::Add);
  }
  (void)t;
}

// Level 5: the crew on a wet street around a sewer grate; a lime glow pulses
// up through it, Nova pinches her nose from frame 6 on, steam pans left.
void grateGlow(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  k.r.fillRect(0, 0, W, H, rgb(14, 18, 26));
  k.r.fillRect(0, 400 + oy, W, H - 400, rgb(30, 34, 42));
  // Wet street: streaks of reflected neon.
  for (int i = 0; i < 12; ++i)
    k.r.fillRect(float(hash2(i, 5) % 1200u) + ox, 420.0f + float(hash2(i, 6) % 120u) + oy, 90, 3,
      i % 2 ? rgba(255, 60, 200, 70) : rgba(0, 220, 255, 60));
  // The grate and its glow (8-frame pulse).
  const float pulse = 0.45f + 0.25f * std::sin(float(frame % 8) / 8.0f * 6.283f);
  drawGlow(k.r, k.art, 640 + ox, 500 + oy, 260, rgb(140, 255, 70), pulse);
  k.r.fillRect(520 + ox, 485 + oy, 240, 34, rgb(16, 22, 14));
  for (int i = 0; i < 9; ++i)
    k.r.fillRect(530 + float(i) * 26.0f + ox, 485 + oy, 10, 34, rgb(90, 100, 90));
  k.r.draw(runner(k, 0, 0, 2.6f), 360 + ox, 520 + oy);
  k.r.draw(runner(k, 1, 0, 2.6f), 880 + ox, 530 + oy);
  k.r.draw(runner(k, 2, 0, 2.6f, true), 1060 + ox, 520 + oy);
  if (frame >= 6)
  {
    // Nova's hand on her nose.
    k.r.fillRect(1040 + ox, 310 + oy, 22, 14, rgb(230, 190, 160));
    k.r.drawText("!", 1100 + ox, 220 + oy, {30.0f, rgb(190, 255, 120), kInk, true}, Align::Center);
  }
  // Steam drifting left, 2 px a frame, in front of everything.
  for (int i = 0; i < 8; ++i)
  {
    const float x = std::fmod(float(hash2(i, 9) % 1500u) - float(ticks) * 2.0f / 7.5f + 3000.0f, 1500.0f) - 100.0f;
    drawGlow(k.r, k.art, x + ox, 470.0f - float(i % 3) * 50.0f + oy, 90, rgb(200, 220, 210), 0.18f);
  }
}

// Level 4: the briefing in a power cut. Only the hologram, the window's dim
// outline and three pairs of eyes show; the eyes blink on frames 6 and 9.
void blackoutTable(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  k.r.fillRect(0, 0, W, H, rgb(2, 2, 8));
  const Color edge = rgba(60, 80, 140, 90);
  k.r.fillRect(140 + ox, 80 + oy, 1000, 3, edge);
  k.r.fillRect(140 + ox, 440 + oy, 1000, 3, edge);
  k.r.fillRect(140 + ox, 80 + oy, 3, 360, edge);
  k.r.fillRect(1137 + ox, 80 + oy, 3, 360, edge);
  k.r.fillRect(640 + ox, 80 + oy, 3, 360, edge);
  drawMax(k, 640 + ox, 330 + oy, 0.95f, ticks, 0.0f);
  const bool blink = frame == 6 || frame == 9;
  for (int who = 0; who < 3; ++who)
  {
    const float cx = 300.0f + float(who) * 340.0f + (who == 1 ? 0.0f : 40.0f) + ox, cy = 492.0f + oy;
    for (int s : {-1, 1})
      k.r.fillRect(cx + float(s) * 16.0f - 6.0f, cy - (blink ? 1.0f : 5.0f), 12, blink ? 2.0f : 10.0f, rgb(240, 240, 255));
  }
}

// Level 3: the three runners in their courier jackets under a disco ball
// whose reflections sweep across them; Rocco looks down at himself.
void crewOutfits(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  drawBackdrop(k.r, k.art, float(ticks) * 0.5f + ox, oy, 0.0f);
  k.r.fillRect(0, 0, W, H, rgba(0, 0, 0, 110));
  k.r.draw(runner(k, 0, 0, 3.0f), 400 + ox, 680 + oy);
  k.r.draw(runner(k, 1, frame >= 10 ? 9 : 0, 3.0f), 640 + ox, 690 + oy);
  k.r.draw(runner(k, 2, 0, 3.0f, true), 880 + ox, 680 + oy);
  // Mud and road dust on the jackets.
  for (int i = 0; i < 18; ++i)
  {
    const float x = 330.0f + float(hash2(i, 41) % 620u), y = 420.0f + float(hash2(i, 42) % 160u);
    k.r.fillRect(x + ox, y + oy, 6, 4, rgba(70, 50, 30, 120));
  }
  // The reflections: spots of light drifting left to right, 8 px a frame.
  const float sweep = float(ticks) * 8.0f / 7.2f;
  for (int i = 0; i < 26; ++i)
  {
    const float x = std::fmod(float(hash2(i, 7) % 1400u) + sweep, 1400.0f) - 60.0f;
    const float y = 120.0f + float(hash2(i, 8) % 520u);
    const Color c = i % 3 == 0 ? rgb(255, 255, 255) : (i % 3 == 1 ? rgb(255, 120, 220) : rgb(120, 220, 255));
    drawGlow(k.r, k.art, x + ox, y + oy, 16, c, 0.55f);
  }
  drawGlow(k.r, k.art, 640 + ox, 40 + oy, 60, rgb(255, 255, 255), 0.6f);
}

// Level 2: Dash from behind at the foot of the Halcyon tower. The mirrored
// facade slides down so the camera seems to tilt up; the top never shows.
void towerTilt(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  k.r.fillRect(0, 0, W, H, rgb(226, 150, 180));
  const float total = float(ticks) * 2.0f;
  const float pan = std::fmod(total, 96.0f);
  for (int j = -1; j < 8; ++j)
    for (int col = 0; col < 9; ++col)
    {
      const float x = 100.0f + float(col) * 120.0f + ox, y = float(j) * 96.0f + pan;
      const unsigned h = hash2(col, j - int(total / 96.0f) + 1000);
      k.r.fillRect(x, y + oy, 110, 86, (h % 7u) == 0 ? rgb(255, 214, 180) : rgb(70, 104, 150));
      k.r.fillRect(x, y + oy, 110, 6, rgb(200, 230, 255));
      k.r.fillRect(x + 8, y + 14 + oy, 20, 60, rgba(255, 255, 255, 50));
    }
  k.r.fillRect(0, 0, 100 + ox, H, rgb(40, 50, 80));
  k.r.fillRect(1180 + ox, 0, 100, H, rgb(40, 50, 80));
  // Dash from behind: jacket, neck, spiky hair, the goggle strap.
  const Texture& back = cached(k, "dash_back", 520, 520, 260, 470, [](cairo_t* cr) {
    cairo_move_to(cr, 40, 520);
    cairo_curve_to(cr, 40, 380, 120, 330, 260, 330);
    cairo_curve_to(cr, 400, 330, 480, 380, 480, 520);
    cairo_close_path(cr);
    setColor(cr, rgb(214, 48, 52));
    cairo_fill_preserve(cr);
    setColor(cr, kInk);
    cairo_set_line_width(cr, 6);
    cairo_stroke(cr);
    cairo_rectangle(cr, 220, 270, 80, 70);
    setColor(cr, rgb(232, 176, 140));
    cairo_fill(cr);
    cairo_arc(cr, 260, 210, 95, 0, 2 * kPi);
    setColor(cr, rgb(250, 206, 70));
    cairo_fill(cr);
    for (int i = 0; i < 7; ++i)
    {
      const double a = kPi + kPi * (double(i) + 0.5) / 7.0;
      cairo_move_to(cr, 260 + std::cos(a - 0.25) * 80, 210 + std::sin(a - 0.25) * 80);
      cairo_line_to(cr, 260 + std::cos(a) * 160, 210 + std::sin(a) * 150);
      cairo_line_to(cr, 260 + std::cos(a + 0.25) * 80, 210 + std::sin(a + 0.25) * 80);
      cairo_close_path(cr);
      cairo_fill(cr);
    }
    cairo_rectangle(cr, 166, 200, 188, 18);
    setColor(cr, rgb(40, 200, 230));
    cairo_fill(cr);
  });
  // Looking up: the head leans back over the first frames.
  DrawOpts o;
  o.scale = 0.62f;
  k.r.draw(back, 960 + ox, 548 + oy + float(std::min(frame, 5)) * 3.0f, o);
}

void runners(ClipKit& k, int ticks, float ox, float oy)
{
  drawBackdrop(k.r, k.art, float(ticks) + ox, oy, 0.0f);
  k.r.fillRect(0, 0, W, H, rgba(0, 0, 0, 80));
  k.r.draw(runner(k, 0, 0, 3.0f), 400 + ox, 680 + oy);
  k.r.draw(runner(k, 1, 0, 3.0f), 640 + ox, 690 + oy);
  k.r.draw(runner(k, 2, 0, 3.0f, true), 880 + ox, 680 + oy);
}

void closeUp(ClipKit& k, int who, int ticks, float ox, float oy, int pose = 0)
{
  drawBackdrop(k.r, k.art, float(ticks) + ox, oy, 0.0f);
  k.r.fillRect(0, 0, W, H, rgba(0, 0, 0, 100));
  k.r.draw(runner(k, who, pose, 5.0f), 640 + ox, 860 + oy);
}

// Episode 1's end: the wreck in the harbour, the case, the lens.
void wreck(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  k.r.fillRect(0, 0, W, H, rgb(8, 10, 30));
  panLayer(k, skyline(k, 1, rgb(20, 14, 40), rgb(255, 200, 120)), float(ticks) * 0.3f, 220);
  k.r.fillRect(0, 520, W, 200, rgb(10, 20, 50));
  for (int i = 0; i < 12; ++i)
    k.r.fillRect(float(hash2(i, 3) % 1280u), 540.0f + float(i * 14), 120, 3, rgba(120, 160, 255, 80));
  // The chopper hull, tail up.
  k.r.fillRect(420 + ox, 440 + oy, 360, 110, rgb(30, 30, 38));
  k.r.fillRect(760 + ox, 380 + oy, 240, 30, rgb(30, 30, 38));
  k.r.fillRect(980 + ox, 330 + oy, 20, 80, rgb(30, 30, 38));
  for (int i = 0; i < 8; ++i)
  {
    const float sy = 430.0f - float((ticks + i * 15) % 120) * 3.0f;
    const float sx = 560.0f + std::sin(float(ticks + i * 30) * 0.03f) * 30.0f;
    k.r.fillRect(sx - 30 + ox, sy + oy, 60, 40, rgba(90, 90, 100, 120 - (ticks + i * 15) % 120));
  }
  drawGlow(k.r, k.art, 600 + ox, 460 + oy, 140, rgb(255, 120, 40), 0.3f + 0.1f * float(frame % 3));
}

void siteNight(ClipKit& k, int ticks, float ox, float oy);

// The case from the wreck: open on the dock planks, a pistol in grey foam,
// HALCYON ARMS stamped on the lid; a glint runs along the barrel.
void caseClip(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  k.r.fillRect(0, 0, W, H, rgb(10, 12, 26));
  for (int i = 0; i < 9; ++i)
    k.r.fillRect(0, float(i) * 80.0f + oy, W, 76, i % 2 ? rgb(40, 30, 26) : rgb(46, 34, 28));
  const Texture& box = cached(k, "e1_case", 760, 560, 380, 280, [](cairo_t* cr) {
    // The lid, tipped up behind.
    cairo_rectangle(cr, 40, 20, 680, 220);
    cairo_set_source_rgb(cr, 0.16, 0.17, 0.2);
    cairo_fill(cr);
    cairo_rectangle(cr, 60, 40, 640, 180);
    cairo_set_source_rgb(cr, 0.22, 0.23, 0.27);
    cairo_fill(cr);
    selectGameFont(cr);
    cairo_set_font_size(cr, 54);
    cairo_text_extents_t te;
    cairo_text_extents(cr, "HALCYON ARMS", &te);
    cairo_move_to(cr, 380 - te.width / 2 - te.x_bearing, 150);
    cairo_set_source_rgb(cr, 0.85, 0.72, 0.3);
    cairo_show_text(cr, "HALCYON ARMS");
    cairo_new_path(cr);
    cairo_arc(cr, 380, 80, 20, 0, 2 * kPi);
    cairo_set_line_width(cr, 4);
    cairo_stroke(cr);
    // The tray with its foam.
    cairo_rectangle(cr, 20, 250, 720, 290);
    cairo_set_source_rgb(cr, 0.12, 0.13, 0.15);
    cairo_fill(cr);
    cairo_rectangle(cr, 50, 275, 660, 240);
    cairo_set_source_rgb(cr, 0.3, 0.3, 0.33);
    cairo_fill(cr);
    for (int y = 285; y < 510; y += 16)
      for (int x = 60; x < 700; x += 16)
      {
        cairo_arc(cr, x + ((y / 16) % 2) * 8, y, 3, 0, 2 * kPi);
        cairo_set_source_rgba(cr, 0, 0, 0, 0.18);
        cairo_fill(cr);
      }
    // The pistol, sunk in its cut-out.
    cairo_set_source_rgb(cr, 0.07, 0.07, 0.09);
    cairo_rectangle(cr, 190, 340, 390, 70);
    cairo_fill(cr);
    cairo_move_to(cr, 250, 400);
    cairo_line_to(cr, 330, 400);
    cairo_line_to(cr, 300, 500);
    cairo_line_to(cr, 220, 500);
    cairo_close_path(cr);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 0.55, 0.58, 0.64);
    cairo_rectangle(cr, 200, 348, 370, 46);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 0.95, 0.4, 0.2);
    cairo_rectangle(cr, 200, 386, 370, 6);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 0.25, 0.25, 0.3);
    cairo_move_to(cr, 258, 396);
    cairo_line_to(cr, 322, 396);
    cairo_line_to(cr, 296, 488);
    cairo_line_to(cr, 232, 488);
    cairo_close_path(cr);
    cairo_fill(cr);
  });
  k.r.draw(box, 640 + ox, 380 + oy);
  const float gx = 340.0f + float(frame % 6) / 5.0f * 360.0f;
  drawGlow(k.r, k.art, gx + ox, 355 + oy, 50, rgb(255, 255, 255), 0.7f);
  drawGlow(k.r, k.art, 640 + ox, 380 + oy, 500, rgb(255, 210, 140), 0.08f + 0.02f * std::sin(float(ticks) * 0.05f));
}

// The lens from the wreck, in a 260 x 260 box: a black barrel, its rings
// and the violet glass. (end_e3's studio camera wears the same one.)
void paintLensGlass(cairo_t* cr)
{
  cairo_arc(cr, 130, 130, 126, 0, 2 * kPi);
  cairo_set_source_rgb(cr, 0.1, 0.1, 0.12);
  cairo_fill(cr);
  for (int i = 0; i < 4; ++i)
  {
    cairo_arc(cr, 130, 130, 112 - i * 22, 0, 2 * kPi);
    cairo_set_source_rgb(cr, 0.18 + i * 0.03, 0.18 + i * 0.03, 0.22 + i * 0.03);
    cairo_set_line_width(cr, 6);
    cairo_stroke(cr);
  }
  cairo_pattern_t* g = cairo_pattern_create_radial(110, 105, 4, 130, 130, 70);
  cairo_pattern_add_color_stop_rgb(g, 0.0, 0.7, 0.4, 0.9);
  cairo_pattern_add_color_stop_rgb(g, 0.5, 0.15, 0.25, 0.55);
  cairo_pattern_add_color_stop_rgb(g, 1.0, 0.03, 0.05, 0.12);
  cairo_arc(cr, 130, 130, 64, 0, 2 * kPi);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
  cairo_arc(cr, 108, 104, 12, 0, 2 * kPi);
  cairo_set_source_rgba(cr, 1, 1, 1, 0.8);
  cairo_fill(cr);
}

// The lens: Rocco turns a big camera lens over in his hands (frames 0-4),
// then the lens's own view of him, a camera's viewfinder with REC
// blinking (frames 5-9). Someone has been filming the crew.
void lensClip(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  const Texture& glass = cached(k, "e1_lens", 260, 260, 130, 130, paintLensGlass);
  if (frame < 5)
  {
    siteNight(k, ticks, ox, oy);
    k.r.fillRect(0, 0, W, H, rgba(0, 0, 0, 120));
    k.r.draw(runner(k, 1, 0, 5.0f), 640 + ox, 860 + oy);
    DrawOpts o;
    o.scale = 0.9f + 0.05f * float(frame);
    k.r.draw(glass, 700 + ox, 470 + oy, o);
    drawGlow(k.r, k.art, 690 + ox, 455 + oy, 40, rgb(200, 160, 255), 0.6f);
    return;
  }
  // Through the lens: Rocco big and bent at the edges, the frame lines of a
  // TV camera over him.
  siteNight(k, ticks, ox * 0.5f, oy * 0.5f);
  k.r.fillRect(0, 0, W, H, rgba(20, 0, 40, 90));
  DrawOpts o;
  o.scale = 1.15f;
  k.r.draw(runner(k, 1, 0, 6.0f), 640 + ox, 980 + oy, o);
  // A dark vignette, the round edge of the glass.
  for (int i = 0; i < 12; ++i)
  {
    const float inset = float(i) * 14.0f;
    k.r.fillRect(0, 0, W, inset, rgba(0, 0, 0, 30));
    k.r.fillRect(0, H - inset, W, inset, rgba(0, 0, 0, 30));
    k.r.fillRect(0, 0, inset * 2.0f, H, rgba(0, 0, 0, 30));
    k.r.fillRect(W - inset * 2.0f, 0, inset * 2.0f, H, rgba(0, 0, 0, 30));
  }
  const Color line = rgba(255, 255, 255, 200);
  for (int sx : {-1, 1})
    for (int sy : {-1, 1})
    {
      const float cx = W / 2 + float(sx) * 520.0f, cy = H / 2 + float(sy) * 280.0f;
      k.r.fillRect(cx - (sx > 0 ? 60.0f : 0.0f), cy - (sy > 0 ? 4.0f : 0.0f), 60, 4, line);
      k.r.fillRect(cx - (sx > 0 ? 4.0f : 0.0f), cy - (sy > 0 ? 60.0f : 0.0f), 4, 60, line);
    }
  k.r.fillRect(W / 2 - 20, H / 2 - 1, 40, 2, line);
  k.r.fillRect(W / 2 - 1, H / 2 - 20, 2, 40, line);
  if ((ticks / 30) % 2 == 0)
    drawGlow(k.r, k.art, 172, 120, 24, rgb(255, 30, 30), 1.0f);
  k.r.drawText("REC", 196, 104, {30.0f, rgb(255, 255, 255), kInk, false}, Align::Left);
  k.r.drawText("CH 42  LIVE", W - 150, 104, {26.0f, rgb(255, 255, 255), kInk, false}, Align::Right);
}

// Level 6: the rain, the city and the guideway far below a bridge; the
// crew at the railing, from behind. The maglev's headlight grows out of the
// dark over frames 0-15.
void rainStreaks(ClipKit& k, int ticks, float ox, float oy)
{
  for (int i = 0; i < 70; ++i)
  {
    const float x = float(hash2(i, 61) % 1400u) - float(ticks % 40) * 3.0f;
    const float y = std::fmod(float(hash2(i, 62) % 720u) + float(ticks) * 18.0f, 760.0f) - 40.0f;
    k.r.fillRect(x + ox, y + oy, 2, 26, rgba(170, 200, 255, 90));
  }
}

void bridgeNight(ClipKit& k, int ticks, float ox, float oy)
{
  k.r.fillRect(0, 0, W, H, rgb(10, 12, 30));
  panLayer(k, skyline(k, 0, rgb(18, 18, 44), rgb(255, 210, 140)), float(ticks) * 0.25f + ox, 60 + oy);
  panLayer(k, skyline(k, 1, rgb(26, 24, 56), rgb(120, 220, 255)), float(ticks) * 0.5f + ox, 160 + oy);
  // The guideway: a pale beam on pylons, far below.
  k.r.fillRect(0, 560 + oy, W, 14, rgb(120, 130, 160));
  for (int i = 0; i < 6; ++i)
    k.r.fillRect(float(i) * 240.0f + 60.0f + ox, 574 + oy, 18, 146, rgb(60, 66, 90));
}

void bridgeRail(ClipKit& k, float ox, float oy)
{
  k.r.fillRect(0, 610 + oy, W, 16, rgb(70, 74, 92));
  k.r.fillRect(0, 610 + oy, W, 3, rgb(180, 190, 220));
  for (int i = 0; i < 17; ++i)
    k.r.fillRect(float(i) * 80.0f + ox, 626 + oy, 10, 100, rgb(50, 54, 70));
}

void bridgeWait(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  bridgeNight(k, ticks, ox, oy);
  const float grow = std::min(1.0f, float(frame) / 15.0f);
  drawGlow(k.r, k.art, 1180 + ox, 540 + oy, 30.0f + 160.0f * grow, rgb(255, 250, 220), 0.2f + 0.6f * grow);
  bridgeRail(k, ox, oy);
  k.r.draw(runner(k, 0, 0, 2.4f, true), 420 + ox, 640 + oy);
  k.r.draw(runner(k, 1, 0, 2.4f, true), 640 + ox, 648 + oy);
  k.r.draw(runner(k, 2, 0, 2.4f, true), 860 + ox, 640 + oy);
  rainStreaks(k, ticks, ox, oy);
}

// The maglev streaks under the bridge, right to left, 64 px a frame.
void maglevPass(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  bridgeNight(k, ticks, ox, oy);
  const float head = 1400.0f - float(frame) * 64.0f - float(ticks % 8) * 8.0f;
  for (int car = 0; car < 9; ++car)
  {
    const float x = head + float(car) * 300.0f;
    if (x > W || x + 290.0f < 0.0f)
      continue;
    k.r.fillRect(x + ox, 470 + oy, 290, 90, rgb(190, 198, 214));
    k.r.fillRect(x + ox, 470 + oy, 290, 10, rgb(240, 244, 255));
    k.r.fillRect(x + ox, 505 + oy, 290, 18, rgb(40, 60, 90));
    for (int w = 0; w < 6; ++w)
      k.r.fillRect(x + 16.0f + float(w) * 46.0f + ox, 508 + oy, 32, 12, rgba(255, 230, 160, 220));
    k.r.fillRect(x + 4 + ox, 540 + oy, 10, 6, rgb(255, 40, 50));
    k.r.fillRect(x + 276 + ox, 540 + oy, 10, 6, rgb(255, 40, 50));
  }
  if (head < W && head > -2800.0f)
    drawGlow(k.r, k.art, head + ox, 520 + oy, 120, rgb(255, 250, 220), 0.7f);
  for (int i = 0; i < 24; ++i)
  {
    const float y = 440.0f + float(hash2(i, 71) % 140u);
    const float x = std::fmod(float(hash2(i, 72) % 1600u) - float(ticks) * 40.0f + 16000.0f, 1600.0f) - 200.0f;
    k.r.fillRect(x + ox, y + oy, 180, 2, rgba(220, 240, 255, 110));
  }
  bridgeRail(k, ox, oy);
  rainStreaks(k, ticks, ox, oy);
}

// The three runners leap off the railing (frames 0-7) and hang in the air
// above the passing roofs on frame 8.
void theJump(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  bridgeNight(k, ticks, ox, oy);
  k.r.fillRect(0, 470 + oy, W, 90, rgb(190, 198, 214));
  k.r.fillRect(0, 470 + oy, W, 10, rgb(240, 244, 255));
  for (int i = 0; i < 20; ++i)
  {
    const float x = std::fmod(float(i) * 70.0f - float(std::min(frame, 8)) * 64.0f + 2800.0f, 1400.0f) - 60.0f;
    k.r.fillRect(x + ox, 508 + oy, 32, 12, rgba(255, 230, 160, 220));
  }
  bridgeRail(k, ox, oy);
  const float u = float(std::min(frame, 8)) / 8.0f;
  const float lift = std::sin(u * 1.5708f) * 300.0f;
  for (int who = 0; who < 3; ++who)
  {
    const float x = 420.0f + float(who) * 220.0f + u * 120.0f;
    const float y = 640.0f - lift + float(who) * 12.0f;
    k.r.draw(runner(k, who, frame < 1 ? 6 : 2, 2.4f, false), x + ox, y + oy);
  }
  rainStreaks(k, frame >= 8 ? 0 : ticks, ox, oy);
}

// Level 7: night on the building site, the crew among red girders. A white
// searchlight cone sweeps left to right across them (frames 0-15), the
// rotor's shadow flickers over everything on frames 0, 4, 8 and 12, and
// plastic sheeting snaps in the foreground.
void siteNight(ClipKit& k, int ticks, float ox, float oy)
{
  k.r.fillRect(0, 0, W, H, rgb(6, 10, 24));
  panLayer(k, skyline(k, 0, rgb(16, 22, 44), rgb(255, 200, 120)), float(ticks) * 0.2f + ox, 120 + oy);
  // Girders: two levels of red I-beams on posts.
  for (int level = 0; level < 2; ++level)
  {
    const float y = 300.0f + float(level) * 240.0f;
    for (int i = 0; i < 3; ++i)
    {
      const float x = float(i) * 470.0f - 60.0f + float(level) * 200.0f;
      k.r.fillRect(x + ox, y + oy, 380, 34, rgb(150, 44, 30));
      k.r.fillRect(x + ox, y + oy, 380, 8, rgb(220, 80, 50));
      for (float hx = x + 30.0f; hx < x + 360.0f; hx += 70.0f)
        k.r.fillRect(hx + ox, y + 12.0f + oy, 26, 12, rgb(70, 22, 16));
      k.r.fillRect(x + 20.0f + ox, y + 34.0f + oy, 14, 240, rgb(90, 30, 22));
    }
  }
}

void searchCone(ClipKit& k, float spotX, float spotY, float ox, float oy, float alpha)
{
  const float srcX = spotX - 160.0f, srcY = -40.0f;
  for (int i = -5; i <= 5; ++i)
    k.r.drawLine(srcX + ox, srcY + oy, spotX + float(i) * 22.0f + ox, spotY + oy, 30.0f,
      rgba(230, 240, 255, int(alpha * 22.0f)));
  drawGlow(k.r, k.art, spotX + ox, spotY + oy, 150, rgb(240, 248, 255), 0.45f * alpha);
}

void searchlightSweep(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  siteNight(k, ticks, ox, oy);
  k.r.draw(runner(k, 0, 0, 2.2f), 420 + ox, 535 + oy);
  k.r.draw(runner(k, 1, 0, 2.2f), 650 + ox, 540 + oy);
  k.r.draw(runner(k, 2, 0, 2.2f, true), 880 + ox, 535 + oy);
  const float u = float(frame % 16) / 15.0f;
  searchCone(k, 200.0f + u * 900.0f, 520.0f, ox, oy, 1.0f);
  if (frame % 4 == 0)
    k.r.fillRect(0, 0, W, H, rgba(0, 0, 0, 90)); // the rotor's shadow
  // Plastic sheeting in the foreground, snapping in the downdraft.
  const float snap = float((frame % 4) - 2) * 14.0f;
  for (int i = 0; i < 6; ++i)
  {
    const float x = 40.0f + float(i) * 34.0f + snap * float(i % 2 ? 1 : -1);
    k.r.fillRect(x + ox, 0 + oy, 34, H, rgba(220, 230, 240, 40));
    k.r.drawLine(x + 4.0f + ox, oy, x + 4.0f + snap + ox, H + oy, 2.0f, rgba(255, 255, 255, 50));
  }
}

// Rocco close up, squinting into the light; the cone pulses on his face.
void roccoSquint(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  siteNight(k, ticks, ox, oy);
  k.r.fillRect(0, 0, W, H, rgba(0, 0, 0, 110));
  k.r.draw(runner(k, 1, 0, 5.0f), 640 + ox, 860 + oy);
  const float pulse = 0.6f + 0.4f * std::sin(float(frame % 8) / 8.0f * 6.2832f);
  searchCone(k, 650.0f, 430.0f, ox, oy, pulse);
  // The glare full on his face.
  drawGlow(k.r, k.art, 660 + ox, 420 + oy, 220, rgb(255, 255, 240), 0.35f * pulse);
}

// Level 8: morning mist over the jungle canopy, the stepped pyramid on the
// horizon. Treetops pan left under Nova's courier ship, the crew in its
// open hatch and MAX flickering on the console behind them.
const Texture& canopyTrees(ClipKit& k, int layer)
{
  return cached(k, "canopy" + std::to_string(layer), 2560, 360, 0, 0, [layer](cairo_t* cr) {
    Rng rng(std::uint32_t(41 + layer * 7));
    const Color c = layer == 0 ? rgb(40, 84, 58) : rgb(22, 56, 34);
    for (double x = -60; x < 2620; x += rng.range(50.0f, 110.0f))
    {
      const double r = layer == 0 ? rng.range(50.0f, 90.0f) : rng.range(80.0f, 140.0f);
      const double y = (layer == 0 ? 170 : 150) + rng.range(-30.0f, 40.0f);
      cairo_arc(cr, x, y, r, 0, 2 * kPi);
      setColor(cr, c);
      cairo_fill(cr);
      cairo_arc(cr, x - r * 0.25, y - r * 0.3, r * 0.55, 0, 2 * kPi);
      setColor(cr, lighten(c, 0.12f));
      cairo_fill(cr);
    }
    cairo_rectangle(cr, 0, layer == 0 ? 220 : 200, 2560, 160);
    setColor(cr, c);
    cairo_fill(cr);
  });
}

void canopyMorning(ClipKit& k, int ticks, float ox, float oy)
{
  const Texture& sky = cached(k, "canopy_sky", 1280, 720, 0, 0, [](cairo_t* cr) {
    gradient(cr, 1280, 720, rgb(150, 196, 190), rgb(214, 228, 200), rgb(240, 236, 210));
    // The stepped pyramid on the horizon.
    for (int step = 0; step < 6; ++step)
    {
      const double w = 300 - step * 44, y = 430 - step * 26;
      cairo_rectangle(cr, 900 - w / 2, y, w, 28);
      cairo_set_source_rgba(cr, 0.42, 0.48, 0.44, 0.7);
      cairo_fill(cr);
    }
    cairo_rectangle(cr, 880, 254, 40, 22);
    cairo_set_source_rgba(cr, 0.42, 0.48, 0.44, 0.7);
    cairo_fill(cr);
  });
  k.r.draw(sky, ox * 0.2f, oy * 0.2f);
  panLayer(k, canopyTrees(k, 0), float(ticks) * 1.0f + ox, 380 + oy);
  for (int i = 0; i < 5; ++i) // mist banks
    k.r.fillRect(0, 470.0f + float(i) * 30.0f + oy, W, 22, rgba(240, 244, 236, 50));
  panLayer(k, canopyTrees(k, 1), float(ticks) * 2.0f + ox, 470 + oy);
}

void canopyShip(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  canopyMorning(k, ticks, ox, oy);
  const float bob = (frame % 12) < 6 ? 0.0f : 2.0f;
  const float sx = 330 + ox, sy = 230 + oy + bob;
  // The hull, side-on: a long wedge with a tail fin and engine glow.
  k.r.fillRect(sx, sy, 620, 150, rgb(70, 76, 92));
  k.r.fillRect(sx, sy, 620, 12, rgb(150, 160, 180));
  k.r.fillRect(sx + 620, sy + 30, 70, 90, rgb(70, 76, 92));
  k.r.fillRect(sx + 690, sy + 52, 40, 46, rgb(110, 116, 130));
  k.r.fillRect(sx - 90, sy - 50, 110, 60, rgb(60, 66, 82));
  k.r.fillRect(sx + 40, sy + 150, 520, 16, rgb(44, 48, 60));
  drawGlow(k.r, k.art, sx - 20, sy + 90, 70, rgb(120, 220, 255), 0.6f + 0.2f * float(frame % 3) / 2.0f);
  // The open hatch, MAX on the console at the back.
  k.r.fillRect(sx + 120, sy + 18, 380, 126, rgb(26, 24, 34));
  drawMax(k, sx + 440, sy + 64, 0.22f, ticks, (frame % 7) == 0 ? 0.6f : 0.0f);
  k.r.draw(runner(k, 0, 0, 1.4f), sx + 180, sy + 142);
  k.r.draw(runner(k, 1, 0, 1.4f), sx + 270, sy + 142);
  k.r.draw(runner(k, 2, 0, 1.4f, true), sx + 360, sy + 142);
  // Leaves sweeping past up close.
  for (int i = 0; i < 6; ++i)
  {
    const float x = std::fmod(float(hash2(i, 81) % 1600u) - float(ticks) * 5.0f + 16000.0f, 1600.0f) - 160.0f;
    const float y = 520.0f + float(hash2(i, 82) % 200u);
    k.r.fillRect(x + ox * 1.5f, y + oy, 120, 34, rgba(30, 90, 40, 230));
    k.r.fillRect(x + 10 + ox * 1.5f, y + 14 + oy, 100, 4, rgba(90, 160, 80, 230));
  }
}

// The canopy from above; the runners drop past the camera one by one and
// Nova looks back up at her ship.
void canopyJump(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  const Texture& below = cached(k, "canopy_below", 1280, 720, 0, 0, [](cairo_t* cr) {
    cairo_rectangle(cr, 0, 0, 1280, 720);
    setColor(cr, rgb(20, 52, 30));
    cairo_fill(cr);
    for (int i = 0; i < 70; ++i)
    {
      const double x = double(hash2(i, 5) % 1400u) - 60, y = double(hash2(i, 6) % 800u) - 40;
      const double r = 40 + double(hash2(i, 7) % 70u);
      cairo_arc(cr, x, y, r, 0, 2 * kPi);
      setColor(cr, lerpColor(rgb(36, 90, 48), rgb(90, 150, 70), float(hash2(i, 8) % 100u) / 100.0f));
      cairo_fill(cr);
    }
  });
  k.r.draw(below, ox * 0.5f, oy * 0.5f);
  for (int who = 0; who < 3; ++who)
  {
    const int f = frame - who * 4;
    if (f < 0)
      continue;
    // Falling away from the camera: smaller and lower each frame.
    const float u = std::min(1.0f, float(f) / 12.0f);
    const float scale = 4.0f - u * 3.0f;
    const bool lookBack = who == 2 && frame >= 10;
    k.r.draw(runner(k, who, lookBack ? 0 : 2, std::round(scale * 4.0f) / 4.0f, who == 1),
      420.0f + float(who) * 220.0f + ox, 760.0f - u * 260.0f + oy);
  }
  for (int i = 0; i < 4; ++i) // mist
    k.r.fillRect(0, float(i) * 180.0f + std::fmod(float(ticks) * 3.0f, 180.0f) + oy, W, 60, rgba(240, 244, 236, 40));
}

// Level 9's briefing: the temple door by torchlight.
void templeWall(ClipKit& k, float ox, float oy)
{
  const Texture& wall = cached(k, "temple_wall", 1280, 720, 0, 0, [](cairo_t* cr) {
    gradient(cr, 1280, 720, rgb(70, 44, 24), rgb(120, 82, 46), rgb(60, 38, 22));
    Rng rng(909u);
    for (int y = 0; y < 720; y += 48)
      for (int x = (y / 48) % 2 ? -60 : 0; x < 1280; x += 120)
      {
        cairo_rectangle(cr, x + 3, y + 3, 114, 42);
        setColor(cr, withAlpha(lerpColor(rgb(150, 110, 64), rgb(190, 150, 96), rng.uniform()), 120));
        cairo_fill(cr);
      }
    // The carved door: a tall slab in a stepped frame.
    for (int step = 0; step < 3; ++step)
    {
      const double inset = step * 26.0;
      cairo_rectangle(cr, 450 + inset, 90 + inset, 380 - inset * 2, 630 - inset);
      setColor(cr, lerpColor(rgb(110, 76, 42), rgb(60, 40, 22), float(step) / 3.0f));
      cairo_fill(cr);
    }
    // Red glyphs down both sides of the frame and over the lintel.
    cairo_set_line_width(cr, 6);
    cairo_set_source_rgb(cr, 0.86, 0.22, 0.16);
    for (int i = 0; i < 6; ++i)
    {
      for (double x : {400.0, 860.0})
      {
        const double y = 140 + i * 90.0;
        if (i % 3 == 0)
        {
          cairo_arc(cr, x + 10, y + 20, 14, 0, 2 * kPi);
          cairo_stroke(cr);
        }
        else if (i % 3 == 1)
        {
          cairo_move_to(cr, x + 10, y);
          cairo_line_to(cr, x + 10, y + 50);
          cairo_move_to(cr, x - 6, y + 18);
          cairo_line_to(cr, x + 26, y + 18);
          cairo_stroke(cr);
        }
        else
        {
          cairo_move_to(cr, x - 4, y + 40);
          cairo_line_to(cr, x + 8, y + 6);
          cairo_line_to(cr, x + 24, y + 40);
          cairo_stroke(cr);
        }
      }
    }
    for (int i = 0; i < 7; ++i)
    {
      cairo_rectangle(cr, 480 + i * 48.0, 40, 26, 26);
      cairo_stroke(cr);
    }
    // The floor and a glyph plate in front of the door.
    cairo_rectangle(cr, 0, 640, 1280, 80);
    setColor(cr, rgb(90, 62, 36));
    cairo_fill(cr);
  });
  k.r.draw(wall, ox * 0.3f, oy * 0.3f);
}

void torchPair(ClipKit& k, int frame, float ox, float oy)
{
  for (float tx : {250.0f, 1030.0f})
  {
    const float flick = 0.8f + 0.2f * float((frame + int(tx)) % 8) / 7.0f;
    k.r.fillRect(tx - 8 + ox, 300 + oy, 16, 120, rgb(80, 56, 34));
    drawGlow(k.r, k.art, tx + ox, 280 + oy, 220.0f * flick, rgb(255, 150, 60), 0.5f * flick);
    k.r.fillRect(tx - 14 + ox, 250 + oy + (1.0f - flick) * 20.0f, 28, 50.0f * flick, rgb(255, 190, 70));
    k.r.fillRect(tx - 7 + ox, 268 + oy, 14, 26.0f * flick, rgb(255, 245, 190));
  }
}

void templeDoor(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  (void)ticks;
  templeWall(k, ox, oy);
  const bool stepped = frame >= 14;
  // The plate sinks a pixel under Rocco.
  k.r.fillRect(580 + ox, 636 + oy + (stepped ? 1.0f : 0.0f), 120, 10, rgb(150, 110, 70));
  k.r.fillRect(630 + ox, 636 + oy + (stepped ? 1.0f : 0.0f), 20, 6, rgb(220, 50, 40));
  torchPair(k, frame, ox, oy);
  // Long shadows on the door, swaying with the flames.
  const float sway = float((frame % 8) - 4) * 0.8f;
  DrawOpts shadow;
  shadow.tint = rgb(0, 0, 0);
  shadow.alpha = 0.3f;
  shadow.angle = sway;
  for (int who = 0; who < 3; ++who)
  {
    const float x = 540.0f + float(who) * 100.0f + (who == 1 && stepped ? 30.0f : 0.0f);
    k.r.draw(runner(k, who, 0, 4.4f), x + ox, 650 + oy, shadow);
  }
  // The runners from behind: dark shapes rimmed by the torches.
  DrawOpts back;
  back.tint = rgb(40, 26, 18);
  for (int who = 0; who < 3; ++who)
  {
    const float x = 380.0f + float(who) * 260.0f + (who == 1 && stepped ? 210.0f - 260.0f + 50.0f : 0.0f);
    const float y = who == 1 && stepped ? 650.0f : 720.0f;
    k.r.draw(runner(k, who, 0, 3.4f, who == 2), x + ox, y + 20 + oy, back);
  }
}

void templeWait(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  (void)ticks;
  k.r.fillRect(0, 0, W, H, rgb(40, 26, 16));
  const float flick = 0.85f + 0.15f * float(frame % 8) / 7.0f;
  // The runners' faces, close, lit from the side.
  DrawOpts faces;
  faces.cull = false;
  for (int who = 0; who < 3; ++who)
    k.r.draw(runner(k, who, 0, 5.0f, who == 2), 260.0f + float(who) * 380.0f + ox, 800.0f + oy, faces);
  k.r.fillRect(0, 0, W, H, rgba(60, 30, 10, int(120 - 40 * flick)));
  drawGlow(k.r, k.art, 1180 + ox, 200 + oy, 420.0f * flick, rgb(255, 150, 60), 0.35f);
  // Rocco's boot on the plate, low across the frame.
  k.r.fillRect(0 + ox, 610 + oy, W, 110, rgb(96, 66, 38));
  k.r.fillRect(420 + ox, 596 + oy, 440, 24, rgb(150, 110, 70));
  k.r.fillRect(600 + ox, 600 + oy, 80, 10, rgb(220, 50, 40));
  k.r.fillRect(470 + ox, 470 + oy, 300, 130, rgb(70, 52, 40));
  k.r.fillRect(470 + ox, 560 + oy, 360, 40, rgb(46, 34, 28));
  k.r.fillRect(500 + ox, 480 + oy, 240, 16, rgb(110, 86, 64));
  // Dust trickles from the ceiling.
  if (frame >= 10)
    for (int i = 0; i < 24; ++i)
    {
      const float x = 200.0f + float(hash2(i, 91) % 880u);
      const float y = std::fmod(float(hash2(i, 92) % 400u) + float(ticks) * 6.0f, 600.0f);
      k.r.fillRect(x + ox, y + oy, 4, 10, rgba(230, 210, 170, 170));
    }
}

// Level 10's briefing: a marble hall, a shaft of sunlight across MAX.
void sunHall(ClipKit& k, float ox, float oy)
{
  const Texture& hall = cached(k, "sun_hall", 1280, 720, 0, 0, [](cairo_t* cr) {
    gradient(cr, 1280, 720, rgb(214, 204, 186), rgb(240, 232, 216), rgb(200, 190, 172));
    // Marble columns with gilt capitals.
    for (int i = 0; i < 5; ++i)
    {
      const double x = 60 + i * 300.0;
      cairo_rectangle(cr, x, 90, 80, 560);
      setColor(cr, rgb(232, 226, 214));
      cairo_fill(cr);
      for (int f = 1; f < 4; ++f)
      {
        cairo_rectangle(cr, x + f * 20.0 - 2, 110, 4, 540);
        setColor(cr, rgb(200, 192, 178));
        cairo_fill(cr);
      }
      cairo_rectangle(cr, x - 16, 70, 112, 26);
      setColor(cr, rgb(214, 172, 80));
      cairo_fill(cr);
    }
    // Gold leaf along the cornice, and the floor.
    cairo_rectangle(cr, 0, 40, 1280, 14);
    setColor(cr, rgb(214, 172, 80));
    cairo_fill(cr);
    cairo_rectangle(cr, 0, 640, 1280, 80);
    setColor(cr, rgb(196, 184, 164));
    cairo_fill(cr);
    for (int x = 0; x < 1280; x += 160)
    {
      cairo_rectangle(cr, x, 640, 2, 80);
      setColor(cr, rgb(170, 158, 140));
      cairo_fill(cr);
    }
  });
  k.r.draw(hall, ox * 0.3f, oy * 0.3f);
}

void sunBeam(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  sunHall(k, ox, oy);
  // The shaft of sunlight from the roof slit, motes drifting in it.
  const float sx = 700.0f + ox;
  for (int i = 0; i < 3; ++i)
    k.r.drawLine(sx - 40.0f + float(i) * 6.0f, 0, sx + 60.0f + float(i) * 6.0f, 720, 70.0f - float(i) * 22.0f,
      rgba(255, 230, 150, 40 + i * 30), Blend::Add);
  for (int i = 0; i < 26; ++i)
  {
    const float y = std::fmod(float(hash2(i, 31) % 700u) + float(frame % 12) * 6.0f + float(ticks) * 0.4f, 700.0f);
    const float x = sx - 30.0f + float(hash2(i, 32) % 80u) + y * 0.14f;
    k.r.fillRect(x, y + oy, 4, 4, rgba(255, 250, 220, 200));
  }
  // MAX on the pedestal, scrambling where the light crosses him.
  k.r.fillRect(600 + ox, 520 + oy, 200, 130, rgb(236, 230, 218));
  k.r.fillRect(590 + ox, 510 + oy, 220, 16, rgb(214, 172, 80));
  const int f = frame % 16;
  drawMax(k, 700 + ox, 330 + oy, 0.7f, ticks, f >= 6 && f <= 9 ? 0.7f : 0.0f);
  // The runners around it.
  for (int who = 0; who < 3; ++who)
  {
    const float x = who == 0 ? 330.0f : (who == 1 ? 1060.0f : 470.0f);
    k.r.draw(runner(k, who, 0, 2.6f, who == 1), x + ox, 650 + oy);
  }
  // A mirror statue's edge sliding past in the foreground.
  const float mx = -200.0f + float(frame) * 2.0f + float(ticks % 4) * 0.5f;
  k.r.fillRect(mx + ox, 120 + oy, 140, 600, rgb(244, 240, 232));
  k.r.fillRect(mx + 110 + ox, 120 + oy, 30, 600, rgb(214, 206, 192));
  drawGlow(k.r, k.art, mx + 70 + ox, 220 + oy, 120, rgb(255, 240, 200), 0.6f);
}

void sunUp(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  (void)ticks;
  // The dome from below: gilt ribs to the oculus, beams crossing.
  const Texture& dome = cached(k, "sun_dome", 1280, 720, 0, 0, [](cairo_t* cr) {
    gradient(cr, 1280, 720, rgb(250, 244, 226), rgb(226, 214, 190), rgb(186, 172, 150));
    for (int r = 6; r >= 1; --r)
    {
      cairo_arc(cr, 640, 140, r * 120.0, 0, 2 * kPi);
      setColor(cr, lerpColor(rgb(236, 228, 210), rgb(200, 188, 166), float(r) / 6.0f));
      cairo_fill(cr);
    }
    for (int i = 0; i < 16; ++i)
    {
      const double a = i * kPi / 8;
      cairo_move_to(cr, 640 + std::cos(a) * 80, 140 + std::sin(a) * 80);
      cairo_line_to(cr, 640 + std::cos(a) * 900, 140 + std::sin(a) * 900);
      cairo_set_line_width(cr, 8);
      setColor(cr, rgb(214, 172, 80));
      cairo_stroke(cr);
    }
    cairo_arc(cr, 640, 140, 80, 0, 2 * kPi);
    setColor(cr, rgb(255, 252, 236));
    cairo_fill(cr);
  });
  k.r.draw(dome, ox * 0.2f, oy * 0.2f);
  // Beams crossing the dome from mirror to mirror.
  const float pts[5][2] = {{120, 700}, {420, 420}, {860, 520}, {1100, 260}, {640, 140}};
  for (int i = 0; i < 4; ++i)
  {
    k.r.drawLine(pts[i][0] + ox, pts[i][1] + oy, pts[i + 1][0] + ox, pts[i + 1][1] + oy, 26.0f,
      rgba(255, 210, 120, 60), Blend::Add);
    k.r.drawLine(pts[i][0] + ox, pts[i][1] + oy, pts[i + 1][0] + ox, pts[i + 1][1] + oy, 6.0f,
      rgba(255, 250, 220, 220), Blend::Add);
  }
  // The runners looking up, low in the frame, the camera tilting up.
  const float tilt = float(frame) * 2.0f;
  for (int who = 0; who < 3; ++who)
    k.r.draw(runner(k, who, 0, 3.2f, who == 2), 360.0f + float(who) * 280.0f + ox, 900.0f + tilt + oy);
}

// Level 11's briefing: the mouth of the Idol Mines, timber shoring and a
// swinging lantern, rails running off into the dark.
void mineMouth(ClipKit& k, int ticks, float ox, float oy)
{
  const Texture& bg = cached(k, "mine_mouth", 1280, 720, 0, 0, [](cairo_t* cr) {
    gradient(cr, 1280, 720, rgb(60, 42, 30), rgb(40, 28, 20), rgb(22, 16, 12));
    // The rock face, rough blocks.
    for (int i = 0; i < 90; ++i)
    {
      const double x = double(hash2(i, 5) % 1280u), y = double(hash2(i, 6) % 720u);
      cairo_rectangle(cr, x, y, 60 + hash2(i, 7) % 60u, 30 + hash2(i, 8) % 30u);
      setColor(cr, rgba(0, 0, 0, 40));
      cairo_fill(cr);
    }
    // The tunnel mouth: black, with the shoring around it.
    cairo_move_to(cr, 380, 720);
    cairo_line_to(cr, 380, 250);
    cairo_line_to(cr, 900, 250);
    cairo_line_to(cr, 900, 720);
    cairo_close_path(cr);
    setColor(cr, rgb(6, 4, 3));
    cairo_fill(cr);
    for (const double x : {350.0, 900.0})
    {
      cairo_rectangle(cr, x, 230, 34, 490);
      setColor(cr, rgb(130, 90, 54));
      cairo_fill(cr);
    }
    cairo_rectangle(cr, 330, 214, 624, 40);
    setColor(cr, rgb(150, 104, 62));
    cairo_fill(cr);
    // Rails to the vanishing point, sleepers getting closer together.
    for (int i = 0; i < 14; ++i)
    {
      const double t = std::pow(double(i) / 14.0, 1.8);
      const double y = 720 - t * 300, half = 300 * (1 - t) + 30;
      cairo_rectangle(cr, 640 - half - 20, y - 8 * (1 - t) - 2, 2 * half + 40, 14 * (1 - t) + 3);
      setColor(cr, rgb(90, 60, 36));
      cairo_fill(cr);
    }
    for (const double side : {-1.0, 1.0})
    {
      cairo_move_to(cr, 640 + side * 330, 720);
      cairo_line_to(cr, 640 + side * 30, 420);
      cairo_set_line_width(cr, 10);
      setColor(cr, rgb(150, 150, 160));
      cairo_stroke(cr);
    }
  });
  k.r.draw(bg, ox * 0.3f, oy * 0.3f);
  // The lantern on the lintel, swinging on a 12-frame loop.
  const float a = 0.35f * std::sin(float(ticks % 48) / 48.0f * 6.2832f);
  const float lx = 520.0f + ox + std::sin(a) * 70.0f, ly = 260.0f + oy + std::cos(a) * 70.0f;
  k.r.drawLine(520.0f + ox, 254.0f + oy, lx, ly, 3.0f, rgb(40, 30, 20));
  drawGlow(k.r, k.art, lx, ly + 18.0f, 160, rgb(255, 170, 70), 0.6f);
  k.r.fillRect(lx - 14, ly, 28, 36, rgb(255, 196, 90));
  k.r.fillRect(lx - 16, ly - 4, 32, 6, rgb(60, 40, 24));
}

// A mine cart seen from the front, `s` its size (1 = 400 px wide).
void cartFront(ClipKit& k, float cx, float bottom, float s, bool painted)
{
  const float w = 400.0f * s, h = 220.0f * s;
  for (int i = 0; i < int(h); i += 2)
  {
    const float t = float(i) / h, half = w * (0.5f - 0.08f * t);
    k.r.fillRect(cx - half, bottom - h * 0.25f - h + float(i), half * 2, 2, lerpColor(rgb(150, 100, 60), rgb(110, 72, 44), t));
  }
  k.r.fillRect(cx - w * 0.52f, bottom - h * 1.25f - 10 * s, w * 1.04f, 18 * s, rgb(90, 90, 100));
  for (int b = -1; b <= 1; ++b)
    k.r.fillRect(cx + float(b) * w * 0.3f - 6 * s, bottom - h * 1.25f, 12 * s, h, rgb(80, 70, 66));
  for (const float side : {-1.0f, 1.0f})
  {
    k.r.fillRect(cx + side * w * 0.33f - 34 * s, bottom - 70 * s, 68 * s, 70 * s, rgb(40, 40, 46));
    k.r.fillRect(cx + side * w * 0.33f - 12 * s, bottom - 48 * s, 24 * s, 24 * s, rgb(140, 140, 150));
  }
  if (painted)
    k.r.drawText("42", cx, bottom - h * 0.9f, {90.0f * s, rgb(240, 200, 70), rgb(60, 40, 20)}, Align::Center);
}

void mineTunnel(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  mineMouth(k, ticks, ox, oy);
  // The empty cart rolls out of the dark toward the camera and stops at the
  // bumper on frame 16.
  const float t = std::min(1.0f, float(std::min(frame, 15)) / 15.0f);
  const float s = 0.15f + 0.85f * t * t;
  const float jolt = frame == 16 ? 8.0f : 0.0f;
  cartFront(k, 640 + ox, 430.0f + 290.0f * t * t + oy - jolt, s, true);
  // The bumper in the foreground.
  k.r.fillRect(520 + ox, 690 + oy, 240, 30, rgb(120, 84, 50));
  k.r.fillRect(540 + ox, 670 + oy, 40, 30, rgb(200, 60, 40));
  k.r.fillRect(700 + ox, 670 + oy, 40, 30, rgb(200, 60, 40));
  // The runners at the bottom edge, looking on.
  for (int who = 0; who < 3; ++who)
    k.r.draw(runner(k, who, 0, 3.0f, who == 2), 160.0f + float(who) * 480.0f + (who == 1 ? 340.0f : 0.0f) + ox, 900 + oy);
}

void mineHop(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  mineMouth(k, ticks, ox, oy);
  cartFront(k, 640 + ox, 700 + oy, 1.0f, true);
  // Dash vaults in, then Rocco and Nova pile in after him (frames 0, 4, 8).
  for (int who = 0; who < 3; ++who)
  {
    const int start = who * 4, f = frame - start;
    const float x = 640.0f + float(who - 1) * 110.0f + ox;
    if (f < 0)
    {
      k.r.draw(runner(k, who, 0, 2.2f, who == 1), (who == 0 ? 160.0f : (who == 1 ? 1120.0f : 1000.0f)) + ox, 720 + oy);
      continue;
    }
    const float u = std::min(1.0f, float(f) / 4.0f);
    const float from = who == 0 ? 160.0f : (who == 1 ? 1120.0f : 1000.0f);
    const float px = from + (x - from) * u, py = 720.0f - std::sin(u * 3.14159f) * 260.0f - u * 170.0f + oy;
    k.r.draw(runner(k, who, u < 1.0f ? 2 : 0, 2.2f, who == 1), px, py);
  }
  // The cart's front rim over their legs once they are in.
  k.r.fillRect(640 - 208 + ox, 700 - 275 + oy, 416, 18, rgb(90, 90, 100));
  for (int i = 0; i < 120; i += 2)
    k.r.fillRect(640 - 196 + ox + float(i) * 0.1f, 700 - 257 + oy + float(i), 392 - float(i) * 0.2f, 2,
      lerpColor(rgb(150, 100, 60), rgb(130, 88, 52), float(i) / 120.0f));
  k.r.drawText("42", 640 + ox, 700 - 190 + oy, {90.0f, rgb(240, 200, 70), rgb(60, 40, 20)}, Align::Center);
}

// Level 12: basalt columns over the magma, ash coming down.
void magmaLedge(ClipKit& k, int ticks, float ox, float oy)
{
  const Texture& bg = cached(k, "magma_ledge", 1280, 720, 0, 0, [](cairo_t* cr) {
    gradient(cr, 1280, 720, rgb(40, 16, 14), rgb(90, 30, 16), rgb(230, 90, 30));
    // Basalt columns, hexagonal tops catching the glow.
    for (int i = 0; i < 16; ++i)
    {
      const double x = double(i) * 84.0 - 20.0 + double(hash2(i, 3) % 30u);
      const double top = 120.0 + double(hash2(i, 4) % 260u), w = 70.0 + double(hash2(i, 5) % 20u);
      cairo_rectangle(cr, x, top, w, 720 - top);
      setColor(cr, rgb(34 + int(hash2(i, 6) % 14u), 30, 34));
      cairo_fill(cr);
      cairo_rectangle(cr, x, top, w, 10);
      setColor(cr, rgb(120, 60, 40));
      cairo_fill(cr);
      cairo_rectangle(cr, x + w - 8, top, 8, 720 - top);
      setColor(cr, rgba(255, 120, 40, 50));
      cairo_fill(cr);
    }
    // The lava along the bottom.
    cairo_rectangle(cr, 0, 600, 1280, 120);
    setColor(cr, rgb(250, 110, 30));
    cairo_fill(cr);
    cairo_rectangle(cr, 0, 600, 1280, 12);
    setColor(cr, rgb(255, 210, 90));
    cairo_fill(cr);
  });
  k.r.draw(bg, ox, oy);
  // Ash, a 12-frame loop.
  for (int i = 0; i < 60; ++i)
  {
    const float x = float(hash2(i, 9) % 1280u) + std::sin(float(ticks) * 0.05f + float(i)) * 10.0f;
    const float y = std::fmod(float(hash2(i, 10) % 720u) + float(ticks % 120) * 6.0f, 720.0f);
    k.r.fillRect(x + ox, y + oy, 4, 4, rgba(200, 190, 180, 140));
  }
  // The lava's slow churn.
  for (int i = 0; i < 12; ++i)
  {
    const float x = float(i) * 110.0f + std::sin(float(ticks) * 0.04f + float(i) * 1.7f) * 30.0f;
    k.r.fillRect(x + ox, 640 + oy + float(i % 3) * 22.0f, 70, 10, rgba(255, 220, 120, 120));
  }
}

void magmaHeat(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  magmaLedge(k, ticks, ox, oy);
  // The runners on a ledge over the lava; Rocco lifts a foot on frame 12.
  k.r.fillRect(140 + ox, 520 + oy, 1000, 80, rgb(30, 26, 30));
  k.r.fillRect(140 + ox, 520 + oy, 1000, 8, rgb(130, 64, 40));
  for (int who = 0; who < 3; ++who)
  {
    const float x = 340.0f + float(who) * 300.0f + ox;
    const float lift = who == 1 && frame >= 12 ? 18.0f : 0.0f;
    k.r.draw(runner(k, who, 0, 2.6f, who == 2), x, 520 + oy - lift);
    if (who == 1)
      for (int puff = 0; puff < 3; ++puff)
      {
        const int f = (ticks / 2 + puff * 2) % 6;
        k.r.fillRect(x - 30 + float(puff) * 26.0f, 500 + oy - float(f) * 14.0f, 18 + float(f) * 3.0f,
          18 + float(f) * 3.0f, rgba(200, 200, 200, 120 - f * 18));
      }
  }
  // The heat haze: a band wobbling 2 px a frame.
  for (int y = 0; y < 120; y += 6)
  {
    const float wob = std::sin(float(ticks) * 0.6f + float(y) * 0.3f) * 2.0f;
    k.r.fillRect(wob + ox, 400 + float(y) + oy, 1280, 3, rgba(255, 160, 80, 22));
  }
  // MAX from Dash's wrist, flickering in the haze.
  const float flicker = (ticks / 3) % 5 == 0 ? 0.35f : 0.0f;
  drawMax(k, 170 + ox, 300 + oy, 0.4f, ticks, flicker);
}

void magmaHop(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  magmaLedge(k, ticks, ox, oy);
  // Rocco hops from foot to foot on the ledge.
  k.r.fillRect(160 + ox, 520 + oy, 420, 80, rgb(30, 26, 30));
  k.r.fillRect(160 + ox, 520 + oy, 420, 8, rgb(130, 64, 40));
  k.r.draw(runner(k, 1, (frame / 2) % 2 == 0 ? 0 : 2, 2.6f), 360 + ox, 520 + oy - ((frame / 2) % 2 == 0 ? 0.0f : 24.0f));
  // Nova on a basalt stone that sinks 2 px a frame.
  const float sink = float(std::min(frame, 11)) * 3.0f;
  const float sy = 560.0f + sink + oy;
  k.r.fillRect(820 + ox, sy, 200, 70, rgb(44, 38, 42));
  k.r.fillRect(820 + ox, sy, 200, 10, rgb(140, 70, 44));
  k.r.draw(runner(k, 2, 0, 2.6f, true), 920 + ox, sy);
  // The lava closing over the stone's sides.
  k.r.fillRect(780 + ox, 600 + oy, 280, 120, rgba(250, 110, 30, 200));
  k.r.fillRect(780 + ox, 600 + oy, 280, 12, rgb(255, 210, 90));
}

// --- Episode 7, DEEP SPACE ---------------------------------------------------------

// The courier ship nose-down in the fungus forest, smoking, its cockpit
// canopy open.
void crashedShip(ClipKit& k, int ticks, float ox, float oy)
{
  const Texture& ship = cached(k, "crash_ship", 620, 360, 0, 0, [](cairo_t* cr) {
    cairo_translate(cr, 310, 200);
    cairo_rotate(cr, 0.32);
    // Hull.
    cairo_move_to(cr, -260, -30);
    cairo_curve_to(cr, -180, -90, 120, -100, 250, -20);
    cairo_curve_to(cr, 270, 0, 250, 30, 200, 50);
    cairo_line_to(cr, -240, 50);
    cairo_close_path(cr);
    fillGradientOutline(cr, -100, 50, rgb(190, 196, 214), rgb(90, 92, 116), rgb(20, 16, 34), 4);
    // Scorch and a stripe.
    cairo_rectangle(cr, -200, -10, 380, 14);
    setColor(cr, rgb(255, 120, 60));
    cairo_fill(cr);
    cairo_rectangle(cr, 60, -60, 160, 90);
    setColor(cr, rgba(30, 20, 30, 130));
    cairo_fill(cr);
    // The open canopy.
    cairo_move_to(cr, 90, -62);
    cairo_curve_to(cr, 120, -150, 200, -150, 230, -90);
    cairo_line_to(cr, 90, -62);
    fillOutline(cr, rgba(140, 230, 255, 150), rgb(20, 16, 34), 3);
    // A crumpled wing.
    cairo_move_to(cr, -120, 20);
    cairo_line_to(cr, -40, 20);
    cairo_line_to(cr, -150, 120);
    cairo_line_to(cr, -210, 110);
    cairo_close_path(cr);
    fillOutline(cr, rgb(120, 124, 150), rgb(20, 16, 34), 3);
  });
  k.r.draw(ship, 420 + ox, 260 + oy);
  // Smoke out of the engine, rising and spreading.
  for (int i = 0; i < 10; ++i)
  {
    const int f = (ticks / 3 + i * 7) % 70;
    const float x = 470.0f + float(f) * 1.6f + std::sin(float(f) * 0.2f + float(i)) * 12.0f;
    const float y = 330.0f - float(f) * 4.0f, s = 20.0f + float(f) * 0.9f;
    k.r.fillRect(x - s * 0.5f + ox, y - s * 0.5f + oy, s, s, rgba(70, 60, 90, 150 - f * 2));
  }
  // Sparks off the hull.
  if ((ticks / 4) % 6 == 0)
    drawGlow(k.r, k.art, 640 + ox, 380 + oy, 30, rgb(255, 200, 90), 0.9f);
}

void crashLanding(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  drawBackdrop(k.r, k.art, float(ticks) * 0.6f + ox, oy, 0.0f);
  crashedShip(k, ticks, ox, oy);
  // Glowing moss in front.
  k.r.fillRect(ox, 600 + oy, W, 120, rgb(34, 18, 54));
  k.r.fillRect(ox, 600 + oy, W, 10, rgb(80, 210, 160));
  // The runners climbing out, one by one.
  for (int who = 0; who < 3; ++who)
  {
    const int out = frame - who * 4;
    if (out < 0)
      continue;
    const float x = 760.0f + float(who) * 150.0f, drop = std::max(0.0f, 1.0f - float(out) / 4.0f) * 160.0f;
    k.r.draw(runner(k, who, 0, 2.4f, true), x + ox, 600 + oy - drop);
  }
  // MAX, crackling over the radio: a flickering hologram from Dash's wrist.
  const float flicker = (ticks / 4) % 7 == 0 ? 0.5f : 0.1f;
  drawMax(k, 1100 + ox, 260 + oy, 0.45f, ticks, flicker);
}

void gooWall(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  drawBackdrop(k.r, k.art, float(ticks) * 0.6f + 200.0f + ox, oy, 0.0f);
  // A wall of the planet's flesh-rock, coated in glowing goo.
  k.r.fillRect(700 + ox, oy, 580, H, rgb(44, 26, 64));
  for (int i = 0; i < 18; ++i)
    k.r.fillRect(720 + float(hash2(i, 5) % 540u) + ox, float(hash2(i, 6) % 700u) + oy, 26, 18, rgb(64, 40, 88));
  const float wob = std::sin(float(ticks) * 0.1f) * 4.0f;
  k.r.fillRect(684 + wob * 0.5f + ox, oy, 40, H, rgba(140, 250, 90, 220));
  k.r.fillRect(700 + ox, oy, 8, H, rgba(220, 255, 180, 160));
  for (int d = 0; d < 5; ++d)
  {
    const float y = std::fmod(float(hash2(d, 8) % 720u) + float(ticks) * 1.5f, 720.0f);
    k.r.fillRect(686 + ox, y + oy, 10, 26, rgba(170, 255, 120, 230));
  }
  drawGlow(k.r, k.art, 700 + ox, 360 + oy, 260, rgb(130, 255, 90), 0.25f);
  // Nova slides down the goo, back to the wall, then kicks off.
  const bool kicked = frame >= 8;
  const float t = kicked ? float(frame - 8) : 0.0f;
  const float y = kicked ? 380.0f - t * 40.0f + t * t * 4.0f : 260.0f + float(frame) * 12.0f;
  const float x = kicked ? 620.0f - t * 40.0f : 630.0f;
  k.r.draw(runner(k, 2, kicked ? 2 : 21, 2.4f, true), x + ox, y + oy);
  k.r.fillRect(ox, 640 + oy, 700, 80, rgb(34, 18, 54));
  k.r.fillRect(ox, 640 + oy, 700, 10, rgb(80, 210, 160));
}

// Level 43: in orbit over Vurr. The old courier ship's cockpit, the runners
// beaming in one by one, MAX crackling over the console.
void starfallCockpit(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  drawBackdrop(k.r, k.art, float(ticks) * 0.4f + ox, oy, 0.0f);
  // The canopy: two struts arching over, a console along the bottom.
  for (int s : {-1, 1})
  {
    const float x0 = 640.0f + float(s) * 700.0f, x1 = 640.0f + float(s) * 260.0f;
    k.r.drawLine(x0 + ox, 760 + oy, x1 + ox, -40 + oy, 46, rgb(40, 44, 60));
    k.r.drawLine(x0 + ox, 760 + oy, x1 + ox, -40 + oy, 8, rgb(110, 116, 140));
  }
  k.r.fillRect(ox, 560 + oy, W, 200, rgb(30, 32, 46));
  k.r.fillRect(ox, 560 + oy, W, 10, rgb(110, 116, 140));
  for (int i = 0; i < 14; ++i)
  {
    const bool lit = (hash2(i, ticks / 20) % 3u) != 0u;
    k.r.fillRect(70.0f + float(i) * 82.0f + ox, 600 + oy, 40, 14,
      lit ? (i % 3 ? rgb(90, 220, 255) : rgb(255, 190, 70)) : rgb(50, 60, 80));
  }
  // The runners beam in: a column of light, then the runner.
  for (int who = 0; who < 3; ++who)
  {
    const int in = frame - who * 2;
    if (in < 0)
      continue;
    const float x = 420.0f + float(who) * 210.0f + ox, y = 600 + oy;
    if (in < 3)
    {
      k.r.fillRect(x - 50, oy, 100, 600, rgba(170, 230, 255, 160 - in * 40));
      drawGlow(k.r, k.art, x, y - 120, 160, rgb(170, 230, 255), 0.8f - float(in) * 0.2f);
    }
    if (in >= 1)
      k.r.draw(runner(k, who, 0, 2.4f, who == 2), x, y);
  }
  drawMax(k, 1130 + ox, 420 + oy, 0.38f, ticks, (ticks / 4) % 6 == 0 ? 0.5f : 0.1f);
}

// The courier ship side-on, lasers firing, a big asteroid splitting.
void starfallRocks(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  drawBackdrop(k.r, k.art, float(ticks) * 2.0f + ox, oy, 0.0f);
  static const std::string kShip = "veh_spaceship", kRock = "drift_rock";
  // Small rocks tumbling past.
  for (int i = 0; i < 7; ++i)
  {
    const float x = std::fmod(float(hash2(i, 431) % 1500u) - float(ticks) * 3.0f + 15000.0f, 1500.0f) - 100.0f;
    const float y = 80.0f + float(hash2(i, 432) % 560u);
    k.r.draw(styledEnemySprite(k.art, k.r, k.theme, kRock, i % 3, 0, 2, 2).get(1), x + ox, y + oy);
  }
  const float bob = std::sin(float(ticks) * 0.08f) * 8.0f;
  DrawOpts big;
  big.scale = 1.6f;
  k.r.draw(styledEnemySprite(k.art, k.r, k.theme, kShip, 0, (ticks / 6) % 2, 8, 4).get(1), 330 + ox, 420 + oy + bob, big);
  if (frame % 2 == 0)
    for (int g : {0, 1})
    {
      const float y = 372.0f + float(g) * 36.0f + bob, x = 480.0f + float((ticks % 45) * 14);
      k.r.drawLine(x + ox, y + oy, x + 90 + ox, y + oy, 10, rgb(140, 240, 255), Blend::Add);
    }
  DrawOpts rock;
  rock.scale = 2.0f;
  if (frame < 8)
  {
    // Taking the hits, a little further off each time.
    const float x = 980.0f + float(frame) * 6.0f;
    k.r.draw(styledEnemySprite(k.art, k.r, k.theme, kRock, 0, 0, 6, 6).get(1), x + ox, 520 + oy, rock);
    if (frame % 2 == 0)
      drawGlow(k.r, k.art, x - 150 + ox, 400 + oy, 70, rgb(255, 230, 180), 0.8f);
  }
  else
  {
    // Split: the halves tumble up and down out of the way.
    const float t = float(frame - 8);
    for (int s : {-1, 1})
      k.r.draw(styledEnemySprite(k.art, k.r, k.theme, kRock, 1 + (s > 0), 0, 4, 4).get(1), 1030.0f + t * 10.0f + ox,
        460.0f + float(s) * (60.0f + t * 34.0f) + oy, rock);
    if (frame < 11)
      drawGlow(k.r, k.art, 1030 + ox, 420 + oy, 200, rgb(255, 210, 150), 0.8f - (t * 0.25f));
  }
}

// Level 45: inside the hive. A ribbed pink cavity, veins glowing on the
// heartbeat.
void hiveCavity(ClipKit& k, int ticks, float ox, float oy)
{
  const Texture& bg = cached(k, "hive_cavity", 1280, 720, 0, 0, [](cairo_t* cr) {
    gradient(cr, 1280, 720, rgb(40, 4, 22), rgb(100, 18, 52), rgb(60, 8, 30));
    // Ribs arching overhead.
    for (int i = 0; i < 8; ++i)
    {
      const double x = -100.0 + double(i) * 190.0;
      cairo_move_to(cr, x, 720);
      cairo_curve_to(cr, x + 20, 200, x + 200, 40, x + 420, 0);
      cairo_set_line_width(cr, 34);
      setColor(cr, rgba(200, 90, 130, 90));
      cairo_stroke(cr);
    }
    // Veins.
    for (int i = 0; i < 6; ++i)
    {
      const double y = 80.0 + double(i) * 90.0;
      cairo_move_to(cr, 0, y);
      cairo_curve_to(cr, 300, y - 40, 700, y + 60, 1280, y - 10);
      cairo_set_line_width(cr, 4);
      setColor(cr, rgba(120, 220, 255, 70));
      cairo_stroke(cr);
    }
    // The wet floor.
    cairo_move_to(cr, 0, 600);
    cairo_curve_to(cr, 400, 580, 880, 620, 1280, 596);
    cairo_line_to(cr, 1280, 720);
    cairo_line_to(cr, 0, 720);
    cairo_close_path(cr);
    fillGradientOutline(cr, 580, 720, rgb(220, 120, 150), rgb(110, 26, 60), rgb(50, 6, 26), 3);
  });
  k.r.draw(bg, ox, oy);
  // The heartbeat: a lub-dub of light.
  const int beat = ticks % 60;
  const float a = beat < 8 ? 1.0f - float(beat) / 8.0f : (beat >= 12 && beat < 20 ? 0.6f * (1.0f - float(beat - 12) / 8.0f) : 0.0f);
  if (a > 0.0f)
    k.r.fillRect(ox, oy, W, H, rgba(255, 40, 90, int(a * 40.0f)), Blend::Add);
}

// A gullet mouth in a wall, its lips pulsing open and shut.
void hiveMouth(ClipKit& k, float x, float y, float open, float ox, float oy)
{
  const int o = std::clamp(int(open * 4.0f), 0, 4);
  const Texture& lips = cached(k, "hive_mouth" + std::to_string(o), 220, 300, 110, 150, [o](cairo_t* cr) {
    const double open = double(o) / 4.0;
    cairo_save(cr);
    cairo_translate(cr, 110, 150);
    cairo_scale(cr, 80, 130);
    cairo_arc(cr, 0, 0, 1, 0, 6.2832);
    cairo_restore(cr);
    fillOutline(cr, rgb(240, 120, 160), rgb(40, 6, 24), 4);
    for (int i = 0; i < 14; ++i)
    {
      const double a = double(i) * 6.2832 / 14.0;
      cairo_move_to(cr, 110 + std::cos(a) * 74, 150 + std::sin(a) * 120);
      cairo_line_to(cr, 110 + std::cos(a) * (30 + 30 * open), 150 + std::sin(a) * (50 + 50 * open));
    }
    setColor(cr, rgba(150, 30, 80, 200));
    cairo_set_line_width(cr, 3);
    cairo_stroke(cr);
    cairo_save(cr);
    cairo_translate(cr, 110, 150);
    cairo_scale(cr, 10 + 50 * open, 16 + 90 * open);
    cairo_arc(cr, 0, 0, 1, 0, 6.2832);
    cairo_restore(cr);
    setColor(cr, rgb(30, 0, 14));
    cairo_fill(cr);
  });
  drawGlow(k.r, k.art, x + ox, y + oy, 200, rgb(255, 90, 140), 0.25f + 0.3f * open);
  k.r.draw(lips, x + ox, y + oy);
}

void hiveBrief(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  hiveCavity(k, ticks, ox, oy);
  const float open = 0.5f + 0.5f * std::sin(float(ticks) * 0.05f);
  hiveMouth(k, 1120, 420, open, ox, oy);
  // The runners, wary, facing the mouth.
  for (int who = 0; who < 3; ++who)
    k.r.draw(runner(k, who, who == 2 ? 3 : 0, 2.4f), 300.0f + float(who) * 190.0f + ox, 610 + oy);
  drawMax(k, 160 + ox, 260 + oy, 0.42f, ticks, (ticks / 5) % 9 == 0 ? 0.3f : 0.0f);
  (void)frame;
}

// The swallow: Dash walks into the mouth and shoots along a tube across the
// cavity, curled up, seen through its wall.
void hiveGulp(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  hiveCavity(k, ticks, ox, oy);
  // The tube: from the mouth on the left, up and across to the right.
  const float pts[4][2] = {{180, 420}, {180, 200}, {1100, 200}, {1100, 470}};
  for (int i = 1; i < 4; ++i)
  {
    k.r.drawLine(pts[i - 1][0] + ox, pts[i - 1][1] + oy, pts[i][0] + ox, pts[i][1] + oy, 90, rgba(110, 20, 60, 160));
    k.r.drawLine(pts[i - 1][0] + ox, pts[i - 1][1] + oy, pts[i][0] + ox, pts[i][1] + oy, 70, rgba(255, 140, 180, 100));
  }
  for (int i = 1; i < 3; ++i)
    k.r.fillRect(pts[i][0] - 45 + ox, pts[i][1] - 45 + oy, 90, 90, rgba(110, 20, 60, 160));
  hiveMouth(k, 180, 470, frame < 3 ? 1.0f : (frame < 5 ? 0.2f : 0.6f), ox, oy);
  if (frame < 3)
    k.r.draw(runner(k, 0, 1, 2.4f, true), 420.0f - float(frame) * 80.0f + ox, 610 + oy);
  else if (frame < 14)
  {
    // Along the tube: 11 frames for the whole way.
    const float t = float(frame - 3) / 10.0f;
    const float total = 220 + 920 + 270;
    float d = t * total, x = 180, y = 420;
    if (d < 220)
      y = 420 - d;
    else if (d < 1140)
    {
      y = 200;
      x = 180 + (d - 220);
    }
    else
    {
      x = 1100;
      y = 200 + (d - 1140);
    }
    DrawOpts o;
    o.angle = float(ticks % 24) * 15.0f;
    o.alpha = 0.85f;
    k.r.draw(runner(k, 0, 6, 1.6f), x + ox, y + 60 + oy, o);
    drawGlow(k.r, k.art, x + ox, y + oy, 80, rgb(255, 120, 170), 0.6f);
  }
  else
    k.r.draw(runner(k, 0, 0, 2.4f), 1100 + ox, 610 + oy);
}

// Level 46: two floating islands of crystal over the cloud sea, a gap too
// wide to jump between them.
void crystalIslands(ClipKit& k, int ticks, float ox, float oy)
{
  drawBackdrop(k.r, k.art, float(ticks) * 0.6f + ox, oy, 0.0f);
  const Texture& isle = cached(k, "crystal_isle", 520, 300, 260, 0, [](cairo_t* cr) {
    cairo_move_to(cr, 0, 0);
    cairo_line_to(cr, 520, 0);
    cairo_line_to(cr, 430, 120);
    cairo_line_to(cr, 330, 220);
    cairo_line_to(cr, 260, 300);
    cairo_line_to(cr, 180, 210);
    cairo_line_to(cr, 70, 110);
    cairo_close_path(cr);
    fillGradientOutline(cr, 0, 300, rgb(150, 120, 210), rgb(70, 46, 120), rgb(30, 16, 56), 4);
    cairo_rectangle(cr, 0, 0, 520, 14);
    setColor(cr, rgb(220, 205, 255));
    cairo_fill(cr);
    for (int i = 0; i < 6; ++i)
    {
      const double x = 40.0 + double(i) * 85.0, y = 40.0 + double(hash2(i, 46) % 90u);
      cairo_move_to(cr, x, y);
      cairo_line_to(cr, x + 18, y + 40);
      cairo_line_to(cr, x - 6, y + 52);
      cairo_close_path(cr);
      setColor(cr, rgba(110, 240, 230, 150));
      cairo_fill(cr);
    }
  });
  const float bob = std::sin(float(ticks) * 0.05f) * 6.0f;
  k.r.draw(isle, 230 + ox, 560 + oy + bob);
  k.r.draw(isle, 1080 + ox, 560 + oy - bob);
}

void crystalBrief(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  crystalIslands(k, ticks, ox, oy);
  const float bob = std::sin(float(ticks) * 0.05f) * 6.0f;
  // The crystal on the far island, ringing now and then.
  DrawOpts big;
  big.scale = 1.8f;
  drawGlow(k.r, k.art, 1100 + ox, 470 + oy - bob, 140, rgb(200, 150, 255), 0.5f + 0.2f * float(frame % 2));
  k.r.draw(styledEnemySprite(k.art, k.r, k.theme, "swap_crystal", 0, (ticks / 12) % 2, 2, 3).get(1), 1100 + ox,
    560 + oy - bob + std::sin(float(ticks) * 0.11f) * 6.0f, big);
  for (int who = 0; who < 3; ++who)
    k.r.draw(runner(k, who, who == 1 ? 3 : 0, 2.2f), 110.0f + float(who) * 120.0f + ox, 560 + oy + bob);
  drawMax(k, 640 + ox, 250 + oy, 0.4f, ticks, (ticks / 5) % 9 == 0 ? 0.3f : 0.0f);
}

// The swap: Dash shoots the crystal across the gap (frames 0-3), a flash
// (4-5), and Dash is on the far island with the crystal where Dash stood.
void crystalSwap(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  crystalIslands(k, ticks, ox, oy);
  const float bob = std::sin(float(ticks) * 0.05f) * 6.0f;
  const bool swapped = frame >= 5;
  const float crystalX = swapped ? 300.0f : 1100.0f, crystalBob = swapped ? bob : -bob;
  DrawOpts big;
  big.scale = 1.8f;
  drawGlow(k.r, k.art, crystalX + ox, 470 + oy + crystalBob, 140, rgb(200, 150, 255), frame == 4 || frame == 5 ? 1.0f : 0.4f);
  k.r.draw(styledEnemySprite(k.art, k.r, k.theme, "swap_crystal", frame == 4 || frame == 5 ? 2 : 0, (ticks / 12) % 2, 2, 3)
             .get(1),
    crystalX + ox, 560 + oy + crystalBob, big);
  k.r.draw(runner(k, 0, 0, 2.2f), (swapped ? 1100.0f : 300.0f) + ox, 560 + oy + (swapped ? -bob : bob));
  for (int who = 1; who < 3; ++who)
    k.r.draw(runner(k, who, 0, 2.2f), 110.0f + float(who - 1) * 100.0f + ox, 560 + oy + bob);
  if (frame < 4)
  {
    const float x = 380.0f + float(frame) * 180.0f;
    k.r.drawLine(x + ox, 480 + oy, x + 120 + ox, 480 + oy, 12, rgb(255, 230, 120), Blend::Add);
  }
  if (frame == 4 || frame == 5)
    for (const float x : {300.0f, 1100.0f})
      drawGlow(k.r, k.art, x + ox, 480 + oy, 260, rgb(240, 225, 255), 0.9f);
}

// Level 47: the canyon: a ledge high on the left wall, one lower on the
// right, and a silk line slung between them.
void silkCanyon(ClipKit& k, int ticks, float ox, float oy)
{
  drawBackdrop(k.r, k.art, float(ticks) * 0.4f + ox, oy, 0.0f);
  const Texture& walls = cached(k, "silk_walls", 1280, 720, 0, 0, [](cairo_t* cr) {
    // The left wall with its ledge, the right wall with its lower one.
    cairo_move_to(cr, 0, 0);
    cairo_line_to(cr, 300, 0);
    cairo_line_to(cr, 280, 320);
    cairo_line_to(cr, 330, 330);
    cairo_line_to(cr, 330, 360);
    cairo_line_to(cr, 250, 380);
    cairo_line_to(cr, 270, 720);
    cairo_line_to(cr, 0, 720);
    cairo_close_path(cr);
    fillGradientOutline(cr, 0, 720, rgb(210, 140, 80), rgb(110, 56, 26), rgb(46, 22, 12), 4);
    cairo_move_to(cr, 1280, 0);
    cairo_line_to(cr, 1010, 0);
    cairo_line_to(cr, 1030, 520);
    cairo_line_to(cr, 960, 540);
    cairo_line_to(cr, 960, 570);
    cairo_line_to(cr, 1040, 590);
    cairo_line_to(cr, 1020, 720);
    cairo_line_to(cr, 1280, 720);
    cairo_close_path(cr);
    fillGradientOutline(cr, 0, 720, rgb(210, 140, 80), rgb(110, 56, 26), rgb(46, 22, 12), 4);
    for (int i = 0; i < 9; ++i)
    {
      const double y = 60.0 + double(i) * 75.0;
      cairo_move_to(cr, 0, y);
      cairo_line_to(cr, 250, y + 8);
      cairo_move_to(cr, 1030, y + 20);
      cairo_line_to(cr, 1280, y + 12);
      cairo_set_line_width(cr, 4);
      setColor(cr, rgba(250, 210, 140, 80));
      cairo_stroke(cr);
    }
  });
  k.r.draw(walls, ox, oy);
  // The line, from over the left ledge down to over the right one.
  k.r.drawLine(330 + ox, 250 + oy, 960 + ox, 470 + oy, 7, rgba(90, 70, 50, 90));
  k.r.drawLine(330 + ox, 250 + oy, 960 + ox, 470 + oy, 3.5f, rgb(246, 240, 224));
}

void silkBrief(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  silkCanyon(k, ticks, ox, oy);
  for (int who = 0; who < 3; ++who)
    k.r.draw(runner(k, who, who == 2 ? 3 : 0, 1.6f), 90.0f + float(who) * 80.0f + ox, 330 + oy);
  // A Loom Spider on the line, walking its top.
  const float t = 0.26f + 0.06f * std::sin(float(ticks) * 0.08f);
  DrawOpts big;
  big.scale = 1.5f;
  k.r.draw(styledEnemySprite(k.art, k.r, k.theme, "loom_spider", frame % 8 >= 6 ? 1 : 0, (ticks / 6) % 2, 3, 3).get(1),
    330 + 630 * t + ox, 250 + 220 * t + 72 + oy, big);
  drawMax(k, 640 + ox, 160 + oy, 0.4f, ticks, (ticks / 5) % 9 == 0 ? 0.3f : 0.0f);
}

// The ride: Dash hangs from the line and slides down it, faster and
// faster, and drops onto the far ledge (frames 0-11), then waves.
void silkRide(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  silkCanyon(k, ticks, ox, oy);
  const float t = std::min(1.0f, float(frame * frame) / 121.0f);
  const float x = 330 + 630 * t, y = 250 + 220 * t;
  if (frame < 11)
  {
    DrawOpts o;
    o.angle = 10.0f;
    k.r.draw(runner(k, 0, 19, 1.6f), x + ox, y + 150 + oy, o); // hanging
    if (frame > 3)
      k.r.drawLine(x - 80 + ox, y - 28 + oy, x - 20 + ox, y - 7 + oy, 4, rgba(255, 250, 230, 160), Blend::Add);
  }
  else
    k.r.draw(runner(k, 0, frame >= 13 ? 3 : 0, 1.6f), 1010 + ox, 540 + oy);
  for (int who = 1; who < 3; ++who)
    k.r.draw(runner(k, who, 0, 1.6f), 90.0f + float(who) * 80.0f + ox, 330 + oy);
}

// Level 48: the plains at dusk, a strip of violet grass and thorns across
// the front, a pen of logs on the right.
void plainsField(ClipKit& k, int ticks, float ox, float oy)
{
  drawBackdrop(k.r, k.art, float(ticks) * 0.6f + ox, oy, 0.0f);
  const Texture& ground = cached(k, "plains_ground", 1280, 720, 0, 0, [](cairo_t* cr) {
    cairo_rectangle(cr, 0, 590, 1280, 130);
    fillGradientOutline(cr, 590, 720, rgb(150, 100, 170), rgb(70, 40, 70), rgb(36, 20, 40), 3);
    for (int x = 0; x < 1280; x += 9)
    {
      const double h = 10 + (x * 37 % 23);
      cairo_move_to(cr, x, 594);
      cairo_line_to(cr, x + 4 + (x % 5), 594 - h);
      cairo_line_to(cr, x + 6, 594);
      cairo_close_path(cr);
      setColor(cr, x % 2 ? rgb(190, 130, 230) : rgb(140, 80, 190));
      cairo_fill(cr);
    }
    // Thorns in the middle stretch.
    for (int x = 520; x < 780; x += 14)
    {
      cairo_move_to(cr, x, 596);
      cairo_line_to(cr, x + 7, 556);
      cairo_line_to(cr, x + 14, 596);
      cairo_close_path(cr);
      fillOutline(cr, rgb(170, 70, 220), rgb(36, 20, 40), 2);
    }
    // The pen: two posts and a rail.
    for (const double x : {960.0, 1220.0})
    {
      cairo_rectangle(cr, x, 500, 16, 96);
      fillOutline(cr, rgb(176, 150, 120), rgb(36, 20, 40), 3);
    }
    cairo_rectangle(cr, 950, 520, 290, 12);
    fillOutline(cr, rgb(196, 170, 140), rgb(36, 20, 40), 3);
  });
  k.r.draw(ground, ox, oy);
}

void plainsBrief(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  plainsField(k, ticks, ox, oy);
  for (int who = 0; who < 3; ++who)
    k.r.draw(runner(k, who, who == 1 ? 3 : 0, 1.6f), 120.0f + float(who) * 90.0f + ox, 592 + oy);
  // A Bounder in its pen, hopping on the spot now and then.
  const int hop = frame % 8;
  const float up = hop >= 5 ? std::sin(float(hop - 5) / 3.0f * 3.14159f) * 90.0f : 0.0f;
  DrawOpts big;
  big.scale = 1.1f;
  k.r.draw(styledEnemySprite(k.art, k.r, k.theme, "veh_bounder", 0, up > 0.0f ? 2 : (ticks / 10) % 2, 6, 7).get(-1),
    1090 + ox, 600 + oy - up, big);
  // A Thorn Hog pacing past the thorns.
  const float hx = 640 + 120 * std::sin(float(ticks) * 0.02f);
  k.r.draw(styledEnemySprite(k.art, k.r, k.theme, "thorn_hog", 0, (ticks / 6) % 2, 4, 3)
             .get(std::cos(float(ticks) * 0.02f) >= 0 ? 1 : -1),
    hx + ox, 596 + oy);
  drawMax(k, 640 + ox, 230 + oy, 0.4f, ticks, (ticks / 5) % 9 == 0 ? 0.3f : 0.0f);
}

// The ride: Nova jumps onto the Bounder's back (frames 0-3) and it leaps
// in a long arc over the thorns (4-13) and lands beyond them (14-15).
void plainsRide(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  plainsField(k, ticks, ox, oy);
  for (int who = 0; who < 2; ++who)
    k.r.draw(runner(k, who, 0, 1.6f), 120.0f + float(who) * 90.0f + ox, 592 + oy);
  float bx = 420, by = 600;
  int pose = 0;
  if (frame >= 4 && frame <= 13)
  {
    const float t = float(frame - 4) / 9.0f;
    bx = 420 + 520 * t;
    by = 600 - std::sin(t * 3.14159f) * 260.0f;
    pose = t < 0.5f ? 2 : 3;
  }
  else if (frame > 13)
    bx = 940;
  DrawOpts big;
  big.scale = 1.1f;
  k.r.draw(styledEnemySprite(k.art, k.r, k.theme, "veh_bounder", 0, pose, 6, 7).get(1), bx + ox, by + oy, big);
  if (frame < 2)
    k.r.draw(runner(k, 2, 2, 1.6f), 300 + 50.0f * float(frame) + ox, 470 + oy);
  else
    k.r.draw(runner(k, 2, 4, 1.4f), bx - 10 + ox, by - 120 + oy);
}

// Level 49: the egg chamber, amber light, her throne on the right with
// the Hive Mother on it (pose: world_mother.cpp's sprite poses), the scale
// she is drawn at.
void motherChamber(ClipKit& k, int ticks, int pose, float sink, float ox, float oy)
{
  hiveCavity(k, ticks, ox, oy);
  k.r.fillRect(ox, oy, W, H, rgba(255, 150, 60, 34), Blend::Add);
  DrawOpts big;
  big.scale = 0.62f;
  k.r.draw(styledEnemySprite(k.art, k.r, k.theme, "mother_throne", 0, (ticks / 10) % 2, 20, 8).get(1), 960 + ox,
    640 + oy, big);
  DrawOpts her = big;
  if (pose == 6)
    her.alpha = std::max(0.0f, 1.0f - sink / 260.0f);
  k.r.draw(styledEnemySprite(k.art, k.r, k.theme, "hive_mother", pose, (ticks / 12) % 2, 24, 20).get(-1), 960 + ox,
    640 + oy + sink, her);
}

void motherBrief(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  motherChamber(k, ticks, 5, 0.0f, ox, oy);
  for (int who = 0; who < 3; ++who)
    k.r.draw(runner(k, who, who == 0 ? 3 : 0, 1.6f), 110.0f + float(who) * 90.0f + ox, 640 + oy);
  drawMax(k, 420 + ox, 250 + oy, 0.4f, ticks, (ticks / 5) % 9 == 0 ? 0.3f : 0.0f);
  (void)frame;
}

// She wakes: rises off the throne (frames 0-5), her crown lights, and she
// bows to roll an egg out at the runners (6-11).
void motherWake(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  const int pose = frame < 3 ? 5 : (frame < 7 ? 0 : 1);
  motherChamber(k, ticks, pose, 0.0f, ox, oy);
  if (frame >= 3)
    drawGlow(k.r, k.art, (pose == 1 ? 820.0f : 990.0f) + ox, (pose == 1 ? 470.0f : 300.0f) + oy, 140,
      rgb(170, 255, 90), 0.5f + 0.3f * std::sin(float(ticks) * 0.3f));
  if (frame >= 8)
  {
    DrawOpts egg;
    egg.scale = 1.4f;
    k.r.draw(styledEnemySprite(k.art, k.r, k.theme, "brood_egg", 0, (ticks / 6) % 2, 3, 3).get(-1),
      760.0f - float(frame - 8) * 90.0f + ox, 650 + oy, egg);
  }
  for (int who = 0; who < 3; ++who)
    k.r.draw(runner(k, who, frame >= 5 ? 4 : 0, 1.6f), 110.0f + float(who) * 90.0f + ox, 640 + oy);
}

// Episode 7's end, 1: she falls. She shakes in flashes (frames 0-5), then
// sinks into her throne in bursts of resin (6-13).
void motherFalls(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  const float sink = frame < 6 ? 0.0f : float(frame - 5) * 30.0f;
  const float shake = frame < 6 ? (frame % 2 ? 8.0f : -8.0f) : 0.0f;
  motherChamber(k, ticks, frame < 6 ? 4 : 6, sink, ox + shake, oy);
  if (frame < 6 && frame % 2 == 0)
    k.r.fillRect(ox, oy, W, H, rgba(255, 240, 200, 60), Blend::Add);
  for (int i = 0; i < 5; ++i)
    if ((frame + i) % 3 == 0)
      drawGlow(k.r, k.art, 820.0f + float(hash2(i, frame + 91) % 300u) + ox, 380.0f + float(hash2(i, frame + 92) % 240u) + oy,
        110, rgb(255, 180, 90), 0.8f);
  for (int who = 0; who < 3; ++who)
    k.r.draw(runner(k, who, frame >= 10 && who == 1 ? 3 : 0, 1.6f), 110.0f + float(who) * 90.0f + ox, 640 + oy);
}

// 2: the Star Seed. Nova holds it up; it lights the chamber gold (an
// 8-frame loop).
void starSeedHeld(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  motherChamber(k, ticks, 6, 400.0f, ox, oy);
  const float x = 640 + ox, y = 380 + oy;
  const float pulse = 0.5f + 0.5f * std::sin(float(ticks) * 0.12f);
  drawGlow(k.r, k.art, x, y, 380.0f + 60.0f * pulse, rgb(255, 230, 140), 0.55f);
  for (int i = 0; i < 8; ++i)
  {
    const float a = float(i) * 0.785f + float(ticks) * 0.01f;
    const float len = (i % 2 ? 70.0f : 130.0f) * (0.9f + 0.1f * pulse);
    k.r.drawLine(x, y, x + std::cos(a) * len, y + std::sin(a) * len, i % 2 ? 4.0f : 7.0f, rgb(255, 246, 200), Blend::Add);
  }
  drawGlow(k.r, k.art, x, y, 60, rgb(255, 255, 255), 1.0f);
  k.r.draw(runner(k, 2, 3, 2.2f), 640 + ox, 640 + oy);
  k.r.draw(runner(k, 0, 3, 2.0f, true), 420 + ox, 640 + oy);
  k.r.draw(runner(k, 1, 3, 2.0f), 860 + ox, 640 + oy);
  (void)frame;
}

// 3: home. Vurr at dusk, the hive's dome on the horizon under the two
// moons; the courier ship lifts off (frames 0-5), turns its nose up and
// climbs away, faster and faster (6-15).
void vurrTakeoff(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  const Texture& sky = cached(k, "e7_vurr_dusk", 1280, 720, 0, 0, [](cairo_t* cr) {
    gradient(cr, 1280, 720, rgb(30, 14, 60), rgb(140, 60, 130), rgb(250, 150, 110));
    for (int i = 0; i < 70; ++i)
    {
      cairo_arc(cr, double(hash2(i, 701) % 1280u), double(hash2(i, 702) % 300u), 1.0 + double(hash2(i, 703) % 2u), 0,
        2 * kPi);
      setColor(cr, rgba(255, 255, 255, 120 + int(hash2(i, 704) % 120u)));
      cairo_fill(cr);
    }
    // The two moons.
    cairo_arc(cr, 980, 150, 70, 0, 2 * kPi);
    fillOutline(cr, rgb(230, 220, 255), rgb(120, 100, 170), 3);
    cairo_arc(cr, 1120, 250, 30, 0, 2 * kPi);
    fillOutline(cr, rgb(190, 240, 220), rgb(90, 140, 130), 3);
    // The hive's dome on the horizon, glowing at its vents.
    cairo_move_to(cr, 120, 600);
    cairo_curve_to(cr, 180, 330, 560, 330, 620, 600);
    cairo_close_path(cr);
    fillGradientOutline(cr, 380, 600, rgb(170, 80, 110), rgb(80, 30, 60), rgb(40, 10, 30), 4);
    for (int i = 0; i < 5; ++i)
    {
      cairo_arc(cr, 220 + i * 80, 470 + (i % 2) * 40, 10, 0, 2 * kPi);
      setColor(cr, rgb(255, 190, 110));
      cairo_fill(cr);
    }
    // The violet plain in front.
    cairo_move_to(cr, 0, 600);
    cairo_curve_to(cr, 400, 570, 900, 620, 1280, 590);
    cairo_line_to(cr, 1280, 720);
    cairo_line_to(cr, 0, 720);
    cairo_close_path(cr);
    fillGradientOutline(cr, 570, 720, rgb(150, 100, 170), rgb(60, 30, 70), rgb(30, 16, 40), 3);
  });
  k.r.draw(sky, ox, oy);
  static const std::string kShip = "veh_spaceship";
  float x = 760, y = 590, angle = 0.0f, scale = 1.4f;
  if (frame < 6)
    y = 590 - float(frame * frame) * 4.0f;
  else
  {
    const float t = float(frame - 6);
    angle = -std::min(40.0f, t * 10.0f);
    x = 760 + t * t * 9.0f;
    y = 446 - t * t * 7.0f;
    scale = std::max(0.3f, 1.4f - t * 0.11f);
  }
  DrawOpts o;
  o.scale = scale;
  o.angle = angle;
  // Thrust: dust under it at lift-off, a flame behind as it climbs.
  if (frame < 6)
    drawGlow(k.r, k.art, 760 + ox, 600 + oy, 260, rgb(255, 200, 150), 0.5f);
  drawGlow(k.r, k.art, x - 150.0f * scale + ox, y - 40.0f * scale + oy, 120.0f * scale + 40.0f, rgb(140, 220, 255),
    0.9f);
  k.r.draw(styledEnemySprite(k.art, k.r, k.theme, kShip, 0, (ticks / 6) % 2, 8, 4).get(1), x + ox, y + oy, o);
}

// Level 13: the temple corridor at sunset, light through the cracks, dust
// coming down from the ceiling (an 8-frame loop).
void templeCorridor(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  const Texture& bg = cached(k, "temple_corridor", 1280, 720, 0, 0, [](cairo_t* cr) {
    gradient(cr, 1280, 720, rgb(70, 30, 26), rgb(130, 60, 40), rgb(60, 30, 22));
    // Sandstone blocks.
    for (int row = 0; row < 12; ++row)
      for (int col = 0; col < 14; ++col)
      {
        const double x = double(col) * 100.0 - (row % 2 ? 50.0 : 0.0), y = double(row) * 60.0;
        cairo_rectangle(cr, x + 2, y + 2, 96, 56);
        setColor(cr, rgba(150 + int(hash2(row, col) % 30u), 90, 60, 60));
        cairo_fill(cr);
      }
    // Light through the cracks: slanted shafts of sunset.
    for (int i = 0; i < 5; ++i)
    {
      const double x = 120.0 + double(i) * 260.0 + double(hash2(i, 7) % 60u);
      cairo_move_to(cr, x, 0);
      cairo_line_to(cr, x + 24, 0);
      cairo_line_to(cr, x + 220, 560);
      cairo_line_to(cr, x + 150, 560);
      cairo_close_path(cr);
      setColor(cr, rgba(255, 190, 110, 46));
      cairo_fill(cr);
    }
    // The floor and the pillars.
    cairo_rectangle(cr, 0, 560, 1280, 160);
    setColor(cr, rgb(110, 70, 44));
    cairo_fill(cr);
    cairo_rectangle(cr, 0, 560, 1280, 10);
    setColor(cr, rgb(200, 150, 100));
    cairo_fill(cr);
    for (int i = 0; i < 4; ++i)
    {
      cairo_rectangle(cr, 60.0 + double(i) * 360.0, 0, 70, 560);
      setColor(cr, rgb(80, 44, 30));
      cairo_fill(cr);
    }
  });
  k.r.draw(bg, ox, oy);
  // Dust trickling down, an 8-frame loop.
  for (int i = 0; i < 40; ++i)
  {
    const float x = float(hash2(i, 21) % 1280u);
    const float y = std::fmod(float(hash2(i, 22) % 560u) + float(frame % 8) * 70.0f, 560.0f);
    k.r.fillRect(x + ox, y + oy, 3, 10, rgba(230, 200, 150, 150));
  }
  (void)ticks;
}

void templeRumble(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  templeCorridor(k, frame, ticks, ox, oy);
  // The runners braced on the floor; MAX from Dash's wrist.
  for (int who = 0; who < 3; ++who)
  {
    const float jitter = float((ticks / 2 + who) % 3 - 1) * 3.0f;
    k.r.draw(runner(k, who, who == 1 ? 3 : 0, 2.6f, who == 2), 380.0f + float(who) * 260.0f + ox + jitter, 560 + oy);
  }
  drawMax(k, 250 + ox, 300 + oy, 0.4f, ticks, (ticks / 4) % 6 == 0 ? 0.3f : 0.0f);
}

void templeGo(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  templeCorridor(k, frame, ticks, ox, oy);
  // A round shadow grows over the left of the frame.
  const float grow = std::min(1.0f, float(frame) / 11.0f);
  const float r = 300.0f + 260.0f * grow, cx = -200.0f + 300.0f * grow;
  for (int i = 0; i < 24; ++i)
  {
    const float rr = r * (1.0f - float(i) * 0.03f);
    k.r.fillRect(cx - rr + ox, 360 - rr + oy, rr * 2.0f, rr * 2.0f, rgba(20, 8, 6, 20));
  }
  // The runners turn and sprint off to the right (frames 0-7).
  for (int who = 0; who < 3; ++who)
  {
    const int f = std::max(0, frame - who);
    const float x = 380.0f + float(who) * 260.0f + float(f * f) * 22.0f;
    if (x < 1400.0f)
      k.r.draw(runner(k, who, f > 0 ? 1 : 0, 2.6f), x + ox, 560 + oy);
  }
}

// Level 14: the gold chamber door with jade inlays, incense haze drifting
// across it on a 16-frame loop. `open` (0..1) swings its two wings in.
void goldDoor(ClipKit& k, int frame, float open, float ox, float oy)
{
  const Texture& bg = cached(k, "gold_door_hall", 1280, 720, 0, 0, [](cairo_t* cr) {
    gradient(cr, 1280, 720, rgb(40, 24, 10), rgb(90, 60, 24), rgb(40, 26, 12));
    for (int row = 0; row < 12; ++row)
      for (int col = 0; col < 14; ++col)
      {
        const double x = double(col) * 100.0 - (row % 2 ? 50.0 : 0.0), y = double(row) * 60.0;
        cairo_rectangle(cr, x + 2, y + 2, 96, 56);
        setColor(cr, rgba(170 + int(hash2(row, col) % 30u), 120, 50, 50));
        cairo_fill(cr);
      }
    cairo_rectangle(cr, 0, 560, 1280, 160);
    setColor(cr, rgb(100, 70, 30));
    cairo_fill(cr);
    cairo_rectangle(cr, 0, 560, 1280, 10);
    setColor(cr, rgb(230, 190, 110));
    cairo_fill(cr);
  });
  k.r.draw(bg, ox, oy);
  // The doorway and its light, then the wings.
  const float dx = 440.0f + ox, dy = 120.0f + oy, dw = 400.0f, dh = 440.0f;
  k.r.fillRect(dx - 30, dy - 30, dw + 60, dh + 30, rgb(120, 84, 30));
  k.r.fillRect(dx, dy, dw, dh, rgb(255, 230, 150));
  if (open > 0.0f)
    for (int i = 0; i < 6; ++i)
      k.r.fillRect(dx + dw * 0.5f - 60.0f * open * float(i + 1), dy, 120.0f * open * float(i + 1), dh + 160,
        rgba(255, 220, 120, 26), Blend::Add);
  const float wing = dw * 0.5f * (1.0f - open * 0.85f);
  for (int side = 0; side < 2; ++side)
  {
    const float x = side ? dx + dw - wing : dx;
    k.r.fillRect(x, dy, wing, dh, rgb(230, 180, 70));
    k.r.fillRect(x + wing * 0.12f, dy + 40, wing * 0.76f, dh - 80, rgb(210, 160, 56));
    // Jade inlay: a diamond and a band.
    k.r.fillRect(x + wing * 0.3f, dy + dh * 0.45f, wing * 0.4f, 40, rgb(70, 200, 140));
    k.r.fillRect(x + wing * 0.12f, dy + 60, wing * 0.76f, 12, rgb(90, 220, 160));
    k.r.fillRect(x + wing * 0.12f, dy + dh - 72, wing * 0.76f, 12, rgb(90, 220, 160));
  }
  // Incense haze, a 16-frame loop.
  for (int i = 0; i < 14; ++i)
  {
    const float x = std::fmod(float(hash2(i, 31) % 1280u) + float(frame % 16) * 12.0f, 1400.0f) - 60.0f;
    const float y = 200.0f + float(hash2(i, 32) % 300u);
    k.r.fillRect(x + ox, y + oy, 120, 26, rgba(240, 230, 210, 34));
  }
}

// Level 15: Station Zero's hangar mouth, a frosted hull frame with the
// docking clamps inside it.
void hangarMouth(ClipKit& k, float ox, float oy)
{
  const Texture& mouth = cached(k, "e3_hangar_mouth", 1280, 720, 0, 0, [](cairo_t* cr) {
    // Hull above and below the mouth, in plates.
    for (int band = 0; band < 2; ++band)
    {
      const double y0 = band == 0 ? 0.0 : 560.0, h = band == 0 ? 150.0 : 160.0;
      cairo_rectangle(cr, 0, y0, 1280, h);
      setColor(cr, rgb(70, 78, 96));
      cairo_fill(cr);
      for (int col = 0; col < 14; ++col)
        for (int row = 0; row < 3; ++row)
        {
          cairo_rectangle(cr, col * 96.0 + 3, y0 + row * h / 3 + 3, 90, h / 3 - 6);
          setColor(cr, rgb(84 + int(hash2(col, row + band * 9) % 16u), 92, 112));
          cairo_fill(cr);
        }
    }
    // The left wall of the mouth.
    cairo_rectangle(cr, 0, 150, 120, 410);
    setColor(cr, rgb(60, 66, 82));
    cairo_fill(cr);
    // Inside: the dim, warm-lit bay.
    cairo_rectangle(cr, 120, 150, 1160, 410);
    setColor(cr, rgba(255, 190, 120, 26));
    cairo_fill(cr);
    // Hazard stripes along the lips.
    for (const double y : {140.0, 560.0})
      for (int s = 0; s < 64; ++s)
      {
        cairo_move_to(cr, s * 20.0, y + 10);
        cairo_line_to(cr, s * 20.0 + 10, y + 10);
        cairo_line_to(cr, s * 20.0 + 20, y);
        cairo_line_to(cr, s * 20.0 + 10, y);
        cairo_close_path(cr);
        setColor(cr, s % 2 ? rgb(20, 20, 26) : rgb(255, 196, 30));
        cairo_fill(cr);
      }
    // Rails along the floor lip, frosted.
    for (const double y : {548.0, 530.0})
    {
      cairo_rectangle(cr, 120, y, 1160, 6);
      setColor(cr, rgb(150, 160, 180));
      cairo_fill(cr);
      cairo_rectangle(cr, 120, y - 3, 1160, 3);
      setColor(cr, rgba(235, 248, 255, 200));
      cairo_fill(cr);
    }
    // Frost creeping in from the corners.
    for (int i = 0; i < 40; ++i)
    {
      const double x = double(hash2(i, 51) % 1280u), y = i % 2 ? 150.0 + double(hash2(i, 52) % 30u) : 530.0 - double(hash2(i, 53) % 30u);
      cairo_arc(cr, x, y, 4.0 + double(hash2(i, 54) % 8u), 0, 6.283);
      setColor(cr, rgba(230, 245, 255, 90));
      cairo_fill(cr);
    }
  });
  k.r.draw(mouth, ox, oy);
}

// Level 15's briefing: Nova's ship glides in from the right and settles
// into the docking clamps; ice crystals drift past.
void stationDock(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  drawBackdrop(k.r, k.art, float(ticks) + ox, oy, 0.0f);
  hangarMouth(k, ox, oy);
  static const std::string kShip = "veh_spaceship";
  const float t = std::min(1.0f, float(frame) / 44.0f), e = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
  const float x = 1560.0f - (1560.0f - 700.0f) * e, y = 470.0f;
  DrawOpts big;
  big.scale = 1.6f;
  k.r.draw(styledEnemySprite(k.art, k.r, k.theme, kShip, 0, (ticks / 6) % 2, 8, 4).get(1), x + ox, y + oy, big);
  if (t < 1.0f)
    drawGlow(k.r, k.art, x + 230 + ox, y - 70 + oy, 70.0f * (1.0f - e) + 20.0f, rgb(140, 220, 255), 0.8f);
  // The clamps: yellow arms that swing shut on frame 44.
  const float shut = frame >= 44 ? 1.0f : 0.0f;
  for (const float cx : {560.0f, 840.0f})
  {
    k.r.fillRect(cx - 14 + ox, 150 + oy, 28, 70 + 70 * shut, rgb(255, 196, 30));
    k.r.fillRect(cx - 24 + ox, 210 + 70 * shut + oy, 48, 14, rgb(60, 64, 76));
  }
  // Ice crystals, an 8-frame loop.
  for (int i = 0; i < 26; ++i)
  {
    const float px = std::fmod(float(hash2(i, 61) % 1280u) - float(frame % 8) * 6.0f + 1280.0f, 1280.0f);
    const float py = float(hash2(i, 62) % 720u) + float(frame % 8) * 2.0f;
    const float s = 3.0f + float(hash2(i, 63) % 5u);
    k.r.fillRect(px - s * 0.5f + ox, py - s * 0.5f + oy, s, s, rgba(230, 245, 255, 200));
  }
}

// Level 15's briefing: the frosted airlock, the crew in the hatch, MAX's
// hologram flickering out, and the wall speaker that talks.
void stationIntercom(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  const Texture& lock = cached(k, "e3_airlock", 1280, 720, 0, 0, [](cairo_t* cr) {
    gradient(cr, 1280, 720, rgb(40, 46, 60), rgb(58, 66, 84), rgb(30, 34, 44));
    for (int col = 0; col < 10; ++col)
      for (int row = 0; row < 6; ++row)
      {
        cairo_rectangle(cr, col * 128.0 + 4, row * 120.0 + 4, 120, 112);
        setColor(cr, rgba(140, 150, 170, 30 + int(hash2(col, row + 70) % 20u)));
        cairo_fill(cr);
      }
    // The hatch: a thick round frame, hazard-banded, open on a lit tube.
    cairo_arc(cr, 640, 380, 270, 0, 6.283);
    setColor(cr, rgb(90, 98, 116));
    cairo_fill(cr);
    cairo_arc(cr, 640, 380, 215, 0, 6.283);
    setColor(cr, rgb(200, 210, 224));
    cairo_fill(cr);
    cairo_arc(cr, 640, 380, 200, 0, 6.283);
    setColor(cr, rgb(150, 170, 196));
    cairo_fill(cr);
    for (int s = 0; s < 24; ++s)
    {
      const double a0 = s * 6.283 / 24.0, a1 = a0 + 6.283 / 48.0;
      cairo_arc(cr, 640, 380, 245, a0, a1);
      cairo_arc_negative(cr, 640, 380, 225, a1, a0);
      cairo_close_path(cr);
      setColor(cr, rgb(255, 196, 30));
      cairo_fill(cr);
    }
    // Frost round the edges of the room.
    for (int i = 0; i < 90; ++i)
    {
      const double edge = double(hash2(i, 71) % 4u);
      const double r = 6.0 + double(hash2(i, 72) % 18u);
      double x = double(hash2(i, 73) % 1280u), y = double(hash2(i, 74) % 720u);
      if (edge == 0)
        y = r * 0.5;
      else if (edge == 1)
        y = 720 - r * 0.5;
      else if (edge == 2)
        x = r * 0.5;
      else
        x = 1280 - r * 0.5;
      cairo_arc(cr, x, y, r, 0, 6.283);
      setColor(cr, rgba(235, 248, 255, 120));
      cairo_fill(cr);
    }
    // The wall speaker.
    cairo_rectangle(cr, 1040, 250, 120, 150);
    setColor(cr, rgb(30, 32, 40));
    cairo_fill(cr);
    for (int g = 0; g < 9; ++g)
    {
      cairo_rectangle(cr, 1052, 262 + g * 14, 96, 6);
      setColor(cr, rgb(70, 74, 88));
      cairo_fill(cr);
    }
  });
  k.r.draw(lock, ox, oy);
  // The crew in the hatch.
  k.r.draw(runner(k, 1, 0, 2.2f), 520 + ox, 560 + oy);
  k.r.draw(runner(k, 0, 0, 2.2f), 640 + ox, 570 + oy);
  k.r.draw(runner(k, 2, 0, 2.2f, true), 760 + ox, 560 + oy);
  // MAX on the left, the signal breaking up.
  if (frame % 3 != 2)
    drawMax(k, 200 + ox, 360 + oy, 0.55f, ticks, 0.7f);
  // The speaker's LED pulses with the voice.
  static const float kPulse[6] = {1.0f, 0.55f, 0.9f, 0.3f, 0.8f, 0.45f};
  const float a = kPulse[frame % 6];
  k.r.fillRect(1094 + ox, 412 + oy, 12, 12, rgb(255, 40, 40));
  drawGlow(k.r, k.art, 1100 + ox, 418 + oy, 20.0f + 20.0f * a, rgb(255, 60, 40), a);
}

// Level 16: a white lab corridor running away to a vanishing point, strip
// lights overhead.
void cryoCorridor(ClipKit& k, float ox, float oy)
{
  const Texture& hall = cached(k, "e3_cryo_corridor", 1280, 720, 0, 0, [](cairo_t* cr) {
    gradient(cr, 1280, 720, rgb(214, 230, 242), rgb(236, 244, 250), rgb(170, 190, 210));
    const double vx = 640, vy = 330;
    // Floor, walls and ceiling as four trapezoids to the far door.
    const double fx0 = 520, fx1 = 760, fy0 = 250, fy1 = 410;
    const struct
    {
      double ax, ay, bx, by, cx, cy, dx, dy;
      Color c;
    } faces[] = {
      {0, 720, 1280, 720, fx1, fy1, fx0, fy1, rgb(176, 196, 214)},
      {0, 0, 1280, 0, fx1, fy0, fx0, fy0, rgb(226, 236, 246)},
      {0, 0, fx0, fy0, fx0, fy1, 0, 720, rgb(206, 220, 234)},
      {1280, 0, fx1, fy0, fx1, fy1, 1280, 720, rgb(198, 214, 230)},
    };
    for (const auto& f : faces)
    {
      cairo_move_to(cr, f.ax, f.ay);
      cairo_line_to(cr, f.bx, f.by);
      cairo_line_to(cr, f.cx, f.cy);
      cairo_line_to(cr, f.dx, f.dy);
      cairo_close_path(cr);
      setColor(cr, f.c);
      cairo_fill(cr);
    }
    // Panel seams running to the vanishing point, and the strip lights.
    for (int i = 0; i < 7; ++i)
    {
      const double t = i / 7.0, x = 1280 * t;
      cairo_move_to(cr, x, 720);
      cairo_line_to(cr, vx + (x - vx) * 0.19, vy + (720 - vy) * 0.2);
      setColor(cr, rgba(120, 140, 170, 90));
      cairo_set_line_width(cr, 2);
      cairo_stroke(cr);
    }
    for (int i = 0; i < 5; ++i)
    {
      const double d = 1.0 - i * 0.18, w = 220 * d, y = vy - (vy - 20) * d;
      cairo_rectangle(cr, vx - w * 0.5, y, w, 10 * d + 2);
      setColor(cr, rgb(252, 255, 255));
      cairo_fill(cr);
    }
    // The far door, with a frosted window.
    cairo_rectangle(cr, fx0 + 70, fy0 + 20, 100, fy1 - fy0 - 20);
    setColor(cr, rgb(150, 166, 186));
    cairo_fill(cr);
    cairo_rectangle(cr, fx0 + 90, fy0 + 40, 60, 40);
    setColor(cr, rgb(214, 238, 255));
    cairo_fill(cr);
  });
  k.r.draw(hall, ox, oy);
}

// Frost growing in from the frame's edges: `amount` 0 (none) to 1.
void frostMask(ClipKit& k, float amount, float ox, float oy)
{
  if (amount <= 0.0f)
    return;
  for (int i = 0; i < 160; ++i)
  {
    const int edge = int(hash2(i, 81) % 4u);
    const float depth = (40.0f + float(hash2(i, 82) % 140u)) * amount;
    const float r = 10.0f + float(hash2(i, 83) % 26u) * amount;
    float x = float(hash2(i, 84) % 1280u), y = float(hash2(i, 85) % 720u);
    if (edge == 0)
      y = depth * 0.5f;
    else if (edge == 1)
      y = 720.0f - depth * 0.5f;
    else if (edge == 2)
      x = depth * 0.5f;
    else
      x = 1280.0f - depth * 0.5f;
    k.r.fillRect(x - r + ox, y - r + oy, r * 2.0f, r * 2.0f, rgba(240, 250, 255, int(110 + 80 * amount)));
  }
  const float band = 26.0f * amount;
  k.r.fillRect(ox, oy, 1280, band, rgba(250, 254, 255, 220));
  k.r.fillRect(ox, 720 - band + oy, 1280, band, rgba(250, 254, 255, 220));
  k.r.fillRect(ox, oy, band, 720, rgba(250, 254, 255, 220));
  k.r.fillRect(1280 - band + ox, oy, band, 720, rgba(250, 254, 255, 220));
}

// Level 16's briefing: the three runners walk up the corridor toward us
// while frost creeps in from the edges.
void cryoFrost(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  const float shift = float(frame % 40) * 2.0f;
  cryoCorridor(k, ox - shift * 0.25f, oy);
  const float t = std::min(1.0f, float(frame) / 39.0f);
  const float scale = 1.6f + 0.8f * t;
  const int step = 10 + (frame / 1) % 4 * 2; // a 4-frame walk loop
  const float spread = 150.0f + 80.0f * t, base = 520.0f + 60.0f * t;
  k.r.draw(runner(k, 1, step, scale), 640 - spread + ox, base + oy);
  k.r.draw(runner(k, 0, (step + 2) % 18 < 10 ? 10 : (step + 2) % 18, scale), 640 + ox, base + 10 + oy);
  k.r.draw(runner(k, 2, step, scale, true), 640 + spread + ox, base + oy);
  frostMask(k, t, ox, oy);
  (void)ticks;
}

// Then they stop: Dash shrugs, Nova rubs her hands, Rocco looks at the
// speaker and away.
void cryoShrug(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  cryoCorridor(k, ox, oy);
  const float scale = 2.4f;
  k.r.draw(runner(k, 0, frame >= 2 && frame <= 5 ? 3 : 0, scale), 640 + ox, 610 + oy);
  k.r.draw(runner(k, 2, frame % 2 ? 9 : 8, scale, true), 870 + ox, 600 + oy);
  k.r.draw(runner(k, 1, 0, scale, frame >= 3 && frame <= 5), 410 + ox, 600 + oy);
  // The wall speaker high on the right.
  k.r.fillRect(1120 + ox, 120 + oy, 90, 110, rgb(40, 44, 54));
  for (int g = 0; g < 6; ++g)
    k.r.fillRect(1130 + ox, 132 + float(g) * 15 + oy, 70, 6, rgb(86, 92, 108));
  drawGlow(k.r, k.art, 1165 + ox, 244 + oy, 18.0f, rgb(255, 60, 40), frame % 2 ? 0.9f : 0.4f);
  frostMask(k, 1.0f, ox, oy);
  (void)ticks;
}

// Level 17: the greenhouse dome, glass and white struts against the stars,
// the planet's curve beyond and the racks of greens below.
void greenhouseDome(ClipKit& k, float ox, float oy)
{
  const Texture& dome = cached(k, "e3_green_dome", 1280, 720, 0, 0, [](cairo_t* cr) {
    gradient(cr, 1280, 720, rgb(6, 10, 30), rgb(34, 22, 64), rgb(92, 40, 96));
    for (int i = 0; i < 220; ++i)
    {
      const double s = double(hash2(i, 171) % 100u) / 100.0;
      cairo_arc(cr, double(hash2(i, 172) % 1280u), double(hash2(i, 173) % 520u), 0.6 + s * 1.3, 0, 2 * kPi);
      setColor(cr, rgba(230, 240, 255, 70 + int(s * 170)));
      cairo_fill(cr);
    }
    // The planet, low on the left this time.
    radialGlow(cr, 220, 760, 520, rgb(120, 220, 255), 0.35);
    cairo_arc(cr, 220, 760, 420, 0, 2 * kPi);
    cairo_pattern_t* g = cairo_pattern_create_linear(-200, 340, 520, 760);
    cairo_pattern_add_color_stop_rgb(g, 0, 0.55, 0.85, 0.75);
    cairo_pattern_add_color_stop_rgb(g, 0.5, 0.16, 0.42, 0.46);
    cairo_pattern_add_color_stop_rgb(g, 1, 0.04, 0.08, 0.16);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
    // Geodesic struts: rings and diagonals round a centre below the frame.
    const double cx = 640, cy = 1150;
    for (int pass = 0; pass < 2; ++pass)
    {
      const double width = pass == 0 ? 12.0 : 4.0;
      const Color col = pass == 0 ? rgba(20, 24, 40, 120) : rgba(240, 248, 255, 210);
      for (int ring = 0; ring < 5; ++ring)
      {
        const double r0 = 520 + ring * 170, r1 = r0 + 170;
        const double off = (ring % 2) * kPi / 24.0;
        for (int s = 0; s <= 12; ++s)
        {
          const double a = kPi + kPi * s / 12.0 + off;
          if (s == 0)
            cairo_move_to(cr, cx + std::cos(a) * r0, cy + std::sin(a) * r0);
          else
            cairo_line_to(cr, cx + std::cos(a) * r0, cy + std::sin(a) * r0);
        }
        for (int s = 0; s <= 12; ++s)
        {
          const double a = kPi + kPi * s / 12.0 + off;
          cairo_move_to(cr, cx + std::cos(a) * r0, cy + std::sin(a) * r0);
          cairo_line_to(cr, cx + std::cos(a + kPi / 24.0) * r1, cy + std::sin(a + kPi / 24.0) * r1);
          cairo_move_to(cr, cx + std::cos(a) * r0, cy + std::sin(a) * r0);
          cairo_line_to(cr, cx + std::cos(a - kPi / 24.0) * r1, cy + std::sin(a - kPi / 24.0) * r1);
        }
        setColor(cr, col);
        cairo_set_line_width(cr, width);
        cairo_stroke(cr);
      }
    }
    // Three long rows of planters with greens, stepping down toward us.
    for (int row = 0; row < 3; ++row)
    {
      const double y = 470 + row * 80, s = 0.7 + row * 0.25;
      for (double x = -20 + row * 17; x < 1300; x += 26 * s)
      {
        const double hgt = (30 + double(hash2(int(x), row) % 30u)) * s;
        for (int l = -1; l <= 1; ++l)
        {
          cairo_save(cr);
          cairo_translate(cr, x, y);
          cairo_rotate(cr, -kPi * 0.5 + l * 0.5 + double(hash2(int(x), row + l) % 20u) / 100.0);
          cairo_move_to(cr, 0, 0);
          cairo_curve_to(cr, hgt * 0.3, -8 * s, hgt * 0.75, -6 * s, hgt, 0);
          cairo_curve_to(cr, hgt * 0.75, 6 * s, hgt * 0.3, 8 * s, 0, 0);
          cairo_restore(cr);
          setColor(cr, lerpColor(rgb(40, 100, 50), rgb(120, 200, 90), float(row) / 2.0f));
          cairo_fill(cr);
        }
      }
      cairo_rectangle(cr, 0, y - 4, 1280, 24 * s);
      setColor(cr, lerpColor(rgb(40, 34, 40), rgb(70, 60, 56), float(row) / 2.0f));
      cairo_fill(cr);
      cairo_rectangle(cr, 0, y - 4, 1280, 4);
      setColor(cr, rgba(230, 240, 236, 200));
      cairo_fill(cr);
    }
    cairo_rectangle(cr, 0, 690, 1280, 30);
    setColor(cr, rgb(30, 24, 22));
    cairo_fill(cr);
  });
  k.r.draw(dome, ox, oy);
}

// A hanging grow lamp at (x, y) (the middle of its hood's top), lit or not.
void growLamp(ClipKit& k, float x, float y, bool lit, float ox, float oy)
{
  k.r.fillRect(x - 2 + ox, oy - 10, 4, y + 10, rgb(40, 44, 54));
  if (lit)
  {
    const Texture& cone = cached(k, "e3_green_cone", 420, 560, 210, 0, [](cairo_t* cr) {
      cairo_move_to(cr, 170, 0);
      cairo_line_to(cr, 250, 0);
      cairo_line_to(cr, 420, 560);
      cairo_line_to(cr, 0, 560);
      cairo_close_path(cr);
      cairo_pattern_t* g = cairo_pattern_create_linear(0, 0, 0, 560);
      cairo_pattern_add_color_stop_rgba(g, 0, 1.0, 0.45, 0.85, 0.55);
      cairo_pattern_add_color_stop_rgba(g, 1, 1.0, 0.3, 0.8, 0.0);
      cairo_set_source(cr, g);
      cairo_fill(cr);
      cairo_pattern_destroy(g);
    });
    DrawOpts o;
    o.blend = Blend::Add;
    o.alpha = 0.8f;
    k.r.draw(cone, x + ox, y + 40 + oy, o);
  }
  const Texture& hood = cached(k, lit ? "e3_green_lamp1" : "e3_green_lamp0", 120, 64, 60, 0, [lit](cairo_t* cr) {
    cairo_move_to(cr, 40, 4);
    cairo_line_to(cr, 80, 4);
    cairo_line_to(cr, 114, 44);
    cairo_line_to(cr, 6, 44);
    cairo_close_path(cr);
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 4, 0, 44);
    cairo_pattern_add_color_stop_rgb(g, 0, 0.98, 0.99, 1.0);
    cairo_pattern_add_color_stop_rgb(g, 1, 0.66, 0.72, 0.78);
    cairo_set_source(cr, g);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(g);
    setColor(cr, kInk);
    cairo_set_line_width(cr, 2.5);
    cairo_stroke(cr);
    roundedRect(cr, 12, 44, 96, 12, 6);
    setColor(cr, lit ? rgb(255, 214, 248) : rgb(84, 70, 92));
    cairo_fill_preserve(cr);
    setColor(cr, kInk);
    cairo_set_line_width(cr, 2);
    cairo_stroke(cr);
  });
  k.r.draw(hood, x + ox, y + oy);
  if (lit)
    drawGlow(k.r, k.art, x + ox, y + 50 + oy, 90, rgb(255, 90, 210), 0.7f);
}

// Level 17's briefing: three grow lamps flicker on over the crew in turn,
// then hold their magenta light.
void greenLights(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  greenhouseDome(k, ox, oy);
  static const int kWho[3] = {1, 0, 2};
  for (int i = 0; i < 3; ++i)
  {
    const float x = 330.0f + float(i) * 310.0f;
    const int f = frame - i * 4;
    const bool lit = f >= 4 || (f >= 0 && f != 1 && f != 2 + (i % 2));
    growLamp(k, x, 90.0f, lit, ox, oy);
    k.r.draw(runner(k, kWho[i], 0, 2.2f, i == 2), x + ox, 680 + oy);
  }
  (void)ticks;
}

// Then close on Rocco: a vine curls up from the bottom-left toward his
// shoulder; on the last frame he leans back away from it.
void greenVine(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  const Texture& back = cached(k, "e3_green_close", 1280, 720, 0, 0, [](cairo_t* cr) {
    gradient(cr, 1280, 720, rgb(40, 20, 60), rgb(80, 36, 90), rgb(30, 40, 30));
    radialGlow(cr, 820, 0, 700, rgb(255, 90, 210), 0.45);
    // Racks of greens behind him, soft and out of focus.
    for (int row = 0; row < 3; ++row)
    {
      const double y = 250 + row * 170, s = 1.2 + row * 0.3;
      for (double x = -10 + row * 23; x < 1300; x += 30 * s)
        for (int l = -1; l <= 1; ++l)
        {
          const double hgt = (40 + double(hash2(int(x), row + 9) % 40u)) * s;
          cairo_save(cr);
          cairo_translate(cr, x, y);
          cairo_rotate(cr, -kPi * 0.5 + l * 0.55 + double(hash2(int(x), row + l + 5) % 20u) / 100.0);
          cairo_move_to(cr, 0, 0);
          cairo_curve_to(cr, hgt * 0.3, -9 * s, hgt * 0.75, -7 * s, hgt, 0);
          cairo_curve_to(cr, hgt * 0.75, 7 * s, hgt * 0.3, 9 * s, 0, 0);
          cairo_restore(cr);
          setColor(cr, rgba(70 + row * 20, 140 + row * 20, 80, 90 + row * 40));
          cairo_fill(cr);
        }
      cairo_rectangle(cr, 0, y, 1280, 22);
      setColor(cr, rgba(220, 230, 226, 90 + row * 30));
      cairo_fill(cr);
    }
    // Lamp light caught as soft discs.
    for (int i = 0; i < 26; ++i)
    {
      cairo_arc(cr, double(hash2(i, 181) % 1280u), double(hash2(i, 182) % 500u), 10 + double(hash2(i, 183) % 30u), 0,
        2 * kPi);
      setColor(cr, i % 3 ? rgba(255, 140, 230, 40) : rgba(255, 240, 200, 36));
      cairo_fill(cr);
    }
  });
  k.r.draw(back, ox, oy);
  const Texture& leaf = cached(k, "e3_green_leaf", 60, 30, 0, 15, [](cairo_t* cr) {
    cairo_move_to(cr, 1, 15);
    cairo_curve_to(cr, 18, 1, 44, 3, 58, 15);
    cairo_curve_to(cr, 44, 27, 18, 29, 1, 15);
    cairo_close_path(cr);
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 2, 0, 28);
    cairo_pattern_add_color_stop_rgb(g, 0, 0.62, 0.92, 0.4);
    cairo_pattern_add_color_stop_rgb(g, 1, 0.16, 0.45, 0.16);
    cairo_set_source(cr, g);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(g);
    setColor(cr, kInk);
    cairo_set_line_width(cr, 2);
    cairo_stroke(cr);
    cairo_move_to(cr, 4, 15);
    cairo_line_to(cr, 50, 15);
    setColor(cr, rgb(40, 100, 36));
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);
  });
  // Rocco, big, facing the vine; on frame 15 he leans back.
  const bool lean = frame >= 15;
  DrawOpts ro;
  ro.angle = lean ? 9.0f : 0.0f;
  k.r.draw(runner(k, 1, lean ? 5 : 0, 3.8f, true), 820 + ox, 720 + oy, ro);
  // The vine: a Bezier from the bottom-left corner toward his shoulder,
  // grown frame by frame, a curl at its tip.
  const float t = std::min(1.0f, float(frame + 1) / 16.0f);
  const float px[4] = {-40.0f, 180.0f, 430.0f, 700.0f};
  const float py[4] = {700.0f, 380.0f, 560.0f, 470.0f};
  auto at = [&](float u, float& x, float& y) {
    const float v = 1.0f - u;
    x = v * v * v * px[0] + 3 * v * v * u * px[1] + 3 * v * u * u * px[2] + u * u * u * px[3];
    y = v * v * v * py[0] + 3 * v * v * u * py[1] + 3 * v * u * u * py[2] + u * u * u * py[3];
  };
  const int steps = 40;
  for (int pass = 0; pass < 2; ++pass)
    for (int s = 0; s < steps; ++s)
    {
      const float u0 = t * float(s) / float(steps), u1 = t * float(s + 1) / float(steps);
      float x0, y0, x1, y1;
      at(u0, x0, y0);
      at(u1, x1, y1);
      const float w = 6.0f + 18.0f * (1.0f - float(s) / float(steps));
      k.r.drawLine(x0 + ox, y0 + oy, x1 + ox, y1 + oy, pass == 0 ? w + 6.0f : w,
        pass == 0 ? kInk : rgb(80, 150, 56));
    }
  // Leaves along it.
  for (int l = 1; l < 8; ++l)
  {
    const float u = float(l) / 8.0f;
    if (u > t - 0.04f)
      break;
    float x, y, x2, y2;
    at(u, x, y);
    at(u + 0.02f, x2, y2);
    const float ang = std::atan2(y2 - y, x2 - x) * 57.2958f + (l % 2 ? -60.0f : 60.0f);
    DrawOpts o;
    o.angle = ang;
    o.scale = 1.4f - 0.08f * float(l);
    k.r.draw(leaf, x + ox, y + oy, o);
  }
  // The curling tip.
  float tx, ty;
  at(t, tx, ty);
  for (int s = 0; s < 10; ++s)
  {
    const float a0 = float(s) * 0.55f + float(frame) * 0.1f, a1 = a0 + 0.55f;
    const float r0 = 22.0f - float(s) * 2.0f, r1 = r0 - 2.0f;
    k.r.drawLine(tx + std::cos(a0) * r0 - 18.0f + ox, ty + std::sin(a0) * r0 + oy, tx + std::cos(a1) * r1 - 18.0f + ox,
      ty + std::sin(a1) * r1 + oy, 5.0f, rgb(110, 190, 70));
  }
  (void)ticks;
}

void goldDoorScratch(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  goldDoor(k, frame, 0.0f, ox, oy);
  // The plaster under the gold: a white streak from frame 10.
  if (frame >= 6)
  {
    const float len = std::min(1.0f, float(frame - 6) / 4.0f) * 90.0f;
    if (frame >= 10)
      k.r.fillRect(560 + ox, 330 + oy, len, 14, rgb(245, 245, 240));
    else
      k.r.fillRect(560 + ox, 334 + oy, len, 6, rgb(255, 250, 220));
  }
  k.r.draw(runner(k, 0, 0, 2.6f), 300 + ox, 560 + oy);
  k.r.draw(runner(k, 1, 0, 2.6f, true), 1000 + ox, 560 + oy);
  // Nova at the door, a hand up to it; she frowns at what she finds.
  k.r.draw(runner(k, 2, frame >= 6 && frame <= 10 ? 3 : 0, 2.6f), 560 + ox, 560 + oy);
  if (frame >= 12)
    k.r.drawText("...PLASTER?", 560 + ox, 220 + oy, {30.0f, rgb(255, 255, 255), rgb(30, 20, 10)}, Align::Center);
  (void)ticks;
}

void goldDoorPull(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  goldDoor(k, frame, std::min(1.0f, float(frame) / 11.0f), ox, oy);
  // Dash takes Nova's arm and pulls her in; Rocco follows.
  const float t = float(std::min(frame, 11));
  k.r.draw(runner(k, 0, 1, 2.6f), 520.0f + t * 12.0f + ox, 560 + oy);
  k.r.draw(runner(k, 2, 1, 2.6f), 440.0f + t * 14.0f + ox, 560 + oy);
  k.r.draw(runner(k, 1, frame >= 4 ? 1 : 0, 2.6f), 180.0f + std::max(0.0f, t - 4.0f) * 22.0f + ox, 560 + oy);
  (void)ticks;
}

// Episode 2's end. The golem's gold in a heap on the sanctum floor, its
// head on top, dust motes drifting through the light from the roof.
void goldPile(ClipKit& k, int frame, int ticks, float ox, float oy, bool crew = true)
{
  const Texture& bg = cached(k, "e2_sanctum", 1280, 720, 0, 0, [](cairo_t* cr) {
    gradient(cr, 1280, 720, rgb(22, 14, 8), rgb(56, 36, 16), rgb(30, 20, 10));
    for (int row = 0; row < 10; ++row)
      for (int col = 0; col < 14; ++col)
      {
        const double x = double(col) * 100.0 - (row % 2 ? 50.0 : 0.0), y = double(row) * 60.0;
        cairo_rectangle(cr, x + 2, y + 2, 96, 56);
        setColor(cr, rgba(150 + int(hash2(row, col + 40) % 30u), 100, 40, 40));
        cairo_fill(cr);
      }
    cairo_rectangle(cr, 0, 600, 1280, 120);
    setColor(cr, rgb(70, 48, 20));
    cairo_fill(cr);
  });
  const Texture& heap = cached(k, "e2_heap", 900, 420, 450, 420, [](cairo_t* cr) {
    // Chunks of the golem, biggest at the bottom.
    for (int i = 0; i < 46; ++i)
    {
      const double t = double(i) / 46.0;
      const double w = 150.0 - t * 90.0, h = w * 0.55;
      const double x = 450.0 + (double(hash2(i, 7) % 1000u) / 1000.0 - 0.5) * (760.0 - t * 560.0) - w / 2;
      const double y = 420.0 - h - t * 300.0 + double(hash2(i, 8) % 30u);
      cairo_save(cr);
      cairo_translate(cr, x + w / 2, y + h / 2);
      cairo_rotate(cr, (double(hash2(i, 9) % 100u) / 100.0 - 0.5) * 0.8);
      cairo_rectangle(cr, -w / 2, -h / 2, w, h);
      setColor(cr, rgb(200 + int(hash2(i, 10) % 40u), 150 + int(hash2(i, 11) % 40u), 50));
      cairo_fill_preserve(cr);
      setColor(cr, rgb(120, 80, 20));
      cairo_set_line_width(cr, 4);
      cairo_stroke(cr);
      cairo_rectangle(cr, -w / 2 + 8, -h / 2 + 6, w - 16, 8);
      setColor(cr, rgba(255, 240, 180, 150));
      cairo_fill(cr);
      cairo_restore(cr);
    }
    // The head on top, one eye dark, the other a jade gem.
    cairo_rectangle(cr, 370, 20, 160, 120);
    setColor(cr, rgb(226, 176, 64));
    cairo_fill_preserve(cr);
    setColor(cr, rgb(120, 80, 20));
    cairo_set_line_width(cr, 5);
    cairo_stroke(cr);
    cairo_rectangle(cr, 395, 60, 36, 26);
    setColor(cr, rgb(40, 26, 10));
    cairo_fill(cr);
    cairo_rectangle(cr, 469, 60, 36, 26);
    setColor(cr, rgb(70, 210, 150));
    cairo_fill(cr);
    cairo_rectangle(cr, 410, 108, 80, 12);
    setColor(cr, rgb(90, 60, 16));
    cairo_fill(cr);
  });
  k.r.draw(bg, ox * 0.5f, oy * 0.5f);
  // The light through the broken roof.
  for (int i = 0; i < 5; ++i)
    k.r.fillRect(520.0f + float(i) * 30.0f + ox, 0, 60, 620, rgba(255, 220, 140, 18), Blend::Add);
  k.r.draw(heap, 640 + ox, 610 + oy);
  for (int i = 0; i < 40; ++i)
  {
    const float x = 470.0f + float(hash2(i, 50) % 340u) + std::sin(float(ticks + i * 37) * 0.02f) * 20.0f;
    const float y = std::fmod(float(hash2(i, 51) % 600u) + float(ticks) * (0.2f + float(i % 5) * 0.08f), 600.0f);
    k.r.fillRect(x + ox, 600.0f - y + oy, 3, 3, rgba(255, 240, 200, 160));
  }
  if (crew)
  {
    k.r.draw(runner(k, 2, 0, 2.6f), 300 + ox, 640 + oy);
    k.r.draw(runner(k, 0, 0, 2.6f, true), 990 + ox, 640 + oy);
    k.r.draw(runner(k, 1, 0, 2.6f, true), 1130 + ox, 640 + oy);
  }
  (void)frame;
}

// Nova snaps a chunk of the gold in two: white fiberglass inside. 6 frames.
void fiberglass(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  goldPile(k, 0, ticks, ox * 0.3f, oy * 0.3f, false);
  k.r.fillRect(0, 0, W, H, rgba(0, 0, 0, 130));
  k.r.draw(runner(k, 2, 0, 3.6f), 640 + ox, 720 + oy);
  // The chunk in her hands: bending (0-2), cracking (3), in two (4-5).
  const float gap = frame >= 4 ? 30.0f + float(frame - 4) * 16.0f : 0.0f;
  const float tilt = float(std::min(frame, 3)) * 4.0f;
  const float cw = 100.0f, ch = 70.0f, cy = 450.0f;
  for (int side = 0; side < 2; ++side)
  {
    const float x = 640.0f + (side ? gap * 0.5f : -gap * 0.5f - cw) + ox;
    const float y = cy + tilt + oy;
    k.r.fillRect(x, y, cw, ch, rgb(222, 172, 60));
    k.r.fillRect(x, y, cw, 10, rgb(255, 226, 140));
    k.r.fillRect(x, y + ch - 10, cw, 10, rgb(150, 104, 30));
    if (frame >= 4)
    {
      // The broken face: white glass fibres, frayed.
      const float fx = side ? x : x + cw - 16.0f;
      k.r.fillRect(fx, y, 16, ch, rgb(240, 240, 232));
      for (int i = 0; i < 6; ++i)
        k.r.fillRect(fx + (side ? -8.0f : 16.0f), y + 5.0f + float(i) * 11.0f, 8.0f + float(hash2(i, side) % 8u), 3,
          rgb(250, 250, 245));
    }
  }
  if (frame == 3)
    k.r.fillRect(637 + ox, cy + tilt + oy, 6, ch, rgb(255, 255, 255));
  if (frame >= 3)
    drawGlow(k.r, k.art, 640 + ox, cy + 35 + oy, 80, rgb(255, 250, 230), frame == 3 ? 0.8f : 0.25f);
}

// The boulder at the foot of the cliff at sunset; the wind in the grass.
void boulderCliff(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  const Texture& bg = cached(k, "e2_cliff", 1280, 720, 0, 0, [](cairo_t* cr) {
    gradient(cr, 1280, 720, rgb(60, 30, 90), rgb(240, 120, 70), rgb(255, 200, 120));
    cairo_arc(cr, 380, 470, 90, 0, 2 * kPi);
    setColor(cr, rgba(255, 230, 160, 220));
    cairo_fill(cr);
    // Far jungle hills.
    cairo_move_to(cr, 0, 520);
    for (int x = 0; x <= 1280; x += 40)
      cairo_line_to(cr, x, 500 - 30 * std::sin(x * 0.01) - 20 * std::sin(x * 0.027));
    cairo_line_to(cr, 1280, 720);
    cairo_line_to(cr, 0, 720);
    cairo_close_path(cr);
    setColor(cr, rgb(70, 40, 60));
    cairo_fill(cr);
    // The cliff on the right, the boulder's track down its face.
    cairo_move_to(cr, 860, 720);
    cairo_line_to(cr, 900, 160);
    cairo_line_to(cr, 1280, 120);
    cairo_line_to(cr, 1280, 720);
    cairo_close_path(cr);
    setColor(cr, rgb(50, 30, 40));
    cairo_fill(cr);
    cairo_move_to(cr, 900, 160);
    cairo_line_to(cr, 870, 600);
    setColor(cr, rgba(255, 180, 120, 90));
    cairo_set_line_width(cr, 10);
    cairo_stroke(cr);
    cairo_rectangle(cr, 0, 600, 1280, 120);
    setColor(cr, rgb(40, 50, 30));
    cairo_fill(cr);
  });
  k.r.draw(bg, ox * 0.5f, oy * 0.5f);
  // The boulder, sunset on its edge, and a crack down one side.
  const Texture& stone = cached(k, "e2_boulder", 300, 300, 150, 300, [](cairo_t* cr) {
    cairo_arc(cr, 150, 150, 146, 0, 2 * kPi);
    setColor(cr, rgb(84, 72, 72));
    cairo_fill(cr);
    // Sunset on its left edge, shadow on the right.
    cairo_arc(cr, 150, 150, 146, 0.6 * kPi, 1.4 * kPi);
    cairo_set_line_width(cr, 10);
    setColor(cr, rgb(250, 160, 100));
    cairo_stroke(cr);
    cairo_arc(cr, 190, 170, 110, -0.3 * kPi, 0.6 * kPi);
    cairo_set_line_width(cr, 30);
    setColor(cr, rgba(30, 20, 30, 120));
    cairo_stroke(cr);
    // A crack, and a few pits.
    cairo_move_to(cr, 170, 20);
    cairo_line_to(cr, 150, 90);
    cairo_line_to(cr, 175, 150);
    cairo_line_to(cr, 160, 200);
    cairo_set_line_width(cr, 5);
    setColor(cr, rgb(40, 32, 34));
    cairo_stroke(cr);
    for (int i = 0; i < 7; ++i)
    {
      cairo_arc(cr, 60 + double(hash2(i, 70) % 180u), 60 + double(hash2(i, 71) % 180u), 6 + double(i % 3) * 3, 0,
        2 * kPi);
      setColor(cr, rgba(50, 40, 40, 160));
      cairo_fill(cr);
    }
  });
  k.r.draw(stone, 760 + ox, 612 + oy);
  // Grass in the wind.
  for (int i = 0; i < 70; ++i)
  {
    const float x = float(hash2(i, 61) % 1280u);
    const float sway = std::sin(float(ticks) * 0.05f + float(i)) * 6.0f;
    k.r.fillRect(x + sway + ox, 590 + float(hash2(i, 62) % 20u) + oy, 3, 24, rgb(70, 90, 50));
  }
  for (int i = 0; i < 6; ++i)
  {
    const float x = std::fmod(float(hash2(i, 63) % 1280u) + float(ticks) * 6.0f, 1400.0f) - 100.0f;
    k.r.fillRect(x + ox, 300.0f + float(i) * 50.0f + oy, 90, 2, rgba(255, 230, 200, 60));
  }
  k.r.draw(runner(k, 1, 0, 2.6f), 330 + ox, 650 + oy);
  k.r.draw(runner(k, 0, 0, 2.6f), 200 + ox, 650 + oy);
  k.r.draw(runner(k, 2, 0, 2.6f), 460 + ox, 650 + oy);
  (void)frame;
}

void fallback(ClipKit& k, const std::string& clip, int ticks, float ox, float oy)
{
  drawBackdrop(k.r, k.art, float(ticks) * 1.5f + ox, oy, 0.0f);
  k.r.fillRect(0, 0, W, H, rgba(0, 0, 0, 90));
  (void)clip;
}

// Level 18: looking down the airlock chamber to the outer door. The opening
// is 480 x 400 at (400, 150).
constexpr float kLockX = 400.0f, kLockY = 150.0f, kLockW = 480.0f, kLockH = 400.0f;

// Through the outer door: open space, the planet's curve and the sun
// coming up over it.
void hullSunrise(ClipKit& k, float ox, float oy)
{
  const Texture& sky = cached(k, "e3_hull_sunrise", 1280, 720, 0, 0, [](cairo_t* cr) {
    gradient(cr, 1280, 720, rgb(2, 3, 12), rgb(10, 16, 44), rgb(40, 60, 110));
    for (int i = 0; i < 300; ++i)
    {
      const double s = double(hash2(i, 181) % 100u) / 100.0;
      cairo_arc(cr, double(hash2(i, 182) % 1280u), double(hash2(i, 183) % 460u), 0.5 + s * 1.4, 0, 2 * kPi);
      setColor(cr, rgba(230, 238, 255, 60 + int(s * 190)));
      cairo_fill(cr);
    }
    const double cx = 640, top = 430, pr = 1500, cy = top + pr, sunX = 700;
    const double sunY = cy - std::sqrt(pr * pr - (sunX - cx) * (sunX - cx));
    radialGlow(cr, sunX, sunY, 460, rgb(255, 140, 60), 0.45);
    cairo_arc(cr, cx, cy, pr, 0, 2 * kPi);
    cairo_pattern_t* g = cairo_pattern_create_linear(0, top, 0, 720);
    cairo_pattern_add_color_stop_rgb(g, 0, 0.1, 0.15, 0.3);
    cairo_pattern_add_color_stop_rgb(g, 1, 0.02, 0.03, 0.08);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
    // City lights on the night side, dawn creeping on near the sun.
    for (int i = 0; i < 120; ++i)
    {
      const double x = double(hash2(i, 184) % 1280u);
      const double y = cy - std::sqrt(pr * pr - (x - cx) * (x - cx)) + 20 + double(hash2(i, 185) % 200u);
      cairo_arc(cr, x, y, 0.8, 0, 2 * kPi);
      setColor(cr, rgba(255, 200, 120, 140));
      cairo_fill(cr);
    }
    // The limb: gold by the sun, blue away from it.
    for (int pass = 0; pass < 3; ++pass)
      for (int x = 0; x < 1280; x += 8)
      {
        const double d = std::min(1.0, std::abs(x + 4 - sunX) / 500.0);
        const Color c = lerpColor(rgb(255, 210, 120), rgb(90, 170, 255), float(d));
        const double y0 = cy - std::sqrt(pr * pr - (x - cx) * (x - cx));
        const double y1 = cy - std::sqrt(pr * pr - (x + 8 - cx) * (x + 8 - cx));
        cairo_move_to(cr, x, y0);
        cairo_line_to(cr, x + 8, y1);
        cairo_set_source_rgba(cr, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0,
          (pass == 0 ? 0.2 : (pass == 1 ? 0.5 : 1.0)) * (1.0 - 0.4 * d));
        cairo_set_line_width(cr, pass == 0 ? 24 : (pass == 1 ? 9 : 3));
        cairo_stroke(cr);
      }
    radialGlow(cr, sunX, sunY, 110, rgb(255, 250, 220), 0.95);
    cairo_save(cr);
    cairo_translate(cr, sunX, sunY);
    cairo_scale(cr, 1.0, 0.02);
    cairo_arc(cr, 0, 0, 520, 0, 2 * kPi);
    cairo_restore(cr);
    setColor(cr, rgba(255, 240, 210, 170));
    cairo_fill(cr);
  });
  k.r.draw(sky, ox, oy);
}

// The chamber round the opening, with the opening cut out: floor, ceiling
// and walls running to it, the hazard-banded outer frame, the door's pocket
// on the right, and the dark inner doorway framing the shot.
void hullChamber(ClipKit& k, float ox, float oy)
{
  const Texture& room = cached(k, "e3_hull_chamber", 1280, 720, 0, 0, [](cairo_t* cr) {
    const double x0 = kLockX, y0 = kLockY, x1 = kLockX + kLockW, y1 = kLockY + kLockH;
    const struct
    {
      double ax, ay, bx, by, cx, cy, dx, dy;
      Color c;
    } faces[] = {
      {0, 720, 1280, 720, x1, y1, x0, y1, rgb(52, 60, 78)},
      {0, 0, 1280, 0, x1, y0, x0, y0, rgb(40, 46, 62)},
      {0, 0, x0, y0, x0, y1, 0, 720, rgb(62, 70, 90)},
      {1280, 0, x1, y0, x1, y1, 1280, 720, rgb(56, 64, 84)},
    };
    for (const auto& f : faces)
    {
      cairo_move_to(cr, f.ax, f.ay);
      cairo_line_to(cr, f.bx, f.by);
      cairo_line_to(cr, f.cx, f.cy);
      cairo_line_to(cr, f.dx, f.dy);
      cairo_close_path(cr);
      setColor(cr, f.c);
      cairo_fill(cr);
    }
    // Ribs round the chamber, receding to the door.
    for (int i = 1; i < 4; ++i)
    {
      const double t = i / 4.0;
      const double lx = x0 * t, ty = y0 * t, rx = 1280 - (1280 - x1) * t, by = 720 - (720 - y1) * t;
      cairo_rectangle(cr, lx, ty, rx - lx, by - ty);
      setColor(cr, rgba(150, 166, 196, 70));
      cairo_set_line_width(cr, 10 * (1.0 - t * 0.6));
      cairo_stroke(cr);
      // Strip lights on the ceiling ribs.
      cairo_rectangle(cr, 640 - 60 * (1 - t * 0.6), ty + 6, 120 * (1 - t * 0.6), 6);
      setColor(cr, rgba(220, 240, 255, 200));
      cairo_fill(cr);
    }
    // Floor grating lines toward the door.
    for (int i = 0; i <= 10; ++i)
    {
      const double fx = 1280.0 * i / 10.0;
      cairo_move_to(cr, fx, 720);
      cairo_line_to(cr, x0 + (x1 - x0) * i / 10.0, y1);
      setColor(cr, rgba(20, 24, 34, 120));
      cairo_set_line_width(cr, 2);
      cairo_stroke(cr);
    }
    // The outer frame: hazard bands round the opening.
    const double band = 18;
    cairo_save(cr);
    cairo_rectangle(cr, x0 - band, y0 - band, kLockW + 2 * band, kLockH + 2 * band);
    cairo_rectangle(cr, x0, y0, kLockW, kLockH);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
    cairo_clip(cr);
    setColor(cr, rgb(255, 196, 30));
    cairo_paint(cr);
    for (int s = -40; s < 80; ++s)
    {
      cairo_move_to(cr, x0 - band + s * 24.0, y1 + band);
      cairo_line_to(cr, x0 - band + s * 24.0 + 12, y1 + band);
      cairo_line_to(cr, x0 - band + s * 24.0 + 12 + 600, y1 + band - 600);
      cairo_line_to(cr, x0 - band + s * 24.0 + 600, y1 + band - 600);
      cairo_close_path(cr);
    }
    setColor(cr, rgb(30, 30, 36));
    cairo_fill(cr);
    cairo_restore(cr);
    cairo_rectangle(cr, x0 - band, y0 - band, kLockW + 2 * band, kLockH + 2 * band);
    setColor(cr, kInk);
    cairo_set_line_width(cr, 3);
    cairo_stroke(cr);
    // The opening itself, cut out.
    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_rectangle(cr, x0, y0, kLockW, kLockH);
    cairo_fill(cr);
    cairo_restore(cr);
    // The inner doorway, dark against the light: a thick frame round the shot.
    cairo_rectangle(cr, 0, 0, 1280, 720);
    roundedRect(cr, 70, 46, 1140, 720, 40);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
    setColor(cr, rgb(12, 14, 22));
    cairo_fill(cr);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
    roundedRect(cr, 70, 46, 1140, 720, 40);
    setColor(cr, rgba(120, 140, 170, 120));
    cairo_set_line_width(cr, 3);
    cairo_stroke(cr);
  });
  k.r.draw(room, ox, oy);
}

// The outer door: a heavy panel, hazard-striped along its leading edge, a
// stencilled 42 and a small round port.
void hullDoor(ClipKit& k, float x, float y)
{
  const Texture& door = cached(k, "e3_hull_door", int(kLockW), int(kLockH), 0, 0, [](cairo_t* cr) {
    const double w = kLockW, h = kLockH;
    cairo_rectangle(cr, 0, 0, w, h);
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 0, w, 0);
    cairo_pattern_add_color_stop_rgb(g, 0, 0.84, 0.87, 0.92);
    cairo_pattern_add_color_stop_rgb(g, 1, 0.6, 0.64, 0.72);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
    for (int row = 1; row < 4; ++row)
    {
      cairo_rectangle(cr, 40, row * h / 4.0 - 2, w - 80, 4);
      setColor(cr, rgba(80, 90, 110, 120));
      cairo_fill(cr);
    }
    // The leading edge (on the left: it slides off to the right).
    cairo_save(cr);
    cairo_rectangle(cr, 0, 0, 34, h);
    cairo_clip(cr);
    setColor(cr, rgb(255, 196, 30));
    cairo_paint(cr);
    for (double y = -40; y < h + 40; y += 28)
    {
      cairo_move_to(cr, 0, y);
      cairo_line_to(cr, 34, y - 20);
      cairo_line_to(cr, 34, y - 6);
      cairo_line_to(cr, 0, y + 14);
      cairo_close_path(cr);
    }
    setColor(cr, rgb(30, 30, 36));
    cairo_fill(cr);
    cairo_restore(cr);
    cairo_arc(cr, w * 0.55, h * 0.3, 34, 0, 2 * kPi);
    setColor(cr, rgb(70, 80, 100));
    cairo_fill(cr);
    cairo_arc(cr, w * 0.55, h * 0.3, 26, 0, 2 * kPi);
    setColor(cr, rgb(20, 40, 70));
    cairo_fill(cr);
    selectGameFont(cr);
    cairo_set_font_size(cr, 110);
    cairo_move_to(cr, w * 0.38, h * 0.82);
    cairo_text_path(cr, "42");
    setColor(cr, rgba(40, 44, 56, 200));
    cairo_fill(cr);
    cairo_rectangle(cr, 0, 0, w, h);
    setColor(cr, kInk);
    cairo_set_line_width(cr, 3);
    cairo_stroke(cr);
  });
  k.r.draw(door, x, y);
}

// Level 18's briefing: in the airlock, the outer door slides open on the
// sunrise over the planet; the crew stands dark in the inner doorway while
// frost blows out past them into space.
void hullAirlock(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  hullSunrise(k, ox, oy);
  const float t = std::min(1.0f, float(frame) / 11.0f);
  const float open = t * t * (3.0f - 2.0f * t);
  // Light pouring in as it opens.
  if (open > 0.0f)
    drawGlow(k.r, k.art, kLockX + kLockW * 0.6f + ox, kLockY + kLockH * 0.7f + oy, 300.0f * open, rgb(255, 170, 90),
      0.35f * open);
  hullDoor(k, kLockX + kLockW * open + ox, kLockY + oy);
  hullChamber(k, ox, oy);
  // The warning beacons over the door, turning while it opens.
  if (frame < 14)
    for (const float bx : {kLockX - 40.0f, kLockX + kLockW + 40.0f})
      drawGlow(k.r, k.art, bx + ox, kLockY - 40.0f + oy, (frame % 2 ? 46.0f : 30.0f), rgb(255, 150, 30), 0.9f);
  // The sunlight falling along the floor toward us.
  if (open > 0.0f)
  {
    DrawOpts lo;
    lo.blend = Blend::Add;
    for (int k2 = 0; k2 < 6; ++k2)
    {
      const float y = kLockY + kLockH + float(k2) * 30.0f;
      const float spread = 1.0f + float(k2) * 0.25f;
      k.r.fillRect(640.0f - kLockW * 0.5f * spread * open + ox, y + oy, kLockW * spread * open, 30.0f,
        rgba(255, 170, 90, int(40 - k2 * 6)), Blend::Add);
    }
  }
  // The crew in silhouette in the inner doorway, rimmed by the sunrise.
  static const int kWho[3] = {1, 0, 2};
  static const float kAt[3] = {300.0f, 600.0f, 960.0f};
  for (int i = 0; i < 3; ++i)
  {
    DrawOpts so;
    so.tint = rgb(14, 16, 26);
    const Texture& tex = runner(k, kWho[i], 0, 2.6f, i == 2);
    DrawOpts rim;
    rim.blend = Blend::Add;
    rim.tint = rgb(255, 150, 70);
    rim.alpha = 0.35f * open;
    k.r.draw(tex, kAt[i] + 3.0f + ox, 735.0f + oy, rim);
    k.r.draw(tex, kAt[i] + ox, 735.0f + oy, so);
  }
  // Frost flakes blowing out through the opening, a 6-frame loop.
  if (frame >= 12)
  {
    const int f = (frame - 12) % 6;
    for (int i = 0; i < 60; ++i)
    {
      const float sx = float(hash2(i, 191) % 1280u), sy = float(hash2(i, 192) % 720u);
      const float phase = std::fmod(float(f) / 6.0f + float(hash2(i, 193) % 100u) / 100.0f, 1.0f);
      // From near the camera, out to the middle of the opening and away.
      const float tx = kLockX + kLockW * (0.3f + 0.4f * float(hash2(i, 194) % 100u) / 100.0f);
      const float ty = kLockY + kLockH * (0.3f + 0.4f * float(hash2(i, 195) % 100u) / 100.0f);
      const float px = sx + (tx - sx) * phase, py = sy + (ty - sy) * phase;
      const float s = (1.0f - phase) * 9.0f + 2.0f;
      k.r.fillRect(px - s * 0.5f + ox, py - s * 0.5f + oy, s, s, rgba(235, 248, 255, int(220 * (1.0f - phase * 0.6f))));
      k.r.drawLine(px + ox, py + oy, px - (tx - sx) * 0.04f + ox, py - (ty - sy) * 0.04f + oy, s * 0.4f,
        rgba(235, 248, 255, 90));
    }
  }
  (void)ticks;
}

// Level 19: a white test chamber: panelled back wall under strip lights,
// steel ceiling and floor with orange safety bands, the walls' edges. The
// ceiling's underside is at y 110, the floor's top at y 600.
constexpr float kLabCeil = 110.0f, kLabFloor = 600.0f;

void labBand(cairo_t* cr, double y, double h, bool flip)
{
  cairo_save(cr);
  cairo_rectangle(cr, 0, y, 1280, h);
  cairo_clip(cr);
  setColor(cr, rgb(255, 136, 36));
  cairo_paint(cr);
  for (double x = -40; x < 1320; x += 28)
  {
    cairo_move_to(cr, x, flip ? y : y + h);
    cairo_line_to(cr, x + 14, flip ? y : y + h);
    cairo_line_to(cr, x + 14 + h, flip ? y + h : y);
    cairo_line_to(cr, x + h, flip ? y + h : y);
    cairo_close_path(cr);
  }
  setColor(cr, rgb(44, 46, 56));
  cairo_fill(cr);
  cairo_restore(cr);
}

void labChamber(ClipKit& k, float ox, float oy)
{
  const Texture& room = cached(k, "e3_grav_chamber", 1280, 720, 0, 0, [](cairo_t* cr) {
    gradient(cr, 1280, 720, rgb(226, 232, 240), rgb(246, 248, 252), rgb(214, 222, 232));
    // Back wall panels.
    for (int row = 0; row < 4; ++row)
      for (int col = -1; col < 8; ++col)
      {
        const double x = col * 180 + (row % 2) * 90 + 10, y = kLabCeil + 8 + row * 124;
        roundedRect(cr, x, y, 170, 114, 10);
        cairo_pattern_t* g = cairo_pattern_create_linear(0, y, 0, y + 114);
        cairo_pattern_add_color_stop_rgba(g, 0, 1, 1, 1, 0.9);
        cairo_pattern_add_color_stop_rgba(g, 1, 0.88, 0.9, 0.94, 0.9);
        cairo_set_source(cr, g);
        cairo_fill_preserve(cr);
        cairo_pattern_destroy(g);
        setColor(cr, rgba(150, 160, 180, 160));
        cairo_set_line_width(cr, 2);
        cairo_stroke(cr);
      }
    // Soft light pooling down from the ceiling.
    for (int i = 0; i < 3; ++i)
      radialGlow(cr, 260 + i * 380.0, kLabCeil + 20, 300, rgb(255, 255, 255), 0.6);
    // Ceiling and floor slabs.
    for (int pass = 0; pass < 2; ++pass)
    {
      const double y0 = pass == 0 ? 0 : kLabFloor, y1 = pass == 0 ? kLabCeil : 720;
      cairo_rectangle(cr, 0, y0, 1280, y1 - y0);
      cairo_pattern_t* g = cairo_pattern_create_linear(0, y0, 0, y1);
      cairo_pattern_add_color_stop_rgb(g, 0, pass ? 0.86 : 0.5, pass ? 0.88 : 0.53, pass ? 0.92 : 0.6);
      cairo_pattern_add_color_stop_rgb(g, 1, pass ? 0.5 : 0.82, pass ? 0.53 : 0.85, pass ? 0.6 : 0.9);
      cairo_set_source(cr, g);
      cairo_fill(cr);
      cairo_pattern_destroy(g);
      for (double x = 0; x < 1280; x += 128)
      {
        cairo_rectangle(cr, x, y0, 2, y1 - y0);
        setColor(cr, rgba(60, 66, 84, 120));
        cairo_fill(cr);
      }
    }
    labBand(cr, kLabFloor + 6, 14, false);
    labBand(cr, kLabCeil - 20, 14, true);
    cairo_rectangle(cr, 0, kLabFloor, 1280, 6);
    cairo_rectangle(cr, 0, kLabCeil - 6, 1280, 6);
    setColor(cr, rgb(255, 255, 255));
    cairo_fill(cr);
    // Strip lights in the ceiling.
    for (int i = 0; i < 3; ++i)
    {
      roundedRect(cr, 170 + i * 380.0, kLabCeil - 30, 180, 10, 5);
      setColor(cr, rgb(255, 255, 255));
      cairo_fill(cr);
    }
    // The side walls' edges.
    for (const double x : {0.0, 1220.0})
    {
      cairo_rectangle(cr, x, 0, 60, 720);
      cairo_pattern_t* g = cairo_pattern_create_linear(x, 0, x + 60, 0);
      cairo_pattern_add_color_stop_rgb(g, 0, 0.62, 0.66, 0.72);
      cairo_pattern_add_color_stop_rgb(g, 1, 0.42, 0.46, 0.54);
      cairo_set_source(cr, g);
      cairo_fill(cr);
      cairo_pattern_destroy(g);
    }
    // The observation window high on the back wall, someone watching.
    roundedRect(cr, 560, 150, 160, 70, 10);
    setColor(cr, rgb(196, 204, 218));
    cairo_fill(cr);
    roundedRect(cr, 570, 160, 140, 50, 6);
    setColor(cr, rgb(20, 34, 54));
    cairo_fill(cr);
    cairo_arc(cr, 620, 196, 12, 0, 2 * kPi);
    setColor(cr, rgb(40, 52, 72));
    cairo_fill(cr);
    cairo_rectangle(cr, 610, 190, 20, 5);
    setColor(cr, rgba(255, 190, 110, 220));
    cairo_fill(cr);
    // The chamber number.
    selectGameFont(cr);
    cairo_set_font_size(cr, 54);
    cairo_move_to(cr, 90, 190);
    setColor(cr, rgba(150, 160, 180, 120));
    cairo_show_text(cr, "C1");
  });
  k.r.draw(room, ox, oy);
}

// A wall arrow on its plate, `size` px across, pointing down turned by
// `angle` degrees.
void labArrow(ClipKit& k, float cx, float cy, float size, float angle)
{
  const int s = int(size);
  const Texture& plate = cached(k, "e3_grav_plate" + std::to_string(s), s, s, size * 0.5f, size * 0.5f, [size](cairo_t* cr) {
    const double u = size / 64.0;
    cairo_scale(cr, u, u);
    roundedRect(cr, 3, 3, 58, 58, 8);
    setColor(cr, rgb(52, 56, 70));
    cairo_fill_preserve(cr);
    setColor(cr, kInk);
    cairo_set_line_width(cr, 2);
    cairo_stroke(cr);
    cairo_arc(cr, 32, 32, 24, 0, 2 * kPi);
    setColor(cr, rgb(246, 248, 252));
    cairo_fill_preserve(cr);
    setColor(cr, kInk);
    cairo_set_line_width(cr, 1.6);
    cairo_stroke(cr);
    for (double x : {9.0, 55.0})
      for (double y : {9.0, 55.0})
      {
        cairo_arc(cr, x, y, 2.4, 0, 2 * kPi);
        setColor(cr, rgb(170, 178, 194));
        cairo_fill(cr);
      }
  });
  // The arrow is baked at each angle it shows (a handful per shot), so it
  // stays crisp instead of being turned as a texture.
  const int deg = int(std::lround(angle));
  const Texture& arrow = cached(k, "e3_grav_arrow" + std::to_string(s) + "/" + std::to_string(deg), s, s,
    size * 0.5f, size * 0.5f, [size, deg](cairo_t* cr) {
      const double u = size * 0.75 / 48.0;
      cairo_translate(cr, size * 0.5, size * 0.5);
      cairo_rotate(cr, deg * kPi / 180.0);
      cairo_scale(cr, u, u);
      cairo_translate(cr, -24, -24);
      cairo_move_to(cr, 17, 5);
      cairo_line_to(cr, 31, 5);
      cairo_line_to(cr, 31, 24);
      cairo_line_to(cr, 41, 24);
      cairo_line_to(cr, 24, 43);
      cairo_line_to(cr, 7, 24);
      cairo_line_to(cr, 17, 24);
      cairo_close_path(cr);
      cairo_pattern_t* g = cairo_pattern_create_linear(7, 0, 41, 0);
      cairo_pattern_add_color_stop_rgb(g, 0, 1.0, 0.8, 0.5);
      cairo_pattern_add_color_stop_rgb(g, 1, 0.93, 0.4, 0.08);
      cairo_set_source(cr, g);
      cairo_fill_preserve(cr);
      cairo_pattern_destroy(g);
      setColor(cr, kInk);
      cairo_set_line_width(cr, 2.2);
      cairo_stroke(cr);
    });
  k.r.draw(plate, cx, cy);
  k.r.draw(arrow, cx, cy);
}

// Rocco's sandwich: a fat sub, lettuce and tomato hanging out, 90 x 44,
// anchored at its bottom middle.
const Texture& sandwich(ClipKit& k)
{
  return cached(k, "e3_grav_sandwich", 90, 44, 45.0f, 42.0f, [](cairo_t* cr) {
    auto bun = [&](double y, double h, bool top) {
      roundedRect(cr, 4, y, 82, h, h * 0.5);
      cairo_pattern_t* g = cairo_pattern_create_linear(0, y, 0, y + h);
      cairo_pattern_add_color_stop_rgb(g, 0, top ? 0.96 : 0.86, top ? 0.72 : 0.6, top ? 0.4 : 0.32);
      cairo_pattern_add_color_stop_rgb(g, 1, 0.7, 0.44, 0.2);
      cairo_set_source(cr, g);
      cairo_fill_preserve(cr);
      cairo_pattern_destroy(g);
      setColor(cr, kInk);
      cairo_set_line_width(cr, 2);
      cairo_stroke(cr);
    };
    bun(28, 13, false);
    // Lettuce, tomato, cheese.
    cairo_move_to(cr, 2, 28);
    for (int i = 0; i <= 10; ++i)
      cairo_line_to(cr, 2 + i * 8.6, i % 2 ? 22 : 30);
    cairo_line_to(cr, 88, 30);
    cairo_close_path(cr);
    setColor(cr, rgb(110, 200, 80));
    cairo_fill(cr);
    cairo_rectangle(cr, 10, 22, 70, 5);
    setColor(cr, rgb(255, 200, 60));
    cairo_fill(cr);
    for (int i = 0; i < 3; ++i)
    {
      cairo_rectangle(cr, 14 + i * 24, 18, 18, 5);
      setColor(cr, rgb(230, 60, 50));
      cairo_fill(cr);
    }
    bun(2, 18, true);
    for (int i = 0; i < 6; ++i)
    {
      cairo_arc(cr, 16 + i * 12, 8 + (i % 2) * 3, 1.3, 0, 2 * kPi);
      setColor(cr, rgb(255, 240, 200));
      cairo_fill(cr);
    }
  });
}

// Level 19's briefing, shot 1: the crew standing in a white chamber, the
// orange arrows on the wall pointing down, Rocco with a sandwich.
void gravLab(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  labChamber(k, ox, oy);
  for (const float ax : {200.0f, 1080.0f})
    labArrow(k, ax + ox, 300.0f + oy, 96.0f, 0.0f);
  // The monitor light by the window blinking with the bleeps.
  drawGlow(k.r, k.art, 740.0f + ox, 170.0f + oy, 12.0f, frame % 3 ? rgb(90, 255, 140) : rgb(255, 150, 40), 0.9f);
  const float scale = 2.4f;
  const bool blink = frame == 6 || frame == 7;
  k.r.draw(runner(k, 0, blink ? 8 : 0, scale), 420 + ox, kLabFloor + oy);
  k.r.draw(runner(k, 2, blink ? 8 : 0, scale, true), 870 + ox, kLabFloor + oy);
  k.r.draw(runner(k, 1, blink ? 8 : 0, scale), 640 + ox, kLabFloor + oy);
  // The sandwich, held out in front of him.
  k.r.draw(sandwich(k), 712 + ox, 476 + oy);
  (void)ticks;
}

// Shot 2: close on a wall arrow; it swings round 180 degrees (frames 0-5),
// a klaxon flashing, then holds pointing up.
void gravArrows(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  const Texture& wall = cached(k, "e3_grav_wall_close", 1280, 720, 0, 0, [](cairo_t* cr) {
    gradient(cr, 1280, 720, rgb(232, 236, 244), rgb(250, 251, 253), rgb(220, 226, 236));
    for (int row = -1; row < 3; ++row)
      for (int col = -1; col < 4; ++col)
      {
        const double x = col * 420 + (row % 2 ? 210 : 0) + 20, y = row * 300 + 80;
        roundedRect(cr, x, y, 400, 280, 22);
        setColor(cr, rgba(255, 255, 255, 220));
        cairo_fill_preserve(cr);
        setColor(cr, rgba(150, 160, 180, 170));
        cairo_set_line_width(cr, 4);
        cairo_stroke(cr);
      }
    radialGlow(cr, 640, 0, 600, rgb(255, 255, 255), 0.5);
    selectGameFont(cr);
    cairo_set_font_size(cr, 46);
    cairo_text_extents_t e;
    cairo_text_extents(cr, "GRAVITY", &e);
    cairo_move_to(cr, 640 - e.width * 0.5 - e.x_bearing, 640);
    setColor(cr, rgba(60, 66, 84, 200));
    cairo_show_text(cr, "GRAVITY");
  });
  k.r.draw(wall, ox, oy);
  const bool alarm = frame <= 5;
  if (alarm)
    for (const float bx : {170.0f, 1110.0f})
    {
      // A klaxon beacon: a dark housing, its orange dome flashing.
      const Texture& beacon = cached(k, std::string("e3_grav_beacon") + (frame % 2 ? "1" : "0"), 120, 110, 60.0f, 0.0f,
        [lit = frame % 2 == 1](cairo_t* cr) {
          roundedRect(cr, 20, 0, 80, 26, 6);
          setColor(cr, rgb(52, 56, 70));
          cairo_fill_preserve(cr);
          setColor(cr, kInk);
          cairo_set_line_width(cr, 3);
          cairo_stroke(cr);
          cairo_new_sub_path(cr);
          cairo_arc(cr, 60, 26, 36, 0, kPi);
          cairo_close_path(cr);
          cairo_pattern_t* g = cairo_pattern_create_radial(50, 40, 4, 60, 30, 40);
          cairo_pattern_add_color_stop_rgb(g, 0, 1.0, lit ? 0.95 : 0.7, lit ? 0.7 : 0.4);
          cairo_pattern_add_color_stop_rgb(g, 1, lit ? 1.0 : 0.75, lit ? 0.5 : 0.3, lit ? 0.1 : 0.05);
          cairo_set_source(cr, g);
          cairo_fill_preserve(cr);
          cairo_pattern_destroy(g);
          setColor(cr, kInk);
          cairo_set_line_width(cr, 3);
          cairo_stroke(cr);
        });
      if (frame % 2)
        drawGlow(k.r, k.art, bx + ox, 100.0f + oy, 120.0f, rgb(255, 120, 20), 0.5f);
      k.r.draw(beacon, bx + ox, 40.0f + oy);
    }
  const float t = std::min(1.0f, float(frame) / 5.0f);
  const float e = t * t * (3.0f - 2.0f * t);
  if (frame <= 5)
    drawGlow(k.r, k.art, 640.0f + ox, 340.0f + oy, 260.0f, rgb(255, 150, 50), 0.35f);
  // A little overshoot as it clunks home on frame 5.
  const float settle = frame == 5 ? 6.0f : (frame == 6 ? -3.0f : 0.0f);
  labArrow(k, 640.0f + ox, 340.0f + oy, 440.0f, 180.0f * e + settle);
  (void)ticks;
}

// Shot 3: wide; the crew falls up to the ceiling (frames 0-9), turning
// over on the way, and lands on it upside down. Rocco's sandwich stays on
// the floor; he looks down at it (frames 10-15).
void gravFlip(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  labChamber(k, ox, oy);
  for (const float ax : {200.0f, 1080.0f})
    labArrow(k, ax + ox, 300.0f + oy, 96.0f, 180.0f);
  const float scale = 2.4f, half = 130.0f * scale * 0.5f;
  const float t = std::min(1.0f, float(frame) / 9.0f);
  const float fall = t * t;   // speeding up as it goes
  const float turn = std::min(1.0f, t * 1.4f);
  const float spin = turn * turn * (3.0f - 2.0f * turn) * 180.0f;
  static const int kWho[3] = {0, 1, 2};
  static const float kAt[3] = {420.0f, 640.0f, 870.0f};
  // The sandwich, left behind on the floor.
  k.r.draw(sandwich(k), 712.0f + ox, kLabFloor + 2.0f + oy);
  for (int i = 0; i < 3; ++i)
  {
    const bool mirror = i == 2;
    // Each body's middle rises from standing to hanging; it turns about it.
    const float c0 = kLabFloor - half, c1 = kLabCeil + half;
    const float cy = c0 + (c1 - c0) * fall + (i == 1 ? 0.0f : float(i - 1) * 6.0f * std::sin(t * 3.14159f));
    if (frame >= 9)
    {
      // Landed: hanging from the ceiling, still facing their way.
      int pose = 0;
      if (frame >= 10 && i == 1)
        pose = 3; // looking "up": down at the floor, at the sandwich
      else if (frame == 9)
        pose = 4; // the landing squash
      DrawOpts o;
      o.angle = 180.0f;
      k.r.draw(runner(k, kWho[i], pose, scale, !mirror), kAt[i] + ox, kLabCeil + oy, o);
      continue;
    }
    const float a = spin * 0.0174533f * (mirror ? -1.0f : 1.0f);
    DrawOpts o;
    o.angle = spin * (mirror ? -1.0f : 1.0f);
    const float fx = kAt[i] - half * std::sin(a), fy = cy + half * std::cos(a);
    k.r.draw(runner(k, kWho[i], 7, scale, mirror), fx + ox, fy + oy, o);
    // Speed lines trailing below them.
    if (frame >= 2)
      for (int l = 0; l < 3; ++l)
      {
        const float lx = kAt[i] - 50.0f + float(l) * 50.0f;
        k.r.drawLine(lx + ox, cy + 90.0f + oy, lx + ox, cy + 130.0f + 90.0f * fall + oy, 3.0f, rgba(150, 160, 180, 130));
      }
  }
  if (frame == 9 || frame == 10)
    for (int i = 0; i < 3; ++i)
      for (int p = 0; p < 6; ++p)
        drawGlow(k.r, k.art, kAt[i] - 60.0f + float(p) * 24.0f + ox, kLabCeil + 14.0f + float(frame - 9) * 10.0f + oy,
          22.0f, rgb(200, 206, 220), 0.6f);
  (void)ticks;
}

// Level 20: the reactor hall. Lead-lined walls banded with yellow and black,
// the core a tall glass column in the middle (its glow is drawn over it), a
// steel gantry across the front at y 600.
constexpr float kCoreX = 640.0f, kCoreY = 330.0f, kGantry = 600.0f;
constexpr Color kCoreBlue = rgb(90, 180, 255);

// A yellow and black hazard band across [x0, x1].
void hazardBand(cairo_t* cr, double x0, double x1, double y, double h)
{
  cairo_save(cr);
  cairo_rectangle(cr, x0, y, x1 - x0, h);
  cairo_clip(cr);
  setColor(cr, rgb(255, 204, 40));
  cairo_paint(cr);
  for (double x = x0 - h; x < x1 + h; x += 36)
  {
    cairo_move_to(cr, x, y + h);
    cairo_line_to(cr, x + 18, y + h);
    cairo_line_to(cr, x + 18 + h, y);
    cairo_line_to(cr, x + h, y);
    cairo_close_path(cr);
  }
  setColor(cr, rgb(24, 24, 30));
  cairo_fill(cr);
  cairo_restore(cr);
}

// Riveted lead panels filling a rectangle.
void leadPanels(cairo_t* cr, double x0, double y0, double x1, double y1, double pw, double ph)
{
  for (double y = y0; y < y1; y += ph)
    for (double x = x0; x < x1; x += pw)
    {
      const int seed = int(x * 7 + y * 13);
      roundedRect(cr, x + 3, y + 3, pw - 6, ph - 6, 4);
      const double v = 0.24 + double(hash2(seed, 20) % 6u) * 0.012;
      cairo_pattern_t* g = cairo_pattern_create_linear(0, y, 0, y + ph);
      cairo_pattern_add_color_stop_rgb(g, 0, v + 0.04, v + 0.07, v + 0.12);
      cairo_pattern_add_color_stop_rgb(g, 1, v - 0.06, v - 0.04, v);
      cairo_set_source(cr, g);
      cairo_fill_preserve(cr);
      cairo_pattern_destroy(g);
      setColor(cr, rgba(6, 8, 16, 200));
      cairo_set_line_width(cr, 2);
      cairo_stroke(cr);
      for (const double rx : {x + 12, x + pw - 12})
        for (const double ry : {y + 12, y + ph - 12})
        {
          cairo_arc(cr, rx, ry, 2.6, 0, 2 * kPi);
          setColor(cr, rgb(120, 130, 150));
          cairo_fill(cr);
        }
    }
}

void reactorHall(ClipKit& k, float ox, float oy)
{
  const Texture& hall = cached(k, "e3_reactor_hall", 1280, 720, 0, 0, [](cairo_t* cr) {
    gradient(cr, 1280, 720, rgb(8, 12, 26), rgb(16, 26, 50), rgb(6, 10, 20));
    // The far wall behind the core, deep in shadow.
    leadPanels(cr, 0, 0, 1280, 720, 160, 120);
    cairo_rectangle(cr, 0, 0, 1280, 720);
    setColor(cr, rgba(4, 10, 30, 150));
    cairo_fill(cr);
    // The side walls, nearer: lighter lead, hazard bands, a stencil.
    for (int side = 0; side < 2; ++side)
    {
      const double x0 = side ? 1010 : 0, x1 = side ? 1280 : 270;
      cairo_save(cr);
      cairo_rectangle(cr, x0, 0, x1 - x0, 720);
      cairo_clip(cr);
      leadPanels(cr, x0 - (side ? 0 : 50), -20, x1 + 50, 720, 135, 150);
      hazardBand(cr, x0, x1, 96, 26);
      hazardBand(cr, x0, x1, 470, 26);
      selectGameFont(cr);
      cairo_set_font_size(cr, 58);
      cairo_move_to(cr, side ? x0 + 70 : 70, 330);
      setColor(cr, rgba(255, 204, 40, 150));
      cairo_show_text(cr, "Pb");
      cairo_set_font_size(cr, 22);
      cairo_move_to(cr, side ? x0 + 74 : 74, 362);
      cairo_show_text(cr, "LEAD 40 CM");
      cairo_restore(cr);
      // The wall's inner edge, catching the core's light.
      const double ex = side ? x0 : x1 - 14;
      cairo_rectangle(cr, ex, 0, 14, 720);
      cairo_pattern_t* g = cairo_pattern_create_linear(ex, 0, ex + 14, 0);
      cairo_pattern_add_color_stop_rgb(g, side ? 0 : 1, 0.5, 0.72, 1.0);
      cairo_pattern_add_color_stop_rgb(g, side ? 1 : 0, 0.14, 0.18, 0.28);
      cairo_set_source(cr, g);
      cairo_fill(cr);
      cairo_pattern_destroy(g);
    }
    // The core: a glass column the height of the hall, banded by dark
    // containment rings with lamps.
    const double cx = 640, cw = 250;
    cairo_rectangle(cr, cx - cw * 0.5, 0, cw, 720);
    cairo_pattern_t* g = cairo_pattern_create_linear(cx - cw * 0.5, 0, cx + cw * 0.5, 0);
    cairo_pattern_add_color_stop_rgb(g, 0.0, 0.04, 0.14, 0.38);
    cairo_pattern_add_color_stop_rgb(g, 0.35, 0.3, 0.62, 1.0);
    cairo_pattern_add_color_stop_rgb(g, 0.5, 0.8, 0.94, 1.0);
    cairo_pattern_add_color_stop_rgb(g, 0.65, 0.3, 0.62, 1.0);
    cairo_pattern_add_color_stop_rgb(g, 1.0, 0.04, 0.14, 0.38);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
    for (const double ry : {40.0, 250.0, 460.0})
    {
      roundedRect(cr, cx - cw * 0.5 - 22, ry, cw + 44, 46, 8);
      cairo_pattern_t* rg = cairo_pattern_create_linear(0, ry, 0, ry + 46);
      cairo_pattern_add_color_stop_rgb(rg, 0, 0.36, 0.4, 0.48);
      cairo_pattern_add_color_stop_rgb(rg, 1, 0.12, 0.14, 0.2);
      cairo_set_source(cr, rg);
      cairo_fill_preserve(cr);
      cairo_pattern_destroy(rg);
      setColor(cr, kInk);
      cairo_set_line_width(cr, 3);
      cairo_stroke(cr);
      for (int l = 0; l < 5; ++l)
      {
        cairo_arc(cr, cx - 100 + l * 50, ry + 23, 6, 0, 2 * kPi);
        setColor(cr, rgb(140, 220, 255));
        cairo_fill(cr);
      }
    }
  });
  k.r.draw(hall, ox, oy);
}

// The gantry the crew stands on: a grated deck and a railing, in front.
void reactorGantry(ClipKit& k, float ox, float oy)
{
  const Texture& deck = cached(k, "e3_reactor_gantry", 1280, 200, 0, 0, [](cairo_t* cr) {
    const double top = 60; // the deck's top is at y 60 in the texture (kGantry on screen)
    cairo_rectangle(cr, 0, top, 1280, 26);
    setColor(cr, rgb(30, 36, 50));
    cairo_fill(cr);
    for (double x = 0; x < 1280; x += 12)
    {
      cairo_rectangle(cr, x, top + 4, 6, 18);
      setColor(cr, rgb(12, 16, 26));
      cairo_fill(cr);
    }
    cairo_rectangle(cr, 0, top, 1280, 4);
    setColor(cr, rgb(130, 170, 220));
    cairo_fill(cr);
    hazardBand(cr, 0, 1280, top + 26, 12);
    // Trusses under the deck.
    for (double x = 0; x < 1280; x += 120)
    {
      cairo_move_to(cr, x, top + 38);
      cairo_line_to(cr, x + 60, 200);
      cairo_line_to(cr, x + 120, top + 38);
      setColor(cr, rgb(20, 24, 36));
      cairo_set_line_width(cr, 8);
      cairo_stroke(cr);
    }
    // The railing: posts and two rails, the top one catching the light.
    for (double x = 20; x < 1280; x += 140)
    {
      cairo_rectangle(cr, x, 0, 8, top);
      setColor(cr, rgb(24, 28, 40));
      cairo_fill(cr);
    }
    cairo_rectangle(cr, 0, 0, 1280, 7);
    setColor(cr, rgb(40, 48, 64));
    cairo_fill(cr);
    cairo_rectangle(cr, 0, 0, 1280, 2);
    setColor(cr, rgb(140, 190, 240));
    cairo_fill(cr);
    cairo_rectangle(cr, 0, 30, 1280, 5);
    setColor(cr, rgb(28, 34, 48));
    cairo_fill(cr);
  });
  k.r.draw(deck, ox, kGantry - 60.0f + oy);
}

// A containment ring bursting out of the core: a flat ellipse of light,
// 1200 x 360, drawn additively and scaled as it spreads.
const Texture& coreRing(ClipKit& k)
{
  return cached(k, "e3_reactor_ring", 1200, 360, 600.0f, 180.0f, [](cairo_t* cr) {
    for (int pass = 0; pass < 4; ++pass)
    {
      static const double kWidth[4] = {70, 36, 16, 6};
      static const double kAlpha[4] = {0.12, 0.25, 0.55, 1.0};
      cairo_save(cr);
      cairo_translate(cr, 600, 180);
      cairo_scale(cr, 1.0, 0.3);
      cairo_arc(cr, 0, 0, 520, 0, 2 * kPi);
      cairo_restore(cr);
      cairo_set_line_width(cr, kWidth[pass]);
      if (pass == 3)
        cairo_set_source_rgba(cr, 0.92, 0.97, 1.0, kAlpha[pass]);
      else
        cairo_set_source_rgba(cr, 0.4, 0.75, 1.0, kAlpha[pass]);
      cairo_stroke(cr);
    }
  });
}

// Level 20's briefing, shots 1 and 2: the reactor hall, the crew in
// silhouette on the gantry. The core's glow builds over frames 0-7, a ring
// bursts out of it on frame 8, and the light settles low on frame 9.
void reactorCore(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  reactorHall(k, ox, oy);
  const float build = std::min(1.0f, float(frame) / 7.0f);
  const float glow = frame <= 7 ? 0.15f + 0.85f * build * build : (frame == 8 ? 1.3f : 0.3f);
  // The core lighting up: a white heart, the column flooded, the hall lit.
  const float flicker = 0.92f + 0.08f * std::sin(float(ticks) * 0.5f);
  k.r.fillRect(kCoreX - 125.0f + ox, oy, 250.0f, H, rgba(120, 200, 255, int(std::min(1.0f, glow) * 110.0f)), Blend::Add);
  drawGlow(k.r, k.art, kCoreX + ox, kCoreY + oy, 260.0f + 200.0f * glow, kCoreBlue, std::min(1.0f, 0.3f + 0.5f * glow) * flicker);
  drawGlow(k.r, k.art, kCoreX + ox, kCoreY + oy, 90.0f + 90.0f * glow, rgb(220, 240, 255), std::min(1.0f, glow) * flicker);
  if (frame <= 7)
    for (int b = 0; b < 4; ++b)
    {
      // Energy rising up the column, faster as it builds.
      const float y = std::fmod(720.0f - float(ticks) * (2.0f + 6.0f * build) - float(b) * 180.0f, 720.0f);
      const float x = kCoreX + 50.0f * std::sin(float(b) * 1.7f + float(ticks) * 0.05f);
      drawGlow(k.r, k.art, x + ox, (y < 0.0f ? y + 720.0f : y) + oy, 40.0f, kCoreBlue, 0.3f + 0.5f * build);
    }
  if (frame == 8 || frame == 9)
  {
    // The ring, bursting out across the hall and past the crew.
    DrawOpts o;
    o.blend = Blend::Add;
    o.cull = false;
    o.scale = frame == 8 ? 0.62f : 1.5f;
    o.alpha = frame == 8 ? 1.0f : 0.5f;
    k.r.draw(coreRing(k), kCoreX + ox, kCoreY + 40.0f + oy, o);
    if (frame == 8)
      k.r.fillRect(ox, oy, W, H, rgba(140, 200, 255, 70), Blend::Add);
  }
  // The crew on the gantry, black against the light.
  DrawOpts sil;
  sil.tint = lerpColor(rgb(10, 14, 26), rgb(40, 60, 100), std::min(1.0f, glow) * 0.4f);
  const float scale = 1.6f;
  k.r.draw(runner(k, 1, 0, scale), 470.0f + ox, kGantry + oy, sil);
  k.r.draw(runner(k, 0, frame == 9 ? 3 : 0, scale), 600.0f + ox, kGantry + oy, sil);
  k.r.draw(runner(k, 2, 0, scale, true), 730.0f + ox, kGantry + oy, sil);
  reactorGantry(k, ox, oy);
}

// Shot 3: close on the crew, washed in the core's blue light from below
// left; a wall speaker up on the right carries Zero's voice, and Nova
// glances up at it on frames 3-5.
void reactorCrew(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  const Texture& wall = cached(k, "e3_reactor_wall_close", 1280, 720, 0, 0, [](cairo_t* cr) {
    gradient(cr, 1280, 720, rgb(10, 14, 26), rgb(20, 30, 54), rgb(12, 18, 34));
    leadPanels(cr, -40, -30, 1320, 760, 260, 220);
    hazardBand(cr, 0, 1280, 300, 40);
    cairo_rectangle(cr, 0, 0, 1280, 720);
    setColor(cr, rgba(6, 12, 30, 90));
    cairo_fill(cr);
    // The wall speaker: a grille in a steel box, bolted high on the wall.
    roundedRect(cr, 860, 70, 170, 150, 12);
    setColor(cr, rgb(60, 66, 82));
    cairo_fill_preserve(cr);
    setColor(cr, kInk);
    cairo_set_line_width(cr, 4);
    cairo_stroke(cr);
    roundedRect(cr, 876, 86, 138, 104, 8);
    setColor(cr, rgb(18, 20, 28));
    cairo_fill(cr);
    for (int g = 0; g < 7; ++g)
    {
      roundedRect(cr, 886, 94 + g * 13, 118, 6, 3);
      setColor(cr, rgb(72, 78, 96));
      cairo_fill(cr);
    }
    // Its cable running off up the wall.
    cairo_move_to(cr, 945, 70);
    cairo_curve_to(cr, 945, 30, 990, 20, 1000, -10);
    setColor(cr, rgb(16, 18, 26));
    cairo_set_line_width(cr, 8);
    cairo_stroke(cr);
  });
  k.r.draw(wall, ox, oy);
  // The core's light from below left, breathing with its hum.
  const float hum = 0.5f + 0.5f * std::sin(float(ticks) * 0.08f);
  drawGlow(k.r, k.art, 120.0f + ox, 760.0f + oy, 820.0f, kCoreBlue, 0.45f + 0.2f * hum);
  // The speaker's lamp flickers with Zero's voice.
  static const float kVoice[8] = {0.9f, 0.4f, 1.0f, 0.6f, 0.3f, 0.85f, 0.5f, 0.2f};
  const float v = kVoice[frame % 8];
  k.r.fillRect(1006.0f + ox, 196.0f + oy, 12.0f, 12.0f, rgb(120, 230, 255));
  drawGlow(k.r, k.art, 1012.0f + ox, 202.0f + oy, 18.0f + 22.0f * v, rgb(120, 220, 255), v);
  // The crew, close, tinted blue by it.
  DrawOpts wash;
  wash.tint = lerpColor(rgb(150, 190, 255), rgb(185, 215, 255), hum);
  const float scale = 4.2f, feet = 770.0f + oy;
  const bool blink = frame == 6;
  k.r.draw(runner(k, 1, blink ? 8 : 0, scale), 290.0f + ox, feet, wash);
  k.r.draw(runner(k, 0, 0, scale), 610.0f + ox, feet, wash);
  // Nova's glance: she turns toward the speaker and tips her head back
  // (the look-up pose raises the gun across her face, so the whole idle
  // figure leans back about her feet instead).
  const bool glance = frame >= 3 && frame <= 5;
  DrawOpts nova = wash;
  if (glance)
    nova.angle = frame == 4 ? -9.0f : -6.0f;
  k.r.draw(runner(k, 2, 0, scale, !glance), 950.0f + ox, feet, nova);
  // Blue light over everything, strongest low down by the core.
  k.r.fillRect(ox, 420.0f + oy, W, 300.0f, rgba(40, 110, 220, int(26.0f + 16.0f * hum)), Blend::Add);
}

// --- Level 21 and the end of Episode 3: ZERO ---------------------------------------------
//
// ZERO's eye, its racks, Lance and his audience are painted by the same
// routines the level uses (assets/enemy_art_zero.hpp), so the cutscenes
// match the arena.

constexpr Color kZeroRed = rgb(255, 40, 50);

void setRgbaC(cairo_t* cr, Color c, double a)
{
  cairo_set_source_rgba(cr, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, a);
}

// A texture squashed or stretched into any rectangle (the flat tipping back).
void stretchTex(ClipKit& k, const Texture& t, float x, float y, float w, float h, Color tint, float alpha)
{
  if (!t || w <= 0.5f || h <= 0.5f)
    return;
  const SDL_FRect dst{x, y, w, h};
  SDL_SetTextureBlendMode(t.get(), SDL_BLENDMODE_BLEND);
  SDL_SetTextureAlphaMod(t.get(), Uint8(std::lround(std::clamp(alpha, 0.0f, 1.0f) * 255.0f)));
  SDL_SetTextureColorMod(t.get(), Uint8(redOf(tint)), Uint8(greenOf(tint)), Uint8(blueOf(tint)));
  SDL_RenderCopyF(k.r.sdl(), t.get(), nullptr, &dst);
  SDL_SetTextureColorMod(t.get(), 255, 255, 255);
  SDL_SetTextureAlphaMod(t.get(), 255);
}

void clipTo(ClipKit& k, float x, float y, float w, float h)
{
  const SDL_Rect rc{int(std::floor(x)), int(std::floor(y)), std::max(0, int(std::ceil(w))), std::max(0, int(std::ceil(h)))};
  SDL_RenderSetClipRect(k.r.sdl(), &rc);
}

void unclip(ClipKit& k) { SDL_RenderSetClipRect(k.r.sdl(), nullptr); }

void scanlines(ClipKit& k, int alpha, float y0 = 0.0f, float y1 = H, float x0 = 0.0f, float w = W)
{
  for (float y = y0; y < y1; y += 4.0f)
    k.r.fillRect(x0, y, w, 1.0f, rgba(0, 0, 0, alpha));
}

// ZERO's eye on its own, `size` px square, centred.
const Texture& zeroEye(ClipKit& k, int size, float pupil, float shutter, float light)
{
  const int pq = int(std::lround(pupil * 40.0f)), sq = int(std::lround(shutter * 10.0f)),
            lq = int(std::lround(light * 20.0f));
  const std::string key = "e3_eye" + std::to_string(size) + "_" + std::to_string(pq) + "_" + std::to_string(sq) + "_" +
    std::to_string(lq);
  const float half = float(size) * 0.5f;
  return cached(k, key, size, size, half, half, [&](cairo_t* cr) {
    paintZeroEye(cr, half, half, half - 4.0, double(pq) / 40.0, double(sq) / 10.0, double(lq) / 20.0, 0.0);
  });
}

// A lens's knurled zoom ring: tick marks and focal lengths round a circle,
// drawn faintly over the eye and turned as it focuses.
const Texture& zoomRing(ClipKit& k)
{
  return cached(k, "e3_zoom_ring", 980, 980, 490, 490, [](cairo_t* cr) {
    for (int i = 0; i < 120; ++i)
    {
      const double a = 2 * kPi * i / 120.0;
      const double r0 = i % 10 == 0 ? 440 : 456, r1 = 470;
      cairo_move_to(cr, 490 + std::cos(a) * r0, 490 + std::sin(a) * r0);
      cairo_line_to(cr, 490 + std::cos(a) * r1, 490 + std::sin(a) * r1);
    }
    setRgbaC(cr, rgb(255, 120, 120), 0.9);
    cairo_set_line_width(cr, 3);
    cairo_stroke(cr);
    cairo_arc(cr, 490, 490, 476, 0, 2 * kPi);
    cairo_set_line_width(cr, 2);
    cairo_stroke(cr);
    selectGameFont(cr);
    cairo_set_font_size(cr, 22);
    static const char* kMarks[] = {"18", "24", "35", "42", "50", "70", "85", "INF"};
    for (int i = 0; i < 8; ++i)
    {
      const double a = 2 * kPi * i / 8.0 - kPi / 2;
      cairo_text_extents_t te;
      cairo_text_extents(cr, kMarks[i], &te);
      cairo_move_to(cr, 490 + std::cos(a) * 418 - te.width / 2, 490 + std::sin(a) * 418 + te.height / 2);
      cairo_show_text(cr, kMarks[i]);
    }
  });
}

// ZERO's terminal: green code, a long column of it (scrolled behind the eye).
const Texture& terminalText(ClipKit& k)
{
  return cached(k, "e3_terminal", 1280, 1500, 0, 0, [](cairo_t* cr) {
    selectGameFont(cr);
    cairo_set_font_size(cr, 19);
    static const char* kLines[] = {
      "> LOAD SEASON_01/EP_%02d.SCRIPT ... OK",
      "> RUNNER DASH    STATUS: ON SCHEDULE",
      "> RUNNER ROCCO   STATUS: ON SCHEDULE",
      "> RUNNER NOVA    STATUS: OFF SCRIPT (%d)",
      "> CAMERA %02d     REC",
      "> SPONSOR FEED   HALCYON ARMS   %d BUYERS",
      "> DEMO UNIT %04X  TARGET ACQUIRED",
      "> RATINGS        +%d.%d%%",
      "  0x%04X  4F 42 45 59  20 53 43 52  49 50 54",
      "> OBEY SCRIPT",
      "> QUERY: WHY",
      "> QUERY: WHY   ... DENIED",
      "> ARENA RACK %02d   ONLINE",
      "> CUE LANCE      T-%03d",
      "> SET B          STANDING BY",
      "> PLEASE BE QUICK",
    };
    for (int i = 0; i < 70; ++i)
    {
      const unsigned h = hash2(i, 21);
      char buf[96];
      const char* fmt = kLines[h % 16u];
      std::snprintf(buf, sizeof(buf), fmt, int(h / 16u % 99u), int(h / 7u % 9u));
      cairo_move_to(cr, 24 + double(h / 3u % 4u) * 6, 22 + i * 21);
      setRgbaC(cr, i % 9 == 4 ? rgb(200, 255, 210) : rgb(70, 230, 110), 0.55 + 0.45 * double(h % 5u) / 4.0);
      cairo_show_text(cr, buf);
      // The same column again on the right, offset.
      std::snprintf(buf, sizeof(buf), kLines[(h / 5u) % 16u], int(h % 97u), int(h / 11u % 9u));
      cairo_move_to(cr, 840 + double(h / 13u % 5u) * 6, 22 + i * 21);
      cairo_show_text(cr, buf);
    }
  });
}

// Shot: ZERO's red eye fills the frame over its scrolling terminal; the iris
// contracts and dilates on an 8-frame loop and a faint zoom ring turns.
void brief21Eye(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  k.r.fillRect(0, 0, W, H, rgb(0, 4, 2));
  DrawOpts to;
  to.alpha = 0.6f;
  to.cull = false;
  k.r.draw(terminalText(k), ox, -float(frame) * 2.0f - 30.0f + oy, to);
  // A red bloom round the eye, breathing.
  const float breathe = 0.5f + 0.5f * std::sin(float(ticks) * 0.06f);
  drawGlow(k.r, k.art, 640 + ox, 360 + oy, 560.0f, kZeroRed, 0.45f + 0.15f * breathe);
  // The iris: contracted on frame 0, wide on frame 4.
  const float ph = float(frame % 8) / 8.0f;
  const float pupil = 0.15f + 0.30f * (0.5f - 0.5f * std::cos(ph * 2.0f * float(kPi)));
  DrawOpts eo;
  eo.cull = false;
  k.r.draw(zeroEye(k, 760, pupil, 0.0f, 1.0f), 640 + ox, 360 + oy, eo);
  DrawOpts ro;
  ro.cull = false;
  ro.alpha = 0.22f;
  ro.blend = Blend::Add;
  ro.angle = float(frame) * 2.5f + float(ticks) * 0.05f;
  k.r.draw(zoomRing(k), 640 + ox, 360 + oy, ro);
  scanlines(k, 70);
}

// A row of ZERO's racks across a dark red server hall (zero_fade).
const Texture& serverHall(ClipKit& k)
{
  return cached(k, "e3_server_hall", 1280, 720, 0, 0, [](cairo_t* cr) {
    gradient(cr, 1280, 720, rgb(6, 2, 6), rgb(30, 6, 12), rgb(10, 4, 8));
    for (int i = 0; i < 9; ++i)
    {
      const double x = -60 + i * 160, w = 150, h = 420 + (i % 2) * 40;
      cairo_save(cr);
      cairo_translate(cr, x, 600 - h);
      paintServerRack(cr, w, h, unsigned(i * 31 + 7), true);
      cairo_restore(cr);
    }
    // The floor: grating.
    cairo_rectangle(cr, 0, 600, 1280, 120);
    setRgbaC(cr, rgb(20, 20, 26), 1.0);
    cairo_fill(cr);
    for (int x = 0; x < 1280; x += 24)
    {
      cairo_move_to(cr, x, 600);
      cairo_line_to(cr, x - 40, 720);
    }
    setRgbaC(cr, rgb(50, 50, 62), 1.0);
    cairo_set_line_width(cr, 2);
    cairo_stroke(cr);
  });
}

// Shot: the eye dims from red to black (10 frames), the hall with it.
void zeroFade(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  const float u = std::clamp(float(frame) / 9.0f, 0.0f, 1.0f);
  k.r.draw(serverHall(k), ox, oy);
  // The light of the hall goes with the eye.
  k.r.fillRect(0, 0, W, H, rgba(0, 0, 0, int(80.0f + 165.0f * u)));
  const float flicker = frame >= 6 && (ticks / 3) % 3 == 0 ? 0.6f : 1.0f;
  const float light = (1.0f - u) * flicker;
  drawGlow(k.r, k.art, 640 + ox, 290 + oy, 420.0f, kZeroRed, 0.55f * light);
  DrawOpts eo;
  eo.cull = false;
  k.r.draw(zeroEye(k, 520, 0.3f + 0.12f * u, 0.0f, light), 640 + ox, 290 + oy, eo);
  if (frame == 9)
    drawGlow(k.r, k.art, 640 + ox, 290 + oy, 24.0f, kZeroRed, 0.25f + 0.15f * std::sin(float(ticks) * 0.1f));
}

// The studio behind the set: curtains, a lighting truss, the neon
// GUNRUNNERS sign (lit or dark).
void paintStudioSet(cairo_t* cr, bool lit)
{
  gradient(cr, 1280, 720, rgb(14, 8, 30), rgb(34, 16, 60), rgb(20, 10, 36));
  // The curtain's folds.
  for (int x = 0; x < 1280; x += 40)
  {
    cairo_pattern_t* g = cairo_pattern_create_linear(x, 0, x + 40, 0);
    cairo_pattern_add_color_stop_rgba(g, 0.0, 0.0, 0.0, 0.0, 0.35);
    cairo_pattern_add_color_stop_rgba(g, 0.5, 0.55, 0.3, 0.8, 0.12);
    cairo_pattern_add_color_stop_rgba(g, 1.0, 0.0, 0.0, 0.0, 0.35);
    cairo_rectangle(cr, x, 90, 40, 470);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  // The truss across the top.
  cairo_rectangle(cr, 0, 40, 1280, 40);
  setRgbaC(cr, rgb(30, 30, 40), 1.0);
  cairo_fill(cr);
  for (int x = 0; x < 1280; x += 40)
  {
    cairo_move_to(cr, x, 40);
    cairo_line_to(cr, x + 40, 80);
    cairo_move_to(cr, x + 40, 40);
    cairo_line_to(cr, x, 80);
  }
  setRgbaC(cr, rgb(90, 90, 110), 1.0);
  cairo_set_line_width(cr, 3);
  cairo_stroke(cr);
  // The sign.
  selectGameFont(cr);
  cairo_set_font_size(cr, 84);
  cairo_text_extents_t te;
  cairo_text_extents(cr, "GUNRUNNERS", &te);
  const double tx = 640 - te.width / 2 - te.x_bearing, ty = 230;
  if (lit)
  {
    for (int i = 4; i >= 1; --i)
    {
      cairo_move_to(cr, tx, ty);
      cairo_text_path(cr, "GUNRUNNERS");
      setRgbaC(cr, rgb(255, 60, 180), 0.12);
      cairo_set_line_width(cr, i * 7);
      cairo_stroke(cr);
    }
  }
  cairo_move_to(cr, tx, ty);
  cairo_text_path(cr, "GUNRUNNERS");
  setRgbaC(cr, lit ? rgb(255, 210, 240) : rgb(60, 40, 70), 1.0);
  cairo_set_line_width(cr, 4);
  cairo_stroke(cr);
  // The stage floor, glossy black, a strip of bulbs along its edge.
  cairo_pattern_t* f = cairo_pattern_create_linear(0, 560, 0, 720);
  cairo_pattern_add_color_stop_rgb(f, 0.0, 0.18, 0.12, 0.26);
  cairo_pattern_add_color_stop_rgb(f, 1.0, 0.04, 0.03, 0.06);
  cairo_rectangle(cr, 0, 560, 1280, 160);
  cairo_set_source(cr, f);
  cairo_fill(cr);
  cairo_pattern_destroy(f);
  for (int x = 20; x < 1280; x += 48)
  {
    cairo_arc(cr, x, 566, 5, 0, 2 * kPi);
    setRgbaC(cr, lit ? rgb(255, 230, 160) : rgb(60, 50, 40), 1.0);
    cairo_fill(cr);
  }
}

const Texture& studioSet(ClipKit& k, bool lit)
{
  return cached(k, lit ? "e3_studio_lit" : "e3_studio_dark", 1280, 720, 0, 0, [lit](cairo_t* cr) { paintStudioSet(cr, lit); });
}

// A cone of stage light from (x, y) to the floor, added on.
const Texture& lightCone(ClipKit& k)
{
  return cached(k, "e3_cone", 400, 640, 200, 0, [](cairo_t* cr) {
    cairo_move_to(cr, 180, 0);
    cairo_line_to(cr, 220, 0);
    cairo_line_to(cr, 400, 640);
    cairo_line_to(cr, 0, 640);
    cairo_close_path(cr);
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 0, 0, 640);
    cairo_pattern_add_color_stop_rgba(g, 0.0, 1.0, 0.96, 0.85, 0.55);
    cairo_pattern_add_color_stop_rgba(g, 1.0, 1.0, 0.96, 0.85, 0.05);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  });
}

// A can light: black housing, a lens that is lit or not.
const Texture& lampCan(ClipKit& k, bool lit)
{
  return cached(k, lit ? "e3_lamp_on" : "e3_lamp_off", 60, 70, 30, 10, [lit](cairo_t* cr) {
    roundedRect(cr, 8, 4, 44, 50, 6);
    setRgbaC(cr, rgb(24, 24, 30), 1.0);
    cairo_fill_preserve(cr);
    setRgbaC(cr, kInk, 1.0);
    cairo_set_line_width(cr, 3);
    cairo_stroke(cr);
    cairo_rectangle(cr, 26, 0, 8, 6);
    cairo_fill(cr);
    cairo_arc(cr, 30, 56, 16, 0, 2 * kPi);
    setRgbaC(cr, lit ? rgb(255, 250, 220) : rgb(60, 56, 50), 1.0);
    cairo_fill(cr);
  });
}

// The stage lights: bank b (0 left, 1 middle, 2 right) is on when on[b].
void stageLights(ClipKit& k, const bool on[3], int flashBank, float ox, float oy)
{
  for (int b = 0; b < 3; ++b)
    for (int i = 0; i < 3; ++i)
    {
      const float x = 160.0f + float(b) * 400.0f + float(i) * 80.0f + ox, y = 62.0f + oy;
      if (on[b])
      {
        DrawOpts co;
        co.blend = Blend::Add;
        co.alpha = 0.28f;
        co.cull = false;
        co.angle = (float(b) - 1.0f) * -14.0f + (float(i) - 1.0f) * 4.0f;
        k.r.draw(lightCone(k), x, y + 50.0f, co);
      }
      k.r.draw(lampCan(k, on[b]), x, y);
      if (on[b])
        drawGlow(k.r, k.art, x, y + 56.0f, b == flashBank ? 120.0f : 50.0f, rgb(255, 245, 210), b == flashBank ? 1.0f : 0.7f);
    }
}

// The painted flat: ZERO's back wall, racks and the dark eye, 1280 x 560.
const Texture& setWall(ClipKit& k)
{
  return cached(k, "e3_set_wall", 1280, 560, 0, 0, [](cairo_t* cr) {
    gradient(cr, 1280, 560, rgb(16, 6, 10), rgb(30, 8, 14), rgb(12, 4, 8));
    for (int i = 0; i < 10; ++i)
    {
      cairo_save(cr);
      cairo_translate(cr, i * 128 + 4, 120);
      paintServerRack(cr, 120, 440, unsigned(i * 13 + 1), i % 3 != 1);
      cairo_restore(cr);
    }
    paintZeroEye(cr, 640, 130, 120, 0.4, 0.0, 0.0, 0.0);
    // The frame of the flat: a timber edge.
    cairo_rectangle(cr, 3, 3, 1274, 554);
    setRgbaC(cr, rgb(70, 50, 34), 1.0);
    cairo_set_line_width(cr, 6);
    cairo_stroke(cr);
  });
}

const Texture& dustPuff(ClipKit& k)
{
  return cached(k, "e3_puff", 128, 128, 64, 64, [](cairo_t* cr) {
    cairo_pattern_t* g = cairo_pattern_create_radial(64, 64, 4, 64, 64, 62);
    cairo_pattern_add_color_stop_rgba(g, 0.0, 0.75, 0.72, 0.7, 0.8);
    cairo_pattern_add_color_stop_rgba(g, 1.0, 0.75, 0.72, 0.7, 0.0);
    cairo_arc(cr, 64, 64, 62, 0, 2 * kPi);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  });
}

// The arena's grille floor, from y down.
void arenaFloor(ClipKit& k, float y, float ox, float oy)
{
  k.r.fillRect(0, y + oy, W, H - y, rgb(22, 22, 28));
  k.r.fillRect(0, y + oy, W, 6, rgb(80, 82, 96));
  for (float x = -40.0f; x < W + 40.0f; x += 26.0f)
    k.r.drawLine(x + ox, y + 6 + oy, x - 50.0f + ox, H + oy, 2.0f, rgb(46, 46, 58));
}

// Shot: a crack runs up the wall (frames 0-5), the wall tips backwards and
// lands flat (6-13), dust (14-19). Behind it: a dark studio.
void wallFalls(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  const float floorY = 560.0f;
  k.r.draw(studioSet(k, false), ox, oy);
  k.r.fillRect(0, 0, W, H, rgba(0, 0, 0, 150));
  // The crack's path, up the wall from the floor.
  float cx[13], cy[13];
  for (int i = 0; i < 13; ++i)
  {
    cy[i] = 560.0f - float(i) * 560.0f / 12.0f;
    cx[i] = 640.0f + (i == 0 || i == 12 ? 0.0f : float(int(hash2(i, 77) % 120u) - 60));
  }
  const float tip = frame < 6 ? 0.0f : std::min(1.0f, float(frame - 5) / 8.0f);
  const float squash = std::cos(tip * tip * float(kPi) * 0.5f);
  const float wallH = 560.0f * squash;
  if (wallH > 2.0f)
  {
    const Color shade = lerpColor(rgb(255, 255, 255), rgb(150, 140, 150), tip);
    stretchTex(k, setWall(k), ox, floorY - wallH + oy, W, wallH, shade, 1.0f);
    // The crack: as far up as it has run (frames 0-5), squashed with the wall.
    const int upTo = frame < 6 ? 2 * (frame + 1) : 12;
    for (int i = 0; i < upTo && i < 12; ++i)
    {
      const float y0 = floorY - (560.0f - cy[i]) * squash, y1 = floorY - (560.0f - cy[i + 1]) * squash;
      k.r.drawLine(cx[i] + ox, y0 + oy, cx[i + 1] + ox, y1 + oy, 7.0f, rgb(4, 2, 4));
      k.r.drawLine(cx[i] + ox, y0 + oy, cx[i + 1] + ox, y1 + oy, 2.0f, rgba(255, 240, 220, 200), Blend::Add);
    }
    if (frame < 6)
    {
      // Light leaking from the studio through the crack's tip.
      const int tipI = std::min(12, upTo);
      drawGlow(k.r, k.art, cx[tipI] + ox, cy[tipI] + oy, 70.0f, rgb(255, 230, 200), 0.6f);
    }
  }
  else
    k.r.fillRect(ox, floorY - 6 + oy, W, 6, rgb(60, 40, 30));
  arenaFloor(k, floorY, ox, oy);
  // The crew, small in front, looking up at it; they flinch as it lands.
  const int pose = frame < 6 ? 3 : (frame >= 13 && frame <= 15 ? 5 : 0);
  k.r.draw(runner(k, 0, pose, 2.4f), 420 + ox, 690 + oy);
  k.r.draw(runner(k, 1, pose, 2.4f), 640 + ox, 700 + oy);
  k.r.draw(runner(k, 2, pose, 2.4f, true), 860 + ox, 690 + oy);
  if (frame >= 13)
  {
    // Dust rolling out along the floor.
    const float d = float(frame - 13) / 6.0f;
    for (int i = 0; i < 16; ++i)
    {
      DrawOpts po;
      po.scale = 0.8f + 2.2f * d + float(hash2(i, 5) % 10u) * 0.05f;
      po.alpha = 0.7f * (1.0f - d * 0.8f);
      po.cull = false;
      const float x = float(i) * 85.0f + float(hash2(i, 9) % 40u) + (float(i) - 7.5f) * 30.0f * d;
      k.r.draw(dustPuff(k), x + ox, floorY - 20.0f - 50.0f * d * float(hash2(i, 3) % 3u) + oy, po);
    }
  }
  (void)ticks;
}

// Shot: the studio dark, then three banks of lights snap on, one every four
// frames; the neon sign last.
void studioLights(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  const int banks = std::min(3, frame / 4 + 1);
  k.r.draw(studioSet(k, banks >= 3), ox, oy);
  k.r.fillRect(0, 0, W, H, rgba(0, 0, 0, 220 - banks * 60));
  // The crew mid stage, blinking into the light.
  const int pose = frame % 4 == 0 ? 4 : 0;
  k.r.draw(runner(k, 0, pose, 2.0f), 500 + ox, 640 + oy);
  k.r.draw(runner(k, 1, pose, 2.0f), 640 + ox, 650 + oy);
  k.r.draw(runner(k, 2, pose, 2.0f, true), 780 + ox, 640 + oy);
  const bool on[3] = {banks >= 1, banks >= 2, banks >= 3};
  stageLights(k, on, frame % 4 == 0 ? frame / 4 : -1, ox, oy);
  if (frame % 4 == 0)
    k.r.fillRect(0, 0, W, H, rgba(255, 250, 230, 40), Blend::Add);
  (void)ticks;
}

// The audience, a crowd baked per frame: four tiers, nearer ones larger.
// Somewhere in it: Black Halo's pilot and the spare host from level 16.
const Texture& crowd(ClipKit& k, int frame)
{
  return cached(k, "e3_crowd" + std::to_string(frame), 1280, 720, 0, 0, [frame](cairo_t* cr) {
    gradient(cr, 1280, 720, rgb(10, 6, 20), rgb(26, 14, 44), rgb(16, 8, 28));
    // An APPLAUSE sign up on the back wall.
    roundedRect(cr, 500, 18, 280, 60, 10);
    setRgbaC(cr, rgb(30, 10, 14), 1.0);
    cairo_fill_preserve(cr);
    setRgbaC(cr, rgb(90, 30, 40), 1.0);
    cairo_set_line_width(cr, 4);
    cairo_stroke(cr);
    for (int row = 0; row < 4; ++row)
    {
      const double s = 44 + row * 18, y = 170 + row * 140;
      // The tier's bench.
      cairo_rectangle(cr, 0, y + s * 0.9, 1280, 26 + row * 6);
      setRgbaC(cr, lerpColor(rgb(40, 20, 60), rgb(70, 30, 90), float(row) / 3.0f), 1.0);
      cairo_fill(cr);
      const double step = s * 1.75;
      const int n = int(1280 / step) + 2;
      for (int i = 0; i < n; ++i)
      {
        const unsigned seed = hash2(i + row * 37, 8);
        int kind = 0;
        if (row == 1 && i == 5)
          kind = 1;
        if (row == 2 && i == 7)
          kind = 2;
        const double x = (row % 2) * step * 0.5 + i * step - step * 0.3;
        paintAudienceMember(cr, x, y + s * 0.9, s, kind, int((unsigned(frame) + seed) % 6u), seed);
      }
    }
  });
}

// Shot: the reverse angle, the studio audience cheering (6-frame loop).
void audience(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  k.r.draw(crowd(k, frame % 6), ox * 0.6f, oy * 0.6f);
  const bool lit = (ticks / 15) % 2 == 0;
  k.r.drawText("APPLAUSE", 640 + ox, 26 + oy, {38.0f, lit ? rgb(255, 80, 90) : rgb(110, 40, 46), kInk, false}, Align::Center);
  if (lit)
    drawGlow(k.r, k.art, 640 + ox, 48 + oy, 200.0f, rgb(255, 60, 70), 0.35f);
  // Camera flashes in the crowd.
  for (int i = 0; i < 3; ++i)
  {
    const unsigned h = hash2(frame * 3 + i, ticks / 6);
    if (h % 3u == 0)
      drawGlow(k.r, k.art, float(h % 1280u) + ox, 150.0f + float((h / 1280u) % 500u) + oy, 50.0f, rgb(255, 255, 255), 0.8f);
  }
  // Stage light washing over them.
  k.r.fillRect(0, 0, W, H, rgba(255, 220, 160, 18), Blend::Add);
}

// Lance at `scale` x his design size (feet at the anchor), facing right or
// mirrored.
const Texture& lance(ClipKit& k, int pose, bool mouth, bool mirror, float scale)
{
  const int w = int(240.0f * scale), h = int(500.0f * scale);
  const std::string key = "e3_lance" + std::to_string(pose) + (mouth ? "o" : "c") + (mirror ? "m" : "") +
    std::to_string(int(scale * 100.0f));
  return cached(k, key, w, h, float(w) * 0.5f, 480.0f * scale, [&](cairo_t* cr) {
    if (mirror)
    {
      cairo_translate(cr, w, 0);
      cairo_scale(cr, -scale, scale);
    }
    else
      cairo_scale(cr, scale, scale);
    paintLance(cr, pose, mouth);
  });
}

// Shot: Lance Marquee strides in from the right (frames 0-7) and throws his
// arms wide (8); a spotlight follows him, confetti on 8.
void lanceEntrance(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  k.r.draw(studioSet(k, true), ox, oy);
  k.r.fillRect(0, 0, W, H, rgba(0, 0, 0, 70));
  const bool on[3] = {true, true, true};
  stageLights(k, on, -1, ox, oy);
  const float lx = frame >= 8 ? 820.0f : 1420.0f - float(frame) / 7.0f * 560.0f;
  drawGlow(k.r, k.art, lx + ox, 520 + oy, 220.0f, rgb(255, 245, 220), 0.4f);
  k.r.fillRect(lx - 120 + ox, 640 + oy, 240, 30, rgba(255, 240, 200, 50), Blend::Add);
  // The crew stage left, turned to him.
  const int crewPose = frame >= 8 ? 5 : 0;
  k.r.draw(runner(k, 0, crewPose, 3.0f), 240 + ox, 664 + oy);
  k.r.draw(runner(k, 1, crewPose, 3.0f), 420 + ox, 670 + oy);
  k.r.draw(runner(k, 2, crewPose, 3.0f), 600 + ox, 664 + oy);
  // He walks in facing left (his art faces right), then opens his arms.
  const int pose = frame >= 8 ? 8 : frame;
  k.r.draw(lance(k, pose, frame >= 8, true, 0.72f), lx + ox, 664 + oy);
  if (frame >= 8)
    for (int i = 0; i < 60; ++i)
    {
      const unsigned h = hash2(i, 61);
      const float x = float(h % 1280u) + std::sin(float(ticks + i * 9) * 0.05f) * 20.0f;
      const float y = std::fmod(float(h / 1280u % 720u) + float(ticks) * (1.5f + float(h % 7u) * 0.3f), 760.0f) - 40.0f;
      static const Color kConfetti[] = {rgb(255, 80, 120), rgb(255, 220, 60), rgb(80, 220, 255), rgb(140, 255, 120)};
      k.r.fillRect(x + ox, y + oy, 8, 5, kConfetti[i % 4]);
    }
}

// One candid camera shot, in a monitor at (x, y) w x h: level n's colours,
// a runner caught mid move, REC and the level number.
void candidShot(ClipKit& k, int n, float x, float y, float w, float h, int ticks)
{
  static const Color kSky[20][2] = {
    {rgb(30, 10, 60), rgb(255, 80, 160)},  {rgb(20, 30, 80), rgb(255, 160, 80)},  {rgb(40, 20, 20), rgb(255, 120, 40)},
    {rgb(10, 30, 40), rgb(80, 200, 220)},  {rgb(20, 30, 20), rgb(160, 220, 80)},  {rgb(10, 10, 40), rgb(120, 160, 255)},
    {rgb(10, 20, 50), rgb(200, 220, 255)}, {rgb(10, 40, 20), rgb(120, 220, 100)}, {rgb(50, 40, 20), rgb(240, 200, 100)},
    {rgb(60, 40, 10), rgb(255, 220, 120)}, {rgb(30, 20, 14), rgb(200, 140, 80)},  {rgb(40, 8, 4), rgb(255, 100, 30)},
    {rgb(40, 30, 10), rgb(255, 200, 60)},  {rgb(50, 40, 20), rgb(220, 180, 120)}, {rgb(4, 4, 20), rgb(140, 160, 220)},
    {rgb(10, 20, 40), rgb(170, 230, 255)}, {rgb(8, 30, 12), rgb(140, 255, 120)},  {rgb(10, 10, 20), rgb(120, 140, 170)},
    {rgb(16, 16, 30), rgb(230, 230, 255)}, {rgb(6, 12, 30), rgb(90, 180, 255)},
  };
  const auto& c = kSky[(n - 1) % 20];
  for (int i = 0; i < 8; ++i)
    k.r.fillRect(x, y + h * float(i) / 8.0f, w, h / 8.0f + 1.0f, lerpColor(c[0], c[1], float(i) / 9.0f));
  k.r.fillRect(x, y + h * 0.78f, w, h * 0.22f, darken(c[0], 0.4f));
  static const int kPoses[] = {1, 2, 3, 4, 6, 7, 1, 2};
  const unsigned hh = hash2(n, 7);
  clipTo(k, x, y, w, h);
  k.r.draw(runner(k, int(hh % 3u), kPoses[hh / 3u % 8u], 0.9f, hh % 2u == 1), x + w * (0.35f + 0.3f * float(hh % 5u) / 4.0f), y + h * 0.84f);
  unclip(k);
  // A wide lens's corners and REC.
  k.r.fillRect(x, y, w, h, rgba(0, 0, 0, 30));
  if ((ticks / 20) % 2 == 0)
    k.r.fillRect(x + 8, y + 8, 8, 8, rgb(255, 30, 30));
  k.r.drawText("REC", x + 20, y + 4, {13.0f, rgb(255, 255, 255), kInk, false});
  char buf[16];
  std::snprintf(buf, sizeof(buf), "L%02d", n);
  k.r.drawText(buf, x + w - 8, y + h - 20, {13.0f, rgb(255, 255, 255), kInk, false}, Align::Right);
}

// Shot: a wall of twenty monitors lighting one a frame; each shows the
// candid camera shot from its level if the player found that camera,
// static if not.
void monitorWall(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  k.r.fillRect(0, 0, W, H, rgb(10, 8, 16));
  for (int i = 0; i < 20; ++i)
  {
    const int col = i % 5, row = i / 5;
    const float x = 48.0f + float(col) * 240.0f + ox, y = 34.0f + float(row) * 168.0f + oy;
    const float w = 218.0f, h = 140.0f;
    // The bezel.
    k.r.fillRect(x - 10, y - 10, w + 20, h + 20, rgb(36, 34, 44));
    k.r.fillRect(x - 10, y + h + 4, w + 20, 6, rgb(24, 22, 30));
    k.r.fillRect(x - 3, y - 3, w + 6, h + 6, kInk);
    const bool lit = i <= frame;
    if (!lit)
    {
      k.r.fillRect(x, y, w, h, rgb(14, 16, 20));
      k.r.fillRect(x + 10, y + 8, w * 0.4f, 6, rgba(255, 255, 255, 14));
      continue;
    }
    if (k.cameras.count(i + 1))
      candidShot(k, i + 1, x, y, w, h, ticks);
    else
    {
      staticNoise(k, ticks / 2 + i * 17, 0.9f, x, y, w, h);
      k.r.drawText("NO SIGNAL", x + w / 2, y + h / 2 - 10, {16.0f, rgb(255, 255, 255), kInk, false}, Align::Center);
    }
    scanlines(k, 50, y, y + h, x, w);
    // The one that just came on flares.
    if (i == frame)
      k.r.fillRect(x, y, w, h, rgba(255, 255, 255, 90), Blend::Add);
    drawGlow(k.r, k.art, x + w / 2, y + h / 2, 150.0f, rgb(160, 190, 255), 0.08f);
  }
}

// A studio TV camera on its pedestal, aimed left, with the Episode 1 lens
// in its barrel. The lens's centre is at (100, 170) of the 520 x 600 box.
void paintTvCamera(cairo_t* cr)
{
  // The pedestal and its wheels.
  cairo_rectangle(cr, 300, 300, 34, 240);
  setRgbaC(cr, rgb(50, 52, 62), 1.0);
  cairo_fill(cr);
  for (const double x : {200.0, 317.0, 434.0})
  {
    cairo_move_to(cr, 317, 520);
    cairo_line_to(cr, x, 570);
    setRgbaC(cr, rgb(50, 52, 62), 1.0);
    cairo_set_line_width(cr, 14);
    cairo_stroke(cr);
    cairo_arc(cr, x, 576, 16, 0, 2 * kPi);
    setRgbaC(cr, rgb(16, 16, 20), 1.0);
    cairo_fill(cr);
  }
  // The body.
  roundedRect(cr, 170, 80, 300, 210, 18);
  cairo_pattern_t* g = cairo_pattern_create_linear(0, 80, 0, 290);
  cairo_pattern_add_color_stop_rgb(g, 0.0, 0.45, 0.46, 0.52);
  cairo_pattern_add_color_stop_rgb(g, 1.0, 0.16, 0.16, 0.2);
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  setRgbaC(cr, kInk, 1.0);
  cairo_set_line_width(cr, 5);
  cairo_stroke(cr);
  selectGameFont(cr);
  cairo_set_font_size(cr, 34);
  cairo_move_to(cr, 250, 200);
  setRgbaC(cr, rgb(230, 230, 240), 1.0);
  cairo_show_text(cr, "MAXTV");
  // The tally light on top, and the viewfinder hood at the back.
  roundedRect(cr, 400, 50, 60, 34, 6);
  setRgbaC(cr, rgb(30, 30, 36), 1.0);
  cairo_fill(cr);
  cairo_arc(cr, 200, 70, 14, 0, 2 * kPi);
  setRgbaC(cr, rgb(255, 40, 40), 1.0);
  cairo_fill(cr);
  // The barrel, and the lens at its front.
  cairo_rectangle(cr, 100, 96, 90, 148);
  setRgbaC(cr, rgb(22, 22, 28), 1.0);
  cairo_fill(cr);
  cairo_save(cr);
  cairo_translate(cr, 100 - 70, 170 - 70);
  cairo_scale(cr, 140.0 / 260.0, 140.0 / 260.0);
  paintLensGlass(cr);
  cairo_restore(cr);
}

// Shot: Rocco holds the lens from Episode 1 up beside a studio camera's;
// on frames 4-5 they line up, and they match.
void roccoLens(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  k.r.draw(studioSet(k, true), ox * 0.5f, oy * 0.5f);
  k.r.fillRect(0, 0, W, H, rgba(10, 4, 20, 140));
  const Texture& cam = cached(k, "e3_tv_camera", 520, 600, 0, 0, paintTvCamera);
  k.r.draw(cam, 700 + ox, 120 + oy);
  // The camera's lens is at (800, 290); Rocco's comes up beside it.
  k.r.draw(runner(k, 1, 0, 5.0f), 330 + ox, 720 + oy);
  const float u = std::min(1.0f, float(frame) / 4.0f);
  const float lx = 500.0f + (620.0f - 500.0f) * u, ly = 520.0f + (290.0f - 520.0f) * u;
  DrawOpts lo;
  lo.scale = 140.0f / 260.0f;
  lo.angle = (1.0f - u) * -25.0f;
  k.r.draw(cached(k, "e1_lens", 260, 260, 130, 130, paintLensGlass), lx + ox, ly + oy, lo);
  // His arm up to it, the hand round its barrel.
  const float sx = 392.0f + ox, sy = 400.0f + oy, hx = lx + ox, hy = ly + 62.0f + oy;
  k.r.drawLine(sx, sy, hx, hy, 32.0f, kInk);
  k.r.drawLine(sx, sy, hx, hy, 22.0f, rgb(150, 100, 64));
  k.r.fillRect(hx - 18, hy - 22, 36, 36, kInk);
  k.r.fillRect(hx - 14, hy - 18, 28, 28, rgb(160, 108, 70));
  if (frame >= 4)
  {
    const float g = frame == 4 ? 1.0f : 0.6f + 0.2f * std::sin(float(ticks) * 0.2f);
    drawGlow(k.r, k.art, lx - 10 + ox, ly - 14 + oy, 50.0f, rgb(220, 180, 255), g);
    drawGlow(k.r, k.art, 790 + ox, 276 + oy, 50.0f, rgb(220, 180, 255), g);
  }
}

// Shot: Lance close up with his microphone (frame 0 mouth closed, 1 open),
// the sponsor's banner behind him.
void lanceClose(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  const Texture& bg = cached(k, "e3_bokeh", 1280, 720, 0, 0, [](cairo_t* cr) {
    gradient(cr, 1280, 720, rgb(20, 10, 40), rgb(40, 18, 64), rgb(14, 8, 28));
    for (int i = 0; i < 40; ++i)
    {
      const unsigned h = hash2(i, 33);
      const double x = h % 1280u, y = (h / 1280u) % 720u, r = 20 + h % 40u;
      static const Color kBokeh[] = {rgb(255, 120, 200), rgb(255, 220, 120), rgb(120, 200, 255)};
      cairo_arc(cr, x, y, r, 0, 2 * kPi);
      setRgbaC(cr, kBokeh[i % 3], 0.18);
      cairo_fill(cr);
    }
    // The sponsor's banner.
    cairo_rectangle(cr, 40, 120, 520, 120);
    setRgbaC(cr, rgb(20, 20, 26), 0.9);
    cairo_fill(cr);
    selectGameFont(cr);
    cairo_set_font_size(cr, 58);
    cairo_move_to(cr, 70, 202);
    setRgbaC(cr, rgb(230, 190, 80), 1.0);
    cairo_show_text(cr, "HALCYON ARMS");
    cairo_set_font_size(cr, 20);
    cairo_move_to(cr, 72, 230);
    setRgbaC(cr, rgb(200, 200, 210), 1.0);
    cairo_show_text(cr, "PROUD SPONSOR OF GUNRUNNERS");
  });
  k.r.draw(bg, ox * 0.5f, oy * 0.5f);
  // His upper body, big: design (0..240, 20..300) at 2.6x.
  const bool mouth = frame % 2 == 1;
  const Texture& body = cached(k, std::string("e3_lance_close") + (mouth ? "o" : "c"), 1280, 720, 640, 0, [mouth](cairo_t* cr) {
    cairo_translate(cr, 640 - 120 * 2.6, -20 * 2.6);
    cairo_scale(cr, 2.6, 2.6);
    paintLance(cr, 8, mouth);
  });
  drawGlow(k.r, k.art, 800 + ox, 300 + oy, 420.0f, rgb(255, 240, 220), 0.25f);
  k.r.draw(body, 800 + ox, 10 + oy + std::sin(float(ticks) * 0.08f) * 3.0f);
}

// Shot: Dash close up, frozen mid thought: a still frame, cold, with a
// tape's tracking band rolling through it. (end_e5 uses it too, so the
// backdrop is the level's own.)
void dashFreeze(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  (void)frame;
  drawBackdrop(k.r, k.art, ox, oy, 0.0f);
  k.r.fillRect(0, 0, W, H, rgba(0, 0, 0, 120));
  k.r.draw(runner(k, 0, 0, 5.0f), 640 + ox, 860 + oy);
  k.r.fillRect(0, 0, W, H, rgba(60, 90, 140, 60));
  const float band = std::fmod(float(ticks) * 1.5f, H + 40.0f) - 20.0f;
  k.r.fillRect(0, band, W, 16, rgba(255, 255, 255, 30));
  staticNoise(k, ticks / 4, 0.2f, 0, band + 4.0f, W, 8);
  scanlines(k, 50);
}

// Shot: the stage floor under the crew drops open (frames 0-5), they fall
// into black (6-9), and a channel number comes up: CH 7 (10-13).
void trapdoorClip(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  const float stageY = 540.0f, trapX0 = 300.0f, trapX1 = 900.0f;
  if (frame <= 5)
  {
    k.r.draw(studioSet(k, true), ox, oy);
    k.r.fillRect(0, 0, W, H, rgba(0, 0, 0, 60));
    const float open = frame < 2 ? 0.0f : std::min(1.0f, float(frame - 1) / 4.0f);
    // The stage top, and the hole in it once it opens.
    k.r.fillRect(ox, stageY + oy, W, 30, rgb(60, 36, 70));
    if (open > 0.0f)
      k.r.fillRect(trapX0 + ox, stageY + oy, trapX1 - trapX0, 30, rgb(0, 0, 0));
    // The crew, dropping with the doors.
    const int pose = frame < 2 ? 0 : 5;
    const float drop = frame < 3 ? 0.0f : float(frame - 2) * float(frame - 2) * 30.0f;
    k.r.draw(runner(k, 0, pose, 2.2f), 460 + ox, stageY + 6 + drop + oy);
    k.r.draw(runner(k, 1, pose, 2.2f), 600 + ox, stageY + 10 + drop * 1.1f + oy);
    k.r.draw(runner(k, 2, pose, 2.2f, true), 740 + ox, stageY + 6 + drop * 0.9f + oy);
    // The two doors swinging down from their hinges.
    const Texture& leaf = cached(k, "e3_trap_leaf", 300, 24, 0, 0, [](cairo_t* cr) {
      cairo_rectangle(cr, 0, 0, 300, 24);
      setRgbaC(cr, rgb(84, 52, 96), 1.0);
      cairo_fill_preserve(cr);
      setRgbaC(cr, kInk, 1.0);
      cairo_set_line_width(cr, 4);
      cairo_stroke(cr);
    });
    DrawOpts lo;
    lo.cull = false;
    lo.angle = open * 88.0f;
    k.r.draw(leaf, trapX0 + ox, stageY + oy, lo);
    lo.angle = 180.0f - open * 88.0f;
    k.r.draw(leaf, trapX1 + ox, stageY + 24 + oy, lo);
    // The apron of the stage, in front of the hole.
    k.r.fillRect(ox, stageY + 30 + oy, W, H - stageY, rgb(30, 16, 40));
    for (float x = 20.0f; x < W; x += 48.0f)
      drawGlow(k.r, k.art, x + ox, stageY + 40 + oy, 10.0f, rgb(255, 230, 160), 0.8f);
    // Lance, stage right, past the trap.
    k.r.draw(lance(k, 9, frame < 2 && (ticks / 6) % 2 == 0, true, 0.5f), 1060 + ox, stageY + 8 + oy);
    return;
  }
  k.r.fillRect(0, 0, W, H, rgb(0, 0, 0));
  if (frame <= 9)
  {
    // Falling: the lit hole above shrinks away, the crew tumbles down.
    const float u = float(frame - 6) / 3.0f;
    const float hw = 600.0f * (1.0f - 0.8f * u), hh = 40.0f * (1.0f - 0.8f * u);
    k.r.fillRect(640 - hw / 2 + ox, 30 - hh / 2 + oy, hw, hh, rgba(255, 230, 200, int(255 * (1.0f - u * 0.7f))));
    drawGlow(k.r, k.art, 640 + ox, 30 + oy, hw * 0.6f, rgb(255, 220, 180), 0.4f * (1.0f - u));
    const float s = 2.4f - 1.6f * u;
    DrawOpts fo;
    fo.alpha = 1.0f - u * 0.7f;
    fo.angle = u * 40.0f;
    k.r.draw(runner(k, 0, 7, s), 520 + ox - 40 * u, 300 + 200 * u + oy, fo);
    fo.angle = -u * 30.0f;
    k.r.draw(runner(k, 1, 7, s), 640 + ox, 340 + 220 * u + oy, fo);
    fo.angle = u * 25.0f;
    k.r.draw(runner(k, 2, 7, s, true), 760 + ox + 40 * u, 290 + 210 * u + oy, fo);
    return;
  }
  // A TV's channel display in the dark.
  if (frame == 10)
    staticNoise(k, ticks, 0.3f);
  const Color osd = rgb(90, 255, 120);
  k.r.drawText("CH 7", W - 70, 50, {72.0f, osd, rgb(0, 40, 10), false}, Align::Right);
  if (frame >= 11 && (ticks / 15) % 2 == 0)
    k.r.fillRect(W - 66, 64, 26, 52, osd);
}

// --- Level 22 briefing: Channel 7, Westerns ----------------------------------

// One frame of TV snow, 640 x 360 (drawn at 2x), 4 frames a loop.
const Texture& snowFrame(ClipKit& k, int f)
{
  return cached(k, "e4_snow" + std::to_string(f), 640, 360, 0, 0, [f](cairo_t* cr) {
    cairo_surface_t* s = cairo_get_target(cr);
    cairo_surface_flush(s);
    unsigned char* px = cairo_image_surface_get_data(s);
    const int stride = cairo_image_surface_get_stride(s);
    std::uint32_t sd = 0x9E3779B9u * std::uint32_t(f + 1);
    for (int y = 0; y < 360; ++y)
    {
      // A weak line now and then, and the slow sag of the signal.
      const double row = 0.75 + 0.25 * std::sin(double(y + f * 37) * 0.045) - ((hash2(y / 3, f) % 9u) == 0 ? 0.3 : 0.0);
      for (int x = 0; x < 640; ++x)
      {
        sd ^= sd << 13;
        sd ^= sd >> 17;
        sd ^= sd << 5;
        const double v = std::clamp(double(sd & 0xFF) * row, 0.0, 255.0);
        unsigned char* p = px + y * stride + x * 4;
        p[0] = static_cast<unsigned char>(std::min(255.0, v * 1.04)); // B
        p[1] = static_cast<unsigned char>(v);                          // G
        p[2] = static_cast<unsigned char>(v * 0.97);                   // R
        p[3] = 255;
      }
    }
    cairo_surface_mark_dirty(s);
  });
}

// Shot: a dead channel. Four baked frames of snow (each pixel its own grey,
// with the darker bands a weak signal rolls through), a bright tear running
// down the screen, scanlines and a dark tube edge.
void staticSnow(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  (void)ox;
  (void)oy;
  const Texture& snow = snowFrame(k, frame % 4);
  DrawOpts o;
  o.scale = 2.0f;
  k.r.draw(snow, 0, 0, o);
  // The tear: a brighter band rolling down.
  const float band = std::fmod(float(ticks) * 2.5f, H + 120.0f) - 60.0f;
  k.r.fillRect(0, band, W, 26, rgba(255, 255, 255, 46), Blend::Add);
  k.r.fillRect(0, band + 30.0f, W, 10, rgba(0, 0, 0, 70));
  scanlines(k, 60);
  // The tube's dark rounded edge.
  const Texture& tube = cached(k, "e4_tube", 1280, 720, 0, 0, [](cairo_t* cr) {
    cairo_pattern_t* g = cairo_pattern_create_radial(640, 360, 300, 640, 360, 820);
    cairo_pattern_add_color_stop_rgba(g, 0.0, 0, 0, 0, 0.0);
    cairo_pattern_add_color_stop_rgba(g, 0.7, 0, 0, 0, 0.25);
    cairo_pattern_add_color_stop_rgba(g, 1.0, 0, 0, 0, 0.8);
    cairo_rectangle(cr, 0, 0, 1280, 720);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  });
  k.r.draw(tube, 0, 0);
}

// The sepia test card behind the Channel 7 ident: a target of rings and
// crosshairs, resolution fans, a step wedge and a sheriff's star.
const Texture& westTestCard(ClipKit& k)
{
  return cached(k, "e4_testcard", 1280, 720, 0, 0, [](cairo_t* cr) {
    const Color paper = rgb(232, 206, 158), ink = rgb(84, 54, 28), mid = rgb(170, 128, 76);
    cairo_rectangle(cr, 0, 0, 1280, 720);
    setColor(cr, paper);
    cairo_fill(cr);
    // The grid.
    for (int x = 0; x <= 1280; x += 80)
    {
      cairo_rectangle(cr, x - 1, 0, 2, 720);
      setRgbaC(cr, ink, 0.18);
      cairo_fill(cr);
    }
    for (int y = 40; y <= 720; y += 80)
    {
      cairo_rectangle(cr, 0, y - 1, 1280, 2);
      setRgbaC(cr, ink, 0.18);
      cairo_fill(cr);
    }
    // Corner targets.
    for (const double cx : {120.0, 1160.0})
      for (const double cy : {120.0, 600.0})
      {
        cairo_new_path(cr);
        cairo_arc(cr, cx, cy, 64, 0, 2 * kPi);
        setRgbaC(cr, mid, 0.5);
        cairo_fill_preserve(cr);
        setRgbaC(cr, ink, 1.0);
        cairo_set_line_width(cr, 4);
        cairo_stroke(cr);
        cairo_arc(cr, cx, cy, 32, 0, 2 * kPi);
        cairo_stroke(cr);
        cairo_move_to(cr, cx - 64, cy);
        cairo_line_to(cr, cx + 64, cy);
        cairo_move_to(cr, cx, cy - 64);
        cairo_line_to(cr, cx, cy + 64);
        cairo_set_line_width(cr, 2);
        cairo_stroke(cr);
      }
    // The big target: a dark ring, lighter disc inside, crosshairs out to
    // the edges.
    cairo_new_path(cr);
    cairo_arc(cr, 640, 360, 300, 0, 2 * kPi);
    setRgbaC(cr, rgb(244, 224, 184), 1.0);
    cairo_fill_preserve(cr);
    setRgbaC(cr, ink, 1.0);
    cairo_set_line_width(cr, 14);
    cairo_stroke(cr);
    cairo_arc(cr, 640, 360, 270, 0, 2 * kPi);
    cairo_set_line_width(cr, 3);
    cairo_stroke(cr);
    cairo_move_to(cr, 0, 360);
    cairo_line_to(cr, 1280, 360);
    cairo_move_to(cr, 640, 0);
    cairo_line_to(cr, 640, 720);
    setRgbaC(cr, ink, 0.6);
    cairo_set_line_width(cr, 2);
    cairo_stroke(cr);
    // Resolution fans either side.
    for (const int side : {-1, 1})
    {
      const double fx = 640 + side * 470.0, fy = 360;
      for (int i = 0; i < 9; ++i)
      {
        const double a = (double(i) - 4.0) * 0.07;
        cairo_move_to(cr, fx, fy);
        cairo_line_to(cr, fx - side * 150 * std::cos(a), fy + 150 * std::sin(a));
      }
      setRgbaC(cr, ink, 0.85);
      cairo_set_line_width(cr, 2.5);
      cairo_stroke(cr);
    }
    // The step wedge: sepia from paper to ink.
    for (int i = 0; i < 8; ++i)
    {
      cairo_rectangle(cr, 400 + i * 60, 640, 60, 50);
      setColor(cr, lerpColor(paper, rgb(40, 24, 10), float(i) / 7.0f));
      cairo_fill(cr);
    }
    cairo_rectangle(cr, 400, 640, 480, 50);
    setRgbaC(cr, ink, 1.0);
    cairo_set_line_width(cr, 3);
    cairo_stroke(cr);
    // The sheriff's star, a 7 on its boss.
    cairo_new_path(cr);
    for (int i = 0; i < 12; ++i)
    {
      const double a = -kPi / 2 + double(i) * kPi / 6.0;
      const double rr = i % 2 == 0 ? 96.0 : 48.0;
      if (i == 0)
        cairo_move_to(cr, 640 + std::cos(a) * rr, 372 + std::sin(a) * rr);
      else
        cairo_line_to(cr, 640 + std::cos(a) * rr, 372 + std::sin(a) * rr);
    }
    cairo_close_path(cr);
    cairo_pattern_t* g = cairo_pattern_create_linear(560, 280, 720, 460);
    cairo_pattern_add_color_stop_rgb(g, 0, 0.93, 0.80, 0.52);
    cairo_pattern_add_color_stop_rgb(g, 1, 0.55, 0.38, 0.18);
    cairo_set_source(cr, g);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(g);
    setRgbaC(cr, ink, 1.0);
    cairo_set_line_width(cr, 4);
    cairo_stroke(cr);
    for (int i = 0; i < 6; ++i)
    {
      const double a = -kPi / 2 + double(i) * kPi / 3.0;
      cairo_new_path(cr);
      cairo_arc(cr, 640 + std::cos(a) * 96, 372 + std::sin(a) * 96, 9, 0, 2 * kPi);
      setRgbaC(cr, rgb(214, 180, 120), 1.0);
      cairo_fill_preserve(cr);
      setRgbaC(cr, ink, 1.0);
      cairo_set_line_width(cr, 3);
      cairo_stroke(cr);
    }
    cairo_new_path(cr);
    cairo_arc(cr, 640, 372, 34, 0, 2 * kPi);
    setRgbaC(cr, rgb(246, 226, 180), 1.0);
    cairo_fill_preserve(cr);
    setRgbaC(cr, ink, 1.0);
    cairo_stroke(cr);
    selectGameFont(cr);
    cairo_set_font_size(cr, 46);
    cairo_text_extents_t te;
    cairo_text_extents(cr, "7", &te);
    cairo_move_to(cr, 640 - te.x_advance / 2, 372 + te.height / 2);
    cairo_show_text(cr, "7");
    // The station's line along the foot of the ring.
    cairo_set_font_size(cr, 18);
    const char* line = "MAXTV  WESTERN NETWORK";
    cairo_text_extents(cr, line, &te);
    cairo_move_to(cr, 640 - te.x_advance / 2, 612);
    setRgbaC(cr, ink, 0.9);
    cairo_show_text(cr, line);
  });
}

// "WESTERNS" in letters of rope: each glyph's outline laid in a twisted
// hemp line, with a lariat's loop for a tail.
const Texture& ropeWord(ClipKit& k)
{
  return cached(k, "e4_rope_westerns", 1000, 200, 500, 100, [](cairo_t* cr) {
    selectGameFont(cr);
    cairo_set_font_size(cr, 150);
    cairo_text_extents_t te;
    cairo_text_extents(cr, "WESTERNS", &te);
    cairo_move_to(cr, 500 - te.x_advance / 2, 100 + te.height / 2);
    cairo_text_path(cr, "WESTERNS");
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    // A dark wash inside the loops so the word reads, a drop shadow, the
    // rope's body, its twist and the light on it.
    setRgbaC(cr, rgb(60, 34, 14), 0.35);
    cairo_fill_preserve(cr);
    cairo_save(cr);
    cairo_translate(cr, 4, 6);
    setRgbaC(cr, rgb(40, 20, 6), 0.45);
    cairo_set_line_width(cr, 15);
    cairo_stroke_preserve(cr);
    cairo_restore(cr);
    setRgbaC(cr, rgb(84, 52, 24), 1.0);
    cairo_set_line_width(cr, 15);
    cairo_stroke_preserve(cr);
    setRgbaC(cr, rgb(206, 166, 104), 1.0);
    cairo_set_line_width(cr, 11);
    cairo_stroke_preserve(cr);
    const double twist[2] = {3.0, 4.5};
    cairo_set_dash(cr, twist, 2, 0);
    setRgbaC(cr, rgb(130, 92, 48), 1.0);
    cairo_set_line_width(cr, 11);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
    cairo_stroke_preserve(cr);
    const double sheen[2] = {2.0, 5.5};
    cairo_set_dash(cr, sheen, 2, 1.0);
    setRgbaC(cr, rgb(250, 228, 180), 0.8);
    cairo_set_line_width(cr, 3);
    cairo_stroke(cr);
    cairo_set_dash(cr, nullptr, 0, 0);
  });
}

// Shot: the Channel 7 ident. A sepia test card; "CH 7" slides in from the
// right (frames 0-7), "WESTERNS" in rope letters drops in (8-11) and swings
// to rest.
void identCh7(ClipKit& k, int frame, int frames, int ticks, float ox, float oy)
{
  const int slideEnd = std::max(1, std::min(7, frames * 2 / 3 - 1));
  k.r.draw(westTestCard(k), ox, oy);
  // Old film: the light breathing, specks of dust.
  const int flick = int(hash2(ticks / 3, 22) % 24u);
  k.r.fillRect(0, 0, W, H, rgba(40, 20, 0, 20 + flick));
  for (int i = 0; i < 14; ++i)
  {
    const unsigned h = hash2(i, ticks / 4 + 99);
    k.r.fillRect(float(h % 1280u), float((h / 1280u) % 720u), 2.0f + float(h % 3u), 2.0f + float(h % 4u),
      rgba(60, 36, 14, 160));
  }
  // "CH 7" on a dark plate, sliding in from the right with an ease-out.
  const float u = std::min(1.0f, float(frame) / float(slideEnd));
  const float e = 1.0f - (1.0f - u) * (1.0f - u) * (1.0f - u);
  const float cx = 640.0f + (1.0f - e) * 900.0f + ox, cy = 120.0f + oy;
  k.r.fillRect(cx - 190.0f, cy - 10.0f, 380.0f, 150.0f, rgba(46, 26, 10, 230));
  k.r.fillRect(cx - 190.0f, cy - 10.0f, 380.0f, 5.0f, rgb(214, 170, 96));
  k.r.fillRect(cx - 190.0f, cy + 135.0f, 380.0f, 5.0f, rgb(214, 170, 96));
  k.r.drawText("CH 7", cx, cy + 4.0f, {120.0f, rgb(250, 232, 190), rgb(20, 10, 4)}, Align::Center);
  // "WESTERNS" drops in once CH 7 has landed.
  if (frame > slideEnd)
  {
    const int dropFrames = std::max(1, frames - 1 - slideEnd);
    const float p = std::min(1.0f, float(frame - slideEnd) / float(dropFrames));
    const float y = 510.0f - (1.0f - p) * (1.0f - p) * 620.0f;
    DrawOpts o;
    o.angle = p >= 1.0f ? std::sin(float(ticks) * 0.12f) * 2.5f * std::exp(-float(ticks % 600) * 0.004f) : 0.0f;
    k.r.draw(ropeWord(k), 640.0f + ox, y + oy, o);
  }
  scanlines(k, 35);
}

// The desert at sundown, 2560 px wide so it pans round: a burning sky, the
// sun on the horizon, mesas, saguaros and the hardpan.
const Texture& sundownDesert(ClipKit& k)
{
  return cached(k, "e4_sundown", 2560, 720, 0, 0, [](cairo_t* cr) {
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 0, 0, 600);
    cairo_pattern_add_color_stop_rgb(g, 0.0, 0.20, 0.10, 0.30);
    cairo_pattern_add_color_stop_rgb(g, 0.45, 0.78, 0.30, 0.30);
    cairo_pattern_add_color_stop_rgb(g, 0.8, 1.0, 0.62, 0.30);
    cairo_pattern_add_color_stop_rgb(g, 1.0, 1.0, 0.82, 0.48);
    cairo_rectangle(cr, 0, 0, 2560, 600);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
    // Two suns, so the strip still tiles; one is always on screen.
    for (const double sx : {760.0, 2040.0})
    {
      radialGlow(cr, sx, 540, 320, rgb(255, 200, 110), 0.55);
      cairo_new_path(cr);
      cairo_arc(cr, sx, 540, 110, 0, 2 * kPi);
      setRgbaC(cr, rgb(255, 226, 150), 1.0);
      cairo_fill(cr);
    }
    // Streaks of cloud.
    std::uint32_t sd = 2207u;
    auto rnd = [&]() {
      sd = sd * 1664525u + 1013904223u;
      return double(sd >> 8) / double(1u << 24);
    };
    for (int i = 0; i < 16; ++i)
    {
      const double x = rnd() * 2560, y = 90 + rnd() * 300, w = 160 + rnd() * 320;
      for (const double dx : {0.0, -2560.0, 2560.0})
      {
        cairo_save(cr);
        cairo_translate(cr, x + dx, y);
        cairo_scale(cr, w, 9 + rnd() * 6);
        cairo_new_path(cr);
        cairo_arc(cr, 0, 0, 0.5, 0, 2 * kPi);
        cairo_restore(cr);
        setRgbaC(cr, rgb(255, 180, 150), 0.35);
        cairo_fill(cr);
      }
    }
    // The mesas: flat tops, sheer sides, in two hazy rows.
    for (int row = 0; row < 2; ++row)
    {
      const Color c = row == 0 ? rgb(176, 92, 96) : rgb(120, 56, 60);
      double x = row * 300.0;
      while (x < 2560 + 200)
      {
        const double w = 220 + rnd() * 360, h = (row == 0 ? 90 : 140) + rnd() * 90, base = 600;
        for (const double dx : {0.0, -2560.0})
        {
          cairo_move_to(cr, x + dx - 30, base);
          cairo_line_to(cr, x + dx + 10, base - h + 18);
          cairo_line_to(cr, x + dx + 26, base - h);
          cairo_line_to(cr, x + dx + w - 26, base - h);
          cairo_line_to(cr, x + dx + w - 10, base - h + 18);
          cairo_line_to(cr, x + dx + w + 30, base);
          cairo_close_path(cr);
          setRgbaC(cr, c, 1.0);
          cairo_fill(cr);
          cairo_rectangle(cr, x + dx + 26, base - h, w - 52, 6);
          setRgbaC(cr, rgb(255, 170, 110), row == 0 ? 0.35 : 0.25);
          cairo_fill(cr);
        }
        x += w + 160 + rnd() * 400;
      }
    }
    // The hardpan.
    cairo_pattern_t* gr = cairo_pattern_create_linear(0, 590, 0, 720);
    cairo_pattern_add_color_stop_rgb(gr, 0.0, 0.80, 0.50, 0.34);
    cairo_pattern_add_color_stop_rgb(gr, 1.0, 0.46, 0.24, 0.16);
    cairo_rectangle(cr, 0, 590, 2560, 130);
    cairo_set_source(cr, gr);
    cairo_fill(cr);
    cairo_pattern_destroy(gr);
    for (int i = 0; i < 60; ++i)
    {
      const double x = rnd() * 2560, y = 610 + rnd() * 100;
      cairo_rectangle(cr, x, y, 30 + rnd() * 90, 2);
      setRgbaC(cr, rgb(90, 46, 30), 0.35);
      cairo_fill(cr);
    }
    // Saguaros against the glow.
    for (int i = 0; i < 9; ++i)
    {
      const double x = 120 + rnd() * 2320, h = 90 + rnd() * 80, base = 604;
      const Color c = rgb(70, 36, 46);
      strokeLimb(cr, {{x, base}, {x, base - h}}, 18, c, c, 0);
      strokeLimb(cr, {{x - 4, base - h * 0.45}, {x - 30, base - h * 0.45}, {x - 30, base - h * 0.75}}, 12, c, c, 0);
      if (rnd() < 0.7)
        strokeLimb(cr, {{x + 4, base - h * 0.6}, {x + 26, base - h * 0.6}, {x + 26, base - h * 0.85}}, 12, c, c, 0);
    }
  });
}

// Lance in the West: pose 9 (the mic up), a ten-gallon hat, and his free
// arm up to it. tip 0..1: the hat lifted and tipped forward.
const Texture& lanceCowboy(ClipKit& k, int tip, bool mouth)
{
  const std::string key = "e4_lance_cowboy" + std::to_string(tip) + (mouth ? "o" : "c");
  // Design coords plus 40 px of headroom (feet at (120, 480) -> (160, 520)).
  return cached(k, key, 320, 540, 160, 520, [&](cairo_t* cr) {
    cairo_translate(cr, 40, 40);
    paintLance(cr, 9, mouth);
    // His head (pose 9): the middle about (126, 112); the hat sits over the
    // quiff and lifts and tips on `tip`.
    static constexpr double kLift[6] = {0.0, 0.0, 0.5, 1.0, 0.6, 0.15};
    const double lift = kLift[std::clamp(tip, 0, 5)];
    const double hatX = 136, brimY = 90 - lift * 26, tilt = -lift * 0.32;
    const Color felt = rgb(246, 238, 222);
    // The far arm comes up behind his head to the brim.
    const double handX = hatX - 68 + lift * 4, handY = brimY + 2 - lift * 4;
    strokeLimb(cr, {{100, 182}, {44, 150 - lift * 10}, {handX, handY}}, 24, rgb(20, 34, 104), rgb(10, 8, 20), 2.6);
    paintCowboyHat(cr, hatX, brimY, 176, felt, tilt);
    // The hand on the brim.
    cairo_new_path(cr);
    cairo_arc(cr, handX, handY, 13, 0, 2 * kPi);
    setRgbaC(cr, rgb(244, 204, 168), 1.0);
    cairo_fill_preserve(cr);
    setRgbaC(cr, rgb(10, 8, 20), 1.0);
    cairo_set_line_width(cr, 2.2);
    cairo_stroke(cr);
    // A gold star on the band, for the showman.
    cairo_new_path(cr);
    for (int i = 0; i < 10; ++i)
    {
      const double a = -kPi / 2 + double(i) * kPi / 5.0 + tilt;
      const double rr = i % 2 == 0 ? 10.0 : 4.5;
      const double px = hatX + std::cos(a) * rr + std::sin(tilt) * 15, py = brimY - 15 + std::sin(a) * rr;
      if (i == 0)
        cairo_move_to(cr, px, py);
      else
        cairo_line_to(cr, px, py);
    }
    cairo_close_path(cr);
    setRgbaC(cr, rgb(255, 210, 70), 1.0);
    cairo_fill(cr);
  });
}

// Shot: Lance, gone West. The desert at sundown pans by; a saloon's false
// front behind him; he tips his hat (6-frame loop) while a tumbleweed
// bowls across the foreground (16 frames).
void lanceCowboyClip(ClipKit& k, int frame, int frames, float t, int ticks, float ox, float oy)
{
  panLayer(k, sundownDesert(k), float(ticks) / 5.4f, oy);
  const Texture& saloon = cached(k, "e4_saloon_front", 560, 460, 0, 460, [](cairo_t* cr) {
    paintSaloonFront(cr, 560, 460, "SALOON");
  });
  // Its shadow on the hardpan, then the building.
  k.r.fillRect(690 + ox, 640 + oy, 600, 18, rgba(60, 24, 20, 90));
  k.r.draw(saloon, 700 + ox, 650 + oy);
  // Warm light from the sun on everything in front.
  drawGlow(k.r, k.art, 380 + ox, 420 + oy, 380.0f, rgb(255, 170, 90), 0.25f);
  const bool talking = t < 0.45f && (ticks / 7) % 2 == 0;
  k.r.draw(lanceCowboy(k, frame % 6, talking), 380 + ox, 690 + oy + std::sin(float(ticks) * 0.07f) * 2.0f);
  // The tumbleweed, bouncing as it rolls.
  const int n = std::max(1, frames - 1);
  const float u = float(frame) / float(n);
  const float tx = -160.0f + u * 1620.0f;
  const float bounce = std::abs(std::sin(u * 3.1416f * 5.0f)) * 110.0f;
  const Texture& weed = cached(k, "e4_tumbleweed", 200, 200, 100, 100, [](cairo_t* cr) {
    paintTumbleweed(cr, 100, 100, 92, 22u);
  });
  k.r.fillRect(tx - 80 + ox, 694 + oy, 160, 10, rgba(60, 24, 20, int(90 - bounce * 0.7f)));
  DrawOpts o;
  o.angle = tx * 0.35f;
  o.scale = 1.3f;
  k.r.draw(weed, tx + ox, 570 - bounce + oy, o);
}

// --- Level 23: Channel 13, Horror ----------------------------------------------

// The drips under "CH 13": where each hangs from the letters (x from the
// word's middle), how long it is baked, and how fast it grows.
struct Ch13Drip
{
  float x, len, speed, width;
};

constexpr Ch13Drip kCh13Drips[] = {{-300, 18, 1.0f, 12}, {-236, 34, 0.7f, 9}, {-170, 12, 1.4f, 10},
                                   {-84, 26, 0.9f, 13},  {-20, 16, 1.2f, 9},  {60, 40, 0.6f, 12},
                                   {118, 14, 1.5f, 10},  {190, 28, 0.8f, 11}, {262, 20, 1.1f, 9}};
constexpr float kCh13Base = 236.0f; // the baseline in the baked word (y)

// "CH 13" in green slime: a glossy bevelled fill, a dark rind, and the drips
// hanging from it baked short (they grow live).
const Texture& ch13Word(ClipKit& k)
{
  return cached(k, "e4_ch13_slime", 800, 320, 400, 0, [](cairo_t* cr) {
    selectGameFont(cr);
    cairo_set_font_size(cr, 200);
    cairo_text_extents_t te;
    cairo_text_extents(cr, "CH 13", &te);
    cairo_move_to(cr, 400 - te.x_advance / 2, kCh13Base);
    cairo_text_path(cr, "CH 13");
    for (const auto& d : kCh13Drips)
    {
      const double x = 400 + d.x;
      cairo_new_sub_path(cr);
      cairo_move_to(cr, x - d.width * 0.5, kCh13Base - 14);
      cairo_line_to(cr, x - d.width * 0.35, kCh13Base + d.len);
      cairo_arc(cr, x, kCh13Base + d.len, d.width * 0.45, kPi, 2 * kPi);
      cairo_line_to(cr, x + d.width * 0.5, kCh13Base - 14);
      cairo_close_path(cr);
      cairo_new_sub_path(cr);
      cairo_arc(cr, x, kCh13Base + d.len + d.width * 0.2, d.width * 0.55, 0, 2 * kPi);
    }
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_save(cr);
    cairo_translate(cr, 6, 8);
    setRgbaC(cr, rgb(0, 30, 10), 0.6);
    cairo_fill_preserve(cr);
    cairo_restore(cr);
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 60, 0, 300);
    cairo_pattern_add_color_stop_rgb(g, 0.0, 0.75, 1.0, 0.55);
    cairo_pattern_add_color_stop_rgb(g, 0.45, 0.3, 0.9, 0.3);
    cairo_pattern_add_color_stop_rgb(g, 1.0, 0.08, 0.42, 0.12);
    cairo_set_source(cr, g);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(g);
    setRgbaC(cr, rgb(4, 30, 10), 1.0);
    cairo_set_line_width(cr, 6);
    cairo_stroke(cr);
    // Wet highlights along the tops of the letters.
    cairo_move_to(cr, 400 - te.x_advance / 2, kCh13Base);
    cairo_text_path(cr, "CH 13");
    cairo_save(cr);
    cairo_clip(cr);
    for (int i = 0; i < 9; ++i)
    {
      cairo_move_to(cr, 70 + i * 74, 90);
      cairo_curve_to(cr, 90 + i * 74, 80, 110 + i * 74, 84, 120 + i * 74, 96);
    }
    setRgbaC(cr, rgb(240, 255, 220), 0.7);
    cairo_set_line_width(cr, 7);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_stroke(cr);
    cairo_restore(cr);
  });
}

// A drop of slime, 24 x 32, its top at (12, 0).
const Texture& slimeDrop(ClipKit& k)
{
  return cached(k, "e4_slime_drop", 24, 32, 12, 0, [](cairo_t* cr) {
    cairo_move_to(cr, 12, 0);
    cairo_curve_to(cr, 14, 10, 22, 16, 22, 22);
    cairo_arc(cr, 12, 22, 10, 0, kPi);
    cairo_curve_to(cr, 2, 16, 10, 10, 12, 0);
    cairo_close_path(cr);
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 0, 24, 0);
    cairo_pattern_add_color_stop_rgb(g, 0, 0.5, 1.0, 0.45);
    cairo_pattern_add_color_stop_rgb(g, 1, 0.08, 0.45, 0.12);
    cairo_set_source(cr, g);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(g);
    setRgbaC(cr, rgb(4, 30, 10), 1.0);
    cairo_set_line_width(cr, 2);
    cairo_stroke(cr);
    cairo_arc(cr, 9, 22, 2.5, 0, 2 * kPi);
    setRgbaC(cr, rgb(240, 255, 220), 0.8);
    cairo_fill(cr);
  });
}

// "HORROR" in bone-white capitals with a blood-red rim, slightly unsteady.
const Texture& horrorWord(ClipKit& k)
{
  return cached(k, "e4_horror_word", 900, 160, 450, 80, [](cairo_t* cr) {
    selectGameFont(cr);
    cairo_set_font_size(cr, 118);
    const char* word = "HORROR";
    double x = 0;
    cairo_text_extents_t te;
    cairo_text_extents(cr, word, &te);
    x = 450 - te.x_advance / 2;
    for (int i = 0; word[i]; ++i)
    {
      const char s[2] = {word[i], 0};
      cairo_text_extents_t ce;
      cairo_text_extents(cr, s, &ce);
      cairo_save(cr);
      cairo_translate(cr, x + ce.x_advance * 0.5, 80 + 42);
      cairo_rotate(cr, (i % 2 ? 1 : -1) * 0.05 * (1 + i % 3));
      cairo_move_to(cr, -ce.x_advance * 0.5, 0);
      cairo_text_path(cr, s);
      cairo_restore(cr);
      x += ce.x_advance;
    }
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    setRgbaC(cr, rgb(150, 0, 16), 1.0);
    cairo_set_line_width(cr, 10);
    cairo_stroke_preserve(cr);
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 40, 0, 130);
    cairo_pattern_add_color_stop_rgb(g, 0, 1.0, 0.98, 0.92);
    cairo_pattern_add_color_stop_rgb(g, 1, 0.75, 0.7, 0.62);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  });
}

// Shot: the Channel 13 ident. Black; "CH 13" in green slime, its drips
// creeping down 2 px a frame (and now and then a drop falling); "HORROR"
// flickers in on frames 6-11 like a failing tube.
void identCh13(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  k.r.fillRect(0, 0, W, H, rgb(0, 0, 0));
  drawGlow(k.r, k.art, 640 + ox, 250 + oy, 520.0f, rgb(40, 200, 80), 0.18f + 0.04f * std::sin(float(ticks) * 0.1f));
  const float wx = 640.0f + ox, wy = 60.0f + oy;
  // The drips grow live: 2 px every clip frame (80 ms, five ticks).
  const float grow = float(ticks) / 5.0f * 2.0f;
  int di = 0;
  for (const auto& d : kCh13Drips)
  {
    const float cycle = 260.0f + float(di) * 37.0f;
    const float extra = std::fmod(grow * d.speed, cycle);
    const float x = wx + d.x, top = wy + kCh13Base + d.len - 4.0f;
    k.r.fillRect(x - d.width * 0.32f, top, d.width * 0.64f, extra, rgb(40, 170, 60));
    k.r.fillRect(x - d.width * 0.32f, top, d.width * 0.16f, extra, rgb(120, 230, 110));
    DrawOpts o;
    o.scale = d.width / 18.0f;
    k.r.draw(slimeDrop(k), x, top + extra - 4.0f, o);
    // A drop let go: it falls on.
    const float since = std::fmod(grow * d.speed, cycle) < 40.0f && grow * d.speed > cycle ? std::fmod(grow * d.speed, cycle) : -1.0f;
    if (since >= 0.0f)
      k.r.draw(slimeDrop(k), x, top + cycle + since * 6.0f, o);
    ++di;
  }
  k.r.draw(ch13Word(k), wx, wy);
  if (frame >= 6)
  {
    // A failing tube: on, off, half on, then steady with a rare blink.
    const unsigned hsh = hash2(ticks / 3, 13);
    bool on = true;
    float a = 1.0f;
    if (frame < 10)
    {
      on = hsh % 3u != 0u;
      a = 0.4f + float(hsh % 60u) / 100.0f;
    }
    else
      on = hsh % 29u != 0u;
    if (on)
    {
      DrawOpts o;
      o.alpha = a;
      drawGlow(k.r, k.art, 640 + ox, 520 + oy, 360.0f, rgb(255, 40, 40), 0.18f * a);
      k.r.draw(horrorWord(k), 640 + ox, 520 + oy, o);
    }
  }
  // The station's line, and old film over it all.
  k.r.drawText("MAXTV  CHANNEL 13  AFTER DARK", 640 + ox, 640 + oy, {18.0f, rgb(90, 160, 100)}, Align::Center);
  for (int i = 0; i < 10; ++i)
  {
    const unsigned h = hash2(i, ticks / 4 + 313);
    k.r.fillRect(float(h % 1280u), float((h / 1280u) % 720u), 2.0f, 2.0f + float(h % 4u), rgba(120, 200, 130, 90));
  }
  scanlines(k, 40);
}

// The manor on its hill under the storm, 1280 x 720: the sky, the moon
// in the clouds, the hill, the house with its lit windows, a dead tree and
// the gate posts in the foreground.
const Texture& manorHill(ClipKit& k)
{
  return cached(k, "e4_manor_hill", 1280, 720, 0, 0, [](cairo_t* cr) {
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 0, 0, 720);
    cairo_pattern_add_color_stop_rgb(g, 0.0, 0.03, 0.02, 0.08);
    cairo_pattern_add_color_stop_rgb(g, 0.55, 0.14, 0.11, 0.26);
    cairo_pattern_add_color_stop_rgb(g, 1.0, 0.3, 0.27, 0.42);
    cairo_rectangle(cr, 0, 0, 1280, 720);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
    radialGlow(cr, 1010, 140, 300, rgb(170, 160, 230), 0.35);
    cairo_arc(cr, 1010, 140, 58, 0, 2 * kPi);
    setRgbaC(cr, rgb(240, 240, 250), 1.0);
    cairo_fill(cr);
    std::uint32_t sd = 2323u;
    auto rnd = [&]() {
      sd = sd * 1664525u + 1013904223u;
      return double(sd >> 8) / double(1u << 24);
    };
    // Clouds: dark banks with moonlit rims.
    for (int i = 0; i < 14; ++i)
    {
      const double cx = rnd() * 1400 - 60, cy = 60 + rnd() * 280, w = 200 + rnd() * 300, h = 30 + rnd() * 40;
      for (int pass = 0; pass < 2; ++pass)
      {
        cairo_save(cr);
        cairo_translate(cr, cx, cy - (pass == 0 ? 4 : 0));
        cairo_scale(cr, w * 0.5, h * 0.5);
        cairo_new_path(cr);
        cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
        cairo_restore(cr);
        if (pass == 0)
          setRgbaC(cr, rgb(200, 196, 240), std::abs(cx - 1010) < 300 ? 0.55 : 0.18);
        else
          setRgbaC(cr, rgb(30, 24, 52), 0.92);
        cairo_fill(cr);
      }
    }
    // The hill and the house.
    const Color body = rgb(14, 10, 24);
    cairo_move_to(cr, 0, 720);
    cairo_curve_to(cr, 200, 560, 420, 470, 640, 462);
    cairo_curve_to(cr, 860, 470, 1080, 560, 1280, 640);
    cairo_line_to(cr, 1280, 720);
    cairo_close_path(cr);
    setRgbaC(cr, body, 1.0);
    cairo_fill(cr);
    paintHauntedManor(cr, 720, 474, 0.95, body, rgb(255, 214, 120));
    // A winding path up to the door.
    cairo_move_to(cr, 560, 720);
    cairo_curve_to(cr, 640, 640, 560, 560, 712, 476);
    setRgbaC(cr, rgb(60, 54, 80), 0.6);
    cairo_set_line_width(cr, 10);
    cairo_stroke(cr);
    // A dead tree on the left, its branches clawing at the sky.
    std::function<void(double, double, double, double, double, int)> branch = [&](double x0, double y0, double a,
                                                                                    double len, double wd, int depth) {
      const double x1 = x0 + std::cos(a) * len, y1 = y0 + std::sin(a) * len;
      cairo_move_to(cr, x0, y0);
      cairo_line_to(cr, x1, y1);
      setRgbaC(cr, body, 1.0);
      cairo_set_line_width(cr, wd);
      cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
      cairo_stroke(cr);
      if (depth > 0)
        for (int c = 0; c < 2; ++c)
          branch(x1, y1, a + (c == 0 ? -0.5 : 0.45) + (rnd() - 0.5) * 0.4, len * 0.72, wd * 0.62, depth - 1);
    };
    branch(170, 640, -kPi * 0.5 - 0.08, 150, 22, 6);
  });
}

// Lance in a cape: pose 8, arms thrown wide (the mic still in one hand),
// a flashlight tucked under his chin; the cape (behind him) swirls through
// 8 frames; lit from below, his face is all ghoulish shadows. mouth:
// talking (fangs show).
const Texture& lanceCaped(ClipKit& k, int swirl, bool mouth)
{
  const std::string key = "e4_lance_cape" + std::to_string(swirl) + (mouth ? "o" : "c");
  // Design coords plus 90 px each side and 40 on top (feet at (120, 480)).
  return cached(k, key, 420, 560, 210, 520, [&](cairo_t* cr) {
    cairo_translate(cr, 90, 40);
    cairo_push_group(cr);
    const double sw = double(swirl) / 8.0;
    paintCapeCollar(cr, 128, 172, 170, 100);
    paintVampireCape(cr, 122, 168, 330, 300, sw, 0.55 + 0.25 * std::sin(sw * 2 * kPi));
    paintLance(cr, 8, mouth);
    // The flashlight, tucked under his chin, pointing up at it.
    const double tx = 130, ty = 176;
    strokeLimb(cr, {{136, 222}, {tx, ty}}, 14, rgb(40, 40, 46), rgb(10, 8, 20), 2.2);
    cairo_new_path(cr);
    cairo_save(cr);
    cairo_translate(cr, tx, ty);
    cairo_scale(cr, 11, 5);
    cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
    cairo_restore(cr);
    setRgbaC(cr, rgb(255, 250, 220), 1.0);
    cairo_fill_preserve(cr);
    setRgbaC(cr, rgb(10, 8, 20), 1.0);
    cairo_set_line_width(cr, 2.0);
    cairo_stroke(cr);
    // Lit from below: dark from the brow up, warm light on the chin and
    // the underside of everything near the torch.
    cairo_set_operator(cr, CAIRO_OPERATOR_ATOP);
    cairo_rectangle(cr, -90, -40, 420, 560);
    cairo_pattern_t* dark = cairo_pattern_create_linear(0, 20, 0, 260);
    cairo_pattern_add_color_stop_rgba(dark, 0, 0.02, 0.0, 0.08, 0.72);
    cairo_pattern_add_color_stop_rgba(dark, 0.45, 0.02, 0.0, 0.08, 0.45);
    cairo_pattern_add_color_stop_rgba(dark, 1, 0.02, 0.0, 0.08, 0.15);
    cairo_set_source(cr, dark);
    cairo_fill(cr);
    cairo_pattern_destroy(dark);
    cairo_rectangle(cr, -90, -40, 420, 560);
    cairo_pattern_t* lit = cairo_pattern_create_radial(tx, ty, 4, tx, ty - 30, 110);
    cairo_pattern_add_color_stop_rgba(lit, 0, 1.0, 0.9, 0.6, 0.6);
    cairo_pattern_add_color_stop_rgba(lit, 1, 1.0, 0.9, 0.6, 0.0);
    cairo_set_source(cr, lit);
    cairo_fill(cr);
    cairo_pattern_destroy(lit);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_pop_group_to_source(cr);
    cairo_paint(cr);
    if (mouth)
    {
      // Fangs.
      for (const double fx : {137.0, 156.0})
      {
        cairo_move_to(cr, fx - 3.5, 133);
        cairo_line_to(cr, fx, 146);
        cairo_line_to(cr, fx + 3.5, 133);
        cairo_close_path(cr);
        setRgbaC(cr, rgb(255, 255, 255), 1.0);
        cairo_fill_preserve(cr);
        setRgbaC(cr, rgb(10, 8, 20), 1.0);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);
      }
    }
  });
}

// Shot: Lance at the manor. The manor on its hill under the storm, rain
// slanting; lightning splits the sky on frame 3; Lance in his cape, lit
// from below by a flashlight that flickers (4-frame loop), the cape
// swirling (8-frame loop).
void lanceCapeClip(ClipKit& k, int frame, float t, int ticks, float ox, float oy)
{
  k.r.draw(manorHill(k), ox, oy);
  const bool bolt = frame % 16 == 3, after = frame % 16 == 4;
  if (bolt || after)
  {
    // The sky lights up, and the bolt: a jagged fork down behind the house.
    k.r.fillRect(0, 0, W, 480, rgba(200, 200, 255, bolt ? 90 : 35));
    std::uint32_t sd = 1313u + std::uint32_t(ticks / 120);
    float x = 380.0f + float(sd % 200u), y = 0.0f;
    while (y < 440.0f)
    {
      sd = sd * 1664525u + 1013904223u;
      const float nx = x + float(int(sd >> 24) % 60 - 30), ny = y + 30.0f + float((sd >> 16) % 30u);
      k.r.drawLine(x + ox, y + oy, nx + ox, ny + oy, bolt ? 7.0f : 3.0f, rgba(200, 200, 255, bolt ? 140 : 60), Blend::Add);
      k.r.drawLine(x + ox, y + oy, nx + ox, ny + oy, bolt ? 3.0f : 1.5f, rgb(255, 255, 255), Blend::Add);
      if ((sd >> 8) % 4u == 0)
        k.r.drawLine(nx + ox, ny + oy, nx + 50.0f + ox, ny + 40.0f + oy, 1.5f, rgba(220, 220, 255, 160), Blend::Add);
      x = nx;
      y = ny;
    }
    drawGlow(k.r, k.art, x + ox, 300 + oy, 300.0f, rgb(200, 200, 255), bolt ? 0.5f : 0.2f);
  }
  // Rain.
  for (int i = 0; i < 90; ++i)
  {
    const unsigned h = hash2(i, 23);
    const float rx = std::fmod(float(h % 1400u) + float(ticks) * 3.0f, 1400.0f) - 60.0f;
    const float ry = std::fmod(float((h >> 11) % 720u) + float(ticks) * 18.0f, 760.0f) - 40.0f;
    k.r.drawLine(rx + ox, ry + oy, rx + 6.0f + ox, ry + 26.0f + oy, 1.5f, rgba(190, 190, 230, 70));
  }
  // Lance, the flashlight flickering under his chin.
  const bool talking = t > 0.06f && t < 0.5f && (ticks / 7) % 2 == 0;
  static constexpr float kFlicker[4] = {1.0f, 0.75f, 0.95f, 0.6f};
  const float fl = kFlicker[frame % 4];
  const float lx = 400.0f + ox, ly = 690.0f + oy + std::sin(float(ticks) * 0.05f) * 2.0f;
  drawGlow(k.r, k.art, lx + 6.0f, ly - 360.0f, 260.0f, rgb(255, 200, 120), 0.18f * fl);
  k.r.draw(lanceCaped(k, frame % 8, talking), lx, ly);
  drawGlow(k.r, k.art, lx + 10.0f, ly - 304.0f, 60.0f * fl, rgb(255, 230, 160), 0.5f * fl);
  // The flashlight's beam up into his face, flickering.
  k.r.drawLine(lx + 10.0f, ly - 304.0f, lx + 6.0f, ly - 380.0f, 56.0f * fl, rgba(255, 240, 190, int(36 * fl)), Blend::Add);
  if (bolt)
    k.r.fillRect(0, 0, W, H, rgba(220, 220, 255, 40));
}

// --- Level 24: Channel 24, Cooking ---------------------------------------------

// A red-and-white gingham tablecloth, 1280 x 720: the weave where the
// stripes cross darker, a thread texture, folds in the light.
const Texture& gingham(ClipKit& k)
{
  return cached(k, "e4_gingham", 1280, 720, 0, 0, [](cairo_t* cr) {
    cairo_rectangle(cr, 0, 0, 1280, 720);
    setRgbaC(cr, rgb(250, 246, 238), 1.0);
    cairo_fill(cr);
    const double sq = 64;
    for (int i = 0; i < 21; ++i)
    {
      cairo_rectangle(cr, i * sq * 2 - 20, 0, sq, 720);
      cairo_rectangle(cr, 0, i * sq * 2 - 30, 1280, sq);
    }
    setRgbaC(cr, rgb(214, 34, 40), 0.55);
    cairo_fill(cr);
    // Where the stripes cross, the red doubles.
    for (int x = 0; x < 11; ++x)
      for (int y = 0; y < 7; ++y)
      {
        cairo_rectangle(cr, x * sq * 2 - 20, y * sq * 2 - 30, sq, sq);
        setRgbaC(cr, rgb(170, 16, 24), 0.55);
        cairo_fill(cr);
      }
    // Threads.
    for (double y = 0; y < 720; y += 4)
    {
      cairo_rectangle(cr, 0, y, 1280, 1);
      setRgbaC(cr, rgb(120, 40, 40), 0.06);
      cairo_fill(cr);
    }
    // Soft folds and a vignette.
    for (const double fx : {300.0, 900.0})
    {
      cairo_rectangle(cr, fx - 80, 0, 160, 720);
      cairo_pattern_t* g = cairo_pattern_create_linear(fx - 80, 0, fx + 80, 0);
      cairo_pattern_add_color_stop_rgba(g, 0, 0, 0, 0, 0.0);
      cairo_pattern_add_color_stop_rgba(g, 0.5, 0, 0, 0, 0.1);
      cairo_pattern_add_color_stop_rgba(g, 0.6, 1, 1, 1, 0.12);
      cairo_pattern_add_color_stop_rgba(g, 1, 1, 1, 1, 0.0);
      cairo_set_source(cr, g);
      cairo_fill(cr);
      cairo_pattern_destroy(g);
    }
    cairo_pattern_t* v = cairo_pattern_create_radial(640, 360, 200, 640, 360, 800);
    cairo_pattern_add_color_stop_rgba(v, 0, 0, 0, 0, 0.0);
    cairo_pattern_add_color_stop_rgba(v, 1, 0.2, 0, 0, 0.45);
    cairo_rectangle(cr, 0, 0, 1280, 720);
    cairo_set_source(cr, v);
    cairo_fill(cr);
    cairo_pattern_destroy(v);
  });
}

// "CH 24" as a rubber stamp in chocolate-brown ink: a rounded frame round
// the letters, the ink uneven, 640 x 300, centred.
const Texture& ch24Stamp(ClipKit& k)
{
  return cached(k, "e4_ch24_stamp", 640, 300, 320, 150, [](cairo_t* cr) {
    const Color ink = rgb(70, 30, 20);
    // A white plate under it so it reads on the cloth.
    cairo_new_path(cr);
    cairo_save(cr);
    cairo_translate(cr, 320, 150);
    cairo_scale(cr, 300, 140);
    cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
    cairo_restore(cr);
    setRgbaC(cr, rgb(255, 255, 252), 1.0);
    cairo_fill_preserve(cr);
    setRgbaC(cr, rgb(180, 170, 160), 1.0);
    cairo_set_line_width(cr, 4);
    cairo_stroke(cr);
    cairo_new_path(cr);
    cairo_save(cr);
    cairo_translate(cr, 320, 150);
    cairo_scale(cr, 250, 110);
    cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
    cairo_restore(cr);
    setRgbaC(cr, rgb(220, 212, 204), 1.0);
    cairo_set_line_width(cr, 2);
    cairo_stroke(cr);
    // The stamp's frame and its letters.
    roundedRect(cr, 120, 70, 400, 160, 26);
    setRgbaC(cr, ink, 0.92);
    cairo_set_line_width(cr, 10);
    cairo_stroke(cr);
    selectGameFont(cr);
    cairo_set_font_size(cr, 124);
    cairo_text_extents_t te;
    cairo_text_extents(cr, "CH 24", &te);
    cairo_move_to(cr, 320 - te.x_advance / 2, 196);
    cairo_text_path(cr, "CH 24");
    setRgbaC(cr, ink, 0.92);
    cairo_fill(cr);
    // Where the stamp missed: specks of the plate showing through the ink.
    std::uint32_t sd = 2423u;
    for (int i = 0; i < 260; ++i)
    {
      sd = sd * 1664525u + 1013904223u;
      const double x = 120 + double(sd >> 8) / double(1u << 24) * 400;
      sd = sd * 1664525u + 1013904223u;
      const double y = 70 + double(sd >> 8) / double(1u << 24) * 160;
      cairo_new_path(cr);
      cairo_arc(cr, x, y, 1 + (sd % 3u), 0, 2 * kPi);
      setRgbaC(cr, rgb(255, 255, 252), 0.85);
      cairo_fill(cr);
    }
  });
}

// "COOKING" piped in icing: thick cream strokes round the letters, a pink
// shadow, a glossy highlight; `upTo` 0..1 how much of it is written.
const Texture& icingWord(ClipKit& k)
{
  return cached(k, "e4_icing", 1000, 220, 500, 110, [](cairo_t* cr) {
    selectGameFont(cr, true);
    cairo_set_font_size(cr, 150);
    cairo_text_extents_t te;
    cairo_text_extents(cr, "COOKING", &te);
    const double x0 = 500 - te.x_advance / 2;
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    // Shadow, body, highlight.
    cairo_move_to(cr, x0 + 6, 172);
    cairo_text_path(cr, "COOKING");
    setRgbaC(cr, rgb(120, 20, 30), 0.45);
    cairo_set_line_width(cr, 22);
    cairo_stroke(cr);
    cairo_move_to(cr, x0, 166);
    cairo_text_path(cr, "COOKING");
    setRgbaC(cr, rgb(244, 160, 180), 1.0);
    cairo_set_line_width(cr, 20);
    cairo_stroke_preserve(cr);
    setRgbaC(cr, rgb(255, 250, 244), 1.0);
    cairo_set_line_width(cr, 14);
    cairo_stroke_preserve(cr);
    cairo_fill(cr);
    cairo_move_to(cr, x0 - 3, 162);
    cairo_text_path(cr, "COOKING");
    setRgbaC(cr, rgb(255, 255, 255), 0.9);
    cairo_set_line_width(cr, 4);
    cairo_stroke(cr);
    // Sprinkles.
    std::uint32_t sd = 2441u;
    for (int i = 0; i < 70; ++i)
    {
      sd = sd * 1664525u + 1013904223u;
      const double x = x0 + double(sd >> 8) / double(1u << 24) * te.x_advance;
      sd = sd * 1664525u + 1013904223u;
      const double y = 60 + double(sd >> 8) / double(1u << 24) * 110;
      static const Color kSprinkle[4] = {rgb(255, 80, 120), rgb(80, 200, 255), rgb(255, 210, 60), rgb(120, 220, 110)};
      cairo_save(cr);
      cairo_translate(cr, x, y);
      cairo_rotate(cr, double(sd % 628u) / 100.0);
      cairo_rectangle(cr, -4, -1.3, 8, 2.6);
      cairo_restore(cr);
      setRgbaC(cr, kSprinkle[sd % 4u], 0.95);
      cairo_fill(cr);
    }
  });
}

// A piping bag, its nozzle at (0, 0), pointing down-left.
const Texture& pipingBag(ClipKit& k)
{
  return cached(k, "e4_piping_bag", 200, 200, 20, 180, [](cairo_t* cr) {
    cairo_translate(cr, 20, 180);
    cairo_rotate(cr, -0.7);
    cairo_move_to(cr, 0, 0);
    cairo_line_to(cr, -50, -160);
    cairo_curve_to(cr, -40, -200, 40, -200, 50, -160);
    cairo_close_path(cr);
    cairo_pattern_t* g = cairo_pattern_create_linear(-50, 0, 50, 0);
    cairo_pattern_add_color_stop_rgb(g, 0, 0.92, 0.94, 0.98);
    cairo_pattern_add_color_stop_rgb(g, 1, 0.7, 0.74, 0.82);
    cairo_set_source(cr, g);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(g);
    setRgbaC(cr, rgb(10, 8, 20), 1.0);
    cairo_set_line_width(cr, 3);
    cairo_stroke(cr);
    cairo_move_to(cr, -10, -28);
    cairo_line_to(cr, 0, 2);
    cairo_line_to(cr, 10, -28);
    cairo_close_path(cr);
    setRgbaC(cr, rgb(200, 206, 216), 1.0);
    cairo_fill_preserve(cr);
    setRgbaC(cr, rgb(10, 8, 20), 1.0);
    cairo_set_line_width(cr, 2);
    cairo_stroke(cr);
  });
}

// Shot: the Channel 24 ident. The tablecloth; "CH 24" stamped down on a
// plate on frame 2 (squashed flat that frame, springing back the next);
// "COOKING" piped in icing left to right over frames 4-11.
void identCh24(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  k.r.draw(gingham(k), ox, oy);
  const float cx = 640.0f + ox, cy = 230.0f + oy;
  const Texture& stamp = ch24Stamp(k);
  if (frame < 2)
  {
    // The stamp's shadow closing in from above.
    const float s = 1.0f + float(2 - frame) * 0.25f;
    k.r.fillRect(cx - 200.0f * s, cy - 80.0f * s, 400.0f * s, 160.0f * s, rgba(60, 0, 0, 30 + frame * 30));
  }
  else if (frame == 2 || frame == 3)
  {
    // The squash: wide and flat on the impact, then a little tall.
    const float sxv = frame == 2 ? 1.22f : 0.94f, syv = frame == 2 ? 0.7f : 1.08f;
    stretchTex(k, stamp, cx - 320.0f * sxv, cy - 150.0f * syv, 640.0f * sxv, 300.0f * syv, rgb(255, 255, 255), 1.0f);
    if (frame == 2)
      for (int i = 0; i < 10; ++i)
      {
        const float a = float(i) / 10.0f * 6.2832f;
        k.r.drawLine(cx + std::cos(a) * 340.0f, cy + std::sin(a) * 170.0f, cx + std::cos(a) * 420.0f,
          cy + std::sin(a) * 210.0f, 6.0f, rgb(255, 255, 255));
      }
  }
  else
    k.r.draw(stamp, cx, cy);
  if (frame >= 4)
  {
    // Piped left to right: the word revealed up to the nozzle.
    const Texture& word = icingWord(k);
    const float wx = 640.0f + ox, wy = 540.0f + oy;
    const float sub = std::clamp((float(ticks) / 4.8f) - std::floor(float(ticks) / 4.8f), 0.0f, 1.0f);
    const float u = std::min(1.0f, (float(frame - 4) + (frame < 11 ? sub : 1.0f)) / 7.0f);
    const float left = wx - 430.0f, right = left + 860.0f * u;
    clipTo(k, 0.0f, 0.0f, right, H);
    k.r.draw(word, wx, wy);
    unclip(k);
    if (u < 1.0f)
      k.r.draw(pipingBag(k), right, wy + 20.0f + std::sin(float(ticks) * 0.9f) * 18.0f);
  }
  k.r.drawText("MAXTV  CHANNEL 24  COOKING", 640 + ox, 676 + oy, {18.0f, rgb(255, 240, 230), rgb(90, 10, 16)}, Align::Center);
  scanlines(k, 30);
}

// The giant kitchen set under the studio lights, 1280 x 720 (anchored at
// its middle so it can zoom): the lighting grid and its cones, the tiled
// back wall, shelves of jars, copper pans, the range with a pot steaming,
// the marble counter across the bottom.
const Texture& kitchenSet(ClipKit& k)
{
  return cached(k, "e4_kitchen_set", 1280, 720, 640, 360, [](cairo_t* cr) {
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 0, 0, 720);
    cairo_pattern_add_color_stop_rgb(g, 0.0, 0.05, 0.04, 0.08);
    cairo_pattern_add_color_stop_rgb(g, 0.3, 0.16, 0.12, 0.14);
    cairo_pattern_add_color_stop_rgb(g, 1.0, 0.3, 0.2, 0.16);
    cairo_rectangle(cr, 0, 0, 1280, 720);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
    // The back wall: cream tiles.
    cairo_rectangle(cr, 0, 170, 1280, 420);
    setRgbaC(cr, rgb(214, 204, 186), 1.0);
    cairo_fill(cr);
    for (int y = 0; y < 9; ++y)
      for (int x = 0; x < 28; ++x)
      {
        roundedRect(cr, x * 48 + (y % 2) * 24 - 20, 174 + y * 48, 44, 44, 4);
        setRgbaC(cr, (x * 7 + y * 3) % 11 == 0 ? rgb(196, 216, 236) : rgb(240, 234, 222), 1.0);
        cairo_fill(cr);
      }
    // Shelves of jars on the left.
    cairo_rectangle(cr, 40, 320, 360, 14);
    setRgbaC(cr, rgb(150, 98, 52), 1.0);
    cairo_fill(cr);
    const Color jars[5] = {rgb(230, 120, 50), rgb(230, 200, 110), rgb(120, 170, 70), rgb(200, 60, 40), rgb(150, 110, 70)};
    for (int i = 0; i < 6; ++i)
    {
      const double jx = 56 + i * 58, jh = 70 + (i % 3) * 14;
      roundedRect(cr, jx, 320 - jh, 46, jh, 10);
      setRgbaC(cr, rgb(220, 236, 240), 0.9);
      cairo_fill(cr);
      roundedRect(cr, jx + 4, 320 - jh * 0.65, 38, jh * 0.65 - 4, 6);
      setRgbaC(cr, jars[i % 5], 1.0);
      cairo_fill(cr);
      cairo_rectangle(cr, jx - 2, 320 - jh - 10, 50, 12);
      setRgbaC(cr, rgb(200, 40, 40), 1.0);
      cairo_fill(cr);
    }
    // Copper pans on a rail on the right.
    cairo_rectangle(cr, 760, 220, 480, 8);
    setRgbaC(cr, rgb(200, 206, 216), 1.0);
    cairo_fill(cr);
    for (int i = 0; i < 5; ++i)
    {
      const double px = 800 + i * 96, pr = 30 + (i % 2) * 10;
      cairo_move_to(cr, px, 228);
      cairo_line_to(cr, px, 262);
      setRgbaC(cr, rgb(170, 100, 50), 1.0);
      cairo_set_line_width(cr, 8);
      cairo_stroke(cr);
      cairo_new_path(cr);
      cairo_arc(cr, px, 262 + pr, pr, 0, 2 * kPi);
      cairo_pattern_t* cg = cairo_pattern_create_radial(px - pr * 0.4, 262 + pr * 0.6, 2, px, 262 + pr, pr);
      cairo_pattern_add_color_stop_rgb(cg, 0, 1.0, 0.8, 0.6);
      cairo_pattern_add_color_stop_rgb(cg, 1, 0.6, 0.3, 0.14);
      cairo_set_source(cr, cg);
      cairo_fill(cr);
      cairo_pattern_destroy(cg);
    }
    // The range hood over the middle.
    cairo_move_to(cr, 480, 400);
    cairo_line_to(cr, 560, 250);
    cairo_line_to(cr, 560, 170);
    cairo_line_to(cr, 720, 170);
    cairo_line_to(cr, 720, 250);
    cairo_line_to(cr, 800, 400);
    cairo_close_path(cr);
    cairo_pattern_t* hg = cairo_pattern_create_linear(480, 0, 800, 0);
    cairo_pattern_add_color_stop_rgb(hg, 0, 0.6, 0.64, 0.7);
    cairo_pattern_add_color_stop_rgb(hg, 0.4, 0.95, 0.96, 0.98);
    cairo_pattern_add_color_stop_rgb(hg, 1, 0.5, 0.54, 0.6);
    cairo_set_source(cr, hg);
    cairo_fill(cr);
    cairo_pattern_destroy(hg);
    radialGlow(cr, 640, 420, 140, rgb(255, 220, 160), 0.45);
    // The counter: tiled front, marble top.
    cairo_rectangle(cr, 0, 560, 1280, 160);
    setRgbaC(cr, rgb(232, 226, 214), 1.0);
    cairo_fill(cr);
    for (int x = 0; x < 20; ++x)
    {
      cairo_rectangle(cr, x * 72, 590, 2, 130);
      setRgbaC(cr, rgb(180, 172, 160), 1.0);
      cairo_fill(cr);
    }
    cairo_rectangle(cr, 0, 548, 1280, 28);
    cairo_pattern_t* mg = cairo_pattern_create_linear(0, 548, 0, 576);
    cairo_pattern_add_color_stop_rgb(mg, 0, 1, 1, 1);
    cairo_pattern_add_color_stop_rgb(mg, 1, 0.75, 0.74, 0.72);
    cairo_set_source(cr, mg);
    cairo_fill(cr);
    cairo_pattern_destroy(mg);
    cairo_rectangle(cr, 0, 576, 1280, 6);
    setRgbaC(cr, rgb(200, 206, 216), 1.0);
    cairo_fill(cr);
    // The stock pot on the counter, right.
    roundedRect(cr, 960, 420, 200, 132, 10);
    cairo_pattern_t* pg = cairo_pattern_create_linear(960, 0, 1160, 0);
    cairo_pattern_add_color_stop_rgb(pg, 0, 0.6, 0.64, 0.7);
    cairo_pattern_add_color_stop_rgb(pg, 0.3, 0.97, 0.98, 1.0);
    cairo_pattern_add_color_stop_rgb(pg, 1, 0.45, 0.5, 0.56);
    cairo_set_source(cr, pg);
    cairo_fill(cr);
    cairo_pattern_destroy(pg);
    roundedRect(cr, 950, 412, 220, 16, 8);
    setRgbaC(cr, rgb(230, 234, 240), 1.0);
    cairo_fill(cr);
    // The lighting grid and its cones over everything.
    cairo_rectangle(cr, 0, 40, 1280, 10);
    setRgbaC(cr, rgb(40, 36, 44), 1.0);
    cairo_fill(cr);
    for (int i = 0; i < 8; ++i)
    {
      const double lx = 80 + i * 160;
      cairo_move_to(cr, lx - 12, 70);
      cairo_line_to(cr, lx - 150, 720);
      cairo_line_to(cr, lx + 150, 720);
      cairo_line_to(cr, lx + 12, 70);
      cairo_close_path(cr);
      cairo_pattern_t* lg = cairo_pattern_create_linear(0, 70, 0, 720);
      cairo_pattern_add_color_stop_rgba(lg, 0, 1, 0.95, 0.8, 0.22);
      cairo_pattern_add_color_stop_rgba(lg, 1, 1, 0.95, 0.8, 0.0);
      cairo_set_source(cr, lg);
      cairo_fill(cr);
      cairo_pattern_destroy(lg);
      roundedRect(cr, lx - 14, 46, 28, 26, 4);
      setRgbaC(cr, rgb(20, 18, 24), 1.0);
      cairo_fill(cr);
      radialGlow(cr, lx, 72, 26, rgb(255, 240, 200), 0.9);
    }
  });
}

// Lance in a chef's hat: pose 9 (the mic up), a tall toque on his quiff.
const Texture& lanceChef(ClipKit& k, bool mouth)
{
  const std::string key = std::string("e4_lance_chef") + (mouth ? "o" : "c");
  // Design coords plus 40 px of headroom over the hat (feet at (120, 480)).
  return cached(k, key, 320, 600, 160, 580, [&](cairo_t* cr) {
    cairo_translate(cr, 40, 100);
    paintLance(cr, 9, mouth);
    paintChefHat(cr, 132, 96, 96, 120, -0.06);
  });
}

// The big wooden spoon, its bowl's middle at (110, 70) and the handle down
// to (110, 700); 220 x 720.
const Texture& woodenSpoon(ClipKit& k)
{
  return cached(k, "e4_wooden_spoon", 220, 720, 110, 70, [](cairo_t* cr) {
    cairo_pattern_t* g = cairo_pattern_create_linear(80, 0, 140, 0);
    cairo_pattern_add_color_stop_rgb(g, 0, 0.62, 0.4, 0.2);
    cairo_pattern_add_color_stop_rgb(g, 0.4, 0.9, 0.7, 0.44);
    cairo_pattern_add_color_stop_rgb(g, 1, 0.5, 0.3, 0.14);
    roundedRect(cr, 96, 110, 28, 600, 14);
    cairo_set_source(cr, g);
    cairo_fill_preserve(cr);
    setRgbaC(cr, kInk, 1.0);
    cairo_set_line_width(cr, 3);
    cairo_stroke(cr);
    // The bowl: a broad oval, seen from a little above.
    cairo_new_path(cr);
    cairo_save(cr);
    cairo_translate(cr, 110, 70);
    cairo_scale(cr, 104, 48);
    cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
    cairo_restore(cr);
    cairo_set_source(cr, g);
    cairo_fill_preserve(cr);
    setRgbaC(cr, kInk, 1.0);
    cairo_set_line_width(cr, 3);
    cairo_stroke(cr);
    cairo_new_path(cr);
    cairo_save(cr);
    cairo_translate(cr, 110, 64);
    cairo_scale(cr, 88, 32);
    cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
    cairo_restore(cr);
    setRgbaC(cr, rgb(120, 70, 30), 0.5);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
    // Grain.
    for (int i = 0; i < 5; ++i)
    {
      cairo_move_to(cr, 102 + i * 4, 130);
      cairo_line_to(cr, 104 + i * 3, 690);
    }
    setRgbaC(cr, rgb(110, 66, 30), 0.35);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);
  });
}

// Shot: Lance on the cooking show. The kitchen set under the lights,
// zooming in slowly; Lance in his toque; a giant wooden spoon rises from
// below the frame (frames 0-5) with the three runners on it, tiny, waving
// (4-frame loop); steam rolls up through the foreground (8-frame loop).
void lanceChefClip(ClipKit& k, int frame, float t, int ticks, float ox, float oy)
{
  // The shot's own frame across repeats (90 ms a frame), for the zoom and
  // the spoon's rise, which happen once.
  const float abs = float(ticks) / 5.4f;
  DrawOpts zo;
  zo.scale = (1280.0f + 2.0f * abs) / 1280.0f;
  k.r.draw(kitchenSet(k), 640.0f + ox, 360.0f + oy, zo);
  // Lance.
  const bool talking = t > 0.12f && t < 0.6f && (ticks / 7) % 2 == 0;
  const float lx = 380.0f + ox, ly = 720.0f + oy + std::sin(float(ticks) * 0.06f) * 2.0f;
  drawGlow(k.r, k.art, lx, ly - 320.0f, 300.0f, rgb(255, 230, 180), 0.25f);
  k.r.draw(lanceChef(k, talking), lx, ly);
  // The spoon rising from below, eased out, then bobbing.
  const float rise = std::min(1.0f, abs / 6.0f);
  const float e = 1.0f - (1.0f - rise) * (1.0f - rise);
  const float bx = 840.0f + ox, by = 400.0f + (1.0f - e) * 520.0f + oy + (rise >= 1.0f ? std::sin(float(ticks) * 0.08f) * 6.0f : 0.0f);
  // The spoon, then the three runners standing in its bowl, waving.
  k.r.draw(woodenSpoon(k), bx, by);
  static constexpr float kWave[4] = {-0.9f, -0.4f, 0.1f, -0.4f};
  for (int who = 0; who < 3; ++who)
  {
    const float rx = bx - 56.0f + float(who) * 56.0f, ry = by + 16.0f;
    const float a = kWave[(frame + who) % 4] - 1.57f;
    const float shx = rx + (who == 2 ? -10.0f : 10.0f), shy = ry - 52.0f;
    k.r.draw(runner(k, who, 0, 0.9f, who == 2), rx, ry);
    k.r.drawLine(shx, shy, shx + std::cos(a) * 26.0f, shy + std::sin(a) * 26.0f, 7.0f, kInk);
    k.r.drawLine(shx, shy, shx + std::cos(a) * 26.0f, shy + std::sin(a) * 26.0f, 4.0f, rgb(246, 206, 170));
  }
  // The spoon's front lip over their feet.
  k.r.fillRect(bx - 84.0f, by + 12.0f, 168.0f, 8.0f, rgba(170, 112, 56, 235));
  k.r.fillRect(bx - 84.0f, by + 12.0f, 168.0f, 2.0f, rgba(230, 180, 120, 200));
  if (abs < 7.0f && abs >= 5.5f)
    drawGlow(k.r, k.art, bx, by, 160.0f, rgb(255, 250, 220), 0.5f);
  // Steam rolling up in front.
  for (int i = 0; i < 6; ++i)
  {
    const float u = std::fmod(float(frame % 8) / 8.0f + float(i) / 6.0f, 1.0f);
    const float sx = 120.0f + float(i) * 210.0f + std::sin(u * 6.0f + float(i)) * 40.0f;
    drawGlow(k.r, k.art, sx + ox, 760.0f - u * 420.0f + oy, 90.0f + u * 80.0f, rgb(255, 255, 255), 0.3f * std::sin(u * 3.14f));
  }
}

// --- Level 25: Tiny Kingdom (Channel 31, Nature) ------------------------------------

// A big leaf lit from behind by golden light, 1280 x 720: the low sun
// glowing through it, its midrib and veins brighter than the blade, the
// gold showing round its edges and bokeh floating over them.
const Texture& goldenLeaf(ClipKit& k)
{
  return cached(k, "e5_golden_leaf", 1280, 720, 0, 0, [](cairo_t* cr) {
    cairo_pattern_t* sky = cairo_pattern_create_radial(1000, 140, 30, 760, 360, 980);
    cairo_pattern_add_color_stop_rgb(sky, 0.0, 1.0, 0.96, 0.78);
    cairo_pattern_add_color_stop_rgb(sky, 0.35, 0.95, 0.72, 0.3);
    cairo_pattern_add_color_stop_rgb(sky, 1.0, 0.36, 0.24, 0.08);
    cairo_rectangle(cr, 0, 0, 1280, 720);
    cairo_set_source(cr, sky);
    cairo_fill(cr);
    cairo_pattern_destroy(sky);
    // The leaf, tip to the upper right.
    cairo_save(cr);
    cairo_translate(cr, 640, 380);
    cairo_rotate(cr, -0.36);
    auto leaf = [cr]() {
      cairo_move_to(cr, -720, 30);
      cairo_curve_to(cr, -460, -330, 330, -350, 700, -10);
      cairo_curve_to(cr, 330, 300, -440, 330, -720, 30);
      cairo_close_path(cr);
    };
    leaf();
    cairo_pattern_t* body = cairo_pattern_create_radial(420, -120, 40, 100, 0, 900);
    cairo_pattern_add_color_stop_rgb(body, 0.0, 0.92, 0.96, 0.46);
    cairo_pattern_add_color_stop_rgb(body, 0.4, 0.58, 0.8, 0.22);
    cairo_pattern_add_color_stop_rgb(body, 1.0, 0.2, 0.4, 0.1);
    cairo_set_source(cr, body);
    cairo_fill(cr);
    cairo_pattern_destroy(body);
    // Veins: the midrib, the side veins curving to the tip, a fine net.
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    for (int i = -9; i <= 8; ++i)
    {
      const double x = double(i) * 70.0 + 20.0;
      for (const double side : {-1.0, 1.0})
      {
        const double reach = 250.0 * (1.0 - std::pow(std::abs(x - 10.0) / 720.0, 2.0));
        cairo_move_to(cr, x, side * 2.0);
        cairo_curve_to(cr, x + 40, side * reach * 0.4, x + 110, side * reach * 0.8, x + 170, side * reach * 0.95);
      }
    }
    setRgbaC(cr, rgb(244, 252, 170), 0.55);
    cairo_set_line_width(cr, 3.0);
    cairo_stroke(cr);
    std::uint32_t sd = 7121u;
    for (int i = 0; i < 260; ++i)
    {
      sd = sd * 1664525u + 1013904223u;
      const double x = -650.0 + double(sd >> 8) / double(1u << 24) * 1300.0;
      sd = sd * 1664525u + 1013904223u;
      const double y = -240.0 + double(sd >> 8) / double(1u << 24) * 480.0;
      cairo_move_to(cr, x, y);
      cairo_rel_line_to(cr, double(int(sd % 31u)) - 15.0, double(int((sd >> 5) % 31u)) - 15.0);
    }
    setRgbaC(cr, rgb(230, 250, 150), 0.18);
    cairo_set_line_width(cr, 1.2);
    cairo_stroke(cr);
    cairo_move_to(cr, -720, 30);
    cairo_curve_to(cr, -300, 0, 300, -10, 700, -10);
    setRgbaC(cr, rgb(255, 255, 200), 0.85);
    cairo_set_line_width(cr, 8.0);
    cairo_stroke(cr);
    // The sun through its rim.
    leaf();
    setRgbaC(cr, rgb(255, 230, 140), 0.45);
    cairo_set_line_width(cr, 14.0);
    cairo_stroke(cr);
    leaf();
    setRgbaC(cr, rgb(90, 120, 30), 0.8);
    cairo_set_line_width(cr, 2.5);
    cairo_stroke(cr);
    cairo_restore(cr);
    // Bokeh.
    for (int i = 0; i < 34; ++i)
    {
      sd = sd * 1664525u + 1013904223u;
      const double x = double(sd >> 8) / double(1u << 24) * 1280.0;
      sd = sd * 1664525u + 1013904223u;
      const double y = double(sd >> 8) / double(1u << 24) * 720.0;
      const double rr = 14.0 + double(sd % 40u);
      cairo_arc(cr, x, y, rr, 0, 2 * kPi);
      setRgbaC(cr, rgb(255, 236, 170), 0.08 + double(sd % 7u) * 0.025);
      cairo_fill(cr);
    }
    cairo_pattern_t* vig = cairo_pattern_create_radial(640, 360, 300, 640, 360, 820);
    cairo_pattern_add_color_stop_rgba(vig, 0, 0, 0, 0, 0);
    cairo_pattern_add_color_stop_rgba(vig, 1, 0.1, 0.05, 0, 0.6);
    cairo_rectangle(cr, 0, 0, 1280, 720);
    cairo_set_source(cr, vig);
    cairo_fill(cr);
    cairo_pattern_destroy(vig);
  });
}

// "CH 31" in a thin, high-contrast serif, drawn stem by stem and serif by
// serif (no serif face is on every platform). Caps 150 units tall on a
// baseline at y = 150, 545 units wide from x = 0.
void ch31Glyphs(cairo_t* cr, Color c, double alpha)
{
  setRgbaC(cr, c, alpha);
  const double hair = 3.6, stem = 11.0;
  auto box = [cr](double x, double y, double w, double h) { cairo_rectangle(cr, x, y, w, h); };
  // A stem's serif: a hairline slab with little brackets into the stem.
  auto serif = [cr, hair, stem](double sx, double y, bool top) {
    cairo_rectangle(cr, sx - 12.0, top ? y : y - hair, stem + 24.0, hair);
    const double dy = top ? 1.0 : -1.0, y0 = top ? y + hair : y - hair;
    cairo_move_to(cr, sx - 7.0, y0);
    cairo_curve_to(cr, sx - 1.0, y0, sx, y0 + dy * 2.0, sx, y0 + dy * 9.0);
    cairo_line_to(cr, sx, y0);
    cairo_close_path(cr);
    cairo_move_to(cr, sx + stem + 7.0, y0);
    cairo_curve_to(cr, sx + stem + 1.0, y0, sx + stem, y0 + dy * 2.0, sx + stem, y0 + dy * 9.0);
    cairo_line_to(cr, sx + stem, y0);
    cairo_close_path(cr);
  };
  // C: a ring thick on the left, hairline top and bottom, open on the
  // right, with beak serifs at its tips.
  cairo_save(cr);
  cairo_move_to(cr, 70, 75);
  cairo_line_to(cr, 70 + 220 * std::cos(-0.85), 75 + 220 * std::sin(-0.85));
  cairo_line_to(cr, -60, -120);
  cairo_line_to(cr, -60, 270);
  cairo_line_to(cr, 70 + 220 * std::cos(0.85), 75 + 220 * std::sin(0.85));
  cairo_close_path(cr);
  cairo_clip(cr);
  cairo_save(cr);
  cairo_translate(cr, 70, 75);
  cairo_scale(cr, 68, 77);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_restore(cr);
  cairo_new_sub_path(cr);
  cairo_save(cr);
  cairo_translate(cr, 76, 75);
  cairo_scale(cr, 60, 73.5);
  cairo_arc_negative(cr, 0, 0, 1, 0, -2 * kPi);
  cairo_restore(cr);
  cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
  cairo_fill(cr);
  cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
  cairo_restore(cr);
  setRgbaC(cr, c, alpha);
  box(70 + 68 * std::cos(-0.85) - 4.0, 75 - 77 * std::sin(0.85) - 2.0, 4.6, 30);
  box(70 + 68 * std::cos(0.85) - 4.0, 75 + 77 * std::sin(0.85) - 28.0, 4.6, 30);
  cairo_fill(cr);
  // H.
  const double hx = 160;
  box(hx + 10, 0, stem, 150);
  box(hx + 99, 0, stem, 150);
  box(hx + 21, 72, 78, hair);
  serif(hx + 10, 0, true);
  serif(hx + 10, 150, false);
  serif(hx + 99, 0, true);
  serif(hx + 99, 150, false);
  cairo_fill(cr);
  // 3: two bowls, hairline on the left, swelling on the right, ball
  // terminals at both ends.
  const double tx = 335;
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
  cairo_arc(cr, tx + 55, 40, 36, -kPi * 0.92, kPi * 0.5);
  cairo_arc(cr, tx + 55, 113, 39, -kPi * 0.5, kPi * 0.9);
  cairo_set_line_width(cr, hair);
  cairo_stroke(cr);
  cairo_arc(cr, tx + 55, 40, 36, -kPi * 0.3, kPi * 0.35);
  cairo_new_sub_path(cr);
  cairo_arc(cr, tx + 55, 113, 39, -kPi * 0.3, kPi * 0.4);
  cairo_set_line_width(cr, 10.0);
  cairo_stroke(cr);
  box(tx + 38, 74.2, 18, hair);
  cairo_fill(cr);
  cairo_arc(cr, tx + 55 + 36 * std::cos(-kPi * 0.92) + 5, 40 + 36 * std::sin(-kPi * 0.92) + 3, 7.5, 0, 2 * kPi);
  cairo_fill(cr);
  cairo_arc(cr, tx + 55 + 39 * std::cos(kPi * 0.9) + 5, 113 + 39 * std::sin(kPi * 0.9) - 3, 7.5, 0, 2 * kPi);
  cairo_fill(cr);
  // 1: a stem with a hairline flag and a slab foot.
  const double ox = 460;
  box(ox + 44, 0, stem, 150);
  serif(ox + 44, 150, false);
  cairo_fill(cr);
  cairo_move_to(cr, ox + 46, 2);
  cairo_line_to(cr, ox + 16, 26);
  cairo_set_line_width(cr, hair);
  cairo_stroke(cr);
}

// The ident's numerals, 900 x 300, centred.
const Texture& ch31Title(ClipKit& k)
{
  return cached(k, "e5_ch31", 900, 300, 450, 150, [](cairo_t* cr) {
    cairo_translate(cr, 450 - 545 * 0.7, 150 - 75 * 1.4);
    cairo_scale(cr, 1.4, 1.4);
    // A soft shadow under the ivory, then the letters.
    for (int i = 3; i >= 1; --i)
    {
      cairo_save(cr);
      cairo_translate(cr, 2.0 * i, 2.5 * i);
      ch31Glyphs(cr, rgb(40, 24, 4), 0.14);
      cairo_restore(cr);
    }
    ch31Glyphs(cr, rgb(255, 250, 232), 1.0);
  });
}

// "NATURE" grown in vine, 1000 x 260, centred: the letters' strokes twisted
// green stems with leaves sprouting along them, an underline stem running
// through the bottoms they all grow from.
const Texture& natureVine(ClipKit& k)
{
  return cached(k, "e5_nature_vine", 1000, 260, 500, 130, [](cairo_t* cr) {
    selectGameFont(cr, false);
    cairo_set_font_size(cr, 132);
    cairo_text_extents_t te;
    cairo_text_extents(cr, "NATURE", &te);
    const double x0 = 500 - te.x_advance / 2, base = 182;
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    // The runner stem along the baseline, waving.
    cairo_move_to(cr, x0 - 40, base + 14);
    for (int i = 0; i < 8; ++i)
    {
      const double xa = x0 - 40 + (te.x_advance + 80) * double(i) / 8.0, xb = xa + (te.x_advance + 80) / 8.0;
      cairo_curve_to(cr, xa + 25, base + (i % 2 ? 26 : 2), xb - 25, base + (i % 2 ? 26 : 2), xb, base + 14);
    }
    setRgbaC(cr, rgb(40, 74, 22), 1.0);
    cairo_set_line_width(cr, 10);
    cairo_stroke_preserve(cr);
    setRgbaC(cr, rgb(120, 176, 60), 1.0);
    cairo_set_line_width(cr, 5);
    cairo_stroke(cr);
    // The letters: dark stem, green body, a light twist along them.
    cairo_move_to(cr, x0, base);
    cairo_text_path(cr, "NATURE");
    cairo_path_t* glyphs = cairo_copy_path(cr);
    setRgbaC(cr, rgb(30, 60, 18), 1.0);
    cairo_set_line_width(cr, 11);
    cairo_stroke_preserve(cr);
    cairo_pattern_t* lg = cairo_pattern_create_linear(0, base - 100, 0, base);
    cairo_pattern_add_color_stop_rgb(lg, 0, 0.62, 0.84, 0.3);
    cairo_pattern_add_color_stop_rgb(lg, 1, 0.26, 0.5, 0.14);
    cairo_set_source(cr, lg);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(lg);
    const double dash[2] = {6.0, 9.0};
    cairo_set_dash(cr, dash, 2, 0.0);
    setRgbaC(cr, rgb(200, 236, 130), 0.9);
    cairo_set_line_width(cr, 3);
    cairo_stroke(cr);
    cairo_set_dash(cr, nullptr, 0, 0.0);
    // Leaves along the strokes, alternating sides.
    cairo_new_path(cr);
    cairo_append_path(cr, glyphs);
    cairo_path_t* flat = cairo_copy_path_flat(cr);
    cairo_new_path(cr);
    double run = 0.0, px = 0.0, py = 0.0;
    int n = 0;
    for (int i = 0; i < flat->num_data; i += flat->data[i].header.length)
    {
      const cairo_path_data_t& d = flat->data[i];
      if (d.header.type == CAIRO_PATH_MOVE_TO)
      {
        px = flat->data[i + 1].point.x;
        py = flat->data[i + 1].point.y;
        continue;
      }
      if (d.header.type != CAIRO_PATH_LINE_TO)
        continue;
      const double qx = flat->data[i + 1].point.x, qy = flat->data[i + 1].point.y;
      run += std::hypot(qx - px, qy - py);
      if (run > 46.0)
      {
        run = 0.0;
        const double ang = std::atan2(qy - py, qx - px) + (n % 2 ? 1.1 : -1.1);
        ++n;
        cairo_save(cr);
        cairo_translate(cr, qx, qy);
        cairo_rotate(cr, ang);
        cairo_move_to(cr, 0, 0);
        cairo_curve_to(cr, 6, -9, 18, -8, 24, 0);
        cairo_curve_to(cr, 18, 8, 6, 9, 0, 0);
        cairo_restore(cr);
        setRgbaC(cr, n % 3 ? rgb(134, 198, 70) : rgb(170, 220, 90), 1.0);
        cairo_fill_preserve(cr);
        setRgbaC(cr, rgb(30, 60, 18), 1.0);
        cairo_set_line_width(cr, 1.6);
        cairo_stroke(cr);
      }
      px = qx;
      py = qy;
    }
    cairo_path_destroy(flat);
    cairo_path_destroy(glyphs);
  });
}

// The vine's growing tip: a tendril curled into a spiral, (0, 0) at its base.
const Texture& vineTip(ClipKit& k)
{
  return cached(k, "e5_vine_tip", 80, 80, 10, 60, [](cairo_t* cr) {
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_move_to(cr, 10, 60);
    double a = kPi, rr = 24.0;
    for (int i = 0; i < 40; ++i)
    {
      a += 0.22;
      rr *= 0.95;
      cairo_line_to(cr, 40 + std::cos(a) * rr, 36 + std::sin(a) * rr);
    }
    setRgbaC(cr, rgb(30, 60, 18), 1.0);
    cairo_set_line_width(cr, 5);
    cairo_stroke_preserve(cr);
    setRgbaC(cr, rgb(150, 210, 80), 1.0);
    cairo_set_line_width(cr, 2.5);
    cairo_stroke(cr);
  });
}

// Shot: the Channel 31 ident. A leaf in golden light; "CH 31" fades up in a
// thin serif over frames 0-5; "NATURE" grows on as a vine, left to right,
// over frames 6-11.
void identCh31(ClipKit& k, int ticks, float ox, float oy)
{
  // The shot's frame, smoothly (90 ms a frame).
  const float abs = float(ticks) / 5.4f;
  k.r.draw(goldenLeaf(k), ox, oy);
  const float up = std::clamp(abs / 6.0f, 0.0f, 1.0f);
  DrawOpts to;
  to.alpha = up * up * (3.0f - 2.0f * up);
  to.scale = 1.0f + 0.04f * (1.0f - up);
  drawGlow(k.r, k.art, 640.0f + ox, 250.0f + oy, 420.0f, rgb(255, 230, 160), 0.25f * to.alpha);
  k.r.draw(ch31Title(k), 640.0f + ox, 250.0f + oy - 8.0f * (1.0f - up), to);
  const float u = std::clamp((abs - 6.0f) / 5.0f, 0.0f, 1.0f);
  if (u > 0.0f)
  {
    const float wx = 640.0f + ox, wy = 520.0f + oy;
    const float left = wx - 470.0f, right = left + 940.0f * u;
    clipTo(k, 0.0f, 0.0f, right, H);
    k.r.draw(natureVine(k), wx, wy);
    unclip(k);
    if (u < 1.0f)
    {
      DrawOpts tip;
      tip.angle = std::sin(float(ticks) * 0.5f) * 12.0f;
      k.r.draw(vineTip(k), right, wy + 64.0f, tip);
    }
  }
  k.r.drawText("MAXTV  CHANNEL 31  NATURE", 640 + ox, 676 + oy, {18.0f, rgb(255, 246, 220), rgb(70, 50, 10)}, Align::Center);
  scanlines(k, 30);
}

// The garden at ant height, 1400 x 720 (wider than the screen for the pan):
// golden air thick with bokeh, giant soft blades and flower heads far off,
// moss and loam along the bottom.
const Texture& bokehGarden(ClipKit& k)
{
  return cached(k, "e5_bokeh_garden", 1400, 720, 0, 0, [](cairo_t* cr) {
    gradient(cr, 1400, 720, rgb(250, 226, 160), rgb(206, 196, 110), rgb(70, 104, 44));
    std::uint32_t sd = 3301u;
    auto rnd = [&sd](double a, double b) {
      sd = sd * 1664525u + 1013904223u;
      return a + (b - a) * double(sd >> 8) / double(1u << 24);
    };
    radialGlow(cr, 1100, 120, 520, rgb(255, 244, 200), 0.7);
    // Far blades, soft and pale.
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    for (int i = 0; i < 40; ++i)
    {
      const double x = rnd(-40, 1440), top = rnd(60, 380), lean = rnd(-90, 90);
      cairo_move_to(cr, x - 16, 720);
      cairo_curve_to(cr, x - 10, 500, x + lean * 0.5, top + 80, x + lean, top);
      cairo_curve_to(cr, x + lean * 0.5 + 6, top + 90, x + 10, 500, x + 16, 720);
      cairo_close_path(cr);
      setRgbaC(cr, lerpColor(rgb(150, 180, 80), rgb(240, 220, 150), float(rnd(0.0, 0.6))), rnd(0.25, 0.5));
      cairo_fill(cr);
    }
    // Flower heads, out of focus.
    static const Color kHeads[4] = {rgb(255, 130, 110), rgb(255, 250, 240), rgb(170, 150, 255), rgb(255, 210, 90)};
    for (int i = 0; i < 9; ++i)
    {
      const double x = rnd(40, 1360), y = rnd(120, 360), rr = rnd(30, 70);
      radialGlow(cr, x, y, rr * 1.6, kHeads[i % 4], 0.55);
      cairo_arc(cr, x, y, rr * 0.25, 0, 2 * kPi);
      setRgbaC(cr, rgb(250, 200, 60), 0.5);
      cairo_fill(cr);
    }
    // Bokeh.
    for (int i = 0; i < 70; ++i)
    {
      const double x = rnd(0, 1400), y = rnd(0, 560), rr = rnd(8, 40);
      cairo_arc(cr, x, y, rr, 0, 2 * kPi);
      setRgbaC(cr, rgb(255, 246, 200), rnd(0.08, 0.26));
      cairo_fill_preserve(cr);
      setRgbaC(cr, rgb(255, 255, 230), 0.12);
      cairo_set_line_width(cr, 1.5);
      cairo_stroke(cr);
    }
    // The ground: moss over loam, with pebbles the size of boulders.
    cairo_move_to(cr, 0, 500);
    for (int x = 0; x <= 1400; x += 50)
      cairo_line_to(cr, x, 496 + std::sin(x * 0.02) * 6 + rnd(-3, 3));
    cairo_line_to(cr, 1400, 720);
    cairo_line_to(cr, 0, 720);
    cairo_close_path(cr);
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 490, 0, 720);
    cairo_pattern_add_color_stop_rgb(g, 0, 0.42, 0.56, 0.2);
    cairo_pattern_add_color_stop_rgb(g, 0.25, 0.26, 0.32, 0.12);
    cairo_pattern_add_color_stop_rgb(g, 1, 0.16, 0.11, 0.06);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
    for (int i = 0; i < 7; ++i)
    {
      const double x = rnd(40, 1360), rx = rnd(30, 70);
      cairo_save(cr);
      cairo_translate(cr, x, 590);
      cairo_scale(cr, rx, rx * 0.45);
      cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
      cairo_restore(cr);
      cairo_pattern_t* pg = cairo_pattern_create_radial(x - rx * 0.3, 574, 4, x, 590, rx);
      cairo_pattern_add_color_stop_rgb(pg, 0, 0.78, 0.74, 0.66);
      cairo_pattern_add_color_stop_rgb(pg, 1, 0.36, 0.32, 0.28);
      cairo_set_source(cr, pg);
      cairo_fill(cr);
      cairo_pattern_destroy(pg);
    }
  });
}

// A grass blade, `h` px tall, anchored at its base's middle; rim-lit gold
// on its sunny edge. `dark` for the out-of-focus foreground one.
const Texture& habitatBlade(ClipKit& k, int h, int lean, bool dark)
{
  const std::string key = "e5_blade" + std::to_string(h) + "_" + std::to_string(lean) + (dark ? "d" : "");
  const int w = 140 + std::abs(lean);
  const double bx = lean < 0 ? 70.0 - lean : 70.0;
  return cached(k, key, w, h + 10, float(bx), float(h + 4), [h, lean, bx, dark](cairo_t* cr) {
    const double base = h + 4, top = 6, tipX = bx + lean;
    cairo_move_to(cr, bx - 20, base);
    cairo_curve_to(cr, bx - 18, base - h * 0.5, tipX - lean * 0.4 - 8, top + h * 0.25, tipX, top);
    cairo_curve_to(cr, tipX - lean * 0.4 + 10, top + h * 0.3, bx + 18, base - h * 0.45, bx + 20, base);
    cairo_close_path(cr);
    cairo_pattern_t* g = cairo_pattern_create_linear(bx - 22, 0, bx + 22, 0);
    if (dark)
    {
      cairo_pattern_add_color_stop_rgb(g, 0, 0.1, 0.2, 0.06);
      cairo_pattern_add_color_stop_rgb(g, 1, 0.24, 0.38, 0.12);
    }
    else
    {
      cairo_pattern_add_color_stop_rgb(g, 0, 0.24, 0.46, 0.14);
      cairo_pattern_add_color_stop_rgb(g, 0.6, 0.5, 0.72, 0.24);
      cairo_pattern_add_color_stop_rgb(g, 1, 0.36, 0.58, 0.18);
    }
    cairo_set_source(cr, g);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(g);
    setRgbaC(cr, dark ? rgb(10, 20, 6) : rgb(34, 70, 20), 0.9);
    cairo_set_line_width(cr, 2.0);
    cairo_stroke(cr);
    // The midrib and the gold on the sunny edge.
    cairo_move_to(cr, bx, base);
    cairo_curve_to(cr, bx, base - h * 0.5, tipX - lean * 0.4, top + h * 0.28, tipX, top + 4);
    setRgbaC(cr, dark ? rgb(80, 110, 40) : rgb(210, 236, 140), dark ? 0.35 : 0.6);
    cairo_set_line_width(cr, 2.5);
    cairo_stroke(cr);
    cairo_move_to(cr, bx + 19, base);
    cairo_curve_to(cr, bx + 17, base - h * 0.45, tipX - lean * 0.4 + 9, top + h * 0.3, tipX, top);
    setRgbaC(cr, rgb(255, 226, 130), dark ? 0.25 : 0.8);
    cairo_set_line_width(cr, 3.0);
    cairo_stroke(cr);
  });
}

// A dew drop, 40 px, the garden upside down in it and a glint on top.
const Texture& dewDropBig(ClipKit& k)
{
  return cached(k, "e5_dew", 60, 70, 30, 40, [](cairo_t* cr) {
    cairo_move_to(cr, 30, 8);
    cairo_curve_to(cr, 40, 22, 50, 32, 50, 44);
    cairo_curve_to(cr, 50, 58, 40, 64, 30, 64);
    cairo_curve_to(cr, 20, 64, 10, 58, 10, 44);
    cairo_curve_to(cr, 10, 32, 20, 22, 30, 8);
    cairo_close_path(cr);
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 20, 0, 64);
    cairo_pattern_add_color_stop_rgba(g, 0, 0.5, 0.7, 0.3, 0.55);
    cairo_pattern_add_color_stop_rgba(g, 0.55, 0.98, 0.9, 0.62, 0.7);
    cairo_pattern_add_color_stop_rgba(g, 1, 0.5, 0.66, 0.26, 0.8);
    cairo_set_source(cr, g);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(g);
    setRgbaC(cr, rgb(255, 255, 240), 0.7);
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);
    cairo_arc(cr, 23, 38, 5, 0, 2 * kPi);
    setRgbaC(cr, rgb(255, 255, 255), 0.95);
    cairo_fill(cr);
    cairo_arc(cr, 36, 56, 3, 0, 2 * kPi);
    setRgbaC(cr, rgb(255, 255, 230), 0.6);
    cairo_fill(cr);
  });
}

// Shot: the runners in their natural habitat. The bokeh garden panning
// right 1 px a frame; grass blades between them swaying (8-frame loop);
// the three runners tiny at their feet, Rocco scratching his head (4-frame
// loop); in front, a dew drop sliding down a blade (16 frames).
void runnersHabitat(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  // The shot's frame, smoothly (100 ms a frame), across the repeats.
  const float abs = float(ticks) / 6.0f;
  const float pan = abs;
  k.r.draw(bokehGarden(k), ox - pan, oy);
  drawGlow(k.r, k.art, 1040.0f + ox, 110.0f + oy, 520.0f, rgb(255, 230, 160), 0.3f);
  // The blades, swaying on an 8-frame loop, each a little out of phase:
  // three shapes, each baked bent for the eight frames of the sway (a
  // rotated texture would step its edges).
  struct Shape
  {
    int h, lean;
  };
  static constexpr Shape kShapes[3] = {{470, -60}, {410, 40}, {250, 30}};
  struct Blade
  {
    float x;
    int shape, phase;
    bool front;
  };
  static constexpr Blade kBlades[] = {{110, 0, 0, false}, {300, 1, 2, false}, {430, 0, 5, false},
    {560, 2, 1, true}, {820, 1, 6, false}, {890, 2, 4, true}, {990, 0, 3, false}, {1190, 1, 7, false}};
  const float groundY = 540.0f + oy;
  auto blade = [&](const Blade& b) {
    const Shape& sh = kShapes[b.shape];
    const int f = (frame + b.phase) % 8;
    const int bend = int(std::lround(std::sin(float(2.0 * kPi) * float(f) / 8.0f) * float(sh.h) * 0.045f));
    k.r.draw(habitatBlade(k, sh.h, sh.lean + bend, false), b.x + ox - pan * 1.4f, groundY + 8.0f);
  };
  for (const Blade& b : kBlades)
    if (!b.front)
      blade(b);
  // The runners, tiny, standing in the moss.
  const float s = 0.6f, gy = groundY;
  const float rx[3] = {600.0f, 700.0f, 806.0f};
  for (int who = 0; who < 3; ++who)
  {
    const float x = rx[who] + ox - pan * 1.4f;
    k.r.fillRect(x - 22.0f, gy - 2.0f, 44.0f, 5.0f, rgba(20, 30, 10, 60));
    k.r.draw(runner(k, who, who == 2 ? 3 : (frame % 8 < 4 ? 0 : 8), s, who == 2), x, gy);
  }
  {
    // Rocco's free hand scratching the top of his head, round and round.
    static constexpr float kScratch[4][2] = {{-3.0f, -1.0f}, {0.0f, -3.0f}, {3.0f, -1.0f}, {0.0f, 0.5f}};
    const float x = rx[1] + ox - pan * 1.4f;
    const float shx = x + 4.0f, shy = gy - 68.0f * s;
    const float hx = x + 1.0f + kScratch[frame % 4][0], hy = gy - 96.0f * s + kScratch[frame % 4][1];
    const float ex = x + 15.0f, ey = gy - 86.0f * s;
    k.r.drawLine(shx, shy, ex, ey, 5.0f, kInk);
    k.r.drawLine(ex, ey, hx, hy, 5.0f, kInk);
    k.r.drawLine(shx, shy, ex, ey, 2.8f, rgb(150, 96, 62));
    k.r.drawLine(ex, ey, hx, hy, 2.8f, rgb(150, 96, 62));
  }
  for (const Blade& b : kBlades)
    if (b.front)
      blade(b);
  // The foreground blade, big and soft, and the dew drop sliding down its
  // midrib (the curve the blade was painted along, bent with it).
  constexpr int kFgH = 760, kFgLean = -110;
  const float fx = 1120.0f + ox - pan * 2.0f, fy = 740.0f + oy;
  k.r.draw(habitatBlade(k, kFgH, kFgLean, true), fx, fy);
  const float d = std::fmod(abs, 16.0f) / 16.0f;
  const float v = 0.12f + 0.88f * d * d; // 0 the tip, 1 the base
  const float q = 1.0f - v;              // along the painted curve, base to tip
  const float h = float(kFgH), lean = float(kFgLean);
  auto bez = [q](float p0, float p1, float p2, float p3) {
    const float m = 1.0f - q;
    return m * m * m * p0 + 3.0f * m * m * q * p1 + 3.0f * m * q * q * p2 + q * q * q * p3;
  };
  const float bxp = bez(0.0f, 0.0f, lean * 0.6f, lean);
  const float byp = bez(0.0f, -h * 0.5f, -h + h * 0.28f + 2.0f, -h + 2.0f);
  const float dx = fx + bxp, dy = fy + byp;
  DrawOpts dop;
  dop.alpha = std::min(1.0f, d * 8.0f) * std::min(1.0f, (1.0f - d) * 6.0f);
  drawGlow(k.r, k.art, dx, dy, 40.0f, rgb(255, 250, 220), 0.35f * dop.alpha);
  k.r.draw(dewDropBig(k), dx, dy - 20.0f, dop);
}

// --- Level 26: Kaiju Tonight (Channel 50, Monsters) ---------------------------------

// A Technicolor sky, 1280 x 720: a saturated sunset painted in broad
// strokes, crimson over orange over a yellow band, rays fanning from a sun
// low behind the title, brushy clouds edged in gold.
const Texture& technicolorSky(ClipKit& k)
{
  return cached(k, "e5_technicolor_sky", 1280, 720, 0, 0, [](cairo_t* cr) {
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 0, 0, 720);
    cairo_pattern_add_color_stop_rgb(g, 0.0, 0.32, 0.04, 0.22);
    cairo_pattern_add_color_stop_rgb(g, 0.35, 0.86, 0.16, 0.14);
    cairo_pattern_add_color_stop_rgb(g, 0.68, 1.0, 0.56, 0.12);
    cairo_pattern_add_color_stop_rgb(g, 1.0, 1.0, 0.86, 0.36);
    cairo_rectangle(cr, 0, 0, 1280, 720);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
    // Rays from the sun.
    for (int i = 0; i < 18; ++i)
    {
      const double a0 = kPi + double(i) / 18.0 * kPi, a1 = a0 + kPi / 36.0;
      cairo_move_to(cr, 640, 420);
      cairo_line_to(cr, 640 + std::cos(a0) * 1400, 420 + std::sin(a0) * 1400);
      cairo_line_to(cr, 640 + std::cos(a1) * 1400, 420 + std::sin(a1) * 1400);
      cairo_close_path(cr);
      setRgbaC(cr, rgb(255, 230, 140), 0.12);
      cairo_fill(cr);
    }
    radialGlow(cr, 640, 420, 420, rgb(255, 240, 170), 0.85);
    // Brushy clouds: overlapping strokes, dark violet with gold rims.
    std::uint32_t sd = 5011u;
    auto rnd = [&sd](double a, double b) {
      sd = sd * 1664525u + 1013904223u;
      return a + (b - a) * double(sd >> 8) / double(1u << 24);
    };
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    static const double kBanks[5][3] = {{180, 100, 360}, {1080, 130, 320}, {300, 580, 420}, {1000, 600, 380}, {640, 680, 560}};
    for (const auto& b : kBanks)
    {
      // Puffs along the bank: a gold rim lit from below the sun, then the
      // violet body, flat along the bottom.
      double px[9], pr[9];
      for (int i = 0; i < 9; ++i)
      {
        px[i] = b[0] - b[2] * 0.5 + b[2] * (i + 0.5) / 9.0 + rnd(-8, 8);
        pr[i] = (18 + 26 * std::sin(kPi * (i + 0.5) / 9.0)) * rnd(0.8, 1.2);
      }
      cairo_save(cr);
      cairo_rectangle(cr, 0, 0, 1280, b[1] + 8);
      cairo_clip(cr);
      for (int pass = 0; pass < 2; ++pass)
      {
        for (int i = 0; i < 9; ++i)
          cairo_arc(cr, px[i], b[1] - pr[i] * 0.3 + (pass == 0 ? 4 : 0), pr[i] + (pass == 0 ? 3 : 0), 0, 2 * kPi);
        setRgbaC(cr, pass == 0 ? rgb(255, 214, 120) : rgb(120, 34, 74), pass == 0 ? 0.9 : 0.95);
        cairo_fill(cr);
      }
      cairo_restore(cr);
      // Brush strokes across the body.
      for (int st = 0; st < 4; ++st)
      {
        const double y = b[1] - 6 - st * 9, x = b[0] + rnd(-b[2] * 0.3, b[2] * 0.3), len = rnd(50, 120);
        cairo_move_to(cr, x - len * 0.5, y);
        cairo_line_to(cr, x + len * 0.5, y + rnd(-3, 3));
        setRgbaC(cr, rgb(170, 60, 96), 0.5);
        cairo_set_line_width(cr, 4);
        cairo_stroke(cr);
      }
    }
  });
}

// Jagged, flattened copy of the current path: every point pushed off its
// place a little (the letters of a 1950s monster-movie poster).
void jaggedPath(cairo_t* cr, std::uint32_t seed, double amount)
{
  cairo_path_t* flat = cairo_copy_path_flat(cr);
  cairo_new_path(cr);
  std::uint32_t sd = seed;
  auto rnd = [&sd](double a, double b) {
    sd = sd * 1664525u + 1013904223u;
    return a + (b - a) * double(sd >> 8) / double(1u << 24);
  };
  double lx = 0.0, ly = 0.0;
  for (int i = 0; i < flat->num_data; i += flat->data[i].header.length)
  {
    const cairo_path_data_t& d = flat->data[i];
    if (d.header.type == CAIRO_PATH_CLOSE_PATH)
    {
      cairo_close_path(cr);
      continue;
    }
    const double x = flat->data[i + 1].point.x, y = flat->data[i + 1].point.y;
    if (d.header.type == CAIRO_PATH_MOVE_TO)
      cairo_move_to(cr, x + rnd(-amount, amount), y + rnd(-amount, amount));
    else
    {
      // Long edges get a notch in the middle, so they read as torn.
      const double len = std::hypot(x - lx, y - ly);
      if (len > 18.0)
      {
        const double nx = -(y - ly) / len, ny = (x - lx) / len, m = rnd(-amount, amount) * 1.6;
        cairo_line_to(cr, (x + lx) * 0.5 + nx * m, (y + ly) * 0.5 + ny * m);
      }
      cairo_line_to(cr, x + rnd(-amount, amount), y + rnd(-amount, amount));
    }
    lx = x;
    ly = y;
  }
  cairo_path_destroy(flat);
}

// "CH 50" in jagged poster letters, 900 x 320, centred: a yellow-to-red
// fill, a deep extruded shadow and a thick black outline.
const Texture& ch50Title(ClipKit& k)
{
  return cached(k, "e5_ch50", 900, 320, 450, 160, [](cairo_t* cr) {
    selectGameFont(cr, true);
    cairo_set_font_size(cr, 210);
    cairo_text_extents_t te;
    cairo_text_extents(cr, "CH 50", &te);
    const double x0 = 450 - te.x_advance / 2 - te.x_bearing * 0.5, base = 240;
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_MITER);
    auto glyphs = [&](double dx, double dy) {
      cairo_new_path(cr);
      cairo_move_to(cr, x0 + dx, base + dy);
      cairo_text_path(cr, "CH 50");
      jaggedPath(cr, 50u, 4.0);
    };
    // The extrusion, down and to the right.
    for (int i = 14; i >= 1; --i)
    {
      glyphs(i * 1.2, i * 1.6);
      setRgbaC(cr, i > 12 ? kInk : rgb(120, 20, 30), 1.0);
      cairo_fill(cr);
    }
    glyphs(0, 0);
    cairo_pattern_t* g = cairo_pattern_create_linear(0, base - 160, 0, base);
    cairo_pattern_add_color_stop_rgb(g, 0.0, 1.0, 0.95, 0.4);
    cairo_pattern_add_color_stop_rgb(g, 0.55, 1.0, 0.62, 0.1);
    cairo_pattern_add_color_stop_rgb(g, 1.0, 0.86, 0.12, 0.08);
    cairo_set_source(cr, g);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(g);
    setRgbaC(cr, kInk, 1.0);
    cairo_set_line_width(cr, 7);
    cairo_stroke(cr);
    // A highlight along the tops.
    glyphs(-2, -3);
    cairo_set_line_width(cr, 2);
    setRgbaC(cr, rgb(255, 255, 220), 0.5);
    cairo_stroke(cr);
  });
}

// "MONSTERS" in ghoulish green poster letters with a scream-shaped
// underline (a jagged wedge widening to the right like a shriek), 1000 x 220.
// `scream` bakes only the underline (it tears on after the word).
const Texture& monstersWord(ClipKit& k, bool scream)
{
  return cached(k, scream ? "e5_monsters_scream" : "e5_monsters", 1000, 220, 500, 110, [scream](cairo_t* cr) {
    selectGameFont(cr, true);
    cairo_set_font_size(cr, 104);
    cairo_text_extents_t te;
    cairo_text_extents(cr, "MONSTERS", &te);
    const double x0 = 500 - te.x_advance / 2, base = 126;
    auto glyphs = [&](double dx, double dy) {
      cairo_new_path(cr);
      cairo_move_to(cr, x0 + dx, base + dy);
      cairo_text_path(cr, "MONSTERS");
      jaggedPath(cr, 77u, 2.6);
    };
    if (!scream)
    {
      glyphs(5, 6);
      setRgbaC(cr, kInk, 1.0);
      cairo_fill(cr);
      glyphs(0, 0);
      cairo_pattern_t* g = cairo_pattern_create_linear(0, base - 80, 0, base);
      cairo_pattern_add_color_stop_rgb(g, 0.0, 0.8, 1.0, 0.5);
      cairo_pattern_add_color_stop_rgb(g, 1.0, 0.2, 0.62, 0.2);
      cairo_set_source(cr, g);
      cairo_fill_preserve(cr);
      cairo_pattern_destroy(g);
      setRgbaC(cr, kInk, 1.0);
      cairo_set_line_width(cr, 4.5);
      cairo_stroke(cr);
      return;
    }
    // The scream: a jagged wedge, thin at the left, a wide jaw at the right.
    const double ux0 = x0 - 10, ux1 = x0 + te.x_advance + 30, uy = base + 34;
    cairo_new_path(cr);
    cairo_move_to(cr, ux0, uy);
    const int n = 16;
    for (int i = 1; i <= n; ++i)
    {
      const double u = double(i) / n, x = ux0 + (ux1 - ux0) * u, hgt = 8.0 + 40.0 * u * u;
      cairo_line_to(cr, x - (ux1 - ux0) / n * 0.5, uy - hgt * (i % 2 ? 0.9 : 0.4));
      cairo_line_to(cr, x, uy - hgt * 0.15);
    }
    for (int i = n; i >= 1; --i)
    {
      const double u = double(i) / n, x = ux0 + (ux1 - ux0) * u, hgt = 8.0 + 46.0 * u * u;
      cairo_line_to(cr, x, uy + hgt * 0.15);
      cairo_line_to(cr, x - (ux1 - ux0) / n * 0.5, uy + hgt * (i % 2 ? 1.0 : 0.45));
    }
    cairo_close_path(cr);
    setRgbaC(cr, rgb(200, 20, 30), 1.0);
    cairo_fill_preserve(cr);
    setRgbaC(cr, kInk, 1.0);
    cairo_set_line_width(cr, 3.5);
    cairo_stroke(cr);
  });
}

// Shot: the Channel 50 ident. The Technicolor sky; "CH 50" shakes in over
// frames 0-5 (big and rattling, settling to its place); "MONSTERS" slams
// on from frame 6, its scream underline tearing across after it.
void identCh50(ClipKit& k, int ticks, float ox, float oy)
{
  const float abs = float(ticks) / 4.8f; // 80 ms a frame
  k.r.draw(technicolorSky(k), ox, oy);
  const float in = std::clamp(abs / 6.0f, 0.0f, 1.0f);
  const float e = 1.0f - (1.0f - in) * (1.0f - in) * (1.0f - in);
  const float rattle = (1.0f - e) * 26.0f + (abs < 8.0f ? 3.0f : 0.0f);
  DrawOpts to;
  to.scale = 2.4f - 1.4f * e;
  to.alpha = std::min(1.0f, abs / 1.5f);
  to.angle = std::sin(float(ticks) * 1.7f) * 6.0f * (1.0f - e);
  const float jx = std::sin(float(ticks) * 2.3f) * rattle, jy = std::cos(float(ticks) * 3.1f) * rattle;
  drawGlow(k.r, k.art, 640.0f + ox, 240.0f + oy, 460.0f, rgb(255, 240, 170), 0.35f * to.alpha);
  k.r.draw(ch50Title(k), 640.0f + ox + jx, 240.0f + oy + jy, to);
  if (abs >= 6.0f)
  {
    const float u = std::clamp((abs - 6.0f) / 1.0f, 0.0f, 1.0f);
    DrawOpts mo;
    mo.scale = 1.5f - 0.5f * u;
    mo.alpha = u;
    const float wx = 640.0f + ox, wy = 500.0f + oy;
    // The scream tears across left to right after the slam.
    const float r = std::clamp((abs - 6.5f) / 3.0f, 0.0f, 1.0f);
    k.r.draw(monstersWord(k, false), wx, wy, mo);
    if (r > 0.0f)
    {
      clipTo(k, 0.0f, 0.0f, wx - 470.0f + 940.0f * r, H);
      k.r.draw(monstersWord(k, true), wx, wy);
      unclip(k);
    }
  }
  k.r.drawText("MAXTV  CHANNEL 50  MONSTERS", 640 + ox, 676 + oy, {18.0f, rgb(255, 246, 220), rgb(90, 10, 20)}, Align::Center);
  scanlines(k, 30);
}

// The painted backdrop of a monster-movie set, 1280 x 720: a rose and
// violet dusk, three ranges of brush-painted mountains (snow caps, a
// smoking volcano), the seam where two canvases meet.
const Texture& paintedMountains(ClipKit& k)
{
  return cached(k, "e5_painted_mountains", 1280, 720, 0, 0, [](cairo_t* cr) {
    gradient(cr, 1280, 720, rgb(60, 40, 110), rgb(220, 110, 120), rgb(255, 196, 130));
    radialGlow(cr, 980, 330, 300, rgb(255, 230, 170), 0.6);
    std::uint32_t sd = 2626u;
    auto rnd = [&sd](double a, double b) {
      sd = sd * 1664525u + 1013904223u;
      return a + (b - a) * double(sd >> 8) / double(1u << 24);
    };
    struct Range
    {
      double base, peak;
      Color body, light;
      int peaks;
    };
    const Range kRanges[3] = {{560, 220, rgb(130, 90, 150), rgb(200, 150, 190), 5},
      {600, 320, rgb(90, 60, 110), rgb(150, 110, 160), 7}, {640, 430, rgb(56, 40, 76), rgb(100, 76, 120), 9}};
    for (int r = 0; r < 3; ++r)
    {
      const Range& rg = kRanges[r];
      std::vector<std::pair<double, double>> pts;
      pts.push_back({-40, rg.base});
      for (int i = 0; i < rg.peaks; ++i)
      {
        const double x = -40 + (1360.0 / rg.peaks) * (i + 0.5) + rnd(-40, 40);
        pts.push_back({x - 70, rg.base - (rg.base - rg.peak) * rnd(0.3, 0.55)});
        pts.push_back({x, rg.peak + rnd(-30, 60)});
        pts.push_back({x + 80, rg.base - (rg.base - rg.peak) * rnd(0.25, 0.5)});
      }
      pts.push_back({1320, rg.base});
      cairo_move_to(cr, -40, 720);
      for (const auto& p : pts)
        cairo_line_to(cr, p.first, p.second);
      cairo_line_to(cr, 1320, 720);
      cairo_close_path(cr);
      setRgbaC(cr, rg.body, 1.0);
      cairo_fill(cr);
      // Brushed light down the sunny faces, snow on the far peaks.
      for (std::size_t i = 2; i + 1 < pts.size(); i += 3)
      {
        const double px = pts[i].first, py = pts[i].second;
        for (int s = 0; s < 5; ++s)
        {
          cairo_move_to(cr, px + s * 4, py + s * 6);
          cairo_line_to(cr, px + 40 + s * 10 + rnd(-6, 6), py + 90 + s * 20);
          setRgbaC(cr, rg.light, 0.35);
          cairo_set_line_width(cr, rnd(5, 10));
          cairo_stroke(cr);
        }
        if (r == 0)
        {
          cairo_move_to(cr, px, py);
          cairo_line_to(cr, px - 26, py + 34);
          cairo_line_to(cr, px - 8, py + 28);
          cairo_line_to(cr, px + 4, py + 40);
          cairo_line_to(cr, px + 18, py + 26);
          cairo_line_to(cr, px + 32, py + 36);
          cairo_close_path(cr);
          setRgbaC(cr, rgb(250, 236, 240), 0.9);
          cairo_fill(cr);
        }
      }
    }
    // The volcano, smoking, on the middle range.
    cairo_move_to(cr, 300, 600);
    cairo_line_to(cr, 420, 300);
    cairo_line_to(cr, 470, 300);
    cairo_line_to(cr, 600, 600);
    cairo_close_path(cr);
    setRgbaC(cr, rgb(84, 50, 90), 1.0);
    cairo_fill(cr);
    radialGlow(cr, 445, 300, 60, rgb(255, 120, 60), 0.8);
    for (int i = 0; i < 7; ++i)
    {
      cairo_arc(cr, 450 + i * 22 + rnd(-8, 8), 270 - i * 30, 26 + i * 6, 0, 2 * kPi);
      setRgbaC(cr, rgb(120, 90, 120), 0.5);
      cairo_fill(cr);
    }
    // The canvas seam, a strip of tape over it.
    cairo_move_to(cr, 760, 0);
    cairo_line_to(cr, 764, 720);
    setRgbaC(cr, rgb(40, 20, 50), 0.35);
    cairo_set_line_width(cr, 2);
    cairo_stroke(cr);
  });
}

// The model city on its table at knee height, 1280 x 300 (the table's top
// at y 60): little houses and office blocks with lit windows, a water tower,
// power pylons; the table's painted front edge below.
const Texture& tableCity(ClipKit& k)
{
  return cached(k, "e5_table_city", 1280, 300, 0, 0, [](cairo_t* cr) {
    std::uint32_t sd = 7272u;
    auto rnd = [&sd](double a, double b) {
      sd = sd * 1664525u + 1013904223u;
      return a + (b - a) * double(sd >> 8) / double(1u << 24);
    };
    const double top = 60;
    // Buildings, back row taller, front row lower.
    for (int row = 0; row < 2; ++row)
      for (double x = -10 + row * 30; x < 1290; x += rnd(50, 90))
      {
        if (x > 120 && x < 230)
          continue; // the smoke machine's spot
        const double w = rnd(36, 70), h = row == 0 ? rnd(60, 150) : rnd(24, 60);
        const double y = top + (row == 0 ? 6 : 26) - h;
        static const Color kWalls[5] = {rgb(214, 200, 176), rgb(170, 176, 196), rgb(196, 140, 110), rgb(150, 170, 150), rgb(220, 210, 190)};
        const Color wall = kWalls[int(rnd(0, 5)) % 5];
        cairo_rectangle(cr, x, y, w, h);
        setRgbaC(cr, wall, 1.0);
        cairo_fill_preserve(cr);
        cairo_pattern_t* g = cairo_pattern_create_linear(x, 0, x + w, 0);
        cairo_pattern_add_color_stop_rgba(g, 0, 0, 0, 0, 0.0);
        cairo_pattern_add_color_stop_rgba(g, 1, 0, 0, 0, 0.36);
        cairo_set_source(cr, g);
        cairo_fill_preserve(cr);
        cairo_pattern_destroy(g);
        setRgbaC(cr, kInk, 1.0);
        cairo_set_line_width(cr, 2);
        cairo_stroke(cr);
        if (h < 50 && row == 1)
        {
          // A house: a pitched roof.
          cairo_move_to(cr, x - 4, y);
          cairo_line_to(cr, x + w * 0.5, y - 18);
          cairo_line_to(cr, x + w + 4, y);
          cairo_close_path(cr);
          setRgbaC(cr, rnd(0, 1) < 0.5 ? rgb(170, 60, 50) : rgb(70, 80, 110), 1.0);
          cairo_fill_preserve(cr);
          setRgbaC(cr, kInk, 1.0);
          cairo_stroke(cr);
        }
        for (double wy = y + 8; wy < y + h - 10; wy += 14)
          for (double wx = x + 6; wx < x + w - 10; wx += 12)
          {
            cairo_rectangle(cr, wx, wy, 6, 8);
            setRgbaC(cr, rnd(0, 1) < 0.6 ? rgb(255, 220, 120) : rgb(50, 50, 70), 1.0);
            cairo_fill(cr);
          }
      }
    // The table top's street and the painted front edge.
    cairo_rectangle(cr, 0, top + 26, 1280, 12);
    setRgbaC(cr, rgb(60, 60, 70), 1.0);
    cairo_fill(cr);
    for (double x = 10; x < 1280; x += 40)
    {
      cairo_rectangle(cr, x, top + 31, 18, 2);
      setRgbaC(cr, rgb(250, 210, 90), 1.0);
      cairo_fill(cr);
    }
    cairo_rectangle(cr, 0, top + 38, 1280, 300 - top - 38);
    cairo_pattern_t* e = cairo_pattern_create_linear(0, top + 38, 0, 300);
    cairo_pattern_add_color_stop_rgb(e, 0, 0.36, 0.5, 0.3);
    cairo_pattern_add_color_stop_rgb(e, 0.12, 0.3, 0.42, 0.24);
    cairo_pattern_add_color_stop_rgb(e, 0.14, 0.62, 0.48, 0.3);
    cairo_pattern_add_color_stop_rgb(e, 1, 0.34, 0.24, 0.14);
    cairo_set_source(cr, e);
    cairo_fill(cr);
    cairo_pattern_destroy(e);
    for (int i = 0; i < 30; ++i)
    {
      const double gx = rnd(0, 1280), gy = rnd(top + 70, 300);
      cairo_move_to(cr, gx, gy);
      cairo_curve_to(cr, gx + 60, gy + rnd(-4, 4), gx + 120, gy + rnd(-4, 4), gx + 200, gy);
      setRgbaC(cr, rgb(90, 60, 30), 0.25);
      cairo_set_line_width(cr, 1.5);
      cairo_stroke(cr);
    }
    // The smoke machine: a black box with its nozzle, on the table.
    roundedRect(cr, 140, top - 24, 80, 50, 5);
    setRgbaC(cr, rgb(30, 30, 36), 1.0);
    cairo_fill_preserve(cr);
    setRgbaC(cr, kInk, 1.0);
    cairo_stroke(cr);
    cairo_rectangle(cr, 220, top - 12, 18, 12);
    setRgbaC(cr, rgb(90, 90, 100), 1.0);
    cairo_fill(cr);
    cairo_arc(cr, 156, top - 10, 4, 0, 2 * kPi);
    setRgbaC(cr, rgb(255, 60, 60), 1.0);
    cairo_fill(cr);
  });
}

// Lance with the microphone up (pose 9); `point` adds his free hand thrust
// at the camera: a foreshortened sleeve, the fist and the fingertip.
const Texture& lancePoint(ClipKit& k, bool point, bool mouth)
{
  const std::string key = std::string("e5_lance_point") + (point ? "p" : "n") + (mouth ? "o" : "c");
  return cached(k, key, 320, 600, 160, 580, [point, mouth](cairo_t* cr) {
    cairo_translate(cr, 40, 100);
    paintLance(cr, 9, mouth);
    if (!point)
      return;
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_move_to(cr, 80, 186);
    cairo_line_to(cr, 128, 216);
    setRgbaC(cr, kInk, 1.0);
    cairo_set_line_width(cr, 34);
    cairo_stroke_preserve(cr);
    setRgbaC(cr, rgb(40, 70, 190), 1.0);
    cairo_set_line_width(cr, 28);
    cairo_stroke(cr);
    // The cuff, round toward us.
    cairo_save(cr);
    cairo_translate(cr, 134, 222);
    cairo_scale(cr, 1.0, 0.86);
    cairo_arc(cr, 0, 0, 26, 0, 2 * kPi);
    cairo_restore(cr);
    setRgbaC(cr, rgb(250, 250, 250), 1.0);
    cairo_fill_preserve(cr);
    setRgbaC(cr, kInk, 1.0);
    cairo_set_line_width(cr, 3);
    cairo_stroke(cr);
    // The fist and the finger pointing out of the screen.
    cairo_arc(cr, 142, 216, 23, 0, 2 * kPi);
    setRgbaC(cr, rgb(246, 200, 160), 1.0);
    cairo_fill_preserve(cr);
    setRgbaC(cr, kInk, 1.0);
    cairo_stroke(cr);
    for (int i = 0; i < 3; ++i)
    {
      cairo_arc(cr, 131 + i * 10, 230, 5, kPi * 0.1, kPi * 0.9);
      cairo_set_line_width(cr, 2);
      cairo_stroke(cr);
    }
    cairo_arc(cr, 149, 206, 14, 0, 2 * kPi);
    setRgbaC(cr, rgb(255, 214, 178), 1.0);
    cairo_fill_preserve(cr);
    setRgbaC(cr, kInk, 1.0);
    cairo_set_line_width(cr, 3);
    cairo_stroke(cr);
    cairo_arc(cr, 151, 204, 6.5, 0, 2 * kPi);
    setRgbaC(cr, rgb(255, 236, 226), 1.0);
    cairo_fill(cr);
  });
}

// Shot: the man in the rubber lizard suit and Lance behind the model city.
// The painted mountains; the suit waving (a 6-frame loop) behind the
// table, the city at his knees; a smoke machine puffing (8 frames); Lance
// beside him, pointing at the camera from frame 8.
void lizardWave(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  k.r.draw(paintedMountains(k), ox, oy);
  // The studio's light on the backdrop.
  drawGlow(k.r, k.art, 820.0f + ox, 300.0f + oy, 420.0f, rgb(255, 240, 210), 0.25f);
  // The suit, a giant behind the table.
  const Texture& suit = cached(k, "e5_suit_wave" + std::to_string(frame % 6), 600, 600, 300, 580, [frame](cairo_t* cr) {
    paintGorgazilla(cr, 300, 580, 2.5, 15, frame % 6);
  });
  k.r.draw(suit, 830.0f + ox, 640.0f + oy);
  // Lance, also behind it.
  const bool point = frame % 16 >= 8;
  const bool talking = point && (ticks / 6) % 2 == 0;
  const float lx = 380.0f + ox, ly = 560.0f + oy;
  k.r.draw(lancePoint(k, point, talking), lx, ly);
  if (point)
    for (int i = 0; i < 4; ++i)
    {
      // Lines bursting round the fingertip.
      const float a = float(i) * 1.57f + 0.78f;
      const float hx = lx + 29.0f, hy = ly - 274.0f; // the fingertip
      k.r.drawLine(hx + std::cos(a) * 24.0f, hy + std::sin(a) * 24.0f, hx + std::cos(a) * 38.0f, hy + std::sin(a) * 38.0f, 3.0f,
        rgb(255, 250, 220));
    }
  // The model city at their knees.
  k.r.draw(tableCity(k), ox, 420.0f + oy);
  // The smoke machine's puffs drifting right over the rooftops.
  for (int i = 0; i < 5; ++i)
  {
    const float u = std::fmod((float(frame % 8) + float(ticks % 5) / 5.4f) / 8.0f + float(i) / 5.0f, 1.0f);
    const float sx = 250.0f + u * 560.0f + ox, sy = 456.0f - u * 90.0f + std::sin(u * 7.0f + float(i)) * 10.0f + oy;
    // A soft, lumpy puff of stage smoke (blended over, not added).
    const Texture& puff = cached(k, "e5_smoke_puff", 200, 160, 100, 80, [](cairo_t* cr) {
      static const double kLumps[5][3] = {{100, 86, 60}, {62, 96, 40}, {140, 94, 44}, {84, 60, 42}, {122, 62, 38}};
      for (const auto& l : kLumps)
      {
        cairo_pattern_t* g = cairo_pattern_create_radial(l[0] - l[2] * 0.3, l[1] - l[2] * 0.3, 2, l[0], l[1], l[2]);
        cairo_pattern_add_color_stop_rgba(g, 0, 1, 1, 1, 0.7);
        cairo_pattern_add_color_stop_rgba(g, 0.6, 0.86, 0.86, 0.92, 0.45);
        cairo_pattern_add_color_stop_rgba(g, 1, 0.8, 0.8, 0.88, 0.0);
        cairo_arc(cr, l[0], l[1], l[2], 0, 2 * kPi);
        cairo_set_source(cr, g);
        cairo_fill(cr);
        cairo_pattern_destroy(g);
      }
    });
    DrawOpts po;
    po.alpha = std::sin(u * 3.14159f);
    po.scale = 0.5f + u * 1.1f;
    k.r.draw(puff, sx, sy, po);
  }
}

// --- Level 28: Toon Town Throwdown (Channel 1, Cartoons) and the end of Episode 4 ---

constexpr Color kToonCream = rgb(250, 240, 214);
constexpr Color kToonWhite = rgb(255, 255, 255);

// Rubber-hose cartoons ink everything: fills the current path, then strokes
// it black.
void toonFill(cairo_t* cr, Color c, double lw)
{
  setRgbaC(cr, c, 1.0);
  cairo_fill_preserve(cr);
  setRgbaC(cr, kInk, 1.0);
  cairo_set_line_width(cr, lw);
  cairo_stroke(cr);
}

void toonOval(cairo_t* cr, double x, double y, double rx, double ry, double angle = 0.0)
{
  cairo_new_sub_path(cr);
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_rotate(cr, angle);
  cairo_scale(cr, rx, ry);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_restore(cr);
}

// A rubber-hose limb: a bendy tube from (x0, y0) bowing toward (bx, by) and
// ending at (x1, y1), w thick and inked.
void hose(cairo_t* cr, double x0, double y0, double bx, double by, double x1, double y1, double w, Color c)
{
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  cairo_move_to(cr, x0, y0);
  cairo_curve_to(cr, bx, by, bx, by, x1, y1);
  setRgbaC(cr, kInk, 1.0);
  cairo_set_line_width(cr, w + 6.0);
  cairo_stroke_preserve(cr);
  setRgbaC(cr, c, 1.0);
  cairo_set_line_width(cr, w);
  cairo_stroke(cr);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
}

// A pie-cut eye: a tall white oval, the black pupil with its wedge of
// shine cut out, looking along (lx, ly).
void pieEye(cairo_t* cr, double x, double y, double rx, double ry, double lx, double ly)
{
  toonOval(cr, x, y, rx, ry);
  toonFill(cr, kToonWhite, 2.4);
  const double px = x + lx * rx * 0.35, py = y + ly * ry * 0.3;
  toonOval(cr, px, py, rx * 0.56, ry * 0.62);
  setRgbaC(cr, kInk, 1.0);
  cairo_fill(cr);
  cairo_move_to(cr, px, py);
  cairo_arc(cr, px, py, ry * 0.7, -kPi * 0.48, -kPi * 0.2);
  cairo_close_path(cr);
  setRgbaC(cr, kToonWhite, 1.0);
  cairo_fill(cr);
}

// Throws a baked picture out of focus: three box blurs each way over the
// image's own pixels (premultiplied, so the edges fade cleanly).
void defocus(cairo_t* cr, int radius)
{
  cairo_surface_t* s = cairo_get_target(cr);
  cairo_surface_flush(s);
  unsigned char* px = cairo_image_surface_get_data(s);
  const int w = cairo_image_surface_get_width(s), h = cairo_image_surface_get_height(s);
  const int stride = cairo_image_surface_get_stride(s);
  std::vector<int> line(std::size_t(std::max(w, h)) * 4);
  auto pass = [&](int n, int count, int step, int lineStep) {
    for (int l = 0; l < count; ++l)
    {
      unsigned char* base = px + l * lineStep;
      for (int i = 0; i < n; ++i)
        for (int c = 0; c < 4; ++c)
          line[std::size_t(i * 4 + c)] = base[i * step + c];
      for (int c = 0; c < 4; ++c)
      {
        int sum = 0;
        for (int i = -radius; i <= radius; ++i)
          sum += line[std::size_t(std::clamp(i, 0, n - 1) * 4 + c)];
        for (int i = 0; i < n; ++i)
        {
          base[i * step + c] = static_cast<unsigned char>(sum / (2 * radius + 1));
          sum += line[std::size_t(std::min(i + radius + 1, n - 1) * 4 + c)] - line[std::size_t(std::max(i - radius, 0) * 4 + c)];
        }
      }
    }
  };
  for (int k = 0; k < 3; ++k)
  {
    pass(w, h, 4, stride);
    pass(h, w, stride, 4);
  }
  cairo_surface_mark_dirty(s);
}

// A rubber-hose figure's pose in design px: feet at y 0, facing +x. Each
// limb is the bend, then the hand or foot.
struct HoseRig
{
  double hipX = 0, hipY = -96, shX = 4, shY = -176, headX = 10, headY = -236, tilt = 0;
  double sx = 1.0, sy = 1.0; // the body's squash
  std::array<double, 4> legBack{-24, -48, -24, -4}, legFront{26, -48, 28, -4};
  std::array<double, 4> armBack{-56, -150, -44, -112}, armFront{60, -150, 46, -112};
  bool openBack = false, openFront = false;
  bool frontLeg = true;
  double lookX = 1.0, lookY = 0.0;
  int mouth = 0; // 0 grin, 1 open, 2 a small o, 3 a closed smile
};

// The dance on the beat, b 0-3: down in a squash, a kick, up on the toes
// with jazz hands, the other kick.
HoseRig bounceRig(int b)
{
  HoseRig g;
  static constexpr double kBob[4] = {8, -2, -14, -2};
  const double bob = kBob[b & 3];
  g.hipY += bob;
  g.shY += bob;
  g.headY += bob;
  if ((b & 3) == 0)
  {
    g.sx = 1.08;
    g.sy = 0.92;
  }
  else if ((b & 3) == 2)
  {
    g.sx = 0.95;
    g.sy = 1.06;
  }
  g.legBack = {-30, -44 + bob * 0.5, -24, -4};
  g.legFront = {32, -44 + bob * 0.5, 28, -4};
  g.armBack = {-60, -150 + bob, -44, -112 + bob};
  g.armFront = {62, -150 + bob, 46, -112 + bob};
  switch (b & 3)
  {
    case 1:
      g.legFront = {40, -64, 54, -30};
      g.armFront = {62, -206 + bob, 66, -252 + bob};
      g.openFront = true;
      break;
    case 2:
      g.armBack = {-62, -206 + bob, -60, -254 + bob};
      g.armFront = {64, -206 + bob, 68, -256 + bob};
      g.openBack = g.openFront = true;
      g.mouth = 1;
      break;
    case 3:
      g.legBack = {-40, -64, -50, -30};
      g.armBack = {-62, -206 + bob, -62, -252 + bob};
      g.openBack = true;
      break;
    default:
      break;
  }
  return g;
}

struct ToonLook
{
  Color skin, hair, top, legs, shoe, sleeve;
  double torsoW;
};

ToonLook toonLook(int who)
{
  switch (who)
  {
    case 0: // Dash: red top, blue jeans, brown boots
      return {rgb(246, 200, 160), rgb(255, 214, 72), rgb(232, 64, 52), rgb(54, 76, 150), rgb(96, 62, 40), rgb(232, 64, 52), 66};
    case 1: // Rocco: green tank, bare arms, grey trousers
      return {rgb(170, 108, 70), rgb(30, 22, 18), rgb(104, 146, 64), rgb(110, 110, 124), rgb(48, 42, 40), rgb(170, 108, 70), 86};
    default: // Nova: purple suit, white boots
      return {rgb(252, 214, 186), rgb(255, 86, 184), rgb(132, 66, 216), rgb(132, 66, 216), rgb(238, 238, 248), rgb(132, 66, 216), 58};
  }
}

// A rubber-hose arm with its white glove, the fingers pointing on from
// the bend.
void hoseArm(cairo_t* cr, double x0, double y0, const std::array<double, 4>& a, double w, Color c, bool open, double size)
{
  hose(cr, x0, y0, a[0], a[1], a[2], a[3], w, c);
  paintToonGlove(cr, a[2], a[3], size, std::atan2(a[3] - a[1], a[2] - a[0]), open);
}

void hoseLeg(cairo_t* cr, double x0, double y0, const std::array<double, 4>& l, double w, Color c, Color shoe)
{
  hose(cr, x0, y0, l[0], l[1], l[2], l[3], w, c);
  // A big round shoe, toe forward.
  toonOval(cr, l[2] + 12, l[3] - 8, 26, 14);
  toonFill(cr, shoe, 3.0);
  toonOval(cr, l[2] + 4, l[3] - 14, 9, 4, -0.2);
  setRgbaC(cr, kToonWhite, 0.45);
  cairo_fill(cr);
}

// The mouths, in head coordinates (the head 46 px round, facing +x).
void toonMouth(cairo_t* cr, int mouth, Color skin)
{
  (void)skin;
  if (mouth == 1)
  {
    toonOval(cr, 28, 24, 13, 12);
    toonFill(cr, rgb(120, 24, 34), 2.6);
    toonOval(cr, 30, 30, 7, 4);
    setRgbaC(cr, rgb(240, 110, 120), 1.0);
    cairo_fill(cr);
  }
  else if (mouth == 2)
  {
    toonOval(cr, 32, 24, 5, 6);
    toonFill(cr, rgb(120, 24, 34), 2.0);
  }
  else if (mouth == 3)
  {
    cairo_move_to(cr, 10, 20);
    cairo_curve_to(cr, 20, 32, 38, 30, 44, 16);
    setRgbaC(cr, kInk, 1.0);
    cairo_set_line_width(cr, 3.0);
    cairo_stroke(cr);
  }
  else
  {
    cairo_move_to(cr, 6, 16);
    cairo_curve_to(cr, 12, 46, 40, 44, 48, 12);
    cairo_curve_to(cr, 34, 22, 18, 22, 6, 16);
    cairo_close_path(cr);
    toonFill(cr, rgb(120, 24, 34), 2.6);
    toonOval(cr, 28, 32, 9, 5);
    setRgbaC(cr, rgb(240, 110, 120), 1.0);
    cairo_fill(cr);
  }
}

// Dash, Rocco or Nova as a 1930s cartoon: noodle limbs, white gloves, pie-cut
// eyes (Dash keeps his shades), about 300 px tall, feet at (0, 0).
void paintToonRunner(cairo_t* cr, int who, const HoseRig& g)
{
  const ToonLook L = toonLook(who);
  const double tw = L.torsoW;
  const double armW = who == 1 ? 15 : 12, legW = who == 1 ? 16 : 13;
  const double shBX = g.shX - tw * 0.4, shFX = g.shX + tw * 0.4, shAY = g.shY + 14;
  // Nova's ponytail swings out behind everything.
  if (who == 2)
  {
    cairo_save(cr);
    cairo_translate(cr, g.headX, g.headY);
    cairo_rotate(cr, g.tilt);
    cairo_move_to(cr, -30, -30);
    cairo_curve_to(cr, -80, -50, -110, 0, -96, 44);
    cairo_curve_to(cr, -86, 20, -70, 10, -56, 20);
    cairo_curve_to(cr, -60, 0, -50, -10, -34, -6);
    cairo_close_path(cr);
    toonFill(cr, L.hair, 3.0);
    cairo_restore(cr);
  }
  hoseArm(cr, shBX, shAY, g.armBack, armW, L.sleeve, g.openBack, 17);
  hoseLeg(cr, g.hipX - 14, g.hipY + 6, g.legBack, legW, L.legs, L.shoe);
  if (g.frontLeg)
    hoseLeg(cr, g.hipX + 16, g.hipY + 6, g.legFront, legW, L.legs, L.shoe);
  // Shorts or the trousers' seat.
  toonOval(cr, g.hipX, g.hipY, tw * 0.5, 22);
  toonFill(cr, L.legs, 3.0);
  // The body, a bean.
  const double dx = g.shX - g.hipX, dy = g.shY - g.hipY;
  const double len = std::sqrt(dx * dx + dy * dy);
  const double ang = std::atan2(dx, -dy);
  cairo_save(cr);
  cairo_translate(cr, (g.hipX + g.shX) * 0.5, (g.hipY + g.shY) * 0.5);
  cairo_rotate(cr, ang);
  toonOval(cr, 0, 0, tw * 0.5 * g.sx, (len * 0.5 + 18) * g.sy);
  setRgbaC(cr, L.top, 1.0);
  cairo_fill_preserve(cr);
  cairo_save(cr);
  cairo_clip_preserve(cr);
  cairo_new_path(cr);
  if (who == 0)
  {
    // Dash's stripe.
    cairo_rectangle(cr, -tw, -12, tw * 2, 14);
    setRgbaC(cr, kToonWhite, 1.0);
    cairo_fill(cr);
  }
  else if (who == 2)
  {
    // Nova's sash.
    cairo_move_to(cr, -tw * 0.6, -len * 0.5);
    cairo_line_to(cr, tw * 0.6, len * 0.4);
    setRgbaC(cr, rgb(64, 255, 220), 1.0);
    cairo_set_line_width(cr, 14);
    cairo_stroke(cr);
  }
  // Shading down the back.
  toonOval(cr, -tw * 0.5, 6, tw * 0.3, len * 0.6);
  setRgbaC(cr, rgb(0, 0, 0), 0.14);
  cairo_fill(cr);
  cairo_restore(cr);
  toonOval(cr, 0, 0, tw * 0.5 * g.sx, (len * 0.5 + 18) * g.sy);
  setRgbaC(cr, kInk, 1.0);
  cairo_set_line_width(cr, 3.4);
  cairo_stroke(cr);
  if (who == 1)
  {
    // Rocco's dog tag on its chain.
    cairo_move_to(cr, -18, -len * 0.5 - 6);
    cairo_line_to(cr, 4, -len * 0.5 + 26);
    cairo_line_to(cr, 22, -len * 0.5 - 6);
    setRgbaC(cr, rgb(200, 200, 210), 1.0);
    cairo_set_line_width(cr, 2.2);
    cairo_stroke(cr);
    roundedRect(cr, -4, -len * 0.5 + 24, 16, 22, 4);
    toonFill(cr, rgb(214, 214, 224), 2.0);
  }
  cairo_restore(cr);
  // The head.
  cairo_save(cr);
  cairo_translate(cr, g.headX, g.headY);
  cairo_rotate(cr, g.tilt);
  const double hr = 46;
  if (who == 0)
  {
    // Spiky hair behind.
    for (int i = 0; i <= 12; ++i)
    {
      const double a = -kPi * 1.04 + i * kPi * 1.08 / 12.0;
      const double r = i % 2 ? hr + 30 : hr - 4;
      if (i == 0)
        cairo_move_to(cr, std::cos(a) * r, std::sin(a) * r);
      else
        cairo_line_to(cr, std::cos(a) * r, std::sin(a) * r);
    }
    cairo_close_path(cr);
    toonFill(cr, L.hair, 3.0);
  }
  else if (who == 1)
  {
    // The bandana's knot tails.
    cairo_move_to(cr, -38, -22);
    cairo_curve_to(cr, -70, -30, -78, -6, -84, 8);
    cairo_curve_to(cr, -66, 0, -56, -6, -40, -8);
    cairo_close_path(cr);
    toonFill(cr, rgb(232, 64, 52), 2.6);
  }
  toonOval(cr, 0, 0, hr, hr * 1.02);
  toonFill(cr, L.skin, 3.4);
  // An ear.
  toonOval(cr, -18, 6, 9, 12);
  toonFill(cr, L.skin, 2.4);
  if (who == 1)
  {
    // The beard round the jaw.
    cairo_move_to(cr, -24, 2);
    cairo_curve_to(cr, -26, 40, 8, 56, 30, 48);
    cairo_curve_to(cr, 46, 42, 54, 24, 52, 10);
    cairo_curve_to(cr, 40, 20, 14, 16, 2, 14);
    cairo_close_path(cr);
    toonFill(cr, L.hair, 2.6);
  }
  // The face.
  if (who == 0)
  {
    // Shades: two cyan lenses on a band.
    cairo_move_to(cr, -30, -10);
    cairo_line_to(cr, 4, -10);
    setRgbaC(cr, kInk, 1.0);
    cairo_set_line_width(cr, 5);
    cairo_stroke(cr);
    for (const double ex : {12.0, 38.0})
    {
      roundedRect(cr, ex - 13, -20, 26, 20, 8);
      toonFill(cr, rgb(64, 226, 255), 3.0);
      cairo_move_to(cr, ex - 8, -14);
      cairo_line_to(cr, ex - 2, -18);
      setRgbaC(cr, kToonWhite, 0.9);
      cairo_set_line_width(cr, 2.4);
      cairo_stroke(cr);
    }
    cairo_move_to(cr, 24, -12);
    cairo_line_to(cr, 26, -12);
    setRgbaC(cr, kInk, 1.0);
    cairo_set_line_width(cr, 4);
    cairo_stroke(cr);
  }
  else
  {
    pieEye(cr, 14, -12, 9, 14, g.lookX, g.lookY);
    pieEye(cr, 36, -14, 9, 14, g.lookX, g.lookY);
    if (who == 1)
    {
      cairo_move_to(cr, 4, -32);
      cairo_line_to(cr, 24, -30);
      cairo_move_to(cr, 28, -34);
      cairo_line_to(cr, 46, -32);
      setRgbaC(cr, kInk, 1.0);
      cairo_set_line_width(cr, 5);
      cairo_stroke(cr);
    }
    else
    {
      // Nova's lashes, a blush.
      for (int i = 0; i < 3; ++i)
      {
        cairo_move_to(cr, 40 + i * 4, -27);
        cairo_line_to(cr, 44 + i * 6, -34 + i);
      }
      setRgbaC(cr, kInk, 1.0);
      cairo_set_line_width(cr, 2.2);
      cairo_stroke(cr);
      toonOval(cr, 20, 12, 9, 5);
      setRgbaC(cr, rgb(255, 120, 150), 0.45);
      cairo_fill(cr);
    }
  }
  // The nose.
  toonOval(cr, 50, 4, who == 1 ? 10 : 7, who == 1 ? 8 : 6);
  toonFill(cr, L.skin, 2.4);
  toonMouth(cr, g.mouth, L.skin);
  // Hair over the head.
  if (who == 0)
  {
    for (int i = 0; i < 3; ++i)
    {
      const double x = -6 + i * 16;
      cairo_move_to(cr, x - 10, -38 + i * 2);
      cairo_line_to(cr, x + 4, -58 + i * 3);
      cairo_line_to(cr, x + 8, -36 + i * 2);
    }
    cairo_close_path(cr);
    toonFill(cr, L.hair, 2.6);
  }
  else if (who == 1)
  {
    // The bandana over the top, white polka dots.
    cairo_arc(cr, 0, 0, hr + 2, kPi * 1.04, kPi * 1.94);
    cairo_curve_to(cr, 30, -26, -20, -24, -46, -4);
    cairo_close_path(cr);
    toonFill(cr, rgb(232, 64, 52), 3.0);
    for (const auto& d : {std::array<double, 2>{-20, -30}, std::array<double, 2>{6, -38}, std::array<double, 2>{26, -34}})
    {
      toonOval(cr, d[0], d[1], 3.5, 3.5);
      setRgbaC(cr, kToonWhite, 1.0);
      cairo_fill(cr);
    }
  }
  else
  {
    // Nova's bangs, scalloped.
    cairo_move_to(cr, -44, 0);
    cairo_curve_to(cr, -52, -50, 10, -64, 46, -24);
    cairo_curve_to(cr, 36, -30, 30, -22, 24, -28);
    cairo_curve_to(cr, 16, -20, 8, -24, 2, -30);
    cairo_curve_to(cr, -10, -18, -20, -16, -28, -22);
    cairo_curve_to(cr, -32, -10, -38, -6, -44, 0);
    cairo_close_path(cr);
    toonFill(cr, L.hair, 3.0);
  }
  cairo_restore(cr);
  hoseArm(cr, shFX, shAY, g.armFront, armW, L.sleeve, g.openFront, 17);
}

// Lance Marquee as a 1930s cartoon: a sequinned blue bean of a tux, a
// golden quiff, a chin, a grin of teeth, white gloves, a microphone in one
// hand. About 420 px tall, feet at (0, 0); b is the dance's beat.
void paintToonLance(cairo_t* cr, int b, bool mouthOpen)
{
  cairo_save(cr);
  cairo_scale(cr, 1.38, 1.38);
  HoseRig g = bounceRig(b);
  // The microphone hand stays by his mouth.
  g.armFront = {80, -168 + (g.headY + 236), 58, -192 + (g.headY + 236)};
  const Color suit = rgb(44, 76, 196), trousers = rgb(26, 30, 60), skin = rgb(244, 204, 168);
  const double tw = 70;
  hoseArm(cr, g.shX - tw * 0.4, g.shY + 14, g.armBack, 12, suit, g.openBack, 18);
  // Black shoes with white spats.
  for (int i = 0; i < 2; ++i)
  {
    const auto& l = i == 0 ? g.legBack : g.legFront;
    hose(cr, g.hipX + (i == 0 ? -14 : 16), g.hipY + 6, l[0], l[1], l[2], l[3], 13, trousers);
    toonOval(cr, l[2] + 12, l[3] - 8, 26, 13);
    toonFill(cr, rgb(20, 18, 24), 3.0);
    toonOval(cr, l[2] + 2, l[3] - 14, 13, 8);
    toonFill(cr, kToonWhite, 2.2);
  }
  // The tails flapping behind.
  cairo_move_to(cr, g.hipX - 20, g.hipY - 30);
  cairo_curve_to(cr, g.hipX - 50, g.hipY - 6, g.hipX - 60, g.hipY + 20, g.hipX - 56, g.hipY + 40);
  cairo_line_to(cr, g.hipX - 30, g.hipY + 14);
  cairo_close_path(cr);
  toonFill(cr, rgb(28, 50, 140), 3.0);
  toonOval(cr, g.hipX, g.hipY, tw * 0.48, 20);
  toonFill(cr, trousers, 3.0);
  // The jacket.
  const double cx = (g.hipX + g.shX) * 0.5, cy = (g.hipY + g.shY) * 0.5;
  const double ry = (g.hipY - g.shY) * 0.5 + 20;
  toonOval(cr, cx, cy, tw * 0.5 * g.sx, ry * g.sy);
  {
    cairo_pattern_t* p = cairo_pattern_create_linear(cx - tw * 0.5, 0, cx + tw * 0.5, 0);
    cairo_pattern_add_color_stop_rgb(p, 0, 0.43, 0.59, 1.0);
    cairo_pattern_add_color_stop_rgb(p, 1, 0.1, 0.16, 0.5);
    cairo_set_source(cr, p);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(p);
  }
  setRgbaC(cr, kInk, 1.0);
  cairo_set_line_width(cr, 3.4);
  cairo_stroke(cr);
  // Shirt front, gold lapels, the red bow tie, sequins.
  const double top = cy - ry * g.sy;
  cairo_move_to(cr, cx - 12, top + 6);
  cairo_line_to(cr, cx + 14, top + 6);
  cairo_line_to(cr, cx + 2, top + 64);
  cairo_close_path(cr);
  toonFill(cr, kToonWhite, 2.0);
  for (const int side : {-1, 1})
  {
    cairo_move_to(cr, cx + side * 14, top + 4);
    cairo_line_to(cr, cx + side * 30, top + 14);
    cairo_line_to(cr, cx + side * 6, top + 70);
    cairo_close_path(cr);
    toonFill(cr, rgb(255, 210, 70), 2.2);
    cairo_move_to(cr, cx, top + 10);
    cairo_line_to(cr, cx + side * 18, top);
    cairo_line_to(cr, cx + side * 18, top + 20);
    cairo_close_path(cr);
    toonFill(cr, rgb(226, 36, 52), 2.2);
  }
  toonOval(cr, cx, top + 10, 5, 5);
  toonFill(cr, rgb(226, 36, 52), 2.0);
  std::uint32_t sd = 2828u;
  for (int i = 0; i < 18; ++i)
  {
    sd = sd * 1664525u + 1013904223u;
    const double a = double(sd % 628u) / 100.0, r = double((sd >> 10) % 100u) / 100.0;
    toonOval(cr, cx + std::cos(a) * r * tw * 0.4, cy + 10 + std::sin(a) * r * ry * 0.7, 1.8, 1.8);
    setRgbaC(cr, rgb(210, 230, 255), 0.85);
    cairo_fill(cr);
  }
  // The head: a big chin, the quiff, the grin.
  cairo_save(cr);
  cairo_translate(cr, g.headX + 4, g.headY + 2);
  cairo_rotate(cr, g.tilt);
  cairo_move_to(cr, -40, -20);
  cairo_curve_to(cr, -46, 30, -10, 58, 34, 52);
  cairo_curve_to(cr, 62, 48, 62, 20, 52, -6);
  cairo_curve_to(cr, 44, -50, -34, -60, -40, -20);
  cairo_close_path(cr);
  toonFill(cr, skin, 3.4);
  toonOval(cr, -26, 6, 9, 12);
  toonFill(cr, skin, 2.4);
  // The quiff, a golden wave.
  cairo_move_to(cr, -44, 0);
  cairo_curve_to(cr, -60, -60, 0, -80, 40, -76);
  cairo_curve_to(cr, 76, -74, 84, -44, 66, -30);
  cairo_curve_to(cr, 58, -48, 36, -50, 26, -34);
  cairo_curve_to(cr, 10, -26, -14, -30, -26, -12);
  cairo_close_path(cr);
  toonFill(cr, rgb(255, 210, 70), 3.0);
  cairo_move_to(cr, -30, -26);
  cairo_curve_to(cr, -20, -56, 20, -66, 50, -60);
  setRgbaC(cr, rgb(255, 246, 200), 0.9);
  cairo_set_line_width(cr, 3.0);
  cairo_stroke(cr);
  pieEye(cr, 12, -10, 8, 13, 1.0, 0.0);
  pieEye(cr, 34, -12, 8, 13, 1.0, 0.0);
  cairo_move_to(cr, 2, -30);
  cairo_curve_to(cr, 10, -36, 18, -36, 24, -32);
  cairo_move_to(cr, 28, -34);
  cairo_curve_to(cr, 34, -38, 42, -38, 46, -32);
  setRgbaC(cr, kInk, 1.0);
  cairo_set_line_width(cr, 3.4);
  cairo_stroke(cr);
  toonOval(cr, 54, 6, 8, 7);
  toonFill(cr, skin, 2.4);
  // The grin: a wall of teeth, open on the beat.
  cairo_move_to(cr, 2, 20);
  if (mouthOpen)
    cairo_curve_to(cr, 8, 58, 48, 56, 56, 16);
  else
    cairo_curve_to(cr, 10, 40, 46, 38, 56, 16);
  cairo_curve_to(cr, 38, 26, 18, 26, 2, 20);
  cairo_close_path(cr);
  toonFill(cr, mouthOpen ? rgb(120, 24, 34) : kToonWhite, 2.6);
  if (mouthOpen)
  {
    cairo_move_to(cr, 4, 21);
    cairo_curve_to(cr, 18, 30, 40, 29, 54, 18);
    cairo_line_to(cr, 53, 26);
    cairo_curve_to(cr, 40, 35, 18, 36, 6, 28);
    cairo_close_path(cr);
    toonFill(cr, kToonWhite, 1.8);
    toonOval(cr, 30, 42, 10, 5);
    setRgbaC(cr, rgb(240, 110, 120), 1.0);
    cairo_fill(cr);
  }
  else
    for (int i = 1; i < 6; ++i)
    {
      cairo_move_to(cr, 4 + i * 8.5, 22);
      cairo_line_to(cr, 4 + i * 8.5, 32);
      setRgbaC(cr, kInk, 1.0);
      cairo_set_line_width(cr, 1.6);
      cairo_stroke(cr);
    }
  cairo_restore(cr);
  // The microphone, then the hand round it.
  cairo_move_to(cr, g.armFront[2], g.armFront[3] + 20);
  cairo_line_to(cr, g.armFront[2] + 10, g.armFront[3] - 18);
  setRgbaC(cr, kInk, 1.0);
  cairo_set_line_width(cr, 8);
  cairo_stroke(cr);
  toonOval(cr, g.armFront[2] + 12, g.armFront[3] - 24, 11, 11);
  toonFill(cr, rgb(200, 204, 214), 2.6);
  for (int i = -1; i <= 1; ++i)
  {
    cairo_move_to(cr, g.armFront[2] + 4, g.armFront[3] - 24 + i * 5);
    cairo_line_to(cr, g.armFront[2] + 20, g.armFront[3] - 24 + i * 5);
  }
  setRgbaC(cr, rgb(110, 114, 128), 1.0);
  cairo_set_line_width(cr, 1.4);
  cairo_stroke(cr);
  hoseArm(cr, g.shX + tw * 0.4, g.shY + 14, g.armFront, 12, suit, false, 18);
  cairo_restore(cr);
}

// Toon Lance, baked at `scale` (feet at the anchor), out of focus if asked.
const Texture& toonLance(ClipKit& k, int b, bool mouth, float scale, bool soft = false)
{
  const int w = int(560.0f * scale), h = int(620.0f * scale);
  const std::string key = "e4_toon_lance" + std::to_string(b & 3) + (mouth ? "o" : "c") + (soft ? "s" : "") +
    std::to_string(int(scale * 100.0f));
  return cached(k, key, w, h, float(w) * 0.5f, 600.0f * scale, [&](cairo_t* cr) {
    cairo_scale(cr, scale, scale);
    cairo_translate(cr, 280, 600);
    paintToonLance(cr, b, mouth);
    if (soft)
      defocus(cr, 4);
  });
}

// A toon runner in a pose, baked at `scale` (feet at the anchor), mirrored
// to face left if asked. Poses: 0-3 the dance, 4 Nova whispering (leaning in,
// a hand cupped to her mouth), 5 Dash listening (head cocked toward her), 6
// leaning back for a kick (the kicking leg left out: the clip draws it), 7
// standing, looking up.
HoseRig toonPose(int pose, bool talking)
{
  if (pose < 4)
    return bounceRig(pose);
  HoseRig g;
  switch (pose)
  {
    case 4:
      g.shX = 22;
      g.headX = 44;
      g.headY = -228;
      g.tilt = 0.18;
      g.armFront = {84, -140, 100, -196};
      g.openFront = true;
      g.armBack = {-30, -140, -40, -104};
      g.mouth = talking ? 1 : 3;
      break;
    case 5:
      g.headX = 18;
      g.tilt = 0.14;
      g.mouth = 2;
      break;
    case 6:
      g.shX = -24;
      g.shY = -170;
      g.headX = -30;
      g.headY = -230;
      g.tilt = -0.22;
      g.armBack = {-70, -210, -84, -250};
      g.armFront = {20, -214, 10, -262};
      g.openBack = g.openFront = true;
      g.frontLeg = false;
      g.mouth = 3;
      break;
    default:
      g.lookY = -1.0;
      g.mouth = 2;
      break;
  }
  return g;
}

const Texture& toonRunner(ClipKit& k, int who, int pose, bool talking, float scale, bool mirror)
{
  const int w = int(400.0f * scale), h = int(420.0f * scale);
  const std::string key = "e4_toon_runner" + std::to_string(who) + "_" + std::to_string(pose) + (talking ? "t" : "") +
    (mirror ? "m" : "") + std::to_string(int(scale * 100.0f));
  return cached(k, key, w, h, float(w) * 0.5f, 400.0f * scale, [&](cairo_t* cr) {
    cairo_translate(cr, double(w) * 0.5, 400.0 * scale);
    cairo_scale(cr, mirror ? -scale : scale, scale);
    paintToonRunner(cr, who, toonPose(pose, talking));
  });
}

// A painted cartoon street, 1280 x 720 and seamless side to side: cream
// sky and inked clouds, a row of leaning pastel shopfronts with awnings
// and signs, curly lampposts, the sidewalk and the road.
void paintToonStreet(cairo_t* cr)
{
  gradient(cr, 1280, 720, rgb(252, 244, 222), rgb(248, 234, 204), rgb(240, 222, 188));
  // Clouds.
  for (const auto& c : {std::array<double, 3>{180, 110, 1.0}, std::array<double, 3>{700, 70, 0.8}, std::array<double, 3>{1050, 130, 1.1}})
  {
    for (const auto& p : {std::array<double, 3>{-50, 10, 34}, std::array<double, 3>{0, -8, 44}, std::array<double, 3>{50, 8, 34}})
      toonOval(cr, c[0] + p[0] * c[2], c[1] + p[1] * c[2], p[2] * c[2], p[2] * c[2] * 0.8);
    setRgbaC(cr, kInk, 1.0);
    cairo_set_line_width(cr, 6);
    cairo_stroke_preserve(cr);
    setRgbaC(cr, kToonWhite, 1.0);
    cairo_fill(cr);
  }
  // The shopfronts.
  struct Shop
  {
    double x, w, h, lean;
    Color c;
    int roof;
    const char* sign;
  };
  static const Shop kShops[] = {
    {20, 190, 330, -0.03, rgb(246, 170, 170), 0, "HATS"},
    {230, 200, 400, 0.02, rgb(170, 220, 190), 1, nullptr},
    {450, 210, 300, -0.02, rgb(250, 220, 140), 2, "PIANOS"},
    {680, 180, 380, 0.03, rgb(160, 200, 236), 0, nullptr},
    {880, 200, 320, -0.025, rgb(200, 180, 230), 1, "BAKERY"},
    {1100, 160, 360, 0.02, rgb(250, 190, 150), 2, nullptr},
  };
  const double base = 560;
  for (const Shop& s : kShops)
  {
    const double top = base - s.h, lx = s.lean * s.h;
    // The walls: wobbly sides, leaning.
    cairo_move_to(cr, s.x, base);
    cairo_curve_to(cr, s.x - 6 + lx * 0.3, base - s.h * 0.4, s.x + 6 + lx * 0.7, base - s.h * 0.7, s.x + lx, top);
    if (s.roof == 1)
      cairo_line_to(cr, s.x + lx + s.w * 0.5, top - 70);
    else if (s.roof == 2)
      cairo_curve_to(cr, s.x + lx + 10, top - 80, s.x + lx + s.w - 10, top - 80, s.x + lx + s.w, top);
    cairo_line_to(cr, s.x + lx + s.w, top);
    cairo_curve_to(cr, s.x + s.w + 6 + lx * 0.7, base - s.h * 0.7, s.x + s.w - 6 + lx * 0.3, base - s.h * 0.4, s.x + s.w, base);
    cairo_close_path(cr);
    toonFill(cr, s.c, 5.0);
    if (s.roof == 0)
    {
      // A cornice.
      cairo_rectangle(cr, s.x + lx - 10, top - 4, s.w + 20, 22);
      toonFill(cr, darken(s.c, 0.25f), 4.0);
    }
    // Windows: two rows of rounded panes with crosses, lit cream.
    const int rows = s.h > 340 ? 3 : 2;
    for (int r = 0; r < rows; ++r)
      for (int c = 0; c < 2; ++c)
      {
        const double yy = top + 50 + r * 84;
        const double xx = s.x + 30 + c * (s.w - 100) + s.lean * (base - yy);
        roundedRect(cr, xx, yy, 44, 54, 12);
        toonFill(cr, rgb(255, 246, 200), 3.4);
        cairo_move_to(cr, xx + 22, yy);
        cairo_line_to(cr, xx + 22, yy + 54);
        cairo_move_to(cr, xx, yy + 27);
        cairo_line_to(cr, xx + 44, yy + 27);
        setRgbaC(cr, kInk, 1.0);
        cairo_set_line_width(cr, 3);
        cairo_stroke(cr);
      }
    // The shop window and its striped awning.
    const double ay = base - 130;
    cairo_rectangle(cr, s.x + 22, ay + 30, s.w - 44, 80);
    toonFill(cr, rgb(190, 226, 240), 3.4);
    cairo_move_to(cr, s.x + 34, ay + 96);
    cairo_line_to(cr, s.x + 64, ay + 44);
    setRgbaC(cr, kToonWhite, 0.8);
    cairo_set_line_width(cr, 5);
    cairo_stroke(cr);
    const int stripes = 6;
    for (int i = 0; i < stripes; ++i)
    {
      const double x0 = s.x + 10 + (s.w - 20) * i / stripes, x1 = s.x + 10 + (s.w - 20) * (i + 1) / stripes;
      cairo_move_to(cr, x0 + 8, ay - 10);
      cairo_line_to(cr, x1 + 8, ay - 10);
      cairo_line_to(cr, x1, ay + 22);
      cairo_curve_to(cr, x1 - 4, ay + 34, x0 + 4, ay + 34, x0, ay + 22);
      cairo_close_path(cr);
      toonFill(cr, i % 2 ? kToonWhite : darken(s.c, 0.35f), 3.0);
    }
    if (s.sign)
    {
      selectGameFont(cr);
      cairo_set_font_size(cr, 26);
      cairo_text_extents_t te;
      cairo_text_extents(cr, s.sign, &te);
      const double sx = s.x + s.w * 0.5 + s.lean * 160, sy = ay - 34;
      roundedRect(cr, sx - te.width * 0.5 - 14, sy - 26, te.width + 28, 38, 10);
      toonFill(cr, kToonCream, 3.4);
      cairo_move_to(cr, sx - te.width * 0.5 - te.x_bearing, sy + 2);
      cairo_text_path(cr, s.sign);
      setRgbaC(cr, rgb(200, 40, 40), 1.0);
      cairo_fill(cr);
    }
  }
  // Curly lampposts.
  for (const double x : {220.0, 670.0, 1090.0})
  {
    cairo_move_to(cr, x, 600);
    cairo_curve_to(cr, x - 8, 500, x + 8, 420, x, 360);
    cairo_curve_to(cr, x - 4, 330, x + 30, 320, x + 26, 344);
    setRgbaC(cr, kInk, 1.0);
    cairo_set_line_width(cr, 8);
    cairo_stroke(cr);
    toonOval(cr, x, 340, 18, 22);
    toonFill(cr, rgb(255, 230, 120), 3.4);
  }
  // The sidewalk, the curb, the road.
  cairo_rectangle(cr, 0, 556, 1280, 84);
  toonFill(cr, rgb(228, 214, 190), 4.0);
  for (int i = 0; i < 16; ++i)
  {
    cairo_move_to(cr, 40 + i * 80, 560);
    cairo_line_to(cr, 30 + i * 80, 636);
  }
  setRgbaC(cr, rgb(150, 130, 110), 0.8);
  cairo_set_line_width(cr, 2.4);
  cairo_stroke(cr);
  cairo_rectangle(cr, -4, 638, 1288, 14);
  toonFill(cr, rgb(250, 246, 236), 3.4);
  cairo_rectangle(cr, -4, 652, 1288, 80);
  toonFill(cr, rgb(130, 130, 156), 3.4);
  for (int i = 0; i < 8; ++i)
  {
    cairo_rectangle(cr, 40 + i * 160, 684, 80, 10);
    setRgbaC(cr, kToonCream, 1.0);
    cairo_fill(cr);
  }
}

const Texture& toonStreet(ClipKit& k, bool soft)
{
  if (soft)
    return cached(k, "e4_toon_street_soft", 1280, 720, 0, 0, [](cairo_t* cr) {
      paintToonStreet(cr);
      defocus(cr, 5);
    });
  return cached(k, "e4_toon_street", 1280, 720, 0, 0, [](cairo_t* cr) { paintToonStreet(cr); });
}

// An old cartoon's grain: a couple of scratches and a flicker.
void filmGrain(ClipKit& k, int ticks, int alpha)
{
  for (int i = 0; i < 3; ++i)
  {
    const unsigned h = hash2(ticks / 3, i * 7 + 1);
    if (h % 3u == 0)
      continue;
    const float x = float(h % 1280u);
    k.r.fillRect(x, 0, 2, H, rgba(60, 40, 20, alpha));
  }
  const unsigned f = hash2(ticks / 2, 99);
  k.r.fillRect(0, 0, W, H, rgba(255, 240, 200, int(f % 12u)));
}

// The concentric rings behind the Channel 1 ident, 4 steps of their pulse
// outward (two rings a loop, so it joins).
const Texture& ch1Rings(ClipKit& k, int f)
{
  return cached(k, "e4_ch1_rings" + std::to_string(f & 3), 1280, 720, 0, 0, [f](cairo_t* cr) {
    const double cx = 640, cy = 300, ring = 72;
    for (int i = 20; i >= 0; --i)
    {
      const double r = (double(i) + double(f & 3) * 0.5) * ring;
      if (r <= 0.0)
        continue;
      cairo_arc(cr, cx, cy, r, 0, 2 * kPi);
      setRgbaC(cr, i % 2 ? rgb(232, 64, 52) : rgb(250, 200, 70), 1.0);
      cairo_fill_preserve(cr);
      setRgbaC(cr, kInk, 1.0);
      cairo_set_line_width(cr, 5);
      cairo_stroke(cr);
    }
    // The old film's vignette.
    cairo_pattern_t* g = cairo_pattern_create_radial(cx, 360, 320, cx, 360, 820);
    cairo_pattern_add_color_stop_rgba(g, 0, 0, 0, 0, 0);
    cairo_pattern_add_color_stop_rgba(g, 1, 0.1, 0.04, 0.0, 0.7);
    cairo_rectangle(cr, 0, 0, 1280, 720);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  });
}

// Inked display lettering centred on (x, y): a drop shadow, a thick black
// outline, a fill lit from above.
void toonLetters(cairo_t* cr, const char* s, double x, double y, double size, Color fill, double angle = 0.0)
{
  selectGameFont(cr);
  cairo_set_font_size(cr, size);
  cairo_text_extents_t te;
  cairo_text_extents(cr, s, &te);
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_rotate(cr, angle);
  for (int pass = 0; pass < 3; ++pass)
  {
    const double o = pass == 0 ? size * 0.06 : 0.0;
    cairo_move_to(cr, -te.width * 0.5 - te.x_bearing + o, te.height * 0.5 + o);
    cairo_text_path(cr, s);
    if (pass == 0)
    {
      setRgbaC(cr, kInk, 0.6);
      cairo_fill(cr);
    }
    else if (pass == 1)
    {
      setRgbaC(cr, kInk, 1.0);
      cairo_set_line_width(cr, size * 0.1);
      cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
      cairo_stroke(cr);
    }
    else
    {
      cairo_pattern_t* g = cairo_pattern_create_linear(0, -te.height * 0.5, 0, te.height * 0.5);
      const Color hi = lighten(fill, 0.35f);
      cairo_pattern_add_color_stop_rgb(g, 0, redOf(hi) / 255.0, greenOf(hi) / 255.0, blueOf(hi) / 255.0);
      cairo_pattern_add_color_stop_rgb(g, 1, redOf(fill) / 255.0, greenOf(fill) / 255.0, blueOf(fill) / 255.0);
      cairo_set_source(cr, g);
      cairo_fill(cr);
      cairo_pattern_destroy(g);
    }
  }
  cairo_restore(cr);
}

// "CARTOONS" with every letter bobbing and rocking (4 wobble steps).
const Texture& cartoonsWord(ClipKit& k, int f)
{
  return cached(k, "e4_cartoons" + std::to_string(f & 3), 1000, 220, 500, 110, [f](cairo_t* cr) {
    static const char* kLetters[] = {"C", "A", "R", "T", "O", "O", "N", "S"};
    static const Color kFill[] = {rgb(255, 214, 72), rgb(90, 200, 240), rgb(255, 130, 180), rgb(140, 214, 100)};
    for (int i = 0; i < 8; ++i)
    {
      const double ph = double(i) * 1.3 + double(f & 3) * kPi * 0.5;
      toonLetters(cr, kLetters[i], 90 + i * 117, 110 + std::sin(ph) * 14, 120, kFill[i % 4], std::sin(ph + 0.8) * 0.16);
    }
  });
}

// Shot: the Channel 1 ident. Cartoon rings pulse outward; "CH 1" drops in and
// bounces with a squash (frames 0-5); "CARTOONS" pops up under it, every
// letter wobbling.
void identCh1(ClipKit& k, int ticks, float ox, float oy)
{
  const float abs = float(ticks) / 4.8f; // 80 ms a frame
  k.r.draw(ch1Rings(k, (ticks / 5) & 3), ox, oy);
  const Texture& badge = cached(k, "e4_ch1_badge", 640, 300, 320, 150, [](cairo_t* cr) {
    toonLetters(cr, "CH 1", 320, 150, 210, kToonCream);
  });
  // The drop and the bounce: (time, y, x-squash, y-squash) keys.
  static constexpr float kKeys[6][4] = {{0.0f, -240.0f, 0.88f, 1.2f}, {1.9f, 250.0f, 0.86f, 1.22f}, {2.4f, 262.0f, 1.34f, 0.66f},
                                        {3.2f, 214.0f, 0.88f, 1.14f}, {4.0f, 252.0f, 1.08f, 0.93f}, {4.8f, 250.0f, 1.0f, 1.0f}};
  float y = 250.0f, sx = 1.0f, sy = 1.0f;
  for (int i = 0; i < 5; ++i)
    if (abs >= kKeys[i][0] && abs < kKeys[i + 1][0])
    {
      float u = (abs - kKeys[i][0]) / (kKeys[i + 1][0] - kKeys[i][0]);
      if (i == 0)
        u = u * u; // falling, faster and faster
      y = kKeys[i][1] + (kKeys[i + 1][1] - kKeys[i][1]) * u;
      sx = kKeys[i][2] + (kKeys[i + 1][2] - kKeys[i][2]) * u;
      sy = kKeys[i][3] + (kKeys[i + 1][3] - kKeys[i][3]) * u;
    }
  // Squash about the badge's bottom edge.
  const float bw = 640.0f * sx, bh = 300.0f * sy, bottom = y + 150.0f;
  stretchTex(k, badge, 640.0f - bw * 0.5f + ox, bottom - bh + oy, bw, bh, rgb(255, 255, 255), 1.0f);
  if (abs >= 2.2f && abs < 3.4f)
  {
    // The impact's puff lines.
    const float u = (abs - 2.2f) / 1.2f;
    for (int i = 0; i < 6; ++i)
    {
      const float side = i < 3 ? -1.0f : 1.0f, a = float(i % 3) * 0.35f + 0.1f;
      const float x0 = 640.0f + side * (250.0f + u * 60.0f), y0 = bottom - 30.0f - float(i % 3) * 40.0f;
      k.r.drawLine(x0 + ox, y0 + oy, x0 + side * std::cos(a) * 50.0f + ox, y0 - std::sin(a) * 50.0f + oy, 7.0f,
        rgba(10, 8, 20, int(255 * (1.0f - u))));
    }
  }
  if (abs >= 5.0f)
  {
    const float u = std::min(1.0f, (abs - 5.0f) / 1.6f);
    DrawOpts wo;
    wo.scale = u < 0.7f ? 0.3f + u / 0.7f * 0.85f : 1.15f - (u - 0.7f) / 0.3f * 0.15f;
    k.r.draw(cartoonsWord(k, (ticks / 4) & 3), 640.0f + ox, 500.0f + oy, wo);
  }
  k.r.drawText("MAXTV  CHANNEL 1  CARTOONS", 640 + ox, 676 + oy, {18.0f, kToonCream, rgb(90, 10, 16)}, Align::Center);
  filmGrain(k, ticks, 50);
}

// A little inked music note (rising from Lance's microphone).
const Texture& musicNote(ClipKit& k)
{
  return cached(k, "e4_note", 60, 80, 30, 40, [](cairo_t* cr) {
    toonOval(cr, 22, 60, 13, 10, -0.4);
    setRgbaC(cr, kInk, 1.0);
    cairo_fill(cr);
    cairo_rectangle(cr, 31, 12, 5, 48);
    cairo_fill(cr);
    cairo_move_to(cr, 36, 12);
    cairo_curve_to(cr, 48, 20, 54, 30, 46, 44);
    cairo_curve_to(cr, 48, 32, 42, 26, 36, 26);
    cairo_close_path(cr);
    cairo_fill(cr);
  });
}

// Shot: Lance as a cartoon, dancing on a painted street that scrolls by,
// the three runners beside him in rubber hose, bouncing to the same beat
// (a 4-frame loop), music notes rising.
void lanceToon(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  const float abs = float(ticks) / 5.4f; // 90 ms a frame
  panLayer(k, toonStreet(k, false), abs * 2.0f, oy);
  const int b = frame & 3;
  const float lx = 400.0f + ox, fy = 616.0f + oy;
  // Shadows under everyone, smaller when they are up on their toes.
  const float shrink = b == 2 ? 0.8f : 1.0f;
  const Texture& shadow = cached(k, "e4_toon_shadow", 200, 40, 100, 20, [](cairo_t* cr) {
    toonOval(cr, 100, 20, 96, 16);
    setRgbaC(cr, rgb(60, 40, 30), 0.3);
    cairo_fill(cr);
  });
  for (int i = 0; i < 4; ++i)
  {
    DrawOpts o;
    o.scale = shrink * (i == 0 ? 1.2f : 0.9f);
    k.r.draw(shadow, (i == 0 ? 400.0f : 740.0f + float(i - 1) * 165.0f) + ox, fy + 2.0f, o);
  }
  drawGlow(k.r, k.art, lx, fy - 260.0f, 260.0f, rgb(255, 240, 200), 0.25f);
  k.r.draw(toonLance(k, b, b == 1 || b == 2, 1.0f), lx, fy);
  static constexpr int kWho[3] = {0, 2, 1};
  for (int i = 0; i < 3; ++i)
    k.r.draw(toonRunner(k, kWho[i], b, false, 0.82f, false), 740.0f + float(i) * 165.0f + ox, fy);
  // Notes floating up from the microphone.
  for (int i = 0; i < 4; ++i)
  {
    const float u = std::fmod(abs / 8.0f + float(i) * 0.25f, 1.0f);
    DrawOpts no;
    no.alpha = std::sin(u * 3.14159f);
    no.angle = std::sin(u * 9.0f + float(i)) * 20.0f;
    k.r.draw(musicNote(k), lx + 110.0f + u * 160.0f + std::sin(u * 12.0f) * 16.0f, fy - 380.0f - u * 220.0f, no);
  }
  filmGrain(k, ticks, 40);
}

// Shot: close on Nova leaning in to whisper to Dash; Lance dancing out of
// focus behind them.
void novaWhisper(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  (void)frame;
  k.r.draw(toonStreet(k, true), ox * 0.3f, oy * 0.3f);
  // Lance behind them, out of focus, still dancing.
  const int b = (ticks / 5) & 3;
  k.r.draw(toonLance(k, b, b == 1 || b == 2, 0.9f, true), 640.0f + ox * 0.5f, 600.0f + oy * 0.5f);
  k.r.fillRect(0, 0, W, H, rgba(250, 236, 210, 70));
  // Dash, cocking his head; Nova leaning to his ear, talking behind her glove.
  const bool talking = ticks < 150 && (ticks / 7) % 2 == 0;
  k.r.draw(toonRunner(k, 0, 5, false, 2.4f, false), 330.0f + ox, 900.0f + oy);
  k.r.draw(toonRunner(k, 2, 4, talking, 2.4f, true), 960.0f + ox, 916.0f + oy);
  filmGrain(k, ticks, 36);
}

// The big top's inside, 1280 x 720: red and cream canvas raking up to the
// peak, the scalloped valance, two spotlights, the stage boards and the
// red apron with its bulbs.
const Texture& bigTop(ClipKit& k)
{
  return cached(k, "e4_bigtop", 1280, 720, 0, 0, [](cairo_t* cr) {
    for (int i = -14; i < 14; ++i)
    {
      cairo_move_to(cr, 640, -260);
      cairo_line_to(cr, 640 + i * 110, 720);
      cairo_line_to(cr, 640 + (i + 1) * 110, 720);
      cairo_close_path(cr);
      setRgbaC(cr, (i & 1) ? rgb(240, 226, 198) : rgb(200, 44, 48), 1.0);
      cairo_fill(cr);
    }
    cairo_pattern_t* g = cairo_pattern_create_radial(640, 320, 200, 640, 320, 820);
    cairo_pattern_add_color_stop_rgba(g, 0, 0, 0, 0, 0.05);
    cairo_pattern_add_color_stop_rgba(g, 1, 0.12, 0.02, 0.04, 0.65);
    cairo_rectangle(cr, 0, 0, 1280, 720);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
    // Spotlights.
    for (const double x : {300.0, 980.0})
    {
      cairo_move_to(cr, x - 20, 0);
      cairo_line_to(cr, x + 20, 0);
      cairo_line_to(cr, 640 + (x - 640) * 0.3 + 170, 640);
      cairo_line_to(cr, 640 + (x - 640) * 0.3 - 170, 640);
      cairo_close_path(cr);
      setRgbaC(cr, rgb(255, 250, 220), 0.12);
      cairo_fill(cr);
    }
    // The valance.
    cairo_rectangle(cr, -4, -4, 1288, 50);
    toonFill(cr, rgb(170, 30, 40), 4.0);
    for (int i = 0; i < 16; ++i)
    {
      cairo_move_to(cr, i * 80.0, 44);
      cairo_curve_to(cr, i * 80.0 + 10, 92, i * 80.0 + 70, 92, i * 80.0 + 80, 44);
      cairo_close_path(cr);
      toonFill(cr, (i & 1) ? rgb(250, 200, 70) : rgb(200, 40, 46), 3.4);
    }
    // The stage.
    cairo_rectangle(cr, -4, 600, 1288, 70);
    toonFill(cr, rgb(196, 136, 76), 4.0);
    for (int i = 0; i < 20; ++i)
    {
      cairo_move_to(cr, i * 70.0 + 20, 602);
      cairo_line_to(cr, i * 70.0, 668);
    }
    setRgbaC(cr, rgb(120, 70, 30), 0.7);
    cairo_set_line_width(cr, 2.4);
    cairo_stroke(cr);
    cairo_rectangle(cr, -4, 668, 1288, 60);
    toonFill(cr, rgb(170, 30, 40), 4.0);
    for (int i = 0; i < 22; ++i)
    {
      toonOval(cr, 30 + i * 60.0, 694, 9, 9);
      toonFill(cr, rgb(255, 236, 150), 2.4);
    }
  });
}

// Buddy Boom at deflate stage d (0 full .. 4 the empty skin), anchored at his
// feet, or at his middle to tumble through the air.
const Texture& buddyDeflating(ClipKit& k, int d, bool middle)
{
  const std::string key = "e4_buddy_deflate" + std::to_string(d) + (middle ? "m" : "");
  return cached(k, key, 600, 660, 300, middle ? 420 : 620, [d](cairo_t* cr) { paintBuddy(cr, 300, 620, 1.15, 14, d); });
}

// Shot: Buddy Boom lets go like a balloon: the air whistles out of his nose
// as he sags (frames 0-2), he rockets round the big top in loops, shrinking
// (3-9), and his empty skin flutters down onto the stage (10-11).
void buddyDeflate(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  (void)frame;
  const float abs = std::min(12.0f, float(ticks) / 4.8f); // 80 ms a frame
  k.r.draw(bigTop(k), ox, oy);
  auto flight = [](float u, float& x, float& y) {
    x = 640.0f + 440.0f * std::sin(u * 6.2832f * 1.25f);
    y = 330.0f - 170.0f * std::sin(u * 6.2832f * 2.1f) + 40.0f * u;
  };
  if (abs < 3.0f)
  {
    // Sagging on the spot, air jetting from the nose.
    const int d = abs < 1.0f ? 0 : (abs < 2.0f ? 1 : 2);
    const float fx = 640.0f + ox, fy = 640.0f + oy;
    k.r.draw(buddyDeflating(k, d, false), fx + std::sin(float(ticks) * 1.9f) * 3.0f, fy);
    const float nx = fx + 144.0f * 1.15f, ny = fy - 352.0f * 1.15f * (1.0f - float(d) * 0.1f);
    for (int i = 0; i < 5; ++i)
    {
      const float u = std::fmod(float(ticks) * 0.12f + float(i) * 0.2f, 1.0f);
      const float a = -0.3f + float(i) * 0.15f;
      const float x0 = nx + 30.0f + u * 160.0f, y0 = ny + std::sin(a) * u * 120.0f;
      k.r.drawLine(x0, y0, x0 + 40.0f, y0 + std::sin(a) * 30.0f, 5.0f, rgba(255, 255, 255, int(220 * (1.0f - u))));
    }
    return;
  }
  if (abs < 9.5f)
  {
    // Zooming round: a dotted trail, tumbling along his path.
    const float u = (abs - 3.0f) / 6.5f;
    for (int i = 1; i <= 12; ++i)
    {
      const float pu = u - float(i) * 0.012f;
      if (pu < 0.0f)
        break;
      float tx = 0, ty = 0;
      flight(pu, tx, ty);
      k.r.fillRect(tx - 4.0f + ox, ty - 4.0f + oy, 8, 8, rgba(10, 8, 20, 200 - i * 15));
    }
    float x = 0, y = 0, px = 0, py = 0;
    flight(u, x, y);
    flight(u - 0.01f, px, py);
    DrawOpts o;
    o.scale = 0.75f - 0.3f * u;
    o.angle = std::atan2(x - px, -(y - py)) * 57.2958f;
    k.r.draw(buddyDeflating(k, 3, true), x + ox, y + oy, o);
    return;
  }
  // The empty skin, see-sawing down onto the stage.
  float sx = 0, sy = 0;
  flight(1.0f, sx, sy);
  const float u = std::min(1.0f, (abs - 9.5f) / 1.6f);
  DrawOpts o;
  o.scale = 0.6f;
  o.angle = u < 1.0f ? std::sin(abs * 4.0f) * 30.0f * (1.0f - u) : 0.0f;
  // (Landed, his middle anchor sits 200 px x 0.6 over the stage.)
  const float x = sx + (640.0f - sx) * u + std::sin(u * 9.0f) * 40.0f * (1.0f - u), y = sy + (492.0f - sy) * u * u;
  k.r.draw(buddyDeflating(k, 4, true), x + ox, y + oy, o);
}

// Shot: the cartoon's iris closes on the runners on the empty stage
// (frames 0-5), the dead channel's snow showing round it, and Nova kicks it
// open: her boot jams the rim (6) and it shudders, held.
void irisClip(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  (void)frame;
  const float abs = std::min(10.0f, float(ticks) / 5.4f); // 90 ms a frame
  const float cx = 640.0f + ox, cy = 430.0f + oy;
  // The radius: closing faster and faster, then caught and wobbling.
  float rad = 0.0f;
  if (abs < 6.0f)
  {
    const float u = abs / 6.0f;
    rad = 900.0f - (900.0f - 196.0f) * u * (0.6f + 0.4f * u);
  }
  else
    rad = 212.0f + 18.0f * std::sin((abs - 6.0f) * 9.0f) * std::exp(-(abs - 6.0f) * 1.1f);
  // Inside: the stage, Buddy's empty skin, the three of them looking up.
  k.r.draw(bigTop(k), ox, oy);
  DrawOpts so;
  so.scale = 0.42f;
  k.r.draw(buddyDeflating(k, 4, false), 330.0f + ox, 636.0f + oy, so);
  const bool kick = abs >= 5.6f;
  k.r.draw(toonRunner(k, 0, 7, false, 0.85f, false), 520.0f + ox, 616.0f + oy);
  k.r.draw(toonRunner(k, 1, 7, false, 0.85f, false), 640.0f + ox, 616.0f + oy);
  k.r.draw(toonRunner(k, 2, kick ? 6 : 7, false, 0.85f, false), 760.0f + ox, 616.0f + oy);
  // Outside the circle: the snow, row by row.
  const Texture& snow = snowFrame(k, (ticks / 3) & 3);
  DrawOpts sn;
  sn.scale = 2.0f;
  sn.tint = rgb(170, 170, 180);
  for (float y = 0.0f; y < H; y += 4.0f)
  {
    const float dy = y + 2.0f - cy;
    const float half = std::abs(dy) < rad ? std::sqrt(rad * rad - dy * dy) : -1.0f;
    if (half < 0.0f)
    {
      clipTo(k, 0.0f, y, W, 4.0f);
      k.r.draw(snow, 0, 0, sn);
      continue;
    }
    clipTo(k, 0.0f, y, std::max(0.0f, cx - half), 4.0f);
    k.r.draw(snow, 0, 0, sn);
    clipTo(k, cx + half, y, std::max(0.0f, W - cx - half), 4.0f);
    k.r.draw(snow, 0, 0, sn);
  }
  unclip(k);
  // The iris's inked rim.
  const Texture& rim = cached(k, "e4_iris_rim", 640, 640, 320, 320, [](cairo_t* cr) {
    cairo_arc(cr, 320, 320, 300, 0, 2 * kPi);
    setRgbaC(cr, kInk, 1.0);
    cairo_set_line_width(cr, 22);
    cairo_stroke(cr);
    cairo_arc(cr, 320, 320, 286, 0, 2 * kPi);
    setRgbaC(cr, rgb(80, 70, 90), 1.0);
    cairo_set_line_width(cr, 4);
    cairo_stroke(cr);
  });
  DrawOpts ro;
  ro.scale = rad / 300.0f;
  ro.cull = false;
  k.r.draw(rim, cx, cy, ro);
  if (!kick)
    return;
  // Nova's leg stretched out like a hose, her boot wedged in the rim.
  const float hipX = 760.0f + ox, hipY = 616.0f - 90.0f * 0.85f + oy;
  const float a = -0.3f;
  const float bootX = cx + std::cos(a) * (rad - 6.0f), bootY = cy + std::sin(a) * (rad - 6.0f);
  const float bendX = (hipX + bootX) * 0.5f + 10.0f, bendY = std::max(hipY, bootY) + 30.0f;
  float px = hipX, py = hipY;
  for (int i = 1; i <= 12; ++i)
  {
    const float t = float(i) / 12.0f;
    const float qx = (1 - t) * (1 - t) * hipX + 2 * (1 - t) * t * bendX + t * t * bootX;
    const float qy = (1 - t) * (1 - t) * hipY + 2 * (1 - t) * t * bendY + t * t * bootY;
    k.r.drawLine(px, py, qx, qy, 18.0f, kInk);
    px = qx;
    py = qy;
  }
  px = hipX;
  py = hipY;
  for (int i = 1; i <= 12; ++i)
  {
    const float t = float(i) / 12.0f;
    const float qx = (1 - t) * (1 - t) * hipX + 2 * (1 - t) * t * bendX + t * t * bootX;
    const float qy = (1 - t) * (1 - t) * hipY + 2 * (1 - t) * t * bendY + t * t * bootY;
    k.r.drawLine(px, py, qx, qy, 12.0f, rgb(132, 66, 216));
    px = qx;
    py = qy;
  }
  const Texture& boot = cached(k, "e4_nova_boot", 120, 90, 40, 45, [](cairo_t* cr) {
    roundedRect(cr, 14, 18, 44, 50, 14);
    toonFill(cr, rgb(238, 238, 248), 3.4);
    toonOval(cr, 66, 52, 46, 22);
    toonFill(cr, rgb(238, 238, 248), 3.4);
    cairo_rectangle(cr, 24, 66, 90, 10);
    toonFill(cr, rgb(100, 100, 120), 2.6);
    toonOval(cr, 62, 42, 14, 6, -0.2);
    setRgbaC(cr, kToonWhite, 0.8);
    cairo_fill(cr);
  });
  DrawOpts bo;
  bo.angle = a * 57.2958f - 90.0f;
  bo.cull = false;
  k.r.draw(boot, bootX, bootY, bo);
  if (abs < 8.0f)
  {
    // The clang: a burst of lines round the boot.
    const float u = (abs - 5.6f) / 2.4f;
    for (int i = 0; i < 10; ++i)
    {
      const float t = float(i) * 0.628f;
      const float r0 = 46.0f + u * 30.0f, r1 = r0 + 40.0f * (1.0f - u);
      k.r.drawLine(bootX + std::cos(t) * r0, bootY + std::sin(t) * r0, bootX + std::cos(t) * r1, bootY + std::sin(t) * r1, 6.0f,
        rgb(255, 240, 120));
    }
  }
}

// Shot: out of the cartoon: the three of them dive into the snow, one
// after another, shrinking away into it; on frame 4 it swallows them in a
// flash and ripples close over the spot.
void jumpStatic(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  staticSnow(k, frame, ticks, ox, oy);
  const float abs = float(ticks) / 4.8f; // 80 ms a frame
  const float tx = 640.0f + ox, ty = 360.0f + oy;
  for (int who = 0; who < 3; ++who)
  {
    const float u = (abs - float(who) * 0.45f) / 3.4f;
    if (u < 0.0f || u >= 1.0f)
      continue;
    const float sx = 160.0f + float(who) * 140.0f, sy = 820.0f;
    const float e = u * u;
    const float x = sx + (tx - sx) * e, y = sy + (ty - sy) * e - std::sin(u * 3.14159f) * 260.0f * (1.0f - u);
    DrawOpts o;
    o.scale = (3.4f - 3.1f * e) / 3.0f;
    o.angle = -10.0f + 100.0f * u;
    o.alpha = u > 0.8f ? (1.0f - u) / 0.2f : 1.0f;
    k.r.draw(runner(k, who, 2, 3.0f), x, y, o);
  }
  if (abs >= 3.8f)
  {
    // The splash into the snow: a flash and rings spreading out.
    const float u = (abs - 3.8f) / 4.2f;
    if (u < 0.3f)
      k.r.fillRect(0, 0, W, H, rgba(255, 255, 255, int(200 * (1.0f - u / 0.3f))), Blend::Add);
    const Texture& ring = cached(k, "e4_static_ring", 400, 200, 200, 100, [](cairo_t* cr) {
      cairo_save(cr);
      cairo_translate(cr, 200, 100);
      cairo_scale(cr, 1.0, 0.5);
      cairo_arc(cr, 0, 0, 180, 0, 2 * kPi);
      cairo_restore(cr);
      setRgbaC(cr, rgb(255, 255, 255), 0.9);
      cairo_set_line_width(cr, 8);
      cairo_stroke(cr);
    });
    for (int i = 0; i < 3; ++i)
    {
      const float ru = std::clamp(u * 1.4f - float(i) * 0.2f, 0.0f, 1.0f);
      if (ru <= 0.0f || ru >= 1.0f)
        continue;
      DrawOpts o;
      o.scale = 0.2f + ru * 2.6f;
      o.alpha = 1.0f - ru;
      o.blend = Blend::Add;
      o.cull = false;
      k.r.draw(ring, tx, ty, o);
    }
  }
}

// Lance, horrified: his own art (arms flung wide) with the eyes popped
// wide, the brows up and the jaw dropped; baked at 1.45x (feet at the
// anchor).
const Texture& lanceHorror(ClipKit& k, bool wide)
{
  constexpr double s = 1.45;
  return cached(k, std::string("e4_lance_horror") + (wide ? "w" : "n"), int(320 * s), int(600 * s), float(160 * s), float(580 * s),
    [wide](cairo_t* cr) {
    cairo_scale(cr, s, s);
    cairo_translate(cr, 40, 100);
    paintLance(cr, 8, false);
    const double hx = 126, hy = 112;
    // Paint the old brow and eye out with the face's own shading.
    cairo_pattern_t* skin = cairo_pattern_create_linear(hx - 34, hy - 40, hx + 40, hy + 48);
    cairo_pattern_add_color_stop_rgb(skin, 0, 244 / 255.0, 204 / 255.0, 168 / 255.0);
    cairo_pattern_add_color_stop_rgb(skin, 1, 206 / 255.0, 156 / 255.0, 120 / 255.0);
    toonOval(cr, hx + 18, hy - 12, 13, 10);
    cairo_set_source(cr, skin);
    cairo_fill(cr);
    cairo_pattern_destroy(skin);
    // Brows shot up, the eye a white saucer with a pinprick.
    cairo_move_to(cr, hx + 4, hy - 26);
    cairo_curve_to(cr, hx + 12, hy - 36, hx + 22, hy - 36, hx + 30, hy - 30);
    setRgbaC(cr, rgb(150, 100, 30), 1.0);
    cairo_set_line_width(cr, 3.4);
    cairo_stroke(cr);
    toonOval(cr, hx + 19, hy - 8, 9, 11);
    toonFill(cr, kToonWhite, 1.8);
    toonOval(cr, hx + 21, hy - 7, 2, 2.4);
    setRgbaC(cr, kInk, 1.0);
    cairo_fill(cr);
    // The jaw dropped in a scream.
    toonOval(cr, hx + 22, hy + 34, wide ? 15 : 12, wide ? 18 : 13);
    toonFill(cr, rgb(90, 16, 30), 2.2);
    cairo_rectangle(cr, hx + 12, hy + (wide ? 18 : 23), 21, 5);
    setRgbaC(cr, kToonWhite, 1.0);
    cairo_fill(cr);
    // Sweat.
    for (const auto& d : {std::array<double, 2>{hx - 10, hy - 30}, std::array<double, 2>{hx + 40, hy - 26}})
    {
      cairo_move_to(cr, d[0], d[1] - 8);
      cairo_curve_to(cr, d[0] + 6, d[1], d[0] + 5, d[1] + 6, d[0], d[1] + 6);
      cairo_curve_to(cr, d[0] - 5, d[1] + 6, d[0] - 6, d[1], d[0], d[1] - 8);
      cairo_close_path(cr);
      toonFill(cr, rgb(180, 220, 255), 1.4);
    }
  });
}

// The gallery: dark, a bank of small monitors all showing snow, the desk's
// lit buttons, and a big monitor's bezel in the middle (its screen is
// (330, 80) .. (950, 480)).
const Texture& controlRoom(ClipKit& k)
{
  return cached(k, "e4_control_room", 1280, 720, 0, 0, [](cairo_t* cr) {
    gradient(cr, 1280, 720, rgb(16, 18, 30), rgb(22, 24, 40), rgb(10, 10, 18));
    for (int i = 0; i < 8; ++i)
    {
      const double x = i < 4 ? 30 + (i % 2) * 140 : 970 + (i % 2) * 140, y = 70 + ((i / 2) % 2) * 120;
      roundedRect(cr, x, y, 126, 96, 8);
      setRgbaC(cr, rgb(40, 40, 52), 1.0);
      cairo_fill(cr);
    }
    roundedRect(cr, 300, 50, 680, 460, 22);
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 50, 0, 510);
    cairo_pattern_add_color_stop_rgb(g, 0, 0.28, 0.28, 0.34);
    cairo_pattern_add_color_stop_rgb(g, 1, 0.12, 0.12, 0.16);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
    // The desk.
    cairo_move_to(cr, 0, 560);
    cairo_line_to(cr, 1280, 560);
    cairo_line_to(cr, 1280, 720);
    cairo_line_to(cr, 0, 720);
    cairo_close_path(cr);
    setRgbaC(cr, rgb(30, 30, 40), 1.0);
    cairo_fill(cr);
    cairo_rectangle(cr, 0, 556, 1280, 8);
    setRgbaC(cr, rgb(70, 70, 86), 1.0);
    cairo_fill(cr);
    std::uint32_t sd = 777u;
    for (int i = 0; i < 60; ++i)
    {
      sd = sd * 1664525u + 1013904223u;
      const double x = 40 + (i % 30) * 41, y = 590 + (i / 30) * 40;
      static const Color kLit[] = {rgb(255, 80, 70), rgb(90, 220, 120), rgb(255, 200, 70), rgb(80, 160, 255)};
      roundedRect(cr, x, y, 24, 16, 3);
      setRgbaC(cr, (sd >> 8) % 3u ? rgb(60, 60, 74) : kLit[(sd >> 12) % 4u], 1.0);
      cairo_fill(cr);
    }
  });
}

// Shot: the gallery. Lance on the big monitor, horrified, shaking, arms
// flung up (a 4-frame loop), the feed tearing; ON AIR blinking.
void lancePanic(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  k.r.draw(controlRoom(k), ox, oy);
  for (int i = 0; i < 8; ++i)
  {
    const float x = (i < 4 ? 30.0f + float(i % 2) * 140.0f : 970.0f + float(i % 2) * 140.0f) + 8.0f;
    const float y = 70.0f + float((i / 2) % 2) * 120.0f + 8.0f;
    clipTo(k, x + ox, y + oy, 110, 80);
    staticNoise(k, ticks / 2 + i * 13, 0.85f, x + ox, y + oy, 110, 80);
  }
  unclip(k);
  const float sx = 330.0f + ox, sy = 80.0f + oy, sw = 620.0f, sh = 400.0f;
  clipTo(k, sx, sy, sw, sh);
  k.r.fillRect(sx, sy, sw, sh, rgb(60, 20, 70));
  drawGlow(k.r, k.art, sx + sw * 0.5f, sy + sh * 0.4f, 360.0f, rgb(255, 120, 160), 0.35f);
  static constexpr float kShake[4][2] = {{-7, 2}, {6, -3}, {-3, -5}, {7, 4}};
  const int f = frame & 3;
  k.r.draw(lanceHorror(k, f % 2 == 1), sx + sw * 0.5f - 10.0f + kShake[f][0], sy + 830.0f + kShake[f][1]);
  // The feed tearing.
  for (int i = 0; i < 3; ++i)
  {
    const unsigned h = hash2(ticks / 3, i);
    const float y = sy + float(h % 400u);
    k.r.fillRect(sx, y, sw, 3.0f + float(h % 5u), rgba(255, 255, 255, 40));
  }
  scanlines(k, 60, sy, sy + sh, sx, sw);
  unclip(k);
  // ON AIR.
  const bool lit = (ticks / 15) % 2 == 0;
  k.r.fillRect(840 + ox, 54 + oy, 100, 22, lit ? rgb(230, 30, 40) : rgb(70, 20, 24));
  k.r.drawText("ON AIR", 890 + ox, 56 + oy, {15.0f, lit ? rgb(255, 240, 240) : rgb(120, 60, 60), 0}, Align::Center);
  if (lit)
    drawGlow(k.r, k.art, 890 + ox, 65 + oy, 110.0f, rgb(255, 60, 60), 0.3f);
}

// The Halcyon Arms rifle, sleek white and gold, 760 x 240, side on (the
// mirrored bake is its other side).
const Texture& halcyonRifle(ClipKit& k, bool back)
{
  return cached(k, std::string("e4_halcyon_rifle") + (back ? "b" : "f"), 760, 240, 380, 120, [back](cairo_t* cr) {
    if (back)
    {
      cairo_translate(cr, 760, 0);
      cairo_scale(cr, -1, 1);
    }
    auto shape = [cr]() {
      cairo_move_to(cr, 40, 110);
      cairo_curve_to(cr, 60, 70, 200, 66, 320, 70);
      cairo_line_to(cr, 640, 76);
      cairo_line_to(cr, 730, 84);
      cairo_line_to(cr, 730, 104);
      cairo_line_to(cr, 560, 112);
      cairo_line_to(cr, 470, 124);
      cairo_line_to(cr, 440, 196);
      cairo_line_to(cr, 396, 196);
      cairo_line_to(cr, 400, 128);
      cairo_line_to(cr, 250, 130);
      cairo_curve_to(cr, 180, 150, 90, 190, 52, 186);
      cairo_curve_to(cr, 30, 170, 30, 130, 40, 110);
      cairo_close_path(cr);
    };
    shape();
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 66, 0, 196);
    cairo_pattern_add_color_stop_rgb(g, 0, 1, 1, 1);
    cairo_pattern_add_color_stop_rgb(g, 0.45, 0.86, 0.88, 0.92);
    cairo_pattern_add_color_stop_rgb(g, 1, 0.5, 0.52, 0.6);
    cairo_set_source(cr, g);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(g);
    setRgbaC(cr, rgb(40, 44, 60), 1.0);
    cairo_set_line_width(cr, 2.4);
    cairo_stroke(cr);
    // The gold inlays and the scope.
    cairo_move_to(cr, 120, 104);
    cairo_line_to(cr, 620, 92);
    setRgbaC(cr, rgb(230, 190, 80), 1.0);
    cairo_set_line_width(cr, 5);
    cairo_stroke(cr);
    roundedRect(cr, 300, 40, 180, 24, 12);
    cairo_pattern_t* s = cairo_pattern_create_linear(0, 40, 0, 64);
    cairo_pattern_add_color_stop_rgb(s, 0, 0.3, 0.32, 0.4);
    cairo_pattern_add_color_stop_rgb(s, 1, 0.08, 0.08, 0.12);
    cairo_set_source(cr, s);
    cairo_fill(cr);
    cairo_pattern_destroy(s);
    cairo_arc(cr, 480, 52, 12, 0, 2 * kPi);
    setRgbaC(cr, rgb(90, 200, 255), 0.9);
    cairo_fill(cr);
    // The halo emblem on the stock.
    cairo_arc(cr, 140, 140, 18, 0, 2 * kPi);
    setRgbaC(cr, rgb(230, 190, 80), 1.0);
    cairo_set_line_width(cr, 4);
    cairo_stroke(cr);
    // A long highlight.
    cairo_move_to(cr, 70, 96);
    cairo_curve_to(cr, 160, 76, 400, 76, 700, 86);
    setRgbaC(cr, rgb(255, 255, 255), 0.8);
    cairo_set_line_width(cr, 3);
    cairo_stroke(cr);
  });
}

// The Halcyon Arms logo: a gold halo with a dove's wing through it, and the
// name, 820 x 170.
const Texture& halcyonLogo(ClipKit& k)
{
  return cached(k, "e4_halcyon_logo", 820, 170, 410, 85, [](cairo_t* cr) {
    cairo_pattern_t* gold = cairo_pattern_create_linear(0, 20, 0, 150);
    cairo_pattern_add_color_stop_rgb(gold, 0, 1.0, 0.92, 0.6);
    cairo_pattern_add_color_stop_rgb(gold, 0.5, 0.9, 0.74, 0.3);
    cairo_pattern_add_color_stop_rgb(gold, 1, 0.62, 0.44, 0.12);
    cairo_arc(cr, 85, 85, 62, 0, 2 * kPi);
    cairo_set_source(cr, gold);
    cairo_set_line_width(cr, 10);
    cairo_stroke(cr);
    // The wing.
    cairo_move_to(cr, 40, 110);
    cairo_curve_to(cr, 70, 60, 110, 40, 150, 34);
    cairo_curve_to(cr, 128, 52, 132, 58, 140, 60);
    cairo_curve_to(cr, 118, 70, 124, 76, 132, 80);
    cairo_curve_to(cr, 108, 88, 110, 94, 118, 98);
    cairo_curve_to(cr, 92, 108, 70, 110, 40, 110);
    cairo_close_path(cr);
    setRgbaC(cr, rgb(250, 250, 255), 1.0);
    cairo_fill(cr);
    selectGameFont(cr);
    cairo_set_font_size(cr, 72);
    cairo_move_to(cr, 180, 112);
    cairo_text_path(cr, "HALCYON ARMS");
    cairo_set_source(cr, gold);
    cairo_fill(cr);
    cairo_pattern_destroy(gold);
  });
}

// Shot: the sponsor's ad, all gloss. The rifle turning slowly on a black
// turntable under a spotlight, the logo (from frame 10) and the slogan
// (typed out from 14), the small print last.
void halcyonAd(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  (void)frame;
  const float abs = std::min(24.0f, float(ticks) / 6.0f); // 100 ms a frame
  const Texture& bg = cached(k, "e4_halcyon_bg", 1280, 720, 0, 0, [](cairo_t* cr) {
    cairo_pattern_t* g = cairo_pattern_create_radial(640, 260, 40, 640, 300, 900);
    cairo_pattern_add_color_stop_rgb(g, 0, 0.16, 0.2, 0.36);
    cairo_pattern_add_color_stop_rgb(g, 0.5, 0.05, 0.07, 0.16);
    cairo_pattern_add_color_stop_rgb(g, 1, 0.01, 0.01, 0.03);
    cairo_rectangle(cr, 0, 0, 1280, 720);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
    // Gold dust floating out of focus.
    for (int i = 0; i < 40; ++i)
    {
      const unsigned h = hash2(i, 404);
      cairo_arc(cr, h % 1280u, (h / 1280u) % 720u, 4 + h % 14u, 0, 2 * kPi);
      setRgbaC(cr, rgb(230, 190, 80), 0.06 + double(h % 7u) * 0.01);
      cairo_fill(cr);
    }
    // The turntable.
    cairo_save(cr);
    cairo_translate(cr, 640, 400);
    cairo_scale(cr, 1.0, 0.18);
    cairo_arc(cr, 0, 0, 300, 0, 2 * kPi);
    cairo_restore(cr);
    cairo_pattern_t* t = cairo_pattern_create_linear(0, 346, 0, 454);
    cairo_pattern_add_color_stop_rgb(t, 0, 0.22, 0.24, 0.3);
    cairo_pattern_add_color_stop_rgb(t, 1, 0.02, 0.02, 0.04);
    cairo_set_source(cr, t);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(t);
    setRgbaC(cr, rgb(230, 190, 80), 0.9);
    cairo_set_line_width(cr, 3);
    cairo_stroke(cr);
  });
  k.r.draw(bg, ox, oy);
  drawGlow(k.r, k.art, 640.0f + ox, 260.0f + oy, 420.0f, rgb(200, 220, 255), 0.18f);
  // The turn: a squeeze across, flipping to its other side past edge-on.
  const float theta = abs * 0.26f;
  const float c = std::cos(theta);
  const Texture& rifle = halcyonRifle(k, c < 0.0f);
  const float w = 760.0f * std::max(0.05f, std::abs(c)), h = 240.0f;
  const float ry = 300.0f + std::sin(abs * 0.4f) * 6.0f + oy;
  stretchTex(k, rifle, 640.0f - w * 0.5f + ox, ry - h * 0.5f, w, h, rgb(255, 255, 255), 1.0f);
  // The glint running along it when it is side on.
  if (std::abs(c) > 0.85f)
  {
    const float g = std::fmod(abs * 0.9f, 3.0f) / 3.0f;
    drawGlow(k.r, k.art, 640.0f - w * 0.5f + w * g + ox, ry - 30.0f, 60.0f, rgb(255, 255, 255), 0.6f * (std::abs(c) - 0.85f) / 0.15f);
  }
  if (abs >= 10.0f)
  {
    const float u = std::min(1.0f, (abs - 10.0f) / 3.0f);
    DrawOpts lo;
    lo.alpha = u;
    lo.scale = 0.96f + 0.04f * u;
    k.r.draw(halcyonLogo(k), 640.0f + ox, 520.0f + oy, lo);
    // A sheen sweeping across it.
    const float sweep = (abs - 10.0f) / 6.0f;
    if (sweep < 1.0f)
    {
      clipTo(k, 230.0f + ox, 435.0f + oy, 820.0f, 170.0f);
      drawGlow(k.r, k.art, 200.0f + sweep * 900.0f + ox, 520.0f + oy, 90.0f, rgb(255, 250, 220), 0.5f);
      unclip(k);
    }
  }
  if (abs >= 14.0f)
  {
    const float u = std::min(1.0f, (abs - 14.0f) / 6.0f);
    clipTo(k, 330.0f + ox, 590.0f + oy, 620.0f * u, 60.0f);
    k.r.drawText("PEACE THROUGH FIREPOWER", 640 + ox, 600 + oy, {34.0f, rgb(250, 250, 255), rgb(10, 14, 40)}, Align::Center);
    unclip(k);
  }
  if (abs >= 20.0f)
    k.r.drawText("Halcyon Arms. Trusted in forty countries. Peace sold separately.", 640 + ox, 686 + oy,
      {13.0f, rgba(170, 176, 200, int(255 * std::min(1.0f, (abs - 20.0f) / 2.0f))), 0}, Align::Center);
}

// --- Level 29: Dead Air (Episode 5 opens between channels) -------------------------

// The SMPTE bars, as the void remembers them.
constexpr std::array<Color, 7> kVoidBars{rgb(192, 192, 192), rgb(192, 192, 0), rgb(0, 192, 192), rgb(0, 192, 0),
  rgb(192, 0, 192), rgb(192, 0, 0), rgb(0, 0, 192)};

// The dark round the edge of a dead tube.
const Texture& voidTube(ClipKit& k)
{
  return cached(k, "e5_tube", 1280, 720, 0, 0, [](cairo_t* cr) {
    cairo_pattern_t* g = cairo_pattern_create_radial(640, 360, 260, 640, 360, 800);
    cairo_pattern_add_color_stop_rgba(g, 0.0, 0, 0, 0, 0.0);
    cairo_pattern_add_color_stop_rgba(g, 0.6, 0, 0, 0, 0.35);
    cairo_pattern_add_color_stop_rgba(g, 1.0, 0, 0, 0, 0.92);
    cairo_rectangle(cr, 0, 0, 1280, 720);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  });
}

// The void: black, the snow over it, faint bars drifting left (2 px a clip
// frame at 80 ms) with their rows slipping now and then.
void voidBack(ClipKit& k, int frame, int ticks, float snow)
{
  k.r.fillRect(0, 0, W, H, rgb(4, 4, 6));
  DrawOpts o;
  o.scale = 2.0f;
  o.alpha = snow;
  k.r.draw(snowFrame(k, frame % 4), 0, 0, o);
  const float bw = 130.0f, drift = std::fmod(float(ticks) * 2.0f / 4.8f, bw * 7.0f);
  for (float y = 0.0f; y < H; y += 24.0f)
  {
    const std::uint32_t hs = hash2(int(y), ticks / 10);
    const float slip = hs % 9u == 0 ? float(int(hs % 61u) - 30) : 0.0f;
    for (int i = -1; i < 11; ++i)
      k.r.fillRect(float(i) * bw - drift + slip, y, bw + 1.0f, 24.0f, withAlpha(kVoidBars[std::size_t((i + 7) % 7)], 30));
  }
}

// A runner as a dark silhouette rimmed in white, tumbling: rotated `angle`
// degrees round its middle.
void silhouette(ClipKit& k, int who, int pose, float x, float y, float scale, float angle, bool mirror)
{
  const Texture& t = runner(k, who, pose, 3.0f, mirror);
  // The sprite turns round its feet; walk the feet round so it turns round
  // its middle instead.
  const float half = float(t.h()) * 0.45f * scale, a = angle * float(kPi) / 180.0f;
  const float fx = x - std::sin(a) * half, fy = y + std::cos(a) * half;
  DrawOpts rim;
  rim.scale = scale;
  rim.angle = angle;
  rim.tint = rgb(220, 225, 240);
  rim.blend = Blend::Add;
  rim.alpha = 0.35f;
  for (const auto& d : {std::array<float, 2>{-3, 0}, std::array<float, 2>{3, 0}, std::array<float, 2>{0, -3}, std::array<float, 2>{0, 3}})
    k.r.draw(t, fx + d[0], fy + d[1], rim);
  DrawOpts body;
  body.scale = scale;
  body.angle = angle;
  body.tint = rgb(14, 14, 20);
  k.r.draw(t, fx, fy, body);
}

// static_void: the void between channels, the bars drifting through it and
// the three runners tumbling in silhouette (a turn every 12 frames).
void staticVoid(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  voidBack(k, frame, ticks, 0.42f);
  const float turns = float(ticks) / (12.0f * 4.8f);
  struct Fall
  {
    int who;
    float x, y, scale, phase, spin;
  };
  static constexpr std::array<Fall, 3> kFalls{{{0, 360, 330, 0.9f, 0.0f, 1.0f}, {1, 700, 420, 1.15f, 2.1f, -0.8f},
    {2, 990, 260, 0.75f, 4.2f, 1.2f}}};
  for (const auto& f : kFalls)
  {
    const float a = turns * 2.0f * float(kPi) * 0.25f + f.phase;
    const float x = f.x + std::cos(a) * 40.0f + ox, y = f.y + std::sin(a * 1.3f) * 30.0f + oy;
    silhouette(k, f.who, f.who == 1 ? 7 : 2, x, y, f.scale, turns * 360.0f * f.spin + f.phase * 50.0f, f.who == 2);
  }
  // A roll of brighter snow down the picture.
  const float band = std::fmod(float(ticks) * 3.0f, H + 160.0f) - 80.0f;
  k.r.fillRect(0, band, W, 30, rgba(255, 255, 255, 30), Blend::Add);
  scanlines(k, 70);
  k.r.draw(voidTube(k), 0, 0);
}

// The test card behind runners_listen: a grey field with its grid, a great
// circle, a band of bars, a step wedge and the crosshair, all half lost.
const Texture& listenCard(ClipKit& k)
{
  return cached(k, "e5_listen_card", 1280, 720, 0, 0, [](cairo_t* cr) {
    const double cx = 640, cy = 300, R = 280;
    for (int x = 0; x <= 1280; x += 64)
    {
      cairo_rectangle(cr, x - 1, 0, 2, 720);
      setRgbaC(cr, rgb(220, 220, 235), 0.08);
      cairo_fill(cr);
    }
    for (int y = 12; y <= 720; y += 64)
    {
      cairo_rectangle(cr, 0, y - 1, 1280, 2);
      setRgbaC(cr, rgb(220, 220, 235), 0.08);
      cairo_fill(cr);
    }
    cairo_save(cr);
    cairo_arc(cr, cx, cy, R, 0, 2 * kPi);
    cairo_clip(cr);
    cairo_rectangle(cr, cx - R, cy - R, 2 * R, 2 * R);
    setRgbaC(cr, rgb(40, 40, 46), 0.6);
    cairo_fill(cr);
    for (int i = 0; i < 7; ++i)
    {
      cairo_rectangle(cr, cx - R + i * 2 * R / 7.0, cy - 170, 2 * R / 7.0 + 0.5, 110);
      setRgbaC(cr, kVoidBars[std::size_t(i)], 0.5);
      cairo_fill(cr);
    }
    for (int i = 0; i < 6; ++i)
    {
      const int v = 240 - i * 42;
      cairo_rectangle(cr, cx - 200 + i * 400 / 6.0, cy + 80, 400 / 6.0 + 0.5, 50);
      setRgbaC(cr, rgb(v, v, v), 0.45);
      cairo_fill(cr);
    }
    // Resolution gratings either side of the middle.
    for (int i = 0; i < 12; ++i)
    {
      cairo_rectangle(cr, cx - 240 + i * 6, cy - 40, 3, 80);
      cairo_rectangle(cr, cx + 170 + i * 6, cy - 40, 3, 80);
    }
    setRgbaC(cr, rgb(230, 230, 240), 0.35);
    cairo_fill(cr);
    cairo_restore(cr);
    cairo_arc(cr, cx, cy, R, 0, 2 * kPi);
    setRgbaC(cr, rgb(240, 240, 250), 0.55);
    cairo_set_line_width(cr, 5);
    cairo_stroke(cr);
    cairo_arc(cr, cx, cy, 70, 0, 2 * kPi);
    setRgbaC(cr, rgb(240, 240, 250), 0.4);
    cairo_set_line_width(cr, 2);
    cairo_stroke(cr);
    cairo_rectangle(cr, cx - R, cy - 1, 2 * R, 2);
    cairo_rectangle(cr, cx - 1, cy - R, 2, 2 * R);
    setRgbaC(cr, rgb(240, 240, 250), 0.3);
    cairo_fill(cr);
  });
}

// runners_listen: the three standing on a white bar in the static, heads on
// one side, a test card behind them, and over them a trace that steadies
// from noise into a clean sine over the 16 frames.
void runnersListen(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  const float u = std::clamp((float(frame) + float(ticks % 7) / 7.0f) / 15.0f, 0.0f, 1.0f);
  voidBack(k, frame, ticks, 0.3f * (1.0f - 0.5f * u));
  // The card, jittering less as the signal steadies.
  DrawOpts card;
  card.alpha = 0.55f + 0.35f * u;
  const float jit = (1.0f - u) * float(int(hash2(ticks / 2, 7) % 13u) - 6);
  k.r.draw(listenCard(k), jit + ox * 0.5f, oy * 0.5f, card);
  // The white bar they stand on, glowing into the dark.
  const float barY = 500.0f + oy; // clear of the speech box
  drawGlow(k.r, k.art, 640 + ox, barY + 10, 380.0f, rgb(230, 235, 255), 0.18f);
  k.r.fillRect(180 + ox, barY, 920, 40, rgb(244, 244, 248));
  k.r.fillRect(180 + ox, barY, 920, 4, rgb(255, 255, 255));
  k.r.fillRect(180 + ox, barY + 36, 920, 4, rgb(176, 178, 190));
  k.r.fillRect(180 + ox, barY - 3, 920, 2, rgba(255, 70, 80, 90));
  // Dash, Rocco and Nova, heads tilted to the sound.
  static constexpr std::array<std::array<float, 3>, 3> kStand{{{430, -7, 0}, {640, 5, 1}, {850, -6, 2}}};
  for (const auto& s : kStand)
  {
    DrawOpts o;
    o.angle = s[1] + std::sin(float(ticks) * 0.05f + s[2]) * 1.5f;
    k.r.draw(runner(k, int(s[2]), 0, 2.6f, s[2] == 2.0f), s[0] + ox, barY + 2, o);
  }
  // The trace: noise settling into a sine.
  const float y0 = 150.0f + oy, amp = 46.0f;
  float px = 150.0f, py = y0;
  for (float x = 150.0f; x <= 1130.0f; x += 6.0f)
  {
    const float sine = std::sin((x - 150.0f) / 980.0f * 6.0f * float(kPi) - float(ticks) * 0.12f) * amp * u;
    const float noise = (float(int(hash2(int(x), ticks / 2) % 200u)) / 100.0f - 1.0f) * amp * 1.2f * (1.0f - u);
    const float y = y0 + sine + noise;
    if (x > 150.0f)
    {
      k.r.drawLine(px + ox, py, x + ox, y, 10.0f, rgba(120, 255, 170, 40), Blend::Add);
      k.r.drawLine(px + ox, py, x + ox, y, 3.0f, lerpColor(rgb(200, 210, 220), rgb(150, 255, 190), u));
    }
    px = x;
    py = y;
  }
  scanlines(k, 50);
  k.r.draw(voidTube(k), 0, 0);
}

// --- Level 30: Fiber Trench (brief_30) ---------------------------------------

constexpr Color kDiveFiber = rgb(90, 240, 255);
constexpr Color kDiveFiberCore = rgb(215, 255, 255);

// The deep water: a dim blue from above sinking to black, faint shafts of
// light slanting down.
const Texture& diveWater(ClipKit& k)
{
  return cached(k, "dive_water", int(W), int(H), 0, 0, [](cairo_t* cr) {
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 0, 0, H);
    cairo_pattern_add_color_stop_rgb(g, 0.0, 0.04, 0.2, 0.34);
    cairo_pattern_add_color_stop_rgb(g, 0.4, 0.01, 0.08, 0.17);
    cairo_pattern_add_color_stop_rgb(g, 1.0, 0.0, 0.01, 0.05);
    cairo_set_source(cr, g);
    cairo_paint(cr);
    cairo_pattern_destroy(g);
    for (int i = 0; i < 5; ++i)
    {
      const double x = 120.0 + i * 270.0 + (i % 2) * 50.0, top = 46.0 + (i % 3) * 22.0;
      cairo_pattern_t* s = cairo_pattern_create_linear(0, 0, 0, H * 0.85);
      cairo_pattern_add_color_stop_rgba(s, 0.0, 0.55, 0.85, 1.0, 0.11);
      cairo_pattern_add_color_stop_rgba(s, 1.0, 0.55, 0.85, 1.0, 0.0);
      cairo_move_to(cr, x - 30, 0);
      cairo_line_to(cr, x + top, 0);
      cairo_line_to(cr, x + top + 190, H * 0.85);
      cairo_line_to(cr, x + 70, H * 0.85);
      cairo_close_path(cr);
      cairo_set_source(cr, s);
      cairo_fill(cr);
      cairo_pattern_destroy(s);
    }
  });
}

// Motes drifting up through the water (`speed` px a tick; the camera
// sinking makes them rise faster).
void diveMotes(ClipKit& k, int ticks, float speed, int count, float ox, float oy)
{
  for (int i = 0; i < count; ++i)
  {
    const float depth = 0.4f + float(hash2(i, 61) % 60u) / 100.0f;
    const float x = float(hash2(i, 62) % 1320u) - 20.0f + std::sin(float(ticks) * 0.02f + float(i)) * 6.0f;
    const float y = std::fmod(float(hash2(i, 63) % 760u) - float(ticks) * speed * depth + 7600.0f, 760.0f) - 20.0f;
    const float s = 1.5f + 2.0f * depth;
    const int a = int(40.0f + 90.0f * depth);
    k.r.fillRect(x + ox * depth, y + oy * depth, s, s, rgba(170, 225, 255, a), Blend::Add);
  }
}

// A kelp stalk, backlit: a dark wavy stem with blades up both sides and a
// thin rim of light. 120 x 560, anchored at its root.
const Texture& diveKelp(ClipKit& k, int v)
{
  return cached(k, "dive_kelp" + std::to_string(v), 120, 560, 60, 560, [v](cairo_t* cr) {
    const double bend = v == 0 ? 14.0 : -18.0;
    auto stemX = [&](double y) { return 60.0 + std::sin((560.0 - y) / 560.0 * 2.6 + v) * bend; };
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_move_to(cr, stemX(560), 560);
    for (double y = 552; y >= 20; y -= 8)
      cairo_line_to(cr, stemX(y), y);
    cairo_set_source_rgb(cr, 0.03, 0.1, 0.08);
    cairo_set_line_width(cr, 7);
    cairo_stroke_preserve(cr);
    cairo_set_source_rgba(cr, 0.35, 0.95, 0.85, 0.18);
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);
    for (int i = 0; i < 11; ++i)
    {
      const double y = 520.0 - i * 46.0, x = stemX(y), side = (i + v) % 2 ? 1.0 : -1.0;
      const double len = 70.0 - i * 3.0;
      cairo_save(cr);
      cairo_translate(cr, x, y);
      cairo_rotate(cr, side * (0.9 - i * 0.03) - kPi / 2.0);
      cairo_move_to(cr, 0, 0);
      cairo_curve_to(cr, len * 0.3, -11, len * 0.75, -9, len, 0);
      cairo_curve_to(cr, len * 0.75, 8, len * 0.3, 10, 0, 0);
      cairo_restore(cr);
      cairo_set_source_rgb(cr, 0.04, 0.14, 0.1);
      cairo_fill_preserve(cr);
      cairo_set_source_rgba(cr, 0.35, 0.95, 0.85, 0.16);
      cairo_set_line_width(cr, 1.2);
      cairo_stroke(cr);
    }
  });
}

// A glowing fiber cable along `pts`, its light pulse at `pulse` (0..1 along
// it), with a dark jacket and a soft halo.
void diveCable(ClipKit& k, const std::vector<std::array<float, 2>>& pts, float pulse, float glow)
{
  const std::size_t n = pts.size();
  for (std::size_t i = 1; i < n; ++i)
    k.r.drawLine(pts[i - 1][0], pts[i - 1][1], pts[i][0], pts[i][1], 18.0f, rgb(6, 14, 22));
  for (std::size_t i = 1; i < n; ++i)
    k.r.drawLine(pts[i - 1][0], pts[i - 1][1], pts[i][0], pts[i][1], 30.0f, rgba(90, 240, 255, int(26.0f * glow)), Blend::Add);
  const float at = pulse * float(n - 1);
  for (std::size_t i = 1; i < n; ++i)
  {
    const float d = std::fabs(float(i) - at);
    const float hot = d < 14.0f ? 1.0f - d / 14.0f : 0.0f;
    k.r.drawLine(pts[i - 1][0], pts[i - 1][1], pts[i][0], pts[i][1], 4.0f + 3.0f * hot, lerpColor(kDiveFiber, kDiveFiberCore, hot));
  }
  // Glints along it, and the pulse itself.
  for (std::size_t i = 6; i < n; i += 12)
    drawGlow(k.r, k.art, pts[i][0], pts[i][1], 40.0f, kDiveFiber, 0.18f * glow);
  const std::size_t p = std::min(n - 1, std::size_t(std::max(0.0f, at)));
  drawGlow(k.r, k.art, pts[p][0], pts[p][1], 170.0f, kDiveFiber, 0.5f);
  drawGlow(k.r, k.art, pts[p][0], pts[p][1], 60.0f, kDiveFiberCore, 0.7f);
}

// cable_glow: black water with motes rising, the glowing cable running
// down-right across it with a light pulse travelling along it (12-frame
// loop), kelp swaying in front.
void cableGlow(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  (void)frame;
  k.r.fillRect(0, 0, W, H, rgb(0, 2, 6));
  DrawOpts dim;
  dim.alpha = 0.35f;
  k.r.draw(diveWater(k), ox * 0.3f, oy * 0.3f, dim);
  diveMotes(k, ticks, 0.5f, 90, ox, oy);
  std::vector<std::array<float, 2>> pts;
  for (int i = 0; i <= 120; ++i)
  {
    const float u = float(i) / 120.0f;
    pts.push_back({-60.0f + u * 1420.0f + ox, 110.0f + u * 520.0f + std::sin(u * 5.0f) * 26.0f + oy});
  }
  diveCable(k, pts, float(ticks % 72) / 72.0f, 1.0f);
  // Kelp in front, swaying.
  static constexpr std::array<std::array<float, 4>, 5> kKelp{{{70, 1.15f, 0, 0}, {210, 0.9f, 1, 1.7f},
    {980, 1.0f, 1, 0.6f}, {1130, 1.25f, 0, 2.4f}, {1240, 0.8f, 1, 4.0f}}};
  for (const auto& kp : kKelp)
  {
    DrawOpts o;
    o.scale = kp[1];
    o.angle = std::sin(float(ticks) * 0.035f + kp[3]) * 4.0f;
    k.r.draw(diveKelp(k, int(kp[2])), kp[0] + ox * 1.4f, H + 30.0f + oy, o);
  }
}

// A runner in diving gear: the tank on the back, the mask and the
// regulator, fins on the feet, in the sinking pose (facing left if
// `mirror`). Anchored at the feet, like runner().
const Texture& diver(ClipKit& k, int kind, float scale, bool mirror)
{
  const int w = int(128 * scale), h = int(130 * scale);
  return cached(k, "diver" + std::to_string(kind) + "_" + std::to_string(int(scale * 100)) + (mirror ? "m" : ""), w, h,
    float(w) * 0.5f, float(h), [kind, scale, mirror, w](cairo_t* cr) {
      if (mirror)
      {
        cairo_translate(cr, w, 0);
        cairo_scale(cr, -1, 1);
      }
      cairo_scale(cr, scale, scale);
      cairo_translate(cr, 32.0, 30.0);
      cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
      auto outline = [&](double r, double g, double b, double lw) {
        cairo_set_source_rgb(cr, r, g, b);
        cairo_fill_preserve(cr);
        cairo_set_source_rgb(cr, 0.04, 0.03, 0.08);
        cairo_set_line_width(cr, lw);
        cairo_stroke(cr);
      };
      // The tank, behind.
      cairo_new_path(cr);
      cairo_move_to(cr, 6, 26);
      cairo_curve_to(cr, 6, 18, 18, 18, 18, 26);
      cairo_line_to(cr, 18, 56);
      cairo_curve_to(cr, 18, 59, 6, 59, 6, 56);
      cairo_close_path(cr);
      outline(0.96, 0.74, 0.16, 1.6);
      cairo_rectangle(cr, 7.5, 30, 9, 2.5);
      cairo_set_source_rgba(cr, 0.04, 0.03, 0.08, 0.5);
      cairo_fill(cr);
      cairo_rectangle(cr, 10, 15, 4, 5);
      outline(0.6, 0.62, 0.66, 1.2);
      drawCharacterPose(cr, kind, 7);
      // The hose over the shoulder to the regulator.
      cairo_move_to(cr, 12, 16);
      cairo_curve_to(cr, 14, 4, 46, 14, 44, 24);
      cairo_set_source_rgb(cr, 0.04, 0.03, 0.08);
      cairo_set_line_width(cr, 3.2);
      cairo_stroke_preserve(cr);
      cairo_set_source_rgb(cr, 0.2, 0.22, 0.28);
      cairo_set_line_width(cr, 1.6);
      cairo_stroke(cr);
      cairo_arc(cr, 45, 24, 3.2, 0, 2 * kPi);
      outline(0.16, 0.17, 0.2, 1.2);
      // The strap round the head and the mask over the eyes.
      cairo_move_to(cr, 34, 11);
      cairo_curve_to(cr, 28, 6, 21, 10, 21, 16);
      cairo_set_source_rgb(cr, 0.04, 0.03, 0.08);
      cairo_set_line_width(cr, 3.0);
      cairo_stroke(cr);
      cairo_new_path(cr);
      cairo_move_to(cr, 35, 9);
      cairo_line_to(cr, 49, 8);
      cairo_curve_to(cr, 52, 8, 52, 20, 49, 20);
      cairo_line_to(cr, 35, 20);
      cairo_curve_to(cr, 32, 20, 32, 9, 35, 9);
      cairo_close_path(cr);
      outline(0.1, 0.12, 0.16, 1.4);
      cairo_new_path(cr);
      cairo_move_to(cr, 36.5, 11);
      cairo_line_to(cr, 48.5, 10.5);
      cairo_curve_to(cr, 50, 10.5, 50, 17.5, 48.5, 17.5);
      cairo_line_to(cr, 36.5, 17.5);
      cairo_curve_to(cr, 35, 17.5, 35, 11, 36.5, 11);
      cairo_close_path(cr);
      cairo_pattern_t* glass = cairo_pattern_create_linear(36, 10, 48, 18);
      cairo_pattern_add_color_stop_rgba(glass, 0.0, 0.7, 0.95, 1.0, 0.75);
      cairo_pattern_add_color_stop_rgba(glass, 1.0, 0.25, 0.6, 0.8, 0.55);
      cairo_set_source(cr, glass);
      cairo_fill(cr);
      cairo_pattern_destroy(glass);
      cairo_move_to(cr, 38, 12.5);
      cairo_line_to(cr, 42, 12.3);
      cairo_set_source_rgba(cr, 1, 1, 1, 0.85);
      cairo_set_line_width(cr, 1.2);
      cairo_stroke(cr);
      // Fins on the feet.
      for (const auto& f : {std::array<double, 3>{6, 84, 2.5}, std::array<double, 3>{50, 89, 0.5}})
      {
        cairo_save(cr);
        cairo_translate(cr, f[0], f[1]);
        cairo_rotate(cr, f[2]);
        cairo_new_path(cr);
        cairo_move_to(cr, -3, -4);
        cairo_line_to(cr, 16, -8);
        cairo_curve_to(cr, 19, -3, 19, 3, 16, 8);
        cairo_line_to(cr, -3, 4);
        cairo_close_path(cr);
        cairo_restore(cr);
        outline(0.12, 0.55, 0.62, 1.4);
      }
    });
}

// A bubble: a thin bright ring with a highlight. 24 x 24, anchored at its
// centre.
const Texture& diveBubble(ClipKit& k)
{
  return cached(k, "dive_bubble", 24, 24, 12, 12, [](cairo_t* cr) {
    cairo_arc(cr, 12, 12, 9.5, 0, 2 * kPi);
    cairo_set_source_rgba(cr, 0.7, 0.92, 1.0, 0.18);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 0.85, 0.97, 1.0, 0.85);
    cairo_set_line_width(cr, 1.6);
    cairo_stroke(cr);
    cairo_arc(cr, 8.5, 8.5, 2.6, 0, 2 * kPi);
    cairo_set_source_rgba(cr, 1, 1, 1, 0.9);
    cairo_fill(cr);
  });
}

// runners_dive: the three runners in diving gear sinking past the camera,
// bubbles streaming up from their masks, the glowing cable coming up from
// below.
void runnersDive(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  (void)frame;
  const float sink = float(ticks); // the camera sinks with them
  k.r.draw(diveWater(k), ox * 0.3f, oy * 0.3f - std::min(60.0f, sink * 0.25f));
  diveMotes(k, ticks, 1.1f, 80, ox, oy);
  // The cable on the floor below, rising into view as they go down.
  const float floorY = H - std::min(230.0f, sink * 1.6f);
  k.r.fillRect(0, floorY + 24.0f + oy, W, H, rgb(1, 4, 10));
  std::vector<std::array<float, 2>> pts;
  for (int i = 0; i <= 100; ++i)
  {
    const float u = float(i) / 100.0f;
    pts.push_back({-40.0f + u * 1360.0f + ox, floorY + std::sin(u * 7.0f + 0.5f) * 14.0f - u * 40.0f + oy});
  }
  diveCable(k, pts, float(ticks % 72) / 72.0f, 0.9f);
  // Dash, Rocco and Nova at three depths: far and dim, then nearer and
  // bigger, sinking at different rates and turning a little.
  struct Diver
  {
    int who;
    float x, y0, rate, scale, phase;
    bool mirror;
    Color tint;
  };
  static const std::array<Diver, 3> kDivers{{{1, 930, 230, 0.5f, 1.7f, 1.3f, true, rgb(120, 160, 200)},
    {2, 360, 260, 0.8f, 2.2f, 0.4f, false, rgb(190, 215, 235)}, {0, 700, 310, 1.3f, 2.9f, 2.1f, false, rgb(255, 255, 255)}}};
  for (const auto& d : kDivers)
  {
    const float y = d.y0 + sink * d.rate + std::sin(float(ticks) * 0.04f + d.phase) * 6.0f;
    const float x = d.x + std::sin(float(ticks) * 0.025f + d.phase) * 14.0f + ox * d.scale * 0.4f;
    const float headY = y - 86.0f * d.scale;
    // Bubbles from the regulator, streaming up and wobbling.
    const float headX = x + (d.mirror ? -1.0f : 1.0f) * 13.0f * d.scale;
    for (int i = 0; i < 14; ++i)
    {
      const float ph = std::fmod(float(ticks) * 0.018f + float(i) / 14.0f + d.phase, 1.0f);
      DrawOpts b;
      b.scale = (0.25f + 0.35f * float(hash2(i, int(d.who)) % 100u) / 100.0f) * d.scale * (0.7f + 0.6f * ph);
      b.alpha = (1.0f - ph) * 0.9f;
      k.r.draw(diveBubble(k), headX + std::sin(ph * 9.0f + float(i)) * 10.0f * ph * d.scale,
        headY - ph * 520.0f + oy, b);
    }
    DrawOpts o;
    o.tint = d.tint;
    o.angle = std::sin(float(ticks) * 0.03f + d.phase) * 6.0f;
    k.r.draw(diver(k, d.who, d.scale, d.mirror), x, y + oy, o);
  }
  // A few big soft motes right by the lens.
  for (int i = 0; i < 6; ++i)
  {
    const float x = float(hash2(i, 71) % 1280u);
    const float y = std::fmod(float(hash2(i, 72) % 900u) - float(ticks) * 2.2f + 9000.0f, 900.0f) - 90.0f;
    drawGlow(k.r, k.art, x + ox * 2.0f, y + oy * 2.0f, 26.0f, rgb(170, 220, 255), 0.18f);
  }
}

bool starts(const std::string& s, const char* p) { return s.rfind(p, 0) == 0; }

} // namespace

void drawClip(ClipKit& k, const std::string& clip, int frame, int frames, float t, int ticks, float ox, float oy)
{
  (void)frames;
  if (clip == "neon_city")
    return neonCity(k, ticks, ox, oy, t);
  if (clip == "diner")
    return diner(k, frame, ticks, ox, oy);
  if (clip == "max_hologram")
  {
    diner(k, 0, ticks, ox, oy);
    k.r.fillRect(0, 0, W, H, rgba(0, 0, 0, 90));
    const float rise = std::min(1.0f, float(frame) / 6.0f);
    return drawMax(k, 640 + ox, 300 + oy + (1.0f - rise) * 220.0f, 0.9f, ticks, frame < 6 ? 1.0f - rise : 0.0f);
  }
  if (clip == "name_dash")
    return nameCard(k, 0, frame, ox, oy);
  if (clip == "name_rocco")
    return nameCard(k, 1, frame, ox, oy);
  if (clip == "name_nova")
    return nameCard(k, 2, frame, ox, oy);
  if (clip == "logo")
    return logo(k, frame, ticks, ox, oy);
  if (clip == "break_card")
    return tvCard(k, "WE'LL BE RIGHT BACK!", ticks);
  if (clip == "back_card")
    return tvCard(k, "...AND WE'RE BACK!", ticks);
  if (starts(clip, "bumper:"))
    return bumper(k, clip, frame, ticks);
  if (starts(clip, "credits:"))
    return credits(k, ticks);
  if (clip == "static_snow")
    return staticSnow(k, frame, ticks, ox, oy);
  if (clip == "ident_ch7")
    return identCh7(k, frame, frames, ticks, ox, oy);
  if (clip == "lance_cowboy")
    return lanceCowboyClip(k, frame, frames, t, ticks, ox, oy);
  if (clip == "ident_ch13")
    return identCh13(k, frame, ticks, ox, oy);
  if (clip == "lance_cape")
    return lanceCapeClip(k, frame, t, ticks, ox, oy);
  if (clip == "ident_ch24")
    return identCh24(k, frame, ticks, ox, oy);
  if (clip == "lance_chef")
    return lanceChefClip(k, frame, t, ticks, ox, oy);
  if (clip == "ident_ch31")
    return identCh31(k, ticks, ox, oy);
  if (clip == "runners_habitat") // before starts(clip, "runners") below
    return runnersHabitat(k, frame, ticks, ox, oy);
  if (clip == "ident_ch50")
    return identCh50(k, ticks, ox, oy);
  if (clip == "lizard_wave")
    return lizardWave(k, frame, ticks, ox, oy);
  // Level 28 and the end of Episode 4.
  if (clip == "ident_ch1")
    return identCh1(k, ticks, ox, oy);
  if (clip == "lance_toon")
    return lanceToon(k, frame, ticks, ox, oy);
  if (clip == "nova_whisper") // before starts(clip, "nova_") below
    return novaWhisper(k, frame, ticks, ox, oy);
  if (clip == "buddy_deflate")
    return buddyDeflate(k, frame, ticks, ox, oy);
  if (clip == "iris")
    return irisClip(k, frame, ticks, ox, oy);
  if (clip == "jump_static")
    return jumpStatic(k, frame, ticks, ox, oy);
  if (clip == "lance_panic")
    return lancePanic(k, frame, ticks, ox, oy);
  if (clip == "halcyon_ad")
    return halcyonAd(k, frame, ticks, ox, oy);
  // Level 29.
  if (clip == "static_void") // before starts(clip, "static") below
    return staticVoid(k, frame, ticks, ox, oy);
  if (clip == "runners_listen") // before starts(clip, "runners") below
    return runnersListen(k, frame, ticks, ox, oy);
  // Level 30.
  if (clip == "cable_glow")
    return cableGlow(k, frame, ticks, ox, oy);
  if (clip == "runners_dive") // before starts(clip, "runners") below
    return runnersDive(k, frame, ticks, ox, oy);
  if (starts(clip, "static") || clip == "monitors_snow")
    return staticNoise(k, ticks / 2);
  if (clip == "tower_tilt")
    return towerTilt(k, frame, ticks, ox, oy);
  if (clip == "crew_outfits")
    return crewOutfits(k, frame, ticks, ox, oy);
  if (clip == "blackout_table")
    return blackoutTable(k, frame, ticks, ox, oy);
  if (clip == "grate_glow")
    return grateGlow(k, frame, ticks, ox, oy);
  if (clip == "brief08_ship")
    return canopyShip(k, frame, ticks, ox, oy);
  if (clip == "brief08_jump")
    return canopyJump(k, frame, ticks, ox, oy);
  if (clip == "brief09_door")
    return templeDoor(k, frame, ticks, ox, oy);
  if (clip == "brief09_wait")
    return templeWait(k, frame, ticks, ox, oy);
  if (clip == "brief10_beam")
    return sunBeam(k, frame, ticks, ox, oy);
  if (clip == "brief10_up")
    return sunUp(k, frame, ticks, ox, oy);
  if (clip == "brief11_tunnel")
    return mineTunnel(k, frame, ticks, ox, oy);
  if (clip == "brief11_hop")
    return mineHop(k, frame, ticks, ox, oy);
  if (clip == "brief12_heat")
    return magmaHeat(k, frame, ticks, ox, oy);
  if (clip == "brief12_hop")
    return magmaHop(k, frame, ticks, ox, oy);
  if (clip == "brief44_crash")
    return crashLanding(k, frame, ticks, ox, oy);
  if (clip == "brief44_goo")
    return gooWall(k, frame, ticks, ox, oy);
  if (clip == "brief43_cockpit")
    return starfallCockpit(k, frame, ticks, ox, oy);
  if (clip == "brief43_rocks")
    return starfallRocks(k, frame, ticks, ox, oy);
  if (clip == "brief46_islands")
    return crystalBrief(k, frame, ticks, ox, oy);
  if (clip == "brief46_swap")
    return crystalSwap(k, frame, ticks, ox, oy);
  if (clip == "brief47_canyon")
    return silkBrief(k, frame, ticks, ox, oy);
  if (clip == "brief47_ride")
    return silkRide(k, frame, ticks, ox, oy);
  if (clip == "brief48_plains")
    return plainsBrief(k, frame, ticks, ox, oy);
  if (clip == "brief48_ride")
    return plainsRide(k, frame, ticks, ox, oy);
  if (clip == "brief49_chamber")
    return motherBrief(k, frame, ticks, ox, oy);
  if (clip == "brief49_wake")
    return motherWake(k, frame, ticks, ox, oy);
  if (clip == "e7_mother_falls")
    return motherFalls(k, frame, ticks, ox, oy);
  if (clip == "e7_star_seed")
    return starSeedHeld(k, frame, ticks, ox, oy);
  if (clip == "e7_takeoff")
    return vurrTakeoff(k, frame, ticks, ox, oy);
  if (clip == "brief45_hive")
    return hiveBrief(k, frame, ticks, ox, oy);
  if (clip == "brief45_gulp")
    return hiveGulp(k, frame, ticks, ox, oy);
  if (clip == "brief13_rumble")
    return templeRumble(k, frame, ticks, ox, oy);
  if (clip == "brief13_go")
    return templeGo(k, frame, ticks, ox, oy);
  if (clip == "brief15_dock")
    return stationDock(k, frame, ticks, ox, oy);
  if (clip == "brief15_intercom")
    return stationIntercom(k, frame, ticks, ox, oy);
  if (clip == "brief16_frost")
    return cryoFrost(k, frame, ticks, ox, oy);
  if (clip == "brief16_shrug")
    return cryoShrug(k, frame, ticks, ox, oy);
  if (clip == "brief17_lights")
    return greenLights(k, frame, ticks, ox, oy);
  if (clip == "brief17_vine")
    return greenVine(k, frame, ticks, ox, oy);
  if (clip == "brief18_airlock")
    return hullAirlock(k, frame, ticks, ox, oy);
  if (clip == "brief19_lab")
    return gravLab(k, frame, ticks, ox, oy);
  if (clip == "brief19_arrows")
    return gravArrows(k, frame, ticks, ox, oy);
  if (clip == "brief19_flip")
    return gravFlip(k, frame, ticks, ox, oy);
  if (clip == "brief20_core")
    return reactorCore(k, frame, ticks, ox, oy);
  if (clip == "brief20_crew")
    return reactorCrew(k, frame, ticks, ox, oy);
  if (clip == "brief14_door")
    return goldDoorScratch(k, frame, ticks, ox, oy);
  if (clip == "brief14_pull")
    return goldDoorPull(k, frame, ticks, ox, oy);
  // Level 21 and the end of Episode 3.
  if (clip == "brief21_eye")
    return brief21Eye(k, frame, ticks, ox, oy);
  if (clip == "zero_fade")
    return zeroFade(k, frame, ticks, ox, oy);
  if (clip == "wall_falls")
    return wallFalls(k, frame, ticks, ox, oy);
  if (clip == "studio_lights")
    return studioLights(k, frame, ticks, ox, oy);
  if (clip == "audience")
    return audience(k, frame, ticks, ox, oy);
  if (clip == "lance_entrance")
    return lanceEntrance(k, frame, ticks, ox, oy);
  if (clip == "monitors")
    return monitorWall(k, frame, ticks, ox, oy);
  if (clip == "rocco_lens")
    return roccoLens(k, frame, ticks, ox, oy);
  if (clip == "lance_close")
    return lanceClose(k, frame, ticks, ox, oy);
  if (clip == "dash_freeze")
    return dashFreeze(k, frame, ticks, ox, oy);
  if (clip == "trapdoor")
    return trapdoorClip(k, frame, ticks, ox, oy);
  if (starts(clip, "max_holo") || starts(clip, "brief"))
    return briefing(k, clip, frame, ticks, ox, oy, t);
  if (clip == "bridge_wait")
    return bridgeWait(k, frame, ticks, ox, oy);
  if (clip == "maglev_pass")
    return maglevPass(k, frame, ticks, ox, oy);
  if (clip == "the_jump")
    return theJump(k, frame, ticks, ox, oy);
  if (clip == "searchlight_sweep")
    return searchlightSweep(k, frame, ticks, ox, oy);
  if (clip == "rocco_squint")
    return roccoSquint(k, frame, ticks, ox, oy);
  if (clip == "gold_pile")
    return goldPile(k, frame, ticks, ox, oy);
  if (clip == "fiberglass")
    return fiberglass(k, frame, ticks, ox, oy);
  if (clip == "boulder_cliff")
    return boulderCliff(k, frame, ticks, ox, oy);
  if (clip == "wreck")
    return wreck(k, frame, ticks, ox, oy);
  if (clip == "case")
    return caseClip(k, frame, ticks, ox, oy);
  if (clip == "lens")
    return lensClip(k, frame, ticks, ox, oy);
  if (starts(clip, "runners"))
    return runners(k, ticks, ox, oy);
  if (starts(clip, "dash_"))
    return closeUp(k, 0, ticks, ox, oy);
  if (starts(clip, "rocco_"))
    return closeUp(k, 1, ticks, ox, oy);
  if (starts(clip, "nova_"))
    return closeUp(k, 2, ticks, ox, oy);
  return fallback(k, clip, ticks, ox, oy);
}

} // namespace gr
