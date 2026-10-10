// Level 26's mechanics, played by hand in small rooms: a model house broken
// by a jump (not by a walk, except for Rocco), a roof that gives way after
// 20 frames stood on (and the street slab under it), a tower toppling into
// a bridge (and onto a runner under it), the Atomic Breath burning a
// building, a power line snapped by a jump blacking out its Maser Truck, a
// Maser beam, a tank squad firing in turn and a tank stomped, a Toy Jet
// formation's two runs, the diner jumped clear over, and Gorgazilla's
// three phases. Then the bot plays Top-Down Rampage with every runner.

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
// runner at (px, py), plus `entities`.
std::string room(int px, int py, const std::string& entities, const std::string& extra = "")
{
  std::vector<std::string> map(24, std::string(40, '.'));
  for (int x = 0; x < 40; ++x)
    map[0][std::size_t(x)] = map[22][std::size_t(x)] = map[23][std::size_t(x)] = '#';
  for (int y = 0; y < 24; ++y)
    map[std::size_t(y)][0] = map[std::size_t(y)][39] = '#';
  map[std::size_t(py)][std::size_t(px)] = 'P';
  std::string s = "name=CITY TEST\nepisode=4\ntheme=model_city\nweapon=atomic_breath\n" + extra + "[map]\n";
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

int enemyShots(const World& w)
{
  int n = 0;
  for (const auto& pr : w.projectiles())
    n += pr.alive && pr.kind == ShotKind::Enemy;
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
    Keys right, jumpRight, breathe;
    right.right = true;
    jumpRight.right = jumpRight.jump = true;
    breathe.fire = true;
    const std::string house = "@ building H rect=10,21,10,21 kind=house\n";

    {
      // Walked into, a house stands; jumped into, it breaks and rates.
      const auto level = std::make_shared<const Level>(Level::parse(room(5, 21, house)));
      World w(level, 0, theme, art);
      Keys prev;
      hold(w, right, 20, prev);
      check(w.city().buildings[0].standing() && w.player().x + Player::kWidth == 20, "walked into, the house stands");
      hold(w, jumpRight, 6, prev);
      check(!w.city().buildings[0].standing() && w.map().block(10, 21) == Tile::Empty, "jumped into, it breaks");
      check(w.city().rating == 100 && w.stats().score >= 100, "its rating goes on the meter and the score");
    }
    {
      // Rocco walks through it.
      const auto level = std::make_shared<const Level>(Level::parse(room(5, 21, house)));
      World w(level, 1, theme, art);
      Keys prev;
      hold(w, right, 20, prev);
      check(!w.city().buildings[0].standing(), "Rocco walks through a house");
    }
    {
      // A roof stood on cracks for 20 frames, then gives way with the street under it.
      const auto level = std::make_shared<const Level>(Level::parse(room(6, 15,
        "@ building R rect=4,19,10,21 kind=roof under=7,22,9,22\n")));
      World w(level, 0, theme, art);
      idle(w, 12);
      check(w.player().state == PlayerState::OnGround && w.city().buildings[0].standing(), "the runner lands on the roof");
      idle(w, kRoofCrack - 12);
      check(w.city().buildings[0].standing() && w.city().buildings[0].cracks > 0, "it cracks");
      idle(w, 12);
      check(!w.city().buildings[0].standing() && w.map().block(8, 22) == Tile::Empty, "then gives way, slab and all");
    }
    {
      // A tower jumped into leans, falls and lies as a bridge as long as it was tall.
      const auto level = std::make_shared<const Level>(Level::parse(room(8, 21,
        "@ building T rect=12,14,12,21 kind=tower fall=r\n")));
      World w(level, 0, theme, art);
      Keys prev;
      hold(w, right, 10, prev);
      hold(w, jumpRight, 4, prev);
      check(w.city().buildings[0].state == BuildingState::Leaning, "a tower jumped into leans");
      hold(w, Keys{}, kTowerLean + kTowerFall + 2, prev);
      check(w.city().buildings[0].state == BuildingState::Lying && w.map().block(12, 15) == Tile::Empty &&
          w.map().block(13, 20) == Tile::Solid && w.map().block(20, 21) == Tile::Solid &&
          w.map().block(21, 21) == Tile::Empty,
        "it lies as a bridge 8 blocks long, 2 rows thick");
      check(w.city().rating == 500, "a tower rates 500");
    }
    {
      // A tower falling onto the runner costs 2 hearts and puts the runner on top.
      const auto level = std::make_shared<const Level>(Level::parse(room(8, 21,
        "@ building T rect=12,14,12,21 kind=tower fall=l\n")));
      World w(level, 0, theme, art);
      Keys prev;
      hold(w, right, 10, prev);
      hold(w, jumpRight, 4, prev);
      const int hp = w.player().hp;
      hold(w, Keys{}, kTowerLean + kTowerFall + 4, prev);
      check(w.player().hp == hp - 2, "a tower falling on the runner costs 2 hearts");
      check(w.player().y < 20 * kCellsPerTile, "and the runner ends up on it");
    }
    {
      // The Atomic Breath burns a building in 6 frames, a unit of ammo every 4.
      const auto level = std::make_shared<const Level>(Level::parse(room(5, 21, "@ building B rect=10,20,10,21 kind=block\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      const int ammo = w.player().ammo;
      Keys prev;
      hold(w, breathe, 5, prev);
      check(w.city().buildings[0].standing() && w.city().breath > 0, "5 frames of breath: still standing");
      hold(w, breathe, 3, prev);
      check(!w.city().buildings[0].standing(), "6 frames of breath burn it down");
      check(w.player().ammo == ammo - 2, "a unit of ammo every 4 frames");
      // Holding down sweeps it toward the floor.
      Keys sweep = breathe;
      sweep.down = true;
      hold(w, sweep, 12, prev);
      check(w.city().breathAng > 0, "holding down sweeps the beam down");
    }
    {
      // A Maser Truck: a ring, then a beam along its row for 2 hearts.
      const auto level = std::make_shared<const Level>(Level::parse(room(16, 21, "@ maser_truck 22 21\n")));
      World w(level, 0, theme, art);
      const int hp = w.player().hp;
      idle(w, 30);
      check(w.player().hp == hp - 2, "a Maser beam costs 2 hearts");
    }
    {
      // A power line snapped by a jump blacks out its sector: the truck's ring fizzles.
      const auto level = std::make_shared<const Level>(Level::parse(room(16, 21,
        "@ sector A rect=0,0,39,23\n@ powerline L path=12,18;28,18 sector=A\n@ maser_truck 24 21\n")));
      World w(level, 0, theme, art);
      const int hp = w.player().hp;
      Keys prev, up;
      up.jump = true;
      hold(w, up, 6, prev);
      check(w.city().lines[0].snapped && w.city().sectors[0].dark, "jumped into, the line snaps; the sector goes dark");
      hold(w, Keys{}, 80, prev);
      check(w.player().hp == hp, "a truck in a dark sector cannot fire");
    }
    {
      // A tank squad fires in turn, never two turrets lifted at once.
      const auto level = std::make_shared<const Level>(Level::parse(room(8, 21,
        "@ squad S kind=model_tank count=4 rect=16,21,34,21\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::God);
      check(w.city().squads[0].tanks.size() == 4, "the squad has its 4 tanks");
      int shots = 0, most = 0;
      for (int f = 0; f < 150; ++f)
      {
        idle(w, 1);
        int lifted = 0;
        for (int t : w.city().squads[0].tanks)
          lifted += w.enemies()[std::size_t(t)].tell > 0;
        most = std::max(most, lifted);
        shots = std::max(shots, enemyShots(w));
      }
      check(shots > 0 && most == 1, "the squad fires, one turret at a time");
    }
    {
      // Landed on, a tank is squashed.
      const auto level = std::make_shared<const Level>(Level::parse(room(10, 14,
        "@ squad S kind=model_tank count=1 rect=10,21,11,21\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::God);
      idle(w, 14);
      check(!w.enemies()[0].alive, "a tank landed on is squashed");
    }
    {
      // A Toy Jet formation: called in by the trigger, it grows in the
      // backdrop, makes two runs dropping shots, then leaves.
      const auto level = std::make_shared<const Level>(Level::parse(room(5, 21,
        "@ formation F kind=toy_jet count=3 trigger=8 row=10\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::God);
      Keys prev;
      hold(w, right, 8, prev);
      check(w.city().formations[0].state == 1, "passing the trigger calls the formation");
      int shots = 0;
      bool seen = false;
      for (int f = 0; f < 400 && w.city().formations[0].state != 4; ++f)
      {
        step(w, Keys{}, prev);
        shots = std::max(shots, enemyShots(w));
        for (int j : w.city().formations[0].jets)
          seen = seen || !w.enemies()[std::size_t(j)].hidden;
      }
      check(seen && shots > 0, "the jets strafe and drop shots");
      check(w.city().formations[0].state == 4 && w.city().formations[0].run == 2, "after two runs they leave");
    }
    {
      // The diner jumped clear over: the secret.
      const auto level = std::make_shared<const Level>(Level::parse(room(4, 21,
        "@ building diner rect=13,21,13,21 kind=house\n")));
      World w(level, 0, theme, art);
      Keys prev;
      for (int f = 0; f < 30 && w.player().x < 18; ++f)
        step(w, right, prev);
      hold(w, jumpRight, 14, prev);
      hold(w, right, 10, prev);
      check(w.city().buildings[0].standing() && w.city().dinerSaid && w.stats().score >= 10000,
        "the diner jumped clear over is worth 10000");
    }
    {
      // Gorgazilla: the door shuts behind the runner; three phases of 20,
      // a health drop at each change, Turbo off the crate at the first
      // swipe of phase 3, then he sits, the zipper opens and he walks off.
      const auto level = std::make_shared<const Level>(Level::parse(room(5, 21,
        "@ lizard 30 21 arena=0,0,39,23 door=2,10,2,21 piers=3,19,20,37 drops=8,21;30,21 turbo=34,21 dish=37,18\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::God);
      idle(w, 2);
      check(w.city().lizard.doorShut && w.map().block(2, 15) == Tile::Solid && w.lizardFight(), "the arena door shuts");
      Keys prev, shoot;
      shoot.fire = true;
      hold(w, right, 30, prev); // up close, so he swipes
      bool health = false, turbo = false;
      for (int f = 0; f < 3000 && w.city().lizard.state < LizardState::Sit; ++f)
      {
        Keys k;
        // In phase 3, hold fire till his first swipe.
        k.fire = f % 2 == 0 && (w.city().lizard.phase < 3 || w.city().lizard.turboDropped);
        k.right = w.player().facing < 0 && f % 2 == 1;
        step(w, k, prev);
        for (const auto& it : w.items())
          health = health || (it.kind == ItemKind::Health && !it.taken);
        turbo = turbo || w.city().lizard.turboDropped;
      }
      check(w.city().lizard.state >= LizardState::Sit, "60 hearts of shots beat him");
      check(health, "a phase change drops a health box");
      check(turbo, "his first phase-3 swipe knocks the Turbo box loose");
      check(w.stats().score >= 25000, "Gorgazilla scores 25000");
      idle(w, 45 + 125);
      check(w.city().lizard.state == LizardState::Done && !w.city().lizard.doorShut, "he walks off set; the door opens");
    }
    {
      // Top-Down Rampage: walked into, a building breaks; the bot makes the
      // 20000 with every runner inside the minute.
      const auto level = std::make_shared<const Level>(Level::loadFile(argv[1]));
      {
        World w(level, 0, theme, art);
        Keys prev, down;
        down.down = true;
        down.right = true;
        hold(w, down, 30, prev);
        int broken = 0;
        for (const auto& b : w.city().buildings)
          broken += !b.standing();
        check(w.topDown().on && broken > 0 && w.player().state == PlayerState::OnGround, "top-down: walking breaks buildings");
      }
      for (int ch = 0; ch < 6; ++ch)
      {
        World w(level, ch, theme, art);
        Bot bot;
        const int f = botPlay(w, 60 * 15, bot);
        std::printf("     top-down rampage, runner %d: score %d, %d frames, %d hearts lost\n", ch, w.stats().score, f,
          w.player().maxHp - w.player().hp);
        check(w.state() != WorldState::Playing && !w.bonusFailed() && w.stats().score >= 20000, "the bot makes 20000");
      }
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf("%s\n", gFailures == 0 ? "all city tests passed" : "SOME TESTS FAILED");
  return gFailures == 0 ? 0 : 1;
}
