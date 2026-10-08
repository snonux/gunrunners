#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace td
{

namespace
{

constexpr float kTile = float(kTileSize);
constexpr float kGravity = 0.32f;
constexpr float kMaxFall = 6.0f;
constexpr float kJumpCutVelocity = -2.0f;
constexpr int kCoyoteTicks = 6;
constexpr int kJumpBufferTicks = 8;
constexpr int kInvulnerableTicks = 80;

const Rect kPlayerBox{Player::kBoxX, Player::kBoxY, Player::kBoxW, Player::kBoxH};
const Rect kWalkerBox{2.0f, 1.0f, 12.0f, 15.0f};

int toTile(float v) { return int(std::floor(v / kTile)); }

Rect offsetBox(Vec2 pos, const Rect& off)
{
  return {pos.x + off.x, pos.y + off.y, off.w, off.h};
}

} // namespace

Rect Enemy::box() const
{
  switch (kind)
  {
    case EnemyKind::Walker:
      return offsetBox(pos, kWalkerBox);
    case EnemyKind::Flyer:
      return {pos.x + 2.0f, pos.y + 2.0f, 12.0f, 10.0f};
    case EnemyKind::Turret:
    default:
      return {pos.x + 1.0f, pos.y + 4.0f, 14.0f, 12.0f};
  }
}

Rect Bullet::box() const
{
  return fromEnemy ? Rect{pos.x, pos.y, 5.0f, 5.0f} : Rect{pos.x, pos.y, 6.0f, 3.0f};
}

World::World(const Level& level, int characterIndex, const Theme& theme, const Art& art)
  : mLevel(level)
  , mCharacter(&characterByIndex(characterIndex))
  , mCharacterIndex(characterIndex)
  , mTheme(theme)
  , mArt(art)
{
  mPlayer.pos = {float(mLevel.startTx) * kTile, float(mLevel.startTy + 1) * kTile - 24.0f};
  mPlayer.hp = mCharacter->maxHp;

  for (const auto& s : mLevel.spawns)
  {
    const float x = float(s.tx) * kTile;
    const float y = float(s.ty) * kTile;
    switch (s.kind)
    {
      case 'g':
        mPickups.push_back({PickupKind::Gem, {x + 4.0f, y + 4.0f}, (s.tx + s.ty) % 4, false});
        ++mStats.gemsTotal;
        break;
      case 'h':
        mPickups.push_back({PickupKind::Health, {x + 4.0f, y + 4.0f}, 0, false});
        break;
      case 'w':
      {
        Enemy e{EnemyKind::Walker, {x, y + kTile - 16.0f}, {}, {}, 3};
        e.home = e.pos;
        mEnemies.push_back(e);
        break;
      }
      case 'f':
      {
        Enemy e{EnemyKind::Flyer, {x, y}, {}, {x, y}, 2};
        e.timer = s.tx * 13;
        mEnemies.push_back(e);
        break;
      }
      case 't':
      {
        Enemy e{EnemyKind::Turret, {x, y + kTile - 16.0f}, {}, {}, 5};
        e.timer = 40;
        mEnemies.push_back(e);
        break;
      }
      default:
        break;
    }
  }
  for (const auto t : mLevel.tiles)
    if (t == Tile::Crate)
      ++mStats.gemsTotal; // every crate drops a gem
  mStats.enemiesTotal = int(mEnemies.size());

  mCamera.centerOn(mPlayer.box(), mLevel.widthPx(), mLevel.heightPx());
  mBaseCamY = mCamera.y();
}

// --- Physics -----------------------------------------------------------------

void World::moveBody(Vec2& pos, Vec2& vel, const Rect& off, bool& onGround)
{
  // Horizontal
  pos.x += vel.x;
  Rect b = offsetBox(pos, off);
  {
    const int top = toTile(b.y);
    const int bottom = toTile(b.bottom() - 0.01f);
    if (vel.x > 0.0f)
    {
      const int tx = toTile(b.right() - 0.01f);
      for (int ty = top; ty <= bottom; ++ty)
      {
        if (mLevel.isSolid(tx, ty))
        {
          pos.x = float(tx) * kTile - off.x - off.w;
          vel.x = 0.0f;
          break;
        }
      }
    }
    else if (vel.x < 0.0f)
    {
      const int tx = toTile(b.x);
      for (int ty = top; ty <= bottom; ++ty)
      {
        if (mLevel.isSolid(tx, ty))
        {
          pos.x = float(tx + 1) * kTile - off.x;
          vel.x = 0.0f;
          break;
        }
      }
    }
  }

  // Vertical
  const float prevBottom = pos.y + off.y + off.h;
  pos.y += vel.y;
  onGround = false;
  b = offsetBox(pos, off);
  const int left = toTile(b.x);
  const int right = toTile(b.right() - 0.01f);
  if (vel.y >= 0.0f)
  {
    const int ty = toTile(b.bottom() - 0.01f);
    for (int tx = left; tx <= right; ++tx)
    {
      const Tile t = mLevel.at(tx, ty);
      const bool lands = t == Tile::Solid || t == Tile::Crate ||
        (t == Tile::Platform && prevBottom <= float(ty) * kTile + 0.01f);
      if (lands)
      {
        pos.y = float(ty) * kTile - off.y - off.h;
        vel.y = 0.0f;
        onGround = true;
        break;
      }
    }
  }
  else
  {
    const int ty = toTile(b.y);
    for (int tx = left; tx <= right; ++tx)
    {
      if (mLevel.isSolid(tx, ty))
      {
        pos.y = float(ty + 1) * kTile - off.y;
        vel.y = 0.0f;
        break;
      }
    }
  }
}

bool World::overlapsTile(const Rect& r, Tile kind) const
{
  for (int ty = toTile(r.y); ty <= toTile(r.bottom() - 0.01f); ++ty)
  {
    for (int tx = toTile(r.x); tx <= toTile(r.right() - 0.01f); ++tx)
    {
      if (mLevel.at(tx, ty) != kind)
        continue;
      // Spikes only occupy the lower part of their tile.
      const Rect hit{float(tx) * kTile + 1.0f, float(ty) * kTile + 7.0f, 14.0f, 9.0f};
      if (hit.intersects(r))
        return true;
    }
  }
  return false;
}

bool World::isActive(const Rect& r) const
{
  // Like RigelEngine's entity activation system: only things near the
  // visible area think, so enemies wait for the player to arrive.
  const Rect view{mCamera.x() - 48.0f, mCamera.y() - 48.0f, float(kViewW) + 96.0f, float(kViewH) + 96.0f};
  return view.intersects(r);
}

// --- Update ------------------------------------------------------------------

void World::update(const Input& input)
{
  ++mStateTicks;
  switch (mState)
  {
    case WorldState::Playing:
      ++mStats.ticks;
      updatePlayer(input);
      updateEnemies();
      updateBullets();
      updatePickups();
      break;
    case WorldState::Cleared:
      if (mStateTicks < 40)
      {
        Particle p;
        p.pos = {mPlayer.pos.x + mRng.range(2.0f, 14.0f), mPlayer.pos.y + 24.0f};
        p.vel = {0.0f, mRng.range(-2.5f, -1.0f)};
        p.life = p.maxLife = 30;
        p.color = mRng.uniform() < 0.5f ? mTheme.accentA : mTheme.accentB;
        p.size = 2;
        p.gravity = false;
        mParticles.push_back(p);
      }
      break;
    case WorldState::Dead:
      updateEnemies();
      updateBullets();
      break;
  }
  updateParticles();
  if (mState == WorldState::Playing)
    mCamera.update(mPlayer.box(), mLevel.widthPx(), mLevel.heightPx());
  mCamera.tick();
}

void World::updatePlayer(const Input& in)
{
  auto& p = mPlayer;
  const auto& c = *mCharacter;

  if (p.knockback > 0)
  {
    --p.knockback;
  }
  else
  {
    const int dir = (in.right ? 1 : 0) - (in.left ? 1 : 0);
    p.vel.x = float(dir) * c.runSpeed;
    if (dir != 0)
      p.facing = dir;
  }

  const bool jumpPressed = in.jump && !p.jumpHeld;
  p.jumpHeld = in.jump;
  if (jumpPressed)
    p.jumpBuffer = kJumpBufferTicks;
  else if (p.jumpBuffer > 0)
    --p.jumpBuffer;

  if (p.jumpBuffer > 0 && (p.onGround || p.coyote > 0))
  {
    p.vel.y = c.jumpVelocity;
    p.jumpBuffer = 0;
    p.coyote = 0;
    p.onGround = false;
    explode({p.pos.x + 8.0f, p.pos.y + 23.0f}, rgba(255, 255, 255, 160), rgba(200, 200, 220, 120), 5, 0.8f);
  }
  if (!in.jump && p.vel.y < kJumpCutVelocity)
    p.vel.y = kJumpCutVelocity; // variable jump height

  p.vel.y = std::min(p.vel.y + kGravity, kMaxFall);
  const bool wasOnGround = p.onGround;
  moveBody(p.pos, p.vel, kPlayerBox, p.onGround);
  if (p.onGround && !wasOnGround)
    explode({p.pos.x + 8.0f, p.pos.y + 23.0f}, rgba(255, 255, 255, 120), rgba(200, 200, 220, 100), 3, 0.6f);
  p.coyote = p.onGround ? kCoyoteTicks : std::max(0, p.coyote - 1);

  if (p.fireCooldown > 0)
    --p.fireCooldown;
  if (in.fire && p.fireCooldown == 0)
    firePlayerWeapon();

  if (p.onGround && p.vel.x != 0.0f)
    ++p.animTicks;
  else
    p.animTicks = 0;
  if (p.invulnerable > 0)
    --p.invulnerable;
  if (p.muzzleFlash > 0)
    --p.muzzleFlash;

  const Rect box = p.box();
  if (overlapsTile(box, Tile::Spikes))
  {
    hurtPlayer(1, box.cx() - float(p.facing));
    if (mState == WorldState::Playing)
      p.vel.y = -5.0f;
  }

  if (p.pos.y > float(mLevel.heightPx()) + 32.0f)
  {
    p.hp = 0;
    mState = WorldState::Dead;
    mStateTicks = 0;
    return;
  }

  const Rect exitBox{float(mLevel.exitTx) * kTile + 4.0f, float(mLevel.exitTy - 1) * kTile + 4.0f, 8.0f, 28.0f};
  if (box.intersects(exitBox))
  {
    mState = WorldState::Cleared;
    mStateTicks = 0;
    p.vel = {};
    mStats.score += p.hp * 250;
  }
}

void World::firePlayerWeapon()
{
  auto& p = mPlayer;
  const auto& c = *mCharacter;
  const float dir = float(p.facing);
  const Vec2 muzzle{p.pos.x + (p.facing > 0 ? 15.0f : -5.0f), p.pos.y + 11.0f};

  auto spawn = [&](float vy, int life, int sprite) {
    Bullet b;
    b.pos = muzzle;
    b.vel = {dir * c.bulletSpeed, vy};
    b.life = life;
    b.sprite = sprite;
    mBullets.push_back(b);
  };

  switch (c.weapon)
  {
    case Weapon::Blaster:
      spawn(0.0f, 70, 0);
      break;
    case Weapon::Scatter:
      spawn(-0.8f, 30, 1);
      spawn(0.0f, 30, 1);
      spawn(0.8f, 30, 1);
      mCamera.shake(3, 1.0f);
      break;
    case Weapon::Rapid:
      spawn(mRng.range(-0.15f, 0.15f), 55, 2);
      break;
  }
  p.fireCooldown = c.fireCooldown;
  p.muzzleFlash = 3;
}

void World::hurtPlayer(int amount, float fromX)
{
  auto& p = mPlayer;
  if (p.invulnerable > 0 || mState != WorldState::Playing)
    return;
  p.hp -= amount;
  p.invulnerable = kInvulnerableTicks;
  p.knockback = 14;
  p.vel.x = p.box().cx() < fromX ? -1.8f : 1.8f;
  p.vel.y = -3.2f;
  mCamera.shake(8, 2.0f);
  explode({p.pos.x + 8.0f, p.pos.y + 12.0f}, rgb(255, 80, 80), rgb(255, 255, 255), 8, 1.5f);
  if (p.hp <= 0)
  {
    p.hp = 0;
    mState = WorldState::Dead;
    mStateTicks = 0;
    mCamera.shake(20, 3.0f);
    explode({p.pos.x + 8.0f, p.pos.y + 12.0f}, rgb(255, 200, 60), rgb(255, 80, 40), 40, 3.0f);
  }
}

void World::updateEnemies()
{
  const Rect pbox = mPlayer.box();
  for (auto& e : mEnemies)
  {
    if (!e.alive)
      continue;
    e.active = isActive(e.box());
    if (!e.active)
      continue;
    ++e.timer;
    if (e.flash > 0)
      --e.flash;

    switch (e.kind)
    {
      case EnemyKind::Walker:
      {
        e.vel.x = float(e.dir) * 0.55f;
        e.vel.y = std::min(e.vel.y + kGravity, kMaxFall);
        moveBody(e.pos, e.vel, kWalkerBox, e.onGround);
        const Rect b = e.box();
        const bool hitWall = e.vel.x == 0.0f;
        const int frontX = toTile(e.dir > 0 ? b.right() + 1.0f : b.x - 1.0f);
        const int belowY = toTile(b.bottom() + 1.0f);
        const Tile below = mLevel.at(frontX, belowY);
        const bool ledge = e.onGround && below != Tile::Solid && below != Tile::Crate && below != Tile::Platform;
        if (hitWall || ledge)
          e.dir = -e.dir;
        break;
      }
      case EnemyKind::Flyer:
      {
        const float dx = pbox.cx() - (e.home.x + 8.0f);
        if (std::abs(dx) < 120.0f)
          e.home.x += dx > 0.0f ? 0.25f : -0.25f;
        e.pos.x = e.home.x + std::sin(float(e.timer) * 0.021f) * 20.0f;
        e.pos.y = e.home.y + std::sin(float(e.timer) * 0.05f) * 10.0f;
        e.dir = pbox.cx() < e.pos.x + 8.0f ? -1 : 1;
        break;
      }
      case EnemyKind::Turret:
      {
        const float dx = pbox.cx() - (e.pos.x + 8.0f);
        const float dy = pbox.cy() - (e.pos.y + 8.0f);
        e.dir = dx < 0.0f ? -1 : 1;
        if (e.timer % 95 == 0 && std::abs(dx) < 200.0f && std::abs(dy) < 60.0f && mState == WorldState::Playing)
        {
          Bullet b;
          b.fromEnemy = true;
          b.pos = {e.pos.x + (e.dir > 0 ? 15.0f : -4.0f), e.pos.y + 5.0f};
          b.vel = {float(e.dir) * 2.4f, clampTo(dy / std::max(40.0f, std::abs(dx)) * 2.4f, -1.0f, 1.0f)};
          b.life = 120;
          mBullets.push_back(b);
          explode({b.pos.x + 2.0f, b.pos.y + 2.0f}, mTheme.enemyEye, rgb(255, 255, 255), 4, 0.8f);
        }
        break;
      }
    }

    if (mState == WorldState::Playing && e.box().intersects(pbox))
      hurtPlayer(1, e.box().cx());
  }
}

void World::updateBullets()
{
  for (auto& b : mBullets)
  {
    if (!b.alive)
      continue;
    b.pos.x += b.vel.x;
    b.pos.y += b.vel.y;
    if (--b.life <= 0)
    {
      b.alive = false;
      continue;
    }
    const Rect bb = b.box();
    const int tx = toTile(bb.cx());
    const int ty = toTile(bb.cy());
    if (mLevel.isSolid(tx, ty))
    {
      if (!b.fromEnemy && mLevel.at(tx, ty) == Tile::Crate)
        damageCrate(tx, ty);
      explode({bb.cx(), bb.cy()}, rgb(255, 255, 200), mTheme.accentA, 4, 1.0f);
      b.alive = false;
      continue;
    }
    if (b.fromEnemy)
    {
      if (mState == WorldState::Playing && bb.intersects(mPlayer.box()))
      {
        hurtPlayer(1, bb.cx() - b.vel.x);
        b.alive = false;
      }
      continue;
    }
    for (auto& e : mEnemies)
    {
      if (!e.alive || !e.active || !bb.intersects(e.box()))
        continue;
      --e.hp;
      e.flash = 5;
      b.alive = false;
      explode({bb.cx(), bb.cy()}, rgb(255, 255, 255), mTheme.enemyLight, 4, 1.2f);
      if (e.hp <= 0)
        killEnemy(e);
      break;
    }
  }
  mBullets.erase(
    std::remove_if(mBullets.begin(), mBullets.end(), [](const Bullet& b) { return !b.alive; }),
    mBullets.end());
}

void World::killEnemy(Enemy& e)
{
  e.alive = false;
  ++mStats.kills;
  const Vec2 c{e.pos.x + 8.0f, e.pos.y + 8.0f};
  explode(c, mTheme.enemyBody, rgb(255, 200, 60), 18, 2.2f);
  explode(c, mTheme.enemyEye, rgb(255, 255, 255), 8, 1.4f);
  mCamera.shake(6, 1.5f);
  switch (e.kind)
  {
    case EnemyKind::Walker:
      mStats.score += 200;
      break;
    case EnemyKind::Flyer:
      mStats.score += 150;
      break;
    case EnemyKind::Turret:
      mStats.score += 300;
      break;
  }
}

void World::damageCrate(int tx, int ty)
{
  auto& hp = mLevel.crateHp[std::size_t(ty * mLevel.width + tx)];
  if (--hp > 0)
    return;
  mLevel.set(tx, ty, Tile::Empty);
  const Vec2 c{float(tx) * kTile + 8.0f, float(ty) * kTile + 8.0f};
  explode(c, mTheme.platform, mTheme.platformDark, 14, 2.0f);
  mStats.score += 50;
  mPickups.push_back({PickupKind::Gem, {c.x - 4.0f, c.y - 4.0f}, (tx + ty) % 4, false});
}

void World::updatePickups()
{
  const Rect pbox = mPlayer.box();
  for (auto& pk : mPickups)
  {
    if (pk.taken)
      continue;
    const Rect r{pk.pos.x, pk.pos.y, 8.0f, 8.0f};
    if (!r.intersects(pbox))
      continue;
    pk.taken = true;
    if (pk.kind == PickupKind::Gem)
    {
      ++mStats.gems;
      mStats.score += 100;
      explode({r.cx(), r.cy()}, mArt.gem[std::size_t(pk.variant)].get(3, 3), rgb(255, 255, 255), 8, 1.2f);
    }
    else
    {
      mPlayer.hp = std::min(mCharacter->maxHp, mPlayer.hp + 1);
      mStats.score += 50;
      explode({r.cx(), r.cy()}, rgb(255, 60, 80), rgb(255, 255, 255), 8, 1.2f);
    }
  }
}

void World::explode(Vec2 at, Color a, Color b, int count, float speed)
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
    mParticles.push_back(p);
  }
}

void World::updateParticles()
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
}

// --- Rendering ---------------------------------------------------------------

void World::draw(Canvas& c, int frame) const
{
  drawSky(c, mTheme, frame);
  drawBackdrop(c, mArt, mCamera.x(), mCamera.y(), mBaseCamY);

  const int camX = mCamera.renderX();
  const int camY = mCamera.renderY();
  const int tx0 = std::max(0, camX / kTileSize - 1);
  const int ty0 = std::max(0, camY / kTileSize - 1);
  const int tx1 = std::min(mLevel.width - 1, (camX + kViewW) / kTileSize + 1);
  const int ty1 = std::min(mLevel.height - 1, (camY + kViewH) / kTileSize + 1);

  for (const auto& d : mLevel.decorations)
    if (d.first >= tx0 && d.first <= tx1)
      drawDecoration(c, mTheme, d.first * kTileSize - camX, d.second * kTileSize - camY, frame);

  drawExit(c, mTheme, mLevel.exitTx * kTileSize - camX, (mLevel.exitTy - 1) * kTileSize - camY, frame);

  for (int ty = ty0; ty <= ty1; ++ty)
  {
    for (int tx = tx0; tx <= tx1; ++tx)
    {
      const int sx = tx * kTileSize - camX;
      const int sy = ty * kTileSize - camY;
      switch (mLevel.at(tx, ty))
      {
        case Tile::Solid:
          {
            // Mostly plain blocks, with the occasional detailed variant.
            const auto h = hash2(tx, ty) % 10u;
            c.blit(mArt.solid[h < 7u ? 0u : h - 6u], sx, sy);
          }
          if (!mLevel.isSolid(tx, ty - 1) && ty > 0)
            c.blit(mArt.solidTop, sx, sy);
          break;
        case Tile::Platform:
          c.blit(mArt.platform, sx, sy);
          break;
        case Tile::Spikes:
          c.blit(mArt.spikes, sx, sy);
          break;
        case Tile::Crate:
          c.blit(mArt.crate, sx, sy);
          break;
        case Tile::Empty:
          break;
      }
    }
  }

  for (std::size_t i = 0; i < mPickups.size(); ++i)
  {
    const auto& pk = mPickups[i];
    if (pk.taken)
      continue;
    const int bob = int(std::sin(float(frame + int(i) * 9) * 0.1f) * 2.0f);
    const Image& img = pk.kind == PickupKind::Gem ? mArt.gem[std::size_t(pk.variant)] : mArt.health;
    c.blit(img, int(pk.pos.x) - camX, int(pk.pos.y) - camY + bob);
  }

  for (const auto& e : mEnemies)
  {
    if (!e.alive)
      continue;
    const int sx = int(e.pos.x) - camX;
    const int sy = int(e.pos.y) - camY;
    if (sx < -32 || sx > kViewW + 32)
      continue;
    const Color flash = e.flash > 0 ? rgb(255, 255, 255) : 0;
    switch (e.kind)
    {
      case EnemyKind::Walker:
        c.blit(mArt.walker[std::size_t((e.timer / 10) % 2)], sx, sy + 1, e.dir < 0, flash);
        break;
      case EnemyKind::Flyer:
        c.blit(mArt.flyer[std::size_t((e.timer / 4) % 2)], sx, sy, e.dir < 0, flash);
        break;
      case EnemyKind::Turret:
        c.blit(mArt.turret, sx, sy + 1, e.dir < 0, flash);
        break;
    }
  }

  const auto& p = mPlayer;
  const bool blinkHidden = p.invulnerable > 0 && (p.invulnerable / 3) % 2 == 0;
  const bool teleported = mState == WorldState::Cleared && mStateTicks > 30;
  if (mState != WorldState::Dead && !blinkHidden && !teleported)
  {
    int frameIdx = kFrameIdle;
    if (!p.onGround)
      frameIdx = kFrameJump;
    else if (p.vel.x != 0.0f)
    {
      static constexpr int kRunCycle[4] = {kFrameRun1, kFrameIdle, kFrameRun2, kFrameIdle};
      frameIdx = kRunCycle[(p.animTicks / 6) % 4];
    }
    const Color flash = (mState == WorldState::Cleared && (mStateTicks / 2) % 2 == 0) ? mTheme.accentB : 0;
    const auto& img = mArt.characters[std::size_t(mCharacterIndex)].frames[std::size_t(frameIdx)];
    c.blit(img, int(p.pos.x) - camX, int(p.pos.y) - camY, p.facing < 0, flash);
    if (p.muzzleFlash > 0)
    {
      const int mx = int(p.pos.x) - camX + (p.facing > 0 ? 16 : -4);
      const int my = int(p.pos.y) - camY + 10;
      c.fillRect(mx, my, 4, 4, withAlpha(rgb(255, 240, 150), 220));
      c.fillRect(mx - 1, my + 1, 6, 2, rgb(255, 255, 255));
    }
  }

  for (const auto& b : mBullets)
  {
    const Image& img = b.fromEnemy ? mArt.enemyBullet : mArt.playerBullet[std::size_t(b.sprite)];
    c.blit(img, int(b.pos.x) - camX, int(b.pos.y) - camY, b.vel.x < 0.0f);
  }

  for (const auto& pt : mParticles)
  {
    const int a = 255 * pt.life / std::max(1, pt.maxLife);
    c.fillRect(int(pt.pos.x) - camX, int(pt.pos.y) - camY, pt.size, pt.size, withAlpha(pt.color, std::min(alphaOf(pt.color), a + 60)));
  }

  // HUD
  c.fillRect(0, 0, kViewW, 11, rgba(0, 0, 0, 150));
  for (int i = 0; i < mCharacter->maxHp; ++i)
    c.drawText("@", 3 + i * 7, 2, i < p.hp ? rgb(255, 60, 90) : rgb(70, 70, 85));
  const int nameX = 3 + mCharacter->maxHp * 7 + 4;
  c.drawText(mCharacter->name, nameX, 2, mTheme.hudText);
  char buf[64];
  std::snprintf(buf, sizeof(buf), "*%02d/%02d", mStats.gems, mStats.gemsTotal);
  c.drawTextCentered(buf, kViewW / 2 + 10, 2, mTheme.accentA);
  std::snprintf(buf, sizeof(buf), "SCORE %06d", mStats.score);
  c.drawText(buf, kViewW - 3 - Canvas::textWidth(buf), 2, mTheme.hudText);
}

} // namespace td
