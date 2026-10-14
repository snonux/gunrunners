#pragma once

#include "data/level.hpp"
#include "game/collision.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace gr
{

// Level 41, Sweeps Week (world_sweeps.cpp, world_sweeps_draw.cpp): the
// Audience Vote. At each `@ vote` checkpoint the studio audience votes on a
// rule for the segment up to the next one: a ballot of a friendly, a
// neutral and a nasty option (weighted by the style score of the segment
// just played), the applause meter swinging over it for 22 frames (90
// ticks at 60 Hz), the winner shown for 8 (30 ticks). The Applause
// Cannon's hit-streak meter, the Warm-up Comic's rubber chickens, Audience
// Plants, Cue Card Drones, the turntables and the Paintball bonus
// (rules=paint) live in the same files.

// The ballot's 90 + 30 frames are 60 Hz ticks: at 15 Hz the runner would be
// through the segment before its rule came in.
constexpr int kSwSwing = 22;        // frames the ballot's applause meter swings
constexpr int kSwShow = 8;          // frames the winner shows
constexpr int kSwHitsPerStep = 4;   // Applause Cannon hits in a row per meter step
constexpr int kSwMeterMax = 4;      // x4
constexpr int kSwChickenBounces = 3; // a rubber chicken bounces this often, then lies still
constexpr int kSwKnock = 4;         // cells a chicken knocks the runner back (Rocco 2)
constexpr int kSwPlantReach = 10;   // cells: a Plant stands for a runner this close
constexpr int kSwPlantSit = 150;    // frames out of reach before a Plant sits down
constexpr int kSwComicReach = 30;   // cells: how far a Comic throws
constexpr int kSwDroneFlip = 10;    // frames a drone's card flips before it swoops
constexpr int kSwTurnFlip = 60;     // frames between a turntable's turns
constexpr int kSwTurnWarn = 15;     // its arrows light this long before a turn
constexpr int kSwLight = 10;        // cells: the runner's light in LIGHTS OUT
constexpr int kSwVirusEvery = 4;    // frames per cell the Virus drifts
constexpr int kSwLaunch = 18;       // cells a spring pad launches the runner

// The rules on the ballot, as bits: a friendly double option holds two.
enum SwRuleBit : unsigned
{
  kSwBigHeads = 1u << 0,
  kSwDoubleGems = 1u << 1,
  kSwLowGrav = 1u << 2,
  kSwQuack = 1u << 3,
  kSwTurbo = 1u << 4,
  kSwMirror = 1u << 5,
  kSwConfetti = 1u << 6, // ???
  kSwSlippery = 1u << 7,
  kSwHats = 1u << 8,
  kSwLightsOut = 1u << 9,
  kSwVirus = 1u << 10,
};
constexpr int kSwRuleCount = 11;

enum class SwCat
{
  Friendly,
  Neutral,
  Nasty,
  Secret,
  Fanfare, // the last vote: the exit fanfare only
};

struct SwOption
{
  unsigned rules = 0;
  SwCat cat = SwCat::Friendly;
  int fanfare = -1; // SwCat::Fanfare: which one
  int weight = 100; // x100
};

// `@ vote ID x y virus=X,Y fanfare=1`: the checkpoint at block (x, y) holds a
// ballot; its winner rules up to the next vote. `virus` is where VIRUS FOR
// THE RUNNER's V comes in for that segment.
struct SwVote
{
  std::string id;
  int x = 0, y = 0;           // cells: the checkpoint's bottom-left
  int virusX = -1, virusY = -1; // cells
  bool fanfare = false;
  bool done = false;
};

// A Warm-up Comic, an Audience Plant or a Cue Card Drone.
struct SwActor
{
  int enemy = -1;
  int seg = 0;          // the segment it was placed in
  bool hat = false;     // PARTY HATS
  bool popper = false;  // ???: a confetti cannon
  // Plant: 0 sitting, 1.. frames standing up (tell), then standing.
  int stand = 0;
  int away = 0;         // frames the runner has been out of reach
  // Drone: its path (cells), the point it heads for, the swoop.
  std::vector<std::pair<int, int>> path;
  int target = 1;
  int flip = 0;         // frames into the card flip
  int swoop = 0;        // 0 hovering, 1 diving, 2 climbing back
  int homeY = 0;
  int card = 0;         // BOO!, THROW THINGS!, VOTE NASTY
};

// A rubber chicken in flight.
struct SwChicken
{
  float x = 0.0f, y = 0.0f; // cells, top-left (2 x 2)
  float vx = 0.0f, vy = 0.0f;
  int bounces = 0;
  int still = 0;            // frames lying on the floor
  CellBox box() const { return {int(x), int(y), 2, 2}; }
};

// `@ turntable x y w= dir=r|l`: a stage turntable in the floor, block row y,
// w blocks wide from x; its surface carries a cell every other frame and
// turns round every 60 frames.
struct SwTurntable
{
  int x0 = 0, x1 = 0, row = 0; // blocks
  int dir = 1;                 // at frame 0
  int dirAt(int t) const { return (t / kSwTurnFlip) % 2 == 0 ? dir : -dir; }
};

// `@ crowd x y look=fan|cop|bouncer|mannequin|pilot`: an audience member
// (scenery); the cameo ones turn their heads to follow the runner.
struct SwCrowd
{
  int x = 0, y = 0; // cells, bottom-left
  int look = 0;     // 0 a fan, 1 Chrome Cop, 2 Bouncer, 3 Mannequin, 4 Black Halo's pilot
};

// Paintball's spring pad: `@ springpad x y w=`, its top the floor row y.
struct SwPad
{
  int x0 = 0, x1 = 0, row = 0; // blocks
};

struct SweepsState
{
  bool on = false;
  bool paint = false; // Paintball (rules=paint)
  std::vector<SwVote> votes;
  std::vector<SwActor> actors;
  std::vector<SwChicken> chickens;
  std::vector<SwTurntable> turntables;
  std::vector<SwCrowd> crowd;
  std::vector<CellBox> arenas; // marked arenas: several Comics throw, several Plants stand
  // The vote.
  int cur = 0;            // votes done: the segment the runner is in
  int ballot = -1;        // the vote whose ballot is up, -1 none
  int ballotT = 0;        // frames into it
  SwOption options[3];
  int winner = 0;
  int style = 0;          // the style score the ballot was weighted by
  unsigned rules = kSwBigHeads; // in force now (the rehearsal's BIG HEADS)
  unsigned lastWin = kSwBigHeads;
  int fanfare = -1;       // the exit fanfare the last vote picked
  bool played = false;    // the fanfare played at the exit
  unsigned force = 0;     // tests: every ballot's winner
  bool quackOk = false;   // every duck of levels 1-40 found (the profile)
  std::uint32_t rng = 0x41u * 2654435761u;
  // The segment's style: hearts lost, the meter's best step.
  int lost = 0, best = 1;
  // The Applause Cannon.
  int meter = 1, streak = 0;
  int lastHp = 0, lastGems = 0;
  // SLIPPERY FLOORS: the runner's speed in eighths of a cell a frame.
  int slide = 0, slideCarry = 0;
  int clock = 0;          // turntables, TURBO FOR EVERYONE's fourth frames
  int boost = 0;          // TURBO FOR EVERYONE: frames run
  // Paintball: per block 0 nothing, 1 a floor top, 2 painted, 3 a wall, 4 painted wall.
  std::vector<std::uint8_t> paints;
  int tops = 0, painted = 0, goal = 80, chant = 0;
  std::vector<SwPad> pads;
  int seatX = -1, seatY = -1; // `@ seatladder x y`: a ladder block once the seat over it is broken
};

// "LOW GRAVITY + DOUBLE GEMS", "???", "BRASS BAND"...
std::string sweepsOptionName(const SwOption& o);
// One rule's name on the ballot.
const char* sweepsRuleName(unsigned bit);

} // namespace gr
