// The bot in Level 28, Toon Town Throwdown: Buddy Boom and the Inkwell
// bonus.
//
// Buddy Boom is fought like Gorgazilla: short moves tried in copies of the
// world, each followed by a few simple policies, scored on his hit points
// against the runner's hearts. In the Inkwell each short move (walk, jump,
// draw level, up or down) is tried two deep in copies of the world, then
// the runner is let fall to see where they end up; the score is gems
// taken, then how close they land to the next gem, then the ink left.

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

struct BuddyMove
{
  int dir;
  bool up, down, jump;
  int fireAt; // -1 never, 9 held, else the frame of a tap (and two later)
};
const BuddyMove kBuddyMoves[] = {
  {0, false, false, false, -1}, // wait
  {-1, false, false, false, -1},
  {1, false, false, false, -1},
  {0, false, false, true, -1},
  {-1, false, false, true, -1},
  {1, false, false, true, -1},
  {0, false, false, false, 0}, // shoot ahead
  {0, false, true, false, 0},  // crouch and shoot
  {0, false, false, false, 1},
  {-1, false, false, false, 0}, // turn and shoot
  {1, false, false, false, 0},
  {0, true, false, false, 0}, // shoot up
  {0, true, false, false, 1},
  {-1, false, false, true, 1}, // jump and shoot
  {1, false, false, true, 1},
  {0, false, false, true, 2},
};
constexpr int kBuddyMoveCount = int(sizeof(kBuddyMoves) / sizeof(kBuddyMoves[0]));

Input buddyInput(const BuddyMove& m, int f)
{
  Input in;
  in.left = m.dir < 0;
  in.right = m.dir > 0;
  in.up = m.up;
  in.down = m.down;
  in.jump = m.jump;
  in.fire = m.fireAt == 9 || m.fireAt == f || (m.fireAt >= 0 && f == m.fireAt + 2);
  return in;
}

// Something of his about to land on the runner.
bool buddyDanger(const World& w)
{
  const auto& b = w.toon().buddy;
  const auto& p = w.player();
  const CellBox near{p.x - 6, p.y - 8, Player::kWidth + 12, 10};
  for (const auto& wave : b.waves)
    if (wave.y >= p.y - 1 && wave.y <= p.y + 1 && std::abs(wave.x - float(p.x)) < 10.0f &&
        (wave.x - float(p.x)) * float(wave.dir) < 2.0f)
      return true;
  for (const auto& pie : b.pies)
    if (near.contains(int(pie.x), int(pie.y)))
      return true;
  if (b.state == BuddyState::Erase && b.t >= kEraseTell - 6 && b.eraseSeg >= 0 && w.segmentUnder(p.x + 1) == b.eraseSeg)
    return true;
  return false;
}

// Where to stand: phase 1 just outside his reach, phase 2 under his
// platform's edge, phase 3 under his drawing.
float buddyStand(const World& w)
{
  const auto& b = w.toon().buddy;
  const float bc = b.x + Buddy::kW * 0.5f;
  const float px = float(w.player().x) + 1.5f;
  const float lo = float((b.x0 + 1) * kCellsPerTile + 2), hi = float(b.x1 * kCellsPerTile - 2);
  if (b.phase == 3 && b.lineY >= 0)
    return std::clamp(bc + (px < bc ? -3.0f : 3.0f), lo, hi);
  const float off = b.phase == 1 ? 15.0f : 12.0f;
  float best = std::clamp(px < bc ? bc - off : bc + off, lo, hi);
  if (std::abs(best - bc) < off - 2.0f)
    best = std::clamp(px < bc ? bc + off : bc - off, lo, hi);
  return best;
}

} // namespace

Input Bot::fightBuddy(const World& world)
{
  const auto& p = world.player();
  if (mFightQueue.empty())
  {
    constexpr int kAhead = 40;
    const int hp0 = p.hp, buddy0 = world.buddyHp();
    int best = 0;
    long bestScore = std::numeric_limits<long>::min();
    for (int m = 0; m < kBuddyMoveCount; ++m)
    {
      const auto firstPtr = world.cloneForSim();
      World& first = *firstPtr;
      Input prev = mFightPrev;
      bool dead = false;
      for (int f = 0; f < 4 && !dead; ++f)
      {
        const Input in = buddyInput(kBuddyMoves[m], f);
        first.update(simInput(in, prev));
        prev = in;
        dead = first.player().state == PlayerState::Dying;
      }
      if (dead)
        continue;
      const long firstScore = 200L * (buddy0 - first.buddyHp());
      long moveScore = std::numeric_limits<long>::min();
      for (int follow = 0; follow < 6; ++follow)
      {
        World sim(first);
        Input fp = prev;
        long score = firstScore;
        int hpB = sim.buddyHp();
        for (int f = 0; f < kAhead; ++f)
        {
          Input in;
          const auto& sp = sim.player();
          const auto& b = sim.toon().buddy;
          const float pcx = float(sp.x) + 1.5f, bc = b.x + Buddy::kW * 0.5f;
          const bool ground = sp.state == PlayerState::OnGround;
          const int face = bc < pcx ? -1 : 1;
          const float to = buddyStand(sim);
          const bool there = std::abs(to - pcx) <= 1.0f;
          const bool trigger = f % 2 == 1;
          const bool danger = ground && buddyDanger(sim) && !fp.jump;
          switch (follow)
          {
            case 0: // to the spot, shoot at him, jump what comes
            case 1: // the same, crouched
              if (!there)
              {
                in.left = to < pcx;
                in.right = to > pcx;
              }
              else if (sp.facing != face)
                (face < 0 ? in.left : in.right) = true;
              else
              {
                in.down = follow == 1;
                in.fire = trigger;
              }
              in.jump = danger;
              break;
            case 2: // stay put, shooting at him
              if (sp.facing != face)
                (face < 0 ? in.left : in.right) = true;
              in.fire = trigger;
              in.jump = danger;
              break;
            case 3: // under him, shooting up
              if (!there)
              {
                in.left = to < pcx;
                in.right = to > pcx;
              }
              else
              {
                in.up = true;
                in.fire = trigger;
              }
              in.jump = danger;
              break;
            case 4: // back off from him
              (face < 0 ? in.right : in.left) = true;
              in.jump = danger;
              break;
            case 5: // jump-shoot at him
              if (sp.facing != face)
                (face < 0 ? in.left : in.right) = true;
              in.jump = ground && !fp.jump;
              in.fire = trigger;
              break;
          }
          sim.update(simInput(in, fp));
          fp = in;
          score += long(hpB - sim.buddyHp()) * (200 - 3 * f);
          hpB = sim.buddyHp();
          if (sim.player().state == PlayerState::Dying || sim.state() != WorldState::Playing)
            break;
        }
        const auto& sp = sim.player();
        const long lost = std::max(0, hp0 - sp.hp);
        score -= lost * (500L + 1500L / std::max(1, sp.hp));
        score -= long(std::abs(float(sp.x) + 1.5f - buddyStand(sim)) * 3.0f);
        if (sp.state == PlayerState::Dying)
          score -= 1000000L;
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
    {
      const auto& b = world.toon().buddy;
      std::fprintf(stderr, "buddy f%d state %d t %d cool %d phase %d hp %d nose %d at %.1f,%.1f; runner %d,%d hp %d -> move %d (%ld)\n",
        world.stats().frames, int(b.state), b.t, b.cool, b.phase, world.buddyHp(), b.nose, b.x, b.y, p.x, p.y, p.hp,
        best, bestScore);
    }
    for (int f = 0; f < 4; ++f)
      mFightQueue.push_back(buddyInput(kBuddyMoves[best], f));
  }
  Input in = mFightQueue.front();
  mFightQueue.pop_front();
  mFightPrev = in;
  return in;
}

namespace
{

constexpr int kInkStep = 6;

struct InkMove
{
  int dir;
  bool jump;
  int fire; // 0 no, 1 level, 2 up, 3 down
};

Input inkInput(const InkMove& m, int f)
{
  Input in;
  in.left = m.dir < 0;
  in.right = m.dir > 0;
  in.jump = m.jump && f < 4;
  in.fire = m.fire > 0;
  in.up = m.fire == 2;
  in.down = m.fire == 3;
  return in;
}

// The next gem: the nearest, counting a climb double.
const Item* inkTarget(const World& w)
{
  const auto& p = w.player();
  const Item* best = nullptr;
  int bestD = 1 << 30;
  for (const auto& it : w.items())
  {
    if (it.taken || it.kind != ItemKind::Gem)
      continue;
    const int d = std::abs(it.x - p.x) + 2 * std::max(0, p.y - it.y) + std::max(0, it.y - p.y);
    if (d < bestD)
    {
      bestD = d;
      best = &it;
    }
  }
  return best;
}

} // namespace

Input Bot::inkwell(const World& world)
{
  if (!mInkQueue.empty())
  {
    const Input in = mInkQueue.front();
    mInkQueue.pop_front();
    mInkPrev = in;
    return in;
  }
  std::vector<InkMove> moves;
  for (int dir = -1; dir <= 1; ++dir)
    for (int jump = 0; jump < 2; ++jump)
      for (int fire = 0; fire < 4; ++fire)
        moves.push_back({dir, jump != 0, fire});
  const Item* target = inkTarget(world);
  const int tx = target ? target->x : world.player().x, ty = target ? target->y : world.player().y;
  const int gems0 = world.stats().gems;
  const int start = world.ink().startX;
  auto run = [&](World& sim, const InkMove& m, Input& prev) {
    for (int f = 0; f < kInkStep && sim.state() == WorldState::Playing; ++f)
    {
      const Input in = inkInput(m, f);
      sim.update(simInput(in, prev));
      prev = in;
    }
  };
  auto evaluate = [&](World& sim, Input prev) {
    // Let go and see where the runner ends up.
    for (int f = 0; f < 24 && sim.state() == WorldState::Playing && sim.player().state != PlayerState::OnGround; ++f)
    {
      const Input in;
      sim.update(simInput(in, prev));
      prev = in;
    }
    const auto& sp = sim.player();
    double score = double(sim.stats().gems - gems0) * 10000.0;
    if (sim.state() != WorldState::Playing)
      score += 1e6;
    if (sp.state != PlayerState::OnGround)
      score -= 3000.0; // still falling: off the page, most likely
    if (sp.x == start && sp.y == world.ink().startY && (world.player().x != start || world.player().y != world.ink().startY))
      score -= 5000.0;  // back at the start
    const Item* t = inkTarget(sim);
    const int gx = t ? t->x : tx, gy = t ? t->y : ty;
    score -= double(std::abs(gx - sp.x)) * 10.0 + double(std::max(0, sp.y - gy)) * 25.0 +
      double(std::max(0, gy - sp.y)) * 5.0;
    score += double(sim.ink().ink) * 1.0;
    return score;
  };
  double bestScore = -1e18;
  int best = 0;
  for (std::size_t a = 0; a < moves.size(); ++a)
  {
    const auto firstPtr = world.cloneForSim();
    Input prevA = mInkPrev;
    run(*firstPtr, moves[a], prevA);
    double moveScore = -1e18;
    for (std::size_t b = 0; b < moves.size(); ++b)
    {
      World sim(*firstPtr);
      Input prevB = prevA;
      run(sim, moves[b], prevB);
      moveScore = std::max(moveScore, evaluate(sim, prevB));
    }
    if (moveScore > bestScore)
    {
      bestScore = moveScore;
      best = int(a);
    }
  }
  static const bool debug = std::getenv("GR_FIGHT_DEBUG") != nullptr;
  if (debug)
    std::fprintf(stderr, "ink f%d at %d,%d ink %d gems %d target %d,%d -> dir %d jump %d fire %d (%.0f)\n",
      world.stats().frames, world.player().x, world.player().y, world.ink().ink, gems0, tx, ty,
      moves[std::size_t(best)].dir, int(moves[std::size_t(best)].jump), moves[std::size_t(best)].fire, bestScore);
  for (int f = 0; f < kInkStep; ++f)
    mInkQueue.push_back(inkInput(moves[std::size_t(best)], f));
  const Input in = mInkQueue.front();
  mInkQueue.pop_front();
  mInkPrev = in;
  return in;
}

} // namespace gr
