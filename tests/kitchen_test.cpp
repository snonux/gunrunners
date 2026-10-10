// Level 24's mechanics, played by hand in small rooms: a belt carrying the
// runner (not in Turbo), items looping round, a chef's bell flipping a
// station, a pantry flap, a meatball growing in sauce and going down the
// sink, a small meatball's bounce, a cleaver stuck in a wall as a handle,
// an Egg Timer wound back and popping, the Pepper Grinder's spin-up and
// sneeze, the toaster and the chef's hand. Then Turntable, played by the bot.

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

// A 40 x 24 room: ceiling row 0, floor rows 22-23, walls at x 0 and 39, the
// runner at (x, 21), plus whatever `rows` and `entities` say.
std::string room(int px, const std::vector<std::pair<int, std::string>>& rows, const std::string& entities)
{
  std::vector<std::string> map(24, std::string(40, '.'));
  for (int x = 0; x < 40; ++x)
    map[0][std::size_t(x)] = map[22][std::size_t(x)] = map[23][std::size_t(x)] = '#';
  for (int y = 0; y < 24; ++y)
    map[std::size_t(y)][0] = map[std::size_t(y)][39] = '#';
  for (const auto& r : rows)
    for (std::size_t i = 0; i < r.second.size(); ++i)
      if (r.second[i] != ' ')
        map[std::size_t(r.first)][1 + i] = r.second[i];
  map[21][std::size_t(px)] = 'P';
  std::string s = "name=KITCHEN TEST\nepisode=4\ntheme=tv_kitchen\nweapon=pepper_grinder\n[map]\n";
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

int firstOf(const World& w, EnemyKind kind)
{
  for (std::size_t i = 0; i < w.enemies().size(); ++i)
    if (w.enemies()[i].kind == kind)
      return int(i);
  return -1;
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

    {
      // A medium belt carries the runner half a cell a frame; in Turbo it
      // doesn't. Gems on a looping belt come back at its start.
      const auto level = std::make_shared<const Level>(Level::parse(room(6, {{21, "                  g"}},
        "@ belt B1 rect=1,22,30,22 speed=1/2 dir=r loop=1\n")));
      World w(level, 0, theme, art);
      check(w.kitchen().on && w.kitchen().belts.size() == 1 && w.kitchen().belts[0].period == 2, "the belt is set up");
      const int x0 = w.player().x;
      idle(w, 20);
      std::printf("     carried %d cells in 20 frames\n", w.player().x - x0);
      check(w.player().x - x0 >= 9 && w.player().x - x0 <= 11, "it carries the runner half a cell a frame");
      int gx = -1;
      for (const auto& it : w.items())
        gx = it.x;
      bool wrapped = false;
      for (int f = 0; f < 200 && !wrapped; ++f)
      {
        idle(w, 1);
        for (const auto& it : w.items())
          wrapped = wrapped || it.x < gx - 10;
      }
      check(wrapped, "a gem on a looping belt comes back round");
      w.cheat(Cheat::Turbo);
      const int x1 = w.player().x;
      idle(w, 10);
      check(w.player().x == x1, "in Turbo the belt doesn't move the runner");
    }
    {
      // A bell flips its station's belts after its ding; it rests 30 frames.
      const auto level = std::make_shared<const Level>(Level::parse(room(6, {},
        "@ belt B1 rect=1,22,30,22 speed=1/4 dir=r station=s1\n"
        "@ switch id=bell x=20 y=20 kind=shootable states=2 station=s1\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      w.cheat(Cheat::Turbo); // no spin-up, and the belt holds still
      Keys prev, fire;
      fire.fire = true;
      for (int f = 0; f < 20 && w.kitchen().bells[0].rings == 0; ++f)
        step(w, (f % 2 == 0) ? fire : Keys{}, prev);
      check(w.kitchen().bells[0].rings == 1 && w.kitchen().belts[0].dir == 1, "a shot rings the bell");
      hold(w, Keys{}, kBellDing + 1, prev);
      check(w.kitchen().belts[0].dir == -1, "after the ding the belt runs the other way");
    }
    {
      // A flap opens only for a runner a belt pushes into it.
      const auto level = std::make_shared<const Level>(Level::parse(room(14, {},
        "@ belt B1 rect=11,22,30,22 speed=1 dir=l\n"
        "@ flap 10 19 h=3\n")));
      World w(level, 0, theme, art);
      check(w.map().block(10, 20) == Tile::Solid, "the flap is shut");
      bool through = false;
      for (int f = 0; f < 60 && !through; ++f)
      {
        idle(w, 1);
        through = w.player().x / kCellsPerTile < 10;
      }
      check(through, "the belt pushes the runner through it");
      Keys walk, left;
      left.left = true;
      hold(w, left, 6, walk);
      idle(w, 40);
      check(w.map().block(10, 20) == Tile::Solid, "and it swings shut");
      Keys prev, right;
      right.right = true;
      const int x0 = w.player().x;
      hold(w, right, 20, prev);
      check(w.player().x / kCellsPerTile < 10 && w.player().x - x0 <= 4, "walking into it from inside, it stays shut");
    }
    {
      // A meatball rolls with its belt, through sauce (one size bigger) and
      // off the end into the sink.
      const auto level = std::make_shared<const Level>(Level::parse(room(6, {{22, "                        ..."}},
        "@ belt B1 rect=12,22,24,22 speed=1/2 dir=r\n"
        "@ meatball MB 13 21\n"
        "@ sauce rect=16,21,18,21\n"
        "@ sink rect=25,22,27,22\n")));
      World w(level, 0, theme, art);
      const int m = firstOf(w, EnemyKind::Meatball);
      bool grew = false;
      for (int f = 0; f < 120 && !grew; ++f)
      {
        idle(w, 1);
        grew = w.enemies()[std::size_t(m)].variant == 1;
      }
      check(grew && w.enemies()[std::size_t(m)].w == 5, "sauce grows it a size");
      for (int f = 0; f < 200 && w.enemies()[std::size_t(m)].alive; ++f)
        idle(w, 1);
      check(!w.enemies()[std::size_t(m)].alive && w.stats().kills == 0, "down the sink it goes, no kill");
    }
    {
      // A small meatball bounces the runner back without hurting; a big one
      // hurts.
      const auto level = std::make_shared<const Level>(Level::parse(room(16, {},
        "@ meatball MB 10 21 dir=r\n@ meatball BIG 24 21 size=big dir=l\n")));
      World w(level, 0, theme, art);
      const int hp = w.player().hp;
      const int x0 = w.player().x;
      for (int f = 0; f < 60 && w.player().x == x0; ++f)
        idle(w, 1);
      check(w.player().hp == hp && w.player().x > x0, "a small one bounces the runner back");
      for (int f = 0; f < 200 && w.player().hp == hp; ++f)
        idle(w, 1);
      check(w.player().hp < hp, "a big one hurts");
    }
    {
      // A Cleaver Bot lobs a cleaver; one that hits a wall sticks there as a
      // handle and works loose.
      const auto level = std::make_shared<const Level>(Level::parse(room(14, {{17, "         #"}, {18, "         #"}, {19, "         #"}, {20, "         #"}, {21, "         #"}},
        "@ cleaver_bot CB 6 21\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::God);
      bool stuck = false, thrown = false;
      for (int f = 0; f < 300 && !stuck; ++f)
      {
        idle(w, 1);
        for (const auto& pr : w.projectiles())
          thrown = thrown || (pr.alive && pr.thrown == 4);
        stuck = !w.kitchen().cleavers.empty();
      }
      check(thrown, "it lobs cleavers");
      check(stuck, "one that hits the wall between sticks there");
      if (stuck)
      {
        const auto& c = w.kitchen().cleavers[0];
        check(w.map().block(c.bx, c.by) == Tile::Platform, "a cleaver in the wall is a one-way handle");
        const int bx = c.bx, by = c.by;
        idle(w, kCleaverStick + 1);
        check(w.map().block(bx, by) == Tile::Empty, "it works loose");
      }
    }
    {
      // An Egg Timer's dial runs down from 90; a hit winds it back 10; at 0
      // it pops and hurts within 2 blocks.
      const auto level = std::make_shared<const Level>(Level::parse(room(12, {}, "@ egg_timer E 14 21\n")));
      World w(level, 0, theme, art);
      const int e = firstOf(w, EnemyKind::EggTimer);
      idle(w, 2);
      check(w.enemies()[std::size_t(e)].attach == 1 && w.enemies()[std::size_t(e)].cool > kEggDial - 5, "its dial starts");
      w.cheat(Cheat::Ammo);
      const int dial = w.enemies()[std::size_t(e)].cool;
      Keys prev, fire;
      fire.fire = true;
      hold(w, fire, kGrinderSpin + 3, prev);
      hold(w, Keys{}, 2, prev);
      check(w.enemies()[std::size_t(e)].cool > dial - kGrinderSpin - 5, "a hit winds it back");
      const int hp = w.player().hp;
      for (int f = 0; f < 200 && w.enemies()[std::size_t(e)].alive; ++f)
        idle(w, 1);
      check(!w.enemies()[std::size_t(e)].alive && w.player().hp < hp, "it pops and hurts the runner beside it");
    }
    {
      // The Pepper Grinder spins up 15 frames before the first pepper; five
      // hits on one enemy within 30 frames make it sneeze.
      const auto level = std::make_shared<const Level>(Level::parse(room(6, {}, "@ meatball MB 16 21 size=giant\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      const int ammo = w.player().ammo;
      Keys prev, fire;
      fire.fire = true;
      hold(w, fire, kGrinderSpin - 2, prev);
      check(w.player().ammo == ammo, "no pepper while it spins up");
      bool sneezed = false;
      for (int f = 0; f < 40 && !sneezed; ++f)
      {
        step(w, fire, prev);
        sneezed = w.kitchen().sneezing > 0;
      }
      check(w.player().ammo < ammo, "then it fires");
      check(sneezed, "five hits make an enemy sneeze");
    }
    {
      // The toaster: shoot its lever, and 30 frames later the toast pops a
      // runner standing on it 12 cells up.
      const auto level = std::make_shared<const Level>(Level::parse(room(4, {{20, "       ####"}, {21, "       ####"}},
        "@ toaster rect=8,20,11,21 lever=15,18\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      w.cheat(Cheat::Turbo);
      Keys prev, right, jump, fire, both;
      right.right = true;
      jump.jump = true;
      both = right;
      both.jump = true;
      hold(w, both, 8, prev);
      hold(w, Keys{}, 20, prev);
      const int top = w.player().y;
      check(top == 20 * kCellsPerTile - 1, "the runner is on the toaster");
      fire.fire = true;
      step(w, fire, prev);
      int best = w.player().y;
      for (int f = 0; f < kToastPop + 30; ++f)
      {
        step(w, Keys{}, prev);
        best = std::min(best, w.player().y);
      }
      std::printf("     toaster: feet %d, rose to %d\n", top, best);
      check(top - best >= 10, "the toast launches the runner");
    }
    {
      // The chef's hand lifts the runner 2 blocks and sets them down.
      const auto level = std::make_shared<const Level>(Level::parse(room(10, {}, "@ script special x=12 y=21\n")));
      World w(level, 0, theme, art);
      Keys prev, right;
      right.right = true;
      const int y0 = w.player().y;
      for (int f = 0; f < 30 && w.kitchen().hand == 0; ++f)
        step(w, right, prev);
      check(w.kitchen().hand > 0, "the hand comes down");
      hold(w, right, 20, prev);
      check(w.player().y == y0 - 4, "it holds the runner up 2 blocks");
      const int hp = w.player().hp;
      hold(w, Keys{}, kHandFrames, prev);
      check(w.kitchen().hand == 0 && w.player().y == y0 && w.player().hp == hp, "and sets them down unhurt");
    }
    if (std::string(argv[1]) != "-")
    {
      // Turntable: the bot gathers the gems while the room turns, with
      // each runner.
      const auto level = std::make_shared<const Level>(Level::loadFile(argv[1]));
      for (int ch = 0; ch < 6; ++ch)
      {
      World w(level, ch, theme, art);
      Bot bot;
      Input prev;
      for (int f = 0; f < 65 * 15 && w.state() == WorldState::Playing; ++f)
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
      std::printf("     turntable, runner %d: %d gems, %d frames\n", ch, w.stats().gems, w.stats().frames);
      check(w.stats().gems >= 18, "the bot gathers three quarters of the gems or more");
      }
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf("%s\n", gFailures == 0 ? "all kitchen tests passed" : "SOME TESTS FAILED");
  return gFailures == 0 ? 0 : 1;
}
