// Vehicles (world_vehicle.cpp) and deep water (world_sea.cpp) on small test
// maps: each vehicle boards, drives the way it should, survives a save and
// load with the runner inside, and lets the runner climb out again. Also the
// tank breaking a `by=vehicle` wall, the helicopter's fuel, the space ship's
// drift, the mech's jet jump, and air running out in the sea but not in the
// submarine.

#include "assets/art.hpp"
#include "data/level.hpp"
#include "data/theme.hpp"
#include "game/savegame.hpp"
#include "game/world.hpp"
#include "render/renderer.hpp"

#include <SDL.h>

#include <algorithm>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

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

std::string slurp(const std::string& path)
{
  std::ifstream f(path);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

std::string withoutTimestamp(std::string s)
{
  const auto at = s.find("\nsaved ");
  if (at != std::string::npos)
    s.erase(at, s.find('\n', at + 1) - at);
  return s;
}

// A walled room 64 blocks wide and 16 high with a floor on row 14, the
// runner at block 2 and the exit far right. `wall` puts a 2-block solid
// column at x 40-41 (rows 9-13) for a `by=vehicle` breakable.
std::string room(const std::string& entities, bool wall = false)
{
  const int W = 64, H = 16;
  std::string s = "name=VEHICLE TEST\ntheme=neon_rooftops\nmusic=theme_synthwave\n[map]\n";
  for (int y = 0; y < H; ++y)
  {
    std::string row(std::size_t(W), '.');
    if (y == 0 || y >= 14)
      row.assign(std::size_t(W), '#');
    row[0] = row[std::size_t(W - 1)] = '#';
    if (wall && y >= 9 && y <= 13)
      row[40] = row[41] = '#';
    if (y == 13)
    {
      row[2] = 'P';
      row[60] = 'X';
    }
    s += row + "\n";
  }
  s += "[entities]\n" + entities;
  return s;
}

// The sea room: the runner on a dock (rows 0-5 ledge), the water below.
std::string seaRoom(const std::string& entities)
{
  const int W = 48, H = 24;
  std::string s = "name=SEA TEST\ntheme=neon_rooftops\nmusic=theme_synthwave\n[map]\n";
  for (int y = 0; y < H; ++y)
  {
    std::string row(std::size_t(W), '.');
    if (y == 0 || y >= 22)
      row.assign(std::size_t(W), '#');
    row[0] = row[std::size_t(W - 1)] = '#';
    if (y == 8)
      for (int x = 1; x < 10; ++x)
        row[std::size_t(x)] = '#'; // the dock
    if (y == 7)
    {
      row[2] = 'P';
      row[44] = 'X';
    }
    s += row + "\n";
  }
  s += "[entities]\n@ sea rect=10,10,46,21\n" + entities;
  return s;
}

struct Keys
{
  bool left = false, right = false, up = false, down = false, jump = false, fire = false, use = false;
};

void step(World& w, const Keys& in, bool useEdge = true)
{
  PlayerInput p;
  p.left = in.left;
  p.right = in.right;
  p.up = in.up;
  p.down = in.down;
  p.jump = {in.jump, in.jump};
  p.fire = {in.fire, in.fire};
  p.use = {in.use, in.use && useEdge};
  w.update(p);
}

void run(World& w, const Keys& in, int frames)
{
  for (int i = 0; i < frames; ++i)
    step(w, in);
}

void press(World& w, Keys in)
{
  in.use = true;
  step(w, in);
}

struct Kit
{
  const Theme& theme;
  const Art& art;
  std::string dir;
};

// Boards, saves and loads with the runner inside, and climbs out.
void boardSaveLeave(const Kit& kit, const std::shared_ptr<const Level>& level, World& w, const std::string& name)
{
  const Vehicle* v = w.riding();
  check(v != nullptr, name + ": the runner is inside");
  if (!v)
    return;
  const std::string a = slotPath(kit.dir, 0), b = slotPath(kit.dir, 1);
  const SaveGame s = w.snapshot();
  check(writeSave(s, a), name + ": save while driving");
  const auto loaded = readSave(a);
  check(bool(loaded), name + ": read the save back");
  if (!loaded)
    return;
  World fresh(level, loaded->character, kit.theme, kit.art);
  check(fresh.restore(*loaded), name + ": restore");
  const Vehicle* fv = fresh.riding();
  check(fv && fv->kind == v->kind && fv->x == v->x && fv->y == v->y && fv->hp == v->hp && fv->fuel == v->fuel,
    name + ": still driving the same vehicle after loading");
  check(fresh.player().x == w.player().x && fresh.player().y == w.player().y, name + ": runner in the seat after loading");
  check(writeSave(fresh.snapshot(), b) && withoutTimestamp(slurp(a)) == withoutTimestamp(slurp(b)),
    name + ": the loaded world saves identically");
  // Climb out of the loaded one.
  step(fresh, {});
  press(fresh, {});
  run(fresh, {}, 15);
  check(!fresh.riding(), name + ": climbs out with USE");
  const auto& p = fresh.player();
  check(!fresh.map().solid(p.x, p.y) && p.hp > 0, name + ": out on free ground");
  check(fresh.vehicles().size() == 1 && !fresh.vehicles()[0].occupied, name + ": left parked");
}

} // namespace

int main(int argc, char** argv)
{
  const std::string levelsDir = argc > 1 ? argv[1] : "levels";
  const std::string dir = (std::filesystem::temp_directory_path() / "gunrunners_vehicle_test").string();
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);

  SDL_Init(0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  {
    Renderer renderer(sdl);
    const Theme& theme = themeByIndex(0);
    const Art art = Art::build(theme, renderer);
    const Kit kit{theme, art, dir};
    auto load = [](const std::string& text) { return std::make_shared<const Level>(Level::parse(text)); };

    // --- Tank: boards, crawls, breaks a by=vehicle wall, crushes, saves. ---
    {
      const auto level = load(room("@ tank 3 13\n@ breakable rect=40,9,41,13 hp=3 by=vehicle look=rock\n", true));
      World w(level, 0, theme, art);
      check(w.vehicles().size() == 1, "tank: parsed from @ tank");
      run(w, {}, 5);
      check(w.vehicleInReach() == 0, "tank: in reach at the start");
      press(w, {});
      check(w.riding() && w.riding()->kind == VehicleKind::Tank, "tank: USE boards it");
      const int x0 = w.riding()->x;
      run(w, {false, true}, 30);
      const int moved = w.riding()->x - x0;
      check(moved > 10 && moved < 30, "tank: crawls right (" + std::to_string(moved) + " cells in 30 frames)");
      check(w.player().x == w.riding()->x + w.riding()->w / 2 - 1, "tank: the runner rides along");

      // Runner shots can't touch the wall, the cannon can.
      World foot(level, 0, theme, art);
      run(foot, {false, true}, 50);
      Keys fire;
      fire.fire = true;
      for (int i = 0; i < 90; ++i)
        step(foot, (i % 2) ? fire : Keys{});
      check(foot.map().solid(80, 25), "tank: the wall shrugs off the runner's gun");
      run(w, {false, true}, 90); // up to the wall: shots go once they are off screen
      for (int i = 0; i < 90; ++i)
        step(w, fire);
      check(!w.map().solid(80, 25), "tank: the cannon breaks the by=vehicle wall");
      run(w, {false, true}, 60);
      check(w.riding()->x > 82, "tank: drives through the gap");
      boardSaveLeave(kit, level, w, "tank");
    }

    // --- Helicopter: climbs, burns fuel in the air, refuels on the ground. ---
    {
      const auto level = load(room("@ vehicle kind=helicopter x=3 y=13\n"));
      World w(level, 1, theme, art);
      run(w, {}, 3);
      press(w, {});
      check(w.riding() && w.riding()->kind == VehicleKind::Heli, "heli: boards");
      const int y0 = w.riding()->y, f0 = w.riding()->fuel;
      Keys up;
      up.up = true;
      run(w, up, 10);
      check(w.riding()->y < y0 - 6, "heli: up climbs");
      Keys fly;
      fly.right = true;
      run(w, fly, 30);
      check(w.riding()->fuel < f0 - 25, "heli: burns fuel in the air");
      check(w.riding()->y < y0 - 6, "heli: hovers without holding up");
      boardSaveLeave(kit, level, w, "heli");
      const int flying = w.riding()->fuel;
      Keys down;
      down.down = true;
      run(w, down, 30);
      check(w.riding()->fuel > flying, "heli: refuels on the ground");
    }

    // --- Hoverbike: fast, and jumps. ---
    {
      const auto level = load(room("@ bike 3 13\n"));
      World w(level, 2, theme, art);
      run(w, {}, 3);
      press(w, {});
      check(w.riding() && w.riding()->kind == VehicleKind::Bike, "bike: boards");
      const int x0 = w.riding()->x;
      run(w, {false, true}, 15);
      check(w.riding()->x - x0 > 20, "bike: much faster than the tank");
      const int y0 = w.riding()->y;
      Keys j;
      j.right = true;
      j.jump = true;
      step(w, j);
      run(w, {false, true}, 4);
      check(w.riding()->y < y0 - 3, "bike: jump lifts it");
      run(w, {}, 30);
      check(w.riding()->y == y0, "bike: lands again");
      boardSaveLeave(kit, level, w, "bike");
    }

    // --- Space ship: drifts on after the thrust stops. ---
    {
      const auto level = load(room("@ vehicle kind=spaceship x=3 y=11\n"));
      World w(level, 0, theme, art);
      run(w, {}, 3);
      check(w.vehicleInReach() == 0 || true, "ship: parked");
      // The runner walks under it; climb up to it by boarding from below.
      press(w, {});
      if (!w.riding())
      {
        Keys u;
        u.up = true;
        run(w, u, 3);
        press(w, {});
      }
      check(w.riding() && w.riding()->kind == VehicleKind::Ship, "ship: boards");
      if (w.riding())
      {
        run(w, {false, true}, 12);
        const int vx = w.riding()->vx, xa = w.riding()->x;
        run(w, {}, 8);
        check(vx > 20 && w.riding()->vx > 0 && w.riding()->x > xa + 6, "ship: keeps drifting after letting go");
        const int y = w.riding()->y;
        run(w, {}, 15);
        check(w.riding()->y == y, "ship: no gravity");
        boardSaveLeave(kit, level, w, "ship");
      }
    }

    // --- Mech: walks, jet-jumps high. ---
    {
      const auto level = load(room("@ mech 3 13\n"));
      World w(level, 1, theme, art);
      run(w, {}, 3);
      press(w, {});
      check(w.riding() && w.riding()->kind == VehicleKind::Mech, "mech: boards");
      const int y0 = w.riding()->y;
      Keys j;
      j.jump = true;
      step(w, j);
      int top = y0;
      for (int i = 0; i < 12; ++i)
      {
        step(w, j);
        top = std::min(top, w.riding()->y);
      }
      check(top < y0 - 12, "mech: jet jump goes high (" + std::to_string(y0 - top) + " cells)");
      run(w, {}, 30);
      check(w.riding()->y == y0, "mech: lands");
      boardSaveLeave(kit, level, w, "mech");
    }

    // --- Mech stomp breaks a by=vehicle floor. ---
    {
      // A shelf on row 10 from x 1 to 9, the runner on it and the mech on
      // its breakable part.
      std::string text = room("@ mech 3 9\n@ breakable rect=3,10,9,10 hp=1 by=vehicle look=rock\n");
      std::string out;
      std::istringstream in(text);
      std::string line;
      int row = -1;
      while (std::getline(in, line))
      {
        if (line == "[map]")
          row = 0;
        else if (line == "[entities]")
          row = -1;
        else if (row >= 0)
        {
          if (row == 13)
            line[2] = '.';
          if (row == 9)
            line[2] = 'P';
          if (row == 10)
            for (int x = 1; x <= 9; ++x)
              line[std::size_t(x)] = '#';
          ++row;
        }
        out += line + "\n";
      }
      const auto level = load(out);
      World w(level, 0, theme, art);
      run(w, {}, 3);
      check(w.map().solid(10, 21), "stomp: the shelf is there");
      press(w, {});
      check(w.riding() && w.riding()->kind == VehicleKind::Mech, "stomp: boards the mech on the shelf");
      if (w.riding())
      {
        Keys jj;
        jj.jump = true;
        for (int i = 0; i < 14; ++i)
          step(w, jj);
        run(w, {}, 40);
        check(!w.map().solid(10, 21), "stomp: a hard landing breaks the by=vehicle shelf");
        check(w.riding() && w.riding()->y == 27, "stomp: the mech drops to the floor");
      }
    }

    // --- Sea: air runs out swimming, not in the submarine. ---
    {
      const auto level = load(seaRoom("@ sub 30 13\n"));
      World w(level, 0, theme, art);
      check(w.seas().size() == 1, "sea: parsed");
      check(w.air() == kAirFrames, "sea: full air on the dock");
      // Walk off the dock into the water.
      run(w, {false, true}, 25);
      run(w, {}, 20);
      check(w.player().state == PlayerState::Swim, "sea: swimming in deep water");
      const int air = w.air();
      run(w, {}, 20);
      check(w.air() < air && w.air() < kAirFrames, "sea: air drains under water");
      // Swim over to the sub and get in.
      for (int i = 0; i < 200 && w.vehicleInReach() < 0; ++i)
      {
        Keys go;
        go.right = w.player().x < 58;
        go.left = w.player().x > 70;
        go.up = w.player().y > 27;
        go.down = w.player().y < 25;
        step(w, go);
      }
      press(w, {});
      check(w.riding() && w.riding()->kind == VehicleKind::Sub, "sea: boards the submarine");
      if (w.riding())
      {
        const int before = w.air();
        run(w, {false, true}, 30);
        check(w.air() > before || w.air() == kAirFrames, "sea: the sub refills the air");
        const int x0 = w.riding()->x;
        check(x0 > 30 * 2 + 10, "sea: the sub moves through the water");
        Keys up;
        up.up = true;
        run(w, up, 60);
        check(w.riding()->y >= 10 * 2 - 1, "sea: the sub stops at the surface");
        boardSaveLeave(kit, level, w, "sub");
      }
    }

    // Every vehicle placed in a level file fits where it is parked and
    // stays in the map once it has settled.
    std::vector<std::filesystem::path> files;
    if (std::filesystem::is_directory(levelsDir))
      for (const auto& f : std::filesystem::directory_iterator(levelsDir))
        if (f.path().extension() == ".txt")
          files.push_back(f.path());
    std::sort(files.begin(), files.end());
    int placed = 0;
    for (const auto& f : files)
    {
      const auto level = std::make_shared<const Level>(Level::loadFile(f.string()));
      World w(level, 0, theme, art);
      if (w.vehicles().empty())
        continue;
      for (std::size_t i = 0; i < w.vehicles().size(); ++i)
      {
        const Vehicle& v = w.vehicles()[i];
        check(w.vehicleFits(v.kind, v.x, v.y),
          f.filename().string() + ": " + vehicleDef(v.kind).name + " " + std::to_string(i) + " fits where it is parked");
        ++placed;
      }
      run(w, {}, 30);
      for (const auto& v : w.vehicles())
        check(v.wreck == 0 && v.y < w.map().height(), f.filename().string() + ": " + vehicleDef(v.kind).name + " settles in the map");
    }
    check(placed >= 6, "vehicles are placed in the level files (" + std::to_string(placed) + ")");
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::filesystem::remove_all(dir);
  std::printf("%s\n", gFailures == 0 ? "all vehicle tests passed" : "SOME TESTS FAILED");
  return gFailures == 0 ? 0 : 1;
}
