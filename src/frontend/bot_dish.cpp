// The bot in Level 32: riding a dish's rim or feed arm, it asks a copy of
// the world whether staying on through the next turn leaves it nearer the
// goal (the planner's distance field counts the whole sweep of a ride as
// floor, so on its own it jumps off toward the tower from the overhead pose
// instead of waiting for the west one). If so it stands still until the
// dish gets there; otherwise the planner does the rest.

#include "frontend/bot.hpp"
#include "game/world.hpp"

namespace gr
{

namespace
{

constexpr int kLookAhead = 400; // frames a copy rides on, at most
constexpr int kBetter = 6;      // how much nearer the later pose must be

// The ride the runner stands on, or -1.
int rideUnder(const World& world)
{
  const auto& p = world.player();
  if (p.state != PlayerState::OnGround)
    return -1;
  const auto& rides = world.dish().rides;
  for (std::size_t i = 0; i < rides.size(); ++i)
  {
    const auto& pl = world.platforms()[std::size_t(rides[i].platform)];
    if (p.y + 1 == pl.y && p.x + 3 > pl.x && p.x < pl.x + pl.w)
      return int(i);
  }
  return -1;
}

DishPhase phaseOf(const World& world, int ride)
{
  const auto& s = world.dish();
  const int dish = s.rides[std::size_t(ride)].dish;
  if (dish >= 0 && std::size_t(dish) < s.small.size())
    return dishPhase(s.small[std::size_t(dish)].t, true);
  return dishPhase(s.t, false);
}

} // namespace

bool Bot::dish(const World& world, Input& in)
{
  // Waiting: still on (a moving ride can be a cell off for a frame).
  if (mDishWait > 0 && world.player().state == PlayerState::OnGround)
  {
    --mDishWait;
    in = Input{};
    return true;
  }
  mDishWait = 0;
  const int ride = rideUnder(world);
  if (ride < 0)
  {
    mDishChecked = -1;
    return false;
  }
  // One look per ride and pose (or turn) it is in.
  const DishPhase ph = phaseOf(world, ride);
  const int key = ride * 16 + int(ph.from) * 4 + int(ph.to) + (ph.n > 0 ? 64 * 16 : 0);
  if (key == mDishChecked)
    return false;
  mDishChecked = key;
  // Ride on in a copy until the dish holds its next pose.
  auto w = world.cloneForSim();
  const DishPose start = ph.n > 0 ? ph.to : ph.from;
  int f = 0;
  for (; f < kLookAhead && w->state() == WorldState::Playing; ++f)
  {
    w->update(PlayerInput{});
    const DishPhase q = phaseOf(*w, ride);
    if (q.n == 0 && q.from != start)
      break;
  }
  if (f >= kLookAhead || w->state() != WorldState::Playing || rideUnder(*w) != ride ||
      w->player().hp < world.player().hp - 1)
    return false;
  if (mPlanner.heuristic(*w) + kBetter > mPlanner.heuristic(world))
    return false;
  mDishWait = f;
  in = Input{};
  return true;
}

} // namespace gr
