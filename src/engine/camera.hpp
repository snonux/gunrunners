#pragma once

#include "base/math.hpp"

namespace gr
{

// Dead-zone camera ported from RigelEngine's game_logic/camera.cpp. It works
// on the 8 px cell grid at the 15 Hz logic rate: it only scrolls once the
// player leaves a box around the middle of the screen, at most 2 cells per
// frame. At the 60 Hz render rate the view eases towards that position, so
// it starts and stops scrolling smoothly instead of jumping to full speed,
// and a short, fading screen shake can be added on top.
class Camera
{
public:
  // View size in cells (40 x 22.5 for a 1280x720 screen).
  Camera(float viewW, float viewH) : mViewW(viewW), mViewH(viewH) {}

  struct Target
  {
    int left, top, right, bottom; // player box in cells
    bool tight;                   // climbing or flying: keep the player centred vertically
  };

  void centerOn(const Target& t, int worldW, int worldH);
  // One logic frame. manualScroll is -1 (look up), 0 or 1 (look down).
  void update(const Target& t, int manualScroll, int worldW, int worldH);
  void shake(int ticks, float strength);
  // 60 Hz. alpha is how far the next rendered frame is between the previous
  // and the current logic frame.
  void tick(float alpha);

  // Smoothed position in world pixels, including shake.
  float renderX() const { return mSmoothX + mShakeX; }
  float renderY() const { return mSmoothY + mShakeY; }
  int x() const { return mX; }
  int y() const { return mY; }
  float viewW() const { return mViewW; }
  float viewH() const { return mViewH; }

private:
  void clampToWorld(int worldW, int worldH);

  float mViewW;
  float mViewH;
  int mX = 0;
  int mY = 0;
  int mPrevX = 0;
  int mPrevY = 0;
  float mSmoothX = 0.0f;
  float mSmoothY = 0.0f;
  bool mSnap = true;
  int mShakeTicks = 0;
  int mShakeTotal = 0;
  float mShakeStrength = 0.0f;
  Rng mRng{777u};
  float mShakeX = 0.0f;
  float mShakeY = 0.0f;
};

} // namespace gr
