// The cutscene clips: scenes drawn with the game's vector art. A clip is a
// function of its frame (and the shot's progress); shared pieces (skylines,
// the MAX hologram, the runners, TV static, cards) are baked with Cairo the
// first time they are used and cached in the ClipKit.

#include "assets/art.hpp"
#include "base/math.hpp"
#include "data/theme.hpp"
#include "frontend/cutscene.hpp"
#include "render/vector.hpp"

#include <algorithm>
#include <cmath>
#include <functional>

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
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
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

// The lens: Rocco turns a big camera lens over in his hands (frames 0-4),
// then the lens's own view of him, a camera's viewfinder with REC
// blinking (frames 5-9). Someone has been filming the crew.
void lensClip(ClipKit& k, int frame, int ticks, float ox, float oy)
{
  const Texture& glass = cached(k, "e1_lens", 260, 260, 130, 130, [](cairo_t* cr) {
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
  });
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

void fallback(ClipKit& k, const std::string& clip, int ticks, float ox, float oy)
{
  drawBackdrop(k.r, k.art, float(ticks) * 1.5f + ox, oy, 0.0f);
  k.r.fillRect(0, 0, W, H, rgba(0, 0, 0, 90));
  (void)clip;
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
  if (starts(clip, "static") || clip == "monitors_snow" || clip == "jump_static")
    return staticNoise(k, ticks / 2);
  if (clip == "tower_tilt")
    return towerTilt(k, frame, ticks, ox, oy);
  if (clip == "crew_outfits")
    return crewOutfits(k, frame, ticks, ox, oy);
  if (clip == "blackout_table")
    return blackoutTable(k, frame, ticks, ox, oy);
  if (clip == "grate_glow")
    return grateGlow(k, frame, ticks, ox, oy);
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
