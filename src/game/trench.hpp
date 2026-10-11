#pragma once

#include "game/collision.hpp"

#include <array>
#include <vector>

namespace gr
{

// Level 30, Fiber Trench (world_trench.cpp, world_trench_draw.cpp): the map
// is under water (`@ fluid kind=water`). In the water a runner sinks slowly
// and every jump is a short swim stroke; three strokes (Nova four) and they
// must touch ground, a harpoon line or an air pocket again. Currents push
// swimmers (not Rocco), the abyss under the trench sends them back to the
// last ground they stood on, and the Harpoon Line strings a line between two
// harpoons to walk on. Also the Bubble Up bonus (rules=bubble).

constexpr std::array<int, 8> kStrokeArc{1, 1, 1, 1, 0, 0, 0, 0}; // a swim stroke: 4 cells
constexpr int kStrokes = 3;       // strokes between touching ground (Nova 4)
constexpr int kSinkEvery = 3;     // frames per cell sunk in the water (Rocco 2)
constexpr int kAbyssFade = 20;    // frames of fade after the abyss sends a runner back
constexpr int kLineReach = 32;    // cells: the longest line
constexpr int kLineSlope = 8;     // cells: two harpoons further apart in height make no line
constexpr int kHarpoonPull = 8;   // frames a harpooned small enemy is pulled in
constexpr int kAnglerReach = 6;   // cells: an Anglerfish bites this close
constexpr int kAnglerLunge = 6;   // cells it lunges
constexpr int kEelWake = 10;      // cells: a Cable Eel wakes this close
constexpr int kEelReach = 8;      // cells it lunges out
constexpr int kEelOut = 6, kEelBack = 10;
constexpr int kQuietFrames = 45;  // frames by the quiet junction box before it speaks
constexpr int kWhaleEvery = 600;  // frames between the whale's crossings
// Bubble Up.
constexpr int kBubbleSink = 2;    // cells a tap of jump sinks the bubble

// `@ current rect= dir=l|r|u|d push=N|1/2`.
struct Current
{
  CellBox rect{0, 0, 0, 0};
  int dx = 0, dy = 0;
  int every = 1; // frames per cell pushed
};

// `@ anchor x y spot=sx`: a ring a harpoon sticks in, and where a runner
// plants the other end of a line to it (block x; the line runs at the height
// of the ground there).
struct Anchor
{
  int x = 0, y = 0;   // cells: the ring's block (top-left)
  int spotX = 0;      // cells: the middle of the planting spot
  int row = 0;        // cells: the line's top row (the ground at the spot)
  CellBox catchBox() const { return {x, y - 4, 2, 6}; }
};

struct Harpoon
{
  int x = 0, y = 0;  // cells
  int anchor = -1;
};

// The Box Jelly's column: rise, hold, sink, rest.
struct JellyColumn
{
  int enemy = -1;
  int homeX = 0, bottom = 0; // cells
  int rise = 12;             // cells
  int up = 48, hold = 30, down = 48, rest = 30;
  int phase = 0;
};

// A Cable Eel's junction box.
struct EelBox
{
  int enemy = -1;
  int bx = 0, by = 0; // blocks: the box (solid)
  int fx = 0, fy = 0; // facing
  int ext = 0;        // cells out
  int state = 0;      // 0 in, 1 the tell, 2 out, 3 back
  int t = 0;
};

struct TrenchState
{
  bool on = false;
  bool water = false;
  CellBox waterBox{0, 0, 0, 0};
  std::vector<CellBox> pockets;
  std::vector<Current> currents;
  std::vector<CellBox> abyss;
  std::vector<CellBox> darks;
  std::vector<Anchor> anchors;
  std::vector<Harpoon> harpoons; // the last two
  bool line = false;
  int lineX0 = 0, lineX1 = 0, lineY = 0; // cells: the line's walkable top
  int strokes = 0;
  bool wasGround = false;
  int sinkTick = 0;
  int pushTick = 0;
  int groundX = 0, groundY = 0; // the last solid ground stood on
  int fade = 0;
  std::vector<std::pair<int, int>> pulls; // enemy, frames left
  std::vector<JellyColumn> jellies;
  std::vector<EelBox> eels;
  // Easter eggs: the quiet junction box, the porthole's alien.
  int quietX = -1, quietY = -1, quietT = 0;
  bool quietSpoke = false;
  int portX = -1, portY = -1, wave = 0, waves = 0;
  // Bubble Up (rules=bubble).
  bool bubble = false;
  int bubbleTick = 0;
  std::vector<int> rings; // cells: rows of the bubble rings
  int ringX = 0, ringY = 0; // where a popped bubble starts again
  int pop = 0;
};

} // namespace gr
