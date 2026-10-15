// HARD: every campaign level gets extra enemies, bonus levels none. For each
// level file: the extras stand in open blocks inside the map, away from the
// start and the checkpoints; the same file always gets the same twins; the level's own
// entities keep their order. Then each level is built and run for a while
// in a World with the twins in it, saved and loaded back.

#include "assets/art.hpp"
#include "data/enemies.hpp"
#include "data/hard_mode.hpp"
#include "data/level.hpp"
#include "data/theme.hpp"
#include "game/world.hpp"
#include "render/renderer.hpp"

#include <SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <string>
#include <vector>

using namespace gr;

namespace
{

int gFailures = 0;

void check(bool ok, const std::string& what)
{
  if (!ok || std::getenv("GR_HARD_VERBOSE"))
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
  if (!ok)
    ++gFailures;
}

bool isEnemy(const EntityDef& e) { return enemyIndex(e.kind) >= 0; }

int countEnemies(const Level& lv)
{
  return int(std::count_if(lv.entities.begin(), lv.entities.end(), isEnemy));
}

} // namespace

int main(int argc, char** argv)
{
  const std::string dir = argc > 1 ? argv[1] : "levels";
  std::vector<std::string> files;
  if (DIR* d = opendir(dir.c_str()))
  {
    while (dirent* ent = readdir(d))
    {
      const std::string name = ent->d_name;
      if (name.size() > 4 && name.compare(name.size() - 4, 4, ".txt") == 0)
        files.push_back(name);
    }
    closedir(d);
  }
  std::sort(files.begin(), files.end());
  check(files.size() > 50, "found the level files");

  SDL_Init(0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  int totalBefore = 0, totalAdded = 0, campaignLevels = 0;
  {
    Renderer renderer(sdl);
    const Theme& theme = themeByIndex(0);
    const Art art = Art::build(theme, renderer);

    for (const auto& name : files)
    {
      const Level normal = Level::loadFile(dir + "/" + name);
      Level hard = normal;
      const int added = addHardModeEnemies(hard);
      const bool bonus = !normal.rules.empty() || normal.timer > 0;
      check(hard.hard && hard.hardExtras == added, name + ": marked hard");
      if (bonus || normal.entities.empty())
      {
        check(added == 0, name + ": a bonus level (or the training stage) gets no extra enemies");
        continue;
      }
      ++campaignLevels;
      const int before = countEnemies(normal);
      totalBefore += before;
      totalAdded += added;
      std::printf("%-34s enemies %2d  +%2d\n", name.c_str(), before, added);
      check(before == 0 || added > 0, name + ": gets extra enemies");
      check(added <= std::max(3, before), name + ": at most one extra per enemy (three in a level with fewer)");

      // The level's own entities come first, unchanged.
      bool same = hard.entities.size() == normal.entities.size() + std::size_t(added);
      for (std::size_t i = 0; same && i < normal.entities.size(); ++i)
        same = hard.entities[i].kind == normal.entities[i].kind && hard.entities[i].x == normal.entities[i].x &&
          hard.entities[i].y == normal.entities[i].y && hard.entities[i].keys == normal.entities[i].keys;
      check(same, name + ": the level's own entities keep their order");

      Level again = normal;
      addHardModeEnemies(again);
      bool deterministic = again.entities.size() == hard.entities.size();
      for (std::size_t i = normal.entities.size(); deterministic && i < hard.entities.size(); ++i)
        deterministic = again.entities[i].x == hard.entities[i].x && again.entities[i].y == hard.entities[i].y;
      check(deterministic, name + ": the same twins every time");

      for (std::size_t i = normal.entities.size(); i < hard.entities.size(); ++i)
      {
        const EntityDef& t = hard.entities[i];
        if (std::getenv("GR_HARD_LIST"))
          std::printf("extra %s %s %d %d\n", name.c_str(), t.kind.c_str(), t.x, t.y);
        const EnemyDef& d = enemyDef(enemyIndex(t.kind));
        const int bw = (d.w + 1) / 2;
        const int top = (2 * t.y + 2 - d.h) >> 1;
        bool open = t.x >= 1 && t.x + bw <= hard.width - 1 && top >= 0;
        for (int r = top; open && r <= t.y; ++r)
          for (int c = t.x; open && c < t.x + bw; ++c)
            open = hard.at(c, r) == Tile::Empty || (hard.at(c, r) == Tile::Platform && r < t.y);
        char what[160];
        std::snprintf(what, sizeof(what), "%s: twin %s at %d,%d stands in open blocks", name.c_str(), t.kind.c_str(), t.x, t.y);
        check(open, what);
        const bool farFromStart = std::abs(t.x - hard.startTx) >= 16 || std::abs(t.y - hard.startTy) >= 10;
        std::snprintf(what, sizeof(what), "%s: twin %s at %d,%d is away from the start", name.c_str(), t.kind.c_str(), t.x, t.y);
        check(farFromStart, what);
        bool clearOfCheckpoints = true;
        for (const auto& s : hard.spawns)
          if (s.kind == 'c' && std::abs(t.x - s.tx) < 10 && std::abs(t.y - s.ty) < 8)
            clearOfCheckpoints = false;
        std::snprintf(what, sizeof(what), "%s: twin %s at %d,%d is away from the checkpoints", name.c_str(), t.kind.c_str(),
          t.x, t.y);
        check(clearOfCheckpoints, what);
        check(t.keys.count("carrier") == 0 && t.keys.count("hat") == 0, name + ": twins carry no Virus and no easter egg");
      }

      // Built and run: the twins are enemies in the world, nothing falls over,
      // and a save of the hard level loads back into a hard world.
      const auto lvHard = std::make_shared<const Level>(hard);
      const auto lvNormal = std::make_shared<const Level>(normal);
      World w(lvHard, 0, theme, art);
      World n(lvNormal, 0, theme, art);
      check(w.enemies().size() == n.enemies().size() + std::size_t(added), name + ": every twin is in the world");
      for (int f = 0; f < 300; ++f)
        w.update(PlayerInput{});
      const SaveGame s = w.snapshot();
      World back(lvHard, 0, theme, art);
      check(back.restore(s), name + ": a hard save loads into the hard level");
      check(back.enemies().size() == w.enemies().size(), name + ": with all its enemies");
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf("%d campaign levels: %d enemies, HARD adds %d\n", campaignLevels, totalBefore, totalAdded);
  std::printf(gFailures ? "%d FAILED\n" : "all passed\n", gFailures);
  return gFailures ? 1 : 0;
}
