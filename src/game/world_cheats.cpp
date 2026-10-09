// The pause menu's secret cheats: God Mode, health, the prototype with full
// ammo, Turbo, a cure, the access card, rapid fire and a way out. Using any
// of them marks the run, so the tally pays no bonuses and nothing reaches
// the profile (scores, stars, the Arsenal).

#include "data/weapons.hpp"
#include "game/world.hpp"

#include <algorithm>

namespace gr
{

bool World::cheat(Cheat c)
{
  auto& p = mPlayer;
  if (p.state == PlayerState::Dying || p.state == PlayerState::Teleporting || mState != WorldState::Playing)
    return false;
  mStats.cheated = true;
  const Vec2 at{(float(p.x) + 1.5f) * kCellSize, (float(p.y) - 2.5f) * kCellSize};
  switch (c)
  {
    case Cheat::God:
      mGod = !mGod;
      showMessage(mGod ? "CHEAT: GOD MODE ON" : "CHEAT: GOD MODE OFF");
      break;
    case Cheat::Health:
      p.hp = p.maxHp;
      showMessage("CHEAT: FULL HEALTH");
      break;
    case Cheat::Ammo:
      if (mLevelProto >= 0)
      {
        const ProtoDef& def = protoDef(mLevelProto);
        p.weapon = Weapon::Proto;
        p.proto = mLevelProto;
        p.ammo = def.maxAmmo;
        p.charge = 0;
        p.shotCooldown = 0;
        showMessage(std::string("CHEAT: ") + def.name + " - FULL AMMO");
      }
      else
      {
        p.rapidFire = std::max(p.rapidFire, 700);
        showMessage("CHEAT: NO PROTOTYPE HERE - RAPID FIRE INSTEAD");
      }
      break;
    case Cheat::Turbo:
      startTurbo();
      break;
    case Cheat::Cure:
      p.virus = 0;
      showMessage("CHEAT: VIRUS CURED");
      break;
    case Cheat::Card:
      p.hasKey = true;
      showMessage("CHEAT: ACCESS CARD");
      break;
    case Cheat::Rapid:
      p.rapidFire = std::max(p.rapidFire, 700);
      showMessage("CHEAT: RAPID FIRE");
      break;
    case Cheat::Exit:
      p.state = PlayerState::Teleporting;
      setVisual(PlayerVisual::Standing);
      mState = WorldState::Exiting;
      mStateFrames = 0;
      playSound(Sfx::Teleport);
      showMessage("CHEAT: SKIPPING THE LEVEL");
      return true;
    case Cheat::Count:
      return false;
  }
  playSound(Sfx::Item);
  burst(at, rgb(255, 230, 60), rgb(255, 255, 255), 14, 1.6f);
  return true;
}

} // namespace gr
