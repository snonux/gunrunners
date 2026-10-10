// The bot in Level 26, Kaiju Tonight: towers, the station roof, power
// lines, Gorgazilla, and the Top-Down Rampage bonus.
//
// The planner walks the street (standing buildings are open to its field,
// so it jumps into them); these rules topple a tower in the way and keep
// out from under it while it falls, stand on the station roof until it
// gives way when the bonus is wanted, and jump to snap a live power line
// overhead in a sector whose Maser Truck is still about. Gorgazilla is
// fought like ZERO: short moves tried in copies of the world, followed by
// a few simple policies, scored on his hearts against the runner's. In
// the bonus each move is tried the same way, scored on the damage rating.

#include "frontend/bot.hpp"
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

PlayerInput simInput(const Input& in, const Input& prev)
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

CellBox cellsOf(int x0, int y0, int x1, int y1)
{
  return {x0 * kCellsPerTile, y0 * kCellsPerTile, (x1 - x0 + 1) * kCellsPerTile, (y1 - y0 + 1) * kCellsPerTile};
}

// The tower a runner must topple to go on east, or -1.
int towerAhead(const World& w)
{
  const auto& p = w.player();
  const auto& bs = w.city().buildings;
  for (std::size_t i = 0; i < bs.size(); ++i)
  {
    const auto& b = bs[i];
    if (b.kind != BuildingKind::Tower || b.state != BuildingState::Standing)
      continue;
    const int face = b.x0 * kCellsPerTile;
    if (p.x + Player::kWidth <= face && face - (p.x + Player::kWidth) <= 3 && p.y >= b.y0 * kCellsPerTile &&
        p.y <= (b.y1 + 1) * kCellsPerTile)
      return int(i);
  }
  return -1;
}

// While a tower creaks over or falls: false if none is.
bool towerFalling(const World& w, Input& in)
{
  const auto& p = w.player();
  for (const auto& b : w.city().buildings)
  {
    if (b.state != BuildingState::Leaning && b.state != BuildingState::Falling)
      continue;
    const CellBox bridge = cellsOf(b.bx0, b.by0 - 1, b.bx1, b.by1);
    const CellBox near{bridge.x - 2, bridge.y - 6, bridge.w + 4, bridge.h + 12};
    if (!near.intersects(p.box()) && std::abs(p.x - b.x0 * kCellsPerTile) > 24)
      continue;
    // Out from under it, the way away from the tower's foot; else wait.
    if (near.intersects(p.box()))
      (b.fall < 0 ? in.left : in.right) = true;
    return true;
  }
  return false;
}

// Moves of four frames against Gorgazilla; fireAt is the frame the
// trigger goes down (-1: not at all).
struct LizardMove
{
  int dir;
  bool down, jump;
  int fireAt;
};
const LizardMove kLizardMoves[] = {
  {0, false, false, -1}, // wait
  {-1, false, false, -1},
  {1, false, false, -1},
  {0, false, true, -1},
  {-1, false, true, -1},
  {1, false, true, -1},
  {0, false, false, 0},  // shoot ahead
  {0, true, false, 0},   // crouch and shoot
  {0, true, false, 2},
  {-1, false, false, 0}, // turn left and shoot
  {1, false, false, 0},
  {-1, true, false, 1},  // turn and crouch-shoot
  {1, true, false, 1},
  {-1, false, true, 3},  // jump and shoot
  {1, false, true, 3},
  {0, true, false, 9},   // crouch and hold the trigger (the Atomic Breath sweeps down)
  {0, false, false, 9},  // hold the trigger
  {-1, true, false, 9},
  {1, true, false, 9},
};
constexpr int kLizardMoveCount = int(sizeof(kLizardMoves) / sizeof(kLizardMoves[0]));

Input lizardInput(const LizardMove& m, int f)
{
  Input in;
  in.left = m.dir < 0;
  in.right = m.dir > 0;
  in.down = m.down;
  in.jump = m.jump;
  in.fire = m.fireAt == 9 || m.fireAt == f || (m.fireAt >= 0 && f == m.fireAt + 2);
  return in;
}

// Where to stand: on the fallen harbor tower, out of his tail's reach.
float lizardStand(const World& w)
{
  const auto& z = w.city().lizard;
  const float lc = z.x + Lizard::kW * 0.5f;
  float lo = float(z.piers[0][0]), hi = float(z.piers[1][1]);
  for (const auto& b : w.city().buildings)
    if (b.kind == BuildingKind::Tower && b.state == BuildingState::Lying && b.bx0 * kCellsPerTile > z.x0 * kCellsPerTile)
    {
      lo = float(b.bx0 * kCellsPerTile);
      hi = float((b.bx1 + 1) * kCellsPerTile - Player::kWidth);
    }
  const float px = float(w.player().x) + 1.5f;
  const float west = lc - 14.0f, east = lc + 14.0f;
  float best = std::clamp(px < lc ? west : east, lo, hi);
  if (std::abs(best - lc) < 10.0f)
    best = std::clamp(px < lc ? east : west, lo, hi);
  return best;
}

// Something of his about to land on the runner.
bool lizardDanger(const World& w)
{
  const auto& z = w.city().lizard;
  const auto& p = w.player();
  if (z.state == LizardState::Roar && z.t >= kRoarLock && z.beamRow >= p.y - 5 && z.beamRow <= p.y)
    return true;
  if (z.state == LizardState::Swipe && z.t >= kSwipeTell - 4 && std::abs((z.x + 2.0f) - (float(p.x) + 1.5f)) < 14.0f)
    return true;
  if (z.state == LizardState::Under && z.t >= kUnder - 6 && std::abs(float(z.splashX) - float(p.x)) < 10.0f)
    return true;
  for (const auto& pr : w.projectiles())
    if (pr.alive && pr.kind == ShotKind::Enemy &&
        pr.box().intersects({p.x - 3, p.y - 8, Player::kWidth + 6, 11}))
      return true;
  return false;
}

// Toward the nearest standing building (top-down); returns how far it is.
float towardBuilding(const World& w, Input& in)
{
  const auto& p = w.player();
  const float px = float(p.x) + 1.5f, py = float(p.y) - 2.0f;
  float best = 1e9f, bx = px, by = py;
  for (const auto& b : w.city().buildings)
    if (b.standing())
    {
      const float cx = (float(b.x0 + b.x1 + 1) * 0.5f) * kCellsPerTile, cy = (float(b.y0 + b.y1 + 1) * 0.5f) * kCellsPerTile;
      const float d = std::abs(cx - px) + std::abs(cy - py);
      if (d < best)
      {
        best = d;
        bx = cx;
        by = cy;
      }
    }
  in.left = bx < px - 1.0f;
  in.right = bx > px + 1.0f;
  in.up = by < py - 1.0f;
  in.down = by > py + 1.0f;
  return best;
}

} // namespace

bool Bot::city(const World& world, Input& in)
{
  const auto& c = world.city();
  const auto& p = world.player();
  if (p.state == PlayerState::Dying)
    return false;
  if (towerFalling(world, in))
    return true;
  // A tower in the way: jump into it.
  if (p.state == PlayerState::OnGround && towerAhead(world) >= 0)
  {
    in.right = true;
    in.jump = true;
    return true;
  }
  // The station roof: stand on it until it gives way (the subway, the bonus).
  if (mPlanner.wantsBonus() && p.state == PlayerState::OnGround)
    for (const auto& b : c.buildings)
      if (b.kind == BuildingKind::Roof && b.standing() &&
          cellsOf(b.x0, b.y0 - 1, b.x1, b.y0 - 1).intersects({p.x, p.y, Player::kWidth, 1}))
        return true; // stand still
  // A live power line overhead in a sector with a truck still about: snap it.
  if (p.state == PlayerState::OnGround)
    for (const auto& l : c.lines)
    {
      if (l.snapped || p.x + 1 < l.x0 || p.x + 1 > l.x1 || p.y - l.y > 10 || p.y < l.y)
        continue;
      bool truck = false;
      for (const auto& e : world.enemies())
        truck = truck || (e.alive && e.kind == EnemyKind::MaserTruck && e.variant >= 0 &&
                           std::size_t(e.variant) < c.sectors.size() && c.sectors[std::size_t(e.variant)].id == l.sector);
      if (truck)
      {
        in.jump = true;
        return true;
      }
    }
  return false;
}

Input Bot::fightLizard(const World& world)
{
  const auto& p = world.player();
  // The harbor tower first: it is the way across and the only line of fire.
  {
    Input in;
    if (towerFalling(world, in))
    {
      mFightQueue.clear();
      mFightPrev = in;
      return in;
    }
    for (const auto& b : world.city().buildings)
      if (b.kind == BuildingKind::Tower && b.standing() && b.x0 * kCellsPerTile > p.x &&
          b.x0 >= world.city().lizard.x0)
      {
        mFightQueue.clear();
        const int face = b.x0 * kCellsPerTile;
        if (face - (p.x + Player::kWidth) > 2)
          in.right = true;
        else if (p.state == PlayerState::OnGround)
        {
          in.right = true;
          in.jump = !mFightPrev.jump;
        }
        in.jump = in.jump || (p.state == PlayerState::OnGround && lizardDanger(world) && !mFightPrev.jump);
        mFightPrev = in;
        return in;
      }
  }
  if (mFightQueue.empty())
  {
    constexpr int kAhead = 40;
    const int hp0 = p.hp, liz0 = world.lizardHp();
    int best = 0;
    long bestScore = std::numeric_limits<long>::min();
    for (int m = 0; m < kLizardMoveCount; ++m)
    {
      const auto firstPtr = world.cloneForSim();
      World& first = *firstPtr;
      Input prev = mFightPrev;
      bool dead = false;
      for (int f = 0; f < 4 && !dead; ++f)
      {
        const Input in = lizardInput(kLizardMoves[m], f);
        first.update(simInput(in, prev));
        prev = in;
        dead = first.player().state == PlayerState::Dying;
      }
      if (dead)
        continue;
      const long firstScore = 200L * (liz0 - first.lizardHp());
      long moveScore = std::numeric_limits<long>::min();
      for (int follow = 0; follow < 5; ++follow)
      {
        World sim(first);
        Input fp = prev;
        long score = firstScore;
        int liz = sim.lizardHp();
        for (int f = 0; f < kAhead; ++f)
        {
          Input in;
          const auto& sp = sim.player();
          const auto& z = sim.city().lizard;
          const float pcx = float(sp.x) + 1.5f, lc = z.x + Lizard::kW * 0.5f;
          const bool ground = sp.state == PlayerState::OnGround;
          const int face = lc < pcx ? -1 : 1;
          const float to = lizardStand(sim);
          const bool there = std::abs(to - pcx) <= 1.0f;
          // The Atomic Breath is held (and swept down crouching); the blaster tapped.
          const bool breath = sp.weapon == Weapon::Proto && sp.proto == int(ProtoId::AtomicBreath) && sp.ammo > 0;
          const bool trigger = breath || f % 2 == 1;
          switch (follow)
          {
            case 0: // to the spot, crouch-shoot at him, jump what comes
            case 1: // the same, standing shots
              if (!there)
              {
                in.left = to < pcx;
                in.right = to > pcx;
              }
              else if (sp.facing != face)
                (face < 0 ? in.left : in.right) = true;
              else
              {
                in.down = follow == 0;
                in.fire = trigger;
              }
              in.jump = ground && lizardDanger(sim) && !fp.jump;
              break;
            case 2: // stay put, shooting
              if (sp.facing != face)
                (face < 0 ? in.left : in.right) = true;
              in.down = true;
              in.fire = trigger;
              in.jump = ground && lizardDanger(sim) && !fp.jump;
              break;
            case 3: // back off from him
              (face < 0 ? in.right : in.left) = true;
              in.jump = ground && lizardDanger(sim) && !fp.jump;
              break;
            case 4: // wait, jumping what comes
              in.jump = ground && lizardDanger(sim) && !fp.jump;
              break;
          }
          sim.update(simInput(in, fp));
          fp = in;
          score += long(liz - sim.lizardHp()) * (200 - 3 * f);
          liz = sim.lizardHp();
          if (sim.player().state == PlayerState::Dying || sim.state() != WorldState::Playing)
            break;
        }
        const auto& sp = sim.player();
        const long lost = std::max(0, hp0 - sp.hp);
        score -= lost * (500L + 1500L / std::max(1, sp.hp));
        score -= long(std::abs(float(sp.x) + 1.5f - lizardStand(sim)) * 3.0f);
        if (sp.state == PlayerState::Dying)
          score -= 1000000L;
        moveScore = std::max(moveScore, score);
      }
      if (moveScore > bestScore)
      {
        bestScore = moveScore;
        best = m;
      }
    }
    static const bool debug = std::getenv("GR_FIGHT_DEBUG") != nullptr;
    if (debug)
      std::fprintf(stderr, "lizard f%d state %d t %d cool %d phase %d hp %d at %.1f; runner %d,%d hp %d -> move %d (%ld)\n",
        world.stats().frames, int(world.city().lizard.state), world.city().lizard.t, world.city().lizard.cool,
        world.city().lizard.phase, world.lizardHp(),
        world.city().lizard.x, p.x, p.y, p.hp, best, bestScore);
    for (int f = 0; f < 4; ++f)
      mFightQueue.push_back(lizardInput(kLizardMoves[best], f));
  }
  Input in = mFightQueue.front();
  mFightQueue.pop_front();
  mFightPrev = in;
  return in;
}

Input Bot::topDown(const World& world)
{
  if (!mTopQueue.empty())
  {
    const Input in = mTopQueue.front();
    mTopQueue.pop_front();
    mTopPrev = in;
    return in;
  }
  constexpr int kHold = 4, kAhead = 28;
  const int hp0 = world.player().hp, score0 = world.stats().score;
  long bestScore = std::numeric_limits<long>::min();
  Input bestIn;
  for (int dx = -1; dx <= 1; ++dx)
    for (int dy = -1; dy <= 1; ++dy)
    {
      Input start;
      start.left = dx < 0;
      start.right = dx > 0;
      start.up = dy < 0;
      start.down = dy > 0;
      auto sim = world.cloneForSim();
      Input prev = mTopPrev;
      long score = 0;
      for (int f = 0; f < kAhead && sim->state() == WorldState::Playing; ++f)
      {
        Input in = start;
        if (f >= kHold)
          towardBuilding(*sim, in);
        in.fire = f % 2 == 1;
        sim->update(simInput(in, prev));
        prev = in;
        if (sim->player().state == PlayerState::Dying)
          break;
      }
      score += long(sim->stats().score - score0) * 10;
      score -= long(std::max(0, hp0 - sim->player().hp)) * 30000L;
      if (sim->player().state == PlayerState::Dying)
        score -= 10000000L;
      if (sim->state() != WorldState::Playing)
        score += 1000000L;
      // Nearer the next building is better too.
      Input probe;
      score -= long(towardBuilding(*sim, probe));
      if (score > bestScore)
      {
        bestScore = score;
        bestIn = start;
      }
    }
  for (int f = 0; f < kHold; ++f)
  {
    Input in = bestIn;
    in.fire = f % 2 == 1;
    mTopQueue.push_back(in);
  }
  const Input in = mTopQueue.front();
  mTopQueue.pop_front();
  mTopPrev = in;
  return in;
}

} // namespace gr
