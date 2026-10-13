#pragma once

#include "data/level.hpp"
#include "game/collision.hpp"

#include <string>
#include <vector>

namespace gr
{

// Level 39, The Gallery (world_gallery.cpp, world_gallery_draw.cpp): closed
// rooms linked only by studio monitors. Up in front of a monitor (feet on
// its bottom row) plays a static wipe and puts the runner in front of the
// one it shows; panning monitors show a camera spot moving along a floor.
// Shots fly through monitors too. The Echo Mic repeats every shot a second
// later. The Rewind bonus (rules=rewind) is in the same files.

constexpr int kGlHop = 6;          // frames of the static wipe
constexpr int kGlEcho = 15;        // frames before an Echo Mic shot repeats
constexpr int kGlCount = 22;       // the Floor Manager's 3-2-1 (point 7, fingers 5 each)
constexpr int kGlCountTurbo = 44;  // in Turbo it runs at half speed
constexpr int kGlManagerSight = 24; // cells
constexpr int kGlCable = 12;       // cells of cable behind a Wrangler
constexpr int kGlTrip = 15;        // frames lying down after a trip
constexpr int kGlWhip = 10;        // frames the cable whips up when it turns
constexpr int kGlBoomPass = 10;    // frames of whoosh before a boom pass
constexpr int kGlBoomSweep = 8;    // frames the mic sweeps the arc
constexpr int kGlRewindKeep = 45;  // frames of history (3 s)
constexpr int kGlRewindPlay = 20;  // frames of VHS rewind

// `@ portal ID x y to=ID|x,y label= oneway=1 virus=1`: a 2 x 3 block
// monitor, (x, y) its top-left block.
struct Monitor
{
  std::string id, label, to;
  int x = 0, y = 0; // blocks
  int target = -1;  // a monitor
  int cam = -1;     // a camspot
  int toX = -1, toY = -1; // cells: a spot (feet middle, feet row)
  bool oneway = false, virus = false, dead = false;
  CellBox box() const { return {x * 2, y * 2, 4, 6}; }
  int feet() const { return (y + 2) * 2 + 1; }
};

// `@ camspot ID path=x,y;x,y speed=n/d`: where a panning monitor's camera
// looks, pingponging along a floor (one point: it stays).
struct CamSpot
{
  std::string id;
  int x0 = 0, x1 = 0, row = 0; // cells: the path's ends, feet row
  int n = 1, d = 2;            // cells per frame
};

// A shot waiting to repeat (the Echo Mic).
struct MicEcho
{
  int t = 0;
  int x = 0, y = 0, dx = 1, dy = 0, damage = 2;
};

// A Cable Wrangler's cable, a Boom Op's arc, a Floor Manager's count.
struct GalleryActor
{
  int enemy = -1;
  int cableX0 = 0, cableX1 = 0, cableY = 0; // cells
  bool dropped = false;                    // shot: the cable lies where it is
  int whip = 0;
  int count = 0; // the Floor Manager's frames into the count
  int pass = 0;  // the Boom Op's frames into a pass
};

// The Rewind bonus's crushers: `@ crusher x row w= to= period= tell=`.
struct GlCrusher
{
  int x0 = 0, w = 2, rest = 0, to = 0, period = 30, tell = 8;
  int t = 0, face = 0; // face: cells, the bottom row now
  CellBox box() const { return {x0 * 2, 0, w * 2, face + 1}; }
};

// The bot's way through (`@ step x y act=go|hop|pan arg=`).
struct GalleryStep
{
  int x = 0, y = 0; // cells: feet
  std::string act, arg;
  int lo = 0, hi = 0; // act=pan: enter while the camspot is in [lo, hi] (cells)
};

struct GalleryState
{
  bool on = false;
  std::vector<Monitor> monitors;
  std::vector<CamSpot> cams;
  std::vector<MicEcho> echoes;
  std::vector<GalleryActor> actors;
  std::vector<GlCrusher> crushers;
  std::vector<GalleryStep> route;
  int routeAt = 0;
  int t = 0;          // frames: the camspots' clock
  int hop = 0;        // frames of static wipe left
  int hopTo = -1;     // the monitor being entered
  int trip = 0;       // frames the runner lies tripped
  int arrived = -1;   // the monitor the runner last came out of
  // Rewind (rules=rewind).
  bool rewind = false;
  int rewinding = 0;  // frames of VHS rewind left
  int rewinds = 0;
};

} // namespace gr
