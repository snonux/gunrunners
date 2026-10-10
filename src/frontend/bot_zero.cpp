// The bot against ZERO (Level 21, world_zero.cpp): the same short
// look-ahead as Kaan-Tolok's fighter in bot.cpp. Each four-frame move is
// tried in copies of the world, followed by a few simple policies for 48
// frames, and scored on the damage the eye and the Echoes take against the
// hearts lost; in Overload, ending up off the lit segments costs too.

#include "frontend/bot.hpp"

#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

namespace gr
{

namespace
{

PlayerInput simInput(const Input& in, const Input& prev)
{
  PlayerInput p;
  p.left = in.left;
  p.right = in.right;
  p.up = in.up;
  p.down = in.down;
  p.jump.pressed = in.jump;
  p.jump.triggered = in.jump && !prev.jump;
  p.fire.pressed = in.fire;
  p.fire.triggered = in.fire && !prev.fire;
  return p;
}

// Moves of four frames; fireAt is the frame the trigger goes down (-1: not
// at all).
struct ZeroMove
{
  int dir;
  bool up, down, jump;
  int fireAt;
};
const ZeroMove kZeroMoves[] = {
  {0, false, false, false, -1}, // wait
  {-1, false, false, false, -1},
  {1, false, false, false, -1},
  {0, false, false, true, -1},
  {-1, false, false, true, -1},
  {1, false, false, true, -1},
  {0, true, false, false, 0},   // look up and shoot
  {0, true, false, false, 2},
  {0, false, false, false, 0},  // shoot ahead
  {0, false, true, false, 0},   // crouch and shoot
  {-1, false, false, false, 0}, // turn left and shoot
  {1, false, false, false, 0},
  {-1, false, false, true, 3},  // jump left, shoot
  {1, false, false, true, 3},
};
constexpr int kMoveCount = int(sizeof(kZeroMoves) / sizeof(kZeroMoves[0]));

Input zeroInput(const ZeroMove& m, int f)
{
  Input in;
  in.left = m.dir < 0;
  in.right = m.dir > 0;
  in.up = m.up;
  in.down = m.down;
  in.jump = m.jump;
  in.fire = m.fireAt == f;
  return in;
}

int echoHp(const World& w)
{
  int hp = 0;
  for (const auto& e : w.enemies())
    if (e.alive && e.kind == EnemyKind::Echo)
      hp += e.hp;
  return hp;
}

int turretHp(const World& w)
{
  int hp = 0;
  for (int i : w.zero().boss.turrets)
    if (w.enemies()[std::size_t(i)].alive)
      hp += w.enemies()[std::size_t(i)].hp;
  return hp;
}

// The nearest live Echo, or null.
const Enemy* nearestEcho(const World& w)
{
  const auto& p = w.player();
  const Enemy* best = nullptr;
  for (const auto& e : w.enemies())
    if (e.alive && e.kind == EnemyKind::Echo && (!best || std::abs(e.x - p.x) < std::abs(best->x - p.x)))
      best = &e;
  return best;
}

// The middle (cells) of the nearest safe segment in Overload, else the eye's.
float standX(const World& w)
{
  const auto& b = w.zero().boss;
  const float px = float(w.player().x) + 1.5f;
  if (b.phase != ZeroPhase::Overload)
    return b.cx;
  float best = b.cx, bd = 1e9f;
  for (int k = 0; k < kZeroSegments; ++k)
    if ((b.safe >> k) & 1u)
    {
      const float x = float((b.ax0 + 3 * k) * kCellsPerTile) + 3.0f;
      // Under the eye is better while it is open.
      const float d = std::abs(x - px) + 0.3f * std::abs(x - b.cx);
      if (d < bd)
      {
        bd = d;
        best = x;
      }
    }
  return best;
}

// A shot of ZERO's or a turret's about to land on the runner.
bool shotNear(const World& w)
{
  const auto& p = w.player();
  const CellBox near{p.x - 3, p.y - 8, Player::kWidth + 6, 11};
  for (const auto& pr : w.projectiles())
    if (pr.alive && pr.kind == ShotKind::Enemy && pr.box().intersects(near))
      return true;
  return false;
}

} // namespace

Input Bot::fightZero(const World& world)
{
  if (mFightQueue.empty())
  {
    constexpr int kAhead = 48;
    const int hp0 = world.player().hp, zero0 = world.zeroHp(), echo0 = echoHp(world), turret0 = turretHp(world);
    const int pops0 = world.lavaPops();
    int best = 0;
    long bestScore = std::numeric_limits<long>::min();
    for (int m = 0; m < kMoveCount; ++m)
    {
      const auto firstPtr = world.cloneForSim();
      World& first = *firstPtr;
      Input prev = mFightPrev;
      bool dead = false;
      for (int f = 0; f < 4 && !dead; ++f)
      {
        const Input in = zeroInput(kZeroMoves[m], f);
        first.update(simInput(in, prev));
        prev = in;
        dead = first.player().state == PlayerState::Dying;
      }
      const long firstScore = 200L * (zero0 - first.zeroHp()) + 60L * (echo0 - echoHp(first));
      long moveScore = std::numeric_limits<long>::min();
      for (int follow = 0; follow < 7 && !dead; ++follow)
      {
        World sim(first);
        Input fp = prev;
        long score = firstScore;
        int zeroHp = sim.zeroHp(), echoes = echoHp(sim), turrets = turretHp(sim);
        for (int f = 0; f < kAhead; ++f)
        {
          Input in;
          const auto& sp = sim.player();
          const auto& b = sim.zero().boss;
          const float pcx = float(sp.x) + 1.5f;
          const bool ground = sp.state == PlayerState::OnGround;
          const float to = standX(sim);
          auto walkTo = [&](float x) {
            in.left = x < pcx - 1.0f;
            in.right = x > pcx + 1.0f;
          };
          const Enemy* echo = nearestEcho(sim);
          switch (follow)
          {
            case 0: // under the eye (or the nearest lit segment), shooting up
            case 1: // the same, hopping out of the way of shots
              walkTo(to);
              if (!in.left && !in.right)
              {
                in.up = true;
                in.fire = f % 2 == 1;
              }
              if (follow == 1)
                in.jump = ground && shotNear(sim);
              break;
            case 2: // hunt the nearest Echo
            case 3: // the same, jumping its shots
              if (echo)
              {
                const int dir = echo->x + 1 < sp.x + 1 ? -1 : 1;
                in.left = dir < 0 && (sp.facing > 0 || std::abs(echo->x - sp.x) > 12);
                in.right = dir > 0 && (sp.facing < 0 || std::abs(echo->x - sp.x) > 12);
                in.fire = f % 2 == 1;
                in.jump = follow == 3 && ground && shotNear(sim);
              }
              else
              {
                walkTo(to);
                in.up = !in.left && !in.right;
                in.fire = f % 2 == 1;
              }
              break;
            case 4: // stand and shoot up
              in.up = true;
              in.fire = f % 2 == 1;
              break;
            case 5: // to the lit segment, then wait
              walkTo(to);
              break;
            case 6: // shoot at a turret overhead (they hang low enough)
              in.up = true;
              in.fire = f % 2 == 1;
              in.jump = ground && shotNear(sim);
              break;
          }
          (void)b;
          sim.update(simInput(in, fp));
          fp = in;
          score += long(zeroHp - sim.zeroHp()) * (200 - 3 * f);
          zeroHp = sim.zeroHp();
          score += long(echoes - echoHp(sim)) * (60 - f);
          echoes = echoHp(sim);
          score += long(turrets - turretHp(sim)) * 15;
          turrets = turretHp(sim);
          if (sim.player().state == PlayerState::Dying || sim.state() != WorldState::Playing)
            break;
        }
        const auto& sp = sim.player();
        const long lost = std::max(0, hp0 - sp.hp);
        score -= lost * (400L + 1200L / std::max(1, sp.hp));
        score -= 900L * (sim.lavaPops() - pops0);
        const auto& b = sim.zero().boss;
        if (b.phase == ZeroPhase::Overload)
        {
          // Off the lit segments when they drop costs.
          if (!sim.overSafeSegment())
            score -= b.cycle >= 30 && b.cycle < 62 ? 600L : 150L;
        }
        else
          score -= long(std::abs(float(sp.x) + 1.5f - b.cx) * 2.0f);
        if (sp.state == PlayerState::Dying)
          score -= 1000000L;
        moveScore = std::max(moveScore, score);
      }
      if (dead)
        continue;
      if (moveScore > bestScore)
      {
        bestScore = moveScore;
        best = m;
      }
    }
    static const bool debug = std::getenv("GR_FIGHT_DEBUG") != nullptr;
    if (debug)
      std::fprintf(stderr, "zero f%d phase %d hp %d at %d,%d runner hp %d -> move %d (%ld)\n", world.stats().frames,
        int(world.zero().boss.phase), world.zeroHp(), world.player().x, world.player().y, world.player().hp, best,
        bestScore);
    for (int f = 0; f < 4; ++f)
      mFightQueue.push_back(zeroInput(kZeroMoves[best], f));
  }
  Input in = mFightQueue.front();
  mFightQueue.pop_front();
  mFightPrev = in;
  return in;
}

} // namespace gr
