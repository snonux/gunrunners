// Moving and weighted platforms (SPEC 3.2), hatches, breakable terrain and
// spawners. Platforms are one-way tops: the player and walkers stand on
// them and ride along; jumping up through them works like a `=` block.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace gr
{

namespace
{

constexpr float kCellPx = float(kCellSize) * kPixelScale;
constexpr float kTilePx = kCellPx * float(kCellsPerTile);

int sgn(int v) { return (v > 0) - (v < 0); }

} // namespace

// --- Setup -------------------------------------------------------------------

void World::setupPlatform(const EntityDef& e)
{
  Platform pl;
  pl.id = e.id;
  pl.x = pl.prevX = e.x * kCellsPerTile;
  pl.w = std::max(1, e.num("w", 3)) * kCellsPerTile;
  pl.h = kCellsPerTile;
  // The row given is the top surface.
  pl.homeY = e.y * kCellsPerTile;
  pl.y = pl.prevY = pl.startY = e.num("start", e.y) * kCellsPerTile;
  const std::string mode = e.str("mode", "loop");
  if (mode == "pulley")
  {
    pl.mode = PlatformMode::Pulley;
    pl.travel = e.num("drop", 2) * kCellsPerTile;
    pl.slack = e.num("slack", 8);
    pl.rehome = e.num("rehome", 30);
    pl.brake = pl.braked = e.num("brake", 0) != 0;
  }
  else
  {
    pl.mode = PlatformMode::Path;
    pl.pingpong = mode == "pingpong";
    for (const auto& [px, py] : e.path("path"))
      pl.path.emplace_back(px * kCellsPerTile, py * kCellsPerTile);
    if (pl.path.empty())
      pl.path.emplace_back(pl.x, pl.y);
    const std::string sp = e.str("speed", "1");
    const auto slash = sp.find('/');
    pl.speedNum = std::max(1, std::atoi(sp.substr(0, slash).c_str()));
    pl.speedDen = slash == std::string::npos ? 1 : std::max(1, std::atoi(sp.substr(slash + 1).c_str()));
  }
  mPlatforms.push_back(pl);
  mPlatformPairs.push_back(e.str("pair"));
}

void World::linkPlatforms()
{
  // `pair=` names the other gondola; the one listed first is side A.
  for (std::size_t i = 0; i < mPlatforms.size(); ++i)
  {
    auto& a = mPlatforms[i];
    if (a.mode != PlatformMode::Pulley || a.pair >= 0)
      continue;
    for (std::size_t j = 0; j < mPlatforms.size(); ++j)
      if (j != i && mPlatforms[j].id == mPlatformPairs[i])
      {
        a.pair = int(j);
        a.sideA = true;
        auto& b = mPlatforms[j];
        b.pair = int(i);
        b.sideA = false;
        b.travel = a.travel;
        // B mirrors A on the cable.
        b.startY = b.homeY - (a.startY - a.homeY);
        b.y = b.prevY = b.startY;
        break;
      }
  }
  mPlatformPairs.clear();
  for (auto& s : mSpawners)
    for (std::size_t i = 0; i < mPlatforms.size(); ++i)
      if (mPlatforms[i].id == s.onto)
        s.platform = int(i);
  syncPlatformCollision();
}

// --- Update ------------------------------------------------------------------

bool World::standsOn(const CellBox& b, const Platform& pl) const
{
  return b.bottom() + 1 == pl.y && b.x < pl.x + pl.w && pl.x < b.x + b.w;
}

int World::platformWeight(const Platform& pl, bool& player) const
{
  int w = 0;
  const auto& p = mPlayer;
  player = (p.state == PlayerState::OnGround || p.state == PlayerState::Recovering) && standsOn(p.box(), pl);
  if (player)
    w += mCharacterIndex == 1 ? 2 : 1; // Rocco is heavy
  for (const auto& e : mEnemies)
    if (e.alive && e.kind == EnemyKind::Walker && standsOn(e.box(), pl))
      w += enemyDef(e.def).weight;
  return w;
}

bool World::movePlatform(Platform& pl, int dx, int dy)
{
  auto& p = mPlayer;
  const bool riding = (p.state == PlayerState::OnGround || p.state == PlayerState::Recovering) && standsOn(p.box(), pl);
  if (riding && dy < 0)
  {
    const CellBox up = boxAt(p.x, p.y - 1, Player::kWidth, p.height());
    if (mMap.overlapsSolid(up))
      return false; // would crush the rider: stop
  }
  std::vector<Enemy*> riders;
  for (auto& e : mEnemies)
    if (e.alive && e.kind == EnemyKind::Walker && standsOn(e.box(), pl))
      riders.push_back(&e);
  pl.x += dx;
  pl.y += dy;
  if (riding)
  {
    if (dx != 0)
      mMap.moveHorizontally(p.x, p.y, Player::kWidth, p.height(), dx);
    p.y += dy;
  }
  for (Enemy* e : riders)
  {
    e->x += dx;
    e->y += dy;
  }
  return true;
}

void World::updatePlatforms()
{
  if (mPlatforms.empty())
    return;
  for (auto& pl : mPlatforms)
  {
    pl.prevX = pl.x;
    pl.prevY = pl.y;
    if (pl.shudder > 0)
      --pl.shudder;
  }
  for (std::size_t i = 0; i < mPlatforms.size(); ++i)
  {
    auto& a = mPlatforms[i];
    if (a.mode == PlatformMode::Path)
    {
      if (a.path.size() < 2)
        continue;
      // speedNum cells every speedDen frames.
      if (++a.moveTick % a.speedDen != 0)
        continue;
      for (int s = 0; s < a.speedNum; ++s)
      {
        const auto [tx, ty] = a.path[std::size_t(a.target)];
        const int dx = sgn(tx - a.x), dy = sgn(ty - a.y);
        if (dx == 0 && dy == 0)
        {
          if (a.pingpong)
          {
            if (a.target + a.step < 0 || a.target + a.step >= int(a.path.size()))
              a.step = -a.step;
            a.target += a.step;
          }
          else
          {
            a.target = (a.target + 1) % int(a.path.size());
          }
          continue;
        }
        movePlatform(a, dx, dy);
      }
      continue;
    }
    if (!a.sideA || a.pair < 0)
      continue;
    auto& b = mPlatforms[std::size_t(a.pair)];
    bool onA = false, onB = false;
    const int wa = platformWeight(a, onA), wb = platformWeight(b, onB);
    static const bool debug = std::getenv("GR_PLAT_DEBUG") != nullptr;
    if (debug && (wa || wb))
      std::fprintf(stderr, "f%d %s y%d/%d w%d/%d braked%d slack%d bal%d p%d,%d st%d\n", mStats.frames, a.id.c_str(), a.y,
        b.y, wa, wb, a.braked, a.slackLeft, a.balance, mPlayer.x, mPlayer.y, int(mPlayer.state));
    if (a.brake)
    {
      if (a.braked && onA)
        a.braked = false;
      else if (!a.braked && a.y == a.startY && !onA)
        a.braked = true;
    }
    const int bal = a.braked ? 0 : sgn(wa - wb);
    if (bal != a.balance)
    {
      // The cable creaks and both gondolas shudder before they move.
      a.balance = bal;
      a.slackLeft = a.slack;
      if (bal != 0)
      {
        a.shudder = b.shudder = a.slack;
        if (isOnScreen(a.box(), 4) || isOnScreen(b.box(), 4))
          playSound(Sfx::Land);
      }
    }
    a.idle = (onA || onB || bal != 0) ? 0 : a.idle + 1;
    int dir = 0; // +1: A sinks, B rises
    if (a.slackLeft > 0)
      --a.slackLeft;
    else if (bal != 0)
      dir = (++a.moveTick % 2 == 0) ? bal : 0; // half a cell per frame
    else if (!a.braked && a.idle >= a.rehome && a.y != a.startY)
      dir = (++a.moveTick % 4 == 0) ? sgn(a.startY - a.y) : 0; // drift home at a quarter
    if (dir == 0)
      continue;
    if (a.y + dir < a.homeY - a.travel || a.y + dir > a.homeY + a.travel)
      continue;
    // Both sides move or neither does.
    Platform saveA = a, saveB = b;
    const auto savedPlayer = mPlayer;
    if (!movePlatform(a, 0, dir) || !movePlatform(b, 0, -dir))
    {
      a = saveA;
      b = saveB;
      mPlayer = savedPlayer;
    }
  }
  syncPlatformCollision();
}

void World::syncPlatformCollision()
{
  mMap.clearPlatforms();
  if (mFreeFall)
    return; // in free fall gondolas bounce you instead
  for (const auto& pl : mPlatforms)
    mMap.addPlatform(pl.box());
}

// --- Hatches, breakables, spawners ----------------------------------------------

void World::updateHatches()
{
  if (mHatches.empty())
    return;
  const auto& p = mPlayer;
  if (p.state != PlayerState::OnGround)
    return;
  // Standing on the slab a hatch sits in opens it for good.
  const int slabTop = p.y + 1;
  if ((slabTop & 1) != 0)
    return;
  const int ty = slabTop / kCellsPerTile;
  for (auto& h : mHatches)
    if (!h.open && h.ty == ty)
    {
      h.open = true;
      mMap.setBlock(h.tx, h.ty, h.tile);
    }
}

bool World::hitBreakable(const CellBox& shot, int damage, int kind)
{
  for (auto& b : mBreakables)
  {
    if (b.broken)
      continue;
    const CellBox area{b.x0 * kCellsPerTile, b.y0 * kCellsPerTile, (b.x1 - b.x0 + 1) * kCellsPerTile,
      (b.y1 - b.y0 + 1) * kCellsPerTile};
    if (!area.intersects(shot))
      continue;
    if ((b.by == 1 && kind != 1) || (b.by == 2 && kind == 0))
      return true; // only explosions (or heavy hits) break this
    b.hp -= std::max(1, damage);
    const Vec2 c = cellCenter(shot);
    burst(c, rgb(220, 240, 255), rgb(140, 200, 255), 8, 1.4f);
    if (b.hp > 0)
    {
      playSound(Sfx::Hit);
      return true;
    }
    b.broken = true;
    for (int ty = b.y0; ty <= b.y1; ++ty)
      for (int tx = b.x0; tx <= b.x1; ++tx)
      {
        mMap.setBlock(tx, ty, Tile::Empty);
        const Vec2 bc{(float(tx) + 0.5f) * kCellSize * kCellsPerTile, (float(ty) + 0.5f) * kCellSize * kCellsPerTile};
        burst(bc, rgb(230, 245, 255), rgb(150, 210, 255), 10, 2.2f);
      }
    playSound(Sfx::SmallExplosion);
    return true;
  }
  return false;
}

void World::updateSpawners()
{
  for (auto& s : mSpawners)
  {
    if (s.cooldown > 0)
    {
      --s.cooldown;
      continue;
    }
    if (s.platform < 0 || s.platform >= int(mPlatforms.size()))
      continue;
    const Platform& pl = mPlatforms[std::size_t(s.platform)];
    const Platform& a = pl.sideA || pl.pair < 0 ? pl : mPlatforms[std::size_t(pl.pair)];
    if (pl.y != pl.startY || (a.brake && !a.braked))
      continue;
    bool occupied = false;
    for (const auto& e : mEnemies)
      if (e.alive && (standsOn(e.box(), pl) || (e.platform == s.platform && e.y <= mMap.height())))
        occupied = true;
    if (occupied)
      continue;
    spawnEnemy(s.def, s.x, s.y);
    auto& e = mEnemies.back();
    e.platform = s.platform;
    e.active = true;
    e.dir = (pl.x + pl.w / 2) > e.x ? 1 : -1;
    s.cooldown = 60;
  }
}

// --- Free fall (bonus rule) ------------------------------------------------------

// No jumping and no ground until the net: you fall a cell a frame (half
// that holding up, two holding down) and steer a cell a frame. Gondolas and
// ropes bounce you two cells back up and hold you for 8 frames. Landing on
// the net clears the bonus.
void World::updateFreeFall(int mvX, int mvY)
{
  auto& p = mPlayer;
  if (mvX != 0)
  {
    if (mvX != p.facing)
      p.facing = mvX;
    mMap.moveHorizontally(p.x, p.y, Player::kWidth, p.height(), mvX);
  }
  int fall = 0;
  if (mStall > 0)
    --mStall;
  else
    fall = mvY < 0 ? (p.oddFrame ? 1 : 0) : (mvY > 0 ? 2 : 1);
  bool landed = false;
  for (int i = 0; i < fall && !landed; ++i)
  {
    if (mMap.moveVertically(p.x, p.y, Player::kWidth, p.height(), 1) != MoveResult::Completed)
      landed = true;
    else
    {
      // Bounce off whatever is in the way.
      const CellBox b = p.box();
      bool hit = false;
      for (const auto& pl : mPlatforms)
        hit = hit || pl.box().intersects(b);
      for (const auto& r : mRopes)
        hit = hit || r.intersects(b);
      if (hit)
      {
        mMap.moveVertically(p.x, p.y, Player::kWidth, p.height(), -3);
        mStall = 8;
        burst(cellCenter(b), rgb(220, 240, 255), rgb(140, 200, 255), 6, 1.2f);
        playSound(Sfx::Land);
        break;
      }
    }
  }
  if (mMap.onSolidGround(p.box()))
    landed = true;
  p.state = landed ? PlayerState::OnGround : PlayerState::Falling;
  setVisual(landed ? PlayerVisual::Standing : PlayerVisual::Falling);
  if (landed && mState == WorldState::Playing)
  {
    showMessage("NICE CATCH!");
    playSound(Sfx::Teleport);
    mState = WorldState::Exiting;
    mStateFrames = 0;
  }
}

// --- Drawing -------------------------------------------------------------------

void World::drawPlatforms(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  for (const auto& rope : mRopes)
  {
    const float x = (float(rope.x) + 1.0f) * kCellPx - camX, y0 = float(rope.y) * kCellPx - camY;
    const float y1 = y0 + float(rope.h) * kCellPx;
    if (x < -64.0f || x > float(kScreenW) + 64.0f || y1 < 0.0f || y0 > float(kScreenH))
      continue;
    const float sway = std::sin(float(frame) * 0.05f + float(rope.x)) * 6.0f;
    r.drawLine(x, y0, x + sway, y1, 5.0f, rgb(230, 210, 150));
    r.fillRect(x + sway - 10.0f, y1 - 8.0f, 20.0f, 16.0f, rgb(250, 200, 60));
  }
  const Color steel = rgb(70, 84, 110), steelLight = rgb(150, 170, 205), glass = rgba(160, 220, 255, 60);
  for (std::size_t i = 0; i < mPlatforms.size(); ++i)
  {
    const auto& pl = mPlatforms[i];
    float x = (float(pl.prevX) + (float(pl.x) - float(pl.prevX)) * alpha) * kCellPx - camX;
    const float y = (float(pl.prevY) + (float(pl.y) - float(pl.prevY)) * alpha) * kCellPx - camY;
    const float w = float(pl.w) * kCellPx;
    if (x > float(kScreenW) + 64.0f || x + w < -64.0f || y > float(kScreenH) + 400.0f || y < -2000.0f)
      continue;
    if (pl.shudder > 0)
      x += ((frame / 2) % 2 ? 2.0f : -2.0f);
    if (pl.mode == PlatformMode::Pulley || mFreeFall)
    {
      // Cable up to the pulley wheel above the pair (free fall: a loose
      // gondola on its own cable).
      const int top = pl.mode == PlatformMode::Pulley ? pl.homeY - pl.travel - 3 * kCellsPerTile
                                                       : pl.y - 5 * kCellsPerTile;
      const float wheelY = float(top) * kCellPx - camY;
      r.fillRect(x + w * 0.5f - 2.0f, wheelY, 4.0f, y - 64.0f - wheelY, rgb(40, 44, 60));
      if (pl.sideA && pl.pair >= 0)
      {
        const auto& b = mPlatforms[std::size_t(pl.pair)];
        const float bx = float(b.x) * kCellPx - camX + float(b.w) * kCellPx * 0.5f;
        const float ax = x + w * 0.5f;
        const float l = std::min(ax, bx), rr = std::max(ax, bx);
        r.fillRect(l - 10.0f, wheelY - 14.0f, rr - l + 20.0f, 20.0f, steel);
        r.fillRect(l - 10.0f, wheelY - 14.0f, rr - l + 20.0f, 4.0f, steelLight);
        drawGlow(r, mArt, (l + rr) * 0.5f, wheelY - 4.0f, 24, rgb(255, 200, 120), pl.shudder > 0 ? 0.6f : 0.2f);
      }
      // The gondola: a glass cabin on a steel floor.
      r.fillRect(x + 4.0f, y - 64.0f, w - 8.0f, 64.0f, glass);
      r.fillRect(x + 4.0f, y - 64.0f, w - 8.0f, 5.0f, steelLight);
      r.fillRect(x + 4.0f, y - 64.0f, 5.0f, 64.0f, steel);
      r.fillRect(x + w - 9.0f, y - 64.0f, 5.0f, 64.0f, steel);
      r.fillRect(x + w * 0.5f - 2.0f, y - 64.0f, 4.0f, 64.0f, withAlpha(steel, 160));
      r.fillRect(x, y, w, 16.0f, steel);
      r.fillRect(x, y, w, 4.0f, steelLight);
      r.fillRect(x + 6.0f, y + 16.0f, w - 12.0f, 8.0f, rgb(40, 44, 60));
      // A warning light shows which way it is about to go.
      if (pl.shudder > 0)
        drawGlow(r, mArt, x + w * 0.5f, y + 8.0f, 30, rgb(255, 170, 60), 0.8f);
      continue;
    }
    for (float tx = 0.0f; tx < w - 1.0f; tx += kTilePx)
      r.draw(mArt.platform, x + tx, y);
  }

  // Closed hatches: hazard stripes on the trapdoor.
  for (const auto& h : mHatches)
  {
    if (h.open)
      continue;
    const float x = float(h.tx) * kTilePx - camX, y = float(h.ty) * kTilePx - camY;
    if (x < -64.0f || x > float(kScreenW) || y < -64.0f || y > float(kScreenH))
      continue;
    for (int s = 0; s < 4; ++s)
      r.fillRect(x + float(s) * 16.0f, y + 4.0f, 8.0f, 6.0f, rgb(255, 200, 40));
  }

  // Breakable glass: cracks over the panes.
  for (const auto& b : mBreakables)
  {
    if (b.broken)
      continue;
    for (int ty = b.y0; ty <= b.y1; ++ty)
      for (int tx = b.x0; tx <= b.x1; ++tx)
      {
        const float x = float(tx) * kTilePx - camX, y = float(ty) * kTilePx - camY;
        if (x < -64.0f || x > float(kScreenW) || y < -64.0f || y > float(kScreenH))
          continue;
        const unsigned hsh = hash2(tx, ty);
        r.fillRect(x + float(hsh % 40u) + 8.0f, y + 6.0f, 2.0f, 52.0f, rgba(255, 255, 255, 140));
        r.fillRect(x + 6.0f, y + float((hsh >> 6) % 40u) + 8.0f, 52.0f, 2.0f, rgba(255, 255, 255, 110));
      }
  }
}

} // namespace gr
