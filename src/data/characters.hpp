#pragma once

namespace gr
{

enum class Weapon
{
  Blaster, // single shot, medium rate
  Scatter, // three pellets, short range
  Rapid,   // fast, light shots
};

// The three selectable Gunrunners. Stats are deliberately readable at a
// glance on the select screen (1-5 pips).
struct CharacterDef
{
  const char* name;
  const char* role;
  float runSpeed;    // px per tick
  float jumpVelocity; // px per tick (negative = up)
  int maxHp;
  int fireCooldown; // ticks
  Weapon weapon;
  float bulletSpeed;
  int speedPips, jumpPips, powerPips;
};

constexpr int kCharacterCount = 3;
const CharacterDef& characterByIndex(int index);

} // namespace gr
