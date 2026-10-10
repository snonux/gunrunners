// Vector art for the vehicles (world_vehicle.cpp) and the deep water's
// residents (world_sea.cpp). Same contract as the styled enemies in
// enemy_art.cpp: draw facing right into a w x h box with a 32 px margin;
// the sprite is anchored at the bottom centre of the box. Parts that move on
// their own (the tank's barrel, the rotor, the riders) are drawn by the
// game over these.

#include "assets/vehicle_art.hpp"

#include "render/vector.hpp"

#include <cmath>

namespace gr
{

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr Color kInk = rgb(26, 20, 38);
constexpr double kLine = 2.4;
constexpr double kM = 32.0;

Color lighten(Color c, float t) { return lerpColor(c, rgb(255, 255, 255), t); }
Color darken(Color c, float t) { return lerpColor(c, rgb(0, 0, 0), t); }
void circle(cairo_t* cr, double x, double y, double r) { cairo_arc(cr, x, y, r, 0, 2 * kPi); }

void rivets(cairo_t* cr, double x0, double x1, double y, double gap)
{
  for (double x = x0; x <= x1; x += gap)
  {
    circle(cr, x, y, 2.2);
    setColor(cr, rgba(255, 255, 255, 150));
    cairo_fill(cr);
  }
}

// Tank: treads, a sloped hull, a turret (the barrel is drawn by the game).
void tank(cairo_t* cr, double w, double h, int variant, int frame)
{
  const double x0 = kM, y0 = kM;
  const Color olive = variant == 1 ? rgb(170, 150, 110) : rgb(110, 140, 76);
  // Treads.
  const double ty = y0 + h * 0.66, th = h * 0.34;
  roundedRect(cr, x0 + 2, ty, w - 4, th - 2, th * 0.45);
  fillOutline(cr, rgb(48, 46, 52), kInk, kLine);
  for (int i = 0; i < 6; ++i)
  {
    const double wx = x0 + w * (0.12 + 0.152 * i);
    circle(cr, wx, ty + th * 0.48, th * 0.3);
    fillOutline(cr, rgb(96, 96, 104), kInk, 1.4);
    circle(cr, wx, ty + th * 0.48, th * 0.1);
    setColor(cr, rgb(170, 170, 176));
    cairo_fill(cr);
  }
  for (double lx = x0 + 8 + (frame % 2) * 9.0; lx < x0 + w - 8; lx += 18.0)
  {
    cairo_rectangle(cr, lx, ty - 1, 7, 5);
    setColor(cr, rgb(80, 78, 86));
    cairo_fill(cr);
    cairo_rectangle(cr, lx, ty + th - 6, 7, 4);
    cairo_fill(cr);
  }
  // Hull.
  cairo_move_to(cr, x0 + w * 0.04, ty + 4);
  cairo_line_to(cr, x0 + w * 0.12, y0 + h * 0.42);
  cairo_line_to(cr, x0 + w * 0.86, y0 + h * 0.42);
  cairo_line_to(cr, x0 + w * 0.99, ty + 4);
  cairo_close_path(cr);
  fillGradientOutline(cr, y0 + h * 0.42, ty + 4, lighten(olive, 0.2f), darken(olive, 0.3f), kInk, kLine);
  rivets(cr, x0 + w * 0.16, x0 + w * 0.84, y0 + h * 0.5, 16);
  // A stripe and a star.
  cairo_rectangle(cr, x0 + w * 0.12, y0 + h * 0.56, w * 0.76, 5);
  setColor(cr, rgba(255, 220, 90, 200));
  cairo_fill(cr);
  // Turret.
  cairo_move_to(cr, x0 + w * 0.3, y0 + h * 0.43);
  cairo_curve_to(cr, x0 + w * 0.32, y0 + h * 0.12, x0 + w * 0.68, y0 + h * 0.12, x0 + w * 0.72, y0 + h * 0.43);
  cairo_close_path(cr);
  fillGradientOutline(cr, y0 + h * 0.15, y0 + h * 0.43, lighten(olive, 0.35f), darken(olive, 0.15f), kInk, kLine);
  // Open hatch.
  roundedRect(cr, x0 + w * 0.42, y0 + h * 0.13, w * 0.16, h * 0.06, 3);
  fillOutline(cr, darken(olive, 0.4f), kInk, 1.6);
  circle(cr, x0 + w * 0.62, y0 + h * 0.3, 5);
  setColor(cr, rgb(255, 220, 90));
  cairo_fill(cr);
}

// Helicopter: a glass bubble at the front, the boom and tail rotor behind,
// skids under it. The main rotor is drawn by the game.
void heli(cairo_t* cr, double w, double h, int /*variant*/, int frame)
{
  const double x0 = kM, y0 = kM;
  const Color paint = rgb(240, 180, 40), dark = rgb(150, 90, 20);
  // Tail boom and fin.
  cairo_move_to(cr, x0 + w * 0.42, y0 + h * 0.36);
  cairo_line_to(cr, x0 + w * 0.06, y0 + h * 0.3);
  cairo_line_to(cr, x0 + w * 0.06, y0 + h * 0.42);
  cairo_line_to(cr, x0 + w * 0.42, y0 + h * 0.6);
  cairo_close_path(cr);
  fillGradientOutline(cr, y0 + h * 0.3, y0 + h * 0.6, lighten(paint, 0.1f), dark, kInk, kLine);
  cairo_move_to(cr, x0 + w * 0.03, y0 + h * 0.08);
  cairo_line_to(cr, x0 + w * 0.1, y0 + h * 0.08);
  cairo_line_to(cr, x0 + w * 0.12, y0 + h * 0.36);
  cairo_line_to(cr, x0 + w * 0.03, y0 + h * 0.36);
  cairo_close_path(cr);
  fillOutline(cr, paint, kInk, kLine);
  // Tail rotor.
  const double a = frame % 2 ? 0.5 : -0.5;
  cairo_save(cr);
  cairo_translate(cr, x0 + w * 0.065, y0 + h * 0.24);
  cairo_rotate(cr, a);
  roundedRect(cr, -3, -h * 0.2, 6, h * 0.4, 3);
  setColor(cr, rgba(60, 60, 70, 200));
  cairo_fill(cr);
  cairo_restore(cr);
  // Cabin.
  cairo_move_to(cr, x0 + w * 0.36, y0 + h * 0.22);
  cairo_curve_to(cr, x0 + w * 0.6, y0 + h * 0.12, x0 + w * 0.94, y0 + h * 0.18, x0 + w * 0.98, y0 + h * 0.52);
  cairo_curve_to(cr, x0 + w * 0.98, y0 + h * 0.72, x0 + w * 0.7, y0 + h * 0.8, x0 + w * 0.44, y0 + h * 0.76);
  cairo_curve_to(cr, x0 + w * 0.32, y0 + h * 0.7, x0 + w * 0.32, y0 + h * 0.3, x0 + w * 0.36, y0 + h * 0.22);
  cairo_close_path(cr);
  fillGradientOutline(cr, y0 + h * 0.15, y0 + h * 0.8, lighten(paint, 0.25f), dark, kInk, kLine);
  // Glass (see-through: the pilot is drawn behind it).
  cairo_move_to(cr, x0 + w * 0.66, y0 + h * 0.2);
  cairo_curve_to(cr, x0 + w * 0.86, y0 + h * 0.2, x0 + w * 0.95, y0 + h * 0.34, x0 + w * 0.96, y0 + h * 0.52);
  cairo_line_to(cr, x0 + w * 0.66, y0 + h * 0.52);
  cairo_close_path(cr);
  fillOutline(cr, rgba(150, 220, 255, 110), kInk, 1.8);
  cairo_move_to(cr, x0 + w * 0.72, y0 + h * 0.25);
  cairo_line_to(cr, x0 + w * 0.82, y0 + h * 0.24);
  cairo_set_line_width(cr, 3);
  setColor(cr, rgba(255, 255, 255, 170));
  cairo_stroke(cr);
  // Mast.
  cairo_rectangle(cr, x0 + w * 0.53, y0 + h * 0.02, w * 0.04, h * 0.16);
  fillOutline(cr, rgb(70, 70, 80), kInk, 1.4);
  // Skids.
  for (double sx : {0.48, 0.82})
    strokeLimb(cr, {{x0 + w * sx, y0 + h * 0.74}, {x0 + w * (sx - 0.04), y0 + h * 0.95}}, 4, rgb(70, 70, 80), kInk, 1.2);
  strokeLimb(cr, {{x0 + w * 0.36, y0 + h * 0.95}, {x0 + w * 0.94, y0 + h * 0.95}}, 5, rgb(70, 70, 80), kInk, 1.2);
  // Nose light and the show's logo.
  radialGlow(cr, x0 + w * 0.97, y0 + h * 0.62, 10, rgb(255, 80, 80), 0.9);
  selectGameFont(cr);
  cairo_set_font_size(cr, h * 0.16);
  cairo_move_to(cr, x0 + w * 0.42, y0 + h * 0.66);
  setColor(cr, rgba(40, 20, 10, 200));
  cairo_show_text(cr, "MAX-TV");
}

// Hoverbike: a low fairing, a windscreen and two glowing hover pads.
void bike(cairo_t* cr, double w, double h, int /*variant*/, int frame)
{
  const double x0 = kM, y0 = kM;
  const Color paint = rgb(255, 60, 150);
  // Hover pads.
  for (double px : {0.2, 0.74})
  {
    radialGlow(cr, x0 + w * px, y0 + h * 0.96, 26 + (frame % 2) * 4, rgb(80, 230, 255), 0.85);
    roundedRect(cr, x0 + w * (px - 0.11), y0 + h * 0.7, w * 0.22, h * 0.2, 6);
    fillOutline(cr, rgb(50, 50, 64), kInk, kLine);
    cairo_rectangle(cr, x0 + w * (px - 0.08), y0 + h * 0.86, w * 0.16, 4);
    setColor(cr, rgb(150, 250, 255));
    cairo_fill(cr);
  }
  // Fairing.
  cairo_move_to(cr, x0 + w * 0.02, y0 + h * 0.5);
  cairo_curve_to(cr, x0 + w * 0.1, y0 + h * 0.26, x0 + w * 0.5, y0 + h * 0.3, x0 + w * 0.72, y0 + h * 0.28);
  cairo_curve_to(cr, x0 + w * 0.9, y0 + h * 0.26, x0 + w * 1.0, y0 + h * 0.46, x0 + w * 0.98, y0 + h * 0.58);
  cairo_curve_to(cr, x0 + w * 0.8, y0 + h * 0.78, x0 + w * 0.2, y0 + h * 0.8, x0 + w * 0.02, y0 + h * 0.66);
  cairo_close_path(cr);
  fillGradientOutline(cr, y0 + h * 0.26, y0 + h * 0.8, lighten(paint, 0.3f), darken(paint, 0.35f), kInk, kLine);
  // Racing stripe and windscreen.
  cairo_move_to(cr, x0 + w * 0.1, y0 + h * 0.52);
  cairo_line_to(cr, x0 + w * 0.92, y0 + h * 0.5);
  cairo_set_line_width(cr, 4);
  setColor(cr, rgb(255, 230, 90));
  cairo_stroke(cr);
  cairo_move_to(cr, x0 + w * 0.7, y0 + h * 0.3);
  cairo_line_to(cr, x0 + w * 0.8, y0 + h * 0.02);
  cairo_line_to(cr, x0 + w * 0.88, y0 + h * 0.06);
  cairo_line_to(cr, x0 + w * 0.84, y0 + h * 0.3);
  cairo_close_path(cr);
  fillOutline(cr, rgba(150, 230, 255, 140), kInk, 1.6);
  // Headlight and exhaust.
  radialGlow(cr, x0 + w * 0.98, y0 + h * 0.5, 14, rgb(255, 250, 200), 0.9);
  radialGlow(cr, x0 + w * 0.0, y0 + h * 0.58, 12 + (frame % 2) * 6, rgb(255, 120, 60), 0.8);
}

// Submarine: a yellow hull, a conning tower, portholes and a propeller.
void sub(cairo_t* cr, double w, double h, int /*variant*/, int frame)
{
  const double x0 = kM, y0 = kM;
  const Color paint = rgb(250, 200, 40), dark = rgb(170, 110, 20);
  // Propeller.
  cairo_save(cr);
  cairo_translate(cr, x0 + w * 0.04, y0 + h * 0.62);
  cairo_scale(cr, 0.35, 1.0);
  for (int k = 0; k < 2; ++k)
  {
    const double a = (frame % 4) * kPi / 4 + k * kPi;
    cairo_move_to(cr, 0, 0);
    cairo_line_to(cr, std::cos(a) * 20 - 6, std::sin(a) * h * 0.3);
    cairo_line_to(cr, std::cos(a) * 20 + 6, std::sin(a) * h * 0.3);
    cairo_close_path(cr);
  }
  cairo_restore(cr);
  setColor(cr, rgb(120, 110, 100));
  cairo_fill(cr);
  // Tower.
  roundedRect(cr, x0 + w * 0.42, y0 + h * 0.06, w * 0.22, h * 0.36, 8);
  fillGradientOutline(cr, y0, y0 + h * 0.4, lighten(paint, 0.2f), dark, kInk, kLine);
  strokeLimb(cr, {{x0 + w * 0.54, y0 + h * 0.08}, {x0 + w * 0.54, y0 - h * 0.12}, {x0 + w * 0.62, y0 - h * 0.12}}, 4,
    rgb(110, 110, 120), kInk, 1.2);
  // Hull.
  roundedRect(cr, x0 + w * 0.06, y0 + h * 0.34, w * 0.92, h * 0.58, h * 0.29);
  fillGradientOutline(cr, y0 + h * 0.34, y0 + h * 0.92, lighten(paint, 0.25f), dark, kInk, kLine);
  // Fins.
  cairo_move_to(cr, x0 + w * 0.12, y0 + h * 0.42);
  cairo_line_to(cr, x0 + w * 0.03, y0 + h * 0.3);
  cairo_line_to(cr, x0 + w * 0.06, y0 + h * 0.58);
  cairo_close_path(cr);
  fillOutline(cr, dark, kInk, 1.6);
  // Portholes: the front one is the cockpit (see-through).
  for (int i = 0; i < 2; ++i)
  {
    circle(cr, x0 + w * (0.3 + 0.16 * i), y0 + h * 0.62, h * 0.12);
    fillOutline(cr, rgb(70, 140, 190), kInk, 2.0);
    circle(cr, x0 + w * (0.3 + 0.16 * i) - 3, y0 + h * 0.58, h * 0.04);
    setColor(cr, rgba(255, 255, 255, 180));
    cairo_fill(cr);
  }
  circle(cr, x0 + w * 0.78, y0 + h * 0.6, h * 0.2);
  fillOutline(cr, rgba(150, 220, 255, 100), rgb(120, 110, 90), 4.0);
  rivets(cr, x0 + w * 0.18, x0 + w * 0.6, y0 + h * 0.42, 14);
  // The lamp.
  roundedRect(cr, x0 + w * 0.94, y0 + h * 0.5, w * 0.06, h * 0.16, 3);
  fillOutline(cr, rgb(255, 250, 210), kInk, 1.4);
}

// Space ship: a dart with a glass canopy, swept wings and a glowing engine.
void ship(cairo_t* cr, double w, double h, int /*variant*/, int frame)
{
  const double x0 = kM, y0 = kM;
  const Color paint = rgb(220, 232, 250), trim = rgb(60, 120, 255);
  // Engine glow.
  radialGlow(cr, x0 + w * 0.02, y0 + h * 0.55, 22 + (frame % 2) * 8, rgb(120, 200, 255), 0.9);
  // Upper and lower wing.
  for (int s = -1; s <= 1; s += 2)
  {
    cairo_move_to(cr, x0 + w * 0.3, y0 + h * 0.55);
    cairo_line_to(cr, x0 + w * 0.08, y0 + h * (0.55 + s * 0.48));
    cairo_line_to(cr, x0 + w * 0.22, y0 + h * (0.55 + s * 0.48));
    cairo_line_to(cr, x0 + w * 0.6, y0 + h * 0.55);
    cairo_close_path(cr);
    fillOutline(cr, s < 0 ? lighten(trim, 0.25f) : trim, kInk, kLine);
  }
  // Fuselage.
  cairo_move_to(cr, x0 + w * 0.04, y0 + h * 0.36);
  cairo_line_to(cr, x0 + w * 0.6, y0 + h * 0.3);
  cairo_curve_to(cr, x0 + w * 0.86, y0 + h * 0.34, x0 + w * 0.98, y0 + h * 0.48, x0 + w * 1.0, y0 + h * 0.56);
  cairo_curve_to(cr, x0 + w * 0.9, y0 + h * 0.7, x0 + w * 0.6, y0 + h * 0.78, x0 + w * 0.04, y0 + h * 0.74);
  cairo_close_path(cr);
  fillGradientOutline(cr, y0 + h * 0.3, y0 + h * 0.78, rgb(255, 255, 255), darken(paint, 0.35f), kInk, kLine);
  cairo_move_to(cr, x0 + w * 0.1, y0 + h * 0.56);
  cairo_line_to(cr, x0 + w * 0.92, y0 + h * 0.56);
  cairo_set_line_width(cr, 3);
  setColor(cr, trim);
  cairo_stroke(cr);
  // Canopy (see-through).
  cairo_move_to(cr, x0 + w * 0.46, y0 + h * 0.34);
  cairo_curve_to(cr, x0 + w * 0.56, y0 + h * 0.06, x0 + w * 0.8, y0 + h * 0.1, x0 + w * 0.86, y0 + h * 0.38);
  cairo_close_path(cr);
  fillOutline(cr, rgba(140, 220, 255, 110), kInk, 1.8);
  // Nozzle and gun tips.
  roundedRect(cr, x0 - 2, y0 + h * 0.4, w * 0.08, h * 0.3, 4);
  fillOutline(cr, rgb(90, 96, 120), kInk, 1.6);
  for (double gy : {0.12, 0.98})
  {
    cairo_rectangle(cr, x0 + w * 0.5, y0 + h * gy - 3, w * 0.36, 6);
    fillOutline(cr, rgb(110, 116, 140), kInk, 1.2);
  }
}

// Mech walker: a cockpit pod on two legs with an arm cannon. Frame 0-3
// steps the legs.
void mech(cairo_t* cr, double w, double h, int /*variant*/, int frame)
{
  const double x0 = kM, y0 = kM;
  const Color paint = rgb(220, 100, 50), steel = rgb(110, 112, 124);
  const double hipY = y0 + h * 0.5;
  const double swing[4] = {0.0, 0.08, 0.0, -0.08};
  for (int leg = 0; leg < 2; ++leg)
  {
    const double s = swing[(frame + leg * 2) % 4] * w;
    const double hx = x0 + w * (leg ? 0.62 : 0.38);
    const double kx = hx + s + w * 0.08, ky = y0 + h * 0.74;
    const double fx = hx + s * 1.6, fy = y0 + h * 0.97;
    const Color c = leg ? steel : darken(steel, 0.25f);
    strokeLimb(cr, {{hx, hipY}, {kx, ky}, {fx, fy - 6}}, 14, c, kInk, kLine);
    roundedRect(cr, fx - w * 0.16, fy - 10, w * 0.32, 12, 4);
    fillOutline(cr, darken(paint, 0.2f), kInk, kLine);
    circle(cr, kx, ky, 8);
    fillOutline(cr, rgb(70, 70, 80), kInk, 1.6);
  }
  // Hip block.
  roundedRect(cr, x0 + w * 0.24, hipY - 12, w * 0.52, 24, 6);
  fillOutline(cr, rgb(80, 82, 92), kInk, kLine);
  // Cockpit pod.
  roundedRect(cr, x0 + w * 0.08, y0 + h * 0.06, w * 0.84, h * 0.36, 18);
  fillGradientOutline(cr, y0 + h * 0.06, y0 + h * 0.42, lighten(paint, 0.25f), darken(paint, 0.3f), kInk, kLine);
  cairo_move_to(cr, x0 + w * 0.48, y0 + h * 0.1);
  cairo_curve_to(cr, x0 + w * 0.82, y0 + h * 0.1, x0 + w * 0.9, y0 + h * 0.16, x0 + w * 0.9, y0 + h * 0.28);
  cairo_line_to(cr, x0 + w * 0.48, y0 + h * 0.28);
  cairo_close_path(cr);
  fillOutline(cr, rgba(150, 220, 255, 110), kInk, 1.8);
  rivets(cr, x0 + w * 0.16, x0 + w * 0.84, y0 + h * 0.36, 14);
  // Arm cannon.
  roundedRect(cr, x0 + w * 0.7, y0 + h * 0.3, w * 0.36, h * 0.09, 5);
  fillOutline(cr, steel, kInk, kLine);
  circle(cr, x0 + w * 1.05, y0 + h * 0.345, 5);
  setColor(cr, rgb(255, 200, 90));
  cairo_fill(cr);
  // Jet nozzles on the back.
  roundedRect(cr, x0 + w * 0.0, y0 + h * 0.2, w * 0.12, h * 0.16, 4);
  fillOutline(cr, rgb(70, 70, 80), kInk, 1.6);
}

// --- Deep water residents --------------------------------------------------------

void piranha(cairo_t* cr, double w, double h, int /*variant*/, int frame)
{
  const double x0 = kM, y0 = kM;
  const Color body = rgb(200, 70, 60), belly = rgb(255, 170, 110);
  const double tail = frame % 2 ? 0.12 : -0.12;
  cairo_move_to(cr, x0 + w * 0.2, y0 + h * 0.5);
  cairo_line_to(cr, x0, y0 + h * (0.1 + tail));
  cairo_line_to(cr, x0, y0 + h * (0.9 + tail));
  cairo_close_path(cr);
  fillOutline(cr, darken(body, 0.2f), kInk, kLine);
  cairo_save(cr);
  cairo_translate(cr, x0 + w * 0.58, y0 + h * 0.5);
  cairo_scale(cr, w * 0.42, h * 0.5);
  circle(cr, 0, 0, 1);
  cairo_restore(cr);
  fillGradientOutline(cr, y0, y0 + h, body, belly, kInk, kLine);
  // Teeth and a mean eye.
  for (int i = 0; i < 4; ++i)
  {
    const double tx = x0 + w * (0.74 + i * 0.05);
    cairo_move_to(cr, tx, y0 + h * 0.6);
    cairo_line_to(cr, tx + 3, y0 + h * 0.74);
    cairo_line_to(cr, tx + 6, y0 + h * 0.6);
    cairo_close_path(cr);
  }
  setColor(cr, rgb(255, 255, 255));
  cairo_fill(cr);
  circle(cr, x0 + w * 0.78, y0 + h * 0.34, 5);
  setColor(cr, rgb(255, 230, 60));
  cairo_fill(cr);
  circle(cr, x0 + w * 0.79, y0 + h * 0.34, 2.2);
  setColor(cr, kInk);
  cairo_fill(cr);
}

void jellyfish(cairo_t* cr, double w, double h, int variant, int frame)
{
  const double x0 = kM, y0 = kM;
  const double squash = variant ? 0.85 : 1.0;
  for (int i = 0; i < 5; ++i)
  {
    const double tx = x0 + w * (0.2 + 0.15 * i);
    const double sway = std::sin(frame * 1.6 + i) * 5.0;
    strokeLimb(cr, {{tx, y0 + h * 0.5}, {tx + sway, y0 + h * 0.75}, {tx - sway, y0 + h}}, 3,
      rgba(220, 150, 255, 200), rgba(120, 60, 160, 200), 1.0);
  }
  cairo_save(cr);
  cairo_translate(cr, x0 + w * 0.5, y0 + h * 0.5);
  cairo_scale(cr, w * 0.48 / squash, h * 0.45 * squash);
  cairo_arc(cr, 0, 0, 1, kPi, 2 * kPi);
  cairo_close_path(cr);
  cairo_restore(cr);
  fillOutline(cr, rgba(230, 160, 255, 190), rgb(140, 70, 190), kLine);
  radialGlow(cr, x0 + w * 0.5, y0 + h * 0.3, w * 0.4, rgb(255, 180, 255), 0.6);
}

void seaMine(cairo_t* cr, double w, double h, int variant, int /*frame*/)
{
  const double x0 = kM, y0 = kM;
  const double cx = x0 + w * 0.5, cy = y0 + h * 0.45, r = w * 0.36;
  for (int i = 0; i < 8; ++i)
  {
    const double a = i * kPi / 4;
    strokeLimb(cr, {{cx, cy}, {cx + std::cos(a) * r * 1.35, cy + std::sin(a) * r * 1.35}}, 5, rgb(90, 90, 100), kInk,
      1.2);
  }
  circle(cr, cx, cy, r);
  fillGradientOutline(cr, cy - r, cy + r, rgb(110, 110, 120), rgb(40, 40, 50), kInk, kLine);
  // The chain down to the sea bed.
  strokeLimb(cr, {{cx, cy + r}, {cx, y0 + h + 20}}, 2, rgb(80, 80, 90), rgb(80, 80, 90), 0.0);
  radialGlow(cr, cx, cy - r * 0.2, 10, variant ? rgb(255, 60, 40) : rgb(255, 160, 60), variant ? 1.0 : 0.6);
}

void angler(cairo_t* cr, double w, double h, int variant, int frame)
{
  const double x0 = kM, y0 = kM;
  const Color body = rgb(60, 70, 96);
  const double tail = frame % 2 ? 0.08 : -0.08;
  cairo_move_to(cr, x0 + w * 0.2, y0 + h * 0.5);
  cairo_line_to(cr, x0, y0 + h * (0.15 + tail));
  cairo_line_to(cr, x0, y0 + h * (0.85 + tail));
  cairo_close_path(cr);
  fillOutline(cr, darken(body, 0.2f), kInk, kLine);
  cairo_save(cr);
  cairo_translate(cr, x0 + w * 0.56, y0 + h * 0.55);
  cairo_scale(cr, w * 0.42, h * 0.45);
  circle(cr, 0, 0, 1);
  cairo_restore(cr);
  fillGradientOutline(cr, y0 + h * 0.1, y0 + h, lighten(body, 0.2f), darken(body, 0.4f), kInk, kLine);
  // The jaw: wide open on the lunge.
  const double open = variant == 2 ? 0.2 : (variant == 1 ? 0.1 : 0.04);
  cairo_move_to(cr, x0 + w * 0.7, y0 + h * 0.62);
  cairo_line_to(cr, x0 + w * 1.0, y0 + h * (0.6 - open));
  cairo_line_to(cr, x0 + w * 1.0, y0 + h * (0.68 + open));
  cairo_close_path(cr);
  setColor(cr, rgb(20, 10, 20));
  cairo_fill(cr);
  for (int i = 0; i < 5; ++i)
  {
    const double tx = x0 + w * (0.76 + i * 0.05);
    cairo_move_to(cr, tx, y0 + h * (0.6 - open * 0.7));
    cairo_line_to(cr, tx + 3, y0 + h * (0.68 - open * 0.2));
    cairo_line_to(cr, tx + 6, y0 + h * (0.6 - open * 0.7));
    cairo_close_path(cr);
  }
  setColor(cr, rgb(240, 240, 220));
  cairo_fill(cr);
  circle(cr, x0 + w * 0.74, y0 + h * 0.36, 6);
  setColor(cr, rgb(230, 255, 120));
  cairo_fill(cr);
  // The lure on its stalk.
  strokeLimb(cr, {{x0 + w * 0.62, y0 + h * 0.16}, {x0 + w * 0.82, y0 - h * 0.2}, {x0 + w * 1.02, y0 + h * 0.04}}, 3,
    rgb(90, 100, 130), kInk, 1.0);
  radialGlow(cr, x0 + w * 1.02, y0 + h * 0.08, variant ? 34 : 22, rgb(200, 255, 140), variant ? 1.0 : 0.8);
  circle(cr, x0 + w * 1.02, y0 + h * 0.08, 5);
  setColor(cr, rgb(240, 255, 200));
  cairo_fill(cr);
}

// Bounder (level 48): a big, friendly, flea-like alien with a saddle on its
// back. Long hind legs folded under it (stretched out in a leap, tucked up
// coming down), little front legs, a round head with antennae. The rider
// is drawn by the game on the saddle. Frames: 0, 1 trotting, 2 leaping,
// 3 coming down.
void bounder(cairo_t* cr, double w, double h, int /*variant*/, int frame)
{
  const double x0 = kM, y0 = kM;
  const Color shell = rgb(236, 150, 90), belly = rgb(255, 214, 160);
  const bool leap = frame == 2, fall = frame == 3;
  const double bodyTop = y0 + h * (leap ? 0.38 : 0.44), bodyBot = y0 + h * (leap ? 0.74 : 0.84);
  const double cy = (bodyTop + bodyBot) * 0.5, ry = (bodyBot - bodyTop) * 0.5, rx = w * 0.42;
  const double cx = x0 + w * 0.46;
  // Hind leg (the far one first, darker): thigh, shin, foot.
  auto hindLeg = [&](double dx, Color c) {
    const double hipX = cx - rx * 0.45 + dx, hipY = cy + ry * 0.2;
    double kx = hipX - w * 0.2, ky = hipY - h * 0.16, fx = hipX - w * 0.06, fy = y0 + h;
    if (leap)
    {
      kx = hipX - w * 0.2;
      ky = hipY + h * 0.1;
      fx = hipX - w * 0.34;
      fy = y0 + h * 0.98;
    }
    else if (fall)
    {
      kx = hipX - w * 0.14;
      ky = hipY - h * 0.1;
      fy = y0 + h * 0.92;
    }
    else if (frame == 1)
    {
      kx -= w * 0.04;
      fx += w * 0.04;
    }
    strokeLimb(cr, {{hipX, hipY}, {kx, ky}, {fx, fy}}, 11, c, kInk, kLine);
    circle(cr, fx, fy - 3, 7);
    fillOutline(cr, darken(c, 0.2f), kInk, 1.6);
  };
  auto frontLeg = [&](double dx, Color c) {
    const double sx = cx + rx * 0.5 + dx, sy = cy + ry * 0.5;
    const double reach = leap ? w * 0.12 : (frame == 1 ? w * 0.03 : 0.0);
    strokeLimb(cr, {{sx, sy}, {sx + w * 0.08 + reach, sy + h * 0.06}, {sx + w * 0.04 + reach, leap ? y0 + h * 0.86 : y0 + h}},
      7, c, kInk, kLine);
  };
  hindLeg(w * 0.08, darken(shell, 0.3f));
  frontLeg(-w * 0.06, darken(shell, 0.3f));
  // The body: a glossy shell in plates, a pale belly.
  cairo_save(cr);
  cairo_translate(cr, cx, cy);
  cairo_scale(cr, rx, ry);
  circle(cr, 0, 0, 1);
  cairo_restore(cr);
  fillGradientOutline(cr, bodyTop, bodyBot, lighten(shell, 0.25f), darken(shell, 0.25f), kInk, kLine);
  cairo_save(cr);
  cairo_translate(cr, cx + rx * 0.1, cy + ry * 0.45);
  cairo_scale(cr, rx * 0.75, ry * 0.42);
  circle(cr, 0, 0, 1);
  cairo_restore(cr);
  setColor(cr, belly);
  cairo_fill(cr);
  for (int k = 1; k <= 3; ++k)
  {
    // Plate seams across the back.
    const double sx = cx - rx + rx * 0.5 * k;
    cairo_move_to(cr, sx, cy - ry * 0.95 + std::abs(sx - cx) / rx * ry * 0.3);
    cairo_curve_to(cr, sx + 8, cy - ry * 0.2, sx + 8, cy + ry * 0.2, sx, cy + ry * 0.5);
    cairo_set_line_width(cr, 2.0);
    setColor(cr, withAlpha(darken(shell, 0.4f), 170));
    cairo_stroke(cr);
  }
  // A shine.
  cairo_save(cr);
  cairo_translate(cr, cx - rx * 0.25, cy - ry * 0.55);
  cairo_scale(cr, rx * 0.35, ry * 0.15);
  circle(cr, 0, 0, 1);
  cairo_restore(cr);
  setColor(cr, rgba(255, 255, 255, 120));
  cairo_fill(cr);
  // The saddle and its strap.
  const double sx = x0 + w * 0.45, sy = bodyTop + 4;
  roundedRect(cr, sx - w * 0.17, sy - 8, w * 0.34, 16, 7);
  fillOutline(cr, rgb(120, 60, 40), kInk, 1.8);
  cairo_move_to(cr, sx - w * 0.04, sy + 6);
  cairo_line_to(cr, sx - w * 0.02, bodyBot - ry * 0.5);
  cairo_set_line_width(cr, 4.0);
  setColor(cr, rgb(110, 54, 36));
  cairo_stroke(cr);
  hindLeg(0.0, shell);
  frontLeg(0.0, shell);
  // The head: round, big friendly eyes, antennae bobbing.
  const double hx = cx + rx * 0.95, hy = cy - ry * 0.15, hr = h * 0.1;
  const double bob = frame == 1 ? 4.0 : 0.0;
  for (int k = 0; k < 2; ++k)
  {
    const double ax = hx + hr * (0.1 + k * 0.4), ay = hy - hr * 0.8;
    cairo_move_to(cr, ax, ay);
    cairo_curve_to(cr, ax + 6, ay - 24, ax + 24 + k * 8, ay - 30 - bob, ax + 30 + k * 10, ay - 22 - bob);
    cairo_set_line_width(cr, 2.6);
    setColor(cr, kInk);
    cairo_stroke(cr);
    circle(cr, ax + 30 + k * 10, ay - 22 - bob, 4);
    setColor(cr, rgb(255, 230, 120));
    cairo_fill(cr);
  }
  circle(cr, hx, hy, hr);
  fillGradientOutline(cr, hy - hr, hy + hr, lighten(shell, 0.3f), shell, kInk, kLine);
  circle(cr, hx + hr * 0.35, hy - hr * 0.15, hr * 0.42);
  setColor(cr, rgb(255, 255, 255));
  cairo_fill(cr);
  circle(cr, hx + hr * 0.48, hy - hr * 0.12, hr * 0.22);
  setColor(cr, rgb(30, 20, 40));
  cairo_fill(cr);
  circle(cr, hx + hr * 0.42, hy - hr * 0.24, hr * 0.08);
  setColor(cr, rgb(255, 255, 255));
  cairo_fill(cr);
  // A smile.
  cairo_arc(cr, hx + hr * 0.4, hy + hr * 0.3, hr * 0.3, 0.2, 1.6);
  cairo_set_line_width(cr, 2.0);
  setColor(cr, kInk);
  cairo_stroke(cr);
}

} // namespace

bool drawVehicleArt(cairo_t* cr, const std::string& key, double w, double h, int variant, int frame)
{
  using Fn = void (*)(cairo_t*, double, double, int, int);
  static const struct
  {
    const char* key;
    Fn fn;
  } kRoutines[] = {
    {"veh_tank", tank},
    {"veh_helicopter", heli},
    {"veh_hoverbike", bike},
    {"veh_submarine", sub},
    {"veh_spaceship", ship},
    {"veh_mech", mech},
    {"veh_bounder", bounder},
    {"piranha", piranha},
    {"jellyfish", jellyfish},
    {"sea_mine", seaMine},
    {"angler", angler},
  };
  for (const auto& r : kRoutines)
    if (key == r.key)
    {
      r.fn(cr, w, h, variant, frame);
      return true;
    }
  return false;
}

} // namespace gr
