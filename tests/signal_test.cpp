// Level 29's mechanics, played by hand in small rooms: a dial shot steps the
// signal (the incoming bars flash first, the change waits while the runner
// stands on the outgoing colour), Tri-Tone tints (Up + fire turns the
// tint, a tinted shot sets a dial to it, three tints light the black bars
// and the bonus entrance), a Static Swarm latching (a bite every 15
// frames), the Test Card (only the quadrant in signal takes damage; its
// arena shuts and opens) and a Ghost Image's halo. Then the Wraparound:
// edges join, a fall comes back in at the top, a swarm's touch sends the
// runner home, and the bot collects every gem with every runner.

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

// A 40 x 24 room: ceiling row 0, floor rows 22-23, walls at x 0 and 39 (none
// with `open`), the runner at (px, py), plus `walls` and `entities`.
std::string room(int px, int py, const std::string& entities, const std::vector<Wall>& walls = {},
  const std::string& extra = "", bool open = false)
{
  std::vector<std::string> map(24, std::string(40, '.'));
  for (int x = 0; x < 40; ++x)
  {
    map[22][std::size_t(x)] = map[23][std::size_t(x)] = '#';
    if (!open)
      map[0][std::size_t(x)] = '#';
  }
  for (int y = 0; y < 24 && !open; ++y)
    map[std::size_t(y)][0] = map[std::size_t(y)][39] = '#';
  for (const auto& w : walls)
    for (int y = w.y0; y <= w.y1; ++y)
      for (int x = w.x0; x <= w.x1; ++x)
        map[std::size_t(y)][std::size_t(x)] = w.c;
  map[std::size_t(py)][std::size_t(px)] = 'P';
  std::string s = "name=SIGNAL TEST\nepisode=5\ntheme=dead_air\nweapon=tri_tone_blaster\n" + extra + "[map]\n";
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

// A tap of fire, then the button let go.
void tap(World& w, Keys pad, Keys& prev)
{
  pad.fire = true;
  step(w, pad, prev);
  pad.fire = false;
  step(w, pad, prev);
}

bool barSolid(const World& w, int i)
{
  return w.layers()[std::size_t(w.signal().bars[std::size_t(i)].layer)].solid;
}

const Enemy* first(const World& w, EnemyKind k)
{
  for (const auto& e : w.enemies())
    if (e.alive && e.kind == k)
      return &e;
  return nullptr;
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
    Keys none, up;
    up.up = true;

    {
      // A blaster shot at a dial: the green bars flash, then green is in.
      const auto level = std::make_shared<const Level>(Level::parse(room(5, 21,
        "@ signal start=red\n@ bar color=red rect=20,18,23,18\n@ bar color=green rect=26,18,29,18\n"
        "@ rgbswitch s1 9 20 mode=shoot\n")));
      World w(level, 0, theme, art);
      idle(w, 4);
      check(w.signal().on && w.signal().color == kSignalRed && barSolid(w, 0) && !barSolid(w, 1),
        "the signal starts red: red bars solid, green ghosts");
      Keys prev;
      tap(w, none, prev);
      idle(w, 4);
      check(w.signal().pending == kSignalGreen && w.signal().color == kSignalRed && w.signal().flash > 0,
        "a dial shot: green waits, its bars flashing");
      idle(w, kSignalFlash + 2);
      check(w.signal().color == kSignalGreen && w.signal().pending < 0 && !barSolid(w, 0) && barSolid(w, 1),
        "...then green is in");
    }
    {
      // On a red bar the change waits until the runner leaves it.
      const auto level = std::make_shared<const Level>(Level::parse(room(5, 15,
        "@ signal start=red\n@ bar color=red rect=3,16,8,16\n@ bar color=green rect=12,16,20,16\n"
        "@ rgbswitch s1 11 14 mode=shoot\n")));
      World w(level, 0, theme, art);
      idle(w, 6);
      Keys prev;
      tap(w, none, prev);
      idle(w, 30);
      check(w.signal().pending == kSignalGreen && w.signal().color == kSignalRed &&
          w.player().state == PlayerState::OnGround && w.player().y == 31,
        "standing on red: the change waits");
      Keys jumpRight;
      jumpRight.jump = jumpRight.right = true;
      hold(w, jumpRight, 6, prev);
      check(w.signal().color == kSignalGreen, "airborne: it lands");
      Keys right;
      right.right = true;
      hold(w, right, 14, prev);
      idle(w, 8);
      check(w.player().state == PlayerState::OnGround && w.player().y == 31 && w.player().x >= 12 * 2,
        "...and the runner lands on the green bar");
    }
    {
      // The Tri-Tone: Up + fire turns the tint, a tinted shot sets a dial to
      // it; all three tints fired light the black bars and the bonus entrance.
      const auto level = std::make_shared<const Level>(Level::parse(room(5, 21,
        "@ signal start=red\n@ bar color=black rect=20,18,23,18\n@ rgbswitch s1 9 20 mode=shoot\n",
        {{30, 21, 30, 21, 'B'}})));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      idle(w, 2);
      check(w.player().weapon == Weapon::Proto && w.signal().tint == kSignalRed, "the Tri-Tone, tinted red");
      bool dormant = false;
      for (const auto& pr : w.props())
        dormant = dormant || (pr.kind == PropKind::BonusDoor && pr.dormant);
      check(dormant && !barSolid(w, 0), "black bars ghosts, the bonus entrance hidden");
      Keys prev;
      const int ammo = w.player().ammo;
      tap(w, up, prev);
      idle(w, 2);
      check(w.signal().tint == kSignalGreen && w.player().ammo == ammo, "Up + fire turns the tint, no shot");
      tap(w, none, prev);
      idle(w, kSignalFlash + 6);
      check(w.signal().color == kSignalGreen && w.signal().pips[kSignalGreen] > 0,
        "a green shot sets the dial to green and lights its pip");
      idle(w, kTintCool);
      tap(w, up, prev);
      idle(w, 2);
      tap(w, none, prev);
      idle(w, kTintCool);
      tap(w, up, prev);
      idle(w, 2);
      tap(w, none, prev);
      idle(w, 4);
      dormant = false;
      for (const auto& pr : w.props())
        dormant = dormant || (pr.kind == PropKind::BonusDoor && pr.dormant);
      check(w.blackLit() && barSolid(w, 0) && !dormant, "three tints: black bars solid, the bonus entrance shows");
      idle(w, kPipFrames);
      check(!w.blackLit() && !barSolid(w, 0), "a pip runs out: black bars gone");
    }
    {
      // A plain Static Swarm drifts at the runner and latches on: a heart per
      // 15 frames. Turbo shakes it off.
      const auto level =
        std::make_shared<const Level>(Level::parse(room(5, 21, "@ static_swarm 14 20 carrier=0\n")));
      World w(level, 0, theme, art);
      const int hp = w.player().hp;
      idle(w, 60);
      const Enemy* s = first(w, EnemyKind::StaticSwarm);
      check(s && s->attach != 0, "the swarm latches on");
      idle(w, kSwarmBite * 2 + 1);
      check(w.player().hp < hp, "...and bites");
      w.cheat(Cheat::Turbo);
      idle(w, 2);
      s = first(w, EnemyKind::StaticSwarm);
      check(s && s->attach == 0, "Turbo shakes it off");
    }
    {
      // The Test Card: walking in shuts its arena; only the quadrant in
      // signal takes damage; down, the walls open.
      const auto level = std::make_shared<const Level>(Level::parse(room(3, 21,
        "@ signal start=red\n@ test_card 26 18\n@ cardarena rect=10,1,38,21 wall=9,16,21 exit=37,16,21\n"
        "@ rgbswitch t1 12 10 mode=timed period=40 warn=8 rect=10,1,38,21\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::God);
      Keys prev, right;
      right.right = true;
      while (w.player().x < 53 && w.stats().frames < 200)
        step(w, right, prev);
      check(w.signal().wallShut && w.cardFight() && w.map().solid(9 * 2, 40) && w.map().solid(37 * 2, 40),
        "in the arena: both walls shut");
      // Shots straight up into it while red is in.
      for (int k = 0; k < 3; ++k)
      {
        tap(w, up, prev);
        idle(w, 3);
      }
      idle(w, 6);
      const auto& sg = w.signal();
      check(sg.color == kSignalRed ? sg.cardHp[0] < 3 && sg.cardHp[1] == 3 && sg.cardHp[2] == 3 : sg.cardHp[0] < 3,
        "only the red quadrant takes the hits");
      for (int f = 0; f < 900 && !w.signal().cardDead; f += 4)
      {
        tap(w, up, prev);
        idle(w, 2);
      }
      check(w.signal().cardDead && !w.signal().wallShut && !w.map().solid(9 * 2, 40) && !w.map().solid(37 * 2, 40),
        "all three quadrants down: the card is gone, the walls open");
    }
    {
      // A Ghost Image's halo turns blaster shots; after it fires, they hurt.
      const auto level = std::make_shared<const Level>(Level::parse(room(5, 21, "@ ghost_image 16 21\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::God);
      Keys prev;
      idle(w, 2);
      const int hp = first(w, EnemyKind::GhostImage)->hp;
      tap(w, none, prev);
      idle(w, 8);
      check(first(w, EnemyKind::GhostImage)->hp == hp, "the halo turns a blaster shot");
      for (int f = 0; f < 200 && first(w, EnemyKind::GhostImage)->dive == 0; ++f)
        idle(w, 1);
      const Enemy* g = first(w, EnemyKind::GhostImage);
      Keys face;
      (g->x < w.player().x ? face.left : face.right) = true;
      step(w, face, prev);
      tap(w, none, prev);
      idle(w, 8);
      g = first(w, EnemyKind::GhostImage);
      check(!g || g->hp < hp, "after it fires, no halo: a blaster shot hurts");
    }

    // The Wraparound.
    {
      const auto level = std::make_shared<const Level>(Level::parse(
        room(2, 21, "@ static_swarm 30 10 carrier=0\n", {{18, 22, 21, 23, '.'}, {14, 6, 25, 6}}, "rules=wrap_xy\n", true)));
      World w(level, 0, theme, art);
      idle(w, 2);
      check(w.signal().wrap && w.map().wraps(), "rules=wrap_xy: the edges join");
      Keys prev, left, right;
      left.left = true;
      right.right = true;
      hold(w, left, 10, prev);
      check(w.player().x > 60 && w.player().state == PlayerState::OnGround, "out at the left, in at the right");
      hold(w, right, 10, prev);
      check(w.player().x < 20, "...and back");
      while (w.player().x < 37 && w.stats().frames < 200)
        step(w, right, prev);
      idle(w, 40);
      check(w.player().state == PlayerState::OnGround && w.player().y == 11,
        "down the floor's gap, in at the top, onto the ledge");
    }
    {
      const auto level = std::make_shared<const Level>(
        Level::parse(room(2, 21, "@ static_swarm 10 21 carrier=0\n", {}, "rules=wrap_xy\n", true)));
      World w(level, 0, theme, art);
      const int x0 = w.player().x, hp = w.player().hp;
      Keys prev, right;
      right.right = true;
      hold(w, right, 8, prev);
      bool home = false;
      for (int f = 0; f < 60 && !home; ++f)
      {
        idle(w, 1);
        home = w.player().x == x0;
      }
      check(home && w.player().hp == hp, "a swarm's touch: back home, no harm");
    }
    {
      const auto level = std::make_shared<const Level>(Level::loadFile(argv[1]));
      for (int c = 0; c < 6; ++c)
      {
        World w(level, c, theme, art);
        Bot bot;
        botPlay(w, 900, bot);
        char what[80];
        std::snprintf(what, sizeof what, "the bot clears the Wraparound (runner %d): %d gems", c, w.stats().gems);
        check(w.stats().gems >= 30, what);
      }
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf("%d failure(s)\n", gFailures);
  return gFailures == 0 ? 0 : 1;
}
