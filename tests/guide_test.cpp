// The guide arrow (ASSISTS: GUIDE ARROW), in small test rooms and on every
// campaign level: off until switched on and in bonus levels; points the way
// along a route (round a ceiling to a ladder, not straight up into it) to
// the nearest gem, box or bot still missing, then to the exit; a runner who
// only follows the arrow collects what it points at. On the real levels it
// finds something to point at from the start, quickly.
//
// The guide keeps its grids and search scratch between logic frames: what
// it says when it works its answer out must be what a search from scratch
// (World::guideFresh) says, as the runner moves, things are taken, the
// card is picked up and a force field goes down, on the test rooms and
// along scripted runs through campaign levels.

#include "assets/art.hpp"
#include "data/campaign.hpp"
#include "data/level.hpp"
#include "data/theme.hpp"
#include "game/guide.hpp"
#include "game/world.hpp"
#include "render/renderer.hpp"

#include <SDL.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <memory>
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

// A walled room 64 blocks wide and 16 high, floor on row 14, the runner at
// block `px` of row 13, the exit near the right wall.
std::string room(const std::string& header, const std::function<void(int, std::string&)>& edit = {}, int px = 2)
{
  const int W = 64, H = 16;
  std::string s = "name=GUIDE TEST\ntheme=neon_rooftops\nmusic=theme_synthwave\n" + header + "[map]\n";
  for (int y = 0; y < H; ++y)
  {
    std::string row(std::size_t(W), '.');
    if (y == 0 || y >= 14)
      row.assign(std::size_t(W), '#');
    row[0] = row[std::size_t(W - 1)] = '#';
    if (y == 13)
    {
      row[std::size_t(px)] = 'P';
      row[std::size_t(W - 4)] = 'X';
    }
    if (edit)
      edit(y, row);
    s += row + "\n";
  }
  s += "[entities]\n";
  return s;
}

std::shared_ptr<const Level> load(const std::string& text)
{
  return std::make_shared<const Level>(Level::parse(text));
}

void guideOn(World& w)
{
  World::PlayOptions o = w.playOptions();
  o.guide = true;
  w.setPlayOptions(o);
}

const char* kindName(World::GuideKind k)
{
  switch (k)
  {
    case World::GuideKind::Item:
      return "item";
    case World::GuideKind::Enemy:
      return "bot";
    case World::GuideKind::Exit:
      return "exit";
    default:
      return "none";
  }
}

std::string describe(const World::Guide& g)
{
  char buf[128];
  std::snprintf(buf, sizeof(buf), "%s dx %d dy %d at %d,%d%s%s left %d", kindName(g.kind), g.dx, g.dy, g.tx, g.ty,
    g.routed ? " routed" : "", g.walkable ? " walkable" : "", g.left);
  return buf;
}

bool same(const World::Guide& a, const World::Guide& b)
{
  return a.kind == b.kind && a.dx == b.dx && a.dy == b.dy && a.tx == b.tx && a.ty == b.ty && a.routed == b.routed &&
    a.walkable == b.walkable && a.fire == b.fire && a.left == b.left && a.fromX == b.fromX && a.fromY == b.fromY &&
    a.frame == b.frame;
}

// Every cell of two grids alike.
bool sameGrid(const GuideGrid& a, const GuideGrid& b)
{
  if (a.width() != b.width() || a.height() != b.height() || a.wrapX() != b.wrapX() || a.wrapY() != b.wrapY())
    return false;
  for (int c = 0; c < a.width() * a.height(); ++c)
    if (a.blocked(c) != b.blocked(c) || a.run(c) != b.run(c) || a.fit(c) != b.fit(c) || a.tall(c) != b.tall(c) ||
      a.ladder(c) != b.ladder(c) || a.pipe(c) != b.pipe(c) || a.support(c) != b.support(c))
      return false;
  return true;
}

// The cached guide against one worked out from scratch, on the logic
// frames it works its answer out (in between it keeps the last one).
// Counts the frames compared and the ones that differ.
struct Compare
{
  int checked = 0;
  int differ = 0;
  std::string first;
  void operator()(const World& w)
  {
    const World::Guide& g = w.guide();
    if (g.frame != w.stats().frames || g.kind == World::GuideKind::None)
      return;
    const World::Guide fresh = w.guideFresh();
    ++checked;
    if (!same(g, fresh))
    {
      if (differ++ == 0)
        first = "frame " + std::to_string(g.frame) + ": kept " + describe(g) + ", fresh " + describe(fresh);
    }
  }
};

// Plays by the arrow alone: left or right as it says, up (climb, or jump)
// when it points up. Returns the frames until `done`, -1 if never.
int followArrow(World& w, const std::function<bool()>& done, int frames, bool shoot = false, Compare* compare = nullptr)
{
  bool jumpWas = false;
  for (int f = 0; f < frames; ++f)
  {
    if (done())
      return f;
    if (compare)
      (*compare)(w);
    const World::Guide& g = w.guide();
    PlayerInput in;
    // In reach of something to shoot: turn to it (a tap), then stand and fire.
    const bool turn = !g.fire || (g.dx > 0) != (w.player().facing > 0);
    in.left = g.dx < 0 && turn;
    in.right = g.dx > 0 && turn;
    in.up = g.dy < 0;
    in.down = g.dy > 0; // down, crouch to shoot low, crawl
    // Up at a ladder climbs it; anywhere else it is a jump. Sideways off a
    // ladder, or up at its top, is a jump too.
    const CellBox b = w.player().box();
    bool ladder = w.player().state == PlayerState::Ladder;
    for (int x = b.left(); x <= b.right() && !ladder; ++x)
      ladder = w.map().ladder(x, b.top());
    const bool onLadder = w.player().state == PlayerState::Ladder;
    // At the top of a ladder, up is a jump off it.
    bool above = false;
    for (int x = b.left(); x <= b.right() && !above; ++x)
      above = w.map().ladder(x, b.top() - 1);
    if (onLadder && !above)
      ladder = false;
    // Jump is held for the whole jump (letting go cuts it short), and let go
    // for a frame on landing (or on a ladder) to jump again.
    // Down and jump lets go of a pipe.
    const bool want = (g.dy < 0 && !ladder) || (onLadder && g.dx != 0) || (w.player().state == PlayerState::Pipe && g.dy > 0);
    const bool landed = w.player().state != PlayerState::Jumping && jumpWas; // landed, or caught a ladder
    const bool jump = (want || w.player().state == PlayerState::Jumping) && !landed;
    in.jump = {jump, jump && !jumpWas};
    const bool fire = shoot && f % 2 == 0;
    in.fire = {fire, fire};
    jumpWas = jump;
    if (std::getenv("GR_GUIDE_TRACE"))
      std::printf("     f %d runner %d,%d state %d: %s\n", f, w.player().x, w.player().y, int(w.player().state), describe(g).c_str());
    w.update(in);
  }
  return done() ? frames : -1;
}

} // namespace

int main()
{
  const std::string levels = std::string(GR_DATA_DIR) + "/levels/";
  SDL_Init(0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  {
    Renderer renderer(sdl);
    const Theme& theme = themeByIndex(0);
    const Art art = Art::build(theme, renderer);
    check(art.guideArrow.w() > 0, "the arrow is baked");

    // A gem down the corridor to the right.
    const auto gemRoom = load(room("", [](int y, std::string& row) {
      if (y == 13)
        row[20] = 'g';
    }));
    {
      World w(gemRoom, 0, theme, art);
      check(w.guide().kind == World::GuideKind::None, "off until switched on: " + describe(w.guide()));
      guideOn(w);
      const World::Guide& g = w.guide();
      check(g.kind == World::GuideKind::Item && g.dx == 1 && g.dy == 0 && g.walkable,
        "a gem to the right: points right (" + describe(g) + ")");
      check(g.left == 1, "one thing left");
      const int f = followArrow(w, [&] { return w.stats().gems == 1; }, 300);
      check(f >= 0, "following the arrow takes the gem (frames " + std::to_string(f) + ")");
      w.update(PlayerInput{});
      check(w.guide().kind == World::GuideKind::Exit && w.guide().dx == 1, "then it points at the exit: " + describe(w.guide()));
      const int out = followArrow(w, [&] { return w.state() != WorldState::Playing; }, 600);
      if (out < 0)
        std::printf("     runner at %d,%d state %d: %s\n", w.player().x, w.player().y, int(w.player().state), describe(w.guide()).c_str());
      check(out >= 0, "and following it reaches the exit (frames " + std::to_string(out) + ")");
      check(w.guide().kind == World::GuideKind::None, "no arrow while leaving");
    }

    // The gem sits on a shelf over the runner's head; the only way up is a
    // ladder behind them. The arrow points back to the ladder, not up.
    const auto shelfRoom = load(room(
      "",
      [](int y, std::string& row) {
        if (y == 9)
          for (int x = 6; x < 63; ++x)
            row[std::size_t(x)] = '#';
        if (y >= 8 && y <= 13)
          row[3] = 'H';
        if (y == 8)
          row[14] = 'g';
      },
      10));
    {
      World w(shelfRoom, 0, theme, art);
      guideOn(w);
      const World::Guide& g = w.guide();
      check(g.kind == World::GuideKind::Item && g.dx == -1 && g.walkable,
        "a gem on the shelf overhead: points back to the ladder (" + describe(g) + ")");
      const int f = followArrow(w, [&] { return w.stats().gems == 1; }, 900);
      if (f < 0)
        std::printf("     runner at %d,%d state %d: %s\n", w.player().x, w.player().y, int(w.player().state), describe(w.guide()).c_str());
      check(f >= 0, "following the arrow climbs up and takes it (frames " + std::to_string(f) + ")");
    }

    // A ledge 2 to 5 blocks over the floor with a gem on it, for Dash,
    // Rocco and Nova: the arrow finds a way up exactly when the runner's
    // own jump (and steer) lands on it.
    for (int runner = 0; runner < 3; ++runner)
      for (int rows = 2; rows <= 5; ++rows)
        for (const char tile : {'=', '#'})
        {
          const auto ledge = load(room("", [&](int y, std::string& row) {
            if (y == 14 - rows)
              for (int x = 8; x < 20; ++x)
                row[std::size_t(x)] = tile;
            if (y == 13 - rows)
              row[14] = 'g';
          }, 4));
          // Up to the ledge, jump holding right, on along it; or (through a
          // one-way ledge) a jump from right under the gem.
          bool lands = false;
          for (const int under : {0, 1})
          {
            World jumper(ledge, runner, theme, art);
            bool jumpWas = false;
            int at = -1;
            for (int f = 0; f < 80 && jumper.stats().gems == 0; ++f)
            {
              PlayerInput in;
              if (at < 0 && (under ? jumper.player().x >= 27 : f >= 3))
                at = f;
              in.right = !under || at < 0;
              const bool j = at >= 0 && f < at + 20;
              in.jump = {j, j && !jumpWas};
              jumpWas = j;
              jumper.update(in);
            }
            lands = lands || jumper.stats().gems == 1;
          }
          World w(ledge, runner, theme, art);
          guideOn(w);
          const bool walkable = w.guide().walkable;
          check(walkable == lands, "runner " + std::to_string(runner) + ", a ledge " + std::to_string(rows) + " blocks up (" +
            tile + "): the route " + (walkable ? "goes up" : "doesn't") + ", the jump " + (lands ? "lands" : "doesn't"));
        }

    // A bot down the corridor; a box of merchandise; nothing else counts.
    const auto botRoom = load(room("", [](int y, std::string& row) {
      if (y == 13)
      {
        row[30] = 'w';
        row[8] = 'h'; // a health box: not part of 100%
      }
    }));
    {
      World w(botRoom, 0, theme, art);
      guideOn(w);
      const World::Guide& g = w.guide();
      check(g.kind == World::GuideKind::Enemy && g.dx == 1 && g.left == 1, "a bot to the right: points at it (" + describe(g) + ")");
    }
    const auto merchRoom = load(room("", [](int y, std::string& row) {
      if (y == 13)
        row[12] = 'm';
    }, 30));
    {
      World w(merchRoom, 0, theme, art);
      guideOn(w);
      const World::Guide& g = w.guide();
      check(g.kind == World::GuideKind::Item && g.dx == -1, "a box of merchandise behind: points left (" + describe(g) + ")");
    }

    // Nothing to collect: straight to the exit. A bonus level: no arrow.
    {
      World w(load(room("")), 0, theme, art);
      guideOn(w);
      check(w.guide().kind == World::GuideKind::Exit && w.guide().dx == 1, "an empty room: the exit (" + describe(w.guide()) + ")");
    }
    {
      World w(load(room("rules=airjump\ntimer=60\ngoal=exit\n", [](int y, std::string& row) {
        if (y == 13)
          row[20] = 'g';
      })),
        0, theme, art);
      guideOn(w);
      check(w.guide().kind == World::GuideKind::None, "a bonus level: no arrow (" + describe(w.guide()) + ")");
    }

    // The grids kept between searches: a map's revision changes with its
    // tiles (not when a block is set to what it is), copies keep it, and a
    // kept grid matches one built from scratch once brought up to date.
    {
      const auto wallRoom = load(room("", [](int y, std::string& row) {
        if (y >= 10 && y <= 13)
          row[20] = '#';
      }));
      CollisionMap map(*wallRoom);
      const CollisionMap copy = map;
      const CollisionMap other(*wallRoom);
      check(copy.revision() == map.revision(), "a copied map keeps its revision");
      check(other.revision() != map.revision(), "two maps made apart never share one");
      const std::uint64_t r0 = map.revision();
      map.setBlock(20, 12, Tile::Solid);
      check(map.revision() == r0, "setting a block to what it is leaves the revision");
      GuideGrid::Statics st;
      st.mapRevision = map.revision();
      GuideGrid kept;
      check(kept.update(map, st, {}), "the first update builds the grids");
      check(!kept.update(map, st, {}), "the same statics again: the grids are kept");
      for (int y = 10; y <= 13; ++y)
        map.setBlock(20, y, Tile::Empty);
      check(map.revision() != r0, "opening the wall changes the revision");
      GuideGrid fresh;
      GuideGrid::Statics now = st;
      now.mapRevision = map.revision();
      fresh.update(map, now, {}, true);
      // Told nothing changed, the grid keeps the wall: what the revision is for.
      GuideGrid stale = kept;
      check(!stale.update(map, st, {}) && !sameGrid(stale, fresh), "with the old revision the grid is stale (it would be wrong)");
      check(kept.update(map, now, {}) && sameGrid(kept, fresh), "with the new one it is built again, as from scratch");
      // Platforms and floats move every frame: laid over the kept grids.
      map.addPlatform({30, 20, 6, 2});
      map.addFloat({44, 24, 4, 1});
      const std::vector<std::array<int, 3>> decks{{8, 22, 6}};
      check(!kept.update(map, now, decks), "platforms moving keep the grids");
      fresh.update(map, now, decks, true);
      check(sameGrid(kept, fresh), "and stand on them as a grid from scratch does");
      check(kept.support(kept.cellAt(30, 19)) && kept.support(kept.cellAt(28, 19)) && !kept.support(kept.cellAt(27, 19)),
        "the box stands on a platform where a column of it is over it");
      map.clearPlatforms();
      map.clearFloats();
      kept.update(map, now, {});
      fresh.update(map, now, {}, true);
      check(sameGrid(kept, fresh) && !kept.support(kept.cellAt(30, 19)), "and not once they have gone");
      map.setWrap(true);
      check(map.revision() != now.mapRevision, "wrapping the map round changes the revision");
    }

    // The kept guide says what one worked out from scratch says, while the
    // runner moves, takes the gem and walks out.
    {
      World w(gemRoom, 0, theme, art);
      guideOn(w);
      Compare cmp;
      const int f = followArrow(w, [&] { return w.state() != WorldState::Playing; }, 900, false, &cmp);
      check(f >= 0 && w.stats().gems == 1, "the gem room by the arrow, comparing (frames " + std::to_string(f) + ")");
      check(cmp.checked > 20 && cmp.differ == 0, "the gem room: kept and fresh agree on " + std::to_string(cmp.checked) +
        " frames (" + std::to_string(cmp.differ) + " differ) " + cmp.first);
    }

    // A gem behind a force field: no way there until the card is held, then
    // through it; walking up to it takes the field down (the map changes).
    {
      const auto fieldRoom = load(room("", [](int y, std::string& row) {
        if (y >= 1 && y <= 13)
          row[30] = 'D';
        if (y == 13)
          row[40] = 'g';
      }));
      World w(fieldRoom, 0, theme, art);
      guideOn(w);
      Compare cmp;
      cmp(w);
      check(!w.guide().routed && w.guide().dx == 1, "a gem behind a force field: straight at it, no route (" + describe(w.guide()) + ")");
      w.cheat(Cheat::Card);
      for (int f = 0; f < 6; ++f)
      {
        w.update(PlayerInput{});
        cmp(w);
      }
      check(w.guide().routed && w.guide().walkable && w.guide().dx == 1,
        "with the card: a route through the field (" + describe(w.guide()) + ")");
      const int f = followArrow(w, [&] { return w.stats().gems == 1; }, 600, false, &cmp);
      check(f >= 0 && !w.map().forceFieldsOn(), "following it takes the field down and the gem (frames " + std::to_string(f) + ")");
      check(cmp.checked > 10 && cmp.differ == 0, "the force field: kept and fresh agree on " + std::to_string(cmp.checked) +
        " frames (" + std::to_string(cmp.differ) + " differ) " + cmp.first);
    }

    // Campaign levels, a scripted run each (walking both ways, jumping,
    // climbing, shooting; lifts, bots and crates on the move): kept and fresh
    // agree on every frame the guide is worked out.
    for (int n = 1; n <= 6; ++n)
    {
      const auto level = std::make_shared<const Level>(Level::loadFile(levelFile(GR_DATA_DIR, n)));
      World w(level, n % 3, theme, art);
      guideOn(w);
      w.cheat(Cheat::God);
      Compare cmp;
      for (int f = 0; f < 150 && w.state() == WorldState::Playing; ++f)
      {
        cmp(w);
        PlayerInput in;
        in.right = (f / 40) % 4 != 3;
        in.left = !in.right;
        in.up = (f / 50) % 3 == 1;
        in.jump.pressed = in.jump.triggered = f % 17 == 0;
        in.fire.pressed = true;
        in.fire.triggered = f % 6 == 0;
        w.update(in);
        (void)w.takeSounds();
      }
      check(cmp.checked > 50 && cmp.differ == 0, level->slug + ": kept and fresh agree on " + std::to_string(cmp.checked) +
        " frames (" + std::to_string(cmp.differ) + " differ) " + cmp.first);
    }

    // Every campaign level and extra: something to point at from the start.
    std::vector<std::string> files;
    for (int n = 1; n <= kAllLevels; ++n)
      files.push_back(levelFile(GR_DATA_DIR, n));
    for (const auto& e : std::filesystem::directory_iterator(levels))
      if (e.path().filename().string().rfind("extra_", 0) == 0)
        files.push_back(e.path().string());
    int walkable = 0;
    double worst = 0.0;
    std::string worstLevel;
    for (const auto& path : files)
    {
      const auto level = std::make_shared<const Level>(Level::loadFile(path));
      World w(level, 0, theme, art);
      guideOn(w);
      const auto t0 = std::chrono::steady_clock::now();
      const World::Guide g = w.guide();
      const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
      if (ms > worst)
      {
        worst = ms;
        worstLevel = level->slug;
      }
      walkable += g.walkable;
      std::printf("     %-28s %s (%.1f ms)\n", level->slug.c_str(), describe(g).c_str(), ms);
      check(g.kind != World::GuideKind::None && (g.dx != 0 || g.dy != 0), level->slug + ": the arrow points somewhere");
    }
    // Level 1 played by the arrow alone (shooting all the time, no damage).
    {
      const auto level = std::make_shared<const Level>(Level::loadFile(levelFile(GR_DATA_DIR, 1)));
      World w(level, 0, theme, art);
      guideOn(w);
      w.cheat(Cheat::God);
      const int left0 = w.guide().left;
      const int f = followArrow(w, [&] { return w.state() != WorldState::Playing; }, 3000, true);
      const auto& st = w.stats();
      std::printf("     level 1 by the arrow: %d frames, gems %d/%d, merch %d/%d, bots %d/%d, letters %s, left %d (%s)\n", f,
        st.gems, st.gemsTotal, st.merch, st.merchTotal, st.kills, st.enemiesTotal, st.letters.c_str(), w.guide().left,
        describe(w.guide()).c_str());
      // A crude player (no timing, no looking ahead) still gets a good way
      // round: the crates crouched, the crawlspace, up the ladders.
      check(left0 - w.guide().left >= 25, "level 1: following the arrow alone collects and clears most of it");
    }
    // More levels and runners by the arrow alone, on request:
    // GR_GUIDE_LEVELS=2,8,12 GR_GUIDE_RUNNERS=0,1,2 (reported, not checked).
    if (const char* list = std::getenv("GR_GUIDE_LEVELS"))
    {
      const char* rl = std::getenv("GR_GUIDE_RUNNERS");
      std::vector<int> runners;
      for (const char* q = rl ? rl : "0"; *q;)
      {
        runners.push_back(std::atoi(q));
        while (*q && *q != ',')
          ++q;
        if (*q)
          ++q;
      }
      for (const char* q = list; *q;)
      {
        const int n = std::atoi(q);
        while (*q && *q != ',')
          ++q;
        if (*q)
          ++q;
        for (const int runner : runners)
        {
          const auto level = std::make_shared<const Level>(Level::loadFile(levelFile(GR_DATA_DIR, n)));
          World w(level, runner, theme, art);
          guideOn(w);
          w.cheat(Cheat::God);
          const int left0 = w.guide().left;
          const int f = followArrow(w, [&] { return w.state() != WorldState::Playing; }, 6000, true);
          const auto& st = w.stats();
          std::printf("     level %d runner %d by the arrow: %d frames, %d of %d done, gems %d/%d, merch %d/%d, bots %d/%d, left %d (%s; at %d,%d)\n",
            n, runner, f, left0 - w.guide().left, left0, st.gems, st.gemsTotal, st.merch, st.merchTotal, st.kills,
            st.enemiesTotal, w.guide().left, describe(w.guide()).c_str(), w.player().x, w.player().y);
        }
      }
    }
    std::printf("     %d of %d levels start with a walkable route; slowest search %.1f ms (%s)\n", walkable,
      int(files.size()), worst, worstLevel.c_str());
    check(walkable * 4 >= int(files.size()) * 3, "most levels start with a walkable route");
    check(worst < 250.0, "every search is quick");
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf("%s\n", gFailures == 0 ? "all guide checks passed" : "guide checks FAILED");
  return gFailures == 0 ? 0 : 1;
}
