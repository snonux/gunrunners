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
    default:
      break;
  }
  if (p.turbo > 0)
    damage *= 2;
  else if (p.virus > 0)
    damage = std::max(1, damage / 2);
  pr.damage = damage;
}

} // namespace gr
