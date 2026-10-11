// The bot in Level 30: standing at an anchor ring's planting spot with the
// Harpoon Line, it crouches and fires (a harpoon at its feet), turns to the
// ring and fires again; the two harpoons make the line the planner's field
// already counts on, and the planner walks it. In the Bubble Up bonus it
// steers the bubble: every short steer is tried two deep in copies of the
// world and scored by a distance field to the exit that keeps a cell clear
// of the spikes.

#include "frontend/bot.hpp"
#include "game/world.hpp"

#include <cstdio>
#include <cstdlib>
#include <queue>

namespace gr
{

namespace
{

constexpr int kBubbleStep = 4;
constexpr int kFar = 1 << 28;

PlayerInput bubbleInput(const Input& in, const Input& prev)
{
  PlayerInput p;
  p.left = in.left;
  p.right = in.right;
  p.jump.pressed = in.jump;
  p.jump.triggered = in.jump && !prev.jump;
  return p;
}

// A steer: a direction, and taps of jump (frames 0 and 2) to sink.
Input steerInput(int dir, int taps, int f)
{
  Input in;
  in.left = dir < 0;
  in.right = dir > 0;
  in.jump = (taps >= 1 && f == 0) || (taps >= 2 && f == 2);
  return in;
}

// Distance (cells moved) from every runner position to the exit: up is
// cheap, sideways a little dearer, down (taps) dearest; positions whose
// box, a cell bigger all round, touches spikes are out.
std::vector<int> bubbleField(const World& w)
{
  const auto& map = w.map();
  const int W = map.width(), H = map.height();
  const int h = w.player().height();
  std::vector<int> dist(std::size_t(W * H), kFar);
  auto ok = [&](int x, int y) {
    if (x < 0 || y < 0 || x + Player::kWidth > W || y >= H)
      return false;
    const CellBox b = boxAt(x, y, Player::kWidth, h);
    for (int yy = b.y; yy <= b.bottom(); ++yy)
      for (int xx = b.x; xx <= b.right(); ++xx)
        if (yy >= 0 && map.solid(xx, yy))
          return false;
    return !map.overlapsHazard(boxAt(x - 1, y + 1, Player::kWidth + 2, h + 2));
  };
  const auto& lv = w.level();
  const CellBox exitZone{lv.exitTx * kCellsPerTile, (lv.exitTy + 1) * kCellsPerTile - 6, 2, 6};
  using QE = std::pair<int, int>;
  std::priority_queue<QE, std::vector<QE>, std::greater<QE>> q;
  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x)
      if (ok(x, y) && boxAt(x, y, Player::kWidth, h).intersects(exitZone))
      {
        dist[std::size_t(y * W + x)] = 0;
        q.push({0, y * W + x});
      }
  while (!q.empty())
  {
    const auto [d, i] = q.top();
    q.pop();
    if (d != dist[std::size_t(i)])
      continue;
    const int x = i % W, y = i / W;
    // Backwards: who gets here. From below by rising (1), from the side (2),
    // from above by sinking (4).
    const int from[3][3] = {{x, y + 1, 1}, {x - 1, y, 2}, {x + 1, y, 2}};
    for (const auto& f : from)
      if (ok(f[0], f[1]) && d + f[2] < dist[std::size_t(f[1] * W + f[0])])
      {
        dist[std::size_t(f[1] * W + f[0])] = d + f[2];
        q.push({d + f[2], f[1] * W + f[0]});
      }
    if (ok(x, y - 1) && d + 4 < dist[std::size_t((y - 1) * W + x)])
    {
      dist[std::size_t((y - 1) * W + x)] = d + 4;
      q.push({d + 4, (y - 1) * W + x});
    }
  }
  return dist;
}

} // namespace

Input Bot::bubbleUp(const World& world)
{
  if (!mTrenchQueue.empty())
  {
    const Input in = mTrenchQueue.front();
    mTrenchQueue.pop_front();
    mBubblePrev = in;
    return in;
  }
  const int W = world.map().width();
  if (mBubbleField.size() != std::size_t(W * world.map().height()))
    mBubbleField = bubbleField(world);
  const auto& p0 = world.player();
  const int gems0 = world.stats().gems;
  auto run = [&](World& sim, int dir, int taps, Input& prev) {
    for (int f = 0; f < kBubbleStep && sim.state() == WorldState::Playing; ++f)
    {
      const Input in = steerInput(dir, taps, f);
      sim.update(bubbleInput(in, prev));
      prev = in;
    }
  };
  auto evaluate = [&](const World& sim) {
    const auto& sp = sim.player();
    if (sim.state() != WorldState::Playing)
      return 1e9;
    if (sim.trench().pop > 0)
      return -1e7;
    double score = double(sim.stats().gems - gems0) * 300.0;
    const int x = sp.x, y = sp.y;
    if (x < 0 || y < 0 || x >= W || y >= sim.map().height())
      return score - 1e6;
    return score - double(std::min(mBubbleField[std::size_t(y * W + x)], 100000)) * 10.0;
  };
  double bestScore = -1e18;
  int bestDir = 0, bestTaps = 0;
  for (int dir = -1; dir <= 1; ++dir)
    for (int taps = 0; taps <= 2; ++taps)
    {
      const auto first = world.cloneForSim();
      Input prevA = mBubblePrev;
      run(*first, dir, taps, prevA);
      double score = -1e18;
      if (first->state() != WorldState::Playing || first->trench().pop > 0)
        score = evaluate(*first);
      else
        for (int dir2 = -1; dir2 <= 1; ++dir2)
          for (int taps2 = 0; taps2 <= 2; ++taps2)
          {
            World sim(*first);
            Input prevB = prevA;
            run(sim, dir2, taps2, prevB);
            score = std::max(score, evaluate(sim));
          }
      score += 0.1 * evaluate(*first);
      if (score > bestScore)
      {
        bestScore = score;
        bestDir = dir;
        bestTaps = taps;
      }
    }
  static const bool debug = std::getenv("GR_FIGHT_DEBUG") != nullptr;
  if (debug)
    std::fprintf(stderr, "bubble f%d at %d,%d field %d -> dir %d taps %d (%.0f)\n", world.stats().frames, p0.x, p0.y,
      mBubbleField[std::size_t(p0.y * W + p0.x)], bestDir, bestTaps, bestScore);
  for (int f = 0; f < kBubbleStep; ++f)
    mTrenchQueue.push_back(steerInput(bestDir, bestTaps, f));
  const Input in = mTrenchQueue.front();
  mTrenchQueue.pop_front();
  mBubblePrev = in;
  return in;
}

bool Bot::trench(const World& world, Input& in)
{
  const auto& t = world.trench();
  const auto& p = world.player();
  if (!mTrenchQueue.empty())
  {
    in = mTrenchQueue.front();
    mTrenchQueue.pop_front();
    return true;
  }
  if (p.state != PlayerState::OnGround || p.weapon != Weapon::Proto || p.proto != int(ProtoId::HarpoonLine) ||
      p.ammo <= 0)
    return false;
  for (std::size_t i = 0; i < t.anchors.size(); ++i)
  {
    const auto& a = t.anchors[i];
    // On the line's row near the spot, with the ring in a line's reach.
    const int ringX = a.x < p.x ? a.x + 1 : a.x;
    if (p.y + 1 != a.row || std::abs(p.x + 1 - a.spotX) > 4 || std::abs(p.x + 1 - ringX) > kLineReach ||
        std::abs(p.x + 1 - ringX) < 2)
      continue;
    bool strung = false, planted = false;
    for (const auto& h : t.harpoons)
    {
      strung = strung || h.anchor == int(i);
      planted = planted || (h.anchor < 0 && h.y == p.y + 1 && std::abs(h.x - (p.x + 1)) <= 3);
    }
    if (strung && t.line)
      continue;
    const int dir = a.x < p.x ? -1 : 1;
    Input none, turn, fire;
    turn.left = dir < 0;
    turn.right = dir > 0;
    fire.fire = true;
    if (!planted)
    {
      Input down, plant;
      down.down = true;
      plant.down = plant.fire = true;
      mTrenchQueue = {down, plant, down, none};
      for (int k = 0; k < 8; ++k)
        mTrenchQueue.push_back(none); // the Harpoon Line's cooldown
      if (p.ammo < 2)
      {
        in = mTrenchQueue.front();
        mTrenchQueue.pop_front();
        return true; // the last harpoon: the ring waits for a refill
      }
    }
    if (p.facing != dir)
      mTrenchQueue.push_back(turn); // a turn on the spot (crouching keeps the facing)
    mTrenchQueue.push_back(none);
    mTrenchQueue.push_back(fire);
    for (int k = 0; k < 12; ++k)
      mTrenchQueue.push_back(none); // the shot on its way to the ring
    in = mTrenchQueue.front();
    mTrenchQueue.pop_front();
    return true;
  }
  return false;
}

} // namespace gr
