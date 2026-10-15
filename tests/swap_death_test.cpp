// SWAP RUNNER ON DEATH (an ASSISTS AND OPTIONS row): a runner who dies sits
// out the rest of the level and the whole next one, and the next free runner
// takes over at the checkpoint. Checks, in a test room with a pit:
//   - with the option off a death brings the same runner back;
//   - with it on the next runner steps in with full hearts, the fallen one is
//     benched for two levels, and C / CHANGE RUNNER can't pick a benched one;
//   - once every other runner is out, the last one just comes back;
//   - the bench survives a save and a load;
// and through the whole game, that the bench carries into the next campaign
// level one level shorter, and that the next level starts with a free runner.

#include "assets/art.hpp"
#include "data/characters.hpp"
#include "data/level.hpp"
#include "data/theme.hpp"
#include "frontend/game.hpp"
#include "game/profile.hpp"
#include "game/savegame.hpp"
#include "game/world.hpp"
#include "render/renderer.hpp"

#include <SDL.h>

#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <unistd.h>
#include <vector>

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

// A walled room 40 blocks wide and 16 high, floor on row 14 with a bottomless
// pit at blocks 8-11, the runner at block 2 of row 13.
std::string pitRoom()
{
  const int W = 40, H = 16;
  std::string s = "name=SWAP TEST\ntheme=neon_rooftops\nmusic=theme_synthwave\n[map]\n";
  for (int y = 0; y < H; ++y)
  {
    std::string row(std::size_t(W), '.');
    if (y == 0 || y >= 14)
      row.assign(std::size_t(W), '#');
    row[0] = row[std::size_t(W - 1)] = '#';
    if (y >= 14)
      for (int x = 8; x <= 11; ++x)
        row[std::size_t(x)] = '.';
    if (y == 13)
    {
      row[2] = 'P';
      row[std::size_t(W - 4)] = 'X';
    }
    s += row + "\n";
  }
  return s + "[entities]\n";
}

// Walks right into the pit and waits out the death scene.
void dieInPit(World& w)
{
  PlayerInput right;
  right.right = true;
  for (int i = 0; i < 200 && w.player().state != PlayerState::Dying; ++i)
    w.update(right);
  for (int i = 0; i < 200 && w.player().state == PlayerState::Dying; ++i)
    w.update(PlayerInput{});
}

int benchOf(const World& w, int i)
{
  return std::size_t(i) < w.bench().size() ? w.bench()[std::size_t(i)] : 0;
}

} // namespace

int main()
{
  const std::string dir = (std::filesystem::temp_directory_path() /
    ("gunrunners_swap_death_test_" + std::to_string(::getpid()))).string();
  std::filesystem::remove_all(dir);

  SDL_Init(0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  {
    Renderer renderer(sdl);
    const Theme& theme = themeByIndex(0);
    const Art art = Art::build(theme, renderer);
    const auto level = std::make_shared<const Level>(Level::parse(pitRoom()));
    World::PlayOptions swap;
    swap.swapOnDeath = true;

    // Off (the default): the same runner comes back.
    {
      World w(level, 0, theme, art);
      dieInPit(w);
      check(w.stats().deaths == 1, "the pit kills");
      check(w.characterIndex() == 0 && w.bench().empty(), "option off: the same runner comes back, nobody benched");
    }

    // On: the next runner steps in, and the fallen one sits out.
    {
      World w(level, 0, theme, art);
      w.setPlayOptions(swap);
      dieInPit(w);
      check(w.characterIndex() == 1, "option on: Rocco steps in for Dash (" + std::to_string(w.characterIndex()) + ")");
      check(w.player().hp == characterByIndex(1).maxHp && w.player().maxHp == characterByIndex(1).maxHp,
        "the new runner has full hearts");
      check(w.player().state != PlayerState::Dying && w.player().x < 8 * kCellsPerTile, "back at the checkpoint");
      check(benchOf(w, 0) == 2 && benchOf(w, 1) == 0, "Dash sits out this level and the next");
      check(!w.switchCharacter(0) && w.characterIndex() == 1, "a benched runner can't be switched to");
      check(w.nextFreeRunner(1) == 2, "the C key skips past the benched runner");
      check(w.switchCharacter(3) && w.characterIndex() == 3, "a free runner still can be");
      check(w.nextFreeRunner(5) == 1, "C wraps round past Dash to Rocco");

      // The bench survives a save and a load.
      std::filesystem::create_directories(dir);
      const std::string path = slotPath(dir, 0);
      check(writeSave(w.snapshot(), path), "saved");
      const auto back = readSave(path);
      World x(level, back ? back->character : 0, theme, art);
      check(back && x.restore(*back), "loaded");
      check(benchOf(x, 0) == 2 && x.characterIndex() == 3, "the bench is in the savegame");

      // Every other runner falls in turn; the last one just comes back.
      int deaths = 0;
      while (w.nextFreeRunner(w.characterIndex()) >= 0 && deaths < 40)
      {
        dieInPit(w);
        ++deaths;
      }
      const int last = w.characterIndex();
      int out = 0;
      for (int i = 0; i < characterCount(); ++i)
        out += benchOf(w, i) > 0;
      check(out == characterCount() - 1 && !w.benched(last), "all but one runner sit out");
      dieInPit(w);
      check(w.characterIndex() == last && w.player().state != PlayerState::Dying,
        "with nobody left to swap in, the last runner respawns as usual");
    }

    // Through the game: Level 1 cleared with Dash benched, Rocco playing.
    {
      GameOptions o;
      o.saveDir = dir + "/game";
      o.skipMenu = true;
      o.startLevel = 1;
      o.noCutscenes = true;
      o.character = 0;
      {
        Profile p;
        p.controls["opt.swapdeath"] = {1};
        p.save(o.saveDir);
      }
      Game g(o, renderer, nullptr);
      for (int i = 0; i < 60 && !g.inLevel(); ++i)
        g.tick({});
      World* w = const_cast<World*>(g.world());
      check(w && w->playOptions().swapOnDeath, "the profile switches the option on");
      if (w)
      {
        // Dash fell (as in the room above), Rocco took over and stands at the exit.
        const std::string name1 = w->level().name;
        SaveGame s = w->snapshot();
        s.character = 1;
        s.hp = characterByIndex(1).maxHp;
        s.bench = {2, 0, 0, 0, 0, 0};
        s.x = w->level().exitTx * kCellsPerTile;
        s.y = (w->level().exitTy + 1) * kCellsPerTile - 1;
        check(w->restore(s), "Rocco put at Level 1's exit");
        for (int i = 0; i < 3000 && g.world() && g.world()->level().name == name1; ++i)
          g.tick({});
        for (int i = 0; i < 60 && !g.inLevel(); ++i)
          g.tick({});
        const World* w2 = g.world();
        check(w2 && w2->level().name != name1, "on to Level 2");
        if (w2)
        {
          check(w2->characterIndex() == 1, "Level 2 starts with Rocco, not the benched Dash (" +
            std::to_string(w2->characterIndex()) + ")");
          check(benchOf(*w2, 0) == 1, "Dash still sits out Level 2");
          // Level 2 cleared: Dash is back for Level 3.
          World* m = const_cast<World*>(w2);
          const std::string name2 = m->level().name;
          SaveGame s2 = m->snapshot();
          s2.x = m->level().exitTx * kCellsPerTile;
          s2.y = (m->level().exitTy + 1) * kCellsPerTile - 1;
          check(m->restore(s2), "Rocco put at Level 2's exit");
          for (int i = 0; i < 3000 && g.world() && g.world()->level().name == name2; ++i)
            g.tick({});
          for (int i = 0; i < 60 && !g.inLevel(); ++i)
            g.tick({});
          World* w3 = const_cast<World*>(g.world());
          check(w3 && w3->level().name != name2, "on to Level 3");
          if (w3)
          {
            check(!w3->benched(0), "Dash is free again in Level 3");
            check(w3->switchCharacter(0), "and can be switched to");
          }
        }
      }
    }
  }
  std::filesystem::remove_all(dir);
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf(gFailures ? "%d FAILED\n" : "all passed\n", gFailures);
  return gFailures ? 1 : 0;
}
