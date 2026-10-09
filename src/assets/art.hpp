#pragma once

#include "data/theme.hpp"
#include "render/renderer.hpp"

#include <array>
#include <map>
#include <string>

namespace gr
{

// The game logic works in "world pixels" (8 per cell, 16 per level block,
// 320x180 visible, like the original). Everything is drawn at 4x that, i.e.
// 1280x720, from smooth vector art baked at startup, so the game looks crisp
// instead of pixelated.
constexpr float kPixelScale = 4.0f;

// A sprite baked once facing right and once mirrored facing left. Sprites
// are anchored at a point (e.g. the middle of the feet), so mirroring keeps
// them in place.
struct Sprite
{
  std::array<Texture, 2> facing;
  const Texture& get(int dir) const { return facing[dir < 0 ? 1 : 0]; }
};

constexpr int kRunFrames = 8;

// HUD panel sizes in screen pixels.
constexpr int kHudPanelH = 64;
constexpr int kHudHealthW = 430;
constexpr int kHudWeaponW = 260;
constexpr int kHudInventoryW = 130;
constexpr int kHudLettersW = 160;
constexpr int kHudScoreW = 224;
constexpr int kItemIcons = 21;

// Every pose a Gunrunner can strike. All are anchored at the bottom centre of
// the player's collision box.
struct CharacterArt
{
  std::array<Sprite, 2> idle;
  std::array<Sprite, kRunFrames> run;
  Sprite lookUp;
  Sprite crouch;
  Sprite coil;
  Sprite jump;
  Sprite fall;
  Sprite fallFast;
  Sprite tuck; // rotated for somersaults
  std::array<Sprite, 2> climb;
  Sprite hang;
  std::array<Sprite, 4> hangMove;
  Sprite hangAimDown;
  Sprite hangLegsUp;
  Sprite jetpack;
  Sprite hurt;
  Texture portrait;
};

// Index into Art::items.
enum ItemIcon
{
  kIconHealth,
  kIconMerch0,
  kIconMerch1,
  kIconMerch2,
  kIconLaser,
  kIconRocket,
  kIconFlame,
  kIconRapidFire,
  kIconKey,
  kIconGem0,
  kIconGem1,
  kIconGem2,
  kIconGem3,
  kIconLetterG,
  kIconLetterU,
  kIconLetterN,
  kIconTurbo,
  kIconVirus,
  kIconProto,  // prototype gun, drawn pale so it can be tinted per weapon
  kIconDuck,   // the rubber duck in synthwave shades
  kIconCamera, // the candid camera
};

struct Art
{
  std::array<CharacterArt, 3> characters;
  std::array<Color, 3> characterColor;
  std::array<Sprite, 2> walker;
  std::array<Sprite, 2> flyer;
  Sprite turret;

  // Items are 2x2 cells, drawn with their top-left at the box's top-left.
  std::array<Texture, kItemIcons> items;
  std::array<Texture, 3> boxes; // white, blue, green
  std::array<Color, 3> boxColor;
  std::array<Color, 4> gemColor;

  std::array<Texture, 3> solid;
  Texture solidTop;
  Texture platform;
  Texture cloud; // one-way platforms in levels with flags=clouds
  // Styled enemies, baked the first time they are drawn (enemy_art.cpp).
  mutable std::map<std::string, Sprite> styled;
  Texture spikes;
  Texture ladder;
  Texture pipe;
  Texture fieldEmitter;
  Texture fieldBeam;
  Texture beaconOff;
  Texture beaconOn;

  // Projectiles, drawn centred and rotated for vertical shots.
  Texture shotNormal;
  Texture shotLaser;
  Texture shotRocket;
  Texture shotFlame;
  Texture enemyShot;

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
  // Health, weapon, inventory, letters and score panels.
  std::array<Texture, 5> hudPanels;
  Texture hudSlot;

  static Art build(const Theme& theme, const Renderer& renderer);
};

// Additive soft light at screen position (cx, cy).
void drawGlow(Renderer& r, const Art& art, float cx, float cy, float radius, Color c, float alpha);
// camX/camY are in screen pixels.
// farShift/nearShift: extra px the far and near layers have slid (the
// maglev's rushing city).
void drawBackdrop(Renderer& r, const Art& art, float camX, float camY, float baseCamY, float farShift = 0.0f,
  float nearShift = 0.0f);
void drawDecoration(Renderer& r, const Art& art, const Theme& theme, float x, float y, int seed, int frame);
// (x, y) is the bottom-left corner of the 4x6 cell exit.
void drawExit(Renderer& r, const Art& art, const Theme& theme, float x, float y, int frame);
// A runner in a pose for cutscenes and menus, anchored at the bottom centre.
// pose: 0 idle, 1 run, 2 jump, 3 look up, 4 crouch, 5 hurt, 6 coil, 7 fall,
// 8 idle (other frame).
Texture bakeCharacterPose(const Renderer& r, int kind, int pose, float scale, bool mirror = false);
// Rounded translucent panel for menus and overlays.
Texture makePanel(const Renderer& r, int w, int h, Color fill, Color border, double radius);

} // namespace gr
