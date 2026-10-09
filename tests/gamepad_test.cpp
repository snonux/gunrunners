// Plugs a virtual gamepad in and out while the game's input code watches,
// as a Bluetooth or USB pad does on Linux and Android: it must be opened
// on arrival, drive the game's Input, hide the touch overlay, and be
// closed when it goes away.

#include "frontend/controls.hpp"
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

} // namespace

int main()
{
#if SDL_VERSION_ATLEAST(2, 24, 0)
  SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "0");
  if (SDL_Init(SDL_INIT_EVENTS) != 0)
    return 1;
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  {
    Renderer renderer(sdl);
    Controls controls;
    TouchControls touch(renderer, true);
    const auto pump = [&] {
      SDL_GameControllerUpdate();
      SDL_Event ev;
      while (SDL_PollEvent(&ev))
      {
        touch.handleEvent(ev, nullptr);
        controls.handleEvent(ev);
      }
    };

    SDL_VirtualJoystickDesc desc{};
    desc.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    desc.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
    desc.naxes = SDL_CONTROLLER_AXIS_MAX;
    desc.nbuttons = SDL_CONTROLLER_BUTTON_MAX;
    desc.name = "Test Pad";
    const int device = SDL_JoystickAttachVirtualEx(&desc);
    check(device >= 0, "a virtual pad attaches");
    pump();
    SDL_Joystick* joy = SDL_JoystickFromInstanceID(SDL_JoystickGetDeviceInstanceID(device));
    check(joy != nullptr, "the pad is opened when it arrives");
    if (!joy)
      return 1;

    SDL_JoystickSetVirtualButton(joy, SDL_CONTROLLER_BUTTON_A, 1);
    pump();
    Input in = controls.read();
    check(in.fire && in.confirm && !in.jump, "A fires and confirms");
    check(!touch.visible(), "a pad button hides the touch overlay");
    SDL_JoystickSetVirtualButton(joy, SDL_CONTROLLER_BUTTON_A, 0);

    touch.touch(1, 900.0f, 300.0f);
    touch.release(1);
    check(touch.visible(), "a touch brings the overlay back");
    SDL_JoystickSetVirtualAxis(joy, SDL_CONTROLLER_AXIS_LEFTX, -30000);
    pump();
    in = controls.read();
    check(in.left && !in.right, "the left stick steers");
    check(!touch.visible(), "pushing the stick hides the overlay");
    SDL_JoystickSetVirtualAxis(joy, SDL_CONTROLLER_AXIS_LEFTX, 0);

    SDL_JoystickSetVirtualButton(joy, SDL_CONTROLLER_BUTTON_START, 1);
    SDL_JoystickSetVirtualButton(joy, SDL_CONTROLLER_BUTTON_Y, 1);
    SDL_JoystickSetVirtualButton(joy, SDL_CONTROLLER_BUTTON_X, 1);
    pump();
    in = controls.read();
    check(in.pause && in.swap && in.jump && !in.fire && !in.left, "Start pauses, Y switches runner, X jumps");
    SDL_JoystickSetVirtualButton(joy, SDL_CONTROLLER_BUTTON_START, 0);
    SDL_JoystickSetVirtualButton(joy, SDL_CONTROLLER_BUTTON_Y, 0);
    SDL_JoystickSetVirtualButton(joy, SDL_CONTROLLER_BUTTON_X, 0);
    pump();

    SDL_JoystickDetachVirtual(device);
    pump();
    in = controls.read();
    check(!in.jump && !in.fire && !in.left, "unplugging the pad leaves nothing held");
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf("%s\n", gFailures ? "FAILED" : "all passed");
  return gFailures ? 1 : 0;
#else
  std::printf("skipped: virtual gamepads need SDL 2.24\n");
  return 0;
#endif
}
