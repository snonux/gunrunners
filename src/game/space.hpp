#pragma once

#include "game/collision.hpp"

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

// Level 43: an asteroid drifting through space. Shots push it along and
// chip at it; a big one splits into two middle ones, those into small
// ones, and the small ones crumble.
struct DriftRock
{
  int x = 0, y = 0;   // sixteenths of a cell, top-left
  int vx = 0, vy = 0; // sixteenths of a cell a frame
  int size = 6;       // cells across: 6 big, 4 middle, 2 small
  int hp = 6;
  int flash = 0;
  int look = 0;      // which of the rock sprites
  bool alive = true;
  int cx() const { return x / 16; }
  int cy() const { return y / 16; }
};

// A thing in space the level draws over the rock it is made of: the
// derelict probe, the landing pad.
struct SpaceProp
{
  std::string kind;
  int x = 0, y = 0, w = 1, h = 1; // blocks
};

// Level 46: a Swap Crystal. Shoot it and you and it trade places in a
// flash. Some drift along a path until they are first swapped.
struct SwapCrystal
{
  std::string id;
  int x = 0, y = 0;                       // cells, top-left (2 x 3 cells)
  std::vector<std::pair<int, int>> path;  // cells, top-left: where it drifts, back and forth
  int speed = 4;                          // frames per cell along the path
  int leg = 0, step = 0, dir = 1;         // where it is along the path
  bool drifting = false;
  bool cracked = false; // the Virus's: swap with it and you catch it
  bool hidden = false;  // only seen in a pool's reflection (until it is swapped)
  bool gone = false;    // a cracked one shatters once swapped
  int cool = 0;         // frames before it rings again
  int flash = 0;
  static constexpr int kW = 2, kH = 3;
  CellBox box() const { return {x, y, kW, kH}; }
  // What a shot has to touch: a cell wider either side (its glow).
  CellBox hitBox() const { return {x - 1, y, kW + 2, kH}; }
};

// Level 47: a Silk Line, a taut strand from a high anchor (a) down to a
// low one (b), in cells: where the hands go. Jump into one and you hang
// from it and slide down it. A Loom Spider can cut it; it is spun again.
struct SilkLine
{
  std::string id;
  float ax = 0, ay = 0, bx = 0, by = 0;
  float len = 0, ux = 0, uy = 0; // length and unit step, a to b
  float spun = 0;   // how far from a it is spun (len: all of it)
  bool spin = false; // spun as you come near (`spin=1`), not there before
  bool spinning = false;
  bool mine = false; // the Silk Shooter's
  int regrow = 0;    // frames until a cut line is spun again
  int cool = 0;      // frames before it can be grabbed again
  int twang = 0;     // frames it shivers (grabbed, about to be cut)
  bool whole() const { return spun >= len; }
  void point(float s, float& x, float& y) const
  {
    x = ax + ux * s;
    y = ay + uy * s;
  }
};

// Level 47: a cocoon hanging on a web: shoot it open (gems pour out), or
// the green one (the Virus) bursts on you if you touch it.
struct SilkCocoon
{
  int x = 0, y = 0; // cells, top-left (2 x 3)
  bool virus = false;
  int gems = 0;
  bool popped = false;
  static constexpr int kW = 2, kH = 3;
  CellBox box() const { return {x, y, kW, kH}; }
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
  // Level 43: open space (`flags=space`): drifting rocks (and the ones the
  // level started with, for a respawn), the probe and the pad, and the
  // block x where the ship starts down into Vurr's clouds.
  bool starfall = false;
  std::vector<DriftRock> rocks, rocksAtStart;
  std::vector<SpaceProp> props;
  int cloudX = -1;
  // Level 46: the Swap Crystals (and where they started, for a respawn),
  // the reflecting pools (cells: x0, x1, surface row).
  bool crystals = false;
  std::vector<SwapCrystal> swaps, swapsAtStart;
  std::vector<std::array<int, 3>> pools;
  // Shots a Prism Bat split last frame: x, y (cells), direction, damage,
  // shot kind, the bat's id (the three pieces fly on past it).
  std::vector<std::array<int, 6>> prisms;
  // Level 47: Silk Lines and cocoons (and as they started, for a respawn).
  bool silk = false;
  std::vector<SilkLine> lines, linesAtStart;
  std::vector<SilkCocoon> cocoons, cocoonsAtStart;
  // Level 48: Bounders to ride, the Tamer's Whip's crack (cells: where it
  // starts, which way, how long it is; frames it still shows).
  bool plains = false;
  int whipX = 0, whipY = 0, whipDir = 1, whipLen = 0, whipShow = 0;
};

} // namespace gr
