// The data files the menus look for (data/campaign.cpp): WORLD SELECT and
// the title screen call levelFile() and friends every frame, so the disk is
// asked once per path and the answer kept until forgetDataFiles(). Checks a
// scratch data directory with files that are missing, appear and disappear,
// and the real one under GR_DATA_DIR.

#include "data/campaign.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

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

} // namespace

int main()
{
  const std::string dir = (std::filesystem::temp_directory_path() / "gunrunners_campaign_files_test").string();
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir + "/levels");
  std::filesystem::create_directories(dir + "/cutscenes");
  forgetDataFiles();

  emptyDir(dir);
  appearAndDisappear(dir);

  // The shipped data: level 1 and the training stage are there.
  const std::string data = GR_DATA_DIR;
  check(levelFile(data, 1) == data + "/levels/01_rooftop_run.txt", "shipped: level 1");
  check(levelFile(data, 1) == levelFile(data, 1), "shipped: asking twice gives the same answer");
  check(dataFileExists(data + "/levels/level1.txt"), "shipped: the training stage");

  std::filesystem::remove_all(dir);
  std::printf("%s\n", gFailures == 0 ? "all ok" : "FAILED");
  return gFailures == 0 ? 0 : 1;
}
