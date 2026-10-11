#pragma once

#include "game/collision.hpp"

#include <string>
#include <vector>

namespace gr
{

// Level 32, Dish Array (world_dish.cpp, world_dish_draw.cpp): a run across
// the desert past a radio telescope array. The big dishes track a satellite
// on one shared 900-frame cycle (east, overhead, west and back); their rims
// and feed arms are `@ platform ... mode=script path=E;O;W` rides, and their
// struts `@ layer driver=script state=east,over` are solid only in those
// poses. The teaching dish (`@ dish small=1`) turns on its own 180-frame
// cycle while the runner is near. The Signal Jammer turns robots (Vulture
// Drones, Scorpion Mechs) on their friends or a `@ dummy`. Sand Sharks swim
// in `@ sand`. Also the Heatstroke bonus (rules=swap_lr).

enum class DishPose
{
  East,
  Over,
  West,
};

constexpr int kDishCycle = 900;  // frames: the big dishes' cycle
constexpr int kDishGroan = 30;   // frames of groan before a big turn
constexpr int kSmallCycle = 180; // frames: the teaching dish's cycle
constexpr int kSmallGroan = 10;
constexpr int kSmallRadius = 6;  // blocks: the teaching dish turns while the runner is this close
// Signal Jammer: a cone in the facing.
constexpr int kJamLength = 10;   // cells
constexpr int kJamHeight = 6;    // cells
constexpr int kJamLife = 6;      // frames
constexpr int kJamFrames = 60;   // a robot is jammed this long
constexpr int kJamStun = 8;      // an organic enemy flinches this long
// Sand Shark.
constexpr int kSharkBreach = 20; // frames out of the sand
constexpr int kSharkReach = 2;   // cells (middle to middle): it breaches when the runner is this close
constexpr int kSharkCircle = 6;  // cells: it circles a runner off the sand this far off
constexpr int kSharksAwake = 2;  // at most this many move at once
// Vulture Drone.
constexpr int kVultureHigh = 10;  // cells: it circles this far over the ground under it (give or take 3)
constexpr int kScrapCap = 3;     // cells a frame
constexpr int kScrapEnemy = 2;   // damage to an enemy a jammed drone drops on
// Scorpion Mech.
constexpr int kBeamLength = 32;  // cells
constexpr int kAimMax = 30;      // frames the aim line turns
constexpr int kAimLock = 8;      // frames it holds before firing
constexpr int kBeamFrames = 4;   // frames the beam shows
constexpr int kBeamEnemy = 4;    // damage to an enemy a jammed mech fires on
constexpr int kMechSight = 32;   // cells: it aims at a runner this close
// Heatstroke (rules=swap_lr): left and right swap every 150 frames, the
// screen wobbling for the 22 before.
constexpr int kSwapEvery = 150;
constexpr int kSwapWobble = 22;
constexpr int kEggFrames = 300;  // the minidish easter egg's music

// Where a cycle has the dishes at frame t: turning from `from` to `to`, k of
// n frames along (n == 0: holding `from`), and the groan before a turn.
struct DishPhase
{
  DishPose from = DishPose::East, to = DishPose::East;
  int k = 0, n = 0;
  bool groan = false;
};
DishPhase dishPhase(int t, bool small);

// A rim or feed arm: a platform with its three poses (cells, top-left).
struct DishRide
{
  int platform = -1;
  int dish = -1; // the `@ dish` (small) it belongs to, -1 the big cycle
  int px[3] = {0, 0, 0}, py[3] = {0, 0, 0};
};

// A strut: a script layer solid in some poses.
struct DishStrut
{
  int layer = -1;
  int dish = -1;
  bool solidIn[3] = {false, false, false};
};

// `@ dish id small=1 radius=`: the teaching dish's own clock.
struct SmallDish
{
  std::string id;
  int x = 0, y = 0;  // blocks: its middle
  int radius = kSmallRadius;
  int t = 0;
  bool turning = false; // the runner is near
};

// `@ minidish x y`: shot, it turns one pose.
struct MiniDish
{
  int x = 0, y = 0; // cells, bottom-left
  int pose = 0;     // 0 east, 1 overhead, 2 west
  int turn = 0;     // frames of the turn's wobble
};

struct JamCone
{
  int x = 0, y = 0; // cells: the mouth (top of the cone's middle row)
  int dir = 1;
  int life = kJamLife;
  std::vector<int> hit;
  CellBox box() const;
};

struct Scrap
{
  int x = 0, y = 0;  // cells, top-left (2 x 2)
  int vy = 0;
  bool carrier = false;
  bool jammed = false; // dropped by a jammed drone: it hurts enemies
  int groundY = 0;     // where it lands (for the shadow)
  int fall = 0;        // frames fallen
};

struct Beam
{
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0; // cells
  int frames = kBeamFrames;
};

// A robot's state the Jammer turns.
struct Robot
{
  int enemy = -1;
  int jam = 0;      // frames left jammed
  int target = -1;  // jammed: the enemy it goes for (-2 the dummy, -1 none)
  // Vulture Drone: its circle.
  int zx0 = 0, zx1 = 0; // cells: the stretch it circles over
  int phase = 0;
  // Scorpion Mech: the aim line's end and its state.
  int aimState = 0; // 0 looking, 1 turning, 2 locked
  int aimT = 0;
  int ax = 0, ay = 0; // cells
};

// A Sand Shark under the sand.
struct Shark
{
  int enemy = -1;
  int state = 0;  // 0 swimming, 1 the bulge (tell), 2 breached
  bool awake = false;
  int t = 0;
  int sx0 = 0, sx1 = 0, surface = 0; // cells: its sand's x range and top row
};

struct DishState
{
  bool on = false;
  int t = 0;                // the big cycle's clock
  int satellitePeriod = kDishCycle;
  std::vector<DishRide> rides;
  std::vector<DishStrut> struts;
  std::vector<SmallDish> small;
  std::vector<CellBox> sand;
  std::vector<MiniDish> minis;
  std::vector<JamCone> cones;
  std::vector<Scrap> scraps;
  std::vector<Beam> beams;
  std::vector<Robot> robots;
  std::vector<Shark> sharks;
  int dummyX = -1, dummyY = -1; // cells: its middle
  // The cactus's hat (shot: it pops up and lands back on).
  int cactusX = -1, cactusY = -1, hatUp = 0;
  int egg = 0; // frames left of the minidish easter egg's music
  bool eggDone = false;
  // Heatstroke.
  bool swapLr = false;
  bool harmlessScrap = false; // the bonus: scrap pushes back instead
  int swapT = 0;
  bool swapped = false;
};

} // namespace gr
