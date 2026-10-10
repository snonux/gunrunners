#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace gr
{

// Level 21, ZERO (world_zero.cpp, world_zero_draw.cpp): Live Rewiring (the
// level script moves the bulkheads between named states, previewed on the
// monitors), the Phase Rifle's shots through one wall, Lattice Turrets on
// their ceiling rails, Repair Swarms that rebuild what was broken, Echoes
// replaying the runner's own moves, and ZERO itself, the red eye in a ring
// of servers, with the reveal after it. The Wireframe bonus
// (rules=wireframe) turns every `#` into a passable wireframe and makes
// `@ hitbox rect=` the only solid ground.

constexpr int kShiftPreview = 45; // frames the monitors show the next state before it moves
constexpr int kShiftSlide = 30;   // frames a bulkhead slides (a shift's slide= can make it longer)
constexpr int kEchoDelay = 75;    // frames an Echo replays behind the runner
constexpr int kEchoTell = 8;      // frames ahead its flicker shows where it will fire
constexpr int kEchoHistory = 256; // frames of the runner's moves kept (a ring)
constexpr int kRebuildFrames = 90; // a wreck a Repair Swarm tends comes back this long after it broke
constexpr int kGlintFrames = 15;  // a fake wall or hidden pocket glints after a phase shot passes
constexpr int kZeroSegments = 12; // the arena floor's drop-away segments
constexpr int kOverloadCycle = 120;
constexpr int kRevealFrames = 180; // the wall falls, the lights come on, Lance walks on

// One step of the level script (`@ shift ID trigger=x:N|switch:ID open=A,B
// close=C,D slide=F`): when it triggers the monitors preview it for
// kShiftPreview frames, then its bulkheads slide over `slide` frames.
struct ZeroShift
{
  std::string id;
  int triggerX = -1;         // cells: the runner's left passing this
  std::string triggerSwitch; // or this kill switch shot
  std::vector<std::string> openIds, closeIds; // load time
  std::vector<int> open, close; // layer indices
  int slide = kShiftSlide;
  int t = -1;                // frames since it triggered (-1 waiting)
  bool done = false;
};

// A bulkhead (a script layer named BK_*) or one of the arena's floor
// segments (SEG*): `pos` is how far it is shut, 0 open .. 1 shut, drawn
// sliding; the layer turns solid once it is all the way shut.
struct ZeroDoor
{
  int layer = -1;
  float pos = 1.0f;
  float from = 1.0f, to = 1.0f; // this slide
  int slide = 0, t = 0;         // frames of the slide, frames into it
  bool fromCeiling = true;      // slides down out of the ceiling (else up out of the floor)
  bool segment = false;         // an arena floor segment
  int shake = 0;                // segments: frames of the shake before a drop
};

// A monitor hanging from the corridor's ceiling (`@ monitor x y`).
struct ZeroMonitor
{
  int x = 0, y = 0; // blocks
};

// `@ lasergrid rect= dmg= every=`: beams across the rect, on for the first
// half of every `every` frames; a runner in them takes `dmg`.
struct ZeroGrid
{
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0; // blocks
  int dmg = 1;
  int every = 6;
};

// A shootable kill switch (`@ switch ID x y kind=shootable`).
struct ZeroSwitch
{
  std::string id;
  int x = 0, y = 0; // blocks
  bool hit = false;
};

// An Echo pad (`@ echopad ID x y wake=SWITCH`): armed from the start, or
// once its switch is shot; the runner passing it starts an Echo, which
// steps out kEchoDelay frames later, where the runner stood.
struct EchoPad
{
  std::string id;
  int x = 0, y = 0;      // blocks
  std::string wake;      // load time: the switch that arms it ("" armed)
  bool armed = true;
  int passedAt = -1;     // frame the runner passed it (-1 not yet, or its Echo is out)
  int echo = -1;         // the Echo it put out (enemy index), -1 none
};

// One frame of the runner, as an Echo replays it.
struct EchoFrame
{
  int16_t x = 0, y = 0;   // cells, bottom-left
  int8_t facing = 1;
  uint8_t visual = 0;     // PlayerVisual
  int8_t shotDx = 0, shotDy = 0; // a shot fired this frame (0, 0: none)
  int8_t shotOx = 0, shotOy = 0; // its muzzle, from x, y
};

// Something a Repair Swarm can put back: a Lattice Turret (enemy index) or
// a `rebuild=1` breakable door.
struct ZeroWreck
{
  bool turret = true;
  int index = -1;
  int x = 0, y = 0; // cells: the wreck's middle
  int t = 0;        // frames since it broke
  int tended = 0;   // frames a swarm has been at it (draws the progress ring)
};

// A terminal (`@ terminal x y text= code=1`): up at it shows its text;
// code=1 also listens for up, up, down, down.
struct ZeroTerminal
{
  int x = 0, y = 0; // blocks
  std::string text;
  bool code = false;
  int shown = 0;    // frames its screen is lit
};

// Scenery (`@ deco kind=K rect=` or `x y`, theme station_servers): rack
// (a server rack front), 42 (RACK 42's label), sign (text=), fakewall (a
// rack face you can walk through: `@ fakewall rect=`).
struct ZeroDeco
{
  std::string kind, text;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0; // blocks
  int glint = 0; // fake walls and pockets: frames of the glint left
};

enum class ZeroPhase
{
  Idle,     // waiting for the runner to come into the arena
  Racks,    // 1: servers orbit the eye; shots only through the gaps (or phased through a rack)
  Retract,  // the racks pull back into the walls
  Echoes,   // 2: shuttered, three Echoes; then open for kOpen frames
  Overload, // 3: the floor drops away, the eye charges
  Dying,    // the eye goes dark
  Reveal,   // the wall falls: a studio
  Done,
};

// ZERO (`@ boss ZERO x y w=6 h=6 arena=x0,y0,x1,y1 door=x0,y0,x1,y1`).
struct ZeroBoss
{
  bool on = false;
  bool away = false;      // the runner respawned outside: the fight waits for them
  int x = 0, y = 0, w = 6, h = 6; // blocks: the eye
  float cx = 0.0f, cy = 0.0f;     // cells: its middle
  int ax0 = 0, ay0 = 0, ax1 = 0, ay1 = 0; // blocks: the arena (inside its walls)
  int doorX0 = -1, doorY0 = 0, doorX1 = 0, doorY1 = 0; // blocks: the door that shuts behind you
  int wallX0 = -1, wallY0 = 0, wallX1 = 0, wallY1 = 0; // blocks: the painted flat that falls
  int floorRow = 0;       // blocks: the segments' row
  ZeroPhase phase = ZeroPhase::Idle;
  int t = 0;              // frames in this phase
  int hp = 30;            // this phase's hearts left (30 a phase)
  int flash = 0;          // frames of the hit flash
  int glance = 0;         // frames of a spark where a shot glanced off
  // Phase 1: eight 2x4 racks orbit at radius 7 blocks with two gaps.
  float orbit = 0.0f;     // radians: slot 0's angle (0 east, growing clockwise on screen)
  float racks = 1.0f;     // 1 out, 0 pulled back into the walls
  int fan = 0;            // frames into the 40-frame fan cycle
  int iris = 0;           // frames of the iris contracting (the tell) left
  // Phase 2.
  bool shutter = true;
  int open = 0;           // frames of the open window left
  int wave = 0;           // waves of Echoes so far
  std::array<int, 3> echoes{-1, -1, -1}; // enemy indices
  // Phase 3.
  int cycle = 0;          // frames into the 120-frame cycle
  int cycles = 0;
  uint16_t safe = 0;      // the safe segments this cycle (bit k: SEGk+1)
  uint16_t prevSafe = 0;
  uint32_t seed = 42;
  int beam = 0;           // frames of the beam on the grille
  // The reveal.
  int reveal = -1;        // frames into it (-1 not yet)
  int lights = 0;         // banks of studio lights on (0-3)
  bool exitOpen = false;
  std::vector<int> turrets;   // ALT1, ALT2 (enemy indices)
  std::vector<std::string> turretIds; // load time
  bool racksBlock(float sx, float sy) const; // a rack is at this point (cells)
  bool eyeOpen() const;   // shots can hurt it now
  int total() const;      // hearts left over all three phases
};

struct ZeroState
{
  bool on = false;
  bool wireframe = false;          // rules=wireframe (the bonus)
  std::vector<std::pair<int, int>> wires; // the bonus: blocks that were `#` (drawn as wireframe)
  std::vector<std::array<int, 4>> hitboxes; // the bonus: `@ hitbox rect=` (blocks)
  std::vector<ZeroShift> shifts;
  std::vector<ZeroDoor> doors;
  std::vector<ZeroMonitor> monitors;
  std::vector<ZeroGrid> grids;
  std::vector<ZeroSwitch> switches;
  std::vector<EchoPad> pads;
  std::vector<ZeroWreck> wrecks;
  std::vector<ZeroTerminal> terminals;
  std::vector<ZeroDeco> decos;
  std::vector<int> rebuild;        // breakables (index) that come back; their full hp
  std::vector<int> rebuildHp;
  int state = 0;                   // shifts done (the state the corridor is in)
  int preview = -1;                // the shift the monitors show (-1 none)
  int previewT = 0;                // frames into its preview
  // The runner's last kEchoHistory frames.
  std::array<EchoFrame, kEchoHistory> history{};
  int historyLen = 0;              // frames recorded (up to kEchoHistory)
  int historyAt = 0;               // the next slot
  bool fired = false;              // fireShot ran this frame
  int8_t firedDx = 0, firedDy = 0, firedOx = 0, firedOy = 0;
  int codeStep = 0, codeT = 0;     // the terminal's up, up, down, down
  bool upHeld = false, downHeld = false;
  int joke = 0;                    // frames the joke shows
  bool inAir = false;              // last frame the runner was off the ground (a landing stomps a grating)
  ZeroBoss boss;
  std::vector<std::string> names;  // load time: each enemy's id, by index
  // The frame this many frames ago (clamped to the oldest kept).
  const EchoFrame& ago(int frames) const
  {
    const int back = frames < historyLen ? frames : (historyLen > 0 ? historyLen - 1 : 0);
    const int i = ((historyAt - 1 - back) % kEchoHistory + kEchoHistory) % kEchoHistory;
    return history[std::size_t(i)];
  }
};

} // namespace gr
