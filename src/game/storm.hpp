#pragma once

#include "game/collision.hpp"

#include <string>
#include <vector>

namespace gr
{

// Level 31, Storm Relay (world_storm.cpp, world_storm_draw.cpp): a climb up
// a sea cliff in a thunderstorm. Each `@ lightning` zone strikes its highest
// metal (`@ metal` rects, Kite Drones, machines, a stuck Tesla rod) every
// `period` frames after a 22-frame glow; for 4 frames anything touching the
// struck metal is hurt. Strikes power dead `@ machine`s (a lift, a hatch's
// generator, the lighthouse lamp). Storm Gulls swoop, Kite Drones fly figure
// eights on their tethers and Foghorns blow runners back. Also the Bungee
// bonus (rules=bungee).

constexpr int kStrikeGlow = 22;    // frames the target glows before a strike
constexpr int kStrikeHurt = 4;     // frames a strike hurts what touches its metal
constexpr int kRodFlight = 72;    // cells a thrown rod flies
constexpr int kRodLife = 300;      // frames a stuck rod stays unstruck
constexpr int kRodsMax = 2;        // stuck at once (a third pulls out the oldest)
constexpr int kRodArc = 6;         // damage to each enemy close to a struck rod
constexpr int kRodReach = 8;       // cells: how close
constexpr int kArcFrames = 4;      // frames the white arcs show
// Storm Gull: wheels this high over the runner, then a V-dive.
constexpr int kGullWheel = 16;     // cells
constexpr int kGullDive = 2;       // cells a frame
constexpr int kGullEvery = 45;     // frames between spawns
// Kite Drone: a figure eight on its tether.
constexpr int kKiteRadius = 10;    // cells
constexpr int kKiteLoop = 90;      // frames a loop
constexpr int kKiteReach = 12;     // cells from its anchor when it turns at you
constexpr int kKiteChase = 12;     // frames it comes at you
constexpr int kSparkFrames = 8;    // a spark running down the tether
// Foghorn: a cone of sound in its facing.
constexpr int kFogLength = 16;     // cells
constexpr int kFogHeight = 6;      // cells
constexpr int kFogPush = 8;        // frames of push (a cell a frame)
// The lamp's ship answers in Morse.
constexpr int kMorseDot = 4, kMorseDash = 12;
// Bungee (rules=bungee).
constexpr float kBungeeRest = 12.0f;  // cells of slack rope
constexpr float kBungeeMax = 28.0f;   // cells: it stretches no further
constexpr float kBungeePull = 1.0f / 16.0f; // cells a frame^2 per cell of stretch
constexpr float kBungeeFling = 4.0f;  // cells a frame at most
constexpr float kBungeeFall = 0.25f;  // gravity on the rope
constexpr int kBungeeRing = 200;      // points a ring

// `@ metal [id] rect=`: metal that conducts. Rects with the same id are one
// piece.
// G-U-N in Morse from the ship on the horizon: is its light on, t frames
// after the lamp lit?
bool morseLight(int t);

struct MetalGroup
{
  std::string id;
  std::vector<CellBox> boxes;
  int top = 0; // cells
};

enum class MachineKind
{
  Lift,      // its platform (`lift=`) runs once powered
  Door,      // its `opens=` blocks open for good
  Generator, // the same, and `ladder=x` turns that column of them into ladder
  Lamp,      // the lighthouse lamp: lights, and a ship answers
};

// `@ machine ID kind= rect=`: a dead machine in a metal housing.
struct Machine
{
  std::string id;
  MachineKind kind = MachineKind::Door;
  CellBox box{0, 0, 0, 0};
  std::string liftId;
  int platform = -1;
  int ox0 = 0, oy0 = 0, ox1 = -1, oy1 = -1; // blocks: what opens
  int ladderX = -1;                         // block column that turns into ladder
  bool powered = false;
};

// A Tesla rod stuck in a wall or a machine.
struct Rod
{
  int x = 0, y = 0; // cells: the rod's tip block (2 x 1)
  int dir = 1;
  int life = kRodLife;
  int machine = -1;
  CellBox box() const { return {x, y, 2, 1}; }
};

enum class TargetKind
{
  None,
  Metal,
  Machine,
  Rod,
  Enemy,
};

struct StrikeTarget
{
  TargetKind kind = TargetKind::None;
  int index = -1;
};

// `@ lightning ID zone= period= glow= damage= cycle=a,b,c`.
struct StormZone
{
  std::string id;
  CellBox rect{0, 0, 0, 0};
  int period = 90, glow = kStrikeGlow, damage = 2;
  int t = 0;    // frames since the last strike
  int hurt = 0; // frames left of the strike's hurt
  StrikeTarget target;
  std::vector<int> cycle; // metal groups that take the strike in turn
  int cycleAt = 0;
  int extra = -1;         // the cycle's group this strike
  int boltX = 0, boltY = 0; // cells: where the bolt came down
  std::vector<CellBox> live; // what the strike touches while it hurts
  bool glowing() const { return target.kind != TargetKind::None && t >= period - glow; }
};

struct Kite
{
  int enemy = -1;
  int ax = 0, ay = 0; // cells: the tether's anchor
  int phase = 0;
  int state = 0; // 0 loop, 1 the tell, 2 coming at you, 3 back to the loop
  int t = 0;
  int spark = 0; // frames left of a spark down the tether
};

// `@ spawn rect= kind=storm_gull count= max=`.
struct GullSpawn
{
  CellBox rect{0, 0, 0, 0};
  int def = -1;
  int count = 3, max = 2;
  int spawned = 0;
  int cool = 0;
  std::vector<int> enemies;
};

struct Arc
{
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0; // cells
  int frames = kArcFrames;
};

struct StormState
{
  bool on = false;
  std::vector<MetalGroup> metals;
  std::vector<Machine> machines;
  std::vector<Rod> rods;
  std::vector<StormZone> zones;
  std::vector<Kite> kites;
  std::vector<GullSpawn> spawns;
  std::vector<Arc> arcs;
  // A Foghorn's blast on the runner.
  int push = 0, pushDir = 0, pushTick = 0;
  // Easter eggs: the fry gull, the lamp's Morse.
  int fryX = -1, fryY = -1; // cells
  int fryFlown = 0;         // frames since it flapped off, 0 perched
  int morse = -1;           // frames since the lamp lit, -1 dark
  int lamp = -1;            // the lamp machine
  // Bungee (rules=bungee).
  bool bungee = false;
  float postX = 0.0f, postY = 0.0f; // cells
  CellBox tower{0, 0, 0, 0};        // the lighthouse behind the post (drawn only)
  float bx = 0.0f, by = 0.0f;       // the runner's middle, in cells
  float vx = 0.0f, vy = 0.0f;
  bool flying = false;
  bool stretching = false; // a direction held on the ground: letting go flings
  std::vector<std::pair<int, int>> rings; // cells: middles
  std::vector<bool> ringTaken;
};

} // namespace gr
