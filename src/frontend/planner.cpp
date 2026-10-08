#include "frontend/planner.hpp"

#include "game/world.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <limits>
#include <queue>
#include <unordered_map>

namespace gr
{

namespace
{

constexpr int kAirBudget = 12;     // cells of sideways movement in one jump
constexpr int kInf = std::numeric_limits<int>::max() / 4;
constexpr int kMacroFrames = 4;
constexpr int kBudget = 2600;      // nodes expanded per plan
constexpr int kProgress = 20;      // heuristic units that count as progress

struct Macro
{
  bool left, right, up, down, jump;
  int jumpFrames; // frames the jump button is held, from the first
  bool fire;      // tap on the first frame
};

// Short input patterns the search strings together.
const Macro kMacros[] = {
  {false, true, false, false, false, 0, false},  // right
  {true, false, false, false, false, 0, false},  // left
  {false, true, false, false, true, 4, false},   // full jump right
  {true, false, false, false, true, 4, false},   // full jump left
  {false, false, false, false, true, 4, false},  // jump straight up
  {false, true, false, false, true, 1, false},   // hop right
  {true, false, false, false, true, 1, false},   // hop left
  {false, false, false, false, false, 0, false}, // wait
  {false, false, true, false, false, 0, false},  // up (ladders, bonus doors)
  {false, false, false, true, false, 0, false},  // down (ladders, crawl)
  {false, false, false, false, false, 0, true},  // shoot ahead
  {false, false, false, true, false, 0, true},   // crouch and shoot
  {false, false, true, false, false, 0, true},   // shoot up
  {false, true, false, true, false, 0, false},   // crawl right
  {false, false, false, true, true, 1, false},   // down + jump: drop from a pipe
  {false, false, false, false, false, 0, false}, // a long wait (gondolas settle)
  {false, false, false, false, false, 0, false}, // stand on a subwoofer until it launches you
  {false, false, false, false, false, 0, false}, // sit out the chorus (laser fans)
};
constexpr int kLongWait = 15;
constexpr int kWaitLaunch = 16;
constexpr int kWaitVerse = 17;
// Frames a macro runs for in this world, 0 if it does not apply.
int macroFrames(int m, const World& w)
{
  if (m == kLongWait)
  {
    bool opening = false;
    for (const auto& d : w.doors())
      opening = opening || (d.solid && d.breaker >= 0 && w.breakers()[std::size_t(d.breaker)].on);
    bool tide = false;
    for (const auto& f : w.fluids())
      tide = tide || f.tide;
    return w.platforms().empty() && !opening && !tide && w.bubbles().empty() ? 0 : 24;
  }
  if (m == kWaitLaunch)
  {
    const int f = w.framesToNextLaunch();
    return f < 0 ? 0 : f + 1;
  }
  if (m == kWaitVerse)
  {
    // From the fans' preview bar to the end of the chorus.
    const int bar = phraseBar(w.clock());
    if (w.fans().empty() || bar < 7 || bar > 13)
      return 0;
    return 14 * 30 - w.clock() % kPhraseFrames;
  }
  return 4;
}
constexpr int kMacroCount = int(sizeof(kMacros) / sizeof(kMacros[0]));

Input macroInput(const Macro& m, int f)
{
  Input in;
  in.left = m.left;
  in.right = m.right;
  in.up = m.up;
  in.down = m.down;
  in.jump = m.jump && f < m.jumpFrames;
  in.fire = m.fire && f == 0;
  return in;
}

PlayerInput toPlayerInput(const Input& in, const Input& prev)
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

std::uint64_t mix(std::uint64_t h, std::uint64_t v)
{
  h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
  return h;
}

bool stable(const Player& p)
{
  return p.state == PlayerState::OnGround || p.state == PlayerState::Ladder || p.state == PlayerState::Pipe;
}

int clockPeriod(const World& w)
{
  int period = 1;
  for (const auto& l : w.layers())
  {
    int p = 1;
    if (l.driver == LayerDriver::Beat)
      p = 30 * l.bars;
    else if (l.driver == LayerDriver::Timer)
      p = std::max(1, l.on + l.off);
    period = std::max(period, p);
  }
  if (!w.pads().empty())
    period = std::max(period, 30); // subwoofers bump on the beat
  if (!w.fans().empty())
    period = std::max(period, kPhraseFrames); // laser fans sweep in the chorus
  for (const auto& f : w.fluids())
    if (f.tide)
      period = std::max(period, 300); // the tide clock
  return period;
}

} // namespace

Planner::Goal Planner::chooseGoal(const World& w) const
{
  Goal g;
  const auto& lv = w.level();
  g.kind = 0;
  g.x = lv.exitTx * kCellsPerTile;
  g.y = (lv.exitTy + 1) * kCellsPerTile - 1;
  // The prototype is what the level is built around: grab the nearest one.
  if (!mSkipProto && !w.stats().protoFound)
  {
    const auto& p = w.player();
    Goal best;
    int bestD = -1;
    auto consider = [&](int x, int y) {
      const int d = std::abs(x - p.x) + std::abs(y - p.y);
      if (bestD < 0 || d < bestD)
      {
        bestD = d;
        best = {2, x, y};
      }
    };
    for (const auto& it : w.items())
      if (it.kind == ItemKind::Proto && !it.taken)
        consider(it.x, it.y);
    for (const auto& b : w.boxes())
      if (b.alive && b.content == ItemKind::Proto)
        consider(b.x, b.y);
    if (bestD >= 0)
      return best;
  }
  // A force field still on and no card: fetch the card first.
  bool fields = false;
  if (w.map().forceFieldsOn())
    for (int ty = 0; ty < lv.height && !fields; ++ty)
      for (int tx = 0; tx < lv.width && !fields; ++tx)
        fields = w.map().block(tx, ty) == Tile::ForceField;
  if (fields && !w.player().hasKey)
  {
    for (const auto& it : w.items())
      if (it.kind == ItemKind::Key)
        return {1, it.x, it.y};
    for (const auto& b : w.boxes())
      if (b.alive && b.content == ItemKind::Key)
        return {1, b.x, b.y};
  }
  if (mTakeBonus && !mSkipBonus)
    for (const auto& pr : w.props())
      if (pr.kind == PropKind::BonusDoor && !pr.used)
      {
        // Walled in by something only the prototype breaks (level 3's
        // speaker): top up its ammo first from a box close by.
        const auto& p = w.player();
        const ProtoDef& lp = protoDef(std::max(0, protoIndex(w.level().weapon)));
        int need = 0;
        const CellBox door{pr.x - 4, pr.y - 4, pr.w + 8, pr.h + 8};
        for (const auto& b : w.breakables())
          if (!b.broken && b.by == 3 &&
              door.intersects({b.x0 * kCellsPerTile, b.y0 * kCellsPerTile, (b.x1 - b.x0 + 1) * kCellsPerTile,
                (b.y1 - b.y0 + 1) * kCellsPerTile}))
            need += (b.hp + lp.damage - 1) / std::max(1, lp.damage);
        const int ammo = p.weapon == Weapon::Proto ? p.ammo : 0;
        if (need > ammo && !mSkipProto)
        {
          Goal best;
          int bestD = 90;
          for (const auto& b : w.boxes())
          {
            const int d = std::abs(b.x - p.x) + std::abs(b.y - p.y);
            if (b.alive && b.content == ItemKind::Proto && d < bestD)
            {
              bestD = d;
              best = {2, b.x, b.y};
            }
          }
          for (const auto& it : w.items())
          {
            const int d = std::abs(it.x - p.x) + std::abs(it.y - p.y);
            if (!it.taken && it.kind == ItemKind::Proto && d < bestD)
            {
              bestD = d;
              best = {2, it.x, it.y};
            }
          }
          if (best.kind == 2)
            return best;
        }
        return {3, pr.x, pr.y, pr.w, pr.h};
      }
  // Power cuts: a Grid Leech heading for a breaker comes first, then the
  // breakers in order, skipping any behind a shutter that is down.
  const auto& p = w.player();
  for (std::size_t i = 0; i < w.enemies().size(); ++i)
  {
    const auto& e = w.enemies()[i];
    if (e.alive && e.kind == EnemyKind::Leech && std::abs(e.x - p.x) < 120)
      return {5, e.x / 4 * 4, e.y / 4 * 4, 0, 0, int(i)};
  }
  for (std::size_t i = 0; i < w.breakers().size(); ++i)
  {
    const auto& b = w.breakers()[i];
    if (b.on || b.throwing > 0)
      continue;
    bool behind = false;
    for (const auto& d : w.doors())
    {
      const int dx = d.x0 * kCellsPerTile;
      const bool power = d.breaker >= 0 &&
        (w.breakers()[std::size_t(d.breaker)].on || w.breakers()[std::size_t(d.breaker)].throwing > 0);
      if (d.solid && d.h > d.w && !power &&
          dx > std::min(p.x, b.x) && dx < std::max(p.x, b.x))
        behind = true;
    }
    if (!behind)
      return {4, b.x, b.y, 2, 4, int(i)};
  }
  return g;
}

void Planner::buildField(const World& w, const Goal& goal)
{
  const CollisionMap& map = w.map();
  mW = map.width();
  mH = map.height();
  const int W = mW, H = mH;
  // With the card (or once the card is no longer needed) force fields open.
  const bool openFields = goal.kind == 0 || goal.kind == 3 || w.player().hasKey;
  int jumpH = 0;
  for (int v : w.character().jumpArc)
    jumpH += v;

  // Cell solidity as the planner sees it: switchable layers count as solid
  // (the search handles their timing), force fields are open on the way to
  // the exit.
  std::vector<std::uint8_t> blocked(std::size_t(W * H)), top(std::size_t(W * H));
  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x)
    {
      bool b = map.solid(x, y);
      if (b && openFields && map.forceField(x, y))
        b = false;
      blocked[std::size_t(y * W + x)] = b;
      // Sludge surfaces and bubbles come and go: not ground to the field.
      top[std::size_t(y * W + x)] = b || (map.solidTop(x, y) && !map.floatTop(x, y));
    }
  // Shutters whose breaker is on are rolling up: the search waits for them.
  for (const auto& d : w.doors())
    if (d.solid && d.breaker >= 0 && w.breakers()[std::size_t(d.breaker)].on)
      for (int y = d.y0 * kCellsPerTile; y < (d.y0 + d.h) * kCellsPerTile; ++y)
        for (int x = d.x0 * kCellsPerTile; x < (d.x0 + d.w) * kCellsPerTile; ++x)
          if (x >= 0 && y >= 0 && x < W && y < H)
            blocked[std::size_t(y * W + x)] = top[std::size_t(y * W + x)] = 0;
  // Breakables the current weapon can shatter: the search shoots them.
  const auto& pl0 = w.player();
  const bool sound = pl0.weapon == Weapon::Proto && pl0.proto == int(ProtoId::BassCannon);
  mWalls.clear();
  for (std::size_t bi = 0; bi < w.breakables().size(); ++bi)
  {
    const auto& b = w.breakables()[bi];
    if (b.broken || !(b.by == 0 || (b.by == 3 && sound) || (b.by == 1 && pl0.weapon == Weapon::Rocket)))
      continue;
    const CellBox area{b.x0 * kCellsPerTile - 12, b.y0 * kCellsPerTile - 12, (b.x1 - b.x0 + 1) * kCellsPerTile + 24,
      (b.y1 - b.y0 + 1) * kCellsPerTile + 24};
    if (area.intersects({goal.x, goal.y - 5, std::max(2, goal.w), std::max(6, goal.h)}))
      mWalls.push_back(int(bi));
    for (int y = b.y0 * kCellsPerTile; y < (b.y1 + 1) * kCellsPerTile; ++y)
      for (int x = b.x0 * kCellsPerTile; x < (b.x1 + 1) * kCellsPerTile; ++x)
        if (x >= 0 && y >= 0 && x < W && y < H)
          blocked[std::size_t(y * W + x)] = top[std::size_t(y * W + x)] = 0;
  }
  for (const auto& l : w.layers())
    for (int ty = l.y0; ty <= l.y1; ++ty)
      for (int tx = l.x0; tx <= l.x1; ++tx)
        for (int dy = 0; dy < kCellsPerTile; ++dy)
          for (int dx = 0; dx < kCellsPerTile; ++dx)
          {
            const int x = tx * kCellsPerTile + dx, y = ty * kCellsPerTile + dy;
            if (x < 0 || y < 0 || x >= W || y >= H)
              continue;
            // Timed layers are only solid some of the time: the field
            // treats them as one-way platforms and the search does the timing.
            const bool timed = l.driver == LayerDriver::Beat || l.driver == LayerDriver::Timer;
            if (l.tile == Tile::Solid && !timed)
              blocked[std::size_t(y * W + x)] = 1;
            else if (l.tile == Tile::Solid)
              blocked[std::size_t(y * W + x)] = 0;
            if ((l.tile == Tile::Solid && !timed) || dy == 0)
              top[std::size_t(y * W + x)] = 1;
          }
  // Sludge that never moves holds you up like a floor (it still hurts).
  for (const auto& f : w.fluids())
    if (!f.tide && !w.autorun() && f.surface + 1 < H)
      for (int x = std::max(0, f.x0); x <= std::min(W - 1, f.x1); ++x)
        top[std::size_t((f.surface + 1) * W + x)] = 1;
  // The raft rides the tide: anywhere between low and high water.
  for (const auto& b : w.bubbles())
  {
    if (!b.raft)
      continue;
    int y0 = b.y, y1 = b.y;
    for (const auto& f : w.fluids())
      if (f.covers(b.x + b.w / 2) && f.tide)
      {
        y0 = f.high - b.h + 1;
        y1 = f.low - b.h + 1;
      }
    for (int y = std::max(0, y0); y <= std::min(H - 1, y1); ++y)
      for (int x = std::max(0, b.x); x < std::min(W, b.x + b.w); ++x)
        top[std::size_t(y * W + x)] = 1;
  }
  // Moving platforms: anywhere along their travel is somewhere to stand;
  // the search finds when they are actually there.
  for (const auto& pl : w.platforms())
  {
    int y0 = pl.y, y1 = pl.y;
    int x0 = pl.x, x1 = pl.x;
    if (pl.mode == PlatformMode::Pulley)
    {
      y0 = pl.homeY - pl.travel;
      y1 = pl.homeY + pl.travel;
    }
    else if (pl.powered >= 0 && !w.breakers()[std::size_t(pl.powered)].on)
    {
      // A lift with no power stays where it is.
    }
    else
      for (const auto& [px, py] : pl.path)
      {
        x0 = std::min(x0, px);
        x1 = std::max(x1, px);
        y0 = std::min(y0, py);
        y1 = std::max(y1, py);
      }
    for (int y = std::max(0, y0); y <= std::min(H - 1, y1); ++y)
      for (int x = std::max(0, x0); x < std::min(W, x1 + pl.w); ++x)
        top[std::size_t(y * W + x)] = 1;
  }
  auto isBlocked = [&](int x, int y) {
    if (x < 0 || x >= W)
      return true;
    if (y < 0)
      return true;
    if (y >= H)
      return false;
    return blocked[std::size_t(y * W + x)] != 0;
  };
  auto isTop = [&](int x, int y) {
    if (x < 0 || x >= W || y < 0 || y >= H)
      return false;
    return top[std::size_t(y * W + x)] != 0;
  };
  // Subwoofers: how high you get from standing on one (a launch, or a jump
  // on the beat).
  std::vector<int> padLift(std::size_t(W * H), 0);
  for (const auto& pad : w.pads())
  {
    const int lift = std::max(jumpH + 2, pad.fire > 0 ? jumpH * pad.launchX10 / 10 : 0);
    for (int x = std::max(0, pad.x); x < std::min(W, pad.x + pad.w); ++x)
      if (pad.y >= 0 && pad.y < H)
        padLift[std::size_t(pad.y * W + x)] = lift;
  }

  // Per player position (bottom-left cell): valid, height above ground,
  // ladder and pipe.
  std::vector<std::uint8_t> valid(std::size_t(W * H)), ladder(std::size_t(W * H)), hang(std::size_t(W * H)),
    hazard(std::size_t(W * H));
  std::vector<int> support(std::size_t(W * H), kInf), lift(std::size_t(W * H), jumpH);
  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x)
    {
      bool ok = true;
      // Duck Rapids: crouching under the low pipes counts as getting through.
      for (int yy = y - (w.autorun() ? 3 : 4); yy <= y && ok; ++yy)
        for (int xx = x; xx <= x + 2 && ok; ++xx)
          ok = !isBlocked(xx, yy);
      const std::size_t i = std::size_t(y * W + x);
      valid[i] = ok;
      if (!ok)
        continue;
      for (int k = 0; k < 64 && y + 1 + k < H; ++k)
      {
        if (isTop(x, y + 1 + k) || isTop(x + 1, y + 1 + k) || isTop(x + 2, y + 1 + k))
        {
          support[i] = k;
          for (int xx = x; xx <= x + 2; ++xx)
            if (xx < W && y + 1 + k < H)
              lift[i] = std::max(lift[i], padLift[std::size_t((y + 1 + k) * W + xx)]);
          break;
        }
      }
      for (int yy = y - 4; yy <= y; ++yy)
        if (map.ladder(x + 1, yy))
          ladder[i] = 1;
      hang[i] = map.climbable(x + 1, y - 5) || map.climbable(x + 1, y - 4);
      for (int yy = y - 4; yy <= y && !hazard[i]; ++yy)
        for (int xx = x; xx <= x + 2; ++xx)
          if (map.hazard(xx, yy))
            hazard[i] = 1;
      // Feet in sludge cost hearts; where the tide only sometimes reaches,
      // the search sorts out the timing.
      for (const auto& f : w.fluids())
        if (!hazard[i] && x + 2 >= f.x0 && x <= f.x1 && y <= f.y1)
          hazard[i] = y >= f.surface ? 1 : (f.tide && y >= f.high ? 2 : 0);
    }

  const int A = kAirBudget + 1;
  const int N = W * H * A;
  auto node = [&](int x, int y, int a) { return (y * W + x) * A + a; };
  // Forward edges, then Dijkstra backwards from the goal.
  std::vector<int> revStart(std::size_t(N) + 1, 0);
  std::vector<std::pair<int, int>> edges; // (to, from|cost<<..) built as (from, to, cost)
  struct E
  {
    int from, to, cost;
  };
  std::vector<E> list;
  list.reserve(std::size_t(N) / 4);
  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x)
    {
      const std::size_t i = std::size_t(y * W + x);
      if (!valid[i])
        continue;
      const int s = support[i];
      const bool grounded = s == 0 || ladder[i] || hang[i];
      for (int a = 0; a < A; ++a)
      {
        if (grounded && a > 0)
          continue;
        const int from = node(x, y, a);
        auto add = [&](int tx, int ty, int ta, int cost) {
          if (tx < 0 || tx >= W || ty < 0 || ty >= H)
            return;
          const std::size_t j = std::size_t(ty * W + tx);
          if (!valid[j])
            return;
          const bool g2 = support[j] == 0 || ladder[j] || hang[j];
          if (g2)
            ta = 0;
          if (ta >= A)
            return;
          list.push_back({from, node(tx, ty, ta), cost + (hazard[j] == 1 ? 40 : (hazard[j] == 2 ? 6 : 0))});
        };
        // Sideways: free on the ground, from the air budget in a jump.
        for (int dx : {-1, 1})
        {
          add(x + dx, y, grounded ? (s == 0 ? 0 : 1) : a + 1, 2);
          if (s == 0)
            add(x + dx, y - 1, 0, 2); // stair step
        }
        // Up: within a jump's height of the ground, or on a ladder.
        if (ladder[i] || (s < lift[i] && s < kInf))
          add(x, y - 1, ladder[i] ? 0 : a, 2);
        // Down: falling, or climbing down.
        add(x, y + 1, a, 1);
      }
    }
  for (const auto& e : list)
    ++revStart[std::size_t(e.to) + 1];
  for (int i = 0; i < N; ++i)
    revStart[std::size_t(i) + 1] += revStart[std::size_t(i)];
  std::vector<std::pair<int, int>> rev(list.size());
  {
    std::vector<int> fill(revStart.begin(), revStart.end() - 1);
    for (const auto& e : list)
      rev[std::size_t(fill[std::size_t(e.to)]++)] = {e.from, e.cost};
  }

  mDist.assign(std::size_t(N), kInf);
  using QE = std::pair<int, int>;
  std::priority_queue<QE, std::vector<QE>, std::greater<QE>> q;
  const CellBox goalBox = goal.kind == 0 ? CellBox{goal.x, goal.y - 5, 2, 6}
    : goal.kind == 3                      ? CellBox{goal.x, goal.y, goal.w, goal.h}
    : goal.kind == 4                      ? boxAt(goal.x, goal.y, 2, 4)
    : goal.kind == 5                      ? CellBox{goal.x - 14, goal.y - 3, 30, 6} // in range for a level shot
                                          : CellBox{goal.x, goal.y - 1, 2, 2};
  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x)
    {
      const std::size_t i = std::size_t(y * W + x);
      if (!valid[i])
        continue;
      if (!boxAt(x, y, 3, 5).intersects(goalBox))
        continue;
      if ((goal.kind == 0 || goal.kind == 4 || goal.kind == 5) && support[i] != 0)
        continue;
      // A bonus entrance can be up in the air: jumping into it is fine.
      if (goal.kind == 3 && support[i] > lift[i])
        continue;
      for (int a = 0; a < A; ++a)
      {
        mDist[std::size_t(node(x, y, a))] = 0;
        q.push({0, node(x, y, a)});
      }
    }
  static const bool debug = std::getenv("GR_PLANNER_DEBUG") != nullptr;
  if (debug)
    std::fprintf(stderr, "field: goal %d at %d,%d: %zu goal nodes, %zu edges\n", goal.kind, goal.x, goal.y, q.size(),
      list.size());
  while (!q.empty())
  {
    const auto [d, n] = q.top();
    q.pop();
    if (d != mDist[std::size_t(n)])
      continue;
    for (int k = revStart[std::size_t(n)]; k < revStart[std::size_t(n) + 1]; ++k)
    {
      const auto [from, cost] = rev[std::size_t(k)];
      if (d + cost < mDist[std::size_t(from)])
      {
        mDist[std::size_t(from)] = d + cost;
        q.push({d + cost, from});
      }
    }
  }
  if (std::getenv("GR_FIELD_DUMP"))
    dumpField();
  if (const char* probe = std::getenv("GR_FIELD_PROBE"))
  {
    int px = 0, py = 0;
    if (std::sscanf(probe, "%d,%d", &px, &py) == 2)
      for (int y = py - 3; y <= py + 3; ++y)
        for (int x = px - 3; x <= px + 3; ++x)
        {
          if (x < 0 || y < 0 || x >= W || y >= H)
            continue;
          const std::size_t i = std::size_t(y * W + x);
          std::fprintf(stderr, "probe %d,%d valid %d support %d ladder %d dist0 %d\n", x, y, valid[i], support[i],
            ladder[i], mDist[std::size_t(node(x, y, 0))]);
        }
  }
}

void Planner::dumpField() const
{
  const int A = kAirBudget + 1;
  for (int by = 0; by < mH / 2; ++by)
  {
    std::string row;
    for (int bx = 0; bx < mW / 2; ++bx)
    {
      int best = kInf;
      for (int y = by * 2; y < by * 2 + 2; ++y)
        for (int x = bx * 2; x < bx * 2 + 2; ++x)
          for (int a = 0; a < A; ++a)
            best = std::min(best, mDist[std::size_t((y * mW + x) * A + a)]);
      row += best >= kInf ? '.' : char('0' + std::min(9, best / 100));
    }
    std::fprintf(stderr, "%s\n", row.c_str());
  }
}

int Planner::heuristic(const World& w) const
{
  const auto& p = w.player();
  // The card still in its box: breaking the box is part of the way there.
  int extra = 0;
  if (mGoalKind == 1 || mGoalKind == 2)
    for (const auto& b : w.boxes())
      if (b.alive && b.content == (mGoalKind == 1 ? ItemKind::Key : ItemKind::Proto))
        extra = 40;
  // A Leech: every hit on it is progress.
  if (mGoalKind == 5 && mGoalIndex >= 0 && std::size_t(mGoalIndex) < w.enemies().size())
    extra += 20 * std::max(0, w.enemies()[std::size_t(mGoalIndex)].hp);
  // A shutter rolling up close by: waiting for it is progress.
  for (const auto& d : w.doors())
    if (d.solid && d.breaker >= 0 && w.breakers()[std::size_t(d.breaker)].on &&
        std::abs(d.x0 * kCellsPerTile - p.x) < 40)
      extra += std::max(0, d.opentime - d.open);
  for (int bi : mWalls)
    if (std::size_t(bi) < w.breakables().size() && !w.breakables()[std::size_t(bi)].broken)
      extra += 12 * std::max(0, w.breakables()[std::size_t(bi)].hp);
  if (p.x < 0 || p.y < 0 || p.x >= mW || p.y >= mH)
    return kInf;
  const int A = kAirBudget + 1;
  const int base = (p.y * mW + p.x) * A;
  int best = kInf;
  for (int a = 0; a < A; ++a)
    best = std::min(best, mDist[std::size_t(base + a)]);
  if (best >= kInf)
  {
    // Somewhere the field does not reach (mid-jump inside a gap): look
    // around for the nearest known value.
    for (int r = 1; r <= 3 && best >= kInf; ++r)
      for (int dy = -r; dy <= r; ++dy)
        for (int dx = -r; dx <= r; ++dx)
        {
          const int x = p.x + dx, y = p.y + dy;
          if (x < 0 || y < 0 || x >= mW || y >= mH)
            continue;
          for (int a = 0; a < A; ++a)
            best = std::min(best, mDist[std::size_t((y * mW + x) * A + a)] + 4 * r);
        }
  }
  return best >= kInf ? kInf : best + extra;
}

Input Planner::next(const World& world)
{
  if (mQueue.empty())
    plan(world);
  Input in;
  if (!mQueue.empty())
  {
    in = mQueue.front();
    mQueue.pop_front();
  }
  mPrev = in;
  return in;
}

void Planner::plan(const World& world)
{
  const auto& p0 = world.player();
  if (p0.state == PlayerState::Dying || p0.state == PlayerState::Teleporting || world.state() != WorldState::Playing)
  {
    mQueue.push_back(Input{});
    return;
  }

  // A bonus the search gave up on gets another try further on: the way
  // there can be easier from the next room.
  if (mSkipBonus && mSkipBonusAt >= 0 && p0.x > mSkipBonusAt + 40)
  {
    mSkipBonus = false;
    mSkipBonusAt = -1;
  }
  Goal goal = chooseGoal(world);
  // A prototype the field cannot reach, or one the search keeps failing to
  // get to, is skipped.
  if (goal.kind == 3 && mGoalKind == 3 && mFails >= 6)
  {
    mSkipBonus = true;
    mSkipBonusAt = p0.x;
    goal = chooseGoal(world);
  }
  if (goal.kind == 2 && mGoalKind == 2 && mFails >= 6)
  {
    mSkipProto = true;
    mSkipHadKey = world.player().hasKey;
  }
  // Behind a force field, a prototype becomes reachable with the card.
  if (mSkipProto && !mSkipHadKey && world.player().hasKey)
  {
    mSkipProto = false;
    goal = chooseGoal(world);
  }
  for (int attempt = 0; attempt < 3; ++attempt)
  {
    if (mSkipProto && goal.kind == 2)
      goal = chooseGoal(world);
    // The field also changes when a wall breaks or the Bass Cannon arrives.
    int broken = 0;
    for (const auto& b : world.breakables())
      broken += b.broken;
    const bool sound = world.player().weapon == Weapon::Proto && world.player().proto == int(ProtoId::BassCannon);
    int powered = 0;
    for (const auto& b : world.breakers())
      powered = powered * 2 + b.on;
    const int keyHash = goal.kind * 1000000 + goal.x * 1000 + goal.y + (world.player().hasKey ? 500000000 : 0) +
      (std::min(broken, 15) * 2 + (sound ? 1 : 0)) * 10000000 + powered * 7919;
    if (mDist.empty() || keyHash != mGoalKeyHash)
    {
      buildField(world, goal);
      mGoalKeyHash = keyHash;
      mGoalKind = goal.kind;
      mGoalIndex = goal.index;
    }
    if ((goal.kind != 2 && goal.kind != 3) || heuristic(world) < kInf)
      break;
    if (goal.kind == 3)
    {
      mSkipBonus = true;
      mSkipBonusAt = -1;
      goal = chooseGoal(world);
      continue;
    }
    mSkipProto = true;
    mSkipHadKey = world.player().hasKey;
  }

  struct Node
  {
    std::unique_ptr<World> w;
    int parent;
    int macro;
    int g;
    int h;
    Input last;
    int frames; // how long its macro ran
  };
  std::vector<Node> nodes;
  nodes.reserve(kBudget * 4);
  const int period = clockPeriod(world);
  auto keyOf = [&](const World& w) {
    const auto& p = w.player();
    std::uint64_t k = 1469598103934665603ull;
    k = mix(k, std::uint64_t(p.x) | (std::uint64_t(p.y) << 16));
    k = mix(k, std::uint64_t(int(p.state)) | (std::uint64_t(std::min(p.frames, 12)) << 8) |
                 (std::uint64_t(p.facing > 0) << 16) | (std::uint64_t(w.clock() % period) << 20));
    k = mix(k, std::uint64_t(p.hp) | (std::uint64_t(int(p.weapon)) << 8) | (std::uint64_t(p.hasKey) << 12));
    int alive = 0;
    for (const auto& e : w.enemies())
      alive += e.alive;
    for (const auto& b : w.boxes())
      alive += b.alive * 64;
    k = mix(k, std::uint64_t(alive) | (std::uint64_t(w.items().size()) << 20));
    // Bouncers and Keepers close by: shoving one away, or wearing a Keeper
    // down, is progress too.
    for (const auto& e : w.enemies())
      if (e.alive &&
          (e.kind == EnemyKind::Bouncer || e.kind == EnemyKind::Stepper || e.kind == EnemyKind::Keeper) &&
          std::abs(e.x - p.x) < 24 && std::abs(e.y - p.y) < 16)
        k = mix(k, std::uint64_t(e.x) | (std::uint64_t(e.y) << 16) |
                     (e.kind == EnemyKind::Keeper ? std::uint64_t(e.hp) << 32 : 0));
    k = mix(k, std::uint64_t(w.launching()));
    int cracks = 0;
    for (const auto& b : w.breakables())
      cracks = cracks * 7 + b.hp;
    k = mix(k, std::uint64_t(cracks));
    k = mix(k, std::uint64_t(w.bonusRequested()) | (std::uint64_t(w.stats().protoFound) << 1));
    for (const auto& pl : w.platforms())
      k = mix(k, std::uint64_t(pl.y) | (std::uint64_t(pl.x) << 16) | (std::uint64_t(pl.braked) << 32));
    for (const auto& b : w.breakers())
      k = mix(k, std::uint64_t(b.on) | (std::uint64_t(b.throwing) << 1));
    for (const auto& d : w.doors())
      k = mix(k, std::uint64_t(d.open) | (std::uint64_t(d.solid) << 16));
    for (const auto& e : w.enemies())
      if (e.alive && e.kind == EnemyKind::Leech)
        k = mix(k, std::uint64_t(e.aimX) | (std::uint64_t(e.hp) << 16));
    for (const auto& f : w.fluids())
      k = mix(k, std::uint64_t(f.surface) | (std::uint64_t(std::uint32_t(f.floodAt)) << 16));
    for (const auto& b : w.bubbles())
      k = mix(k, std::uint64_t(b.x) | (std::uint64_t(b.y) << 16) | (std::uint64_t(b.stood) << 32));
    return k;
  };
  const int h0 = heuristic(world);
  const int hp0 = p0.hp;
  auto score = [&](const Node& n) { return n.g + n.h; };
  auto cmp = [&](int a, int b) { return score(nodes[std::size_t(a)]) > score(nodes[std::size_t(b)]); };
  std::priority_queue<int, std::vector<int>, decltype(cmp)> open(cmp);
  std::unordered_map<std::uint64_t, int> seen;

  nodes.push_back({world.cloneForSim(), -1, -1, 0, h0, mPrev, 0});
  open.push(0);
  int found = -1, bestStable = -1;
  int expanded = 0;
  while (!open.empty() && expanded < kBudget)
  {
    const int ni = open.top();
    open.pop();
    const Node& n = nodes[std::size_t(ni)];
    const World& nw = *n.w;
    const auto& np = nw.player();
    const bool gotProto = nw.stats().protoFound &&
      (!world.stats().protoFound || (np.weapon == Weapon::Proto && (p0.weapon != Weapon::Proto || np.ammo > p0.ammo)));
    const bool thrown = goal.kind == 4 && std::size_t(goal.index) < nw.breakers().size() &&
      (nw.breakers()[std::size_t(goal.index)].on || nw.breakers()[std::size_t(goal.index)].throwing > 0);
    const bool leechGone = goal.kind == 5 && std::size_t(goal.index) < nw.enemies().size() &&
      !nw.enemies()[std::size_t(goal.index)].alive;
    const bool success = nw.state() != WorldState::Playing || (goal.kind == 1 && np.hasKey) ||
      (goal.kind == 2 && gotProto) || (goal.kind == 3 && nw.bonusRequested()) || thrown || leechGone;
    if (ni != 0 && success)
    {
      found = ni;
      break;
    }
    if (ni != 0 && stable(np) && np.hp >= hp0 - 1)
    {
      if (n.h <= h0 - kProgress)
      {
        found = ni;
        break;
      }
      if (bestStable < 0 || n.h < nodes[std::size_t(bestStable)].h)
        bestStable = ni;
    }
    ++expanded;
    for (int m = 0; m < kMacroCount; ++m)
    {
      // Waiting only matters while something moves on its own.
      const int frames = macroFrames(m, nw);
      if (frames == 0)
        continue;
      auto child = std::make_unique<World>(nw);
      Input prev = n.last;
      bool dead = false;
      for (int f = 0; f < frames; ++f)
      {
        const Input in = macroInput(kMacros[m], f);
        child->update(toPlayerInput(in, prev));
        prev = in;
        if (child->player().state == PlayerState::Dying || child->bonusFailed())
        {
          dead = true;
          break;
        }
        if (child->state() != WorldState::Playing)
          break;
      }
      if (dead)
        continue;
      const auto key = keyOf(*child);
      const int lost = std::max(0, nw.player().hp - child->player().hp);
      // Waiting for the drop counts as a short wait: it is the way up.
      const int g = n.g + (m == kWaitLaunch ? 8 : (m == kWaitVerse ? frames / 2 : frames)) + lost * 60 + (m >= 10 && m <= 12 ? 1 : 0);
      const auto it = seen.find(key);
      if (it != seen.end() && it->second <= g)
        continue;
      seen[key] = g;
      const int h = child->state() != WorldState::Playing ? 0 : heuristic(*child);
      if (h >= kInf)
        continue;
      nodes.push_back({std::move(child), ni, m, g, h, prev, frames});
      open.push(int(nodes.size()) - 1);
    }
    // Free worlds we will not expand again (keeps memory flat).
    nodes[std::size_t(ni)].w.reset();
  }

  if (found < 0 && bestStable >= 0 && nodes[std::size_t(bestStable)].h < h0)
    found = bestStable;
  static const bool debug = std::getenv("GR_PLANNER_DEBUG") != nullptr;
  if (debug)
    std::fprintf(stderr, "plan f%d at %d,%d goal %d h0 %d expanded %d nodes %zu -> %s (h %d)\n", world.clock(), p0.x,
      p0.y, goal.kind, h0, expanded, nodes.size(), found < 0 ? "none" : "ok",
      found < 0 ? -1 : nodes[std::size_t(found)].h);
  if (found < 0)
  {
    // No progress found: wiggle so the next plan starts somewhere else.
    ++mFails;
    const Macro& m = kMacros[(mFails * 7) % 8];
    for (int f = 0; f < kMacroFrames * 2; ++f)
      mQueue.push_back(macroInput(m, f % kMacroFrames));
    return;
  }
  mFails = 0;
  std::vector<int> chain;
  for (int i = found; i > 0; i = nodes[std::size_t(i)].parent)
    chain.push_back(i);
  std::reverse(chain.begin(), chain.end());
  for (int i : chain)
    for (int f = 0; f < nodes[std::size_t(i)].frames; ++f)
      mQueue.push_back(macroInput(kMacros[nodes[std::size_t(i)].macro], f));
}

} // namespace gr
