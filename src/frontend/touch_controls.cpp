#include "frontend/touch_controls.hpp"

#include "render/vector.hpp"

#include <algorithm>
#include <cmath>

namespace gr
{

namespace
{

// Geometry at the medium size, measured from the corner each control hugs.
constexpr float kStickR = 125.0f;
constexpr float kStickFromLeft = 175.0f;
constexpr float kStickFromBottom = 175.0f;
// A thumb this close to the stick's centre steers nowhere.
constexpr float kDeadZone = 26.0f;
// A touch counts for a button within this multiple of its drawn radius.
constexpr float kHitSlop = 1.35f;
// The art is baked at the large size and scaled down.
constexpr float kBakeScale = 1.2f;
constexpr float kSizes[3] = {0.8f, 1.0f, 1.2f};

struct ButtonSpec
{
  float fromRight, fromBottom, r;
};
// jump, fire, switch (bottom right); pause hugs the top right instead.
constexpr ButtonSpec kSpecs[3] = {
  {115.0f, 130.0f, 72.0f},
  {268.0f, 80.0f, 60.0f},
  {115.0f, 280.0f, 44.0f},
};
constexpr float kPauseFromRight = 52.0f;
constexpr float kPauseY = 120.0f;
constexpr float kPauseR = 34.0f;

constexpr Color kNeon = rgb(46, 242, 255);
constexpr Color kPink = rgb(255, 46, 196);
constexpr Color kGold = rgb(255, 214, 64);

void ring(cairo_t* cr, double cx, double cy, double r, Color c)
{
  cairo_arc(cr, cx, cy, r, 0.0, 2.0 * M_PI);
  setColor(cr, withAlpha(rgb(10, 8, 30), 150));
  cairo_fill_preserve(cr);
  setColor(cr, c);
  cairo_set_line_width(cr, 4.0 * kBakeScale);
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

// A ring of radius r (at the medium size), baked large, anchored at its centre.
template <typename Draw>
Texture bake(Renderer& renderer, float r, Draw draw)
{
  const double rr = r * kBakeScale;
  const int size = int(std::ceil(rr * 2.0)) + 10;
  const double c = size * 0.5;
  VectorImage img(size, size);
  draw(img.cr(), c, rr);
  return img.toTexture(renderer, float(c), float(c));
}

Texture bakeStick(Renderer& renderer, bool arrows)
{
  return bake(renderer, kStickR, [arrows](cairo_t* cr, double c, double r) {
    ring(cr, c, c, r, kNeon);
    if (!arrows)
      return;
    setColor(cr, kNeon);
    for (int i = 0; i < 4; ++i)
    {
      const double a = i * M_PI * 0.5;
      triangle(cr, c + std::cos(a) * r * 0.72, c + std::sin(a) * r * 0.72, r * 0.13, a);
    }
  });
}

Texture bakeNub(Renderer& renderer)
{
  return bake(renderer, 32.0f, [](cairo_t* cr, double c, double r) {
    radialGlow(cr, c, c, r, kNeon, 0.9);
    cairo_arc(cr, c, c, r * 0.55, 0.0, 2.0 * M_PI);
    setColor(cr, withAlpha(kNeon, 200));
    cairo_fill(cr);
  });
}

// Icons rather than letters, so the overlay reads the same in any language.
Texture bakeIcon(Renderer& renderer, float r, Color col, int icon)
{
  return bake(renderer, r, [col, icon](cairo_t* cr, double c, double rr) {
    ring(cr, c, c, rr, col);
    setColor(cr, col);
    const double s = rr * 0.42;
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    switch (icon)
    {
      case 0: // jump: a chevron up
        cairo_set_line_width(cr, rr * 0.16);
        cairo_move_to(cr, c - s, c + s * 0.45);
        cairo_line_to(cr, c, c - s * 0.55);
        cairo_line_to(cr, c + s, c + s * 0.45);
        cairo_stroke(cr);
        break;
      case 1: // fire: a muzzle burst
        for (int i = 0; i < 8; ++i)
        {
          const double a = i * M_PI / 4.0;
          const double len = (i % 2) ? s * 0.55 : s;
          cairo_line_to(cr, c + std::cos(a) * len, c + std::sin(a) * len);
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
      case 3: // pause: two bars
        cairo_rectangle(cr, c - s * 0.6, c - s * 0.7, s * 0.4, s * 1.4);
        cairo_rectangle(cr, c + s * 0.2, c - s * 0.7, s * 0.4, s * 1.4);
        cairo_fill(cr);
        break;
      case 4: // OK: a tick
        cairo_set_line_width(cr, rr * 0.16);
        cairo_move_to(cr, c - s, c + s * 0.05);
        cairo_line_to(cr, c - s * 0.25, c + s * 0.75);
        cairo_line_to(cr, c + s, c - s * 0.6);
        cairo_stroke(cr);
        break;
      default: // BACK: an arrow pointing left
        cairo_set_line_width(cr, rr * 0.14);
        cairo_move_to(cr, c + s, c);
        cairo_line_to(cr, c - s * 0.7, c);
        cairo_stroke(cr);
        triangle(cr, c - s * 0.6, c, s * 0.6, M_PI);
        break;
    }
  });
}

} // namespace

TouchControls::TouchControls(Renderer& renderer, bool visible)
  : mVisible(visible)
  , mStick(bakeStick(renderer, false))
  , mDpad(bakeStick(renderer, true))
  , mNub(bakeNub(renderer))
  , mOk(bakeIcon(renderer, kSpecs[0].r, kGold, 4))
  , mBack(bakeIcon(renderer, kSpecs[1].r, kPink, 5))
{
  mPlayIcons[kJump] = bakeIcon(renderer, kSpecs[0].r, kNeon, 0);
  mPlayIcons[kFire] = bakeIcon(renderer, kSpecs[1].r, kPink, 1);
  mPlayIcons[kSwap] = bakeIcon(renderer, kSpecs[2].r, kNeon, 2);
  mPlayIcons[kPause] = bakeIcon(renderer, kPauseR, kNeon, 3);
}

void TouchControls::configure(Layout layout, int size)
{
  mLayout = layout;
  mScale = kSizes[std::clamp(size, 0, 2)];
}

TouchControls::Circle TouchControls::stickHome() const
{
  return {kStickFromLeft * mScale, float(kScreenH) - kStickFromBottom * mScale, kStickR * mScale};
}

TouchControls::Circle TouchControls::button(int b) const
{
  if (b == kPause)
    return {float(kScreenW) - kPauseFromRight, kPauseY, kPauseR * mScale};
  const ButtonSpec& s = kSpecs[b];
  return {float(kScreenW) - s.fromRight * mScale, float(kScreenH) - s.fromBottom * mScale, s.r * mScale};
}

bool TouchControls::buttonShown(int b) const
{
  return mLayout == Layout::Play || b == kJump || b == kFire;
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

void TouchControls::press(SDL_FingerID id, float x, float y)
{
  mVisible = true;
  lift(id);
  Finger f{id, x, y, x < float(kScreenW) * 0.5f, 0.0f, 0.0f};
  const Circle home = stickHome();
  if (f.stick && mLayout == Layout::Play)
  {
    // The stick centres under the thumb, kept clear of the screen's edges
    // and of the HUD.
    f.ox = std::clamp(x, home.r * 0.7f, float(kScreenW) * 0.5f - home.r * 0.7f);
    f.oy = std::clamp(y, 110.0f + home.r * 0.7f, float(kScreenH) - home.r * 0.7f);
  }
  else
  {
    f.ox = home.x;
    f.oy = home.y;
  }
  mFingers.push_back(f);
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

int TouchControls::buttonAt(float x, float y) const
{
  int best = -1;
  float bestD = 0.0f;
  for (int b = 0; b < kButtonCount; ++b)
  {
    if (!buttonShown(b))
      continue;
    const Circle c = button(b);
    const float d = std::hypot(x - c.x, y - c.y) / c.r;
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
    if (!f.stick && buttonAt(f.x, f.y) == b)
      return true;
  return false;
}

Input TouchControls::held() const
{
  Input in;
  const float dead = kDeadZone * mScale;
  for (const auto& f : mFingers)
  {
    if (!f.stick)
      continue;
    const float dx = f.x - f.ox;
    const float dy = f.y - f.oy;
    if (dx * dx + dy * dy < dead * dead)
      continue;
    // Eight sectors: a direction counts unless the thumb is more than
    // ~63 degrees away from it, so diagonals press two directions.
    const float ax = std::fabs(dx), ay = std::fabs(dy);
    if (ax * 2.0f > ay)
    {
      in.left = in.left || dx < 0.0f;
      in.right = in.right || dx > 0.0f;
    }
    if (ay * 2.0f > ax)
    {
      in.up = in.up || dy < 0.0f;
      in.down = in.down || dy > 0.0f;
    }
  }
  // Same meaning as a pad's buttons: A jumps and confirms, B fires and
  // backs out of menus, Y switches runner, Start pauses. In menus the two
  // shown buttons are OK and BACK only, so BACK can't fire a stray shot.
  in.jump = in.confirm = buttonHeld(kJump);
  if (mLayout == Layout::Play)
  {
    in.fire = in.back = buttonHeld(kFire);
    in.swap = buttonHeld(kSwap);
    in.pause = buttonHeld(kPause);
  }
  else
  {
    in.back = buttonHeld(kFire);
  }
  return in;
}

Input TouchControls::read()
{
  const Input in = held() | mTapped;
  mTapped = Input{};
  return in;
}

void TouchControls::draw(Renderer& renderer) const
{
  if (!mVisible)
    return;
  const float scale = mScale / kBakeScale;
  const bool play = mLayout == Layout::Play;
  const float idle = play ? 0.4f : 0.55f;

  // The stick: where each thumb put it, or faintly at home.
  const Circle home = stickHome();
  bool stickDrawn = false;
  for (const auto& f : mFingers)
  {
    if (!f.stick)
      continue;
    DrawOpts o;
    o.scale = scale;
    o.alpha = 0.75f;
    renderer.draw(play ? mStick : mDpad, f.ox, f.oy, o);
    float dx = f.x - f.ox, dy = f.y - f.oy;
    const float d = std::hypot(dx, dy);
    const float reach = home.r * 0.75f;
    if (d > reach)
    {
      dx *= reach / d;
      dy *= reach / d;
    }
    DrawOpts n;
    n.scale = scale;
    n.blend = Blend::Add;
    renderer.draw(mNub, f.ox + dx, f.oy + dy, n);
    stickDrawn = true;
  }
  if (!stickDrawn)
  {
    DrawOpts o;
    o.scale = scale;
    o.alpha = idle;
    renderer.draw(play ? mStick : mDpad, home.x, home.y, o);
  }

  for (int b = 0; b < kButtonCount; ++b)
  {
    if (!buttonShown(b))
      continue;
    const Circle c = button(b);
    const bool held = buttonHeld(b);
    DrawOpts o;
    o.alpha = held ? 0.95f : idle;
    o.scale = scale * (held ? 0.92f : 1.0f);
    const Texture& t = play ? mPlayIcons[std::size_t(b)] : (b == kJump ? mOk : mBack);
    renderer.draw(t, c.x, c.y, o);
  }
}

} // namespace gr
