#pragma once

#include "game/collision.hpp"

#include <string>
#include <vector>

namespace gr
{

// Level 28, Toon Town Throwdown (world_toon.cpp, world_toon_draw.cpp):
// cartoon physics. A runner who walks off a ledge keeps running on air at
// the height they left from while they hold that way; let go or turn and
// they hold up an "uh-oh" sign, then fall. Painted tunnels on walls are
// passages (real, with a shadow) or solid paint (a bonk). The Portable
// Hole opens a wall or a floor for a while. Buddy Boom waits in the big
// top. The Inkwell bonus (rules=ink) is InkState, also in world_toon.cpp.

constexpr int kUhOh = 8;          // frames of the "uh-oh" sign before the fall
constexpr int kUhOhNova = 16;     // Nova floats one beat longer
constexpr int kBonk = 15;         // frames dazed after running into painted rock
constexpr int kHoleOpen = 75;     // frames a Portable Hole stays open
constexpr int kHoleHold = 6;      // frames fire is held to throw it (a tap is a blaster shot)
constexpr int kPianoTell = 22;    // frames of the piano's shadow before it drops
constexpr int kPianoStay = 150;   // frames a dropped piano stands as a step
constexpr int kGoonFlat = 45;     // frames a Rubber Goon lies flat after a hit
constexpr int kGoonBounce = 10;   // cells a flat Rubber Goon bounces a runner
constexpr int kBuddyScore = 50000;
constexpr int kBuddyHp = 30;      // per phase
constexpr int kSlamTell = 15;     // the mallet overhead
constexpr int kEraseTell = 22;    // the eraser's shadow on a floor segment
constexpr int kInkMax = 160;      // cells of line in the inkwell
constexpr int kInkRefill = 80;
constexpr int kInkFade = 600;     // frames a line lasts

// `@ tunnel x y real=0|1`: a tunnel painted on a wall, 2 x 3 blocks (x, y
// its top-left block).
struct ToonTunnel
{
  int x = 0, y = 0;
  bool real = false;
};

// The open Portable Hole: the blocks it took out of a wall or a floor.
struct ToonHole
{
  bool open = false;
  bool wall = false;
  int t = 0;              // frames left open
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0; // blocks
  std::vector<int> saved; // the tiles it took, row by row
  bool touched = false;   // the runner has been in it (out again, it is picked up)
  int tunnel = -1;        // a fake tunnel it opened (the programmer art behind it)
};

// The Portable Hole in flight.
struct ToonToss
{
  bool on = false;
  float fx = 0.0f, fy = 0.0f, vx = 0.0f, vy = 0.0f; // cells
};

// A Piano Bird's piano: its shadow grows, it drops, it stands as a step.
struct Piano
{
  int x = 0;              // cells: left (3 blocks wide, 2 tall)
  float y = 0.0f;         // cells: top while falling
  float vy = 0.0f;
  int state = 0;          // 0 the shadow grows, 1 falling, 2 standing, 3 gone
  int t = 0;
  int floor = 0;          // cells: the row it will land on (top of the ground)
  int bx = 0, by = 0;     // blocks: where it stands
  std::vector<int> saved; // the tiles under it while it stands
};

// `@ spawner kind=piano_bird trigger=x`: a Piano Bird flies in east along
// row 8 once the runner passes block x (one at a time).
struct BirdSpawner
{
  int trigger = 0; // cells
  int enemy = -1;
  bool fired = false;
};

struct MalletWave
{
  float x = 0.0f; // cells
  int y = 0;      // cells: the floor row it runs along (its bottom)
  int dir = 1;
};

struct CreamPie
{
  float x = 0.0f, y = 0.0f, vx = 0.0f, vy = 0.0f; // cells
  int side = 0;   // the half of the screen it came from (-1 left, 1 right)
};

enum class BuddyState
{
  Waiting,  // before the runner is in the arena
  Walk,     // phase 1: to within 6 blocks
  Slam,     // the mallet overhead, then down
  Spin,     // between phases
  Hop,      // phase 2: to the other platform
  Pie,      // the wind-up, then a pie
  Pencil,   // phase 3: up on the giant pencil
  Draw,     // a new floor line in the air (outline, then solid)
  Stand,    // on his drawing
  Erase,    // the eraser's shadow on a floor segment
  Dizzy,    // at 0, on his drawing
  Fall,     // into the ink
  Done,
};

// Buddy Boom (`@ buddy x y arena= door= segments= drops= turbo= ink=`).
struct Buddy
{
  bool on = false;
  BuddyState state = BuddyState::Waiting;
  int phase = 1;
  int hp = kBuddyHp;
  float x = 0.0f, prevX = 0.0f; // cells: the body's left
  float y = 0.0f, prevY = 0.0f; // cells: the row under his feet... his bottom row
  float vx = 0.0f, vy = 0.0f;
  int dir = -1;
  int t = 0;
  int cool = 30;
  int attacks = 0;
  int flash = 0;
  int nose = 0;               // frames the nose flashes red (phase 1's weak point)
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;       // blocks: the arena
  int doorX = 0, doorY0 = 0, doorY1 = 0;    // blocks
  bool doorShut = false;
  int floorY = 0;             // blocks: the arena floor's row
  int segX0 = 0, segW = 5, segs = 8;        // blocks: the erasable floor segments
  std::vector<int> erased;    // per segment: frames left gone (0 there)
  int eraseSeg = -1;          // the segment under the eraser's shadow
  int platX[2][2] = {{0, 0}, {0, 0}};       // blocks: the ground-30 platforms
  int platY = 0;
  int target = 0;             // the platform he hops to
  int lineY = -1, lineX0 = 0, lineX1 = 0;   // blocks: his drawing
  bool lineSolid = false;
  std::vector<int> lineSaved;
  int dizzyHits = 0;          // blaster hits on the line while he sits
  std::vector<MalletWave> waves;
  std::vector<CreamPie> pies;
  int whiteL = 0, whiteR = 0; // frames a half of the screen is white with cream
  int birdT = 0;              // frames to the next Piano Bird (phase 2)
  int dropX[2] = {0, 0}, dropY = 0;         // blocks: the phase drops
  int turboX = -1, turboY = 0;              // blocks: phase 3's Turbo
  bool turboDropped = false;
  int startX = 0;
  float deflate = 0.0f;       // in the ink: 0 to 1
  bool holed = false;         // dropped through a Portable Hole (THE END?)
  static constexpr int kW = 9, kH = 15;
};

struct ToonState
{
  bool on = false;
  // The air-run: the way, how long, the "uh-oh" after it.
  int airDir = 0;
  int airT = 0;
  int uhOh = 0;
  bool jumpHeld = false; // this frame (a flat Rubber Goon bounces a held jump higher)
  int bonk = 0;          // frames dazed
  bool bonkLatch = false; // still against the paint (no second bonk until the runner backs off)
  std::vector<ToonTunnel> tunnels;
  ToonHole hole;
  ToonToss toss;
  std::vector<Piano> pianos;
  std::vector<BirdSpawner> spawners;
  Buddy buddy;
  // The canyon's sign: air-running past it, the runner tips their hat.
  int signX = -1, signY = 0; // blocks
  int tip = 0;
  bool tipped = false;
  // Secrets: the cartoon studio, Buddy's dressing room.
  CellBox studio{-100, -100, 0, 0}, dressing{-100, -100, 0, 0};
  bool studioFound = false, dressingFound = false;
  int prizeX = -1, prizeY = 0, prizeScore = 25000; // blocks
  bool prizeTaken = false;
  std::string prizeKind;
  int fullhealX = -1, fullhealY = 0;              // blocks
  bool fullhealUsed = false;
  std::vector<CellBox> ponds; // ink: a heart, and back on the nearest floor still there
  int retro = 0;              // frames of the programmer art behind the fake tunnel
};

// The Inkwell bonus (rules=ink): hold fire to draw a one-way line.
struct InkBlock
{
  int x = 0, y = 0; // blocks
  int age = 0;
};

struct InkRefill
{
  int x = 0, y = 0; // blocks
  bool taken = false;
};

struct InkState
{
  bool on = false;
  int ink = kInkMax;
  std::vector<InkBlock> blocks;
  bool drawing = false;
  float penX = 0.0f, penY = 0.0f; // cells
  int penDx = 1, penDy = 0;
  std::vector<InkRefill> refills;
  int startX = 0, startY = 0; // cells: where a fall puts the runner back
};

} // namespace gr
