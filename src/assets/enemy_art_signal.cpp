// Dead Air (Level 29) in the style of enemy_art.cpp, but made of picture
// rather than of stuff: the Ghost Image is a newsreader burnt into the
// tube (grey, scanned, its edges split red and blue), the Static Swarm a
// churning cloud of snow, the rubber duck wears the seven bar colours, and
// the Test Card is a square plate with a turning disc of four quarters and
// a little face in the middle. Each enemy is drawn in screen pixels into a
// texture with a 32 px margin, the cell box at (32, 32) .. (32 + w, 32 + h),
// facing right.

#include "assets/enemy_art_signal.hpp"

#include "base/math.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace gr
{

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr double kM = 32.0;
constexpr Color kInk = rgb(26, 20, 38);
constexpr Color kDark = rgb(16, 16, 22);

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

void vgrad(cairo_t* cr, double y0, double y1, Color a, Color b, bool preserve = false)
{
  cairo_pattern_t* g = cairo_pattern_create_linear(0, y0, 0, y1);
  stop(g, 0, a);
  stop(g, 1, b);
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

// A limb: a thick round-capped line through three points.
void limb(cairo_t* cr, double x0, double y0, double x1, double y1, double x2, double y2, double w, Color c, Color edge)
{
  cairo_move_to(cr, x0, y0);
  cairo_curve_to(cr, x1, y1, x1, y1, x2, y2);
  setRgba(cr, edge, 1.0);
  cairo_set_line_width(cr, w + 3.0);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  cairo_stroke_preserve(cr);
  setRgba(cr, c, 1.0);
  cairo_set_line_width(cr, w);
  cairo_stroke(cr);
}

// --- The Ghost Image ---------------------------------------------------------------------------

// The newsreader, opaque, in a 96 x 160 box (facing right).
void newsreader(cairo_t* cr, bool tell, int frame)
{
  const Color suit = rgb(186, 190, 208), suitDark = rgb(138, 142, 162), edge = rgb(84, 88, 108);
  const Color skin = rgb(222, 224, 234), hair = rgb(108, 112, 132);
  const double sway = frame == 1 ? 2.0 : 0.0;
  // The far arm, behind the body.
  limb(cr, 30, 54, 24, 78, 26 - sway, 100, 12, suitDark, edge);
  ellipse(cr, 26 - sway, 103, 5.5, 6.5);
  setRgba(cr, darken(skin, 0.12f), 1.0);
  cairo_fill(cr);
  // Legs: trousers to the floor (faded out later).
  for (const double lx : {38.0, 58.0})
  {
    cairo_move_to(cr, lx - 8, 100);
    cairo_line_to(cr, lx + 8, 100);
    cairo_line_to(cr, lx + 6.5, 158);
    cairo_line_to(cr, lx - 6.5, 158);
    cairo_close_path(cr);
    vgrad(cr, 100, 158, suitDark, darken(suitDark, 0.2f), true);
    stroke(cr, edge, 1.6);
  }
  // The jacket.
  cairo_move_to(cr, 30, 48);
  cairo_curve_to(cr, 24, 50, 22, 58, 24, 66);
  cairo_line_to(cr, 30, 108);
  cairo_line_to(cr, 70, 108);
  cairo_line_to(cr, 76, 66);
  cairo_curve_to(cr, 78, 58, 76, 50, 70, 48);
  cairo_close_path(cr);
  vgrad(cr, 46, 108, lighten(suit, 0.15f), suitDark, true);
  stroke(cr, edge, 2.0);
  // Shirt and tie in the V, the lapels either side.
  cairo_move_to(cr, 43, 48);
  cairo_line_to(cr, 57, 48);
  cairo_line_to(cr, 50, 74);
  cairo_close_path(cr);
  setRgba(cr, rgb(244, 245, 252), 1.0);
  cairo_fill(cr);
  cairo_move_to(cr, 48, 50);
  cairo_line_to(cr, 52, 50);
  cairo_line_to(cr, 53.5, 76);
  cairo_line_to(cr, 50, 82);
  cairo_line_to(cr, 46.5, 76);
  cairo_close_path(cr);
  setRgba(cr, rgb(92, 96, 116), 1.0);
  cairo_fill(cr);
  for (const double s : {-1.0, 1.0})
  {
    cairo_move_to(cr, 50 + s * 9, 47);
    cairo_line_to(cr, 50 + s * 4, 62);
    cairo_line_to(cr, 50 + s * 1, 84);
    stroke(cr, edge, 1.6);
  }
  // A button and a pocket square.
  circle(cr, 50, 92, 1.8);
  setRgba(cr, edge, 1.0);
  cairo_fill(cr);
  cairo_move_to(cr, 62, 64);
  cairo_line_to(cr, 70, 64);
  cairo_line_to(cr, 68, 60);
  cairo_line_to(cr, 65, 62);
  cairo_close_path(cr);
  setRgba(cr, rgb(250, 250, 255), 1.0);
  cairo_fill(cr);
  // The near arm: hanging and swinging, or out at the camera.
  if (tell)
  {
    limb(cr, 70, 54, 86, 58, 104, 56, 12, suit, edge);
    ellipse(cr, 110, 56, 7, 5.5);
    setRgba(cr, skin, 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, edge, 1.4);
    // The pointing finger.
    cairo_move_to(cr, 114, 54.5);
    cairo_line_to(cr, 122, 54.5);
    stroke(cr, skin, 3.6);
  }
  else
  {
    limb(cr, 70, 54, 78, 76, 76 + sway, 98, 12, suit, edge);
    ellipse(cr, 76 + sway, 102, 5.5, 6.5);
    setRgba(cr, skin, 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, edge, 1.4);
  }
  // Neck and head.
  cairo_rectangle(cr, 45, 36, 11, 12);
  setRgba(cr, darken(skin, 0.1f), 1.0);
  cairo_fill(cr);
  ellipse(cr, 52, 24, 14, 17, 0.08);
  vgrad(cr, 6, 42, lighten(skin, 0.3f), darken(skin, 0.1f), true);
  stroke(cr, edge, 1.8);
  // Slicked hair with a side parting.
  cairo_move_to(cr, 38, 26);
  cairo_curve_to(cr, 34, 10, 46, 4, 54, 6);
  cairo_curve_to(cr, 62, 6, 68, 12, 66, 20);
  cairo_curve_to(cr, 60, 13, 52, 12, 46, 14);
  cairo_curve_to(cr, 42, 16, 40, 22, 38, 26);
  cairo_close_path(cr);
  setRgba(cr, hair, 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, edge, 1.4);
  cairo_move_to(cr, 47, 8);
  cairo_curve_to(cr, 52, 10, 58, 9, 63, 14);
  stroke(cr, lighten(hair, 0.35f), 1.2);
  // A thin smile; the eyes are the tracking line, painted over later.
  cairo_move_to(cr, 50, 33);
  cairo_curve_to(cr, 54, 35, 58, 34, 60, 32);
  stroke(cr, edge, 1.4);
}

void ghostImage(cairo_t* cr, double w, double h, int variant, int frame)
{
  const bool tell = variant == 1, after = variant == 2;
  cairo_save(cr);
  cairo_translate(cr, kM, kM);
  cairo_scale(cr, w / 96.0, h / 160.0);
  // The figure, its scan lines cut out of it.
  cairo_push_group(cr);
  newsreader(cr, tell, frame);
  cairo_set_operator(cr, CAIRO_OPERATOR_DEST_OUT);
  for (double y = (frame % 2) * 2.0; y < 170; y += 4.0)
  {
    cairo_rectangle(cr, -20, y, 160, 1.4);
    cairo_set_source_rgba(cr, 0, 0, 0, after ? 0.6 : 0.45);
    cairo_fill(cr);
  }
  cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
  cairo_pattern_t* fig = cairo_pop_group(cr);
  // Faded out from the knees down.
  cairo_pattern_t* fade = cairo_pattern_create_linear(0, 0, 0, 160);
  cairo_pattern_add_color_stop_rgba(fade, 0.0, 0, 0, 0, 1.0);
  cairo_pattern_add_color_stop_rgba(fade, 0.62, 0, 0, 0, 0.95);
  cairo_pattern_add_color_stop_rgba(fade, 1.0, 0, 0, 0, 0.05);
  cairo_push_group(cr);
  cairo_set_source(cr, fig);
  cairo_mask(cr, fade);
  cairo_pattern_t* body = cairo_pop_group(cr);
  if (after)
  {
    // An after-image: one flat grey.
    setRgba(cr, rgb(176, 178, 192), 0.9);
    cairo_mask(cr, body);
  }
  else
  {
    // Split a little red one way and blue the other, like a picture out of
    // convergence.
    cairo_save(cr);
    cairo_translate(cr, -2.2, 0);
    setRgba(cr, rgb(255, 60, 80), 0.4);
    cairo_mask(cr, body);
    cairo_restore(cr);
    cairo_save(cr);
    cairo_translate(cr, 2.2, 0);
    setRgba(cr, rgb(60, 140, 255), 0.4);
    cairo_mask(cr, body);
    cairo_restore(cr);
    cairo_set_source(cr, body);
    cairo_paint_with_alpha(cr, 0.94);
    // The eyes: one bright tracking line across the face, flaring in the
    // tell.
    const double l = tell ? 34 : 26, gy = 22;
    cairo_pattern_t* glow = cairo_pattern_create_radial(54, gy, 2, 54, gy, tell ? 26 : 16);
    cairo_pattern_add_color_stop_rgba(glow, 0, 0.8, 1.0, 1.0, tell ? 0.75 : 0.45);
    cairo_pattern_add_color_stop_rgba(glow, 1, 0.8, 1.0, 1.0, 0.0);
    cairo_set_source(cr, glow);
    cairo_paint(cr);
    cairo_pattern_destroy(glow);
    cairo_rectangle(cr, 54 - l * 0.5, gy - 2.2, l, 4.4);
    setRgba(cr, rgb(230, 255, 255), 1.0);
    cairo_fill(cr);
    cairo_rectangle(cr, 54 - l * 0.5 - 6, gy - 0.6, l + 12, 1.2);
    setRgba(cr, rgb(255, 255, 255), 0.7);
    cairo_fill(cr);
  }
  cairo_pattern_destroy(body);
  cairo_pattern_destroy(fade);
  cairo_pattern_destroy(fig);
  cairo_restore(cr);
}

// --- The Static Swarm ----------------------------------------------------------------------------

void staticSwarm(cairo_t* cr, double w, double h, bool carrier, int frame)
{
  const double cx = kM + w * 0.5, cy = kM + h * 0.5, R = std::min(w, h) * 0.46;
  Rng rng(std::uint32_t(29101 + frame * 977 + (carrier ? 31 : 0)));
  const Color smoke = carrier ? rgb(20, 40, 24) : rgb(30, 31, 36);
  const Color tint = carrier ? rgb(150, 255, 120) : rgb(235, 238, 250);
  // A dark churning body: overlapping soft lobes.
  for (int i = 0; i < 6; ++i)
  {
    const double a = i * kPi / 3.0 + frame * 0.4 + rng.range(-0.3f, 0.3f);
    const double d = i == 0 ? 0.0 : R * rng.range(0.25f, 0.45f);
    const double lx = cx + std::cos(a) * d, ly = cy + std::sin(a) * d * 0.8, lr = R * rng.range(0.6f, 0.8f);
    cairo_pattern_t* g = cairo_pattern_create_radial(lx, ly, 0, lx, ly, lr);
    stop(g, 0.0, smoke, 0.5);
    stop(g, 0.7, smoke, 0.22);
    stop(g, 1.0, smoke, 0.0);
    circle(cr, lx, ly, lr);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  // A faint glow of its own.
  {
    cairo_pattern_t* g = cairo_pattern_create_radial(cx, cy, 0, cx, cy, R * 1.2);
    stop(g, 0.0, tint, 0.18);
    stop(g, 1.0, tint, 0.0);
    circle(cr, cx, cy, R * 1.2);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  // The snow: flecks thick in the middle, thinning out, a few strays.
  for (int i = 0; i < 1300; ++i)
  {
    const double u = (rng.uniform() + rng.uniform() + rng.uniform()) / 1.5 - 1.0;
    const double v = (rng.uniform() + rng.uniform() + rng.uniform()) / 1.5 - 1.0;
    const double x = cx + u * R * 1.25, y = cy + v * R * 1.1;
    const double fall = std::max(0.0, 1.0 - std::hypot(u, v));
    const double fw = rng.range(1.4f, 4.0f), fh = rng.range(1.1f, 2.2f);
    cairo_rectangle(cr, x, y, fw, fh);
    const int g8 = int(rng.range(120, 255));
    const Color c = carrier && rng.uniform() < 0.6f ? lerpColor(rgb(g8, g8, g8), tint, 0.7f) : rgb(g8, g8, std::min(255, g8 + 10));
    setRgba(cr, c, (0.25 + 0.75 * fall) * rng.range(0.5f, 1.0f));
    cairo_fill(cr);
  }
  // Tears: a few bright streaks across, slipped sideways.
  for (int i = 0; i < 3; ++i)
  {
    const double y = cy + rng.range(-R * 0.7f, R * 0.7f), l = rng.range(30, 80), x = cx + rng.range(-40, 10);
    cairo_rectangle(cr, x, y, l, rng.range(1.2f, 2.4f));
    setRgba(cr, lighten(tint, 0.4f), 0.35);
    cairo_fill(cr);
  }
  // Two slits of eyes, looking ahead.
  for (const auto& e : {std::array<double, 2>{cx + 8, cy - 9}, std::array<double, 2>{cx + 30, cy - 7}})
  {
    cairo_pattern_t* g = cairo_pattern_create_radial(e[0], e[1], 0, e[0], e[1], 14);
    stop(g, 0.0, tint, 0.55);
    stop(g, 1.0, tint, 0.0);
    circle(cr, e[0], e[1], 14);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
    ellipse(cr, e[0], e[1], 6.5, 2.3, -0.12);
    setRgba(cr, lighten(tint, 0.6f), 1.0);
    cairo_fill(cr);
  }
}

// --- The duck's costume ----------------------------------------------------------------------------

// The rubber duck (the item icon's shapes) painted in the seven bar
// colours, its beak and shades put back on top.
void duckBars(cairo_t* cr, double w, double h, int variant)
{
  if (variant != 1)
    return;
  cairo_save(cr);
  cairo_translate(cr, kM, kM);
  cairo_scale(cr, w / 64.0, h / 64.0);
  static constexpr std::array<Color, 7> kStripe{rgb(236, 236, 236), rgb(240, 220, 40), rgb(40, 220, 230), rgb(60, 210, 80),
    rgb(220, 60, 210), rgb(235, 50, 50), rgb(50, 80, 235)};
  auto stripes = [&] {
    for (int i = 0; i < 7; ++i)
    {
      cairo_rectangle(cr, 9 + i * 6.3, 8, 6.6, 52);
      setRgba(cr, kStripe[std::size_t(i)], 1.0);
      cairo_fill(cr);
    }
    // A soft shade from below.
    cairo_rectangle(cr, 0, 8, 64, 52);
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 14, 0, 58);
    cairo_pattern_add_color_stop_rgba(g, 0, 1, 1, 1, 0.2);
    cairo_pattern_add_color_stop_rgba(g, 0.5, 1, 1, 1, 0.0);
    cairo_pattern_add_color_stop_rgba(g, 1, 0, 0, 0, 0.3);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  };
  // The body.
  cairo_save(cr);
  cairo_translate(cr, 30, 42);
  cairo_scale(cr, 1.0, 0.7);
  cairo_arc(cr, 0, 0, 20, 0, 2 * kPi);
  cairo_restore(cr);
  cairo_save(cr);
  cairo_clip_preserve(cr);
  stripes();
  cairo_restore(cr);
  stroke(cr, kInk, 2.4);
  // The head.
  cairo_arc(cr, 38, 24, 12, 0, 2 * kPi);
  cairo_save(cr);
  cairo_clip_preserve(cr);
  stripes();
  cairo_restore(cr);
  stroke(cr, kInk, 2.4);
  ellipse(cr, 34, 18, 4.5, 2.6, -0.5);
  setRgba(cr, rgb(255, 255, 255), 0.55);
  cairo_fill(cr);
  // The beak, the shades and the wing.
  cairo_move_to(cr, 48, 24);
  cairo_line_to(cr, 60, 27);
  cairo_line_to(cr, 48, 30);
  cairo_close_path(cr);
  setRgba(cr, rgb(255, 130, 30), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 2.0);
  cairo_new_path(cr);
  cairo_move_to(cr, 34, 18);
  cairo_line_to(cr, 46, 18);
  cairo_arc(cr, 46, 20, 2, -kPi * 0.5, 0);
  cairo_line_to(cr, 48, 22);
  cairo_arc(cr, 46, 22, 2, 0, kPi * 0.5);
  cairo_line_to(cr, 34, 24);
  cairo_arc(cr, 34, 22, 2, kPi * 0.5, kPi);
  cairo_line_to(cr, 32, 20);
  cairo_arc(cr, 34, 20, 2, kPi, kPi * 1.5);
  cairo_close_path(cr);
  setRgba(cr, rgb(255, 255, 255), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.6);
  cairo_move_to(cr, 14, 40);
  cairo_curve_to(cr, 20, 34, 28, 36, 32, 42);
  stroke(cr, kInk, 2.0, 0.6);
  cairo_restore(cr);
}

// The quarter's outline: an annulus from r0 to r1 over the top-left
// quarter, a hair short of each edge so the four sit apart.
void wedgePath(cairo_t* cr, double cx, double cy, double s)
{
  const double r0 = 40 * s, r1 = 104 * s, gap = 0.035;
  const double a0 = kPi + gap, a1 = kPi * 1.5 - gap;
  cairo_new_path(cr);
  cairo_arc(cr, cx, cy, r1, a0, a1);
  cairo_arc_negative(cr, cx, cy, r0, a1 + 0.03, a0 - 0.03);
  cairo_close_path(cr);
}

} // namespace

// --- The Test Card -----------------------------------------------------------------------------

void paintTestCardBack(cairo_t* cr, double cx, double cy, double s)
{
  const double half = 124 * s;
  // A soft shadow, then the plate.
  cairo_pattern_t* sh = cairo_pattern_create_radial(cx, cy + 10 * s, half * 0.8, cx, cy + 10 * s, half * 1.25);
  cairo_pattern_add_color_stop_rgba(sh, 0, 0, 0, 0, 0.5);
  cairo_pattern_add_color_stop_rgba(sh, 1, 0, 0, 0, 0.0);
  cairo_rectangle(cr, cx - half * 1.3, cy - half * 1.3, half * 2.6, half * 2.6);
  cairo_set_source(cr, sh);
  cairo_fill(cr);
  cairo_pattern_destroy(sh);
  roundedRect(cr, cx - half, cy - half, half * 2, half * 2, 26 * s);
  vgrad(cr, cy - half, cy + half, rgb(64, 66, 78), rgb(30, 31, 38), true);
  setRgba(cr, kDark, 1.0);
  cairo_set_line_width(cr, 3 * s);
  cairo_stroke(cr);
  // The grid, white and faint.
  cairo_save(cr);
  roundedRect(cr, cx - half, cy - half, half * 2, half * 2, 26 * s);
  cairo_clip(cr);
  for (int k = -4; k <= 4; ++k)
  {
    const double o = k * 31 * s;
    cairo_rectangle(cr, cx + o - 0.6 * s, cy - half, 1.2 * s, half * 2);
    cairo_rectangle(cr, cx - half, cy + o - 0.6 * s, half * 2, 1.2 * s);
  }
  setRgba(cr, rgb(255, 255, 255), 0.16);
  cairo_fill(cr);
  cairo_restore(cr);
  roundedRect(cr, cx - half + 7 * s, cy - half + 7 * s, half * 2 - 14 * s, half * 2 - 14 * s, 20 * s);
  stroke(cr, rgb(255, 255, 255), 1.6 * s, 0.5);
  // Corner brackets.
  for (const double sx : {-1.0, 1.0})
    for (const double sy : {-1.0, 1.0})
    {
      const double x = cx + sx * (half - 16 * s), y = cy + sy * (half - 16 * s);
      cairo_move_to(cr, x, y - sy * 14 * s);
      cairo_line_to(cr, x, y);
      cairo_line_to(cr, x - sx * 14 * s, y);
      stroke(cr, rgb(255, 255, 255), 2.6 * s, 0.8);
    }
  // The well the disc turns in.
  circle(cr, cx, cy, 110 * s);
  cairo_pattern_t* w = cairo_pattern_create_radial(cx, cy, 20 * s, cx, cy, 110 * s);
  stop(w, 0.0, rgb(26, 26, 32));
  stop(w, 0.85, rgb(14, 14, 18));
  stop(w, 1.0, rgb(4, 4, 6));
  cairo_set_source(cr, w);
  cairo_fill(cr);
  cairo_pattern_destroy(w);
  // Its rim, lit from above.
  circle(cr, cx, cy, 112 * s);
  cairo_pattern_t* rim = cairo_pattern_create_linear(cx, cy - 112 * s, cx, cy + 112 * s);
  cairo_pattern_add_color_stop_rgba(rim, 0, 1, 1, 1, 0.6);
  cairo_pattern_add_color_stop_rgba(rim, 1, 1, 1, 1, 0.08);
  cairo_set_source(cr, rim);
  cairo_set_line_width(cr, 2.4 * s);
  cairo_stroke(cr);
  cairo_pattern_destroy(rim);
}

void paintTestCardWedge(cairo_t* cr, double cx, double cy, double s, Color c, bool dead)
{
  const Color base = dead ? rgb(104, 104, 110) : c;
  wedgePath(cr, cx, cy, s);
  cairo_pattern_t* g = cairo_pattern_create_radial(cx, cy, 40 * s, cx, cy, 104 * s);
  stop(g, 0.0, darken(base, 0.3f));
  stop(g, 0.55, base);
  stop(g, 1.0, lighten(base, 0.18f));
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  cairo_save(cr);
  cairo_clip(cr);
  if (dead)
  {
    // Off the air: scanned and snowy, and cracked across.
    for (double y = cy - 110 * s; y < cy; y += 4 * s)
    {
      cairo_rectangle(cr, cx - 110 * s, y, 110 * s, 1.3 * s);
      setRgba(cr, rgb(0, 0, 0), 0.28);
      cairo_fill(cr);
    }
    Rng rng(29151u);
    for (int i = 0; i < 160; ++i)
    {
      const double a = rng.range(float(kPi), float(kPi * 1.5)), d = rng.range(40.0f, 104.0f) * s;
      cairo_rectangle(cr, cx + std::cos(a) * d, cy + std::sin(a) * d, rng.range(1.5f, 3.5f) * s, 1.4 * s);
      const int v = int(rng.range(150, 240));
      setRgba(cr, rgb(v, v, v), rng.range(0.4f, 0.9f));
      cairo_fill(cr);
    }
    cairo_move_to(cr, cx - 96 * s, cy - 30 * s);
    cairo_line_to(cr, cx - 70 * s, cy - 38 * s);
    cairo_line_to(cr, cx - 62 * s, cy - 58 * s);
    cairo_line_to(cr, cx - 40 * s, cy - 66 * s);
    stroke(cr, kDark, 2.2 * s, 0.8);
  }
  else
  {
    // A resolution grating: fine rings, and the gloss along the rim.
    for (int k = 0; k < 6; ++k)
    {
      cairo_arc(cr, cx, cy, (52 + k * 8) * s, kPi, kPi * 1.5);
      stroke(cr, darken(base, 0.5f), 0.9 * s, 0.22);
    }
    cairo_arc(cr, cx, cy, 97 * s, kPi * 1.08, kPi * 1.42);
    stroke(cr, rgb(255, 255, 255), 4 * s, 0.35);
    cairo_arc(cr, cx, cy, 46 * s, kPi * 1.1, kPi * 1.4);
    stroke(cr, rgb(255, 255, 255), 1.6 * s, 0.25);
  }
  cairo_restore(cr);
  wedgePath(cr, cx, cy, s);
  setRgba(cr, kDark, 1.0);
  cairo_set_line_width(cr, 2.6 * s);
  cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
  cairo_stroke(cr);
}

void paintTestCardWedgeMask(cairo_t* cr, double cx, double cy, double s)
{
  wedgePath(cr, cx, cy, s);
  setRgba(cr, rgb(255, 255, 255), 1.0);
  cairo_fill(cr);
}

void paintTestCardFace(cairo_t* cr, double cx, double cy, double s, int mouth)
{
  const double R = 36 * s;
  circle(cr, cx, cy, R);
  cairo_pattern_t* g = cairo_pattern_create_radial(cx - 10 * s, cy - 12 * s, 4 * s, cx, cy, R);
  stop(g, 0.0, rgb(252, 252, 254));
  stop(g, 1.0, rgb(190, 192, 202));
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  setRgba(cr, kDark, 1.0);
  cairo_set_line_width(cr, 3.2 * s);
  cairo_stroke(cr);
  // The crosshair and its ring, printed pale.
  cairo_save(cr);
  circle(cr, cx, cy, R - 2 * s);
  cairo_clip(cr);
  cairo_rectangle(cr, cx - R, cy - 0.6 * s, R * 2, 1.2 * s);
  cairo_rectangle(cr, cx - 0.6 * s, cy - R, 1.2 * s, R * 2);
  setRgba(cr, rgb(90, 92, 110), 0.35);
  cairo_fill(cr);
  circle(cr, cx, cy, 24 * s);
  stroke(cr, rgb(90, 92, 110), 1.2 * s, 0.3);
  cairo_restore(cr);
  // Two pill eyes with a glint.
  for (const double ex : {-12.0, 12.0})
  {
    roundedRect(cr, cx + ex * s - 3.6 * s, cy - 18 * s, 7.2 * s, 14 * s, 3.6 * s);
    setRgba(cr, rgb(22, 22, 30), 1.0);
    cairo_fill(cr);
    circle(cr, cx + ex * s - 1 * s, cy - 14.5 * s, 1.5 * s);
    setRgba(cr, rgb(255, 255, 255), 0.9);
    cairo_fill(cr);
  }
  // The mouth.
  if (mouth == 1)
  {
    roundedRect(cr, cx - 11 * s, cy + 4 * s, 22 * s, 15 * s, 6 * s);
    setRgba(cr, rgb(22, 22, 30), 1.0);
    cairo_fill(cr);
    ellipse(cr, cx, cy + 15 * s, 6 * s, 3 * s);
    setRgba(cr, rgb(230, 80, 90), 1.0);
    cairo_fill(cr);
  }
  else
  {
    const double bend = mouth == 2 ? -5.0 : 4.0;
    cairo_move_to(cr, cx - 11 * s, cy + 12 * s);
    cairo_curve_to(cr, cx - 4 * s, cy + (12 + bend) * s, cx + 4 * s, cy + (12 + bend) * s, cx + 11 * s, cy + 12 * s);
    stroke(cr, rgb(22, 22, 30), 3 * s);
  }
}

bool drawSignalArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame)
{
  (void)t;
  if (key == "ghost_image")
    ghostImage(cr, w, h, std::clamp(variant, 0, 2), frame % 2);
  else if (key == "static_swarm")
    staticSwarm(cr, w, h, variant == 1, frame % 4);
  else if (key == "duck_bars")
    duckBars(cr, w, h, variant);
  else
    return false;
  return true;
}

} // namespace gr
