#include "game/s3.hpp"
#include "game/savegame.hpp"
#include "stall_server.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <cctype>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <netinet/in.h>
#include <string>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <utility>
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

// An HTTP server on 127.0.0.1 that answers its connections in turn with the
// given replies, one each, then accepts no more: enough for s3GetFile and
// s3PutFile, which only sign their requests. Each request opens a
// connection of its own (every reply says Connection: close), so a sync's
// download and upload are two connections. The server records each
// request's method and body, so a test can see whether a sync uploaded
// (a PUT) and which bytes.
class ScriptedServer
{
public:
  struct Reply
  {
    int status;
    std::string body;
  };
  struct Request
  {
    std::string method;
    std::string body;
  };

  // One request, answered with 200 and body.
  explicit ScriptedServer(std::string body)
    : ScriptedServer(std::vector<Reply>{{200, std::move(body)}})
  {
  }
  explicit ScriptedServer(std::vector<Reply> replies) : mReplies(std::move(replies))
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
  ~ScriptedServer()
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
  // The requests answered so far, in order. Each is recorded before its
  // reply is sent, so all of a finished sync's requests are here.
  std::vector<Request> requests() const
  {
    std::lock_guard<std::mutex> lock(mMutex);
    return mRequests;
  }

private:
  void serve()
  {
    for (const Reply& reply : mReplies)
    {
      const int fd = ::accept(mListen, nullptr, nullptr);
      if (fd < 0)
        return;
      Request request = readRequest(fd);
      {
        std::lock_guard<std::mutex> lock(mMutex);
        mRequests.push_back(std::move(request));
      }
      writeAll(fd, "HTTP/1.1 " + std::to_string(reply.status) + (reply.status == 200 ? " OK" : " Error") +
        "\r\nContent-Length: " + std::to_string(reply.body.size()) + "\r\nConnection: close\r\n\r\n" +
        reply.body);
      ::close(fd);
    }
  }

  static void writeAll(int fd, const std::string& data)
  {
    ssize_t n;
    for (std::size_t sent = 0; sent < data.size() && (n = ::write(fd, data.data() + sent, data.size() - sent)) > 0;)
      sent += std::size_t(n);
  }

  // Reads one request: its headers, then as many body bytes as its
  // Content-Length says, after a 100 Continue should curl ask for one
  // (it may, for a larger upload).
  static Request readRequest(int fd)
  {
    std::string data;
    char buf[1024];
    ssize_t n = 0;
    std::size_t end = std::string::npos;
    while ((end = data.find("\r\n\r\n")) == std::string::npos && (n = ::read(fd, buf, sizeof(buf))) > 0)
      data.append(buf, std::size_t(n));
    if (end == std::string::npos)
      return {};
    std::string headers = data.substr(0, end + 2);
    std::transform(headers.begin(), headers.end(), headers.begin(),
      [](unsigned char c) { return char(std::tolower(c)); });
    const std::string lengthField = "\r\ncontent-length:";
    const std::size_t field = headers.find(lengthField);
    const std::size_t length = field == std::string::npos ? 0 :
      std::size_t(std::strtoull(headers.c_str() + field + lengthField.size(), nullptr, 10));
    if (headers.find("\r\nexpect: 100-continue") != std::string::npos)
      writeAll(fd, "HTTP/1.1 100 Continue\r\n\r\n");
    Request request;
    request.method = data.substr(0, data.find(' '));
    request.body = data.substr(end + 4);
    while (request.body.size() < length && (n = ::read(fd, buf, sizeof(buf))) > 0)
      request.body.append(buf, std::size_t(n));
    return request;
  }

  std::vector<Reply> mReplies;
  mutable std::mutex mMutex;
  std::vector<Request> mRequests;
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
  ScriptedServer server(kBody);
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
    ScriptedServer server(kBody);
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
  ScriptedServer server(kBody);
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
  ScriptedServer server(cloudCopy);
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

// A sync that uploads after comparing removes its download before the
// upload starts, not only when the upload returns (up to a request timeout
// later), so a kill during the upload leaves one file fewer. The upload's
// first active check, at s3PutFile's start, is the first one after the
// snapshot exists: there it counts the downloads left and cancels, so the
// server with a single reply (for the download) is enough.
void downloadGoneBeforeUpload(const fs::path& dir)
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
  local.savedAt = "2026-01-02 10:00";
  const std::string save = (dir / "up.sav").string();
  check(writeSave(local, save), "a newer local save, to upload");
  ScriptedServer server(cloudCopy);
  check(server.port() > 0, "the local server for an upload after a compare listens");
  int downloadsAtUpload = -1;
  const S3Active watch{[&dir, &downloadsAtUpload]() {
    if (filesStartingWith(dir, "up.sav.cloud-upload-") == 0)
      return true;
    downloadsAtUpload = filesStartingWith(dir, "up.sav.cloud-incoming-");
    return false;
  }, nullptr};
  std::string error;
  const S3SyncResult result = syncS3File(localConfig(server.port()), "up.sav", save, &error, watch);
  check(result == S3SyncResult::Error && downloadsAtUpload == 0,
    "a sync that uploads after comparing has removed its download when the upload starts");
  check(filesStartingWith(dir, "up.sav.cloud-") == 0, "...and leaves no file of its own");
}

// A valid save of level, saved at when.
SaveGame saveAt(const std::string& when, const std::string& level)
{
  SaveGame save;
  save.levelName = level;
  save.savedAt = when;
  save.hp = 3;
  return save;
}

// The bytes writeSave writes for save (through a scratch file in dir).
std::string saveBytes(const SaveGame& save, const fs::path& dir)
{
  const std::string scratch = (dir / "scratch.body").string();
  if (!writeSave(save, scratch))
    return {};
  const std::string bytes = read(scratch);
  fs::remove(scratch);
  return bytes;
}

// One offline sync of a clean local save: dir/name holds local, and the
// server answers the sync's requests with replies in turn.
struct SyncRun
{
  S3SyncResult result = S3SyncResult::Error;
  std::string error;
  std::vector<ScriptedServer::Request> requests;
  bool leftovers = true; // a "<name>.cloud-*" file is left in dir
};

SyncRun runSync(const fs::path& dir, const std::string& name, const SaveGame& local,
  std::vector<ScriptedServer::Reply> replies)
{
  const std::string save = (dir / name).string();
  SyncRun run;
  if (!writeSave(local, save))
    return run;
  ScriptedServer server(std::move(replies));
  if (server.port() == 0)
    return run;
  run.result = syncS3File(localConfig(server.port()), name, save, &run.error);
  run.requests = server.requests();
  run.leftovers = filesStartingWith(dir, name + ".cloud-") != 0;
  return run;
}

// The sync only downloaded: one GET, no upload.
bool onlyDownloaded(const SyncRun& run)
{
  return run.requests.size() == 1 && run.requests[0].method == "GET";
}

// The sync downloaded, then uploaded bytes.
bool uploaded(const SyncRun& run, const std::string& bytes)
{
  return run.requests.size() == 2 && run.requests[0].method == "GET" &&
    run.requests[1].method == "PUT" && run.requests[1].body == bytes;
}

// Saved at the same time with the same bytes: nothing to do, and no upload.
void sameTimeEqualUnchanged(const fs::path& dir)
{
  const SaveGame local = saveAt("2026-01-01 10:00", "Alpha");
  const std::string bytes = saveBytes(local, dir);
  const SyncRun run = runSync(dir, "equal.sav", local, {{200, bytes}});
  check(run.result == S3SyncResult::Unchanged && onlyDownloaded(run),
    "same time, same bytes: the sync is Unchanged and uploads nothing");
  check(read((dir / "equal.sav").string()) == bytes, "...and the local save stays");
  check(!run.leftovers, "...and no .cloud-* file is left");
}

// Saved at the same time with different bytes: the greater bytes win, both
// ways round, so two devices settle on the same save. Flipping the
// comparison in compareSameTime fails both halves: the greater local save
// would be replaced, the smaller one uploaded.
void sameTimeGreaterBytesWin(const fs::path& dir)
{
  const SaveGame alpha = saveAt("2026-01-01 10:00", "Alpha");
  const SaveGame bravo = saveAt("2026-01-01 10:00", "Bravo");
  const std::string lowBytes = saveBytes(alpha, dir);
  const std::string highBytes = saveBytes(bravo, dir);
  check(!lowBytes.empty() && lowBytes < highBytes, "two saves of the same time, Alpha's bytes less than Bravo's");
  const SyncRun up = runSync(dir, "greater.sav", bravo, {{200, lowBytes}, {200, ""}});
  check(up.result == S3SyncResult::Uploaded && uploaded(up, highBytes),
    "same time, local bytes greater: the local save is uploaded");
  check(read((dir / "greater.sav").string()) == highBytes, "...and stays as it was");
  check(!up.leftovers, "...and no .cloud-* file is left");
  const SyncRun down = runSync(dir, "smaller.sav", alpha, {{200, highBytes}});
  check(down.result == S3SyncResult::Downloaded && onlyDownloaded(down),
    "same time, local bytes smaller: the cloud save is Downloaded, nothing uploaded");
  check(read((dir / "smaller.sav").string()) == highBytes, "...and the local file now has the cloud's bytes");
  check(!down.leftovers, "...and no .cloud-* file is left");
}

// The cloud has no save yet (404): the local one goes up.
void missingCloudSaveUploads(const fs::path& dir)
{
  const SaveGame local = saveAt("2026-01-01 10:00", "Alpha");
  const SyncRun run = runSync(dir, "first.sav", local, {{404, ""}, {200, ""}});
  check(run.result == S3SyncResult::Uploaded && uploaded(run, saveBytes(local, dir)),
    "no cloud save (404): the local save is uploaded");
  check(!run.leftovers, "...and no .cloud-* file is left");
}

// A cloud object that is no valid save is an error, never installed, and
// nothing is uploaded over it.
void invalidCloudSaveIsError(const fs::path& dir)
{
  const SaveGame local = saveAt("2026-01-01 10:00", "Alpha");
  const SyncRun run = runSync(dir, "invalid.sav", local, {{200, "not a save\n"}});
  check(run.result == S3SyncResult::Error && run.error == "cloud object is not a valid Gunrunners save" &&
      onlyDownloaded(run),
    "an invalid cloud save is an error, and nothing is uploaded");
  check(read((dir / "invalid.sav").string()) == saveBytes(local, dir), "...and the local save is untouched");
  check(!run.leftovers, "...and no .cloud-* file is left");
}

// A clean local save newer than the cloud's goes up; one older is replaced
// by the cloud's.
void newerSaveWins(const fs::path& dir)
{
  const SaveGame older = saveAt("2026-01-01 10:00", "Alpha");
  const SaveGame newer = saveAt("2026-01-02 10:00", "Alpha");
  const std::string olderBytes = saveBytes(older, dir);
  const std::string newerBytes = saveBytes(newer, dir);
  const SyncRun up = runSync(dir, "newer.sav", newer, {{200, olderBytes}, {200, ""}});
  check(up.result == S3SyncResult::Uploaded && uploaded(up, newerBytes),
    "a clean local save newer than the cloud's is Uploaded");
  check(read((dir / "newer.sav").string()) == newerBytes, "...and stays as it was");
  check(!up.leftovers, "...and no .cloud-* file is left");
  const SyncRun down = runSync(dir, "older.sav", older, {{200, newerBytes}});
  check(down.result == S3SyncResult::Downloaded && onlyDownloaded(down) &&
      read((dir / "older.sav").string()) == newerBytes,
    "a clean local save older than the cloud's is replaced by it (Downloaded)");
  check(!down.leftovers, "...and no .cloud-* file is left");
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
  // The fixed temp names of older builds: a save's write temp and a
  // download's (s3GetFile wrote through "<path>.s3.tmp").
  "slot4.sav.tmp",
  "quick.sav.cloud-incoming-456.s3.tmp",
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
  "slot1.sav.tmpx",
  "slot1.sav.s3.tmp",
  "slot1.sav.cloud-upload-123.s3.tmp",
  "slot1.sav.cloud-incoming-.s3.tmp",
  "slot1.sav.cloud-incoming-4x6.s3.tmp",
  "slot1.sav.cloud-incoming-456.s3.tmp.1",
  "slot1.sav.cloud-incoming-456.s3",
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

// Sets the times of the symlink at path itself, not of its target, to an
// hour ago (older than kCloudLeftoverAge). fs::last_write_time follows
// symlinks, so this takes utimensat with AT_SYMLINK_NOFOLLOW.
bool makeLinkOld(const fs::path& path)
{
  timespec times[2] = {};
  times[0].tv_sec = times[1].tv_sec = std::time(nullptr) - 3600;
  return ::utimensat(AT_FDCWD, path.c_str(), times, AT_SYMLINK_NOFOLLOW) == 0;
}

// Symlinks named like leftovers, as old as stale ones, are no leftovers:
// neither sweep removes the link or its target (here a real save), so a
// planted link cannot make the game delete what it points to.
void symlinkedLeftoversStay(const fs::path& dir)
{
  plant(dir, "slot2.sav", "real slot2\n");
  const fs::path upload = dir / "slot1.sav.cloud-upload-1";
  const fs::path temp = dir / "cloud.txt.tmp.1.0";
  fs::create_symlink("slot2.sav", upload);
  fs::create_symlink("slot2.sav", temp);
  check(makeLinkOld(upload) && makeLinkOld(temp), "symlinks named like leftovers are an hour old");
  check(removeCloudLeftovers(dir.string()) == 0 && removeCloudLeftovers(dir.string(), std::chrono::seconds(0)) == 0,
    "the start's sweep removes no symlink, however old");
  std::string error;
  check(removeS3Config(dir.string(), &error), "turning the cloud off past a symlinked cloud.txt temp works");
  check(fs::is_symlink(upload) && fs::is_symlink(temp), "...and both symlinks stay");
  check(read((dir / "slot2.sav").string()) == "real slot2\n", "...and so does their target, the save");
}

// Turning the cloud off fails, keeping cloud.txt, when a temp of it with
// the secret may stay behind: the save dir cannot be listed (no read
// permission, so its temps cannot be found, while cloud.txt itself could
// still be removed), or a temp cannot be removed (no write permission).
// Root ignores these permissions, so the test is skipped as root.
void blockedConfigRemovalFails(const fs::path& dir)
{
  if (::geteuid() == 0)
  {
    std::puts("skip  blocked cloud config removal (root ignores directory permissions)");
    return;
  }
  check(saveS3Config(dir.string(), configWithSecret("kept")), "a config to fail to remove is saved");
  plant(dir, "cloud.txt.tmp.1.0", "secret_key=leaked\n");
  std::string error;
  fs::permissions(dir, fs::perms::owner_write | fs::perms::owner_exec);
  const bool unlisted = removeS3Config(dir.string(), &error);
  fs::permissions(dir, fs::perms::owner_all);
  check(!unlisted && error.find("cannot list") == 0, "a save dir that cannot be listed fails the removal");
  check(loadS3Config(dir.string()).secretKey == "kept" && fs::exists(dir / "cloud.txt.tmp.1.0"),
    "...and cloud.txt stays, as does the temp");
  error.clear();
  fs::permissions(dir, fs::perms::owner_read | fs::perms::owner_exec);
  const bool unremoved = removeS3Config(dir.string(), &error);
  const int swept = removeCloudLeftovers(dir.string(), std::chrono::seconds(0));
  fs::permissions(dir, fs::perms::owner_all);
  check(!unremoved && error.find("cannot remove") == 0, "a temp that cannot be removed fails the removal");
  check(swept == 0, "...and the start's sweep skips it");
  check(loadS3Config(dir.string()).secretKey == "kept" && fs::exists(dir / "cloud.txt.tmp.1.0"),
    "...and cloud.txt stays, as does the temp");
}

// Leftovers of killed syncs and writes are removed, never anything else;
// a failed snapshot leaves nothing behind in the first place.
void leftoverTests()
{
  const fs::path dir = fs::temp_directory_path() / ("gunrunners_s3_leftover_test_" + std::to_string(::getpid()));
  fs::remove_all(dir);
  for (const char* sub : {"sweep", "remove", "symlink", "blocked"})
    fs::create_directories(dir / sub);
  staleLeftoversRemoved(dir / "sweep");
  removeConfigSweepsTemps(dir / "remove");
  symlinkedLeftoversStay(dir / "symlink");
  blockedConfigRemovalFails(dir / "blocked");
#ifndef GR_NO_CURL
  ::setenv("no_proxy", "127.0.0.1", 1);
  ::setenv("NO_PROXY", "127.0.0.1", 1);
  fs::create_directories(dir / "snapshot");
  failedSnapshotLeavesNoTemp(dir / "snapshot");
  fs::create_directories(dir / "upload");
  downloadGoneBeforeUpload(dir / "upload");
#endif
  fs::remove_all(dir);
}

// How a sync of a clean local save settles with the cloud's
// (downloadAndReconcile, reconcile, compareSameTime), against the local
// server, which the Garage integration below otherwise covers only when
// it is configured.
void reconcileTests()
{
#ifndef GR_NO_CURL
  ::setenv("no_proxy", "127.0.0.1", 1);
  ::setenv("NO_PROXY", "127.0.0.1", 1);
  const fs::path dir = fs::temp_directory_path() / ("gunrunners_s3_reconcile_test_" + std::to_string(::getpid()));
  fs::remove_all(dir);
  fs::create_directories(dir);
  sameTimeEqualUnchanged(dir);
  sameTimeGreaterBytesWin(dir);
  missingCloudSaveUploads(dir);
  invalidCloudSaveIsError(dir);
  newerSaveWins(dir);
  fs::remove_all(dir);
#endif
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
  reconcileTests();
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
