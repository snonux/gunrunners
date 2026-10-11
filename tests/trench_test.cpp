// Level 30's mechanics, played by hand in small rooms under water: a jump
// is a 4-cell swim stroke and the strokes run out (Nova has one more), the
// runner sinks a cell every third frame (Rocco every other), air pockets,
// currents (Rocco too heavy for them), the abyss's return, the Harpoon
// Line (plant one, shoot an anchor ring: a line to walk; too far: none),
// a Cable Eel out of reach in its box, saving with a line. Then Bubble Up:
// the bubble rises, a tap of jump sinks it, spikes pop it back to the last
// ring, and the bot steers every runner to the exit.

#include "assets/art.hpp"
#include "data/level.hpp"
#include "data/theme.hpp"
#include "frontend/bot.hpp"
#include "game/savegame.hpp"
#include "game/world.hpp"
#include "render/renderer.hpp"

#include <SDL.h>

#include <cstdio>
#include <cstdlib>
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

struct Wall
{
  int x0, y0, x1, y1;
  char c = '#';
};

// A 40 x 24 room under water: ceiling row 0, floor rows 22-23, walls at x 0
// and 39, the runner at (px, py), plus `walls` and `entities`.
std::string room(int px, int py, const std::string& entities, const std::vector<Wall>& walls = {},
  const std::string& extra = "")
{
  std::vector<std::string> map(24, std::string(40, '.'));
  for (int x = 0; x < 40; ++x)
    map[22][std::size_t(x)] = map[23][std::size_t(x)] = map[0][std::size_t(x)] = '#';
  for (int y = 0; y < 24; ++y)
    map[std::size_t(y)][0] = map[std::size_t(y)][39] = '#';
  for (const auto& w : walls)
    for (int y = w.y0; y <= w.y1; ++y)
      for (int x = w.x0; x <= w.x1; ++x)
        map[std::size_t(y)][std::size_t(x)] = w.c;
  map[std::size_t(py)][std::size_t(px)] = 'P';
  std::string s = "name=TRENCH TEST\nepisode=5\ntheme=deep_sea\nweapon=harpoon_line\n" + extra + "[map]\n";
  for (const auto& row : map)
    s += row + "\n";
  s += "[entities]\n";
  if (extra.find("bubble") == std::string::npos)
    s += "@ fluid kind=water rect=0,0,39,23\n";
  s += entities;
  return s;
}

struct Keys
{
  bool left = false, right = false, down = false, jump = false, fire = false, up = false;
};

void step(World& w, Keys pad, Keys& prev)
{
  PlayerInput p;
  p.left = pad.left;
  p.right = pad.right;
  p.up = pad.up;
  p.down = pad.down;
  p.jump = {pad.jump, pad.jump && !prev.jump};
  p.fire = {pad.fire, pad.fire && !prev.fire};
  w.update(p);
  prev = pad;
}

void hold(World& w, Keys pad, int frames, Keys& prev)
{
  for (int f = 0; f < frames; ++f)
    step(w, pad, prev);
}

void idle(World& w, int frames)
{
  Keys prev;
  for (int f = 0; f < frames; ++f)
    step(w, Keys{}, prev);
}

// A tap of a button (jump or fire), then let go.
void tap(World& w, Keys pad, Keys& prev)
{
  step(w, pad, prev);
  pad.fire = pad.jump = false;
  step(w, pad, prev);
}

int botPlay(World& w, int frames, Bot& bot)
{
  Input prev;
  int f = 0;
  for (; f < frames && w.state() == WorldState::Playing; ++f)
  {
    const Input in = bot.play(w);
    PlayerInput p;
    p.left = in.left;
    p.right = in.right;
    p.up = in.up;
    p.down = in.down;
    p.jump = {in.jump, in.jump && !prev.jump};
    p.fire = {in.fire, in.fire && !prev.fire};
    prev = in;
    w.update(p);
  }
  return f;
}

} // namespace

int main(int argc, char** argv)
{
  if (argc < 2)
    return 2;
  SDL_Init(0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  {
    Renderer renderer(sdl);
    const Theme& theme = themeByIndex(0);
    const Art art = Art::build(theme, renderer);
    Keys none, jump, fire, down, left, right;
    jump.jump = true;
    fire.fire = true;
    down.down = true;
    left.left = true;
    right.right = true;

    auto make = [&](const std::string& text, int character = 0) {
      return World(std::make_shared<const Level>(Level::parse(text)), character, theme, art);
    };
    Keys crouchFire = down;
    crouchFire.fire = true;

    // Swimming: a jump is a 4-cell stroke, three strokes (Nova four), then
    // no more until ground; the runner sinks a cell every third frame
    // (Rocco every other).
    for (int character : {0, 2})
    {
      World w = make(room(10, 21, ""), character);
      idle(w, 4);
      const int y0 = w.player().y;
      Keys prev;
      int top = y0;
      for (int k = 0; k < 5; ++k)
      {
        tap(w, jump, prev);
        for (int f = 0; f < 4; ++f)
        {
          idle(w, 1);
          top = std::min(top, w.player().y);
        }
      }
      const int want = character == 2 ? 16 : 12;
      check(y0 - top == want, character == 2 ? "Nova: four strokes climb 16 cells, a fifth does nothing"
                                             : "Dash: three strokes climb 12 cells, a fourth does nothing");
    }
    for (int character : {0, 1})
    {
      World w = make(room(10, 11, ""), character);
      idle(w, 3);
      const int y0 = w.player().y;
      idle(w, 30);
      const int sunk = w.player().y - y0;
      check(character == 1 ? sunk == 15 : sunk == 10,
        character == 1 ? "Rocco sinks a cell every other frame" : "Dash sinks a cell every third frame");
    }
    {
      // An air pocket: a real jump, and strokes back.
      World w = make(room(10, 21, "@ airpocket rect=1,10,38,21\n"));
      idle(w, 4);
      const int y0 = w.player().y;
      Keys prev;
      tap(w, jump, prev);
      int top = y0;
      for (int f = 0; f < 10; ++f)
      {
        idle(w, 1);
        top = std::min(top, w.player().y);
      }
      check(!w.underwater() && y0 - top > 4, "in an air pocket a jump is a jump");
    }
    // A current pushes a swimmer; Rocco is too heavy for it.
    for (int character : {0, 1})
    {
      World w = make(room(10, 21, "@ current rect=1,1,38,21 dir=r push=1\n"), character);
      idle(w, 4);
      const int x0 = w.player().x;
      idle(w, 10);
      check(w.player().x == x0, character == 1 ? "Rocco: a current leaves him standing"
                                               : "a current leaves a runner on the ground alone");
      Keys prev;
      tap(w, jump, prev);
      idle(w, 6);
      check(character == 1 ? w.player().x == x0 : w.player().x > x0 + 4,
        character == 1 ? "...and swimming" : "...but carries a swimmer");
    }
    {
      // The abyss: a heart, and back at the last ground stood on.
      World w = make(room(10, 21, "@ abyss rect=16,20,30,21\n", {{16, 22, 30, 23, '.'}}));
      idle(w, 4);
      const int hp = w.player().hp;
      Keys prev;
      hold(w, right, 14, prev);
      bool back = false;
      for (int f = 0; f < 120 && !back; ++f)
      {
        step(w, none, prev);
        back = w.player().hp == hp - 1;
      }
      check(back && w.player().state == PlayerState::OnGround && w.player().x < 16 * 2 && w.trench().fade > 0,
        "the abyss costs a heart and puts the runner back on the ground");
    }
    {
      // The Harpoon Line: plant one on a ledge, shoot the ring across the
      // floor: a line at the ledge's height to walk on.
      World w = make(room(31, 19, "@ anchor 16 20 spot=31\n", {{30, 20, 35, 21}}));
      w.cheat(Cheat::Ammo);
      idle(w, 4);
      const int ammo = w.player().ammo;
      Keys prev;
      step(w, down, prev);
      step(w, crouchFire, prev);
      step(w, down, prev);
      idle(w, 2);
      check(w.trench().harpoons.size() == 1 && w.player().ammo == ammo - 1 && !w.trench().line,
        "crouched, fire plants a harpoon (one harpoon: no line)");
      step(w, left, prev);
      step(w, none, prev);
      tap(w, fire, prev);
      idle(w, 14);
      const auto& t = w.trench();
      check(t.line && t.lineY == 40 && t.lineX0 <= 33 && t.lineX1 >= 62,
        "a harpoon in the anchor ring strings a line at the ledge's height");
      hold(w, left, 24, prev);
      check(w.player().state == PlayerState::OnGround && w.player().y == 39 && w.player().x < 50 && w.lineUnder(),
        "...and the runner walks out on it");
      // A save keeps the line.
      SaveGame s = w.snapshot();
      World fresh = make(room(31, 19, "@ anchor 16 20 spot=31\n", {{30, 20, 35, 21}}));
      check(fresh.restore(s) && fresh.trench().line && fresh.trench().lineX0 == t.lineX0 && fresh.player().y == 39 &&
          fresh.lineUnder(),
        "a save keeps the line, and the runner on it");
    }
    {
      // Too far: the second harpoon drops, no line.
      World w = make(room(36, 19, "@ anchor 2 20 spot=36\n", {{30, 20, 38, 21}}));
      w.cheat(Cheat::Ammo);
      idle(w, 4);
      Keys prev;
      step(w, down, prev);
      step(w, crouchFire, prev);
      step(w, down, prev);
      idle(w, 2);
      step(w, left, prev);
      step(w, none, prev);
      tap(w, fire, prev);
      idle(w, 20);
      check(!w.trench().line && w.trench().harpoons.size() == 1, "a ring too far away: no line");
    }
    {
      // A Cable Eel: shots pass its box while it is in; near it, it sparks
      // and lunges out.
      World w = make(room(5, 21, "@ cable_eel 20 20 face=l\n"));
      idle(w, 4);
      const Enemy* eel = nullptr;
      for (const auto& e : w.enemies())
        if (e.kind == EnemyKind::CableEel)
          eel = &e;
      const int hp = eel ? eel->hp : 0;
      Keys prev;
      for (int k = 0; k < 3; ++k)
      {
        tap(w, fire, prev);
        idle(w, 3);
      }
      idle(w, 10);
      check(eel && eel->hp == hp && w.trench().eels[0].state == 0, "shots do nothing to an eel in its box");
      hold(w, right, 22, prev);
      bool out = false;
      for (int f = 0; f < 30 && !out; ++f)
      {
        step(w, none, prev);
        out = w.trench().eels[0].state == 2 && w.trench().eels[0].ext > 0;
      }
      check(out, "close to its box it sparks and lunges out");
    }

    // Bubble Up: the bubble rises half a cell a frame, a tap of jump sinks
    // it two cells, spikes pop it back to the last ring passed.
    {
      World w = make(room(10, 21, "@ ring 10 16\n", {{1, 8, 38, 8, 'v'}}, "rules=bubble\n"));
      const int y0 = w.player().y;
      idle(w, 20);
      check(w.player().y == y0 - 10, "the bubble rises half a cell a frame");
      Keys prev;
      const int y1 = w.player().y;
      step(w, jump, prev);
      check(w.player().y == y1 + 2, "a tap of jump sinks it two cells");
      bool popped = false;
      for (int f = 0; f < 80 && !popped; ++f)
      {
        step(w, none, prev);
        popped = w.trench().pop > 0;
      }
      check(popped && w.player().y == 33 && w.trench().ringY == 33 && w.player().hp == 9,
        "spikes pop it: back at the ring passed, no heart lost");
    }
    {
      // The bot takes every runner up the bonus to its exit.
      const auto level = std::make_shared<const Level>(Level::loadFile(argv[1]));
      for (int character = 0; character < 6; ++character)
      {
        World w(level, character, theme, art);
        Bot bot;
        const int frames = botPlay(w, 75 * 15, bot);
        char what[96];
        std::snprintf(what, sizeof what, "runner %d rides the bubble to the exit (%d frames)", character, frames);
        check(w.state() != WorldState::Playing, what);
      }
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf("%d failure(s)\n", gFailures);
  return gFailures == 0 ? 0 : 1;
}
