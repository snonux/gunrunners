// Ladders, played by hand in small rooms: up a ladder past the end of a
// ledge and off onto it; down a ladder from the floor above, through a hole
// one block wide with a one-way block beside it, and back up. Then every
// level file: a runner on a ladder is 3 cells wide and hangs from the
// ladder's left column, so it fills the right half of the block LEFT of the
// ladder too; that block must never be solid, or the runner is stuck there.

#include "assets/art.hpp"
#include "data/level.hpp"
#include "data/theme.hpp"
#include "game/world.hpp"
#include "render/renderer.hpp"

#include <SDL.h>

#include <cstdio>
#include <dirent.h>
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
// runner at (px, py), then `rows` (y, x0, text) written over it.
struct Patch
{
  int y, x;
  std::string text;
};

std::string room(int px, int py, const std::vector<Patch>& patches)
{
  std::vector<std::string> map(24, std::string(40, '.'));
  for (int x = 0; x < 40; ++x)
    map[0][std::size_t(x)] = map[22][std::size_t(x)] = map[23][std::size_t(x)] = '#';
  for (int y = 0; y < 24; ++y)
    map[std::size_t(y)][0] = map[std::size_t(y)][39] = '#';
  for (const auto& p : patches)
    map[std::size_t(p.y)].replace(std::size_t(p.x), p.text.size(), p.text);
  map[std::size_t(py)][std::size_t(px)] = 'P';
  std::string s = "name=LADDER TEST\nepisode=1\ntheme=glass_canyon\nweapon=spark_disc\n[map]\n";
  for (const auto& row : map)
    s += row + "\n";
  return s + "[entities]\n";
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

// A ladder column (block x, rows y0..y1) in a level's map.
struct LadderRun
{
  int x, y0, y1;
};

bool blocks(Tile t)
{
  return t == Tile::Solid || t == Tile::Grate || t == Tile::ForceField; // a spike's half is open
}

// Every ladder block whose left neighbour the runner can't hang in, as
// "file x,y" lines.
std::vector<std::string> badLadders(const std::string& dir)
{
  std::vector<std::string> bad;
  DIR* d = opendir(dir.c_str());
  if (!d)
    return {"cannot open " + dir};
  std::vector<std::string> files;
  while (const dirent* e = readdir(d))
  {
    const std::string n = e->d_name;
    if (n.size() > 4 && n.compare(n.size() - 4, 4, ".txt") == 0)
      files.push_back(n);
  }
  closedir(d);
  for (const auto& n : files)
  {
    const Level level = Level::loadFile(dir + "/" + n);
    for (int y = 0; y < level.height; ++y)
      for (int x = 0; x < level.width; ++x)
        if (level.at(x, y) == Tile::Ladder && (x == 0 || blocks(level.at(x - 1, y))))
          bad.push_back(n + " " + std::to_string(x) + "," + std::to_string(y));
  }
  return bad;
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
    Keys up, down, jumpLeft, jumpRight;
    up.up = true;
    down.down = true;
    jumpLeft.jump = jumpLeft.left = true;
    jumpRight.jump = jumpRight.right = true;
    {
      // A ledge (row 14) ending in a one-way block at x 9, and a ladder at
      // x 10 from the floor up past it to row 12 (or only up to the ledge's
      // own row): up to the top, then a jump to the left lands on the ledge.
      // The same with a wall: a pit with a ladder one block off its side.
      for (int shape = 0; shape < 3; ++shape)
      {
        const int top = shape == 0 ? 12 : 14;
        std::vector<Patch> rows{{14, 1, shape == 2 ? "########" : "########="}};
        for (int y = 15; shape == 2 && y <= 21; ++y)
          rows.push_back({y, 1, "########"});
        for (int y = top; y <= 21; ++y)
          rows.push_back({y, 10, "H"});
        const auto level = std::make_shared<const Level>(Level::parse(room(10, 21, rows)));
        for (int c = 0; c < 6; ++c)
        {
          World w(level, c, theme, art);
          Keys prev;
          hold(w, up, 80, prev);
          const auto& p = w.player();
          const bool atTop = p.state == PlayerState::Ladder && p.box().top() == top * 2;
          hold(w, jumpLeft, 40, prev);
          hold(w, Keys{}, 20, prev);
          char what[96];
          std::snprintf(what, sizeof what, "runner %d: up the ladder%s, off onto the %s", c,
            shape == 0 ? " past the ledge" : " to the top", shape == 2 ? "pit's side" : "ledge");
          check(atTop && p.state == PlayerState::OnGround && p.y == 27 && p.box().right() <= 19, what);
        }
      }
    }
    {
      // A floor at row 12 with a hole one block wide (x 10) and a one-way
      // block beside it (x 9); a ladder from the hole down to the floor.
      // Standing over the hole, down climbs down to the bottom; up climbs
      // back, and a jump to the right lands on the floor again.
      std::vector<Patch> rows{{12, 1, "########=H#############################"}};
      rows[0].text.resize(38);
      for (int y = 13; y <= 21; ++y)
        rows.push_back({y, 10, "H"});
      const auto level = std::make_shared<const Level>(Level::parse(room(10, 11, rows)));
      for (int c = 0; c < 6; ++c)
      {
        World w(level, c, theme, art);
        Keys prev;
        hold(w, Keys{}, 10, prev);
        const auto& p = w.player();
        const bool standing = p.state == PlayerState::OnGround && p.y == 23;
        hold(w, down, 100, prev);
        const bool bottom = p.state == PlayerState::OnGround && p.y == 43 && p.x == 19;
        hold(w, up, 100, prev);
        const bool top = p.state == PlayerState::Ladder;
        hold(w, jumpRight, 40, prev);
        hold(w, Keys{}, 20, prev);
        char what[96];
        std::snprintf(what, sizeof what, "runner %d: down through the hole to the floor below", c);
        check(standing && bottom, what);
        std::snprintf(what, sizeof what, "runner %d: back up the ladder and out onto the floor", c);
        check(top && p.state == PlayerState::OnGround && p.y == 23 && p.box().left() >= 22, what);
      }
    }
    {
      // Down on plain floor still crouches; down over a ladder in a floor
      // that has no room beside it (both sides solid) stays put.
      std::vector<Patch> rows{{12, 1, "#########H############################"}};
      for (int y = 13; y <= 21; ++y)
        rows.push_back({y, 10, "H"});
      {
        World w(std::make_shared<const Level>(Level::parse(room(20, 11, rows))), 0, theme, art);
        Keys prev;
        hold(w, Keys{}, 10, prev);
        hold(w, down, 10, prev);
        check(w.player().visual == PlayerVisual::Crouching, "down on plain floor crouches");
      }
      {
        World w(std::make_shared<const Level>(Level::parse(room(10, 11, rows))), 0, theme, art);
        Keys prev;
        hold(w, Keys{}, 10, prev);
        hold(w, down, 30, prev);
        check(w.player().y == 23 && w.player().state == PlayerState::OnGround,
          "down over a ladder with no room beside it stays on the floor");
      }
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();

  for (const auto& b : badLadders(argv[1]))
    std::printf("     ladder with a solid block on its left: %s\n", b.c_str());
  check(badLadders(argv[1]).empty(), "every level: no ladder has a solid block on its left");

  std::printf("%s\n", gFailures == 0 ? "all ladder tests passed" : "ladder tests FAILED");
  return gFailures == 0 ? 0 : 1;
}
