// Plays part of the level with the bot, saves, loads into a fresh world and
// checks that the reloaded world saves back to exactly the same file. Also
// checks that a file cut short is rejected, and pins what readSave makes of
// hand-written files: which it rejects, and how it reads odd lines.

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
#include <optional>
#include <sstream>
#include <string>
#include <unistd.h>
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


// Writes text as a save file and reads it back.
std::optional<SaveGame> parse(const std::string& dir, const std::string& text)
{
  const std::string path = dir + "/parse.sav";
  {
    std::ofstream f(path);
    f << text;
  }
  return readSave(path);
}

// The smallest save readSave takes: header, a live player, "end".
const std::string kHeader = "gunrunners-save 1\n";
const std::string kPlayer = "player 1 2 1 5 0 0 0 0\n";

// Whole files readSave turns down.
void checkRejected(const std::string& dir)
{
  check(bool(parse(dir, kHeader + "level Roof\n" + kPlayer + "end\n")), "minimal save is read");
  check(!parse(dir, kHeader + kPlayer), "save without end is rejected");
  check(!parse(dir, "gunrunners-sav 1\n" + kPlayer + "end\n"), "wrong magic is rejected");
  check(!parse(dir, "gunrunners-save 2\n" + kPlayer + "end\n"), "other version is rejected");
  check(!parse(dir, "gunrunners-save x\n" + kPlayer + "end\n"), "unreadable version is rejected");
  check(!parse(dir, ""), "empty file is rejected");
  check(!parse(dir, kHeader + "player 1 2 1 0 0 0 0 0\nend\n"), "dead player is rejected");
  check(!parse(dir, kHeader + "end\n"), "save without player is rejected");
  check(!parse(dir, kHeader + "player 1 2 1 x 0 0 0 0\nend\n"), "unreadable player hp is rejected");
}

// Lines readSave skips or reads leniently; the save itself still loads.
void checkLenientLines(const std::string& dir)
{
  const auto s = parse(dir, kHeader + "level Roof Top Run\nwobble 1 2 3\n\n" + kPlayer +
    "campaign 7 levels/07 chopper.txt\nletters -\nenemy 1 5 x\nenemy 1 5 3 4 -1 0 1\nenemy 0 2 3 4 1 0 0 6 2 9\n"
    "platform 1 2 3\nitem 1 2 3\nitem 1 2 3 4 1\nbreaker 1 50 0 -1\nprop x\nprop 1\nbox 0\nend\ndoor 4\n");
  check(bool(s), "save with odd lines is read");
  if (!s)
    return;
  check(s->levelName == "Roof Top Run" && s->levelNumber == 7 && s->levelFile == "levels/07 chopper.txt",
    "names keep their spaces");
  check(s->letters.empty() && s->hp == 5 && s->hasKey == false, "letters '-' read as none");
  check(s->enemies.size() == 2 && s->enemies[0].def == -1 && s->enemies[0].platform == -1 &&
      s->enemies[1].def == 6 && s->enemies[1].platform == 2 && s->enemies[1].attach == 9 && !s->enemies[1].alive,
    "short enemy line skipped, old and new enemy lines read");
  check(s->platforms.empty(), "short platform line skipped");
  check(s->items.size() == 1 && s->items[0].floating, "short item line skipped");
  check(s->breakers.size() == 1 && s->breakers[0].on && s->breakers[0].thrownAt == 50 && !s->breakers[0].leechKilled &&
      s->breakers[0].leechIn == -1,
    "breaker read in written order");
  check(s->props.size() == 1 && s->props[0] && s->boxes.size() == 1 && !s->boxes[0], "unreadable prop skipped");
  check(s->doors.size() == 1 && s->doors[0] == 4, "lines after end are still read");
}

// Int lists: "key count values...".
void checkIntLists(const std::string& dir)
{
  const auto s = parse(dir, kHeader + kPlayer +
    "floods 3 7 8\nvalves x 1 2\ntrain -2 4\nchopper 2 1 2 3\njungle 1 4\njungle 1 5\n"
    "ratpipes 1 9\nbubbled 1 3\nbench 0 1\nend\n");
  check(bool(s), "save with odd int lists is read");
  if (!s)
    return;
  check(s->floods == std::vector<int>{7, 8}, "short int list keeps its values");
  check(s->valves.empty() && s->train.empty(), "bad or negative count reads no values");
  check(s->chopper == std::vector<int>{1, 2}, "values past the count are ignored");
  check(s->jungle == std::vector<int>{4, 5}, "a repeated int list adds to the list");
  check(s->ratPipes == std::vector<int>{9} && s->bubbled == std::vector<int>{3} && s->bench.empty(),
    "int lists land in their own members");
}

// Every int list survives writeSave and readSave, each in its own member,
// and the read save writes back byte for byte.
void checkAllIntLists(const std::string& dir)
{
  using List = std::vector<int> SaveGame::*;
  const List lists[] = {&SaveGame::floods, &SaveGame::valves, &SaveGame::ratPipes, &SaveGame::bubbled,
    &SaveGame::train, &SaveGame::chopper, &SaveGame::jungle, &SaveGame::temple, &SaveGame::light, &SaveGame::mine,
    &SaveGame::boulder, &SaveGame::sanctum, &SaveGame::station, &SaveGame::green, &SaveGame::hull,
    &SaveGame::grav, &SaveGame::reactor, &SaveGame::zero, &SaveGame::west, &SaveGame::manor, &SaveGame::kitchen,
    &SaveGame::garden, &SaveGame::city, &SaveGame::noir, &SaveGame::toon, &SaveGame::signal, &SaveGame::trench,
    &SaveGame::storm, &SaveGame::dish, &SaveGame::magma, &SaveGame::fleet, &SaveGame::tower, &SaveGame::warehouse,
    &SaveGame::wardrobe, &SaveGame::writers, &SaveGame::gallery, &SaveGame::flytower, &SaveGame::sweeps,
    &SaveGame::finale, &SaveGame::explored, &SaveGame::vehicles, &SaveGame::swap, &SaveGame::guns, &SaveGame::bench};
  SaveGame s;
  s.hp = 3;
  s.savedAt = "2026-10-16 12:00";
  int k = 0;
  for (List l : lists)
  {
    ++k;
    s.*l = {k, -k, k * 100};
  }
  const std::string a = dir + "/lists.sav", b = dir + "/lists2.sav";
  check(writeSave(s, a), "write every int list");
  const auto r = readSave(a);
  bool same = bool(r);
  for (List l : lists)
    same = same && (*r).*l == s.*l;
  check(same, "every int list reads back into its own member");
  check(r && writeSave(*r, b) && slurp(a) == slurp(b), "every int list writes back byte for byte");
}

} // namespace

int main(int argc, char** argv)
{
  const std::string levelPath = argc > 1 ? argv[1] : "levels/level1.txt";
  const int playFrames = argc > 2 ? std::atoi(argv[2]) : 200;
  // One directory per run: ctest -j runs the savegame tests side by side.
  const std::string dir = (std::filesystem::temp_directory_path() /
    ("gunrunners_savegame_test_" + std::to_string(::getpid()))).string();
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  checkRejected(dir);
  checkLenientLines(dir);
  checkIntLists(dir);
  checkAllIntLists(dir);

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
