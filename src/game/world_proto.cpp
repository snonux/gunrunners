// Prototype weapons (SPEC.md section 4): the green W boxes of each level.
// Every prototype starts from the same data row (fire mode, speed, damage,
// ammo); what makes one special is handled here by its ProtoId.

#include "game/world.hpp"

#include <algorithm>

namespace gr
{

namespace
{

constexpr int kBowDraw = 12; // frames of a full draw of the Jade Bow

} // namespace

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
    mPaint.clear();
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
  if (ProtoId(p.proto) == ProtoId::LockOnRockets)
  {
    // Hold to paint up to three targets, 8 frames each; release to fire a
    // rocket at each (or one straight ahead with none painted).
    if (fire.pressed)
    {
      ++p.charge;
      if (p.charge % 8 == 0 && int(mPaint.size()) < std::min(3, p.ammo))
        paintTarget();
      return;
    }
    if (p.charge > 0 && (p.shotCooldown == 0 || p.turbo > 0))
    {
      p.charge = 0;
      launchRockets();
      p.shotCooldown = def.cooldown;
    }
    p.charge = 0;
    mPaint.clear();
    return;
  }
  if (ProtoId(p.proto) == ProtoId::SunstoneLance)
  {
    // Hold for the beam (world_light.cpp traces it), a shot of ammo every
    // 4 frames; a tap under 4 frames fires a glint instead.
    if (fire.pressed)
    {
      ++p.charge;
      mLanceOn = p.charge >= 4;
      if (mLanceOn && p.charge % 4 == 0 && --p.ammo <= 0)
      {
        p.ammo = 0;
        p.weapon = Weapon::Normal;
        p.proto = -1;
        p.charge = 0;
        mLanceOn = false;
        showMessage("OUT OF AMMO - BACK TO THE BLASTER");
      }
      return;
    }
    if (p.charge > 0 && p.charge < 4 && (p.shotCooldown == 0 || p.turbo > 0))
    {
      fireShot();
      p.shotCooldown = def.cooldown;
    }
    p.charge = 0;
    mLanceOn = false;
    return;
  }
  if (ProtoId(p.proto) == ProtoId::JadeBow)
  {
    // Hold to draw, release to loose: a full draw takes 12 frames (Turbo
    // makes any shot one).
    if (fire.pressed)
    {
      if (++p.charge == kBowDraw)
        playSound(Sfx::Click);
      return;
    }
    if (p.charge > 0 && (p.shotCooldown == 0 || p.turbo > 0))
    {
      mBowFull = p.charge >= kBowDraw || p.turbo > 0;
      fireShot();
      mBowFull = false;
      p.shotCooldown = def.cooldown;
    }
    p.charge = 0;
    return;
  }
  if (ProtoId(p.proto) == ProtoId::BreachCharge && breachTrigger(fire))
  {
    p.charge = 0;
    return; // set the charges off instead of placing another
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
  if (shoot && ProtoId(p.proto) == ProtoId::Boomerang)
  {
    // One Boomerang in the air at a time.
    for (const auto& pr : mProjectiles)
      shoot = shoot && !(pr.alive && pr.kind == ShotKind::Proto && pr.proto == p.proto);
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
  if (ProtoId(p.proto) == ProtoId::BassCannon && dx != 0)
  {
    // A wall of sound instead of a projectile (world_club.cpp).
    int damage = def.damage;
    if (p.turbo > 0)
      damage *= 2;
    else if (p.virus > 0)
      damage = std::max(1, damage / 2);
    fireCone(ox, oy, dx, damage);
    return;
  }
  if (ProtoId(p.proto) == ProtoId::ArcCaster && dx != 0)
  {
    // Instant lightning instead of a projectile (world_maglev.cpp).
    int damage = def.damage;
    if (p.turbo > 0)
      damage *= 2;
    else if (p.virus > 0)
      damage = std::max(1, damage / 2);
    fireArc(ox, oy, dx, damage);
    return;
  }
  if (ProtoId(p.proto) == ProtoId::BreachCharge)
  {
    // Not a projectile: a sticky charge (world_station.cpp).
    placeCharge(ox, oy);
    return;
  }
  if (ProtoId(p.proto) == ProtoId::BlastingCaps)
  {
    // Not a projectile: a cap with a fuse (world_mine.cpp).
    throwCap(ox, oy);
    return;
  }
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
    case ProtoId::FlareGun:
      pr.flare = true; // sticks and lights up (world_dark.cpp)
      break;
    case ProtoId::LockOnRockets:
    {
      // A homing rocket: climbs, then curves in on its target from above.
      pr.precise = true;
      pr.fx = float(pr.x);
      pr.fy = float(pr.y);
      pr.target = mNextTarget;
      if (pr.target != kNoTarget)
      {
        pr.vx = 0.55f * float(dx == 0 ? p.facing : dx);
        pr.vy = -0.83f;
      }
      else
      {
        pr.vx = float(dx);
        pr.vy = float(dy);
      }
      break;
    }
    case ProtoId::Boomerang:
      // Out, a block up at the turn, and back (world_jungle.cpp); it passes
      // through what it hits, once each way.
      pr.pierce = true;
      pr.w = pr.h = 2;
      if (pr.dy < 0)
        pr.y = oy - 1;
      pr.sx = pr.sy = 0;
      playSound(Sfx::Whoosh);
      break;
    case ProtoId::SnareBolas:
    {
      // A flat lob, about ten blocks (world_temple.cpp pins what it hits).
      pr.precise = true;
      pr.w = pr.h = 2;
      pr.y = oy - 1;
      pr.fx = float(pr.x);
      pr.fy = float(pr.y);
      const int dir = dx == 0 ? p.facing : dx;
      pr.vx = dy < 0 ? 0.4f * float(dir) : float(dir);
      pr.vy = dy < 0 ? -1.0f : -0.55f;
      pr.gy = 0.125f;
      playSound(Sfx::Whoosh);
      break;
    }
    case ProtoId::SerpentSpear:
      // Pierces up to three; sticks in the first wall as a foothold whose
      // top is the row under the thrower's feet (world_lava.cpp).
      pr.pierceLeft = 2;
      pr.w = 3;
      pr.x = dx < 0 ? ox - pr.w + 1 : ox;
      pr.footRow = dy == 0 ? p.y + 1 : -1;
      playSound(Sfx::Whoosh);
      break;
    case ProtoId::JadeBow:
      // A quick shot, or a full draw: faster, 4 damage, through every enemy
      // in its line; it breaks an armor plate and flies along a Glyph
      // Sentinel's glyphs to their master (world_sanctum.cpp).
      if (mBowFull)
      {
        pr.pierce = true;
        pr.strong = true;
        if (dx != 0)
        {
          pr.w = 3;
          pr.x = dx < 0 ? ox - pr.w + 1 : ox;
        }
      }
      else
      {
        pr.speed = 3;
        damage = 1;
      }
      break;
    case ProtoId::BubbleGun:
      // A slow bubble that drifts up; it traps what it hits (world_sludge.cpp).
      pr.w = pr.h = 2;
      pr.y = oy - 1;
      pr.range = 48;
      playSound(Sfx::Bubble);
      break;
    default:
      break;
  }
  if (p.turbo > 0)
    damage *= 2;
  else if (p.virus > 0)
    damage = std::max(1, damage / 2);
  pr.damage = damage;
  if (ProtoId(p.proto) == ProtoId::FanDarts)
  {
    // A fan of three: straight on, and 15 degrees either side.
    const Projectile base = pr;
    for (int side : {-1, 1})
    {
      Projectile d = base;
      d.precise = true;
      d.fx = float(d.x);
      d.fy = float(d.y);
      if (dx != 0)
      {
        d.vx = 0.966f * float(dx);
        d.vy = 0.259f * float(side);
      }
      else
      {
        d.vx = 0.259f * float(side);
        d.vy = 0.966f * float(dy);
      }
      mProjectiles.push_back(d);
    }
    playSound(Sfx::Dart);
  }
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
