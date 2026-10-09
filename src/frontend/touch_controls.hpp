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
//
// Made to be hit without looking: the controls hug the physical screen's
// edges (on a phone wider than 16:9 they sit in the bars beside the
// picture), a touch near a button but outside it still presses the nearest
// one, a thumb that drifts off keeps its button, and sliding onto another
// button switches to it.
//
// Swipes work as well, anywhere on the right half in a level: a quick swipe
// up jumps (held while the finger stays down, so a thumb on Fire can jump
// without letting go), a quick swipe down switches runner.
class TouchControls
{
public:
  enum class Layout
  {
    Play,
    Menu,
  };

  TouchControls(Renderer& renderer, bool visible);

  // The renderer's output size in pixels, so the controls can use the
  // letterbox bars around the 1280x720 picture. Call once per frame.
  void setScreen(int pixelW, int pixelH);
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

  // Tests: fingers at logical positions at a time in ms, without SDL events.
  void touch(SDL_FingerID id, float x, float y, Uint32 ms = 0)
  {
    mNow = ms;
    press(id, x, y);
  }
  void slide(SDL_FingerID id, float x, float y, Uint32 ms = 0)
  {
    mNow = ms;
    move(id, x, y);
  }
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
    int button;     // the button this finger holds, or -1
    float sx, sy;   // where the current swipe started
    Uint32 st;      // ...and when
    bool swipeJump; // swiped up: holds jump until lifted
    bool swiped;    // made a swipe: keeps its button from now on
  };

  void press(SDL_FingerID id, float x, float y);
  void move(SDL_FingerID id, float x, float y);
  void lift(SDL_FingerID id);
  void swipe(Finger& f);
  void drawAt(Renderer& renderer, float sx, float sy) const;
  Input held() const;
  Circle stickHome() const;
  Circle button(int b) const;
  bool buttonShown(int b) const;
  int buttonFor(float x, float y) const;
  int buttonSlidTo(float x, float y, int current) const;
  bool buttonHeld(int b) const;

  Layout mLayout = Layout::Menu;
  float mScale = 1.0f;
  // The visible area in logical coordinates: the picture is 0..1280 x
  // 0..720, the bars beside or above it reach further.
  float mLeft = 0.0f, mTop = 0.0f;
  float mRight = float(kScreenW), mBottom = float(kScreenH);
  bool mVisible;
  std::vector<Finger> mFingers;
  Input mTapped; // presses since the last read()
  Uint32 mNow = 0; // the latest touch event's time, ms

  Texture mStick;
  Texture mDpad;
  Texture mNub;
  std::array<Texture, kButtonCount> mPlayIcons;
  Texture mOk;
  Texture mBack;
};

} // namespace gr
