// Rebinding: the defaults, moving a key or pad button to another action,
// the profile round trip, the keyboard read through custom bindings, a
// virtual pad with a rebound button, and the touch layout editor.

#include "frontend/bindings.hpp"
#include "frontend/controls.hpp"
#include "frontend/touch_controls.hpp"
#include "game/profile.hpp"
#include "render/renderer.hpp"

#include <SDL.h>

#include <cstdio>
#include <cstdlib>
#include <string>

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

void model()
{
  Bindings b = Bindings::defaults();
  check(b.keys[std::size_t(Act::Jump)][0] == SDL_SCANCODE_X, "default: X jumps");
  check(b.keys[std::size_t(Act::Fire)][0] == SDL_SCANCODE_Z, "default: Z fires");

  b.bindKey(Act::Jump, 0, SDL_SCANCODE_Z);
  check(b.keys[std::size_t(Act::Jump)][0] == SDL_SCANCODE_Z, "Z bound to jump");
  check(b.keys[std::size_t(Act::Fire)][0] == 0, "...and taken from fire");
  b.bindPad(Act::Swap, SDL_CONTROLLER_BUTTON_A);
  check(b.pad[std::size_t(Act::Swap)] == std::vector<int>{SDL_CONTROLLER_BUTTON_A}, "pad A bound to switch");
  check(!b.pad[std::size_t(Act::Fire)].empty() && b.pad[std::size_t(Act::Fire)][0] == SDL_CONTROLLER_BUTTON_B,
    "...and taken from fire, which keeps B");

  std::map<std::string, std::vector<int>> controls;
  b.toProfile(controls);
  check(Bindings::fromProfile(controls) == b, "bindings survive the profile");
  check(Bindings::fromProfile({}) == Bindings::defaults(), "an old profile gets the defaults");

  // Through the profile file itself.
  const std::string dir = "bindings_test_profile";
  Profile p;
  p.controls = controls;
  p.controls["touch.layout"] = {10, -20, 0, 0, -300, 0, 0, 0, 0, 0};
  check(p.save(dir), "profile saved");
  const Profile q = Profile::load(dir);
  check(Bindings::fromProfile(q.controls) == b, "bindings read back from profile.txt");
  check(q.controls.at("touch.layout") == p.controls.at("touch.layout"), "touch layout read back");
  std::remove((dir + "/profile.txt").c_str());
}

void keyboard()
{
  Controls c;
  Bindings b = Bindings::defaults();
  b.bindKey(Act::Jump, 0, SDL_SCANCODE_K);
  b.bindKey(Act::Left, 0, SDL_SCANCODE_J);
  c.setBindings(b);
  Uint8 keys[SDL_NUM_SCANCODES] = {};
  keys[SDL_SCANCODE_K] = 1;
  check(c.readKeys(keys, false).jump, "K jumps after rebinding");
  keys[SDL_SCANCODE_K] = 0;
  keys[SDL_SCANCODE_LEFT] = 1;
  check(!c.readKeys(keys, false).left, "in a level the old Left arrow no longer walks");
  check(c.readKeys(keys, true).left, "...but menus still answer the arrows");
  keys[SDL_SCANCODE_LEFT] = 0;
  keys[SDL_SCANCODE_ESCAPE] = 1;
  check(c.readKeys(keys, false).pause, "Esc always pauses");
}

void pad()
{
#if SDL_VERSION_ATLEAST(2, 24, 0)
  Controls c;
  Bindings b = Bindings::defaults();
  b.bindPad(Act::Jump, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
  c.setBindings(b);
  SDL_VirtualJoystickDesc desc{};
  desc.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
  desc.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
  desc.naxes = SDL_CONTROLLER_AXIS_MAX;
  desc.nbuttons = SDL_CONTROLLER_BUTTON_MAX;
  const int device = SDL_JoystickAttachVirtualEx(&desc);
  auto pump = [&c] {
    SDL_GameControllerUpdate();
    SDL_Event ev;
    while (SDL_PollEvent(&ev))
      c.handleEvent(ev);
  };
  pump();
  SDL_Joystick* joy = SDL_JoystickFromInstanceID(SDL_JoystickGetDeviceInstanceID(device));
  SDL_JoystickSetVirtualButton(joy, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, 1);
  pump();
  check(c.read().jump, "pad: LB jumps after rebinding");
  SDL_JoystickSetVirtualButton(joy, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, 0);
  SDL_JoystickSetVirtualButton(joy, SDL_CONTROLLER_BUTTON_X, 1);
  pump();
  check(!c.read().jump, "pad: X no longer jumps");
  SDL_JoystickDetachVirtual(device);
  pump();
#else
  std::printf("skipped pad checks: virtual gamepads need SDL 2.24\n");
#endif
}

void touchLayout(Renderer& r)
{
  TouchControls t(r, true);
  t.configure(TouchControls::Layout::Play, 1);
  // Jump sits left of the corner (see touch_test.cpp): drag it 300 left.
  t.setEditing(true);
  t.touch(1, 988.0f, 628.0f);
  t.slide(1, 688.0f, 628.0f);
  t.release(1);
  check(t.takeLayoutChange(), "editor: dragging reports a change");
  const std::vector<int> layout = t.layout();
  check(layout.size() == 10 && layout[2] == -300 && layout[3] == 0, "editor: jump moved 300 left");
  check(!t.read().jump, "editor: dragging does not jump");
  t.touch(2, 760.0f, 352.0f); // DONE
  check(t.read().back, "editor: DONE reads as back");
  t.release(2);
  t.setEditing(false);

  TouchControls u(r, true);
  u.configure(TouchControls::Layout::Play, 1);
  u.setLayout(layout);
  u.touch(3, 688.0f, 628.0f);
  check(u.read().jump, "play: jump answers at its new spot");
  u.release(3);
  u.touch(4, 520.0f, 300.0f);
  check(!u.read().jump, "play: the left half is still the stick");
  u.release(4);
  u.configure(TouchControls::Layout::Menu, 1);
  u.touch(5, 1160.0f, 580.0f);
  check(u.read().confirm, "menus keep their own layout");
}

} // namespace

int main()
{
  SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "0");
  SDL_Init(SDL_INIT_EVENTS);
  model();
  keyboard();
  pad();
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  {
    Renderer r(sdl);
    touchLayout(r);
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf("%s\n", gFailures ? "FAILED" : "all passed");
  return gFailures ? 1 : 0;
}
