// The bot in Fright Night Manor (world_manor.cpp): at the east edge of the
// reflected gallery's hole it waits for the Portrait Ghosts to come out
// over the hole and pins two of them with the Silver Crossbow where they
// make a bridge (it tries the shot in a copy of the world first); the
// planner then jumps across. In Both Sides it steers both runners at once
// by trying short runs of inputs in copies of the world.

#include "frontend/bot.hpp"
#include "frontend/planner.hpp"

#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace gr
{

namespace
{

PlayerInput manorInput(const Input& in, const Input& prev)
{
  PlayerInput p;
  p.left = in.left;
  p.right = in.right;
  p.up = in.up;
  p.down = in.down;
  p.jump.pressed = in.jump;
  p.jump.triggered = in.jump && !prev.jump;
  p.fire.pressed = in.fire;
  p.fire.triggered = in.fire && !prev.fire;
  return p;
}

// The ghost platforms over the hole, by their left block.
std::vector<int> bridgePlatforms(const World& w)
{
  const auto& m = w.manor();
  std::vector<int> xs;
  for (const auto& pl : m.platforms)
    if (pl.bx > m.bridgeLanding && pl.bx + 1 < m.bridgeEdge && pl.by >= m.bridgeRow - 4 && pl.by <= m.bridgeRow - 1)
      xs.push_back(pl.bx);
  std::sort(xs.begin(), xs.end());
  return xs;
}

// Would these platforms (plus one at bx) still make a bridge? Gaps of at
// most 4 blocks from the edge and between them, 5 down to the landing.
bool fits(const World& w, std::vector<int> xs, int bx)
{
  const auto& m = w.manor();
  xs.push_back(bx);
  std::sort(xs.begin(), xs.end());
  if (xs.size() > 2)
    return false;
  if (xs.size() == 1)
  {
    // The first one can be the near end (a jump from the edge) or the far
    // one (a jump down to the landing), with room for the other between.
    const int x = xs[0];
    const bool nearEnd = m.bridgeEdge - (x + 2) >= 0 && m.bridgeEdge - (x + 2) <= 4;
    const bool farEnd = x - (m.bridgeLanding + 1) <= 5 && x + 3 <= m.bridgeEdge - 2;
    return nearEnd || farEnd;
  }
  // The platform nearest the edge, then the next, then the landing.
  const int a = xs.back();
  if (m.bridgeEdge - (a + 2) > 4 || m.bridgeEdge - (a + 2) < 0)
    return false;
  const int b = xs.front();
  const int gap = a - (b + 2);
  // (The landing is lower: a gap of 5 down to it is still a jump.)
  if (gap < 1 || gap > 4 || b - (m.bridgeLanding + 1) > 5)
    return false;
  return true;
}

} // namespace

bool Bot::manor(const World& world, Input& in)
{
  const auto& m = world.manor();
  const auto& p = world.player();
  if (m.flip > 0 || p.state == PlayerState::Dying)
    return false;
  // The crypt detour (the planner walks there): at the broken mirror, shoot
  // it open, then Up; at the crypt's mirror, Up.
  if (mPlanner.wantsBonus() && p.state == PlayerState::OnGround)
  {
    bool inCrypt = false;
    const Planner::Goal g = Planner::cryptGoal(world, inCrypt);
    if (g.kind == 18 && p.x + 1 >= g.x && p.x + 1 < g.x + g.w && p.y == g.y + g.h - 1)
    {
      const auto& mi = m.mirrors[std::size_t(g.index)];
      in = Input{};
      if (mi.kind == MirrorKind::Broken && mi.third == 0)
        in.fire = !mManorPrev.fire && p.shotCooldown <= 1;
      else
        in.up = !mManorPrev.up;
      mManorPrev = in;
      return true;
    }
  }
  if (m.bridgeEdge < 0 || m.side != kSideMirror)
    return false;
  const auto xs = bridgePlatforms(world);
  if (xs.size() >= 2)
    return false; // the bridge is up: the planner crosses
  // Only on the gallery floor east of the hole, near its edge.
  const int feet = (m.bridgeRow * kCellsPerTile) - 1;
  const int edgeX = m.bridgeEdge * kCellsPerTile;
  if (p.y < feet - 1 || p.y > feet || p.x < edgeX - 1 || p.x > edgeX + 10)
    return false;
  in = Input{};
  // To the edge first, then face west.
  if (p.x > edgeX)
  {
    in.left = true;
    mManorPrev = in;
    return true;
  }
  if (p.facing > 0)
  {
    in.left = true;
    mManorPrev = in;
    return true;
  }
  const bool crossbow = p.weapon == Weapon::Proto && p.proto == int(ProtoId::SilverCrossbow) && p.ammo > 0;
  if (!crossbow || mManorPrev.fire || p.shotCooldown > 1)
  {
    mManorPrev = in;
    return true;
  }
  // Fire now? Only if the bolt pins a ghost where the bridge needs it, or
  // a ghost is about to lunge at the runner.
  bool danger = false;
  for (const auto& e : world.enemies())
    if (e.alive && e.kind == EnemyKind::PortraitGhost && e.attach >= 2 && e.stun == 0 &&
        std::abs(e.x + 2 - (p.x + 1)) <= 8 && std::abs(e.y - p.y) <= 6)
      danger = true;
  const auto simPtr = world.cloneForSim();
  World& sim = *simPtr;
  Input fire;
  fire.fire = true;
  sim.update(manorInput(fire, mManorPrev));
  Input prev = fire;
  const std::size_t before = sim.manor().platforms.size();
  bool good = false;
  for (int f = 0; f < 24 && sim.manor().platforms.size() == before; ++f)
  {
    sim.update(manorInput(Input{}, prev));
    prev = Input{};
  }
  if (sim.manor().platforms.size() > before)
  {
    const auto& pl = sim.manor().platforms.back();
    good = pl.by >= m.bridgeRow - 4 && pl.by <= m.bridgeRow - 1 && fits(world, xs, pl.bx);
    if (std::getenv("GR_MANOR_DEBUG"))
      std::fprintf(stderr, "  sim pin at %d,%d\n", pl.bx, pl.by);
  }
  else if (std::getenv("GR_MANOR_DEBUG"))
    std::fprintf(stderr, "  sim no pin\n");
  if (good || danger)
    in.fire = true;
  if (std::getenv("GR_MANOR_DEBUG"))
  {
    std::fprintf(stderr, "bridge f%d at %d,%d ammo %d plats %zu good %d danger %d", world.stats().frames, p.x, p.y,
      p.ammo, xs.size(), int(good), int(danger));
    for (const auto& e : world.enemies())
      if (e.kind == EnemyKind::PortraitGhost)
        std::fprintf(stderr, " | g %d,%d a%d st%d hp%d", e.x, e.y, e.attach, e.stun, e.hp);
    std::fprintf(stderr, "\n");
  }
  mManorPrev = in;
  return true;
}

// Both Sides: both runners on their exits at once. Try short runs of inputs
// and keep the one that brings both closest to their exits (measured on the
// planner-free distance: blocks left and right, plus a climb).
Input Bot::split(const World& world)
{
  if (!mSplitQueue.empty())
  {
    const Input in = mSplitQueue.front();
    mSplitQueue.pop_front();
    mManorPrev = in;
    return in;
  }
  const auto& s = world.manor().split;
  auto dist = [&](const World& w) {
    const auto& a = w.player();
    const auto& b = w.twin();
    const int ta = std::abs(a.x - s.topExitX * kCellsPerTile) + 2 * std::abs(a.y - (s.topExitY * kCellsPerTile + 1));
    const int tb = std::abs(b.x - s.botExitX * kCellsPerTile) + 2 * std::abs(b.y - (s.botExitY * kCellsPerTile + 1));
    return ta + tb;
  };
  // Candidates: hold right / left, with a jump at the start or none, or wait.
  struct Cand
  {
    bool left, right, jump;
    int jumpFrames;
  };
  static const Cand kCands[] = {
    {false, true, false, 0}, {false, true, true, 6}, {true, false, false, 0}, {true, false, true, 6},
    {false, false, true, 6}, {false, false, false, 0}, {false, true, true, 2}, {true, false, true, 2},
  };
  constexpr int kRun = 12;
  long bestScore = std::numeric_limits<long>::max();
  int best = 5;
  for (int c = 0; c < int(sizeof(kCands) / sizeof(kCands[0])); ++c)
  {
    const auto simPtr = world.cloneForSim();
    World& sim = *simPtr;
    Input prev = mManorPrev;
    for (int f = 0; f < kRun && sim.state() == WorldState::Playing; ++f)
    {
      Input i;
      i.left = kCands[c].left;
      i.right = kCands[c].right;
      i.jump = kCands[c].jump && f < kCands[c].jumpFrames;
      sim.update(manorInput(i, prev));
      prev = i;
    }
    // A second step: the best of right / right + jump after it (one ply of
    // look-ahead, so a jump that only pays off later is seen).
    long second = std::numeric_limits<long>::max();
    if (sim.state() == WorldState::Playing)
      for (int c2 = 0; c2 < 4; ++c2)
      {
        const auto s2 = sim.cloneForSim();
        Input pr2 = prev;
        for (int f = 0; f < kRun && s2->state() == WorldState::Playing; ++f)
        {
          Input i;
          i.left = kCands[c2].left;
          i.right = kCands[c2].right;
          i.jump = kCands[c2].jump && f < kCands[c2].jumpFrames;
          s2->update(manorInput(i, pr2));
          pr2 = i;
        }
        second = std::min(second, s2->state() != WorldState::Playing ? -100000L : long(dist(*s2)));
      }
    else
      second = -100000L;
    const long score = (sim.state() != WorldState::Playing ? -200000L : long(dist(sim)) * 2) + second -
      long(sim.stats().gems - world.stats().gems) * 6;
    if (score < bestScore)
    {
      bestScore = score;
      best = c;
    }
  }
  Input prev = mManorPrev;
  for (int f = 0; f < 4; ++f)
  {
    Input i;
    i.left = kCands[best].left;
    i.right = kCands[best].right;
    i.jump = kCands[best].jump && f < kCands[best].jumpFrames;
    mSplitQueue.push_back(i);
    prev = i;
  }
  const Input in = mSplitQueue.front();
  mSplitQueue.pop_front();
  mManorPrev = in;
  return in;
}

} // namespace gr
