#include "game/world.hpp"

#include <algorithm>

namespace gr
{

bool World::canSave() const
{
  return mState == WorldState::Playing && mPlayer.state != PlayerState::Dying &&
    mPlayer.state != PlayerState::Teleporting && mPlayer.cart < 0 && mPlayer.tube < 0 && !mPinball && !mSurfing &&
    (!mGolem.on || mGolem.phase == GolemPhase::Seated || mGolem.phase == GolemPhase::Done) &&
    (!mStation.on() || stationCanSave());
}

SaveGame World::snapshot() const
{
  SaveGame s;
  s.levelName = mLevel->name;
  s.character = mCharacterIndex;
  if (mCharacter.custom)
    s.runner = encodeRunner(mCharacter);

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
    // A Loader Mech's legs (it holds nothing: saving waits for the throw).
    if (e.kind == EnemyKind::Loader)
      es.attach = e.ox;
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
    // A Viper hanging or catching its breath comes back coiled on its branch.
    if (e.kind == EnemyKind::Viper)
    {
      es.attach = 0;
      es.y = e.aimY;
    }
    if (e.kind == EnemyKind::Decoupler && e.alive && e.attach != 0)
    {
      es.attach = 0;
      es.x = e.railX0;
      es.y = e.aimY;
    }
    // A Rock Mole out of the rock comes back burrowed at home.
    if (e.kind == EnemyKind::Mole)
    {
      es.attach = 0;
      es.x = e.railX0;
      es.y = e.railX1;
    }
    // A Magma Toad comes back under the lava at home, an Ember Wisp
    // drifting, a Basalt Crab right way up.
    if (e.kind == EnemyKind::Toad)
    {
      es.attach = 0;
      es.x = e.railX0;
      es.y = e.railX1 + 2;
    }
    if (e.kind == EnemyKind::Wisp || e.kind == EnemyKind::Crab)
      es.attach = 0;
    // Episode 7's aliens come back on their feet (not mid-leap or on a wall).
    if (e.kind == EnemyKind::Skitter || e.kind == EnemyKind::Spitpod || e.kind == EnemyKind::Gloop ||
        e.kind == EnemyKind::Polyp)
      es.attach = 0;
    // A Spear Runner about to throw comes back fleeing; a Pit Snake back
    // coiled in its hole.
    if (e.kind == EnemyKind::SpearRunner && e.attach == 2)
      es.attach = 1;
    if (e.kind == EnemyKind::PitSnake)
    {
      es.attach = 0;
      es.y = e.aimY + 1;
    }
    if (i >= mLevelEnemyCount)
    {
      es.def = e.def;
      es.platform = e.platform;
    }
    s.enemies.push_back(es);
  }
  for (const auto& pl : mPlatforms)
  {
    s.platforms.push_back(
      {pl.x, pl.y, pl.balance, pl.slackLeft, pl.idle, pl.moveTick, pl.target, pl.step, pl.braked});
    // Level 12: a ferry's crossing, and when the bridge began to rise (it
    // comes back risen).
    auto& sp = s.platforms.back();
    if (pl.mode == PlatformMode::Sink)
    {
      sp.balance = pl.ferry;
      sp.slackLeft = pl.ferryWait;
      sp.idle = pl.parked ? 1 : 0;
    }
    if (pl.mode == PlatformMode::Rise)
    {
      sp.balance = pl.riseAt < 0 ? -1 : 0;
      if (pl.riseAt >= 0)
        sp.y = pl.homeY;
    }
  }
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
  if (!mBridges.empty() || !mJRopes.empty())
  {
    // A bridge that is creaking counts as down; a load on its way down as landed.
    for (const auto& b : mBridges)
      s.jungle.push_back(b.down || b.creak > 0);
    for (const auto& rope : mJRopes)
    {
      s.jungle.push_back(rope.cut ? 0 : rope.hp);
      const int state = rope.load >= 0 ? mLoads[std::size_t(rope.load)].state : 0;
      s.jungle.push_back(state == 1 ? (mLoads[std::size_t(rope.load)].cage ? 3 : 2) : state);
    }
  }
  if (!mStoneKeys.empty() || !mKeyDoors.empty() || !mSecretDoors.empty() || !mPlates.empty())
  {
    // A key door already sinking counts as open.
    for (const auto& k : mStoneKeys)
      s.temple.push_back(k.taken);
    for (const auto& d : mKeyDoors)
      s.temple.push_back(d.open || d.sink >= 0);
    for (const auto& d : mSecretDoors)
      s.temple.push_back(d.open);
    for (const auto& pl : mPlates)
      s.temple.push_back(std::min(pl.presses, 99));
  }
  if (!mMirrors.empty() || !mSunDoors.empty())
  {
    // A mirror mid-turn is saved where it is going.
    for (const auto& m : mMirrors)
      s.light.push_back(m.to);
    for (const auto& d : mSunDoors)
      s.light.push_back(d.open);
  }
  if (!mLevers.empty() || !mTrapdoors.empty() || !mRubble.empty() || mDaysSign.w > 0)
  {
    for (const auto& l : mLevers)
      s.mine.push_back(l.state);
    for (const auto& t : mTrapdoors)
      s.mine.push_back(t.open);
    for (const auto& rb : mRubble)
      s.mine.push_back(rb[3]);
    s.mine.push_back(mDays);
  }
  if (mGreedOn || mGolem.on || !mCoinHeaps.empty())
  {
    s.sanctum = {mGreed, mOffered, mRefillUsed, mGolem.on && mGolem.phase == GolemPhase::Done};
    for (const auto& a : mAltars)
      for (int v : {int(a.open), int(a.dropped), a.offered})
        s.sanctum.push_back(v);
    for (const auto& h : mCoinHeaps)
      s.sanctum.push_back(h.waves);
  }
  if (mStation.on())
  {
    s.station = {mStation.setFired};
    for (const auto& pn : mStation.panels)
      for (int v : {pn.cool, int(pn.dented)})
        s.station.push_back(v);
    for (const auto& c : mStation.crates)
      for (int v : {int(c.alive), c.bx, c.by})
        s.station.push_back(v);
  }
  if (!mBoulders.empty() || !mCracks.empty())
  {
    // A boulder after the runner comes back up its hole (as on a respawn);
    // the demo boulder rolling, or one in its chute, counts as gone.
    for (const auto& b : mBoulders)
    {
      const bool gone = b.state == Boulder::Gone || b.state == Boulder::Chute || (b.wake >= 0 && b.state != Boulder::Wait);
      s.boulder.push_back(gone ? 1 : 0);
      s.boulder.push_back(b.halted);
      s.boulder.push_back(b.teeter >= 0);
    }
    for (const auto& c : mCracks)
      s.boulder.push_back(c.open);
    s.boulder.push_back(mWrongWay);
  }
  s.explored = exploredRuns();
  if (!mVehicles.empty() || !mSeas.empty())
  {
    for (const auto& v : mVehicles)
    {
      const int fields[] = {v.x, v.y, v.facing, v.hp, v.fuel, v.homeX, v.homeY, v.homeFacing, v.wreck > 0 ? 1 : 0};
      s.vehicles.insert(s.vehicles.end(), std::begin(fields), std::end(fields));
    }
    s.vehicles.push_back(p.vehicle);
    s.vehicles.push_back(mAir);
  }
  return s;
}

bool World::switchCharacter(int index)
{
  if (!canSave() || index < 0 || index >= characterCount() || index == mCharacterIndex)
    return false;
  auto& p = mPlayer;
  const auto& next = characterByIndex(index);
  // Same fraction of hearts, rounded up so a switch never kills you.
  p.hp = std::max(1, (p.hp * next.maxHp + p.maxHp - 1) / p.maxHp);
  p.maxHp = next.maxHp;
  mCharacter = next;
  mCharacterIndex = index;

  const Vec2 c{(float(p.x) + 1.5f) * kCellSize, (float(p.y) - 2.0f) * kCellSize};
  burst(c, mArt.runnerColor(next), rgb(255, 255, 255), 24, 2.0f);
  flashAt(c, 90.0f, mArt.runnerColor(next), 16);
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
      (!s.jungle.empty() && s.jungle.size() != mBridges.size() + mJRopes.size() * 2) ||
      (!s.temple.empty() &&
        s.temple.size() != mStoneKeys.size() + mKeyDoors.size() + mSecretDoors.size() + mPlates.size()) ||
      (!s.light.empty() && s.light.size() != mMirrors.size() + mSunDoors.size()) ||
      (!s.mine.empty() && s.mine.size() != mLevers.size() + mTrapdoors.size() + mRubble.size() + 1) ||
      (!s.boulder.empty() && s.boulder.size() != mBoulders.size() * 3 + mCracks.size() + 1) ||
      (!s.sanctum.empty() && s.sanctum.size() != 4 + mAltars.size() * 3 + mCoinHeaps.size()) ||
      (!s.station.empty() && s.station.size() != 1 + mStation.panels.size() * 2 + mStation.crates.size() * 3) ||
      (!s.vehicles.empty() && s.vehicles.size() != mVehicles.size() * 9 + 2) ||
      s.boxes.size() != mBoxes.size() || s.checkpoints.size() != mCheckpoints.size() ||
      s.weapon < 0 || s.weapon > int(Weapon::Proto) || (!s.props.empty() && s.props.size() != mProps.size()))
    return false;
  for (const auto& it : s.items)
    if (it.kind < 0 || it.kind > int(ItemKind::Duck))
      return false;
  for (std::size_t i = mLevelEnemyCount; i < s.enemies.size(); ++i)
    if (s.enemies[i].def < 0 || s.enemies[i].def >= enemyDefCount())
      return false;

  mCharacterIndex = resolveSavedRunner(s.character, s.runner);
  mCharacter = characterByIndex(mCharacterIndex);
  auto& p = mPlayer;
  p = Player{};
  p.x = p.prevX = mSafeX = s.x;
  p.y = p.prevY = mSafeY = s.y;
  p.facing = s.facing < 0 ? -1 : 1;
  p.maxHp = mCharacter.maxHp;
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
    if (e.kind == EnemyKind::Toad)
    {
      e.attach = 0;
      e.hidden = true;
      e.platform = -1;
    }
    if (e.kind == EnemyKind::Wisp || e.kind == EnemyKind::Crab)
      e.attach = 0;
    if (e.kind == EnemyKind::SpearRunner)
      e.attach = se.attach; // asleep, fleeing, or past its throw
    if (e.kind == EnemyKind::CoinBeetle || e.kind == EnemyKind::Sentinel)
      e.attach = se.attach; // crawling, rattling or hopping; its glyph row
    if (e.kind == EnemyKind::Loader)
    {
      e.ox = se.attach; // its legs
      e.aimX = -1;
    }
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
    pl.bell = 0;
    pl.parked = pl.waitRider && !pl.path.empty() &&
      (std::make_pair(pl.x, pl.y) == pl.path.front() || std::make_pair(pl.x, pl.y) == pl.path.back());
    if (pl.mode == PlatformMode::Sink)
    {
      pl.acc = 0;
      pl.ferry = pl.ferry > 0 ? std::clamp(sp.balance, 1, 3) : 0;
      pl.ferryWait = sp.slackLeft;
      pl.parked = sp.idle != 0;
      pl.balance = pl.slackLeft = pl.idle = 0;
    }
    if (pl.mode == PlatformMode::Rise)
    {
      pl.riseAt = sp.balance < 0 ? -1 : 0;
      pl.balance = 0;
    }
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
  if (!s.jungle.empty())
  {
    for (std::size_t i = 0; i < mBridges.size(); ++i)
      if (s.jungle[i] != 0)
      {
        auto& b = mBridges[i];
        b.down = true;
        for (int tx = b.x0; tx <= b.x1; ++tx)
          mMap.setBlock(tx, b.y, Tile::Empty);
      }
    for (std::size_t i = 0; i < mJRopes.size(); ++i)
    {
      auto& rope = mJRopes[i];
      const int hp = s.jungle[mBridges.size() + i * 2], state = s.jungle[mBridges.size() + i * 2 + 1];
      rope.hp = std::max(0, hp);
      rope.cut = hp <= 0;
      if (rope.load < 0 || state < 2)
        continue;
      Load& l = mLoads[std::size_t(rope.load)];
      l.state = l.cage ? 3 : 2; // a cage's gems are with the items
      l.fall = 10;
      if (!l.cage)
        for (int tx = l.landX0; tx <= l.landX1; ++tx)
          mMap.setBlock(tx, l.landRow, Tile::Platform);
    }
  }
  if (!s.temple.empty())
  {
    std::size_t at = 0;
    for (auto& k : mStoneKeys)
      k.taken = s.temple[at++] != 0;
    for (auto& d : mKeyDoors)
      if (s.temple[at++] != 0)
      {
        d.open = true;
        for (int ty = d.ty; ty < d.ty + d.h; ++ty)
          mMap.setBlock(d.tx, ty, Tile::Empty);
      }
    for (auto& d : mSecretDoors)
      if (s.temple[at++] != 0)
      {
        d.open = true;
        if (!d.bonus)
          for (int ty = d.y0; ty <= d.y1; ++ty)
            for (int tx = d.x0; tx <= d.x1; ++tx)
              mMap.setBlock(tx, ty, Tile::Empty);
      }
    for (auto& pl : mPlates)
      pl.presses = std::max(0, s.temple[at++]);
  }
  if (!s.light.empty())
  {
    std::size_t at = 0;
    for (auto& m : mMirrors)
    {
      m.angle = m.to = ((s.light[at++] % 8) + 8) % 8;
      m.turn = 0;
    }
    for (auto& d : mSunDoors)
    {
      const bool open = s.light[at++] != 0;
      if (open && d.opens >= 0)
        d.open = true; // the stone sun: its hatch has its own entry
      else if (open != d.open)
        setSunDoor(d, open);
      d.lit = 0;
    }
  }
  mBeamPaths.clear();
  mReflects.clear();
  mLanceOn = false;
  if (!s.mine.empty())
  {
    std::size_t at = 0;
    for (auto& l : mLevers)
      l.state = std::clamp(s.mine[at++], 0, l.states - 1);
    for (auto& t : mTrapdoors)
      if (s.mine[at++] != 0 && !t.open)
      {
        t.open = true;
        for (int tx = t.x0; tx <= t.x1; ++tx)
          mMap.setBlock(tx, t.y, Tile::Empty);
      }
    for (auto& rb : mRubble)
      if (s.mine[at++] != 0 && !rb[3])
      {
        rb[3] = 1;
        mMap.setBlock(rb[1], rb[2], Tile::Solid);
      }
    mDays = std::max(0, s.mine[at++]);
  }
  resetMine();
  if (!s.boulder.empty())
  {
    std::size_t at = 0;
    for (auto& b : mBoulders)
    {
      const bool gone = s.boulder[at++] != 0;
      b.state = gone ? Boulder::Gone : Boulder::Wait;
      b.x = b.prevX = b.homeX;
      b.y = b.prevY = b.homeY;
      b.acc = b.vy = b.timer = b.shelter = 0;
      b.angle = b.prevAngle = 0.0f;
      b.halted = s.boulder[at++] != 0;
      if (s.boulder[at++] == 0)
        b.teeter = -1;
    }
    for (auto& c : mCracks)
      if (s.boulder[at++] != 0 && !c.open)
      {
        c.open = true;
        for (int ty = c.by0; ty <= c.by1; ++ty)
          for (int tx = c.bx0; tx <= c.bx1; ++tx)
            mMap.setBlock(tx, ty, Tile::Empty);
      }
    mWrongWay = s.boulder[at++] != 0;
  }
  for (auto& c : mChutes)
  {
    c.shut = 0;
    setChute(c, false);
  }
  if (!s.station.empty())
  {
    auto& st = mStation;
    std::size_t at = 0;
    st.setFired = s.station[at++] != 0;
    if (st.setFired)
      for (int i : st.sleepers)
        mEnemies[std::size_t(i)].hidden = false;
    for (auto& pn : st.panels)
    {
      pn.cool = s.station[at++];
      pn.dented = s.station[at++] != 0;
      pn.open = 0;
    }
    for (auto& c : st.crates)
      if (c.alive && !c.loose)
        mMap.setBlock(c.bx, c.by, Tile::Empty);
    for (auto& c : st.crates)
    {
      c.alive = s.station[at++] != 0;
      c.bx = s.station[at++];
      c.by = s.station[at++];
      c.loose = c.thrown = c.held = false;
      if (c.alive)
        mMap.setBlock(c.bx, c.by, Tile::Solid);
    }
    st.charges.clear();
    st.debris.clear();
    st.seams.clear();
    st.caught = -1;
    st.detonate = 0;
  }
  if (!s.sanctum.empty())
  {
    std::size_t at = 0;
    mGreed = s.sanctum[at++];
    mOffered = s.sanctum[at++];
    mRefillUsed = s.sanctum[at++] != 0;
    const bool down = s.sanctum[at++] != 0;
    for (auto& a : mAltars)
    {
      a.open = s.sanctum[at++] != 0;
      const bool dropped = s.sanctum[at++] != 0;
      a.offered = s.sanctum[at++];
      a.stand = a.flare = 0;
      if (dropped && !a.dropped)
      {
        a.dropped = true;
        for (int ty = a.ty0; ty <= a.ty1 && a.tx1 >= a.tx0; ++ty)
          for (int tx = a.tx0; tx <= a.tx1; ++tx)
            mMap.setBlock(tx, ty, Tile::Empty);
        mMap.setBlock(a.bx, a.by, Tile::Empty);
        mMap.setBlock(a.bx + 1, a.by, Tile::Empty);
      }
      // An empty-handed offering woke the bonus entrance.
      if (!a.offer && a.open && !a.dropped)
        for (auto& pr : mProps)
          if (pr.kind == PropKind::BonusDoor)
            pr.dormant = false;
    }
    for (auto& h : mCoinHeaps)
    {
      h.waves = s.sanctum[at++];
      h.cool = 0;
    }
    for (auto& g : mGlyphRows)
      g.t = g.cool = 0;
    if (down)
    {
      mGolem.phase = GolemPhase::Done;
      mGolem.lid = true;
      if (mGolem.camera >= 0)
        mEnemies[std::size_t(mGolem.camera)].hidden = true;
    }
  }
  mStill = 0;
  syncTotems();
  // Scarab Tides: the carpet is as wide as the beetles left.
  for (auto& e : mEnemies)
    if (e.kind == EnemyKind::Scarabs)
      e.w = std::max(2, e.hp + e.hp / 3);
  mFruits.clear();
  for (auto& b : mBridges)
    b.drop = 8;
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
  restoreExplored(s.explored);
  if (!s.vehicles.empty())
  {
    std::size_t at = 0;
    for (auto& v : mVehicles)
    {
      v.x = v.prevX = s.vehicles[at++];
      v.y = v.prevY = s.vehicles[at++];
      v.facing = s.vehicles[at++] < 0 ? -1 : 1;
      v.hp = std::clamp(s.vehicles[at++], 1, vehicleDef(v.kind).hp);
      v.fuel = std::clamp(s.vehicles[at++], 0, vehicleDef(v.kind).fuel);
      v.homeX = s.vehicles[at++];
      v.homeY = s.vehicles[at++];
      v.homeFacing = s.vehicles[at++] < 0 ? -1 : 1;
      v.wreck = s.vehicles[at++] != 0 ? 1 : 0; // a wreck is back home on the next frame
      v.occupied = false;
      v.vx = v.vy = v.ax = v.ay = 0;
      v.air = -1;
    }
    const int ride = s.vehicles[at++];
    mAir = std::clamp(s.vehicles[at++], 0, kAirFrames);
    if (ride >= 0 && ride < int(mVehicles.size()) && mVehicles[std::size_t(ride)].wreck == 0)
    {
      boardVehicle(ride);
      mTexts.clear();
      mParticles.clear();
    }
    syncPlatformCollision();
  }
  mCamera.centerOn(cameraTarget(), mMap.width(), mMap.height());
  return true;
}

} // namespace gr
