// Tiny Kingdom (Level 25) in the style of enemy_art.cpp: the Soldier Ant,
// the Jumping Spider and the Bombardier Beetle, each drawn in screen pixels
// into a texture with a 32 px margin, the cell box at (32, 32) .. (32 + w,
// 32 + h), facing right; and the rubber duck's pith helmet.

#include "assets/enemy_art_garden.hpp"

#include <algorithm>
#include <cmath>

namespace gr
{

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr Color kInk = rgb(24, 14, 10);
constexpr double kLine = 2.0;
constexpr double kM = 32.0;

Color darken(Color c, float t) { return lerpColor(c, rgb(0, 0, 0), t); }
Color lighten(Color c, float t) { return lerpColor(c, rgb(255, 255, 255), t); }

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

void stop(cairo_pattern_t* g, double at, Color c, double a = 1.0)
{
  cairo_pattern_add_color_stop_rgba(g, at, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, a);
}

void ink(cairo_t* cr, double line = kLine)
{
  setColor(cr, kInk);
  cairo_set_line_width(cr, line);
  cairo_stroke(cr);
}

// Glossy chitin: a radial gradient lit from the upper left, inked.
void chitin(cairo_t* cr, double x, double y, double r, Color c, double line = kLine)
{
  cairo_pattern_t* g = cairo_pattern_create_radial(x - r * 0.4, y - r * 0.5, r * 0.05, x, y, r * 1.1);
  stop(g, 0, lighten(c, 0.55f));
  stop(g, 0.35, c);
  stop(g, 1, darken(c, 0.55f));
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  if (line > 0)
    ink(cr, line);
  else
    cairo_new_path(cr);
}

// A shine: a small soft white crescent.
void shine(cairo_t* cr, double x, double y, double rx, double ry, double a, double alpha = 0.75)
{
  ellipse(cr, x, y, rx, ry, a);
  setRgba(cr, rgb(255, 255, 255), alpha);
  cairo_fill(cr);
}

void glow(cairo_t* cr, double x, double y, double r, Color c, double a)
{
  cairo_pattern_t* g = cairo_pattern_create_radial(x, y, 0, x, y, r);
  stop(g, 0, c, a);
  stop(g, 0.45, c, a * 0.45);
  stop(g, 1, c, 0.0);
  circle(cr, x, y, r);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
}

// A jointed leg: hip to knee to foot, `w` thick, inked; a dot at the knee.
void leg(cairo_t* cr, double hx, double hy, double kx, double ky, double fx, double fy, double w, Color c)
{
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
  cairo_move_to(cr, hx, hy);
  cairo_line_to(cr, kx, ky);
  cairo_line_to(cr, fx, fy);
  setColor(cr, kInk);
  cairo_set_line_width(cr, w + 2.4);
  cairo_stroke_preserve(cr);
  setColor(cr, c);
  cairo_set_line_width(cr, w);
  cairo_stroke(cr);
  cairo_move_to(cr, hx, hy);
  cairo_line_to(cr, kx, ky);
  setRgba(cr, lighten(c, 0.45f), 0.6);
  cairo_set_line_width(cr, std::max(0.8, w * 0.3));
  cairo_stroke(cr);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
  cairo_set_line_join(cr, CAIRO_LINE_JOIN_MITER);
}

// --- The Soldier Ant --------------------------------------------------------------

void soldierAnt(cairo_t* cr, double w, double h, int variant, int frame)
{
  const bool sick = variant >= 3;
  const int pose = variant % 3;
  const bool tell = pose == 1, angry = pose == 2;
  const double s = std::min(w / 96.0, h / 64.0);
  const double bx = kM + (w - 96 * s) * 0.5, by = kM + h - 64 * s;
  auto X = [&](double x) { return bx + x * s; };
  auto Y = [&](double y) { return by + y * s; };
  const Color body = sick ? rgb(132, 168, 70) : rgb(132, 44, 26);
  const Color bodyDark = sick ? rgb(70, 96, 40) : rgb(70, 22, 14);
  const Color legC = sick ? rgb(104, 132, 56) : rgb(96, 34, 20);
  // The shadow.
  ellipse(cr, X(50), Y(63), 40 * s, 3.5 * s);
  setRgba(cr, rgb(0, 0, 0), 0.25);
  cairo_fill(cr);
  // The march: a tripod gait, three legs a side; (near, phase) per leg.
  const double hips[3] = {50, 56, 62};
  const double reach[3] = {-16, 0, 16};
  const int f = tell ? 0 : frame % 4;
  auto footOf = [&](int i, bool near, double& fx, double& fy) {
    const bool groupA = (i % 2 == 0) == near;
    const double ph = (f + (groupA ? 0 : 2)) % 4;
    const double swing = ph == 0 ? -5 : (ph == 1 ? 0 : (ph == 2 ? 5 : 0));
    const double lift = ph == 1 ? 4 : 0;
    fx = X(hips[i] + reach[i] * 1.2 + swing);
    fy = Y(63 - lift);
  };
  // Far legs (behind the body).
  for (int i = 0; i < 3; ++i)
  {
    double fx, fy;
    footOf(i, false, fx, fy);
    const double hx = X(hips[i] + 1), hy = Y(36);
    leg(cr, hx, hy, (hx + fx) * 0.5 + reach[i] * 0.2 * s, Y(30), fx, fy, 2.6 * s, darken(legC, 0.35f));
  }
  // The gaster (abdomen), petiole nodes, thorax, head.
  ellipse(cr, X(22), Y(36), 20 * s, 15 * s, -0.15);
  chitin(cr, X(22), Y(36), 20 * s, body);
  if (!sick)
    for (int k = 0; k < 3; ++k)
    {
      cairo_new_path(cr);
      cairo_arc(cr, X(22), Y(36), (8 + k * 5) * s, -2.2, -1.0);
      setRgba(cr, bodyDark, 0.5);
      cairo_set_line_width(cr, 1.2);
      cairo_stroke(cr);
    }
  circle(cr, X(43), Y(37), 5 * s);
  chitin(cr, X(43), Y(37), 5 * s, body, 1.6);
  circle(cr, X(49), Y(34), 4.4 * s);
  chitin(cr, X(49), Y(34), 4.4 * s, body, 1.6);
  ellipse(cr, X(58), Y(30), 12 * s, 8.5 * s, -0.25);
  chitin(cr, X(58), Y(30), 12 * s, body);
  // The head: big, the soldier's.
  ellipse(cr, X(78), Y(25), 13.5 * s, 12 * s, 0.1);
  chitin(cr, X(78), Y(25), 13.5 * s, body);
  // Mandibles: closed, or open and gnashing.
  for (int k = 0; k < 2; ++k)
  {
    const double open = angry ? (k == 0 ? -0.5 : 0.45) : (k == 0 ? -0.08 : 0.1);
    cairo_save(cr);
    cairo_translate(cr, X(88), Y(31));
    cairo_rotate(cr, open);
    cairo_move_to(cr, 0, -2.5 * s);
    cairo_curve_to(cr, 6 * s, -3 * s, 12 * s, 0, 13 * s, 5 * s);
    cairo_curve_to(cr, 9 * s, 2 * s, 4 * s, 2 * s, 0, 2.5 * s);
    cairo_close_path(cr);
    cairo_restore(cr);
    setColor(cr, k == 0 ? darken(body, 0.3f) : body);
    cairo_fill_preserve(cr);
    ink(cr, 1.4);
  }
  // The eye.
  ellipse(cr, X(82), Y(20), 3.6 * s, 3 * s);
  setColor(cr, angry ? rgb(255, 60, 30) : rgb(20, 10, 8));
  cairo_fill(cr);
  shine(cr, X(81), Y(19), 1.2 * s, 0.9 * s, 0, 0.9);
  // The antennae: elbowed, forward; flared high on the tell.
  for (int k = 0; k < 2; ++k)
  {
    const double d = k == 0 ? 0 : 3;
    const double ex = tell ? 78 + d : 88 + d, ey = tell ? -6 : 2 + d;
    const double tx = tell ? 92 + d * 2 : 102 + d, ty = tell ? -20 + d : 4 + d * 1.5;
    cairo_move_to(cr, X(84), Y(15));
    cairo_line_to(cr, X(ex), Y(ey));
    cairo_line_to(cr, X(tx), Y(ty));
    setColor(cr, kInk);
    cairo_set_line_width(cr, 2.6 * s + 1.2);
    cairo_stroke_preserve(cr);
    setColor(cr, k == 0 ? darken(legC, 0.2f) : legC);
    cairo_set_line_width(cr, 2.0 * s);
    cairo_stroke(cr);
    circle(cr, X(tx), Y(ty), 2.2 * s);
    setColor(cr, legC);
    cairo_fill_preserve(cr);
    ink(cr, 1.0);
  }
  if (tell)
  {
    // Alarm marks round the head.
    for (int k = 0; k < 3; ++k)
    {
      const double a = -2.2 + k * 0.55;
      cairo_move_to(cr, X(86) + std::cos(a) * 26 * s, Y(8) + std::sin(a) * 26 * s);
      cairo_line_to(cr, X(86) + std::cos(a) * 34 * s, Y(8) + std::sin(a) * 34 * s);
    }
    setColor(cr, rgb(255, 230, 80));
    cairo_set_line_width(cr, 2.4);
    cairo_stroke(cr);
  }
  // Near legs.
  for (int i = 0; i < 3; ++i)
  {
    double fx, fy;
    footOf(i, true, fx, fy);
    const double hx = X(hips[i]), hy = Y(35);
    leg(cr, hx, hy, (hx + fx) * 0.5 + reach[i] * 0.25 * s, Y(28), fx, fy, 3.0 * s, legC);
  }
  // Shines.
  shine(cr, X(16), Y(28), 6 * s, 3 * s, -0.4, 0.55);
  shine(cr, X(74), Y(17), 4 * s, 2.2 * s, -0.3, 0.6);
  shine(cr, X(55), Y(25), 3.5 * s, 1.8 * s, -0.3, 0.5);
  if (sick)
  {
    // Blotches and a drip of green drool.
    const double spots[4][3] = {{16, 40, 3.5}, {28, 30, 2.6}, {60, 33, 2.4}, {74, 30, 2.2}};
    for (const auto& sp : spots)
    {
      circle(cr, X(sp[0]), Y(sp[1]), sp[2] * s);
      setRgba(cr, rgb(200, 230, 90), 0.8);
      cairo_fill(cr);
      circle(cr, X(sp[0]) + 0.6, Y(sp[1]) + 0.6, sp[2] * s * 0.45);
      setRgba(cr, rgb(70, 100, 30), 0.8);
      cairo_fill(cr);
    }
    cairo_move_to(cr, X(92), Y(34));
    cairo_curve_to(cr, X(93), Y(40), X(91), Y(44), X(92), Y(46 + (frame % 2) * 3));
    setRgba(cr, rgb(160, 240, 80), 0.9);
    cairo_set_line_width(cr, 2.2);
    cairo_stroke(cr);
    circle(cr, X(92), Y(47 + (frame % 2) * 3), 2.2 * s);
    setRgba(cr, rgb(160, 240, 80), 0.95);
    cairo_fill(cr);
    glow(cr, X(50), Y(32), 46 * s, rgb(140, 255, 80), 0.18);
  }
}

// --- The Jumping Spider ------------------------------------------------------------

void jumpingSpider(cairo_t* cr, double w, double h, int variant, int frame)
{
  const double s = std::min(w / 128.0, h / 96.0);
  const double bx = kM + (w - 128 * s) * 0.5, by = kM + h - 96 * s;
  auto X = [&](double x) { return bx + x * s; };
  auto Y = [&](double y) { return by + y * s; };
  const bool crouch = variant == 1, leap = variant == 2;
  const Color fur = rgb(62, 50, 50), furLight = rgb(150, 130, 120), band = rgb(244, 240, 230);
  const Color legC = rgb(84, 66, 60);
  const double drop = crouch ? 12 : 0;       // the body sinks for the crouch
  const double cy = (leap ? 48 : 58) + drop; // the cephalothorax's middle
  const double tilt = leap ? -0.22 : (crouch ? 0.06 : 0.0);
  if (!leap)
  {
    ellipse(cr, X(68), Y(95), 56 * s, 4 * s);
    setRgba(cr, rgb(0, 0, 0), 0.25);
    cairo_fill(cr);
  }
  // A leg's hip, knee and foot in design units; i 0 is the front pair.
  struct Pose
  {
    double hx, kx, ky, fx, fy;
  };
  auto pose = [&](int i, bool near) {
    static const double kStand[4][4] = {{92, 118, -18, 132}, {86, 104, -26, 114}, {80, 62, -26, 46}, {76, 38, -16, 12}};
    const double* l = kStand[i];
    const double o = near ? 0 : 5;
    Pose p{l[0] + o, l[1] + o, cy + l[2], l[3] + o, 95};
    if (crouch)
    {
      p.ky -= 10;
      p.fx += i < 2 ? -6 : 6;
    }
    if (leap)
    {
      if (i < 2)
      {
        p.kx = 118 - i * 6 + o;
        p.ky = cy - 22 + i * 4;
        p.fx = 138 - i * 2 + o;
        p.fy = cy - 6 + i * 10;
      }
      else
      {
        p.kx = 52 - (i - 2) * 12 + o;
        p.ky = cy + 6;
        p.fx = 16 - (i - 2) * 8 + o;
        p.fy = cy + 26 - (i - 2) * 4;
      }
    }
    else if (i == 1 && frame % 2 == 1 && !crouch)
      p.fx += 3; // a step
    return p;
  };
  auto drawLeg = [&](int i, bool near) {
    const Pose p = pose(i, near);
    const double hy = cy + 8;
    const Color c = near ? legC : darken(legC, 0.35f);
    leg(cr, X(p.hx), Y(hy), X(p.kx), Y(p.ky), X(p.fx), Y(p.fy), (near ? 6.0 : 5.0) * s, c);
    // White rings at the knee and halfway down the shin.
    for (const double u : {0.0, 0.5})
    {
      circle(cr, X(p.kx + (p.fx - p.kx) * u), Y(p.ky + (p.fy - p.ky) * u), 2.6 * s);
      setRgba(cr, band, near ? 0.9 : 0.45);
      cairo_fill(cr);
    }
  };
  for (int i = 3; i >= 0; --i)
    drawLeg(i, false);
  // The abdomen: fuzzy, a white chevron band and orange spots.
  cairo_save(cr);
  cairo_translate(cr, X(44), Y(cy - 6 + (leap ? 4 : 0)));
  cairo_rotate(cr, tilt);
  ellipse(cr, 0, 0, 27 * s, 21 * s);
  chitin(cr, 0, 0, 27 * s, fur);
  cairo_move_to(cr, -22 * s, -9 * s);
  cairo_curve_to(cr, -8 * s, -17 * s, 10 * s, -17 * s, 24 * s, -7 * s);
  setRgba(cr, band, 0.95);
  cairo_set_line_width(cr, 4.5 * s);
  cairo_stroke(cr);
  for (int k = 0; k < 3; ++k)
  {
    circle(cr, (-13 + k * 11) * s, 3 * s, 3 * s);
    setRgba(cr, rgb(255, 150, 60), 0.95);
    cairo_fill(cr);
  }
  for (int k = 0; k < 30; ++k)
  {
    const double a = k * 2 * kPi / 30;
    cairo_move_to(cr, std::cos(a) * 25 * s, std::sin(a) * 19 * s);
    cairo_line_to(cr, std::cos(a + 0.08) * 30 * s, std::sin(a + 0.08) * 23 * s);
  }
  setRgba(cr, furLight, 0.7);
  cairo_set_line_width(cr, 1.2);
  cairo_stroke(cr);
  cairo_restore(cr);
  // The near legs come out from under the cephalothorax.
  for (int i = 3; i >= 0; --i)
    drawLeg(i, true);
  // The cephalothorax: a big square-fronted head.
  cairo_save(cr);
  cairo_translate(cr, X(88), Y(cy));
  cairo_rotate(cr, tilt);
  cairo_scale(cr, 1.12, 1.12);
  cairo_move_to(cr, -24 * s, 6 * s);
  cairo_curve_to(cr, -26 * s, -14 * s, -10 * s, -24 * s, 10 * s, -24 * s);
  cairo_curve_to(cr, 22 * s, -24 * s, 28 * s, -14 * s, 28 * s, -2 * s);
  cairo_curve_to(cr, 28 * s, 10 * s, 22 * s, 16 * s, 6 * s, 16 * s);
  cairo_curve_to(cr, -12 * s, 16 * s, -22 * s, 14 * s, -24 * s, 6 * s);
  cairo_close_path(cr);
  chitin(cr, 0, -4 * s, 28 * s, fur);
  for (int k = 0; k < 14; ++k)
  {
    const double x = -18 + k * 3.2;
    cairo_move_to(cr, x * s, -22 * s);
    cairo_line_to(cr, (x + 1.5) * s, -27 * s);
  }
  setRgba(cr, furLight, 0.8);
  cairo_set_line_width(cr, 1.3);
  cairo_stroke(cr);
  cairo_move_to(cr, -18 * s, -14 * s);
  cairo_curve_to(cr, -6 * s, -20 * s, 6 * s, -20 * s, 14 * s, -16 * s);
  setRgba(cr, band, 0.85);
  cairo_set_line_width(cr, 3 * s);
  cairo_stroke(cr);
  // The eyes: two huge front ones, a small one beside, one up top.
  const double glint = crouch ? 1.0 : 0.8;
  for (int k = 0; k < 2; ++k)
  {
    const double ex = (k == 0 ? 15 : 26) * s, ey = -3 * s, er = (k == 0 ? 10.5 : 9.0) * s;
    circle(cr, ex, ey, er);
    cairo_pattern_t* g = cairo_pattern_create_radial(ex - er * 0.3, ey - er * 0.4, er * 0.1, ex, ey, er);
    stop(g, 0, rgb(90, 80, 104));
    stop(g, 0.5, rgb(18, 14, 24));
    stop(g, 1, rgb(4, 2, 6));
    cairo_set_source(cr, g);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(g);
    ink(cr, 1.6);
    circle(cr, ex - er * 0.32, ey - er * 0.35, er * 0.3);
    setRgba(cr, rgb(255, 255, 255), 0.95 * glint);
    cairo_fill(cr);
    circle(cr, ex + er * 0.3, ey + er * 0.35, er * 0.12);
    setRgba(cr, rgb(200, 230, 255), 0.6 * glint);
    cairo_fill(cr);
  }
  circle(cr, 2 * s, -7 * s, 3.4 * s);
  setColor(cr, rgb(10, 8, 12));
  cairo_fill(cr);
  circle(cr, 1 * s, -8 * s, 1 * s);
  setRgba(cr, rgb(255, 255, 255), 0.8);
  cairo_fill(cr);
  circle(cr, -7 * s, -16 * s, 2.4 * s);
  setColor(cr, rgb(10, 8, 12));
  cairo_fill(cr);
  // The chelicerae: iridescent teal; the white fuzzy palps.
  for (int k = 0; k < 2; ++k)
  {
    ellipse(cr, (17 + k * 6) * s, 12 * s, 4.5 * s, 6.5 * s, -0.2);
    cairo_pattern_t* g = cairo_pattern_create_linear(13 * s, 6 * s, 27 * s, 18 * s);
    stop(g, 0, rgb(120, 255, 220));
    stop(g, 0.5, rgb(40, 160, 170));
    stop(g, 1, rgb(60, 60, 170));
    cairo_set_source(cr, g);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(g);
    ink(cr, 1.3);
  }
  const double palp = frame % 2 ? 2.0 : -1.0;
  for (int k = 0; k < 2; ++k)
  {
    ellipse(cr, (30 + k * 3) * s, (9 + palp * (k ? -1 : 1)) * s, 4.5 * s, 3.6 * s);
    setColor(cr, k ? band : darken(band, 0.15f));
    cairo_fill_preserve(cr);
    ink(cr, 1.2);
  }
  cairo_restore(cr);
  if (crouch)
    glow(cr, X(108), Y(cy - 4), 24 * s, rgb(255, 255, 255), 0.3);
}

// --- The Bombardier Beetle ---------------------------------------------------------

void bombardierBeetle(cairo_t* cr, double w, double h, int variant, int frame)
{
  const double s = std::min(w / 128.0, h / 96.0);
  const double bx = kM + (w - 128 * s) * 0.5, by = kM + h - 96 * s;
  auto X = [&](double x) { return bx + x * s; };
  auto Y = [&](double y) { return by + y * s; };
  const int swell = variant >= 1 && variant <= 3 ? variant : 0;
  const bool fired = variant == 4;
  const Color orange = rgb(222, 112, 36), elytra = rgb(34, 46, 92);
  const double tip = fired ? -0.16 : -0.04 * swell; // the rear tips up
  ellipse(cr, X(66), Y(95), 54 * s, 4 * s);
  setRgba(cr, rgb(0, 0, 0), 0.28);
  cairo_fill(cr);
  // Legs: three a side, far then near; walking frames shuffle them.
  const double hips[3] = {78, 66, 54};
  const double feet[3] = {104, 70, 34};
  for (int side = 0; side < 2; ++side)
  {
    const bool near = side == 1;
    for (int i = 0; i < 3; ++i)
    {
      const double step = (frame % 2 == (i + side) % 2) ? 4 : -2;
      const double hx = X(hips[i] + (near ? 0 : 3)), hy = Y(74);
      const double fx = X(feet[i] + step + (near ? 0 : 5)), fy = Y(95);
      leg(cr, hx, hy, (hx + fx) * 0.5 + (i == 1 ? 0 : (i == 0 ? 6 : -6)) * s, Y(70), fx, fy, (near ? 4.2 : 3.6) * s,
        near ? rgb(60, 36, 24) : rgb(40, 24, 16));
    }
    if (side == 0)
    {
      // The abdomen's tip under the elytra: glowing and swelling on the tell.
      const double ax = X(16 - swell * 2.5), ay = Y(70 - swell * 2);
      const double ar = (8 + swell * 3.5) * s;
      if (swell > 0 || fired)
        glow(cr, ax, ay, ar * 3.2, rgb(255, 170, 60), fired ? 0.4 : 0.25 + 0.15 * swell);
      ellipse(cr, ax, ay, ar * 1.2, ar);
      cairo_pattern_t* g = cairo_pattern_create_radial(ax, ay, 0, ax, ay, ar * 1.2);
      if (swell > 0)
      {
        stop(g, 0, rgb(255, 255, 220));
        stop(g, 0.45, rgb(255, 200, 80));
        stop(g, 1, rgb(230, 90, 30));
      }
      else
      {
        stop(g, 0, rgb(250, 150, 70));
        stop(g, 1, rgb(160, 70, 30));
      }
      cairo_set_source(cr, g);
      cairo_fill_preserve(cr);
      cairo_pattern_destroy(g);
      ink(cr, 1.6);
      if (fired)
        for (int k = 0; k < 3; ++k)
        {
          circle(cr, ax - (8 + k * 9) * s, ay - (4 + k * 6) * s - (frame % 2) * 2, (5 - k) * s);
          setRgba(cr, rgb(250, 246, 236), 0.7 - k * 0.18);
          cairo_fill(cr);
        }
      // The elytra: a domed shell, blue-black with orange shoulder flashes.
      cairo_save(cr);
      cairo_translate(cr, X(78), Y(74));
      cairo_rotate(cr, tip);
      cairo_move_to(cr, 8 * s, 0);
      cairo_curve_to(cr, 8 * s, -28 * s, -16 * s, -40 * s, -38 * s, -36 * s);
      cairo_curve_to(cr, -58 * s, -32 * s, -66 * s, -14 * s, -64 * s, 0);
      cairo_close_path(cr);
      chitin(cr, -26 * s, -20 * s, 38 * s, elytra);
      // The seam and ridges.
      cairo_move_to(cr, 6 * s, -18 * s);
      cairo_curve_to(cr, -14 * s, -30 * s, -40 * s, -28 * s, -60 * s, -8 * s);
      setRgba(cr, rgb(10, 14, 30), 0.8);
      cairo_set_line_width(cr, 1.6);
      cairo_stroke(cr);
      for (int k = 0; k < 3; ++k)
      {
        cairo_move_to(cr, (2 - k * 4) * s, (-6 - k * 3) * s);
        cairo_curve_to(cr, (-16 - k * 3) * s, (-22 - k * 4) * s, (-40 - k * 2) * s, (-20 - k * 4) * s, (-58 + k * 2) * s, (-4 - k * 2) * s);
      }
      setRgba(cr, rgb(110, 140, 230), 0.35);
      cairo_set_line_width(cr, 1.0);
      cairo_stroke(cr);
      shine(cr, -30 * s, -30 * s, 14 * s, 4 * s, -0.15, 0.55);
      // The rim lighting blue-green.
      cairo_move_to(cr, -62 * s, -6 * s);
      cairo_curve_to(cr, -60 * s, -24 * s, -50 * s, -32 * s, -38 * s, -35 * s);
      setRgba(cr, rgb(120, 255, 210), 0.45);
      cairo_set_line_width(cr, 1.6);
      cairo_stroke(cr);
      cairo_restore(cr);
    }
  }
  // The pronotum and head: bright orange.
  ellipse(cr, X(94), Y(62), 15 * s, 15 * s);
  chitin(cr, X(94), Y(62), 15 * s, orange);
  ellipse(cr, X(112), Y(64), 10 * s, 9 * s);
  chitin(cr, X(112), Y(64), 10 * s, orange);
  shine(cr, X(90), Y(54), 5 * s, 2.6 * s, -0.4);
  // Eye and mandibles.
  circle(cr, X(116), Y(60), 3.2 * s);
  setColor(cr, rgb(12, 8, 6));
  cairo_fill(cr);
  shine(cr, X(115), Y(59), 1 * s, 1 * s, 0, 0.9);
  cairo_move_to(cr, X(120), Y(68));
  cairo_curve_to(cr, X(126), Y(68), X(128), Y(72), X(126), Y(74));
  setColor(cr, kInk);
  cairo_set_line_width(cr, 2.4);
  cairo_stroke(cr);
  // Antennae: beaded, waving forward.
  const double wave = frame % 2 ? 3 : 0;
  for (int k = 0; k < 2; ++k)
  {
    const double ox = k * 4;
    cairo_move_to(cr, X(116 + ox * 0.3), Y(56));
    cairo_curve_to(cr, X(124 + ox), Y(44 - wave), X(130 + ox), Y(40), X(140 + ox), Y(38 + wave + ox));
    setColor(cr, kInk);
    cairo_set_line_width(cr, 3.0);
    cairo_stroke_preserve(cr);
    setColor(cr, k ? rgb(210, 110, 40) : rgb(160, 80, 30));
    cairo_set_line_width(cr, 1.6);
    cairo_stroke(cr);
  }
}

} // namespace

bool drawGardenArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame)
{
  (void)t;
  if (key == "soldier_ant")
    soldierAnt(cr, w, h, variant, frame);
  else if (key == "jumping_spider")
    jumpingSpider(cr, w, h, variant, frame);
  else if (key == "bombardier_beetle")
    bombardierBeetle(cr, w, h, variant, frame);
  else if (key == "duck_pith")
  {
    // A pith helmet over the item icon's duck (its head at about (38, 24)):
    // a khaki dome with a brim, a band round it, a vent knob on top.
    const double s = std::min(w, h) / 64.0;
    const double cx = kM + 37 * s, base = kM + 16 * s;
    cairo_save(cr);
    cairo_translate(cr, cx, base);
    cairo_rotate(cr, -0.12);
    ellipse(cr, 0, 0, 22 * s, 5 * s);
    cairo_pattern_t* g = cairo_pattern_create_linear(-22 * s, 0, 22 * s, 0);
    stop(g, 0, rgb(250, 236, 196));
    stop(g, 0.5, rgb(222, 202, 150));
    stop(g, 1, rgb(160, 136, 90));
    cairo_set_source(cr, g);
    cairo_fill_preserve(cr);
    ink(cr, 1.6);
    cairo_move_to(cr, -15 * s, -1 * s);
    cairo_curve_to(cr, -15 * s, -22 * s, 15 * s, -22 * s, 15 * s, -1 * s);
    cairo_close_path(cr);
    cairo_set_source(cr, g);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(g);
    ink(cr, 1.6);
    cairo_move_to(cr, -14.6 * s, -4 * s);
    cairo_curve_to(cr, -5 * s, -6.5 * s, 5 * s, -6.5 * s, 14.6 * s, -4 * s);
    setColor(cr, rgb(110, 80, 50));
    cairo_set_line_width(cr, 3.0 * s);
    cairo_stroke(cr);
    circle(cr, 0, -16.5 * s, 2.6 * s);
    setColor(cr, rgb(200, 178, 128));
    cairo_fill_preserve(cr);
    ink(cr, 1.0);
    shine(cr, -6 * s, -12 * s, 4 * s, 2 * s, -0.6, 0.6);
    cairo_restore(cr);
  }
  else
    return false;
  return true;
}

} // namespace gr
