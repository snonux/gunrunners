// profile.txt, runners.txt and the savegames are replaced all or nothing:
// a write that fails (file size limit, the temp file or the rename refused)
// must keep the old file, leave no temp file behind and report false.
// Before the fix a failed flush went unnoticed and an empty temp file was
// renamed over profile.txt, wiping all campaign progress. Each write uses a
// temp name of its own; whatever already sits at such a name (a leftover, a
// symlink) is skipped, never written through or removed.

#include "base/atomic_file.hpp"
#include "data/characters.hpp"
#include "game/profile.hpp"
#include "game/savegame.hpp"

#include <csignal>
#include <exception>
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

// How many files next to path have a temp name of it: "<name>.tmp", or
// "<name>.tmp.<pid>.<n>" as writeFileAtomic makes them.
int tempsOf(const std::string& path)
{
  const fs::path p(path);
  const std::string prefix = p.filename().string() + ".tmp";
  int n = 0;
  for (const auto& entry : fs::directory_iterator(p.parent_path()))
    n += entry.path().filename().string().rfind(prefix, 0) == 0 ? 1 : 0;
  return n;
}

void writesAndReplaces(const std::string& dir)
{
  const std::string path = dir + "/plain.txt";
  std::string error;
  check(writeFileAtomic(path, "first\n", &error) && slurp(path) == "first\n", "writes a new file");
  check(writeFileAtomic(path, "second\n", &error) && slurp(path) == "second\n", "replaces an existing file");
  check(tempsOf(path) == 0, "no temp file left after a good write");
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
  check(tempsOf(path) == 0, "a failed write removes the temp file");
  check(error.find("cannot write") == 0, "a failed write says why: " + error);
}

// A stale "<name>.tmp" (the fixed temp name of older builds, or any file of
// someone else) is not a temp name writeFileAtomic uses: it stays exactly as
// it was, and the write neither trips over it nor leaves a temp of its own.
void staleTempLeftAlone(const std::string& dir)
{
  const std::string path = dir + "/stale.txt";
  const std::string stale = "a leftover from an older build's crash\n";
  put(path + ".tmp", stale);
  check(writeFileAtomic(path, "new\n") && slurp(path) == "new\n", "a stale .tmp file: the write works");
  check(slurp(path + ".tmp") == stale, "...and the stale file is left untouched");
  check(tempsOf(path) == 1, "...and no temp file of the write is left");
}

// The counter value the next writeFileAtomic of this process takes. A write
// into a missing directory fails on its first temp name and names it,
// "cannot write <dir>/missing/probe.tmp.<pid>.<n>: ...", so n + 1 is next.
unsigned long long nextTempCounter(const std::string& dir)
{
  std::string error;
  writeFileAtomic(dir + "/missing/probe", "", &error);
  const std::string mark = ".tmp." + std::to_string(getpid()) + ".";
  const std::size_t at = error.find(mark);
  check(at != std::string::npos, "a failed write names its temp file: " + error);
  return at == std::string::npos ? 0 : std::stoull(error.substr(at + mark.size())) + 1;
}

// Plants symlinks to target at the next 4 temp names a write of path takes.
void plantSymlinks(const std::string& dir, const std::string& path, const std::string& target)
{
  const unsigned long long next = nextTempCounter(dir);
  const std::string prefix = path + ".tmp." + std::to_string(getpid()) + ".";
  for (unsigned long long n = next; n < next + 4; ++n)
    fs::create_symlink(target, prefix + std::to_string(n));
}

// Symlinks at the very temp names the write takes must not redirect it onto
// their targets: O_EXCL skips them, they stay, and the targets keep their
// content.
void symlinkAtTempNotFollowed(const std::string& dir)
{
  const std::string path = dir + "/linked.txt";
  const std::string victim = dir + "/victim.txt";
  put(victim, "victim\n");
  plantSymlinks(dir, path, victim);
  check(writeFileAtomic(path, "new\n") && slurp(path) == "new\n", "symlinks at the temp names: the write works");
  check(slurp(victim) == "victim\n", "symlinks at the temp names do not clobber their target");
  check(tempsOf(path) == 4, "...and are left alone, with no temp file of the write left");
  check(!fs::is_symlink(path), "...and do not become the file");

  const std::string dangling = dir + "/dangling.txt";
  plantSymlinks(dir, dangling, dir + "/nowhere.txt");
  check(writeFileAtomic(dangling, "new\n"), "dangling symlinks at the temp names: the write works");
  check(!fs::exists(fs::symlink_status(dir + "/nowhere.txt")), "...and creates nothing at their target");
}

// A temp file that cannot be created: the name with ".tmp.<pid>.<n>" added
// is longer than a file name may be (255 bytes on the usual file systems),
// though the file name of path itself (254 bytes) is not.
void uncreatableTempKeepsOld(const std::string& dir)
{
  const std::string path = dir + "/" + std::string(250, 'n') + ".txt";
  put(path, "old\n");
  std::string error;
  check(slurp(path) == "old\n", "a long file name can be written directly");
  check(!writeFileAtomic(path, "new\n", &error), "an uncreatable temp file reports false");
  check(slurp(path) == "old\n", "an uncreatable temp file keeps the old file");
  check(error.find("cannot write") == 0, "...and says why: " + error);
  check(!writeFileAtomic(dir + "/missing/file.txt", "new\n"), "a missing directory reports false");
}

void refusedRenameCleansUp(const std::string& dir)
{
  // A non-empty directory where the file should go: the rename fails.
  const std::string path = dir + "/occupied";
  fs::create_directories(path + "/inside");
  std::string error;
  check(!writeFileAtomic(path, "new\n", &error), "a refused rename reports false");
  check(fs::is_directory(path + "/inside"), "a refused rename keeps what was there");
  check(tempsOf(path) == 0, "a refused rename removes the temp file");
  check(error.find("cannot replace") == 0, "a refused rename says why: " + error);
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
  check(tempsOf(dir + "/profile.txt") == 0, "a failed profile save leaves no temp file");
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
  check(tempsOf(dir + "/runners.txt") == 0, "a failed runners save leaves no temp file");
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
  check(tempsOf(path) == 0, "a failed savegame write leaves no temp file");

  // A non-empty directory where the savegame should go: the rename fails.
  const std::string occupied = dir + "/slot2.sav";
  fs::create_directories(occupied + "/inside");
  error.clear();
  check(!writeSave(s, occupied, &error) && !error.empty(), "a refused savegame rename reports false and why");
  check(fs::is_directory(occupied + "/inside"), "a refused savegame rename keeps what was there");
  check(tempsOf(occupied) == 0, "a refused savegame rename leaves no temp file");
}

// Removes the test's directory however main is left, a throwing check
// included (main catches, so the stack unwinds and this runs).
class TempDir
{
public:
  explicit TempDir(fs::path path) : mPath(std::move(path))
  {
    fs::remove_all(mPath);
    fs::create_directories(mPath);
  }
  ~TempDir()
  {
    std::error_code ec;
    fs::remove_all(mPath, ec);
  }
  std::string str() const { return mPath.string(); }

private:
  fs::path mPath;
};

void runAll(const std::string& d)
{
  writesAndReplaces(d);
  failedWriteKeepsOld(d);
  staleTempLeftAlone(d);
  symlinkAtTempNotFollowed(d);
  uncreatableTempKeepsOld(d);
  refusedRenameCleansUp(d);
  profileSurvivesFailedSave(d + "/profile");
  runnersSurviveFailedSave(d + "/runners");
  savegameSurvivesFailedSave(d + "/saves");
}

} // namespace

int main()
{
  {
    const TempDir dir(fs::temp_directory_path() / ("gunrunners_atomic_file_test_" + std::to_string(getpid())));
    try
    {
      runAll(dir.str());
    }
    catch (const std::exception& e)
    {
      check(false, std::string("no exception: ") + e.what());
    }
  }
  std::printf("%s\n", gFailures ? "FAILED" : "all ok");
  return gFailures ? 1 : 0;
}
