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

Vec2 cellCenter(const CellBox& b)
{
  return {(float(b.x) + float(b.w) * 0.5f) * kCellSize, (float(b.y) + float(b.h) * 0.5f) * kCellSize};
}

int weaponBit(Weapon w) { return 1 << int(w); }

} // namespace

World::World(std::shared_ptr<const Level> level, int characterIndex, const Theme& theme, const Art& art)
  : mLevel(std::move(level))
  , mMap(*mLevel)
  , mCharacter(&characterByIndex(characterIndex))
  , mCharacterIndex(characterIndex)
  , mTheme(theme)
  , mArt(art)
{
  auto& p = mPlayer;
  p.x = p.prevX = mRespawnX = mLevel->startTx * kCellsPerTile;
  p.y = p.prevY = mRespawnY = mLevel->startTy * kCellsPerTile + 1;
  mSafeX = p.x;
  mSafeY = p.y;
  p.hp = p.maxHp = mCharacter->maxHp;
  p.weapon = mCharacter->startWeapon;
  p.ammo = mCharacter->startAmmo;
  mLevelProto = protoIndex(mLevel->weapon);
  mLayerMask.assign(std::size_t(mLevel->width * mLevel->height), 0);

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
  const CellBox b = mPlayer.box();
  const bool tight = mPlayer.state == PlayerState::Ladder || mPlayer.state == PlayerState::Jetpack;
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

  ++mStateFrames;
  switch (mState)
  {
    case WorldState::Playing:
      ++mStats.frames;
      updateLayers(false);
      updateBonusRules(input);
      if (mState != WorldState::Playing)
        break;
      updatePlayer(input);
      updateProps(input);
      updatePlayerInteractions();
      if (mPlayer.state == PlayerState::OnGround && !mMap.overlapsHazard(mPlayer.box()))
      {
        mSafeX = mPlayer.x;
        mSafeY = mPlayer.y;
      }
      updateEnemies();
      updateProjectiles();
      updateItems();
      mCamera.update(cameraTarget(), mManualScroll, mMap.width(), mMap.height());
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
  const bool playerVulnerable = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;

  for (auto& e : mEnemies)
  {
    if (!e.alive)
      continue;
    // Enemies wake up once they scroll into view and stay awake.
    if (!e.active)
      e.active = isOnScreen(e.box(), 1);
    if (!e.active)
      continue;
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
    }

    if (playerVulnerable && !(def.flags & kEnemyHarmless) && e.box().intersects(p.hitBox()))
      hurtPlayer(1);
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
    const CellBox b = pr.box();
    if (mMap.overlapsSolid(b))
    {
      const Vec2 c = cellCenter(b);
      burst(c, rgb(255, 255, 210), pr.kind == ShotKind::Enemy ? mTheme.enemyEye : mTheme.accentA, 5, 1.0f);
      if (pr.kind == ShotKind::Rocket)
        explodeAt(b.x + b.w / 2, b.y, 3, pr.damage);
      return true;
    }
    if (pr.kind == ShotKind::Enemy)
    {
      if (b.intersects(mPlayer.hitBox()) && mPlayer.state != PlayerState::Dying)
      {
        hurtPlayer(1);
        return true;
      }
      return false;
    }
    for (auto& box : mBoxes)
    {
      if (!box.alive || !box.box().intersects(b))
        continue;
      if (pr.kind == ShotKind::Rocket)
      {
        explodeAt(b.x + b.w / 2, b.y, 3, pr.damage);
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
      if (!e.alive || !e.active || !e.box().intersects(b))
        continue;
      if (std::find(pr.hit.begin(), pr.hit.end(), e.id) != pr.hit.end())
        continue;
      if (pr.kind == ShotKind::Rocket)
      {
        explodeAt(b.x + b.w / 2, b.y, 3, pr.damage);
        return true;
      }
      damageEnemy(e, pr.damage);
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
    for (int i = 0; i < pr.speed && pr.alive; ++i)
    {
      pr.x += pr.dx;
      pr.y += pr.dy;
      if (collide(pr))
        pr.alive = false;
    }
    if (pr.alive && !isOnScreen(pr.box(), 2))
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
    if (it.taken)
      continue;
    ++it.frames;
    if (!it.floating)
    {
      // Released items hop out of their box, then drop to the floor.
      if (it.vx != 0 && it.frames <= 6)
        mMap.moveHorizontally(it.x, it.y, 2, 2, it.vx);
      if (it.frames <= 2)
        mMap.moveVertically(it.x, it.y, 2, 2, -1);
      else
        mMap.moveVertically(it.x, it.y, 2, 2, it.frames > 5 ? 2 : 1);
      if (it.y > mMap.height() + 2)
        it.taken = true;
    }
    if (it.pickupDelay > 0)
    {
      --it.pickupDelay;
      continue;
    }
    if (canCollect && it.box().intersects(p.box()))
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
      if (p.hp < p.maxHp)
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
