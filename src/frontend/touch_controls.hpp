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
//   thumb lands) and Jump, Fire, Switch runner, Pause and Map on the
//   right, laid out and mapped like a pad's A (Jump, the corner), X (Fire,
//   left of it), Y (Switch runner, above it), Start and Back.
// - In menus, cutscenes and tallies: direct touch. A tap on an item picks
//   it and a drag scrolls (the game gets them from takeTaps / takeDrag);
//   a small BACK sits in the top left corner. The pad button in the bottom
//   left corner brings back the menu pad (a fixed d-pad, OK and BACK) for
//   those who'd rather steer the menus like a gamepad.
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
//
// The thumbs scheme (the default) drops the Jump and Fire buttons. The left
// thumb walks and turns with the stick in the bottom left, down crouches,
// up (or a flick up) jumps. Anywhere else, a touch shoots at the spot it
// lands on: plain guns shoot exactly at it, at any angle, the rest get
// the shot snapped to the ones they have (up, left, right, or crouched to
// one side). Sliding the finger keeps aiming at it; a flick re-aims. Switch runner,
// Use (vehicles), Map and Pause sit in a row along the top, away from both
// thumbs. The buttons scheme keeps Jump and Fire buttons and the swipes
// above, and a touch on an empty spot of the right half shoots at it too.
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
  // Bakes the overlay's textures (again): the constructor does, and the
  // window code after a device reset, which leaves them dead. dropTextures
  // lets go of them first, while the SDL_Renderer that made them still
  // exists (recoverRender); nothing may draw the overlay in between.
  void bakeTextures(Renderer& renderer);
  void dropTextures();

  // The thumbs scheme (see above) or the Jump and Fire buttons.
  void setThumbs(bool on) { mThumbs = on; }
  bool thumbs() const { return mThumbs; }
  // Where the shooting thumb aims: ahead (the way the runner faces), up,
  // down (crouched), left, right, or crouched to the left or right.
  enum Aim
  {
    kAimAhead,
    kAimUp,
    kAimDown,
    kAimLeft,
    kAimRight,
    kAimDownLeft,
    kAimDownRight,
  };
  int aim() const { return mAim; }
  // Where the runner is on the screen (logical units), so a touch can be
  // aimed at the spot it lands on; known false when there is no runner to
  // aim from (then a touch shoots ahead). Call once per frame.
  void setRunner(float x, float y, bool known)
  {
    mRunnerX = x;
    mRunnerY = y;
    mRunnerKnown = known;
  }
  // The aim from the runner to a spot on the screen.
  int aimAt(float x, float y) const;
  // The time now (SDL ticks, ms): a touch on the right waits a moment for a
  // flick before it fires. Call once per tick before read().
  void advance(Uint32 now);

  // Menus: taps since the last call (logical positions), and how far a
  // finger dragged since the last call. Call once per frame.
  std::vector<SDL_FPoint> takeTaps();
  SDL_FPoint takeDrag();
  // The menu pad (d-pad, OK, BACK) is shown in menus; the player switches
  // it with the pad button, takeMenuPadChange is true once per switch.
  void setMenuPad(bool on) { mMenuPad = on; }
  bool menuPad() const { return mMenuPad; }
  bool takeMenuPadChange();

  // Where the player moved the controls in a level (the profile keeps it):
  // x, y offsets for the stick, then jump, fire, switch, pause, map, use
  // and next weapon.
  void setLayout(const std::vector<int>& offsets);
  std::vector<int> layout() const;
  // The layout editor: every control can be dragged; RESET puts them back,
  // DONE reads as a back press. takeLayoutChange is true once per change.
  void setEditing(bool on);
  bool editing() const { return mEditing; }
  bool takeLayoutChange();
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
    kFire, // X; BACK in menus
    kSwap, // Y
    kPause,
    kMap, // left of pause
    kUse, // get in or out of a vehicle, along the top
    kWeapon, // the next gun carried, along the top
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
    // Menus: a finger on neither the pad nor a button points at the menu.
    bool pointer = false;
    bool dragging = false; // moved too far to be a tap (or was used up)
    float px = 0.0f, py = 0.0f; // where the drag was last reported
    // Thumbs: a touch on the right half shoots. It waits a moment for a
    // flick (which aims) before it fires, and lets go of fire for a tick
    // after a flick so the next shot goes the new way.
    bool shooter = false;
    bool firing = false;
    int fireGap = 0;
    bool fired = false; // a read() saw it fire
    Uint32 downAt = 0;
  };

  void press(SDL_FingerID id, float x, float y);
  void move(SDL_FingerID id, float x, float y);
  void lift(SDL_FingerID id);
  void swipe(Finger& f);
  void drawAt(Renderer& renderer, float sx, float sy) const;
  Input held() const;
  Input aimed() const; // fire, aimed the way the right thumb last flicked
  void aimFlick(Finger& f);
  void aimFollow(float x, float y);
  bool moved() const;
  Circle place(Circle c, int element) const;
  void editPress(Finger& f);
  void editMove(const Finger& f);
  Circle stickHome() const;
  bool inStickZone(float x, float y) const;
  bool onFireMark(float x, float y) const;
  Circle padToggle() const;
  bool menuPadShown() const;
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
  std::array<SDL_FPoint, 1 + kButtonCount> mOffset{};
  bool mEditing = false;
  bool mLayoutChanged = false;
  bool mMenuPad = false;
  bool mThumbs = true;
  bool mThumbsUsed = false; // till the first touch in a level, a hint shows how it goes
  int mAim = kAimAhead;
  // A touch aims at its spot exactly (not the fire mark's shot ahead).
  bool mExact = false;
  float mAimPtX = 0.0f, mAimPtY = 0.0f;
  float mRunnerX = 0.0f, mRunnerY = 0.0f;
  bool mRunnerKnown = false;
  bool mMenuPadChanged = false;
  std::vector<SDL_FPoint> mTaps;
  SDL_FPoint mDrag{0.0f, 0.0f};

  Texture mStick;
  Texture mDpad;
  Texture mNub;
  std::array<Texture, kButtonCount> mPlayIcons;
  Texture mOk;
  Texture mBack;
  Texture mPadIcon;
  std::array<Texture, 4> mAimArrows; // up, down, left, right
};

} // namespace gr
