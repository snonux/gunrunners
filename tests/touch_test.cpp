// Drives the on-screen gamepad with fingers at known spots and checks the
// Input it produces, in both layouts and at every size. And its textures
// baked again (after SDL_RENDER_DEVICE_RESET) draw it as before.

#include "frontend/touch_controls.hpp"
#include "render/renderer.hpp"

#include <SDL.h>

#include <cstdio>
#include <vector>

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
// In a level too Jump takes the corner and Fire the inner spot, like a
// pad's A and X.
constexpr float kPlayJumpX = kJumpX, kPlayJumpY = kJumpY;
constexpr float kPlayFireX = kFireX, kPlayFireY = kFireY;
constexpr float kSwapX = 1160.0f, kSwapY = 420.0f;
constexpr float kPauseX = 1224.0f, kPauseY = 120.0f;
constexpr float kMapX = 1130.0f, kMapY = 120.0f;

// The menu pad: shown when the player switched it on.
void menuLayout(Renderer& r)
{
  TouchControls t(r, true);
  t.configure(Layout::Menu, 1);
  t.setMenuPad(true);

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
  t.touch(6, kMapX, kMapY);
  in = t.read();
  check(!in.swap && !in.pause && !in.map, "menu: no switch, pause or map buttons");
  for (int f = 4; f <= 6; ++f)
    t.release(f);
  t.takeTaps();

  // Away from the pad a finger still points at the menu.
  t.touch(7, 640.0f, 300.0f);
  t.release(7);
  check(t.takeTaps().size() == 1 && !t.read().confirm, "menu pad: a tap between its controls picks an item");
}

// Without the menu pad (the default): taps, drags, a small BACK and the
// pad button.
void directTouch(Renderer& r)
{
  TouchControls t(r, true);
  t.configure(Layout::Menu, 1);
  check(!t.menuPad(), "direct: the menu pad starts hidden");

  t.touch(1, 640.0f, 300.0f);
  Input in = t.read();
  check(!in.confirm && !in.up && !in.back, "direct: a finger on an item presses nothing");
  check(t.takeTaps().empty(), "direct: ...and is no tap until it lifts");
  t.release(1);
  auto taps = t.takeTaps();
  check(taps.size() == 1 && taps[0].x == 640.0f && taps[0].y == 300.0f, "direct: lifting it taps there");
  check(t.takeTaps().empty(), "direct: ...once");

  t.touch(2, kStickX, kStickY - 80.0f);
  check(!t.read().up, "direct: no d-pad where the menu pad would be");
  t.release(2);
  check(t.takeTaps().size() == 1, "direct: ...that spot taps too");

  t.touch(3, 640.0f, 400.0f);
  t.slide(3, 640.0f, 410.0f);
  t.release(3);
  check(t.takeTaps().size() == 1, "direct: a finger that wobbles a little still taps");

  t.touch(4, 640.0f, 400.0f);
  t.slide(4, 640.0f, 360.0f);
  t.slide(4, 600.0f, 300.0f);
  const SDL_FPoint d = t.takeDrag();
  check(d.x == -40.0f && d.y == -100.0f, "direct: a drag reports how far it went");
  t.slide(4, 600.0f, 280.0f);
  check(t.takeDrag().y == -20.0f, "direct: ...since the last call");
  t.release(4);
  check(t.takeTaps().empty(), "direct: a drag is no tap");

  t.touch(5, 56.0f, 56.0f);
  in = t.read();
  check(in.back && !in.fire && !in.confirm, "direct: the small BACK in the corner backs out");
  t.release(5);
  check(t.takeTaps().empty(), "direct: ...and taps nothing");

  t.touch(6, 56.0f, 664.0f);
  check(t.menuPad() && t.takeMenuPadChange(), "direct: the pad button brings the menu pad");
  check(!t.takeMenuPadChange(), "direct: ...one change");
  t.release(6);
  check(t.takeTaps().empty(), "direct: ...and taps nothing");
  t.touch(7, kStickX, kStickY - 80.0f);
  check(t.read().up, "direct: the menu pad's d-pad works then");
  t.release(7);
  t.touch(8, 56.0f, 664.0f);
  t.release(8);
  check(!t.menuPad(), "direct: the pad button hides it again");

  // A menu that closes under the finger takes no tap, and the finger
  // does nothing in the level.
  t.touch(9, 900.0f, 300.0f);
  t.configure(Layout::Play, 1);
  t.slide(9, 900.0f, 150.0f, 100);
  in = t.read();
  check(!in.jump && !in.swap, "direct: a menu finger does nothing in a level");
  t.release(9);
  check(t.takeTaps().empty() && t.takeDrag().y == 0.0f, "direct: ...and taps nothing");

  t.touch(10, 640.0f, 300.0f);
  t.release(10);
  check(t.takeTaps().empty(), "direct: in a level a touch is no tap");
}

void playLayout(Renderer& r)
{
  TouchControls t(r, true);
  t.setThumbs(false); // the Jump and Fire buttons
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
  check(in.pause && !in.map, "play: pause");
  t.release(5);
  t.touch(6, kMapX, kMapY);
  in = t.read();
  check(in.map && !in.pause, "play: map");
  t.release(6);
  t.release(1);
  in = t.read();
  check(!in.left && !in.right && !in.jump && !in.pause, "play: nothing held after lifting");
}

void sizes(Renderer& r)
{
  TouchControls t(r, true);
  t.setThumbs(false); // the Jump and Fire buttons
  // Large: the jump button grows towards the screen's middle.
  t.configure(Layout::Play, 2);
  t.touch(1, 1280.0f - 120.0f * 1.2f, 720.0f - 140.0f * 1.2f);
  check(t.read().jump, "large: jump at its large spot");
  t.release(1);
  t.configure(Layout::Play, 0);
  t.touch(2, 1280.0f - 120.0f * 0.8f, 720.0f - 140.0f * 0.8f);
  check(t.read().jump, "small: jump at its small spot");
  t.release(2);
  t.touch(3, 640.0f + 20.0f, 300.0f);
  check(!t.read().jump, "small: empty screen presses nothing");
}

// Near misses: thumbs land beside buttons, between them, at the screen's
// edge, and drift while held.
void forgiving(Renderer& r)
{
  TouchControls t(r, true);
  t.setThumbs(false); // the Jump and Fire buttons
  t.configure(Layout::Play, 1);

  t.touch(1, 1276.0f, 712.0f);
  check(t.read().jump, "the bottom-right corner of the screen jumps");
  t.release(1);
  t.touch(2, (kPlayJumpX + kPlayFireX) * 0.5f + 12.0f, (kPlayJumpY + kPlayFireY) * 0.5f);
  Input in = t.read();
  check(in.jump != in.fire, "the gap between jump and fire presses one of them");
  t.release(2);
  t.touch(3, kPlayFireX - 120.0f, kPlayFireY + 40.0f);
  check(t.read().fire, "a touch well left of fire still fires");
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
  t.setThumbs(false); // the Jump and Fire buttons
  t.configure(Layout::Play, 1);
  t.setScreen(2400, 1080); // 20:9: 160 logical units of bar on each side
  SDL_Event ev{};
  ev.type = SDL_FINGERDOWN;
  ev.tfinger.fingerId = 1;
  ev.tfinger.x = 0.99f;
  ev.tfinger.y = 0.85f;
  t.handleEvent(ev, nullptr);
  check(t.read().jump, "20:9: a thumb in the right-hand bar jumps");
  ev.type = SDL_FINGERUP;
  t.handleEvent(ev, nullptr);
  t.touch(2, 1440.0f - 120.0f, 720.0f - 140.0f);
  check(t.read().jump, "20:9: jump sits 120 in from the screen's right edge");
  t.release(2);
  t.touch(3, -160.0f + 175.0f, 400.0f);
  t.slide(3, -160.0f + 260.0f, 400.0f);
  check(t.read().right, "20:9: the stick works in the left-hand bar");
  t.release(3);
}

void swipes(Renderer& r)
{
  TouchControls t(r, true);
  t.setThumbs(false); // the Jump and Fire buttons
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
  t.setMenuPad(true);
  t.touch(4, kFireX, kFireY, 3000); // BACK
  t.slide(4, kFireX, kFireY - 100.0f, 3100);
  in = t.read();
  check(!in.jump && !in.confirm, "menus: no swipes");
  t.release(4);
}

// The thumbs scheme: the left thumb moves, crouches and jumps; the right
// thumb shoots, aimed the way it last flicked; the rest sits along the top.
void thumbs(Renderer& r)
{
  TouchControls t(r, true);
  t.configure(Layout::Play, 1);
  check(t.thumbs(), "thumbs: the default scheme");

  t.touch(1, 300.0f, 400.0f, 0);
  t.slide(1, 380.0f, 400.0f, 10);
  Input in = t.read();
  check(in.right && !in.jump && !in.fire, "thumbs: the left thumb walks");
  t.slide(1, 300.0f, 480.0f, 20);
  in = t.read();
  check(in.down && !in.jump, "thumbs: ...down crouches");
  t.slide(1, 300.0f, 320.0f, 30);
  in = t.read();
  check(in.jump && in.upJump && in.up, "thumbs: ...up jumps");
  t.slide(1, 360.0f, 340.0f, 40);
  in = t.read();
  check(in.jump && in.right, "thumbs: ...up and over is a running jump");
  t.slide(1, 300.0f, 320.0f, 50);
  t.slide(1, 300.0f, 400.0f, 60);
  t.read();
  t.slide(1, 300.0f, 320.0f, 70);
  t.slide(1, 300.0f, 400.0f, 80);
  check(t.read().jump, "thumbs: a flick up between two ticks still jumps");
  t.release(1);

  t.advance(1000);
  t.touch(2, 900.0f, 400.0f, 1000);
  in = t.read();
  check(!in.fire, "thumbs: a right touch waits a moment for a flick");
  t.advance(1060);
  in = t.read();
  check(in.fire && !in.up && !in.down && !in.aimLeft && !in.aimRight && !in.jump,
    "thumbs: ...then fires the way the runner faces");
  t.advance(1500);
  check(t.read().fire, "thumbs: ...and keeps firing while held");
  t.release(2);
  check(!t.read().fire, "thumbs: lifting stops");

  t.touch(3, 900.0f, 400.0f, 2000);
  t.release(3);
  check(t.read().fire, "thumbs: a quick tap still fires once");
  check(!t.read().fire, "thumbs: ...once");

  t.touch(4, 900.0f, 500.0f, 3000);
  t.slide(4, 900.0f, 420.0f, 3030);
  in = t.read();
  check(in.fire && in.up && !in.jump && t.aim() == TouchControls::kAimUp, "thumbs: a flick up fires upwards");
  t.advance(3500);
  in = t.read();
  check(in.fire && in.up, "thumbs: ...and keeps aiming up");
  t.slide(4, 900.0f, 420.0f, 3600);
  t.slide(4, 820.0f, 420.0f, 3650);
  in = t.read();
  check(!in.fire && t.aim() == TouchControls::kAimLeft, "thumbs: a flick left lets go of fire for a tick");
  in = t.read();
  check(in.fire && in.aimLeft && !in.up && !in.left, "thumbs: ...then fires left");
  t.release(4);

  t.touch(5, 1000.0f, 300.0f, 5000);
  t.advance(5060);
  in = t.read();
  check(in.fire && !in.aimLeft && !in.up, "thumbs: each touch aims afresh (ahead, with no runner to aim from)");
  t.release(5);
  t.touch(6, 1000.0f, 300.0f, 6000);
  t.slide(6, 1000.0f, 380.0f, 6040);
  in = t.read();
  check(in.fire && in.down && t.aim() == TouchControls::kAimDown, "thumbs: a flick down fires low");
  t.release(6);
  t.read();

  // Two thumbs: walk, jump and shoot at once.
  t.touch(7, 300.0f, 400.0f, 7000);
  t.slide(7, 360.0f, 330.0f, 7010);
  t.touch(8, 1000.0f, 400.0f, 7010);
  t.advance(7100);
  in = t.read();
  check(in.right && in.jump && in.fire, "thumbs: walk, jump and shoot together");
  t.release(7);
  t.release(8);

  // The other buttons sit along the top, away from both thumbs.
  t.touch(9, 1280.0f - 244.0f, 120.0f, 8000);
  in = t.read();
  check(in.swap && !in.fire, "thumbs: switch runner is at the top");
  t.release(9);
  t.touch(10, 1280.0f - 338.0f, 120.0f, 8100);
  in = t.read();
  check(in.use && !in.fire, "thumbs: so is use (vehicles)");
  t.release(10);
  t.touch(11, kMapX, kMapY, 8200);
  check(t.read().map, "thumbs: and the map");
  t.release(11);
  t.touch(12, kPauseX, kPauseY, 8300);
  check(t.read().pause, "thumbs: and pause");
  t.release(12);
  t.touch(13, kSwapX, kSwapY, 8400);
  t.advance(8500);
  in = t.read();
  check(in.fire && !in.swap, "thumbs: where switch was, the right thumb now shoots");
  t.release(13);
  t.read();

  // A thumb held when a menu opens does nothing there.
  t.touch(14, 900.0f, 500.0f, 9000);
  t.advance(9100);
  t.configure(Layout::Menu, 1);
  in = t.read();
  check(!in.back && !in.fire && !in.confirm, "thumbs: a held thumb does nothing in a menu");
  t.release(14);
  check(t.takeTaps().empty() && !t.read().fire, "thumbs: ...nor when it lifts");

  t.setThumbs(false);
  t.configure(Layout::Play, 1);
  t.touch(15, 640.0f + 20.0f, 300.0f, 10000);
  t.advance(10200);
  check(t.read().fire, "buttons: an empty spot on the right shoots at it");
  t.release(15);
  t.read();
  t.touch(16, 300.0f, 400.0f, 10300);
  t.slide(16, 300.0f, 320.0f, 10310);
  in = t.read();
  check(in.up && !in.jump, "buttons: the stick up does not jump");
  t.release(16);
}

// Tap to aim: a touch shoots at the spot it lands on, snapped to the shots
// a runner has.
void tapToAim(Renderer& r)
{
  TouchControls t(r, true);
  t.configure(Layout::Play, 1);
  t.setRunner(640.0f, 400.0f, true);
  struct Case
  {
    float x, y;
    int aim;
    const char* what;
  };
  const Case cases[] = {
    {640.0f, 150.0f, TouchControls::kAimUp, "aim: an enemy straight above gets a shot up"},
    {800.0f, 220.0f, TouchControls::kAimUp, "aim: ...and one up and a little to the side"},
    {1000.0f, 330.0f, TouchControls::kAimRight, "aim: one to the right and a bit up gets a shot right"},
    {300.0f, 330.0f, TouchControls::kAimLeft, "aim: the left half outside the stick shoots left"},
    {800.0f, 660.0f, TouchControls::kAimDownRight, "aim: low to the right gets a crouched shot right"},
    {560.0f, 690.0f, TouchControls::kAimDownLeft, "aim: low to the left gets a crouched shot left"},
  };
  Uint32 ms = 0;
  for (const auto& c : cases)
  {
    t.touch(1, c.x, c.y, ms);
    t.advance(ms + 60);
    const Input in = t.read();
    check(in.fire && t.aim() == c.aim, c.what);
    t.release(1);
    t.read();
    ms += 1000;
  }
  t.touch(2, 640.0f, 150.0f, ms);
  t.advance(ms + 60);
  Input in = t.read();
  check(in.fire && in.up && !in.jump, "aim: a shot up holds up, not jump");
  t.release(2);
  t.read();
  t.touch(3, 1000.0f, 330.0f, ms + 1000);
  t.advance(ms + 1060);
  in = t.read();
  check(in.fire && in.aimRight && !in.right, "aim: a shot right turns the runner, it does not walk");
  t.release(3);
  t.read();
  t.touch(4, 175.0f, 600.0f, ms + 2000);
  t.slide(4, 255.0f, 600.0f, ms + 2010);
  t.advance(ms + 2100);
  in = t.read();
  check(in.right && !in.fire, "aim: the stick in the bottom left still steers");
  t.release(4);

  // No runner (a cutscene-like moment, or off screen): straight ahead.
  t.setRunner(0.0f, 0.0f, false);
  t.touch(5, 640.0f, 150.0f, ms + 3000);
  t.advance(ms + 3060);
  check(t.read().fire && t.aim() == TouchControls::kAimAhead, "aim: without a runner on screen, ahead");
  t.release(5);
  t.read();

  // The fire mark in the corner shoots the way the runner faces.
  t.setRunner(640.0f, 400.0f, true);
  t.touch(6, 1160.0f, 580.0f, ms + 4000);
  t.advance(ms + 4060);
  check(t.read().fire && t.aim() == TouchControls::kAimAhead, "aim: the fire mark shoots ahead");
  t.release(6);
  t.read();

  // The buttons scheme aims taps on empty spots the same way; a swipe from
  // one is still a swipe, not a shot.
  t.setThumbs(false);
  t.setRunner(640.0f, 400.0f, true);
  t.touch(6, 800.0f, 150.0f, ms + 4000);
  t.advance(ms + 4060);
  in = t.read();
  check(in.fire && in.up, "aim (buttons): a tap above shoots up");
  t.release(6);
  t.read();
  t.touch(7, 800.0f, 300.0f, ms + 5000);
  t.slide(7, 800.0f, 400.0f, ms + 5030);
  t.advance(ms + 5100);
  in = t.read();
  check(in.swap && !in.fire, "aim (buttons): a quick swipe down still switches runner, no shot");
  t.release(7);
  check(!t.read().fire, "aim (buttons): ...not even on the lift");
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

// Both layouts drawn, the surface's pixels once they reach it.
std::vector<Uint32> overlayPixels(TouchControls& t, Renderer& r, SDL_Surface* surface)
{
  std::vector<Uint32> px;
  for (Layout l : {Layout::Menu, Layout::Play})
  {
    t.configure(l, 1);
    r.clear(rgb(0, 0, 0));
    t.draw(r);
    SDL_RenderPresent(r.sdl());
    for (int y = 0; y < surface->h; ++y)
    {
      const auto* bytes = static_cast<const unsigned char*>(surface->pixels) + y * surface->pitch;
      const auto* row = reinterpret_cast<const Uint32*>(bytes);
      px.insert(px.end(), row, row + surface->w);
    }
  }
  return px;
}

// bakeTextures (the window code's answer to SDL_RENDER_DEVICE_RESET) makes
// the overlay's textures again, and it looks as it did.
void rebaked(Renderer& r, SDL_Surface* surface)
{
  TouchControls t(r, true);
  const std::vector<Uint32> before = overlayPixels(t, r, surface);
  t.bakeTextures(r);
  const std::vector<Uint32> after = overlayPixels(t, r, surface);
  bool lit = false;
  for (Uint32 p : before)
    lit = lit || (p & 0xFFFFFFu) != 0;
  check(lit && after == before, "the overlay baked again draws as before");
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
    directTouch(r);
    playLayout(r);
    sizes(r);
    forgiving(r);
    letterbox(r);
    swipes(r);
    thumbs(r);
    tapToAim(r);
    visibility(r);
    rebaked(r, surface);
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
