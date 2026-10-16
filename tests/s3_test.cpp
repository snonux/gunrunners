#include "game/s3.hpp"
#include "game/savegame.hpp"

#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <netinet/in.h>
#include <string>
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

// A download goes through the same all-or-nothing write as the saves: the
// body lands in full, and a download that cannot be installed (here a
// directory in the way) reports an error and leaves no temp file behind.
void downloadsInstallAllOrNothing()
{
#ifndef GR_NO_CURL
  ::setenv("no_proxy", "127.0.0.1", 1);
  ::setenv("NO_PROXY", "127.0.0.1", 1);
  namespace fs = std::filesystem;
  const fs::path dir = fs::temp_directory_path() / ("gunrunners_s3_get_test_" + std::to_string(::getpid()));
  fs::remove_all(dir);
  fs::create_directories(dir);
  const std::string body = "gunrunners-save 1\nlevel LOCAL\nend\n";
  std::string error;
  const std::string good = (dir / "good.sav").string();
  {
    OneShotServer server(body);
    check(server.port() > 0 && s3GetFile(localConfig(server.port()), "good.sav", good, &error) ==
        S3GetResult::Downloaded && read(good) == body,
      "a download lands in full");
  }
  const std::string blocked = (dir / "blocked.sav").string();
  fs::create_directories(blocked + "/inside");
  {
    OneShotServer server(body);
    check(s3GetFile(localConfig(server.port()), "blocked.sav", blocked, &error) == S3GetResult::Error &&
        !error.empty(),
      "a download that cannot be installed is an error");
  }
  check(fs::is_directory(blocked + "/inside"), "a refused download keeps what was there");
  int entries = 0;
  for (const auto& entry : fs::directory_iterator(dir))
    entries += entry.path().filename() == "good.sav" || entry.path().filename() == "blocked.sav" ? 0 : 1;
  check(entries == 0, "a refused download leaves no temp file behind");
  fs::remove_all(dir);
#endif
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
