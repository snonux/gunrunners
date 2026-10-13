#pragma once

#include "game/collision.hpp"

#include <string>
#include <vector>

namespace gr
{

// Level 34, Zeppelin Relay (world_fleet.cpp, world_fleet_draw.cpp): a run
// left to right across a fleet of airships over a sea of clouds. Each ship
// (`@ ship`) is a one-way deck that moves as one platform, bobbing and
// drifting on sines; what sits on it (items, checkpoints, the captain's
// log) moves with it. The Grapple Gun hooks brass rings (`@ ring`) to swing
// on and yanks light enemies off the decks. Peck Parrots sink the ship they
// sit on, Sky Pirates jet in from the right, Balloon Mines rise out of the
// clouds, and the Looter sinks once you board it. The Human Cannonball bonus
// (rules=cannonball) fires the runner at three nets.

// Ships.
constexpr int kShipSinkEvery = 22;     // frames per cell a perched parrot sinks its ship (and it rises back)
constexpr int kDipFrames = 30;     // Rocco's hard landing on a small deck
constexpr int kDipFall = 6;        // cells of fall that make it hard
constexpr int kDipDeck = 24;       // cells: decks this long or shorter dip
constexpr int kLooterEvery = 10;   // frames per cell the Looter sinks once boarded
constexpr int kLooterFrames = 280; // and it is gone into the clouds
constexpr int kNameStill = 30;     // frames standing still beside a hull name (the egg)
// The Grapple Gun.
constexpr int kHookRange = 24;     // cells
constexpr float kHookSpeed = 4.0f; // cells a frame
constexpr float kHookCatch = 2.5f; // cells from a ring's middle
constexpr int kLineMin = 4, kLineMax = 26; // cells of line
constexpr int kLineWinch = 12;     // a longer line is wound in to this
constexpr int kYankFrames = 8;     // a hooked light enemy is pulled in
constexpr int kYankSpeed = 2;      // cells a frame
constexpr int kRingAmpMax = 70;    // degrees
// Sky Pirate.
constexpr int kPirateFly = 2;      // cells a frame jetting in
constexpr int kCutlassReach = 4;   // cells
constexpr int kPistolRange = 30;   // cells
// Peck Parrot.
constexpr int kParrotDive = 30;    // frames of a dive at most
constexpr int kParrotHop = 20;     // frames between the carrier's hops
// Balloon Mine.
constexpr int kMineRise = 4;       // frames per cell
constexpr int kMineFuse = 8;       // frames of red flashing
constexpr int kMineNear = 2;       // cells: this close sets it off
constexpr int kMineRadius = 4;     // cells: the shrapnel's reach
constexpr int kMineTop = 12;       // cells: gone above this row
// Human Cannonball.
constexpr float kCannonSpeed = 5.0f;   // cells a frame
constexpr float kCannonGravity = 0.25f;
constexpr float kCannonSteer = 0.125f;
constexpr int kCannonReload = 20;      // frames after a landing

// Something that rides a ship: where it sits at rest, in cells.
enum class Cargo
{
  Item,
  Box,
  Checkpoint,
  Prop,
  Enemy,
  Treasure,
};

struct Stowed
{
  Cargo kind = Cargo::Item;
  int index = 0;
  int x = 0, y = 0;
};

struct Ship
{
  std::string id, name;
  int platform = -1;       // its deck in mPlatforms
  int x0 = 0, x1 = 0, g = 0; // blocks: the deck at rest
  int baseX = 0, baseY = 0; // cells: the deck's top-left at rest
  int bobA = 0, bobP = 120;     // cells either way, frames
  int driftA = 0, driftP = 300; // cells either way, frames
  int phase = 0;
  bool balloon = true;
  bool tiles = false; // steady: its deck is in the map, not a platform
  int sinkMax = 0;   // cells a parrot sinks it at most
  int sink = 0;      // cells sunk now
  int sinkT = 0;
  int dip = 0;       // frames of Rocco's dip left
  bool looter = false;
  int looterT = -1;  // frames since it was boarded, -1 not yet
  int ox = 0, oy = 0; // cells off its rest now
  int still = 0;      // frames stood still on it (the hull name egg)
  bool named = false;
  std::vector<Stowed> cargo;
  int deckW() const { return (x1 - x0 + 1) * 2; }
};

struct Ring
{
  int x = 0, y = 0; // cells: the middle
};

struct Yank
{
  int enemy = -1;
  int frames = 0;
  int dir = 0;
};

// A Sky Pirate waits hidden until the runner passes its trigger.
struct Pirate
{
  int enemy = -1;
  int triggerX = 0; // cells
  int state = 0;    // 0 waiting, 1 jetting in, 2 on deck
  int tx = 0, ty = 0; // where it lands
  int attack = 0;   // 1 cutlass, 2 pistol (while e.tell runs)
  int fall = 0;
};

struct Parrot
{
  int enemy = -1;
  int ship = -1;   // the ship whose balloon it sits on
  int perchX = 0, perchY = 0; // cells, at rest
  int state = 0;   // 0 perched, 1 squawk, 2 diving, 3 flying back
  int t = 0;
  float fx = 0.0f, fy = 0.0f, vx = 0.0f, vy = 0.0f;
  bool carrier = false;
  int railX0 = 0, railX1 = 0; // cells: the carrier hops only here
  int railY = 0;
  int hpSeen = 0;
  int hopT = 0;
};

struct MineSpawn
{
  int x0 = 0, x1 = 0, y = 0; // cells
  int every = 90, max = 2;
  int t = 0;
  int seed = 1;
  std::vector<int> enemies;
};

struct Mine
{
  int enemy = -1;
  int fuse = -1; // frames of flashing left, -1 not lit
  int sub = 0;
};

struct Shard
{
  float x = 0.0f, y = 0.0f, vx = 0.0f, vy = 0.0f;
  int life = 0;
};

struct FleetState
{
  bool on = false;
  int t = 0; // the fleet's clock
  std::vector<Ship> ships;
  std::vector<Ring> rings;
  int ring = -1;   // the ring the runner swings from
  bool up = false; // this frame's aim (set by updatePlayer)
  int mvX = 0;
  std::vector<Yank> yanks;
  std::vector<Pirate> pirates;
  std::vector<Parrot> parrots;
  std::vector<MineSpawn> spawns;
  std::vector<Mine> mines;
  std::vector<Shard> shards;
  int killY = 1 << 20; // cells: below this, the clouds
  // The captain's log on the Looter.
  int logX = -1, logY = -1, logScore = 10000;
  bool logTaken = false;
  // The bomb bay: its hatch opens from inside.
  int bayX0 = -1, bayX1 = -1, bayY = -1; // cells: the bay's floor row (feet row + 1)
  int hatchTx = -1, hatchTy = -1;        // block
  bool hatchOpen = false;
  int dropX = -1, dropY = -1; // cells: where to stand to drop for the bay's ring (feet)
  // The grapple chain: stand at its near end (x, feet row), over at chainX1.
  int chainX0 = -1, chainY = -1, chainX1 = -1;
  // Rocco's hard landings: the highest point of this fall.
  int airTop = -1;
  // Human Cannonball (rules=cannonball).
  bool cannonball = false;
  int cannonX = 0, cannonY = 0; // cells: the muzzle's foot
  int angle = 45;               // degrees: 30, 45 or 60
  bool aimHeld = false;
  bool flying = false;
  float fx = 0.0f, fy = 0.0f, vx = 0.0f, vy = 0.0f;
  int reload = 0;
  struct Net
  {
    int x0 = 0, x1 = 0, y = 0; // cells
    int score = 0;
  };
  std::vector<Net> nets;
  int hits = 0;
};

} // namespace gr
