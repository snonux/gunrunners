// The guide arrow (GUIDE ARROW in ASSISTS AND OPTIONS): a blinking arrow by
// the runner that points the way to the nearest thing still missing for a
// 100% level (a gem, a G-U-N letter, a box of merchandise, a weapon not had
// yet, a gem cache, a bot), then to the exit. Off in bonus levels.
//
// "Nearest" and "the way" come from a breadth-first search over the
// runner's possible positions (bottom-left cell, crouched box): walking,
// climbing ladders and pipes, dropping through one-way floors, and jumps
// as high as this runner's jump with some drift, falling only downwards.
// What that can't reach (a ride, a teleport, a door still shut) falls back
// to a search through any open space, then to a straight line.

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
// Search costs: a step walked, climbed or fallen straight 2 (3 diagonally),
// a step of a jump or steering a fall 5, so a route walks and climbs where
// it can and only jumps where it has to.
constexpr int kAirStep = 5;

constexpr float kCellPx = float(kCellSize) * kPixelScale;

struct Target
{
  CellBox box; // where the runner has to get to: the thing itself, or close enough to shoot it
  World::GuideKind kind;
  CellBox thing; // what the arrow is for
  bool shoot = false;
  bool crate = false; // on the floor: shot crouched, from the same floor
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

  const int W = mMap.width(), H = mMap.height();
  const bool wrapX = mMap.wraps() || mMap.wrapsX();
  const bool wrapY = mMap.wraps();
  const int N = W * H;

  // Cells in the way: walls, except breakables any shot opens.
  static thread_local std::vector<std::uint8_t> blocked, run, fit, support, climb;
  static thread_local std::vector<int> goal;
  blocked.assign(std::size_t(N), 0);
  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x)
      blocked[std::size_t(y * W + x)] = mMap.solid(x, y) || mMap.hazard(x, y);
  for (const auto& b : mBreakables)
    if (!b.broken && b.by == 0)
      for (int y = b.y0 * kCellsPerTile; y < (b.y1 + 1) * kCellsPerTile && y < H; ++y)
        for (int x = b.x0 * kCellsPerTile; x < (b.x1 + 1) * kCellsPerTile && x < W; ++x)
          if (x >= 0 && y >= 0 && !mMap.hazard(x, y))
            blocked[std::size_t(y * W + x)] = 0;
  // fit: the crouched box with its bottom-left at (x, y) is clear.
  // support: bit 1 standing on something, bit 2 on a ladder or a pipe (either: a fresh jump).
  // climb: a ladder or a pipe holds you (up and down without a jump).
  // run: clear cells in a column up to and including this one (at most kStandH).
  run.assign(std::size_t(N), 0);
  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x)
      if (!blocked[std::size_t(y * W + x)])
        run[std::size_t(y * W + x)] = std::uint8_t(std::min(kStandH, (y > 0 ? run[std::size_t((y - 1) * W + x)] : wrapY ? kStandH - 1 : 0) + 1));
  fit.assign(std::size_t(N), 0);
  support.assign(std::size_t(N), 0);
  climb.assign(std::size_t(N), 0);
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
      bool hold = false;
      for (int dx = 0; dx < kBoxW && !hold; ++dx)
        for (int dy = -1; dy <= kBoxH && !hold; ++dy)
          hold = mMap.ladder(wrapX ? wrapIndex(x + dx, W) : x + dx, y - dy);
      for (int dx = 0; dx < kBoxW && !hold; ++dx)
        for (int dy = kBoxH; dy <= kBoxH + 2 && !hold; ++dy)
          hold = mMap.climbable(wrapX ? wrapIndex(x + dx, W) : x + dx, y - dy);
      bool stand = false;
      for (int dx = 0; dx < kBoxW && !stand; ++dx)
        stand = mMap.solidTop(wrapX ? wrapIndex(x + dx, W) : x + dx, y + 1);
      // A ladder goes on up while the standing box's top row is on it.
      bool goesUp = false;
      for (int dx = 0; dx < kBoxW && !goesUp; ++dx)
        goesUp = mMap.ladder(wrapX ? wrapIndex(x + dx, W) : x + dx, y - kStandH + 1);
      climb[i] = std::uint8_t((hold ? 1 : 0) | (goesUp ? 2 : 0));
      support[i] = std::uint8_t((stand ? 1 : 0) | (hold ? 2 : 0));
    }

  // Goal cells: where the box touches a target (a bot or a crate: close
  // enough to shoot it, beside it rather than in it, so the shot hits).
  goal.assign(std::size_t(N), 0);
  for (std::size_t t = 0; t < targets.size(); ++t)
  {
    const CellBox& b = targets[t].box;
    const CellBox& th = targets[t].thing;
    for (int y = b.top(); y <= b.bottom() + kBoxH - 1; ++y)
      for (int x = b.left() - kBoxW + 1; x <= b.right(); ++x)
      {
        if (targets[t].shoot && x <= th.right() + kShootGap && x + kBoxW - 1 >= th.left() - kShootGap)
          continue;
        if (targets[t].crate && y != th.bottom())
          continue;
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
  int J = 0;
  for (const int c : jumpArc())
    J += c;
  J = std::max(J - 1, 4); // a cell short of the top: the arc has to clear an edge too

  // Up a ladder to (x, y): its top row still on the ladder.
  auto ladderUp = [&](int x, int y) {
    if (wrapY)
      y = wrapIndex(y, H);
    return y >= 0 && y < H && (climb[std::size_t(y * W + x)] & 2) != 0;
  };

  // One search: free = any open space, no gravity. Cheapest first (walking
  // costs least, then climbing, then a jump or a fall, so a route only
  // jumps where it has to). Returns the goal state found, -1 for none; prev
  // links each state to the one before it.
  static thread_local std::vector<int> prev, cost;
  static thread_local std::array<std::vector<int>, 8> buckets; // costs mod 8 (a step costs 2..5)
  auto search = [&](bool free, int A) {
    const int S = N * A;
    prev.assign(std::size_t(S), -1);
    cost.assign(std::size_t(S), 0x7fffffff);
    for (auto& q : buckets)
      q.clear();
    if (sx < 0 || sx >= W || sy < 0 || sy >= H)
      return -1;
    const std::size_t si = std::size_t(sy * W + sx);
    const int sa = free || support[si] || mPlayer.state == PlayerState::Jumping ? A - 1 : 0;
    const int s0 = int(si) * A + sa;
    prev[std::size_t(s0)] = s0;
    cost[std::size_t(s0)] = 0;
    buckets[0].push_back(s0);
    int pending = 1;
    for (int c = 0; pending > 0; ++c)
    {
      auto& bucket = buckets[std::size_t(c & 7)];
      for (std::size_t q = 0; q < bucket.size(); ++q)
      {
        const int s = bucket[q];
        --pending;
        if (cost[std::size_t(s)] != c)
          continue; // reached cheaper since
        const int i = s / A;
        if (goal[std::size_t(i)] != 0)
          return s;
        const int x = i % W, y = i / W;
        const bool feet = (support[std::size_t(i)] & 1) != 0; // standing on something
        const bool hold = climb[std::size_t(i)] != 0;
        const int a = feet ? A - 1 : s % A;
        for (int dy = -1; dy <= 1; ++dy)
          for (int dx = -1; dx <= 1; ++dx)
          {
            if (dx == 0 && dy == 0)
              continue;
            int na = A - 1;
            int step = dx != 0 && dy != 0 ? 3 : 2;
            if (!free)
            {
              if (dy < 0)
              {
                // Up: climbing, or the rising part of a jump.
                if (hold && dx == 0 && ladderUp(x, y - 1))
                  na = A - 1;
                else if (a > 0)
                {
                  na = a - 1;
                  step = kAirStep;
                }
                else
                  continue;
              }
              else if (dy == 0)
              {
                // Sideways: walking, or drifting near a jump's top (or
                // jumping off a ladder).
                if (feet)
                  na = A - 1;
                else if (a > 0)
                {
                  na = a - 1;
                  step = kAirStep;
                }
                else
                  continue;
              }
              else
                na = 0; // down: falling (or down a ladder, a fresh start below)
              if (dy > 0 && !feet && dx != 0)
                step = kAirStep; // steering a fall
            }
            int nx = x + dx, ny = y + dy;
            if (wrapX)
              nx = wrapIndex(nx, W);
            if (wrapY)
              ny = wrapIndex(ny, H);
            if (nx < 0 || nx >= W || ny < 0 || ny >= H)
              continue;
            const std::size_t ni = std::size_t(ny * W + nx);
            if (!fit[ni])
              continue;
            if (dx != 0 && dy != 0)
            {
              // No squeezing between two corners.
              const int cx = wrapX ? wrapIndex(x + dx, W) : x + dx;
              const int cy = wrapY ? wrapIndex(y + dy, H) : y + dy;
              if (!fit[std::size_t(y * W + cx)] && !fit[std::size_t(cy * W + x)])
                continue;
            }
            // Landing on something is a fresh start; so is a ladder or a
            // pipe climbed onto (not one a jump only passes through).
            if (!free && ((support[ni] & 1) || ((support[ni] & 2) && dx == 0)))
              na = A - 1;
            const int ns = int(ni) * A + na;
            if (cost[std::size_t(ns)] <= c + step)
              continue;
            cost[std::size_t(ns)] = c + step;
            prev[std::size_t(ns)] = s;
            buckets[std::size_t((c + step) & 7)].push_back(ns);
            ++pending;
          }
      }
      bucket.clear();
    }
    return -1;
  };

  int A = 1;
  int found = -1;
  if (!freeOnly)
  {
    A = J + 1;
    found = search(false, A);
    mGuide.walkable = found >= 0;
  }
  if (found < 0)
  {
    A = 1;
    found = search(true, A);
  }

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
    // Walk the route back to the start (path runs goal to start).
    std::vector<int> path;
    for (int s = found;; s = prev[std::size_t(s)])
    {
      path.push_back(s / A);
      if (prev[std::size_t(s)] == s)
        break;
    }
    std::reverse(path.begin(), path.end());
    if (std::getenv("GR_GUIDE_DEBUG"))
    {
      std::fprintf(stderr, "guide:");
      for (const int c : path)
        std::fprintf(stderr, " %d,%d", c % W, c / W);
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
    // Aim kLookAhead steps along it; a route that starts with a walk (or a
    // climb) aims where that ends (the foot of the ladder, the edge to jump
    // from, the ladder's top), so the arrow says "this way" first and "up"
    // or "jump" once you are there.
    std::size_t k = std::min<std::size_t>(std::size_t(kLookAhead), path.size() - 1);
    auto moveKind = [&](std::size_t j) {
      const int a0 = path[j], a1 = path[j + 1];
      if (a0 / W == a1 / W && (support[std::size_t(a0)] & 1))
        return 1; // a walk
      if (a0 % W == a1 % W && a1 / W < a0 / W && climb[std::size_t(a0)] && climb[std::size_t(a1)])
        return 2; // a climb
      return 0;
    };
    const int first = moveKind(0);
    std::size_t run1 = 0;
    while (first != 0 && run1 + 1 < path.size() && run1 < k && moveKind(run1) == first)
      ++run1;
    if (run1 >= 1 && run1 < k)
      k = run1;
    const int c = path[k];
    const int cx = c % W, cy = c / W;
    // The box there, middle to middle (the runner's own box height).
    toward(float(cx) + float(pb.w) * 0.5f, float(cy - pb.h + 1) + float(pb.h) * 0.5f);
    if (mGuide.dx == 0 && mGuide.dy == 0)
      toward(float(mGuide.tx) + 0.5f, float(mGuide.ty) + 0.5f);
    // A walk into a crawlspace just ahead: down that way (crouch and crawl).
    if (first == 1 && mGuide.dy == 0)
      for (std::size_t j = 1; j <= std::min<std::size_t>(3, run1); ++j)
      {
        const int q = path[j];
        bool stands = true;
        for (int dx = 0; dx < kBoxW && stands; ++dx)
        {
          const int qx = wrapX ? wrapIndex(q % W + dx, W) : q % W + dx;
          stands = qx >= 0 && qx < W && run[std::size_t((q / W) * W + qx)] >= kStandH;
        }
        if (!stands)
        {
          mGuide.dy = 1;
          break;
        }
      }
    return mGuide;
  }

  // Nothing found a way: the nearest target as the crow flies.
  std::size_t best = 0;
  float bestD = 0.0f;
  for (std::size_t t = 0; t < targets.size(); ++t)
  {
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
  const float reach = 96.0f + bob;
  const float ax = cx + ux * reach, ay = cy + uy * reach;
  drawGlow(r, mArt, ax, ay, 46.0f, c, 0.45f * fade);
  DrawOpts o;
  o.tint = c;
  o.alpha = fade;
  o.angle = std::atan2(uy, ux) * 57.2958f;
  r.draw(mArt.guideArrow, ax, ay, o);
  if (g.kind == GuideKind::Exit)
    r.drawText("EXIT", ax, ay + (uy > 0.5f ? 28.0f : -52.0f), {16.0f, c, rgb(10, 8, 20), true}, Align::Center, fade);

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
