// The data files the menus look for (data/campaign.cpp): WORLD SELECT and
// the title screen call levelFile() and friends every frame, so the disk is
// asked once per path and the answer kept until forgetDataFiles(). Checks a
// scratch data directory with files that are missing, appear and disappear,
// a directory named like a level file, and the real one under GR_DATA_DIR;
// then the title screen over the scratch directory: TRAINING STAGE only
// with levels/level1.txt, an EXTRAS level deleted while its lookup was kept
// fails to load and forgets the kept lookups, and a level1.txt added while
// the game runs shows up once the title opens again.

#include "data/campaign.hpp"
#include "frontend/game.hpp"

#include <SDL.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

using namespace gr;

namespace
{

int gFailures = 0;

void check(bool ok, const std::string& what)
{
  std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
  if (!ok)
    ++gFailures;
}

void touch(const std::string& path) { std::ofstream(path) << "x\n"; }

// Nothing built yet: every lookup comes back empty, edge numbers included.
void emptyDir(const std::string& dir)
{
  check(levelFile(dir, 1).empty(), "missing: no level 1");
  check(firstSpaceLevel(dir) == 0, "missing: no space level built");
  check(bonusFile(dir, 1).empty(), "missing: no bonus level");
  check(bonusFile(dir, 43).empty(), "a level without a bonus has no bonus file");
  check(extraFile(dir, 0).empty(), "missing: no extra level");
  check(extraFile(dir, -1).empty() && extraFile(dir, kExtraLevels).empty(), "extras out of range: empty");
  check(cutsceneFile(dir, "intro").empty(), "missing: no cutscene");
}

// A file that appears or disappears while the game runs is noticed only
// after forgetDataFiles(): that is what spares the disk every frame.
void appearAndDisappear(const std::string& dir)
{
  const std::string level1 = dir + "/levels/01_rooftop_run.txt";
  touch(level1);
  check(levelFile(dir, 1).empty(), "appears: the kept answer (missing) still holds");
  forgetDataFiles();
  check(levelFile(dir, 1) == level1, "appears: found after forgetDataFiles()");
  std::filesystem::remove(level1);
  check(levelFile(dir, 1) == level1, "disappears: the kept answer (found) still holds, no disk access");
  forgetDataFiles();
  check(levelFile(dir, 1).empty(), "disappears: gone after forgetDataFiles()");

  touch(dir + "/levels/44_crash_garden.txt");
  touch(dir + "/levels/extra_motor_pool.txt");
  touch(dir + "/levels/02_bonus_free_fall.txt");
  touch(dir + "/cutscenes/intro.txt");
  forgetDataFiles();
  check(firstSpaceLevel(dir) == 44, "the first space level built is 44");
  check(extraFile(dir, 0) == dir + "/levels/extra_motor_pool.txt", "extra level found");
  check(extraFile(dir, 1).empty(), "the other extra is still missing");
  check(bonusFile(dir, 2) == dir + "/levels/02_bonus_free_fall.txt", "bonus level found");
  check(cutsceneFile(dir, "intro") == dir + "/cutscenes/intro.txt", "cutscene found");
  check(dataFileExists(dir + "/cutscenes/intro.txt"), "dataFileExists agrees with cutsceneFile");
  check(!dataFileExists(dir + "/levels/level1.txt"), "dataFileExists: a missing file");
  // Another data directory is asked about on its own.
  check(levelFile(dir + "/nowhere", 44).empty(), "another data dir: its own answer");
}

// Only regular files count: a directory named like a level file (an
// unpack or a tool gone wrong) is not a level, though fopen() opens it.
void directoryIsNoLevel(const std::string& dir)
{
  const std::string fake = dir + "/levels/03_club_laserdisc.txt";
  std::filesystem::create_directories(fake);
  std::filesystem::create_directories(dir + "/cutscenes/outro.txt");
  forgetDataFiles();
  check(levelFile(dir, 3).empty(), "a directory named like a level file is not a level");
  check(!dataFileExists(fake), "dataFileExists: a directory is not a data file");
  check(cutsceneFile(dir, "outro").empty(), "a directory named like a cutscene is not one");
  std::filesystem::remove_all(fake);
  std::filesystem::remove_all(dir + "/cutscenes/outro.txt");
  forgetDataFiles();
}

// Ticks and draws like the game's loop (taps hit what was drawn last), as
// in menu_taps_test.cpp.
void run(Game& g, int ticks, Input in = {})
{
  for (int i = 0; i < ticks; ++i)
    g.tick(in);
  g.render();
}

void tap(Game& g, float x, float y)
{
  g.tapAt(x, y);
  run(g, 2);
}

Input pressed(bool Input::*button)
{
  Input in;
  in.*button = true;
  return in;
}

// The title screen's items for a fresh profile, 64 apart from NEW GAME:
// NEW GAME, WORLD SELECT, LOAD GAME, ARSENAL, then TRAINING STAGE if
// levels/level1.txt is there, else SETTINGS.
float titleItem(int i)
{
  return 252.0f + 64.0f * float(i);
}

// Game::titleItems() over a data directory without levels/level1.txt: the
// fifth item is SETTINGS, with CONTROLS on its second row (a second tap on
// a controls cell listens for a key).
void titleWithoutTraining(Game& g)
{
  run(g, 30);
  tap(g, 640.0f, titleItem(4));
  tap(g, 640.0f, 232.0f + 64.0f + 15.0f); // SETTINGS > CONTROLS
  tap(g, 770.0f, 205.0f);
  tap(g, 770.0f, 205.0f);
  check(g.listening(), "title: no level1.txt, no TRAINING STAGE (the fifth item is SETTINGS, then CONTROLS)");
  tap(g, 300.0f, 650.0f); // stop listening
  run(g, 2, pressed(&Input::back)); // to SETTINGS
  run(g, 2);
  run(g, 2, pressed(&Input::back)); // to the title
  run(g, 30);
  check(!g.listening() && !g.inLevel(), "title: back from CONTROLS");
}

// WORLD SELECT's grid, as worlds.cpp lays it out: cards 280x236, 20 apart,
// four to a row, centred on the screen's width from y 126. The kEpisodes
// episode cards (DEEP SPACE the last) come first, then EXTRAS: card 7, the
// fourth of the second row. (790, 500), the third of that row, is DEEP SPACE,
// which menu_taps_test.cpp taps. Taps hit a card's centre.
constexpr float kCardW = 280.0f, kCardH = 236.0f, kCardGap = 20.0f, kGridY = 126.0f;
constexpr int kColumns = 4;
constexpr float kGridX = (float(kScreenW) - kColumns * kCardW - (kColumns - 1) * kCardGap) / 2.0f;
constexpr int kExtrasCard = kEpisodes;
constexpr float kExtrasCardX = kGridX + float(kExtrasCard % kColumns) * (kCardW + kCardGap) + kCardW / 2.0f;
constexpr float kExtrasCardY = kGridY + float(kExtrasCard / kColumns) * (kCardH + kCardGap) + kCardH / 2.0f;
// The list screen's first row (renderList() in flow.cpp): drawn at y 120,
// its tap target 68 high from 6 above that; tapped at its middle.
constexpr float kFirstListRowY = 120.0f - 6.0f + 68.0f / 2.0f;

// The negative case: an extra level deleted after its lookup was kept is
// still offered, and picking it fails (Game::readLevelFile's catch), which
// forgets every kept lookup at once. extraFile() is asked again without the
// title reopening, so an empty answer proves the failure path forgot them.
// Starts and ends on the title.
void deletedExtraForgets(Game& g, const std::string& dir)
{
  const std::string extra = dir + "/levels/extra_motor_pool.txt";
  std::filesystem::copy_file(std::string(GR_DATA_DIR) + "/levels/extra_motor_pool.txt", extra,
    std::filesystem::copy_options::overwrite_existing);
  tap(g, 640.0f, titleItem(1));
  run(g, 30);
  check(g.worldSelect(), "extras: WORLD SELECT opens");
  tap(g, kExtrasCardX, kExtrasCardY);
  run(g, 30);
  check(!g.worldSelect() && !g.inLevel(), "extras: the EXTRAS list opens");
  check(extraFile(dir, 0) == extra, "extras: MOTOR POOL's lookup is kept");
  std::filesystem::remove(extra);
  check(extraFile(dir, 0) == extra, "extras: deleted, the kept lookup still says found");
  tap(g, 640.0f, kFirstListRowY);
  run(g, 5);
  check(!g.inLevel(), "extras: the deleted level does not start");
  check(extraFile(dir, 0).empty(), "extras: the failed load forgot the kept lookups (no title in between)");
  run(g, 2, pressed(&Input::back));
  run(g, 30);
  check(g.worldSelect(), "extras: back to WORLD SELECT");
  run(g, 2, pressed(&Input::back));
  run(g, 30);
  check(!g.worldSelect() && !g.inLevel(), "extras: back to the title");
}

// A level1.txt copied in while the title is open is not asked about again
// until the title opens anew (there and back from WORLD SELECT); then the
// fifth item is TRAINING STAGE: runner select, and a tap on the card plays.
void trainingAppears(Game& g, const std::string& dir)
{
  const std::string level1 = dir + "/levels/level1.txt";
  std::filesystem::copy_file(std::string(GR_DATA_DIR) + "/levels/level1.txt", level1);
  check(!dataFileExists(level1), "title: a level1.txt copied in is not seen while the title stays open");
  tap(g, 640.0f, titleItem(1));
  run(g, 30);
  check(g.worldSelect(), "title: WORLD SELECT opens");
  run(g, 2, pressed(&Input::back));
  run(g, 30);
  check(!g.worldSelect(), "title: back from WORLD SELECT (the title opens anew)");
  check(dataFileExists(level1), "title: reopened, it asked the disk again");
  tap(g, 640.0f, titleItem(4));
  run(g, 30);
  tap(g, 640.0f, 400.0f);
  run(g, 5);
  check(g.inLevel(), "title: TRAINING STAGE offered and played from the copied level1.txt");
}

// Game::titleItems() offers TRAINING STAGE by dataFileExists(), through a
// whole headless Game (software renderer, no audio) over the scratch dir.
void titleScreen(const std::string& dir)
{
  SDL_Init(0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  const std::string saves = dir + "/saves";
  {
    Renderer renderer(sdl);
    GameOptions o;
    o.saveDir = saves;
    o.dataDir = dir;
    o.touch = true;
    o.noCutscenes = true;
    Game g(o, renderer, nullptr);
    titleWithoutTraining(g);
    deletedExtraForgets(g, dir);
    trainingAppears(g, dir);
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
}

} // namespace

int main()
{
  // The pid keeps two build trees running ctest at once apart.
  const std::string dir = (std::filesystem::temp_directory_path() /
    ("gunrunners_campaign_files_test_" + std::to_string(::getpid()))).string();
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir + "/levels");
  std::filesystem::create_directories(dir + "/cutscenes");
  forgetDataFiles();

  emptyDir(dir);
  appearAndDisappear(dir);
  directoryIsNoLevel(dir);

  // The shipped data: level 1 and the training stage are there.
  const std::string data = GR_DATA_DIR;
  check(levelFile(data, 1) == data + "/levels/01_rooftop_run.txt", "shipped: level 1");
  check(levelFile(data, 1) == levelFile(data, 1), "shipped: asking twice gives the same answer");
  check(dataFileExists(data + "/levels/level1.txt"), "shipped: the training stage");

  titleScreen(dir);

  std::filesystem::remove_all(dir);
  std::printf("%s\n", gFailures == 0 ? "all ok" : "FAILED");
  return gFailures == 0 ? 0 : 1;
}
