// The bot in Level 33: for the bonus, from the coolant pipe's nook it turns
// to the RATINGS server and shoots it (the planner walks it there, goal 18);
// that opens the bonus entrance. The planner does the rest, its search
// seeing the lava, the Firewalls' dips and the Fan Bots' gusts coming in
// its copies of the world.

#include "frontend/bot.hpp"
#include "game/world.hpp"

#include <cstdlib>

namespace gr
{

bool Bot::magma(const World& world, Input& in)
{
  if (!mMagmaQueue.empty())
  {
    in = mMagmaQueue.front();
    mMagmaQueue.pop_front();
    return true;
  }
  const auto& m = world.magma();
  const auto& p = world.player();
  if (m.serverX < 0 || m.serverShot || !mPlanner.wantsBonus() || p.state != PlayerState::OnGround)
    return false;
  const CellBox server = m.serverBox();
  const int dx = server.x + server.w / 2 - (p.x + 1);
  const int shotRow = p.y - 2;
  if (std::abs(dx) > 12 || shotRow < server.y || shotRow > server.bottom())
    return false;
  const int dir = dx > 0 ? 1 : -1;
  Input turn, none, fire;
  turn.left = dir < 0;
  turn.right = dir > 0;
  fire.fire = true;
  if (p.facing != dir)
    mMagmaQueue.push_back(turn);
  mMagmaQueue.push_back(none);
  mMagmaQueue.push_back(fire);
  mMagmaQueue.push_back(none);
  mMagmaQueue.push_back(none);
  in = mMagmaQueue.front();
  mMagmaQueue.pop_front();
  return true;
}

} // namespace gr
