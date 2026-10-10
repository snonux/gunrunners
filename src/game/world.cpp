#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace gr
{

namespace
{

constexpr int kItemBoxScore = 100;
constexpr int kTurboFrames = kTurboFramesTotal;
constexpr int kVirusFrames = kVirusFramesTotal;
constexpr int kItemPickupDelay = 3;
constexpr int kBonusPoints = 100000;

int sgn(int v) { return (v > 0) - (v < 0); }

int weaponBit(Weapon w) { return 1 << int(w); }

} // namespace

World::World(std::shared_ptr<const Level> level, int characterIndex, const Theme& theme, const Art& art)
  : mLevel(std::move(level))
  , mMap(*mLevel)
  , mCharacter(characterByIndex(characterIndex))
  , mCharacterIndex(characterIndex)
  , mTheme(theme)
  , mArt(art)
{
  auto& p = mPlayer;
  p.x = p.prevX = mRespawnX = mLevel->startTx * kCellsPerTile;
  p.y = p.prevY = mRespawnY = mLevel->startTy * kCellsPerTile + 1;
  mSafeX = p.x;
  mSafeY = p.y;
  p.hp = p.maxHp = mCharacter.maxHp;
  p.weapon = mCharacter.startWeapon;
  p.ammo = mCharacter.startAmmo;
  p.rapidFire = mCharacter.startRapidFire;
  mLevelProto = protoIndex(mLevel->weapon);
  mLayerMask.assign(std::size_t(mLevel->width * mLevel->height), 0);
  mExplored = std::make_shared<std::vector<std::uint8_t>>(std::size_t(mLevel->width * mLevel->height), 0);

  int merchCount = 0;
  for (const auto& s : mLevel->spawns)
  {
    const int x = s.tx * kCellsPerTile;
    const int y = s.ty * kCellsPerTile + 1; // bottom row of the block
    auto addBox = [&](ItemKind kind, int variant) {
      ItemBox b;
      b.content = kind;
      b.x = x;
      b.y = y;
      b.variant = variant;
      mBoxes.push_back(b);
    };
    auto addLoose = [&](ItemKind kind, int variant) {
      Item it;
      it.kind = kind;
      it.x = it.prevX = x;
      it.y = it.prevY = y;
      it.variant = variant;
      it.floating = true;
      mItems.push_back(it);
    };
    auto addEnemy = [&](const char* key) { spawnEnemy(enemyIndex(key), x, y); };
    switch (s.kind)
    {
      case 'g':
        addLoose(ItemKind::Gem, (s.tx + s.ty) % 4);
        ++mStats.gemsTotal;
        break;
      case '1':
        addLoose(ItemKind::LetterG, 0);
        break;
      case '2':
        addLoose(ItemKind::LetterU, 0);
        break;
      case '3':
        addLoose(ItemKind::LetterN, 0);
        break;
      case 'h':
        addBox(ItemKind::Health, 0);
        break;
      case 'm':
        addBox(ItemKind::Merch, merchCount++ % 3);
        ++mStats.merchTotal;
        break;
      case 'L':
        addBox(ItemKind::Laser, 0);
        mStats.weaponsTotal |= weaponBit(Weapon::Laser);
        break;
      case 'R':
        addBox(ItemKind::Rocket, 0);
        mStats.weaponsTotal |= weaponBit(Weapon::Rocket);
        break;
      case 'F':
        addBox(ItemKind::Flame, 0);
        mStats.weaponsTotal |= weaponBit(Weapon::Flame);
        break;
      case 'r':
        addBox(ItemKind::RapidFire, 0);
        break;
      case 'k':
        addBox(ItemKind::Key, 0);
        break;
      case 'T':
        addBox(ItemKind::Turbo, 0);
        break;
      case 'V':
        addLoose(ItemKind::Virus, 0);
        break;
      case 'c':
        mCheckpoints.push_back({x, y, false});
        break;
      case 'w':
        addEnemy("walker");
        break;
      case 'f':
        addEnemy("flyer");
        break;
      case 't':
        addEnemy("turret");
        break;
      case 'C':
        addEnemy("candid_camera");
        break;
      case 'W':
        addBox(ItemKind::Proto, 0);
        break;
      case 'Q':
        addLoose(ItemKind::Duck, 0);
        break;
      case '$':
      {
        Prop pr;
        pr.kind = PropKind::GemCache;
        pr.x = x;
        pr.y = y - 1;
        mProps.push_back(pr);
        mStats.gemsTotal += 5;
        break;
      }
      case 'B':
      {
        // Given by its top-left block; a 2 x 3 block patch of static.
        Prop pr;
        pr.kind = PropKind::BonusDoor;
        pr.x = s.tx * kCellsPerTile;
        pr.y = s.ty * kCellsPerTile;
        pr.w = 4;
        pr.h = 6;
        mProps.push_back(pr);
        break;
      }
      default:
        break;
    }
  }
  setupEntities();
  linkPlatforms();
  linkMine();
  linkLava();
  linkBoulders();
  linkSanctum();
  linkGolden();
  linkStation();
  linkCryo();
  linkGreen();
  linkHull();
  linkOrbit();
  linkGrav();
  linkReactor();
  if (mSpace.starfall)
    finishStarfallSetup();
  if (mSpace.crystals)
    finishCrystalSetup();
  for (const auto& e : mEnemies)
    mSpace.silk = mSpace.silk || e.kind == EnemyKind::LoomSpider || e.kind == EnemyKind::CocoonPod;
  if (mSpace.silk)
    finishSilkSetup();
  if (mPinball)
    setupPinball();
  if (mSurfing)
    setupSurf();
  mLevelEnemyCount = mEnemies.size();
  mStats.enemiesTotal = 0;
  for (const auto& e : mEnemies)
    if (!(enemyDef(e.def).flags & kEnemyNoTally))
      ++mStats.enemiesTotal;
  updateLayers(true);

  mCamera.centerOn(cameraTarget(), mMap.width(), mMap.height());
  mBaseCamY = mCamera.y();
}

Camera::Target World::cameraTarget() const
{
  if (const Vehicle* v = riding())
  {
    const CellBox b = v->box();
    return {b.left(), b.top(), b.right(), b.bottom(), false};
  }
  const CellBox b = mPlayer.box();
  if (mFlight)
    return {b.left(), b.top(), b.right(), b.bottom() + 12, false}; // keep the street in view below
  const bool tight = mPlayer.state == PlayerState::Ladder || mPlayer.state == PlayerState::Jetpack;
  if (mSurfing && !mSurf.free && mSurf.boulder >= 0)
  {
    // Boulder Surfing: the boulder and the ground under it in view too.
    const Boulder& bo = mBoulders[std::size_t(mSurf.boulder)];
    return {std::min(b.left(), bo.x), b.top(), std::max(b.right(), bo.x + bo.size - 1), bo.y + bo.size + 2, false};
  }
  if (motherFight())
  {
    // The Hive Mother: her crown and the arena floor both in view.
    const int floor = mSpace.mother.floor;
    return {b.left(), std::min(b.top(), floor - 15), b.right(), std::max(b.bottom(), floor), false};
  }
  return {b.left(), b.top(), b.right(), b.bottom(), tight};
}

std::vector<Sfx> World::takeSounds()
{
  std::vector<Sfx> out;
  out.swap(mSounds);
  return out;
}

std::vector<Bonus> World::bonuses() const
{
  std::vector<Bonus> out;
  const auto& s = mStats;
  if (s.cheated)
    return out;
  if (!s.tookDamage)
    out.push_back({"NO DAMAGE TAKEN", kBonusPoints});
  if (s.enemiesTotal > 0 && s.kills == s.enemiesTotal)
    out.push_back({"ALL BOTS DESTROYED", kBonusPoints});
  if (s.weaponsTotal != 0 && (s.weaponsCollected & s.weaponsTotal) == s.weaponsTotal)
    out.push_back({"EVERY WEAPON COLLECTED", kBonusPoints});
  if (s.merchTotal > 0 && s.merch == s.merchTotal)
    out.push_back({"ALL MERCHANDISE COLLECTED", kBonusPoints});
  if (s.gemsTotal > 0 && s.gems == s.gemsTotal)
    out.push_back({"ALL GEMS COLLECTED", kBonusPoints});
  if (mLevel->par > 0 && s.frames <= mLevel->par * 15)
    out.push_back({"UNDER PAR TIME", kBonusPoints});
  if (mBonusStar)
    out.push_back({"BONUS STAR", 10000});
  // Gold Fever pays: with the greed meter at 10 or more when Kaan-Tolok
  // falls, every gem of the level counts double.
  if (mGolem.on && mGolem.phase == GolemPhase::Done && mGreed >= 10 && s.gems > 0)
    out.push_back({"GOLD FEVER PAYOUT", s.gems * 500});
  return out;
}

// --- Update ------------------------------------------------------------------

void World::update(const PlayerInput& input)
{
  mPlayer.prevX = mPlayer.x;
  mPlayer.prevY = mPlayer.y;
  for (auto& e : mEnemies)
  {
    e.prevX = e.x;
    e.prevY = e.y;
  }
  for (auto& it : mItems)
  {
    it.prevX = it.x;
    it.prevY = it.y;
  }
  for (auto& pr : mProjectiles)
  {
    pr.prevX = pr.x;
    pr.prevY = pr.y;
  }
  for (auto& v : mVehicles)
  {
    v.prevX = v.x;
    v.prevY = v.y;
  }

  ++mStateFrames;
  switch (mState)
  {
    case WorldState::Playing:
      ++mStats.frames;
      updateLayers(false);
      updateBonusRules(input);
      if (mState != WorldState::Playing)
        break;
      // Stop Motion: the world only moves on frames the runner moves.
      mReactor.moving = !mReactor.stopMotion || runnerMoving(input);
      if (mReactor.moving)
        updatePlatforms();
      if (!mVines.empty())
        updateVines();
      if (mFlight)
        updateFlight(input);
      else if (mTrapmaster)
        updateTrapmaster(input);
      else if (mPinball)
        updatePinball(input);
      else if (mSurfing && !mSurf.free)
        updateSurf(input);
      else if (mStation.recoil)
        updateDrift(input);
      else if (mOrbit.on)
        updateOrbit(input);
      else if (mGrav.on)
        updateGravPlayer(input);
      else
        updatePlayer(input);
      if (mReactor.moving)
        updateMovingWorld(input);
      updatePlayerInteractions();
      if (mReactor.moving)
        updateSpawners();
      if (mPlayer.state == PlayerState::OnGround && mPlayer.cart < 0 && mPlayer.vehicle < 0 &&
          !mMap.overlapsHazard(mPlayer.box()) &&
          (mFluids.empty() || wadeFluid() < 0))
      {
        mSafeX = mPlayer.x;
        mSafeY = mPlayer.y;
      }
      if (mReactor.moving)
      {
        updateEnemies();
        updateProjectiles();
        updateItems();
      }
      mCamera.update(cameraTarget(), mManualScroll, mMap.width(), mMap.height());
      markExplored();
      break;
    case WorldState::Exiting:
      if (mStateFrames > 24)
      {
        mState = WorldState::Done;
        mStateFrames = 0;
      }
      break;
    case WorldState::Done:
      break;
  }
}

// Everything but the runner (Stop Motion holds it still while the runner is).
void World::updateMovingWorld(const PlayerInput& input)
{
  updateVehicles(input);
  updateSea();
  updateClub();
  updateDark(input);
  updateSludge(input);
  updateMaglev(input);
  updateChopper(input);
  updateJungle(input);
  updateTemple(input);
  updateLight(input);
  updateMine(input);
  updateLava(input);
  updateSpace(input);
  updateHive();
  updateStarfall();
  updateCrystals();
  if (mSpace.silk)
    updateSilk();
  if (mSpace.plains)
    updatePlains();
  updateBoulders(input);
  updateSanctum(input);
  if (mSpace.mother.on)
    updateMother();
  updateGolden();
  updateStation(input);
  updateCryo(input);
  updateGreen(input);
  updateHull(input);
  updateGrav(input);
  updateReactor(input);
  updateHatches();
  updateProps(input);
}

bool World::isOnScreen(const CellBox& b, int margin) const
{
  const CellBox view{
    mCamera.x() - margin,
    mCamera.y() - margin,
    int(std::ceil(kViewCellsW)) + margin * 2,
    int(std::ceil(kViewCellsH)) + margin * 2};
  return view.intersects(b);
}

void World::updateEnemies()
{
  const auto& p = mPlayer;
  const CellBox pbox = p.box();
  const bool playerVulnerable = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting && p.tube < 0;

  for (auto& e : mEnemies)
  {
    if (!e.alive)
      continue;
    // Enemies wake up once they scroll into view and stay awake.
    if (!e.active)
      e.active = isOnScreen(e.box(), 1);
    if (!e.active || e.trapped)
      continue;
    if (e.stun > 0)
    {
      --e.stun; // dazed out of a popped bubble
      continue;
    }
    if (e.tangle > 0 && tangled(e))
      continue; // lying in the Snare Bolas' cords
    if (e.frozen > 0)
    {
      updateFrozen(e); // a block of ice (level 16): no moves, no bite
      continue;
    }
    ++e.timer;
    const EnemyDef& def = enemyDef(e.def);

    switch (e.kind)
    {
      case EnemyKind::Walker:
      {
        if (!mMap.onSolidGround(e.box()))
        {
          mMap.moveVertically(e.x, e.y, e.w, e.h, 1);
          if (e.y > mMap.height() + 4)
            e.alive = false; // fell off the map
          break;
        }
        if (e.timer % std::max(1, def.stepEvery) != 0)
          break;
        const CellBox b = e.box();
        const int aheadX = e.dir > 0 ? b.right() + 1 : b.left() - 1;
        const bool wall = e.dir > 0 ? mMap.touchingRightWall(b) : mMap.touchingLeftWall(b);
        const bool ledge = !mMap.solidTop(aheadX, b.bottom() + 1);
        if (wall || ledge)
          e.dir = -e.dir;
        else
          e.x += e.dir;
        break;
      }

      case EnemyKind::Flyer:
      {
        // Hovers above the player's head and dives down every now and then:
        // look up and shoot it before it gets you.
        const int targetX = pbox.x;
        const int targetBottom = pbox.top() - 3;
        e.dir = targetX < e.x ? -1 : 1;
        auto tryMove = [&](int dx, int dy) {
          const CellBox moved = boxAt(e.x + dx, e.y + dy, e.w, e.h);
          if (!mMap.overlapsSolid(moved))
          {
            e.x += dx;
            e.y += dy;
          }
        };
        if (e.dive > 0)
        {
          tryMove(0, 1);
          if (--e.dive == 0)
            e.dive = -6;
          break;
        }
        if (e.dive < 0)
        {
          tryMove(0, -1);
          ++e.dive;
          break;
        }
        const int dx = targetX - e.x;
        if (dx != 0 && (std::abs(dx) > 6 || e.timer % 2 == 0))
          tryMove(sgn(dx), 0);
        const int dy = targetBottom - e.y;
        if (dy != 0 && e.timer % 2 == 0)
          tryMove(0, sgn(dy));
        if (def.flags & kEnemyBeatDive)
        {
          // Shakes through beat 4, then dives on beat 1, once per bar.
          const int bar = mStats.frames / 30;
          const int inBar = mStats.frames % 30;
          const bool overhead = std::abs(dx) <= 2 && std::abs(dy) <= 2;
          e.tell = (overhead && inBar >= 22 && e.lastDive != bar + 1) ? 30 - inBar : 0;
          if (overhead && inBar == 0 && e.lastDive != bar && playerVulnerable)
          {
            e.dive = 4;
            e.lastDive = bar;
          }
        }
        else if (std::abs(dx) <= 1 && std::abs(dy) <= 1 && e.timer % std::max(1, def.cooldown) == 0 &&
                 playerVulnerable)
        {
          e.dive = 4;
        }
        break;
      }

      case EnemyKind::Turret:
      {
        const CellBox b = e.box();
        const int dxc = (pbox.x + 1) - (b.x + 1);
        const int dyc = (pbox.y + 2) - b.y;
        e.dir = dxc < 0 ? -1 : 1;
        const bool inRange = std::abs(dxc) < def.range && std::abs(dyc) < 12 && playerVulnerable && isOnScreen(b, 0);
        const int cd = std::max(1, def.cooldown);
        // The lens glows for `tell` frames before each shot.
        e.tell = (inRange && def.tell > 0 && cd - e.timer % cd <= def.tell) ? cd - e.timer % cd : 0;
        if (e.timer % cd == 0 && inRange)
        {
          // Aim in one of eight directions, like the original wall guns.
          int sx = sgn(dxc), sy = 0;
          if (std::abs(dyc) * 2 > std::abs(dxc))
            sy = sgn(dyc);
          if (std::abs(dxc) * 2 < std::abs(dyc))
            sx = 0;
          spawnProjectile(ShotKind::Enemy, e.dir > 0 ? b.right() + 1 : b.left() - 1, b.top(), sx, sy);
          playSound(Sfx::EnemyShot);
        }
        break;
      }

      case EnemyKind::Camera:
        break;
      case EnemyKind::Crawler:
        updateCrawler(e, def);
        break;
      case EnemyKind::Rider:
        updateRider(e, def);
        break;
      case EnemyKind::Sniper:
        updateSniper(e, def);
        break;
      case EnemyKind::Bouncer:
        updateBouncer(e, def);
        break;
      case EnemyKind::Disco:
        updateDisco(e, def);
        break;
      case EnemyKind::Raver:
        updateRaver(e, def);
        break;
      case EnemyKind::Stepper:
        updateStepper(e, def);
        break;
      case EnemyKind::Stalker:
        updateStalker(e, def);
        break;
      case EnemyKind::Looter:
        updateLooter(e, def);
        break;
      case EnemyKind::Leech:
        updateLeech(e, def);
        break;
      case EnemyKind::Gator:
        updateGator(e, def);
        break;
      case EnemyKind::Keeper:
        updateKeeper(e, def);
        break;
      case EnemyKind::Hopper:
        updateHopper(e, def);
        break;
      case EnemyKind::RailDrone:
        updateRailDrone(e, def);
        break;
      case EnemyKind::Decoupler:
        updateDecoupler(e, def);
        break;
      case EnemyKind::Trooper:
        updateTrooper(e, def);
        break;
      case EnemyKind::Biker:
        updateBiker(e, def);
        break;
      case EnemyKind::Shield:
        updateShield(e, def);
        break;
      case EnemyKind::Howler:
        updateHowler(e, def);
        break;
      case EnemyKind::Viper:
        updateViper(e, def);
        break;
      case EnemyKind::Cutter:
        updateCutter(e, def);
        break;
      case EnemyKind::Guardian:
        updateGuardian(e, def);
        break;
      case EnemyKind::DartFace:
        updateDartFace(e, def);
        break;
      case EnemyKind::Scarabs:
        updateScarabs(e, def);
        break;
      case EnemyKind::Hunter:
      {
        // Trapmaster's cultists walk to the idol (world_temple.cpp moves them).
        break;
      }
      case EnemyKind::Wraith:
        updateWraith(e, def);
        break;
      case EnemyKind::Monk:
        updateMonk(e, def);
        break;
      case EnemyKind::Moth:
        updateMoth(e, def);
        break;
      case EnemyKind::Bandit:
        updateBandit(e, def);
        break;
      case EnemyKind::Bat:
        updateBat(e, def);
        break;
      case EnemyKind::Mole:
        updateMole(e, def);
        break;
      case EnemyKind::Toad:
        updateToad(e, def);
        break;
      case EnemyKind::Wisp:
        updateWisp(e, def);
        break;
      case EnemyKind::Crab:
        updateCrab(e, def);
        break;
      case EnemyKind::Skitter:
        updateSkitter(e, def);
        break;
      case EnemyKind::Spitpod:
        updateSpitpod(e, def);
        break;
      case EnemyKind::Gloop:
        updateGloop(e, def);
        break;
      case EnemyKind::Mite:
        updateMite(e, def);
        break;
      case EnemyKind::Polyp:
        updatePolyp(e, def);
        break;
      case EnemyKind::Warden:
        updateWarden(e, def);
        break;
      case EnemyKind::VoidRay:
        updateVoidRay(e, def);
        break;
      case EnemyKind::RockLeech:
        updateRockLeech(e, def);
        break;
      case EnemyKind::Blinker:
        updateBlinker(e, def);
        break;
      case EnemyKind::ShardGolem:
        updateShardGolem(e, def);
        break;
      case EnemyKind::PrismBat:
        updatePrismBat(e, def);
        break;
      case EnemyKind::LoomSpider:
        updateLoomSpider(e, def);
        break;
      case EnemyKind::CocoonPod:
        updateCocoonPod(e, def);
        break;
      case EnemyKind::Dropling:
        updateDropling(e, def);
        break;
      case EnemyKind::ThornHog:
        updateThornHog(e, def);
        break;
      case EnemyKind::SkyGulper:
        updateSkyGulper(e, def);
        break;
      case EnemyKind::Thornbush:
        break;
      case EnemyKind::EggGuard:
        updateEggGuard(e, def);
        break;
      case EnemyKind::SporeNurse:
        updateSporeNurse(e, def);
        break;
      case EnemyKind::SpearRunner:
        updateSpearRunner(e, def);
        break;
      case EnemyKind::PitSnake:
        updatePitSnake(e, def);
        break;
      case EnemyKind::Totem:
        updateTotem(e, def);
        break;
      case EnemyKind::Drummer:
        updateDrummer(e, def);
        break;
      case EnemyKind::CoinBeetle:
        updateCoinBeetle(e, def);
        break;
      case EnemyKind::Sentinel:
        updateSentinel(e, def);
        break;
      case EnemyKind::Loader:
        updateLoader(e, def);
        break;
      case EnemyKind::WeldDrone:
        updateWeldDrone(e, def);
        break;
      case EnemyKind::Tether:
        updateTether(e, def);
        break;
      case EnemyKind::Puck:
        updatePuck(e, def);
        break;
      case EnemyKind::SleeperPod:
        updateSleeperPod(e, def);
        break;
      case EnemyKind::Mutant:
        updateMutant(e, def);
        break;
      case EnemyKind::LabArm:
        updateLabArm(e, def);
        break;
      case EnemyKind::Puffer:
        updatePuffer(e, def);
        break;
      case EnemyKind::Snapjaw:
        updateSnapjaw(e, def);
        break;
      case EnemyKind::Glob:
        updateGlob(e, def);
        break;
      case EnemyKind::Barnacle:
        updateBarnacle(e, def);
        break;
      case EnemyKind::EvaRam:
        updateEvaRam(e, def);
        break;
      case EnemyKind::Mites:
        updateMites(e, def);
        break;
      case EnemyKind::Fish:
        updateFish(e, def);
        break;
      case EnemyKind::Jelly:
        updateJelly(e, def);
        break;
      case EnemyKind::SeaMine:
        updateSeaMine(e, def);
        break;
      case EnemyKind::Angler:
        updateAngler(e, def);
        break;
      case EnemyKind::FlipWalker:
        updateFlipWalker(e, def);
        break;
      case EnemyKind::Probe:
        updateProbe(e, def);
        break;
      case EnemyKind::TestSubject:
        updateTestSubject(e, def);
        break;
      case EnemyKind::Spark:
        updateSpark(e, def);
        break;
      case EnemyKind::ShieldDrone:
        updateShieldDrone(e, def);
        break;
      case EnemyKind::Imp:
        updateImp(e, def);
        break;
    }

    // Growth Spurt: at x1.5 or bigger, small Globs bounce off.
    const bool frozen = (e.kind == EnemyKind::Stalker && e.attach == 1) || (e.kind == EnemyKind::Puck && e.variant == 1) ||
      (mGreen.grow && mGreen.size >= 2 && e.kind == EnemyKind::Glob && e.w <= 2);
    if (e.alive && playerVulnerable && !frozen && !e.hidden && !(def.flags & kEnemyHarmless) && !mFloorLava &&
        p.vehicle < 0 && e.box().intersects(p.hitBox()))
      touchPlayer(e);
  }
}

void World::spawnProjectile(ShotKind kind, int ax, int ay, int dx, int dy)
{
  Projectile pr;
  pr.kind = kind;
  pr.dx = dx;
  pr.dy = dy;
  const bool vertical = dx == 0;
  int len = 2;
  switch (kind)
  {
    case ShotKind::Normal:
      pr.speed = 2;
      pr.damage = 1;
      break;
    case ShotKind::Laser:
      pr.speed = 5;
      pr.damage = 2;
      pr.pierce = true;
      len = 3;
      break;
    case ShotKind::Rocket:
      pr.speed = 2;
      pr.damage = 8;
      len = 3;
      break;
    case ShotKind::Flame:
      pr.speed = 5;
      pr.damage = 2;
      pr.pierce = true;
      break;
    case ShotKind::Enemy:
      pr.speed = 1;
      pr.damage = 1;
      len = 1;
      break;
    case ShotKind::Proto:
      len = 2; // speed and damage come from the prototype (fireProto)
      break;
  }
  if (kind != ShotKind::Enemy && kind != ShotKind::Proto)
  {
    // Turbo doubles your firepower; the virus halves it.
    if (mPlayer.turbo > 0)
      pr.damage *= 2;
    else if (mPlayer.virus > 0)
      pr.damage = std::max(1, pr.damage / 2);
  }
  // Flames are fat: a horizontal blast also scorches boxes on the floor.
  const int thick = kind == ShotKind::Flame ? 2 : 1;
  pr.w = vertical ? thick : len;
  pr.h = vertical ? len : thick;
  pr.x = dx < 0 ? ax - pr.w + 1 : ax;
  pr.y = dy < 0 ? ay - pr.h + 1 : ay;
  pr.prevX = pr.x;
  pr.prevY = pr.y;
  mProjectiles.push_back(pr);
}

void World::updateProjectiles()
{
  auto collide = [this](Projectile& pr) -> bool {
    // Level 20: the Deflector Bracer sends enemy shots back.
    if (mReactor.on && pr.kind == ShotKind::Enemy && shotAtBracer(pr))
      return false;
    const CellBox b = pr.box();
    // Level 13: a Totem Stack's heads are solid, but shots hit the heads.
    if (!mBoulders.empty() && shotAtTotem(pr, b))
      return true;
    if (mGolden && pr.kind != ShotKind::Enemy && shotAtGolden(pr, b))
      return true;
    // Level 15: crates break to one shot; a Loader Mech's legs are their own target.
    if (mStation.on() && pr.kind != ShotKind::Enemy && shotAtStation(pr, b))
      return true;
    // Level 16: the HOST (SPARE) pod only goes tink.
    if (mCryo.on && pr.kind != ShotKind::Enemy && shotAtCryoEarly(pr, b))
      return true;
    // Level 18: Rivet Mites are their own targets.
    if (mHull.on && pr.kind != ShotKind::Enemy && shotAtHull(pr, b))
      return true;
    // Level 19: a shot throws a gravity switch.
    if (mGrav.on && pr.kind != ShotKind::Enemy && shotAtGrav(pr, b))
      return true;
    // Level 17: a switch lights its grow lamps.
    if (mGreen.on && pr.kind != ShotKind::Enemy && shotAtGreen(pr, b))
      return true;
    // Level 45: a valve set into a tube turns when shot.
    if (mSpace.hive && pr.kind != ShotKind::Enemy && shotAtValve(pr))
      return true;
    // Level 43: shots push drifting rocks along and chip at them.
    if (mSpace.starfall && pr.kind != ShotKind::Enemy && shotAtRock(pr))
      return true;
    // Level 46: a shot at a Swap Crystal swaps you with it.
    if (mSpace.crystals && pr.kind != ShotKind::Enemy && shotAtCrystal(pr))
      return true;
    // Level 47: a cocoon pops open.
    if (mSpace.silk && pr.kind != ShotKind::Enemy && shotAtCocoon(pr))
      return true;
    if (mMap.overlapsSolid(b))
    {
      // The Silk Shooter strings a line where it hits rock.
      silkShotHit(pr);
      // The Swap Rifle's shots bounce off a wall once.
      if (bounceSwapShot(pr))
        return false;
      if (pr.kind != ShotKind::Enemy)
        hitBreakable(b, pr.damage, pr.vehicle ? 5 : (pr.kind == ShotKind::Rocket ? 1 : (pr.damage >= 4 ? 2 : 0)));
      if (mGolden && pr.kind != ShotKind::Enemy)
        gildBox(b);
      const Vec2 c = cellCenter(b);
      burst(c, rgb(255, 255, 210), pr.kind == ShotKind::Enemy ? mTheme.enemyEye : mTheme.accentA, 5, 1.0f);
      if (pr.kind == ShotKind::Rocket)
        explodeAt(b.x + b.w / 2, b.y, pr.radius > 0 ? pr.radius : 3, pr.damage);
      if (pr.lob && pr.vy > 0.0f && !mMap.solid(pr.x, pr.y - 1) && pr.kind != ShotKind::Rocket)
        mPuddles.push_back({pr.x - 3, pr.y, 6, 30}); // a glowstick splashes
      if (pr.flare)
        stickFlare(pr, -1);
      if (pr.footRow >= 0)
        stickSpear(pr);
      shotAtSpace(pr);
      shotAtHive(pr);
      return true;
    }
    if ((!mFluids.empty() || !mDevNull.empty()) && shotAtSludge(pr))
      return true;
    if (!mLavas.empty() && shotAtLava(pr))
      return true;
    if (pr.kind == ShotKind::Enemy)
    {
      if (mPlayer.vehicle >= 0 && shotAtVehicle(b))
        return true;
      if (b.intersects(mPlayer.hitBox()) && mPlayer.state != PlayerState::Dying)
      {
        if (mPlayer.turbo > 0 && mPlayer.cart >= 0)
        {
          // Level 11: in Turbo, shots bounce off a runner in a cart.
          burst(cellCenter(b), rgb(255, 255, 255), rgb(255, 200, 60), 6, 1.4f);
          playSound(Sfx::Hit);
          return true;
        }
        if (pr.carrier)
        {
          if (mPlayer.mercy == 0 && mPlayer.virus == 0)
          {
            infect();
            mPlayer.mercy = 20;
          }
        }
        else
        {
          hurtPlayer(1);
        }
        return true;
      }
      return false;
    }
    shotAtProps(b);
    if (!mLevers.empty() && shotAtMine(pr, b))
      return true;
    if (!mMirrors.empty() && shotAtLight(pr, b))
      return true;
    if (!mBubbles.empty() && shotAtBubbles(b))
      return true;
    if ((!mJRopes.empty() || !mFruits.empty()) && shotAtJungle(pr))
      return true;
    if ((mBoss.on || !mLatches.empty()) && shotAtBoss(pr))
      return true;
    // Level 14: Kaan-Tolok soaks shots but where its gem shows; the glyphs
    // of a Glyph Sentinel's row take what isn't a full draw.
    if (mGolem.on && shotAtGolem(pr, b))
      return true;
    if (mSpace.mother.on && shotAtMother(pr, b))
      return true;
    if (!mGlyphRows.empty() && shotAtGlyph(pr, b))
      return true;
    for (auto& box : mBoxes)
    {
      if (!box.alive || !box.box().intersects(b))
        continue;
      if (pr.kind == ShotKind::Rocket)
      {
        explodeAt(b.x + b.w / 2, b.y, pr.radius > 0 ? pr.radius : 3, pr.damage);
        return true;
      }
      destroyBox(box);
      if (!pr.pierce)
        return true;
    }
    // A virus can be shot down before it gets to you.
    for (auto& it : mItems)
    {
      if (it.taken || it.kind != ItemKind::Virus || !it.box().intersects(b))
        continue;
      it.taken = true;
      const Vec2 c = cellCenter(it.box());
      burst(c, rgb(140, 255, 70), rgb(40, 120, 30), 18, 1.8f);
      flashAt(c, 70.0f, rgb(120, 255, 60), 12);
      addScore(250, c);
      playSound(Sfx::SmallExplosion);
      if (!pr.pierce)
        return true;
    }
    for (auto& e : mEnemies)
    {
      if (!e.alive || !e.active || e.trapped || e.hidden || !e.box().intersects(b))
        continue;
      if (std::find(pr.hit.begin(), pr.hit.end(), e.id) != pr.hit.end())
        continue;
      // Level 10: only light hurts a Wraith, a Monk's shield sends shots back,
      // a moth's swarm scatters.
      if (e.kind == EnemyKind::Wraith || e.kind == EnemyKind::Monk || e.kind == EnemyKind::Moth)
      {
        const int r = shotAtLightEnemy(pr, e);
        if (r == 1)
          return true;
        if (r == 2)
          continue;
      }
      // Level 12: a toad hit in the air drops back into the lava; a crab's
      // shell takes shots from the front and above.
      if (e.kind == EnemyKind::Toad && (e.attach == 2 || e.attach == 4))
      {
        damageEnemy(e, e.hp);
        if (!pr.pierce && pr.pierceLeft <= 0)
          return true;
        if (!pr.pierce)
          --pr.pierceLeft;
        pr.hit.push_back(e.id);
        continue;
      }
      if (e.kind == EnemyKind::Crab && shotAtCrab(pr, e) == 1)
        return true;
      // Level 16: the Freeze Ray, frozen blocks, the pods behind their glass.
      if (mCryo.on && pr.kind != ShotKind::Enemy)
      {
        const int r = shotAtCryo(pr, e);
        if (r == 1)
          return true;
        if (r == 2)
          continue;
      }
      // Episode 7: the Goo Gun glues what it hits.
      if (pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::GooGun) && shotAtAlien(pr, e))
        return true;
      // Level 46: the Swap Rifle swaps you with what it hits; a Shard
      // Golem is only hurt in its back; a Prism Bat splits a shot in three.
      if ((e.kind == EnemyKind::ShardGolem || e.kind == EnemyKind::PrismBat ||
            (pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::SwapRifle))) &&
          shotAtCrystalAlien(pr, e))
        return true;
      // The Bubble Gun traps what fits in a bubble.
      if (pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::BubbleGun) && trapEnemy(e))
        return true;
      // In the dark, shots pass through a Night Stalker; only light pins it.
      if (e.kind == EnemyKind::Stalker && !pr.flare && e.attach != 1)
        continue;
      if (pr.kind == ShotKind::Rocket)
      {
        explodeAt(b.x + b.w / 2, b.y, pr.radius > 0 ? pr.radius : 3, pr.damage);
        return true;
      }
      // A Shield Trooper's shield stops shots from the front; homing
      // rockets come in over it.
      if (e.kind == EnemyKind::Shield && !(pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::LockOnRockets)) &&
          pr.dy <= 0 && (pr.precise ? (pr.vx < 0.0f ? -1 : 1) : pr.dx) == -e.dir)
      {
        e.flash = 3;
        burst(cellCenter(b), rgb(255, 255, 255), rgb(160, 200, 255), 6, 1.4f);
        playSound(Sfx::Land);
        return true;
      }
      // Level 9: the Snare Bolas, a Stone Guardian's front, the beetles.
      if ((pr.proto == int(ProtoId::SnareBolas) || e.kind == EnemyKind::Guardian || e.kind == EnemyKind::Scarabs) &&
          shotAtTemple(pr, e))
        return true;
      const bool wasAlive = e.alive;
      if (!shotHitsEnemy(e, pr.dx, pr.damage))
        return true; // a Bouncer took it on the chest
      if (wasAlive && !e.alive && pr.kind == ShotKind::Proto)
        ++mStats.protoKills;
      if (pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::DeflectorBracer))
        bracerPush(e, pr.dx); // the pulse shot knocks it back a block
      if (pr.flare)
      {
        stickFlare(pr, e.alive ? int(&e - mEnemies.data()) : -1);
        return true;
      }
      burst(cellCenter(b), rgb(255, 255, 255), mTheme.enemyLight, 5, 1.2f);
      if (!pr.pierce && pr.pierceLeft <= 0)
        return true;
      if (!pr.pierce)
        --pr.pierceLeft;
      pr.hit.push_back(e.id);
    }
    return false;
  };

  for (auto& pr : mProjectiles)
  {
    if (!pr.alive)
      continue;
    if (pr.age++ == 0)
    {
      // First frame: the shot appears at the muzzle.
      if (collide(pr))
        pr.alive = false;
      continue;
    }
    if (pr.gy != 0.0f)
      pr.vy = std::min(1.5f, pr.vy + pr.gy);
    if (pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::BubbleGun) && pr.age % 8 == 0)
    {
      // Bubbles drift up as they go.
      --pr.y;
      if (collide(pr))
      {
        pr.alive = false;
        continue;
      }
    }
    if (pr.target != kNoTarget)
      steerRocket(pr);
    for (int i = 0; i < pr.speed && pr.alive; ++i)
    {
      if (pr.precise)
      {
        pr.fx += pr.vx;
        pr.fy += pr.vy;
        pr.x = int(std::floor(pr.fx));
        pr.y = int(std::floor(pr.fy));
      }
      else if (pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::Boomerang))
      {
        if (!stepBoomerang(pr))
        {
          pr.alive = false;
          break;
        }
        if (collide(pr))
          pr.alive = false;
        continue;
      }
      else if (pr.ride >= 0 || (pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::SparkDisc)))
      {
        if (!stepSurfaceShot(pr))
        {
          pr.alive = false;
          break;
        }
        if (collide(pr))
          pr.alive = false;
        continue;
      }
      else
      {
        pr.x += pr.dx;
        pr.y += pr.dy;
      }
      if (collide(pr))
        pr.alive = false;
      if (pr.range > 0 && --pr.range == 0)
        pr.alive = false;
    }
    if (pr.ride > 0 && --pr.ride == 0)
      pr.alive = false;
    const bool comesBack = pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::Boomerang);
    // Level 46: a shot up keeps going a while past the top of the view, at
    // the crystals hovering up there.
    const bool upAtCrystals = (mSpace.crystals && pr.dy < 0 && !pr.precise) ||
      (pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::SilkShooter)); // and on down at the rock
    if (pr.alive && !isOnScreen(pr.box(), pr.lob || pr.target != kNoTarget || comesBack || upAtCrystals ? 12 : 2))
      pr.alive = false;
  }
  mProjectiles.erase(
    std::remove_if(mProjectiles.begin(), mProjectiles.end(), [](const Projectile& p) { return !p.alive; }),
    mProjectiles.end());
}

void World::explodeAt(int cx, int cy, int radius, int damage)
{
  const CellBox area{cx - radius, cy - radius, radius * 2 + 1, radius * 2 + 1};
  for (auto& e : mEnemies)
    if (e.alive && e.active && e.box().intersects(area))
      damageEnemy(e, damage);
  for (auto& b : mBoxes)
    if (b.alive && b.box().intersects(area))
      destroyBox(b);
  explodeAtBoss(cx, cy, radius, damage);
  const Vec2 c{(float(cx) + 0.5f) * kCellSize, (float(cy) + 0.5f) * kCellSize};
  burst(c, rgb(255, 220, 90), rgb(255, 90, 30), 30, 3.0f);
  burst(c, rgb(255, 255, 255), rgb(255, 160, 60), 10, 1.5f);
  flashAt(c, 150.0f, rgb(255, 150, 50), 22);
  mCamera.shake(10, 1.5f);
  playSound(Sfx::Explosion);
}

void World::damageEnemy(Enemy& e, int damage)
{
  if (!e.alive)
    return;
  if (e.kind == EnemyKind::Scarabs)
  {
    killBeetles(e, std::max(3, damage)); // a hit takes out up to three in a line
    return;
  }
  // Level 20: nothing in a Shield Drone's bubble can be hurt.
  if (mReactor.on && reactorBlocksDamage(e))
    return;
  e.hp -= damage;
  e.flash = 8;
  if (e.hp <= 0)
    killEnemy(e);
  else
    playSound(Sfx::Hit);
}

void World::killEnemy(Enemy& e)
{
  e.alive = false;
  const EnemyDef& def = enemyDef(e.def);
  if (!(def.flags & kEnemyNoTally))
    ++mStats.kills;
  const Vec2 c = cellCenter(e.box());
  burst(c, mTheme.enemyBody, rgb(255, 200, 60), 22, 2.4f);
  burst(c, mTheme.enemyEye, rgb(255, 255, 255), 10, 1.4f);
  flashAt(c, 110.0f, rgb(255, 170, 70), 18);
  playSound(Sfx::Explosion);
  addScore(def.score, c);
  if (e.kind == EnemyKind::Looter)
    dropLoot(e);
  if (e.kind == EnemyKind::CoinBeetle)
    dropGems(e.x, e.y, 2);
  if (e.kind == EnemyKind::Gloop)
    alienKilled(e);
  if (e.kind == EnemyKind::Leech)
    for (const auto& c : mCables)
      if (c.breaker >= 0 && &c - mCables.data() == e.attach)
      {
        // Killed on the cable: no more Leeches on this line.
        mBreakers[std::size_t(c.breaker)].leechKilled = true;
        mBreakers[std::size_t(c.breaker)].leechIn = -1;
      }
  if (e.kind == EnemyKind::SeaMine)
    blowSeaMine(e);
  if (e.kind == EnemyKind::Glob)
    splitGlob(e);
  if (e.kind == EnemyKind::Camera)
  {
    mStats.camera = true;
    showMessage("SMILE! YOU'RE ON CANDID CAMERA");
  }
}

int boxColor(ItemKind content)
{
  switch (content)
  {
    case ItemKind::Health:
    case ItemKind::Merch:
      return 1; // blue
    case ItemKind::Laser:
    case ItemKind::Rocket:
    case ItemKind::Flame:
    case ItemKind::Proto:
      return 2; // green
    default:
      return 0; // white
  }
}

void World::destroyBox(ItemBox& b)
{
  b.alive = false;
  const Vec2 c = cellCenter(b.box());
  const Color color = mArt.boxColor[std::size_t(boxColor(b.content))];
  burst(c, color, rgb(60, 60, 80), 16, 2.0f, false);
  flashAt(c, 60.0f, color, 10);
  playSound(Sfx::BoxBreak);
  addScore(kItemBoxScore, c);

  Item it;
  it.kind = b.content;
  it.x = it.prevX = b.x;
  it.y = it.prevY = b.y;
  it.variant = b.variant;
  it.pickupDelay = kItemPickupDelay;
  // Boxes resting on a switchable layer (a neon sign) are anchored: what
  // they release stays put instead of dropping when the sign goes dark.
  const int below = (b.y + 1) / kCellsPerTile, bx = b.x / kCellsPerTile;
  if (below < mLevel->height && mLayerMask[std::size_t(below * mLevel->width + bx)])
    it.floating = true;
  mItems.push_back(it);
}

void World::updateItems()
{
  const auto& p = mPlayer;
  const bool canCollect = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  for (auto& it : mItems)
  {
    if (it.taken || it.heldBy >= 0)
      continue; // a Looter has it
    ++it.frames;
    if (!it.floating)
    {
      // Released items hop out of their box, then drop to the floor.
      // (In a chamber turned over (level 19), up.)
      int down = 1;
      if (mGrav.on)
      {
        const int z = gravZoneAt(it.x + 1, it.y);
        down = z >= 0 && mGrav.zones[std::size_t(z)].dir == Grav::Up ? -1 : 1;
      }
      if (it.vx != 0 && it.frames <= 6)
        mMap.moveHorizontally(it.x, it.y, 2, 2, it.vx);
      if (it.frames <= 2)
        mMap.moveVertically(it.x, it.y, 2, 2, -down);
      else
        mMap.moveVertically(it.x, it.y, 2, 2, (it.frames > 5 ? 2 : 1) * down);
      if (it.y > mMap.height() + 2)
        it.taken = true;
      if (!mFluids.empty())
        floatItem(it);
    }
    if (it.pickupDelay > 0)
    {
      --it.pickupDelay;
      continue;
    }
    // At the wheel, whatever the vehicle flies through is yours.
    if (canCollect && it.box().intersects(riding() ? riding()->box() : p.box()))
      collectItem(it);
  }
  mItems.erase(
    std::remove_if(mItems.begin(), mItems.end(), [](const Item& i) { return i.taken; }), mItems.end());
}

void World::collectItem(Item& it)
{
  auto& p = mPlayer;
  it.taken = true;
  const Vec2 c = cellCenter(it.box());
  auto takeWeapon = [&](Weapon w, const char* msg) {
    p.weapon = w;
    p.ammo = maxAmmo(w);
    mStats.weaponsCollected |= weaponBit(w);
    addScore(2000, c);
    showMessage(msg);
    playSound(Sfx::WeaponPickup);
    burst(c, rgb(120, 255, 140), rgb(255, 255, 255), 14, 1.6f);
  };
  switch (it.kind)
  {
    case ItemKind::Health:
      if (it.variant == 1 && p.hp < p.maxHp)
      {
        // A full refill (before a boss).
        p.hp = p.maxHp;
        addScore(500, c);
        showMessage("FULL HEALTH");
      }
      else if (p.hp < p.maxHp)
      {
        ++p.hp;
        addScore(500, c);
      }
      else
      {
        addScore(10000, c);
        showMessage("FULL HEALTH BONUS");
      }
      playSound(Sfx::Health);
      burst(c, rgb(255, 80, 110), rgb(255, 255, 255), 12, 1.4f);
      break;
    case ItemKind::Merch:
    {
      static const char* const kNames[3] = {
        "GUNRUNNERS MIXTAPE", "LIMITED RUNNER CAP", "COLLECTIBLE ACTION FIGURE"};
      ++mStats.merch;
      addScore(2000, c);
      showMessage(kNames[it.variant % 3]);
      playSound(Sfx::Item);
      burst(c, rgb(120, 190, 255), rgb(255, 255, 255), 12, 1.4f);
      break;
    }
    case ItemKind::Laser:
      takeWeapon(Weapon::Laser, "LASER - SHOOTS THROUGH ENEMIES");
      break;
    case ItemKind::Rocket:
      takeWeapon(Weapon::Rocket, "ROCKETS - HEAVY DAMAGE");
      break;
    case ItemKind::Flame:
      takeWeapon(Weapon::Flame, "FLAMER - HOLD DOWN + FIRE TO FLY");
      break;
    case ItemKind::RapidFire:
      p.rapidFire = 700;
      addScore(500, c);
      showMessage("RAPID FIRE - JUST HOLD THE TRIGGER");
      playSound(Sfx::Item);
      burst(c, rgb(255, 230, 90), rgb(255, 255, 255), 12, 1.4f);
      break;
    case ItemKind::Key:
      p.hasKey = true;
      addScore(500, c);
      showMessage("ACCESS CARD - OPENS FORCE FIELDS");
      playSound(Sfx::Key);
      burst(c, rgb(255, 230, 90), rgb(255, 255, 255), 12, 1.4f);
      break;
    case ItemKind::Turbo:
      addScore(500, c);
      startTurbo();
      break;
    case ItemKind::Virus:
      infect();
      break;
    case ItemKind::Proto:
      takeProto(c);
      break;
    case ItemKind::Duck:
      mStats.duck = true;
      addScore(1000, c);
      showMessage("RUBBER DUCK! SQUEAK");
      playSound(Sfx::Item);
      burst(c, rgb(255, 230, 60), rgb(255, 255, 255), 14, 1.4f);
      break;
    case ItemKind::Gem:
      ++mStats.gems;
      gainGreed();
      addScore(500, c);
      playSound(Sfx::Gem);
      burst(c, mArt.gemColor[std::size_t(it.variant % 4)], rgb(255, 255, 255), 10, 1.3f);
      break;
    case ItemKind::LetterG:
    case ItemKind::LetterU:
    case ItemKind::LetterN:
    {
      static const char kLetters[3] = {'G', 'U', 'N'};
      mStats.letters += kLetters[int(it.kind) - int(ItemKind::LetterG)];
      addScore(1000, c);
      burst(c, mTheme.accentA, rgb(255, 255, 255), 16, 1.6f);
      if (mStats.letters.size() == 3)
      {
        if (mStats.letters == "GUN")
        {
          addScore(kBonusPoints, {c.x, c.y - 24.0f});
          showMessage("G-U-N IN ORDER!");
        }
        else
        {
          addScore(10000, {c.x, c.y - 24.0f});
          showMessage("G-U-N COLLECTED");
        }
        playSound(Sfx::LettersComplete);
        flashAt(c, 160.0f, mTheme.accentA, 30);
      }
      else
      {
        playSound(Sfx::Letter);
      }
      break;
    }
  }
}

void World::startTurbo()
{
  auto& p = mPlayer;
  const bool cured = p.virus > 0;
  p.virus = 0;
  p.turbo = kTurboFrames;
  p.hp = p.maxHp; // full points in every category, health included
  const Vec2 c{(float(p.x) + 1.5f) * kCellSize, (float(p.y) - 2.0f) * kCellSize};
  burst(c, rgb(255, 200, 60), rgb(255, 255, 255), 30, 2.4f);
  flashAt(c, 150.0f, rgb(255, 170, 40), 24);
  playSound(Sfx::TurboOn);
  showMessage(cured ? "TURBO MODE - AND THE VIRUS IS GONE" : "TURBO MODE - EVERYTHING MAXED OUT");
}

void World::infect()
{
  auto& p = mPlayer;
  const Vec2 c{(float(p.x) + 1.5f) * kCellSize, (float(p.y) - 2.0f) * kCellSize};
  burst(c, rgb(140, 255, 70), rgb(60, 110, 30), 22, 1.6f);
  if (p.turbo > 0)
  {
    // Turbo burns the virus off, but uses itself up doing it.
    p.turbo = 0;
    playSound(Sfx::EffectEnd);
    showMessage("TURBO BURNED THE VIRUS OFF");
    return;
  }
  p.virus = kVirusFrames;
  playSound(Sfx::VirusOn);
  showMessage("VIRUS! SLOWER, WEAKER, LOWER JUMPS");
}

void World::addScore(int points, Vec2 at)
{
  mStats.score += points;
  if (mSimulation)
    return;
  FloatingText t;
  t.pos = at;
  t.text = std::to_string(points);
  t.color = points >= 10000 ? mTheme.accentA : rgb(255, 255, 255);
  t.life = points >= 10000 ? 90 : 50;
  mTexts.push_back(t);
}

void World::showMessage(const std::string& text)
{
  mMessage = text;
  mMessageTicks = 200;
}

// --- Effects (60 Hz) ---------------------------------------------------------

void World::burst(Vec2 at, Color a, Color b, int count, float speed, bool glow)
{
  if (mSimulation)
    return;
  for (int i = 0; i < count; ++i)
  {
    Particle p;
    p.pos = at;
    const float ang = mRng.range(0.0f, 6.2831853f);
    const float spd = mRng.range(0.3f, 1.0f) * speed;
    p.vel = {std::cos(ang) * spd, std::sin(ang) * spd - speed * 0.4f};
    p.life = p.maxLife = mRng.irange(14, 32);
    p.color = (i % 2) ? a : b;
    p.size = speed > 1.8f && (i % 3 == 0) ? 2 : 1;
    p.gravity = true;
    p.glow = glow;
    mParticles.push_back(p);
  }
}

void World::flashAt(Vec2 at, float radius, Color c, int life)
{
  if (mSimulation)
    return;
  mFlashes.push_back({at, radius, c, life, life});
}

void World::tickEffects(float alpha)
{
  for (auto& e : mEnemies)
  {
    const float tx = float(e.prevX) + float(e.x - e.prevX) * alpha;
    const float ty = float(e.prevY) + float(e.y - e.prevY) * alpha;
    if (e.drawSnap || !e.active)
    {
      e.drawX = tx;
      e.drawY = ty;
      e.drawSnap = !e.active;
    }
    else
    {
      e.drawX += (tx - e.drawX) * 0.3f;
      e.drawY += (ty - e.drawY) * 0.3f;
    }
  }
  for (auto& p : mParticles)
  {
    p.pos.x += p.vel.x;
    p.pos.y += p.vel.y;
    if (p.gravity)
      p.vel.y += 0.12f;
    --p.life;
  }
  mParticles.erase(
    std::remove_if(mParticles.begin(), mParticles.end(), [](const Particle& p) { return p.life <= 0; }),
    mParticles.end());
  for (auto& t : mTexts)
  {
    t.pos.y -= 0.35f;
    --t.life;
  }
  mTexts.erase(
    std::remove_if(mTexts.begin(), mTexts.end(), [](const FloatingText& t) { return t.life <= 0; }), mTexts.end());
  for (auto& f : mFlashes)
    --f.life;
  mFlashes.erase(
    std::remove_if(mFlashes.begin(), mFlashes.end(), [](const Flash& f) { return f.life <= 0; }), mFlashes.end());
  for (auto& e : mEnemies)
    if (e.flash > 0)
      --e.flash;
  if (mPlayer.muzzleTicks > 0)
    --mPlayer.muzzleTicks;
  if (mMessageTicks > 0)
    --mMessageTicks;
  if (mFieldFlash > 0)
    --mFieldFlash;

  // Thruster sparks while the jetpack is on.
  if (mPlayer.state == PlayerState::Jetpack)
  {
    Particle p;
    p.pos = {(float(mPlayer.x) + 1.5f + mRng.range(-0.5f, 0.5f)) * kCellSize, float(mPlayer.y + 1) * kCellSize};
    p.vel = {mRng.range(-0.4f, 0.4f), mRng.range(1.0f, 2.5f)};
    p.life = p.maxLife = mRng.irange(8, 16);
    p.color = mRng.uniform() < 0.5f ? rgb(255, 200, 60) : rgb(255, 90, 30);
    p.gravity = false;
    mParticles.push_back(p);
  }
  // Turbo afterimage trail and effect particles.
  const auto& pl = mPlayer;
  const Vec2 here{(float(pl.prevX) + float(pl.x - pl.prevX) * alpha + 1.5f) * kCellSize,
    (float(pl.prevY) + float(pl.y - pl.prevY) * alpha + 1.0f) * kCellSize};
  mTickCount++;
  if (pl.turbo == 0 || mTickCount % 3 == 0)
  {
    for (std::size_t i = mTrail.size() - 1; i > 0; --i)
      mTrail[i] = pl.turbo > 0 ? mTrail[i - 1] : here;
    mTrail[0] = here;
  }
  if ((pl.turbo > 0 || pl.virus > 0) && pl.state != PlayerState::Dying && mTickCount % 2 == 0)
  {
    Particle p;
    p.pos = {here.x + mRng.range(-12.0f, 12.0f), here.y - mRng.range(4.0f, 36.0f)};
    p.life = p.maxLife = mRng.irange(14, 26);
    p.gravity = false;
    if (pl.turbo > 0)
    {
      p.vel = {float(-pl.facing) * mRng.range(0.5f, 1.5f), mRng.range(-0.6f, 0.2f)};
      p.color = mRng.uniform() < 0.5f ? rgb(255, 210, 80) : rgb(255, 120, 30);
    }
    else
    {
      p.vel = {mRng.range(-0.2f, 0.2f), mRng.range(-0.8f, -0.3f)}; // bubbles rising
      p.color = mRng.uniform() < 0.6f ? rgb(140, 255, 80) : rgb(60, 160, 40);
      p.size = 2;
    }
    mParticles.push_back(p);
  }
  mCamera.tick(alpha);
}

} // namespace gr
