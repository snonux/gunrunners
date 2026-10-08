#include "game/world.hpp"

#include <algorithm>

namespace gr
{

bool World::canSave() const
{
  return mState == WorldState::Playing && mPlayer.state != PlayerState::Dying &&
    mPlayer.state != PlayerState::Teleporting;
}

SaveGame World::snapshot() const
{
  SaveGame s;
  s.levelName = mLevel.name;
  s.character = mCharacterIndex;

  const auto& p = mPlayer;
  s.x = mSafeX;
  s.y = mSafeY;
  s.facing = p.facing;
  s.hp = p.hp;
  s.weapon = int(p.weapon);
  s.ammo = p.ammo;
  s.rapidFire = p.rapidFire;
  s.hasKey = p.hasKey;
  s.respawnX = mRespawnX;
  s.respawnY = mRespawnY;

  const auto& st = mStats;
  s.score = st.score;
  s.gems = st.gems;
  s.kills = st.kills;
  s.merch = st.merch;
  s.weaponsCollected = st.weaponsCollected;
  s.deaths = st.deaths;
  s.frames = st.frames;
  s.tookDamage = st.tookDamage;
  s.letters = st.letters;
  s.forceFieldsOn = mMap.forceFieldsOn();

  for (const auto& e : mEnemies)
    s.enemies.push_back({e.alive, e.hp, e.x, e.y, e.dir, e.timer, e.active});
  for (const auto& b : mBoxes)
    s.boxes.push_back(b.alive);
  // Items still waiting to be picked up, including ones that already popped
  // out of a box.
  for (const auto& it : mItems)
    if (!it.taken)
      s.items.push_back({int(it.kind), it.variant, it.x, it.y, it.floating});
  for (const auto& c : mCheckpoints)
    s.checkpoints.push_back(c.active);
  return s;
}

bool World::restore(const SaveGame& s)
{
  if (s.levelName != mLevel.name || s.character != mCharacterIndex || s.enemies.size() != mEnemies.size() ||
      s.boxes.size() != mBoxes.size() || s.checkpoints.size() != mCheckpoints.size() ||
      s.weapon < 0 || s.weapon > int(Weapon::Flame))
    return false;
  for (const auto& it : s.items)
    if (it.kind < 0 || it.kind > int(ItemKind::LetterN))
      return false;

  auto& p = mPlayer;
  p = Player{};
  p.x = p.prevX = mSafeX = s.x;
  p.y = p.prevY = mSafeY = s.y;
  p.facing = s.facing < 0 ? -1 : 1;
  p.maxHp = mCharacter->maxHp;
  p.hp = std::min(std::max(1, s.hp), p.maxHp);
  p.weapon = Weapon(s.weapon);
  p.ammo = s.ammo;
  p.rapidFire = s.rapidFire;
  p.hasKey = s.hasKey;
  p.mercy = 20; // a moment to get your bearings
  if (!mMap.onSolidGround(p.box()))
  {
    p.state = PlayerState::Falling;
    p.visual = PlayerVisual::Falling;
  }
  mRespawnX = s.respawnX;
  mRespawnY = s.respawnY;

  auto& st = mStats;
  st.score = s.score;
  st.gems = s.gems;
  st.kills = s.kills;
  st.merch = s.merch;
  st.weaponsCollected = s.weaponsCollected;
  st.deaths = s.deaths;
  st.frames = s.frames;
  st.tookDamage = s.tookDamage;
  st.letters = s.letters;
  if (!s.forceFieldsOn)
    mMap.disableForceFields();

  for (std::size_t i = 0; i < mEnemies.size(); ++i)
  {
    auto& e = mEnemies[i];
    const auto& se = s.enemies[i];
    e.alive = se.alive;
    e.hp = se.hp;
    e.x = e.prevX = se.x;
    e.y = e.prevY = se.y;
    e.dir = se.dir < 0 ? -1 : 1;
    e.timer = se.timer;
    e.active = se.active;
    e.dive = 0;
    e.flash = 0;
    e.drawSnap = true;
  }
  for (std::size_t i = 0; i < mBoxes.size(); ++i)
    mBoxes[i].alive = s.boxes[i];
  mItems.clear();
  for (const auto& si : s.items)
  {
    Item it;
    it.kind = ItemKind(si.kind);
    it.variant = si.variant;
    it.x = it.prevX = si.x;
    it.y = it.prevY = si.y;
    it.floating = si.floating;
    mItems.push_back(it);
  }
  for (std::size_t i = 0; i < mCheckpoints.size(); ++i)
    mCheckpoints[i].active = s.checkpoints[i];

  mProjectiles.clear();
  mParticles.clear();
  mTexts.clear();
  mFlashes.clear();
  mState = WorldState::Playing;
  mStateFrames = 0;
  mCamera.centerOn(cameraTarget(), mMap.width(), mMap.height());
  return true;
}

} // namespace gr
