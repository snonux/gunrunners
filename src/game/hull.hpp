#pragma once

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace gr
{

// Level 18, Hull Walk (world_hull.cpp): low gravity, the Recoil Cannon's
// kick, rivet plates and the mites that unbolt them, Space Barnacles, EVA
// Rams, the drifting camera, and the Planetoids bonus (rules=radial_gravity,
// world_orbit.cpp).

// A hull plate (`@ hullplate ID x0= x1= y= rivets=4`): one block row of
// solid tiles held by its rivets. Each Rivet Mite on a rivet takes it out in
// kRivetFrames; when the last is out the plate drifts off (a moving
// platform, up 1/4 cell and east 1/8 a frame) and is gone after 120 frames.
struct HullPlate
{
  std::string id;
  int x0 = 0, x1 = 0, y = 0;   // blocks
  std::vector<int> rivets;     // cells (x) of the rivets still in, on row y * 2
  std::vector<int> unbolt;     // per rivet: frames of unbolting so far (0..kRivetFrames)
  bool drifting = false;
  int platform = -1;           // the platform it became (index into the platforms)
  int life = 0;                // frames since it came loose
};

constexpr int kRivetFrames = 45;

// One Rivet Mite (2 x 1 cells) of a swarm. The swarm is one enemy
// (`rivet_mites`, 5 mites, one shared AI): it is counted once, and dies with
// its last mite.
enum class MiteState
{
  Crawl,   // walking along its plate toward a rivet
  Unbolt,  // on a rivet, working it loose
  Tell,    // the runner is close: it glows green (10 frames)
  Hop,     // leaping at the runner
  Ride,    // clinging to the runner (it infects a carrier=1 runner)
  Fall,    // knocked off, dropping back to the hull
  Dead,
};

struct RivetMite
{
  int swarm = -1;              // index into the enemies
  MiteState state = MiteState::Crawl;
  float x = 0.0f, y = 0.0f;    // cells: left, bottom row (y is the row it stands in)
  float vx = 0.0f, vy = 0.0f;  // hopping and falling
  int dir = 1;
  int timer = 0;               // frames in this state
  int cool = 0;                // frames until it can hop again
  int rivet = -1;              // the rivet it is unbolting (index into the plate's)
  int plate = -1;              // its plate
  int flash = 0;               // hit flash
};

// The candid camera tumbling round a loop (`@ drifter ID path=x,y;... speed=1/4`):
// it moves the level's candid camera (the `C` enemy nearest its path's
// start), which pops and scores when shot as usual.
struct HullDrifter
{
  std::string id;
  std::vector<std::pair<int, int>> path; // cells
  float fx = 0.0f, fy = 0.0f;            // cells
  int target = 1;
  float speed = 0.25f;
  int enemy = -1;                        // the camera (index into the enemies)
  float spin = 0.0f;                     // tumble angle (drawn), radians
};

// A piece of scenery drawn over (or behind) the tiles: `@ deco kind=K rect=`
// with K one of mast, dish, truss, wing, airlock, ufo, girder, sign42.
struct HullDeco
{
  std::string kind;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0; // blocks
};

// A flag the runner planted after standing still on the hull for 30 s.
struct HullFlag
{
  int x = 0, y = 0; // cells: the pole's foot (y is the row the runner stood in; the ground is y + 1)
  int age = 0;      // frames since it went up (it unfurls over the first 20)
};

struct HullState
{
  bool on = false;
  bool lowgrav = false;              // flags=lowgrav
  std::vector<HullPlate> plates;
  std::vector<RivetMite> mites;
  std::vector<HullDrifter> drifters;
  std::vector<HullDeco> decos;
  std::vector<HullFlag> flags;
  // The runner pushed sideways (the Recoil Cannon's kick, an EVA Ram's hit):
  // `shove` cells left to go in `shoveDir`, over `shoveFrames` frames.
  int shove = 0, shoveDir = 0, shoveFrames = 0;
  bool boostUsed = false;            // the downward shot's boost, once per jump
  int kick = 0;                      // frames of the cannon's muzzle blast (drawn)
  int kickDir = 0;                   // 1 right, -1 left, 2 down
  int fallTick = 0;                  // low gravity: falls 1 cell a frame, 2 every 5th
  int airDir = 0;                    // the direction of take-off (0 standing)
  int airTick = 0;                   // low gravity: steering drops every 4th step
  int still = 0;                     // frames the runner has stood still (the flag)
  int alien = 0;                     // frames of the UFO alien's thumbs-up left
  int alienX = 0, alienY = 0;        // cells: where it pops up (the hatch's bottom middle)
  bool ufoOpen = false;              // the UFO's hatch has been shot open
};

// Planetoids (rules=radial_gravity): each `@ planetoid x y r=` pulls the
// runner toward its centre within r + 4 blocks; walking follows the surface
// all the way round, a jump launches along the surface normal, and between
// fields the runner drifts in a straight line. `@ part x y` are the alien's
// ship parts (goal=collect:N counts them).
struct Planetoid
{
  float cx = 0.0f, cy = 0.0f; // cells, the centre
  float r = 0.0f;             // cells, the surface
  int look = 0;               // which of the rock looks (drawn)
};

struct OrbitPart
{
  float x = 0.0f, y = 0.0f; // cells, the centre
  int kind = 0;             // 0..4: fin, dish, engine, cockpit, landing leg (drawn)
  bool taken = false;
  int flash = 0;
};

struct OrbitState
{
  bool on = false;
  std::vector<Planetoid> planets;
  std::vector<OrbitPart> parts;
  int planet = -1;              // the planetoid stood on, -1 adrift
  float ang = 0.0f;             // radians: the runner's "up" (0 straight up, clockwise)
  float px = 0.0f, py = 0.0f;   // cells: the runner's feet
  float vx = 0.0f, vy = 0.0f;   // cells a frame, adrift
  int air = 0;                  // frames since take-off
  int lastPlanet = 0;           // where a runner lost in space comes back
  int lost = 0;                 // frames adrift outside every field
  int partsTaken = 0;
};

} // namespace gr
