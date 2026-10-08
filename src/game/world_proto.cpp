// Prototype weapons (SPEC.md section 4): the green W boxes of each level.
// Every prototype starts from the same data row (fire mode, speed, damage,
// ammo); what makes one special is handled here by its ProtoId.

#include "game/world.hpp"

#include <algorithm>

namespace gr
{

bool World::onTheBeat() const
{
  // "On the beat": the first 3 frames of any beat.
  return framesIntoBeat(mStats.frames) < 3;
}

void World::takeProto(const Vec2& at)
{
  auto& p = mPlayer;
  if (mLevelProto < 0)
    return;
  const ProtoDef& def = protoDef(mLevelProto);
  const bool first = !mStats.protoFound;
  if (p.weapon == Weapon::Proto && p.proto == mLevelProto)
  {
    p.ammo = std::min(def.maxAmmo, p.ammo + def.boxAmmo);
  }
  else
  {
    p.weapon = Weapon::Proto;
    p.proto = mLevelProto;
    p.ammo = def.boxAmmo;
    p.charge = 0;
    p.shotCooldown = 0;
  }
  mStats.protoFound = true;
  addScore(2000, at);
  showMessage(std::string(def.name) + (first ? " - LOGGED TO THE ARSENAL" : " - MORE AMMO"));
  playSound(Sfx::WeaponPickup);
  burst(at, def.color, rgb(255, 255, 255), 18, 1.8f);
  flashAt(at, 90.0f, def.color, 16);
}

void World::updateProtoShooting(const Button& fire)
{
  auto& p = mPlayer;
  if (p.shotCooldown > 0)
    --p.shotCooldown;
  if (!canFire())
  {
    p.charge = 0;
    return;
  }
  const ProtoDef& def = protoDef(p.proto);
  bool shoot = false;
  switch (def.mode)
  {
    case FireMode::Auto:
      // Turbo removes the delay; the virus turns auto into tap.
      if (p.virus > 0)
        shoot = fire.triggered;
      else
        shoot = fire.pressed && (p.turbo > 0 || p.shotCooldown == 0);
      break;
    case FireMode::Tap:
    case FireMode::Hold:
    case FireMode::Charge:
    case FireMode::Place:
      shoot = fire.triggered && (p.shotCooldown == 0 || p.turbo > 0);
      break;
  }
  p.charge = fire.pressed ? p.charge + 1 : 0;
  if (shoot && ProtoId(p.proto) == ProtoId::SparkDisc)
  {
    // At most two discs out at a time.
    int out = 0;
    for (const auto& pr : mProjectiles)
      out += pr.alive && pr.kind == ShotKind::Proto && pr.proto == p.proto;
    shoot = out < 2;
  }
  if (shoot)
  {
    fireShot();
    p.shotCooldown = def.cooldown;
  }
}

void World::fireProto(int ox, int oy, int dx, int dy)
{
  auto& p = mPlayer;
  const ProtoDef& def = protoDef(p.proto);
  spawnProjectile(ShotKind::Proto, ox, oy, dx, dy);
  Projectile& pr = mProjectiles.back();
  pr.proto = p.proto;
  pr.speed = std::max(1, def.speed);
  int damage = def.damage;
  switch (ProtoId(p.proto))
  {
    case ProtoId::PulsePistol:
      if (onTheBeat())
      {
        damage *= 2;
        pr.pierceLeft = 1;
        pr.strong = true;
      }
      break;
    case ProtoId::SparkDisc:
      // A disc: pierces every enemy once, rides walls (stepSurfaceShot).
      pr.pierce = true;
      pr.w = pr.h = 2;
      if (pr.dy < 0)
        pr.y = oy - 1;
      break;
    default:
      break;
  }
  if (p.turbo > 0)
    damage *= 2;
  else if (p.virus > 0)
    damage = std::max(1, damage / 2);
  pr.damage = damage;
}

// Surface riders (the Spark Disc): fly straight; on touching a wall, turn
// to ride along it for 30 frames, wrapping outer corners and turning at
// inner ones, like a wall follower. Returns false when the shot is done.
bool World::stepSurfaceShot(Projectile& pr)
{
  auto freeAt = [&](int dx, int dy) {
    return !mMap.overlapsSolid({pr.x + dx, pr.y + dy, pr.w, pr.h});
  };
  if (pr.ride < 0)
  {
    if (freeAt(pr.dx, pr.dy))
    {
      pr.x += pr.dx;
      pr.y += pr.dy;
      return true;
    }
    // Hit a surface: start riding it. Walls send the disc up, ceilings
    // and floors along them the way it was facing.
    pr.ride = 30;
    pr.sx = pr.dx;
    pr.sy = pr.dy;
    if (pr.dx != 0)
    {
      pr.dx = 0;
      pr.dy = -1;
    }
    else
    {
      pr.dx = mPlayer.facing;
      pr.dy = 0;
    }
    playSound(Sfx::Hit);
  }
  // Outer corner: the surface fell away, wrap around it.
  if (freeAt(pr.sx, pr.sy) && freeAt(pr.dx + pr.sx, pr.dy + pr.sy))
  {
    const int ndx = pr.sx, ndy = pr.sy;
    pr.sx = -pr.dx;
    pr.sy = -pr.dy;
    pr.dx = ndx;
    pr.dy = ndy;
  }
  // Inner corner: blocked ahead, turn away from the surface.
  for (int turn = 0; turn < 3 && !freeAt(pr.dx, pr.dy); ++turn)
  {
    const int ndx = -pr.sx, ndy = -pr.sy;
    pr.sx = pr.dx;
    pr.sy = pr.dy;
    pr.dx = ndx;
    pr.dy = ndy;
  }
  if (!freeAt(pr.dx, pr.dy))
    return false;
  pr.x += pr.dx;
  pr.y += pr.dy;
  return true;
}

} // namespace gr
