// Level 21's mechanics, played by hand in small rooms: a level-script
// shift sliding a bulkhead shut, the Phase Rifle through one wall but not
// two, a Lattice Turret on its rail, an Echo pad, a Repair Swarm rebuilding
// a door, a kill switch arming a pad, a grating broken by a landing only,
// the terminal's code, and the Wireframe rules. Then the Wireframe bonus,
// played by the bot.

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

// A 40 x 24 room: ceiling row 0, floor rows 22-23, walls at x 0 and 39, the
// runner at (x, 21), plus whatever `rows` and `entities` say.
std::string room(int px, const std::vector<std::pair<int, std::string>>& rows, const std::string& entities,
  const std::string& header = "")
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
  std::string s = "name=ZERO TEST\nepisode=3\ntheme=station_servers\nweapon=phase_rifle\n" + header + "[map]\n";
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

int liveEchoes(const World& w)
{
  int n = 0;
  for (const auto& e : w.enemies())
    n += e.alive && e.kind == EnemyKind::Echo ? 1 : 0;
  return n;
}

} // namespace

int main(int argc, char** argv)
{
  SDL_Init(0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  {
    Renderer renderer(sdl);
    const Theme& theme = themeByIndex(0);
    const Art art = Art::build(theme, renderer);

    {
      // Passing x 12 starts the shift: the monitors preview it, then the
      // bulkhead slides shut and is solid.
      const auto level = std::make_shared<const Level>(Level::parse(room(4, {},
        "@ layer BK rect=30,18,31,21 driver=script style=zero solid=0\n@ shift S trigger=x:12 close=BK\n"
        "@ monitor 20 1\n")));
      World w(level, 0, theme, art);
      check(w.zero().doors.size() == 1 && w.zero().doors[0].pos <= 0.0f, "the bulkhead starts open");
      Keys prev, right;
      right.right = true;
      int frames = 0;
      while (w.zero().shifts[0].t < 0 && frames < 120)
      {
        step(w, right, prev);
        ++frames;
      }
      check(w.zero().shifts[0].t >= 0 && w.zero().preview == 0, "passing its x triggers it, the monitors preview it");
      idle(w, kShiftPreview - 2);
      check(w.zero().doors[0].pos <= 0.0f, "during the preview nothing moves");
      idle(w, kShiftSlide + 8);
      std::printf("     door pos %.2f, done %d\n", double(w.zero().doors[0].pos), int(w.zero().shifts[0].done));
      check(w.zero().shifts[0].done && w.zero().doors[0].pos >= 1.0f, "then the bulkhead slides shut");
      for (int f = 0; f < 60; ++f)
        step(w, right, prev);
      check(w.player().x + Player::kWidth <= 30 * kCellsPerTile, "and the runner cannot pass it");
    }
    {
      // The Phase Rifle's shot passes one wall and breaks the door behind
      // it; two walls stop it.
      auto shoot = [&](std::vector<int> walls) {
        walls.push_back(18);
        std::vector<std::pair<int, std::string>> rows;
        for (int y = 17; y <= 21; ++y)
        {
          std::string r(38, ' ');
          for (int x : walls)
            r[std::size_t(x - 1)] = '#';
          rows.push_back({y, r});
        }
        const auto level = std::make_shared<const Level>(
          Level::parse(room(6, rows, "@ breakable D rect=18,17,18,21 by=any hp=1\n")));
        World w(level, 0, theme, art);
        w.cheat(Cheat::Ammo);
        Keys prev, fire;
        fire.fire = true;
        for (int f = 0; f < 60; ++f)
          step(w, (f % 20) == 0 ? fire : Keys{}, prev);
        return w.breakables()[0].broken && w.map().block(10, 20) == Tile::Solid;
      };
      check(shoot({10}), "a Phase Rifle shot goes through one wall and breaks the door behind");
      check(!shoot({10, 13}), "not through two");
    }
    {
      // A Lattice Turret runs along its rail to stand over the runner, then
      // fires straight down.
      const auto level = std::make_shared<const Level>(
        Level::parse(room(8, {}, "@ lattice_turret LT 14 16 rail=4,36\n")));
      World w(level, 0, theme, art);
      const int hp = w.player().hp;
      const int t = firstOf(w, EnemyKind::LatticeTurret);
      const int x0 = w.enemies()[std::size_t(t)].x;
      idle(w, 150);
      const int x1 = w.enemies()[std::size_t(t)].x;
      std::printf("     turret x %d -> %d (runner %d), hp %d -> %d\n", x0, x1, w.player().x, hp, w.player().hp);
      check(std::abs(x1 + 2 - (w.player().x + 1)) <= 3, "a Lattice Turret comes along its rail over the runner");
      check(w.player().hp < hp, "and shoots down at them");
    }
    {
      // An Echo pad: the runner passes it and kEchoDelay frames later an
      // Echo steps out and walks where they walked.
      const auto level = std::make_shared<const Level>(Level::parse(room(4, {}, "@ echopad E 8 21\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::God);
      Keys prev, right;
      right.right = true;
      hold(w, right, 20, prev);
      check(liveEchoes(w) == 0, "no Echo yet");
      int e = -1;
      for (int f = 0; f < kEchoDelay + 20 && e < 0; ++f)
      {
        step(w, right, prev);
        e = firstOf(w, EnemyKind::Echo);
      }
      check(e >= 0 && w.enemies()[std::size_t(e)].alive, "an Echo steps out of the pad");
      const int ex = e >= 0 ? w.enemies()[std::size_t(e)].x : 0;
      idle(w, 8);
      check(e >= 0 && w.enemies()[std::size_t(e)].x > ex, "and walks the runner's way");
    }
    {
      // A shootable kill switch arms its pad.
      const auto level = std::make_shared<const Level>(
        Level::parse(room(4, {}, "@ switch K1 20 20 kind=shootable\n@ echopad E 30 21 wake=K1\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      check(!w.zero().pads[0].armed, "a pad with a switch starts dark");
      Keys prev, fire;
      fire.fire = true;
      for (int f = 0; f < 60 && !w.zero().switches[0].hit; ++f)
        step(w, (f % 20) == 0 ? fire : Keys{}, prev);
      check(w.zero().switches[0].hit && w.zero().pads[0].armed, "shooting the switch arms it");
    }
    {
      // A Repair Swarm puts a broken rebuild=1 door back.
      const auto level = std::make_shared<const Level>(Level::parse(room(4,
        {{18, "             #"}, {19, "             #"}, {20, "             #"}, {21, "             #"}},
        "@ breakable D rect=14,18,14,21 by=any hp=1 look=rackdoor rebuild=1\n@ repair_swarm RS 17 15\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      w.cheat(Cheat::God);
      Keys prev, fire;
      fire.fire = true;
      for (int f = 0; f < 40 && !w.breakables()[0].broken; ++f)
        step(w, (f % 20) == 0 ? fire : Keys{}, prev);
      check(w.breakables()[0].broken && w.map().block(14, 20) == Tile::Empty, "the door breaks");
      int frames = 0;
      while (w.breakables()[0].broken && frames < 600)
      {
        idle(w, 1);
        ++frames;
      }
      std::printf("     rebuilt after %d frames\n", frames);
      check(!w.breakables()[0].broken && w.map().block(14, 20) == Tile::Solid && frames >= kRebuildFrames,
        "a Repair Swarm builds it back");
    }
    {
      // A grating breaks under a landing, not under fire.
      const auto level = std::make_shared<const Level>(
        Level::parse(room(10, {}, "@ breakable G rect=9,22,11,22 by=stomp hp=1 look=grate\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      Keys prev, crouch;
      crouch.down = crouch.fire = true;
      for (int f = 0; f < 40; ++f)
        step(w, (f % 20) == 0 ? crouch : Keys{}, prev);
      check(!w.breakables()[0].broken, "shooting a grating does nothing");
      Keys jump;
      jump.jump = true;
      hold(w, jump, 6, prev);
      idle(w, 40);
      check(w.breakables()[0].broken, "landing on it breaks it");
    }
    {
      // The terminal: up shows its text; up, up, down, down the joke.
      const auto level = std::make_shared<const Level>(
        Level::parse(room(10, {}, "@ terminal 10 21 text=\"HELLO\" code=1\n")));
      World w(level, 0, theme, art);
      Keys prev, up, down;
      up.up = true;
      down.down = true;
      hold(w, up, 3, prev);
      check(w.zero().terminals[0].shown > 0, "up at a terminal lights it");
      idle(w, 2);
      hold(w, up, 2, prev);
      idle(w, 2);
      hold(w, down, 2, prev);
      idle(w, 2);
      hold(w, down, 2, prev);
      idle(w, 2);
      check(w.zero().joke > 0, "up, up, down, down: the joke");
    }
    {
      // Wireframe: `#` inside the frame is passable; hitboxes are solid.
      const auto level = std::make_shared<const Level>(Level::parse(room(10,
        {{12, "##################"}, {16, "        ##########"}},
        "@ hitbox rect=1,16,8,16\n", "rules=wireframe\ntimer=90\ngoal=collect:1\n")));
      World w(level, 0, theme, art);
      check(w.map().block(5, 12) == Tile::Empty && w.map().block(3, 16) == Tile::Solid,
        "walls are wireframe, a hitbox is solid");
      Keys prev, jump;
      jump.jump = true;
      hold(w, jump, 6, prev);
      idle(w, 30);
      check(w.player().state == PlayerState::OnGround, "the runner jumps through a wall and lands");
    }
    if (argc > 1)
    {
      // The Wireframe bonus: the bot collects 40 gems.
      const auto level = std::make_shared<const Level>(Level::loadFile(argv[1]));
      World w(level, 0, theme, art);
      Bot bot;
      Input prev;
      for (int f = 0; f < 90 * 15 && w.state() == WorldState::Playing; ++f)
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
      std::printf("     wireframe: %d gems in %d frames\n", w.stats().gems, w.stats().frames);
      check(w.stats().gems >= 40, "the bot gets 40 gems in the Wireframe bonus");
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf("%s\n", gFailures == 0 ? "all zero tests passed" : "SOME TESTS FAILED");
  return gFailures == 0 ? 0 : 1;
}
