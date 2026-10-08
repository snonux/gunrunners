#include "engine/camera.hpp"

namespace gr
{

namespace
{

// Fractions of the view; RigelEngine uses tiles 10..21 of a 32 tile view.
constexpr float kDeadZoneLeft = 0.36f;
constexpr float kDeadZoneRight = 0.52f;
constexpr float kDeadZoneTop = 0.30f;
constexpr float kDeadZoneBottom = 0.70f;
constexpr float kMaxScrollX = 4.0f;
constexpr float kMaxScrollY = 4.0f;

} // namespace

void Camera::centerOn(const Rect& t, int worldW, int worldH)
{
  mX = t.cx() - float(mViewW) * 0.4f;
  mY = t.cy() - float(mViewH) * 0.55f;
  clampToWorld(worldW, worldH);
}

void Camera::update(const Rect& t, int worldW, int worldH)
{
  const float left = mX + float(mViewW) * kDeadZoneLeft;
  const float right = mX + float(mViewW) * kDeadZoneRight;
  const float top = mY + float(mViewH) * kDeadZoneTop;
  const float bottom = mY + float(mViewH) * kDeadZoneBottom;

  float dx = 0.0f;
  if (t.cx() > right)
    dx = t.cx() - right;
  else if (t.cx() < left)
    dx = t.cx() - left;

  float dy = 0.0f;
  if (t.y < top)
    dy = t.y - top;
  else if (t.bottom() > bottom)
    dy = t.bottom() - bottom;

  mX += gr::clampTo(dx, -kMaxScrollX, kMaxScrollX);
  mY += gr::clampTo(dy, -kMaxScrollY, kMaxScrollY);
  clampToWorld(worldW, worldH);
}

void Camera::clampToWorld(int worldW, int worldH)
{
  mX = gr::clampTo(mX, 0.0f, float(std::max(0, worldW - mViewW)));
  mY = gr::clampTo(mY, 0.0f, float(std::max(0, worldH - mViewH)));
}

void Camera::shake(int ticks, float strength)
{
  mShakeTicks = std::max(mShakeTicks, ticks);
  mShakeStrength = std::max(mShakeStrength, strength);
}

void Camera::tick()
{
  if (mShakeTicks > 0)
  {
    --mShakeTicks;
    mShakeX = mRng.range(-mShakeStrength, mShakeStrength);
    mShakeY = mRng.range(-mShakeStrength, mShakeStrength);
    if (mShakeTicks == 0)
      mShakeStrength = 0.0f;
  }
  else
  {
    mShakeX = mShakeY = 0.0f;
  }
}


} // namespace gr
