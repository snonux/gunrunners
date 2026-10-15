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
  bool use = false;   // E, gamepad LB: get in or out of a vehicle
  bool say = false;   // V: the runner says a random line
  bool nextWeapon = false; // Q, gamepad LT (Android: left stick click): the next gun carried
  // Touch (thumbs scheme): fire aimed left or right turns the runner that
  // way first, and a jump that came from pushing the stick up (which on a
  // ladder climbs instead).
  bool aimLeft = false;
  bool aimRight = false;
  bool upJump = false;

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
    r.use = use || o.use;
    r.say = say || o.say;
    r.nextWeapon = nextWeapon || o.nextWeapon;
    r.aimLeft = aimLeft || o.aimLeft;
    r.aimRight = aimRight || o.aimRight;
    r.upJump = upJump || o.upJump;
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
  Button use; // get in or out of a vehicle
};

} // namespace gr
