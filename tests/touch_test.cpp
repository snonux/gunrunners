// Drives the on-screen gamepad with fingers at known spots and checks the
// Input it produces, in both layouts and at every size.

#include "frontend/touch_controls.hpp"
#include "render/renderer.hpp"

#include <SDL.h>

#include <cstdio>

using namespace gr;

namespace
{

int gFailures = 0;

void check(bool ok, const char* what)
{
  std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok)
    ++gFailures;
}

using Layout = TouchControls::Layout;

// Medium-size spots (see touch_controls.cpp).
constexpr float kStickX = 175.0f, kStickY = 545.0f;
constexpr float kJumpX = 1160.0f, kJumpY = 580.0f;
constexpr float kFireX = 988.0f, kFireY = 628.0f;
// In a level Fire takes the corner and Jump the inner spot.
constexpr float kPlayJumpX = kFireX, kPlayJumpY = kFireY;
constexpr float kPlayFireX = kJumpX, kPlayFireY = kJumpY;
constexpr float kSwapX = 1160.0f, kSwapY = 420.0f;
constexpr float kPauseX = 1224.0f, kPauseY = 120.0f;

void menuLayout(Renderer& r)
{
  TouchControls t(r, true);
  t.configure(Layout::Menu, 1);

  t.touch(1, kStickX, kStickY - 80.0f);
  Input in = t.read();
  check(in.up && !in.down && !in.left && !in.right, "menu: d-pad up");
  t.slide(1, kStickX + 80.0f, kStickY + 80.0f);
  in = t.read();
  check(in.right && in.down && !in.up && !in.left, "menu: sliding to a diagonal presses two directions");
  t.slide(1, kStickX + 5.0f, kStickY - 5.0f);
  in = t.read();
  check(!in.up && !in.down && !in.left && !in.right, "menu: the centre is a dead zone");
  t.release(1);

  // A tap that starts and ends before the next tick still counts once.
  t.touch(2, kJumpX, kJumpY);
  t.release(2);
  in = t.read();
  check(in.confirm && in.jump, "menu: a quick tap on OK confirms");
  in = t.read();
  check(!in.confirm, "menu: ...and only for one tick");

  t.touch(3, kFireX, kFireY);
  in = t.read();
  check(in.back && !in.fire && !in.pause, "menu: BACK backs out, never fires or pauses");
  t.release(3);

  t.touch(4, kSwapX, kSwapY);
  t.touch(5, kPauseX, kPauseY);
  in = t.read();
  check(!in.swap && !in.pause, "menu: no switch or pause buttons");
}

void playLayout(Renderer& r)
{
  TouchControls t(r, true);
  t.configure(Layout::Play, 1);

  // The stick centres where the thumb lands.
  t.touch(1, 380.0f, 420.0f);
  Input in = t.read();
  check(!in.left && !in.right && !in.up && !in.down, "play: landing the thumb steers nowhere");
  t.slide(1, 460.0f, 420.0f);
  in = t.read();
  check(in.right && !in.up && !in.down, "play: floating stick right");
  t.slide(1, 300.0f, 420.0f);
  in = t.read();
  check(in.left, "play: floating stick left");
  // Sliding past the middle of the screen keeps steering.
  t.slide(1, 900.0f, 420.0f);
  in = t.read();
  check(in.right && !in.jump && !in.fire, "play: the stick keeps its thumb across the screen");

  t.touch(2, kPlayJumpX, kPlayJumpY);
  t.touch(3, kPlayFireX, kPlayFireY);
  in = t.read();
  check(in.right && in.jump && in.fire, "play: steer, jump and fire at once");
  t.release(2);
  t.release(3);

  t.touch(4, kSwapX, kSwapY);
  in = t.read();
  check(in.swap && !in.jump, "play: switch runner");
  t.release(4);
  t.touch(5, kPauseX, kPauseY);
  in = t.read();
  check(in.pause, "play: pause");
  t.release(5);
  t.release(1);
  in = t.read();
  check(!in.left && !in.right && !in.jump && !in.pause, "play: nothing held after lifting");
}

void sizes(Renderer& r)
{
  TouchControls t(r, true);
  // Large: the jump button grows towards the screen's middle.
  t.configure(Layout::Play, 2);
  t.touch(1, 1280.0f - 120.0f * 1.2f, 720.0f - 140.0f * 1.2f);
  check(t.read().fire, "large: fire at its large spot");
  t.release(1);
  t.configure(Layout::Play, 0);
  t.touch(2, 1280.0f - 120.0f * 0.8f, 720.0f - 140.0f * 0.8f);
  check(t.read().fire, "small: fire at its small spot");
  t.release(2);
  t.touch(3, 640.0f + 20.0f, 300.0f);
  check(!t.read().jump, "small: empty screen presses nothing");
}

// Near misses: thumbs land beside buttons, between them, at the screen's
// edge, and drift while held.
void forgiving(Renderer& r)
{
  TouchControls t(r, true);
  t.configure(Layout::Play, 1);

  t.touch(1, 1276.0f, 712.0f);
  check(t.read().fire, "the bottom-right corner of the screen fires");
  t.release(1);
  t.touch(2, (kPlayJumpX + kPlayFireX) * 0.5f + 12.0f, (kPlayJumpY + kPlayFireY) * 0.5f);
  Input in = t.read();
  check(in.jump != in.fire, "the gap between jump and fire presses one of them");
  t.release(2);
  t.touch(3, kPlayJumpX - 120.0f, kPlayJumpY + 40.0f);
  check(t.read().jump, "a touch well left of jump still jumps");
  t.release(3);

  t.touch(4, kPlayFireX, kPlayFireY);
  t.slide(4, kPlayFireX - 40.0f, kPlayFireY + 100.0f, 1000);
  in = t.read();
  check(in.fire && !in.jump && !in.swap, "a thumb drifting off fire keeps firing");
  t.slide(4, kPlayJumpX, kPlayJumpY, 2000);
  in = t.read();
  check(in.jump && !in.fire, "sliding onto jump switches to jump");
  t.release(4);
  check(!t.read().jump, "lifting lets go");
}

// On a phone wider than 16:9 the controls sit in the bars beside the
// picture, under the thumbs.
void letterbox(Renderer& r)
{
  TouchControls t(r, true);
  t.configure(Layout::Play, 1);
  t.setScreen(2400, 1080); // 20:9: 160 logical units of bar on each side
  SDL_Event ev{};
  ev.type = SDL_FINGERDOWN;
  ev.tfinger.fingerId = 1;
  ev.tfinger.x = 0.99f;
  ev.tfinger.y = 0.85f;
  t.handleEvent(ev, nullptr);
  check(t.read().fire, "20:9: a thumb in the right-hand bar fires");
  ev.type = SDL_FINGERUP;
  t.handleEvent(ev, nullptr);
  t.touch(2, 1440.0f - 120.0f, 720.0f - 140.0f);
  check(t.read().fire, "20:9: fire sits 120 in from the screen's right edge");
  t.release(2);
  t.touch(3, -160.0f + 175.0f, 400.0f);
  t.slide(3, -160.0f + 260.0f, 400.0f);
  check(t.read().right, "20:9: the stick works in the left-hand bar");
  t.release(3);
}

void swipes(Renderer& r)
{
  TouchControls t(r, true);
  t.configure(Layout::Play, 1);

  t.touch(1, kPlayFireX, kPlayFireY, 0);
  t.slide(1, kPlayFireX, kPlayFireY - 100.0f, 100);
  Input in = t.read();
  check(in.jump && in.fire, "a quick swipe up from fire jumps and keeps firing");
  t.slide(1, kPlayFireX, kPlayFireY - 110.0f, 400);
  in = t.read();
  check(in.jump && in.fire, "...jump stays held while the finger is down");
  t.release(1);
  check(!t.read().jump, "...and lets go with it");

  t.touch(2, 800.0f, 300.0f, 1000);
  t.slide(2, 800.0f, 400.0f, 1100);
  in = t.read();
  check(in.swap && !in.jump, "a quick swipe down on the right half switches runner");
  check(!t.read().swap, "...once");
  t.release(2);

  t.touch(3, kPlayFireX, kPlayFireY, 2000);
  t.slide(3, kPlayFireX, kPlayFireY - 40.0f, 2300);
  t.slide(3, kPlayFireX, kPlayFireY - 80.0f, 2600);
  in = t.read();
  check(!in.jump && in.fire, "a slow drift up is not a swipe");
  t.release(3);

  t.configure(Layout::Menu, 1);
  t.touch(4, kFireX, kFireY, 3000); // BACK
  t.slide(4, kFireX, kFireY - 100.0f, 3100);
  in = t.read();
  check(!in.jump && !in.confirm, "menus: no swipes");
  t.release(4);
}

void visibility(Renderer& r)
{
  TouchControls t(r, true);
  SDL_Event key{};
  key.type = SDL_KEYDOWN;
  key.key.keysym.scancode = SDL_SCANCODE_LEFT;
  t.handleEvent(key, nullptr);
  check(!t.visible(), "a key press hides the overlay");
  t.touch(1, 900.0f, 300.0f);
  check(t.visible(), "a touch brings it back");
  key.key.keysym.scancode = SDL_SCANCODE_AC_BACK;
  t.handleEvent(key, nullptr);
  check(t.visible(), "Android's back key keeps it");
}

} // namespace

int main()
{
  SDL_Init(0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  {
    Renderer r(sdl);
    menuLayout(r);
    playLayout(r);
    sizes(r);
    forgiving(r);
    letterbox(r);
    swipes(r);
    visibility(r);
    // Drawing both layouts at every size must not trip anything.
    TouchControls t(r, true);
    t.touch(1, 300.0f, 500.0f);
    for (int size = 0; size < 3; ++size)
      for (Layout l : {Layout::Menu, Layout::Play})
      {
        t.configure(l, size);
        t.draw(r);
      }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf("%s\n", gFailures ? "FAILED" : "all passed");
  return gFailures ? 1 : 0;
}
