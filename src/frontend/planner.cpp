#include "frontend/planner.hpp"

#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <functional>
#include <limits>
#include <optional>
#include <queue>
#include <unordered_map>

namespace gr
{

namespace
{

constexpr int kAirBudget = 12;     // cells of sideways movement in one jump
constexpr int kInf = std::numeric_limits<int>::max() / 4;
constexpr int kMacroFrames = 4;
constexpr int kBudget = Planner::kNodeBudget; // nodes expanded per plan
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
constexpr int kMacroCount = kMacroCountBase + 11;
constexpr int kLongWait = 15;
constexpr int kWaitLaunch = 16;
constexpr int kWaitVerse = 17;
// Swing vines (level 8): pump the vine you hold, then launch at the top of
// a swing to the right or to the left. Their inputs are worked out as they
// run (see rollMacro), not taken from kMacros.
constexpr int kVineRight = kMacroCountBase;
constexpr int kVineLeft = kMacroCountBase + 1;
// Blasting Caps (level 11): throw one at a cracked rock wall (or set one
// down, crouched), back off out of its blast and wait for the bang.
constexpr int kCapRight = kMacroCountBase + 2;
constexpr int kCapLeft = kMacroCountBase + 3;
constexpr int kCapSetRight = kMacroCountBase + 4;
constexpr int kCapSetLeft = kMacroCountBase + 5;
bool capMacro(int m) { return m >= kCapRight && m <= kCapSetLeft; }
// Serpent Spears (level 12): jump beside a wall and throw at the top of the
// jump; the spear sticks as a foothold to land on, a jump higher.
constexpr int kSpearRight = kMacroCountBase + 6;
constexpr int kSpearLeft = kMacroCountBase + 7;
bool spearMacro(int m) { return m == kSpearRight || m == kSpearLeft; }
// Level 27: jump straight up and fire at frame 2, 4 or 6 of the jump (a Crow
// on a ledge, out of reach of a shot from the ground).
constexpr int kJumpShot = kMacroCountBase + 8;
bool jumpShotMacro(int m) { return m >= kJumpShot && m < kJumpShot + 3; }
Input jumpShotInput(int m, int f, bool& done)
{
  Input in;
  in.jump = f < 4;
  in.fire = f == 2 + (m - kJumpShot) * 2;
  done = f >= 10;
  return in;
}

bool spearsHeld(const World& w)
{
  const auto& p = w.player();
  return p.weapon == Weapon::Proto && p.proto == int(ProtoId::SerpentSpear) && p.ammo > 0;
}

// Standing with a wall face right beside you that a spear thrown at the top
// of a jump would stick in.
bool spearUseful(const World& w, int dir)
{
  const auto& p = w.player();
  if (!spearsHeld(w) || p.state != PlayerState::OnGround || p.cart >= 0)
    return false;
  const int face = dir > 0 ? p.x + Player::kWidth : p.x - 1;
  const auto& map = w.map();
  return map.solid(face, p.y - 2) && map.solid(face, p.y - 6) && !map.solid(dir > 0 ? p.x + 2 : p.x, p.y - 6);
}

// A spear step's input this frame: jump beside the wall, throw at the top,
// land on the foothold.
Input spearInput(const World& w, int m, int f, bool& done, bool& fail)
{
  Input in;
  const auto& p = w.player();
  const int dir = m == kSpearRight ? 1 : -1;
  in.right = dir > 0;
  in.left = dir < 0;
  if (f == 0)
  {
    in.jump = true;
    return in;
  }
  if (p.state == PlayerState::Dying || f > 36)
  {
    fail = true;
    return in;
  }
  if (p.state == PlayerState::OnGround)
  {
    done = true;
    return in;
  }
  if (p.state == PlayerState::Jumping)
  {
    in.jump = true;
    const auto& arc = w.runnerJumpArc();
    const bool apex = p.frames >= int(arc.size()) || arc[std::size_t(p.frames)] == 0;
    in.fire = apex && p.shotCooldown == 0 && p.facing == dir;
  }
  return in;
}

// Whether there is a cracked rock wall close enough to be worth a cap.
bool capsUseful(const World& w)
{
  const auto& p = w.player();
  if (p.weapon != Weapon::Proto || p.proto != int(ProtoId::BlastingCaps) || p.ammo <= 0 || !w.caps().empty() ||
      p.cart >= 0 || p.state != PlayerState::OnGround)
    return false;
  for (const auto& b : w.breakables())
    if (!b.broken && b.by == 1 &&
        CellBox{b.x0 * kCellsPerTile - 14, b.y0 * kCellsPerTile - 10, (b.x1 - b.x0 + 1) * kCellsPerTile + 28,
          (b.y1 - b.y0 + 1) * kCellsPerTile + 20}
          .intersects(p.box()))
      return true;
  return false;
}
// Frames a macro runs for in this world, 0 if it does not apply.
int macroFrames(int m, const World& w)
{
  // Level 30: crouched, the Harpoon Line plants a harpoon (Bot::trench does that).
  // Other shots only at an enemy close by: harpoons in the walls string stray lines.
  if (m < kMacroCountBase && kMacros[m].fire && w.player().weapon == Weapon::Proto &&
      w.player().proto == int(ProtoId::RedPen))
  {
    // Level 38: the Red Pen only at an enemy close by; Bot::writers strikes
    // the words (a stray shot could strike a FLOOR out from under you).
    const auto& p = w.player();
    bool near = false;
    for (const auto& e : w.enemies())
      near = near || (e.alive && !e.hidden && e.kind != EnemyKind::WritersBlock && std::abs(e.x - p.x) < 24 &&
                       std::abs(e.y - p.y) < 10);
    if (!near)
      return 0;
  }
  if (m < kMacroCountBase && kMacros[m].fire && w.player().weapon == Weapon::Proto &&
      w.player().proto == int(ProtoId::HarpoonLine))
  {
    if (kMacros[m].down)
      return 0;
    const auto& p = w.player();
    bool near = false;
    for (const auto& e : w.enemies())
      near = near || (e.alive && !e.hidden && std::abs(e.x - p.x) < 24 && std::abs(e.y - p.y) < 10);
    if (!near)
      return 0;
  }
  if (m == kVineRight || m == kVineLeft)
    return w.player().state == PlayerState::Swing && w.player().vine >= 0 ? 1 : 0; // the real length comes out of the run
  if (capMacro(m))
    return capsUseful(w) ? 1 : 0;
  if (spearMacro(m))
    return spearUseful(w, m == kSpearRight ? 1 : -1) ? 1 : 0;
  if (jumpShotMacro(m))
  {
    // Only with a Crow close by.
    for (const auto& e : w.enemies())
      if (e.alive && e.kind == EnemyKind::Crow && std::abs(e.x - w.player().x) < 24 && std::abs(e.y - w.player().y) < 16)
        return 1;
    return 0;
  }
  if (m == kLongWait)
  {
    bool opening = false;
    for (const auto& d : w.doors())
      opening = opening || (d.solid && d.breaker >= 0 && w.breakers()[std::size_t(d.breaker)].on);
    bool tide = false;
    for (const auto& f : w.fluids())
      tide = tide || f.tide;
    // Level 45: a mouth that only opens while the hive breathes in.
    for (const auto& t : w.gulletTubes())
      tide = tide || t.breath;
    // Level 46: a crystal drifting into line.
    for (const auto& c : w.swapCrystals())
      tide = tide || (c.drifting && !c.gone);
    // Level 47: a Silk Line being spun (or about to be).
    for (const auto& l : w.silkLines())
      tide = tide || l.spin || l.spinning || l.regrow > 0;
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

// Level 27: alarms raised, plus one on its way (a Crow's head turning, the
// "?" half full).
int alarmHeat(const World& w)
{
  const auto& n = w.noir();
  if (!n.on)
    return 0;
  bool coming = n.alarm == 0 && n.suspicion >= kSpotFill / 2;
  for (const auto& e : w.enemies())
    coming = coming || (e.alive && e.kind == EnemyKind::Crow && e.tell > 0);
  return n.alarms + (coming ? 1 : 0);
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
    (p.state == PlayerState::Swing && p.silk < 0) || p.state == PlayerState::Cling;
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

// A cap macro's input this frame: face the way, throw (or crouch and set
// it down), then keep 9 cells from it until it has gone off.
Input capInput(const World& w, int m, int f, bool& done, bool& fail)
{
  Input in;
  const auto& p = w.player();
  const int dir = (m == kCapRight || m == kCapSetRight) ? 1 : -1;
  const bool set = m == kCapSetRight || m == kCapSetLeft;
  if (f == 0)
  {
    // Turn to face it first (a tap that hardly moves you).
    in.right = dir > 0 && p.facing < 0;
    in.left = dir < 0 && p.facing > 0;
    in.down = set;
    return in;
  }
  if (f == 1 || (set && f == 2))
  {
    in.down = set;
    in.fire = f == (set ? 2 : 1);
    return in;
  }
  if (p.state == PlayerState::Dying || f > 60)
  {
    fail = true;
    return in;
  }
  if (w.caps().empty())
  {
    done = true;
    return in;
  }
  const auto& c = w.caps().front();
  const float mid = float(p.x) + 1.5f;
  if (std::abs(c.fx - mid) < 9.0f)
  {
    in.right = c.fx < mid;
    in.left = !in.right;
  }
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
  // Level 45: the hive breathes (its breathing mouths) and its heart beats.
  for (const auto& t : w.gulletTubes())
    if (t.breath)
      period = std::max(period, World::kBreathPeriod);
  // Level 32: the dishes' cycle; Heatstroke's swaps.
  if (w.dish().on)
    period = std::max(period, kDishCycle);
  if (w.dish().swapLr)
    period = std::max(period, 2 * kSwapEvery);
  // Level 46: crystals drifting back and forth.
  for (const auto& c : w.swapCrystals())
    if (c.drifting)
      period = std::max(period, 2 * std::max(1, int(c.path.size()) - 1) * c.speed);
  return period;
}

} // namespace

Planner::Goal Planner::cryptGoal(const World& w, bool& inCrypt)
{
  inCrypt = false;
  const auto& m = w.manor();
  const auto& p = w.player();
  int door = -1;
  for (std::size_t i = 0; i < w.props().size(); ++i)
    if (w.props()[i].kind == PropKind::BonusDoor && !w.props()[i].dormant)
      door = int(i);
  if (!m.on || door < 0 || m.split.on)
    return {};
  const bool used = w.props()[std::size_t(door)].used;
  for (std::size_t i = 0; i < m.mirrors.size(); ++i)
  {
    const auto& bm = m.mirrors[i];
    if (bm.kind != MirrorKind::Broken || bm.to < 0)
      continue;
    const auto& cm = m.mirrors[std::size_t(bm.to)];
    auto at = [&](const ManorMirror& mi, int index) {
      return Goal{18, mi.x * kCellsPerTile, (mi.y + 3) * kCellsPerTile - 5, 4, 5, index};
    };
    const int cryptFloor = (cm.y + 3) * kCellsPerTile - 1;
    if (p.x / kCellsPerTile < cm.x + 3 && std::abs(p.y - cryptFloor) <= 3)
    {
      if (used)
        return at(cm, bm.to);
      inCrypt = true;
      return {};
    }
    const int floor = (bm.y + 3) * kCellsPerTile - 1;
    if (!used && std::abs(p.y - floor) <= 3)
      return at(bm, int(i));
  }
  return {};
}

Planner::Goal Planner::towerGoal(const World& w) const
{
  const auto& t = w.tower();
  const auto& p = w.player();
  const TowerDoor* crawl = nullptr;
  const TowerDoor* back = nullptr;
  for (const auto& d : t.doors)
    (d.id == "crawl" ? crawl : back) = &d;
  const Prop* bonus = nullptr;
  for (const auto& pr : w.props())
    if (pr.kind == PropKind::BonusDoor && !pr.used && !pr.dormant)
      bonus = &pr;
  const bool want = bonus && mTakeBonus && !mSkipBonus;
  if (back && std::abs(p.y - back->y) <= 4 && std::abs(w.towerDx(back->x, p.x)) < 60)
  {
    // In the crawl space: the entrance, then the door back out.
    if (want)
      return {3, bonus->x, bonus->y, bonus->w, bonus->h};
    return {18, back->x, back->y - 4, 2, 5, -1};
  }
  if (want && crawl && std::abs(p.y - crawl->y) <= 30)
  {
    if (std::getenv("GR_TOWER_DEBUG"))
      std::fprintf(stderr, "tower goal f%d: crawl door %d,%d\n", w.stats().frames, crawl->x, crawl->y);
    return {18, crawl->x, crawl->y - 4, 2, 5, -1};
  }
  // The Satellite Swarm's boxes beside the way.
  if (!mSkipProto)
    for (const auto& b : w.boxes())
      if (b.alive && b.content == ItemKind::Proto && std::abs(w.towerDx(p.x, b.x)) <= 24 && std::abs(b.y - p.y) <= 8 &&
          (p.weapon != Weapon::Proto || p.ammo < protoDef(std::max(0, protoIndex(w.level().weapon))).maxAmmo))
        return {2, b.x, b.y};
  const int n = int(t.route.size());
  if (n == 0)
    return {};
  int next = t.routeAt + 1;
  if (next >= n)
  {
    if (t.titan.on && t.titan.phase != TitanPhase::Done && p.y > t.titan.deckY + 1)
      next = n - 1; // fallen off the deck mid-fight: back up
    else
      return {};
  }
  const auto& wp = t.route[std::size_t(next)];
  if (std::getenv("GR_TOWER_DEBUG"))
    std::fprintf(stderr, "tower goal f%d: route %d/%d at %d,%d (runner %d,%d rot %d)\n", w.stats().frames, next, n, wp.x,
      wp.y, p.x, p.y, t.rot);
  return {18, wp.x - 1, wp.y - 4, 4, 5, -1};
}

// Level 36: the first step on the warehouse route that is not done yet.
int warehouseStep(const World& w)
{
  const auto& s = w.warehouse();
  for (int i = s.routeAt; i < int(s.route.size()); ++i)
    if (!w.warehouseStepDone(i))
      return i;
  return -1;
}

// Throw Everything: a loose block the bot may lift (not one of the stacks
// it is building).
bool warehouseLiftable(const World& w, int bx, int by)
{
  const auto& s = w.warehouse();
  if (w.map().block(bx, by) != Tile::Solid)
    return false;
  for (const auto& st : s.route)
    if (st.act == "stack" && std::atoi(st.arg.c_str()) == bx)
      return false;
  for (const auto& [lx, ly] : s.loose)
    if (lx == bx && ly == by)
      return true;
  return false;
}

Planner::Goal Planner::warehouseGoal(const World& w) const
{
  const auto& s = w.warehouse();
  const int i = warehouseStep(w);
  if (i < 0)
    return {};
  const auto& st = s.route[std::size_t(i)];
  if (st.act == "stack")
  {
    // Carrying: to the throw spot (the throw lands six blocks on). Else
    // beside the nearest loose block on the step's floor, close enough to
    // lift it: the runner's feet row or one of the two rows above.
    if (s.carry >= 0 || s.liftT > 0)
      return {18, st.x, st.y - 4, 2, 5, -1};
    const auto& p = w.player();
    const auto& m = w.map();
    const int floor = st.y / kCellsPerTile;
    auto standable = [&](int x) {
      for (int cy = st.y - 4; cy <= st.y; ++cy)
        for (int cx = x; cx < x + 3; ++cx)
          if (m.solid(cx, cy))
            return false;
      return m.solidTop(x, st.y + 1) || m.solidTop(x + 1, st.y + 1) || m.solidTop(x + 2, st.y + 1);
    };
    Goal best;
    int bestD = -1;
    for (const auto& [bx, by] : s.loose)
    {
      if (by < floor - 2 || by > floor || !warehouseLiftable(w, bx, by))
        continue;
      for (int x : {bx * kCellsPerTile + 2, bx * kCellsPerTile - 3}) // right of it, left of it
      {
        if (!standable(x))
          continue;
        const int d = std::abs(x - st.x); // nearest the throw spot: a goal that holds still
        if (bestD < 0 || d < bestD)
        {
          bestD = d;
          best = {18, x + 1, st.y - 4, 1, 5, -1};
        }
      }
    }
    if (std::getenv("GR_WARE_DEBUG"))
      std::fprintf(stderr, "ware goal f%d: step %d lift goal %d,%d (runner %d,%d)\n", w.stats().frames, i, best.x, best.y,
        p.x, p.y);
    return best;
  }
  if (st.act == "push")
    for (const auto& c : s.crates)
      if (c.id == st.arg)
      {
        // Behind the crate on its floor, a few cells back.
        const int y = c.y + kCrateSize - 1;
        return {18, c.x - 9, y - 4, 3, 5, -1};
      }
  return {18, st.x - 1, st.y - 4, 4, 5, -1};
}

// Level 37: the first step on the wardrobe route that is not done yet.
int wardrobeStep(const World& w)
{
  const auto& s = w.wardrobe();
  for (int i = s.routeAt; i < int(s.route.size()); ++i)
    if (!w.wardrobeStepDone(i))
      return i;
  return -1;
}

// Level 38: the first step on the writers' route that is not done yet.
int writersStep(const World& w)
{
  const auto& s = w.writers();
  for (int i = s.routeAt; i < int(s.route.size()); ++i)
    if (!w.writersStepDone(i))
      return i;
  return -1;
}

int galleryStep(const World& w)
{
  const auto& s = w.gallery();
  for (int i = s.routeAt; i < int(s.route.size()); ++i)
    if (!w.galleryStepDone(i))
      return i;
  return -1;
}

Planner::Goal Planner::galleryGoal(const World& w) const
{
  const int i = galleryStep(w);
  if (i < 0)
    return {};
  const auto& st = w.gallery().route[std::size_t(i)];
  return {18, st.x - 2, st.y - 4, 4, 5, -1};
}

int flyTowerStep(const World& w)
{
  const auto& s = w.flyTower();
  for (int i = s.routeAt; i < int(s.route.size()); ++i)
    if (!w.flyTowerStepDone(i))
      return i;
  return -1;
}

Planner::Goal Planner::flyTowerGoal(const World& w) const
{
  const int i = flyTowerStep(w);
  if (i < 0)
    return {};
  const auto& st = w.flyTower().route[std::size_t(i)];
  return {18, st.x - 2, st.y - 4, 4, 5, -1};
}

// Level 41: for the bonus, the letter G in the hollow under the stands (the
// bonus door wakes once it is taken). In Paintball the nearest unpainted
// block top (kind 12, like Golden Touch's marked blocks).
Planner::Goal Planner::sweepsGoal(const World& w) const
{
  const auto& s = w.sweeps();
  const auto& p = w.player();
  if (s.paint)
  {
    const int bw = w.level().width;
    int best = -1, bestKey = 0;
    for (int ty = 1; ty < w.level().height; ++ty)
      for (int tx = 0; tx < bw; ++tx)
      {
        const int id = ty * bw + tx;
        if (s.paints[std::size_t(id)] != 1 ||
            std::find(mPaintSkip.begin(), mPaintSkip.end(), id) != mPaintSkip.end())
          continue;
        const int key = std::abs(tx * kCellsPerTile - p.x) + 3 * std::abs(ty * kCellsPerTile - 1 - p.y);
        if (best < 0 || key < bestKey)
        {
          best = id;
          bestKey = key;
        }
      }
    if (best >= 0)
      return {12, (best % bw) * kCellsPerTile, (best / bw) * kCellsPerTile - 1, 2, 1, best};
    return {};
  }
  if (!mTakeBonus || mSkipBonus || w.stats().letters.find('G') != std::string::npos)
    return {};
  for (const auto& it : w.items())
    if (!it.taken && it.kind == ItemKind::LetterG && std::abs(it.x - p.x) <= 40)
      return {18, it.x - 1, it.y - 4, 4, 5, -1};
  return {};
}

// Level 42: the route's next step (the station's floor plate, its ceiling
// plate, the studio's first monitor; Bot::finale waits on the plates and
// presses up at the monitor); in Off the Air, the console beside the
// switch's glass cover until it is broken.
Planner::Goal Planner::finaleGoal(const World& w) const
{
  const auto& f = w.finale();
  if (f.reel)
    return {};
  const auto& b = f.boss;
  if (b.on && b.phase == FnPhase::OffAir && b.coverHp > 0)
  {
    const CellBox c = finaleCoverBox(w);
    return {18, c.x - 3, c.bottom() - 4, 2, 5, -1};
  }
  for (int i = 0; i < int(f.route.size()); ++i)
  {
    if (w.finaleStepDone(i))
      continue;
    const auto& st = f.route[std::size_t(i)];
    // A fuse on the way to it comes first (the Level 22 fuse goal below).
    const auto& ws = w.west();
    const int px = w.player().x / kCellsPerTile;
    for (const auto& fu : ws.fuses)
      if (fu.bot && !fu.lit() && !fu.cells.empty() && fu.cells[0].x < st.x / kCellsPerTile && px <= fu.cells[0].x + 2 &&
          !(fu.barrel >= 0 && (ws.barrels[std::size_t(fu.barrel)].blown || ws.barrels[std::size_t(fu.barrel)].t >= 0)))
        return {};
    if (st.act == "ceiling")
      return {18, st.x - 1, st.y - 1 - 4, 4, 5, -1}; // hanging from the slab: y is the feet's row at the top
    if (st.act == "hop")
      return {18, st.x, st.y - 4, 3, 5, -1}; // the middle on the monitor's bottom row
    return {18, st.x - 1, st.y - 4, 4, 5, -1};
  }
  // Into the arena (the show starts behind the ON AIR door; Bot::fightFinale fights it).
  if (b.on && b.phase != FnPhase::OffAir && b.phase != FnPhase::Switched && w.player().x < b.arenaX0 + 4 &&
      w.player().x >= b.arenaX0 - 80)
    return {18, b.arenaX0 + 6, b.floorY - 4, 4, 5, -1};
  return {};
}

Planner::Goal Planner::writersGoal(const World& w) const
{
  const auto& s = w.writers();
  const int i = writersStep(w);
  if (i < 0)
    return {};
  const auto& st = s.route[std::size_t(i)];
  // Low on Red Pen: a pen box (or what fell out of one) on this floor first.
  const auto& p = w.player();
  if (p.weapon != Weapon::Proto || p.ammo < 6)
  {
    const int floor = w.writersFloor(p.y);
    Goal best;
    int bestD = 60;
    for (const auto& b : w.boxes())
    {
      const int d = std::abs(b.x - p.x) + std::abs(b.y - p.y);
      if (b.alive && b.content == ItemKind::Proto && d < bestD && w.writersFloor(b.y) == floor)
      {
        bestD = d;
        best = {2, b.x, b.y};
      }
    }
    for (const auto& it : w.items())
    {
      const int d = std::abs(it.x - p.x) + std::abs(it.y - p.y);
      if (!it.taken && it.kind == ItemKind::Proto && d < bestD && w.writersFloor(it.y) == floor)
      {
        bestD = d;
        best = {2, it.x, it.y};
      }
    }
    if (best.kind == 2)
      return best;
  }
  return {18, st.x - 2, st.y - 4, 4, 5, -1};
}

Planner::Goal Planner::wardrobeGoal(const World& w) const
{
  const auto& s = w.wardrobe();
  const int i = wardrobeStep(w);
  if (i < 0)
    return {};
  const auto& st = s.route[std::size_t(i)];
  // A booth: inside it; a plate: on it (both 2 blocks wide).
  return {18, st.x, st.y - 4, 4, 5, -1};
}

Planner::Goal Planner::chooseGoal(const World& w) const
{
  Goal g;
  const auto& lv = w.level();
  g.kind = 0;
  g.x = lv.exitTx * kCellsPerTile;
  g.y = (lv.exitTy + 1) * kCellsPerTile - 1;
  // Level 35: up the turning tower by its route. The planner knows no wrap,
  // but the world keeps the runner in the middle of the map and the next
  // spot on the route is always less than half a lap away. The crawl
  // space's doors for the bonus (Bot::tower presses up at them); the
  // Halcyon Titan is Bot::fightTitan's.
  if (w.tower().wrap)
  {
    const Goal t = towerGoal(w);
    if (t.kind != 0)
      return t;
  }
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
  // Level 38: the next spot on the writers' route, or the keyboard; Bot::writers
  // strikes the words in the way and types the bridge.
  if (w.writers().on && !w.writers().breakout)
  {
    const Goal h = writersGoal(w);
    if (h.kind != 0)
      return h;
  }
  // Level 39: the route from monitor to monitor; Bot::gallery steps in.
  if (w.gallery().on && !w.gallery().route.empty())
  {
    const Goal h = galleryGoal(w);
    if (h.kind != 0)
      return h;
  }
  // Level 40: up the tower step by step; Bot::flyTower rides the trusses.
  if (w.flyTower().on && !w.flyTower().route.empty())
  {
    const Goal h = flyTowerGoal(w);
    if (h.kind != 0)
      return h;
  }
  // Level 41: the letter G for the bonus; Paintball's block tops.
  if (w.sweeps().on)
  {
    const Goal h = sweepsGoal(w);
    if (h.kind != 0)
      return h;
  }
  // Level 42: the route's plates and monitor; the switch's console.
  if (w.finale().on)
  {
    const Goal h = finaleGoal(w);
    if (h.kind != 0)
      return h;
  }
  // Level 37: the next booth or heavy plate; Bot::wardrobe steps in.
  if (w.wardrobe().on && !w.wardrobe().totem)
  {
    const Goal h = wardrobeGoal(w);
    if (h.kind != 0)
      return h;
  }
  // Level 36: the route's next job (a lever, a crane to wait for, a crate to
  // push into place); Bot::warehouse does it once the runner is there.
  if (w.warehouse().on)
  {
    const Goal h = warehouseGoal(w);
    if (h.kind != 0)
      return h;
  }
  // Level 27: for the bonus, stand under the painting (Bot::noir shoots it open).
  if (w.noir().paintX >= 0 && !w.noir().paintOpen && mTakeBonus && !mSkipBonus)
  {
    const auto& n = w.noir();
    const int px = w.player().x / kCellsPerTile;
    if (px <= n.paintX + 2)
      return {18, n.paintX * kCellsPerTile - 2, (n.paintY + 4) * kCellsPerTile - 5, 3, 5, -1};
  }
  // Level 31: for the bonus, stand on the cave floor a way off the dead
  // generator (Bot::storm sticks a rod in it; the strike opens the hatch).
  if (w.storm().on && mTakeBonus && !mSkipBonus)
    for (const auto& m : w.storm().machines)
      if (m.kind == MachineKind::Generator && !m.powered && w.player().y > m.box.y - 20)
        return {18, m.box.x + 10, m.box.bottom() - 4, 3, 5, -1};
  // Level 33: for the bonus, stand in the coolant pipe's nook beside the
  // RATINGS server (Bot::magma shoots it; that opens the entrance).
  if (w.magma().serverX >= 0 && !w.magma().serverShot && mTakeBonus && !mSkipBonus &&
      std::abs(w.player().y - w.magma().serverY) <= 30)
    return {18, w.magma().serverX + 8, w.magma().serverY - 4, 3, 5, -1};
  // Level 34: for the bonus, the drop off D's bow for the bomb bay's ring;
  // then the near end of the grapple chain (Bot::fleet does the swinging).
  if (w.fleet().on)
  {
    const auto& f = w.fleet();
    const auto& p = w.player();
    if (f.dropX >= 0 && !f.hatchOpen && mTakeBonus && !mSkipBonus && p.x <= f.dropX + 30)
      return {18, f.dropX, f.dropY - 4, 4, 5, -1};
    bool inBay = false; // in the bomb bay, the entrance not yet taken: that is next
    if (f.hatchOpen && mTakeBonus && !mSkipBonus && p.x <= f.dropX + 30)
      for (const auto& pr : w.props())
        inBay = inBay || (pr.kind == PropKind::BonusDoor && !pr.used && !pr.dormant);
    if (!inBay && f.chainX0 >= 0 && p.x < f.chainX1 && p.x <= f.chainX0 + 8)
      return {18, f.chainX0 - 2, f.chainY - 4, 4, 5, -1};
  }
  // Level 23: the bonus detour through the crypt; then the next mirror to
  // step through, or lever to throw, on the manor's route.
  bool inCrypt = false;
  if (w.manor().on && mTakeBonus && !mSkipBonus)
  {
    const Goal g = cryptGoal(w, inCrypt);
    if (g.kind == 18)
      return g;
  }
  if (w.manor().on && !inCrypt && w.manor().step < int(w.manor().route.size()))
  {
    const auto& m = w.manor();
    const auto& st = m.route[std::size_t(m.step)];
    if (st.mirror >= 0)
    {
      const auto& mi = m.mirrors[std::size_t(st.mirror)];
      return {17, mi.x * kCellsPerTile, (mi.y + 3) * kCellsPerTile - 5, 4, 5, m.step};
    }
    const auto& l = m.levers[std::size_t(st.lever)];
    return {17, l.x * kCellsPerTile - 1, (l.y + 1) * kCellsPerTile - 5, 4, 5, m.step};
  }
  // Level 22: a fuse on the way (bot=1) is lit from its stand spot.
  if (w.west().on)
  {
    const auto& ws = w.west();
    const int px = w.player().x / kCellsPerTile;
    for (std::size_t i = 0; i < ws.fuses.size(); ++i)
    {
      const auto& f = ws.fuses[i];
      if (!f.bot || f.standX < 0 || f.lit() ||
          (f.barrel >= 0 && (ws.barrels[std::size_t(f.barrel)].blown || ws.barrels[std::size_t(f.barrel)].t >= 0)))
        continue;
      if (px > f.cells[0].x + 2)
        continue;
      return {16, f.standX * kCellsPerTile, f.cells[0].y * kCellsPerTile - 3, 4, 5, int(i)};
    }
  }
  // Level 21's Wireframe bonus: gems, the nearest first.
  if (w.zero().wireframe && lv.goal.rfind("collect:", 0) == 0)
  {
    const auto& p = w.player();
    Goal best;
    int bestD = -1;
    for (std::size_t i = 0; i < w.items().size(); ++i)
    {
      const auto& it = w.items()[i];
      const int d = std::abs(it.x - p.x) + 2 * std::abs(it.y - p.y);
      if (!it.taken && it.kind == ItemKind::Gem && (bestD < 0 || d < bestD) &&
          std::find(mGemSkip.begin(), mGemSkip.end(), int(i)) == mGemSkip.end())
      {
        bestD = d;
        best = {15, it.x, it.y - 1, 2, 2, int(i)};
      }
    }
    if (bestD >= 0)
      return best;
  }
  // Level 12: out of Serpent Spears, fetch more from the nearest box.
  if (!mSkipProto && w.level().weapon == "serpent_spear" && w.stats().protoFound && !spearsHeld(w))
  {
    const auto& p = w.player();
    Goal best;
    int bestD = 160;
    for (const auto& b : w.boxes())
    {
      const int d = std::abs(b.x - p.x) + std::abs(b.y - p.y);
      if (b.alive && b.content == ItemKind::Proto && d < bestD && b.x + 16 >= p.x) // not back the way you came
      {
        bestD = d;
        best = {2, b.x, b.y};
      }
    }
    for (const auto& it : w.items())
    {
      const int d = std::abs(it.x - p.x) + std::abs(it.y - p.y);
      if (!it.taken && it.kind == ItemKind::Proto && d < bestD && it.x + 16 >= p.x)
      {
        bestD = d;
        best = {2, it.x, it.y};
      }
    }
    if (best.kind == 2)
      return best;
  }
  // A force field still up and no card: fetch the card first.
  if (w.map().hasForceFields() && !w.player().hasKey)
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
  // Level 13: the bonus patch shows on the boulder that rolls over the
  // runner in its alcove's shelter: go and shelter there (the bot waits).
  if (mTakeBonus && !mSkipBonus)
    for (const auto& b : w.boulders())
      if (b.teeter >= 0 && b.state <= Boulder::Lip && std::size_t(b.teeter) < w.alcoves().size())
      {
        const Alcove& a = w.alcoves()[std::size_t(b.teeter)];
        return {9, a.x0 + 2, a.feet, a.x1 - a.x0 - 3, 1, b.teeter};
      }
  // Level 14's bonus, Golden Touch: until the gate opens, gild the marked
  // blocks left to right (the drops on the way only go one way), standing
  // on the leftmost one the search can still get to.
  if (w.golden() && !w.goldReached())
  {
    const auto& p = w.player();
    const int bw = lv.width;
    int best = -1, bestKey = 0;
    for (int ty = 1; ty < lv.height; ++ty)
      for (int tx = 0; tx < bw; ++tx)
      {
        if (w.goldAt(tx, ty) != 1 || w.map().solid(tx * kCellsPerTile, (ty - 1) * kCellsPerTile))
          continue;
        const int id = ty * bw + tx;
        if (std::find(mPaintSkip.begin(), mPaintSkip.end(), id) != mPaintSkip.end())
          continue;
        const int key = tx * 1000 + std::abs(tx * kCellsPerTile - p.x) + 3 * std::abs(ty * kCellsPerTile - 1 - p.y);
        if (best < 0 || key < bestKey)
        {
          best = id;
          bestKey = key;
        }
      }
    if (best >= 0)
      return {12, (best % bw) * kCellsPerTile, (best / bw) * kCellsPerTile - 1, 2, 1, best};
  }
  // Level 14: Gold Fever's altars.
  if (w.greedOn())
  {
    const Goal a = altarGoal(w);
    if (a.kind != 0)
      return a;
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
  // Level 20: the lift cage opens once every valve is shut: the nearest
  // valve still open, then the next.
  if (w.reactor().cageX0 >= 0 && !w.reactor().cageOpen)
  {
    const auto& p = w.player();
    Goal best;
    int bestD = -1;
    for (std::size_t i = 0; i < w.reactor().valves.size(); ++i)
    {
      const auto& v = w.reactor().valves[i];
      const int d = std::abs(v.x * kCellsPerTile - p.x) + std::abs(v.y * kCellsPerTile - p.y);
      if (!v.shut && (bestD < 0 || d < bestD))
      {
        bestD = d;
        best = {14, v.x * kCellsPerTile - 1, v.y * kCellsPerTile + 1, 2, 1, int(i)};
      }
    }
    if (bestD >= 0)
      return best;
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

// Level 14: for the bonus, every gem goes to the offering altar next to the
// false one, then the runner holds up empty-handed at the false altar; on
// the way to Kaan-Tolok, gems go back at the last altar until the greed
// meter is under 10, so the golem has no armor.
// On the bonus route, a false altar dropping its floor (lingering on it
// with gems) loses the bonus door.
static bool dropsFalse(const World& w)
{
  for (const auto& a : w.altars())
    if (!a.offer && a.dropped)
      return true;
  return false;
}

Planner::Goal Planner::altarGoal(const World& w) const
{
  const auto& altars = w.altars();
  const auto& p = w.player();
  int fa = -1, last = -1;
  for (std::size_t i = 0; i < altars.size(); ++i)
  {
    if (!altars[i].offer)
      fa = int(i);
    else if (last < 0 || altars[i].bx > altars[std::size_t(last)].bx)
      last = int(i);
  }
  auto at = [&](int kind, int i) {
    const Altar& a = altars[std::size_t(i)];
    return Goal{kind, a.bx * kCellsPerTile - 2, (a.by + 1) * kCellsPerTile - 1, 1, 1, i};
  };
  if (mTakeBonus && !mSkipBonus && fa >= 0 && !altars[std::size_t(fa)].open)
  {
    if (w.stats().gems == 0)
      return at(10, fa);
    int best = -1;
    for (std::size_t i = 0; i < altars.size(); ++i)
      if (altars[i].offer &&
          (best < 0 || std::abs(altars[i].bx - altars[std::size_t(fa)].bx) <
                         std::abs(altars[std::size_t(best)].bx - altars[std::size_t(fa)].bx)))
        best = int(i);
    if (best >= 0)
      return at(10, best);
  }
  if (last >= 0 && w.greed() >= 10 && w.stats().gems > 0 && !w.golemFight() &&
      p.x < altars[std::size_t(last)].bx * kCellsPerTile + 24)
    return at(11, last);
  return {};
}

int Planner::valveToHold(const World& w) const
{
  const auto& rs = w.reactor();
  for (std::size_t i = 0; i < rs.valves.size(); ++i)
    if (!rs.valves[i].shut && w.atValve(int(i)))
      return int(i);
  return -1;
}

int Planner::altarToHold(const World& w) const
{
  if (!w.greedOn())
    return -1;
  const Goal g = altarGoal(w);
  return g.kind >= 10 ? g.index : -1;
}

void Planner::buildField(const World& w, const Goal& goal)
{
  // Level 19: the map with its turned-over view stacked under it (rows Hc
  // and on): a runner hanging from a ceiling is one standing on the turned
  // map's floor. Switches flip between the two; walking into a chamber whose
  // down is the other way turns the runner over.
  const auto& gs = w.grav();
  const bool stacked = gs.on && !gs.gun && !gs.zones.empty();
  std::optional<CollisionMap> stack;
  if (stacked)
  {
    stack.emplace(w.map());
    stack->makeStack(w.map());
  }
  const CollisionMap& map = stacked ? *stack : w.map();
  const int Hc = w.map().height();
  mStackHc = stacked ? Hc : 0;
  mW = map.width();
  mH = map.height();
  const int W = mW, H = mH;
  // With the card (or once the card is no longer needed) force fields open.
  const bool openFields = goal.kind == 0 || goal.kind == 3 || w.player().hasKey;
  int jumpH = 0;
  for (int v : w.runnerJumpArc())
    jumpH += v;
  if (w.bounce())
    jumpH = 18; // Bounce House: up to 18 cells, held jump builds it
  // Level 30: under water a jump is a 4-cell swim stroke, and the strokes chain.
  if (w.trench().water)
    jumpH = std::max(jumpH, 4 * w.strokesMax());

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
  // Golden Touch: a gold door that can still open opens as the runner comes;
  // the gate opens once the gold is in.
  for (const auto& d : w.goldDoors())
    if (!d.gold && (!d.gate || w.goldReached()))
      for (int y = d.by0 * kCellsPerTile; y < (d.by1 + 1) * kCellsPerTile; ++y)
        for (int x = d.bx * kCellsPerTile; x < (d.bx + 1) * kCellsPerTile; ++x)
          if (x >= 0 && y >= 0 && x < W && y < H)
            blocked[std::size_t(y * W + x)] = top[std::size_t(y * W + x)] = 0;
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
  const bool caps = pl0.weapon == Weapon::Proto && pl0.proto == int(ProtoId::BlastingCaps) && pl0.ammo > 0;
  mWalls.clear();
  for (std::size_t bi = 0; bi < w.breakables().size(); ++bi)
  {
    const auto& b = w.breakables()[bi];
    // Growth Spurt: big enough, the runner walks through cracked blocks.
    const bool big = w.green().grow && w.green().size >= 2;
    if (b.broken || !(b.by == 0 || (b.by == 3 && sound) || (b.by == 1 && (pl0.weapon == Weapon::Rocket || caps)) ||
                       (b.by == 2 && big)))
      continue;
    const CellBox area{b.x0 * kCellsPerTile - 12, b.y0 * kCellsPerTile - 12, (b.x1 - b.x0 + 1) * kCellsPerTile + 24,
      (b.y1 - b.y0 + 1) * kCellsPerTile + 24};
    // Cracked rock (level 11) takes a cap thrown and a wait out of its blast:
    // every one still standing counts the same from anywhere, so blowing one
    // is progress and walking up to it never looks like a step back.
    if (area.intersects({goal.x, goal.y - 5, std::max(2, goal.w), std::max(6, goal.h)}) || (b.by == 1 && caps))
      mWalls.push_back(int(bi));
    for (int y = b.y0 * kCellsPerTile; y < (b.y1 + 1) * kCellsPerTile; ++y)
      for (int x = b.x0 * kCellsPerTile; x < (b.x1 + 1) * kCellsPerTile; ++x)
        if (x >= 0 && y >= 0 && x < W && y < H)
          blocked[std::size_t(y * W + x)] = top[std::size_t(y * W + x)] = 0;
  }
  // Level 19: a test gate opens once its subjects are down (see heuristic).
  for (const auto& t : w.grav().gates)
    if (!t.open && !t.enemies.empty())
      for (int y = t.by * kCellsPerTile; y < (t.by + t.h) * kCellsPerTile; ++y)
        for (int x = t.bx * kCellsPerTile; x < (t.bx + 1) * kCellsPerTile; ++x)
          if (x >= 0 && y >= 0 && x < W && y < H)
            blocked[std::size_t(y * W + x)] = top[std::size_t(y * W + x)] = 0;
  // Level 26: a model building breaks when jumped into (Rocco walks
  // through), so it is no wall to the field, and the station roof's street
  // slab with it; a tower that still stands is the bridge it will lie as.
  for (const auto& b : w.city().buildings)
  {
    auto set = [&](int bx0, int by0, int bx1, int by1, char v) {
      for (int y = by0 * kCellsPerTile; y < (by1 + 1) * kCellsPerTile; ++y)
        for (int x = bx0 * kCellsPerTile; x < (bx1 + 1) * kCellsPerTile; ++x)
          if (x >= 0 && y >= 0 && x < W && y < H)
            blocked[std::size_t(y * W + x)] = top[std::size_t(y * W + x)] = v;
    };
    if (b.standing() || b.state == BuildingState::Crumbling || b.state == BuildingState::Leaning ||
        b.state == BuildingState::Falling)
    {
      set(b.x0, b.y0, b.x1, b.y1, 0);
      if (b.ux0 >= 0 && b.state != BuildingState::Leaning && b.state != BuildingState::Falling)
        set(b.ux0, b.uy0, b.ux1, b.uy1, 0);
    }
    if (b.kind == BuildingKind::Tower && b.state != BuildingState::Lying)
      set(b.bx0, b.by0, b.bx1, b.by1, 1);
  }
  // Its canal and bay: no floor.
  for (const auto& pd : w.city().ponds)
    for (int y = std::max(0, pd.y); y <= std::min(H - 1, pd.y + pd.h); ++y)
      for (int x = std::max(0, pd.x); x <= std::min(W - 1, pd.x + pd.w - 1); ++x)
      {
        blocked[std::size_t(y * W + x)] = 1;
        if (y > pd.y)
          top[std::size_t(y * W + x)] = 0;
      }
  // Level 15: a crate one shot breaks is no wall to the field.
  for (const auto& c : w.station().crates)
    if (c.alive && !c.loose && !c.held)
      for (int y = c.by * kCellsPerTile; y < (c.by + 1) * kCellsPerTile; ++y)
        for (int x = c.bx * kCellsPerTile; x < (c.bx + 1) * kCellsPerTile; ++x)
          if (x >= 0 && y >= 0 && x < W && y < H)
            blocked[std::size_t(y * W + x)] = top[std::size_t(y * W + x)] = 0;
  // Level 13: a Totem Stack is a wall the search shoots down a head at a time.
  for (const auto& e : w.enemies())
    if (e.alive && e.kind == EnemyKind::Totem)
      for (int y = e.y - e.h + 1; y <= e.y; ++y)
        for (int x = e.x; x < e.x + e.w; ++x)
          if (x >= 0 && y >= 0 && x < W && y < H)
            blocked[std::size_t(y * W + x)] = top[std::size_t(y * W + x)] = 0;
  // Level 21: a bulkhead the level script will open (its shift is under way
  // or waits on a spot the route passes) is no wall to the field; an open
  // one is no wall either. The search waits for it.
  std::vector<std::uint8_t> willOpen(w.layers().size());
  for (const auto& sh : w.zero().shifts)
    if (!sh.done && (sh.t >= 0 || sh.triggerX >= 0))
      for (int li : sh.open)
        willOpen[std::size_t(li)] = 1;
  // Level 23: a lever door, once its levers are thrown, is open.
  for (const auto& d : w.manor().doors)
    willOpen[std::size_t(d.layer)] = !w.layers()[std::size_t(d.layer)].solid;
  // Level 29: a black bar out of its pips is not there.
  for (const auto& b : w.signal().bars)
    if (b.color == kSignalBlack && !w.layers()[std::size_t(b.layer)].solid)
      willOpen[std::size_t(b.layer)] = 1;
  // Level 42: the ON AIR door and the switch's glass cover, once open, are no walls.
  for (int li : {w.finale().door, w.finale().cover})
    if (li >= 0 && !w.layers()[std::size_t(li)].solid)
      willOpen[std::size_t(li)] = 1;
  std::vector<std::uint8_t> dishStrut(w.layers().size());
  for (const auto& st : w.dish().struts)
    dishStrut[std::size_t(st.layer)] = 1;
  for (std::size_t li = 0; li < w.layers().size(); ++li)
  {
    const auto& l = w.layers()[li];
    if ((l.style == 5 && (!l.solid || willOpen[li])) || (l.style == 6 && !l.solid) || (l.style != 5 && willOpen[li]))
    {
      for (int y = l.y0 * kCellsPerTile; y < (l.y1 + 1) * kCellsPerTile; ++y)
        for (int x = l.x0 * kCellsPerTile; x < (l.x1 + 1) * kCellsPerTile; ++x)
          if (x >= 0 && y >= 0 && x < W && y < H)
            blocked[std::size_t(y * W + x)] = top[std::size_t(y * W + x)] = 0;
      continue;
    }
    for (int ty = l.y0; ty <= l.y1; ++ty)
      for (int tx = l.x0; tx <= l.x1; ++tx)
        for (int dy = 0; dy < kCellsPerTile; ++dy)
          for (int dx = 0; dx < kCellsPerTile; ++dx)
          {
            const int x = tx * kCellsPerTile + dx, y = ty * kCellsPerTile + dy;
            if (x < 0 || y < 0 || x >= W || y >= H)
              continue;
            // Timed layers are only solid some of the time: the field
            // treats them as one-way platforms and the search does the timing
            // (Level 29's colour bars too: the search shoots the dials).
            const bool timed = l.driver == LayerDriver::Beat || l.driver == LayerDriver::Timer || l.style == 7 ||
              dishStrut[li]; // Level 32: a strut is there in some of the dishes' poses
            if (l.tile == Tile::Solid && !timed)
              blocked[std::size_t(y * W + x)] = 1;
            else if (l.tile == Tile::Solid)
              blocked[std::size_t(y * W + x)] = 0;
            if ((l.tile == Tile::Solid && !timed) || dy == 0)
              top[std::size_t(y * W + x)] = 1;
          }
  }
  // Level 30: a harpoon line can run from each anchor ring to its planting
  // spot (Bot::trench strings it there).
  if (w.trench().on && (w.level().weapon == "harpoon_line"))
    for (const auto& a : w.trench().anchors)
      if (a.row >= 0 && a.row < H)
        for (int x = std::max(0, std::min(a.x, a.spotX)); x <= std::min(W - 1, std::max(a.x + 1, a.spotX)); ++x)
          top[std::size_t(a.row * W + x)] = 1;
  // Level 23: over the reflected gallery's hole the field counts on a
  // bridge of pinned ghosts (Bot::manor pins them at the edge).
  {
    const auto& m = w.manor();
    int over = 0;
    for (const auto& pl : m.platforms)
      over += pl.bx > m.bridgeLanding && pl.bx + 1 < m.bridgeEdge ? 1 : 0;
    if (m.bridgeEdge >= 0 && m.side == kSideMirror && over < 2)
    {
      const int y = (m.bridgeRow - 2) * kCellsPerTile;
      for (int x = (m.bridgeLanding + 1) * kCellsPerTile; x < m.bridgeEdge * kCellsPerTile; ++x)
        if (x >= 0 && y >= 0 && x < W && y < H)
          top[std::size_t(y * W + x)] = 1;
    }
  }
  // Level 17: a plant is there when its lamps are lit (or the lamp test
  // takes them as lit), and gone otherwise, withering or not.
  std::vector<std::uint8_t> plantLadder(std::size_t(W * H)), plantGone(std::size_t(W * H));
  {
    const auto& g = w.green();
    for (const auto& pl : g.plants)
    {
      bool on = !pl.lamps.empty();
      for (int l : pl.lamps)
        on = on && (std::size_t(l) < mLampOn.size() ? mLampOn[std::size_t(l)] != 0 : g.lamps[std::size_t(l)].lit);
      for (const auto& [tx, ty] : pl.tiles)
        for (int dy = 0; dy < kCellsPerTile; ++dy)
          for (int dx = 0; dx < kCellsPerTile; ++dx)
          {
            const int x = tx * kCellsPerTile + dx, y = ty * kCellsPerTile + dy;
            if (x < 0 || y < 0 || x >= W || y >= H)
              continue;
            const std::size_t i = std::size_t(y * W + x);
            if (!on)
            {
              blocked[i] = top[i] = 0;
              plantGone[i] = 1;
              continue;
            }
            switch (pl.kind)
            {
              case PlantKind::Bridge:
              case PlantKind::Stairs:
                blocked[i] = top[i] = 1;
                break;
              case PlantKind::Leaf:
              case PlantKind::Flower:
                blocked[i] = 0;
                top[i] = dy == 0;
                break;
              case PlantKind::Ladder:
                blocked[i] = top[i] = 0;
                plantLadder[i] = 1;
                break;
            }
          }
    }
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
    // Level 25: the dandelion's seed head comes once it goes to seed (Bot::garden sees to that).
    const bool seedhead = w.garden().seedhead >= 0 && &pl == &w.platforms()[std::size_t(w.garden().seedhead)];
    if (pl.hidden && !seedhead)
      continue; // a train not here yet
    int y0 = pl.y, y1 = pl.y;
    int x0 = pl.x, x1 = pl.x;
    if (pl.mode == PlatformMode::Sink || pl.mode == PlatformMode::Rise)
      y0 = y1 = pl.homeY; // level 12: stones at rest, the bridge once it is up
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
        if (pl.mode != PlatformMode::Sink)
        {
          y0 = std::min(y0, py);
          y1 = std::max(y1, py);
        }
      }
    for (int y = std::max(0, y0); y <= std::min(H - 1, y1); ++y)
      for (int x = std::max(0, x0); x < std::min(W, x1 + pl.w); ++x)
        top[std::size_t(y * W + x)] = 1;
  }
  // Level 25: the pond is no floor; only what floats over it is.
  for (const auto& pd : w.garden().ponds)
    for (int y = std::max(0, pd.y0); y <= std::min(H - 1, pd.y1 + 1); ++y)
      for (int x = std::max(0, pd.x0); x <= std::min(W - 1, pd.x1); ++x)
      {
        blocked[std::size_t(y * W + x)] = 1;
        if (y > pd.y0)
          top[std::size_t(y * W + x)] = 0;
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
  // Level 37: spring racks throw the runner up as they arrive.
  if (w.wardrobe().on)
    for (const auto& k : w.wardrobe().racks)
      for (int x = std::max(0, k.x); x < std::min(W, k.x + k.w); ++x)
        if (k.y + 1 >= 0 && k.y + 1 < H)
          padLift[std::size_t((k.y + 1) * W + x)] = std::max(jumpH, w.rackLaunch());

  // Per player position (bottom-left cell): valid, height above ground,
  // ladder and pipe.
  std::vector<std::uint8_t> valid(std::size_t(W * H)), ladder(std::size_t(W * H)), hang(std::size_t(W * H)),
    hazard(std::size_t(W * H)), spearOnly(std::size_t(W * H));
  // Level 12: with Serpent Spears (or more to fetch), a foothold can go
  // beside any wall face.
  const bool spears = spearsHeld(w) || w.level().weapon == "serpent_spear";
  auto spearCell = [&](int x, int y) {
    for (int dir : {1, -1})
    {
      const int face = dir > 0 ? x + 3 : x - 1;
      const int fx = dir > 0 ? x + 1 : x;
      if (!map.solid(face, y - 2) || !(map.solid(face, y + 1) || map.solid(face, y + 2)))
        continue;
      bool room = true;
      for (int yy = y + 1; yy <= y + 2; ++yy)
        for (int xx = fx; xx <= fx + 1; ++xx)
          room = room && !map.solid(xx, yy) && w.lavaAt(xx, yy) < 0;
      if (room)
        return true;
    }
    return false;
  };
  std::vector<std::uint8_t> spearAt(std::size_t(W * H));
  if (spears)
    for (int y = 0; y < H; ++y)
      for (int x = 0; x < W; ++x)
        spearAt[std::size_t(y * W + x)] = spearCell(x, y);
  std::vector<CellBox> bites;
  for (const auto& e : w.enemies())
    if (e.alive && e.kind == EnemyKind::Snapjaw)
      bites.push_back({e.x - 2, e.y - e.h - 1, e.w + 4, e.h + 4});
  std::vector<int> support(std::size_t(W * H), kInf), lift(std::size_t(W * H), jumpH);
  // Level 44: beside a goo wall you cling, and a kick off it is a jump from
  // there (back to the same wall, or across a chimney): like ground.
  std::vector<std::uint8_t> clingAt(std::size_t(W * H));
  if (w.hasGoo())
  {
    auto gooCell = [&](int cx, int cy) {
      return cx >= 0 && cy >= 0 && map.solid(cx, cy) && w.gooBlockAt(cx / kCellsPerTile, cy / kCellsPerTile);
    };
    for (int y = 0; y < H; ++y)
      for (int x = 0; x < W; ++x)
        for (int yy = y - 3; yy <= y - 2; ++yy)
          if (gooCell(x - 1, yy) || gooCell(x + 3, yy))
            clingAt[std::size_t(y * W + x)] = 1;
  }
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
      // Lava pops you back out (Rocco's shelf only burns).
      if (!w.lavas().empty() && w.player().turbo == 0)
      {
        const CellBox body = boxAt(x, y, 3, 5);
        for (const auto& l : w.lavas())
          if (l.box().intersects(body))
          {
            if (l.wade)
              hazard[i] = 1;
            else
              valid[i] = 0;
          }
        if (!valid[i])
          continue;
      }
      for (int k = 0; k < 64 && y + 1 + k < H; ++k)
      {
        // A foothold you could stick below counts as ground to jump from,
        // and so does a goo wall to cling to.
        if (k > 0 && (spearAt[std::size_t((y + k) * W + x)] || clingAt[std::size_t((y + k) * W + x)]))
        {
          support[i] = k;
          break;
        }
        if (isTop(x, y + 1 + k) || isTop(x + 1, y + 1 + k) || isTop(x + 2, y + 1 + k))
        {
          support[i] = k;
          for (int xx = x; xx <= x + 2; ++xx)
            if (xx < W && y + 1 + k < H)
              lift[i] = std::max(lift[i], padLift[std::size_t((y + 1 + k) * W + xx)]);
          break;
        }
      }
      if (spears && support[i] != 0 && spearAt[i])
      {
        support[i] = 0;
        spearOnly[i] = 1;
      }
      if (clingAt[i])
        support[i] = 0;
      for (int yy = y - 4; yy <= y; ++yy)
        if (yy >= 0 && ((map.ladder(x + 1, yy) && !plantGone[std::size_t(yy * W + x + 1)]) ||
                         plantLadder[std::size_t(yy * W + x + 1)]))
          ladder[i] = 1;
      hang[i] = map.climbable(x + 1, y - 5) || map.climbable(x + 1, y - 4);
      for (int yy = y - 4; yy <= y && !hazard[i]; ++yy)
        for (int xx = x; xx <= x + 2; ++xx)
          if (map.hazard(xx, yy))
            hazard[i] = 1;
      // Level 17: a Snapjaw's bite reaches a cell or two past its jaws.
      for (const auto& b : bites)
        if (!hazard[i] && b.intersects(boxAt(x, y, 3, 5)))
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

  // Level 19: each position's chamber (by the box's middle, in the map),
  // and whether its down is up.
  std::vector<int> zoneOf;
  std::vector<char> zoneUp;
  if (stacked)
  {
    zoneOf.assign(std::size_t(W * H), -1);
    for (int y = 0; y < H; ++y)
      for (int x = 0; x < W; ++x)
        zoneOf[std::size_t(y * W + x)] = w.gravZoneAt(x + 1, y < Hc ? y - 2 : 2 * Hc + 1 - y);
    for (const auto& z : gs.zones)
      zoneUp.push_back(z.dir == Grav::Up);
  }
  // The same box, turned over: a row of one half to the row of the other.
  auto flipRow = [&](int y) { return 2 * Hc + 3 - y; };

  // Level 30: a current up faster than the water sinks you holds a swimmer
  // up (sideways is free, down is not); one down faster than strokes climb
  // only lets them sink. Rocco is too heavy for either.
  std::vector<std::int8_t> drift(std::size_t(W * H), 0);
  if (w.trench().water && w.characterIndex() != 1)
    for (const auto& c : w.trench().currents)
      if (c.dy != 0 && c.every < kSinkEvery)
        for (int y = 0; y < H; ++y)
          for (int x = 0; x < W; ++x)
            if (c.rect.contains(x + 1, y - 2))
              drift[std::size_t(y * W + x)] = std::int8_t(c.dy);

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
      const bool lifted = drift[i] < 0 && s != 0;
      const bool grounded = s == 0 || ladder[i] || hang[i] || lifted;
      for (int a = 0; a < A; ++a)
      {
        if (grounded && a > 0)
          continue;
        const int from = node(x, y, a);
        auto add = [&](int tx, int ty, int ta, int cost) {
          if (tx < 0 || tx >= W || ty < 0 || ty >= H)
            return;
          if (stacked)
          {
            // Into another chamber whose down is the other way: turned over.
            const int zf = zoneOf[i], zt = zoneOf[std::size_t(ty * W + tx)];
            if (zt >= 0 && zt != zf && (zoneUp[std::size_t(zt)] != 0) != (ty >= Hc))
            {
              ty = flipRow(ty);
              if (ty < 0 || ty >= H)
                return;
            }
          }
          const std::size_t j = std::size_t(ty * W + tx);
          if (!valid[j])
            return;
          const bool g2 = support[j] == 0 || ladder[j] || hang[j] || (drift[j] < 0 && support[j] != 0);
          if (g2)
            ta = 0;
          if (ta >= A)
            return;
          list.push_back({from, node(tx, ty, ta),
            cost + (hazard[j] == 1 ? 40 : (hazard[j] == 2 ? 6 : 0)) + (spearOnly[j] ? 6 : 0)});
        };
        // Sideways: free on the ground, from the air budget in a jump.
        for (int dx : {-1, 1})
        {
          add(x + dx, y, grounded ? (s == 0 || lifted ? 0 : 1) : a + 1, 2);
          if (s == 0)
            add(x + dx, y - 1, 0, 2); // stair step
        }
        // Up: within a jump's height of the ground, or on a ladder.
        if (ladder[i] || lifted || (s < lift[i] && s < kInf && drift[i] <= 0))
          add(x, y - 1, ladder[i] || lifted ? 0 : a, 2);
        // Down: falling, or climbing down; never through a one-way floor
        // you stand on (there is no dropping through them).
        bool oneWay = false;
        if (s == 0 && !ladder[i] && y + 1 < H)
          for (int xx = x; xx <= x + 2 && !oneWay; ++xx)
            oneWay = isTop(xx, y + 1) && !isBlocked(xx, y + 1) && map.solidTop(xx, y + 1) &&
              !map.ladder(xx & ~1, y + 1) &&
              (map.block(xx / kCellsPerTile, (y + 1) / kCellsPerTile) == Tile::Platform || map.slope(xx, y + 1));
        if (!oneWay && !lifted)
          add(x, y + 1, a, 1);
      }
    }
  // Level 19: up on a switch (on the ground) turns its chamber over, and
  // the runner with it.
  if (stacked)
    for (const auto& sw : gs.switches)
    {
      const CellBox sb{sw.bx * kCellsPerTile, sw.by * kCellsPerTile, 2, 2};
      for (int y = 0; y < H; ++y)
        for (int x = std::max(0, sb.x - 2); x <= std::min(W - 1, sb.x + 1); ++x)
        {
          const std::size_t i = std::size_t(y * W + x);
          const CellBox real = y < Hc ? boxAt(x, y, 3, 5) : CellBox{x, 2 * Hc - 1 - y, 3, 5};
          if (!valid[i] || support[i] != 0 || !real.intersects(sb))
            continue;
          const int ty = flipRow(y);
          if (ty < 0 || ty >= H || !valid[std::size_t(ty * W + x)])
            continue;
          list.push_back({node(x, y, 0), node(x, ty, 0), 8});
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
  // Level 45: a Gullet Tube's mouth takes you to its far end (down the
  // branch its valve is set to); a floor exit spits you up first.
  for (const auto& t : w.gulletTubes())
  {
    const int b = t.valve ? t.set : 0;
    int ex = 0, ey = 0;
    w.tubeExit(t, b, ex, ey);
    if (ex < 0 || ey < 0 || ex >= W || ey >= H)
      continue;
    const CellBox trig = w.mouthTrigger(t);
    const int cost = t.len[std::size_t(b)] / 2 + 6;
    const int rise = t.outY[std::size_t(b)] < 0 ? 10 : 0;
    for (int y = std::max(0, trig.y - 1); y <= std::min(H - 1, trig.bottom() + Player::kHeight); ++y)
      for (int x = std::max(0, trig.x - Player::kWidth); x <= std::min(W - 1, trig.right()); ++x)
      {
        if (!valid[std::size_t(y * W + x)] || !boxAt(x, y, Player::kWidth, Player::kHeight).intersects(trig))
          continue;
        for (int k = 0; k <= rise; ++k)
          if (ey - k >= 0 && valid[std::size_t((ey - k) * W + ex)])
            for (int a = 0; a < A; ++a)
              list.push_back({node(x, y, a), node(ex, ey - k, 0), cost + k});
      }
  }
  // Level 46: shooting a Swap Crystal puts you where it is: from anywhere
  // on the ground level with it (a shot ahead) or under it (a shot up),
  // nothing solid in between. A drifting one: anywhere along its path (the
  // search does the timing).
  for (const auto& c : w.swapCrystals())
  {
    if (c.gone || c.hidden || c.cracked)
      continue;
    std::vector<std::pair<int, int>> spots = c.path;
    if (!c.drifting)
      spots = {{c.x, c.y}};
    for (const auto& [cx, cy] : spots)
    {
      SwapCrystal at = c;
      at.x = cx;
      at.y = cy;
      int sx = 0, sy = 0;
      if (!w.crystalSwapSpot(at, sx, sy) || sx < 0 || sy < 0 || sx >= W || sy >= H || !valid[std::size_t(sy * W + sx)])
        continue;
      auto shooter = [&](int fx, int fy) {
        if (fx < 0 || fy < 0 || fx >= W || fy >= H)
          return;
        const std::size_t i = std::size_t(fy * W + fx);
        if (!valid[i] || support[i] != 0 || (fx == sx && fy == sy))
          return;
        list.push_back({node(fx, fy, 0), node(sx, sy, 0), 8 + (std::abs(fx - sx) + std::abs(fy - sy)) / 4});
      };
      // Ahead: the muzzle is two cells over the feet, three cells right of
      // x facing right, one cell left of it facing left; a shot has to touch
      // the crystal's glow (a cell either side of it) while it is still on
      // screen, which the camera keeps to about 19 cells ahead of you going
      // right and 16 going left.
      for (int fy = cy + 2; fy <= cy + SwapCrystal::kH + 1; ++fy)
      {
        const int row = fy - 2;
        if (row < 0 || row >= H)
          continue;
        for (int fx = cx - 4; fx >= std::max(0, cx - 19); --fx)
        {
          if (map.solid(fx + 3, row))
            break;
          shooter(fx, fy);
        }
        for (int fx = cx + SwapCrystal::kW + 2; fx <= std::min(W - 1, cx + 17); ++fx)
        {
          if (map.solid(fx - 1, row))
            break;
          shooter(fx, fy);
        }
      }
      // Up: the muzzle's column is x + 2 facing right, x facing left, five
      // cells up from the feet.
      for (int col = cx - 1; col <= cx + SwapCrystal::kW; ++col)
        for (int fy = cy + SwapCrystal::kH + 5; fy <= std::min(H - 1, cy + SwapCrystal::kH + 18); ++fy)
        {
          if (map.solid(col, fy - 5))
            break;
          shooter(col - 2, fy);
          shooter(col, fy);
        }
    }
  }
  // Level 47: hands that meet a Silk Line in the air (a jump into it, a
  // step off a ledge under it) ride it to its low end and drop off there.
  // A line still to be spun, or cut and to be spun again, counts too (the
  // search waits for it).
  for (const auto& l : w.silkLines())
  {
    if (l.mine)
      continue;
    SilkLine whole = l;
    whole.spun = whole.len;
    int ex = -1, ey = -1;
    for (float s = whole.len; s >= 0.0f && ex < 0; s -= 1.0f)
      if (w.silkHangSpot(whole, s, ex, ey))
        --ey; // let go: back to a 5-cell box
      else
        ex = -1;
    if (ex < 0 || ey < 0 || ex >= W || ey >= H || !valid[std::size_t(ey * W + ex)])
      continue;
    for (float s = 0.0f; s < whole.len - 1.0f; s += 1.0f)
    {
      int hx = 0, hy = 0;
      if (!w.silkHangSpot(whole, s, hx, hy))
        continue;
      const int cost = int((whole.len - s) / 2.0f) + 2;
      for (int dy = -2; dy <= 0; ++dy)
        for (int dx = -1; dx <= 1; ++dx)
        {
          const int fx = hx + dx, fy = hy - 1 + dy;
          if (fx < 0 || fy < 0 || fx >= W || fy >= H)
            continue;
          const std::size_t i = std::size_t(fy * W + fx);
          if (!valid[i] || support[i] == 0)
            continue;
          for (int a = 0; a < A; ++a)
            list.push_back({node(fx, fy, a), node(ex, ey, 0), cost});
        }
    }
  }
  // Level 27's Silent Picture: the ladder gag's top carries the runner over
  // the gap onto the far roof.
  if (const auto& f = w.film(); f.gagX >= 0 && f.gagState == 0)
  {
    const int ex = (f.gagX + f.gagLen + 1) * kCellsPerTile, ey = (f.gagY1 + 1) * kCellsPerTile - 1;
    if (ex < W && ey < H && valid[std::size_t(ey * W + ex)])
      for (int fy = std::max(0, f.gagY0 * kCellsPerTile); fy <= std::min(H - 1, f.gagY0 * kCellsPerTile + 6); ++fy)
        for (int fx = std::max(0, f.gagX * kCellsPerTile - 2); fx <= std::min(W - 1, f.gagX * kCellsPerTile + 1); ++fx)
          if (valid[std::size_t(fy * W + fx)])
            for (int a = 0; a < A; ++a)
              list.push_back({node(fx, fy, a), node(ex, ey, 0), 16});
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
    : goal.kind == 3 || goal.kind >= 9    ? CellBox{goal.x, goal.y, goal.w, goal.h}
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
      const CellBox body = stacked && y >= Hc ? CellBox{x, 2 * Hc - 1 - y, 3, 5} : boxAt(x, y, 3, 5);
      if (!body.intersects(goalBox))
        continue;
      if ((goal.kind == 0 || goal.kind == 4 || goal.kind == 5 || goal.kind == 8 || goal.kind >= 9) && support[i] != 0)
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

int Planner::fieldRow(const World& w) const
{
  // Level 19: hanging from a ceiling is the stacked turned-over map's floor.
  const auto& p = w.player();
  if (mStackHc > 0 && p.grav == Grav::Up)
    return 2 * mStackHc - 1 - p.y;
  return p.y;
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
  // Level 17: a plant still growing close by: waiting for it is progress.
  for (const auto& pl : w.green().plants)
  {
    const int n = int(pl.tiles.size());
    if (mGoalKind == 13 || !pl.growing || n == 0 || pl.prog >= n * 90)
      continue;
    const int rx = pl.tiles.front().first * kCellsPerTile, ry = pl.tiles.front().second * kCellsPerTile;
    if (std::abs(rx - p.x) < 40 && std::abs(ry - p.y) < 40)
      extra += (n * 90 - pl.prog) / std::max(1, 2 * n);
  }
  // Level 21: a bulkhead the script is opening: waiting for it is progress.
  for (const auto& sh : w.zero().shifts)
    if (!sh.done && sh.t >= 0 && !sh.open.empty())
      extra += std::max(0, kShiftPreview + sh.slide - sh.t) / 2;
  // A key door that will open: getting to it and waiting while it sinks.
  for (const auto& d : w.keyDoors())
    if (!d.open && w.stoneKeysHeld() >= d.keys)
      extra += d.sink < 0 ? 40 : 20 - std::min(d.sink, 20);
  // A Totem Stack ahead: every head shot off is progress. Every stack
  // ahead counts, however far, so walking towards one is no step back.
  for (const auto& e : w.enemies())
    if (e.alive && e.kind == EnemyKind::Totem && e.x + e.w > p.x - 2)
      extra += 12 * e.hp;
  // Level 19: every Test Subject still behind its gate is a shot to come.
  for (const auto& t : w.grav().gates)
    if (!t.open)
      for (int en : t.enemies)
        if (std::size_t(en) < w.enemies().size() && w.enemies()[std::size_t(en)].alive)
          extra += 20 * std::max(1, w.enemies()[std::size_t(en)].hp);
  // Level 15: every crate still standing is a shot to come.
  for (const auto& c : w.station().crates)
    if (c.alive && !c.loose)
      extra += 6;
  for (int bi : mWalls)
    if (std::size_t(bi) < w.breakables().size() && !w.breakables()[std::size_t(bi)].broken)
      extra += (w.breakables()[std::size_t(bi)].by == 1 ? 60 : 12) * std::max(0, w.breakables()[std::size_t(bi)].hp);
  if (p.x < 0 || p.y < 0 || p.x >= mW || p.y >= mH)
    return kInf;
  const int A = kAirBudget + 1;
  if (p.tube >= 0 && std::size_t(p.tube) < w.gulletTubes().size())
  {
    // Inside a Gullet Tube: as good as being at its far end already.
    const auto& t = w.gulletTubes()[std::size_t(p.tube)];
    int ex = 0, ey = 0;
    w.tubeExit(t, p.tubeBranch, ex, ey);
    if (ex < 0 || ey < 0 || ex >= mW || ey >= mH)
      return kInf;
    int best = kInf;
    for (int a = 0; a < A; ++a)
      best = std::min(best, mDist[std::size_t((ey * mW + ex) * A + a)]);
    return best >= kInf ? kInf : best + std::max(0, t.len[std::size_t(p.tubeBranch)] - p.tubeS) / 2 + extra;
  }
  if (p.silk >= 0 && std::size_t(p.silk) < w.silkLines().size())
  {
    // On a Silk Line: as good as at its low end already.
    SilkLine whole = w.silkLines()[std::size_t(p.silk)];
    whole.spun = whole.len;
    int ex = -1, ey = -1;
    for (float s = whole.len; s >= p.silkS && ex < 0; s -= 1.0f)
      if (w.silkHangSpot(whole, s, ex, ey))
        --ey;
      else
        ex = -1;
    if (ex >= 0 && ey >= 0 && ex < mW && ey < mH)
    {
      int best = kInf;
      for (int a = 0; a < A; ++a)
        best = std::min(best, mDist[std::size_t((ey * mW + ex) * A + a)]);
      if (best < kInf)
        return best + int((whole.len - p.silkS) / 2.0f) + extra;
    }
  }
  const int py = fieldRow(w);
  if (py < 0 || py >= mH)
    return kInf;
  const int base = (py * mW + p.x) * A;
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
          const int x = p.x + dx, y = py + dy;
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

bool Planner::lampGoal(const World& w, Goal& g)
{
  const auto& gs = w.green();
  const auto& lamps = gs.lamps;
  const auto& p = w.player();
  // Candidates: lamps out that some plant needs.
  std::vector<char> cand(lamps.size(), 0);
  std::uint64_t key = 1469598103934665603ull ^ std::uint64_t(g.kind * 7919 + g.x * 131 + g.y);
  bool any = false;
  for (const auto& pl : gs.plants)
    for (int l : pl.lamps)
      if (!lamps[std::size_t(l)].lit && !lamps[std::size_t(l)].locked)
      {
        cand[std::size_t(l)] = 1;
        any = true;
      }
  if (!any)
    return false;
  for (std::size_t i = 0; i < lamps.size(); ++i)
    key = (key ^ std::uint64_t(lamps[i].lit ? 2 : cand[i])) * 1099511628211ull;
  // The runner's floor row matters too (the next plant along changes).
  key = (key ^ std::uint64_t(p.y / 16)) * 1099511628211ull;
  if (key != mLampKey)
  {
    mLampKey = key;
    mLampWanted = -1;
    mLampOn.assign(lamps.size(), 0);
    for (std::size_t i = 0; i < lamps.size(); ++i)
      mLampOn[i] = lamps[i].lit || cand[i];
    buildField(w, g);
    const int all = heuristic(w);
    std::vector<char> need(lamps.size(), 0);
    if (all < kInf)
      for (std::size_t c = 0; c < lamps.size(); ++c)
      {
        if (!cand[c])
          continue;
        mLampOn[c] = 0;
        buildField(w, g);
        need[c] = heuristic(w) > all + 30;
        mLampOn[c] = 1;
      }
    // The next plant along the way: the one furthest from the goal that is
    // still nearer to it than the runner.
    buildField(w, g);
    const int A = kAirBudget + 1;
    int bestPlant = -1, bestD = -1;
    for (std::size_t pi = 0; pi < gs.plants.size(); ++pi)
    {
      const auto& pl = gs.plants[pi];
      bool wanted = false;
      for (int l : pl.lamps)
        wanted = wanted || need[std::size_t(l)];
      if (!wanted)
        continue;
      int d = kInf;
      for (const auto& [tx, ty] : pl.tiles)
        for (int y = ty * kCellsPerTile - 2; y <= ty * kCellsPerTile + 1; ++y)
          for (int x = tx * kCellsPerTile - 3; x <= tx * kCellsPerTile + 1; ++x)
            if (x >= 0 && y >= 0 && x < mW && y < mH)
              d = std::min(d, mDist[std::size_t((y * mW + x) * A)]);
      if (d < kInf && d <= all + 4 && d > bestD)
      {
        bestD = d;
        bestPlant = int(pi);
      }
    }
    if (bestPlant >= 0)
    {
      int bestDist = -1;
      for (int l : gs.plants[std::size_t(bestPlant)].lamps)
      {
        const auto& lamp = lamps[std::size_t(l)];
        if (!need[std::size_t(l)] || lamp.sw < 0)
          continue;
        const auto& sw = gs.switches[std::size_t(lamp.sw)];
        const int dist = std::abs(sw.bx * kCellsPerTile - p.x) + std::abs(sw.by * kCellsPerTile - p.y);
        if (bestDist < 0 || dist < bestDist)
        {
          bestDist = dist;
          mLampWanted = l;
        }
      }
    }
    mLampOn.clear();
    mDist.clear(); // the real goal's field is built next
    static const bool debug = std::getenv("GR_PLANNER_DEBUG") != nullptr;
    if (debug)
      std::fprintf(stderr, "lamps: lamp %d wanted (plant %d, h %d)\n", mLampWanted, bestPlant, all);
  }
  if (mLampWanted < 0)
    return false;
  // Where to stand: level with the switch (a shot or the Trimmer's cone),
  // or under it to shoot up.
  const auto& sw = gs.switches[std::size_t(lamps[std::size_t(mLampWanted)].sw)];
  const int sx = sw.bx * kCellsPerTile, sy = sw.by * kCellsPerTile;
  int feet = -1;
  for (int y = sy + 2; y < sy + 40 && y < w.map().height(); ++y)
    if (w.map().solidTop(sx, y) || w.map().solidTop(sx + 1, y))
    {
      feet = y - 1;
      break;
    }
  if (feet < 0)
    return false;
  Goal sg;
  sg.kind = 13;
  sg.index = mLampWanted;
  if (feet - sy <= 3)
  {
    // Level: on the side the runner comes from, facing the switch.
    const bool fromLeft = p.x < sx;
    sg.x = fromLeft ? sx - 6 : sx + 2;
    sg.y = feet;
    sg.w = 4;
    sg.h = 1;
  }
  else
  {
    sg.x = sx - 2;
    sg.y = feet;
    sg.w = 4;
    sg.h = 1;
  }
  g = sg;
  return true;
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

// A vine, cap, spear or jump-shot macro: its input helper decides frame by
// frame (and when it is done), so a plan replays the inputs it recorded.
static bool selfTimedMacro(int m)
{
  return m == kVineRight || m == kVineLeft || capMacro(m) || spearMacro(m) || jumpShotMacro(m);
}

// Plays search macro m on child, one logic frame at a time, for frames
// frames (a self-timed macro instead runs until its input helper says
// done); any macro stops early once the level ends (child.state() !=
// Playing). prev is the input of the frame before (it carries the button
// edges across) and ends as the macro's last input. seq collects a
// self-timed macro's inputs for the plan to replay, one per frame played,
// so its size is how long that macro ran (the others are rebuilt from
// kMacros for their set frame count, see Node::frames). False: the macro
// failed, the runner died on the way, the bonus run failed
// (child.bonusFailed()), or, with falseAltarFails (level 14's bonus route),
// a false altar dropped; the child is no use then.
static bool rollMacro(World& child, int m, int frames, bool falseAltarFails, Input& prev, std::vector<Input>& seq)
{
  const bool selfTimed = selfTimedMacro(m);
  int launchedAt = -1;
  for (int f = 0; selfTimed || f < frames; ++f)
  {
    bool done = false, fail = false;
    const Input in = spearMacro(m) ? spearInput(child, m, f, done, fail)
      : capMacro(m)                ? capInput(child, m, f, done, fail)
      : jumpShotMacro(m)           ? jumpShotInput(m, f, done)
      : m == kVineRight || m == kVineLeft ? vineInput(child, m == kVineRight ? 1 : -1, f, launchedAt, done, fail)
                                          : macroInput(kMacros[m], f);
    if (fail)
      return false;
    if (done)
      break;
    if (selfTimed)
      seq.push_back(in);
    child.update(toPlayerInput(in, prev));
    prev = in;
    if (child.player().state == PlayerState::Dying || child.bonusFailed() || (falseAltarFails && dropsFalse(child)))
      return false;
    if (child.state() != WorldState::Playing)
      break;
  }
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

// worldStateHash's parts, in the order the fold has always taken (changing
// the order would change every hash, so it is kept). Each one carries on
// the running hash: h * base + term, in std::uint32_t, which wraps modulo
// 2^32 rather than overflowing int (task oe3). The bool and int terms
// convert to unsigned, negative ones wrapping (well defined).
namespace
{

// Switches, doors and pickups: levels 4 and 8-10, Golden Touch and level 17.
std::uint32_t foldSwitchesAndDoors(std::uint32_t powered, const World& world)
{
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
  // Level 10: sun doors.
  for (const auto& d : world.sunDoors())
    powered = powered * 2 + d.open;
  // Golden Touch: statues set, doors opened or gilded.
  if (world.golden())
  {
    for (const auto& e : world.enemies())
      powered = powered * 2 + e.alive;
    for (const auto& d : world.goldDoors())
      powered = powered * 3 + (d.gold ? 2 : d.open > 0);
    powered = powered * 2 + world.goldReached();
  }
  powered = powered * 5 + (world.green().grow ? world.green().size : 0);
  // Level 17: lamps lit or out, plants there or not.
  for (const auto& l : world.green().lamps)
    powered = powered * 2 + l.lit;
  for (const auto& pl : world.green().plants)
    powered = powered * 2 + (pl.shown > 0);
  return powered;
}

// Blocks that moved or broke: levels 23, 26, 27, 34-38, 40 and Throw Everything.
std::uint32_t foldSetPieces(std::uint32_t powered, const World& world)
{
  // Level 23: the side, the levers, the ghosts pinned as platforms.
  if (world.manor().on)
  {
    powered = powered * 2 + world.manor().side;
    powered = powered * 17 + world.manor().step;
    for (const auto& pl : world.manor().platforms)
      powered = powered * 131u + unsigned(pl.bx * 97 + pl.by);
  }
  // Level 26: buildings broken, towers fallen.
  for (const auto& b : world.city().buildings)
    powered = powered * 3u + (b.standing() ? 0u : (b.state == BuildingState::Lying ? 2u : 1u));
  powered = powered * 3 + world.film().gagState; // Level 27's bonus: the ladder gag
  powered = powered * 2 + world.fleet().hatchOpen; // Level 34: the bomb bay's hatch
  powered = powered * 131u + unsigned(world.tower().rot); // Level 35: how far the tower has turned
  // Level 36: where the metal crates rest (they are solid map blocks).
  for (const auto& c : world.warehouse().crates)
    powered = powered * 131u + unsigned(c.stamped ? c.x * 997 + c.y : 7);
  // Level 37: who the runner is, shutters, cutting tables, the push rack.
  const auto& wr = world.wardrobe();
  powered = powered * 131u + unsigned(world.characterIndex() * 7 + (world.player().turbo > 0));
  for (const auto& sh : wr.shutters)
    powered = powered * 2 + sh.open;
  for (const auto& t : wr.tables)
    powered = powered * 131u + unsigned(t.row);
  for (const auto& k : wr.pushRacks)
    powered = powered * 131u + unsigned(k.x);
  // Level 38: words struck (walls and doors gone), the bridge, Writer's Blocks sitting.
  const auto& ws = world.writers();
  for (const auto& wd : ws.words)
    if (wd.target == WordTarget::Rect)
      powered = powered * 2 + (wd.gone() ? 1 : 0);
  powered = powered * 131u + unsigned(ws.bridge.typed);
  for (const auto& b : ws.blocks)
    powered = powered * 131u + unsigned(b.stampX * 97 + b.stampY + 1);
  // Level 40: catwalk blocks a sandbag broke.
  for (const auto& g : world.flyTower().gaps)
    powered = powered * 131u + unsigned(g.bx * 97 + g.by + 1);
  // Throw Everything: the loose blocks where they lie (thrown ones are new map blocks).
  for (const auto& [lx, ly] : world.warehouse().loose)
    powered = powered * 131u + unsigned(lx * 97 + ly);
  return powered;
}

// Machines, barrels, doors and crystals: levels 31, 22, 42, 19 and 46.
std::uint32_t foldMechanisms(std::uint32_t powered, const World& world)
{
  // Level 31: machines powered (the generator's hatch, the lift).
  for (const auto& m : world.storm().machines)
    powered = powered * 2 + m.powered;
  // Level 22: barrels blown (bridges down, rock gone).
  for (const auto& b : world.west().barrels)
    powered = powered * 2 + b.blown;
  // Level 42: the ON AIR door and the switch's glass cover, open or shut.
  for (int li : {world.finale().door, world.finale().cover})
    powered = powered * 2 + (li >= 0 && !world.layers()[std::size_t(li)].solid);
  // Level 19: which way each chamber is turned, and the gates.
  for (const auto& z : world.grav().zones)
    powered = powered * 2 + (z.dir == Grav::Up);
  for (const auto& t : world.grav().gates)
    powered = powered * 2 + t.open;
  // Level 46: where the Swap Crystals are (a drifting one is anywhere on
  // its path until swapped).
  for (const auto& c : world.swapCrystals())
    powered = powered * 131u + (c.gone ? 1u : (c.drifting ? 2u : unsigned(c.x * 977 + c.y + 3)));
  return powered;
}

} // namespace

// Moved out of Planner::plan unchanged (task oe3's review), so the hashes
// and plans are bit-identical to the inline fold: the same terms in the
// same order, pendingDoor (the planner's mPendingDoor, -1 for none) folded
// in after the level 17 terms where the inline code had it.
std::uint32_t worldStateHash(const World& world, int pendingDoor)
{
  std::uint32_t powered = foldSwitchesAndDoors(0, world);
  powered = powered * 31 + pendingDoor + 1; // Level 10: the sun door the field takes as opening
  powered = foldSetPieces(powered, world);
  return foldMechanisms(powered, world);
}

// Every term goes through std::uint32_t, so the sum wraps modulo 2^32
// instead of overflowing int (task oe3). The fields keep the weights the
// int version had (kind 1000000, x 1000, the card 500000000, walls and the
// Bass Cannon 10000000, powered 7919), so the bits match what that version
// gave on a wrapping build.
std::uint32_t goalKeyHash(int kind, int x, int y, bool hasKey, int broken, bool sound, std::uint32_t powered)
{
  const auto u = [](int v) { return std::uint32_t(v); }; // negative values wrap too, well defined
  return u(kind) * 1000000u + u(x) * 1000u + u(y) + (hasKey ? 500000000u : 0u) +
    (u(std::min(broken, 15)) * 2u + (sound ? 1u : 0u)) * 10000000u + powered * 7919u;
}

namespace
{

// One node of a plan's search: a simulated world after a macro, linked to
// the node it was played from.
struct Node
{
  std::unique_ptr<World> w; // reset once the node is expanded (keeps memory flat)
  int parent;               // the node it was played from (-1: the start)
  int macro;                // the macro played to get here (-1: the start)
  int g;                    // cost so far
  int h;                    // the heuristic here (0 once the level ended)
  Input last;               // the macro's last input (carries the button edges on)
  // How long its macro ran: a self-timed macro's frames played (seq's
  // size); for any other macro its set frame count, which it also holds
  // when the macro stopped early because the level ended (the plan then
  // replays all of those frames).
  int frames;
  std::vector<Input> seq; // a self-timed macro's inputs
};

// Orders the search's open list: the lowest g + h on top.
struct ByScore
{
  const std::deque<Node>* nodes;
  bool operator()(int a, int b) const
  {
    const Node& na = (*nodes)[std::size_t(a)];
    const Node& nb = (*nodes)[std::size_t(b)];
    return na.g + na.h > nb.g + nb.h;
  }
};

// GR_BOT_LOUD=1: the bot does not mind Level 27's alarms.
bool botLoud()
{
  static const bool loud = std::getenv("GR_BOT_LOUD") != nullptr;
  return loud;
}

// The cost of a child node: its parent's g plus the macro's frames (a wait
// for the drop counts as a short one: it is the way up; a self-timed macro
// half its frames), 60 per heart lost, 90 per Level 27 alarm (as much as a
// lost heart and a half: a stealthy route is worth waiting for; loud:
// GR_BOT_LOUD=1 doesn't care) and 1 for a shot (macros 10-12).
int childCost(int g, const World& from, const World& to, int m, int frames, int length, bool loud)
{
  const bool selfTimed = selfTimedMacro(m);
  const int lost = std::max(0, from.player().hp - to.player().hp);
  const int alarms = loud ? 0 : std::max(0, alarmHeat(to) - alarmHeat(from));
  return g + (m == kWaitLaunch ? 8 : (m == kWaitVerse ? frames / 2 : (selfTimed ? length / 2 : length))) + lost * 60 +
    alarms * 90 + (m >= 10 && m <= 12 ? 1 : 0);
}

// searchKey's parts, folded in the order the inline key always took them
// (changing the order would change every key, and with it which states the
// search merges). Each one carries on the running key k through mix().

// The runner (where, how, which way, the clock's phase, HP, weapon, card),
// level 19's downs, what is alive and the items out.
std::uint64_t keyRunner(const World& w, int period)
{
  const auto& p = w.player();
  std::uint64_t k = 1469598103934665603ull;
  k = mix(k, std::uint64_t(p.x) | (std::uint64_t(p.y) << 16));
  k = mix(k, std::uint64_t(int(p.state)) | (std::uint64_t(std::min(p.frames, 12)) << 8) |
               (std::uint64_t(p.facing > 0) << 16) | (std::uint64_t(w.clock() % period) << 20));
  k = mix(k, std::uint64_t(p.hp) | (std::uint64_t(int(p.weapon)) << 8) | (std::uint64_t(p.hasKey) << 12));
  // Level 19: the runner's down, and the chambers'.
  if (w.grav().on)
  {
    std::uint64_t gk = std::uint64_t(int(p.grav)) + 1;
    for (const auto& z : w.grav().zones)
      gk = gk * 2 + (z.dir == Grav::Up);
    k = mix(k, gk);
  }
  int alive = 0;
  for (const auto& e : w.enemies())
    alive += e.alive;
  for (const auto& b : w.boxes())
    alive += b.alive * 64;
  return mix(k, std::uint64_t(alive) | (std::uint64_t(w.items().size()) << 20));
}

// Levels 29, 30 and 31: the signal, the trench, the storm.
std::uint64_t keySignalTrenchStorm(std::uint64_t k, const World& w)
{
  // Level 29: the signal (and a change on its way), the pips, the tint.
  if (w.signal().on)
  {
    const auto& sg = w.signal();
    k = mix(k, std::uint64_t(sg.color) | (std::uint64_t(sg.pending + 1) << 2) | (std::uint64_t(sg.flash) << 4) |
                 (std::uint64_t(sg.tint) << 8) | (std::uint64_t(sg.pips[0] > 0) << 10) |
                 (std::uint64_t(sg.pips[1] > 0) << 11) | (std::uint64_t(sg.pips[2] > 0) << 12) |
                 (std::uint64_t(sg.wallShut) << 13));
  }
  // Level 30: strokes used, the sinking's beat, the line.
  if (w.trench().on)
  {
    const auto& t = w.trench();
    k = mix(k, std::uint64_t(t.strokes) | (std::uint64_t(t.sinkTick % 3) << 4) | (std::uint64_t(t.line) << 6) |
                 (std::uint64_t(t.lineX0) << 8) | (std::uint64_t(t.lineY) << 20) |
                 (std::uint64_t(t.harpoons.size()) << 32) | (std::uint64_t(t.fade) << 36));
  }
  // Level 31: each zone's clock (its glow and strike), rods, powered
  // machines, a Foghorn's blast.
  if (w.storm().on)
  {
    const auto& s = w.storm();
    std::uint64_t sk = 7;
    for (const auto& z : s.zones)
      sk = sk * 131 + std::uint64_t(z.t);
    for (const auto& m : s.machines)
      sk = sk * 2 + std::uint64_t(m.powered);
    sk = sk * 3 + s.rods.size();
    for (const auto& e : w.enemies())
      if (e.alive && e.kind == EnemyKind::Foghorn)
        sk = sk * 64 + std::uint64_t(e.dive + e.tell * 9 + (e.cool > 0 ? 1 : 0) * 37);
    k = mix(k, sk);
  }
  return k;
}

// Level 32: the teaching dish's clock, the sharks', drones' and mechs'
// attacks on their way, a mech's HP, scrap falling.
std::uint64_t keyDish(std::uint64_t k, const World& w)
{
  if (!w.dish().on)
    return k;
  const auto& d = w.dish();
  std::uint64_t dk = 11;
  for (const auto& sd : d.small)
    dk = dk * 181 + std::uint64_t(sd.t);
  for (const auto& sh : d.sharks)
    dk = dk * 64 + std::uint64_t(sh.state * 21 + std::min(sh.t, 20));
  for (const auto& r : d.robots)
  {
    const Enemy& e = w.enemies()[std::size_t(r.enemy)];
    dk = dk * 97 + std::uint64_t(r.aimState * 31 + std::min(r.aimT, 30)) + (r.jam > 0 ? 3000u : 0u) +
      (e.cool > 0 ? 6000u : 0u);
    if (e.kind == EnemyKind::ScorpionMech)
      dk = dk * 16 + std::uint64_t(std::max(0, e.hp));
  }
  for (const auto& sc : d.scraps)
    dk = dk * 131 + std::uint64_t(sc.y);
  return mix(k, dk);
}

// Levels 21, 23 and 22: the zero-g script, the manor, the fuses.
std::uint64_t keyZeroManorWest(std::uint64_t k, const World& w)
{
  // Level 21: where the script's shifts and bulkheads are.
  if (!w.zero().shifts.empty())
  {
    std::uint64_t zk = 5;
    for (const auto& sh : w.zero().shifts)
      zk = zk * 131 + std::uint64_t(sh.done ? 130 : std::min(129, sh.t + 1));
    for (const auto& d : w.zero().doors)
      zk = zk * 11 + std::uint64_t(d.pos * 10.0f);
    k = mix(k, zk);
  }
  // Level 23: the side, the shimmer of a step through, the route's step,
  // the levers and the ghosts pinned as platforms.
  if (w.manor().on)
  {
    const auto& m = w.manor();
    std::uint64_t mk = std::uint64_t(m.side) | (std::uint64_t(m.flip) << 1) | (std::uint64_t(m.step) << 6);
    for (const auto& l : m.levers)
      mk = mk * 2 + std::uint64_t(l.state);
    for (const auto& pl : m.platforms)
      mk = mk * 131 + std::uint64_t(pl.bx * 97 + pl.by);
    k = mix(k, mk);
  }
  // Level 22: fuse cells burning or burnt, barrels hissing or blown.
  if (w.west().on)
  {
    std::uint64_t wk = 9;
    for (const auto& f : w.west().fuses)
      for (const auto& c : f.cells)
        wk = wk * 3 + std::uint64_t(c.state);
    for (const auto& b : w.west().barrels)
      wk = wk * 3 + (b.blown ? 2u : b.t >= 0 ? 1u : 0u);
    k = mix(k, wk);
  }
  return k;
}

// Growth Spurt and level 17, Bouncers and Keepers close by, a launch, the
// cracks, the film's gag.
std::uint64_t keyGreenAndNear(std::uint64_t k, const World& w)
{
  const auto& p = w.player();
  // Growth Spurt: the runner's size.
  if (w.green().grow)
    k = mix(k, std::uint64_t(w.green().size) + 77);
  // Level 17: lamps lit and plants grown.
  if (!w.green().plants.empty())
  {
    std::uint64_t gk = 0;
    for (const auto& l : w.green().lamps)
      gk = gk * 2 + l.lit;
    for (const auto& pl : w.green().plants)
      gk = gk * 61 + std::uint64_t(pl.shown);
    k = mix(k, gk);
  }
  // Bouncers and Keepers close by: shoving one away, or wearing a Keeper
  // down, is progress too.
  for (const auto& e : w.enemies())
    if (e.alive && (e.kind == EnemyKind::Bouncer || e.kind == EnemyKind::Stepper || e.kind == EnemyKind::Keeper) &&
        std::abs(e.x - p.x) < 24 && std::abs(e.y - p.y) < 16)
      k = mix(k, std::uint64_t(e.x) | (std::uint64_t(e.y) << 16) |
                   (e.kind == EnemyKind::Keeper ? std::uint64_t(e.hp) << 32 : 0));
  k = mix(k, std::uint64_t(w.launching()));
  int cracks = 0;
  for (const auto& b : w.breakables())
    cracks = cracks * 7 + b.hp;
  k = mix(k, std::uint64_t(cracks));
  if (w.film().on)
    k = mix(k, std::uint64_t(w.film().gagState) | (std::uint64_t(w.film().slide) << 2) |
                 (std::uint64_t(w.film().pratfall) << 8));
  return k;
}

// Levels 27, 26 and 15: the noir alarm, the city, the station.
std::uint64_t keyNoirCityStation(std::uint64_t k, const World& w)
{
  const auto& p = w.player();
  // Level 27: the alarm, the "?" and where the Flatfeet and Goons are.
  if (w.noir().on)
  {
    const auto& nr = w.noir();
    std::uint64_t nk = std::uint64_t(nr.alarm > 0) | (std::uint64_t(nr.suspicion) << 1) |
      (std::uint64_t(nr.alarms) << 5);
    for (const auto& e : w.enemies())
      if (e.alive && !e.hidden && (e.kind == EnemyKind::Flatfoot || e.kind == EnemyKind::Goon) &&
          std::abs(e.x - p.x) < 48)
        nk = nk * 131 + std::uint64_t(e.x * 2 + (e.dir > 0)) + (std::uint64_t(e.hp) << 20);
    k = mix(k, nk);
  }
  // Level 26: which buildings stand, how cracked the roof under the runner is.
  if (w.city().on)
  {
    std::uint64_t c = 0;
    for (const auto& b : w.city().buildings)
      c = c * 7 + std::uint64_t(int(b.state)) + std::uint64_t(std::min(b.cracks, 1));
    k = mix(k, c);
  }
  // Level 15: crates (shot, thrown, sucked out), charges out, open panels.
  if (w.station().on())
  {
    const auto& st = w.station();
    std::uint64_t c = 0;
    for (const auto& cr : st.crates)
      c = c * 3 + (cr.alive ? 1 + cr.loose : 0);
    k = mix(k, c);
    k = mix(k, std::uint64_t(st.charges.size()) | (std::uint64_t(st.caught + 1) << 8) |
                 (std::uint64_t(st.detonate) << 16));
    for (const auto& pn : st.panels)
      k = mix(k, std::uint64_t(pn.open) | (std::uint64_t(pn.cool) << 16));
  }
  return k;
}

// The bonus asked for and the prototype found; platforms, breakers, doors,
// Leeches and Totems, fluids, bubbles, the trail; level 8's vines.
std::uint64_t keyProps(std::uint64_t k, const World& w)
{
  const auto& p = w.player();
  k = mix(k, std::uint64_t(w.bonusRequested()) | (std::uint64_t(w.stats().protoFound) << 1));
  for (const auto& pl : w.platforms())
    k = mix(k, std::uint64_t(pl.y) | (std::uint64_t(pl.x) << 16) | (std::uint64_t(pl.braked) << 32));
  for (const auto& b : w.breakers())
    k = mix(k, std::uint64_t(b.on) | (std::uint64_t(b.throwing) << 1));
  for (const auto& d : w.doors())
    k = mix(k, std::uint64_t(d.open) | (std::uint64_t(d.solid) << 16));
  for (const auto& e : w.enemies())
    if (e.alive && (e.kind == EnemyKind::Leech || e.kind == EnemyKind::Totem))
      k = mix(k, std::uint64_t(e.aimX) | (std::uint64_t(e.hp) << 16));
  for (const auto& f : w.fluids())
    k = mix(k, std::uint64_t(f.surface) | (std::uint64_t(std::uint32_t(f.floodAt)) << 16));
  for (const auto& b : w.bubbles())
    k = mix(k, std::uint64_t(b.x) | (std::uint64_t(b.y) << 16) | (std::uint64_t(b.stood) << 32));
  for (const auto& t : w.trail())
    k = mix(k, std::uint64_t(t.x) | (std::uint64_t(t.y) << 16));
  // Level 8: the vines close by (where they are in their swing, how high
  // they go), and the runner's swing and fling. (The bridges, the ropes and
  // what hangs from them follow in keySilkGooJungle.)
  for (const auto& v : w.vines())
    if (std::abs(v.ax - p.x) < 48)
      k = mix(k, std::uint64_t(v.t) | (std::uint64_t(v.amp) << 8) | (std::uint64_t(v.cool) << 16) |
                   (std::uint64_t(v.pump) << 24));
  if (p.state == PlayerState::Swing)
    k = mix(k, std::uint64_t(p.vine) | (std::uint64_t(p.vineAt) << 8));
  return mix(k, std::uint64_t(p.fling + 4) | (std::uint64_t(p.vineArc) << 4) | (std::uint64_t(p.airJumped) << 5) |
                  (std::uint64_t(p.airArc) << 6));
}

// Levels 45 and 46: the hive's tubes, the Swap Crystals.
std::uint64_t keyHiveCrystals(std::uint64_t k, const World& w)
{
  const auto& p = w.player();
  // Level 45: in a tube (how far along), the valves, shots in the tubes
  // and the mite pores.
  if (w.hasHive())
  {
    k = mix(k, std::uint64_t(p.tube + 1) | (std::uint64_t(p.tubeBranch) << 8) | (std::uint64_t(p.tubeS) << 16));
    for (const auto& t : w.gulletTubes())
      k = mix(k, std::uint64_t(t.set));
    for (const auto& e : w.enemies())
      if (e.alive && (e.kind == EnemyKind::Mite || e.kind == EnemyKind::Polyp || e.kind == EnemyKind::Warden) &&
          std::abs(e.x - p.x) < 40 && std::abs(e.y - p.y) < 24)
        k = mix(k, std::uint64_t(e.x) | (std::uint64_t(e.y) << 12) | (std::uint64_t(e.hp) << 24) |
                     (std::uint64_t(e.attach + e.tell * 4) << 32));
  }
  // Level 46: the Swap Crystals close by, and your shots on their way to
  // one (a shot up takes longer than a macro to get there).
  if (w.hasCrystals())
  {
    for (const auto& c : w.swapCrystals())
      if (std::abs(c.x - p.x) < 48 && std::abs(c.y - p.y) < 30)
        k = mix(k, std::uint64_t(c.x) | (std::uint64_t(c.y) << 16) | (std::uint64_t(c.cool) << 32) |
                     (std::uint64_t(c.gone) << 40));
    for (const auto& pr : w.projectiles())
      if (pr.alive && pr.kind != ShotKind::Enemy)
        k = mix(k, std::uint64_t(pr.x) | (std::uint64_t(pr.y) << 16) | (std::uint64_t(pr.dy + 2) << 32));
  }
  return k;
}

// Levels 47 and 44: Silk Lines, goo walls; level 8's bridges, ropes and loads.
std::uint64_t keySilkGooJungle(std::uint64_t k, const World& w)
{
  const auto& p = w.player();
  // Level 47: on a Silk Line (how far, how fast), and the lines close by
  // (spun how far, shivering before a cut).
  if (w.hasSilk())
  {
    k = mix(k, std::uint64_t(p.silk + 1) | (std::uint64_t(int(p.silkS * 2.0f)) << 8) |
                 (std::uint64_t(int(p.silkV * 10.0f)) << 24));
    for (const auto& l : w.silkLines())
      if (std::abs(l.ax - float(p.x)) < 90.0f && std::abs(l.ay - float(p.y)) < 60.0f)
        k = mix(k, std::uint64_t(int(l.spun)) | (std::uint64_t(l.twang) << 12) | (std::uint64_t(l.cool) << 20) |
                     (std::uint64_t(l.regrow + 1) << 28));
    for (const auto& e : w.enemies())
      if (e.alive && (e.kind == EnemyKind::LoomSpider || e.kind == EnemyKind::Dropling) && std::abs(e.x - p.x) < 48 &&
          std::abs(e.y - p.y) < 30)
        k = mix(k, std::uint64_t(e.x) | (std::uint64_t(e.y) << 12) | (std::uint64_t(e.hp) << 24) |
                     (std::uint64_t(e.attach + e.tell * 8) << 32));
  }
  // Level 44: on a goo wall, or kicked off one.
  if (w.hasGoo())
    k = mix(k, std::uint64_t(p.wall + 2) | (std::uint64_t(p.kick + 8) << 4) | (std::uint64_t(p.kickArc) << 8) |
                 (std::uint64_t(w.gooPatches().size()) << 12));
  // Level 8: the bridges, the ropes and what hangs from them.
  for (const auto& b : w.bridges())
    k = mix(k, std::uint64_t(b.down) | (std::uint64_t(b.chops) << 1) | (std::uint64_t(std::min(b.heavy, 63)) << 4) |
                 (std::uint64_t(b.left + 1) << 12) | (std::uint64_t(b.creak) << 24));
  for (const auto& rope : w.jungleRopes())
    k = mix(k, std::uint64_t(rope.hp) | (std::uint64_t(rope.cut) << 8));
  for (const auto& l : w.loads())
    k = mix(k, std::uint64_t(l.state) | (std::uint64_t(l.fall) << 4));
  return k;
}

// Level 9: traps close by, the floor cracking, keys and plates, and the
// Guardians and beetles near you.
std::uint64_t keyTomb(std::uint64_t k, const World& w)
{
  const auto& p = w.player();
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
  return k;
}

// Level 10: mirrors (where they are turning to), sun doors (how long lit),
// and the Monks, Wraiths and moths close by.
std::uint64_t keyTemple(std::uint64_t k, const World& w)
{
  const auto& p = w.player();
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
  return k;
}

// Level 11: the carts (where on their rails, how fast, in the air or not),
// caps on their fuses, the levers, and the moles, bats and bandits close by.
std::uint64_t keyMine(std::uint64_t k, const World& w)
{
  const auto& p = w.player();
  k = mix(k, std::uint64_t(p.cart + 1) | (std::uint64_t(p.cartDuck) << 8));
  for (const auto& c : w.carts())
    if (std::abs(int(c.fx) - p.x) < 64)
      k = mix(k, std::uint64_t(c.rail + 1) | (std::uint64_t(std::uint32_t(c.s)) << 8) | (std::uint64_t(c.speed) << 32) |
                   (std::uint64_t(c.dir > 0) << 40) | (std::uint64_t(c.hop) << 41) | (std::uint64_t(c.falling) << 48) |
                   (std::uint64_t(c.lost) << 49) | (std::uint64_t(int(c.fy) & 0xff) << 56));
  for (const auto& c : w.caps())
    k = mix(k, std::uint64_t(c.fuse) | (std::uint64_t(int(c.fx)) << 8) | (std::uint64_t(int(c.fy)) << 24));
  for (const auto& l : w.levers())
    k = mix(k, std::uint64_t(l.state));
  for (const auto& e : w.enemies())
    if (e.alive && (e.kind == EnemyKind::Mole || e.kind == EnemyKind::Bat || e.kind == EnemyKind::Bandit) &&
        std::abs(e.x - p.x) < 48 && std::abs(e.y - p.y) < 24)
      k = mix(k, std::uint64_t(e.x) | (std::uint64_t(e.y) << 12) | (std::uint64_t(e.hp) << 24) |
                   (std::uint64_t(e.attach + e.tell * 4) << 32) | (std::uint64_t(e.hidden) << 48));
  return k;
}

// The hunter: where its spot is and how far along a lock or salvo is, and
// its strikes.
std::uint64_t keyHunter(std::uint64_t k, const World& w)
{
  const auto& h = w.hunter();
  if (!h.on)
    return k;
  k = mix(k, std::uint64_t(int(h.sx)) | (std::uint64_t(int(h.sy)) << 16) |
               (std::uint64_t(std::min(h.locking, h.lock)) << 32) | (std::uint64_t(h.beep) << 40) |
               (std::uint64_t(h.left) << 48) | (std::uint64_t(h.cool > 0) << 56));
  for (const auto& st : w.strikes())
    k = mix(k, std::uint64_t(st.x) | (std::uint64_t(st.y) << 16) | (std::uint64_t(st.t) << 32));
  return k;
}

// The search's state key: two simulated worlds with the same key count as
// the same state, so the search keeps only the cheaper one. It holds what
// the plan's progress can depend on: the runner, the clock's phase within
// period (clockPeriod), and each level's moving parts and those close by.
std::uint64_t searchKey(const World& w, int period)
{
  std::uint64_t k = keyRunner(w, period);
  k = keySignalTrenchStorm(k, w);
  k = keyDish(k, w);
  k = keyZeroManorWest(k, w);
  k = keyGreenAndNear(k, w);
  k = keyNoirCityStation(k, w);
  k = keyProps(k, w);
  k = keyHiveCrystals(k, w);
  k = keySilkGooJungle(k, w);
  k = keyTomb(k, w);
  k = keyTemple(k, w);
  k = keyMine(k, w);
  return keyHunter(k, w);
}

} // namespace

// One plan's best-first search: from the start world, it plays every macro
// on the most promising node (lowest cost so far plus heuristic) until it
// reaches the goal, makes enough progress to a stable spot, or runs out of
// its node budget.
struct Planner::Search
{
  Search(const Planner& planner, const World& start, const Goal& goal);
  Search(const Search&) = delete; // open points into nodes
  Search& operator=(const Search&) = delete;

  int run(const Input& prev);
  bool stopsAt(int ni);
  bool reached(const World& nw) const;
  bool targetDone(const World& nw) const;
  // What expanding a node needs of it, copied out (see expand).
  struct Parent
  {
    int index;
    const World& w;
    int g;
    Input last;
  };
  void expand(int ni);
  void addChild(const Parent& parent, int m);
  void queueChain(int found, std::deque<Input>& queue) const;

  const Planner& planner; // its distance field gives the heuristic
  const World& start;
  const Goal& goal;
  const int period; // clockPeriod(start), for searchKey
  const int h0;     // the heuristic at the start
  const int hp0;    // the runner's HP at the start
  const bool loud;  // botLoud()
  // Grows while a node is expanded (its children are pushed back). A deque,
  // not a vector: push_back never moves the nodes already in it, so a
  // reference to one stays valid however large a plan's search grows (a
  // vector outgrowing its reserve once freed the node being expanded, task
  // ne3). Only push_back, index access and size() are used on it.
  std::deque<Node> nodes;
  std::priority_queue<int, std::vector<int>, ByScore> open; // indices into nodes
  std::unordered_map<std::uint64_t, int> seen;              // searchKey -> the lowest g reached there
  int bestStable = -1; // the stable node closest to the goal so far
  int expanded = 0;
};

Planner::Search::Search(const Planner& planner, const World& start, const Goal& goal)
  : planner(planner), start(start), goal(goal), period(clockPeriod(start)), h0(planner.heuristic(start)),
    hp0(start.player().hp), loud(botLoud()), open(ByScore{&nodes})
{
}

// Runs the search from the start world (prev: the input before it, for the
// button edges). Returns the node the plan goes to: one that stopped the
// search (stopsAt), else, once the budget is spent or the open list runs
// dry, the best stable node if it is closer than the start; -1 for none.
int Planner::Search::run(const Input& prev)
{
  nodes.push_back({start.cloneForSim(), -1, -1, 0, h0, prev, 0, {}});
  open.push(0);
  while (!open.empty() && expanded < kBudget)
  {
    const int ni = open.top();
    open.pop();
    if (stopsAt(ni))
      return ni;
    ++expanded;
    expand(ni);
    // Free worlds we will not expand again (keeps memory flat).
    nodes[std::size_t(ni)].w.reset();
  }
  if (bestStable >= 0 && nodes[std::size_t(bestStable)].h < h0)
    return bestStable;
  return -1;
}

// Whether the search stops at node ni (never the start): it reached the
// goal, or it is a stable, safe spot (no more than a heart lost, no train
// coming, no Flatfoot looking) at least kProgress closer to the goal.
// Short of that, a stable, safe node closer than any before is kept as
// bestStable.
bool Planner::Search::stopsAt(int ni)
{
  if (ni == 0)
    return false;
  const Node& n = nodes[std::size_t(ni)];
  const World& nw = *n.w;
  if (reached(nw))
    return true;
  if (!stable(nw) || nw.player().hp < hp0 - 1 || nw.trainDanger() ||
      !(loud || nw.noir().suspicion == 0 || nw.noir().alarm > 0)) // not stopping where a Flatfoot is looking
    return false;
  const bool done = n.h <= h0 - kProgress;
  const bool better = bestStable < 0 || n.h < nodes[std::size_t(bestStable)].h;
  if (!(done || better) || !safeAhead(nw))
    return false;
  if (done)
    return true;
  bestStable = ni;
  return false;
}

// The success predicate: in world nw the level ended, or the goal is met
// (the card or the prototype taken, the bonus asked for, a gem taken, the
// manor's step done, standing at the crypt's mirror, the goal's fuse lit or
// any fuse burning, or one of targetDone's).
bool Planner::Search::reached(const World& nw) const
{
  const auto& np = nw.player();
  const auto& p0 = start.player();
  const bool gotProto = nw.stats().protoFound &&
    (!start.stats().protoFound || (np.weapon == Weapon::Proto && (p0.weapon != Weapon::Proto || np.ammo > p0.ammo)));
  return nw.state() != WorldState::Playing || (goal.kind == 1 && np.hasKey) || targetDone(nw) ||
    (goal.kind == 2 && gotProto) || (goal.kind == 3 && nw.bonusRequested()) ||
    (goal.kind == 15 && nw.stats().gems > start.stats().gems) ||
    (goal.kind == 17 && nw.manor().step > start.manor().step) ||
    (goal.kind == 18 && np.state == PlayerState::OnGround && np.x + 1 >= goal.x && np.x + 1 < goal.x + goal.w &&
      np.y == goal.y + goal.h - 1) ||
    (goal.kind == 16 && std::size_t(goal.index) < nw.west().fuses.size() &&
      (nw.west().fuses[std::size_t(goal.index)].lit() || nw.west().anyBurning()));
}

// Goal kinds 4-14 met in world nw: the breaker thrown, the Leech gone, a
// stone key taken, the plate pressed, the mirror turned, in the shelter or
// beside the altar or at the valve, the block gilded or painted, the lamp lit
// (or lit longer).
bool Planner::Search::targetDone(const World& nw) const
{
  const std::size_t i = std::size_t(goal.index);
  const bool thrown =
    goal.kind == 4 && i < nw.breakers().size() && (nw.breakers()[i].on || nw.breakers()[i].throwing > 0);
  const bool leechGone = goal.kind == 5 && i < nw.enemies().size() && !nw.enemies()[i].alive;
  const bool keyTaken = goal.kind == 6 && nw.stoneKeysHeld() > start.stoneKeysHeld();
  const bool pressed = goal.kind == 7 && i < nw.plates().size() && nw.plates()[i].presses > start.plates()[i].presses;
  const bool turned = goal.kind == 8 && i < nw.mirrors().size() && nw.mirrors()[i].to == goal.h;
  const bool sheltered = (goal.kind == 9 && nw.inShelter(goal.index)) ||
    ((goal.kind == 10 || goal.kind == 11) && nw.besideAltar(goal.index)) ||
    (goal.kind == 12 && !nw.sweeps().paint &&
      nw.goldAt(goal.index % nw.level().width, goal.index / nw.level().width) != 1) ||
    (goal.kind == 12 && nw.sweeps().paint && nw.sweeps().paints[i] != 1) || // Level 41
    (goal.kind == 14 && nw.atValve(goal.index));
  const bool lit = goal.kind == 13 && i < nw.green().lamps.size() && nw.green().lamps[i].lit &&
    (!start.green().lamps[i].lit || nw.green().lamps[i].left > start.green().lamps[i].left);
  return thrown || leechGone || keyTaken || pressed || turned || sheltered || lit;
}

// Expands node ni: a child for every macro that can be played there.
// Expanding pushes the children onto nodes. Two guards keep that from
// reading freed memory (task ne3): nodes is a deque, whose push_back leaves
// existing elements in place, and what the expansion needs of the node is
// copied out here (Parent) rather than read through a reference to it (so a
// change back to a vector stays safe). Its World lives on the heap behind
// the node's unique_ptr and stays put either way.
void Planner::Search::expand(int ni)
{
  const Node& n = nodes[std::size_t(ni)];
  const Parent parent{ni, *n.w, n.g, n.last};
  for (int m = 0; m < kMacroCount; ++m)
    addChild(parent, m);
}

// Plays macro m from parent's world and keeps the child unless the macro
// failed, an equal state (searchKey) was reached as cheaply already, or the
// goal is out of reach from it.
void Planner::Search::addChild(const Parent& parent, int m)
{
  // Waiting only matters while something moves on its own.
  const int frames = macroFrames(m, parent.w);
  if (frames == 0)
    return;
  auto child = std::make_unique<World>(parent.w);
  Input prev = parent.last;
  std::vector<Input> seq;
  if (!rollMacro(*child, m, frames, goal.kind == 10, prev, seq))
    return;
  // The macro's length: a self-timed one's frames played (one recorded
  // input each), else its set frames (also when the level ended early).
  const int length = selfTimedMacro(m) ? int(seq.size()) : frames;
  const auto key = searchKey(*child, period);
  const int g = childCost(parent.g, parent.w, *child, m, frames, length, loud);
  const auto it = seen.find(key);
  if (it != seen.end() && it->second <= g)
    return;
  seen[key] = g;
  const int h = child->state() != WorldState::Playing ? 0 : planner.heuristic(*child);
  if (h >= kInf)
    return;
  nodes.push_back({std::move(child), parent.index, m, g, h, prev, length, std::move(seq)});
  open.push(int(nodes.size()) - 1);
}

// Queues the inputs from the start to node found: each node's macro in
// order, a self-timed one's recorded inputs, any other rebuilt from kMacros
// for its frames.
void Planner::Search::queueChain(int found, std::deque<Input>& queue) const
{
  std::vector<int> chain;
  for (int i = found; i > 0; i = nodes[std::size_t(i)].parent)
    chain.push_back(i);
  std::reverse(chain.begin(), chain.end());
  for (int i : chain)
  {
    const Node& n = nodes[std::size_t(i)];
    if (!n.seq.empty())
      queue.insert(queue.end(), n.seq.begin(), n.seq.end());
    else
      for (int f = 0; f < n.frames; ++f)
        queue.push_back(macroInput(kMacros[n.macro], f));
  }
}

// The goal for this plan: chooseGoal's, after the skip rules for targets
// the search keeps failing to reach (skipStuckTargets).
Planner::Goal Planner::pickGoal(const World& world)
{
  const auto& p0 = world.player();
  // A bonus the search gave up on gets another try further on: the way
  // there can be easier from the next room.
  if (mSkipBonus && mSkipBonusAt >= 0 && p0.x > mSkipBonusAt + 40)
  {
    mSkipBonus = false;
    mSkipBonusAt = -1;
  }
  Goal goal = chooseGoal(world);
  // A bonus target (the entrance, a plate, a shelter, an altar: kinds 3, 7,
  // 9, 10) the search keeps failing to reach is skipped for now (mSkipBonus)
  // and the goal chosen again.
  if ((goal.kind == 3 || goal.kind == 7 || goal.kind == 9 || goal.kind == 10) && mGoalKind == goal.kind && mFails >= 6)
  {
    mSkipBonus = true;
    mSkipBonusAt = p0.x;
    goal = chooseGoal(world);
  }
  // The Wireframe bonus: a gem the search keeps failing to reach is left
  // until another one is taken (from there the way can be easier).
  if (world.stats().gems != mGemSkipCount)
  {
    mGemSkip.clear();
    mGemSkipCount = world.stats().gems;
  }
  skipStuckTargets(world, goal);
  return goal;
}

// Gems, marked blocks and prototypes the search keeps failing to reach are
// skipped (goal chosen again); a prototype skipped gets another chance on
// level 23's other side, or once the card is in hand.
void Planner::skipStuckTargets(const World& world, Goal& goal)
{
  while (goal.kind == 15 && mGoalKind == 15 && mGoalIndex == goal.index && mFails >= 4)
  {
    mGemSkip.push_back(goal.index);
    mFails = 0;
    goal = chooseGoal(world);
  }
  // Golden Touch: a block the search keeps failing to reach is left.
  while (goal.kind == 12 && mGoalKind == 12 && mGoalIndex == goal.index && mFails >= 4)
  {
    mPaintSkip.push_back(goal.index);
    mFails = 0;
    goal = chooseGoal(world);
  }
  if (goal.kind == 2 && mGoalKind == 2 && mFails >= 6)
  {
    mSkipProto = true;
    mSkipHadKey = world.player().hasKey;
  }
  // Level 23: a prototype out of reach on one side may be there on the other.
  if (world.manor().on && world.manor().side != mManorSide)
  {
    mManorSide = world.manor().side;
    mSkipProto = false;
    goal = chooseGoal(world);
  }
  // Behind a force field, a prototype becomes reachable with the card.
  if (mSkipProto && !mSkipHadKey && world.player().hasKey)
  {
    mSkipProto = false;
    goal = chooseGoal(world);
  }
}

// Up to three tries at a goal the distance field reaches: the field is
// brought up to date for it (updateField), and a gem, a marked block, a
// bonus target or a prototype the field cannot reach is skipped for the
// next goal.
void Planner::settleGoal(const World& world, Goal& goal)
{
  for (int attempt = 0; attempt < 3; ++attempt)
  {
    if (mSkipProto && goal.kind == 2)
      goal = chooseGoal(world);
    updateField(world, goal);
    if (goal.kind == 15 && heuristic(world) >= kInf)
    {
      mGemSkip.push_back(goal.index);
      goal = chooseGoal(world);
      continue;
    }
    if (goal.kind == 12 && heuristic(world) >= kInf)
    {
      mPaintSkip.push_back(goal.index);
      goal = chooseGoal(world);
      continue;
    }
    if ((goal.kind != 2 && goal.kind != 3 && goal.kind != 7 && goal.kind != 9 && goal.kind != 10) ||
        heuristic(world) < kInf)
      break;
    if (goal.kind == 3 || goal.kind == 7 || goal.kind == 9 || goal.kind == 10)
    {
      mSkipBonus = true;
      mSkipBonusAt = -1;
      goal = chooseGoal(world);
      continue;
    }
    mSkipProto = true;
    mSkipHadKey = world.player().hasKey;
  }
}

// Fits the goal to level 10's sun doors and level 17's lamps, then rebuilds
// the distance field when its key (goalKeyHash) changed.
void Planner::updateField(const World& world, Goal& goal)
{
  // Level 10: a sun door in the way: turn a mirror, clear moths off the
  // beam, or go and wait for it.
  mPendingDoor = -1;
  if (!world.sunDoors().empty() && (goal.kind == 0 || goal.kind == 3))
    lightGoal(world, goal);
  mDoorOpen.assign(world.sunDoors().size(), 0);
  if (mPendingDoor >= 0)
    mDoorOpen = mRouteOpen;
  // Level 17: a grow lamp the way needs.
  if (!world.green().plants.empty() && (goal.kind == 0 || goal.kind == 3))
    lampGoal(world, goal);
  // The field also changes when a wall breaks or the Bass Cannon arrives.
  int broken = 0;
  for (const auto& b : world.breakables())
    broken += b.broken;
  const bool sound = world.player().weapon == Weapon::Proto && world.player().proto == int(ProtoId::BassCannon);
  // Everything else the field depends on (switches, doors, set pieces),
  // folded into one unsigned number by worldStateHash.
  const std::uint32_t powered = worldStateHash(world, mPendingDoor);
  const std::uint32_t keyHash = goalKeyHash(goal.kind, goal.x, goal.y, world.player().hasKey, broken, sound, powered);
  if (mDist.empty() || keyHash != mGoalKeyHash)
  {
    buildField(world, goal);
    mGoalKeyHash = keyHash;
    mGoalKind = goal.kind;
    mGoalIndex = goal.index;
  }
}

// Level 10: at a door that is about to open (or that a Monk will open),
// stand and wait: queues 8 idle frames and returns true.
bool Planner::waitAtPendingDoor(const World& world)
{
  if (mPendingDoor < 0 || std::size_t(mPendingDoor) >= world.sunDoors().size())
    return false;
  const SunDoor& d = world.sunDoors()[std::size_t(mPendingDoor)];
  const CellBox near{d.x * kCellsPerTile - 8, d.y * kCellsPerTile - 8, d.w * kCellsPerTile + 16,
    d.h * kCellsPerTile + 16};
  if (d.open || !near.intersects(world.player().box()) || !stable(world))
    return false;
  for (int f = 0; f < 8; ++f)
    mQueue.push_back(Input{});
  return true;
}

// Plans the next stretch of input into mQueue: picks and settles the goal,
// then searches toward it (Search). Without progress found it queues a
// wiggle so the next plan starts somewhere else.
void Planner::plan(const World& world)
{
  mLastNodes = 0; // stays 0 when this plan needs no search
  const auto& p0 = world.player();
  if (p0.state == PlayerState::Dying || p0.state == PlayerState::Teleporting || world.state() != WorldState::Playing)
  {
    mQueue.push_back(Input{});
    return;
  }
  Goal goal = pickGoal(world);
  settleGoal(world, goal);
  if (waitAtPendingDoor(world))
    return;
  Search search(*this, world, goal);
  const int found = search.run(mPrev);
  mLastNodes = int(search.nodes.size());
  static const bool debug = std::getenv("GR_PLANNER_DEBUG") != nullptr;
  if (debug)
    std::fprintf(stderr, "plan f%d at %d,%d goal %d h0 %d expanded %d nodes %zu -> %s (h %d)\n", world.clock(), p0.x,
      p0.y, goal.kind, search.h0, search.expanded, search.nodes.size(), found < 0 ? "none" : "ok",
      found < 0 ? -1 : search.nodes[std::size_t(found)].h);
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
  search.queueChain(found, mQueue);
}

} // namespace gr
