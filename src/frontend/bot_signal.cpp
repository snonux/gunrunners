// The bot in Level 29, Dead Air: the Test Card and the Wraparound bonus.
//
// Fought like Buddy Boom: short moves tried in copies of the world, each
// followed by a few simple policies, scored on the card's quadrants against
// the runner's hearts. Only the quadrant in the signal's colour takes
// damage, and the timed dials change the signal, so waiting out a dead
// colour scores nothing until the next one comes round.
//
// In the Wraparound each short move (walk, jump, shoot) is tried two deep
// in copies of the world, then the runner is let fall. Where they end up is
// scored by a search over the blocks the runner can stand on, edges joined:
// walk, step off and fall (through the bottom and in at the top), or jump a
// gap or up to three blocks; the cost is how long it takes to the nearest
// gem. Swarms close by cost too, and a touch (back home) costs a lot.

#include "frontend/bot.hpp"
#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <queue>
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

struct CardMove
{
  int dir;
  bool up, down, jump;
  int fireAt; // -1 never, else the frame of a tap (and two later)
};
const CardMove kCardMoves[] = {
  {0, false, false, false, -1}, // wait
  {-1, false, false, false, -1},
  {1, false, false, false, -1},
  {0, false, false, true, -1},
  {-1, false, false, true, -1},
  {1, false, false, true, -1},
  {0, false, false, false, 0}, // shoot ahead
  {0, false, true, false, 0},  // crouch and shoot
  {-1, false, false, false, 0}, // turn and shoot
  {1, false, false, false, 0},
  {0, true, false, false, 0}, // shoot up (the Tri-Tone turns its tint)
  {-1, false, false, true, 1}, // jump and shoot
  {1, false, false, true, 1},
  {0, false, false, true, 2},
};
constexpr int kCardMoveCount = int(sizeof(kCardMoves) / sizeof(kCardMoves[0]));

Input cardInput(const CardMove& m, int f)
{
  Input in;
  in.left = m.dir < 0;
  in.right = m.dir > 0;
  in.up = m.up;
  in.down = m.down;
  in.jump = m.jump;
  in.fire = m.fireAt == f || (m.fireAt >= 0 && f == m.fireAt + 2);
  return in;
}

int cardHp(const World& w)
{
  const auto& s = w.signal();
  return s.cardHp[0] + s.cardHp[1] + s.cardHp[2];
}

CellBox cardBox(const World& w)
{
  return w.enemies()[std::size_t(w.signal().card)].box();
}

// Where to stand: on the floor a few cells off the card's side the runner
// is on, in reach of a jump shot.
float cardStand(const World& w)
{
  const CellBox b = cardBox(w);
  const float px = float(w.player().x) + 1.5f;
  const float bc = float(b.x) + float(b.w) * 0.5f;
  return px < bc ? float(b.left()) - 5.0f : float(b.right()) + 6.0f;
}

} // namespace

Input Bot::fightCard(const World& world)
{
  const auto& p = world.player();
  if (mFightQueue.empty())
  {
    constexpr int kAhead = 40;
    const int hp0 = p.hp, card0 = cardHp(world);
    int best = 0;
    long bestScore = std::numeric_limits<long>::min();
    for (int m = 0; m < kCardMoveCount; ++m)
    {
      const auto firstPtr = world.cloneForSim();
      World& first = *firstPtr;
      Input prev = mFightPrev;
      bool dead = false;
      for (int f = 0; f < 4 && !dead; ++f)
      {
        const Input in = cardInput(kCardMoves[m], f);
        first.update(simInput(in, prev));
        prev = in;
        dead = first.player().state == PlayerState::Dying;
      }
      if (dead)
        continue;
      const long firstScore = 200L * (card0 - cardHp(first));
      long moveScore = std::numeric_limits<long>::min();
      for (int follow = 0; follow < 6; ++follow)
      {
        World sim(first);
        Input fp = prev;
        long score = firstScore;
        int hpC = cardHp(sim);
        for (int f = 0; f < kAhead && sim.cardFight(); ++f)
        {
          Input in;
          const auto& sp = sim.player();
          const CellBox b = cardBox(sim);
          const float pcx = float(sp.x) + 1.5f, bc = float(b.x) + float(b.w) * 0.5f;
          const bool ground = sp.state == PlayerState::OnGround;
          const int face = bc < pcx ? -1 : 1;
          const float to = cardStand(sim);
          const bool there = std::abs(to - pcx) <= 1.0f;
          const bool trigger = f % 2 == 1;
          const bool turned = sp.facing == face;
          switch (follow)
          {
            case 0: // to the spot, jump-shoot at it
              if (!there)
              {
                in.left = to < pcx;
                in.right = to > pcx;
              }
              else if (!turned)
                (face < 0 ? in.left : in.right) = true;
              else
              {
                in.jump = ground && !fp.jump;
                in.fire = trigger;
              }
              break;
            case 1: // stay, jump-shoot at it
              if (!turned)
                (face < 0 ? in.left : in.right) = true;
              in.jump = ground && !fp.jump;
              in.fire = trigger;
              break;
            case 2: // stay, shoot at it level
            case 3: // the same, crouched
              if (!turned)
                (face < 0 ? in.left : in.right) = true;
              else
              {
                in.down = follow == 3;
                in.fire = trigger;
              }
              break;
            case 4: // under it, shooting up
              if (std::abs(bc - pcx) > 1.0f)
              {
                in.left = bc < pcx;
                in.right = bc > pcx;
              }
              else
              {
                in.up = true;
                in.fire = trigger;
              }
              break;
            case 5: // back off from it
              (face < 0 ? in.right : in.left) = true;
              in.jump = ground && !fp.jump && sim.signal().beam.on;
              break;
          }
          sim.update(simInput(in, fp));
          fp = in;
          score += long(hpC - cardHp(sim)) * (200 - 3 * f);
          hpC = cardHp(sim);
          if (sim.player().state == PlayerState::Dying || sim.state() != WorldState::Playing)
            break;
        }
        const auto& sp = sim.player();
        const long lost = std::max(0, hp0 - sp.hp);
        score -= lost * (500L + 1500L / std::max(1, sp.hp));
        if (sim.cardFight())
          score -= long(std::abs(float(sp.x) + 1.5f - cardStand(sim)) * 3.0f);
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
      const auto& s = world.signal();
      std::fprintf(stderr, "card f%d color %d pending %d hp %d/%d/%d beam %d/%d; runner %d,%d hp %d -> move %d (%ld)\n",
        world.stats().frames, s.color, s.pending, s.cardHp[0], s.cardHp[1], s.cardHp[2], int(s.beam.on),
        int(s.beam.burning), p.x, p.y, p.hp, best, bestScore);
    }
    for (int f = 0; f < 4; ++f)
      mFightQueue.push_back(cardInput(kCardMoves[best], f));
  }
  Input in = mFightQueue.front();
  mFightQueue.pop_front();
  mFightPrev = in;
  return in;
}

namespace
{

constexpr int kWrapStep = 6;

struct WrapMove
{
  int dir;
  bool jump, fire;
  bool nudge = false; // the direction for two frames only
};

Input wrapInput(const WrapMove& m, int f)
{
  Input in;
  in.left = m.dir < 0 && (!m.nudge || f < 2);
  in.right = m.dir > 0 && (!m.nudge || f < 2);
  in.jump = m.jump && f < 4;
  in.fire = m.fire && (f == 0 || f == 3);
  return in;
}

// Blocks, joined at the edges.
struct WrapGrid
{
  int w = 0, h = 0;
  std::vector<char> solid;
  std::vector<std::vector<int>> gems; // per block: indices into the gem list
  int left = 0;                       // gems not taken

  int at(int bx, int by) const { return ((by % h + h) % h) * w + ((bx % w) + w) % w; }
  bool wall(int bx, int by) const { return solid[std::size_t(at(bx, by))] != 0; }
  bool stand(int bx, int by) const { return !wall(bx, by) && !wall(bx, by - 1) && wall(bx, by + 1); }
};

WrapGrid wrapGrid(const World& w)
{
  WrapGrid g;
  const auto& m = w.map();
  g.w = m.widthBlocks();
  g.h = m.heightBlocks();
  g.solid.assign(std::size_t(g.w * g.h), 0);
  g.gems.assign(std::size_t(g.w * g.h), {});
  for (int by = 0; by < g.h; ++by)
    for (int bx = 0; bx < g.w; ++bx)
      g.solid[std::size_t(by * g.w + bx)] = m.solid(bx * kCellsPerTile, by * kCellsPerTile) ? 1 : 0;
  int n = 0;
  for (const auto& it : w.items())
  {
    if (it.taken || it.kind != ItemKind::Gem)
      continue;
    g.gems[std::size_t(g.at(it.x / kCellsPerTile, it.y / kCellsPerTile))].push_back(n++);
  }
  g.left = n;
  return g;
}

// A gem in reach standing at (bx, by): 0 none, 1 by walking, 2 by jumping.
int wrapGemAt(const WrapGrid& g, int bx, int by)
{
  int best = 0;
  for (int dx = -1; dx <= 1; ++dx)
    for (int dy = -4; dy <= 0; ++dy)
      if (!g.gems[std::size_t(g.at(bx + dx, by + dy))].empty())
      {
        if (dy >= -1)
          return 1;
        best = 2;
      }
  return best;
}

// Falling down column bx from row by: where the runner lands (-1: never),
// whether a gem is on the way, and how many rows it takes.
int wrapFall(const WrapGrid& g, int bx, int by, bool& gem, int& rows)
{
  gem = false;
  for (rows = 0; rows < g.h * 2; ++rows)
  {
    const int y = by + rows;
    for (int dx = 0; dx <= 1; ++dx)
      for (int dy = -1; dy <= 0; ++dy)
        gem = gem || !g.gems[std::size_t(g.at(bx + dx, y + dy))].empty();
    if (g.wall(bx, y + 1))
      return ((y % g.h) + g.h) % g.h;
  }
  return -1;
}

// Nothing in the way of a jump dx blocks along from (bx, by) (feet).
bool wrapClear(const WrapGrid& g, int bx, int by, int dir, int dx)
{
  for (int i = 1; i <= dx; ++i)
    for (int dy = -2; dy <= -1; ++dy)
      if (g.wall(bx + dir * i, by + dy))
        return false;
  return true;
}

// Frames (about) from standing at (bx, by) to the nearest gem.
int wrapCost(const WrapGrid& g, int bx, int by)
{
  if (g.left == 0)
    return 0;
  const int n = g.w * g.h;
  std::vector<int> dist(std::size_t(n), 1 << 29);
  using Node = std::pair<int, int>;
  std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;
  int best = 1 << 29;
  auto push = [&](int x, int y, int d) {
    const int k = g.at(x, y);
    if (d < dist[std::size_t(k)])
    {
      dist[std::size_t(k)] = d;
      open.push({d, k});
    }
  };
  bool gem = false;
  int rows = 0;
  if (!g.stand(bx, by))
  {
    const int y = wrapFall(g, bx, by, gem, rows);
    if (gem)
      return rows;
    if (y < 0)
      return 1 << 20;
    push(bx, y, rows);
  }
  else
    push(bx, by, 0);
  while (!open.empty())
  {
    const auto [d, k] = open.top();
    open.pop();
    if (d > dist[std::size_t(k)] || d >= best)
      continue;
    const int x = k % g.w, y = k / g.w;
    const int reach = wrapGemAt(g, x, y);
    if (reach > 0)
    {
      best = std::min(best, d + (reach == 2 ? 8 : 0));
      continue;
    }
    for (int dir = -1; dir <= 1; dir += 2)
    {
      if (g.wall(x + dir, y) || g.wall(x + dir, y - 1))
        continue;
      if (g.stand(x + dir, y))
        push(x + dir, y, d + 2);
      else if (!g.wall(x + 2 * dir, y) && !g.wall(x + 2 * dir, y - 1))
      {
        // Off the edge (into a gap the runner fits through).
        const int ly = wrapFall(g, x + dir, y, gem, rows);
        if (gem)
          best = std::min(best, d + 2 + rows);
        else if (ly >= 0)
          push(x + dir, ly, d + 2 + rows);
      }
      // Jumps: across a gap, or up to three blocks.
      for (int dx = 2; dx <= 4; ++dx)
        for (int dy = -3; dy <= 1; ++dy)
          if (g.stand(x + dir * dx, y + dy) && !g.wall(x, y - 2) && wrapClear(g, x, std::min(y, y + dy), dir, dx))
            push(x + dir * dx, y + dy, d + 8 + 2 * dx);
    }
    for (int dy = -3; dy <= -1; ++dy)
      for (int dx = -1; dx <= 1; dx += 2)
        if (g.stand(x + dx, y + dy) && !g.wall(x, y - 2) && !g.wall(x, y - 3))
          push(x + dx, y + dy, d + 10);
  }
  return best;
}

// Cells between two points on the torus.
int wrapDist(int a, int b, int size)
{
  const int d = std::abs(a - b) % size;
  return std::min(d, size - d);
}

} // namespace

Input Bot::wraparound(const World& world)
{
  if (!mInkQueue.empty())
  {
    const Input in = mInkQueue.front();
    mInkQueue.pop_front();
    mInkPrev = in;
    return in;
  }
  std::vector<WrapMove> moves;
  for (int dir = -1; dir <= 1; ++dir)
    for (int jump = 0; jump < 2; ++jump)
      for (int fire = 0; fire < 2; ++fire)
        moves.push_back({dir, jump != 0, fire != 0});
  moves.push_back({-1, false, false, true});
  moves.push_back({1, false, false, true});
  const auto& s = world.signal();
  const int gems0 = world.stats().gems;
  const auto& p0 = world.player();
  const bool home0 = wrapDist(p0.x, s.homeX, world.map().width()) < 3 && std::abs(p0.y - s.homeY) < 3;
  auto run = [&](World& sim, const WrapMove& m, Input& prev) {
    for (int f = 0; f < kWrapStep && sim.state() == WorldState::Playing; ++f)
    {
      const Input in = wrapInput(m, f);
      sim.update(simInput(in, prev));
      prev = in;
    }
  };
  auto evaluate = [&](World& sim, Input prev) {
    int sent = 0;
    for (int f = 0; f < 24 && sim.state() == WorldState::Playing && sim.player().state != PlayerState::OnGround; ++f)
    {
      const Input in;
      sim.update(simInput(in, prev));
      prev = in;
    }
    const auto& sp = sim.player();
    const int W = sim.map().width(), H = sim.map().height();
    if (!home0 && wrapDist(sp.x, s.homeX, W) < 1 && sp.y == s.homeY)
      sent = 1;
    double score = double(sim.stats().gems - gems0) * 10000.0;
    if (sim.state() != WorldState::Playing)
      score += 1e6;
    score -= sent * 6000.0;
    const WrapGrid g = wrapGrid(sim);
    score -= double(wrapCost(g, (sp.x + 1) / kCellsPerTile, sp.y / kCellsPerTile)) * 10.0;
    for (const auto& e : sim.enemies())
    {
      if (!e.alive || e.kind != EnemyKind::StaticSwarm)
        continue;
      const int d = std::max(wrapDist(e.x + 2, sp.x + 1, W), wrapDist(e.y - 2, sp.y - 2, H));
      if (d < 10)
        score -= double(10 - d) * 150.0;
    }
    for (const auto& e : sim.enemies())
      if (!e.alive && e.kind == EnemyKind::StaticSwarm)
        score += 400.0;
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
    // Progress now beats the same progress a move later.
    World now(*firstPtr);
    moveScore += 0.1 * evaluate(now, prevA);
    if (moveScore > bestScore)
    {
      bestScore = moveScore;
      best = int(a);
    }
  }
  static const bool debug = std::getenv("GR_FIGHT_DEBUG") != nullptr;
  if (debug)
    std::fprintf(stderr, "wrap f%d at %d,%d gems %d cost %d -> dir %d jump %d fire %d (%.0f)\n", world.stats().frames, p0.x,
      p0.y, gems0, wrapCost(wrapGrid(world), (p0.x + 1) / kCellsPerTile, p0.y / kCellsPerTile), moves[std::size_t(best)].dir, int(moves[std::size_t(best)].jump),
      int(moves[std::size_t(best)].fire), bestScore);
  for (int f = 0; f < kWrapStep; ++f)
    mInkQueue.push_back(wrapInput(moves[std::size_t(best)], f));
  const Input in = mInkQueue.front();
  mInkQueue.pop_front();
  mInkPrev = in;
  return in;
}

} // namespace gr
