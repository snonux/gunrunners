#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace gr
{

namespace
{

constexpr float S = kPixelScale;
constexpr float kCellPx = float(kCellSize) * kPixelScale; // 32
constexpr float kTilePx = float(kTileSize) * kPixelScale; // 64
constexpr Color kHudInk = rgb(10, 8, 20);

float lerpCells(int prev, int cur, float alpha)
{
  return (float(prev) + float(cur - prev) * alpha) * kCellPx;
}

int itemIcon(ItemKind kind, int variant)
{
  switch (kind)
  {
    case ItemKind::Health:
      return kIconHealth;
    case ItemKind::Merch:
      return kIconMerch0 + variant % 3;
    case ItemKind::Laser:
      return kIconLaser;
    case ItemKind::Rocket:
      return kIconRocket;
    case ItemKind::Flame:
      return kIconFlame;
    case ItemKind::RapidFire:
      return kIconRapidFire;
    case ItemKind::Key:
      return kIconKey;
    case ItemKind::Gem:
      return kIconGem0 + variant % 4;
    case ItemKind::LetterG:
      return kIconLetterG;
    case ItemKind::LetterU:
      return kIconLetterU;
    case ItemKind::Turbo:
      return kIconTurbo;
    case ItemKind::Virus:
      return kIconVirus;
    case ItemKind::LetterN:
    default:
      return kIconLetterN;
  }
}

int weaponIcon(Weapon w)
{
  switch (w)
  {
    case Weapon::Laser:
      return kIconLaser;
    case Weapon::Rocket:
      return kIconRocket;
    case Weapon::Flame:
      return kIconFlame;
    case Weapon::Normal:
    default:
      return -1;
  }
}

} // namespace

void World::draw(Renderer& r, int frame, float alpha) const
{
  const float camX = mCamera.renderX() * S;
  const float camY = mCamera.renderY() * S;
  drawBackdrop(r, mArt, camX, camY, float(mBaseCamY) * kCellPx);

  const int tx0 = std::max(0, int(camX / kTilePx) - 1);
  const int tx1 = std::min(mLevel->width - 1, int((camX + float(kScreenW)) / kTilePx) + 1);
  for (const auto& d : mLevel->decorations)
    if (d.first >= tx0 - 1 && d.first <= tx1 + 1)
      drawDecoration(r, mArt, mTheme, float(d.first) * kTilePx - camX, float(d.second) * kTilePx - camY, d.first * 31 + d.second, frame);

  drawExit(r, mArt, mTheme, float(mLevel->exitTx) * kTilePx - 32.0f - camX, float(mLevel->exitTy + 1) * kTilePx - camY, frame);

  for (const auto& cp : mCheckpoints)
  {
    const float x = float(cp.x) * kCellPx - camX;
    const float y = float(cp.y - 3) * kCellPx - camY;
    if (cp.active)
    {
      drawGlow(r, mArt, x + 32, y + 26, 70 + 10 * std::sin(float(frame) * 0.15f), mTheme.accentB, 0.6f);
      r.draw(mArt.beaconOn, x, y);
    }
    else
    {
      r.draw(mArt.beaconOff, x, y);
    }
  }

  drawTiles(r, camX, camY, frame);

  // Item boxes and items.
  for (const auto& b : mBoxes)
  {
    if (!b.alive)
      continue;
    const float x = float(b.x) * kCellPx - camX;
    const float y = float(b.y - 1) * kCellPx - camY;
    if (x < -128.0f || x > float(kScreenW) + 64.0f)
      continue;
    r.draw(mArt.boxes[std::size_t(boxColor(b.content))], x, y);
  }
  for (std::size_t i = 0; i < mItems.size(); ++i)
  {
    const auto& it = mItems[i];
    float x = lerpCells(it.prevX, it.x, alpha) - camX;
    float y = lerpCells(it.prevY - 1, it.y - 1, alpha) - camY;
    if (x < -128.0f || x > float(kScreenW) + 64.0f)
      continue;
    if (it.floating)
      y += std::sin(float(frame + int(i) * 9) * 0.08f) * 6.0f;
    const int icon = itemIcon(it.kind, it.variant);
    if (it.kind == ItemKind::Gem)
      drawGlow(r, mArt, x + 32, y + 34, 50, mArt.gemColor[std::size_t(it.variant % 4)], 0.45f + 0.2f * std::sin(float(frame) * 0.15f + float(i)));
    else if (it.kind == ItemKind::Turbo)
      drawGlow(r, mArt, x + 32, y + 32, 72, rgb(255, 170, 40), 0.55f + 0.2f * std::sin(float(frame) * 0.2f));
    else if (it.kind == ItemKind::Virus)
    {
      // Drifts and twitches so it reads as alive, and dangerous.
      x += std::sin(float(frame + int(i) * 13) * 0.05f) * 10.0f;
      y += std::sin(float(frame) * 0.31f) * 2.0f;
      drawGlow(r, mArt, x + 32, y + 32, 70, rgb(120, 255, 60), 0.4f + 0.2f * std::sin(float(frame) * 0.13f));
    }
    else if (it.kind >= ItemKind::LetterG)
      drawGlow(r, mArt, x + 32, y + 32, 64, mTheme.accentA, 0.45f + 0.15f * std::sin(float(frame) * 0.1f));
    r.draw(mArt.items[std::size_t(icon)], x, y);
  }

  // Enemies.
  for (const auto& e : mEnemies)
  {
    if (!e.alive)
      continue;
    const float x = e.drawX * kCellPx + float(e.w) * kCellPx * 0.5f - camX;
    const float y = (e.drawY + 1.0f) * kCellPx - camY;
    if (x < -160.0f || x > float(kScreenW) + 160.0f)
      continue;
    const Texture* tex = nullptr;
    switch (e.kind)
    {
      case EnemyKind::Walker:
        tex = &mArt.walker[std::size_t((e.x & 2) >> 1)].get(e.dir);
        break;
      case EnemyKind::Flyer:
        tex = &mArt.flyer[std::size_t((frame / 3) % 2)].get(e.dir);
        drawGlow(r, mArt, x, y - 30, 36, mTheme.enemyEye, 0.35f + (e.dive > 0 ? 0.4f : 0.0f));
        break;
      case EnemyKind::Turret:
        tex = &mArt.turret.get(e.dir);
        break;
    }
    r.draw(*tex, x, y);
    if (e.flash > 0)
    {
      DrawOpts o;
      o.blend = Blend::Add;
      o.alpha = float(e.flash) / 8.0f;
      r.draw(*tex, x, y, o);
    }
  }

  drawPlayer(r, camX, camY, frame, alpha);

  // Projectiles.
  for (const auto& pr : mProjectiles)
  {
    const float cx = lerpCells(pr.prevX, pr.x, alpha) + float(pr.w) * kCellPx * 0.5f - camX;
    float cy = lerpCells(pr.prevY, pr.y, alpha) + float(pr.h) * kCellPx * 0.5f - camY;
    DrawOpts o;
    if (pr.dx < 0)
      o.angle = 180.0f;
    else if (pr.dy < 0)
      o.angle = -90.0f;
    else if (pr.dy > 0)
      o.angle = 90.0f;
    const Texture* tex = &mArt.shotNormal;
    Color glow = rgb(255, 190, 70);
    switch (pr.kind)
    {
      case ShotKind::Normal:
        break;
      case ShotKind::Laser:
        tex = &mArt.shotLaser;
        glow = rgb(80, 230, 255);
        break;
      case ShotKind::Rocket:
        tex = &mArt.shotRocket;
        glow = rgb(255, 140, 50);
        break;
      case ShotKind::Flame:
        tex = &mArt.shotFlame;
        glow = rgb(255, 120, 30);
        break;
      case ShotKind::Enemy:
        tex = &mArt.enemyShot;
        glow = mTheme.enemyEye;
        o.angle = 0.0f;
        break;
    }
    if (pr.kind != ShotKind::Enemy && pr.dy == 0)
      cy -= 6.0f; // line up with the gun barrel
    drawGlow(r, mArt, cx, cy, 44, glow, 0.7f);
    r.draw(*tex, cx, cy, o);
  }

  for (const auto& f : mFlashes)
  {
    const float t = float(f.life) / float(f.maxLife);
    drawGlow(r, mArt, f.pos.x * S - camX, f.pos.y * S - camY, f.radius * (1.4f - 0.4f * t), f.color, t);
  }

  for (const auto& pt : mParticles)
  {
    const float life = float(pt.life) / float(std::max(1, pt.maxLife));
    DrawOpts o;
    o.tint = pt.color;
    o.alpha = std::min(1.0f, life * 1.4f) * float(alphaOf(pt.color)) / 255.0f;
    o.blend = pt.glow ? Blend::Add : Blend::Alpha;
    o.scale = pt.size > 1 ? 1.1f : 0.7f;
    r.draw(mArt.dot, pt.pos.x * S - camX, pt.pos.y * S - camY, o);
  }

  for (const auto& t : mTexts)
  {
    const float a = std::min(1.0f, float(t.life) / 20.0f);
    r.drawText(t.text, t.pos.x * S - camX, t.pos.y * S - camY - 40.0f, {22.0f, t.color, kHudInk}, Align::Center, a);
  }

  r.draw(mArt.vignette, 0.0f, 0.0f);
  drawHud(r, frame);
}

void World::drawTiles(Renderer& r, float camX, float camY, int frame) const
{
  const int tx0 = std::max(0, int(camX / kTilePx) - 1);
  const int ty0 = std::max(0, int(camY / kTilePx) - 1);
  const int tx1 = std::min(mLevel->width - 1, int((camX + float(kScreenW)) / kTilePx) + 1);
  const int ty1 = std::min(mLevel->height - 1, int((camY + float(kScreenH)) / kTilePx) + 1);

  // Climbables first, so platforms and walls overlap their ends.
  for (int ty = ty0; ty <= ty1; ++ty)
  {
    for (int tx = tx0; tx <= tx1; ++tx)
    {
      const float x = float(tx) * kTilePx - camX;
      const float y = float(ty) * kTilePx - camY;
      const Tile t = mMap.block(tx, ty);
      if (t == Tile::Ladder)
        r.draw(mArt.ladder, x, y);
      else if (t == Tile::Pipe)
        r.draw(mArt.pipe, x, y);
    }
  }

  for (int ty = ty0; ty <= ty1; ++ty)
  {
    for (int tx = tx0; tx <= tx1; ++tx)
    {
      const float x = float(tx) * kTilePx - camX;
      const float y = float(ty) * kTilePx - camY;
      switch (mMap.block(tx, ty))
      {
        case Tile::Solid:
        {
          // Shade blocks darker the deeper they sit below the surface.
          int depth = 0;
          while (depth < 3 && (mMap.block(tx, ty - depth - 1) == Tile::Solid))
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
        case Tile::ForceField:
        {
          const bool on = mMap.forceFieldsOn();
          if (on)
          {
            DrawOpts beam;
            beam.blend = Blend::Add;
            beam.tint = rgb(255, 70, 90);
            beam.alpha = 0.55f + 0.25f * std::sin(float(frame) * 0.4f + float(ty));
            r.draw(mArt.fieldBeam, x, y, beam);
            drawGlow(r, mArt, x + 32, y + 32, 56, rgb(255, 60, 80), 0.25f);
          }
          else if (mFieldFlash > 0)
          {
            DrawOpts beam;
            beam.blend = Blend::Add;
            beam.tint = rgb(255, 255, 255);
            beam.alpha = float(mFieldFlash) / 40.0f;
            r.draw(mArt.fieldBeam, x, y, beam);
          }
          if (mMap.block(tx, ty - 1) != Tile::ForceField)
            r.draw(mArt.fieldEmitter, x, y - 4);
          if (mMap.block(tx, ty + 1) != Tile::ForceField)
            r.draw(mArt.fieldEmitter, x, y + 48);
          break;
        }
        default:
          break;
      }
    }
  }
  // Surface trims go on top so their glow/grass can overlap neighbours.
  for (int ty = std::max(1, ty0); ty <= ty1; ++ty)
    for (int tx = tx0; tx <= tx1; ++tx)
      if (mMap.block(tx, ty) == Tile::Solid && mMap.block(tx, ty - 1) != Tile::Solid && mMap.block(tx, ty - 1) != Tile::Spikes)
        r.draw(mArt.solidTop, float(tx) * kTilePx - camX, float(ty) * kTilePx - camY);
}

void World::drawPlayer(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& p = mPlayer;
  if (p.hidden)
    return;
  // Mercy frames: Duke blinks the sprite on and off, then flashes it white.
  // Strobing at the logic rate looks harsh in HD, so the runner turns
  // see-through with a gentle pulse instead, and glows white at the end.
  const bool ghost = p.mercy > 10;
  const bool flashWhite = p.mercy > 0 && p.mercy <= 10;
  const float pulse = 0.5f + 0.5f * std::sin(float(frame) * 0.35f);

  const auto& ca = mArt.characters[std::size_t(mCharacterIndex)];
  const Sprite* spr = &ca.idle[std::size_t((frame / 30) % 2)];
  DrawOpts o;
  float lift = 0.0f;
  switch (p.visual)
  {
    case PlayerVisual::Standing:
      break;
    case PlayerVisual::Walking:
      spr = &ca.run[std::size_t(p.walkFrame % kRunFrames)];
      break;
    case PlayerVisual::LookingUp:
      spr = &ca.lookUp;
      break;
    case PlayerVisual::Crouching:
      spr = &ca.crouch;
      break;
    case PlayerVisual::Coiling:
      spr = &ca.coil;
      break;
    case PlayerVisual::Jumping:
      spr = &ca.jump;
      break;
    case PlayerVisual::Somersault:
      spr = &ca.tuck;
      lift = 30.0f * 1.65f;
      o.angle = float(p.facing) * (float(std::max(0, p.somersault)) + alpha) * 45.0f;
      break;
    case PlayerVisual::Falling:
      spr = &ca.fall;
      break;
    case PlayerVisual::FallingFull:
      spr = &ca.fallFast;
      break;
    case PlayerVisual::ClimbingLadder:
      spr = &ca.climb[std::size_t(p.climbFrame % 2)];
      break;
    case PlayerVisual::Hanging:
      spr = &ca.hang;
      break;
    case PlayerVisual::MovingOnPipe:
      spr = &ca.hangMove[std::size_t(p.pipeFrame % 4)];
      break;
    case PlayerVisual::AimingDownOnPipe:
      spr = &ca.hangAimDown;
      break;
    case PlayerVisual::PullingLegsUp:
      spr = &ca.hangLegsUp;
      break;
    case PlayerVisual::Jetpack:
      spr = &ca.jetpack;
      break;
    case PlayerVisual::Dying:
      spr = &ca.hurt;
      if (p.deathPhase == 2)
        o.angle = float(-p.facing) * 80.0f;
      break;
  }

  const float x = lerpCells(p.prevX, p.x, alpha) + 1.5f * kCellPx - camX;
  const float y = lerpCells(p.prevY + 1, p.y + 1, alpha) - camY - lift;

  if (p.state == PlayerState::Jetpack)
  {
    drawGlow(r, mArt, x, y + 4, 60 + 10 * std::sin(float(frame) * 0.8f), rgb(255, 140, 40), 0.8f);
    drawGlow(r, mArt, x, y + 2, 20, rgb(255, 255, 220), 1.0f);
  }

  const Texture& tex = spr->get(p.facing);
  if (p.state == PlayerState::Teleporting)
  {
    const float t = std::min(1.0f, float(mStateFrames) / 16.0f);
    o.alpha = 1.0f - t;
    r.draw(tex, x, y, o);
    DrawOpts glow = o;
    glow.blend = Blend::Add;
    glow.tint = mTheme.accentB;
    glow.alpha = std::min(1.0f, float(mStateFrames) / 6.0f) * (1.0f - t * 0.6f);
    r.draw(tex, x, y, glow);
    drawGlow(r, mArt, x, y - 80, 130 * (1.0f - t * 0.5f), mTheme.accentB, 0.6f);
    return;
  }

  if (ghost)
    o.alpha = 0.45f + 0.3f * pulse;
  if (p.turbo > 0)
  {
    // Afterimages trailing behind, and a hot aura.
    for (std::size_t i = 0; i < mTrail.size(); ++i)
    {
      DrawOpts t = o;
      t.blend = Blend::Add;
      t.tint = rgb(255, 170, 50);
      t.alpha = 0.45f * (1.0f - float(i) / float(mTrail.size()));
      r.draw(tex, mTrail[i].x * S - camX, mTrail[i].y * S - camY - lift, t);
    }
    drawGlow(r, mArt, x, y - 80, 120 + 14 * std::sin(float(frame) * 0.4f), rgb(255, 160, 40), 0.6f);
  }
  if (p.virus > 0)
  {
    o.tint = rgb(150, 255, 120);
    drawGlow(r, mArt, x, y - 80, 90, rgb(110, 255, 60), 0.25f + 0.1f * pulse);
  }
  r.draw(tex, x, y, o);
  if (flashWhite)
  {
    DrawOpts w = o;
    w.blend = Blend::Add;
    w.alpha = 0.25f + 0.35f * pulse;
    r.draw(tex, x, y, w);
  }

  if (p.muzzleTicks > 0)
  {
    static constexpr float kMuzzle[5][2] = {{1.95f, 2.75f}, {1.95f, 1.75f}, {0.45f, 5.6f}, {-0.1f, -0.6f}, {0.0f, -0.6f}};
    const auto& m = kMuzzle[int(p.muzzleStance)];
    float mx = x + float(p.facing) * m[0] * kCellPx;
    if (p.muzzleStance == Stance::Down)
      mx = x - float(p.facing) * 0.3f * kCellPx;
    const float my = y - m[1] * kCellPx;
    Color c = rgb(255, 220, 120);
    if (p.weapon == Weapon::Laser)
      c = rgb(120, 240, 255);
    else if (p.weapon == Weapon::Flame)
      c = rgb(255, 130, 40);
    const float a = float(p.muzzleTicks) / 6.0f;
    drawGlow(r, mArt, mx, my, 44, c, a);
    drawGlow(r, mArt, mx, my, 16, rgb(255, 255, 255), a);
  }
}

void World::drawHud(Renderer& r, int frame) const
{
  const auto& p = mPlayer;
  const TextStyle label{15.0f, rgb(190, 188, 214), kHudInk};
  const TextStyle value{26.0f, mTheme.hudText, kHudInk};
  const float top = 10.0f;
  char buf[64];

  // Health.
  float x = 12.0f;
  r.draw(mArt.hudPanels[0], x, top);
  r.drawText(mCharacter->name, x + 16, top + 6, label);
  for (int i = 0; i < p.maxHp; ++i)
  {
    const bool full = i < p.hp;
    const float hx = x + 14.0f + float(i) * 33.0f;
    const bool low = p.hp <= 2 && full && (frame / 12) % 2 == 0;
    DrawOpts o;
    if (low)
      o.tint = rgb(255, 255, 255);
    r.draw(full ? mArt.heartFull : mArt.heartEmpty, hx, top + 26, o);
  }

  // Weapon and ammo.
  x += float(kHudHealthW) + 10.0f;
  r.draw(mArt.hudPanels[1], x, top);
  r.drawText("WEAPON", x + 16, top + 6, label);
  const int icon = weaponIcon(p.weapon);
  if (icon >= 0)
  {
    DrawOpts io;
    io.scale = 0.62f;
    r.draw(mArt.items[std::size_t(icon)], x + 18, top + 20, io);
  }
  else
  {
    DrawOpts so;
    so.scale = 0.75f;
    r.draw(mArt.shotNormal, x + 40, top + 44, so);
  }
  r.drawText(weaponName(p.weapon), x + 74, top + 24, {22.0f, mTheme.hudText, kHudInk});
  if (p.weapon == Weapon::Normal)
  {
    r.drawText("INF", x + kHudWeaponW - 16, top + 24, {22.0f, mTheme.accentB, kHudInk}, Align::Right);
  }
  else
  {
    std::snprintf(buf, sizeof(buf), "%d", p.ammo);
    r.drawText(buf, x + kHudWeaponW - 16, top + 24, {22.0f, mTheme.accentB, kHudInk}, Align::Right);
    const float frac = float(p.ammo) / float(maxAmmo(p.weapon));
    r.fillRect(x + 74, top + 52, float(kHudWeaponW - 90), 4, rgba(255, 255, 255, 40));
    r.fillRect(x + 74, top + 52, float(kHudWeaponW - 90) * frac, 4, mTheme.accentB);
  }

  // Inventory: rapid fire and the access card.
  x += float(kHudWeaponW) + 10.0f;
  r.draw(mArt.hudPanels[2], x, top);
  r.drawText("ITEMS", x + 16, top + 6, label);
  for (int s = 0; s < 2; ++s)
  {
    const float sx = x + 14.0f + float(s) * 54.0f;
    r.draw(mArt.hudSlot, sx, top + 16);
    const bool have = s == 0 ? p.rapidFire > 0 : p.hasKey;
    if (!have)
      continue;
    const bool expiring = s == 0 && p.rapidFire < 30 && (frame / 6) % 2 == 0;
    if (expiring)
      continue;
    DrawOpts io;
    io.scale = 0.5f;
    r.draw(mArt.items[std::size_t(s == 0 ? kIconRapidFire : kIconKey)], sx + 6, top + 22, io);
    if (s == 0)
      r.fillRect(sx + 4, top + 56, 36.0f * float(p.rapidFire) / 700.0f, 3, rgb(255, 220, 80));
  }

  // G-U-N letters.
  x += float(kHudInventoryW) + 10.0f;
  r.draw(mArt.hudPanels[3], x, top);
  r.drawText("LETTERS", x + 16, top + 6, label);
  static const char* const kLetters[3] = {"G", "U", "N"};
  for (int i = 0; i < 3; ++i)
  {
    const bool got = mStats.letters.find(kLetters[i][0]) != std::string::npos;
    const float lx = x + 34.0f + float(i) * 46.0f;
    if (got)
      drawGlow(r, mArt, lx, top + 40, 26, mTheme.accentA, 0.6f);
    r.drawText(kLetters[i], lx, top + 22, {28.0f, got ? mTheme.accentA : rgba(255, 255, 255, 50), got ? kHudInk : 0}, Align::Center);
  }

  // Score.
  x += float(kHudLettersW) + 10.0f;
  r.draw(mArt.hudPanels[4], x, top);
  r.drawText("SCORE", x + 16, top + 6, label);
  std::snprintf(buf, sizeof(buf), "%07d", mStats.score);
  r.drawText(buf, x + kHudScoreW - 16, top + 22, value, Align::Right);

  // Timed effects: Turbo and Virus, under the health panel.
  auto effectBar = [&](const char* name, int left, int total, Color c, float ey) {
    const bool blink = left < 45 && (frame / 8) % 2 == 0;
    r.fillRect(12, ey, 300, 26, rgba(8, 6, 22, 190));
    r.drawText(name, 22, ey + 3, {17.0f, blink ? rgb(255, 255, 255) : c, kHudInk});
    r.fillRect(110, ey + 9, 190, 8, rgba(255, 255, 255, 40));
    r.fillRect(110, ey + 9, 190.0f * float(left) / float(total), 8, c);
  };
  float ey = top + float(kHudPanelH) + 8.0f;
  if (p.turbo > 0)
  {
    effectBar("TURBO", p.turbo, kTurboFramesTotal, rgb(255, 180, 50), ey);
    ey += 32.0f;
  }
  if (p.virus > 0)
    effectBar("VIRUS", p.virus, kVirusFramesTotal, rgb(130, 255, 70), ey);

  // Pickup and tutorial messages, like Duke's message line.
  if (mMessageTicks > 0)
  {
    const float a = std::min(1.0f, float(mMessageTicks) / 20.0f);
    r.drawText(mMessage, float(kScreenW) / 2.0f, 92.0f, {26.0f, rgb(255, 255, 255), kHudInk}, Align::Center, a);
  }
}

} // namespace gr
