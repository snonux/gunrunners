#pragma once

#include "game/collision.hpp"

#include <array>
#include <string>
#include <vector>

namespace gr
{

// Level 37, Wardrobe (world_wardrobe.cpp, world_wardrobe_draw.cpp): the
// costume department. Changing booths turn the runner into the runner on
// the booth's sign, each with their own hearts; heavy plates only go down
// under Rocco (or a Turbo runner) and open shutters or lower the cutting
// tables over the needle pit; spring racks launch each runner a different
// height under ceiling spikes. Mannequins creep up while you look away,
// Powder Puffers fog the room, Rack Runners roll their aisles, and the
// Decoy Launcher lobs a mannequin dressed as you that they all go for.
// The Totem bonus (rules=totem) stacks the three runners into one.

// Booths.
constexpr int kBoothCurtain = 12; // frames behind the curtain (can't be hit)
// Spring racks: launch heights in cells.
constexpr int kRackRocco = 4, kRackDash = 14, kRackNova = 22, kRackTurbo = 14;
constexpr int kTumble = 12;       // frames without air control after ceiling spikes
// Heavy things.
constexpr int kTableStep = 4;     // frames per block a cutting table comes down (8 blocks: 32)
constexpr int kPushRackEvery = 2; // frames per cell a pushed rack moves
// Decoy Launcher.
constexpr int kDecoyLife = 75;
constexpr int kDecoyHp = 6;
constexpr int kDecoyBonk = 6;     // cells: the landing bonk's radius
constexpr int kDecoyLure = 24;    // cells: enemies this close go for it
// Mannequin.
constexpr int kMannequinTwitch = 8;
// Powder Puffer.
constexpr int kPuffCloud = 90;    // frames a cloud lasts
constexpr int kPuffW = 10, kPuffH = 8; // cells: the cloud in front of it
constexpr int kPuffSight = 20;    // cells
// Rack Runner.
constexpr int kRackKnock = 6;     // cells of knockback
// Easter eggs.
constexpr int kSitFrames = 30;
constexpr int kToonAirRun = 8;   // frames the cartoon runs on air
// Totem.
constexpr int kTotemThrowUp = 13;   // cells up a thrown runner goes
constexpr int kTotemThrowFwd = 8;   // cells forward
constexpr int kTotemSwitch = 8;     // frames of down to switch control

// A changing booth (`@ booth x y runner=dash|rocco|nova|any|toon exit=1`),
// 2 x 3 blocks, (x, y) its bottom-left block.
struct Booth
{
  int x = 0, y = 0; // cells: bottom-left (the floor row the runner stands on)
  int runner = -1;  // character index; -1 any (the exit), -2 the toon booth
  bool exit = false;
  int curtain = 0;  // frames the curtain still swishes
  CellBox box() const { return boxAt(x, y, 4, 6); }
};

// A heavy plate (`@ switch ID x y kind=plate weight=2 latch=1 w=`): the
// floor block under (x, y). It fires a shutter or a table by its id.
struct HeavyPlate
{
  std::string id;
  int x = 0, y = 0, w = 2; // cells: the standing row and its width
  int weight = 2;
  bool latch = true;
  bool down = false;
  std::string target; // a shutter or a table
  std::string onTable; // rides this table: pressable once it is down
  int hint = 0;        // frames until "too light" shows again
};

// A shutter (`@ shutter ID rect= plate=`): solid until its plate goes down.
struct Shutter
{
  std::string id;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0; // blocks
  bool open = false;
  int t = 0; // frames of opening done
};

// A cutting table (`@ table ID rect=x0,row,x1,row to=ROW`): a row of
// one-way blocks that goes down to `to` once its plate is pressed (one-way,
// so a ladder beside it stays climbable).
struct CutTable
{
  std::string id;
  int x0 = 0, x1 = 0; // blocks
  int row = 0, toRow = 0;
  bool lower = false;
  int t = 0;          // frames since it started down
  bool down() const { return row == toRow; }
};

// A spring rack (`@ rack x y w=2`): the floor under the standing row.
struct SpringRack
{
  int x = 0, y = 0, w = 4; // cells; y is the standing feet row
  int bounce = 0;          // frames of the spring's wobble
};

// The archive's push rack (`@ pushrack ID x y w= h= door=x,y to=x,y`): Rocco
// pushes it; behind it a door leads down into the vault.
struct PushRack
{
  std::string id;
  int x = 0, y = 0, w = 2, h = 3; // blocks: top-left, size
  int homeX = 0;
  int push = 0;   // frames pushed toward the next block
  int slide = 0;  // frames of the last move's wobble
};

// A door you press up at (`@ vaultdoor x y to=x,y`), 1 block wide.
struct VaultDoor
{
  int x = 0, y = 0;     // cells: the door's left column, the feet row
  int toX = 0, toY = 0; // cells: where the runner comes out
};

// The Decoy Launcher's mannequin: flying, then standing.
struct Decoy
{
  float x = 0, y = 0;   // cells: bottom-left (feet)
  float vx = 0, vy = 0;
  bool landed = false;
  int life = kDecoyLife;
  int hp = kDecoyHp;
  int facing = 1;
  int look = 0;         // the runner it is dressed as
  CellBox box() const { return boxAt(int(x), int(y), 3, 5); }
};

struct Mannequin
{
  int enemy = -1;
  int twitch = 0;  // frames before it moves off
  bool frozen = true;
  int pose = 0;
};

struct Puffer
{
  int enemy = -1;
  int homeY = 0;
  int cloud = 0;     // frames the cloud still hangs
  int cloudDir = 1;
  int cloudX = 0, cloudY = 0; // cells: the cloud's top-left
  bool hit = false;           // this cloud has infected the runner
};

struct RackRunner
{
  int enemy = -1;
  int x0 = 0, x1 = 0; // cells: its aisle
  int roll = 0;       // cells left of this roll (sign: direction)
  int stopped = 0;    // a decoy holds it
};

// The bot's way through (`@ step x y act= arg=`): booth RUNNER, plate ID, go.
struct WardrobeStep
{
  int x = 0, y = 0; // cells: feet
  std::string act, arg;
};

// The bot's way through the Totem bonus: one per tower.
struct TotemTower
{
  int x = 0;      // cells: the tower's left edge
  int plateX = 0; // cells: the heavy plate beyond it
};

// Totem: the three runners, stacked or not.
struct TotemBody
{
  int runner = 0;      // character index (Rocco 1, Dash 0, Nova 2)
  int x = 0, y = 0;    // cells: feet, bottom-left
  int on = -1;         // the body it stands on
  bool active = false; // the player is this body (or the stack it tops)
};

struct WardrobeState
{
  bool on = false;
  std::vector<Booth> booths;
  std::vector<HeavyPlate> plates;
  std::vector<Shutter> shutters;
  std::vector<CutTable> tables;
  std::vector<SpringRack> racks;
  std::vector<PushRack> pushRacks;
  std::vector<VaultDoor> doors;
  std::vector<Decoy> decoys;
  std::vector<Mannequin> mannequins;
  std::vector<Puffer> puffers;
  std::vector<RackRunner> rackRunners;
  std::vector<CellBox> curtains; // costume curtains hiding a secret (drawn over it)
  std::vector<WardrobeStep> route;
  int routeAt = 0;
  bool upHeld = false;
  int pushDir = 0;   // the way the runner pushes this frame
  int onRack = -1;   // the spring rack the runner stands on
  bool tumbled = false; // fell off ceiling spikes: the next rack doesn't launch
  // Booths: the runner behind the curtain, the runner they become.
  int booth = -1, boothT = 0, boothTo = -1;
  // Every runner's hearts (-1: not used yet, full when they step in).
  std::array<int, 8> pools{-1, -1, -1, -1, -1, -1, -1, -1};
  int checkpointRunner = -1; // who touched the last checkpoint
  int checkpointsSeen = 0;
  int tumble = 0;            // frames without air control
  // Easter eggs and secrets.
  int suitX = -1, suitY = -1; // the Gorgazilla suit rack
  bool suitTouched = false, suit = false;
  int mirrorX = -1, mirrorY = -1; // the bulb mirror
  int sit = 0;
  bool mustache = false;
  bool toon = false;   // the OUT OF ORDER booth's cartoon
  int toonAir = 0;     // frames run on air
  int puffed = 0;      // frames the runner has been inside a cloud
  // Totem (rules=totem).
  bool totem = false;
  std::vector<TotemBody> bodies;
  std::vector<TotemTower> towers;
  int downHeld = 0;
  int throwT = 0;     // frames of a throw's forward push left
  int throwDir = 1;
};

} // namespace gr
