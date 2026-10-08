#pragma once

#include <array>

namespace gr
{

// Duke Nukem II's four guns: the default blaster has unlimited ammo, the
// others hold 32 shots (64 for the flamethrower) and fall back to the
// blaster when empty.
enum class Weapon
{
  Normal,
  Laser,  // pierces enemies
  Rocket, // heavy damage
  Flame,  // pierces, and down + fire works as a jetpack
  Proto,  // the level's prototype (Player::proto)
};

constexpr int kWeaponCount = 4;
const char* weaponName(Weapon w);
int maxAmmo(Weapon w);

// The three selectable Gunrunners. They share Duke's movement rules (one cell
// per frame, same controls) and differ in health, jump arc and the gun they
// start with.
struct CharacterDef
{
  const char* name;
  const char* role;
  int maxHp;
  // Cells moved up per 15 Hz frame of a jump (RigelEngine's JUMP_ARC).
  std::array<int, 8> jumpArc;
  Weapon startWeapon;
  int startAmmo;
  int healthPips, jumpPips, powerPips;
};

constexpr int kCharacterCount = 3;
const CharacterDef& characterByIndex(int index);

} // namespace gr
