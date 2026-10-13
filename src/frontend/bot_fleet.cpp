// The bot in Level 34. The planner walks it from ship to ship (its search
// sees the decks bob and drift in its copies of the world) to the near end
// of the grapple chain; there this takes over: standing, swinging or in
// the air, it tries hooking the next ring (forward, or up and forward, now
// or a few frames into a jump) in copies of the world and does the first
// that catches, and from the last ring the release that lands on the far
// deck. For the bonus it drops off D's bow, hooks the ring under C's hull
// on the way down, reels in and jumps up through the bomb bay's grating;
// the planner then takes the entrance. Human Cannonball: each shot's angle
// and steer, tried in copies, for the most points.

#include "frontend/bot.hpp"
#include "game/world.hpp"

#include <cstdlib>
#include <cstdio>

namespace gr
{

namespace
{

PlayerInput toPI(const Input& in, const Input& prev)
{
  PlayerInput p;
  p.left = in.left;
  p.right = in.right;
  p.up = in.up;
  p.down = in.down;
  p.jump = {in.jump, in.jump && !prev.jump};
  p.fire = {in.fire, in.fire && !prev.fire};
  return p;
}

Input make(int dx, bool up, bool jump, bool fire)
{
  Input in;
  in.left = dx < 0;
  in.right = dx > 0;
  in.up = up;
  in.jump = jump;
  in.fire = fire;
  return in;
}

bool dead(const World& w)
{
  const auto& p = w.player();
  return w.state() != WorldState::Playing || p.state == PlayerState::Dying || p.state == PlayerState::Teleporting;
}

// Plays seq in a copy, then `after` frames of `tail`; true as soon as ok(w).
template <typename Ok>
bool tryOut(const World& world, const Input& prev, const std::vector<Input>& seq, const Input& tail, int after, Ok ok,
  int* took = nullptr)
{
  auto w = world.cloneForSim();
  Input last = prev;
  const int total = int(seq.size()) + after;
  for (int i = 0; i < total; ++i)
  {
    const Input in = i < int(seq.size()) ? seq[std::size_t(i)] : tail;
    w->update(toPI(in, last));
    last = in;
    if (dead(*w))
      return false;
    if (ok(*w))
    {
      if (took)
        *took = i + 1;
      return true;
    }
  }
  return false;
}

// The chain's ring after `from` (by x), -1 if none.
int nextRing(const FleetState& f, int fromX)
{
  int best = -1;
  for (std::size_t i = 0; i < f.rings.size(); ++i)
  {
    const auto& r = f.rings[i];
    if (r.x > fromX && r.x > f.chainX0 && r.x < f.chainX1 && (best < 0 || r.x < f.rings[std::size_t(best)].x))
      best = int(i);
  }
  return best;
}

} // namespace

bool Bot::fleetQueue(Input& in)
{
  if (mFleetQueue.empty())
    return false;
  in = mFleetQueue.front();
  mFleetQueue.pop_front();
  mFleetPrev = in;
  mFleetLast = true;
  return true;
}

bool Bot::fleet(const World& world, Input& in)
{
  if (fleetQueue(in))
    return true;
  const auto& f = world.fleet();
  const auto& p = world.player();
  const Input prev = mFleetLast ? mFleetPrev : mPlanner.mPrev;
  mFleetLast = false;
  if (dead(world))
    return false;
  const bool hasHook = p.weapon == Weapon::Proto && ProtoId(p.proto) == ProtoId::GrappleGun && p.ammo > 0;
  const bool debug = std::getenv("GR_FLEET_DEBUG") != nullptr;
  auto take = [&](std::vector<Input> seq, int took, const char* what) {
    seq.resize(std::size_t(std::min<int>(int(seq.size()), took)));
    if (debug)
      std::fprintf(stderr, "fleet f%d %s at %d,%d (%zu frames)\n", world.stats().frames, what, p.x, p.y, seq.size());
    for (const auto& s : seq)
      mFleetQueue.push_back(s);
    return fleetQueue(in);
  };

  // --- The bomb bay, for the bonus -------------------------------------------------------------
  if (mPlanner.mTakeBonus && f.dropX >= 0 && !f.hatchOpen && hasHook)
  {
    int bay = -1;
    for (std::size_t i = 0; i < f.rings.size(); ++i)
      if (f.rings[i].x >= f.bayX0 && f.rings[i].x <= f.bayX1 && f.rings[i].y > f.bayY)
        bay = int(i);
    if (bay >= 0 && f.ring == bay)
    {
      // Reel in; jump straight up through the grating when that lands in the bay.
      const std::vector<Input> jump{make(0, false, true, false)};
      int took = 0;
      if (p.vineAt <= kLineMin + 1 &&
          tryOut(world, prev, jump, Input{}, 40, [](const World& w) { return w.fleet().hatchOpen; }, &took))
        return take(jump, 1, "bay jump");
      in = make(0, true, false, false);
      mFleetPrev = in;
      mFleetLast = true;
      return true;
    }
    if (bay >= 0 && p.state == PlayerState::OnGround && std::abs(p.x - f.dropX) <= 3 && p.y == f.dropY)
    {
      // Off the bow, and the hook at the ring as you fall past it.
      auto hooked = [bay](const World& w) { return w.fleet().ring == bay; };
      for (int k = 1; k <= 10; ++k)
        for (int j = 0; j <= 14; ++j)
        {
          std::vector<Input> seq(std::size_t(k), make(-1, false, false, false));
          for (int i = 0; i < j; ++i)
            seq.push_back(make(-1, false, false, false));
          seq.push_back(make(-1, false, false, true));
          int took = 0;
          if (tryOut(world, prev, seq, Input{}, 10, hooked, &took))
          {
            mPlanner.reset();
            return take(seq, took, "bay drop");
          }
        }
    }
  }
  if (f.hatchOpen && mPlanner.mTakeBonus && mPlanner.mSkipBonus)
  {
    mPlanner.mSkipBonus = false; // in the bay: the entrance is in reach now
    mPlanner.mSkipBonusAt = -1;
  }

  // --- The grapple chain ---------------------------------------------------------------------------
  if (f.chainX0 < 0 || p.x < f.chainX0 - 8 || p.x >= f.chainX1 || p.y >= f.killY)
    return false;
  const bool onFar = p.state == PlayerState::OnGround && p.x >= f.chainX1;
  if (onFar)
    return false;
  // Never back to a ring already swung from (on the deck: start again).
  if (p.state == PlayerState::OnGround)
    mFleetRingX = -1;
  if (f.ring >= 0)
    mFleetRingX = std::max(mFleetRingX, f.rings[std::size_t(f.ring)].x);
  const int fromX = std::max(mFleetRingX, f.ring >= 0 ? f.rings[std::size_t(f.ring)].x : p.x);
  const int target = nextRing(f, fromX);
  auto hookedNext = [&](const World& w) {
    const auto& g = w.fleet();
    return g.ring >= 0 && g.rings[std::size_t(g.ring)].x > fromX;
  };
  auto landed = [&](const World& w) {
    const auto& q = w.player();
    return q.state == PlayerState::OnGround && q.x >= w.fleet().chainX1;
  };
  const int dir = 1;
  if (p.state == PlayerState::OnGround)
  {
    if (!hasHook || target < 0)
      return false;
    // From the deck's end: hook now, after a few steps, or out of a jump.
    for (int k = 0; k <= 6; ++k)
      for (bool up : {true, false})
      {
        std::vector<Input> seq(std::size_t(k), make(dir, false, false, false));
        seq.push_back(make(dir, up, false, true));
        int took = 0;
        if (tryOut(world, prev, seq, Input{}, 12, hookedNext, &took))
          return take(seq, took, "hook from deck");
      }
    for (int k = 1; k <= 10; ++k)
      for (bool up : {true, false})
      {
        std::vector<Input> seq{make(dir, false, true, false)};
        for (int i = 1; i < k; ++i)
          seq.push_back(make(dir, false, true, false));
        seq.push_back(make(dir, up, true, true));
        int took = 0;
        if (tryOut(world, prev, seq, Input{}, 12, hookedNext, &took))
          return take(seq, took, "hook from a jump");
      }
    // Not in reach yet: on toward the end of the deck.
    if (p.x >= f.chainX0)
      return false;
    in = make(dir, false, false, false);
    mFleetPrev = in;
    mFleetLast = true;
    return true;
  }
  if (p.state == PlayerState::Swing && f.ring >= 0)
  {
    if (target < 0)
    {
      // The last ring: let go when that lands on the far deck.
      const std::vector<Input> jump{make(dir, false, true, false)};
      int took = 0;
      if (tryOut(world, prev, jump, make(dir, false, false, false), 40, landed, &took))
        return take(jump, 1, "release onto the deck");
    }
    else if (hasHook)
    {
      for (bool up : {true, false})
      {
        const std::vector<Input> seq{make(dir, up, false, true)};
        int took = 0;
        if (tryOut(world, prev, seq, Input{}, 12, hookedNext, &took))
          return take(seq, took, "hook from the swing");
      }
      for (int k = 1; k <= 12; ++k)
        for (bool up : {true, false})
        {
          std::vector<Input> seq{make(dir, false, true, false)};
          for (int i = 1; i < k; ++i)
            seq.push_back(make(dir, false, true, false));
          seq.push_back(make(dir, up, true, true));
          int took = 0;
          if (tryOut(world, prev, seq, Input{}, 12, hookedNext, &took))
            return take(seq, took, "hook after a release");
        }
    }
    // Pump the swing toward the far side.
    const auto& v = world.vines()[std::size_t(p.vine)];
    in = make(v.swingDir() > 0 ? 1 : 0, false, false, false);
    mFleetPrev = in;
    mFleetLast = true;
    return true;
  }
  if ((p.state == PlayerState::Jumping || p.state == PlayerState::Falling) && hasHook && target >= 0)
  {
    for (bool up : {true, false})
    {
      const std::vector<Input> seq{make(dir, up, false, true)};
      int took = 0;
      if (tryOut(world, prev, seq, Input{}, 12, hookedNext, &took))
        return take(seq, took, "hook in the air");
    }
  }
  return false;
}

// --- Human Cannonball -----------------------------------------------------------------------

Input Bot::cannonball(const World& world)
{
  Input in;
  if (fleetQueue(in))
    return in;
  const auto& f = world.fleet();
  mFleetLast = false;
  if (f.flying || f.reload > 0 || dead(world))
    return Input{};
  // Each angle with each steer: the one worth the most.
  int bestGain = -1;
  std::vector<Input> best;
  for (int angle : {30, 45, 60})
    for (int steer : {-1, 0, 1})
    {
      std::vector<Input> seq;
      int a = f.angle;
      while (a != angle)
      {
        const bool up = angle > a;
        Input press;
        press.up = up;
        press.down = !up;
        seq.push_back(press);
        seq.push_back(Input{});
        a += up ? 15 : -15;
      }
      seq.push_back(Input{});
      Input fire;
      fire.jump = true;
      seq.push_back(fire);
      for (int i = 0; i < 60; ++i)
        seq.push_back(make(steer, false, false, false));
      auto w = world.cloneForSim();
      const int before = w->stats().score;
      Input last = mFleetPrev;
      int used = 0;
      bool fired = false;
      for (const auto& s : seq)
      {
        w->update(toPI(s, last));
        last = s;
        ++used;
        fired = fired || w->fleet().flying;
        if (fired && !w->fleet().flying)
          break;
        if (w->state() != WorldState::Playing)
          break;
      }
      const int gain = w->stats().score - before;
      if (gain > bestGain)
      {
        bestGain = gain;
        best.assign(seq.begin(), seq.begin() + used);
      }
    }
  for (const auto& s : best)
    mFleetQueue.push_back(s);
  fleetQueue(in);
  return in;
}

} // namespace gr
