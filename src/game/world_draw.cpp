#include "game/world.hpp"

#include "assets/enemy_art.hpp"
#include "game/reactor_draw.hpp"
#include "game/signal_draw.hpp"
#include "game/toon_draw.hpp"
#include "game/trench_draw.hpp"
#include "game/storm_draw.hpp"
#include "game/fleet_draw.hpp"
#include "game/dish_draw.hpp"
#include "game/magma_draw.hpp"
#include "game/zero_draw.hpp"

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
    case ItemKind::Proto:
      return kIconProto;
    case ItemKind::Duck:
      return kIconDuck;
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
  // Both Sides: the screen is split between the two runners.
  if (mManor.split.on && mSplitPass == 0)
  {
    drawSplit(r, frame, alpha);
    return;
  }
  const float camX = (mSplitPass != 0 ? mSplitCamX : mCamera.renderX() * S) + dishWobble(*this, frame); // Heatstroke's wobble
  const float camY = mSplitPass != 0 ? mSplitCamY : mCamera.renderY() * S;
  if (mTrain)
  {
    // The maglev: the city rushes past (world_maglev.cpp).
    const float s = mScroll + trainSpeed() * alpha;
    drawBackdrop(r, mArt, camX, camY, float(mBaseCamY) * kCellPx, s * float(mScrollSpeeds[1]), s * float(mScrollSpeeds[2]));
  }
  else
    drawBackdrop(r, mArt, camX, camY, float(mBaseCamY) * kCellPx);
  if (mZero.wireframe)
    drawWireframeBackdrop(r, camX, camY, frame); // the Wireframe bonus's debug view (world_zero_draw.cpp)
  drawStarfallSky(r, camX, camY, frame);
  if (mManor.on)
    drawManorWall(r, camX, camY, frame); // Level 23: the inside walls and windows
  if (mToon.on || mInk.on)
    drawToonSet(r, *this, mArt, camX, camY, frame); // Level 28: rooms, the big top, the Inkwell's page
  if (mSignal.on && mSignal.air)
    drawSignalSet(r, *this, mArt, camX, camY, frame); // Level 29: the snow, the Test Card's circle, the bars by the exit
  if (mTrench.on)
    drawTrenchSet(r, *this, mArt, camX, camY, frame); // Level 30: the deep water, the hold, the station, the cable
  if (mStorm.on)
    drawStormSet(r, *this, mArt, camX, camY, frame); // Level 31: the lightning, the sea, the caves' and the tower's walls
  if (mFleet.on)
    drawFleetSet(r, *this, mArt, camX, camY, frame, alpha); // Level 34: the ships, their balloons and hulls, the bomb bay
  if (mDish.on)
    drawDishSet(r, *this, mArt, camX, camY, frame, alpha); // Level 32: the satellite, the dishes, the cabin, the bunker, the tower
  if (mMagma.on)
    drawMagmaSet(r, *this, mArt, camX, camY, frame, alpha); // Level 33: the back aisles, the stations, the crater, the airship

  const int tx0 = std::max(0, int(camX / kTilePx) - 1);
  const int tx1 = std::min(mLevel->width - 1, int((camX + float(kScreenW)) / kTilePx) + 1);
  for (const auto& d : mLevel->decorations)
    if (d.first >= tx0 - 1 && d.first <= tx1 + 1)
      drawDecoration(r, mArt, mTheme, float(d.first) * kTilePx - camX, float(d.second) * kTilePx - camY, d.first * 31 + d.second, frame);

  if ((!mBoss.on || mBoss.exitT >= 0) && (!mGolem.on || mGolem.phase == GolemPhase::Done) &&
    (!mSpace.mother.on || mSpace.mother.phase == MotherPhase::Done) && (!mZero.boss.on || mZero.boss.exitOpen) &&
    (!mTower.titan.on || mTower.titan.phase == TitanPhase::Done))
  {
    // After Black Halo the exit drops out of the crane cab.
    float drop = 0.0f;
    if (mBoss.on && mBoss.exitT < 15)
      drop = float(15 - mBoss.exitT) / 15.0f * 3.0f * kTilePx;
    drawExit(r, mArt, mTheme, float(mLevel->exitTx) * kTilePx - 32.0f - camX,
      float(mLevel->exitTy + 1) * kTilePx - camY - drop, frame);
  }

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

  drawProps(r, camX, camY, frame, false);
  drawMaglevBack(r, camX, camY, frame, alpha);
  drawChopperBack(r, camX, camY, frame, alpha);
  drawSeaBack(r, camX, camY, frame);
  if (mGarden.on)
    drawGardenWalls(r, camX, camY, frame); // Level 25: the soil behind the burrows
  drawTiles(r, camX, camY, frame);
  drawLayers(r, camX, camY, frame);
  drawPlatforms(r, camX, camY, frame, alpha);
  drawJungleBack(r, camX, camY, frame, alpha);
  drawTempleBack(r, camX, camY, frame, alpha);
  drawLightBack(r, camX, camY, frame, alpha);
  drawMineBack(r, camX, camY, frame, alpha);
  drawLavaBack(r, camX, camY, frame, alpha);
  drawSpaceBack(r, camX, camY, frame, alpha);
  drawHiveBack(r, camX, camY, frame, alpha);
  drawStarfallBack(r, camX, camY, frame, alpha);
  drawCrystalBack(r, camX, camY, frame, alpha);
  drawSilkBack(r, camX, camY, frame, alpha);
  drawBoulderBack(r, camX, camY, frame, alpha);
  drawSanctumBack(r, camX, camY, frame, alpha);
  drawMotherBack(r, camX, camY, frame, alpha);
  drawStationBack(r, camX, camY, frame, alpha);
  drawCryoBack(r, camX, camY, frame);
  if (mGreen.on)
    drawGreenBack(r, camX, camY, frame);
  if (mHull.on)
    drawHullBack(r, camX, camY, frame);
  if (mOrbit.on)
    drawOrbitBack(r, camX, camY, frame);
  if (mGrav.on)
    drawGravBack(r, camX, camY, frame);
  if (mReactor.on)
  {
    reactorRenderAlpha() = alpha;
    drawReactorBack(r, camX, camY, frame);
  }
  if (mZero.on)
    drawZeroBack(r, camX, camY, frame, alpha);
  if (mWest.on)
    drawWestBack(r, camX, camY, frame, alpha);
  if (mManor.on)
    drawManorBack(r, camX, camY, frame, alpha);
  if (mKitchen.on)
    drawKitchenBack(r, camX, camY, frame, alpha);
  if (mTurn.on)
    drawTurntableBack(r, camX, camY, frame, alpha);
  if (mGarden.on)
    drawGardenBack(r, camX, camY, frame, alpha);
  if (mGlide.on)
    drawGlideBack(r, camX, camY, frame, alpha);
  if (mCity.on)
    drawCityBack(r, camX, camY, frame, alpha);
  if (mNoir.on)
    drawNoirBack(r, camX, camY, frame, alpha);
  if (mToon.on || mInk.on)
    drawToonBack(r, camX, camY, frame, alpha);
  if (mSignal.on)
    drawSignalBack(r, camX, camY, frame, alpha);
  if (mTrench.on)
    drawTrenchBack(r, camX, camY, frame, alpha);
  if (mStorm.on)
    drawStormBack(r, camX, camY, frame, alpha);
  if (mDish.on)
    drawDishBack(r, camX, camY, frame, alpha);
  if (mMagma.on)
    drawMagmaBack(r, camX, camY, frame, alpha);
  if (mFleet.on)
    drawFleetBack(r, camX, camY, frame, alpha);
  if (mTower.on)
    drawTowerBack(r, camX, camY, frame, alpha);
  if (mWarehouse.on)
    drawWarehouseBack(r, camX, camY, frame, alpha);
  if (mGolden)
    drawGolden(r, camX, camY, frame);
  drawClub(r, camX, camY, frame);
  drawSludgeBack(r, camX, camY, frame);

  // Item boxes and items; ones still sealed inside a breakable (the mixer,
  // the mirror ball) stay out of sight until it breaks.
  auto sealed = [&](int cx, int cy) {
    for (const auto& br : mBreakables)
      if (!br.broken && (br.look == 1 || br.look == 2 || br.look == 5) && cx >= br.x0 * kCellsPerTile &&
          cx < (br.x1 + 1) * kCellsPerTile && cy >= br.y0 * kCellsPerTile && cy < (br.y1 + 1) * kCellsPerTile)
        return true;
    return false;
  };
  for (const auto& b : mBoxes)
  {
    if (!b.alive || sealed(b.x, b.y - 1))
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
    if (sealed(it.x, it.y - 1))
      continue;
    float x = lerpCells(it.prevX, it.x, alpha) - camX;
    float y = lerpCells(it.prevY - 1, it.y - 1, alpha) - camY;
    if (x < -128.0f || x > float(kScreenW) + 64.0f)
      continue;
    if (it.floating && !(it.kind == ItemKind::Virus && it.variant == 1))
      y += std::sin(float(frame + int(i) * 9) * 0.08f) * 6.0f;
    const int icon = itemIcon(it.kind, it.variant);
    if (it.kind == ItemKind::Gem)
      drawGlow(r, mArt, x + 32, y + 34, 50, mArt.gemColor[std::size_t(it.variant % 4)], 0.45f + 0.2f * std::sin(float(frame) * 0.15f + float(i)));
    else if (it.kind == ItemKind::Turbo)
      drawGlow(r, mArt, x + 32, y + 32, 72, rgb(255, 170, 40), 0.55f + 0.2f * std::sin(float(frame) * 0.2f));
    else if (it.kind == ItemKind::Virus && it.variant == 1)
    {
      // The spiked drink: a health box at first glance, but its underside
      // glows green and it wears a cocktail umbrella.
      drawGlow(r, mArt, x + 32, y + 58, 40, rgb(120, 255, 60), 0.45f + 0.15f * std::sin(float(frame) * 0.13f));
      DrawOpts dop;
      dop.tint = rgb(150, 190, 255);
      r.draw(mArt.items[std::size_t(itemIcon(ItemKind::Health, 0))], x, y, dop);
      r.fillRect(x + 8, y + 52, 48, 8, rgba(110, 255, 70, 220));
      r.drawLine(x + 44, y + 14, x + 52, y - 10, 3.0f, rgb(240, 220, 170));
      r.drawLine(x + 36, y - 8, x + 66, y - 14, 7.0f, rgb(255, 90, 160));
      r.drawLine(x + 42, y - 13, x + 60, y - 16, 4.0f, rgb(255, 210, 90));
      continue;
    }
    else if (it.kind == ItemKind::Virus && it.variant == 3)
    {
      // Level 22: Dr. Fizz's Miracle Tonic on the saloon shelf, still (it
      // doesn't bob), a green glow in the glass.
      const float ty = lerpCells(it.prevY - 1, it.y - 1, alpha) - camY;
      drawGlow(r, mArt, x + 32, ty + 40, 44, rgb(120, 255, 60), 0.3f + 0.12f * std::sin(float(frame) * 0.13f));
      DrawOpts tint;
      tint.tint = rgb(220, 255, 210);
      r.draw(styledEnemySprite(mArt, r, mTheme, mWest.tonicSkin.empty() ? "tonic_bottle" : mWest.tonicSkin, 0, 0, 2, 2).get(1),
        x + 32, ty + 64, tint);
      continue;
    }
    else if (it.kind == ItemKind::Virus)
    {
      // Drifts and twitches so it reads as alive, and dangerous.
      x += std::sin(float(frame + int(i) * 13) * 0.05f) * 10.0f;
      y += std::sin(float(frame) * 0.31f) * 2.0f;
      drawGlow(r, mArt, x + 32, y + 32, 70, rgb(120, 255, 60), 0.4f + 0.2f * std::sin(float(frame) * 0.13f));      if (it.variant == 2)
      {
        // Level 43: a green comet, its tail streaming away behind it.
        for (int k = 6; k >= 1; --k)
        {
          const float tx = x + 32.0f + float(k) * 22.0f, ty = y + 32.0f - float(k) * 9.0f;
          const float wob = std::sin(float(frame) * 0.2f + float(k)) * 4.0f;
          drawGlow(r, mArt, tx, ty + wob, 46.0f - float(k) * 5.0f, rgb(140, 255, 90), 0.5f - float(k) * 0.06f);
        }
        r.drawLine(x + 40, y + 30, x + 170, y - 24, 10.0f, rgba(150, 255, 110, 90), Blend::Add);
      }
    }
    else if (it.kind == ItemKind::Proto)
    {
      const Color c = mLevelProto >= 0 ? protoDef(mLevelProto).color : mTheme.accentA;
      drawGlow(r, mArt, x + 32, y + 32, 72, c, 0.6f + 0.2f * std::sin(float(frame) * 0.2f));
      if (mStorm.on && mLevelProto == int(ProtoId::TeslaRod))
      {
        drawTeslaIcon(r, mArt, x, y, 1.0f, frame); // Level 31 (world_storm_draw.cpp)
        continue;
      }
      if (mFleet.on && mLevelProto == int(ProtoId::GrappleGun))
      {
        drawGrappleIcon(r, mArt, x, y, 1.0f, frame); // Level 34 (world_fleet_draw.cpp)
        continue;
      }
      if (mDish.on && mLevelProto == int(ProtoId::SignalJammer))
      {
        drawJammerIcon(r, mArt, x, y, 1.0f, frame); // Level 32 (world_dish_draw.cpp)
        continue;
      }
      if (mMagma.on && mLevelProto == int(ProtoId::HeatSink))
      {
        drawSinkIcon(r, mArt, x, y, 1.0f, frame); // Level 33 (world_magma_draw.cpp)
        continue;
      }
      DrawOpts po;
      po.tint = lerpColor(c, rgb(255, 255, 255), 0.35f);
      r.draw(mArt.items[std::size_t(icon)], x, y, po);
      continue;
    }
    else if (it.kind == ItemKind::Duck)
    {
      drawGlow(r, mArt, x + 32, y + 36, 56, rgb(255, 230, 60), 0.35f);
      if (!mRails.empty())
      {
        // Idol Mines' duck wears a miner's helmet with its lamp on.
        r.draw(mArt.items[std::size_t(icon)], x, y);
        r.fillRect(x + 28, y + 4, 26, 10, rgb(250, 200, 50));
        r.fillRect(x + 24, y + 13, 34, 4, rgb(190, 140, 30));
        r.fillRect(x + 48, y + 6, 7, 6, rgb(255, 250, 210));
        drawGlow(r, mArt, x + 56, y + 9, 28, rgb(255, 250, 200), 0.7f);
        continue;
      }
      if (mManor.on && mLevel->themeKey == "haunted_manor")
      {
        // Fright Night Manor's duck wears a vampire cape.
        r.draw(styledEnemySprite(mArt, r, mTheme, "duck_cape", 0, 0, 2, 2).get(1), x + 32, y + 64);
        r.draw(mArt.items[std::size_t(icon)], x, y);
        r.draw(styledEnemySprite(mArt, r, mTheme, "duck_cape", 1, 0, 2, 2).get(1), x + 32, y + 64);
        continue;
      }
      if (mGarden.on && mLevel->themeKey == "macro_garden")
      {
        // Tiny Kingdom's duck wears a pith helmet.
        r.draw(mArt.items[std::size_t(icon)], x, y);
        r.draw(styledEnemySprite(mArt, r, mTheme, "duck_pith", 0, 0, 2, 2).get(1), x + 32, y + 64);
        continue;
      }
      if (mCity.on && mLevel->themeKey == "model_city")
      {
        // Kaiju Tonight's duck wears a rubber monster suit.
        r.draw(styledEnemySprite(mArt, r, mTheme, "duck_suit", 0, 0, 2, 2).get(1), x + 32, y + 64);
        r.draw(mArt.items[std::size_t(icon)], x, y);
        r.draw(styledEnemySprite(mArt, r, mTheme, "duck_suit", 1, 0, 2, 2).get(1), x + 32, y + 64);
        continue;
      }
      if (mToon.on && mLevel->themeKey == "rubber_hose_toon")
      {
        // Toon Town Throwdown's duck wears white gloves.
        r.draw(mArt.items[std::size_t(icon)], x, y);
        r.draw(styledEnemySprite(mArt, r, mTheme, "duck_gloves", 1, 0, 2, 2).get(1), x + 32, y + 64);
        continue;
      }
      if (mSignal.air && mLevel->themeKey == "dead_air")
      {
        // Dead Air's duck is striped in the seven bar colours.
        r.draw(mArt.items[std::size_t(icon)], x, y);
        r.draw(styledEnemySprite(mArt, r, mTheme, "duck_bars", 1, 0, 2, 2).get(1), x + 32, y + 64);
        continue;
      }
      if (mTrench.on && mLevel->themeKey == "deep_sea")
      {
        // Fiber Trench's duck wears a diving mask and a snorkel.
        r.draw(mArt.items[std::size_t(icon)], x, y);
        r.draw(styledEnemySprite(mArt, r, mTheme, "duck_diving_mask", 1, 0, 2, 2).get(1), x + 32, y + 64);
        continue;
      }
      if (mDish.on && mLevel->themeKey == "desert_array")
      {
        // Dish Array's duck wears sunglasses.
        r.draw(mArt.items[std::size_t(icon)], x, y);
        r.draw(styledEnemySprite(mArt, r, mTheme, "duck_sunglasses", 1, 0, 2, 2).get(1), x + 32, y + 64);
        continue;
      }
      if (mMagma.on && mLevel->themeKey == "volcano_datacenter")
      {
        // Magma Mainframe's duck wears a heat suit's hood.
        r.draw(mArt.items[std::size_t(icon)], x, y);
        r.draw(styledEnemySprite(mArt, r, mTheme, "duck_heat_suit", 1, 0, 2, 2).get(1), x + 32, y + 64);
        continue;
      }
      if (mStorm.on && mLevel->themeKey == "storm_cliffs")
      {
        // Storm Relay's duck wears a sou'wester.
        r.draw(mArt.items[std::size_t(icon)], x, y);
        r.draw(styledEnemySprite(mArt, r, mTheme, "duck_sou_wester", 1, 0, 2, 2).get(1), x + 32, y + 64);
        continue;
      }
      if (mFleet.on && mLevel->themeKey == "sunset_airships")
      {
        // Zeppelin Relay's duck wears a flying cap and goggles.
        r.draw(mArt.items[std::size_t(icon)], x, y);
        r.draw(styledEnemySprite(mArt, r, mTheme, "duck_aviator", 1, 0, 2, 2).get(1), x + 32, y + 64);
        continue;
      }
      if (mKitchen.on && mLevel->themeKey == "tv_kitchen")
      {
        // Too Many Cooks' duck wears a chef's hat.
        r.draw(mArt.items[std::size_t(icon)], x, y);
        r.draw(styledEnemySprite(mArt, r, mTheme, "duck_chef", 0, 0, 2, 2).get(1), x + 32, y + 64);
        continue;
      }
      if (mWest.on && mLevel->themeKey == "western_gulch")
      {
        // Dry Gulch's duck wears a cowboy hat.
        r.draw(mArt.items[std::size_t(icon)], x, y);
        r.draw(styledEnemySprite(mArt, r, mTheme, "duck_cowboy", 0, 0, 2, 2).get(1), x + 32, y + 64);
        continue;
      }
      if (mSpace.starfall)
      {
        // Starfall's duck floats in a space helmet.
        r.draw(mArt.items[std::size_t(icon)], x, y);
        r.draw(styledEnemySprite(mArt, r, mTheme, "duck_helmet", 0, 0, 2, 2).get(1), x + 32, y + 64);
        continue;
      }
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
    float x = e.drawX * kCellPx + float(e.w) * kCellPx * 0.5f - camX;
    const float y = (e.drawY + 1.0f) * kCellPx - camY;
    if (x < -160.0f || x > float(kScreenW) + 160.0f)
      continue;
    const EnemyDef& def = enemyDef(e.def);
    // Hidden inside something breakable (the camera in the mirror ball).
    bool inside = false;
    for (const auto& b : mBreakables)
      inside = inside || (!b.broken && e.x / kCellsPerTile >= b.x0 && e.x / kCellsPerTile <= b.x1 &&
                           e.y / kCellsPerTile >= b.y0 && e.y / kCellsPerTile <= b.y1);
    if (inside || e.hidden || e.y < 0 || (e.kind == EnemyKind::Decoupler && e.attach == 0))
      continue;
    if (mTop.on && e.kind == EnemyKind::ModelTank)
      continue; // seen from above (world_city_draw.cpp)
    if (e.kind == EnemyKind::StaticSwarm || e.kind == EnemyKind::TestCard || e.kind == EnemyKind::GhostImage)
      continue; // Level 29 (world_signal_draw.cpp)
    if (e.kind == EnemyKind::Anglerfish || e.kind == EnemyKind::BoxJelly || e.kind == EnemyKind::CableEel)
      continue; // Level 30 (world_trench_draw.cpp)
    if (e.kind == EnemyKind::StormGull || e.kind == EnemyKind::KiteDrone || e.kind == EnemyKind::Foghorn)
      continue; // Level 31 (world_storm_draw.cpp)
    if (e.kind == EnemyKind::SandShark || e.kind == EnemyKind::VultureDrone || e.kind == EnemyKind::ScorpionMech)
      continue; // Level 32 (world_dish_draw.cpp)
    if (e.kind == EnemyKind::Firewall || e.kind == EnemyKind::LavaDaemon || e.kind == EnemyKind::FanBot)
      continue; // Level 33 (world_magma_draw.cpp)
    if (e.kind == EnemyKind::SkyPirate || e.kind == EnemyKind::PeckParrot || e.kind == EnemyKind::BalloonMine)
      continue; // Level 34 (world_fleet_draw.cpp)
    if (e.kind == EnemyKind::Forklift || e.kind == EnemyKind::Stagehand || e.kind == EnemyKind::Mimic)
      continue; // Level 36 (world_warehouse_draw.cpp)
    if (e.kind == EnemyKind::MastClimber || e.kind == EnemyKind::RelayDrone || e.kind == EnemyKind::FeedbackNode)
      continue; // Level 35 (world_tower_draw.cpp)
    if (e.kind == EnemyKind::Echo)
      continue; // a hologram of the runner (drawZeroFront) // (asleep in its coupling, or riding a train not here yet)
    if (e.tell > 0 && (e.kind == EnemyKind::Flyer || e.kind == EnemyKind::Viper))
      x += ((frame / 2) % 2 ? 4.0f : -4.0f); // shakes before it dives (the Viper's leaves rustle)
    const Texture* tex = nullptr;
    float hop = 0.0f;
    DrawOpts eo;
    if (def.tint != 0)
      eo.tint = def.tint;
    if (e.kind == EnemyKind::Wraith)
      eo.alpha = 0.72f; // half there
    switch (def.look)
    {
      case EnemyLook::Walker:
        tex = &mArt.walker[std::size_t((e.x & 2) >> 1)].get(e.dir);
        break;
      case EnemyLook::Flyer:
        tex = &mArt.flyer[std::size_t((frame / 3) % 2)].get(e.dir);
        drawGlow(r, mArt, x, y - 30, 36, mTheme.enemyEye, 0.35f + (e.dive > 0 || e.tell > 0 ? 0.4f : 0.0f));
        break;
      case EnemyLook::Turret:
        tex = &mArt.turret.get(e.dir);
        if (e.tell > 0)
          drawGlow(r, mArt, x + float(e.dir) * 30.0f, y - 40, 50, mTheme.enemyEye, 0.9f - float(e.tell) * 0.05f);
        break;
      case EnemyLook::Styled:
      {
        int variant = (e.kind == EnemyKind::Crawler && (e.attach == -1 || e.attach == 1)) ? 0 : 1;
        if (e.kind == EnemyKind::Bouncer || e.kind == EnemyKind::Disco || e.kind == EnemyKind::Raver)
          variant = e.tell > 0 ? 1 : 0; // the club's tells
        else if (e.kind == EnemyKind::Stepper)
          variant = 0;
        else if (e.kind == EnemyKind::Stalker)
          variant = e.attach == 1 ? 1 : (e.tell > 0 || e.dive > 0 ? 2 : 0); // frozen in light, lunging
        else if (e.kind == EnemyKind::Looter)
          variant = e.dive > 0 ? 1 : 0; // the sack is full
        else if (e.kind == EnemyKind::Leech)
          variant = e.tell > 0 ? 1 : 0;
        else if (e.kind == EnemyKind::Gator)
          variant = e.variant + (e.dive > 0 || e.trapped ? 2 : 0); // sunglasses; jaws open
        else if (e.kind == EnemyKind::Keeper)
          variant = e.attach == 2 ? 1 : 0; // turning the wheel
        else if (e.kind == EnemyKind::Hopper)
          variant = e.attach == 1 ? 1 : (e.attach == 2 ? 2 : 0); // crouching, leaping
        else if (e.kind == EnemyKind::RailDrone)
          variant = e.tell > 0 ? 1 : 0; // the bay is open
        else if (e.kind == EnemyKind::Decoupler)
          variant = e.attach == 2 ? 1 : 0; // working the coupling
        else if (e.kind == EnemyKind::Trooper)
          variant = e.attach == 1 ? 2 : (e.tell > 0 ? 1 : 0); // on the rope, aiming
        else if (e.kind == EnemyKind::Biker)
          variant = e.attach == 2 ? 1 : 0; // revving, headlight on
        else if (e.kind == EnemyKind::Shield)
          variant = e.tell > 0 ? 1 : 0; // the gun over the shield
        else if (e.kind == EnemyKind::Howler)
          variant = e.variant * 2 + (e.tell > 0 ? 1 : 0); // Dash's jacket; winding up a throw
        else if (e.kind == EnemyKind::Viper)
          variant = e.attach == 2 ? 1 : 0; // hanging from the branch
        else if (e.kind == EnemyKind::Cutter)
          variant = e.tell > 0 ? 1 : 0; // the machete up
        else if (e.kind == EnemyKind::Guardian)
          variant = e.tangle > 0 ? 3 : (e.dive > 0 ? 2 : (e.tell > 0 ? 1 : 0)); // club up, swinging; face down
        else if (e.kind == EnemyKind::DartFace)
          variant = e.tell > 0 ? 1 : 0; // the eyes glow
        else if (e.kind == EnemyKind::Scarabs)
          variant = e.carrier ? 1 : 0;
        else if (e.kind == EnemyKind::Wraith)
          variant = e.tell > 0 || e.dive > 0 ? 1 : 0; // smoking edges
        else if (e.kind == EnemyKind::Monk)
          variant = e.aimX > 0 ? 2 : (e.attach > 0 ? 3 : (e.tell > 0 || e.dive > 0 ? 1 : 0)); // frozen; lowered; planted
        else if (e.kind == EnemyKind::Moth || e.kind == EnemyKind::Bat)
          variant = 0;
        else if (e.kind == EnemyKind::Bandit || e.kind == EnemyKind::Mole)
          variant = e.tell > 0 || (e.kind == EnemyKind::Mole && e.timer >= 4 && e.timer < 11) ? 1 : 0; // pistol up; rock up
        else if (e.kind == EnemyKind::Toad)
          variant = e.attach == 2 || e.attach == 4 || e.attach == 5 ? 1 : 0; // in the air
        else if (e.kind == EnemyKind::Wisp)
          variant = e.carrier ? 1 : 0;
        else if (e.kind == EnemyKind::Crab)
          variant = (e.dive > 0 ? 2 : (e.tell > 0 ? 1 : 0)) + (e.variant ? 3 : 0); // claws open, flipped; party hat
        else if (e.kind == EnemyKind::Skitter)
          variant = e.attach == 1 ? 1 : (e.attach == 2 ? 2 : 0); // clicking, leaping
        else if (e.kind == EnemyKind::Loader)
          variant = (e.ox <= 0 ? 1 : 0) + (e.tell > 0 || e.aimX >= 0 ? 2 : 0); // legless; lifting a crate
        else if (e.kind == EnemyKind::WeldDrone)
          variant = e.attach != 0 ? 2 : (e.tell > 0 ? 1 : 0); // on a wall; the torch flares
        else if (e.kind == EnemyKind::Tether)
          variant = e.attach < 0 && e.tell > 0 ? 1 : 0; // wobbling before a ram
        else if (e.kind == EnemyKind::Puck && e.variant == 1)
          variant = 3; // Air Hockey's goalie
        else if (e.kind == EnemyKind::Puck || e.kind == EnemyKind::Mutant || e.kind == EnemyKind::LabArm)
          variant = std::min(e.attach, 3); // resting, spinning up, sliding; arms up, lunging; lamp red, claws
        else if (e.kind == EnemyKind::SleeperPod)
          variant = e.attach + (e.carrier ? 3 : 0); // frosted, thawed, empty; green frost
        else if (e.kind == EnemyKind::Puffer)
          variant = std::min(e.attach, 2) + (e.carrier ? 3 : 0) + (e.variant == 1 ? 6 : 0); // swelling, puffed; carrier; ceiling
        else if (e.kind == EnemyKind::Snapjaw)
          variant = std::min(e.attach, 2) + 3 * std::clamp(e.variant, 0, 3); // asleep, twitching, biting; facing
        else if (e.kind == EnemyKind::Glob)
          variant = e.attach == 2 ? 2 : (e.attach == 1 ? 1 : 0); // squashed, in the air
        else if (e.kind == EnemyKind::Barnacle)
          variant = std::clamp(e.attach, 0, 2) + 3 * std::clamp(e.variant, 0, 3); // shut, cracking, open; facing
        else if (e.kind == EnemyKind::EvaRam)
          variant = std::clamp(e.attach, 0, 3); // hovering, thruster hot, ramming, coasting
        else if (e.kind == EnemyKind::Spitpod)
          variant = e.tell > 0 ? 1 : 0; // the bulb swells
        else if (e.kind == EnemyKind::Gloop)
          variant = e.attach == 1 ? 2 : (e.tell > 0 ? 1 : 0); // squashed, in the air
        else if (e.kind == EnemyKind::Polyp)
          variant = e.attach; // shut, puckering, breathing in
        else if (e.kind == EnemyKind::Warden)
          variant = e.tell > 0 ? 1 : 0; // its belly glows while it calls
        else if (e.kind == EnemyKind::VoidRay)
          variant = e.attach; // gliding, fins lit, diving
        else if (e.kind == EnemyKind::RockLeech)
          variant = e.tell > 0 ? 1 : 0; // swelling
        else if (e.kind == EnemyKind::Blinker)
          variant = e.attach; // pacing, fading out, there and lunging
        else if (e.kind == EnemyKind::ShardGolem)
          variant = e.cool > 0 ? 1 : 0; // turning round
        else if (e.kind == EnemyKind::PrismBat)
          variant = 0;
        else if (e.kind == EnemyKind::LoomSpider)
          variant = e.tell > 0 ? 1 : 0; // fangs out, about to cut
        else if (e.kind == EnemyKind::CocoonPod)
          variant = e.cool > 0 ? 1 : 0; // split open, its Dropling out
        else if (e.kind == EnemyKind::Dropling)
          variant = e.attach == 2 ? 1 : 0; // biting
        else if (e.kind == EnemyKind::ThornHog)
          variant = e.stun > 0 ? 3 : (e.attach == 2 ? 2 : (e.attach == 1 ? 1 : 0)); // dazed, charging, pawing
        else if (e.kind == EnemyKind::SkyGulper)
          variant = e.cool > enemyDef(e.def).cooldown - 12 ? 1 : 0; // mouth shut on a gulp
        else if (e.kind == EnemyKind::Thornbush)
          variant = e.carrier ? 1 : 0; // the green one
        else if (e.kind == EnemyKind::EggGuard)
          variant = e.attach >= 2 ? 2 : e.attach; // an egg, prowling, horns down
        else if (e.kind == EnemyKind::SporeNurse)
          variant = 0;
        else if (e.kind == EnemyKind::SpearRunner)
          variant = e.attach == 2 ? 1 : 0; // the spear up
        else if (e.kind == EnemyKind::PitSnake)
          variant = e.attach == 3 ? 0 : e.attach; // coiled, hissing, reared
        else if (e.kind == EnemyKind::Totem)
          variant = e.tell; // which head's mouth glows
        else if (e.kind == EnemyKind::Drummer || e.kind == EnemyKind::Sentinel)
          variant = e.tell > 0 ? 1 : 0; // the drum on the beat, the glyphs lighting
        else if (e.kind == EnemyKind::CoinBeetle)
          variant = e.attach == 1 ? 1 : (e.attach == 2 ? 2 : 0); // rattling, hopping
        else if (e.kind == EnemyKind::FlipWalker)
          variant = e.dive > 0 ? 2 : (e.tell > 0 ? 1 : 0); // flailing in a fall; turning to face you
        else if (e.kind == EnemyKind::Probe)
          variant = e.tell > 0 ? 1 : 0; // the core glowing before a shot
        else if (e.kind == EnemyKind::TestSubject)
          variant = (e.dive > 0 ? 2 : (e.tell > 0 ? 1 : 0)) + (e.carrier ? 4 : 0) + 8 * (e.id % 4); // crouching; bib
        else if (e.kind == EnemyKind::Spark)
          variant = 0;
        else if (e.kind == EnemyKind::ShieldDrone)
        {
          variant = 0; // the emitter flares while its bubble blocks a hit
          for (const auto& g : mReactor.drones)
            if (g.enemy == int(&e - mEnemies.data()) && g.shimmer > 0)
              variant = 1;
        }
        else if (e.kind == EnemyKind::LatticeTurret)
          variant = e.tell > 0 ? 1 : 0; // the barrel glowing
        else if (e.kind == EnemyKind::RepairSwarm)
          variant = 0;
        else if (e.kind == EnemyKind::Duelist)
          variant = e.tell > kDuelDraw / 2 ? 2 // the hand over the holster, twitching
            : (e.tell > 0 || e.attach == 3 || (mWest.noon.on && mWest.noon.phase == DuelPhase::Fired) ? 1 : 0); // drawn
        else if (e.kind == EnemyKind::TumbleMine)
          variant = e.tell > 0 && (frame / 3) % 2 == 0 ? 1 : 0; // blinking red
        else if (e.kind == EnemyKind::WindowBandit)
          variant = 0;
        else if (e.kind == EnemyKind::PortraitGhost)
          variant = e.stun > 0 ? 4 : (e.attach == 2 ? 2 : (e.attach == 3 ? 3 : 1)); // tell, lunge; pinned
        else if (e.kind == EnemyKind::HauntedArmor)
          variant = e.dive > 0 ? 3 : (e.attach == 2 ? 2 : (e.attach == 1 && e.stun == 0 ? 1 : 0)); // walking, visor, swing
        else if (e.kind == EnemyKind::Poltergeist)
          variant = std::clamp(e.variant, 0, 2) + (e.tell > 0 && e.stun == 0 ? 3 : 0); // book, tureen, candelabra; rattling
        else if (e.kind == EnemyKind::Meatball)
          variant = std::clamp(e.variant, 0, 2) + (e.carrier ? 3 : 0); // small, big, giant; moldy
        else if (e.kind == EnemyKind::CleaverBot)
          variant = e.tell > 0 ? 1 : (e.cool > enemyDef(e.def).cooldown - 6 ? 2 : 0); // arm up; follow-through
        else if (e.kind == EnemyKind::EggTimer)
          variant = e.attach == 0 ? 24 // asleep; the dial, ringing at the end
                                  : std::clamp(e.cool * 12 / (kEggDial + 1), 0, 11) + (e.cool <= kEggRing ? 12 : 0);
        else if (e.kind == EnemyKind::Imp)
          variant = (e.aimX > 0 ? 3 : (e.dive > 0 ? 2 : (e.tell > 0 ? 1 : 0))) + (e.variant == 1 ? 4 : 0) +
            8 * std::clamp(e.attach, 0, 4); // glowing, lunging, saluting; hard hat; hotter after each pulse
        else if (e.kind == EnemyKind::SoldierAnt)
        {
          // Level 25: its column's tell (antennae up), angry (mandibles open); the sick column.
          const auto& cols = mGarden.columns;
          const bool inCol = e.attach >= 0 && e.attach < int(cols.size());
          const bool alarm = inCol && cols[std::size_t(e.attach)].tell > 0;
          const bool angry = !inCol || cols[std::size_t(e.attach)].hostile > 0;
          variant = (alarm ? 1 : (angry ? 2 : 0)) + (e.carrier ? 3 : 0);
        }
        else if (e.kind == EnemyKind::JumpingSpider)
          variant = std::clamp(e.attach, 0, 2); // crouched, leaping
        else if (e.kind == EnemyKind::BombardierBeetle)
          variant = e.attach == 1 ? 1 + std::clamp((enemyDef(e.def).tell - e.tell) * 3 / std::max(1, enemyDef(e.def).tell), 0, 2)
                                  : (e.cool > enemyDef(e.def).cooldown - 8 ? 4 : 0); // the abdomen swelling; just fired
        else if (e.kind == EnemyKind::ModelTank)
          variant = e.tell > 0 ? 1 : 0; // Level 26: the turret lifted and flashing
        else if (e.kind == EnemyKind::ToyJet)
          variant = e.tell > 0 ? 1 + (e.tell / 2) % 4 : 0; // spinning out
        else if (e.kind == EnemyKind::MaserTruck)
          variant = sectorDark(e.variant) ? 2 : (e.tell > 0 ? 1 : 0); // powerless; charging
        else if (e.kind == EnemyKind::GrinningBomb)
          variant = std::clamp(e.attach, 0, 2); // Level 28: grinning, the frown, the lunge
        else if (e.kind == EnemyKind::PianoBird)
          variant = e.cool <= 20 ? 0 : 1; // carrying a piano; let go of it
        else if (e.kind == EnemyKind::RubberGoon)
          variant = e.dive > 0 ? (e.aimY > 0 ? 4 : 3) : (e.attach > 0 ? 2 : (e.tell > 0 ? 1 : 0)); // flat; punching, coiled
        else if (e.stun > 0)
          variant = 0;
        int dirForArt = e.kind == EnemyKind::Crawler && variant == 0 ? -e.attach : e.dir;
        if (e.kind == EnemyKind::Barnacle)
          dirForArt = 1; // its art is turned to its surface already
        if (e.kind == EnemyKind::EvaRam && e.attach == 2)
        {
          // The ram's exhaust streaming out behind it.
          const float bx = x - float(e.dir) * float(e.w) * kCellPx * 0.5f, by = y - float(e.h) * kCellPx * 0.5f;
          for (int k = 1; k <= 4; ++k)
            drawGlow(r, mArt, bx - float(e.dir * k) * 26.0f, by, 30.0f - float(k) * 4.0f, rgb(255, 150, 60),
              0.55f - float(k) * 0.1f);
        }
        else if (e.kind == EnemyKind::EvaRam && e.attach == 1)
          drawGlow(r, mArt, x - float(e.dir) * float(e.w) * kCellPx * 0.5f, y - float(e.h) * kCellPx * 0.5f, 44.0f,
            rgb(255, 120, 50), (frame / 2) % 2 ? 0.8f : 0.4f);
        if (e.kind == EnemyKind::Probe)
        {
          // Its pull field: faint rings closing in on it, 16 cells out.
          const float pcx = x, pcy = y - float(e.h) * kCellPx * 0.5f;
          for (int k = 0; k < 4; ++k)
          {
            const float ph = std::fmod(float(k) * 0.25f + 1.0f - float(frame % 120) / 120.0f, 1.0f);
            const float rr = 16.0f * kCellPx * ph;
            const int a = int(60.0f * std::sin(ph * 3.14159f));
            if (rr < 40.0f || a <= 4)
              continue;
            const int segs = 48;
            for (int i = 0; i < segs; i += 2)
            {
              const float a0 = float(i) * 6.28318f / float(segs) + float(frame) * 0.004f, a1 = a0 + 6.28318f / float(segs);
              r.drawLine(pcx + std::cos(a0) * rr, pcy + std::sin(a0) * rr, pcx + std::cos(a1) * rr, pcy + std::sin(a1) * rr,
                2.0f, rgba(150, 80, 230, a));
            }
          }
          if (e.tell > 0)
            drawGlow(r, mArt, pcx, pcy, 70.0f, rgb(230, 140, 255), 0.5f + 0.3f * float((frame / 2) % 2));
        }
        if (e.kind == EnemyKind::Spark)
        {
          // Level 20: crackling, its light thrown on the wall round it.
          const float flick = 0.45f + 0.25f * float(hash2(e.id, frame / 2) % 100u) / 100.0f;
          drawGlow(r, mArt, x, y - float(e.h) * kCellPx * 0.5f, 90.0f, rgb(80, 170, 255), flick);
        }
        else if (e.kind == EnemyKind::Imp)
          drawGlow(r, mArt, x, y - float(e.h) * kCellPx * 0.45f, 60.0f + float(std::clamp(e.attach, 0, 4)) * 8.0f,
            e.attach >= 3 ? rgb(255, 240, 120) : rgb(150, 255, 70),
            e.tell > 0 ? 0.85f : 0.3f + 0.08f * float(std::clamp(e.attach, 0, 4)));
        const int animFrame = e.kind == EnemyKind::Bat ? (frame / 3 + e.aimX) % 2
          : e.kind == EnemyKind::Spark                 ? (frame / 2) % 4
          : e.kind == EnemyKind::Imp                   ? (frame / 4) % 2
          : e.kind == EnemyKind::RepairSwarm           ? (frame / 3) % 4
          : e.kind == EnemyKind::LatticeTurret         ? (frame / 6) % 3
          : e.kind == EnemyKind::Duelist               ? (frame / 2) % 2
          : e.kind == EnemyKind::PortraitGhost         ? (frame / 6) % 2
          : e.kind == EnemyKind::HauntedArmor          ? (e.attach == 1 ? (e.x / 3) % 2 : 0)
          : e.kind == EnemyKind::Poltergeist           ? (e.tell > 0 ? (frame / 2) % 2 : (frame / 10) % 2)
          : e.kind == EnemyKind::Meatball              ? ((e.x % 4) + 4) % 4 // the crust turns as it rolls
          : e.kind == EnemyKind::EggTimer              ? (e.attach != 0 && e.cool <= kEggRing ? (frame / 2) % 2 : (e.x / 2) % 2)
          : e.kind == EnemyKind::SoldierAnt            ? ((e.x % 4) + 4) % 4 // the march
          : e.kind == EnemyKind::BombardierBeetle      ? ((e.x % 2) + 2) % 2
          : e.kind == EnemyKind::ModelTank || e.kind == EnemyKind::MaserTruck ? ((e.x % 2) + 2) % 2 // tracks, wheels
          : e.kind == EnemyKind::ToyJet                ? (frame / 3) % 2
          : e.kind == EnemyKind::GrinningBomb          ? (e.attach == 0 ? (e.x / 2) % 2 : (frame / 2) % 2)
          : e.kind == EnemyKind::PianoBird             ? (frame / 4) % 2
          : e.kind == EnemyKind::RubberGoon            ? (e.dive > 0 ? (frame / 2) % 2 : (e.x / 2) % 2)
          : e.kind == EnemyKind::TumbleMine || e.kind == EnemyKind::WindowBandit ? 0
                                                       : (frame / 8) % 2;
        tex = &styledEnemySprite(mArt, r, mTheme, def.key, variant, animFrame, e.w, e.h).get(dirForArt);
        if ((e.kind == EnemyKind::FlipWalker || e.kind == EnemyKind::TestSubject) && e.attach == 2)
        {
          // Walking a ceiling (level 19): turned over top to bottom, feet up
          // on the box's top, still facing its way.
          tex = &styledEnemySprite(mArt, r, mTheme, def.key, variant, animFrame, e.w, e.h).get(-dirForArt);
          eo.angle = 180.0f;
          hop = float(e.h) * kCellPx;
        }
        if (e.kind == EnemyKind::Raver && e.dive > 0)
          hop = 14.0f; // hops on the beat
        if (e.kind == EnemyKind::PortraitGhost)
        {
          // Level 23: seen only in the reflection; in the real manor a
          // ghost is a faint warp in the air, and a black silhouette in the
          // lightning. Pinned, it is a ledge anyone can see.
          const bool unseen = !mManor.split.on && mManor.side == kSideReal && e.stun == 0;
          if (unseen && mManor.flash > 0)
          {
            eo.tint = rgb(8, 6, 16);
            eo.alpha = 0.9f;
          }
          else if (unseen)
            eo.alpha = e.attach == 2 || e.attach == 3 ? 0.2f : 0.1f;
          else
            eo.alpha = 0.9f;
          if (e.stun == 0)
            hop = 4.0f + std::sin(float(frame) * 0.1f + float(e.id)) * 5.0f; // hovering
        }
        if (e.kind == EnemyKind::Poltergeist && e.tell > 0 && e.stun == 0)
        {
          // The rattle: it lifts a cell and shakes.
          hop = kCellPx * std::min(1.0f, float(kPolterTell - e.tell + 1) / 4.0f);
          x += (frame / 2) % 2 ? 3.0f : -3.0f;
        }
        if ((e.kind == EnemyKind::PortraitGhost || e.kind == EnemyKind::HauntedArmor || e.kind == EnemyKind::Poltergeist) &&
            e.stun > 0 && e.stun <= 15 && (frame / 3) % 2 == 0)
          eo.alpha *= 0.35f; // a pin about to give
        if (e.kind == EnemyKind::HauntedArmor && e.dive > 0)
        {
          // The halberd's sweep: an arc three blocks deep in front of it.
          const float a = float(e.dive) / 6.0f;
          const float hx = x + float(e.dir) * float(e.w) * kCellPx * 0.3f, hy = y - float(e.h) * kCellPx * 0.6f;
          for (int k = 0; k < 9; ++k)
          {
            const float t0 = -1.1f + float(k) * 0.26f, t1 = t0 + 0.26f;
            const float rr = 3.0f * kTilePx;
            r.drawLine(hx + float(e.dir) * std::cos(t0) * rr, hy + std::sin(t0) * rr, hx + float(e.dir) * std::cos(t1) * rr,
              hy + std::sin(t1) * rr, 10.0f * a, rgba(230, 240, 255, int(150 * a)), Blend::Add);
          }
        }
        if (e.kind == EnemyKind::TumbleMine)
        {
          // Rolling: turned by how far it has gone, round its middle (the
          // sprite turns round its feet, so shift it to keep the middle put).
          const float rad = float(e.h) * kCellPx * 0.5f, a = e.drawX * kCellPx / rad;
          eo.angle = a * 57.2958f;
          x -= rad * std::sin(a);
          hop = rad - rad * std::cos(a);
          if (e.tell > 0)
            x += ((frame / 2) % 2 ? 2.0f : -2.0f); // rattling
        }
        if (e.kind == EnemyKind::Bouncer && e.dive < 0)
          x += ((frame / 2) % 2 ? 3.0f : -3.0f); // staggered
        if (e.kind == EnemyKind::Crawler && e.tell > 0)
          drawGlow(r, mArt, x, y - float(e.h) * kCellPx * 0.5f, 40, mTheme.enemyEye, 0.9f - float(e.tell) * 0.06f);
        if (e.kind == EnemyKind::Rider && e.tell > 0)
        {
          // The rail lights up before a sweep.
          const float rx0 = float(e.railX0) * kCellPx - camX;
          const float rx1 = float(e.railX1 + e.w) * kCellPx - camX;
          const float ry = y - float(e.h) * kCellPx - 6.0f;
          const float a = 0.5f + 0.5f * float((frame / 3) % 2);
          r.fillRect(rx0, ry, rx1 - rx0, 4.0f, withAlpha(rgb(255, 230, 90), int(200 * a)));
        }
        if (e.kind == EnemyKind::Sniper && e.tell > 0)
        {
          // The laser sight, cut by the first solid block.
          const CellBox b = e.box();
          const int eyeX = b.x + (e.dir > 0 ? b.w - 1 : 0), eyeY = b.y + 1;
          int hx = 0, hy = 0;
          lineOfFire(eyeX, eyeY, e.aimX, e.aimY, hx, hy);
          const float sx = (float(eyeX) + 0.5f) * kCellPx - camX, sy = (float(eyeY) + 0.5f) * kCellPx - camY;
          const float ex = (float(hx) + 0.5f) * kCellPx - camX, ey = (float(hy) + 0.5f) * kCellPx - camY;
          const bool holding = e.tell <= 9;
          r.drawLine(sx, sy, ex, ey, holding ? 4.0f : 2.0f, rgba(255, 40, 40, holding ? 230 : 150), Blend::Add);
          drawGlow(r, mArt, ex, ey, 16, rgb(255, 50, 50), 0.7f);
        }
        break;
      }
      case EnemyLook::Camera:
      {
        tex = &mArt.items[kIconCamera];
        DrawOpts co = eo;
        r.draw(*tex, x - 32, y - 64, co);
        if ((frame / 20) % 2 == 0)
          drawGlow(r, mArt, x - 17, y - 48, 16, rgb(255, 40, 50), 0.8f);
        if (e.flash > 0)
        {
          co.blend = Blend::Add;
          co.alpha = float(e.flash) / 8.0f;
          r.draw(*tex, x - 32, y - 64, co);
        }
        continue;
      }
    }
    if ((e.flags() & kEnemyCarrier) && e.kind != EnemyKind::Meatball && e.kind != EnemyKind::SoldierAnt) // green already
      eo.tint = lerpColor(eo.tint, rgb(120, 255, 80), 0.5f);
    if (e.tangle > 0 && e.kind != EnemyKind::Guardian)
    {
      // Toppled in the Snare Bolas' cords.
      eo.angle = 90.0f * float(e.tangleRoll < 0 ? -1 : 1);
      hop = float(e.w) * kCellPx * 0.5f; // lying on the floor, not half in it
    }
    r.draw(*tex, x, y - hop, eo);
    if (e.tangle > 0)
      for (int k = 0; k < 3; ++k) // the cords
        r.drawLine(x - 30.0f + float(k) * 22.0f, y - 6.0f, x - 14.0f + float(k) * 22.0f, y - 40.0f, 3.0f,
          rgb(150, 110, 60));
    if (e.flash > 0)
    {
      DrawOpts o;
      o.blend = Blend::Add;
      o.alpha = float(e.flash) / 8.0f;
      if (eo.angle == 180.0f)
        o.angle = eo.angle; // turned over on a ceiling (level 19)
      r.draw(*tex, x, y - hop, o);
    }
    if (e.stun > 0 && (e.kind == EnemyKind::HauntedArmor || e.kind == EnemyKind::Poltergeist))
    {
      // Level 23: the Silver Crossbow bolt that pins it, sticking out.
      const float bx = x - float(e.dir) * 6.0f, by = y - hop - float(e.h) * kCellPx * 0.55f;
      const float d = float(-e.dir);
      r.drawLine(bx, by, bx + d * 40.0f, by - 4.0f, 4.0f, rgb(20, 18, 30));
      r.drawLine(bx, by, bx + d * 40.0f, by - 4.0f, 2.0f, rgb(220, 226, 240));
      r.drawLine(bx + d * 32.0f, by - 3.0f, bx + d * 42.0f, by - 10.0f, 3.0f, rgb(250, 250, 255));
      r.drawLine(bx + d * 32.0f, by - 3.0f, bx + d * 42.0f, by + 3.0f, 3.0f, rgb(250, 250, 255));
    }
  }

  // Power cuts: the dark goes over the level but under the runner.
  drawDark(r, camX, camY, frame, alpha);
  if (mFlight)
    drawFlightShip(r, camX, camY, frame, alpha);
  else if (mPinball)
    drawPinball(r, camX, camY, frame, alpha);
  else if (mPlayer.vehicle < 0)
    drawPlayer(r, camX, camY, frame, alpha);
  drawVehicles(r, camX, camY, frame, alpha);
  // Sludge goes over the runner's feet and anything swimming in it.
  drawSludgeFront(r, camX, camY, frame, alpha);
  drawMaglevFront(r, camX, camY, frame, alpha);
  drawChopperFront(r, camX, camY, frame, alpha);
  drawJungleFront(r, camX, camY, frame, alpha);
  drawTempleFront(r, camX, camY, frame, alpha);
  drawLightFront(r, camX, camY, frame, alpha);
  drawMineFront(r, camX, camY, frame, alpha);
  drawLavaFront(r, camX, camY, frame, alpha);
  drawSpaceFront(r, camX, camY, frame, alpha);
  drawPlainsFront(r, camX, camY, frame, alpha);
  drawMotherFront(r, camX, camY, frame, alpha);
  drawHiveFront(r, camX, camY, frame, alpha);
  drawBoulderFront(r, camX, camY, frame, alpha);
  drawSanctumFront(r, camX, camY, frame, alpha);
  drawStationFront(r, camX, camY, frame, alpha);
  drawCryoFront(r, camX, camY, frame);
  if (mGreen.on)
    drawGreenFront(r, camX, camY, frame);
  if (mHull.on)
    drawHullFront(r, camX, camY, frame);
  if (mOrbit.on)
    drawOrbitFront(r, camX, camY, frame);
  if (mGrav.on)
    drawGravFront(r, camX, camY, frame);
  if (mReactor.on)
    drawReactorFront(r, camX, camY, frame);
  if (mZero.on)
    drawZeroFront(r, camX, camY, frame, alpha);
  if (mWest.on)
    drawWestFront(r, camX, camY, frame, alpha);
  if (mManor.on)
    drawManorFront(r, camX, camY, frame, alpha);
  if (mKitchen.on)
    drawKitchenFront(r, camX, camY, frame, alpha);
  if (mTurn.on)
    drawTurntableFront(r, camX, camY, frame, alpha);
  if (mGarden.on)
    drawGardenFront(r, camX, camY, frame, alpha);
  if (mGlide.on)
    drawGlideFront(r, camX, camY, frame, alpha);
  if (mCity.on)
    drawCityFront(r, camX, camY, frame, alpha);
  if (mNoir.on || mFilm.on)
    drawNoirFront(r, camX, camY, frame, alpha);
  if (mToon.on || mInk.on)
    drawToonFront(r, camX, camY, frame, alpha);
  if (mSignal.on)
    drawSignalFront(r, camX, camY, frame, alpha);
  if (mTrench.on)
    drawTrenchFront(r, camX, camY, frame, alpha);
  if (mStorm.on)
    drawStormFront(r, camX, camY, frame, alpha);
  if (mDish.on)
    drawDishFront(r, camX, camY, frame, alpha);
  if (mMagma.on)
    drawMagmaFront(r, camX, camY, frame, alpha);
  if (mFleet.on)
    drawFleetFront(r, camX, camY, frame, alpha);
  if (mTower.on)
    drawTowerFront(r, camX, camY, frame, alpha);
  if (mWarehouse.on)
    drawWarehouseFront(r, camX, camY, frame, alpha);

  // Projectiles.
  for (const auto& pr : mProjectiles)
  {
    const float cx = lerpCells(pr.prevX, pr.x, alpha) + float(pr.w) * kCellPx * 0.5f - camX;
    float cy = lerpCells(pr.prevY, pr.y, alpha) + float(pr.h) * kCellPx * 0.5f - camY;
    if (mZero.on && drawZeroShot(r, mArt, pr, cx, cy, frame))
      continue; // an Echo's shot or the Phase Rifle's (world_zero_draw.cpp)
    if (mSignal.on && drawSignalShot(r, mArt, pr, cx, cy, frame))
      continue; // a Tri-Tone shot in its tint (world_signal_draw.cpp)
    if (mTrench.on && drawTrenchShot(r, mArt, pr, cx, cy, frame))
      continue; // a harpoon and its line (world_trench_draw.cpp)
    if (mFleet.on && drawFleetShot(r, *this, mArt, pr, cx, cy, frame))
      continue; // the Grapple Gun's hook or a Sky Pirate's ball (world_fleet_draw.cpp)
    if (mStorm.on && drawStormShot(r, mArt, pr, cx, cy, frame))
      continue; // a Tesla rod (world_storm_draw.cpp)
    if (mMagma.on && drawMagmaShot(r, *this, mArt, pr, cx, cy, frame))
      continue; // a Heat Sink fireball or a Lava Daemon's (world_magma_draw.cpp)
    if (pr.thrown == 4)
    {
      // Level 24: a Cleaver Bot's cleaver, spinning end over end.
      const int d = pr.dx < 0 ? -1 : 1;
      const float a = float(frame + pr.age * 3) * 0.5f * float(d);
      const float rad = 32.0f;
      DrawOpts o;
      o.angle = a * 57.2958f;
      drawGlow(r, mArt, cx, cy, 40.0f, rgb(220, 230, 255), 0.25f);
      r.draw(styledEnemySprite(mArt, r, mTheme, "cleaver", 0, (frame / 3) % 2, 2, 2).get(d), cx - rad * std::sin(a),
        cy + rad * std::cos(a), o);
      continue;
    }
    if (pr.thrown > 0)
    {
      // Level 23: a Poltergeist's book, plate or candlestick, tumbling; a
      // carrier plate glows green.
      const float a = float(frame + pr.age * 3) * 0.35f * float(pr.dx < 0 ? -1 : 1);
      const float rad = 32.0f;
      DrawOpts o;
      o.angle = pr.thrown == 1 ? std::sin(a) * 20.0f : a * 57.2958f;
      const float ang = o.angle / 57.2958f;
      if (pr.carrier)
      {
        drawGlow(r, mArt, cx, cy, 70.0f, rgb(120, 255, 60), 0.6f + 0.2f * std::sin(float(frame) * 0.4f));
        o.tint = rgb(170, 255, 140);
      }
      else
        drawGlow(r, mArt, cx, cy, 40.0f, rgb(120, 255, 150), 0.25f);
      r.draw(styledEnemySprite(mArt, r, mTheme, "manor_thrown", std::clamp(pr.thrown - 1, 0, 2), (frame / 3) % 2, 2, 2)
               .get(pr.dx < 0 ? -1 : 1),
        cx - rad * std::sin(ang), cy + rad * std::cos(ang), o);
      continue;
    }
    if (pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::SilverCrossbow))
    {
      // Level 23: a silver crossbow bolt.
      const float d = float(pr.dx < 0 ? -1 : 1), by = cy - 6.0f;
      drawGlow(r, mArt, cx, by, 30.0f, rgb(220, 230, 255), 0.4f);
      r.drawLine(cx - d * 26.0f, by, cx + d * 22.0f, by, 5.0f, rgb(20, 18, 30));
      r.drawLine(cx - d * 26.0f, by, cx + d * 20.0f, by, 2.5f, rgb(230, 234, 246));
      r.drawLine(cx + d * 14.0f, by - 6.0f, cx + d * 26.0f, by, 3.0f, rgb(255, 255, 255));
      r.drawLine(cx + d * 14.0f, by + 6.0f, cx + d * 26.0f, by, 3.0f, rgb(255, 255, 255));
      r.drawLine(cx - d * 26.0f, by - 6.0f, cx - d * 16.0f, by, 3.0f, rgb(240, 240, 250));
      r.drawLine(cx - d * 26.0f, by + 6.0f, cx - d * 16.0f, by, 3.0f, rgb(240, 240, 250));
      continue;
    }
    if (pr.spike)
    {
      // A Space Barnacle's spike: a short dark needle along its flight.
      float vx = pr.vx, vy = pr.vy;
      if (vx == 0.0f && vy == 0.0f)
      {
        vx = float(pr.dx);
        vy = float(pr.dy);
      }
      const float len = std::max(0.001f, std::sqrt(vx * vx + vy * vy));
      const float ux = vx / len, uy = vy / len;
      drawGlow(r, mArt, cx, cy, 24.0f, rgb(255, 120, 210), 0.35f);
      r.drawLine(cx - ux * 16.0f, cy - uy * 16.0f, cx + ux * 14.0f, cy + uy * 14.0f, 6.0f, rgb(36, 26, 48));
      r.drawLine(cx - ux * 14.0f, cy - uy * 14.0f, cx + ux * 10.0f, cy + uy * 10.0f, 2.0f, rgb(170, 140, 190));
      r.drawLine(cx + ux * 8.0f, cy + uy * 8.0f, cx + ux * 18.0f, cy + uy * 18.0f, 2.5f, rgb(255, 200, 240));
      continue;
    }
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
        if (pr.strong && mReactor.on)
        {
          // Level 20: an enemy shot thrown back by the Deflector Bracer.
          glow = rgb(110, 210, 255);
          o.tint = rgb(190, 235, 255);
          o.scale = 1.3f;
        }
        break;
      case ShotKind::Laser:
        tex = &mArt.shotLaser;
        glow = rgb(80, 230, 255);
        break;
      case ShotKind::Rocket:
        tex = &mArt.shotRocket;
        glow = rgb(255, 140, 50);
        if (pr.precise)
          o.angle = std::atan2(pr.vy, pr.vx) * 57.2958f;
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
      case ShotKind::Proto:
      {
        tex = &mArt.shotLaser;
        glow = pr.proto >= 0 ? protoDef(pr.proto).color : mTheme.accentA;
        o.tint = lerpColor(glow, rgb(255, 255, 255), 0.4f);
        if (pr.proto == int(ProtoId::SparkDisc))
        {
          // A spinning ring of sparks rather than a bolt.
          tex = &mArt.enemyShot;
          o.scale = 1.6f;
          o.angle = float(frame * 40 % 360);
          o.blend = Blend::Add;
          for (int k = 0; k < 4; ++k)
          {
            const float a = float(frame) * 0.9f + float(k) * 1.5708f;
            r.fillRect(cx + std::cos(a) * 18.0f - 3.0f, cy + std::sin(a) * 18.0f - 3.0f, 6, 6,
              rgb(220, 250, 255), Blend::Add);
          }
        }
        if (pr.proto == int(ProtoId::BubbleGun))
        {
          // A wobbling soap bubble.
          const float wob = 1.0f + 0.08f * std::sin(float(frame) * 0.5f);
          r.draw(styledEnemySprite(mArt, r, mTheme, "bubble", 2, 0, 2, 2).get(1), cx, cy + 32.0f * wob);
          continue;
        }
        if (pr.proto == int(ProtoId::BileBlaster))
        {
          // A gob of bile with a trail of drops.
          const float s = pr.strong ? 1.4f : 1.0f;
          drawGlow(r, mArt, cx, cy, 34.0f * s, rgb(210, 240, 70), 0.6f);
          r.draw(styledEnemySprite(mArt, r, mTheme, "bile_blob", 0, (frame / 3) % 2, 1, 1).get(pr.dx < 0 ? -1 : 1), cx,
            cy + 16.0f);
          continue;
        }
        if (pr.proto == int(ProtoId::GooGun))
        {
          // A wobbling glob of goo (world_space.cpp splats it on walls).
          r.draw(styledEnemySprite(mArt, r, mTheme, "goo_blob", 0, (frame / 4) % 2, 1, 1).get(pr.dx < 0 ? -1 : 1), cx,
            cy + 16.0f);
          continue;
        }
        if (pr.proto == int(ProtoId::SnareBolas))
        {
          // Two stone weights on a spinning cord.
          const float a = float(frame) * 0.9f;
          const float ex = std::cos(a) * 22.0f, ey = std::sin(a) * 22.0f;
          r.drawLine(cx - ex, cy - ey, cx + ex, cy + ey, 3.0f, rgb(150, 110, 60));
          for (const float s : {-1.0f, 1.0f})
          {
            r.fillRect(cx + s * ex - 7.0f, cy + s * ey - 7.0f, 14.0f, 14.0f, rgb(120, 96, 70));
            r.fillRect(cx + s * ex - 4.0f, cy + s * ey - 7.0f, 8.0f, 4.0f, rgb(180, 150, 110));
          }
          continue;
        }
        if (pr.proto == int(ProtoId::StarSeed))
        {
          // A slow spinning star of gold light, bigger for a full charge.
          const float s = pr.strong ? 1.5f : 1.0f + 0.1f * float(pr.damage - 2);
          drawGlow(r, mArt, cx, cy, 60.0f * s, rgb(255, 220, 120), 0.8f);
          for (int k = 0; k < 4; ++k)
          {
            const float a = float(frame) * 0.15f + float(k) * 0.785f;
            const float len = (k % 2 ? 16.0f : 30.0f) * s;
            r.drawLine(cx - std::cos(a) * len, cy - std::sin(a) * len, cx + std::cos(a) * len, cy + std::sin(a) * len,
              k % 2 ? 3.0f : 5.0f, rgb(255, 246, 200), Blend::Add);
          }
          drawGlow(r, mArt, cx, cy, 18.0f * s, rgb(255, 255, 255), 1.0f);
          continue;
        }
        if (pr.proto == int(ProtoId::Boomerang))
        {
          // A spinning wooden V.
          const float a = float(frame) * 0.7f;
          const Color wood = rgb(214, 160, 84);
          for (const float arm : {0.0f, 2.1f})
            r.drawLine(cx, cy, cx + std::cos(a + arm) * 26.0f, cy + std::sin(a + arm) * 26.0f, 9.0f, wood);
          r.fillRect(cx - 5.0f, cy - 5.0f, 10.0f, 10.0f, rgb(240, 200, 120));
          continue;
        }
        if (pr.proto == int(ProtoId::DeflectorBracer))
        {
          // The Bracer's pulse shot: a small rippling ring of blue energy.
          drawGlow(r, mArt, cx, cy - 6.0f, 46.0f, rgb(90, 200, 255), 0.6f);
          const float rr = 13.0f + 2.5f * std::sin(float(frame) * 0.7f);
          for (int k = 0; k < 16; ++k)
          {
            const float a0 = float(k) * 0.3927f, a1 = a0 + 0.3927f;
            r.drawLine(cx + std::cos(a0) * rr * 0.7f, cy - 6.0f + std::sin(a0) * rr, cx + std::cos(a1) * rr * 0.7f,
              cy - 6.0f + std::sin(a1) * rr, 4.0f, rgb(150, 225, 255), Blend::Add);
          }
          drawGlow(r, mArt, cx, cy - 6.0f, 10.0f, rgb(255, 255, 255), 0.8f);
          continue;
        }
        if (pr.proto == int(ProtoId::LockOnRockets))
        {
          tex = &mArt.shotRocket;
          if (pr.precise)
            o.angle = std::atan2(pr.vy, pr.vx) * 57.2958f;
        }
        if (pr.strong)
        {
          o.scale = 1.5f;
          drawGlow(r, mArt, cx, cy - 6.0f, 70, rgb(255, 255, 255), 0.5f);
        }
        break;
      }
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

  drawSeaFront(r, camX, camY, frame);
  drawProps(r, camX, camY, frame, true);

  if (mSplitPass != 0)
    return; // Both Sides: the HUD goes over both halves (drawSplit)
  r.draw(mArt.vignette, 0.0f, 0.0f);
  drawHud(r, frame);
  drawVehicleHud(r, frame);
  drawAirHud(r, frame);
  drawBeatHud(r, frame);
  drawClubHud(r, frame);
  drawTideHud(r, frame);
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
      if (mLayerMask[std::size_t(ty * mLevel->width + tx)])
        continue; // a creeper draws itself (level 17)
      if (t == Tile::Ladder)
        r.draw(mArt.ladder, x, y);
      else if (t == Tile::Pipe)
        r.draw(mArt.pipe, x, y);
    }
  }

  const Texture& platformTex = mLevel->flag("clouds") ? mArt.cloud : mArt.platform;
  for (int ty = ty0; ty <= ty1; ++ty)
  {
    for (int tx = tx0; tx <= tx1; ++tx)
    {
      const float x = float(tx) * kTilePx - camX;
      const float y = float(ty) * kTilePx - camY;
      if (mLayerMask[std::size_t(ty * mLevel->width + tx)])
        continue; // drawn by its layer
      bool prop = false; // a mirror ball or speaker draws itself
      for (const auto& b : mBreakables)
        prop = prop || (b.look != 0 && b.look != 3 && b.look != 6 && !b.broken && tx >= b.x0 && tx <= b.x1 && ty >= b.y0 && ty <= b.y1);
      if (prop)
        continue;
      switch (mMap.block(tx, ty))
      {
        case Tile::Solid:
        {
          if (mZero.wireframe)
            break; // the Wireframe bonus draws its own (world_zero_draw.cpp)
          if (mWarehouse.on && warehouseDrawsBlock(tx, ty))
            break; // a metal crate (world_warehouse_draw.cpp)
          if (mCity.on && cityDrawsBlock(tx, ty))
            break; // a model building (world_city_draw.cpp)
          if ((mToon.on || mInk.on) && toonDrawsBlock(*this, tx, ty))
            break; // a piano standing (world_toon_draw.cpp)
          // Shade blocks darker the deeper they sit below the surface.
          int depth = 0;
          while (depth < 3 && (mMap.block(tx, ty - depth - 1) == Tile::Solid) && !(mCity.on && cityDrawsBlock(tx, ty - depth - 1)))
            ++depth;
          static constexpr int kShade[4] = {255, 210, 175, 145};
          // (Level 35's turning tower: by the block's own x, not where it has turned to.)
          const int hx = mTower.wrap ? (((tx - mTower.rot / kCellsPerTile) % mLevel->width) + mLevel->width) % mLevel->width : tx;
          const auto h = (hash2(hx, ty) >> 8) % 10u;
          DrawOpts o;
          o.tint = rgb(kShade[depth], kShade[depth], kShade[depth]);
          r.draw(mArt.solid[h < 7u ? 0u : (h < 9u ? 1u : 2u)], x, y, o);
          break;
        }
        case Tile::Platform:
          if ((mToon.on || mInk.on) && toonDrawsBlock(*this, tx, ty))
            break; // an inked line (world_toon_draw.cpp)
          r.draw(platformTex, x, y);
          break;
        case Tile::Grate:
          r.draw(styledEnemySprite(mArt, r, mTheme, "grate", 0, 0, 2, 2).get(1), x + 32.0f, y + 64.0f);
          break;
        case Tile::Spikes:
          r.draw(mArt.spikes, x, y);
          break;
        case Tile::SpikesDown:
        {
          // Hanging from a ceiling (level 19): the floor spikes turned over.
          DrawOpts o;
          o.angle = 180.0f;
          r.draw(mArt.spikes, x + 64.0f, y + 64.0f, o);
          break;
        }
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
      if (mMap.block(tx, ty) == Tile::Solid && (mMap.block(tx, ty - 1) != Tile::Solid || (mCity.on && cityDrawsBlock(tx, ty - 1)) ||
                                                  ((mToon.on || mInk.on) && toonDrawsBlock(*this, tx, ty - 1))) &&
          !(mCity.on && cityDrawsBlock(tx, ty)) && !(mWarehouse.on && warehouseDrawsBlock(tx, ty)) &&
          !((mToon.on || mInk.on) && toonDrawsBlock(*this, tx, ty)) &&
          mMap.block(tx, ty - 1) != Tile::Spikes && !mLayerMask[std::size_t(ty * mLevel->width + tx)] && !mZero.wireframe)
        r.draw(mArt.solidTop, float(tx) * kTilePx - camX, float(ty) * kTilePx - camY);
}

void World::drawPlayer(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& p = mPlayer;
  if (p.hidden || p.tube >= 0)
    return; // in a Gullet Tube, drawHiveFront draws you
  if (mTop.on)
  {
    drawTopRunner(r, camX, camY, frame, alpha); // seen from above (world_city_draw.cpp)
    return;
  }
  // Mercy frames: Duke blinks the sprite on and off, then flashes it white.
  // Strobing at the logic rate looks harsh in HD, so the runner turns
  // see-through with a gentle pulse instead, and glows white at the end.
  const bool ghost = p.mercy > 10;
  const bool flashWhite = p.mercy > 0 && p.mercy <= 10;
  const float pulse = 0.5f + 0.5f * std::sin(float(frame) * 0.35f);

  const auto& ca = mArt.runner(mCharacter);
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
      if (mBreakdance)
      {
        // Windmills: a 24-frame spin on the floor.
        spr = &ca.tuck;
        lift = 30.0f * 1.65f;
        o.angle = float(p.facing) * float(frame % 96) * 3.75f;
      }
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
    case PlayerVisual::Clinging:
      spr = &ca.cling;
      break;
    case PlayerVisual::Dying:
      spr = &ca.hurt;
      if (p.deathPhase == 2)
        o.angle = float(-p.facing) * 80.0f;
      break;
  }

  float x = lerpCells(p.prevX, p.x, alpha) + 1.5f * kCellPx - camX;
  float y = lerpCells(p.prevY + 1, p.y + 1, alpha) - camY - lift;
  if (p.state == PlayerState::Swing && p.vine >= 0 && std::size_t(p.vine) < mVines.size())
  {
    // Hanging along the vine, hands on it, at the in-between time.
    const Vine& v = mVines[std::size_t(p.vine)];
    float t = float(v.t) - 1.0f + alpha;
    if (t < 0.0f)
      t += float(v.period);
    const float a = float(v.amp) * std::cos(6.2831853f * t / float(v.period)) * 0.0174533f;
    const float hx = float(v.ax) + float(p.vineAt) * std::sin(a), hy = float(v.ay) + float(p.vineAt) * std::cos(a);
    x = (hx + 5.6f * std::sin(a)) * kCellPx - camX;
    y = (hy + 5.6f * std::cos(a)) * kCellPx - camY;
    o.angle = -a * 57.2958f;
  }
  if (p.cart >= 0 && std::size_t(p.cart) < mCarts.size())
  {
    // Standing (or ducking) in a mine cart, tilted with the track.
    const Cart& c = mCarts[std::size_t(p.cart)];
    float da = c.angle - c.prevAngle;
    if (da > 3.14159f)
      da -= 6.28318f;
    else if (da < -3.14159f)
      da += 6.28318f;
    const float a = c.prevAngle + da * alpha;
    const float fx = c.prevFx + (c.fx - c.prevFx) * alpha, fy = c.prevFy + (c.fy - c.prevFy) * alpha;
    const float off = p.cartDuck ? 0.0f : 1.0f;
    x = (fx + std::sin(a) * off) * kCellPx - camX;
    y = (fy - std::cos(a) * off) * kCellPx - camY;
    o.angle = a * 57.2958f;
  }
  if (mTurn.on)
  {
    // Turntable: upright on screen (gravity stays down), feet under the middle.
    x = (mTurn.prevSx + (mTurn.sx - mTurn.prevSx) * alpha) * kCellPx - camX;
    y = (mTurn.prevSy + (mTurn.sy - mTurn.prevSy) * alpha + 2.0f) * kCellPx - camY;
  }
  if (mOrbit.on)
  {
    // Planetoids: standing out from the little world's surface, feet on it.
    x = mOrbit.px * kCellPx - camX;
    y = mOrbit.py * kCellPx - camY;
    o.angle = mOrbit.ang * 57.2958f;
  }

  // Level 19: a runner whose down is not down. Up hangs from the ceiling:
  // the sprite turned over top to bottom (the other facing's art turned
  // 180 degrees, so left stays left) with its feet on the box's top row;
  // Left and Right stand on a wall, turned a quarter, feet on that wall.
  // (ux, uy) is the runner's up on screen, (tx, ty) where the turbo trail's
  // points (the box's bottom middle, upright) put the feet.
  float ux = 0.0f, uy = -1.0f, tx = 0.0f, ty = 0.0f;
  int face = p.facing;
  if (p.grav != Grav::Down && !mOrbit.on)
  {
    const float bx = lerpCells(p.prevX, p.x, alpha) - camX, by = lerpCells(p.prevY, p.y, alpha) - camY;
    switch (p.grav)
    {
      case Grav::Up:
        x = bx + 1.5f * kCellPx;
        y = by + lift;
        ux = 0.0f;
        uy = 1.0f;
        ty = -kCellPx;
        o.angle += 180.0f;
        face = -p.facing;
        break;
      case Grav::Left:
        x = bx + lift;
        y = by + 1.5f * kCellPx;
        ux = 1.0f;
        uy = 0.0f;
        tx = -1.5f * kCellPx;
        ty = 0.5f * kCellPx;
        o.angle += 90.0f;
        break;
      default: // Right
        x = bx + kCellPx - lift;
        y = by + 1.5f * kCellPx;
        ux = -1.0f;
        uy = 0.0f;
        tx = -0.5f * kCellPx;
        ty = 0.5f * kCellPx;
        o.angle -= 90.0f;
        break;
    }
    lift = 0.0f;
  }

  if (p.state == PlayerState::Jetpack)
  {
    drawGlow(r, mArt, x, y + 4, 60 + 10 * std::sin(float(frame) * 0.8f), rgb(255, 140, 40), 0.8f);
    drawGlow(r, mArt, x, y + 2, 20, rgb(255, 255, 220), 1.0f);
  }

  const Texture& tex = spr->get(face);
  if (mGreen.grow)
    o.scale *= mGreen.scale(); // Growth Spurt
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
    drawGlow(r, mArt, x + ux * 80.0f, y + uy * 80.0f, 130 * (1.0f - t * 0.5f), mTheme.accentB, 0.6f);
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
      r.draw(tex, mTrail[i].x * S - camX + tx, mTrail[i].y * S - camY - lift + ty, t);
    }
    drawGlow(r, mArt, x + ux * 80.0f, y + uy * 80.0f, 120 + 14 * std::sin(float(frame) * 0.4f), rgb(255, 160, 40), 0.6f);
  }
  if (p.virus > 0)
  {
    o.tint = rgb(150, 255, 120);
    drawGlow(r, mArt, x + ux * 80.0f, y + uy * 80.0f, 90, rgb(110, 255, 60), 0.25f + 0.1f * pulse);
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
    float my = y - m[1] * kCellPx;
    if (uy != -1.0f)
    {
      // Turned (level 19): the same point on the turned sprite, the muzzle's
      // forward along the screen's x (Up) or the wall (Left, Right).
      const float fwd = mx - x, up = y - my;
      if (uy > 0.0f)
      {
        mx = x + fwd;
        my = y + up;
      }
      else
      {
        mx = x + ux * up;
        my = y + ux * fwd;
      }
    }
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
  r.drawText(mCharacter.name, x + 16, top + 6, label);
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
  const bool proto = p.weapon == Weapon::Proto && p.proto >= 0;
  if (proto && mTrench.on && p.proto == int(ProtoId::HarpoonLine))
    drawHarpoonIcon(r, mArt, x + 18, top + 20); // Level 30: a speargun (world_trench_draw.cpp)
  else if (proto && mStorm.on && p.proto == int(ProtoId::TeslaRod))
    drawTeslaIcon(r, mArt, x + 18, top + 20, 0.62f, frame); // Level 31: a copper rod (world_storm_draw.cpp)
  else if (proto && mFleet.on && p.proto == int(ProtoId::GrappleGun))
    drawGrappleIcon(r, mArt, x + 18, top + 20, 0.62f, frame); // Level 34: a line gun (world_fleet_draw.cpp)
  else if (proto && mDish.on && p.proto == int(ProtoId::SignalJammer))
    drawJammerIcon(r, mArt, x + 18, top + 20, 0.62f, frame); // Level 32: a handheld jammer (world_dish_draw.cpp)
  else if (proto && mMagma.on && p.proto == int(ProtoId::HeatSink))
    drawSinkIcon(r, mArt, x + 18, top + 20, 0.62f, frame); // Level 33: a heat sink (world_magma_draw.cpp)
  else if (proto)
  {
    DrawOpts io;
    io.scale = 0.62f;
    io.tint = lerpColor(protoDef(p.proto).color, rgb(255, 255, 255), 0.3f);
    r.draw(mArt.items[kIconProto], x + 18, top + 20, io);
  }
  else if (icon >= 0)
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
  if (proto)
  {
    // Long names (HEDGE TRIMMER, LOCK-ON ROCKETS) shrink to clear the ammo count.
    const std::string name = protoDef(p.proto).name;
    const float size = name.size() > 11 ? 17.0f * 11.0f / float(name.size()) : 17.0f;
    r.drawText(name, x + 74, top + 26 + (17.0f - size) * 0.5f, {size, mTheme.hudText, kHudInk});
  }
  else
    r.drawText(weaponName(p.weapon), x + 74, top + 24, {22.0f, mTheme.hudText, kHudInk});
  if (p.weapon == Weapon::Normal)
  {
    r.drawText("INF", x + kHudWeaponW - 16, top + 24, {22.0f, mTheme.accentB, kHudInk}, Align::Right);
  }
  else
  {
    std::snprintf(buf, sizeof(buf), "%d", p.ammo);
    r.drawText(buf, x + kHudWeaponW - 16, top + 24, {22.0f, mTheme.accentB, kHudInk}, Align::Right);
    const float frac = float(p.ammo) / float(proto ? protoDef(p.proto).maxAmmo : maxAmmo(p.weapon));
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
  drawTempleHud(r, x + 68.0f, top); // Level 9's stone keys, in the second slot
  drawLightHud(r, frame);           // Negative Space's sun and moon
  drawMineHud(r, frame);            // Pinball Mine's lanterns

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

  // Black Halo's armour, phase by phase, along the bottom.
  if (bossFight())
  {
    const float bw = 600.0f, bx = (float(kScreenW) - bw) * 0.5f, by = float(kScreenH) - 54.0f;
    r.fillRect(bx - 10, by - 30, bw + 20, 52, rgba(8, 6, 22, 200));
    r.drawText("BLACK HALO", bx, by - 26, {17.0f, rgb(255, 90, 60), kHudInk});
    const float frac = float(mBoss.total()) / 82.0f;
    r.fillRect(bx, by, bw, 12, rgba(255, 255, 255, 40));
    r.fillRect(bx, by, bw * frac, 12, (mBoss.flash > 0 && (frame / 2) % 2) ? rgb(255, 255, 255) : rgb(255, 70, 50));
    for (const float cut : {24.0f / 82.0f, 54.0f / 82.0f})
      r.fillRect(bx + bw * cut - 1.0f, by - 3, 3, 18, rgb(20, 16, 30));
  }

  drawSanctumHud(r, frame);
  drawMotherHud(r, frame);
  drawStationHud(r, frame);
  drawCryoHud(r, frame);
  if (mGreen.on)
    drawGreenHud(r, frame);
  if (mHull.on)
    drawHullHud(r, frame);
  if (mGrav.on)
    drawGravHud(r, frame);
  if (mReactor.on)
    drawReactorHud(r, frame);
  if (mZero.on)
    drawZeroHud(r, frame);
  if (mWest.on)
    drawWestHud(r, frame);
  if (mManor.on)
    drawManorHud(r, frame);
  if (mKitchen.on)
    drawKitchenHud(r, frame);
  if (mCity.on)
    drawCityHud(r, frame);
  if (mNoir.on || mFilm.on)
    drawNoirHud(r, frame);
  if (mToon.on || mInk.on)
    drawToonHud(r, frame);
  if (mSignal.on && mSignal.air)
    drawSignalHud(r, frame);
  if (mTrench.on)
    drawTrenchHud(r, frame);
  if (mStorm.on)
    drawStormHud(r, frame);
  if (mDish.swapLr)
    drawDishHud(r, frame);
  if (mMagma.on)
    drawMagmaHud(r, frame);
  if (mFleet.on)
    drawFleetHud(r, frame);
  if (mTower.on)
    drawTowerHud(r, frame);
  if (mWarehouse.on)
    drawWarehouseHud(r, frame);
  if (mGarden.on)
    drawGardenHud(r, frame);
  if (mGolden)
    drawGoldenHud(r, frame);

  // Bonus level countdown.
  if (mBonusLevel && mLevel->timer > 0)
  {
    const int perSecond = mMagma.overclock ? 45 : 15; // Overclock runs three frames to one
    std::snprintf(buf, sizeof(buf), "%d", (mBonusFramesLeft + perSecond - 1) / perSecond);
    const bool hurry = mBonusFramesLeft < 150 && (frame / 10) % 2 == 0;
    r.fillRect(float(kScreenW) / 2.0f - 70.0f, 84, 140, 52, rgba(8, 6, 22, 190));
    r.drawText(buf, float(kScreenW) / 2.0f, 88, {40.0f, hurry ? rgb(255, 80, 80) : mTheme.accentA, kHudInk, true}, Align::Center);
  }

  // Pickup and tutorial messages, like Duke's message line.
  if (mMessageTicks > 0)
  {
    const float a = std::min(1.0f, float(mMessageTicks) / 20.0f);
    r.drawText(mMessage, float(kScreenW) / 2.0f, 92.0f, {26.0f, rgb(255, 255, 255), kHudInk}, Align::Center, a);
  }
}

} // namespace gr
