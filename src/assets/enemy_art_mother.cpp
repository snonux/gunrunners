// Level 49, The Hive Mother (Episode 7, DEEP SPACE): the Hive Mother, her
// throne, the Egg Guards and the Spore Nurses, in the style of
// enemy_art.cpp: each drawn in screen pixels into a texture with a 32 px
// margin, the cell box at (32, 32) .. (32 + w, 32 + h), facing right.

#include "assets/enemy_art_alien.hpp"

#include "base/math.hpp"

#include <algorithm>
#include <cmath>
#include <map>

namespace gr
{

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr Color kInk = rgb(30, 14, 12);
constexpr double kLine = 3.0;
constexpr double kM = 32.0; // margin
constexpr Color kChitin = rgb(150, 78, 44);
constexpr Color kResin = rgb(255, 184, 96);
constexpr Color kCrown = rgb(170, 255, 90);
constexpr Color kSac = rgb(250, 120, 170);

Color lighten(Color c, float t) { return lerpColor(c, rgb(255, 255, 255), t); }
Color darken(Color c, float t) { return lerpColor(c, rgb(0, 0, 0), t); }

void circle(cairo_t* cr, double x, double y, double r) { cairo_arc(cr, x, y, r, 0, 2 * kPi); }

void ellipse(cairo_t* cr, double x, double y, double rx, double ry, double angle = 0.0)
{
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_rotate(cr, angle);
  cairo_scale(cr, rx, ry);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_restore(cr);
}

void setRgba(cairo_t* cr, Color c, double a)
{
  cairo_set_source_rgba(cr, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, a);
}

struct Box
{
  cairo_t* cr;
  const Theme& t;
  double w, h;
  int variant, frame;
};

// --- The Hive Mother ------------------------------------------------------------------------
// Her sprite is 24 x 20 cells: her body's 16 x 14 box sits in its bottom
// middle (cells 4..20, rows 6..20), her crown on top. Poses: 0 upright, 1
// bowed (crown low in front, laying), 2 breathing in (mouth wide, sacs
// swollen), 3 charging on all fours, 4 dazed against a wall, 5 asleep, 6
// dead.

struct MotherPose
{
  double abX, abY, abRx, abRy;     // the egg sac of an abdomen
  double thX, thY, thRx, thRy, thA; // the thorax and its tilt
  double hdX, hdY, hdR;             // the head
  double crownA;                    // the way the crown points (radians, -pi/2 up)
  int eyes;                         // 0 open, 1 shut, 2 crossed
  bool mouth;                       // wide open
  double sacs;                      // how swollen (1 normal)
  bool low;                         // on all fours
};

MotherPose motherPose(int pose)
{
  switch (pose)
  {
    case 1: return {7.5, 14.5, 5.8, 5.0, 14.0, 12.5, 3.2, 4.0, 0.9, 19.0, 13.0, 2.6, -0.35, 0, false, 1.0, false};
    case 2: return {7.5, 14.0, 5.8, 5.2, 13.5, 11.5, 3.2, 4.2, 0.5, 18.5, 10.5, 2.8, -1.2, 0, true, 1.7, false};
    case 3: return {8.0, 16.8, 6.0, 3.2, 14.0, 16.8, 3.6, 2.6, 1.4, 19.5, 16.5, 2.5, -0.1, 0, false, 0.9, true};
    case 4: return {8.0, 17.4, 6.0, 2.8, 14.0, 17.6, 3.6, 2.2, 1.5, 19.8, 18.2, 2.4, 0.2, 2, false, 0.9, true};
    case 5: return {9.0, 16.5, 6.4, 3.6, 15.0, 17.0, 3.2, 2.6, 1.3, 18.8, 17.6, 2.4, -0.3, 1, false, 0.8, true};
    case 6: return {9.0, 18.0, 6.8, 2.2, 15.0, 18.4, 3.4, 1.8, 1.5, 19.5, 18.8, 2.2, 0.4, 2, false, 0.5, true};
    default: return {7.5, 14.0, 5.8, 5.2, 12.8, 10.5, 3.0, 4.6, 0.15, 12.5, 6.4, 2.7, -kPi / 2, 0, false, 1.0, false};
  }
}

void hiveMother(const Box& b)
{
  cairo_t* cr = b.cr;
  const double u = b.w / 24.0; // px per cell
  auto X = [&](double c) { return kM + c * u; };
  auto Y = [&](double c) { return kM + c * u; };
  const MotherPose p = motherPose(b.variant);
  const bool dead = b.variant == 6;
  const Color chitin = dead ? rgb(110, 90, 80) : kChitin;
  const Color resin = dead ? rgb(170, 150, 120) : kResin;
  const double sway = (b.frame % 2 ? 0.15 : -0.15) * (p.low ? 0.4 : 1.0);
  const double floorY = Y(20.0);

  // Legs: three pairs, the far ones darker; jointed up high like a mantis.
  auto leg = [&](double fromX, double fromY, double kneeX, double kneeY, double footX, Color c) {
    strokeLimb(cr, {{X(fromX), Y(fromY)}, {X(kneeX), Y(kneeY)}, {X(footX), floorY - 4}}, u * 0.55, c, kInk, 2.4);
    circle(cr, X(kneeX), Y(kneeY), u * 0.4);
    fillOutline(cr, lighten(c, 0.2f), kInk, 1.8);
  };
  const double kneeUp = p.low ? 2.4 : 4.5;
  const double step = b.variant == 3 ? (b.frame % 2 ? 1.2 : -1.2) : 0.0;
  for (int pass = 0; pass < 2; ++pass)
  {
    const Color c = pass == 0 ? darken(chitin, 0.4f) : chitin;
    const double off = pass == 0 ? 0.8 : 0.0;
    leg(p.abX + 1.0, p.abY + 1.0, p.abX - 2.5 + off, p.abY - kneeUp * 0.6, p.abX - 3.8 + off - step, c);
    leg(p.thX - 1.0, p.thY + 1.5, p.thX - 2.0 + off, p.thY - kneeUp * 0.4, p.thX - 3.0 + off + step, c);
    leg(p.thX + 0.5, p.thY + 1.5, p.thX + 2.8 + off, p.thY - kneeUp * 0.3, p.thX + 4.5 + off - step, c);
  }

  // The abdomen: a vast translucent egg sac, eggs glowing in it.
  ellipse(cr, X(p.abX), Y(p.abY), p.abRx * u, p.abRy * u, -0.12);
  fillGradientOutline(cr, Y(p.abY - p.abRy), Y(p.abY + p.abRy), lighten(resin, 0.25f), darken(resin, 0.35f), kInk, kLine);
  cairo_save(cr);
  ellipse(cr, X(p.abX), Y(p.abY), p.abRx * u * 0.9, p.abRy * u * 0.86, -0.12);
  cairo_clip(cr);
  for (int k = 0; k < 9; ++k)
  {
    const double ex = p.abX - p.abRx * 0.7 + (k % 3) * p.abRx * 0.55 + (k / 3 % 2) * 0.6;
    const double ey = p.abY - p.abRy * 0.5 + (k / 3) * p.abRy * 0.45;
    ellipse(cr, X(ex), Y(ey), u * 0.9, u * 0.7, 0.3);
    fillOutline(cr, rgba(255, 245, 200, 200), rgba(140, 70, 30, 160), 1.6);
    if (!dead)
      radialGlow(cr, X(ex), Y(ey), u * 0.9, rgb(255, 230, 140), 0.25 + 0.15 * std::sin(b.frame * 0.9 + k));
  }
  // Chitin bands over it.
  for (int k = 0; k < 4; ++k)
  {
    const double bx = p.abX - p.abRx * 0.75 + k * p.abRx * 0.5;
    cairo_move_to(cr, X(bx), Y(p.abY - p.abRy * 1.1));
    cairo_curve_to(cr, X(bx - 0.9), Y(p.abY - p.abRy * 0.3), X(bx - 0.9), Y(p.abY + p.abRy * 0.3), X(bx), Y(p.abY + p.abRy * 1.1));
    cairo_set_line_width(cr, u * 0.5);
    setColor(cr, darken(chitin, 0.15f));
    cairo_stroke(cr);
  }
  cairo_restore(cr);
  ellipse(cr, X(p.abX), Y(p.abY), p.abRx * u, p.abRy * u, -0.12);
  cairo_set_line_width(cr, kLine);
  setColor(cr, kInk);
  cairo_stroke(cr);
  // Dazed: vents open in her back, glowing.
  if (b.variant == 4)
    for (int k = 0; k < 3; ++k)
    {
      ellipse(cr, X(p.abX - 2.5 + k * 2.4), Y(p.abY - p.abRy * 0.6), u * 0.8, u * 0.4);
      fillOutline(cr, rgb(255, 150, 200), kInk, 2.0);
      radialGlow(cr, X(p.abX - 2.5 + k * 2.4), Y(p.abY - p.abRy * 0.6), u * 1.6, rgb(255, 120, 190), 0.6);
    }

  // The sacs on her flanks: one under her tail, one at her throat.
  auto sac = [&](double cx, double cy) {
    const double r = 1.2 * p.sacs;
    ellipse(cr, X(cx), Y(cy - r * 0.6), u * r, u * r * 1.15);
    fillGradientOutline(cr, Y(cy - r * 1.8), Y(cy + r * 0.6), lighten(dead ? resin : kSac, 0.35f), darken(kSac, 0.25f), kInk, 2.4);
    for (int k = 0; k < 3; ++k)
    {
      cairo_move_to(cr, X(cx - r * 0.6 + k * r * 0.6), Y(cy - r * 1.4));
      cairo_line_to(cr, X(cx - r * 0.4 + k * r * 0.5), Y(cy + r * 0.1));
    }
    cairo_set_line_width(cr, 1.6);
    setRgba(cr, rgb(140, 30, 70), 0.6);
    cairo_stroke(cr);
  };
  sac(4.6, std::min(19.6, p.abY + p.abRy * 0.9));

  // The thorax, plated.
  ellipse(cr, X(p.thX), Y(p.thY), p.thRx * u, p.thRy * u, p.thA * 0.3 + sway * 0.1);
  fillGradientOutline(cr, Y(p.thY - p.thRy), Y(p.thY + p.thRy), lighten(chitin, 0.3f), darken(chitin, 0.3f), kInk, kLine);
  for (int k = -1; k <= 1; ++k)
  {
    cairo_move_to(cr, X(p.thX - p.thRx * 0.8), Y(p.thY + k * p.thRy * 0.4));
    cairo_line_to(cr, X(p.thX + p.thRx * 0.8), Y(p.thY + k * p.thRy * 0.4 - 0.3));
  }
  cairo_set_line_width(cr, 2.0);
  setColor(cr, darken(chitin, 0.45f));
  cairo_stroke(cr);

  // The neck to the head.
  strokeLimb(cr, {{X(p.thX + 0.6), Y(p.thY - p.thRy * 0.5)}, {X(p.hdX - 0.4), Y(p.hdY + 0.8)}}, u * 1.6, chitin, kInk, kLine);

  // Scythe arms, raised unless she is low.
  for (int pass = 0; pass < 2; ++pass)
  {
    const Color c = pass == 0 ? darken(chitin, 0.35f) : lighten(chitin, 0.1f);
    const double o = pass == 0 ? -0.6 : 0.0;
    const double sx = p.thX + 1.5 + o, sy = p.thY - p.thRy * 0.3;
    const double ex = p.low ? sx + 4.0 : sx + 3.0 + sway, ey = p.low ? sy + 1.5 : sy - 2.5;
    const double tx = p.low ? ex + 2.0 : ex + 1.5, ty = p.low ? floorY / u - 1.2 : ey + 4.0;
    strokeLimb(cr, {{X(sx), Y(sy)}, {X(ex), Y(ey)}}, u * 0.75, c, kInk, 2.4);
    cairo_move_to(cr, X(ex - 0.3), Y(ey - 0.3));
    cairo_curve_to(cr, X(ex + 1.6), Y(ey + 0.4), X(tx + 0.6), Y(ty - 1.6), X(tx), Y(ty));
    cairo_curve_to(cr, X(tx - 0.2), Y(ty - 1.6), X(ex + 0.6), Y(ey + 1.2), X(ex - 0.2), Y(ey + 0.6));
    cairo_close_path(cr);
    fillOutline(cr, lighten(c, 0.35f), kInk, 2.0);
  }

  // The throat sac hangs under her head.
  sac(std::min(20.5, p.hdX + 1.0), std::min(19.6, p.hdY + 3.6 + p.sacs));

  // The head: long and smooth, a fan of eyes, mandibles.
  const double hx = X(p.hdX), hy = Y(p.hdY), hr = p.hdR * u;
  const double face = p.crownA + kPi / 2; // her face turns with her crown
  cairo_save(cr);
  cairo_translate(cr, hx, hy);
  cairo_rotate(cr, p.low ? 0.2 : (b.variant == 1 ? 0.5 : (b.variant == 2 ? 0.25 : 0.0)));
  // Mandibles.
  const double gape = p.mouth ? 0.7 : 0.25;
  for (int k = -1; k <= 1; k += 2)
  {
    cairo_move_to(cr, hr * 0.5, k * hr * 0.25);
    cairo_curve_to(cr, hr * 1.2, k * hr * (0.3 + gape), hr * 1.6, k * hr * (0.2 + gape * 0.6), hr * 1.7, k * hr * 0.05);
    cairo_curve_to(cr, hr * 1.3, k * hr * (0.15 + gape * 0.4), hr * 1.0, k * hr * 0.2, hr * 0.5, k * hr * 0.05);
    cairo_close_path(cr);
    fillOutline(cr, lighten(chitin, 0.4f), kInk, 2.0);
  }
  if (p.mouth)
  {
    ellipse(cr, hr * 0.85, 0, hr * 0.55, hr * 0.5);
    fillOutline(cr, rgb(60, 10, 30), kInk, 2.0);
    radialGlow(cr, hr * 0.85, 0, hr * 0.6, rgb(255, 120, 190), 0.5);
  }
  ellipse(cr, 0, 0, hr * 1.05, hr * 0.8);
  fillGradientOutline(cr, -hr * 0.8, hr * 0.8, lighten(chitin, 0.35f), darken(chitin, 0.2f), kInk, kLine);
  // Eyes: a fan of green beads.
  for (int k = 0; k < 5; ++k)
  {
    const double ex = hr * (0.1 + 0.16 * k), ey = -hr * (0.35 - 0.06 * k);
    if (p.eyes == 0)
    {
      circle(cr, ex, ey, hr * 0.13);
      fillOutline(cr, dead ? rgb(120, 120, 100) : rgb(200, 255, 120), kInk, 1.4);
      circle(cr, ex + hr * 0.03, ey - hr * 0.03, hr * 0.04);
      setColor(cr, rgb(255, 255, 255));
      cairo_fill(cr);
    }
    else if (p.eyes == 1)
    {
      cairo_move_to(cr, ex - hr * 0.1, ey);
      cairo_line_to(cr, ex + hr * 0.1, ey);
      cairo_set_line_width(cr, 2.4);
      setColor(cr, kInk);
      cairo_stroke(cr);
    }
    else if (k % 2 == 0)
    {
      for (int s = -1; s <= 1; s += 2)
      {
        cairo_move_to(cr, ex - hr * 0.1, ey - s * hr * 0.1);
        cairo_line_to(cr, ex + hr * 0.1, ey + s * hr * 0.1);
      }
      cairo_set_line_width(cr, 2.4);
      setColor(cr, kInk);
      cairo_stroke(cr);
    }
  }
  cairo_restore(cr);

  // The crown: crystal spikes from the back of her head, pointing the
  // way her head does.
  const double cx = hx + std::cos(p.crownA) * hr * 0.75, cy = hy + std::sin(p.crownA) * hr * 0.75;
  const double crownLen = (b.variant == 0 ? 3.6 : 2.6) * u;
  if (!dead && b.variant != 5)
    radialGlow(cr, cx, cy, crownLen * 1.1, kCrown, 0.35);
  for (int k = -2; k <= 2; ++k)
  {
    const double a = p.crownA + k * 0.32;
    const double len = crownLen * (1.0 - std::abs(k) * 0.18);
    const double s = std::sin(a), c = std::cos(a);
    const double bx = cx - s * k * u * 0.5, by = cy + c * k * u * 0.5;
    cairo_move_to(cr, bx - s * u * 0.45, by + c * u * 0.45);
    cairo_line_to(cr, bx + c * len, by + s * len);
    cairo_line_to(cr, bx + s * u * 0.45, by - c * u * 0.45);
    cairo_close_path(cr);
    fillGradientOutline(cr, by - len, by + u, dead ? rgb(160, 160, 150) : lighten(kCrown, 0.5f),
      dead ? rgb(90, 90, 80) : darken(kCrown, 0.3f), kInk, 2.0);
  }
  (void)face;

  // Asleep: a slow breath of Z's; dead: drips of resin.
  if (b.variant == 5)
    for (int k = 0; k < 3; ++k)
    {
      const double zx = hx + u * (1.0 + k * 0.9), zy = hy - u * (2.0 + k * 1.2 + (b.frame % 2) * 0.3);
      const double s = u * (0.35 + k * 0.12);
      cairo_move_to(cr, zx - s, zy - s);
      cairo_line_to(cr, zx + s, zy - s);
      cairo_line_to(cr, zx - s, zy + s);
      cairo_line_to(cr, zx + s, zy + s);
      cairo_set_line_width(cr, 3.0);
      setColor(cr, rgb(255, 230, 180));
      cairo_stroke(cr);
    }
}

// The throne: a mound of resin and eggs, a spire of resin either side.
void motherThrone(const Box& b)
{
  cairo_t* cr = b.cr;
  const double x0 = kM, y0 = kM, w = b.w, h = b.h;
  for (int k = 0; k < 2; ++k)
  {
    const double sx = k == 0 ? x0 + w * 0.04 : x0 + w * 0.96;
    cairo_move_to(cr, sx - w * 0.05, y0 + h);
    cairo_curve_to(cr, sx - w * 0.03, y0 + h * 0.4, sx - w * 0.01, y0 + h * 0.1, sx, y0);
    cairo_curve_to(cr, sx + w * 0.01, y0 + h * 0.1, sx + w * 0.03, y0 + h * 0.4, sx + w * 0.05, y0 + h);
    cairo_close_path(cr);
    fillGradientOutline(cr, y0, y0 + h, lighten(kResin, 0.2f), darken(kResin, 0.45f), kInk, kLine);
  }
  cairo_move_to(cr, x0, y0 + h);
  cairo_curve_to(cr, x0 + w * 0.1, y0 + h * 0.55, x0 + w * 0.3, y0 + h * 0.62, x0 + w * 0.5, y0 + h * 0.6);
  cairo_curve_to(cr, x0 + w * 0.7, y0 + h * 0.62, x0 + w * 0.9, y0 + h * 0.55, x0 + w, y0 + h);
  cairo_close_path(cr);
  fillGradientOutline(cr, y0 + h * 0.55, y0 + h, kResin, darken(kResin, 0.5f), kInk, kLine);
  for (int k = 0; k < 11; ++k)
  {
    const double ex = x0 + w * (0.08 + 0.084 * k), ey = y0 + h * (0.8 + 0.08 * (k % 2));
    ellipse(cr, ex, ey, 14, 18, 0.2 * (k % 3 - 1));
    fillOutline(cr, rgb(255, 240, 200), rgb(120, 60, 30), 1.8);
    radialGlow(cr, ex, ey, 16, rgb(255, 220, 120), 0.3 + 0.2 * std::sin(b.frame * 0.8 + k));
  }
}

// --- Her brood ------------------------------------------------------------------------------

// An Egg Guard: 0 a leathery egg, veins pulsing; 1 the hatchling, a horned
// beetle on six legs; 2 horns down for the charge.
void eggGuard(const Box& b)
{
  cairo_t* cr = b.cr;
  const double x0 = kM, y0 = kM, w = b.w, h = b.h;
  if (b.variant == 0)
  {
    const double wob = std::sin(b.frame * 1.7) * 0.06;
    ellipse(cr, x0 + w * 0.5, y0 + h * 0.56, w * 0.38, h * 0.44, wob);
    fillGradientOutline(cr, y0 + h * 0.12, y0 + h, rgb(255, 244, 214), rgb(210, 170, 120), kInk, kLine);
    for (int k = 0; k < 3; ++k)
    {
      cairo_move_to(cr, x0 + w * (0.3 + 0.2 * k), y0 + h * 0.2);
      cairo_curve_to(cr, x0 + w * (0.25 + 0.2 * k), y0 + h * 0.45, x0 + w * (0.38 + 0.2 * k), y0 + h * 0.6,
        x0 + w * (0.32 + 0.2 * k), y0 + h * 0.92);
    }
    cairo_set_line_width(cr, 2.0);
    setRgba(cr, rgb(120, 200, 60), 0.5 + 0.3 * std::sin(b.frame * 1.1));
    cairo_stroke(cr);
    radialGlow(cr, x0 + w * 0.5, y0 + h * 0.6, w * 0.3, rgb(190, 255, 90), 0.25);
    return;
  }
  const bool charge = b.variant == 2;
  const Color shell = rgb(200, 120, 60);
  const double step = b.frame % 2 ? 3.0 : -3.0;
  for (int k = 0; k < 3; ++k)
  {
    const double lx = x0 + w * (0.25 + 0.25 * k);
    strokeLimb(cr, {{lx, y0 + h * 0.6}, {lx + (k % 2 ? step : -step) - 4, y0 + h * 0.8}, {lx + (k % 2 ? -step : step), y0 + h - 2}},
      4.0, darken(shell, 0.4f), kInk, 1.4);
  }
  const double tilt = charge ? 0.25 : 0.0;
  ellipse(cr, x0 + w * 0.45, y0 + h * 0.5, w * 0.4, h * 0.3, tilt);
  fillGradientOutline(cr, y0 + h * 0.2, y0 + h * 0.8, lighten(shell, 0.3f), darken(shell, 0.3f), kInk, kLine);
  cairo_move_to(cr, x0 + w * 0.45, y0 + h * 0.22);
  cairo_line_to(cr, x0 + w * 0.45, y0 + h * 0.78);
  cairo_set_line_width(cr, 2.0);
  setColor(cr, kInk);
  cairo_stroke(cr);
  // The head and its horn.
  const double hx = x0 + w * 0.86, hy = y0 + h * (charge ? 0.62 : 0.48);
  circle(cr, hx, hy, w * 0.14);
  fillOutline(cr, darken(shell, 0.2f), kInk, 2.0);
  cairo_move_to(cr, hx + w * 0.05, hy - h * 0.06);
  if (charge)
    cairo_line_to(cr, hx + w * 0.38, hy + h * 0.02);
  else
    cairo_curve_to(cr, hx + w * 0.2, hy - h * 0.2, hx + w * 0.22, hy - h * 0.36, hx + w * 0.12, hy - h * 0.46);
  cairo_line_to(cr, hx + w * 0.1, hy + h * 0.04);
  cairo_close_path(cr);
  fillOutline(cr, rgb(255, 240, 200), kInk, 1.8);
  circle(cr, hx + w * 0.02, hy - h * 0.04, 3.2);
  setColor(cr, charge ? rgb(255, 70, 60) : rgb(190, 255, 90));
  cairo_fill(cr);
}

// A Spore Nurse: a floating jelly with a lantern of spores under it and
// trailing feelers.
void sporeNurse(const Box& b)
{
  cairo_t* cr = b.cr;
  const double x0 = kM, y0 = kM, w = b.w, h = b.h;
  const double cx = x0 + w * 0.5;
  const double pulse = std::sin(b.frame * 1.2);
  for (int k = 0; k < 4; ++k)
  {
    const double fx = cx - w * 0.3 + k * w * 0.2;
    cairo_move_to(cr, fx, y0 + h * 0.5);
    cairo_curve_to(cr, fx - 6 + pulse * 4, y0 + h * 0.75, fx + 6 - pulse * 4, y0 + h * 0.9, fx, y0 + h + 10);
  }
  cairo_set_line_width(cr, 3.0);
  setColor(cr, rgb(170, 230, 160));
  cairo_stroke(cr);
  radialGlow(cr, cx, y0 + h * 0.7, w * 0.45, rgb(170, 255, 100), 0.45 + 0.2 * pulse);
  circle(cr, cx, y0 + h * 0.7, w * 0.14);
  fillOutline(cr, rgb(220, 255, 150), kInk, 1.8);
  cairo_move_to(cr, x0 + w * 0.06, y0 + h * 0.52);
  cairo_curve_to(cr, x0 + w * 0.06, y0 + h * (0.02 - 0.04 * pulse), x0 + w * 0.94, y0 + h * (0.02 - 0.04 * pulse),
    x0 + w * 0.94, y0 + h * 0.52);
  cairo_curve_to(cr, x0 + w * 0.7, y0 + h * 0.44, x0 + w * 0.3, y0 + h * 0.44, x0 + w * 0.06, y0 + h * 0.52);
  cairo_close_path(cr);
  fillGradientOutline(cr, y0, y0 + h * 0.52, rgba(220, 255, 230, 230), rgba(110, 200, 150, 230), kInk, kLine);
  for (int k = 0; k < 3; ++k)
  {
    circle(cr, x0 + w * (0.3 + 0.2 * k), y0 + h * 0.28, 3.0);
    setColor(cr, rgb(255, 255, 255));
    cairo_fill(cr);
  }
}

using DrawFn = void (*)(const Box&);

} // namespace

bool drawMotherArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame)
{
  static const std::map<std::string, DrawFn> kRoutines{
    {"hive_mother", hiveMother},
    {"mother_throne", motherThrone},
    {"egg_guard", eggGuard},
    {"brood_egg", eggGuard},
    {"spore_nurse", sporeNurse},
    {"brood_nurse", sporeNurse},
  };
  const auto it = kRoutines.find(key);
  if (it == kRoutines.end())
    return false;
  it->second(Box{cr, t, w, h, variant, frame});
  return true;
}

} // namespace gr
