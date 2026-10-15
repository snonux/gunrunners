// The weapon switch: every gun picked up stays carried (three guns plus the
// blaster at most), a pickup still puts the new gun in hand, a gun that
// runs dry still drops back to the blaster, and the spares survive a save.
// A test room drives the real world logic with edge-correct input.

#include "assets/art.hpp"
#include "data/characters.hpp"
#include "data/level.hpp"
#include "data/theme.hpp"
#include "game/savegame.hpp"
#include "game/world.hpp"
#include "render/renderer.hpp"

#include <SDL.h>

#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>

using namespace gr;

namespace
{

int gFailures = 0;

void check(bool ok, const std::string& what)
{
  std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
  if (!ok)
    ++gFailures;
}

// A walled room, floor on row 14, the runner at block 2 of row 13 and
// `boxes` from block 6 on, four blocks apart.
std::shared_ptr<const Level> room(const std::string& boxes)
{
  const int W = 64, H = 16;
  std::string s = "name=WEAPONS TEST\ntheme=neon_rooftops\nmusic=theme_synthwave\nweapon=pulse_pistol\n[map]\n";
  for (int y = 0; y < H; ++y)
  {
    std::string row(std::size_t(W), '.');
    if (y == 0 || y >= 14)
      row.assign(std::size_t(W), '#');
    row[0] = row[std::size_t(W - 1)] = '#';
    if (y == 13)
    {
      row[2] = 'P';
      row[std::size_t(W - 4)] = 'X';
      for (std::size_t i = 0; i < boxes.size(); ++i)
        row[6 + i * 4] = boxes[i];
    }
    s += row + "\n";
  }
  s += "[entities]\n";
  return std::make_shared<const Level>(Level::parse(s));
}

struct Keys
{
  bool right = false, down = false, fire = false;
};

struct Driver
{
  World& w;
  Keys prev{};
  void step(const Keys& in)
  {
    PlayerInput p;
    p.right = in.right;
    p.down = in.down;
    p.fire = {in.fire, in.fire && !prev.fire};
    prev = in;
    w.update(p);
  }
  void run(const Keys& in, int frames)
  {
    for (int i = 0; i < frames; ++i)
      step(in);
  }
};

// Shoots the next box open (crouched) and walks on until its gun is in hand.
bool takeNext(Driver& d, Weapon want, int proto = -1)
{
  auto inHand = [&] {
    const auto& p = d.w.player();
    return p.weapon == want && (want != Weapon::Proto || p.proto == proto);
  };
  for (int i = 0; i < 400 && !inHand(); ++i)
  {
    Keys k;
    k.fire = i % 2 == 0 && i < 8; // a few shots, not the whole magazine
    k.down = i < 40; // crouched, a shot hits the box on the floor
    k.right = i >= 40;
    d.step(k);
  }
  d.run({}, 2);
  return inHand();
}

std::string guns(const World& w)
{
  const auto& p = w.player();
  auto name = [](Weapon g, int proto) {
    return g == Weapon::Proto && proto >= 0 ? std::string(protoDef(proto).name) : std::string(weaponName(g));
  };
  std::string s = name(p.weapon, p.proto);
  for (int i = 0; i < p.spareCount; ++i)
    s += ", " + name(p.spares[std::size_t(i)].weapon, p.spares[std::size_t(i)].proto);
  return s;
}

} // namespace

int main()
{
  const std::string dir = (std::filesystem::temp_directory_path() / "gunrunners_weapons_test").string();
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);

  SDL_Init(0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  {
    Renderer renderer(sdl);
    const Theme& theme = themeByIndex(0);
    const Art art = Art::build(theme, renderer);
    const int pistol = protoIndex("pulse_pistol");

    // Dash starts with the blaster alone: nothing to switch to.
    {
      const auto level = room("LRFW");
      World w(level, 0, theme, art);
      Driver d{w};
      d.run({}, 3);
      check(guns(w) == "BLASTER", "Dash starts with the blaster only (" + guns(w) + ")");
      check(!w.nextWeapon(), "nothing to switch to with the blaster alone");

      check(takeNext(d, Weapon::Laser), "the laser box puts the laser in hand (" + guns(w) + ")");
      check(guns(w) == "LASER, BLASTER", "the blaster is kept (" + guns(w) + ")");
      check(takeNext(d, Weapon::Rocket), "the rocket box puts rockets in hand (" + guns(w) + ")");
      check(guns(w) == "ROCKETS, BLASTER, LASER", "the laser is kept (" + guns(w) + ")");
      check(takeNext(d, Weapon::Flame), "the flamer box puts the flamer in hand (" + guns(w) + ")");
      check(guns(w) == "FLAMER, BLASTER, LASER, ROCKETS", "three guns plus the blaster (" + guns(w) + ")");

      const int flame = w.player().ammo;
      check(w.nextWeapon() && guns(w) == "BLASTER, LASER, ROCKETS, FLAMER", "switch: flamer to the blaster (" + guns(w) + ")");
      check(w.nextWeapon() && guns(w) == "LASER, ROCKETS, FLAMER, BLASTER", "switch: the blaster to the laser (" + guns(w) + ")");
      check(w.nextWeapon() && guns(w) == "ROCKETS, FLAMER, BLASTER, LASER", "switch: the laser to rockets (" + guns(w) + ")");
      check(w.nextWeapon() && guns(w) == "FLAMER, BLASTER, LASER, ROCKETS", "switch: back round to the flamer (" + guns(w) + ")");
      check(w.player().ammo == flame, "the flamer kept its ammo while stowed");

      // Full: the prototype takes the place of the gun in hand.
      check(takeNext(d, Weapon::Proto, pistol), "the W box puts the prototype in hand (" + guns(w) + ")");
      check(guns(w) == "PULSE PISTOL, BLASTER, LASER, ROCKETS", "full: the flamer in hand made room (" + guns(w) + ")");

      // A save keeps the spares, in order.
      const std::string path = slotPath(dir, 1);
      check(writeSave(w.snapshot(), path), "saves");
      const auto loaded = readSave(path);
      World fresh(level, 0, theme, art);
      check(loaded && fresh.restore(*loaded), "restores");
      check(guns(fresh) == guns(w), "the spares come back from a save (" + guns(fresh) + ")");
    }

    // With the blaster in hand and three guns carried, the gun held last makes room.
    {
      World w(room("LRFW"), 0, theme, art);
      Driver d{w};
      d.run({}, 3);
      takeNext(d, Weapon::Laser);
      takeNext(d, Weapon::Rocket);
      takeNext(d, Weapon::Flame);
      w.nextWeapon();
      check(guns(w) == "BLASTER, LASER, ROCKETS, FLAMER", "blaster in hand (" + guns(w) + ")");
      takeNext(d, Weapon::Proto, pistol);
      check(guns(w) == "PULSE PISTOL, LASER, ROCKETS, BLASTER", "the flamer (held last) made room (" + guns(w) + ")");
    }

    // Running dry drops back to the blaster, as before; the other spares stay.
    {
      World w(room("L"), 1, theme, art); // Rocco: rockets
      Driver d{w};
      d.run({}, 3);
      check(guns(w) == "ROCKETS, BLASTER", "Rocco starts with rockets and the blaster (" + guns(w) + ")");
      takeNext(d, Weapon::Laser);
      check(guns(w) == "LASER, BLASTER, ROCKETS", "Rocco keeps his rockets for the laser (" + guns(w) + ")");
      for (int i = 0; i < 2000 && w.player().weapon == Weapon::Laser; ++i)
      {
        Keys k;
        k.fire = i % 2 == 0;
        d.step(k);
      }
      check(guns(w) == "BLASTER, ROCKETS", "out of laser ammo: back to the blaster, rockets kept (" + guns(w) + ")");
    }

    // No switch while fire is held.
    {
      World w(room(""), 1, theme, art);
      Driver d{w};
      Keys fire;
      fire.fire = true;
      d.run(fire, 3);
      check(!w.nextWeapon() && w.player().weapon == Weapon::Rocket, "no switch while firing");
      d.run({}, 1);
      check(w.nextWeapon() && w.player().weapon == Weapon::Normal, "switches once fire is let go");
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf("%d failure(s)\n", gFailures);
  return gFailures == 0 ? 0 : 1;
}
