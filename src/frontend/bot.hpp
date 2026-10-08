#pragma once

#include "game/input.hpp"

namespace td
{

class World;

// A tiny autopilot used for attract mode and for recording gameplay clips
// headlessly. It plays the same way a human would: by producing Input.
// (RigelEngine records and replays input for its demo loop; a bot keeps
// working while the level layout is still changing.)
class Bot
{
public:
  Input play(const World& world);
  Input menu(int cursor, int target, int ticksInMenu) const;

private:
  int mJumpHold = 0;
  int mJumpRelease = 0;
  int mStuckTicks = 0;
  float mLastX = -1.0f;
};

} // namespace td
