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
  if (clip == "brief13_rumble")
    return templeRumble(k, frame, ticks, ox, oy);
  if (clip == "brief13_go")
    return templeGo(k, frame, ticks, ox, oy);
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
