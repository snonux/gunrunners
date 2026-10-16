// A stand-alone level (--level) never writes the player's profile (task le3).
// It plays with a default profile (the campaign's is not loaded), so a setting
// changed there used to save that default profile over the real profile.txt:
// campaign progress, the Arsenal and the Bonus Stars were gone. With a
// profile on disk ('reached 30', two prototypes, three stars), checks that in
// a stand-alone level
//   - ASSISTS AND OPTIONS (pause > CONTROLS > ASSISTS, the first row toggled),
//     FULLSCREEN, the menu pad and going to the background (suspend) leave
//     profile.txt byte for byte as it was;
//   - those settings still apply for the run: the toggle reaches the level
//     being played (HOLD TO FIRE in its play options, off before; this also
//     proves the presses hit the row, so the unchanged file is no accident),
//     fullscreen and the menu pad;
//   - suspend reports the unwritten profile as saved (nothing to write);
// and, the control, that in the campaign the same changes do reach the
// profile, next to the progress already in it. Last, the campaign still
// reports a refused write through saveProfile: with the save directory
// replaced by a regular file, suspend returns false (a writable one, true).

#include "frontend/game.hpp"
#include "game/profile.hpp"
#include "game/world.hpp"
#include "render/renderer.hpp"

#include <SDL.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
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

// Ticks until the level runs, then a little more.
void settle(Game& g)
{
  for (int i = 0; i < 60 && !g.inLevel(); ++i)
    g.tick({});
  for (int i = 0; i < 120; ++i)
    g.tick({});
  g.render();
}

// A button held for 3 ticks (as a --press holds it), then let go long enough
// for the menus to see it released.
void press(Game& g, bool Input::*button)
{
  Input in;
  in.*button = true;
  for (int i = 0; i < 3; ++i)
    g.tick(in);
  for (int i = 0; i < 17; ++i)
    g.tick({});
  g.render();
}

// Pause > CONTROLS (three up from RESUME: QUIT GAME, QUIT TO TITLE, CONTROLS;
// no CHEATS or FULLSCREEN item without a window) > ASSISTS AND OPTIONS (two up
// from the top row: RESET, then ASSISTS) > the first row (HOLD TO FIRE)
// toggled.
void toggleFirstAssist(Game& g)
{
  press(g, &Input::pause);
  for (int i = 0; i < 3; ++i)
    press(g, &Input::up);
  press(g, &Input::confirm);
  for (int i = 0; i < 2; ++i)
    press(g, &Input::up);
  press(g, &Input::confirm);
  press(g, &Input::confirm);
}

std::string readFile(const std::string& path)
{
  std::ifstream f(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

// The player's profile as the campaign left it.
void writeProgress(const std::string& dir)
{
  std::filesystem::create_directories(dir);
  Profile p;
  p.reached = 30;
  p.protos = {"laser", "rockets"};
  p.stars = {1, 2, 3};
  p.save(dir);
}

bool progressKept(const Profile& p)
{
  return p.reached == 30 && p.protos == std::set<std::string>{"laser", "rockets"} && p.stars == std::set<int>{1, 2, 3};
}

bool holdFireOn(const Profile& p)
{
  const auto it = p.controls.find("opt.holdfire");
  return it != p.controls.end() && !it->second.empty() && it->second[0] != 0;
}

// HOLD TO FIRE as the level being played has it (the menu sets the world's
// play options right away, saved or not).
bool worldHoldsFire(const Game& g)
{
  return g.world() && g.world()->playOptions().holdFire;
}

GameOptions standAlone(const std::string& dir)
{
  GameOptions o;
  o.saveDir = dir;
  o.skipMenu = true;
  o.noCutscenes = true;
  o.levelPath = std::string(GR_DATA_DIR) + "/levels/01_rooftop_run.txt";
  return o;
}

GameOptions campaignLevelOne(const std::string& dir)
{
  GameOptions o;
  o.saveDir = dir;
  o.skipMenu = true;
  o.startLevel = 1;
  o.noCutscenes = true;
  return o;
}

void checkStandAlone(Renderer& renderer, const std::string& dir)
{
  writeProgress(dir);
  const std::string before = readFile(dir + "/profile.txt");
  check(!before.empty() && progressKept(Profile::load(dir)), "stand-alone: the player's profile is on disk");
  Game g(standAlone(dir), renderer, nullptr);
  settle(g);
  check(g.inLevel(), "stand-alone: Level 1 runs on its own");
  check(g.world() && !worldHoldsFire(g), "stand-alone: HOLD TO FIRE starts off (a default profile)");
  toggleFirstAssist(g);
  check(worldHoldsFire(g), "stand-alone: the toggle took effect in the level being played");
  check(readFile(dir + "/profile.txt") == before, "stand-alone: an ASSISTS AND OPTIONS toggle leaves profile.txt alone");
  g.setFullscreen(true);
  check(g.fullscreenSetting(), "stand-alone: FULLSCREEN applies for the run");
  g.setMenuPad(true);
  check(g.menuPad(), "stand-alone: the menu pad applies for the run");
  check(g.suspend(), "stand-alone: suspend counts the profile it does not write as saved");
  check(readFile(dir + "/profile.txt") == before,
    "stand-alone: FULLSCREEN, the menu pad and suspend leave profile.txt alone");
  const Profile p = Profile::load(dir);
  check(progressKept(p) && !p.fullscreen && !holdFireOn(p), "stand-alone: the progress is all there, no setting saved");
}

// The control: the same changes in the campaign are saved, and the progress
// stays (so the stand-alone checks above did press the menus right).
void checkCampaign(Renderer& renderer, const std::string& dir)
{
  writeProgress(dir);
  Game g(campaignLevelOne(dir), renderer, nullptr);
  settle(g);
  check(g.inLevel(), "campaign: Level 1 runs");
  toggleFirstAssist(g);
  check(worldHoldsFire(g), "campaign: the toggle took effect in the level being played");
  check(holdFireOn(Profile::load(dir)), "campaign: the ASSISTS AND OPTIONS toggle is saved");
  g.setFullscreen(true);
  g.setMenuPad(true);
  const Profile p = Profile::load(dir);
  check(p.fullscreen, "campaign: FULLSCREEN is saved");
  check(progressKept(p), "campaign: ...next to the progress already there");
}

// A campaign save the disk refuses still comes back from saveProfile (only a
// stand-alone level skips the write): suspend, which saves the profile,
// returns true into a writable directory and false once the save directory
// has become a regular file (profile.txt cannot go below it, root or not).
void checkCampaignRefused(Renderer& renderer, const std::string& dir)
{
  writeProgress(dir);
  Game g(campaignLevelOne(dir), renderer, nullptr);
  settle(g);
  check(g.inLevel(), "refused: Level 1 runs");
  check(g.suspend(), "refused: the control, suspend saves the profile to a writable directory");
  std::filesystem::remove_all(dir);
  {
    std::ofstream file(dir);
    file << "not a directory\n";
  }
  check(std::filesystem::is_regular_file(dir), "refused: the save directory is now a regular file");
  check(!g.suspend(), "refused: suspend reports the profile write the disk refused");
}

} // namespace

int main()
{
  // Unbuffered: under ctest stdout is a pipe, and a crash would lose it all.
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  const std::string saves = (std::filesystem::temp_directory_path() /
    ("gunrunners_profile_keep_test_" + std::to_string(::getpid()))).string();
  std::filesystem::remove_all(saves);

  SDL_Init(0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  {
    Renderer renderer(sdl);
    checkStandAlone(renderer, saves + "/stand_alone");
    checkCampaign(renderer, saves + "/campaign");
    checkCampaignRefused(renderer, saves + "/refused");
  }
  std::filesystem::remove_all(saves);
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf(gFailures ? "%d FAILED\n" : "all passed\n", gFailures);
  return gFailures ? 1 : 0;
}
