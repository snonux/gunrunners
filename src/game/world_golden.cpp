// Level 14's bonus level, Golden Touch (SPEC 14, rules=golden_touch):
// everything the runner touches (stands on, bumps or shoots) turns gold.
// Marked blocks gild with their neighbours, moving platforms freeze where
// they are, Temple Cats become gold statues to stand on, and gold doors
// never open. With enough of the marked blocks gold (goal=paint:80) the
// exit gate opens, unless the runner touched it first.

#include "game/world.hpp"

#include "assets/art.hpp"

#include <algorithm>
#include <cmath>

namespace gr
{

namespace
{

constexpr float kTilePx = float(kCellsPerTile * kCellSize) * kPixelScale; // 64
constexpr int kDoorFrames = 8;   // a door's slide up
constexpr int kDoorNear = 16;    // cells: it starts opening with the runner this close
constexpr int kStatue = 1 << 20; // a gilded cat's stun: it never moves again
const Color kGold = rgb(244, 200, 80);
const Color kGoldLight = rgb(255, 238, 160);
const Color kGoldDark = rgb(160, 112, 30);

bool visible(float x, float y, float w, float h)
{
  return x + w > -64.0f && x < float(kScreenW) + 64.0f && y + h > -64.0f && y < float(kScreenH) + 64.0f;
}

CellBox doorBox(const GoldDoor& d)
{
  return {d.bx * kCellsPerTile, d.by0 * kCellsPerTile, kCellsPerTile, (d.by1 - d.by0 + 1) * kCellsPerTile};
}

} // namespace

bool World::setupGoldenEntity(const EntityDef& e)
{
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  if (e.kind == "goldfield" && e.rect("rect", x0, y0, x1, y1))
  {
    // Every solid block in the rect is marked.
    if (mGold.empty())
      return true;
    for (int ty = y0; ty <= y1; ++ty)
      for (int tx = x0; tx <= x1; ++tx)
        if (tx >= 0 && ty >= 0 && tx < mLevel->width && ty < mLevel->height &&
            mMap.solid(tx * kCellsPerTile, ty * kCellsPerTile) && mGold[std::size_t(ty * mLevel->width + tx)] == 0)
        {
          mGold[std::size_t(ty * mLevel->width + tx)] = 1;
          ++mGoldMarked;
        }
    return true;
  }
  if ((e.kind == "golddoor" || e.kind == "goldgate") && e.rect("rect", x0, y0, x1, y1))
  {
    GoldDoor d;
    d.bx = x0;
    d.by0 = y0;
    d.by1 = y1;
    d.gate = e.kind == "goldgate";
    mGoldDoors.push_back(d);
    return true;
  }
  return false;
}

void World::linkGolden()
{
  for (auto& d : mGoldDoors)
    setGoldDoor(d, true);
}

int World::goldAt(int bx, int by) const
{
  if (mGold.empty() || bx < 0 || by < 0 || bx >= mLevel->width || by >= mLevel->height)
    return 0;
  return mGold[std::size_t(by * mLevel->width + bx)];
}

void World::setGoldDoor(GoldDoor& d, bool solid)
{
  for (int ty = d.by0; ty <= d.by1; ++ty)
    mMap.setBlock(d.bx, ty, solid ? Tile::Solid : Tile::Empty);
}

// A marked block turns gold, and the marked blocks around it with it.
void World::gild(int bx, int by)
{
  bool any = false;
  for (int dy = -1; dy <= 1; ++dy)
    for (int dx = -1; dx <= 1; ++dx)
    {
      const int tx = bx + dx, ty = by + dy;
      if (goldAt(tx, ty) != 1)
        continue;
      mGold[std::size_t(ty * mLevel->width + tx)] = 2;
      ++mGoldDone;
      any = true;
    }
  if (!any)
    return;
  burst({(float(bx) + 0.5f) * float(kCellsPerTile * kCellSize), (float(by) + 0.5f) * float(kCellsPerTile * kCellSize)},
    kGoldLight, kGold, 3, 0.8f);
  if (mStats.frames % 3 == 0)
    playSound(Sfx::Gem);
}

void World::gildBox(const CellBox& b)
{
  for (int ty = b.top() / kCellsPerTile; ty <= b.bottom() / kCellsPerTile; ++ty)
    for (int tx = b.left() / kCellsPerTile; tx <= b.right() / kCellsPerTile; ++tx)
      if (goldAt(tx, ty) == 1)
        gild(tx, ty);
}

// A Temple Cat stops dead in gold; it turns solid (2 x 2 blocks, on the
// block grid) as soon as the runner is out of the way.
void World::gildEnemy(Enemy& e)
{
  if (e.attach == 9 || !e.alive)
    return;
  e.attach = 9;
  e.stun = kStatue;
  e.aimX = (e.x + 1) / kCellsPerTile;
  e.aimY = e.y / kCellsPerTile;
  addScore(enemyDef(e.def).score, cellCenter(e.box()));
  burst(cellCenter(e.box()), kGoldLight, kGold, 14, 1.6f);
  playSound(Sfx::Chime);
}

bool World::shotAtGolden(Projectile& pr, const CellBox& b)
{
  for (auto& d : mGoldDoors)
    if (!d.gold && d.open < kDoorFrames && doorBox(d).intersects(b))
    {
      // A door shot before it opened is gold for good (the gate only
      // minds being touched).
      if (!d.gate)
      {
        d.gold = true;
        playSound(Sfx::Clunk);
        showMessage("A GOLD DOOR NEVER OPENS");
      }
      burst(cellCenter(b), kGoldLight, kGold, 6, 1.2f);
      return true;
    }
  for (auto& pl : mPlatforms)
    if (!pl.frozen && pl.mode == PlatformMode::Path && pl.box().intersects(b))
    {
      pl.frozen = true;
      burst(cellCenter(b), kGoldLight, kGold, 10, 1.4f);
      playSound(Sfx::Chime);
      return true;
    }
  for (auto& e : mEnemies)
    if (e.alive && e.attach != 9 && !e.hidden && e.box().intersects(b))
    {
      gildEnemy(e);
      return true;
    }
  (void)pr;
  return false;
}

void World::updateGolden()
{
  if (!mGolden)
    return;
  auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  const CellBox pb = p.box();
  const CellBox touch{pb.x - 1, pb.y - 1, pb.w + 2, pb.h + 2};

  if (alive)
  {
    // Whatever the runner is up against turns gold.
    gildBox(touch);
    for (auto& pl : mPlatforms)
      if (!pl.frozen && pl.mode == PlatformMode::Path && touch.intersects(pl.box()))
      {
        pl.frozen = true;
        burst(cellCenter(pl.box()), kGoldLight, kGold, 12, 1.4f);
        playSound(Sfx::Chime);
      }
    for (auto& e : mEnemies)
      if (e.alive && e.attach != 9 && touch.intersects(e.box()))
        gildEnemy(e);
  }

  // Statues set into the floor once the runner is clear of them.
  for (auto& e : mEnemies)
  {
    if (!e.alive || e.attach != 9)
      continue;
    const CellBox statue{e.aimX * kCellsPerTile, (e.aimY - 1) * kCellsPerTile, 2 * kCellsPerTile, 2 * kCellsPerTile};
    if (statue.intersects(pb))
      continue;
    bool clear = true;
    for (int ty = e.aimY - 1; ty <= e.aimY; ++ty)
      for (int tx = e.aimX; tx <= e.aimX + 1; ++tx)
        clear = clear && !mMap.solid(tx * kCellsPerTile, ty * kCellsPerTile);
    if (clear)
      for (int ty = e.aimY - 1; ty <= e.aimY; ++ty)
        for (int tx = e.aimX; tx <= e.aimX + 1; ++tx)
        {
          mMap.setBlock(tx, ty, Tile::Solid);
          mGold[std::size_t(ty * mLevel->width + tx)] = 3;
        }
    // A gold statue gilds the floor it stands on, gaps beside it too.
    gild(e.aimX, e.aimY + 1);
    gild(e.aimX + 1, e.aimY + 1);
    e.alive = false;
    playSound(Sfx::Clunk);
  }

  // Doors open as the runner comes near, unless touched first; the gate
  // waits for the gold.
  const bool reached = goldReached();
  for (auto& d : mGoldDoors)
  {
    const CellBox box = doorBox(d);
    if (d.open >= kDoorFrames)
      continue;
    if (!d.gold && alive && touch.intersects(box))
    {
      d.gold = true;
      d.open = 0;
      playSound(Sfx::Clunk);
      showMessage(d.gate ? "YOU TOUCHED THE GATE - IT'S GOLD NOW" : "A GOLD DOOR NEVER OPENS");
      continue;
    }
    if (d.gold)
      continue;
    const bool near = alive && std::abs(pb.x + 1 - box.x) <= kDoorNear && pb.bottom() >= box.y - 6 &&
      pb.top() <= box.bottom() + 6;
    if (d.gate ? reached : (near || d.open > 0))
    {
      if (d.open == 0)
        playSound(d.gate ? Sfx::LettersComplete : Sfx::Creak);
      if (d.gate && !mGateTold)
      {
        mGateTold = true;
        showMessage("ALL THAT GLITTERS - THE GATE OPENS");
      }
      if (++d.open >= kDoorFrames)
        setGoldDoor(d, false);
    }
  }
}

void World::drawGolden(Renderer& r, float camX, float camY, int frame) const
{
  const int tx0 = std::max(0, int(camX / kTilePx) - 1);
  const int tx1 = std::min(mLevel->width - 1, int((camX + float(kScreenW)) / kTilePx) + 1);
  const int ty0 = std::max(0, int(camY / kTilePx) - 1);
  const int ty1 = std::min(mLevel->height - 1, int((camY + float(kScreenH)) / kTilePx) + 1);
  for (int ty = ty0; ty <= ty1; ++ty)
    for (int tx = tx0; tx <= tx1; ++tx)
    {
      const int g = goldAt(tx, ty);
      if (g == 0 || !mMap.solid(tx * kCellsPerTile, ty * kCellsPerTile))
        continue;
      const float x = float(tx) * kTilePx - camX, y = float(ty) * kTilePx - camY;
      if (g == 1)
      {
        // Marked: a gold-leaf outline waiting for a touch.
        const Color c = rgba(255, 220, 120, 70 + 30 * ((frame / 8 + tx + ty) % 2));
        r.fillRect(x + 4, y + 4, 56, 4, c);
        r.fillRect(x + 4, y + 56, 56, 4, c);
        r.fillRect(x + 4, y + 8, 4, 48, c);
        r.fillRect(x + 56, y + 8, 4, 48, c);
        continue;
      }
      r.fillRect(x, y, 64, 64, kGold);
      r.fillRect(x, y, 64, 8, kGoldLight);
      r.fillRect(x, y + 56, 64, 8, kGoldDark);
      if ((tx * 7 + ty * 3 + frame / 6) % 23 == 0)
        drawGlow(r, mArt, x + 20, y + 16, 20, rgb(255, 255, 220), 0.8f);
      if (g == 3 && goldAt(tx, ty - 1) != 3)
      {
        // A statue's top: the cat's ears.
        r.fillRect(x + 10, y - 10, 12, 10, kGold);
        r.fillRect(x + 42, y - 10, 12, 10, kGold);
      }
    }

  // Frozen platforms.
  for (const auto& pl : mPlatforms)
    if (pl.frozen)
    {
      const float x = float(pl.x) * kTilePx * 0.5f - camX, y = float(pl.y) * kTilePx * 0.5f - camY;
      r.fillRect(x, y, float(pl.w) * kTilePx * 0.5f, 22, kGold);
      r.fillRect(x, y, float(pl.w) * kTilePx * 0.5f, 6, kGoldLight);
    }

  // Cats caught mid-step, before they set.
  for (const auto& e : mEnemies)
    if (e.alive && e.attach == 9)
    {
      const float x = float(e.aimX) * kTilePx - camX, y = float(e.aimY - 1) * kTilePx - camY;
      r.fillRect(x + 8, y + 20, 112, 108, kGold);
      r.fillRect(x + 8, y + 20, 112, 10, kGoldLight);
    }

  // Doors and the gate.
  for (const auto& d : mGoldDoors)
  {
    const CellBox b = doorBox(d);
    const float x = float(b.x) * kTilePx * 0.5f - camX, y = float(b.y) * kTilePx * 0.5f - camY;
    const float h = float(b.h) * kTilePx * 0.5f;
    if (!visible(x, y, 64, h))
      continue;
    const float up = h * float(d.open) / float(kDoorFrames);
    if (up >= h)
      continue;
    if (d.gate)
    {
      for (int k = 0; k < 4; ++k)
        r.fillRect(x + 4 + float(k) * 16, y, 8, h - up, d.gold ? kGold : rgb(80, 200, 140));
      r.fillRect(x, y + h - up - 10, 64, 10, d.gold ? kGoldDark : rgb(40, 120, 80));
    }
    else
    {
      r.fillRect(x, y, 64, h - up, d.gold ? kGold : rgb(120, 96, 70));
      r.fillRect(x + 8, y + 8, 48, std::max(0.0f, h - up - 16), d.gold ? kGoldLight : rgb(150, 120, 90));
      r.fillRect(x + 26, y + 8, 12, std::max(0.0f, h - up - 16), d.gold ? kGoldDark : rgb(80, 200, 140));
    }
  }
}

void World::drawGoldenHud(Renderer& r, int /*frame*/) const
{
  const float x = float(kScreenW) - 312.0f, y = 96.0f;
  const float frac = mGoldMarked > 0 ? float(mGoldDone) / float(mGoldMarked) : 0.0f;
  r.fillRect(x, y, 300, 52, rgba(8, 6, 22, 190));
  r.drawText("GOLD", x + 10, y + 4, {17.0f, kGold, rgb(20, 12, 4)});
  r.fillRect(x + 70, y + 11, 180, 8, rgba(255, 255, 255, 40));
  r.fillRect(x + 70, y + 11, 180.0f * std::min(1.0f, frac), 8, goldReached() ? rgb(80, 220, 150) : kGold);
  r.fillRect(x + 70 + 180.0f * float(mGoldGoal) / 100.0f, y + 7, 3, 16, rgb(255, 255, 255));
  r.drawText(std::to_string(int(frac * 100.0f + 0.5f)) + "%", x + 290, y + 2, {20.0f, rgb(255, 240, 200), rgb(20, 12, 4)},
    Align::Right);
  r.drawText(std::to_string(mGoldDone) + " / " + std::to_string(mGoldMarked) + " BLOCKS", x + 10, y + 28,
    {15.0f, rgb(200, 190, 160), rgb(20, 12, 4)});
}

} // namespace gr
