// The Hydroponics domes' wildlife (Level 17) in the style of enemy_art.cpp:
// each drawn in screen pixels into a texture with a 32 px margin, the cell
// box at (32, 32) .. (32 + w, 32 + h), facing right.

#include "assets/enemy_art_green.hpp"

#include <algorithm>
#include <cmath>

namespace gr
{

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr Color kInk = rgb(16, 24, 18);
constexpr double kLine = 2.4;
constexpr double kM = 32.0;

void setRgba(cairo_t* cr, Color c, double a)
{
  cairo_set_source_rgba(cr, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, a);
}

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

// Fills the current path with a radial gradient lit from the upper left
// and outlines it.
void fillBall(cairo_t* cr, double x, double y, double r, Color light, Color dark, double alpha = 1.0)
{
  cairo_pattern_t* g = cairo_pattern_create_radial(x - r * 0.35, y - r * 0.4, r * 0.1, x, y, r * 1.1);
  cairo_pattern_add_color_stop_rgba(g, 0, redOf(light) / 255.0, greenOf(light) / 255.0, blueOf(light) / 255.0, alpha);
  cairo_pattern_add_color_stop_rgba(g, 1, redOf(dark) / 255.0, greenOf(dark) / 255.0, blueOf(dark) / 255.0, alpha);
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  setColor(cr, kInk);
  cairo_set_line_width(cr, kLine);
  cairo_stroke(cr);
}

void fillShaded(cairo_t* cr, double y0, double y1, Color top, Color bottom)
{
  cairo_pattern_t* g = cairo_pattern_create_linear(0, y0, 0, y1);
  cairo_pattern_add_color_stop_rgb(g, 0, redOf(top) / 255.0, greenOf(top) / 255.0, blueOf(top) / 255.0);
  cairo_pattern_add_color_stop_rgb(g, 1, redOf(bottom) / 255.0, greenOf(bottom) / 255.0, blueOf(bottom) / 255.0);
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  setColor(cr, kInk);
  cairo_set_line_width(cr, kLine);
  cairo_stroke(cr);
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

// A pointed leaf from (x, y) along angle a.
void leaf(cairo_t* cr, double x, double y, double a, double len, double width, Color light, Color dark)
{
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_rotate(cr, a);
  cairo_move_to(cr, 0, 0);
  cairo_curve_to(cr, len * 0.3, -width, len * 0.75, -width * 0.8, len, 0);
  cairo_curve_to(cr, len * 0.75, width * 0.8, len * 0.3, width, 0, 0);
  cairo_close_path(cr);
  cairo_pattern_t* g = cairo_pattern_create_linear(0, -width, 0, width);
  cairo_pattern_add_color_stop_rgb(g, 0, redOf(light) / 255.0, greenOf(light) / 255.0, blueOf(light) / 255.0);
  cairo_pattern_add_color_stop_rgb(g, 1, redOf(dark) / 255.0, greenOf(dark) / 255.0, blueOf(dark) / 255.0);
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  setColor(cr, kInk);
  cairo_set_line_width(cr, 1.8);
  cairo_stroke(cr);
  cairo_move_to(cr, len * 0.06, 0);
  cairo_line_to(cr, len * 0.85, 0);
  setRgba(cr, dark, 0.9);
  cairo_set_line_width(cr, 1.4);
  cairo_stroke(cr);
  cairo_restore(cr);
}

// A virus speck: a small dark-green ball with knobbed spikes.
void virusSpeck(cairo_t* cr, double x, double y, double r)
{
  for (int k = 0; k < 6; ++k)
  {
    const double a = k * kPi / 3.0 + 0.3;
    cairo_move_to(cr, x + std::cos(a) * r, y + std::sin(a) * r);
    cairo_line_to(cr, x + std::cos(a) * r * 1.7, y + std::sin(a) * r * 1.7);
  }
  setColor(cr, rgb(40, 90, 20));
  cairo_set_line_width(cr, 1.6);
  cairo_stroke(cr);
  circle(cr, x, y, r);
  setColor(cr, rgb(60, 130, 30));
  cairo_fill(cr);
  circle(cr, x - r * 0.3, y - r * 0.3, r * 0.35);
  setColor(cr, rgb(190, 255, 120));
  cairo_fill(cr);
}

// Spore Puffer: a bulbous fungus pod on a stalk, rooted in a mound of
// mycelium. Variant 0: idle; 1: inflating (swollen, glowing veins); 2: just
// puffed (deflated, spores in the air). +3: a carrier (sickly green, virus
// specks). +6: hanging from a ceiling (upside down).
void sporePuffer(cairo_t* cr, double w, double h, int variant, int frame)
{
  const int state = variant % 3;
  const bool carrier = (variant / 3) % 2 == 1;
  const bool hanging = variant >= 6;
  cairo_save(cr);
  if (hanging)
  {
    cairo_translate(cr, 0, 2 * kM + h);
    cairo_scale(cr, 1, -1);
  }
  const Color podLight = carrier ? rgb(200, 236, 110) : rgb(236, 150, 230);
  const Color podDark = carrier ? rgb(70, 110, 30) : rgb(110, 40, 120);
  const Color spot = carrier ? rgb(240, 255, 190) : rgb(255, 230, 200);
  const Color sporeCol = carrier ? rgb(170, 255, 90) : rgb(255, 220, 160);
  const double cx = kM + w * 0.5, base = kM + h;
  // The mound of mycelium and moss it grows out of.
  ellipse(cr, cx, base - h * 0.04, w * 0.36, h * 0.09);
  fillShaded(cr, base - h * 0.13, base, rgb(120, 96, 70), rgb(60, 44, 32));
  for (int k = 0; k < 5; ++k)
  {
    cairo_move_to(cr, cx - w * 0.3 + k * w * 0.15, base - h * 0.02);
    cairo_curve_to(cr, cx - w * 0.28 + k * w * 0.15, base - h * 0.08, cx - w * 0.22 + k * w * 0.15, base - h * 0.06,
      cx - w * 0.2 + k * w * 0.15, base - h * 0.1);
  }
  setRgba(cr, rgb(240, 230, 200), 0.7);
  cairo_set_line_width(cr, 1.5);
  cairo_stroke(cr);
  leaf(cr, cx - w * 0.12, base - h * 0.08, kPi + 0.5, w * 0.24, 7, rgb(130, 210, 90), rgb(50, 110, 40));
  leaf(cr, cx + w * 0.1, base - h * 0.08, -0.45, w * 0.26, 7, rgb(130, 210, 90), rgb(50, 110, 40));

  // Pod size by state.
  double rx = w * 0.3, ry = h * 0.26;
  double py = kM + h * 0.36;
  if (state == 1)
  {
    rx *= 1.2;
    ry *= 1.22;
    py -= h * 0.02;
  }
  else if (state == 2)
  {
    rx *= 0.86;
    ry *= 0.66;
    py += h * 0.08;
  }
  // The stalk, a little bent.
  cairo_move_to(cr, cx - 8, base - h * 0.08);
  cairo_curve_to(cr, cx - 12, base - h * 0.3, cx - 2, py + ry * 0.8, cx - 6, py + ry * 0.6);
  cairo_line_to(cr, cx + 6, py + ry * 0.6);
  cairo_curve_to(cr, cx + 10, py + ry * 0.8, cx + 2, base - h * 0.3, cx + 8, base - h * 0.08);
  cairo_close_path(cr);
  fillShaded(cr, py, base, carrier ? rgb(220, 230, 170) : rgb(250, 236, 214), carrier ? rgb(150, 160, 100) : rgb(190, 160, 140));
  // The gill skirt under the pod.
  ellipse(cr, cx, py + ry * 0.72, rx * 0.62, ry * 0.18);
  fillShaded(cr, py + ry * 0.55, py + ry * 0.9, carrier ? rgb(230, 240, 180) : rgb(255, 214, 230),
    carrier ? rgb(150, 170, 90) : rgb(190, 120, 160));
  if (state == 1)
    glow(cr, cx, py, rx * 1.5, carrier ? rgb(170, 255, 80) : rgb(255, 120, 230), 0.45);
  // The pod.
  if (state == 2)
  {
    // Deflated: a wrinkled, sagging sack.
    cairo_move_to(cr, cx - rx, py + ry * 0.4);
    cairo_curve_to(cr, cx - rx * 1.1, py - ry * 0.6, cx - rx * 0.3, py - ry * 1.1, cx, py - ry * 0.85);
    cairo_curve_to(cr, cx + rx * 0.3, py - ry * 1.15, cx + rx * 1.1, py - ry * 0.5, cx + rx, py + ry * 0.4);
    cairo_curve_to(cr, cx + rx * 0.5, py + ry * 0.75, cx - rx * 0.5, py + ry * 0.75, cx - rx, py + ry * 0.4);
    cairo_close_path(cr);
    fillBall(cr, cx, py, std::max(rx, ry), podLight, podDark);
    for (int k = -2; k <= 2; ++k)
    {
      cairo_move_to(cr, cx + k * rx * 0.32, py - ry * 0.6);
      cairo_curve_to(cr, cx + k * rx * 0.36 + 4, py - ry * 0.1, cx + k * rx * 0.3 - 4, py + ry * 0.2, cx + k * rx * 0.34,
        py + ry * 0.5);
    }
    setRgba(cr, podDark, 0.8);
    cairo_set_line_width(cr, 2);
    cairo_stroke(cr);
  }
  else
  {
    ellipse(cr, cx, py, rx, ry);
    fillBall(cr, cx, py, std::max(rx, ry), podLight, podDark);
    if (state == 1)
    {
      // Veins standing out on the swollen skin.
      for (int k = 0; k < 6; ++k)
      {
        const double a = kPi * (1.1 + 0.16 * k);
        cairo_move_to(cr, cx + std::cos(a) * rx * 0.15, py - ry * 0.85);
        cairo_curve_to(cr, cx + std::cos(a) * rx * 0.6, py + std::sin(a) * ry * 0.2, cx + std::cos(a) * rx * 0.8,
          py + ry * 0.2, cx + std::cos(a) * rx * 0.95, py + ry * 0.5);
      }
      setRgba(cr, carrier ? rgb(230, 255, 150) : rgb(255, 210, 250), 0.75);
      cairo_set_line_width(cr, 2.2);
      cairo_stroke(cr);
    }
  }
  // Spots, the pore on top, and a shine.
  const double spots[][3] = {{-0.5, -0.1, 0.13}, {0.35, -0.35, 0.11}, {0.1, 0.25, 0.09}, {-0.15, -0.55, 0.08},
    {0.6, 0.15, 0.08}};
  for (const auto& s : spots)
  {
    ellipse(cr, cx + s[0] * rx, py + s[1] * ry, s[2] * rx * (state == 2 ? 0.8 : 1.0), s[2] * ry);
    setRgba(cr, spot, state == 2 ? 0.55 : 0.85);
    cairo_fill(cr);
  }
  ellipse(cr, cx, py - ry * (state == 2 ? 0.86 : 0.97), rx * 0.2, ry * 0.08);
  setColor(cr, carrier ? rgb(40, 60, 16) : rgb(60, 16, 60));
  cairo_fill(cr);
  if (state != 2)
  {
    ellipse(cr, cx - rx * 0.42, py - ry * 0.45, rx * 0.16, ry * 0.1);
    setRgba(cr, rgb(255, 255, 255), 0.55);
    cairo_fill(cr);
  }
  if (carrier)
  {
    virusSpeck(cr, cx + rx * 0.45, py - ry * 0.05, 3.2);
    virusSpeck(cr, cx - rx * 0.3, py + ry * 0.35, 2.8);
    virusSpeck(cr, cx - rx * 0.1, py - ry * 0.6, 2.6);
  }
  if (state == 1)
  {
    // A wisp leaking from the pore.
    const double top = py - ry;
    for (int k = 0; k < 4; ++k)
    {
      circle(cr, cx + (k % 2 ? 6 : -5) + frame * 2, top - 6 - k * 7.0, 3.5 - k * 0.5);
      setRgba(cr, sporeCol, 0.75 - k * 0.15);
      cairo_fill(cr);
    }
  }
  else if (state == 2)
  {
    // The puff: a cloud of spores rising off the pore.
    const double top = py - ry;
    for (int k = 0; k < 7; ++k)
    {
      const double a = kPi * (1.1 + 0.8 * k / 6.0);
      const double d = 18 + (k % 3) * 9 + frame * 3;
      circle(cr, cx + std::cos(a) * d * 1.3, top + std::sin(a) * d * 0.9 - 4, 9 - (k % 3) * 2);
      setRgba(cr, sporeCol, 0.32);
      cairo_fill(cr);
    }
    for (int k = 0; k < 16; ++k)
    {
      const double a = kPi * (1.0 + k / 15.0);
      const double d = 10 + (k * 7) % 30 + frame * 3;
      circle(cr, cx + std::cos(a) * d * 1.4, top + std::sin(a) * d - 2, 2.0);
      setRgba(cr, carrier ? rgb(120, 220, 40) : rgb(255, 250, 220), 0.9);
      cairo_fill(cr);
    }
  }
  cairo_restore(cr);
}

// One lobe of a Snapjaw's trap, hinged at the origin, opening to the right
// (side +1) or left (-1), `len` long; teeth along its rim (the x = 0 edge).
void trapLobe(cairo_t* cr, double side, double len, double width, double angle, bool showInside)
{
  cairo_save(cr);
  cairo_rotate(cr, angle * side);
  cairo_scale(cr, side, 1);
  // The outside: a D bulging outward.
  cairo_move_to(cr, 0, 0);
  cairo_curve_to(cr, width * 1.25, -len * 0.05, width * 1.2, -len * 0.95, 0, -len);
  cairo_close_path(cr);
  fillShaded(cr, -len, 0, rgb(150, 220, 90), rgb(50, 120, 40));
  if (showInside)
  {
    // The red inner face along the rim.
    cairo_move_to(cr, 0, -len * 0.04);
    cairo_curve_to(cr, width * 0.55, -len * 0.12, width * 0.55, -len * 0.88, 0, -len * 0.96);
    cairo_close_path(cr);
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 0, width * 0.5, 0);
    cairo_pattern_add_color_stop_rgb(g, 0, 1.0, 0.45, 0.5);
    cairo_pattern_add_color_stop_rgb(g, 1, 0.65, 0.08, 0.2);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
    for (int k = 0; k < 3; ++k)
    {
      circle(cr, width * 0.2, -len * (0.3 + 0.2 * k), 2.2);
      setRgba(cr, rgb(255, 230, 120), 0.9);
      cairo_fill(cr);
    }
  }
  // Teeth along the rim.
  const int teeth = 7;
  for (int k = 0; k < teeth; ++k)
  {
    const double y = -len * (0.1 + 0.85 * k / (teeth - 1));
    cairo_move_to(cr, 0, y - 4);
    cairo_line_to(cr, -11, y - 1);
    cairo_line_to(cr, 0, y + 3);
    cairo_close_path(cr);
  }
  setColor(cr, rgb(250, 248, 220));
  cairo_fill_preserve(cr);
  setColor(cr, kInk);
  cairo_set_line_width(cr, 1.2);
  cairo_stroke(cr);
  // Rim line.
  cairo_move_to(cr, 0, 0);
  cairo_line_to(cr, 0, -len);
  setColor(cr, rgb(200, 40, 60));
  cairo_set_line_width(cr, 3);
  cairo_stroke(cr);
  cairo_restore(cr);
}

// Snapjaw: a flytrap-like mouth on a stalk, leaves round its root. Variant
// by state: 0 asleep (closed), 1 twitching (ajar), 2 biting (wide open);
// plus 3 * orientation: 0 floor (mouth up), 1 ceiling (mouth down), 2 on a
// wall to its left (mouth right), 3 on a wall to its right (mouth left).
void snapjaw(cairo_t* cr, double w, double h, int variant, int frame)
{
  const int state = variant % 3;
  const int orient = (variant / 3) % 4;
  const double cx = kM + w * 0.5, cy = kM + h * 0.5, base = kM + h;
  cairo_save(cr);
  cairo_translate(cr, cx, cy);
  cairo_rotate(cr, orient == 1 ? kPi : (orient == 2 ? kPi * 0.5 : (orient == 3 ? -kPi * 0.5 : 0.0)));
  cairo_translate(cr, -cx, -cy);
  // Root leaves.
  leaf(cr, cx - 4, base - 4, kPi + 0.35, w * 0.38, 9, rgb(120, 200, 80), rgb(40, 100, 36));
  leaf(cr, cx + 4, base - 4, -0.35, w * 0.38, 9, rgb(120, 200, 80), rgb(40, 100, 36));
  leaf(cr, cx - 2, base - 6, kPi + 1.0, w * 0.26, 7, rgb(140, 220, 90), rgb(50, 110, 40));
  // The stalk, swaying a little when twitching.
  const double sway = state == 1 ? (frame % 2 ? 4.0 : -4.0) : 0.0;
  const double hingeY = base - h * (state == 2 ? 0.44 : 0.38);
  cairo_move_to(cr, cx - 6, base - 4);
  cairo_curve_to(cr, cx - 10, base - h * 0.2, cx - 6 + sway, hingeY + 18, cx - 5 + sway, hingeY);
  cairo_line_to(cr, cx + 5 + sway, hingeY);
  cairo_curve_to(cr, cx + 6 + sway, hingeY + 18, cx + 10, base - h * 0.2, cx + 6, base - 4);
  cairo_close_path(cr);
  fillShaded(cr, hingeY, base, rgb(110, 190, 70), rgb(40, 90, 30));
  // The trap.
  const double len = h * 0.5, width = w * 0.24;
  const double open = state == 0 ? 0.0 : (state == 1 ? 0.22 : 0.62);
  cairo_save(cr);
  cairo_translate(cr, cx + sway, hingeY + 2);
  if (state == 2)
    glow(cr, 0, -len * 0.5, len * 0.6, rgb(255, 80, 100), 0.35);
  trapLobe(cr, -1, len, width, open, state > 0);
  trapLobe(cr, 1, len, width, open, state > 0);
  // The hinge knot.
  ellipse(cr, 0, 0, 9, 6);
  fillShaded(cr, -6, 6, rgb(150, 210, 90), rgb(60, 110, 40));
  // Where an asleep trap's Zz goes, kept upright whatever its orientation.
  double zx = width + 12, zy = -len * 0.9;
  cairo_user_to_device(cr, &zx, &zy);
  if (state == 2)
  {
    // Drool, and snap lines.
    ellipse(cr, 0, -len * 0.2, 3, 6);
    setRgba(cr, rgb(220, 255, 200), 0.8);
    cairo_fill(cr);
    for (int k = -1; k <= 1; k += 2)
    {
      cairo_move_to(cr, k * len * 0.75, -len * 0.95);
      cairo_line_to(cr, k * len * 0.95, -len * 1.1);
      cairo_move_to(cr, k * len * 0.85, -len * 0.75);
      cairo_line_to(cr, k * len * 1.05, -len * 0.8);
    }
    setRgba(cr, rgb(255, 255, 255), 0.85);
    cairo_set_line_width(cr, 2.5);
    cairo_stroke(cr);
  }
  cairo_restore(cr);
  cairo_restore(cr);
  if (state == 0)
  {
    cairo_device_to_user(cr, &zx, &zy);
    zy -= (frame % 2) * 3;
    cairo_set_line_width(cr, 2);
    setRgba(cr, rgb(230, 255, 230), 0.85);
    for (int k = 0; k < 2; ++k)
    {
      const double s = 8 - k * 3, x = zx + k * 10, y = zy - k * 10 - 4;
      cairo_move_to(cr, x - s * 0.5, y - s * 0.5);
      cairo_line_to(cr, x + s * 0.5, y - s * 0.5);
      cairo_line_to(cr, x - s * 0.5, y + s * 0.5);
      cairo_line_to(cr, x + s * 0.5, y + s * 0.5);
    }
    cairo_stroke(cr);
  }
}

// Glob: a translucent green slime blob with a nucleus and two eyes. Variant
// 0: idle (a dome); 1: squashed flat before a hop; 2: airborne (stretched).
void glob(cairo_t* cr, double w, double h, int variant, int frame)
{
  const double cx = kM + w * 0.5, base = kM + h;
  double rx = w * 0.46, top = kM + h * 0.18;
  if (variant == 1)
  {
    rx = w * 0.54;
    top = kM + h * 0.5;
  }
  else if (variant == 2)
  {
    rx = w * 0.34;
    top = kM + h * 0.02;
  }
  const double wob = (frame % 2 ? 1.0 : -1.0) * w * 0.01;
  const double bottom = variant == 2 ? base - h * 0.08 : base - 1;
  const double hgt = bottom - top;
  auto body = [&]() {
    cairo_move_to(cr, cx - rx, bottom);
    if (variant == 2)
    {
      // A teardrop pulled up, the tip trailing below.
      cairo_curve_to(cr, cx - rx * 1.1, bottom - hgt * 0.6, cx - rx * 0.6 + wob, top, cx + wob, top);
      cairo_curve_to(cr, cx + rx * 0.6 + wob, top, cx + rx * 1.1, bottom - hgt * 0.6, cx + rx, bottom);
      cairo_curve_to(cr, cx + rx * 0.5, bottom + h * 0.06, cx - rx * 0.5, bottom + h * 0.06, cx - rx, bottom);
    }
    else
    {
      cairo_curve_to(cr, cx - rx * 1.05, bottom - hgt * 0.8, cx - rx * 0.55 + wob, top, cx + wob, top);
      cairo_curve_to(cr, cx + rx * 0.55 + wob, top, cx + rx * 1.05, bottom - hgt * 0.8, cx + rx, bottom);
      cairo_close_path(cr);
    }
    cairo_close_path(cr);
  };
  // Shadow puddle.
  if (variant != 2)
  {
    ellipse(cr, cx, bottom, rx * 1.02, h * 0.04);
    setRgba(cr, rgb(30, 90, 30), 0.5);
    cairo_fill(cr);
  }
  const double my = top + hgt * 0.55;
  glow(cr, cx, my, std::max(rx, hgt * 0.6) * 1.15, rgb(120, 255, 100), 0.25);
  body();
  cairo_pattern_t* g = cairo_pattern_create_radial(cx - rx * 0.3, top + hgt * 0.3, rx * 0.05, cx, my, rx * 1.2);
  cairo_pattern_add_color_stop_rgba(g, 0, 0.82, 1.0, 0.7, 0.75);
  cairo_pattern_add_color_stop_rgba(g, 0.6, 0.35, 0.85, 0.3, 0.62);
  cairo_pattern_add_color_stop_rgba(g, 1, 0.12, 0.5, 0.15, 0.8);
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  setColor(cr, rgb(20, 70, 24));
  cairo_set_line_width(cr, kLine);
  cairo_stroke(cr);
  // Inside: bubbles and the nucleus.
  const double s = w / 128.0;
  const double bubbles[][3] = {{-0.5, 0.75, 5}, {0.45, 0.8, 4}, {-0.2, 0.9, 3}, {0.55, 0.5, 3}};
  for (const auto& b : bubbles)
  {
    circle(cr, cx + b[0] * rx, top + b[1] * hgt, b[2] * s);
    setRgba(cr, rgb(230, 255, 210), 0.55);
    cairo_set_line_width(cr, 1.4);
    cairo_stroke(cr);
  }
  const double nx = cx - rx * 0.25, ny = top + hgt * 0.68;
  ellipse(cr, nx, ny, rx * 0.26, hgt * 0.16);
  cairo_pattern_t* n = cairo_pattern_create_radial(nx - 3, ny - 3, 1, nx, ny, rx * 0.3);
  cairo_pattern_add_color_stop_rgba(n, 0, 0.75, 0.95, 0.4, 0.95);
  cairo_pattern_add_color_stop_rgba(n, 1, 0.2, 0.5, 0.12, 0.9);
  cairo_set_source(cr, n);
  cairo_fill(cr);
  cairo_pattern_destroy(n);
  circle(cr, nx + 2 * s, ny, std::max(2.0, rx * 0.07));
  setRgba(cr, rgb(30, 80, 20), 0.8);
  cairo_fill(cr);
  // Eyes, looking ahead (right).
  const double ey = top + hgt * (variant == 1 ? 0.35 : 0.32);
  const double er = std::max(4.0, w * 0.075) * (variant == 1 ? 0.85 : 1.0);
  for (int k = 0; k < 2; ++k)
  {
    const double ex = cx + rx * (0.05 + 0.4 * k);
    ellipse(cr, ex, ey, er, er * (variant == 1 ? 0.7 : 1.15));
    setColor(cr, rgb(250, 255, 245));
    cairo_fill_preserve(cr);
    setColor(cr, rgb(20, 60, 20));
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);
    circle(cr, ex + er * 0.35, ey + (variant == 2 ? -er * 0.3 : er * 0.1), er * 0.5);
    setColor(cr, rgb(10, 20, 10));
    cairo_fill(cr);
    circle(cr, ex + er * 0.15, ey - er * 0.25, er * 0.18);
    setColor(cr, rgb(255, 255, 255));
    cairo_fill(cr);
  }
  // The shine on top.
  ellipse(cr, cx - rx * 0.45, top + hgt * 0.22, rx * 0.14, hgt * 0.08);
  setRgba(cr, rgb(255, 255, 255), 0.7);
  cairo_fill(cr);
  if (variant == 1)
  {
    // Splats squeezed out at the sides.
    for (int k = -1; k <= 1; k += 2)
    {
      ellipse(cr, cx + k * rx * 1.12, bottom - 4 * s, 5 * s, 3 * s);
      setRgba(cr, rgb(120, 230, 90), 0.8);
      cairo_fill(cr);
    }
  }
  if (variant == 2)
  {
    // Drips falling off the tip.
    for (int k = 0; k < 2; ++k)
    {
      ellipse(cr, cx + (k ? 6 : -8) * s, bottom + h * (0.08 + 0.05 * k), 3 * s, 4.5 * s);
      setRgba(cr, rgb(120, 230, 90), 0.75);
      cairo_fill(cr);
    }
  }
}

} // namespace

bool drawGreenArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame)
{
  (void)t;
  if (key == "spore_puffer")
    sporePuffer(cr, w, h, variant, frame);
  else if (key == "snapjaw")
    snapjaw(cr, w, h, variant, frame);
  else if (key == "glob" || key == "glob_medium" || key == "glob_small")
    glob(cr, w, h, variant, frame);
  else
    return false;
  return true;
}

} // namespace gr
