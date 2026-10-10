// Level 47's Silk Lines on small test maps: walk off a ledge into a line
// and you slide down it to the far ledge; jump off and you keep its speed;
// a Loom Spider cuts the line under you; a `spin=1` line is spun as you
// come near; the Silk Shooter strings a line where it hits rock; cocoons
// pop open for gems (or the Virus); a Cocoon Pod lets its Dropling down.

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

// 40 x 20 blocks: a ledge x 0-9 with its top on row 8, a gap, a lower
// ledge x 30-39 with its top on row 14 (the gap falls off the map). `wall`
// puts a wall at x 20-21 instead of the gap's far side being open.
std::string testLevel(const std::string& extra, int start = 6, bool wall = false)
{
  std::string map;
  for (int y = 0; y < 20; ++y)
  {
    std::string row(40, '.');
    for (int x = 0; x < 40; ++x)
      if ((x <= 9 && y >= 8) || (x >= 30 && y >= 14) || (wall && x >= 20 && x <= 21 && y >= 4))
        row[std::size_t(x)] = '#';
    if (y == 7)
      row[std::size_t(start)] = 'P';
    map += row + "\n";
  }
  return "name=SILK TEST\nepisode=7\ntheme=silk_canyon\nweapon=silk_shooter\n[map]\n" + map + "[entities]\n" + extra;
}

PlayerInput press(bool right, bool left, bool fire, bool jump = false)
{
  PlayerInput p;
  p.right = right;
  p.left = left;
  p.fire = {fire, fire};
  p.jump = {jump, jump};
  return p;
}

void idle(World& w, int frames)
{
  for (int f = 0; f < frames; ++f)
    w.update(press(false, false, false));
}

constexpr const char* kLine = "@ silk 9 6 to=33,11\n";

} // namespace

int main()
{
  SDL_Init(0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  {
    Renderer renderer(sdl);
    const Theme& theme = themeByIndex(themeIndexForKey("silk_canyon"));
    const Art art = Art::build(theme, renderer);
    auto make = [&](const std::string& text) { return std::make_shared<const Level>(Level::parse(text)); };

    {
      // Off the ledge into the line, down it, onto the far ledge.
      const auto level = make(testLevel(kLine));
      World w(level, 0, theme, art);
      idle(w, 4);
      check(w.silkLines().size() == 1, "one line");
      bool rode = false;
      for (int f = 0; f < 120; ++f)
      {
        w.update(press(f < 12, false, false));
        rode = rode || w.player().silk >= 0;
      }
      check(rode, "walking off the ledge catches the line");
      check(w.player().x >= 60 && w.player().state == PlayerState::OnGround && w.player().hp > 0,
        "slid down it onto the far ledge");
    }

    {
      // Jump off it halfway: carried on at its speed.
      const auto level = make(testLevel(kLine));
      World w(level, 0, theme, art);
      idle(w, 4);
      int f = 0;
      for (; f < 60 && w.player().silk < 0; ++f)
        w.update(press(true, false, false));
      for (int k = 0; k < 8; ++k)
        w.update(press(false, false, false));
      const float v = w.player().silkV;
      w.update(press(false, false, false, true));
      check(v > 1.0f && w.player().state == PlayerState::Jumping && w.player().fling >= 1,
        "jumping off keeps the line's speed");
    }

    for (int i = 0; i < 2; ++i)
    {
      // A Loom Spider cuts the line you ride (placed on it, or `spider=1`).
      const auto level = make(testLevel(i == 0 ? std::string(kLine) + "@ loom_spider 14 7\n" : "@ silk 9 6 to=33,11 spider=1\n"));
      World w(level, 0, theme, art);
      w.cheat(Cheat::God);
      idle(w, 2);
      check(w.enemies()[0].kind == EnemyKind::LoomSpider && w.enemies()[0].attach == 1, "the spider is on the line");
      bool cut = false;
      for (int f = 0; f < 60 && !cut; ++f)
      {
        w.update(press(f < 12, false, false));
        cut = w.silkLines()[0].spun == 0.0f && w.player().silk < 0 && w.player().state == PlayerState::Falling;
      }
      check(cut, "it cuts the line under you");
      bool back = false;
      for (int f = 0; f < 200 && !back; ++f)
      {
        w.update(press(false, false, false));
        back = w.silkLines()[0].whole();
      }
      check(back, "and spins it again");
    }

    {
      // A line spun as you come near.
      const auto level = make(testLevel("@ silk 9 6 to=33,11 spin=1\n"));
      World w(level, 0, theme, art);
      check(w.silkLines()[0].spun == 0.0f, "not there at first");
      idle(w, 40);
      check(w.silkLines()[0].whole(), "spun out as you come near");
    }

    {
      // The Silk Shooter: a line from your hands down to where it hits.
      const auto level = make(testLevel("", 9, true));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      check(w.player().weapon == Weapon::Proto && w.player().proto == int(ProtoId::SilkShooter), "holding the Silk Shooter");
      idle(w, 2);
      w.update(press(false, false, true));
      idle(w, 20);
      const bool made = !w.silkLines().empty() && w.silkLines().back().whole() && w.silkLines().back().by > w.silkLines().back().ay;
      check(made, "a shot into rock strings a line down to it");
      bool rode = false;
      for (int f = 0; f < 40; ++f)
      {
        w.update(press(f < 6, false, false));
        rode = rode || w.player().silk >= 0;
      }
      check(rode, "stepping off into it, you ride it");
    }

    {
      // Cocoons: shot open for gems; the green one gives you the Virus.
      const auto level = make(testLevel("@ cocoon 6 7 gems=4\n@ cocoon 8 7 virus=1\n", 2));
      World w(level, 0, theme, art);
      idle(w, 2);
      const int gems = w.stats().gemsTotal;
      for (int f = 0; f < 20; ++f)
        w.update(press(false, false, f == 0));
      check(w.stats().gemsTotal == gems + 4, "a shot cocoon pours out its gems");
      for (int f = 0; f < 40 && w.player().virus == 0; ++f)
        w.update(press(true, false, false));
      check(w.player().virus > 0, "touching the green one infects");
    }

    {
      // A Cocoon Pod lets its Dropling down as you pass under.
      const auto level = make(testLevel("@ cocoon_pod 6 3\n", 2));
      World w(level, 0, theme, art);
      w.cheat(Cheat::God);
      idle(w, 2);
      check(w.enemies().size() == 2 && w.enemies()[1].kind == EnemyKind::Dropling && w.enemies()[1].hidden,
        "its Dropling waits in the pod");
      bool down = false;
      for (int f = 0; f < 40 && !down; ++f)
      {
        w.update(press(f < 8, false, false));
        down = !w.enemies()[1].hidden && w.enemies()[1].y > w.enemies()[0].y + 4;
      }
      check(down, "and drops it as you pass under");
      bool home = false;
      for (int f = 0; f < 80 && !home; ++f)
      {
        w.update(press(false, true, false));
        home = w.enemies()[1].hidden;
      }
      check(home, "then climbs back into it");
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
