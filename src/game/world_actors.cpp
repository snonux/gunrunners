// The campaign's enemy behaviours beyond the PoC's walker, flyer and turret
// (SPEC section 5): clingers, rail riders, snipers. Each one has a clear
// tell before it attacks.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace gr
{

namespace
{

int sgn(int v) { return (v > 0) - (v < 0); }

} // namespace

void World::touchPlayer(const Enemy& e)
{
  auto& p = mPlayer;
  if (p.state == PlayerState::Dying || p.state == PlayerState::Teleporting)
    return;
  if (e.flags() & kEnemyCarrier)
  {
    // Carriers infect instead of hurting.
    if (p.mercy > 0 || p.virus > 0)
      return;
    infect();
    p.mercy = 20;
    return;
  }
  const bool fresh = p.mercy == 0 && p.turbo == 0;
  hurtPlayer(1);
  if (fresh && e.kind == EnemyKind::Bouncer)
  {
    // Shoved three blocks away from him.
    const CellBox pb = p.box(), b = e.box();
    const int dir = pb.x + 1 < b.x + b.w / 2 ? -1 : 1;
    mMap.moveHorizontally(p.x, p.y, Player::kWidth, p.height(), dir * 6);
    if (p.state == PlayerState::OnGround && !mMap.onSolidGround(p.box()))
      startFalling();
  }
}

bool World::lineOfFire(int x0, int y0, int x1, int y1, int& hitX, int& hitY) const
{
  // Walks the cells from (x0, y0) toward (x1, y1) and past it, up to 40
  // cells; stops at the first solid cell.
  const float dx = float(x1 - x0), dy = float(y1 - y0);
  const float len = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
  const float sx = dx / len, sy = dy / len;
  float fx = float(x0) + 0.5f, fy = float(y0) + 0.5f;
  for (int i = 0; i < 48; ++i)
  {
    fx += sx;
    fy += sy;
    const int cx = int(std::floor(fx)), cy = int(std::floor(fy));
    if (mMap.solid(cx, cy))
    {
      hitX = cx;
      hitY = cy;
      return false;
    }
  }
  hitX = int(std::floor(fx));
  hitY = int(std::floor(fy));
  return true;
}

void World::shootAt(Enemy& e, int fromX, int fromY, int speed, int range)
{
  const CellBox pb = mPlayer.box();
  Projectile pr;
  pr.kind = ShotKind::Enemy;
  pr.w = pr.h = 1;
  pr.speed = speed;
  pr.damage = 1;
  pr.range = range;
  pr.carrier = (e.flags() & kEnemyCarrier) != 0;
  const float dx = float(pb.x + 1 - fromX), dy = float(pb.y + 2 - fromY);
  const float len = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
  pr.precise = true;
  pr.fx = float(fromX);
  pr.fy = float(fromY);
  pr.vx = dx / len;
  pr.vy = dy / len;
  pr.dx = sgn(int(std::lround(pr.vx * 2.0f)));
  pr.dy = sgn(int(std::lround(pr.vy * 2.0f)));
  pr.x = pr.prevX = fromX;
  pr.y = pr.prevY = fromY;
  mProjectiles.push_back(pr);
  playSound(Sfx::EnemyShot);
}

// Finds the surface a clinger was placed against and fits its box to it:
// upright on walls (2 wide, 3 tall), flat on ceilings and floors.
void World::placeClinger(Enemy& e)
{
  const int cx = e.x, by = e.y;
  for (int k = 0; k <= 3; ++k)
    if (mMap.solid(cx + k, by))
    {
      if (k == 0)
        break;
      e.attach = 1;
      e.w = 2;
      e.h = 3;
      e.x = cx + k - 2;
      return;
    }
  if (mMap.solid(cx - 1, by))
  {
    e.attach = -1;
    e.w = 2;
    e.h = 3;
    return;
  }
  for (int k = 1; k <= 3; ++k)
    if (mMap.solid(cx + 1, by - e.h + 1 - k))
    {
      e.attach = 2;
      e.y = by - k + 1;
      return;
    }
  e.attach = 0;
}

// Glass Crawler and friends: stick to a wall or ceiling, creep toward the
// player along it, glow and spit a short-range spark.
void World::updateCrawler(Enemy& e, const EnemyDef& def)
{
  const CellBox pb = mPlayer.box();
  const int pcx = pb.x + 1, pcy = pb.y + 2;
  CellBox b = e.box();
  const int ecx = b.x + b.w / 2, ecy = b.y + b.h / 2;

  auto attached = [&](const CellBox& nb) {
    for (int i = 0; i < (e.attach == 2 || e.attach == 0 ? nb.w : nb.h); ++i)
    {
      bool s = false;
      switch (e.attach)
      {
        case -1: s = mMap.solid(nb.left() - 1, nb.top() + i); break;
        case 1: s = mMap.solid(nb.right() + 1, nb.top() + i); break;
        case 2: s = mMap.solid(nb.left() + i, nb.top() - 1); break;
        default: s = mMap.solidTop(nb.left() + i, nb.bottom() + 1); break;
      }
      if (s)
        return true;
    }
    return false;
  };

  if (e.tell == 0 && e.timer % std::max(1, def.stepEvery) == 0)
  {
    int mx = 0, my = 0;
    if (e.attach == -1 || e.attach == 1)
      my = sgn(pcy - ecy);
    else
      mx = sgn(pcx - ecx);
    if (mx != 0 || my != 0)
    {
      const CellBox nb = boxAt(e.x + mx, e.y + my, e.w, e.h);
      if (!mMap.overlapsSolid(nb) && attached(nb))
      {
        e.x += mx;
        e.y += my;
      }
    }
    if (mx != 0)
      e.dir = mx;
  }

  const int dist = std::max(std::abs(pcx - ecx), std::abs(pcy - ecy));
  const bool vulnerable = mPlayer.state != PlayerState::Dying && mPlayer.state != PlayerState::Teleporting;
  if (e.tell > 0)
  {
    if (--e.tell == 0)
    {
      shootAt(e, ecx, ecy, 1, def.range);
      e.lastDive = e.timer + def.cooldown;
    }
  }
  else if (vulnerable && dist <= def.range + 6 && e.timer >= e.lastDive && isOnScreen(b, 0))
  {
    e.tell = def.tell; // mandibles glow
  }
}

// Squeegee Drone: parks at one end of its rail, lights the rail up, then
// sweeps to the other end and shoves whoever is in the way.
void World::updateRider(Enemy& e, const EnemyDef& def)
{
  const int period = std::max(30, def.cooldown);
  const int phase = e.timer % period;
  if (e.dive == 0)
  {
    e.tell = phase >= period - def.tell ? period - phase : 0;
    if (phase == 0)
      e.dive = e.x <= e.railX0 ? 1 : -1; // start a sweep toward the far end
  }
  if (e.dive != 0)
  {
    for (int s = 0; s < 2; ++s)
    {
      const int nx = e.x + e.dive;
      if (nx < e.railX0 || nx > e.railX1)
      {
        e.dive = 0;
        break;
      }
      e.x = nx;
    }
    e.dir = e.dive != 0 ? e.dive : e.dir;
    auto& p = mPlayer;
    if (e.dive != 0 && e.box().intersects(p.hitBox()) && p.state != PlayerState::Dying)
    {
      // A shove, no damage: three blocks along the sweep.
      mMap.moveHorizontally(p.x, p.y, Player::kWidth, p.height(), e.dive * 3 * kCellsPerTile);
      if (p.state == PlayerState::OnGround && !mMap.onSolidGround(p.box()))
        startFalling();
      playSound(Sfx::Hit);
    }
  }
}

// Penthouse Sniper: wakes while you are below it, tracks you with a red
// laser, holds still, then fires along the line.
void World::updateSniper(Enemy& e, const EnemyDef& def)
{
  const CellBox pb = mPlayer.box();
  const CellBox b = e.box();
  const int eyeX = b.x + (e.dir > 0 ? b.w - 1 : 0), eyeY = b.y + 1;
  const bool awake = pb.top() > b.bottom() && pb.top() - b.bottom() <= def.range * kCellsPerTile &&
    std::abs(pb.x - b.x) < 40 && mPlayer.state != PlayerState::Dying;
  e.dir = pb.x + 1 >= b.x + 1 ? 1 : -1;
  constexpr int kHold = 8;
  const int cycle = def.tell + kHold;
  if (!awake)
  {
    e.tell = 0;
    e.dive = 0;
    return;
  }
  if (e.timer < e.lastDive)
    return; // cooling down
  ++e.dive;
  if (e.dive <= def.tell)
  {
    e.aimX = pb.x + 1;
    e.aimY = pb.y - 2;
  }
  e.tell = cycle - e.dive + 1;
  if (e.dive >= cycle)
  {
    // Fire along the held line.
    Projectile pr;
    pr.kind = ShotKind::Enemy;
    pr.w = pr.h = 1;
    pr.speed = 4;
    pr.damage = 1;
    pr.carrier = (e.flags() & kEnemyCarrier) != 0;
    const float dx = float(e.aimX - eyeX), dy = float(e.aimY - eyeY);
    const float len = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
    pr.precise = true;
    pr.fx = float(eyeX);
    pr.fy = float(eyeY);
    pr.vx = dx / len;
    pr.vy = dy / len;
    pr.dx = e.dir;
    pr.x = pr.prevX = eyeX;
    pr.y = pr.prevY = eyeY;
    mProjectiles.push_back(pr);
    playSound(Sfx::EnemyShot);
    e.dive = 0;
    e.tell = 0;
    e.lastDive = e.timer + def.cooldown;
  }
}

} // namespace gr
