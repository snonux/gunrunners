// Level 17's Grow Lamps and Hedge Trimmer, played by hand in small rooms: a
// switch shot from below lights its lamp and the bridge grows, withers when
// it goes out; the Trimmer's cone cuts thorns and shreds spores; a Glob
// splits; a Snapjaw bites. Then the Growth Spurt rules.

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
  std::string s = "name=GREEN TEST\nepisode=3\ntheme=station_greenhouse\nweapon=hedge_trimmer\n" + header + "[map]\n";
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

const GrowLamp* lampById(const World& w, const char* id)
{
  for (const auto& l : w.green().lamps)
    if (l.id == id)
      return &l;
  return nullptr;
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

    for (bool trimmer : {false, true})
    {
      // A switch overhead, shot from below: its lamp lights and the bridge
      // over the pit grows; a timed lamp goes out and the bridge withers.
      const auto level = std::make_shared<const Level>(Level::parse(room(5,
        {{10, "           ....    "}, {11, "           ....    "}},
        "@ switch SW 6 7 kind=shootable\n@ lamp LP 14 2 switch=SW timer=90\n"
        "@ plant BR lamp=LP kind=bridge rect=12,9,15,9 root=12,9\n")));
      World w(level, 0, theme, art);
      if (trimmer)
      {
        w.cheat(Cheat::Ammo);
      }
      Keys prev, up;
      up.up = true;
      idle(w, 2);
      Keys shoot = up;
      shoot.fire = true;
      step(w, up, prev);
      step(w, shoot, prev);
      idle(w, 2);
      const GrowLamp* l = lampById(w, "LP");
      check(l && l->lit, trimmer ? "with the Trimmer, a shot up lights the lamp" : "a shot up at a switch lights its lamp");
      idle(w, 50);
      check(w.map().block(15, 9) == Tile::Solid && w.map().block(12, 9) == Tile::Solid, "the bridge grows in");
      idle(w, 80);
      check(l && !l->lit && w.map().block(15, 9) == Tile::Empty, "the lamp goes out and the bridge withers");
    }
    {
      // The Trimmer's cone: a thorn wall goes in 3 cuts (4 damage each),
      // a unit of ammo a cut.
      const auto level = std::make_shared<const Level>(Level::parse(room(5, {{9, "      #"}, {8, "      #"}, {7, "      #"}},
        "@ breakable rect=7,7,7,9 by=any hp=12 look=thorn\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      const int ammo0 = w.player().ammo;
      Keys prev, hold;
      hold.fire = true;
      int frames = 0;
      while (!w.breakables()[0].broken && frames < 60)
      {
        step(w, hold, prev);
        ++frames;
      }
      std::printf("     thorns: %d frames, %d ammo\n", frames, ammo0 - w.player().ammo);
      check(w.breakables()[0].broken && ammo0 - w.player().ammo == 3, "the Trimmer cuts a thorn wall in 3 cuts");
    }
    {
      // A seed pod only gives to the Trimmer.
      const auto level = std::make_shared<const Level>(Level::parse(room(5, {{9, "      #"}},
        "@ breakable rect=7,9,7,9 by=trimmer hp=4 look=seedpod\n")));
      World w(level, 0, theme, art);
      Keys prev, shoot, crouch;
      shoot.fire = true;
      for (int f = 0; f < 40; ++f)
        step(w, (f % 2) ? Keys{} : shoot, prev);
      check(!w.breakables()[0].broken, "blaster shots don't open a seed pod");
      w.cheat(Cheat::Ammo);
      for (int f = 0; f < 12; ++f)
        step(w, shoot, prev);
      check(w.breakables()[0].broken, "the Trimmer does");
    }
    {
      // A Spore Puffer's cloud slows the runner; two carriers' clouds at
      // once infect; the Trimmer shreds a cloud.
      const auto level = std::make_shared<const Level>(Level::parse(room(10, {},
        "@ spore_puffer A 9 2 carrier=1 face=ceiling\n@ spore_puffer B 11 2 carrier=1 face=ceiling\n")));
      World w(level, 0, theme, art);
      bool slowed = false, infected = false;
      for (int f = 0; f < 200 && !infected; ++f)
      {
        w.update(PlayerInput{});
        slowed = slowed || w.green().slow > 0;
        infected = w.player().virus > 0;
      }
      check(slowed, "a spore cloud slows the runner");
      check(infected, "two carriers' clouds at once infect");
      World w2(level, 0, theme, art);
      w2.cheat(Cheat::Ammo);
      Keys prev, hold;
      hold.fire = true;
      int shredded = 0;
      for (int f = 0; f < 120; ++f)
      {
        const std::size_t before = w2.green().clouds.size();
        step(w2, hold, prev);
        if (w2.green().clouds.size() < before)
          ++shredded;
      }
      check(shredded > 0, "the Trimmer shreds spore clouds");
    }
    {
      // A Glob splits in two, and those in two again; small ones just die.
      const auto level = std::make_shared<const Level>(Level::parse(room(3, {}, "@ glob G 14 9\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::God);
      Keys prev, shoot;
      shoot.fire = true;
      int most = 0;
      for (int f = 0; f < 600; ++f)
      {
        step(w, (f % 2) ? Keys{} : shoot, prev);
        int n = 0;
        for (const auto& e : w.enemies())
          n += e.alive && e.kind == EnemyKind::Glob;
        most = std::max(most, n);
      }
      int alive = 0, kills = 0;
      for (const auto& e : w.enemies())
      {
        alive += e.alive;
        kills += !e.alive;
      }
      std::printf("     globs: %d at most, %d bodies in all, %d alive\n", most, kills + alive, alive);
      check(kills + alive == 7 && most >= 2, "a big Glob splits into 2 medium, those into 4 small");
    }
    {
      // A Snapjaw bites what comes within reach, after its tell.
      const auto level = std::make_shared<const Level>(Level::parse(room(3, {}, "@ snapjaw S 8 9\n")));
      World w(level, 0, theme, art);
      const int hp0 = w.player().hp;
      Keys prev, right;
      right.right = true;
      for (int f = 0; f < 7; ++f)
        step(w, right, prev);
      idle(w, 20);
      check(w.player().hp < hp0, "a Snapjaw bites the runner up close");
    }
    {
      // Growth Spurt: the bot grows to smash the wall, shoots itself small
      // for the tunnel and gets out inside the 90 seconds.
      const std::string bonus = argc > 1 ? argv[1] : "levels/17_bonus_growth_spurt.txt";
      const auto level = std::make_shared<const Level>(Level::loadFile(bonus));
      World w(level, 0, theme, art);
      Bot bot;
      Input prev;
      int biggest = 0;
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
        biggest = std::max(biggest, w.green().size);
      }
      std::printf("     growth spurt: out in %d frames, biggest x%.2f\n", w.stats().frames, 1.0 + 0.25 * biggest);
      check(biggest >= 2 && w.state() == WorldState::Exiting, "the bot grows, shrinks and gets out of Growth Spurt");
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf("%s\n", gFailures == 0 ? "all green tests passed" : "SOME TESTS FAILED");
  return gFailures == 0 ? 0 : 1;
}
