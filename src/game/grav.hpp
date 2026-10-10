#pragma once

#include <string>
#include <vector>

namespace gr
{

// Level 19, Gravity Lab (world_grav.cpp): chambers whose gravity a switch
// turns over, the Grav Grenade's vortex, Flip Walkers, Gravity Probes and
// Test Subjects, and the Gun Gravity bonus (rules=gun_gravity).

// Which way is down. The runner's own (Player::grav) decides its box: Down
// is the usual box standing on its feet (x, y the bottom-left cell); Up
// hangs from the feet at the top (x, y the top-left cell); Left stands on
// a wall to the left (x the feet's column, y the top row, 5 wide, 3 tall);
// Right on a wall to the right (x the feet's column, at the box's right).
enum class Grav
{
  Down,
  Up,
  Left,
  Right,
};

// A chamber (`@ gravzone ID rect= dir=down|up switch=SID`): everything in
// it falls toward `dir`. Its switch turns it over.
struct GravZone
{
  std::string id;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0; // blocks
  Grav dir = Grav::Down;
  int sw = -1;
  int flip = 0;     // frames since it last turned over (the wall arrows' animation)
  int lastFlip = -1000; // the clock of its last flip (two quick hits: the closet)
};

// A wall panel (`@ switch ID x y kind=panel`): press up on it or shoot it
// and its chamber turns over (it rests 10 frames after each).
struct GravSwitch
{
  std::string id;
  int bx = 0, by = 0;
  int zone = -1;
  int flash = 0;     // frames since it was last thrown (it rests 10)
  int lastHit = -1000; // the clock of the hit before
};

// An orange arrow painted on a wall (`@ deco kind=arrow x y`): it points
// the way its chamber's gravity does (drawn).
struct GravArrow
{
  int bx = 0, by = 0;
  int zone = -1;
};

// The Grav Grenade: lobbed toward the current down; it stops at the first
// thing it touches (or after 12 frames) and hangs there as a vortex for 45
// frames, pulling enemies within 6 blocks to its middle, then pops.
struct GravVortex
{
  float x = 0.0f, y = 0.0f;   // cells, the middle
  float vx = 0.0f, vy = 0.0f; // flying
  int flight = 0;             // frames flown (-1: hanging as a vortex)
  int life = 0;               // frames of vortex left
  Grav down = Grav::Down;     // the way it falls
};

// A gate that opens once the enemies it names are dead
// (`@ testgate ID x y h=3 opens=TS1,TS2`), or once its switch is hit twice
// within 15 frames (`hits=SID`: the closet's door).
struct TestGate
{
  std::string id;
  int bx = 0, by = 0, h = 3; // blocks: the top block and its height
  std::vector<int> enemies;
  int sw = -1;
  bool open = false;
  int opened = 0; // frames since it opened (drawn sliding up)
};

// A wall that is not there (`@ fakewall x y h=`): drawn as wall, walked through.
struct FakeWall
{
  int bx = 0, by = 0, h = 1;
};

// Scenery drawn over (or behind) the chambers (`@ deco kind=mug|desk|chain|window
// x y` or rect=): the STATION MANAGER mug on its desk (it never falls), the
// chains a ledge hangs from, the observation window.
struct GravDeco
{
  std::string kind;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0; // blocks
};

struct GravState
{
  bool on = false;
  bool gun = false;           // rules=gun_gravity: down is where the last shot went
  std::vector<GravZone> zones;
  std::vector<GravSwitch> switches;
  std::vector<GravArrow> arrows;
  std::vector<GravVortex> vortices;
  std::vector<TestGate> gates;
  std::vector<FakeWall> fakes;
  int dizzy = 0;              // frames of the runner's dizzy stars (two flips in 15 frames)
  int lastFlip = -1000;       // the clock of the last flip the runner went through
  std::vector<GravDeco> decos;
  // While loading: each zone's switch, each gate's switch and enemies, and
  // the enemies' ids (linkGrav resolves them, then clears these).
  std::vector<std::string> zoneSw, gateSw;
  std::vector<std::vector<std::string>> gateOpens;
  std::vector<std::pair<std::string, int>> names;
  int respawns = 0;           // counts respawns (a respawn inside a turned update leaves the runner as placed)
  bool turned = false;        // the runner turned and has not landed yet (the thud)
  bool jumped = false;        // the runner took off this frame (Test Subjects copy it)
  bool upHeld = false;        // up was held last frame (a switch takes the press)
  int lastShot = 0;           // Gun Gravity: frames since the last shot turned gravity
};

} // namespace gr
