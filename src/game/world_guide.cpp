// The guide arrow (GUIDE ARROW in SETTINGS > ASSISTS): a blinking arrow by
// the runner that points the way to the nearest thing still missing for a
// 100% level (a gem, a G-U-N letter, a box of merchandise, a weapon not had
// yet, a gem cache, a bot), then to the exit. Off in bonus levels.
//
// "Nearest" and "the way" come from a cheapest-first search over what the
// runner can do a logic frame at a time, as player.cpp does it (guide.cpp):
// walking (turning round takes a frame), crawling, ladders and pipes, and
// this runner's own jump arc (with its hang at the top, ceilings and
// steering in the air), then falling. What that can't reach (a lift, a
// pulley, a teleport, a door still shut) is floated to through open space,
// as little as the route can; with no gravity (flying, swimming) the whole
// route is. The arrow then says what to press now: walk this way, jump (and
// steer), climb, or crouch and crawl.
//
// The grids the search reads are kept (mGuideCache) while the map, the
// breakables, the lava and the card stay the same, so a search costs the
// states it visits rather than the whole map each logic frame.

#include "game/world.hpp"

#include "assets/art.hpp"
#include "game/guide.hpp"
#include "render/renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace gr
{

namespace
{

constexpr int kBoxW = GuideGrid::kBoxW;
constexpr int kStandH = GuideGrid::kStandH;
constexpr int kStates = GuideRouter::kStates;
constexpr int kRecompute = 4;   // logic frames between searches while the runner stands still
constexpr int kLookAhead = 10;  // cells along the route the arrow aims at
constexpr int kEnemyReachX = 8; // close enough to shoot it: cells either side
constexpr int kEnemyReachY = 1;

constexpr float kCellPx = float(kCellSize) * kPixelScale;

// Close enough to shoot it: kEnemyReachX cells either side, about level.
GuideTarget guideShootAt(const CellBox& b, GuideKind kind)
{
  CellBox r = b;
  r.x -= kEnemyReachX;
  r.w += 2 * kEnemyReachX;
  r.y -= kEnemyReachY;
  r.h += 2 * kEnemyReachY;
  return {r, kind, b, true};
}

int weaponBit(Weapon w) { return 1 << int(w); }

// GR_GUIDE_DEBUG=1: the targets and the route on stderr.
bool guideDebug()
{
  static const bool on = std::getenv("GR_GUIDE_DEBUG") != nullptr;
  return on;
}

// Turning a route into the arrow: the runner's box and middle (in cells),
// the grid the route was found on, and the answer being filled in.
struct Arrow
{
  World::Guide& g;
  const GuideGrid& grid;
  const CellBox pb;
  const float px, py;

  // Points at (tx, ty) (cells, the shorter way round on a wrapping map),
  // eight ways, 45 degrees each. Nothing when already there.
  void toward(float tx, float ty) const
  {
    const int W = grid.width(), H = grid.height();
    float vx = tx - px, vy = ty - py;
    if (grid.wrapX() && std::fabs(vx) > float(W) * 0.5f)
      vx -= std::copysign(float(W), vx);
    if (grid.wrapY() && std::fabs(vy) > float(H) * 0.5f)
      vy -= std::copysign(float(H), vy);
    if (std::fabs(vx) < 0.5f && std::fabs(vy) < 0.5f)
      return;
    const float ang = std::atan2(vy, vx);
    const int oct = int(std::lround(ang / (3.14159265f / 4.0f)));
    static const int kDx[8] = {1, 1, 0, -1, -1, -1, 0, 1};
    static const int kDy[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    const int o = GuideGrid::wrapIndex(oct, 8);
    g.dx = kDx[o];
    g.dy = kDy[o];
  }

  void settle(const GuideTarget& t) const
  {
    g.kind = t.kind;
    g.tx = t.thing.left() + t.thing.w / 2;
    g.ty = t.thing.top() + t.thing.h / 2;
  }
};

// Steps along a route: which way each one goes (-1, 0, 1), the shorter way
// round on a wrapping map.
struct Steps
{
  const GuideRoute& r;
  const GuideGrid& grid;

  std::size_t size() const { return r.states.size(); }
  int stepX(std::size_t j) const
  {
    if (j + 1 >= size())
      return 0;
    const int W = grid.width();
    int d = r.cell(j + 1) % W - r.cell(j) % W;
    if (grid.wrapX() && std::abs(d) > W / 2)
      d -= d > 0 ? W : -W;
    return d > 0 ? 1 : d < 0 ? -1 : 0;
  }
  // Sideways this step: a move, or turning round (which takes a frame).
  int moveX(std::size_t j) const
  {
    if (const int d = stepX(j); d != 0 || r.perCell != kStates || j + 1 >= size())
      return d;
    const int a = r.states[j] & 1, b = r.states[j + 1] & 1;
    return a != b ? (b ? 1 : -1) : 0;
  }
  int stepY(std::size_t j) const
  {
    if (j + 1 >= size())
      return 0;
    const int W = grid.width(), H = grid.height();
    int d = r.cell(j + 1) / W - r.cell(j) / W;
    if (grid.wrapY() && std::abs(d) > H / 2)
      d -= d > 0 ? H : -H;
    return d > 0 ? 1 : d < 0 ? -1 : 0;
  }
};

bool rising(int ph) { return ph >= 1 && ph <= 8; }

// Standing: walk, step onto a ladder, jump (steering from its second
// frame) or walk off the edge.
void pressStanding(const Steps& st, int& dx, int& dy)
{
  const int ph1 = st.r.phase(1);
  if (ph1 == 0)
    dx = st.moveX(0); // walk (crouched into a crawlspace: pressOwnMoves)
  else if (ph1 == GuideRouter::kOnLadder)
    dy = st.size() > 2 && st.r.phase(2) == GuideRouter::kOnLadder && st.stepY(1) > 0 ? 1 : -1; // onto the ladder
  else if (rising(ph1))
  {
    dy = -1; // jump, steering from its second frame
    dx = st.moveX(1);
  }
  else
    dx = st.moveX(0); // off the edge
  if (dx == 0 && dy == 0)
    dy = 1;
}

// On a ladder, a pipe or in the air.
void pressHolding(const Steps& st, int ph0, int& dx, int& dy)
{
  const int ph1 = st.r.phase(1);
  if (ph0 == GuideRouter::kOnLadder)
  {
    if (ph1 == GuideRouter::kOnLadder)
      dy = st.stepY(0);
    else if (rising(ph1))
    {
      dy = -1; // jump off it: sideways says jump, not climb
      dx = st.moveX(0) != 0 ? st.moveX(0) : st.moveX(1);
    }
    else
      dy = 1; // down off its foot
  }
  else if (ph0 == GuideRouter::kOnPipe)
  {
    dx = st.moveX(0);
    if (rising(ph1))
      dy = -1; // up and jump
    else if (ph1 != GuideRouter::kOnPipe && dx == 0)
      dy = 1; // let go
  }
  else
  {
    // In the air: steer this frame; up while it rises and to grab a
    // ladder, down while it falls.
    dx = st.moveX(0);
    if (ph1 == GuideRouter::kOnLadder)
    {
      dx = 0;
      dy = -1;
    }
    else
      dy = rising(ph1) || ph1 == GuideRouter::kOnPipe ? -1 : ph1 == 0 ? 0 : 1;
  }
}

// The runner's own moves: what to press now.
void pressOwnMoves(const Arrow& a, const Steps& st)
{
  const int ph0 = st.r.phase(0);
  int dx = 0, dy = 0;
  if (ph0 == 0)
    pressStanding(st, dx, dy);
  else
    pressHolding(st, ph0, dx, dy);
  a.g.dx = dx;
  a.g.dy = dy;
  // A walk into a crawlspace, the next step: down that way (crouch and
  // crawl; crouching any sooner only crouches).
  if (ph0 != 0 || st.r.phase(1) != 0)
    return;
  const int W = a.grid.width();
  const int q = st.r.cell(1);
  for (int c = 0; c < kBoxW; ++c)
  {
    const int qx = a.grid.wrapX() ? GuideGrid::wrapIndex(q % W + c, W) : q % W + c;
    if (qx < 0 || qx >= W || a.grid.run((q / W) * W + qx) < kStandH)
    {
      a.g.dy = 1;
      break;
    }
  }
}

// Floating, or with no gravity: where along the route the arrow aims.
// Floating, kLookAhead steps along it; with no gravity, a walk aims where
// it ends, else kLookAhead steps on.
std::size_t aimStep(const Steps& st)
{
  std::size_t k = std::min<std::size_t>(std::size_t(kLookAhead), st.size() - 1);
  const GuideRoute& r = st.r;
  if (r.perCell == kStates)
  {
    k = 1;
    while (k + 1 < st.size() && k < std::size_t(kLookAhead) && r.phase(k + 1) == GuideRouter::kFloating)
      ++k;
    return k;
  }
  const int W = st.grid.width();
  auto flat = [&](std::size_t j) { return r.cell(j) / W == r.cell(j + 1) / W && st.grid.support(r.cell(j)); };
  std::size_t run = 0;
  while (run + 1 < st.size() && run < k && flat(run))
    ++run;
  return run >= 1 && run < k ? run : k;
}

// The arrow along a route found to target t. Where it aims depends on the
// route's first move: a walk (or a climb) aims where it ends (the foot of
// the ladder, the edge to jump from, the ladder's top), so the arrow says
// "this way" first and "up" or "jump" once you are there; in the air it
// says how to steer; a fall aims at where it lands.
void guideAlong(const Arrow& a, const GuideRoute& r, const GuideTarget& t)
{
  a.settle(t);
  a.g.routed = true;
  a.g.walkable = r.walkable;
  const Steps st{r, a.grid};
  if (st.size() <= 1)
  {
    // There: face what to shoot; the exit only needs standing in.
    if (t.shoot)
    {
      a.g.dx = float(a.g.tx) + 0.5f >= a.px ? 1 : -1;
      // Low down (a crate on the floor): crouch and shoot, the arrow
      // pointing down at it.
      a.g.dy = t.thing.top() > a.pb.bottom() - 3 ? 1 : 0;
      a.g.fire = true;
    }
    else
      a.toward(float(a.g.tx) + 0.5f, t.kind == GuideKind::Exit ? a.py : float(a.g.ty) + 0.5f);
    return;
  }
  if (r.perCell == kStates && r.phase(1) != GuideRouter::kFloating)
  {
    pressOwnMoves(a, st);
    return;
  }
  const int W = a.grid.width();
  const int c = r.cell(aimStep(st));
  const int cx = c % W, cy = c / W;
  // The box there, middle to middle (the runner's own box height).
  a.toward(float(cx) + float(a.pb.w) * 0.5f, float(cy - a.pb.h + 1) + float(a.pb.h) * 0.5f);
  if (a.g.dx == 0 && a.g.dy == 0)
    a.toward(float(a.g.tx) + 0.5f, float(a.g.ty) + 0.5f);
}

// Nothing found a way: the nearest target as the crow flies.
void guideCrowFlies(const Arrow& a, const std::vector<GuideTarget>& targets)
{
  const int W = a.grid.width(), H = a.grid.height();
  std::size_t best = 0;
  float bestD = 0.0f;
  for (std::size_t t = 0; t < targets.size(); ++t)
  {
    if (targets[t].key)
      continue;
    const CellBox& b = targets[t].box;
    float vx = float(b.left()) + float(b.w) * 0.5f - a.px, vy = float(b.top()) + float(b.h) * 0.5f - a.py;
    if (a.grid.wrapX() && std::fabs(vx) > float(W) * 0.5f)
      vx = float(W) - std::fabs(vx);
    if (a.grid.wrapY() && std::fabs(vy) > float(H) * 0.5f)
      vy = float(H) - std::fabs(vy);
    const float d = vx * vx + vy * vy;
    if (t == 0 || d < bestD)
    {
      best = t;
      bestD = d;
    }
  }
  a.settle(targets[best]);
  a.g.routed = false;
  a.toward(float(a.g.tx) + 0.5f, float(a.g.ty) + 0.5f);
}

// Where the runner is in their own moves now.
GuideRouter::Start guideStart(const Player& p, const GuideGrid& grid)
{
  const CellBox pb = p.box();
  GuideRouter::Start start;
  start.cell = grid.cellAt(pb.left(), pb.bottom());
  if (p.state == PlayerState::Ladder)
    start.phase = GuideRouter::kOnLadder;
  else if (p.state == PlayerState::Pipe)
    start.phase = GuideRouter::kOnPipe;
  else if (p.state == PlayerState::Jumping)
    start.phase = std::clamp(p.frames, 1, 8);
  else if (p.state == PlayerState::Falling)
    start.phase = GuideRouter::kFall + std::clamp(p.frames, 0, 2);
  start.facing = p.facing;
  return start;
}

// With no gravity (the gunship, a vehicle in the air or the water, the
// top-down levels, gravity turned, a jetpack, swimming) only open space
// counts.
bool guideNoGravity(const World& w)
{
  const Player& p = w.player();
  const Vehicle* ride = w.riding();
  const bool flies = ride && (ride->kind == VehicleKind::Heli || ride->kind == VehicleKind::Sub || ride->kind == VehicleKind::Ship);
  return w.flight() || flies || w.topDown().on || p.grav != Grav::Down || p.state == PlayerState::Jetpack ||
    p.state == PlayerState::Swim;
}

void debugTargets(const std::vector<GuideTarget>& targets, bool fields, bool key)
{
  std::fprintf(stderr, "guide targets (fields %d key %d):", int(fields), int(key));
  for (const auto& t : targets)
    std::fprintf(stderr, " %d,%d%s", t.thing.x, t.thing.y, t.shoot ? "s" : "");
  std::fprintf(stderr, "\n");
}

void debugRoute(const GuideRoute& r, const std::vector<GuideTarget>& targets, int W)
{
  if (r.target < 0)
  {
    std::fprintf(stderr, "guide: no route\n");
    return;
  }
  const GuideTarget& t = targets[std::size_t(r.target)];
  std::fprintf(stderr, "guide: to %d,%d %dx%d%s%s%s:", t.thing.x, t.thing.y, t.thing.w, t.thing.h, t.shoot ? " shoot" : "",
    t.crate ? " crate" : "", r.perCell == 1 ? " (free)" : r.walkable ? "" : " (floating)");
  for (std::size_t j = 0; j < r.states.size(); ++j)
    std::fprintf(stderr, " %d,%d/%d", r.cell(j) % W, r.cell(j) / W, r.phase(j));
  std::fprintf(stderr, "\n");
}

} // namespace

bool World::guideOn() const
{
  if (!mPlay.guide || !mLevel->rules.empty() || mManor.split.on)
    return false; // bonus levels have rules; Both Sides splits the screen
  if (mState != WorldState::Playing)
    return false;
  return mPlayer.state != PlayerState::Dying && mPlayer.state != PlayerState::Teleporting;
}

const World::Guide& World::guide() const
{
  if (!guideOn())
  {
    mGuide = Guide{};
    mGuideAt = -1000;
    return mGuide;
  }
  int left = 0;
  const std::vector<GuideTarget> targets = guideTargets(left);
  // Again when the runner moves or something is taken, else every few
  // frames (bots move).
  const int now = mStats.frames;
  const bool moved = mPlayer.x != mGuide.fromX || mPlayer.y != mGuide.fromY;
  if (now >= mGuideAt && now - mGuideAt < kRecompute && left == mGuide.left && !moved)
    return mGuide;
  mGuideAt = now;
  mGuide = routeGuide(mGuideCache.get(), targets, left, false);
  return mGuide;
}

World::Guide World::guideFresh() const
{
  if (!guideOn())
    return Guide{};
  int left = 0;
  const std::vector<GuideTarget> targets = guideTargets(left);
  GuideCache cache;
  return routeGuide(cache, targets, left, true);
}

// What is still missing for 100% (`left` of them), then the exit, and the
// access card when a force field is up.
std::vector<GuideTarget> World::guideTargets(int& left) const
{
  std::vector<GuideTarget> targets;
  guideMissing(targets);
  left = int(targets.size());
  if (targets.empty() && mLevel->hasExit)
  {
    // Where standing on the ground leaves (player.cpp's exit zone).
    const CellBox exit{mLevel->exitTx * kCellsPerTile, (mLevel->exitTy + 1) * kCellsPerTile - 6, 2, 6};
    targets.push_back({exit, GuideKind::Exit, exit});
  }
  guideCard(targets);
  return targets;
}

// A weapon the tally still wants.
bool World::guideWantsWeapon(ItemKind k) const
{
  const int missing = mStats.weaponsTotal & ~mStats.weaponsCollected;
  switch (k)
  {
    case ItemKind::Laser:
      return (missing & weaponBit(Weapon::Laser)) != 0;
    case ItemKind::Rocket:
      return (missing & weaponBit(Weapon::Rocket)) != 0;
    case ItemKind::Flame:
      return (missing & weaponBit(Weapon::Flame)) != 0;
    default:
      return false;
  }
}

// The tally's gems, merchandise, weapons and bots, plus the G-U-N letters.
void World::guideMissing(std::vector<GuideTarget>& targets) const
{
  for (const auto& it : mItems)
  {
    if (it.taken || it.parked || it.heldBy >= 0)
      continue;
    const bool want = it.kind == ItemKind::Gem || it.kind == ItemKind::Merch || it.kind == ItemKind::LetterG ||
      it.kind == ItemKind::LetterU || it.kind == ItemKind::LetterN || guideWantsWeapon(it.kind);
    if (want)
      targets.push_back({it.box(), GuideKind::Item, it.box()});
  }
  for (const auto& b : mBoxes)
    if (b.alive && (b.content == ItemKind::Merch || b.content == ItemKind::Gem || guideWantsWeapon(b.content)))
    {
      GuideTarget t = guideShootAt(b.box(), GuideKind::Item); // a crate: shoot it open
      t.crate = true;
      targets.push_back(t);
    }
  for (const auto& pr : mProps)
    if (pr.kind == PropKind::GemCache && !pr.used && !pr.dormant)
      targets.push_back({{pr.x, pr.y, pr.w, pr.h}, GuideKind::Item, {pr.x, pr.y, pr.w, pr.h}});
  for (std::size_t i = 0; i < mEnemies.size() && i < mLevelEnemyCount; ++i)
  {
    const Enemy& e = mEnemies[i];
    if (!e.alive || e.hidden || (enemyDef(e.def).flags & kEnemyNoTally))
      continue;
    targets.push_back(guideShootAt(e.box(), GuideKind::Enemy));
  }
}

// A force field in the way: the access card first (the route tries without
// it before it goes for it).
void World::guideCard(std::vector<GuideTarget>& targets) const
{
  if (!mMap.forceFieldsOn() || mPlayer.hasKey || targets.empty())
    return;
  for (const auto& it : mItems)
    if (it.kind == ItemKind::Key && !it.taken && !it.parked && it.heldBy < 0)
    {
      GuideTarget t{it.box(), GuideKind::Item, it.box()};
      t.key = true;
      targets.push_back(t);
    }
  for (const auto& b : mBoxes)
    if (b.alive && b.content == ItemKind::Key)
    {
      GuideTarget t = guideShootAt(b.box(), GuideKind::Item);
      t.crate = t.key = true;
      targets.push_back(t);
    }
}

// All the cached grids depend on besides the map's tiles: the card (with
// it, a force field goes down as you walk up to it), the breakables any
// shot opens and the lava pools that burn.
GuideStatics World::guideStatics() const
{
  GuideStatics s;
  s.mapRevision = mMap.revision();
  s.fieldsDown = mMap.forceFieldsOn() && mPlayer.hasKey;
  for (const auto& b : mBreakables)
    if (!b.broken && b.by == 0)
      s.open.push_back({b.x0, b.y0, b.x1, b.y1});
  for (const auto& l : mLavas)
    if (!l.wade)
      s.lava.push_back({l.x0, l.y0, l.x1, l.y1});
  return s;
}

// Works the guide out with `cache`'s grids and search (`fresh`: build the
// grids from scratch whatever they were).
World::Guide World::routeGuide(GuideCache& cache, const std::vector<GuideTarget>& targets, int left, bool fresh) const
{
  Guide g;
  g.left = left;
  g.fromX = mPlayer.x;
  g.fromY = mPlayer.y;
  g.frame = mStats.frames;
  if (targets.empty())
    return g;
  if (guideDebug())
    debugTargets(targets, mMap.forceFieldsOn(), mPlayer.hasKey);

  // The world's platforms where they are now (lifts, pulleys, decks).
  std::vector<std::array<int, 3>> decks;
  for (const auto& pl : mPlatforms)
    if (!pl.dead)
      decks.push_back({pl.x, pl.y, pl.w});
  GuideGrid& grid = cache.grid;
  grid.update(mMap, guideStatics(), decks, fresh);

  const CellBox pb = mPlayer.box();
  GuideRouter::Moves moves;
  moves.arc = jumpArc();
  moves.steer = mPlayer.turbo > 0 ? 2 : 1;
  const GuideRoute r = cache.router.route(grid, targets, guideStart(mPlayer, grid), moves, guideNoGravity(*this));
  if (guideDebug())
    debugRoute(r, targets, grid.width());

  // Middle of the runner, in cells.
  const Arrow a{g, grid, pb, float(pb.left()) + float(pb.w) * 0.5f, float(pb.top()) + float(pb.h) * 0.5f};
  if (r.target >= 0)
    guideAlong(a, r, targets[std::size_t(r.target)]);
  else
    guideCrowFlies(a, targets);
  return g;
}

void World::drawGuide(Renderer& r, float camX, float camY, int frame) const
{
  const Guide& g = guide();
  if (g.kind == GuideKind::None || (g.dx == 0 && g.dy == 0))
    return;
  // Blinks: on for 26 of every 40 render frames, fading in and out.
  const int phase = frame % 40;
  if (phase >= 26)
    return;
  const float fade = std::min(1.0f, std::min(float(phase), float(26 - phase)) / 4.0f);
  const Color c = g.kind == GuideKind::Exit ? rgb(80, 255, 140) : g.kind == GuideKind::Enemy ? rgb(255, 92, 84) : rgb(255, 214, 64);

  const CellBox b = mPlayer.box();
  const float cx = (float(b.left()) + float(b.w) * 0.5f) * kCellPx - camX;
  const float cy = (float(b.top()) + float(b.h) * 0.5f) * kCellPx - camY;
  const float len = std::sqrt(float(g.dx * g.dx + g.dy * g.dy));
  const float ux = float(g.dx) / len, uy = float(g.dy) / len;
  const float bob = 6.0f * std::sin(float(frame) * 0.25f);
  // At the screen's border the way it points: where a line from the runner
  // that way meets a frame inset from the edges: below the HUD panels and
  // the TURBO / VIRUS bars, above the message line and a boss's bar, out of
  // the bottom corners (the touch stick and buttons); bobbing outwards.
  constexpr float kLeft = 56.0f, kRight = float(kScreenW) - 56.0f;
  constexpr float kTop = float(kHudPanelH) + 90.0f, kBottom = float(kScreenH) - 165.0f;
  constexpr float kCornerW = 330.0f, kCornerTop = 400.0f;
  const float ox = std::clamp(cx, kLeft, kRight), oy = std::clamp(cy, kTop, kBottom);
  float t = 1e9f;
  if (ux > 0.0f)
    t = std::min(t, (kRight - ox) / ux);
  if (ux < 0.0f)
    t = std::min(t, (kLeft - ox) / ux);
  if (uy > 0.0f)
    t = std::min(t, (kBottom - oy) / uy);
  if (uy < 0.0f)
    t = std::min(t, (kTop - oy) / uy);
  float ax = ox + ux * t, ay = oy + uy * t;
  if (ay > kCornerTop && (ax < kLeft + kCornerW || ax > kRight - kCornerW))
  {
    if (std::fabs(uy) < 0.9f && (ax <= kLeft + 1.0f || ax >= kRight - 1.0f))
      ay = kCornerTop; // on a side: up above the corner
    else
      ax = std::clamp(ax, kLeft + kCornerW, kRight - kCornerW); // on the bottom: in from it
  }
  ax += ux * bob;
  ay += uy * bob;
  drawGlow(r, mArt, ax, ay, 46.0f, c, 0.45f * fade);
  DrawOpts o;
  o.tint = c;
  o.alpha = fade;
  o.angle = std::atan2(uy, ux) * 57.2958f;
  r.draw(mArt.guideArrow, ax, ay, o);
  if (g.kind == GuideKind::Exit)
  {
    // The label on the inside of the arrow.
    const float lx = ax - ux * 70.0f, ly = ay - uy * 50.0f - 8.0f;
    r.drawText("EXIT", lx, ly, {16.0f, c, rgb(10, 8, 20), true}, Align::Center, fade);
  }

  // The target itself, when it is on screen: a small arrow over it.
  const float tx = (float(g.tx) + 0.5f) * kCellPx - camX;
  const float ty = (float(g.ty) + 0.5f) * kCellPx - camY;
  if (tx < 0.0f || tx > float(kScreenW) || ty < 0.0f || ty > float(kScreenH))
    return;
  const float dist = std::hypot(tx - cx, ty - cy);
  if (dist < 120.0f)
    return; // right there
  DrawOpts m;
  m.tint = c;
  m.alpha = fade;
  m.scale = 0.6f;
  m.angle = 90.0f; // pointing down at it
  r.draw(mArt.guideArrow, tx, ty - 56.0f - bob, m);
}

} // namespace gr
