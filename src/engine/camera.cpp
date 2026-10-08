// Dead-zone camera after RigelEngine's src/game_logic/camera.cpp
// (GPL-2.0-or-later, Copyright (C) 2017 Nikolai Wuttke), working in cells
// and widened for the 16:9 HD view.

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
// Fraction of the remaining distance the view covers per 60 Hz tick.
constexpr float kEase = 0.3f;

} // namespace

void Camera::centerOn(const Target& t, int worldW, int worldH)
{
  mX = t.left - 18;
  mY = t.bottom - 15;
  clampToWorld(worldW, worldH);
  mPrevX = mX;
  mPrevY = mY;
  mSnap = true;
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

void Camera::shake(int ticks, float strength)
{
  if (ticks >= mShakeTicks)
    mShakeTotal = ticks;
  mShakeTicks = std::max(mShakeTicks, ticks);
  mShakeStrength = std::max(mShakeStrength, strength);
}

void Camera::tick(float alpha)
{
  const float targetX = (float(mPrevX) + float(mX - mPrevX) * alpha) * float(kCellSize);
  const float targetY = (float(mPrevY) + float(mY - mPrevY) * alpha) * float(kCellSize);
  if (mSnap)
  {
    mSmoothX = targetX;
    mSmoothY = targetY;
    mSnap = false;
  }
  else
  {
    mSmoothX += (targetX - mSmoothX) * kEase;
    mSmoothY += (targetY - mSmoothY) * kEase;
  }

  if (mShakeTicks > 0)
  {
    // Fades out over its length so it reads as one jolt, not a jitter.
    const float a = mShakeStrength * float(mShakeTicks) / float(std::max(1, mShakeTotal));
    --mShakeTicks;
    mShakeX = mRng.range(-a, a);
    mShakeY = mRng.range(-a, a);
    if (mShakeTicks == 0)
      mShakeStrength = 0.0f;
  }
  else
  {
    mShakeX = mShakeY = 0.0f;
  }
}

} // namespace gr
