// Double jump, played by hand in an open room: every runner gets one more
// jump in mid-air, as high as its JUMP pips allow (Rocco and Bolt 3 cells
// up to Nova's 6), only one per flight, back after landing; and a running
// jump is always a somersault.

#include "assets/art.hpp"
#include "data/characters.hpp"
#include "data/level.hpp"
#include "data/theme.hpp"
#include "game/world.hpp"
#include "render/renderer.hpp"

#include <SDL.h>

#include <algorithm>
#include <cstdio>
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

// A 60 x 24 block room with a floor on block rows 22 and 23 and the runner
// at block (3, 21): the feet on cell row 43, nothing overhead.
std::string room()
{
  const int h = 24, w = 60;
  std::vector<std::string> map(std::size_t(h), std::string(std::size_t(w), '.'));
  for (int y = 0; y < h; ++y)
    map[std::size_t(y)][0] = map[std::size_t(y)][1] = map[std::size_t(y)][w - 2] = map[std::size_t(y)][w - 1] = '#';
  for (int x = 0; x < w; ++x)
    map[std::size_t(h - 2)][std::size_t(x)] = map[std::size_t(h - 1)][std::size_t(x)] = '#';
  map[21][3] = 'P';
  std::string s = "name=DOUBLE JUMP TEST\nepisode=1\ntheme=neon_rooftops\nweapon=pulse_pistol\n[map]\n";
  for (const auto& row : map)
    s += row + "\n";
  s += "[entities]\n";
  return s;
}

// One logic frame: jump held or not (a press is the frame it goes down).
void step(World& w, bool jump, bool& held, bool right = false, bool run = false)
{
  PlayerInput in;
  in.jump.pressed = jump;
  in.jump.triggered = jump && !held;
  in.right = right;
  in.run = run;
  held = jump;
  w.update(in);
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
    auto make = [&](int character) {
      World w(std::make_shared<const Level>(Level::parse(room())), character, theme, art);
      w.setDoubleJump(true); // as the game sets it for people
      return w;
    };

    check(doubleJumpCells(2) == 3 && doubleJumpCells(3) == 4 && doubleJumpCells(4) == 5 && doubleJumpCells(5) == 6,
      "double jump heights: 3, 4, 5 and 6 cells for 2 to 5 JUMP pips");
    check(doubleJumpCells(characterByIndex(2).jumpPips) > doubleJumpCells(characterByIndex(1).jumpPips),
      "Nova double jumps higher than Rocco");

    for (int character = 0; character < kDefaultRunners; ++character)
    {
      const CharacterDef& def = characterByIndex(character);
      std::printf("runner %d (%s)\n", character, def.name.c_str());
      int first = 0;
      for (int v : def.jumpArc)
        first += v;
      const int second = doubleJumpCells(def.jumpPips);

      // A plain full jump, holding the button: the runner's own height.
      World w = make(character);
      bool held = false;
      for (int f = 0; f < 5; ++f)
        step(w, false, held);
      const int ground = w.player().y;
      int top = ground;
      for (int f = 0; f < 30; ++f)
      {
        step(w, true, held);
        top = std::min(top, w.player().y);
      }
      check(ground - top == first, "a held jump goes the runner's jump height");
      for (int f = 0; f < 20; ++f)
        step(w, false, held);
      check(w.player().state == PlayerState::OnGround && w.player().y == ground, "back on the ground");

      // Jump, let go after 4 frames, press again: the double jump rises its
      // own height from where it starts.
      for (int f = 0; f < 5; ++f)
        step(w, true, held);
      step(w, false, held);
      check(w.player().state == PlayerState::Jumping, "still rising when the second press comes");
      const int from = w.player().y;
      top = from;
      bool somersault = false;
      for (int f = 0; f < 30; ++f)
      {
        step(w, true, held);
        top = std::min(top, w.player().y);
        somersault = somersault || w.player().visual == PlayerVisual::Somersault;
      }
      check(from - top == second, "the double jump rises the runner's double jump height");
      check(ground - top > first, "a double jump goes higher than a single jump");
      check(somersault, "the double jump flips");

      // A third press in the same flight does nothing; landing gives it back.
      World t = make(character);
      held = false;
      for (int f = 0; f < 5; ++f)
        step(t, false, held);
      for (int f = 0; f < 4; ++f)
        step(t, true, held);
      step(t, false, held);
      step(t, true, held); // the double jump
      for (int f = 0; f < 3; ++f)
        step(t, true, held);
      step(t, false, held);
      const int beforeThird = t.player().y;
      top = beforeThird;
      for (int f = 0; f < 12; ++f)
      {
        step(t, f < 6, held);
        top = std::min(top, t.player().y);
      }
      check(top >= beforeThird - 2, "only one double jump per flight");
      for (int f = 0; f < 40; ++f)
        step(t, false, held);
      check(t.player().state == PlayerState::OnGround, "landed");
      check(!t.player().airJumped, "landing gives the double jump back");

      // A running jump: always a somersault.
      World r = make(character);
      held = false;
      for (int f = 0; f < 5; ++f)
        step(r, false, held);
      for (int f = 0; f < 6; ++f)
        step(r, false, held, true, true);
      bool flipped = false;
      for (int f = 0; f < 6; ++f)
      {
        step(r, true, held, true, true);
        flipped = flipped || r.player().visual == PlayerVisual::Somersault;
      }
      check(flipped, "a running jump is a somersault");

      // The bot plays without it: a press in the air is only a press.
      World b(std::make_shared<const Level>(Level::parse(room())), character, theme, art);
      held = false;
      for (int f = 0; f < 5; ++f)
        step(b, false, held);
      for (int f = 0; f < 5; ++f)
        step(b, true, held);
      step(b, false, held);
      const int botFrom = b.player().y;
      top = botFrom;
      for (int f = 0; f < 12; ++f)
      {
        step(b, f < 6, held);
        top = std::min(top, b.player().y);
      }
      check(top >= botFrom - 2 && !b.player().airJumped, "no double jump for the bot");
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf("%s\n", gFailures ? "FAILED" : "passed");
  return gFailures ? 1 : 0;
}
