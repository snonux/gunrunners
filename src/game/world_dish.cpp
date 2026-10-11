// Level 32, Dish Array: the big dishes' shared cycle (their rims and feed
// arms as rides, their struts as layers), the teaching dish, the Signal
// Jammer, Sand Sharks, Vulture Drones and Scorpion Mechs, the minidishes and
// the Heatstroke bonus (rules=swap_lr). Drawing is in world_dish_draw.cpp.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace gr
{

namespace
{

constexpr float kPi = 3.14159265f;

int sgn(int v) { return (v > 0) - (v < 0); }

int poseOf(const std::string& s)
{
  if (s == "east" || s == "e" || s == "E")
    return 0;
  if (s == "over" || s == "overhead" || s == "o" || s == "O")
    return 1;
  if (s == "west" || s == "w" || s == "W")
    return 2;
  return -1;
}

// The cycles: hold, turn, hold, turn, hold, swing back.
struct Leg
{
  int start, end;
  DishPose from, to;
};
constexpr Leg kBig[] = {{0, 240, DishPose::East, DishPose::East}, {240, 285, DishPose::East, DishPose::Over},
  {285, 525, DishPose::Over, DishPose::Over}, {525, 570, DishPose::Over, DishPose::West},
  {570, 810, DishPose::West, DishPose::West}, {810, 900, DishPose::West, DishPose::East}};
constexpr Leg kSmall[] = {{0, 45, DishPose::East, DishPose::East}, {45, 60, DishPose::East, DishPose::Over},
  {60, 105, DishPose::Over, DishPose::Over}, {105, 120, DishPose::Over, DishPose::West},
  {120, 150, DishPose::West, DishPose::West}, {150, 180, DishPose::West, DishPose::East}};

} // namespace

DishPhase dishPhase(int t, bool small)
{
  const Leg* legs = small ? kSmall : kBig;
  const int cycle = small ? kSmallCycle : kDishCycle;
  const int groan = small ? kSmallGroan : kDishGroan;
  t = ((t % cycle) + cycle) % cycle;
  DishPhase ph;
  for (int i = 0; i < 6; ++i)
  {
    const Leg& l = legs[i];
    if (t < l.start || t >= l.end)
      continue;
    ph.from = l.from;
    ph.to = l.to;
    if (l.from != l.to)
    {
      ph.k = t - l.start;
      ph.n = l.end - l.start;
    }
    else
    {
      // The groan before the next turn.
      ph.groan = t >= l.end - groan;
    }
    break;
  }
  return ph;
}

CellBox JamCone::box() const
{
  return {dir > 0 ? x : x - kJamLength + 1, y - kJamHeight / 2, kJamLength, kJamHeight};
}

// --- Setup ---------------------------------------------------------------------------------

bool World::setupDishEntity(const EntityDef& e)
{
  if (mLevel->themeKey != "desert_array")
    return false;
  auto& s = mDish;
  s.on = true;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  if (e.kind == "satellite")
  {
    s.satellitePeriod = std::max(60, e.num("period", kDishCycle));
    return true;
  }
  if (e.kind == "dish" && e.hasPos)
  {
    SmallDish d;
    d.id = e.id;
    d.x = e.x;
    d.y = e.y;
    d.radius = e.num("radius", kSmallRadius);
    s.small.push_back(d);
    return true;
  }
  if (e.kind == "sand" && e.rect("rect", x0, y0, x1, y1))
  {
    s.sand.push_back({x0 * kCellsPerTile, y0 * kCellsPerTile, (x1 - x0 + 1) * kCellsPerTile, (y1 - y0 + 1) * kCellsPerTile});
    return true;
  }
  if (e.kind == "minidish" && e.hasPos)
  {
    MiniDish m;
    m.x = e.x * kCellsPerTile;
    m.y = e.y * kCellsPerTile + 1;
    s.minis.push_back(m);
    return true;
  }
  if (e.kind == "dummy" && e.hasPos)
  {
    s.dummyX = e.x * kCellsPerTile + 1;
    s.dummyY = e.y * kCellsPerTile + 1;
    return true;
  }
  if (e.kind == "cactus" && e.hasPos)
  {
    s.cactusX = e.x * kCellsPerTile;
    s.cactusY = e.y * kCellsPerTile + 1;
    return true;
  }
  if (e.kind == "duck")
    return true; // the duck's sunglasses are only drawn
  return false;
}

void World::setupDishEnemy(Enemy& en, const EntityDef& e)
{
  auto& s = mDish;
  if (!s.on)
    return;
  const int index = int(mEnemies.size()) - 1;
  if (en.kind == EnemyKind::VultureDrone || en.kind == EnemyKind::ScorpionMech)
  {
    Robot r;
    r.enemy = index;
    const std::vector<int> zone = e.list("zone");
    if (zone.size() >= 2)
    {
      r.zx0 = std::min(zone[0], zone[1]) * kCellsPerTile;
      r.zx1 = (std::max(zone[0], zone[1]) + 1) * kCellsPerTile;
    }
    else
    {
      r.zx0 = en.x - 30;
      r.zx1 = en.x + 30;
    }
    r.phase = (en.x * 7) % 120;
    s.robots.push_back(r);
  }
  if (en.kind == EnemyKind::SandShark)
  {
    Shark k;
    k.enemy = index;
    en.hidden = true;
    s.sharks.push_back(k);
  }
}

void World::linkDish()
{
  auto& s = mDish;
  s.on = s.on || mLevel->themeKey == "desert_array";
  s.swapLr = mLevel->rules.find("swap_lr") != std::string::npos;
  s.harmlessScrap = s.swapLr;
  if (!s.on && !s.swapLr)
    return;
  // Rims and feed arms: platforms with mode=script and three poses.
  for (const auto& e : mLevel->entities)
  {
    if (e.kind == "platform" && e.str("mode") == "script")
    {
      const auto path = e.path("path");
      if (path.empty())
        continue;
      DishRide ride;
      for (std::size_t i = 0; i < mPlatforms.size(); ++i)
        if (!e.id.empty() && mPlatforms[i].id == e.id)
          ride.platform = int(i);
      if (ride.platform < 0)
        continue;
      for (int k = 0; k < 3; ++k)
      {
        const auto& pt = path[std::min(std::size_t(k), path.size() - 1)];
        ride.px[k] = pt.first * kCellsPerTile;
        ride.py[k] = pt.second * kCellsPerTile;
      }
      const std::string dish = e.str("dish");
      for (std::size_t i = 0; i < s.small.size(); ++i)
        if (!dish.empty() && s.small[i].id == dish)
          ride.dish = int(i);
      mPlatforms[std::size_t(ride.platform)].dead = true; // the dish moves it
      s.rides.push_back(ride);
    }
    if (e.kind == "layer" && e.str("driver") == "script" && e.has("state"))
    {
      DishStrut st;
      for (std::size_t i = 0; i < mLayers.size(); ++i)
        if (!e.id.empty() && mLayers[i].id == e.id)
          st.layer = int(i);
      if (st.layer < 0)
        continue;
      const std::string list = e.str("state");
      std::size_t at = 0;
      while (at < list.size())
      {
        const std::size_t comma = std::min(list.find(',', at), list.size());
        const int pose = poseOf(list.substr(at, comma - at));
        if (pose >= 0)
          st.solidIn[pose] = true;
        at = comma + 1;
      }
      const std::string dish = e.str("dish");
      for (std::size_t i = 0; i < s.small.size(); ++i)
        if (!dish.empty() && s.small[i].id == dish)
          st.dish = int(i);
      s.struts.push_back(st);
    }
  }
  // Vulture Drones start at their height over the ground.
  for (const auto& r : s.robots)
  {
    Enemy& e = mEnemies[std::size_t(r.enemy)];
    if (e.kind == EnemyKind::VultureDrone)
      e.y = groundUnder(e.x + e.w / 2, e.y) - kVultureHigh;
  }
  placeDishes(true);
  updateLayers(true);
}

// --- The dishes ----------------------------------------------------------------------------

DishPhase World::ridePhase(int dish) const
{
  const auto& s = mDish;
  if (dish >= 0 && std::size_t(dish) < s.small.size())
    return dishPhase(s.small[std::size_t(dish)].t, true);
  return dishPhase(s.t, false);
}

// Moves each ride toward where its dish has it now (two cells a frame at
// most, carrying its rider), and sets each strut solid or not. snap: put
// them there at once (setup, a load).
void World::placeDishes(bool snap)
{
  auto& s = mDish;
  for (const auto& ride : s.rides)
  {
    Platform& pl = mPlatforms[std::size_t(ride.platform)];
    const DishPhase ph = ridePhase(ride.dish);
    const int a = int(ph.from), b = int(ph.to);
    int tx = ride.px[a], ty = ride.py[a];
    if (ph.n > 0)
    {
      tx = ride.px[a] + (ride.px[b] - ride.px[a]) * ph.k / ph.n;
      ty = ride.py[a] + (ride.py[b] - ride.py[a]) * ph.k / ph.n;
    }
    if (snap)
    {
      pl.x = pl.prevX = tx;
      pl.y = pl.prevY = ty;
      continue;
    }
    for (int step = 0; step < 2 && (pl.x != tx || pl.y != ty); ++step)
    {
      const int dx = sgn(tx - pl.x), dy = sgn(ty - pl.y);
      // A move that would crush its rider waits (and catches up after).
      if (!movePlatform(pl, dx, dy) && dx != 0)
        movePlatform(pl, dx, 0);
    }
  }
  for (const auto& st : s.struts)
  {
    const DishPhase ph = ridePhase(st.dish);
    const bool solid = ph.n > 0 ? st.solidIn[int(ph.from)] && st.solidIn[int(ph.to)] : st.solidIn[int(ph.from)];
    mLayers[std::size_t(st.layer)].scriptSolid = solid;
  }
}

void World::updateDishRides()
{
  auto& s = mDish;
  if (!s.on)
    return;
  const auto& p = mPlayer;
  const int before = s.t;
  s.t = (s.t + 1) % kDishCycle;
  const bool hear = !mSimulation;
  if (hear && dishPhase(s.t, false).groan && !dishPhase(before, false).groan)
    playSound(Sfx::Rumble);
  for (auto& d : s.small)
  {
    const int dx = std::abs(p.x / kCellsPerTile - d.x), dy = std::abs(p.y / kCellsPerTile - d.y);
    d.turning = dx <= d.radius && dy <= d.radius;
    if (!d.turning)
      continue;
    const int was = d.t;
    d.t = (d.t + 1) % kSmallCycle;
    if (hear && dishPhase(d.t, true).groan && !dishPhase(was, true).groan)
      playSound(Sfx::Rumble);
  }
  placeDishes(false);
}

// --- The Signal Jammer ---------------------------------------------------------------------

void World::fireJam(int ox, int oy, int dir)
{
  auto& s = mDish;
  JamCone c;
  c.dir = dir == 0 ? mPlayer.facing : dir;
  c.x = c.dir < 0 ? ox + 1 : ox;
  c.y = oy;
  s.cones.push_back(c);
  // The minidishes and the cactus take it as a shot.
  shotAtDishProps(c.box());
  if (!mSimulation)
    playSound(Sfx::Zap);
}

// The enemy (or the dummy) a jammed robot goes for: the nearest.
bool World::jamTarget(Robot& r, const Enemy& self, CellBox& out)
{
  const auto& s = mDish;
  const int cx = self.x + self.w / 2, cy = self.y - self.h / 2;
  long best = -1;
  r.target = -1;
  for (std::size_t i = 0; i < mEnemies.size(); ++i)
  {
    const Enemy& o = mEnemies[i];
    if (&o == &self || !o.alive || o.hidden || !o.active)
      continue;
    const long dx = o.x + o.w / 2 - cx, dy = o.y - o.h / 2 - cy;
    const long d = dx * dx + dy * dy;
    if (best < 0 || d < best)
    {
      best = d;
      r.target = int(i);
      out = o.box();
    }
  }
  if (s.dummyX >= 0)
  {
    const long dx = s.dummyX - cx, dy = s.dummyY - cy;
    const long d = dx * dx + dy * dy;
    if (best < 0 || d < best)
    {
      best = d;
      r.target = -2;
      out = {s.dummyX - 1, s.dummyY - 1, 2, 2};
    }
  }
  return best >= 0;
}

// The first solid cell row under (x, y), or the map's bottom.
int World::groundUnder(int x, int y) const
{
  int g = std::max(0, y);
  while (g < mMap.height() && !mMap.solid(x, g))
    ++g;
  return g;
}

Robot* World::robotOf(const Enemy& e)
{
  const int index = int(&e - mEnemies.data());
  for (auto& r : mDish.robots)
    if (r.enemy == index)
      return &r;
  return nullptr;
}

void World::updateJamCones()
{
  auto& s = mDish;
  for (auto& c : s.cones)
  {
    const CellBox b = c.box();
    for (std::size_t i = 0; i < mEnemies.size(); ++i)
    {
      Enemy& e = mEnemies[i];
      if (!e.alive || !e.active || e.hidden || !e.box().intersects(b))
        continue;
      if (std::find(c.hit.begin(), c.hit.end(), e.id) != c.hit.end())
        continue;
      c.hit.push_back(e.id);
      damageEnemy(e, protoDef(ProtoId::SignalJammer).damage);
      if (!e.alive)
      {
        ++mStats.protoKills;
        continue;
      }
      if (e.flags() & kEnemyRobot)
      {
        if (Robot* r = robotOf(e))
        {
          r->jam = kJamFrames;
          r->aimState = 0;
          e.tell = 0;
        }
      }
      else
        e.stun = kJamStun; // an organic enemy only flinches
    }
    --c.life;
  }
  s.cones.erase(std::remove_if(s.cones.begin(), s.cones.end(), [](const JamCone& c) { return c.life <= 0; }), s.cones.end());
}

// --- Props ---------------------------------------------------------------------------------

bool World::shotAtDishProps(const CellBox& hit)
{
  auto& s = mDish;
  bool any = false;
  for (auto& m : s.minis)
    if (m.turn == 0 && CellBox{m.x, m.y - 3, 2, 4}.intersects(hit))
    {
      m.pose = (m.pose + 1) % 3;
      m.turn = 8;
      any = true;
      if (!mSimulation)
        playSound(Sfx::Clunk);
    }
  if (s.cactusX >= 0 && s.hatUp == 0 && CellBox{s.cactusX, s.cactusY - 3, 2, 4}.intersects(hit))
  {
    s.hatUp = 20;
    any = true;
    if (!mSimulation)
      playSound(Sfx::Boing);
  }
  return any;
}

bool World::shotAtDish(Projectile& pr, const CellBox& hit)
{
  if (pr.kind == ShotKind::Enemy || !shotAtDishProps(hit))
    return false;
  pr.alive = false;
  return true;
}

// --- Update --------------------------------------------------------------------------------

void World::updateDish(const PlayerInput& input)
{
  (void)input;
  auto& s = mDish;
  auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  // Heatstroke: left and right swap every 150 frames.
  if (s.swapLr)
  {
    if (++s.swapT >= kSwapEvery)
    {
      s.swapT = 0;
      s.swapped = !s.swapped;
      if (!mSimulation)
        playSound(Sfx::Swap);
    }
  }
  updateJamCones();
  for (auto& r : s.robots)
    if (r.jam > 0)
      --r.jam;
  // At most two Sand Sharks swim at once: the two nearest the runner.
  if (!s.sharks.empty())
  {
    std::vector<std::pair<int, std::size_t>> near;
    for (std::size_t i = 0; i < s.sharks.size(); ++i)
    {
      const Enemy& e = mEnemies[std::size_t(s.sharks[i].enemy)];
      if (e.alive)
        near.emplace_back(std::abs(e.x + e.w / 2 - (p.x + 1)), i);
    }
    std::sort(near.begin(), near.end());
    for (auto& k : s.sharks)
      k.awake = false;
    for (std::size_t i = 0; i < near.size() && int(i) < kSharksAwake; ++i)
      s.sharks[near[i].second].awake = true;
  }
  // Scrap falls (a cell a frame faster each frame, 3 at most).
  for (auto& sc : s.scraps)
  {
    sc.vy = std::min(kScrapCap, sc.vy + 1);
    ++sc.fall;
    for (int i = 0; i < sc.vy && sc.vy > 0; ++i)
    {
      ++sc.y;
      const CellBox b{sc.x, sc.y, 2, 2};
      if (mMap.overlapsSolid(b) || sc.y > mMap.height())
      {
        sc.vy = -1; // landed: gone
        break;
      }
      if (alive && b.intersects(p.hitBox()))
      {
        if (s.harmlessScrap)
        {
          // The bonus: a hit knocks the runner back 2 blocks.
          mMap.moveHorizontally(p.x, p.y, Player::kWidth, p.height(), -p.facing * 4);
          if (p.state == PlayerState::OnGround && !mMap.onSolidGround(p.box()))
            startFalling();
          if (!mSimulation)
            playSound(Sfx::Clunk);
        }
        else if (sc.carrier)
        {
          if (p.virus == 0 && p.mercy == 0)
            infect();
        }
        else
          hurtPlayer(1);
        sc.vy = -1;
        break;
      }
      if (sc.jammed)
      {
        bool hit = false;
        for (auto& e : mEnemies)
          if (e.alive && !e.hidden && e.box().intersects(b))
          {
            damageEnemy(e, kScrapEnemy);
            hit = true;
            break;
          }
        if (hit)
        {
          sc.vy = -1;
          break;
        }
      }
    }
  }
  s.scraps.erase(std::remove_if(s.scraps.begin(), s.scraps.end(), [](const Scrap& sc) { return sc.vy < 0; }), s.scraps.end());
  for (auto& b : s.beams)
    --b.frames;
  s.beams.erase(std::remove_if(s.beams.begin(), s.beams.end(), [](const Beam& b) { return b.frames <= 0; }), s.beams.end());
  // The minidishes: all five overhead while the big dishes hold overhead and
  // the speakers play the opening theme (backwards, the story goes).
  for (auto& m : s.minis)
    if (m.turn > 0)
      --m.turn;
  if (s.hatUp > 0)
    --s.hatUp;
  if (!s.eggDone && !s.minis.empty())
  {
    const DishPhase ph = dishPhase(s.t, false);
    bool all = ph.n == 0 && ph.from == DishPose::Over;
    for (const auto& m : s.minis)
      all = all && m.pose == 1;
    if (all)
    {
      s.eggDone = true;
      s.egg = kEggFrames;
      mMusicOverride = "theme_opening";
      showMessage("THE DISHES PICK UP SOMETHING OLD");
    }
  }
  if (s.egg > 0 && --s.egg == 0)
    mMusicOverride = mLevel->music;
}

// --- Enemies -------------------------------------------------------------------------------

void World::updateSandShark(Enemy& e, const EnemyDef& def)
{
  auto& s = mDish;
  Shark* k = nullptr;
  for (auto& kk : s.sharks)
    if (kk.enemy == int(&e - mEnemies.data()))
      k = &kk;
  if (!k)
    return;
  if (k->sx1 <= k->sx0)
  {
    // Its sand: the `@ sand` rect it sits in.
    k->sx0 = e.x;
    k->sx1 = e.x + e.w;
    k->surface = e.y + 1;
    for (const auto& r : s.sand)
      if (r.contains(e.x, e.y + 1) || r.contains(e.x + e.w - 1, e.y + 1))
      {
        k->sx0 = r.x;
        k->sx1 = r.x + r.w;
        k->surface = r.y;
        break;
      }
  }
  const auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  bool onSand = false;
  if (alive && p.state == PlayerState::OnGround)
    for (const auto& r : s.sand)
      onSand = onSand || CellBox{p.x, p.y + 1, Player::kWidth, 1}.intersects(r);
  if (e.cool > 0)
    --e.cool;
  const int cx = e.x + e.w / 2, pcx = p.x + 1;
  switch (k->state)
  {
    case 0: // under the sand, only the fin showing
    {
      e.hidden = true;
      if (!k->awake || !alive)
        break;
      int goal = pcx;
      if (!onSand)
        goal = pcx + (e.dir < 0 ? -kSharkCircle : kSharkCircle); // circling 3 blocks out
      const int lo = k->sx0 + e.w / 2, hi = k->sx1 - e.w + e.w / 2;
      goal = std::clamp(goal, lo, hi);
      if (cx != goal)
        e.x += sgn(goal - cx);
      else if (!onSand)
        e.dir = -e.dir; // round it goes, the other way
      if (onSand && e.cool == 0 && std::abs(pcx - (e.x + e.w / 2)) <= kSharkReach)
      {
        k->state = 1;
        k->t = 0;
        e.tell = def.tell;
        if (!mSimulation)
          playSound(Sfx::Rumble);
      }
      break;
    }
    case 1: // the fin stops and the sand bulges
      e.tell = std::max(0, def.tell - k->t);
      if (++k->t >= def.tell)
      {
        k->state = 2;
        k->t = 0;
        e.tell = 0;
        e.hidden = false;
        if (!mSimulation)
          playSound(Sfx::Snap);
      }
      break;
    case 2: // out of the sand: it bites, and it can be hit
      e.hidden = false;
      if (++k->t >= kSharkBreach)
      {
        k->state = 0;
        k->t = 0;
        e.hidden = true;
        e.cool = def.cooldown;
      }
      break;
  }
  e.y = k->surface - 1;
}

void World::updateVultureDrone(Enemy& e, const EnemyDef& def)
{
  auto& s = mDish;
  Robot* r = robotOf(e);
  if (!r)
    return;
  const auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  ++r->phase;
  if (e.cool > 0)
    --e.cool;
  const int cx = e.x + e.w / 2;
  int tx = cx;
  bool target = false;
  CellBox tb{0, 0, 0, 0};
  if (r->jam > 0)
  {
    target = jamTarget(*r, e, tb);
    tx = tb.x + tb.w / 2;
  }
  else if (alive && p.x + 1 >= r->zx0 - 8 && p.x + 1 <= r->zx1 + 8)
  {
    tb = p.hitBox();
    tx = p.x + 1;
    target = true;
  }
  if (!target)
  {
    // Circling: back and forth over its stretch.
    if ((e.dir > 0 && cx >= r->zx1) || (e.dir < 0 && cx <= r->zx0))
      e.dir = -e.dir;
    tx = cx + (e.dir < 0 ? -1 : 1);
  }
  if (tx != cx)
  {
    e.dir = sgn(tx - cx);
    e.x += e.dir;
  }
  // It bobs up and down, 5 blocks or so over the ground under it.
  const float a = 2.0f * kPi * float(r->phase % 120) / 120.0f;
  const int wantY = groundUnder(e.x + e.w / 2, e.y) - kVultureHigh + int(std::lround(3.0f * std::sin(a)));
  e.y += sgn(wantY - e.y);
  // Over its target: scrap.
  if (target && e.cool == 0 && std::abs(tx - (e.x + e.w / 2)) <= 1 && tb.y > e.y)
  {
    Scrap sc;
    sc.x = e.x + e.w / 2 - 1;
    sc.y = e.y + 1;
    sc.carrier = e.carrier;
    sc.jammed = r->jam > 0;
    sc.groundY = sc.y;
    while (sc.groundY < mMap.height() && !mMap.overlapsSolid({sc.x, sc.groundY + 2, 2, 1}))
      ++sc.groundY;
    s.scraps.push_back(sc);
    e.cool = def.cooldown;
    if (!mSimulation)
      playSound(Sfx::Whistle);
  }
}

void World::fireBeam(Enemy& e, Robot& r)
{
  auto& s = mDish;
  auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  const int ox = e.x + e.w / 2, oy = e.y - e.h + 1;
  float dx = float(r.ax - ox), dy = float(r.ay - oy);
  const float len = std::sqrt(dx * dx + dy * dy);
  if (len < 0.5f)
  {
    dx = float(e.dir < 0 ? -1 : 1);
    dy = 0.0f;
  }
  else
  {
    dx /= len;
    dy /= len;
  }
  Beam b;
  b.x0 = ox;
  b.y0 = oy;
  bool hitPlayer = false;
  std::vector<int> hit;
  float fx = float(ox) + 0.5f, fy = float(oy) + 0.5f;
  int endX = ox, endY = oy;
  for (int i = 0; i < kBeamLength * 2; ++i)
  {
    fx += dx * 0.5f;
    fy += dy * 0.5f;
    const int cx = int(std::floor(fx)), cy = int(std::floor(fy));
    endX = cx;
    endY = cy;
    const CellBox c{cx, cy, 1, 1};
    if (mMap.overlapsSolid(c))
    {
      hitBreakable(c, 1, 8); // a `by=beam` wall
      break;
    }
    if (alive && !hitPlayer && c.intersects(p.hitBox()))
    {
      hitPlayer = true;
      if (e.carrier)
      {
        if (p.virus == 0 && p.mercy == 0)
          infect();
      }
      else
        hurtPlayer(1);
    }
    if (r.jam > 0)
      for (std::size_t j = 0; j < mEnemies.size(); ++j)
      {
        Enemy& o = mEnemies[j];
        if (&o == &e || !o.alive || o.hidden || !o.box().intersects(c) ||
            std::find(hit.begin(), hit.end(), int(j)) != hit.end())
          continue;
        hit.push_back(int(j));
        damageEnemy(o, kBeamEnemy);
      }
  }
  b.x1 = endX;
  b.y1 = endY;
  s.beams.push_back(b);
  if (!mSimulation)
    playSound(Sfx::EyeBeam);
}

void World::updateScorpionMech(Enemy& e, const EnemyDef& def)
{
  Robot* r = robotOf(e);
  if (!r)
    return;
  const auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  if (!mMap.onSolidGround(e.box()))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, 1);
    return;
  }
  if (e.cool > 0)
    --e.cool;
  const int ox = e.x + e.w / 2, oy = e.y - e.h + 1;
  CellBox tb{0, 0, 0, 0};
  bool has = false;
  if (r->jam > 0)
    has = jamTarget(*r, e, tb);
  else if (alive)
  {
    tb = p.hitBox();
    has = std::abs(tb.x + tb.w / 2 - ox) <= kMechSight && std::abs(tb.y + tb.h / 2 - oy) <= 16;
  }
  const int tcx = tb.x + tb.w / 2, tcy = tb.y + tb.h / 2;
  switch (r->aimState)
  {
    case 0: // patrolling, a cell every 4 frames
    {
      // It notices a runner in front of it (or right behind it).
      const bool seen = r->jam > 0 || sgn(tcx - ox) == (e.dir < 0 ? -1 : 1) || std::abs(tcx - ox) <= 8;
      if (has && seen && e.cool == 0)
      {
        r->aimState = 1;
        r->aimT = 0;
        if (tcx != ox)
          e.dir = sgn(tcx - ox);
        r->ax = ox + (e.dir < 0 ? -8 : 8);
        r->ay = oy;
        e.tell = kAimMax + kAimLock;
        if (!mSimulation)
          playSound(Sfx::EyeCharge);
        break;
      }
      if (e.timer % std::max(1, def.stepEvery) != 0)
        break;
      const CellBox b = e.box();
      const int aheadX = e.dir > 0 ? b.right() + 1 : b.left() - 1;
      const bool wall = e.dir > 0 ? mMap.touchingRightWall(b) : mMap.touchingLeftWall(b);
      const bool ledge = !mMap.solidTop(aheadX, b.bottom() + 1);
      if (wall || ledge)
        e.dir = -e.dir;
      else
        e.x += e.dir < 0 ? -1 : 1;
      break;
    }
    case 1: // the aim line turns toward the target, a cell a frame
      if (!has)
      {
        r->aimState = 0;
        e.tell = 0;
        break;
      }
      ++r->aimT;
      r->ax += sgn(tcx - r->ax);
      r->ay += sgn(tcy - r->ay);
      e.tell = std::max(1, kAimMax + kAimLock - r->aimT);
      if (r->aimT >= kAimMax || (r->aimT >= kAimLock && std::abs(tcx - r->ax) <= 1 && std::abs(tcy - r->ay) <= 1))
      {
        r->aimState = 2;
        r->aimT = 0;
        e.tell = kAimLock;
      }
      break;
    case 2: // locked
      e.tell = std::max(1, kAimLock - r->aimT);
      if (++r->aimT >= kAimLock)
      {
        fireBeam(e, *r);
        r->aimState = 0;
        e.tell = 0;
        e.cool = def.cooldown;
      }
      break;
  }
}

// --- Saves ---------------------------------------------------------------------------------

bool World::dishCanSave() const
{
  const auto& s = mDish;
  for (const auto& r : s.robots)
    if (r.jam > 0 || r.aimState != 0)
      return false;
  for (const auto& k : s.sharks)
    if (k.state != 0)
      return false;
  return s.cones.empty() && s.scraps.empty() && s.beams.empty() && s.egg == 0;
}

std::vector<int> World::dishSave() const
{
  const auto& s = mDish;
  std::vector<int> v{s.t, s.swapT, s.swapped, s.eggDone, int(s.small.size()), int(s.minis.size())};
  for (const auto& d : s.small)
    v.push_back(d.t);
  for (const auto& m : s.minis)
    v.push_back(m.pose);
  return v;
}

bool World::validDishSave(const std::vector<int>& v) const
{
  const auto& s = mDish;
  return v.size() == 6 + s.small.size() + s.minis.size() && v[4] == int(s.small.size()) && v[5] == int(s.minis.size()) &&
    v[0] >= 0 && v[0] < kDishCycle;
}

void World::loadDish(const std::vector<int>& v)
{
  auto& s = mDish;
  s.t = v[0];
  s.swapT = v[1];
  s.swapped = v[2] != 0;
  s.eggDone = v[3] != 0;
  std::size_t at = 6;
  for (auto& d : s.small)
    d.t = ((v[at++] % kSmallCycle) + kSmallCycle) % kSmallCycle;
  for (auto& m : s.minis)
    m.pose = std::clamp(v[at++], 0, 2);
  placeDishes(true);
  updateLayers(true);
}

} // namespace gr
