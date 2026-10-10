// The bot in Dry Gulch (world_west.cpp): it backs off from a barrel about
// to blow and waits out a burning fuse, fights a Duelist or a Tumble Mine
// that is close by trying a few short input runs in copies of the world
// (fire, jump the shot, crouch, back off), and plays High Noon's duels:
// stand on the mark, fire on the second ring, jump a shot.

#include "frontend/bot.hpp"

#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace gr
{

namespace
{

PlayerInput westInput(const Input& in, const Input& prev)
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

// Frames until barrel i blows: its hiss, or the spark nearest it along a
// burning fuse (-1: nothing on its way).
int framesToBlow(const WestState& ws, int i)
{
  const Barrel& b = ws.barrels[std::size_t(i)];
  if (b.blown)
    return -1;
  if (b.t >= 0)
    return b.t;
  int best = -1;
  for (const auto& f : ws.fuses)
  {
    if (f.barrel != i)
      continue;
    for (std::size_t k = 0; k < f.cells.size(); ++k)
      if (f.cells[k].state == 1)
      {
        const int t = int(f.cells.size() - 1 - k) * f.framesPerBlock + f.cells[k].t + b.flash;
        if (best < 0 || t < best)
          best = t;
      }
  }
  return best;
}

// A threat close by: a live Duelist calling the runner out, or a Tumble Mine
// rolling at them on their floor.
int nearThreat(const World& w)
{
  const auto& p = w.player();
  int best = -1, bestD = 0;
  for (std::size_t i = 0; i < w.enemies().size(); ++i)
  {
    const Enemy& e = w.enemies()[i];
    if (!e.alive || !e.active)
      continue;
    const int d = std::abs((e.x + e.w / 2) - (p.x + 1));
    bool threat = false;
    if (e.kind == EnemyKind::Duelist)
      threat = d <= kDuelRange * kCellsPerTile && std::abs(e.y - p.y) <= 3;
    if (e.kind == EnemyKind::TumbleMine)
      threat = d <= 26 && std::abs(e.y - p.y) <= 4 && ((e.x > p.x) == (e.dir < 0) || d <= 8);
    if (threat && (best < 0 || d < bestD))
    {
      best = int(i);
      bestD = d;
    }
  }
  return best;
}

} // namespace

bool Bot::west(const World& world, Input& in)
{
  const auto& ws = world.west();
  const auto& p = world.player();
  if (p.state == PlayerState::Dying || p.state == PlayerState::Teleporting)
    return false;
  if (!mWestQueue.empty())
  {
    in = mWestQueue.front();
    mWestQueue.pop_front();
    mWestPrev = in;
    return true;
  }
  // A barrel about to blow: get out of its radius (and stay out).
  const float px = float(p.x) + 1.5f;
  for (std::size_t i = 0; i < ws.barrels.size(); ++i)
  {
    const int t = framesToBlow(ws, int(i));
    if (t < 0 || t > 120)
      continue;
    const Barrel& b = ws.barrels[i];
    const float bx = float(b.x * kCellsPerTile + 1);
    const float reach = float((b.radius + 2) * kCellsPerTile + 2);
    if (std::abs(px - bx) <= reach && std::abs(float(p.y - (b.y * kCellsPerTile + 1))) <= float((b.radius + 3) * kCellsPerTile))
    {
      in.left = px < bx;
      in.right = px >= bx;
      mWestPrev = in;
      return true;
    }
  }
  // Anything burning: wait for it (what it blows up is usually the way on).
  if (ws.anyBurning())
  {
    in = Input{};
    mWestPrev = in;
    return true;
  }
  // A Duelist or a Tumble Mine close by: try short runs of inputs in
  // copies of the world and take the one that hurts it most for the
  // fewest hearts.
  const int threat = nearThreat(world);
  if (threat < 0)
    return false;
  const Enemy& te = world.enemies()[std::size_t(threat)];
  const int face = te.x + te.w / 2 > p.x + 1 ? 1 : -1;
  constexpr int kRun = 16;
  // Candidates: 0 stand and fire, 1 jump now and fire, 2 crouch and fire,
  // 3 back off, 4 stand still, 5 jump straight up.
  long bestScore = std::numeric_limits<long>::min();
  int best = 0;
  for (int c = 0; c < 6; ++c)
  {
    const auto simPtr = world.cloneForSim();
    World& sim = *simPtr;
    Input prev = mWestPrev;
    const int hp0 = sim.player().hp;
    const int ehp0 = te.hp;
    for (int f = 0; f < kRun; ++f)
    {
      Input i;
      const bool facing = sim.player().facing == face;
      switch (c)
      {
        case 0:
        case 1:
        case 2:
          if (!facing && f == 0)
          {
            i.left = face < 0;
            i.right = face > 0;
          }
          else
            i.fire = f % 2 == 1;
          i.jump = c == 1 && f < 6;
          i.down = c == 2;
          break;
        case 3:
          i.left = face > 0;
          i.right = face < 0;
          break;
        case 4:
          break;
        case 5:
          i.jump = f < 6;
          break;
      }
      sim.update(westInput(i, prev));
      prev = i;
      if (sim.player().state == PlayerState::Dying)
        break;
    }
    const Enemy& se = sim.enemies()[std::size_t(threat)];
    const int ehp = se.alive ? se.hp : 0;
    long score = 100L * (ehp0 - ehp) - 1000L * std::max(0, hp0 - sim.player().hp);
    if (sim.player().state == PlayerState::Dying)
      score -= 100000L;
    if (c == 3)
      score -= 30; // backing off loses ground
    if (score > bestScore)
    {
      bestScore = score;
      best = c;
    }
  }
  // Play the first four frames of the winner, then look again.
  Input prev = mWestPrev;
  for (int f = 0; f < 4; ++f)
  {
    Input i;
    const bool facing = p.facing == face;
    switch (best)
    {
      case 0:
      case 1:
      case 2:
        if (!facing && f == 0)
        {
          i.left = face < 0;
          i.right = face > 0;
        }
        else
          i.fire = f % 2 == 1 && !prev.fire;
        i.jump = best == 1;
        i.down = best == 2;
        break;
      case 3:
        i.left = face > 0;
        i.right = face < 0;
        break;
      case 5:
        i.jump = true;
        break;
      default:
        break;
    }
    mWestQueue.push_back(i);
    prev = i;
  }
  in = mWestQueue.front();
  mWestQueue.pop_front();
  mWestPrev = in;
  return true;
}

Input Bot::highNoon(const World& world)
{
  const auto& n = world.west().noon;
  const auto& p = world.player();
  Input in;
  // Back to the mark, facing the duelist.
  if (p.x < n.runnerX - 1)
    in.right = true;
  else if (p.x > n.runnerX + 1)
    in.left = true;
  else if (p.facing < 0)
    in.right = true;
  // After the second ring: fire (one tap), and jump a shot coming.
  const bool live = n.phase == DuelPhase::Draw || n.phase == DuelPhase::Fired;
  bool ours = false;
  for (const auto& pr : world.projectiles())
    ours = ours || (pr.alive && pr.kind != ShotKind::Enemy);
  if (live && !ours && !mWestPrev.fire && p.facing > 0)
    in.fire = true;
  for (const auto& pr : world.projectiles())
    if (pr.alive && pr.kind == ShotKind::Enemy && pr.x > p.x && pr.x - (p.x + 3) <= 10)
      in.jump = p.state == PlayerState::OnGround || mWestPrev.jump;
  mWestPrev = in;
  return in;
}

} // namespace gr
