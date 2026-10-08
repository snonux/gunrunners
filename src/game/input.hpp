#pragma once

namespace td
{

// One tick of player input. Mirrors RigelEngine's PlayerInput: the game
// logic never talks to SDL directly, which is what lets the bot and the
// headless recorder drive the exact same code path as a human.
struct Input
{
  bool left = false;
  bool right = false;
  bool up = false;
  bool down = false;
  bool jump = false;
  bool fire = false;
  bool confirm = false;
};

} // namespace td
