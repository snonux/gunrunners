// Regression checks for the 2026-10-14 audit (B01-B08) and the options it
// asked for. Small test rooms drive the real world logic with edge-correct
// input (a press triggers once, then stays held), the way the frontend
// latches a human's keys:
//   B01/B06 a theme change keeps the run, a bonus level's clock included;
//   B02     a vehicle saved mid-jump or mid-drift carries on the same after
//           loading;
//   B03     a goal reached on the clock's last frame wins;
//   B04     switching runners back and forth never adds hearts;
//   B05     getting back into a falling hoverbike or mech is no fresh jump;
//   B07     the Silent Picture and Overclock show real seconds;
//   B08     cheats on the way to a TV keep its bonus star off the profile;
// plus hold to fire, USE-only exits, jump help and low damage.

#include "assets/art.hpp"
#include "data/characters.hpp"
#include "data/level.hpp"
#include "data/theme.hpp"
#include "frontend/game.hpp"
#include "game/savegame.hpp"
#include "game/world.hpp"
#include "render/renderer.hpp"

#include <SDL.h>

#include <cstdio>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
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

// A walled room W blocks wide and 16 high, floor on row 14, the runner at
// block 2 of row 13. `edit` changes rows (y, row) before they are added.
std::string room(const std::string& header, const std::string& entities,
  const std::function<void(int, std::string&)>& edit = {}, int W = 64)
{
  const int H = 16;
  std::string s = "name=AUDIT TEST\ntheme=neon_rooftops\nmusic=theme_synthwave\n" + header + "[map]\n";
  for (int y = 0; y < H; ++y)
  {
    std::string row(std::size_t(W), '.');
    if (y == 0 || y >= 14)
      row.assign(std::size_t(W), '#');
    row[0] = row[std::size_t(W - 1)] = '#';
    if (y == 13)
    {
      row[2] = 'P';
      row[std::size_t(W - 4)] = 'X';
    }
    if (edit)
      edit(y, row);
    s += row + "\n";
  }
  s += "[entities]\n" + entities;
  return s;
}

std::shared_ptr<const Level> load(const std::string& text)
{
  return std::make_shared<const Level>(Level::parse(text));
}

struct Keys
{
  bool left = false, right = false, up = false, down = false, jump = false, fire = false, use = false;
};

// Edge-correct input: a button triggers on the first frame it is down.
struct Driver
{
  World& w;
  Keys prev{};
  void step(const Keys& in)
  {
    PlayerInput p;
    p.left = in.left;
    p.right = in.right;
    p.up = in.up;
    p.down = in.down;
    p.jump = {in.jump, in.jump && !prev.jump};
    p.fire = {in.fire, in.fire && !prev.fire};
    p.use = {in.use, in.use && !prev.use};
    prev = in;
    w.update(p);
  }
  void run(const Keys& in, int frames)
  {
    for (int i = 0; i < frames; ++i)
      step(in);
  }
};

int shotsIn(World& w)
{
  int n = 0;
  for (const auto s : w.takeSounds())
    n += s == Sfx::Shot;
  return n;
}

std::string vehicleState(const World& w)
{
  if (w.vehicles().empty())
    return "-";
  const Vehicle& v = w.vehicles()[0];
  char buf[160];
  std::snprintf(buf, sizeof(buf), "pos %d,%d v %d,%d a %d,%d air %d cool %d,%d occ %d | runner %d,%d", v.x, v.y, v.vx,
    v.vy, v.ax, v.ay, v.air, v.cool, v.cool2, int(v.occupied), w.player().x, w.player().y);
  return buf;
}

struct Kit
{
  const Theme& theme;
  const Art& art;
  std::string dir;
};

// --- B02: a vehicle saved mid-move carries on the same after loading. -----

void vehicleSaveMidMove(const Kit& kit, const std::string& name, const std::string& entity, const Keys& move,
  const Keys& jumpKeys)
{
  const auto level = load(room("", entity + "\n"));
  World w(level, 0, kit.theme, kit.art);
  Driver d{w};
  d.run({}, 3);
  Keys use;
  use.use = true;
  d.step(use);
  check(w.riding() != nullptr, name + ": boards");
  if (!w.riding())
    return;
  d.step({});
  d.run(jumpKeys, 4);
  const SaveGame s = w.snapshot();
  const std::string path = slotPath(kit.dir, 2);
  check(writeSave(s, path), name + ": saves mid-move");
  const auto loaded = readSave(path);
  if (!loaded)
    return;
  World fresh(level, loaded->character, kit.theme, kit.art);
  check(fresh.restore(*loaded), name + ": restores");
  Driver f{fresh};
  f.prev = d.prev;
  bool same = vehicleState(w) == vehicleState(fresh);
  std::string at = same ? "" : " at load: " + vehicleState(w) + " vs " + vehicleState(fresh);
  for (int i = 0; i < 15 && same; ++i)
  {
    const Keys in = i < 5 ? jumpKeys : move;
    d.step(in);
    f.step(in);
    same = vehicleState(w) == vehicleState(fresh); // shots in flight are not saved; the guns' cooldowns are
    if (!same)
      at = " after " + std::to_string(i + 1) + " frames: " + vehicleState(w) + " vs " + vehicleState(fresh);
  }
  check(same, name + ": moves the same after loading as without" + at);
}

// --- B05: back into a falling ride is no fresh jump. ----------------------

void reboardInAir(const Kit& kit, const std::string& name, const std::string& entity)
{
  const auto level = load(room("", entity + "\n"));
  World w(level, 0, kit.theme, kit.art);
  Driver d{w};
  d.run({}, 3);
  Keys use;
  use.use = true;
  d.step(use);
  d.step({});
  Keys jump;
  jump.jump = true;
  d.run(jump, 3);
  d.step({});
  const Vehicle& v = w.vehicles()[0];
  check(!w.map().onSolidGround(v.box()), name + ": in the air after the jump");
  d.step(use); // out
  check(!w.riding(), name + ": USE climbs out in the air");
  d.step({});
  d.step(use); // back in, still in the air
  const bool back = w.riding() != nullptr;
  const int y0 = w.vehicles()[0].y;
  const bool airborne = !w.map().onSolidGround(w.vehicles()[0].box());
  d.step(jump);
  const int y1 = w.vehicles()[0].y;
  if (back && airborne)
    check(y1 >= y0, name + ": jump in mid-air after getting back in does not rise (y " + std::to_string(y0) + " -> " +
      std::to_string(y1) + ")");
  else
    check(true, name + ": landed before getting back in (nothing to check)");
  // On the ground it still jumps.
  d.run({}, 30);
  const int g = w.vehicles()[0].y;
  d.step(jump);
  d.step(jump);
  check(w.vehicles()[0].y < g, name + ": jumps from the ground");
}

} // namespace

int main()
{
  const std::string dir = (std::filesystem::temp_directory_path() / "gunrunners_audit_test").string();
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  const std::string levels = std::string(GR_DATA_DIR) + "/levels/";

  SDL_Init(0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  {
    Renderer renderer(sdl);
    const Theme& theme = themeByIndex(0);
    const Art art = Art::build(theme, renderer);
    const Kit kit{theme, art, dir};

    // --- B02 ---
    {
      Keys move;
      move.right = true;
      move.fire = true;
      Keys jumpRight = move;
      jumpRight.jump = true;
      vehicleSaveMidMove(kit, "B02 hoverbike", "@ bike 3 13", move, jumpRight);
      vehicleSaveMidMove(kit, "B02 mech", "@ vehicle kind=mech x=3 y=13", move, jumpRight);
      vehicleSaveMidMove(kit, "B02 bounder", "@ vehicle kind=bounder x=3 y=13", move, jumpRight);
      Keys upRight = move;
      upRight.up = true;
      vehicleSaveMidMove(kit, "B02 space ship", "@ vehicle kind=spaceship x=3 y=13", move, upRight);
      vehicleSaveMidMove(kit, "B02 helicopter", "@ vehicle kind=helicopter x=3 y=13", move, upRight);
      vehicleSaveMidMove(kit, "B02 tank", "@ tank 3 13", move, move);
    }

    // --- B05 ---
    reboardInAir(kit, "B05 hoverbike", "@ bike 3 13");
    reboardInAir(kit, "B05 mech", "@ vehicle kind=mech x=3 y=13");

    // --- B03: the last gem on the clock's final frame wins. ---
    {
      const auto level = load(room("rules=test\ntimer=1\ngoal=collect:1\n", "",
        [](int y, std::string& row) {
          if (y == 13)
            row[5] = 'g';
        }));
      // Find the wait that has the pickup land on the clock's last frame.
      bool found = false;
      for (int wait = 0; wait < 15 && !found; ++wait)
      {
        World w(level, 0, theme, art);
        Driver d{w};
        d.run({}, wait);
        Keys right;
        right.right = true;
        for (int i = 0; i < 15 && w.stats().gems == 0 && w.state() == WorldState::Playing; ++i)
          d.step(right);
        if (w.stats().gems != 1 || w.state() != WorldState::Playing || w.bonusFramesLeft() != 1)
          continue;
        found = true;
        d.step({});
        check(w.state() == WorldState::Exiting && !w.bonusFailed(),
          "B03: a gem taken on the last frame of the clock wins (state " + std::to_string(int(w.state())) +
            ", failed " + std::to_string(int(w.bonusFailed())) + ")");
      }
      check(found, "B03: the pickup can land on the clock's last frame");
      // And the clock still runs out when the goal is not met.
      World w(level, 0, theme, art);
      Driver d{w};
      d.run({}, 15);
      check(w.state() == WorldState::Done && w.bonusFailed(), "B03: the clock still runs out without the gem");
    }

    // --- B04: runner switches never add hearts. ---
    {
      const auto level = load(room("", ""));
      int worst = 0;
      std::string worstAt;
      for (int a = 0; a < characterCount(); ++a)
        for (int b = 0; b < characterCount(); ++b)
        {
          if (a == b)
            continue;
          for (int hp = 1; hp <= characterByIndex(a).maxHp; ++hp)
          {
            World w(level, a, theme, art);
            SaveGame s = w.snapshot();
            s.hp = hp;
            World x(level, a, theme, art);
            x.restore(s);
            for (int k = 0; k < 12; ++k)
            {
              x.switchCharacter(b);
              x.switchCharacter(a);
            }
            const int gain = x.player().hp - hp;
            if (gain > worst)
            {
              worst = gain;
              worstAt = std::to_string(a) + "<->" + std::to_string(b) + " from " + std::to_string(hp);
            }
          }
        }
      check(worst == 0, "B04: twelve switches back and forth add no hearts" + (worstAt.empty() ? "" : " (" + worstAt + ")"));

      // Three runners round and round, and a save in between.
      World w(level, 0, theme, art);
      SaveGame s = w.snapshot();
      s.hp = 1;
      World x(level, 0, theme, art);
      x.restore(s);
      for (int k = 0; k < 10; ++k)
      {
        x.switchCharacter(1);
        x.switchCharacter(2);
        x.switchCharacter(0);
      }
      check(x.player().hp == 1, "B04: round three runners, back to 1 heart");
      x.switchCharacter(1);
      const std::string path = slotPath(dir, 3);
      writeSave(x.snapshot(), path);
      const auto back = readSave(path);
      World y(level, back ? back->character : 1, theme, art);
      if (back)
        y.restore(*back);
      y.switchCharacter(0);
      check(y.player().hp == 1, "B04: a save between switches keeps the count (" + std::to_string(y.player().hp) + ")");
      check(y.player().hp >= 1, "B04: a switch at one heart is safe");
    }

    // --- B07: real seconds on the countdown. ---
    {
      for (const char* file : {"27_bonus_silent_picture.txt", "33_bonus_overclock.txt", "01_bonus_cloud_nine.txt"})
      {
        const auto level = std::make_shared<const Level>(Level::loadFile(levels + file));
        World w(level, 0, theme, art);
        check(w.bonusSecondsLeft() == level->timer && !w.bonusHurry(),
          std::string("B07: ") + file + " shows its " + std::to_string(level->timer) + " seconds (" +
            std::to_string(w.bonusSecondsLeft()) + ")");
      }
      // The hurry warning starts ten real seconds before the end.
      const auto level = load(room("rules=test\ntimer=11\ngoal=exit\n", ""));
      World w(level, 0, theme, art);
      Driver d{w};
      d.run({}, 15);
      const bool before = w.bonusHurry();
      d.step({});
      check(!before && w.bonusHurry() && w.bonusSecondsLeft() == 10, "B07: the warning starts at ten seconds");
    }

    // --- Options: hold to fire, USE-only exits, jump help, low damage. ---
    {
      const auto level = load(room("", ""));
      for (int hold = 0; hold < 2; ++hold)
      {
        World w(level, 0, theme, art);
        World::PlayOptions o;
        o.holdFire = hold != 0;
        w.setPlayOptions(o);
        Driver d{w};
        d.run({}, 2);
        w.takeSounds();
        Keys fire;
        fire.fire = true;
        int shots = 0;
        for (int i = 0; i < 15; ++i)
        {
          d.step(fire);
          shots += shotsIn(w);
        }
        if (hold)
          check(shots == 5, "hold to fire: five shots a second (" + std::to_string(shots) + ")");
        else
          check(shots == 1, "tap to fire by default: one shot per press (" + std::to_string(shots) + ")");
      }
      // Rapid fire stays faster than hold to fire.
      {
        World w(level, 4, theme, art); // Skye starts with rapid fire
        World::PlayOptions o;
        o.holdFire = true;
        w.setPlayOptions(o);
        Driver d{w};
        d.run({}, 2);
        w.takeSounds();
        Keys fire;
        fire.fire = true;
        int shots = 0;
        for (int i = 0; i < 15; ++i)
        {
          d.step(fire);
          shots += shotsIn(w);
        }
        check(shots > 5, "rapid fire beats hold to fire (" + std::to_string(shots) + " a second)");
      }

      // Down + jump in a vehicle: only with the option.
      const auto bike = load(room("", "@ bike 3 13\n"));
      for (int opt = 0; opt < 2; ++opt)
      {
        World w(bike, 0, theme, art);
        World::PlayOptions o;
        o.downJumpLeaves = opt != 0;
        w.setPlayOptions(o);
        Driver d{w};
        d.run({}, 3);
        Keys use;
        use.use = true;
        d.step(use);
        d.step({});
        Keys downJump;
        downJump.down = true;
        downJump.jump = true;
        d.step(downJump);
        if (opt)
          check(!w.riding(), "down + jump climbs out with the option on");
        else
          check(w.riding() != nullptr, "down + jump stays aboard by default (USE climbs out)");
      }

      // Jump help: a press just before landing still jumps.
      const auto drop = load(room("", "", [](int y, std::string& row) {
        if (y == 13)
          row[2] = '.';
        if (y == 5)
          row[2] = 'P';
      }));
      for (int opt = 0; opt < 2; ++opt)
      {
        World w(drop, 0, theme, art);
        World::PlayOptions o;
        o.jumpHelp = opt != 0;
        w.setPlayOptions(o);
        Driver d{w};
        // Fall until a frame before landing (found on a copy).
        int fallFrames = 0;
        int ground = 0;
        {
          World probe(drop, 0, theme, art);
          Driver pd{probe};
          do
          {
            pd.step({});
            ++fallFrames;
          } while (probe.player().state != PlayerState::OnGround && fallFrames < 60);
          ground = probe.player().y;
        }
        d.run({}, fallFrames - 2);
        Keys jump;
        jump.jump = true;
        d.step(jump); // pressed in the air, just before landing
        d.step({});   // let go: the hard landing's recovery frame
        d.run({}, 2);
        const bool jumped = w.player().state == PlayerState::Jumping || w.player().y < ground - 1;
        if (opt)
          check(jumped, "jump help: a press just before landing jumps");
        else
          check(!jumped, "without jump help, a press released before landing is lost");
      }

      // Jump help on a hoverbike: a press just before it lands jumps again.
      for (int opt = 0; opt < 2; ++opt)
      {
        auto rideAndJump = [&](World& w, Driver& d) {
          d.run({}, 3);
          Keys use;
          use.use = true;
          d.step(use);
          d.step({});
          Keys jump;
          jump.jump = true;
          d.step(jump);
          d.step({});
          (void)w;
        };
        // How long a jump takes, on a copy.
        int airFrames = 0;
        {
          World probe(bike, 0, theme, art);
          Driver pd{probe};
          rideAndJump(probe, pd);
          while (probe.vehicles()[0].air >= 0 && airFrames < 60)
          {
            pd.step({});
            ++airFrames;
          }
        }
        World w(bike, 0, theme, art);
        World::PlayOptions o;
        o.jumpHelp = opt != 0;
        w.setPlayOptions(o);
        Driver d{w};
        rideAndJump(w, d);
        d.run({}, airFrames - 2);
        Keys jump;
        jump.jump = true;
        d.step(jump); // just before landing
        d.step({});
        d.step({});
        const bool again = w.vehicles()[0].air >= 0;
        if (opt)
          check(again, "jump help: the hoverbike jumps again off a press just before landing");
        else
          check(!again, "without jump help, the hoverbike ignores a press just before landing");
      }

      // Low damage: half the hearts.
      const auto spikes = load(room("", "", [](int y, std::string& row) {
        if (y == 13)
          for (int x = 4; x < 12; ++x)
            row[std::size_t(x)] = '^';
      }));
      int lost[2] = {0, 0};
      for (int opt = 0; opt < 2; ++opt)
      {
        World w(spikes, 1, theme, art); // Rocco: plenty of hearts
        World::PlayOptions o;
        o.lowDamage = opt != 0;
        w.setPlayOptions(o);
        Driver d{w};
        Keys right;
        right.right = true;
        d.run(right, 4);
        d.run({}, 90);
        lost[opt] = w.player().maxHp - w.player().hp;
      }
      check(lost[0] >= 2 && lost[1] == lost[0] / 2,
        "low damage takes half the hearts (" + std::to_string(lost[1]) + " of " + std::to_string(lost[0]) + ")");
    }
  }

  // --- B01, B06, B08: through the whole game. ---
  {
    Renderer renderer(sdl);
    // B01: Pinball can't be saved; a theme change used to drop the run.
    {
      GameOptions o;
      o.saveDir = dir + "/b01";
      o.levelPath = levels + "11_bonus_pinball.txt";
      o.skipMenu = true;
      o.noCutscenes = true;
      Game g(o, renderer, nullptr);
      for (int i = 0; i < 32; ++i)
        g.tick({});
      const World* before = g.world();
      const bool inBefore = g.inLevel();
      g.cycleTheme();
      g.tick({});
      check(inBefore && g.inLevel() && g.world() == before, "B01: a theme change keeps a Pinball run going");
    }
    // B06: a stand-alone bonus level keeps its clock.
    {
      GameOptions o;
      o.saveDir = dir + "/b06";
      o.levelPath = levels + "01_bonus_cloud_nine.txt";
      o.skipMenu = true;
      o.noCutscenes = true;
      Game g(o, renderer, nullptr);
      for (int i = 0; i < 400; ++i)
        g.tick({});
      const int left = g.world() ? g.world()->bonusFramesLeft() : -1;
      g.cycleTheme();
      g.cycleTheme();
      const int after = g.world() ? g.world()->bonusFramesLeft() : -2;
      check(left > 0 && left < 60 * 15 && after == left,
        "B06: a theme change keeps the bonus clock (" + std::to_string(left) + " -> " + std::to_string(after) + ")");
    }
    // B08: cheats in Level 1, then its TV: the bonus level counts as cheated.
    {
      GameOptions o;
      o.saveDir = dir + "/b08";
      o.skipMenu = true;
      o.startLevel = 1;
      o.noCutscenes = true;
      Game g(o, renderer, nullptr);
      for (int i = 0; i < 60 && !g.inLevel(); ++i)
        g.tick({});
      const World* main = g.world();
      const Prop* tv = nullptr;
      if (main)
        for (const auto& pr : main->props())
          if (pr.kind == PropKind::BonusDoor)
            tv = &pr;
      check(main && tv, "B08: Level 1 has its TV");
      if (main && tv)
      {
        SaveGame s = main->snapshot();
        s.x = tv->x;
        s.y = tv->y + tv->h - 1;
        s.cheated = true;
        writeSave(s, quickSavePath(o.saveDir));
        Input load;
        load.quickLoad = true;
        g.tick(load);
        g.tick({});
        const std::string mainName = g.world() ? g.world()->level().name : "";
        Input up;
        up.up = true;
        for (int i = 0; i < 40 && g.world() && g.world()->level().name == mainName; ++i)
          g.tick(up);
        for (int i = 0; i < 4; ++i)
          g.tick({});
        const bool inBonus = g.world() && g.world()->level().name != mainName;
        check(inBonus, "B08: the TV leads into the bonus level");
        check(inBonus && g.world()->stats().cheated, "B08: the bonus level knows cheats were used on the way");
      }
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf(gFailures ? "%d FAILED\n" : "all passed\n", gFailures);
  return gFailures ? 1 : 0;
}
