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
#include <cstdlib>
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
  const int playFrames = argc > 2 ? std::atoi(argv[2]) : 200;
  const std::string dir = (std::filesystem::temp_directory_path() / "gunrunners_savegame_test").string();
  std::filesystem::remove_all(dir);

  SDL_Init(0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  {
    Renderer renderer(sdl);
    const Theme& theme = themeByIndex(0);
    const Art art = Art::build(theme, renderer);
    const auto level = std::make_shared<const Level>(Level::loadFile(levelPath));

    for (int character = 0; character < characterCount(); ++character)
    {
      World world(level, character, theme, art);
      Bot bot;
      // Far enough to have killed things, broken boxes and caught the virus.
      for (int f = 0; f < playFrames; ++f)
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
      {
        // The map comes back block for block.
        bool same = !s.explored.empty();
        for (int ty = 0; ty < level->height && same; ++ty)
          for (int tx = 0; tx < level->width && same; ++tx)
            same = fresh.explored(tx, ty) == world.explored(tx, ty);
        check(same && world.explored(level->startTx, level->startTy), "explored map after load");
      }
      const std::string b = dir + "/reloaded.sav";
      check(writeSave(fresh.snapshot(), b), "write reloaded save");
      const std::string sa = withoutTimestamp(slurp(a)), sb = withoutTimestamp(slurp(b));
      check(sa == sb, "reloaded world saves identically");
      if (sa != sb)
      {
        std::istringstream ia(sa), ib(sb);
        std::string la, lb;
        while (std::getline(ia, la) && std::getline(ib, lb))
          if (la != lb)
            std::printf("     first difference: '%s' vs '%s'\n", la.c_str(), lb.c_str());
      }

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

    // A custom runner: stored in runners.txt, and carried in the save so
    // the save still loads once the runner is deleted.
    {
      RunnerParts parts;
      parts.body = 1;
      parts.hair = 2;
      parts.glow = 5;
      const CharacterDef custom = makeCustomRunner("c0test01", "Tin Can!", parts, Weapon::Rocket, 4, 4, 2);
      check(custom.name == "TIN CAN!" && custom.maxHp == 10 && custom.startAmmo == 12, "custom runner stats");
      const CharacterDef greedy = makeCustomRunner("c0test02", "", parts, Weapon::Laser, 5, 5, 5);
      check(runnerPointsLeft(greedy.healthPips, greedy.jumpPips, greedy.powerPips) == 0 && greedy.name == "RUNNER",
        "custom runner stays within the budget");
      const int index = putCustomRunner(custom);
      check(index == kDefaultRunners && characterByIndex(index).id == "c0test01", "custom runner joins the roster");
      check(saveCustomRunners(dir), "write runners.txt");
      clearCustomRunners();
      loadCustomRunners(dir);
      check(characterCount() == kDefaultRunners + 1 && encodeRunner(characterByIndex(index)) == encodeRunner(custom),
        "runners.txt round trip");

      World world(level, index, theme, art);
      check(world.player().hp == 10 && world.player().ammo == 12, "custom runner starts with its stats");
      Bot bot;
      for (int f = 0; f < 60; ++f)
      {
        const Input in = bot.play(world);
        PlayerInput p;
        p.right = in.right;
        p.left = in.left;
        p.jump = {in.jump, in.jump};
        p.fire = {in.fire, in.fire};
        world.update(p);
      }
      const std::string a = dir + "/custom.sav";
      check(writeSave(world.snapshot(), a), "write custom runner save");
      removeCustomRunner("c0test01");
      check(runnerIndexById("c0test01") < 0, "custom runner deleted");
      const auto loaded = readSave(a);
      check(loaded && !loaded->runner.empty(), "save carries the custom runner");
      if (loaded)
      {
        World fresh(level, 0, theme, art);
        check(fresh.restore(*loaded), "restore a deleted custom runner's save");
        check(fresh.character().name == "TIN CAN!" && fresh.character().maxHp == 10, "the runner comes back");
        check(characterByIndex(fresh.characterIndex()).transient, "brought back without being saved again");
        clearCustomRunners();
      }
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
