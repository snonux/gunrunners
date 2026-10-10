// The things living on Station Zero's hull (Level 18) in the style of
// enemy_art.cpp: each drawn in screen pixels into a texture with a 32 px
// margin, the cell box at (32, 32) .. (32 + w, 32 + h), facing right.

#include "assets/enemy_art_hull.hpp"

#include <algorithm>
#include <cmath>

namespace gr
{

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr Color kInk = rgb(20, 18, 30);
constexpr double kLine = 2.4;
constexpr double kM = 32.0;
constexpr Color kSpikeGlow = rgb(255, 120, 210);

void setRgba(cairo_t* cr, Color c, double a)
{
  cairo_set_source_rgba(cr, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, a);
}

Color lighten3(Color c) { return lerpColor(c, rgb(255, 255, 255), 0.35f); }
Color darken3(Color c) { return lerpColor(c, rgb(0, 0, 0), 0.35f); }

void circle(cairo_t* cr, double x, double y, double r) { cairo_arc(cr, x, y, r, 0, 2 * kPi); }

void ellipse(cairo_t* cr, double x, double y, double rx, double ry)
{
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_scale(cr, rx, ry);
  cairo_new_sub_path(cr);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_restore(cr);
}

// Fills the current path with a gradient from (x0, y0) to (x1, y1) and
// outlines it.
void fillLinear(cairo_t* cr, double x0, double y0, double x1, double y1, Color a, Color b, double line = kLine)
{
  cairo_pattern_t* g = cairo_pattern_create_linear(x0, y0, x1, y1);
  cairo_pattern_add_color_stop_rgb(g, 0, redOf(a) / 255.0, greenOf(a) / 255.0, blueOf(a) / 255.0);
  cairo_pattern_add_color_stop_rgb(g, 1, redOf(b) / 255.0, greenOf(b) / 255.0, blueOf(b) / 255.0);
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  if (line > 0)
  {
    setColor(cr, kInk);
    cairo_set_line_width(cr, line);
    cairo_stroke(cr);
  }
  else
    cairo_new_path(cr);
}

void glow(cairo_t* cr, double x, double y, double r, Color c, double a)
{
  cairo_pattern_t* g = cairo_pattern_create_radial(x, y, 0, x, y, r);
  cairo_pattern_add_color_stop_rgba(g, 0, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, a);
  cairo_pattern_add_color_stop_rgba(g, 1, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, 0);
  cairo_set_source(cr, g);
  circle(cr, x, y, r);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
}

// --- Space Barnacle --------------------------------------------------------------

// The barnacle in its own frame: `len` along the surface it clings to, `dep`
// out from it, the surface along y = dep. state 0 closed, 1 the tell (a
// glowing crack), 2 open with its six spike sockets just emptied.
void barnacleLocal(cairo_t* cr, double len, double dep, int state, int frame)
{
  const Color shellLight = rgb(212, 198, 222), shellMid = rgb(150, 132, 166), shellDark = rgb(84, 68, 102);
  const double cx = len * 0.5, base = dep - 3;
  const double topY = dep * 0.2, rimW = len * 0.2;
  const double s = std::min(len / 128.0, dep / 96.0);
  // The crusty foot it is cemented down with, and two baby barnacles.
  cairo_move_to(cr, len * 0.04, dep);
  for (int k = 0; k <= 10; ++k)
  {
    const double x = len * (0.04 + 0.092 * k);
    cairo_line_to(cr, x, base - 5 * s - ((k * 7) % 3) * 2.5 * s);
  }
  cairo_line_to(cr, len * 0.96, dep);
  cairo_close_path(cr);
  fillLinear(cr, 0, base - 12 * s, 0, dep, shellMid, shellDark, 2.0);
  for (const double bx : {0.11, 0.86})
  {
    cairo_move_to(cr, len * bx - 8 * s, base);
    cairo_line_to(cr, len * bx - 4 * s, base - 9 * s);
    cairo_line_to(cr, len * bx + 4 * s, base - 9 * s);
    cairo_line_to(cr, len * bx + 8 * s, base);
    cairo_close_path(cr);
    fillLinear(cr, 0, base - 9 * s, 0, base, shellLight, shellMid, 1.6);
  }
  // The shell: a volcano of five wall plates.
  const double bl = len * 0.13, br = len * 0.87;
  const double tl = cx - rimW, tr = cx + rimW;
  cairo_move_to(cr, bl, base);
  cairo_curve_to(cr, bl + len * 0.02, dep * 0.6, tl - len * 0.06, topY + dep * 0.12, tl, topY);
  cairo_line_to(cr, tr, topY);
  cairo_curve_to(cr, tr + len * 0.06, topY + dep * 0.12, br - len * 0.02, dep * 0.6, br, base);
  cairo_close_path(cr);
  fillLinear(cr, bl, topY, br, base, shellLight, shellDark, kLine);
  // Plate seams, bowing out with the cone.
  for (int k = 1; k < 5; ++k)
  {
    const double f = k / 5.0;
    const double xb = bl + (br - bl) * f, xt = tl + (tr - tl) * f;
    cairo_move_to(cr, xt, topY + 2);
    cairo_curve_to(cr, xt + (xt - cx) * 0.2, topY + dep * 0.3, xb + (xb - cx) * 0.05, dep * 0.7, xb, base - 1);
  }
  setRgba(cr, shellDark, 0.85);
  cairo_set_line_width(cr, 2.0);
  cairo_stroke(cr);
  // Growth ridges.
  for (int k = 1; k <= 4; ++k)
  {
    const double f = k / 5.0, y = topY + (base - topY) * f;
    const double half = rimW + (len * 0.37 - rimW) * std::pow(f, 0.8);
    cairo_move_to(cr, cx - half, y);
    cairo_curve_to(cr, cx - half * 0.5, y + 4 * s, cx + half * 0.5, y + 4 * s, cx + half, y);
  }
  setRgba(cr, rgb(250, 244, 255), 0.35);
  cairo_set_line_width(cr, 1.6);
  cairo_stroke(cr);
  // Crust: pale specks and a highlight down the lit side.
  for (int k = 0; k < 16; ++k)
  {
    const double f = 0.15 + 0.8 * double((k * 37) % 100) / 100.0;
    const double y = topY + (base - topY) * f;
    const double half = rimW + (len * 0.37 - rimW) * f;
    const double x = cx + half * (double((k * 53) % 100) / 50.0 - 1.0) * 0.85;
    circle(cr, x, y, (1.0 + (k % 3)) * s);
    setRgba(cr, k % 2 ? rgb(236, 228, 240) : shellDark, 0.6);
    cairo_fill(cr);
  }
  cairo_move_to(cr, tl + 4 * s, topY + 6 * s);
  cairo_curve_to(cr, tl - len * 0.04, topY + dep * 0.2, bl + len * 0.06, dep * 0.5, bl + len * 0.05, base - 10 * s);
  setRgba(cr, rgb(255, 255, 255), 0.4);
  cairo_set_line_width(cr, 3.0 * s);
  cairo_stroke(cr);
  // Six spike sockets round the shell; they hold spikes until it fires.
  const double sockets[6][2] = {{0.27, 0.62}, {0.37, 0.38}, {0.45, 0.68}, {0.55, 0.68}, {0.63, 0.38}, {0.73, 0.62}};
  for (int k = 0; k < 6; ++k)
  {
    const double x = len * sockets[k][0], y = topY + (base - topY) * sockets[k][1];
    ellipse(cr, x, y, 4.2 * s, 3.4 * s);
    setColor(cr, rgb(40, 28, 52));
    cairo_fill_preserve(cr);
    setRgba(cr, shellLight, 0.8);
    cairo_set_line_width(cr, 1.2);
    cairo_stroke(cr);
    const double out = std::atan2(y - dep * 0.9, x - cx);
    if (state < 2)
    {
      // A dark needle tip poking out.
      cairo_move_to(cr, x + std::cos(out + 1.4) * 2.4 * s, y + std::sin(out + 1.4) * 2.4 * s);
      cairo_line_to(cr, x + std::cos(out) * 11 * s, y + std::sin(out) * 11 * s);
      cairo_line_to(cr, x + std::cos(out - 1.4) * 2.4 * s, y + std::sin(out - 1.4) * 2.4 * s);
      cairo_close_path(cr);
      setColor(cr, rgb(52, 40, 66));
      cairo_fill_preserve(cr);
      setColor(cr, kInk);
      cairo_set_line_width(cr, 1.0);
      cairo_stroke(cr);
      if (state == 1)
        glow(cr, x, y, 7 * s, kSpikeGlow, 0.7);
    }
    else
    {
      // Empty, still glowing, a wisp of vapour.
      glow(cr, x, y, 9 * s, kSpikeGlow, 0.55);
      circle(cr, x + std::cos(out) * 9 * s, y + std::sin(out) * 9 * s, 2.5 * s);
      setRgba(cr, rgb(255, 230, 250), 0.45);
      cairo_fill(cr);
    }
  }
  // The top: two beak plates over the opening.
  const double ry = dep * 0.075;
  if (state == 0)
  {
    ellipse(cr, cx, topY, rimW, ry);
    fillLinear(cr, 0, topY - ry, 0, topY + ry, shellMid, shellDark, 2.0);
    cairo_move_to(cr, cx - rimW * 0.8, topY);
    cairo_line_to(cr, cx + rimW * 0.8, topY);
    setColor(cr, kInk);
    cairo_set_line_width(cr, 2.0);
    cairo_stroke(cr);
  }
  else
  {
    const double open = state == 1 ? 0.25 : 1.0;
    const double pulse = state == 1 ? (frame % 2 ? 1.0 : 0.75) : 0.6;
    // The glowing inside.
    glow(cr, cx, topY, rimW * (1.3 + open), kSpikeGlow, 0.65 * pulse);
    ellipse(cr, cx, topY, rimW * 0.9, ry * (0.5 + open));
    setColor(cr, state == 1 ? rgb(255, 170, 230) : rgb(70, 20, 60));
    cairo_fill(cr);
    if (state == 2)
    {
      // Two eyes peering up out of the dark.
      for (int k = -1; k <= 1; k += 2)
      {
        circle(cr, cx + k * rimW * 0.32, topY, 3.6 * s);
        setColor(cr, rgb(255, 210, 245));
        cairo_fill(cr);
        circle(cr, cx + k * rimW * 0.32 + 0.8 * s, topY + 0.6 * s, 1.6 * s);
        setColor(cr, kInk);
        cairo_fill(cr);
      }
    }
    // The beak plates, tilted apart.
    for (int k = -1; k <= 1; k += 2)
    {
      cairo_save(cr);
      cairo_translate(cr, cx + k * rimW, topY);
      cairo_rotate(cr, -k * open * 0.9);
      cairo_move_to(cr, 0, 0);
      cairo_curve_to(cr, -k * rimW * 0.4, -ry * 2.2, -k * rimW * 0.9, -ry * 1.6, -k * rimW * 0.95, -ry * 0.2);
      cairo_close_path(cr);
      fillLinear(cr, 0, -ry * 2, 0, 0, shellLight, shellMid, 1.8);
      cairo_restore(cr);
    }
    // The crack: glowing seams running down the shell.
    if (state == 1)
    {
      for (int k = -1; k <= 1; k += 2)
      {
        cairo_move_to(cr, cx + k * rimW * 0.3, topY + 3);
        cairo_line_to(cr, cx + k * rimW * 0.6, topY + dep * 0.18);
        cairo_line_to(cr, cx + k * rimW * 0.4, topY + dep * 0.3);
        cairo_line_to(cr, cx + k * rimW * 0.8, topY + dep * 0.46);
      }
      setRgba(cr, kSpikeGlow, 0.5);
      cairo_set_line_width(cr, 6.0 * s);
      cairo_stroke_preserve(cr);
      setColor(cr, rgb(255, 236, 250));
      cairo_set_line_width(cr, 2.0 * s);
      cairo_stroke(cr);
    }
  }
}

// Space Barnacle. Variant = state + 3 * face: state 0 closed, 1 the tell, 2
// open; face 0 floor, 1 ceiling, 2 on a wall to its left, 3 on a wall to its
// right. On a wall the box is expected on its side (3 wide, 4 tall).
void spaceBarnacle(cairo_t* cr, double w, double h, int variant, int frame)
{
  const int state = std::clamp(variant % 3, 0, 2);
  const int face = std::clamp(variant / 3, 0, 3);
  const bool wall = face >= 2;
  const double len = wall ? h : w, dep = wall ? w : h;
  cairo_save(cr);
  switch (face)
  {
    case 0:
      cairo_translate(cr, kM, kM);
      break;
    case 1:
      cairo_translate(cr, kM + w, kM + h);
      cairo_rotate(cr, kPi);
      break;
    case 2: // the wall on its left: the surface along x = kM
      cairo_translate(cr, kM + dep, kM);
      cairo_rotate(cr, kPi * 0.5);
      break;
    default: // the wall on its right
      cairo_translate(cr, kM, kM + len);
      cairo_rotate(cr, -kPi * 0.5);
      break;
  }
  barnacleLocal(cr, len, dep, state, frame);
  cairo_restore(cr);
}

// --- EVA Ram ---------------------------------------------------------------------

// A curled ram's horn, a spiral of metal, round (x, y).
void ramHorn(cairo_t* cr, double x, double y, double r)
{
  cairo_new_path(cr);
  for (int i = 0; i <= 40; ++i)
  {
    const double t = i / 40.0, a = -kPi * 0.6 + t * kPi * 1.9, rr = r * (1.0 - 0.6 * t);
    const double px = x + std::cos(a) * rr, py = y + std::sin(a) * rr;
    if (i == 0)
      cairo_move_to(cr, px, py);
    else
      cairo_line_to(cr, px, py);
  }
  setColor(cr, kInk);
  cairo_set_line_width(cr, r * 0.62);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  cairo_stroke_preserve(cr);
  setColor(cr, rgb(196, 170, 120));
  cairo_set_line_width(cr, r * 0.46);
  cairo_stroke_preserve(cr);
  setRgba(cr, rgb(255, 240, 200), 0.7);
  cairo_set_line_width(cr, r * 0.12);
  cairo_stroke(cr);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
  // Ridges across the horn.
  for (int i = 2; i < 36; i += 5)
  {
    const double t = i / 40.0, a = -kPi * 0.6 + t * kPi * 1.9, rr = r * (1.0 - 0.6 * t);
    const double nx = std::cos(a), ny = std::sin(a), w = r * 0.22;
    cairo_move_to(cr, x + nx * (rr - w), y + ny * (rr - w));
    cairo_line_to(cr, x + nx * (rr + w), y + ny * (rr + w));
  }
  setRgba(cr, rgb(120, 96, 60), 0.8);
  cairo_set_line_width(cr, 1.4);
  cairo_stroke(cr);
}

// The thruster's flame out of the nozzle at (x, y), pointing left, `len` long.
void exhaust(cairo_t* cr, double x, double y, double len, double rad, double heat)
{
  glow(cr, x - len * 0.3, y, rad * 2.2 + len * 0.3, rgb(255, 150, 60), 0.35 + 0.3 * heat);
  for (int layer = 0; layer < 3; ++layer)
  {
    const double l = len * (1.0 - layer * 0.3), rr = rad * (1.0 - layer * 0.3);
    cairo_move_to(cr, x, y - rr);
    cairo_curve_to(cr, x - l * 0.4, y - rr * 1.1, x - l * 0.8, y - rr * 0.4, x - l, y);
    cairo_curve_to(cr, x - l * 0.8, y + rr * 0.4, x - l * 0.4, y + rr * 1.1, x, y + rr);
    cairo_close_path(cr);
    const Color c = layer == 0 ? rgb(255, 120, 40) : (layer == 1 ? rgb(255, 210, 90) : rgb(255, 255, 230));
    setRgba(cr, c, layer == 0 ? 0.75 : 0.9);
    cairo_fill(cr);
  }
}

// EVA Ram: a stubby hovering robot with a thruster bell at the back and a
// battering plate (ram's horns and all) at the front. Variant 0 hovering, 1
// the tell (the thruster flashing hot), 2 ramming (a long exhaust flame), 3
// coasting after a ram (the thruster sputtering).
void evaRam(cairo_t* cr, double w, double h, int variant, int frame)
{
  const double s = std::min(w, h) / 96.0;
  const double bob = variant == 0 ? (frame % 2 ? -2.0 : 1.0) * s : 0.0;
  const double cx = kM + w * 0.46, cy = kM + h * 0.48 + bob;
  const double bw = w * 0.6, bh = h * 0.56;
  const double nozzleX = cx - bw * 0.5 - 6 * s;
  cairo_save(cr);
  if (variant == 2)
  {
    // Leaning into the ram.
    cairo_translate(cr, cx, cy);
    cairo_rotate(cr, 0.08);
    cairo_translate(cr, -cx, -cy);
  }
  // The flame or the puffs behind it.
  if (variant == 0)
    exhaust(cr, nozzleX - 6 * s, cy, (12 + (frame % 2) * 4) * s, 6 * s, 0.0);
  else if (variant == 1)
  {
    const double heat = frame % 2 ? 1.0 : 0.55;
    glow(cr, nozzleX - 4 * s, cy, 34 * s * (0.8 + heat * 0.4), rgb(255, 120, 50), 0.7 * heat);
    exhaust(cr, nozzleX - 6 * s, cy, (8 + heat * 10) * s, 7 * s, heat);
  }
  else if (variant == 2)
  {
    exhaust(cr, nozzleX - 4 * s, cy, w * 0.2 + kM + (frame % 2) * 4 * s, 11 * s, 1.0);
    // Speed lines past it.
    for (int k = 0; k < 3; ++k)
    {
      const double y = cy - bh * 0.6 + k * bh * 0.6;
      cairo_move_to(cr, kM + w * 0.1 - k * 6, y);
      cairo_line_to(cr, kM + w * 0.35 - k * 6, y);
    }
    setRgba(cr, rgb(255, 255, 255), 0.6);
    cairo_set_line_width(cr, 2.0);
    cairo_stroke(cr);
  }
  else
  {
    if (frame % 2 == 0)
      exhaust(cr, nozzleX - 6 * s, cy, 7 * s, 4 * s, 0.0);
    for (int k = 0; k < 3; ++k)
    {
      const double px = nozzleX - (10 + k * 9 + (frame % 2) * 4) * s, py = cy + (k % 2 ? -4 : 3) * s;
      circle(cr, px, py, (4 + k * 1.6) * s);
      setRgba(cr, rgb(170, 176, 190), 0.55 - k * 0.12);
      cairo_fill(cr);
    }
  }
  // The thruster bell.
  cairo_move_to(cr, nozzleX + 10 * s, cy - 8 * s);
  cairo_line_to(cr, nozzleX - 4 * s, cy - 13 * s);
  cairo_line_to(cr, nozzleX - 4 * s, cy + 13 * s);
  cairo_line_to(cr, nozzleX + 10 * s, cy + 8 * s);
  cairo_close_path(cr);
  const Color bellHot = variant == 1 && frame % 2 ? rgb(255, 150, 80) : rgb(130, 136, 150);
  fillLinear(cr, 0, cy - 13 * s, 0, cy + 13 * s, lighten3(bellHot), darken3(bellHot));
  // Under-jets keeping it up.
  for (int k = -1; k <= 1; k += 2)
  {
    const double jx = cx + k * bw * 0.25, jy = cy + bh * 0.5;
    roundedRect(cr, jx - 5 * s, jy - 2 * s, 10 * s, 7 * s, 2 * s);
    fillLinear(cr, 0, jy, 0, jy + 6 * s, rgb(150, 156, 170), rgb(70, 76, 90), 1.6);
    if (variant != 3 || frame % 2)
    {
      cairo_move_to(cr, jx - 3 * s, jy + 5 * s);
      cairo_line_to(cr, jx, jy + (11 + (frame % 2) * 3) * s);
      cairo_line_to(cr, jx + 3 * s, jy + 5 * s);
      cairo_close_path(cr);
      setRgba(cr, rgb(140, 220, 255), 0.85);
      cairo_fill(cr);
    }
  }
  // The body: a stubby white capsule with an orange band.
  roundedRect(cr, cx - bw * 0.5, cy - bh * 0.5, bw, bh, bh * 0.42);
  fillLinear(cr, 0, cy - bh * 0.5, 0, cy + bh * 0.5, rgb(248, 250, 255), rgb(150, 160, 178));
  cairo_save(cr);
  roundedRect(cr, cx - bw * 0.5, cy - bh * 0.5, bw, bh, bh * 0.42);
  cairo_clip(cr);
  cairo_rectangle(cr, cx - bw * 0.5, cy + bh * 0.08, bw, bh * 0.16);
  setColor(cr, rgb(255, 140, 40));
  cairo_fill(cr);
  for (double x = cx - bw * 0.5; x < cx + bw * 0.5; x += 10 * s)
  {
    cairo_move_to(cr, x, cy + bh * 0.24);
    cairo_line_to(cr, x + 5 * s, cy + bh * 0.08);
  }
  setRgba(cr, rgb(40, 30, 30), 0.5);
  cairo_set_line_width(cr, 2.0 * s);
  cairo_stroke(cr);
  cairo_restore(cr);
  roundedRect(cr, cx - bw * 0.5, cy - bh * 0.5, bw, bh, bh * 0.42);
  setColor(cr, kInk);
  cairo_set_line_width(cr, kLine);
  cairo_stroke(cr);
  // Shine and a panel line.
  cairo_move_to(cr, cx - bw * 0.3, cy - bh * 0.36);
  cairo_line_to(cr, cx + bw * 0.15, cy - bh * 0.36);
  setRgba(cr, rgb(255, 255, 255), 0.85);
  cairo_set_line_width(cr, 3.0 * s);
  cairo_stroke(cr);
  // The visor: a cyan eye strip (red when it is about to ram).
  const Color eye = variant == 1 || variant == 2 ? rgb(255, 70, 60) : rgb(110, 230, 255);
  roundedRect(cr, cx + bw * 0.05, cy - bh * 0.28, bw * 0.34, bh * 0.24, bh * 0.1);
  fillLinear(cr, 0, cy - bh * 0.28, 0, cy - bh * 0.04, rgb(30, 36, 50), rgb(10, 12, 20), 2.0);
  glow(cr, cx + bw * 0.28, cy - bh * 0.16, 12 * s, eye, 0.7);
  roundedRect(cr, cx + bw * 0.2, cy - bh * 0.21, bw * 0.14, bh * 0.1, bh * 0.05);
  setColor(cr, eye);
  cairo_fill(cr);
  // A ram's horn curled on its side.
  ramHorn(cr, cx - bw * 0.05, cy - bh * 0.06, bh * 0.3);
  // The battering plate on two pistons.
  const double px = kM + w * 0.86;
  for (int k = -1; k <= 1; k += 2)
  {
    cairo_rectangle(cr, cx + bw * 0.42, cy + k * bh * 0.22 - 3 * s, px - cx - bw * 0.42 + 2, 6 * s);
    fillLinear(cr, 0, cy + k * bh * 0.22 - 3 * s, 0, cy + k * bh * 0.22 + 3 * s, rgb(220, 226, 236), rgb(110, 118, 134), 1.6);
  }
  roundedRect(cr, px, cy - bh * 0.62, w * 0.11, bh * 1.24, 4 * s);
  fillLinear(cr, px, 0, px + w * 0.11, 0, rgb(120, 128, 146), rgb(56, 62, 78));
  cairo_save(cr);
  roundedRect(cr, px, cy - bh * 0.62, w * 0.11, bh * 1.24, 4 * s);
  cairo_clip(cr);
  for (double y = cy - bh * 0.62; y < cy + bh * 0.62; y += 12 * s)
  {
    cairo_move_to(cr, px, y + 6 * s);
    cairo_line_to(cr, px + w * 0.11, y);
    cairo_line_to(cr, px + w * 0.11, y + 6 * s);
    cairo_line_to(cr, px, y + 12 * s);
    cairo_close_path(cr);
  }
  setRgba(cr, rgb(255, 196, 40), 0.85);
  cairo_fill(cr);
  cairo_restore(cr);
  roundedRect(cr, px, cy - bh * 0.62, w * 0.11, bh * 1.24, 4 * s);
  setColor(cr, kInk);
  cairo_set_line_width(cr, kLine);
  cairo_stroke(cr);
  if (variant == 2)
    glow(cr, px + w * 0.11, cy, 20 * s, rgb(255, 255, 255), 0.35);
  cairo_restore(cr);
}

// --- Rivet Mite ------------------------------------------------------------------

// One Rivet Mite: a little metallic tick, its six legs scuttling with the
// frame, pincers at the front for gripping rivets. Variant 1 glows green
// (the tell, and the Virus it carries).
void rivetMite(cairo_t* cr, double w, double h, int variant, int frame)
{
  const bool green = variant == 1;
  const double s = h / 32.0;
  const double cx = kM + w * 0.45, cy = kM + h * 0.42;
  const double rx = w * 0.32, ry = h * 0.34;
  if (green)
    glow(cr, cx, cy, w * 0.75, rgb(120, 255, 90), 0.5);
  // Legs: three a side, bent knees, the pairs swapping with the frame.
  for (int k = 0; k < 3; ++k)
  {
    const double lx = cx - rx * 0.6 + k * rx * 0.6;
    const double swing = ((k + frame) % 2 ? 1.0 : -1.0) * 4 * s;
    cairo_move_to(cr, lx, cy + ry * 0.4);
    cairo_line_to(cr, lx + swing - 5 * s, cy + ry * 0.4 - 5 * s);
    cairo_line_to(cr, lx + swing - 8 * s, kM + h - 1);
  }
  setColor(cr, kInk);
  cairo_set_line_width(cr, 3.4 * s);
  cairo_stroke_preserve(cr);
  setColor(cr, green ? rgb(140, 220, 110) : rgb(150, 160, 178));
  cairo_set_line_width(cr, 1.6 * s);
  cairo_stroke(cr);
  // Pincers: a little wrench mouth.
  const double hx = cx + rx * 1.05;
  for (int k = -1; k <= 1; k += 2)
  {
    cairo_move_to(cr, hx - 2 * s, cy + k * 2 * s);
    cairo_curve_to(cr, hx + 6 * s, cy + k * 7 * s, hx + 11 * s, cy + k * 4 * s, hx + 11 * s, cy + k * 1 * s);
  }
  setColor(cr, kInk);
  cairo_set_line_width(cr, 3.2 * s);
  cairo_stroke_preserve(cr);
  setColor(cr, rgb(220, 200, 150));
  cairo_set_line_width(cr, 1.4 * s);
  cairo_stroke(cr);
  // The head.
  ellipse(cr, hx - 2 * s, cy, 6 * s, 5 * s);
  fillLinear(cr, 0, cy - 5 * s, 0, cy + 5 * s, green ? rgb(120, 200, 100) : rgb(110, 118, 136),
    green ? rgb(40, 100, 40) : rgb(50, 56, 70), 1.8);
  // Antennae.
  cairo_move_to(cr, hx - 2 * s, cy - 4 * s);
  cairo_curve_to(cr, hx + 2 * s, cy - 12 * s, hx + 8 * s, cy - 12 * s, hx + 10 * s, cy - 10 * s);
  setColor(cr, kInk);
  cairo_set_line_width(cr, 1.4);
  cairo_stroke(cr);
  // The shell: segmented and shiny.
  ellipse(cr, cx, cy, rx, ry);
  if (green)
    fillLinear(cr, 0, cy - ry, 0, cy + ry, rgb(200, 255, 170), rgb(50, 140, 50));
  else
    fillLinear(cr, 0, cy - ry, 0, cy + ry, rgb(232, 238, 248), rgb(96, 104, 124));
  cairo_save(cr);
  ellipse(cr, cx, cy, rx, ry);
  cairo_clip(cr);
  for (int k = 1; k < 3; ++k)
  {
    const double x = cx - rx + k * rx * 0.66;
    cairo_move_to(cr, x, cy - ry);
    cairo_curve_to(cr, x + 3 * s, cy - ry * 0.3, x + 3 * s, cy + ry * 0.3, x, cy + ry);
  }
  setRgba(cr, green ? rgb(30, 90, 30) : rgb(60, 66, 84), 0.8);
  cairo_set_line_width(cr, 1.4);
  cairo_stroke(cr);
  cairo_restore(cr);
  ellipse(cr, cx - rx * 0.3, cy - ry * 0.45, rx * 0.4, ry * 0.18);
  setRgba(cr, rgb(255, 255, 255), 0.75);
  cairo_fill(cr);
  // A beady eye.
  circle(cr, hx, cy - 1.5 * s, 1.8 * s);
  setColor(cr, green ? rgb(200, 255, 140) : rgb(255, 80, 60));
  cairo_fill(cr);
}

} // namespace

bool drawHullArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame)
{
  (void)t;
  if (key == "space_barnacle")
    spaceBarnacle(cr, w, h, variant, frame);
  else if (key == "eva_ram")
    evaRam(cr, w, h, variant, frame);
  else if (key == "rivet_mites")
    rivetMite(cr, w, h, variant, frame);
  else
    return false;
  return true;
}

} // namespace gr
