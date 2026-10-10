// Episode 7, DEEP SPACE (docs/DEEP_SPACE.md). Level 46, Crystal Drift:
// Swap Crystals (shoot one and you and it trade places in a flash; some
// drift on a path until first swapped, one only shows in a pool's
// reflection, a cracked green one gives you the Virus), the Swap Rifle
// (its shots swap you with aliens too, and bounce once off a wall), and the
// islands' aliens: Blinkers, Shard Golems and Prism Bats.

#include "game/world.hpp"

#include "assets/enemy_art.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <tuple>

namespace gr
{

namespace
{

constexpr float kCellPx = float(kCellSize) * kPixelScale; // 32
constexpr float kTilePx = kCellPx * float(kCellsPerTile); // 64
constexpr int kSwapCool = 12;  // frames before a crystal rings again
constexpr int kBlinkPause = 4; // frames a Blinker stands after it appears
constexpr int kLungeFrames = 6;
constexpr int kBlinkGap = 7;   // cells from you to where a Blinker appears
const Color kCrystal = rgb(200, 150, 255);
const Color kCrystalLight = rgb(240, 225, 255);
const Color kVirusGreen = rgb(150, 255, 70);

int sgn(int v) { return (v > 0) - (v < 0); }

// A crystal set on block (bx, by) hovers in it, its bottom on the block's
// bottom row.
std::pair<int, int> crystalCell(int bx, int by)
{
  return {bx * kCellsPerTile, by * kCellsPerTile + kCellsPerTile - SwapCrystal::kH};
}

bool visible(float x, float y, float w, float h)
{
  return x + w > -96.0f && x < float(kScreenW) + 96.0f && y + h > -96.0f && y < float(kScreenH) + 96.0f;
}

// Bob: a crystal floats a few pixels up and down.
float bob(int frame, int seed) { return 4.0f * std::sin(float(frame + seed * 13) * 0.11f); }

} // namespace

// --- Setup ---------------------------------------------------------------------------

bool World::setupCrystalEntity(const EntityDef& e)
{
  if (e.kind == "crystal" && e.hasPos)
  {
    // `@ crystal x y path=x,y;x,y speed=N cracked=1 hidden=1`: blocks. A
    // path makes it drift from its block through the points and back,
    // a cell every `speed` frames, until it is first swapped.
    SwapCrystal c;
    c.id = e.id;
    std::tie(c.x, c.y) = crystalCell(e.x, e.y);
    const auto pts = e.path("path");
    if (!pts.empty())
    {
      int x = c.x, y = c.y;
      c.path.push_back({x, y});
      for (const auto& [bx, by] : pts)
      {
        const auto [tx, ty] = crystalCell(bx, by);
        while (x != tx || y != ty)
        {
          x += sgn(tx - x);
          y += sgn(ty - y);
          c.path.push_back({x, y});
        }
      }
      c.drifting = c.path.size() > 1;
      c.speed = std::max(1, e.num("speed", 4));
    }
    c.cracked = e.num("cracked", 0) != 0;
    c.hidden = e.num("hidden", 0) != 0;
    mSpace.swaps.push_back(std::move(c));
    mSpace.crystals = true;
    return true;
  }
  if (e.kind == "pool" && e.has("rect"))
  {
    // `@ pool rect=x0,y,x1,y`: a mirror-still pool of liquid crystal lying
    // on the floor blocks x0..x1 of row y (you walk on it); it reflects
    // the crystals over it, the hidden ones too.
    const auto r = e.list("rect");
    if (r.size() >= 4)
      mSpace.pools.push_back({r[0] * kCellsPerTile, (r[2] + 1) * kCellsPerTile - 1, r[1] * kCellsPerTile});
    mSpace.crystals = true;
    return true;
  }
  return false;
}

void World::finishCrystalSetup()
{
  mSpace.swapsAtStart = mSpace.swaps;
}

void World::resetCrystals()
{
  mSpace.swaps = mSpace.swapsAtStart;
  mSpace.prisms.clear();
  for (auto& e : mEnemies)
    if (e.kind == EnemyKind::Blinker)
    {
      e.attach = 0;
      e.tell = 0;
    }
}

// --- Swapping ------------------------------------------------------------------------

bool World::fitRunner(int& x, int& y) const
{
  static const int kNudge[][2] = {{0, 0}, {-1, 0}, {1, 0}, {0, -1}, {-1, -1}, {1, -1}, {0, -2}, {-2, 0},
    {2, 0}, {0, 1}, {-1, -2}, {1, -2}, {0, -3}, {0, 2}};
  for (const auto& n : kNudge)
  {
    const int nx = x + n[0], ny = y + n[1];
    if (nx < 0 || nx + Player::kWidth > mMap.width() || ny - Player::kHeight + 1 < 0 || ny >= mMap.height())
      continue;
    if (!mMap.overlapsSolid(boxAt(nx, ny, Player::kWidth, Player::kHeight)))
    {
      x = nx;
      y = ny;
      return true;
    }
  }
  return false;
}

bool World::crystalSwapSpot(const SwapCrystal& c, int& x, int& y) const
{
  x = c.x;
  y = c.y + SwapCrystal::kH - 1;
  return fitRunner(x, y);
}

bool World::canSwap() const
{
  const auto& p = mPlayer;
  return p.state != PlayerState::Dying && p.state != PlayerState::Teleporting && p.vehicle < 0 && p.cart < 0 &&
    p.tube < 0;
}

// Puts the runner at (x, y) (the feet), out of whatever they were doing,
// falling or standing.
void World::dropRunnerAt(int x, int y)
{
  auto& p = mPlayer;
  p.x = p.prevX = x;
  p.y = p.prevY = y;
  p.wall = p.kick = 0;
  p.kickArc = p.vineArc = false;
  p.fling = 0;
  mLaunch = 0;
  startFalling();
}

void World::swapWithCrystal(SwapCrystal& c)
{
  auto& p = mPlayer;
  c.flash = 10;
  c.cool = kSwapCool;
  const Vec2 at = cellCenter(c.box());
  int x = 0, y = 0;
  if (!canSwap() || !crystalSwapSpot(c, x, y))
  {
    burst(at, kCrystalLight, kCrystal, 6, 1.2f);
    playSound(Sfx::Swap);
    return;
  }
  const Vec2 from = cellCenter(p.box());
  // The crystal takes your place: hovering where you stood.
  c.x = p.x;
  c.y = p.y - SwapCrystal::kH + 1;
  c.drifting = false;
  c.hidden = false;
  dropRunnerAt(x, y);
  burst(at, kCrystalLight, kCrystal, 16, 1.8f);
  burst(from, kCrystalLight, kCrystal, 12, 1.4f);
  flashAt(at, 90.0f, kCrystal, 10);
  flashAt(from, 70.0f, kCrystal, 8);
  playSound(Sfx::Swap);
  if (c.cracked)
  {
    // The cracked one: you catch the Virus off it, and it shatters.
    if (p.virus == 0)
      infect();
    c.gone = true;
    burst(cellCenter(c.box()), kVirusGreen, rgb(40, 120, 30), 18, 1.8f);
    playSound(Sfx::SmallExplosion);
  }
}

bool World::shotAtCrystal(Projectile& pr)
{
  const CellBox b = pr.box();
  for (auto& c : mSpace.swaps)
  {
    if (c.gone || !c.hitBox().intersects(b))
      continue;
    if (c.cool > 0)
    {
      // Still ringing: the shot just glances off it.
      burst(cellCenter(b), kCrystalLight, kCrystal, 4, 1.0f, false);
      return true;
    }
    swapWithCrystal(c);
    return true;
  }
  return false;
}

void World::swapWithEnemy(Enemy& e)
{
  auto& p = mPlayer;
  if (!canSwap())
    return;
  const CellBox eb = e.box();
  int x = eb.x + eb.w / 2 - 1, y = e.y;
  if (!fitRunner(x, y))
    return;
  // It goes where you stood; it has to fit there too.
  int ex = p.x + 1 - e.w / 2, ey = p.y;
  bool fits = false;
  for (int up = 0; up <= 3 && !fits; ++up)
    for (int dx : {0, -1, 1})
      if (!mMap.overlapsSolid(boxAt(ex + dx, ey - up, e.w, e.h)))
      {
        ex += dx;
        ey -= up;
        fits = true;
        break;
      }
  if (!fits)
    return;
  const Vec2 from = cellCenter(p.box()), at = cellCenter(eb);
  e.x = e.prevX = ex;
  e.y = e.prevY = ey;
  e.drawSnap = true;
  e.stun = std::max(e.stun, 10); // dazed by it
  dropRunnerAt(x, y);
  burst(at, kCrystalLight, kCrystal, 14, 1.6f);
  burst(from, kCrystalLight, kCrystal, 14, 1.6f);
  flashAt(at, 80.0f, kCrystal, 8);
  playSound(Sfx::Swap);
}

bool World::shotAtCrystalAlien(Projectile& pr, Enemy& e)
{
  const bool rifle = pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::SwapRifle);
  if (rifle && std::find(pr.hit.begin(), pr.hit.end(), e.id) == pr.hit.end())
    swapWithEnemy(e); // and it still takes the hit below
  if (e.kind == EnemyKind::ShardGolem)
  {
    // Only its back: a shot going the way it faces.
    const int going = pr.precise ? (pr.vx < 0.0f ? -1 : 1) : pr.dx;
    if (going != e.dir || (!pr.precise && pr.dy != 0))
    {
      e.flash = 3;
      burst(cellCenter(pr.box()), rgb(255, 255, 255), kCrystal, 6, 1.4f);
      playSound(Sfx::Land);
      return true;
    }
    return false;
  }
  if (e.kind == EnemyKind::PrismBat && !rifle && !pr.precise && pr.dx != 0 && pr.kind != ShotKind::Rocket)
  {
    // The shot comes out the far side in three.
    const CellBox eb = e.box();
    const int x = pr.dx > 0 ? eb.right() + 1 : eb.x - 1;
    mSpace.prisms.push_back({x, eb.y + eb.h / 2, pr.dx, pr.damage, int(pr.kind), e.id});
    if (pr.kind == ShotKind::Proto)
      mSpace.prisms.back()[4] = -1 - pr.proto;
  }
  return false;
}

// --- Each frame ----------------------------------------------------------------------

void World::updateCrystals()
{
  for (auto& c : mSpace.swaps)
  {
    if (c.cool > 0)
      --c.cool;
    if (c.flash > 0)
      --c.flash;
    if (!c.drifting || clock() % c.speed != 0)
      continue;
    // Back and forth along its path.
    const int n = int(c.path.size());
    if (c.step + c.dir < 0 || c.step + c.dir >= n)
      c.dir = -c.dir;
    c.step = std::clamp(c.step + c.dir, 0, n - 1);
    c.x = c.path[std::size_t(c.step)].first;
    c.y = c.path[std::size_t(c.step)].second;
  }
  // Shots a Prism Bat split: three pieces, straight on and 25 degrees
  // either side.
  for (const auto& s : mSpace.prisms)
    for (const float a : {0.0f, -0.44f, 0.44f})
    {
      Projectile pr;
      pr.kind = s[4] < 0 ? ShotKind::Proto : ShotKind(s[4]);
      pr.proto = s[4] < 0 ? -1 - s[4] : -1;
      pr.w = pr.h = 1;
      pr.speed = 2;
      pr.damage = std::max(1, s[3]);
      pr.precise = true;
      pr.fx = float(s[0]);
      pr.fy = float(s[1]);
      pr.vx = float(s[2]) * std::cos(a);
      pr.vy = std::sin(a);
      pr.dx = s[2];
      pr.dy = 0;
      pr.x = pr.prevX = s[0];
      pr.y = pr.prevY = s[1];
      pr.range = 40;
      pr.hit.push_back(s[5]);
      mProjectiles.push_back(pr);
    }
  if (!mSpace.prisms.empty())
    playSound(Sfx::Swap);
  mSpace.prisms.clear();
}

bool World::bounceSwapShot(Projectile& pr)
{
  if (pr.kind != ShotKind::Proto || pr.proto != int(ProtoId::SwapRifle) || pr.bounced || pr.precise)
    return false;
  // Back out of the wall and the other way.
  pr.x -= pr.dx;
  pr.y -= pr.dy;
  pr.dx = -pr.dx;
  pr.dy = -pr.dy;
  pr.bounced = true;
  pr.hit.clear();
  burst(cellCenter(pr.box()), kCrystalLight, kCrystal, 5, 1.0f);
  playSound(Sfx::Click);
  return true;
}

// --- Aliens --------------------------------------------------------------------------

// Blinker: paces its island; when you come close a shimmer shows where it
// will appear (the tell: a few cells in front of you), it blinks there,
// stands a moment and lunges.
void World::updateBlinker(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  if (e.attach != 1 && !mMap.onSolidGround(e.box()))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, 1);
    mMap.moveVertically(e.x, e.y, e.w, e.h, 1);
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return;
  }
  const CellBox pb = p.box();
  const int dx = (pb.x + 1) - (e.x + e.w / 2);
  if (e.cool > 0)
    --e.cool;
  switch (e.attach)
  {
    case 1: // the shimmer
      if (--e.tell <= 0)
      {
        e.x = e.prevX = e.aimX;
        e.y = e.prevY = e.aimY;
        e.drawSnap = true;
        e.dir = (pb.x + 1) < e.x + e.w / 2 ? -1 : 1;
        e.attach = 2;
        e.ox = 0;
        burst(cellCenter(e.box()), kCrystalLight, kCrystal, 12, 1.4f);
      }
      return;
    case 2: // there: a moment, then the lunge
      if (++e.ox <= kBlinkPause)
        return;
      for (int s = 0; s < 2; ++s)
      {
        const CellBox b = e.box();
        const int aheadX = e.dir > 0 ? b.right() + 1 : b.left() - 1;
        if ((e.dir > 0 ? mMap.touchingRightWall(b) : mMap.touchingLeftWall(b)) ||
            !mMap.solidTop(aheadX, b.bottom() + 1))
        {
          e.ox = kBlinkPause + kLungeFrames;
          break;
        }
        e.x += e.dir;
      }
      if (e.ox >= kBlinkPause + kLungeFrames)
      {
        e.attach = 0;
        e.cool = def.cooldown;
      }
      return;
    default:
      break;
  }
  // Pacing.
  if (e.timer % std::max(1, def.stepEvery) == 0)
  {
    const CellBox b = e.box();
    const int aheadX = e.dir > 0 ? b.right() + 1 : b.left() - 1;
    if ((e.dir > 0 ? mMap.touchingRightWall(b) : mMap.touchingLeftWall(b)) || !mMap.solidTop(aheadX, b.bottom() + 1))
      e.dir = -e.dir;
    else
      e.x += e.dir;
  }
  const bool sees = e.cool == 0 && std::abs(dx) <= def.range && std::abs(pb.bottom() - e.y) <= 8 &&
    p.state != PlayerState::Dying && p.vehicle < 0 && isOnScreen(e.box(), 0) &&
    (p.state == PlayerState::OnGround || p.state == PlayerState::Recovering);
  if (!sees)
    return;
  // Where it will appear: on your floor, in front of you if it fits, else
  // behind you.
  for (int side : {p.facing, -p.facing})
    for (int gap : {kBlinkGap, kBlinkGap - 1, kBlinkGap + 1})
    {
      const int tx = side > 0 ? pb.right() + gap - 1 : pb.x - gap - e.w + 2;
      const int ty = pb.bottom();
      const CellBox tb = boxAt(tx, ty, e.w, e.h);
      if (tx < 0 || tx + e.w > mMap.width() || mMap.overlapsSolid(tb) || !mMap.onSolidGround(tb))
        continue;
      e.aimX = tx;
      e.aimY = ty;
      e.attach = 1;
      e.tell = def.tell;
      if (isOnScreen(e.box(), 0))
        playSound(Sfx::Blink);
      return;
    }
}

// Shard Golem: plods along its island, turning at walls and edges and now
// and then on its own; shots spark off its crystal hide, all but the glowing
// shard on its back.
void World::updateShardGolem(Enemy& e, const EnemyDef& def)
{
  if (!mMap.onSolidGround(e.box()))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, 1);
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return;
  }
  if (e.stun > 0)
    return;
  if (e.cool > 0)
  {
    --e.cool; // standing, turned round
    return;
  }
  if (e.timer % std::max(1, def.stepEvery) != 0)
    return;
  const CellBox b = e.box();
  const int aheadX = e.dir > 0 ? b.right() + 1 : b.left() - 1;
  ++e.ox;
  if ((e.dir > 0 ? mMap.touchingRightWall(b) : mMap.touchingLeftWall(b)) || !mMap.solidTop(aheadX, b.bottom() + 1) ||
      e.ox >= 22)
  {
    e.dir = -e.dir;
    e.ox = 0;
    e.cool = 20;
    return;
  }
  e.x += e.dir;
}

// Prism Bat: flutters in a figure eight round where it was put.
void World::updatePrismBat(Enemy& e, const EnemyDef& def)
{
  if (e.railX1 == 0)
  {
    e.railX0 = e.x;
    e.aimY = e.y;
    e.railX1 = 1;
    e.oy = (e.x * 7) % 90;
  }
  if (e.stun > 0)
    return;
  const double t = double(e.timer + e.oy) * 0.07;
  const int tx = e.railX0 + int(std::lround(double(def.range) * std::sin(t)));
  const int ty = e.aimY + int(std::lround(2.5 * std::sin(2.0 * t)));
  if (tx != e.x)
    mMap.moveHorizontally(e.x, e.y, e.w, e.h, sgn(tx - e.x));
  if (ty != e.y)
    mMap.moveVertically(e.x, e.y, e.w, e.h, sgn(ty - e.y));
  const int nd = sgn(tx - e.x);
  if (nd != 0)
    e.dir = nd;
}

// --- Drawing -------------------------------------------------------------------------

void World::drawCrystalBack(Renderer& r, float camX, float camY, int frame, float /*alpha*/) const
{
  if (!mSpace.crystals)
    return;
  // Pools: a mirror-still sheet of liquid crystal over the floor, and in
  // it the crystals hovering over it, upside down.
  for (const auto& pool : mSpace.pools)
  {
    const float x0 = float(pool[0]) * kCellPx - camX, x1 = float(pool[1] + 1) * kCellPx - camX;
    const float y = float(pool[2]) * kCellPx - camY;
    if (!visible(x0, y - 400.0f, x1 - x0, 500.0f))
      continue;
    r.fillRect(x0, y, x1 - x0, kTilePx * 0.75f, rgba(220, 200, 255, 200));
    r.fillRect(x0, y, x1 - x0, 6.0f, rgba(255, 255, 255, 170));
    for (const auto& c : mSpace.swaps)
    {
      if (c.gone || c.x + SwapCrystal::kW <= pool[0] || c.x > pool[1] || c.y > pool[2])
        continue;
      const float cx = (float(c.x) + 1.0f) * kCellPx - camX;
      DrawOpts o;
      o.angle = 180.0f;
      o.alpha = c.hidden ? 0.75f : 0.45f;
      o.scale = 0.55f;
      r.draw(styledEnemySprite(mArt, r, mTheme, "swap_crystal", c.cracked ? 1 : 0, (frame / 12) % 2, 2, 3).get(-1),
        cx, y + 4.0f, o);
    }
    // A glint sliding along the surface.
    const float gx = x0 + std::fmod(float(frame) * 3.0f, std::max(1.0f, x1 - x0));
    r.fillRect(gx, y + 2.0f, 40.0f, 3.0f, rgba(255, 255, 255, 140), Blend::Add);
  }
  // A Blinker's shimmer where it is about to appear.
  for (const auto& e : mEnemies)
  {
    if (!e.alive || e.kind != EnemyKind::Blinker || e.attach != 1)
      continue;
    const float x = (float(e.aimX) + float(e.w) * 0.5f) * kCellPx - camX, y = float(e.aimY + 1) * kCellPx - camY;
    DrawOpts o;
    o.alpha = 0.5f + 0.4f * float((frame / 2) % 2);
    r.draw(styledEnemySprite(mArt, r, mTheme, "blink_shimmer", 0, (frame / 3) % 2, e.w, e.h).get(1), x, y, o);
  }
  // The crystals.
  for (std::size_t i = 0; i < mSpace.swaps.size(); ++i)
  {
    const SwapCrystal& c = mSpace.swaps[i];
    if (c.gone || c.hidden)
      continue;
    const float x = (float(c.x) + 1.0f) * kCellPx - camX;
    const float y = float(c.y + SwapCrystal::kH) * kCellPx - camY + bob(frame, int(i));
    if (!visible(x - 64.0f, y - 128.0f, 128.0f, 160.0f))
      continue;
    drawGlow(r, mArt, x, y - 48.0f, 70.0f, c.cracked ? kVirusGreen : kCrystal, c.flash > 0 ? 0.9f : 0.35f);
    const int v = c.cracked ? 1 : (c.flash > 0 ? 2 : 0);
    r.draw(styledEnemySprite(mArt, r, mTheme, "swap_crystal", v, (frame / 12) % 2, 2, 3).get(1), x, y);
    if (c.drifting)
    {
      // Sparkles trailing it.
      const int f = frame % 24;
      r.fillRect(x - 3.0f + float((f * 7) % 20 - 10), y - 20.0f - float(f) * 2.0f, 5.0f, 5.0f,
        rgba(240, 220, 255, 200 - f * 8), Blend::Add);
    }
  }
}

} // namespace gr
