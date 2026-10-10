// The bot in Level 27: under the painting (the planner's goal 18 takes it
// there when it wants the bonus) it shoots up until the painting swings
// open and the safe's ladder is there; then the planner climbs to the
// projection booth's bonus door. The stealth itself is the planner's: an
// alarm costs it as much as a heart and a half (planner.cpp).

#include "frontend/bot.hpp"
#include "game/world.hpp"

#include <cstdlib>

namespace gr
{

bool Bot::noir(const World& world, Input& in)
{
  const auto& n = world.noir();
  const auto& p = world.player();
  if (n.paintX < 0 || n.paintOpen || !mPlanner.wantsBonus() || p.state != PlayerState::OnGround)
    return false;
  const int x = n.paintX * kCellsPerTile - 2;
  if (p.x + 1 < x || p.x + 1 >= x + 3 || p.y != (n.paintY + 4) * kCellsPerTile - 1)
    return false;
  in.up = true;
  in.fire = world.clock() % 2 == 0; // taps
  return true;
}

} // namespace gr
