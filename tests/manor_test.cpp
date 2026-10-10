// Level 23's mechanics, played by hand in small rooms: a mirror flipping
// the side (side-only walls, enemies and items), the backwards step, two
// levers opening a door, the Silver Crossbow pinning a Portrait Ghost as a
// platform, a Haunted Armor's swing, a Poltergeist's throw and the coffin.
// Then Both Sides, played by the bot.

#include "assets/art.hpp"
#include "data/level.hpp"
#include "data/theme.hpp"
#include "frontend/bot.hpp"
#include "game/world.hpp"
#include "render/renderer.hpp"

#include <SDL.h>

#include <cstdio>
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

// A 40 x 24 room: ceiling row 0, floor rows 22-23, walls at x 0 and 39, the
// runner at (x, 21), plus whatever `rows` and `entities` say.
std::string room(int px, const std::vector<std::pair<int, std::string>>& rows, const std::string& entities)
{
  std::vector<std::string> map(24, std::string(40, '.'));
  for (int x = 0; x < 40; ++x)
    map[0][std::size_t(x)] = map[22][std::size_t(x)] = map[23][std::size_t(x)] = '#';
  for (int y = 0; y < 24; ++y)
    map[std::size_t(y)][0] = map[std::size_t(y)][39] = '#';
  for (const auto& r : rows)
    for (std::size_t i = 0; i < r.second.size(); ++i)
      if (r.second[i] != ' ')
        map[std::size_t(r.first)][1 + i] = r.second[i];
  map[21][std::size_t(px)] = 'P';
  std::string s = "name=MANOR TEST\nepisode=4\ntheme=haunted_manor\nweapon=silver_crossbow\n[map]\n";
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

void hold(World& w, Keys pad, int frames, Keys& prev)
{
  for (int f = 0; f < frames; ++f)
    step(w, pad, prev);
}

void idle(World& w, int frames)
{
  Keys prev;
  for (int f = 0; f < frames; ++f)
    step(w, Keys{}, prev);
}

int firstOf(const World& w, EnemyKind kind)
{
  for (std::size_t i = 0; i < w.enemies().size(); ++i)
    if (w.enemies()[i].kind == kind)
      return int(i);
  return -1;
}

} // namespace

int main(int argc, char** argv)
{
  if (argc < 2)
    return 2;
  SDL_Init(0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  {
    Renderer renderer(sdl);
    const Theme& theme = themeByIndex(0);
    const Art art = Art::build(theme, renderer);

    {
      // Up at a mirror: a shimmer, then the other side. Side-only walls,
      // enemies and items swap with it.
      const auto level = std::make_shared<const Level>(Level::parse(room(10, {{21, "             g"}},
        "@ mirror m1 10 19\n"
        "@ layer wr rect=20,18,20,21 tile=# driver=script side=real\n"
        "@ layer wm rect=24,18,24,21 tile=# driver=script side=mirror\n"
        "@ haunted_armor HA 30 21 side=mirror\n"
        "@ sideitem 14 21 side=mirror\n")));
      World w(level, 0, theme, art);
      const int ha = firstOf(w, EnemyKind::HauntedArmor);
      int gem = -1;
      for (std::size_t i = 0; i < w.items().size(); ++i)
        gem = int(i);
      check(w.manor().on && w.manor().side == kSideReal, "the manor starts on the real side");
      check(w.map().block(20, 21) == Tile::Solid && w.map().block(24, 21) == Tile::Empty, "real walls stand, mirror ones don't");
      check(ha >= 0 && w.enemies()[std::size_t(ha)].hidden, "a mirror-side armor is out of sight");
      check(gem >= 0 && w.items()[std::size_t(gem)].taken, "a mirror-side gem is parked");
      Keys prev, up;
      up.up = true;
      step(w, up, prev);
      check(w.manor().flip > 0, "Up at the mirror starts the shimmer");
      hold(w, Keys{}, kMirrorShimmer + 1, prev);
      check(w.manor().side == kSideMirror, "and the runner is in the reflection");
      check(w.map().block(20, 21) == Tile::Empty && w.map().block(24, 21) == Tile::Solid, "the walls swapped");
      check(!w.enemies()[std::size_t(ha)].hidden && !w.items()[std::size_t(gem)].taken, "the armor and the gem are here");
      Keys down;
      down.down = true;
      step(w, Keys{}, prev);
      step(w, down, prev);
      hold(w, Keys{}, kMirrorShimmer / 2 + 1, prev);
      check(w.manor().side == kSideReal && w.manor().reflection > 0, "Down steps through backwards and leaves a reflection");
    }
    {
      // Two levers, one door: the first one alone only clunks, the second
      // (shot from across the room) opens it.
      const auto level = std::make_shared<const Level>(Level::parse(room(14, {},
        "@ switch id=l1 x=14 y=21 kind=lever states=2\n"
        "@ switch id=l2 x=22 y=21 kind=lever states=2\n"
        "@ layer door rect=26,18,26,21 tile=# driver=switch switch=l1,l2 state=1\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      Keys prev, up, crouch, shoot;
      up.up = true;
      step(w, up, prev);
      check(w.manor().levers.size() == 2 && w.manor().levers[0].state == 1, "Up at a lever throws it");
      check(w.map().block(26, 21) == Tile::Solid, "one lever is not enough");
      crouch.down = true;
      shoot = crouch;
      shoot.fire = true;
      hold(w, crouch, 4, prev);
      step(w, shoot, prev);
      hold(w, crouch, 20, prev);
      check(w.manor().levers[1].state == 1, "a bolt throws the other lever");
      check(w.map().block(26, 21) == Tile::Empty, "and the door opens");
    }
    {
      // A Portrait Ghost comes out of its frame; a bolt pins it, it is a
      // platform for a while, shots pass it, then it shakes loose.
      const auto level = std::make_shared<const Level>(Level::parse(room(8, {}, "@ portrait_ghost PG 18 18\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      w.cheat(Cheat::God);
      const int g = firstOf(w, EnemyKind::PortraitGhost);
      check(g >= 0 && w.enemies()[std::size_t(g)].hidden, "the ghost waits in its portrait");
      idle(w, 2);
      check(w.enemies()[std::size_t(g)].attach != 0, "and comes out for a runner close by");
      Keys prev, fire;
      fire.fire = true;
      for (int f = 0; f < 120 && w.manor().platforms.empty(); ++f)
        step(w, (f % 2 == 0 && w.player().shotCooldown == 0) ? fire : Keys{}, prev);
      check(w.manor().platforms.size() == 1 && w.enemies()[std::size_t(g)].stun > 0, "a bolt pins it as a platform");
      const auto& pl = w.manor().platforms[0];
      check(w.map().block(pl.bx, pl.by) == Tile::Platform, "its top is something to stand on");
      const int hp = w.enemies()[std::size_t(g)].hp;
      hold(w, Keys{}, 12, prev);
      step(w, fire, prev);
      hold(w, Keys{}, 20, prev);
      check(w.enemies()[std::size_t(g)].hp == hp, "a pinned ghost lets shots pass");
      hold(w, Keys{}, kPinFrames, prev);
      check(w.manor().platforms.empty(), "it shakes loose");
    }
    {
      // A Haunted Armor stands still until the runner comes close, then
      // tells and swings its halberd.
      const auto level = std::make_shared<const Level>(Level::parse(room(6, {}, "@ haunted_armor HA 20 21\n")));
      World w(level, 0, theme, art);
      const int a = firstOf(w, EnemyKind::HauntedArmor);
      idle(w, 20);
      check(w.enemies()[std::size_t(a)].attach == 0, "the armor stands still");
      const int hp = w.player().hp;
      Keys prev, right;
      right.right = true;
      for (int f = 0; f < 200 && w.player().hp == hp; ++f)
        step(w, f < 40 ? right : Keys{}, prev);
      check(w.enemies()[std::size_t(a)].attach != 0, "it wakes as the runner passes close");
      check(w.player().hp < hp, "and its swing hurts");
    }
    {
      // A Poltergeist lobs its object at the runner; every third tureen
      // plate is a carrier.
      const auto level = std::make_shared<const Level>(Level::parse(room(8, {}, "@ poltergeist P 16 21 object=tureen\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::God);
      int thrown = 0, carriers = 0;
      for (int f = 0; f < 600 && thrown < 3; ++f)
      {
        const std::size_t before = w.projectiles().size();
        idle(w, 1);
        for (std::size_t i = before; i < w.projectiles().size(); ++i)
          if (w.projectiles()[i].thrown == 2)
          {
            ++thrown;
            carriers += w.projectiles()[i].carrier ? 1 : 0;
          }
      }
      check(thrown == 3, "the poltergeist throws plates");
      check(carriers == 1, "the third one is a carrier");
    }
    {
      // The coffin: a shot lifts the lid and the sign changes its mind.
      const auto level = std::make_shared<const Level>(Level::parse(room(8, {}, "@ coffin rect=20,20,23,21 sign=22,19\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      Keys prev, crouch, shoot;
      crouch.down = true;
      shoot = crouch;
      shoot.fire = true;
      hold(w, crouch, 4, prev);
      step(w, shoot, prev);
      hold(w, crouch, 20, prev);
      check(w.manor().coffinWoke, "the coffin wakes");
    }
    {
      // Both Sides: the bot steers both runners onto their exits at once.
      const auto level = std::make_shared<const Level>(Level::loadFile(argv[1]));
      World w(level, 0, theme, art);
      check(w.manor().split.on, "Both Sides runs split");
      Bot bot;
      Input prev;
      for (int f = 0; f < 95 * 15 && w.state() == WorldState::Playing; ++f)
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
      std::printf("     both sides: %d gems, %d frames\n", w.stats().gems, w.stats().frames);
      check(w.state() != WorldState::Playing && w.manor().split.topOn && w.manor().split.botOn,
        "the bot brings both runners out together");
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf("%s\n", gFailures == 0 ? "all manor tests passed" : "SOME TESTS FAILED");
  return gFailures == 0 ? 0 : 1;
}
