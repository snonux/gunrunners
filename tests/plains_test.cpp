// Level 48's Bounders and the Tamer's Whip on small test maps: climb on a
// Bounder and it hops twice as high as you jump; it runs over thorn grass
// that hurts on foot and flattens a Thorn Hog; landing on its back mounts
// it; it won't fit a low tunnel you walk through; got off far from home, it
// trots back; the whip stuns a hog and calls a Bounder to you; a Sky Gulper
// spits a runner on foot back but can't swallow one on a Bounder; the green
// thornbush gives you the Virus.

#include "assets/art.hpp"
#include "data/level.hpp"
#include "data/theme.hpp"
#include "game/world.hpp"
#include "render/renderer.hpp"

#include <SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
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

// 60 x 20 blocks, the ground's top on row 14. `thorns`: thorn grass on
// the ground's top row at x 20-29 (the ground under it one row lower).
// `tunnel`: a rock roof at x 30-39 over rows 0-10 (a 3-block-high tunnel).
std::string testLevel(const std::string& extra, int start = 4, bool thorns = false, bool tunnel = false)
{
  std::string map;
  for (int y = 0; y < 20; ++y)
  {
    std::string row(60, '.');
    for (int x = 0; x < 60; ++x)
    {
      if (y >= 14 || x == 0 || x == 59)
        row[std::size_t(x)] = '#';
      if (thorns && x >= 20 && x <= 29 && y == 14)
        row[std::size_t(x)] = '^';
      if (tunnel && x >= 30 && x <= 39 && y <= 10)
        row[std::size_t(x)] = '#';
    }
    if (y == 13)
      row[std::size_t(start)] = 'P';
    map += row + "\n";
  }
  return "name=PLAINS TEST\nepisode=7\ntheme=bounder_plains\nweapon=tamers_whip\n[map]\n" + map + "[entities]\n" + extra;
}

PlayerInput press(int dx, bool jump = false, bool fire = false, bool use = false)
{
  PlayerInput p;
  p.right = dx > 0;
  p.left = dx < 0;
  p.jump = {jump, jump};
  p.fire = {fire, fire};
  p.use = {use, use};
  return p;
}

void idle(World& w, int frames)
{
  for (int f = 0; f < frames; ++f)
    w.update(press(0));
}

// Walks to the Bounder and climbs on with USE.
bool mount(World& w)
{
  for (int f = 0; f < 80 && !w.riding(); ++f)
  {
    const Vehicle& v = w.vehicles()[0];
    const int dx = v.x + v.w / 2 - 1 - w.player().x;
    w.update(press(std::abs(dx) > 1 ? (dx > 0 ? 1 : -1) : 0, false, false, f % 2 == 0));
  }
  return w.riding() != nullptr;
}

int highest(World& w, int frames, int dx)
{
  int top = w.player().y;
  for (int f = 0; f < frames; ++f)
  {
    w.update(press(dx, true));
    top = std::min(top, w.player().y);
  }
  return top;
}

} // namespace

int main()
{
  SDL_Init(0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  {
    Renderer renderer(sdl);
    const Theme& theme = themeByIndex(themeIndexForKey("bounder_plains"));
    const Art art = Art::build(theme, renderer);
    auto make = [&](const std::string& text) { return std::make_shared<const Level>(Level::parse(text)); };

    {
      // On foot vs on its back: it hops twice as high.
      const auto level = make(testLevel("@ bounder 10 13\n"));
      World w(level, 0, theme, art);
      idle(w, 4);
      check(w.vehicles().size() == 1 && w.vehicles()[0].kind == VehicleKind::Bounder, "a Bounder from @ bounder");
      const int ground = w.player().y;
      const int onFoot = ground - highest(w, 20, 0);
      check(mount(w), "USE climbs on");
      idle(w, 6);
      const int saddle = w.player().y;
      const int riding = saddle - highest(w, 30, 0);
      std::printf("     jump %d cells on foot, %d on the Bounder\n", onFoot, riding);
      check(riding >= 2 * onFoot - 1, "it hops about twice as high");
      idle(w, 30);
      bool off = false;
      for (int f = 0; f < 4 && !off; ++f)
      {
        PlayerInput in = press(0, f == 1);
        in.down = true;
        w.update(in);
        off = !w.riding();
      }
      check(off, "down + jump gets off");
    }

    {
      // Thorn grass: it hurts on foot, not on a Bounder; a Bounder runs a
      // Thorn Hog down.
      const auto level = make(testLevel("@ bounder 10 13\n@ thorn_hog 26 12\n", 4, true));
      World w(level, 0, theme, art);
      idle(w, 4);
      check(mount(w), "climbs on");
      const int hp = w.player().hp, vhp = w.vehicles()[0].hp;
      for (int f = 0; f < 60; ++f)
        w.update(press(1));
      check(w.riding() && w.riding()->x > 30 * 2, "rode over the thorns");
      check(w.player().hp == hp && w.vehicles()[0].hp == vhp, "unhurt by them");
      check(!w.enemies()[0].alive, "the Thorn Hog flattened");
    }
    {
      const auto level = make(testLevel("", 16, true));
      World w(level, 0, theme, art);
      idle(w, 4);
      const int hp = w.player().hp;
      for (int f = 0; f < 20; ++f)
        w.update(press(1));
      check(w.player().hp < hp, "on foot, thorns hurt");
    }

    {
      // Landing on its back puts you in the saddle.
      const auto level = make(testLevel("@ bounder 8 13\n", 9));
      World w(level, 0, theme, art);
      idle(w, 4);
      check(!w.riding(), "standing beside it");
      bool on = false;
      for (int f = 0; f < 40 && !on; ++f)
      {
        w.update(press(0, f < 12));
        on = w.riding() != nullptr;
      }
      check(on, "jump and come down on its back: riding");
    }

    {
      // A low tunnel: on foot through it, the Bounder can't follow.
      const auto level = make(testLevel("@ bounder 20 13\n", 4, false, true));
      World w(level, 0, theme, art);
      idle(w, 4);
      check(mount(w), "climbs on");
      for (int f = 0; f < 60; ++f)
        w.update(press(1));
      check(w.riding() && w.riding()->x + w.riding()->w <= 30 * 2, "it won't fit the tunnel");
      for (int f = 0; f < 4 && w.riding(); ++f)
        w.update(press(0, false, false, f % 2 == 0));
      for (int f = 0; f < 60; ++f)
        w.update(press(1));
      check(!w.riding() && w.player().x > 40 * 2, "on foot you walk through");
    }

    {
      // Got off far from its pen, it trots home.
      const auto level = make(testLevel("@ bounder 6 13\n", 4));
      World w(level, 0, theme, art);
      idle(w, 4);
      const int home = w.vehicles()[0].x;
      check(mount(w), "climbs on");
      for (int f = 0; f < 40; ++f)
        w.update(press(1));
      for (int f = 0; f < 4 && w.riding(); ++f)
        w.update(press(0, false, false, f % 2 == 0));
      check(!w.riding() && w.vehicles()[0].x > home + 20, "left it far from home");
      idle(w, 160);
      check(std::abs(w.vehicles()[0].x - home) <= 2, "it trotted back to its pen");
    }

    {
      // The whip: a hog in reach is dazed; a Bounder in earshot comes.
      const auto level = make(testLevel("@ thorn_hog 9 13\n@ bounder 18 13\n", 4));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      check(w.player().weapon == Weapon::Proto && w.player().proto == int(ProtoId::TamersWhip), "holding the whip");
      idle(w, 2);
      const int far = w.vehicles()[0].x;
      w.update(press(1, false, true));
      w.update(press(0));
      const Enemy& hog = w.enemies()[0];
      check(hog.stun > 0 || !hog.alive, "the crack dazes the hog");
      check(w.vehicles()[0].call > 0, "and calls the Bounder");
      idle(w, 80);
      check(w.vehicles()[0].x < far - 10, "which trots over");
    }

    {
      // A Sky Gulper spits a runner on foot back, unhurt.
      const auto level = make(testLevel("@ sky_gulper 14 13\n", 4));
      World w(level, 0, theme, art);
      idle(w, 2);
      const int hp = w.player().hp;
      int furthest = w.player().x;
      bool gulped = false;
      for (int f = 0; f < 60 && !gulped; ++f)
      {
        w.update(press(1));
        gulped = w.player().x < furthest - 6;
        furthest = std::max(furthest, w.player().x);
      }
      check(gulped, "walk into a Sky Gulper: spat back");
      check(w.player().hp == hp, "unhurt");
    }
    {
      // On a Bounder it can't swallow you.
      const auto level = make(testLevel("@ bounder 6 13\n@ sky_gulper 18 13\n", 4));
      World w(level, 0, theme, art);
      idle(w, 2);
      check(mount(w), "climbs on");
      int furthest = w.riding()->x;
      bool back = false;
      for (int f = 0; f < 60; ++f)
      {
        w.update(press(1));
        if (!w.riding())
          break;
        back = back || w.riding()->x < furthest - 4;
        furthest = std::max(furthest, w.riding()->x);
      }
      check(w.riding() && !back && furthest > 24 * 2, "rides on past it");
    }

    {
      // The green thornbush: the Virus.
      const auto level = make(testLevel("@ thornbush 10 13 carrier=1\n", 4));
      World w(level, 0, theme, art);
      idle(w, 2);
      for (int f = 0; f < 30 && w.player().virus == 0; ++f)
        w.update(press(1));
      check(w.player().virus > 0, "the green thornbush infects");
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
