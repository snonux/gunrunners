// The guns a runner carries: the one in hand plus up to two spares
// (Player::spares), the blaster always among them. A pickup still puts the
// new gun in hand, as it always did; the gun held so far now stays in the
// spares instead of being thrown away, and nextWeapon cycles through them.

#include "game/world.hpp"

#include <algorithm>

namespace gr
{

namespace
{

bool sameGun(Weapon a, int protoA, Weapon b, int protoB)
{
  return a == b && (a != Weapon::Proto || protoA == protoB);
}

} // namespace

void World::tidySpares()
{
  auto& p = mPlayer;
  if (p.weapon != Weapon::Proto)
    p.proto = -1;
  bool blaster = p.weapon == Weapon::Normal;
  int n = 0;
  for (int i = 0; i < p.spareCount; ++i)
  {
    const Player::Spare s = p.spares[std::size_t(i)];
    if (s.weapon == Weapon::Normal)
    {
      if (blaster)
        continue; // back in hand (the gun that was there ran dry)
      blaster = true;
    }
    else if (sameGun(s.weapon, s.proto, p.weapon, p.proto) || s.ammo <= 0 ||
             (s.weapon == Weapon::Proto && (s.proto < 0 || s.proto != mLevelProto)))
      continue;
    p.spares[std::size_t(n++)] = s;
  }
  // The blaster always comes along.
  if (!blaster)
    p.spares[std::size_t(n < int(p.spares.size()) ? n++ : n - 1)] = Player::Spare{};
  p.spareCount = n;
}

void World::stowGun()
{
  auto& p = mPlayer;
  p.charge = 0;
  mHoldFireWait = 0;
  mLanceOn = false;
  mPaint.clear();
  mGreen.trim = 0;
  mCity.breath = 0;
  mCity.breathAng = 0;
  mGarden.mist = 0;
  mFly.stream = 0;
  mWarehouse.pulling = false;
}

Player::Spare* World::spareGun(Weapon w, int proto)
{
  auto& p = mPlayer;
  for (int i = 0; i < p.spareCount; ++i)
    if (sameGun(p.spares[std::size_t(i)].weapon, p.spares[std::size_t(i)].proto, w, proto))
      return &p.spares[std::size_t(i)];
  return nullptr;
}

bool World::equip(Weapon w, int proto)
{
  auto& p = mPlayer;
  if (w != Weapon::Proto)
    proto = -1;
  tidySpares();
  if (sameGun(p.weapon, p.proto, w, proto))
    return true;
  Player::Spare next{w, proto, 0};
  bool had = false;
  for (int i = 0; i < p.spareCount; ++i)
    if (sameGun(p.spares[std::size_t(i)].weapon, p.spares[std::size_t(i)].proto, w, proto))
    {
      next = p.spares[std::size_t(i)];
      std::copy(p.spares.begin() + i + 1, p.spares.begin() + p.spareCount, p.spares.begin() + i);
      --p.spareCount;
      had = true;
      break;
    }
  const Player::Spare held{p.weapon, p.proto, p.ammo};
  if (p.spareCount < int(p.spares.size()))
    p.spares[std::size_t(p.spareCount++)] = held;
  else if (held.weapon == Weapon::Normal)
    p.spares[std::size_t(p.spareCount - 1)] = held; // the gun held most recently makes room
  // Otherwise the gun in hand is dropped for the new one.
  stowGun();
  p.weapon = w;
  p.proto = proto;
  p.ammo = next.ammo;
  return had;
}

bool World::weaponLocked() const
{
  // High Noon's Six-Shooter, Air Hockey's Freeze Ray and Trigger Happy's
  // blaster are the level's rule, not a pickup.
  return mWest.noon.on || mCryo.zeroFriction || mFly.trigger;
}

bool World::nextWeapon()
{
  auto& p = mPlayer;
  tidySpares();
  if (p.spareCount == 0 || mState != WorldState::Playing || weaponLocked() || mFireHeld || p.vehicle >= 0 ||
      p.state == PlayerState::Dying || p.state == PlayerState::Teleporting)
    return false;
  const Player::Spare held{p.weapon, p.proto, p.ammo};
  const Player::Spare next = p.spares[0];
  std::copy(p.spares.begin() + 1, p.spares.begin() + p.spareCount, p.spares.begin());
  p.spares[std::size_t(p.spareCount - 1)] = held;
  stowGun();
  p.weapon = next.weapon;
  p.proto = next.proto;
  p.ammo = next.ammo;
  showMessage(p.weapon == Weapon::Proto ? protoDef(p.proto).name : weaponName(p.weapon));
  playSound(Sfx::FlintCock);
  return true;
}

} // namespace gr
