// Level 36's mechanics, played by hand in small rooms: a lever sends a
// magnet crane off with a crate, which never drops onto the runner and sits
// on a crate below it; the Magnet Gun's pull and push; a crate shoved into
// the foam pit coming back; a ride crane; a Forklift Bot lifting the runner
// (and shoving Rocco instead); a Stagehand's flat stopping shots; a Prop
// Mimic's lunge; the TURBODUDES crate; Throw Everything's lift and throw;
// and a save with a crane moved. Then the bot clears the Prop Warehouse and
// Throw Everything with every runner.

#include "assets/art.hpp"
#include "data/level.hpp"
#include "data/theme.hpp"
#include "frontend/bot.hpp"
#include "game/world.hpp"
#include "render/renderer.hpp"

#include <SDL.h>

#include <cstdio>
#include <cstdlib>
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

struct Wall
{
  int x0, y0, x1, y1;
  char c = '#';
};

// A 40 x h room with walls, a floor on rows h-2 and h-1, the runner at
// (px, py), plus `walls` (a '.' wall clears) and `entities`.
std::string room(int px, int py, const std::string& entities, const std::vector<Wall>& walls = {}, int h = 24,
  const std::string& extra = "weapon=magnet_gun\n")
{
  std::vector<std::string> map(std::size_t(h), std::string(40, '.'));
  for (int y = 0; y < h; ++y)
    map[std::size_t(y)][0] = map[std::size_t(y)][1] = map[std::size_t(y)][38] = map[std::size_t(y)][39] = '#';
  for (int x = 0; x < 40; ++x)
    map[std::size_t(h - 2)][std::size_t(x)] = map[std::size_t(h - 1)][std::size_t(x)] = '#';
  for (const auto& w : walls)
    for (int y = w.y0; y <= w.y1; ++y)
      for (int x = w.x0; x <= w.x1; ++x)
        map[std::size_t(y)][std::size_t(x)] = w.c;
  map[std::size_t(py)][std::size_t(px)] = 'P';
  std::string s = "name=WAREHOUSE TEST\nepisode=6\ntheme=prop_warehouse\n" + extra + "[map]\n";
  for (const auto& row : map)
    s += row + "\n";
  s += "[entities]\n" + entities;
  return s;
}

struct Keys
{
  bool left = false, right = false, down = false, jump = false, fire = false, up = false;
};

void step(World& w, Keys pad, Keys& prev)
{
  PlayerInput p;
  p.left = pad.left;
  p.right = pad.right;
  p.up = pad.up;
  p.down = pad.down;
  p.jump = {pad.jump, pad.jump && !prev.jump};
  p.fire = {pad.fire, pad.fire && !prev.fire};
  w.update(p);
  prev = pad;
}

void idle(World& w, int frames)
{
  Keys prev;
  for (int f = 0; f < frames; ++f)
    step(w, Keys{}, prev);
}

void hold(World& w, Keys pad, int frames, Keys& prev)
{
  for (int f = 0; f < frames; ++f)
    step(w, pad, prev);
}

int botPlay(World& w, int frames, Bot& bot)
{
  Input prev;
  int f = 0;
  for (; f < frames && w.state() == WorldState::Playing; ++f)
  {
    const Input in = bot.play(w);
    PlayerInput p;
    p.left = in.left;
    p.right = in.right;
    p.up = in.up;
    p.down = in.down;
    p.jump = {in.jump, in.jump && !prev.jump};
    p.fire = {in.fire, in.fire && !prev.fire};
    prev = in;
    w.update(p);
  }
  return f;
}

const Enemy* firstOf(const World& w, EnemyKind kind)
{
  for (const auto& e : w.enemies())
    if (e.alive && e.kind == kind)
      return &e;
  return nullptr;
}

const MetalCrate& crate(const World& w, const char* id)
{
  for (const auto& c : w.warehouse().crates)
    if (c.id == id)
      return c;
  return w.warehouse().crates.front();
}

} // namespace

int main(int argc, char** argv)
{
  if (argc < 3)
    return 2;
  SDL_Init(0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  {
    Renderer renderer(sdl);
    const Theme& theme = themeByIndex(0);
    const Art art = Art::build(theme, renderer);
    Keys none, right, left, up, fire;
    right.right = true;
    left.left = true;
    up.up = true;
    fire.fire = true;
    auto make = [&](const std::string& text, int character = 0) {
      return World(std::make_shared<const Level>(Level::parse(text)), character, theme, art);
    };
    const std::string crane = "@ crate MC1 10 20 metal=1\n@ crane cr1 x0=6 x1=31 row=6 start=10 lever=L1\n";

    {
      // A lever sends the crane off: it lifts the crate below it, carries it
      // to the far end of its rail and drops it there, on whole blocks.
      World w = make(room(4, 21, crane + "@ switch L1 4 21 kind=lever\n"));
      Keys prev;
      step(w, up, prev);
      step(w, none, prev);
      idle(w, 300);
      const auto& c = crate(w, "MC1");
      const auto& cr = w.warehouse().cranes[0];
      std::printf("     crate at %d,%d\n", c.x, c.y);
      check(c.state == CrateState::Rest && c.x == 60 && c.y == 40 && cr.cranesMoved == 1 && cr.phase == CranePhase::Idle,
        "a lever sends the crane off with the crate and it drops it at the far end");
      check(w.map().block(30, 20) == Tile::Solid && w.map().block(10, 20) == Tile::Empty, "the crate is solid where it rests");
      // ...onto a crate a block beside where it lands: it sits on top of it.
      World v = make(room(4, 21, crane + "@ switch L1 4 21 kind=lever\n@ crate MC2 31 20 metal=1\n"));
      step(v, up, prev);
      step(v, none, prev);
      idle(v, 300);
      const auto& d = crate(v, "MC1");
      check(d.state == CrateState::Rest && d.x == 62 && d.y == 36, "stack assist: a dropped crate sits on the crate beside it");
    }
    {
      // A crate never drops onto the runner: it waits, blinking, until they
      // step out from under it.
      World w = make(room(30, 21, crane + "@ switch L1 30 21 kind=lever\n"));
      Keys prev;
      step(w, up, prev);
      step(w, none, prev);
      const int hp = w.player().hp;
      idle(w, 300);
      const bool waited = crate(w, "MC1").state == CrateState::Held && w.player().hp == hp;
      hold(w, left, 20, prev);
      idle(w, 40);
      check(waited && crate(w, "MC1").state == CrateState::Rest, "a crate waits over the runner and drops once they move");
    }
    {
      // The Magnet Gun: held, it pulls a crate in; tapped, it shoves it off.
      World w = make(room(4, 21, "@ crate MC1 14 20 metal=1\n"));
      w.cheat(Cheat::Ammo);
      Keys prev;
      hold(w, fire, 30, prev);
      const int pulled = crate(w, "MC1").x;
      step(w, none, prev);
      idle(w, 20);
      std::printf("     pulled to %d\n", pulled);
      check(pulled < 28 && pulled >= 11, "held, the Magnet Gun pulls a crate toward the runner");
      const int x0 = crate(w, "MC1").x;
      idle(w, 10);
      step(w, fire, prev);
      step(w, none, prev);
      idle(w, 40);
      const auto& c = crate(w, "MC1");
      std::printf("     pushed from %d to %d\n", x0, c.x);
      check(c.state == CrateState::Rest && c.x >= x0 + 10 && c.x % kCellsPerTile == 0, "tapped, it shoves the crate six blocks off");
    }
    {
      // A crate shoved off a ledge into the foam pit comes back to its start.
      World w = make(room(3, 21, "@ crate MC1 7 20 metal=1\n@ foam rect=12,26,37,27\n", {{2, 22, 11, 27}}, 30));
      w.cheat(Cheat::Ammo);
      Keys prev;
      step(w, fire, prev);
      step(w, none, prev);
      bool gone = false;
      for (int f = 0; f < 60 && !gone; ++f)
      {
        idle(w, 1);
        gone = crate(w, "MC1").state == CrateState::Gone;
      }
      idle(w, kCrateRespawn + 5);
      const auto& c = crate(w, "MC1");
      check(gone && c.state == CrateState::Rest && c.x == 14 && c.y == 40, "a crate in the foam pit comes back to its start");
    }
    {
      // A ride crane: once its lever is thrown, stepping onto its crate
      // rings the bell and it carries the runner over the pit.
      World w = make(room(9, 21,
        "@ switch L1 9 21 kind=lever\n@ crane cr1 x0=10 x1=20 row=8 start=10 end=16 deck=22 ride=1 lever=L1\n",
        {{2, 22, 9, 27}, {20, 22, 37, 27}}, 30));
      Keys prev;
      step(w, up, prev);
      step(w, none, prev);
      hold(w, right, 3, prev);
      idle(w, 120);
      const auto& cr = w.warehouse().cranes[0];
      const auto& pl = w.platforms()[std::size_t(cr.platform)];
      check(cr.phase == CranePhase::RideHold && pl.x == 32 && w.player().state == PlayerState::OnGround &&
          w.player().x >= 32 && w.player().x < 36,
        "a ride crane carries the runner over the pit");
    }
    for (int character : {0, 1})
    {
      // A Forklift Bot charges, lifts the runner on its forks and carries
      // them off; Rocco is too heavy and is shoved back instead.
      World w = make(room(10, 21, "@ forklift 20 21 dir=l lane=3,36\n"), character);
      const int hp = w.player().hp, x0 = w.player().x;
      bool grabbed = false;
      for (int f = 0; f < 120 && !grabbed && w.player().hp == hp; ++f)
      {
        idle(w, 1);
        grabbed = w.warehouse().grabbedBy >= 0;
      }
      std::printf("     runner %d: grabbed %d, hp %d of %d, x %d from %d\n", character, grabbed, w.player().hp, hp,
        w.player().x, x0);
      if (character == 1)
        check(!grabbed && w.player().hp == hp - 1 && w.player().x < x0, "a Forklift Bot shoves Rocco back");
      else
      {
        bool moved = false;
        for (int f = 0; f < 60 && w.warehouse().grabbedBy >= 0; ++f)
        {
          idle(w, 1);
          moved = moved || w.player().x < x0 - 6;
        }
        check(grabbed && moved && w.warehouse().grabbedBy < 0 && w.player().state != PlayerState::Dying,
          "a Forklift Bot lifts the runner and carries them off");
      }
    }
    {
      // A Stagehand sets its flat down in front of the runner; it stops shots.
      World w = make(room(4, 21, "@ stagehand 18 21\n", {}, 24, ""));
      w.cheat(Cheat::God);
      bool down = false;
      for (int f = 0; f < 200 && !down; ++f)
      {
        idle(w, 1);
        down = w.warehouse().flats[0].down;
      }
      Keys prev;
      for (int k = 0; k < 6; ++k)
      {
        step(w, fire, prev);
        hold(w, none, 8, prev);
      }
      const auto& fl = w.warehouse().flats[0];
      std::printf("     flat down %d at %d, hp %d\n", down, fl.x, fl.hp);
      const Enemy* e = firstOf(w, EnemyKind::Stagehand);
      check(down && fl.hp < kFlatHp && e && e->hp == enemyDef(e->def).hp, "a Stagehand's flat stops the runner's shots");
    }
    {
      // A Prop Mimic lunges at a runner beside it.
      World w = make(room(8, 21, "@ mimic 11 21\n", {}, 24, ""));
      const int x0 = firstOf(w, EnemyKind::Mimic)->x;
      int nearest = x0;
      for (int f = 0; f < 40; ++f)
      {
        idle(w, 1);
        if (const Enemy* e = firstOf(w, EnemyKind::Mimic))
          nearest = std::min(nearest, e->x);
      }
      check(nearest <= x0 - 3, "a Prop Mimic lunges at the runner");
    }
    {
      // The TURBODUDES crate: two shots show the old title.
      World w = make(room(4, 21, "@ turbocrate 10 21\n", {}, 24, ""));
      Keys prev;
      for (int k = 0; k < 2; ++k)
      {
        step(w, fire, prev);
        hold(w, none, 12, prev);
      }
      check(w.warehouse().tdHits == 2 && w.warehouse().tdShow > 0, "two shots at the TURBODUDES crate show the old title");
    }
    {
      // Throw Everything: fire beside a loose block lifts it; fire again
      // throws it six blocks on, where it lands as a solid block.
      World w = make(room(4, 21, "@ loose rect=6,21,6,21\n", {}, 24, "rules=throw_all\n"));
      Keys prev;
      hold(w, right, 1, prev);
      hold(w, none, 2, prev);
      const bool solid = w.map().block(6, 21) == Tile::Solid;
      step(w, fire, prev);
      step(w, none, prev);
      const bool lifted = w.warehouse().carry == 0 && w.map().block(6, 21) == Tile::Empty;
      idle(w, kLiftFrames + 2);
      step(w, fire, prev);
      step(w, none, prev);
      idle(w, kThrowFrames + 2);
      const int col = (w.player().x + 1) / kCellsPerTile + 6;
      check(solid && lifted, "fire lifts a loose block");
      check(w.warehouse().carry < 0 && w.map().block(col, 21) == Tile::Solid, "fire again throws it six blocks on");
    }
    {
      // A save with the crane gone and come back loads with the crate where
      // it dropped it.
      World w = make(room(4, 21, crane + "@ switch L1 4 21 kind=lever\n"));
      Keys prev;
      step(w, up, prev);
      step(w, none, prev);
      idle(w, 300);
      const SaveGame s = w.snapshot();
      World v = make(room(4, 21, crane + "@ switch L1 4 21 kind=lever\n"));
      const bool ok = v.restore(s);
      check(ok && crate(v, "MC1").x == 60 && v.map().block(30, 20) == Tile::Solid && v.map().block(10, 20) == Tile::Empty &&
          v.warehouse().levers[0].throws == 1,
        "a save loads with the crate where the crane dropped it");
    }

    // The bot clears the Prop Warehouse and Throw Everything with every runner.
    const auto level = std::make_shared<const Level>(Level::loadFile(argv[1]));
    const auto bonus = std::make_shared<const Level>(Level::loadFile(argv[2]));
    for (int character = 0; character < 6; ++character)
    {
      World w(level, character, theme, art);
      Bot bot;
      const int f = botPlay(w, 3000, bot);
      std::printf("     runner %d: %d frames, %d deaths\n", character, f, w.stats().deaths);
      check(w.state() != WorldState::Playing, "the bot clears the Prop Warehouse");
    }
    for (int character = 0; character < 6; ++character)
    {
      World w(bonus, character, theme, art);
      Bot bot;
      const int f = botPlay(w, 1350, bot);
      std::printf("     runner %d: %d frames\n", character, f);
      check(w.state() != WorldState::Playing && !w.bonusFailed(), "the bot clears Throw Everything");
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf(gFailures == 0 ? "all warehouse tests passed\n" : "%d warehouse test(s) failed\n", gFailures);
  return gFailures == 0 ? 0 : 1;
}
