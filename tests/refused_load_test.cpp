// A save the game refuses to load leaves the run in progress alone. With
// Level 1 running (on NORMAL), quick loads of campaign saves that can't be
// played are refused:
//   - a HARD save of Level 2 with a box the level no longer has (as after
//     an update of the level);
//   - the same save pointing at a level file that is gone;
//   - the same save naming another level than its file holds;
//   - a save of Level 1 itself with a box too many.
// Each time Level 1's world carries on, ticked and drawn (before, the saves
// of another level tore it down first, and the next tick or frame crashed on
// the missing world), a new quick save still records Level 1, and HARD stays
// off in the profile. The boxed Level 2 save is also refused from the LOAD
// GAME slots (the menu stays open over the same world), inside Level 1's
// bonus level (the bonus plays on, and the world parked behind it comes back
// when it ends, with Level 1's file and number) and on the title screen (no
// level starts, and NEW GAME then plays on NORMAL). The intact Level 2 save
// is the control each time: it loads, brings Level 2 with its HARD enemies
// and switches HARD on (mid-bonus it also ends the bonus level, so a quick
// save works again). Last, SWAP RUNNER ON DEATH: after a load, a wiped-out
// level starts over with the runner it started with (a save of the level
// played), with the save's runner (a save from another level), and a
// refused load changes neither.

#include "frontend/game.hpp"
#include "game/profile.hpp"
#include "game/savegame.hpp"
#include "game/world.hpp"
#include "render/renderer.hpp"

#include <SDL.h>

#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <unistd.h>

using namespace gr;

namespace
{

int gFailures = 0;

// Returns ok, so a case can stop where its setup failed.
bool check(bool ok, const std::string& what)
{
  std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
  if (!ok)
    ++gFailures;
  return ok;
}

// Ticks and draws like the game's loop.
void run(Game& g, int ticks, Input in = {})
{
  for (int i = 0; i < ticks; ++i)
    g.tick(in);
  g.render();
}

// A key pressed for one tick, then let go.
void press(Game& g, bool Input::*button)
{
  Input in;
  in.*button = true;
  run(g, 1, in);
  run(g, 1);
}

// A campaign game on a level, settled in and able to save.
GameOptions campaignAt(const std::string& saves, int level, bool hard)
{
  GameOptions o;
  o.saveDir = saves;
  o.skipMenu = true;
  o.startLevel = level;
  o.noCutscenes = true;
  o.hard = hard;
  return o;
}

void settle(Game& g)
{
  for (int i = 0; i < 60 && !g.inLevel(); ++i)
    g.tick({});
  run(g, 120);
}

// F5, and the quick save it wrote.
std::optional<SaveGame> quickSave(Game& g, const std::string& saves)
{
  std::filesystem::remove(quickSavePath(saves));
  press(g, &Input::quickSave);
  return readSave(quickSavePath(saves));
}

bool hardInProfile(const std::string& saves)
{
  const Profile p = Profile::load(saves);
  const auto it = p.controls.find("opt.hard");
  return it != p.controls.end() && !it->second.empty() && it->second[0] != 0;
}

// A copy of a save with a box its level does not have: it does not fit.
SaveGame withExtraBox(SaveGame s)
{
  s.boxes.push_back(true);
  return s;
}

// F9 on a save that must be refused: the same world plays on, in Level 1.
void checkRefused(Game& g, const std::string& saves, const SaveGame& s, const std::string& what)
{
  check(writeSave(s, quickSavePath(saves)), what + ": written as the quick save");
  const World* before = g.world();
  const int clock = before ? before->clock() : 0;
  press(g, &Input::quickLoad);
  check(g.world() == before && g.inLevel(), what + ": refused, the run in progress carries on");
  run(g, 30);
  const World* w = g.world();
  check(w == before && w && w->level().name == "STAGE 1 - ROOFTOP RUN" && !w->level().hard,
    what + ": still Level 1 on NORMAL after ticks and frames");
  check(w && w->clock() > clock, what + ": its clock keeps running");
  check(!hardInProfile(saves), what + ": HARD stays off");
  // The level's number and file are still Level 1's: a quick save records them.
  const auto saved = quickSave(g, saves);
  check(saved && saved->levelNumber == 1 && saved->levelFile == "levels/01_rooftop_run.txt" && !saved->hard,
    what + ": a quick save still records Level 1, on NORMAL");
}

// A quick save of Level 2 on HARD (the option, not the profile).
std::optional<SaveGame> hardLevelTwoSave(Renderer& renderer, const std::string& saves)
{
  Game a(campaignAt(saves, 2, true), renderer, nullptr);
  settle(a);
  return quickSave(a, saves);
}

// The saves that can't be played, each a copy of one that loads (two:
// Level 2 on HARD, one: Level 1, the level running).
void checkRefusals(Game& g, const std::string& saves, const SaveGame& two, const SaveGame& one)
{
  checkRefused(g, saves, withExtraBox(two), "Level 2 with a box it no longer has");
  SaveGame gone = two;
  gone.levelFile = "levels/99_no_such_level.txt";
  checkRefused(g, saves, gone, "a level file that is gone");
  SaveGame renamed = two;
  renamed.levelName = "STAGE 99 - NOWHERE";
  checkRefused(g, saves, renamed, "another level than its file holds");
  checkRefused(g, saves, withExtraBox(one), "Level 1 with a box too many");
}

// The control: the intact save loads, with its level and difficulty.
void checkIntactLoads(Game& g, const std::string& saves, const SaveGame& two)
{
  check(writeSave(two, quickSavePath(saves)), "the intact Level 2 save written as the quick save");
  press(g, &Input::quickLoad);
  // The level's name tells the new world (a pointer could be reused).
  const World* w = g.world();
  check(w && g.inLevel() && w->level().name == "STAGE 2 - GLASS CANYON" && w->level().hard,
    "the intact save loads Level 2 on HARD");
  check(hardInProfile(saves), "...and switches HARD on in the profile");
  run(g, 120);
  const auto loaded = quickSave(g, saves);
  check(loaded && loaded->levelNumber == 2 && loaded->levelFile == two.levelFile && loaded->hard,
    "...and a quick save then is Level 2's");
}

// Pause > LOAD GAME: the slot menu, on the newest save.
void openLoadMenu(Game& g)
{
  press(g, &Input::pause);
  press(g, &Input::down); // SAVE GAME
  press(g, &Input::down); // LOAD GAME
  press(g, &Input::confirm);
}

// Slot 1 in Level 1: a save there that does not fit is refused with the slot
// menu still open (ticked and drawn over the level: a crash site before).
void checkSlotLoads(Renderer& renderer, const std::string& dir, const SaveGame& two)
{
  Game g(campaignAt(dir, 1, false), renderer, nullptr);
  settle(g);
  check(writeSave(withExtraBox(two), slotPath(dir, 0)), "slots: a save that does not fit in slot 1");
  const World* before = g.world();
  if (!check(before != nullptr, "slots: Level 1 runs"))
    return;
  const int clock = before->clock();
  openLoadMenu(g);
  check(before && !g.inLevel() && g.world() == before, "slots: LOAD GAME opens over Level 1");
  press(g, &Input::confirm);
  run(g, 30);
  // The menu holds the world still: the same one, at the same frame.
  check(!g.inLevel() && g.world() == before && g.world()->clock() == clock,
    "slots: refused, the menu stays over the same world, held still");
  press(g, &Input::back); // to the pause menu
  press(g, &Input::back); // to the level
  run(g, 30);
  check(g.inLevel() && g.world() == before && g.world()->clock() > clock,
    "slots: back in Level 1, the same world plays on");
  check(!hardInProfile(dir), "slots: HARD stays off");
  check(writeSave(two, slotPath(dir, 0)), "slots: the intact save in slot 1");
  openLoadMenu(g);
  press(g, &Input::confirm);
  const World* w = g.world();
  check(g.inLevel() && w && w->level().name == "STAGE 2 - GLASS CANYON" && w->level().hard &&
    hardInProfile(dir), "slots: the intact save loads Level 2 on HARD from the same menu");
}

const Prop* bonusDoor(const World& w)
{
  for (const auto& pr : w.props())
    if (pr.kind == PropKind::BonusDoor)
      return &pr;
  return nullptr;
}

// Level 1's TV: a quick save puts the runner on it, and up goes in (as in
// audit_test's B08). Returns the world parked behind the bonus level, or
// nullptr if the way in failed.
const World* enterBonus(Game& g, const std::string& dir)
{
  const Prop* tv = g.world() ? bonusDoor(*g.world()) : nullptr;
  if (!tv)
    return nullptr;
  SaveGame s = g.world()->snapshot();
  s.x = tv->x;
  s.y = tv->y + tv->h - 1;
  writeSave(s, quickSavePath(dir));
  press(g, &Input::quickLoad);
  const World* main = g.world(); // the loaded world, at the TV
  Input up;
  up.up = true;
  for (int i = 0; i < 60 && (!g.world() || g.world() == main); ++i)
    g.tick(g.world() == main ? up : Input{});
  run(g, 4);
  const World* bonus = g.world();
  const bool in = main && bonus && bonus != main && bonus->level().name != main->level().name;
  check(in, "bonus: Level 1's TV leads into its bonus level");
  return in ? main : nullptr;
}

// The bonus level runs out: the world parked behind it comes back (the same
// one, its clock going on from where it stood) with Level 1's file and number.
void checkParkedWorldReturns(Game& g, const std::string& dir, const World* main, int parkedClock)
{
  for (int i = 0; i < 8000 && g.world() != main; ++i)
    g.tick({});
  run(g, 120);
  const World* back = g.world();
  // 120 ticks are 30 logic frames on from where it stood; a world built anew
  // would be at about 30 in all.
  check(back == main && g.inLevel() && back->level().name == "STAGE 1 - ROOFTOP RUN" &&
      back->clock() >= parkedClock + 20,
    "bonus: when it ends, the parked Level 1 world comes back and plays on (" + std::to_string(parkedClock) +
      " -> " + std::to_string(back ? back->clock() : -1) + ")");
  const auto saved = quickSave(g, dir);
  check(saved && saved->levelNumber == 1 && saved->levelFile == "levels/01_rooftop_run.txt" && !saved->hard,
    "bonus: ...and a quick save there is Level 1's, on NORMAL");
}

// A refused load inside Level 1's bonus level: the bonus plays on, and the
// world parked behind it is the one that comes back once its clock runs out.
void checkRefusedInBonus(Renderer& renderer, const std::string& dir, const SaveGame& two)
{
  Game g(campaignAt(dir, 1, false), renderer, nullptr);
  settle(g);
  const World* main = enterBonus(g, dir);
  if (!main)
    return;
  const int parkedClock = main->clock(); // it stands still while parked
  run(g, 30);
  const World* bonus = g.world();
  if (!check(bonus != nullptr, "bonus: the bonus level runs"))
    return;
  const int clock = bonus->clock();
  check(writeSave(withExtraBox(two), quickSavePath(dir)), "bonus: a save that does not fit as the quick save");
  press(g, &Input::quickLoad);
  check(g.world() == bonus && g.world()->clock() >= clock, "bonus: refused, the same bonus world");
  run(g, 30);
  // 32 ticks are 8 logic frames on; a bonus world built anew would be at 8 in all.
  check(g.inLevel() && g.world() == bonus && g.world()->clock() >= clock + 5,
    "bonus: ...plays on (" + std::to_string(clock) + " -> " + std::to_string(g.world() ? g.world()->clock() : -1) + ")");
  check(!hardInProfile(dir), "bonus: HARD stays off");
  check(g.world() && g.world()->canSave() && !quickSave(g, dir),
    "bonus: no quick save while the bonus level runs (its world could save)");
  checkParkedWorldReturns(g, dir, main, parkedClock);
}

// The control mid-bonus: the intact save loads and ends the bonus level, so
// nothing is parked any more and a quick save works (refused while it is).
void checkIntactLoadsInBonus(Renderer& renderer, const std::string& dir, const SaveGame& two)
{
  Game g(campaignAt(dir, 1, false), renderer, nullptr);
  settle(g);
  if (!enterBonus(g, dir))
    return;
  check(writeSave(two, quickSavePath(dir)), "bonus: the intact save as the quick save");
  press(g, &Input::quickLoad);
  const World* w = g.world();
  check(g.inLevel() && w && w->level().name == "STAGE 2 - GLASS CANYON" && w->level().hard &&
    hardInProfile(dir), "bonus: the intact save loads Level 2 on HARD");
  run(g, 120);
  const auto saved = quickSave(g, dir);
  check(saved && saved->levelNumber == 2 && saved->levelFile == two.levelFile && saved->hard,
    "bonus: ...and ends the bonus level: a quick save there is Level 2's");
}

GameOptions onTitle(const std::string& dir)
{
  GameOptions o = campaignAt(dir, 1, false);
  o.skipMenu = false;
  return o;
}

// F9 on the title screen, no level running: a refused save starts nothing,
// and HARD stays off in memory too (NEW GAME then plays Level 1 on NORMAL).
void checkRefusedOnTitle(Renderer& renderer, const std::string& dir, const SaveGame& two)
{
  Game g(onTitle(dir), renderer, nullptr);
  run(g, 30);
  std::filesystem::create_directories(dir);
  check(writeSave(withExtraBox(two), quickSavePath(dir)), "title: a save that does not fit as the quick save");
  press(g, &Input::quickLoad);
  run(g, 30);
  check(!g.inLevel() && !g.world(), "title: refused, no level starts");
  check(!hardInProfile(dir), "title: HARD stays off");
  press(g, &Input::confirm); // NEW GAME
  run(g, 30);
  press(g, &Input::confirm); // runner select: Dash
  for (int i = 0; i < 60 && !g.inLevel(); ++i)
    g.tick({});
  const World* w = g.world();
  check(g.inLevel() && w && w->level().name == "STAGE 1 - ROOFTOP RUN" && !w->level().hard,
    "title: ...and NEW GAME then starts Level 1 on NORMAL");
}

// The control on the title screen: F9 on the intact save starts Level 2.
void checkIntactLoadsOnTitle(Renderer& renderer, const std::string& dir, const SaveGame& two)
{
  Game g(onTitle(dir), renderer, nullptr);
  run(g, 30);
  std::filesystem::create_directories(dir);
  check(writeSave(two, quickSavePath(dir)), "title: the intact save as the quick save");
  press(g, &Input::quickLoad);
  const World* w = g.world();
  check(g.inLevel() && w && w->level().name == "STAGE 2 - GLASS CANYON" && w->level().hard &&
    hardInProfile(dir), "title: the intact save starts Level 2 on HARD");
}

// SWAP RUNNER ON DEATH on, in a fresh profile.
void swapDeathOn(const std::string& dir)
{
  std::filesystem::create_directories(dir);
  Profile p;
  p.controls["opt.swapdeath"] = {1};
  p.save(dir);
}

// Bolt alone on one heart over Level 1's spike shaft (as in swap_death_test),
// as a campaign save of Level 1: his death puts every runner down.
SaveGame doomedBolt(const World& levelOne)
{
  SaveGame s = levelOne.snapshot();
  s.levelFile = "levels/01_rooftop_run.txt";
  s.levelNumber = 1;
  s.character = 5;
  s.hp = 1;
  s.bench = {1, 1, 1, 1, 1, 0};
  s.x = 80;
  s.y = 55;
  return s;
}

// F9 on the doomed save, then ticks until the level starts over: the runner
// it starts over with (-1 if the save did not load or nobody fell).
int startOverRunner(Game& g, const std::string& dir, const SaveGame& doomed)
{
  writeSave(doomed, quickSavePath(dir));
  press(g, &Input::quickLoad);
  const World* loaded = g.world();
  if (!loaded || loaded->characterIndex() != 5)
    return -1;
  for (int i = 0; i < 600 && g.world() == loaded; ++i)
    g.tick({});
  return g.world() && g.world() != loaded ? g.world()->characterIndex() : -1;
}

// A save of the level being played keeps the runner it started with (Dash)
// for the start over. Returns the doomed save for the other two cases.
SaveGame checkStartOverSameLevel(Renderer& renderer, const std::string& dir)
{
  swapDeathOn(dir);
  Game g(campaignAt(dir, 1, false), renderer, nullptr);
  settle(g);
  if (!check(g.world() != nullptr, "swap death: Level 1 runs"))
    return {};
  const SaveGame doomed = doomedBolt(*g.world());
  const int r = startOverRunner(g, dir, doomed);
  check(r == 0, "swap death: a save of the level played starts over with its first runner, Dash (" +
    std::to_string(r) + ")");
  return doomed;
}

// A save from another level (Level 2 runs) starts over with its own runner.
void checkStartOverOtherLevel(Renderer& renderer, const std::string& dir, const SaveGame& doomed)
{
  swapDeathOn(dir);
  Game g(campaignAt(dir, 2, false), renderer, nullptr);
  settle(g);
  const int r = startOverRunner(g, dir, doomed);
  check(r == 5, "swap death: a save from another level starts over with its own runner, Bolt (" +
    std::to_string(r) + ")");
}

// A refused save (another level's, as Jade) leaves the start over to Dash.
// This is the one check that sees mLevel itself: had the refusal left the
// save's level (Level 2, HARD) in mLevel, the doomed Level 1 save would pass
// for one from another level and start over with Bolt. So the refused save
// must be one whose level is read again (another level or difficulty); a
// save of Level 1 on NORMAL would not catch it.
void checkStartOverAfterRefusal(Renderer& renderer, const std::string& dir, const SaveGame& doomed,
  const SaveGame& two)
{
  swapDeathOn(dir);
  Game g(campaignAt(dir, 1, false), renderer, nullptr);
  settle(g);
  SaveGame jade = withExtraBox(two);
  jade.character = 3;
  checkRefused(g, dir, jade, "swap death: a refused save as Jade");
  const int r = startOverRunner(g, dir, doomed);
  check(r == 0, "swap death: ...and the level still starts over with Dash (" + std::to_string(r) + ")");
}

// Level 1 on NORMAL, and every load tried on it.
void checkLoadsInLevelOne(Renderer& renderer, const std::string& saves, const SaveGame& two)
{
  Game g(campaignAt(saves, 1, false), renderer, nullptr);
  settle(g);
  const std::optional<SaveGame> one = quickSave(g, saves);
  check(one && one->levelNumber == 1 && one->levelFile == "levels/01_rooftop_run.txt" && !one->hard,
    "Level 1 runs on NORMAL and saves");
  if (!one)
    return;
  checkRefusals(g, saves, two, *one);
  checkIntactLoads(g, saves, two);
}

} // namespace

int main()
{
  // Unbuffered: under ctest stdout is a pipe, and a crash would lose it all.
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  const std::string saves = (std::filesystem::temp_directory_path() /
    ("gunrunners_refused_load_test_" + std::to_string(::getpid()))).string();
  std::filesystem::remove_all(saves);

  SDL_Init(0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  {
    Renderer renderer(sdl);
    const std::optional<SaveGame> two = hardLevelTwoSave(renderer, saves);
    check(two && two->levelNumber == 2 && two->levelFile == "levels/02_glass_canyon.txt" && two->hard,
      "a HARD quick save of Level 2");
    check(!hardInProfile(saves), "HARD is off in the profile to begin with");
    if (two)
    {
      checkLoadsInLevelOne(renderer, saves, *two);
      checkSlotLoads(renderer, saves + "/slots", *two);
      checkRefusedInBonus(renderer, saves + "/bonus", *two);
      checkIntactLoadsInBonus(renderer, saves + "/bonus_load", *two);
      checkRefusedOnTitle(renderer, saves + "/title", *two);
      checkIntactLoadsOnTitle(renderer, saves + "/title_load", *two);
      const SaveGame doomed = checkStartOverSameLevel(renderer, saves + "/swap_same");
      checkStartOverOtherLevel(renderer, saves + "/swap_other", doomed);
      checkStartOverAfterRefusal(renderer, saves + "/swap_refused", doomed, *two);
    }
  }
  std::filesystem::remove_all(saves);
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf(gFailures ? "%d FAILED\n" : "all passed\n", gFailures);
  return gFailures ? 1 : 0;
}
