// The Live Finale (Level 42) in the style of enemy_art.cpp: smooth shapes
// with a dark ink line, lit cool from the rig above and gold from the
// stage. The Censor Bar's cassette; the Ad Break's rolling billboard and
// its free samples; the Stunt Doubles; the duck in its host jacket. Each is
// drawn in screen pixels into a texture with a 32 px margin, the cell box
// at (32, 32) .. (32 + w, 32 + h), facing right.

#include "assets/enemy_art_finale42.hpp"

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
constexpr Color kInk = rgb(8, 8, 18);
constexpr Color kGold = rgb(236, 186, 74);
constexpr Color kGoldHi = rgb(255, 242, 180);
constexpr Color kGoldLo = rgb(124, 84, 24);
constexpr Color kChrome = rgb(196, 204, 222);
constexpr Color kChromeLo = rgb(70, 76, 98);
constexpr Color kHalcyon = rgb(30, 70, 170);

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

// Fills the path lit cool from the top left over a gold bounce at the
// bottom right (the stage).
void lit(cairo_t* cr, double x0, double y0, double x1, double y1, Color c, bool preserve = true)
{
  cairo_pattern_t* g = cairo_pattern_create_linear(x0, y0, x1, y1);
  stop(g, 0.0, lerpColor(lighten(c, 0.22f), rgb(150, 190, 255), 0.08f));
  stop(g, 0.5, c);
  stop(g, 1.0, lerpColor(darken(c, 0.25f), rgb(255, 190, 80), 0.12f));
  cairo_set_source(cr, g);
  if (preserve)
    cairo_fill_preserve(cr);
  else
    cairo_fill(cr);
  cairo_pattern_destroy(g);
}

void vgrad(cairo_t* cr, double y0, double y1, std::initializer_list<std::pair<double, Color>> stops, bool preserve = true)
{
  cairo_pattern_t* g = cairo_pattern_create_linear(0, y0, 0, y1);
  for (const auto& s : stops)
    stop(g, s.first, s.second);
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

void ink(cairo_t* cr, double w = 2.0) { stroke(cr, kInk, w); }

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

void frameBox(cairo_t* cr, double w, double h, double dw, double dh)
{
  cairo_translate(cr, kM, kM);
  cairo_scale(cr, w / dw, h / dh);
}

void limb(cairo_t* cr, std::initializer_list<std::array<double, 2>> pts, double width, Color c)
{
  for (int pass = 0; pass < 3; ++pass)
  {
    bool first = true;
    for (const auto& p : pts)
    {
      if (first)
        cairo_move_to(cr, p[0], p[1] - (pass == 2 ? width * 0.25 : 0.0));
      else
        cairo_line_to(cr, p[0], p[1] - (pass == 2 ? width * 0.25 : 0.0));
      first = false;
    }
    if (pass == 0)
      stroke(cr, kInk, width + 3.4);
    else if (pass == 1)
      stroke(cr, c, width);
    else
      stroke(cr, lighten(c, 0.3f), width * 0.25, 0.45);
  }
}

void starPath(cairo_t* cr, double cx, double cy, double ro, double ri, int points)
{
  for (int i = 0; i < points * 2; ++i)
  {
    const double a = -kPi / 2 + i * kPi / points, rr = i % 2 ? ri : ro;
    if (i == 0)
      cairo_move_to(cr, cx + std::cos(a) * rr, cy + std::sin(a) * rr);
    else
      cairo_line_to(cr, cx + std::cos(a) * rr, cy + std::sin(a) * rr);
  }
  cairo_close_path(cr);
}

// The Halcyon Arms shield: a gold-rimmed blue shield with an H on it.
void shield(cairo_t* cr, double cx, double cy, double s)
{
  cairo_move_to(cr, cx - s, cy - s);
  cairo_line_to(cr, cx + s, cy - s);
  cairo_curve_to(cr, cx + s, cy + s * 0.4, cx + s * 0.4, cy + s * 0.9, cx, cy + s * 1.2);
  cairo_curve_to(cr, cx - s * 0.4, cy + s * 0.9, cx - s, cy + s * 0.4, cx - s, cy - s);
  cairo_close_path(cr);
  lit(cr, cx - s, cy - s, cx + s, cy + s, kHalcyon);
  stroke(cr, kGold, s * 0.14);
  text(cr, "H", cx, cy + s * 0.45, s * 1.25, kGoldHi);
}

// --- Censor Bar ---------------------------------------------------------------------------------

void censorBar(cairo_t* cr, double w, double h, int frame)
{
  cairo_save(cr);
  frameBox(cr, w, h, 128, 64);
  // The rotors on their arms.
  for (const double rx : {22.0, 106.0})
  {
    cairo_move_to(cr, rx, 22);
    cairo_line_to(cr, rx, 10);
    stroke(cr, kInk, 4.0);
    cairo_move_to(cr, rx, 22);
    cairo_line_to(cr, rx, 10);
    stroke(cr, rgb(70, 70, 86), 2.0);
    if (frame)
    {
      ellipse(cr, rx, 9, 19, 3);
      setRgba(cr, rgb(200, 210, 230), 0.35);
      cairo_fill(cr);
    }
    else
    {
      cairo_move_to(cr, rx - 18, 7);
      cairo_line_to(cr, rx + 18, 11);
      stroke(cr, kInk, 3.6);
      cairo_move_to(cr, rx - 18, 7);
      cairo_line_to(cr, rx + 18, 11);
      stroke(cr, rgb(140, 146, 170), 1.8);
    }
    circle(cr, rx, 9, 3);
    setRgba(cr, rgb(40, 40, 50), 1.0);
    cairo_fill(cr);
  }
  // The black lip of the bar hanging from its slot.
  cairo_rectangle(cr, 16, 44, 96, 12);
  vgrad(cr, 44, 56, {{0.0, rgb(30, 30, 36)}, {1.0, rgb(0, 0, 0)}});
  ink(cr, 1.4);
  // The cassette.
  roundRect(cr, 4, 18, 120, 30, 14);
  vgrad(cr, 18, 48, {{0.0, rgb(90, 92, 110)}, {0.18, rgb(30, 30, 40)}, {0.7, rgb(10, 10, 16)}, {1.0, rgb(24, 22, 30)}});
  ink(cr, 2.2);
  cairo_move_to(cr, 16, 22);
  cairo_line_to(cr, 112, 22);
  stroke(cr, rgb(255, 255, 255), 1.4, 0.35);
  // The chrome end caps.
  for (const double cx : {14.0, 114.0})
  {
    circle(cr, cx, 33, 10);
    lit(cr, cx - 10, 23, cx + 10, 43, kChrome);
    ink(cr, 1.6);
    circle(cr, cx, 33, 3.5);
    setRgba(cr, kChromeLo, 1.0);
    cairo_fill(cr);
  }
  // CENSORED, stencilled.
  text(cr, "CENSORED", 64, 39, 13, rgb(240, 240, 245), 0.95);
  // The tally at its nose.
  softDot(cr, 122, 33, 9, 9, rgb(255, 40, 50), 0.6);
  circle(cr, 122, 33, 3);
  setRgba(cr, rgb(255, 120, 120), 1.0);
  cairo_fill(cr);
  cairo_restore(cr);
}

// --- Ad Break ---------------------------------------------------------------------------------

void adScreen(cairo_t* cr, int ad)
{
  // The screen at (16, 32) 160 x 124.
  cairo_save(cr);
  cairo_rectangle(cr, 16, 32, 160, 124);
  cairo_clip(cr);
  switch (ad % 4)
  {
    case 0: // The shield.
      cairo_rectangle(cr, 16, 32, 160, 124);
      vgrad(cr, 32, 156, {{0.0, rgb(20, 40, 110)}, {1.0, rgb(6, 12, 50)}}, false);
      softDot(cr, 96, 84, 70, 50, rgb(90, 140, 255), 0.4);
      shield(cr, 96, 80, 30);
      text(cr, "HALCYON ARMS", 96, 146, 15, kGoldHi);
      break;
    case 1: // The rifle, NEW!
      cairo_rectangle(cr, 16, 32, 160, 124);
      vgrad(cr, 32, 156, {{0.0, rgb(10, 14, 30)}, {1.0, rgb(30, 40, 80)}}, false);
      for (int i = 0; i < 12; ++i)
      {
        const double a = i * kPi / 6;
        cairo_move_to(cr, 96, 100);
        cairo_line_to(cr, 96 + std::cos(a) * 140, 100 + std::sin(a) * 140);
        cairo_line_to(cr, 96 + std::cos(a + 0.2) * 140, 100 + std::sin(a + 0.2) * 140);
        cairo_close_path(cr);
        setRgba(cr, rgb(90, 140, 255), 0.12);
        cairo_fill(cr);
      }
      // The rifle in silhouette.
      cairo_move_to(cr, 34, 104);
      cairo_line_to(cr, 70, 96);
      cairo_line_to(cr, 150, 96);
      cairo_line_to(cr, 160, 92);
      cairo_line_to(cr, 162, 100);
      cairo_line_to(cr, 120, 104);
      cairo_line_to(cr, 104, 118);
      cairo_line_to(cr, 92, 118);
      cairo_line_to(cr, 96, 106);
      cairo_line_to(cr, 72, 110);
      cairo_line_to(cr, 40, 122);
      cairo_close_path(cr);
      lit(cr, 30, 90, 160, 120, rgb(150, 160, 190));
      ink(cr, 1.6);
      starPath(cr, 140, 58, 26, 16, 10);
      lit(cr, 114, 32, 166, 84, rgb(255, 60, 70));
      ink(cr, 1.6);
      text(cr, "NEW!", 140, 64, 15, rgb(255, 255, 255));
      break;
    case 2: // BUY NOW!
      cairo_rectangle(cr, 16, 32, 160, 124);
      vgrad(cr, 32, 156, {{0.0, rgb(230, 40, 60)}, {1.0, rgb(120, 10, 30)}}, false);
      for (int i = 0; i < 8; ++i)
        softDot(cr, 24 + i * 22, 44 + (i % 3) * 40, 6, 6, kGoldHi, 0.7);
      text(cr, "BUY", 96, 90, 38, rgb(255, 255, 255));
      text(cr, "NOW!", 96, 132, 38, kGoldHi);
      break;
    default: // 42% OFF.
      cairo_rectangle(cr, 16, 32, 160, 124);
      vgrad(cr, 32, 156, {{0.0, rgb(255, 210, 80)}, {1.0, rgb(230, 140, 30)}}, false);
      circle(cr, 96, 92, 50);
      setRgba(cr, rgb(255, 255, 255), 0.95);
      cairo_fill(cr);
      text(cr, "42%", 96, 100, 34, rgb(220, 30, 50));
      text(cr, "OFF", 96, 126, 18, kHalcyon);
      break;
  }
  // The scanlines and a gloss.
  for (double y = 33; y < 156; y += 3)
  {
    cairo_rectangle(cr, 16, y, 160, 1);
    setRgba(cr, rgb(0, 0, 0), 0.12);
    cairo_fill(cr);
  }
  cairo_move_to(cr, 16, 32);
  cairo_line_to(cr, 110, 32);
  cairo_line_to(cr, 16, 100);
  cairo_close_path(cr);
  setRgba(cr, rgb(255, 255, 255), 0.07);
  cairo_fill(cr);
  cairo_restore(cr);
}

void adBreak(cairo_t* cr, double w, double h, bool jingle, int frame)
{
  cairo_save(cr);
  frameBox(cr, w, h, 192, 256);
  // The dolly and its wheels.
  for (const double wx : {34.0, 76.0, 116.0, 158.0})
  {
    circle(cr, wx, 244, 10);
    lit(cr, wx - 10, 234, wx + 10, 254, rgb(40, 40, 50));
    ink(cr, 1.8);
    circle(cr, wx, 244, 3.5);
    setRgba(cr, kChrome, 1.0);
    cairo_fill(cr);
  }
  roundRect(cr, 14, 218, 164, 20, 5);
  lit(cr, 14, 218, 178, 238, rgb(56, 60, 80));
  ink(cr, 2.0);
  for (double x = 22; x < 176; x += 16)
  {
    cairo_move_to(cr, x, 236);
    cairo_line_to(cr, x + 8, 220);
    stroke(cr, rgb(240, 200, 40), 4.0, 0.85);
  }
  // The legs.
  for (const double lx : {40.0, 152.0})
  {
    cairo_move_to(cr, lx, 218);
    cairo_line_to(cr, lx, 160);
    stroke(cr, kChromeLo, 9.0);
    cairo_move_to(cr, lx, 218);
    cairo_line_to(cr, lx, 160);
    stroke(cr, kChrome, 4.0);
  }
  // The SAMPLES hatch.
  roundRect(cr, 62, 168, 68, 50, 5);
  lit(cr, 62, 168, 130, 218, rgb(36, 40, 70));
  ink(cr, 2.0);
  if (jingle)
  {
    cairo_rectangle(cr, 68, 184, 56, 32);
    setRgba(cr, rgb(4, 4, 10), 1.0);
    cairo_fill(cr);
    softDot(cr, 96, 204, 26, 12, kGold, 0.5);
    cairo_rectangle(cr, 68, 172, 56, 12);
    lit(cr, 68, 172, 124, 184, rgb(80, 86, 120));
    ink(cr, 1.4);
  }
  else
  {
    cairo_rectangle(cr, 68, 184, 56, 30);
    lit(cr, 68, 184, 124, 214, rgb(60, 66, 100));
    ink(cr, 1.4);
  }
  text(cr, "SAMPLES", 96, 181, 9.5, kGoldHi);
  // The billboard: gold frame, the screen.
  roundRect(cr, 4, 20, 184, 148, 10);
  lit(cr, 4, 20, 188, 168, kGold);
  ink(cr, 2.4);
  roundRect(cr, 10, 26, 172, 136, 6);
  setRgba(cr, kGoldLo, 1.0);
  cairo_fill(cr);
  adScreen(cr, frame);
  cairo_rectangle(cr, 16, 32, 160, 124);
  ink(cr, 1.6);
  // Bulbs along the frame.
  for (double x = 14; x < 184; x += 17)
  {
    circle(cr, x, 23, 2.2);
    setRgba(cr, (int(x) / 17 + frame) % 2 ? kGoldHi : rgb(200, 150, 70), 1.0);
    cairo_fill(cr);
  }
  // The horn speakers.
  for (int side = 0; side < 2; ++side)
  {
    const double sx = side ? 160 : 32, dir = side ? 1 : -1;
    const double flare = jingle ? 1.35 : 1.0;
    cairo_move_to(cr, sx - dir * 6, 20);
    cairo_line_to(cr, sx - dir * 6, 8);
    stroke(cr, kChromeLo, 4.0);
    cairo_move_to(cr, sx - dir * 8, 4);
    cairo_line_to(cr, sx + dir * 4, 0);
    cairo_line_to(cr, sx + dir * 22 * flare, -10 * flare);
    cairo_line_to(cr, sx + dir * 22 * flare, 18 * flare);
    cairo_line_to(cr, sx + dir * 4, 10);
    cairo_line_to(cr, sx - dir * 8, 8);
    cairo_close_path(cr);
    lit(cr, sx - 20, -10, sx + 20, 18, rgb(220, 224, 236));
    ink(cr, 1.8);
    ellipse(cr, sx + dir * 22 * flare, 4, 3, 14 * flare);
    setRgba(cr, rgb(30, 30, 40), 1.0);
    cairo_fill(cr);
    if (jingle)
      for (int k = 1; k <= 2; ++k)
      {
        cairo_arc(cr, sx + dir * 22 * flare, 4, 8.0 + k * 7.0, side ? -0.8 : kPi - 0.8, side ? 0.8 : kPi + 0.8);
        stroke(cr, kGoldHi, 2.0, 0.8 / k);
      }
  }
  if (jingle)
  {
    // Notes.
    for (int k = 0; k < 3; ++k)
    {
      const double nx = 60 + k * 36, ny = 4 - (k % 2) * 10;
      ellipse(cr, nx, ny, 5, 3.6, -0.4);
      setRgba(cr, kGoldHi, 1.0);
      cairo_fill_preserve(cr);
      ink(cr, 1.2);
      cairo_move_to(cr, nx + 4.4, ny - 1);
      cairo_line_to(cr, nx + 4.4, ny - 18);
      cairo_line_to(cr, nx + 11, ny - 14);
      stroke(cr, kGoldHi, 2.0);
    }
  }
  cairo_restore(cr);
}

// --- Sample bot ---------------------------------------------------------------------------------

void sampleBot(cairo_t* cr, double w, double h, int frame)
{
  cairo_save(cr);
  frameBox(cr, w, h, 64, 96);
  const double step = frame ? 5.0 : -5.0;
  // The legs and feet.
  limb(cr, {{24, 70}, {24 - step, 88}}, 5.0, rgb(60, 64, 84));
  limb(cr, {{40, 70}, {40 + step, 88}}, 5.0, rgb(60, 64, 84));
  for (const double fx : {24 - step, 40 + step})
  {
    ellipse(cr, fx + 3, 90, 7, 4);
    lit(cr, fx - 4, 86, fx + 10, 94, rgb(220, 60, 70));
    ink(cr, 1.4);
  }
  // The box.
  roundRect(cr, 6, 30, 52, 44, 5);
  lit(cr, 6, 30, 58, 74, kHalcyon);
  ink(cr, 2.2);
  // The ribbon.
  cairo_rectangle(cr, 28, 30, 8, 44);
  lit(cr, 28, 30, 36, 74, kGold, false);
  cairo_rectangle(cr, 6, 46, 52, 7);
  lit(cr, 6, 46, 58, 53, kGold, false);
  // The bow.
  for (const double d : {-1.0, 1.0})
  {
    ellipse(cr, 32 + d * 10, 24, 10, 6, d * 0.4);
    lit(cr, 20, 16, 44, 30, kGold);
    ink(cr, 1.6);
  }
  circle(cr, 32, 27, 4);
  lit(cr, 28, 23, 36, 31, kGoldHi);
  ink(cr, 1.4);
  // The eyes, looking right.
  for (const double ex : {40.0, 52.0})
  {
    ellipse(cr, ex, 40, 5, 6);
    setRgba(cr, rgb(255, 255, 255), 1.0);
    cairo_fill_preserve(cr);
    ink(cr, 1.3);
    circle(cr, ex + 1.8, 41, 2.4);
    setRgba(cr, kInk, 1.0);
    cairo_fill(cr);
  }
  // A grin under them.
  cairo_arc(cr, 46, 60, 7, 0.2, kPi - 0.2);
  stroke(cr, kInk, 2.0);
  // The FREE tag on a string.
  cairo_move_to(cr, 10, 34);
  cairo_curve_to(cr, 2, 40, 0, 50, 2, 56);
  stroke(cr, rgb(240, 240, 240), 1.0);
  roundRect(cr, -6, 54, 18, 11, 2);
  setRgba(cr, rgb(255, 255, 255), 1.0);
  cairo_fill_preserve(cr);
  ink(cr, 1.2);
  text(cr, "FREE", 3, 62.5, 5.8, rgb(220, 30, 50));
  cairo_restore(cr);
}

// --- Stunt Double -------------------------------------------------------------------------------

void stuntDouble(cairo_t* cr, double w, double h, int who, bool tell, int frame)
{
  static const Color kStripe[] = {rgb(232, 64, 52), rgb(104, 146, 64), rgb(132, 66, 216)};
  static const Color kHair[] = {rgb(255, 214, 72), rgb(240, 70, 70), rgb(255, 86, 184)};
  const Color stripe = kStripe[who % 3], hair = kHair[who % 3];
  const Color suit = rgb(30, 30, 40);
  const double bulk = who == 1 ? 1.15 : (who == 2 ? 0.9 : 1.0);
  cairo_save(cr);
  frameBox(cr, w, h, 96, 160);
  const double run = frame ? 1.0 : -1.0;
  // The back leg and arm.
  limb(cr, {{44, 102}, {44 - 10 * run, 126}, {42 - 14 * run, 150}}, 11 * bulk, darken(suit, 0.2f));
  // Nova's ponytail swings behind.
  if (who == 2)
  {
    cairo_move_to(cr, 38, 30);
    cairo_curve_to(cr, 20, 34, 14, 50, 10, 66);
    cairo_curve_to(cr, 20, 58, 28, 46, 40, 38);
    cairo_close_path(cr);
    lit(cr, 10, 30, 40, 66, hair);
    ink(cr, 1.6);
  }
  // The torso: a padded suit, the runner's stripe, STUNT.
  roundRect(cr, 48 - 17 * bulk, 50, 34 * bulk, 56, 10);
  lit(cr, 30, 50, 66, 106, suit);
  ink(cr, 2.2);
  cairo_save(cr);
  roundRect(cr, 48 - 17 * bulk, 50, 34 * bulk, 56, 10);
  cairo_clip(cr);
  cairo_move_to(cr, 30, 56);
  cairo_line_to(cr, 66, 80);
  cairo_line_to(cr, 66, 88);
  cairo_line_to(cr, 30, 64);
  cairo_close_path(cr);
  setRgba(cr, stripe, 1.0);
  cairo_fill(cr);
  // The pads.
  for (const double py : {62.0, 78.0, 94.0})
  {
    cairo_move_to(cr, 30, py);
    cairo_line_to(cr, 66, py);
    stroke(cr, rgb(60, 60, 76), 1.2, 0.8);
  }
  cairo_restore(cr);
  text(cr, "STUNT", 49, 76, 9, rgb(240, 240, 240), 0.95);
  // The belt.
  cairo_rectangle(cr, 48 - 17 * bulk, 100, 34 * bulk, 6);
  setRgba(cr, rgb(16, 16, 22), 1.0);
  cairo_fill(cr);
  cairo_rectangle(cr, 46, 100, 6, 6);
  setRgba(cr, kChrome, 1.0);
  cairo_fill(cr);
  // The front leg, a knee pad, the boot.
  limb(cr, {{52, 102}, {54 + 12 * run, 126}, {52 + 16 * run, 150}}, 11 * bulk, suit);
  ellipse(cr, 54 + 12 * run, 126, 7, 6);
  lit(cr, 46, 120, 62, 132, stripe);
  ink(cr, 1.4);
  for (const double bx : {42 - 14 * run, 52 + 16 * run})
  {
    roundRect(cr, bx - 7, 146, 18, 10, 4);
    lit(cr, bx - 7, 146, bx + 11, 156, rgb(20, 20, 26));
    ink(cr, 1.4);
  }
  // The head: the black mask with white eye-holes, the wig.
  if (who == 0)
    for (int k = 0; k < 6; ++k)
    {
      const double a = kPi * (1.05 + k * 0.17);
      cairo_move_to(cr, 50 + std::cos(a - 0.2) * 12, 30 + std::sin(a - 0.2) * 12);
      cairo_line_to(cr, 50 + std::cos(a) * 26, 30 + std::sin(a) * 24);
      cairo_line_to(cr, 50 + std::cos(a + 0.2) * 12, 30 + std::sin(a + 0.2) * 12);
      cairo_close_path(cr);
      lit(cr, 24, 6, 76, 30, hair);
      ink(cr, 1.3);
    }
  circle(cr, 50, 32, 15);
  lit(cr, 35, 17, 65, 47, rgb(24, 24, 32));
  ink(cr, 2.0);
  cairo_move_to(cr, 40, 22);
  cairo_curve_to(cr, 44, 18, 52, 17, 58, 20);
  stroke(cr, rgb(255, 255, 255), 1.4, 0.3);
  if (who == 1)
  {
    // Rocco's bandana over the mask, the knot's tails.
    cairo_move_to(cr, 35, 28);
    cairo_curve_to(cr, 38, 14, 62, 14, 65, 28);
    cairo_line_to(cr, 65, 24);
    cairo_curve_to(cr, 60, 16, 40, 16, 35, 24);
    cairo_close_path(cr);
    cairo_move_to(cr, 35, 26);
    cairo_curve_to(cr, 38, 12, 62, 12, 65, 26);
    cairo_close_path(cr);
    lit(cr, 35, 12, 65, 28, hair);
    ink(cr, 1.4);
    cairo_move_to(cr, 36, 24);
    cairo_line_to(cr, 24, 22);
    cairo_line_to(cr, 26, 30);
    cairo_close_path(cr);
    lit(cr, 24, 20, 36, 30, darken(hair, 0.15f));
    ink(cr, 1.2);
  }
  else if (who == 2)
  {
    cairo_move_to(cr, 36, 26);
    cairo_curve_to(cr, 38, 12, 60, 10, 66, 24);
    cairo_curve_to(cr, 56, 20, 46, 20, 36, 26);
    cairo_close_path(cr);
    lit(cr, 36, 10, 66, 26, hair);
    ink(cr, 1.4);
  }
  for (const double ex : {52.0, 60.0})
  {
    ellipse(cr, ex, 31, 3.2, 2.4, -0.15);
    setRgba(cr, rgb(250, 250, 250), 1.0);
    cairo_fill(cr);
    circle(cr, ex + 1.0, 31.2, 1.2);
    setRgba(cr, kInk, 1.0);
    cairo_fill(cr);
  }
  cairo_move_to(cr, 54, 40);
  cairo_line_to(cr, 61, 39);
  stroke(cr, rgb(250, 250, 250), 1.4, 0.8);
  // The gun arm: forward and level in the tell, the shot going.
  const double ax = tell ? 84 : 72, ay = tell ? 66 : 80;
  limb(cr, {{50, 58}, {tell ? 66.0 : 60.0, tell ? 64.0 : 74.0}, {ax, ay}}, 9 * bulk, suit);
  roundRect(cr, ax - 4, ay - 6, 20, 9, 2.5);
  lit(cr, ax - 4, ay - 6, ax + 16, ay + 3, rgb(90, 96, 120));
  ink(cr, 1.4);
  cairo_rectangle(cr, ax + 2, ay - 4, 10, 2);
  setRgba(cr, stripe, 1.0);
  cairo_fill(cr);
  circle(cr, ax, ay, 5);
  lit(cr, ax - 5, ay - 5, ax + 5, ay + 5, rgb(40, 40, 50));
  ink(cr, 1.3);
  if (tell)
  {
    softDot(cr, ax + 24, ay - 2, 14, 10, rgb(255, 230, 140), 0.9);
    starPath(cr, ax + 24, ay - 2, 10, 4, 6);
    setRgba(cr, rgb(255, 255, 230), 1.0);
    cairo_fill(cr);
  }
  cairo_restore(cr);
}

// --- The duck's host jacket ------------------------------------------------------------------------

void duckHost(cairo_t* cr, double w, double h, int variant, int frame)
{
  if (variant != 1)
    return;
  cairo_save(cr);
  cairo_translate(cr, kM, kM);
  cairo_scale(cr, w / 64.0, h / 64.0);
  // The jacket over the body, in gold sequins.
  cairo_save(cr);
  ellipse(cr, 30, 42, 20, 14);
  cairo_clip(cr);
  cairo_move_to(cr, 6, 31);
  cairo_line_to(cr, 54, 31);
  cairo_line_to(cr, 54, 52);
  cairo_curve_to(cr, 40, 56, 20, 56, 6, 52);
  cairo_close_path(cr);
  lit(cr, 10, 31, 50, 54, rgb(214, 160, 40), false);
  for (int k = 0; k < 40; ++k)
  {
    const double x = 8 + (k % 8) * 6 + ((k / 8) % 2) * 3, y = 33 + (k / 8) * 4.2;
    circle(cr, x, y, 1.2);
    setRgba(cr, (k + frame) % 3 == 0 ? rgb(255, 255, 255) : rgb(255, 226, 120), (k + frame) % 3 == 0 ? 1.0 : 0.7);
    cairo_fill(cr);
  }
  // The black satin lapels and the shirt's V.
  cairo_move_to(cr, 30, 31);
  cairo_line_to(cr, 42, 31);
  cairo_line_to(cr, 37, 46);
  cairo_close_path(cr);
  setRgba(cr, rgb(250, 250, 250), 1.0);
  cairo_fill(cr);
  for (const double d : {-1.0, 1.0})
  {
    cairo_move_to(cr, 36 + d * 7, 31);
    cairo_line_to(cr, 36 + d * 3, 46);
    cairo_line_to(cr, 36 + d * 11, 38);
    cairo_close_path(cr);
    lit(cr, 26, 31, 48, 46, rgb(26, 26, 36), false);
  }
  cairo_restore(cr);
  cairo_move_to(cr, 12, 52);
  cairo_curve_to(cr, 24, 55, 40, 55, 50, 50);
  stroke(cr, kInk, 1.6);
  // The bow tie.
  for (const double d : {-1.0, 1.0})
  {
    cairo_move_to(cr, 36, 33);
    cairo_line_to(cr, 36 + d * 6, 30);
    cairo_line_to(cr, 36 + d * 6, 36);
    cairo_close_path(cr);
    setRgba(cr, rgb(16, 16, 22), 1.0);
    cairo_fill_preserve(cr);
    ink(cr, 0.8);
  }
  // The chrome grin on the beak.
  roundRect(cr, 43, 30.5, 11, 4.2, 1.6);
  lit(cr, 43, 30.5, 54, 34.7, kChrome);
  ink(cr, 0.9);
  for (double x = 45.5; x < 53; x += 2.4)
  {
    cairo_move_to(cr, x, 30.8);
    cairo_line_to(cr, x, 34.4);
    stroke(cr, kChromeLo, 0.6);
  }
  softDot(cr, 46, 31.4, 2.5, 1.0, rgb(255, 255, 255), 0.9);
  cairo_restore(cr);
}

} // namespace

bool drawFinale42Art(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame)
{
  (void)t;
  if (key == "censorbar")
    censorBar(cr, w, h, frame % 2);
  else if (key == "adbreak")
    adBreak(cr, w, h, (variant & 1) != 0, frame % 4);
  else if (key == "sample_bot")
    sampleBot(cr, w, h, frame % 2);
  else if (key == "double")
    stuntDouble(cr, w, h, variant & 3, (variant & 4) != 0, frame % 2);
  else if (key == "duck_host")
    duckHost(cr, w, h, variant, frame % 2);
  else
    return false;
  return true;
}

} // namespace gr
