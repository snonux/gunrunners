// Drives the on-screen gamepad with fingers at known spots and checks the
// Input it produces, in both layouts and at every size. And after
// SDL_RENDER_DEVICE_RESET (recoverRender, in place and with a new renderer)
// its textures made again draw it as before, aim arrows included, none from
// before the reset; left out of the recovery, it is caught drawing those.

#include "frontend/touch_controls.hpp"
#include "frontend/window_renderer.hpp"
#include "game/world.hpp"
#include "render/renderer.hpp"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <memory>
#include <string>
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
void vehicleButton(Renderer& r)
{
  const Theme& theme = themeByIndex(0);
  const Art art = Art::build(theme, r);
  std::string map = "name=TOUCH VEHICLE\n[map]\n";
  for (int y = 0; y < 16; ++y)
  {
    std::string row(64, y == 0 || y >= 14 ? '#' : '.');
    row.front() = row.back() = '#';
    if (y == 13)
    {
      row[2] = 'P';
      row[60] = 'X';
    }
    map += row + "\n";
  }
  map += "[entities]\n@ tank 3 13\n";
  const auto level = std::make_shared<const Level>(Level::parse(map));
  for (bool thumbs : {false, true})
    for (int size = 0; size < 3; ++size)
    {
      World world(level, 0, theme, art);
      TouchControls t(r, true);
      t.configure(Layout::Play, size);
      t.setThumbs(thumbs);
      world.update({});
      t.setVehicle(false, false);
      check(std::string(t.vehicleLabel()) == "RIDE", "vehicle: boarding is labelled RIDE");
      const float scale = 0.8f + float(size) * 0.2f;
      const float x = 1280.0f - (thumbs ? 338.0f : 244.0f) * scale;
      auto use = [&]() {
        t.touch(1, x, 120);
        t.release(1);
        const Input in = t.read();
        check(in.use && !in.fire && !in.jump && !in.swap, "vehicle: labelled button only presses USE");
        PlayerInput p;
        p.use = {in.use, in.use};
        world.update(p);
        world.update({});
      };
      use();
      check(world.riding(), "vehicle: touch boards a real tank at every size and scheme");
      t.setVehicle(true, world.canLeaveVehicle());
      check(std::string(t.vehicleLabel()).empty(), "vehicle: riding leaves the steering wheel without an EXIT label");
      t.draw(r);
      use();
      check(!world.riding(), "vehicle: the same touch button exits the tank");
      t.setVehicle(true, false);
      check(std::string(t.vehicleLabel()).empty(), "vehicle: a blocked exit also keeps the button free of an EXIT label");
      t.draw(r);
      t.configure(Layout::Menu, size);
      t.touch(2, x, 120);
      t.release(2);
      check(!t.read().use, "vehicle: a menu tap cannot board or exit");
    }
}

void largeMenuCorners(Renderer& r)
{
  TouchControls t(r, true);
  t.configure(Layout::Menu, 0);
  t.touch(1, 108, 64);
  t.release(1);
  check(t.read().back && t.takeTaps().empty(), "small setting: BACK still has a large target");
  t.touch(2, 108, 656);
  t.release(2);
  check(t.menuPad() && t.takeTaps().empty(), "small setting: menu pad toggle still has a large target");
}

void navigationTaps(Renderer& r)
{
  const std::vector<SDL_FRect> navigation = {
    {24, 300, 152, 88}, {1104, 300, 152, 88},
    {500, 610, 152, 68}, {680, 610, 152, 68}};
  for (int size = 0; size < 3; ++size)
    for (bool pad : {false, true})
    {
      TouchControls t(r, true);
      t.configure(Layout::Menu, size);
      t.setMenuPad(pad);
      t.setMenuNavigation(navigation);
      for (SDL_FPoint point : {SDL_FPoint{100, 380}, {1180, 380}, {600, 640}, {756, 640}, {820, 640}})
      {
        // SDL finger events, as on Android, including the lower button edges
        // which the large pad used to take as direction/OK/BACK presses.
        SDL_Event event{};
        event.type = SDL_FINGERDOWN;
        event.tfinger.fingerId = 1;
        event.tfinger.x = point.x / float(kScreenW);
        event.tfinger.y = point.y / float(kScreenH);
        t.handleEvent(event, nullptr);
        event.type = SDL_FINGERUP;
        t.handleEvent(event, nullptr);
        const Input in = t.read();
        const auto taps = t.takeTaps();
        check(taps.size() == 1 && std::abs(taps[0].x - point.x) < 0.01f &&
          std::abs(taps[0].y - point.y) < 0.01f,
          "navigation: a real finger event taps the visible button at every size/pad setting");
        check(!in.left && !in.right && !in.up && !in.down && !in.confirm && !in.back,
          "navigation: the optional pad cannot steal the button tap");
      }
      t.touch(2, 988, 628);
      t.release(2);
      const Input back = t.read();
      const auto backTaps = t.takeTaps();
      check(pad ? back.back && backTaps.empty() : !back.back && backTaps.size() == 1,
        "navigation: editor page buttons leave the menu pad BACK usable");
      t.setMenuNavigation({});
      t.touch(2, 988, 628);
      t.release(2);
      check(pad ? t.read().back : t.takeTaps().size() == 1,
        "navigation: clearing targets restores the usual menu pad behaviour");
      t.read();
      t.takeTaps();
      t.setMenuNavigation(navigation);
      t.setThumbs(false);
      t.configure(Layout::Play, size);
      t.touch(3, kPlayFireX, kPlayFireY);
      t.release(3);
      check(t.read().fire && t.takeTaps().empty(), "navigation: menu targets cannot intercept play controls");
    }
}

void vehicleDoubleTaps(Renderer& r)
{
  for (bool thumbs : {false, true})
  {
    TouchControls t(r, true);
    t.configure(Layout::Play, 1);
    t.setThumbs(thumbs);
    t.setVehicle(true, true);
    t.setVehicleTarget({600, 300, 240, 160}, 0);
    auto tap = [&](float x, float y, Uint32 time) {
      t.touch(1, x, y, time);
      t.release(1);
      return t.read();
    };
    check(!tap(700, 360, 0).use && !tap(700, 360, 800).use,
      "vehicle: slow taps do not exit");
    check(!tap(900, 300, 1000).use && !tap(900, 300, 1200).use,
      "vehicle: double-tapping outside the vehicle does not exit");
    t.touch(1, 700, 360, 2000);
    t.slide(1, 760, 360, 2050);
    t.release(1);
    t.read();
    check(!tap(700, 360, 2200).use, "vehicle: a steering/aim drag is not the first tap of a double-tap");
    t.touch(1, 700, 360, 3000);
    t.advance(3400);
    t.release(1);
    t.read();
    check(!tap(700, 360, 3500).use, "vehicle: holding to shoot is not a double-tap");
    tap(700, 360, 4000);
    tap(900, 300, 4100);
    check(!tap(700, 360, 4200).use, "vehicle: another touch between the taps cancels the gesture");
    tap(700, 320, 5000);
    check(!tap(820, 440, 5200).use, "vehicle: taps far apart on the body do not exit");
    tap(700, 360, 6000);
    t.setVehicleTarget({600, 300, 240, 160}, 1);
    check(!tap(700, 360, 6200).use, "vehicle: changing vehicle cancels the first tap");
    tap(700, 360, 7000);
    t.configure(Layout::Menu, 1);
    check(!tap(700, 360, 7200).use, "vehicle: menu taps never complete an exit gesture");
    t.takeTaps();
    t.configure(Layout::Play, 1);
    check(!tap(700, 360, 7400).use, "vehicle: closing a menu starts with no pending double-tap");
    tap(700, 360, 8000);
    const Input exit = tap(708, 366, 8200);
    check(exit.use && !exit.fire && !exit.jump && !exit.swap,
      "vehicle: two nearby taps request one exit without firing/jumping on the second");
    check(!t.read().use, "vehicle: the double-tap exit is one press");
    t.touch(1, 700, 360, 8500);
    t.setVehicleTarget({600, 300, 240, 160}, 2);
    t.release(1);
    t.read();
    check(!tap(700, 360, 8700).use, "vehicle: changing vehicle while a finger is down cancels its tap");
    t.setVehicle(false, false);
    t.setVehicleTarget({}, -1);
    check(!tap(700, 360, 9000).use && !tap(700, 360, 9200).use,
      "vehicle: double-tapping on foot cannot board a parked vehicle");
    t.setVehicle(true, true);
    t.setVehicleTarget({1100, 520, 140, 150}, 0);
    t.setThumbs(true);
    check(!tap(kPlayJumpX, kPlayJumpY, 10000).use && !tap(kPlayJumpX, kPlayJumpY, 10200).use,
      "vehicle: the visible fire mark is always a fire control, not a vehicle double-tap");
    t.setThumbs(thumbs);
    t.setVehicleTarget({600, 300, 240, 160}, 3);
    SDL_Event event{};
    event.type = SDL_FINGERDOWN;
    event.tfinger.fingerId = 1;
    event.tfinger.timestamp = 11000;
    event.tfinger.x = 700.0f / kScreenW;
    event.tfinger.y = 360.0f / kScreenH;
    t.handleEvent(event, nullptr);
    event.type = SDL_FINGERUP;
    event.tfinger.timestamp = 11100;
    event.tfinger.x = 800.0f / kScreenW;
    t.handleEvent(event, nullptr);
    t.read();
    check(!tap(700, 360, 11200).use, "vehicle: movement reported only on lift still cancels a tap");
  }
}

void navigationDrift(Renderer& r)
{
  TouchControls t(r, true);
  t.configure(Layout::Menu, 2);
  t.setMenuPad(true);
  t.setMenuNavigation({{24, 300, 152, 88}, {1104, 300, 152, 88}});
  t.touch(1, 100, 340);
  check(t.takeTaps().size() == 1, "navigation: button responds on press, before lifting");
  t.slide(1, 130, 360, 100); // more than the list's 18-unit tap slop
  t.slide(1, 1180, 350, 200); // even crossing to NEXT must not press it
  t.slide(1, 988, 628, 300); // nor the menu pad's BACK
  t.release(1);
  const SDL_FPoint drag = t.takeDrag();
  const Input in = t.read();
  check(t.takeTaps().empty(), "navigation: drift, holding and release never repeat a press");
  check(drag.x == 0 && drag.y == 0 && !in.back && !in.confirm && !in.up && !in.down,
    "navigation: a held button cannot become scrolling or a pad press");
  t.touch(2, 640, 260);
  t.slide(2, 640, 180);
  t.release(2);
  check(t.takeTaps().empty() && t.takeDrag().y == -80,
    "navigation: dragging the list still scrolls instead of clicking");
}

// Test the same native SDL event-watch path as Android, including its
// logical-size conversion. Direct calls to handleEvent cannot exercise it.
void queuedNavigation()
{
  SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
  if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
  {
    check(false, "queued touch: dummy video initializes");
    return;
  }
  for (SDL_Point screen : {SDL_Point{1280, 720}, {1600, 720}, {2400, 1080}, {1280, 960}})
  {
    SDL_Window* window = SDL_CreateWindow("touch test", 0, 0, screen.x, screen.y, SDL_WINDOW_HIDDEN);
    auto windowRenderer = window ? std::make_unique<WindowRenderer>(window) : nullptr;
    SDL_Renderer* sdl = windowRenderer ? windowRenderer->sdl() : nullptr;
    if (!sdl)
    {
      check(false, "queued touch: window renderer created");
      windowRenderer.reset();
      if (window)
        SDL_DestroyWindow(window);
      continue;
    }
    {
      Renderer renderer(sdl);
      const float scale = std::min(float(screen.x) / kScreenW, float(screen.y) / kScreenH);
      const float barX = (float(screen.x) - kScreenW * scale) * 0.5f;
      const float barY = (float(screen.y) - kScreenH * scale) * 0.5f;
      auto push = [&](Uint32 type, float x, float y) {
        SDL_Event event{};
        event.type = type;
        event.tfinger.touchId = 1;
        event.tfinger.fingerId = 1;
        event.tfinger.windowID = SDL_GetWindowID(window);
        event.tfinger.x = (barX + x * scale) / float(screen.x);
        event.tfinger.y = (barY + y * scale) / float(screen.y);
        check(SDL_PushEvent(&event) == 1, "queued touch: event enters SDL's queue");
      };
      for (int size = 0; size < 3; ++size)
        for (bool pad : {false, true})
        {
          TouchControls touch(renderer, true);
          touch.configure(Layout::Menu, size);
          // Deliberately leave the initial 16:9 bounds: the first queued
          // event must learn this window's actual output size itself.
          touch.setMenuPad(pad);
          touch.setMenuNavigation({{24, 300, 152, 88}, {1104, 300, 152, 88}});
          for (SDL_FPoint point : {SDL_FPoint{100, 340}, {1180, 340}})
          {
            push(SDL_FINGERDOWN, point.x, point.y);
            push(SDL_FINGERMOTION, point.x + 25, point.y + 20);
            push(SDL_FINGERUP, point.x + 25, point.y + 20);
            int taps = 0;
            SDL_Event event;
            while (windowRenderer->pollEvent(event))
            {
              if (event.type == SDL_FINGERDOWN)
                check(std::abs(event.tfinger.x - (barX + point.x * scale) / screen.x) < 0.0001f &&
                  std::abs(event.tfinger.y - (barY + point.y * scale) / screen.y) < 0.0001f,
                  "queued touch: WindowRenderer preserves physical finger coordinates");
              touch.handleEvent(event, window);
              for (const auto& tap : touch.takeTaps())
              {
                ++taps;
                check(std::abs(tap.x - point.x) < 0.1f && std::abs(tap.y - point.y) < 0.1f,
                  "queued touch: the physical button maps to the drawn button at every aspect ratio");
              }
            }
            const SDL_FPoint drag = touch.takeDrag();
            const Input in = touch.read();
            check(taps == 1 && drag.x == 0 && drag.y == 0 && !in.confirm && !in.back,
              "queued touch: jitter gives one navigation press, no swipe or overlay action");
          }
          // The original coordinates are also needed for BACK in the bars:
          // SDL2 clamps those touches to the picture's edge before polling.
          touch.setMenuPad(false);
          const float corner = 64.0f * std::max(1.0f, 0.8f + float(size) * 0.2f);
          const float x = -barX / scale + corner, y = -barY / scale + corner;
          push(SDL_FINGERDOWN, x, y);
          push(SDL_FINGERUP, x, y);
          SDL_Event event;
          while (windowRenderer->pollEvent(event))
          {
            touch.handleEvent(event, window);
          }
          check(touch.read().back && touch.takeTaps().empty(),
            "queued touch: BACK in the letterbox bars keeps its physical touch position");
          touch.configure(Layout::Play, size);
          touch.setThumbs(pad);
          touch.setVehicle(true, true);
          touch.setVehicleTarget({600, 300, 240, 160}, 0);
          auto poll = [&] {
            SDL_Event input;
            while (windowRenderer->pollEvent(input))
            {
              touch.handleEvent(input, window);
            }
            return touch.read();
          };
          push(SDL_FINGERDOWN, 700, 360);
          push(SDL_FINGERUP, 700, 360);
          check(!poll().use, "queued touch: one tap on the vehicle never exits");
          push(SDL_FINGERDOWN, 708, 366);
          push(SDL_FINGERUP, 708, 366);
          check(poll().use, "queued touch: double-tap exits in both control schemes and every screen shape");
        }
    }
    windowRenderer.reset();
    SDL_DestroyWindow(window);
  }
}

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
  check(in.fire && in.aimUp && !in.jump && t.aim() == TouchControls::kAimUp, "thumbs: a flick up fires upwards");
  t.advance(3500);
  in = t.read();
  check(in.fire && in.aimUp, "thumbs: ...and keeps aiming up");
  t.slide(4, 900.0f, 420.0f, 3600);
  t.slide(4, 820.0f, 420.0f, 3650);
  in = t.read();
  check(!in.fire && t.aim() == TouchControls::kAimLeft, "thumbs: a flick left lets go of fire for a tick");
  in = t.read();
  check(in.fire && in.aimLeft && !in.aimUp && !in.left, "thumbs: ...then fires left");
  t.release(4);

  t.touch(5, 1000.0f, 300.0f, 5000);
  t.advance(5060);
  in = t.read();
  check(in.fire && !in.aimLeft && !in.aimUp && !in.aim.on,
    "thumbs: each touch aims afresh (ahead, with no runner to aim from)");
  t.release(5);
  t.touch(6, 1000.0f, 300.0f, 6000);
  t.slide(6, 1000.0f, 380.0f, 6040);
  in = t.read();
  check(in.fire && in.aimDown && t.aim() == TouchControls::kAimDown, "thumbs: a flick down fires low");
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

// Tap to aim: a touch shoots exactly at the spot it lands on (Input::aim,
// measured from the runner), and snapped to the shots a runner has for the
// guns that only shoot up, ahead or low.
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
  check(in.fire && in.aimUp && !in.up && !in.jump, "aim: a shot up is aimed up (the game looks up), not a jump");
  t.release(2);
  t.read();
  t.touch(3, 1000.0f, 330.0f, ms + 1000);
  t.advance(ms + 1060);
  in = t.read();
  check(in.fire && in.aimRight && !in.right, "aim: a shot right turns the runner, it does not walk");
  t.release(3);
  t.read();
  // Exact: the spot from the runner's middle, which follows the finger and
  // the runner.
  t.touch(3, 900.0f, 250.0f, ms + 1500);
  t.advance(ms + 1560);
  in = t.read();
  check(in.fire && in.aim.on && in.aim.spot && in.aim.x == 260.0f && in.aim.y == -150.0f && in.aimRight && !in.aimUp,
    "aim: a touch aims exactly at its spot");
  t.slide(3, 880.0f, 260.0f, ms + 1700);
  t.slide(3, 500.0f, 300.0f, ms + 1900);
  in = t.read();
  check(in.fire && in.aim.on && in.aim.x == -140.0f && in.aim.y == -100.0f && in.aimLeft,
    "aim: sliding the finger keeps aiming at it");
  t.setRunner(700.0f, 400.0f, true);
  in = t.read();
  check(in.aim.on && in.aim.x == -200.0f && in.aim.y == -100.0f, "aim: ...from wherever the runner is now");
  t.setRunner(640.0f, 400.0f, true);
  t.release(3);
  t.read();
  t.touch(3, 1000.0f, 330.0f, ms + 1950);
  t.release(3);
  in = t.read();
  check(in.fire && in.aim.on && in.aim.x == 360.0f, "aim: a quick tap still aims exactly");
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
  in = t.read();
  check(in.fire && t.aim() == TouchControls::kAimAhead && !in.aim.on, "aim: the fire mark shoots ahead");
  t.release(6);
  t.read();

  // The buttons scheme aims taps on empty spots the same way; a swipe from
  // one is still a swipe, not a shot.
  t.setThumbs(false);
  t.setRunner(640.0f, 400.0f, true);
  t.touch(6, 800.0f, 150.0f, ms + 4000);
  t.advance(ms + 4060);
  in = t.read();
  check(in.fire && in.aimUp && in.aim.on && in.aim.spot && in.aim.x == 160.0f && in.aim.y == -250.0f,
    "aim (buttons): a tap above shoots up, exactly at it");
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

// The surface's pixels once the draws reach it, added to px.
void grabPixels(Renderer& r, SDL_Surface* surface, std::vector<Uint32>& px)
{
  SDL_RenderPresent(r.sdl());
  for (int y = 0; y < surface->h; ++y)
  {
    const auto* bytes = static_cast<const unsigned char*>(surface->pixels) + y * surface->pitch;
    const auto* row = reinterpret_cast<const Uint32*>(bytes);
    px.insert(px.end(), row, row + surface->w);
  }
}

// Both layouts drawn, then in a level a shooting thumb on the fire mark
// flicked up, down, left and right, which shows all four aim arrows.
std::vector<Uint32> overlayPixels(TouchControls& t, Renderer& r, SDL_Surface* surface)
{
  std::vector<Uint32> px;
  for (Layout l : {Layout::Menu, Layout::Play})
  {
    t.configure(l, 1);
    r.clear(rgb(0, 0, 0));
    t.draw(r);
    grabPixels(r, surface, px);
  }
  t.setRunner(640.0f, 400.0f, true);
  Uint32 ms = 0;
  for (const SDL_FPoint by : {SDL_FPoint{0.0f, -90.0f}, SDL_FPoint{0.0f, 90.0f}, SDL_FPoint{-90.0f, 0.0f},
         SDL_FPoint{90.0f, 0.0f}})
  {
    t.touch(1, 1160.0f, 580.0f, ms);
    t.slide(1, 1160.0f + by.x, 580.0f + by.y, ms + 20);
    t.advance(ms + 60);
    t.read();
    r.clear(rgb(0, 0, 0));
    t.draw(r);
    grabPixels(r, surface, px);
    t.release(1);
    t.read();
    ms += 1000;
  }
  return px;
}

// recoverRender with the overlay (the window code's answer to
// SDL_RENDER_DEVICE_RESET): its textures made again in place, and with a
// new SDL_Renderer (on the same surface, as the window code makes one
// again), draw it as before, none of them from before the reset. Left out
// of a recovery, it is caught drawing textures from before it.
void rebaked()
{
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  {
    Renderer r(sdl);
    TouchControls t(r, true);
    overlayPixels(t, r, surface); // the first touch in a level hides its hint for good
    const std::vector<Uint32> before = overlayPixels(t, r, surface);
    bool lit = false;
    for (Uint32 p : before)
      lit = lit || (p & 0xFFFFFFu) != 0;
    check(lit && overlayPixels(t, r, surface) == before, "the overlay draws the same twice");

    recoverRender(r, ResetAction::InPlace, TextureOwners{}, nullptr);
    overlayPixels(t, r, surface);
    check(r.staleDraws() > 0, "an overlay left out of a recovery draws textures from before it");

    const TextureOwners owners{[&t] { t.dropTextures(); }, [&t, &r] { t.bakeTextures(r); }};
    const auto remake = [&sdl, surface] {
      SDL_DestroyRenderer(sdl);
      sdl = SDL_CreateSoftwareRenderer(surface);
      return sdl;
    };
    for (const ResetAction action : {ResetAction::InPlace, ResetAction::Recreate})
    {
      const std::size_t stale = r.staleDraws();
      recoverRender(r, action, owners, remake);
      const std::vector<Uint32> after = overlayPixels(t, r, surface);
      const bool inPlace = action == ResetAction::InPlace;
      check(r.staleDraws() == stale && r.sdl() == sdl,
        inPlace ? "made again in place, no texture from before is drawn" : "...and with a new renderer");
      check(after == before,
        inPlace ? "the overlay made again in place draws as before" : "...and with a new renderer");
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
}

} // namespace

int main()
{
  SDL_Init(0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  {
    Renderer r(sdl);
    vehicleButton(r);
    largeMenuCorners(r);
    navigationTaps(r);
    navigationDrift(r);
    vehicleDoubleTaps(r);
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
  rebaked();
  queuedNavigation();
  SDL_Quit();
  std::printf("%s\n", gFailures ? "FAILED" : "all passed");
  return gFailures ? 1 : 0;
}
