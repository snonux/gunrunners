// The bot in Level 25: waiting for the dandelion's seed head, and the
// Dandelion Drift bonus.
//
// At the dandelion the bot mists its base if the Chrono Mister has ammo,
// or else waits by the seed head's foot for the next time-lapse sweep; once
// the head goes to seed the planner rides it up to the exit.
//
// In Dandelion Drift the bot tries a handful of ways to steer (up, down or
// neither for a while, then neither) in a copy of the world every few frames
// and keeps the one that bounces least and gathers the most gems.

#include "frontend/bot.hpp"
#include "game/world.hpp"

#include <cmath>

namespace gr
{

namespace
{

PlayerInput glideInput(const Input& in)
{
  PlayerInput p;
  p.up = in.up;
  p.down = in.down;
  return p;
}

constexpr int kGlideStep = 4;   // frames each choice is held before choosing again
constexpr int kGlideAhead = 40; // frames looked ahead

} // namespace

bool Bot::garden(const World& world, Input& in)
{
  const auto& g = world.garden();
  const auto& p = world.player();
  const bool mister = p.weapon == Weapon::Proto && p.proto == int(ProtoId::ChronoMister) && p.ammo > 0;
  // A Jumping Spider crouched to leap at where the runner stands: move off
  // the spot, away from the spider.
  if (p.state == PlayerState::OnGround)
    for (const auto& e : world.enemies())
      if (e.alive && e.kind == EnemyKind::JumpingSpider && e.attach == 1 && std::abs(p.x - e.ox) < 8 &&
          std::abs(p.y - e.oy) < 6)
      {
        const int away = e.x + e.w / 2 > p.x + 1 ? -1 : 1;
        (away > 0 ? in.right : in.left) = true;
        return true;
      }
  // The ladybug in front of the bonus door: mist her on the way past.
  if (mPlanner.wantsBonus() && g.ladybugX >= 0 && g.ladybug == 0 && mister && p.state == PlayerState::OnGround)
  {
    const CellBox bug{g.ladybugX * kCellsPerTile, g.ladybugY * kCellsPerTile, 2 * kCellsPerTile, 3 * kCellsPerTile};
    const int top = p.y - 4;
    const CellBox right{p.x + Player::kWidth, top, 6, 4}, left{p.x - 6, top, 6, 4};
    if (right.intersects(bug) || left.intersects(bug))
    {
      const int dir = right.intersects(bug) ? 1 : -1;
      if (p.facing != dir)
        (dir > 0 ? in.right : in.left) = true;
      else
        in.fire = true;
      return true;
    }
  }
  // A Soldier Ant column on the march just ahead: wait for it to go back
  // into its anthill (half its period) rather than walk into it.
  if (p.state == PlayerState::OnGround)
    for (const auto& c : g.columns)
    {
      if (c.hostile > 0 || c.tell > 0 || std::abs(p.y - c.y) > 4 || p.x + Player::kWidth >= c.x0 || p.x < c.x0 - 6)
        continue;
      bool out = false;
      for (int a : c.ants)
      {
        const Enemy& e = world.enemies()[std::size_t(a)];
        out = out || (e.alive && !e.hidden);
      }
      if (out)
        return true; // stand still
    }
  if (g.dandelionX < 0 || g.seeded || p.state != PlayerState::OnGround)
    return false;
  const int base = g.dandelionX * kCellsPerTile;
  if (p.x < base - 16 || p.x > base + 8)
    return false;
  // Mist the base from a few cells to its left, facing it; without ammo,
  // wait beside it.
  const int standX = base - 8;
  if (p.x < standX - 1)
    in.right = true;
  else if (p.x > standX + 1)
    in.left = true;
  else if (p.facing < 0)
    in.right = true;
  else if (mister && g.seeding == 0)
    in.fire = true;
  return true;
}

Input Bot::glide(const World& world)
{
  if (!mGlideQueue.empty())
  {
    const Input in = mGlideQueue.front();
    mGlideQueue.pop_front();
    return in;
  }
  const int gems0 = world.stats().gems;
  double bestScore = -1e18;
  Input bestStart;
  int bestHold = kGlideStep;
  for (int dir = -1; dir <= 1; ++dir)
    for (int hold : {kGlideStep, 16})
    {
      if (dir == 0 && hold != kGlideStep)
        continue;
      Input start;
      start.up = dir < 0;
      start.down = dir > 0;
      auto sim = world.cloneForSim();
      double score = 0.0;
      const int bounces0 = sim->glide().bounces;
      for (int f = 0; f < kGlideAhead && sim->state() == WorldState::Playing; ++f)
      {
        sim->update(glideInput(f < hold ? start : Input{}));
        score += double(sim->stats().gems - gems0) * (1000.0 - f);
        if (sim->glide().bounces > bounces0)
        {
          score -= 100000.0 - f * 100.0; // the later the better
          break;
        }
      }
      if (sim->state() != WorldState::Playing)
        score += 1e6; // through the exit
      score += double(sim->player().x);
      if (score > bestScore)
      {
        bestScore = score;
        bestStart = start;
        bestHold = hold;
      }
    }
  for (int f = 0; f < std::min(bestHold, kGlideStep); ++f)
    mGlideQueue.push_back(bestStart);
  const Input in = mGlideQueue.front();
  mGlideQueue.pop_front();
  return in;
}

} // namespace gr
