// Level 45's Gullet Tubes on a small test map: walking into a mouth swallows
// the runner and spits them out at the far end, a shot turns a valve (and
// with it the branch), a Bile Blaster shot fired into a mouth comes out of
// the far end, and a breathing mouth only swallows while the hive breathes in.

#include "assets/art.hpp"
#include "data/level.hpp"
#include "data/theme.hpp"
#include "game/world.hpp"
#include "render/renderer.hpp"

#include <SDL.h>

#include <cstdio>
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

// Two rooms (x 1-13 and 20-38, ground 10) with solid rock between; tube
// `a` runs from a mouth in room A's east wall to room B, tube `v` from a
// floor mouth in room B to its west end or (the valve in room B turned)
// back to room A. Tube `b` breathes, from room B's east wall to room A.
std::string testLevel(bool breathing)
{
  std::string map;
  for (int y = 0; y < 14; ++y)
  {
    std::string row(40, '#');
    for (int x = 0; x < 40; ++x)
      if (y >= 6 && y <= 9 && ((x >= 1 && x <= 13) || (x >= 20 && x <= 38)))
        row[std::size_t(x)] = '.';
    if (y == 9)
      row[3] = 'P';
    map += row + "\n";
  }
  std::string s = "name=HIVE TEST\nepisode=7\ntheme=hive_gullets\nweapon=bile_blaster\n[map]\n" + map + "[entities]\n";
  if (!breathing)
    s += "@ tube a path=14,9;16,12;18,12;19,9 in=l out=r\n"
         "@ tube v path=34,10;34,12;22,12;19,9 in=u out=r alt=34,10;34,13;10,13;10,10 altout=u valve=30,8\n";
  else
    s += "@ tube b path=14,9;16,12;18,12;19,9 in=l out=r breath=1\n";
  return s;
}

PlayerInput press(bool right, bool left, bool fire, bool firstFrame)
{
  PlayerInput p;
  p.right = right;
  p.left = left;
  p.fire = {fire, fire && firstFrame};
  return p;
}

} // namespace

int main()
{
  SDL_Init(0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  {
    Renderer renderer(sdl);
    const Theme& theme = themeByIndex(themeIndexForKey("hive_gullets"));
    const Art art = Art::build(theme, renderer);
    const auto level = std::make_shared<const Level>(Level::parse(testLevel(false)));

    {
      // Walk right into tube a's mouth.
      World w(level, 0, theme, art);
      check(w.gulletTubes().size() == 2, "two tubes");
      bool swallowed = false;
      for (int f = 0; f < 200 && w.player().x < 40; ++f)
      {
        w.update(press(true, false, false, false));
        swallowed = swallowed || w.player().tube >= 0;
      }
      check(swallowed, "the mouth swallows the runner");
      check(w.player().tube < 0 && w.player().x >= 40, "spat out in the next room");
      for (int f = 0; f < 10; ++f)
        w.update(press(false, false, false, false));
      check(w.player().state == PlayerState::OnGround && w.player().x >= 40, "standing in the next room");

      // Shoot the valve (to the right, at head height): tube v changes branch.
      check(w.gulletTubes()[1].set == 0, "valve starts on the main branch");
      for (int f = 0; f < 40 && w.gulletTubes()[1].set == 0; ++f)
        w.update(press(false, false, f % 6 == 0, true));
      check(w.gulletTubes()[1].set == 1, "a shot turns the valve");
    }

    {
      // A Bile Blaster shot into tube a's mouth comes out in room B.
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      check(w.player().weapon == Weapon::Proto && w.player().proto == int(ProtoId::BileBlaster), "holding the Bile Blaster");
      for (int f = 0; f < 6; ++f)
        w.update(press(true, false, false, false));
      bool out = false;
      for (int f = 0; f < 90 && !out; ++f)
      {
        w.update(press(false, false, f == 2, true));
        for (const auto& pr : w.projectiles())
          out = out || (pr.alive && pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::BileBlaster) && pr.x >= 40);
      }
      check(out, "a Bile Blaster shot rides the tube to the far end");
      check(w.player().tube < 0 && w.player().x < 28, "the runner stayed behind");
    }

    {
      // A breathing mouth: shut while the hive breathes out.
      const auto breathing = std::make_shared<const Level>(Level::parse(testLevel(true)));
      World w(breathing, 0, theme, art);
      const auto& t = w.gulletTubes().front();
      bool shut = false, open = false;
      for (int f = 0; f < World::kBreathPeriod; ++f)
      {
        (w.mouthOpen(t) ? open : shut) = true;
        w.update(press(false, false, false, false));
      }
      check(open && shut, "the breathing mouth opens and shuts");
      // Walk into it: the runner waits at the shut mouth, then goes in.
      int swallowedAt = -1;
      for (int f = 0; f < 400 && swallowedAt < 0; ++f)
      {
        const bool wasOpen = w.mouthOpen(t);
        w.update(press(true, false, false, false));
        if (w.player().tube >= 0)
        {
          swallowedAt = f;
          check(wasOpen || w.mouthOpen(t), "it only swallows while open");
        }
      }
      check(swallowedAt >= 0, "the breathing mouth swallows the runner");
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
