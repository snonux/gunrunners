// The bot in Level 31: in the sea caves with Tesla rods in hand, it sticks a
// rod in the dead generator (the next strike powers it and opens the hatch
// to the smugglers' room and the bonus); the planner does the rest, its
// search seeing the strikes coming in its copies of the world. In the Bungee
// bonus it tries every stretch either way in copies of the world and takes
// the fling that gets the most rings (the quickest of those).

#include "frontend/bot.hpp"
#include "game/world.hpp"

#include <cstdlib>

namespace gr
{

namespace
{

constexpr int kMaxStretch = 16; // frames a direction is held
constexpr int kFlight = 80;     // frames a fling is followed

PlayerInput heldInput(int dir)
{
  PlayerInput p;
  p.left = dir < 0;
  p.right = dir > 0;
  return p;
}

} // namespace

bool Bot::storm(const World& world, Input& in)
{
  if (!mStormQueue.empty())
  {
    in = mStormQueue.front();
    mStormQueue.pop_front();
    return true;
  }
  const auto& s = world.storm();
  const auto& p = world.player();
  if (p.state != PlayerState::OnGround || p.weapon != Weapon::Proto || p.proto != int(ProtoId::TeslaRod) ||
      p.ammo <= 0 || !mPlanner.wantsBonus())
    return false;
  // The generator: from a few blocks off on its floor, facing it, fire.
  for (const auto& m : s.machines)
  {
    if (m.kind != MachineKind::Generator || m.powered)
      continue;
    const int dx = m.box.x + m.box.w / 2 - (p.x + 1);
    const int shotRow = p.y - 2;
    if (std::abs(dx) < 6 || std::abs(dx) > 16 || shotRow < m.box.y || shotRow > m.box.bottom())
      continue;
    for (const auto& r : s.rods)
      if (r.machine >= 0)
      {
        in = Input{}; // one is in already: wait for the strike, clear of it
        return true;
      }
    const int dir = dx > 0 ? 1 : -1;
    Input turn, none, fire;
    turn.left = dir < 0;
    turn.right = dir > 0;
    fire.fire = true;
    if (p.facing != dir)
      mStormQueue.push_back(turn);
    mStormQueue.push_back(none);
    mStormQueue.push_back(fire);
    mStormQueue.push_back(none);
    in = mStormQueue.front();
    mStormQueue.pop_front();
    return true;
  }
  return false;
}

Input Bot::bungee(const World& world)
{
  if (!mStormQueue.empty())
  {
    const Input in = mStormQueue.front();
    mStormQueue.pop_front();
    return in;
  }
  const auto& s = world.storm();
  if (s.flying || world.player().state != PlayerState::OnGround)
    return Input{};
  const int score0 = world.stats().score;
  int bestDir = 0, bestK = 0, bestValue = -(1 << 30);
  for (int dir : {-1, 1})
    for (int k = 1; k <= kMaxStretch; ++k)
    {
      auto w = world.cloneForSim();
      for (int f = 0; f < k; ++f)
        w->update(heldInput(dir));
      int f = 0;
      for (; f < kFlight && w->state() == WorldState::Playing; ++f)
      {
        w->update(PlayerInput{});
        if (f > 2 && !w->storm().flying)
          break;
      }
      const int gain = w->stats().score - score0;
      const int value = gain * 4 - (k + f);
      if (value > bestValue)
      {
        bestValue = value;
        bestDir = dir;
        bestK = k;
      }
    }
  Input hold;
  hold.left = bestDir < 0;
  hold.right = bestDir > 0;
  for (int f = 0; f < bestK; ++f)
    mStormQueue.push_back(hold);
  mStormQueue.push_back(Input{});
  const Input in = mStormQueue.front();
  mStormQueue.pop_front();
  return in;
}

} // namespace gr
