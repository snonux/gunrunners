#include "data/characters.hpp"

namespace gr
{

namespace
{

const std::array<CharacterDef, kCharacterCount> kCharacters{{
  // Dash plays exactly like Duke: 9 health, the classic jump arc.
  {"DASH", "THE ALL-ROUNDER", 9, {2, 2, 1, 1, 1, 0, 0, 0}, Weapon::Normal, 0, 3, 3, 3},
  {"ROCCO", "THE HEAVY", 12, {2, 2, 1, 1, 0, 0, 0, 0}, Weapon::Rocket, 12, 5, 2, 5},
  {"NOVA", "THE ACROBAT", 7, {2, 2, 2, 1, 1, 1, 0, 0}, Weapon::Laser, 16, 2, 5, 4},
}};

} // namespace

const char* weaponName(Weapon w)
{
  switch (w)
  {
    case Weapon::Laser:
      return "LASER";
    case Weapon::Rocket:
      return "ROCKETS";
    case Weapon::Flame:
      return "FLAMER";
    case Weapon::Normal:
    default:
      return "BLASTER";
  }
}

int maxAmmo(Weapon w)
{
  return w == Weapon::Flame ? 64 : 32;
}

const CharacterDef& characterByIndex(int index)
{
  return kCharacters[std::size_t(((index % kCharacterCount) + kCharacterCount) % kCharacterCount)];
}

} // namespace gr
