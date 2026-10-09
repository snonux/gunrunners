#pragma once

#include "game/input.hpp"
#include "render/renderer.hpp"

#include <SDL.h>

#include <array>
#include <vector>

namespace gr
{

// An on-screen gamepad for touch screens (Android, or --touch on a desktop
// touch screen). Two layouts:
//
// - In a level: a floating stick on the left half (it centres wherever the
//   thumb lands) and Jump, Fire, Switch runner and Pause on the right,
//   mapped like a pad's A, B, Y and Start.
// - In menus, cutscenes and tallies: a fixed d-pad, OK and BACK.
//
// Three sizes (the TOUCH PAD setting). The overlay hides itself when a
// keyboard or gamepad is used and comes back with the next touch.
class TouchControls
{
public:
  enum class Layout
  {
    Play,
    Menu,
  };

  TouchControls(Renderer& renderer, bool visible);

  // Feed every SDL event through here (after Controls).
  void handleEvent(const SDL_Event& ev, SDL_Window* window);
  // Call once per frame before read(): which layout, and the size setting
  // (0 small, 1 medium, 2 large).
  void configure(Layout layout, int size);
  // The buttons held now, plus any press since the last call, so a tap that
  // starts and ends between two ticks is not lost. Call once per tick.
  Input read();
  // Draws the overlay in the game's 1280x720 logical coordinates.
  void draw(Renderer& renderer) const;
  bool visible() const { return mVisible; }

  // Tests: fingers at logical positions, without SDL events.
  void touch(SDL_FingerID id, float x, float y) { press(id, x, y); }
  void slide(SDL_FingerID id, float x, float y) { move(id, x, y); }
  void release(SDL_FingerID id) { lift(id); }

private:
  enum ButtonId
  {
    kJump, // A; OK in menus
    kFire, // B; BACK in menus
    kSwap, // Y
    kPause,
    kButtonCount,
  };
  struct Circle
  {
    float x, y, r;
  };
  struct Finger
  {
    SDL_FingerID id;
    float x, y;
    bool stick;     // landed on the left half: steers until lifted
    float ox, oy;   // the stick's centre for this finger
  };

  void press(SDL_FingerID id, float x, float y);
  void move(SDL_FingerID id, float x, float y);
  void lift(SDL_FingerID id);
  Input held() const;
  Circle stickHome() const;
  Circle button(int b) const;
  bool buttonShown(int b) const;
  int buttonAt(float x, float y) const;
  bool buttonHeld(int b) const;

  Layout mLayout = Layout::Menu;
  float mScale = 1.0f;
  bool mVisible;
  std::vector<Finger> mFingers;
  Input mTapped; // presses since the last read()

  Texture mStick;
  Texture mDpad;
  Texture mNub;
  std::array<Texture, kButtonCount> mPlayIcons;
  Texture mOk;
  Texture mBack;
};

} // namespace gr
