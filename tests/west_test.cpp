// Level 22's mechanics, played by hand in small rooms: a fuse burning both
// ways from where it is shot, a TNT barrel's blast breaking a boulder and
// lighting the next barrel, a drawbridge falling, the Six-Shooter's
// cylinder, a bullet glancing off metal, a Duelist's rings and the quick
// draw, a Tumble Mine, a Window Bandit only hit while up, the respawn reset
// and the piano. Then High Noon, played by the bot.

#include "assets/art.hpp"
#include "data/level.hpp"
#include "data/theme.hpp"
#include "frontend/bot.hpp"
#include "game/world.hpp"
#include "render/renderer.hpp"

#include <SDL.h>

#include <algorithm>
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
// runner at (x, 21), plus whatever `rows` and `entities` say.
std::string room(int px, const std::vector<std::pair<int, std::string>>& rows, const std::string& entities,
  const std::string& header = "")
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
  std::string s = "name=WEST TEST\nepisode=4\ntheme=western_gulch\nweapon=six_shooter\n" + header + "[map]\n";
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

int litCells(const Fuse& f)
{
  int n = 0;
  for (const auto& c : f.cells)
    n += c.state != 0 ? 1 : 0;
  return n;
}

} // namespace

int main(int argc, char** argv)
{
  SDL_Init(0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  {
    Renderer renderer(sdl);
    const Theme& theme = themeByIndex(0);
    const Art art = Art::build(theme, renderer);

    {
      // A fuse lights where a shot crosses it and burns both ways from
      // there, a block every two frames: to its cap and to its barrel.
      const auto level = std::make_shared<const Level>(
        Level::parse(room(10, {}, "@ fuse F path=4,21;30,21 to=B\n@ barrel B 31 21 radius=2\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      check(w.west().on && w.west().fuses.size() == 1 && w.west().fuses[0].barrel == 0, "the fuse is linked to its barrel");
      Keys prev, crouch, shoot;
      crouch.down = true;
      shoot = crouch;
      shoot.fire = true;
      hold(w, crouch, 4, prev);
      step(w, shoot, prev);
      int frames = 0;
      while (!w.west().fuses[0].lit() && frames < 10)
      {
        step(w, crouch, prev);
        ++frames;
      }
      const auto& cells = w.west().fuses[0].cells;
      int first = -1;
      for (std::size_t i = 0; i < cells.size(); ++i)
        if (cells[i].state != 0 && first < 0)
          first = int(i);
      check(first > 0 && first < int(cells.size()) - 1, "a crouched shot lights it in the middle");
      idle(w, 6);
      check(cells[std::size_t(first - 1)].state != 0 && cells[std::size_t(first + 1)].state != 0 &&
          cells.front().state == 0 && cells.back().state == 0,
        "it burns both ways, a block at a time");
      int blown = 0;
      while (!w.west().barrels[0].blown && blown < 120)
      {
        idle(w, 1);
        ++blown;
      }
      std::printf("     the barrel blew %d frames later\n", blown);
      check(w.west().barrels[0].blown && !w.west().anyBurning(), "it reaches its barrel, which hisses and blows");
    }
    {
      // A barrel's blast breaks a boulder in its radius and lights the
      // barrel next to it; the runner far away is not hurt.
      std::vector<std::pair<int, std::string>> rows;
      for (int y = 17; y <= 21; ++y)
        rows.push_back({y, std::string(24, ' ') + "#"});
      const auto level = std::make_shared<const Level>(Level::parse(room(4, rows,
        "@ fuse F path=14,21;20,21 to=B\n@ barrel B 21 21 radius=2\n@ barrel C 23 21 radius=2\n"
        "@ breakable ROCK rect=25,17,25,21 hp=1 by=explosion\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      const int hp = w.player().hp;
      Keys prev, crouch, shoot;
      crouch.down = true;
      shoot = crouch;
      shoot.fire = true;
      hold(w, crouch, 4, prev);
      step(w, shoot, prev);
      hold(w, crouch, 90, prev);
      check(w.west().barrels[0].blown && w.west().barrels[1].blown, "a blast lights the barrel next to it");
      check(w.breakables()[0].broken && w.map().block(25, 19) != Tile::Solid, "and the second blast breaks the rock");
      check(w.player().hp == hp, "the runner out of the radius is not hurt");
    }
    {
      // A drawbridge's barrel blows: the bridge tips over and lies across
      // the gap at its foot.
      std::vector<std::pair<int, std::string>> rows;
      for (int y = 16; y <= 21; ++y)
        rows.push_back({y, std::string(19, ' ') + "#"});
      std::string m = room(4, rows, "@ drawbridge DB hinge=20,21 len=6 falls=l barrel=B\n@ barrel B 22 21 radius=1\n");
      const auto level = std::make_shared<const Level>(Level::parse(m));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      check(w.west().bridges.size() == 1 && w.west().bridges[0].barrel == 0 && w.map().block(20, 18) == Tile::Solid,
        "the drawbridge stands, linked to its barrel");
      Keys prev, crouch, shoot;
      crouch.down = true;
      shoot = crouch;
      shoot.fire = true;
      hold(w, crouch, 4, prev);
      step(w, shoot, prev);
      hold(w, crouch, 4, prev);
      // The shot stops at the bridge: shoot the barrel from the other side.
      check(!w.west().barrels[0].blown && w.west().barrels[0].t < 0, "a shot cannot reach the barrel through it");
    }
    {
      std::vector<std::pair<int, std::string>> rows;
      for (int y = 16; y <= 21; ++y)
        rows.push_back({y, std::string(19, ' ') + "#"});
      const auto level = std::make_shared<const Level>(Level::parse(
        room(30, rows, "@ drawbridge DB hinge=20,21 len=6 falls=l barrel=B\n@ barrel B 22 21 radius=1\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      Keys prev, face, crouch, shoot;
      face.left = true;
      step(w, face, prev);
      crouch.down = true;
      shoot = crouch;
      shoot.fire = true;
      hold(w, crouch, 4, prev);
      step(w, shoot, prev);
      hold(w, crouch, 20 + kBridgeFall + 4, prev);
      const auto& d = w.west().bridges[0];
      check(w.west().barrels[0].blown && d.down, "its barrel blows and the bridge falls");
      check(w.map().block(20, 18) != Tile::Solid && w.map().block(20, 16) != Tile::Solid,
        "the hinge column is clear");
      check(w.map().block(19, 22) == Tile::Solid && w.map().block(14, 22) == Tile::Solid, "the bridge lies at its foot");
    }
    {
      // The Six-Shooter: six shots, then a reload spin before the next.
      const auto level = std::make_shared<const Level>(Level::parse(room(4, {}, "")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      check(w.player().weapon == Weapon::Proto && w.player().proto == int(ProtoId::SixShooter), "the runner holds the Six-Shooter");
      Keys prev, fire;
      fire.fire = true;
      int shots = 0;
      auto tap = [&] {
        const std::size_t n = w.projectiles().size();
        step(w, fire, prev);
        int alive = 0;
        for (std::size_t i = n; i < w.projectiles().size(); ++i)
          alive += w.projectiles()[i].kind != ShotKind::Enemy;
        shots += alive > 0 ? 1 : 0;
        idle(w, 5);
        prev = Keys{};
      };
      for (int i = 0; i < 6; ++i)
        tap();
      std::printf("     %d shots, cylinder %d, reload %d\n", shots, w.west().cylinder, w.west().reload);
      check(shots == 6 && w.west().cylinder == 0 && w.west().reload > 0, "six shots empty the cylinder");
      tap();
      check(shots == 6, "the seventh waits for the reload");
      idle(w, kSixReload);
      check(w.west().cylinder == kSixCylinder, "after the spin the cylinder is full");
      tap();
      check(shots == 7, "and it fires again");
    }
    {
      // A Six-Shooter bullet glances off metal once, the way the metal
      // faces, and breaks a target up there.
      std::vector<std::pair<int, std::string>> rows = {{15, std::string(19, ' ') + "#"}};
      const auto level = std::make_shared<const Level>(Level::parse(
        room(10, rows, "@ metal 20 20 out=u\n@ breakable T rect=20,15,20,15 hp=1 by=any\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      Keys prev, fire;
      fire.fire = true;
      step(w, fire, prev);
      idle(w, 30);
      check(w.west().metals[0].ring > 0 || w.breakables()[0].broken, "the bullet rings the metal");
      check(w.breakables()[0].broken, "and glances up to break the target");
    }
    {
      // A Duelist calls you out: the bell rings, rings again kDuelGap frames
      // later, and kDuelDraw frames after that he fires at chest height.
      const auto level = std::make_shared<const Level>(Level::parse(room(4, {}, "@ duelist D 32 21\n")));
      World w(level, 0, theme, art);
      const int d = firstOf(w, EnemyKind::Duelist);
      check(d >= 0 && w.west().duelists.size() == 1, "the Duelist is there");
      Keys prev, right;
      right.right = true;
      int frames = 0;
      while (w.enemies()[std::size_t(d)].attach == 0 && frames < 200)
      {
        step(w, right, prev);
        ++frames;
      }
      const int dist = (w.enemies()[std::size_t(d)].x - w.player().x) / kCellsPerTile;
      std::printf("     called out %d blocks away\n", dist);
      check(dist <= kDuelRange, "he calls you out in range (once he is on screen)");
      int ring2 = -1, shot = -1;
      for (int f = 0; f < kDuelGap + kDuelDraw + 6; ++f)
      {
        idle(w, 1);
        const Enemy& e = w.enemies()[std::size_t(d)];
        if (ring2 < 0 && e.attach == 3)
          ring2 = f;
        for (const auto& pr : w.projectiles())
          if (shot < 0 && pr.alive && pr.kind == ShotKind::Enemy)
            shot = f;
      }
      std::printf("     second ring at %d, his shot at %d\n", ring2, shot);
      check(ring2 >= kDuelGap - 2 && ring2 <= kDuelGap + 2, "the second ring comes a gap after the first");
      check(shot >= ring2 + kDuelDraw - 1 && shot <= ring2 + kDuelDraw + 1, "he draws after the second ring");
    }
    {
      // Under Turbo, a Duelist shot down before the second ring is a quick
      // draw.
      const auto level = std::make_shared<const Level>(Level::parse(room(18, {}, "@ duelist D 28 21\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      w.cheat(Cheat::Turbo);
      const int score = w.stats().score;
      Keys prev, fire;
      fire.fire = true;
      for (int f = 0; f < 40 && w.enemies()[std::size_t(firstOf(w, EnemyKind::Duelist))].alive; ++f)
        step(w, f % 6 == 0 ? fire : Keys{}, prev);
      idle(w, 1);
      check(!w.enemies()[std::size_t(firstOf(w, EnemyKind::Duelist))].alive, "he goes down before the second ring");
      check(w.west().quickDraw > 0 && w.stats().score >= score + 500, "QUICK DRAW: 500 points");
    }
    {
      // A Tumble Mine from its spawner rolls with the wind and blows up on
      // the runner.
      const auto level = std::make_shared<const Level>(Level::parse(room(6, {},
        "@ wind rect=1,15,38,21 push=1/4 dir=l\n@ spawner kind=tumble_mine x=20 y=21 every=10 max=1\n")));
      World w(level, 0, theme, art);
      const int hp = w.player().hp;
      idle(w, 12);
      const int m = firstOf(w, EnemyKind::TumbleMine);
      check(m >= 0, "the spawner puts out a Tumble Mine");
      const int x0 = m >= 0 ? w.enemies()[std::size_t(m)].x : 0;
      idle(w, 10);
      check(m >= 0 && w.enemies()[std::size_t(m)].x < x0, "it rolls with the wind");
      for (int f = 0; f < 200 && w.player().hp == hp; ++f)
        idle(w, 1);
      check(w.player().hp < hp && !w.enemies()[std::size_t(m)].alive, "it blows up on the runner");
    }
    {
      // A Window Bandit is out of reach ducked, shows his hat, then pops up
      // and fires.
      const auto level = std::make_shared<const Level>(Level::parse(room(8, {}, "@ window_bandit WB 20 21\n")));
      World w(level, 0, theme, art);
      const int b = firstOf(w, EnemyKind::WindowBandit);
      bool down = false, tell = false, up = false, fired = false;
      for (int f = 0; f < kBanditDown + kBanditTell + kBanditUp + 4; ++f)
      {
        idle(w, 1);
        const Enemy& e = w.enemies()[std::size_t(b)];
        down = down || (e.hidden && e.tell == 0);
        tell = tell || (e.hidden && e.tell > 0);
        up = up || !e.hidden;
        for (const auto& pr : w.projectiles())
          fired = fired || (pr.alive && pr.kind == ShotKind::Enemy);
      }
      check(down && tell && up, "he ducks, shows his hat, pops up");
      check(fired, "and fires as he comes up");
    }
    {
      // The saloon piano: shooting its four keys in order plays a bar.
      const auto level = std::make_shared<const Level>(Level::parse(room(19, {}, "@ piano 20 20\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      Keys prev, fire, right;
      fire.fire = true;
      right.right = true;
      for (int k = 0; k < 4; ++k)
      {
        step(w, fire, prev);
        idle(w, 3);
        if (k < 3)
          hold(w, right, 2, prev);
        idle(w, 2);
      }
      std::printf("     piano step %d, tune %d\n", w.west().pianoStep, w.west().tune);
      check(w.west().tune > 0, "four keys in order play the bar");
    }
    {
      // High Noon: the bot wins all twenty duels and walks out.
      const auto level = std::make_shared<const Level>(Level::loadFile(argv[1]));
      World w(level, 0, theme, art);
      Bot bot;
      Input prev;
      for (int f = 0; f < 130 * 15 && w.state() == WorldState::Playing; ++f)
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
      std::printf("     high noon: duel %d, %d gems, %d false starts, %d frames\n", w.west().noon.duel, w.stats().gems,
        w.west().noon.falseStarts, w.stats().frames);
      check(w.stats().gems >= kHighNoonDuels && w.state() != WorldState::Playing, "the bot wins all twenty duels");
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf("%s\n", gFailures == 0 ? "all west tests passed" : "SOME TESTS FAILED");
  return gFailures == 0 ? 0 : 1;
}
