// The guide arrow (GUIDE ARROW in SETTINGS > ASSISTS): a blinking arrow by
// the runner that points the way to the nearest thing still missing for a
// 100% level (a gem, a G-U-N letter, a box of merchandise, a weapon not had
// yet, a gem cache, a bot), then to the exit. Off in bonus levels.
//
// "Nearest" and "the way" come from a cheapest-first search over what the
// runner can do a logic frame at a time, as player.cpp does it: walking
// (turning round takes a frame), crawling, ladders and pipes, and this
// runner's own jump arc (with its hang at the top, ceilings and steering
// in the air), then falling. What that can't reach (a lift, a pulley, a
// teleport, a door still shut) is floated to through open space, as little
// as the route can; with no gravity (flying, swimming) the whole route is.
// The arrow then says what to press now: walk this way, jump (and steer),
// climb, or crouch and crawl.

#include "game/world.hpp"

#include "assets/art.hpp"
#include "render/renderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <vector>

namespace gr
{

namespace
{

constexpr int kBoxW = Player::kWidth; // 3 cells
constexpr int kBoxH = 4;              // crouched: crawlspaces count
constexpr int kStandH = 5;            // standing
constexpr int kRecompute = 4;         // logic frames between searches while the runner stands still
constexpr int kLookAhead = 10;        // cells along the route the arrow aims at
constexpr int kEnemyReachX = 8;       // close enough to shoot it: cells either side
constexpr int kEnemyReachY = 1;
constexpr int kShootGap = 2; // cells between the runner and what it shoots (the gun sticks out)

constexpr float kCellPx = float(kCellSize) * kPixelScale;

struct Target
{
  CellBox box; // where the runner has to get to: the thing itself, or close enough to shoot it
  World::GuideKind kind;
  CellBox thing; // what the arrow is for
  bool shoot = false;
  bool crate = false; // on the floor: shot crouched, from the same floor
  bool key = false;   // the access card: only when nothing else can be reached
};

// Close enough to shoot it: kEnemyReachX cells either side, about level.
Target guideShootAt(const CellBox& b, World::GuideKind kind)
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

int wrapIndex(int v, int n)
{
  v %= n;
  return v < 0 ? v + n : v;
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

  // What is still missing for 100%: the tally's gems, merchandise, weapons
  // and bots, plus the G-U-N letters.
  std::vector<Target> targets;
  for (const auto& it : mItems)
  {
    if (it.taken || it.parked || it.heldBy >= 0)
      continue;
    bool want = false;
    switch (it.kind)
    {
      case ItemKind::Gem:
      case ItemKind::Merch:
      case ItemKind::LetterG:
      case ItemKind::LetterU:
      case ItemKind::LetterN:
        want = true;
        break;
      case ItemKind::Laser:
        want = (mStats.weaponsTotal & ~mStats.weaponsCollected & weaponBit(Weapon::Laser)) != 0;
        break;
      case ItemKind::Rocket:
        want = (mStats.weaponsTotal & ~mStats.weaponsCollected & weaponBit(Weapon::Rocket)) != 0;
        break;
      case ItemKind::Flame:
        want = (mStats.weaponsTotal & ~mStats.weaponsCollected & weaponBit(Weapon::Flame)) != 0;
        break;
      default:
        break;
    }
    if (want)
      targets.push_back({it.box(), GuideKind::Item, it.box()});
  }
  for (const auto& b : mBoxes)
  {
    if (!b.alive)
      continue;
    bool want = b.content == ItemKind::Merch || b.content == ItemKind::Gem;
    if (b.content == ItemKind::Laser)
      want = (mStats.weaponsTotal & ~mStats.weaponsCollected & weaponBit(Weapon::Laser)) != 0;
    else if (b.content == ItemKind::Rocket)
      want = (mStats.weaponsTotal & ~mStats.weaponsCollected & weaponBit(Weapon::Rocket)) != 0;
    else if (b.content == ItemKind::Flame)
      want = (mStats.weaponsTotal & ~mStats.weaponsCollected & weaponBit(Weapon::Flame)) != 0;
    if (want)
    {
      Target t = guideShootAt(b.box(), GuideKind::Item); // a crate: shoot it open
      t.crate = true;
      targets.push_back(t);
    }
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
  const int left = int(targets.size());
  if (targets.empty() && mLevel->hasExit)
  {
    // Where standing on the ground leaves (player.cpp's exit zone).
    const CellBox exit{mLevel->exitTx * kCellsPerTile, (mLevel->exitTy + 1) * kCellsPerTile - 6, 2, 6};
    targets.push_back({exit, GuideKind::Exit, exit});
  }
  // A force field in the way: the access card first (the route tries
  // without it before it goes for it).
  const bool fields = mMap.forceFieldsOn();
  if (fields && !mPlayer.hasKey && !targets.empty())
  {
    for (const auto& it : mItems)
      if (it.kind == ItemKind::Key && !it.taken && !it.parked && it.heldBy < 0)
      {
        Target t{it.box(), GuideKind::Item, it.box()};
        t.key = true;
        targets.push_back(t);
      }
    for (const auto& b : mBoxes)
      if (b.alive && b.content == ItemKind::Key)
      {
        Target t = guideShootAt(b.box(), GuideKind::Item);
        t.crate = t.key = true;
        targets.push_back(t);
      }
  }

  // Again when the runner moves or something is taken, else every few
  // frames (bots move).
  const int now = mStats.frames;
  const bool moved = mPlayer.x != mGuide.fromX || mPlayer.y != mGuide.fromY;
  if (now >= mGuideAt && now - mGuideAt < kRecompute && left == mGuide.left && !moved)
    return mGuide;
  mGuideAt = now;
  mGuide = Guide{};
  mGuide.left = left;
  mGuide.fromX = mPlayer.x;
  mGuide.fromY = mPlayer.y;
  if (targets.empty())
    return mGuide;

  if (guideDebug())
  {
    std::fprintf(stderr, "guide targets (fields %d key %d):", int(mMap.forceFieldsOn()), int(mPlayer.hasKey));
    for (const auto& t : targets)
      std::fprintf(stderr, " %d,%d%s", t.thing.x, t.thing.y, t.shoot ? "s" : "");
    std::fprintf(stderr, "\n");
  }
  const int W = mMap.width(), H = mMap.height();
  const bool wrapX = mMap.wraps() || mMap.wrapsX();
  const bool wrapY = mMap.wraps();
  const int N = W * H;

  // Cells in the way: walls, except breakables any shot opens.
  static thread_local std::vector<std::uint8_t> blocked, run, fit, support;
  static thread_local std::vector<int> goal;
  blocked.assign(std::size_t(N), 0);
  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x)
      // With the card, a force field goes down as you walk up to it.
      blocked[std::size_t(y * W + x)] =
        (mMap.solid(x, y) && !(fields && mPlayer.hasKey && mMap.forceField(x, y))) || mMap.hazard(x, y);
  for (const auto& b : mBreakables)
    if (!b.broken && b.by == 0)
      for (int y = b.y0 * kCellsPerTile; y < (b.y1 + 1) * kCellsPerTile && y < H; ++y)
        for (int x = b.x0 * kCellsPerTile; x < (b.x1 + 1) * kCellsPerTile && x < W; ++x)
          if (x >= 0 && y >= 0 && !mMap.hazard(x, y))
            blocked[std::size_t(y * W + x)] = 0;
  // Lava pools (level 12) burn and throw you back: as bad as a wall.
  for (const auto& l : mLavas)
    if (!l.wade)
      for (int y = std::max(0, l.y0); y <= std::min(H - 1, l.y1); ++y)
        for (int x = std::max(0, l.x0); x <= std::min(W - 1, l.x1); ++x)
          blocked[std::size_t(y * W + x)] = 1;
  // fit: the crouched box with its bottom-left at (x, y) is clear.
  // support: 1 standing on something.
  // run: clear cells in a column up to and including this one (at most kStandH).
  run.assign(std::size_t(N), 0);
  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x)
      if (!blocked[std::size_t(y * W + x)])
        run[std::size_t(y * W + x)] = std::uint8_t(std::min(kStandH, (y > 0 ? run[std::size_t((y - 1) * W + x)] : wrapY ? kStandH - 1 : 0) + 1));
  fit.assign(std::size_t(N), 0);
  support.assign(std::size_t(N), 0);
  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x)
    {
      bool ok = true;
      for (int dx = 0; dx < kBoxW && ok; ++dx)
      {
        const int cx = wrapX ? wrapIndex(x + dx, W) : x + dx;
        ok = cx >= 0 && cx < W && run[std::size_t(y * W + cx)] >= kBoxH;
      }
      if (!ok)
        continue;
      const std::size_t i = std::size_t(y * W + x);
      fit[i] = 1;
      bool stand = false;
      for (int dx = 0; dx < kBoxW && !stand; ++dx)
        stand = mMap.solidTop(wrapX ? wrapIndex(x + dx, W) : x + dx, y + 1);
      support[i] = stand ? 1 : 0;
    }

  // Platforms where they are now (lifts, pulleys, decks): stood on like a
  // one-way floor. They move on; the route is searched again as they do.
  for (const auto& pl : mPlatforms)
  {
    if (pl.dead)
      continue;
    const int y = pl.y - 1;
    if (y < 0 || y >= H)
      continue;
    for (int x = pl.x - kBoxW + 1; x <= pl.x + pl.w - 1; ++x)
    {
      const int gx = wrapX ? wrapIndex(x, W) : x;
      if (gx < 0 || gx >= W)
        continue;
      const std::size_t i = std::size_t(y * W + gx);
      if (fit[i])
        support[i] |= 1;
    }
  }

  // Goal cells: where the box touches a target (a bot or a crate: close
  // enough to shoot it, beside it rather than in it, so the shot hits).
  goal.assign(std::size_t(N), 0);
  for (std::size_t t = 0; t < targets.size(); ++t)
  {
    if (targets[t].key)
      continue;
    const CellBox& b = targets[t].box;
    const CellBox& th = targets[t].thing;
    for (int y = b.top(); y <= b.bottom() + kStandH - 1; ++y) // the standing box (crouched only to crawl)
      for (int x = b.left() - kBoxW + 1; x <= b.right(); ++x)
      {
        if (targets[t].shoot && x <= th.right() + kShootGap && x + kBoxW - 1 >= th.left() - kShootGap)
          continue;
        if (targets[t].crate && y != th.bottom())
          continue;
        if (targets[t].crate)
        {
          // Crouching over the top of a ladder climbs down it instead.
          bool ladderBelow = false;
          for (int dx = 0; dx < kBoxW && !ladderBelow; ++dx)
          {
            const int lx = wrapX ? wrapIndex(x + dx, W) : x + dx;
            ladderBelow = lx >= 0 && lx < W && y + 1 < H && mMap.ladder(lx, y + 1);
          }
          if (ladderBelow)
            continue;
        }
        int gx = x, gy = y;
        if (wrapX)
          gx = wrapIndex(gx, W);
        if (wrapY)
          gy = wrapIndex(gy, H);
        if (gx < 0 || gx >= W || gy < 0 || gy >= H)
          continue;
        int& g = goal[std::size_t(gy * W + gx)];
        if (g == 0)
          g = int(t) + 1;
      }
  }

  const CellBox pb = mPlayer.box();
  const int sx = wrapX ? wrapIndex(pb.left(), W) : pb.left();
  const int sy = wrapY ? wrapIndex(pb.bottom(), H) : pb.bottom();
  const Vehicle* ride = riding();
  const bool flies = ride && (ride->kind == VehicleKind::Heli || ride->kind == VehicleKind::Sub || ride->kind == VehicleKind::Ship);
  const bool freeOnly = mFlight || flies || mTop.on || mPlayer.grav != Grav::Down ||
    mPlayer.state == PlayerState::Jetpack || mPlayer.state == PlayerState::Swim;

  auto cellAt = [&](int x, int y) {
    if (wrapX)
      x = wrapIndex(x, W);
    if (wrapY)
      y = wrapIndex(y, H);
    return x < 0 || x >= W || y < 0 || y >= H ? -1 : y * W + x;
  };
  auto fitAt = [&](int x, int y) {
    const int c = cellAt(x, y);
    return c >= 0 && fit[std::size_t(c)] != 0;
  };

  // Cheapest first, a state at a time: P states per cell, expand(s, cost,
  // push) offers the next ones. Returns the goal state reached, -1 for
  // none; prev links each state to the one before it.
  static thread_local std::vector<int> prev, cost;
  static thread_local std::array<std::vector<int>, 32> buckets; // costs mod 32 (a step costs 1..31)
  bool allowKey = false;
  auto search = [&](int P, int s0, const auto& expand) {
    prev.assign(std::size_t(N * P), -1);
    cost.assign(std::size_t(N * P), 0x7fffffff);
    for (auto& q : buckets)
      q.clear();
    if (s0 < 0)
      return -1;
    prev[std::size_t(s0)] = s0;
    cost[std::size_t(s0)] = 0;
    buckets[0].push_back(s0);
    int pending = 1;
    for (int c = 0; pending > 0; ++c)
    {
      auto& bucket = buckets[std::size_t(c & 31)];
      for (std::size_t q = 0; q < bucket.size(); ++q)
      {
        const int s = bucket[q];
        --pending;
        if (cost[std::size_t(s)] != c)
          continue; // reached cheaper since
        if (const int g = goal[std::size_t(s / P)]; g != 0)
        {
          // Shooting needs the ground under you (in the runner's own moves).
          const Target& t = targets[std::size_t(g - 1)];
          if ((allowKey || !t.key) && (P == 1 || !t.shoot || ((s % P) / 2 == 0 && (support[std::size_t(s / P)] & 1))))
            return s;
        }
        // P states per cell: two facings of each phase (P == 1: just cells).
        expand(s, [&](int cell, int phase, int step, int face = 1) {
          if (cell < 0)
            return;
          const int ns = P == 1 ? cell : cell * P + phase * 2 + (face > 0 ? 1 : 0);
          if (cost[std::size_t(ns)] <= c + step)
            return;
          cost[std::size_t(ns)] = c + step;
          prev[std::size_t(ns)] = s;
          buckets[std::size_t((c + step) & 31)].push_back(ns);
          ++pending;
        });
      }
      bucket.clear();
    }
    return -1;
  };

  // Any open space, eight ways, no gravity (flying, swimming, and what the
  // runner's own moves can't reach).
  auto expandFree = [&](int s, const auto& push) {
    const int x = s % W, y = s / W;
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx)
      {
        if ((dx == 0 && dy == 0) || !fitAt(x + dx, y + dy))
          continue;
        if (dx != 0 && dy != 0 && !fitAt(x + dx, y) && !fitAt(x, y + dy))
          continue; // no squeezing between two corners
        push(cellAt(x + dx, y + dy), 0, dx != 0 && dy != 0 ? 3 : 2);
      }
  };

  // The runner's own moves, a logic frame a step, as player.cpp makes them.
  // Phases: 0 standing; 1..8 jumping, that frame of the jump arc next (a 0
  // in the arc hangs a frame at the top; 8: the arc is over, falling
  // starts); 9..11 falling (a cell a frame, a cell, then two); 12 floating
  // (below); 13 on a ladder; 14 hanging from a pipe. In the air the runner
  // steers a cell a frame (two with Turbo; not on a jump's first frame,
  // unless it leaves a ladder or a pipe); walking and climbing are a cell a
  // frame. Walking costs 2 a frame, the air 2 a frame and a jump 3 more to
  // start, so a route walks where it can.
  const std::array<int, 8> arc = jumpArc();
  const int steer = mPlayer.turbo > 0 ? 2 : 1;
  constexpr int P = 30; // 15 phases, facing left or right (turning round takes a frame)
  constexpr int kFall = 9;
  // Floating: where only something the runner can't do by themselves gets
  // them (a lift, a pulley, a vehicle, a switch): a step through open space,
  // dear (kFloat) so a route floats as little as it can. Only when the
  // runner's own moves reach nothing.
  constexpr int kFloating = 12;
  constexpr int kOnLadder = 13;
  constexpr int kOnPipe = 14;
  constexpr int kFloat = 20;
  bool hybrid = false;
  auto tallAt = [&](int x, int y) { // the standing box (jumping, falling, climbing) is clear
    if (!fitAt(x, y))
      return false;
    for (int dx = 0; dx < kBoxW; ++dx)
    {
      const int c = cellAt(x + dx, y);
      if (c < 0 || run[std::size_t(c)] < kStandH)
        return false;
    }
    return true;
  };
  auto ladderAt = [&](int x, int y) {
    if (wrapX)
      x = wrapIndex(x, W);
    if (wrapY)
      y = wrapIndex(y, H);
    return x >= 0 && x < W && y >= 0 && y < H && mMap.ladder(x, y);
  };
  auto pipeAt = [&](int x, int y) {
    if (wrapX)
      x = wrapIndex(x, W);
    if (wrapY)
      y = wrapIndex(y, H);
    return x >= 0 && x < W && y >= 0 && y < H && mMap.climbable(x, y);
  };
  auto wallAt = [&](int x, int y) {
    const int c = cellAt(x, y);
    return c < 0 || blocked[std::size_t(c)] != 0;
  };
  // Up onto a ladder (any column of the box's top row on it): the runner's
  // middle snaps onto it.
  auto grabLadder = [&](int x, int y, int step, int face, const auto& push) {
    for (int dx = 0; dx < kBoxW; ++dx)
      if (ladderAt(x + dx, y - kStandH + 1) && tallAt(x + dx - 1, y))
      {
        push(cellAt(x + dx - 1, y), kOnLadder, step, face);
        return;
      }
  };
  // Steering against the way the runner faces only turns them round.
  auto steerBy = [&](int& x, int y, int dx, int& face) {
    if (dx == 0)
      return;
    if (dx != face)
    {
      face = dx;
      return;
    }
    for (int k = 0; k < steer && tallAt(x + dx, y); ++k)
      x += dx;
  };
  // Falling `down` cells from (x, y) this frame: lands, catches a pipe, or
  // falls on as `next`.
  auto fallBy = [&](int x, int y, int down, int next, int face, const auto& push) {
    for (int k = 0; k < down; ++k)
    {
      const int c = cellAt(x, y);
      if (c < 0)
        return;
      if (support[std::size_t(c)] & 1)
      {
        push(c, 0, 2, face); // landed
        return;
      }
      if (pipeAt(x + 1, y - kStandH + 1) && tallAt(x, y + 1))
      {
        push(cellAt(x, y + 1), kOnPipe, 2, face); // caught a pipe
        return;
      }
      if (!tallAt(x, y + 1))
        return; // out of the bottom of the map
      ++y;
    }
    push(cellAt(x, y), next, 2, face);
  };
  // One frame of a jump from (x, y), arc frame f (steered dx first). Pipes
  // overhead are caught on the way up (not on the first frame).
  auto jumpFrame = [&](int x, int y, int dx, int f, int face, const auto& push) {
    steerBy(x, y, dx, face);
    if (f >= 8)
    {
      fallBy(x, y, 1, kFall, face, push); // the arc is over: falling, a cell now
      return;
    }
    const int up = arc[std::size_t(f)];
    if (f > 0 && pipeAt(x + 1, y - kStandH))
    {
      push(cellAt(x, y), kOnPipe, 2, face);
      return;
    }
    int moved = 0;
    while (moved < up && tallAt(x, y - 1))
    {
      --y;
      ++moved;
      if (f > 0 && pipeAt(x + 1, y - kStandH))
      {
        push(cellAt(x, y), kOnPipe, 2, face);
        return;
      }
    }
    if (moved == up)
      push(cellAt(x, y), f + 1, 2, face);
    else if (up == 2 && moved == 1)
      push(cellAt(x, y), 4, 2, face); // half a rise under a ceiling: the arc skips on
    else
      fallBy(x, y, 1, kFall, face, push); // a bump on the head: falling
  };
  auto expandArcs = [&](int s, const auto& push) {
    const int i = s / P, phase = (s % P) / 2, face = (s & 1) ? 1 : -1;
    const int x = i % W, y = i / W;
    const bool feet = (support[std::size_t(i)] & 1) != 0;
    // A jump's first frame (a jump costs 3 more than its frames).
    auto jumpFrom = [&](int jx, int jy, int dx) {
      jumpFrame(jx, jy, dx, 0, face, [&](int c, int ph, int step, int f) { push(c, ph, step + 3, f); });
    };
    if (hybrid)
    {
      expandFree(i, [&](int c, int, int step, int = 1) { push(c, kFloating, kFloat + step - 2, face); });
      if (phase == kFloating)
      {
        if (feet)
          push(i, 0, 1, face); // let go
        else
          grabLadder(x, y, 1, face, push);
        push(i, kFall, 1, face);
        return;
      }
    }
    if (phase == 0)
    {
      if (!feet)
      {
        push(i, kFall, 1, face); // nothing under you: falling
        return;
      }
      for (int dx = -1; dx <= 1; dx += 2)
      {
        if (dx != face)
          push(i, 0, 1, dx); // turning round takes a frame
        else if (fitAt(x + dx, y))
        {
          const int c = cellAt(x + dx, y);
          if (support[std::size_t(c)] & 1)
            push(c, 0, 2, face); // a step
          else if (tallAt(x + dx, y))
            fallBy(x + dx, y, 1, kFall, face, push); // off the edge: down a cell at once
        }
        else if (fitAt(x + dx, y - 1) && (support[std::size_t(cellAt(x + dx, y - 1))] & 1))
          push(cellAt(x + dx, y - 1), 0, 2, face); // a stair
      }
      if (tallAt(x, y))
        grabLadder(x, y, 2, face, push); // (not from a crawl: up does nothing there)
      // Down onto a ladder below the feet (room for it beneath).
      for (int dx = 0; dx < kBoxW; ++dx)
        if (ladderAt(x + dx, y + 1))
        {
          const int sx2 = x + dx - 1;
          bool clear = tallAt(sx2, y);
          for (int k = 0; k < kBoxW && clear; ++k)
            clear = !wallAt(sx2 + k, y + 1);
          if (clear)
            push(cellAt(sx2, y), kOnLadder, 2, face);
          break;
        }
      // A jump, with headroom for it.
      if (tallAt(x, y) && tallAt(x, y - 1))
        jumpFrom(x, y, 0);
      return;
    }
    if (phase == kOnLadder)
    {
      if (ladderAt(x + 1, y - kStandH) && tallAt(x, y - 1))
        push(cellAt(x, y - 1), kOnLadder, 2, face); // up
      if (ladderAt(x + 1, y + 1))
      {
        bool clear = true;
        for (int k = 0; k < kBoxW && clear; ++k)
          clear = !wallAt(x + k, y + 1);
        if (clear)
          push(cellAt(x, y + 1), kOnLadder, 2, face); // down
      }
      else if (feet)
        push(i, 0, 1, face); // down off the foot of it
      else
        fallBy(x, y, 1, kFall, face, push); // down off the bottom: falling
      // Jumping off it, steering on the first frame (the other way: turning
      // round on it).
      if (tallAt(x, y - 1))
        for (int dx = -1; dx <= 1; ++dx)
          jumpFrom(x, y, dx);
      return;
    }
    if (phase == kOnPipe)
    {
      // Hand over hand while the pipe goes on.
      for (int dx = -1; dx <= 1; dx += 2)
        if (dx != face)
          push(i, kOnPipe, 1, dx);
        else if (tallAt(x + dx, y))
          push(cellAt(x + dx, y), pipeAt(dx < 0 ? x : x + kBoxW - 1, y - kStandH) ? kOnPipe : kFall, 2, face);
      push(i, kFall, 2, face); // let go
      // Up and jump: a jump off the pipe.
      if (tallAt(x, y - 1) && tallAt(x, y - 2))
        for (int dx = -1; dx <= 1; ++dx)
          jumpFrom(x, y - 1, dx);
      return;
    }
    // In the air, up grabs a ladder (a jump only from its third frame).
    // Dearer than walking to it and climbing, so a route only does it when
    // it has to.
    if (phase >= 3)
      grabLadder(x, y, 4, face, push);
    for (int dx = -1; dx <= 1; ++dx)
    {
      if (phase <= 8)
        jumpFrame(x, y, dx, phase, face, push);
      else
      {
        const int k = phase - kFall;
        int nx = x, nf = face;
        steerBy(nx, y, dx, nf);
        fallBy(nx, y, k >= 2 ? 2 : 1, kFall + std::min(k + 1, 2), nf, push);
      }
    }
  };

  int P0 = 1;
  int found = -1;
  if (!freeOnly)
  {
    // Where the runner is in their own moves now.
    const int c0 = cellAt(sx, sy);
    int ph = 0;
    if (mPlayer.state == PlayerState::Ladder)
      ph = kOnLadder;
    else if (mPlayer.state == PlayerState::Pipe)
      ph = kOnPipe;
    else if (mPlayer.state == PlayerState::Jumping && mPlayer.airArc)
      ph = kFall; // a double jump is not modelled: plan from a fall (the arrow never asks for one)
    else if (mPlayer.state == PlayerState::Jumping)
      ph = std::clamp(mPlayer.frames, 1, 8);
    else if (mPlayer.state == PlayerState::Falling)
      ph = kFall + std::clamp(mPlayer.frames, 0, 2);
    P0 = P;
    found = search(P, c0 < 0 ? -1 : c0 * P + ph * 2 + (mPlayer.facing > 0 ? 1 : 0), expandArcs);
    if (found < 0 && targets.back().key)
    {
      allowKey = true; // nothing else in reach: the card
      found = search(P, c0 < 0 ? -1 : c0 * P + ph * 2 + (mPlayer.facing > 0 ? 1 : 0), expandArcs);
    }
    mGuide.walkable = found >= 0;
    if (found < 0)
    {
      hybrid = true; // floating where it must
      allowKey = false;
      found = search(P, c0 < 0 ? -1 : c0 * P + ph * 2 + (mPlayer.facing > 0 ? 1 : 0), expandArcs);
      if (found < 0 && targets.back().key)
      {
        allowKey = true;
        found = search(P, c0 < 0 ? -1 : c0 * P + ph * 2 + (mPlayer.facing > 0 ? 1 : 0), expandArcs);
      }
    }
    if (guideDebug())
      std::fprintf(stderr, "guide arcs from %d,%d/%d: %d\n", sx, sy, ph, found);
  }
  if (found < 0)
  {
    if (guideDebug())
      std::fprintf(stderr, "guide free (only %d)\n", int(freeOnly));
    P0 = 1;
    allowKey = false;
    found = search(1, cellAt(sx, sy), expandFree);
    if (found < 0 && targets.back().key)
    {
      allowKey = true;
      found = search(1, cellAt(sx, sy), expandFree);
    }
  }
  const int A = P0;

  // Middle of the runner and of a target, in cells.
  const float px = float(pb.left()) + float(pb.w) * 0.5f;
  const float py = float(pb.top()) + float(pb.h) * 0.5f;
  auto toward = [&](float tx, float ty) {
    float vx = tx - px, vy = ty - py;
    if (wrapX && std::fabs(vx) > float(W) * 0.5f)
      vx -= std::copysign(float(W), vx);
    if (wrapY && std::fabs(vy) > float(H) * 0.5f)
      vy -= std::copysign(float(H), vy);
    if (std::fabs(vx) < 0.5f && std::fabs(vy) < 0.5f)
      return;
    // Eight ways, 45 degrees each.
    const float ang = std::atan2(vy, vx);
    const int oct = int(std::lround(ang / (3.14159265f / 4.0f)));
    static const int kDx[8] = {1, 1, 0, -1, -1, -1, 0, 1};
    static const int kDy[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    const int o = wrapIndex(oct, 8);
    mGuide.dx = kDx[o];
    mGuide.dy = kDy[o];
  };
  auto settle = [&](const Target& t) {
    mGuide.kind = t.kind;
    mGuide.tx = t.thing.left() + t.thing.w / 2;
    mGuide.ty = t.thing.top() + t.thing.h / 2;
  };

  if (found >= 0)
  {
    const int ti = goal[std::size_t(found / A)] - 1;
    settle(targets[std::size_t(ti)]);
    const Target& t0 = targets[std::size_t(ti)];
    mGuide.routed = true;
    // Walk the route back to the start (it runs goal to start).
    std::vector<int> states;
    for (int s = found;; s = prev[std::size_t(s)])
    {
      states.push_back(s);
      if (prev[std::size_t(s)] == s)
        break;
    }
    std::reverse(states.begin(), states.end());
    std::vector<int> path(states.size());
    for (std::size_t j = 0; j < states.size(); ++j)
      path[j] = states[j] / A;
    auto phaseAt = [&](std::size_t j) { return A == P ? (states[j] % P) / 2 : 0; };
    if (guideDebug())
    {
      const Target& td = targets[std::size_t(ti)];
      std::fprintf(stderr, "guide: to %d,%d %dx%d%s%s:", td.thing.x, td.thing.y, td.thing.w, td.thing.h, td.shoot ? " shoot" : "",
        td.crate ? " crate" : "");
      for (std::size_t j = 0; j < path.size(); ++j)
        std::fprintf(stderr, " %d,%d/%d", path[j] % W, path[j] / W, phaseAt(j));
      std::fprintf(stderr, "\n");
    }
    if (path.size() <= 1)
    {
      // There: face what to shoot; the exit only needs standing in.
      if (t0.shoot)
      {
        mGuide.dx = float(mGuide.tx) + 0.5f >= px ? 1 : -1;
        // Low down (a crate on the floor): crouch and shoot, the arrow
        // pointing down at it.
        mGuide.dy = t0.thing.top() > pb.bottom() - 3 ? 1 : 0;
        mGuide.fire = true;
      }
      else
        toward(float(mGuide.tx) + 0.5f, t0.kind == GuideKind::Exit ? py : float(mGuide.ty) + 0.5f);
      return mGuide;
    }
    // Where the arrow aims depends on the route's first move: a walk (or a
    // climb) aims where it ends (the foot of the ladder, the edge to jump
    // from, the ladder's top), so the arrow says "this way" first and "up"
    // or "jump" once you are there; in the air it says how to steer; a
    // fall aims at where it lands. Searching with no gravity, kLookAhead
    // steps along.
    std::size_t k = std::min<std::size_t>(std::size_t(kLookAhead), path.size() - 1);
    auto stepX = [&](std::size_t j) {
      if (j + 1 >= path.size())
        return 0;
      int d = path[j + 1] % W - path[j] % W;
      if (wrapX && std::abs(d) > W / 2)
        d -= d > 0 ? W : -W;
      return d > 0 ? 1 : d < 0 ? -1 : 0;
    };
    // Sideways this step: a move, or turning round (which takes a frame).
    auto moveX = [&](std::size_t j) {
      if (const int d = stepX(j); d != 0 || A != P || j + 1 >= path.size())
        return d;
      return (states[j] & 1) != (states[j + 1] & 1) ? ((states[j + 1] & 1) ? 1 : -1) : 0;
    };
    auto stepY = [&](std::size_t j) {
      if (j + 1 >= path.size())
        return 0;
      int d = path[j + 1] / W - path[j] / W;
      if (wrapY && std::abs(d) > H / 2)
        d -= d > 0 ? H : -H;
      return d > 0 ? 1 : d < 0 ? -1 : 0;
    };
    auto rising = [](int ph) { return ph >= 1 && ph <= 8; };
    bool walk = false;
    if (A == P && phaseAt(1) != kFloating)
    {
      // The runner's own moves: what to press now.
      const int ph0 = phaseAt(0), ph1 = phaseAt(1);
      int dx = 0, dy = 0;
      if (ph0 == 0)
      {
        if (ph1 == 0)
        {
          dx = moveX(0); // walk (crouched into a crawlspace, below)
          walk = true;
        }
        else if (ph1 == kOnLadder)
          dy = path.size() > 2 && phaseAt(2) == kOnLadder && stepY(1) > 0 ? 1 : -1; // onto the ladder
        else if (rising(ph1))
        {
          dy = -1; // jump, steering from its second frame
          dx = moveX(1);
        }
        else
          dx = moveX(0); // off the edge
        if (dx == 0 && dy == 0)
          dy = 1;
      }
      else if (ph0 == kOnLadder)
      {
        if (ph1 == kOnLadder)
          dy = stepY(0);
        else if (rising(ph1))
        {
          dy = -1; // jump off it: sideways says jump, not climb
          dx = moveX(0) != 0 ? moveX(0) : moveX(1);
        }
        else
          dy = 1; // down off its foot
      }
      else if (ph0 == kOnPipe)
      {
        dx = moveX(0);
        if (rising(ph1))
          dy = -1; // up and jump
        else if (ph1 != kOnPipe && dx == 0)
          dy = 1; // let go
      }
      else
      {
        // In the air: steer this frame; up while it rises and to grab a
        // ladder, down while it falls.
        dx = moveX(0);
        if (ph1 == kOnLadder)
        {
          dx = 0;
          dy = -1;
        }
        else
          dy = rising(ph1) || ph1 == kOnPipe ? -1 : ph1 == 0 ? 0 : 1;
      }
      mGuide.dx = dx;
      mGuide.dy = dy;
      // A walk into a crawlspace, the next step: down that way (crouch and
      // crawl; crouching any sooner only crouches).
      if (walk && path.size() > 1)
      {
        const int q = path[1];
        for (int c = 0; c < kBoxW; ++c)
        {
          const int qx = wrapX ? wrapIndex(q % W + c, W) : q % W + c;
          if (qx < 0 || qx >= W || run[std::size_t((q / W) * W + qx)] < kStandH)
          {
            mGuide.dy = 1;
            break;
          }
        }
      }
      return mGuide;
    }
    if (A == P)
    {
      // Floating: kLookAhead steps along it.
      k = 1;
      while (k + 1 < path.size() && k < std::size_t(kLookAhead) && phaseAt(k + 1) == kFloating)
        ++k;
    }
    else
    {
      // No gravity: a walk aims where it ends, else kLookAhead steps on.
      std::size_t run1 = 0;
      auto flat = [&](std::size_t j) { return path[j] / W == path[j + 1] / W && (support[std::size_t(path[j])] & 1); };
      while (run1 + 1 < path.size() && run1 < k && flat(run1))
        ++run1;
      if (run1 >= 1 && run1 < k)
        k = run1;
    }
    const int c = path[k];
    const int cx = c % W, cy = c / W;
    // The box there, middle to middle (the runner's own box height).
    toward(float(cx) + float(pb.w) * 0.5f, float(cy - pb.h + 1) + float(pb.h) * 0.5f);
    if (mGuide.dx == 0 && mGuide.dy == 0)
      toward(float(mGuide.tx) + 0.5f, float(mGuide.ty) + 0.5f);
    return mGuide;
  }

  // Nothing found a way: the nearest target as the crow flies.
  std::size_t best = 0;
  float bestD = 0.0f;
  for (std::size_t t = 0; t < targets.size(); ++t)
  {
    if (targets[t].key)
      continue;
    const CellBox& b = targets[t].box;
    float vx = float(b.left()) + float(b.w) * 0.5f - px, vy = float(b.top()) + float(b.h) * 0.5f - py;
    if (wrapX && std::fabs(vx) > float(W) * 0.5f)
      vx = float(W) - std::fabs(vx);
    if (wrapY && std::fabs(vy) > float(H) * 0.5f)
      vy = float(H) - std::fabs(vy);
    const float d = vx * vx + vy * vy;
    if (t == 0 || d < bestD)
    {
      best = t;
      bestD = d;
    }
  }
  settle(targets[best]);
  mGuide.routed = false;
  toward(float(mGuide.tx) + 0.5f, float(mGuide.ty) + 0.5f);
  return mGuide;
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
