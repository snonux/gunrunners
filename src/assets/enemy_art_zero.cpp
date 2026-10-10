// ZERO's server cathedral (Level 21) in the style of enemy_art.cpp: the
// Lattice Turret and the Repair Swarm, each drawn in screen pixels into a
// texture with a 32 px margin, the cell box at (32, 32) .. (32 + w, 32 + h),
// facing right. Also the pieces the level and the cutscenes share: a server
// rack, ZERO's eye, Lance Marquee and his studio audience.

#include "assets/enemy_art_zero.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace gr
{

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr Color kInk = rgb(14, 12, 20);
constexpr double kLine = 2.4;
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

void ellipse(cairo_t* cr, double x, double y, double rx, double ry)
{
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_scale(cr, rx, ry);
  cairo_new_sub_path(cr);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_restore(cr);
}

void stop(cairo_pattern_t* g, double at, Color c, double a = 1.0)
{
  cairo_pattern_add_color_stop_rgba(g, at, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, a);
}

void fillLinear(cairo_t* cr, double x0, double y0, double x1, double y1, Color a, Color b, double line = kLine)
{
  cairo_pattern_t* g = cairo_pattern_create_linear(x0, y0, x1, y1);
  stop(g, 0, a);
  stop(g, 1, b);
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

void fillRadial(cairo_t* cr, double x, double y, double r, Color light, Color dark, double line = kLine)
{
  cairo_pattern_t* g = cairo_pattern_create_radial(x - r * 0.35, y - r * 0.4, r * 0.1, x, y, r);
  stop(g, 0, light);
  stop(g, 1, dark);
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
  stop(g, 0, c, a);
  stop(g, 1, c, 0);
  cairo_set_source(cr, g);
  cairo_arc(cr, x, y, r, 0, 2 * kPi);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
}

// A limb: an inked stroke with a coloured core through three points.
void limb(cairo_t* cr, double x0, double y0, double x1, double y1, double x2, double y2, double width, Color c,
  double line = kLine)
{
  cairo_move_to(cr, x0, y0);
  cairo_line_to(cr, x1, y1);
  cairo_line_to(cr, x2, y2);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
  setColor(cr, kInk);
  cairo_set_line_width(cr, width + 2 * line);
  cairo_stroke_preserve(cr);
  setColor(cr, c);
  cairo_set_line_width(cr, width);
  cairo_stroke(cr);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
  cairo_set_line_join(cr, CAIRO_LINE_JOIN_MITER);
}

double rnd(std::uint32_t& s)
{
  s ^= s << 13;
  s ^= s >> 17;
  s ^= s << 5;
  return double(s & 0xFFFF) / 65535.0;
}

void centred(cairo_t* cr, const char* s, double cx, double baseline, double size)
{
  selectGameFont(cr);
  cairo_set_font_size(cr, size);
  cairo_text_extents_t e;
  cairo_text_extents(cr, s, &e);
  cairo_move_to(cr, cx - e.width * 0.5 - e.x_bearing, baseline);
  cairo_show_text(cr, s);
}

// --- Lattice Turret ------------------------------------------------------------------------

// A gun hanging under its ceiling rail: a trolley with two wheels in the
// rail's channel, a triangular lattice frame of struts, and the gun pod at
// its point with the barrel down. The barrel's tip glows red for the tell.
void latticeTurret(cairo_t* cr, double w, double h, int variant, int frame)
{
  const double s = w / 128.0, cx = kM + w * 0.5, top = kM;
  const Color steel = rgb(92, 98, 116), steelLight = rgb(176, 182, 200), steelDark = rgb(36, 38, 48);
  const Color red = rgb(255, 50, 56);
  const bool tell = variant == 1;
  // The trolley.
  roundedRect(cr, kM + 14 * s, top, w - 28 * s, 16 * s, 4 * s);
  fillLinear(cr, 0, top, 0, top + 16 * s, steelLight, steelDark);
  for (const double wx : {kM + 30 * s, kM + w - 30 * s})
  {
    circle(cr, wx, top + 4 * s, 6 * s);
    fillRadial(cr, wx, top + 4 * s, 6 * s, steelLight, steelDark, 1.6);
    circle(cr, wx, top + 4 * s, 1.8 * s);
    setColor(cr, kInk);
    cairo_fill(cr);
  }
  // The lattice: two outer struts down to the pod and cross bracing.
  const double podY = top + h * 0.52;
  const double lx = kM + 18 * s, rx = kM + w - 18 * s, by = top + 16 * s;
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  for (int pass = 0; pass < 2; ++pass)
  {
    cairo_move_to(cr, lx, by);
    cairo_line_to(cr, cx - 14 * s, podY);
    cairo_move_to(cr, rx, by);
    cairo_line_to(cr, cx + 14 * s, podY);
    cairo_move_to(cr, lx, by + 2 * s);
    cairo_line_to(cr, rx, by + 2 * s);
    // Bracing zigzag.
    const int n = 4;
    for (int i = 0; i < n; ++i)
    {
      const double t0 = double(i) / n, t1 = double(i + 1) / n;
      const double ax = lx + (cx - 14 * s - lx) * t0, ay = by + (podY - by) * t0;
      const double bx = rx + (cx + 14 * s - rx) * t1, byy = by + (podY - by) * t1;
      const double ax1 = lx + (cx - 14 * s - lx) * t1, ay1 = by + (podY - by) * t1;
      cairo_move_to(cr, ax, ay);
      cairo_line_to(cr, bx, byy);
      cairo_move_to(cr, bx, byy);
      cairo_line_to(cr, ax1, ay1);
    }
    setColor(cr, pass == 0 ? kInk : steel);
    cairo_set_line_width(cr, pass == 0 ? 6.5 * s : 3.0 * s);
    cairo_stroke(cr);
  }
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
  // Little red lights at the lattice's joints, blinking in turn.
  for (int i = 0; i < 3; ++i)
  {
    const double t = (i + 0.5) / 3.0;
    const double jx = lx + (cx - 14 * s - lx) * t, jy = by + (podY - by) * t;
    const bool on = (frame + i) % 3 == 0 || tell;
    if (on)
      glow(cr, jx, jy, 7 * s, red, 0.7);
    circle(cr, jx, jy, 2.2 * s);
    setColor(cr, on ? lighten(red, 0.4f) : rgb(90, 20, 24));
    cairo_fill(cr);
  }
  // The gun pod: a rounded housing with a slit sensor.
  ellipse(cr, cx, podY + 4 * s, 22 * s, 16 * s);
  fillRadial(cr, cx, podY + 4 * s, 24 * s, steelLight, steelDark);
  roundedRect(cr, cx - 12 * s, podY - 2 * s, 24 * s, 5 * s, 2 * s);
  setColor(cr, rgb(20, 4, 8));
  cairo_fill(cr);
  cairo_rectangle(cr, cx - 10 * s + std::fmod(frame * 5.0, 16.0) * s, podY - 1 * s, 4 * s, 3 * s);
  setColor(cr, red);
  cairo_fill(cr);
  // The barrel, straight down, with cooling rings.
  const double bTop = podY + 14 * s, bBot = kM + h - 2 * s;
  cairo_rectangle(cr, cx - 6 * s, bTop, 12 * s, bBot - bTop);
  fillLinear(cr, cx - 6 * s, 0, cx + 6 * s, 0, steelLight, steelDark, 2.0);
  for (double y = bTop + 5 * s; y < bBot - 8 * s; y += 6 * s)
  {
    cairo_rectangle(cr, cx - 8 * s, y, 16 * s, 3 * s);
    fillLinear(cr, cx - 8 * s, 0, cx + 8 * s, 0, steel, steelDark, 1.2);
  }
  cairo_rectangle(cr, cx - 8 * s, bBot - 6 * s, 16 * s, 6 * s);
  fillLinear(cr, cx - 8 * s, 0, cx + 8 * s, 0, tell ? lighten(red, 0.3f) : steel, tell ? darken(red, 0.4f) : steelDark, 1.6);
  if (tell)
  {
    glow(cr, cx, bBot - 2 * s, 26 * s, red, 0.85);
    glow(cr, cx, bBot - 2 * s, 9 * s, rgb(255, 230, 220), 0.9);
  }
}

// --- Repair Swarm --------------------------------------------------------------------------

// A cloud of tiny nanobots: little domed drones with blue-white lights on a
// swirling path round the middle, a couple of welding sparks, a faint haze.
void repairSwarm(cairo_t* cr, double w, double h, int frame)
{
  const double cx = kM + w * 0.5, cy = kM + h * 0.5, s = w / 96.0;
  glow(cr, cx, cy, 48 * s, rgb(90, 200, 255), 0.22);
  std::uint32_t seed = 0x51ed27u;
  const double spin = frame * kPi / 8.0;
  for (int i = 0; i < 22; ++i)
  {
    const double rr = (10 + rnd(seed) * 34) * s;
    const double a = rnd(seed) * 2 * kPi + spin * (i % 2 ? 1.0 : -0.7) * (40 * s / std::max(rr, 1.0));
    const double x = cx + std::cos(a) * rr, y = cy + std::sin(a) * rr * 0.8;
    const double bs = (2.6 + rnd(seed) * 1.6) * s;
    // The drone: a dark body with two tiny wings and a light.
    cairo_move_to(cr, x - bs * 2.2, y - bs * 0.4);
    cairo_line_to(cr, x + bs * 2.2, y - bs * 0.4);
    setColor(cr, rgba(150, 170, 200, 200));
    cairo_set_line_width(cr, 1.2);
    cairo_stroke(cr);
    ellipse(cr, x, y, bs * 1.3, bs);
    fillRadial(cr, x, y, bs * 1.4, rgb(150, 160, 180), rgb(30, 34, 46), 1.0);
    circle(cr, x + bs * 0.3, y - bs * 0.2, bs * 0.45);
    setColor(cr, (i + frame) % 5 == 0 ? rgb(255, 255, 255) : rgb(120, 220, 255));
    cairo_fill(cr);
  }
  // Welding sparks.
  for (int k = 0; k < 3; ++k)
  {
    const double a = spin * 1.7 + k * 2.1;
    const double x = cx + std::cos(a) * 16 * s, y = cy + std::sin(a) * 12 * s;
    glow(cr, x, y, 9 * s, rgb(255, 220, 120), 0.8);
    for (int j = 0; j < 3; ++j)
    {
      const double b = a + j * 2.1 + frame;
      cairo_move_to(cr, x, y);
      cairo_line_to(cr, x + std::cos(b) * 7 * s, y + std::sin(b) * 7 * s);
    }
    setColor(cr, rgb(255, 240, 180));
    cairo_set_line_width(cr, 1.4);
    cairo_stroke(cr);
  }
}

// --- Echo (a fallback) -------------------------------------------------------------------

void echoHologram(cairo_t* cr, double w, double h, int frame)
{
  const double cx = kM + w * 0.5;
  const Color cyan = rgb(90, 240, 255);
  glow(cr, cx, kM + h * 0.5, h * 0.6, cyan, 0.3);
  circle(cr, cx, kM + h * 0.16, w * 0.22);
  roundedRect(cr, cx - w * 0.26, kM + h * 0.3, w * 0.52, h * 0.4, 8);
  roundedRect(cr, cx - w * 0.22, kM + h * 0.68, w * 0.16, h * 0.32, 5);
  roundedRect(cr, cx + w * 0.06, kM + h * 0.68, w * 0.16, h * 0.32, 5);
  setRgba(cr, cyan, 0.45);
  cairo_fill(cr);
  for (double y = kM + (frame % 2) * 2; y < kM + h; y += 4)
  {
    cairo_rectangle(cr, kM, y, w, 1.4);
    setRgba(cr, rgb(220, 255, 255), 0.25);
    cairo_fill(cr);
  }
}

} // namespace

bool drawZeroArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame)
{
  (void)t;
  if (key == "lattice_turret")
    latticeTurret(cr, w, h, variant, frame);
  else if (key == "repair_swarm")
    repairSwarm(cr, w, h, frame);
  else if (key == "echo")
    echoHologram(cr, w, h, frame);
  else
    return false;
  return true;
}

// --- Shared pieces --------------------------------------------------------------------------

void paintServerRack(cairo_t* cr, double w, double h, unsigned seed, bool lit, const char* label)
{
  std::uint32_t sd = seed * 2654435761u + 77u;
  if (sd == 0)
    sd = 1;
  const double rail = std::max(5.0, std::min(10.0, w * 0.07));
  // The cabinet.
  roundedRect(cr, 1, 1, w - 2, h - 2, 5);
  fillLinear(cr, 0, 0, w, 0, rgb(40, 42, 52), rgb(10, 10, 14), 2.4);
  // The top cap with its vent slots and an optional label.
  const double capH = std::min(22.0, h * 0.1);
  cairo_rectangle(cr, 4, 4, w - 8, capH);
  fillLinear(cr, 0, 4, 0, 4 + capH, rgb(60, 62, 74), rgb(18, 18, 24), 1.4);
  if (label != nullptr && label[0] != 0)
  {
    roundedRect(cr, w * 0.5 - 34, 6, 68, capH - 4, 2);
    setColor(cr, rgb(210, 206, 190));
    cairo_fill(cr);
    setColor(cr, rgb(30, 26, 22));
    centred(cr, label, w * 0.5, 6 + capH * 0.7, std::min(12.0, capH * 0.62));
  }
  else
    for (double x = 12; x < w - 12; x += 8)
    {
      cairo_rectangle(cr, x, 8, 4, capH - 8);
      setColor(cr, rgb(6, 6, 8));
      cairo_fill(cr);
    }
  // The units between the rails.
  const double x0 = 4 + rail, x1 = w - 4 - rail;
  const double uh = 16.0;
  for (double y = 6 + capH; y + uh < h - 12; y += uh)
  {
    const int kind = int(rnd(sd) * 6.0);
    cairo_rectangle(cr, x0, y + 1, x1 - x0, uh - 2);
    fillLinear(cr, 0, y, 0, y + uh, rgb(54, 56, 66), rgb(16, 16, 22), 0);
    cairo_rectangle(cr, x0, y + 1, x1 - x0, 1);
    setColor(cr, rgba(255, 255, 255, 34));
    cairo_fill(cr);
    const double uw = x1 - x0;
    if (kind == 0)
    {
      // Drive bays.
      for (double bx = x0 + 4; bx < x0 + uw * 0.62; bx += 9)
      {
        roundedRect(cr, bx, y + 3, 7, uh - 6, 1);
        setColor(cr, rgb(10, 10, 14));
        cairo_fill(cr);
        cairo_rectangle(cr, bx + 1, y + uh - 6, 5, 1.2);
        setColor(cr, rgb(110, 114, 126));
        cairo_fill(cr);
      }
    }
    else if (kind == 1)
    {
      // A vent grille.
      for (double bx = x0 + 4; bx < x0 + uw * 0.6; bx += 4)
      {
        cairo_rectangle(cr, bx, y + 4, 2, uh - 8);
        setColor(cr, rgb(6, 6, 8));
        cairo_fill(cr);
      }
    }
    else if (kind == 2)
    {
      // A patch panel.
      for (double bx = x0 + 4; bx < x0 + uw * 0.7; bx += 6)
        for (const double dy : {3.0, 9.0})
        {
          cairo_rectangle(cr, bx, y + dy, 4, 4);
          setColor(cr, rgb(4, 4, 6));
          cairo_fill(cr);
        }
    }
    else
    {
      // A blank plate with a handle.
      roundedRect(cr, x0 + 6, y + 5, 14, uh - 10, 2);
      setColor(cr, rgb(90, 94, 106));
      cairo_set_line_width(cr, 1.4);
      cairo_stroke(cr);
    }
    // Status lights.
    for (int i = 0; i < 3; ++i)
    {
      const double lx = x1 - 8 - i * 7, ly = y + uh * 0.5;
      const double pick = rnd(sd);
      const Color c = pick < 0.5 ? rgb(255, 50, 60) : (pick < 0.85 ? rgb(70, 255, 120) : rgb(255, 180, 60));
      if (lit && pick > 0.2)
      {
        glow(cr, lx, ly, 5, c, 0.5);
        circle(cr, lx, ly, 1.7);
        setColor(cr, lighten(c, 0.3f));
      }
      else
      {
        circle(cr, lx, ly, 1.7);
        setColor(cr, darken(c, 0.7f));
      }
      cairo_fill(cr);
    }
  }
  // The rails.
  for (const double x : {4.0, w - 4 - rail})
  {
    cairo_rectangle(cr, x, 4 + capH, rail, h - 8 - capH);
    fillLinear(cr, x, 0, x + rail, 0, rgb(84, 88, 100), rgb(22, 22, 28), 1.2);
    for (double y = 8 + capH; y < h - 10; y += 8)
    {
      cairo_rectangle(cr, x + rail * 0.3, y, rail * 0.4, 3);
      setColor(cr, rgb(4, 4, 6));
      cairo_fill(cr);
    }
  }
  // The kick plate and a glass door's sheen.
  cairo_rectangle(cr, 4, h - 12, w - 8, 8);
  fillLinear(cr, 0, h - 12, 0, h - 4, rgb(50, 52, 62), rgb(14, 14, 18), 1.2);
  cairo_move_to(cr, w * 0.15, 4 + capH);
  cairo_line_to(cr, w * 0.38, 4 + capH);
  cairo_line_to(cr, w * 0.12, h * 0.5);
  cairo_line_to(cr, 6, h * 0.5);
  cairo_line_to(cr, 6, 4 + capH + w * 0.1);
  cairo_close_path(cr);
  setRgba(cr, rgb(255, 255, 255), 0.05);
  cairo_fill(cr);
}

void paintZeroEye(cairo_t* cr, double cx, double cy, double rad, double pupil, double shutter, double light, double ring)
{
  light = std::clamp(light, 0.0, 1.0);
  // The housing: a black square bezel, bolted, rounded at the corners.
  roundedRect(cr, cx - rad, cy - rad, rad * 2, rad * 2, rad * 0.22);
  fillLinear(cr, cx - rad, cy - rad, cx + rad, cy + rad, rgb(58, 60, 72), rgb(10, 10, 14), 3.0);
  for (int sx : {-1, 1})
    for (int sy : {-1, 1})
    {
      const double bx = cx + sx * rad * 0.84, by = cy + sy * rad * 0.84;
      circle(cr, bx, by, rad * 0.04);
      fillRadial(cr, bx, by, rad * 0.04, rgb(190, 194, 206), rgb(50, 52, 62), 1.2);
    }
  // The tally light, top right, and the model plate along the bottom.
  {
    const double tx = cx + rad * 0.78, ty = cy - rad * 0.64;
    roundedRect(cr, tx - rad * 0.08, ty - rad * 0.06, rad * 0.16, rad * 0.12, rad * 0.03);
    setColor(cr, rgb(20, 20, 26));
    cairo_fill(cr);
    if (light > 0.3)
      glow(cr, tx, ty, rad * 0.16, rgb(255, 30, 40), 0.8 * light);
    ellipse(cr, tx, ty, rad * 0.055, rad * 0.04);
    setColor(cr, lerpColor(rgb(70, 10, 14), rgb(255, 90, 90), float(light)));
    cairo_fill(cr);
    setColor(cr, rgb(150, 154, 168));
    centred(cr, "ZERO-1  1:1.2  f=42mm", cx, cy + rad * 0.95, rad * 0.06);
  }
  // The barrel.
  const double rb = rad * 0.9;
  circle(cr, cx, cy, rb);
  fillRadial(cr, cx, cy, rb, rgb(70, 72, 84), rgb(6, 6, 8), 3.0);
  // The knurled zoom ring.
  const double r0 = rad * 0.8, r1 = rad * 0.88;
  circle(cr, cx, cy, r1);
  setColor(cr, rgb(18, 18, 22));
  cairo_fill(cr);
  for (int k = 0; k < 120; ++k)
  {
    const double a = ring + k * 2 * kPi / 120;
    cairo_move_to(cr, cx + std::cos(a) * r0, cy + std::sin(a) * r0);
    cairo_line_to(cr, cx + std::cos(a) * r1, cy + std::sin(a) * r1);
  }
  setColor(cr, rgb(64, 66, 76));
  cairo_set_line_width(cr, 1.6);
  cairo_stroke(cr);
  // Focal lengths round the ring's inner edge.
  {
    static const char* kMarks[] = {"24", "35", "50", "85", "135", "200"};
    selectGameFont(cr);
    cairo_set_font_size(cr, rad * 0.055);
    for (int k = 0; k < 6; ++k)
    {
      const double a = ring * 0.5 - kPi * 0.5 + (k - 2.5) * 0.22;
      cairo_save(cr);
      cairo_translate(cr, cx + std::cos(a) * rad * 0.75, cy + std::sin(a) * rad * 0.75);
      cairo_rotate(cr, a + kPi * 0.5);
      cairo_text_extents_t e;
      cairo_text_extents(cr, kMarks[k], &e);
      cairo_move_to(cr, -e.width * 0.5, e.height * 0.5);
      setColor(cr, k == 2 ? rgb(255, 210, 60) : rgb(220, 222, 230));
      cairo_show_text(cr, kMarks[k]);
      cairo_restore(cr);
    }
  }
  // The inner rim and the glass.
  const double rg = rad * 0.68;
  circle(cr, cx, cy, rad * 0.71);
  setColor(cr, rgb(8, 8, 10));
  cairo_fill(cr);
  cairo_save(cr);
  circle(cr, cx, cy, rg);
  cairo_clip(cr);
  {
    cairo_pattern_t* g = cairo_pattern_create_radial(cx, cy, 0, cx, cy, rg);
    stop(g, 0, lerpColor(rgb(20, 2, 4), rgb(255, 80, 70), float(light)));
    stop(g, 0.55, lerpColor(rgb(10, 0, 2), rgb(200, 16, 24), float(light)));
    stop(g, 0.85, lerpColor(rgb(4, 0, 0), rgb(90, 4, 10), float(light)));
    stop(g, 1, rgb(4, 0, 2));
    cairo_set_source(cr, g);
    cairo_paint(cr);
    cairo_pattern_destroy(g);
    // The iris's striations.
    const double ri = rg * 0.82;
    const double rp = rg * std::clamp(pupil, 0.08, 0.6);
    for (int k = 0; k < 72; ++k)
    {
      const double a = k * 2 * kPi / 72 + (k % 2) * 0.02;
      const double inner = rp * 1.05, outer = ri * (0.86 + 0.14 * ((k * 37) % 7) / 6.0);
      cairo_move_to(cr, cx + std::cos(a) * inner, cy + std::sin(a) * inner);
      cairo_line_to(cr, cx + std::cos(a) * outer, cy + std::sin(a) * outer);
    }
    setRgba(cr, lerpColor(rgb(40, 6, 8), rgb(255, 170, 140), float(light)), 0.45);
    cairo_set_line_width(cr, 1.6);
    cairo_stroke(cr);
    circle(cr, cx, cy, ri);
    setRgba(cr, rgb(30, 0, 4), 0.6);
    cairo_set_line_width(cr, rg * 0.05);
    cairo_stroke(cr);
    // The pupil with a hot rim.
    circle(cr, cx, cy, rp * 1.12);
    setRgba(cr, lerpColor(rgb(40, 4, 6), rgb(255, 210, 160), float(light)), 0.7);
    cairo_fill(cr);
    circle(cr, cx, cy, rp);
    setColor(cr, rgb(2, 0, 2));
    cairo_fill(cr);
    // The shutter: an iris diaphragm's eight blades closing in.
    if (shutter > 0.0)
    {
      const double open = rg * (1.0 - std::clamp(shutter, 0.0, 1.0));
      const double rotate = shutter * 0.9;
      for (int k = 0; k < 8; ++k)
      {
        const double a = rotate + k * kPi / 4;
        cairo_move_to(cr, cx + std::cos(a) * rg * 1.1, cy + std::sin(a) * rg * 1.1);
        cairo_line_to(cr, cx + std::cos(a + kPi / 4 + 0.35) * rg * 1.1, cy + std::sin(a + kPi / 4 + 0.35) * rg * 1.1);
        cairo_line_to(cr, cx + std::cos(a + kPi / 4 + 1.2) * open, cy + std::sin(a + kPi / 4 + 1.2) * open);
        cairo_line_to(cr, cx + std::cos(a + 1.2) * open, cy + std::sin(a + 1.2) * open);
        cairo_close_path(cr);
        cairo_pattern_t* g = cairo_pattern_create_linear(cx + std::cos(a) * rg, cy + std::sin(a) * rg, cx, cy);
        stop(g, 0, rgb(40, 42, 50));
        stop(g, 1, rgb(96, 100, 114));
        cairo_set_source(cr, g);
        cairo_fill_preserve(cr);
        cairo_pattern_destroy(g);
        setColor(cr, rgb(10, 10, 14));
        cairo_set_line_width(cr, 1.8);
        cairo_stroke(cr);
      }
    }
    // Reflections on the glass.
    cairo_new_path(cr);
    cairo_arc(cr, cx, cy, rg * 0.8, kPi * 1.1, kPi * 1.45);
    setRgba(cr, rgb(255, 255, 255), 0.35);
    cairo_set_line_width(cr, rg * 0.07);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_stroke(cr);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
    circle(cr, cx - rg * 0.42, cy - rg * 0.42, rg * 0.08);
    setRgba(cr, rgb(255, 255, 255), 0.6);
    cairo_fill(cr);
    circle(cr, cx + rg * 0.3, cy + rg * 0.38, rg * 0.04);
    setRgba(cr, rgb(255, 255, 255), 0.25);
    cairo_fill(cr);
  }
  cairo_restore(cr);
  circle(cr, cx, cy, rg);
  setColor(cr, rgb(110, 114, 128));
  cairo_set_line_width(cr, 2.0);
  cairo_stroke(cr);
}

void paintLance(cairo_t* cr, int pose, bool mouthOpen)
{
  const Color skin = rgb(244, 204, 168), skinDark = rgb(206, 156, 120);
  const Color suit = rgb(44, 76, 196), suitDark = rgb(20, 34, 104), suitLight = rgb(110, 150, 255);
  const Color gold = rgb(255, 210, 70), goldDark = rgb(190, 130, 20);
  const Color trousers = rgb(26, 30, 60), shoe = rgb(16, 14, 20);
  const Color scarf = rgb(226, 36, 52);
  const bool stride = pose >= 0 && pose <= 7;
  const double ph = stride ? pose * kPi / 4.0 : 0.0;
  const double swing = stride ? std::sin(ph) : 0.0;
  const double bob = stride ? std::abs(std::cos(ph)) * 6.0 : 0.0;
  const double hipX = 120, hipY = 298 - bob;
  // Legs: the back one first.
  auto leg = [&](double sw, bool back) {
    const double a = sw * 0.42;
    const double kx = hipX + std::sin(a) * 90, ky = hipY + std::cos(a) * 90;
    const double bend = sw < 0 ? -sw * 0.5 : 0.0;
    const double fx = kx + std::sin(a - bend) * 92, fy = std::min(480.0 - 8.0, ky + std::cos(a - bend) * 92);
    limb(cr, hipX, hipY, kx, ky, fx, fy, 30, back ? darken(trousers, 0.25f) : trousers, 2.6);
    // The shoe.
    roundedRect(cr, fx - 14, fy - 6, 46, 16, 7);
    fillLinear(cr, 0, fy - 6, 0, fy + 10, rgb(80, 80, 96), shoe, 2.4);
  };
  leg(-swing, true);
  // The back arm.
  const double shY = hipY - 128;
  if (pose == 8)
    limb(cr, hipX - 20, shY + 10, hipX - 80, shY - 28, hipX - 130, shY - 70, 24, suitDark, 2.6);
  else if (pose == 9)
    limb(cr, hipX - 20, shY + 10, hipX - 44, shY + 70, hipX - 16, shY + 116, 24, suitDark, 2.6);
  else
    limb(cr, hipX - 14, shY + 10, hipX - 14 - swing * 40, shY + 70, hipX - 8 - swing * 66, shY + 124, 24, suitDark, 2.6);
  if (pose == 8)
  {
    circle(cr, hipX - 134, shY - 76, 13);
    fillRadial(cr, hipX - 134, shY - 76, 13, skin, skinDark, 2.2);
  }
  leg(swing, false);
  // The jacket: a sequinned tux with gold lapels and tails.
  cairo_move_to(cr, hipX - 44, shY + 4);
  cairo_curve_to(cr, hipX - 50, shY + 60, hipX - 50, hipY - 20, hipX - 46, hipY + 18);
  cairo_line_to(cr, hipX - 10, hipY + 6);
  cairo_line_to(cr, hipX + 30, hipY + 10);
  cairo_curve_to(cr, hipX + 46, hipY - 30, hipX + 50, shY + 60, hipX + 42, shY + 4);
  cairo_curve_to(cr, hipX + 20, shY - 8, hipX - 22, shY - 8, hipX - 44, shY + 4);
  cairo_close_path(cr);
  fillLinear(cr, hipX - 50, 0, hipX + 50, 0, suitLight, suitDark, 2.6);
  // Sequins.
  std::uint32_t sd = 4242u;
  for (int i = 0; i < 40; ++i)
  {
    const double x = hipX - 40 + rnd(sd) * 80, y = shY + 8 + rnd(sd) * (hipY - shY);
    circle(cr, x, y, 1.3);
    setRgba(cr, rgb(220, 235, 255), 0.4 + 0.5 * rnd(sd));
    cairo_fill(cr);
  }
  // Shirt front, lapels, the scarf.
  cairo_move_to(cr, hipX - 12, shY - 2);
  cairo_line_to(cr, hipX + 14, shY - 2);
  cairo_line_to(cr, hipX + 4, shY + 80);
  cairo_close_path(cr);
  setColor(cr, rgb(250, 250, 255));
  cairo_fill(cr);
  for (const int side : {-1, 1})
  {
    cairo_move_to(cr, hipX + side * 14, shY - 4);
    cairo_line_to(cr, hipX + side * 30, shY + 6);
    cairo_line_to(cr, hipX + side * 8, shY + 92);
    cairo_line_to(cr, hipX + side * 2, shY + 84);
    cairo_close_path(cr);
    fillLinear(cr, 0, shY, 0, shY + 90, lighten(gold, 0.3f), goldDark, 1.8);
  }
  cairo_move_to(cr, hipX - 14, shY - 6);
  cairo_curve_to(cr, hipX - 4, shY + 4, hipX + 8, shY + 4, hipX + 16, shY - 6);
  cairo_curve_to(cr, hipX + 12, shY + 20, hipX + 22, shY + 50, hipX + 30, shY + 64);
  cairo_line_to(cr, hipX + 16, shY + 66);
  cairo_curve_to(cr, hipX + 8, shY + 40, hipX + 2, shY + 22, hipX - 4, shY + 12);
  cairo_close_path(cr);
  fillLinear(cr, 0, shY - 6, 0, shY + 66, lighten(scarf, 0.25f), darken(scarf, 0.3f), 2.0);
  // The buttons.
  for (int i = 0; i < 2; ++i)
  {
    circle(cr, hipX + 8, shY + 100 + i * 22, 3.2);
    fillRadial(cr, hipX + 8, shY + 100 + i * 22, 3.2, lighten(gold, 0.5f), goldDark, 1.0);
  }
  // The head.
  const double hx = hipX + 6, hy = shY - 58;
  limb(cr, hx - 4, hy + 30, hx - 2, hy + 44, hx - 2, hy + 52, 22, skinDark, 2.0);
  cairo_move_to(cr, hx - 34, hy - 16);
  cairo_curve_to(cr, hx - 38, hy + 22, hx - 20, hy + 46, hx + 6, hy + 48);
  cairo_curve_to(cr, hx + 30, hy + 48, hx + 40, hy + 30, hx + 38, hy - 4);
  cairo_curve_to(cr, hx + 38, hy - 40, hx - 30, hy - 46, hx - 34, hy - 16);
  cairo_close_path(cr);
  fillLinear(cr, hx - 34, hy - 40, hx + 40, hy + 48, skin, skinDark, 2.6);
  // An ear.
  ellipse(cr, hx - 26, hy + 6, 7, 10);
  fillRadial(cr, hx - 26, hy + 6, 10, skin, skinDark, 2.0);
  // The quiff: a golden wave swept up and forward.
  cairo_move_to(cr, hx - 38, hy + 4);
  cairo_curve_to(cr, hx - 48, hy - 40, hx - 20, hy - 62, hx + 18, hy - 66);
  cairo_curve_to(cr, hx + 50, hy - 70, hx + 62, hy - 50, hx + 52, hy - 34);
  cairo_curve_to(cr, hx + 44, hy - 46, hx + 26, hy - 44, hx + 18, hy - 30);
  cairo_curve_to(cr, hx + 4, hy - 22, hx - 16, hy - 22, hx - 24, hy - 6);
  cairo_close_path(cr);
  fillLinear(cr, hx - 40, hy - 70, hx + 50, hy - 10, lighten(gold, 0.45f), goldDark, 2.6);
  for (int i = 0; i < 3; ++i)
  {
    cairo_move_to(cr, hx - 30 + i * 12, hy - 14 - i * 6);
    cairo_curve_to(cr, hx - 20 + i * 12, hy - 46 - i * 4, hx + 10 + i * 10, hy - 58, hx + 36 + i * 4, hy - 56 + i * 4);
  }
  setRgba(cr, rgb(255, 250, 210), 0.7);
  cairo_set_line_width(cr, 1.6);
  cairo_stroke(cr);
  // The face: a brow, a wink of an eye, a grin full of teeth.
  limb(cr, hx + 8, hy - 14, hx + 18, hy - 18, hx + 28, hy - 15, 3, rgb(150, 100, 30), 0.8);
  ellipse(cr, hx + 20, hy - 4, 5, 6);
  setColor(cr, rgb(255, 255, 255));
  cairo_fill(cr);
  circle(cr, hx + 22, hy - 3, 3);
  setColor(cr, rgb(40, 90, 200));
  cairo_fill(cr);
  circle(cr, hx + 22.5, hy - 3, 1.3);
  setColor(cr, kInk);
  cairo_fill(cr);
  // The nose.
  cairo_move_to(cr, hx + 32, hy - 2);
  cairo_curve_to(cr, hx + 42, hy + 10, hx + 40, hy + 14, hx + 32, hy + 14);
  setColor(cr, skinDark);
  cairo_set_line_width(cr, 2.2);
  cairo_stroke(cr);
  // The grin.
  if (mouthOpen)
  {
    cairo_move_to(cr, hx + 6, hy + 22);
    cairo_curve_to(cr, hx + 16, hy + 46, hx + 34, hy + 42, hx + 38, hy + 22);
    cairo_close_path(cr);
    setColor(cr, rgb(90, 16, 30));
    cairo_fill_preserve(cr);
    setColor(cr, kInk);
    cairo_set_line_width(cr, 2.0);
    cairo_stroke(cr);
    cairo_rectangle(cr, hx + 8, hy + 22, 29, 6);
    setColor(cr, rgb(255, 255, 255));
    cairo_fill(cr);
    ellipse(cr, hx + 22, hy + 36, 7, 3);
    setColor(cr, rgb(230, 90, 110));
    cairo_fill(cr);
  }
  else
  {
    cairo_move_to(cr, hx + 4, hy + 22);
    cairo_curve_to(cr, hx + 14, hy + 34, hx + 32, hy + 32, hx + 38, hy + 20);
    cairo_curve_to(cr, hx + 30, hy + 26, hx + 14, hy + 27, hx + 4, hy + 22);
    cairo_close_path(cr);
    setColor(cr, rgb(255, 255, 255));
    cairo_fill_preserve(cr);
    setColor(cr, kInk);
    cairo_set_line_width(cr, 1.8);
    cairo_stroke(cr);
    for (int i = 1; i < 5; ++i)
    {
      cairo_move_to(cr, hx + 4 + i * 7, hy + 23 + std::sin(i * 0.7) * 2);
      cairo_line_to(cr, hx + 4 + i * 7, hy + 28);
    }
    setRgba(cr, rgb(180, 180, 190), 0.8);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);
  }
  // A twinkle off the teeth.
  glow(cr, hx + 30, hy + 24, 8, rgb(255, 255, 255), 0.8);
  // The front arm and the microphone.
  double handX = 0, handY = 0;
  if (pose == 8)
  {
    handX = hipX + 150;
    handY = shY - 70;
    limb(cr, hipX + 26, shY + 10, hipX + 90, shY - 28, handX, handY, 26, suit, 2.6);
  }
  else if (pose == 9)
  {
    handX = hipX + 62;
    handY = shY - 30;
    limb(cr, hipX + 22, shY + 14, hipX + 60, shY + 60, handX, handY, 26, suit, 2.6);
  }
  else
  {
    handX = hipX + 20 + swing * 70;
    handY = shY + 120;
    limb(cr, hipX + 18, shY + 12, hipX + 18 + swing * 40, shY + 68, handX, handY, 26, suit, 2.6);
  }
  // The mic: a black handle, a silver mesh ball, a MaxTV flag.
  const double ma = pose == 9 ? -0.5 : (pose == 8 ? -0.9 : 0.25);
  const double mx = handX + std::sin(ma) * 34, my = handY - std::cos(ma) * 34;
  limb(cr, handX - std::sin(ma) * 10, handY + std::cos(ma) * 10, (handX + mx) * 0.5, (handY + my) * 0.5, mx, my, 9,
    rgb(30, 30, 36), 1.8);
  roundedRect(cr, (handX + mx) * 0.5 - 9, (handY + my) * 0.5 - 6, 18, 12, 2);
  setColor(cr, rgb(255, 50, 60));
  cairo_fill(cr);
  circle(cr, mx, my, 12);
  fillRadial(cr, mx, my, 12, rgb(240, 240, 250), rgb(90, 94, 110), 2.0);
  circle(cr, handX, handY, 13);
  fillRadial(cr, handX, handY, 13, skin, skinDark, 2.2);
}

void paintAudienceMember(cairo_t* cr, double x, double y, double s, int kind, int frame, unsigned seed)
{
  std::uint32_t sd = seed * 2246822519u + 3u;
  if (sd == 0)
    sd = 1;
  static const Color kSkins[] = {rgb(250, 212, 180), rgb(222, 172, 130), rgb(168, 112, 74), rgb(110, 70, 46),
    rgb(236, 190, 150)};
  static const Color kHair[] = {rgb(40, 30, 26), rgb(120, 70, 30), rgb(230, 200, 110), rgb(200, 70, 40), rgb(70, 70, 80),
    rgb(240, 240, 240)};
  static const Color kShirts[] = {rgb(230, 60, 80), rgb(60, 140, 230), rgb(250, 200, 60), rgb(90, 200, 120),
    rgb(170, 90, 220), rgb(240, 240, 240), rgb(255, 140, 50)};
  Color skin = kSkins[int(rnd(sd) * 4.99)];
  Color hair = kHair[int(rnd(sd) * 5.99)];
  Color shirt = kShirts[int(rnd(sd) * 6.99)];
  const int style = int(rnd(sd) * 4.0);
  const bool waves = rnd(sd) < 0.75;
  const double phase = rnd(sd) * 6.0;
  if (kind == 1)
  {
    skin = rgb(230, 190, 150);
    shirt = rgb(70, 86, 62);
  }
  else if (kind == 2)
  {
    skin = rgb(244, 204, 168);
    shirt = rgb(44, 76, 196);
  }
  const double r = s * 0.5;
  // Arms up and waving (on a 6-frame loop, each at its own phase).
  const double t = (frame + phase) * kPi / 3.0;
  if (waves || kind != 0)
    for (const int side : {-1, 1})
    {
      const double up = 0.6 + 0.4 * std::sin(t + side);
      const double sx = x + side * r * 1.25, sy = y + r * 1.0;
      const double ex = sx + side * r * (0.6 + 0.3 * std::cos(t)), ey = sy - r * 1.3 * up;
      const double hx = ex + side * r * 0.2 * std::sin(t * 1.5), hy = ey - r * 1.1 * up;
      limb(cr, sx, sy, ex, ey, hx, hy, r * 0.42, shirt, 1.6);
      circle(cr, hx, hy, r * 0.28);
      fillRadial(cr, hx, hy, r * 0.28, skin, darken(skin, 0.25f), 1.4);
    }
  // Shoulders.
  cairo_move_to(cr, x - r * 1.6, y + r * 2.6);
  cairo_curve_to(cr, x - r * 1.6, y + r * 0.9, x - r * 0.9, y + r * 0.5, x, y + r * 0.5);
  cairo_curve_to(cr, x + r * 0.9, y + r * 0.5, x + r * 1.6, y + r * 0.9, x + r * 1.6, y + r * 2.6);
  cairo_close_path(cr);
  fillLinear(cr, 0, y, 0, y + r * 2.6, lighten(shirt, 0.15f), darken(shirt, 0.35f), 1.8);
  if (kind == 2)
  {
    // The red scarf, and frost on the shoulders.
    cairo_move_to(cr, x - r * 0.5, y + r * 0.45);
    cairo_line_to(cr, x + r * 0.5, y + r * 0.45);
    cairo_line_to(cr, x + r * 0.2, y + r * 1.6);
    cairo_close_path(cr);
    setColor(cr, rgb(226, 36, 52));
    cairo_fill(cr);
    for (const int side : {-1, 1})
    {
      ellipse(cr, x + side * r * 1.1, y + r * 0.95, r * 0.45, r * 0.18);
      setRgba(cr, rgb(220, 244, 255), 0.85);
      cairo_fill(cr);
    }
  }
  if (kind == 1)
  {
    // The flight suit's zip and a squadron patch.
    cairo_move_to(cr, x, y + r * 0.5);
    cairo_line_to(cr, x, y + r * 2.6);
    setColor(cr, rgb(40, 46, 36));
    cairo_set_line_width(cr, 2);
    cairo_stroke(cr);
    circle(cr, x + r * 0.8, y + r * 1.3, r * 0.22);
    setColor(cr, rgb(255, 150, 40));
    cairo_fill(cr);
  }
  // The head.
  circle(cr, x, y - r * 0.4, r);
  fillRadial(cr, x, y - r * 0.4, r, lighten(skin, 0.15f), darken(skin, 0.2f), 1.8);
  if (kind == 1)
  {
    // A white flight helmet with an orange stripe, the visor pushed up.
    cairo_new_path(cr);
    cairo_arc(cr, x, y - r * 0.45, r * 1.12, kPi * 1.0, kPi * 2.0);
    cairo_line_to(cr, x + r * 1.12, y - r * 0.1);
    cairo_line_to(cr, x - r * 1.12, y - r * 0.1);
    cairo_close_path(cr);
    fillLinear(cr, 0, y - r * 1.6, 0, y, rgb(250, 250, 246), rgb(170, 172, 168), 1.8);
    cairo_rectangle(cr, x - r * 0.18, y - r * 1.56, r * 0.36, r * 1.0);
    setColor(cr, rgb(255, 120, 40));
    cairo_fill(cr);
    roundedRect(cr, x - r * 0.95, y - r * 1.3, r * 1.9, r * 0.42, r * 0.15);
    fillLinear(cr, 0, y - r * 1.3, 0, y - r * 0.9, rgb(255, 190, 80), rgb(140, 70, 20), 1.4);
  }
  else if (kind == 2)
  {
    // A golden quiff.
    cairo_move_to(cr, x - r * 1.0, y - r * 0.4);
    cairo_curve_to(cr, x - r * 1.2, y - r * 1.6, x + r * 0.2, y - r * 2.0, x + r * 1.3, y - r * 1.5);
    cairo_curve_to(cr, x + r * 0.8, y - r * 1.2, x + r * 0.2, y - r * 1.1, x - r * 0.6, y - r * 0.6);
    cairo_close_path(cr);
    fillLinear(cr, 0, y - r * 2.0, 0, y - r * 0.4, rgb(255, 236, 150), rgb(200, 140, 30), 1.6);
    circle(cr, x + r * 0.7, y - r * 1.55, r * 0.12);
    setColor(cr, rgb(230, 248, 255));
    cairo_fill(cr);
  }
  else if (style == 0)
  {
    // Short hair.
    cairo_new_path(cr);
    cairo_arc(cr, x, y - r * 0.5, r * 1.02, kPi * 1.05, kPi * 1.95);
    cairo_close_path(cr);
    setColor(cr, hair);
    cairo_fill(cr);
  }
  else if (style == 1)
  {
    // Long hair down the sides.
    cairo_new_path(cr);
    cairo_arc(cr, x, y - r * 0.45, r * 1.08, kPi * 0.9, kPi * 2.1);
    cairo_line_to(cr, x + r * 1.1, y + r * 0.5);
    cairo_line_to(cr, x + r * 0.8, y + r * 0.5);
    cairo_line_to(cr, x + r * 0.8, y - r * 0.6);
    cairo_line_to(cr, x - r * 0.8, y - r * 0.6);
    cairo_line_to(cr, x - r * 0.8, y + r * 0.5);
    cairo_line_to(cr, x - r * 1.1, y + r * 0.5);
    cairo_close_path(cr);
    setColor(cr, hair);
    cairo_fill(cr);
  }
  else if (style == 2)
  {
    // A cap.
    cairo_new_path(cr);
    cairo_arc(cr, x, y - r * 0.55, r * 1.02, kPi, 2 * kPi);
    cairo_close_path(cr);
    setColor(cr, shirt);
    cairo_fill(cr);
    cairo_rectangle(cr, x - r * 0.2, y - r * 0.62, r * 1.5, r * 0.22);
    setColor(cr, darken(shirt, 0.3f));
    cairo_fill(cr);
  }
  else
  {
    // An afro.
    circle(cr, x, y - r * 0.85, r * 1.15);
    setColor(cr, hair);
    cairo_fill(cr);
    circle(cr, x, y - r * 0.25, r * 0.85);
    fillRadial(cr, x, y - r * 0.3, r * 0.85, lighten(skin, 0.15f), darken(skin, 0.2f), 0);
  }
  // A cheering face: eyes and an open mouth.
  for (const int side : {-1, 1})
  {
    circle(cr, x + side * r * 0.35, y - r * 0.45, r * 0.1);
    setColor(cr, kInk);
    cairo_fill(cr);
  }
  ellipse(cr, x, y - r * 0.02, r * 0.28, r * (0.16 + 0.06 * std::sin(t * 2.0)));
  setColor(cr, rgb(110, 20, 40));
  cairo_fill(cr);
}

} // namespace gr
