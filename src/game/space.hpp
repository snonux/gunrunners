#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <utility>
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

// Level 45: a Gullet Tube. Step into its mouth and the hive swallows you;
// you shoot along it and are spat out at the far end. Branches: 0 is
// `path`, 1 is `alt` (a valve on the tube picks which).
struct GulletTube
{
  std::string id;
  std::array<std::vector<std::pair<int, int>>, 2> path; // cells, ring centre to ring centre
  std::array<int, 2> len{};                             // cells along each branch
  int mx = 0, my = 0;                                   // the mouth's block
  int inX = 1, inY = 0;                                 // the side its mouth opens to
  std::array<int, 2> ex{}, ey{};                        // each branch's exit block
  std::array<int, 2> outX{}, outY{};                    // and the side it opens to
  bool valve = false;
  int vx = 0, vy = 0; // the valve (cells, its centre)
  int set = 0;        // the branch the valve sends you down
  int flip = 0;       // frames since the valve last turned
  bool breath = false; // the mouth only opens while the hive breathes in
  bool pore = false;   // a small unmarked pore (a secret)
  int gulp = 0;        // frames since it swallowed something
};

// A wall pore that spits Hive Mites when you come close.
struct MitePore
{
  int x = 0, y = 0; // cells: the opening's middle
  int dir = 1;      // the side it opens to
  int count = 3;
  int state = 0; // 0 waiting, 1 rattling, 2 spitting, 3 spent (re-arms)
  int t = 0;
  int left = 0;      // mites still to come out
  std::vector<int> mites; // enemy indices it spat
};

// A Bile Blaster shot travelling a tube.
struct TubeShot
{
  int tube = 0, branch = 0;
  int s = 0; // cells along
  int damage = 1;
};

struct SpaceState
{
  // Level 44: blocks coated in goo (`@ goo rect=`), per block.
  std::vector<std::uint8_t> gooBlock;
  bool goo = false; // the level has goo, or a Goo Gun to make some
  std::vector<GooPatch> patches;
  // Gloops killed this frame: they split next frame (x, y, w).
  std::vector<std::array<int, 3>> splits;
  // Level 45: the hive's tubes, mite pores, shots in tubes, mites to bring
  // in next frame (x, y, dir), the green dripping pores (cells).
  bool hive = false;
  std::vector<GulletTube> tubes;
  std::vector<MitePore> pores;
  std::vector<TubeShot> tubeShots;
  std::vector<std::array<int, 4>> mites; // x, y, dir, pore (-1: a Warden's)
  std::vector<std::pair<int, int>> drips;
};

} // namespace gr
