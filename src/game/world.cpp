#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace gr
{

namespace
{

constexpr int kItemBoxScore = 100;
constexpr int kItemPickupDelay = 3;
constexpr int kBonusPoints = 100000;

int sgn(int v) { return (v > 0) - (v < 0); }

Vec2 cellCenter(const CellBox& b)
{
  return {(float(b.x) + float(b.w) * 0.5f) * kCellSize, (float(b.y) + float(b.h) * 0.5f) * kCellSize};
}

int weaponBit(Weapon w) { return 1 << int(w); }

} // namespace

World::World(const Level& level, int characterIndex, const Theme& theme, const Art& art)
  : mLevel(level)
  , mMap(mLevel)
  , mCharacter(&characterByIndex(characterIndex))
  , mCharacterIndex(characterIndex)
  , mTheme(theme)
  , mArt(art)
{
  auto& p = mPlayer;
  p.x = p.prevX = mRespawnX = mLevel.startTx * kCellsPerTile;
  p.y = p.prevY = mRespawnY = mLevel.startTy * kCellsPerTile + 1;
  p.hp = p.maxHp = mCharacter->maxHp;
  p.weapon = mCharacter->startWeapon;
  p.ammo = mCharacter->startAmmo;

  int enemyId = 0;
  int merchCount = 0;
  for (const auto& s : mLevel.spawns)
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
    auto addEnemy = [&](EnemyKind kind, int w, int h, int hp) {
      Enemy e;
      e.kind = kind;
      e.x = e.prevX = x;
      e.y = e.prevY = y;
      e.w = w;
      e.h = h;
      e.hp = hp;
      e.id = enemyId++;
      e.timer = s.tx * 7;
      mEnemies.push_back(e);
    };
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
      case 'c':
        mCheckpoints.push_back({x, y, false});
        break;
      case 'w':
        addEnemy(EnemyKind::Walker, 3, 3, 3);
        break;
      case 'f':
        addEnemy(EnemyKind::Flyer, 3, 3, 2);
        break;
      case 't':
        addEnemy(EnemyKind::Turret, 3, 2, 4);
        break;
      default:
        break;
    }
  }
  mStats.enemiesTotal = int(mEnemies.size());

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
      updatePlayer(input);
      updatePlayerInteractions();
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

    switch (e.kind)
    {
      case EnemyKind::Walker:
      {
        if (!mMap.onSolidGround(e.box()))
        {
          mMap.moveVertically(e.x, e.y, e.w, e.h, 1);
          break;
        }
        if (e.timer % 2 != 0)
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
        if (std::abs(dx) <= 1 && std::abs(dy) <= 1 && e.timer % 24 == 0 && playerVulnerable)
          e.dive = 4;
        break;
      }

      case EnemyKind::Turret:
      {
        const CellBox b = e.box();
        const int dxc = (pbox.x + 1) - (b.x + 1);
        const int dyc = (pbox.y + 2) - b.y;
        e.dir = dxc < 0 ? -1 : 1;
        if (e.timer % 20 == 0 && std::abs(dxc) < 22 && std::abs(dyc) < 12 && playerVulnerable &&
            isOnScreen(b, 0))
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
    }

    if (playerVulnerable && e.box().intersects(p.hitBox()))
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
      if (!pr.pierce)
        return true;
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
  mCamera.shake(10, 4.0f);
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
  ++mStats.kills;
  const Vec2 c = cellCenter(e.box());
  burst(c, mTheme.enemyBody, rgb(255, 200, 60), 22, 2.4f);
  burst(c, mTheme.enemyEye, rgb(255, 255, 255), 10, 1.4f);
  flashAt(c, 110.0f, rgb(255, 170, 70), 18);
  mCamera.shake(6, 2.5f);
  playSound(Sfx::Explosion);
  switch (e.kind)
  {
    case EnemyKind::Walker:
      addScore(250, c);
      break;
    case EnemyKind::Flyer:
      addScore(500, c);
      break;
    case EnemyKind::Turret:
      addScore(1000, c);
      break;
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

void World::addScore(int points, Vec2 at)
{
  mStats.score += points;
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
  mFlashes.push_back({at, radius, c, life, life});
}

void World::tickEffects()
{
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
  mCamera.tick();
}

} // namespace gr
