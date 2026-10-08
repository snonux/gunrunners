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
  s.levelName = mLevel->name;
  s.character = mCharacterIndex;

  const auto& p = mPlayer;
  s.x = mSafeX;
  s.y = mSafeY;
  s.facing = p.facing;
  s.hp = p.hp;
  s.weapon = int(p.weapon);
  s.ammo = p.ammo;
  s.rapidFire = p.rapidFire;
  s.turbo = p.turbo;
  s.virus = p.virus;
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
  s.proto = p.weapon == Weapon::Proto ? p.proto : -1;
  s.protoFound = st.protoFound;
  s.duck = st.duck;
  s.camera = st.camera;
  s.bonusStar = mBonusStar;
  s.protoKills = st.protoKills;
  for (const auto& pr : mProps)
    s.props.push_back(pr.used);

  for (std::size_t i = 0; i < mEnemies.size(); ++i)
  {
    const auto& e = mEnemies[i];
    if (i >= mLevelEnemyCount && !e.alive)
      continue; // a spawned enemy that is gone for good
    SaveGame::EnemyState es{e.alive, e.hp, e.x, e.y, e.dir, e.timer, e.active};
    es.attach = e.attach;
    if (i >= mLevelEnemyCount)
    {
      es.def = e.def;
      es.platform = e.platform;
    }
    s.enemies.push_back(es);
  }
  for (const auto& pl : mPlatforms)
    s.platforms.push_back(
      {pl.x, pl.y, pl.balance, pl.slackLeft, pl.idle, pl.moveTick, pl.target, pl.step, pl.braked});
  for (const auto& h : mHatches)
    s.hatches.push_back(h.open);
  for (const auto& b : mBreakables)
    s.breakables.push_back(b.broken ? 0 : b.hp);
  for (const auto& b : mBoxes)
    s.boxes.push_back(b.alive);
  // Items still waiting to be picked up, including ones that already popped
  // out of a box.
  for (const auto& it : mItems)
    if (!it.taken)
      s.items.push_back({int(it.kind), it.variant, it.x, it.y, it.floating});
  for (const auto& c : mCheckpoints)
    s.checkpoints.push_back(c.active);
  for (const auto& b : mBreakers)
    s.breakers.push_back({b.on || b.throwing > 0, b.leechKilled, b.throwing > 0 ? mStats.frames : b.thrownAt, b.leechIn});
  for (const auto& d : mDoors)
    s.doors.push_back(d.solid ? d.open : -1);
  s.allLitAt = mAllLitAt;
  return s;
}

bool World::switchCharacter(int index)
{
  if (!canSave() || index < 0 || index >= kCharacterCount || index == mCharacterIndex)
    return false;
  auto& p = mPlayer;
  const auto& next = characterByIndex(index);
  // Same fraction of hearts, rounded up so a switch never kills you.
  p.hp = std::max(1, (p.hp * next.maxHp + p.maxHp - 1) / p.maxHp);
  p.maxHp = next.maxHp;
  mCharacter = &next;
  mCharacterIndex = index;

  const Vec2 c{(float(p.x) + 1.5f) * kCellSize, (float(p.y) - 2.0f) * kCellSize};
  burst(c, mArt.characterColor[std::size_t(index)], rgb(255, 255, 255), 24, 2.0f);
  flashAt(c, 90.0f, mArt.characterColor[std::size_t(index)], 16);
  playSound(Sfx::Teleport);
  showMessage(std::string(next.name) + " STEPS IN");
  return true;
}

bool World::restore(const SaveGame& s)
{
  if (s.levelName != mLevel->name || s.enemies.size() < mLevelEnemyCount ||
      (!s.platforms.empty() && s.platforms.size() != mPlatforms.size()) ||
      (!s.hatches.empty() && s.hatches.size() != mHatches.size()) ||
      (!s.breakables.empty() && s.breakables.size() != mBreakables.size()) ||
      (!s.breakers.empty() && s.breakers.size() != mBreakers.size()) ||
      (!s.doors.empty() && s.doors.size() != mDoors.size()) ||
      s.boxes.size() != mBoxes.size() || s.checkpoints.size() != mCheckpoints.size() ||
      s.weapon < 0 || s.weapon > int(Weapon::Proto) || (!s.props.empty() && s.props.size() != mProps.size()))
    return false;
  for (const auto& it : s.items)
    if (it.kind < 0 || it.kind > int(ItemKind::Duck))
      return false;
  for (std::size_t i = mLevelEnemyCount; i < s.enemies.size(); ++i)
    if (s.enemies[i].def < 0 || s.enemies[i].def >= enemyDefCount())
      return false;

  mCharacter = &characterByIndex(std::clamp(s.character, 0, kCharacterCount - 1));
  mCharacterIndex = std::clamp(s.character, 0, kCharacterCount - 1);
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
  p.turbo = std::max(0, s.turbo);
  p.virus = std::max(0, s.virus);
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
  if (p.weapon == Weapon::Proto)
  {
    p.proto = s.proto >= 0 ? s.proto : mLevelProto;
    if (p.proto < 0)
      p.weapon = Weapon::Normal;
  }
  st.protoFound = s.protoFound;
  st.duck = s.duck;
  st.camera = s.camera;
  st.protoKills = s.protoKills;
  mBonusStar = s.bonusStar;
  for (std::size_t i = 0; i < s.props.size(); ++i)
    mProps[i].used = s.props[i];

  mEnemies.resize(mLevelEnemyCount);
  for (std::size_t i = mLevelEnemyCount; i < s.enemies.size(); ++i)
  {
    const auto& se = s.enemies[i];
    spawnEnemy(se.def, se.x, se.y);
    mEnemies.back().platform = se.platform;
  }
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
    e.tell = 0;
    e.flash = 0;
    if (e.kind == EnemyKind::Leech)
    {
      e.attach = se.attach;
      e.aimY = 0; // finds its place on the cable again
    }
    if (e.kind == EnemyKind::Stalker)
      e.attach = se.attach; // frozen in light
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
  for (std::size_t i = 0; i < s.platforms.size(); ++i)
  {
    auto& pl = mPlatforms[i];
    const auto& sp = s.platforms[i];
    pl.x = pl.prevX = sp.x;
    pl.y = pl.prevY = sp.y;
    pl.balance = sp.balance;
    pl.slackLeft = sp.slackLeft;
    pl.idle = sp.idle;
    pl.moveTick = sp.moveTick;
    pl.target = pl.path.empty() ? sp.target : std::clamp(sp.target, 0, int(pl.path.size()) - 1);
    pl.step = sp.step < 0 ? -1 : 1;
    pl.braked = sp.braked;
    pl.shudder = 0;
  }
  syncPlatformCollision();
  for (std::size_t i = 0; i < s.hatches.size(); ++i)
    if (s.hatches[i] && !mHatches[i].open)
    {
      mHatches[i].open = true;
      mMap.setBlock(mHatches[i].tx, mHatches[i].ty, mHatches[i].tile);
    }
  for (std::size_t i = 0; i < s.breakables.size(); ++i)
  {
    auto& b = mBreakables[i];
    b.hp = s.breakables[i];
    if (b.hp <= 0 && !b.broken)
    {
      b.broken = true;
      for (int ty = b.y0; ty <= b.y1; ++ty)
        for (int tx = b.x0; tx <= b.x1; ++tx)
          mMap.setBlock(tx, ty, Tile::Empty);
    }
  }

  for (std::size_t i = 0; i < s.breakers.size(); ++i)
  {
    auto& b = mBreakers[i];
    b.on = s.breakers[i].on;
    b.thrownAt = s.breakers[i].thrownAt;
    b.leechKilled = s.breakers[i].leechKilled;
    b.leechIn = s.breakers[i].leechIn;
    b.throwing = 0;
  }
  for (std::size_t i = 0; i < s.doors.size(); ++i)
  {
    auto& d = mDoors[i];
    d.open = std::max(0, s.doors[i]);
    applyDoor(d, s.doors[i] >= 0);
  }
  mAllLitAt = s.allLitAt;
  mFlares.clear();
  mPings.clear();
  for (auto& it : mItems)
    it.heldBy = -1;

  mProjectiles.clear();
  mPuddles.clear();
  mCones.clear();
  mLaunch = mLaunchBump = 0;
  mBreakdance = false;
  mQueued = mQueuedJump = false;
  mQueuedDx = mBeatDx = mBeatMove = mBeatJump = 0;
  mParticles.clear();
  mTexts.clear();
  mFlashes.clear();
  mState = WorldState::Playing;
  mStateFrames = 0;
  updateLayers(false); // the beat signs follow the restored music clock
  mCamera.centerOn(cameraTarget(), mMap.width(), mMap.height());
  return true;
}

} // namespace gr
