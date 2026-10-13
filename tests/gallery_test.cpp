// Level 39's mechanics, played by hand in small rooms: up into a monitor
// and out of the other one, and back; a shot through a monitor; the Echo
// Mic's free repeat; a panning monitor; the Floor Manager's count; a Cable
// Wrangler's trip; Rewind's crusher. Then the bot clears the Gallery and
// Rewind with every runner.

#include "assets/art.hpp"
#include "data/level.hpp"
#include "data/theme.hpp"
#include "frontend/bot.hpp"
#include "game/world.hpp"
#include "render/renderer.hpp"

#include <SDL.h>

#include <algorithm>
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

// A 40 x 24 room with walls, a floor on rows 22 and 23, the runner at
// (px, py), plus `walls` (a '.' wall clears) and `entities`.
std::string room(int px, int py, const std::string& entities, const std::vector<Wall>& walls = {},
  const std::string& extra = "weapon=echo_mic\n")
{
  const int h = 24;
  std::vector<std::string> map(std::size_t(h), std::string(40, '.'));
  for (int y = 0; y < h; ++y)
    map[std::size_t(y)][0] = map[std::size_t(y)][1] = map[std::size_t(y)][38] = map[std::size_t(y)][39] = '#';
  for (int x = 0; x < 40; ++x)
    map[std::size_t(h - 2)][std::size_t(x)] = map[std::size_t(h - 1)][std::size_t(x)] = '#';
  for (const auto& w : walls)
    for (int y = w.y0; y <= w.y1; ++y)
      for (int x = w.x0; x <= w.x1; ++x)
        map[std::size_t(y)][std::size_t(x)] = w.c;
  map[std::size_t(py)][std::size_t(px)] = 'P';
  std::string s = "name=GALLERY TEST\nepisode=6\ntheme=gallery\n" + extra + "[map]\n";
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

void tap(World& w, Keys pad, Keys& prev)
{
  step(w, pad, prev);
  step(w, Keys{}, prev);
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

const Enemy* firstOf(const World& w, EnemyKind kind)
{
  for (const auto& e : w.enemies())
    if (e.kind == kind && e.alive)
      return &e;
  return nullptr;
}

} // namespace

int main(int argc, char** argv)
{
  if (argc < 3)
    return 2;
  SDL_Init(0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  {
    Renderer renderer(sdl);
    const Theme& theme = themeByIndex(0);
    const Art art = Art::build(theme, renderer);
    Keys none, right, left, up, fire;
    right.right = true;
    left.left = true;
    up.up = true;
    fire.fire = true;
    auto make = [&](const std::string& text, int character = 0) {
      return World(std::make_shared<const Level>(Level::parse(text)), character, theme, art);
    };
    // Two monitors in one room, split by a wall: A at (6, 19), B at (30, 19).
    const std::string pair = "@ portal A 6 19 to=B label=\"CAM B\"\n@ portal B 30 19 to=A label=\"CAM A\"\n";
    const std::vector<Wall> split = {{20, 2, 20, 21}};
    {
      // Up in front of a monitor: the static wipe, then out in front of the other.
      World w = make(room(7, 21, pair, split));
      Keys prev;
      tap(w, up, prev);
      const bool wiping = w.gallery().hop > 0;
      idle(w, kGlHop + 2);
      const auto& p = w.player();
      std::printf("     at %d,%d\n", p.x, p.y);
      check(wiping, "up at a monitor starts the static wipe");
      check(p.x + 1 >= 60 && p.x + 1 < 64 && p.y == 43, "the runner comes out in front of the other monitor");
      tap(w, up, prev);
      idle(w, kGlHop + 2);
      check(w.player().x + 1 < 20, "and back again (two-way)");
      // Away from a monitor up does nothing.
      hold(w, right, 6, prev);
      tap(w, up, prev);
      check(w.gallery().hop == 0, "up away from a monitor does nothing");
    }
    {
      // A shot into a monitor comes out of the other one and hits what is there.
      World w = make(room(3, 21, pair + "@ floormgr 34 21\n", split));
      const Enemy* fm = firstOf(w, EnemyKind::FloorManager);
      const int hp = fm ? fm->hp : 0;
      Keys prev;
      w.cheat(Cheat::Ammo);
      tap(w, fire, prev);
      for (int i = 0; i < 30; ++i)
      {
        idle(w, 1);
        if (std::getenv("GR_DBG"))
          for (const auto& pr : w.projectiles())
            std::printf("     f%d shot %d,%d dx %d hop %d alive %d\n", i, pr.x, pr.y, pr.dx, pr.hopCool, pr.alive);
      }
      fm = firstOf(w, EnemyKind::FloorManager);
      std::printf("     floor manager hp %d -> %d\n", hp, fm ? fm->hp : 0);
      check(!fm || fm->hp < hp, "a shot goes through a monitor");
    }
    {
      // The Echo Mic: a shot comes again 15 frames later, costing nothing.
      World w = make(room(5, 21, ""));
      w.cheat(Cheat::Ammo);
      Keys prev;
      const int ammo = w.player().ammo;
      tap(w, fire, prev);
      const int after = w.player().ammo;
      std::printf("     ammo %d -> %d, weapon %d, shots %d\n", ammo, after, int(w.player().weapon), int(w.projectiles().size()));
      idle(w, kGlEcho + 1);
      bool echo = false;
      for (const auto& pr : w.projectiles())
        echo = echo || (pr.alive && pr.echo);
      check(after == ammo - 1 && echo, "an Echo Mic shot repeats once, for free");
    }
    {
      // A panning monitor drops the runner where its camera looks.
      World w = make(room(5, 21, "@ camspot CS path=24,21;34,21 speed=1/2\n@ portal M 4 19 to=CS\n"));
      Keys prev;
      idle(w, 10);
      const int cx = w.camX(w.gallery().cams[0]);
      tap(w, up, prev);
      idle(w, kGlHop + 2);
      std::printf("     camera at %d, runner at %d\n", cx, w.player().x + 1);
      check(std::abs(w.player().x + 1 - cx) <= 6 && w.player().y == 43, "a panning monitor drops the runner at its camera");
    }
    {
      // The Floor Manager counts 3-2-1 and his room fires; out of sight the count is off.
      World w = make(room(10, 21, "@ floormgr 18 21\n@ boomop 24 21\n"));
      idle(w, 4);
      const Enemy* fm = firstOf(w, EnemyKind::FloorManager);
      check(fm && fm->tell > 0, "the Floor Manager starts his count");
      int shots = 0;
      for (int i = 0; i < kGlCount + 4; ++i)
      {
        idle(w, 1);
        for (const auto& pr : w.projectiles())
          shots = std::max(shots, int(pr.alive && pr.kind == ShotKind::Enemy ? 1 : 0));
      }
      check(shots > 0, "on one, his room fires at the runner");
    }
    {
      // A Cable Wrangler's cable trips the runner: a heart and lying down.
      World w = make(room(10, 21, "@ wrangler 20 21\n"));
      const int hp = w.player().hp;
      bool tripped = false;
      for (int i = 0; i < 200 && !tripped; ++i)
      {
        idle(w, 1);
        tripped = w.gallery().trip > 0;
      }
      check(tripped && w.player().hp < hp, "the cable trips the runner");
    }
    {
      // Rewind: a death takes the runner back to where they stood.
      World w = make(room(12, 21, "@ crusher 12 4 w=2 to=21 period=30 tell=8\n", {}, "rules=rewind\ntimer=120\ngoal=exit\n"));
      Keys prev;
      idle(w, 40);
      idle(w, kGlRewindPlay + 30);
      std::printf("     rewinds %d deaths %d\n", w.gallery().rewinds, w.stats().deaths);
      check(w.gallery().rewinds > 0 && w.stats().deaths == 0 && w.player().state != PlayerState::Dying,
            "a crusher rewinds the runner instead of killing them");
    }

    // The bot clears the Gallery and Rewind with every runner.
    const auto level = std::make_shared<const Level>(Level::loadFile(argv[1]));
    const auto bonus = std::make_shared<const Level>(Level::loadFile(argv[2]));
    const int only = std::getenv("GR_GALLERY_RUNNER") ? std::atoi(std::getenv("GR_GALLERY_RUNNER")) : -1;
    for (int character = 0; character < 6; ++character)
    {
      if (only >= 0 && character != only)
        continue;
      World w(level, character, theme, art);
      Bot bot;
      const int f = botPlay(w, 9000, bot);
      std::printf("     runner %d: %d frames, %d deaths, route %d\n", character, f, w.stats().deaths, w.gallery().routeAt);
      check(w.state() != WorldState::Playing, "the bot clears the Gallery");
    }
    for (int character = 0; character < 6; ++character)
    {
      if (only >= 0 && character != only)
        continue;
      World w(bonus, character, theme, art);
      Bot bot;
      const int f = botPlay(w, 1800, bot);
      std::printf("     runner %d: %d frames, %d rewinds\n", character, f, w.gallery().rewinds);
      check(w.state() != WorldState::Playing && !w.bonusFailed(), "the bot clears Rewind");
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf(gFailures == 0 ? "all gallery tests passed\n" : "%d gallery test(s) failed\n", gFailures);
  return gFailures == 0 ? 0 : 1;
}
