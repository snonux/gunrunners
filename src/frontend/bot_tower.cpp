// The bot in Level 35. The planner climbs the turning tower by its route
// (Planner::towerGoal); these rules press up at the crawl space's doors
// (in for the bonus, out again after), and fight the Halcyon Titan from
// the top deck the way Gorgazilla is fought: short moves tried in copies
// of the world, each followed by a few simple policies, scored on its
// armour against the runner's hearts, with a spot to make for in each
// phase (in front at a safe range in the demo reel, close behind it in its
// orbit, beside whichever vent panel is open in the overheat). Copycat:
// walks tried in copies, for the most dummies in the pits.

#include "frontend/bot.hpp"
#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace gr
{

namespace
{

PlayerInput simIn(const Input& in, const Input& prev)
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

bool nearDoor(const World& w, const TowerDoor& d)
{
  const auto& p = w.player();
  return p.state == PlayerState::OnGround && std::abs(w.towerDx(d.x, p.x)) <= 1 && std::abs(p.y - d.y) <= 1;
}

// The runner's middle relative to the Titan's (cells, the short way round).
int fromTitan(const World& w)
{
  const auto& t = w.tower().titan;
  return w.towerDx(t.x + kTitanW / 2, w.player().x + 1);
}

// Where to stand, relative to the Titan's middle.
int titanSpot(const World& w)
{
  const auto& t = w.tower().titan;
  const int dx = fromTitan(w);
  switch (t.phase)
  {
    case TitanPhase::Orbit:
      return dx < 0 ? -12 : 12; // right behind it as it runs
    case TitanPhase::Overheat:
    {
      // Beside the open panel (or the next one to open), out of its steam.
      int i = t.open;
      if (i < 0)
        for (int k = 0; k < 4 && i < 0; ++k)
          if (t.panel[std::size_t((t.nextMove + k) % 4)] > 0)
            i = (t.nextMove + k) % 4;
      const bool back = i >= 2;
      return (back ? -t.facing : t.facing) * 15;
    }
    default:
      return t.facing * 22; // in front, out of the flames' reach
  }
}

// How far round to the spot (cells, signed), never through the Titan: a
// spot on its other side is the long way round the deck.
int spotGap(const World& w)
{
  const int W = w.map().width();
  const int dx = fromTitan(w), spot = titanSpot(w);
  int d = spot - dx;
  if ((dx < 0) != (spot < 0))
    d = dx < 0 ? -(W - std::abs(d)) : W - std::abs(d); // away from it, round the back
  return d;
}

// The way round to the spot: -1, 0 or 1.
int towardSpot(const World& w)
{
  const int d = spotGap(w);
  return std::abs(d) <= 1 ? 0 : (d < 0 ? -1 : 1);
}

// What a runner should do this frame by reflex: crouch under the laser,
// jump the shockwave and the lattice shots.
void titanReflex(const World& w, Input& in, const Input& prev)
{
  const auto& t = w.tower().titan;
  const auto& p = w.player();
  const int dx = fromTitan(w);
  const bool ground = p.state == PlayerState::OnGround;
  if (t.move == TitanMove::Laser && t.moveT >= kLaserTell - 4 && dx * t.facing > 0)
  {
    in.down = true;
    in.jump = false;
    in.left = in.right = false;
    return;
  }
  if (t.wave >= 0 && ground)
  {
    const int ahead = std::abs(dx) - t.wave;
    if (ahead > 0 && ahead <= 6)
      in.jump = !prev.jump;
  }
  // The hatch in the deck: jump it.
  const int way = in.left ? -1 : in.right ? 1 : 0;
  if (ground && way != 0 && !w.map().solidTop(way > 0 ? p.x + Player::kWidth + 1 : p.x - 2, p.y + 1))
    in.jump = !prev.jump;
  for (const auto& pr : w.projectiles())
    if (pr.alive && pr.kind == ShotKind::Enemy && ground &&
        pr.box().intersects({p.x - 6, p.y - 3, Player::kWidth + 12, 4}))
      in.jump = !prev.jump;
}

struct TitanMoveTry
{
  int dir;
  bool down;
  bool jump;
};

constexpr TitanMoveTry kTitanMoves[] = {
  {0, false, false}, {-1, false, false}, {1, false, false}, {0, true, false},
  {-1, false, true}, {1, false, true}, {0, false, true},
};

Input titanInput(const TitanMoveTry& m, int f, const World& w)
{
  Input in;
  in.left = m.dir < 0;
  in.right = m.dir > 0;
  in.down = m.down;
  in.jump = m.jump;
  in.fire = f % 2 == 1;
  (void)w;
  return in;
}

} // namespace

bool Bot::tower(const World& world, Input& in)
{
  const auto& t = world.tower();
  const auto& p = world.player();
  if (p.state == PlayerState::Dying)
    return false;
  // A door the planner brought us to: press up (in for the bonus, out after).
  bool want = false;
  for (const auto& pr : world.props())
    want = want || (pr.kind == PropKind::BonusDoor && !pr.used && !pr.dormant);
  want = want && mPlanner.wantsBonus();
  for (const auto& d : t.doors)
    if (nearDoor(world, d) && (d.id == "crawl") == want)
    {
      in.up = !mTowerPrev.up;
      mTowerPrev = in;
      return true;
    }
  // A Feedback Node about to push: wait just out of its ring's reach until
  // it has pushed (it cools for a while after), or back away from it.
  if (p.state == PlayerState::OnGround)
    for (const auto& n : t.nodes)
    {
      const auto& e = world.enemies()[std::size_t(n.enemy)];
      if (!e.alive || n.state == 3)
        continue;
      const CellBox b = e.box(), pb = p.hitBox();
      const int dx = world.towerDx(b.x + 2, pb.x + 1);
      const int dy = (pb.y + 2) - (b.y + 2);
      const float dist = std::sqrt(float(dx * dx + dy * dy));
      if (dist > float(kNodeRadius + 4))
        continue;
      if (dist > float(kNodeRadius))
      {
        mTowerPrev = in; // stand still
        return true;
      }
      // Inside its ring: away from it if the ledge goes on that way.
      const int away = dx < 0 ? -1 : 1;
      const int ax = away > 0 ? p.x + Player::kWidth : p.x - 1;
      if (world.map().solidTop(ax, p.y + 1))
      {
        (away < 0 ? in.left : in.right) = true;
        mTowerPrev = in;
        return true;
      }
    }
  mTowerPrev = Input{};
  return false;
}

Input Bot::fightTitan(const World& world)
{
  const auto& p = world.player();
  const auto& t = world.tower().titan;
  if (mFightQueue.empty())
  {
    constexpr int kAhead = 36;
    const int hp0 = p.hp, ti0 = world.titanHp();
    const int deck = t.deckY;
    int best = 0;
    long bestScore = std::numeric_limits<long>::min();
    for (int m = 0; m < int(sizeof(kTitanMoves) / sizeof(kTitanMoves[0])); ++m)
    {
      const auto firstPtr = world.cloneForSim();
      World& first = *firstPtr;
      Input prev = mFightPrev;
      bool dead = false;
      for (int f = 0; f < 4 && !dead; ++f)
      {
        const Input in = titanInput(kTitanMoves[m], f, first);
        first.update(simIn(in, prev));
        prev = in;
        dead = first.player().state == PlayerState::Dying;
      }
      if (dead)
        continue;
      const long firstScore = 300L * (ti0 - first.titanHp());
      long moveScore = std::numeric_limits<long>::min();
      for (int follow = 0; follow < 4; ++follow)
      {
        World sim(first);
        Input fp = prev;
        long score = firstScore;
        int ti = sim.titanHp();
        for (int f = 0; f < kAhead; ++f)
        {
          Input in;
          const auto& sp = sim.player();
          const int dx = fromTitan(sim);
          const int face = dx > 0 ? -1 : 1; // toward it
          const int go = towardSpot(sim);
          switch (follow)
          {
            case 0: // to the spot, firing; turn to it once there
            case 1: // the same, crouched once there
              if (go != 0)
                (go < 0 ? in.left : in.right) = true;
              else if (sp.facing != face)
                (face < 0 ? in.left : in.right) = true;
              else
                in.down = follow == 1;
              break;
            case 2: // stay, facing it
              if (sp.facing != face)
                (face < 0 ? in.left : in.right) = true;
              break;
            case 3: // away from it
              (face < 0 ? in.right : in.left) = true;
              break;
          }
          in.fire = f % 2 == 1;
          titanReflex(sim, in, fp);
          sim.update(simIn(in, fp));
          fp = in;
          score += long(ti - sim.titanHp()) * (300 - 4 * f);
          ti = sim.titanHp();
          if (sim.player().state == PlayerState::Dying || sim.state() != WorldState::Playing)
            break;
        }
        const auto& sp = sim.player();
        const long lost = std::max(0, hp0 - sp.hp);
        score -= lost * (800L + 2000L / std::max(1, sp.hp));
        if (sp.state == PlayerState::Dying)
          score -= 1000000L;
        if (sp.y > deck + 1)
          score -= 20000L; // through the hatch
        score -= long(std::abs(spotGap(sim))) * 6;
        moveScore = std::max(moveScore, score);
      }
      if (moveScore > bestScore)
      {
        bestScore = moveScore;
        best = m;
      }
    }
    static const bool debug = std::getenv("GR_FIGHT_DEBUG") != nullptr;
    if (debug)
      std::fprintf(stderr, "titan f%d phase %d move %d/%d core %d open %d hp %d at %d; runner %d,%d dx %d hp %d -> move %d (%ld)\n",
        world.stats().frames, int(t.phase), int(t.move), t.moveT, t.core, t.open, world.titanHp(), t.x, p.x, p.y,
        fromTitan(world), p.hp, best, bestScore);
    for (int f = 0; f < 4; ++f)
      mFightQueue.push_back(titanInput(kTitanMoves[best], f, world));
  }
  Input in = mFightQueue.front();
  mFightQueue.pop_front();
  mFightPrev = in;
  return in;
}

Input Bot::copycat(const World& world)
{
  if (!mFightQueue.empty())
  {
    Input in = mFightQueue.front();
    mFightQueue.pop_front();
    mFightPrev = in;
    return in;
  }
  // A step (or a hop) now, then a walk one way for a while, jumping the
  // floor's holes: the start that walks the most dummies into pits, soonest.
  const int caught0 = world.tower().caught;
  long bestScore = std::numeric_limits<long>::min();
  std::vector<Input> bestSeq;
  for (int dir = -1; dir <= 1; ++dir)
    for (int jump = 0; jump <= 1; ++jump)
    {
      std::vector<Input> seq;
      for (int f = 0; f < 4; ++f)
      {
        Input in;
        in.left = dir < 0;
        in.right = dir > 0;
        in.jump = jump && f == 0;
        seq.push_back(in);
      }
      auto first = world.cloneForSim();
      Input prev = mFightPrev;
      bool lost = false;
      for (const auto& in : seq)
      {
        first->update(simIn(in, prev));
        prev = in;
        lost = lost || first->state() != WorldState::Playing || first->bonusFailed();
      }
      if (lost && !(first->state() != WorldState::Playing && !first->bonusFailed()))
        continue;
      long score = long(first->tower().caught - caught0) * 10000L;
      long follow = 0;
      for (int way = -1; way <= 1; way += 2)
      {
        World sim(*first);
        Input fp = prev;
        long s = 0;
        for (int f = 0; f < 48 && sim.state() == WorldState::Playing; ++f)
        {
          Input in;
          in.left = way < 0;
          in.right = way > 0;
          // A hole in the floor just ahead: jump it.
          const auto& sp = sim.player();
          const int ax = way > 0 ? sp.x + Player::kWidth + 1 : sp.x - 2;
          if (sp.state == PlayerState::OnGround && !sim.map().solidTop(ax, sp.y + 1))
            in.jump = !fp.jump;
          sim.update(simIn(in, fp));
          fp = in;
          if (sim.bonusFailed())
          {
            s -= 1000000L;
            break;
          }
          s = std::max(s, long(sim.tower().caught - first->tower().caught) * 10000L - f * 10L);
        }
        follow = std::max(follow, s);
      }
      score += follow;
      if (score > bestScore)
      {
        bestScore = score;
        bestSeq = seq;
      }
    }
  for (const auto& in : bestSeq)
    mFightQueue.push_back(in);
  if (mFightQueue.empty())
    mFightQueue.push_back(Input{});
  Input in = mFightQueue.front();
  mFightQueue.pop_front();
  mFightPrev = in;
  return in;
}

} // namespace gr
