// Too Many Cooks (Level 24) in the style of enemy_art.cpp: the Meatball, the
// Cleaver Bot and the Egg Timer, each drawn in screen pixels into a texture
// with a 32 px margin, the cell box at (32, 32) .. (32 + w, 32 + h), facing
// right. Also a thrown cleaver, the duck's chef's hat and the pieces the
// level and the cutscenes share: a toque, the chef's giant hand, the chef's
// bell, the golden whisk, a meatball and a cleaver.

#include "assets/enemy_art_kitchen.hpp"

#include <algorithm>
#include <cmath>

namespace gr
{

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr Color kInk = rgb(30, 18, 14);
constexpr double kLine = 2.2;
constexpr double kM = 32.0;

constexpr Color kSkin = rgb(246, 206, 170);
constexpr Color kSkinDark = rgb(198, 140, 104);
constexpr Color kLinen = rgb(252, 250, 246);
constexpr Color kLinenShade = rgb(200, 204, 214);
constexpr Color kSteel = rgb(214, 222, 232);
constexpr Color kSteelDark = rgb(110, 120, 136);
constexpr Color kSauce = rgb(206, 38, 24);

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

// Fills the path with a linear gradient and inks its outline (line 0: no ink).
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

// Chrome: bright, mid, a dark band and a bright rim again, across (x0..x1).
void fillChrome(cairo_t* cr, double x0, double y0, double x1, double y1, double line = kLine)
{
  cairo_pattern_t* g = cairo_pattern_create_linear(x0, y0, x1, y1);
  stop(g, 0, rgb(250, 252, 255));
  stop(g, 0.3, kSteel);
  stop(g, 0.62, kSteelDark);
  stop(g, 0.8, rgb(170, 180, 194));
  stop(g, 1, rgb(236, 240, 246));
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
  cairo_new_path(cr);
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

// A cartoon eye: white, the pupil looking (lx, ly), a lid cut down by
// `lid` 0..1 (an angry slant when `angry`).
void eye(cairo_t* cr, double x, double y, double r, double lx, double ly, double lid, bool angry, double line)
{
  ellipse(cr, x, y, r, r * 1.15);
  setColor(cr, rgb(255, 255, 255));
  cairo_fill_preserve(cr);
  setColor(cr, kInk);
  cairo_set_line_width(cr, line);
  cairo_stroke(cr);
  circle(cr, x + lx * r * 0.4, y + ly * r * 0.4, r * 0.5);
  setColor(cr, kInk);
  cairo_fill(cr);
  circle(cr, x + lx * r * 0.4 - r * 0.15, y + ly * r * 0.4 - r * 0.18, r * 0.15);
  setColor(cr, rgb(255, 255, 255));
  cairo_fill(cr);
  if (lid > 0 || angry)
  {
    cairo_save(cr);
    ellipse(cr, x, y, r, r * 1.15);
    cairo_clip(cr);
    const double top = y - r * 1.2 + lid * r * 2.3;
    cairo_move_to(cr, x - r * 1.3, angry ? top - r * 0.5 : top);
    cairo_line_to(cr, x + r * 1.3, angry ? top + r * 0.5 : top);
    cairo_line_to(cr, x + r * 1.3, y - r * 2);
    cairo_line_to(cr, x - r * 1.3, y - r * 2);
    cairo_close_path(cr);
    setColor(cr, kInk);
    cairo_fill(cr);
    cairo_restore(cr);
  }
}

// --- The Meatball ------------------------------------------------------------------

void meatball(cairo_t* cr, double w, double h, int variant, int frame)
{
  const bool moldy = variant >= 3;
  const int size = variant % 3;
  const double cx = kM + w * 0.5, cy = kM + h * 0.5 + 1, rad = std::min(w, h) * 0.47;
  const double roll = double(frame % 4) * kPi * 0.5 * 0.5;
  // Its shadow on the belt.
  ellipse(cr, cx, kM + h - 1, rad * 0.8, rad * 0.12);
  setRgba(cr, rgb(0, 0, 0), 0.25);
  cairo_fill(cr);
  paintMeatball(cr, cx, cy, rad, roll, moldy, !moldy);
  // The face: eyes on the front (right), looking where it rolls; the bigger
  // ones scowl.
  const double er = rad * (size == 0 ? 0.2 : 0.17);
  const double ex = cx + rad * 0.2, ey = cy - rad * 0.08;
  eye(cr, ex, ey, er, 0.7, 0.1, moldy ? 0.35 : 0.0, size > 0 && !moldy, 1.4 + size * 0.4);
  eye(cr, ex + er * 2.3, ey + er * 0.15, er * 0.9, 0.7, 0.1, moldy ? 0.35 : 0.0, size > 0 && !moldy, 1.4 + size * 0.4);
  if (size > 0 && !moldy)
  {
    // Gritted teeth.
    const double mx = cx + rad * 0.42, my = cy + rad * 0.32;
    cairo_rectangle(cr, mx - rad * 0.22, my - rad * 0.07, rad * 0.44, rad * 0.14);
    setColor(cr, rgb(255, 250, 240));
    cairo_fill_preserve(cr);
    setColor(cr, kInk);
    cairo_set_line_width(cr, 1.4);
    cairo_stroke(cr);
    for (int i = 1; i < 4; ++i)
    {
      cairo_move_to(cr, mx - rad * 0.22 + i * rad * 0.11, my - rad * 0.07);
      cairo_line_to(cr, mx - rad * 0.22 + i * rad * 0.11, my + rad * 0.07);
    }
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);
  }
  else
  {
    cairo_move_to(cr, cx + rad * 0.3, cy + rad * 0.34);
    cairo_curve_to(cr, cx + rad * 0.4, cy + rad * (moldy ? 0.28 : 0.44), cx + rad * 0.55, cy + rad * (moldy ? 0.28 : 0.44),
      cx + rad * 0.64, cy + rad * 0.32);
    setColor(cr, kInk);
    cairo_set_line_width(cr, 1.8);
    cairo_stroke(cr);
  }
  if (moldy)
  {
    // Flies buzzing round it, in two places a frame.
    for (int f = 0; f < 3; ++f)
    {
      const double a = f * 2.1 + (frame % 2) * 0.9 + (frame / 2 % 2) * 0.4;
      const double fx = cx + std::cos(a) * rad * 1.2, fy = cy - rad * 0.55 + std::sin(a) * rad * 0.45;
      // A dotted trail.
      for (int k = 1; k <= 3; ++k)
      {
        circle(cr, fx - std::cos(a + 1.2) * k * 5.0, fy - std::sin(a + 1.2) * k * 3.5, 0.9);
        setRgba(cr, rgb(30, 40, 20), 0.5);
        cairo_fill(cr);
      }
      for (const int side : {-1, 1})
      {
        ellipse(cr, fx + side * 2.6, fy - 3.2 - (frame % 2) * 1.2, 2.6, 1.6);
        setRgba(cr, rgb(220, 240, 255), 0.75);
        cairo_fill(cr);
      }
      ellipse(cr, fx, fy, 3.0, 2.3);
      setColor(cr, rgb(20, 26, 16));
      cairo_fill(cr);
      circle(cr, fx + 1.8, fy - 0.6, 0.9);
      setColor(cr, rgb(200, 40, 30));
      cairo_fill(cr);
    }
  }
}

// --- The Cleaver Bot ---------------------------------------------------------------

void cleaverBot(cairo_t* cr, double w, double h, int variant, int frame)
{
  const double sx = w / 96.0, sy = h / 160.0, s = std::min(sx, sy);
  auto X = [&](double x) { return kM + x * sx; };
  auto Y = [&](double y) { return kM + y * sy; };
  const bool tell = variant == 1, thrown = variant == 2;
  // The far arm: a fist on the hip, behind the body.
  limb(cr, X(20), Y(88), X(4), Y(106), X(14), Y(122), 9 * s, kSteel, 1.6);
  // Legs: two stubby chrome legs on rubber feet.
  for (const double lx : {32.0, 62.0})
  {
    cairo_rectangle(cr, X(lx - 6), Y(136), 12 * sx, 16 * sy);
    fillChrome(cr, X(lx - 6), 0, X(lx + 6), 0, 1.6);
    cairo_new_path(cr);
    roundedRect(cr, X(lx - 12), Y(150), 24 * sx, 10 * sy, 4 * s);
    fillLinear(cr, 0, Y(150), 0, Y(160), rgb(70, 66, 70), rgb(24, 22, 26), 1.6);
  }
  // The body: a stockpot, riveted band, two loop handles.
  for (const int side : {-1, 1})
  {
    ellipse(cr, X(48 + side * 40), Y(96), 8 * sx, 6 * sy);
    setColor(cr, kInk);
    cairo_set_line_width(cr, 5.5 * s);
    cairo_stroke_preserve(cr);
    setColor(cr, kSteel);
    cairo_set_line_width(cr, 3 * s);
    cairo_stroke(cr);
  }
  roundedRect(cr, X(12), Y(72), 72 * sx, 68 * sy, 7 * s);
  fillChrome(cr, X(12), 0, X(84), 0);
  cairo_rectangle(cr, X(12), Y(80), 72 * sx, 8 * sy);
  fillLinear(cr, 0, Y(80), 0, Y(88), rgb(150, 160, 176), rgb(92, 100, 116), 1.4);
  for (int i = 0; i < 6; ++i)
  {
    circle(cr, X(18 + i * 12), Y(84), 1.6 * s);
    setColor(cr, rgb(240, 244, 250));
    cairo_fill(cr);
  }
  // A dial on its belly: the mode it is in.
  circle(cr, X(56), Y(110), 10 * s);
  fillRadial(cr, X(56), Y(110), 10 * s, rgb(60, 60, 68), rgb(20, 20, 24), 1.6);
  const double da = tell ? 0.9 : -0.6;
  cairo_move_to(cr, X(56), Y(110));
  cairo_line_to(cr, X(56) + std::cos(da - kPi / 2) * 8 * s, Y(110) + std::sin(da - kPi / 2) * 8 * s);
  setColor(cr, tell ? rgb(255, 80, 60) : rgb(255, 210, 90));
  cairo_set_line_width(cr, 2.0 * s);
  cairo_stroke(cr);
  // The head: the pot's lid, domed, a knob for a nose... and a visor.
  cairo_move_to(cr, X(10), Y(74));
  cairo_curve_to(cr, X(14), Y(46), X(82), Y(46), X(86), Y(74));
  cairo_close_path(cr);
  fillChrome(cr, X(10), Y(46), X(86), Y(74));
  roundedRect(cr, X(36), Y(54), 44 * sx, 12 * sy, 5 * s);
  fillLinear(cr, 0, Y(54), 0, Y(66), rgb(40, 40, 50), rgb(10, 10, 16), 1.6);
  const Color eyeC = tell ? rgb(255, 60, 40) : rgb(120, 230, 255);
  glow(cr, X(64), Y(60), (tell ? 18 : 12) * s, eyeC, tell ? 0.9 : 0.6);
  roundedRect(cr, X(56), Y(57), 16 * sx, 6 * sy, 3 * s);
  setColor(cr, lighten(eyeC, 0.4f));
  cairo_fill(cr);
  if (tell)
  {
    // The brow cut down hard.
    cairo_move_to(cr, X(50), Y(52));
    cairo_line_to(cr, X(80), Y(58));
    setColor(cr, kInk);
    cairo_set_line_width(cr, 3 * s);
    cairo_stroke(cr);
  }
  // The steam vent puffing.
  circle(cr, X(22), Y(64), 3 * s);
  setColor(cr, kInk);
  cairo_fill(cr);
  for (int k = 0; k < 3; ++k)
  {
    const double py = Y(58) - k * 9 * s - (frame % 2) * 4 * s, px = X(20) - k * 4 * s + (frame % 2) * 2 * s;
    circle(cr, px, py, (3 + k * 1.5) * s);
    setRgba(cr, rgb(255, 255, 255), 0.55 - k * 0.15);
    cairo_fill(cr);
  }
  // The toque.
  paintChefHat(cr, X(46), Y(50), 46 * sx, 44 * sy, -0.08);
  // The cleaver arm (the near side).
  const Color arm = kSteel;
  if (tell)
  {
    // Raised over its head, the cleaver back and up for the throw.
    limb(cr, X(76), Y(86), X(90), Y(56), X(80), Y(22), 9 * s, arm, 1.6);
    cairo_save(cr);
    cairo_translate(cr, X(80), Y(22));
    cairo_rotate(cr, -kPi * 0.62);
    paintCleaver(cr, 60 * s);
    cairo_restore(cr);
    circle(cr, X(80), Y(22), 6.5 * s);
    fillRadial(cr, X(80), Y(22), 6.5 * s, rgb(240, 244, 250), kSteelDark, 1.6);
  }
  else if (thrown)
  {
    // Follow-through: the arm out in front, the hand empty.
    limb(cr, X(76), Y(86), X(94), Y(84), X(110), Y(76), 9 * s, arm, 1.6);
    for (int f = 0; f < 3; ++f)
    {
      cairo_move_to(cr, X(110), Y(74 + f * 3));
      cairo_line_to(cr, X(118), Y(70 + f * 4));
    }
    setColor(cr, kInk);
    cairo_set_line_width(cr, 3.4 * s);
    cairo_stroke(cr);
    circle(cr, X(110), Y(76), 6.5 * s);
    fillRadial(cr, X(110), Y(76), 6.5 * s, rgb(240, 244, 250), kSteelDark, 1.6);
  }
  else
  {
    // At its side, the cleaver hanging blade down, swaying on frame.
    limb(cr, X(76), Y(86), X(92), Y(102), X(88), Y(120), 9 * s, arm, 1.6);
    cairo_save(cr);
    cairo_translate(cr, X(88), Y(116));
    cairo_rotate(cr, kPi * 0.5 + (frame % 2 ? 0.05 : -0.05));
    paintCleaver(cr, 54 * s);
    cairo_restore(cr);
    circle(cr, X(88), Y(118), 6.5 * s);
    fillRadial(cr, X(88), Y(118), 6.5 * s, rgb(240, 244, 250), kSteelDark, 1.6);
  }
}

// --- The Egg Timer -----------------------------------------------------------------

void eggTimer(cairo_t* cr, double w, double h, int variant, int frame)
{
  const double s = std::min(w, h) / 96.0;
  const bool asleep = variant >= 24, ringing = !asleep && variant >= 12;
  const int dial = asleep ? 11 : variant % 12;
  const double cx = kM + w * 0.5, base = kM + h;
  cairo_save(cr);
  if (ringing)
  {
    cairo_translate(cr, cx, base);
    cairo_rotate(cr, frame % 2 ? 0.09 : -0.09);
    cairo_translate(cr, -cx, -base);
  }
  // Feet, toddling.
  for (const int side : {-1, 1})
  {
    const double lift = (frame % 2 == (side > 0 ? 0 : 1)) && !asleep ? 4 * s : 0.0;
    ellipse(cr, cx + side * 16 * s + 6 * s, base - 5 * s - lift, 12 * s, 5.5 * s);
    fillLinear(cr, 0, base - 12 * s, 0, base, rgb(255, 170, 60), rgb(210, 110, 20), 1.6);
  }
  // The egg: cream white, a little lopsided.
  const double ey = base - 50 * s;
  cairo_move_to(cr, cx, ey - 40 * s);
  cairo_curve_to(cr, cx + 26 * s, ey - 40 * s, cx + 38 * s, ey + 8 * s, cx + 36 * s, ey + 22 * s);
  cairo_curve_to(cr, cx + 34 * s, ey + 42 * s, cx - 34 * s, ey + 42 * s, cx - 36 * s, ey + 22 * s);
  cairo_curve_to(cr, cx - 38 * s, ey + 8 * s, cx - 26 * s, ey - 40 * s, cx, ey - 40 * s);
  cairo_close_path(cr);
  cairo_pattern_t* g = cairo_pattern_create_radial(cx - 12 * s, ey - 16 * s, 4 * s, cx, ey, 50 * s);
  stop(g, 0, rgb(255, 255, 250));
  stop(g, 0.7, rgb(246, 232, 204));
  stop(g, 1, rgb(214, 190, 150));
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  setColor(cr, kInk);
  cairo_set_line_width(cr, kLine);
  cairo_stroke(cr);
  // The seam where the top half turns.
  cairo_move_to(cr, cx - 35 * s, ey + 4 * s);
  cairo_curve_to(cr, cx - 20 * s, ey + 10 * s, cx + 20 * s, ey + 10 * s, cx + 35 * s, ey + 4 * s);
  setRgba(cr, rgb(150, 120, 80), 0.8);
  cairo_set_line_width(cr, 1.4);
  cairo_stroke(cr);
  // The dial on its belly: the red time left as a wedge, ticks round it,
  // the hand pointing at what is left.
  const double dx = cx + 3 * s, dy = ey + 20 * s, dr = 17 * s;
  circle(cr, dx, dy, dr);
  setColor(cr, rgb(255, 252, 244));
  cairo_fill_preserve(cr);
  setColor(cr, kInk);
  cairo_set_line_width(cr, 1.6);
  cairo_stroke(cr);
  const double left = double(dial + 1) / 12.0;
  const double a0 = -kPi / 2, a1 = a0 + left * 2 * kPi;
  cairo_move_to(cr, dx, dy);
  cairo_arc(cr, dx, dy, dr * 0.84, a0, a1);
  cairo_close_path(cr);
  setColor(cr, dial <= 2 ? rgb(240, 40, 30) : rgb(232, 80, 52));
  cairo_fill(cr);
  for (int i = 0; i < 12; ++i)
  {
    const double a = a0 + i * kPi / 6;
    cairo_move_to(cr, dx + std::cos(a) * dr * 0.86, dy + std::sin(a) * dr * 0.86);
    cairo_line_to(cr, dx + std::cos(a) * dr * (i % 3 == 0 ? 0.62 : 0.74), dy + std::sin(a) * dr * (i % 3 == 0 ? 0.62 : 0.74));
  }
  setColor(cr, kInk);
  cairo_set_line_width(cr, 1.2);
  cairo_stroke(cr);
  cairo_move_to(cr, dx, dy);
  cairo_line_to(cr, dx + std::cos(a1) * dr * 0.8, dy + std::sin(a1) * dr * 0.8);
  cairo_set_line_width(cr, 2.2);
  cairo_stroke(cr);
  circle(cr, dx, dy, 2.4 * s);
  cairo_fill(cr);
  // The face over the dial.
  if (asleep)
  {
    for (const double ex : {-8.0, 12.0})
    {
      cairo_move_to(cr, cx + ex * s - 6 * s, ey - 12 * s);
      cairo_curve_to(cr, cx + ex * s - 3 * s, ey - 8 * s, cx + ex * s + 3 * s, ey - 8 * s, cx + ex * s + 6 * s, ey - 12 * s);
    }
    setColor(cr, kInk);
    cairo_set_line_width(cr, 2.0);
    cairo_stroke(cr);
    selectGameFont(cr);
    cairo_set_font_size(cr, 14 * s);
    cairo_move_to(cr, cx + 26 * s, ey - 34 * s);
    setColor(cr, rgb(90, 110, 200));
    cairo_show_text(cr, "z");
  }
  else
  {
    eye(cr, cx - 6 * s, ey - 12 * s, 7 * s, 0.6, 0.2, 0.0, ringing || dial <= 3, 1.6);
    eye(cr, cx + 13 * s, ey - 11 * s, 6.5 * s, 0.6, 0.2, 0.0, ringing || dial <= 3, 1.6);
  }
  // The bell on its crown: a brass dome and a striker.
  const double by = ey - 40 * s;
  cairo_move_to(cr, cx - 11 * s, by + 2 * s);
  cairo_curve_to(cr, cx - 10 * s, by - 12 * s, cx + 10 * s, by - 12 * s, cx + 11 * s, by + 2 * s);
  cairo_close_path(cr);
  fillLinear(cr, cx - 11 * s, 0, cx + 11 * s, 0, rgb(255, 228, 130), rgb(176, 120, 30), 1.6);
  circle(cr, cx, by - 12 * s, 2.8 * s);
  fillRadial(cr, cx, by - 12 * s, 2.8 * s, rgb(255, 236, 160), rgb(170, 110, 30), 1.2);
  cairo_restore(cr);
  if (ringing)
  {
    // Ring lines either side of the bell.
    for (const int side : {-1, 1})
      for (int k = 0; k < 3; ++k)
      {
        const double r0 = (16 + k * 7 + (frame % 2) * 3) * s;
        cairo_new_path(cr);
        cairo_arc(cr, cx, by - 4 * s, r0, side > 0 ? -0.9 : kPi - 0.4, side > 0 ? -0.9 + 0.5 : kPi + 0.1);
        setRgba(cr, rgb(255, 210, 60), 0.9 - k * 0.25);
        cairo_set_line_width(cr, 2.4);
        cairo_stroke(cr);
      }
  }
}

} // namespace

// --- Shared pieces -----------------------------------------------------------------

void paintChefHat(cairo_t* cr, double cx, double bandY, double w, double h, double tilt)
{
  cairo_save(cr);
  cairo_translate(cr, cx, bandY);
  cairo_rotate(cr, tilt);
  const double bh = h * 0.3, hw = w * 0.5;
  // The crown: puffed lobes billowing out over the band.
  cairo_new_path(cr);
  cairo_move_to(cr, -hw * 0.92, -bh);
  cairo_curve_to(cr, -hw * 1.45, -bh - h * 0.25, -hw * 1.2, -h * 0.98, -hw * 0.5, -h * 0.9);
  cairo_curve_to(cr, -hw * 0.35, -h * 1.12, hw * 0.35, -h * 1.12, hw * 0.5, -h * 0.9);
  cairo_curve_to(cr, hw * 1.2, -h * 0.98, hw * 1.45, -bh - h * 0.25, hw * 0.92, -bh);
  cairo_close_path(cr);
  cairo_pattern_t* g = cairo_pattern_create_linear(-hw, 0, hw, 0);
  stop(g, 0, kLinen);
  stop(g, 0.6, rgb(244, 244, 248));
  stop(g, 1, kLinenShade);
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  setColor(cr, kInk);
  cairo_set_line_width(cr, std::max(1.0, w * 0.035));
  cairo_stroke(cr);
  // The folds between the lobes.
  for (const double fx : {-0.45, 0.0, 0.45})
  {
    cairo_move_to(cr, hw * fx, -bh - h * 0.05);
    cairo_curve_to(cr, hw * (fx + 0.06), -h * 0.55, hw * (fx - 0.04), -h * 0.75, hw * fx * 1.1, -h * 0.92);
  }
  setRgba(cr, kLinenShade, 0.9);
  cairo_set_line_width(cr, std::max(0.8, w * 0.025));
  cairo_stroke(cr);
  // The band, pleated.
  cairo_rectangle(cr, -hw, -bh, w, bh);
  cairo_pattern_t* bg = cairo_pattern_create_linear(-hw, 0, hw, 0);
  stop(bg, 0, rgb(255, 255, 255));
  stop(bg, 1, kLinenShade);
  cairo_set_source(cr, bg);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(bg);
  setColor(cr, kInk);
  cairo_set_line_width(cr, std::max(1.0, w * 0.035));
  cairo_stroke(cr);
  for (int i = 1; i < 6; ++i)
  {
    cairo_move_to(cr, -hw + w * i / 6.0, -bh + 1);
    cairo_line_to(cr, -hw + w * i / 6.0, -1);
  }
  setRgba(cr, kLinenShade, 1.0);
  cairo_set_line_width(cr, std::max(0.6, w * 0.02));
  cairo_stroke(cr);
  cairo_restore(cr);
}

void paintChefHand(cairo_t* cr, double s, int pose, double sleeve)
{
  const double line = 3.0 * s;
  auto skin = [&](double x0, double y0, double x1, double y1) {
    fillLinear(cr, x0, y0, x1, y1, lighten(kSkin, 0.15f), kSkinDark, line);
  };
  if (pose == 0)
  {
    // The sleeve, down from above to the wrist, and its cuff.
    cairo_rectangle(cr, -22 * s, -6 * s - sleeve * s, 54 * s, sleeve * s - 22 * s);
    fillLinear(cr, -22 * s, 0, 32 * s, 0, kLinen, kLinenShade, line);
    roundedRect(cr, -26 * s, -34 * s, 62 * s, 22 * s, 4 * s);
    fillLinear(cr, -26 * s, 0, 36 * s, 0, rgb(255, 255, 255), kLinenShade, line);
    for (const double by : {-26.0, -18.0})
    {
      circle(cr, 22 * s, by * s, 2.4 * s);
      setColor(cr, rgb(40, 40, 50));
      cairo_fill(cr);
    }
    // The palm, flat, the heel by the wrist, the fingers out to the left,
    // their tips curling up a little.
    cairo_move_to(cr, 14 * s, -14 * s);
    cairo_curve_to(cr, 20 * s, 2 * s, 12 * s, 22 * s, -10 * s, 24 * s);
    cairo_line_to(cr, -96 * s, 22 * s);
    cairo_curve_to(cr, -118 * s, 20 * s, -128 * s, 8 * s, -126 * s, -8 * s);
    cairo_curve_to(cr, -124 * s, -18 * s, -114 * s, -18 * s, -112 * s, -8 * s);
    cairo_curve_to(cr, -110 * s, -2 * s, -104 * s, -6 * s, -96 * s, -6 * s);
    cairo_line_to(cr, -10 * s, -6 * s);
    cairo_close_path(cr);
    skin(0, -18 * s, 0, 24 * s);
    // Finger lines along the side.
    for (const double fy : {4.0, 12.0})
    {
      cairo_move_to(cr, -110 * s, fy * s);
      cairo_line_to(cr, -50 * s, fy * s + 1 * s);
    }
    setColor(cr, kSkinDark);
    cairo_set_line_width(cr, 1.6 * s);
    cairo_stroke(cr);
    // The thumb on the near side, pointing along.
    cairo_move_to(cr, -4 * s, 2 * s);
    cairo_curve_to(cr, -20 * s, -14 * s, -46 * s, -16 * s, -64 * s, -12 * s);
    cairo_curve_to(cr, -72 * s, -10 * s, -72 * s, -2 * s, -62 * s, -1 * s);
    cairo_curve_to(cr, -44 * s, 0, -24 * s, 6 * s, -10 * s, 12 * s);
    cairo_close_path(cr);
    skin(0, -16 * s, 0, 12 * s);
    ellipse(cr, -64 * s, -9 * s, 5 * s, 3 * s);
    setRgba(cr, rgb(255, 236, 226), 0.9);
    cairo_fill(cr);
    return;
  }
  // Thumbs up: the sleeve off to the right, the cuff, the fist, the thumb.
  cairo_rectangle(cr, 18 * s, -34 * s, sleeve * s, 68 * s);
  fillLinear(cr, 0, -34 * s, 0, 34 * s, kLinen, kLinenShade, line);
  roundedRect(cr, 8 * s, -38 * s, 26 * s, 76 * s, 4 * s);
  fillLinear(cr, 0, -38 * s, 0, 38 * s, rgb(255, 255, 255), kLinenShade, line);
  for (const double by : {-18.0, 14.0})
  {
    circle(cr, 21 * s, by * s, 2.6 * s);
    setColor(cr, rgb(40, 40, 50));
    cairo_fill(cr);
  }
  // The fist: the back of the hand, then the curled fingers stacked on the
  // left.
  roundedRect(cr, -58 * s, -26 * s, 70 * s, 58 * s, 14 * s);
  skin(0, -26 * s, 0, 32 * s);
  for (int i = 0; i < 4; ++i)
  {
    const double fy = -24 * s + i * 14 * s;
    roundedRect(cr, -76 * s, fy, 34 * s, 15 * s, 7 * s);
    skin(0, fy, 0, fy + 15 * s);
  }
  // The thumb, up.
  cairo_move_to(cr, -44 * s, -20 * s);
  cairo_curve_to(cr, -50 * s, -40 * s, -50 * s, -66 * s, -42 * s, -76 * s);
  cairo_curve_to(cr, -34 * s, -86 * s, -20 * s, -82 * s, -20 * s, -70 * s);
  cairo_curve_to(cr, -20 * s, -52 * s, -16 * s, -36 * s, -10 * s, -22 * s);
  cairo_close_path(cr);
  skin(-50 * s, 0, -10 * s, 0);
  // Its nail.
  roundedRect(cr, -40 * s, -82 * s, 14 * s, 12 * s, 5 * s);
  setRgba(cr, rgb(255, 232, 222), 0.95);
  cairo_fill_preserve(cr);
  setColor(cr, kSkinDark);
  cairo_set_line_width(cr, 1.4 * s);
  cairo_stroke(cr);
}

void paintChefBell(cairo_t* cr, double cx, double top, double s, double swing, double ring)
{
  // The bracket: a curled iron arm.
  cairo_move_to(cr, cx, top - 6);
  cairo_line_to(cr, cx, top + s * 0.14);
  setColor(cr, kInk);
  cairo_set_line_width(cr, s * 0.08);
  cairo_stroke(cr);
  cairo_save(cr);
  cairo_translate(cr, cx, top + s * 0.14);
  cairo_rotate(cr, swing);
  // The clapper, swinging less (it lags the bell).
  const double cl = -swing * 0.8;
  cairo_move_to(cr, 0, 0);
  cairo_line_to(cr, std::sin(cl) * s * 0.78, std::cos(cl) * s * 0.78);
  setColor(cr, kInk);
  cairo_set_line_width(cr, s * 0.04);
  cairo_stroke(cr);
  circle(cr, std::sin(cl) * s * 0.8, std::cos(cl) * s * 0.8, s * 0.08);
  fillRadial(cr, std::sin(cl) * s * 0.8, std::cos(cl) * s * 0.8, s * 0.08, rgb(255, 230, 150), rgb(150, 100, 20), 1.4);
  // The bell: the crown, the waist, the flared lip.
  const double hw = s * 0.42;
  cairo_move_to(cr, -s * 0.12, 0);
  cairo_curve_to(cr, -s * 0.26, 0, -s * 0.24, s * 0.3, -s * 0.28, s * 0.5);
  cairo_curve_to(cr, -s * 0.3, s * 0.62, -hw, s * 0.68, -hw, s * 0.74);
  cairo_line_to(cr, hw, s * 0.74);
  cairo_curve_to(cr, hw, s * 0.68, s * 0.3, s * 0.62, s * 0.28, s * 0.5);
  cairo_curve_to(cr, s * 0.24, s * 0.3, s * 0.26, 0, s * 0.12, 0);
  cairo_close_path(cr);
  cairo_pattern_t* g = cairo_pattern_create_linear(-hw, 0, hw, 0);
  stop(g, 0, rgb(150, 96, 20));
  stop(g, 0.3, lerpColor(rgb(255, 222, 120), rgb(255, 255, 230), float(ring)));
  stop(g, 0.55, rgb(230, 170, 50));
  stop(g, 1, rgb(120, 76, 14));
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  setColor(cr, kInk);
  cairo_set_line_width(cr, std::max(1.4, s * 0.035));
  cairo_stroke(cr);
  // The lip's band and the shine.
  cairo_rectangle(cr, -hw, s * 0.66, hw * 2, s * 0.08);
  setRgba(cr, rgb(255, 240, 180), 0.6);
  cairo_fill(cr);
  cairo_move_to(cr, -s * 0.14, s * 0.1);
  cairo_curve_to(cr, -s * 0.2, s * 0.3, -s * 0.2, s * 0.45, -s * 0.24, s * 0.6);
  setRgba(cr, rgb(255, 255, 240), 0.7 + 0.3 * ring);
  cairo_set_line_width(cr, s * 0.05);
  cairo_stroke(cr);
  cairo_restore(cr);
}

void paintGoldenWhisk(cairo_t* cr, double len)
{
  const double hl = len * 0.42, hw = len * 0.075;
  // The wires: loops from the collar out and round.
  const int loops = 5;
  for (int pass = 0; pass < 2; ++pass)
    for (int i = 0; i < loops; ++i)
    {
      const double spread = len * (0.12 + 0.05 * std::abs(i - 2));
      const double side = i < 2 ? -1.0 : (i > 2 ? 1.0 : 0.0);
      const double bow = side * spread * 0.6;
      cairo_move_to(cr, -hw * 0.6, -hl);
      cairo_curve_to(cr, -hw - spread + bow, -hl - len * 0.35, -spread * 0.9 + bow, -len * 0.98, bow, -len);
      cairo_curve_to(cr, spread * 0.9 + bow, -len * 0.98, hw + spread + bow, -hl - len * 0.35, hw * 0.6, -hl);
      if (pass == 0)
      {
        setColor(cr, rgb(110, 70, 10));
        cairo_set_line_width(cr, std::max(2.4, len * 0.03));
      }
      else
      {
        setColor(cr, i % 2 ? rgb(255, 222, 110) : rgb(255, 240, 170));
        cairo_set_line_width(cr, std::max(1.2, len * 0.015));
      }
      cairo_stroke(cr);
    }
  // The collar and the handle.
  roundedRect(cr, -hw * 1.15, -hl - len * 0.04, hw * 2.3, len * 0.07, hw * 0.4);
  fillLinear(cr, -hw, 0, hw, 0, rgb(255, 236, 150), rgb(150, 100, 20), 1.6);
  roundedRect(cr, -hw, -hl, hw * 2, hl, hw);
  fillLinear(cr, -hw, 0, hw, 0, rgb(255, 240, 170), rgb(170, 116, 24), 1.8);
  cairo_move_to(cr, -hw * 0.4, -hl + 4);
  cairo_line_to(cr, -hw * 0.4, -6);
  setRgba(cr, rgb(255, 255, 230), 0.8);
  cairo_set_line_width(cr, hw * 0.35);
  cairo_stroke(cr);
  circle(cr, 0, -hw * 1.2, hw * 0.35);
  setColor(cr, rgb(120, 76, 10));
  cairo_fill(cr);
}

void paintMeatball(cairo_t* cr, double cx, double cy, double rad, double roll, bool moldy, bool sauce)
{
  const Color light = moldy ? rgb(170, 186, 120) : rgb(204, 124, 76);
  const Color dark = moldy ? rgb(70, 86, 50) : rgb(96, 42, 24);
  // The lumpy outline.
  const int n = 22;
  for (int i = 0; i <= n; ++i)
  {
    const double a = double(i) / n * 2 * kPi;
    const double bump = 1.0 + 0.04 * std::sin(a * 5 + roll * 5) + 0.025 * std::sin(a * 9 + 1.3 + roll * 9);
    const double x = cx + std::cos(a) * rad * bump, y = cy + std::sin(a) * rad * bump;
    if (i == 0)
      cairo_move_to(cr, x, y);
    else
      cairo_line_to(cr, x, y);
  }
  cairo_close_path(cr);
  fillRadial(cr, cx, cy, rad * 1.05, light, dark, std::max(1.6, rad * 0.05));
  // The crust: darker seared patches and lighter crumbs, turning with it.
  cairo_save(cr);
  circle(cr, cx, cy, rad * 0.97);
  cairo_clip(cr);
  for (int i = 0; i < 16; ++i)
  {
    const double a = i * 2.39996 + roll, d = rad * (0.2 + 0.7 * std::fmod(i * 0.618, 1.0));
    const double x = cx + std::cos(a) * d, y = cy + std::sin(a) * d * 0.95;
    const double rr = rad * (0.05 + 0.05 * (i % 3));
    ellipse(cr, x, y, rr * 1.3, rr);
    if (moldy)
      setRgba(cr, i % 3 == 0 ? rgb(230, 240, 210) : rgb(60, 80, 40), i % 3 == 0 ? 0.75 : 0.5);
    else
      setRgba(cr, i % 4 == 0 ? rgb(236, 170, 120) : rgb(70, 28, 14), i % 4 == 0 ? 0.6 : 0.45);
    cairo_fill(cr);
  }
  // Parsley flecks (or mould fuzz).
  for (int i = 0; i < 10; ++i)
  {
    const double a = i * 1.7 + roll, d = rad * (0.15 + 0.75 * std::fmod(i * 0.414 + 0.2, 1.0));
    const double x = cx + std::cos(a) * d, y = cy + std::sin(a) * d;
    if (moldy)
    {
      for (int k = 0; k < 6; ++k)
      {
        const double fa = k * kPi / 3;
        cairo_move_to(cr, x, y);
        cairo_line_to(cr, x + std::cos(fa) * rad * 0.07, y + std::sin(fa) * rad * 0.07);
      }
      setRgba(cr, rgb(236, 246, 220), 0.7);
      cairo_set_line_width(cr, 1.0);
      cairo_stroke(cr);
    }
    else
    {
      cairo_save(cr);
      cairo_translate(cr, x, y);
      cairo_rotate(cr, a);
      cairo_rectangle(cr, -rad * 0.04, -rad * 0.025, rad * 0.08, rad * 0.05);
      cairo_restore(cr);
      setColor(cr, i % 2 ? rgb(60, 130, 40) : rgb(90, 160, 50));
      cairo_fill(cr);
    }
  }
  cairo_restore(cr);
  if (sauce)
  {
    // The sauce: a glossy cap over the top, dripping.
    cairo_new_path(cr);
    cairo_arc(cr, cx, cy, rad * 1.03, kPi * 1.08, kPi * 1.92);
    const double y0 = cy - rad * 0.2;
    cairo_curve_to(cr, cx + rad * 0.8, y0 + rad * 0.2, cx + rad * 0.6, y0 + rad * 0.05, cx + rad * 0.5, y0 + rad * 0.12);
    cairo_curve_to(cr, cx + rad * 0.45, y0 + rad * 0.38, cx + rad * 0.3, y0 + rad * 0.38, cx + rad * 0.28, y0 + rad * 0.1);
    cairo_curve_to(cr, cx + rad * 0.1, y0 - rad * 0.05, cx - rad * 0.05, y0 + rad * 0.2, cx - rad * 0.2, y0 + rad * 0.05);
    cairo_curve_to(cr, cx - rad * 0.3, y0 + rad * 0.28, cx - rad * 0.45, y0 + rad * 0.28, cx - rad * 0.5, y0 + rad * 0.05);
    cairo_curve_to(cr, cx - rad * 0.65, y0 + rad * 0.15, cx - rad * 0.8, y0 + rad * 0.15, cx - rad * 0.9, y0 + rad * 0.05);
    cairo_close_path(cr);
    fillLinear(cr, 0, cy - rad, 0, cy + rad * 0.2, lighten(kSauce, 0.25f), darken(kSauce, 0.25f), std::max(1.4, rad * 0.04));
    ellipse(cr, cx - rad * 0.3, cy - rad * 0.72, rad * 0.22, rad * 0.08);
    setRgba(cr, rgb(255, 230, 220), 0.85);
    cairo_fill(cr);
    // A basil leaf on top.
    cairo_save(cr);
    cairo_translate(cr, cx + rad * 0.15, cy - rad * 0.92);
    cairo_rotate(cr, -0.4);
    ellipse(cr, 0, 0, rad * 0.16, rad * 0.07);
    cairo_restore(cr);
    fillLinear(cr, 0, cy - rad, 0, cy - rad * 0.85, rgb(110, 200, 80), rgb(40, 110, 30), 1.2);
  }
  else if (moldy)
  {
    // Green slime on top, and a fur of mould at the edge.
    cairo_new_path(cr);
    cairo_arc(cr, cx, cy, rad * 1.02, kPi * 1.15, kPi * 1.85);
    cairo_curve_to(cr, cx + rad * 0.5, cy - rad * 0.3, cx - rad * 0.4, cy - rad * 0.2, cx - rad * 0.88, cy - rad * 0.45);
    cairo_close_path(cr);
    fillLinear(cr, 0, cy - rad, 0, cy - rad * 0.3, rgb(170, 230, 90), rgb(80, 140, 40), std::max(1.2, rad * 0.035));
    for (int i = 0; i < 28; ++i)
    {
      const double a = i * 2 * kPi / 28;
      cairo_move_to(cr, cx + std::cos(a) * rad * 0.96, cy + std::sin(a) * rad * 0.96);
      cairo_line_to(cr, cx + std::cos(a + 0.05) * rad * 1.1, cy + std::sin(a + 0.05) * rad * 1.1);
    }
    setRgba(cr, rgb(210, 230, 190), 0.7);
    cairo_set_line_width(cr, 1.2);
    cairo_stroke(cr);
  }
}

void paintCleaver(cairo_t* cr, double len)
{
  const double hl = len * 0.4, bh = len * 0.44, th = len * 0.11;
  // The handle: dark wood with three brass rivets.
  roundedRect(cr, 0, -bh * 0.5 + bh * 0.08, hl, th, th * 0.45);
  fillLinear(cr, 0, -bh * 0.5, 0, -bh * 0.5 + bh * 0.08 + th, rgb(150, 92, 52), rgb(70, 38, 20), std::max(1.2, len * 0.025));
  for (int i = 0; i < 3; ++i)
  {
    circle(cr, hl * (0.2 + i * 0.28), -bh * 0.5 + bh * 0.08 + th * 0.5, th * 0.17);
    setColor(cr, rgb(250, 220, 140));
    cairo_fill(cr);
  }
  // The blade: a broad slab, square at the back, the edge along the bottom.
  cairo_move_to(cr, hl - len * 0.02, -bh * 0.5);
  cairo_line_to(cr, len, -bh * 0.5);
  cairo_line_to(cr, len, bh * 0.38);
  cairo_curve_to(cr, len, bh * 0.5, len - len * 0.04, bh * 0.5, len - len * 0.08, bh * 0.5);
  cairo_line_to(cr, hl + len * 0.04, bh * 0.5);
  cairo_line_to(cr, hl - len * 0.02, bh * 0.1);
  cairo_close_path(cr);
  cairo_pattern_t* g = cairo_pattern_create_linear(0, -bh * 0.5, 0, bh * 0.5);
  stop(g, 0, rgb(170, 180, 194));
  stop(g, 0.45, rgb(232, 238, 246));
  stop(g, 0.8, rgb(150, 160, 176));
  stop(g, 1, rgb(255, 255, 255));
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  setColor(cr, kInk);
  cairo_set_line_width(cr, std::max(1.2, len * 0.025));
  cairo_stroke(cr);
  // The honed edge and the hanging hole.
  cairo_move_to(cr, hl + len * 0.05, bh * 0.4);
  cairo_line_to(cr, len - len * 0.06, bh * 0.4);
  setRgba(cr, rgb(255, 255, 255), 0.9);
  cairo_set_line_width(cr, std::max(1.0, len * 0.02));
  cairo_stroke(cr);
  circle(cr, len - len * 0.12, -bh * 0.28, len * 0.045);
  setColor(cr, kInk);
  cairo_fill(cr);
}

bool drawKitchenArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame)
{
  (void)t;
  if (key == "meatball")
    meatball(cr, w, h, variant, frame);
  else if (key == "cleaver_bot")
    cleaverBot(cr, w, h, variant, frame);
  else if (key == "egg_timer")
    eggTimer(cr, w, h, variant, frame);
  else if (key == "cleaver")
  {
    const double len = std::min(w, h) * 0.9;
    cairo_save(cr);
    cairo_translate(cr, kM + w * 0.5 - len * 0.5, kM + h * 0.5);
    paintCleaver(cr, len);
    cairo_restore(cr);
    if (frame % 2 == 0)
      glow(cr, kM + w * 0.5 + len * 0.3, kM + h * 0.5 - len * 0.1, len * 0.18, rgb(255, 255, 255), 0.8);
  }
  else if (key == "duck_chef")
  {
    // Over the item icon's duck: its head at about (38, 24).
    const double s = std::min(w, h) / 64.0;
    paintChefHat(cr, kM + 37 * s, kM + 15 * s, 26 * s, 26 * s, -0.12);
  }
  else
    return false;
  return true;
}

} // namespace gr
