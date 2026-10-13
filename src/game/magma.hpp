#pragma once

#include "game/collision.hpp"

#include <string>
#include <vector>

namespace gr
{

// Level 33, Magma Mainframe (world_magma.cpp, world_magma_draw.cpp): a
// vertical escape up a data centre inside a volcano. Rising lava
// (`@ fluid kind=rising`) climbs after the runner; touching a checkpoint
// vents coolant (`@ purge`) and stops it for a while. The Heat Sink sucks
// fireballs and enemy shots out of the air and throws them back as one big
// fireball, the only thing that hurts a Firewall. Lava Daemons lob the
// fireballs, Fan Bots blow across the cooling shaft, a corrupted rack blows
// Virus dust, a magma gutter burns, and the RATINGS server opens the
// Overclock bonus (rules=overclock: everything runs at three times speed).

// Rising lava.
constexpr int kPurgeFrames = 150; // a purge stops the lava this long
constexpr int kSirenFrames = 22;  // the siren before it rises again
constexpr int kRespawnDepth = 24; // cells: on a respawn the lava sits this far below the checkpoint
constexpr int kPopClear = 8;      // cells: a pop lands this far above the surface at least
constexpr int kLavaHearts = 2;
constexpr int kRebootFrames = 90; // the DO NOT REBOOT rack's reboot screen (the lava waits)
constexpr int kGutterEvery = 15;  // frames per heart in the magma gutter
// Heat Sink.
constexpr int kSinkReach = 8;    // cells: the cone in front
constexpr int kSinkMax = 6;      // stored shots
constexpr int kSinkRadius = 2;   // cells: the fireball's blast
// Firewall.
constexpr int kWallFlicker = 8;  // frames of flicker before a dip
constexpr int kWallDip = 30;     // frames the flames are low at a port
constexpr int kWallKnock = 4;    // cells of knockback
constexpr int kWallTall = 6;     // cells: the flames' height, and dipped
constexpr int kWallLow = 2;
// Lava Daemon.
constexpr int kDaemonLeap = 60;  // frames per leap
constexpr int kDaemonArc = 16;   // frames in the air
constexpr int kDaemonSight = 30; // cells
// Fan Bot (its wind is a `@ wind` zone made at link time).
constexpr int kFanPeriod = 105;
constexpr int kFanOn = 60;
constexpr int kFanSpin = 15; // frames of spin-up before a gust
constexpr int kFanBand = 6;  // cells: the gust's height
// The corrupted rack.
constexpr int kRackSpin = 15;

// `@ purge id x y`: a coolant station at a checkpoint.
struct Purge
{
  std::string id;
  int x = 0, y = 0; // cells: bottom-left of its checkpoint
  bool fired = false;
  int steam = 0; // frames of the steam burst (drawn)
  CellBox box() const { return boxAt(x - 2, y, 6, 6); }
};

// A Firewall's sweep along its floor.
struct Firewall
{
  int enemy = -1;
  int x0 = 0, x1 = 0;     // cells: the sweep's ends (its left edge)
  std::vector<int> ports; // cells: its left edge when it stands over a port
  int startX = 0, startDir = 1;
  int pause = 0;     // frames left at a port (flicker, then dip)
  int lastPort = -1; // the port it last stopped at
  bool harmless = false; // Overclock: contact only pushes
};

// A Lava Daemon's perches and its lob.
struct Daemon
{
  int enemy = -1;
  int ax = 0, bx = 0, floorY = 0; // cells: perches and its feet's row
  int leapT = 0;   // frames into the current leap (0: perched)
  int fromX = 0, toX = 0;
  bool marshmallow = false; // ignores the runner until shot
  bool roused = true;
};

// A Fan Bot: its wind zone.
struct Fan
{
  int enemy = -1;
  int zone = -1;
};

// `@ virusrack x y hp= on= off= rect=`: Virus dust blowing over `rect`.
struct VirusRack
{
  int x = 0, y = 0; // cells: bottom-left
  CellBox dust{0, 0, 0, 0};
  int hp = 4;
  int on = 60, off = 45;
  int t = 0;
  int dir = -1;
  bool blowing() const { return hp > 0 && t < on; }
  bool spinning() const { return hp > 0 && t >= on + off - kRackSpin; }
  CellBox box() const { return boxAt(x, y, 4, 6); }
};

struct MagmaState
{
  bool on = false;
  // Rising lava.
  bool rising = false;
  int x0 = 0, x1 = 0, floorY = 0; // cells: its columns, and its bottom row
  int startY = 0, surface = 0;    // cells: the top wet row, at the start and now
  int capY = 0;                   // cells: it never rises above this row
  int rateNum = 1, rateDen = 20;  // cells per frame
  int delay = 150;                // frames before it starts
  int pause = 0;                  // frames left of a purge (or the reboot)
  int sub = 0;                    // frames toward the next cell
  int pops = 0;
  // Drawing only: the last pop's 20-frame arc out of the lava.
  int popFrame = -1000;           // mStats.frames when it happened
  int popFromX = 0, popFromY = 0; // cells: where the runner touched the lava
  int popToX = 0, popToY = 0;     // cells: where the pop set the runner down
  std::vector<Purge> purges;
  CellBox gutter{0, 0, 0, 0};
  int gutterT = 0;
  // Heat Sink.
  bool sucking = false;
  int sinkT = 0; // frames held
  // Enemies and props.
  std::vector<Firewall> walls;
  std::vector<Daemon> daemons;
  std::vector<Fan> fans;
  std::vector<VirusRack> racks;
  int rebootX = -1, rebootY = -1; // cells: the DO NOT REBOOT rack's bottom-left
  int reboot = 0;                 // frames left of its reboot screen
  bool rebooted = false;          // once per life
  int serverX = -1, serverY = -1; // cells: the RATINGS server's bottom-left
  int serverScore = 10000;
  bool serverShot = false;
  // Overclock.
  bool overclock = false;
  bool blast = false; // a Heat Sink fireball going off: Firewalls can be hurt
  CellBox serverBox() const { return boxAt(serverX, serverY, 4, 6); }
  CellBox rebootBox() const { return boxAt(rebootX, rebootY, 4, 6); }
};

} // namespace gr
