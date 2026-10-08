// Styled enemies: the campaign's residents each get their own vector
// drawing, keyed by the enemy's key. Sprites are baked the first time an
// enemy is drawn and cached in Art (per theme).
//
// Every routine draws in screen pixels into a texture with a 32 px margin;
// the enemy's cell box is (32, 32) .. (32 + w * 32, 32 + h * 32), and the
// sprite is anchored at the bottom centre of that box, like the classic
// enemies.

#include "assets/enemy_art.hpp"

#include "render/vector.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>

namespace gr
{

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr Color kInk = rgb(26, 20, 38);
constexpr double kLine = 2.4;
constexpr double kM = 32.0; // margin

Color lighten(Color c, float t) { return lerpColor(c, rgb(255, 255, 255), t); }
Color darken(Color c, float t) { return lerpColor(c, rgb(0, 0, 0), t); }

void circle(cairo_t* cr, double x, double y, double r) { cairo_arc(cr, x, y, r, 0, 2 * kPi); }

struct Ctx
{
  cairo_t* cr;
  const Theme& t;
  double w, h; // box size in px
  int variant;
  int frame;
};

// Glass Crawler (level 2): a glassy six-legged bug. Variant 0 clings to a
// wall (head up, legs to the wall side), 1 lies flat.
void glassCrawler(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const Color body = rgba(150, 225, 255, 190), edge = rgb(40, 90, 140);
  cairo_save(cr);
  if (c.variant == 0)
  {
    // Draw lying flat in a w=h-swapped space, then rotate to stand on the wall.
    cairo_translate(cr, kM + c.w, kM);
    cairo_rotate(cr, kPi / 2);
  }
  else
  {
    cairo_translate(cr, kM, kM);
  }
  const double L = c.variant == 0 ? c.h : c.w, H = c.variant == 0 ? c.w : c.h;
  const double kick = c.frame ? 3.0 : -3.0;
  for (int i = 0; i < 3; ++i)
  {
    const double lx = L * (0.3 + 0.2 * i);
    strokeLimb(cr, {{lx, H * 0.55}, {lx - 8 + (i % 2 ? kick : -kick), H + 2}}, 3.0, edge, kInk, 1.2);
    strokeLimb(cr, {{lx, H * 0.45}, {lx - 8 - (i % 2 ? kick : -kick), -2}}, 3.0, edge, kInk, 1.2);
  }
  for (int s = 0; s < 3; ++s)
  {
    cairo_save(cr);
    cairo_translate(cr, L * (0.25 + 0.25 * s), H * 0.5);
    cairo_scale(cr, 1.0, 0.7);
    circle(cr, 0, 0, H * (s == 2 ? 0.42 : 0.38));
    cairo_restore(cr);
    fillOutline(cr, body, edge, kLine);
  }
  // Head and mandibles at the front (toward +x).
  strokeLimb(cr, {{L * 0.86, H * 0.38}, {L + 4, H * 0.22}}, 3.0, rgb(230, 250, 255), kInk, 1.2);
  strokeLimb(cr, {{L * 0.86, H * 0.62}, {L + 4, H * 0.78}}, 3.0, rgb(230, 250, 255), kInk, 1.2);
  circle(cr, L * 0.82, H * 0.4, 3.2);
  fillOutline(cr, c.t.enemyEye, kInk, 1.0);
  circle(cr, L * 0.82, H * 0.6, 3.2);
  fillOutline(cr, c.t.enemyEye, kInk, 1.0);
  strokeLimb(cr, {{L * 0.2, H * 0.32}, {L * 0.6, H * 0.3}}, 2.0, rgba(255, 255, 255, 200), kInk, 0.0);
  cairo_restore(cr);
}

// Squeegee Drone (level 2): a window-cleaning drone with twin rotors and a
// long rubber blade.
void squeegeeDrone(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  // Rotors.
  for (double rx : {x0 + w * 0.2, x0 + w * 0.8})
  {
    strokeLimb(cr, {{rx, y0 + h * 0.28}, {rx, y0 + h * 0.12}}, 4, c.t.enemyDark, kInk, 1.2);
    cairo_save(cr);
    cairo_translate(cr, rx, y0 + h * 0.1);
    cairo_scale(cr, 1.0, 0.22);
    circle(cr, 0, 0, c.frame ? w * 0.2 : w * 0.15);
    cairo_restore(cr);
    setColor(cr, withAlpha(c.t.enemyLight, 110));
    cairo_fill(cr);
  }
  // Body.
  roundedRect(cr, x0 + w * 0.1, y0 + h * 0.25, w * 0.8, h * 0.38, 12);
  fillGradientOutline(cr, y0 + h * 0.25, y0 + h * 0.63, lighten(c.t.enemyLight, 0.25f), c.t.enemyBody, kInk, kLine);
  roundedRect(cr, x0 + w * 0.3, y0 + h * 0.33, w * 0.4, h * 0.14, 6);
  setColor(cr, rgb(16, 14, 24));
  cairo_fill(cr);
  radialGlow(cr, x0 + w * 0.5, y0 + h * 0.4, 16, c.t.enemyEye, 0.6);
  circle(cr, x0 + w * 0.58, y0 + h * 0.4, 4);
  setColor(cr, lighten(c.t.enemyEye, 0.3f));
  cairo_fill(cr);
  // Arm and blade.
  strokeLimb(cr, {{x0 + w * 0.5, y0 + h * 0.62}, {x0 + w * 0.5, y0 + h * 0.8}}, 6, rgb(180, 190, 205), kInk, 1.4);
  roundedRect(cr, x0 + 2, y0 + h * 0.8, w - 4, h * 0.1, 3);
  fillOutline(cr, rgb(200, 210, 225), kInk, kLine);
  roundedRect(cr, x0 + 4, y0 + h * 0.9, w - 8, h * 0.08, 2);
  fillOutline(cr, rgb(30, 30, 36), kInk, 1.4);
  // Water drips.
  for (int i = 0; i < 4; ++i)
  {
    circle(cr, x0 + w * (0.15 + 0.23 * i), y0 + h + 4 + ((i + c.frame) % 2) * 4, 2.2);
    setColor(cr, rgba(150, 220, 255, 200));
    cairo_fill(cr);
  }
}

// Penthouse Sniper (level 2): a chrome robot in a black suit with a long
// rifle and a red eye.
void penthouseSniper(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  const Color suit = rgb(30, 30, 44), chrome = rgb(200, 210, 228);
  // Legs.
  strokeLimb(cr, {{x0 + w * 0.38, y0 + h * 0.62}, {x0 + w * 0.34, y0 + h - 4}}, 9, suit, kInk, kLine);
  strokeLimb(cr, {{x0 + w * 0.62, y0 + h * 0.62}, {x0 + w * 0.66, y0 + h - 4}}, 9, suit, kInk, kLine);
  roundedRect(cr, x0 + w * 0.22, y0 + h - 8, w * 0.22, 8, 3);
  fillOutline(cr, rgb(20, 20, 26), kInk, 1.4);
  roundedRect(cr, x0 + w * 0.56, y0 + h - 8, w * 0.22, 8, 3);
  fillOutline(cr, rgb(20, 20, 26), kInk, 1.4);
  // Torso: suit jacket with a white collar.
  roundedRect(cr, x0 + w * 0.2, y0 + h * 0.3, w * 0.6, h * 0.36, 10);
  fillGradientOutline(cr, y0 + h * 0.3, y0 + h * 0.66, rgb(60, 60, 80), suit, kInk, kLine);
  cairo_move_to(cr, x0 + w * 0.42, y0 + h * 0.3);
  cairo_line_to(cr, x0 + w * 0.5, y0 + h * 0.42);
  cairo_line_to(cr, x0 + w * 0.58, y0 + h * 0.3);
  cairo_close_path(cr);
  fillOutline(cr, rgb(240, 240, 248), kInk, 1.2);
  // Head: chrome dome with a visor.
  circle(cr, x0 + w * 0.5, y0 + h * 0.18, w * 0.22);
  fillGradientOutline(cr, y0, y0 + h * 0.36, lighten(chrome, 0.3f), darken(chrome, 0.3f), kInk, kLine);
  roundedRect(cr, x0 + w * 0.42, y0 + h * 0.14, w * 0.32, h * 0.07, 3);
  setColor(cr, rgb(16, 14, 24));
  cairo_fill(cr);
  radialGlow(cr, x0 + w * 0.66, y0 + h * 0.175, 10, rgb(255, 40, 40), 0.8);
  circle(cr, x0 + w * 0.66, y0 + h * 0.175, 2.6);
  setColor(cr, rgb(255, 120, 120));
  cairo_fill(cr);
  // Rifle, held level and pointing forward.
  roundedRect(cr, x0 + w * 0.3, y0 + h * 0.4, w * 0.95, 7, 3);
  fillOutline(cr, rgb(50, 52, 64), kInk, 1.4);
  roundedRect(cr, x0 + w * 0.75, y0 + h * 0.37, w * 0.24, 5, 2);
  fillOutline(cr, chrome, kInk, 1.0);
  strokeLimb(cr, {{x0 + w * 0.3, y0 + h * 0.36}, {x0 + w * 0.5, y0 + h * 0.44}}, 7, suit, kInk, 1.4);
}

// Bouncer (level 3): a wall of a man in a black SECURITY tee and shades,
// arms crossed. Variant 1 is the tell: feet planted, arms up in a flex.
// `cardboard` draws the bonus level's cut-out copy of him.
void bouncerFigure(const Ctx& c, bool cardboard)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  const Color skin = cardboard ? rgb(196, 150, 96) : rgb(176, 118, 84);
  const Color shirt = cardboard ? rgb(150, 110, 66) : rgb(26, 26, 32);
  const Color pants = cardboard ? rgb(170, 128, 80) : rgb(40, 40, 58);
  const Color ink = cardboard ? rgb(90, 60, 30) : kInk;
  const bool flex = c.variant == 1;
  if (cardboard)
  {
    // The easel stand behind the cut-out.
    strokeLimb(cr, {{x0 + w * 0.5, y0 + h * 0.5}, {x0 + w * 0.85, y0 + h}}, 5, rgb(120, 86, 50), ink, 1.2);
  }
  // Legs, wide apart.
  const double stance = flex ? 0.08 : 0.0;
  strokeLimb(cr, {{x0 + w * 0.36, y0 + h * 0.62}, {x0 + w * (0.3 - stance), y0 + h - 6}}, 16, pants, ink, kLine);
  strokeLimb(cr, {{x0 + w * 0.64, y0 + h * 0.62}, {x0 + w * (0.7 + stance), y0 + h - 6}}, 16, pants, ink, kLine);
  for (double fx : {0.3 - stance, 0.7 + stance})
  {
    roundedRect(cr, x0 + w * fx - 14, y0 + h - 10, 28, 10, 4);
    fillOutline(cr, cardboard ? rgb(110, 80, 46) : rgb(16, 16, 20), ink, 1.4);
  }
  // Torso: a big wedge.
  cairo_move_to(cr, x0 + w * 0.08, y0 + h * 0.24);
  cairo_line_to(cr, x0 + w * 0.92, y0 + h * 0.24);
  cairo_line_to(cr, x0 + w * 0.76, y0 + h * 0.64);
  cairo_line_to(cr, x0 + w * 0.24, y0 + h * 0.64);
  cairo_close_path(cr);
  fillGradientOutline(cr, y0 + h * 0.24, y0 + h * 0.64, lighten(shirt, 0.15f), shirt, ink, kLine);
  // SECURITY, as a stripe of block letters.
  for (int i = 0; i < 8; ++i)
  {
    cairo_rectangle(cr, x0 + w * (0.24 + 0.066 * i), y0 + h * 0.4, w * 0.045, h * 0.035);
    setColor(cr, cardboard ? rgb(90, 60, 30) : rgb(235, 235, 240));
    cairo_fill(cr);
  }
  // Arms.
  if (flex)
  {
    for (int sgn : {-1, 1})
    {
      const double sx = x0 + w * (0.5 + sgn * 0.38), ex = x0 + w * (0.5 + sgn * 0.5);
      strokeLimb(cr, {{sx, y0 + h * 0.28}, {ex, y0 + h * 0.2}, {x0 + w * (0.5 + sgn * 0.36), y0 + h * 0.05}}, 15, skin, ink, kLine);
      circle(cr, x0 + w * (0.5 + sgn * 0.36), y0 + h * 0.05, 10);
      fillOutline(cr, skin, ink, kLine);
      circle(cr, ex, y0 + h * 0.19, 12); // the biceps
      fillOutline(cr, lighten(skin, 0.1f), ink, kLine);
    }
  }
  else
  {
    roundedRect(cr, x0 + w * 0.16, y0 + h * 0.33, w * 0.68, h * 0.09, 10);
    fillOutline(cr, skin, ink, kLine);
    strokeLimb(cr, {{x0 + w * 0.3, y0 + h * 0.375}, {x0 + w * 0.7, y0 + h * 0.375}}, 2, darken(skin, 0.25f), ink, 0.0);
  }
  // Head: small on a thick neck, shaved, shades.
  roundedRect(cr, x0 + w * 0.4, y0 + h * 0.17, w * 0.2, h * 0.08, 4);
  fillOutline(cr, skin, ink, kLine);
  circle(cr, x0 + w * 0.5, y0 + h * 0.12, w * 0.15);
  fillGradientOutline(cr, y0, y0 + h * 0.24, lighten(skin, 0.15f), skin, ink, kLine);
  roundedRect(cr, x0 + w * 0.4, y0 + h * 0.095, w * 0.26, h * 0.035, 3);
  setColor(cr, cardboard ? rgb(70, 46, 24) : rgb(10, 10, 14));
  cairo_fill(cr);
  if (!cardboard)
  {
    cairo_rectangle(cr, x0 + w * 0.46, y0 + h * 0.1, w * 0.06, h * 0.01);
    setColor(cr, rgba(150, 220, 255, 200));
    cairo_fill(cr);
    if (flex)
      radialGlow(cr, x0 + w * 0.5, y0 + h * 0.15, w * 0.45, rgb(255, 60, 60), 0.35);
  }
}

void bouncer(const Ctx& c) { bouncerFigure(c, false); }
void cardboardBouncer(const Ctx& c) { bouncerFigure(c, true); }

// Disco Drone (level 3): a mirror ball on a chain with a tiny rotor. The
// facets shimmer; variant 1 is the tell, every facet white hot.
void discoDrone(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double cx = kM + c.w * 0.5, cy = kM + c.h * 0.58, rad = c.w * 0.42;
  strokeLimb(cr, {{cx, kM - 24}, {cx, cy - rad}}, 3, rgb(160, 160, 180), kInk, 1.0);
  circle(cr, cx, cy, rad);
  fillGradientOutline(cr, cy - rad, cy + rad, rgb(220, 220, 240), rgb(90, 90, 120), kInk, kLine);
  cairo_save(cr);
  circle(cr, cx, cy, rad - 1);
  cairo_clip(cr);
  const int rows = 6, cols = 8;
  for (int j = 0; j < rows; ++j)
    for (int i = 0; i < cols; ++i)
    {
      const double fy = cy - rad + (j + 0.5) * 2 * rad / rows;
      const double span = std::sqrt(std::max(0.0, rad * rad - (fy - cy) * (fy - cy)));
      const double fx = cx - span + (i + 0.5 + 0.5 * (c.frame % 2)) * 2 * span / cols;
      const unsigned hsh = unsigned(i * 7 + j * 13 + c.frame * 5) % 5u;
      Color col = hsh == 0 ? rgb(255, 255, 255) : (hsh == 1 ? rgb(255, 120, 220) : (hsh == 2 ? rgb(120, 220, 255) : rgb(150, 150, 175)));
      if (c.variant == 1)
        col = rgb(255, 255, 255);
      cairo_rectangle(cr, fx - span / cols + 1, fy - rad / rows + 1, 2 * span / cols - 2, 2 * rad / rows - 2);
      setColor(cr, col);
      cairo_fill(cr);
    }
  cairo_restore(cr);
  if (c.variant == 1)
    radialGlow(cr, cx, cy, rad * 1.8, rgb(255, 255, 255), 0.6);
  // A little rotor cap.
  roundedRect(cr, cx - 10, cy - rad - 8, 20, 10, 3);
  fillOutline(cr, c.t.enemyDark, kInk, 1.4);
}

// Glow Raver (level 3): a dancer in neon with a glowstick in each hand.
// Variant 1 is the tell: one stick held high, about to be thrown.
void glowRaver(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  const Color top = rgb(255, 60, 200), legs = rgb(40, 30, 80), skin = rgb(230, 180, 140), glow = rgb(120, 255, 90);
  const double kick = c.frame ? 4.0 : -4.0;
  strokeLimb(cr, {{x0 + w * 0.42, y0 + h * 0.6}, {x0 + w * 0.34 + kick, y0 + h - 4}}, 9, legs, kInk, kLine);
  strokeLimb(cr, {{x0 + w * 0.58, y0 + h * 0.6}, {x0 + w * 0.66 - kick, y0 + h - 4}}, 9, legs, kInk, kLine);
  roundedRect(cr, x0 + w * 0.28, y0 + h * 0.32, w * 0.44, h * 0.3, 8);
  fillGradientOutline(cr, y0 + h * 0.32, y0 + h * 0.62, lighten(top, 0.25f), top, kInk, kLine);
  auto stick = [&](double hx, double hy, double ex, double ey) {
    strokeLimb(cr, {{hx, hy}, {ex, ey}}, 6, glow, kInk, 1.2);
    radialGlow(cr, (hx + ex) * 0.5, (hy + ey) * 0.5, 18, glow, 0.7);
  };
  // Left arm swings down; right arm holds the stick up when telling.
  strokeLimb(cr, {{x0 + w * 0.32, y0 + h * 0.36}, {x0 + w * 0.14, y0 + h * 0.52}}, 7, skin, kInk, kLine);
  stick(x0 + w * 0.14, y0 + h * 0.52, x0 + w * 0.04, y0 + h * 0.66);
  if (c.variant == 1)
  {
    strokeLimb(cr, {{x0 + w * 0.68, y0 + h * 0.36}, {x0 + w * 0.82, y0 + h * 0.12}}, 7, skin, kInk, kLine);
    stick(x0 + w * 0.82, y0 + h * 0.12, x0 + w * 0.9, y0 - 10);
  }
  else
  {
    strokeLimb(cr, {{x0 + w * 0.68, y0 + h * 0.36}, {x0 + w * 0.88, y0 + h * 0.3}}, 7, skin, kInk, kLine);
    stick(x0 + w * 0.88, y0 + h * 0.3, x0 + w * 1.0, y0 + h * 0.18);
  }
  circle(cr, x0 + w * 0.5, y0 + h * 0.2, w * 0.2);
  fillOutline(cr, skin, kInk, kLine);
  // Hair spikes and visor.
  for (int i = 0; i < 4; ++i)
    strokeLimb(cr, {{x0 + w * (0.38 + 0.08 * i), y0 + h * 0.08}, {x0 + w * (0.34 + 0.1 * i), y0 - 4}}, 5, rgb(0, 230, 255), kInk, 1.0);
  roundedRect(cr, x0 + w * 0.4, y0 + h * 0.17, w * 0.28, h * 0.05, 3);
  setColor(cr, rgb(255, 240, 80));
  cairo_fill(cr);
}


// Night Stalker (level 4): a tall, thin shape in a long coat with a hat,
// made to be seen mostly as two yellow eyes. Variant 1 is frozen in light
// (arms up over its face, flinching); 2 is the lunge (leaning in, claws out).
void nightStalker(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  const Color coat = rgb(30, 28, 44), coatHi = rgb(70, 64, 96), skin = rgb(120, 116, 140);
  const double lean = c.variant == 2 ? w * 0.14 : 0.0;
  const double stride = c.frame ? 5.0 : -5.0;
  // Legs, long and thin.
  strokeLimb(cr, {{x0 + w * 0.42, y0 + h * 0.62}, {x0 + w * 0.36 + stride, y0 + h - 4}}, 8, coat, kInk, kLine);
  strokeLimb(cr, {{x0 + w * 0.58, y0 + h * 0.62}, {x0 + w * 0.64 - stride, y0 + h - 4}}, 8, coat, kInk, kLine);
  // The coat: a tall trapezoid, flaring at the hem.
  cairo_move_to(cr, x0 + w * 0.34 + lean, y0 + h * 0.2);
  cairo_line_to(cr, x0 + w * 0.66 + lean, y0 + h * 0.2);
  cairo_line_to(cr, x0 + w * 0.8, y0 + h * 0.74);
  cairo_line_to(cr, x0 + w * 0.2, y0 + h * 0.74);
  cairo_close_path(cr);
  fillGradientOutline(cr, y0 + h * 0.2, y0 + h * 0.74, coatHi, coat, kInk, kLine);
  // Arms.
  if (c.variant == 1)
  {
    for (int sgn : {-1, 1})
      strokeLimb(cr, {{x0 + w * (0.5 + sgn * 0.14), y0 + h * 0.24}, {x0 + w * (0.5 + sgn * 0.32), y0 + h * 0.14},
                       {x0 + w * (0.5 + sgn * 0.08), y0 + h * 0.08}}, 7, coat, kInk, kLine);
  }
  else
  {
    const double reach = c.variant == 2 ? w * 0.5 : w * 0.18;
    strokeLimb(cr, {{x0 + w * 0.6 + lean, y0 + h * 0.26}, {x0 + w * 0.62 + reach, y0 + h * 0.4}}, 7, coat, kInk, kLine);
    for (int k = 0; k < 3; ++k)
      strokeLimb(cr, {{x0 + w * 0.62 + reach, y0 + h * 0.4}, {x0 + w * 0.7 + reach, y0 + h * (0.37 + 0.03 * k)}}, 2, skin, kInk, 0.6);
    strokeLimb(cr, {{x0 + w * 0.4, y0 + h * 0.26}, {x0 + w * 0.3, y0 + h * 0.5}}, 7, coat, kInk, kLine);
  }
  // Head under a wide-brimmed hat; the face is a void with eyes.
  circle(cr, x0 + w * 0.5 + lean, y0 + h * 0.13, w * 0.15);
  fillOutline(cr, rgb(12, 10, 18), kInk, kLine);
  roundedRect(cr, x0 + w * 0.18 + lean, y0 + h * 0.04, w * 0.64, h * 0.025, 2);
  fillOutline(cr, coat, kInk, 1.4);
  roundedRect(cr, x0 + w * 0.34 + lean, y0 - h * 0.03, w * 0.32, h * 0.08, 4);
  fillOutline(cr, coat, kInk, 1.4);
  const double eye = c.variant == 2 ? 5.0 : (c.variant == 1 ? 1.5 : 3.0);
  for (int sgn : {-1, 1})
  {
    cairo_save(cr);
    cairo_translate(cr, x0 + w * 0.5 + lean + sgn * w * 0.07, y0 + h * 0.14);
    cairo_scale(cr, 1.0, eye / 3.0);
    circle(cr, 0, 0, 3.5);
    cairo_restore(cr);
    setColor(cr, rgb(255, 230, 90));
    cairo_fill(cr);
  }
  radialGlow(cr, x0 + w * 0.5 + lean, y0 + h * 0.14, w * 0.3, rgb(255, 220, 80), c.variant == 2 ? 0.5 : 0.25);
}

// Looter (level 4): a hunched figure in a hoodie with a swag sack. Variant
// 1: the sack is full and it's running.
void looter(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  const Color hoodie = rgb(70, 80, 60), pants = rgb(40, 40, 50), sack = rgb(150, 120, 80);
  const double run = c.frame ? 7.0 : -7.0;
  strokeLimb(cr, {{x0 + w * 0.42, y0 + h * 0.62}, {x0 + w * 0.32 + run, y0 + h - 4}}, 9, pants, kInk, kLine);
  strokeLimb(cr, {{x0 + w * 0.58, y0 + h * 0.62}, {x0 + w * 0.66 - run, y0 + h - 4}}, 9, pants, kInk, kLine);
  // The sack over its back (left: it faces right).
  circle(cr, x0 + w * 0.22, y0 + h * 0.36, w * (c.variant == 1 ? 0.24 : 0.14));
  fillGradientOutline(cr, y0 + h * 0.1, y0 + h * 0.6, lighten(sack, 0.2f), sack, kInk, kLine);
  // Hunched body.
  roundedRect(cr, x0 + w * 0.3, y0 + h * 0.3, w * 0.42, h * 0.34, 12);
  fillGradientOutline(cr, y0 + h * 0.3, y0 + h * 0.64, lighten(hoodie, 0.2f), hoodie, kInk, kLine);
  strokeLimb(cr, {{x0 + w * 0.36, y0 + h * 0.36}, {x0 + w * 0.22, y0 + h * 0.24}}, 7, hoodie, kInk, kLine);
  strokeLimb(cr, {{x0 + w * 0.66, y0 + h * 0.38}, {x0 + w * 0.86, y0 + h * 0.5}}, 7, hoodie, kInk, kLine);
  // Hooded head, leaning forward.
  circle(cr, x0 + w * 0.66, y0 + h * 0.24, w * 0.16);
  fillGradientOutline(cr, y0 + h * 0.08, y0 + h * 0.4, lighten(hoodie, 0.25f), hoodie, kInk, kLine);
  circle(cr, x0 + w * 0.72, y0 + h * 0.26, w * 0.09);
  setColor(cr, rgb(16, 14, 20));
  cairo_fill(cr);
  for (int sgn : {-1, 1})
  {
    circle(cr, x0 + w * 0.72 + sgn * 3.5, y0 + h * 0.25, 1.8);
    setColor(cr, rgb(255, 255, 255));
    cairo_fill(cr);
  }
}

// Grid Leech (level 4): a segmented green slug crackling with current.
// Variant 1: the sparks before it touches you.
void gridLeech(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  const Color body = rgb(90, 200, 70), dark = rgb(30, 90, 30);
  for (int k = 3; k >= 0; --k)
  {
    const double cx = x0 + w * (0.2 + 0.2 * k), cy = y0 + h * 0.55 + (c.frame && k % 2 ? -3.0 : 0.0);
    circle(cr, cx, cy, h * (0.26 + 0.04 * (k == 3)));
    fillGradientOutline(cr, cy - h * 0.3, cy + h * 0.3, lighten(body, 0.3f), dark, kInk, kLine);
  }
  // The head's jaws.
  circle(cr, x0 + w * 0.88, y0 + h * 0.5, 4);
  setColor(cr, rgb(255, 255, 160));
  cairo_fill(cr);
  // Crackle.
  const int arcs = c.variant == 1 ? 6 : 3;
  for (int k = 0; k < arcs; ++k)
  {
    const double a = (k * 2.1 + c.frame) * 1.3;
    const double sx = x0 + w * 0.5 + std::cos(a) * w * 0.3, sy = y0 + h * 0.5 + std::sin(a) * h * 0.3;
    strokeLimb(cr, {{sx, sy}, {sx + std::cos(a) * 10, sy + std::sin(a + 1) * 10}, {sx + std::cos(a) * 18, sy + std::sin(a) * 18}},
      2, rgb(200, 255, 160), rgb(200, 255, 160), 0.0);
  }
  radialGlow(cr, x0 + w * 0.5, y0 + h * 0.5, w * (c.variant == 1 ? 1.4 : 0.9), rgb(120, 255, 90), 0.6);
}

// Sludge Gator (level 5): a long green snout and a ridged back. Variant 1
// wears sunglasses; 2 and 3 have their jaws open (lunging, or nodding in a
// bubble).
void sludgeGator(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  const Color hide = rgb(70, 120, 50), belly = rgb(170, 190, 110);
  const bool open = c.variant >= 2;
  const double nod = (c.variant == 3 && c.frame) ? 4.0 : 0.0;
  // Tail and body.
  cairo_move_to(cr, x0 - 6, y0 + h * 0.7);
  cairo_curve_to(cr, x0 + w * 0.1, y0 + h * 0.2, x0 + w * 0.5, y0 + h * 0.1, x0 + w * 0.66, y0 + h * 0.3);
  cairo_line_to(cr, x0 + w * 0.66, y0 + h * 0.95);
  cairo_curve_to(cr, x0 + w * 0.4, y0 + h, x0 + w * 0.1, y0 + h * 0.95, x0 - 6, y0 + h * 0.7);
  cairo_close_path(cr);
  fillGradientOutline(cr, y0, y0 + h, lighten(hide, 0.2f), hide, kInk, kLine);
  for (int k = 0; k < 5; ++k)
  {
    const double sx = x0 + w * (0.12 + 0.11 * k), sy = y0 + h * (0.32 - 0.03 * (k % 2));
    cairo_move_to(cr, sx - 5, sy + 4);
    cairo_line_to(cr, sx, sy - 6);
    cairo_line_to(cr, sx + 5, sy + 4);
    cairo_close_path(cr);
    fillOutline(cr, darken(hide, 0.2f), kInk, 1.4);
  }
  // Snout: upper and lower jaw.
  const double jaw = open ? h * 0.5 : 0.0;
  cairo_move_to(cr, x0 + w * 0.6, y0 + h * 0.3 + nod);
  cairo_line_to(cr, x0 + w + 8, y0 + h * 0.5 - jaw * 0.6 + nod);
  cairo_line_to(cr, x0 + w + 8, y0 + h * 0.62 - jaw * 0.6 + nod);
  cairo_line_to(cr, x0 + w * 0.6, y0 + h * 0.62 + nod);
  cairo_close_path(cr);
  fillGradientOutline(cr, y0, y0 + h, lighten(hide, 0.25f), hide, kInk, kLine);
  cairo_move_to(cr, x0 + w * 0.6, y0 + h * 0.66 + nod);
  cairo_line_to(cr, x0 + w + 4, y0 + h * 0.7 + jaw * 0.3 + nod);
  cairo_line_to(cr, x0 + w + 4, y0 + h * 0.84 + jaw * 0.3 + nod);
  cairo_line_to(cr, x0 + w * 0.6, y0 + h * 0.92 + nod);
  cairo_close_path(cr);
  fillOutline(cr, belly, kInk, kLine);
  if (open)
    for (int k = 0; k < 4; ++k)
    {
      const double tx = x0 + w * (0.68 + 0.08 * k), ty = y0 + h * 0.62 + nod - jaw * 0.4 * (k + 1) / 4.0;
      cairo_move_to(cr, tx - 3, ty);
      cairo_line_to(cr, tx, ty + 6);
      cairo_line_to(cr, tx + 3, ty);
      cairo_close_path(cr);
      setColor(cr, rgb(250, 250, 230));
      cairo_fill(cr);
    }
  // The eye on its bump, or the shades.
  circle(cr, x0 + w * 0.62, y0 + h * 0.26 + nod, h * 0.2);
  fillOutline(cr, lighten(hide, 0.1f), kInk, kLine);
  if (c.variant == 1 || c.variant == 3)
  {
    roundedRect(cr, x0 + w * 0.54, y0 + h * 0.18 + nod, w * 0.22, h * 0.16, 3);
    fillOutline(cr, rgb(16, 16, 22), kInk, 1.2);
    cairo_move_to(cr, x0 + w * 0.58, y0 + h * 0.21 + nod);
    cairo_line_to(cr, x0 + w * 0.63, y0 + h * 0.21 + nod);
    cairo_set_line_width(cr, 2.0);
    setColor(cr, rgb(200, 220, 255));
    cairo_stroke(cr);
  }
  else
  {
    circle(cr, x0 + w * 0.64, y0 + h * 0.24 + nod, 4);
    setColor(cr, rgb(255, 220, 60));
    cairo_fill(cr);
    roundedRect(cr, x0 + w * 0.635, y0 + h * 0.2 + nod, 2, 8, 1);
    setColor(cr, kInk);
    cairo_fill(cr);
  }
}

// Pipe Rat (level 5): grey, pink tail, fast.
void pipeRat(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  const Color fur = rgb(120, 110, 110);
  strokeLimb(cr, {{x0 + w * 0.15, y0 + h * 0.6}, {x0 - 8, y0 + h * 0.3}, {x0 - 18, y0 + h * 0.6}}, 3, rgb(230, 150, 160),
    kInk, 1.0);
  cairo_save(cr);
  cairo_translate(cr, x0 + w * 0.45, y0 + h * 0.5);
  cairo_scale(cr, w * 0.42, h * 0.5);
  circle(cr, 0, 0, 1.0);
  cairo_restore(cr);
  fillGradientOutline(cr, y0, y0 + h, lighten(fur, 0.2f), fur, kInk, kLine);
  cairo_move_to(cr, x0 + w * 0.75, y0 + h * 0.2);
  cairo_line_to(cr, x0 + w + 6, y0 + h * 0.6);
  cairo_line_to(cr, x0 + w * 0.75, y0 + h * 0.85);
  cairo_close_path(cr);
  fillOutline(cr, fur, kInk, kLine);
  circle(cr, x0 + w + 5, y0 + h * 0.6, 2.5);
  setColor(cr, rgb(240, 140, 150));
  cairo_fill(cr);
  circle(cr, x0 + w * 0.68, y0 + h * 0.15, 5);
  fillOutline(cr, rgb(220, 150, 160), kInk, 1.4);
  circle(cr, x0 + w * 0.86, y0 + h * 0.4, 2);
  setColor(cr, rgb(255, 60, 60));
  cairo_fill(cr);
  const double run = c.frame ? 4.0 : -4.0;
  for (double lx : {0.3, 0.65})
    strokeLimb(cr, {{x0 + w * lx, y0 + h * 0.8}, {x0 + w * lx + run, y0 + h + 2}}, 3, darken(fur, 0.2f), kInk, 0.8);
}

// Valve Keeper (level 5): a stocky sewer worker in waders and a gas mask.
// Variant 1: both hands on the wheel.
void valveKeeper(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  const Color overall = rgb(230, 150, 40), waders = rgb(50, 70, 50), mask = rgb(60, 60, 64);
  const double stride = c.frame ? 4.0 : -4.0;
  roundedRect(cr, x0 + w * 0.2 + stride * 0.5, y0 + h * 0.6, w * 0.25, h * 0.4, 5);
  fillOutline(cr, waders, kInk, kLine);
  roundedRect(cr, x0 + w * 0.55 - stride * 0.5, y0 + h * 0.6, w * 0.25, h * 0.4, 5);
  fillOutline(cr, waders, kInk, kLine);
  roundedRect(cr, x0 + w * 0.12, y0 + h * 0.24, w * 0.76, h * 0.42, 12);
  fillGradientOutline(cr, y0 + h * 0.24, y0 + h * 0.66, lighten(overall, 0.2f), overall, kInk, kLine);
  // Reflective stripes.
  roundedRect(cr, x0 + w * 0.14, y0 + h * 0.46, w * 0.72, 5, 2);
  setColor(cr, rgb(240, 240, 220));
  cairo_fill(cr);
  if (c.variant == 1)
  {
    strokeLimb(cr, {{x0 + w * 0.3, y0 + h * 0.32}, {x0 + w * 0.7, y0 + h * 0.14}}, 9, overall, kInk, kLine);
    strokeLimb(cr, {{x0 + w * 0.7, y0 + h * 0.32}, {x0 + w * 0.95, y0 + h * 0.18}}, 9, overall, kInk, kLine);
  }
  else
  {
    strokeLimb(cr, {{x0 + w * 0.16, y0 + h * 0.3}, {x0 + w * 0.02, y0 + h * 0.56}}, 9, overall, kInk, kLine);
    strokeLimb(cr, {{x0 + w * 0.84, y0 + h * 0.3}, {x0 + w * 1.0, y0 + h * 0.5}, {x0 + w * 1.08, y0 + h * 0.42}}, 9,
      overall, kInk, kLine);
  }
  // Helmet and gas mask.
  circle(cr, x0 + w * 0.55, y0 + h * 0.13, w * 0.22);
  fillOutline(cr, mask, kInk, kLine);
  for (int sgn : {-1, 1})
  {
    circle(cr, x0 + w * (0.55 + sgn * 0.09), y0 + h * 0.11, 5);
    fillOutline(cr, rgb(180, 230, 120), kInk, 1.2);
  }
  circle(cr, x0 + w * 0.7, y0 + h * 0.2, 7);
  fillOutline(cr, rgb(90, 90, 96), kInk, 1.4);
  cairo_arc(cr, x0 + w * 0.55, y0 + h * 0.06, w * 0.26, kPi, 2 * kPi);
  fillOutline(cr, rgb(255, 210, 40), kInk, kLine);
}

// A Bubble Gun bubble (variant 2: the shot), or the raft (bubble_raft).
void bubble(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  const double wob = c.frame ? 0.04 : -0.04;
  cairo_save(cr);
  cairo_translate(cr, x0 + w * 0.5, y0 + h * 0.5);
  cairo_scale(cr, w * (0.52 + wob), h * (0.52 - wob));
  circle(cr, 0, 0, 1.0);
  cairo_restore(cr);
  cairo_pattern_t* g = cairo_pattern_create_radial(x0 + w * 0.4, y0 + h * 0.35, 1, x0 + w * 0.5, y0 + h * 0.5, w * 0.55);
  cairo_pattern_add_color_stop_rgba(g, 0.0, 1.0, 1.0, 1.0, 0.05);
  cairo_pattern_add_color_stop_rgba(g, 0.75, 0.6, 0.9, 1.0, 0.18);
  cairo_pattern_add_color_stop_rgba(g, 1.0, 0.85, 1.0, 1.0, 0.75);
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  cairo_set_line_width(cr, 2.0);
  cairo_set_source_rgba(cr, 0.9, 1.0, 1.0, 0.85);
  cairo_stroke(cr);
  // The shine.
  cairo_arc(cr, x0 + w * 0.34, y0 + h * 0.3, std::min(w, h) * 0.14, kPi, kPi * 1.6);
  cairo_set_line_width(cr, 3.0);
  cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.9);
  cairo_stroke(cr);
}

void bubbleRaft(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  const int n = std::max(3, int(w / 26.0));
  for (int row = 0; row < 2; ++row)
    for (int k = 0; k < n - row; ++k)
    {
      const double r = h * (row == 0 ? 0.42 : 0.34);
      const double cx = x0 + w * (k + 0.5 + row * 0.5) / n, cy = y0 + h * (row == 0 ? 0.62 : 0.3);
      circle(cr, cx, cy + ((k + c.frame) % 2 ? 2.0 : -2.0), r);
      cairo_set_source_rgba(cr, 0.75, 0.95, 1.0, 0.35);
      cairo_fill_preserve(cr);
      cairo_set_line_width(cr, 2.0);
      cairo_set_source_rgba(cr, 0.95, 1.0, 1.0, 0.9);
      cairo_stroke(cr);
      cairo_arc(cr, cx - r * 0.3, cy - r * 0.35, r * 0.3, kPi, kPi * 1.6);
      cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.9);
      cairo_stroke(cr);
    }
}

// The flood valve: a red wheel on a pipe. Variant 1 wears a padlock; the
// frame turns the wheel.
void valve(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  roundedRect(cr, x0 + w * 0.4, y0 + h * 0.4, w * 0.2, h * 0.6, 3);
  fillGradientOutline(cr, y0, y0 + h, rgb(150, 150, 140), rgb(80, 80, 76), kInk, kLine);
  const double cx = x0 + w * 0.5, cy = y0 + h * 0.4, r = w * 0.42;
  circle(cr, cx, cy, r);
  cairo_set_line_width(cr, 9.0);
  setColor(cr, kInk);
  cairo_stroke_preserve(cr);
  cairo_set_line_width(cr, 6.0);
  setColor(cr, rgb(210, 40, 40));
  cairo_stroke(cr);
  for (int k = 0; k < 3; ++k)
  {
    const double a = (k / 3.0 + c.frame / 12.0) * 2 * kPi;
    strokeLimb(cr, {{cx - std::cos(a) * r, cy - std::sin(a) * r}, {cx + std::cos(a) * r, cy + std::sin(a) * r}}, 4,
      rgb(190, 40, 40), kInk, 1.2);
  }
  circle(cr, cx, cy, 8);
  fillOutline(cr, rgb(230, 230, 220), kInk, 1.6);
  if (c.variant == 1)
  {
    cairo_arc(cr, cx + r * 0.6, cy + r * 0.4, 9, kPi, 2 * kPi);
    cairo_set_line_width(cr, 4.0);
    setColor(cr, rgb(200, 200, 210));
    cairo_stroke(cr);
    roundedRect(cr, cx + r * 0.6 - 12, cy + r * 0.4, 24, 20, 3);
    fillOutline(cr, rgb(250, 210, 60), kInk, 1.6);
  }
}

// A rat pipe's mouth in the wall (faces right).
void ratPipe(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  roundedRect(cr, x0 - 10, y0 + h * 0.1, w * 0.9 + 10, h * 0.8, 6);
  fillGradientOutline(cr, y0, y0 + h, rgb(120, 130, 120), rgb(60, 70, 60), kInk, kLine);
  cairo_save(cr);
  cairo_translate(cr, x0 + w * 0.82, y0 + h * 0.5);
  cairo_scale(cr, w * 0.16, h * 0.42);
  circle(cr, 0, 0, 1.0);
  cairo_restore(cr);
  fillOutline(cr, rgb(20, 20, 18), kInk, kLine);
  circle(cr, x0 + w * 0.82, y0 + h * 0.42, 2);
  setColor(cr, rgb(255, 60, 60));
  cairo_fill(cr);
  circle(cr, x0 + w * 0.86, y0 + h * 0.42, 2);
  cairo_fill(cr);
}

// A sewer floor grate: dark bars over a faint glow from below.
void grate(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  cairo_rectangle(cr, x0, y0, w, h);
  setColor(cr, rgb(24, 30, 22));
  cairo_fill(cr);
  cairo_rectangle(cr, x0, y0 + h * 0.3, w, h * 0.7);
  cairo_pattern_t* g = cairo_pattern_create_linear(0, y0, 0, y0 + h);
  cairo_pattern_add_color_stop_rgba(g, 0.0, 0.3, 0.5, 0.1, 0.0);
  cairo_pattern_add_color_stop_rgba(g, 1.0, 0.45, 0.75, 0.15, 0.55);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
  roundedRect(cr, x0, y0, w, h * 0.22, 3);
  fillGradientOutline(cr, y0, y0 + h * 0.22, rgb(150, 160, 150), rgb(90, 100, 90), kInk, 1.6);
  for (int k = 0; k < 6; ++k)
  {
    roundedRect(cr, x0 + 3 + k * (w - 6) / 6.0, y0 + h * 0.2, 5, h * 0.8, 2);
    fillGradientOutline(cr, y0, y0 + h, rgb(130, 140, 130), rgb(60, 66, 60), kInk, 1.0);
  }
}

// /dev/null: a big round outflow pipe with a stencil.
void devnullPipe(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  circle(cr, x0 + w * 0.5, y0 + h * 0.5, w * 0.5);
  fillGradientOutline(cr, y0, y0 + h, rgb(150, 150, 140), rgb(70, 70, 66), kInk, kLine);
  circle(cr, x0 + w * 0.5, y0 + h * 0.5, w * 0.38);
  cairo_pattern_t* g = cairo_pattern_create_radial(x0 + w * 0.5, y0 + h * 0.5, 2, x0 + w * 0.5, y0 + h * 0.5, w * 0.38);
  cairo_pattern_add_color_stop_rgb(g, 0.0, 0.0, 0.0, 0.0);
  cairo_pattern_add_color_stop_rgb(g, 1.0, 0.12, 0.12, 0.14);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
}

// The Duck Rapids ride: a big rubber duck (faces right).
void rubberDuck(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  const Color yellow = rgb(255, 214, 40);
  cairo_move_to(cr, x0, y0 + h * 0.3);
  cairo_curve_to(cr, x0 + w * 0.1, y0 + h * 1.05, x0 + w * 0.8, y0 + h * 1.1, x0 + w * 0.92, y0 + h * 0.5);
  cairo_line_to(cr, x0 + w * 0.2, y0 + h * 0.45);
  cairo_close_path(cr);
  fillGradientOutline(cr, y0, y0 + h, lighten(yellow, 0.3f), darken(yellow, 0.15f), kInk, kLine);
  circle(cr, x0 + w * 0.82, y0 + h * 0.18, h * 0.32);
  fillGradientOutline(cr, y0 - h * 0.2, y0 + h * 0.5, lighten(yellow, 0.3f), yellow, kInk, kLine);
  cairo_move_to(cr, x0 + w * 0.92, y0 + h * 0.18);
  cairo_line_to(cr, x0 + w + 12, y0 + h * 0.26);
  cairo_line_to(cr, x0 + w * 0.92, y0 + h * 0.36);
  cairo_close_path(cr);
  fillOutline(cr, rgb(255, 130, 30), kInk, kLine);
  circle(cr, x0 + w * 0.86, y0 + h * 0.08, 3.5);
  setColor(cr, kInk);
  cairo_fill(cr);
}

using DrawFn = void (*)(const Ctx&);

DrawFn routineFor(const std::string& key)
{
  static const std::map<std::string, DrawFn> kRoutines{
    {"glass_crawler", glassCrawler},
    {"squeegee_drone", squeegeeDrone},
    {"penthouse_sniper", penthouseSniper},
    {"bouncer", bouncer},
    {"cardboard_bouncer", cardboardBouncer},
    {"disco_drone", discoDrone},
    {"glow_raver", glowRaver},
    {"night_stalker", nightStalker},
    {"looter", looter},
    {"grid_leech", gridLeech},
    {"sludge_gator", sludgeGator},
    {"pipe_rat", pipeRat},
    {"valve_keeper", valveKeeper},
    {"bubble", bubble},
    {"bubble_raft", bubbleRaft},
    {"valve", valve},
    {"rat_pipe", ratPipe},
    {"grate", grate},
    {"devnull_pipe", devnullPipe},
    {"rubber_duck", rubberDuck},
  };
  const auto it = kRoutines.find(key);
  return it == kRoutines.end() ? nullptr : it->second;
}

// Anything without its own drawing yet: a generic bot in the theme colours.
void fallbackBot(const Ctx& c)
{
  cairo_t* cr = c.cr;
  roundedRect(cr, kM + 2, kM + 2, c.w - 4, c.h - 4, 10);
  fillGradientOutline(cr, kM, kM + c.h, lighten(c.t.enemyLight, 0.2f), c.t.enemyBody, kInk, kLine);
  radialGlow(cr, kM + c.w * 0.6, kM + c.h * 0.35, 12, c.t.enemyEye, 0.7);
  circle(cr, kM + c.w * 0.62, kM + c.h * 0.35, 4);
  setColor(cr, lighten(c.t.enemyEye, 0.3f));
  cairo_fill(cr);
}

} // namespace

const Sprite& styledEnemySprite(const Art& art, const Renderer& r, const Theme& t, const std::string& key, int variant,
  int frame, int wCells, int hCells)
{
  const std::string id = key + "/" + std::to_string(variant) + "/" + std::to_string(frame) + "/" +
    std::to_string(wCells) + "x" + std::to_string(hCells);
  auto it = art.styled.find(id);
  if (it != art.styled.end())
    return it->second;
  const double w = wCells * 32.0, h = hCells * 32.0;
  VectorImage img(int(w + 2 * kM), int(h + 2 * kM));
  const Ctx c{img.cr(), t, w, h, variant, frame};
  if (DrawFn fn = routineFor(key))
    fn(c);
  else
    fallbackBot(c);
  Sprite s;
  const float ax = float(kM + w * 0.5), ay = float(kM + h);
  s.facing[0] = img.toTexture(r, ax, ay, false);
  s.facing[1] = img.toTexture(r, ax, ay, true);
  return art.styled.emplace(id, std::move(s)).first->second;
}

} // namespace gr
