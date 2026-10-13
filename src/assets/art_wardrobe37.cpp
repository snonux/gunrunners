// Level 37, Wardrobe (theme key wardrobe, look "wardrobe"): the costume
// department. The masses are dark mahogany wainscot, one raised panel to a
// block, lit on its top and left, a gilt bead inside the frame; now and
// then a brass heating grille, or a wardrobe door's brass keyhole plate.
// Where you stand, a crimson carpet runner with a gold braid edge. The
// one-way ledges are the gilt top bars of the makeup hall's mirror frames,
// vanity bulbs set in them. The needles are a tomato-red pincushion strip
// bristling with sewing needles and coloured pins; the ladder a wooden
// library ladder on brass rails. Behind: a ceiling lost in warm shadow,
// cream walls between pilasters hung with swagged crimson velvet and gold
// fringe, brass sconces; nearer, rows of costume racks, hat boxes stacked
// on top. The rooms' furniture is drawn by the level
// (world_wardrobe_draw.cpp).

#include "assets/art_wardrobe37.hpp"

#include "base/math.hpp"
#include "render/vector.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <string_view>

namespace gr
{

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr Color kWood = rgb(104, 50, 34);
constexpr Color kWoodHi = rgb(156, 84, 52);
constexpr Color kWoodLo = rgb(54, 22, 16);
constexpr Color kBrass = rgb(214, 170, 80);
constexpr Color kBrassHi = rgb(255, 230, 150);
constexpr Color kBrassLo = rgb(124, 84, 30);
constexpr Color kVelvet = rgb(150, 20, 40);
constexpr Color kVelvetHi = rgb(214, 62, 76);
constexpr Color kVelvetLo = rgb(66, 6, 18);
constexpr Color kCream = rgb(236, 220, 192);
constexpr Color kInk = rgb(26, 12, 16);

Color darken(Color c, float t) { return lerpColor(c, rgb(0, 0, 0), t); }
Color lighten(Color c, float t) { return lerpColor(c, rgb(255, 255, 255), t); }

void setRgba(cairo_t* cr, Color c, double a)
{
  cairo_set_source_rgba(cr, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, a);
}

void stop(cairo_pattern_t* g, double at, Color c, double a = 1.0)
{
  cairo_pattern_add_color_stop_rgba(g, at, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, a);
}

void fillLinear(cairo_t* cr, double x0, double y0, double x1, double y1, std::initializer_list<std::pair<double, Color>> stops,
  double alpha = 1.0, bool preserve = false)
{
  cairo_pattern_t* g = cairo_pattern_create_linear(x0, y0, x1, y1);
  for (const auto& s : stops)
    stop(g, s.first, s.second, alpha);
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

// A brass bulb (a vanity lamp) at (x, y), lit.
void bulb(cairo_t* cr, double x, double y, double rad)
{
  softDot(cr, x, y, rad * 3.2, rad * 3.2, rgb(255, 214, 140), 0.55);
  cairo_arc(cr, x, y, rad + 1.4, 0, 2 * kPi);
  setRgba(cr, kBrassLo, 1.0);
  cairo_fill(cr);
  cairo_pattern_t* g = cairo_pattern_create_radial(x - rad * 0.3, y - rad * 0.3, 0.5, x, y, rad);
  stop(g, 0.0, rgb(255, 255, 240));
  stop(g, 0.6, rgb(255, 236, 170));
  stop(g, 1.0, rgb(250, 190, 90));
  cairo_arc(cr, x, y, rad, 0, 2 * kPi);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
}

// Wood grain: fine wavy lines across a rectangle.
void grain(cairo_t* cr, double x, double y, double w, double h, Rng& rng, bool across)
{
  cairo_save(cr);
  cairo_rectangle(cr, x, y, w, h);
  cairo_clip(cr);
  const double span = across ? h : w;
  for (double k = 2; k < span; k += rng.range(2.5f, 5.0f))
  {
    if (across)
    {
      const double yy = y + k;
      cairo_move_to(cr, x, yy);
      cairo_curve_to(cr, x + w * 0.3, yy + rng.range(-1.5f, 1.5f), x + w * 0.6, yy + rng.range(-1.5f, 1.5f), x + w, yy + rng.range(-1, 1));
    }
    else
    {
      const double xx = x + k;
      cairo_move_to(cr, xx, y);
      cairo_curve_to(cr, xx + rng.range(-1.5f, 1.5f), y + h * 0.3, xx + rng.range(-1.5f, 1.5f), y + h * 0.6, xx + rng.range(-1, 1), y + h);
    }
    stroke(cr, rng.uniform() < 0.5f ? kWoodLo : kWoodHi, rng.range(0.5f, 1.1f), rng.range(0.15f, 0.35f));
  }
  cairo_restore(cr);
}

// One raised mahogany panel filling the block: the frame (stiles and
// rails) lit top-left, the panel bevelled in, a gilt bead round it.
void panel(cairo_t* cr)
{
  Rng rng(37011u);
  cairo_rectangle(cr, 0, 0, 64, 64);
  fillLinear(cr, 0, 0, 0, 64, {{0.0, kWoodHi}, {0.15, kWood}, {1.0, darken(kWood, 0.18f)}});
  grain(cr, 0, 0, 64, 64, rng, false);
  // The frame's joints: a dark line at the block's edges.
  cairo_rectangle(cr, 0.5, 0.5, 63, 63);
  stroke(cr, kWoodLo, 1.2, 0.9);
  cairo_move_to(cr, 1.5, 62);
  cairo_line_to(cr, 1.5, 1.5);
  cairo_line_to(cr, 62, 1.5);
  stroke(cr, lighten(kWoodHi, 0.25f), 1.0, 0.5);
  // The raised panel, bevelled: light on its top and left bevels.
  const double x0 = 10, y0 = 10, x1 = 54, y1 = 54;
  cairo_move_to(cr, x0, y0);
  cairo_line_to(cr, x1, y0);
  cairo_line_to(cr, x1 - 5, y0 + 5);
  cairo_line_to(cr, x0 + 5, y0 + 5);
  cairo_close_path(cr);
  setRgba(cr, kWoodHi, 1.0);
  cairo_fill(cr);
  cairo_move_to(cr, x0, y0);
  cairo_line_to(cr, x0 + 5, y0 + 5);
  cairo_line_to(cr, x0 + 5, y1 - 5);
  cairo_line_to(cr, x0, y1);
  cairo_close_path(cr);
  setRgba(cr, lerpColor(kWoodHi, kWood, 0.4f), 1.0);
  cairo_fill(cr);
  cairo_move_to(cr, x1, y0);
  cairo_line_to(cr, x1, y1);
  cairo_line_to(cr, x1 - 5, y1 - 5);
  cairo_line_to(cr, x1 - 5, y0 + 5);
  cairo_close_path(cr);
  setRgba(cr, kWoodLo, 1.0);
  cairo_fill(cr);
  cairo_move_to(cr, x0, y1);
  cairo_line_to(cr, x1, y1);
  cairo_line_to(cr, x1 - 5, y1 - 5);
  cairo_line_to(cr, x0 + 5, y1 - 5);
  cairo_close_path(cr);
  setRgba(cr, darken(kWoodLo, 0.2f), 1.0);
  cairo_fill(cr);
  cairo_rectangle(cr, x0 + 5, y0 + 5, x1 - x0 - 10, y1 - y0 - 10);
  fillLinear(cr, 0, y0 + 5, 0, y1 - 5, {{0.0, lerpColor(kWood, kWoodHi, 0.35f)}, {1.0, kWood}});
  grain(cr, x0 + 5, y0 + 5, x1 - x0 - 10, y1 - y0 - 10, rng, false);
  // The gilt bead.
  cairo_rectangle(cr, x0 - 2.5, y0 - 2.5, x1 - x0 + 5, y1 - y0 + 5);
  stroke(cr, kBrassLo, 2.0, 0.9);
  cairo_move_to(cr, x0 - 2.5, y1 + 2.5);
  cairo_line_to(cr, x0 - 2.5, y0 - 2.5);
  cairo_line_to(cr, x1 + 2.5, y0 - 2.5);
  stroke(cr, kBrassHi, 0.9, 0.7);
  // A little polish on the panel.
  softDot(cr, 24, 22, 14, 6, rgb(255, 200, 160), 0.12);
}

// Variant 1: a brass heating grille in the panel, its slots dark.
void grille(cairo_t* cr)
{
  cairo_rectangle(cr, 16, 22, 32, 22);
  fillLinear(cr, 0, 22, 0, 44, {{0.0, kBrassHi}, {0.5, kBrass}, {1.0, kBrassLo}}, 1.0, true);
  stroke(cr, kInk, 1.2, 0.8);
  for (int k = 0; k < 5; ++k)
  {
    cairo_rectangle(cr, 19 + k * 5.6, 25, 3.2, 16);
    setRgba(cr, rgb(30, 14, 10), 1.0);
    cairo_fill(cr);
  }
  for (const double x : {18.5, 45.5})
    for (const double y : {24.5, 41.5})
    {
      cairo_arc(cr, x, y, 1.1, 0, 2 * kPi);
      setRgba(cr, kBrassHi, 1.0);
      cairo_fill(cr);
    }
}

// Variant 2: a wardrobe door's keyhole plate and a little brass pull.
void keyhole(cairo_t* cr)
{
  cairo_save(cr);
  cairo_translate(cr, 32, 32);
  cairo_move_to(cr, 0, -14);
  cairo_curve_to(cr, 8, -14, 9, -6, 7, 0);
  cairo_curve_to(cr, 9, 6, 8, 14, 0, 14);
  cairo_curve_to(cr, -8, 14, -9, 6, -7, 0);
  cairo_curve_to(cr, -9, -6, -8, -14, 0, -14);
  cairo_close_path(cr);
  fillLinear(cr, -9, -14, 9, 14, {{0.0, kBrassHi}, {0.5, kBrass}, {1.0, kBrassLo}}, 1.0, true);
  setRgba(cr, kInk, 0.8);
  cairo_set_line_width(cr, 1.2);
  cairo_stroke(cr);
  cairo_arc(cr, 0, -3, 2.6, 0, 2 * kPi);
  setRgba(cr, rgb(20, 8, 6), 1.0);
  cairo_fill(cr);
  cairo_move_to(cr, -1.6, -2);
  cairo_line_to(cr, -2.4, 6);
  cairo_line_to(cr, 2.4, 6);
  cairo_line_to(cr, 1.6, -2);
  cairo_close_path(cr);
  cairo_fill(cr);
  cairo_restore(cr);
}

} // namespace

bool isWardrobe(const Theme& t) { return std::string_view(t.look) == "wardrobe"; }

Texture bakeWardrobeSolid(const Renderer& r, const Theme& t, int variant)
{
  (void)t;
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  panel(cr);
  if (variant == 1)
    grille(cr);
  else if (variant == 2)
    keyhole(cr);
  return img.toTexture(r, 0.0f, 0.0f);
}

// Where you stand: a crimson carpet runner over the edge, its pile catching
// the light, a gold braid along its front, the shadow it throws on the
// panel below.
Texture bakeWardrobeSolidTop(const Renderer& r, const Theme& t, float topOffset, int height)
{
  (void)t;
  VectorImage img(64, height);
  cairo_t* cr = img.cr();
  const double y = topOffset;
  {
    cairo_pattern_t* g = cairo_pattern_create_linear(0, y + 10, 0, y + 22);
    stop(g, 0.0, kInk, 0.5);
    stop(g, 1.0, kInk, 0.0);
    cairo_rectangle(cr, 0, y + 10, 64, 12);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
  }
  // The carpet.
  cairo_rectangle(cr, 0, y - 4, 64, 11);
  fillLinear(cr, 0, y - 4, 0, y + 7, {{0.0, kVelvetHi}, {0.3, kVelvet}, {1.0, kVelvetLo}});
  Rng rng(37021u);
  for (int i = 0; i < 70; ++i)
  {
    cairo_rectangle(cr, rng.range(0, 64), rng.range(float(y) - 3.0f, float(y) + 6.0f), 1, 1);
    setRgba(cr, i % 2 ? rgb(255, 120, 130) : rgb(40, 0, 10), rng.range(0.15f, 0.35f));
    cairo_fill(cr);
  }
  // A woven pattern band: little gold lozenges.
  for (double x = 4; x < 64; x += 8)
  {
    cairo_move_to(cr, x, y - 0.5);
    cairo_line_to(cr, x + 2.5, y + 1.5);
    cairo_line_to(cr, x, y + 3.5);
    cairo_line_to(cr, x - 2.5, y + 1.5);
    cairo_close_path(cr);
    setRgba(cr, rgb(230, 180, 90), 0.55);
    cairo_fill(cr);
  }
  cairo_rectangle(cr, 0, y - 5, 64, 1.6);
  setRgba(cr, rgb(255, 150, 150), 0.55);
  cairo_fill(cr);
  // The gold braid.
  cairo_rectangle(cr, 0, y + 7, 64, 4);
  fillLinear(cr, 0, y + 7, 0, y + 11, {{0.0, kBrassHi}, {0.5, kBrass}, {1.0, kBrassLo}});
  for (double x = 0; x < 64; x += 4)
  {
    cairo_move_to(cr, x, y + 7.5);
    cairo_line_to(cr, x + 3, y + 10.5);
    stroke(cr, kBrassLo, 0.8, 0.8);
  }
  cairo_rectangle(cr, 0, y + 11, 64, 1);
  setRgba(cr, kInk, 0.8);
  cairo_fill(cr);
  return img.toTexture(r, 0.0f, topOffset);
}

// The top bar of a makeup mirror's gilt frame: a rounded gilt rail with a
// bead-and-reel moulding, below it the frame's face with two vanity bulbs
// set in it, and the inner lip. 64 x 44, the walking surface at the top;
// it tiles side by side (the glass under it is drawn by the level).
Texture bakeWardrobePlatform(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(64, 44);
  cairo_t* cr = img.cr();
  // The frame's face.
  cairo_rectangle(cr, 0, 9, 64, 18);
  fillLinear(cr, 0, 9, 0, 27, {{0.0, rgb(250, 230, 170)}, {0.4, rgb(226, 186, 104)}, {1.0, rgb(150, 104, 40)}});
  // The inner lip.
  cairo_rectangle(cr, 0, 27, 64, 4);
  fillLinear(cr, 0, 27, 0, 31, {{0.0, kBrassLo}, {1.0, rgb(70, 44, 16)}});
  cairo_rectangle(cr, 0, 31, 64, 1);
  setRgba(cr, kInk, 0.8);
  cairo_fill(cr);
  // The top rail.
  cairo_rectangle(cr, 0, 0, 64, 9);
  fillLinear(cr, 0, 0, 0, 9, {{0.0, rgb(255, 246, 200)}, {0.35, kBrassHi}, {0.7, kBrass}, {1.0, kBrassLo}});
  for (double x = 2; x < 64; x += 8)
  {
    cairo_save(cr);
    cairo_translate(cr, x + 2, 4.5);
    cairo_scale(cr, 2.2, 2.6);
    cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
    cairo_restore(cr);
    setRgba(cr, rgb(255, 250, 220), 0.5);
    cairo_fill(cr);
    cairo_arc(cr, x + 6, 4.5, 1.0, 0, 2 * kPi);
    setRgba(cr, kBrassLo, 0.8);
    cairo_fill(cr);
  }
  cairo_rectangle(cr, 0, 0.5, 64, 9);
  stroke(cr, kInk, 1.0, 0.6);
  // The bulbs.
  for (const double x : {16.0, 48.0})
    bulb(cr, x, 18, 5.2);
  return img.toTexture(r, 0.0f, 0.0f);
}

// A wooden library ladder: two rails with brass shoes, round rungs.
Texture bakeWardrobeLadder(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  for (const double y : {14.0, 46.0})
  {
    cairo_rectangle(cr, 14, y - 3, 36, 6);
    fillLinear(cr, 0, y - 3, 0, y + 3, {{0.0, kWoodHi}, {0.5, kWood}, {1.0, kWoodLo}}, 1.0, true);
    stroke(cr, kInk, 1.0, 0.9);
  }
  for (const double x : {9.0, 49.0})
  {
    cairo_rectangle(cr, x, -2, 7, 68);
    fillLinear(cr, x, 0, x + 7, 0, {{0.0, kWoodHi}, {0.5, kWood}, {1.0, kWoodLo}}, 1.0, true);
    stroke(cr, kInk, 1.2, 0.9);
    // A brass band once a block.
    cairo_rectangle(cr, x, 28, 7, 4);
    fillLinear(cr, 0, 28, 0, 32, {{0.0, kBrassHi}, {1.0, kBrassLo}});
  }
  return img.toTexture(r, 16.0f, 0.0f);
}

// Needles: a pincushion strip along the bottom (tomato red, tufted), and
// out of it long sewing needles (steel, an eye near the top) and shorter
// dressmaker's pins with round coloured heads. Turned over for the
// ceiling's needles.
Texture bakeWardrobeSpikes(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  static constexpr Color kHeads[4] = {rgb(255, 210, 60), rgb(80, 170, 255), rgb(255, 255, 255), rgb(120, 220, 120)};
  struct Pin
  {
    double x, top, lean;
    bool needle;
    int head;
  };
  static constexpr Pin kPins[7] = {
    {5, 22, -2, true, 0}, {13, 36, 1, false, 0}, {22, 18, 1, true, 0}, {31, 34, -1, false, 1},
    {40, 20, -1, true, 0}, {50, 32, 2, false, 3}, {58, 24, 1, true, 0}};
  for (const auto& p : kPins)
  {
    const double xb = p.x, yb = 56, xt = p.x + p.lean, yt = p.top;
    cairo_move_to(cr, xb - 1.6, yb);
    cairo_line_to(cr, xt, yt);
    cairo_line_to(cr, xb + 1.6, yb);
    cairo_close_path(cr);
    fillLinear(cr, xb - 2, 0, xb + 2, 0, {{0.0, rgb(250, 252, 255)}, {0.5, rgb(170, 176, 190)}, {1.0, rgb(90, 94, 110)}}, 1.0, true);
    stroke(cr, kInk, 0.9, 0.9);
    if (p.needle)
    {
      // A needle stands point up (its eye is down in the cushion): a glint
      // runs down from the point.
      cairo_move_to(cr, xt, yt + 1);
      cairo_line_to(cr, xt + (xb - xt) * 0.3, yt + (yb - yt) * 0.3);
      stroke(cr, rgb(255, 255, 255), 0.9, 0.9);
    }
    else
    {
      // A shorter needle still threaded: a loop of coloured thread through
      // its eye, trailing over the cushion.
      const double ex = xb + (xt - xb) * 0.18, ey = yb + (yt - yb) * 0.18;
      cairo_move_to(cr, ex, ey);
      cairo_curve_to(cr, ex + 8, ey - 6, ex + 10, ey + 2, ex + 6, yb + 2);
      cairo_move_to(cr, ex, ey);
      cairo_curve_to(cr, ex - 7, ey - 4, ex - 9, ey + 3, ex - 4, yb + 3);
      stroke(cr, kHeads[p.head], 1.4, 1.0);
      cairo_move_to(cr, xt, yt + 1);
      cairo_line_to(cr, xt + (xb - xt) * 0.3, yt + (yb - yt) * 0.3);
      stroke(cr, rgb(255, 255, 255), 0.9, 0.9);
    }
  }
  // The cushion.
  cairo_move_to(cr, 0, 64);
  cairo_line_to(cr, 0, 54);
  for (int k = 0; k < 4; ++k)
    cairo_curve_to(cr, k * 16 + 4, 49, k * 16 + 12, 49, k * 16 + 16, 54);
  cairo_line_to(cr, 64, 64);
  cairo_close_path(cr);
  fillLinear(cr, 0, 49, 0, 64, {{0.0, rgb(240, 80, 70)}, {0.5, rgb(190, 36, 36)}, {1.0, rgb(100, 14, 18)}}, 1.0, true);
  stroke(cr, kInk, 1.4, 0.9);
  for (int k = 1; k < 4; ++k)
  {
    cairo_move_to(cr, k * 16, 54);
    cairo_line_to(cr, k * 16, 64);
    stroke(cr, rgb(90, 10, 14), 1.4, 0.8);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// The room's height lost in warm shadow: near-black crimson overhead,
// warming lower down, the haze of bulb light in it.
Texture bakeWardrobeSky(const Renderer& r, const Theme& t)
{
  VectorImage img(kScreenW, kScreenH);
  cairo_t* cr = img.cr();
  cairo_rectangle(cr, 0, 0, kScreenW, kScreenH);
  fillLinear(cr, 0, 0, 0, kScreenH,
    {{0.0, darken(t.skyTop, 0.4f)}, {0.45, t.skyTop}, {0.8, t.skyMid}, {1.0, lerpColor(t.skyMid, t.skyBottom, 0.6f)}});
  Rng rng(37031u);
  for (int i = 0; i < 6; ++i)
    softDot(cr, rng.range(0, float(kScreenW)), rng.range(260, 620), rng.range(200, 380), rng.range(120, 200), rgb(255, 170, 110), 0.10);
  // Dust in the light.
  for (int i = 0; i < 90; ++i)
  {
    cairo_arc(cr, rng.range(0, float(kScreenW)), rng.range(0, float(kScreenH)), rng.range(0.5f, 1.2f), 0, 2 * kPi);
    setRgba(cr, rgb(255, 220, 180), rng.range(0.05f, 0.2f));
    cairo_fill(cr);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

namespace
{

constexpr int kFarH = kScreenH + 360;
constexpr int kNearH = kScreenH + 420;

// A swag of crimson velvet from x0 to x1, hanging from y: a deep curve of
// folds with gold fringe along its hem.
void swag(cairo_t* cr, double x0, double x1, double y, double drop)
{
  const double mid = (x0 + x1) * 0.5;
  cairo_move_to(cr, x0, y);
  cairo_line_to(cr, x1, y);
  cairo_curve_to(cr, x1 - 20, y + drop * 0.7, mid + (x1 - x0) * 0.25, y + drop, mid, y + drop);
  cairo_curve_to(cr, mid - (x1 - x0) * 0.25, y + drop, x0 + 20, y + drop * 0.7, x0, y);
  cairo_close_path(cr);
  fillLinear(cr, 0, y, 0, y + drop, {{0.0, kVelvetLo}, {0.4, kVelvet}, {0.8, kVelvetHi}, {1.0, kVelvet}}, 1.0, true);
  stroke(cr, kInk, 1.6, 0.8);
  // Folds.
  for (int k = 1; k < 4; ++k)
  {
    const double f = double(k) / 4.0;
    cairo_move_to(cr, x0 + (x1 - x0) * 0.08 * k, y + 4);
    cairo_curve_to(cr, x0 + (x1 - x0) * (0.1 + f * 0.2), y + drop * f * 0.9, x1 - (x1 - x0) * (0.1 + f * 0.2), y + drop * f * 0.9,
      x1 - (x1 - x0) * 0.08 * k, y + 4);
    stroke(cr, kVelvetLo, 3.0, 0.45);
  }
  // The fringe.
  for (double x = x0 + 6; x < x1 - 6; x += 5)
  {
    const double u = (x - x0) / (x1 - x0);
    const double hy = y + drop * (1.0 - std::pow(2.0 * u - 1.0, 2.0)) * 0.98 + 2;
    cairo_move_to(cr, x, hy);
    cairo_line_to(cr, x, hy + 12);
    stroke(cr, kBrass, 1.6, 0.9);
  }
}

// A tall drape hanging from y0 to y1 at x, w wide, caught by a gold tie-back
// at `tie` (0..1 down it), its hem pooling a little.
void drape(cairo_t* cr, double x, double w, double y0, double y1, double tie, bool left)
{
  const double ty = y0 + (y1 - y0) * tie;
  const double pinch = left ? x + w * 0.25 : x + w * 0.75;
  cairo_move_to(cr, x, y0);
  cairo_line_to(cr, x + w, y0);
  cairo_curve_to(cr, x + w, ty * 0.6 + y0 * 0.4, left ? pinch + 10 : x + w, ty, left ? pinch + 6 : x + w, ty);
  cairo_curve_to(cr, left ? pinch + 30 : x + w + 10, ty + 60, x + w + 14, y1 - 30, x + w + 16, y1);
  cairo_line_to(cr, x - 16, y1);
  cairo_curve_to(cr, x - 14, y1 - 30, left ? x - 10 : pinch - 30, ty + 60, left ? x : pinch - 6, ty);
  cairo_curve_to(cr, left ? x : pinch - 10, ty, x, ty * 0.6 + y0 * 0.4, x, y0);
  cairo_close_path(cr);
  fillLinear(cr, x - 16, 0, x + w + 16, 0,
    {{0.0, kVelvetLo}, {0.2, kVelvet}, {0.35, kVelvetHi}, {0.5, kVelvet}, {0.7, kVelvetLo}, {0.85, kVelvet}, {1.0, kVelvetLo}}, 1.0, true);
  stroke(cr, kInk, 1.6, 0.7);
  for (int k = 1; k < 5; ++k)
  {
    const double fx = x + w * k / 5.0;
    cairo_move_to(cr, fx, y0 + 4);
    cairo_curve_to(cr, fx, ty * 0.7, (fx + pinch) * 0.5, ty - 10, pinch + (fx - pinch) * 0.2, ty);
    cairo_curve_to(cr, pinch + (fx - pinch) * 0.6, ty + 80, fx + (fx - pinch) * 0.3, y1 - 40, fx + (fx - pinch) * 0.35, y1);
    stroke(cr, k % 2 ? kVelvetLo : kVelvetHi, 2.2, 0.35);
  }
  // The tie-back: a gold cord with a tassel.
  cairo_save(cr);
  cairo_translate(cr, pinch, ty);
  cairo_scale(cr, 1.0, 0.35);
  cairo_arc(cr, 0, 0, 22, 0, 2 * kPi);
  cairo_restore(cr);
  stroke(cr, kBrass, 4.0, 1.0);
  cairo_move_to(cr, pinch + (left ? 18 : -18), ty);
  cairo_line_to(cr, pinch + (left ? 22 : -22), ty + 26);
  stroke(cr, kBrass, 2.4, 1.0);
  cairo_move_to(cr, pinch + (left ? 16 : -28), ty + 24);
  cairo_line_to(cr, pinch + (left ? 28 : -16), ty + 24);
  cairo_line_to(cr, pinch + (left ? 30 : -14), ty + 44);
  cairo_line_to(cr, pinch + (left ? 14 : -30), ty + 44);
  cairo_close_path(cr);
  fillLinear(cr, 0, ty + 24, 0, ty + 44, {{0.0, kBrassHi}, {1.0, kBrassLo}});
}

// A brass wall sconce, lit, at (x, y).
void sconce(cairo_t* cr, double x, double y)
{
  softDot(cr, x, y - 10, 90, 90, rgb(255, 190, 120), 0.35);
  cairo_move_to(cr, x, y + 14);
  cairo_curve_to(cr, x - 4, y + 4, x - 14, y + 6, x - 14, y - 4);
  cairo_move_to(cr, x, y + 14);
  cairo_curve_to(cr, x + 4, y + 4, x + 14, y + 6, x + 14, y - 4);
  stroke(cr, kBrassLo, 3.0, 1.0);
  cairo_rectangle(cr, x - 4, y + 10, 8, 10);
  setRgba(cr, kBrass, 1.0);
  cairo_fill(cr);
  for (const double s : {-14.0, 14.0})
  {
    cairo_move_to(cr, x + s - 7, y - 4);
    cairo_line_to(cr, x + s + 7, y - 4);
    cairo_line_to(cr, x + s + 4, y - 18);
    cairo_line_to(cr, x + s - 4, y - 18);
    cairo_close_path(cr);
    setRgba(cr, rgb(255, 236, 200), 0.95);
    cairo_fill(cr);
    softDot(cr, x + s, y - 12, 14, 14, rgb(255, 230, 170), 0.6);
  }
}

} // namespace

// The far wall (layerW x kFarH, it tiles side by side): cream walls between
// fluted pilasters, a dado rail and dark wainscot low down, swags of
// crimson velvet across the top with gold fringe, tall drapes tied back
// either side of every other bay, sconces between. Hazy with distance.
Texture bakeWardrobeFar(const Renderer& r, const Theme& t, int layerW)
{
  (void)t;
  const int h = kFarH;
  VectorImage img(layerW, h);
  cairo_t* cr = img.cr();
  const double W = layerW;
  // The wall.
  cairo_rectangle(cr, 0, 0, W, h);
  fillLinear(cr, 0, 0, 0, h, {{0.0, rgb(60, 30, 30)}, {0.2, rgb(150, 116, 96)}, {0.5, rgb(196, 164, 132)}, {1.0, rgb(150, 112, 88)}});
  // A faint damask: rows of little lozenges.
  for (double y = 170; y < h - 220; y += 46)
    for (double x = std::fmod(y, 92.0) * 0.5; x < W; x += 46)
    {
      cairo_move_to(cr, x, y - 9);
      cairo_curve_to(cr, x + 8, y - 4, x + 8, y + 4, x, y + 9);
      cairo_curve_to(cr, x - 8, y + 4, x - 8, y - 4, x, y - 9);
      setRgba(cr, rgb(255, 236, 200), 0.10);
      cairo_fill(cr);
    }
  // The wainscot and its dado rail.
  const double dado = h - 230;
  cairo_rectangle(cr, 0, dado, W, h - dado);
  fillLinear(cr, 0, dado, 0, h, {{0.0, rgb(96, 50, 36)}, {1.0, rgb(60, 28, 22)}});
  for (double x = 20; x < W; x += 120)
  {
    cairo_rectangle(cr, x, dado + 30, 96, 150);
    stroke(cr, rgb(40, 18, 14), 2.0, 0.6);
    cairo_rectangle(cr, x + 2, dado + 32, 96, 150);
    stroke(cr, rgb(150, 90, 60), 1.0, 0.35);
  }
  cairo_rectangle(cr, 0, dado - 8, W, 12);
  fillLinear(cr, 0, dado - 8, 0, dado + 4, {{0.0, rgb(220, 180, 110)}, {1.0, rgb(110, 70, 30)}});
  // Pilasters every bay.
  const double bay = W / 8.0;
  for (int b = 0; b < 8; ++b)
  {
    const double x = b * bay;
    cairo_rectangle(cr, x - 22, 120, 44, dado - 120);
    fillLinear(cr, x - 22, 0, x + 22, 0, {{0.0, rgb(170, 140, 112)}, {0.4, rgb(232, 214, 186)}, {1.0, rgb(150, 120, 96)}});
    for (int f = -1; f <= 1; ++f)
    {
      cairo_move_to(cr, x + f * 10, 150);
      cairo_line_to(cr, x + f * 10, dado - 30);
      stroke(cr, rgb(130, 100, 80), 3.0, 0.5);
    }
    cairo_rectangle(cr, x - 30, 120, 60, 18);
    fillLinear(cr, 0, 120, 0, 138, {{0.0, rgb(240, 214, 150)}, {1.0, rgb(150, 110, 60)}});
    cairo_rectangle(cr, x - 28, dado - 24, 56, 16);
    fillLinear(cr, 0, dado - 24, 0, dado - 8, {{0.0, rgb(220, 196, 160)}, {1.0, rgb(130, 100, 80)}});
  }
  // Tall drapes either side of every other bay, sconces in the others.
  for (int b = 0; b < 8; ++b)
  {
    const double x0 = b * bay + 24, x1 = (b + 1) * bay - 24;
    if (b % 2 == 0)
    {
      // A deep arched doorway between the drapes, dark beyond.
      const double mid = (x0 + x1) * 0.5;
      cairo_move_to(cr, mid - 70, dado + 4);
      cairo_line_to(cr, mid - 70, 300);
      cairo_arc(cr, mid, 300, 70, kPi, 2 * kPi);
      cairo_line_to(cr, mid + 70, dado + 4);
      cairo_close_path(cr);
      fillLinear(cr, 0, 230, 0, dado, {{0.0, rgb(30, 10, 14)}, {1.0, rgb(70, 30, 26)}}, 1.0, true);
      stroke(cr, rgb(214, 170, 90), 4.0, 0.8);
      softDot(cr, mid, dado - 40, 60, 30, rgb(255, 180, 110), 0.25);
      drape(cr, x0, 70, 150, dado + 10, 0.55, true);
      drape(cr, x1 - 70, 70, 150, dado + 10, 0.55, false);
    }
    else
    {
      sconce(cr, (x0 + x1) * 0.5, 330);
      // A framed old costume sketch.
      const double fx = (x0 + x1) * 0.5 - 50, fy = 400;
      cairo_rectangle(cr, fx, fy, 100, 130);
      fillLinear(cr, 0, fy, 0, fy + 130, {{0.0, rgb(240, 228, 200)}, {1.0, rgb(200, 180, 150)}}, 1.0, true);
      stroke(cr, rgb(170, 130, 60), 6.0, 1.0);
      cairo_move_to(cr, fx + 50, fy + 24);
      cairo_curve_to(cr, fx + 30, fy + 50, fx + 36, fy + 90, fx + 22, fy + 112);
      cairo_line_to(cr, fx + 78, fy + 112);
      cairo_curve_to(cr, fx + 64, fy + 90, fx + 70, fy + 50, fx + 50, fy + 24);
      setRgba(cr, b % 4 == 1 ? rgb(170, 60, 80) : rgb(60, 90, 150), 0.6);
      cairo_fill(cr);
      cairo_arc(cr, fx + 50, fy + 20, 8, 0, 2 * kPi);
      setRgba(cr, rgb(120, 90, 70), 0.6);
      cairo_fill(cr);
    }
  }
  // The pelmet: a velvet band along the top with a run of swags under it.
  cairo_rectangle(cr, 0, 0, W, 120);
  fillLinear(cr, 0, 0, 0, 120, {{0.0, rgb(20, 4, 8)}, {0.7, kVelvetLo}, {1.0, kVelvet}});
  cairo_rectangle(cr, 0, 112, W, 8);
  fillLinear(cr, 0, 112, 0, 120, {{0.0, kBrassHi}, {1.0, kBrassLo}});
  for (double x = 0; x < W; x += bay * 0.5)
    swag(cr, x, x + bay * 0.5, 118, 70);
  // Haze: the far wall sits back in the warm air.
  cairo_rectangle(cr, 0, 0, W, h);
  setRgba(cr, rgb(120, 60, 50), 0.22);
  cairo_fill(cr);
  return img.toTexture(r, 0.0f, 0.0f);
}

namespace
{

// A garment on a hanger hanging from a rail at (x, y): a coat, a dress, a
// cape or a doublet, in colour c.
void garment(cairo_t* cr, double x, double y, int kind, Color c, double len)
{
  // The hanger.
  cairo_move_to(cr, x, y - 2);
  cairo_curve_to(cr, x + 5, y - 2, x + 4, y - 9, x, y - 8);
  stroke(cr, rgb(40, 30, 24), 1.4, 1.0);
  cairo_move_to(cr, x - 14, y + 8);
  cairo_line_to(cr, x, y);
  cairo_line_to(cr, x + 14, y + 8);
  stroke(cr, rgb(60, 40, 30), 2.0, 1.0);
  const Color lo = darken(c, 0.45f), hi = lighten(c, 0.15f);
  switch (kind % 4)
  {
    case 0: // a long coat
      cairo_move_to(cr, x - 14, y + 7);
      cairo_line_to(cr, x + 14, y + 7);
      cairo_line_to(cr, x + 18, y + len);
      cairo_line_to(cr, x - 18, y + len);
      cairo_close_path(cr);
      break;
    case 1: // a gown, flaring
      cairo_move_to(cr, x - 9, y + 7);
      cairo_line_to(cr, x + 9, y + 7);
      cairo_curve_to(cr, x + 8, y + len * 0.4, x + 24, y + len * 0.8, x + 26, y + len);
      cairo_line_to(cr, x - 26, y + len);
      cairo_curve_to(cr, x - 24, y + len * 0.8, x - 8, y + len * 0.4, x - 9, y + 7);
      cairo_close_path(cr);
      break;
    case 2: // a cape
      cairo_move_to(cr, x - 6, y + 6);
      cairo_line_to(cr, x + 6, y + 6);
      cairo_line_to(cr, x + 22, y + len * 0.75);
      cairo_line_to(cr, x - 22, y + len * 0.75);
      cairo_close_path(cr);
      break;
    default: // a doublet with puffed sleeves
      cairo_move_to(cr, x - 14, y + 7);
      cairo_line_to(cr, x + 14, y + 7);
      cairo_line_to(cr, x + 12, y + len * 0.55);
      cairo_line_to(cr, x - 12, y + len * 0.55);
      cairo_close_path(cr);
      cairo_new_sub_path(cr);
      cairo_arc(cr, x - 16, y + 14, 7, 0, 2 * kPi);
      cairo_new_sub_path(cr);
      cairo_arc(cr, x + 16, y + 14, 7, 0, 2 * kPi);
      break;
  }
  fillLinear(cr, x - 24, 0, x + 24, 0, {{0.0, lo}, {0.35, hi}, {0.6, c}, {1.0, lo}}, 1.0, true);
  stroke(cr, kInk, 1.2, 0.7);
  // A seam or buttons.
  cairo_move_to(cr, x, y + 9);
  cairo_line_to(cr, x, y + len * 0.5);
  stroke(cr, lo, 1.2, 0.6);
}

} // namespace

// The near layer (layerW x kNearH, tiles side by side; see-through above):
// rows of costume racks, brass rails on stands crammed with coats, gowns,
// capes and doublets, hat boxes stacked on top, dimmed by distance.
Texture bakeWardrobeNear(const Renderer& r, const Theme& t, int layerW)
{
  (void)t;
  VectorImage img(layerW, kNearH);
  cairo_t* cr = img.cr();
  static constexpr Color kCloth[10] = {rgb(170, 30, 50), rgb(40, 70, 140), rgb(220, 170, 60), rgb(70, 120, 70), rgb(120, 60, 140),
    rgb(230, 220, 200), rgb(40, 40, 50), rgb(200, 90, 40), rgb(90, 160, 180), rgb(240, 140, 170)};
  Rng rng(37061u);
  const double W = layerW;
  const double rail = 520, floor = 760;
  double x = rng.range(10, 60);
  while (x < W - 40)
  {
    const double len = rng.range(300, 520);
    const double x1 = std::min(W - 20, x + len);
    // The stands.
    for (const double sx : {x, x1})
    {
      cairo_move_to(cr, sx, rail - 10);
      cairo_line_to(cr, sx, floor);
      stroke(cr, rgb(120, 86, 40), 5.0, 1.0);
      cairo_move_to(cr, sx - 24, floor);
      cairo_line_to(cr, sx + 24, floor);
      stroke(cr, rgb(90, 60, 30), 5.0, 1.0);
    }
    // Garments, crammed.
    for (double gx = x + 18; gx < x1 - 14; gx += rng.range(20, 30))
      garment(cr, gx, rail, int(rng.next() % 4u), kCloth[rng.next() % 10u], rng.range(150, 220));
    // The rail.
    cairo_move_to(cr, x - 6, rail);
    cairo_line_to(cr, x1 + 6, rail);
    stroke(cr, rgb(200, 160, 80), 5.0, 1.0);
    // Hat boxes on top: striped drums with lids.
    for (double hx = x + rng.range(10, 60); hx < x1 - 40; hx += rng.range(70, 140))
    {
      double y = rail - 14;
      const int stack = 1 + int(rng.next() % 3u);
      for (int s = 0; s < stack; ++s)
      {
        const double bw = rng.range(44, 62), bh = rng.range(30, 40);
        const Color c = kCloth[rng.next() % 10u];
        cairo_rectangle(cr, hx, y - bh, bw, bh);
        fillLinear(cr, hx, 0, hx + bw, 0, {{0.0, darken(c, 0.4f)}, {0.4, lighten(c, 0.1f)}, {1.0, darken(c, 0.5f)}}, 1.0, true);
        stroke(cr, kInk, 1.2, 0.7);
        for (double sx = hx + 6; sx < hx + bw - 2; sx += 10)
        {
          cairo_rectangle(cr, sx, y - bh + 6, 4, bh - 6);
          setRgba(cr, rgb(255, 245, 225), 0.25);
          cairo_fill(cr);
        }
        cairo_rectangle(cr, hx - 3, y - bh - 4, bw + 6, 8);
        fillLinear(cr, 0, y - bh - 4, 0, y - bh + 4, {{0.0, lighten(c, 0.25f)}, {1.0, darken(c, 0.4f)}}, 1.0, true);
        stroke(cr, kInk, 1.0, 0.7);
        y -= bh + 4;
        hx += rng.range(-6, 6);
      }
    }
    x = x1 + rng.range(80, 260);
  }
  // Dimmed with distance and the warm gloom.
  cairo_set_operator(cr, CAIRO_OPERATOR_ATOP);
  cairo_rectangle(cr, 0, 0, W, kNearH);
  setRgba(cr, rgb(70, 26, 26), 0.5);
  cairo_fill(cr);
  cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
  return img.toTexture(r, 0.0f, 0.0f);
}

// The stage door (96 x 144; (0, 0) the top-left of the 64 x 128 exit
// area): a dark green door ajar in a cream frame, stage light pouring
// round it, a gold star on it, a caged lamp over it and STAGE above.
Texture bakeWardrobeExitBase(const Renderer& r, const Theme& t)
{
  (void)t;
  VectorImage img(96, 144);
  cairo_t* cr = img.cr();
  cairo_translate(cr, 16, 8);
  // The frame.
  cairo_rectangle(cr, -6, 6, 76, 122);
  fillLinear(cr, -6, 0, 70, 0, {{0.0, rgb(180, 160, 130)}, {0.3, kCream}, {1.0, rgb(150, 126, 100)}}, 1.0, true);
  stroke(cr, kInk, 2.0);
  // The light beyond, through the gap.
  cairo_rectangle(cr, 2, 16, 60, 112);
  fillLinear(cr, 0, 16, 0, 128, {{0.0, rgb(255, 250, 220)}, {1.0, rgb(255, 210, 130)}});
  softDot(cr, 32, 70, 50, 70, rgb(255, 255, 230), 0.6);
  // The door, swung in a little (a narrowed panel).
  cairo_move_to(cr, 2, 16);
  cairo_line_to(cr, 46, 22);
  cairo_line_to(cr, 46, 124);
  cairo_line_to(cr, 2, 128);
  cairo_close_path(cr);
  fillLinear(cr, 2, 0, 46, 0, {{0.0, rgb(30, 70, 50)}, {0.5, rgb(46, 100, 70)}, {1.0, rgb(24, 56, 40)}}, 1.0, true);
  stroke(cr, kInk, 1.8);
  // The star.
  cairo_save(cr);
  cairo_translate(cr, 24, 56);
  cairo_move_to(cr, 0, -12);
  for (int k = 1; k < 10; ++k)
  {
    const double a = -kPi / 2 + k * kPi / 5, rr = k % 2 ? 5.0 : 12.0;
    cairo_line_to(cr, std::cos(a) * rr, std::sin(a) * rr);
  }
  cairo_close_path(cr);
  fillLinear(cr, 0, -12, 0, 12, {{0.0, kBrassHi}, {1.0, kBrassLo}}, 1.0, true);
  stroke(cr, kInk, 1.0);
  cairo_restore(cr);
  cairo_arc(cr, 40, 80, 2.6, 0, 2 * kPi);
  setRgba(cr, kBrass, 1.0);
  cairo_fill(cr);
  // The sign.
  cairo_rectangle(cr, 4, -6, 56, 14);
  setRgba(cr, rgb(30, 14, 16), 1.0);
  cairo_fill_preserve(cr);
  stroke(cr, kBrass, 1.6);
  selectGameFont(cr);
  cairo_set_font_size(cr, 10.5);
  cairo_text_extents_t e;
  cairo_text_extents(cr, "STAGE", &e);
  cairo_move_to(cr, 32 - e.width * 0.5 - e.x_bearing, 5);
  setRgba(cr, rgb(255, 226, 150), 1.0);
  cairo_show_text(cr, "STAGE");
  // The lamp.
  bulb(cr, 64, 4, 4.0);
  return img.toTexture(r, 16.0f, 8.0f);
}

} // namespace gr
