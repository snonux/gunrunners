#pragma once

#include "frontend/planner.hpp"
#include "game/input.hpp"

#include <deque>

namespace gr
{

class World;

// A small autopilot used for attract mode and for recording gameplay clips
// headlessly. It plays the same way a human would: by producing Input, once
// per 15 Hz logic frame. (RigelEngine records and replays input for its demo
// loop; a bot keeps working while the level layout is still changing.)
class Bot
{
public:
  // The search planner plays; the old reactive rules remain as a fallback
  // for the PoC level (playRules).
  Input play(const World& world);
  Input playRules(const World& world);
  Input menu(int cursor, int target, int ticksInMenu) const;
  // Black Halo's arena: short look-ahead fights instead of the planner.
  Input fightBoss(const World& world);
  // Pilot Seat: fly at the cardboard.
  Input fly(const World& world);
  // Trapmaster: spring the trap that will catch the most hunters.
  Input trapmaster(const World& world);
  void setTakeBonus(bool take) { mPlanner.setTakeBonus(take); }

private:
  Planner mPlanner;
  std::deque<Input> mFightQueue;
  std::deque<Input> mTrapQueue;
  bool mFighting = false;
  Input mFightPrev;
  int mJumpHold = 0;
  int mJumpRelease = 0;
  int mStuck = 0;
  int mLastX = -1;
  int mJetpackUntilX = -1;
  bool mFiredLast = false;
  bool mSeekFlamer = false; // stuck at a jetpack wall: go back for the flamethrower
};

} // namespace gr
