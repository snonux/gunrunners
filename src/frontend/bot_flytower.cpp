// The bot in Level 40. The planner walks the fly tower's route
// (Planner::flyTowerGoal): the spot beside each truss, the catwalks, the
// pyro wall's ledges. These rules step in: at a truss, board it once it is
// home and stand still on it until it is up; on the ground, wait before a
// flame column that burns (or will within a few frames) between the runner
// and the next spot; with Pyro Rig fuel to spare, Dash, Nova and Skye
// jetpack up the pyro wall's left lane instead of climbing the ledges.
// Trigger Happy (rules=jumpfire) is a script: the discs storey by storey,
// a press to jump and another at the top of the jump.

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

constexpr int kFtSafeAhead = 12; // frames a flame must stay cold for the bot to cross it

// A flame column between the runner and where it is going, burning or about
// to: wait. One the runner is already in: get out (no wait).
bool flameAhead(const World& w, int dir)
{
  const auto& p = w.player();
  if (dir == 0)
    return false;
  const CellBox ahead{dir > 0 ? p.x + 3 : p.x - 10, p.y - 12, 10, 15};
  for (const auto& py : w.flyTower().pyros)
  {
    const CellBox f = py.flame();
    if (f.intersects(p.hitBox()))
      return false;
    if (f.intersects(ahead) && w.pyroHeat(py, kFtSafeAhead) > 0)
      return true;
  }
  return false;
}

bool nearStep(const Player& p, const FtStep& st, int dx)
{
  return p.state == PlayerState::OnGround && std::abs(p.x + 1 - st.x) <= dx && std::abs(p.y - st.y) <= 2;
}

} // namespace

bool Bot::flyTower(const World& w, Input& in)
{
  const auto& s = w.flyTower();
  const auto& p = w.player();
  if (p.state == PlayerState::Dying || p.state == PlayerState::Teleporting)
  {
    mFlyJet = false;
    return false;
  }
  const bool ground = p.state == PlayerState::OnGround;
  const int step = flyTowerStep(w);
  // The jetpack lane: from the spot at its foot, straight up to the floor
  // `arg` names, then left over its edge.
  if (!mFlyJet && ground && step > 0)
  {
    const auto& prev = s.route[std::size_t(step - 1)];
    const bool fuel = p.weapon == Weapon::Proto && p.proto == int(ProtoId::PyroRig) && p.ammo >= 36;
    const int c = w.characterIndex();
    if (prev.act == "jet" && fuel && (c == 0 || c == 2 || c == 4) && nearStep(p, prev, 2))
    {
      mFlyJet = true;
      mFlyDisc = step - 1; // the lane's step
    }
  }
  if (mFlyJet)
  {
    const auto& lane = s.route[std::size_t(std::clamp(mFlyDisc, 0, int(s.route.size()) - 1))];
    const int laneX = lane.x;
    const int top = std::atoi(lane.arg.c_str()) * kCellsPerTile + 1;
    const bool jetting = p.state == PlayerState::Jetpack;
    const bool fuel = p.weapon == Weapon::Proto && p.proto == int(ProtoId::PyroRig) && p.ammo > 0;
    if (!fuel || (!jetting && !ground && p.y > top) || (ground && p.y <= top))
    {
      mFlyJet = false;
      return false;
    }
    in.down = true;
    in.fire = true;
    if (p.y > top - 2)
    {
      if (p.x > laneX)
        in.left = true;
      else if (p.x < laneX)
        in.right = true;
      return true;
    }
    if (p.x > laneX - 4)
    {
      in.left = true;
      return true;
    }
    // Over the floor: let go and drop onto it.
    in = Input{};
    mFlyJet = false;
    return true;
  }
  if (step < 0)
  {
    // On to the exit (or the bonus): still mind the flames.
    if (ground)
    {
      const int goalX = w.level().exitTx * kCellsPerTile;
      if (flameAhead(w, sgn(goalX - (p.x + 1))) && mPlanner.wantsBonus() == false)
        return true;
    }
    return false;
  }
  const auto& st = s.route[std::size_t(step)];
  // A truss: board it once it is home, ride it up standing still.
  if (st.truss >= 0)
  {
    const auto& t = s.trusses[std::size_t(st.truss)];
    const auto& pl = w.platforms()[std::size_t(t.platform)];
    const int middle = pl.x + pl.w / 2 - 1; // the runner's x in its middle
    const bool on = ground && p.y + 1 == pl.y && p.x < pl.x + pl.w && pl.x < p.x + Player::kWidth;
    if (on)
    {
      if (std::abs(p.x - middle) > 1 && w.trussHome(t))
        (middle > p.x ? in.right : in.left) = true;
      return true;
    }
    if (nearStep(p, st, 6))
    {
      if (w.trussHome(t))
      {
        const int dir = sgn(middle - p.x);
        if (flameAhead(w, dir))
          return true;
        (dir > 0 ? in.right : in.left) = true;
      }
      return true; // it is away: wait here
    }
    return false;
  }
  // Wait before a flame on the way to the next spot.
  if (ground && flameAhead(w, sgn(st.x - (p.x + 1))))
    return true;
  return false;
}

// --- Trigger Happy ---------------------------------------------------------------------------

Input Bot::triggerHappy(const World& w)
{
  Input in;
  const auto& s = w.flyTower();
  const auto& p = w.player();
  const auto& map = w.map();
  if (p.state == PlayerState::Dying || p.state == PlayerState::Teleporting)
    return in;
  auto press = [&] {
    in.fire = !mFlyPrevFire;
    mFlyPrevFire = in.fire;
  };
  auto release = [&] { mFlyPrevFire = false; };
  // On a ladder: up to its top, then jump off toward the next storey's discs.
  if (p.state == PlayerState::Ladder)
  {
    const CellBox b = p.box();
    if (map.ladder(b.left() + 1, b.top() - 1))
    {
      in.up = true;
      release();
      return in;
    }
    const int dir = p.x < 40 ? 1 : -1;
    (dir > 0 ? in.right : in.left) = true;
    press();
    return in;
  }
  const int ground = (p.y + 1) / kCellsPerTile; // the floor's row
  const bool onGround = p.state == PlayerState::OnGround;
  const int gun = p.y - 2;
  // In the air: a press when the gun is level with the disc.
  if (!onGround)
  {
    if (mFlyDisc >= 0 && std::size_t(mFlyDisc) < s.targets.size() && !s.targets[std::size_t(mFlyDisc)].hit)
    {
      const CellBox t = s.targets[std::size_t(mFlyDisc)].box();
      if (gun >= t.y && gun <= t.bottom() && sgn(t.x - (p.x + 1)) == p.facing)
      {
        press();
        return in;
      }
    }
    if (mJumpHold != 0)
      (mJumpHold > 0 ? in.right : in.left) = true;
    release();
    return in;
  }
  mJumpHold = 0;
  // The disc it is after, else the nearest one left on this floor.
  int best = -1, bestD = 1 << 20;
  if (mFlyDisc >= 0 && std::size_t(mFlyDisc) < s.targets.size() && !s.targets[std::size_t(mFlyDisc)].hit &&
      s.targets[std::size_t(mFlyDisc)].y + 5 == ground)
    best = mFlyDisc;
  const bool keep = best >= 0;
  for (std::size_t i = 0; i < s.targets.size() && !keep; ++i)
  {
    const auto& t = s.targets[i];
    if (t.hit || t.y + 5 != ground)
      continue;
    const int d = std::abs(t.x * kCellsPerTile - (p.x + 1));
    if (d < bestD)
    {
      bestD = d;
      best = int(i);
    }
  }
  auto standable = [&](int x) {
    return x >= 0 && x + 2 < map.width() && map.solidTop(x + 1, p.y + 1) &&
      !map.overlapsSolid(boxAt(x, p.y, Player::kWidth, Player::kHeight));
  };
  int want = -1, face = 0;
  if (best >= 0)
  {
    const auto& t = s.targets[std::size_t(best)];
    const int left = t.x * kCellsPerTile - 7, right = t.x * kCellsPerTile + 6;
    const bool fromLeft = p.x + 1 < t.x * kCellsPerTile;
    const int first = fromLeft ? left : right, second = fromLeft ? right : left;
    want = standable(first) ? first : second;
    face = want < t.x * kCellsPerTile ? 1 : -1;
    mFlyDisc = best;
  }
  else
  {
    // This floor is done: its ladder (storey 1 right, storey 2 left).
    const int lx = ground >= 20 ? 36 : 3;
    want = lx * kCellsPerTile - 1;
    if (p.x == want)
    {
      in.up = true;
      release();
      return in;
    }
  }
  const int dir = sgn(want - p.x);
  if (dir != 0)
  {
    (dir > 0 ? in.right : in.left) = true;
    // A gap or a board just ahead: jump it (the press fires too).
    const int ahead = dir > 0 ? p.x + 3 : p.x - 1;
    const bool gap = !map.solidTop(ahead + dir, p.y + 1) && !map.solidTop(ahead, p.y + 1);
    const bool board = map.solid(ahead, p.y) || map.solid(ahead + dir, p.y);
    if ((gap || board) && std::abs(want - p.x) > 1)
    {
      press();
      mJumpHold = dir;
      return in;
    }
    release();
    return in;
  }
  if (p.facing != face)
  {
    (face > 0 ? in.right : in.left) = true;
    release();
    return in;
  }
  press();
  return in;
}

} // namespace gr
