#pragma once

#include "game/collision.hpp"

#include <array>
#include <string>
#include <vector>

namespace gr
{

// Level 36, Prop Warehouse (world_warehouse.cpp, world_warehouse_draw.cpp):
// MaxTV's prop store. Gantry cranes with magnet heads lift metal crates and
// Forklift Bots and drop them where the runner wants a step; the cranes over
// the crane yard's foam pit carry their crate flush with the floor, so it
// can be ridden. The Magnet Gun pulls metal toward the runner (hold) or
// shoves it away (tap). Forklift Bots lift the runner on their forks and
// carry them off, Stagehands set down scenery flats to shoot from behind,
// and Prop Mimics wait among the real boxes. The Throw Everything bonus
// (rules=throw_all) lifts and throws anything loose.

// Magnet cranes.
constexpr int kCraneBell = 15;  // frames the bell rings before it moves
constexpr int kCraneHang = 6;   // cells below the rail a load hangs
constexpr int kCraneHead = 4;   // cells: the magnet's width (2 blocks)
constexpr int kCrateSize = 4;   // cells: a metal crate is 2 x 2 blocks
constexpr int kCrateRespawn = 60;
constexpr int kCrateBlink = 8;
// Magnet Gun.
constexpr int kPullAfter = 6;    // frames held before a press is a pull, not a push
constexpr int kPullReach = 20;   // cells: the beam's length (10 blocks)
constexpr int kPullAmmoEvery = 4;
constexpr int kPushReach = 16;   // cells: the pulse's range (8 blocks)
constexpr int kPushSlide = 12;   // cells a pushed crate slides (6 blocks)
// Forklift Bot.
constexpr int kForkSight = 24;   // cells ahead it charges from
constexpr int kForkCarry = 32;   // cells it carries the runner at most (16 blocks)
constexpr int kForkTell = 15;
constexpr int kRoccoShove = 6;   // cells: Rocco is pushed back, not lifted
// Stagehand.
constexpr int kStageGap = 12;    // cells in front of the runner it sets its flat down
constexpr int kStageTell = 12;
constexpr int kFlatHp = 6;
constexpr int kFlatW = 4, kFlatH = 6; // cells
// Prop Mimic.
constexpr int kMimicReach = 4;   // cells: it lunges at a runner this close
constexpr int kMimicTell = 10;
constexpr int kMimicBreath = 60; // frames between breaths
constexpr int kMimicBreathe = 8; // frames a breath lasts
// Easter eggs.
constexpr int kPropLabel = 45;
constexpr int kOldTitle = 90;
// Throw Everything.
constexpr int kLiftFrames = 8;
constexpr int kThrowCells = 12;  // cells ahead a throw lands (6 blocks)
constexpr int kThrowFrames = 8;  // frames a throw flies

// A metal crate (`@ crate id x y metal=1`, 2 x 2 blocks, x y its top-left
// block). While it rests it is solid map blocks; lifted, sliding or falling
// it is off the map.
enum class CrateState
{
  Rest,
  Held,  // on a crane's magnet
  Slide, // shoved by the Magnet Gun or pulled toward the runner
  Fall,
  Gone,  // in the foam pit, back at its start soon
};

struct MetalCrate
{
  std::string id;
  int x = 0, y = 0;         // cells, top-left (whole blocks while resting)
  int homeX = 0, homeY = 0; // cells: where it respawns
  CrateState state = CrateState::Rest;
  int slide = 0;  // cells left to slide (sign: direction)
  int speed = 0;  // cells a frame while sliding
  int fall = 0;   // cells a frame while falling
  int t = 0;      // Gone: frames until it comes back
  int crane = -1; // Held: on this crane's magnet
  int blink = 0;  // waiting above the runner before it drops
  bool stamped = false;
  std::array<unsigned char, 4> under{}; // the map's blocks under it while stamped
  std::vector<int> hit;                 // enemies hit on this slide or fall
  CellBox box() const { return {x, y, kCrateSize, kCrateSize}; }
};

// A gantry crane (`@ crane id x0= x1= row= lever= start= ride=1`): a trolley
// on a rail with a magnet head on a cable.
enum class CranePhase
{
  Idle,
  Bell,  // ringing before it moves
  Lower, // the magnet goes down onto what is below
  Lift,  // up to the hang height with what it got
  Move,  // along the rail to the far end
  Drop,  // a crate waiting to drop (over the runner)
  // Ride cranes (their load is a platform).
  RideBell,
  Ride,
  RideHold,
  RideBack,
};

struct Crane
{
  std::string id, leverId;
  int x0 = 0, x1 = 0;  // cells: the range of the head's left cell
  int rail = 0;        // cells: the rail's row
  int x = 0;           // cells: the head's left cell
  int hook = 0;        // cells: the magnet's bottom row
  int dir = 1;
  CranePhase phase = CranePhase::Idle;
  int t = 0;
  int lever = -1;
  int load = -1;       // crate index, or -2 - enemy index
  bool ride = false;
  int platform = -1;   // ride cranes: the crate as a platform
  int startX = 0;      // ride cranes: where it waits
  int cranesMoved = 0; // times it carried something (the bot's progress)
  int hookTop() const { return rail + 1; }
};

// A lever on the wall (`@ switch id x y kind=lever`): up beside it throws it.
struct CraneLever
{
  std::string id;
  int x = 0, y = 0; // cells: its bottom-left, on the floor row
  bool on = false;
  int flash = 0;
  int throws = 0;
};

// A scenery flat a Stagehand carries and sets down: blocks shots both ways.
struct Flat
{
  int x = 0, y = 0; // cells: bottom-left
  int hp = kFlatHp;
  int enemy = -1;   // its Stagehand
  bool down = false; // set down (not carried)
  bool alive = true;
  CellBox box() const { return boxAt(x, y, kFlatW, kFlatH); }
};

// A Stagehand's script, a Forklift Bot's state.
struct Stagehand
{
  int enemy = -1;
  int flat = -1;
  int lean = 0; // frames leaning out to shoot
};

struct Forklift
{
  int enemy = -1;
  int charge = 0;    // cells left of this charge
  bool carrying = false;
  int carried = 0;   // cells carried
  int x0 = 0, x1 = 0; // cells: its patrol
  int fall = 0;
  int held = -1;     // on this crane's magnet
};

struct Mimic
{
  int enemy = -1;
  int breath = 0;
  int lunge = 0; // cells left of the lunge (sign: direction)
  int homeX = 0;
};

// The props of levels 1-35 on the shelves (`@ deco x y kind=prop level=n`).
struct PropDeco
{
  int level = 0;
  int x = 0, y = 0; // cells: bottom-left
  int label = 0;    // frames the label still shows
  bool seen = false;
};

// Throw Everything: something lifted or flying.
struct Thrown
{
  int kind = 0;     // 0 a crate or wall piece (a block), 1 a dummy
  float x = 0, y = 0; // cells: top-left
  int fromX = 0, fromY = 0, toX = 0, toY = 0; // cells: the throw's ends
  int t = 0;
  bool flying = false;
};

// The bot's way through (`@ route`): where to stand and what to do there.
struct WarehouseStep
{
  int x = 0, y = 0;   // cells: feet
  std::string act;    // "", "lever ID", "push", "wait ID", "ride ID", "crate ID X"
  std::string arg;
  int n = 0;
};

struct WarehouseState
{
  bool on = false;
  bool throwAll = false; // rules=throw_all
  std::vector<MetalCrate> crates;
  std::vector<Crane> cranes;
  std::vector<CraneLever> levers;
  std::vector<Flat> flats;
  std::vector<Stagehand> stagehands;
  std::vector<Forklift> forklifts;
  std::vector<Mimic> mimics;
  std::vector<PropDeco> props;
  std::vector<CellBox> foam; // crates that land in here go back to their start
  std::vector<WarehouseStep> route;
  int routeAt = 0;
  // The Magnet Gun.
  int fireHeld = 0;
  bool pulling = false;
  int pullCrate = -1, pullEnemy = -1;
  int beam = 0;      // cells: the beam drawn this frame
  int pulse = 0;     // frames the push pulse shows
  int ammoTick = 0;
  // A Forklift Bot has the runner on its forks.
  int grabbedBy = -1;
  // The TURBODUDES crate.
  int tdX = -1, tdY = -1, tdHits = 0, tdShow = 0;
  // Throw Everything.
  std::vector<std::pair<int, int>> loose; // blocks that can be lifted
  int carry = -1;   // 0 a block, 1 a dummy (lifted overhead)
  int liftT = 0;
  std::vector<Thrown> thrown;
  bool upHeld = false;
};

} // namespace gr
