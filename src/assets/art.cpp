#include "assets/art.hpp"

#include "base/math.hpp"
#include "render/vector.hpp"

#include <cmath>
#include <vector>

namespace td
{

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr Color kInk = rgb(26, 20, 38);
constexpr double kLine = 2.4;

Color lighten(Color c, float t) { return lerpColor(c, rgb(255, 255, 255), t); }
Color darken(Color c, float t) { return lerpColor(c, rgb(0, 0, 0), t); }

Sprite toSprite(const VectorImage& img, const Renderer& r, float ax, float ay)
{
  Sprite s;
  s.facing[0] = img.toTexture(r, ax, ay, false);
  s.facing[1] = img.toTexture(r, ax, ay, true);
  return s;
}

// --- Characters --------------------------------------------------------------

struct Look
{
  int kind; // 0 Dash, 1 Rocco, 2 Nova
  Color skin, skinShade, hair, hairShade;
  Color top, topShade, pants, pantsShade, boots;
  Color gun, gunLight, accent;
  double torsoW, headR;
};

Look lookFor(int kind)
{
  switch (kind)
  {
    case 0:
      return {0,
              rgb(246, 200, 160), rgb(214, 156, 118), rgb(255, 214, 72), rgb(226, 140, 30),
              rgb(232, 64, 52), rgb(150, 28, 40), rgb(54, 76, 150), rgb(32, 44, 96), rgb(80, 52, 36),
              rgb(150, 160, 182), rgb(220, 228, 240), rgb(64, 226, 255),
              22.0, 11.5};
    case 1:
      return {1,
              rgb(170, 108, 70), rgb(126, 76, 48), rgb(42, 30, 24), rgb(20, 14, 10),
              rgb(104, 146, 64), rgb(62, 94, 38), rgb(110, 110, 124), rgb(70, 70, 82), rgb(48, 42, 40),
              rgb(214, 120, 40), rgb(255, 206, 120), rgb(255, 80, 60),
              28.0, 12.5};
    default:
      return {2,
              rgb(252, 214, 186), rgb(224, 164, 142), rgb(255, 86, 184), rgb(180, 30, 120),
              rgb(132, 66, 216), rgb(78, 34, 140), rgb(132, 66, 216), rgb(78, 34, 140), rgb(238, 238, 248),
              rgb(110, 236, 255), rgb(240, 255, 255), rgb(64, 255, 220),
              19.0, 11.0};
  }
}

struct Pose
{
  double bob = 0.0;
  double lean = 0.0;
  double thigh[2] = {0.0, 0.0}; // [0] back leg, [1] front leg; radians, + = forward
  double knee[2] = {0.0, 0.0};
  double armLift = 0.0;
  double hairSwing = 0.0;
};

void drawLeg(cairo_t* cr, const Look& L, double hx, double hy, double thigh, double knee, bool back)
{
  const double l1 = 17.0, l2 = 17.0;
  const double kx = hx + std::sin(thigh) * l1, ky = hy + std::cos(thigh) * l1;
  const double a2 = thigh - knee;
  const double fx = kx + std::sin(a2) * l2, fy = ky + std::cos(a2) * l2;
  strokeLimb(cr, {{hx, hy}, {kx, ky}, {fx, fy}}, 10.0, back ? L.pantsShade : L.pants, kInk, kLine);
  roundedRect(cr, fx - 6.0, fy - 4.0, 16.0, 9.0, 3.8);
  const Color boot = back ? darken(L.boots, 0.25f) : L.boots;
  fillGradientOutline(cr, fy - 4.0, fy + 5.0, lighten(boot, 0.2f), darken(boot, 0.2f), kInk, kLine);
}

void drawCharacter(cairo_t* cr, const Look& L, const Pose& P)
{
  const double hipX = 31.0 + P.lean * 0.3, hipY = 58.0 + P.bob;
  const double lean = P.lean;
  const bool bareArms = L.kind == 1;
  const Color sleeve = bareArms ? L.skin : L.top;
  const Color sleeveShade = bareArms ? L.skinShade : L.topShade;
  const double hcx = 34.0 + lean, hcy = 16.0 + P.bob, r = L.headR;

  // Nova's ponytail sits behind everything.
  if (L.kind == 2)
  {
    const double s = P.hairSwing;
    cairo_move_to(cr, hcx - 5, hcy - 9);
    cairo_curve_to(cr, hcx - 22, hcy - 12, hcx - 26 + s, hcy + 10, hcx - 16 + s * 0.6, hcy + 20);
    cairo_curve_to(cr, hcx - 14, hcy + 10, hcx - 10, hcy + 2, hcx - 8, hcy - 1);
    cairo_close_path(cr);
    fillGradientOutline(cr, hcy - 12, hcy + 20, L.hair, L.hairShade, kInk, kLine);
  }
  // Rocco's bandana knot tails.
  if (L.kind == 1)
  {
    const double s = P.hairSwing * 0.6;
    for (double off : {-1.0, 5.0})
    {
      cairo_move_to(cr, hcx - r + 1, hcy - 5);
      cairo_curve_to(cr, hcx - r - 6, hcy - 7 + off, hcx - r - 12, hcy - 2 + off + s, hcx - r - 13, hcy + 1 + off + s);
      cairo_curve_to(cr, hcx - r - 8, hcy + off, hcx - r - 4, hcy - 1, hcx - r + 1, hcy - 1);
      cairo_close_path(cr);
      fillOutline(cr, rgb(214, 40, 44), kInk, kLine);
    }
  }

  // Back arm and back leg.
  strokeLimb(cr, {{29.0 + lean, 37.0 + P.bob}, {25.0 + lean, 47.0 + P.bob}, {27.0 + lean, 55.0 + P.bob}}, 7.5, sleeveShade, kInk, kLine);
  drawLeg(cr, L, hipX - 2.0, hipY, P.thigh[0], P.knee[0], true);

  // Torso.
  const double tw = L.torsoW;
  const double tx = 32.0 - tw / 2.0 + lean, ty = 29.0 + P.bob, th = 33.0;
  roundedRect(cr, tx, ty, tw, th, 7.0);
  fillGradientOutline(cr, ty, ty + th, lighten(L.top, 0.15f), L.topShade, kInk, kLine);
  if (L.kind == 0)
  {
    roundedRect(cr, tx + tw * 0.55, ty + 3.0, tw * 0.22, th - 10.0, 2.0);
    fillOutline(cr, rgb(236, 236, 244), kInk, 1.2);
  }
  else if (L.kind == 1)
  {
    cairo_arc(cr, tx + tw * 0.62, ty + 12.0, 2.4, 0, 2 * kPi);
    fillOutline(cr, rgb(210, 214, 222), kInk, 1.0);
    strokeLimb(cr, {{tx + tw * 0.45, ty + 1.0}, {tx + tw * 0.62, ty + 10.0}}, 1.0, rgb(200, 200, 210), kInk, 0.0);
  }
  else
  {
    strokeLimb(cr, {{tx + 3.0, ty + 5.0}, {tx + tw - 4.0, ty + th - 9.0}}, 3.0, L.accent, kInk, 0.0);
  }
  roundedRect(cr, tx - 1.0, ty + th - 7.0, tw + 2.0, 6.0, 2.0);
  fillOutline(cr, darken(L.pantsShade, 0.35f), kInk, 1.6);
  roundedRect(cr, tx + tw * 0.6, ty + th - 6.5, 4.0, 5.0, 1.0);
  fillOutline(cr, L.gunLight, kInk, 0.8);

  // Front leg.
  drawLeg(cr, L, hipX + 2.0, hipY, P.thigh[1], P.knee[1], false);

  // Neck and head.
  roundedRect(cr, 29.0 + lean, 22.0 + P.bob, 8.0, 9.0, 2.0);
  fillOutline(cr, L.skinShade, kInk, 1.4);
  {
    cairo_arc(cr, hcx, hcy, r, 0, 2 * kPi);
    cairo_pattern_t* p = cairo_pattern_create_radial(hcx + 3, hcy - 4, 1, hcx, hcy, r + 2);
    cairo_pattern_add_color_stop_rgb(p, 0, redOf(L.skin) / 255.0 * 1.05, greenOf(L.skin) / 255.0 * 1.05, blueOf(L.skin) / 255.0 * 1.05);
    cairo_pattern_add_color_stop_rgb(p, 1, redOf(L.skinShade) / 255.0, greenOf(L.skinShade) / 255.0, blueOf(L.skinShade) / 255.0);
    cairo_set_source(cr, p);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(p);
    setColor(cr, kInk);
    cairo_set_line_width(cr, kLine);
    cairo_stroke(cr);
  }
  cairo_arc(cr, hcx - 4.0, hcy + 1.5, 2.6, 0, 2 * kPi);
  fillOutline(cr, L.skinShade, kInk, 1.2);

  if (L.kind != 0)
  {
    // eye
    cairo_save(cr);
    cairo_translate(cr, hcx + 5.5, hcy - 1.0);
    cairo_scale(cr, 1.0, 1.25);
    cairo_arc(cr, 0, 0, 2.5, 0, 2 * kPi);
    cairo_restore(cr);
    fillOutline(cr, rgb(255, 255, 255), kInk, 1.0);
    cairo_arc(cr, hcx + 6.5, hcy - 0.6, 1.4, 0, 2 * kPi);
    setColor(cr, kInk);
    cairo_fill(cr);
    strokeLimb(cr, {{hcx - 0.5, hcy - 5.0}, {hcx + 8.0, hcy - 5.5}}, 1.4, darken(L.hair, 0.1f), kInk, 0.0);
  }

  switch (L.kind)
  {
    case 0:
    {
      // spiky hair
      static const double pts[][2] = {
        {-12, 5}, {-18, -2}, {-12, -5}, {-16, -13}, {-6, -11}, {-5, -21}, {1, -13},
        {7, -19}, {8, -11}, {15, -12}, {11, -5}, {13, -3}, {7, -5}, {1, -4}, {-4, -1}, {-7, 5}};
      cairo_move_to(cr, hcx + pts[0][0], hcy + pts[0][1]);
      for (const auto& p : pts)
        cairo_line_to(cr, hcx + p[0], hcy + p[1]);
      cairo_close_path(cr);
      fillGradientOutline(cr, hcy - 21, hcy + 5, lighten(L.hair, 0.3f), L.hairShade, kInk, kLine);
      // visor shades
      roundedRect(cr, hcx + 1.0, hcy - 4.5, 13.0, 6.5, 3.0);
      fillGradientOutline(cr, hcy - 4.5, hcy + 2.0, lighten(L.accent, 0.4f), darken(L.accent, 0.35f), kInk, 1.8);
      strokeLimb(cr, {{hcx + 4.0, hcy - 3.0}, {hcx + 11.0, hcy - 3.0}}, 1.2, rgba(255, 255, 255, 220), kInk, 0.0);
      strokeLimb(cr, {{hcx + 5.0, hcy + 6.0}, {hcx + 8.5, hcy + 5.0}}, 1.3, kInk, kInk, 0.0);
      break;
    }
    case 1:
    {
      // beard
      cairo_move_to(cr, hcx - 3.0, hcy + 1.5);
      cairo_arc_negative(cr, hcx, hcy, r + 0.6, kPi * 0.88, kPi * 0.05);
      cairo_line_to(cr, hcx + r - 2.0, hcy + 3.0);
      cairo_line_to(cr, hcx + 4.0, hcy + 4.5);
      cairo_close_path(cr);
      fillGradientOutline(cr, hcy, hcy + r, L.hair, L.hairShade, kInk, kLine);
      strokeLimb(cr, {{hcx + 5.0, hcy + 6.5}, {hcx + 9.0, hcy + 6.0}}, 1.6, rgb(150, 60, 50), kInk, 0.0);
      // bandana
      cairo_arc(cr, hcx, hcy, r + 1.0, kPi * 1.0, kPi * 2.0);
      cairo_line_to(cr, hcx + r + 1.0, hcy - 3.0);
      cairo_line_to(cr, hcx - r - 1.0, hcy - 2.0);
      cairo_close_path(cr);
      fillGradientOutline(cr, hcy - r, hcy - 2, rgb(240, 70, 70), rgb(170, 24, 30), kInk, kLine);
      for (int i = 0; i < 3; ++i)
      {
        cairo_arc(cr, hcx - 5.0 + i * 5.5, hcy - 8.0 + (i % 2) * 2.0, 1.1, 0, 2 * kPi);
        setColor(cr, rgba(255, 255, 255, 200));
        cairo_fill(cr);
      }
      break;
    }
    default:
    {
      // hair cap with bangs
      cairo_move_to(cr, hcx - r - 1.5, hcy + 5.0);
      cairo_arc(cr, hcx, hcy, r + 1.5, kPi * 0.92, kPi * 1.92);
      cairo_line_to(cr, hcx + 7.0, hcy - 3.0);
      cairo_line_to(cr, hcx + 3.0, hcy - 6.0);
      cairo_line_to(cr, hcx - 2.0, hcy - 2.0);
      cairo_line_to(cr, hcx - 5.0, hcy + 6.0);
      cairo_close_path(cr);
      fillGradientOutline(cr, hcy - r, hcy + 6, lighten(L.hair, 0.25f), L.hairShade, kInk, kLine);
      // headband
      cairo_new_path(cr);
      cairo_arc(cr, hcx, hcy, r - 0.5, kPi * 1.12, kPi * 1.72);
      setColor(cr, L.accent);
      cairo_set_line_width(cr, 3.2);
      cairo_stroke(cr);
      strokeLimb(cr, {{hcx + 5.0, hcy + 6.0}, {hcx + 8.0, hcy + 5.6}}, 1.3, rgb(200, 70, 110), kInk, 0.0);
      break;
    }
  }

  // Front arm, gun and hand.
  const double sx = 34.0 + lean, sy = 36.0 + P.bob;
  const double hx = 47.0 + lean, hy = 43.0 + P.bob - P.armLift;
  strokeLimb(cr, {{sx, sy}, {40.0 + lean, 44.0 + P.bob - P.armLift * 0.5}, {hx, hy}}, 8.0, sleeve, kInk, kLine);
  const double gl = L.kind == 1 ? 24.0 : (L.kind == 2 ? 19.0 : 20.0);
  const double gh = L.kind == 1 ? 10.0 : 7.5;
  roundedRect(cr, hx - 1.0, hy + 1.0, 4.5, 8.0, 1.5);
  fillOutline(cr, darken(L.gun, 0.45f), kInk, 1.4);
  roundedRect(cr, hx - 4.0, hy - gh + 1.0, gl, gh, 2.8);
  fillGradientOutline(cr, hy - gh + 1.0, hy + 1.0, L.gunLight, L.gun, kInk, kLine);
  roundedRect(cr, hx - 4.0 + gl - 1.0, hy - gh * 0.5 - 1.0, 6.0, 3.6, 1.2);
  fillOutline(cr, darken(L.gun, 0.3f), kInk, 1.4);
  roundedRect(cr, hx + 2.0, hy - gh + 2.5, gl * 0.45, 2.0, 1.0);
  setColor(cr, withAlpha(L.accent, 230));
  cairo_fill(cr);
  cairo_arc(cr, hx, hy, 3.8, 0, 2 * kPi);
  fillOutline(cr, L.skin, kInk, 1.8);
}

Pose idlePose(int frame)
{
  Pose p;
  p.bob = frame == 0 ? 0.0 : 0.8;
  p.thigh[0] = -0.1;
  p.thigh[1] = 0.1;
  p.knee[0] = p.knee[1] = 0.06;
  p.armLift = frame == 0 ? 0.0 : -0.6;
  return p;
}

Pose runPose(int frame)
{
  const double ph = 2.0 * kPi * double(frame) / double(kRunFrames);
  Pose p;
  p.lean = 2.0;
  p.bob = -1.8 * std::abs(std::cos(ph));
  for (int leg = 0; leg < 2; ++leg)
  {
    const double q = ph + (leg == 0 ? kPi : 0.0);
    p.thigh[leg] = 0.62 * std::sin(q);
    p.knee[leg] = 0.15 + 1.15 * std::max(0.0, std::cos(q));
  }
  p.armLift = 1.2 * std::sin(ph);
  p.hairSwing = 3.0 * std::sin(ph + 1.0);
  return p;
}

Pose jumpPose()
{
  Pose p;
  p.thigh[1] = 0.95;
  p.knee[1] = 1.5;
  p.thigh[0] = -0.25;
  p.knee[0] = 0.95;
  p.armLift = 3.0;
  p.hairSwing = 5.0;
  return p;
}

Pose fallPose()
{
  Pose p;
  p.thigh[1] = 0.4;
  p.knee[1] = 0.35;
  p.thigh[0] = -0.35;
  p.knee[0] = 0.55;
  p.armLift = 1.0;
  p.hairSwing = -5.0;
  return p;
}

constexpr int kCharTexW = 128;
constexpr int kCharTexH = 120;
constexpr double kCharOffX = 32.0;
constexpr double kCharOffY = 20.0;

Sprite bakeCharacter(const Renderer& r, const Look& look, const Pose& pose)
{
  VectorImage img(kCharTexW, kCharTexH);
  cairo_translate(img.cr(), kCharOffX, kCharOffY);
  drawCharacter(img.cr(), look, pose);
  return toSprite(img, r, float(kCharOffX), float(kCharOffY));
}

CharacterArt buildCharacter(const Renderer& r, int kind)
{
  const Look look = lookFor(kind);
  CharacterArt art;
  for (int i = 0; i < 2; ++i)
    art.idle[std::size_t(i)] = bakeCharacter(r, look, idlePose(i));
  for (int i = 0; i < kRunFrames; ++i)
    art.run[std::size_t(i)] = bakeCharacter(r, look, runPose(i));
  art.jump = bakeCharacter(r, look, jumpPose());
  art.fall = bakeCharacter(r, look, fallPose());

  VectorImage portrait(kCharTexW * 2, kCharTexH * 2);
  cairo_scale(portrait.cr(), 2.0, 2.0);
  cairo_translate(portrait.cr(), kCharOffX, kCharOffY);
  drawCharacter(portrait.cr(), look, idlePose(0));
  art.portrait = portrait.toTexture(r, float(kCharTexW), 0.0f);
  return art;
}

// --- Enemies ---------------------------------------------------------------

constexpr int kEnemyTex = 96;
constexpr double kEnemyOff = 16.0;

Sprite bakeWalker(const Renderer& r, const Theme& t, int frame)
{
  VectorImage img(kEnemyTex, kEnemyTex);
  cairo_t* cr = img.cr();
  cairo_translate(cr, kEnemyOff, kEnemyOff);
  const double up0 = frame == 0 ? 0.0 : -3.0, up1 = frame == 0 ? -3.0 : 0.0;
  strokeLimb(cr, {{22, 46}, {20, 57 + up0}}, 7, t.enemyDark, kInk, kLine);
  roundedRect(cr, 11, 55 + up0, 17, 8, 3.5);
  fillOutline(cr, t.enemyDark, kInk, kLine);
  strokeLimb(cr, {{42, 46}, {44, 57 + up1}}, 7, t.enemyDark, kInk, kLine);
  roundedRect(cr, 37, 55 + up1, 17, 8, 3.5);
  fillOutline(cr, t.enemyDark, kInk, kLine);
  roundedRect(cr, 3, 28, 9, 15, 4.5);
  fillGradientOutline(cr, 28, 43, t.enemyLight, t.enemyDark, kInk, kLine);
  roundedRect(cr, 10, 22, 44, 28, 10);
  fillGradientOutline(cr, 22, 50, t.enemyLight, t.enemyDark, kInk, kLine);
  roundedRect(cr, 18, 33, 26, 9, 4);
  setColor(cr, rgba(0, 0, 0, 90));
  cairo_fill(cr);
  for (int i = 0; i < 3; ++i)
  {
    cairo_arc(cr, 24 + i * 7, 37.5, 1.8, 0, 2 * kPi);
    setColor(cr, i == 1 ? t.enemyEye : t.accentB);
    cairo_fill(cr);
  }
  roundedRect(cr, 52, 28, 9, 15, 4.5);
  fillGradientOutline(cr, 28, 43, t.enemyLight, t.enemyDark, kInk, kLine);
  strokeLimb(cr, {{26, 6}, {22, -4}}, 2, t.enemyDark, kInk, 1.0);
  cairo_arc(cr, 22, -5, 3, 0, 2 * kPi);
  fillOutline(cr, t.enemyEye, kInk, 1.4);
  roundedRect(cr, 15, 3, 34, 22, 10);
  fillGradientOutline(cr, 3, 25, lighten(t.enemyLight, 0.2f), t.enemyBody, kInk, kLine);
  roundedRect(cr, 22, 9, 26, 9, 4.5);
  setColor(cr, rgb(16, 14, 24));
  cairo_fill(cr);
  radialGlow(cr, 38, 13.5, 14, t.enemyEye, 0.55);
  roundedRect(cr, 30, 11.5, 16, 4, 2);
  setColor(cr, lighten(t.enemyEye, 0.4f));
  cairo_fill(cr);
  strokeLimb(cr, {{20, 6}, {30, 5}}, 1.6, rgba(255, 255, 255, 170), kInk, 0.0);
  return toSprite(img, r, float(kEnemyOff), float(kEnemyOff));
}

Sprite bakeFlyer(const Renderer& r, const Theme& t, int frame)
{
  VectorImage img(kEnemyTex, kEnemyTex);
  cairo_t* cr = img.cr();
  cairo_translate(cr, kEnemyOff, kEnemyOff);
  radialGlow(cr, 32, 54, 12, t.enemyEye, 0.6);
  strokeLimb(cr, {{10, 16}, {54, 16}}, 4, t.enemyDark, kInk, 1.6);
  for (double cx : {10.0, 54.0})
  {
    cairo_save(cr);
    cairo_translate(cr, cx, 12);
    cairo_scale(cr, 1.0, 0.25);
    cairo_arc(cr, 0, 0, frame == 0 ? 13.0 : 10.0, 0, 2 * kPi);
    cairo_restore(cr);
    setColor(cr, withAlpha(t.enemyLight, 120));
    cairo_fill(cr);
    roundedRect(cr, cx - 2, 11, 4, 6, 1.5);
    fillOutline(cr, t.enemyDark, kInk, 1.2);
  }
  roundedRect(cr, 27, 44, 10, 7, 3);
  fillOutline(cr, t.enemyDark, kInk, kLine);
  cairo_arc(cr, 32, 31, 16, 0, 2 * kPi);
  {
    cairo_pattern_t* p = cairo_pattern_create_radial(26, 24, 2, 32, 31, 18);
    cairo_pattern_add_color_stop_rgb(p, 0, 1, 1, 1);
    cairo_pattern_add_color_stop_rgb(p, 0.35, redOf(t.enemyLight) / 255.0, greenOf(t.enemyLight) / 255.0, blueOf(t.enemyLight) / 255.0);
    cairo_pattern_add_color_stop_rgb(p, 1, redOf(t.enemyDark) / 255.0, greenOf(t.enemyDark) / 255.0, blueOf(t.enemyDark) / 255.0);
    cairo_set_source(cr, p);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(p);
    setColor(cr, kInk);
    cairo_set_line_width(cr, kLine);
    cairo_stroke(cr);
  }
  cairo_arc(cr, 39, 32, 6.5, 0, 2 * kPi);
  fillOutline(cr, rgb(16, 14, 24), kInk, 1.4);
  radialGlow(cr, 39, 32, 12, t.enemyEye, 0.6);
  cairo_arc(cr, 39.5, 32, 3.2, 0, 2 * kPi);
  setColor(cr, lighten(t.enemyEye, 0.3f));
  cairo_fill(cr);
  return toSprite(img, r, float(kEnemyOff), float(kEnemyOff));
}

Sprite bakeTurret(const Renderer& r, const Theme& t)
{
  VectorImage img(kEnemyTex, kEnemyTex);
  cairo_t* cr = img.cr();
  cairo_translate(cr, kEnemyOff, kEnemyOff);
  roundedRect(cr, 38, 28, 26, 9, 3.5);
  fillGradientOutline(cr, 28, 37, t.enemyLight, t.enemyDark, kInk, kLine);
  roundedRect(cr, 60, 25.5, 6, 14, 2);
  fillOutline(cr, t.enemyDark, kInk, kLine);
  cairo_move_to(cr, 14, 46);
  cairo_arc(cr, 32, 46, 19, kPi, 2 * kPi);
  cairo_close_path(cr);
  fillGradientOutline(cr, 27, 46, lighten(t.enemyLight, 0.2f), t.enemyBody, kInk, kLine);
  radialGlow(cr, 30, 37, 11, t.enemyEye, 0.6);
  cairo_arc(cr, 30, 37, 4.2, 0, 2 * kPi);
  fillOutline(cr, lighten(t.enemyEye, 0.3f), kInk, 1.4);
  cairo_move_to(cr, 6, 63);
  cairo_line_to(cr, 11, 45);
  cairo_line_to(cr, 53, 45);
  cairo_line_to(cr, 58, 63);
  cairo_close_path(cr);
  fillGradientOutline(cr, 45, 63, t.enemyBody, t.enemyDark, kInk, kLine);
  for (int i = 0; i < 4; ++i)
  {
    roundedRect(cr, 13 + i * 10, 52, 6, 6, 1.5);
    setColor(cr, rgba(0, 0, 0, 80));
    cairo_fill(cr);
  }
  return toSprite(img, r, float(kEnemyOff), float(kEnemyOff));
}

// --- Items -------------------------------------------------------------------

Texture bakeGem(const Renderer& r, Color c)
{
  VectorImage img(48, 48);
  cairo_t* cr = img.cr();
  const double cx = 24, top = 9, mid = 20, bot = 40, hw = 13;
  auto tri = [&](double x1, double y1, double x2, double y2, double x3, double y3, Color col) {
    cairo_move_to(cr, x1, y1);
    cairo_line_to(cr, x2, y2);
    cairo_line_to(cr, x3, y3);
    cairo_close_path(cr);
    setColor(cr, col);
    cairo_fill(cr);
  };
  tri(cx - hw, mid, cx - 5, top, cx, mid, lighten(c, 0.55f));
  tri(cx, mid, cx - 5, top, cx + 5, top, lighten(c, 0.75f));
  tri(cx, mid, cx + 5, top, cx + hw, mid, lighten(c, 0.3f));
  tri(cx - hw, mid, cx, mid, cx, bot, c);
  tri(cx, mid, cx + hw, mid, cx, bot, darken(c, 0.3f));
  cairo_move_to(cr, cx - hw, mid);
  cairo_line_to(cr, cx - 5, top);
  cairo_line_to(cr, cx + 5, top);
  cairo_line_to(cr, cx + hw, mid);
  cairo_line_to(cr, cx, bot);
  cairo_close_path(cr);
  setColor(cr, darken(c, 0.55f));
  cairo_set_line_width(cr, 1.8);
  cairo_stroke(cr);
  strokeLimb(cr, {{17, 13}, {17, 19}}, 1.4, rgba(255, 255, 255, 230), kInk, 0.0);
  strokeLimb(cr, {{14, 16}, {20, 16}}, 1.4, rgba(255, 255, 255, 230), kInk, 0.0);
  return img.toTexture(r, 8.0f, 8.0f);
}

void heartPath(cairo_t* cr, double cx, double cy, double s)
{
  cairo_move_to(cr, cx, cy + s * 0.95);
  cairo_curve_to(cr, cx - s * 1.4, cy, cx - s * 0.9, cy - s * 0.95, cx, cy - s * 0.4);
  cairo_curve_to(cr, cx + s * 0.9, cy - s * 0.95, cx + s * 1.4, cy, cx, cy + s * 0.95);
  cairo_close_path(cr);
}

Texture bakeHeart(const Renderer& r, int size, Color c, Color shade, bool shine)
{
  VectorImage img(size, size);
  cairo_t* cr = img.cr();
  const double s = size * 0.42;
  heartPath(cr, size / 2.0, size / 2.0 + 1, s);
  fillGradientOutline(cr, size * 0.1, size * 0.9, c, shade, kInk, size / 14.0);
  if (shine)
  {
    cairo_arc(cr, size * 0.36, size * 0.36, size * 0.08, 0, 2 * kPi);
    setColor(cr, rgba(255, 255, 255, 220));
    cairo_fill(cr);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// --- Tiles -------------------------------------------------------------------

void speckle(cairo_t* cr, Rng& rng, int count, Color a, Color b, double x0, double y0, double w, double h)
{
  for (int i = 0; i < count; ++i)
  {
    cairo_arc(cr, x0 + rng.uniform() * w, y0 + rng.uniform() * h, 0.6 + rng.uniform() * 1.4, 0, 2 * kPi);
    setColor(cr, rng.uniform() < 0.5f ? a : b);
    cairo_fill(cr);
  }
}

Texture bakeSolid(const Renderer& r, const Theme& t, int variant)
{
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  Rng rng(std::uint32_t(variant * 7919 + int(t.id) * 31 + 7));
  switch (t.id)
  {
    case ThemeId::NeonOverdrive:
    {
      cairo_rectangle(cr, 0, 0, 64, 64);
      setColor(cr, t.rockDark);
      cairo_fill(cr);
      roundedRect(cr, 1.5, 1.5, 61, 61, 6);
      fillGradientOutline(cr, 0, 64, lighten(t.rock, 0.12f), t.rock, darken(t.rockDark, 0.2f), 2.0);
      strokeLimb(cr, {{6, 32}, {58, 32}}, 1.5, withAlpha(t.rockDark, 200), kInk, 0.0);
      cairo_move_to(cr, 4, 4);
      cairo_line_to(cr, 30, 4);
      cairo_line_to(cr, 4, 30);
      cairo_close_path(cr);
      setColor(cr, rgba(255, 255, 255, 12));
      cairo_fill(cr);
      if (variant == 1)
      {
        for (int i = 0; i < 2; ++i)
        {
          const double x = 10 + i * 26, y = 12 + i * 26;
          radialGlow(cr, x + 6, y + 4, 12, t.accentA, 0.35);
          roundedRect(cr, x, y, 12, 8, 2);
          setColor(cr, withAlpha(lighten(t.accentA, 0.2f), 220));
          cairo_fill(cr);
        }
      }
      else if (variant == 2)
      {
        for (int i = 0; i < 4; ++i)
        {
          roundedRect(cr, 12, 40 + i * 4.5, 40, 2.2, 1);
          setColor(cr, withAlpha(t.rockDark, 230));
          cairo_fill(cr);
        }
        radialGlow(cr, 50, 14, 8, t.accentB, 0.5);
      }
      break;
    }
    case ThemeId::TempleOfTurbo:
    {
      cairo_rectangle(cr, 0, 0, 64, 64);
      setColor(cr, darken(t.rockDark, 0.2f));
      cairo_fill(cr);
      const double bricks[][4] = {{1.5, 1.5, 61, 29}, {1.5, 33.5, 29, 29}, {33.5, 33.5, 29, 29}};
      for (const auto& b : bricks)
      {
        roundedRect(cr, b[0], b[1], b[2], b[3], 6);
        fillGradientOutline(cr, b[1], b[1] + b[3], lighten(t.rock, 0.12f), darken(t.rock, 0.1f), t.rockDark, 1.6);
        speckle(cr, rng, 18, withAlpha(t.rockLight, 120), withAlpha(t.rockDark, 120), b[0] + 3, b[1] + 3, b[2] - 6, b[3] - 6);
      }
      if (variant >= 1)
      {
        const double mx = variant == 1 ? 8 : 44, my = variant == 1 ? 26 : 56;
        for (int i = 0; i < 6; ++i)
        {
          cairo_arc(cr, mx + rng.range(-7, 7), my + rng.range(-4, 4), rng.range(3, 6), 0, 2 * kPi);
          setColor(cr, withAlpha(i % 2 ? t.trim : t.trimGlow, 210));
          cairo_fill(cr);
        }
      }
      break;
    }
    case ThemeId::StationZero:
    {
      cairo_rectangle(cr, 0, 0, 64, 64);
      setColor(cr, t.rockDark);
      cairo_fill(cr);
      roundedRect(cr, 1.5, 1.5, 61, 61, 4);
      fillGradientOutline(cr, 0, 64, t.rockLight, t.rock, darken(t.rockDark, 0.3f), 2.0);
      for (int i = 0; i < 14; ++i)
      {
        const double y = 5 + rng.uniform() * 54;
        strokeLimb(cr, {{5, y}, {59, y}}, 0.7, rgba(255, 255, 255, 18), kInk, 0.0);
      }
      for (double x : {7.0, 57.0})
        for (double y : {7.0, 57.0})
        {
          cairo_arc(cr, x, y, 2.6, 0, 2 * kPi);
          fillOutline(cr, lighten(t.rockLight, 0.3f), t.rockDark, 1.0);
        }
      if (variant == 1)
      {
        for (int i = 0; i < 5; ++i)
        {
          roundedRect(cr, 16, 18 + i * 6, 32, 3, 1.5);
          setColor(cr, darken(t.rockDark, 0.3f));
          cairo_fill(cr);
        }
      }
      else if (variant == 2)
      {
        roundedRect(cr, 16, 18, 32, 24, 3);
        fillOutline(cr, rgb(10, 24, 30), t.rockDark, 2.0);
        for (int i = 0; i < 4; ++i)
          strokeLimb(cr, {{20, 23 + i * 5.0}, {20 + rng.range(10, 24), 23 + i * 5.0}}, 1.6, withAlpha(t.accentB, 220), kInk, 0.0);
        radialGlow(cr, 32, 30, 22, t.accentB, 0.18);
      }
      break;
    }
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

constexpr int kTopTexH = 96;
constexpr double kTopOff = 32.0;

Texture bakeSolidTop(const Renderer& r, const Theme& t)
{
  VectorImage img(64, kTopTexH);
  cairo_t* cr = img.cr();
  Rng rng(99u + std::uint32_t(t.id));
  const double y = kTopOff;
  switch (t.id)
  {
    case ThemeId::NeonOverdrive:
    {
      cairo_pattern_t* p = cairo_pattern_create_linear(0, y - 26, 0, y + 22);
      cairo_pattern_add_color_stop_rgba(p, 0.0, redOf(t.trim) / 255.0, greenOf(t.trim) / 255.0, blueOf(t.trim) / 255.0, 0.0);
      cairo_pattern_add_color_stop_rgba(p, 0.5, redOf(t.trim) / 255.0, greenOf(t.trim) / 255.0, blueOf(t.trim) / 255.0, 0.45);
      cairo_pattern_add_color_stop_rgba(p, 1.0, redOf(t.trim) / 255.0, greenOf(t.trim) / 255.0, blueOf(t.trim) / 255.0, 0.0);
      cairo_rectangle(cr, 0, y - 26, 64, 48);
      cairo_set_source(cr, p);
      cairo_fill(cr);
      cairo_pattern_destroy(p);
      cairo_rectangle(cr, 0, y - 1, 64, 6);
      setColor(cr, t.trim);
      cairo_fill(cr);
      cairo_rectangle(cr, 0, y, 64, 2.5);
      setColor(cr, rgb(235, 255, 255));
      cairo_fill(cr);
      break;
    }
    case ThemeId::TempleOfTurbo:
    {
      cairo_move_to(cr, 0, y - 3);
      for (int x = 0; x <= 64; x += 4)
        cairo_line_to(cr, x, y - 3 - (x % 8 == 0 ? rng.range(4, 12) : rng.range(0, 3)));
      cairo_line_to(cr, 64, y + 8);
      for (int x = 64; x >= 0; x -= 8)
        cairo_line_to(cr, x, y + 8 + rng.range(0, 7));
      cairo_close_path(cr);
      fillGradientOutline(cr, y - 15, y + 14, t.trimGlow, darken(t.trim, 0.2f), darken(t.trim, 0.55f), 1.6);
      for (int i = 0; i < 4; ++i)
      {
        cairo_arc(cr, rng.range(4, 60), y + rng.range(2, 8), 1.5, 0, 2 * kPi);
        setColor(cr, i % 2 ? rgb(255, 230, 80) : rgb(255, 120, 150));
        cairo_fill(cr);
      }
      break;
    }
    case ThemeId::StationZero:
    {
      cairo_save(cr);
      cairo_rectangle(cr, 0, y, 64, 10);
      cairo_clip(cr);
      setColor(cr, t.trim);
      cairo_paint(cr);
      for (int x = -16; x < 80; x += 16)
      {
        cairo_move_to(cr, x, y + 10);
        cairo_line_to(cr, x + 8, y + 10);
        cairo_line_to(cr, x + 18, y);
        cairo_line_to(cr, x + 10, y);
        cairo_close_path(cr);
        setColor(cr, rgb(30, 30, 36));
        cairo_fill(cr);
      }
      cairo_restore(cr);
      cairo_move_to(cr, 0, y + 1);
      for (int x = 0; x <= 64; x += 8)
        cairo_line_to(cr, x, y - 2 - rng.range(0, 3));
      cairo_line_to(cr, 64, y + 1);
      cairo_close_path(cr);
      setColor(cr, rgba(235, 248, 255, 240));
      cairo_fill(cr);
      for (int i = 0; i < 3; ++i)
      {
        const double x = rng.range(6, 58), len = rng.range(5, 12);
        cairo_move_to(cr, x - 2.5, y + 10);
        cairo_line_to(cr, x + 2.5, y + 10);
        cairo_line_to(cr, x, y + 10 + len);
        cairo_close_path(cr);
        setColor(cr, rgba(200, 240, 255, 200));
        cairo_fill(cr);
      }
      break;
    }
  }
  return img.toTexture(r, 0.0f, float(kTopOff));
}

Texture bakePlatform(const Renderer& r, const Theme& t)
{
  VectorImage img(64, 40);
  cairo_t* cr = img.cr();
  switch (t.id)
  {
    case ThemeId::NeonOverdrive:
      radialGlow(cr, 32, 8, 34, t.platform, 0.25);
      roundedRect(cr, 1, 1, 62, 14, 6);
      fillGradientOutline(cr, 1, 15, lighten(t.platform, 0.3f), t.platformDark, darken(t.platformDark, 0.5f), 2.0);
      strokeLimb(cr, {{8, 5}, {56, 5}}, 2.2, rgba(255, 230, 250, 230), kInk, 0.0);
      break;
    case ThemeId::TempleOfTurbo:
      roundedRect(cr, 1, 1, 62, 15, 4);
      fillGradientOutline(cr, 1, 16, lighten(t.platform, 0.2f), t.platformDark, darken(t.platformDark, 0.5f), 2.0);
      strokeLimb(cr, {{8, 6}, {28, 7}}, 1.0, withAlpha(t.platformDark, 200), kInk, 0.0);
      strokeLimb(cr, {{34, 10}, {56, 9}}, 1.0, withAlpha(t.platformDark, 200), kInk, 0.0);
      for (double x : {6.0, 58.0})
      {
        roundedRect(cr, x - 2.5, -1, 5, 18, 2);
        fillOutline(cr, rgb(200, 170, 110), darken(t.platformDark, 0.5f), 1.2);
      }
      break;
    case ThemeId::StationZero:
      roundedRect(cr, 1, 1, 62, 12, 3);
      fillGradientOutline(cr, 1, 13, t.platform, t.platformDark, darken(t.platformDark, 0.5f), 2.0);
      for (int x = 6; x < 60; x += 7)
      {
        roundedRect(cr, x, 5, 4, 5, 1);
        setColor(cr, darken(t.platformDark, 0.5f));
        cairo_fill(cr);
      }
      strokeLimb(cr, {{4, 2.5}, {60, 2.5}}, 1.4, rgba(240, 252, 255, 230), kInk, 0.0);
      break;
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

Texture bakeSpikes(const Renderer& r, const Theme& t)
{
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  if (t.id == ThemeId::NeonOverdrive)
    for (int s = 0; s < 4; ++s)
      radialGlow(cr, 8 + s * 16, 30, 12, t.hazard, 0.35);
  for (int s = 0; s < 4; ++s)
  {
    const double x = s * 16.0;
    cairo_move_to(cr, x + 1.5, 62);
    cairo_line_to(cr, x + 8, 26);
    cairo_line_to(cr, x + 14.5, 62);
    cairo_close_path(cr);
    cairo_pattern_t* p = cairo_pattern_create_linear(x, 0, x + 16, 0);
    cairo_pattern_add_color_stop_rgb(p, 0, redOf(t.hazardLight) / 255.0, greenOf(t.hazardLight) / 255.0, blueOf(t.hazardLight) / 255.0);
    cairo_pattern_add_color_stop_rgb(p, 1, redOf(t.hazard) / 255.0, greenOf(t.hazard) / 255.0, blueOf(t.hazard) / 255.0);
    cairo_set_source(cr, p);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(p);
    setColor(cr, kInk);
    cairo_set_line_width(cr, 2.0);
    cairo_stroke(cr);
  }
  roundedRect(cr, 0, 58, 64, 6, 2);
  fillOutline(cr, darken(t.hazard, 0.5f), kInk, 1.5);
  return img.toTexture(r, 0.0f, 0.0f);
}

Texture bakeCrate(const Renderer& r, const Theme& t)
{
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  roundedRect(cr, 2, 2, 60, 60, 7);
  fillGradientOutline(cr, 2, 62, lighten(t.platform, 0.25f), t.platformDark, kInk, 2.5);
  roundedRect(cr, 10, 10, 44, 44, 4);
  setColor(cr, withAlpha(darken(t.platformDark, 0.3f), 160));
  cairo_fill(cr);
  strokeLimb(cr, {{13, 13}, {51, 51}}, 6, lighten(t.platform, 0.1f), kInk, 1.6);
  strokeLimb(cr, {{51, 13}, {13, 51}}, 6, lighten(t.platform, 0.1f), kInk, 1.6);
  for (double x : {7.0, 57.0})
    for (double y : {7.0, 57.0})
    {
      cairo_arc(cr, x, y, 2.2, 0, 2 * kPi);
      setColor(cr, lighten(t.accentA, 0.4f));
      cairo_fill(cr);
    }
  return img.toTexture(r, 0.0f, 0.0f);
}

// --- Backdrops ---------------------------------------------------------------

constexpr int kLayerW = 2560;

void verticalGradient(cairo_t* cr, double w, double h, std::initializer_list<std::pair<double, Color>> stops)
{
  cairo_pattern_t* p = cairo_pattern_create_linear(0, 0, 0, h);
  for (const auto& s : stops)
    cairo_pattern_add_color_stop_rgba(
      p, s.first, redOf(s.second) / 255.0, greenOf(s.second) / 255.0, blueOf(s.second) / 255.0, alphaOf(s.second) / 255.0);
  cairo_rectangle(cr, 0, 0, w, h);
  cairo_set_source(cr, p);
  cairo_fill(cr);
  cairo_pattern_destroy(p);
}

Texture bakeSky(const Renderer& r, const Theme& t)
{
  VectorImage img(kScreenW, kScreenH);
  cairo_t* cr = img.cr();
  verticalGradient(cr, kScreenW, kScreenH, {{0.0, t.skyTop}, {0.58, t.skyMid}, {1.0, t.skyBottom}});
  Rng rng(555u + std::uint32_t(t.id));
  switch (t.id)
  {
    case ThemeId::NeonOverdrive:
    {
      for (int i = 0; i < 160; ++i)
      {
        cairo_arc(cr, rng.uniform() * kScreenW, rng.uniform() * 330, 0.6 + rng.uniform() * 1.3, 0, 2 * kPi);
        setColor(cr, rgba(255, 220, 255, 90 + rng.irange(0, 150)));
        cairo_fill(cr);
      }
      radialGlow(cr, 880, 420, 520, rgb(255, 80, 160), 0.35);
      cairo_push_group(cr);
      cairo_arc(cr, 880, 420, 190, 0, 2 * kPi);
      cairo_pattern_t* p = cairo_pattern_create_linear(0, 230, 0, 610);
      cairo_pattern_add_color_stop_rgb(p, 0, 1.0, 0.95, 0.45);
      cairo_pattern_add_color_stop_rgb(p, 0.55, 1.0, 0.45, 0.45);
      cairo_pattern_add_color_stop_rgb(p, 1, 0.9, 0.15, 0.6);
      cairo_set_source(cr, p);
      cairo_fill(cr);
      cairo_pattern_destroy(p);
      cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
      for (int i = 0; i < 9; ++i)
      {
        const double y = 440 + i * 20.0;
        cairo_rectangle(cr, 600, y, 560, 3 + i * 1.2);
        cairo_fill(cr);
      }
      cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
      cairo_pop_group_to_source(cr);
      cairo_paint(cr);
      break;
    }
    case ThemeId::TempleOfTurbo:
    {
      radialGlow(cr, 320, 170, 600, rgb(255, 250, 200), 0.6);
      cairo_arc(cr, 320, 170, 70, 0, 2 * kPi);
      setColor(cr, rgba(255, 252, 230, 230));
      cairo_fill(cr);
      for (int i = 0; i < 7; ++i)
      {
        const double a = 0.35 + i * 0.16;
        cairo_move_to(cr, 320, 170);
        cairo_line_to(cr, 320 + std::cos(a) * 1600, 170 + std::sin(a) * 1600);
        cairo_line_to(cr, 320 + std::cos(a + 0.05) * 1600, 170 + std::sin(a + 0.05) * 1600);
        cairo_close_path(cr);
        setColor(cr, rgba(255, 255, 220, 22));
        cairo_fill(cr);
      }
      for (int i = 0; i < 9; ++i)
      {
        cairo_save(cr);
        cairo_translate(cr, rng.uniform() * kScreenW, 60 + rng.uniform() * 220);
        cairo_scale(cr, 1.0, 0.35);
        cairo_arc(cr, 0, 0, 60 + rng.uniform() * 90, 0, 2 * kPi);
        cairo_restore(cr);
        setColor(cr, rgba(255, 255, 255, 40));
        cairo_fill(cr);
      }
      break;
    }
    case ThemeId::StationZero:
    {
      const Color neb[3] = {rgb(120, 40, 170), rgb(20, 120, 160), rgb(160, 40, 90)};
      for (int i = 0; i < 6; ++i)
        radialGlow(cr, rng.uniform() * kScreenW, rng.uniform() * 500, 200 + rng.uniform() * 260, neb[i % 3], 0.22);
      for (int i = 0; i < 380; ++i)
      {
        const double x = rng.uniform() * kScreenW, y = rng.uniform() * kScreenH;
        const double s = rng.uniform();
        cairo_arc(cr, x, y, 0.5 + s * 1.4, 0, 2 * kPi);
        setColor(cr, rgba(230, 240, 255, 80 + int(s * 175)));
        cairo_fill(cr);
        if (s > 0.96)
        {
          radialGlow(cr, x, y, 10, rgb(200, 220, 255), 0.5);
          strokeLimb(cr, {{x - 7, y}, {x + 7, y}}, 0.8, rgba(255, 255, 255, 160), kInk, 0.0);
          strokeLimb(cr, {{x, y - 7}, {x, y + 7}}, 0.8, rgba(255, 255, 255, 160), kInk, 0.0);
        }
      }
      const double px = 960, py = 230, pr = 140;
      auto ring = [&](double a0, double a1) {
        cairo_save(cr);
        cairo_translate(cr, px, py);
        cairo_rotate(cr, -0.28);
        cairo_scale(cr, 1.0, 0.26);
        cairo_new_path(cr);
        cairo_arc(cr, 0, 0, 240, a0, a1);
        cairo_restore(cr);
        setColor(cr, rgba(220, 230, 255, 150));
        cairo_set_line_width(cr, 10);
        cairo_stroke(cr);
      };
      ring(kPi, 2 * kPi);
      radialGlow(cr, px, py, pr + 60, rgb(120, 170, 255), 0.35);
      cairo_arc(cr, px, py, pr, 0, 2 * kPi);
      cairo_pattern_t* p = cairo_pattern_create_radial(px - 50, py - 60, 10, px, py, pr);
      cairo_pattern_add_color_stop_rgb(p, 0, 0.75, 0.88, 1.0);
      cairo_pattern_add_color_stop_rgb(p, 0.6, 0.25, 0.45, 0.85);
      cairo_pattern_add_color_stop_rgb(p, 1, 0.05, 0.08, 0.25);
      cairo_set_source(cr, p);
      cairo_fill(cr);
      cairo_pattern_destroy(p);
      for (int i = 0; i < 5; ++i)
      {
        cairo_save(cr);
        cairo_arc(cr, px, py, pr, 0, 2 * kPi);
        cairo_clip(cr);
        cairo_rectangle(cr, px - pr, py - 70 + i * 34, pr * 2, 8 + i * 2);
        setColor(cr, rgba(255, 255, 255, 18));
        cairo_fill(cr);
        cairo_restore(cr);
      }
      ring(0, kPi);
      break;
    }
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// Draws `item(dx)` three times so layers tile seamlessly when scrolled.
template <typename F>
void wrapped(F item)
{
  for (double dx : {-double(kLayerW), 0.0, double(kLayerW)})
    item(dx);
}

Texture bakeBackFar(const Renderer& r, const Theme& t)
{
  VectorImage img(kLayerW, kScreenH);
  cairo_t* cr = img.cr();
  Rng rng(1234u + std::uint32_t(t.id));
  const Color haze = lerpColor(t.farLayer, t.skyMid, 0.35f);
  switch (t.id)
  {
    case ThemeId::NeonOverdrive:
    {
      double x = 0;
      while (x < kLayerW)
      {
        const double w = rng.range(70, 170), h = rng.range(220, 470);
        const std::uint32_t seed = rng.next();
        wrapped([&](double dx) {
          Rng wr(seed);
          cairo_rectangle(cr, x + dx, kScreenH - h, w, h);
          cairo_pattern_t* p = cairo_pattern_create_linear(0, kScreenH - h, 0, kScreenH);
          cairo_pattern_add_color_stop_rgb(p, 0, redOf(haze) / 255.0 * 1.25, greenOf(haze) / 255.0 * 1.1, blueOf(haze) / 255.0 * 1.2);
          cairo_pattern_add_color_stop_rgb(p, 1, redOf(t.farLayer) / 255.0, greenOf(t.farLayer) / 255.0, blueOf(t.farLayer) / 255.0);
          cairo_set_source(cr, p);
          cairo_fill(cr);
          cairo_pattern_destroy(p);
          for (double wy = kScreenH - h + 14; wy < kScreenH - 10; wy += 16)
            for (double wx = x + 10; wx < x + w - 12; wx += 13)
              if (wr.uniform() < 0.3f)
              {
                cairo_rectangle(cr, wx + dx, wy, 6, 8);
                setColor(cr, withAlpha(wr.uniform() < 0.6f ? t.accentA : t.accentB, 90 + wr.irange(0, 110)));
                cairo_fill(cr);
              }
          if (wr.uniform() < 0.5f)
          {
            strokeLimb(cr, {{x + dx + w * 0.5, kScreenH - h}, {x + dx + w * 0.5, kScreenH - h - 40}}, 3, t.farLayer, kInk, 0.0);
            radialGlow(cr, x + dx + w * 0.5, kScreenH - h - 42, 10, rgb(255, 60, 80), 0.9);
          }
        });
        x += w + rng.range(0, 30);
      }
      break;
    }
    case ThemeId::TempleOfTurbo:
    {
      for (int ridge = 0; ridge < 2; ++ridge)
      {
        const Color c = lerpColor(t.farLayer, t.skyBottom, ridge == 0 ? 0.55f : 0.25f);
        cairo_move_to(cr, 0, kScreenH);
        for (int x = 0; x <= kLayerW; x += 8)
        {
          const double f = double(x) / kLayerW * 2 * kPi;
          const double h = (ridge == 0 ? 360 : 260) + 70 * std::sin(f * 2 + ridge) + 35 * std::sin(f * 5 + 1.3 * ridge) + 18 * std::sin(f * 11);
          cairo_line_to(cr, x, kScreenH - h);
        }
        cairo_line_to(cr, kLayerW, kScreenH);
        cairo_close_path(cr);
        setColor(cr, c);
        cairo_fill(cr);
        if (ridge == 0)
        {
          const double px = 1500, base = kScreenH - 300;
          const Color pc = lerpColor(t.farLayer, t.skyBottom, 0.45f);
          for (int s = 0; s < 7; ++s)
          {
            const double w = 460 - s * 62;
            cairo_rectangle(cr, px - w / 2, base - s * 34, w, 35);
            setColor(cr, pc);
            cairo_fill(cr);
          }
          cairo_rectangle(cr, px - 30, base - 7 * 34 - 30, 60, 34);
          setColor(cr, pc);
          cairo_fill(cr);
          radialGlow(cr, px, base - 7 * 34 - 12, 60, t.accentA, 0.7);
        }
      }
      verticalGradient(cr, kLayerW, kScreenH, {{0.55, withAlpha(t.skyBottom, 0)}, {1.0, withAlpha(t.skyBottom, 110)}});
      break;
    }
    case ThemeId::StationZero:
    {
      double x = 0;
      while (x < kLayerW)
      {
        const double w = rng.range(140, 300), h = rng.range(90, 200), y = kScreenH - h - rng.range(60, 200);
        const std::uint32_t seed = rng.next();
        wrapped([&](double dx) {
          Rng wr(seed);
          roundedRect(cr, x + dx, y, w, h, 18);
          fillGradientOutline(cr, y, y + h, lighten(t.farLayer, 0.15f), t.farLayer, kInk, 0.0);
          cairo_rectangle(cr, x + dx + w * 0.5 - 8, y + h, 16, kScreenH - y - h);
          setColor(cr, t.farLayer);
          cairo_fill(cr);
          for (double ly = y + 20; ly < y + h - 14; ly += 22)
          {
            cairo_rectangle(cr, x + dx + 18, ly, w - 36, 5);
            setColor(cr, withAlpha(t.accentB, 70 + wr.irange(0, 120)));
            cairo_fill(cr);
          }
          radialGlow(cr, x + dx + w - 14, y + 10, 10, t.accentA, 0.9);
        });
        x += w + rng.range(80, 220);
      }
      break;
    }
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

Texture bakeBackNear(const Renderer& r, const Theme& t)
{
  VectorImage img(kLayerW, kScreenH);
  cairo_t* cr = img.cr();
  Rng rng(9876u + std::uint32_t(t.id));
  switch (t.id)
  {
    case ThemeId::NeonOverdrive:
    {
      double x = 0;
      while (x < kLayerW)
      {
        const double w = rng.range(120, 240), h = rng.range(150, 330);
        const std::uint32_t seed = rng.next();
        wrapped([&](double dx) {
          Rng wr(seed);
          cairo_rectangle(cr, x + dx, kScreenH - h, w, h);
          setColor(cr, t.nearLayer);
          cairo_fill(cr);
          cairo_rectangle(cr, x + dx, kScreenH - h, w, 4);
          setColor(cr, withAlpha(t.skyBottom, 90));
          cairo_fill(cr);
          if (wr.uniform() < 0.75f)
          {
            const Color sign = wr.uniform() < 0.5f ? t.platform : t.trim;
            const double sx = x + dx + 16, sy = kScreenH - h + 26, sw = w - 32, sh = 34;
            roundedRect(cr, sx, sy, sw, sh, 8);
            setColor(cr, withAlpha(sign, 60));
            cairo_set_line_width(cr, 12);
            cairo_stroke(cr);
            roundedRect(cr, sx, sy, sw, sh, 8);
            setColor(cr, lighten(sign, 0.35f));
            cairo_set_line_width(cr, 3);
            cairo_stroke(cr);
            for (int k = 0; k < 3; ++k)
            {
              roundedRect(cr, sx + 14 + k * (sw - 28) / 3.0, sy + 12, (sw - 28) / 3.0 - 10, 10, 4);
              setColor(cr, withAlpha(sign, 200));
              cairo_fill(cr);
            }
          }
          roundedRect(cr, x + dx + 20, kScreenH - h - 24, 40, 24, 4);
          setColor(cr, t.nearLayer);
          cairo_fill(cr);
        });
        x += w + rng.range(20, 120);
      }
      break;
    }
    case ThemeId::TempleOfTurbo:
    {
      double x = 0;
      while (x < kLayerW)
      {
        const double trunkH = rng.range(300, 520);
        const std::uint32_t seed = rng.next();
        wrapped([&](double dx) {
          Rng wr(seed);
          const double bx = x + dx;
          cairo_move_to(cr, bx + 20, kScreenH);
          cairo_curve_to(cr, bx + 30, kScreenH - trunkH * 0.5, bx + 10, kScreenH - trunkH * 0.8, bx + 28, kScreenH - trunkH);
          cairo_line_to(cr, bx + 44, kScreenH - trunkH);
          cairo_curve_to(cr, bx + 30, kScreenH - trunkH * 0.8, bx + 50, kScreenH - trunkH * 0.5, bx + 46, kScreenH);
          cairo_close_path(cr);
          setColor(cr, darken(t.nearLayer, 0.2f));
          cairo_fill(cr);
          for (int b = 0; b < 9; ++b)
          {
            const double cx = bx + 36 + wr.range(-80, 80), cy = kScreenH - trunkH + wr.range(-50, 30);
            const double rr = wr.range(34, 64);
            cairo_arc(cr, cx, cy, rr, 0, 2 * kPi);
            cairo_pattern_t* p = cairo_pattern_create_radial(cx - rr * 0.3, cy - rr * 0.4, 2, cx, cy, rr);
            const Color hi = lighten(t.nearLayer, 0.25f);
            cairo_pattern_add_color_stop_rgb(p, 0, redOf(hi) / 255.0, greenOf(hi) / 255.0, blueOf(hi) / 255.0);
            cairo_pattern_add_color_stop_rgb(p, 1, redOf(t.nearLayer) / 255.0 * 0.8, greenOf(t.nearLayer) / 255.0 * 0.8, blueOf(t.nearLayer) / 255.0 * 0.8);
            cairo_set_source(cr, p);
            cairo_fill(cr);
            cairo_pattern_destroy(p);
          }
          for (int v = 0; v < 3; ++v)
          {
            const double vx = bx + wr.range(-50, 110), vy = kScreenH - trunkH + 10, len = wr.range(90, 240);
            cairo_move_to(cr, vx, vy);
            cairo_curve_to(cr, vx + 12, vy + len * 0.3, vx - 12, vy + len * 0.7, vx + 4, vy + len);
            setColor(cr, withAlpha(lighten(t.trim, 0.1f), 200));
            cairo_set_line_width(cr, 3);
            cairo_stroke(cr);
          }
        });
        x += rng.range(160, 320);
      }
      break;
    }
    case ThemeId::StationZero:
    {
      for (int x = 0; x < kLayerW; x += 320)
      {
        wrapped([&](double dx) {
          const double bx = x + dx, top = 160;
          for (double px : {bx, bx + 84})
          {
            cairo_rectangle(cr, px, top, 10, kScreenH - top);
            setColor(cr, t.nearLayer);
            cairo_fill(cr);
          }
          for (double y = top; y < kScreenH; y += 84)
          {
            cairo_rectangle(cr, bx, y, 94, 8);
            setColor(cr, t.nearLayer);
            cairo_fill(cr);
            strokeLimb(cr, {{bx + 5, y + 4}, {bx + 89, y + 84}}, 4, t.nearLayer, kInk, 0.0);
            strokeLimb(cr, {{bx + 89, y + 4}, {bx + 5, y + 84}}, 4, t.nearLayer, kInk, 0.0);
          }
          radialGlow(cr, bx + 47, top - 6, 26, t.accentA, 0.8);
          cairo_arc(cr, bx + 47, top - 6, 5, 0, 2 * kPi);
          setColor(cr, lighten(t.accentA, 0.4f));
          cairo_fill(cr);
        });
      }
      break;
    }
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

Texture bakeVignette(const Renderer& r)
{
  VectorImage img(kScreenW, kScreenH);
  cairo_t* cr = img.cr();
  cairo_pattern_t* p = cairo_pattern_create_radial(640, 360, 320, 640, 360, 820);
  cairo_pattern_add_color_stop_rgba(p, 0, 0, 0, 0, 0);
  cairo_pattern_add_color_stop_rgba(p, 1, 0, 0, 0, 0.55);
  cairo_set_source(cr, p);
  cairo_paint(cr);
  cairo_pattern_destroy(p);
  return img.toTexture(r, 0.0f, 0.0f);
}

Texture bakeGlow(const Renderer& r, int radius)
{
  VectorImage img(radius * 2, radius * 2);
  cairo_t* cr = img.cr();
  cairo_pattern_t* p = cairo_pattern_create_radial(radius, radius, 0, radius, radius, radius);
  cairo_pattern_add_color_stop_rgba(p, 0.0, 1, 1, 1, 1.0);
  cairo_pattern_add_color_stop_rgba(p, 0.25, 1, 1, 1, 0.55);
  cairo_pattern_add_color_stop_rgba(p, 0.6, 1, 1, 1, 0.15);
  cairo_pattern_add_color_stop_rgba(p, 1.0, 1, 1, 1, 0.0);
  cairo_set_source(cr, p);
  cairo_paint(cr);
  cairo_pattern_destroy(p);
  return img.toTexture(r, float(radius), float(radius));
}

Texture bakeDecoBase(const Renderer& r, const Theme& t)
{
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  switch (t.id)
  {
    case ThemeId::NeonOverdrive:
      roundedRect(cr, 29, 34, 6, 30, 2);
      fillOutline(cr, rgb(40, 34, 60), kInk, 1.6);
      roundedRect(cr, 4, 4, 56, 32, 6);
      fillOutline(cr, rgb(28, 20, 44), kInk, 2.2);
      roundedRect(cr, 9, 9, 46, 22, 4);
      setColor(cr, darken(t.platform, 0.5f));
      cairo_set_line_width(cr, 3);
      cairo_stroke(cr);
      break;
    case ThemeId::TempleOfTurbo:
      strokeLimb(cr, {{32, 62}, {32, 30}}, 6, rgb(100, 64, 34), kInk, 1.8);
      cairo_move_to(cr, 20, 26);
      cairo_line_to(cr, 44, 26);
      cairo_line_to(cr, 38, 36);
      cairo_line_to(cr, 26, 36);
      cairo_close_path(cr);
      fillGradientOutline(cr, 26, 36, rgb(170, 140, 90), rgb(110, 84, 50), kInk, 2.0);
      break;
    case ThemeId::StationZero:
      roundedRect(cr, 20, 44, 24, 20, 4);
      fillGradientOutline(cr, 44, 64, t.rockLight, t.rockDark, kInk, 2.0);
      cairo_arc(cr, 32, 42, 9, kPi, 2 * kPi);
      cairo_close_path(cr);
      fillOutline(cr, darken(t.accentA, 0.4f), kInk, 2.0);
      break;
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

Texture bakeDecoLit(const Renderer& r, const Theme& t)
{
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  switch (t.id)
  {
    case ThemeId::NeonOverdrive:
      roundedRect(cr, 9, 9, 46, 22, 4);
      setColor(cr, lighten(t.platform, 0.4f));
      cairo_set_line_width(cr, 3);
      cairo_stroke(cr);
      strokeLimb(cr, {{17, 20}, {42, 20}}, 3, lighten(t.platform, 0.5f), kInk, 0.0);
      strokeLimb(cr, {{35, 14}, {43, 20}, {35, 26}}, 3, lighten(t.platform, 0.5f), kInk, 0.0);
      break;
    case ThemeId::TempleOfTurbo:
      cairo_move_to(cr, 32, 2);
      cairo_curve_to(cr, 44, 14, 42, 26, 32, 28);
      cairo_curve_to(cr, 22, 26, 20, 14, 32, 2);
      setColor(cr, rgb(255, 140, 40));
      cairo_fill(cr);
      cairo_move_to(cr, 32, 10);
      cairo_curve_to(cr, 38, 18, 37, 25, 32, 27);
      cairo_curve_to(cr, 27, 25, 26, 18, 32, 10);
      setColor(cr, rgb(255, 236, 140));
      cairo_fill(cr);
      break;
    case ThemeId::StationZero:
      cairo_arc(cr, 32, 42, 7, kPi, 2 * kPi);
      cairo_close_path(cr);
      setColor(cr, lighten(t.accentA, 0.3f));
      cairo_fill(cr);
      break;
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

Texture bakeExitBase(const Renderer& r, const Theme& t)
{
  // 96 wide, 144 tall; anchor so (0,0) is the top-left of the 64x128 exit area.
  VectorImage img(96, 144);
  cairo_t* cr = img.cr();
  cairo_translate(cr, 16, 8);
  roundedRect(cr, -10, 112, 84, 18, 8);
  fillGradientOutline(cr, 112, 130, lighten(t.rockLight, 0.2f), t.rockDark, kInk, 2.4);
  cairo_save(cr);
  cairo_translate(cr, 32, 114);
  cairo_scale(cr, 1.0, 0.28);
  cairo_arc(cr, 0, 0, 30, 0, 2 * kPi);
  cairo_restore(cr);
  setColor(cr, lighten(t.accentB, 0.3f));
  cairo_fill(cr);
  roundedRect(cr, -10, -6, 84, 14, 6);
  fillGradientOutline(cr, -6, 8, lighten(t.rockLight, 0.2f), t.rockDark, kInk, 2.4);
  for (double x : {-4.0, 60.0})
  {
    roundedRect(cr, x, 4, 8, 112, 3);
    fillGradientOutline(cr, 0, 120, t.rockLight, t.rockDark, kInk, 2.0);
    for (int i = 0; i < 5; ++i)
    {
      cairo_arc(cr, x + 4, 20 + i * 20, 2.2, 0, 2 * kPi);
      setColor(cr, t.accentA);
      cairo_fill(cr);
    }
  }
  return img.toTexture(r, 16.0f, 8.0f);
}

Texture bakeExitBeam(const Renderer& r)
{
  VectorImage img(64, 112);
  cairo_t* cr = img.cr();
  cairo_pattern_t* p = cairo_pattern_create_linear(0, 0, 64, 0);
  cairo_pattern_add_color_stop_rgba(p, 0.0, 1, 1, 1, 0.0);
  cairo_pattern_add_color_stop_rgba(p, 0.5, 1, 1, 1, 0.9);
  cairo_pattern_add_color_stop_rgba(p, 1.0, 1, 1, 1, 0.0);
  cairo_set_source(cr, p);
  cairo_paint(cr);
  cairo_pattern_destroy(p);
  return img.toTexture(r, 0.0f, 0.0f);
}

} // namespace

Texture makePanel(const Renderer& r, int w, int h, Color fill, Color border, double radius)
{
  VectorImage img(w, h);
  cairo_t* cr = img.cr();
  roundedRect(cr, 2, 2, w - 4, h - 4, radius);
  fillGradientOutline(cr, 0, h, lighten(fill, 0.08f), fill, border, 3.0);
  roundedRect(cr, 6, 5, w - 12, std::min(24.0, h * 0.3), radius * 0.7);
  setColor(cr, rgba(255, 255, 255, 14));
  cairo_fill(cr);
  return img.toTexture(r, 0.0f, 0.0f);
}

Art Art::build(const Theme& theme, const Renderer& r)
{
  Art art;
  for (int i = 0; i < 3; ++i)
    art.characters[std::size_t(i)] = buildCharacter(r, i);

  for (int f = 0; f < 2; ++f)
  {
    art.walker[std::size_t(f)] = bakeWalker(r, theme, f);
    art.flyer[std::size_t(f)] = bakeFlyer(r, theme, f);
  }
  art.turret = bakeTurret(r, theme);

  art.gemColor = {theme.accentA, theme.accentB, rgb(110, 255, 130), rgb(255, 110, 230)};
  for (std::size_t i = 0; i < 4; ++i)
    art.gem[i] = bakeGem(r, art.gemColor[i]);
  {
    VectorImage img(48, 48);
    heartPath(img.cr(), 24, 25, 14);
    fillGradientOutline(img.cr(), 8, 40, rgb(255, 110, 130), rgb(200, 20, 50), kInk, 2.4);
    cairo_arc(img.cr(), 18, 19, 3, 0, 2 * kPi);
    setColor(img.cr(), rgba(255, 255, 255, 220));
    cairo_fill(img.cr());
    art.health = img.toTexture(r, 8.0f, 8.0f);
  }

  for (int v = 0; v < 3; ++v)
    art.solid[std::size_t(v)] = bakeSolid(r, theme, v);
  art.solidTop = bakeSolidTop(r, theme);
  art.platform = bakePlatform(r, theme);
  art.spikes = bakeSpikes(r, theme);
  art.crate = bakeCrate(r, theme);

  {
    VectorImage bolt(28, 12);
    roundedRect(bolt.cr(), 1, 2, 26, 8, 4);
    fillGradientOutline(bolt.cr(), 2, 10, rgb(255, 255, 230), rgb(255, 190, 60), kInk, 0.0);
    art.playerBullet[0] = bolt.toTexture(r, 2.0f, 0.0f);
    VectorImage pellet(16, 16);
    cairo_arc(pellet.cr(), 8, 8, 6, 0, 2 * kPi);
    fillGradientOutline(pellet.cr(), 2, 14, rgb(255, 250, 220), rgb(255, 140, 40), kInk, 0.0);
    art.playerBullet[1] = pellet.toTexture(r, 2.0f, 2.0f);
    VectorImage needle(32, 8);
    roundedRect(needle.cr(), 1, 1, 30, 6, 3);
    fillGradientOutline(needle.cr(), 1, 7, rgb(255, 255, 255), rgb(90, 240, 255), kInk, 0.0);
    art.playerBullet[2] = needle.toTexture(r, 2.0f, 0.0f);
    VectorImage orb(24, 24);
    radialGlow(orb.cr(), 12, 12, 12, theme.enemyEye, 1.0);
    cairo_arc(orb.cr(), 12, 12, 5, 0, 2 * kPi);
    setColor(orb.cr(), rgb(255, 255, 255));
    cairo_fill(orb.cr());
    art.enemyBullet = orb.toTexture(r, 2.0f, 2.0f);
  }

  art.sky = bakeSky(r, theme);
  art.backFar = bakeBackFar(r, theme);
  art.backNear = bakeBackNear(r, theme);
  art.vignette = bakeVignette(r);
  const int radii[4] = {16, 32, 64, 128};
  for (std::size_t i = 0; i < 4; ++i)
    art.glow[i] = bakeGlow(r, radii[i]);
  {
    VectorImage dot(16, 16);
    radialGlow(dot.cr(), 8, 8, 8, rgb(255, 255, 255), 1.0);
    cairo_arc(dot.cr(), 8, 8, 3, 0, 2 * kPi);
    setColor(dot.cr(), rgb(255, 255, 255));
    cairo_fill(dot.cr());
    art.dot = dot.toTexture(r, 8.0f, 8.0f);
  }
  art.decoBase = bakeDecoBase(r, theme);
  art.decoLit = bakeDecoLit(r, theme);
  art.exitBase = bakeExitBase(r, theme);
  art.exitBeam = bakeExitBeam(r);
  art.heartFull = bakeHeart(r, 32, rgb(255, 90, 110), rgb(200, 20, 50), true);
  art.heartEmpty = bakeHeart(r, 32, rgb(80, 80, 96), rgb(50, 50, 60), false);
  art.hudLeft = makePanel(r, 420, 54, rgba(8, 6, 20, 170), withAlpha(theme.accentA, 140), 14);
  art.hudCenter = makePanel(r, 170, 54, rgba(8, 6, 20, 170), withAlpha(theme.accentA, 140), 14);
  art.hudRight = makePanel(r, 300, 54, rgba(8, 6, 20, 170), withAlpha(theme.accentA, 140), 14);
  return art;
}

void drawGlow(Renderer& r, const Art& art, float cx, float cy, float radius, Color c, float alpha)
{
  std::size_t i = 0;
  const float radii[4] = {16.0f, 32.0f, 64.0f, 128.0f};
  while (i < 3 && radii[i] < radius)
    ++i;
  DrawOpts o;
  o.blend = Blend::Add;
  o.tint = c;
  o.alpha = alpha;
  o.scale = radius / radii[i];
  r.draw(art.glow[i], cx, cy, o);
}

void drawBackdrop(Renderer& r, const Art& art, float camX, float camY, float baseCamY)
{
  r.draw(art.sky, 0.0f, 0.0f);
  auto layer = [&](const Texture& t, float px, float py) {
    const float w = float(t.w());
    float ox = std::fmod(camX * px, w);
    if (ox < 0.0f)
      ox += w;
    const float oy = (camY - baseCamY) * py;
    for (float x = -ox; x < float(kScreenW); x += w)
      r.draw(t, x, -oy);
  };
  layer(art.backFar, 0.12f, 0.06f);
  layer(art.backNear, 0.3f, 0.15f);
}

void drawDecoration(Renderer& r, const Art& art, const Theme& t, float x, float y, int seed, int frame)
{
  r.draw(art.decoBase, x, y);
  DrawOpts lit;
  lit.blend = Blend::Add;
  switch (t.id)
  {
    case ThemeId::NeonOverdrive:
    {
      const bool on = (hash2(seed, frame / 7) % 13u) != 0;
      if (on)
      {
        r.draw(art.decoLit, x, y, lit);
        drawGlow(r, art, x + 32, y + 20, 70, t.platform, 0.45f);
      }
      break;
    }
    case ThemeId::TempleOfTurbo:
    {
      const float flick = 0.75f + 0.25f * float(hash2(seed, frame / 3) % 100u) / 100.0f;
      lit.alpha = flick;
      r.draw(art.decoLit, x, y - (1.0f - flick) * 6.0f, lit);
      drawGlow(r, art, x + 32, y + 18, 90 * flick, rgb(255, 150, 60), 0.5f);
      break;
    }
    case ThemeId::StationZero:
    {
      const float a = float(frame) * 0.12f + float(seed);
      lit.alpha = 0.6f + 0.4f * std::sin(a * 2.0f);
      r.draw(art.decoLit, x, y, lit);
      drawGlow(r, art, x + 32 + std::cos(a) * 26.0f, y + 40, 44, t.accentA, 0.5f + 0.3f * std::sin(a));
      break;
    }
  }
}

void drawExit(Renderer& r, const Art& art, const Theme& t, float x, float y, int frame)
{
  DrawOpts beam;
  beam.blend = Blend::Add;
  beam.tint = t.accentB;
  beam.alpha = 0.45f + 0.2f * std::sin(float(frame) * 0.1f);
  r.draw(art.exitBeam, x, y + 8, beam);
  for (int i = 0; i < 3; ++i)
  {
    const float phase = std::fmod(float(frame) * 0.012f + float(i) / 3.0f, 1.0f);
    drawGlow(r, art, x + 32, y + 116 - phase * 104, 30, t.accentB, 0.5f * (1.0f - phase));
  }
  drawGlow(r, art, x + 32, y + 64, 110, t.accentB, 0.25f);
  r.draw(art.exitBase, x, y);
  r.drawText("EXIT", x + 32, y - 40, {22.0f, t.accentA, rgb(20, 16, 28)}, Align::Center);
}

} // namespace td
