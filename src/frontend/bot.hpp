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
  // Pinball Mine: time the flippers by trying them in copies of the world.
  Input pinball(const World& world);
  // The Floor Is Lava: pick each bounce by trying moves in copies of the
  // world, looking one head further on.
  Input floorLava(const World& world);
  Input surf(const World& world);
  // Idol Mines: climb into a cart that is going your way and ride it,
  // hopping the gaps and ducking under what hangs low.
  Input ride(const World& world);
  int boardable(const World& world) const;
  void setTakeBonus(bool take) { mPlanner.setTakeBonus(take); }

private:
  Planner mPlanner;
  std::deque<Input> mFightQueue;
  std::deque<Input> mTrapQueue;
  std::deque<Input> mPinQueue;
  std::deque<Input> mLavaQueue;
  Input mLavaPrev;
  std::deque<Input> mSurfQueue;
  Input mSurfPrev;
  std::deque<Input> mRideQueue;
  bool mRiding = false;
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
