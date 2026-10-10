#include "frontend/planner.hpp"

#include "game/world.hpp"

#include <algorithm>
#include <cmath>
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
constexpr int kMacroCountBase = int(sizeof(kMacros) / sizeof(kMacros[0]));
constexpr int kMacroCount = kMacroCountBase + 2;
constexpr int kLongWait = 15;
constexpr int kWaitLaunch = 16;
constexpr int kWaitVerse = 17;
// Swing vines (level 8): pump the vine you hold, then launch at the top of
// a swing to the right or to the left. Their inputs are worked out as they
// run (see Planner::plan), not taken from kMacros.
constexpr int kVineRight = kMacroCountBase;
constexpr int kVineLeft = kMacroCountBase + 1;
// Frames a macro runs for in this world, 0 if it does not apply.
int macroFrames(int m, const World& w)
{
  if (m == kVineRight || m == kVineLeft)
    return w.player().state == PlayerState::Swing ? 1 : 0; // the real length comes out of the run
  if (m == kLongWait)
  {
    bool opening = false;
    for (const auto& d : w.doors())
      opening = opening || (d.solid && d.breaker >= 0 && w.breakers()[std::size_t(d.breaker)].on);
    bool tide = false;
    for (const auto& f : w.fluids())
      tide = tide || f.tide;
    return w.platforms().empty() && !opening && !tide && w.bubbles().empty() && !w.trainBusy() && w.sunDoors().empty()
      ? 0
      : 24;
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

bool stable(const World& w)
{
  const Player& p = w.player();
  if (w.bounce())
    return true; // Bounce House: you never stand, anywhere will do
  // Level 9: a floor tile that cracks under you is no place to stop.
  if (p.state == PlayerState::OnGround)
    for (const auto& c : w.collapseTiles())
      if (c.ty * kCellsPerTile == p.y + 1 && c.tx * kCellsPerTile + 1 >= p.x && c.tx * kCellsPerTile <= p.x + 2)
        return false;
  return p.state == PlayerState::OnGround || p.state == PlayerState::Ladder || p.state == PlayerState::Pipe ||
    p.state == PlayerState::Swing;
}

// A vine macro's input this frame: push along the swing until a launch
// the right way is ready, then jump (and keep leaning that way a moment).
// Sets `done` once the launch is a few frames old, `fail` if the runner is
// off the vine any other way or it takes too long.
Input vineInput(const World& w, int dir, int f, int& launchedAt, bool& done, bool& fail)
{
  Input in;
  const auto& p = w.player();
  if (launchedAt >= 0)
  {
    in.right = dir > 0;
    in.left = dir < 0;
    done = f - launchedAt >= 3;
    return in;
  }
  if (p.state != PlayerState::Swing || p.vine < 0 || f > 120)
  {
    fail = true;
    return in;
  }
  if (w.vineLaunchDir() == dir)
  {
    in.jump = true;
    in.right = dir > 0;
    in.left = dir < 0;
    launchedAt = f;
    return in;
  }
  const int sd = w.vines()[std::size_t(p.vine)].swingDir();
  in.right = sd > 0;
  in.left = sd < 0;
  return in;
}

// Under the hunter's light, a place to stop must also be safe a little
// while after: some simple way out (stay, crouch, hop, run either way) has
// to get through the next 40 frames without losing a heart. Rockets in the
// air and a salvo counting down only show up that far ahead.
bool safeAhead(const World& w)
{
  if (!w.hunter().on || (w.hunter().beep == 0 && w.hunter().left == 0 && w.strikes().empty()))
    return true;
  static const Input kWays[] = {
    {}, // stay
    [] { Input i; i.right = true; return i; }(),
    [] { Input i; i.left = true; return i; }(),
  };
  for (const Input& way : kWays)
  {
    World sim(w);
    const int hp = sim.player().hp;
    Input prev;
    bool ok = true;
    for (int f = 0; f < 40 && ok; ++f)
    {
      sim.update(toPlayerInput(way, prev));
      prev = way;
      ok = sim.player().hp >= hp && sim.player().state != PlayerState::Dying;
      if (sim.state() != WorldState::Playing)
        break;
    }
    if (ok)
      return true;
  }
  return false;
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
  if (!w.gantries().empty())
    period = std::max(period, 1 << 12); // the train: gantries, rings and couplings run on time
  // Level 9: traps on a cycle, Dart Faces on their rhythm.
  for (const auto& t : w.traps())
    if (t.plate < 0 && t.cycle > 0)
      period = std::max(period, t.cycle);
  for (const auto& e : w.enemies())
    if (e.alive && e.kind == EnemyKind::DartFace)
      period = std::max(period, 30);
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
  // Level 9: the bonus patch wakes on a plate's third press.
  if (mTakeBonus && !mSkipBonus)
    for (const auto& d : w.secretDoors())
      if (d.bonus && !d.open && d.plate >= 0 && std::size_t(d.plate) < w.plates().size())
      {
        const Plate& pl = w.plates()[std::size_t(d.plate)];
        return {7, pl.x, pl.y - 1, 0, 0, d.plate};
      }
  if (mTakeBonus && !mSkipBonus)
    for (const auto& pr : w.props())
      if (pr.kind == PropKind::BonusDoor && !pr.used && !pr.dormant)
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
  // Level 9: a key door still wanting stone keys: fetch the nearest.
  for (const auto& d : w.keyDoors())
  {
    if (d.open || d.sink >= 0 || w.stoneKeysHeld() >= d.keys)
      continue;
    Goal best;
    int bestD = -1;
    for (const auto& k : w.stoneKeys())
    {
      const int dist = std::abs(k.x - p.x) + std::abs(k.y - p.y);
      if (!k.taken && (bestD < 0 || dist < bestD))
      {
        bestD = dist;
        best = {6, k.x, k.y + 1};
      }
    }
    if (bestD >= 0)
      return best;
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
  if (w.bounce())
    jumpH = 18; // Bounce House: up to 18 cells, held jump builds it

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
  // Level 9: a key door opens for a runner with every stone key, and the
  // collapsing floor counts as floor (the search finds out when it falls).
  for (const auto& d : w.keyDoors())
    if (!d.open && w.stoneKeysHeld() >= d.keys)
      for (int y = d.ty * kCellsPerTile; y < (d.ty + d.h) * kCellsPerTile; ++y)
        for (int x = d.tx * kCellsPerTile; x < (d.tx + 1) * kCellsPerTile; ++x)
          if (x >= 0 && y >= 0 && x < W && y < H)
            blocked[std::size_t(y * W + x)] = top[std::size_t(y * W + x)] = 0;
  for (const auto& c : w.collapseTiles())
    for (int y = c.ty * kCellsPerTile; y < (c.ty + 1) * kCellsPerTile; ++y)
      for (int x = c.tx * kCellsPerTile; x < (c.tx + 1) * kCellsPerTile; ++x)
        if (x >= 0 && y >= 0 && x < W && y < H)
          blocked[std::size_t(y * W + x)] = top[std::size_t(y * W + x)] = 1;
  // Level 10: sun doors that will open (the light is set up, or a Monk will
  // do it) and the ones the need test opens.
  for (std::size_t di = 0; di < w.sunDoors().size() && di < mDoorOpen.size(); ++di)
  {
    const SunDoor& d = w.sunDoors()[di];
    if (!mDoorOpen[di] || d.open)
      continue;
    for (int y = d.y * kCellsPerTile; y < (d.y + d.h) * kCellsPerTile; ++y)
      for (int x = d.x * kCellsPerTile; x < (d.x + d.w) * kCellsPerTile; ++x)
        if (x >= 0 && y >= 0 && x < W && y < H)
          blocked[std::size_t(y * W + x)] = top[std::size_t(y * W + x)] = 0;
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
  // Logs still hanging over the ravine: their landings are ground to the
  // field, and cutting their ropes (see heuristic) is how it gets there.
  for (const auto& l : w.loads())
    if (!l.cage && l.state < 2)
      for (int tx = l.landX0; tx <= l.landX1; ++tx)
        for (int x = tx * kCellsPerTile; x < (tx + 1) * kCellsPerTile; ++x)
          if (x >= 0 && x < W && l.landRow * kCellsPerTile < H)
            top[std::size_t(l.landRow * kCellsPerTile * W + x)] = 1;
  // Moving platforms: anywhere along their travel is somewhere to stand;
  // the search finds when they are actually there.
  // Light Trail: your feet draw the ground as you go.
  if (w.lightTrail())
    for (std::size_t i = 0; i < top.size(); ++i)
      top[i] = top[i] || !blocked[i];
  for (const auto& pl : w.platforms())
  {
    if (pl.hidden)
      continue; // a train not here yet
    int y0 = pl.y, y1 = pl.y;
    int x0 = pl.x, x1 = pl.x;
    if (pl.mode == PlatformMode::Pulley)
    {
      y0 = pl.homeY - pl.travel;
      y1 = pl.homeY + pl.travel;
    }
    else if ((pl.powered >= 0 && !w.breakers()[std::size_t(pl.powered)].on) || w.latchedPlatform(pl))
    {
      // A lift with no power, or a hook still on its latch, stays where it is.
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

  // Swing vines: anywhere the hands can be along a vine's swing is
  // somewhere to hold on, and near the top of a swing it launches you.
  struct Launch
  {
    int x, y, dir;
  };
  std::vector<Launch> launches;
  for (const auto& v : w.vines())
    for (int deg = -60; deg <= 60; deg += 2)
    {
      const float a = float(deg) * 3.14159265f / 180.0f;
      for (int at = std::max(2, v.len - 6); at <= v.len; ++at)
      {
        const float hx = float(v.ax) + float(at) * std::sin(a), hy = float(v.ay) + float(at) * std::cos(a);
        const int px = int(std::lround(hx - 1.5f)), py = int(std::lround(hy)) + 5;
        if (px < 0 || py < 0 || px >= W || py >= H || !valid[std::size_t(py * W + px)])
          continue;
        hang[std::size_t(py * W + px)] = 1;
        if (std::abs(deg) >= 30 && at == v.len)
          launches.push_back({px, py - 1, deg > 0 ? 1 : -1});
      }
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
  // The launch: 14 cells up, 2 a frame sideways, and still 2 a frame
  // sideways on the way down.
  static const int kArc[] = {3, 3, 2, 2, 2, 1, 1, 0, -1, -1, -2, -2, -2, -2, -2, -2};
  for (const auto& l : launches)
  {
    int x = l.x, y = l.y, f = 0;
    bool ok = true;
    auto step = [&](int dx, int dy) {
      if (x + dx < 0 || x + dx >= W || y + dy < 0 || y + dy >= H || !valid[std::size_t((y + dy) * W + x + dx)])
        return false;
      x += dx;
      y += dy;
      return true;
    };
    for (int dy : kArc)
    {
      ++f;
      for (int k = 0; k < 2 && ok; ++k)
        ok = step(l.dir, 0);
      for (int k = 0; k < std::abs(dy) && ok; ++k)
        ok = step(0, dy > 0 ? -1 : 1);
      if (!ok)
        break;
      const std::size_t j = std::size_t(y * W + x);
      list.push_back({node(l.x, l.y + 1, 0), node(x, y, 0), 4 + 3 * f + (hazard[j] == 1 ? 40 : 0)});
      if (dy < 0 && support[j] == 0)
        break; // landed
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
    : goal.kind == 5 && goal.w == 1       ? CellBox{goal.x - 2, goal.y, 6, 18}     // under it, to shoot up
    : goal.kind == 5                      ? CellBox{goal.x - 14, goal.y - 3, 30, 6} // in range for a level shot
    : goal.kind == 8 && goal.w > 0        ? CellBox{goal.x - 22, goal.y - 2, 18, 3} // on its floor, left of it
    : goal.kind == 8                      ? CellBox{goal.x + 6, goal.y - 2, 18, 3}  // on its floor, right of it
                                          : CellBox{goal.x, goal.y - 1, 2, 2};
  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x)
    {
      const std::size_t i = std::size_t(y * W + x);
      if (!valid[i])
        continue;
      if (!boxAt(x, y, 3, 5).intersects(goalBox))
        continue;
      if ((goal.kind == 0 || goal.kind == 4 || goal.kind == 5 || goal.kind == 8) && support[i] != 0)
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
  // A log's rope not yet behind you: wearing it through is progress (the
  // field already counts the log as down). Counted the same from anywhere
  // before it, so walking up to it never looks like a step back.
  for (const auto& rope : w.jungleRopes())
  {
    if (rope.cut || rope.load < 0)
      continue;
    const Load& l = w.loads()[std::size_t(rope.load)];
    if (!l.cage && (l.landX1 + 1) * kCellsPerTile + 40 > p.x)
      extra += 20 * std::max(0, rope.hp);
  }
  // A key door that will open: getting to it and waiting while it sinks.
  for (const auto& d : w.keyDoors())
    if (!d.open && w.stoneKeysHeld() >= d.keys)
      extra += d.sink < 0 ? 40 : 20 - std::min(d.sink, 20);
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

bool Planner::solveMirrors(const World& w, int door)
{
  // A depth-first search over the mirrors in the order a beam meets them:
  // each one the beam reaches gets every angle, cheapest turn first, until
  // the beam ends on the door.
  const auto& mirrors = w.mirrors();
  std::vector<int> angles(mirrors.size());
  for (std::size_t i = 0; i < mirrors.size(); ++i)
    angles[i] = mirrors[i].to;
  std::vector<char> fixed(mirrors.size(), 0);
  std::vector<int> order;
  std::function<bool(const SunBeam&)> dfs = [&](const SunBeam& b) {
    int stop = -1;
    const int r = w.traceBeamFor(b.x, b.y, b.dir, angles, fixed, &stop);
    if (r == door)
      return true;
    if (r != -2 || stop < 0)
      return false;
    const int now = mirrors[std::size_t(stop)].to;
    fixed[std::size_t(stop)] = 1;
    order.push_back(stop);
    for (int k : {0, 1, 7, 2, 6, 3, 5, 4})
    {
      angles[std::size_t(stop)] = (now + k) % 8;
      if (dfs(b))
        return true;
    }
    order.pop_back();
    fixed[std::size_t(stop)] = 0;
    angles[std::size_t(stop)] = now;
    return false;
  };
  for (std::size_t bi = 0; bi < w.sunBeams().size(); ++bi)
  {
    order.clear();
    std::fill(fixed.begin(), fixed.end(), 0);
    for (std::size_t i = 0; i < mirrors.size(); ++i)
      angles[i] = mirrors[i].to;
    if (dfs(w.sunBeams()[bi]))
    {
      mWant.assign(mirrors.size(), -1);
      for (int i : order)
        mWant[std::size_t(i)] = angles[std::size_t(i)];
      mOrder = order;
      mSolveBeam = int(bi);
      return true;
    }
  }
  mWant.assign(mirrors.size(), -1);
  mOrder.clear();
  mSolveBeam = -1;
  return false;
}

bool Planner::lightGoal(const World& w, Goal& g)
{
  const auto& doors = w.sunDoors();
  const auto& p = w.player();
  const int feet = p.y / kCellsPerTile;
  std::vector<char> target(doors.size(), 0); // a hatch the stone sun opens
  for (const auto& d : doors)
    if (d.opens >= 0)
      target[std::size_t(d.opens)] = 1;
  int door = -1;
  if (g.kind == 3)
  {
    // The bonus entrance down in the vault: the cracked disc over it first,
    // once you are up on its floor (until then, the way up comes first).
    int crack = -1;
    for (std::size_t i = 0; i < doors.size() && crack < 0; ++i)
    {
      const auto& d = doors[i];
      if (d.crack && !d.open && g.y / kCellsPerTile > d.y && std::abs(g.x / kCellsPerTile - d.x) < 8)
        crack = int(i);
    }
    if (crack < 0)
      return false;
    if (feet <= doors[std::size_t(crack)].y)
      door = crack;
    else
    {
      g = {};
      g.x = w.level().exitTx * kCellsPerTile;
      g.y = (w.level().exitTy + 1) * kCellsPerTile - 1;
    }
  }
  if (door < 0)
  {
    // Route doors at or under the runner's floor, lowest first; the first
    // one the exit cannot do without is the one to open.
    std::vector<int> cands;
    std::uint64_t key = 1;
    for (std::size_t i = 0; i < doors.size(); ++i)
    {
      const auto& d = doors[i];
      key = key * 3 + (d.open ? 2 : 0);
      if (d.open || !d.latch || d.crack || target[i] || d.y + d.h - 1 > feet + 1)
        continue;
      cands.push_back(int(i));
      key = key * 3 + 1;
    }
    std::sort(cands.begin(), cands.end(), [&](int a, int b) { return doors[std::size_t(a)].y > doors[std::size_t(b)].y; });
    if (key != mLightKey)
    {
      mLightKey = key;
      mLightDoor = -1;
      Goal exit = chooseGoal(w);
      exit.kind = 0;
      exit.x = w.level().exitTx * kCellsPerTile;
      exit.y = (w.level().exitTy + 1) * kCellsPerTile - 1;
      for (int c : cands)
      {
        mDoorOpen.assign(doors.size(), 0);
        for (int o : cands)
          if (o != c)
          {
            mDoorOpen[std::size_t(o)] = 1;
            if (doors[std::size_t(o)].opens >= 0)
              mDoorOpen[std::size_t(doors[std::size_t(o)].opens)] = 1;
          }
        buildField(w, exit);
        if (heuristic(w) >= kInf)
        {
          mLightDoor = c;
          break;
        }
      }
      mDoorOpen.assign(doors.size(), 0);
      mDist.clear(); // the real goal's field is built next
      static const bool debug = std::getenv("GR_PLANNER_DEBUG") != nullptr;
      if (debug)
        std::fprintf(stderr, "light: door %d needed (of %zu candidates)\n", mLightDoor, cands.size());
    }
    door = mLightDoor;
    if (door < 0)
      return false;
  }
  const SunDoor& d = doors[std::size_t(door)];
  // Solve the mirrors for it (again when a mirror or a door changed).
  std::uint64_t sk = std::uint64_t(door + 1);
  for (const auto& m : w.mirrors())
    sk = sk * 9 + std::uint64_t(m.to);
  for (const auto& od : doors)
    sk = sk * 2 + od.open;
  if (sk != mSolveKey)
  {
    mSolveKey = sk;
    mSolved = solveMirrors(w, door);
  }
  const int pending = d.opens >= 0 ? d.opens : door;
  if (mSolved)
  {
    // The next mirror the beam meets that is not set yet.
    for (int i : mOrder)
    {
      const Mirror& m = w.mirrors()[std::size_t(i)];
      if (m.to == mWant[std::size_t(i)])
        continue;
      const int k = ((mWant[std::size_t(i)] - m.to) % 8 + 8) % 8;
      const int dir = k <= 4 ? 1 : -1;
      g = {8, m.x * kCellsPerTile, (m.y + 2) * kCellsPerTile - 1, dir, (m.to + dir + 8) % 8, i};
      return true;
    }
    // All set: moths in the way of the beam come off it.
    if (mSolveBeam >= 0 && std::size_t(mSolveBeam) < w.beamPaths().size())
    {
      const BeamPath& path = w.beamPaths()[std::size_t(mSolveBeam)];
      if (!path.pts.empty())
      {
        const auto [ex, ey] = path.pts.back();
        const bool atDoor = ex / kCellsPerTile >= d.x - 1 && ex / kCellsPerTile <= d.x + d.w &&
          ey / kCellsPerTile >= d.y - 1 && ey / kCellsPerTile <= d.y + d.h;
        int best = -1, bestD = 6 * 6 * 4;
        for (std::size_t i = 0; i < w.enemies().size() && !atDoor; ++i)
        {
          const Enemy& e = w.enemies()[i];
          if (!e.alive || e.kind != EnemyKind::Moth)
            continue;
          const int dd = (e.x + 1 - ex) * (e.x + 1 - ex) + (e.y - ey) * (e.y - ey);
          if (dd < bestD)
          {
            bestD = dd;
            best = int(i);
          }
        }
        if (best >= 0)
        {
          const Enemy& e = w.enemies()[std::size_t(best)];
          g = {5, e.x / 2 * 2, e.y / 2 * 2, 1, 0, best};
          return true;
        }
      }
    }
  }
  // Nothing to turn (it is opening, or a Monk will reflect the light onto
  // it): go to it and wait. The field takes it, and the route doors after
  // it, as open.
  mPendingDoor = pending;
  mRouteOpen.assign(doors.size(), 0);
  for (std::size_t i = 0; i < doors.size(); ++i)
    if (!doors[i].open && doors[i].latch && !doors[i].crack)
      mRouteOpen[i] = 1; // (a hatch the stone sun opens included)
  mRouteOpen[std::size_t(pending)] = 1;
  return true;
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
  if ((goal.kind == 3 || goal.kind == 7) && mGoalKind == goal.kind && mFails >= 6)
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
    // Level 10: a sun door in the way: turn a mirror, clear moths off the
    // beam, or go and wait for it.
    mPendingDoor = -1;
    if (!world.sunDoors().empty() && (goal.kind == 0 || goal.kind == 3))
      lightGoal(world, goal);
    mDoorOpen.assign(world.sunDoors().size(), 0);
    if (mPendingDoor >= 0)
      mDoorOpen = mRouteOpen;
    // The field also changes when a wall breaks or the Bass Cannon arrives.
    int broken = 0;
    for (const auto& b : world.breakables())
      broken += b.broken;
    const bool sound = world.player().weapon == Weapon::Proto && world.player().proto == int(ProtoId::BassCannon);
    int powered = 0;
    for (const auto& b : world.breakers())
      powered = powered * 2 + b.on;
    // Level 8: rope bridges that fell, logs that landed.
    for (const auto& b : world.bridges())
      powered = powered * 2 + b.down;
    for (const auto& l : world.loads())
      powered = powered * 3 + std::min(l.state, 2);
    // Level 9: stone keys, the key door, secret walls a plate opened.
    powered = powered * 5 + world.stoneKeysHeld();
    for (const auto& d : world.keyDoors())
      powered = powered * 2 + d.open;
    for (const auto& d : world.secretDoors())
      powered = powered * 2 + d.open;
    // Level 10: sun doors, and the one the field takes as opening.
    for (const auto& d : world.sunDoors())
      powered = powered * 2 + d.open;
    powered = powered * 31 + mPendingDoor + 1;
    const int keyHash = goal.kind * 1000000 + goal.x * 1000 + goal.y + (world.player().hasKey ? 500000000 : 0) +
      (std::min(broken, 15) * 2 + (sound ? 1 : 0)) * 10000000 + powered * 7919;
    if (mDist.empty() || keyHash != mGoalKeyHash)
    {
      buildField(world, goal);
      mGoalKeyHash = keyHash;
      mGoalKind = goal.kind;
      mGoalIndex = goal.index;
    }
    if ((goal.kind != 2 && goal.kind != 3 && goal.kind != 7) || heuristic(world) < kInf)
      break;
    if (goal.kind == 3 || goal.kind == 7)
    {
      mSkipBonus = true;
      mSkipBonusAt = -1;
      goal = chooseGoal(world);
      continue;
    }
    mSkipProto = true;
    mSkipHadKey = world.player().hasKey;
  }
  // Level 10: at a door that is about to open (or that a Monk will open),
  // stand and wait.
  if (mPendingDoor >= 0 && std::size_t(mPendingDoor) < world.sunDoors().size())
  {
    const SunDoor& d = world.sunDoors()[std::size_t(mPendingDoor)];
    const CellBox near{d.x * kCellsPerTile - 8, d.y * kCellsPerTile - 8, d.w * kCellsPerTile + 16,
      d.h * kCellsPerTile + 16};
    if (!d.open && near.intersects(p0.box()) && stable(world))
    {
      for (int f = 0; f < 8; ++f)
        mQueue.push_back(Input{});
      return;
    }
  }

  struct Node
  {
    std::unique_ptr<World> w;
    int parent;
    int macro;
    int g;
    int h;
    Input last;
    int frames;             // how long its macro ran
    std::vector<Input> seq; // a vine macro's inputs
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
    for (const auto& t : w.trail())
      k = mix(k, std::uint64_t(t.x) | (std::uint64_t(t.y) << 16));
    // The hunter: where its spot is and how far along a lock or salvo is.
    // Level 8: the vines close by (where they are in their swing, how high
    // they go), the bridges, the ropes and what hangs from them.
    for (const auto& v : w.vines())
      if (std::abs(v.ax - p.x) < 48)
        k = mix(k, std::uint64_t(v.t) | (std::uint64_t(v.amp) << 8) | (std::uint64_t(v.cool) << 16) |
                     (std::uint64_t(v.pump) << 24));
    if (p.state == PlayerState::Swing)
      k = mix(k, std::uint64_t(p.vine) | (std::uint64_t(p.vineAt) << 8));
    k = mix(k, std::uint64_t(p.fling + 4) | (std::uint64_t(p.vineArc) << 4));
    for (const auto& b : w.bridges())
      k = mix(k, std::uint64_t(b.down) | (std::uint64_t(b.chops) << 1) | (std::uint64_t(std::min(b.heavy, 63)) << 4) |
                   (std::uint64_t(b.left + 1) << 12) | (std::uint64_t(b.creak) << 24));
    for (const auto& rope : w.jungleRopes())
      k = mix(k, std::uint64_t(rope.hp) | (std::uint64_t(rope.cut) << 8));
    for (const auto& l : w.loads())
      k = mix(k, std::uint64_t(l.state) | (std::uint64_t(l.fall) << 4));
    // Level 9: traps close by, the floor cracking, keys and plates, and the
    // Guardians and beetles near you.
    for (const auto& t : w.traps())
    {
      const CellBox r = t.reach();
      if (std::abs(r.x - p.x) < 80 || (t.kind == TrapKind::Stone && p.x > t.x0 - 40 && p.x < t.x1 + 40))
        k = mix(k, std::uint64_t(t.state) | (std::uint64_t(t.t) << 4) | (std::uint64_t(t.sx) << 16));
    }
    for (const auto& c : w.collapseTiles())
      if (c.state != 0 && std::abs(c.tx * kCellsPerTile - p.x) < 40)
        k = mix(k, std::uint64_t(c.tx) | (std::uint64_t(c.state) << 12) | (std::uint64_t(c.t) << 16));
    if (!w.plates().empty() || !w.stoneKeys().empty())
    {
      std::uint64_t pk = std::uint64_t(w.stoneKeysHeld());
      for (const auto& pl : w.plates())
        pk = pk * 7 + std::uint64_t(std::min(pl.presses, 3) * 2 + pl.down);
      for (const auto& d : w.keyDoors())
        pk = pk * 31 + std::uint64_t(d.sink + 1);
      k = mix(k, pk);
    }
    for (const auto& e : w.enemies())
      if (e.alive && (e.kind == EnemyKind::Guardian || e.kind == EnemyKind::Scarabs) && std::abs(e.x - p.x) < 40 &&
          std::abs(e.y - p.y) < 16)
        k = mix(k, std::uint64_t(e.x) | (std::uint64_t(e.hp) << 16) | (std::uint64_t(e.tell + e.dive * 32) << 24) |
                     (std::uint64_t(e.tangle) << 40) | (std::uint64_t(e.dir > 0) << 48));
    // Level 10: mirrors (where they are turning to), sun doors (how long
    // lit), and the Monks, Wraiths and moths close by.
    for (const auto& m : w.mirrors())
      k = mix(k, std::uint64_t(m.to) | (std::uint64_t(m.turn) << 4));
    for (const auto& d : w.sunDoors())
      k = mix(k, std::uint64_t(d.open) | (std::uint64_t(std::min(d.lit, 15)) << 1) | (std::uint64_t(d.hold) << 8));
    for (const auto& e : w.enemies())
      if (e.alive && (e.kind == EnemyKind::Monk || e.kind == EnemyKind::Wraith || e.kind == EnemyKind::Moth) &&
          std::abs(e.x - p.x) < 40 && std::abs(e.y - p.y) < 24)
        k = mix(k, std::uint64_t(e.x) | (std::uint64_t(e.y) << 12) | (std::uint64_t(e.hp) << 24) |
                     (std::uint64_t(e.tell + e.dive * 16 + e.attach * 256) << 32) | (std::uint64_t(e.aimX) << 48) |
                     (std::uint64_t(e.dir > 0) << 60));
    if (const auto& h = w.hunter(); h.on)
    {
      k = mix(k, std::uint64_t(int(h.sx)) | (std::uint64_t(int(h.sy)) << 16) |
                   (std::uint64_t(std::min(h.locking, h.lock)) << 32) | (std::uint64_t(h.beep) << 40) |
                   (std::uint64_t(h.left) << 48) | (std::uint64_t(h.cool > 0) << 56));
      for (const auto& st : w.strikes())
        k = mix(k, std::uint64_t(st.x) | (std::uint64_t(st.y) << 16) | (std::uint64_t(st.t) << 32));
    }
    return k;
  };
  const int h0 = heuristic(world);
  const int hp0 = p0.hp;
  auto score = [&](const Node& n) { return n.g + n.h; };
  auto cmp = [&](int a, int b) { return score(nodes[std::size_t(a)]) > score(nodes[std::size_t(b)]); };
  std::priority_queue<int, std::vector<int>, decltype(cmp)> open(cmp);
  std::unordered_map<std::uint64_t, int> seen;

  nodes.push_back({world.cloneForSim(), -1, -1, 0, h0, mPrev, 0, {}});
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
    const bool keyTaken = goal.kind == 6 && nw.stoneKeysHeld() > world.stoneKeysHeld();
    const bool pressed = goal.kind == 7 && std::size_t(goal.index) < nw.plates().size() &&
      nw.plates()[std::size_t(goal.index)].presses > world.plates()[std::size_t(goal.index)].presses;
    const bool turned = goal.kind == 8 && std::size_t(goal.index) < nw.mirrors().size() &&
      nw.mirrors()[std::size_t(goal.index)].to == goal.h;
    const bool success = nw.state() != WorldState::Playing || (goal.kind == 1 && np.hasKey) || keyTaken || pressed || turned ||
      (goal.kind == 2 && gotProto) || (goal.kind == 3 && nw.bonusRequested()) || thrown || leechGone;
    if (ni != 0 && success)
    {
      found = ni;
      break;
    }
    if (ni != 0 && stable(nw) && np.hp >= hp0 - 1 && !nw.trainDanger())
    {
      const bool done = n.h <= h0 - kProgress;
      const bool better = bestStable < 0 || n.h < nodes[std::size_t(bestStable)].h;
      if ((done || better) && safeAhead(nw))
      {
        if (done)
        {
          found = ni;
          break;
        }
        bestStable = ni;
      }
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
      const bool vine = m == kVineRight || m == kVineLeft;
      std::vector<Input> seq;
      int launchedAt = -1;
      int ran = 0;
      for (int f = 0; vine || f < frames; ++f)
      {
        bool done = false, fail = false;
        const Input in =
          vine ? vineInput(*child, m == kVineRight ? 1 : -1, f, launchedAt, done, fail) : macroInput(kMacros[m], f);
        if (fail)
        {
          dead = true;
          break;
        }
        if (done)
          break;
        if (vine)
          seq.push_back(in);
        child->update(toPlayerInput(in, prev));
        prev = in;
        ++ran;
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
      const int g = n.g + (m == kWaitLaunch ? 8 : (m == kWaitVerse ? frames / 2 : (vine ? ran / 2 : frames))) + lost * 60 +
        (m >= 10 && m <= 12 ? 1 : 0);
      const auto it = seen.find(key);
      if (it != seen.end() && it->second <= g)
        continue;
      seen[key] = g;
      const int h = child->state() != WorldState::Playing ? 0 : heuristic(*child);
      if (h >= kInf)
        continue;
      nodes.push_back({std::move(child), ni, m, g, h, prev, vine ? ran : frames, std::move(seq)});
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
  {
    const Node& n = nodes[std::size_t(i)];
    if (!n.seq.empty())
      mQueue.insert(mQueue.end(), n.seq.begin(), n.seq.end());
    else
      for (int f = 0; f < n.frames; ++f)
        mQueue.push_back(macroInput(kMacros[n.macro], f));
  }
}

} // namespace gr
