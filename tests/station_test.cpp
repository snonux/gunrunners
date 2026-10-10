// Level 15's Breach and Vent, played by hand: a Breach Charge thrown and
// set off opens the bonus panel (and the vent carries the runner through
// it), blows the cracked wall into the secret crawl, and opens the training
// bay's panel, whose vent takes the loose crates out while the runner holds
// the handrail. Then the bot flies the Asteroid Belt bonus on recoil alone.

#include "assets/art.hpp"
#include "data/level.hpp"
#include "data/theme.hpp"
#include "frontend/bot.hpp"
#include "game/world.hpp"
#include "render/renderer.hpp"

#include <SDL.h>

#include <cstdio>
#include <fstream>
#include <sstream>
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

// Level 15 with the runner moved to block (x, y).
std::string levelWithStart(const std::string& path, int x, int y)
{
  std::ifstream f(path);
  std::stringstream ss;
  ss << f.rdbuf();
  std::string text = ss.str();
  std::stringstream in(text);
  std::string line, out;
  bool map = false;
  int row = 0;
  while (std::getline(in, line))
  {
    if (line == "[map]")
      map = true;
    else if (!line.empty() && line[0] == '[')
      map = false;
    else if (map)
    {
      for (auto& c : line)
        if (c == 'P')
          c = '.';
      if (row == y && x < int(line.size()))
        line[std::size_t(x)] = 'P';
      ++row;
    }
    out += line + "\n";
  }
  return out;
}

struct Pad
{
  bool left = false, right = false, fire = false;
};

void step(World& w, Pad pad, Pad& prev)
{
  PlayerInput p;
  p.left = pad.left;
  p.right = pad.right;
  p.fire = {pad.fire, pad.fire && !prev.fire};
  w.update(p);
  prev = pad;
}

// Faces `dir`, throws a charge, lets it stick, then holds fire to set it off.
void breach(World& w, int dir)
{
  Pad prev;
  Pad face;
  face.left = dir < 0;
  face.right = dir > 0;
  step(w, face, prev);
  step(w, Pad{}, prev);
  Pad fire;
  fire.fire = true;
  step(w, fire, prev);
  for (int f = 0; f < 8; ++f)
    step(w, Pad{}, prev);
  for (int f = 0; f < 12; ++f)
    step(w, fire, prev);
  step(w, Pad{}, prev);
}

} // namespace

int main(int argc, char** argv)
{
  const std::string path = argc > 1 ? argv[1] : "levels/15_hangar_bay.txt";
  SDL_Init(0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  {
    Renderer renderer(sdl);
    const Theme& theme = themeByIndex(0);
    const Art art = Art::build(theme, renderer);

    {
      // The bonus panel at the end of the secret crawl.
      const auto level = std::make_shared<const Level>(Level::parse(levelWithStart(path, 152, 11)));
      World w(level, 0, theme, art);
      check(w.cheat(Cheat::Ammo), "Breach Charges in hand");
      breach(w, 1);
      bool opened = false;
      for (const auto& pn : w.station().panels)
        opened = opened || (pn.bonus && pn.open > 0);
      check(opened, "the bonus panel opens to a charge");
      for (int f = 0; f < 60 && !w.bonusRequested(); ++f)
        w.update(PlayerInput{});
      check(w.bonusRequested(), "its vent carries the runner into the bonus level");
    }
    {
      // The cracked wall at the east end of the cab deck.
      const auto level = std::make_shared<const Level>(Level::parse(levelWithStart(path, 117, 11)));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      breach(w, 1);
      bool broken = false;
      for (const auto& b : w.breakables())
        broken = broken || (b.broken && b.x0 == 121);
      check(broken, "a charge blows the cracked wall");
    }
    {
      // The training bay: the runner on the handrail stays, the crates go.
      const auto level = std::make_shared<const Level>(Level::parse(levelWithStart(path, 6, 13)));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      int before = 0;
      for (const auto& c : w.station().crates)
        before += c.alive && c.bx < 36 && c.by < 14;
      const int x0 = w.player().x;
      breach(w, -1);
      for (int f = 0; f < 60; ++f)
        w.update(PlayerInput{});
      int after = 0;
      for (const auto& c : w.station().crates)
        after += c.alive && c.bx < 36 && c.by < 14;
      check(before == 3 && after == 0, "the bay's vent takes its crates out into space");
      check(w.player().x - x0 < 4 && w.player().x - x0 > -4 && w.station().caught < 0,
        "the runner holding the handrail stays put");
    }
    {
      // The Asteroid Belt: 40 gems inside the minute.
      const std::string belt = argc > 2 ? argv[2] : "levels/15_bonus_asteroid_belt.txt";
      const auto level = std::make_shared<const Level>(Level::loadFile(belt));
      World w(level, 0, theme, art);
      Bot bot;
      Input prev;
      for (int f = 0; f < 900 && w.state() == WorldState::Playing; ++f)
      {
        const Input in = bot.play(w);
        PlayerInput p;
        p.left = in.left;
        p.right = in.right;
        p.up = in.up;
        p.down = in.down;
        p.fire = {in.fire, in.fire && !prev.fire};
        prev = in;
        w.update(p);
      }
      check(w.stats().gems >= 40 && w.state() == WorldState::Exiting, "the bot flies the belt and takes all 40 gems");
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf("%s\n", gFailures == 0 ? "all station tests passed" : "SOME TESTS FAILED");
  return gFailures == 0 ? 0 : 1;
}
