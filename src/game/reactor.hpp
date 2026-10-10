#pragma once

#include <string>
#include <utility>
#include <vector>

namespace gr
{

// Level 20, Reactor Core (world_reactor.cpp): the Core Pulse and the lead
// booths that keep it off, the Deflector Bracer, Conduit Sparks riding their
// wires, Shield Drones, Isotope Imps, the coolant drip, the valves that
// stop the pulses and open the lift cage, and the Stop Motion bonus
// (rules=stop_motion: the world only moves while the runner does).

constexpr int kPulseHum = 45;      // frames of rising hum before a ring sets off
constexpr int kPulseFlash = 15;    // frames the ring's flash lights the level
constexpr int kPulseWindDown = 30; // frames the hum takes to die once the valves are shut
constexpr int kBracerTap = 4;      // fire held fewer frames than this: a pulse shot on release
constexpr int kBracerRaise = 45;   // frames a raised shield stays up
constexpr int kBracerSoak = 100;   // frames until the shield can soak another pulse
constexpr int kValveHold = 30;     // frames of holding up to shut a valve

// The core (`@ pulse x y period= damage= speed= gaps= phase= reach=`):
// every `period` frames a ring sets off from (x, y) and grows `speed` blocks
// a frame (8; it dies `reach` blocks out, by default past the map); it
// costs `damage` hearts to a runner it passes outside the booths. Gaps are
// open arcs of the ring, in degrees (0 east, 90 down), as from-to pairs.
struct CoreRing
{
  float r = 0.0f;   // cells
  int side = 0;     // which side of it the runner was on last frame (-1 in, 1 out)
  bool soaked = false;
};

struct CorePulse
{
  float x = 0.0f, y = 0.0f; // cells: the centre
  int period = 150;
  int damage = 2;
  float speed = 16.0f;      // cells a frame (8 blocks)
  int clock = 0;            // frames into the period; a ring sets off at `period`
  int phase = 0;            // the first ring comes `period - phase` frames in
  float reach = 0.0f;       // cells: where rings die (past the map's furthest corner)
  std::vector<std::pair<int, int>> gaps;
  std::vector<CoreRing> rings;
};

// A lead booth (`@ shield rect=`): a runner inside is safe from the pulse.
struct LeadBooth
{
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0; // blocks
};

// A wire a Conduit Spark rides (`@ wire ID path=x,y;...`, blocks): a loop
// when the path ends where it starts, else back and forth.
struct CoreWire
{
  std::string id;
  std::vector<std::pair<int, int>> path; // cells: the spark's bottom-left at each point
  bool loop = false;
  float len = 0.0f;                      // cells
};

// A spark's place on its wire (Enemy::attach is the wire's index).
struct SparkRide
{
  int enemy = -1;
  std::string wireId; // load time
  int wire = -1;
  float s = 0.0f; // cells along the wire
  int dir = 1;    // back-and-forth wires: which way it is going
};

// The leaking coolant pipe (`@ drip x y period=`): a drop swells for 8
// frames, falls a cell a frame and infects a runner it lands on.
struct CoolantDrip
{
  int x = 0, y = 0; // cells: where the drop hangs (the pipe's underside)
  int period = 20;
  int t = 0;        // frames into the period (it swells over the first 8)
  std::vector<float> drops; // cells: the falling drops' bottom rows
};

// A valve (`@ valve ID x y`): hold up at it for kValveHold frames to shut it.
struct CoreValve
{
  std::string id;
  int x = 0, y = 0; // blocks
  int held = 0;     // frames held so far
  bool shut = false;
};

// A Shield Drone and the group it hovers over (`guard=II1,II2`); its
// bubble keeps every other enemy within kBubble cells from harm.
struct DroneGroup
{
  int enemy = -1;
  std::vector<std::string> names; // load time
  std::vector<int> group;         // enemy indices
  int shimmer = 0;                // frames of the bubble's shimmer (a blocked hit)
};

constexpr int kBubble = 8; // cells (4 blocks)

// The thrown-back pulse: a 6-block arc running out from the shield.
struct BracerArc
{
  float x = 0.0f, y = 0.0f; // cells: its middle
  int dir = 1;
  int life = 0;             // frames out
  std::vector<int> hit;     // enemy ids it has passed
};

// Scenery (`@ deco kind=K rect=` or `x y`, theme station_reactor): core
// (the glowing column on the back wall), dosimeter (reads 42), crane,
// window (thick lead glass), sign (text=).
struct ReactorDeco
{
  std::string kind, text;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0; // blocks
};

struct ReactorState
{
  bool on = false;
  bool stopMotion = false;       // rules=stop_motion
  bool moving = true;            // stop motion: this frame the world moves
  std::vector<CorePulse> pulses;
  std::vector<LeadBooth> booths;
  std::vector<CoreWire> wires;
  std::vector<SparkRide> sparks;
  std::vector<CoolantDrip> drips;
  std::vector<CoreValve> valves;
  std::vector<BracerArc> arcs;
  std::vector<ReactorDeco> decos;
  // The lift cage (`@ liftcage rect=`): its bars (the rect's east column)
  // are solid until every valve is shut.
  int cageX0 = -1, cageY0 = 0, cageX1 = 0, cageY1 = 0;
  bool cageOpen = false;
  int stopped = -1;      // frames since the last valve shut (-1: still pulsing)
  int flash = 0;         // frames of the ring's flash left
  // The crane catwalk's unlit ladder (`@ unlit rect=`), seen only in a flash.
  int unlitX0 = -1, unlitY0 = 0, unlitX1 = 0, unlitY1 = 0;
  // The DO NOT PRESS console (`@ console x y`); frames of the backwards
  // fanfare left.
  int consoleX = -1, consoleY = 0;
  int fanfare = 0;
  bool upHeld = false;
  // The Deflector Bracer.
  int raise = 0;          // frames the shield has been up (0: down)
  bool spent = false;     // this raise ran out: release fire to raise again
  int soakCool = 0;       // frames until it can soak a pulse again
  int reflect = 0;        // frames of the flash where a shot bounced off (drawn)
  int soakFlash = 0;      // frames of the flash where it soaked a pulse (drawn)
  std::vector<DroneGroup> drones;
  std::vector<std::string> names; // load time: each enemy's id, by index
};

} // namespace gr
