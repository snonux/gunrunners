// The guide arrow's route search (see guide.hpp and world_guide.cpp).
//
// The search plays the runner's own moves a logic frame at a time, as
// player.cpp makes them: walking (turning round takes a frame), crawling,
// ladders and pipes, this runner's jump arc (with its hang at the top,
// ceilings and steering in the air), then falling. In the air the runner
// steers a cell a frame (two with Turbo; not on a jump's first frame,
// unless it leaves a ladder or a pipe); walking and climbing are a cell a
// frame. Walking costs 2 a frame, the air 2 a frame and a jump 3 more to
// start, so a route walks where it can. What that can't reach (a lift, a
// pulley, a teleport, a door still shut) is floated to through open space,
// dear (kFloat a step) so a route floats as little as it can.
//
// The search runs every logic frame the runner moves, so it must cost the
// states it visits, not the whole map (working every grid out from the map
// and clearing arrays of cells times states for each of up to six searches
// cost up to 118 ms a frame on the bigger levels). So the grids that only
// the map decides are kept while it stays the same, the tests a step makes
// (the standing box, ladders, pipes) are read from them, and a state counts
// as reached only when stamped by this search.
//
// The access card (GuideTarget::key) is not routed to: markGoals skips its
// targets, so no search ever ends at it, and with a force field up and no
// card the arrow only routes to what is on this side of the field (or
// points as the crow flies, which skips the card too). A known open issue,
// kept as it was when the search was moved here.

#include "game/guide.hpp"

#include <algorithm>

namespace gr
{

namespace
{

constexpr int kBoxW = GuideGrid::kBoxW;
constexpr int kBoxH = GuideGrid::kBoxH;
constexpr int kStandH = GuideGrid::kStandH;
constexpr int kShootGap = 2; // cells between the runner and what it shoots (the gun sticks out)
constexpr int kFloat = 20;   // a step floating: dear, so a route floats as little as it can

} // namespace

// ---- GuideCacheSlot ----

GuideCacheSlot::GuideCacheSlot() = default;
GuideCacheSlot::~GuideCacheSlot() = default;
GuideCacheSlot::GuideCacheSlot(const GuideCacheSlot&) {}
GuideCacheSlot& GuideCacheSlot::operator=(const GuideCacheSlot& o)
{
  if (this != &o)
    mCache.reset();
  return *this;
}
GuideCacheSlot::GuideCacheSlot(GuideCacheSlot&&) noexcept = default;
GuideCacheSlot& GuideCacheSlot::operator=(GuideCacheSlot&&) noexcept = default;

GuideCache& GuideCacheSlot::get()
{
  if (!mCache)
    mCache = std::make_unique<GuideCache>();
  return *mCache;
}

// ---- GuideGrid ----

bool GuideGrid::update(const CollisionMap& map, const Statics& statics, const std::vector<std::array<int, 3>>& decks, bool fresh)
{
  const bool rebuild = fresh || !mBuilt || !(statics == mStatics);
  if (rebuild)
    buildStatics(map, statics);
  // Platforms where they are now (the map's lifts and floats, the world's
  // decks): stood on like a one-way floor. They move on; the route is
  // searched again as they do.
  mSupport = mBase;
  for (const auto& p : map.platforms())
    standOn(p);
  for (const auto& f : map.floats())
    standOn(f);
  for (const auto& d : decks)
  {
    const int y = d[1] - 1;
    if (y < 0 || y >= mH)
      continue;
    for (int x = d[0] - kBoxW + 1; x <= d[0] + d[2] - 1; ++x)
    {
      const int gx = mWrapX ? wrapIndex(x, mW) : x;
      if (gx >= 0 && gx < mW && mFit[std::size_t(y * mW + gx)])
        mSupport[std::size_t(y * mW + gx)] = 1;
    }
  }
  return rebuild;
}

// The box stands on the top row of `top` wherever one of its columns is
// over it (CollisionMap::solidTop counts the map's platforms and floats).
void GuideGrid::standOn(const CellBox& top)
{
  const int y = top.top() - 1;
  if (y < 0 || y >= mH)
    return;
  for (int cx = std::max(0, top.left()); cx <= std::min(mW - 1, top.right()); ++cx)
    for (int dx = 0; dx < kBoxW; ++dx)
    {
      const int x = mWrapX ? wrapIndex(cx - dx, mW) : cx - dx;
      if (x >= 0 && x < mW && mFit[std::size_t(y * mW + x)])
        mSupport[std::size_t(y * mW + x)] = 1;
    }
}

void GuideGrid::buildStatics(const CollisionMap& map, const Statics& statics)
{
  mW = map.width();
  mH = map.height();
  mWrapX = map.wraps() || map.wrapsX();
  mWrapY = map.wraps();
  mStatics = statics;
  mBuilt = true;
  buildBlocked(map, statics);
  buildFit(map);
}

// Cells in the way: walls (but force fields with the card) and spikes;
// not breakables any shot opens; lava pools that burn and throw you back
// (level 12) are as bad as a wall.
void GuideGrid::buildBlocked(const CollisionMap& map, const Statics& statics)
{
  const std::size_t N = std::size_t(mW) * std::size_t(mH);
  mBlocked.assign(N, 0);
  mLadder.assign(N, 0);
  mPipe.assign(N, 0);
  for (int y = 0; y < mH; ++y)
    for (int x = 0; x < mW; ++x)
    {
      const std::size_t i = std::size_t(y * mW + x);
      mBlocked[i] = (map.solid(x, y) && !(statics.fieldsDown && map.forceField(x, y))) || map.hazard(x, y);
      mLadder[i] = map.ladder(x, y);
      mPipe[i] = map.climbable(x, y);
    }
  for (const auto& b : statics.open)
    for (int y = b[1] * kCellsPerTile; y < (b[3] + 1) * kCellsPerTile && y < mH; ++y)
      for (int x = b[0] * kCellsPerTile; x < (b[2] + 1) * kCellsPerTile && x < mW; ++x)
        if (x >= 0 && y >= 0 && !map.hazard(x, y))
          mBlocked[std::size_t(y * mW + x)] = 0;
  for (const auto& l : statics.lava)
    for (int y = std::max(0, l[1]); y <= std::min(mH - 1, l[3]); ++y)
      for (int x = std::max(0, l[0]); x <= std::min(mW - 1, l[2]); ++x)
        mBlocked[std::size_t(y * mW + x)] = 1;
}

// run: clear cells in a column up to and including this one (at most
// kStandH). fit and tall: the crouched and the standing box with its
// bottom-left here are clear. mBase: that box stands on the tiles.
void GuideGrid::buildFit(const CollisionMap& map)
{
  const std::size_t N = std::size_t(mW) * std::size_t(mH);
  mRun.assign(N, 0);
  for (int y = 0; y < mH; ++y)
    for (int x = 0; x < mW; ++x)
      if (!mBlocked[std::size_t(y * mW + x)])
        mRun[std::size_t(y * mW + x)] =
          std::uint8_t(std::min(kStandH, (y > 0 ? mRun[std::size_t((y - 1) * mW + x)] : mWrapY ? kStandH - 1 : 0) + 1));
  mFit.assign(N, 0);
  mTall.assign(N, 0);
  mBase.assign(N, 0);
  for (int y = 0; y < mH; ++y)
    for (int x = 0; x < mW; ++x)
    {
      bool fit = true, tall = true;
      for (int dx = 0; dx < kBoxW && fit; ++dx)
      {
        const int cx = mWrapX ? wrapIndex(x + dx, mW) : x + dx;
        fit = cx >= 0 && cx < mW && mRun[std::size_t(y * mW + cx)] >= kBoxH;
        tall = tall && fit && mRun[std::size_t(y * mW + cx)] >= kStandH;
      }
      if (!fit)
        continue;
      const std::size_t i = std::size_t(y * mW + x);
      mFit[i] = 1;
      mTall[i] = tall ? 1 : 0;
      bool stand = false;
      for (int dx = 0; dx < kBoxW && !stand; ++dx)
        stand = map.solidTopTiles(mWrapX ? wrapIndex(x + dx, mW) : x + dx, y + 1);
      mBase[i] = stand ? 1 : 0;
    }
}

bool GuideGrid::ladderBelow(int x, int y) const
{
  // CollisionMap::ladder: rows off the top of the map are empty unless the
  // map wraps top to bottom (columns are in the map already).
  if (x < 0 || x >= mW || y >= mH)
    return false;
  if (y < 0)
    return mWrapY && mLadder[std::size_t(wrapIndex(y, mH) * mW + x)] != 0;
  return mLadder[std::size_t(y * mW + x)] != 0;
}

// ---- GuideRouter: the search ----

GuideRoute GuideRouter::route(const GuideGrid& grid, const std::vector<GuideTarget>& targets, const Start& start,
  const Moves& moves, bool freeOnly)
{
  // The grid and the targets are only borrowed for this call (the targets
  // are a vector local to World::guide()): forgotten again before it
  // returns, so the router kept in the cache never points at them.
  mGrid = &grid;
  mTargets = &targets;
  mMoves = moves;
  GuideRoute r = routeFrom(start, freeOnly);
  mGrid = nullptr;
  mTargets = nullptr;
  return r;
}

// route() with the grid and the targets in hand.
GuideRoute GuideRouter::routeFrom(const Start& start, bool freeOnly)
{
  markGoals(*mTargets);
  GuideRoute r;
  int found = -1;
  if (!freeOnly)
  {
    const int s0 = start.cell < 0 ? -1 : start.cell * kStates + start.phase * 2 + (start.facing > 0 ? 1 : 0);
    found = searchArcs(s0, false);
    r.walkable = found >= 0;
    if (found < 0)
      found = searchArcs(s0, true); // floating where it must
    r.perCell = kStates;
  }
  if (found < 0)
  {
    found = searchFree(start.cell);
    r.perCell = 1;
  }
  if (found >= 0)
    walkBack(found, r.perCell, r);
  return r;
}

// Goal cells: where the box touches a target (a bot or a crate: close
// enough to shoot it, beside it rather than in it, so the shot hits).
// The first target over a cell has it. The access card's targets get no
// goal cells (a known open issue: the card is never routed to).
void GuideRouter::markGoals(const std::vector<GuideTarget>& targets)
{
  const GuideGrid& g = *mGrid;
  const int W = g.width(), H = g.height();
  mGoal.assign(std::size_t(W) * std::size_t(H), 0);
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
            ladderBelow = g.ladderBelow(g.wrapX() ? GuideGrid::wrapIndex(x + dx, W) : x + dx, y + 1);
          if (ladderBelow)
            continue;
        }
        const int c = g.cellAt(x, y);
        if (c >= 0 && mGoal[std::size_t(c)] == 0)
          mGoal[std::size_t(c)] = int(t) + 1;
      }
  }
}

// In the runner's own moves (with floating where it must when `hybrid`).
int GuideRouter::searchArcs(int s0, bool hybrid)
{
  mHybrid = hybrid;
  return search(kStates, s0, [this](int s, const auto& push) { expandArcs(s, push); });
}

int GuideRouter::searchFree(int s0)
{
  return search(1, s0, [this](int s, const auto& push) { expandFree(s, push); });
}

// A goal state the route may end in: shooting needs the ground under you
// (in the runner's own moves).
bool GuideRouter::arrived(int s, int P) const
{
  const GuideTarget& t = (*mTargets)[std::size_t(mGoal[std::size_t(s / P)] - 1)];
  return P == 1 || !t.shoot || ((s % P) / 2 == 0 && supportAt(s / P));
}

// Reached `ns` for `c` from `from`.
void GuideRouter::visit(int ns, int from, int c)
{
  mSeen[std::size_t(ns)] = mStamp;
  mCost[std::size_t(ns)] = c;
  mPrev[std::size_t(ns)] = from;
  mBuckets[std::size_t(c & 31)].push_back(ns);
  ++mPending;
}

// Cheapest first, a state at a time: P states per cell, expand(s, push)
// offers the next ones. Returns the first goal state reached (markGoals: a
// cell of any target but the access card's), -1 for none; mPrev links each
// state to the one before it.
template <class Expand> int GuideRouter::search(int P, int s0, const Expand& expand)
{
  begin(std::size_t(mGrid->width()) * std::size_t(mGrid->height()) * std::size_t(P));
  if (s0 < 0)
    return -1;
  visit(s0, s0, 0);
  for (int c = 0; mPending > 0; ++c)
  {
    auto& bucket = mBuckets[std::size_t(c & 31)];
    for (std::size_t q = 0; q < bucket.size(); ++q)
    {
      const int s = bucket[q];
      --mPending;
      if (mCost[std::size_t(s)] != c)
        continue; // reached cheaper since
      if (mGoal[std::size_t(s / P)] != 0 && arrived(s, P))
        return s;
      // P states per cell: two facings of each phase (P == 1: just cells).
      expand(s, [&](int cell, int phase, int step, int face = 1) {
        if (cell < 0)
          return;
        const int ns = P == 1 ? cell : cell * P + phase * 2 + (face > 0 ? 1 : 0);
        if (mSeen[std::size_t(ns)] == mStamp && mCost[std::size_t(ns)] <= c + step)
          return;
        visit(ns, s, c + step);
      });
    }
    bucket.clear();
  }
  return -1;
}

// A new search over `states` states: a new stamp, so nothing reached before
// counts, and empty buckets. The stamp is a byte (see mSeen): every 255th
// search it wraps round and mSeen is cleared once, a memset of a byte a
// state, which costs far less than a search.
void GuideRouter::begin(std::size_t states)
{
  if (mSeen.size() < states)
  {
    mSeen.resize(states, 0);
    mCost.resize(states);
    mPrev.resize(states);
  }
  if (++mStamp == 0) // wrapped round: old stamps could pass for new ones
  {
    std::fill(mSeen.begin(), mSeen.end(), 0);
    mStamp = 1;
  }
  for (auto& q : mBuckets)
    q.clear();
  mPending = 0;
}

// The route, start to goal (mPrev runs goal to start).
void GuideRouter::walkBack(int found, int P, GuideRoute& r) const
{
  r.target = mGoal[std::size_t(found / P)] - 1;
  for (int s = found;; s = mPrev[std::size_t(s)])
  {
    r.states.push_back(s);
    if (mPrev[std::size_t(s)] == s)
      break;
  }
  std::reverse(r.states.begin(), r.states.end());
}

// ---- GuideRouter: the moves ----

// Any open space, eight ways, no gravity (flying, swimming, and what the
// runner's own moves can't reach).
template <class Push> void GuideRouter::expandFree(int s, const Push& push) const
{
  const int W = mGrid->width();
  const int x = s % W, y = s / W;
  for (int dy = -1; dy <= 1; ++dy)
    for (int dx = -1; dx <= 1; ++dx)
    {
      if ((dx == 0 && dy == 0) || !fitAt(x + dx, y + dy))
        continue;
      if (dx != 0 && dy != 0 && !fitAt(x + dx, y) && !fitAt(x, y + dy))
        continue; // no squeezing between two corners
      push(mGrid->cellAt(x + dx, y + dy), 0, dx != 0 && dy != 0 ? 3 : 2);
    }
}

// The runner's own moves from state s, a logic frame a step.
template <class Push> void GuideRouter::expandArcs(int s, const Push& push) const
{
  const int W = mGrid->width();
  const int i = s / kStates, phase = (s % kStates) / 2, face = (s & 1) ? 1 : -1;
  const int x = i % W, y = i / W;
  if (mHybrid)
  {
    expandFloating(i, x, y, phase, face, push);
    if (phase == kFloating)
      return;
  }
  if (phase == 0)
    expandStanding(i, x, y, face, push);
  else if (phase == kOnLadder)
    expandLadder(i, x, y, face, push);
  else if (phase == kOnPipe)
    expandPipe(i, x, y, face, push);
  else
    expandAir(x, y, phase, face, push);
}

// Floating: where only something the runner can't do by themselves gets
// them (a lift, a pulley, a vehicle, a switch): a step through open space,
// dear (kFloat). Only when the runner's own moves reach nothing.
template <class Push> void GuideRouter::expandFloating(int i, int x, int y, int phase, int face, const Push& push) const
{
  expandFree(i, [&](int c, int, int step, int = 1) { push(c, kFloating, kFloat + step - 2, face); });
  if (phase != kFloating)
    return;
  if (supportAt(i))
    push(i, 0, 1, face); // let go
  else
    grabLadder(x, y, 1, face, push);
  push(i, kFall, 1, face);
}

template <class Push> void GuideRouter::expandStanding(int i, int x, int y, int face, const Push& push) const
{
  if (!supportAt(i))
  {
    push(i, kFall, 1, face); // nothing under you: falling
    return;
  }
  const GuideGrid& g = *mGrid;
  for (int dx = -1; dx <= 1; dx += 2)
  {
    if (dx != face)
      push(i, 0, 1, dx); // turning round takes a frame
    else if (fitAt(x + dx, y))
    {
      const int c = g.cellAt(x + dx, y);
      if (supportAt(c))
        push(c, 0, 2, face); // a step
      else if (tallAt(x + dx, y))
        fallBy(x + dx, y, 1, kFall, face, push); // off the edge: down a cell at once
    }
    else if (fitAt(x + dx, y - 1) && supportAt(g.cellAt(x + dx, y - 1)))
      push(g.cellAt(x + dx, y - 1), 0, 2, face); // a stair
  }
  if (tallAt(x, y))
    grabLadder(x, y, 2, face, push); // (not from a crawl: up does nothing there)
  // Down onto a ladder below the feet (room for it beneath).
  for (int dx = 0; dx < kBoxW; ++dx)
    if (ladderAt(x + dx, y + 1))
    {
      const int sx = x + dx - 1;
      bool clear = tallAt(sx, y);
      for (int k = 0; k < kBoxW && clear; ++k)
        clear = !wallAt(sx + k, y + 1);
      if (clear)
        push(g.cellAt(sx, y), kOnLadder, 2, face);
      break;
    }
  // A jump, with headroom for it (a jump costs 3 more than its frames).
  if (tallAt(x, y) && tallAt(x, y - 1))
    jumpFrame(x, y, 0, 0, face, [&](int c, int ph, int step, int f) { push(c, ph, step + 3, f); });
}

template <class Push> void GuideRouter::expandLadder(int i, int x, int y, int face, const Push& push) const
{
  const GuideGrid& g = *mGrid;
  if (ladderAt(x + 1, y - kStandH) && tallAt(x, y - 1))
    push(g.cellAt(x, y - 1), kOnLadder, 2, face); // up
  if (ladderAt(x + 1, y + 1))
  {
    bool clear = true;
    for (int k = 0; k < kBoxW && clear; ++k)
      clear = !wallAt(x + k, y + 1);
    if (clear)
      push(g.cellAt(x, y + 1), kOnLadder, 2, face); // down
  }
  else if (supportAt(i))
    push(i, 0, 1, face); // down off the foot of it
  else
    fallBy(x, y, 1, kFall, face, push); // down off the bottom: falling
  // Jumping off it, steering on the first frame (the other way: turning
  // round on it).
  if (tallAt(x, y - 1))
    for (int dx = -1; dx <= 1; ++dx)
      jumpFrame(x, y, dx, 0, face, [&](int c, int ph, int step, int f) { push(c, ph, step + 3, f); });
}

template <class Push> void GuideRouter::expandPipe(int i, int x, int y, int face, const Push& push) const
{
  const GuideGrid& g = *mGrid;
  // Hand over hand while the pipe goes on.
  for (int dx = -1; dx <= 1; dx += 2)
    if (dx != face)
      push(i, kOnPipe, 1, dx);
    else if (tallAt(x + dx, y))
      push(g.cellAt(x + dx, y), pipeAt(dx < 0 ? x : x + kBoxW - 1, y - kStandH) ? kOnPipe : kFall, 2, face);
  push(i, kFall, 2, face); // let go
  // Up and jump: a jump off the pipe.
  if (tallAt(x, y - 1) && tallAt(x, y - 2))
    for (int dx = -1; dx <= 1; ++dx)
      jumpFrame(x, y - 1, dx, 0, face, [&](int c, int ph, int step, int f) { push(c, ph, step + 3, f); });
}

// Jumping (phases 1..8) or falling (kFall..kFall + 2), steering either way.
template <class Push> void GuideRouter::expandAir(int x, int y, int phase, int face, const Push& push) const
{
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
}

// Up onto a ladder (any column of the box's top row on it): the runner's
// middle snaps onto it.
template <class Push> void GuideRouter::grabLadder(int x, int y, int step, int face, const Push& push) const
{
  for (int dx = 0; dx < kBoxW; ++dx)
    if (ladderAt(x + dx, y - kStandH + 1) && tallAt(x + dx - 1, y))
    {
      push(mGrid->cellAt(x + dx - 1, y), kOnLadder, step, face);
      return;
    }
}

// Steering against the way the runner faces only turns them round.
void GuideRouter::steerBy(int& x, int y, int dx, int& face) const
{
  if (dx == 0)
    return;
  if (dx != face)
  {
    face = dx;
    return;
  }
  for (int k = 0; k < mMoves.steer && tallAt(x + dx, y); ++k)
    x += dx;
}

// Falling `down` cells from (x, y) this frame: lands, catches a pipe, or
// falls on as `next`.
template <class Push> void GuideRouter::fallBy(int x, int y, int down, int next, int face, const Push& push) const
{
  for (int k = 0; k < down; ++k)
  {
    const int c = mGrid->cellAt(x, y);
    if (c < 0)
      return;
    if (supportAt(c))
    {
      push(c, 0, 2, face); // landed
      return;
    }
    if (pipeAt(x + 1, y - kStandH + 1) && tallAt(x, y + 1))
    {
      push(mGrid->cellAt(x, y + 1), kOnPipe, 2, face); // caught a pipe
      return;
    }
    if (!tallAt(x, y + 1))
      return; // out of the bottom of the map
    ++y;
  }
  push(mGrid->cellAt(x, y), next, 2, face);
}

// One frame of a jump from (x, y), arc frame f (steered dx first). Pipes
// overhead are caught on the way up (not on the first frame).
template <class Push> void GuideRouter::jumpFrame(int x, int y, int dx, int f, int face, const Push& push) const
{
  steerBy(x, y, dx, face);
  if (f >= 8)
  {
    fallBy(x, y, 1, kFall, face, push); // the arc is over: falling, a cell now
    return;
  }
  const int up = mMoves.arc[std::size_t(f)];
  if (f > 0 && pipeAt(x + 1, y - kStandH))
  {
    push(mGrid->cellAt(x, y), kOnPipe, 2, face);
    return;
  }
  int moved = 0;
  while (moved < up && tallAt(x, y - 1))
  {
    --y;
    ++moved;
    if (f > 0 && pipeAt(x + 1, y - kStandH))
    {
      push(mGrid->cellAt(x, y), kOnPipe, 2, face);
      return;
    }
  }
  if (moved == up)
    push(mGrid->cellAt(x, y), f + 1, 2, face);
  else if (up == 2 && moved == 1)
    push(mGrid->cellAt(x, y), 4, 2, face); // half a rise under a ceiling: the arc skips on
  else
    fallBy(x, y, 1, kFall, face, push); // a bump on the head: falling
}

} // namespace gr
