#pragma once

#include <string>
#include <vector>

namespace gr
{

// Level 24, Too Many Cooks (world_kitchen.cpp, world_kitchen_draw.cpp):
// conveyor belts that carry the runner and everything on them, the chef's
// bells that reverse a station's belts, pantry flaps only a belt pushes you
// through, sauce pans that grow meatballs, the sink that eats them, the
// meatball hatch, Cleaver Bots whose cleavers stick in walls as handles,
// Egg Timers, the Pepper Grinder's sneeze, the toaster and the chef's hand.
// The Turntable bonus (rules=turntable, world_turntable.cpp) is a microwave
// interior that turns while gravity stays down.

constexpr int kBellDing = 8;       // frames from a shot to the belts flipping
constexpr int kBellRest = 30;      // frames before a bell can ring again
constexpr int kEggDial = 90;       // an Egg Timer's dial when it wakes
constexpr int kEggRing = 12;       // it rings for the last frames
constexpr int kEggWind = 10;       // each hit winds it back
constexpr int kEggRadius = 2;      // blocks: the pop's reach
constexpr int kCleaverStick = 90;  // frames a cleaver stays in a wall as a handle
constexpr int kSneezeHits = 5;     // Pepper Grinder hits on one enemy...
constexpr int kSneezeWindow = 30;  // ...within these frames make it sneeze
constexpr int kSneezeStun = 15;
constexpr int kGrinderSpin = 15;   // spin-up frames before the first shot
constexpr int kHandFrames = 60;    // the chef's hand: lift, show, set down
constexpr int kToastPop = 30;      // frames from the toaster's lever to the pop
constexpr int kToastLaunch = 12;   // cells the toast launches a runner
constexpr int kThumbsFrames = 30;  // the hand's thumbs-up at the exit (no damage taken)

// `@ belt ID rect=x0,y,x1,y speed=1/4 dir=l|r station=S loop=1 hang=1`: the
// rect row is the belt's top surface (solid; a hang bar for hang=1).
struct Belt
{
  std::string id, station;
  int x0 = 0, x1 = 0, y = 0; // blocks
  int period = 2;            // frames per cell: 4 slow, 2 medium, 1 fast
  int dir = 1;
  bool loop = false;         // items reaching the end come back at the start
  bool hang = false;         // the order rail: carries a runner hanging on it
  int flash = 0;             // frames of arrows flashing (a bell rang)
  int phase = 0;             // cells moved (the arrows scroll)
};

// `@ switch ID x= y= kind=shootable station=S bot=1|bonus stand=X`.
struct ChefBell
{
  std::string id, station;
  int x = 0, y = 0;  // blocks
  int ding = 0;      // frames until the station's belts flip
  int rest = 0;      // frames until it can ring again
  int rings = 0;
  int bot = 0;       // 1: the bot rings it on its way; 2: only for the bonus
  int standX = -1;   // blocks: where the bot rings it from
};

// `@ flap x y h=3`: opens only for a runner a belt pushes into it.
struct Flap
{
  int x = 0, y = 0, h = 3; // blocks: the column x, rows y..y+h-1
  int open = 0;            // frames it stays open
};

struct KitchenRect
{
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0; // blocks
};

// `@ spawner kind=meatball x= y= every=90 max=3 carrier=4`.
struct MeatballHatch
{
  int x = 0, y = 0;  // cells, bottom-left of the meatball
  int every = 90, max = 3, carrier = 4;
  int t = 0;
  int count = 0;     // meatballs dropped (every carrier-th one is moldy)
  std::vector<int> balls; // its meatballs (enemy indices) still about
};

// A cleaver stuck in a wall: a one-way handle for a while.
struct StuckCleaver
{
  int bx = 0, by = 0; // blocks
  int t = 0;
  int dir = 1;        // the way it flew
  bool set = false;   // it put the platform tile there
};

// `@ deco kind=basket|pot|oven rect=`: the set dressing the kitchen draws
// itself (world_kitchen_draw.cpp).
struct KitchenDeco
{
  std::string kind;
  KitchenRect r;
};

struct KitchenState
{
  bool on = false;
  std::vector<Belt> belts;
  std::vector<ChefBell> bells;
  std::vector<Flap> flaps;
  std::vector<KitchenRect> sauces, sinks;
  std::vector<MeatballHatch> hatches;
  std::vector<StuckCleaver> cleavers;
  // Sneeze counts: the enemy, its hits in the window, the frame of the first.
  struct Sneeze
  {
    int enemy = -1, hits = 0, since = 0;
  };
  std::vector<Sneeze> sneezes;
  int sneezing = 0; // frames of the last "ACHOO" (drawn)
  // The chef's hand (`@ script id=special x= y=`).
  int handX = -1, handY = -1;
  int hand = 0;            // frames left of the lift
  bool handDone = false;
  int handBaseY = 0;       // the runner's feet when it took them
  // Finished without a scratch: at the exit the hand comes back for a
  // thumbs-up (`hand` counts it down, the runner held still) first.
  bool thumbs = false, thumbsDone = false;
  std::vector<KitchenDeco> decos;
  // The toaster (`@ toaster rect=x0,y0,x1,y1 lever=x,y`).
  KitchenRect toaster{-1, -1, -1, -1};
  int leverX = -1, leverY = -1;
  int toast = 0;           // frames until the pop
  int popped = 0;          // frames of the toast up (drawn)
  int checkpoints = -1;
  std::vector<int> cpDirs; // the belts' directions at the last checkpoint
};

// The Turntable bonus (rules=turntable): the room turns about its centre
// while gravity stays down the screen. Physics runs in the room's frame
// (cells); the runner and the gems are circles.
struct TurnGem
{
  float x = 0.0f, y = 0.0f;   // cells, the centre (room frame)
  float vx = 0.0f, vy = 0.0f; // cells a frame
  int variant = 0;
  bool gone = false;          // taken or out through a grille
};

struct TurnState
{
  bool on = false;
  int w = 0, h = 0;                // blocks
  std::vector<unsigned char> solid; // 1 wall, 2 grille (gems pass), by block
  std::vector<KitchenRect> grilles;
  float cx = 0.0f, cy = 0.0f;      // cells: the centre it turns about
  float ang = 0.0f, prevAng = 0.0f; // radians, clockwise on screen
  float px = 0.0f, py = 0.0f;      // cells: the runner's middle (room frame)
  float vx = 0.0f, vy = 0.0f;
  float sx = 0.0f, sy = 0.0f, prevSx = 0.0f, prevSy = 0.0f; // the middle on screen (cells)
  bool ground = false;
  std::vector<TurnGem> gems;
  int lost = 0;                    // gems out through the grilles
  float x0 = 0.0f, y0 = 0.0f, x1 = 0.0f, y1 = 0.0f; // cells: the room's outside
};

} // namespace gr
