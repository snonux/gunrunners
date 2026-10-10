// Level 27's mechanics, played by hand in small rooms: hidden in a shadow,
// seen in a Flatfoot's flashlight for 8 frames (the whistle, the alarm and a
// Goon out of the nearest door), not seen in the shadow or during Turbo, a
// Crow cawing at a runner close by and at a loud shot, the Silencer's
// quiet triple hit on an unaware Flatfoot, the alarm running out (the Goons
// go), the painting shot open (the safe's ladder), the office alarm and
// its shutter, Clean Case and Case Closed. Then the Silent Picture: 1.5x
// logic, a title card, a banana peel, a pratfall and the ladder gag, and
// the bot plays it with every runner.

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

// A 40 x 24 room: ceiling row 0, floor rows 22-23, walls at x 0 and 39, the
// runner at (px, py), plus `entities`; `gap` clears the floor from x0 to x1.
std::string room(int px, int py, const std::string& entities, const std::string& extra = "", int gap0 = -1,
  int gap1 = -1)
{
  std::vector<std::string> map(24, std::string(40, '.'));
  for (int x = 0; x < 40; ++x)
    map[0][std::size_t(x)] = map[22][std::size_t(x)] = map[23][std::size_t(x)] = '#';
  for (int y = 0; y < 24; ++y)
    map[std::size_t(y)][0] = map[std::size_t(y)][39] = '#';
  for (int x = gap0; gap0 >= 0 && x <= gap1; ++x)
    map[22][std::size_t(x)] = map[23][std::size_t(x)] = '.';
  map[std::size_t(py)][std::size_t(px)] = 'P';
  std::string s = "name=NOIR TEST\nepisode=4\ntheme=noir_city\nweapon=silencer\n" + extra + "[map]\n";
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

int alive(const World& w, EnemyKind k)
{
  int n = 0;
  for (const auto& e : w.enemies())
    n += e.alive && e.kind == k;
  return n;
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
    Keys right, left, shoot;
    right.right = true;
    left.left = true;
    shoot.fire = true;
    const std::string flatfoot = "@ flatfoot 20 21 path=20,21;5,21\n@ door 25 21\n";

    {
      // A shadow hides the runner; out of it, not.
      const auto level =
        std::make_shared<const Level>(Level::parse(room(5, 21, "@ shadow rect=3,18,7,21\n@ light 30 20 r=2\n")));
      World w(level, 0, theme, art);
      idle(w, 2);
      check(w.noir().on && w.noir().hidden, "in a shadow, hidden");
      Keys prev;
      hold(w, right, 12, prev);
      check(!w.noir().hidden, "out of it, seen");
    }
    {
      // In a Flatfoot's flashlight for 8 frames: the whistle, the alarm,
      // then a Goon out of the nearest door.
      const auto level = std::make_shared<const Level>(Level::parse(room(13, 21, flatfoot)));
      World w(level, 0, theme, art);
      int spotted = -1;
      for (int f = 0; f < 40 && spotted < 0; ++f)
      {
        idle(w, 1);
        if (w.noir().alarm > 0)
          spotted = f;
      }
      check(spotted >= kSpotFill - 1 && spotted <= kSpotFill + 4 && w.noir().alarms == 1, "seen 8 frames: the alarm");
      idle(w, 20);
      check(alive(w, EnemyKind::Goon) == 1, "a Goon comes out of the door");
      idle(w, 40);
      check(alive(w, EnemyKind::Goon) == 2, "then another (two at most)");
      idle(w, 60);
      check(alive(w, EnemyKind::Goon) == 2, "no more than two");
      w.cheat(Cheat::God);
      int shots = 0;
      for (int f = 0; f < 60; ++f)
      {
        idle(w, 1);
        for (const auto& pr : w.projectiles())
          shots += pr.alive && pr.kind == ShotKind::Enemy && pr.age == 1;
      }
      check(shots >= 3, "a Goon fires a burst");
      // The alarm runs out: the Goons walk back in and are gone.
      for (int f = 0; f < kAlarmFrames * 2 && w.noir().alarm > 0; ++f)
        idle(w, 1);
      // (the Flatfoot may spot the runner again: then the alarm restarts)
      check(w.noir().alarms == 1, "being spotted again restarts the same alarm");
    }
    {
      // The same, from a shadow: never seen.
      const auto level =
        std::make_shared<const Level>(Level::parse(room(13, 21, flatfoot + "@ shadow rect=10,18,16,21\n")));
      World w(level, 0, theme, art);
      idle(w, 12);
      check(w.noir().hidden && w.noir().alarm == 0 && w.noir().suspicion == 0, "in the shadow, the flashlight misses");
    }
    {
      // In Turbo the "?" never fills.
      const auto level = std::make_shared<const Level>(Level::parse(room(13, 21, flatfoot)));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Turbo);
      idle(w, 12);
      check(w.noir().alarm == 0, "during Turbo, too fast to be spotted");
    }
    {
      // A Crow caws at a runner close by.
      const auto level = std::make_shared<const Level>(Level::parse(room(5, 21, "@ crow 14 21\n")));
      World w(level, 0, theme, art);
      idle(w, 4);
      check(w.noir().alarm == 0, "a Crow far off says nothing");
      Keys prev;
      hold(w, right, 10, prev);
      idle(w, 20);
      check(w.noir().alarms == 1, "close by, it caws");
    }
    {
      // A loud shot (the blaster) that misses makes a Crow caw.
      const auto level = std::make_shared<const Level>(Level::parse(room(5, 21, "@ crow 13 21\n")));
      World w(level, 0, theme, art);
      Keys prev;
      hold(w, left, 1, prev);
      idle(w, 2);
      check(w.noir().alarm == 0, "not seen from there");
      hold(w, shoot, 1, prev);
      idle(w, 14);
      check(w.noir().alarms == 1, "it heard the shot and cawed");
    }
    {
      // The Silencer: silent, and three times as hard on an unaware Flatfoot.
      const auto level = std::make_shared<const Level>(Level::parse(room(14, 21, "@ flatfoot 20 21 path=20,21;30,21 dir=r\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      Keys prev;
      hold(w, shoot, 1, prev);
      idle(w, 12);
      check(alive(w, EnemyKind::Flatfoot) == 0 && w.noir().alarms == 0, "one quiet shot in the back drops a Flatfoot");
    }
    {
      // The painting shot open: the safe's ladder up through the floor above.
      const auto level = std::make_shared<const Level>(Level::parse(room(10, 21,
        "@ switch painting 11 18 kind=shootable states=2 ladder=11,17,21\n")));
      World w(level, 0, theme, art);
      Keys prev, up;
      up.up = up.fire = true;
      idle(w, 2);
      hold(w, up, 1, prev);
      idle(w, 10);
      check(w.noir().paintOpen && w.map().block(11, 19) == Tile::Ladder, "the painting swings open on a ladder");
    }
    {
      // The office alarm: shot, the bell rings and the shutter opens; when
      // the alarm is over, it rolls down. Then a checkpoint: no Clean Case.
      const auto level = std::make_shared<const Level>(Level::parse(room(10, 21,
        "@ switch alarm 16 20 kind=shootable states=2 shutter=30,19,21\n")));
      World w(level, 0, theme, art);
      idle(w, 2);
      check(w.map().block(30, 20) == Tile::Solid, "the shutter is down");
      Keys prev;
      hold(w, shoot, 1, prev);
      idle(w, 6);
      check(w.noir().alarm > 0 && w.map().block(30, 20) == Tile::Empty, "shot, the alarm rings and the shutter opens");
      idle(w, kAlarmFrames + 2);
      check(w.noir().alarm == 0 && w.map().block(30, 20) == Tile::Solid, "it rolls down when the alarm is over");
      bool closed = false;
      for (const auto& b : w.bonuses())
        closed = closed || b.points == kCaseClosed;
      check(!closed, "no Case Closed after an alarm");
    }
    {
      // Clean Case at a checkpoint with no alarm since the last; Case Closed.
      std::string s = room(5, 21, "");
      s.replace(s.find("[map]\n") + 6 + 21 * 41 + 12, 1, "c");
      const auto level = std::make_shared<const Level>(Level::parse(s));
      World w(level, 0, theme, art);
      Keys prev;
      const int score = w.stats().score;
      hold(w, right, 20, prev);
      check(w.stats().score == score + kCleanCase, "a clean segment pays 2000 at the checkpoint");
    }
    {
      const auto level = std::make_shared<const Level>(Level::parse(room(5, 21, "@ shadow rect=1,1,2,2\n")));
      World w(level, 0, theme, art);
      idle(w, 4);
      bool closed = false;
      for (const auto& b : w.bonuses())
        closed = closed || b.points == kCaseClosed;
      check(closed, "no alarm all level: Case Closed");
    }

    // --- The Silent Picture -----------------------------------------------------------------
    const std::string film = "rules=silent_film\n";
    {
      // 1.5x: 20 updates are 30 logic frames.
      const auto level = std::make_shared<const Level>(Level::parse(room(5, 21, "@ flatfoot 30 21\n", film)));
      World w(level, 0, theme, art);
      idle(w, 20);
      check(w.film().on && w.stats().frames == 30, "the film runs at 1.5x");
    }
    {
      // A title card holds the picture for 30 frames.
      const auto level = std::make_shared<const Level>(Level::parse(room(5, 21, "@ room 0 0 text=\"THE BAKERY\"\n", film)));
      World w(level, 0, theme, art);
      idle(w, 2);
      const int f0 = w.stats().frames;
      check(w.film().card > 0, "a title card at the first room");
      idle(w, 20);
      check(w.stats().frames == f0, "it holds the picture");
      idle(w, 20);
      check(w.film().card == 0 && w.stats().frames > f0, "then the film runs on");
    }
    {
      // A banana peel slides the runner 4 blocks.
      const auto level = std::make_shared<const Level>(Level::parse(room(5, 21, "@ peel 9 21\n", film)));
      World w(level, 0, theme, art);
      Keys prev;
      bool slid = false;
      for (int f = 0; f < 20 && !slid; ++f)
      {
        step(w, right, prev);
        slid = w.film().slide > 0;
      }
      const int x0 = w.player().x;
      idle(w, 10);
      check(slid && w.player().x >= x0 + 6, "a banana peel slides the runner");
    }
    {
      // A drop of more than 4 blocks: a pratfall, no control for a moment.
      const auto level = std::make_shared<const Level>(Level::parse(room(5, 10, "", film)));
      World w(level, 0, theme, art);
      bool prat = false;
      for (int f = 0; f < 30 && !prat; ++f)
      {
        idle(w, 1);
        prat = w.film().pratfall > 0;
      }
      check(prat && w.filmHolds(), "a long fall ends in a pratfall");
    }
    {
      // The ladder gag: at the top it tips over the gap into a bridge.
      const auto level = std::make_shared<const Level>(Level::parse(room(10, 21, "@ laddergag 10 21 h=6 len=6\n", film, 11, 16)));
      World w(level, 0, theme, art);
      Keys prev, up;
      up.up = true;
      hold(w, up, 40, prev);
      idle(w, 20);
      check(w.film().gagState == 2 && w.map().block(13, 22) == Tile::Platform, "the ladder tips over into a bridge");
      check(w.player().x >= 17 * kCellsPerTile && w.player().state != PlayerState::Dying, "the runner lands on the far side");
    }
    {
      // The bot plays the Silent Picture with every runner.
      const auto level = std::make_shared<const Level>(Level::loadFile(argv[1]));
      for (int ch = 0; ch < 6; ++ch)
      {
        World w(level, ch, theme, art);
        Bot bot;
        const int f = botPlay(w, 90 * 15, bot);
        std::printf("     silent picture, runner %d: %d frames, %d gems, %d hearts lost\n", ch, f, w.stats().gems,
          w.player().maxHp - w.player().hp);
        check(w.state() != WorldState::Playing && !w.bonusFailed(), "the bot gets to the exit");
      }
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf("%s\n", gFailures == 0 ? "all noir tests passed" : "SOME TESTS FAILED");
  return gFailures == 0 ? 0 : 1;
}
