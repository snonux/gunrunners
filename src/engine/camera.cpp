#include "engine/camera.hpp"

#include "data/level.hpp"

#include <cmath>

namespace gr
{

namespace
{

// RigelEngine keeps the player between cells 10 and 21 of its 32 cell wide
// view; the wider HD view adds 4 cells on each side.
constexpr int kDeadZoneStartX = 14;
constexpr int kDeadZoneEndX = 25;
// Vertical dead zones: {2, 19} and the tight {7, 13} of a 20 cell high view,
// shifted down to leave room for the HUD.
constexpr int kDeadZoneTop = 5;
constexpr int kDeadZoneBottom = 20;
constexpr int kTightTop = 8;
constexpr int kTightBottom = 15;
constexpr int kMaxAdjust = 2;

} // namespace

void Camera::centerOn(const Target& t, int worldW, int worldH)
{
  mX = t.left - 18;
  mY = t.bottom - 15;
  clampToWorld(worldW, worldH);
  mPrevX = mX;
  mPrevY = mY;
}

void Camera::update(const Target& t, int manualScroll, int worldW, int worldH)
{
  mPrevX = mX;
  mPrevY = mY;

  mY += manualScroll * kMaxAdjust;

  const int zoneLeft = mX + kDeadZoneStartX;
  const int zoneRight = mX + kDeadZoneEndX;
  const int zoneTop = mY + (t.tight ? kTightTop : kDeadZoneTop);
  const int zoneBottom = mY + (t.tight ? kTightBottom : kDeadZoneBottom);

  int dx = 0;
  if (t.left < zoneLeft)
    dx = t.left - zoneLeft;
  else if (t.right > zoneRight)
    dx = t.right - zoneRight;
  int dy = 0;
  if (t.top < zoneTop)
    dy = t.top - zoneTop;
  else if (t.bottom > zoneBottom)
    dy = t.bottom - zoneBottom;

  mX += clampTo(dx, -kMaxAdjust, kMaxAdjust);
  mY += clampTo(dy, -kMaxAdjust, kMaxAdjust);
  clampToWorld(worldW, worldH);
}

void Camera::clampToWorld(int worldW, int worldH)
{
  mX = clampTo(mX, 0, std::max(0, worldW - int(std::ceil(mViewW))));
  mY = clampTo(mY, 0, std::max(0, worldH - int(std::ceil(mViewH))));
}

float Camera::renderX(float alpha) const
{
  return (float(mPrevX) + float(mX - mPrevX) * alpha) * float(kCellSize) + mShakeX;
}

float Camera::renderY(float alpha) const
{
  return (float(mPrevY) + float(mY - mPrevY) * alpha) * float(kCellSize) + mShakeY;
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
