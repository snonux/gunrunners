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
#include <netinet/in.h>
#include <string>
#include <sys/resource.h>
#include <sys/socket.h>
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

#ifndef GR_NO_CURL
// While alive, no file of this process may grow past 0 bytes: every write
// fails with EFBIG (SIGXFSZ is ignored so it does not kill the test), as in
// tests/atomic_file_test.cpp. Sockets are not files, so the local server and
// curl still talk. Nothing may print in its scope, in case stdout is a file.
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

namespace fs = std::filesystem;

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

} // namespace
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
// the 10 s CURLOPT_CONNECTTIMEOUT. Skipped where the accept queue can't be
// filled that way (not Linux, or a kernel like gVisor or WSL1 that queues
// differently or keeps the queue out of tcp_info); once it is full, a
// misbehaving abort still fails.
void stalledConnectAborts(const std::filesystem::path& dir)
{
  grtest::StallServer server(grtest::StallServer::Stall::Connect);
  if (server.connectStallUnsupported())
  {
    std::puts("connect-phase cancel test skipped (the accept queue does not fill here)");
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
