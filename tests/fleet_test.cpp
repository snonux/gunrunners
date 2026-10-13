// Level 34's mechanics, played by hand in small rooms: a drifting deck
// carrying the runner, a parrot on a balloon sinking its ship, the Grapple
// Gun's hook catching a ring and the jump off it, the clouds, Balloon Mines
// rising, the bomb bay's hatch opening from inside, the captain's log, and
// the Human Cannonball's aim, launch and nets. Then the bot clears Human
// Cannonball with every runner.

#include "assets/art.hpp"
#include "data/level.hpp"
#include "data/theme.hpp"
#include "frontend/bot.hpp"
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

// A 40 x h room over the clouds: ceiling row 0, floor rows h-2 and h-1,
// walls at x 0 and 39, the runner at (px, py), plus `walls` and `entities`.
std::string room(int px, int py, const std::string& entities, const std::vector<Wall>& walls = {}, int h = 24)
{
  std::vector<std::string> map(std::size_t(h), std::string(40, '.'));
  for (int x = 0; x < 40; ++x)
    map[std::size_t(h - 2)][std::size_t(x)] = map[std::size_t(h - 1)][std::size_t(x)] = map[0][std::size_t(x)] = '#';
  for (int y = 0; y < h; ++y)
    map[std::size_t(y)][0] = map[std::size_t(y)][39] = '#';
  for (const auto& w : walls)
    for (int y = w.y0; y <= w.y1; ++y)
      for (int x = w.x0; x <= w.x1; ++x)
        map[std::size_t(y)][std::size_t(x)] = w.c;
  map[std::size_t(py)][std::size_t(px)] = 'P';
  std::string s = "name=FLEET TEST\nepisode=5\ntheme=sunset_airships\nweapon=grapple_gun\n[map]\n";
  for (const auto& row : map)
    s += row + "\n";
  s += "[entities]\n" + entities;
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

void idle(World& w, int frames)
{
  Keys prev;
  for (int f = 0; f < frames; ++f)
    step(w, Keys{}, prev);
}

void hold(World& w, Keys pad, int frames, Keys& prev)
{
  for (int f = 0; f < frames; ++f)
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

int countOf(const World& w, EnemyKind kind)
{
  int n = 0;
  for (const auto& e : w.enemies())
    n += e.alive && !e.hidden && e.kind == kind;
  return n;
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
    Keys none, right, jump, up, upFire;
    right.right = true;
    jump.jump = true;
    up.up = true;
    upFire.up = true;
    upFire.fire = true;

    auto make = [&](const std::string& text, int character = 0) {
      return World(std::make_shared<const Level>(Level::parse(text)), character, theme, art);
    };

    {
      // A drifting deck carries the runner standing on it.
      World w = make(room(14, 13, "@ ship A 10 14 name=TEST x1=20 drift=2/60\n"));
      idle(w, 5);
      const int x0 = w.player().x, ox0 = w.fleet().ships[0].ox;
      idle(w, 12);
      const auto& p = w.player();
      const int moved = w.fleet().ships[0].ox - ox0;
      check(moved != 0 && p.x - x0 == moved && p.state == PlayerState::OnGround, "a drifting deck carries the runner");
    }
    {
      // A parrot perched on a ship's balloon sinks it, down to its limit.
      World w = make(room(3, 21, "@ ship B 10 14 name=TEST x1=20 sink=1\n@ peck_parrot 15 5\n"));
      idle(w, 200);
      const auto& sh = w.fleet().ships[0];
      check(sh.sink > 0 && sh.sink <= kCellsPerTile, "a parrot on the balloon sinks its ship");
    }
    {
      // Up and fire: the hook flies straight up and catches the ring.
      World w = make(room(10, 21, "@ ring 10 12\n"));
      w.cheat(Cheat::Ammo);
      Keys prev;
      step(w, upFire, prev);
      hold(w, up, 8, prev);
      const bool hooked = w.fleet().ring == 0 && w.player().state == PlayerState::Swing;
      hold(w, right, 20, prev);
      step(w, jump, prev);
      hold(w, none, 2, prev);
      const auto& p = w.player();
      check(hooked, "the Grapple Gun's hook catches a ring and the runner swings");
      check(w.fleet().ring < 0 && (p.state == PlayerState::Jumping || p.state == PlayerState::Falling),
        "jump lets go of the ring");
    }
    {
      // The clouds: falling into them is a death.
      World w = make(room(5, 5, "@ killzone rect=0,16,39,17\n", {}, 24));
      idle(w, 40);
      check(w.player().state == PlayerState::Dying || w.stats().deaths > 0, "falling into the clouds kills");
    }
    {
      // Balloon Mines come up out of the clouds.
      World w = make(room(14, 21, "@ spawn kind=balloon_mine rect=20,20,30,20 every=10 max=1\n"));
      idle(w, 15);
      int y0 = -1;
      for (const auto& e : w.enemies())
        if (e.alive && e.kind == EnemyKind::BalloonMine)
          y0 = e.y;
      idle(w, 10);
      int y1 = -1;
      for (const auto& e : w.enemies())
        if (e.alive && e.kind == EnemyKind::BalloonMine)
          y1 = e.y;
      check(countOf(w, EnemyKind::BalloonMine) == 1 && y0 >= 0 && y1 < y0, "a Balloon Mine rises out of the clouds");
    }
    {
      // The bomb bay: standing on its floor opens the hatch.
      World w = make(room(12, 20, "@ bay rect=10,18,16,20 hatch=16,17\n", {{10, 21, 16, 21}}));
      const bool shut = !w.fleet().hatchOpen;
      idle(w, 3);
      check(shut && w.fleet().hatchOpen, "the bomb bay's hatch opens from inside");
    }
    {
      // The captain's log pays 10000.
      World w = make(room(5, 21, "@ treasure 8 21 art=captains_log score=10000\n"));
      const int score = w.stats().score;
      Keys prev;
      hold(w, right, 10, prev);
      check(w.fleet().logTaken && w.stats().score >= score + 10000, "the captain's log pays 10000");
    }

    // Human Cannonball: up and down aim, jump fires, a net pays.
    const auto bonus = std::make_shared<const Level>(Level::loadFile(argv[1]));
    {
      World w(bonus, 0, theme, art);
      Keys prev, down, left;
      down.down = true;
      left.left = true;
      const int a0 = w.fleet().angle;
      step(w, up, prev);
      step(w, none, prev);
      const int a1 = w.fleet().angle;
      for (int i = 0; i < 2; ++i)
      {
        step(w, down, prev);
        step(w, none, prev);
      }
      const int a2 = w.fleet().angle;
      step(w, up, prev);
      step(w, none, prev);
      const int score = w.stats().score;
      step(w, jump, prev);
      hold(w, none, 2, prev);
      check(a0 == 45 && a1 == 60 && a2 == 30 && w.fleet().flying, "the cannon aims with up and down and fires with jump");
      hold(w, left, 60, prev);
      check(!w.fleet().flying && w.fleet().hits == 1 && w.stats().score >= score + 1000,
        "a shot at 45 degrees steered back lands in the near net");
    }
    for (int character = 0; character < 6; ++character)
    {
      World w(bonus, character, theme, art);
      Bot bot;
      const int f = botPlay(w, 900, bot);
      std::printf("     runner %d: %d frames\n", character, f);
      check(w.state() != WorldState::Playing && !w.bonusFailed(), "the bot clears Human Cannonball");
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf(gFailures == 0 ? "all fleet tests passed\n" : "%d fleet test(s) failed\n", gFailures);
  return gFailures == 0 ? 0 : 1;
}
