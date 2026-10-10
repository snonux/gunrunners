// Level 49's Hive Mother and the Star Seed on a small test arena: she
// sleeps until you come in, then the door shuts and she wakes; standing
// shots glance off her, jump shots at her bowed crown hurt; she lays eggs
// and calls Spore Nurses, which heal her; breathing in, she pulls you
// toward her; charging, she runs to the wall and stands dazed against it;
// the fight can't be saved; a charged Star Seed goes through two eggs.

#include "assets/art.hpp"
#include "data/level.hpp"
#include "data/theme.hpp"
#include "game/world.hpp"
#include "render/renderer.hpp"

#include <SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

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

// 60 x 20 blocks, the ground's top on row 14. With `arena`: a wall at x 10
// with a door at rows 11-13, the arena x 11-58, rows 2-13, and the Hive
// Mother on her throne at x 49.
std::string testLevel(const std::string& extra, bool arena, int start = 4)
{
  std::string map;
  for (int y = 0; y < 20; ++y)
  {
    std::string row(60, '.');
    for (int x = 0; x < 60; ++x)
    {
      if (y >= 14 || x == 0 || x == 59)
        row[std::size_t(x)] = '#';
      if (arena && x == 10 && y <= 10)
        row[std::size_t(x)] = '#';
    }
    if (y == 13)
      row[std::size_t(start)] = 'P';
    map += row + "\n";
  }
  std::string ents = extra;
  if (arena)
    ents += "@ hive_mother 49 13 arena=11,2,58,13 door=10,11,10,13\n";
  return "name=MOTHER TEST\nepisode=7\ntheme=hive_mother\nweapon=star_seed\n[map]\n" + map + "[entities]\n" + ents;
}

PlayerInput press(int dx, bool jump = false, bool fire = false)
{
  PlayerInput p;
  p.right = dx > 0;
  p.left = dx < 0;
  p.jump = {jump, jump};
  p.fire = {fire, fire};
  return p;
}

void idle(World& w, int frames)
{
  for (int f = 0; f < frames; ++f)
    w.update(press(0));
}

// Walks right until the fight starts (or gives up).
bool walkIn(World& w)
{
  for (int f = 0; f < 200 && w.mother().phase == MotherPhase::Asleep; ++f)
    w.update(press(1));
  return w.mother().phase != MotherPhase::Asleep;
}

int countAll(const World& w, EnemyKind kind)
{
  int n = 0;
  for (const auto& e : w.enemies())
    n += e.kind == kind;
  return n;
}

int countAlive(const World& w, EnemyKind kind)
{
  int n = 0;
  for (const auto& e : w.enemies())
    n += e.alive && e.kind == kind;
  return n;
}

// Walks up to `gap` cells off her front.
void approach(World& w, int gap)
{
  for (int f = 0; f < 200 && w.player().x + 3 < int(w.mother().x) - gap; ++f)
    w.update(press(1));
}

// Shoots at her from where the runner stands, jumping while she bows.
void duel(World& w, int frames, bool jumps)
{
  for (int f = 0; f < frames; ++f)
  {
    const bool ground = w.player().state == PlayerState::OnGround;
    w.update(press(0, jumps && ground && w.mother().bowed(), f % 2 == 0));
  }
}

} // namespace

int main()
{
  SDL_Init(0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  {
    Renderer renderer(sdl);
    const Theme& theme = themeByIndex(themeIndexForKey("hive_mother"));
    const Art art = Art::build(theme, renderer);
    auto make = [&](const std::string& text) { return std::make_shared<const Level>(Level::parse(text)); };

    {
      // Asleep until you come in; then the door shuts and she wakes.
      const auto level = make(testLevel("", true));
      World w(level, 0, theme, art);
      idle(w, 4);
      check(w.mother().on && w.mother().phase == MotherPhase::Asleep, "@ hive_mother: asleep on her throne");
      check(!w.motherFight() && w.canSave(), "no fight yet, saving allowed");
      check(walkIn(w), "walking in wakes her");
      check(w.map().solid(10 * 2, 13 * 2), "the door shut behind");
      check(!w.canSave(), "no saving in the fight");
      idle(w, 64);
      check(w.mother().phase == MotherPhase::Crown, "she rises: the crown phase");
      check(countAlive(w, EnemyKind::SporeNurse) == 2, "two Spore Nurses drift in");
      check(!w.exitPowered(), "no exit while she lives");

      // Standing shots glance off her; jump shots into the bowed crown hurt.
      approach(w, 8);
      const int hp0 = w.motherHp();
      duel(w, 170, false);
      check(w.motherHp() >= hp0, "standing shots glance off her chitin");
      check(countAll(w, EnemyKind::EggGuard) >= 1, "she laid an egg");
      const int hp1 = w.motherHp();
      duel(w, 170, true);
      std::printf("     her hp %d -> %d with jump shots\n", hp1, w.motherHp());
      check(w.motherHp() < hp1, "jump shots into her bowed crown hurt");
    }

    {
      // Spore Nurses heal her.
      const auto level = make(testLevel("", true));
      World w(level, 0, theme, art);
      idle(w, 2);
      walkIn(w);
      approach(w, 8);
      for (int f = 0; f < 400 && w.motherHp() > w.motherHpMax() - 4; ++f)
      {
        const bool ground = w.player().state == PlayerState::OnGround;
        w.update(press(0, ground && w.mother().bowed(), f % 2 == 0));
      }
      const int hurt = w.motherHp();
      check(hurt < w.motherHpMax(), "hurt her");
      check(w.mother().phase == MotherPhase::Crown && countAlive(w, EnemyKind::SporeNurse) == 2, "the nurses still live");
      // Walk out of her reach to the door and wait.
      int healed = 0;
      for (int f = 0; f < 700 && w.mother().phase == MotherPhase::Crown; ++f)
      {
        w.update(press(-1));
        healed = std::max(healed, w.motherHp() - hurt);
      }
      check(healed > 0, "the nurses heal her");
    }

    {
      // Breathing in, she pulls you toward her; charging, she runs to the
      // wall and stands dazed against it.
      const auto level = make(testLevel("", true));
      World w(level, 0, theme, art);
      idle(w, 2);
      walkIn(w);
      approach(w, 8);
      for (int f = 0; f < 3000 && w.mother().phase == MotherPhase::Crown; ++f)
      {
        const bool ground = w.player().state == PlayerState::OnGround;
        w.update(press(0, ground && w.mother().bowed(), f % 2 == 0));
      }
      check(w.mother().phase == MotherPhase::Inhale, "the crown broken: she breathes in");
      for (int f = 0; f < 400 && !w.mother().inhaling(); ++f)
        w.update(press(-1));
      const int x0 = w.player().x;
      idle(w, 24);
      check(w.mother().inhaling() && w.player().x > x0, "she pulls you toward her");
      for (int f = 0; f < 4000 && w.mother().phase == MotherPhase::Inhale; ++f)
        w.update(press(0, false, f % 2 == 0));
      check(w.mother().phase == MotherPhase::Charge, "the sacs burst: she charges");
      bool dazed = false;
      for (int f = 0; f < 200 && !dazed; ++f)
      {
        w.update(press(0));
        dazed = w.mother().run == 2;
      }
      const auto& m = w.mother();
      check(dazed && (int(m.x) == m.arena.x || int(m.x) == m.arena.x + m.arena.w - HiveMother::kW),
        "she runs to the wall and stands dazed");
      // (God Mode: no ledges here to dodge her on.) Walk up to her while
      // she is dazed and shoot her back.
      w.cheat(Cheat::God);
      for (int f = 0; f < 3000 && w.motherFight(); ++f)
      {
        const int mx = int(m.x), px = w.player().x;
        const int toHer = mx + HiveMother::kW / 2 < px ? -1 : 1;
        const int gap = toHer > 0 ? mx - (px + 3) : px - (mx + HiveMother::kW);
        const bool facing = (toHer < 0) == (w.player().facing < 0);
        const int dx = !facing || gap > 8 ? toHer : 0;
        w.update(press(dx, false, f % 2 == 0 && m.run == 2));
      }
      idle(w, 100);
      check(m.phase == MotherPhase::Done, "she falls");
      check(w.exitPowered(), "the exit opens");
      check(!w.canSave(), "(no saving after her either: the exit is right there)");
    }

    {
      // The Star Seed: a tap is a small star, a full charge goes through two
      // eggs in a row and breaks both.
      const auto level = make(testLevel("@ egg_guard 11 13\n@ egg_guard 14 13\n", false, 4));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      check(w.player().weapon == Weapon::Proto && w.player().proto == int(ProtoId::StarSeed), "holding the Star Seed");
      idle(w, 2);
      for (int f = 0; f < 20; ++f)
        w.update(press(0, false, true));
      w.update(press(0));
      idle(w, 60);
      check(!w.enemies()[0].alive && !w.enemies()[1].alive, "a full star goes through both eggs");
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  if (gFailures)
    std::printf("%d FAILED\n", gFailures);
  else
    std::printf("ALL OK\n");
  return gFailures ? 1 : 0;
}
