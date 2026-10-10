// Level 28's mechanics, played by hand in small rooms: the air-run (held,
// let go: the "uh-oh", then the fall), a painted tunnel (a passage) and
// painted rock (a bonk), the Portable Hole in a wall and in a floor (it
// opens, it is picked up walking through), a Grinning Bomb's lunge, a
// flat Rubber Goon's bounce, a Piano Bird's piano (shadow, drop, a step),
// Buddy Boom's nose after a slam, the phases and the dizzy line cracking.
// Then the Inkwell: fire draws a line, a jump and a line are a step up,
// and the bot collects every gem with every runner.

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

// A 40 x 24 room: ceiling row 0, floor rows 22-23, walls at x 0 and 39, the
// runner at (px, py), plus `walls` and `entities`; `gap` clears the floor
// from x0 to x1.
std::string room(int px, int py, const std::string& entities, const std::vector<Wall>& walls = {},
  const std::string& extra = "", int gap0 = -1, int gap1 = -1)
{
  std::vector<std::string> map(24, std::string(40, '.'));
  for (int x = 0; x < 40; ++x)
    map[0][std::size_t(x)] = map[22][std::size_t(x)] = map[23][std::size_t(x)] = '#';
  for (int y = 0; y < 24; ++y)
    map[std::size_t(y)][0] = map[std::size_t(y)][39] = '#';
  for (int x = gap0; gap0 >= 0 && x <= gap1; ++x)
    map[22][std::size_t(x)] = map[23][std::size_t(x)] = '.';
  for (const auto& w : walls)
    for (int y = w.y0; y <= w.y1; ++y)
      for (int x = w.x0; x <= w.x1; ++x)
        map[std::size_t(y)][std::size_t(x)] = w.c;
  map[std::size_t(py)][std::size_t(px)] = 'P';
  std::string s = "name=TOON TEST\nepisode=4\ntheme=rubber_hose_toon\nweapon=portable_hole\n" + extra + "[map]\n";
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

int alive(const World& w, EnemyKind k)
{
  int n = 0;
  for (const auto& e : w.enemies())
    n += e.alive && e.kind == k;
  return n;
}

const Enemy* first(const World& w, EnemyKind k)
{
  for (const auto& e : w.enemies())
    if (e.alive && e.kind == k)
      return &e;
  return nullptr;
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
    Keys right, left, shoot, rightShoot;
    right.right = true;
    left.left = true;
    shoot.fire = true;
    rightShoot.right = rightShoot.fire = true;

    {
      // Off a ledge holding the way: on along the same row. Let go: the
      // "uh-oh" for a beat, then the fall.
      const auto level = std::make_shared<const Level>(
        Level::parse(room(3, 15, "", {{1, 16, 8, 16}, {30, 16, 38, 16}})));
      World w(level, 0, theme, art);
      idle(w, 4);
      const int y0 = w.player().y;
      Keys prev;
      hold(w, right, 16, prev);
      check(w.player().x > 9 * 2 && w.player().y == y0 && w.airRunning(), "off the ledge, running on air");
      const int x1 = w.player().x;
      idle(w, 1);
      check(w.airRunning() && w.toon().uhOh > 0 && w.player().y == y0, "let go: the uh-oh sign");
      idle(w, kUhOh - 2);
      check(w.player().y == y0 && w.player().x == x1, "...holding still in the air");
      idle(w, 6);
      check(w.player().y > y0 && !w.airRunning(), "then down");
    }
    {
      // Held all the way across: onto the far ledge.
      const auto level = std::make_shared<const Level>(
        Level::parse(room(3, 15, "", {{1, 16, 8, 16}, {24, 16, 38, 16}})));
      World w(level, 0, theme, art);
      idle(w, 4);
      const int y0 = w.player().y;
      Keys prev;
      hold(w, right, 50, prev);
      idle(w, 12);
      check(w.player().y == y0 && w.player().x > 24 * 2 && w.player().state == PlayerState::OnGround,
        "air-run across the gap onto the far ledge");
    }
    {
      // No jumping on air.
      const auto level = std::make_shared<const Level>(
        Level::parse(room(3, 15, "", {{1, 16, 8, 16}, {30, 16, 38, 16}})));
      World w(level, 0, theme, art);
      idle(w, 4);
      const int y0 = w.player().y;
      Keys prev, rj = right;
      hold(w, right, 14, prev);
      rj.jump = true;
      hold(w, rj, 3, prev);
      check(w.player().y == y0, "no jump from thin air");
    }
    {
      // A painted tunnel is a passage; painted rock is a bonk.
      const auto level = std::make_shared<const Level>(Level::parse(
        room(5, 21, "@ tunnel 20 19 real=1\n", {{20, 1, 21, 21}})));
      World w(level, 0, theme, art);
      Keys prev;
      hold(w, right, 45, prev);
      check(w.player().x > 22 * 2, "through the real tunnel");
    }
    {
      const auto level = std::make_shared<const Level>(Level::parse(
        room(5, 21, "@ tunnel 20 19 real=0\n", {{20, 1, 21, 21}})));
      World w(level, 0, theme, art);
      Keys prev;
      int bonked = 0;
      for (int f = 0; f < 40; ++f)
      {
        step(w, right, prev);
        bonked += w.toon().bonk == kBonk;
      }
      check(bonked == 1 && w.player().x < 20 * 2, "painted rock: one bonk");
      // The Portable Hole opens the painting (the programmer art behind).
      w.cheat(Cheat::Ammo);
      hold(w, left, 1, prev);
      hold(w, right, 1, prev);
      idle(w, kBonk + 2);
      const int ammo = w.player().ammo;
      hold(w, shoot, kHoleHold, prev);
      idle(w, 6);
      check(w.toon().hole.open && w.toon().hole.wall && w.toon().retro > 0 && w.player().ammo == ammo - 1,
        "the hole on painted rock: open, the programmer art behind");
      hold(w, right, 10, prev);
      check(w.toon().bonk == 0 && w.player().x >= 20 * 2 - 1, "into the opened painting, no bonk");
    }
    {
      // The Portable Hole through a wall 3 thick, walked through: picked up.
      const auto level = std::make_shared<const Level>(Level::parse(room(5, 21, "", {{20, 1, 22, 21}})));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      Keys prev;
      hold(w, right, 18, prev);
      idle(w, 2);
      const int ammo = w.player().ammo;
      hold(w, shoot, kHoleHold, prev);
      idle(w, 6);
      check(w.toon().hole.open && w.toon().hole.wall && w.toon().hole.x0 == 20 && w.toon().hole.x1 == 22 &&
          w.toon().hole.y1 == 21,
        "a hole in a 3-thick wall, on the floor");
      hold(w, right, 20, prev);
      check(w.player().x > 23 * 2 && !w.toon().hole.open && w.player().ammo == ammo, "walked through: picked up");
    }
    {
      // With the Portable Hole, a tap is still a blaster shot.
      const auto level = std::make_shared<const Level>(Level::parse(room(5, 21, "")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      Keys prev;
      const int ammo = w.player().ammo;
      hold(w, shoot, 1, prev);
      int shots = 0;
      for (const auto& pr : w.projectiles())
        shots += pr.alive;
      idle(w, 4);
      check(shots == 1 && !w.toon().toss.on && w.player().ammo == ammo && w.player().weapon == Weapon::Proto,
        "a tap with the Portable Hole: a blaster shot, no hole");
    }
    {
      // Too thick: it comes back.
      const auto level = std::make_shared<const Level>(Level::parse(room(5, 21, "", {{20, 1, 24, 21}})));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      Keys prev;
      hold(w, right, 18, prev);
      const int ammo = w.player().ammo;
      hold(w, shoot, kHoleHold, prev);
      idle(w, 8);
      check(!w.toon().hole.open && !w.toon().toss.on && w.player().ammo == ammo, "a wall 5 thick: the hole comes back");
    }
    {
      // In a floor: a hole; it closes after a while.
      const auto level = std::make_shared<const Level>(Level::parse(room(5, 15, "", {{1, 16, 30, 16}})));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      Keys prev, crouch;
      crouch.down = crouch.fire = true;
      idle(w, 4);
      hold(w, crouch, kHoleHold, prev);
      idle(w, 10);
      check(w.toon().hole.open && !w.toon().hole.wall && w.toon().hole.y0 == 16, "a hole in the floor ahead");
      idle(w, kHoleOpen + 2);
      check(!w.toon().hole.open, "it closes by itself");
    }
    {
      // A Grinning Bomb lunges and blows up next to the runner.
      const auto level = std::make_shared<const Level>(Level::parse(room(5, 21, "@ grinning_bomb 14 21\n")));
      World w(level, 0, theme, art);
      const int hp = w.player().hp;
      idle(w, 60);
      check(alive(w, EnemyKind::GrinningBomb) == 0 && w.player().hp < hp, "a Grinning Bomb lunges and blows up");
    }
    {
      // A Rubber Goon: shot flat, then a trampoline.
      const auto level = std::make_shared<const Level>(Level::parse(room(5, 21, "@ rubber_goon 20 21\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::God);
      w.cheat(Cheat::Rapid);
      Keys prev;
      for (int f = 0; f < 40; ++f)
      {
        const Enemy* g = first(w, EnemyKind::RubberGoon);
        if (g && g->dive > 0)
          break;
        step(w, f % 2 ? shoot : Keys{}, prev);
      }
      const Enemy* g = first(w, EnemyKind::RubberGoon);
      check(g && g->dive > 0, "shot flat");
      int top = w.player().y;
      if (g)
      {
        const int gx = g->x;
        Keys rj = right;
        rj.jump = true;
        int jumpAt = -1;
        for (int f = 0; f < 60; ++f)
        {
          Keys k;
          if (jumpAt < 0 && w.player().x + 9 >= gx)
            jumpAt = f;
          if (w.player().x + 1 < gx)
            k = jumpAt >= 0 && f < jumpAt + 3 ? rj : right;
          if (jumpAt >= 0 && f > jumpAt + 3)
            k.jump = true; // held: higher
          step(w, k, prev);
          top = std::min(top, w.player().y);
        }
      }
      check(top < 21 * 2 + 1 - kGoonBounce, "landing on him: bounced high");
    }
    {
      // A Piano Bird drops a piano where the runner is: the shadow, then it
      // lands and stands as a step.
      const auto level = std::make_shared<const Level>(Level::parse(room(20, 21, "@ spawner kind=piano_bird trigger=2\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::God);
      int shadow = 0, standing = 0;
      for (int f = 0; f < 200; ++f)
      {
        idle(w, 1);
        for (const auto& pn : w.toon().pianos)
        {
          shadow += pn.state == 0;
          standing = std::max(standing, int(pn.state == 2));
        }
      }
      check(shadow >= kPianoTell - 2 && standing, "a piano: the shadow, then a step");
    }
    {
      // Buddy Boom, phase 1: rubber all over but the nose, and the nose only
      // while it flashes after a slam.
      const std::string buddy =
        "@ buddy 26 21 arena=2,4,38,22 door=2,19,2,21 floor=3,6,6,22 platforms=6,10,28,32,16 drops=8,21;30,21 turbo=20,10\n";
      const auto level = std::make_shared<const Level>(Level::parse(room(8, 21, buddy,
        {{6, 16, 10, 16, '='}, {28, 16, 32, 16, '='}})));
      World w(level, 0, theme, art);
      w.cheat(Cheat::God);
      Keys prev;
      hold(w, right, 2, prev);
      check(w.buddyFight() && w.toon().buddy.doorShut, "in the big top: the door shuts");
      const int hp0 = w.buddyHp();
      for (int f = 0; f < 40 && w.toon().buddy.nose == 0; ++f)
        step(w, f % 2 ? shoot : Keys{}, prev);
      check(w.buddyHp() == hp0, "rubber: the blaster bounces off");
      for (int f = 0; f < 120 && w.toon().buddy.nose == 0; ++f)
        idle(w, 1);
      check(w.toon().buddy.nose > 0, "a slam: the nose flashes");
      for (int f = 0; f < 24; ++f)
        step(w, f % 2 ? shoot : Keys{}, prev);
      check(w.buddyHp() < hp0, "the nose takes the hits");
      // Phases 2 and 3, then dizzy on his line: the line cracks under him.
      w.cheat(Cheat::Rapid);
      int phase3 = 0, dizzy = 0, done = 0;
      Bot bot;
      Input bprev;
      for (int f = 0; f < 6000 && !done; ++f)
      {
        const Input in = bot.play(w);
        PlayerInput p;
        p.left = in.left;
        p.right = in.right;
        p.up = in.up;
        p.down = in.down;
        p.jump = {in.jump, in.jump && !bprev.jump};
        p.fire = {in.fire, in.fire && !bprev.fire};
        bprev = in;
        w.update(p);
        phase3 = std::max(phase3, int(w.toon().buddy.phase == 3));
        dizzy = std::max(dizzy, int(w.toon().buddy.state == BuddyState::Dizzy));
        done = w.toon().buddy.state == BuddyState::Done;
      }
      check(phase3 && dizzy && done && !w.toon().buddy.doorShut, "the bot beats Buddy Boom: dizzy, the ink, the door");
      check(w.exitPowered(), "the iris opens");
    }
  }
  {
    // The Inkwell.
    const auto level = std::make_shared<const Level>(Level::loadFile(argv[1]));
    Renderer renderer(sdl);
    const Theme& theme = themeByIndex(0);
    const Art art = Art::build(theme, renderer);
    {
      World w(level, 0, theme, art);
      check(w.ink().on && w.ink().ink == kInkMax, "the Inkwell: a full well");
      Keys prev, draw, right;
      right.right = true;
      draw.right = draw.fire = true;
      idle(w, 4);
      const int y0 = w.player().y;
      hold(w, draw, 30, prev);
      check(w.ink().ink < kInkMax && w.player().y == y0 && w.player().x > 6 * 2 + 4,
        "drawing ahead: walking on your own line");
      // A jump and a line at the top: a step up.
      Keys jump = right;
      jump.jump = true;
      Keys up = draw;
      up.jump = true;
      for (int f = 0; f < 17; ++f)
        step(w, f < 5 ? jump : f < 11 ? up : draw, prev);
      check(w.player().state == PlayerState::OnGround && w.player().y < y0 - 2, "jump, draw: a step up");
    }
    for (int c = 0; c < 6; ++c)
    {
      World w(level, c, theme, art);
      Bot bot;
      botPlay(w, 1350, bot);
      char what[80];
      std::snprintf(what, sizeof what, "the bot fills the Inkwell's page (runner %d): %d gems", c, w.stats().gems);
      check(w.stats().gems >= 20 || w.state() != WorldState::Playing, what);
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf("%d failure(s)\n", gFailures);
  return gFailures == 0 ? 0 : 1;
}
