// Level 18's Low Gravity and Recoil Cannon, played by hand in small rooms:
// jump heights, the cannon's kick and its downward boost, Rivet Mites
// unbolting a plate and hopping on the runner, a Space Barnacle's ring of
// spikes and an EVA Ram's hit. Then the Planetoids bonus, played by the bot.

#include "assets/art.hpp"
#include "data/level.hpp"
#include "data/theme.hpp"
#include "frontend/bot.hpp"
#include "game/world.hpp"
#include "render/renderer.hpp"

#include <SDL.h>

#include <algorithm>
#include <cstdlib>
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

// A 40 x 12 room: floor row 10, walls at x 0 and 39, the runner at (x, 9),
// plus whatever `rows` and `entities` say.
std::string room(int px, const std::vector<std::pair<int, std::string>>& rows, const std::string& entities,
  const std::string& header = "flags=lowgrav\n")
{
  std::vector<std::string> map(12, std::string(40, '.'));
  for (int x = 0; x < 40; ++x)
  {
    map[10][std::size_t(x)] = '#';
    map[11][std::size_t(x)] = '#';
  }
  for (int y = 0; y < 12; ++y)
    map[std::size_t(y)][0] = map[std::size_t(y)][39] = '#';
  for (const auto& r : rows)
    for (std::size_t i = 0; i < r.second.size(); ++i)
      if (r.second[i] != ' ')
        map[std::size_t(r.first)][1 + i] = r.second[i];
  map[9][std::size_t(px)] = 'P';
  std::string s = "name=HULL TEST\nepisode=3\ntheme=station_hull\nweapon=recoil_cannon\n" + header + "[map]\n";
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

// How many cells a held jump rises, and in how many frames it is back down.
int jumpHeight(World& w, int& airFrames)
{
  Keys prev, hold;
  hold.jump = true;
  const int y0 = w.player().y;
  int top = y0;
  airFrames = 0;
  for (int f = 0; f < 80; ++f)
  {
    step(w, hold, prev);
    top = std::min(top, w.player().y);
    if (f > 2 && w.player().state == PlayerState::OnGround)
      break;
    ++airFrames;
  }
  return y0 - top;
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
      // Low gravity: Dash 11 cells (7 in normal gravity), Rocco 10, Nova 14,
      // and longer in the air.
      const auto low = std::make_shared<const Level>(Level::parse(room(5, {}, "")));
      const auto normal = std::make_shared<const Level>(Level::parse(room(5, {}, "", "")));
      const int want[] = {11, 10, 14};
      for (int c = 0; c < 3; ++c)
      {
        World a(low, c, theme, art), b(normal, c, theme, art);
        idle(a, 2);
        idle(b, 2);
        int airLow = 0, airNormal = 0;
        const int hLow = jumpHeight(a, airLow), hNormal = jumpHeight(b, airNormal);
        std::printf("     runner %d: %d cells in %d frames (normal %d in %d)\n", c, hLow, airLow, hNormal, airNormal);
        check(hLow == want[c] && airLow > airNormal, "low gravity: higher and longer jumps");
      }
    }
    {
      // The Recoil Cannon on the ground: 2 blocks back over 6 frames (Rocco 1).
      const auto level = std::make_shared<const Level>(Level::parse(room(20, {}, "")));
      for (int c : {0, 1})
      {
        World w(level, c, theme, art);
        w.cheat(Cheat::Ammo);
        idle(w, 2);
        const int x0 = w.player().x;
        Keys prev, shoot;
        shoot.fire = true;
        step(w, shoot, prev);
        idle(w, 8);
        std::printf("     runner %d kicked %d cells\n", c, x0 - w.player().x);
        check(x0 - w.player().x == (c == 1 ? 2 : 4), "a shot kicks the runner back");
      }
    }
    {
      // A shot straight down at the top of a jump is a second jump, once.
      const auto level = std::make_shared<const Level>(Level::parse(room(20, {}, "")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      idle(w, 2);
      const int y0 = w.player().y;
      Keys prev, hold, down;
      hold.jump = true;
      down.jump = true;
      down.down = true;
      int top = y0, f = 0;
      bool boosted = false, again = false;
      for (; f < 9; ++f)
        step(w, hold, prev);
      for (int k = 0; k < 40; ++k)
      {
        Keys pad = down;
        pad.fire = k % 12 == 0; // a second press later does nothing
        const int yBefore = w.player().y;
        step(w, pad, prev);
        top = std::min(top, w.player().y);
        boosted = boosted || w.hull().boostUsed;
        again = again || (k == 12 && w.player().y < yBefore);
      }
      // The room's top (row 0) stops it at 15 cells; a plain jump goes 11.
      std::printf("     boosted jump: %d cells\n", y0 - top);
      check(boosted && y0 - top >= 14, "a downward shot in the air boosts the jump");
      check(!again, "once per jump");
    }
    {
      // Rivet Mites work the rivets out and the plate drifts off.
      const auto level = std::make_shared<const Level>(Level::parse(room(14, {},
        "@ hullplate PL x0=20 x1=26 y=10 rivets=4\n@ rivet_mites M 23 9 carrier=1\n")));
      World w(level, 0, theme, art);
      int loose = -1;
      for (int f = 0; f < 300 && loose < 0; ++f)
      {
        idle(w, 1);
        if (w.hull().plates[0].drifting)
          loose = f;
      }
      std::printf("     plate loose after %d frames\n", loose);
      check(loose > 0 && w.map().block(23, 10) == Tile::Empty, "the mites unbolt the plate");
      idle(w, 20);
      bool rose = false;
      for (const auto& pl : w.platforms())
        rose = rose || (pl.id == "plate:PL" && pl.y < 20);
      check(rose, "and it drifts off");
    }
    {
      // A mite near the runner glows green and hops on: a carrier infects.
      const auto level = std::make_shared<const Level>(Level::parse(room(26, {},
        "@ hullplate PL x0=24 x1=30 y=10 rivets=4\n@ rivet_mites M 28 9 carrier=1\n")));
      World w(level, 0, theme, art);
      bool told = false, rode = false;
      for (int f = 0; f < 60 && w.player().virus == 0; ++f)
      {
        idle(w, 1);
        for (const auto& m : w.hull().mites)
        {
          told = told || m.state == MiteState::Tell;
          rode = rode || m.state == MiteState::Ride;
        }
      }
      check(told && rode && w.player().virus > 0, "a carrier mite hops on and infects");
      // Shooting all five kills the swarm.
      World w2(level, 0, theme, art);
      w2.cheat(Cheat::God);
      w2.cheat(Cheat::Ammo);
      Keys prev, shoot;
      shoot.fire = true;
      for (int f = 0; f < 200 && w2.enemies()[0].alive; ++f)
        step(w2, (f % 12) ? Keys{} : shoot, prev);
      int dead = 0;
      for (const auto& m : w2.hull().mites)
        dead += m.state == MiteState::Dead;
      std::printf("     mites dead %d\n", dead);
      check(dead > 0, "shots kill mites");
    }
    {
      // A Space Barnacle opens up and fires a ring of six spikes.
      const auto level = std::make_shared<const Level>(Level::parse(room(6, {}, "@ space_barnacle S 10 9\n")));
      World w(level, 0, theme, art);
      int spikes = 0;
      for (int f = 0; f < 40 && spikes == 0; ++f)
      {
        idle(w, 1);
        for (const auto& pr : w.projectiles())
          spikes += pr.alive && pr.spike;
      }
      check(spikes >= 4, "a barnacle fires its spikes");
    }
    {
      // An EVA Ram lines up with a runner on the ground and rams: a heart
      // and 3 blocks of push (Rocco only takes the heart).
      const auto level = std::make_shared<const Level>(Level::parse(room(18, {}, "@ eva_ram R 26 7\n")));
      for (int c : {0, 1})
      {
        World w(level, c, theme, art);
        const int hp0 = w.player().hp;
        int x0 = w.player().x, hitAt = -1;
        for (int f = 0; f < 150 && hitAt < 0; ++f)
        {
          x0 = w.player().x;
          idle(w, 1);
          if (w.player().hp < hp0)
            hitAt = f;
        }
        idle(w, 6);
        std::printf("     runner %d rammed at %d, pushed %d cells\n", c, hitAt, x0 - w.player().x);
        check(hitAt > 0 && (c == 1 ? x0 - w.player().x <= 1 : x0 - w.player().x >= 5), "an EVA Ram rams the runner");
      }
    }
    if (argc > 1)
    {
      // Planetoids: the bot hops from world to world for the ship's parts.
      const auto level = std::make_shared<const Level>(Level::loadFile(argv[1]));
      World w(level, 0, theme, art);
      Bot bot;
      Input prev;
      for (int f = 0; f < 120 * 15 && w.state() == WorldState::Playing; ++f)
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
        if (std::getenv("ORBIT_DEBUG") && f % 15 == 0)
          std::printf("f%d planet %d ang %.2f at %.1f,%.1f v %.2f,%.2f parts %d\n", f, w.orbit().planet, w.orbit().ang,
            w.orbit().px, w.orbit().py, w.orbit().vx, w.orbit().vy, w.orbit().partsTaken);
      }
      std::printf("     planetoids: %d parts in %d frames\n", w.orbit().partsTaken, w.stats().frames);
      check(w.state() == WorldState::Exiting, "the bot collects the ship's parts on the planetoids");
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf("%s\n", gFailures == 0 ? "all hull tests passed" : "SOME TESTS FAILED");
  return gFailures == 0 ? 0 : 1;
}
