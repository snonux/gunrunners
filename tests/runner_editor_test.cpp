// The runner editor without a screen: rows, the stat budget, the name
// keyboard, and save / delete.

#include "frontend/runner_editor.hpp"

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

using Result = RunnerEditor::Result;

Result press(RunnerEditor& e, bool ok, bool cancel = false, int dir = 0, int side = 0)
{
  return e.tick(ok, cancel, dir, side);
}

void down(RunnerEditor& e, int rows)
{
  for (int i = 0; i < rows; ++i)
    press(e, false, false, 1);
}

} // namespace

int main()
{
  SDL_Init(0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  {
    Renderer renderer(sdl);
    const Theme& theme = themeByIndex(0);
    const Art art = Art::build(theme, renderer);

    // Remixing Rocco: a new runner with his look, trimmed to the budget.
    RunnerEditor e(renderer, remixRunner(characterByIndex(1)), true);
    CharacterDef d = e.runner();
    check(d.custom && d.id != "rocco" && d.id[0] == 'c', "a remix is a new custom runner");
    check(runnerPointsLeft(d.healthPips, d.jumpPips, d.powerPips) >= 0, "a remix keeps to the budget");
    check(d.parts == characterByIndex(1).parts && d.startWeapon == Weapon::Rocket, "a remix keeps the look and gun");

    // Name: open the keyboard, clear it, type with the grid and for real.
    check(press(e, true) == Result::None && e.naming(), "OK on NAME opens the keyboard");
    for (int i = 0; i < 12; ++i)
      e.backspace();
    press(e, false, false, -1);    // DONE -> up to the digits row
    press(e, false, false, -1, 0); // up to U..'
    press(e, false, false, -1);    // K..T
    press(e, false, false, -1);    // A..J, column 7: H
    press(e, true);
    e.typeText("i there, robot!");
    check(e.runner().name == "HI THERE, R" || e.runner().name == "HI THERE R", "typed name is upper case and short");
    check(int(e.runner().name.size()) <= kMaxRunnerName, "name length is capped");
    press(e, false, true);
    check(!e.naming(), "back closes the keyboard");

    // Body: a robot gets robot heads and optics.
    down(e, 1);
    press(e, false, false, 0, 1);
    check(e.runner().parts.body == 1, "BODY switches to a robot");
    check(e.runner().parts.hair < hairStyleCount(1) && e.runner().parts.face < faceCount(1), "robot parts in range");

    // VOICE: a remix starts with the voice it was made from, and steps
    // through the six.
    down(e, 13); // Voice
    check(runnerVoice(e.runner()) == "rocco", "a remix of Rocco sounds like Rocco");
    press(e, false, false, 0, 1);
    check(runnerVoice(e.runner()) == "nova", "VOICE steps to the next voice");

    // Stats: HEALTH up to the limit of the budget, never over it.
    down(e, 1); // Health
    for (int i = 0; i < 6; ++i)
      press(e, false, false, 0, 1);
    d = e.runner();
    check(runnerPointsLeft(d.healthPips, d.jumpPips, d.powerPips) >= 0 && d.healthPips <= kMaxPips,
      "HEALTH stops at the budget");
    down(e, 1); // Jump
    for (int i = 0; i < 6; ++i)
      press(e, false, false, 0, -1);
    check(e.runner().jumpPips == kMinPips[1], "JUMP stops at its minimum");

    // RANDOMIZE spends the whole budget.
    down(e, 2);
    press(e, true);
    d = e.runner();
    check(runnerPointsLeft(d.healthPips, d.jumpPips, d.powerPips) == 0, "RANDOMIZE spends every point");

    // SAVE, and no DELETE on a new runner.
    down(e, 1);
    check(press(e, true) == Result::Save, "SAVE RUNNER saves");
    down(e, 1);
    check(press(e, true) != Result::Delete, "a new runner has no DELETE");

    // Editing a saved one: DELETE asks twice.
    const int index = putCustomRunner(e.runner());
    RunnerEditor edit(renderer, characterByIndex(index), false);
    check(edit.runner().id == characterByIndex(index).id, "editing keeps the id");
    press(edit, false, false, -1); // wraps to the last row: DELETE
    check(press(edit, true) == Result::None, "DELETE asks first");
    check(press(edit, true) == Result::Delete, "DELETE again deletes");
    check(press(edit, false, true) == Result::Cancel, "back leaves without saving");

    // Touch rows have pages, with a larger keyboard and no accidental action
    // when paging past either end. SAVE and DELETE stay on the last page.
    RunnerEditor touch(renderer, characterByIndex(index), false);
    touch.setTouchHints(true);
    check(!touch.tapAt(576, 640).ok, "touch: disabled PREV does not act");
    auto name = touch.tapAt(700, 140);
    press(touch, name.ok);
    check(touch.naming(), "touch: a tap low in the NAME row opens naming");
    check(!touch.tapAt(980, 600).ok, "touch: outside the keyboard does not type");
    auto done = touch.tapAt(878, 590);
    press(touch, done.ok);
    check(!touch.naming(), "touch: enlarged DONE key closes naming");
    touch.tapAt(756, 640);
    touch.tapAt(756, 640);
    check(!touch.tapAt(756, 640).ok, "touch: disabled NEXT on the last page does not act");
    check(!touch.tapAt(700, 600).ok, "touch: blank rows do not activate a hidden action");
    auto save = touch.tapAt(700, 95 + 3 * 64 + 40);
    check(press(touch, save.ok) == Result::Save, "touch: SAVE is reachable on the last page");
    auto del = touch.tapAt(700, 95 + 4 * 64 + 40);
    check(press(touch, del.ok) == Result::None, "touch: DELETE asks before deleting");
    touch.tapAt(576, 640); // paging away cancels the confirmation
    touch.tapAt(756, 640);
    del = touch.tapAt(700, 95 + 4 * 64 + 40);
    check(press(touch, del.ok) == Result::None, "touch: paging cancels DELETE confirmation");
    check(press(touch, del.ok) == Result::Delete, "touch: a second DELETE confirms");

    // The preview draws.
    renderer.beginFrame();
    edit.render(renderer, art, theme, 30);
    check(true, "renders");
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  if (gFailures == 0)
    std::printf("all runner editor tests passed\n");
  return gFailures == 0 ? 0 : 1;
}
