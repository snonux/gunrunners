#pragma once

namespace gr
{

// One 60 Hz tick of raw input. Mirrors RigelEngine's PlayerInput: the game
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

struct Button
{
  bool pressed = false;   // held right now
  bool triggered = false; // went down since the last logic frame
};

// Input as the 15 Hz game logic sees it. Presses that happen between two
// logic frames are latched, so a quick tap is never lost.
struct PlayerInput
{
  bool left = false;
  bool right = false;
  bool up = false;
  bool down = false;
  Button jump;
  Button fire;
};

} // namespace gr
