// The guide arrow (ASSISTS: GUIDE ARROW), in small test rooms and on every
// campaign level: off until switched on and in bonus levels; points the way
// along a route (round a ceiling to a ladder, not straight up into it) to
// the nearest gem, box or bot still missing, then to the exit; a runner who
// only follows the arrow collects what it points at. On the real levels it
// finds something to point at from the start, quickly. With a force field
// up and the access card still to fetch, it leads to the card first only
// when nothing else can be reached without it.
//
// The guide keeps its grids and search scratch between logic frames: what
// it says when it works its answer out must be what a search from scratch
// (World::guideFresh) says, as the runner moves, things are taken, the
// card is picked up and a force field goes down, on the test rooms and
// along a scripted run through every campaign level. The grids it keeps
// are built again whenever what they depend on changes (the map's tiles,
// breakables opened, lava, the card), and would be wrong if they weren't.
// The router kept between searches routes as a brand-new one over hundreds
// of searches, round its byte-sized stamp's wrap and back, and would not if
// the wrap didn't clear what the searches before it saw.

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

namespace gr
{
// The test's way into GuideRouter's search stamp (a friend of the class).
struct GuideRouterTest
{
  static int stamp(const GuideRouter& r) { return r.mStamp; }
  static void setStamp(GuideRouter& r, std::uint8_t s) { r.mStamp = s; }
};
} // namespace gr

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
// With `every` above 1 only one in that many of those frames is compared
// (a search from scratch costs the whole map; the kept guide is still
// asked every frame, so it changes as it would in play).
// A call that sees the same answer it counted last (same frame, place and
// count left) is skipped: while the world holds still (no logic frame),
// guide() keeps its answer and g.frame still equals stats().frames, so
// without this the repeats of one frame would count again toward a test's
// bar. A runner moved without a new frame (a monitor wipe's teleport) gives
// a new place, so that answer still counts.
struct Compare
{
  int every = 1;
  int worked = 0; // frames the kept guide worked its answer out on
  int checked = 0;
  int differ = 0;
  std::string first;
  bool counted = false; // `last` holds the answer counted last
  World::Guide last;
  void operator()(const World& w)
  {
    const World::Guide& g = w.guide();
    if (g.frame != w.stats().frames || g.kind == World::GuideKind::None || repeat(g))
      return;
    counted = true;
    last = g;
    if (worked++ % every != 0)
      return;
    const World::Guide fresh = w.guideFresh();
    ++checked;
    if (!same(g, fresh))
    {
      if (differ++ == 0)
        first = "frame " + std::to_string(g.frame) + ": kept " + describe(g) + ", fresh " + describe(fresh);
    }
  }
  // The same answer as the one counted last: a held frame asked again.
  bool repeat(const World::Guide& g) const
  {
    return counted && g.frame == last.frame && g.fromX == last.fromX && g.fromY == last.fromY && g.left == last.left;
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

// ---- The grids kept between searches ----

// The walled room with a wall 4 blocks high at block 20 of the corridor.
std::shared_ptr<const Level> wallRoom()
{
  return load(room("", [](int y, std::string& row) {
    if (y >= 10 && y <= 13)
      row[20] = '#';
  }));
}

// A map's revision changes with its tiles and its wrapping (not when a
// block is set to what it is); copies keep it.
void checkRevision()
{
  const auto level = wallRoom();
  CollisionMap map(*level);
  const CollisionMap copy = map;
  const CollisionMap other(*level);
  check(copy.revision() == map.revision(), "a copied map keeps its revision");
  check(other.revision() != map.revision(), "two maps made apart never share one");
  const std::uint64_t r0 = map.revision();
  map.setBlock(20, 12, Tile::Solid);
  check(map.revision() == r0, "setting a block to what it is leaves the revision");
  map.setBlock(20, 12, Tile::Empty);
  check(map.revision() != r0, "opening the wall changes the revision");
  const std::uint64_t r1 = map.revision();
  map.setWrap(true);
  check(map.revision() != r1, "wrapping the map round changes the revision");
}

// `kept0` was built for `then`; the world is now as `now` says. Told
// nothing changed (`then` again) the grid keeps what it had and is wrong:
// what the key is for. With `now` it is built again and matches a grid
// built from scratch.
void checkRebuilt(const CollisionMap& map, const GuideGrid& kept0, const GuideStatics& then, const GuideStatics& now,
  const std::string& what)
{
  GuideGrid fresh;
  fresh.update(map, now, {}, true);
  GuideGrid stale = kept0;
  check(!stale.update(map, then, {}) && !sameGrid(stale, fresh), what + ": with the old key the grid is stale (it would be wrong)");
  GuideGrid kept = kept0;
  check(kept.update(map, now, {}) && sameGrid(kept, fresh), what + ": with the new key it is built again, as from scratch");
}

// The grids are kept while the key stays the same, and built again once
// the map's tiles change (its revision).
void checkGridKept()
{
  const auto level = wallRoom();
  CollisionMap map(*level);
  GuideStatics st;
  st.mapRevision = map.revision();
  GuideGrid kept;
  check(kept.update(map, st, {}), "the first update builds the grids");
  check(!kept.update(map, st, {}), "the same statics again: the grids are kept");
  for (int y = 10; y <= 13; ++y)
    map.setBlock(20, y, Tile::Empty);
  GuideStatics now = st;
  now.mapRevision = map.revision();
  checkRebuilt(map, kept, st, now, "the wall taken out of the map");
}

// Platforms and floats move every frame: laid over the kept grids, as a
// grid from scratch has them, and gone once they are.
void checkGridPlatforms()
{
  const auto level = wallRoom();
  CollisionMap map(*level);
  GuideStatics st;
  st.mapRevision = map.revision();
  GuideGrid kept, fresh;
  kept.update(map, st, {});
  map.addPlatform({30, 20, 6, 2});
  map.addFloat({44, 24, 4, 1});
  const std::vector<std::array<int, 3>> decks{{8, 22, 6}};
  check(!kept.update(map, st, decks), "platforms moving keep the grids");
  fresh.update(map, st, decks, true);
  check(sameGrid(kept, fresh), "and stand on them as a grid from scratch does");
  check(kept.support(kept.cellAt(30, 19)) && kept.support(kept.cellAt(28, 19)) && !kept.support(kept.cellAt(27, 19)),
    "the box stands on a platform where a column of it is over it");
  map.clearPlatforms();
  map.clearFloats();
  kept.update(map, st, {});
  fresh.update(map, st, {}, true);
  check(sameGrid(kept, fresh) && !kept.support(kept.cellAt(30, 19)), "and not once they have gone");
}

// Breakables any shot opens and lava pools that burn are part of the key
// too: a change to either builds the grids again.
void checkGridStatics()
{
  const auto level = wallRoom();
  const CollisionMap map(*level);
  const int k = kCellsPerTile;
  GuideStatics st;
  st.mapRevision = map.revision();
  GuideGrid kept;
  kept.update(map, st, {});
  const int wall = kept.cellAt(20 * k, 12 * k);
  check(kept.blocked(wall), "the wall is in the way");
  GuideStatics opened = st;
  opened.open.push_back({20, 10, 20, 13});
  checkRebuilt(map, kept, st, opened, "the wall a breakable a shot opens");
  kept.update(map, opened, {});
  check(!kept.blocked(wall), "and then it is not in the way");
  GuideStatics burning = opened;
  burning.lava.push_back({40 * k, 13 * k, 42 * k - 1, 14 * k - 1});
  checkRebuilt(map, kept, opened, burning, "a lava pool that burns");
  kept.update(map, burning, {});
  check(kept.blocked(kept.cellAt(41 * k, 13 * k)), "and the lava is as bad as a wall");
}

// ---- The router kept between searches ----

// A room for the router on its own, 24 blocks by 12 (48 by 24 cells): a
// wall 5 blocks high the runner can't jump, a ladder up to a one-way
// shelf. Room for every kind of search, small enough for hundreds of them.
std::shared_ptr<const Level> routerRoom()
{
  return load("name=GUIDE TEST\ntheme=neon_rooftops\nmusic=theme_synthwave\n[map]\n"
              "########################\n"
              "#......................#\n"
              "#......................#\n"
              "#.............H========#\n"
              "#.............H........#\n"
              "#.............H........#\n"
              "#.....#.......H........#\n"
              "#.....#.......H........#\n"
              "#.....#.......H........#\n"
              "#.....#.......H........#\n"
              "#P....#.......H.......X#\n"
              "########################\n"
              "[entities]\n");
}

// The router room's grid (the map gone again: the grid keeps no pointer to it).
GuideGrid routerGrid()
{
  const auto level = routerRoom();
  const CollisionMap map(*level);
  GuideStatics st;
  st.mapRevision = map.revision();
  GuideGrid grid;
  grid.update(map, st, {});
  return grid;
}

bool sameRoute(const GuideRoute& a, const GuideRoute& b)
{
  return a.states == b.states && a.perCell == b.perCell && a.target == b.target && a.walkable == b.walkable;
}

// What the router looks for in turn (in cells): a gem on the floor, one on
// the shelf, a bot to shoot, a gem behind the wall, two gems at once, and
// nothing at all (every search runs dry: three searches in one route()).
std::vector<std::vector<GuideTarget>> routerTargets()
{
  const GuideTarget floorGem{{40, 20, 2, 2}, GuideKind::Item, {40, 20, 2, 2}};
  const GuideTarget shelfGem{{38, 4, 2, 2}, GuideKind::Item, {38, 4, 2, 2}};
  const GuideTarget bot{{12, 14, 19, 12}, GuideKind::Enemy, {20, 18, 3, 4}, true};
  const GuideTarget walledGem{{4, 20, 2, 2}, GuideKind::Item, {4, 20, 2, 2}};
  return {{floorGem}, {shelfGem}, {bot}, {walledGem}, {floorGem, walledGem}, {}};
}

// One router kept from one route() to the next against a brand-new one for
// each, over 640 routes: a search or more each, so the byte-sized stamp
// wraps round (and mSeen is cleared) at least twice. From starts all over
// the room in every phase and facing, the targets changing every 7 routes,
// the steer now and then, and some searches through open space only.
void checkRouterKept()
{
  const GuideGrid grid = routerGrid();
  const auto targets = routerTargets();
  const int rows[] = {21, 5, 12, 6}; // on the floor, on the shelf, in the air, up high
  GuideRouter kept;
  GuideRouter::Moves moves;
  moves.arc = {2, 2, 1, 1, 1, 0, 0, 0}; // Dash
  const int routes = 640;
  int wraps = 0, differ = 0, found = 0;
  std::string first;
  for (int i = 0; i < routes; ++i)
  {
    const auto& t = targets[std::size_t(i / 7) % targets.size()];
    const GuideRouter::Start start{grid.cellAt(1 + i * 7 % 46, rows[i % 4]), i * 5 % 15, i % 2 ? 1 : -1};
    moves.steer = i % 11 == 0 ? 2 : 1;
    const bool freeOnly = i % 9 == 0;
    const int stamp0 = GuideRouterTest::stamp(kept);
    const GuideRoute a = kept.route(grid, t, start, moves, freeOnly);
    wraps += GuideRouterTest::stamp(kept) < stamp0; // at most 3 searches a route: a drop is a wrap
    const GuideRoute b = GuideRouter().route(grid, t, start, moves, freeOnly);
    found += b.target >= 0;
    if (!sameRoute(a, b) && differ++ == 0)
      first = ", the first at route " + std::to_string(i);
  }
  check(wraps >= 2 && differ == 0, "one router over " + std::to_string(routes) + " routes, round its stamp " +
    std::to_string(wraps) + " times: as a new router every time (" + std::to_string(differ) + " differ" + first + ")");
  check(found * 2 >= routes, "and most of them find a way (" + std::to_string(found) + ")");
}

// The clear at the wrap is needed. Two routers search from beside the gem
// (stamp 1: the cells round it seen, cheap). One then wraps round as
// begin() does (stamp 255 to 0: mSeen cleared, stamp 1); the other comes
// round to stamp 1 with mSeen as it was, as if begin() skipped the clear.
// From further off, the first routes as a new router; the second takes the
// cells by the gem for seen already, cheaper, and can't go through them.
void checkRouterWrapClears()
{
  const GuideGrid grid = routerGrid();
  const std::vector<GuideTarget> gem{{{24, 20, 2, 2}, GuideKind::Item, {24, 20, 2, 2}}};
  const GuideRouter::Start beside{grid.cellAt(32, 21)}, further{grid.cellAt(42, 21)};
  const GuideRoute want = GuideRouter().route(grid, gem, further, {}, true);
  GuideRouter wrapped, skipped;
  wrapped.route(grid, gem, beside, {}, true);
  skipped.route(grid, gem, beside, {}, true);
  GuideRouterTest::setStamp(wrapped, 255);
  GuideRouterTest::setStamp(skipped, 0);
  check(want.target == 0 && sameRoute(wrapped.route(grid, gem, further, {}, true), want),
    "wrapped round, the router clears what it saw and routes as a new one");
  check(!sameRoute(skipped.route(grid, gem, further, {}, true), want),
    "without the clear it would take what the search before saw for seen (stale)");
}

// ---- The guide kept between searches against one from scratch ----

// Kept and fresh agree on at least `least` compared frames (and the kept
// guide worked its answer out on at least as many).
bool agrees(const Compare& cmp, int least)
{
  return cmp.worked >= least && cmp.checked >= least && cmp.differ == 0;
}

std::string agreement(const Compare& cmp, const std::string& what)
{
  return what + ": kept and fresh agree on " + std::to_string(cmp.checked) + " frames of " + std::to_string(cmp.worked) +
    " worked out (" + std::to_string(cmp.differ) + " differ) " + cmp.first;
}

void checkAgree(const Compare& cmp, int least, const std::string& what)
{
  check(agrees(cmp, least), agreement(cmp, what));
}

// While the runner moves, takes the gem and walks out.
void checkKeptGemRoom(const std::shared_ptr<const Level>& gemRoom, const Theme& theme, const Art& art)
{
  World w(gemRoom, 0, theme, art);
  guideOn(w);
  Compare cmp;
  const int f = followArrow(w, [&] { return w.state() != WorldState::Playing; }, 900, false, &cmp);
  check(f >= 0 && w.stats().gems == 1, "the gem room by the arrow, comparing (frames " + std::to_string(f) + ")");
  checkAgree(cmp, 21, "the gem room");
}

// A gem behind a force field: no way there until the card is held, then
// through it; walking up to it takes the field down (the map changes).
void checkKeptForceField(const Theme& theme, const Art& art)
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
  checkAgree(cmp, 11, "the force field");
}

// ---- The access card ----

// A walled room with a force field (block 30) between the runner (block 20)
// and a gem (block 40), and the access card in a crate at block `card` of
// the corridor; `more` changes it further.
std::shared_ptr<const Level> cardRoom(int card, const std::function<void(int, std::string&)>& more = {})
{
  return load(room("", [&](int y, std::string& row) {
    if (y >= 1 && y <= 13)
      row[30] = 'D';
    if (y == 13)
    {
      row[40] = 'g';
      row[std::size_t(card)] = 'k';
    }
    if (more)
      more(y, row);
  }, 20));
}

// The gem behind the field and the card in a crate behind the runner (block
// 14): the arrow routes back to the crate, away from the gem, and a runner
// who follows it (shooting) opens the crate, takes the card, walks through
// the field and takes the gem, the kept guide agreeing with a fresh one
// all the way. (When the card was never a goal, the arrow pointed straight
// at the gem, with no route.)
void checkCardFirst(const Theme& theme, const Art& art)
{
  const int k = kCellsPerTile;
  World w(cardRoom(14), 0, theme, art);
  guideOn(w);
  const World::Guide& g = w.guide();
  check(g.kind == World::GuideKind::Item && g.routed && g.walkable && g.dx == -1 && g.tx < 20 * k && g.left == 1,
    "a gem behind a force field, the card in a crate behind: routes back to the card (" + describe(g) + ")");
  Compare cmp;
  bool card = false;
  const int f = followArrow(w, [&] {
    card = card || w.player().hasKey;
    return w.stats().gems == 1;
  }, 900, true, &cmp);
  check(f >= 0 && card && !w.map().forceFieldsOn(),
    "following it takes the card, then the field down and the gem (frames " + std::to_string(f) + ")");
  checkAgree(cmp, 11, "the card first");
}

// A gem on this side of the field too (block 26), further off than the
// crate with the card (block 14, close enough to shoot after a step or
// two): reached without the card, it comes first.
void checkCardNotFirst(const Theme& theme, const Art& art)
{
  const int k = kCellsPerTile;
  World w(cardRoom(14, [](int y, std::string& row) {
    if (y == 13)
      row[26] = 'g';
  }), 0, theme, art);
  guideOn(w);
  const World::Guide g = w.guide();
  check(g.kind == World::GuideKind::Item && g.routed && g.walkable && g.dx == 1 && g.tx >= 26 * k && g.tx < 30 * k &&
      g.left == 2,
    "a gem this side of the field, further than the card: the gem first (" + describe(g) + ")");
  check(same(g, w.guideFresh()), "and so says a guide from scratch");
}

// The card shut in too, behind a second field (block 10): nothing can be
// reached, so the arrow points as the crow flies at the nearest target,
// the card's crate (block 4) rather than the gem (block 40).
void checkCardCrowFlies(const Theme& theme, const Art& art)
{
  const int k = kCellsPerTile;
  World w(cardRoom(4, [](int y, std::string& row) {
    if (y >= 1 && y <= 13)
      row[10] = 'D';
  }), 0, theme, art);
  guideOn(w);
  const World::Guide& g = w.guide();
  check(g.kind == World::GuideKind::Item && !g.routed && g.dx == -1 && g.tx < 10 * k,
    "the card shut in too: straight at it, the nearest, no route (" + describe(g) + ")");
}

// A scripted run of `frames` live frames (walking right, then left;
// climbing a while in the middle; jumping and shooting all along), the
// kept guide compared with `cmp` on every frame. A frame the world holds
// still (level 42's roll-ins, wipes and commercial breaks, a title card)
// does not count: no logic runs, so the guide has nothing new to work out,
// and a long hold would leave a short run next to nothing to compare (and
// `cmp` skips the repeats of one held frame). At most four times as many
// calls as live frames asked for, so a world that never lets go still ends.
void scriptedRun(World& w, int frames, Compare& cmp)
{
  for (int f = 0, calls = 0; f < frames && calls < frames * 4 && w.state() == WorldState::Playing; ++calls)
  {
    cmp(w);
    PlayerInput in;
    in.right = f < frames * 4 / 5;
    in.left = !in.right;
    in.up = f >= frames / 3 && f < frames * 2 / 3;
    in.jump.pressed = in.jump.triggered = f % 17 == 0;
    in.fire.pressed = true;
    in.fire.triggered = f % 6 == 0;
    const int before = w.stats().frames;
    w.update(in);
    (void)w.takeSounds();
    f += w.stats().frames != before;
  }
}

// Campaign level `n`, a scripted run: levels 1-6 run longer and compare
// every frame worked out; the rest a shorter run, one frame in three, to
// keep the test quick. Each must compare `least` frames: a run that left
// Playing too early (or held still) would show too little to trust.
bool keptLevelAgrees(int n, const Theme& theme, const Art& art, int& compared)
{
  const bool longRun = n <= 6;
  const int least = longRun ? 51 : 10;
  const auto level = std::make_shared<const Level>(Level::loadFile(levelFile(GR_DATA_DIR, n)));
  World w(level, n % 3, theme, art);
  guideOn(w);
  w.cheat(Cheat::God);
  Compare cmp;
  cmp.every = longRun ? 1 : 3;
  scriptedRun(w, longRun ? 150 : 90, cmp);
  compared = cmp.checked;
  if (!agrees(cmp, least))
    std::printf("     %s (at least %d)\n", agreement(cmp, level->slug).c_str(), least);
  return agrees(cmp, least);
}

// Every campaign level, a scripted run each (lifts, bots and crates on the
// move; wrapping, turning maps, slopes, lava, breakables, beat and timer
// layers, doors and gravity where the level has them): kept and fresh
// agree. One line for them all; a level that fails says so on its own.
void checkKeptCampaign(const Theme& theme, const Art& art)
{
  int failed = 0, total = 0, fewest = -1, fewestLevel = 0;
  for (int n = 1; n <= kAllLevels; ++n)
  {
    int compared = 0;
    failed += !keptLevelAgrees(n, theme, art, compared);
    total += compared;
    if (n > 6 && (fewest < 0 || compared < fewest))
    {
      fewest = compared;
      fewestLevel = n;
    }
  }
  check(failed == 0, "every campaign level: kept and fresh agree (" + std::to_string(total) + " frames compared, " +
    std::to_string(failed) + " levels fail; fewest in a short run " + std::to_string(fewest) + ", level " +
    std::to_string(fewestLevel) + ")");
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

    // The grids and the guide kept between searches.
    checkRevision();
    checkGridKept();
    checkGridPlatforms();
    checkGridStatics();
    checkRouterKept();
    checkRouterWrapClears();
    checkKeptGemRoom(gemRoom, theme, art);
    checkKeptForceField(theme, art);
    checkCardFirst(theme, art);
    checkCardNotFirst(theme, art);
    checkCardCrowFlies(theme, art);
    checkKeptCampaign(theme, art);

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
