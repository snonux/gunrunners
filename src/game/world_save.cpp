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
  s.god = mGod;
  s.cheated = st.cheated;
  for (const auto& pr : mProps)
    s.props.push_back(pr.used);

  for (std::size_t i = 0; i < mEnemies.size(); ++i)
  {
    const auto& e = mEnemies[i];
    if (i >= mLevelEnemyCount && !e.alive)
      continue; // a spawned enemy that is gone for good
    SaveGame::EnemyState es{e.alive, e.hp, e.x, e.y, e.dir, e.timer, e.active};
    es.attach = e.attach;
    // Mid-leap Hoppers and working Decouplers come back at rest.
    if (e.kind == EnemyKind::Hopper)
      es.attach = 0;
    // A Rappel Trooper still on its rope comes back landed; a Hover Biker
    // turning or revving comes back charging.
    if (e.kind == EnemyKind::Trooper && e.attach != 0)
    {
      es.attach = 0;
      es.y = e.aimY;
    }
    if (e.kind == EnemyKind::Biker)
      es.attach = 0;
    if (e.kind == EnemyKind::Decoupler && e.alive && e.attach != 0)
    {
      es.attach = 0;
      es.x = e.railX0;
      es.y = e.aimY;
    }
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
  for (const auto& f : mFluids)
    s.floods.push_back(f.floodAt);
  for (const auto& v : mValves)
    s.valves.push_back(v.locked);
  for (const auto& rp : mRatPipes)
  {
    // A burst in progress is saved as spent.
    s.ratPipes.push_back(rp.armed && rp.rattle == 0 && rp.left == 0);
    s.ratPipes.push_back(rp.idle);
  }
  for (const auto& b : mBubbles)
    if (b.life < 0 && b.enemy >= 0)
      s.bubbled.push_back(b.enemy);
  if (mTrain)
  {
    // A gantry sweeping now counts as gone by.
    const Platform* passer = mPasser >= 0 ? &mPlatforms[std::size_t(mPasser)] : nullptr;
    s.train = {mBrakeAt, mTunnelState, mGapDone, mGapUntil, mTunnelNext, mTunnelMouthX, mTunnelExitX,
      passer && passer->hidden, passer && passer->running};
    for (std::size_t i = 0; i < mLevelGantries; ++i)
      s.train.push_back(mGantries[i].fired);
  }
  if (mHunter.on || mBoss.on || mFlight || !mLatches.empty() || !mRappels.empty())
  {
    // A gunship going down counts as down.
    const auto& b = mBoss;
    const bool falling = b.phase == BossPhase::Falling;
    s.chopper = {mHunter.demoDone, mHunter.cool, falling ? int(BossPhase::Done) : int(b.phase)};
    for (int hp : b.hp)
      s.chopper.push_back(hp);
    s.chopper.push_back(falling ? 0 : b.exitT);
    for (int v : {mPops, mTrucks, mGemScore, mPopNext, mTruckNext})
      s.chopper.push_back(v);
    for (const auto& l : mLatches)
      s.chopper.push_back(l.open);
    for (const auto& z : mRappels)
      s.chopper.push_back(z.next);
  }
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
      (!s.floods.empty() && s.floods.size() != mFluids.size()) ||
      (!s.valves.empty() && s.valves.size() != mValves.size()) ||
      (!s.ratPipes.empty() && s.ratPipes.size() != mRatPipes.size() * 2) ||
      (!s.train.empty() && s.train.size() != 9 + mLevelGantries) ||
      (!s.chopper.empty() && s.chopper.size() != kChopperSave + mLatches.size() + mRappels.size()) ||
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
  st.cheated = s.cheated;
  mGod = s.god;
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
    if (e.kind == EnemyKind::Stalker || e.kind == EnemyKind::Keeper)
      e.attach = se.attach; // frozen in light; the Keeper's errand
    if (e.kind == EnemyKind::Hopper)
      e.attach = 0; // lands where it was
    if (e.kind == EnemyKind::Decoupler)
      e.attach = se.attach; // asleep in its coupling, or gone with the rear cars
    e.stun = 0;
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
  // The tide: floods, padlocks, rat pipes; only lasting bubbles come back.
  for (std::size_t i = 0; i < s.floods.size(); ++i)
    mFluids[i].floodAt = s.floods[i];
  for (std::size_t i = 0; i < s.valves.size(); ++i)
    mValves[i].locked = s.valves[i] != 0;
  for (std::size_t i = 0; i < mRatPipes.size() && i * 2 + 1 < s.ratPipes.size(); ++i)
  {
    mRatPipes[i].armed = s.ratPipes[i * 2] != 0;
    mRatPipes[i].idle = s.ratPipes[i * 2 + 1];
  }
  for (std::size_t i = 0; i < mBubbles.size();)
  {
    const auto& b = mBubbles[i];
    const bool kept = b.enemy < 0 || std::find(s.bubbled.begin(), s.bubbled.end(), b.enemy) != s.bubbled.end();
    if (!kept)
    {
      mEnemies[std::size_t(b.enemy)].trapped = false;
      mBubbles.erase(mBubbles.begin() + std::ptrdiff_t(i));
      continue;
    }
    ++i;
  }
  if (s.train.size() == 9 + mLevelGantries)
  {
    mBrakeAt = s.train[0];
    mTunnelState = s.train[1];
    mGapDone = s.train[2] != 0;
    mGapUntil = s.train[3];
    mTunnelNext = s.train[4];
    mTunnelMouthX = s.train[5];
    mTunnelExitX = s.train[6];
    if (mPasser >= 0)
    {
      mPlatforms[std::size_t(mPasser)].hidden = s.train[7] != 0;
      mPlatforms[std::size_t(mPasser)].running = s.train[8] != 0;
    }
    for (std::size_t i = 0; i < mLevelGantries; ++i)
      mGantries[i].fired = s.train[9 + i] != 0;
    if (mBrakeAt >= 0 && mStats.frames - mBrakeAt >= 90 && mStationLayer >= 0)
      mLayers[std::size_t(mStationLayer)].scriptSolid = true;
    syncPlatformCollision();
  }
  if (!s.chopper.empty())
  {
    const auto& c = s.chopper;
    const int phase = c[2];
    if (phase < int(BossPhase::Waiting) || phase > int(BossPhase::Done))
      return false;
    auto& b = mBoss;
    b.phase = BossPhase(phase);
    for (std::size_t i = 0; i < b.hp.size(); ++i)
      b.hp[i] = std::max(0, c[3 + i]);
    b.exitT = c[8];
    mPops = c[9];
    mTrucks = c[10];
    mGemScore = c[11];
    mPopNext = c[12];
    mTruckNext = c[13];
    for (std::size_t i = 0; i < mLatches.size(); ++i)
      mLatches[i].open = c[kChopperSave + i] != 0;
    for (std::size_t i = 0; i < mRappels.size(); ++i)
      mRappels[i].next = c[kChopperSave + mLatches.size() + i];
    // The gunship comes back hovering at the start of its cycle.
    b.x = b.prevX = b.arena.right() - Boss::kW - 2;
    b.y = b.prevY = 6;
    b.fallT = 0;
    resetBossCycle();
    mHunter.demoDone = c[0] != 0;
    mHunter.cool = std::max(0, c[1]);
    if (b.on && b.phase != BossPhase::Waiting && b.phase != BossPhase::Done)
      mMusicOverride = "boss_black_halo";
  }
  mStrikes.clear();
  mPaint.clear();
  mRadio = 0;
  for (auto& f : mFluids)
    f.surface = fluidSurface(f);
  mSludgeTicks = 0;
  mDiving = false;
  mBump = 0;
  syncFloats();
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
