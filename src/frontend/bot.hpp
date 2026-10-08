#pragma once

#include "game/input.hpp"

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
  Input play(const World& world);
  Input menu(int cursor, int target, int ticksInMenu) const;

private:
  int mJumpHold = 0;
  int mJumpRelease = 0;
  int mStuck = 0;
  int mLastX = -1;
  int mJetpackUntilX = -1;
  bool mFiredLast = false;
  bool mSeekFlamer = false; // stuck at a jetpack wall: go back for the flamethrower
};

} // namespace gr
