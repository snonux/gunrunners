// profile.txt, runners.txt and the savegames are replaced all or nothing:
// a write that fails (disk full, file size limit, the temp file or the
// rename refused) must keep the old file, leave no temp file behind and
// report false. Before the fix a failed flush went unnoticed and an empty
// temp file was renamed over profile.txt, wiping all campaign progress.

#include "base/atomic_file.hpp"
#include "data/characters.hpp"
#include "game/profile.hpp"
#include "game/savegame.hpp"

#include <csignal>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/resource.h>
#include <unistd.h>

using namespace gr;
namespace fs = std::filesystem;

namespace
{

int gFailures = 0;

void check(bool ok, const std::string& what)
{
  std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
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

void put(const std::string& path, const std::string& data)
{
  std::ofstream(path, std::ios::trunc) << data;
}

// While alive, no file of this process may grow past 0 bytes: every write
// fails with EFBIG (SIGXFSZ is ignored so it does not kill the test), the
// way `ulimit -f 0` makes the game's saves fail. Nothing may print in its
// scope, in case stdout is a file.
class NoFileGrowth
{
public:
  NoFileGrowth()
  {
    getrlimit(RLIMIT_FSIZE, &mOld);
    mOldHandler = std::signal(SIGXFSZ, SIG_IGN);
    rlimit none = mOld;
    none.rlim_cur = 0;
    mActive = setrlimit(RLIMIT_FSIZE, &none) == 0;
  }
  ~NoFileGrowth()
  {
    setrlimit(RLIMIT_FSIZE, &mOld);
    std::signal(SIGXFSZ, mOldHandler);
  }
  bool active() const { return mActive; }

private:
  rlimit mOld{};
  void (*mOldHandler)(int) = SIG_DFL;
  bool mActive = false;
};

void writesAndReplaces(const std::string& dir)
{
  const std::string path = dir + "/plain.txt";
  std::string error;
  check(writeFileAtomic(path, "first\n", &error) && slurp(path) == "first\n", "writes a new file");
  check(writeFileAtomic(path, "second\n", &error) && slurp(path) == "second\n", "replaces an existing file");
  check(!fs::exists(path + ".tmp"), "no temp file left after a good write");
}

void failedWriteKeepsOld(const std::string& dir)
{
  const std::string path = dir + "/full.txt";
  put(path, "old\n");
  std::string error;
  bool ok = true;
  bool limited = false;
  {
    NoFileGrowth guard;
    limited = guard.active();
    ok = writeFileAtomic(path, "new data that cannot be written\n", &error);
  }
  check(limited, "file size limit set");
  check(!ok, "a failed write reports false");
  check(slurp(path) == "old\n", "a failed write keeps the old file");
  check(!fs::exists(path + ".tmp"), "a failed write removes the temp file");
  check(error.find("cannot write") == 0, "a failed write says why: " + error);
}

// /dev/full takes the open but fails every write with ENOSPC: a full disk.
void diskFullKeepsOld(const std::string& dir)
{
  if (!fs::exists("/dev/full"))
  {
    std::printf("skip no /dev/full\n");
    return;
  }
  const std::string path = dir + "/devfull.txt";
  put(path, "old\n");
  fs::create_symlink("/dev/full", path + ".tmp");
  check(!writeFileAtomic(path, "new\n"), "a full disk reports false");
  check(slurp(path) == "old\n", "a full disk keeps the old file");
  check(!fs::exists(fs::symlink_status(path + ".tmp")), "a full disk leaves no temp file");
}

void unopenableTempKeepsOld(const std::string& dir)
{
  const std::string path = dir + "/blocked.txt";
  put(path, "old\n");
  fs::create_directory(path + ".tmp");
  std::string error;
  check(!writeFileAtomic(path, "new\n", &error), "an unopenable temp file reports false");
  check(slurp(path) == "old\n", "an unopenable temp file keeps the old file");
  check(fs::is_directory(path + ".tmp"), "...and leaves what is in the way alone");
  check(!error.empty(), "...and says why");
}

void refusedRenameCleansUp(const std::string& dir)
{
  // A non-empty directory where the file should go: the rename fails.
  const std::string path = dir + "/occupied";
  fs::create_directories(path + "/inside");
  std::string error;
  check(!writeFileAtomic(path, "new\n", &error), "a refused rename reports false");
  check(fs::is_directory(path + "/inside"), "a refused rename keeps what was there");
  check(!fs::exists(path + ".tmp"), "a refused rename removes the temp file");
  check(!error.empty(), "a refused rename says why: " + error);
  check(!writeFileAtomic(dir + "/missing/file.txt", "new\n"), "a missing directory reports false");
}

Profile progress()
{
  Profile p;
  p.reached = 30;
  p.protos = {"railgun", "boomerang"};
  p.stars = {3, 7};
  p.scores[5] = 12345;
  return p;
}

// The bug as the game hit it: profile.txt with progress, then a save that
// cannot write.
void profileSurvivesFailedSave(const std::string& dir)
{
  const Profile p = progress();
  check(p.save(dir), "profile saved");
  const std::string before = slurp(dir + "/profile.txt");
  check(Profile::load(dir).reached == 30, "profile read back");

  Profile changed = p;
  changed.cutscenes.insert("briefing1");
  bool ok = true;
  {
    NoFileGrowth guard;
    ok = changed.save(dir);
  }
  check(!ok, "a failed profile save reports false");
  check(slurp(dir + "/profile.txt") == before, "a failed profile save keeps profile.txt");
  const Profile q = Profile::load(dir);
  check(q.reached == 30 && q.protos == p.protos && q.stars == p.stars && q.scores == p.scores,
    "the progress survives a failed save");
  check(!fs::exists(dir + "/profile.txt.tmp"), "a failed profile save leaves no temp file");
}

void runnersSurviveFailedSave(const std::string& dir)
{
  RunnerParts parts;
  putCustomRunner(makeCustomRunner("c0atom01", "Keeper", parts, Weapon::Laser, 3, 3, 3));
  check(saveCustomRunners(dir), "runners.txt saved");
  const std::string before = slurp(dir + "/runners.txt");
  check(before.find("c0atom01") != std::string::npos, "runners.txt holds the runner");

  clearCustomRunners();
  bool ok = true;
  {
    NoFileGrowth guard;
    ok = saveCustomRunners(dir);
  }
  check(!ok, "a failed runners save reports false");
  check(slurp(dir + "/runners.txt") == before, "a failed runners save keeps runners.txt");
  check(!fs::exists(dir + "/runners.txt.tmp"), "a failed runners save leaves no temp file");
}

void savegameSurvivesFailedSave(const std::string& dir)
{
  const std::string path = dir + "/slot1.sav";
  SaveGame s;
  s.levelName = "KEEPER";
  check(writeSave(s, path), "savegame written");
  const std::string before = slurp(path);
  s.levelName = "LOST";
  std::string error;
  bool ok = true;
  {
    NoFileGrowth guard;
    ok = writeSave(s, path, &error);
  }
  check(!ok && !error.empty(), "a failed savegame write reports false and why");
  check(slurp(path) == before, "a failed savegame write keeps the old save");
  check(!fs::exists(path + ".tmp"), "a failed savegame write leaves no temp file");
}

} // namespace

int main()
{
  const fs::path dir = fs::temp_directory_path() / ("gunrunners_atomic_file_test_" + std::to_string(getpid()));
  fs::remove_all(dir);
  fs::create_directories(dir);
  const std::string d = dir.string();
  writesAndReplaces(d);
  failedWriteKeepsOld(d);
  diskFullKeepsOld(d);
  unopenableTempKeepsOld(d);
  refusedRenameCleansUp(d);
  profileSurvivesFailedSave(d + "/profile");
  runnersSurviveFailedSave(d + "/runners");
  savegameSurvivesFailedSave(d + "/saves");
  fs::remove_all(dir);
  std::printf("%s\n", gFailures ? "FAILED" : "all ok");
  return gFailures ? 1 : 0;
}
