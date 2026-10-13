// Wardrobe (Level 37) in the style of enemy_art.cpp: smooth shapes with a
// dark ink line, lit warm from the bulbs (top left), shadows going to
// crimson-brown. The Mannequin is a jointed shop dummy in pale wood with a
// blank egg head, the Powder Puffer a fat pink powder puff that squeezes a
// rubber bulb to fire its atomiser, the Rack Runner a garment rack on
// casters with angry eyes in its coats. The Gorgazilla suit hangs on its
// hanger and can be worn; the duck dresses up in a feather boa and a top
// hat. Each is drawn in screen pixels into a texture with a 32 px margin,
// the cell box at (32, 32) .. (32 + w, 32 + h), facing right.

#include "assets/enemy_art_wardrobe37.hpp"

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
constexpr Color kInk = rgb(30, 14, 18);
constexpr Color kWoodPale = rgb(236, 206, 166);
constexpr Color kWoodShade = rgb(176, 130, 96);
constexpr Color kJoint = rgb(150, 100, 70);
constexpr Color kBrass = rgb(214, 170, 80);
constexpr Color kBrassHi = rgb(255, 232, 150);
constexpr Color kBrassLo = rgb(124, 84, 30);
constexpr Color kChrome = rgb(206, 210, 220);
constexpr Color kChromeLo = rgb(96, 100, 114);
constexpr Color kPuff = rgb(255, 178, 204);
constexpr Color kPuffLo = rgb(214, 110, 150);
constexpr Color kSick = rgb(170, 214, 110);
constexpr Color kSickLo = rgb(96, 140, 60);
constexpr Color kZilla = rgb(84, 122, 78);
constexpr Color kZillaLo = rgb(40, 62, 42);
constexpr Color kSpine = rgb(224, 226, 210);

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

void hgrad(cairo_t* cr, double x0, double x1, std::initializer_list<std::pair<double, Color>> stops, double a = 1.0,
  bool preserve = false)
{
  cairo_pattern_t* g = cairo_pattern_create_linear(x0, 0, x1, 0);
  for (const auto& s : stops)
    stop(g, s.first, s.second, a);
  cairo_set_source(cr, g);
  if (preserve)
    cairo_fill_preserve(cr);
  else
    cairo_fill(cr);
  cairo_pattern_destroy(g);
}

// A radial fill, lit from (lx, ly).
void rgrad(cairo_t* cr, double lx, double ly, double r0, double cx, double cy, double r1, Color hi, Color mid, Color lo,
  bool preserve = false)
{
  cairo_pattern_t* g = cairo_pattern_create_radial(lx, ly, r0, cx, cy, r1);
  stop(g, 0.0, hi);
  stop(g, 0.55, mid);
  stop(g, 1.0, lo);
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

void frameBox(cairo_t* cr, double w, double h, double dw, double dh)
{
  cairo_translate(cr, kM, kM);
  cairo_scale(cr, w / dw, h / dh);
}

// A limb: an ink outline under a coloured stroke through the points.
void limb(cairo_t* cr, std::initializer_list<std::array<double, 2>> pts, double width, Color c)
{
  for (int pass = 0; pass < 2; ++pass)
  {
    bool first = true;
    for (const auto& p : pts)
    {
      if (first)
        cairo_move_to(cr, p[0], p[1]);
      else
        cairo_line_to(cr, p[0], p[1]);
      first = false;
    }
    stroke(cr, pass == 0 ? kInk : c, pass == 0 ? width + 3.0 : width);
  }
}

// A ball joint.
void joint(cairo_t* cr, double x, double y, double r, Color c)
{
  circle(cr, x, y, r);
  rgrad(cr, x - r * 0.4, y - r * 0.4, 0.5, x, y, r, lighten(c, 0.35f), c, darken(c, 0.35f), true);
  stroke(cr, kInk, 1.4);
}

void text(cairo_t* cr, const char* s, double cx, double baseline, double size, Color c, double a = 1.0, double angle = 0.0)
{
  selectGameFont(cr);
  cairo_set_font_size(cr, size);
  cairo_text_extents_t e;
  cairo_text_extents(cr, s, &e);
  cairo_save(cr);
  cairo_translate(cr, cx, baseline);
  cairo_rotate(cr, angle);
  cairo_move_to(cr, -e.width * 0.5 - e.x_bearing, 0);
  setRgba(cr, c, a);
  cairo_show_text(cr, s);
  cairo_restore(cr);
}

// --- Mannequin ------------------------------------------------------------------------------------

// The poses of its walk, caught mid-step: for each, the far and near arm's
// elbow and hand, the far and near leg's knee and foot (design units, 96
// x 160, facing right, the feet on y 158).
struct Pose
{
  double farElbow[2], farHand[2], nearElbow[2], nearHand[2];
  double farKnee[2], farFoot[2], nearKnee[2], nearFoot[2];
};

constexpr Pose kPoses[4] = {
  // 0: arms reaching ahead, low; feet together.
  {{70, 64}, {86, 70}, {68, 66}, {88, 74}, {44, 128}, {42, 157}, {54, 128}, {56, 157}},
  // 1: the near arm up as if to grab, a long stride.
  {{58, 70}, {66, 88}, {76, 40}, {90, 30}, {38, 126}, {26, 157}, {62, 126}, {70, 157}},
  // 2: both arms out, the far leg forward.
  {{74, 56}, {92, 52}, {72, 60}, {94, 58}, {56, 126}, {66, 157}, {40, 126}, {30, 157}},
  // 3: arms swinging, mid-stride.
  {{40, 70}, {34, 90}, {72, 70}, {86, 84}, {60, 126}, {70, 157}, {44, 126}, {34, 157}},
};

void mannequin(cairo_t* cr, double w, double h, int pose, bool twitch, int outfit)
{
  cairo_save(cr);
  frameBox(cr, w, h, 96, 160);
  const Pose& p = kPoses[pose % 4];
  const Color far = darken(kWoodPale, 0.22f), near = kWoodPale;
  const double sx0 = 38, sx1 = 60, sy = 46; // shoulders (far, near)
  const double hx0 = 42, hx1 = 54, hy = 100; // hips
  // Far limbs, behind the body.
  limb(cr, {{sx0, sy}, {p.farElbow[0], p.farElbow[1]}, {p.farHand[0], p.farHand[1]}}, 8, far);
  joint(cr, p.farElbow[0], p.farElbow[1], 4.5, darken(kJoint, 0.15f));
  ellipse(cr, p.farHand[0], p.farHand[1], 5.5, 4.5);
  setRgba(cr, far, 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.4);
  limb(cr, {{hx0, hy}, {p.farKnee[0], p.farKnee[1]}, {p.farFoot[0], p.farFoot[1] - 3}}, 10, far);
  joint(cr, p.farKnee[0], p.farKnee[1], 5, darken(kJoint, 0.15f));
  ellipse(cr, p.farFoot[0] + 4, p.farFoot[1] - 2, 9, 3.5);
  setRgba(cr, far, 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.4);
  // The torso: a shop dummy's form, a chest, a pinched waist, the hips.
  cairo_move_to(cr, 34, 44);
  cairo_curve_to(cr, 40, 38, 58, 38, 64, 44);
  cairo_curve_to(cr, 68, 60, 62, 76, 58, 86);
  cairo_curve_to(cr, 64, 94, 62, 104, 56, 106);
  cairo_line_to(cr, 40, 106);
  cairo_curve_to(cr, 34, 104, 34, 94, 40, 86);
  cairo_curve_to(cr, 36, 76, 30, 60, 34, 44);
  cairo_close_path(cr);
  hgrad(cr, 30, 68, {{0.0, kWoodShade}, {0.35, lighten(kWoodPale, 0.2f)}, {0.7, kWoodPale}, {1.0, kWoodShade}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  // The waist seam and the screw at the hip.
  cairo_move_to(cr, 40, 86);
  cairo_curve_to(cr, 46, 89, 52, 89, 58, 86);
  stroke(cr, kWoodShade, 1.6);
  circle(cr, 48, 96, 2.2);
  setRgba(cr, kBrass, 1.0);
  cairo_fill(cr);
  // The neck and the head (cocked when it twitches).
  limb(cr, {{49, 40}, {50, 30}}, 7, kWoodPale);
  cairo_save(cr);
  cairo_translate(cr, 51, 30);
  cairo_rotate(cr, twitch ? 0.35 : 0.0);
  ellipse(cr, 1, -12, 12, 15.5);
  rgrad(cr, -4, -20, 1, 1, -12, 17, lighten(kWoodPale, 0.5f), kWoodPale, kWoodShade, true);
  stroke(cr, kInk, 2.0);
  // No face: just the faint ridge of a nose and a brow.
  cairo_move_to(cr, 10, -16);
  cairo_curve_to(cr, 13, -12, 13, -9, 11, -6);
  stroke(cr, kWoodShade, 1.6, 0.7);
  cairo_move_to(cr, 2, -17);
  cairo_curve_to(cr, 5, -19, 9, -19, 11, -17);
  stroke(cr, kWoodShade, 1.4, 0.55);
  cairo_restore(cr);
  // What it wears.
  if (outfit == 0)
  {
    // A tape measure slung round its neck, hanging down both sides.
    cairo_move_to(cr, 40, 44);
    cairo_curve_to(cr, 44, 50, 56, 50, 60, 44);
    cairo_move_to(cr, 42, 46);
    cairo_curve_to(cr, 40, 60, 42, 70, 40, 82);
    cairo_move_to(cr, 58, 46);
    cairo_curve_to(cr, 60, 58, 58, 66, 61, 74);
    stroke(cr, kInk, 6.0);
    cairo_move_to(cr, 40, 44);
    cairo_curve_to(cr, 44, 50, 56, 50, 60, 44);
    cairo_move_to(cr, 42, 46);
    cairo_curve_to(cr, 40, 60, 42, 70, 40, 82);
    cairo_move_to(cr, 58, 46);
    cairo_curve_to(cr, 60, 58, 58, 66, 61, 74);
    stroke(cr, rgb(250, 214, 60), 4.0);
    for (double y = 52; y < 82; y += 4)
    {
      cairo_move_to(cr, 39.5, y);
      cairo_line_to(cr, 42.5, y);
      stroke(cr, kInk, 0.8, 0.8);
    }
  }
  else if (outfit == 1)
  {
    // A half-made waistcoat, pinned, a paper pattern tag.
    cairo_move_to(cr, 36, 46);
    cairo_curve_to(cr, 42, 44, 46, 50, 48, 58);
    cairo_line_to(cr, 50, 90);
    cairo_line_to(cr, 40, 88);
    cairo_curve_to(cr, 36, 76, 32, 60, 36, 46);
    cairo_close_path(cr);
    hgrad(cr, 32, 52, {{0.0, rgb(60, 30, 90)}, {0.6, rgb(120, 70, 160)}, {1.0, rgb(70, 40, 110)}}, 1.0, true);
    stroke(cr, kInk, 1.6);
    cairo_move_to(cr, 62, 46);
    cairo_curve_to(cr, 56, 44, 52, 50, 50, 58);
    cairo_line_to(cr, 50, 90);
    cairo_line_to(cr, 58, 88);
    cairo_curve_to(cr, 62, 76, 66, 60, 62, 46);
    cairo_close_path(cr);
    hgrad(cr, 50, 66, {{0.0, rgb(130, 80, 170)}, {1.0, rgb(60, 30, 90)}}, 1.0, true);
    stroke(cr, kInk, 1.6);
    for (const double y : {64.0, 74.0, 84.0})
    {
      circle(cr, 51.5, y, 1.8);
      setRgba(cr, kBrassHi, 1.0);
      cairo_fill(cr);
    }
    for (const double y : {52.0, 66.0})
    {
      cairo_move_to(cr, 60, y);
      cairo_line_to(cr, 66, y - 3);
      stroke(cr, kChrome, 1.2);
      circle(cr, 66, y - 3, 1.5);
      setRgba(cr, rgb(240, 60, 60), 1.0);
      cairo_fill(cr);
    }
  }
  else
  {
    // A sash across the chest and a hat with a long feather.
    cairo_move_to(cr, 36, 46);
    cairo_line_to(cr, 44, 44);
    cairo_line_to(cr, 62, 84);
    cairo_line_to(cr, 54, 88);
    cairo_close_path(cr);
    hgrad(cr, 36, 62, {{0.0, rgb(200, 30, 50)}, {1.0, rgb(120, 10, 30)}}, 1.0, true);
    stroke(cr, kInk, 1.4);
    cairo_save(cr);
    cairo_translate(cr, 51, 30);
    cairo_rotate(cr, twitch ? 0.35 : 0.0);
    ellipse(cr, 1, -22, 18, 4);
    vgrad(cr, -26, -18, {{0.0, rgb(70, 60, 110)}, {1.0, rgb(30, 24, 50)}}, 1.0, true);
    stroke(cr, kInk, 1.4);
    cairo_move_to(cr, -8, -22);
    cairo_curve_to(cr, -8, -34, 10, -34, 10, -22);
    cairo_close_path(cr);
    vgrad(cr, -34, -22, {{0.0, rgb(90, 80, 140)}, {1.0, rgb(40, 34, 70)}}, 1.0, true);
    stroke(cr, kInk, 1.4);
    cairo_move_to(cr, -4, -28);
    cairo_curve_to(cr, -18, -44, -30, -40, -36, -30);
    cairo_curve_to(cr, -26, -34, -16, -32, -4, -26);
    cairo_close_path(cr);
    vgrad(cr, -44, -26, {{0.0, rgb(255, 250, 230)}, {1.0, rgb(230, 200, 150)}}, 1.0, true);
    stroke(cr, kInk, 1.0);
    cairo_restore(cr);
  }
  // Near limbs, in front.
  joint(cr, sx1, sy, 5.5, kJoint);
  joint(cr, sx0, sy, 5, darken(kJoint, 0.15f));
  const double neX = twitch ? 74 : p.nearElbow[0], neY = twitch ? 34 : p.nearElbow[1];
  const double nhX = twitch ? 82 : p.nearHand[0], nhY = twitch ? 14 : p.nearHand[1];
  limb(cr, {{sx1, sy}, {neX, neY}, {nhX, nhY}}, 8, near);
  joint(cr, neX, neY, 4.5, kJoint);
  ellipse(cr, nhX, nhY, 5.5, 4.5);
  setRgba(cr, near, 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.4);
  joint(cr, hx0, hy, 5, darken(kJoint, 0.15f));
  limb(cr, {{hx1, hy}, {p.nearKnee[0], p.nearKnee[1]}, {p.nearFoot[0], p.nearFoot[1] - 3}}, 10, near);
  joint(cr, hx1, hy, 5.5, kJoint);
  joint(cr, p.nearKnee[0], p.nearKnee[1], 5, kJoint);
  ellipse(cr, p.nearFoot[0] + 4, p.nearFoot[1] - 2, 9, 3.5);
  setRgba(cr, near, 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.4);
  cairo_restore(cr);
}

// --- Powder Puffer --------------------------------------------------------------------------------

// The puff's fluffy outline round (cx, cy): scallops all the way round.
void fluff(cairo_t* cr, double cx, double cy, double rx, double ry)
{
  const int n = 22;
  for (int k = 0; k <= n; ++k)
  {
    const double a = 2 * kPi * k / n;
    const double rr = 1.0 + (k % 2 ? 0.06 : -0.02);
    const double x = cx + std::cos(a) * rx * rr, y = cy + std::sin(a) * ry * rr;
    if (k == 0)
      cairo_move_to(cr, x, y);
    else
    {
      const double am = 2 * kPi * (k - 0.5) / n;
      cairo_curve_to(cr, cx + std::cos(am) * rx * 1.12, cy + std::sin(am) * ry * 1.12, cx + std::cos(am) * rx * 1.12,
        cy + std::sin(am) * ry * 1.12, x, y);
    }
  }
  cairo_close_path(cr);
}

// The carrier's expiry date, stamped across its middle: baked on its own so
// it reads the right way round whichever way the carrier faces. `mirrored`
// puts it where the flipped body has its middle.
void expLabel(cairo_t* cr, double w, double h, bool mirrored)
{
  cairo_save(cr);
  frameBox(cr, w, h, 128, 128);
  const double x = mirrored ? 128.0 - 66.0 : 66.0, y = 92.0, a = mirrored ? 0.12 : -0.12;
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_rotate(cr, a);
  roundRect(cr, -30, -10, 60, 16, 3);
  setRgba(cr, rgb(240, 250, 210), 0.9);
  cairo_fill_preserve(cr);
  stroke(cr, rgb(40, 70, 20), 1.4);
  cairo_restore(cr);
  text(cr, "EXP 09/26", x, y + 4, 10.5, rgb(30, 60, 10), 1.0, a);
  cairo_restore(cr);
}

void powderPuffer(cairo_t* cr, double w, double h, int v, int blink)
{
  cairo_save(cr);
  frameBox(cr, w, h, 128, 128);
  const bool carrier = v >= 3;
  const int act = v % 3; // 0 hover, 1 squeeze, 2 puff
  const Color body = carrier ? kSick : kPuff, bodyLo = carrier ? kSickLo : kPuffLo;
  // The squeeze bulb at its back, on a tube.
  const double bx = 22, by = 70;
  const double brx = act == 1 ? 9 : 16, bry = act == 1 ? 21 : 15;
  cairo_move_to(cr, bx + 8, by);
  cairo_curve_to(cr, 36, by - 4, 40, by - 2, 46, by);
  stroke(cr, kInk, 6.0);
  cairo_move_to(cr, bx + 8, by);
  cairo_curve_to(cr, 36, by - 4, 40, by - 2, 46, by);
  stroke(cr, rgb(170, 40, 60), 3.5);
  ellipse(cr, bx, by, brx, bry);
  rgrad(cr, bx - 5, by - 6, 1, bx, by, std::max(brx, bry), rgb(255, 120, 130), rgb(200, 40, 60), rgb(110, 10, 30), true);
  stroke(cr, kInk, 2.0);
  // A net over the bulb.
  for (int k = -2; k <= 2; ++k)
  {
    cairo_move_to(cr, bx + k * brx * 0.4, by - bry * 0.95);
    cairo_line_to(cr, bx + k * brx * 0.4, by + bry * 0.95);
    stroke(cr, rgb(250, 210, 120), 0.9, 0.6);
  }
  if (act == 1)
  {
    // Squeeze marks.
    for (const double s : {-1.0, 1.0})
    {
      cairo_move_to(cr, bx + s * (brx + 6), by - 8);
      cairo_line_to(cr, bx + s * (brx + 3), by);
      cairo_line_to(cr, bx + s * (brx + 6), by + 8);
      stroke(cr, kInk, 1.6, 0.8);
    }
  }
  // The puff.
  const double cx = 70, cy = 66;
  const double rx = act == 1 ? 46 : 42, ry = act == 1 ? 44 : 40;
  fluff(cr, cx, cy, rx, ry);
  rgrad(cr, cx - 16, cy - 18, 2, cx, cy, rx + 6, lighten(body, 0.55f), body, bodyLo, true);
  stroke(cr, kInk, 2.2);
  // The velvet ring and the satin ribbon on top.
  ellipse(cr, cx, cy, rx * 0.62, ry * 0.6);
  stroke(cr, lighten(body, 0.4f), 2.0, 0.6);
  cairo_move_to(cr, cx - 8, cy - ry + 4);
  cairo_curve_to(cr, cx - 10, cy - ry - 14, cx + 10, cy - ry - 14, cx + 8, cy - ry + 4);
  stroke(cr, kInk, 7.0);
  cairo_move_to(cr, cx - 8, cy - ry + 4);
  cairo_curve_to(cr, cx - 10, cy - ry - 14, cx + 10, cy - ry - 14, cx + 8, cy - ry + 4);
  stroke(cr, carrier ? rgb(200, 220, 140) : rgb(255, 240, 248), 4.5);
  ellipse(cr, cx, cy - ry + 3, 7, 4);
  setRgba(cr, carrier ? rgb(150, 180, 90) : rgb(255, 210, 230), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.2);
  // Its eyes, under heavy lashes (shut on the blink), looking ahead.
  for (const double ex : {74.0, 92.0})
  {
    if (blink)
    {
      cairo_move_to(cr, ex - 6, cy - 6);
      cairo_curve_to(cr, ex - 3, cy - 2, ex + 3, cy - 2, ex + 6, cy - 6);
      stroke(cr, kInk, 2.2);
    }
    else
    {
      ellipse(cr, ex, cy - 6, 6, act == 1 ? 7.5 : 6.5);
      setRgba(cr, rgb(255, 255, 255), 1.0);
      cairo_fill_preserve(cr);
      stroke(cr, kInk, 1.6);
      circle(cr, ex + 2, cy - 5, 3.2);
      setRgba(cr, kInk, 1.0);
      cairo_fill(cr);
      circle(cr, ex + 3, cy - 6.5, 1.0);
      setRgba(cr, rgb(255, 255, 255), 1.0);
      cairo_fill(cr);
    }
    for (int k = 0; k < 3; ++k)
    {
      cairo_move_to(cr, ex - 4 + k * 4, cy - 12);
      cairo_line_to(cr, ex - 6 + k * 5, cy - 17);
      stroke(cr, kInk, 1.4);
    }
  }
  // Rouge on its cheeks, a pout.
  glow(cr, 98, cy + 6, 9, carrier ? rgb(120, 170, 60) : rgb(255, 90, 130), 0.6);
  cairo_move_to(cr, 84, cy + 12);
  cairo_curve_to(cr, 87, cy + (act == 2 ? 18 : 15), 91, cy + (act == 2 ? 18 : 15), 94, cy + 12);
  stroke(cr, kInk, 1.8);
  // The atomiser nozzle at its front.
  cairo_rectangle(cr, 108, 70, 14, 8);
  hgrad(cr, 108, 122, {{0.0, kBrassLo}, {0.4, kBrassHi}, {1.0, kBrass}}, 1.0, true);
  stroke(cr, kInk, 1.4);
  ellipse(cr, 123, 74, 3, 5);
  setRgba(cr, kBrassLo, 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.2);
  if (act == 2)
  {
    for (int k = 0; k < 5; ++k)
    {
      const double a = -0.5 + k * 0.25;
      glow(cr, 124 + std::cos(a) * 8, 74 + std::sin(a) * 8, 7, carrier ? rgb(200, 255, 150) : rgb(255, 235, 245), 0.9);
    }
  }
  if (carrier)
  {
    // A few sick spots (the expiry date goes on top, see expLabel).
    for (const auto& s : {std::array<double, 3>{50, 46, 4}, {60, 90, 3}, {96, 84, 3.5}})
    {
      circle(cr, s[0], s[1], s[2]);
      setRgba(cr, rgb(110, 150, 40), 0.7);
      cairo_fill(cr);
    }
  }
  cairo_restore(cr);
}

// --- Rack Runner ----------------------------------------------------------------------------------

// A coat on a hanger at (x, top), `len` long, in c, swung by `swing`
// radians about the hanger's hook.
void coat(cairo_t* cr, double x, double top, double len, Color c, double swing, int kind)
{
  cairo_save(cr);
  cairo_translate(cr, x, top);
  cairo_rotate(cr, swing);
  cairo_move_to(cr, 0, -2);
  cairo_curve_to(cr, 5, -2, 4, -9, 0, -8);
  stroke(cr, kChromeLo, 1.6);
  cairo_move_to(cr, -14, 7);
  cairo_line_to(cr, 0, 0);
  cairo_line_to(cr, 14, 7);
  stroke(cr, kInk, 2.4);
  if (kind == 1)
  {
    cairo_move_to(cr, -9, 6);
    cairo_line_to(cr, 9, 6);
    cairo_curve_to(cr, 8, len * 0.4, 22, len * 0.8, 24, len);
    cairo_line_to(cr, -24, len);
    cairo_curve_to(cr, -22, len * 0.8, -8, len * 0.4, -9, 6);
  }
  else
  {
    cairo_move_to(cr, -15, 6);
    cairo_line_to(cr, 15, 6);
    cairo_line_to(cr, 19, len);
    cairo_line_to(cr, -19, len);
  }
  cairo_close_path(cr);
  hgrad(cr, -24, 24, {{0.0, darken(c, 0.45f)}, {0.35, lighten(c, 0.15f)}, {0.65, c}, {1.0, darken(c, 0.5f)}}, 1.0, true);
  stroke(cr, kInk, 1.6);
  cairo_move_to(cr, 0, 8);
  cairo_line_to(cr, 0, len * 0.55);
  stroke(cr, darken(c, 0.4f), 1.4);
  for (int b = 0; b < 3 && kind != 1; ++b)
  {
    circle(cr, 3, 18 + b * 14, 1.8);
    setRgba(cr, kBrassHi, 1.0);
    cairo_fill(cr);
  }
  cairo_restore(cr);
}

void caster(cairo_t* cr, double x, double y, int frame, bool rolling)
{
  cairo_rectangle(cr, x - 3, y - 14, 6, 6);
  setRgba(cr, kChromeLo, 1.0);
  cairo_fill(cr);
  circle(cr, x, y - 4, 7);
  rgrad(cr, x - 2, y - 6, 0.5, x, y - 4, 7, rgb(90, 90, 100), rgb(40, 40, 46), rgb(14, 14, 18), true);
  stroke(cr, kInk, 1.4);
  const double a = frame * kPi * 0.25 + (rolling ? 0.4 : 0.0);
  cairo_move_to(cr, x - std::cos(a) * 5, y - 4 - std::sin(a) * 5);
  cairo_line_to(cr, x + std::cos(a) * 5, y - 4 + std::sin(a) * 5);
  stroke(cr, kChrome, 1.4);
}

void rackRunner(cairo_t* cr, double w, double h, int v, int frame)
{
  cairo_save(cr);
  frameBox(cr, w, h, 192, 192);
  static constexpr Color kCoats[7] = {rgb(170, 34, 54), rgb(46, 76, 150), rgb(226, 176, 60), rgb(70, 120, 76), rgb(120, 64, 150),
    rgb(232, 222, 204), rgb(200, 96, 46)};
  const bool rolling = v == 2, rattle = v == 1;
  const double lean = rolling ? -0.18 : 0.0;
  // The frame: two uprights and a top rail, a base bar.
  for (const double x : {18.0, 174.0})
  {
    cairo_move_to(cr, x, 168);
    cairo_line_to(cr, x + (rolling ? 6 : 0), 16);
    stroke(cr, kInk, 8.0);
    cairo_move_to(cr, x, 168);
    cairo_line_to(cr, x + (rolling ? 6 : 0), 16);
    stroke(cr, kChrome, 5.0);
  }
  const double railY = 20 + (rattle && frame ? 2 : 0);
  // Coats behind the rail's line, crammed together.
  for (int k = 0; k < 7; ++k)
  {
    const double x = 34 + k * 21.5;
    double swing = lean;
    if (rattle)
      swing = (k % 2 == frame % 2 ? 0.14 : -0.14);
    coat(cr, x, railY, 128 + (k * 37 % 3) * 8, kCoats[k], swing, k % 3 == 1 ? 1 : 0);
  }
  cairo_move_to(cr, 10, railY);
  cairo_line_to(cr, 182, railY);
  stroke(cr, kInk, 8.0);
  cairo_move_to(cr, 10, railY);
  cairo_line_to(cr, 182, railY);
  stroke(cr, kChrome, 5.0);
  for (const double x : {10.0, 182.0})
  {
    circle(cr, x, railY, 6);
    setRgba(cr, kChrome, 1.0);
    cairo_fill_preserve(cr);
    stroke(cr, kInk, 1.6);
  }
  // The glare between the coats: two angry eyes in the dark gap.
  const double ey = 82;
  cairo_move_to(cr, 108, ey - 20);
  cairo_curve_to(cr, 124, ey - 24, 150, ey - 22, 160, ey - 14);
  cairo_curve_to(cr, 162, ey + 2, 150, ey + 16, 132, ey + 14);
  cairo_curve_to(cr, 114, ey + 12, 104, ey + 2, 108, ey - 20);
  cairo_close_path(cr);
  setRgba(cr, rgb(20, 8, 12), 0.95);
  cairo_fill(cr);
  for (const double ex : {124.0, 146.0})
  {
    ellipse(cr, ex, ey - 2, 8, 6);
    setRgba(cr, rgb(255, 236, 120), 1.0);
    cairo_fill(cr);
    glow(cr, ex, ey - 2, 14, rgb(255, 200, 80), 0.5);
    circle(cr, ex + 3, ey - 1, 3);
    setRgba(cr, kInk, 1.0);
    cairo_fill(cr);
    cairo_move_to(cr, ex - 10, ey - 12 + (ex > 130 ? 4 : 0));
    cairo_line_to(cr, ex + 9, ey - 8 - (ex > 130 ? 0 : -2));
    stroke(cr, kInk, 3.0);
  }
  // A tag hanging off the rail: its size.
  cairo_move_to(cr, 64, railY + 2);
  cairo_line_to(cr, 70, railY + 26);
  stroke(cr, kInk, 1.0);
  roundRect(cr, 62, railY + 24, 18, 12, 2);
  setRgba(cr, rgb(250, 244, 230), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.0);
  // The base and the casters.
  cairo_move_to(cr, 12, 168);
  cairo_line_to(cr, 180, 168);
  stroke(cr, kInk, 9.0);
  cairo_move_to(cr, 12, 168);
  cairo_line_to(cr, 180, 168);
  stroke(cr, kChrome, 6.0);
  for (const double x : {18.0, 62.0, 130.0, 174.0})
    caster(cr, x, 190, frame + (x > 100 ? 1 : 0), rolling);
  if (rattle)
  {
    for (int k = 0; k < 3; ++k)
    {
      cairo_move_to(cr, 40 + k * 56, 4);
      cairo_line_to(cr, 46 + k * 56, -2);
      cairo_move_to(cr, 52 + k * 56, 4);
      cairo_line_to(cr, 58 + k * 56, -2);
      stroke(cr, kInk, 2.0, 0.8);
    }
  }
  cairo_restore(cr);
}

// --- The Gorgazilla suit --------------------------------------------------------------------------

// A spiky rubber monster's back plates along a path of points.
void spines(cairo_t* cr, std::initializer_list<std::array<double, 3>> pts)
{
  for (const auto& p : pts)
  {
    cairo_move_to(cr, p[0] - 6, p[1] + 3);
    cairo_line_to(cr, p[0] + std::sin(p[2]) * 12, p[1] - std::cos(p[2]) * 12);
    cairo_line_to(cr, p[0] + 6, p[1] + 3);
    cairo_close_path(cr);
    vgrad(cr, p[1] - 12, p[1] + 3, {{0.0, lighten(kSpine, 0.3f)}, {1.0, darken(kSpine, 0.3f)}}, 1.0, true);
    stroke(cr, kInk, 1.4);
  }
}

void suitHanging(cairo_t* cr, double w, double h)
{
  cairo_save(cr);
  frameBox(cr, w, h, 96, 160);
  // The hanger.
  cairo_move_to(cr, 48, 4);
  cairo_curve_to(cr, 54, 4, 54, -4, 48, -4);
  stroke(cr, kChromeLo, 2.0);
  // The tail, down to the floor and pooling.
  cairo_move_to(cr, 30, 96);
  cairo_curve_to(cr, 18, 120, 10, 150, 30, 158);
  cairo_curve_to(cr, 44, 160, 52, 156, 60, 150);
  cairo_curve_to(cr, 44, 148, 36, 132, 44, 100);
  cairo_close_path(cr);
  hgrad(cr, 10, 60, {{0.0, kZillaLo}, {0.5, kZilla}, {1.0, kZillaLo}}, 1.0, true);
  stroke(cr, kInk, 1.8);
  // The body, limp: a sagging green sack with the belly scales.
  cairo_move_to(cr, 26, 18);
  cairo_curve_to(cr, 40, 8, 58, 8, 70, 18);
  cairo_curve_to(cr, 74, 50, 76, 90, 66, 112);
  cairo_curve_to(cr, 56, 120, 38, 120, 28, 112);
  cairo_curve_to(cr, 20, 90, 22, 50, 26, 18);
  cairo_close_path(cr);
  hgrad(cr, 20, 76, {{0.0, kZillaLo}, {0.35, lighten(kZilla, 0.15f)}, {0.7, kZilla}, {1.0, kZillaLo}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  for (int k = 0; k < 6; ++k)
  {
    ellipse(cr, 50, 40 + k * 12, 12 - k * 0.6, 4.5);
    setRgba(cr, rgb(186, 184, 128), 0.9);
    cairo_fill_preserve(cr);
    stroke(cr, kZillaLo, 1.0);
  }
  // The zip down the back, open a little.
  cairo_move_to(cr, 30, 24);
  cairo_line_to(cr, 28, 96);
  stroke(cr, kChrome, 1.4);
  // Limp arms.
  limb(cr, {{28, 30}, {20, 60}, {22, 86}}, 10, kZilla);
  limb(cr, {{68, 30}, {76, 60}, {72, 86}}, 10, kZilla);
  for (const double x : {22.0, 72.0})
    for (int c = -1; c <= 1; ++c)
    {
      cairo_move_to(cr, x + c * 3, 88);
      cairo_line_to(cr, x + c * 4, 94);
      stroke(cr, rgb(240, 236, 210), 2.0);
    }
  // The legs, empty feet.
  limb(cr, {{38, 112}, {36, 140}, {38, 154}}, 12, kZilla);
  limb(cr, {{58, 112}, {60, 140}, {62, 154}}, 12, kZilla);
  // The head flopped forward over its chest, its eyes glassy, tongue out.
  cairo_move_to(cr, 34, 14);
  cairo_curve_to(cr, 40, 0, 70, 0, 78, 16);
  cairo_curve_to(cr, 84, 30, 82, 46, 72, 50);
  cairo_curve_to(cr, 62, 52, 46, 46, 40, 38);
  cairo_curve_to(cr, 34, 30, 32, 20, 34, 14);
  cairo_close_path(cr);
  hgrad(cr, 32, 84, {{0.0, kZillaLo}, {0.4, lighten(kZilla, 0.2f)}, {1.0, kZillaLo}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  // The jaw hanging open, the teeth.
  cairo_move_to(cr, 48, 42);
  cairo_curve_to(cr, 58, 50, 70, 52, 78, 44);
  stroke(cr, kInk, 2.4);
  for (int k = 0; k < 4; ++k)
  {
    cairo_move_to(cr, 52 + k * 6, 44 + k * 1.5);
    cairo_line_to(cr, 54 + k * 6, 49 + k * 1.5);
    cairo_line_to(cr, 56 + k * 6, 45 + k * 1.5);
    setRgba(cr, rgb(250, 248, 230), 1.0);
    cairo_fill(cr);
  }
  ellipse(cr, 66, 26, 6, 5);
  setRgba(cr, rgb(250, 230, 120), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.4);
  cairo_move_to(cr, 62, 26);
  cairo_line_to(cr, 70, 26);
  stroke(cr, kInk, 2.0);
  spines(cr, {{40, 12, -0.6}, {30, 26, -0.9}, {24, 46, -1.2}, {22, 66, -1.4}, {24, 86, -1.5}});
  // The costume tag.
  roundRect(cr, 52, -2, 30, 11, 2);
  setRgba(cr, rgb(250, 244, 226), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.0);
  text(cr, "GORGA", 67, 6.5, 7.0, rgb(160, 30, 30));
  cairo_restore(cr);
}

// The suit worn: an upright rubber monster, the runner's eyes peering out
// of its jaws. v 0 standing, 1/2 the stride, 3 in the air.
void suitWorn(cairo_t* cr, double w, double h, int v)
{
  cairo_save(cr);
  frameBox(cr, w, h, 128, 192);
  const double step = v == 1 ? 1.0 : v == 2 ? -1.0 : 0.0;
  const bool air = v == 3;
  // The tail, swinging behind.
  cairo_move_to(cr, 40, 120);
  cairo_curve_to(cr, 20, 140 + step * 6, 8, 160, 2, 180 - (air ? 20 : 0));
  cairo_curve_to(cr, 14, 176, 30, 160, 52, 140);
  cairo_close_path(cr);
  hgrad(cr, 0, 52, {{0.0, kZillaLo}, {1.0, kZilla}}, 1.0, true);
  stroke(cr, kInk, 1.8);
  // Far leg.
  limb(cr, {{56, 140}, {50 - step * 10, 164}, {48 - step * 12, air ? 176.0 : 186.0}}, 18, darken(kZilla, 0.25f));
  // The body.
  cairo_move_to(cr, 36, 70);
  cairo_curve_to(cr, 38, 52, 90, 50, 94, 70);
  cairo_curve_to(cr, 100, 100, 98, 130, 86, 146);
  cairo_curve_to(cr, 72, 154, 52, 154, 42, 146);
  cairo_curve_to(cr, 32, 126, 32, 96, 36, 70);
  cairo_close_path(cr);
  hgrad(cr, 32, 100, {{0.0, kZillaLo}, {0.35, lighten(kZilla, 0.18f)}, {0.7, kZilla}, {1.0, kZillaLo}}, 1.0, true);
  stroke(cr, kInk, 2.2);
  for (int k = 0; k < 6; ++k)
  {
    ellipse(cr, 74 + k * 0.5, 84 + k * 11, 14 - k, 5);
    setRgba(cr, rgb(186, 184, 128), 0.95);
    cairo_fill_preserve(cr);
    stroke(cr, kZillaLo, 1.0);
  }
  spines(cr, {{40, 66, -0.5}, {34, 86, -1.0}, {34, 108, -1.3}, {38, 130, -1.6}});
  // Near leg.
  limb(cr, {{74, 142}, {78 + step * 10, 166}, {80 + step * 12, air ? 178.0 : 186.0}}, 18, kZilla);
  ellipse(cr, 84 + step * 12, air ? 180 : 187, 14, 5);
  setRgba(cr, kZilla, 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.6);
  for (int c = 0; c < 3; ++c)
  {
    cairo_move_to(cr, 92 + step * 12 + c * 3, air ? 178 : 185);
    cairo_line_to(cr, 98 + step * 12 + c * 3, air ? 180 : 188);
    stroke(cr, rgb(240, 236, 210), 2.2);
  }
  // Arms up, claws out, in a monster pose.
  limb(cr, {{86, 80}, {104, air ? 58.0 : 76.0}, {116, air ? 44.0 : 62.0}}, 12, kZilla);
  for (int c = -1; c <= 1; ++c)
  {
    cairo_move_to(cr, 116 + c * 3, air ? 44.0 : 62.0);
    cairo_line_to(cr, 122 + c * 2, air ? 36 : 54);
    stroke(cr, rgb(240, 236, 210), 2.4);
  }
  // The head: a big snout, open jaws with the runner's eyes inside.
  cairo_move_to(cr, 40, 40);
  cairo_curve_to(cr, 44, 6, 96, 0, 112, 22);
  cairo_curve_to(cr, 124, 30, 126, 44, 118, 52);
  cairo_curve_to(cr, 104, 64, 70, 70, 52, 64);
  cairo_curve_to(cr, 42, 60, 38, 50, 40, 40);
  cairo_close_path(cr);
  hgrad(cr, 38, 126, {{0.0, kZillaLo}, {0.4, lighten(kZilla, 0.22f)}, {1.0, kZilla}}, 1.0, true);
  stroke(cr, kInk, 2.2);
  // The mouth hole.
  cairo_move_to(cr, 70, 36);
  cairo_curve_to(cr, 86, 30, 112, 34, 120, 42);
  cairo_curve_to(cr, 110, 56, 84, 58, 70, 50);
  cairo_close_path(cr);
  setRgba(cr, rgb(40, 10, 16), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.6);
  for (int k = 0; k < 5; ++k)
  {
    cairo_move_to(cr, 80 + k * 8, 34 + k * 0.6);
    cairo_line_to(cr, 83 + k * 8, 40);
    cairo_line_to(cr, 86 + k * 8, 34.5 + k * 0.6);
    setRgba(cr, rgb(250, 248, 230), 1.0);
    cairo_fill(cr);
  }
  for (const double ex : {88.0, 102.0})
  {
    ellipse(cr, ex, 46, 4, 3.4);
    setRgba(cr, rgb(255, 255, 255), 1.0);
    cairo_fill(cr);
    circle(cr, ex + 1.5, 46, 1.8);
    setRgba(cr, kInk, 1.0);
    cairo_fill(cr);
  }
  // Its own (rubber) eye on top, a brow ridge.
  ellipse(cr, 84, 18, 7, 5.5);
  setRgba(cr, rgb(250, 220, 90), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kInk, 1.4);
  ellipse(cr, 85, 18, 1.8, 4.5);
  setRgba(cr, kInk, 1.0);
  cairo_fill(cr);
  cairo_move_to(cr, 74, 10);
  cairo_line_to(cr, 96, 12);
  stroke(cr, kZillaLo, 3.0);
  spines(cr, {{56, 14, -0.2}, {44, 26, -0.7}});
  cairo_restore(cr);
}

// --- The duck's boa and top hat -------------------------------------------------------------------

// A fluffy feather boa along a path: overlapping pink puffs with lighter
// tips.
void boa(cairo_t* cr, std::initializer_list<std::array<double, 2>> pts)
{
  for (const auto& p : pts)
  {
    for (int k = 0; k < 7; ++k)
    {
      const double a = k * 2 * kPi / 7;
      ellipse(cr, p[0] + std::cos(a) * 3.2, p[1] + std::sin(a) * 2.6, 3.6, 2.4, a);
      setRgba(cr, k % 2 ? rgb(255, 120, 190) : rgb(250, 80, 160), 0.9);
      cairo_fill(cr);
    }
    circle(cr, p[0] - 1, p[1] - 1, 2.2);
    setRgba(cr, rgb(255, 210, 236), 0.9);
    cairo_fill(cr);
  }
}

void duckBoaTopHat(cairo_t* cr, double w, double h, int variant)
{
  cairo_save(cr);
  cairo_translate(cr, kM, kM);
  cairo_scale(cr, w / 64.0, h / 64.0);
  if (variant == 0)
  {
    // The boa's back, round behind the duck's neck and trailing.
    boa(cr, {{30, 30}, {24, 32}, {19, 35}, {15, 39}, {12, 44}, {10, 49}});
  }
  else
  {
    // The boa's front, across its chest and down.
    boa(cr, {{33, 32}, {38, 34}, {43, 35}, {48, 34}, {51, 38}, {50, 43}, {48, 48}, {47, 53}});
    // The top hat, tipped back a little.
    cairo_save(cr);
    cairo_translate(cr, 41, 15);
    cairo_rotate(cr, -0.15);
    ellipse(cr, 0, 0, 13, 3.2);
    vgrad(cr, -3, 3, {{0.0, rgb(70, 70, 80)}, {1.0, rgb(14, 14, 20)}}, 1.0, true);
    stroke(cr, kInk, 1.2);
    cairo_move_to(cr, -8, 0);
    cairo_line_to(cr, -7, -17);
    cairo_curve_to(cr, -4, -19, 4, -19, 7, -17);
    cairo_line_to(cr, 8, 0);
    cairo_close_path(cr);
    hgrad(cr, -8, 8, {{0.0, rgb(20, 20, 26)}, {0.4, rgb(80, 80, 96)}, {1.0, rgb(14, 14, 18)}}, 1.0, true);
    stroke(cr, kInk, 1.2);
    cairo_rectangle(cr, -7.8, -5, 15.6, 3.5);
    setRgba(cr, rgb(210, 30, 50), 1.0);
    cairo_fill(cr);
    cairo_move_to(cr, -5, -16);
    cairo_line_to(cr, -5.5, -6);
    stroke(cr, rgb(255, 255, 255), 1.0, 0.35);
    cairo_restore(cr);
  }
  cairo_restore(cr);
}

} // namespace

bool drawWardrobe37Art(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame)
{
  (void)t;
  if (key == "mannequin")
    mannequin(cr, w, h, std::clamp(variant, 0, 7) % 4, variant >= 4, ((frame % 3) + 3) % 3);
  else if (key == "powder_puffer" && variant == 6)
    expLabel(cr, w, h, frame % 2 == 1);
  else if (key == "powder_puffer")
    powderPuffer(cr, w, h, std::clamp(variant, 0, 5), frame % 2);
  else if (key == "rack_runner")
    rackRunner(cr, w, h, std::clamp(variant, 0, 2), frame % 2);
  else if (key == "gorgazilla_suit")
    suitHanging(cr, w, h);
  else if (key == "gorgazilla_worn")
    suitWorn(cr, w, h, std::clamp(variant, 0, 3));
  else if (key == "duck_boa_top_hat")
    duckBoaTopHat(cr, w, h, variant);
  else
    return false;
  return true;
}

} // namespace gr
