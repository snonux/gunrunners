#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace gr
{

// Episode 7, DEEP SPACE (world_space.cpp, docs/DEEP_SPACE.md): the state
// its levels add to the world, kept in one place.

// A Goo Gun splat (or a Gloop's) on a wall face: cells x, y0..y1 of the
// wall are goo until `life` runs out.
struct GooPatch
{
  int x = 0, y0 = 0, y1 = 0; // cells, the wall's column
  int side = 1;              // the coated face looks this way (-1 west, 1 east)
  int life = 150;
};

struct SpaceState
{
  // Level 44: blocks coated in goo (`@ goo rect=`), per block.
  std::vector<std::uint8_t> gooBlock;
  bool goo = false; // the level has goo, or a Goo Gun to make some
  std::vector<GooPatch> patches;
  // Gloops killed this frame: they split next frame (x, y, w).
  std::vector<std::array<int, 3>> splits;
};

} // namespace gr
