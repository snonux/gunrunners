// The bot in Level 36. The planner walks the warehouse route
// (Planner::warehouseGoal): to each lever, to the spot behind a crate that
// has to be pushed into place, to each throw spot in the Throw Everything
// bonus. These rules do the job once the runner stands there: up at a
// lever, stand still while its crane works, tap the Magnet Gun at the
// crate until it is where the route wants it; in the bonus, lift the
// nearest loose block and throw it onto the stack.

#include "frontend/bot.hpp"
#include "frontend/planner.hpp"
#include "game/world.hpp"

#include <algorithm>
#include <cstdlib>

namespace gr
{

namespace
{

int sgn(int v) { return (v > 0) - (v < 0); }

// A loose block in reach of the runner's hands on the side `dir`.
bool looseInReach(const World& w, int dir)
{
  const auto& p = w.player();
  const int fx = dir > 0 ? p.x + Player::kWidth : p.x - 1;
  const int tx = fx / kCellsPerTile;
  for (int ty : {p.y / kCellsPerTile, (p.y - 2) / kCellsPerTile, (p.y - 4) / kCellsPerTile})
    if (warehouseLiftable(w, tx, ty))
      return true;
  return false;
}

} // namespace

bool Bot::warehouse(const World& w, Input& in)
{
  const auto& s = w.warehouse();
  const auto& p = w.player();
  const int i = warehouseStep(w);
  if (i < 0 || p.state != PlayerState::OnGround)
  {
    mWarePrev = Input{};
    return false;
  }
  const auto& st = s.route[std::size_t(i)];
  auto tap = [&](bool& key) {
    key = !mWarePrevFire;
    mWarePrevFire = key;
  };
  if (st.act == "stack")
  {
    if (s.liftT > 0)
      return true; // lifting: hold still
    const int col = st.arg.empty() ? 0 : std::atoi(st.arg.c_str());
    if (s.carry >= 0)
    {
      if ((p.x + 1) / kCellsPerTile != st.x / kCellsPerTile || std::abs(p.y - st.y) > 1)
        return false;
      const int dir = sgn(col * kCellsPerTile + 1 - (p.x + 1));
      if (dir != 0 && p.facing != dir)
      {
        (dir > 0 ? in.right : in.left) = true;
        return true;
      }
      tap(in.fire);
      return true;
    }
    for (int dir : {1, -1})
      if (looseInReach(w, dir))
      {
        if (p.facing != dir)
          (dir > 0 ? in.right : in.left) = true;
        else
          tap(in.fire);
        return true;
      }
    return false;
  }
  const bool here = std::abs(p.x - st.x) <= 3 && std::abs(p.y - st.y) <= 2;
  if (st.act == "lever")
  {
    if (!here)
      return false;
    in.up = !mWarePrev.up;
    mWarePrev = in;
    return true;
  }
  if (st.act == "wait")
    return here; // stand still while the crane works
  if (st.act == "push")
  {
    for (const auto& c : s.crates)
    {
      if (c.id != st.arg)
        continue;
      if (c.state != CrateState::Rest)
        return std::abs(p.x - c.x) < 24; // sliding: wait for it
      const int gap = c.x - (p.x + Player::kWidth);
      if (gap < 2 || gap > 10 || std::abs((c.y + kCrateSize - 1) - p.y) > 1)
        return false;
      if (p.facing < 0)
      {
        in.right = true;
        return true;
      }
      tap(in.fire);
      return true;
    }
  }
  return false;
}

} // namespace gr
