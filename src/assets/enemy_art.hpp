#pragma once

#include "assets/art.hpp"

#include <string>

namespace gr
{

// The sprite for a styled enemy (EnemyLook::Styled), drawn by the routine
// for its key and cached in `art`. `variant` picks a pose family (e.g. a
// clinger on a wall or flat), `frame` an animation frame; the box is
// wCells x hCells and the sprite is anchored at its bottom centre.
const Sprite& styledEnemySprite(const Art& art, const Renderer& r, const Theme& t, const std::string& key, int variant,
  int frame, int wCells, int hCells);

} // namespace gr
