// The runner's reflection (`@ deco kind=reflection`, Level 2's easter egg and
// Level 10's odd mirror) shows only while the runner stands in its glass:
// anywhere else the frame is pixel for pixel the frame without the deco (it
// once followed the runner round all of Level 2 as a ghost). In the glass it
// does draw.

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

// A 20 x 11 room, all of it on screen: ceiling row 0, floor rows 9-10,
// walls at x 0 and 19, the runner at (px, 8), plus `entities`.
std::string room(int px, const std::string& entities)
{
  std::vector<std::string> map(11, std::string(20, '.'));
  for (int x = 0; x < 20; ++x)
    map[0][std::size_t(x)] = map[9][std::size_t(x)] = map[10][std::size_t(x)] = '#';
  for (int y = 0; y < 11; ++y)
    map[std::size_t(y)][0] = map[std::size_t(y)][19] = '#';
  map[8][std::size_t(px)] = 'P';
  std::string s = "name=REFLECTION TEST\nepisode=1\ntheme=glass_canyon\nweapon=spark_disc\n[map]\n";
  for (const auto& row : map)
    s += row + "\n";
  return s + "[entities]\n" + entities;
}

// The frame a level draws after `frames` idle logic frames.
std::vector<unsigned char> frameOf(const std::string& text, SDL_Surface* surface, Renderer& renderer,
  const Theme& theme, const Art& art, int frames)
{
  const auto level = std::make_shared<const Level>(Level::parse(text));
  World w(level, 2, theme, art);
  for (int f = 0; f < frames; ++f)
    w.update(PlayerInput{});
  SDL_SetRenderDrawColor(renderer.sdl(), 0, 0, 0, 255);
  SDL_RenderClear(renderer.sdl());
  w.draw(renderer, frames, 0.0f);
  SDL_RenderPresent(renderer.sdl());
  const auto* px = static_cast<const unsigned char*>(surface->pixels);
  return std::vector<unsigned char>(px, px + std::size_t(surface->pitch * surface->h));
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
    const std::vector<std::pair<std::string, std::string>> glasses{
      {"Level 2's glass", "@ deco kind=reflection rect=11,5,17,8\n"},
      {"Level 10's odd mirror", "@ deco 14 6 kind=reflection w=2 h=3 text=odd rect=14,6,15,8\n"},
    };
    for (const auto& [name, deco] : glasses)
    {
      // Away from the glass (x 4): no reflection anywhere on screen. The
      // odd mirror's gilt frame still draws, so compare it with the runner
      // in a room where the frame is out of view instead.
      const bool odd = deco.find("odd") != std::string::npos;
      const auto away = frameOf(room(4, deco), surface, renderer, theme, art, 20);
      const auto plain = frameOf(room(4, ""), surface, renderer, theme, art, 20);
      std::string what = name + ": no reflection of a runner away from it";
      if (!odd)
        check(away == plain, what.c_str());
      // In the glass (x 14): it draws.
      const auto in = frameOf(room(14, deco), surface, renderer, theme, art, 20);
      const auto inPlain = frameOf(room(14, ""), surface, renderer, theme, art, 20);
      what = name + ": a reflection of a runner in it";
      check(in != inPlain, what.c_str());
      if (odd)
      {
        // The odd mirror away from the runner: the frame alone, so the same
        // as with the runner a step further off.
        const auto further = frameOf(room(2, deco), surface, renderer, theme, art, 20);
        const auto furtherPlain = frameOf(room(2, ""), surface, renderer, theme, art, 20);
        std::size_t diffAway = 0, diffFurther = 0;
        for (std::size_t i = 0; i < away.size(); ++i)
        {
          diffAway += away[i] != plain[i];
          diffFurther += further[i] != furtherPlain[i];
        }
        what = name + ": no reflection of a runner away from it";
        check(diffAway == diffFurther, what.c_str());
      }
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf("%s\n", gFailures == 0 ? "all reflection tests passed" : "reflection tests FAILED");
  return gFailures == 0 ? 0 : 1;
}
