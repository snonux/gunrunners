// Quitting while a cloud save sync is still running: ~Game cancels the
// S3 workers instead of waiting out libcurl's timeouts (10 s connect, 30 s
// transfer), and the interrupted sync leaves the local saves as they were.
// Three cases, each on its own local stalling server:
//   - the start-up refresh stalls: quitting cancels it at once;
//   - a quick save's upload stalls: quitting gives it its 2 s grace
//     (kQuitUploadGrace in menus.cpp) and then cancels it, so it takes
//     about 2-3 s, neither less (no grace) nor the 30 s timeout (the check
//     allows up to 5 s);
//   - the constructor throws after starting the refresh (a level file that
//     is not there): Game::CloudCancel cancels it as the members unwind.
// And the other end of a quit that never ran ~Game (Android kills a
// backgrounded app): the next start removes the temp files it left.

#include "frontend/game.hpp"
#include "stall_server.hpp"

#include <SDL.h>

#include <chrono>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
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

double secondsSince(std::chrono::steady_clock::time_point start)
{
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

// Times a Game's destruction.
double timeQuit(std::unique_ptr<Game>& game)
{
  const auto start = std::chrono::steady_clock::now();
  game.reset();
  return secondsSince(start);
}

GameOptions cloudOptions(const grtest::StallServer& server, const std::string& saves)
{
  GameOptions o;
  o.saveDir = saves;
  o.noCutscenes = true;
  o.s3.endpoint = server.endpoint();
  o.s3.bucket = "saves";
  o.s3.accessKey = "key";
  o.s3.secretKey = "secret";
  return o;
}

SaveGame seedSave()
{
  SaveGame save;
  save.levelName = "Cloud Quit Test";
  save.savedAt = "2026-01-01 10:00";
  save.hp = 3;
  return save;
}

// Starts a Game whose cloud refresh is stuck on the server, then times its
// destruction. The first slot (slotPath index 0) holds a save not yet
// uploaded (dirty), so the refresh has an upload stalling next to the
// downloads of the other slots and the quick save.
double quitDuringSync(grtest::StallServer& server, const std::string& saves,
  SDL_Renderer* sdl)
{
  Renderer renderer(sdl);
  auto game = std::make_unique<Game>(cloudOptions(server, saves), renderer, nullptr);
  // One request per slot and one for the quick save, all in parallel.
  const int requests = kSaveSlots + 1;
  check(server.acceptClients(requests, 5000) == requests,
    "every request of the cloud refresh reaches the server");
  // So the quit below really cuts off live requests, rather than finding
  // them already failed.
  check(server.openClients() == requests, "the requests still wait when the game quits");
  const double seconds = timeQuit(game);
  check(server.clientsClosed(2000), "quitting ends every stalled request");
  return seconds;
}

void quitDuringRefresh(const std::filesystem::path& dir, SDL_Renderer* sdl)
{
  std::filesystem::create_directories(dir);
  const std::string slot = slotPath(dir.string(), 0);
  const SaveGame save = seedSave();
  check(writeS3Save(save, slot), "seed a save that is waiting for its upload");

  // The server outlives the Game: closing it would end the requests early.
  grtest::StallServer server;
  check(server.ok(), "a local stalling server listens");
  if (!server.ok())
    return;
  const double seconds = quitDuringSync(server, dir.string(), sdl);
  std::printf("quitting during the refresh took %.2f s\n", seconds);
  check(seconds < 5.0, "quitting does not wait for the stalled cloud sync");
  const auto after = readSave(slot);
  check(after && after->savedAt == save.savedAt, "the local save is untouched");
  check(std::filesystem::exists(slot + ".cloud-dirty"),
    "the interrupted upload stays marked for the next start");
  check(grtest::noCloudTemporaries(dir), "no temporary cloud files are left behind");
}

// Into Level 1 of the campaign, settled in and able to save.
void settle(Game& g)
{
  for (int i = 0; i < 60 && !g.inLevel(); ++i)
    g.tick({});
  for (int i = 0; i < 120; ++i)
    g.tick({});
}

// F5 for one tick, then let go: the quick save, which queues its upload
// (Game::saveTo). Were the refresh not collected yet (pollCloud runs on
// tick), the save waits on its dirty marker and pollCloud queues it once
// the refresh is in, so it gets to the server either way.
void quickSave(Game& g)
{
  Input in;
  in.quickSave = true;
  g.tick(in);
  g.tick({});
}

// Ticks the game until the quick save's upload connects, for up to 5 s,
// and takes its request; true if that is a PUT, left waiting.
bool uploadReachesServer(Game& g, grtest::StallServer& server)
{
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  bool connected = false;
  while (!connected && std::chrono::steady_clock::now() < deadline)
  {
    g.tick({});
    connected = server.waitForClient(10);
  }
  return connected && server.serveRequest(1000) == "PUT";
}

// Ticks until the game has collected every cloud request (pollCloud runs
// on tick), for up to 5 s; true if it did.
bool cloudSettled(Game& g)
{
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (g.cloudSyncing() && std::chrono::steady_clock::now() < deadline)
  {
    g.tick({});
    if (g.cloudSyncing())
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return !g.cloudSyncing();
}

// A Game in Level 1 whose start-up refresh found nothing in the cloud, with
// a quick save's upload stuck on the server; nullptr if the setup failed.
std::unique_ptr<Game> uploadStalled(grtest::StallServer& server,
  const std::string& saves, Renderer& renderer)
{
  GameOptions o = cloudOptions(server, saves);
  o.skipMenu = true;
  o.startLevel = 1;
  auto game = std::make_unique<Game>(o, renderer, nullptr);
  int gets = 0;
  for (int i = 0; i < kSaveSlots + 1; ++i)
    gets += server.serveRequest(5000) == "GET" ? 1 : 0;
  check(gets == kSaveSlots + 1, "the start-up refresh finds no saves in the cloud");
  // The quick save must come after the refresh looked at the quick save's
  // dirty marker, or the refresh would upload it (and quitting cancels a
  // refresh at once, with no grace). The requests being over is not
  // enough: a worker checks the marker after its request. The game
  // collecting the refresh is: its future is ready only once every worker
  // has returned, so every check is done (Game::cloudSyncing).
  check(server.answeredClosed(5000), "the refresh's requests are over");
  check(cloudSettled(*game), "the game collects the finished refresh");
  settle(*game);
  check(game->inLevel(), "Level 1 runs");
  quickSave(*game);
  check(bool(readSave(quickSavePath(saves))), "the quick save is written");
  const bool stalled = uploadReachesServer(*game, server) && server.openClients() == 1;
  check(stalled, "the quick save's upload waits on the server when the game quits");
  if (!stalled)
    game.reset();
  return game;
}

// Quitting right after a SAVE: the upload gets its grace, then is cut off.
void quitDuringUpload(const std::filesystem::path& dir, SDL_Renderer* sdl)
{
  std::filesystem::create_directories(dir);
  grtest::StallServer server;
  check(server.ok(), "a local server that stalls uploads listens");
  if (!server.ok())
    return;
  Renderer renderer(sdl);
  auto game = uploadStalled(server, dir.string(), renderer);
  if (!game)
    return;
  const double seconds = timeQuit(game);
  std::printf("quitting during the upload took %.2f s\n", seconds);
  check(seconds >= 1.5, "quitting gives the upload its grace period");
  // Well under the 10 s connect and 30 s transfer timeouts, with slack
  // for a slow (sanitizer) build.
  check(seconds < 5.0, "quitting cuts the upload off after the grace period");
  check(server.clientsClosed(2000), "quitting ends the stalled upload");
  check(std::filesystem::exists(quickSavePath(dir.string()) + ".cloud-dirty"),
    "the cut-off upload stays marked for the next start");
  check(grtest::noCloudTemporaries(dir), "no temporary upload files are left behind");
}

// The constructor throws after beginCloudRefresh: ~Game never runs, so
// only CloudCancel keeps the refresh's futures from waiting out the 30 s
// transfer timeout as they are destroyed.
void throwingConstructor(const std::filesystem::path& dir, SDL_Renderer* sdl)
{
  std::filesystem::create_directories(dir);
  grtest::StallServer server;
  check(server.ok(), "a local stalling server listens for the throwing game");
  if (!server.ok())
    return;
  Renderer renderer(sdl);
  GameOptions o = cloudOptions(server, dir.string());
  o.levelPath = "/nonexistent"; // Level::loadFile throws
  bool threw = false;
  const auto start = std::chrono::steady_clock::now();
  try
  {
    Game game(o, renderer, nullptr);
  }
  catch (const std::exception&)
  {
    threw = true;
  }
  const double seconds = secondsSince(start);
  std::printf("the throwing constructor took %.2f s\n", seconds);
  check(threw, "a level file that is not there makes the constructor throw");
  check(seconds < 5.0, "the throw does not wait for the stalled cloud refresh");
}

// A game killed mid-sync left an upload snapshot, a download and a
// cloud.txt write temp holding the secret key, all older than
// kCloudLeftoverAge. Starting a Game (no cloud configured) removes them and
// keeps the save beside them and a file merely named like a leftover.
void startupRemovesLeftovers(const std::filesystem::path& dir, SDL_Renderer* sdl)
{
  namespace fs = std::filesystem;
  fs::create_directories(dir);
  const auto old = fs::file_time_type::clock::now() - std::chrono::hours(1);
  const std::string slot = slotPath(dir.string(), 0);
  const SaveGame save = seedSave();
  check(writeSave(save, slot), "seed a save beside the leftovers");
  fs::last_write_time(slot, old);
  const char* const leftovers[] = {
    "slot1.sav.cloud-upload-123", "quick.sav.cloud-incoming-456", "cloud.txt.tmp.123.0"};
  for (const char* name : leftovers)
  {
    std::ofstream(dir / name) << "secret_key=leaked\n";
    fs::last_write_time(dir / name, old);
  }
  const fs::path lookalike = dir / "slot9.sav.cloud-upload-123";
  std::ofstream(lookalike) << "not a leftover\n";
  fs::last_write_time(lookalike, old);
  {
    Renderer renderer(sdl);
    GameOptions o;
    o.saveDir = dir.string();
    o.noCutscenes = true;
    Game game(o, renderer, nullptr);
  }
  bool gone = true;
  for (const char* name : leftovers)
    gone = gone && !fs::exists(dir / name);
  check(gone, "the start removes what a killed game left");
  const auto after = readSave(slot);
  check(after && after->savedAt == save.savedAt, "...and keeps the save");
  check(fs::exists(lookalike), "...and a file merely named like a leftover");
}

} // namespace

int main()
{
  SDL_Init(0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  const std::filesystem::path dir = std::filesystem::temp_directory_path() /
    ("gunrunners_cloud_quit_test_" + std::to_string(::getpid()));
  std::filesystem::remove_all(dir);

  quitDuringRefresh(dir / "refresh", sdl);
  quitDuringUpload(dir / "upload", sdl);
  throwingConstructor(dir / "throw", sdl);
  startupRemovesLeftovers(dir / "start", sdl);

  std::filesystem::remove_all(dir);
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf("%s\n", gFailures == 0 ? "all cloud quit tests passed" : "SOME CLOUD QUIT TESTS FAILED");
  return gFailures == 0 ? 0 : 1;
}
