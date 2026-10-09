// Level 3, Club Laserdisc (SPEC 03): the music runs in 16-bar phrases and
// the club moves with it. Subwoofer pads bump you on every beat and launch
// you on the drop; laser fans sweep during the chorus; the Bass Cannon
// fires a wall of sound. Also the club's residents (Bouncer, Disco Drone,
// Glow Raver) and the Step on the Beat bonus rule.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace gr
{

namespace
{

constexpr float kPi = 3.14159265f;
constexpr float kCellPx = float(kCellSize) * kPixelScale;

int sgn(int v) { return (v > 0) - (v < 0); }

// The three beams of a fan at this frame (degrees), and whether they are
// live (chorus) or just a preview (the bar before it).
bool fanBeams(const LaserFan& f, int frame, float out[3], bool& live)
{
  const int bar = phraseBar(frame);
  live = bar >= 8 && bar <= 13;
  const bool preview = bar == 7;
  if (!live && !preview)
    return false;
  const float t = float(frame % 30) / 29.0f;
  const float lo = float(f.a0) + 20.0f, hi = float(f.a1) - 20.0f;
  const float a = (frame / 30) % 2 == 0 ? lo + (hi - lo) * t : hi - (hi - lo) * t;
  out[0] = a - 20.0f;
  out[1] = a;
  out[2] = a + 20.0f;
  return true;
}

} // namespace

int phraseBar(int frame) { return ((frame % kPhraseFrames) + kPhraseFrames) % kPhraseFrames / 30; }

int dropHit(int frame)
{
  const int f = ((frame % kPhraseFrames) + kPhraseFrames) % kPhraseFrames;
  return f == 0 ? 1 : (f == 30 ? 2 : (f == 60 ? 3 : 0));
}

int World::jumpHeight() const
{
  int h = 0;
  for (int v : jumpArc())
    h += v;
  return h;
}

int World::padUnder(const CellBox& b) const
{
  for (std::size_t i = 0; i < mPads.size(); ++i)
  {
    const auto& pad = mPads[i];
    if (b.bottom() + 1 == pad.y && b.x < pad.x + pad.w && pad.x < b.x + b.w)
      return int(i);
  }
  return -1;
}

int World::framesToNextLaunch() const
{
  const auto& p = mPlayer;
  if (p.state != PlayerState::OnGround)
    return -1;
  const int i = padUnder(p.box());
  if (i < 0 || mPads[std::size_t(i)].fire <= 0)
    return -1;
  const int hitAt = (mPads[std::size_t(i)].fire - 1) * 30;
  const int f = mStats.frames % kPhraseFrames;
  return ((hitAt - f) % kPhraseFrames + kPhraseFrames) % kPhraseFrames;
}

// --- Launches -----------------------------------------------------------------

void World::startLaunch(int cells)
{
  auto& p = mPlayer;
  mLaunch = cells;
  mLaunchBump = 0;
  p.state = PlayerState::Jumping;
  p.frames = 8;
  p.somersault = -1;
  setVisual(PlayerVisual::Jumping);
}

// Straight up, fast at first and easing off; air control as in a jump.
void World::updateLaunch(int mvX)
{
  auto& p = mPlayer;
  updateHorizontalMovementInAir(mvX);
  const int step = std::clamp((mLaunch + 2) / 3, 1, 3);
  for (int i = 0; i < step && mLaunch > 0; ++i)
  {
    if (mMap.moveVertically(p.x, p.y, Player::kWidth, p.height(), -1) != MoveResult::Completed)
    {
      mLaunch = 0;
      break;
    }
    --mLaunch;
  }
  if (mLaunch == 0)
    startFalling();
}

// --- The club, every frame ------------------------------------------------------

void World::updateClub()
{
  const int f = mStats.frames;
  auto& p = mPlayer;
  if (!mPads.empty())
  {
    const int hit = dropHit(f);
    const bool grounded = p.state == PlayerState::OnGround && mLaunch == 0;
    const int under = grounded ? padUnder(p.box()) : -1;
    if (under >= 0)
    {
      const Pad& pad = mPads[std::size_t(under)];
      const Vec2 c{(float(pad.x) + float(pad.w) * 0.5f) * kCellSize, float(pad.y) * kCellSize};
      if (hit > 0 && pad.fire == hit)
      {
        startLaunch(jumpHeight() * pad.launchX10 / 10);
        burst(c, rgb(200, 120, 255), rgb(255, 255, 255), 24, 2.6f);
        flashAt(c, 120.0f, rgb(190, 90, 255), 16);
        mCamera.shake(8, 1.2f);
        playSound(Sfx::Jump);
      }
      else if (framesIntoBeat(f) == 0 && pad.bump > 0)
      {
        startLaunch(pad.bump);
        mLaunchBump = pad.bump;
      }
    }
    if (hit > 0)
      for (auto& e : mEnemies)
      {
        if (!e.alive || (e.kind != EnemyKind::Walker && e.kind != EnemyKind::Bouncer))
          continue;
        const int i = padUnder(e.box());
        if (i < 0 || mPads[std::size_t(i)].fire != hit)
          continue;
        // Into the ceiling for all their HP.
        burst(cellCenter(e.box()), rgb(200, 120, 255), rgb(255, 255, 255), 20, 3.0f);
        killEnemy(e);
      }
  }

  updateFans();

  for (auto& pd : mPuddles)
  {
    --pd.life;
    const CellBox area{pd.x, pd.y - 1, pd.w, 1};
    if (area.intersects(p.hitBox()))
      hurtPlayer(1);
  }
  mPuddles.erase(std::remove_if(mPuddles.begin(), mPuddles.end(), [](const Puddle& pd) { return pd.life <= 0; }),
    mPuddles.end());

  updateCones();
}

void World::updateFans()
{
  const auto& p = mPlayer;
  if (p.state == PlayerState::Dying || p.state == PlayerState::Teleporting)
    return;
  const CellBox hb = p.hitBox();
  for (const auto& fan : mFans)
  {
    float beams[3];
    bool live = false;
    if (!fanBeams(fan, mStats.frames, beams, live) || !live)
      continue;
    for (float deg : beams)
    {
      const float a = deg * kPi / 180.0f;
      const float dx = std::cos(a), dy = -std::sin(a);
      bool hit = false;
      for (float t = 0.5f; t <= float(fan.len) && !hit; t += 0.5f)
      {
        const int cx = int(std::floor(float(fan.x) + 0.5f + dx * t)), cy = int(std::floor(float(fan.y) + 0.5f + dy * t));
        if (mMap.solid(cx, cy))
          break;
        hit = hb.contains(cx, cy);
      }
      if (hit)
      {
        hurtPlayer(1);
        return;
      }
    }
  }
}

// --- The Bass Cannon ------------------------------------------------------------

namespace
{

// The cone as four 2-cell slices, taller with distance.
CellBox coneSlice(const Cone& c, int d)
{
  static const int kHeights[4] = {2, 3, 5, 6};
  const int h = kHeights[d];
  const int x = c.dir > 0 ? c.x + 2 * d : c.x - 2 * d - 1;
  return {x, c.y - h / 2, 2, h};
}

} // namespace

void World::fireCone(int ox, int oy, int dir, int damage)
{
  Cone c;
  c.dir = dir == 0 ? mPlayer.facing : dir;
  c.x = c.dir < 0 ? ox + 1 : ox; // the muzzle is a 2-cell shot box
  c.y = oy;
  c.damage = damage;
  // The mouth must be in the open; the first slice may touch a wall.
  mCones.push_back(c);
  mCamera.shake(4, 0.8f);
  // The first frame breaks glass and hits the decks.
  for (int d = 0; d < 4; ++d)
  {
    const CellBox s = coneSlice(c, d);
    if (mMap.overlapsSolid(s))
    {
      hitBreakable(s, damage, 3);
      break; // sound stops at the first wall
    }
    shotAtProps(s);
  }
}

void World::updateCones()
{
  for (auto& c : mCones)
  {
    for (int d = 0; d < 4; ++d)
    {
      const CellBox s = coneSlice(c, d);
      if (mMap.overlapsSolid(s))
        break;
      for (auto& e : mEnemies)
      {
        if (!e.alive || !e.active || !e.box().intersects(s))
          continue;
        if (std::find(c.hit.begin(), c.hit.end(), e.id) != c.hit.end())
          continue;
        c.hit.push_back(e.id);
        const bool wasAlive = e.alive;
        shotHitsEnemy(e, c.dir, c.damage);
        if (wasAlive && !e.alive)
          ++mStats.protoKills;
        if (e.alive)
          knockBack(e, c.dir, 6);
      }
    }
    --c.life;
  }
  mCones.erase(std::remove_if(mCones.begin(), mCones.end(), [](const Cone& c) { return c.life <= 0; }), mCones.end());
}

void World::knockBack(Enemy& e, int dir, int cells)
{
  if (e.kind != EnemyKind::Walker && e.kind != EnemyKind::Bouncer && e.kind != EnemyKind::Stepper)
    return;
  for (int i = 0; i < cells; ++i)
  {
    if (mMap.overlapsSolid(boxAt(e.x + dir, e.y, e.w, e.h)))
      break;
    e.x += dir;
  }
}

// A shot moving `dir` (cells per step along x) hits e. Bouncers shrug off
// anything that hits them from the front.
bool World::shotHitsEnemy(Enemy& e, int dir, int damage)
{
  if (e.kind == EnemyKind::Bouncer && dir != 0 && dir == -e.dir)
  {
    e.dive = -8; // staggered
    e.flash = 4;
    burst(cellCenter(e.box()), rgb(255, 255, 255), rgb(200, 200, 220), 6, 1.4f);
    playSound(Sfx::Land);
    return false;
  }
  damageEnemy(e, damage);
  return true;
}

// Shots passing through props: the DJ decks.
void World::shotAtProps(const CellBox& b)
{
  bool any = false;
  for (auto& pr : mProps)
  {
    if (pr.kind != PropKind::Decks || !pr.box().intersects(b))
      continue;
    any = true;
    pr.timer = 6; // the record scratches
    if (onTheBeat())
      pr.hold = mStats.frames + 1;
  }
  if (!any || !mMusicOverride.empty())
    return;
  // Both decks on the beat within one bar: chiptune for the rest of the level.
  int lo = 1 << 30, hi = -1, decks = 0;
  for (const auto& pr : mProps)
    if (pr.kind == PropKind::Decks)
    {
      ++decks;
      if (pr.hold <= 0)
        return;
      lo = std::min(lo, pr.hold);
      hi = std::max(hi, pr.hold);
    }
  if (decks >= 2 && hi - lo <= 30)
  {
    mMusicOverride = "theme_chiptune";
    showMessage("THE DJ SWITCHES TO CHIPTUNE!");
    addScore(4200, cellCenter(b));
  }
}

// --- The club's residents ----------------------------------------------------------

void World::updateBouncer(Enemy& e, const EnemyDef& def)
{
  if (!mMap.onSolidGround(e.box()))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, 1);
    mMap.moveVertically(e.x, e.y, e.w, e.h, 1);
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return;
  }
  if (e.dive < 0)
  {
    ++e.dive; // staggered
    return;
  }
  auto& p = mPlayer;
  const CellBox pb = p.box(), b = e.box();
  const bool vulnerable = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  const int pcx = pb.x + 1, ecx = b.x + b.w / 2;
  const bool level = pb.bottom() >= b.top() && pb.top() <= b.bottom();
  const int dist = std::abs(pcx - ecx);
  if (e.tell > 0)
  {
    if (--e.tell == 0)
    {
      // The shove: a heart and three blocks back.
      const CellBox zone = e.dir > 0 ? CellBox{b.right() + 1, b.y, 4, b.h} : CellBox{b.left() - 4, b.y, 4, b.h};
      if (vulnerable && (zone.intersects(p.hitBox()) || b.intersects(p.hitBox())))
      {
        hurtPlayer(1);
        mMap.moveHorizontally(p.x, p.y, Player::kWidth, p.height(), e.dir * 6);
        if (p.state == PlayerState::OnGround && !mMap.onSolidGround(p.box()))
          startFalling();
      }
      e.lastDive = e.timer + def.cooldown;
    }
    return;
  }
  const int post = e.railX0;
  const bool near = level && dist <= 16 && vulnerable;
  if (near)
    e.dir = pcx < ecx ? -1 : 1; // squares up to you
  if (near && dist <= b.w / 2 + 4 && e.timer >= e.lastDive)
  {
    e.tell = def.tell; // plants his feet and flexes
    return;
  }
  if (e.timer % std::max(1, def.stepEvery) != 0)
    return;
  const int nx = e.x + e.dir;
  const int aheadX = e.dir > 0 ? b.right() + 1 : b.left() - 1;
  const bool blocked = nx < post - def.range || nx > post + def.range ||
    (e.dir > 0 ? mMap.touchingRightWall(b) : mMap.touchingLeftWall(b)) || !mMap.solidTop(aheadX, b.bottom() + 1);
  if (blocked)
  {
    if (!near)
      e.dir = -e.dir;
    return;
  }
  e.x = nx;
}

void World::updateDisco(Enemy& e, const EnemyDef& /*def*/)
{
  const CellBox b = e.box();
  if (!isOnScreen(b, 0))
  {
    e.tell = 0;
    return;
  }
  const int inBar = mStats.frames % 30;
  e.tell = inBar >= 22 ? 30 - inBar : 0; // the facets flash white
  if (inBar != 0 || mPlayer.state == PlayerState::Dying)
    return;
  // Eight shards; the ball turns 45 degrees a bar, so the spread alternates.
  const float off = (mStats.frames / 30) % 2 ? 22.5f : 0.0f;
  for (int k = 0; k < 8; ++k)
  {
    const float a = (float(k) * 45.0f + off) * kPi / 180.0f;
    Projectile pr;
    pr.kind = ShotKind::Enemy;
    pr.w = pr.h = 1;
    pr.speed = 1;
    pr.damage = 1;
    pr.range = 40;
    pr.precise = true;
    pr.fx = float(b.x + 1);
    pr.fy = float(b.y - 1);
    pr.vx = std::cos(a);
    pr.vy = std::sin(a);
    pr.dx = sgn(int(std::lround(pr.vx * 2.0f)));
    pr.x = pr.prevX = b.x + 1;
    pr.y = pr.prevY = b.y - 1;
    mProjectiles.push_back(pr);
  }
  playSound(Sfx::EnemyShot);
}

void World::updateRaver(Enemy& e, const EnemyDef& def)
{
  const int f = mStats.frames, inBar = f % 30;
  const CellBox pb = mPlayer.box(), b = e.box();
  const int dx = (pb.x + 1) - (b.x + 1);
  const bool inRange = std::abs(dx) <= def.range && isOnScreen(b, 0) && mPlayer.state != PlayerState::Dying;
  e.dir = dx < 0 ? -1 : 1;
  e.dive = framesIntoBeat(f) < 3 ? 1 : 0; // hops on every beat
  e.tell = (inRange && inBar >= 15) ? 30 - inBar : 0; // whistle, then the stick goes up
  if (!inRange || inBar != 0)
    return;
  // A glowstick lobbed at you: lands where you stand now.
  const float g = 0.15f;
  const float sx = float(b.x + 1), sy = float(b.y - b.h);
  const float tx = float(pb.x + 1), ty = float(pb.bottom());
  const float T = std::clamp(std::fabs(tx - sx) / 1.2f, 8.0f, 18.0f); // peaks 6 cells up at most
  Projectile pr;
  pr.kind = ShotKind::Enemy;
  pr.w = pr.h = 1;
  pr.speed = 1;
  pr.damage = 1;
  pr.precise = true;
  pr.lob = true;
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
  playSound(Sfx::Shot);
}

// Cardboard Bouncers: a step toward you on every beat, nothing in between.
void World::updateStepper(Enemy& e, const EnemyDef& /*def*/)
{
  if (!mMap.onSolidGround(e.box()))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, 1);
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return;
  }
  if (framesIntoBeat(mStats.frames) != 0)
    return;
  const CellBox pb = mPlayer.box(), b = e.box();
  e.dir = pb.x + 1 < b.x + b.w / 2 ? -1 : 1;
  for (int i = 0; i < 2; ++i)
  {
    const CellBox nb = e.box();
    const int aheadX = e.dir > 0 ? nb.right() + 1 : nb.left() - 1;
    if ((e.dir > 0 ? mMap.touchingRightWall(nb) : mMap.touchingLeftWall(nb)) || !mMap.solidTop(aheadX, nb.bottom() + 1))
      break;
    e.x += e.dir;
  }
}

// --- Step on the Beat --------------------------------------------------------------

// Inputs are queued; on each beat the last one runs: a step of 2 blocks, or
// a jump that starts on the beat. Nothing else moves you between beats.
PlayerInput World::beatStepInput(const PlayerInput& in)
{
  const int dx = in.left ? -1 : (in.right ? 1 : 0);
  if (dx != 0 || in.jump.pressed)
  {
    mQueued = true;
    if (dx != 0)
      mQueuedDx = dx;
    if (in.jump.triggered)
      mQueuedJump = true;
  }
  if (framesIntoBeat(mStats.frames) == 0 && mQueued)
  {
    mBeatDx = mQueuedDx;
    mBeatMove = mQueuedJump ? 8 : (mQueuedDx != 0 ? 4 : 0);
    mBeatJump = mQueuedJump ? 4 : 0;
    mQueued = mQueuedJump = false;
    mQueuedDx = 0;
  }
  PlayerInput out;
  out.fire = in.fire;
  out.up = in.up;
  out.down = in.down;
  if (mBeatMove > 0)
  {
    --mBeatMove;
    out.left = mBeatDx < 0;
    out.right = mBeatDx > 0;
  }
  if (mBeatJump > 0)
  {
    out.jump.pressed = true;
    out.jump.triggered = mBeatJump == 4;
    --mBeatJump;
  }
  return out;
}

// --- Drawing ------------------------------------------------------------------------

void World::drawClub(Renderer& r, float camX, float camY, int frame) const
{
  const int f = mStats.frames;
  const int bar = phraseBar(f);
  const bool riser = bar >= 14;
  const float beatPulse = 1.0f - float(framesIntoBeat(f)) / 8.0f;

  // The dance floor: lit tiles that change on every beat.
  for (const auto& pr : mProps)
  {
    if (pr.kind != PropKind::DanceFloor)
      continue;
    static const Color kTiles[4] = {rgb(255, 60, 200), rgb(0, 230, 255), rgb(255, 230, 60), rgb(160, 90, 255)};
    const int beat = f / 8;
    for (int tx = pr.x; tx < pr.x + pr.w; tx += kCellsPerTile)
    {
      const float x = float(tx) * kCellPx - camX, y = float(pr.y) * kCellPx - camY;
      if (x < -64.0f || x > float(kScreenW) || y < -64.0f || y > float(kScreenH))
        continue;
      const unsigned h = hash2(tx / 2, beat);
      const Color c = kTiles[h % 4u];
      r.fillRect(x + 3.0f, y + 2.0f, 58.0f, 12.0f, withAlpha(c, (h & 16u) ? 230 : 120), Blend::Add);
    }
  }

  // Pads: a speaker cone in the floor.
  for (const auto& pad : mPads)
  {
    const float x = float(pad.x) * kCellPx - camX, y = float(pad.y) * kCellPx - camY;
    const float w = float(pad.w) * kCellPx;
    if (x > float(kScreenW) || x + w < 0.0f || y < -64.0f || y > float(kScreenH) + 64.0f)
      continue;
    r.fillRect(x, y - 6.0f, w, 22.0f, rgb(24, 20, 34));
    r.fillRect(x + 6.0f, y - 2.0f, w - 12.0f, 14.0f, rgb(60, 50, 80));
    r.fillRect(x + w * 0.3f, y, w * 0.4f, 10.0f, rgb(110, 90, 140));
    const Color glow = pad.fire > 0 ? rgb(200, 110, 255) : rgb(0, 220, 255);
    drawGlow(r, mArt, x + w * 0.5f, y, w * 0.45f, glow, (riser && pad.fire > 0 ? 0.8f : 0.25f) + 0.3f * beatPulse);
    // Chain pads count their own hit: 1, 2 or 3 lamps.
    for (int i = 0; i < pad.fire && pad.fire > 0; ++i)
    {
      const bool lit = riser || (f % kPhraseFrames) < pad.fire * 30;
      r.fillRect(x + 10.0f + float(i) * 18.0f, y + 6.0f, 10.0f, 6.0f, lit ? rgb(255, 230, 90) : rgb(70, 60, 50));
    }
  }

  // Laser fans.
  for (const auto& fan : mFans)
  {
    const float hx = (float(fan.x) + 0.5f) * kCellPx - camX, hy = (float(fan.y) + 0.5f) * kCellPx - camY;
    if (hx < -800.0f || hx > float(kScreenW) + 800.0f || hy < -800.0f || hy > float(kScreenH) + 800.0f)
      continue;
    float beams[3];
    bool live = false;
    const bool on = fanBeams(fan, f, beams, live);
    r.fillRect(hx - 22.0f, hy - 16.0f, 44.0f, 14.0f, rgb(40, 44, 54));
    drawGlow(r, mArt, hx, hy, 22, rgb(80, 255, 120), on ? 0.9f : 0.2f);
    if (!on)
      continue;
    for (float deg : beams)
    {
      const float a = deg * kPi / 180.0f;
      const float dx = std::cos(a), dy = -std::sin(a);
      float len = float(fan.len);
      for (float t = 0.5f; t <= float(fan.len); t += 0.5f)
        if (mMap.solid(int(std::floor(float(fan.x) + 0.5f + dx * t)), int(std::floor(float(fan.y) + 0.5f + dy * t))))
        {
          len = t;
          break;
        }
      const float ex = hx + dx * len * kCellPx, ey = hy + dy * len * kCellPx;
      if (live)
      {
        r.drawLine(hx, hy, ex, ey, 10.0f, rgba(60, 255, 110, 90), Blend::Add);
        r.drawLine(hx, hy, ex, ey, 4.0f, rgba(200, 255, 210, 230), Blend::Add);
      }
      else
      {
        r.drawLine(hx, hy, ex, ey, 2.0f, rgba(80, 255, 130, 120), Blend::Add);
      }
    }
  }

  // Puddles.
  for (const auto& pd : mPuddles)
  {
    const float x = float(pd.x) * kCellPx - camX, y = float(pd.y) * kCellPx - camY;
    const float a = std::min(1.0f, float(pd.life) / 10.0f);
    r.fillRect(x, y - 8.0f, float(pd.w) * kCellPx, 10.0f, withAlpha(rgb(120, 255, 90), int(200 * a)), Blend::Add);
    drawGlow(r, mArt, x + float(pd.w) * kCellPx * 0.5f, y - 4.0f, 60, rgb(120, 255, 90), 0.4f * a);
  }

  // The Bass Cannon's wall of sound: rings spreading out of the muzzle.
  for (const auto& c : mCones)
  {
    const float age = float(3 - c.life);
    for (int ring = 0; ring < 3; ++ring)
    {
      const float d = (age + float(ring)) * 2.2f + 1.0f;
      const float cx = (float(c.x) + 0.5f + float(c.dir) * d) * kCellPx - camX;
      const float cy = (float(c.y) + 0.5f) * kCellPx - camY;
      const float h = (1.5f + d * 0.4f) * kCellPx;
      const Color col = withAlpha(rgb(190, 110, 255), 200 - ring * 50 - int(age) * 40);
      r.drawLine(cx, cy - h * 0.5f, cx + float(c.dir) * 10.0f, cy, 6.0f, col, Blend::Add);
      r.drawLine(cx + float(c.dir) * 10.0f, cy, cx, cy + h * 0.5f, 6.0f, col, Blend::Add);
    }
  }
  (void)frame;
}

void World::drawClubHud(Renderer& r, int frame) const
{
  bool chain = false;
  for (const auto& pad : mPads)
    chain = chain || pad.fire > 0;
  if (!chain || mState != WorldState::Playing)
    return;
  const int bar = phraseBar(mStats.frames);
  if (bar < 14)
    return;
  const char* text = bar == 14 ? "DROP IN 2" : "DROP IN 1";
  const float pulse = 0.7f + 0.3f * float((frame / 4) % 2);
  r.drawText(text, float(kScreenW) * 0.5f, 104.0f, {34.0f, rgb(220, 150, 255), rgb(20, 10, 30), true}, Align::Center,
    pulse);
}

std::vector<std::string> World::musicVariants() const
{
  std::vector<std::string> out;
  if (!mLevel)
    return out;
  for (const auto& pr : mProps)
    if (pr.kind == PropKind::Decks)
    {
      out.emplace_back("theme_chiptune");
      break;
    }
  if (mCab.w > 0)
    out.push_back("elevator_" + mLevel->music);
  return out;
}

} // namespace gr
