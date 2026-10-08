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

using DrawFn = void (*)(const Ctx&);

DrawFn routineFor(const std::string& key)
{
  static const std::map<std::string, DrawFn> kRoutines{
    {"glass_crawler", glassCrawler},
    {"squeegee_drone", squeegeeDrone},
    {"penthouse_sniper", penthouseSniper},
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
