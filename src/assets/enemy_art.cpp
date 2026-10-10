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

// Track Hopper (level 6): a springy rail-yard robot on piston legs.
// Variant 0 stands, 1 crouches to leap, 2 is in the air (legs tucked).
void trackHopper(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  const Color body = rgb(230, 170, 40), dark = rgb(60, 56, 64);
  const double squat = c.variant == 1 ? h * 0.18 : 0.0;
  const double tuck = c.variant == 2 ? h * 0.15 : 0.0;
  // Legs: two pistons.
  for (double lx : {0.28, 0.66})
  {
    strokeLimb(cr, {{x0 + w * lx, y0 + h * 0.55 + squat}, {x0 + w * (lx - 0.08), y0 + h * 0.8 - tuck},
      {x0 + w * lx, y0 + h - tuck}}, 7, dark, kInk, kLine);
    roundedRect(cr, x0 + w * (lx - 0.12), y0 + h - 8 - tuck, w * 0.26, 8, 3);
    fillOutline(cr, rgb(90, 90, 100), kInk, kLine);
  }
  // Body: a hazard-striped box with one big eye.
  roundedRect(cr, x0 + w * 0.08, y0 + h * 0.18 + squat, w * 0.84, h * 0.42, 10);
  fillGradientOutline(cr, y0 + h * 0.18 + squat, y0 + h * 0.6 + squat, lighten(body, 0.2f), body, kInk, kLine);
  for (int i = 0; i < 4; ++i)
  {
    cairo_move_to(cr, x0 + w * (0.14 + i * 0.2), y0 + h * 0.52 + squat);
    cairo_line_to(cr, x0 + w * (0.24 + i * 0.2), y0 + h * 0.52 + squat);
    cairo_line_to(cr, x0 + w * (0.18 + i * 0.2), y0 + h * 0.6 + squat);
    cairo_line_to(cr, x0 + w * (0.08 + i * 0.2), y0 + h * 0.6 + squat);
    cairo_close_path(cr);
    setColor(cr, rgb(30, 30, 30));
    cairo_fill(cr);
  }
  radialGlow(cr, x0 + w * 0.68, y0 + h * 0.34 + squat, 16, rgb(255, 60, 40), 0.8);
  circle(cr, x0 + w * 0.68, y0 + h * 0.34 + squat, 7);
  fillOutline(cr, rgb(255, 120, 90), kInk, 1.4);
  // Antenna.
  strokeLimb(cr, {{x0 + w * 0.3, y0 + h * 0.2 + squat}, {x0 + w * 0.22, y0 + h * 0.04 + squat}}, 3, dark, kInk, 1.0);
}

// Rail Drone (level 6): a sleek hover pod with a bomb bay underneath
// (variant 1: the bay is open).
void railDrone(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  cairo_save(cr);
  cairo_translate(cr, x0 + w * 0.5, y0 + h * 0.45);
  cairo_scale(cr, w * 0.5, h * 0.36);
  circle(cr, 0, 0, 1.0);
  cairo_restore(cr);
  fillGradientOutline(cr, y0 + h * 0.1, y0 + h * 0.8, rgb(210, 216, 230), rgb(110, 116, 136), kInk, kLine);
  roundedRect(cr, x0 + w * 0.55, y0 + h * 0.25, w * 0.3, h * 0.2, 4);
  fillOutline(cr, rgb(60, 200, 255), kInk, 1.4);
  // Bay doors.
  const double open = c.variant == 1 ? 10 : 0;
  roundedRect(cr, x0 + w * 0.3 - open, y0 + h * 0.72, w * 0.18, 8, 2);
  fillOutline(cr, rgb(70, 74, 90), kInk, 1.2);
  roundedRect(cr, x0 + w * 0.52 + open, y0 + h * 0.72, w * 0.18, 8, 2);
  fillOutline(cr, rgb(70, 74, 90), kInk, 1.2);
  if (c.variant == 1)
    radialGlow(cr, x0 + w * 0.5, y0 + h * 0.8, 18, rgb(255, 80, 60), 0.8);
  // Thruster glow at the back.
  radialGlow(cr, x0 + w * 0.05, y0 + h * 0.45, 20, rgb(120, 200, 255), 0.7);
}

// Decoupler (level 6): a hunched gremlin in grease-stained overalls with a
// huge wrench (variant 1: heaving on the coupling).
void decoupler(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  const Color suit = rgb(70, 90, 120), skin = rgb(120, 200, 110);
  roundedRect(cr, x0 + w * 0.2, y0 + h * 0.62, w * 0.24, h * 0.38, 5);
  fillOutline(cr, darken(suit, 0.3f), kInk, kLine);
  roundedRect(cr, x0 + w * 0.56, y0 + h * 0.62, w * 0.24, h * 0.38, 5);
  fillOutline(cr, darken(suit, 0.3f), kInk, kLine);
  roundedRect(cr, x0 + w * 0.1, y0 + h * 0.3, w * 0.8, h * 0.4, 12);
  fillGradientOutline(cr, y0 + h * 0.3, y0 + h * 0.7, lighten(suit, 0.2f), suit, kInk, kLine);
  // Head with goggles.
  circle(cr, x0 + w * 0.5, y0 + h * 0.2, w * 0.24);
  fillOutline(cr, skin, kInk, kLine);
  for (int sg : {-1, 1})
  {
    circle(cr, x0 + w * (0.5 + sg * 0.1), y0 + h * 0.18, 6);
    fillOutline(cr, rgb(255, 200, 60), kInk, 1.4);
  }
  // The wrench: raised overhead while it works.
  const double ang = c.variant == 1 ? (c.frame ? -0.9 : -0.5) : 0.6;
  const double hx = x0 + w * 0.85, hy = y0 + h * 0.4;
  const double ex = hx + std::cos(ang) * w * 0.5, ey = hy + std::sin(ang) * w * 0.5;
  strokeLimb(cr, {{hx, hy}, {ex, ey}}, 6, rgb(180, 186, 200), kInk, kLine);
  circle(cr, ex, ey, 9);
  fillOutline(cr, rgb(180, 186, 200), kInk, kLine);
}

// A trooper figure (level 7): helmet, visor, body armour, rifle. Used by the
// Rappel Trooper (variant 0 patrol, 1 aiming, 2 on the rope: arms up, legs
// together) and the Shield Trooper (with a riot shield in front; variant 1
// raises the gun over it).
void trooperFigure(const Ctx& c, bool shield)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  const Color armour = shield ? rgb(46, 52, 70) : rgb(70, 78, 64), pants = rgb(36, 38, 46);
  const Color visor = shield ? rgb(255, 80, 60) : rgb(255, 200, 60);
  const bool rope = !shield && c.variant == 2;
  const bool aim = c.variant == 1;
  const double step = rope ? 0.0 : (c.frame ? 5.0 : -5.0);
  // Legs.
  const double spread = rope ? 0.03 : 0.1;
  strokeLimb(cr, {{x0 + w * 0.42, y0 + h * 0.6}, {x0 + w * (0.5 - spread) + step, y0 + h - 5}}, 11, pants, kInk, kLine);
  strokeLimb(cr, {{x0 + w * 0.58, y0 + h * 0.6}, {x0 + w * (0.5 + spread) - step, y0 + h - 5}}, 11, pants, kInk, kLine);
  for (double fx : {0.5 - spread, 0.5 + spread})
  {
    roundedRect(cr, x0 + w * fx - 9, y0 + h - 9, 20, 9, 3);
    fillOutline(cr, rgb(20, 20, 24), kInk, 1.2);
  }
  // Torso: a plated vest with pouches.
  roundedRect(cr, x0 + w * 0.26, y0 + h * 0.3, w * 0.48, h * 0.34, 10);
  fillGradientOutline(cr, y0 + h * 0.3, y0 + h * 0.64, lighten(armour, 0.2f), armour, kInk, kLine);
  for (int i = 0; i < 3; ++i)
  {
    roundedRect(cr, x0 + w * (0.3 + 0.14 * i), y0 + h * 0.5, w * 0.11, h * 0.07, 2);
    fillOutline(cr, darken(armour, 0.3f), kInk, 1.0);
  }
  // Head: helmet and glowing visor, facing right.
  circle(cr, x0 + w * 0.52, y0 + h * 0.2, w * 0.19);
  fillGradientOutline(cr, y0 + h * 0.06, y0 + h * 0.32, lighten(armour, 0.3f), darken(armour, 0.2f), kInk, kLine);
  roundedRect(cr, x0 + w * 0.52, y0 + h * 0.17, w * 0.2, h * 0.05, 3);
  setColor(cr, visor);
  cairo_fill(cr);
  radialGlow(cr, x0 + w * 0.64, y0 + h * 0.195, 14, visor, aim ? 0.9 : 0.4);
  if (rope)
  {
    // Both hands up on the rope.
    strokeLimb(cr, {{x0 + w * 0.34, y0 + h * 0.34}, {x0 + w * 0.46, y0 + h * 0.02}}, 8, armour, kInk, kLine);
    strokeLimb(cr, {{x0 + w * 0.66, y0 + h * 0.34}, {x0 + w * 0.54, y0 + h * 0.06}}, 8, armour, kInk, kLine);
    return;
  }
  // The rifle: level when patrolling, raised and glowing at the muzzle when aiming.
  const double gy = y0 + h * (aim ? 0.36 : 0.44);
  strokeLimb(cr, {{x0 + w * 0.36, gy + 4}, {x0 + w * 0.62, gy + 6}}, 8, armour, kInk, kLine);
  roundedRect(cr, x0 + w * 0.42, gy - 4, w * 0.62, 9, 2);
  fillOutline(cr, rgb(30, 30, 36), kInk, 1.4);
  if (aim)
    radialGlow(cr, x0 + w * 1.06, gy, 16, rgb(255, 140, 60), 0.9);
  if (shield)
  {
    // The riot shield: a tall clear slab in front with a white stencil band.
    roundedRect(cr, x0 + w * 0.72, y0 + h * (c.variant == 1 ? 0.34 : 0.22), w * 0.26, h * 0.74, 6);
    fillGradientOutline(cr, y0 + h * 0.2, y0 + h, rgba(170, 200, 230, 210), rgba(90, 110, 140, 210), kInk, kLine);
    for (int i = 0; i < 4; ++i)
    {
      cairo_rectangle(cr, x0 + w * 0.76, y0 + h * (0.46 + 0.05 * i), w * 0.18, h * 0.02);
      setColor(cr, rgba(255, 255, 255, 140));
      cairo_fill(cr);
    }
  }
}

void rappelTrooper(const Ctx& c) { trooperFigure(c, false); }
void shieldTrooper(const Ctx& c) { trooperFigure(c, true); }

// Hover Biker (level 7): a low jet bike with a hunched rider; variant 1 is
// the rev before a charge (headlight blazing, exhaust flaring).
void hoverBiker(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  const Color paint = rgb(220, 40, 70), dark = rgb(40, 36, 46);
  const bool rev = c.variant == 1;
  const double bob = c.frame ? 2.0 : -2.0;
  radialGlow(cr, x0 + w * 0.5, y0 + h - 4, w * 0.4, rgb(90, 200, 255), rev ? 0.9 : 0.5);
  // The bike: a swept wedge.
  cairo_move_to(cr, x0 + w * 0.02, y0 + h * 0.55 + bob);
  cairo_line_to(cr, x0 + w * 0.3, y0 + h * 0.42 + bob);
  cairo_line_to(cr, x0 + w * 0.82, y0 + h * 0.48 + bob);
  cairo_line_to(cr, x0 + w * 0.98, y0 + h * 0.66 + bob);
  cairo_line_to(cr, x0 + w * 0.86, y0 + h * 0.84 + bob);
  cairo_line_to(cr, x0 + w * 0.1, y0 + h * 0.84 + bob);
  cairo_close_path(cr);
  fillGradientOutline(cr, y0 + h * 0.42, y0 + h * 0.84, lighten(paint, 0.25f), paint, kInk, kLine);
  roundedRect(cr, x0 + w * 0.14, y0 + h * 0.7 + bob, w * 0.7, h * 0.08, 3);
  setColor(cr, dark);
  cairo_fill(cr);
  // Rider: hunched forward, helmet low.
  roundedRect(cr, x0 + w * 0.34, y0 + h * 0.16 + bob, w * 0.26, h * 0.34, 8);
  fillOutline(cr, dark, kInk, kLine);
  strokeLimb(cr, {{x0 + w * 0.56, y0 + h * 0.28 + bob}, {x0 + w * 0.76, y0 + h * 0.46 + bob}}, 7, dark, kInk, kLine);
  circle(cr, x0 + w * 0.64, y0 + h * 0.16 + bob, h * 0.15);
  fillGradientOutline(cr, y0, y0 + h * 0.3, rgb(250, 250, 255), rgb(150, 150, 170), kInk, kLine);
  roundedRect(cr, x0 + w * 0.65, y0 + h * 0.12 + bob, w * 0.08, h * 0.07, 2);
  setColor(cr, rgb(30, 30, 40));
  cairo_fill(cr);
  // Headlight and exhaust.
  radialGlow(cr, x0 + w * 0.97, y0 + h * 0.62 + bob, rev ? 40 : 16, rgb(255, 250, 200), rev ? 1.0 : 0.6);
  radialGlow(cr, x0 + w * 0.02, y0 + h * 0.58 + bob, rev ? 34 : 14, rgb(255, 140, 40), rev ? 1.0 : 0.5);
}

// The cardboard crowd of the Pilot Seat (level 7 bonus): a runner cut-out on
// an easel and a delivery truck cut-out, both plainly painted cardboard.
void cardboardRunner(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  const Color card = rgb(196, 150, 96), ink = rgb(90, 60, 30);
  strokeLimb(cr, {{x0 + w * 0.5, y0 + h * 0.5}, {x0 + w * 0.15, y0 + h}}, 5, rgb(120, 86, 50), ink, 1.2);
  strokeLimb(cr, {{x0 + w * 0.45, y0 + h * 0.6}, {x0 + w * 0.25, y0 + h * 0.8}, {x0 + w * 0.35, y0 + h - 3}}, 12, card, ink, kLine);
  strokeLimb(cr, {{x0 + w * 0.55, y0 + h * 0.6}, {x0 + w * 0.8, y0 + h * 0.76}, {x0 + w * 0.82, y0 + h - 3}}, 12, card, ink, kLine);
  roundedRect(cr, x0 + w * 0.28, y0 + h * 0.28, w * 0.44, h * 0.36, 8);
  fillOutline(cr, rgb(210, 90, 60), ink, kLine);
  strokeLimb(cr, {{x0 + w * 0.32, y0 + h * 0.34}, {x0 + w * 0.12, y0 + h * 0.46}}, 9, card, ink, kLine);
  strokeLimb(cr, {{x0 + w * 0.68, y0 + h * 0.34}, {x0 + w * 0.9, y0 + h * 0.24}}, 9, card, ink, kLine);
  circle(cr, x0 + w * 0.52, y0 + h * 0.16, w * 0.17);
  fillOutline(cr, card, ink, kLine);
  cairo_arc(cr, x0 + w * 0.56, y0 + h * 0.17, w * 0.08, 0.2, 2.9);
  setColor(cr, ink);
  cairo_set_line_width(cr, 2.0);
  cairo_stroke(cr);
  circle(cr, x0 + w * 0.6, y0 + h * 0.13, 2.5);
  cairo_fill(cr);
}

void cardboardTruck(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  const Color card = rgb(196, 150, 96), ink = rgb(90, 60, 30);
  roundedRect(cr, x0 + w * 0.02, y0 + h * 0.08, w * 0.66, h * 0.66, 6);
  fillOutline(cr, card, ink, kLine);
  cairo_move_to(cr, x0 + w * 0.68, y0 + h * 0.3);
  cairo_line_to(cr, x0 + w * 0.88, y0 + h * 0.3);
  cairo_line_to(cr, x0 + w * 0.98, y0 + h * 0.52);
  cairo_line_to(cr, x0 + w * 0.98, y0 + h * 0.74);
  cairo_line_to(cr, x0 + w * 0.68, y0 + h * 0.74);
  cairo_close_path(cr);
  fillOutline(cr, rgb(210, 90, 60), ink, kLine);
  roundedRect(cr, x0 + w * 0.76, y0 + h * 0.36, w * 0.12, h * 0.16, 3);
  fillOutline(cr, rgb(150, 190, 210), ink, 1.4);
  // A painted logo in blocky strokes.
  for (int i = 0; i < 5; ++i)
  {
    cairo_rectangle(cr, x0 + w * (0.08 + 0.11 * i), y0 + h * 0.3, w * 0.07, h * 0.2);
    setColor(cr, rgb(200, 60, 50));
    cairo_fill(cr);
  }
  for (double wx : {0.18, 0.5, 0.84})
  {
    circle(cr, x0 + w * wx, y0 + h * 0.82, h * 0.14);
    fillOutline(cr, rgb(70, 50, 30), ink, kLine);
    circle(cr, x0 + w * wx, y0 + h * 0.82, h * 0.05);
    setColor(cr, card);
    cairo_fill(cr);
  }
}

// Black Halo (level 7's boss): a matte black attack gunship, nose to the
// right, tail boom to the left. The pods, light, rotors and weak-spot
// markers are drawn by the world; this is the airframe. Variant 1: the belly
// hatch is open; variant 2: the burning wreck.
void blackHalo(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  const bool wreck = c.variant == 2;
  const Color hull = wreck ? rgb(40, 34, 32) : rgb(34, 36, 44), edge = rgb(90, 96, 116);
  // Tail boom and fin.
  cairo_move_to(cr, x0, y0 + h * 0.28);
  cairo_line_to(cr, x0 + w * 0.06, y0 + h * 0.06);
  cairo_line_to(cr, x0 + w * 0.12, y0 + h * 0.06);
  cairo_line_to(cr, x0 + w * 0.14, y0 + h * 0.36);
  cairo_line_to(cr, x0 + w * 0.42, y0 + h * 0.4);
  cairo_line_to(cr, x0 + w * 0.42, y0 + h * 0.58);
  cairo_line_to(cr, x0 + w * 0.1, y0 + h * 0.56);
  cairo_line_to(cr, x0, y0 + h * 0.5);
  cairo_close_path(cr);
  fillGradientOutline(cr, y0, y0 + h * 0.6, lighten(hull, 0.15f), hull, kInk, kLine);
  // Fuselage: an angular, stealthy body tapering to the nose.
  cairo_move_to(cr, x0 + w * 0.36, y0 + h * 0.3);
  cairo_line_to(cr, x0 + w * 0.6, y0 + h * 0.16);
  cairo_line_to(cr, x0 + w * 0.8, y0 + h * 0.22);
  cairo_line_to(cr, x0 + w * 0.98, y0 + h * 0.56);
  cairo_line_to(cr, x0 + w * 0.9, y0 + h * 0.76);
  cairo_line_to(cr, x0 + w * 0.42, y0 + h * 0.84);
  cairo_line_to(cr, x0 + w * 0.32, y0 + h * 0.64);
  cairo_close_path(cr);
  fillGradientOutline(cr, y0 + h * 0.16, y0 + h * 0.84, lighten(hull, 0.2f), hull, kInk, kLine);
  // Panel lines.
  cairo_set_line_width(cr, 1.6);
  setColor(cr, edge);
  cairo_move_to(cr, x0 + w * 0.42, y0 + h * 0.5);
  cairo_line_to(cr, x0 + w * 0.88, y0 + h * 0.48);
  cairo_move_to(cr, x0 + w * 0.56, y0 + h * 0.2);
  cairo_line_to(cr, x0 + w * 0.54, y0 + h * 0.8);
  cairo_stroke(cr);
  // Canopy: tinted red.
  cairo_move_to(cr, x0 + w * 0.66, y0 + h * 0.28);
  cairo_line_to(cr, x0 + w * 0.8, y0 + h * 0.3);
  cairo_line_to(cr, x0 + w * 0.92, y0 + h * 0.54);
  cairo_line_to(cr, x0 + w * 0.7, y0 + h * 0.52);
  cairo_close_path(cr);
  fillOutline(cr, wreck ? rgb(60, 40, 30) : rgb(200, 40, 50), kInk, 1.6);
  if (!wreck)
    radialGlow(cr, x0 + w * 0.8, y0 + h * 0.42, 30, rgb(255, 60, 60), 0.4);
  // Rotor mast.
  roundedRect(cr, x0 + w * 0.47, y0, w * 0.06, h * 0.18, 3);
  fillOutline(cr, rgb(60, 62, 72), kInk, 1.4);
  // Stub wings with the halo stripe.
  roundedRect(cr, x0 + w * 0.36, y0 + h * 0.6, w * 0.4, h * 0.1, 4);
  fillOutline(cr, darken(hull, 0.2f), kInk, 1.6);
  cairo_rectangle(cr, x0 + w * 0.4, y0 + h * 0.63, w * 0.32, h * 0.03);
  setColor(cr, rgb(255, 200, 60));
  cairo_fill(cr);
  if (c.variant == 1)
  {
    roundedRect(cr, x0 + w * 0.42, y0 + h * 0.78, w * 0.2, h * 0.12, 3);
    setColor(cr, rgb(255, 140, 60));
    cairo_fill(cr);
    radialGlow(cr, x0 + w * 0.52, y0 + h * 0.86, 40, rgb(255, 120, 40), 0.8);
  }
  // Landing skids.
  cairo_set_line_width(cr, 4.0);
  setColor(cr, rgb(70, 72, 80));
  cairo_move_to(cr, x0 + w * 0.36, y0 + h * 0.98);
  cairo_line_to(cr, x0 + w * 0.86, y0 + h * 0.98);
  cairo_move_to(cr, x0 + w * 0.44, y0 + h * 0.84);
  cairo_line_to(cr, x0 + w * 0.42, y0 + h * 0.98);
  cairo_move_to(cr, x0 + w * 0.76, y0 + h * 0.8);
  cairo_line_to(cr, x0 + w * 0.78, y0 + h * 0.98);
  cairo_stroke(cr);
  if (wreck)
  {
    radialGlow(cr, x0 + w * 0.5, y0 + h * 0.3, 60, rgb(255, 120, 30), 0.9);
    radialGlow(cr, x0 + w * 0.2, y0 + h * 0.4, 40, rgb(255, 80, 20), c.frame ? 0.8 : 0.5);
  }
  else
  {
    radialGlow(cr, x0 + w * 0.06, y0 + h * 0.08, 10, rgb(255, 40, 40), c.frame ? 0.9 : 0.3);
  }
}

// The cement mixer on Chopper Down's roof R2 (a breakable): a striped drum
// tilted on its stand. Frame 1 turns the drum's stripes a little.
void cementMixer(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  const Color orange = rgb(226, 124, 40), steel = rgb(90, 92, 100);
  // The stand: two legs, an axle and a wheel.
  strokeLimb(cr, {{x0 + w * 0.2, y0 + h}, {x0 + w * 0.42, y0 + h * 0.55}}, 8, steel, kInk, kLine);
  strokeLimb(cr, {{x0 + w * 0.82, y0 + h}, {x0 + w * 0.6, y0 + h * 0.55}}, 8, steel, kInk, kLine);
  circle(cr, x0 + w * 0.82, y0 + h * 0.9, h * 0.09);
  fillOutline(cr, rgb(30, 30, 34), kInk, kLine);
  // The drum, tilted up to the right.
  cairo_save(cr);
  cairo_translate(cr, x0 + w * 0.5, y0 + h * 0.45);
  cairo_rotate(cr, -0.35);
  cairo_move_to(cr, -w * 0.42, -h * 0.22);
  cairo_line_to(cr, w * 0.18, -h * 0.34);
  cairo_line_to(cr, w * 0.42, -h * 0.14);
  cairo_line_to(cr, w * 0.42, h * 0.14);
  cairo_line_to(cr, w * 0.18, h * 0.34);
  cairo_line_to(cr, -w * 0.42, h * 0.22);
  cairo_close_path(cr);
  fillGradientOutline(cr, -h * 0.34, h * 0.34, lighten(orange, 0.25f), darken(orange, 0.2f), kInk, kLine);
  // Spiral stripes.
  for (int k = 0; k < 4; ++k)
  {
    const double sx = -w * 0.34 + w * 0.17 * k + (c.frame ? w * 0.06 : 0.0);
    cairo_move_to(cr, sx, -h * 0.27);
    cairo_line_to(cr, sx + w * 0.08, h * 0.27);
    cairo_set_line_width(cr, 6);
    setColor(cr, darken(orange, 0.35f));
    cairo_stroke(cr);
  }
  // The mouth.
  cairo_save(cr);
  cairo_translate(cr, w * 0.42, 0);
  cairo_scale(cr, w * 0.05, h * 0.14);
  circle(cr, 0, 0, 1.0);
  cairo_restore(cr);
  fillOutline(cr, rgb(50, 46, 44), kInk, 1.6);
  cairo_restore(cr);
}

// Black Halo's gun pod (3 x 2 cells): a rounded pod with a twin barrel,
// pointing left (the gunship's nose side).
void haloPod(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  for (int k = 0; k < 2; ++k)
  {
    roundedRect(cr, x0 - w * 0.2, y0 + h * (0.3 + 0.24 * k), w * 0.4, h * 0.14, 3);
    fillOutline(cr, rgb(40, 40, 46), kInk, 1.4);
  }
  roundedRect(cr, x0 + w * 0.08, y0 + h * 0.12, w * 0.86, h * 0.76, h * 0.36);
  fillGradientOutline(cr, y0 + h * 0.12, y0 + h * 0.88, rgb(120, 126, 120), rgb(50, 56, 52), kInk, kLine);
  cairo_rectangle(cr, x0 + w * 0.3, y0 + h * 0.2, w * 0.05, h * 0.6);
  setColor(cr, rgb(255, 200, 60));
  cairo_fill(cr);
  if (c.variant == 1)
    radialGlow(cr, x0 - w * 0.15, y0 + h * 0.5, 22, rgb(255, 160, 60), 0.9);
}

// Black Halo's searchlight (2 x 2 cells): a round lamp in a cowl.
void haloLight(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  radialGlow(cr, x0 + w * 0.5, y0 + h * 0.5, w * 0.9, rgb(240, 248, 255), 0.6);
  circle(cr, x0 + w * 0.5, y0 + h * 0.5, w * 0.42);
  fillOutline(cr, rgb(60, 62, 72), kInk, kLine);
  circle(cr, x0 + w * 0.5, y0 + h * 0.5, w * 0.3);
  fillGradientOutline(cr, y0 + h * 0.2, y0 + h * 0.8, rgb(255, 255, 255), rgb(190, 220, 255), kInk, 1.2);
}

using DrawFn = void (*)(const Ctx&);

// Howler (level 8): a long-armed monkey in the canopy. Odd variants wind up
// a throw (fruit held high); variants 2-3 wear a tiny copy of Dash's jacket.
void howler(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  const Color fur = rgb(120, 76, 44), face = rgb(222, 184, 140);
  const bool throwing = c.variant % 2 == 1, jacket = c.variant >= 2;
  // The tail, curling up behind.
  strokeLimb(cr, {{x0 + w * 0.3, y0 + h * 0.8}, {x0 + w * 0.02, y0 + h * 0.7}, {x0 + w * 0.05, y0 + h * 0.3},
                   {x0 + w * 0.2, y0 + h * 0.25}}, 7, fur, kInk, kLine);
  // Legs and body.
  for (double lx : {0.32, 0.6})
  {
    roundedRect(cr, x0 + w * lx, y0 + h * 0.72, w * 0.16, h * 0.28, 5);
    fillOutline(cr, darken(fur, 0.15f), kInk, kLine);
  }
  roundedRect(cr, x0 + w * 0.25, y0 + h * 0.38, w * 0.5, h * 0.42, 14);
  if (jacket)
    fillGradientOutline(cr, y0 + h * 0.38, y0 + h * 0.8, rgb(240, 70, 60), rgb(180, 30, 40), kInk, kLine);
  else
    fillGradientOutline(cr, y0 + h * 0.38, y0 + h * 0.8, lighten(fur, 0.15f), fur, kInk, kLine);
  roundedRect(cr, x0 + w * 0.38, y0 + h * 0.5, w * 0.24, h * 0.24, 8);
  fillOutline(cr, face, kInk, 1.6); // the belly
  // Arms: one hanging, one up with the fruit when it throws.
  strokeLimb(cr, {{x0 + w * 0.3, y0 + h * 0.45}, {x0 + w * 0.16, y0 + h * 0.85}}, 6, fur, kInk, kLine);
  const double hx = x0 + w * (throwing ? 0.86 : 0.84), hy = y0 + h * (throwing ? 0.05 : 0.85);
  strokeLimb(cr, {{x0 + w * 0.7, y0 + h * 0.45}, {x0 + w * 0.82, y0 + h * 0.35}, {hx, hy}}, 6,
    jacket ? rgb(200, 40, 50) : fur, kInk, kLine);
  if (throwing)
  {
    circle(cr, hx, hy - 6, 10);
    fillOutline(cr, rgb(240, 120, 50), kInk, kLine);
  }
  // Head: round, a pale face, wide mouth (howling) every other frame.
  circle(cr, x0 + w * 0.52, y0 + h * 0.26, w * 0.2);
  fillOutline(cr, fur, kInk, kLine);
  cairo_save(cr);
  cairo_translate(cr, x0 + w * 0.56, y0 + h * 0.3);
  cairo_scale(cr, 1.0, 0.8);
  circle(cr, 0, 0, w * 0.13);
  cairo_restore(cr);
  fillOutline(cr, face, kInk, 1.6);
  for (double ex : {0.5, 0.62})
  {
    circle(cr, x0 + w * ex, y0 + h * 0.24, 3);
    setColor(cr, kInk);
    cairo_fill(cr);
  }
  const double mouth = c.frame ? 6 : 3;
  cairo_save(cr);
  cairo_translate(cr, x0 + w * 0.57, y0 + h * 0.35);
  cairo_scale(cr, 1.0, mouth / 6.0);
  circle(cr, 0, 0, 6);
  cairo_restore(cr);
  setColor(cr, rgb(90, 30, 30));
  cairo_fill(cr);
  if (jacket)
  {
    // Dash's goggles on its forehead.
    roundedRect(cr, x0 + w * 0.4, y0 + h * 0.1, w * 0.28, 8, 4);
    fillOutline(cr, rgb(120, 220, 255), kInk, 1.4);
  }
}

// Canopy Viper (level 8): coiled on its branch (variant 0, 2 x 2), or
// hanging down from it to strike (variant 1, 2 x 4).
void viper(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  const Color scales = rgb(90, 180, 70), belly = rgb(220, 230, 120);
  auto head = [&](double hx, double hy, double dir) {
    cairo_save(cr);
    cairo_translate(cr, hx, hy);
    cairo_scale(cr, dir, 1.0);
    cairo_move_to(cr, -10, -9);
    cairo_curve_to(cr, 6, -14, 18, -4, 18, 2);
    cairo_curve_to(cr, 14, 10, 0, 12, -10, 9);
    cairo_close_path(cr);
    cairo_restore(cr);
    fillOutline(cr, lighten(scales, 0.1f), kInk, kLine);
    circle(cr, hx + dir * 6, hy - 3, 3);
    setColor(cr, rgb(255, 220, 40));
    cairo_fill(cr);
    if (c.frame)
    {
      cairo_move_to(cr, hx + dir * 18, hy + 2);
      cairo_line_to(cr, hx + dir * 28, hy);
      cairo_line_to(cr, hx + dir * 32, hy - 3);
      cairo_move_to(cr, hx + dir * 28, hy);
      cairo_line_to(cr, hx + dir * 32, hy + 4);
      setColor(cr, rgb(220, 40, 60));
      cairo_set_line_width(cr, 2.0);
      cairo_stroke(cr);
    }
  };
  if (c.variant == 1)
  {
    // Hanging: the tail wrapped on the branch, the body in an S down to
    // the head at the bottom.
    strokeLimb(cr, {{x0 + w * 0.2, y0 + 4}, {x0 + w * 0.8, y0 + h * 0.15}, {x0 + w * 0.3, y0 + h * 0.4},
                     {x0 + w * 0.7, y0 + h * 0.65}, {x0 + w * 0.45, y0 + h * 0.86}}, 11, scales, kInk, kLine);
    for (int i = 0; i < 4; ++i)
    {
      circle(cr, x0 + w * (i % 2 ? 0.62 : 0.4), y0 + h * (0.2 + i * 0.17), 3);
      setColor(cr, belly);
      cairo_fill(cr);
    }
    head(x0 + w * 0.45, y0 + h * 0.9, 1.0);
    return;
  }
  // Coiled: three stacked rings, head on top.
  for (int i = 0; i < 3; ++i)
  {
    const double ry = y0 + h * (0.85 - i * 0.2), rw = w * (0.48 - i * 0.08);
    cairo_save(cr);
    cairo_translate(cr, x0 + w * 0.5, ry);
    cairo_scale(cr, 1.0, 0.4);
    circle(cr, 0, 0, rw);
    cairo_restore(cr);
    fillOutline(cr, i % 2 ? lighten(scales, 0.1f) : scales, kInk, kLine);
  }
  head(x0 + w * 0.5, y0 + h * 0.2, 1.0);
}

// Bridge Cutter (level 8): a poacher in khaki and a bandana with a machete.
// Variant 1 has the machete up for a chop.
void cutter(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  const Color khaki = rgb(170, 150, 96), skin = rgb(190, 140, 100);
  for (double lx : {0.28, 0.54})
  {
    roundedRect(cr, x0 + w * lx, y0 + h * 0.62, w * 0.18, h * 0.38, 5);
    fillOutline(cr, darken(khaki, 0.25f), kInk, kLine);
    roundedRect(cr, x0 + w * (lx - 0.03), y0 + h * 0.93, w * 0.26, h * 0.07, 3);
    fillOutline(cr, rgb(70, 50, 34), kInk, 1.6);
  }
  roundedRect(cr, x0 + w * 0.18, y0 + h * 0.3, w * 0.62, h * 0.38, 10);
  fillGradientOutline(cr, y0 + h * 0.3, y0 + h * 0.68, lighten(khaki, 0.15f), khaki, kInk, kLine);
  cairo_move_to(cr, x0 + w * 0.2, y0 + h * 0.36); // a rope coil over the shoulder
  cairo_line_to(cr, x0 + w * 0.76, y0 + h * 0.62);
  setColor(cr, rgb(200, 170, 110));
  cairo_set_line_width(cr, 5.0);
  cairo_stroke(cr);
  // Head, bandana, stubble.
  circle(cr, x0 + w * 0.5, y0 + h * 0.2, w * 0.2);
  fillOutline(cr, skin, kInk, kLine);
  cairo_rectangle(cr, x0 + w * 0.3, y0 + h * 0.07, w * 0.4, h * 0.06);
  fillOutline(cr, rgb(200, 50, 40), kInk, 1.6);
  circle(cr, x0 + w * 0.6, y0 + h * 0.19, 3);
  setColor(cr, kInk);
  cairo_fill(cr);
  // The machete arm.
  const bool up = c.variant == 1;
  const double sx = x0 + w * 0.74, sy = y0 + h * 0.38;
  const double hx = up ? x0 + w * 0.86 : x0 + w * 0.92, hy = up ? y0 + h * 0.12 : y0 + h * 0.52;
  strokeLimb(cr, {{sx, sy}, {hx, hy}}, 7, khaki, kInk, kLine);
  const double bx = up ? hx - 4 : hx + 30, by = up ? hy - 34 : hy + 4;
  cairo_move_to(cr, hx, hy);
  cairo_line_to(cr, bx, by);
  cairo_line_to(cr, bx + (up ? 12 : 4), by + (up ? 4 : 10));
  cairo_close_path(cr);
  fillOutline(cr, rgb(210, 216, 226), kInk, 1.6);
}

// Stone Guardian (level 9): a sandstone temple warrior with a jackal's
// headdress and a stone club. Variant 0 holds the club down, 1 raises it
// (the tell), 2 swings it out in front, 3 lies face down in the Snare
// Bolas' cords.
void guardian(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const Color stone = rgb(196, 160, 104), shade = rgb(140, 108, 66), band = rgb(60, 120, 150);
  if (c.variant == 3)
  {
    // Face down along the floor, the club dropped beside it.
    const double y = kM + c.h, x0 = 0, len = kM * 2 + c.w;
    roundedRect(cr, x0 + 18, y - 44, len - 60, 40, 12);
    fillGradientOutline(cr, y - 44, y - 4, lighten(stone, 0.1f), shade, kInk, kLine);
    roundedRect(cr, len - 54, y - 50, 44, 46, 10); // the head
    fillOutline(cr, stone, kInk, kLine);
    roundedRect(cr, len - 46, y - 64, 30, 18, 4); // the headdress
    fillOutline(cr, band, kInk, kLine);
    roundedRect(cr, x0 + 4, y - 18, 70, 14, 6); // the club
    fillOutline(cr, shade, kInk, kLine);
    for (int k = 0; k < 4; ++k)
      strokeLimb(cr, {{x0 + 40 + k * 32.0, y - 50}, {x0 + 52 + k * 32.0, y - 2}}, 3, rgb(150, 110, 60), kInk, 1.0);
    return;
  }
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  // Legs and kilt.
  for (double lx : {0.24, 0.56})
  {
    roundedRect(cr, x0 + w * lx, y0 + h * 0.7, w * 0.2, h * 0.3, 6);
    fillGradientOutline(cr, y0 + h * 0.7, y0 + h, stone, shade, kInk, kLine);
  }
  cairo_move_to(cr, x0 + w * 0.2, y0 + h * 0.56);
  cairo_line_to(cr, x0 + w * 0.8, y0 + h * 0.56);
  cairo_line_to(cr, x0 + w * 0.86, y0 + h * 0.76);
  cairo_line_to(cr, x0 + w * 0.14, y0 + h * 0.76);
  cairo_close_path(cr);
  fillOutline(cr, rgb(236, 220, 170), kInk, kLine);
  // The torso, broad and square, with a collar.
  roundedRect(cr, x0 + w * 0.16, y0 + h * 0.26, w * 0.68, h * 0.32, 10);
  fillGradientOutline(cr, y0 + h * 0.26, y0 + h * 0.58, lighten(stone, 0.12f), stone, kInk, kLine);
  roundedRect(cr, x0 + w * 0.24, y0 + h * 0.26, w * 0.52, h * 0.06, 4);
  fillOutline(cr, band, kInk, 1.6);
  for (int k = 0; k < 4; ++k)
  {
    cairo_rectangle(cr, x0 + w * (0.28 + k * 0.12), y0 + h * 0.27, w * 0.05, h * 0.04);
    setColor(cr, rgb(230, 190, 70));
    cairo_fill(cr);
  }
  // The back arm.
  strokeLimb(cr, {{x0 + w * 0.2, y0 + h * 0.32}, {x0 + w * 0.08, y0 + h * 0.56}}, 9, shade, kInk, kLine);
  // The head: a jackal's muzzle under a striped headdress.
  cairo_move_to(cr, x0 + w * 0.3, y0 + h * 0.06);
  cairo_line_to(cr, x0 + w * 0.7, y0 + h * 0.06);
  cairo_line_to(cr, x0 + w * 0.76, y0 + h * 0.26);
  cairo_line_to(cr, x0 + w * 0.24, y0 + h * 0.26);
  cairo_close_path(cr);
  fillOutline(cr, band, kInk, kLine);
  for (int k = 1; k < 4; ++k)
  {
    cairo_move_to(cr, x0 + w * (0.3 - k * 0.015), y0 + h * (0.06 + k * 0.05));
    cairo_line_to(cr, x0 + w * (0.7 + k * 0.015), y0 + h * (0.06 + k * 0.05));
    cairo_set_line_width(cr, 3);
    setColor(cr, rgb(230, 190, 70));
    cairo_stroke(cr);
  }
  for (double ex : {0.36, 0.56})
  {
    cairo_move_to(cr, x0 + w * ex, y0 + h * 0.06); // ears
    cairo_line_to(cr, x0 + w * (ex + 0.04), y0 - h * 0.04);
    cairo_line_to(cr, x0 + w * (ex + 0.08), y0 + h * 0.06);
    cairo_close_path(cr);
    fillOutline(cr, stone, kInk, 1.6);
  }
  cairo_move_to(cr, x0 + w * 0.6, y0 + h * 0.12);
  cairo_line_to(cr, x0 + w * 0.94, y0 + h * 0.16);
  cairo_line_to(cr, x0 + w * 0.92, y0 + h * 0.21);
  cairo_line_to(cr, x0 + w * 0.6, y0 + h * 0.22);
  cairo_close_path(cr);
  fillOutline(cr, stone, kInk, kLine);
  const double glow = c.variant > 0 ? 0.9 : 0.5;
  radialGlow(cr, x0 + w * 0.64, y0 + h * 0.14, 12, rgb(255, 60, 40), glow);
  circle(cr, x0 + w * 0.64, y0 + h * 0.14, 3.5);
  setColor(cr, rgb(255, 200, 150));
  cairo_fill(cr);
  // The club arm: down, raised over the head, or swung out front.
  double hx = x0 + w * 0.86, hy = y0 + h * 0.6, cx = hx, cy = hy + h * 0.2;
  if (c.variant == 1)
  {
    hx = x0 + w * 0.7;
    hy = y0 - h * 0.02;
    cx = x0 + w * 0.42;
    cy = y0 - h * 0.14;
  }
  else if (c.variant == 2)
  {
    hx = x0 + w * 1.04;
    hy = y0 + h * 0.36;
    cx = x0 + w * 1.5;
    cy = y0 + h * 0.4;
  }
  strokeLimb(cr, {{x0 + w * 0.8, y0 + h * 0.32}, {(x0 + w * 0.86 + hx) * 0.5, (y0 + h * 0.46 + hy) * 0.5}, {hx, hy}}, 9,
    stone, kInk, kLine);
  strokeLimb(cr, {{hx, hy}, {cx, cy}}, 14, shade, kInk, kLine);
  circle(cr, cx, cy, 13);
  fillOutline(cr, shade, kInk, kLine);
  if (c.variant == 2)
    for (int k = 0; k < 3; ++k)
      strokeLimb(cr, {{x0 + w * (0.9 + k * 0.1), y0 + h * 0.08}, {x0 + w * (1.2 + k * 0.1), y0 + h * 0.3}}, 2.5,
        rgba(255, 255, 255, 160), rgba(255, 255, 255, 0), 0.0);
}

// Dart Face (level 9): a carved face in the ceiling with a dart in its
// mouth. Variant 1: the eyes glow before it spits.
void dartFace(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  roundedRect(cr, x0 - 4, y0 - 4, w + 8, h + 8, 8);
  fillGradientOutline(cr, y0, y0 + h, rgb(210, 172, 116), rgb(150, 112, 70), kInk, kLine);
  // Brow, eyes, nose and the round mouth.
  cairo_rectangle(cr, x0 + w * 0.1, y0 + h * 0.18, w * 0.8, h * 0.08);
  setColor(cr, rgb(120, 86, 50));
  cairo_fill(cr);
  for (double ex : {0.3, 0.7})
  {
    if (c.variant == 1)
      radialGlow(cr, x0 + w * ex, y0 + h * 0.38, 14, rgb(255, 60, 30), 0.9);
    circle(cr, x0 + w * ex, y0 + h * 0.38, 5);
    setColor(cr, c.variant == 1 ? rgb(255, 210, 160) : rgb(40, 26, 18));
    cairo_fill(cr);
  }
  cairo_move_to(cr, x0 + w * 0.5, y0 + h * 0.42);
  cairo_line_to(cr, x0 + w * 0.42, y0 + h * 0.62);
  cairo_line_to(cr, x0 + w * 0.58, y0 + h * 0.62);
  cairo_close_path(cr);
  setColor(cr, rgb(130, 96, 56));
  cairo_fill(cr);
  circle(cr, x0 + w * 0.5, y0 + h * 0.8, 7);
  fillOutline(cr, rgb(30, 18, 12), kInk, 1.6);
  cairo_rectangle(cr, x0 + w * 0.5 - 1.5, y0 + h * 0.74, 3, 12);
  setColor(cr, rgb(220, 220, 200));
  cairo_fill(cr);
}

// Scarab Tide (level 9): a carpet of beetles, one every two cells, legs
// scuttling on alternate frames. Variant 1 (the carrier) glows green.
void scarabs(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const bool carrier = c.variant == 1;
  const Color shell = carrier ? rgb(90, 200, 60) : rgb(40, 110, 110), sheen = carrier ? rgb(200, 255, 140) : rgb(120, 220, 210);
  const double y = kM + c.h;
  if (carrier)
    radialGlow(cr, kM + c.w * 0.5, y - 10, c.w * 0.6, rgb(120, 255, 80), 0.25);
  int k = 0;
  for (double x = kM + 4; x < kM + c.w - 8; x += 26, ++k)
  {
    const double bx = x + ((k % 2) ? 6 : 0), by = y - 14 - ((k % 3) == 1 ? 6 : 0);
    for (int l = -1; l <= 1; ++l)
    {
      const double sway = (c.frame + k) % 2 ? 4 : -4;
      strokeLimb(cr, {{bx + 12 + l * 6, by + 4}, {bx + 12 + l * 9 + sway, by + 14}}, 1.6, kInk, kInk, 0.0);
    }
    cairo_save(cr);
    cairo_translate(cr, bx + 12, by);
    cairo_scale(cr, 1.0, 0.7);
    circle(cr, 0, 0, 12);
    cairo_restore(cr);
    fillGradientOutline(cr, by - 9, by + 9, sheen, shell, kInk, 1.8);
    cairo_move_to(cr, bx + 12, by - 8);
    cairo_line_to(cr, bx + 12, by + 8);
    cairo_set_line_width(cr, 1.4);
    setColor(cr, kInk);
    cairo_stroke(cr);
    circle(cr, bx + 24, by, 4);
    fillOutline(cr, darken(shell, 0.4f), kInk, 1.2);
  }
}

// Treasure Hunter (Trapmaster): a hooded cultist with a sack over his
// shoulder and a lantern, walking to the idol.
void treasureHunter(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  const double step = c.frame ? 0.06 : -0.06;
  strokeLimb(cr, {{x0 + w * 0.42, y0 + h * 0.7}, {x0 + w * (0.36 + step), y0 + h}}, 8, rgb(70, 50, 40), kInk, kLine);
  strokeLimb(cr, {{x0 + w * 0.58, y0 + h * 0.7}, {x0 + w * (0.64 - step), y0 + h}}, 8, rgb(70, 50, 40), kInk, kLine);
  // The sack on his back.
  circle(cr, x0 + w * 0.2, y0 + h * 0.42, w * 0.22);
  fillOutline(cr, rgb(170, 140, 90), kInk, kLine);
  // The robe.
  cairo_move_to(cr, x0 + w * 0.34, y0 + h * 0.24);
  cairo_line_to(cr, x0 + w * 0.68, y0 + h * 0.24);
  cairo_line_to(cr, x0 + w * 0.8, y0 + h * 0.78);
  cairo_line_to(cr, x0 + w * 0.22, y0 + h * 0.78);
  cairo_close_path(cr);
  fillGradientOutline(cr, y0 + h * 0.24, y0 + h * 0.78, rgb(150, 40, 50), rgb(90, 20, 30), kInk, kLine);
  // The hood, a dark face and two eyes.
  circle(cr, x0 + w * 0.52, y0 + h * 0.17, w * 0.2);
  fillOutline(cr, rgb(130, 34, 44), kInk, kLine);
  circle(cr, x0 + w * 0.58, y0 + h * 0.18, w * 0.12);
  setColor(cr, rgb(30, 20, 24));
  cairo_fill(cr);
  for (double ex : {0.55, 0.64})
  {
    circle(cr, x0 + w * ex, y0 + h * 0.17, 2.5);
    setColor(cr, rgb(255, 220, 120));
    cairo_fill(cr);
  }
  // The lantern out in front.
  strokeLimb(cr, {{x0 + w * 0.62, y0 + h * 0.34}, {x0 + w * 0.9, y0 + h * 0.48}}, 6, rgb(150, 40, 50), kInk, kLine);
  radialGlow(cr, x0 + w * 0.94, y0 + h * 0.56, 20, rgb(255, 190, 80), 0.7);
  roundedRect(cr, x0 + w * 0.88, y0 + h * 0.5, w * 0.13, h * 0.1, 3);
  fillOutline(cr, rgb(255, 220, 120), kInk, 1.6);
}

// Shade Wraith (level 10): a tattered shadow with two cold eyes. Variant 1
// smokes at the edges (it is close, and hisses).
void wraith(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  const double sway = c.frame ? 4.0 : -4.0;
  if (c.variant == 1)
    for (int k = 0; k < 7; ++k)
    {
      circle(cr, x0 + w * (0.1 + 0.13 * k), y0 + h * (0.2 + 0.11 * ((k * 3) % 7)), 8 + (k % 3) * 3);
      setColor(cr, rgba(40, 20, 60, 110));
      cairo_fill(cr);
    }
  // The shroud, ragged at the hem.
  cairo_move_to(cr, x0 + w * 0.5, y0 - 4);
  cairo_curve_to(cr, x0 + w * 1.05, y0 + h * 0.05, x0 + w * 0.95, y0 + h * 0.6, x0 + w * 0.92 + sway, y0 + h);
  for (int k = 0; k < 5; ++k)
  {
    const double fx = 0.92 - 0.2 * (k + 1);
    cairo_line_to(cr, x0 + w * (fx + 0.1) + sway * 0.5, y0 + h * 0.84);
    cairo_line_to(cr, x0 + w * fx + (k % 2 ? sway : -sway), y0 + h + (k % 2 ? -2 : 4));
  }
  cairo_curve_to(cr, x0 + w * 0.02, y0 + h * 0.6, x0 - w * 0.05, y0 + h * 0.05, x0 + w * 0.5, y0 - 4);
  cairo_close_path(cr);
  fillGradientOutline(cr, y0, y0 + h, rgba(70, 50, 100, 235), rgba(20, 12, 34, 200), rgb(120, 100, 170), kLine);
  // The hood's hollow and the eyes.
  cairo_save(cr);
  cairo_translate(cr, x0 + w * 0.6, y0 + h * 0.2);
  cairo_scale(cr, 1.0, 0.8);
  circle(cr, 0, 0, w * 0.24);
  cairo_restore(cr);
  setColor(cr, rgb(10, 6, 18));
  cairo_fill(cr);
  for (double ex : {0.54, 0.7})
  {
    radialGlow(cr, x0 + w * ex, y0 + h * 0.2, 10, rgb(150, 230, 255), 0.8);
    circle(cr, x0 + w * ex, y0 + h * 0.2, 3);
    setColor(cr, rgb(220, 250, 255));
    cairo_fill(cr);
  }
  // A reaching hand.
  strokeLimb(cr, {{x0 + w * 0.7, y0 + h * 0.42}, {x0 + w * 1.0, y0 + h * 0.5}, {x0 + w * 1.12, y0 + h * 0.46}}, 5,
    rgba(60, 44, 90, 230), rgb(120, 100, 170), 1.4);
}

// Mirror Monk (level 10): a robed monk behind a round mirror shield.
// Variant 1 plants the shield (the bash coming), 2 stands frozen in a
// sunbeam with the shield blazing, 3 has it lowered after a bash.
void monk(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  const double step = c.variant == 0 ? (c.frame ? 0.05 : -0.05) : 0.0;
  strokeLimb(cr, {{x0 + w * 0.4, y0 + h * 0.72}, {x0 + w * (0.36 + step), y0 + h}}, 8, rgb(120, 80, 50), kInk, kLine);
  strokeLimb(cr, {{x0 + w * 0.56, y0 + h * 0.72}, {x0 + w * (0.6 - step), y0 + h}}, 8, rgb(120, 80, 50), kInk, kLine);
  // The robe and the rope belt.
  cairo_move_to(cr, x0 + w * 0.3, y0 + h * 0.24);
  cairo_line_to(cr, x0 + w * 0.64, y0 + h * 0.24);
  cairo_line_to(cr, x0 + w * 0.78, y0 + h * 0.86);
  cairo_line_to(cr, x0 + w * 0.16, y0 + h * 0.86);
  cairo_close_path(cr);
  fillGradientOutline(cr, y0 + h * 0.24, y0 + h * 0.86, rgb(236, 226, 206), rgb(190, 174, 150), kInk, kLine);
  cairo_rectangle(cr, x0 + w * 0.22, y0 + h * 0.52, w * 0.5, h * 0.04);
  setColor(cr, rgb(200, 150, 60));
  cairo_fill(cr);
  // The shaved head and a calm face.
  circle(cr, x0 + w * 0.46, y0 + h * 0.15, w * 0.17);
  fillOutline(cr, rgb(222, 178, 136), kInk, kLine);
  cairo_rectangle(cr, x0 + w * 0.5, y0 + h * 0.14, w * 0.1, 2.5);
  setColor(cr, kInk);
  cairo_fill(cr);
  // The shield: a disc of polished bronze in front of him (lowered to his
  // side after a bash, variant 3).
  const double sx = x0 + w * (c.variant == 3 ? 0.62 : (c.variant == 0 ? 0.84 : 0.94)),
               sy = y0 + h * (c.variant == 3 ? 0.72 : 0.5);
  if (c.variant == 2)
    radialGlow(cr, sx, sy, w * 0.9, rgb(255, 230, 140), 0.9);
  strokeLimb(cr, {{x0 + w * 0.56, y0 + h * 0.34}, {sx - 6, sy}}, 7, rgb(236, 226, 206), kInk, kLine);
  cairo_save(cr);
  cairo_translate(cr, sx, sy);
  cairo_scale(cr, 0.42, 1.0);
  circle(cr, 0, 0, h * 0.28);
  cairo_restore(cr);
  fillGradientOutline(cr, sy - h * 0.28, sy + h * 0.28, c.variant == 2 ? rgb(255, 255, 230) : rgb(250, 230, 170),
    rgb(190, 140, 60), kInk, kLine);
  cairo_move_to(cr, sx - 2, sy - h * 0.2);
  cairo_line_to(cr, sx + 3, sy - h * 0.05);
  cairo_set_line_width(cr, 3);
  setColor(cr, rgba(255, 255, 255, 200));
  cairo_stroke(cr);
}

// Sun Moth (level 10): a small pale moth with dusty gold wings.
void sunMoth(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double cx = kM + c.w * 0.5, cy = kM + c.h * 0.5;
  const double flap = c.frame ? 0.45 : 1.0;
  radialGlow(cr, cx, cy, c.w * 0.7, rgb(255, 230, 150), 0.35);
  for (int side : {-1, 1})
  {
    cairo_save(cr);
    cairo_translate(cr, cx, cy);
    cairo_scale(cr, side * flap, 1.0);
    cairo_move_to(cr, 0, -2);
    cairo_curve_to(cr, c.w * 0.3, -c.h * 0.6, c.w * 0.62, -c.h * 0.3, c.w * 0.5, c.h * 0.05);
    cairo_curve_to(cr, c.w * 0.42, c.h * 0.4, c.w * 0.12, c.h * 0.36, 0, c.h * 0.1);
    cairo_close_path(cr);
    cairo_restore(cr);
    fillOutline(cr, rgb(250, 226, 160), rgb(160, 120, 60), 1.6);
  }
  cairo_save(cr);
  cairo_translate(cr, cx, cy);
  cairo_scale(cr, 0.35, 1.0);
  circle(cr, 0, 0, c.h * 0.3);
  cairo_restore(cr);
  fillOutline(cr, rgb(120, 90, 50), kInk, 1.4);
}

// A mine cart (level 11): riveted iron tub on four wheels. Variant 1 has a
// big 42 painted on its side (cart CT1).
void mineCart(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  const Color iron = rgb(110, 92, 84), rust = rgb(160, 84, 44);
  // The tub, wider at the top.
  cairo_move_to(cr, x0 + 2, y0 + h * 0.08);
  cairo_line_to(cr, x0 + w - 2, y0 + h * 0.08);
  cairo_line_to(cr, x0 + w * 0.9, y0 + h * 0.78);
  cairo_line_to(cr, x0 + w * 0.1, y0 + h * 0.78);
  cairo_close_path(cr);
  fillGradientOutline(cr, y0, y0 + h * 0.8, lighten(rust, 0.15f), darken(iron, 0.25f), kInk, kLine);
  // The rim and two bands of rivets.
  roundedRect(cr, x0 - 2, y0 + h * 0.02, w + 4, h * 0.12, 4);
  fillOutline(cr, rgb(90, 84, 90), kInk, kLine);
  for (double band : {0.32, 0.6})
  {
    cairo_move_to(cr, x0 + w * (0.06 + band * 0.06), y0 + h * band);
    cairo_line_to(cr, x0 + w * (0.94 - band * 0.06), y0 + h * band);
    cairo_set_line_width(cr, 5);
    setColor(cr, rgb(80, 72, 76));
    cairo_stroke(cr);
    for (double rx = 0.14; rx < 0.9; rx += 0.12)
    {
      circle(cr, x0 + w * rx, y0 + h * band, 2.2);
      setColor(cr, rgb(200, 190, 180));
      cairo_fill(cr);
    }
  }
  if (c.variant == 1)
  {
    selectGameFont(cr);
    cairo_set_font_size(cr, h * 0.42);
    cairo_move_to(cr, x0 + w * 0.36, y0 + h * 0.62);
    setColor(cr, rgb(255, 214, 70));
    cairo_show_text(cr, "42");
  }
  // Wheels.
  for (double wx : {0.24, 0.76})
  {
    circle(cr, x0 + w * wx, y0 + h * 0.84, h * 0.16);
    fillOutline(cr, rgb(60, 56, 62), kInk, kLine);
    circle(cr, x0 + w * wx, y0 + h * 0.84, h * 0.05);
    setColor(cr, rgb(170, 160, 150));
    cairo_fill(cr);
  }
}

// Cart Bandit (level 11): a masked outlaw in his own cart, pistol drawn.
// Variant 1 raises the pistol (about to shoot).
void cartBandit(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  const double cartTop = y0 + h * 0.42;
  // Him, from the waist up.
  cairo_move_to(cr, x0 + w * 0.32, y0 + h * 0.2);
  cairo_line_to(cr, x0 + w * 0.66, y0 + h * 0.2);
  cairo_line_to(cr, x0 + w * 0.7, cartTop + 10);
  cairo_line_to(cr, x0 + w * 0.28, cartTop + 10);
  cairo_close_path(cr);
  fillGradientOutline(cr, y0 + h * 0.2, cartTop, rgb(150, 60, 50), rgb(90, 36, 30), kInk, kLine);
  circle(cr, x0 + w * 0.5, y0 + h * 0.12, w * 0.12);
  fillOutline(cr, rgb(214, 170, 130), kInk, kLine);
  // Bandana over the face and a wide hat.
  cairo_rectangle(cr, x0 + w * 0.38, y0 + h * 0.12, w * 0.24, h * 0.05);
  setColor(cr, rgb(200, 40, 50));
  cairo_fill(cr);
  roundedRect(cr, x0 + w * 0.26, y0 + h * 0.02, w * 0.48, h * 0.04, 3);
  fillOutline(cr, rgb(80, 56, 40), kInk, kLine);
  roundedRect(cr, x0 + w * 0.38, y0 - h * 0.04, w * 0.24, h * 0.08, 5);
  fillOutline(cr, rgb(80, 56, 40), kInk, kLine);
  cairo_rectangle(cr, x0 + w * 0.44, y0 + h * 0.08, w * 0.04, 3);
  cairo_rectangle(cr, x0 + w * 0.54, y0 + h * 0.08, w * 0.04, 3);
  setColor(cr, kInk);
  cairo_fill(cr);
  // The pistol arm.
  const double gy = c.variant == 1 ? y0 + h * 0.1 : y0 + h * 0.3;
  strokeLimb(cr, {{x0 + w * 0.62, y0 + h * 0.26}, {x0 + w * 0.82, gy}}, 7, rgb(150, 60, 50), kInk, kLine);
  cairo_rectangle(cr, x0 + w * 0.8, gy - 5, w * 0.16, 7);
  fillOutline(cr, rgb(60, 60, 70), kInk, 1.6);
  if (c.variant == 1)
    radialGlow(cr, x0 + w * 0.98, gy - 2, 12, rgb(255, 240, 160), 0.9);
  // His cart.
  cairo_move_to(cr, x0, cartTop);
  cairo_line_to(cr, x0 + w, cartTop);
  cairo_line_to(cr, x0 + w * 0.92, y0 + h * 0.86);
  cairo_line_to(cr, x0 + w * 0.08, y0 + h * 0.86);
  cairo_close_path(cr);
  fillGradientOutline(cr, cartTop, y0 + h, rgb(120, 110, 100), rgb(60, 54, 58), kInk, kLine);
  cairo_rectangle(cr, x0 + w * 0.1, cartTop + h * 0.16, w * 0.8, 4);
  setColor(cr, rgb(200, 160, 60));
  cairo_fill(cr);
  for (double wx : {0.24, 0.76})
  {
    circle(cr, x0 + w * wx, y0 + h * 0.9, h * 0.08);
    fillOutline(cr, rgb(50, 46, 52), kInk, kLine);
  }
}

// Cave bat (level 11): leathery wings, frame 0 up and 1 down.
void caveBat(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double cx = kM + c.w * 0.5, cy = kM + c.h * 0.5;
  const double lift = c.frame ? 0.5 : -0.6;
  for (int side : {-1, 1})
  {
    cairo_move_to(cr, cx, cy);
    cairo_curve_to(cr, cx + side * c.w * 0.3, cy + lift * c.h * 0.6, cx + side * c.w * 0.6, cy + lift * c.h * 0.7,
      cx + side * c.w * 0.75, cy + lift * c.h * 0.2);
    cairo_line_to(cr, cx + side * c.w * 0.5, cy + c.h * 0.12);
    cairo_line_to(cr, cx + side * c.w * 0.32, cy + c.h * 0.02);
    cairo_line_to(cr, cx + side * c.w * 0.18, cy + c.h * 0.16);
    cairo_close_path(cr);
    fillOutline(cr, rgb(70, 50, 70), kInk, 1.6);
  }
  cairo_save(cr);
  cairo_translate(cr, cx, cy);
  cairo_scale(cr, 0.7, 1.0);
  circle(cr, 0, 0, c.h * 0.24);
  cairo_restore(cr);
  fillOutline(cr, rgb(90, 64, 84), kInk, 1.6);
  for (int side : {-1, 1})
  {
    circle(cr, cx + side * c.w * 0.07, cy - c.h * 0.06, 2.4);
    setColor(cr, rgb(255, 80, 60));
    cairo_fill(cr);
  }
}

// Rock Mole (level 11): a burly digger with a miner's lamp and big claws.
// Variant 1 holds a rock up to throw.
void rockMole(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  cairo_save(cr);
  cairo_translate(cr, x0 + w * 0.5, y0 + h * 0.58);
  cairo_scale(cr, 1.0, 0.86);
  circle(cr, 0, 0, w * 0.44);
  cairo_restore(cr);
  fillGradientOutline(cr, y0 + h * 0.1, y0 + h, rgb(140, 110, 90), rgb(84, 62, 50), kInk, kLine);
  // Snout and nose.
  circle(cr, x0 + w * 0.82, y0 + h * 0.5, w * 0.12);
  fillOutline(cr, rgb(230, 170, 160), kInk, kLine);
  circle(cr, x0 + w * 0.92, y0 + h * 0.48, w * 0.05);
  setColor(cr, rgb(220, 90, 110));
  cairo_fill(cr);
  // Squinting eye and the helmet lamp.
  cairo_rectangle(cr, x0 + w * 0.6, y0 + h * 0.36, w * 0.1, 3);
  setColor(cr, kInk);
  cairo_fill(cr);
  cairo_arc(cr, x0 + w * 0.5, y0 + h * 0.28, w * 0.3, kPi, 2 * kPi);
  fillOutline(cr, rgb(240, 200, 60), kInk, kLine);
  radialGlow(cr, x0 + w * 0.7, y0 + h * 0.14, 16, rgb(255, 250, 200), 0.8);
  // Claws.
  const double ry = c.variant == 1 ? y0 + h * 0.02 : y0 + h * 0.84;
  for (int k = 0; k < 3; ++k)
    strokeLimb(cr, {{x0 + w * 0.7, y0 + h * 0.7}, {x0 + w * (0.86 + 0.04 * k), ry + k * 4.0}}, 4, rgb(240, 230, 210),
      kInk, 1.2);
  if (c.variant == 1)
  {
    circle(cr, x0 + w * 0.92, y0 + h * 0.0, w * 0.14);
    fillOutline(cr, rgb(130, 120, 110), kInk, kLine);
  }
}

// A Blasting Cap: a red stick with a spitting fuse. Variant 1 is the blink.
void blastingCap(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  roundedRect(cr, x0 + w * 0.1, y0 + h * 0.25, w * 0.8, h * 0.75, 4);
  fillGradientOutline(cr, y0, y0 + h, c.variant ? rgb(255, 200, 180) : rgb(230, 60, 50), rgb(140, 30, 30), kInk, 1.8);
  cairo_rectangle(cr, x0 + w * 0.1, y0 + h * 0.55, w * 0.8, 3);
  setColor(cr, rgb(250, 230, 200));
  cairo_fill(cr);
  strokeLimb(cr, {{x0 + w * 0.5, y0 + h * 0.25}, {x0 + w * 0.65, y0 - h * 0.1}}, 2.5, rgb(90, 80, 70), kInk, 1.0);
  radialGlow(cr, x0 + w * 0.68, y0 - h * 0.14, 9, rgb(255, 230, 120), 1.0);
}

// Pinball Mine's ball: a gold nugget with the runner's colour stripe.
void pinBall(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double cx = kM + c.w * 0.5, cy = kM + c.h * 0.5, rad = c.w * 0.48;
  const Color stripe = c.variant == 0 ? rgb(0, 220, 255) : (c.variant == 1 ? rgb(255, 120, 40) : rgb(220, 90, 255));
  circle(cr, cx, cy, rad);
  fillGradientOutline(cr, cy - rad, cy + rad, rgb(255, 240, 170), rgb(200, 140, 30), kInk, kLine);
  cairo_save(cr);
  circle(cr, cx, cy, rad);
  cairo_clip(cr);
  cairo_rectangle(cr, cx - rad, cy - rad * 0.2, rad * 2, rad * 0.4);
  setColor(cr, stripe);
  cairo_fill(cr);
  cairo_restore(cr);
  circle(cr, cx - rad * 0.35, cy - rad * 0.4, rad * 0.22);
  setColor(cr, rgba(255, 255, 255, 200));
  cairo_fill(cr);
}

// A Magma Toad: a squat basalt-crusted toad, lava glowing through the
// cracks. Variant 1 is the leap, legs out.
void magmaToad(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  const bool leap = c.variant == 1;
  // Back legs.
  if (leap)
    strokeLimb(cr, {{x0 + w * 0.3, y0 + h * 0.65}, {x0 + w * 0.05, y0 + h * 0.98}}, 9, rgb(70, 60, 62), kInk, 1.4);
  else
  {
    cairo_save(cr);
    cairo_translate(cr, x0 + w * 0.22, y0 + h * 0.82);
    cairo_scale(cr, 1.0, 0.6);
    circle(cr, 0, 0, w * 0.2);
    cairo_restore(cr);
    fillOutline(cr, rgb(70, 60, 62), kInk, kLine);
  }
  // Body.
  cairo_save(cr);
  cairo_translate(cr, x0 + w * 0.5, y0 + h * (leap ? 0.55 : 0.62));
  cairo_scale(cr, 1.0, 0.72);
  circle(cr, 0, 0, w * 0.42);
  cairo_restore(cr);
  fillGradientOutline(cr, y0 + h * 0.2, y0 + h, rgb(96, 86, 88), rgb(44, 38, 42), kInk, kLine);
  // Glowing cracks.
  cairo_move_to(cr, x0 + w * 0.3, y0 + h * 0.5);
  cairo_line_to(cr, x0 + w * 0.42, y0 + h * 0.62);
  cairo_line_to(cr, x0 + w * 0.36, y0 + h * 0.76);
  cairo_move_to(cr, x0 + w * 0.6, y0 + h * 0.48);
  cairo_line_to(cr, x0 + w * 0.56, y0 + h * 0.66);
  cairo_set_line_width(cr, 3.0);
  setColor(cr, rgb(255, 150, 50));
  cairo_stroke(cr);
  radialGlow(cr, x0 + w * 0.45, y0 + h * 0.62, 26, rgb(255, 120, 40), 0.35);
  // Belly and mouth.
  cairo_move_to(cr, x0 + w * 0.62, y0 + h * 0.66);
  cairo_curve_to(cr, x0 + w * 0.75, y0 + h * 0.74, x0 + w * 0.88, y0 + h * 0.7, x0 + w * 0.94, y0 + h * 0.6);
  cairo_set_line_width(cr, 3.0);
  setColor(cr, kInk);
  cairo_stroke(cr);
  // The eye on top, glowing.
  circle(cr, x0 + w * 0.72, y0 + h * (leap ? 0.3 : 0.38), w * 0.11);
  fillOutline(cr, rgb(255, 210, 90), kInk, kLine);
  circle(cr, x0 + w * 0.75, y0 + h * (leap ? 0.3 : 0.38), w * 0.04);
  setColor(cr, kInk);
  cairo_fill(cr);
  // Front leg.
  strokeLimb(cr, {{x0 + w * 0.68, y0 + h * 0.75}, {x0 + w * (leap ? 0.9 : 0.74), y0 + h * (leap ? 0.9 : 0.98)}}, 6,
    rgb(80, 70, 72), kInk, 1.2);
}

// An Ember Wisp: a flame with a hot core. Variant 1 has the green carrier core.
void emberWisp(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double cx = kM + c.w * 0.5, cy = kM + c.h * 0.55, rad = c.w * 0.36;
  radialGlow(cr, cx, cy, rad * 2.2, rgb(255, 130, 40), 0.6);
  cairo_move_to(cr, cx - rad, cy);
  cairo_curve_to(cr, cx - rad, cy - rad * 1.2, cx - rad * 0.2, cy - rad * 1.4, cx, cy - rad * (c.frame ? 1.9 : 1.7));
  cairo_curve_to(cr, cx + rad * 0.3, cy - rad * 1.3, cx + rad, cy - rad * 1.1, cx + rad, cy);
  cairo_arc(cr, cx, cy, rad, 0, kPi);
  cairo_close_path(cr);
  fillGradientOutline(cr, cy - rad * 1.8, cy + rad, rgb(255, 230, 120), rgb(255, 90, 30), rgb(150, 40, 10), 1.6);
  circle(cr, cx, cy + rad * 0.1, rad * 0.45);
  setColor(cr, c.variant == 1 ? rgb(140, 255, 70) : rgb(255, 255, 220));
  cairo_fill(cr);
  // Two dark eyes.
  circle(cr, cx - rad * 0.3, cy - rad * 0.25, rad * 0.12);
  circle(cr, cx + rad * 0.3, cy - rad * 0.25, rad * 0.12);
  setColor(cr, rgb(90, 20, 10));
  cairo_fill(cr);
}

// A Basalt Crab: a black shell plated like basalt columns, glowing seams.
// Variants: 0 walking, 1 claws open, 2 flipped belly up; +3 with a party hat.
void basaltCrab(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  const int pose = c.variant % 3;
  const bool hat = c.variant >= 3;
  if (pose == 2)
  {
    // Belly up: pale underside, legs waving.
    for (int k = 0; k < 4; ++k)
      strokeLimb(cr,
        {{x0 + w * (0.25 + 0.16 * k), y0 + h * 0.5},
          {x0 + w * (0.2 + 0.16 * k) + (c.frame ? 4.0 : -4.0), y0 + h * 0.12}},
        4, rgb(80, 70, 72), kInk, 1.0);
    cairo_save(cr);
    cairo_translate(cr, x0 + w * 0.5, y0 + h * 0.72);
    cairo_scale(cr, 1.0, 0.45);
    circle(cr, 0, 0, w * 0.42);
    cairo_restore(cr);
    fillGradientOutline(cr, y0 + h * 0.5, y0 + h, rgb(220, 180, 150), rgb(160, 110, 90), kInk, kLine);
    return;
  }
  // Legs.
  for (int k = 0; k < 3; ++k)
  {
    strokeLimb(cr, {{x0 + w * (0.3 + 0.1 * k), y0 + h * 0.7}, {x0 + w * (0.18 + 0.1 * k), y0 + h * 0.98}}, 4,
      rgb(80, 70, 72), kInk, 1.0);
    strokeLimb(cr, {{x0 + w * (0.5 + 0.1 * k), y0 + h * 0.7}, {x0 + w * (0.58 + 0.1 * k), y0 + h * 0.98}}, 4,
      rgb(80, 70, 72), kInk, 1.0);
  }
  // Shell.
  cairo_save(cr);
  cairo_translate(cr, x0 + w * 0.46, y0 + h * 0.6);
  cairo_scale(cr, 1.0, 0.55);
  circle(cr, 0, 0, w * 0.36);
  cairo_restore(cr);
  fillGradientOutline(cr, y0 + h * 0.3, y0 + h * 0.85, rgb(90, 84, 90), rgb(34, 30, 36), kInk, kLine);
  for (int k = 0; k < 3; ++k)
  {
    const double sx = x0 + w * (0.3 + 0.13 * k);
    cairo_move_to(cr, sx, y0 + h * 0.45);
    cairo_line_to(cr, sx + 4, y0 + h * 0.75);
  }
  cairo_set_line_width(cr, 2.5);
  setColor(cr, rgb(255, 130, 50));
  cairo_stroke(cr);
  // Eyes on stalks.
  for (int k = 0; k < 2; ++k)
  {
    const double ex = x0 + w * (0.62 + 0.08 * k);
    strokeLimb(cr, {{ex, y0 + h * 0.42}, {ex + 2, y0 + h * 0.24}}, 3, rgb(70, 60, 62), kInk, 1.0);
    circle(cr, ex + 2, y0 + h * 0.22, 5);
    fillOutline(cr, rgb(255, 210, 90), kInk, 1.2);
  }
  // The big claw out front.
  const bool open = pose == 1;
  strokeLimb(cr, {{x0 + w * 0.74, y0 + h * 0.62}, {x0 + w * 0.86, y0 + h * 0.5}}, 7, rgb(80, 70, 72), kInk, 1.2);
  cairo_move_to(cr, x0 + w * 0.84, y0 + h * 0.52);
  cairo_line_to(cr, x0 + w * 1.0, y0 + h * (open ? 0.24 : 0.4));
  cairo_line_to(cr, x0 + w * 0.96, y0 + h * 0.5);
  cairo_close_path(cr);
  fillOutline(cr, rgb(110, 100, 104), kInk, kLine);
  cairo_move_to(cr, x0 + w * 0.84, y0 + h * 0.56);
  cairo_line_to(cr, x0 + w * 1.0, y0 + h * (open ? 0.78 : 0.6));
  cairo_line_to(cr, x0 + w * 0.92, y0 + h * 0.6);
  cairo_close_path(cr);
  fillOutline(cr, rgb(110, 100, 104), kInk, kLine);
  if (hat)
  {
    cairo_move_to(cr, x0 + w * 0.36, y0 + h * 0.34);
    cairo_line_to(cr, x0 + w * 0.46, y0 + h * 0.0);
    cairo_line_to(cr, x0 + w * 0.56, y0 + h * 0.34);
    cairo_close_path(cr);
    fillOutline(cr, rgb(255, 90, 160), kInk, kLine);
    for (int k = 0; k < 3; ++k)
    {
      circle(cr, x0 + w * (0.42 + 0.04 * k), y0 + h * (0.26 - 0.08 * k), 2.5);
      setColor(cr, k % 2 ? rgb(255, 230, 80) : rgb(90, 220, 255));
      cairo_fill(cr);
    }
    circle(cr, x0 + w * 0.46, y0 + h * 0.0, 4);
    setColor(cr, rgb(255, 255, 255));
    cairo_fill(cr);
  }
}

// Boulder (level 13): a carved temple ball, lit from above. The carvings
// that turn as it rolls are drawn over it at run time (world_boulder.cpp).
void boulder(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double cx = kM + c.w * 0.5, cy = kM + c.h * 0.5, r = c.w * 0.5 - 4;
  circle(cr, cx, cy, r);
  cairo_pattern_t* g = cairo_pattern_create_radial(cx - r * 0.35, cy - r * 0.4, r * 0.1, cx, cy, r);
  cairo_pattern_add_color_stop_rgb(g, 0.0, 0.86, 0.74, 0.56);
  cairo_pattern_add_color_stop_rgb(g, 0.6, 0.6, 0.47, 0.33);
  cairo_pattern_add_color_stop_rgb(g, 1.0, 0.3, 0.22, 0.15);
  cairo_set_source(cr, g);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(g);
  setColor(cr, kInk);
  cairo_set_line_width(cr, kLine * 1.5);
  cairo_stroke(cr);
  // Pits and chips in the stone.
  for (int i = 0; i < 26; ++i)
  {
    const double a = double(i) * 2.399, d = r * std::sqrt(double(i % 13) / 13.0) * 0.9;
    circle(cr, cx + std::cos(a) * d, cy + std::sin(a) * d, 3 + i % 4);
    setColor(cr, rgba(50, 34, 20, 70));
    cairo_fill(cr);
  }
  // A band of glyphs round its belly.
  cairo_arc(cr, cx, cy, r * 0.82, 0, 2 * kPi);
  setColor(cr, rgba(60, 40, 24, 90));
  cairo_set_line_width(cr, 6.0);
  cairo_stroke(cr);
}

// Spear Runner (level 13): a temple runner in a feathered headband with a
// spear. Variant 1 has turned to throw, the spear up.
void spearRunner(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  const Color cloth = rgb(200, 80, 50), skin = rgb(170, 110, 70);
  for (double lx : {0.3, 0.52})
  {
    roundedRect(cr, x0 + w * lx, y0 + h * 0.62, w * 0.16, h * 0.36, 5);
    fillOutline(cr, skin, kInk, kLine);
  }
  roundedRect(cr, x0 + w * 0.22, y0 + h * 0.3, w * 0.56, h * 0.36, 10);
  fillGradientOutline(cr, y0 + h * 0.3, y0 + h * 0.66, lighten(skin, 0.1f), skin, kInk, kLine);
  roundedRect(cr, x0 + w * 0.2, y0 + h * 0.56, w * 0.6, h * 0.14, 4); // the loincloth
  fillOutline(cr, cloth, kInk, 1.6);
  circle(cr, x0 + w * 0.5, y0 + h * 0.2, w * 0.19);
  fillOutline(cr, skin, kInk, kLine);
  cairo_rectangle(cr, x0 + w * 0.3, y0 + h * 0.1, w * 0.4, h * 0.05);
  fillOutline(cr, rgb(240, 200, 60), kInk, 1.4);
  // Feathers.
  for (int k = 0; k < 3; ++k)
  {
    cairo_move_to(cr, x0 + w * (0.36 + k * 0.1), y0 + h * 0.1);
    cairo_line_to(cr, x0 + w * (0.3 + k * 0.12), y0 + h * 0.0);
    setColor(cr, k == 1 ? rgb(60, 170, 90) : rgb(220, 60, 50));
    cairo_set_line_width(cr, 5.0);
    cairo_stroke(cr);
  }
  circle(cr, x0 + w * 0.6, y0 + h * 0.19, 3);
  setColor(cr, kInk);
  cairo_fill(cr);
  // The spear: carried level, or raised to throw.
  const bool up = c.variant == 1;
  const double sx = x0 + w * 0.7, sy = y0 + h * 0.38;
  const double hx = up ? x0 + w * 0.7 : x0 + w * 0.86, hy = up ? y0 + h * 0.12 : y0 + h * 0.46;
  strokeLimb(cr, {{sx, sy}, {hx, hy}}, 6, skin, kInk, kLine);
  const double ax = up ? hx - 60 : hx - 70, ay = up ? hy + 6 : hy;
  const double bx = up ? hx + 50 : hx + 40, by = up ? hy - 8 : hy;
  cairo_move_to(cr, ax, ay);
  cairo_line_to(cr, bx, by);
  setColor(cr, rgb(120, 80, 40));
  cairo_set_line_width(cr, 5.0);
  cairo_stroke(cr);
  cairo_move_to(cr, bx, by - 6);
  cairo_line_to(cr, bx + 18, by);
  cairo_line_to(cr, bx, by + 6);
  cairo_close_path(cr);
  fillOutline(cr, rgb(210, 216, 226), kInk, 1.4);
}

// Cultist (level 13's bonus, Boulder Surfing): a hooded figure in a long
// robe, arms raised to the boulder it worships; variant 1 is mid-stride.
void cultist(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  const Color robe = rgb(120, 40, 60), trim = rgb(230, 180, 70), skin = rgb(170, 110, 70);
  const double step = c.variant == 1 ? w * 0.06 : 0.0;
  for (double lx : {0.3 - step / w, 0.54 + step / w})
  {
    roundedRect(cr, x0 + w * lx, y0 + h * 0.86, w * 0.16, h * 0.12, 4);
    fillOutline(cr, rgb(70, 50, 40), kInk, kLine);
  }
  // The robe, wider at the hem.
  cairo_move_to(cr, x0 + w * 0.34, y0 + h * 0.24);
  cairo_line_to(cr, x0 + w * 0.66, y0 + h * 0.24);
  cairo_line_to(cr, x0 + w * 0.84, y0 + h * 0.9);
  cairo_line_to(cr, x0 + w * 0.16, y0 + h * 0.9);
  cairo_close_path(cr);
  fillGradientOutline(cr, y0 + h * 0.24, y0 + h * 0.9, lighten(robe, 0.15f), robe, kInk, kLine);
  cairo_rectangle(cr, x0 + w * 0.47, y0 + h * 0.3, w * 0.06, h * 0.58);
  setColor(cr, trim);
  cairo_fill(cr);
  // Arms raised in worship.
  for (int side : {-1, 1})
  {
    const double sx = x0 + w * (0.5 + side * 0.14), sy = y0 + h * 0.32;
    strokeLimb(cr, {{sx, sy}, {sx + side * w * 0.16, y0 + h * 0.18}, {sx + side * w * 0.2, y0 + h * 0.04}}, 9, robe,
      kInk, kLine);
    circle(cr, sx + side * w * 0.2, y0 + h * 0.04, 6);
    fillOutline(cr, skin, kInk, 1.4);
  }
  // The hood, the face in its shadow, two glowing eyes.
  cairo_move_to(cr, x0 + w * 0.5, y0 + h * 0.02);
  cairo_curve_to(cr, x0 + w * 0.76, y0 + h * 0.06, x0 + w * 0.74, y0 + h * 0.28, x0 + w * 0.64, y0 + h * 0.3);
  cairo_line_to(cr, x0 + w * 0.36, y0 + h * 0.3);
  cairo_curve_to(cr, x0 + w * 0.26, y0 + h * 0.28, x0 + w * 0.24, y0 + h * 0.06, x0 + w * 0.5, y0 + h * 0.02);
  cairo_close_path(cr);
  fillOutline(cr, robe, kInk, kLine);
  circle(cr, x0 + w * 0.5, y0 + h * 0.18, w * 0.13);
  setColor(cr, rgb(30, 18, 24));
  cairo_fill(cr);
  for (double ex : {0.45, 0.56})
  {
    circle(cr, x0 + w * ex, y0 + h * 0.17, 2.6);
    setColor(cr, rgb(255, 200, 80));
    cairo_fill(cr);
  }
}

// Pit Snake (level 13): coiled down its hole (variant 0), the head up and
// hissing (1), or reared with its jaws open (2).
void pitSnake(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const double x0 = kM, y0 = kM, w = c.w, h = c.h;
  const Color scales = rgb(150, 120, 60), belly = rgb(230, 210, 140);
  const double neckTop = c.variant == 2 ? y0 + 8 : (c.variant == 1 ? y0 + h * 0.35 : y0 + h * 0.5);
  strokeLimb(cr, {{x0 + w * 0.5, y0 + h}, {x0 + w * 0.42, (neckTop + y0 + h) * 0.5}, {x0 + w * 0.55, neckTop + 10}}, 14,
    scales, kInk, kLine);
  // Bands.
  for (double t = 0.2; t < 0.9; t += 0.25)
  {
    const double yy = neckTop + 10 + (y0 + h - neckTop - 10) * t;
    cairo_rectangle(cr, x0 + w * 0.4, yy, w * 0.2, 3);
    setColor(cr, belly);
    cairo_fill(cr);
  }
  // The head.
  cairo_save(cr);
  cairo_translate(cr, x0 + w * 0.56, neckTop + 6);
  cairo_move_to(cr, -14, -8);
  cairo_curve_to(cr, 4, -16, 22, -6, 24, 0);
  cairo_curve_to(cr, 20, 8, 2, 12, -14, 8);
  cairo_close_path(cr);
  cairo_restore(cr);
  fillOutline(cr, lighten(scales, 0.12f), kInk, kLine);
  circle(cr, x0 + w * 0.62, neckTop + 2, 3);
  setColor(cr, rgb(255, 60, 40));
  cairo_fill(cr);
  if (c.variant >= 1)
  {
    // The forked tongue.
    cairo_move_to(cr, x0 + w * 0.56 + 24, neckTop + 6);
    cairo_line_to(cr, x0 + w * 0.56 + 36, neckTop + 4);
    cairo_line_to(cr, x0 + w * 0.56 + 40, neckTop);
    cairo_move_to(cr, x0 + w * 0.56 + 36, neckTop + 4);
    cairo_line_to(cr, x0 + w * 0.56 + 40, neckTop + 9);
    setColor(cr, rgb(220, 40, 60));
    cairo_set_line_width(cr, 2.0);
    cairo_stroke(cr);
  }
}

// Totem Stack (level 13): carved heads one on another, as many as fit its
// height. Variant n > 0: head n (from the top) has its mouth aglow.
void totem(const Ctx& c)
{
  cairo_t* cr = c.cr;
  const int heads = std::max(1, int(c.h / 64.0 + 0.5));
  const Color wood[3] = {rgb(170, 110, 60), rgb(140, 150, 90), rgb(180, 90, 70)};
  for (int i = 0; i < heads; ++i)
  {
    const double x = kM + 2, y = kM + i * 64.0 + 2, w = c.w - 4, hh = 60.0;
    roundedRect(cr, x, y, w, hh, 8);
    fillGradientOutline(cr, y, y + hh, lighten(wood[i % 3], 0.15f), wood[i % 3], kInk, kLine);
    // Brow, eyes, the mouth.
    cairo_rectangle(cr, x + 4, y + 12, w - 8, 6);
    setColor(cr, darken(wood[i % 3], 0.3f));
    cairo_fill(cr);
    for (double ex : {0.3, 0.7})
    {
      circle(cr, x + w * ex, y + 26, 6);
      fillOutline(cr, rgb(250, 240, 210), kInk, 1.4);
    }
    const bool glow = c.variant == i + 1;
    roundedRect(cr, x + w * 0.25, y + 38, w * 0.5, 14, 4);
    fillOutline(cr, glow ? rgb(255, 200, 80) : rgb(40, 24, 14), kInk, 1.6);
    if (glow)
      radialGlow(cr, x + w * 0.5, y + 45, 30, rgb(255, 170, 60), 0.6);
  }
}

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
    {"track_hopper", trackHopper},
    {"rail_drone", railDrone},
    {"decoupler", decoupler},
    {"rappel_trooper", rappelTrooper},
    {"shield_trooper", shieldTrooper},
    {"hover_biker", hoverBiker},
    {"cardboard_runner", cardboardRunner},
    {"cardboard_truck", cardboardTruck},
    {"black_halo", blackHalo},
    {"cement_mixer", cementMixer},
    {"halo_pod", haloPod},
    {"halo_light", haloLight},
    {"howler", howler},
    {"viper", viper},
    {"cutter", cutter},
    {"guardian", guardian},
    {"dartface", dartFace},
    {"scarabs", scarabs},
    {"treasure_hunter", treasureHunter},
    {"wraith", wraith},
    {"monk", monk},
    {"moth", sunMoth},
    {"cartbandit", cartBandit},
    {"bat", caveBat},
    {"mole", rockMole},
    {"mine_cart", mineCart},
    {"blasting_cap", blastingCap},
    {"pin_ball", pinBall},
    {"toad", magmaToad},
    {"wisp", emberWisp},
    {"crab", basaltCrab},
    {"boulder", boulder},
    {"spearrunner", spearRunner},
    {"cultist", cultist},
    {"pitsnake", pitSnake},
    {"totem", totem},
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
