#pragma once

#include "game/input.hpp"
#include "render/renderer.hpp"

#include <SDL.h>

#include <vector>

namespace gr
{

// An on-screen gamepad for touch screens (Android, or --touch on a desktop
// touch screen): a d-pad on the left half of the screen and Jump, Fire,
// Switch and Pause buttons on the right, mapped like a real pad's A, B, Y
// and Start. Any finger on the left half steers by its offset from the
// d-pad's centre, so the thumb can slide around without lifting.
// The overlay hides itself when a keyboard or gamepad is used and comes
// back with the next touch.
class TouchControls
{
public:
  explicit TouchControls(Renderer& renderer, bool visible);

  // Feed every SDL event through here (after Controls).
  void handleEvent(const SDL_Event& ev, SDL_Window* window);
  // The buttons held now, plus any tap that started and ended since the
  // last call (a quick tap between two ticks is not lost). Call once per tick.
  Input read();
  // Draws the overlay in the game's 1280x720 logical coordinates.
  void draw(Renderer& renderer) const;

  // Headless tests: a finger at this logical position (no SDL events).
  void injectFinger(SDL_FingerID id, float x, float y);

private:
  struct Finger
  {
    SDL_FingerID id;
    float x;
    float y;
    bool dpad; // started on the left half: steers until lifted
  };
  enum ButtonId
  {
    kJump,
    kFire,
    kSwap,
    kPause,
    kButtonCount,
  };
  struct ButtonSpot
  {
    float x, y, r;
  };

  void press(SDL_FingerID id, float x, float y);
  void move(SDL_FingerID id, float x, float y);
  void lift(SDL_FingerID id);
  Input held() const;
  void dpadDirections(bool& left, bool& right, bool& up, bool& down) const;
  bool buttonHeld(int b) const;
  int buttonAt(float x, float y) const;

  static const ButtonSpot kButtons[kButtonCount];
  std::vector<Finger> mFingers;
  Input mTapped; // presses since the last read()
  bool mVisible;
  Texture mDpad;
  Texture mDpadNub;
  Texture mButtonTex[kButtonCount];
};

} // namespace gr
