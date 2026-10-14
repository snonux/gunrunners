// Level 40's mechanics, played by hand in small rooms: a pyro cue burns
// (and its column lifts the runner); a truss flies in on cue with the runner
// on it and waits for one under it; a confetti blast pushes everyone but
// Rocco; a followspot locks on and a Rigger shoots; a sandbag breaks a
// catwalk that is hung again; the Pyro Rig's stream and jetpack; the Q99
// fuse; the towel; one button in Trigger Happy. Then the bot clears the Fly
// Tower and Trigger Happy with every runner.

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
  const std::string& extra = "weapon=pyro_rig\n")
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
  std::string s = "name=FLY TOWER TEST\nepisode=6\ntheme=fly_tower\n" + extra + "[map]\n";
  for (const auto& row : map)
    s += row + "\n";
  s += "[entities]\n" + entities;
  return s;
}

// A cue list over the whole room.
std::string sheet(int period) { return "@ cuelist L zone=0,0,39,23 period=" + std::to_string(period) + "\n"; }

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
  const bool trace = std::getenv("GR_FLY_TRACE") != nullptr;
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
    if (trace)
    {
      const auto& pl = w.player();
      std::printf("f%d %d,%d st%d hp%d route%d in %c%c%c%c%c%c\n", f, pl.x, pl.y, int(pl.state), pl.hp,
                  w.flyTower().routeAt, in.left ? 'L' : '.', in.right ? 'R' : '.', in.up ? 'U' : '.', in.down ? 'D' : '.',
                  in.jump ? 'J' : '.', in.fire ? 'F' : '.');
    }
    w.update(p);
  }
  return f;
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
    Keys right, left, fire, jump, jet;
    right.right = true;
    left.left = true;
    fire.fire = true;
    jump.jump = true;
    jet.down = true;
    jet.fire = true;
    auto make = [&](const std::string& text, int character = 0) {
      return World(std::make_shared<const Level>(Level::parse(text)), character, theme, art);
    };
    {
      // A pyro cue: the nozzle glows, then the column burns the runner in it.
      World w = make(room(10, 21, "@ pyro PY 10 21 h=8\n" + sheet(200) +
                                    "@ cue C list=L t=20 kind=pyro target=PY label=TEST\n"));
      idle(w, 2);
      const auto& py = w.flyTower().pyros[0];
      const bool cold = w.pyroHeat(py, 0) == 0;
      idle(w, 20 - kFtGlow);
      const bool glow = w.pyroHeat(py, kFtGlow) == 1 && w.pyroHeat(py, 0) == 0;
      const int hp = w.player().hp;
      idle(w, kFtGlow + 2);
      std::printf("     hp %d -> %d\n", hp, w.player().hp);
      check(cold && glow, "a pyro nozzle glows before its cue");
      check(w.pyroHeat(py, 0) == 2 && w.player().hp < hp, "on its cue the column burns the runner");
    }
    {
      // A burning column lifts a jump higher (an updraft).
      auto top = [&](bool burning) {
        World w = make(room(10, 21, "@ pyro PY 10 21 h=10\n" + sheet(200) + "@ cue C list=L t=" +
                                      std::string(burning ? "3" : "150") + " kind=pyro target=PY dur=60\n"));
        w.cheat(Cheat::God); // unhurt, so the jump is the same
        Keys prev;
        idle(w, 4);
        tap(w, jump, prev);
        int best = w.player().y;
        for (int i = 0; i < 30; ++i)
        {
          idle(w, 1);
          best = std::min(best, w.player().y);
        }
        return best;
      };
      const int cold = top(false), hot = top(true);
      std::printf("     jump top %d, in the column %d\n", cold, hot);
      check(hot < cold, "a burning column lifts the runner");
    }
    {
      // A truss flies in on cue with the runner on it.
      World w = make(room(21, 20, "@ platform T 20 21 w=4 mode=truss stops=21,12\n" + sheet(300) +
                                    "@ cue C list=L t=10 kind=truss target=T stop=1\n"));
      idle(w, 3);
      const int y0 = w.player().y;
      const bool home = w.trussHome(w.flyTower().trusses[0]);
      idle(w, 4);
      const bool still = w.player().y == y0;
      idle(w, 30);
      std::printf("     runner %d -> %d\n", y0, w.player().y);
      check(home && still, "a truss waits for its cue");
      check(w.player().y == 23 && w.player().state == PlayerState::OnGround, "on its cue it flies in with the runner");
    }
    {
      // A truss flying out never comes down on the runner: it waits.
      World w = make(room(21, 21, "@ platform T 20 12 w=4 mode=truss stops=12,21\n" + sheet(300) +
                                    "@ cue C list=L t=2 kind=truss target=T stop=1\n"));
      idle(w, 40);
      const auto& t = w.flyTower().trusses[0];
      const int hp = w.player().hp;
      check(t.waiting && w.player().hp == hp && w.player().state == PlayerState::OnGround,
            "a truss waits over the runner's head");
      Keys prev;
      hold(w, right, 12, prev);
      idle(w, 12);
      check(!w.flyTower().trusses[0].waiting && w.platforms()[0].y == 42, "and comes down once they are out");
    }
    {
      // Confetti: the blast pushes the runner along its lane; Rocco stands firm.
      const std::string ents = "@ confetti CF 30 21 dir=l len=12\n" + sheet(200) +
        "@ cue C list=L t=5 kind=confetti target=CF\n";
      World w = make(room(26, 21, ents));
      const int x0 = w.player().x;
      idle(w, 5 + kFtConfettiPush + 2);
      World rocco = make(room(26, 21, ents), 1);
      const int r0 = rocco.player().x;
      idle(rocco, 5 + kFtConfettiPush + 2);
      std::printf("     pushed %d -> %d, Rocco %d -> %d\n", x0, w.player().x, r0, rocco.player().x);
      check(w.player().x < x0 - 10, "a confetti blast pushes the runner");
      check(rocco.player().x == r0, "Rocco isn't pushed");
    }
    {
      // A followspot locks on (its beam turns amber), and a Rigger fires at the lit runner.
      World w = make(room(10, 21, "@ followspot FS 20 12\n@ rigger 18 21\n" + sheet(300) +
                                    "@ cue C list=L t=0 kind=spot target=FS on=1\n"));
      bool lit = false, shot = false;
      for (int i = 0; i < 120 && !(lit && shot); ++i)
      {
        idle(w, 1);
        lit = lit || w.flyTower().lit;
        for (const auto& pr : w.projectiles())
          shot = shot || (pr.alive && pr.kind == ShotKind::Enemy);
      }
      check(lit, "a followspot locks onto the runner");
      check(shot, "a Rigger shoots at the runner");
      // The blackout cue: the lamp off, nobody lit.
      World dark = make(room(10, 21, "@ followspot FS 20 12\n" + sheet(300) +
                                       "@ cue C list=L t=0 kind=spot target=FS on=0\n"));
      bool any = false;
      for (int i = 0; i < 60; ++i)
      {
        idle(dark, 1);
        any = any || dark.flyTower().lit;
      }
      check(!any, "a blackout cue switches it off");
    }
    {
      // A Sandbag Drop cuts a bag over the runner on the catwalk below: it
      // breaks the catwalk, which is hung again later.
      World w = make(room(20, 15, "@ sandbag 20 9\n", {{2, 10, 37, 10, '='}, {2, 16, 37, 16, '='}}));
      int gaps = 0;
      for (int i = 0; i < 120 && gaps == 0; ++i)
      {
        idle(w, 1);
        gaps = int(w.flyTower().gaps.size());
      }
      check(gaps > 0, "a sandbag breaks the catwalk");
      Keys prev;
      hold(w, left, 30, prev);
      idle(w, kFtRehang + 5);
      bool hung = true;
      for (int bx = 2; bx <= 37; ++bx)
        hung = hung && w.map().block(bx, 16) == Tile::Platform;
      check(hung && w.flyTower().gaps.empty(), "the catwalk is hung again");
    }
    {
      // The Pyro Rig: its stream burns a crate and costs a unit every 2 frames.
      World w = make(room(10, 21, "@ breakable rect=14,20,14,21 hp=4 by=any\n", {{14, 20, 14, 21}}));
      w.cheat(Cheat::Ammo);
      Keys prev;
      const int ammo = w.player().ammo;
      hold(w, fire, 20, prev);
      std::printf("     ammo %d -> %d, block %d\n", ammo, w.player().ammo, int(w.map().block(14, 21)));
      check(w.player().ammo == ammo - 10, "the stream costs a unit every 2 frames");
      check(w.map().block(14, 21) == Tile::Empty, "the stream burns a crate");
    }
    {
      // The jetpack: down and fire; in a burning column it is free (and lifted).
      auto climb = [&](bool burning, int& used) {
        World w = make(room(10, 21, "@ pyro PY 10 21 h=10\n" + sheet(300) + "@ cue C list=L t=" +
                                      std::string(burning ? "2" : "200") + " kind=pyro target=PY dur=60\n"));
        w.cheat(Cheat::Ammo);
        Keys prev;
        idle(w, 4);
        const int ammo = w.player().ammo, y0 = w.player().y;
        hold(w, jet, 10, prev);
        used = ammo - w.player().ammo;
        return y0 - w.player().y;
      };
      int usedCold = 0, usedHot = 0;
      const int cold = climb(false, usedCold), hot = climb(true, usedHot);
      std::printf("     jetpack up %d for %d ammo, in a column up %d for %d\n", cold, usedCold, hot, usedHot);
      check(cold > 4 && usedCold == 5, "the Pyro Rig's jetpack burns a unit every 2 frames");
      check(hot > cold && usedHot == 0, "in a burning column it is free and twice as fast");
    }
    {
      // Q99: two hits on the cut cannon's fuse box (behind its nozzle) and it
      // fires, burning the wall down.
      World w = make(room(30, 21, "@ breakable rect=10,18,10,21 hp=6 by=any\n@ pyro Q99 16 21 h=6 w=2 dir=l fuse=2\n",
                          {{10, 18, 10, 21}}, "weapon=normal\n"));
      w.cheat(Cheat::Ammo);
      Keys prev;
      hold(w, left, 2, prev);
      tap(w, fire, prev);
      idle(w, 12);
      const bool one = w.flyTower().pyros[0].fuse == 1 && w.flyTower().pyros[0].fuseT < 0;
      tap(w, fire, prev);
      idle(w, 12);
      const bool lit = w.flyTower().pyros[0].fuseT >= 0;
      idle(w, kFtGlow + 10);
      check(one && lit, "two hits light the Q99 fuse");
      check(w.map().block(10, 21) == Tile::Empty, "the cut cue burns the wall down");
    }
    {
      // The towel: cue THE ANSWER drops it; a touch is a full heal, once a life.
      World w = make(room(10, 21, "@ towel 20 4 land=21\n@ pyro PY 10 21 h=8\n" + sheet(300) +
                                    "@ cue C list=L t=2 kind=pyro target=PY dur=10\n" +
                                    "@ cue Q42 list=L t=5 kind=towel once=1 label=\"THE ANSWER\"\n"));
      idle(w, 5 + 40);
      const int hp = w.player().hp;
      Keys prev;
      hold(w, right, 30, prev);
      std::printf("     hp %d -> %d of %d\n", hp, w.player().hp, w.player().maxHp);
      check(w.flyTower().towelY > 0 && w.player().hp == w.player().maxHp && hp < w.player().maxHp,
            "the towel heals the runner");
    }
    {
      // Trigger Happy: a press on the ground jumps and fires; in the air it fires.
      World w = make(room(10, 21, "@ target 20 17\n", {}, "rules=jumpfire\ntimer=60\ngoal=collect:1\n"));
      Keys prev;
      tap(w, fire, prev);
      const bool jumped = w.player().state == PlayerState::Jumping;
      int shots = 0;
      for (const auto& pr : w.projectiles())
        shots += pr.alive && pr.kind != ShotKind::Enemy ? 1 : 0;
      check(jumped && shots == 1, "a press on the ground jumps and fires");
      // Level with the disc, another press hits it.
      for (int i = 0; i < 12 && w.player().y - 2 > 37; ++i)
        step(w, Keys{}, prev);
      tap(w, fire, prev);
      idle(w, 30);
      std::printf("     discs %d\n", w.flyTower().hits);
      check(w.flyTower().hits == 1, "a press in mid-air shoots a disc");
    }

    // The bot clears the Fly Tower and Trigger Happy with every runner.
    const auto level = std::make_shared<const Level>(Level::loadFile(argv[1]));
    const auto bonus = std::make_shared<const Level>(Level::loadFile(argv[2]));
    const int only = std::getenv("GR_FLY_RUNNER") ? std::atoi(std::getenv("GR_FLY_RUNNER")) : -1;
    const bool skipLevel = std::getenv("GR_FLY_BONUS_ONLY") != nullptr;
    for (int character = 0; character < 6 && !skipLevel; ++character)
    {
      if (only >= 0 && character != only)
        continue;
      World w(level, character, theme, art);
      Bot bot;
      const int f = botPlay(w, 12000, bot);
      std::printf("     runner %d: %d frames, %d deaths, route %d, at %d,%d\n", character, f, w.stats().deaths,
                  w.flyTower().routeAt, w.player().x, w.player().y);
      check(w.state() != WorldState::Playing, "the bot clears the Fly Tower");
    }
    for (int character = 0; character < 6; ++character)
    {
      if (only >= 0 && character != only)
        continue;
      World w(bonus, character, theme, art);
      Bot bot;
      const int f = botPlay(w, 900, bot);
      std::printf("     runner %d: %d frames, %d discs\n", character, f, w.flyTower().hits);
      check(w.state() != WorldState::Playing && !w.bonusFailed(), "the bot clears Trigger Happy");
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf(gFailures == 0 ? "all fly tower tests passed\n" : "%d fly tower test(s) failed\n", gFailures);
  return gFailures == 0 ? 0 : 1;
}
