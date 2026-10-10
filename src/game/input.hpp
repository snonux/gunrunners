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
  bool pause = false; // Esc / P, gamepad Start: open or close the pause menu
  bool back = false;  // Esc / Backspace, gamepad B: leave a menu
  bool swap = false;  // C, gamepad Y: switch to the next runner mid-level
  bool quickSave = false; // F5: save to the quick save slot
  bool quickLoad = false; // F9: load the quick save
  bool map = false;   // M, gamepad Back: open or close the level map

  Input operator|(const Input& o) const
  {
    Input r;
    r.left = left || o.left;
    r.right = right || o.right;
    r.up = up || o.up;
    r.down = down || o.down;
    r.jump = jump || o.jump;
    r.fire = fire || o.fire;
    r.confirm = confirm || o.confirm;
    r.pause = pause || o.pause;
    r.back = back || o.back;
    r.swap = swap || o.swap;
    r.quickSave = quickSave || o.quickSave;
    r.quickLoad = quickLoad || o.quickLoad;
    r.map = map || o.map;
    return r;
  }
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
