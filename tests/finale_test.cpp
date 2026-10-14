// Level 42's mechanics, played by hand in small rooms: a set wagon's
// roll-in, its trick and the RERUN (the neon beat signs, the temple's vines),
// the station's gravity plate and its rest state, the studio's monitors, the
// toon's air-run, a Censor Bar's black bar, an Ad Break's free samples, a
// Stunt Double's shot, the Encore's wheel and shared pool, the backstage
// refill, Lance Marquee's armoured podium, the Encore Engine's core, the
// glass cover and the master switch, the Blooper Reel's rules. Then the bot
// clears The Live Finale (through a trapdoor into the Blooper Reel) and the
// Blooper Reel with every runner.

#include "assets/art.hpp"
#include "data/level.hpp"
#include "data/theme.hpp"
#include "data/weapons.hpp"
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
  const std::string& extra = "weapon=encore\n")
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
  std::string s = "name=FINALE TEST\nepisode=6\ntheme=main_stage\n" + extra + "[map]\n";
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

int count(const World& w, EnemyKind kind)
{
  int n = 0;
  for (const auto& e : w.enemies())
    n += e.alive && e.kind == kind ? 1 : 0;
  return n;
}

int botPlay(World& w, int frames, Bot& bot)
{
  Input prev;
  int f = 0;
  const bool trace = std::getenv("GR_FN_TRACE") != nullptr;
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
      std::printf("f%d %d,%d st%d g%d hp%d boss%d in %c%c%c%c%c%c\n", f, pl.x, pl.y, int(pl.state), int(pl.grav), pl.hp,
                  int(w.finale().boss.phase), in.left ? 'L' : '.', in.right ? 'R' : '.', in.up ? 'U' : '.',
                  in.down ? 'D' : '.', in.jump ? 'J' : '.', in.fire ? 'F' : '.');
      for (const auto& e : w.enemies())
        if (e.alive && e.active && !e.hidden && std::abs(e.x - pl.x) < 30)
          std::printf("   %s %d,%d hp%d tell%d\n", enemyDef(e.def).key, e.x, e.y, e.hp, e.tell);
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
    Keys right, left, fire, up, upFire;
    right.right = true;
    left.left = true;
    fire.fire = true;
    up.up = true;
    upFire.up = upFire.fire = true;
    auto make = [&](const std::string& text, int character = 0) {
      return World(std::make_shared<const Level>(Level::parse(text)), character, theme, art);
    };
    {
      // A wagon rolls in (the world holds still), runs its trick (the beat
      // signs on the beat), reruns while the runner is in the slot and
      // rolls off once the runner is past it.
      World w = make(room(3, 21,
        "@ wagon n kind=neon slot=8,30 trigger=6\n"
        "@ layer S1 rect=14,18,16,18 driver=beat beats=1,2,3 bars=1 style=sign color=cyan\n"));
      const auto& wg = w.finale().wagons[0];
      const Layer& sign = w.layers()[std::size_t(w.finale().signs[0])];
      const bool rest = wg.state == FnWagonState::Idle && sign.driver == LayerDriver::Script && sign.solid;
      Keys prev;
      for (int i = 0; i < 40 && wg.state == FnWagonState::Idle; ++i)
        step(w, right, prev);
      const bool rolling = wg.state == FnWagonState::Rolling && w.finaleFrozen();
      const int x0 = w.player().x;
      hold(w, right, 20, prev);
      const bool still = w.player().x == x0;
      idle(w, kFnRoll);
      const bool running = wg.state == FnWagonState::Running && sign.driver == LayerDriver::Beat;
      hold(w, right, 10, prev); // into the slot
      idle(w, kFnTrick - 10);
      const bool rerun = wg.state == FnWagonState::Rerun && wg.reruns == 1 && sign.driver == LayerDriver::Script && sign.solid;
      idle(w, kFnRerun + 1);
      const bool again = wg.state == FnWagonState::Running;
      hold(w, right, 60, prev);
      check(rest, "at rest the neon signs stand solid");
      check(rolling && still, "past the trigger the turntable turns the wagon in, the world holding still");
      check(running, "the trick runs: the signs on the beat");
      check(rerun && again, "after 450 frames in the slot: RERUN, then the trick again");
      check(wg.state == FnWagonState::Done, "past the slot the wagon rolls off");
    }
    {
      // The temple's vines hang still at rest and swing while its trick runs.
      World w = make(room(3, 21, "@ wagon t kind=temple slot=8,30 trigger=4\n@ vine V1 20 6 len=8 amp=50\n"));
      const auto& v = w.vines()[0];
      const bool still = v.rest == 0 && v.amp == 0;
      Keys prev;
      for (int i = 0; i < 40 && w.finale().wagons[0].state == FnWagonState::Idle; ++i)
        step(w, right, prev);
      idle(w, kFnRoll + 2);
      check(still && v.rest == 50, "the temple's vines hang still at rest and swing in the trick");
    }
    {
      // The station: a plate turns the chamber over 8 frames after the
      // runner steps on it; at rest gravity is down again.
      World w = make(room(3, 21,
        "@ wagon s kind=station slot=4,30 trigger=4\n@ gravzone GZ rect=4,8,30,21 dir=down\n@ plate PU 10 21 zone=GZ dir=up\n",
        {{2, 7, 37, 7}}));
      const auto& wg = w.finale().wagons[0];
      Keys prev;
      for (int i = 0; i < 40 && wg.state == FnWagonState::Idle; ++i)
        step(w, right, prev);
      idle(w, kFnRoll + 1);
      for (int i = 0; i < 40 && w.player().x + 1 < 20; ++i)
        step(w, right, prev);
      idle(w, kFnFlip - 2);
      const bool waits = w.player().grav == Grav::Down;
      idle(w, 30);
      const bool flipped = w.player().grav == Grav::Up && w.player().y <= 16;
      for (int i = 0; i < kFnTrick && wg.state == FnWagonState::Running; ++i)
        idle(w, 1);
      idle(w, 20);
      check(waits && flipped, "a gravity plate turns the chamber over 8 frames later");
      check(wg.state == FnWagonState::Rerun && w.player().grav == Grav::Down, "at rest the chamber's gravity is down");
    }
    {
      // The studio's monitors: up at one, a static wipe, out of the other.
      World w = make(room(3, 21, "@ wagon st kind=studio slot=4,36 trigger=4\n@ portal A 10 19 to=B label=A\n@ portal B 30 19 to=A label=B\n"));
      Keys prev;
      for (int i = 0; i < 40 && w.finale().wagons[0].state == FnWagonState::Idle; ++i)
        step(w, right, prev);
      idle(w, kFnRoll + 1);
      for (int i = 0; i < 40 && w.player().x + 1 < 20; ++i)
        step(w, right, prev);
      tap(w, up, prev);
      const bool wipe = w.finaleFrozen() && w.finale().wipe > 0;
      idle(w, kFnWipe + 1);
      std::printf("     monitor: out at %d,%d\n", w.player().x, w.player().y);
      check(wipe && w.player().x == 60 && w.player().y == 43, "up at a monitor: a static wipe, out of the other one");
    }
    {
      // The toon: off a ledge holding the way, the runner runs on air; let
      // go, an "uh-oh", then the fall.
      World w = make(room(4, 15, "@ wagon t kind=toon slot=4,36 trigger=4\n", {{2, 16, 14, 17}}));
      idle(w, kFnRoll + 4);
      const int y0 = w.player().y;
      Keys prev;
      hold(w, right, 30, prev);
      const bool onAir = w.player().x > 30 && w.player().y == y0;
      idle(w, kFnUhOh - 2);
      const bool uhOh = w.player().y == y0;
      idle(w, 30);
      std::printf("     air-run: x %d, feet %d -> %d\n", w.player().x, y0, w.player().y);
      check(onAir, "in the toon the runner runs on over the gap");
      check(uhOh && w.player().y > y0 + 4, "letting go: the uh-oh, then the fall");
    }
    {
      // A Censor Bar lays its black bar over a runner below; three shots down it.
      World w = make(room(20, 21, "@ censorbar 20 14 path=14,14;26,14\n"));
      w.cheat(Cheat::God);
      idle(w, kFnBarUnroll + 2);
      const bool bar = w.finale().actors[0].bar == kFnBarUnroll;
      Keys prev;
      for (int i = 0; i < 600 && count(w, EnemyKind::CensorBar) > 0; ++i)
      {
        const auto& e = w.enemies()[std::size_t(w.finale().actors[0].enemy)];
        const int target = e.x + e.w / 2 - 2;
        Keys k;
        k.right = w.player().x < target - 2;
        k.left = w.player().x > target + 2;
        k.up = !k.left && !k.right;
        k.fire = k.up && i % 2 == 0;
        step(w, k, prev);
      }
      check(bar, "a Censor Bar unrolls its bar over the runner");
      check(count(w, EnemyKind::CensorBar) == 0, "shots from below take it down");
    }
    {
      // An Ad Break waits in the flies until its wagon runs, drops in,
      // jingles and brings out two sample bots.
      World w = make(room(5, 21, "@ wagon w kind=western slot=4,36 trigger=4\n@ adbreak 15 21 wagon=w\n"));
      w.cheat(Cheat::God);
      const std::size_t ad = std::size_t(w.finale().actors[0].enemy); // sample bots move the list
      idle(w, 2);
      const bool hidden = w.enemies()[ad].hidden;
      idle(w, kFnRoll + kFnAdRoll + kFnJingle + 8);
      const int samples = count(w, EnemyKind::SampleBot);
      int x0 = 1000;
      for (const auto& e : w.enemies())
        if (e.alive && e.kind == EnemyKind::SampleBot)
          x0 = std::min(x0, e.x);
      idle(w, 12);
      int x1 = 1000;
      for (const auto& e : w.enemies())
        if (e.alive && e.kind == EnemyKind::SampleBot)
          x1 = std::min(x1, e.x);
      std::printf("     ad break: %d samples, %d -> %d\n", samples, x0, x1);
      check(hidden && !w.enemies()[ad].hidden, "an Ad Break drops in once its wagon's trick runs");
      check(samples == 2 && x1 < x0, "its jingle brings out two sample bots that walk at the runner");
    }
    {
      // A Stunt Double (Rocco's: 12 HP) fires at the runner.
      World w = make(room(5, 21, "@ double 14 21 runner=rocco\n"));
      w.cheat(Cheat::God);
      const int hp = w.enemies()[0].hp;
      bool shot = false;
      for (int i = 0; i < 300 && !shot; ++i)
      {
        idle(w, 1);
        for (const auto& pr : w.projectiles())
          shot = shot || (pr.alive && pr.kind == ShotKind::Enemy);
      }
      check(hp == 12 && shot, "a Stunt Double has its runner's HP and shoots");
    }
    {
      // The Encore: the blaster free, up + fire flicks the wheel, a
      // prototype shot costs the pool a unit, a beat brings one back, an
      // empty pool fires the blaster; the backstage refill once a life.
      World w = make(room(3, 21, "@ refill 20 21\n"));
      w.setFinaleArsenal({"pulse_pistol", "bass_cannon"});
      w.cheat(Cheat::Ammo);
      const auto& p = w.player();
      const auto& f = w.finale();
      const bool wheel = f.wheel.size() == 3 && p.proto == int(ProtoId::Encore) && p.ammo == kFnPool;
      Keys prev;
      step(w, fire, prev);
      const bool blaster = !w.projectiles().empty() && w.projectiles().back().encore < 0 && p.ammo == kFnPool;
      step(w, Keys{}, prev);
      idle(w, 4);
      tap(w, upFire, prev);
      const bool flick = f.sel == 1;
      idle(w, 4);
      const int pool = p.ammo;
      step(w, fire, prev);
      const bool proto = w.projectiles().back().encore == int(ProtoId::PulsePistol) && p.ammo == pool - 1;
      step(w, Keys{}, prev);
      const int before = p.ammo;
      idle(w, kFnBeat);
      const bool beat = p.ammo == before + 1;
      for (int i = 0; i < 200 && p.ammo > 0; ++i)
        tap(w, fire, prev);
      step(w, fire, prev);
      const bool empty = p.ammo == 0 && w.projectiles().back().encore < 0;
      step(w, Keys{}, prev);
      for (int i = 0; i < 60 && !f.refillUsed; ++i)
        step(w, right, prev);
      const bool refill = f.refillUsed && p.ammo >= kFnPool - 1;
      for (int i = 0; i < 6; ++i)
        tap(w, fire, prev);
      hold(w, left, 12, prev);
      hold(w, right, 14, prev);
      std::printf("     encore: wheel %zu, pool %d -> %d, refill then %d\n", f.wheel.size(), pool, before, p.ammo);
      check(wheel && blaster, "the Encore's wheel holds the Arsenal; the blaster costs nothing");
      check(flick && proto, "up + fire flicks the wheel; a prototype shot costs the pool a unit");
      check(beat, "a ticker beat brings a unit back");
      check(empty, "an empty pool fires the blaster");
      check(refill && p.ammo < kFnPool - 1, "the backstage refill fills the pool once a life");
    }
    {
      // The boss: Lance's podium is armoured until it sinks for the cue
      // cards; the Encore Engine only takes hits on its open core; Off the
      // Air, the glass cover breaks and the master switch ends the show.
      World w = make(room(3, 21,
        "@ lance 20 6 arena=2,4,37,21 path=8,6;30,6 engine=28,14 desk=36 restart=4,21 drops=10,21\n"
        "@ layer cover rect=36,20,36,21 driver=script solid=1 style=station tile=#\n",
        {{36, 21, 36, 21, 'X'}}));
      w.cheat(Cheat::God);
      const auto& b = w.finale().boss;
      const auto& p = w.player();
      Keys prev;
      bool armour = true, lanceHit = false;
      int i = 0;
      for (; i < 9000 && (b.phase == FnPhase::Waiting || b.phase == FnPhase::Intro || b.phase == FnPhase::Lance); ++i)
      {
        const int target = b.x + 3, hp = b.lanceHp, cue = b.cue;
        Keys k;
        k.right = p.x < target - 1;
        k.left = p.x > target + 1;
        k.up = !k.left && !k.right;
        k.fire = k.up && i % 2 == 0;
        step(w, k, prev);
        if (b.lanceHp < hp)
        {
          lanceHit = true;
          armour = armour && (cue > 0 || b.cue > 0);
        }
      }
      std::printf("     Lance down after %d frames\n", i);
      check(lanceHit && armour && b.phase == FnPhase::Engine, "Lance's podium only takes hits while sunk for the cue cards");
      bool core = true, engineHit = false;
      for (i = 0; i < 9000 && (b.phase == FnPhase::Engine || b.phase == FnPhase::Collapse); ++i)
      {
        const int target = b.ex - 14, hp = b.engineHp, open = b.core + b.stagger;
        Keys k;
        k.right = p.x < target - 1;
        k.left = p.x > target + 1;
        k.fire = !k.left && !k.right && p.facing > 0 && i % 2 == 0;
        if (!k.left && !k.right && p.facing < 0)
          k.right = true;
        step(w, k, prev);
        if (b.engineHp < hp)
        {
          engineHit = true;
          core = core && (open > 0 || b.stagger > 0);
        }
      }
      std::printf("     the Engine down after %d frames\n", i);
      check(engineHit && core && b.phase == FnPhase::OffAir, "the Encore Engine only takes hits on its open core");
      const bool shut = !w.exitPowered();
      for (i = 0; i < 400 && b.coverHp > 0; ++i)
      {
        Keys k;
        k.right = p.x < 63;
        k.fire = !k.right && i % 2 == 0;
        step(w, k, prev);
      }
      const bool open = w.exitPowered() && !w.layers()[std::size_t(w.finale().cover)].solid;
      hold(w, right, 30, prev);
      check(shut && open, "Off the Air: the glass cover breaks and the switch works");
      check(b.phase == FnPhase::Switched && w.state() != WorldState::Playing, "the master switch: the ON AIR sign dies");
    }
    {
      // The Blooper Reel: a rule from the first frame, a new one every 150
      // frames, never the same twice running, all six in a round.
      World w = make(room(3, 21, "", {}, "rules=blooper\ntimer=120\ngoal=exit\n"));
      const auto& f = w.finale();
      const bool first = f.rule >= 0;
      unsigned seen = f.rule >= 0 ? 1u << unsigned(f.rule) : 0u;
      int changes = 0, last = f.rule;
      for (int i = 0; i < 6 * kFnReelEvery - 10; ++i)
      {
        idle(w, 1);
        if (f.rule != last)
        {
          ++changes;
          seen |= 1u << unsigned(f.rule);
          last = f.rule;
        }
      }
      check(first && changes == kFnReelRules - 1 && seen == (1u << kFnReelRules) - 1u, "the Blooper Reel deals out all six rules, one at a time");
    }
    const auto level = std::make_shared<const Level>(Level::loadFile(argv[1]));
    const auto bonus = std::make_shared<const Level>(Level::loadFile(argv[2]));
    const int only = std::getenv("GR_FN_RUNNER") ? std::atoi(std::getenv("GR_FN_RUNNER")) : -1;
    const bool skipLevel = std::getenv("GR_FN_BONUS_ONLY") != nullptr;
    for (int character = 0; character < 6 && !skipLevel; ++character)
    {
      if (only >= 0 && character != only)
        continue;
      World w(level, character, theme, art);
      Bot bot;
      bot.setTakeBonus(true);
      const int f = botPlay(w, 20000, bot);
      std::printf("     runner %d: %d frames, %d deaths, at %d,%d, bonus %d\n", character, f, w.stats().deaths,
                  w.player().x, w.player().y, int(w.finale().bonusDone));
      check(w.state() != WorldState::Playing, "the bot clears The Live Finale");
      check(w.finale().bonusDone, "the bot falls through a trapdoor into the Blooper Reel");
    }
    for (int character = 0; character < 6; ++character)
    {
      if (only >= 0 && character != only)
        continue;
      World w(bonus, character, theme, art);
      Bot bot;
      const int f = botPlay(w, 1800, bot);
      std::printf("     runner %d: %d frames\n", character, f);
      check(w.state() != WorldState::Playing && !w.bonusFailed(), "the bot clears the Blooper Reel");
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf(gFailures == 0 ? "all finale tests passed\n" : "%d finale test(s) failed\n", gFailures);
  return gFailures == 0 ? 0 : 1;
}
