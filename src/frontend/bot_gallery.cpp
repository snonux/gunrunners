// The bot in Level 39. The planner walks the gallery's route
// (Planner::galleryGoal): the spot in front of each monitor it takes, the
// floor of each room. These rules step in: up into the monitor at a hop
// step (a panning one once its camera is over the right side), a jump over
// a Cable Wrangler's cable, and in the Rewind bonus a wait before each
// crusher until it has just come back up.

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

} // namespace

bool Bot::gallery(const World& w, Input& in)
{
  const auto& s = w.gallery();
  const auto& p = w.player();
  if (s.hop > 0 || s.rewinding > 0 || s.trip > 0)
  {
    mGalleryUp = false;
    return true; // static, the tape, or lying on the floor
  }
  if (p.state == PlayerState::Dying || p.state == PlayerState::Teleporting)
    return false;
  const bool ground = p.state == PlayerState::OnGround;
  const int mid = p.x + 1;
  const int step = galleryStep(w);
  int goalX = w.level().exitTx * kCellsPerTile;
  if (step >= 0)
    goalX = s.route[std::size_t(step)].x;
  const int dir = sgn(goalX - mid) != 0 ? sgn(goalX - mid) : p.facing;
  // Up into the monitor.
  if (step >= 0 && ground)
  {
    const auto& st = s.route[std::size_t(step)];
    if ((st.act == "hop" || st.act == "pan") && std::abs(mid - st.x) <= 5 && std::abs(mid - st.x) > 1 &&
        std::abs(p.y - st.y) <= 1)
    {
      // Right in front of it first.
      (st.x > mid ? in.right : in.left) = true;
      mGalleryUp = false;
      return true;
    }
    if ((st.act == "hop" || st.act == "pan") && std::abs(mid - st.x) <= 1 && std::abs(p.y - st.y) <= 1)
    {
      bool go = true;
      if (st.act == "pan")
        for (const auto& m : s.monitors)
          if (w.atMonitor(m) && m.cam >= 0)
          {
            const int cx = w.camX(s.cams[std::size_t(m.cam)]);
            go = cx >= st.lo && cx <= st.hi;
          }
      in.up = go && !mGalleryUp;
      mGalleryUp = in.up;
      return true;
    }
  }
  mGalleryUp = false;
  // A cable on the floor just ahead: jump it.
  if (ground)
    for (const auto& a : s.actors)
    {
      const Enemy& e = w.enemies()[std::size_t(a.enemy)];
      if (!e.alive || a.dropped || e.kind != EnemyKind::CableWrangler || std::abs(a.cableY - p.y) > 1)
        continue;
      const int near = dir > 0 ? a.cableX0 - (p.x + 2) : (p.x) - a.cableX1;
      if (near >= 1 && near <= 4)
      {
        in.jump = true;
        (dir > 0 ? in.right : in.left) = true;
        return true;
      }
    }
  // Rewind: wait before a crusher till it has just risen.
  if (ground)
    for (const auto& c : s.crushers)
    {
      const int x0 = c.x0 * kCellsPerTile, x1 = x0 + c.w * kCellsPerTile - 1;
      const int before = dir > 0 ? x0 - (p.x + 2) : p.x - x1;
      if (before >= 1 && before <= 5 && !(c.t <= 1 || c.t >= c.period - 1))
        return true;
    }
  return false;
}

} // namespace gr
