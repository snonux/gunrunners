// Crawlspaces, played by hand in a small room: a tunnel 4 cells high can't
// be walked into standing, down and the way you face crawls through it at
// half the walking speed, the runner stays crouched under the low ceiling
// with down let go (or up held), and out in the open down and a direction
// only crouches.

#include "assets/art.hpp"
#include "data/level.hpp"
#include "data/theme.hpp"
#include "game/world.hpp"
#include "render/renderer.hpp"

#include <SDL.h>

#include <cstdio>
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

// A 40 x 24 block room with a floor on block rows 22 and 23, the runner at
// block (3, 21) and, with `tunnel`, solid rock over blocks 15-30 down to row
// 19, which leaves a passage 2 blocks (4 cells) high above the floor: cells
// 30-61 across, the runner's feet on cell row 43.
std::string room(bool tunnel)
{
  const int h = 24;
  std::vector<std::string> map(std::size_t(h), std::string(40, '.'));
  for (int y = 0; y < h; ++y)
    map[std::size_t(y)][0] = map[std::size_t(y)][1] = map[std::size_t(y)][38] = map[std::size_t(y)][39] = '#';
  for (int x = 0; x < 40; ++x)
    map[std::size_t(h - 2)][std::size_t(x)] = map[std::size_t(h - 1)][std::size_t(x)] = '#';
  if (tunnel)
    for (int y = 0; y <= 19; ++y)
      for (int x = 15; x <= 30; ++x)
        map[std::size_t(y)][std::size_t(x)] = '#';
  map[21][3] = 'P';
  std::string s = "name=CRAWL TEST\nepisode=1\ntheme=neon_rooftops\nweapon=pulse_pistol\n[map]\n";
  for (const auto& row : map)
    s += row + "\n";
  s += "[entities]\n";
  return s;
}

struct Keys
{
  bool left = false, right = false, down = false, up = false;
};

void hold(World& w, Keys pad, int frames)
{
  for (int f = 0; f < frames; ++f)
  {
    PlayerInput p;
    p.left = pad.left;
    p.right = pad.right;
    p.up = pad.up;
    p.down = pad.down;
    w.update(p);
  }
}

} // namespace

int main()
{
  SDL_Init(0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  {
    Renderer renderer(sdl);
    const Theme& theme = themeByIndex(0);
    const Art art = Art::build(theme, renderer);
    auto make = [&](bool tunnel, int character) {
      return World(std::make_shared<const Level>(Level::parse(room(tunnel))), character, theme, art);
    };
    Keys right, crawl, up, none;
    right.right = true;
    crawl.right = crawl.down = true;
    up.up = true;
    for (int character = 0; character < 6; ++character)
    {
      std::printf("runner %d\n", character);
      // Walking: blocked at the tunnel mouth.
      World w = make(true, character);
      hold(w, none, 10);
      const int x0 = w.player().x;
      hold(w, right, 60);
      const int walked = w.player().x - x0;
      check(w.player().x + Player::kWidth <= 30, "standing, the runner can't walk into a tunnel 4 cells high");

      // Crawling: in and through at half speed.
      const int xc = w.player().x;
      hold(w, crawl, 20);
      const bool in = w.player().x > 30 && w.player().visual == PlayerVisual::Crouching;
      check(in, "down and right crawls into the tunnel");
      check(w.player().x - xc <= 11, "crawling is slower than walking");

      // Let go, then hold up: still crouched under the low ceiling.
      hold(w, none, 10);
      const bool stays = w.player().visual == PlayerVisual::Crouching && w.player().x > 30;
      hold(w, up, 10);
      check(stays && w.player().visual == PlayerVisual::Crouching && w.player().y == 43,
            "the runner stays crouched under the low ceiling");

      hold(w, crawl, 120);
      check(w.player().x > 61, "crawling comes out the far end");
      hold(w, none, 10);
      check(w.player().visual != PlayerVisual::Crouching, "out of the tunnel the runner stands up again");

      // Out in the open: down and right only crouches.
      World o = make(false, character);
      hold(o, none, 10);
      const int xo = o.player().x;
      hold(o, crawl, 30);
      check(o.player().x == xo && o.player().visual == PlayerVisual::Crouching,
            "out in the open, down and a direction only crouches");
      check(walked > 0, "the runner walks up to the tunnel");
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf("%s\n", gFailures ? "FAILED" : "passed");
  return gFailures ? 1 : 0;
}
