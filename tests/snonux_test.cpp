// Snonux, the secret runner: hidden until unlocked, then built in at index 6
// with the custom runners after him; everything maxed (hearts, jump, every
// gun full, endless rapid fire, running a cell a frame faster); savegames
// name him and unlock him; the profile remembers the code; and the code
// itself, a runner named SNONUX in the runner editor.

#include "assets/art.hpp"
#include "data/characters.hpp"
#include "data/level.hpp"
#include "data/theme.hpp"
#include "frontend/game.hpp"
#include "game/profile.hpp"
#include "game/world.hpp"
#include "render/renderer.hpp"

#include <SDL.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
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

// A 60 x 24 block room with a floor and the runner at block (3, 21).
std::string room()
{
  const int h = 24, w = 60;
  std::vector<std::string> map(std::size_t(h), std::string(std::size_t(w), '.'));
  for (int y = 0; y < h; ++y)
    map[std::size_t(y)][0] = map[std::size_t(y)][1] = map[std::size_t(y)][w - 2] = map[std::size_t(y)][w - 1] = '#';
  for (int x = 0; x < w; ++x)
    map[std::size_t(h - 2)][std::size_t(x)] = map[std::size_t(h - 1)][std::size_t(x)] = '#';
  map[21][3] = 'P';
  std::string s = "name=SNONUX TEST\nepisode=1\ntheme=neon_rooftops\nweapon=pulse_pistol\n[map]\n";
  for (const auto& row : map)
    s += row + "\n";
  s += "[entities]\n";
  return s;
}

bool carries(const Player& p, Weapon w, int ammo)
{
  for (int i = 0; i < p.spareCount; ++i)
    if (p.spares[std::size_t(i)].weapon == w && p.spares[std::size_t(i)].ammo == ammo)
      return true;
  return false;
}

// Cells walked (or run) to the right in 12 logic frames.
int distance(World& w, bool run)
{
  PlayerInput in;
  in.right = true;
  w.update(in); // the first frame may turn around
  const int x0 = w.player().x;
  in.run = run;
  for (int i = 0; i < 12; ++i)
    w.update(in);
  return w.player().x - x0;
}

// A button held for 3 ticks, then let go long enough for the menus to see
// it released.
void press(Game& g, bool Input::*button)
{
  Input in;
  in.*button = true;
  for (int i = 0; i < 3; ++i)
    g.tick(in);
  for (int i = 0; i < 25; ++i)
    g.tick({});
}

// The cheat code as a player enters it: runner select > NEW RUNNER > NAME,
// typed SNONUX > SAVE RUNNER. Then the level starts with him.
void enterCode(Renderer& renderer, const std::string& dir)
{
  GameOptions o;
  o.saveDir = dir;
  o.noCutscenes = true;
  o.levelPath = std::string(GR_DATA_DIR) + "/levels/01_rooftop_run.txt";
  Game g(o, renderer, nullptr);
  check(!snonuxUnlocked() && characterCount() == kDefaultRunners, "a new game: Snonux is locked");
  for (int i = 0; i < 30; ++i)
    g.tick({});
  press(g, &Input::left);    // wraps to the NEW RUNNER card
  press(g, &Input::confirm); // the editor, on NAME
  press(g, &Input::confirm); // the keyboard
  check(g.wantsText(), "the name keyboard is open");
  for (int i = 0; i < kMaxRunnerName + 2; ++i)
    g.textBackspace();
  g.typeText("snonux");
  g.textDone();
  for (int i = 0; i < 25; ++i)
    g.tick({});
  press(g, &Input::up);      // wraps to SAVE RUNNER
  press(g, &Input::confirm);
  check(snonuxUnlocked() && characterByIndex(kDefaultRunners).id == kSnonuxId, "naming a runner SNONUX unlocks him");
  check(characterCount() == kDefaultRunners + 1, "the code is not kept as a runner of its own");
  press(g, &Input::confirm); // the chosen card: Snonux
  for (int i = 0; i < 60 && !g.inLevel(); ++i)
    g.tick({});
  check(g.world() && g.world()->characterIndex() == kDefaultRunners, "the cursor waits on him: the level starts with Snonux");
}

} // namespace

int main()
{
  clearCustomRunners();
  setSnonuxUnlocked(false);
  check(characterCount() == kDefaultRunners && runnerIndexById(kSnonuxId) < 0, "locked: nowhere in the roster");

  check(isSnonuxCode("SNONUX") && isSnonuxCode("snonux") && isSnonuxCode(" Snonux "), "the code, in any case");
  check(!isSnonuxCode("SNONUX2") && !isSnonuxCode("SNO NUX") && !isSnonuxCode(""), "nothing else is the code");

  // A custom runner made while he is locked moves up one when he joins.
  const CharacterDef custom = makeCustomRunner("c0snotst", "MIKE", RunnerParts{}, Weapon::Laser, 3, 4, 3);
  check(putCustomRunner(custom) == kDefaultRunners, "locked: the first custom runner is 6");
  setSnonuxUnlocked(true);
  check(characterCount() == kDefaultRunners + 2, "unlocked: one more runner");
  const CharacterDef& sx = characterByIndex(kDefaultRunners);
  check(sx.id == kSnonuxId && sx.name == "SNONUX" && !sx.custom, "unlocked: Snonux is built-in runner 6");
  check(runnerIndexById("c0snotst") == kDefaultRunners + 1, "the custom runners follow him");
  check(builtInRunners() == kDefaultRunners + 1, "seven built-in runners");

  int bestHp = 0, bestJump = 0;
  for (int i = 0; i < kDefaultRunners; ++i)
  {
    const auto& d = characterByIndex(i);
    int jump = 0;
    for (int v : d.jumpArc)
      jump += v;
    bestHp = std::max(bestHp, d.maxHp);
    bestJump = std::max(bestJump, jump);
  }
  int jump = 0;
  for (int v : sx.jumpArc)
    jump += v;
  check(sx.maxHp >= bestHp && jump > bestJump, "the most hearts and the highest jump");
  check(sx.healthPips == kMaxPips && sx.jumpPips == kMaxPips && sx.powerPips == kMaxPips, "all stars");
  check(doubleJumpCells(sx.jumpPips) == doubleJumpCells(kMaxPips), "the biggest double jump");

  // Savegames name him; loading one where he is locked unlocks him.
  check(encodeRunner(sx) == kSnonuxId, "a savegame names him");
  setSnonuxUnlocked(false);
  check(savedRunner(kDefaultRunners, kSnonuxId).id == kSnonuxId, "the save list shows him while locked");
  check(resolveSavedRunner(kDefaultRunners, kSnonuxId) == kDefaultRunners && snonuxUnlocked(),
    "loading his save unlocks him");
  removeCustomRunner("c0snotst");

  // The profile keeps the code; an old profile has it off.
  {
    const std::string dir = (std::filesystem::temp_directory_path() / "gunrunners_snonux_test").string();
    std::filesystem::remove_all(dir);
    check(!Profile::load(dir).snonux, "a fresh profile: locked");
    Profile p;
    p.snonux = true;
    check(p.save(dir) && Profile::load(dir).snonux, "the profile remembers the code");
    std::filesystem::remove_all(dir);
  }

  SDL_Init(0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  {
    Renderer renderer(sdl);
    const Theme& theme = themeByIndex(0);
    const Art art = Art::build(theme, renderer);
    auto make = [&](int character) {
      return World(std::make_shared<const Level>(Level::parse(room())), character, theme, art);
    };

    World w = make(kDefaultRunners);
    const Player& p = w.player();
    check(p.hp == sx.maxHp && p.weapon == Weapon::Rocket && p.ammo == maxAmmo(Weapon::Rocket), "full rockets in hand");
    check(carries(p, Weapon::Laser, maxAmmo(Weapon::Laser)) && carries(p, Weapon::Flame, maxAmmo(Weapon::Flame)) &&
        carries(p, Weapon::Normal, 0) && p.spareCount == Player::kMaxGuns - 1,
      "the laser and the flamer full, and the blaster, as spares");
    for (int i = 0; i < 2000; ++i)
      w.update(PlayerInput{});
    check(w.player().rapidFire > 0, "rapid fire never runs out");
    check(w.snapshot().runner == kSnonuxId, "his savegame names him");
    check(bool(art.runner(sx).idle[0].facing[0]) && bool(art.portrait(sx)), "his art bakes");

    World dash = make(0), legend = make(kDefaultRunners);
    const int dWalk = distance(dash, false), sWalk = distance(legend, false);
    World dashRun = make(0), legendRun = make(kDefaultRunners);
    const int dRun = distance(dashRun, true), sRun = distance(legendRun, true);
    std::printf("walk %d vs %d, run %d vs %d cells\n", dWalk, sWalk, dRun, sRun);
    check(sWalk == dWalk && sRun > dRun, "walks like Dash (careful steps), runs faster");

    const std::string dir = (std::filesystem::temp_directory_path() / "gunrunners_snonux_code").string();
    std::filesystem::remove_all(dir);
    setSnonuxUnlocked(false);
    enterCode(renderer, dir);
    std::filesystem::remove_all(dir);
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  setSnonuxUnlocked(false);
  return gFailures == 0 ? 0 : 1;
}
