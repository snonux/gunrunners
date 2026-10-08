#include "data/characters.hpp"

#include <array>

namespace gr
{

namespace
{

const std::array<CharacterDef, kCharacterCount> kCharacters{{
  {"DASH", "THE ALL-ROUNDER", 1.6f, -6.3f, 4, 14, Weapon::Blaster, 5.0f, 3, 3, 3},
  {"ROCCO", "THE HEAVY", 1.35f, -5.9f, 6, 24, Weapon::Scatter, 4.5f, 2, 2, 5},
  {"NOVA", "THE ACROBAT", 1.9f, -7.0f, 3, 7, Weapon::Rapid, 6.0f, 5, 5, 2},
}};

} // namespace

const CharacterDef& characterByIndex(int index)
{
  return kCharacters[std::size_t(((index % kCharacterCount) + kCharacterCount) % kCharacterCount)];
}

} // namespace gr
