#pragma once

#include "game/collision.hpp"
#include "game/grav.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace gr
{

// Level 42, The Live Finale (world_finale.cpp, world_finale_boss.cpp,
// world_finale_draw.cpp): the Season Recap. Six set wagons along the
// turntable bring back earlier levels' systems inside their slots (beat
// signs, swing vines, a gravity chamber with plates, a fuse to a barrel,
// cartoon air-running, a pair of studio monitors); each runs its trick for
// 450 frames once it has rolled in, then reruns while the runner is still
// in the slot. Censor Bars, Ad Breaks and Stunt Doubles; the Encore (every
// prototype the Arsenal holds on one shared pool); Lance Marquee, the
// Encore Engine and the race to the master switch; the Blooper Reel bonus
// (rules=blooper).

constexpr int kFnRoll = 60;        // frames the turntable turns a wagon in
constexpr int kFnTrick = 450;      // frames a wagon's trick runs
constexpr int kFnRerun = 60;       // frames of a RERUN in the rest state
constexpr int kFnFlip = 8;         // frames from a plate to the gravity flip
constexpr int kFnWipe = 6;         // frames of a monitor's static wipe
constexpr int kFnUhOh = 8;         // frames of the "uh-oh" after an air-run (Nova 16)
constexpr int kFnPool = 30;        // the Encore's shared pool
constexpr int kFnBeat = 15;        // frames per ticker beat: the pool refills a unit
constexpr int kFnWheelSpin = 8;    // frames of the Encore's wheel animation
constexpr int kFnBarW = 16, kFnBarH = 6; // cells: a Censor Bar's black bar
constexpr int kFnBarUnroll = 12;   // frames the bar unrolls
constexpr int kFnJingle = 15;      // frames of an Ad Break's jingle
constexpr int kFnAdSamples = 4;    // sample bots alive per Ad Break at most
constexpr int kFnAdRoll = 16;      // frames an Ad Break rolls in from the wings
constexpr int kFnDoubleTell = 10;  // a Stunt Double's muzzle-flash pose

// The boss (world_finale_boss.cpp).
constexpr int kFnLanceHp = 40, kFnEngineHp = 50, kFnCoverHp = 10;
constexpr int kFnPodiumW = 8, kFnPodiumH = 4;   // cells
constexpr int kFnWheelFrames = 22, kFnWheelHold = 8; // the prize wheel (the pointer holds the last 8)
constexpr int kFnAttackGap = 30;
constexpr int kFnGoldTell = 15;
constexpr int kFnTrapTell = 15, kFnTrapOpen = 45, kFnTrapSkip = 300;
constexpr int kFnLaughTell = 12, kFnWaveSpeed = 2;
constexpr int kFnLampTell = 18;
constexpr int kFnCueCards = 60;
constexpr int kFnSinkRows = 3;      // blocks the podium sinks for the cue cards
constexpr int kFnEngineW = 16, kFnEngineH = 16; // cells
constexpr int kFnCoreOpen = 30, kFnStagger = 60;
constexpr int kFnBreathReach = 12;  // cells: the flame cone
constexpr int kFnRingSpeed = 2;     // cells a frame: the Core Pulse
constexpr int kFnPullFrames = 45;
constexpr int kFnCollapse = 45;     // frames the Engine takes to collapse
constexpr int kFnTrapTurn = 45;     // Off the Air: a trapdoor opens every 45 frames
constexpr int kFnLightEvery = 60;
constexpr int kFnCommercial = 45;
constexpr int kFnRunEvery = 4;      // frames per cell Lance runs to the desk
constexpr int kFnIntro = 45;

// The Blooper Reel.
constexpr int kFnReelEvery = 150, kFnReelCard = 15, kFnReelWait = 8;
constexpr int kFnReelRules = 6;

enum class FnWagonKind
{
  Neon,
  Temple,
  Station,
  Western,
  Toon,
  Studio,
};

enum class FnWagonState
{
  Idle,    // waiting for the runner (the rest state)
  Rolling, // the turntable turns it in
  Running, // its trick runs
  Rerun,   // the trick paused in its rest state before it restarts
  Done,    // the runner left the slot on the right
};

// `@ wagon ID kind= slot=x0,x1 trigger=x` (blocks).
struct FnWagon
{
  std::string id;
  FnWagonKind kind = FnWagonKind::Neon;
  int x0 = 0, x1 = 0; // blocks
  int trigger = 0;    // block
  FnWagonState state = FnWagonState::Idle;
  int t = 0;          // frames into the state
  int reruns = 0;
};

// `@ plate ID x y zone=GZ dir=up|down`: standing on it turns the zone's
// gravity `dir` 8 frames later.
struct FnPlate
{
  std::string id, zoneId;
  int bx = 0, by = 0;
  int zone = -1;
  Grav dir = Grav::Up;
  int flip = 0;        // frames until the flip, 0 none
  bool pressed = false;
};

// `@ portal ID x y to=ID label=`: a monitor 2 x 3 blocks, (x, y) its top-left.
struct FnPortal
{
  std::string id, toId, label;
  int bx = 0, by = 0;
  int to = -1;
};

// The bot's way through (`@ step x y act=plate|ceiling|hop arg=`).
struct FnStep
{
  int x = 0, y = 0; // cells: feet
  std::string act, arg;
};

enum class FnActorKind
{
  Censor,
  AdBreak,
  Sample,
  Double,
};

// A Censor Bar, an Ad Break, a sample bot or a Stunt Double.
struct FnActor
{
  FnActorKind kind = FnActorKind::Censor;
  int enemy = -1;
  std::vector<std::pair<int, int>> path; // cells
  int target = 1;
  int bar = -1;       // Censor: frames into its bar (-1 none)
  int barX = 0, barY = 0; // cells: the bar's top-left
  int wagon = -1;     // Ad Break: rolls in with this wagon
  std::string wagonId;
  int spawn = 0;      // Ad Break: sample bots to bring out this frame
  int roll = -1;      // Ad Break: frames into its roll-in, -1 waiting
  int homeX = 0;      // Ad Break: where it stands
  int owner = -1;     // sample bot: its Ad Break (actor index)
  int runner = 0;     // Double: 0 Dash, 1 Rocco, 2 Nova
  int vy = 0;         // Double: its jump (cells a frame, up negative)
  int jumpT = -1;     // Double: frames into its jump arc, -1 on the ground
};

enum class FnPhase
{
  Waiting,  // the ON AIR door is open
  Intro,
  Lance,    // phase 1
  Engine,   // phase 2
  Collapse,
  OffAir,   // phase 3
  Switched, // the master switch is down
};

enum class FnAttack
{
  None,
  Gold,
  Trapdoor,
  Laugh,
  Light,
};

enum class FnEngineMove
{
  Barrage,
  Fist,
  Pulse,
  Arc,
  Magnet,
  Breath,
};

// Lance Marquee and the Encore Engine (`@ lance x y arena= path= engine=
// desk= restart= drops= turbo=`).
struct FnBoss
{
  bool on = false;
  FnPhase phase = FnPhase::Waiting;
  int t = 0;                  // frames into the phase
  int arenaX0 = 0, arenaY0 = 0, arenaX1 = 0, arenaY1 = 0; // cells
  int floorY = 0;             // cells: the arena floor's feet row
  // Lance: the podium's top-left (cells), on its path.
  int x = 0, y = 0, prevX = 0, prevY = 0;
  int pathX0 = 0, pathX1 = 0, pathY = 0;
  int dir = 1;
  int lanceHp = kFnLanceHp, engineHp = kFnEngineHp, coverHp = kFnCoverHp;
  int wheel = -1;             // frames into the prize wheel, -1 not spinning
  FnAttack attack = FnAttack::None;
  FnAttack pick = FnAttack::None; // the wheel's pointer
  int attackT = 0;
  int gap = 0;                // frames until the next spin
  int attacks = 0;            // since the last cue cards
  int cue = 0;                // frames of cue cards left
  int trapSkip = 0;           // frames the wheel skips Trapdoor
  int bag = 0;                // attacks the wheel used this round (bits)
  std::array<int, 3> gold{};  // cells: the columns' left edges
  int trap = -1;              // the trapdoor of the Trapdoor attack
  int waveL = -1, waveR = -1; // cells: the laugh wave's ends, -1 none
  int waveX = 0;
  int lampX = -1;             // cells: the studio lamp's shadow, -1 none
  int hitFlash = 0;
  // The Encore Engine (cells: left, feet row).
  int ex = 0, ey = 0;
  FnEngineMove move = FnEngineMove::Barrage;
  int moveT = -1;             // frames into the move (tell first), -1 idle
  int core = 0;               // frames the core stays open
  int stagger = 0;            // frames staggered (core open, damage x 2)
  bool countered = false;     // the counter landed in this tell or attack
  int coinL = -1, coinR = -1; // cells: Gold Fist's coins rolling out
  int ring = -1;              // cells: the Core Pulse's reach
  std::array<int, 3> arcs{};  // cells: Arc Storm's strike columns
  int shots = 0;              // Beat Barrage: shots fired
  int breath = 0;             // frames of the flame cone
  // Off the Air.
  int deskX = 0;              // cells: the broadcast desk
  int runX = 0, runSub = 0;   // cells: Lance on the back wall
  int runStartX = 0;
  int trapT = 0, trapNext = 0; // the trapdoors' turns
  int openTrap = -1;          // the trapdoor open now (-1 none)
  int lightT = 0;
  int commercial = 0;         // frames of the CUT TO COMMERCIAL card
  int restartX = 0, restartY = 0; // cells
  std::vector<std::pair<int, int>> drops; // cells: h drops
  int turboX = -1, turboY = -1;
  int lampY = 0;
  int signDead = 0;           // frames since the ON AIR sign died
};

// The Blooper Reel's rule sets.
enum FnReelRule : int
{
  kFnAirJump,
  kFnSonar,
  kFnLightTrail,
  kFnStopMotion,
  kFnHeatstroke,
  kFnTriggerHappy,
};

struct FinaleState
{
  bool on = false;
  bool reel = false;            // rules=blooper
  std::vector<FnWagon> wagons;
  std::vector<FnPlate> plates;
  std::vector<FnPortal> portals;
  std::vector<FnStep> route;
  int routeAt = 0;
  std::vector<FnActor> actors;
  int wipe = 0;                 // frames of a monitor's static wipe left
  int wipeTo = -1;              // the monitor it comes out of
  int air = 0;                  // the air-run: way held (0 none)
  int airT = 0;
  int uhOh = 0;
  int refillX = -1, refillY = -1; // cells: the backstage refill
  bool refillUsed = false;
  int pedX = -1, pedY = -1;     // cells: the pedestal under the first W
  bool upHeld = false;          // up was held last frame (a monitor takes the press)
  int vineAmp = 50;             // the temple's vines' swing when the trick runs
  // Layers the finale drives (indices into the world's, -1 none).
  std::array<int, 3> signs{-1, -1, -1};
  std::array<int, 3> traps{-1, -1, -1};
  int door = -1, cover = -1, deckStep = -1, deckMid = -1, deckHigh = -1;
  // The Encore: the wheel of logged prototypes (-1 the blaster).
  std::vector<int> wheel{-1};
  std::vector<std::string> arsenal; // prototype keys the profile holds
  int sel = 0;
  int spin = 0;                 // frames of the wheel animation
  int beatT = 0;
  int holdT = 0;                // hold weapons: frames held
  bool upFire = false;          // up + fire held (one flick a press)
  FnBoss boss;
  // The Blooper Reel.
  int reelT = 0;
  int rule = -1;
  int card = 0;
  int waitT = 0;
  unsigned used = 0;            // rule bits used this round
  std::uint32_t rng = 42u;
  bool bonusDone = false;        // the runner went through a trapdoor into the Blooper Reel
};

class World;
const char* finaleWagonName(FnWagonKind k);
const char* finaleReelName(int rule);
int finaleCounter(FnEngineMove m);     // the prototype that counters the Engine's move
CellBox finalePodium(const FnBoss& b); // Lance's hover podium
CellBox finaleEngine(const FnBoss& b); // the Encore Engine's bulk
CellBox finaleCore(const FnBoss& b);   // its core
CellBox finaleCoverBox(const World& w); // the master switch's glass cover

} // namespace gr
