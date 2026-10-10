#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace gr
{

// Level 22, Dry Gulch (world_west.cpp, world_west_draw.cpp): fuses that burn
// both ways from where they are lit to their TNT barrels, barrels that hiss
// and blow (chaining into fuses and barrels near them), a drawbridge a
// barrel tips over, the Six-Shooter's cylinder and its ricochets off
// `@ metal`, Duelists (the church bell rings twice, then they draw),
// Window Bandits and Tumble Mines rolling with the wind. Episode 4's
// `@ prize` and `@ vskin` live here too. The High Noon bonus
// (rules=onehit) is twenty duels in a row.

constexpr int kSixCylinder = 6;      // Six-Shooter shots before a reload
constexpr int kSixReload = 15;       // frames of the reload spin (kSixReloadTurbo under Turbo)
constexpr int kSixReloadTurbo = 7;
constexpr int kBridgeFall = 8;       // frames a drawbridge takes to tip over
constexpr int kDuelRange = 14;       // blocks: a Duelist calls you out from this far on his ground
constexpr int kDuelGap = 22;         // frames between the bell's two rings (44 under Turbo)
constexpr int kDuelDraw = 8;         // frames from the second ring to his shot
constexpr int kDuelRearm = 15;       // frames from his shot to the next first ring
constexpr int kBanditDown = 28;      // a Window Bandit's frames ducked
constexpr int kBanditTell = 12;      // his hat showing before he pops up
constexpr int kBanditUp = 20;        // frames up (he fires as he comes up)
constexpr int kHighNoonDuels = 20;
constexpr int kHighNoonWalk = 30;    // frames the next duelist takes to walk to his mark

// One block of a fuse: intact, burning (the spark is on it) or burnt away.
struct FuseCell
{
  int x = 0, y = 0;  // blocks
  int8_t state = 0;  // 0 intact, 1 burning, 2 burnt
  int8_t t = 0;      // burning: frames until it burns out and lights its neighbours
};

// `@ fuse ID path=x,y;... to=BARREL from=FUSE speed=1 hidden=1 bot=1 stand=x`.
struct Fuse
{
  std::string id, toId, fromId; // load time
  std::vector<FuseCell> cells;  // the path's blocks in order; the first is its cap
  int barrel = -1;              // the barrel at its far end
  int framesPerBlock = 2;       // speed 1: a cell a frame
  bool hidden = false;          // drawn only once lit (Secret 1)
  bool bot = false;             // the bot lights it at its cap on the way
  int standX = -1;              // blocks: where the bot lights it from
  bool lit() const
  {
    for (const auto& c : cells)
      if (c.state != 0)
        return true;
    return false;
  }
};

// `@ barrel ID x y radius=3 damage=8 hurt=3 flash=12`.
struct Barrel
{
  std::string id;
  int x = 0, y = 0;   // blocks
  int radius = 3;     // blocks
  int damage = 8;     // to enemies and `by=explosion` blocks
  int hurt = 3;       // hearts, to a runner in the radius
  int flash = 12;     // frames of the hiss before it blows
  int t = -1;         // frames of the hiss left (-1 not lit)
  bool blown = false;
};

// `@ drawbridge ID hinge=x,y len=6 falls=l barrel=ID`: a plank standing on
// its hinge (solid tiles x, y-len+1..y) that tips over when its barrel
// blows and lies on the hinge's ground row (y + 1).
struct Drawbridge
{
  std::string id, barrelId;
  int hx = 0, hy = 0, len = 6;
  int dir = -1;      // falls to the left (-1) or right (1)
  int barrel = -1;
  int t = -1;        // frames into the fall (-1 standing)
  bool down = false;
};

// `@ metal x y out=d` or `rect= out=`: a Six-Shooter bullet that hits it
// leaves the other side going `out`. `reveal=1`: shooting it brings out
// the dormant bonus entrance (the vault bell).
struct Metal
{
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0; // blocks
  int outDx = 0, outDy = 1;
  bool reveal = false;
  int ring = 0;      // frames of the ring (drawn swinging)
};

// `@ prize x y kind= score=`.
struct Prize
{
  int x = 0, y = 0; // blocks
  std::string kind;
  int score = 10000;
  bool taken = false;
};

// A wanted poster (`@ deco kind=42` in theme western_gulch): it spins on its
// nail when shot.
struct Poster
{
  int x = 0, y = 0; // blocks
  int spin = 0;     // frames of the spin left
};

// Scenery (theme western_gulch, `@ deco kind=K rect= text=`): facade (a
// false-front building, text its sign), steeple, pole, wire, winch, post (a
// hitching post), legs (the water tower's), stagecoach, headframe (over a
// mine shaft), sheriff. All background.
struct WestDeco
{
  std::string kind, text;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0; // blocks
};

// `@ spawner kind=tumble_mine x= y= every=75 max=3`.
struct MineSpawner
{
  int x = 0, y = 0; // cells, bottom-left
  int every = 75;
  int max = 3;
  int t = 0;
  std::vector<int> mines; // enemy indices it put out
};

// The High Noon bonus (rules=onehit): twenty duels on one street.
enum class DuelPhase
{
  WalkIn,  // the next duelist walks to his mark
  Ring1,   // waiting for the first ring
  Gap,     // between the rings: a shot now is a false start
  Draw,    // after the second ring: he draws in kDuelDraw frames
  Fired,   // he has fired; he fires again every 30 frames
  Won,     // he is down: the gem drops
  Done,    // all twenty: the exit is open
};

struct HighNoon
{
  bool on = false;
  int duel = 1;          // 1..20
  DuelPhase phase = DuelPhase::WalkIn;
  int t = 0;             // frames in this phase
  int gap = kDuelGap;    // this duel's frames between the rings
  int duelist = -1;      // enemy index
  int runnerX = 0, runnerY = 0; // cells: the runner's mark
  int markX = 0, markY = 0;     // cells: the duelist's mark
  int bellX = 0, bellY = 0;     // blocks
  uint32_t seed = 22;
  int falseStarts = 0;
  int message = 0;       // frames of "FALSE START" / "DRAW!" left
  std::string text;
};

struct WestState
{
  bool on = false;
  bool oneHit = false;            // rules=onehit: every hit kills
  std::vector<Fuse> fuses;
  std::vector<Barrel> barrels;
  std::vector<Drawbridge> bridges;
  std::vector<Metal> metals;
  std::vector<Prize> prizes;
  std::vector<Poster> posters;
  std::vector<WestDeco> decos;
  std::vector<MineSpawner> spawners;
  // The Six-Shooter's cylinder.
  int cylinder = kSixCylinder;
  int reload = 0;                 // frames of the reload spin left
  // The saloon piano (`@ piano x y`: four keys from x).
  int pianoX = -1, pianoY = 0;
  int pianoStep = 0, pianoT = 0;  // keys hit in order so far, frames since the last
  int pianoKey = -1, pianoFlash = 0;
  int tune = 0;                   // frames of the honky-tonk bar left
  bool fired = false;             // the runner fired this frame (World::fireShot)
  int firedX = 0, firedY = 0;     // cells: the muzzle
  // `@ vskin`: the Virus re-skinned (the MIRACLE TONIC on the saloon shelf).
  int tonic = -1;                 // item index
  bool tonicBob = true;
  std::string tonicSkin;
  bool tonicNear = false;         // the runner is within 2 blocks: the label shows
  // Duelists: their bell (the church's), rung for every duel.
  int bellX = -1, bellY = -1;     // blocks
  int bellRing = 0;               // frames the bell swings
  int quickDraw = 0;              // frames of "QUICK DRAW!" left
  std::vector<int> duelists;      // enemy indices
  // The fuses and barrels as they were at the last checkpoint.
  int checkpoints = -1;
  std::vector<std::vector<int8_t>> snapFuses;
  std::vector<int> snapBarrels;   // 1 blown
  HighNoon noon;
  bool anyBurning() const;        // a spark on a fuse or a barrel hissing
};

} // namespace gr
