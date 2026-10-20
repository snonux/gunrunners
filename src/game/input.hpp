#pragma once

namespace gr
{

// Free aim: shots at any angle. A direction (the right stick), or with
// `spot` a spot measured from the runner's middle (a touch; logical pixels
// in Input, cells in PlayerInput). Plain guns shoot along it exactly.
struct Aim
{
  bool on = false;
  bool spot = false;
  float x = 0.0f, y = 0.0f;
};

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
  bool randomRunner = false; // R, gamepad RS: switch to a runner picked at random
  bool nextWeapon = false; // Q, gamepad LT (Android: left stick click): the next gun carried
  // Touch (thumbs scheme): fire aimed left or right turns the runner that
  // way first, and a jump that came from pushing the stick up (which on a
  // ladder climbs instead).
  bool aimLeft = false;
  bool aimRight = false;
  bool upJump = false;
  // Free aim (the right stick, a touch). Guns that only shoot up, ahead or
  // low get the aim snapped instead: up, or crouched (aimUp, aimDown, with
  // aimLeft and aimRight); the game picks which. aimFire: the right stick
  // is pushed, so the shots keep coming while it is held.
  Aim aim;
  bool aimUp = false;
  bool aimDown = false;
  bool aimFire = false;

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
    r.randomRunner = randomRunner || o.randomRunner;
    r.nextWeapon = nextWeapon || o.nextWeapon;
    r.aimLeft = aimLeft || o.aimLeft;
    r.aimRight = aimRight || o.aimRight;
    r.upJump = upJump || o.upJump;
    r.aim = aim.on ? aim : o.aim;
    r.aimUp = aimUp || o.aimUp;
    r.aimDown = aimDown || o.aimDown;
    r.aimFire = aimFire || o.aimFire;
    return r;
  }
};

// The right stick pushed the unit way (x, y): aimed that way, and firing
// while held. Snapped as a tap there would be, for the guns that only shoot
// up, ahead or low: within 45 degrees of up shoots up, further below than
// beside a crouched shot, the rest to that side.
inline void aimStick(Input& in, float x, float y)
{
  in.aim.on = true;
  in.aim.spot = false;
  in.aim.x = x;
  in.aim.y = y;
  in.fire = in.aimFire = true;
  const float ax = x < 0.0f ? -x : x;
  in.aimUp = y < 0.0f && -y >= ax;
  in.aimDown = y > 0.0f && y > ax;
  if (!in.aimUp && ax > 0.2f)
    (x < 0.0f ? in.aimLeft : in.aimRight) = true;
}

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
  bool run = false; // the way held was double tapped (run_gesture.hpp)
  Button jump;
  Button fire;
  Button use; // get in or out of a vehicle
  Aim aim;           // free aim (see Aim), in cells
  bool aimHold = false; // the right stick is held: fire keeps shooting
};

} // namespace gr
