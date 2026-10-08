#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace gr
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
    explode({p.pos.x + 8.0f, p.pos.y + 23.0f}, rgba(255, 255, 255, 160), rgba(200, 200, 220, 120), 5, 0.8f, false);
  }
  if (!in.jump && p.vel.y < kJumpCutVelocity)
    p.vel.y = kJumpCutVelocity; // variable jump height

  p.vel.y = std::min(p.vel.y + kGravity, kMaxFall);
  const bool wasOnGround = p.onGround;
  moveBody(p.pos, p.vel, kPlayerBox, p.onGround);
  if (p.onGround && !wasOnGround)
    explode({p.pos.x + 8.0f, p.pos.y + 23.0f}, rgba(255, 255, 255, 120), rgba(200, 200, 220, 100), 3, 0.6f, false);
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
  explode(c, mTheme.platform, mTheme.platformDark, 14, 2.0f, false);
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
      explode({r.cx(), r.cy()}, mArt.gemColor[std::size_t(pk.variant)], rgb(255, 255, 255), 8, 1.2f);
    }
    else
    {
      mPlayer.hp = std::min(mCharacter->maxHp, mPlayer.hp + 1);
      mStats.score += 50;
      explode({r.cx(), r.cy()}, rgb(255, 60, 80), rgb(255, 255, 255), 8, 1.2f);
    }
  }
}

void World::explode(Vec2 at, Color a, Color b, int count, float speed, bool glow)
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

void World::draw(Renderer& r, int frame) const
{
  const float S = kPixelScale;
  const float T = float(kTileSize) * S;
  const float camX = mCamera.renderX() * S;
  const float camY = mCamera.renderY() * S;
  drawBackdrop(r, mArt, camX, camY, mBaseCamY * S);

  const int tx0 = std::max(0, int(camX / T) - 1);
  const int ty0 = std::max(0, int(camY / T) - 1);
  const int tx1 = std::min(mLevel.width - 1, int((camX + float(kScreenW)) / T) + 1);
  const int ty1 = std::min(mLevel.height - 1, int((camY + float(kScreenH)) / T) + 1);
  auto sx = [&](float worldX) { return worldX * S - camX; };
  auto sy = [&](float worldY) { return worldY * S - camY; };

  for (const auto& d : mLevel.decorations)
    if (d.first >= tx0 - 1 && d.first <= tx1 + 1)
      drawDecoration(r, mArt, mTheme, float(d.first) * T - camX, float(d.second) * T - camY, d.first * 31 + d.second, frame);

  drawExit(r, mArt, mTheme, float(mLevel.exitTx) * T - camX, float(mLevel.exitTy - 1) * T - camY, frame);

  for (int ty = ty0; ty <= ty1; ++ty)
  {
    for (int tx = tx0; tx <= tx1; ++tx)
    {
      const float x = float(tx) * T - camX;
      const float y = float(ty) * T - camY;
      switch (mLevel.at(tx, ty))
      {
        case Tile::Solid:
        {
          // Shade blocks darker the deeper they sit below the surface.
          int depth = 0;
          while (depth < 3 && mLevel.isSolid(tx, ty - depth - 1))
            ++depth;
          static constexpr int kShade[4] = {255, 210, 175, 145};
          const auto h = (hash2(tx, ty) >> 8) % 10u;
          DrawOpts o;
          o.tint = rgb(kShade[depth], kShade[depth], kShade[depth]);
          r.draw(mArt.solid[h < 7u ? 0u : (h < 9u ? 1u : 2u)], x, y, o);
          break;
        }
        case Tile::Platform:
          r.draw(mArt.platform, x, y);
          break;
        case Tile::Spikes:
          r.draw(mArt.spikes, x, y);
          break;
        case Tile::Crate:
          r.draw(mArt.crate, x, y);
          break;
        case Tile::Empty:
          break;
      }
    }
  }
  // Surface trims go on top so their glow/grass can overlap neighbours.
  for (int ty = std::max(1, ty0); ty <= ty1; ++ty)
    for (int tx = tx0; tx <= tx1; ++tx)
      if (mLevel.at(tx, ty) == Tile::Solid && !mLevel.isSolid(tx, ty - 1))
        r.draw(mArt.solidTop, float(tx) * T - camX, float(ty) * T - camY);

  for (std::size_t i = 0; i < mPickups.size(); ++i)
  {
    const auto& pk = mPickups[i];
    if (pk.taken)
      continue;
    const float bob = std::sin(float(frame + int(i) * 9) * 0.08f) * 6.0f;
    const float x = sx(pk.pos.x), y = sy(pk.pos.y) + bob;
    if (pk.kind == PickupKind::Gem)
    {
      const Color c = mArt.gemColor[std::size_t(pk.variant)];
      drawGlow(r, mArt, x + 16, y + 16, 40, c, 0.55f + 0.2f * std::sin(float(frame) * 0.15f + float(i)));
      r.draw(mArt.gem[std::size_t(pk.variant)], x, y);
    }
    else
    {
      drawGlow(r, mArt, x + 16, y + 16, 40, rgb(255, 60, 90), 0.5f);
      r.draw(mArt.health, x, y);
    }
  }

  for (const auto& e : mEnemies)
  {
    if (!e.alive)
      continue;
    const float x = sx(e.pos.x), y = sy(e.pos.y);
    if (x < -128.0f || x > float(kScreenW) + 128.0f)
      continue;
    const Texture* tex = nullptr;
    float yOff = 0.0f;
    switch (e.kind)
    {
      case EnemyKind::Walker:
        tex = &mArt.walker[std::size_t((e.timer / 10) % 2)].get(e.dir);
        break;
      case EnemyKind::Flyer:
        tex = &mArt.flyer[std::size_t((e.timer / 3) % 2)].get(e.dir);
        drawGlow(r, mArt, x + 32, y + 62, 26, mTheme.enemyEye, 0.35f);
        break;
      case EnemyKind::Turret:
        tex = &mArt.turret.get(e.dir);
        break;
    }
    r.draw(*tex, x, y + yOff);
    if (e.flash > 0)
    {
      DrawOpts o;
      o.blend = Blend::Add;
      o.alpha = 0.9f;
      r.draw(*tex, x, y + yOff, o);
    }
  }

  const auto& p = mPlayer;
  const bool blinkHidden = p.invulnerable > 0 && (p.invulnerable / 4) % 2 == 0;
  const bool teleported = mState == WorldState::Cleared && mStateTicks > 30;
  if (mState != WorldState::Dead && !blinkHidden && !teleported)
  {
    const auto& ca = mArt.characters[std::size_t(mCharacterIndex)];
    const Sprite* spr = &ca.idle[std::size_t((frame / 30) % 2)];
    if (!p.onGround)
      spr = p.vel.y < 0.0f ? &ca.jump : &ca.fall;
    else if (p.vel.x != 0.0f)
      spr = &ca.run[std::size_t((p.animTicks / 4) % kRunFrames)];
    const float x = sx(p.pos.x), y = sy(p.pos.y);
    r.draw(spr->get(p.facing), x, y);
    if (mState == WorldState::Cleared)
    {
      DrawOpts o;
      o.blend = Blend::Add;
      o.tint = mTheme.accentB;
      o.alpha = float(mStateTicks) / 30.0f;
      r.draw(spr->get(p.facing), x, y, o);
    }
    if (p.muzzleFlash > 0)
    {
      const float mx = x + (p.facing > 0 ? 70.0f : -6.0f);
      const float my = y + 42.0f;
      drawGlow(r, mArt, mx, my, 34, rgb(255, 220, 120), 0.9f);
      drawGlow(r, mArt, mx, my, 12, rgb(255, 255, 255), 1.0f);
    }
  }

  for (const auto& b : mBullets)
  {
    const float x = sx(b.pos.x), y = sy(b.pos.y);
    if (b.fromEnemy)
    {
      drawGlow(r, mArt, x + 10, y + 10, 30, mTheme.enemyEye, 0.8f);
      r.draw(mArt.enemyBullet, x, y);
      continue;
    }
    static constexpr Color kBulletGlow[3] = {rgb(255, 190, 70), rgb(255, 130, 40), rgb(80, 230, 255)};
    drawGlow(r, mArt, x + 12, y + 6, 26, kBulletGlow[b.sprite], 0.8f);
    r.draw(mArt.playerBullet[std::size_t(b.sprite)], x, y);
  }

  for (const auto& pt : mParticles)
  {
    const float life = float(pt.life) / float(std::max(1, pt.maxLife));
    DrawOpts o;
    o.tint = pt.color;
    o.alpha = std::min(1.0f, life * 1.4f) * float(alphaOf(pt.color)) / 255.0f;
    o.blend = pt.glow ? Blend::Add : Blend::Alpha;
    o.scale = pt.size > 1 ? 1.1f : 0.7f;
    r.draw(mArt.dot, sx(pt.pos.x), sy(pt.pos.y), o);
  }

  r.draw(mArt.vignette, 0.0f, 0.0f);

  // HUD
  r.draw(mArt.hudLeft, 16.0f, 12.0f);
  for (int i = 0; i < mCharacter->maxHp; ++i)
    r.draw(i < p.hp ? mArt.heartFull : mArt.heartEmpty, 30.0f + float(i) * 34.0f, 23.0f);
  const TextStyle hudText{24.0f, mTheme.hudText, rgb(10, 8, 20)};
  r.drawText(mCharacter->name, 40.0f + float(mCharacter->maxHp) * 34.0f, 22.0f, hudText);

  r.draw(mArt.hudCenter, float(kScreenW) / 2.0f - 85.0f, 12.0f);
  DrawOpts gemIcon;
  gemIcon.scale = 0.8f;
  r.draw(mArt.gem[0], float(kScreenW) / 2.0f - 62.0f, 22.0f, gemIcon);
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%d / %d", mStats.gems, mStats.gemsTotal);
  r.drawText(buf, float(kScreenW) / 2.0f + 18.0f, 22.0f, {24.0f, mTheme.accentA, rgb(10, 8, 20)}, Align::Center);

  r.draw(mArt.hudRight, float(kScreenW) - 316.0f, 12.0f);
  std::snprintf(buf, sizeof(buf), "SCORE  %06d", mStats.score);
  r.drawText(buf, float(kScreenW) - 34.0f, 22.0f, hudText, Align::Right);
}

} // namespace gr
