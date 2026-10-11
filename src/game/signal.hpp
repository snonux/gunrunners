#pragma once

#include "game/collision.hpp"

#include <string>
#include <vector>

namespace gr
{

// Level 29, Dead Air (world_signal.cpp, world_signal_draw.cpp): the level
// has one signal colour, red, green or blue. Colour bars of that colour are
// solid, the others ghosts. RGB dials change it (shot, or on a timer in
// the Test Card's arena); a change flashes the incoming bars first and then
// waits until the runner is off the outgoing colour. The Tri-Tone Blaster
// fires tinted shots (Up + fire turns the tint); each tint fired lights a
// pip, and with all three lit black bars are solid. Also the Wraparound
// bonus (rules=wrap_xy) and Episode 5's shared treasure.

constexpr int kSignalRed = 0, kSignalGreen = 1, kSignalBlue = 2, kSignalBlack = 3;
constexpr int kSignalFlash = 8;  // frames the incoming bars flash before a change may land
constexpr int kDialLock = 10;    // frames a dial ignores shots after a hit
constexpr int kPipFrames = 90;   // frames a tint pip stays lit
constexpr int kTintCool = 8;     // frames between tint turns
constexpr int kSwarmHiss = 8;    // the cloud brightens before it latches
constexpr int kSwarmBite = 15;   // frames per heart while a plain swarm is latched
constexpr int kSnowFrames = 120; // a quarter of the screen in snow after a carrier latches
constexpr int kCardTurn = 60;    // frames between the Test Card's quarter turns
constexpr int kCardTell = 15;
constexpr int kCardCool = 45;
constexpr int kCardBeam = 6;     // frames a beam burns
constexpr int kCardReach = 40;   // cells: a beam's length
constexpr int kImageCycle = 90;  // frames per halo tint
constexpr int kImageOpen = 30;   // frames after a shot with no halo
constexpr int kImageTell = 12;
constexpr int kTuneShake = 30;

// `@ bar color=red|green|blue|black rect=`: a script layer.
struct ColorBar
{
  int layer = -1;
  int color = kSignalRed;
};

// `@ rgbswitch ID x y mode=shoot|timed period= warn= phase= rect=`.
struct RgbDial
{
  std::string id;
  int x = 0, y = 0; // blocks
  bool timed = false;
  int period = 150, warn = 22, phase = 0;
  CellBox rect{0, 0, 0, 0};
  int lock = 0;  // frames it ignores shots
  int t = 0;     // timed: frames inside its rect
  int flash = 0; // frames it shows a hit
};

// The Test Card's beam: from a quadrant's centre toward where the runner
// was when the tell began.
struct CardBeam
{
  bool on = false;
  int quadrant = 0; // its colour: 0 red, 1 green, 2 blue, 3 yellow
  float x0 = 0.0f, y0 = 0.0f, x1 = 0.0f, y1 = 0.0f; // cells
  int t = 0;        // frames of tell, then of burn
  bool burning = false;
};

// `@ treasure x y score= art=`: a 10,000-point secret item (Episode 5).
struct Treasure
{
  int x = 0, y = 0; // blocks
  int score = 10000;
  std::string art;
  bool taken = false;
};

struct SignalState
{
  bool on = false;
  bool air = false; // Dead Air itself (or Wraparound), not just a level with a treasure
  int color = kSignalRed;
  int pending = -1; // the incoming colour while a change waits
  int flash = 0;    // frames left of the incoming bars' flash
  int pips[3] = {0, 0, 0};
  int tint = kSignalRed; // the Tri-Tone's tint
  int tintCool = 0;
  std::vector<ColorBar> bars;
  std::vector<RgbDial> dials;
  // The Test Card's arena: walking in shuts a white wall behind the runner.
  CellBox arena{-100, -100, 0, 0};
  int wallX = -1, wallY0 = 0, wallY1 = 0; // blocks
  int exitX = -1, exitY0 = 0, exitY1 = 0; // blocks: the way on, shut too until the card is down
  bool wallShut = false;
  int card = -1; // enemy index
  int cardHp[3] = {3, 3, 3};
  int cardTurn = 0;  // quarter turns
  int cardCool = 30;
  int cardStandBy = 0; // frames of "please stand by" after a hit
  CardBeam beam;
  bool cardDead = false;
  // Tune in: past this block with the card down.
  int tuneX = -1;
  bool tuned = false;
  int shake = 0;
  // Snow over a quarter of the screen (0 top left, 1 top right, 2 bottom
  // left, 3 bottom right) from a swarm latched on that side.
  int snow = 0;
  int snowQuarter = 0;
  std::vector<Treasure> treasures;
  int egg = 0; // frame 4242: the backdrop says thanks
  // Wraparound (rules=wrap_xy).
  bool wrap = false;
  int homeX = 0, homeY = 0; // cells: where a swarm's touch puts the runner
};

} // namespace gr
