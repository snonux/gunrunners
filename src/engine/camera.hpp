#pragma once

#include "base/math.hpp"

namespace td
{

// Dead-zone camera modeled after RigelEngine's game_logic/camera.cpp: the
// view only scrolls once the player leaves a box in the middle of the
// screen, and the scroll speed per tick is capped so motion stays smooth.
class Camera
{
public:
  Camera(int viewW, int viewH) : mViewW(viewW), mViewH(viewH) {}

  void centerOn(const Rect& target, int worldW, int worldH);
  void update(const Rect& target, int worldW, int worldH);
  void shake(int ticks, float strength);
  void tick();

  float x() const { return mX; }
  float y() const { return mY; }
  int renderX() const;
  int renderY() const;
  int viewW() const { return mViewW; }
  int viewH() const { return mViewH; }

private:
  void clampToWorld(int worldW, int worldH);

  int mViewW;
  int mViewH;
  float mX = 0.0f;
  float mY = 0.0f;
  int mShakeTicks = 0;
  float mShakeStrength = 0.0f;
  Rng mRng{777u};
  int mShakeX = 0;
  int mShakeY = 0;
};

} // namespace td
