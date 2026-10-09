#include "frontend/touch_controls.hpp"

#include "render/vector.hpp"

#include <algorithm>
#include <cmath>

namespace gr
{

namespace
{

constexpr float kDpadX = 175.0f;
constexpr float kDpadY = 545.0f;
constexpr float kDpadR = 125.0f;
// A thumb this close to the d-pad's centre steers nowhere.
constexpr float kDeadZone = 28.0f;
// A touch counts for a button within this multiple of its drawn radius.
constexpr float kHitSlop = 1.35f;

constexpr Color kNeon = rgb(46, 242, 255);
constexpr Color kPink = rgb(255, 46, 196);

void ring(cairo_t* cr, double cx, double cy, double r, Color c)
{
  cairo_arc(cr, cx, cy, r, 0.0, 2.0 * M_PI);
  setColor(cr, withAlpha(rgb(10, 8, 30), 150));
  cairo_fill_preserve(cr);
  setColor(cr, c);
  cairo_set_line_width(cr, 4.0);
  cairo_stroke(cr);
}

void triangle(cairo_t* cr, double cx, double cy, double size, double angle)
{
  cairo_save(cr);
  cairo_translate(cr, cx, cy);
  cairo_rotate(cr, angle);
  cairo_move_to(cr, size, 0.0);
  cairo_line_to(cr, -size * 0.6, -size * 0.8);
  cairo_line_to(cr, -size * 0.6, size * 0.8);
  cairo_close_path(cr);
  cairo_restore(cr);
  cairo_fill(cr);
}

Texture bakeDpad(Renderer& r)
{
  const int size = int(kDpadR * 2.0f) + 8;
  const double c = size * 0.5;
  VectorImage img(size, size);
  cairo_t* cr = img.cr();
  ring(cr, c, c, kDpadR, kNeon);
  setColor(cr, kNeon);
  for (int i = 0; i < 4; ++i)
  {
    const double a = i * M_PI * 0.5;
    triangle(cr, c + std::cos(a) * kDpadR * 0.72, c + std::sin(a) * kDpadR * 0.72, 16.0, a);
  }
  return img.toTexture(r, float(c), float(c));
}

Texture bakeNub(Renderer& r)
{
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  radialGlow(cr, 32.0, 32.0, 30.0, kNeon, 0.9);
  return img.toTexture(r, 32.0f, 32.0f);
}

// Icons rather than letters, so the overlay needs no font.
Texture bakeButton(Renderer& r, int id, float radius)
{
  const int size = int(radius * 2.0f) + 8;
  const double c = size * 0.5;
  VectorImage img(size, size);
  cairo_t* cr = img.cr();
  const Color col = id == 1 ? kPink : kNeon;
  ring(cr, c, c, radius, col);
  setColor(cr, col);
  const double s = radius * 0.42;
  switch (id)
  {
    case 0: // jump: a chevron up
      cairo_set_line_width(cr, radius * 0.16);
      cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
      cairo_move_to(cr, c - s, c + s * 0.45);
      cairo_line_to(cr, c, c - s * 0.55);
      cairo_line_to(cr, c + s, c + s * 0.45);
      cairo_stroke(cr);
      break;
    case 1: // fire: a muzzle burst
      for (int i = 0; i < 8; ++i)
      {
        const double a = i * M_PI / 4.0;
        const double rr = (i % 2) ? s * 0.55 : s;
        cairo_line_to(cr, c + std::cos(a) * rr, c + std::sin(a) * rr);
        const double b = a + M_PI / 8.0;
        cairo_line_to(cr, c + std::cos(b) * s * 0.3, c + std::sin(b) * s * 0.3);
      }
      cairo_close_path(cr);
      cairo_fill(cr);
      break;
    case 2: // switch runner: two arrows
      triangle(cr, c + s * 0.35, c - s * 0.4, s * 0.55, 0.0);
      triangle(cr, c - s * 0.35, c + s * 0.4, s * 0.55, M_PI);
      break;
    default: // pause: two bars
      cairo_rectangle(cr, c - s * 0.6, c - s * 0.7, s * 0.4, s * 1.4);
      cairo_rectangle(cr, c + s * 0.2, c - s * 0.7, s * 0.4, s * 1.4);
      cairo_fill(cr);
      break;
  }
  return img.toTexture(r, float(c), float(c));
}

} // namespace

const TouchControls::ButtonSpot TouchControls::kButtons[kButtonCount] = {
  {1165.0f, 590.0f, 72.0f}, // jump
  {1012.0f, 640.0f, 60.0f}, // fire
  {1165.0f, 440.0f, 44.0f}, // switch runner
  {1228.0f, 120.0f, 34.0f}, // pause
};

TouchControls::TouchControls(Renderer& renderer, bool visible)
  : mVisible(visible)
  , mDpad(bakeDpad(renderer))
  , mDpadNub(bakeNub(renderer))
{
  for (int b = 0; b < kButtonCount; ++b)
    mButtonTex[b] = bakeButton(renderer, b, kButtons[b].r);
}

void TouchControls::handleEvent(const SDL_Event& ev, SDL_Window* window)
{
  switch (ev.type)
  {
    case SDL_FINGERDOWN:
    case SDL_FINGERMOTION:
    case SDL_FINGERUP:
    {
      // Finger positions are 0..1 of the window; the game draws in a
      // letterboxed 1280x720 logical space.
      int w = 0, h = 0;
      SDL_GetWindowSize(window, &w, &h);
      float lx = 0.0f, ly = 0.0f;
      SDL_RenderWindowToLogical(
        SDL_GetRenderer(window), int(ev.tfinger.x * float(w)), int(ev.tfinger.y * float(h)), &lx, &ly);
      if (ev.type == SDL_FINGERDOWN)
        press(ev.tfinger.fingerId, lx, ly);
      else if (ev.type == SDL_FINGERMOTION)
        move(ev.tfinger.fingerId, lx, ly);
      else
        lift(ev.tfinger.fingerId);
      break;
    }
    case SDL_KEYDOWN:
      // Android's back key is a soft key on the touch screen itself.
      if (ev.key.keysym.scancode != SDL_SCANCODE_AC_BACK)
        mVisible = false;
      break;
    case SDL_CONTROLLERBUTTONDOWN:
      mVisible = false;
      break;
    default:
      break;
  }
}

void TouchControls::injectFinger(SDL_FingerID id, float x, float y)
{
  press(id, x, y);
}

void TouchControls::press(SDL_FingerID id, float x, float y)
{
  mVisible = true;
  lift(id);
  mFingers.push_back({id, x, y, x < float(kScreenW) * 0.5f});
  mTapped = mTapped | held();
}

void TouchControls::move(SDL_FingerID id, float x, float y)
{
  for (auto& f : mFingers)
    if (f.id == id)
    {
      f.x = x;
      f.y = y;
    }
}

void TouchControls::lift(SDL_FingerID id)
{
  mFingers.erase(
    std::remove_if(mFingers.begin(), mFingers.end(), [id](const Finger& f) { return f.id == id; }),
    mFingers.end());
}

void TouchControls::dpadDirections(bool& left, bool& right, bool& up, bool& down) const
{
  for (const auto& f : mFingers)
  {
    if (!f.dpad)
      continue;
    const float dx = f.x - kDpadX;
    const float dy = f.y - kDpadY;
    if (dx * dx + dy * dy < kDeadZone * kDeadZone)
      continue;
    // Eight sectors: a direction counts unless the thumb is more than
    // ~63 degrees away from it, so diagonals press two directions.
    const float ax = std::fabs(dx), ay = std::fabs(dy);
    if (ax * 2.0f > ay)
    {
      left = left || dx < 0.0f;
      right = right || dx > 0.0f;
    }
    if (ay * 2.0f > ax)
    {
      up = up || dy < 0.0f;
      down = down || dy > 0.0f;
    }
  }
}

int TouchControls::buttonAt(float x, float y) const
{
  int best = -1;
  float bestD = 0.0f;
  for (int b = 0; b < kButtonCount; ++b)
  {
    const float dx = x - kButtons[b].x, dy = y - kButtons[b].y;
    const float d = std::sqrt(dx * dx + dy * dy) / kButtons[b].r;
    if (d < kHitSlop && (best < 0 || d < bestD))
    {
      best = b;
      bestD = d;
    }
  }
  return best;
}

bool TouchControls::buttonHeld(int b) const
{
  for (const auto& f : mFingers)
    if (!f.dpad && buttonAt(f.x, f.y) == b)
      return true;
  return false;
}

Input TouchControls::read()
{
  const Input in = held() | mTapped;
  mTapped = Input{};
  return in;
}

Input TouchControls::held() const
{
  Input in;
  dpadDirections(in.left, in.right, in.up, in.down);
  // Same meaning as a pad's buttons: A jumps and confirms, B fires and
  // backs out of menus, Y switches runner, Start pauses.
  in.jump = in.confirm = buttonHeld(kJump);
  in.fire = in.back = buttonHeld(kFire);
  in.swap = buttonHeld(kSwap);
  in.pause = buttonHeld(kPause);
  return in;
}

void TouchControls::draw(Renderer& renderer) const
{
  if (!mVisible)
    return;
  DrawOpts o;
  o.alpha = 0.55f;
  renderer.draw(mDpad, kDpadX, kDpadY, o);
  for (const auto& f : mFingers)
    if (f.dpad)
    {
      // The nub follows the thumb, held inside the ring.
      float dx = f.x - kDpadX, dy = f.y - kDpadY;
      const float d = std::sqrt(dx * dx + dy * dy);
      if (d > kDpadR * 0.8f)
      {
        dx *= kDpadR * 0.8f / d;
        dy *= kDpadR * 0.8f / d;
      }
      DrawOpts n;
      n.blend = Blend::Add;
      renderer.draw(mDpadNub, kDpadX + dx, kDpadY + dy, n);
    }
  for (int b = 0; b < kButtonCount; ++b)
  {
    DrawOpts bo;
    const bool held = buttonHeld(b);
    bo.alpha = held ? 0.95f : 0.55f;
    bo.scale = held ? 0.92f : 1.0f;
    renderer.draw(mButtonTex[b], kButtons[b].x, kButtons[b].y, bo);
  }
}

} // namespace gr
