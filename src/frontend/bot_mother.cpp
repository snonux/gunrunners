// The bot against the Hive Mother (Level 49, world_mother.cpp): the same
// short look-ahead as Kaan-Tolok's fighter in bot.cpp. Each four-frame move
// is tried in copies of the world, followed by a few simple policies for
// 48 frames, and scored on the damage she takes against the hearts lost.

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
// at all), so a jump can shoot from its top.
struct MotherMove
{
  int dir;
  bool down, jump;
  int fireAt;
};
const MotherMove kMoves[] = {
  {0, false, false, -1}, // wait
  {-1, false, false, -1},
  {1, false, false, -1},
  {0, false, true, -1},
  {-1, false, true, -1},
  {1, false, true, -1},
  {0, false, false, 0},  // shoot
  {0, false, true, 3},   // jump, shoot from the top
  {-1, false, false, 0}, // turn left and shoot
  {1, false, false, 0},
  {-1, false, true, 3},  // jump left, shoot
  {1, false, true, 3},
};

Input moveInput(const MotherMove& m, int f)
{
  Input in;
  in.left = m.dir < 0;
  in.right = m.dir > 0;
  in.down = m.down;
  in.jump = m.jump;
  in.fire = m.fireAt == f;
  return in;
}

// Her brood alive: Spore Nurses count for more (they heal her).
int broodScore(const World& w)
{
  int s = 0;
  for (const auto& e : w.enemies())
    if (e.alive && (e.kind == EnemyKind::SporeNurse || e.kind == EnemyKind::EggGuard))
      s += e.kind == EnemyKind::SporeNurse ? 3 : 1;
  return s;
}

// The nearest of her brood (hatched guards and nurses), or null.
const Enemy* nearestBrood(const World& w)
{
  const auto& p = w.player();
  const Enemy* best = nullptr;
  int bestD = 0;
  for (const auto& e : w.enemies())
  {
    if (!e.alive || (e.kind != EnemyKind::SporeNurse && e.kind != EnemyKind::EggGuard))
      continue;
    const int d = std::abs(e.x + e.w / 2 - (p.x + 1)) + std::abs(e.y - p.y);
    if (!best || d < bestD)
    {
      best = &e;
      bestD = d;
    }
  }
  return best;
}

} // namespace

Input Bot::fightMother(const World& world)
{
  if (mFightQueue.empty())
  {
    constexpr int kAhead = 48;
    const int hp0 = world.player().hp, mother0 = world.motherHp(), brood0 = broodScore(world);
    int best = 0;
    long bestScore = std::numeric_limits<long>::min();
    // The best plan's first frames after the move: queued too, so the
    // policy that earned the score gets played (a fresh search every four
    // frames can put the same good plan off for ever).
    constexpr int kKeep = 8;
    std::vector<Input> bestTrace;
    for (int m = 0; m < int(sizeof(kMoves) / sizeof(kMoves[0])); ++m)
    {
      const auto firstPtr = world.cloneForSim();
      World& first = *firstPtr;
      Input prev = mFightPrev;
      bool dead = false;
      for (int f = 0; f < 4 && !dead; ++f)
      {
        const Input in = moveInput(kMoves[m], f);
        first.update(simInput(in, prev));
        prev = in;
        dead = first.player().state == PlayerState::Dying;
      }
      const long firstScore = 200L * (mother0 - first.motherHp());
      long moveScore = std::numeric_limits<long>::min();
      std::vector<Input> moveTrace;
      for (int follow = 0; follow < 9 && !dead; ++follow)
      {
        World sim(first);
        Input fp = prev;
        long score = firstScore;
        std::vector<Input> trace;
        int motherHp = sim.motherHp(), brood = broodScore(sim);
        score += long(brood0 - brood) * 150L;
        bool down = false;
        for (int f = 0; f < kAhead; ++f)
        {
          Input in;
          const auto& sp = sim.player();
          const auto& mo = sim.mother();
          const float mcx = mo.x + float(HiveMother::kW) * 0.5f, pcx = float(sp.x) + 1.5f;
          const int toHer = mcx < pcx ? -1 : 1;
          const bool ground = sp.state == PlayerState::OnGround;
          auto face = [&](int dir) {
            in.left = dir < 0 && sp.facing > 0;
            in.right = dir > 0 && sp.facing < 0;
          };
          switch (follow)
          {
            case 0: // duel: face her, jump-shoot when her crown is low
            case 1: // back off first
            case 2: // close in first
              if (follow == 1 && f < 12)
              {
                in.left = toHer > 0;
                in.right = toHer < 0;
              }
              else if (follow == 2 && f < 8)
              {
                in.left = toHer < 0;
                in.right = toHer > 0;
              }
              else
                face(toHer);
              in.jump = ground && mo.bowed() && f % 8 == 0;
              in.fire = f % 2 == 1;
              break;
            case 3: // hop in place (onto a ledge overhead), shooting
              in.jump = ground && f % 6 == 0;
              face(toHer);
              in.fire = f % 2 == 1;
              break;
            case 4: // hop away from her
            case 5: // hop toward her
              in.left = (follow == 4) == (toHer > 0);
              in.right = !in.left;
              in.jump = ground && f % 6 == 0;
              in.fire = f % 2 == 1;
              break;
            case 6: // shoot the nearest of her brood
              if (const Enemy* e = nearestBrood(sim))
              {
                face(e->x + e->w / 2 < sp.x + 1 ? -1 : 1);
                in.jump = ground && e->y < sp.y - 3 && f % 6 == 0;
              }
              else
                face(toHer);
              in.fire = f % 2 == 1;
              break;
            case 7: // stand and shoot
              face(toHer);
              in.fire = f % 2 == 1;
              break;
            case 8: // lean away from her pull, turning to shoot
              if (f % 4 < 2)
              {
                in.left = toHer > 0;
                in.right = toHer < 0;
              }
              else
              {
                face(toHer);
                in.fire = f % 4 == 3;
              }
              break;
          }
          sim.update(simInput(in, fp));
          fp = in;
          if (f < kKeep)
            trace.push_back(in);
          // Sooner is better, or the search waits for ever.
          score += long(motherHp - sim.motherHp()) * (180 - 3 * f);
          motherHp = sim.motherHp();
          const int nowBrood = broodScore(sim);
          score += long(brood - nowBrood) * (150 - 2 * f);
          brood = nowBrood;
          if (!down && (sim.mother().phase == MotherPhase::Dying || sim.mother().phase == MotherPhase::Done))
          {
            down = true;
            score += 100000L - 1000L * f;
          }
          if (sim.player().state == PlayerState::Dying || sim.state() != WorldState::Playing)
            break;
        }
        const auto& sp = sim.player();
        const long lost = std::max(0, hp0 - sp.hp);
        score += -lost * (400L + 1200L / std::max(1, sp.hp));
        // Between her attacks, wait a little way off her front, on the
        // floor, where her crown and sacs are in reach.
        const auto& mo = sim.mother();
        if (mo.phase == MotherPhase::Inhale || mo.phase == MotherPhase::Crown)
        {
          const float gap =
            std::abs(float(sp.x) + 1.5f - (mo.x + float(HiveMother::kW) * 0.5f)) - float(HiveMother::kW) * 0.5f;
          score -= long(std::abs(gap - 8.0f) * 3.0f);
        }
        if (sp.state == PlayerState::Dying)
          score -= 1000000L;
        if (score > moveScore)
        {
          moveScore = score;
          moveTrace = std::move(trace);
        }
      }
      if (dead)
        continue;
      if (moveScore > bestScore)
      {
        bestScore = moveScore;
        best = m;
        bestTrace = std::move(moveTrace);
      }
    }
    static const bool debug = std::getenv("GR_FIGHT_DEBUG") != nullptr;
    if (debug)
      std::fprintf(stderr, "mother f%d phase %d hp %d at %d,%d runner hp %d -> move %d (%ld) cycle %d x %.0f face %d proto %d ammo %d state %d\n",
        world.stats().frames, int(world.mother().phase), world.motherHp(), world.player().x, world.player().y,
        world.player().hp, best, bestScore, world.mother().cycle, double(world.mother().x), world.player().facing,
        world.player().proto, world.player().ammo, int(world.player().state));
    for (int f = 0; f < 4; ++f)
      mFightQueue.push_back(moveInput(kMoves[best], f));
    for (const Input& in : bestTrace)
      mFightQueue.push_back(in);
  }
  Input in = mFightQueue.front();
  mFightQueue.pop_front();
  mFightPrev = in;
  return in;
}

} // namespace gr
