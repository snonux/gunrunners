// Episode 7, DEEP SPACE (docs/DEEP_SPACE.md). Level 47, Silk Canyon: Silk
// Lines (taut strands strung across the canyon: jump into one and you hang
// from it and slide down it, faster and faster; jump off any time and you
// keep the speed), the Silk Shooter (fires down ahead; where it hits rock a
// new line runs from your hands down to there), cocoons (gems inside, or
// the Virus in the green one) and the canyon's aliens: Loom Spiders (they
// walk their line and cut it under you after a tell, then spin it again),
// Cocoon Pods and their Droplings.

#include "game/world.hpp"

#include "assets/enemy_art.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace gr
{

namespace
{

constexpr float kCellPx = float(kCellSize) * kPixelScale; // 32
constexpr float kGrabReach = 1.6f; // cells from the hands to a line that still catches it
constexpr float kStartSpeed = 0.7f; // cells a frame along the line, just grabbed
constexpr float kMaxSpeed = 2.0f; // the camera keeps up with 2 cells a frame
constexpr float kSpinSpeed = 1.5f; // cells a frame a line is spun out
constexpr int kRegrab = 6;          // frames before a line you let go of catches you again
constexpr int kRegrow = 45;         // frames before a cut line is spun again
constexpr int kShooterReach = 32;   // cells: the Silk Shooter's 16 blocks
constexpr int kShooterLines = 2;    // lines of your own at once
const Color kSilk = rgb(246, 240, 224);
const Color kVirusGreen = rgb(150, 255, 70);

int sgn(int v) { return (v > 0) - (v < 0); }
int sgnf(float v) { return (v > 0.0f) - (v < 0.0f); }

bool visible(float x, float y, float w, float h)
{
  return x + w > -96.0f && x < float(kScreenW) + 96.0f && y + h > -96.0f && y < float(kScreenH) + 96.0f;
}

// A line between two points (cells), its high end first.
SilkLine makeLine(float x0, float y0, float x1, float y1)
{
  SilkLine l;
  if (y1 < y0)
  {
    std::swap(x0, x1);
    std::swap(y0, y1);
  }
  l.ax = x0;
  l.ay = y0;
  l.bx = x1;
  l.by = y1;
  const float dx = x1 - x0, dy = y1 - y0;
  l.len = std::sqrt(dx * dx + dy * dy);
  l.ux = l.len > 0.0f ? dx / l.len : 0.0f;
  l.uy = l.len > 0.0f ? dy / l.len : 1.0f;
  l.spun = l.len;
  return l;
}

// Where a Loom Spider is `s` cells along its line: astride the strand.
void spiderAt(const SilkLine& l, float s, Enemy& e)
{
  float x = 0.0f, y = 0.0f;
  l.point(std::clamp(s, 0.0f, l.len), x, y);
  e.x = int(std::lround(x - float(e.w) * 0.5f));
  e.y = int(std::lround(y)) + e.h / 2;
}

} // namespace

// --- Setup ---------------------------------------------------------------------------

bool World::setupSilkEntity(const EntityDef& e)
{
  if (e.kind == "silk" && e.hasPos && e.has("to"))
  {
    // `@ silk x y to=x,y spin=1 spider=1`: a line between two blocks,
    // through the top middle of each (a runner hanging from a point in
    // block row y stands on a floor at row y + 3). `spin=1`: spun as you
    // come near; it isn't there before. `spider=1`: a Loom Spider on it.
    const auto to = e.list("to");
    if (to.size() < 2 || to[1] == e.y)
      return true;
    SilkLine l = makeLine(float(e.x * kCellsPerTile + 1), float(e.y * kCellsPerTile), float(to[0] * kCellsPerTile + 1),
      float(to[1] * kCellsPerTile));
    l.id = e.id;
    if (e.num("spin", 0) != 0)
    {
      l.spin = true;
      l.spun = 0.0f;
    }
    mSpace.lines.push_back(l);
    mSpace.silk = true;
    if (e.num("spider", 0) != 0)
    {
      // `spider=1`: a Loom Spider on it, at its top.
      spawnEnemy(enemyIndex("loom_spider"), int(l.ax), int(l.ay));
      Enemy& sp = mEnemies.back();
      sp.attach = int(mSpace.lines.size());
      sp.ox = 0;
      sp.dir = 1;
      spiderAt(l, 0.0f, sp);
      sp.prevX = sp.x;
      sp.prevY = sp.y;
    }
    return true;
  }
  if (e.kind == "cocoon" && e.hasPos)
  {
    // `@ cocoon x y gems=N virus=1`: a cocoon hanging in block (x, y) on a
    // strand from the rock above. Shoot it open for its gems; the green
    // one (virus=1) bursts on you if you touch it.
    SilkCocoon c;
    c.x = e.x * kCellsPerTile;
    c.y = e.y * kCellsPerTile + kCellsPerTile - SilkCocoon::kH;
    c.gems = e.num("gems", 0);
    c.virus = e.num("virus", 0) != 0;
    mSpace.cocoons.push_back(c);
    mSpace.silk = true;
    return true;
  }
  return false;
}

void World::finishSilkSetup()
{
  // Spiders onto the nearest line; each pod gets its Dropling, hidden in it.
  std::vector<int> pods;
  for (std::size_t i = 0; i < mEnemies.size(); ++i)
  {
    Enemy& e = mEnemies[i];
    if (e.kind == EnemyKind::CocoonPod)
      pods.push_back(int(i));
    if (e.kind != EnemyKind::LoomSpider || e.attach > 0)
      continue;
    const float cx = float(e.x) + float(e.w) * 0.5f, cy = float(e.y - e.h);
    float best = 1e9f;
    for (std::size_t li = 0; li < mSpace.lines.size(); ++li)
    {
      const SilkLine& l = mSpace.lines[li];
      const float t = std::clamp((cx - l.ax) * l.ux + (cy - l.ay) * l.uy, 0.0f, l.len);
      float px = 0.0f, py = 0.0f;
      l.point(t, px, py);
      const float d = std::hypot(px - cx, py - cy);
      if (d < best)
      {
        best = d;
        e.attach = int(li) + 1;
        e.ox = l.spin ? 0 : int(t);
      }
    }
    e.dir = 1;
    if (e.attach > 0)
      spiderAt(mSpace.lines[std::size_t(e.attach - 1)], float(e.ox), e);
    e.prevX = e.x;
    e.prevY = e.y;
  }
  const int dropling = enemyIndex("dropling");
  for (int pi : pods)
  {
    const Enemy pod = mEnemies[std::size_t(pi)];
    spawnEnemy(dropling, pod.x, pod.y);
    Enemy& d = mEnemies.back();
    d.hidden = true;
    d.railX0 = pi;     // its pod
    d.aimX = pod.x;    // where it rests, in the pod
    d.aimY = pod.y;
    mEnemies[std::size_t(pi)].aimX = int(mEnemies.size()) - 1; // the pod's Dropling
  }
  mSpace.linesAtStart = mSpace.lines;
  mSpace.cocoonsAtStart = mSpace.cocoons;
}

void World::resetSilk()
{
  mSpace.lines = mSpace.linesAtStart;
  mPlayer.silk = -1;
  for (auto& e : mEnemies)
  {
    if (e.kind == EnemyKind::LoomSpider)
      e.tell = 0;
    if (e.kind == EnemyKind::Dropling && e.alive)
    {
      e.attach = 0;
      e.hidden = true;
      e.x = e.prevX = e.aimX;
      e.y = e.prevY = e.aimY;
    }
  }
}

// --- Riding --------------------------------------------------------------------------

bool World::silkHangSpot(const SilkLine& l, float s, int& x, int& y) const
{
  float hx = 0.0f, hy = 0.0f;
  l.point(s, hx, hy);
  x = int(std::lround(hx - 1.5f));
  y = int(std::lround(hy)) + 5;
  return x >= 0 && x + Player::kWidth <= mMap.width() && y - 5 >= 0 && y < mMap.height() &&
    !mMap.overlapsSolid(boxAt(x, y, Player::kWidth, 6));
}

bool World::tryGrabSilk()
{
  auto& p = mPlayer;
  const float hx = float(p.x) + 1.5f, hy = float(p.y - Player::kHeight + 1);
  for (std::size_t i = 0; i < mSpace.lines.size(); ++i)
  {
    const SilkLine& l = mSpace.lines[i];
    if (l.cool > 0 || l.spun < 2.0f)
      continue;
    const float dx = hx - l.ax, dy = hy - l.ay;
    const float t = dx * l.ux + dy * l.uy;
    if (t < -0.5f || t > l.spun - 1.0f)
      continue;
    // Close enough, or the hands went right through it since last frame
    // (falling fast).
    const float side = dx * l.uy - dy * l.ux;
    const float pdx = float(p.prevX) + 1.5f - l.ax, pdy = float(p.prevY - Player::kHeight + 1) - l.ay;
    const float prevSide = pdx * l.uy - pdy * l.ux;
    if (std::abs(side) > kGrabReach && (side > 0.0f) == (prevSide > 0.0f))
      continue;
    const float s = std::max(0.0f, t);
    int x = 0, y = 0;
    if (!silkHangSpot(l, s, x, y))
      continue;
    p.state = PlayerState::Swing;
    p.silk = int(i);
    p.silkS = s;
    // A jump in carries some speed on along it.
    p.silkV = std::max(kStartSpeed, std::min(1.4f, std::abs(float(p.fling)) * 0.6f));
    p.vine = -1;
    p.fling = 0;
    p.vineArc = false;
    p.kick = 0;
    p.kickArc = false;
    p.frames = 0;
    p.somersault = -1;
    p.fromLadder = false;
    p.x = x;
    p.y = y;
    if (l.ux != 0.0f)
      p.facing = sgnf(l.ux);
    setVisual(PlayerVisual::Hanging);
    playSound(Sfx::Zip);
    return true;
  }
  return false;
}

void World::updateSilkRide(int mvX, int mvY, const PlayerInput& in)
{
  auto& p = mPlayer;
  if (p.silk < 0 || std::size_t(p.silk) >= mSpace.lines.size())
  {
    p.silk = -1;
    startFallingDelayed();
    return;
  }
  SilkLine& l = mSpace.lines[std::size_t(p.silk)];
  setVisual(PlayerVisual::Hanging);
  if (l.spun < p.silkS + 0.5f)
  {
    letGoOfSilk(false, true); // the line went slack under you
    return;
  }
  if (in.jump.triggered)
  {
    letGoOfSilk(true, false);
    return;
  }
  if (mvY > 0)
  {
    letGoOfSilk(false, false); // down drops off
    return;
  }
  // Faster and faster, the steeper the faster; pushing along it helps a bit.
  p.silkV += 0.05f + 0.2f * l.uy + (mvX != 0 && mvX == sgnf(l.ux) ? 0.03f : 0.0f);
  if (p.virus > 0)
    p.silkV = std::min(p.silkV, kMaxSpeed * 0.5f);
  p.silkV = std::min(p.silkV, p.turbo > 0 ? kMaxSpeed * 1.25f : kMaxSpeed);
  const float end = std::min(l.len, l.spun);
  const float s = std::min(end, p.silkS + p.silkV);
  int x = 0, y = 0;
  if (!silkHangSpot(l, s, x, y))
  {
    letGoOfSilk(false, true); // something solid in the way
    return;
  }
  p.silkS = s;
  p.x = x;
  p.y = y;
  if (l.ux != 0.0f)
    p.facing = sgnf(l.ux);
  ++p.frames;
  if (s >= end)
    letGoOfSilk(false, true); // off the end, still going
}

void World::letGoOfSilk(bool jump, bool fall)
{
  auto& p = mPlayer;
  SilkLine& l = mSpace.lines[std::size_t(p.silk)];
  l.cool = kRegrab;
  // You keep the speed you had along it, sideways.
  int fling = int(std::lround(l.ux * p.silkV));
  fling = std::clamp(fling, -2, 2);
  if (jump && fling == 0 && l.ux != 0.0f)
    fling = sgnf(l.ux);
  p.silk = -1;
  p.silkS = p.silkV = 0.0f;
  p.y -= 1; // back to a 5-cell box, hands where they were
  if (mMap.overlapsSolid(p.box()))
    ++p.y;
  p.somersault = -1;
  p.frames = 0;
  p.vineArc = false;
  p.fling = fall || jump ? fling : 0;
  if (p.fling != 0)
    p.facing = sgn(p.fling);
  if (jump)
  {
    // A long jump: the usual arc, carried along at the line's speed.
    p.state = PlayerState::Jumping;
    p.fromLadder = true;
    p.jumpRequested = false;
    setVisual(PlayerVisual::Jumping);
    playSound(Sfx::Jump);
    return;
  }
  p.state = PlayerState::Falling;
  setVisual(PlayerVisual::Falling);
}

void World::cutSilk(SilkLine& l)
{
  const bool riding = mPlayer.silk >= 0 && &mSpace.lines[std::size_t(mPlayer.silk)] == &l;
  if (riding)
    letGoOfSilk(false, true);
  // Snapped: bits of silk drift down along where it was.
  for (int k = 1; k < 6; ++k)
  {
    float x = 0.0f, y = 0.0f;
    l.point(l.spun * float(k) / 6.0f, x, y);
    burst({x * kCellPx, y * kCellPx}, kSilk, rgb(200, 190, 170), 3, 0.8f, false);
  }
  l.spun = 0.0f;
  l.spinning = false;
  l.twang = 0;
  l.regrow = l.mine ? -1 : kRegrow;
  playSound(Sfx::Snip);
}

// --- The Silk Shooter ----------------------------------------------------------------

bool World::silkShotHit(Projectile& pr)
{
  if (pr.kind != ShotKind::Proto || pr.proto != int(ProtoId::SilkShooter) || pr.anchorX < 0)
    return false;
  // Where it went in: the cell before the rock.
  const float ex = float(pr.x - pr.dx) + float(pr.w) * 0.5f, ey = float(pr.y - pr.dy);
  const float ax = float(pr.anchorX) + 0.5f, ay = float(pr.anchorY);
  if (ey < ay + 2.0f || std::hypot(ex - ax, ey - ay) < 6.0f)
    return false;
  // At most two of your own: the oldest goes.
  int mine = 0;
  for (auto it = mSpace.lines.rbegin(); it != mSpace.lines.rend(); ++it)
    if (it->mine && it->regrow >= 0 && it->spun > 0.0f && ++mine >= kShooterLines)
      cutSilk(*it);
  SilkLine l = makeLine(ax, ay, ex, ey);
  l.mine = true;
  l.spun = 0.0f;
  l.spinning = true; // it pays out fast from your hands
  l.id = "shot";
  mSpace.lines.push_back(l);
  mSpace.silk = true;
  burst({ex * kCellPx, ey * kCellPx}, kSilk, rgb(220, 200, 160), 8, 1.2f, false);
  playSound(Sfx::Zip);
  return true;
}

// --- Cocoons -------------------------------------------------------------------------

bool World::shotAtCocoon(Projectile& pr)
{
  const CellBox b = pr.box();
  for (auto& c : mSpace.cocoons)
  {
    if (c.popped || !c.box().intersects(b))
      continue;
    c.popped = true;
    const Vec2 at = cellCenter(c.box());
    if (c.virus)
      burst(at, kVirusGreen, rgb(40, 120, 30), 16, 1.4f); // burst safely, out of reach
    else
    {
      burst(at, kSilk, rgb(255, 230, 160), 14, 1.4f);
      if (c.gems > 0)
        dropGems(c.x, c.y + SilkCocoon::kH - 1, c.gems);
    }
    playSound(Sfx::Squelch);
    return true;
  }
  return false;
}

// --- Each frame ----------------------------------------------------------------------

void World::updateSilk()
{
  auto& p = mPlayer;
  const CellBox pb = p.box();
  for (auto& l : mSpace.lines)
  {
    if (l.cool > 0)
      --l.cool;
    if (l.twang > 0)
      --l.twang;
    if (l.regrow > 0 && --l.regrow == 0)
      l.spinning = true;
    // A line to be spun starts as you come near its top.
    if (l.spin && std::abs(float(pb.x + 1) - l.ax) <= 30.0f && std::abs(float(pb.bottom()) - l.ay) <= 20.0f &&
        p.state != PlayerState::Dying)
    {
      l.spin = false;
      l.spinning = true;
    }
    if (l.spinning)
    {
      l.spun = std::min(l.len, l.spun + (l.mine ? 4.0f : kSpinSpeed));
      if (l.spun >= l.len)
        l.spinning = false;
    }
  }
  // The green cocoon bursts on you.
  for (auto& c : mSpace.cocoons)
    if (!c.popped && c.virus && c.box().intersects(pb) && p.state != PlayerState::Dying)
    {
      c.popped = true;
      burst(cellCenter(c.box()), kVirusGreen, rgb(40, 120, 30), 18, 1.8f);
      playSound(Sfx::Squelch);
      if (p.virus == 0)
        infect();
    }
}

// --- Aliens --------------------------------------------------------------------------

// Loom Spider: walks the top of its line, back and forth. Ride its line
// and it stops, the line shivers (the tell) and it cuts it; a while later
// it spins it again, riding the end of the new strand down.
void World::updateLoomSpider(Enemy& e, const EnemyDef& def)
{
  if (e.attach <= 0 || std::size_t(e.attach - 1) >= mSpace.lines.size())
    return;
  SilkLine& l = mSpace.lines[std::size_t(e.attach - 1)];
  if (e.cool > 0)
    --e.cool;
  if (e.stun > 0)
    return;
  if (l.spin || !l.whole())
  {
    // Waiting at the top, or spinning: at the end of the strand.
    e.ox = std::max(0, int(l.spun) - 1);
    e.tell = 0;
    spiderAt(l, float(e.ox), e);
    e.dir = 1;
    return;
  }
  const bool riding = mPlayer.silk == e.attach - 1;
  if (e.tell > 0)
  {
    l.twang = e.tell;
    if (--e.tell == 0)
    {
      cutSilk(l);
      e.cool = 20;
      e.ox = 0;
      spiderAt(l, 0.0f, e);
    }
    return;
  }
  if (riding && e.cool == 0)
  {
    e.tell = std::max(1, def.tell);
    l.twang = e.tell;
    return;
  }
  const int top = std::max(1, std::min(def.range, int(l.len) - 2));
  if (e.ox > top)
  {
    // Done spinning: it scuttles back up to the top.
    e.dir = -1;
    e.ox = std::max(top, e.ox - 2);
    spiderAt(l, float(e.ox), e);
    return;
  }
  if (e.timer % std::max(1, def.stepEvery) != 0)
    return;
  if (e.ox + e.dir < 0 || e.ox + e.dir > top)
    e.dir = -e.dir;
  e.ox = std::clamp(e.ox + e.dir, 0, top);
  spiderAt(l, float(e.ox), e);
}

// Cocoon Pod: hangs still; as you pass under it, it lets its Dropling down.
void World::updateCocoonPod(Enemy& e, const EnemyDef& def)
{
  if (e.cool > 0)
    --e.cool;
  if (e.aimX < 0 || std::size_t(e.aimX) >= mEnemies.size())
    return;
  Enemy& d = mEnemies[std::size_t(e.aimX)];
  if (!d.alive || d.attach != 0 || e.cool > 0)
    return;
  const CellBox pb = mPlayer.box();
  const bool under = pb.right() >= e.x - def.range && pb.x <= e.x + e.w - 1 + def.range && pb.y > e.y &&
    pb.y - e.y <= 30 && mPlayer.state != PlayerState::Dying;
  if (!under)
    return;
  d.hidden = false;
  d.attach = 1;
  d.x = d.prevX = e.x;
  d.y = d.prevY = e.y + d.h;
  e.cool = def.cooldown;
  playSound(Sfx::Chitter);
}

// Dropling: down out of its pod on a thread, a moment's bite, back up.
void World::updateDropling(Enemy& e, const EnemyDef& def)
{
  const bool podAlive = e.railX0 >= 0 && std::size_t(e.railX0) < mEnemies.size() &&
    mEnemies[std::size_t(e.railX0)].alive;
  if (!podAlive && e.attach != 4)
  {
    if (e.hidden)
    {
      e.alive = false; // went with its pod
      return;
    }
    e.attach = 4; // its thread is gone: it drops
  }
  switch (e.attach)
  {
    case 1: // dropping
      for (int k = 0; k < 2; ++k)
        if (mMap.moveVertically(e.x, e.y, e.w, e.h, 1) != MoveResult::Completed || e.y >= mPlayer.y)
        {
          e.attach = 2;
          e.tell = std::max(1, def.tell);
          break;
        }
      if (e.y > mMap.height() + 4)
        e.alive = false;
      return;
    case 2: // biting
      if (--e.tell <= 0)
        e.attach = 3;
      return;
    case 3: // back up
      if (e.y > e.aimY)
        mMap.moveVertically(e.x, e.y, e.w, e.h, -1);
      if (e.y <= e.aimY || (e.prevY == e.y && e.timer % 4 == 3))
      {
        e.attach = 0;
        e.hidden = true;
        e.x = e.prevX = e.aimX;
        e.y = e.prevY = e.aimY;
      }
      return;
    case 4: // falling free
      mMap.moveVertically(e.x, e.y, e.w, e.h, 1);
      if (e.y > mMap.height() + 4)
        e.alive = false;
      return;
    default:
      return;
  }
}

// --- Drawing -------------------------------------------------------------------------

void World::drawSilkBack(Renderer& r, float camX, float camY, int frame, float /*alpha*/) const
{
  if (!mSpace.silk)
    return;
  // The lines: a pale strand with a glint running down it, knots at the
  // anchors; a line about to be cut shivers.
  for (const auto& l : mSpace.lines)
  {
    const float x0 = l.ax * kCellPx - camX, y0 = l.ay * kCellPx - camY;
    const float x1 = l.bx * kCellPx - camX, y1 = l.by * kCellPx - camY;
    if (!visible(std::min(x0, x1), std::min(y0, y1), std::abs(x1 - x0), std::abs(y1 - y0)))
      continue;
    if (!l.mine || l.spun > 0.0f)
    {
      // The anchors: a knot, tied off by strands to the rock above.
      auto knot = [&](float ax, float ay, float x, float y, int alpha) {
        int top = int(ay) - 1;
        while (top > 0 && !mMap.solid(int(ax), top) && int(ay) - top < 16)
          --top;
        if (mMap.solid(int(ax), top))
          for (const float spread : {-0.6f, 0.6f})
            r.drawLine(x, y, x + spread * kCellPx, float(top + 1) * kCellPx - camY, 1.6f, rgba(246, 240, 224, alpha));
        r.fillRect(x - 6.0f, y - 6.0f, 12.0f, 12.0f, rgba(236, 226, 200, alpha));
      };
      knot(l.ax, l.ay, x0, y0, 220);
      if (l.whole() || l.spin)
        knot(l.bx, l.by, x1, y1, l.spin ? 90 : 220);
    }
    if (l.spun <= 0.0f)
      continue;
    const float sx = x0 + (x1 - x0) * (l.spun / l.len), sy = y0 + (y1 - y0) * (l.spun / l.len);
    const int segs = std::max(2, int(l.spun / 3.0f));
    float px = x0, py = y0;
    for (int k = 1; k <= segs; ++k)
    {
      const float t = float(k) / float(segs);
      float x = x0 + (sx - x0) * t, y = y0 + (sy - y0) * t;
      if (l.twang > 0)
        y += 6.0f * std::sin(t * 3.14159f) * std::sin(float(frame) * 1.9f); // shivering
      r.drawLine(px, py, x, y, 5.0f, rgba(90, 70, 50, 90));
      r.drawLine(px, py, x, y, 2.5f, l.twang > 0 ? rgba(255, 200, 160, 255) : withAlpha(kSilk, 235));
      px = x;
      py = y;
    }
    // A glint sliding down it.
    const float g = std::fmod(float(frame) * 0.025f + l.ax * 0.01f, 1.0f) * (l.spun / l.len);
    drawGlow(r, mArt, x0 + (x1 - x0) * g, y0 + (y1 - y0) * g, 18.0f, rgb(255, 250, 230), 0.6f);
  }
  // Droplings' threads up to their pods.
  for (const auto& e : mEnemies)
  {
    if (!e.alive || e.kind != EnemyKind::Dropling || e.hidden || e.attach == 4)
      continue;
    const float x = (float(e.x) + float(e.w) * 0.5f) * kCellPx - camX;
    r.drawLine(x, float(e.aimY - 1) * kCellPx - camY, x, float(e.y - e.h + 1) * kCellPx - camY, 2.0f,
      withAlpha(kSilk, 200));
  }
  // Pods and cocoons hang on strands from the rock above.
  auto strand = [&](int cx, int top) {
    int y = top - 1;
    while (y > 0 && !mMap.solid(cx, y) && top - y < 24)
      --y;
    const float x = (float(cx) + 0.5f) * kCellPx - camX;
    r.drawLine(x, float(y + 1) * kCellPx - camY, x, float(top) * kCellPx - camY + 6.0f, 2.0f, withAlpha(kSilk, 200));
  };
  for (const auto& e : mEnemies)
    if (e.alive && e.kind == EnemyKind::CocoonPod)
      strand(e.x + e.w / 2, e.y - e.h + 1);
  for (std::size_t i = 0; i < mSpace.cocoons.size(); ++i)
  {
    const SilkCocoon& c = mSpace.cocoons[i];
    const float x = (float(c.x) + 1.0f) * kCellPx - camX, y = float(c.y + SilkCocoon::kH) * kCellPx - camY;
    if (!visible(x - 64.0f, y - 160.0f, 128.0f, 180.0f))
      continue;
    if (c.popped)
    {
      // The empty husk.
      r.draw(styledEnemySprite(mArt, r, mTheme, "silk_cocoon", 2, 0, 2, 3).get(1), x, y);
      continue;
    }
    strand(c.x + 1, c.y);
    if (c.virus)
      drawGlow(r, mArt, x, y - 48.0f, 60.0f, kVirusGreen, 0.35f + 0.1f * std::sin(float(frame) * 0.2f));
    DrawOpts o;
    o.angle = 6.0f * std::sin(float(frame + int(i) * 17) * 0.06f); // swaying
    r.draw(styledEnemySprite(mArt, r, mTheme, "silk_cocoon", c.virus ? 1 : 0, (frame / 10) % 2, 2, 3).get(1), x, y, o);
  }
}

} // namespace gr
