// Level 43's open space on small test maps: the runner starts in the
// `pilot=1` ship and can't climb out in space, shots split a big asteroid
// in two and the halves again, a rock knocks the ship back, and a wrecked
// pilot ship is a death that respawns you in a new ship.

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

// Open space, 60 x 14 blocks, the ship at the left facing right. `extra`
// adds entities; `spikes` lines the map's floor with spikes, right under
// the ship.
std::string testLevel(const std::string& extra, bool spikes = false)
{
  std::string map;
  for (int y = 0; y < 14; ++y)
  {
    std::string row(60, '.');
    if (y == 7)
      row[3] = 'P';
    if (spikes && y >= 8)
      for (int x = 0; x < 12; ++x)
        row[std::size_t(x)] = y == 8 ? '^' : '#';
    row[59] = '#';
    map += row + "\n";
  }
  map[std::size_t(12 * 61 + 55)] = 'X';
  map[std::size_t(13 * 61 + 55)] = '#';
  return "name=SPACE TEST\nepisode=7\ntheme=starfall\nflags=space\n[map]\n" + map +
    "[entities]\n@ vehicle 2 7 kind=spaceship pilot=1\n" + extra;
}

PlayerInput press(bool right, bool fire, bool use, bool firstFrame)
{
  PlayerInput p;
  p.right = right;
  p.fire = {fire, fire && firstFrame};
  p.use = {use, use && firstFrame};
  return p;
}

int aliveRocks(const World& w, int size = 0)
{
  int n = 0;
  for (const auto& k : w.driftRocks())
    n += k.alive && (size == 0 || k.size == size);
  return n;
}

} // namespace

int main()
{
  SDL_Init(0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  {
    Renderer renderer(sdl);
    const Theme& theme = themeByIndex(themeIndexForKey("starfall"));
    const Art art = Art::build(theme, renderer);

    {
      // In the ship from the start; no climbing out in space.
      const auto level = std::make_shared<const Level>(Level::parse(testLevel("")));
      World w(level, 0, theme, art);
      check(w.hasStarfall(), "the level is open space");
      check(w.riding() != nullptr, "the runner starts in the pilot ship");
      check(!w.canLeaveVehicle(), "no getting out in space");
      for (int f = 0; f < 4; ++f)
        w.update(press(false, false, true, f == 0));
      check(w.riding() != nullptr, "still in the ship after pressing use");
    }

    {
      // A big rock straight ahead: shots push it and split it, twice.
      const auto level = std::make_shared<const Level>(Level::parse(testLevel("@ rock 14 5 size=3\n")));
      World w(level, 0, theme, art);
      check(aliveRocks(w, 6) == 1, "one big rock");
      const int x0 = w.driftRocks()[0].x;
      bool pushed = false, middle = false, small = false;
      for (int f = 0; f < 240; ++f)
      {
        w.update(press(false, f % 2 == 0, false, true));
        pushed = pushed || (w.driftRocks()[0].alive && w.driftRocks()[0].x > x0);
        middle = middle || aliveRocks(w, 4) >= 2;
        small = small || aliveRocks(w, 2) >= 2;
      }
      check(pushed, "shots push the rock along");
      check(middle, "the big rock splits into two middle ones");
      check(small, "and those into small ones");
    }

    {
      // A rock drifting into the ship dents it and knocks it back.
      const auto level = std::make_shared<const Level>(Level::parse(testLevel("@ rock 8 6 size=2 v=-8,0\n")));
      World w(level, 0, theme, art);
      const int hp = w.riding()->hp;
      bool knocked = false;
      for (int f = 0; f < 30; ++f)
      {
        w.update(press(false, false, false, false));
        knocked = knocked || (w.riding() && w.riding()->vx < 0);
      }
      check(w.riding() && w.riding()->hp < hp, "the rock dents the ship");
      check(knocked, "and knocks it back");
    }

    {
      // Parked over spikes until the armour gives out: a death, then a new
      // ship with you in it.
      const auto level = std::make_shared<const Level>(Level::parse(testLevel("", true)));
      World w(level, 0, theme, art);
      bool died = false, back = false;
      for (int f = 0; f < 900 && !back; ++f)
      {
        PlayerInput in;
        in.down = !died;
        w.update(in);
        died = died || w.player().state == PlayerState::Dying;
        back = died && w.player().state != PlayerState::Dying && w.riding() != nullptr;
      }
      check(died, "a wrecked pilot ship takes the runner with it");
      check(back, "the runner respawns in a new ship");
      check(w.stats().deaths == 1, "one death");
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
