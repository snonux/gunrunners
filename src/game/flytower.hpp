#pragma once

#include "data/level.hpp"
#include "game/collision.hpp"

#include <string>
#include <vector>

namespace gr
{

// Level 40, The Fly Tower (world_flytower.cpp, world_flytower_draw.cpp):
// the Cue Sheet. Each zone of the tower has a looping cue list that starts
// when the runner's feet first enter it; its cues fire pyro, fly trusses in
// and out, blast confetti, switch followspots on and off and drop the
// towel, and the cue board at the top of the screen shows the next three.
// The Pyro Rig's flame stream and jetpack, the Followspot, Rigger and
// Sandbag Drop, and the Trigger Happy bonus (rules=jumpfire) are in the
// same files.

constexpr int kFtGlow = 15;         // frames a nozzle glows before it burns
constexpr int kFtBurn = 30;         // frames a pyro cue burns
constexpr int kFtBurnHurt = 8;      // frames between hearts in a flame
constexpr int kFtChase = 8;         // frames of chase lights before a truss moves
constexpr int kFtConfettiTell = 15; // frames a cannon rattles before its blast
constexpr int kFtConfettiPush = 20; // frames the blast pushes
constexpr int kFtSpotLock = 8;      // frames on the runner before the beam turns amber
constexpr int kFtSpotReachX = 48;   // cells: how far a followspot's beam reaches
constexpr int kFtSpotReachY = 30;
constexpr int kFtRiggerAim = 30;    // frames of laser sight before a Rigger fires
constexpr int kFtRiggerBeep = 15;   // the beep, this many frames before the shot
constexpr int kFtBagTell = 22;      // frames the bag's shadow grows before the cut
constexpr int kFtBagReach = 8;      // cells either side of a Sandbag Drop's bag line
constexpr int kFtRehang = 300;      // frames until a broken catwalk is hung again
constexpr int kFtStreamLen = 8;     // cells: the Pyro Rig's flame stream
constexpr int kFtStreamEvery = 4;   // frames between its hits
constexpr int kFtCutFlicker = 8;    // frames "Q99 (CUT)" flickers back on
constexpr int kFtTowelFall = 1;     // cells a frame the towel falls

enum class FtCueKind
{
  Pyro,
  Wave,
  Truss,
  Confetti,
  Spot,
  Towel,
  Cut,
};

// `@ cuelist ID zone=x0,y0,x1,y1 period=`: a zone's looping cue list.
struct FtCueList
{
  std::string id;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0; // blocks
  int period = 300;
  bool started = false;
  int t = 0; // frames into the loop
};

// `@ cue ID list= t= kind= target=A,B label= ...`.
struct FtCue
{
  std::string id, label;
  int list = -1;
  int t = 0;
  FtCueKind kind = FtCueKind::Pyro;
  std::vector<int> targets; // pyros, trusses, confetti cannons or spots
  int stop = 1;             // truss: the stop it flies to
  bool on = true;           // spot: the lamp on or off
  int dur = kFtBurn;        // pyro: frames it burns
  int warn = 0;             // frames ahead the board shows it (0: always)
  int waveOn = 20, waveOff = 40;
  std::vector<int> offsets; // wave: each target's offset
  bool once = false;        // once a life (the towel)
  bool fired = false;       // this life
  std::string listId, targetIds; // until linkFlyTower
};

// `@ pyro ID x y h= w= dir=up|l|r fuse=`: a flame column (or, sideways, a
// cannon) h blocks long from its nozzle block.
struct FtPyro
{
  std::string id;
  int x = 0, y = 0; // nozzle block
  int h = 8, w = 2;
  int dir = 0;      // 0 up, -1 left, 1 right
  int fuse = 0;     // a fuse box's hits left (the Q99 cannon), 0 none
  int fuseT = -1;   // frames since the fuse went (glow, then burn), -1 not lit
  bool spent = false;
  int faced = 0;    // frames left of the Q99 board flicker
  bool facing = false; // the runner faces it (on its floor)
  CellBox flame() const;   // cells the flame fills
  CellBox fuseBox() const; // the cannon's fuse box
};

// `@ platform ID x y w= mode=truss stops=a,b`: a truss that only moves on
// cue, a cell a frame, to the stop its cue names (chase lights run along it
// the 8 frames before).
struct FtTruss
{
  std::string id;
  int platform = -1;
  std::vector<int> stops; // cells (top surface rows)
  int goal = 0;           // stop index it is heading for
  bool waiting = false;   // the runner is in its way
};

// `@ confetti ID x y dir=l|r len=`: a cannon whose blast pushes the runner
// along its lane.
struct FtConfetti
{
  std::string id;
  int x = 0, y = 0; // block (the cannon stands on the floor below)
  int dir = -1;
  int len = 10;
  CellBox lane() const;
};

// A followspot, a Rigger or a Sandbag Drop.
struct FtActor
{
  std::string id;
  int enemy = -1;
  // Followspot.
  bool lamp = true;
  int beamX = 0;   // cells: where the beam is on the floor
  int lock = 0;    // frames the beam has been on the runner
  // Rigger.
  int aim = 0;     // frames into the aim, 0 none
  int aimX = 0, aimY = 0;
  bool live = false; // aiming at the lit runner (follows them)
  // Sandbag Drop.
  int tell = 0;    // frames of shadow
  int bagX = 0, bagY = 0; // cells: the bag's top-left when it is cut
};

// A sandbag falling.
struct FtBag
{
  int x = 0, y = 0; // cells, top-left (2 x 2 blocks)
  bool hit = false;
  CellBox box() const { return {x, y, 4, 4}; }
};

// Two catwalk blocks a sandbag broke, hung again later.
struct FtGap
{
  int bx = 0, by = 0; // blocks: the left one
  int t = kFtRehang;
  int mask = 3;       // which of the two were catwalk
};

// Trigger Happy's discs: `@ target x y` (a block wide, two tall).
struct FtTarget
{
  int x = 0, y = 0; // block, top
  bool hit = false;
  CellBox box() const { return {x * 2, y * 2, 2, 4}; }
};

// The bot's way through (`@ step x y act=go|ride|jet arg=`).
struct FtStep
{
  int x = 0, y = 0; // cells: feet
  std::string act, arg;
  int truss = -1; // act=ride
};

struct FlyTowerState
{
  bool on = false;
  bool trigger = false; // Trigger Happy (rules=jumpfire)
  std::vector<FtCueList> lists;
  std::vector<FtCue> cues;
  std::vector<FtPyro> pyros;
  std::vector<FtTruss> trusses;
  std::vector<FtConfetti> confetti;
  std::vector<FtActor> actors;
  std::vector<FtBag> bags;
  std::vector<FtGap> gaps;
  std::vector<FtTarget> targets;
  std::vector<FtStep> route;
  int routeAt = 0;
  int active = -1;    // the cue list whose zone the runner is in
  int burnCool = 0;   // frames until a flame can hurt again
  int stream = 0;     // frames the Pyro Rig's stream has been held
  int jetTick = 0;    // the jetpack's fuel tick
  bool lit = false;   // a followspot has the runner (amber)
  int litX = -1, litY = -1; // cells: where the runner was last lit
  // The towel (cue THE ANSWER): it falls from (towelX, towelY0) to land.
  int towelX = -1, towelY0 = 0, towelLand = 0;
  int towelY = -1;    // cells, -1 not dropped
  bool towelUsed = false;
  int hits = 0;       // Trigger Happy: discs shot
  bool groundPress = false; // Trigger Happy: this frame's press jumped (its shot comes after)
};

} // namespace gr
