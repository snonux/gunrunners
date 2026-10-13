#pragma once

#include "game/collision.hpp"

#include <array>
#include <string>
#include <vector>

namespace gr
{

// Level 35, The Transmitter (world_tower.cpp, world_tower_draw.cpp): a climb
// up a lattice mast that turns around the runner. `flags=wrapx` joins the
// map's left and right edges into a ring; the world keeps the runner near
// the middle of the map by turning the whole tower (tiles, actors, the
// camera) a few blocks at a time, so nothing near the runner ever sits
// across the seam. Mast Climbers chase up ladders, Relay Drones orbit the
// mast, Feedback Nodes push the runner off narrow ledges; the Satellite
// Swarm's drones fire with the runner and soak up shots; on the top deck
// the Halcyon Titan is delivered for the fight. The Copycat bonus
// (rules=copycat) makes every enemy copy the runner's moves.

// The turning tower.
constexpr int kTowerKeep = 16; // cells either side of the middle before the tower turns
// Satellite Swarm.
constexpr int kSwarmDrones = 3;
constexpr int kSwarmRadius = 4;    // cells from the runner's middle
constexpr int kSwarmLap = 30;      // frames a lap
constexpr int kDroneOffline = 60;  // frames after soaking up a shot
// Mast Climber.
constexpr int kClimberReach = 4;   // cells: the wrench's swipe
constexpr int kClimberTell = 10;
// Relay Drone.
constexpr int kRelayRange = 24;    // cells: fires from this close on the front half
constexpr int kRelayTell = 10;     // frames of muzzle flash
// Feedback Node.
constexpr int kNodeRadius = 12;    // cells: the ring's reach
constexpr int kNodeTell = 22;      // frames of glow and rising hum
constexpr int kNodePush = 8;       // frames of push, a cell each (Rocco: 4)
constexpr int kNodeCool = 45;
// Halcyon Titan.
constexpr int kTitanW = 10, kTitanH = 12; // cells
constexpr int kTitanIntro = 90;
constexpr int kTitanGap = 30;      // frames between demo attacks
constexpr int kTitanCore = 45;     // frames the core is open after an attack
constexpr int kLaserTell = 22, kLaserFrames = 8, kLaserReach = 40;
constexpr int kMissileTell = 22, kMissileRadius = 2;
constexpr int kFlameTell = 15, kFlameFrames = 45, kFlameReach = 12;
constexpr int kLatticeTell = 15, kLatticeCool = 30;
constexpr int kTitanStompEvery = 90, kTitanStompTell = 15, kTitanWaveSpeed = 2, kTitanWaveReach = 60;
constexpr int kPanelTell = 15, kPanelOpen = 60, kPanelOpenBack = 150, kPanelGap = 30, kSteamReach = 4;
constexpr int kTeeter = 30;
// Easter eggs.
constexpr int kDizzyLaps = 42;     // laps of the tower in one life
constexpr int kDizzyFrames = 300;
constexpr int kAnthemStill = 30;   // frames under the flag

// A hatch in the tower: up moves the runner to the linked door.
struct TowerDoor
{
  std::string id, to;
  int x = 0, y = 0; // cells: bottom-left, its feet row
  int link = -1;
};

// The bot's way up the spiral (`@ route`): spots to stand on, in order.
struct Waypoint
{
  int x = 0, y = 0; // cells: feet
};

// A Mast Climber that waits until the runner climbs a ladder.
struct ClimberWait
{
  int enemy = -1;
  int ladderX = 0; // cells: the ladder's left cell
};

struct Climber
{
  int enemy = -1;
  int t = 0;
  int climb = 0;   // 1 climbing up, -1 down, 0 on its floor
  int giveUp = -1; // checkpoints reached when it set off; two more and it stops
  int fall = 0;
};

struct RelayDrone
{
  int enemy = -1;
  int row = 0; // cells: its flight row (bottom)
  int dir = 1;
};

struct FeedbackNode
{
  int enemy = -1;
  int state = 0; // 0 idle, 1 glowing, 2 pushing, 3 cooling
  int t = 0;
};

struct SwarmDrone
{
  int offline = 0; // frames left offline
  float x = 0.0f, y = 0.0f; // cells: its middle now
};

enum class TitanPhase
{
  Waiting,
  Intro,
  Demo,     // phase 1: the demo reel
  Orbit,    // phase 2
  Overheat, // phase 3
  Topple,
  Done,
};

enum class TitanMove
{
  None,
  Laser,
  Missiles,
  Flame,
};

struct Titan
{
  bool on = false;
  TitanPhase phase = TitanPhase::Waiting;
  int x = 0, y = 0;         // cells: left, feet row
  int prevX = 0, prevY = 0;
  int landX = 0;            // cells: where it lands
  int deckY = 0;            // cells: the deck's feet row
  int facing = 1;
  int t = 0;                // frames into the current phase step
  int sub = 0;              // half-cell steps
  std::array<int, 3> hp{30, 30, 32};
  TitanMove move = TitanMove::None;
  int nextMove = 0;         // 0 laser, 1 missiles, 2 flame
  int moveT = 0;            // frames into the move (the tell first)
  int core = 0;             // frames the core stays open
  std::array<int, 4> cross{}; // cells: missile crosshairs on the deck
  int lattice = 0;          // frames to the next lattice shot's tell
  std::array<int, 4> panel{8, 8, 8, 8}; // front-left, front-right, back-left, back-right
  int open = -1;            // the open panel, -1 none
  int openT = 0;            // frames into its tell + opening
  int gapT = 0;
  int stompT = 0;
  int wave = -1;            // cells: the shockwave's reach from its middle, -1 none
  int fallT = 0;
  int flash = 0;
  int banner = 0;           // frames the holo banner shows
  std::string bannerText;
  int ship = 0;             // frames of the cargo airship's pass
  int total() const { return hp[0] + hp[1] + hp[2]; }
  CellBox box() const { return {x, y - kTitanH + 1, kTitanW, kTitanH}; }
};

struct TowerState
{
  bool on = false;
  bool wrap = false;  // flags=wrapx
  int rot = 0;        // cells the tower has turned, 0..width
  int turns = 0;      // how many times (the bot replans)
  std::vector<TowerDoor> doors;
  std::vector<Waypoint> route;
  int routeAt = -1;   // the last waypoint reached
  std::vector<ClimberWait> waits;
  std::vector<Climber> climbers;
  std::vector<RelayDrone> relays;
  std::vector<FeedbackNode> nodes;
  int pushT = 0, pushDir = 0; // a Feedback Node's push on the runner
  std::array<SwarmDrone, kSwarmDrones> swarm{};
  int swarmT = 0;
  Titan titan;
  // The Titan's sales brochure.
  int brochureX = -1, brochureY = -1, brochureScore = 10000;
  bool brochureTaken = false;
  // Easter eggs: laps walked this life, the dizzy wobble, the waltz; the flag.
  int walked = 0;
  int dizzy = 0;
  bool waltz = false;
  int flagX = -1, flagY = -1, flagT = 0;
  bool anthem = false;
  // Copycat (rules=copycat): pits that catch the dummies.
  bool copycat = false;
  std::vector<CellBox> pits;
  int caught = 0;
  std::vector<int> dummyFall; // per enemy: frames of fall, -1 caught
  std::vector<int> dummyVy;
};

} // namespace gr
