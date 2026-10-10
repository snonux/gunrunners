// Level 46's Swap Crystals and aliens on small test maps: a shot at a
// crystal across a gap trades places with it, a drifting one stops once
// swapped, the cracked one gives you the Virus, Swap Rifle shots bounce off
// a wall once, a Prism Bat splits a shot in three, and a Shard Golem only
// takes shots in its back.

#include "assets/art.hpp"
#include "data/level.hpp"
#include "data/theme.hpp"
#include "game/world.hpp"
#include "render/renderer.hpp"

#include <SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <string>

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

// 40 x 14 blocks, ground on row 10 (rows 10-13). `gap` cuts x 14-19 out of
// it; `wall` puts a wall at x 10. The runner starts at x 4, facing right.
std::string testLevel(const std::string& extra, bool gap, bool wall = false, int start = 4)
{
  std::string map;
  for (int y = 0; y < 14; ++y)
  {
    std::string row(40, '.');
    for (int x = 0; x < 40; ++x)
    {
      if (y >= 10 && !(gap && x >= 14 && x <= 19))
        row[std::size_t(x)] = '#';
      if (wall && x == 10 && y < 10)
        row[std::size_t(x)] = '#';
    }
    if (y == 9)
      row[std::size_t(start)] = 'P';
    map += row + "\n";
  }
  return "name=CRYSTAL TEST\nepisode=7\ntheme=crystal_drift\nweapon=swap_rifle\n[map]\n" + map + "[entities]\n" + extra;
}

PlayerInput press(bool right, bool left, bool fire, bool firstFrame)
{
  PlayerInput p;
  p.right = right;
  p.left = left;
  p.fire = {fire, fire && firstFrame};
  return p;
}

void idle(World& w, int frames)
{
  for (int f = 0; f < frames; ++f)
    w.update(press(false, false, false, false));
}

void shootUntil(World& w, int frames, const std::function<bool()>& done)
{
  for (int f = 0; f < frames && !done(); ++f)
    w.update(press(false, false, f % 8 == 0, true));
}

} // namespace

int main()
{
  SDL_Init(0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  {
    Renderer renderer(sdl);
    const Theme& theme = themeByIndex(themeIndexForKey("crystal_drift"));
    const Art art = Art::build(theme, renderer);
    auto make = [&](const std::string& text) { return std::make_shared<const Level>(Level::parse(text)); };

    {
      // Across the gap: shoot the crystal on the far side.
      const auto level = make(testLevel("@ crystal 21 9\n", true));
      World w(level, 0, theme, art);
      idle(w, 4);
      check(w.swapCrystals().size() == 1, "one crystal");
      const int x0 = w.player().x;
      shootUntil(w, 60, [&] { return w.player().x >= 40; });
      idle(w, 6);
      check(w.player().x >= 40 && w.player().state == PlayerState::OnGround, "swapped across the gap");
      check(std::abs(w.swapCrystals()[0].x - x0) <= 1, "the crystal hovers where the runner stood");
    }

    {
      // A drifting crystal stops once swapped.
      const auto level = make(testLevel("@ crystal 21 9 path=26,9 speed=2\n", true));
      World w(level, 0, theme, art);
      const int cx0 = w.swapCrystals()[0].x;
      idle(w, 12);
      check(w.swapCrystals()[0].x != cx0, "the crystal drifts");
      shootUntil(w, 90, [&] { return w.player().x >= 40; });
      check(w.player().x >= 40 && !w.swapCrystals()[0].drifting, "swapped with it, and it stops");
      const int cx1 = w.swapCrystals()[0].x;
      idle(w, 12);
      check(w.swapCrystals()[0].x == cx1, "it stays put");
    }

    {
      // High up over a slab: shoot up through the slab and stand on it.
      std::string text = testLevel("@ crystal 5 2\n", false);
      const std::size_t at = text.find("[map]\n") + 6 + 3 * 41;
      for (int x = 4; x <= 7; ++x)
        text[at + std::size_t(x)] = '=';
      const auto level = make(text);
      World w(level, 0, theme, art);
      idle(w, 4);
      const int y0 = w.player().y;
      for (int f = 0; f < 60 && w.player().y >= y0 - 4; ++f)
      {
        PlayerInput in = press(false, false, f % 8 == 0, true);
        in.up = true;
        w.update(in);
      }
      idle(w, 10);
      check(w.player().y < y0 - 8 && w.player().state == PlayerState::OnGround, "a shot up swaps you up onto the slab");
    }

    {
      // The cracked one: the Virus, and it shatters.
      const auto level = make(testLevel("@ crystal 9 9 cracked=1\n", false));
      World w(level, 0, theme, art);
      idle(w, 4);
      shootUntil(w, 40, [&] { return w.player().virus > 0; });
      check(w.player().virus > 0, "swapping with the cracked crystal infects");
      check(w.swapCrystals()[0].gone, "and it shatters");
    }

    {
      // The Swap Rifle: a shot off the wall ahead comes back and swaps the
      // runner with the crystal behind them.
      const auto level = make(testLevel("@ crystal 1 9\n", false, true, 5));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      check(w.player().weapon == Weapon::Proto && w.player().proto == int(ProtoId::SwapRifle), "holding the Swap Rifle");
      idle(w, 4);
      shootUntil(w, 40, [&] { return w.player().x <= 4; });
      check(w.player().x <= 4, "the shot bounced back off the wall to the crystal");
    }

    {
      // The Swap Rifle swaps the runner with an alien.
      const auto level = make(testLevel("@ shard_golem 16 9\n", false));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      idle(w, 4);
      const int x0 = w.player().x;
      shootUntil(w, 40, [&] { return w.player().x > x0 + 10; });
      check(w.player().x > x0 + 10, "a Swap Rifle shot trades places with the golem");
      check(w.enemies()[0].x < x0 + 6, "and the golem is where the runner was");
    }

    {
      // A Prism Bat right ahead: a shot through it comes out in three.
      const auto level = make(testLevel("@ prism_bat 12 9\n", false));
      World w(level, 0, theme, art);
      idle(w, 4);
      int most = 0;
      for (int f = 0; f < 120 && most < 3; ++f)
      {
        w.update(press(false, false, f % 10 == 0, true));
        int split = 0;
        for (const auto& pr : w.projectiles())
          split += pr.alive && pr.precise && pr.kind == ShotKind::Normal;
        most = std::max(most, split);
      }
      check(most >= 3, "a Prism Bat splits a shot in three");
    }

    {
      // A Shard Golem: shots spark off its front and hurt its back.
      const auto front = make(testLevel("@ shard_golem 12 9\n", false));
      World w(front, 0, theme, art);
      idle(w, 2);
      const Enemy& g = w.enemies()[0];
      bool hurt = false;
      for (int f = 0; f < 50; ++f)
      {
        const bool facing = g.dir < 0; // its face to the runner
        const int hp = g.hp;
        w.update(press(false, false, f % 8 == 0, true));
        hurt = hurt || (facing && g.dir < 0 && g.hp < hp);
      }
      check(!hurt, "shots at its front spark off");
      const auto back = make(testLevel("@ shard_golem 24 9\n", false, false, 32));
      World b(back, 0, theme, art);
      idle(b, 2);
      for (int f = 0; f < 30; ++f)
        b.update(press(false, f < 2, false, false)); // face left, at its back
      bool backHurt = false;
      for (int f = 0; f < 80 && !backHurt; ++f)
      {
        const Enemy& e = b.enemies()[0];
        const bool away = e.dir < 0;
        const int hp = e.hp;
        b.update(press(false, false, f % 8 == 0, true));
        backHurt = away && (b.enemies()[0].hp < hp || !b.enemies()[0].alive);
      }
      check(backHurt, "a shot in its back hurts it");
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  if (gFailures)
    std::printf("%d FAILED\n", gFailures);
  else
    std::printf("ALL OK\n");
  return gFailures ? 1 : 0;
}
