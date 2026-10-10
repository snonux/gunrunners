// Episode 7, DEEP SPACE (docs/DEEP_SPACE.md). Level 45, Hive Gullets: the
// Gullet Tubes that swallow you and spit you out somewhere else (valves pick
// the branch, some mouths only open while the hive breathes in), Bile
// Blaster shots that ride the tubes too, mite pores, Hive Mites, Polyps and
// Drone Wardens.

#include "game/world.hpp"

#include "assets/enemy_art.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace gr
{

namespace
{

constexpr float kCellPx = float(kCellSize) * kPixelScale; // 32
constexpr int kBreathIn = 40;   // frames of each breath the breathing mouths are open
constexpr int kShotSpeed = 3;   // cells a frame a Bile Blaster shot goes down a tube
constexpr int kPoreRange = 26;  // cells: how close you come before a pore rattles
constexpr int kPoreRattle = 12; // frames of rattling before the first mite
constexpr int kPoreGap = 5;     // frames between mites
constexpr int kPoreRearm = 150; // frames after its last mite dies
constexpr int kLaunch = 10;     // cells a floor mouth spits you up
const Color kFlesh = rgb(255, 120, 160);
const Color kFleshDeep = rgb(150, 30, 80);
const Color kBile = rgb(210, 240, 70);

int sgn(int v) { return (v > 0) - (v < 0); }

float lerpCells(int prev, int cur, float alpha)
{
  return (float(prev) + float(cur - prev) * alpha) * kCellPx;
}

bool visible(float x, float y, float w, float h)
{
  return x + w > -96.0f && x < float(kScreenW) + 96.0f && y + h > -96.0f && y < float(kScreenH) + 96.0f;
}

bool parseDir(const std::string& s, int& dx, int& dy)
{
  dx = s == "r" ? 1 : (s == "l" ? -1 : 0);
  dy = s == "d" ? 1 : (s == "u" ? -1 : 0);
  return dx != 0 || dy != 0;
}

// The middle of a mouth's ring (cells): wall mouths are two blocks tall
// (the block given and the one above), floor and ceiling mouths two wide
// (the block given and the one right of it).
std::pair<int, int> ringCentre(int bx, int by, int dx)
{
  return dx != 0 ? std::pair<int, int>{bx * kCellsPerTile + 1, by * kCellsPerTile}
                 : std::pair<int, int>{bx * kCellsPerTile + 2, by * kCellsPerTile + 1};
}

// The ring's own cells (in the wall).
CellBox ringBox(int bx, int by, int dx)
{
  return dx != 0 ? CellBox{bx * kCellsPerTile, (by - 1) * kCellsPerTile, 2, 4}
                 : CellBox{bx * kCellsPerTile, by * kCellsPerTile, 4, 2};
}

// A point s cells along a branch.
void pointAt(const std::vector<std::pair<int, int>>& path, float s, float& x, float& y)
{
  x = float(path.front().first);
  y = float(path.front().second);
  for (std::size_t i = 1; i < path.size(); ++i)
  {
    const float ax = float(path[i - 1].first), ay = float(path[i - 1].second);
    const float bx = float(path[i].first), by = float(path[i].second);
    const float len = std::hypot(bx - ax, by - ay);
    if (s <= len || i + 1 == path.size())
    {
      const float t = len > 0.0f ? std::clamp(s / len, 0.0f, 1.0f) : 1.0f;
      x = ax + (bx - ax) * t;
      y = ay + (by - ay) * t;
      return;
    }
    s -= len;
  }
}

} // namespace

// --- Setup ----------------------------------------------------------------------------

bool World::setupHiveEntity(const EntityDef& e)
{
  if (e.kind == "tube")
  {
    GulletTube t;
    t.id = e.id;
    const auto main = e.path("path");
    if (main.size() < 2 || !parseDir(e.str("in", "l"), t.inX, t.inY))
    {
      std::fprintf(stderr, "level line %d: a tube needs path= (2+ points) and in=\n", e.line);
      return true;
    }
    t.mx = main.front().first;
    t.my = main.front().second;
    const auto alt = e.path("alt");
    t.valve = !alt.empty() && e.has("valve");
    for (int b = 0; b < (t.valve ? 2 : 1); ++b)
    {
      const auto& pts = b == 0 ? main : alt;
      if (!parseDir(e.str(b == 0 ? "out" : "altout", e.str("out", "r")), t.outX[std::size_t(b)], t.outY[std::size_t(b)]))
        t.outX[std::size_t(b)] = 1;
      t.ex[std::size_t(b)] = pts.back().first;
      t.ey[std::size_t(b)] = pts.back().second;
      auto& cells = t.path[std::size_t(b)];
      cells.push_back(ringCentre(t.mx, t.my, t.inX));
      for (std::size_t i = 1; i + 1 < pts.size(); ++i)
        cells.push_back({pts[i].first * kCellsPerTile + 1, pts[i].second * kCellsPerTile + 1});
      cells.push_back(ringCentre(pts.back().first, pts.back().second, t.outX[std::size_t(b)]));
      float len = 0.0f;
      for (std::size_t i = 1; i < cells.size(); ++i)
        len += std::hypot(float(cells[i].first - cells[i - 1].first), float(cells[i].second - cells[i - 1].second));
      t.len[std::size_t(b)] = std::max(1, int(std::lround(len)));
    }
    if (t.valve)
    {
      const auto v = e.list("valve");
      if (v.size() >= 2)
      {
        t.vx = v[0] * kCellsPerTile + 1;
        t.vy = v[1] * kCellsPerTile + 1;
      }
      t.set = e.num("set", 0) != 0 ? 1 : 0;
    }
    t.breath = e.num("breath", 0) != 0;
    t.pore = e.str("look") == "pore";
    t.gulp = 99;
    mSpace.tubes.push_back(std::move(t));
    mSpace.hive = true;
    return true;
  }
  if (e.kind == "pore" && e.hasPos)
  {
    MitePore p;
    int dy = 0;
    parseDir(e.str("dir", "r"), p.dir, dy);
    if (p.dir == 0)
      p.dir = 1;
    p.x = p.dir > 0 ? (e.x + 1) * kCellsPerTile : e.x * kCellsPerTile - 1;
    p.y = e.y * kCellsPerTile + 1;
    p.count = std::max(1, e.num("count", 3));
    mSpace.pores.push_back(p);
    mSpace.hive = true;
    return true;
  }
  if (e.kind == "drip" && e.hasPos)
  {
    mSpace.drips.push_back({e.x * kCellsPerTile + 1, e.y * kCellsPerTile});
    mSpace.hive = true;
    return true;
  }
  return false;
}

// --- Tubes ----------------------------------------------------------------------------

bool World::mouthOpen(const GulletTube& t) const
{
  return !t.breath || clock() % kBreathPeriod < kBreathIn;
}

CellBox World::mouthTrigger(const GulletTube& t) const
{
  const int x = t.mx * kCellsPerTile, y = t.my * kCellsPerTile;
  if (t.inX > 0)
    return {x + 2, y - 2, 1, 4};
  if (t.inX < 0)
    return {x - 1, y - 2, 1, 4};
  if (t.inY < 0)
    return {x, y - 1, 4, 1};
  return {x, y + 2, 4, 1};
}

void World::tubeExit(const GulletTube& t, int branch, int& x, int& y) const
{
  const std::size_t b = std::size_t(branch);
  const int ex = t.ex[b] * kCellsPerTile, ey = t.ey[b] * kCellsPerTile;
  if (t.outX[b] > 0)
  {
    x = ex + 2;
    y = ey + 1;
  }
  else if (t.outX[b] < 0)
  {
    x = ex - Player::kWidth;
    y = ey + 1;
  }
  else if (t.outY[b] < 0)
  {
    x = ex;
    y = ey - 1;
  }
  else
  {
    x = ex;
    y = ey + 2 + Player::kHeight - 1;
  }
}

void World::swallowPlayer(int tube)
{
  auto& p = mPlayer;
  GulletTube& t = mSpace.tubes[std::size_t(tube)];
  p.tube = tube;
  p.tubeBranch = t.valve ? t.set : 0;
  p.tubeS = 0;
  p.frames = 0;
  p.state = PlayerState::Falling;
  p.somersault = -1;
  p.wall = p.kick = 0;
  p.kickArc = p.vineArc = false;
  p.fling = 0;
  mLaunch = 0;
  setVisual(PlayerVisual::Falling);
  t.gulp = 0;
  const auto [cx, cy] = t.path[0].front();
  burst({(float(cx) + 0.5f) * kCellSize, (float(cy) + 0.5f) * kCellSize}, kFlesh, rgb(255, 220, 230), 10, 1.4f);
  playSound(Sfx::Gulp);
}

void World::updateTubeRide()
{
  auto& p = mPlayer;
  GulletTube& t = mSpace.tubes[std::size_t(p.tube)];
  const int b = p.tubeBranch;
  ++p.frames;
  p.tubeS += p.frames < 3 ? 1 : 2;
  if (p.tubeS < t.len[std::size_t(b)])
  {
    float x = 0.0f, y = 0.0f;
    pointAt(t.path[std::size_t(b)], float(p.tubeS), x, y);
    p.x = int(std::lround(x)) - 1;
    p.y = int(std::lround(y)) + 2;
    return;
  }
  // Spat out at the far end.
  int x = 0, y = 0;
  tubeExit(t, b, x, y);
  p.tube = -1;
  p.x = p.prevX = x;
  p.y = p.prevY = y;
  if (t.outX[std::size_t(b)] != 0)
    p.facing = t.outX[std::size_t(b)];
  if (!mSimulation && mMap.overlapsSolid(p.box()))
    std::fprintf(stderr, "tube %s spits the runner into a wall at %d,%d\n", t.id.c_str(), x, y);
  t.gulp = 0;
  if (t.outY[std::size_t(b)] < 0)
  {
    startLaunch(kLaunch);
    setVisual(PlayerVisual::Somersault);
    p.somersault = 0;
  }
  else
    startFalling();
  const auto [cx, cy] = t.path[std::size_t(b)].back();
  burst({(float(cx) + 0.5f) * kCellSize, (float(cy) + 0.5f) * kCellSize}, kFlesh, rgb(255, 220, 230), 14, 1.8f);
  playSound(Sfx::Gulp);
}

bool World::shotAtHive(Projectile& pr)
{
  if (!mSpace.hive || pr.kind != ShotKind::Proto || pr.proto != int(ProtoId::BileBlaster))
    return false;
  for (std::size_t i = 0; i < mSpace.tubes.size(); ++i)
  {
    GulletTube& t = mSpace.tubes[i];
    if (!mouthOpen(t) || t.pore)
      continue;
    const bool into = t.inX != 0 ? pr.dx == -t.inX : pr.dy == -t.inY;
    CellBox ring = ringBox(t.mx, t.my, t.inX);
    ring = {ring.x - 1, ring.y - 1, ring.w + 2, ring.h + 2};
    if (!into || !ring.intersects(pr.box()))
      continue;
    mSpace.tubeShots.push_back({int(i), t.valve ? t.set : 0, 0, pr.damage});
    t.gulp = 0;
    playSound(Sfx::Gulp);
    return true;
  }
  return false;
}

bool World::shotAtValve(Projectile& pr)
{
  for (auto& t : mSpace.tubes)
    if (t.valve && t.flip > 6 && pr.box().intersects({t.vx - 1, t.vy - 1, 3, 3}))
    {
      t.set ^= 1;
      t.flip = 0;
      burst({(float(t.vx) + 0.5f) * kCellSize, (float(t.vy) + 0.5f) * kCellSize}, kFlesh, kBile, 10, 1.3f);
      playSound(Sfx::Squelch);
      return true;
    }
  return false;
}

void World::resetHive()
{
  mPlayer.tube = -1;
  mSpace.tubeShots.clear();
  mSpace.mites.clear();
  for (auto& p : mSpace.pores)
  {
    p.state = 0;
    p.t = 0;
    p.left = 0;
    p.mites.clear();
  }
}

void World::updateHive()
{
  if (!mSpace.hive)
    return;
  auto& p = mPlayer;
  for (auto& t : mSpace.tubes)
  {
    ++t.gulp;
    ++t.flip;
  }
  // Mouths swallow whoever steps into them.
  if (p.tube < 0 && p.cart < 0 && p.vehicle < 0 && p.state != PlayerState::Dying && p.state != PlayerState::Teleporting)
    for (std::size_t i = 0; i < mSpace.tubes.size(); ++i)
      if (mouthOpen(mSpace.tubes[i]) && mouthTrigger(mSpace.tubes[i]).intersects(p.box()))
      {
        swallowPlayer(int(i));
        break;
      }
  // Bile Blaster shots down the tubes: out the far end.
  for (auto& s : mSpace.tubeShots)
  {
    s.s += kShotSpeed;
    const GulletTube& t = mSpace.tubes[std::size_t(s.tube)];
    const std::size_t b = std::size_t(s.branch);
    if (s.s < t.len[b])
      continue;
    const int ex = t.ex[b] * kCellsPerTile, ey = t.ey[b] * kCellsPerTile;
    const int ax = t.outX[b] > 0 ? ex + 2 : (t.outX[b] < 0 ? ex - 1 : ex + 1);
    const int ay = t.outX[b] != 0 ? ey : (t.outY[b] < 0 ? ey - 1 : ey + 2);
    spawnProjectile(ShotKind::Proto, ax, ay, t.outX[b], t.outY[b]);
    Projectile& pr = mProjectiles.back();
    pr.proto = int(ProtoId::BileBlaster);
    pr.speed = std::max(1, protoDef(ProtoId::BileBlaster).speed);
    pr.damage = s.damage;
    pr.strong = true; // a gob of the hive's own juice: bigger
    s.s = -1;
    playSound(Sfx::Gulp);
  }
  mSpace.tubeShots.erase(std::remove_if(mSpace.tubeShots.begin(), mSpace.tubeShots.end(),
                           [](const TubeShot& s) { return s.s < 0; }),
    mSpace.tubeShots.end());
  // Mites a Warden called last frame drop from its belly.
  const int mite = enemyIndex("hive_mite");
  for (const auto& m : mSpace.mites)
  {
    if (mite < 0 || mMap.overlapsSolid(boxAt(m[0], m[1], 2, 2)))
      continue;
    spawnEnemy(mite, m[0], m[1]);
    Enemy& e = mEnemies.back();
    e.dir = m[2];
    e.active = true;
  }
  mSpace.mites.clear();
  // Pores: rattle when you come close, then spit their mites one by one.
  const CellBox pb = p.box();
  for (auto& pore : mSpace.pores)
  {
    ++pore.t;
    const int dx = (pb.x + 1) - pore.x, dy = pb.bottom() - pore.y;
    const bool near = std::abs(dx) <= kPoreRange && std::abs(dy) <= 12 && p.tube < 0 &&
      p.state != PlayerState::Dying && isOnScreen({pore.x - 1, pore.y - 2, 2, 3}, 0);
    switch (pore.state)
    {
      case 0:
        if (near)
        {
          pore.state = 1;
          pore.t = 0;
          if (isOnScreen({pore.x - 1, pore.y - 2, 2, 3}, 0))
            playSound(Sfx::Chitter);
        }
        break;
      case 1:
        if (pore.t >= kPoreRattle)
        {
          pore.state = 2;
          pore.t = 0;
          pore.left = pore.count;
          pore.mites.clear();
        }
        break;
      case 2:
        if (pore.t % kPoreGap == 1 && pore.left > 0 && mite >= 0)
        {
          const int x = pore.dir > 0 ? pore.x : pore.x - 1;
          if (!mMap.overlapsSolid(boxAt(x, pore.y, 2, 2)))
          {
            spawnEnemy(mite, x, pore.y);
            Enemy& e = mEnemies.back();
            e.dir = pore.dir;
            e.active = true;
            e.cool = 4;
            pore.mites.push_back(int(mEnemies.size()) - 1);
            burst({(float(pore.x) + 0.5f) * kCellSize, (float(pore.y) - 0.5f) * kCellSize}, kFlesh, kFleshDeep, 4,
              0.8f);
          }
          --pore.left;
        }
        if (pore.left <= 0)
        {
          pore.state = 3;
          pore.t = 0;
        }
        break;
      default:
      {
        bool any = false;
        for (int i : pore.mites)
          any = any || (std::size_t(i) < mEnemies.size() && mEnemies[std::size_t(i)].alive &&
                         mEnemies[std::size_t(i)].kind == EnemyKind::Mite);
        if (any)
          pore.t = 0;
        else if (pore.t >= kPoreRearm && !near)
          pore.state = 0;
        break;
      }
    }
  }
}

// --- Aliens ---------------------------------------------------------------------------

void World::updateMite(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  const CellBox b = e.box();
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
  // It runs at you, wherever you are on its floor or below it.
  const CellBox pb = p.box();
  const int dx = (pb.x + 1) - (b.x + 1);
  const bool chase = p.tube < 0 && p.state != PlayerState::Dying && std::abs(dx) <= def.range &&
    pb.bottom() >= b.bottom() - 8 && pb.bottom() <= b.bottom() + 24;
  if (chase && dx != 0 && e.timer % 4 == 0)
    e.dir = sgn(dx);
  if (e.timer % std::max(1, def.stepEvery) != 0)
    return;
  const bool wall = e.dir > 0 ? mMap.touchingRightWall(b) : mMap.touchingLeftWall(b);
  if (wall)
  {
    // A step up to two cells it scrambles up; higher, it turns.
    for (int up = 1; up <= 2; ++up)
      if (!mMap.overlapsSolid(boxAt(e.x + e.dir, e.y - up, e.w, e.h)))
      {
        e.x += e.dir;
        e.y -= up;
        return;
      }
    e.dir = -e.dir;
    return;
  }
  const int aheadX = e.dir > 0 ? b.right() + 1 : b.left() - 1;
  const bool ledge = !mMap.solidTop(aheadX, b.bottom() + 1);
  if (ledge && !(chase && pb.bottom() > b.bottom() + 2))
  {
    e.dir = -e.dir;
    return;
  }
  e.x += e.dir;
}

void World::updatePolyp(Enemy& e, const EnemyDef& def)
{
  auto& p = mPlayer;
  const CellBox b = e.box(), pb = p.box();
  const int mouthX = e.dir > 0 ? b.right() : b.left();
  const int dx = (pb.x + 1) - mouthX;
  const int mid = b.y + b.h / 2;
  const bool reach = (dx == 0 || sgn(dx) == e.dir) && std::abs(dx) <= def.range && pb.bottom() >= mid - 2 &&
    pb.y <= mid + 2 && p.state != PlayerState::Dying && p.tube < 0 && p.cart < 0 && isOnScreen(b, 0);
  if (e.attach == 2)
  {
    // Breathing in: you slide toward its mouth a cell every other frame.
    ++e.ox;
    if (reach && e.ox % 2 == 0 && p.state != PlayerState::Cling && p.turbo == 0)
      mMap.moveHorizontally(p.x, p.y, Player::kWidth, p.height(), -e.dir);
    if (e.ox % 3 == 0 && isOnScreen(b, 0))
    {
      const float fx = (float(mouthX) + float(e.dir) * float(4 + (e.ox * 5) % 12) + 0.5f) * kCellSize;
      burst({fx, (float(mid) + 0.5f) * kCellSize}, rgb(255, 200, 220), kFlesh, 2, 0.4f, false);
    }
    if (e.ox >= 45)
    {
      e.attach = 0;
      e.cool = def.cooldown;
    }
    return;
  }
  if (e.cool > 0)
  {
    --e.cool;
    return;
  }
  if (e.attach == 1)
  {
    // Puckering up: the tell.
    if (--e.tell <= 0)
    {
      e.tell = 0;
      e.attach = 2;
      e.ox = 0;
      playSound(Sfx::Inhale);
    }
    return;
  }
  if (reach)
  {
    e.attach = 1;
    e.tell = def.tell;
  }
}

void World::updateWarden(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  if (e.railX1 == 0)
  {
    // Home: where the level put it; it patrols `range` cells either side.
    e.railX0 = e.x;
    e.aimY = e.y;
    e.railX1 = 1;
  }
  const CellBox b = e.box(), pb = p.box();
  const int dx = (pb.x + 1) - (b.x + b.w / 2);
  const bool sees = p.tube < 0 && p.state != PlayerState::Dying && std::abs(dx) <= def.range + 12 &&
    pb.bottom() >= b.bottom() - 6 && pb.bottom() <= b.bottom() + 30 && isOnScreen(b, 0);
  if (dx != 0 && e.tell == 0)
    e.dir = sgn(dx);
  // Drift over you along its wing, bobbing.
  const int target = sees ? std::clamp(pb.x - 1, e.railX0 - def.range, e.railX0 + def.range) : e.railX0;
  if (e.timer % std::max(1, def.stepEvery) == 0 && target != e.x)
    mMap.moveHorizontally(e.x, e.y, e.w, e.h, sgn(target - e.x));
  const int bobY = e.aimY + ((e.timer / 10) % 2);
  if (e.timer % 3 == 0 && bobY != e.y)
    mMap.moveVertically(e.x, e.y, e.w, e.h, sgn(bobY - e.y));
  if (e.cool > 0)
    --e.cool;
  if (e.tell > 0)
  {
    // Calling: its belly glows, then two mites drop out.
    if (--e.tell == 0)
    {
      for (int side : {0, 2})
        mSpace.mites.push_back({e.x + side, e.y + 2, e.dir, -1});
      e.cool = def.cooldown;
    }
    return;
  }
  if (!sees || e.cool > 0)
    return;
  int mites = 0;
  for (const auto& o : mEnemies)
    mites += o.alive && o.kind == EnemyKind::Mite && std::abs(o.x - e.x) < 40;
  if (mites >= 4)
    return;
  e.tell = def.tell;
  playSound(Sfx::Screech);
}

// --- Drawing --------------------------------------------------------------------------

void World::drawHiveBack(Renderer& r, float camX, float camY, int frame, float /*alpha*/) const
{
  if (!mSpace.hive)
    return;
  // The hive's heartbeat: a lub-dub of light over everything.
  {
    const int beat = clock() % 30;
    const float a = beat < 4 ? 1.0f - float(beat) / 4.0f : (beat >= 6 && beat < 10 ? 0.6f * (1.0f - float(beat - 6) / 4.0f) : 0.0f);
    if (a > 0.0f)
      r.fillRect(0, 0, float(kScreenW), float(kScreenH), rgba(255, 40, 90, int(a * 22.0f)), Blend::Add);
  }
  const bool inhale = clock() % kBreathPeriod < kBreathIn;
  // Tubes: a translucent ribbed membrane along each branch; the branch the
  // valve sends you down is full, the other one slack.
  for (std::size_t ti = 0; ti < mSpace.tubes.size(); ++ti)
  {
    const auto& t = mSpace.tubes[ti];
    if (t.pore)
      continue;
    for (int b = (t.valve ? 1 : 0); b >= 0; --b)
    {
      const auto& path = t.path[std::size_t(b)];
      const bool live = !t.valve || t.set == b;
      const float wide = live ? 44.0f : 34.0f;
      const int rim = live ? 150 : 90;
      for (std::size_t i = 1; i < path.size(); ++i)
      {
        const float x0 = float(path[i - 1].first) * kCellPx - camX, y0 = float(path[i - 1].second) * kCellPx - camY;
        const float x1 = float(path[i].first) * kCellPx - camX, y1 = float(path[i].second) * kCellPx - camY;
        if (!visible(std::min(x0, x1) - 40.0f, std::min(y0, y1) - 40.0f, std::fabs(x1 - x0) + 80.0f, std::fabs(y1 - y0) + 80.0f))
          continue;
        r.drawLine(x0, y0, x1, y1, wide, rgba(110, 20, 60, rim));
        r.drawLine(x0, y0, x1, y1, wide - 12.0f, rgba(255, 130, 170, live ? 95 : 55));
        // Ribs across it.
        const float len = std::hypot(x1 - x0, y1 - y0);
        const float ux = len > 0.0f ? (x1 - x0) / len : 0.0f, uy = len > 0.0f ? (y1 - y0) / len : 0.0f;
        for (float d = 14.0f; d < len; d += 28.0f)
        {
          const float cx = x0 + ux * d, cy = y0 + uy * d;
          r.drawLine(cx - uy * wide * 0.5f, cy + ux * wide * 0.5f, cx + uy * wide * 0.5f, cy - ux * wide * 0.5f, 4.0f,
            rgba(255, 170, 200, live ? 110 : 60));
        }
        // A wet highlight along one side.
        r.drawLine(x0 - uy * wide * 0.28f, y0 + ux * wide * 0.28f - 0.0f, x1 - uy * wide * 0.28f, y1 + ux * wide * 0.28f,
          4.0f, rgba(255, 235, 245, live ? 90 : 40), Blend::Add);
        if (i + 1 < path.size())
        {
          // Round off the bend.
          r.fillRect(x1 - wide * 0.5f, y1 - wide * 0.5f, wide, wide, rgba(110, 20, 60, rim));
          r.fillRect(x1 - wide * 0.5f + 6.0f, y1 - wide * 0.5f + 6.0f, wide - 12.0f, wide - 12.0f,
            rgba(255, 130, 170, live ? 95 : 55));
        }
      }
      // A swallow travels along: peristalsis.
      if (live)
      {
        float px = 0.0f, py = 0.0f;
        pointAt(path, float((clock() * 2 + int(ti) * 37) % std::max(1, t.len[std::size_t(b)])), px, py);
        drawGlow(r, mArt, px * kCellPx - camX, py * kCellPx - camY, 36.0f, kFlesh, 0.35f);
      }
    }
    if (t.valve)
    {
      const float x = float(t.vx) * kCellPx - camX, y = float(t.vy) * kCellPx - camY;
      if (visible(x - 64.0f, y - 64.0f, 128.0f, 128.0f))
      {
        drawGlow(r, mArt, x, y, 50.0f, t.set == 0 ? kFlesh : kBile, t.flip < 8 ? 0.9f : 0.4f);
        r.draw(styledEnemySprite(mArt, r, mTheme, "gullet_valve", t.set, (frame / 10) % 2, 3, 3).get(1), x, y + 48.0f);
      }
    }
  }
  // Mouths: puckered rings, open when they can swallow.
  for (const auto& t : mSpace.tubes)
  {
    const CellBox ring = ringBox(t.mx, t.my, t.inX);
    const float x = (float(ring.x) + float(ring.w) * 0.5f) * kCellPx - camX;
    const float y = float(ring.y + ring.h) * kCellPx - camY;
    if (!visible(x - 128.0f, y - 160.0f, 256.0f, 192.0f))
      continue;
    const bool open = mouthOpen(t);
    int v = open ? 2 : 0;
    if (t.breath)
    {
      const int c = clock() % kBreathPeriod;
      if ((c >= kBreathIn - 6 && c < kBreathIn) || c >= kBreathPeriod - 8)
        v = 1; // about to close, about to open
    }
    if (t.gulp < 6)
      v = 3; // swallowing
    const char* key = t.pore ? "gullet_pore" : (t.inX != 0 ? "gullet_mouth" : "gullet_mouth_up");
    r.draw(styledEnemySprite(mArt, r, mTheme, key, v, (frame / 8) % 2, ring.w, ring.h).get(t.inX < 0 ? -1 : 1), x, y);
    if (open && !t.pore)
      drawGlow(r, mArt, x + float(t.inX) * 40.0f, y - float(ring.h) * kCellPx * 0.5f + float(t.inY) * 40.0f, 80.0f,
        kFlesh, inhale ? 0.4f : 0.28f);
  }
  // Exits: a smaller lip where the tube lets go.
  for (const auto& t : mSpace.tubes)
    for (int b = 0; b < (t.valve ? 2 : 1); ++b)
    {
      const std::size_t bi = std::size_t(b);
      const CellBox ring = ringBox(t.ex[bi], t.ey[bi], t.outX[bi]);
      const float x = (float(ring.x) + float(ring.w) * 0.5f) * kCellPx - camX;
      const float y = float(ring.y + ring.h) * kCellPx - camY;
      if (!visible(x - 128.0f, y - 160.0f, 256.0f, 192.0f))
        continue;
      const int v = t.gulp < 6 && (!t.valve || t.set == b) ? 1 : 0;
      const char* key = t.outX[bi] != 0 ? "gullet_lip" : "gullet_lip_up";
      r.draw(styledEnemySprite(mArt, r, mTheme, key, v, 0, ring.w, ring.h).get(t.outX[bi] < 0 ? -1 : 1), x, y);
    }
  // Mite pores and the green dripping ones.
  for (const auto& pore : mSpace.pores)
  {
    const float x = float(pore.x + (pore.dir > 0 ? 0 : 1)) * kCellPx - camX, y = (float(pore.y) + 1.0f) * kCellPx - camY;
    if (!visible(x - 64.0f, y - 96.0f, 128.0f, 128.0f))
      continue;
    const int v = pore.state == 1 || pore.state == 2 ? 1 : 0;
    const float shake = pore.state == 1 ? float((frame / 2) % 3 - 1) * 3.0f : 0.0f;
    r.draw(styledEnemySprite(mArt, r, mTheme, "mite_pore", v, (frame / 6) % 2, 1, 3).get(-pore.dir),
      x - float(pore.dir) * 14.0f + shake, y);
  }
  for (const auto& [dx, dy] : mSpace.drips)
  {
    const float x = (float(dx) + 0.5f) * kCellPx - camX, y = float(dy) * kCellPx - camY;
    if (!visible(x - 64.0f, y - 64.0f, 128.0f, 220.0f))
      continue;
    r.draw(styledEnemySprite(mArt, r, mTheme, "drip_pore", 0, (frame / 10) % 2, 3, 2).get(1), x, y + 30.0f);
    // A drip falling every so often.
    const int f = (frame + dx * 7) % 48;
    if (f < 24)
      r.fillRect(x - 3.0f, y + 32.0f + float(f * f) * 0.35f, 6.0f, 12.0f, rgba(150, 255, 90, 220));
  }
}

void World::drawHiveFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  if (!mSpace.hive)
    return;
  const auto& p = mPlayer;
  if (p.tube >= 0 && std::size_t(p.tube) < mSpace.tubes.size())
  {
    // The runner, curled up, shooting along inside the membrane.
    const auto& ca = mArt.runner(mCharacter);
    const float x = lerpCells(p.prevX, p.x, alpha) + 1.5f * kCellPx - camX;
    const float y = lerpCells(p.prevY + 1, p.y + 1, alpha) - camY - 2.5f * kCellPx;
    DrawOpts o;
    o.angle = float(frame % 24) * 15.0f * float(p.facing);
    o.alpha = 0.9f;
    o.scale = 0.8f;
    r.draw(ca.tuck.get(p.facing), x, y + 30.0f * 1.65f * 0.8f, o);
    drawGlow(r, mArt, x, y, 60.0f, kFlesh, 0.5f);
    // The membrane over it, stretched tight.
    r.fillRect(x - 26.0f, y - 26.0f, 52.0f, 52.0f, rgba(255, 140, 180, 60));
  }
  // Bile Blaster shots seen through the tube walls.
  for (const auto& s : mSpace.tubeShots)
  {
    const auto& t = mSpace.tubes[std::size_t(s.tube)];
    float px = 0.0f, py = 0.0f;
    pointAt(t.path[std::size_t(s.branch)], float(s.s) + float(kShotSpeed) * alpha, px, py);
    const float x = px * kCellPx - camX, y = py * kCellPx - camY;
    drawGlow(r, mArt, x, y, 40.0f, kBile, 0.8f);
    r.fillRect(x - 7.0f, y - 7.0f, 14.0f, 14.0f, rgba(230, 255, 140, 230));
  }
}

} // namespace gr
