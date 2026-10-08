#pragma once

#include "data/theme.hpp"
#include "render/renderer.hpp"

#include <array>

namespace gr
{

// Game logic runs in "world pixels" (16 per tile, 320x180 view, like the
// original). Everything is drawn at 4x that, i.e. 1280x720, from smooth
// vector art baked at startup, so the game looks crisp instead of pixelated.
constexpr float kPixelScale = 4.0f;

// A sprite baked once facing right and once mirrored facing left.
struct Sprite
{
  std::array<Texture, 2> facing;
  const Texture& get(int dir) const { return facing[dir < 0 ? 1 : 0]; }
};

constexpr int kRunFrames = 8;

struct CharacterArt
{
  std::array<Sprite, 2> idle;
  std::array<Sprite, kRunFrames> run;
  Sprite jump;
  Sprite fall;
  Texture portrait;
};

struct Art
{
  std::array<CharacterArt, 3> characters;
  std::array<Sprite, 2> walker;
  std::array<Sprite, 2> flyer;
  Sprite turret;
  std::array<Texture, 4> gem;
  std::array<Color, 4> gemColor;
  Texture health;

  std::array<Texture, 3> solid;
  Texture solidTop;
  Texture platform;
  Texture spikes;
  Texture crate;

  std::array<Texture, 3> playerBullet;
  Texture enemyBullet;

  Texture sky;
  Texture backFar;
  Texture backNear;
  Texture vignette;
  std::array<Texture, 4> glow; // soft white light, radius 16/32/64/128
  Texture dot;                 // soft particle

  Texture decoBase;
  Texture decoLit;
  Texture exitBase;
  Texture exitBeam;
  Texture heartFull;
  Texture heartEmpty;
  Texture hudLeft;
  Texture hudCenter;
  Texture hudRight;

  static Art build(const Theme& theme, const Renderer& renderer);
};

// Additive soft light at screen position (cx, cy).
void drawGlow(Renderer& r, const Art& art, float cx, float cy, float radius, Color c, float alpha);
// camX/camY are in screen pixels.
void drawBackdrop(Renderer& r, const Art& art, float camX, float camY, float baseCamY);
void drawDecoration(Renderer& r, const Art& art, const Theme& theme, float x, float y, int seed, int frame);
void drawExit(Renderer& r, const Art& art, const Theme& theme, float x, float y, int frame);
// Rounded translucent panel for menus and overlays.
Texture makePanel(const Renderer& r, int w, int h, Color fill, Color border, double radius);

} // namespace gr
