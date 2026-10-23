// Direct touch in the menus, through the whole game: taps on the title
// screen, WORLD SELECT, runner select, the runner editor's name keyboard, the CONTROLS
// menu, the pause menu and the savegame slots, a drag down the pause menu,
// and the map. Positions are the 1280x720 picture's, as the screens draw
// themselves for a fresh profile.

#include "frontend/game.hpp"

#include <SDL.h>

#include <cstdio>
#include <filesystem>
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

// Ticks and draws like the game's loop (taps hit what was drawn last).
void run(Game& g, int ticks, Input in = {})
{
  for (int i = 0; i < ticks; ++i)
    g.tick(in);
  g.render();
}

void tap(Game& g, float x, float y)
{
  g.tapAt(x, y);
  run(g, 2);
}

Input pressed(bool Input::*button)
{
  Input in;
  in.*button = true;
  return in;
}

// The title screen's items for a fresh profile, 64 apart from NEW GAME.
float titleItem(int i)
{
  return 252.0f + 64.0f * float(i);
}

// CONTROLS, the second row of SETTINGS (DIFFICULTY, CONTROLS, ASSISTS, PLAY
// OPTIONS, BACK), at its middle.
constexpr float kSettingsControls = 232.0f + 64.0f + 15.0f;

} // namespace

int main()
{
  SDL_Init(0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  const std::string saves = "menu_taps_test_saves";
  std::filesystem::remove_all(saves);
  {
    Renderer renderer(sdl);
    GameOptions o;
    o.saveDir = saves;
    o.touch = true;
    o.noCutscenes = true;
    Game g(o, renderer, nullptr);
    run(g, 30);

    // Large title rows have pages; the disabled first-page PREV does nothing.
    tap(g, 100.0f, 340.0f);
    tap(g, 1180.0f, 340.0f); // NEXT
    tap(g, 100.0f, 340.0f); // PREV

    // WORLD SELECT: a locked episode stays shut, DEEP SPACE lists its first
    // level, BACK goes up a screen at a time.
    tap(g, 640.0f, titleItem(1));
    run(g, 20);
    check(g.worldSelect(), "world select: opens from the title screen");
    tap(g, 490.0f, 240.0f); // Episode 2, locked on a fresh profile
    check(g.worldSelect(), "world select: a locked episode does not open");
    tap(g, 790.0f, 500.0f); // DEEP SPACE
    run(g, 20);
    check(!g.worldSelect(), "world select: DEEP SPACE opens its levels");
    tap(g, 640.0f, 140.0f); // Level 43
    run(g, 20);
    run(g, 2, pressed(&Input::back));
    run(g, 2);
    check(!g.worldSelect(), "runner select backs out to the episode's levels");
    run(g, 2, pressed(&Input::back));
    run(g, 2);
    check(g.worldSelect(), "list: BACK goes back to world select");
    run(g, 20);
    run(g, 2, pressed(&Input::back));
    run(g, 20);
    check(!g.worldSelect(), "world select: BACK goes to the title screen");

    // SETTINGS, then CONTROLS: one tap picks a cell, a second listens, any
    // tap gives up.
    tap(g, 640.0f, titleItem(5));
    tap(g, 640.0f, kSettingsControls);
    tap(g, 770.0f, 205.0f);
    check(!g.listening(), "controls: a first tap on a cell only picks it");
    tap(g, 770.0f, 205.0f);
    check(g.listening(), "controls: a second tap listens for a key");
    tap(g, 300.0f, 650.0f);
    check(!g.listening(), "controls: a tap while listening gives up");
    // Every touch setting remains reachable through large paged rows.
    tap(g, 1180.0f, 340.0f);
    tap(g, 1180.0f, 340.0f);
    tap(g, 640.0f, 196.0f + 3 * 64.0f + 40.0f);
    check(!g.touchThumbs(), "controls: a tap near the bottom of the scheme row switches scheme");
    tap(g, 640.0f, 196.0f + 3 * 64.0f + 40.0f);
    check(g.touchThumbs(), "controls: the scheme switches back");
    tap(g, 640.0f, 196.0f + 4 * 64.0f + 40.0f);
    check(g.touchSize() == 2, "controls: TOUCH PAD size is reachable on page three");
    tap(g, 640.0f, 196.0f + 5 * 64.0f + 40.0f);
    check(g.touchEditing(), "controls: EDIT TOUCH LAYOUT opens from page three");
    run(g, 2, pressed(&Input::back));
    run(g, 2);
    tap(g, 1180.0f, 340.0f);
    tap(g, 1180.0f, 340.0f); // disabled NEXT on the last page
    tap(g, 640.0f, 230.0f); // RESET TO DEFAULTS
    check(g.touchSize() == 1 && g.touchThumbs(), "controls: last-page reset restores touch defaults");
    for (int i = 0; i < 3; ++i)
      tap(g, 100.0f, 340.0f);
    run(g, 2, pressed(&Input::back)); // to SETTINGS
    run(g, 2);
    // ASSISTS and PLAY OPTIONS keep their last option and BACK reachable.
    tap(g, 640.0f, 232.0f + 2 * 64.0f + 15.0f);
    tap(g, 1180.0f, 340.0f);
    tap(g, 640.0f, 230.0f); // SWAP RUNNER ON DEATH, selected by paging
    check(Profile::load(saves).controls["opt.swapdeath"] == std::vector<int>{1},
      "assists: last-page option changes the saved preference");
    tap(g, 640.0f, 230.0f); // restore it
    tap(g, 640.0f, 310.0f);
    tap(g, 640.0f, 310.0f); // BACK to settings
    tap(g, 640.0f, 232.0f + 3 * 64.0f + 15.0f);
    tap(g, 640.0f, 230.0f); // HOLD TO FIRE
    check(Profile::load(saves).controls["opt.holdfire"] == std::vector<int>{1},
      "play options: large first row changes the saved preference");
    tap(g, 640.0f, 230.0f); // restore it
    tap(g, 1180.0f, 340.0f);
    tap(g, 640.0f, 500.0f); // blank space cannot activate an option
    check(Profile::load(saves).controls["opt.holdfire"] == std::vector<int>{0},
      "play options: blank last-page rows do nothing");
    tap(g, 640.0f, 230.0f); // BACK to settings
    run(g, 2, pressed(&Input::back)); // to the title screen
    run(g, 2);

    // NEW GAME, then REMIX under the card, then the name keyboard.
    g.tapAt(640.0f, titleItem(0));
    for (int i = 0; i < 25; ++i)
      g.tick({});
    // Runner select has not been drawn yet: a tap meant for the title
    // screen must not reach it (as OK, it would start the level).
    g.tapAt(640.0f, titleItem(1));
    run(g, 5);
    check(!g.inLevel(), "a tap on a screen that has gone is ignored");
    tap(g, 640.0f, 655.0f);
    run(g, 12);
    check(!g.wantsText(), "editor: open, not naming yet");
    tap(g, 700.0f, 108.0f);
    check(g.wantsText(), "editor: a tap on NAME opens the keyboard");
    tap(g, 300.0f + 8.5f * 68.0f, 280.0f + 4.5f * 64.0f);
    check(!g.wantsText(), "editor: a tap on DONE closes it");
    run(g, 2, pressed(&Input::back));
    run(g, 25);
    check(!g.inLevel(), "back on runner select");

    // A tap on the chosen card runs.
    tap(g, 640.0f, 400.0f);
    run(g, 5);
    check(g.inLevel(), "select: a tap on the chosen card starts the level");

    // Pause, SAVE GAME, then slot 2: picked by one tap, saved by a second.
    run(g, 60);
    run(g, 2, pressed(&Input::pause));
    run(g, 2);
    check(!g.inLevel(), "pause menu open");
    tap(g, 640.0f, 206.0f + 64.0f + 15.0f);
    tap(g, 640.0f, 296.0f);
    check(!std::filesystem::exists(saves + "/slot2.sav"), "slots: one tap only picks a slot to save over");
    tap(g, 640.0f, 296.0f);
    check(std::filesystem::exists(saves + "/slot2.sav"), "slots: a second tap saves there");
    check(g.inLevel(), "slots: saving goes back to the level");

    // A drag down the pause menu: six rows to SETTINGS, then OK, then CONTROLS.
    run(g, 2, pressed(&Input::pause));
    run(g, 2);
    g.dragBy(0.0f, -46.0f * 6.0f);
    run(g, 8);
    run(g, 2, pressed(&Input::confirm));
    run(g, 2);
    tap(g, 640.0f, kSettingsControls);
    tap(g, 600.0f, 205.0f);
    tap(g, 600.0f, 205.0f);
    check(g.listening(), "pause: a drag up six rows reaches SETTINGS, then CONTROLS");
    tap(g, 300.0f, 650.0f);
    run(g, 2, pressed(&Input::back)); // to SETTINGS
    run(g, 2);
    run(g, 2, pressed(&Input::back)); // to the pause menu
    run(g, 2);
    tap(g, 100.0f, 340.0f); // PREV returns to the first page
    tap(g, 640.0f, 206.0f + 15.0f);
    check(g.inLevel(), "pause: a tap on RESUME goes back to the level");

    // The map: a drag pans, a tap zooms, BACK closes it.
    run(g, 2, pressed(&Input::map));
    run(g, 2);
    check(!g.inLevel(), "map open");
    g.dragBy(-300.0f, 40.0f);
    tap(g, 640.0f, 360.0f);
    run(g, 2, pressed(&Input::back));
    run(g, 2);
    check(g.inLevel(), "map: BACK closes it");

    // RANDOM RUNNER: the hotkey always lands on another runner, and over a
    // dozen presses on more than two of them.
    {
      bool alwaysNew = true;
      bool seen[64] = {};
      int distinct = 0;
      for (int i = 0; i < 12; ++i)
      {
        const int before = g.world()->characterIndex();
        run(g, 2, pressed(&Input::randomRunner));
        run(g, 2);
        const int after = g.world()->characterIndex();
        alwaysNew = alwaysNew && after != before;
        if (after >= 0 && after < 64 && !seen[after])
        {
          seen[after] = true;
          ++distinct;
        }
      }
      check(alwaysNew, "random runner: never the one already playing");
      check(distinct > 2, "random runner: picks among all the runners");
    }

    // ...and on touch, from CHANGE RUNNER in the pause menu.
    {
      const int before = g.world()->characterIndex();
      run(g, 2, pressed(&Input::pause));
      run(g, 2);
      for (int i = 0; i < 5; ++i)
      {
        run(g, 2, pressed(&Input::down));
        run(g, 2);
      }
      run(g, 2, pressed(&Input::confirm));
      run(g, 2);
      check(!g.inLevel(), "change runner menu open");
      tap(g, 640.0f, 514.0f);
      check(g.inLevel(), "change runner: a tap on RANDOM RUNNER goes back to the level");
      check(g.world()->characterIndex() != before, "...as another runner");
    }
  }
  std::filesystem::remove_all(saves);
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  if (gFailures == 0)
    std::puts("all passed");
  return gFailures == 0 ? 0 : 1;
}
