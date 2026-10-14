// The bot in Level 41. The planner walks the stage to the exit (and, for
// the bonus, into the hollow under the stands for the letter G:
// Planner::sweepsGoal); in Paintball it walks from one unpainted block top
// to the next. These rules step in: shoot the Virus's V once it is level
// with the gun, shoot an Audience Plant that stands up in front. In MIRROR
// MODE the world swaps left and right, so the rules swap them back (the
// planner plans in copies of the world, which swap them too).

#include "frontend/bot.hpp"
#include "frontend/planner.hpp"
#include "game/world.hpp"

#include <cstdlib>
#include <utility>

namespace gr
{

namespace
{

int sgn(int v) { return (v > 0) - (v < 0); }

// Something to shoot level with the gun, within `reach` cells: the way to
// face, 0 if none.
int aimAt(const World& w, const CellBox& t, int reach)
{
  const auto& p = w.player();
  const int gun = p.y - 2, mid = p.x + 1;
  if (gun < t.top() || gun > t.bottom() || !w.sweepsVisible(t))
    return 0;
  const int dx = t.x + t.w / 2 - mid;
  if (std::abs(dx) > reach || std::abs(dx) < 3)
    return 0;
  // Nothing solid in the way.
  for (int x = mid; x != t.x + t.w / 2; x += sgn(dx))
    if (w.map().solid(x, gun))
      return 0;
  return sgn(dx);
}

} // namespace

bool Bot::sweeps(const World& w, Input& in)
{
  const auto& s = w.sweeps();
  const auto& p = w.player();
  if (s.paint || p.state != PlayerState::OnGround)
  {
    mSwPrevFire = false;
    return false;
  }
  int face = 0;
  // The V of VIRUS FOR THE RUNNER, once it is level with the gun.
  for (const auto& it : w.items())
    if (!it.taken && it.kind == ItemKind::Virus && it.variant == 7)
      face = face != 0 ? face : aimAt(w, it.box(), 30);
  // A Plant that stood up, in front.
  for (const auto& a : s.actors)
  {
    const Enemy& e = w.enemies()[std::size_t(a.enemy)];
    if (face == 0 && e.alive && e.kind == EnemyKind::AudiencePlant && a.stand > 0 && e.stun == 0)
      face = aimAt(w, e.box(), 24);
  }
  if (face == 0)
  {
    mSwPrevFire = false;
    return false;
  }
  if (p.facing != face)
    (face > 0 ? in.right : in.left) = true;
  else
  {
    in.fire = !mSwPrevFire;
    mSwPrevFire = in.fire;
  }
  // MIRROR MODE: the world swaps them back.
  if (w.sweepsMirrored())
    std::swap(in.left, in.right);
  return true;
}

} // namespace gr
