// The bot in Level 37. The planner walks the wardrobe route
// (Planner::wardrobeGoal): into each booth the route names, onto each heavy
// plate. These rules step in: up inside the booth, stand still behind the
// curtain, wait while a cutting table comes down. The Totem bonus is played
// by a script: at each tower throw Nova, then Dash, onto it; walk them off
// it to their spots past the tower; switch back to Rocco, who walks through
// the tunnel and picks them up again on the heavy plate.

#include "frontend/bot.hpp"
#include "frontend/planner.hpp"
#include "game/world.hpp"

#include <algorithm>
#include <cstdlib>

namespace gr
{

namespace
{

int stackSize(const World& w, int base)
{
  int n = 0;
  for (int i = base; i >= 0; i = w.totemAbove(i))
    ++n;
  return n;
}

} // namespace

Input Bot::totem(const World& w)
{
  const auto& s = w.wardrobe();
  const auto& p = w.player();
  Input in;
  const int me = w.totemPlayer();
  if (me < 0 || p.state == PlayerState::Dying)
    return in;
  int rocco = -1;
  for (std::size_t i = 0; i < s.bodies.size(); ++i)
    if (s.bodies[i].runner == 1)
      rocco = int(i);
  const int rx = rocco == me ? p.x : s.bodies[std::size_t(rocco)].x;
  // The tower Rocco has not passed yet.
  std::size_t k = 0;
  while (k < s.towers.size() && s.towers[k].x + 4 <= rx)
    ++k;
  auto walkTo = [&](int x) {
    if (p.x < x)
      in.right = true;
    else if (p.x > x)
      in.left = true;
    return p.x == x;
  };
  const int runner = s.bodies[std::size_t(me)].runner;
  if (runner == 1)
  {
    if (k < s.towers.size() && stackSize(w, me) >= 2)
    {
      if (walkTo(s.towers[k].x - 7) && p.state == PlayerState::OnGround && s.throwT == 0)
      {
        in.jump = !mRobePrevJump;
        mRobePrevJump = in.jump;
      }
      else
        mRobePrevJump = false;
      if (in.jump && p.facing < 0)
        in.right = true;
      return in;
    }
    in.right = true; // through the tunnel, picking them up; then to the exit
    return in;
  }
  // Dash or Nova on their own: off the tower to their spot, then back to Rocco.
  if (p.state != PlayerState::OnGround || s.throwT > 0 || k >= s.towers.size())
    return in;
  const auto& t = s.towers[k];
  const int spot = runner == 2 ? t.plateX + 1 : t.x + 6;
  if (walkTo(spot) && p.y >= s.bodies[std::size_t(rocco)].y - 1)
    in.down = true;
  return in;
}

bool Bot::wardrobe(const World& w, Input& in)
{
  const auto& s = w.wardrobe();
  const auto& p = w.player();
  if (s.totem)
  {
    in = totem(w);
    return true;
  }
  if (s.boothT > 0 || s.sit > 0)
    return true; // behind the curtain
  const int i = wardrobeStep(w);
  if (i < 0 || p.state != PlayerState::OnGround)
  {
    mRobePrevUp = false;
    return false;
  }
  const auto& st = s.route[std::size_t(i)];
  const int mid = p.x + 1;
  const bool here = mid >= st.x && mid < st.x + 4 && std::abs(p.y - st.y) <= 1;
  if (st.act == "booth" && here)
  {
    in.up = !mRobePrevUp;
    mRobePrevUp = in.up;
    return true;
  }
  if (st.act == "plate")
    for (const auto& pl : s.plates)
    {
      if (pl.id != st.arg || pl.onTable.empty())
        continue;
      // Its table is still coming down: wait at the pit's edge.
      for (const auto& t : s.tables)
        if (t.id == pl.onTable && !t.down() && p.x < t.x0 * kCellsPerTile)
          return true;
    }
  mRobePrevUp = false;
  return false;
}

} // namespace gr
