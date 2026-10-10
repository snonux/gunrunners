#pragma once

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace gr
{

// Episode 3, STATION ZERO (world_station.cpp): the state its levels add to
// the world, kept in one place.

// Level 15: a hull panel in its yellow-black frame (`@ panel`). Only a
// Breach Charge blast opens it; its vent pulls for `frames` frames, then the
// shutter slams, and it can be breached again `reseal` frames later.
struct HullPanel
{
  std::string id, ventId;
  int bx = 0, by = 0, w = 2, h = 2; // blocks
  int vent = -1;
  int reseal = 90;
  bool bonus = false; // lets the runner through, into the bonus level
  int open = 0;       // frames since the breach (1..frames), 0 shut
  int cool = 0;       // frames until it can be breached again
  bool dented = false;
  bool sock = false;  // nothing loose was in reach: a sock floats out
};

// What a panel's vent pulls on (`@ vent`): everything loose in the rect,
// but the rooms in `exclude`.
struct VentZone
{
  std::string id;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0; // blocks, inclusive
  int pull = 1;                       // cells a frame
  int frames = 60;
  std::vector<std::array<int, 4>> exclude;
  bool inside(int bx, int by) const
  {
    if (bx < x0 || bx > x1 || by < y0 || by > y1)
      return false;
    for (const auto& r : exclude)
      if (bx >= r[0] && bx <= r[2] && by >= r[1] && by <= r[3])
        return false;
    return true;
  }
};

// A handrail at hand height (`@ handrail x0= x1= y=`, blocks): standing
// within a block of its span, the runner holds on and no vent moves them.
struct HandRail
{
  int x0 = 0, x1 = 0, y = 0;
};

// A loose 1 x 1 block crate: a solid block while it rests, a flying one
// while a vent pulls it or a Loader Mech has thrown it.
struct Crate
{
  int bx = 0, by = 0;
  int gems = 0;
  bool alive = true;
  bool loose = false;  // off the map, flying
  bool thrown = false; // by a Loader Mech: breaks where it lands
  bool held = false;   // lifted over a Loader Mech's head
  float fx = 0, fy = 0, vx = 0, vy = 0; // cells, top-left, while loose
};

// A Breach Charge out in the world: lobbed, then stuck to whatever it hit.
struct BreachCharge
{
  float fx = 0, fy = 0, vx = 0, vy = 0; // cells: its middle
  float prevFx = 0, prevFy = 0;
  bool stuck = false;
  int enemy = -1; // stuck to this enemy, at ox, oy from its corner
  int ox = 0, oy = 0;
  int age = 0;
};

// Something sucked out through an open panel, tumbling away into space.
struct Debris
{
  float x = 0, y = 0, vx = 0, vy = 0; // cells
  float angle = 0, spin = 0;
  int kind = 0; // 0 crate, 1 gem, 2 enemy, 3 item, 4 sock
  int life = 40;
  int variant = 0;
};

// A weld seam: a cell a Weld Drone crossed, hot for 45 frames (green: it
// infects instead of hurting).
struct Seam
{
  int x = 0, y = 0; // cells
  int life = 45;
  bool green = false;
};

// Asteroid Belt (Level 15's bonus, rules=recoil_only): a rock tumbling on
// a slow loop (`@ asteroid w= h= path= speed=`). Touching it bounces the
// runner; it never hurts.
struct Asteroid
{
  int w = 2, h = 2;                       // blocks
  std::vector<std::pair<int, int>> path;  // blocks, its top-left, a loop
  float speed = 0.25f;                    // cells a frame
  float fx = 0, fy = 0;                   // cells, top-left
  float angle = 0, spin = 0;              // degrees (drawn tumbling)
  int seg = 0;                            // the path point it is heading for
  int look = 0;
};

// The runner adrift: no ground, no gravity, no jump. Every shot pushes the
// other way.
struct DriftState
{
  float fx = 0, fy = 0, vx = 0, vy = 0; // cells: the runner's bottom-left
  int cool = 0;                         // frames until the next shot
  int bump = 0;                         // frames since a bounce (for the sound)
  int shots = 0;
};

struct StationState
{
  std::vector<HullPanel> panels;
  std::vector<VentZone> vents;
  std::vector<HandRail> rails;
  std::vector<Crate> crates;
  std::vector<BreachCharge> charges;
  std::vector<Debris> debris;
  std::vector<Seam> seams;
  int caught = -1;          // the panel whose frame holds the runner
  int caughtX = 0, caughtY = 0;
  bool holding = false;     // on a rail this frame (drawn as a hand on it)
  int detonate = 0;         // frames fire has been held with charges out
  // The cab set piece: standing on the cab's deck wakes its enemies.
  bool setOn = false, setFired = false;
  int setX0 = 0, setX1 = 0, setY = 0; // blocks
  std::vector<int> sleepers; // enemies it wakes
  int camera = -1;          // the candid camera outside the bay doors
  int cameraPanel = -1;     // shown while this panel is open
  // Asteroid Belt.
  bool recoil = false;
  DriftState drift;
  std::vector<Asteroid> asteroids;
  bool on() const { return !panels.empty() || !crates.empty(); }
};

} // namespace gr
