// Quitting while a cloud save sync is still running: ~Game cancels the
// S3 workers instead of waiting out libcurl's timeouts (10 s connect, 30 s
// transfer), and the interrupted sync leaves the local saves as they were.

#include "frontend/game.hpp"
#include "stall_server.hpp"

#include <SDL.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
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

// Starts a Game whose cloud refresh is stuck on the server, then times its
// destruction. The first slot (slotPath index 0) holds a save not yet
// uploaded (dirty), so the refresh has an upload stalling next to the
// downloads of the other slots and the quick save.
double quitDuringSync(grtest::StallServer& server, const std::string& saves,
  SDL_Renderer* sdl)
{
  Renderer renderer(sdl);
  GameOptions o;
  o.saveDir = saves;
  o.noCutscenes = true;
  o.s3.endpoint = server.endpoint();
  o.s3.bucket = "saves";
  o.s3.accessKey = "key";
  o.s3.secretKey = "secret";
  auto game = std::make_unique<Game>(o, renderer, nullptr); // starts the refresh
  // One request per slot and one for the quick save, all in parallel.
  const int requests = kSaveSlots + 1;
  check(server.acceptClients(requests, 5000) == requests,
    "every request of the cloud refresh reaches the server");
  // So the quit below really cuts off live requests, rather than finding
  // them already failed.
  check(server.openClients() == requests, "the requests still wait when the game quits");
  const auto start = std::chrono::steady_clock::now();
  game.reset();
  const double seconds =
    std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  check(server.clientsClosed(2000), "quitting ends every stalled request");
  return seconds;
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
  std::filesystem::create_directories(dir);
  const std::string slot = slotPath(dir.string(), 0);
  SaveGame save;
  save.levelName = "Cloud Quit Test";
  save.savedAt = "2026-01-01 10:00";
  save.hp = 3;
  check(writeS3Save(save, slot), "seed a save that is waiting for its upload");

  // The server outlives the Game: closing it would end the requests early.
  grtest::StallServer server;
  check(server.ok(), "a local stalling server listens");
  if (server.ok())
  {
    const double seconds = quitDuringSync(server, dir.string(), sdl);
    std::printf("quitting took %.2f s\n", seconds);
    check(seconds < 5.0, "quitting does not wait for the stalled cloud sync");
    const auto after = readSave(slot);
    check(after && after->savedAt == save.savedAt, "the local save is untouched");
    check(std::filesystem::exists(slot + ".cloud-dirty"),
      "the interrupted upload stays marked for the next start");
    check(grtest::noCloudTemporaries(dir), "no temporary cloud files are left behind");
  }

  std::filesystem::remove_all(dir);
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf("%s\n", gFailures == 0 ? "all cloud quit tests passed" : "SOME CLOUD QUIT TESTS FAILED");
  return gFailures == 0 ? 0 : 1;
}
