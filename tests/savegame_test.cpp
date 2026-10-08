// Plays part of the level with the bot, saves, loads into a fresh world and
// checks that the reloaded world saves back to exactly the same file. Also
// checks that a file cut short is rejected.

#include "assets/art.hpp"
#include "data/level.hpp"
#include "data/theme.hpp"
#include "frontend/bot.hpp"
#include "game/savegame.hpp"
#include "game/world.hpp"
#include "render/renderer.hpp"

#include <SDL.h>

#include <cstdio>
#include <filesystem>
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

std::string slurp(const std::string& path)
{
  std::ifstream f(path);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

// The timestamp line differs between two writes; drop it.
std::string withoutTimestamp(std::string s)
{
  const auto at = s.find("\nsaved ");
  if (at != std::string::npos)
    s.erase(at, s.find('\n', at + 1) - at);
  return s;
}

} // namespace

int main(int argc, char** argv)
{
  const std::string levelPath = argc > 1 ? argv[1] : "levels/level1.txt";
  const std::string dir = (std::filesystem::temp_directory_path() / "gunrunners_savegame_test").string();
  std::filesystem::remove_all(dir);

  SDL_Init(0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  {
    Renderer renderer(sdl);
    const Theme& theme = themeByIndex(0);
    const Art art = Art::build(theme, renderer);
    const Level level = Level::loadFile(levelPath);

    for (int character = 0; character < kCharacterCount; ++character)
    {
      World world(level, character, theme, art);
      Bot bot;
      // Far enough to have killed things, broken boxes and caught the virus.
      for (int f = 0; f < 200; ++f)
      {
        const Input in = bot.play(world);
        PlayerInput p;
        p.left = in.left;
        p.right = in.right;
        p.up = in.up;
        p.down = in.down;
        p.jump = {in.jump, in.jump};
        p.fire = {in.fire, in.fire};
        world.update(p);
      }
      if (character == 1)
        world.switchCharacter(2); // a mid-level switch must survive a save too

      const std::string a = slotPath(dir, character);
      SaveGame s = world.snapshot();
      check(writeSave(s, a), "write save");
      const auto loaded = readSave(a);
      check(bool(loaded), "read save back");
      if (!loaded)
        continue;

      World fresh(level, loaded->character, theme, art);
      check(fresh.restore(*loaded), "restore into a fresh world");
      check(fresh.characterIndex() == world.characterIndex(), "same runner after load");
      check(fresh.stats().score == world.stats().score, "same score after load");
      const std::string b = slotPath(dir, 4);
      check(writeSave(fresh.snapshot(), b), "write reloaded save");
      check(withoutTimestamp(slurp(a)) == withoutTimestamp(slurp(b)), "reloaded world saves identically");

      // Keep playing after the load: the world must stay sane.
      Bot bot2;
      for (int f = 0; f < 100; ++f)
      {
        const Input in = bot2.play(fresh);
        PlayerInput p;
        p.right = in.right;
        p.left = in.left;
        p.jump = {in.jump, in.jump};
        p.fire = {in.fire, in.fire};
        fresh.update(p);
      }
      check(fresh.player().hp > 0 || fresh.player().state == PlayerState::Dying, "plays on after loading");
    }

    // A truncated file is no save at all.
    const std::string full = slurp(slotPath(dir, 0));
    {
      std::ofstream f(slotPath(dir, 3));
      f << full.substr(0, full.size() / 2);
    }
    check(!readSave(slotPath(dir, 3)), "truncated save is rejected");
    check(!readSave(dir + "/does-not-exist.sav"), "missing save is empty");
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::filesystem::remove_all(dir);
  std::printf("%s\n", gFailures == 0 ? "all savegame tests passed" : "SOME TESTS FAILED");
  return gFailures == 0 ? 0 : 1;
}
