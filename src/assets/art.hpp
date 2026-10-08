#pragma once

#include "data/theme.hpp"
#include "gfx/canvas.hpp"

#include <array>

namespace td
{

enum CharacterFrame
{
  kFrameIdle = 0,
  kFrameRun1 = 1,
  kFrameRun2 = 2,
  kFrameJump = 3,
  kFrameCount = 4,
};

struct CharacterArt
{
  std::array<Image, kFrameCount> frames;
};

// All original placeholder art for TurboDudes, generated from ASCII pixel
// art and small procedural routines at startup. No external asset files.
struct Art
{
  std::array<CharacterArt, 3> characters;
  std::array<Image, 2> walker;
  std::array<Image, 2> flyer;
  Image turret;
  std::array<Image, 4> gem;
  Image health;
  Image crate;
  std::array<Image, 4> solid;
  Image solidTop;
  Image platform;
  Image spikes;
  std::array<Image, 3> playerBullet;
  Image enemyBullet;
  Image backFar;
  Image backNear;

  static Art build(const Theme& theme);
};

void drawSky(Canvas& canvas, const Theme& theme, int frame);
void drawBackdrop(
  Canvas& canvas,
  const Art& art,
  float camX,
  float camY,
  float baseCamY);
void drawDecoration(Canvas& canvas, const Theme& theme, int x, int y, int frame);
void drawExit(Canvas& canvas, const Theme& theme, int x, int y, int frame);

} // namespace td
