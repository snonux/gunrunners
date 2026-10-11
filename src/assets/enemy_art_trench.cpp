// Fiber Trench (Level 30) in the style of enemy_art.cpp, but soft and wet:
// deep-sea things with glassy edges and a cold rim of light so they read on
// dark water. The Anglerfish is a fat, mottled ambusher with an underbite
// and a rod over its head (its lure is lit live, so it shows in the dark);
// the Box Jelly a glassy cube of a bell with four bundles of tentacles,
// violet or, carrying the Virus, a sick green; the Cable Eel is black as
// cable jacket with a glowing line down its side, living in a steel junction
// box. The rubber duck wears a diving mask. Each is drawn in screen pixels
// into a texture with a 32 px margin, the cell box at (32, 32) ..
// (32 + w, 32 + h), facing right.

#include "assets/enemy_art_trench.hpp"

#include "base/math.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>

namespace gr
{

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr double kM = 32.0;
constexpr Color kInk = rgb(8, 12, 22);
constexpr Color kRim = rgb(120, 220, 240);

Color lighten(Color c, float t) { return lerpColor(c, rgb(255, 255, 255), t); }
Color darken(Color c, float t) { return lerpColor(c, rgb(0, 0, 0), t); }

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

// --- Anglerfish -------------------------------------------------------------------------------

// The body's outline: tail base on the left, the hump of the back, the
// big head on the right with the mouth line, the jutting lower jaw, the
// belly. `open` 0..1 drops the jaw and lifts the snout.
void anglerBody(cairo_t* cr, double open)
{
  const double lift = open * 18.0, drop = open * 22.0;
  cairo_move_to(cr, 30, 66);
  cairo_curve_to(cr, 40, 30, 70, 14, 112, 12);
  cairo_curve_to(cr, 150, 10, 172, 24 - lift * 0.5, 184, 46 - lift);
  cairo_curve_to(cr, 188, 54 - lift, 186, 60 - lift, 180, 62 - lift);
  cairo_line_to(cr, 150, 70 - lift * 0.3);                    // the mouth's corner
  cairo_line_to(cr, 190, 80 + drop);                          // the lower jaw's tip
  cairo_curve_to(cr, 188, 98 + drop, 172, 112 + drop * 0.6, 140, 120);
  cairo_curve_to(cr, 104, 128, 62, 116, 44, 96);
  cairo_curve_to(cr, 36, 88, 31, 78, 30, 66);
  cairo_close_path(cr);
}

void anglerfish(cairo_t* cr, double w, double h, int variant, int frame)
{
  (void)w;
  (void)h;
  const bool old = variant == 3, tell = variant == 1, lunge = variant == 2;
  const double open = lunge ? 1.0 : (tell ? 0.3 : (old ? 0.08 : 0.0));
  const double sway = frame % 2 ? 5.0 : -5.0;
  const Color skinTop = old ? rgb(104, 112, 104) : rgb(62, 56, 78);
  const Color skinBot = old ? rgb(54, 60, 58) : rgb(24, 22, 34);
  const Color belly = old ? rgb(140, 140, 126) : rgb(86, 74, 96);
  cairo_save(cr);
  cairo_translate(cr, kM, kM);
  // The tail: a ragged fan, swaying.
  cairo_move_to(cr, 36, 66);
  cairo_curve_to(cr, 20, 58, 10, 40 + sway, 2, 30 + sway);
  cairo_curve_to(cr, 8, 50 + sway, 6, 62 + sway * 0.5, 10, 66 + sway * 0.4);
  cairo_curve_to(cr, 4, 76 + sway * 0.5, 4, 92 + sway, 0, 104 + sway);
  cairo_curve_to(cr, 12, 96 + sway, 24, 80, 36, 72);
  cairo_close_path(cr);
  vgrad(cr, 30, 104, {{0.0, lighten(skinTop, 0.1f)}, {1.0, skinBot}}, 1.0, true);
  stroke(cr, kInk, 2.2);
  for (int k = 0; k < 4; ++k)
  {
    cairo_move_to(cr, 30, 68);
    cairo_line_to(cr, 6, 36 + k * 22 + sway * (1.0 - k / 3.0));
    stroke(cr, lighten(skinTop, 0.25f), 1.0, 0.45);
  }
  // Dorsal spines.
  for (int k = 0; k < 4; ++k)
  {
    const double x = 70 + k * 12;
    cairo_move_to(cr, x - 4, 20 - k * 1.5);
    cairo_line_to(cr, x + 2 + sway * 0.2, 2 + k * 1.0);
    cairo_line_to(cr, x + 6, 17 - k * 1.5);
    cairo_close_path(cr);
    setRgba(cr, darken(skinTop, 0.15f), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.4);
  }
  // The mouth's inside, behind the body.
  if (open > 0.0)
  {
    cairo_move_to(cr, 148, 70);
    cairo_line_to(cr, 182, 60 - open * 18);
    cairo_line_to(cr, 192, 78 + open * 22);
    cairo_close_path(cr);
    setRgba(cr, rgb(70, 10, 24), 1.0);
    cairo_fill(cr);
    glow(cr, 160, 72, 18 + open * 10, rgb(140, 20, 50), 0.6);
  }
  // The body.
  anglerBody(cr, open);
  {
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 10, 0, 126);
    stop(g, 0.0, skinTop);
    stop(g, 0.62, skinBot);
    stop(g, 1.0, belly);
    cairo_set_source(cr, g);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(g);
  }
  // Mottling inside the outline.
  cairo_save(cr);
  cairo_clip_preserve(cr);
  Rng rng(old ? 30111u : 30101u);
  for (int i = 0; i < 26; ++i)
  {
    ellipse(cr, rng.range(40, 180), rng.range(16, 110), rng.range(3, 8), rng.range(2, 5), rng.range(0, 3));
    setRgba(cr, i % 3 ? darken(skinTop, 0.35f) : lighten(skinTop, 0.2f), 0.35);
    cairo_fill(cr);
  }
  // A cold rim of light along the back, so it reads on dark water.
  cairo_new_path(cr);
  cairo_move_to(cr, 34, 60);
  cairo_curve_to(cr, 44, 30, 72, 16, 112, 14);
  cairo_curve_to(cr, 148, 12, 170, 26, 182, 46 - open * 18);
  stroke(cr, old ? rgb(200, 220, 200) : kRim, 3.0, 0.45);
  cairo_restore(cr);
  anglerBody(cr, open);
  stroke(cr, kInk, 2.6);
  // The mouth line and the teeth.
  cairo_move_to(cr, 150, 70 - open * 5);
  cairo_line_to(cr, 182, 62 - open * 18);
  stroke(cr, kInk, 2.0);
  const int teeth = old ? 3 : 6;
  for (int k = 0; k < teeth; ++k)
  {
    const double u = (k + 0.5) / teeth;
    // Upper: hanging down from the lip.
    const double ux = 152 + u * 30, uy = 70 - open * 5 - u * (8 + open * 13);
    cairo_move_to(cr, ux - 2.2, uy);
    cairo_line_to(cr, ux + 0.5, uy + 9 + (k % 2) * 4);
    cairo_line_to(cr, ux + 2.4, uy);
    cairo_close_path(cr);
    // Lower: standing up from the jaw.
    const double lx = 156 + u * 32, ly = 74 + u * (6 + open * 22);
    cairo_move_to(cr, lx - 2.4, ly);
    cairo_line_to(cr, lx + 1.0, ly - 11 - (k % 2) * 5);
    cairo_line_to(cr, lx + 2.6, ly);
    cairo_close_path(cr);
  }
  setRgba(cr, rgb(236, 236, 220), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, rgb(60, 50, 60), 0.8);
  // The pectoral fin.
  cairo_move_to(cr, 96, 82);
  cairo_curve_to(cr, 84, 92, 76, 108 + sway, 70, 114 + sway);
  cairo_curve_to(cr, 88, 112 + sway, 100, 104 + sway * 0.5, 108, 90);
  cairo_close_path(cr);
  vgrad(cr, 82, 114, {{0.0, lighten(skinTop, 0.15f)}, {1.0, darken(skinBot, 0.1f)}}, 0.95, true);
  stroke(cr, kInk, 1.8);
  // The eye.
  if (old)
  {
    cairo_move_to(cr, 143, 40);
    cairo_curve_to(cr, 147, 44, 153, 44, 157, 40);
    stroke(cr, kInk, 2.2);
    // Whiskers (barbels) drooping from the chin.
    for (int k = 0; k < 3; ++k)
    {
      cairo_move_to(cr, 168 + k * 6, 92 + k * 2);
      cairo_curve_to(cr, 170 + k * 6, 104, 164 + k * 8, 112, 172 + k * 7, 122 + sway * 0.5);
      stroke(cr, rgb(170, 176, 160), 1.6, 0.9);
    }
    // Barnacles on its back.
    for (const auto& b : {std::array<double, 3>{96, 20, 4}, std::array<double, 3>{106, 17, 3}, std::array<double, 3>{120, 18, 3.5}})
    {
      circle(cr, b[0], b[1], b[2]);
      setRgba(cr, rgb(220, 222, 206), 1.0);
      cairo_fill_preserve(cr);
      stroke(cr, kInk, 1.0);
      circle(cr, b[0], b[1], b[2] * 0.4);
      setRgba(cr, rgb(40, 40, 44), 1.0);
      cairo_fill(cr);
    }
  }
  else
  {
    circle(cr, 150, 40, 7.5);
    setRgba(cr, tell || lunge ? rgb(255, 236, 150) : rgb(150, 140, 110), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.8);
    circle(cr, 151, 40, 3.6);
    setRgba(cr, rgb(10, 8, 14), 1.0);
    cairo_fill(cr);
    if (tell || lunge)
    {
      glow(cr, 150, 40, 18, rgb(255, 240, 160), 0.6);
      circle(cr, 148.5, 38, 1.6);
      setRgba(cr, rgb(255, 255, 255), 1.0);
      cairo_fill(cr);
    }
  }
  // The rod: from the forehead up and over to where the lure hangs.
  cairo_move_to(cr, 140, 16);
  cairo_curve_to(cr, 150, -18, 192, -22, 204, -10);
  stroke(cr, kInk, 4.2);
  cairo_move_to(cr, 140, 16);
  cairo_curve_to(cr, 150, -18, 192, -22, 204, -10);
  stroke(cr, old ? rgb(150, 150, 136) : rgb(110, 100, 120), 2.0);
  circle(cr, 204, -10, 4.0);
  setRgba(cr, rgb(30, 50, 60), 1.0);
  cairo_fill(cr);
  cairo_restore(cr);
}

// --- Box Jelly -------------------------------------------------------------------------------

void boxJelly(cairo_t* cr, double w, double h, bool carrier, int frame)
{
  const double cx = kM + w * 0.5;
  const double squeeze = frame == 2 ? 1.0 : (frame == 1 ? 0.5 : 0.0);
  const double bw = w * (0.74 - 0.2 * squeeze), bh = h * (0.42 + 0.07 * squeeze), top = kM + h * (0.04 + 0.02 * squeeze);
  const double bottom = top + bh;
  const Color body = carrier ? rgb(120, 255, 130) : rgb(196, 160, 255);
  const Color deep = carrier ? rgb(40, 150, 70) : rgb(110, 80, 200);
  const double swing = frame == 3 ? -1.0 : 1.0;
  // Tentacles first, behind the bell: four bundles from the corners.
  const std::array<double, 4> roots{-0.48, -0.17, 0.17, 0.48};
  for (std::size_t b = 0; b < roots.size(); ++b)
  {
    const double rx = cx + roots[b] * bw, back = (b == 1 || b == 2) ? 0.55 : 1.0;
    for (int k = 0; k < 3; ++k)
    {
      const double len = h * (0.5 + 0.06 * k) - squeeze * h * 0.08;
      const double wav = (6.0 + k * 2.0) * swing * (b % 2 ? 1.0 : -1.0);
      cairo_move_to(cr, rx + (k - 1) * 2.5, bottom + 2);
      cairo_curve_to(cr, rx + wav, bottom + len * 0.35, rx - wav, bottom + len * 0.7, rx + wav * 0.6 + (k - 1) * 3,
        bottom + len);
      stroke(cr, lighten(body, 0.3f), 1.8 - k * 0.3, 0.55 * back);
    }
  }
  // The bell: a rounded box, narrowing to the rim.
  auto bell = [&] {
    const double l = cx - bw * 0.5, r = cx + bw * 0.5, inset = bw * 0.08;
    cairo_move_to(cr, l + inset, bottom);
    cairo_curve_to(cr, l - 2, bottom - bh * 0.3, l - 2, top + bh * 0.25, l + bw * 0.12, top + 2);
    cairo_curve_to(cr, l + bw * 0.3, top - 3, r - bw * 0.3, top - 3, r - bw * 0.12, top + 2);
    cairo_curve_to(cr, r + 2, top + bh * 0.25, r + 2, bottom - bh * 0.3, r - inset, bottom);
    cairo_curve_to(cr, cx + bw * 0.2, bottom + 4, cx - bw * 0.2, bottom + 4, l + inset, bottom);
    cairo_close_path(cr);
  };
  glow(cr, cx, top + bh * 0.5, bw * 0.75, body, 0.25);
  bell();
  {
    cairo_pattern_t* g = cairo_pattern_create_radial(cx - bw * 0.15, top + bh * 0.3, 2, cx, top + bh * 0.5, bw * 0.7);
    stop(g, 0.0, lighten(body, 0.5f), 0.65);
    stop(g, 0.6, body, 0.42);
    stop(g, 1.0, deep, 0.5);
    cairo_set_source(cr, g);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(g);
  }
  stroke(cr, lighten(body, 0.55f), 2.2, 0.85);
  // The box's edges, seen through the glass.
  for (const double s : {-0.22, 0.22})
  {
    cairo_move_to(cr, cx + s * bw, top + 4);
    cairo_curve_to(cr, cx + s * bw * 1.15, top + bh * 0.4, cx + s * bw * 1.15, top + bh * 0.7, cx + s * bw * 1.1, bottom - 2);
    stroke(cr, lighten(body, 0.5f), 1.2, 0.4);
  }
  // The gonads: four leaves hanging inside.
  for (int k = 0; k < 4; ++k)
  {
    const double gx = cx + (k - 1.5) * bw * 0.18;
    ellipse(cr, gx, top + bh * 0.55, bw * 0.06, bh * 0.26, (k - 1.5) * 0.12);
    setRgba(cr, carrier ? rgb(200, 255, 120) : rgb(255, 170, 230), 0.55);
    cairo_fill(cr);
  }
  if (carrier)
  {
    // The Virus inside: spiky green motes.
    for (const auto& v : {std::array<double, 2>{-0.18, 0.4}, std::array<double, 2>{0.15, 0.62}, std::array<double, 2>{0.02, 0.28}})
    {
      const double vx = cx + v[0] * bw, vy = top + v[1] * bh;
      for (int s = 0; s < 6; ++s)
      {
        const double a = s * kPi / 3;
        cairo_move_to(cr, vx, vy);
        cairo_line_to(cr, vx + std::cos(a) * 5, vy + std::sin(a) * 5);
      }
      stroke(cr, rgb(60, 140, 30), 1.4, 0.9);
      circle(cr, vx, vy, 2.6);
      setRgba(cr, rgb(170, 255, 90), 1.0);
      cairo_fill(cr);
    }
  }
  // The pedalia: little arms at the rim's corners.
  for (const double s : {-0.46, 0.46})
  {
    ellipse(cr, cx + s * bw, bottom + 1, 5, 3.5);
    setRgba(cr, lighten(body, 0.4f), 0.85);
    cairo_fill(cr);
  }
  // A highlight on the glass.
  ellipse(cr, cx - bw * 0.24, top + bh * 0.2, bw * 0.1, bh * 0.08, -0.5);
  setRgba(cr, rgb(255, 255, 255), 0.75);
  cairo_fill(cr);
}

// --- Cable Eel -------------------------------------------------------------------------------

void eelHead(cairo_t* cr, double w, double h, bool open)
{
  (void)w;
  (void)h;
  cairo_save(cr);
  cairo_translate(cr, kM, kM);
  const double lift = open ? 14.0 : 0.0;
  if (open)
  {
    // The mouth's inside and the spark between the jaws.
    cairo_move_to(cr, 40, 48);
    cairo_line_to(cr, 96, 26);
    cairo_line_to(cr, 96, 72);
    cairo_close_path(cr);
    setRgba(cr, rgb(50, 10, 30), 1.0);
    cairo_fill(cr);
    glow(cr, 80, 48, 26, rgb(160, 240, 255), 0.8);
    cairo_move_to(cr, 70, 34);
    cairo_line_to(cr, 78, 46);
    cairo_line_to(cr, 72, 50);
    cairo_line_to(cr, 82, 64);
    stroke(cr, rgb(240, 255, 255), 2.2);
  }
  // Upper head.
  cairo_move_to(cr, 0, 20);
  cairo_curve_to(cr, 30, 16, 58, 14, 74, 22 - lift * 0.5);
  cairo_curve_to(cr, 86, 28 - lift, 94, 38 - lift, 94, 46 - lift);
  cairo_line_to(cr, 44, 50);
  cairo_line_to(cr, 0, 50);
  cairo_close_path(cr);
  vgrad(cr, 14, 50, {{0.0, rgb(52, 62, 80)}, {1.0, rgb(18, 22, 34)}}, 1.0, true);
  stroke(cr, kInk, 2.4);
  // Lower jaw.
  cairo_move_to(cr, 0, 50);
  cairo_line_to(cr, 44, 50);
  cairo_line_to(cr, 92, 52 + lift);
  cairo_curve_to(cr, 88, 64 + lift, 66, 76, 40, 78);
  cairo_curve_to(cr, 20, 79, 8, 78, 0, 76);
  cairo_close_path(cr);
  vgrad(cr, 50, 78, {{0.0, rgb(30, 36, 50)}, {1.0, rgb(70, 82, 100)}}, 1.0, true);
  stroke(cr, kInk, 2.4);
  // Teeth along both jaws.
  for (int k = 0; k < 5; ++k)
  {
    const double u = (k + 0.5) / 5.0;
    const double ux = 48 + u * 44, uy = 50 - u * lift * 0.9 + (open ? 0 : -1);
    cairo_move_to(cr, ux - 2, uy);
    cairo_line_to(cr, ux, uy + 6);
    cairo_line_to(cr, ux + 2, uy);
    cairo_close_path(cr);
    const double ly = 51 + u * lift;
    cairo_move_to(cr, ux - 2, ly);
    cairo_line_to(cr, ux + 0.5, ly - 6);
    cairo_line_to(cr, ux + 2, ly);
    cairo_close_path(cr);
  }
  setRgba(cr, rgb(230, 240, 236), 1.0);
  cairo_fill(cr);
  // The glowing line down its side, ending at the gill.
  for (int k = 0; k < 4; ++k)
  {
    glow(cr, 6 + k * 9, 40, 6, rgb(90, 240, 255), 0.7);
    circle(cr, 6 + k * 9, 40, 2.2);
    setRgba(cr, rgb(210, 255, 255), 1.0);
    cairo_fill(cr);
  }
  cairo_move_to(cr, 40, 26);
  cairo_curve_to(cr, 46, 34, 46, 44, 40, 50);
  stroke(cr, rgb(10, 14, 24), 2.0);
  // The eye: amber, glowing.
  glow(cr, 64, 32 - lift * 0.4, 12, rgb(255, 190, 60), 0.7);
  circle(cr, 64, 32 - lift * 0.4, 5.2);
  setRgba(cr, rgb(255, 210, 90), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.6);
  ellipse(cr, 65, 32 - lift * 0.4, 1.4, 4.0);
  setRgba(cr, rgb(10, 8, 14), 1.0);
  cairo_fill(cr);
  // A cold rim along the top.
  cairo_move_to(cr, 2, 22);
  cairo_curve_to(cr, 30, 18, 58, 16, 74, 24 - lift * 0.5);
  stroke(cr, kRim, 2.0, 0.5);
  // Electrode whiskers.
  for (const double s : {-1.0, 1.0})
  {
    const double wy = 50 + s * (8 + lift * 0.8);
    cairo_move_to(cr, 84, wy);
    cairo_line_to(cr, 100, wy + s * 8);
    stroke(cr, rgb(180, 190, 200), 1.6);
    circle(cr, 100, wy + s * 8, 2.2);
    setRgba(cr, rgb(160, 250, 255), 1.0);
    cairo_fill(cr);
  }
  cairo_restore(cr);
}

void eelBody(cairo_t* cr, double w, double h)
{
  const double cx = kM + w * 0.5, cy = kM + h * 0.5, rad = std::min(w, h) * 0.44;
  circle(cr, cx, cy, rad);
  vgrad(cr, cy - rad, cy + rad, {{0.0, rgb(58, 70, 90)}, {0.45, rgb(22, 28, 40)}, {1.0, rgb(60, 72, 92)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  // A cable's banding and its glowing core showing through.
  glow(cr, cx, cy, rad * 0.55, rgb(90, 240, 255), 0.55);
  circle(cr, cx, cy, rad * 0.16);
  setRgba(cr, rgb(210, 255, 255), 1.0);
  cairo_fill(cr);
  cairo_arc(cr, cx, cy, rad * 0.86, -kPi * 0.85, -kPi * 0.2);
  stroke(cr, kRim, 1.8, 0.45);
}

// --- The junction box ------------------------------------------------------------------------

void junctionBox(cairo_t* cr, double w, double h, bool quiet, bool lit)
{
  const double x = kM + 2, y = kM + 2, bw = w - 4, bh = h - 4;
  // The box.
  roundedRect(cr, x, y, bw, bh, 7);
  vgrad(cr, y, y + bh, {{0.0, rgb(150, 164, 172)}, {0.5, rgb(104, 118, 128)}, {1.0, rgb(58, 70, 80)}}, 1.0, true);
  stroke(cr, kInk, 2.2);
  // Weathering: rust bloom and a crust of growth along the bottom.
  for (const auto& rs : {std::array<double, 3>{0.2, 0.75, 7}, std::array<double, 3>{0.62, 0.3, 5}})
    glow(cr, x + rs[0] * bw, y + rs[1] * bh, rs[2], rgb(170, 90, 40), 0.55);
  roundedRect(cr, x, y + bh - 6, bw, 6, 3);
  setRgba(cr, rgb(60, 120, 100), 0.5);
  cairo_fill(cr);
  // The lid's seam and four bolts.
  roundedRect(cr, x + 5, y + 5, bw - 10, bh - 10, 5);
  stroke(cr, rgb(40, 50, 58), 1.2, 0.8);
  for (const double bx : {x + 8.0, x + bw - 8.0})
    for (const double by : {y + 8.0, y + bh - 8.0})
    {
      circle(cr, bx, by, 2.6);
      vgrad(cr, by - 3, by + 3, {{0.0, rgb(220, 226, 230)}, {1.0, rgb(90, 100, 110)}}, 1.0, true);
      stroke(cr, kInk, 0.8);
    }
  // A hazard triangle with its bolt.
  const double tx = x + bw * 0.4, ty = y + bh * 0.52;
  cairo_move_to(cr, tx, ty - 10);
  cairo_line_to(cr, tx + 10, ty + 8);
  cairo_line_to(cr, tx - 10, ty + 8);
  cairo_close_path(cr);
  setRgba(cr, lit ? rgb(255, 220, 60) : rgb(220, 180, 50), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.4);
  cairo_move_to(cr, tx + 1, ty - 5);
  cairo_line_to(cr, tx - 3, ty + 2);
  cairo_line_to(cr, tx + 1, ty + 1);
  cairo_line_to(cr, tx - 1, ty + 6);
  stroke(cr, kInk, 1.4);
  if (quiet)
  {
    // A little speaker grille in place of a mouth.
    for (int k = 0; k < 3; ++k)
      for (int j = 0; j < 3; ++j)
      {
        circle(cr, x + bw * 0.74 + (k - 1) * 4.5, y + bh * 0.5 + (j - 1) * 4.5, 1.4);
        setRgba(cr, rgb(20, 24, 30), 1.0);
        cairo_fill(cr);
      }
  }
  else
  {
    // The mouth: a hatch on the right with rubber flaps.
    roundedRect(cr, x + bw - 10, y + bh * 0.24, 10, bh * 0.52, 3);
    setRgba(cr, rgb(12, 14, 20), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.2);
    for (int k = 1; k < 4; ++k)
    {
      cairo_move_to(cr, x + bw - 10, y + bh * (0.24 + 0.13 * k));
      cairo_line_to(cr, x + bw - 1, y + bh * (0.24 + 0.13 * k));
      stroke(cr, rgb(60, 66, 74), 1.0);
    }
    // A conduit stub on the back.
    roundedRect(cr, x - 4, y + bh * 0.4, 6, bh * 0.2, 2);
    setRgba(cr, rgb(70, 80, 90), 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.0);
  }
  // The LED.
  const Color led = lit ? rgb(255, 190, 60) : rgb(90, 255, 160);
  if (lit)
    glow(cr, x + bw - 9, y + 9, 9, led, 0.8);
  circle(cr, x + bw - 9, y + 9, 2.4);
  setRgba(cr, led, 1.0);
  cairo_fill(cr);
}

// --- The duck's diving mask ------------------------------------------------------------------

void duckMask(cairo_t* cr, double w, double h, int variant)
{
  if (variant != 1)
    return;
  cairo_save(cr);
  cairo_translate(cr, kM, kM);
  cairo_scale(cr, w / 64.0, h / 64.0);
  // The strap round the back of its head.
  cairo_move_to(cr, 32, 20);
  cairo_curve_to(cr, 28, 24, 27, 30, 30, 33);
  stroke(cr, rgb(20, 24, 30), 3.4);
  // The snorkel: a tube up the side of its head, an orange tip.
  cairo_move_to(cr, 29, 30);
  cairo_curve_to(cr, 25, 26, 24, 14, 26, 3);
  stroke(cr, rgb(20, 24, 30), 5.0);
  cairo_move_to(cr, 29, 30);
  cairo_curve_to(cr, 25, 26, 24, 14, 26, 3);
  stroke(cr, rgb(60, 190, 230), 3.0);
  roundedRect(cr, 23.5, 1, 5.5, 5, 1.5);
  setRgba(cr, rgb(255, 130, 40), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, rgb(20, 24, 30), 1.0);
  // The mask: a rubber frame round a pane of glass over its eye.
  roundedRect(cr, 32, 13, 17, 12, 4.5);
  setRgba(cr, rgb(30, 36, 44), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, rgb(10, 12, 16), 1.0);
  roundedRect(cr, 34, 15, 13, 8, 3);
  cairo_pattern_t* g = cairo_pattern_create_linear(34, 15, 47, 23);
  cairo_pattern_add_color_stop_rgba(g, 0, 0.75, 0.95, 1.0, 0.9);
  cairo_pattern_add_color_stop_rgba(g, 1, 0.3, 0.6, 0.8, 0.85);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
  // Its eye behind the glass, and a glint.
  circle(cr, 41, 19, 1.8);
  setRgba(cr, rgb(10, 10, 14), 1.0);
  cairo_fill(cr);
  cairo_move_to(cr, 35.5, 21);
  cairo_line_to(cr, 38.5, 16);
  stroke(cr, rgb(255, 255, 255), 1.2, 0.85);
  cairo_restore(cr);
}

} // namespace

bool drawTrenchArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame)
{
  (void)t;
  if (key == "anglerfish")
    anglerfish(cr, w, h, std::clamp(variant, 0, 3), frame % 2);
  else if (key == "box_jelly")
    boxJelly(cr, w, h, variant == 1, frame % 4);
  else if (key == "cable_eel_head")
    eelHead(cr, w, h, variant == 1);
  else if (key == "cable_eel_body")
    eelBody(cr, w, h);
  else if (key == "junction_box")
    junctionBox(cr, w, h, variant == 1, frame == 1);
  else if (key == "duck_diving_mask")
    duckMask(cr, w, h, variant);
  else
    return false;
  return true;
}

} // namespace gr
