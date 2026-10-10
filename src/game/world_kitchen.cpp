// Level 24, Too Many Cooks: conveyor belts that carry the runner, enemies,
// items standing on them (and the order rail, a runner hanging on
// it); the chef's bells that reverse a station's belts; pantry flaps that
// open only for a runner a belt pushes in; sauce pans that grow meatballs
// and the sink that eats them; the meatball hatch; Cleaver Bots, whose
// cleavers stick in walls as handles; Egg Timers; the Pepper Grinder's
// sneeze; the toaster and the chef's hand. Drawing is in
// world_kitchen_draw.cpp.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>

namespace gr
{

namespace
{

constexpr int kMeatSizes[3] = {3, 5, 7};
constexpr int kMeatHp[3] = {2, 5, 8};

// Frames per cell from a speed like "1/4" (cells a frame).
int beltPeriod(const EntityDef& e)
{
  const float speed = e.real("speed", 0.5f);
  return std::max(1, int(std::lround(1.0f / std::max(0.1f, speed))));
}

bool standingState(const Player& p)
{
  return p.state == PlayerState::OnGround || p.state == PlayerState::Recovering;
}

bool overlapsBelt(const Belt& b, int x, int w)
{
  return x + w - 1 >= b.x0 * kCellsPerTile && x <= b.x1 * kCellsPerTile + kCellsPerTile - 1;
}

} // namespace

// --- Setup ---------------------------------------------------------------------------------

bool World::setupKitchenEntity(const EntityDef& e)
{
  auto& k = mKitchen;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  if (e.kind == "belt" && e.rect("rect", x0, y0, x1, y1))
  {
    Belt b;
    b.id = e.id;
    b.station = e.str("station");
    b.x0 = std::min(x0, x1);
    b.x1 = std::max(x0, x1);
    b.y = y0;
    b.period = beltPeriod(e);
    b.dir = e.str("dir", "r") == "l" ? -1 : 1;
    b.loop = e.num("loop", 0) != 0;
    b.hang = e.num("hang", 0) != 0;
    k.belts.push_back(b);
    k.on = true;
    return true;
  }
  if (e.kind == "switch" && e.has("station"))
  {
    ChefBell b;
    b.id = e.id;
    b.station = e.str("station");
    b.x = e.hasPos ? e.x : e.num("x");
    b.y = e.hasPos ? e.y : e.num("y");
    const std::string bot = e.str("bot");
    b.bot = bot == "bonus" ? 2 : (bot == "1" ? 1 : 0);
    b.standX = e.num("stand", -1);
    k.bells.push_back(b);
    k.on = true;
    return true;
  }
  if (e.kind == "flap" && e.hasPos)
  {
    k.flaps.push_back({e.x, e.y, std::max(1, e.num("h", 3)), 0});
    k.on = true;
    return true;
  }
  if ((e.kind == "sauce" || e.kind == "sink") && e.rect("rect", x0, y0, x1, y1))
  {
    (e.kind == "sauce" ? k.sauces : k.sinks).push_back({x0, y0, x1, y1});
    k.on = true;
    return true;
  }
  if (e.kind == "spawner" && e.str("kind") == "meatball")
  {
    MeatballHatch h;
    h.x = (e.hasPos ? e.x : e.num("x")) * kCellsPerTile;
    h.y = (e.hasPos ? e.y : e.num("y")) * kCellsPerTile + 1;
    h.every = std::max(1, e.num("every", 90));
    h.max = std::max(1, e.num("max", 3));
    h.carrier = e.num("carrier", 4);
    k.hatches.push_back(h);
    k.on = true;
    return true;
  }
  if (e.kind == "script" && e.id == "special")
  {
    k.handX = e.hasPos ? e.x : e.num("x");
    k.handY = e.hasPos ? e.y : e.num("y");
    k.on = true;
    return true;
  }
  if (e.kind == "grille" && e.rect("rect", x0, y0, x1, y1))
  {
    mTurn.grilles.push_back({std::min(x0, x1), std::min(y0, y1), std::max(x0, x1), std::max(y0, y1)});
    return true;
  }
  if (e.kind == "toaster" && e.rect("rect", x0, y0, x1, y1))
  {
    k.toaster = {x0, y0, x1, y1};
    const auto lever = e.list("lever");
    if (lever.size() == 2)
    {
      k.leverX = lever[0];
      k.leverY = lever[1];
    }
    k.on = true;
    return true;
  }
  return false;
}

void World::setMeatballSize(Enemy& e, int size)
{
  size = std::clamp(size, 0, 2);
  const int old = std::clamp(e.variant, 0, 2);
  const int grow = kMeatSizes[size] - e.w;
  e.x -= grow / 2;
  e.prevX = e.x;
  e.w = e.h = kMeatSizes[size];
  e.hp = std::min(kMeatHp[size], e.hp + kMeatHp[size] - kMeatHp[old]);
  e.variant = size;
}

void World::setupKitchenEnemy(Enemy& en, const EntityDef& e)
{
  if (en.kind == EnemyKind::Meatball)
  {
    const std::string size = e.str("size", "small");
    en.variant = 0;
    en.hp = kMeatHp[0];
    setMeatballSize(en, size == "giant" ? 2 : (size == "big" ? 1 : 0));
    en.hp = kMeatHp[en.variant];
    en.dir = e.str("dir", "l") == "r" ? 1 : -1;
  }
  if (en.kind == EnemyKind::EggTimer || en.kind == EnemyKind::CleaverBot)
    en.attach = 0; // asleep / arm down
  if (en.kind == EnemyKind::Meatball || en.kind == EnemyKind::EggTimer || en.kind == EnemyKind::CleaverBot)
    mKitchen.on = true;
}

void World::linkKitchen()
{
  auto& k = mKitchen;
  if (!k.on)
    return;
  for (const auto& f : k.flaps)
    for (int y = f.y; y < f.y + f.h; ++y)
      mMap.setBlock(f.x, y, Tile::Solid);
  k.cpDirs.clear();
  for (const auto& b : k.belts)
    k.cpDirs.push_back(b.dir);
}

// --- Belts ---------------------------------------------------------------------------------

int World::beltUnder(int x, int w, int bottom) const
{
  const auto& k = mKitchen;
  for (std::size_t i = 0; i < k.belts.size(); ++i)
  {
    const Belt& b = k.belts[i];
    if (!b.hang && bottom + 1 == b.y * kCellsPerTile && overlapsBelt(b, x, w))
      return int(i);
  }
  return -1;
}

void World::openFlapsFor(const CellBox& box, int dir)
{
  for (auto& f : mKitchen.flaps)
  {
    const int fx = f.x * kCellsPerTile;
    const bool beside = dir > 0 ? box.right() + 1 == fx : box.left() - 1 == fx + kCellsPerTile - 1;
    const bool rows = box.bottom() >= f.y * kCellsPerTile && box.top() < (f.y + f.h) * kCellsPerTile;
    if (!beside || !rows)
      continue;
    if (f.open == 0)
    {
      for (int y = f.y; y < f.y + f.h; ++y)
        mMap.setBlock(f.x, y, Tile::Empty);
      playSound(Sfx::Creak);
    }
    f.open = 15;
  }
}

void World::moveBelts()
{
  auto& k = mKitchen;
  auto& p = mPlayer;
  const int frame = mStats.frames;
  for (auto& b : k.belts)
  {
    if (b.flash > 0)
      --b.flash;
    if (frame % b.period != 0)
      continue;
    b.phase += b.dir;
    const int lo = b.x0 * kCellsPerTile, hi = b.x1 * kCellsPerTile + kCellsPerTile - 1;
    // The runner: standing on it, or hanging from the order rail. Turbo
    // runs against belts as if they were still.
    if (p.state != PlayerState::Dying && p.turbo == 0 && k.hand == 0 && p.vehicle < 0)
    {
      const CellBox pb = p.box();
      if (!b.hang && standingState(p) && pb.bottom() + 1 == b.y * kCellsPerTile && overlapsBelt(b, pb.x, pb.w))
      {
        openFlapsFor(pb, b.dir);
        mMap.moveHorizontally(p.x, p.y, Player::kWidth, p.height(), b.dir);
      }
      else if (b.hang && p.state == PlayerState::Pipe && pb.top() >= b.y * kCellsPerTile &&
               pb.top() < (b.y + 1) * kCellsPerTile && overlapsBelt(b, pb.x, pb.w) &&
               mMap.climbable(pb.left() + 1 + b.dir, pb.top()))
        p.x += b.dir;
    }
    if (b.hang)
      continue;
    // Enemies standing on it.
    for (auto& e : mEnemies)
    {
      if (!e.alive || e.stun > 0 || (e.kind != EnemyKind::Meatball && e.kind != EnemyKind::EggTimer))
        continue;
      const CellBox eb = e.box();
      if (eb.bottom() + 1 != b.y * kCellsPerTile || !overlapsBelt(b, eb.x, eb.w))
        continue;
      if (!mMap.overlapsSolid(boxAt(e.x + b.dir, e.y, e.w, e.h)))
        e.x += b.dir;
    }
    // Items riding it; on a loop they come back at the start. (Item boxes
    // stay put: the bot's planner goes for them.)
    auto ride = [&](int& x, int y) {
      if (y + 1 != b.y * kCellsPerTile || x + 1 < lo || x > hi)
        return;
      const int nx = x + b.dir;
      if (nx + 1 < lo || nx > hi - 1)
      {
        if (b.loop)
          x = b.dir > 0 ? lo : hi - 1;
        return;
      }
      x = nx;
    };
    for (auto& it : mItems)
      if (!it.taken && it.heldBy < 0)
      {
        const int x0 = it.x;
        ride(it.x, it.y);
        if (std::abs(it.x - x0) > 1)
          it.prevX = it.x; // wrapped round: no tween across the belt
      }
  }
  // Flaps swing shut once nothing is in them.
  for (auto& f : k.flaps)
  {
    if (f.open == 0 || --f.open > 0)
      continue;
    const CellBox fb{f.x * kCellsPerTile, f.y * kCellsPerTile, kCellsPerTile, f.h * kCellsPerTile};
    if (fb.intersects(p.box()))
    {
      f.open = 1;
      continue;
    }
    for (int y = f.y; y < f.y + f.h; ++y)
      mMap.setBlock(f.x, y, Tile::Solid);
  }
}

// --- Enemies -------------------------------------------------------------------------------

void World::updateMeatball(Enemy& e, const EnemyDef& def)
{
  auto& k = mKitchen;
  auto& p = mPlayer;
  if (e.cool > 0)
    --e.cool;
  const CellBox eb = e.box();
  // Down the sink: gone with a squelch.
  for (const auto& s : k.sinks)
  {
    const CellBox sb{s.x0 * kCellsPerTile, s.y0 * kCellsPerTile, (s.x1 - s.x0 + 1) * kCellsPerTile,
      (s.y1 - s.y0 + 1) * kCellsPerTile};
    if (sb.intersects(eb))
    {
      e.alive = false;
      burst(cellCenter(eb), rgb(200, 70, 40), rgb(255, 160, 90), 12, 1.4f);
      playSound(Sfx::Squelch);
      return;
    }
  }
  if (!mMap.onSolidGround(eb))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, 1);
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return;
  }
  // On a belt it rolls the belt's way (the belt adds its own push).
  const int belt = beltUnder(eb.x, eb.w, eb.bottom());
  if (belt >= 0)
    e.dir = k.belts[std::size_t(belt)].dir;
  if (e.timer % std::max(1, def.stepEvery) == 0)
  {
    const bool wall = e.dir > 0 ? mMap.touchingRightWall(eb) : mMap.touchingLeftWall(eb);
    if (!wall)
      e.x += e.dir;
    else if (belt < 0)
      e.dir = -e.dir;
  }
  // Through a sauce pan: one size bigger, once a pass.
  bool inSauce = false;
  for (std::size_t i = 0; i < k.sauces.size(); ++i)
  {
    const auto& s = k.sauces[i];
    const int cx = e.x + e.w / 2;
    if (cx >= s.x0 * kCellsPerTile && cx < (s.x1 + 1) * kCellsPerTile && e.y >= s.y0 * kCellsPerTile &&
        e.y < (s.y1 + 1) * kCellsPerTile)
    {
      inSauce = true;
      if (e.ox == 0 && e.variant < 2)
      {
        const int s2 = kMeatSizes[e.variant + 1];
        const int grow = s2 - e.w;
        if (!mMap.overlapsSolid(boxAt(e.x - grow / 2, e.y, s2, s2)))
        {
          setMeatballSize(e, e.variant + 1);
          playSound(Sfx::Squelch);
        }
      }
      e.ox = 1;
    }
  }
  if (!inSauce)
    e.ox = 0;
  // Contact: a small one bounces the runner back a block; big and giant
  // ones hurt; a moldy one infects.
  if (e.cool == 0 && p.state != PlayerState::Dying && p.state != PlayerState::Teleporting &&
      e.box().intersects(p.hitBox()))
  {
    e.cool = 20;
    if (e.carrier)
      infect();
    else if (e.variant == 0)
    {
      const int dir = p.x + 1 < e.x + e.w / 2 ? -1 : 1;
      mMap.moveHorizontally(p.x, p.y, Player::kWidth, p.height(), 2 * dir);
      playSound(Sfx::Squelch);
    }
    else
      hurtPlayer(1);
  }
}

void World::updateCleaverBot(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  if (e.cool > 0)
    --e.cool;
  const int dx = (p.x + 1) - (e.x + 1);
  e.dir = dx < 0 ? -1 : 1;
  if (e.tell > 0)
  {
    if (--e.tell > 0)
      return;
    // Lob a cleaver where the runner stands now.
    const float g = 0.15f;
    const float sx = float(e.x + 1), sy = float(e.y - 4);
    const float tx = float(p.x + 1), ty = float(p.y - 2);
    const float T = std::clamp(std::fabs(tx - sx) / 1.0f, 10.0f, 22.0f);
    Projectile pr;
    pr.kind = ShotKind::Enemy;
    pr.w = pr.h = 2;
    pr.speed = 1;
    pr.damage = 1;
    pr.precise = true;
    pr.range = 120;
    pr.gy = g;
    pr.fx = sx;
    pr.fy = sy;
    pr.vx = (tx - sx) / T;
    pr.vy = (ty - sy - 0.5f * g * T * T) / T;
    pr.dx = dx < 0 ? -1 : 1;
    pr.x = pr.prevX = int(sx);
    pr.y = pr.prevY = int(sy);
    pr.thrown = 4; // a cleaver
    mProjectiles.push_back(pr);
    playSound(Sfx::Throw);
    e.cool = def.cooldown;
    return;
  }
  const bool inRange = std::abs(dx) <= def.range && std::abs(p.y - e.y) <= 12 && p.state != PlayerState::Dying;
  if (inRange && e.cool == 0)
    e.tell = def.tell; // the arm goes up
}

void World::updateEggTimer(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  if (e.attach == 0)
  {
    e.attach = 1; // woke as it came on screen: the dial starts
    e.cool = kEggDial;
  }
  if (--e.cool <= 0)
  {
    // Pop: a heart to a runner within reach, and gone.
    const Vec2 c = cellCenter(e.box());
    const int reach = kEggRadius * kCellsPerTile + 1;
    if (std::abs(p.x + 1 - (e.x + 1)) <= reach && std::abs(p.y - 2 - (e.y - 1)) <= reach &&
        p.state != PlayerState::Dying)
      hurtPlayer(1);
    e.alive = false;
    burst(c, rgb(255, 240, 200), rgb(255, 200, 60), 18, 2.0f);
    flashAt(c, 90.0f, rgb(255, 220, 120), 12);
    playSound(Sfx::Explosion);
    return;
  }
  if (e.cool <= kEggRing && e.cool % 4 == 0)
    playSound(Sfx::EggRing);
  // Toddles at the runner along its floor.
  const CellBox b = e.box();
  if (!mMap.onSolidGround(b))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, 1);
    return;
  }
  if (e.timer % std::max(1, def.stepEvery) != 0)
    return;
  e.dir = p.x + 1 < e.x + 1 ? -1 : 1;
  const int aheadX = e.dir > 0 ? b.right() + 1 : b.left() - 1;
  const bool wall = e.dir > 0 ? mMap.touchingRightWall(b) : mMap.touchingLeftWall(b);
  if (!wall && mMap.solidTop(aheadX, b.bottom() + 1) && std::abs(p.x + 1 - (e.x + 1)) > 1)
    e.x += e.dir;
}

// --- Shots ---------------------------------------------------------------------------------

bool World::shotAtKitchen(Projectile& pr, const CellBox& b)
{
  auto& k = mKitchen;
  for (auto& bell : k.bells)
  {
    const CellBox bb{bell.x * kCellsPerTile, bell.y * kCellsPerTile, kCellsPerTile, kCellsPerTile};
    if (!bb.intersects(b))
      continue;
    if (bell.ding == 0 && bell.rest == 0)
    {
      bell.ding = kBellDing;
      bell.rest = kBellRest;
      ++bell.rings;
      playSound(Sfx::BellDing);
      for (auto& belt : k.belts)
        if (belt.station == bell.station)
          belt.flash = kBellDing;
    }
    (void)pr;
    return true;
  }
  if (k.leverX >= 0)
  {
    const CellBox lb{k.leverX * kCellsPerTile, k.leverY * kCellsPerTile, kCellsPerTile, kCellsPerTile};
    if (lb.intersects(b))
    {
      if (k.toast == 0)
      {
        k.toast = kToastPop;
        playSound(Sfx::Click);
      }
      return true;
    }
  }
  return false;
}

int World::shotAtKitchenEnemy(Projectile& pr, Enemy& e)
{
  auto& k = mKitchen;
  if (e.kind == EnemyKind::EggTimer && e.attach != 0)
    e.cool = std::min(kEggDial, e.cool + kEggWind); // wound back
  if (pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::PepperGrinder))
  {
    const int idx = int(&e - mEnemies.data());
    KitchenState::Sneeze* s = nullptr;
    for (auto& z : k.sneezes)
      if (z.enemy == idx)
        s = &z;
    if (!s)
    {
      k.sneezes.push_back({idx, 0, mStats.frames});
      s = &k.sneezes.back();
    }
    if (mStats.frames - s->since > kSneezeWindow)
    {
      s->hits = 0;
      s->since = mStats.frames;
    }
    if (++s->hits >= kSneezeHits)
    {
      s->hits = 0;
      s->since = mStats.frames;
      e.stun = kSneezeStun;
      k.sneezing = 20;
      playSound(Sfx::Sneeze);
    }
  }
  return 0;
}

void World::stickCleaver(const Projectile& pr)
{
  auto& k = mKitchen;
  const int dir = pr.vx < 0.0f ? -1 : 1;
  int bx = (pr.x + pr.w / 2) / kCellsPerTile, by = pr.y / kCellsPerTile;
  // In front of the wall it hit.
  for (int i = 0; i < 2 && mMap.block(bx, by) != Tile::Empty; ++i)
    bx -= dir;
  if (mMap.block(bx, by) != Tile::Empty || (mMap.block(bx + dir, by) != Tile::Solid))
    return;
  if (k.cleavers.size() >= 6)
  {
    const auto& old = k.cleavers.front();
    if (old.set)
      mMap.setBlock(old.bx, old.by, Tile::Empty);
    k.cleavers.erase(k.cleavers.begin());
  }
  mMap.setBlock(bx, by, Tile::Platform);
  k.cleavers.push_back({bx, by, kCleaverStick, dir, true});
  playSound(Sfx::CleaverThunk);
}

// --- Update --------------------------------------------------------------------------------

void World::updateKitchen(const PlayerInput& input)
{
  (void)input;
  auto& k = mKitchen;
  auto& p = mPlayer;

  // A new checkpoint: the belts come back this way after a fall.
  int active = 0;
  for (const auto& cp : mCheckpoints)
    active += cp.active ? 1 : 0;
  if (active != k.checkpoints)
  {
    k.checkpoints = active;
    k.cpDirs.clear();
    for (const auto& b : k.belts)
      k.cpDirs.push_back(b.dir);
  }

  // Bells: the station's belts flip after the ding.
  for (auto& bell : k.bells)
  {
    if (bell.rest > 0)
      --bell.rest;
    if (bell.ding > 0 && --bell.ding == 0)
      for (auto& b : k.belts)
        if (b.station == bell.station)
        {
          b.dir = -b.dir;
          b.flash = 6;
        }
  }

  // The chef's hand: lift the runner, show them off, set them down.
  if (!k.handDone && k.handX >= 0 && standingState(p) && p.x / kCellsPerTile >= k.handX &&
      std::abs(p.y / kCellsPerTile - k.handY) <= 2)
  {
    k.handDone = true;
    k.hand = kHandFrames;
    k.handBaseY = p.y;
    p.facing = 1;
    showMessage("TODAY'S SPECIAL!");
  }
  if (k.hand > 0)
  {
    const int f = kHandFrames - k.hand;
    const int lift = f < 8 ? f / 2 : (f < 48 ? 4 : std::max(0, 4 - (f - 48) / 3));
    p.prevY = p.y;
    p.y = k.handBaseY - lift;
    if (--k.hand == 0)
    {
      p.y = p.prevY = k.handBaseY;
      p.state = PlayerState::OnGround;
    }
  }

  moveBelts();

  // The meatball hatch.
  const int meatDef = enemyIndex("meatball");
  for (auto& h : k.hatches)
  {
    h.balls.erase(std::remove_if(h.balls.begin(), h.balls.end(),
                    [&](int i) { return std::size_t(i) >= mEnemies.size() || !mEnemies[std::size_t(i)].alive; }),
      h.balls.end());
    if (meatDef < 0 || std::abs(p.x - h.x) > 48 * kCellsPerTile)
      continue;
    if (++h.t >= h.every && int(h.balls.size()) < h.max)
    {
      h.t = 0;
      spawnEnemy(meatDef, h.x, h.y);
      Enemy& e = mEnemies.back();
      e.dir = -1;
      e.active = true;
      e.carrier = h.carrier > 0 && (++h.count % h.carrier) == 0;
      h.balls.push_back(int(mEnemies.size()) - 1);
    }
  }

  // Cleavers work loose.
  for (auto& c : k.cleavers)
    if (--c.t <= 0 && c.set)
    {
      mMap.setBlock(c.bx, c.by, Tile::Empty);
      c.set = false;
    }
  k.cleavers.erase(std::remove_if(k.cleavers.begin(), k.cleavers.end(), [](const StuckCleaver& c) { return c.t <= 0; }),
    k.cleavers.end());

  // The toaster pops: a runner standing on it goes up with the toast.
  if (k.popped > 0)
    --k.popped;
  if (k.toast > 0 && --k.toast == 0)
  {
    k.popped = 20;
    playSound(Sfx::Toaster);
    const CellBox pb = p.box();
    if (standingState(p) && pb.bottom() + 1 == k.toaster.y0 * kCellsPerTile &&
        pb.right() >= k.toaster.x0 * kCellsPerTile && pb.left() <= k.toaster.x1 * kCellsPerTile + 1)
      startLaunch(kToastLaunch);
  }
  if (k.sneezing > 0)
    --k.sneezing;
}

bool World::kitchenRunnerHit()
{
  return mKitchen.hand > 0; // in the chef's hand: nothing touches you
}

void World::resetKitchen()
{
  auto& k = mKitchen;
  k.hand = 0;
  k.toast = 0;
  for (auto& f : k.flaps)
  {
    f.open = 0;
    for (int y = f.y; y < f.y + f.h; ++y)
      mMap.setBlock(f.x, y, Tile::Solid);
  }
  for (const auto& c : k.cleavers)
    if (c.set)
      mMap.setBlock(c.bx, c.by, Tile::Empty);
  k.cleavers.clear();
  for (std::size_t i = 0; i < k.belts.size() && i < k.cpDirs.size(); ++i)
    k.belts[i].dir = k.cpDirs[i];
  for (auto& b : k.bells)
    b.ding = b.rest = 0;
  for (auto& h : k.hatches)
  {
    for (int i : h.balls)
      if (std::size_t(i) < mEnemies.size())
        mEnemies[std::size_t(i)].alive = false;
    h.balls.clear();
    h.t = 0;
  }
}

// --- Saves ---------------------------------------------------------------------------------

bool World::kitchenCanSave() const
{
  const auto& k = mKitchen;
  bool busy = k.hand > 0 || k.toast > 0 || !k.cleavers.empty();
  for (const auto& b : k.bells)
    busy = busy || b.ding > 0;
  for (const auto& f : k.flaps)
    busy = busy || f.open > 0;
  return !busy;
}

std::vector<int> World::kitchenSave() const
{
  // [hand done, belt directions..., bell rings..., hatch counts...]
  const auto& k = mKitchen;
  std::vector<int> s{k.handDone ? 1 : 0};
  for (const auto& b : k.belts)
    s.push_back(b.dir);
  for (const auto& b : k.bells)
    s.push_back(b.rings);
  for (const auto& h : k.hatches)
    s.push_back(h.count);
  return s;
}

bool World::validKitchenSave(const std::vector<int>& s) const
{
  const auto& k = mKitchen;
  return s.size() == 1 + k.belts.size() + k.bells.size() + k.hatches.size();
}

void World::loadKitchen(const std::vector<int>& s)
{
  auto& k = mKitchen;
  std::size_t i = 0;
  k.handDone = s[i++] != 0;
  for (auto& b : k.belts)
    b.dir = s[i++] < 0 ? -1 : 1;
  for (auto& b : k.bells)
  {
    b.rings = s[i++];
    b.ding = b.rest = 0;
  }
  for (auto& h : k.hatches)
  {
    h.count = s[i++];
    h.t = 0;
    h.balls.clear();
    for (std::size_t j = mLevelEnemyCount; j < mEnemies.size(); ++j)
      if (mEnemies[j].alive && mEnemies[j].kind == EnemyKind::Meatball)
        h.balls.push_back(int(j));
  }
  k.hand = 0;
  k.checkpoints = -1;
}

} // namespace gr
