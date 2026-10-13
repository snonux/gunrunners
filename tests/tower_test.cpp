// Level 35's mechanics, played by hand in small rooms: the tower turning
// round a runner who walks a full lap and more, a Relay Drone circling
// the mast, the doors, a Feedback Node's push (half for Rocco), a Mast
// Climber that waits for its ladder, the Satellite Swarm's volley and a
// drone soaking up a shot, the Halcyon Titan's arrival and the exit that
// only shows once it is down, and a save made in a turned tower. Then the
// bot beats the Titan on a ring deck and clears Copycat with every runner.

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

// A 64 x h ring of the tower (flags=wrapx): floor rows h-2 and h-1 all the
// way round, the runner at (px, py), plus `walls` and `entities`.
std::string ring(int px, int py, const std::string& entities, const std::vector<Wall>& walls = {}, int h = 24,
  const std::string& extra = "")
{
  std::vector<std::string> map(std::size_t(h), std::string(64, '.'));
  for (int x = 0; x < 64; ++x)
    map[std::size_t(h - 2)][std::size_t(x)] = map[std::size_t(h - 1)][std::size_t(x)] = '#';
  for (const auto& w : walls)
    for (int y = w.y0; y <= w.y1; ++y)
      for (int x = w.x0; x <= w.x1; ++x)
        map[std::size_t(y)][std::size_t(x)] = w.c;
  map[std::size_t(py)][std::size_t(px)] = 'P';
  std::string s = "name=TOWER TEST\nepisode=5\ntheme=broadcast_mast\nweapon=satellite_swarm\nflags=wrapx\n" + extra + "[map]\n";
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

const Enemy* firstOf(const World& w, EnemyKind kind)
{
  for (const auto& e : w.enemies())
    if (e.alive && e.kind == kind)
      return &e;
  return nullptr;
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
    Keys none, right, left, up, fire;
    right.right = true;
    left.left = true;
    up.up = true;
    fire.fire = true;

    auto make = [&](const std::string& text, int character = 0) {
      return World(std::make_shared<const Level>(Level::parse(text)), character, theme, art);
    };
    const int W = 128; // cells round the ring

    {
      // Walking one and a half laps: the tower turns, the runner stays in
      // the middle on the floor, and the walk is counted.
      World w = make(ring(5, 21, "@ deco 0 20 kind=42\n"));
      const int mid = W / 2 - 1;
      Keys prev;
      bool centred = true, floor = true;
      for (int f = 0; f < 200; ++f)
      {
        step(w, right, prev);
        const auto& p = w.player();
        centred = centred && std::abs(p.x - mid) <= kTowerKeep + 2;
        floor = floor && p.state == PlayerState::OnGround;
      }
      check(w.tower().turns > 3 && centred && floor, "the tower turns round a runner walking a lap and a half");
      check(w.tower().walked >= 190, "the laps walked are counted");
      check(w.map().solidTop(w.player().x + 1, w.player().y + 1), "the floor under the runner is still there");
      // ...and a gem behind comes round again to be picked up.
      World g = make(ring(30, 21, "", {{28, 21, 28, 21, 'g'}}));
      Keys gp;
      const int gems = g.stats().gems;
      hold(g, right, W + 8, gp);
      check(g.stats().gems == gems + 1, "a gem left behind comes round again a lap later");
    }
    {
      // A Relay Drone circles the mast and stays on the ring as it goes round.
      World w = make(ring(30, 21, "@ relay_drone 40 10\n"));
      bool near = true;
      for (int f = 0; f < 300; ++f)
      {
        idle(w, 1);
        const Enemy* e = firstOf(w, EnemyKind::RelayDrone);
        near = near && e && std::abs(e->x - w.player().x) <= W / 2 + 1;
      }
      check(near && w.tower().relays.size() == 1, "a Relay Drone circles the mast and stays on the ring");
    }
    {
      // A door: up takes the runner to its twin, on its floor.
      World w = make(ring(10, 21, "@ door a 10 21 to=b\n@ door b 40 11 to=a\n", {{30, 12, 50, 12, '='}}));
      Keys prev;
      step(w, up, prev);
      step(w, none, prev);
      const auto& p = w.player();
      const auto& b = w.tower().doors[1];
      check(std::abs(w.towerDx(b.x, p.x)) <= 1 && p.y == b.y && p.state == PlayerState::OnGround,
        "up at a door comes out at its twin");
    }
    for (int character : {0, 1})
    {
      // A Feedback Node glows, then pushes the runner away along the floor
      // (Rocco half as far).
      World w = make(ring(26, 21, "@ feedback_node 30 21\n"), character);
      const Enemy* node = firstOf(w, EnemyKind::FeedbackNode);
      const int d0 = w.towerDx(node->x, w.player().x);
      idle(w, 40);
      const int moved = d0 - w.towerDx(node->x, w.player().x);
      std::printf("     runner %d pushed %d cells\n", character, moved);
      check(character == 1 ? (moved >= 3 && moved <= 5) : (moved >= 7 && moved <= 9),
        character == 1 ? "a Feedback Node pushes Rocco 4 cells" : "a Feedback Node pushes the runner 8 cells");
    }
    {
      // A Mast Climber waits, out of sight, until the runner climbs its ladder.
      World w = make(ring(21, 21, "@ spawn 16 21 kind=mast_climber ladder=22\n", {{22, 12, 22, 21, 'H'}, {10, 12, 21, 12, '='}}));
      const Enemy* e = firstOf(w, EnemyKind::MastClimber);
      const bool hidden = e && e->hidden;
      Keys prev;
      hold(w, right, 1, prev);
      hold(w, up, 6, prev);
      e = firstOf(w, EnemyKind::MastClimber);
      check(hidden && e && !e->hidden && w.player().state == PlayerState::Ladder,
        "a Mast Climber sets off once the runner climbs its ladder");
    }
    {
      // The Satellite Swarm: three drones fire with the runner.
      World w = make(ring(20, 21, ""));
      w.cheat(Cheat::Ammo);
      idle(w, 2);
      const std::size_t shots0 = w.projectiles().size();
      Keys prev;
      step(w, fire, prev);
      int swarm = 0;
      for (const auto& pr : w.projectiles())
        swarm += pr.alive && pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::SatelliteSwarm);
      check(shots0 == 0 && swarm >= 3, "the Satellite Swarm's drones fire with the runner");
      // A Relay Drone's shot at the runner: a drone soaks it up and goes offline.
      World v = make(ring(20, 21, "@ relay_drone 26 19\n"));
      v.cheat(Cheat::Ammo);
      v.cheat(Cheat::God);
      bool offline = false;
      for (int f = 0; f < 200 && !offline; ++f)
      {
        idle(v, 1);
        for (const auto& d : v.tower().swarm)
          offline = offline || d.offline > 0;
      }
      check(offline, "a Swarm drone soaks up an enemy shot and goes offline");
    }
    {
      // The Halcyon Titan: delivered once the runner is on the deck; the
      // exit only shows once it is down.
      World w = make(ring(20, 21, "@ titan 40 21\n", {}, 24, ""));
      const bool waiting = !w.titanFight() && !w.exitPowered();
      idle(w, 2);
      const bool intro = w.tower().titan.phase == TitanPhase::Intro;
      idle(w, kTitanIntro + 2);
      check(waiting && intro && w.tower().titan.phase == TitanPhase::Demo && w.titanHp() == 92,
        "the Halcyon Titan is delivered onto the deck");
      check(!w.exitPowered(), "no way out while the Titan stands");
    }
    {
      // A save made in a turned tower loads with the tower turned the same.
      World w = make(ring(5, 21, "@ deco 0 20 kind=42\n"));
      Keys prev;
      hold(w, right, 90, prev);
      const SaveGame s = w.snapshot();
      World v = make(ring(5, 21, "@ deco 0 20 kind=42\n"));
      const bool ok = v.restore(s);
      check(ok && v.tower().rot == w.tower().rot && v.player().x == w.player().x &&
          v.map().solidTop(v.player().x + 1, v.player().y + 1),
        "a save in a turned tower loads turned the same");
    }
    for (int character = 0; character < 6; ++character)
    {
      // The bot beats the Titan on a ring deck.
      World w = make(ring(20, 21, "@ titan 40 21\n", {}, 24), character);
      Bot bot;
      int f = 0;
      while (f < 4500 && w.tower().titan.phase != TitanPhase::Done && w.player().state != PlayerState::Dying)
        f += botPlay(w, 1, bot);
      std::printf("     runner %d: %d frames, %d hearts left\n", character, f, w.player().hp);
      check(w.tower().titan.phase == TitanPhase::Done, "the bot beats the Halcyon Titan");
    }

    // Copycat: the dummies copy the runner's walk at half speed, and one
    // walked into a pit is caught.
    const auto bonus = std::make_shared<const Level>(Level::loadFile(argv[1]));
    {
      World w(bonus, 0, theme, art);
      const Enemy* e = firstOf(w, EnemyKind::MastClimber);
      const int x0 = e ? e->x : -1;
      Keys prev;
      hold(w, right, 8, prev);
      e = firstOf(w, EnemyKind::MastClimber);
      check(e && e->x - x0 == 4, "a dummy copies the runner's walk at half speed");
    }
    for (int character = 0; character < 6; ++character)
    {
      World w(bonus, character, theme, art);
      Bot bot;
      const int f = botPlay(w, 900, bot);
      std::printf("     runner %d: %d frames, %d caught\n", character, f, w.tower().caught);
      check(w.state() != WorldState::Playing && !w.bonusFailed(), "the bot clears Copycat");
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf(gFailures == 0 ? "all tower tests passed\n" : "%d tower test(s) failed\n", gFailures);
  return gFailures == 0 ? 0 : 1;
}
