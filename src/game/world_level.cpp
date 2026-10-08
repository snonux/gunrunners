// Level entities from the [entities] section, the music clock, switchable
// tile layers (SPEC 3.1), props, easter eggs and the bonus level rules.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace gr
{

namespace
{

constexpr float S = kPixelScale;
constexpr float kCellPx = float(kCellSize) * kPixelScale;
constexpr float kTilePx = float(kTileSize) * kPixelScale;
constexpr Color kInk = rgb(10, 8, 20);
constexpr int kBeatStart[4] = {0, 8, 15, 23};
constexpr int kTelegraph = 8; // frames of warning before a layer goes dark

const Color kSignColors[] = {
  rgb(255, 60, 200), rgb(0, 240, 255), rgb(255, 230, 60), rgb(120, 255, 120), rgb(255, 120, 40)};

Color namedColor(const std::string& name, Color def)
{
  if (name == "pink")
    return kSignColors[0];
  if (name == "cyan")
    return kSignColors[1];
  if (name == "yellow")
    return kSignColors[2];
  if (name == "green")
    return kSignColors[3];
  if (name == "orange")
    return kSignColors[4];
  return def;
}

} // namespace

int beatOfFrame(int frame)
{
  const int f = ((frame % 30) + 30) % 30;
  return f < 8 ? 0 : (f < 15 ? 1 : (f < 23 ? 2 : 3));
}

int beatStartFrame(int beat) { return kBeatStart[std::clamp(beat, 0, 3)]; }

int framesIntoBeat(int frame)
{
  const int f = ((frame % 30) + 30) % 30;
  return f - kBeatStart[beatOfFrame(f)];
}

std::unique_ptr<World> World::cloneForSim() const
{
  auto w = std::make_unique<World>(*this);
  w->mSimulation = true;
  w->mParticles.clear();
  w->mTexts.clear();
  w->mFlashes.clear();
  w->mSounds.clear();
  return w;
}

void World::spawnEnemy(int def, int x, int y)
{
  if (def < 0)
    return;
  const EnemyDef& d = enemyDef(def);
  Enemy e;
  e.kind = d.kind;
  e.def = def;
  e.x = e.prevX = x;
  e.y = e.prevY = y;
  e.w = d.w;
  e.h = d.h;
  e.hp = d.hp;
  e.id = int(mEnemies.size());
  e.timer = (x / kCellsPerTile) * 7;
  mEnemies.push_back(e);
}

void World::setupEntities()
{
  const Level& lv = *mLevel;
  mBonusLevel = !lv.rules.empty() || lv.timer > 0;
  mAirJump = lv.rules.find("airjump") != std::string::npos;
  mFreeFall = lv.rules.find("freefall") != std::string::npos;
  mBonusFramesLeft = lv.timer * 15;
  mHasBeat = mLevelProto == int(ProtoId::PulsePistol);

  for (const auto& e : lv.entities)
  {
    // Blocks to cells: things stand on the bottom row of their block.
    const int cx = e.x * kCellsPerTile;
    const int cy = e.y * kCellsPerTile + 1;
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    const bool hasRect = e.rect("rect", x0, y0, x1, y1);
    const CellBox rectCells{x0 * kCellsPerTile, y0 * kCellsPerTile, (x1 - x0 + 1) * kCellsPerTile,
      (y1 - y0 + 1) * kCellsPerTile};

    if (e.kind == "layer" && hasRect)
    {
      Layer l;
      l.id = e.id;
      l.x0 = x0;
      l.y0 = y0;
      l.x1 = x1;
      l.y1 = y1;
      const std::string tile = e.str("tile", "#");
      l.tile = tile == "=" ? Tile::Platform : Tile::Solid;
      const std::string driver = e.str("driver", "beat");
      if (driver == "timer")
        l.driver = LayerDriver::Timer;
      else if (driver == "switch")
        l.driver = LayerDriver::Switch;
      else if (driver == "script")
        l.driver = LayerDriver::Script;
      else if (driver == "lit")
        l.driver = LayerDriver::Lit;
      else
        l.driver = LayerDriver::Beat;
      l.bars = std::clamp(e.num("bars", 1), 1, 2);
      for (int b : e.list("beats"))
        if (b >= 1 && b <= 8)
          l.beats[std::size_t(b - 1)] = true;
      l.on = e.num("on", 15);
      l.off = e.num("off", 15);
      l.phase = e.num("phase", 0);
      l.switchId = e.str("switch");
      l.switchState = e.num("state", 1);
      l.style = e.str("style", "sign") == "sign" ? 0 : 1;
      l.color = namedColor(e.str("color"), kSignColors[mLayers.size() % 3]);
      l.solid = true;
      for (int ty = y0; ty <= y1; ++ty)
        for (int tx = x0; tx <= x1; ++tx)
          if (tx >= 0 && ty >= 0 && tx < lv.width && ty < lv.height)
            mLayerMask[std::size_t(ty * lv.width + tx)] = 1;
      if (l.driver == LayerDriver::Beat)
        mHasBeat = true;
      mLayers.push_back(l);
      continue;
    }

    const int def = enemyIndex(e.kind == "enemy" ? e.str("kind") : e.kind);
    if (def >= 0 && (e.hasPos || hasRect))
    {
      spawnEnemy(def, hasRect ? rectCells.x : cx, hasRect ? rectCells.y + enemyDef(def).h - 1 : cy);
      Enemy& en = mEnemies.back();
      if (e.str("dir") == "r")
        en.dir = 1;
      en.carrier = e.num("carrier", 0) != 0;
      switch (en.kind)
      {
        case EnemyKind::Crawler:
          placeClinger(en);
          break;
        case EnemyKind::Rider:
          // rect= is the rail: the drone's rows, x0..x1 its ends.
          en.railX0 = rectCells.x;
          en.railX1 = rectCells.x + rectCells.w - en.w;
          en.x = en.prevX = e.str("start") == "r" ? en.railX1 : en.railX0;
          break;
        default:
          break;
      }
      continue;
    }

    if (e.kind == "platform" && e.hasPos)
    {
      setupPlatform(e);
      continue;
    }
    if (e.kind == "rope" && e.hasPos)
    {
      mRopes.push_back({cx, e.y * kCellsPerTile, kCellsPerTile, e.num("h", 6) * kCellsPerTile});
      continue;
    }
    if (e.kind == "hatch" && e.hasPos)
    {
      mHatches.push_back({e.x, e.y, false, e.str("tile", "H") == "=" ? Tile::Platform : Tile::Ladder});
      continue;
    }
    if (e.kind == "breakable" && hasRect)
    {
      Breakable b;
      b.x0 = x0;
      b.y0 = y0;
      b.x1 = x1;
      b.y1 = y1;
      b.hp = e.num("hp", 1);
      const std::string by = e.str("by", "any");
      b.by = by == "explosion" ? 1 : (by == "heavy" ? 2 : 0);
      mBreakables.push_back(b);
      continue;
    }
    if (e.kind == "spawner" && e.hasPos)
    {
      Spawner s;
      s.def = enemyIndex(e.str("enemy"));
      s.x = cx;
      s.y = cy;
      s.onto = e.str("onto");
      if (s.def >= 0)
        mSpawners.push_back(s);
      continue;
    }

    if (e.kind == "deco" || e.kind == "billboard")
    {
      Prop pr;
      const std::string kind = e.kind == "billboard" ? "billboard" : e.str("kind");
      pr.x = e.x * kCellsPerTile;
      pr.y = e.y * kCellsPerTile;
      pr.w = e.num("w", 2) * kCellsPerTile;
      pr.h = e.num("h", 1) * kCellsPerTile;
      if (hasRect)
      {
        pr.x = rectCells.x;
        pr.y = rectCells.y;
        pr.w = rectCells.w;
        pr.h = rectCells.h;
      }
      pr.text = e.str("text");
      if (kind == "42")
      {
        pr.kind = PropKind::Deco42;
        if (pr.text.empty())
          pr.text = "42";
      }
      else if (kind == "billboard")
        pr.kind = PropKind::Billboard;
      else if (kind == "ufo_flyby")
        pr.kind = PropKind::UfoFlyby;
      else if (kind == "reflection")
        pr.kind = PropKind::Reflection;
      else
        pr.kind = PropKind::TextSign;
      mProps.push_back(pr);
      continue;
    }

    if (e.kind == "wind" && hasRect)
    {
      Zone z;
      z.kind = ZoneKind::Wind;
      z.box = rectCells;
      const std::string dir = e.str("dir", "l");
      z.dx = dir == "l" ? -1 : (dir == "r" ? 1 : 0);
      z.dy = dir == "u" ? -1 : (dir == "d" ? 1 : 0);
      const std::string push = e.str("push", "1");
      const auto slash = push.find('/');
      z.num = std::max(1, std::atoi(push.substr(0, slash).c_str()));
      z.den = slash == std::string::npos ? 1 : std::max(1, std::atoi(push.substr(slash + 1).c_str()));
      mZones.push_back(z);
      continue;
    }

    if (!mSimulation)
      std::fprintf(stderr, "level line %d: unknown entity '%s' ignored\n", e.line, e.kind.c_str());
  }
}

// --- Layers --------------------------------------------------------------------

bool World::layerWantsSolid(const Layer& l, int frame) const
{
  switch (l.driver)
  {
    case LayerDriver::Beat:
    {
      const int span = 30 * l.bars;
      const int f = ((frame % span) + span) % span;
      const int beat = (f / 30) * 4 + beatOfFrame(f % 30);
      return l.beats[std::size_t(beat)];
    }
    case LayerDriver::Timer:
    {
      const int period = std::max(1, l.on + l.off);
      const int f = (((frame + l.phase) % period) + period) % period;
      return f < l.on;
    }
    case LayerDriver::Switch:
    case LayerDriver::Script:
      return l.scriptSolid;
    case LayerDriver::Lit:
      return true;
  }
  return true;
}

void World::applyLayer(Layer& l, bool solid)
{
  l.solid = solid;
  for (int ty = l.y0; ty <= l.y1; ++ty)
    for (int tx = l.x0; tx <= l.x1; ++tx)
      mMap.setBlock(tx, ty, solid ? l.tile : Tile::Empty);
}

void World::updateLayers(bool force)
{
  const int frame = mStats.frames;
  const CellBox pbox = mPlayer.box();
  for (auto& l : mLayers)
  {
    const bool want = layerWantsSolid(l, frame);
    // Telegraph: solid now, dark within the next 8 frames.
    l.buzzing = false;
    if (want && (l.driver == LayerDriver::Beat || l.driver == LayerDriver::Timer))
      for (int i = 1; i <= kTelegraph && !l.buzzing; ++i)
        l.buzzing = !layerWantsSolid(l, frame + i);
    if (!force && want == l.solid)
      continue;
    if (want && !force)
    {
      // Never turn solid on top of the player: wait until they leave.
      const CellBox area{l.x0 * kCellsPerTile, l.y0 * kCellsPerTile, (l.x1 - l.x0 + 1) * kCellsPerTile,
        (l.y1 - l.y0 + 1) * kCellsPerTile};
      if (area.intersects(pbox) && mPlayer.state != PlayerState::Dying)
        continue;
    }
    applyLayer(l, want);
  }
}

// --- Props and rules -----------------------------------------------------------

void World::updateProps(const PlayerInput& input)
{
  auto& p = mPlayer;
  const CellBox pbox = p.box();
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  for (auto& pr : mProps)
  {
    switch (pr.kind)
    {
      case PropKind::GemCache:
        if (!pr.used && alive && pr.box().intersects(pbox))
        {
          pr.used = true;
          const Vec2 c{(float(pr.x) + 1.0f) * kCellSize, (float(pr.y) + 1.0f) * kCellSize};
          for (int i = 0; i < 5; ++i)
          {
            Item it;
            it.kind = ItemKind::Gem;
            it.x = it.prevX = pr.x;
            it.y = it.prevY = pr.y + 1;
            it.variant = i % 4;
            it.vx = i - 2;
            it.pickupDelay = 4;
            mItems.push_back(it);
          }
          burst(c, rgb(255, 240, 120), rgb(255, 255, 255), 24, 2.2f);
          flashAt(c, 120.0f, rgb(255, 230, 120), 20);
          playSound(Sfx::LettersComplete);
          showMessage("SECRET GEM CACHE!");
        }
        break;

      case PropKind::BonusDoor:
      {
        const CellBox near{pr.x - 12, pr.y - 12, pr.w + 24, pr.h + 24};
        if (!pr.used && near.intersects(pbox) && pr.hold == 0)
        {
          pr.hold = 1;
          showMessage("A FLICKERING TV... PRESS UP TO TUNE IN");
        }
        if (!pr.used && alive && (input.up || input.down) && pr.box().intersects(pbox) &&
            (p.state == PlayerState::OnGround || p.state == PlayerState::Falling))
        {
          pr.used = true;
          mBonusRequested = true;
          playSound(Sfx::Teleport);
        }
        break;
      }

      case PropKind::UfoFlyby:
        if (pr.timer >= 0)
        {
          if (++pr.timer > 120)
          {
            pr.timer = -1;
            pr.used = true;
          }
        }
        else if (!pr.used)
        {
          const bool inSpot = pbox.right() < pr.x + pr.w && pbox.left() >= pr.x - 2;
          pr.hold = (inSpot && p.visual == PlayerVisual::LookingUp) ? pr.hold + 1 : 0;
          if (pr.hold >= 45)
            pr.timer = 0;
        }
        break;

      case PropKind::Reflection:
        if (pr.timer >= 0)
        {
          if (++pr.timer > 45)
            pr.timer = -1;
        }
        else
        {
          const bool still = p.state == PlayerState::OnGround && p.x == p.prevX && p.y == p.prevY &&
            pr.box().contains(pbox.left(), pbox.top()) && pr.box().contains(pbox.right(), pbox.bottom());
          pr.hold = still ? pr.hold + 1 : 0;
          if (pr.hold >= 30)
          {
            pr.timer = 0;
            pr.hold = -60; // not again straight away
            if (!pr.used)
            {
              pr.used = true;
              addScore(4200, cellCenter(pbox));
            }
          }
        }
        break;

      default:
        break;
    }
  }

  // Wind pushes the player (and only the player) while inside.
  if (alive && p.state != PlayerState::Ladder && p.state != PlayerState::Pipe)
    for (const auto& z : mZones)
      if (z.kind == ZoneKind::Wind && z.box.intersects(pbox) && (mStats.frames % z.den) < z.num)
      {
        if (z.dx != 0)
          mMap.moveHorizontally(p.x, p.y, Player::kWidth, p.height(), z.dx);
        if (z.dy != 0)
          mMap.moveVertically(p.x, p.y, Player::kWidth, p.height(), z.dy);
      }
}

void World::updateBonusRules(const PlayerInput& /*input*/)
{
  if (!mBonusLevel || mState != WorldState::Playing)
    return;
  if (mLevel->timer > 0 && --mBonusFramesLeft <= 0)
  {
    mBonusFramesLeft = 0;
    mBonusFailed = true;
    mState = WorldState::Done;
    mStateFrames = 0;
    showMessage("TIME'S UP!");
    return;
  }
  if (mPlayer.y > mMap.height() + 1 || mPlayer.state == PlayerState::Dying)
  {
    // Falling off or "dying" in a bonus level just ends it: nothing is lost.
    mBonusFailed = true;
    mState = WorldState::Done;
    mStateFrames = 0;
    return;
  }
  // goal=collect:N ends the bonus level as soon as N gems are in.
  const auto& goal = mLevel->goal;
  if (goal.rfind("collect:", 0) == 0 && mStats.gems >= std::atoi(goal.c_str() + 8))
  {
    showMessage("GOAL!");
    playSound(Sfx::Teleport);
    mState = WorldState::Exiting;
    mStateFrames = 0;
  }
}

void World::addBonusReward(int score, int gems, bool star)
{
  mStats.score += score;
  mStats.gems += gems;
  mStats.gemsTotal += gems;
  if (star)
  {
    mBonusStar = true;
    showMessage("BONUS STAR!");
  }
}

// --- Drawing -------------------------------------------------------------------

void World::drawLayers(Renderer& r, float camX, float camY, int frame) const
{
  for (const auto& l : mLayers)
  {
    const float x = float(l.x0) * kTilePx - camX;
    const float y = float(l.y0) * kTilePx - camY;
    const float w = float(l.x1 - l.x0 + 1) * kTilePx;
    const float h = float(l.y1 - l.y0 + 1) * kTilePx;
    if (x > float(kScreenW) + 64.0f || x + w < -64.0f || y > float(kScreenH) + 64.0f || y + h < -64.0f)
      continue;
    const Color c = l.color;
    if (!l.solid)
    {
      // Ghost outline at 30%.
      const Color g = withAlpha(c, 76);
      r.fillRect(x + 4, y + 4, w - 8, 3, g);
      r.fillRect(x + 4, y + h - 7, w - 8, 3, g);
      r.fillRect(x + 4, y + 4, 3, h - 8, g);
      r.fillRect(x + w - 7, y + 4, 3, h - 8, g);
      continue;
    }
    // A lit neon sign: dark board, bright tubes, glow. Buzzing signs flicker.
    float lit = 1.0f;
    if (l.driver == LayerDriver::Lit)
    {
      const int beat = beatOfFrame(mStats.frames);
      lit = l.beats[std::size_t(beat)] ? 1.0f : 0.25f;
    }
    if (l.buzzing)
      lit = ((frame / 2) % 3 == 0) ? 0.35f : (0.75f + 0.25f * std::sin(float(frame) * 1.7f));
    r.fillRect(x + 2, y + 6, w - 4, h - 8, rgba(20, 10, 36, 235));
    r.fillRect(x + 2, y + 2, w - 4, 6, lerpColor(rgb(60, 50, 80), c, 0.5f * lit));
    const Color tube = lerpColor(rgb(80, 60, 90), c, lit);
    r.fillRect(x + 8, y + 14, w - 16, 5, tube);
    r.fillRect(x + 8, y + h - 18, w - 16, 5, tube);
    for (float tx = x + 18; tx < x + w - 18; tx += 26)
      r.fillRect(tx, y + 22, 4, h - 44 > 4 ? h - 44 : 4, withAlpha(tube, 200));
    r.fillRect(x + 8, y + 14, w - 16, 2, withAlpha(rgb(255, 255, 255), int(180 * lit)));
    for (float gx = x + 32; gx < x + w; gx += 64)
      drawGlow(r, mArt, gx, y + h * 0.5f, 70, c, 0.32f * lit);
  }
}

void World::drawProps(Renderer& r, float camX, float camY, int frame, bool foreground) const
{
  const CellBox pbox = mPlayer.box();
  // Wind: streaks drifting the way it blows.
  if (!foreground)
    for (const auto& z : mZones)
    {
      const float zx = float(z.box.x) * kCellPx - camX, zy = float(z.box.y) * kCellPx - camY;
      const float zw = float(z.box.w) * kCellPx, zh = float(z.box.h) * kCellPx;
      if (zx > float(kScreenW) || zx + zw < 0.0f || zy > float(kScreenH) || zy + zh < 0.0f)
        continue;
      const int streaks = std::max(4, z.box.w * z.box.h / 24);
      for (int i = 0; i < streaks; ++i)
      {
        const unsigned hsh = hash2(i, z.box.x * 31 + z.box.y);
        const float speed = 6.0f + float(hsh % 5u);
        const float len = 40.0f + float((hsh >> 4) % 50u);
        const float travel = std::fmod(float(frame) * speed + float(hsh % 997u), zw + len);
        const float sx = z.dx < 0 ? zx + zw - travel : zx + travel - len;
        const float sy = zy + float((hsh >> 8) % unsigned(std::max(1.0f, zh)));
        const float a0 = std::max(zx, sx), a1 = std::min(zx + zw, sx + len);
        if (a1 > a0)
          r.fillRect(a0, sy, a1 - a0, 3.0f, rgba(235, 240, 255, 120));
      }
    }
  for (const auto& pr : mProps)
  {
    const float x = float(pr.x) * kCellPx - camX;
    const float y = float(pr.y) * kCellPx - camY;
    const float w = float(pr.w) * kCellPx;
    const float h = float(pr.h) * kCellPx;
    const bool onScreen = x < float(kScreenW) + 64.0f && x + w > -64.0f && y < float(kScreenH) + 64.0f && y + h > -64.0f;
    switch (pr.kind)
    {
      case PropKind::Billboard:
        if (foreground && onScreen)
        {
          const float a = pr.box().intersects(pbox) ? 0.3f : 1.0f;
          r.fillRect(x, y, w, h, rgba(34, 20, 60, int(235 * a)));
          r.fillRect(x + 8, y + 8, w - 16, h - 16, rgba(255, 60, 200, int(70 * a)));
          r.fillRect(x, y, w, 6, withAlpha(mTheme.accentB, int(255 * a)));
          r.fillRect(x, y + h - 6, w, 6, withAlpha(mTheme.accentB, int(255 * a)));
          r.drawText(pr.text.empty() ? "NEON CITY COLA" : pr.text, x + w * 0.5f, y + h * 0.5f - 20, {30.0f, rgb(255, 230, 90), kInk, true},
            Align::Center, a);
        }
        break;
      case PropKind::TextSign:
        if (!foreground && onScreen)
        {
          r.fillRect(x, y, w, h, rgba(30, 18, 50, 230));
          r.fillRect(x, y, w, 4, mTheme.accentA);
          r.drawText(pr.text, x + w * 0.5f, y + h * 0.5f - 12, {20.0f, rgb(255, 210, 120), kInk}, Align::Center);
        }
        break;
      case PropKind::Deco42:
        if (!foreground && onScreen)
          r.drawText(pr.text, x + w * 0.5f, y + h * 0.5f - 16, {28.0f, rgba(255, 255, 255, 150), kInk, true},
            Align::Center);
        break;
      case PropKind::Reflection:
        if (!foreground)
        {
          // One frame behind you, facing out of the glass. When it waves it
          // stops copying you.
          const auto& p = mPlayer;
          if (p.hidden)
            break;
          const auto& ca = mArt.characters[std::size_t(mCharacterIndex)];
          const Sprite* spr = &ca.idle[0];
          if (pr.timer >= 0)
            spr = (pr.timer / 6) % 2 ? &ca.lookUp : &ca.idle[0];
          else if (p.visual == PlayerVisual::Walking)
            spr = &ca.run[std::size_t(p.walkFrame % kRunFrames)];
          else if (p.state == PlayerState::Jumping || p.state == PlayerState::Falling)
            spr = &ca.jump;
          else if (p.state == PlayerState::Ladder)
            spr = &ca.climb[std::size_t(p.climbFrame % 2)];
          DrawOpts o;
          o.alpha = 0.22f;
          o.tint = rgb(255, 200, 230);
          const float rx = (float(p.prevX) + 1.5f) * kCellPx - camX, ry = float(p.prevY + 1) * kCellPx - camY;
          r.draw(spr->get(-p.facing), rx, ry, o);
        }
        break;
      case PropKind::UfoFlyby:
        if (!foreground && pr.timer >= 0)
        {
          // Crosses the sky in 120 frames (60 Hz ticks are 4 per frame).
          const float t = float(pr.timer) / 120.0f;
          const float ux = -60.0f + t * (float(kScreenW) + 120.0f);
          const float uy = 120.0f + std::sin(t * 9.0f) * 18.0f;
          drawGlow(r, mArt, ux, uy + 8, 50, rgb(120, 255, 200), 0.6f);
          r.fillRect(ux - 22, uy, 44, 8, rgb(200, 210, 230));
          r.fillRect(ux - 10, uy - 8, 20, 9, rgba(150, 255, 220, 200));
          for (int i = 0; i < 3; ++i)
            r.fillRect(ux - 16 + float(i) * 14, uy + 8, 4, 3, ((frame / 4 + i) % 3 == 0) ? rgb(255, 255, 120) : rgb(80, 80, 90));
        }
        break;
      case PropKind::BonusDoor:
        if (!foreground && onScreen)
        {
          const CellBox near{pr.x - 12, pr.y - 12, pr.w + 24, pr.h + 24};
          const bool flicker = near.intersects(pbox) && !pr.used;
          r.fillRect(x - 6, y - 6, w + 12, h + 12, rgb(40, 34, 48));
          r.fillRect(x - 2, y - 2, w + 4, h + 4, rgb(120, 110, 130));
          // TV static: an 8-frame cycle of noise stripes.
          const int phase = flicker ? (frame / 2) % 8 : 0;
          for (int row = 0; row < int(h / 8); ++row)
          {
            const unsigned hsh = hash2(row * 7 + phase * 131, pr.x + phase);
            const int v = int(60 + (hsh % 160u));
            r.fillRect(x, y + float(row) * 8, w, 8, rgb(v, v, v + 10));
            if (hsh % 5u == 0)
              r.fillRect(x + float(hsh % 100u) * w / 100.0f, y + float(row) * 8, 18, 8, rgb(240, 240, 255));
          }
          if (pr.used)
            r.fillRect(x, y, w, h, rgba(0, 0, 0, 160));
          else if (flicker)
            drawGlow(r, mArt, x + w * 0.5f, y + h * 0.5f, 110, rgb(200, 220, 255), 0.35f);
        }
        break;
      case PropKind::GemCache:
        break;
    }
  }
}

void World::drawBeatHud(Renderer& r, int frame) const
{
  if (!mHasBeat)
    return;
  (void)frame;
  const int f = mStats.frames % 30;
  const int beat = beatOfFrame(f);
  const bool flash = mPlayer.weapon == Weapon::Proto && mPlayer.proto == int(ProtoId::PulsePistol) && onTheBeat();
  const float x0 = 20.0f, y0 = float(kScreenH) - 24.0f;
  r.fillRect(x0 - 8, y0 - 76, 4 * 26 + 12, 86, rgba(8, 6, 22, 170));
  for (int i = 0; i < 4; ++i)
  {
    const bool on = i <= beat;
    const float bh = 18.0f + float(i) * 14.0f;
    Color c = on ? kSignColors[i % 3] : rgba(255, 255, 255, 40);
    if (flash && i == beat)
      c = rgb(255, 255, 255);
    r.fillRect(x0 + float(i) * 26.0f, y0 - bh, 18, bh, c);
  }
  if (flash)
    drawGlow(r, mArt, x0 + 50, y0 - 30, 80, rgb(255, 255, 255), 0.3f);
}

} // namespace gr
