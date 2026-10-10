// Level 19's Gravity Switches and Grav Grenade, played by hand in small
// rooms: a chamber that starts upside down, a switch pressed and shot, the
// dizzy stars, a Flip Walker's fall, a Gravity Probe's pull, a Test
// Subject copying a jump, the vortex, a test gate, and Gun Gravity. Then
// the Gun Gravity bonus, played by the bot.

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
  std::string s = "name=GRAV TEST\nepisode=3\ntheme=station_hull\nweapon=grav_grenade\n" + header + "[map]\n";
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

const char* kZone = "@ gravzone LAB rect=1,1,38,21 dir=down switch=S1\n";

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
      // A chamber that starts upside down: the runner falls to the ceiling
      // and walks along it.
      const auto level = std::make_shared<const Level>(
        Level::parse(room(10, {}, "@ gravzone LAB rect=1,1,38,21 dir=up\n")));
      World w(level, 0, theme, art);
      idle(w, 30);
      const auto& p = w.player();
      std::printf("     runner %d,%d grav %d state %d\n", p.x, p.y, int(p.grav), int(p.state));
      check(p.grav == Grav::Up && p.y == 2 && p.state == PlayerState::OnGround, "an upside-down chamber: on the ceiling");
      const int x0 = p.x;
      Keys prev, right;
      right.right = true;
      for (int f = 0; f < 12; ++f)
        step(w, right, prev);
      check(w.player().x > x0 + 4 && w.player().y == 2, "and walking along it");
      // A jump pushes away from the ceiling, and comes back.
      Keys jump;
      jump.jump = true;
      int low = 2;
      for (int f = 0; f < 30; ++f)
      {
        step(w, jump, prev);
        low = std::max(low, w.player().y);
      }
      idle(w, 10);
      std::printf("     jump down to %d\n", low);
      check(low >= 7 && w.player().y == 2, "a jump goes down and falls back up");
    }
    {
      // Up on a switch turns the chamber over; again (after its rest) back.
      const auto level = std::make_shared<const Level>(
        Level::parse(room(10, {}, std::string(kZone) + "@ switch S1 11 20 kind=panel\n")));
      World w(level, 0, theme, art);
      idle(w, 4);
      Keys prev, up;
      up.up = true;
      step(w, up, prev);
      idle(w, 30);
      check(w.grav().zones[0].dir == Grav::Up && w.player().grav == Grav::Up && w.player().y == 2,
        "up on a switch: the chamber turns over, the runner falls up");
      // The switch is down on the floor now: shoot it from the ceiling.
      idle(w, 12);
      check(w.player().state == PlayerState::OnGround, "landed on the ceiling");
    }
    {
      // A shot at a switch throws it.
      const auto level = std::make_shared<const Level>(
        Level::parse(room(6, {}, std::string(kZone) + "@ switch S1 16 20 kind=panel\n")));
      World w(level, 0, theme, art);
      idle(w, 4);
      Keys prev, shoot;
      shoot.fire = true;
      step(w, shoot, prev);
      idle(w, 14);
      check(w.grav().zones[0].dir == Grav::Up, "a shot throws a switch");
    }
    {
      // Two flips within 15 frames make the runner dizzy (a low chamber: the
      // runner stays on its switch).
      std::vector<std::pair<int, std::string>> roof;
      for (int y = 1; y <= 17; ++y)
        roof.push_back({y, std::string(38, '#')});
      const auto level = std::make_shared<const Level>(Level::parse(
        room(10, roof, "@ gravzone LOW rect=1,18,38,21 dir=down\n@ switch S1 11 20 kind=panel\n")));
      World w(level, 0, theme, art);
      idle(w, 4);
      Keys prev, up;
      up.up = true;
      int dizzy = 0;
      for (int f = 0; f < 16; ++f)
      {
        step(w, (f == 0 || f == 11) ? up : Keys{}, prev);
        dizzy = std::max(dizzy, w.grav().dizzy);
      }
      std::printf("     dizzy %d, zone %d, runner grav %d\n", dizzy, int(w.grav().zones[0].dir), int(w.player().grav));
      check(dizzy > 0 && w.player().grav == Grav::Down, "two flips within 15 frames: dizzy stars");
    }
    {
      // A Flip Walker falls when its chamber turns over: the whole height
      // of the chamber kills it.
      const auto level = std::make_shared<const Level>(Level::parse(
        room(4, {}, std::string(kZone) + "@ switch S1 4 20 kind=panel\n@ flip_walker FW 20 21\n")));
      World w(level, 0, theme, art);
      idle(w, 4);
      Keys prev, up;
      up.up = true;
      step(w, up, prev);
      idle(w, 40);
      std::printf("     walker alive %d y %d\n", int(w.enemies()[0].alive), w.enemies()[0].y);
      check(!w.enemies()[0].alive, "a Flip Walker's fall through the whole chamber kills it");
    }
    {
      // A Gravity Probe pulls the runner along the floor; Turbo does not.
      const auto level = std::make_shared<const Level>(
        Level::parse(room(10, {}, "@ gravity_probe GP 15 17\n")));
      for (bool turbo : {false, true})
      {
        World w(level, 0, theme, art);
        w.cheat(Cheat::God);
        if (turbo)
          w.cheat(Cheat::Turbo);
        const int x0 = w.player().x;
        idle(w, 40);
        std::printf("     pulled %d cells (turbo %d)\n", w.player().x - x0, int(turbo));
        check(turbo ? w.player().x == x0 : w.player().x > x0 + 4, turbo ? "Turbo ignores the pull" : "a probe pulls the runner in");
      }
    }
    {
      // A Test Subject crouches and copies the runner's jump.
      const auto level = std::make_shared<const Level>(Level::parse(
        room(4, {}, std::string(kZone) + "@ test_subject TS 20 21\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::God);
      idle(w, 2);
      const int y0 = w.enemies()[0].y;
      Keys prev, jump;
      jump.jump = true;
      int top = y0;
      bool crouched = false;
      for (int f = 0; f < 30; ++f)
      {
        step(w, f < 5 ? jump : Keys{}, prev);
        top = std::min(top, w.enemies()[0].y);
        crouched = crouched || w.enemies()[0].tell > 0;
      }
      std::printf("     subject rose %d cells\n", y0 - top);
      check(crouched && y0 - top >= 5, "a Test Subject copies the jump");
    }
    {
      // The Grav Grenade: hangs where it lands and pulls enemies in.
      const auto level = std::make_shared<const Level>(
        Level::parse(room(6, {}, "@ flip_walker FW 19 21\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      w.cheat(Cheat::God);
      idle(w, 2);
      Keys prev, shoot;
      shoot.fire = true;
      step(w, shoot, prev);
      bool hung = false;
      float vx = 0.0f;
      for (int f = 0; f < 20 && !hung; ++f)
      {
        idle(w, 1);
        for (const auto& v : w.grav().vortices)
        {
          hung = hung || v.flight < 0;
          vx = v.x;
        }
      }
      const int x0 = w.enemies()[0].x;
      idle(w, 10);
      std::printf("     vortex at %.1f\n", double(vx));
      std::printf("     vortex %d, walker %d -> %d\n", int(hung), x0, w.enemies()[0].x);
      check(hung, "the grenade hangs as a vortex");
      check(std::abs(w.enemies()[0].x + 1 - int(vx)) < std::abs(x0 + 1 - int(vx)) || !w.enemies()[0].alive, "and pulls the walker in");
      idle(w, 60);
      check(w.grav().vortices.empty(), "and pops");
    }
    {
      // A test gate opens once its subjects are down.
      const auto level = std::make_shared<const Level>(Level::parse(room(6, {}, std::string(kZone) +
        "@ test_subject TS1 14 21\n@ testgate TG 30 19 h=3 opens=TS1\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::God);
      w.cheat(Cheat::Ammo);
      check(w.map().block(30, 20) == Tile::Solid, "a test gate starts shut");
      Keys prev, shoot;
      shoot.fire = true;
      for (int f = 0; f < 200 && w.enemies()[0].alive; ++f)
        step(w, (f % 6) ? Keys{} : shoot, prev);
      idle(w, 2);
      check(!w.enemies()[0].alive && w.map().block(30, 20) == Tile::Empty, "and opens once its subject is down");
    }
    {
      // Gun Gravity: a shot to the right makes the right wall the floor.
      const auto level = std::make_shared<const Level>(Level::parse(
        room(10, {}, "", "weapon=\nrules=gun_gravity\ntimer=60\ngoal=collect:30\n")));
      World w(level, 0, theme, art);
      idle(w, 2);
      Keys prev, shoot;
      shoot.fire = true;
      step(w, shoot, prev);
      idle(w, 40);
      const CellBox b = w.player().box();
      std::printf("     gun gravity: grav %d box %d,%d %dx%d\n", int(w.player().grav), b.x, b.y, b.w, b.h);
      check(w.player().grav == Grav::Right && b.right() == 77 && w.player().state == PlayerState::OnGround,
        "a shot right: the right wall is the floor");
      // Walking along the wall.
      const int y0 = b.y;
      Keys right;
      right.right = true;
      for (int f = 0; f < 10; ++f)
        step(w, right, prev);
      std::printf("     along the wall %d -> %d\n", y0, w.player().box().y);
      check(w.player().box().y != y0, "and walking along it");
    }
    if (argc > 1)
    {
      // Gun Gravity: the bot gathers the gems from every wall.
      const auto level = std::make_shared<const Level>(Level::loadFile(argv[1]));
      World w(level, 0, theme, art);
      Bot bot;
      Input prev;
      for (int f = 0; f < 60 * 15 && w.state() == WorldState::Playing; ++f)
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
        if (std::getenv("GUN_DEBUG"))
          std::printf("f%d at %d,%d grav %d state %d gems %d world %d\n", f, w.player().x, w.player().y,
            int(w.player().grav), int(w.player().state), w.stats().gems, int(w.state()));
      }
      std::printf("     gun gravity: %d gems in %d frames\n", w.stats().gems, w.stats().frames);
      check(w.state() == WorldState::Exiting, "the bot gathers the gems with Gun Gravity");
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf("%s\n", gFailures == 0 ? "all grav tests passed" : "SOME TESTS FAILED");
  return gFailures == 0 ? 0 : 1;
}
