// Episode 7, DEEP SPACE (docs/DEEP_SPACE.md). Level 44, Crash Garden: goo
// walls you stick to and kick off (the Cling state), the Goo Gun's patches,
// Skitters, Spitpods and Gloops.

#include "game/world.hpp"

#include "assets/enemy_art.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <string>

namespace gr
{

namespace
{

constexpr float kCellPx = float(kCellSize) * kPixelScale; // 32
constexpr float kTilePx = kCellPx * float(kCellsPerTile);  // 64
constexpr int kPatchLife = 150;  // a Goo Gun splat lasts 10 seconds
constexpr int kPatchFade = 30;   // and fades out over the last two
constexpr int kMaxPatches = 4;   // the oldest goes when a fifth lands
constexpr int kGlueFrames = 45;  // a Goo Gun hit glues an alien for 3 seconds
constexpr int kKickCells = 3;    // a kick pushes you 3 cells off the wall first
const Color kGoo = rgb(150, 255, 90);
const Color kGooDeep = rgb(40, 150, 60);

int sgn(int v) { return (v > 0) - (v < 0); }

bool visible(float x, float y, float w, float h)
{
  return x + w > -96.0f && x < float(kScreenW) + 96.0f && y + h > -96.0f && y < float(kScreenH) + 96.0f;
}

// A Skitter's leap and a Gloop's hop: cells up (-) or down (+) per frame.
constexpr int kLeap[] = {-2, -2, -1, -1, 0, 0, 1, 1, 2, 2};
constexpr int kHop[] = {-2, -1, -1, 0, 1, 1, 2};

} // namespace

// --- Goo ------------------------------------------------------------------------------

bool World::gooBlockAt(int tx, int ty) const
{
  const int W = mLevel->width;
  if (mSpace.gooBlock.empty() || tx < 0 || ty < 0 || tx >= W || ty >= mLevel->height)
    return false;
  return mSpace.gooBlock[std::size_t(ty * W + tx)] != 0;
}

bool World::gooAt(int cx, int cy) const
{
  if (!mSpace.goo || cx < 0 || cy < 0 || !mMap.solid(cx, cy))
    return false;
  if (gooBlockAt(cx / kCellsPerTile, cy / kCellsPerTile))
    return true;
  for (const auto& g : mSpace.patches)
    if (cx == g.x && cy >= g.y0 && cy <= g.y1)
      return true;
  return false;
}

bool World::setupSpaceEntity(const EntityDef& e)
{
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  if (e.kind == "goo" && e.rect("rect", x0, y0, x1, y1))
  {
    const int W = mLevel->width, H = mLevel->height;
    if (mSpace.gooBlock.empty())
      mSpace.gooBlock.assign(std::size_t(W * H), 0);
    for (int ty = std::max(0, y0); ty <= std::min(H - 1, y1); ++ty)
      for (int tx = std::max(0, x0); tx <= std::min(W - 1, x1); ++tx)
        mSpace.gooBlock[std::size_t(ty * W + tx)] = 1;
    mSpace.goo = true;
    return true;
  }
  return false;
}

void World::addGoo(int x, int y0, int y1, int side, int life)
{
  // Only the wall's own cells, and only where its face is open.
  while (y0 <= y1 && !(mMap.solid(x, y0) && !mMap.solid(x + side, y0)))
    ++y0;
  while (y1 >= y0 && !(mMap.solid(x, y1) && !mMap.solid(x + side, y1)))
    --y1;
  if (y0 > y1)
    return;
  if (int(mSpace.patches.size()) >= kMaxPatches)
    mSpace.patches.erase(mSpace.patches.begin());
  mSpace.patches.push_back({x, y0, y1, side, life});
  mSpace.goo = true;
}

void World::splatGoo(int cx, int cy, int reach)
{
  // The nearest wall face either side, at that height.
  for (int d = 1; d <= reach; ++d)
    for (int side : {-1, 1})
    {
      const int x = cx + side * d;
      if (mMap.solid(x, cy) && !mMap.solid(x - side, cy))
      {
        addGoo(x, cy - 3, cy + 2, -side, kPatchLife);
        return;
      }
    }
}

void World::resetSpace()
{
  mSpace.patches.clear();
  mSpace.splits.clear();
}

void World::updateSpace(const PlayerInput& /*input*/)
{
  if (!mSpace.goo && mSpace.splits.empty())
    return;
  // Goo Gun patches dry up.
  for (auto& g : mSpace.patches)
    --g.life;
  mSpace.patches.erase(std::remove_if(mSpace.patches.begin(), mSpace.patches.end(),
                         [](const GooPatch& g) { return g.life <= 0; }),
    mSpace.patches.end());
  auto& p = mPlayer;
  if (p.state == PlayerState::Cling)
  {
    const int face = p.wall > 0 ? p.x + Player::kWidth : p.x - 1;
    if (mStats.frames % 4 == 0)
      burst({(float(face) + (p.wall > 0 ? 0.0f : 1.0f)) * kCellSize, (float(p.y) - 1.0f) * kCellSize}, kGoo, kGooDeep,
        1, 0.4f, false);
  }
  // Gloops shot last frame split into two Glooplets that hop apart.
  const int small = enemyIndex("gloop_small");
  for (const auto& s : mSpace.splits)
  {
    if (small < 0)
      break;
    for (int dir : {-1, 1})
    {
      const int x = s[0] + (dir > 0 ? s[2] - 2 : 0);
      if (mMap.overlapsSolid(boxAt(x, s[1], 2, 2)))
        continue;
      spawnEnemy(small, x, s[1]);
      Enemy& e = mEnemies.back();
      e.dir = dir;
      e.active = true;
      e.attach = 1; // already hopping
      e.ox = 0;
      e.cool = 20;
    }
  }
  mSpace.splits.clear();
}

// --- Clinging -------------------------------------------------------------------------

bool World::tryCling(int mvX)
{
  auto& p = mPlayer;
  if (mvX == 0 || p.kick != 0 || p.cart >= 0 || (p.state == PlayerState::Jumping && p.frames < 3))
    return false;
  const int face = mvX > 0 ? p.x + Player::kWidth : p.x - 1;
  if (!gooAt(face, p.y - 3) && !gooAt(face, p.y - 2))
    return false;
  p.state = PlayerState::Cling;
  p.wall = mvX;
  p.facing = -mvX;
  p.frames = 0;
  p.somersault = -1;
  p.fling = 0;
  p.vineArc = p.kickArc = false;
  setVisual(PlayerVisual::Clinging);
  playSound(Sfx::Squelch);
  burst({(float(face) + (mvX > 0 ? 0.0f : 1.0f)) * kCellSize, (float(p.y) - 3.0f) * kCellSize}, kGoo, kGooDeep, 6,
    0.9f);
  return true;
}

void World::updateCling(int mvX, int mvY)
{
  auto& p = mPlayer;
  const int wall = p.wall;
  const int face = wall > 0 ? p.x + Player::kWidth : p.x - 1;
  ++p.frames;
  p.facing = -wall;
  setVisual(PlayerVisual::Clinging);
  if (p.jumpRequested && !mMap.touchingCeiling(p.box()))
  {
    // Kick off: away from the wall a cell a frame, then the full jump.
    p.state = PlayerState::Jumping;
    p.frames = 0;
    p.fromLadder = true;
    p.somersault = -1;
    p.kickArc = true;
    p.kick = -wall * kKickCells;
    p.wall = 0;
    p.jumpRequested = false;
    updateJumpMovement(0, true);
    if (p.state == PlayerState::Jumping)
      setVisual(PlayerVisual::Jumping);
    playSound(Sfx::Jump);
    burst({(float(face) + (wall > 0 ? 0.0f : 1.0f)) * kCellSize, (float(p.y) - 1.0f) * kCellSize}, kGoo, kGooDeep, 5,
      1.2f);
    return;
  }
  if (mvX == -wall || (!gooAt(face, p.y - 3) && !gooAt(face, p.y - 2)))
  {
    // Let go (or the goo ran out under your hands).
    p.wall = 0;
    startFalling();
    return;
  }
  // Slide: held up you hang on, held down you slide fast, else slowly.
  const int slide = mvY < 0 ? 0 : (mvY > 0 ? 2 : (p.frames % 2 == 0 ? 1 : 0));
  for (int i = 0; i < slide; ++i)
    if (mMap.moveVertically(p.x, p.y, Player::kWidth, p.height(), 1) != MoveResult::Completed)
      break;
  if (mMap.onSolidGround(p.box()))
  {
    p.wall = 0;
    landOnGround(false);
  }
}

// --- Shots ----------------------------------------------------------------------------

bool World::shotAtSpace(Projectile& pr)
{
  if (pr.kind != ShotKind::Proto || pr.proto != int(ProtoId::GooGun) || pr.dx == 0)
    return false;
  // Splat on the first wall face the blob reached.
  const int dir = pr.dx;
  int face = -1;
  for (int i = 0; i < pr.w && face < 0; ++i)
  {
    const int c = dir > 0 ? pr.x + i : pr.x + pr.w - 1 - i;
    if (mMap.solid(c, pr.y))
      face = c;
  }
  if (face < 0)
    return false;
  addGoo(face, pr.y - 3, pr.y + 2, -dir, kPatchLife);
  burst({(float(face) + (dir > 0 ? 0.0f : 1.0f)) * kCellSize, (float(pr.y) + 0.5f) * kCellSize}, kGoo, kGooDeep, 10,
    1.4f);
  playSound(Sfx::Squelch);
  return true;
}

bool World::shotAtAlien(Projectile& pr, Enemy& e)
{
  if (pr.kind != ShotKind::Proto || pr.proto != int(ProtoId::GooGun))
    return false;
  // The Goo Gun: a sting, and the feet glued to the spot for a while.
  const bool wasAlive = e.alive;
  damageEnemy(e, pr.damage);
  if (wasAlive && !e.alive)
    ++mStats.protoKills;
  if (e.alive)
  {
    e.stun = kGlueFrames;
    e.tell = 0;
  }
  burst(cellCenter(e.box()), kGoo, kGooDeep, 10, 1.4f);
  playSound(Sfx::Squelch);
  return true;
}

void World::alienKilled(const Enemy& e)
{
  if (e.kind != EnemyKind::Gloop)
    return;
  const bool big = std::string(enemyDef(e.def).key) == "gloop";
  // Goo everywhere: on the nearest wall, and a big one splits in two.
  splatGoo(e.x + e.w / 2, e.y - 1, big ? 8 : 4);
  if (big)
    mSpace.splits.push_back({e.x, e.y, e.w});
  burst(cellCenter(e.box()), kGoo, rgb(230, 255, 200), big ? 18 : 10, 1.8f);
  playSound(Sfx::Squelch);
}

// --- Aliens ---------------------------------------------------------------------------

void World::updateSkitter(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  const CellBox b = e.box(), pb = p.box();
  if (e.cool > 0)
    --e.cool;
  if (e.attach == 2)
  {
    // The leap: 2 cells a frame at you, up and over.
    const int f = e.ox++;
    const int dy = f < int(std::size(kLeap)) ? kLeap[f] : 2;
    for (int i = 0; i < 2; ++i)
      if (mMap.moveHorizontally(e.x, e.y, e.w, e.h, e.dir) != MoveResult::Completed)
        break;
    for (int i = 0; i < std::abs(dy); ++i)
      if (mMap.moveVertically(e.x, e.y, e.w, e.h, sgn(dy)) != MoveResult::Completed)
        break;
    if (e.y > mMap.height() + 4)
      e.alive = false;
    else if (dy > 0 && mMap.onSolidGround(e.box()))
    {
      e.attach = 0;
      e.cool = def.cooldown;
    }
    return;
  }
  if (e.attach == 1)
  {
    // Clicking: the tell before the leap.
    if (--e.tell <= 0)
    {
      e.tell = 0;
      e.attach = 2;
      e.ox = 0;
    }
    return;
  }
  if (e.attach == 3)
  {
    // Up a goo wall, a cell every other frame; over the lip at the top.
    if (e.timer % 2 != 0)
      return;
    const int ahead = e.dir > 0 ? b.right() + 1 : b.left() - 1;
    if (!mMap.solid(ahead, b.bottom()))
    {
      e.attach = 0;
      mMap.moveHorizontally(e.x, e.y, e.w, e.h, e.dir);
      return;
    }
    if (!gooAt(ahead, b.bottom()) ||
        mMap.moveVertically(e.x, e.y, e.w, e.h, -1) != MoveResult::Completed)
    {
      e.attach = 0; // the goo ended: it lets go
      e.dir = -e.dir;
    }
    return;
  }
  if (!mMap.onSolidGround(b))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, 1);
    mMap.moveVertically(e.x, e.y, e.w, e.h, 1);
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return;
  }
  // You, ahead on its floor: it clicks, then leaps.
  const int dx = (pb.x + 1) - (b.x + 1);
  const int dy = pb.bottom() - b.bottom();
  const bool level = std::abs(dy) <= 4 && p.state != PlayerState::Dying && isOnScreen(b, 0);
  if (e.cool == 0 && level && std::abs(dx) <= def.range && (dx == 0 || sgn(dx) == e.dir))
  {
    e.attach = 1;
    e.tell = def.tell;
    playSound(Sfx::Chitter);
    return;
  }
  if (level && std::abs(dx) <= def.range && sgn(dx) == -e.dir && e.timer % 12 == 0)
    e.dir = -e.dir; // it heard you behind it
  if (e.timer % std::max(1, def.stepEvery) != 0)
    return;
  const int aheadX = e.dir > 0 ? b.right() + 1 : b.left() - 1;
  const bool wall = e.dir > 0 ? mMap.touchingRightWall(b) : mMap.touchingLeftWall(b);
  if (wall && gooAt(aheadX, b.bottom()))
  {
    e.attach = 3;
    return;
  }
  const bool ledge = !mMap.solidTop(aheadX, b.bottom() + 1);
  if (wall || ledge)
    e.dir = -e.dir;
  else
    e.x += e.dir;
}

void World::updateSpitpod(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  const CellBox b = e.box(), pb = p.box();
  const int dx = (pb.x + 1) - (b.x + 1);
  const int dy = pb.bottom() - b.bottom();
  const bool inRange = std::abs(dx) <= def.range && dy > -16 && dy < 14 && isOnScreen(b, 0) &&
    p.state != PlayerState::Dying;
  if (e.tell == 0)
    e.dir = dx < 0 ? -1 : 1;
  if (e.cool > 0)
  {
    --e.cool;
    return;
  }
  if (e.tell > 0)
  {
    if (--e.tell > 0)
      return;
    // Acid, lobbed to land where you stand now.
    const float g = 0.15f;
    const float sx = float(b.x + 1 + e.dir), sy = float(b.y + 1);
    const float tx = float(pb.x + 1), ty = float(pb.bottom());
    const float T = std::clamp(std::fabs(tx - sx) / 1.1f, 8.0f, 18.0f);
    Projectile pr;
    pr.kind = ShotKind::Enemy;
    pr.w = pr.h = 1;
    pr.speed = 1;
    pr.damage = 1;
    pr.precise = true;
    pr.carrier = e.carrier;
    pr.range = 80;
    pr.gy = g;
    pr.fx = sx;
    pr.fy = sy;
    pr.vx = (tx - sx) / T;
    pr.vy = (ty - sy - 0.5f * g * T * T) / T;
    pr.dx = e.dir;
    pr.x = pr.prevX = int(sx);
    pr.y = pr.prevY = int(sy);
    mProjectiles.push_back(pr);
    playSound(Sfx::Spit);
    e.cool = def.cooldown;
    return;
  }
  if (inRange)
    e.tell = def.tell; // the bulb swells
}

void World::updateGloop(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  if (e.attach == 1)
  {
    // A hop: a cell a frame sideways, up and down again.
    const int f = e.ox++;
    const int dy = f < int(std::size(kHop)) ? kHop[f] : 2;
    if (mMap.moveHorizontally(e.x, e.y, e.w, e.h, e.dir) != MoveResult::Completed)
      e.dir = -e.dir;
    for (int i = 0; i < std::abs(dy); ++i)
      if (mMap.moveVertically(e.x, e.y, e.w, e.h, sgn(dy)) != MoveResult::Completed)
        break;
    if (e.y > mMap.height() + 4)
      e.alive = false;
    else if (dy > 0 && mMap.onSolidGround(e.box()))
    {
      e.attach = 0;
      e.cool = def.cooldown;
      if (isOnScreen(e.box(), 0) && e.w > 2)
        playSound(Sfx::Squelch);
    }
    return;
  }
  const CellBox b = e.box(), pb = p.box();
  if (!mMap.onSolidGround(b))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, 1);
    mMap.moveVertically(e.x, e.y, e.w, e.h, 1);
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return;
  }
  if (e.cool > 0)
  {
    --e.cool;
    return;
  }
  if (e.tell > 0)
  {
    // Squashed down: the hop comes at the end of it.
    if (--e.tell == 0)
    {
      e.attach = 1;
      e.ox = 0;
    }
    return;
  }
  const int dx = (pb.x + 1) - (b.x + b.w / 2);
  const bool near = std::abs(dx) <= def.range && std::abs(pb.bottom() - b.bottom()) < 12 && isOnScreen(b, 0) &&
    p.state != PlayerState::Dying;
  if (!near)
    return;
  e.dir = dx < 0 ? -1 : 1;
  e.tell = def.tell;
}

// --- Drawing --------------------------------------------------------------------------

void World::drawSpaceBack(Renderer& r, float camX, float camY, int frame, float /*alpha*/) const
{
  if (!mSpace.goo)
    return;
  const int f = (frame / 20) % 2;
  // Goo-coated blocks: a wet sheen over the block and a coat on each open face.
  if (!mSpace.gooBlock.empty())
  {
    const int tx0 = std::max(0, int(camX / kTilePx) - 1);
    const int ty0 = std::max(0, int(camY / kTilePx) - 1);
    const int tx1 = std::min(mLevel->width - 1, int((camX + float(kScreenW)) / kTilePx) + 1);
    const int ty1 = std::min(mLevel->height - 1, int((camY + float(kScreenH)) / kTilePx) + 1);
    for (int ty = ty0; ty <= ty1; ++ty)
      for (int tx = tx0; tx <= tx1; ++tx)
      {
        if (!gooBlockAt(tx, ty) || mMap.block(tx, ty) != Tile::Solid)
          continue;
        const float x = float(tx) * kTilePx - camX, y = float(ty) * kTilePx - camY;
        r.fillRect(x, y, kTilePx, kTilePx, rgba(120, 255, 80, 40), Blend::Add);
        for (int side : {-1, 1})
        {
          if (mMap.block(tx + side, ty) == Tile::Solid)
            continue;
          // The sprite's box starts at the wall (its coat reaches 10 px
          // into it); it is anchored at its bottom middle.
          const float wallX = side > 0 ? x + kTilePx : x;
          r.draw(styledEnemySprite(mArt, r, mTheme, "goo_face", 0, (f + tx + ty) % 2, 1, 2).get(side),
            wallX + float(side) * 16.0f, y + kTilePx);
        }
      }
  }
  // Goo Gun patches, fading out at the end.
  for (const auto& g : mSpace.patches)
  {
    const float wallX = (float(g.x) + (g.side > 0 ? 1.0f : 0.0f)) * kCellPx - camX;
    const float top = float(g.y0) * kCellPx - camY, h = float(g.y1 - g.y0 + 1) * kCellPx;
    if (!visible(wallX - 40.0f, top, 80.0f, h))
      continue;
    DrawOpts o;
    o.alpha = std::min(1.0f, float(g.life) / float(kPatchFade));
    for (int y = g.y0; y <= g.y1; y += 2)
    {
      const float by = float(std::min(y + 2, g.y1 + 1)) * kCellPx - camY;
      r.draw(styledEnemySprite(mArt, r, mTheme, "goo_face", 0, (f + y / 2) % 2, 1, 2).get(g.side),
        wallX + float(g.side) * 16.0f, by, o);
    }
    drawGlow(r, mArt, wallX + float(g.side) * 14.0f, top + h * 0.5f, 50.0f, kGoo, 0.3f * o.alpha);
  }
}

void World::drawSpaceFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  if (!mSpace.goo)
    return;
  // Aliens glued by the Goo Gun.
  for (const auto& e : mEnemies)
  {
    if (!e.alive || e.stun <= 0 || e.hidden)
      continue;
    const float x = (float(e.prevX) + float(e.x - e.prevX) * alpha + float(e.w) * 0.5f) * kCellPx - camX;
    const float y = (float(e.prevY) + float(e.y - e.prevY) * alpha + 1.0f) * kCellPx - camY;
    if (!visible(x - 64.0f, y - 128.0f, 128.0f, 128.0f))
      continue;
    DrawOpts o;
    o.alpha = std::min(1.0f, float(e.stun) / 10.0f);
    r.draw(styledEnemySprite(mArt, r, mTheme, "goo_glue", 0, (frame / 12) % 2, e.w, e.h).get(e.dir), x, y, o);
  }
}

} // namespace gr
