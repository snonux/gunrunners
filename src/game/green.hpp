#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace gr
{

// Level 17, Hydroponics (world_green.cpp): grow lamps and the plants they
// grow, spore clouds, and the Growth Spurt bonus (rules=grow).

// A shootable switch (`@ switch id=SID x y kind=shootable`): each hit
// toggles the lamps wired to it.
struct GreenSwitch
{
  std::string id;
  int bx = 0, by = 0;
  int flash = 0; // frames of the hit flash left (drawn)
};

// A hanging grow lamp (`@ lamp id=ID x y switch=SID timer=T`): timer 0
// stays lit until shot again; otherwise it goes out T frames after it lit,
// flickering for the last kLampFlicker frames.
struct GrowLamp
{
  std::string id;
  int bx = 0, by = 0;
  int sw = -1;      // its switch
  int timer = 0;    // 0: stays on
  bool lit = false;
  bool locked = false; // the top dome's four lamps, once the stairs grew
  int left = 0;     // frames until it goes out (timed lamps)
  int hum = 0;      // frames of the basil's note left (the easter egg)
  bool basil = false; // the basil pot's lamp
};

// A plant (`@ plant id=ID lamp=LID[,LID...] kind= rect= root=`): its
// tiles grow from the root outward while every one of its lamps is lit and
// wither tip-first once one goes out.
enum class PlantKind
{
  Bridge, // solid
  Leaf,   // one-way
  Ladder, // a climbable creeper
  Stairs, // solid
  Flower, // one-way leaves and a bloom
};

struct Plant
{
  std::string id;
  PlantKind kind = PlantKind::Bridge;
  std::vector<int> lamps;
  std::vector<std::pair<int, int>> tiles; // blocks, root first
  // How far it has grown, in 1/90ths of a tile: it grows 2 x tiles a frame
  // (all of it in 45 frames) and withers 3 x tiles a frame (30 frames). A
  // withering tile turns brown 8 frames before it goes: index i is brown
  // once prog - i * 90 <= 24 x tiles.
  int prog = 0;
  int shown = 0;      // tiles set in the map (a prefix of `tiles`)
  bool growing = false;
};

// A puff of spores (3 x 3 blocks) drifting away from its bulb.
struct SporeCloud
{
  float x = 0.0f, y = 0.0f; // top-left cell
  float dx = 0.0f, dy = 0.0f;
  int life = 0;
  bool carrier = false;
};

// Water drawn in the greenhouse style (none placed yet: Level 17 wades
// through `@ water`; `@ pool` belongs to Level 46).
struct GreenPool
{
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0; // blocks
};

struct GreenState
{
  bool on = false;
  std::vector<GreenSwitch> switches;
  std::vector<GrowLamp> lamps;
  std::vector<Plant> plants;
  std::vector<SporeCloud> clouds;
  std::vector<GreenPool> pools;
  int slow = 0;      // frames the runner wades through spores (walk x 1/2)
  int trim = 0;      // frames the Hedge Trimmer has been held (0: idle)
  int trimFace = 1;  // where its cone points (1 right, -1 left)
  std::vector<std::array<int, 4>> splits; // Globs to split off: x, y, enemy def, dir
  int lastGems = 0;
  // Growth Spurt (rules=grow): size steps 0 (1.0) to 4 (2.0).
  bool grow = false;
  int size = 0;
  int sizeFlash = 0;
  std::array<int, 8> arc{}; // the jump arc at this size
  float scale() const { return 1.0f + 0.25f * float(size); }
};

} // namespace gr
