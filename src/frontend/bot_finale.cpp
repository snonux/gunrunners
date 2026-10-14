// The bot in Level 42. The planner walks the stage to the exit by the
// route's steps (Planner::finaleGoal: the station's floor plate, its
// ceiling plate, the studio's first monitor; in Off the Air the switch's
// console). These rules step in: stand still while the turntable turns a
// wagon in (and through a monitor's wipe or a commercial), wait on a
// gravity plate until the chamber turns over, press up into a monitor,
// break the switch's glass cover. Lance Marquee and the Encore Engine are
// fought the way the Titan is: short moves tried in copies of the world,
// each followed by a few simple policies, scored on the boss's health
// against the runner's hearts, with a spot to make for (under the podium,
// out of the Engine's flame reach; for the bonus, the open trapdoor).

#include "frontend/bot.hpp"
#include "frontend/planner.hpp"
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

int sgn(int v) { return (v > 0) - (v < 0); }

int bossHp(const World& w)
{
  const auto& b = w.finale().boss;
  return std::max(0, b.lanceHp) + std::max(0, b.engineHp);
}

// The trapdoor open (or opening) now for the bonus: its middle (cells), -1 none.
int bonusTrap(const World& w, bool want)
{
  const auto& f = w.finale();
  const auto& b = f.boss;
  if (!want || f.bonusDone || b.phase != FnPhase::Lance || b.attack != FnAttack::Trapdoor || b.trap < 0)
    return -1;
  const int li = f.traps[std::size_t(b.trap)];
  if (li < 0)
    return -1;
  const Layer& l = w.layers()[std::size_t(li)];
  return (l.x0 + l.x1 + 1) * kCellsPerTile / 2;
}

// Where the runner's left cell wants to be.
int fightSpot(const World& w, bool want)
{
  const auto& b = w.finale().boss;
  const auto& p = w.player();
  const int trap = bonusTrap(w, want);
  if (trap >= 0)
    return trap - 1;
  if (b.phase == FnPhase::Lance)
    return b.x + kFnPodiumW / 2 - 1; // under the podium's front panel
  if (b.phase == FnPhase::Engine)
    return b.ex - kFnBreathReach - Player::kWidth - 3; // out of the flame's reach, the core level with the gun
  return p.x;
}

// Which way the runner fires: 0 up (at the podium), 1 ahead.
bool fireUp(const World& w) { return w.finale().boss.phase == FnPhase::Lance; }

// Reflexes: jump the laugh wave and the coins, crouch under the barrage and the pulse.
void fightReflex(const World& w, Input& in, const Input& prev)
{
  const auto& b = w.finale().boss;
  const auto& p = w.player();
  const bool ground = p.state == PlayerState::OnGround;
  const int mid = p.x + 1;
  for (int wx : {b.waveL, b.waveR, b.coinL, b.coinR})
    if (wx >= 0 && ground)
    {
      const int ahead = std::abs(wx - mid);
      if (ahead >= 2 && ahead <= 6 && ((wx < mid) == (wx == b.waveL || wx == b.coinL)))
        in.jump = !prev.jump;
    }
  if (b.phase == FnPhase::Engine && ground && !in.jump)
  {
    bool duck = b.ring >= 0 && b.ex - b.ring - mid <= 8;
    for (const auto& pr : w.projectiles())
      duck = duck || (pr.alive && pr.kind == ShotKind::Enemy && pr.y == p.y - 4 && pr.x > mid - 2 && pr.x - mid <= 8);
    if (duck)
    {
      in.down = true;
      in.left = in.right = false;
      in.up = false;
    }
  }
}

struct FnMove
{
  int dir;
  bool down, jump, up;
};

constexpr FnMove kFnMoves[] = {
  {0, false, false, false}, {-1, false, false, false}, {1, false, false, false}, {0, true, false, false},
  {-1, false, true, false}, {1, false, true, false},   {0, false, true, false},  {0, false, false, true},
  {-1, false, false, true}, {1, false, false, true},
};

Input fnInput(const FnMove& m, int f, bool up)
{
  Input in;
  in.left = m.dir < 0;
  in.right = m.dir > 0;
  in.down = m.down;
  in.jump = m.jump;
  in.up = m.up || (up && m.dir == 0 && !m.down && !m.jump);
  in.fire = f % 2 == 1;
  return in;
}

// An enemy level with the gun within `reach` cells: the way to face, 0 if none.
int aimAt(const World& w, const CellBox& t, int reach)
{
  const auto& p = w.player();
  const int gun = p.y - 2, mid = p.x + 1;
  if (gun < t.top() || gun > t.bottom() || !w.finaleVisible(t))
    return 0;
  const int dx = t.x + t.w / 2 - mid;
  if (std::abs(dx) > reach || std::abs(dx) < 2)
    return 0;
  for (int x = mid; x != t.x + t.w / 2; x += sgn(dx))
    if (w.map().solid(x, gun))
      return 0;
  return sgn(dx);
}

} // namespace

Input Bot::fightFinale(const World& world)
{
  const auto& p = world.player();
  const auto& f = world.finale();
  const auto& b = f.boss;
  const bool want = mPlanner.wantsBonus() && !f.bonusDone;
  if (mFightQueue.empty())
  {
    constexpr int kAhead = 36;
    const int hp0 = p.hp, boss0 = bossHp(world);
    const bool up0 = fireUp(world);
    int best = 0;
    long bestScore = std::numeric_limits<long>::min();
    for (int m = 0; m < int(sizeof(kFnMoves) / sizeof(kFnMoves[0])); ++m)
    {
      const auto firstPtr = world.cloneForSim();
      World& first = *firstPtr;
      Input prev = mFightPrev;
      bool dead = false;
      for (int k = 0; k < 4 && !dead; ++k)
      {
        const Input in = fnInput(kFnMoves[m], k, up0);
        first.update(simIn(in, prev));
        prev = in;
        dead = first.player().state == PlayerState::Dying;
      }
      if (dead)
        continue;
      const long firstScore = 300L * (boss0 - bossHp(first)) + (first.finale().bonusDone && want ? 1000000L : 0L);
      long moveScore = std::numeric_limits<long>::min();
      for (int follow = 0; follow < 4; ++follow)
      {
        World sim(first);
        Input fp = prev;
        long score = firstScore;
        int hpB = bossHp(sim);
        bool bonus = sim.finale().bonusDone;
        for (int k = 0; k < kAhead; ++k)
        {
          Input in;
          const auto& sp = sim.player();
          const int spot = fightSpot(sim, want);
          const int go = std::abs(spot - sp.x) <= 1 ? 0 : sgn(spot - sp.x);
          const bool up = fireUp(sim);
          switch (follow)
          {
            case 0: // to the spot, firing
            case 1: // the same, crouched once there
              if (go != 0)
                (go < 0 ? in.left : in.right) = true;
              else if (!up && sp.facing != 1)
                in.right = true;
              else
              {
                in.down = follow == 1;
                in.up = up && follow == 0;
              }
              break;
            case 2: // stay
              in.up = up;
              break;
            case 3: // away from the spot
              (go <= 0 ? in.right : in.left) = true;
              break;
          }
          in.fire = k % 2 == 1;
          fightReflex(sim, in, fp);
          sim.update(simIn(in, fp));
          fp = in;
          score += long(hpB - bossHp(sim)) * (300 - 4 * k);
          hpB = bossHp(sim);
          if (!bonus && sim.finale().bonusDone && want)
          {
            score += 1000000L - 1000L * k;
            bonus = true;
          }
          if (sim.player().state == PlayerState::Dying || sim.state() != WorldState::Playing)
            break;
        }
        const auto& sp = sim.player();
        const long lost = std::max(0, hp0 - sp.hp);
        score -= lost * (800L + 2000L / std::max(1, sp.hp));
        if (sp.state == PlayerState::Dying)
          score -= 1000000L;
        score -= long(std::abs(fightSpot(sim, want) - sp.x)) * 6;
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
      std::fprintf(stderr, "finale f%d phase %d atk %d move %d/%d hp %d/%d runner %d,%d hp %d -> move %d (%ld)\n",
                   world.stats().frames, int(b.phase), int(b.attack), int(b.move), b.moveT, b.lanceHp, b.engineHp, p.x,
                   p.y, p.hp, best, bestScore);
    for (int k = 0; k < 4; ++k)
      mFightQueue.push_back(fnInput(kFnMoves[best], k, up0));
  }
  Input in = mFightQueue.front();
  mFightQueue.pop_front();
  mFightPrev = in;
  return in;
}

bool Bot::finale(const World& w, Input& in)
{
  const auto& f = w.finale();
  const auto& p = w.player();
  if (f.reel)
    return false;
  // The world holds still: wait it out.
  if (w.finaleFrozen())
  {
    mFnPrev = in;
    return true;
  }
  if (p.state == PlayerState::Dying)
    return false;
  bool station = false, studio = false;
  for (const auto& wg : f.wagons)
  {
    station = station || (wg.kind == FnWagonKind::Station && wg.state == FnWagonState::Running);
    studio = studio || (wg.kind == FnWagonKind::Studio && wg.state == FnWagonState::Running);
  }
  // A gravity plate of the route: stand on it until the chamber turns over.
  if (station && p.state == PlayerState::OnGround)
    for (const auto& pl : f.plates)
    {
      const CellBox plate{pl.bx * kCellsPerTile, pl.by * kCellsPerTile, kCellsPerTile, kCellsPerTile};
      if (p.box().intersects(plate) && p.grav != pl.dir && pl.zone >= 0 &&
          w.grav().zones[std::size_t(pl.zone)].dir != pl.dir)
      {
        mFnPrev = in;
        return true;
      }
    }
  // The studio's monitor on the route: up into it.
  if (studio && p.state == PlayerState::OnGround)
    for (int i = 0; i < int(f.route.size()); ++i)
    {
      const auto& st = f.route[std::size_t(i)];
      if (st.act != "hop" || w.finaleStepDone(i))
        continue;
      for (const auto& po : f.portals)
      {
        const int x0 = po.bx * kCellsPerTile;
        if (p.y == (po.by + 2) * kCellsPerTile + 1 && p.x + 1 >= x0 && p.x + 1 <= x0 + 3 && std::abs(po.bx * kCellsPerTile - st.x) <= 4)
        {
          in.up = !mFnPrev.up;
          mFnPrev = in;
          return true;
        }
      }
    }
  // An Ad Break, a sample bot or a Stunt Double level with the gun: shoot it.
  if (p.state == PlayerState::OnGround && p.grav == Grav::Down)
  {
    // A sample bot underfoot can't be shot: back off first (they walk at
    // half speed), or jump over it when a wall is behind.
    const int mid = p.x + 1;
    for (const auto& e : w.enemies())
    {
      if (!e.alive || e.hidden || e.kind != EnemyKind::SampleBot || std::abs(e.y - p.y) > 1)
        continue;
      const int dx = mid - (e.x + e.w / 2);
      const int away = dx != 0 ? sgn(dx) : 1;
      // Keep going until there is room to turn round and fire.
      if (std::abs(dx) >= 3 && !(std::abs(dx) < 6 && p.facing == away))
        continue;
      const int ahead = away > 0 ? p.x + Player::kWidth : p.x - 1;
      const bool wall = w.map().solid(ahead, p.y) || w.map().solid(ahead + away, p.y);
      const int dir = wall ? -away : away;
      (dir > 0 ? in.right : in.left) = true;
      in.jump = wall && !mFnPrev.jump;
      mFnPrev = in;
      return true;
    }
    int face = 0;
    for (const auto& e : w.enemies())
      if (face == 0 && e.alive && e.active && !e.hidden &&
          (e.kind == EnemyKind::AdBreak || e.kind == EnemyKind::SampleBot || e.kind == EnemyKind::StuntDouble))
        face = aimAt(w, e.box(), e.kind == EnemyKind::SampleBot ? 16 : 24);
    if (face != 0)
    {
      if (p.facing != face)
        (face > 0 ? in.right : in.left) = true;
      else
        in.fire = !mFnPrev.fire;
      mFnPrev = in;
      return true;
    }
  }
  // Off the Air: on the console, break the glass cover.
  const auto& b = f.boss;
  if (b.on && b.phase == FnPhase::OffAir && b.coverHp > 0 && p.state == PlayerState::OnGround)
  {
    const CellBox cover = finaleCoverBox(w);
    const int gun = p.y - 2;
    if (gun >= cover.top() && gun <= cover.bottom() && cover.x - (p.x + Player::kWidth) >= 0 &&
        cover.x - (p.x + Player::kWidth) <= 10)
    {
      if (p.facing != 1)
        in.right = true;
      else
        in.fire = !mFnPrev.fire;
      mFnPrev = in;
      return true;
    }
  }
  mFnPrev = Input{};
  return false;
}

} // namespace gr
