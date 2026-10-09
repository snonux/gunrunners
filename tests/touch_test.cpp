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
constexpr float kJumpX = 1165.0f, kJumpY = 590.0f;
constexpr float kFireX = 1012.0f, kFireY = 640.0f;
constexpr float kSwapX = 1165.0f, kSwapY = 440.0f;
constexpr float kPauseX = 1228.0f, kPauseY = 120.0f;

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

  t.touch(2, kJumpX, kJumpY);
  t.touch(3, kFireX, kFireY);
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
  t.touch(1, 1280.0f - 115.0f * 1.2f, 720.0f - 130.0f * 1.2f);
  check(t.read().jump, "large: jump at its large spot");
  t.release(1);
  t.configure(Layout::Play, 0);
  t.touch(2, 1280.0f - 115.0f * 0.8f, 720.0f - 130.0f * 0.8f);
  check(t.read().jump, "small: jump at its small spot");
  t.release(2);
  t.touch(3, 640.0f + 20.0f, 300.0f);
  check(!t.read().jump, "small: empty screen presses nothing");
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
