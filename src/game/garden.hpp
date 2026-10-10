#pragma once

#include <string>
#include <vector>

namespace gr
{

// Level 25, Tiny Kingdom (world_garden.cpp, world_garden_draw.cpp): the
// garden's 810-frame day/dusk cycle (its platforms are timer layers; this
// keeps the sundial, the time-lapse sweep and the pond), buds and seeds the
// Chrono Mister fast-forwards, Soldier Ant columns, Jumping Spiders,
// Bombardier Beetles, the narrator, the dandelion that carries the runner
// to the exit, the rare butterfly and the ladybug in front of the bonus.
// The Dandelion Drift bonus (rules=glide) is in world_glide.cpp.

constexpr int kGardenCycle = 810;   // frames: day 0-404, dusk 405-809
constexpr int kGardenHalf = 405;
constexpr int kGardenSweep = 30;    // the last frames of each half: the time-lapse
constexpr int kGardenDroop = 45;    // platforms about to go droop for these frames
constexpr int kBudOpen = 150;       // frames a misted bud stays open
constexpr int kSeedSprout = 20;     // frames from misting to a stalk
constexpr int kDandelionSeed = 15;  // frames from misting to the seed head
constexpr int kAntHostile = 150;    // frames a provoked column stays angry
constexpr int kAntTell = 10;
constexpr int kMistAmmoEvery = 4;   // frames of mist per unit of ammo
constexpr int kMistDamageEvery = 6; // frames of mist per point of damage
constexpr int kStillLine = 450;     // frames standing still for the narrator's aside

// `@ bud x y w=3`: a closed bud; misted, a one-way platform for a while.
struct Bud
{
  int x = 0, y = 0, w = 3; // blocks: the row it opens on, x..x+w-1
  int open = 0;            // frames left open
};

// `@ seed x y h=`: misted, it sprouts into a stalk (a ladder) h blocks tall.
struct Seed
{
  int x = 0, y = 0, h = 6; // blocks: the foot of the stalk
  int sprout = 0;          // frames until it is up (0 = not misted yet)
  bool grown = false;
};

// `@ antcolumn ID path=x0,y;x1,y count=5 period=300 carrier=1`.
struct AntColumn
{
  std::string id;
  int x0 = 0, x1 = 0, y = 0; // cells: the march's ends, feet row
  int period = 300;
  int phase = 0;             // frames: where in its period it is
  int hostile = 0;           // frames left angry (after the tell)
  int tell = 0;
  bool carrier = false;
  bool homing = false;       // calmed down: walking back to the anthill
  std::vector<int> ants;     // enemy indices
};

// `@ narrate ID rect= runner=any|dash|rocco|nova on=enter|hurt|seedhead line=""`.
struct Narration
{
  std::string id, runner, on, line;
  int x0 = 0, y0 = 0, x1 = -1, y1 = -1; // blocks
  bool said = false;
};

// A Jumping Spider's leap, by enemy index.
struct SpiderLeap
{
  int enemy = -1;
  float fx = 0.0f, fy = 0.0f, vx = 0.0f, vy = 0.0f;
};

struct GardenState
{
  bool on = false;
  bool cycle = false;        // the level runs the day/dusk cycle (it has day_/dusk_ layers)
  std::vector<Bud> buds;
  std::vector<Seed> seeds;
  std::vector<AntColumn> columns;
  std::vector<Narration> lines;
  std::vector<SpiderLeap> leaps;
  // `@ fluid kind=pond rect=`: falling in costs a heart and puts the runner
  // back on the last solid ground.
  struct Pond
  {
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0; // cells
  };
  std::vector<Pond> ponds;
  int safeX = -1, safeY = -1; // the last solid ground stood on (cells)
  // The dandelion (`@ dandelion ID x y`) and its seed head (`@ platform id=seedhead`).
  int dandelionX = -1, dandelionY = -1;
  int seedhead = -1;         // platform index
  int seeding = 0;           // frames until the head goes to seed
  bool seeded = false;
  // The rare butterfly (`@ prize kind=rare_butterfly path= speed=`).
  bool butterfly = false;
  std::vector<std::pair<int, int>> bflyPath; // cells
  float bx = 0.0f, by = 0.0f;
  int bflyTarget = 1;
  int bflyState = 0;         // 0 fluttering, 1 caught, 2 scared off
  int bflyScore = 10000;
  // The ladybug in front of the bonus door (`@ ladybug x y`).
  int ladybugX = -1, ladybugY = -1;
  int ladybug = 0;           // times misted: 1 spells G-U-N (the door wakes), 2 spells 4-2
  bool ladybugMisting = false;
  int mist = 0;              // frames the Chrono Mister has been held
  int mistFace = 1;
  int still = 0;             // frames the runner has stood still
  bool stillSaid = false;
  bool hurtSaid = false;
  int subtitle = 0;          // frames left of the current narrator line
  std::string subtitleText;
};

// Level 25's bonus, Dandelion Drift (rules=glide): the runner hangs from a
// seed head that drifts right; Up and Down steer, `@ wind rect= dir=u|d
// push=` gusts lift or drop it, thorns (spikes, solid blocks) and
// dragonflies bounce it back 4 blocks.
constexpr float kGlideDrift = 0.5f; // cells a frame, to the right
constexpr float kGlideSteer = 0.5f; // cells a frame, up or down
constexpr int kGlideBounce = 12;    // frames a bounce back takes

struct GlideWind
{
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0; // cells
  int dir = -1;                       // -1 up, 1 down
  int push = 1;                       // half cells a frame
};

struct Dragonfly
{
  float x = 0.0f, y = 0.0f, prevX = 0.0f, prevY = 0.0f; // cells: its middle
  float y0 = 0.0f, y1 = 0.0f;                           // its beat
  float speed = 0.25f;
  int dir = 1;
};

struct GlideState
{
  bool on = false;
  float fx = 0.0f, fy = 0.0f;     // the runner, in cells (its top-left)
  int bounce = 0;                 // frames of bouncing back left
  float bounceFrom = 0.0f, bounceTo = 0.0f;
  std::vector<GlideWind> winds;
  std::vector<Dragonfly> flies;
  int bounces = 0;
};

} // namespace gr
