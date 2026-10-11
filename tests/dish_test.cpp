// Level 32's mechanics, played by hand in small rooms: the dishes' cycle
// (holds, turns, the groan 30 frames before each turn), a rim that carries
// its rider from the east pose to overhead, a strut solid only in its poses,
// the teaching dish turning only while the runner is near, a Scorpion Mech's
// aim line locking and its beam (a runner who moves on the lock is missed),
// the Signal Jammer turning a mech on the dummy so its beam breaks the door
// behind it, a Sand Shark breaching under a runner on the sand but only
// circling one on rock, a Vulture Drone's scrap (hurts; a carrier's infects;
// in Heatstroke it knocks back), the minidishes turning when shot, and
// Heatstroke's swapped controls. Then the bot clears Heatstroke with every
// runner.

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

// A 40 x 24 room in the desert: ceiling row 0, floor rows 22-23, walls at
// x 0 and 39, the runner at (px, py), plus `walls`, `entities` and extra
// header lines.
std::string room(int px, int py, const std::string& entities, const std::vector<Wall>& walls = {},
  const std::string& header = "weapon=signal_jammer\n")
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
  std::string s = "name=DISH TEST\nepisode=5\ntheme=desert_array\n" + header + "[map]\n";
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

const Enemy* firstOf(const World& w, EnemyKind kind)
{
  for (const auto& e : w.enemies())
    if (e.kind == kind)
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
    Keys none, fire, left, right, jump;
    fire.fire = true;
    jump.jump = true;
    left.left = true;
    right.right = true;

    auto make = [&](const std::string& text, int character = 0) {
      return World(std::make_shared<const Level>(Level::parse(text)), character, theme, art);
    };

    // The cycle: hold east 240, turn 45, hold overhead 240, turn 45, hold
    // west 240, swing back 90; the groan the 30 frames before each turn.
    {
      const DishPhase a = dishPhase(0, false), b = dishPhase(209, false), c = dishPhase(210, false),
                      d = dishPhase(240, false), e = dishPhase(300, false), f = dishPhase(495, false),
                      g = dishPhase(560, false), h = dishPhase(600, false), i = dishPhase(780, false),
                      j = dishPhase(850, false);
      check(a.n == 0 && a.from == DishPose::East && !a.groan && !b.groan && c.groan && d.n == 45 && d.k == 0 &&
          d.to == DishPose::Over && e.n == 0 && e.from == DishPose::Over && f.groan && g.from == DishPose::Over &&
          g.to == DishPose::West && h.from == DishPose::West && i.groan && j.n == 90 && j.to == DishPose::East,
        "the dishes hold, groan for 30 frames and turn: east, overhead, west and back");
    }
    {
      // A rim carries its rider from east to overhead.
      World w = make(room(11, 17, "@ platform r 10 18 w=4 mode=script path=10,18;12,14;14,18\n"));
      idle(w, 300);
      const Platform& pl = w.platforms()[0];
      const auto& p = w.player();
      check(pl.x == 24 && pl.y == 28 && p.y + 1 == pl.y && p.x >= 24 && p.x < 32,
        "a rim carries its rider from the east pose up to overhead");
    }
    {
      // A strut solid only overhead.
      World w = make(room(5, 21, "@ layer s rect=20,19,22,21 driver=script state=over style=strut\n"));
      idle(w, 100);
      const bool east = w.map().solid(40, 38);
      idle(w, 200);
      check(!east && w.map().solid(40, 38), "a strut is solid only in its poses");
    }
    {
      // The teaching dish turns only while the runner is within 6 blocks.
      const std::string d = "@ dish D0 30 18 small=1 radius=6\n"
                            "@ platform r 28 18 w=4 mode=script path=28,18;29,16;30,18 dish=D0\n";
      World far = make(room(5, 21, d));
      World near = make(room(26, 21, d));
      idle(far, 100);
      idle(near, 100);
      check(far.dish().small[0].t == 0 && near.dish().small[0].t == 100,
        "the teaching dish turns only while the runner is near");
    }
    {
      // A Scorpion Mech's aim line turns, locks for 8 frames, then the beam;
      // standing still it hits, jumping on the lock it misses.
      int hit[2] = {0, 0};
      for (int dodge = 0; dodge < 2; ++dodge)
      {
        World w = make(room(20, 21, "@ scorpion_mech 10 21 dir=r\n"));
        const int hp0 = w.player().hp;
        Keys prev;
        int f = 0;
        bool locked = false;
        for (; f < 120 && w.dish().beams.empty(); ++f)
        {
          locked = locked || w.dish().robots[0].aimState == 2;
          step(w, dodge && locked ? jump : none, prev);
        }
        hit[dodge] = hp0 - w.player().hp;
      }
      check(hit[0] == 1 && hit[1] == 0, "a Scorpion Mech's beam hits a runner who stands still, not one who jumps on the lock");
    }
    {
      // Jammed, a mech fires on the dummy and its beam breaks the door behind.
      World w = make(room(18, 21, "@ scorpion_mech 12 21\n@ dummy 4 20\n@ breakable rect=3,18,3,21 hp=1 by=beam\n",
        {{3, 18, 3, 21}}));
      w.cheat(Cheat::Ammo);
      Keys prev;
      step(w, left, prev);
      idle(w, 2);
      const int hp0 = w.player().hp;
      tap(w, fire, prev);
      const bool jammed = w.dish().robots[0].jam > 0;
      idle(w, 70);
      check(jammed && w.breakables()[0].broken && w.player().hp == hp0,
        "the Jammer turns a mech on the dummy and its beam breaks the door");
    }
    {
      // A Sand Shark breaches under a runner on the sand, and only circles a
      // runner on rock.
      const std::string sand = "@ sand rect=1,22,38,22\n@ sand_shark 8 21\n";
      World on = make(room(16, 21, sand));
      const int hp0 = on.player().hp;
      bool hidden = true;
      for (int f = 0; f < 80 && on.player().hp == hp0; ++f)
      {
        idle(on, 1);
        hidden = hidden && (on.player().hp != hp0 || firstOf(on, EnemyKind::SandShark)->hidden ||
                             on.dish().sharks[0].state == 2);
      }
      World rock = make(room(27, 19, "@ sand rect=1,22,38,22\n@ sand_shark 20 21\n", {{25, 20, 30, 21}}));
      const int x0 = firstOf(rock, EnemyKind::SandShark)->x;
      idle(rock, 200);
      check(on.player().hp < hp0 && hidden && rock.player().hp == hp0 && firstOf(rock, EnemyKind::SandShark)->x != x0,
        "a Sand Shark breaches under a runner on the sand and circles one on rock");
    }
    {
      // A Vulture Drone's scrap hurts; a carrier's infects; Heatstroke's
      // knocks the runner back 2 blocks instead.
      World w = make(room(20, 21, "@ vulture_drone 24 4 zone=10,30\n"));
      World c = make(room(20, 21, "@ vulture_drone 24 4 zone=10,30 carrier=1\n"));
      World h = make(room(20, 21, "@ vulture_drone 24 4 zone=10,30\n", {}, "rules=swap_lr\ntimer=60\ngoal=exit\n"));
      const int hp0 = w.player().hp, x0 = h.player().x;
      idle(w, 90);
      idle(c, 90);
      idle(h, 90);
      check(w.player().hp < hp0 && c.player().virus > 0 && c.player().hp == hp0 && h.player().hp == hp0 &&
          std::abs(h.player().x - x0) >= 4,
        "a Vulture Drone's scrap hurts, a carrier's infects, Heatstroke's knocks back");
    }
    {
      // The minidishes turn a pose a shot.
      World w = make(room(10, 21, "@ minidish 16 21\n", {}, ""));
      Keys prev;
      tap(w, fire, prev);
      idle(w, 20);
      const int one = w.dish().minis[0].pose;
      idle(w, 10);
      tap(w, fire, prev);
      idle(w, 20);
      check(one == 1 && w.dish().minis[0].pose == 2, "a minidish turns a pose with each shot");
    }
    {
      // Heatstroke: after 150 frames right walks left.
      World w = make(room(20, 21, "", {}, "rules=swap_lr\ntimer=60\ngoal=exit\n"));
      Keys prev;
      hold(w, right, 10, prev);
      const int x1 = w.player().x;
      hold(w, none, 145, prev);
      const int x2 = w.player().x;
      hold(w, right, 10, prev);
      check(x1 > 40 && w.dish().swapped && w.player().x < x2, "Heatstroke swaps left and right every 150 frames");
    }

    const auto bonus = std::make_shared<const Level>(Level::loadFile(argv[1]));
    for (int character = 0; character < 6; ++character)
    {
      World w(bonus, character, theme, art);
      Bot bot;
      const int f = botPlay(w, 900, bot);
      std::printf("     runner %d: %d frames\n", character, f);
      check(w.state() != WorldState::Playing && !w.bonusFailed(), "the bot crosses Heatstroke to the mast");
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf(gFailures == 0 ? "all dish tests passed\n" : "%d dish test(s) failed\n", gFailures);
  return gFailures == 0 ? 0 : 1;
}
