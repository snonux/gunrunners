// Level 16's Ice Floors and Freeze Ray, played by hand on small rinks: a
// slide and its stop (Rocco slides further), the kicker's launch, a puck
// frozen and shot into the boards, a Lab Arm's lift and its power box, the
// thin ice that only Rocco's landing breaks, the DO NOT OPEN pod's back
// door. Then the bot plays the Air Hockey bonus.

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

// A 40 x 12 room: floor row 10, walls at x 0 and 39, the runner at (x, 9),
// plus whatever `extra` map rows and entities say.
std::string room(int px, const std::vector<std::pair<int, std::string>>& rows, const std::string& entities,
  const std::string& header = "")
{
  std::vector<std::string> map(12, std::string(40, '.'));
  for (int x = 0; x < 40; ++x)
  {
    map[0][std::size_t(x)] = '#';
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
  std::string s = "name=CRYO TEST\nepisode=3\ntheme=station_cryo\nweapon=freeze_ray\n" + header + "[map]\n";
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

// Cells slid after a 12-frame run right and letting go.
int slideAfterRun(const Theme& theme, const Art& art, int character)
{
  const auto level = std::make_shared<const Level>(Level::parse(room(3, {}, "@ ice rect=1,10,38,10\n")));
  World w(level, character, theme, art);
  Keys prev, right;
  right.right = true;
  for (int f = 0; f < 12; ++f)
    step(w, right, prev);
  const int x0 = w.player().x;
  for (int f = 0; f < 40; ++f)
    step(w, Keys{}, prev);
  return w.player().x - x0;
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
      // Ice: a run, then a slide to a stop; Rocco slides further.
      const int dash = slideAfterRun(theme, art, 0), rocco = slideAfterRun(theme, art, 1);
      std::printf("     slides: Dash %d cells, Rocco %d\n", dash, rocco);
      check(dash >= 5 && dash <= 12, "letting go on ice slides the runner a few blocks");
      check(rocco > dash, "Rocco slides further");
    }
    {
      // Matte: the same run stops dead.
      const auto level = std::make_shared<const Level>(Level::parse(room(3, {}, "@ ice rect=1,10,8,10\n")));
      World w(level, 0, theme, art);
      Keys prev, right;
      right.right = true;
      for (int f = 0; f < 14; ++f)
        step(w, right, prev);
      const int x0 = w.player().x;
      idle(w, 20);
      check(w.player().x - x0 <= 2, "off the ice onto matte, a slide stops within 2 cells");
    }
    {
      // The kicker: crossed at speed, a launch.
      const auto level = std::make_shared<const Level>(
        Level::parse(room(3, {}, "@ ice rect=1,10,38,10\n@ kicker 16 9 dir=r launch=14\n")));
      World w(level, 0, theme, art);
      Keys prev, right;
      right.right = true;
      int top = w.player().y;
      for (int f = 0; f < 60; ++f)
      {
        step(w, right, prev);
        top = std::min(top, w.player().y);
      }
      check(w.player().y - top >= 0 && 19 - top >= 12, "a kicker crossed at speed launches the runner");
    }
    {
      // The Freeze Ray: a puck frozen, then shot into the boards.
      const auto level = std::make_shared<const Level>(
        Level::parse(room(3, {}, "@ ice rect=1,10,38,10\n@ puck_drone PD 12 9\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      Keys prev, shoot;
      shoot.down = true;
      shoot.fire = true;
      Keys crouch;
      crouch.down = true;
      bool frozen = false;
      for (int f = 0; f < 12 && !frozen; ++f)
      {
        step(w, (f % 2) ? crouch : shoot, prev);
        for (const auto& e : w.enemies())
          frozen = frozen || (e.kind == EnemyKind::Puck && e.frozen > 0);
      }
      check(frozen, "a Freeze Ray shot freezes a Puck Drone");
      bool slid = false, dead = false;
      for (int f = 0; f < 60 && !dead; ++f)
      {
        step(w, (f % 2) ? crouch : shoot, prev);
        for (const auto& e : w.enemies())
          if (e.kind == EnemyKind::Puck)
          {
            slid = slid || e.vx > 0.0f;
            dead = !e.alive;
          }
      }
      check(slid && dead, "shot again on the ice, it slides into the boards and shatters");
    }
    {
      // A frozen mutant is a block to stand on.
      const auto level = std::make_shared<const Level>(Level::parse(room(3, {}, "@ pod_mutant M 8 9\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      Keys prev, shoot;
      shoot.fire = true;
      step(w, shoot, prev);
      idle(w, 6);
      const Enemy* m = nullptr;
      for (const auto& e : w.enemies())
        if (e.kind == EnemyKind::Mutant)
          m = &e;
      check(m && m->alive && m->frozen > 0, "a Freeze Ray shot freezes a mutant");
      check(m && w.player().hitBox().intersects(m->box()) == false, "and it does not reach the runner");
    }
    {
      // A Lab Arm: it drops, lifts the runner 2 blocks and lets go; its
      // power box shot out takes it down.
      const auto level = std::make_shared<const Level>(Level::parse(room(10,
        {}, "@ armrail x0=4 x1=20 y=1\n@ lab_arm LA 9 4 rail=6,14\n@ powerbox PX 20 9 arm=LA\n")));
      World w(level, 0, theme, art);
      const int y0 = w.player().y;
      int top = y0;
      bool held = false;
      for (int f = 0; f < 90; ++f)
      {
        w.update(PlayerInput{});
        held = held || w.cryo().held >= 0;
        top = std::min(top, w.player().y);
      }
      check(held && y0 - top == 4, "a Lab Arm grabs the runner and lifts them 2 blocks");
      check(w.cryo().held < 0 && w.player().y == y0, "then lets go");
      // Walk over to the power box and shoot it.
      World w2(level, 0, theme, art);
      w2.cheat(Cheat::Ammo);
      Keys prev, right, shoot, crouch;
      right.right = true;
      shoot.down = shoot.fire = true;
      crouch.down = true;
      for (int f = 0; f < 12; ++f)
        step(w2, right, prev);
      for (int f = 0; f < 40; ++f)
        step(w2, (f % 4) ? crouch : shoot, prev);
      bool armDead = false;
      for (const auto& e : w2.enemies())
        armDead = armDead || (e.kind == EnemyKind::LabArm && !e.alive);
      check(armDead, "its power box shot out powers the Lab Arm down");
    }
    {
      // The thin ice gives under Rocco's landing, not Dash's.
      const std::string text = room(4, {}, "@ breakable rect=3,10,6,10 by=heavy look=ice\n");
      for (int c : {0, 1})
      {
        const auto level = std::make_shared<const Level>(Level::parse(text));
        World w(level, c, theme, art);
        Keys prev, jump;
        jump.jump = true;
        for (int f = 0; f < 4; ++f)
          step(w, jump, prev);
        idle(w, 20);
        bool broken = false;
        for (const auto& b : w.breakables())
          broken = broken || b.broken;
        check(broken == (c == 1), c == 1 ? "Rocco landing breaks the thin ice" : "Dash landing does not");
      }
    }
    {
      // The DO NOT OPEN pod: shot open, its back door is the bonus.
      const auto level = std::make_shared<const Level>(Level::parse(
        room(3, {{7, "      B"}}, "@ breakable rect=7,7,8,9 hp=3 look=pod\n")));
      World w(level, 0, theme, art);
      bool dormant = false;
      for (const auto& pr : w.props())
        dormant = dormant || (pr.kind == PropKind::BonusDoor && pr.dormant);
      check(dormant, "the bonus door waits inside the pod");
      w.cheat(Cheat::Ammo);
      Keys prev, shoot;
      shoot.fire = true;
      for (int f = 0; f < 24; ++f)
        step(w, (f % 4) ? Keys{} : shoot, prev);
      bool open = false;
      for (const auto& pr : w.props())
        open = open || (pr.kind == PropKind::BonusDoor && !pr.dormant);
      check(open, "shot open, the pod's back door is there");
    }
    {
      // Air Hockey: 6 goals inside the 90 seconds.
      const std::string hockey = argc > 1 ? argv[1] : "levels/16_bonus_air_hockey.txt";
      const auto level = std::make_shared<const Level>(Level::loadFile(hockey));
      World w(level, 0, theme, art);
      Bot bot;
      Input prev;
      for (int f = 0; f < 90 * 15 && w.state() == WorldState::Playing; ++f)
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
      std::printf("     air hockey: %d goals in %d frames\n", w.cryo().scored, w.stats().frames);
      check(w.cryo().scored >= 6 && w.state() == WorldState::Exiting, "the bot scores 6 goals at Air Hockey");
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf("%s\n", gFailures == 0 ? "all cryo tests passed" : "SOME TESTS FAILED");
  return gFailures == 0 ? 0 : 1;
}
