#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace gr
{

// Level 16, Cryo Labs (world_cryo.cpp): ice floors, kickers, the Freeze
// Ray's frozen blocks, Lab Arms on their rails, and the Air Hockey bonus.

// A wedge in the ice (`@ kicker x y dir= launch=`, the block the runner's
// feet are in): crossed at 3/4 cell a frame or more, it launches a rise of
// `launch` cells (Nova 2 more) and the runner keeps their speed.
struct Kicker
{
  int bx = 0, by = 0;
  int dir = 1;
  int launch = 14;
};

// A cold draught that hurts (`@ frost rect= dmg= every=`), in blocks.
struct FrostZone
{
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  int dmg = 1, every = 27;
};

// The goal vent on the rink's bumper (`@ goalvent`): a frozen enemy slid
// into it gets a faint cheer, once.
struct GoalVent
{
  int bx = 0, by = 0, w = 1, h = 2; // by: its bottom block
  bool cheered = false;
};

// A Lab Arm's power box (`@ powerbox ID x y arm=`): a solid block (a
// breakable, 3 hits or a sliding block); the arm dies with it.
struct PowerBox
{
  std::string id, arm;
  int breakable = -1;
  int enemy = -1;
};

// Air Hockey (the bonus, rules=zero_friction): its goals (openings in the
// side walls) and where a scored puck comes back.
struct HockeyGoal
{
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0; // blocks
};

// Lab furniture drawn behind the action (`@ deco kind=glass|crewpod|hostpod`).
struct CryoDeco
{
  int kind = 0; // 0 frosted glass partition, 1 a sleeping crew pod, 2 the HOST (SPARE) pod
  int bx = 0, by = 0; // the block it stands in (bottom)
};

struct CryoState
{
  bool on = false;
  bool zeroFriction = false;
  int w = 0;                          // blocks across (for `ice`)
  std::vector<std::uint8_t> ice;      // per block: its top is ice
  std::vector<Kicker> kickers;
  std::vector<FrostZone> frost;
  std::vector<GoalVent> vents;
  std::vector<PowerBox> boxes;
  std::vector<CryoDeco> decos;
  std::vector<std::pair<std::string, int>> ids; // enemies by their level id (Lab Arms)
  int railY = -1, railX0 = 0, railX1 = 0; // the Lab Arms' rail (blocks)
  int pod = -1;                       // the DO NOT OPEN pod (a breakable)
  int hostX = -1, hostY = 0;          // the HOST (SPARE) pod (cells)
  bool hostSeen = false;
  // The runner on ice: speed (cells a frame, signed) and the part of a
  // cell carried over.
  float slide = 0.0f, carry = 0.0f;
  bool fromIce = false; // the slide came off the ice (it carries through the air)
  bool wasAir = false;  // for landings (Rocco on the thin ice)
  int held = -1;     // the Lab Arm holding the runner
  int frostTick = 0;
  int cheer = 0;     // frames of the crowd's cheer left (drawn)
  // Air Hockey.
  std::vector<HockeyGoal> goals;
  int scored = 0, goalTarget = 0;
  int puckX = 0, puckY = 0;           // cells: where a scored puck comes back
  int scoredFlash = 0;
  bool iceAt(int bx, int by) const
  {
    return zeroFriction || (bx >= 0 && by >= 0 && w > 0 && std::size_t(by * w + bx) < ice.size() &&
                             ice[std::size_t(by * w + bx)] != 0);
  }
};

} // namespace gr
