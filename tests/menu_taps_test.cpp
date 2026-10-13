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

// The title screen's items for a fresh profile, 44 apart from NEW GAME.
float titleItem(int i)
{
  return 252.0f + 44.0f * float(i);
}

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

    // CONTROLS: one tap picks a cell, a second listens, any tap gives up.
    tap(g, 640.0f, titleItem(5));
    tap(g, 770.0f, 205.0f);
    check(!g.listening(), "controls: a first tap on a cell only picks it");
    tap(g, 770.0f, 205.0f);
    check(g.listening(), "controls: a second tap listens for a key");
    tap(g, 300.0f, 650.0f);
    check(!g.listening(), "controls: a tap while listening gives up");
    run(g, 2, pressed(&Input::back));
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
    tap(g, 300.0f + 8.5f * 68.0f, 505.0f);
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
    tap(g, 640.0f, 206.0f + 40.5f + 15.0f);
    tap(g, 640.0f, 296.0f);
    check(!std::filesystem::exists(saves + "/slot2.sav"), "slots: one tap only picks a slot to save over");
    tap(g, 640.0f, 296.0f);
    check(std::filesystem::exists(saves + "/slot2.sav"), "slots: a second tap saves there");
    check(g.inLevel(), "slots: saving goes back to the level");

    // A drag down the pause menu: six rows to CONTROLS, then OK.
    run(g, 2, pressed(&Input::pause));
    run(g, 2);
    g.dragBy(0.0f, -46.0f * 6.0f);
    run(g, 8);
    run(g, 2, pressed(&Input::confirm));
    run(g, 2);
    tap(g, 600.0f, 205.0f);
    tap(g, 600.0f, 205.0f);
    check(g.listening(), "pause: a drag up six rows reaches CONTROLS");
    tap(g, 300.0f, 650.0f);
    run(g, 2, pressed(&Input::back));
    run(g, 2);
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
  }
  std::filesystem::remove_all(saves);
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  if (gFailures == 0)
    std::puts("all passed");
  return gFailures == 0 ? 0 : 1;
}
