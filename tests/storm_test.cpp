// Level 31's mechanics, played by hand in small rooms: a lightning zone
// glows on its highest metal for 22 frames, then strikes, and what touches
// the struck metal is hurt (not off it, not in Turbo); ties go to the metal
// nearest the runner. A Tesla rod stuck in a wall takes the next strike after
// the full glow and arcs 6 damage to the enemies around it; stuck in a dead
// generator it powers it (the hatch opens, a column turns into ladder, and a
// save keeps it); a strike powers a dead lift, which then takes a rider up.
// Two rods at most, and a rod falls out after 300 frames. A Foghorn's blast
// blows a runner back (crouched, half as far). A Kite Drone flies its figure
// eight. The lamp's Morse. Then Bungee: stretching on the ground, the fling,
// and the bot gets the bonus's score with every runner.

#include "assets/art.hpp"
#include "data/level.hpp"
#include "data/theme.hpp"
#include "frontend/bot.hpp"
#include "game/savegame.hpp"
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

// A 40 x 24 room on the storm cliffs: ceiling row 0, floor rows 22-23,
// walls at x 0 and 39, the runner at (px, py), plus `walls` and `entities`.
std::string room(int px, int py, const std::string& entities, const std::vector<Wall>& walls = {})
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
  std::string s = "name=STORM TEST\nepisode=5\ntheme=storm_cliffs\nweapon=tesla_rod\n[map]\n";
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

// A tap of a button (jump or fire), then let go.
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
    Keys none, fire, down, left, right;
    fire.fire = true;
    down.down = true;
    left.left = true;
    right.right = true;

    auto make = [&](const std::string& text, int character = 0) {
      return World(std::make_shared<const Level>(Level::parse(text)), character, theme, art);
    };
    const std::string zone = "@ lightning z zone=0,0,39,23 period=40 damage=2\n";

    // A strike: 22 frames of glow on the metal, then the bolt; standing on
    // the struck metal costs the zone's damage, standing off it nothing.
    for (int px : {10, 30})
    {
      World w = make(room(px, 21, "@ metal rect=5,22,14,22\n" + zone));
      const int hp = w.player().hp;
      idle(w, 17);
      const bool dark = !w.storm().zones[0].glowing();
      idle(w, 1);
      const bool glow = w.storm().zones[0].glowing() && w.storm().zones[0].target.kind == TargetKind::Metal;
      idle(w, 21);
      const bool before = w.player().hp == hp;
      idle(w, 1);
      if (px == 10)
        check(dark && glow && before && w.player().hp == hp - 2 && w.storm().zones[0].hurt > 0,
          "the metal glows 22 frames, then the strike hurts a runner standing on it");
      else
        check(glow && w.player().hp == hp, "a runner off the struck metal is not hurt");
    }
    {
      // In Turbo, strikes do no harm.
      World w = make(room(10, 21, "@ metal rect=5,22,14,22\n" + zone));
      w.cheat(Cheat::Turbo);
      const int hp = w.player().hp;
      idle(w, 44);
      check(w.player().hp == hp, "Turbo: a strike does no damage");
    }
    {
      // The highest metal takes the strike (the post, not the floor plate
      // the runner stands on); ties go to the metal nearest the runner.
      World w = make(room(10, 21, "@ metal rect=5,22,14,22\n@ metal post rect=20,15,20,21\n" + zone));
      const int hp = w.player().hp;
      idle(w, 41);
      check(w.storm().zones[0].live.size() == 1 && w.storm().zones[0].boltX == 41 && w.player().hp == hp,
        "the highest metal takes the strike");
      World t = make(room(10, 21, "@ metal a rect=4,18,4,21\n@ metal b rect=30,18,30,21\n" + zone));
      idle(t, 18);
      check(t.storm().zones[0].target.kind == TargetKind::Metal && t.storm().zones[0].target.index == 0,
        "a tie goes to the metal nearest the runner");
    }
    {
      // A Tesla rod in the wall takes the next strike after the full glow;
      // the arcs do 6 damage to the enemies around it.
      World w = make(room(10, 21, "@ metal post rect=5,10,5,21\n@ foghorn 33 19 dir=r\n" + zone, {{33, 20, 35, 21}}));
      w.cheat(Cheat::Ammo);
      idle(w, 2);
      Keys prev;
      step(w, fire, prev);
      int f = 0;
      while (w.storm().rods.empty() && f++ < 20)
        step(w, none, prev);
      const auto& z = w.storm().zones[0];
      const bool stuck = w.storm().rods.size() == 1 && z.target.kind == TargetKind::Rod && z.glowing() &&
        z.t <= z.period - z.glow + 1;
      check(stuck, "a rod in the wall: its zone's next target, after the full glow");
      idle(w, z.period - z.t - 1);
      const Enemy* horn = firstOf(w, EnemyKind::Foghorn);
      const int hpBefore = horn ? horn->hp : -1;
      idle(w, 1);
      horn = firstOf(w, EnemyKind::Foghorn);
      check(horn && hpBefore == 8 && horn->hp == 2 && w.storm().rods.empty() && !w.storm().arcs.empty(),
        "the strike on the rod arcs 6 damage to the enemy beside it");
    }
    {
      // A rod in the dead generator: the strike powers it, the hatch opens
      // and its column turns into ladder; a save keeps it so.
      World w = make(room(10, 21,
        "@ machine gen kind=generator rect=20,20,21,21 opens=25,22,26,22 ladder=26\n" + zone));
      w.cheat(Cheat::Ammo);
      idle(w, 2);
      Keys prev;
      tap(w, fire, prev);
      idle(w, 6);
      const bool inGen = w.storm().rods.size() == 1 && w.storm().rods[0].machine == 0;
      idle(w, 24);
      const auto& map = w.map();
      check(inGen && w.storm().machines[0].powered && !map.solid(50, 44) && map.ladder(52, 44),
        "a rod in the generator: the strike powers it and the hatch opens");
      idle(w, 10);
      SaveGame s = w.snapshot();
      World fresh = make(room(10, 21,
        "@ machine gen kind=generator rect=20,20,21,21 opens=25,22,26,22 ladder=26\n" + zone));
      check(w.canSave() && fresh.restore(s) && fresh.storm().machines[0].powered && fresh.map().ladder(52, 44) &&
          !fresh.map().solid(50, 44),
        "a save keeps the generator powered and its hatch open");
    }
    {
      // A dead lift: the rider waits until a strike powers it, then it goes.
      World w = make(room(9, 19,
        "@ platform lift 8 20 w=3 mode=pingpong path=8,20;8,10 speed=1 wait=rider\n"
        "@ machine m kind=lift lift=lift rect=30,10,30,12\n" + zone));
      idle(w, 4);
      const int y0 = w.platforms()[0].y;
      idle(w, 30);
      const bool dead = w.platforms()[0].y == y0 && w.platforms()[0].dead;
      idle(w, 40);
      check(dead && w.storm().machines[0].powered && w.platforms()[0].y < y0 && w.player().y < 39,
        "a dead lift waits for the strike, then takes its rider up");
    }
    {
      // Two rods at most (a third pulls the oldest); unstruck, a rod falls
      // out after 300 frames.
      World w = make(room(10, 21, ""));
      w.cheat(Cheat::Ammo);
      idle(w, 2);
      Keys prev;
      for (int i = 0; i < 3; ++i)
      {
        tap(w, fire, prev);
        idle(w, 24); // to the far wall
        step(w, i % 2 == 0 ? left : right, prev);
        step(w, none, prev);
      }
      const bool two = w.storm().rods.size() == 2;
      idle(w, kRodLife);
      check(two && w.storm().rods.empty(), "two rods at most, and a rod falls out after 300 frames");
    }
    {
      // A Foghorn's blast blows the runner back four blocks; crouched, two.
      int moved[2] = {0, 0};
      for (int crouch : {0, 1})
      {
        World w = make(room(26, 21, "@ foghorn 30 21 dir=l\n"));
        Keys prev;
        const int x0 = w.player().x;
        hold(w, crouch ? down : none, 40, prev);
        moved[crouch] = x0 - w.player().x;
      }
      check(moved[0] == kFogPush && moved[1] == kFogPush / 2, "a Foghorn blows a runner back (crouched, half as far)");
    }
    {
      // A Kite Drone flies its figure eight about its anchor (on screen, the
      // runner out of its reach).
      World w = make(room(38, 21, "@ kite_drone 21 18\n"));
      const Enemy* k = firstOf(w, EnemyKind::KiteDrone);
      const int ax = k->x, ay = k->y;
      int far = 0, ok = 1;
      for (int f = 0; f < 90; ++f)
      {
        idle(w, 1);
        k = firstOf(w, EnemyKind::KiteDrone);
        far = std::max(far, std::abs(k->x - ax));
        ok = ok && std::abs(k->x - ax) <= kKiteRadius + 2 && std::abs(k->y - ay) <= kKiteRadius;
      }
      check(ok && far >= kKiteRadius - 2, "a Kite Drone flies its figure eight on the tether");
    }
    // The ship's Morse: G (--.), 4-frame dots, 12-frame dashes.
    check(morseLight(0) && morseLight(11) && !morseLight(12) && morseLight(16) && !morseLight(28) && morseLight(32) &&
        !morseLight(36),
      "the ship flashes G-U-N in Morse");

    // Bungee: walking away stretches the rope (to its max), letting go
    // flings the runner toward the post.
    const auto bonus = std::make_shared<const Level>(Level::loadFile(argv[1]));
    {
      World w(bonus, 0, theme, art);
      idle(w, 3);
      Keys prev;
      const int x0 = w.player().x;
      hold(w, left, 40, prev);
      const auto& s = w.storm();
      const float dx = s.postX - (float(w.player().x) + 1.5f), dy = s.postY - (float(w.player().y) - 2.0f);
      const bool stretched = w.player().x < x0 && !s.flying && dx * dx + dy * dy <= kBungeeMax * kBungeeMax;
      step(w, none, prev);
      check(stretched && s.flying && s.vx > 2.0f, "Bungee: stretch the rope on the ground, let go and fly");
    }
    for (int character = 0; character < 6; ++character)
    {
      World w(bonus, character, theme, art);
      Bot bot;
      botPlay(w, 900, bot);
      std::printf("     runner %d: score %d\n", character, w.stats().score);
      check(w.stats().score >= 5000 && !w.bonusFailed(), "the bot flings through the storm rings to the score");
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf(gFailures == 0 ? "all storm tests passed\n" : "%d storm test(s) failed\n", gFailures);
  return gFailures == 0 ? 0 : 1;
}
