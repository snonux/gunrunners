// Level 20's Core Pulse and Deflector Bracer, played by hand in small rooms:
// a ring in the open and in a booth, the Bracer soaking a pulse and its arc,
// a pulse shot, a shot bounced back, a valve and the lift cage, a Shield
// Drone's bubble, an Isotope Imp speeding up, the coolant drip, a Conduit
// Spark on its wire, and Stop Motion. Then the Stop Motion bonus, played by
// the bot.

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
  std::string s = "name=REACTOR TEST\nepisode=3\ntheme=station_reactor\nweapon=deflector_bracer\n" + header + "[map]\n";
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

const char* kPulse = "@ pulse 20 12 period=30 damage=2\n";

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
      // A ring costs two hearts in the open; in a booth, none.
      const auto open = std::make_shared<const Level>(Level::parse(room(10, {}, kPulse)));
      World w(open, 0, theme, art);
      const int hp = w.player().hp;
      int countdown = w.pulseCountdown();
      idle(w, 40);
      std::printf("     hp %d -> %d, counter %d\n", hp, w.player().hp, countdown);
      check(w.player().hp == hp - 2, "a pulse in the open: two hearts");
      check(countdown == 10, "the counter starts at 10");
      const auto booth =
        std::make_shared<const Level>(Level::parse(room(10, {}, std::string(kPulse) + "@ shield rect=8,18,12,21\n")));
      World b(booth, 0, theme, art);
      idle(b, 40);
      check(b.inBooth() && b.player().hp == hp, "in a lead booth: none");
    }
    {
      // The Bracer raised as the ring comes soaks it and throws it back as an
      // arc (3 damage to the drone in its way).
      const auto level = std::make_shared<const Level>(
        Level::parse(room(10, {}, std::string(kPulse) + "@ shield_drone SD 22 20\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      const int hp = w.player().hp, ammo = w.player().ammo;
      idle(w, 22);
      Keys prev, hold;
      hold.fire = true;
      bool up = false, arc = false;
      for (int f = 0; f < 16; ++f)
      {
        step(w, hold, prev);
        up = up || w.bracerUp();
        arc = arc || !w.reactor().arcs.empty();
      }
      idle(w, 10);
      std::printf("     hp %d -> %d, ammo %d -> %d, drone hp %d\n", hp, w.player().hp, ammo, w.player().ammo,
        w.enemies()[0].hp);
      check(up && w.player().ammo == ammo - 1, "holding fire raises the shield (one ammo)");
      check(w.player().hp == hp, "the shield soaks the pulse");
      check(arc && w.enemies()[0].hp == 3, "and throws it back as an arc");
    }
    {
      // A tap fires a pulse shot that knocks an imp back a block (an imp is
      // low: crouch for it).
      const auto level = std::make_shared<const Level>(Level::parse(room(10, {}, "@ isotope_imp II 17 21\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      w.cheat(Cheat::God);
      const int ammo = w.player().ammo;
      Keys prev, tap, crouch;
      tap.fire = tap.down = crouch.down = true;
      step(w, crouch, prev);
      step(w, tap, prev);
      step(w, crouch, prev);
      bool shot = false;
      for (const auto& pr : w.projectiles())
        shot = shot || (pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::DeflectorBracer));
      check(shot && w.player().ammo == ammo - 1, "a tap fires a pulse shot");
      int hpBefore = w.enemies()[0].hp, xBefore = w.enemies()[0].x, pushed = 0;
      for (int f = 0; f < 12 && w.enemies()[0].hp == hpBefore; ++f)
      {
        xBefore = w.enemies()[0].x;
        step(w, crouch, prev);
        if (w.enemies()[0].hp < hpBefore)
          pushed = w.enemies()[0].x - xBefore;
      }
      std::printf("     imp hp %d, pushed %d\n", w.enemies()[0].hp, pushed);
      check(w.enemies()[0].hp == hpBefore - 1 && pushed >= 1, "and knocks it back");
    }
    {
      // A turret's shots bounce off the raised shield and come back at it.
      const auto level = std::make_shared<const Level>(Level::parse(room(10, {{21, "                  t"}}, "")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      const int hp = w.player().hp;
      Keys prev, hold, right;
      hold.fire = true;
      right.right = true;
      step(w, right, prev);
      for (int f = 0; f < 44; ++f)
        step(w, hold, prev);
      int turret = -1;
      for (std::size_t i = 0; i < w.enemies().size(); ++i)
        if (w.enemies()[i].kind == EnemyKind::Turret)
          turret = int(i);
      std::printf("     hp %d -> %d, turret hp %d\n", hp, w.player().hp, turret >= 0 ? w.enemies()[std::size_t(turret)].hp : -1);
      check(w.player().hp == hp && turret >= 0 && w.enemies()[std::size_t(turret)].hp < 4,
        "shots bounce off the shield and hurt the turret");
    }
    {
      // Holding up at the last valve stops the core and opens the lift cage.
      const auto level = std::make_shared<const Level>(Level::parse(room(
        12, {}, std::string("@ pulse 30 12 period=150\n@ valve V1 12 21\n@ liftcage rect=2,18,6,21\n"))));
      World w(level, 0, theme, art);
      check(w.map().block(6, 20) == Tile::Solid && !w.reactor().cageOpen, "the lift cage starts shut");
      Keys prev, up;
      up.up = true;
      for (int f = 0; f < 20; ++f)
        step(w, up, prev);
      step(w, Keys{}, prev);
      check(!w.reactor().valves[0].shut, "letting go too soon: the valve stays open");
      for (int f = 0; f < 31; ++f)
        step(w, up, prev);
      check(w.reactor().valves[0].shut && w.reactor().stopped >= 0, "30 frames held: shut, the core stops");
      check(w.reactor().cageOpen && w.map().block(6, 20) == Tile::Empty && w.pulseCountdown() == -1,
        "the cage opens, the counter goes dark");
    }
    {
      // Nothing in a Shield Drone's bubble can be hurt.
      const auto level = std::make_shared<const Level>(
        Level::parse(room(4, {}, "@ shield_drone SD 15 17 guard=II\n@ isotope_imp II 15 21\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::God);
      Keys prev, shoot, crouch;
      shoot.fire = shoot.down = crouch.down = true;
      int shimmer = 0;
      for (int f = 0; f < 40; ++f)
      {
        step(w, (f % 4) ? crouch : shoot, prev);
        shimmer = std::max(shimmer, w.reactor().drones[0].shimmer);
      }
      int imp = -1;
      for (std::size_t i = 0; i < w.enemies().size(); ++i)
        if (w.enemies()[i].kind == EnemyKind::Imp)
          imp = int(i);
      std::printf("     imp hp %d, shimmer %d\n", imp >= 0 ? w.enemies()[std::size_t(imp)].hp : -1, shimmer);
      check(imp >= 0 && w.enemies()[std::size_t(imp)].alive && w.enemies()[std::size_t(imp)].hp == 3 && shimmer > 0,
        "an imp in the drone's bubble takes no damage");
    }
    {
      // An Isotope Imp gets faster for every pulse it lives through.
      const auto level =
        std::make_shared<const Level>(Level::parse(room(4, {}, std::string(kPulse) + "@ isotope_imp II 14 21\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::God);
      idle(w, 70);
      std::printf("     imp pulses %d\n", w.enemies()[0].attach);
      check(w.enemies()[0].attach == 2, "an imp speeds up after each pulse");
    }
    {
      // The coolant drip infects.
      const auto level = std::make_shared<const Level>(Level::parse(room(10, {}, "@ drip 10 15 period=20\n")));
      World w(level, 0, theme, art);
      idle(w, 40);
      check(w.player().virus > 0, "a coolant drop infects");
    }
    {
      // A Conduit Spark rides its loop.
      const auto level = std::make_shared<const Level>(Level::parse(
        room(4, {}, "@ wire W path=20,21;30,21;30,14;20,14;20,21\n@ conduit_spark CS 20 21 wire=W\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::God);
      int top = 1000, right = 0;
      for (int f = 0; f < 60; ++f)
      {
        idle(w, 1);
        top = std::min(top, w.enemies()[0].y);
        right = std::max(right, w.enemies()[0].x);
      }
      std::printf("     spark reached x %d, y %d\n", right, top);
      check(right == 60 && top == 29, "a spark rides its wire round");
    }
    {
      // Stop Motion: standing still, nothing moves; walking, it all does.
      const auto level = std::make_shared<const Level>(Level::parse(
        room(4, {}, std::string(kPulse) + "@ isotope_imp II 30 21\n", "rules=stop_motion\ntimer=90\ngoal=exit\n")));
      World w(level, 0, theme, art);
      idle(w, 3);
      const int clock = w.reactor().pulses[0].clock, ix = w.enemies()[0].x;
      idle(w, 40);
      check(w.reactor().pulses[0].clock == clock && w.enemies()[0].x == ix, "standing still: the world stands still");
      Keys prev, left;
      left.left = true;
      for (int f = 0; f < 10; ++f)
        step(w, left, prev);
      check(w.reactor().pulses[0].clock == clock + 10, "moving: it moves");
    }
    if (argc > 1)
    {
      // Stop Motion: the bot gets to the exit.
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
      std::printf("     stop motion: exit in %d frames\n", w.stats().frames);
      check(w.state() == WorldState::Exiting, "the bot gets through Stop Motion");
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf("%s\n", gFailures == 0 ? "all reactor tests passed" : "SOME TESTS FAILED");
  return gFailures == 0 ? 0 : 1;
}
