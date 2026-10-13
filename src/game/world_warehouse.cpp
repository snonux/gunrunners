// Level 36, Prop Warehouse: the magnet cranes and their levers, the metal
// crates (solid map blocks while they rest; held, sliding or falling off
// the map), the Magnet Gun's pull and push, Turbo shoving crates, the
// Forklift Bots, Stagehands and their flats, Prop Mimics, the props of the
// earlier levels on the shelves, the TURBODUDES crate, and the Throw
// Everything bonus (rules=throw_all). Drawing is in world_warehouse_draw.cpp.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace gr
{

namespace
{

int sgn(int v) { return (v > 0) - (v < 0); }

bool alive(const Player& p) { return p.state != PlayerState::Dying && p.state != PlayerState::Teleporting; }

} // namespace

// --- Setup ---------------------------------------------------------------------------------

bool World::setupWarehouseEntity(const EntityDef& e)
{
  const bool throwAll = mLevel->rules.find("throw_all") != std::string::npos;
  if (mLevel->themeKey != "prop_warehouse" && !throwAll)
    return false;
  auto& s = mWarehouse;
  s.on = true;
  s.throwAll = throwAll;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  const bool hasRect = e.rect("rect", x0, y0, x1, y1);
  if (e.kind == "crate" && e.hasPos && e.num("metal", 0) != 0)
  {
    MetalCrate c;
    c.id = e.id;
    c.x = c.homeX = e.x * kCellsPerTile;
    c.y = c.homeY = e.y * kCellsPerTile;
    s.crates.push_back(c);
    return true;
  }
  if (e.kind == "crane")
  {
    Crane c;
    c.id = e.id;
    c.leverId = e.str("lever");
    c.x0 = e.num("x0") * kCellsPerTile;
    c.x1 = (e.num("x1") - 1) * kCellsPerTile;
    c.rail = e.num("row") * kCellsPerTile + 1;
    c.x = c.startX = std::clamp(e.num("start", e.num("x0")) * kCellsPerTile, c.x0, c.x1);
    c.hook = c.rail + 2;
    c.ride = e.num("ride", 0) != 0;
    if (c.ride)
    {
      // The crate rides flush with the floor at `deck=`, out to `end=`.
      Platform pl;
      pl.id = "crane:" + c.id;
      pl.crane = true;
      pl.x = pl.prevX = c.x;
      pl.y = pl.prevY = pl.startY = pl.homeY = e.num("deck") * kCellsPerTile;
      pl.w = kCraneHead;
      pl.h = kCellsPerTile;
      c.x1 = std::clamp(e.num("end", e.num("x1") - 1) * kCellsPerTile, c.x0, c.x1);
      pl.path = {{c.x, pl.y}, {c.x1, pl.y}};
      pl.waitRider = pl.parked = true; // how the planner sees it
      c.platform = int(mPlatforms.size());
      c.hook = pl.y - 1;
      mPlatforms.push_back(pl);
      mPlatformPairs.push_back("");
    }
    s.cranes.push_back(c);
    return true;
  }
  if (e.kind == "switch" && e.str("kind") == "lever")
  {
    CraneLever l;
    l.id = e.id.empty() ? e.str("id") : e.id;
    l.x = (e.hasPos ? e.x : e.num("x")) * kCellsPerTile;
    l.y = (e.hasPos ? e.y : e.num("y")) * kCellsPerTile + 1;
    s.levers.push_back(l);
    return true;
  }
  if (e.kind == "foam" && hasRect)
  {
    s.foam.push_back({x0 * kCellsPerTile, y0 * kCellsPerTile, (x1 - x0 + 1) * kCellsPerTile, (y1 - y0 + 1) * kCellsPerTile});
    return true;
  }
  if (e.kind == "deco" && e.hasPos && e.str("kind") == "prop")
  {
    PropDeco d;
    d.level = e.num("level", 1);
    d.x = e.x * kCellsPerTile;
    d.y = e.y * kCellsPerTile + 1;
    s.props.push_back(d);
    return true;
  }
  if (e.kind == "turbocrate" && e.hasPos)
  {
    s.tdX = e.x * kCellsPerTile;
    s.tdY = e.y * kCellsPerTile + 1;
    return true;
  }
  if (e.kind == "step" && e.hasPos)
  {
    WarehouseStep st;
    st.x = e.x * kCellsPerTile;
    st.y = e.y * kCellsPerTile + 1;
    st.act = e.str("act");
    st.arg = e.str("arg");
    st.n = e.num("n", 0);
    s.route.push_back(st);
    return true;
  }
  if (e.kind == "loose" && hasRect)
  {
    for (int ty = y0; ty <= y1; ++ty)
      for (int tx = x0; tx <= x1; ++tx)
        s.loose.emplace_back(tx, ty);
    return true;
  }
  if (e.kind == "duck")
    return true; // the duck's hard hat and vest are only drawn
  return false;
}

void World::setupWarehouseEnemy(Enemy& en, const EntityDef& e)
{
  auto& s = mWarehouse;
  const int i = int(&en - mEnemies.data());
  if (en.kind == EnemyKind::Forklift)
  {
    Forklift f;
    f.enemy = i;
    const auto lane = e.list("lane");
    f.x0 = lane.size() == 2 ? lane[0] * kCellsPerTile : en.x - 20;
    f.x1 = lane.size() == 2 ? (lane[1] + 1) * kCellsPerTile - en.w : en.x + 20;
    s.forklifts.push_back(f);
  }
  else if (en.kind == EnemyKind::Stagehand)
  {
    Flat fl;
    fl.enemy = i;
    fl.x = en.x;
    fl.y = en.y;
    s.flats.push_back(fl);
    s.stagehands.push_back({i, int(s.flats.size()) - 1, 0});
  }
  else if (en.kind == EnemyKind::Mimic)
    s.mimics.push_back({i, int(s.mimics.size()) * 17 % kMimicBreath, 0, en.x});
}

void World::linkWarehouse()
{
  auto& s = mWarehouse;
  if (!s.on)
    return;
  for (auto& c : s.cranes)
    for (std::size_t j = 0; j < s.levers.size(); ++j)
      if (s.levers[j].id == c.leverId)
        c.lever = int(j);
  for (auto& c : s.crates)
    restCrate(c);
  if (s.throwAll)
    for (const auto& [tx, ty] : s.loose)
      if (mMap.block(tx, ty) == Tile::Empty)
        mMap.setBlock(tx, ty, Tile::Solid);
  syncPlatformCollision();
}

// --- Metal crates --------------------------------------------------------------------------

void World::stampCrate(MetalCrate& c)
{
  if (c.stamped)
    return;
  const int tx = c.x / kCellsPerTile, ty = c.y / kCellsPerTile;
  for (int k = 0; k < 4; ++k)
  {
    const int bx = tx + k % 2, by = ty + k / 2;
    c.under[std::size_t(k)] = static_cast<unsigned char>(mMap.block(bx, by));
    mMap.setBlock(bx, by, Tile::Solid);
  }
  c.stamped = true;
}

void World::unstampCrate(MetalCrate& c)
{
  if (!c.stamped)
    return;
  const int tx = c.x / kCellsPerTile, ty = c.y / kCellsPerTile;
  for (int k = 0; k < 4; ++k)
    mMap.setBlock(tx + k % 2, ty + k / 2, static_cast<Tile>(c.under[std::size_t(k)]));
  c.stamped = false;
}

bool World::crateBlocked(const CellBox& b) const
{
  if (mMap.overlapsSolid(b))
    return true;
  const auto& p = mPlayer;
  return alive(p) && b.intersects(p.box());
}

bool World::crateSupported(const MetalCrate& c) const
{
  const int row = c.y + kCrateSize;
  return mMap.solidTop(c.x + 1, row) || mMap.solidTop(c.x + 2, row);
}

// A crate comes to rest where it is: on whole blocks, solid again.
void World::restCrate(MetalCrate& c)
{
  if (c.x % kCellsPerTile != 0)
  {
    const int dir = c.slide >= 0 ? 1 : -1;
    for (int d : {dir, -dir})
      if (!crateBlocked({c.x + d, c.y, kCrateSize, kCrateSize}))
      {
        c.x += d;
        break;
      }
  }
  c.y -= c.y % kCellsPerTile;
  c.state = CrateState::Rest;
  c.slide = 0;
  c.speed = 0;
  c.fall = 0;
  c.hit.clear();
  stampCrate(c);
}

void World::crateHitsEnemies(MetalCrate& c, int damage)
{
  const CellBox b = c.box();
  for (std::size_t i = 0; i < mEnemies.size(); ++i)
  {
    Enemy& e = mEnemies[i];
    if (!e.alive || !e.box().intersects(b) || std::find(c.hit.begin(), c.hit.end(), int(i)) != c.hit.end())
      continue;
    c.hit.push_back(int(i));
    damageEnemy(e, damage);
    playSound(Sfx::Crunch);
  }
}

void World::updateMetalCrates()
{
  auto& s = mWarehouse;
  for (auto& c : s.crates)
  {
    if (c.blink > 0)
      --c.blink;
    switch (c.state)
    {
      case CrateState::Rest:
      case CrateState::Held:
        break;
      case CrateState::Gone:
        if (--c.t <= 0)
        {
          const CellBox home{c.homeX, c.homeY, kCrateSize, kCrateSize};
          if (crateBlocked(home))
          {
            c.t = 15;
            break;
          }
          c.x = c.homeX;
          c.y = c.homeY;
          restCrate(c);
          playSound(Sfx::Clunk);
        }
        break;
      case CrateState::Slide:
      {
        const int dir = sgn(c.slide);
        for (int k = 0; k < c.speed && c.slide != 0; ++k)
        {
          const CellBox next{c.x + dir, c.y, kCrateSize, kCrateSize};
          if (crateBlocked(next))
          {
            c.slide = 0;
            break;
          }
          c.x += dir;
          c.slide -= dir;
          crateHitsEnemies(c, 4);
          if (!crateSupported(c))
          {
            // Off the edge: clear it (whole blocks), then down it goes.
            if (c.x % kCellsPerTile != 0 && !crateBlocked({c.x + dir, c.y, kCrateSize, kCrateSize}))
              c.x += dir;
            c.state = CrateState::Fall;
            c.fall = 0;
            c.hit.clear();
            break;
          }
        }
        if (c.state == CrateState::Slide && c.slide == 0)
        {
          c.slide = dir; // whole blocks: on the way it was going
          restCrate(c);
          playSound(Sfx::Clunk);
        }
        break;
      }
      case CrateState::Fall:
      {
        c.fall = std::min(c.fall + 1, 3);
        bool landed = false;
        for (int k = 0; k < c.fall; ++k)
        {
          const int row = c.y + kCrateSize;
          bool floor = false;
          for (int cx = c.x; cx < c.x + kCrateSize && !floor; ++cx)
            floor = mMap.solidTop(cx, row);
          if (floor)
          {
            landed = true;
            break;
          }
          // Never onto the runner: it waits above them.
          const CellBox next{c.x, c.y + 1, kCrateSize, kCrateSize};
          if (alive(mPlayer) && next.intersects(mPlayer.box()))
          {
            c.blink = kCrateBlink;
            c.fall = 0;
            break;
          }
          c.y += 1;
          crateHitsEnemies(c, 8);
          if (c.y > mMap.height() + 8)
          {
            c.state = CrateState::Gone;
            c.t = kCrateRespawn;
            break;
          }
        }
        if (c.state != CrateState::Fall)
          break;
        if (landed)
        {
          // Stack assist: a crate dropped within a block of one below sits on it.
          for (const auto& o : s.crates)
            if (&o != &c && o.state == CrateState::Rest && o.y == c.y + kCrateSize && std::abs(o.x - c.x) <= kCellsPerTile &&
                o.x != c.x && !mMap.overlapsSolid({o.x, c.y, kCrateSize, kCrateSize}))
            {
              c.x = o.x;
              break;
            }
          bool foam = false;
          for (const auto& f : s.foam)
            foam = foam || f.intersects(c.box());
          playSound(Sfx::Crash);
          burst(cellCenter({c.x, c.y + kCrateSize - 1, kCrateSize, 1}), rgb(200, 190, 170), rgb(120, 110, 100), 8, 1.0f, false);
          if (foam)
          {
            c.state = CrateState::Gone;
            c.t = kCrateRespawn;
          }
          else
            restCrate(c);
        }
        break;
      }
    }
  }
}

// --- Cranes and levers ---------------------------------------------------------------------

void World::throwLever(CraneLever& l)
{
  auto& s = mWarehouse;
  l.on = !l.on;
  l.flash = 10;
  ++l.throws;
  playSound(Sfx::Click);
  const int li = int(&l - s.levers.data());
  for (auto& c : s.cranes)
  {
    if (c.lever != li)
      continue;
    if (c.ride)
    {
      // The first throw wakes a ride crane up; after that it runs on its own.
      continue;
    }
    switch (c.phase)
    {
      case CranePhase::Idle:
        c.phase = CranePhase::Bell;
        c.t = kCraneBell;
        // Toward the rail end farther away.
        c.dir = (c.x - c.x0) > (c.x1 - c.x) ? -1 : 1;
        playSound(Sfx::Bell);
        break;
      case CranePhase::Lower:
      case CranePhase::Lift:
      case CranePhase::Move:
        c.phase = CranePhase::Drop;
        break;
      default:
        break;
    }
  }
}

// The hook's bottom row when the magnet comes down onto what is below it.
int World::craneTarget(const Crane& c, int& load) const
{
  const auto& s = mWarehouse;
  load = -1;
  const CellBox head{c.x, c.rail + 1, kCraneHead, 0};
  for (int y = c.rail + 2; y < mMap.height(); ++y)
  {
    for (std::size_t i = 0; i < s.crates.size(); ++i)
    {
      const auto& k = s.crates[i];
      if (k.state == CrateState::Rest && k.y == y + 1 && k.x < head.x + head.w && head.x < k.x + kCrateSize)
      {
        load = int(i);
        return y;
      }
    }
    for (const auto& f : s.forklifts)
    {
      const Enemy& e = mEnemies[std::size_t(f.enemy)];
      const CellBox b = e.box();
      if (e.alive && b.y == y + 1 && b.x < head.x + head.w && head.x < b.right() + 1)
      {
        load = -2 - f.enemy;
        return y;
      }
    }
    for (int cx = c.x; cx < c.x + kCraneHead; ++cx)
      if (mMap.solid(cx, y + 1))
        return y;
  }
  return mMap.height() - 1;
}

void World::placeCraneLoad(Crane& c)
{
  auto& s = mWarehouse;
  if (c.load >= 0)
  {
    auto& k = s.crates[std::size_t(c.load)];
    k.x = c.x;
    k.y = c.hook + 1;
  }
  else if (c.load <= -2)
  {
    Enemy& e = mEnemies[std::size_t(-2 - c.load)];
    e.x = c.x + kCraneHead / 2 - e.w / 2;
    e.y = c.hook + e.h;
  }
}

void World::releaseCraneLoad(Crane& c)
{
  auto& s = mWarehouse;
  if (c.load >= 0)
  {
    auto& k = s.crates[std::size_t(c.load)];
    // Onto whole blocks: the column below must be clear of the runner.
    const int x = (c.x + kCellsPerTile / 2) / kCellsPerTile * kCellsPerTile;
    int land = k.y;
    while (land < mMap.height() && !mMap.solidTop(x + 1, land + kCrateSize) && !mMap.solidTop(x + 2, land + kCrateSize))
      ++land;
    const CellBox column{x, k.y, kCrateSize, land - k.y + kCrateSize};
    if (alive(mPlayer) && column.intersects(mPlayer.box()))
    {
      k.blink = kCrateBlink;
      return; // waits, blinking, until they move
    }
    k.x = x;
    k.state = CrateState::Fall;
    k.fall = 0;
    k.crane = -1;
    k.hit.clear();
  }
  else if (c.load <= -2)
  {
    for (auto& f : s.forklifts)
      if (f.enemy == -2 - c.load)
      {
        f.held = -1;
        f.fall = 1; // takes 4 when it lands
      }
  }
  if (c.load != -1)
  {
    ++c.cranesMoved;
    playSound(Sfx::Clunk);
  }
  c.load = -1;
  c.phase = CranePhase::Idle;
}

void World::updateCranes()
{
  auto& s = mWarehouse;
  const auto& p = mPlayer;
  for (std::size_t ci = 0; ci < s.cranes.size(); ++ci)
  {
    auto& c = s.cranes[ci];
    ++c.t;
    if (c.ride)
    {
      updateRideCrane(c);
      continue;
    }
    switch (c.phase)
    {
      case CranePhase::Idle:
        if (c.hook > c.rail + 2)
          --c.hook;
        break;
      case CranePhase::Bell:
        c.t -= 2; // counts down
        if (c.t <= 0)
        {
          c.phase = CranePhase::Lower;
          c.t = 0;
        }
        break;
      case CranePhase::Lower:
      {
        int load = -1;
        const int target = craneTarget(c, load);
        const int step = std::max(1, (target - (c.rail + 2) + 7) / 8);
        c.hook = std::min(target, c.hook + step);
        if (c.hook >= target)
        {
          c.load = load;
          if (load >= 0)
          {
            auto& k = s.crates[std::size_t(load)];
            unstampCrate(k);
            k.state = CrateState::Held;
            k.crane = int(ci);
            if (p.state == PlayerState::OnGround && !mMap.onSolidGround(p.box()))
              startFallingDelayed(); // the runner was standing on it
          }
          else if (load <= -2)
            for (auto& f : s.forklifts)
              if (f.enemy == -2 - load)
              {
                f.held = int(ci);
                f.charge = 0;
                f.carrying = false;
              }
          playSound(load != -1 ? Sfx::Clunk : Sfx::Tink);
          c.phase = CranePhase::Lift;
        }
        break;
      }
      case CranePhase::Lift:
      {
        const int hang = c.rail + kCraneHang;
        if (c.hook > hang)
          --c.hook;
        placeCraneLoad(c);
        if (c.hook <= hang)
          c.phase = CranePhase::Move;
        break;
      }
      case CranePhase::Move:
        if (c.t % 2 == 0)
          c.x = std::clamp(c.x + c.dir, c.x0, c.x1);
        placeCraneLoad(c);
        if ((c.dir > 0 && c.x >= c.x1) || (c.dir < 0 && c.x <= c.x0))
          c.phase = CranePhase::Drop;
        break;
      case CranePhase::Drop:
        placeCraneLoad(c);
        releaseCraneLoad(c);
        break;
      default:
        break;
    }
  }
}

// A crane over the crane yard: its crate rides flush with the floor once
// its lever has been thrown. Step on, a bell, out to the far end, a hold,
// and back.
void World::updateRideCrane(Crane& c)
{
  auto& s = mWarehouse;
  if (c.platform < 0)
    return;
  Platform& pl = mPlatforms[std::size_t(c.platform)];
  const bool awake = c.lever < 0 || s.levers[std::size_t(c.lever)].throws > 0;
  const auto& p = mPlayer;
  const bool rider = (p.state == PlayerState::OnGround || p.state == PlayerState::Recovering) && standsOn(p.box(), pl);
  auto step = [&](int to) {
    const int d = sgn(to - pl.x);
    if (d != 0 && c.t % 2 == 0)
      movePlatform(pl, d, 0);
    c.x = pl.x;
    return pl.x == to;
  };
  switch (c.phase)
  {
    case CranePhase::Idle:
      if (awake && rider && pl.x == c.startX)
      {
        c.phase = CranePhase::RideBell;
        c.t = 0;
        playSound(Sfx::Bell);
      }
      break;
    case CranePhase::RideBell:
      if (c.t >= kCraneBell)
      {
        c.phase = CranePhase::Ride;
        c.t = 0;
      }
      break;
    case CranePhase::Ride:
      if (step(c.x1))
      {
        c.phase = CranePhase::RideHold;
        c.t = 0;
        ++c.cranesMoved;
        playSound(Sfx::Clunk);
      }
      break;
    case CranePhase::RideHold:
      if (c.t >= 120)
      {
        c.phase = CranePhase::RideBack;
        c.t = 0;
        playSound(Sfx::Bell);
      }
      break;
    case CranePhase::RideBack:
      if (step(c.startX))
      {
        c.phase = CranePhase::Idle;
        c.t = 0;
      }
      break;
    default:
      c.phase = CranePhase::Idle;
      break;
  }
  pl.parked = c.phase == CranePhase::Idle || c.phase == CranePhase::RideHold;
  c.hook = pl.y - 1;
  syncPlatformCollision();
}

// --- The Magnet Gun ------------------------------------------------------------------------

void World::magnetPull()
{
  auto& s = mWarehouse;
  auto& p = mPlayer;
  const int row = p.stance == Stance::Crouched ? p.y - 1 : p.y - 2;
  const int mx = p.facing > 0 ? p.x + Player::kWidth : p.x - 1;
  s.pullCrate = s.pullEnemy = -1;
  int d = 0;
  for (; d < kPullReach; ++d)
  {
    const int cx = mx + d * p.facing;
    if (mMap.solid(cx, row))
    {
      // A resting crate is solid too: is it one?
      for (std::size_t i = 0; i < s.crates.size(); ++i)
        if (s.crates[i].state == CrateState::Rest && s.crates[i].box().contains(cx, row))
          s.pullCrate = int(i);
      break;
    }
    bool found = false;
    for (std::size_t i = 0; i < s.crates.size() && !found; ++i)
      if ((s.crates[i].state == CrateState::Slide) && s.crates[i].box().contains(cx, row))
      {
        s.pullCrate = int(i);
        found = true;
      }
    for (const auto& f : s.forklifts)
    {
      const Enemy& e = mEnemies[std::size_t(f.enemy)];
      if (!found && e.alive && f.held < 0 && e.box().contains(cx, row))
      {
        s.pullEnemy = f.enemy;
        found = true;
      }
    }
    if (found)
      break;
  }
  s.beam = d + 1;
  if (s.pullCrate >= 0)
  {
    auto& c = s.crates[std::size_t(s.pullCrate)];
    const int gap = p.facing > 0 ? c.x - (p.x + Player::kWidth) : p.x - (c.x + kCrateSize);
    if (gap > kCellsPerTile)
    {
      unstampCrate(c);
      c.state = CrateState::Slide;
      c.speed = 1;
      c.slide = -p.facing; // one cell toward the runner this frame
    }
  }
  else if (s.pullEnemy >= 0)
  {
    Enemy& e = mEnemies[std::size_t(s.pullEnemy)];
    const CellBox b = e.box();
    const int gap = p.facing > 0 ? b.x - (p.x + Player::kWidth) : p.x - (b.right() + 1);
    if (gap > kCellsPerTile)
    {
      int x = e.x;
      mMap.moveHorizontally(x, e.y, e.w, e.h, -p.facing);
      e.x = x;
      e.tell = 0;
      for (auto& f : s.forklifts)
        if (f.enemy == s.pullEnemy)
          f.charge = 0;
    }
  }
}

void World::updateMagnetGun(const Button& fire)
{
  auto& s = mWarehouse;
  auto& p = mPlayer;
  const ProtoDef& def = protoDef(p.proto);
  s.beam = 0;
  if (!canFire())
  {
    s.fireHeld = 0;
    s.pulling = false;
    return;
  }
  if (fire.pressed && p.ammo > 0)
  {
    ++s.fireHeld;
    if (s.fireHeld >= kPullAfter)
    {
      if (!s.pulling)
        playSound(Sfx::Whine);
      s.pulling = true;
      magnetPull();
      if (++s.ammoTick % kPullAmmoEvery == 0 && p.turbo == 0)
        --p.ammo;
    }
  }
  else
  {
    if (s.fireHeld > 0 && s.fireHeld < kPullAfter && (p.shotCooldown == 0 || p.turbo > 0) && p.ammo > 0)
    {
      // A tap: a push pulse.
      Projectile pr;
      pr.kind = ShotKind::Proto;
      pr.proto = int(ProtoId::MagnetGun);
      pr.speed = 3;
      pr.w = 2;
      pr.h = 1;
      pr.damage = 2;
      pr.range = kPushReach;
      pr.dx = p.facing;
      pr.x = pr.prevX = p.facing > 0 ? p.x + Player::kWidth : p.x - 2;
      pr.y = pr.prevY = p.stance == Stance::Crouched ? p.y - 1 : p.y - 2;
      mProjectiles.push_back(pr);
      playSound(Sfx::Thrust);
      s.pulse = 6;
      p.shotCooldown = def.cooldown;
      if (p.turbo == 0)
        --p.ammo;
    }
    s.fireHeld = 0;
    s.pulling = false;
  }
  if (s.pulse > 0)
    --s.pulse;
  if (p.ammo <= 0)
  {
    s.pulling = false;
    p.ammo = 0;
    p.weapon = Weapon::Normal;
    p.proto = -1;
    showMessage("MAGNET GUN EMPTY - BACK TO THE BLASTER");
  }
}

// The push pulse shoves a metal crate or a Forklift Bot along; flats stop
// every shot; the TURBODUDES crate takes two.
bool World::shotAtWarehouse(Projectile& pr, const CellBox& hit)
{
  auto& s = mWarehouse;
  for (auto& f : s.flats)
    if (f.alive && f.down && f.box().intersects(hit))
    {
      pr.alive = false;
      if (pr.kind != ShotKind::Enemy)
      {
        f.hp -= std::max(1, pr.damage);
        playSound(Sfx::Thud);
        if (f.hp <= 0)
        {
          f.alive = false;
          burst(cellCenter(f.box()), rgb(40, 40, 46), rgb(200, 200, 200), 12, 1.4f, false);
          playSound(Sfx::BoxBreak);
        }
      }
      return true;
    }
  if (pr.kind == ShotKind::Enemy)
    return false;
  const bool push = pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::MagnetGun);
  if (push)
  {
    for (auto& c : s.crates)
      if (c.state == CrateState::Rest && c.box().intersects(hit))
      {
        unstampCrate(c);
        c.state = CrateState::Slide;
        c.speed = 2;
        c.slide = pr.dx * kPushSlide;
        c.hit.clear();
        pr.alive = false;
        playSound(Sfx::Clunk);
        return true;
      }
    for (auto& f : s.forklifts)
    {
      Enemy& e = mEnemies[std::size_t(f.enemy)];
      if (e.alive && f.held < 0 && e.box().intersects(hit))
      {
        for (int k = 0; k < 6; ++k)
        {
          int x = e.x;
          mMap.moveHorizontally(x, e.y, e.w, e.h, pr.dx * 2);
          e.x = x;
        }
        f.charge = 0;
        f.carrying = false;
        e.tell = 0;
        if (s.grabbedBy == f.enemy)
          dropFromForks(f);
        damageEnemy(e, pr.damage);
        pr.alive = false;
        return true;
      }
    }
  }
  if (s.tdX >= 0 && s.tdHits < 2 && boxAt(s.tdX, s.tdY, 4, 4).intersects(hit))
  {
    pr.alive = false;
    if (++s.tdHits == 2)
    {
      s.tdShow = kOldTitle;
      showMessage("TURBODUDES - THE ORIGINAL TITLE SCREEN");
      playSound(Sfx::LettersComplete);
    }
    else
      playSound(Sfx::Thud);
    return true;
  }
  return false;
}

// Turbo: walking into a metal crate shoves it a cell a frame.
void World::turboShove(const PlayerInput& input)
{
  auto& s = mWarehouse;
  auto& p = mPlayer;
  if (p.turbo <= 0 || p.state != PlayerState::OnGround || input.left == input.right)
    return;
  const int dir = input.right ? 1 : -1;
  const CellBox ahead = dir > 0 ? CellBox{p.x + Player::kWidth, p.y - p.height() + 1, 1, p.height()}
                                : CellBox{p.x - 1, p.y - p.height() + 1, 1, p.height()};
  for (auto& c : s.crates)
    if (c.state == CrateState::Rest && c.box().intersects(ahead))
    {
      unstampCrate(c);
      if (mMap.overlapsSolid({c.x + dir * kCellsPerTile, c.y, kCrateSize, kCrateSize}))
      {
        stampCrate(c);
        return;
      }
      c.state = CrateState::Slide;
      c.speed = 1;
      c.slide = dir * kCellsPerTile;
      return;
    }
}

// --- Enemies -------------------------------------------------------------------------------

void World::dropFromForks(Forklift& f)
{
  auto& s = mWarehouse;
  auto& p = mPlayer;
  Enemy& e = mEnemies[std::size_t(f.enemy)];
  f.carrying = false;
  f.charge = 0;
  e.cool = enemyDef(e.def).cooldown;
  if (s.grabbedBy != f.enemy)
    return;
  s.grabbedBy = -1;
  // Set down on the floor in front of the forks.
  p.y = e.y;
  p.prevY = p.y;
  int y = p.y;
  while (mMap.overlapsSolid(boxAt(p.x, y, Player::kWidth, p.height())) && y > e.y - e.h - 4)
    --y;
  p.y = p.prevY = y;
  p.state = PlayerState::OnGround;
  if (!mMap.onSolidGround(p.box()))
    startFallingDelayed();
  p.mercy = std::max(p.mercy, 30);
  playSound(Sfx::Thud);
}

void World::updateForklift(Enemy& e, const EnemyDef& def)
{
  auto& s = mWarehouse;
  Forklift* f = nullptr;
  for (auto& k : s.forklifts)
    if (k.enemy == int(&e - mEnemies.data()))
      f = &k;
  if (!f || f->held >= 0)
    return;
  auto& p = mPlayer;
  const bool live = alive(p);
  if (!mMap.onSolidGround(e.box()))
  {
    f->fall = std::max(f->fall, 1);
    int y = e.y;
    mMap.moveVertically(e.x, y, e.w, e.h, 2);
    e.y = y;
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return;
  }
  if (f->fall > 0)
  {
    f->fall = 0;
    // Dropped by a crane: takes 4, and does 8 to anything under it.
    for (auto& o : mEnemies)
      if (&o != &e && o.alive && o.box().intersects(e.box()))
        damageEnemy(o, 8);
    damageEnemy(e, 4);
    playSound(Sfx::Crash);
    if (!e.alive)
      return;
  }
  if (e.cool > 0)
    --e.cool;
  const CellBox b = e.box(), pb = p.hitBox();
  auto canGo = [&](int dir) {
    const int ahead = dir > 0 ? b.right() + 1 : b.x - 1;
    const bool wall = dir > 0 ? mMap.touchingRightWall(b) : mMap.touchingLeftWall(b);
    return !wall && mMap.solidTop(ahead, b.bottom() + 1);
  };
  // The runner on the forks: off to the nearest ledge edge ahead.
  if (f->carrying)
  {
    if (!live || p.state == PlayerState::Jumping)
    {
      if (s.grabbedBy == f->enemy)
        s.grabbedBy = -1;
      f->carrying = false;
      e.cool = def.cooldown;
      return;
    }
    for (int k = 0; k < 2; ++k)
    {
      if (!canGo(e.dir) || f->carried >= kForkCarry)
      {
        dropFromForks(*f);
        return;
      }
      e.x += e.dir;
      ++f->carried;
    }
    p.x = p.prevX = e.dir > 0 ? e.x + e.w - 1 : e.x - Player::kWidth + 1;
    p.y = p.prevY = e.y - 2;
    p.state = PlayerState::OnGround;
    p.facing = -e.dir;
    return;
  }
  const bool sameFloor = std::abs(pb.bottom() - b.bottom()) <= 2;
  const int dx = (pb.x + 1) - (b.x + b.w / 2);
  if (e.tell > 0)
  {
    if (--e.tell == 0)
    {
      f->charge = kForkSight + 8;
      playSound(Sfx::Rev);
    }
    return;
  }
  if (f->charge > 0)
  {
    for (int k = 0; k < 2 && f->charge > 0; ++k, --f->charge)
    {
      if (!canGo(e.dir))
      {
        f->charge = 0;
        break;
      }
      e.x += e.dir;
      const CellBox nb = e.box();
      const CellBox forks = e.dir > 0 ? CellBox{nb.right() + 1, nb.bottom(), 3, 1} : CellBox{nb.x - 3, nb.bottom(), 3, 1};
      const CellBox feet{pb.x, pb.bottom(), pb.w, 1};
      if (live && p.mercy == 0 && s.grabbedBy < 0 && (forks.intersects(feet) || nb.intersects(pb)))
      {
        if (mCharacterIndex == 1)
        {
          // Rocco is too heavy to lift: shoved back instead.
          int x = p.x;
          mMap.moveHorizontally(x, p.y, Player::kWidth, p.height(), e.dir * kRoccoShove);
          p.x = x;
          hurtPlayer(1);
          f->charge = 0;
          e.cool = def.cooldown;
        }
        else
        {
          f->carrying = true;
          f->carried = 0;
          f->charge = 0;
          s.grabbedBy = f->enemy;
          playSound(Sfx::Clunk);
        }
        return;
      }
    }
    if (f->charge == 0)
      e.cool = def.cooldown;
    return;
  }
  // Charges a runner on its floor in front of it.
  if (live && sameFloor && e.cool == 0 && std::abs(dx) <= kForkSight && sgn(dx) == e.dir && s.grabbedBy < 0)
  {
    e.tell = kForkTell;
    playSound(Sfx::Rev);
    return;
  }
  if (live && sameFloor && std::abs(dx) <= kForkSight && e.cool == 0)
  {
    e.dir = dx < 0 ? -1 : 1; // turns to face them
    return;
  }
  // Patrol, half a cell a frame.
  if (e.timer % std::max(1, def.stepEvery) != 0)
    return;
  if (!canGo(e.dir) || (e.dir > 0 && e.x >= f->x1) || (e.dir < 0 && e.x <= f->x0))
    e.dir = -e.dir;
  else
    e.x += e.dir;
}

void World::updateStagehand(Enemy& e, const EnemyDef& def)
{
  auto& s = mWarehouse;
  Stagehand* h = nullptr;
  for (auto& k : s.stagehands)
    if (k.enemy == int(&e - mEnemies.data()))
      h = &k;
  if (!h)
    return;
  const auto& p = mPlayer;
  const bool live = alive(p);
  Flat* fl = h->flat >= 0 ? &s.flats[std::size_t(h->flat)] : nullptr;
  if (fl && !fl->alive)
    fl = nullptr;
  if (!mMap.onSolidGround(e.box()))
  {
    int y = e.y;
    mMap.moveVertically(e.x, y, e.w, e.h, 2);
    e.y = y;
    if (fl && !fl->down)
      fl->y = e.y;
    return;
  }
  if (e.cool > 0)
    --e.cool;
  if (h->lean > 0)
    --h->lean;
  const CellBox b = e.box(), pb = p.hitBox();
  const int dx = (pb.x + 1) - (b.x + 1);
  e.dir = dx < 0 ? -1 : 1;
  if (fl && !fl->down)
  {
    // Carrying the flat to its spot, six blocks in front of the runner.
    const int spot = pb.x + 1 - e.dir * kStageGap;
    fl->x = e.dir > 0 ? e.x + e.w : e.x - kFlatW;
    fl->y = e.y;
    if (std::abs(spot - (b.x + 1)) <= 2 || std::abs(dx) <= kStageGap)
    {
      fl->down = true;
      playSound(Sfx::Thud);
      return;
    }
    if (e.timer % std::max(1, def.stepEvery) == 0)
    {
      const int dir = sgn(spot - (b.x + 1));
      const CellBox nb = e.box();
      const int ahead = dir > 0 ? nb.right() + 1 + kFlatW : nb.x - 1 - kFlatW;
      if (mMap.solidTop(ahead, nb.bottom() + 1) && !(dir > 0 ? mMap.touchingRightWall(nb) : mMap.touchingLeftWall(nb)))
        e.x += dir;
      else
      {
        fl->down = true; // the edge of its floor: here will do
        playSound(Sfx::Thud);
      }
    }
    return;
  }
  // Behind the flat: leans out and shoots.
  if (e.tell > 0)
  {
    if (--e.tell == 0)
    {
      Projectile pr;
      pr.kind = ShotKind::Enemy;
      pr.speed = 1;
      pr.damage = 1;
      pr.w = 2;
      pr.h = 1;
      pr.dx = e.dir;
      // From the far side of its own flat.
      int x0 = e.dir > 0 ? b.right() + 1 : b.x - 2;
      if (fl && fl->down)
        x0 = e.dir > 0 ? std::max(x0, fl->x + kFlatW) : std::min(x0, fl->x - 2);
      pr.x = pr.prevX = x0;
      pr.y = pr.prevY = b.y + 1;
      pr.range = 60;
      mProjectiles.push_back(pr);
      playSound(Sfx::EnemyShot);
      e.cool = def.cooldown;
      h->lean = 8;
    }
    return;
  }
  if (live && e.cool == 0 && std::abs(dx) <= 36 && std::abs(pb.bottom() - b.bottom()) <= 6)
    e.tell = def.tell;
}

void World::updateMimic(Enemy& e, const EnemyDef& def)
{
  auto& s = mWarehouse;
  Mimic* m = nullptr;
  for (auto& k : s.mimics)
    if (k.enemy == int(&e - mEnemies.data()))
      m = &k;
  if (!m)
    return;
  m->breath = (m->breath + 1) % kMimicBreath;
  const auto& p = mPlayer;
  if (e.cool > 0)
    --e.cool;
  const CellBox b = e.box(), pb = p.hitBox();
  if (m->lunge != 0)
  {
    const int d = sgn(m->lunge);
    for (int k = 0; k < 2 && m->lunge != 0; ++k)
    {
      int x = e.x;
      mMap.moveHorizontally(x, e.y, e.w, e.h, d);
      e.x = x;
      m->lunge -= d;
    }
    if (m->lunge == 0)
      e.cool = def.cooldown;
    return;
  }
  if (e.tell > 0)
  {
    if (--e.tell == 0)
    {
      m->lunge = e.dir * kMimicReach;
      playSound(Sfx::Snap);
    }
    return;
  }
  const int gap = pb.x > b.right() ? pb.x - b.right() - 1 : (pb.right() < b.x ? b.x - pb.right() - 1 : 0);
  if (alive(p) && e.cool == 0 && gap <= kMimicReach && std::abs(pb.bottom() - b.bottom()) <= 4)
  {
    e.dir = pb.x + 1 < b.x + b.w / 2 ? -1 : 1;
    e.tell = def.tell;
    return;
  }
  // Back home, slowly.
  if (e.cool == 0 && e.x != m->homeX && e.timer % 2 == 0)
  {
    int x = e.x;
    mMap.moveHorizontally(x, e.y, e.w, e.h, sgn(m->homeX - e.x));
    e.x = x;
  }
}

// --- Each frame ----------------------------------------------------------------------------

void World::updateWarehouse(const PlayerInput& input)
{
  auto& s = mWarehouse;
  auto& p = mPlayer;
  const bool upNow = input.up && !s.upHeld;
  s.upHeld = input.up;
  if (s.throwAll)
  {
    updateThrowAll();
    return;
  }
  for (auto& l : s.levers)
    if (l.flash > 0)
      --l.flash;
  if (alive(p) && upNow && p.state == PlayerState::OnGround)
    for (auto& l : s.levers)
      if (std::abs(p.x + 1 - (l.x + 1)) <= 3 && std::abs(p.y - l.y) <= 2)
      {
        throwLever(l);
        break;
      }
  updateCranes();
  updateMetalCrates();
  turboShove(input);
  // The props of the earlier levels: a label as the runner walks past.
  for (auto& d : s.props)
  {
    if (d.label > 0)
      --d.label;
    const bool near = alive(p) && std::abs(p.x + 1 - (d.x + 2)) <= 2 && p.y >= d.y - 4;
    if (near && !d.seen)
      d.label = kPropLabel;
    d.seen = near;
  }
  if (s.tdShow > 0)
    --s.tdShow;
  // The bot's way through: the furthest step reached.
  if (alive(p) && p.state == PlayerState::OnGround)
    for (int i = std::min(int(s.route.size()) - 1, s.routeAt + 2); i >= s.routeAt; --i)
    {
      const auto& st = s.route[std::size_t(i)];
      if (std::abs(st.x - p.x) <= 3 && std::abs(p.y - st.y) <= 2 && warehouseStepDone(i))
      {
        s.routeAt = i + 1;
        break;
      }
    }
}

// The bot's step i is done once its job is (a lever thrown, a crate where
// it belongs, a crane back).
bool World::warehouseStepDone(int i) const
{
  const auto& s = mWarehouse;
  const auto& st = s.route[std::size_t(i)];
  auto lever = [&](const std::string& id) -> const CraneLever* {
    for (const auto& l : s.levers)
      if (l.id == id)
        return &l;
    return nullptr;
  };
  auto crane = [&](const std::string& id) -> const Crane* {
    for (const auto& c : s.cranes)
      if (c.id == id)
        return &c;
    return nullptr;
  };
  if (st.act == "lever")
  {
    const CraneLever* l = lever(st.arg);
    return !l || l->throws > 0;
  }
  if (st.act == "wait")
  {
    const Crane* c = crane(st.arg);
    return !c || (c->phase == CranePhase::Idle && c->cranesMoved > 0);
  }
  if (st.act == "push")
  {
    for (const auto& c : s.crates)
      if (c.id == st.arg)
        return c.state == CrateState::Rest && c.x >= st.n * kCellsPerTile;
    return true;
  }
  if (st.act == "stack")
  {
    // Throw Everything: n solid blocks on the column, up from the step's floor.
    const int col = std::atoi(st.arg.c_str());
    const int floor = st.y / kCellsPerTile;
    for (int k = 0; k < std::max(1, st.n); ++k)
      if (mMap.block(col, floor - k) != Tile::Solid)
        return false;
    return true;
  }
  return true;
}

void World::resetWarehouse()
{
  auto& s = mWarehouse;
  if (!s.on)
    return;
  s.grabbedBy = -1;
  s.fireHeld = 0;
  s.pulling = false;
  for (auto& f : s.forklifts)
  {
    f.carrying = false;
    f.charge = 0;
  }
  if (s.carry >= 0)
    s.carry = -1;
  // The route goes back to the furthest step at or before where the runner is.
  const auto& p = mPlayer;
  while (s.routeAt > 0 && s.route[std::size_t(s.routeAt - 1)].x > p.x + 8)
    --s.routeAt;
}

// --- Throw Everything ----------------------------------------------------------------------

// Lift what is in front of the runner (fire), throw it (fire again): it lands
// six blocks ahead, on top of whatever is there, as a solid block.
void World::throwAllFire(const Button& fire)
{
  auto& s = mWarehouse;
  auto& p = mPlayer;
  if (!fire.triggered || !alive(p) || p.state == PlayerState::Ladder || s.liftT > 0)
    return;
  if (s.carry < 0)
  {
    // A loose block beside the runner (feet row or the one above), or a dummy.
    const int fx = p.facing > 0 ? p.x + Player::kWidth : p.x - 1;
    const int tx = fx / kCellsPerTile;
    for (int ty : {p.y / kCellsPerTile, (p.y - 2) / kCellsPerTile, (p.y - 4) / kCellsPerTile})
      for (std::size_t i = 0; i < s.loose.size(); ++i)
        if (s.loose[i].first == tx && s.loose[i].second == ty && mMap.block(tx, ty) == Tile::Solid)
        {
          mMap.setBlock(tx, ty, Tile::Empty);
          s.loose.erase(s.loose.begin() + std::ptrdiff_t(i));
          s.carry = 0;
          s.liftT = kLiftFrames;
          playSound(Sfx::Clunk);
          return;
        }
    const CellBox reach = p.facing > 0 ? CellBox{p.x + Player::kWidth, p.y - 4, 3, 5} : CellBox{p.x - 3, p.y - 4, 3, 5};
    for (auto& e : mEnemies)
      if (e.alive && e.box().intersects(reach))
      {
        e.alive = false;
        s.carry = 1;
        s.liftT = kLiftFrames;
        playSound(Sfx::Clunk);
        return;
      }
    return;
  }
  // Throw: the column six blocks ahead (or the last open one before a wall).
  const int col0 = (p.x + 1) / kCellsPerTile;
  int col = col0;
  const int top = (p.y - p.height() - 2) / kCellsPerTile;
  for (int k = 1; k <= kThrowCells / kCellsPerTile; ++k)
  {
    const int c = col0 + p.facing * k;
    if (c < 0 || c >= mMap.width() / kCellsPerTile)
      break;
    bool open = true;
    for (int ty = top; ty <= top + 1; ++ty)
      open = open && mMap.block(c, ty) != Tile::Solid;
    if (!open)
      break;
    col = c;
  }
  auto landing = [&](int c) {
    int row = top;
    while (row + 1 < mMap.height() / kCellsPerTile && mMap.block(c, row + 1) != Tile::Solid &&
           mMap.block(c, row + 1) != Tile::Platform)
      ++row;
    return row;
  };
  int row = landing(col);
  // Stack assist: a throw a block off a taller stack lands on it.
  for (int d : {-1, 1})
  {
    const int c = col + d;
    const int r = landing(c);
    bool stack = false;
    for (const auto& [lx, ly] : s.loose)
      stack = stack || (lx == c && ly == r + 1);
    if (stack && r < row && r >= row - 3)
    {
      col = c;
      row = r;
      break;
    }
  }
  Thrown t;
  t.kind = s.carry;
  t.fromX = p.x;
  t.fromY = p.y - p.height() - 2;
  t.toX = col * kCellsPerTile;
  t.toY = row * kCellsPerTile;
  t.x = float(t.fromX);
  t.y = float(t.fromY);
  t.flying = true;
  s.thrown.push_back(t);
  s.carry = -1;
  playSound(Sfx::Whoosh);
}

void World::updateThrowAll()
{
  auto& s = mWarehouse;
  if (s.liftT > 0)
    --s.liftT;
  for (auto& t : s.thrown)
  {
    if (!t.flying)
      continue;
    ++t.t;
    const float a = float(t.t) / float(kThrowFrames);
    t.x = float(t.fromX) + float(t.toX - t.fromX) * a;
    t.y = float(t.fromY) + float(t.toY - t.fromY) * a - 10.0f * a * (1.0f - a) * 4.0f;
    if (t.t < kThrowFrames)
      continue;
    t.flying = false;
    // Lands as a solid block, on top of anything there (the runner too: it
    // waits a block higher).
    const int tx = t.toX / kCellsPerTile;
    int ty = t.toY / kCellsPerTile;
    while (ty > 0 && (mMap.block(tx, ty) == Tile::Solid ||
                      CellBox{tx * kCellsPerTile, ty * kCellsPerTile, 2, 2}.intersects(mPlayer.box())))
      --ty;
    mMap.setBlock(tx, ty, Tile::Solid);
    s.loose.emplace_back(tx, ty);
    playSound(Sfx::Crash);
    burst(cellCenter({tx * kCellsPerTile, ty * kCellsPerTile, 2, 2}), rgb(200, 170, 120), rgb(120, 90, 60), 6, 0.8f, false);
  }
  s.thrown.erase(std::remove_if(s.thrown.begin(), s.thrown.end(), [](const Thrown& t) { return !t.flying; }), s.thrown.end());
}

// --- Saves ---------------------------------------------------------------------------------

bool World::warehouseCanSave() const
{
  const auto& s = mWarehouse;
  if (s.grabbedBy >= 0 || s.carry >= 0 || !s.thrown.empty())
    return false;
  for (const auto& c : s.crates)
    if (c.state != CrateState::Rest && c.state != CrateState::Gone)
      return false;
  for (const auto& c : s.cranes)
    if (c.phase != CranePhase::Idle)
      return false;
  return true;
}

// [crates..., x y each] [levers... throws each] [route at]
std::vector<int> World::warehouseSave() const
{
  const auto& s = mWarehouse;
  std::vector<int> v;
  v.push_back(int(s.crates.size()));
  for (const auto& c : s.crates)
  {
    v.push_back(c.state == CrateState::Gone ? c.homeX : c.x);
    v.push_back(c.state == CrateState::Gone ? c.homeY : c.y);
  }
  v.push_back(int(s.cranes.size()));
  for (const auto& c : s.cranes)
  {
    v.push_back(c.x);
    v.push_back(c.cranesMoved);
  }
  v.push_back(int(s.levers.size()));
  for (const auto& l : s.levers)
    v.push_back(l.throws);
  v.push_back(s.routeAt);
  v.push_back(s.tdHits);
  return v;
}

bool World::validWarehouseSave(const std::vector<int>& v) const
{
  const auto& s = mWarehouse;
  const std::size_t need = 1 + s.crates.size() * 2 + 1 + s.cranes.size() * 2 + 1 + s.levers.size() + 2;
  return v.size() == need && v[0] == int(s.crates.size()) && v[1 + s.crates.size() * 2] == int(s.cranes.size());
}

void World::loadWarehouse(const std::vector<int>& v)
{
  auto& s = mWarehouse;
  if (!validWarehouseSave(v))
    return;
  std::size_t at = 1;
  for (auto& c : s.crates)
    unstampCrate(c);
  for (auto& c : s.crates)
  {
    c.x = v[at++];
    c.y = v[at++];
    c.crane = -1;
    restCrate(c);
  }
  ++at;
  for (auto& c : s.cranes)
  {
    c.x = v[at++];
    c.cranesMoved = v[at++];
    c.phase = CranePhase::Idle;
    c.load = -1;
    if (c.platform >= 0)
    {
      auto& pl = mPlatforms[std::size_t(c.platform)];
      pl.x = pl.prevX = c.x;
      c.hook = pl.y - 1;
    }
    else
      c.hook = c.rail + 2;
  }
  ++at;
  for (auto& l : s.levers)
  {
    l.throws = v[at++];
    l.on = l.throws % 2 != 0;
  }
  s.routeAt = v[at++];
  s.tdHits = v[at++];
  syncPlatformCollision();
}

} // namespace gr
