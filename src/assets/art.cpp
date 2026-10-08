#include "assets/art.hpp"

#include "base/math.hpp"
#include "render/vector.hpp"

#include <cmath>
#include <string_view>
#include <tuple>
#include <vector>

namespace gr
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

enum class Arms
{
  Aim,       // gun in the front hand, pointing along Pose::aim
  Climb,     // both hands on the ladder
  Hang,      // both hands on the pipe
  HangAim,   // back hand on the pipe, gun pointing down
  Flail,     // hit and flying
};

struct Pose
{
  double bob = 0.0; // + moves hips, torso and head down
  double lean = 0.0;
  double thigh[2] = {0.0, 0.0}; // [0] back leg, [1] front leg; radians, + = forward
  double knee[2] = {0.0, 0.0};
  double armLift = 0.0;
  double hairSwing = 0.0;
  Arms arms = Arms::Aim;
  double aim = 0.0;          // radians: 0 forward, -pi/2 up, +pi/2 down
  double grip[2] = {0, 0};   // hand offsets while climbing or hanging
};

constexpr double kHangHandY = -12.0; // where the hands hold the pipe

void drawLeg(cairo_t* cr, const Look& L, double hx, double hy, double thigh, double knee, bool back)
{
  const double l1 = 17.0, l2 = 17.0;
  const double kx = hx + std::sin(thigh) * l1, ky = hy + std::cos(thigh) * l1;
  const double a2 = thigh - knee;
  const double fx = kx + std::sin(a2) * l2, fy = ky + std::cos(a2) * l2;
  strokeLimb(cr, {{hx, hy}, {kx, ky}, {fx, fy}}, 10.0, back ? L.pantsShade : L.pants, kInk, kLine);
  cairo_save(cr);
  cairo_translate(cr, fx, fy);
  cairo_rotate(cr, -(a2) * 0.6);
  roundedRect(cr, -6.0, -4.0, 16.0, 9.0, 3.8);
  const Color boot = back ? darken(L.boots, 0.25f) : L.boots;
  fillGradientOutline(cr, -4.0, 5.0, lighten(boot, 0.2f), darken(boot, 0.2f), kInk, kLine);
  cairo_restore(cr);
}

void drawHand(cairo_t* cr, const Look& L, double x, double y)
{
  cairo_arc(cr, x, y, 3.8, 0, 2 * kPi);
  fillOutline(cr, L.skin, kInk, 1.8);
}

// The gun, drawn around the hand at (hx, hy) and rotated by angle.
void drawGun(cairo_t* cr, const Look& L, double hx, double hy, double angle)
{
  const double gl = L.kind == 1 ? 24.0 : (L.kind == 2 ? 19.0 : 20.0);
  const double gh = L.kind == 1 ? 10.0 : 7.5;
  cairo_save(cr);
  cairo_translate(cr, hx, hy);
  cairo_rotate(cr, angle);
  roundedRect(cr, -1.0, 1.0, 4.5, 8.0, 1.5);
  fillOutline(cr, darken(L.gun, 0.45f), kInk, 1.4);
  roundedRect(cr, -4.0, -gh + 1.0, gl, gh, 2.8);
  fillGradientOutline(cr, -gh + 1.0, 1.0, L.gunLight, L.gun, kInk, kLine);
  roundedRect(cr, -4.0 + gl - 1.0, -gh * 0.5 - 1.0, 6.0, 3.6, 1.2);
  fillOutline(cr, darken(L.gun, 0.3f), kInk, 1.4);
  roundedRect(cr, 2.0, -gh + 2.5, gl * 0.45, 2.0, 1.0);
  setColor(cr, withAlpha(L.accent, 230));
  cairo_fill(cr);
  cairo_restore(cr);
}

void drawCharacter(cairo_t* cr, const Look& L, const Pose& P)
{
  const double hipX = 31.0 + P.lean * 0.3, hipY = 58.0 + P.bob;
  const double lean = P.lean;
  const bool bareArms = L.kind == 1;
  const Color sleeve = bareArms ? L.skin : L.top;
  const Color sleeveShade = bareArms ? L.skinShade : L.topShade;
  const double hcx = 34.0 + lean, hcy = 16.0 + P.bob, r = L.headR;
  const double bsx = 29.0 + lean, bsy = 37.0 + P.bob; // back shoulder
  const double fsx = 34.0 + lean, fsy = 36.0 + P.bob; // front shoulder

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

  // Back arm.
  switch (P.arms)
  {
    case Arms::Aim:
      strokeLimb(cr, {{bsx, bsy}, {bsx - 4.0, bsy + 10.0 - P.armLift * 0.3}, {bsx - 2.0, bsy + 18.0 - P.armLift * 0.6}}, 7.5, sleeveShade, kInk, kLine);
      break;
    case Arms::Climb:
    {
      const double hx = hcx - 4.0, hy = hcy - 12.0 + P.grip[0];
      strokeLimb(cr, {{bsx, bsy}, {bsx - 5.0, (bsy + hy) * 0.5}, {hx, hy}}, 7.5, sleeveShade, kInk, kLine);
      drawHand(cr, L, hx, hy);
      break;
    }
    case Arms::Hang:
    case Arms::HangAim:
    {
      const double hx = hcx - 5.0 + P.grip[0], hy = kHangHandY;
      strokeLimb(cr, {{bsx, bsy}, {bsx - 3.0, (bsy + hy) * 0.5}, {hx, hy}}, 7.5, sleeveShade, kInk, kLine);
      drawHand(cr, L, hx, hy);
      break;
    }
    case Arms::Flail:
      strokeLimb(cr, {{bsx, bsy}, {bsx - 10.0, bsy - 6.0}, {bsx - 14.0, bsy - 16.0}}, 7.5, sleeveShade, kInk, kLine);
      drawHand(cr, L, bsx - 14.0, bsy - 16.0);
      break;
  }
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
    const double look = P.arms == Arms::Aim && P.aim < -0.5 ? -1.2 : (P.aim > 0.5 ? 1.0 : 0.0);
    cairo_arc(cr, hcx + 6.5, hcy - 0.6 + look, 1.4, 0, 2 * kPi);
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

  // Front arm: holds the gun unless both hands are busy.
  switch (P.arms)
  {
    case Arms::Aim:
    case Arms::HangAim:
    {
      double ox = 13.0, oy = 11.0 - P.armLift;
      if (P.aim < -0.5)
      {
        ox = 6.0;
        oy = -12.0;
      }
      else if (P.aim > 0.5)
      {
        ox = 9.0;
        oy = 13.0;
      }
      const double hx = fsx + ox, hy = fsy + oy;
      strokeLimb(cr, {{fsx, fsy}, {(fsx + hx) * 0.5 - 1.0, (fsy + hy) * 0.5 + 3.0}, {hx, hy}}, 8.0, sleeve, kInk, kLine);
      drawGun(cr, L, hx, hy, P.aim);
      drawHand(cr, L, hx, hy);
      break;
    }
    case Arms::Climb:
    {
      const double hx = hcx + 6.0, hy = hcy - 15.0 + P.grip[1];
      strokeLimb(cr, {{fsx, fsy}, {fsx + 6.0, (fsy + hy) * 0.5 + 2.0}, {hx, hy}}, 8.0, sleeve, kInk, kLine);
      drawHand(cr, L, hx, hy);
      break;
    }
    case Arms::Hang:
    {
      const double hx = hcx + 5.0 + P.grip[1], hy = kHangHandY;
      strokeLimb(cr, {{fsx, fsy}, {fsx + 4.0, (fsy + hy) * 0.5}, {hx, hy}}, 8.0, sleeve, kInk, kLine);
      drawHand(cr, L, hx, hy);
      break;
    }
    case Arms::Flail:
    {
      const double hx = fsx + 14.0, hy = fsy - 14.0;
      strokeLimb(cr, {{fsx, fsy}, {fsx + 10.0, fsy - 4.0}, {hx, hy}}, 8.0, sleeve, kInk, kLine);
      drawGun(cr, L, hx, hy, -1.1);
      drawHand(cr, L, hx, hy);
      break;
    }
  }
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

Pose lookUpPose()
{
  Pose p = idlePose(0);
  p.aim = -kPi / 2.0;
  p.lean = -1.0;
  return p;
}

Pose crouchPose()
{
  Pose p;
  p.bob = 19.0;
  p.lean = 1.0;
  p.thigh[1] = 1.35;
  p.knee[1] = 2.25;
  p.thigh[0] = 0.75;
  p.knee[0] = 2.2;
  p.hairSwing = 2.0;
  return p;
}

Pose coilPose()
{
  Pose p;
  p.bob = 8.0;
  p.thigh[1] = 0.75;
  p.knee[1] = 1.35;
  p.thigh[0] = 0.35;
  p.knee[0] = 1.15;
  p.armLift = -1.0;
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

Pose fallPose(bool fast)
{
  Pose p;
  p.thigh[1] = fast ? 0.55 : 0.4;
  p.knee[1] = fast ? 0.2 : 0.35;
  p.thigh[0] = fast ? -0.5 : -0.35;
  p.knee[0] = 0.55;
  p.armLift = fast ? 4.0 : 1.0;
  p.hairSwing = -6.0;
  return p;
}

Pose tuckPose()
{
  Pose p;
  p.bob = 12.0;
  p.thigh[1] = 1.9;
  p.knee[1] = 2.6;
  p.thigh[0] = 1.6;
  p.knee[0] = 2.5;
  p.armLift = -2.0;
  p.aim = 0.35;
  return p;
}

Pose climbPose(int frame)
{
  Pose p;
  p.arms = Arms::Climb;
  p.lean = 2.0;
  const double s = frame == 0 ? 1.0 : -1.0;
  p.grip[0] = 5.0 * s;
  p.grip[1] = -5.0 * s;
  p.thigh[1] = frame == 0 ? 0.9 : 0.15;
  p.knee[1] = frame == 0 ? 1.5 : 0.3;
  p.thigh[0] = frame == 0 ? 0.1 : 0.85;
  p.knee[0] = frame == 0 ? 0.25 : 1.45;
  return p;
}

Pose hangPose(int frame)
{
  Pose p;
  p.arms = Arms::Hang;
  p.bob = -24.0;
  p.lean = 1.0;
  const double ph = 2.0 * kPi * double(std::max(0, frame)) / 4.0;
  if (frame >= 0)
  {
    p.grip[0] = 4.0 * std::sin(ph);
    p.grip[1] = -4.0 * std::sin(ph);
    p.thigh[1] = 0.35 * std::sin(ph);
    p.thigh[0] = -0.35 * std::sin(ph);
    p.hairSwing = 3.0 * std::cos(ph);
  }
  else
  {
    p.thigh[1] = 0.1;
    p.thigh[0] = -0.08;
  }
  p.knee[0] = 0.35;
  p.knee[1] = 0.25;
  return p;
}

Pose hangAimDownPose()
{
  Pose p = hangPose(-1);
  p.arms = Arms::HangAim;
  p.aim = kPi / 2.0;
  return p;
}

Pose hangLegsUpPose()
{
  Pose p = hangPose(-1);
  p.thigh[1] = 1.7;
  p.knee[1] = 1.9;
  p.thigh[0] = 1.45;
  p.knee[0] = 1.8;
  return p;
}

Pose jetpackPose()
{
  Pose p;
  p.aim = kPi / 2.0;
  p.thigh[1] = 0.25;
  p.knee[1] = 0.5;
  p.thigh[0] = -0.15;
  p.knee[0] = 0.4;
  p.hairSwing = -6.0;
  return p;
}

Pose hurtPose()
{
  Pose p;
  p.arms = Arms::Flail;
  p.lean = -3.0;
  p.thigh[1] = 0.7;
  p.knee[1] = 0.6;
  p.thigh[0] = -0.6;
  p.knee[0] = 0.3;
  p.hairSwing = -7.0;
  return p;
}

// Characters are drawn in a 64x96 design box and baked at Duke Nukem II
// proportions: the 3x5 cell (96x160 px) collision box. Each texture is
// anchored at the middle of the feet.
constexpr double kCharScale = 1.65;
constexpr int kCharTexW = 240;
constexpr int kCharTexH = 256;
constexpr double kCharAnchorX = 120.0;
constexpr double kCharAnchorY = 216.0;
constexpr double kDesignFeetX = 32.0;
constexpr double kDesignFeetY = 96.0;

Sprite bakeCharacter(const Renderer& r, const Look& look, const Pose& pose, double designAnchorY = kDesignFeetY)
{
  VectorImage img(kCharTexW, kCharTexH);
  cairo_t* cr = img.cr();
  cairo_translate(cr, kCharAnchorX, kCharAnchorY);
  cairo_scale(cr, kCharScale, kCharScale);
  cairo_translate(cr, -kDesignFeetX, -designAnchorY);
  drawCharacter(cr, look, pose);
  return toSprite(img, r, float(kCharAnchorX), float(kCharAnchorY));
}

CharacterArt buildCharacter(const Renderer& r, int kind)
{
  const Look look = lookFor(kind);
  CharacterArt art;
  for (int i = 0; i < 2; ++i)
    art.idle[std::size_t(i)] = bakeCharacter(r, look, idlePose(i));
  for (int i = 0; i < kRunFrames; ++i)
    art.run[std::size_t(i)] = bakeCharacter(r, look, runPose(i));
  art.lookUp = bakeCharacter(r, look, lookUpPose());
  art.crouch = bakeCharacter(r, look, crouchPose());
  art.coil = bakeCharacter(r, look, coilPose());
  art.jump = bakeCharacter(r, look, jumpPose());
  art.fall = bakeCharacter(r, look, fallPose(false));
  art.fallFast = bakeCharacter(r, look, fallPose(true));
  // The tuck is spun around its middle, so anchor it there.
  art.tuck = bakeCharacter(r, look, tuckPose(), 66.0);
  for (int i = 0; i < 2; ++i)
    art.climb[std::size_t(i)] = bakeCharacter(r, look, climbPose(i));
  art.hang = bakeCharacter(r, look, hangPose(-1));
  for (int i = 0; i < 4; ++i)
    art.hangMove[std::size_t(i)] = bakeCharacter(r, look, hangPose(i));
  art.hangAimDown = bakeCharacter(r, look, hangAimDownPose());
  art.hangLegsUp = bakeCharacter(r, look, hangLegsUpPose());
  art.jetpack = bakeCharacter(r, look, jetpackPose());
  art.hurt = bakeCharacter(r, look, hurtPose());

  VectorImage portrait(256, 240);
  cairo_scale(portrait.cr(), 2.0, 2.0);
  cairo_translate(portrait.cr(), 32.0, 20.0);
  drawCharacter(portrait.cr(), look, idlePose(0));
  art.portrait = portrait.toTexture(r, 128.0f, 0.0f);
  return art;
}

} // namespace

Texture bakeCharacterPose(const Renderer& r, int kind, int pose, float scale, bool mirror)
{
  const Look look = lookFor(kind);
  Pose P;
  switch (pose)
  {
    case 1: P = runPose(2); break;
    case 2: P = jumpPose(); break;
    case 3: P = lookUpPose(); break;
    case 4: P = crouchPose(); break;
    case 5: P = hurtPose(); break;
    case 6: P = coilPose(); break;
    case 7: P = fallPose(true); break;
    case 9: // slumped, looking down at your own clothes
      P = idlePose(0);
      P.lean = 5.0;
      P.bob = 3.0;
      P.aim = kPi / 3.0;
      break;
    default: P = idlePose(pose == 8 ? 1 : 0); break;
  }
  const int w = int(128 * scale), h = int(130 * scale);
  VectorImage img(w, h);
  cairo_scale(img.cr(), scale, scale);
  cairo_translate(img.cr(), 32.0, 30.0);
  drawCharacter(img.cr(), look, P);
  return img.toTexture(r, float(w) * 0.5f, float(h), mirror);
}

namespace
{

// --- Enemies ---------------------------------------------------------------

// Enemies are drawn in a 64x64 design box and baked at 1.5x, which makes a
// walker 3x3 cells. Anchored at the middle of the feet.
constexpr int kEnemyTex = 144;
constexpr double kEnemyScale = 1.5;
constexpr double kEnemyOff = 16.0;
constexpr float kEnemyAnchorX = 72.0f;
constexpr float kEnemyAnchorY = 120.0f;

Sprite bakeWalker(const Renderer& r, const Theme& t, int frame)
{
  VectorImage img(kEnemyTex, kEnemyTex);
  cairo_t* cr = img.cr();
  cairo_scale(cr, kEnemyScale, kEnemyScale);
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
  return toSprite(img, r, kEnemyAnchorX, kEnemyAnchorY);
}

Sprite bakeFlyer(const Renderer& r, const Theme& t, int frame)
{
  VectorImage img(kEnemyTex, kEnemyTex);
  cairo_t* cr = img.cr();
  cairo_scale(cr, kEnemyScale, kEnemyScale);
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
  return toSprite(img, r, kEnemyAnchorX, kEnemyAnchorY);
}

Sprite bakeTurret(const Renderer& r, const Theme& t)
{
  VectorImage img(kEnemyTex, kEnemyTex);
  cairo_t* cr = img.cr();
  cairo_scale(cr, kEnemyScale, kEnemyScale);
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
  return toSprite(img, r, kEnemyAnchorX, kEnemyAnchorY);
}

// --- Items -------------------------------------------------------------------

enum class ShotStyle
{
  Normal,
  Laser,
  Rocket,
  Flame,
  Enemy,
};

// Item art lives in a 96x96 texture: the 2x2 cell (64x64 px) item box sits
// at (16, 16), with room around it for glows.
constexpr int kItemTex = 96;
constexpr float kItemOff = 16.0f;

Texture bakeItemBox(const Renderer& r, Color c)
{
  VectorImage img(kItemTex, kItemTex);
  cairo_t* cr = img.cr();
  cairo_translate(cr, kItemOff, kItemOff);
  // Shadowed body.
  roundedRect(cr, 3, 5, 58, 58, 8);
  setColor(cr, rgba(0, 0, 0, 90));
  cairo_fill(cr);
  roundedRect(cr, 2, 2, 60, 60, 8);
  fillGradientOutline(cr, 2, 62, rgb(92, 98, 120), rgb(46, 48, 66), kInk, 2.6);
  // Coloured frame and bands.
  roundedRect(cr, 6, 6, 52, 52, 6);
  setColor(cr, c);
  cairo_set_line_width(cr, 4.0);
  cairo_stroke(cr);
  for (double y : {20.0, 40.0})
  {
    roundedRect(cr, 4, y, 56, 5, 2);
    fillOutline(cr, darken(c, 0.25f), kInk, 1.2);
  }
  // Glowing emblem window.
  roundedRect(cr, 18, 18, 28, 28, 6);
  fillOutline(cr, rgb(20, 22, 34), kInk, 1.6);
  radialGlow(cr, 32, 32, 16, c, 0.75);
  cairo_move_to(cr, 32, 23);
  cairo_line_to(cr, 41, 32);
  cairo_line_to(cr, 32, 41);
  cairo_line_to(cr, 23, 32);
  cairo_close_path(cr);
  fillOutline(cr, lighten(c, 0.45f), kInk, 1.4);
  // Rivets and a highlight.
  for (double x : {9.0, 55.0})
    for (double y : {9.0, 55.0})
    {
      cairo_arc(cr, x, y, 2.2, 0, 2 * kPi);
      setColor(cr, lighten(c, 0.5f));
      cairo_fill(cr);
    }
  strokeLimb(cr, {{10, 4.5}, {44, 4.5}}, 1.6, rgba(255, 255, 255, 120), kInk, 0.0);
  return img.toTexture(r, kItemOff, kItemOff);
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

void drawGemShape(cairo_t* cr, Color c, double cx, double top, double mid, double bot, double hw)
{
  auto tri = [&](double x1, double y1, double x2, double y2, double x3, double y3, Color col) {
    cairo_move_to(cr, x1, y1);
    cairo_line_to(cr, x2, y2);
    cairo_line_to(cr, x3, y3);
    cairo_close_path(cr);
    setColor(cr, col);
    cairo_fill(cr);
  };
  const double tw = hw * 0.4;
  tri(cx - hw, mid, cx - tw, top, cx, mid, lighten(c, 0.55f));
  tri(cx, mid, cx - tw, top, cx + tw, top, lighten(c, 0.75f));
  tri(cx, mid, cx + tw, top, cx + hw, mid, lighten(c, 0.3f));
  tri(cx - hw, mid, cx, mid, cx, bot, c);
  tri(cx, mid, cx + hw, mid, cx, bot, darken(c, 0.3f));
  cairo_move_to(cr, cx - hw, mid);
  cairo_line_to(cr, cx - tw, top);
  cairo_line_to(cr, cx + tw, top);
  cairo_line_to(cr, cx + hw, mid);
  cairo_line_to(cr, cx, bot);
  cairo_close_path(cr);
  setColor(cr, darken(c, 0.55f));
  cairo_set_line_width(cr, 2.2);
  cairo_stroke(cr);
}

// Every collectable, drawn into the 64x64 item area.
Texture bakeItemIcon(const Renderer& r, const Theme& t, int icon, Color gemColor)
{
  VectorImage img(kItemTex, kItemTex);
  cairo_t* cr = img.cr();
  cairo_translate(cr, kItemOff, kItemOff);
  switch (icon)
  {
    case kIconHealth:
    {
      // A health molecule: heart in a glass bubble.
      radialGlow(cr, 32, 34, 34, rgb(255, 60, 100), 0.45);
      cairo_arc(cr, 32, 34, 25, 0, 2 * kPi);
      setColor(cr, rgba(255, 220, 230, 60));
      cairo_fill_preserve(cr);
      setColor(cr, rgba(255, 255, 255, 170));
      cairo_set_line_width(cr, 2.2);
      cairo_stroke(cr);
      heartPath(cr, 32, 35, 16);
      fillGradientOutline(cr, 18, 52, rgb(255, 120, 140), rgb(200, 20, 50), kInk, 2.4);
      cairo_arc(cr, 25, 27, 3.2, 0, 2 * kPi);
      setColor(cr, rgba(255, 255, 255, 230));
      cairo_fill(cr);
      cairo_arc(cr, 22, 18, 4, kPi, 1.6 * kPi);
      setColor(cr, rgba(255, 255, 255, 200));
      cairo_set_line_width(cr, 2.0);
      cairo_stroke(cr);
      break;
    }
    case kIconMerch0:
    {
      // Mixtape.
      cairo_save(cr);
      cairo_translate(cr, 32, 36);
      cairo_rotate(cr, -0.12);
      roundedRect(cr, -26, -17, 52, 34, 4);
      fillGradientOutline(cr, -17, 17, rgb(60, 60, 78), rgb(28, 28, 40), kInk, 2.4);
      roundedRect(cr, -21, -13, 42, 14, 2);
      fillGradientOutline(cr, -13, 1, t.accentA, darken(t.accentA, 0.3f), kInk, 1.4);
      strokeLimb(cr, {{-16, -8}, {12, -8}}, 1.6, rgba(255, 255, 255, 200), kInk, 0.0);
      for (double x : {-11.0, 11.0})
      {
        cairo_arc(cr, x, 8, 5.5, 0, 2 * kPi);
        fillOutline(cr, rgb(235, 235, 245), kInk, 1.6);
        cairo_arc(cr, x, 8, 2.2, 0, 2 * kPi);
        setColor(cr, kInk);
        cairo_fill(cr);
      }
      cairo_restore(cr);
      break;
    }
    case kIconMerch1:
    {
      // Runner cap.
      cairo_move_to(cr, 10, 40);
      cairo_curve_to(cr, 10, 14, 50, 12, 52, 38);
      cairo_close_path(cr);
      fillGradientOutline(cr, 14, 40, lighten(t.accentB, 0.2f), darken(t.accentB, 0.35f), kInk, 2.4);
      cairo_move_to(cr, 40, 36);
      cairo_curve_to(cr, 52, 34, 60, 38, 61, 44);
      cairo_curve_to(cr, 52, 45, 44, 43, 38, 41);
      cairo_close_path(cr);
      fillOutline(cr, darken(t.accentB, 0.45f), kInk, 2.2);
      cairo_arc(cr, 31, 16, 3, 0, 2 * kPi);
      fillOutline(cr, darken(t.accentB, 0.3f), kInk, 1.4);
      cairo_arc(cr, 28, 30, 7, 0, 2 * kPi);
      fillOutline(cr, rgb(255, 255, 255), kInk, 1.4);
      strokeLimb(cr, {{24, 30}, {32, 30}}, 2.2, t.accentA, kInk, 0.0);
      break;
    }
    case kIconMerch2:
    {
      // Action figure in a blister card.
      roundedRect(cr, 12, 6, 40, 54, 5);
      fillGradientOutline(cr, 6, 60, rgb(255, 214, 80), rgb(230, 120, 40), kInk, 2.4);
      roundedRect(cr, 17, 14, 30, 42, 6);
      setColor(cr, rgba(255, 255, 255, 110));
      cairo_fill(cr);
      cairo_arc(cr, 32, 23, 5.5, 0, 2 * kPi);
      fillOutline(cr, rgb(246, 200, 160), kInk, 1.6);
      roundedRect(cr, 26, 28, 12, 14, 3);
      fillOutline(cr, rgb(232, 64, 52), kInk, 1.6);
      strokeLimb(cr, {{28, 42}, {27, 52}}, 4, rgb(54, 76, 150), kInk, 1.2);
      strokeLimb(cr, {{36, 42}, {37, 52}}, 4, rgb(54, 76, 150), kInk, 1.2);
      strokeLimb(cr, {{38, 31}, {44, 25}}, 3.5, rgb(232, 64, 52), kInk, 1.2);
      strokeLimb(cr, {{26, 31}, {21, 37}}, 3.5, rgb(232, 64, 52), kInk, 1.2);
      roundedRect(cr, 20, 8, 24, 4, 2);
      setColor(cr, kInk);
      cairo_fill(cr);
      break;
    }
    case kIconLaser:
    {
      radialGlow(cr, 32, 32, 32, rgb(80, 230, 255), 0.4);
      roundedRect(cr, 6, 24, 40, 14, 5);
      fillGradientOutline(cr, 24, 38, rgb(220, 232, 245), rgb(110, 124, 150), kInk, 2.4);
      roundedRect(cr, 44, 28, 16, 6, 2);
      fillOutline(cr, rgb(90, 100, 120), kInk, 2.0);
      roundedRect(cr, 14, 36, 9, 16, 3);
      fillOutline(cr, rgb(70, 76, 96), kInk, 2.0);
      for (int i = 0; i < 4; ++i)
      {
        roundedRect(cr, 18 + i * 6, 27, 3.5, 8, 1.5);
        setColor(cr, rgb(120, 245, 255));
        cairo_fill(cr);
      }
      radialGlow(cr, 60, 31, 9, rgb(120, 255, 255), 0.9);
      break;
    }
    case kIconRocket:
    {
      radialGlow(cr, 32, 32, 32, rgb(255, 120, 60), 0.35);
      roundedRect(cr, 4, 22, 46, 18, 7);
      fillGradientOutline(cr, 22, 40, rgb(120, 150, 90), rgb(56, 76, 40), kInk, 2.4);
      cairo_move_to(cr, 48, 24);
      cairo_line_to(cr, 60, 31);
      cairo_line_to(cr, 48, 38);
      cairo_close_path(cr);
      fillOutline(cr, rgb(240, 70, 60), kInk, 2.2);
      roundedRect(cr, 16, 38, 10, 14, 3);
      fillOutline(cr, rgb(60, 60, 70), kInk, 2.0);
      roundedRect(cr, 10, 26, 30, 4, 2);
      setColor(cr, rgba(255, 255, 255, 110));
      cairo_fill(cr);
      cairo_arc(cr, 8, 31, 4, 0, 2 * kPi);
      fillOutline(cr, rgb(255, 210, 90), kInk, 1.6);
      break;
    }
    case kIconFlame:
    {
      radialGlow(cr, 32, 32, 32, rgb(255, 150, 40), 0.4);
      roundedRect(cr, 8, 22, 20, 32, 8);
      fillGradientOutline(cr, 22, 54, rgb(255, 100, 70), rgb(160, 30, 30), kInk, 2.4);
      strokeLimb(cr, {{24, 30}, {44, 30}}, 6, rgb(130, 136, 150), kInk, 2.0);
      cairo_move_to(cr, 44, 30);
      cairo_curve_to(cr, 50, 16, 58, 24, 62, 18);
      cairo_curve_to(cr, 60, 30, 64, 34, 58, 42);
      cairo_curve_to(cr, 54, 36, 50, 40, 44, 32);
      cairo_close_path(cr);
      fillGradientOutline(cr, 16, 42, rgb(255, 240, 120), rgb(255, 110, 30), kInk, 1.8);
      roundedRect(cr, 12, 18, 12, 6, 2);
      fillOutline(cr, rgb(90, 90, 100), kInk, 1.6);
      break;
    }
    case kIconRapidFire:
    {
      radialGlow(cr, 32, 32, 32, rgb(255, 220, 60), 0.45);
      cairo_arc(cr, 32, 32, 24, 0, 2 * kPi);
      fillGradientOutline(cr, 8, 56, rgb(80, 70, 110), rgb(36, 30, 56), kInk, 2.4);
      cairo_move_to(cr, 36, 10);
      cairo_line_to(cr, 20, 35);
      cairo_line_to(cr, 31, 35);
      cairo_line_to(cr, 27, 54);
      cairo_line_to(cr, 45, 27);
      cairo_line_to(cr, 34, 27);
      cairo_close_path(cr);
      fillGradientOutline(cr, 10, 54, rgb(255, 250, 170), rgb(255, 190, 40), kInk, 2.0);
      break;
    }
    case kIconKey:
    {
      radialGlow(cr, 32, 32, 30, rgb(255, 230, 80), 0.35);
      cairo_save(cr);
      cairo_translate(cr, 32, 33);
      cairo_rotate(cr, -0.2);
      roundedRect(cr, -24, -16, 48, 32, 5);
      fillGradientOutline(cr, -16, 16, rgb(250, 250, 255), rgb(190, 196, 214), kInk, 2.4);
      roundedRect(cr, -24, -9, 48, 7, 0);
      setColor(cr, rgb(255, 200, 40));
      cairo_fill(cr);
      roundedRect(cr, -18, 3, 12, 9, 2);
      fillOutline(cr, rgb(220, 180, 70), kInk, 1.4);
      strokeLimb(cr, {{0, 6}, {17, 6}}, 2, rgb(140, 146, 170), kInk, 0.0);
      strokeLimb(cr, {{0, 11}, {12, 11}}, 2, rgb(140, 146, 170), kInk, 0.0);
      cairo_restore(cr);
      break;
    }
    case kIconGem0:
    case kIconGem1:
    case kIconGem2:
    case kIconGem3:
    {
      radialGlow(cr, 32, 32, 30, gemColor, 0.35);
      drawGemShape(cr, gemColor, 32, 12, 26, 56, 20);
      strokeLimb(cr, {{24, 15}, {24, 23}}, 1.8, rgba(255, 255, 255, 230), kInk, 0.0);
      strokeLimb(cr, {{20, 19}, {28, 19}}, 1.8, rgba(255, 255, 255, 230), kInk, 0.0);
      break;
    }
    case kIconTurbo:
    {
      // Turbo: a hot orange core with double chevrons pointing forward.
      radialGlow(cr, 32, 32, 36, rgb(255, 170, 40), 0.6);
      cairo_arc(cr, 32, 32, 25, 0, 2 * kPi);
      fillGradientOutline(cr, 7, 57, rgb(255, 236, 120), rgb(240, 90, 20), kInk, 2.6);
      cairo_arc(cr, 32, 32, 19, 0, 2 * kPi);
      setColor(cr, rgba(255, 255, 255, 80));
      cairo_set_line_width(cr, 2.0);
      cairo_stroke(cr);
      for (double ox : {-7.0, 6.0})
      {
        cairo_move_to(cr, 23 + ox, 19);
        cairo_line_to(cr, 35 + ox, 32);
        cairo_line_to(cr, 23 + ox, 45);
        cairo_line_to(cr, 29 + ox, 45);
        cairo_line_to(cr, 41 + ox, 32);
        cairo_line_to(cr, 29 + ox, 19);
        cairo_close_path(cr);
        fillOutline(cr, rgb(255, 255, 255), kInk, 2.0);
      }
      break;
    }
    case kIconVirus:
    {
      // Virus: a sickly green germ with knobbed spikes and a mean look.
      const Color body = rgb(130, 220, 60);
      radialGlow(cr, 32, 32, 34, rgb(120, 255, 60), 0.45);
      for (int i = 0; i < 10; ++i)
      {
        const double a = double(i) * 2.0 * kPi / 10.0 + 0.2;
        const double c = std::cos(a), s = std::sin(a);
        strokeLimb(cr, {{32 + c * 16, 33 + s * 16}, {32 + c * 26, 33 + s * 26}}, 3.0, darken(body, 0.2f), kInk, 1.4);
        cairo_arc(cr, 32 + c * 27, 33 + s * 27, 3.6, 0, 2 * kPi);
        fillOutline(cr, rgb(200, 255, 120), kInk, 1.6);
      }
      cairo_arc(cr, 32, 33, 18, 0, 2 * kPi);
      fillGradientOutline(cr, 15, 51, lighten(body, 0.25f), darken(body, 0.45f), kInk, 2.6);
      for (auto [x, y, rad] : {std::tuple{24.0, 40.0, 3.0}, std::tuple{40.0, 42.0, 2.4}, std::tuple{36.0, 24.0, 2.0}})
      {
        cairo_arc(cr, x, y, rad, 0, 2 * kPi);
        setColor(cr, rgba(40, 90, 20, 160));
        cairo_fill(cr);
      }
      for (double ex : {26.0, 38.0})
      {
        cairo_arc(cr, ex, 31, 4.2, 0, 2 * kPi);
        fillOutline(cr, rgb(255, 250, 200), kInk, 1.6);
        cairo_arc(cr, ex + 0.8, 32, 1.8, 0, 2 * kPi);
        setColor(cr, kInk);
        cairo_fill(cr);
      }
      strokeLimb(cr, {{21, 24}, {29, 27}}, 2.2, kInk, kInk, 0.0);
      strokeLimb(cr, {{43, 24}, {35, 27}}, 2.2, kInk, kInk, 0.0);
      break;
    }
    case kIconProto:
    {
      // A chunky sci-fi pistol with a glowing coil, pale so it tints well.
      radialGlow(cr, 32, 32, 32, rgb(255, 255, 255), 0.35);
      roundedRect(cr, 8, 20, 40, 14, 5);
      fillGradientOutline(cr, 20, 34, rgb(250, 250, 255), rgb(170, 175, 195), kInk, 2.4);
      roundedRect(cr, 44, 23, 12, 8, 3);
      fillOutline(cr, rgb(210, 214, 230), kInk, 2.0);
      roundedRect(cr, 14, 32, 11, 18, 3);
      fillGradientOutline(cr, 32, 50, rgb(200, 204, 220), rgb(130, 134, 150), kInk, 2.2);
      for (int i = 0; i < 4; ++i)
      {
        cairo_arc(cr, 26 + i * 6, 27, 3.2, 0, 2 * kPi);
        fillOutline(cr, rgb(255, 255, 255), kInk, 1.4);
      }
      cairo_arc(cr, 57, 27, 3, 0, 2 * kPi);
      setColor(cr, rgb(255, 255, 255));
      cairo_fill(cr);
      break;
    }
    case kIconDuck:
    {
      // Rubber duck wearing white synthwave shades.
      radialGlow(cr, 32, 36, 30, rgb(255, 230, 60), 0.4);
      cairo_save(cr);
      cairo_translate(cr, 30, 42);
      cairo_scale(cr, 1.0, 0.7);
      cairo_arc(cr, 0, 0, 20, 0, 2 * kPi);
      cairo_restore(cr);
      fillGradientOutline(cr, 28, 56, rgb(255, 236, 90), rgb(240, 170, 20), kInk, 2.4);
      cairo_arc(cr, 38, 24, 12, 0, 2 * kPi);
      fillGradientOutline(cr, 12, 36, rgb(255, 240, 110), rgb(245, 190, 30), kInk, 2.4);
      cairo_move_to(cr, 48, 24);
      cairo_line_to(cr, 60, 27);
      cairo_line_to(cr, 48, 30);
      cairo_close_path(cr);
      fillOutline(cr, rgb(255, 130, 30), kInk, 2.0);
      roundedRect(cr, 32, 18, 16, 6, 2);
      fillOutline(cr, rgb(255, 255, 255), kInk, 1.6);
      cairo_move_to(cr, 14, 40);
      cairo_curve_to(cr, 20, 34, 28, 36, 32, 42);
      setColor(cr, rgba(200, 130, 10, 200));
      cairo_set_line_width(cr, 2.0);
      cairo_stroke(cr);
      break;
    }
    case kIconCamera:
    {
      // A hidden camera: a camcorder with a lens and a red tally light.
      roundedRect(cr, 10, 20, 34, 24, 5);
      fillGradientOutline(cr, 20, 44, rgb(90, 90, 110), rgb(40, 40, 54), kInk, 2.4);
      cairo_arc(cr, 48, 32, 10, 0, 2 * kPi);
      fillGradientOutline(cr, 22, 42, rgb(120, 130, 160), rgb(30, 30, 44), kInk, 2.4);
      cairo_arc(cr, 48, 32, 5, 0, 2 * kPi);
      setColor(cr, rgb(120, 200, 255));
      cairo_fill(cr);
      cairo_arc(cr, 46, 30, 1.8, 0, 2 * kPi);
      setColor(cr, rgb(255, 255, 255));
      cairo_fill(cr);
      cairo_arc(cr, 17, 16, 4, 0, 2 * kPi);
      fillOutline(cr, rgb(255, 40, 50), kInk, 1.6);
      roundedRect(cr, 22, 12, 14, 8, 2);
      fillOutline(cr, rgb(70, 70, 86), kInk, 1.8);
      break;
    }
    default:
    {
      // Letters G, U, N in a glowing badge.
      static const char* const kLetters[3] = {"G", "U", "N"};
      const char* letter = kLetters[(icon - kIconLetterG) % 3];
      radialGlow(cr, 32, 32, 34, t.accentA, 0.5);
      cairo_arc(cr, 32, 32, 25, 0, 2 * kPi);
      fillGradientOutline(cr, 7, 57, lighten(t.accentA, 0.25f), darken(t.accentA, 0.35f), kInk, 2.6);
      cairo_arc(cr, 32, 32, 20, 0, 2 * kPi);
      setColor(cr, rgba(255, 255, 255, 70));
      cairo_set_line_width(cr, 2.0);
      cairo_stroke(cr);
      cairo_select_font_face(cr, "DejaVu Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
      cairo_set_font_size(cr, 34);
      cairo_text_extents_t ext;
      cairo_text_extents(cr, letter, &ext);
      cairo_move_to(cr, 32 - ext.width / 2 - ext.x_bearing, 32 - ext.height / 2 - ext.y_bearing);
      cairo_text_path(cr, letter);
      setColor(cr, kInk);
      cairo_set_line_width(cr, 5.0);
      cairo_stroke_preserve(cr);
      setColor(cr, rgb(255, 255, 255));
      cairo_fill(cr);
      break;
    }
  }
  return img.toTexture(r, kItemOff, kItemOff);
}

// --- Climbables, force fields, beacons ---------------------------------------

Texture bakeLadder(const Renderer& r, const Theme& t)
{
  // One block of ladder; the rails straddle the left cell column, so the
  // texture is drawn 16 px left of its block.
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  const Color rail = t.id == ThemeId::LostTemple ? rgb(150, 104, 60) : lighten(t.rockLight, 0.15f);
  const Color rung = t.id == ThemeId::LostTemple ? rgb(186, 140, 84) : t.rockLight;
  for (double y : {12.0, 44.0})
  {
    roundedRect(cr, 12, y - 3, 40, 7, 3);
    fillGradientOutline(cr, y - 3, y + 4, lighten(rung, 0.2f), darken(rung, 0.2f), kInk, 2.0);
  }
  for (double x : {10.0, 48.0})
  {
    cairo_rectangle(cr, x, -2, 7, 68);
    fillGradientOutline(cr, 0, 64, lighten(rail, 0.15f), darken(rail, 0.2f), kInk, 2.0);
  }
  if (t.id == ThemeId::NeonOverdrive)
    for (double y : {12.0, 44.0})
    {
      roundedRect(cr, 20, y - 1, 24, 2, 1);
      setColor(cr, withAlpha(t.platform, 200));
      cairo_fill(cr);
    }
  return img.toTexture(r, 16.0f, 0.0f);
}

Texture bakePipe(const Renderer& r, const Theme& t)
{
  // A hang bar across the top cell row of its block.
  VectorImage img(64, 40);
  cairo_t* cr = img.cr();
  const Color c = t.id == ThemeId::LostTemple ? rgb(120, 150, 90) : lighten(t.rockLight, 0.25f);
  cairo_rectangle(cr, -2, 11, 68, 11);
  fillGradientOutline(cr, 11, 22, lighten(c, 0.35f), darken(c, 0.35f), kInk, 2.2);
  strokeLimb(cr, {{0, 14}, {64, 14}}, 1.6, rgba(255, 255, 255, 120), kInk, 0.0);
  roundedRect(cr, 26, 8, 12, 17, 3);
  fillGradientOutline(cr, 8, 25, lighten(c, 0.1f), darken(c, 0.45f), kInk, 2.0);
  cairo_rectangle(cr, 30, -4, 4, 12);
  fillOutline(cr, darken(c, 0.4f), kInk, 1.6);
  return img.toTexture(r, 0.0f, 0.0f);
}

Texture bakeFieldEmitter(const Renderer& r, const Theme& t)
{
  VectorImage img(64, 20);
  cairo_t* cr = img.cr();
  roundedRect(cr, 4, 2, 56, 16, 5);
  fillGradientOutline(cr, 2, 18, lighten(t.rockLight, 0.2f), t.rockDark, kInk, 2.2);
  for (int i = 0; i < 3; ++i)
  {
    cairo_arc(cr, 18 + i * 14, 10, 3, 0, 2 * kPi);
    setColor(cr, rgb(255, 80, 80));
    cairo_fill(cr);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

Texture bakeFieldBeam(const Renderer& r)
{
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  cairo_pattern_t* p = cairo_pattern_create_linear(0, 0, 64, 0);
  cairo_pattern_add_color_stop_rgba(p, 0.0, 1, 1, 1, 0.0);
  cairo_pattern_add_color_stop_rgba(p, 0.3, 1, 1, 1, 0.35);
  cairo_pattern_add_color_stop_rgba(p, 0.5, 1, 1, 1, 0.95);
  cairo_pattern_add_color_stop_rgba(p, 0.7, 1, 1, 1, 0.35);
  cairo_pattern_add_color_stop_rgba(p, 1.0, 1, 1, 1, 0.0);
  cairo_set_source(cr, p);
  cairo_paint(cr);
  cairo_pattern_destroy(p);
  for (int i = 0; i < 6; ++i)
  {
    strokeLimb(cr, {{24 + (i % 3) * 8.0, i * 11.0}, {28 + ((i + 1) % 3) * 6.0, i * 11.0 + 8}}, 1.6, rgba(255, 255, 255, 220), kInk, 0.0);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

Texture bakeBeacon(const Renderer& r, const Theme& t, bool on)
{
  // 2x4 cells (64x128) with a margin of 16 for the glow.
  VectorImage img(96, 160);
  cairo_t* cr = img.cr();
  cairo_translate(cr, 16, 16);
  const Color light = on ? t.accentB : rgb(110, 110, 130);
  if (on)
    radialGlow(cr, 32, 26, 40, light, 0.6);
  roundedRect(cr, 10, 104, 44, 24, 6);
  fillGradientOutline(cr, 104, 128, lighten(t.rockLight, 0.15f), t.rockDark, kInk, 2.4);
  roundedRect(cr, 26, 40, 12, 66, 4);
  fillGradientOutline(cr, 40, 106, t.rockLight, darken(t.rockDark, 0.2f), kInk, 2.2);
  for (int i = 0; i < 3; ++i)
  {
    roundedRect(cr, 28, 52 + i * 16, 8, 6, 2);
    setColor(cr, on ? light : rgb(70, 70, 86));
    cairo_fill(cr);
  }
  cairo_arc(cr, 32, 26, 15, 0, 2 * kPi);
  fillGradientOutline(cr, 11, 41, lighten(light, 0.5f), darken(light, 0.25f), kInk, 2.4);
  cairo_arc(cr, 27, 21, 4, 0, 2 * kPi);
  setColor(cr, rgba(255, 255, 255, on ? 230 : 120));
  cairo_fill(cr);
  return img.toTexture(r, 16.0f, 16.0f);
}

// --- Projectiles -------------------------------------------------------------

Texture bakeShot(const Renderer& r, ShotStyle style, Color enemyEye)
{
  switch (style)
  {
    case ShotStyle::Normal:
    {
      VectorImage img(72, 28);
      cairo_t* cr = img.cr();
      radialGlow(cr, 46, 14, 14, rgb(255, 190, 70), 0.8);
      cairo_move_to(cr, 4, 14);
      cairo_curve_to(cr, 20, 8, 44, 7, 58, 9);
      cairo_curve_to(cr, 64, 10, 64, 18, 58, 19);
      cairo_curve_to(cr, 44, 21, 20, 20, 4, 14);
      cairo_close_path(cr);
      fillGradientOutline(cr, 7, 21, rgb(255, 255, 230), rgb(255, 170, 40), kInk, 0.0);
      cairo_arc(cr, 54, 14, 3.5, 0, 2 * kPi);
      setColor(cr, rgb(255, 255, 255));
      cairo_fill(cr);
      return img.toTexture(r, 36.0f, 14.0f);
    }
    case ShotStyle::Laser:
    {
      VectorImage img(112, 24);
      cairo_t* cr = img.cr();
      roundedRect(cr, 2, 6, 108, 12, 6);
      setColor(cr, rgba(60, 220, 255, 150));
      cairo_fill(cr);
      roundedRect(cr, 6, 9, 100, 6, 3);
      setColor(cr, rgb(235, 255, 255));
      cairo_fill(cr);
      return img.toTexture(r, 56.0f, 12.0f);
    }
    case ShotStyle::Rocket:
    {
      VectorImage img(104, 36);
      cairo_t* cr = img.cr();
      cairo_move_to(cr, 2, 18);
      cairo_curve_to(cr, 14, 8, 26, 10, 34, 13);
      cairo_line_to(cr, 34, 23);
      cairo_curve_to(cr, 26, 26, 14, 28, 2, 18);
      cairo_close_path(cr);
      fillGradientOutline(cr, 8, 28, rgb(255, 240, 140), rgb(255, 100, 30), kInk, 0.0);
      roundedRect(cr, 32, 11, 52, 14, 6);
      fillGradientOutline(cr, 11, 25, rgb(220, 226, 236), rgb(120, 128, 150), kInk, 2.2);
      cairo_move_to(cr, 82, 11);
      cairo_line_to(cr, 100, 18);
      cairo_line_to(cr, 82, 25);
      cairo_close_path(cr);
      fillOutline(cr, rgb(240, 70, 60), kInk, 2.2);
      cairo_move_to(cr, 36, 11);
      cairo_line_to(cr, 44, 3);
      cairo_line_to(cr, 50, 11);
      cairo_close_path(cr);
      fillOutline(cr, rgb(240, 70, 60), kInk, 1.8);
      cairo_move_to(cr, 36, 25);
      cairo_line_to(cr, 44, 33);
      cairo_line_to(cr, 50, 25);
      cairo_close_path(cr);
      fillOutline(cr, rgb(240, 70, 60), kInk, 1.8);
      return img.toTexture(r, 52.0f, 18.0f);
    }
    case ShotStyle::Flame:
    {
      VectorImage img(80, 48);
      cairo_t* cr = img.cr();
      radialGlow(cr, 46, 24, 30, rgb(255, 140, 30), 0.8);
      cairo_move_to(cr, 4, 24);
      cairo_curve_to(cr, 20, 10, 40, 4, 62, 10);
      cairo_curve_to(cr, 78, 16, 78, 32, 62, 38);
      cairo_curve_to(cr, 40, 44, 20, 38, 4, 24);
      cairo_close_path(cr);
      fillGradientOutline(cr, 6, 42, rgb(255, 250, 170), rgb(255, 80, 20), kInk, 0.0);
      cairo_arc(cr, 56, 24, 9, 0, 2 * kPi);
      setColor(cr, rgba(255, 255, 230, 230));
      cairo_fill(cr);
      return img.toTexture(r, 40.0f, 24.0f);
    }
    case ShotStyle::Enemy:
    default:
    {
      VectorImage img(40, 40);
      cairo_t* cr = img.cr();
      radialGlow(cr, 20, 20, 20, enemyEye, 1.0);
      cairo_arc(cr, 20, 20, 8, 0, 2 * kPi);
      setColor(cr, rgb(255, 255, 255));
      cairo_fill(cr);
      return img.toTexture(r, 20.0f, 20.0f);
    }
  }
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
    case ThemeId::LostTemple:
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
    case ThemeId::LostTemple:
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

// A puff of cloud for one block of one-way platform; neighbours overlap
// (the texture is wider than the block) so a row reads as one cloud.
Texture bakeCloud(const Renderer& r, const Theme& t)
{
  VectorImage img(96, 56);
  cairo_t* cr = img.cr();
  radialGlow(cr, 48, 24, 46, lerpColor(t.skyBottom, rgb(255, 255, 255), 0.6f), 0.25);
  const double puffs[4][3] = {{26, 26, 17}, {48, 18, 22}, {70, 26, 17}, {48, 32, 18}};
  for (const auto& p : puffs)
    cairo_arc(cr, p[0], p[1], p[2], 0, 6.2831853);
  cairo_pattern_t* g = cairo_pattern_create_linear(0, 0, 0, 50);
  const Color top = rgb(255, 255, 255), bottom = lerpColor(t.skyMid, rgb(255, 255, 255), 0.55f);
  cairo_pattern_add_color_stop_rgba(g, 0.0, redOf(top) / 255.0, greenOf(top) / 255.0, blueOf(top) / 255.0, 0.96);
  cairo_pattern_add_color_stop_rgba(g, 1.0, redOf(bottom) / 255.0, greenOf(bottom) / 255.0, blueOf(bottom) / 255.0, 0.92);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
  return img.toTexture(r, 16.0f, 10.0f);
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
    case ThemeId::LostTemple:
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

// --- Club Laserdisc's interior (theme look "club") -----------------------------

bool isClub(const Theme& t) { return std::string_view(t.look) == "club"; }

// The room itself: a black ceiling with a lighting rig, haze and frozen
// beams of light.
Texture bakeClubSky(const Renderer& r, const Theme& t)
{
  VectorImage img(kScreenW, kScreenH);
  cairo_t* cr = img.cr();
  verticalGradient(cr, kScreenW, kScreenH, {{0.0, rgb(4, 2, 10)}, {0.45, t.skyTop}, {1.0, t.skyMid}});
  Rng rng(4242u);
  // Beams fanning down from the rig.
  for (int i = 0; i < 9; ++i)
  {
    const double x0 = 80 + i * 140.0 + rng.range(-30, 30);
    const double spread = rng.range(60, 160), lean = rng.range(-260, 260);
    const Color c = i % 3 == 0 ? t.trim : (i % 3 == 1 ? t.platform : t.hazard);
    cairo_move_to(cr, x0, 60);
    cairo_line_to(cr, x0 + lean - spread, kScreenH);
    cairo_line_to(cr, x0 + lean + spread, kScreenH);
    cairo_close_path(cr);
    cairo_pattern_t* p = cairo_pattern_create_linear(0, 60, 0, kScreenH);
    cairo_pattern_add_color_stop_rgba(p, 0, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, 0.22);
    cairo_pattern_add_color_stop_rgba(p, 1, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, 0.0);
    cairo_set_source(cr, p);
    cairo_fill(cr);
    cairo_pattern_destroy(p);
  }
  // Haze.
  for (int i = 0; i < 14; ++i)
    radialGlow(cr, rng.range(0, kScreenW), rng.range(200, kScreenH), rng.range(120, 260), t.skyBottom, 0.12);
  // The rig: a truss with cans.
  cairo_rectangle(cr, 0, 40, kScreenW, 14);
  setColor(cr, rgb(40, 36, 52));
  cairo_fill(cr);
  for (int x = 0; x < kScreenW; x += 28)
  {
    strokeLimb(cr, {{double(x), 40}, {double(x) + 14, 54}}, 2, rgb(70, 64, 90), kInk, 0.0);
    strokeLimb(cr, {{double(x) + 14, 54}, {double(x) + 28, 40}}, 2, rgb(70, 64, 90), kInk, 0.0);
  }
  for (int i = 0; i < 9; ++i)
  {
    const double x = 80 + i * 140.0;
    roundedRect(cr, x - 12, 52, 24, 20, 4);
    setColor(cr, rgb(20, 18, 28));
    cairo_fill(cr);
    radialGlow(cr, x, 70, 22, i % 2 ? t.trim : t.platform, 0.9);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// Far wall: stacks of speakers and acoustic panels.
Texture bakeClubFar(const Renderer& r, const Theme& t)
{
  VectorImage img(kLayerW, kScreenH);
  cairo_t* cr = img.cr();
  Rng rng(77u);
  for (int px = 0; px < kLayerW; px += 160)
  {
    // Acoustic foam panels.
    for (int py = 120; py < kScreenH; py += 80)
    {
      roundedRect(cr, px + 6, py + 6, 148, 68, 6);
      setColor(cr, lerpColor(t.farLayer, rgb(0, 0, 0), 0.35f + 0.2f * float((px / 160 + py / 80) % 2)));
      cairo_fill(cr);
    }
  }
  double x = 40;
  while (x < kLayerW - 200)
  {
    const int cabs = rng.irange(2, 5);
    const double w = rng.range(110, 150);
    for (int k = 0; k < cabs; ++k)
    {
      const double h = w * 0.8, y = kScreenH - (k + 1) * h;
      roundedRect(cr, x, y, w, h - 4, 6);
      setColor(cr, rgb(16, 14, 24));
      cairo_fill(cr);
      for (int cone = 0; cone < 2; ++cone)
      {
        const double cx = x + w * 0.5, cy = y + h * (cone ? 0.68 : 0.3), rad = h * (cone ? 0.26 : 0.16);
        cairo_arc(cr, cx, cy, rad, 0, 2 * kPi);
        setColor(cr, rgb(40, 36, 56));
        cairo_fill(cr);
        cairo_arc(cr, cx, cy, rad * 0.4, 0, 2 * kPi);
        setColor(cr, withAlpha(t.trim, 90));
        cairo_fill(cr);
      }
    }
    x += w + rng.range(140, 320);
  }
  // A neon strip along the wall.
  cairo_rectangle(cr, 0, 300, kLayerW, 4);
  setColor(cr, withAlpha(t.platform, 120));
  cairo_fill(cr);
  return img.toTexture(r, 0.0f, 0.0f);
}

// Near: the crowd, arms up, as silhouettes against the floor lights.
Texture bakeClubNear(const Renderer& r, const Theme& t)
{
  VectorImage img(kLayerW, kScreenH);
  cairo_t* cr = img.cr();
  Rng rng(31u);
  const Color body = rgb(10, 6, 20);
  for (double x = 10; x < kLayerW - 30; x += rng.range(34, 60))
  {
    const double h = rng.range(150, 210), base = kScreenH;
    const double head = base - h;
    roundedRect(cr, x - 16, head + 26, 32, h, 12);
    setColor(cr, body);
    cairo_fill(cr);
    cairo_arc(cr, x, head + 12, 14, 0, 2 * kPi);
    cairo_fill(cr);
    if (rng.uniform() < 0.55f)
    {
      const double side = rng.uniform() < 0.5f ? -1 : 1;
      strokeLimb(cr, {{x + side * 12, head + 34}, {x + side * 26, head - 4}, {x + side * 20, head - 40}}, 9, body, body, 0.0);
      if (rng.uniform() < 0.4f)
      {
        strokeLimb(cr, {{x + side * 20, head - 40}, {x + side * 24, head - 64}}, 5, rgb(120, 255, 90), rgb(120, 255, 90), 0.0);
        radialGlow(cr, x + side * 22, head - 52, 20, rgb(120, 255, 90), 0.6);
      }
    }
  }
  verticalGradient(cr, kLayerW, kScreenH, {{0.0, rgba(0, 0, 0, 0)}, {0.8, rgba(0, 0, 0, 0)}, {1.0, withAlpha(t.trim, 50)}});
  return img.toTexture(r, 0.0f, 0.0f);
}

// --- Sludge Line's storm drains (theme look "sewer") ----------------------------

bool isSewer(const Theme& t) { return std::string_view(t.look) == "sewer"; }

// The vault: wet brick fading into the dark, a grate of light far above.
Texture bakeSewerSky(const Renderer& r, const Theme& t)
{
  VectorImage img(kScreenW, kScreenH);
  cairo_t* cr = img.cr();
  verticalGradient(cr, kScreenW, kScreenH, {{0.0, rgb(4, 6, 4)}, {0.5, t.skyTop}, {1.0, t.skyMid}});
  Rng rng(9191u);
  for (int y = 0; y < kScreenH; y += 22)
    for (int x = (y / 22) % 2 ? -24 : 0; x < kScreenW; x += 48)
    {
      roundedRect(cr, x + 2, y + 2, 44, 18, 3);
      setColor(cr, withAlpha(lerpColor(t.skyMid, t.skyBottom, rng.uniform() * 0.6f), 70 + rng.irange(0, 40)));
      cairo_fill(cr);
    }
  // Light through a street grate, and its shafts.
  for (int i = 0; i < 3; ++i)
  {
    const double x = 200 + i * 420.0;
    cairo_move_to(cr, x, 0);
    cairo_line_to(cr, x + 60, 0);
    cairo_line_to(cr, x + 160, kScreenH);
    cairo_line_to(cr, x - 40, kScreenH);
    cairo_close_path(cr);
    cairo_pattern_t* p = cairo_pattern_create_linear(0, 0, 0, kScreenH);
    cairo_pattern_add_color_stop_rgba(p, 0, 0.8, 1.0, 0.7, 0.16);
    cairo_pattern_add_color_stop_rgba(p, 1, 0.8, 1.0, 0.7, 0.0);
    cairo_set_source(cr, p);
    cairo_fill(cr);
    cairo_pattern_destroy(p);
  }
  for (int i = 0; i < 10; ++i)
    radialGlow(cr, rng.range(0, kScreenW), rng.range(300, kScreenH), rng.range(100, 240), t.trim, 0.06);
  return img.toTexture(r, 0.0f, 0.0f);
}

// Far: arched tunnel mouths and a run of big pipes.
Texture bakeSewerFar(const Renderer& r, const Theme& t)
{
  VectorImage img(kLayerW, kScreenH);
  cairo_t* cr = img.cr();
  Rng rng(313u);
  for (double x = 60; x < kLayerW - 200; x += rng.range(320, 520))
  {
    const double w = rng.range(160, 240), top = rng.range(260, 380);
    cairo_move_to(cr, x, kScreenH);
    cairo_line_to(cr, x, top + w * 0.5);
    cairo_arc(cr, x + w * 0.5, top + w * 0.5, w * 0.5, kPi, 2 * kPi);
    cairo_line_to(cr, x + w, kScreenH);
    cairo_close_path(cr);
    setColor(cr, rgb(6, 10, 6));
    cairo_fill_preserve(cr);
    cairo_set_line_width(cr, 14);
    setColor(cr, withAlpha(t.farLayer, 255));
    cairo_stroke(cr);
    radialGlow(cr, x + w * 0.5, kScreenH - 60, w * 0.6, t.trim, 0.12);
  }
  for (int k = 0; k < 3; ++k)
  {
    const double y = 140 + k * 46.0;
    cairo_rectangle(cr, 0, y, kLayerW, 26 - k * 4);
    cairo_pattern_t* p = cairo_pattern_create_linear(0, y, 0, y + 26);
    cairo_pattern_add_color_stop_rgb(p, 0, 0.32, 0.36, 0.3);
    cairo_pattern_add_color_stop_rgb(p, 1, 0.12, 0.14, 0.11);
    cairo_set_source(cr, p);
    cairo_fill(cr);
    cairo_pattern_destroy(p);
    for (double x = rng.range(0, 200); x < kLayerW; x += rng.range(240, 400))
    {
      cairo_rectangle(cr, x, y - 4, 12, 34 - k * 4);
      setColor(cr, rgb(70, 76, 64));
      cairo_fill(cr);
    }
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// Near: dripping pipes and hanging moss.
Texture bakeSewerNear(const Renderer& r, const Theme& t)
{
  VectorImage img(kLayerW, kScreenH);
  cairo_t* cr = img.cr();
  Rng rng(808u);
  for (double x = 30; x < kLayerW - 60; x += rng.range(180, 360))
  {
    const double w = rng.range(26, 44);
    cairo_rectangle(cr, x, 0, w, rng.range(180, 420));
    setColor(cr, rgb(30, 36, 28));
    cairo_fill(cr);
    for (int d = 0; d < 3; ++d)
    {
      const double dx = x + rng.range(4, w - 4), dy = rng.range(200, 600);
      cairo_arc(cr, dx, dy, 3, 0, 2 * kPi);
      setColor(cr, withAlpha(t.trimGlow, 150));
      cairo_fill(cr);
    }
  }
  for (double x = 0; x < kLayerW; x += rng.range(20, 50))
  {
    const double len = rng.range(20, 90);
    strokeLimb(cr, {{x, 0}, {x + rng.range(-6, 6), len}}, 4, withAlpha(t.nearLayer, 220), withAlpha(t.nearLayer, 220), 0.0);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

Texture bakeSky(const Renderer& r, const Theme& t)
{
  if (isClub(t))
    return bakeClubSky(r, t);
  if (isSewer(t))
    return bakeSewerSky(r, t);
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
    case ThemeId::LostTemple:
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
  if (isClub(t))
    return bakeClubFar(r, t);
  if (isSewer(t))
    return bakeSewerFar(r, t);
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
    case ThemeId::LostTemple:
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
  if (isClub(t))
    return bakeClubNear(r, t);
  if (isSewer(t))
    return bakeSewerNear(r, t);
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
    case ThemeId::LostTemple:
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
    case ThemeId::LostTemple:
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
    case ThemeId::LostTemple:
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
  {
    art.characters[std::size_t(i)] = buildCharacter(r, i);
    art.characterColor[std::size_t(i)] = lookFor(i).top;
  }
  for (int f = 0; f < 2; ++f)
  {
    art.walker[std::size_t(f)] = bakeWalker(r, theme, f);
    art.flyer[std::size_t(f)] = bakeFlyer(r, theme, f);
  }
  art.turret = bakeTurret(r, theme);

  art.gemColor = {theme.accentA, theme.accentB, rgb(110, 255, 130), rgb(255, 110, 230)};
  art.boxColor = {rgb(235, 240, 255), rgb(70, 150, 255), rgb(80, 230, 110)};
  for (std::size_t i = 0; i < 3; ++i)
    art.boxes[i] = bakeItemBox(r, art.boxColor[i]);
  for (int i = 0; i < kItemIcons; ++i)
  {
    const Color gem = i >= kIconGem0 && i <= kIconGem3 ? art.gemColor[std::size_t(i - kIconGem0)] : 0;
    art.items[std::size_t(i)] = bakeItemIcon(r, theme, i, gem);
  }

  for (int v = 0; v < 3; ++v)
    art.solid[std::size_t(v)] = bakeSolid(r, theme, v);
  art.solidTop = bakeSolidTop(r, theme);
  art.platform = bakePlatform(r, theme);
  art.cloud = bakeCloud(r, theme);
  art.spikes = bakeSpikes(r, theme);
  art.ladder = bakeLadder(r, theme);
  art.pipe = bakePipe(r, theme);
  art.fieldEmitter = bakeFieldEmitter(r, theme);
  art.fieldBeam = bakeFieldBeam(r);
  art.beaconOff = bakeBeacon(r, theme, false);
  art.beaconOn = bakeBeacon(r, theme, true);

  art.shotNormal = bakeShot(r, ShotStyle::Normal, theme.enemyEye);
  art.shotLaser = bakeShot(r, ShotStyle::Laser, theme.enemyEye);
  art.shotRocket = bakeShot(r, ShotStyle::Rocket, theme.enemyEye);
  art.shotFlame = bakeShot(r, ShotStyle::Flame, theme.enemyEye);
  art.enemyShot = bakeShot(r, ShotStyle::Enemy, theme.enemyEye);

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
  art.heartFull = bakeHeart(r, 30, rgb(255, 90, 110), rgb(200, 20, 50), true);
  art.heartEmpty = bakeHeart(r, 30, rgb(80, 80, 96), rgb(50, 50, 60), false);
  const int panelW[5] = {kHudHealthW, kHudWeaponW, kHudInventoryW, kHudLettersW, kHudScoreW};
  for (std::size_t i = 0; i < 5; ++i)
    art.hudPanels[i] = makePanel(r, panelW[i], kHudPanelH, rgba(8, 6, 20, 175), withAlpha(theme.accentA, 150), 14);
  art.hudSlot = makePanel(r, 44, 44, rgba(255, 255, 255, 18), rgba(255, 255, 255, 60), 9);
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
    case ThemeId::LostTemple:
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
  // The teleporter art is a 64x128 frame; scale it up to the 4x6 cell exit.
  constexpr float kScale = 1.5f;
  const float left = x + 64.0f - 32.0f * kScale;
  const float top = y - 128.0f * kScale;
  DrawOpts beam;
  beam.blend = Blend::Add;
  beam.tint = t.accentB;
  beam.alpha = 0.45f + 0.2f * std::sin(float(frame) * 0.1f);
  beam.scale = kScale;
  r.draw(art.exitBeam, left, top + 8 * kScale, beam);
  for (int i = 0; i < 3; ++i)
  {
    const float phase = std::fmod(float(frame) * 0.012f + float(i) / 3.0f, 1.0f);
    drawGlow(r, art, x + 64, y - 18 - phase * 150, 40, t.accentB, 0.5f * (1.0f - phase));
  }
  drawGlow(r, art, x + 64, y - 96, 150, t.accentB, 0.25f);
  DrawOpts base;
  base.scale = kScale;
  r.draw(art.exitBase, left, top, base);
  r.drawText("EXIT", x + 64, top - 50, {26.0f, t.accentA, rgb(20, 16, 28)}, Align::Center);
}

} // namespace gr
