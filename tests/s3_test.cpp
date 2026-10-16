#include "game/s3.hpp"
#include "game/savegame.hpp"
#include "stall_server.hpp"

#include <arpa/inet.h>
#include <csignal>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <netinet/in.h>
#include <string>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

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

std::string env(const char* name)
{
  const char* value = std::getenv(name);
  return value ? value : "";
}

std::string read(const std::string& path)
{
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

// A one-request HTTP server on 127.0.0.1 that answers any request with
// 200 and body: enough for s3GetFile, which only signs the request.
class OneShotServer
{
public:
  explicit OneShotServer(std::string body) : mBody(std::move(body))
  {
    mListen = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t len = sizeof(addr);
    if (mListen < 0 || ::bind(mListen, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
        ::listen(mListen, 1) != 0 || ::getsockname(mListen, reinterpret_cast<sockaddr*>(&addr), &len) != 0)
      return;
    mPort = ntohs(addr.sin_port);
    mThread = std::thread([this] { serve(); });
  }
  ~OneShotServer()
  {
    if (mThread.joinable())
    {
      ::shutdown(mListen, SHUT_RDWR);
      mThread.join();
    }
    if (mListen >= 0)
      ::close(mListen);
  }
  int port() const { return mPort; }

private:
  void serve()
  {
    const int fd = ::accept(mListen, nullptr, nullptr);
    if (fd < 0)
      return;
    std::string request;
    char buf[1024];
    ssize_t n;
    while (request.find("\r\n\r\n") == std::string::npos && (n = ::read(fd, buf, sizeof(buf))) > 0)
      request.append(buf, std::size_t(n));
    const std::string reply = "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(mBody.size()) +
      "\r\nConnection: close\r\n\r\n" + mBody;
    for (std::size_t sent = 0; sent < reply.size() && (n = ::write(fd, reply.data() + sent, reply.size() - sent)) > 0;)
      sent += std::size_t(n);
    ::close(fd);
  }

  std::string mBody;
  int mListen = -1;
  int mPort = 0;
  std::thread mThread;
};

S3Config localConfig(int port)
{
  S3Config config;
  config.endpoint = "http://127.0.0.1:" + std::to_string(port);
  config.bucket = "saves";
  config.region = "local";
  config.accessKey = "key";
  config.secretKey = "secret";
  return config;
}

// While alive, no file of this process may grow past limit bytes (0 by
// default): a write beyond fails with EFBIG (SIGXFSZ is ignored so it does
// not kill the test), as in tests/atomic_file_test.cpp. Sockets are not
// files, so the local server and curl still talk. Nothing may print in its
// scope, in case stdout is a file.
class NoFileGrowth
{
public:
  explicit NoFileGrowth(rlim_t limit = 0)
  {
    getrlimit(RLIMIT_FSIZE, &mOld);
    mOldHandler = std::signal(SIGXFSZ, SIG_IGN);
    rlimit none = mOld;
    none.rlim_cur = limit;
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

namespace fs = std::filesystem;

#ifndef GR_NO_CURL

const std::string kBody = "gunrunners-save 1\nlevel LOCAL\nend\n";

// Files in dir other than the ones a test put there or downloaded: a temp
// file a failed download left behind would show up here.
int strayFiles(const fs::path& dir)
{
  int stray = 0;
  for (const auto& entry : fs::directory_iterator(dir))
  {
    const std::string name = entry.path().filename().string();
    stray += name == "good.sav" || name == "blocked.sav" || name == "full.sav" ? 0 : 1;
  }
  return stray;
}

void downloadLandsInFull(const fs::path& dir)
{
  const std::string good = (dir / "good.sav").string();
  std::string error;
  OneShotServer server(kBody);
  check(server.port() > 0, "the local server for a good download listens");
  check(s3GetFile(localConfig(server.port()), "good.sav", good, &error) == S3GetResult::Downloaded &&
      read(good) == kBody,
    "a download lands in full");
}

// A directory in the way: the body is written, the rename is refused.
void refusedRenameIsError(const fs::path& dir)
{
  const std::string blocked = (dir / "blocked.sav").string();
  fs::create_directories(blocked + "/inside");
  std::string error;
  {
    OneShotServer server(kBody);
    check(server.port() > 0, "the local server for a blocked download listens");
    check(s3GetFile(localConfig(server.port()), "blocked.sav", blocked, &error) == S3GetResult::Error &&
        error.find("cannot replace") == 0,
      "a download that cannot be installed is a \"cannot replace\" error");
  }
  check(fs::is_directory(blocked + "/inside"), "a refused download keeps what was there");
}

// The old leak: a download whose body cannot be written (file size limit,
// like a full disk) left its temp file behind. It must be an error, keep
// the old file and leave no temp file.
void failedWriteIsError(const fs::path& dir)
{
  const std::string full = (dir / "full.sav").string();
  std::ofstream(full) << "old\n";
  std::string error;
  S3GetResult result = S3GetResult::Downloaded;
  bool limited = false;
  OneShotServer server(kBody);
  check(server.port() > 0, "the local server for an unwritable download listens");
  {
    NoFileGrowth guard;
    limited = guard.active();
    result = s3GetFile(localConfig(server.port()), "full.sav", full, &error);
  }
  check(limited, "file size limit set");
  check(result == S3GetResult::Error && error.find("cannot write") == 0,
    "a download that cannot be written is a \"cannot write\" error");
  check(read(full) == "old\n", "a download that cannot be written keeps the old file");
}
#endif

// A download goes through the same all-or-nothing write as the saves: the
// body lands in full, and a download that cannot be written or installed
// reports an error and leaves no temp file behind.
void downloadsInstallAllOrNothing()
{
#ifndef GR_NO_CURL
  ::setenv("no_proxy", "127.0.0.1", 1);
  ::setenv("NO_PROXY", "127.0.0.1", 1);
  const fs::path dir = fs::temp_directory_path() / ("gunrunners_s3_get_test_" + std::to_string(::getpid()));
  fs::remove_all(dir);
  fs::create_directories(dir);
  downloadLandsInFull(dir);
  refusedRenameIsError(dir);
  failedWriteIsError(dir);
  check(strayFiles(dir) == 0, "failed downloads leave no temp file behind");
  fs::remove_all(dir);
#endif
}

// The permission bits of path, or -1 when it cannot be stat'ed.
int modeOf(const std::string& path)
{
  struct stat st;
  return ::stat(path.c_str(), &st) == 0 ? int(st.st_mode & 07777) : -1;
}

// Files in dir whose name starts with prefix: a temp file a write left
// behind ("cloud.txt.tmp.<pid>.<n>") would show up here.
int filesStartingWith(const fs::path& dir, const std::string& prefix)
{
  int n = 0;
  for (const auto& entry : fs::directory_iterator(dir))
    n += entry.path().filename().string().rfind(prefix, 0) == 0 ? 1 : 0;
  return n;
}

S3Config configWithSecret(const std::string& secret)
{
  S3Config config = localConfig(9);
  config.secretKey = secret;
  return config;
}

// cloud.txt holds the secret key, so it is 0600 whatever the umask (even
// 0, where the default 0666 would leave it readable by everyone) and
// whatever mode an older cloud.txt had.
void configIsOwnerOnly(const fs::path& dir)
{
  const std::string path = (dir / "cloud.txt").string();
  std::ofstream(path) << "old\n";
  fs::permissions(path, fs::perms(0644));
  const mode_t old = ::umask(0);
  const bool saved = saveS3Config(dir.string(), configWithSecret("private"));
  ::umask(old);
  check(saved && loadS3Config(dir.string()).secretKey == "private", "cloud.txt is saved");
  check(modeOf(path) == 0600, "cloud.txt is owner-only (0600), even with umask 0");
  check(filesStartingWith(dir, "cloud.txt.tmp") == 0, "saving cloud.txt leaves no temp file");
}

// The old leak: cloud.txt was written through the fixed name cloud.txt.tmp
// with O_CREAT|O_TRUNC, so a symlink planted there received the secret key
// (and its target was chmodded). Now the symlink is not a temp name the
// write uses: it and its target stay as they were, and with a dangling one
// nothing is created at its target. Each save also leaves no temp file of
// its own: the count takes the prefix "cloud.txt.tmp." with the dot, so it
// sees the "cloud.txt.tmp.<pid>.<n>" temp names but not the symlink itself.
void legacyTempSymlinkIgnored(const fs::path& dir)
{
  const std::string legacy = (dir / "cloud.txt.tmp").string();
  const std::string victim = (dir / "victim.txt").string();
  std::ofstream(victim) << "victim\n";
  fs::permissions(victim, fs::perms(0644));
  fs::create_symlink(victim, legacy);
  check(saveS3Config(dir.string(), configWithSecret("linked")), "a symlink at cloud.txt.tmp: the save works");
  check(read(victim) == "victim\n" && modeOf(victim) == 0644,
    "...and its target does not receive the secret, nor a new mode");
  check(fs::is_symlink(legacy) && !fs::is_symlink(dir / "cloud.txt") &&
      loadS3Config(dir.string()).secretKey == "linked",
    "...and it stays a symlink, with cloud.txt a file of its own");
  check(filesStartingWith(dir, "cloud.txt.tmp.") == 0,
    "...and the save leaves no temp file next to it");
  fs::remove(legacy);
  const fs::path nowhere = dir / "nowhere.txt";
  fs::create_symlink(nowhere, legacy);
  check(saveS3Config(dir.string(), configWithSecret("dangling")) &&
      !fs::exists(fs::symlink_status(nowhere)),
    "a dangling symlink at cloud.txt.tmp: nothing is created at its target");
  check(filesStartingWith(dir, "cloud.txt.tmp.") == 0,
    "...and the save leaves no temp file next to it");
  fs::remove(legacy);
}

// A save that cannot be written (file size limit, like a full disk) reports
// false and why, and keeps the old cloud.txt with its old secret.
void failedConfigSaveKeepsOld(const fs::path& dir)
{
  check(saveS3Config(dir.string(), configWithSecret("kept")), "a config to keep is saved");
  const std::string before = read((dir / "cloud.txt").string());
  std::string error;
  bool saved = true;
  bool limited = false;
  {
    NoFileGrowth guard;
    limited = guard.active();
    saved = saveS3Config(dir.string(), configWithSecret("lost"), &error);
  }
  check(limited, "file size limit set");
  check(!saved && error.find("cannot write") == 0, "a cloud.txt that cannot be written is an error");
  check(read((dir / "cloud.txt").string()) == before && loadS3Config(dir.string()).secretKey == "kept",
    "...and the old cloud.txt is kept");
  check(filesStartingWith(dir, "cloud.txt.tmp") == 0, "...and no temp file is left");
}

// The dirty marker goes through the same write: a symlink at its old fixed
// temp name is not followed, and the marker holds a token.
void dirtyMarkerIgnoresSymlink(const fs::path& dir)
{
  const std::string save = (dir / "slot.sav").string();
  const std::string victim = (dir / "marker-victim.txt").string();
  std::ofstream(victim) << "victim\n";
  fs::create_symlink(victim, save + ".cloud-dirty.tmp");
  std::string error;
  check(markS3Dirty(save, &error), "a symlink at .cloud-dirty.tmp: marking dirty works");
  check(read(victim) == "victim\n", "...and its target does not receive the token");
  check(!read(save + ".cloud-dirty").empty() && modeOf(save + ".cloud-dirty") == 0600,
    "...and the marker holds a token, owner-only");
  check(filesStartingWith(dir, "slot.sav.cloud-dirty.tmp.") == 0, "...and no temp file is left");
}

// cloud.txt and the dirty marker are written all or nothing, owner-only,
// never through whatever sits at a temp name.
void configWritesAreSafe()
{
  const fs::path dir = fs::temp_directory_path() / ("gunrunners_s3_cloud_txt_test_" + std::to_string(::getpid()));
  fs::remove_all(dir);
  fs::create_directories(dir);
  configIsOwnerOnly(dir);
  legacyTempSymlinkIgnored(dir);
  failedConfigSaveKeepsOld(dir);
  dirtyMarkerIgnoresSymlink(dir);
  fs::remove_all(dir);
}

#ifndef GR_NO_CURL
// The old leak: a sync whose local save is newer than the cloud's downloads
// the cloud's copy to compare, then snapshots the local save to upload it.
// When that snapshot failed (a full disk), the download stayed behind, and
// so did the part of the snapshot written. The file size limit lets the
// small download through and stops the big save's snapshot.
void failedSnapshotLeavesNoTemp(const fs::path& dir)
{
  SaveGame remote;
  remote.levelName = "Remote";
  remote.savedAt = "2026-01-01 10:00";
  remote.hp = 3;
  const std::string body = (dir / "remote.body").string();
  check(writeSave(remote, body), "the cloud's older save is written");
  const std::string cloudCopy = read(body);
  fs::remove(body);
  SaveGame local = remote;
  local.levelName = std::string(16384, 'L');
  local.savedAt = "2026-01-02 10:00";
  const std::string save = (dir / "snap.sav").string();
  check(writeSave(local, save), "a newer local save, too big to snapshot under the limit");
  const std::string before = read(save);
  OneShotServer server(cloudCopy);
  check(server.port() > 0, "the local server for a failing snapshot listens");
  std::string error;
  S3SyncResult result = S3SyncResult::Uploaded;
  bool limited = false;
  {
    NoFileGrowth guard(4096);
    limited = guard.active();
    result = syncS3File(localConfig(server.port()), "snap.sav", save, &error);
  }
  check(limited, "file size limit set");
  check(cloudCopy.size() < 4096 && before.size() > 4096, "the download fits the limit, the snapshot does not");
  check(result == S3SyncResult::Error && error.find("cannot snapshot local save") == 0,
    "a snapshot that cannot be written is a \"cannot snapshot\" error");
  check(read(save) == before, "...and the local save is untouched");
  check(filesStartingWith(dir, "snap.sav.cloud-") == 0,
    "...and neither the download nor a partial snapshot is left");
}
#endif

// A file of name in dir holding text, last written an hour ago when old
// (older than kCloudLeftoverAge), else just now.
void plant(const fs::path& dir, const std::string& name, const std::string& text, bool old = true)
{
  std::ofstream(dir / name) << text;
  if (old)
    fs::last_write_time(dir / name, fs::file_time_type::clock::now() - std::chrono::hours(1));
}

// What killed syncs and writes leave behind, one of each kind (see
// removeCloudLeftovers): all of it is removed once old enough.
const char* const kLeftovers[] = {
  "slot1.sav.cloud-upload-123",
  "slot2.sav.cloud-incoming-456",
  "slot2.sav.cloud-incoming-456.tmp.77.0",
  "quick.sav.cloud-dirty.tmp.77.1",
  "slot3.sav.cloud-dirty.tmp",
  "slot5.sav.tmp.77.2",
  "cloud.txt.tmp.123.0",
  "cloud.txt.tmp",
};

// Saves, their state, and files merely named like a save or a leftover:
// none of them is ever removed, however old.
const char* const kKeepers[] = {
  "slot1.sav",
  "quick.sav",
  "slot1.sav.cloud-dirty",
  "cloud.txt",
  "profile.txt",
  "slot6.sav",
  "slot6.sav.cloud-upload-123",
  "notes.sav.cloud-incoming-456",
  "slot1.sav.cloud-upload-",
  "slot1.sav.cloud-upload-12x",
  "slot1.sav.cloud-upload-123.tmp.77.0",
  "slot1.sav.cloud-incoming-456.tmp.77",
  "slot1.sav.tmp.77",
  "slot1.sav.bak",
  "cloud.txt.tmp.123",
  "cloud.txt.tmp.x.0",
  "cloud.txt.bak",
  "xslot1.sav.cloud-upload-123",
};

// The start's sweep: old leftovers go; the saves, cloud.txt, look-alikes, a
// directory with a leftover's name and a leftover still new (another game
// process may be syncing with it) stay, the saves with their contents.
void staleLeftoversRemoved(const fs::path& dir)
{
  for (const char* name : kLeftovers)
    plant(dir, name, "secret_key=leaked\n");
  for (const char* name : kKeepers)
    plant(dir, name, std::string("real ") + name + "\n");
  fs::create_directories(dir / "slot4.sav.cloud-upload-999");
  plant(dir, "slot4.sav.cloud-upload-777", "in use\n", false);
  check(removeCloudLeftovers(dir.string()) == int(std::size(kLeftovers)),
    "the start removes every old cloud leftover");
  bool gone = true;
  for (const char* name : kLeftovers)
    gone = gone && !fs::exists(fs::symlink_status(dir / name));
  check(gone, "...none of them is left, the secret-holding cloud.txt temps included");
  bool kept = true;
  for (const char* name : kKeepers)
    kept = kept && read((dir / name).string()) == std::string("real ") + name + "\n";
  check(kept, "...and every save, cloud.txt and look-alike stays as it was");
  check(fs::is_directory(dir / "slot4.sav.cloud-upload-999"), "...and a directory named like a leftover stays");
  check(fs::exists(dir / "slot4.sav.cloud-upload-777"), "...and a leftover still new stays");
  check(removeCloudLeftovers(dir.string(), std::chrono::seconds(0)) == 1 &&
      !fs::exists(dir / "slot4.sav.cloud-upload-777"),
    "with no minimum age, the new one goes too");
  check(removeCloudLeftovers((dir / "missing").string()) == 0, "a save dir that is not there is no error");
}

// Turning the cloud off removes cloud.txt's write temps too, however new,
// as they may hold the secret key; nothing else goes.
void removeConfigSweepsTemps(const fs::path& dir)
{
  check(saveS3Config(dir.string(), configWithSecret("removed")), "a config to remove is saved");
  plant(dir, "cloud.txt.tmp.123.0", "secret_key=leaked\n", false);
  plant(dir, "cloud.txt.tmp", "secret_key=legacy\n", false);
  plant(dir, "cloud.txt.tmp.x.0", "kept\n", false);
  plant(dir, "slot1.sav", "kept\n", false);
  std::string error;
  check(removeS3Config(dir.string(), &error), "the cloud config is removed");
  check(!fs::exists(dir / "cloud.txt") && filesStartingWith(dir, "cloud.txt.tmp.1") == 0 &&
      !fs::exists(dir / "cloud.txt.tmp"),
    "...with its write temps, the legacy cloud.txt.tmp included");
  check(read((dir / "cloud.txt.tmp.x.0").string()) == "kept\n" && read((dir / "slot1.sav").string()) == "kept\n",
    "...and nothing else");
}

// Leftovers of killed syncs and writes are removed, never anything else;
// a failed snapshot leaves nothing behind in the first place.
void leftoverTests()
{
  const fs::path dir = fs::temp_directory_path() / ("gunrunners_s3_leftover_test_" + std::to_string(::getpid()));
  fs::remove_all(dir);
  fs::create_directories(dir / "sweep");
  fs::create_directories(dir / "remove");
  staleLeftoversRemoved(dir / "sweep");
  removeConfigSweepsTemps(dir / "remove");
#ifndef GR_NO_CURL
  ::setenv("no_proxy", "127.0.0.1", 1);
  ::setenv("NO_PROXY", "127.0.0.1", 1);
  fs::create_directories(dir / "snapshot");
  failedSnapshotLeavesNoTemp(dir / "snapshot");
#endif
  fs::remove_all(dir);
}

// What quitting the game does to a running sync: the S3Active check turns
// false after ms milliseconds.
S3Active cancelAfter(int ms)
{
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
  return S3Active{[deadline]() { return std::chrono::steady_clock::now() < deadline; }, nullptr};
}

double secondsSince(std::chrono::steady_clock::time_point start)
{
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

// A request cancelled by cancelAfter(300) ended because of the cancel: not
// before it (so it really waited on the server, not failed at once) but
// soon after it (libcurl's progress callback runs about once a second), and
// with libcurl's error for a request aborted by the progress callback.
bool abortedByCancel(std::chrono::steady_clock::time_point start, const std::string& error)
{
  const double seconds = secondsSince(start);
  return seconds >= 0.25 && seconds < 5.0 && error.find("aborted") != std::string::npos;
}

// A request that is cancelled before it starts never opens a connection.
// Runs first: the stalling server never accepts, so a connection would
// stay queued and waitForClient would see it from then on.
void cancelledRequestsDoNotConnect(const grtest::StallServer& server,
  const S3Config& stalled, const std::filesystem::path& dir)
{
  const S3Active cancelled{[]() { return false; }, nullptr};
  const std::string path = (dir / "cancelled.sav").string();
  check(s3GetFile(stalled, stalled.object("cancelled.sav"), path, nullptr, cancelled) ==
      S3GetResult::Error && !std::filesystem::exists(path),
    "a cancelled download fails without writing a file");
  check(!s3PutFile(stalled, stalled.object("cancelled.sav"), path, nullptr, cancelled),
    "a cancelled upload fails");
  check(!server.waitForClient(200), "cancelled requests never reach the server");
}

// Requests stuck on a server that never answers end within about a second
// of the cancel, not after the 30 s CURLOPT_TIMEOUT.
void stalledTransfersAbort(const grtest::StallServer& server,
  const S3Config& stalled, const std::filesystem::path& dir)
{
  const std::string fresh = (dir / "fresh.sav").string();
  std::string error;
  auto start = std::chrono::steady_clock::now();
  const S3GetResult got = s3GetFile(stalled, stalled.object("fresh.sav"), fresh,
    &error, cancelAfter(300));
  check(got == S3GetResult::Error && abortedByCancel(start, error),
    "a stalled download aborts soon after the cancel");
  check(server.waitForClient(0), "the aborted download had reached the server");
  check(!std::filesystem::exists(fresh), "an aborted download writes no file");

  const std::string source = (dir / "source.sav").string();
  {
    std::ofstream file(source, std::ios::binary);
    file << "gunrunners-save test\n";
  }
  error.clear();
  start = std::chrono::steady_clock::now();
  check(!s3PutFile(stalled, stalled.object("source.sav"), source, &error, cancelAfter(300)) &&
      abortedByCancel(start, error),
    "a stalled upload aborts soon after the cancel");
}

// The positive control: while S3Active stays true, a request on the silent
// server keeps waiting; the progress callback does not end it on its own.
// It ends only when its S3Active turns false after 1.2 s.
void activeRequestsKeepWaiting(const S3Config& stalled, const std::filesystem::path& dir)
{
  std::string error;
  const auto start = std::chrono::steady_clock::now();
  const S3GetResult got = s3GetFile(stalled, stalled.object("waiting.sav"),
    (dir / "waiting.sav").string(), &error, cancelAfter(1200));
  const double seconds = secondsSince(start);
  check(got == S3GetResult::Error && seconds >= 1.15 && seconds < 6.0 &&
      error.find("aborted") != std::string::npos,
    "an active request keeps waiting on the server until its cancel");
}

// syncS3File cancelled mid-request: the local save stays as it was, a
// dirty save keeps its marker for the next sync, and no temporary files
// are left behind.
void stalledSyncsAbort(const S3Config& stalled, const std::filesystem::path& dir)
{
  SaveGame save;
  save.levelName = "S3 Stall Test";
  save.savedAt = "2026-01-01 10:00";
  save.hp = 3;
  const std::string dirty = (dir / "dirty.sav").string();
  const std::string clean = (dir / "clean.sav").string();
  check(writeS3Save(save, dirty) && writeSave(save, clean), "seed a dirty and a clean save");

  std::string error;
  auto start = std::chrono::steady_clock::now();
  check(syncS3File(stalled, stalled.object("dirty.sav"), dirty, &error, cancelAfter(300)) ==
      S3SyncResult::Error && abortedByCancel(start, error),
    "a stalled sync upload aborts soon after the cancel");
  const auto afterUpload = readSave(dirty);
  check(afterUpload && afterUpload->savedAt == save.savedAt &&
      std::filesystem::exists(dirty + ".cloud-dirty"),
    "an aborted upload keeps the save and its dirty marker");

  error.clear();
  start = std::chrono::steady_clock::now();
  check(syncS3File(stalled, stalled.object("clean.sav"), clean, &error, cancelAfter(300)) ==
      S3SyncResult::Error && abortedByCancel(start, error),
    "a stalled sync download aborts soon after the cancel");
  const auto afterDownload = readSave(clean);
  check(afterDownload && afterDownload->savedAt == save.savedAt,
    "an aborted download keeps the local save");
  check(grtest::noCloudTemporaries(dir), "aborted syncs leave no temporary files");
}

S3Config stalledConfig(const grtest::StallServer& server)
{
  S3Config stalled;
  stalled.endpoint = server.endpoint();
  stalled.bucket = "saves";
  stalled.accessKey = "key";
  stalled.secretKey = "secret";
  return stalled;
}

// A request whose TCP connect never completes (the server's accept queue is
// full, so its SYNs are dropped) also ends soon after the cancel, not after
// the 10 s CURLOPT_CONNECTTIMEOUT. Skipped only where the accept queue
// can't be watched (not Linux, or no tcp_info for a listening socket); a
// queue that tcp_info shows but that does not fill fails the test, as does
// a misbehaving abort.
void stalledConnectAborts(const std::filesystem::path& dir)
{
  grtest::StallServer server(grtest::StallServer::Stall::Connect);
  if (server.connectStallUnsupported())
  {
    std::puts("connect-phase cancel test skipped (no tcp_info for the accept queue here)");
    return;
  }
  check(server.ok(), "a full accept queue stalls new connections");
  if (!server.ok())
    return;
#ifdef __linux__
  const S3Config stalled = stalledConfig(server);
  const int queued = server.queuedConnections();
  std::string error;
  const auto start = std::chrono::steady_clock::now();
  check(s3GetFile(stalled, stalled.object("connect.sav"), (dir / "connect.sav").string(),
      &error, cancelAfter(300)) == S3GetResult::Error && abortedByCancel(start, error),
    "a request stuck connecting aborts soon after the cancel");
  check(server.queuedConnections() == queued,
    "the aborted request never got past connecting");
#else
  (void)dir; // not reached: Stall::Connect is Linux only (skipped above)
#endif
}

void cancelTests()
{
  grtest::StallServer server;
  check(server.ok(), "a local stalling server listens");
  if (!server.ok())
    return;
  const S3Config stalled = stalledConfig(server);
  const std::filesystem::path dir = std::filesystem::temp_directory_path() /
    ("gunrunners_s3_stall_test_" + std::to_string(::getpid()));
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  cancelledRequestsDoNotConnect(server, stalled, dir);
  stalledTransfersAbort(server, stalled, dir);
  activeRequestsKeepWaiting(stalled, dir);
  stalledSyncsAbort(stalled, dir);
  stalledConnectAborts(dir);
  std::filesystem::remove_all(dir);
}

} // namespace

int main()
{
  downloadsInstallAllOrNothing();
  S3Config invalid;
  std::string error;
  check(!invalid.valid(&error) && !error.empty(), "empty S3 configuration is rejected");
  invalid.endpoint = "http://127.0.0.1:1";
  check(!invalid.valid(&error), "incomplete S3 configuration is rejected");
  S3Config pasted;
  check(parseS3Config(
      "endpoint=https://objects.example\n"
      "bucket=saves\nregion=garage\nprefix=player one\n"
      "access_key=long/key+id\nsecret_key=long/secret+value=\n",
      pasted, &error) && pasted.prefix == "player one" &&
      pasted.secretKey == "long/secret+value=",
    "copy/paste configuration preserves long keys verbatim");
  check(!parseS3Config("endpoint=x\nunknown=y\n", pasted, &error),
    "unknown pasted settings are rejected");

  const std::filesystem::path configDir = std::filesystem::temp_directory_path() /
    ("gunrunners_s3_config_test_" + std::to_string(::getpid()));
  check(saveS3Config(configDir.string(), pasted, &error),
    "pasted configuration is saved");
  const S3Config loadedConfig = loadS3Config(configDir.string());
  check(loadedConfig.endpoint == pasted.endpoint &&
      loadedConfig.accessKey == pasted.accessKey &&
      loadedConfig.secretKey == pasted.secretKey,
    "saved configuration loads without changing credentials");
  check(removeS3Config(configDir.string(), &error) &&
      !loadS3Config(configDir.string()).enabled(),
    "cloud configuration can be disabled");
  std::filesystem::remove_all(configDir);
  configWritesAreSafe();
  leftoverTests();
  cancelTests();

  S3Config config;
  config.endpoint = env("GUNRUNNERS_TEST_S3_ENDPOINT");
  if (config.endpoint.empty())
  {
    std::puts("Garage integration skipped (GUNRUNNERS_TEST_S3_ENDPOINT is unset)");
    return gFailures == 0 ? 0 : 1;
  }
  config.bucket = env("GUNRUNNERS_TEST_S3_BUCKET");
  config.region = env("GUNRUNNERS_TEST_S3_REGION");
  config.accessKey = env("GUNRUNNERS_TEST_S3_ACCESS_KEY");
  config.secretKey = env("GUNRUNNERS_TEST_S3_SECRET_KEY");
  config.prefix = "integration/" + std::to_string(::getpid());
  check(config.valid(&error), "Garage test configuration is complete");
  if (!config.valid())
    return 1;

  const std::filesystem::path dir = std::filesystem::temp_directory_path() /
    ("gunrunners_s3_test_" + std::to_string(::getpid()));
  std::filesystem::create_directories(dir);
  const std::string source = (dir / "source.sav").string();
  const std::string destination = (dir / "destination.sav").string();
  {
    std::ofstream file(source, std::ios::binary);
    file << "gunrunners-save test\nwith binary-ish byte: \x01\n";
  }

  const std::string object = config.object("slot one+1.sav");
  check(s3PutFile(config, object, source, &error), "upload a save to Garage");
  check(s3GetFile(config, object, destination, &error) == S3GetResult::Downloaded,
    "download the save from Garage");
  check(read(source) == read(destination), "Garage round trip preserves every byte");
  check(s3GetFile(config, config.object("missing.sav"), destination, &error) ==
      S3GetResult::NotFound,
    "a missing object is reported as not found");

  SaveGame older;
  older.levelName = "S3 Test";
  older.savedAt = "2026-01-01 10:00";
  older.hp = 3;
  SaveGame newer = older;
  newer.savedAt = "2026-01-02 10:00";
  const std::string syncPath = (dir / "sync.sav").string();
  const std::string remotePath = (dir / "remote.sav").string();
  const std::string syncObject = config.object("sync.sav");
  check(writeSave(older, remotePath) && s3PutFile(config, syncObject, remotePath, &error),
    "seed an older cloud save");
  check(writeSave(newer, syncPath) && markS3Dirty(syncPath, &error),
    "mark newer offline progress dirty");
  check(syncS3File(config, syncObject, syncPath, &error) == S3SyncResult::Uploaded,
    "newer offline progress wins over stale cloud data");
  const auto uploaded = s3GetFile(config, syncObject, remotePath, &error);
  const auto uploadedSave = readSave(remotePath);
  check(uploaded == S3GetResult::Downloaded && uploadedSave &&
      uploadedSave->savedAt == newer.savedAt,
    "offline progress reaches the cloud");

  const std::string corrupt = (dir / "corrupt.sav").string();
  {
    std::ofstream file(corrupt);
    file << "not a save\n";
  }
  const std::string corruptObject = config.object("corrupt.sav");
  check(s3PutFile(config, corruptObject, corrupt, &error), "seed a corrupt cloud object");
  const auto corruptResult = syncS3File(config, corruptObject, syncPath, &error);
  const auto afterCorrupt = readSave(syncPath);
  check(corruptResult == S3SyncResult::Error && afterCorrupt &&
      afterCorrupt->savedAt == newer.savedAt,
    "corrupt cloud data cannot replace a valid local save");

  SaveGame newest = newer;
  newest.savedAt = "2026-01-03 10:00";
  check(writeSave(newest, remotePath) && s3PutFile(config, syncObject, remotePath, &error),
    "seed a newer cloud save");
  const auto newerResult = syncS3File(config, syncObject, syncPath, &error);
  const auto afterNewer = readSave(syncPath);
  check(newerResult == S3SyncResult::Downloaded && afterNewer &&
      afterNewer->savedAt == newest.savedAt,
    "newer cloud progress replaces an older clean cache");

  S3Config unauthorized = config;
  unauthorized.secretKey += "wrong";
  check(!s3PutFile(unauthorized, config.object("unauthorized.sav"), source, &error) &&
      !error.empty(),
    "bad credentials are rejected");

  std::filesystem::remove_all(dir);
  std::printf("%s\n", gFailures == 0 ? "all S3 tests passed" : "SOME S3 TESTS FAILED");
  return gFailures == 0 ? 0 : 1;
}
