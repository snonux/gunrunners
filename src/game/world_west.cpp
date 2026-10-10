// Level 22, Dry Gulch (SPEC 22): Fuses. A fuse is a chain of blocks drawn
// on the ground, walls and poles; a shot, a spark or a blast lights it where
// it touches and the spark burns both ways from there at walking speed,
// splitting where a fork leaves it, until it reaches its TNT barrel. A
// barrel hisses for 12 frames and blows: enemies and `by=explosion` blocks
// in its radius break, a runner in it loses 3 hearts, and every fuse and
// barrel in it catches (chains). One barrel tips a drawbridge over. The
// Six-Shooter fires six, then spins its cylinder; its bullets glance off
// `@ metal` once. Duelists draw on the church bell's second ring, Window
// Bandits pop up to fire, Tumble Mines roll with the gusts down the mine.
// High Noon (rules=onehit) is twenty duels in a row.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace gr
{

namespace
{

// The path's blocks in order (axis-aligned legs, corners once).
std::vector<FuseCell> pathCells(const std::vector<std::pair<int, int>>& pts)
{
  std::vector<FuseCell> out;
  for (std::size_t i = 0; i < pts.size(); ++i)
  {
    if (i == 0)
    {
      out.push_back({pts[0].first, pts[0].second, 0, 0});
      continue;
    }
    int x = pts[i - 1].first, y = pts[i - 1].second;
    const int tx = pts[i].first, ty = pts[i].second;
    while (x != tx || y != ty)
    {
      x += x < tx ? 1 : (x > tx ? -1 : 0);
      if (x == tx)
        y += y < ty ? 1 : (y > ty ? -1 : 0);
      out.push_back({x, y, 0, 0});
    }
  }
  return out;
}

void outDir(const std::string& out, int& dx, int& dy)
{
  dx = out == "r" ? 1 : (out == "l" ? -1 : 0);
  dy = out == "d" ? 1 : (out == "u" ? -1 : 0);
  if (dx == 0 && dy == 0)
    dy = 1;
}

uint32_t lcg(uint32_t& s)
{
  s = s * 1664525u + 1013904223u;
  return s >> 8;
}

} // namespace

bool WestState::anyBurning() const
{
  for (const auto& f : fuses)
    for (const auto& c : f.cells)
      if (c.state == 1)
        return true;
  for (const auto& b : barrels)
    if (b.t >= 0 && !b.blown)
      return true;
  return false;
}

// --- Setup ---------------------------------------------------------------------------------

bool World::setupWestEntity(const EntityDef& e)
{
  auto& w = mWest;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  const bool western = mLevel->themeKey == "western_gulch";
  if (e.kind == "fuse")
  {
    Fuse f;
    f.id = e.id;
    f.cells = pathCells(e.path("path"));
    f.toId = e.str("to");
    f.fromId = e.str("from");
    const float speed = e.real("speed", 1.0f);
    f.framesPerBlock = std::max(1, int(std::lround(2.0f / std::max(0.1f, speed))));
    f.hidden = e.num("hidden", 0) != 0;
    f.bot = e.num("bot", 0) != 0;
    f.standX = e.num("stand", -1);
    if (!f.cells.empty())
      w.fuses.push_back(f);
    w.on = true;
    return true;
  }
  if (e.kind == "barrel" && e.hasPos)
  {
    Barrel b;
    b.id = e.id;
    b.x = e.x;
    b.y = e.y;
    b.radius = e.num("radius", 3);
    b.damage = e.num("damage", 8);
    b.hurt = e.num("hurt", 3);
    b.flash = std::max(1, e.num("flash", 12));
    w.barrels.push_back(b);
    w.on = true;
    return true;
  }
  if (e.kind == "drawbridge")
  {
    Drawbridge d;
    d.id = e.id;
    const auto hinge = e.list("hinge");
    if (hinge.size() == 2)
    {
      d.hx = hinge[0];
      d.hy = hinge[1];
    }
    d.len = std::max(1, e.num("len", 6));
    d.dir = e.str("falls", "l") == "r" ? 1 : -1;
    d.barrelId = e.str("barrel");
    w.bridges.push_back(d);
    w.on = true;
    return true;
  }
  if (e.kind == "metal" && (e.hasPos || e.rect("rect", x0, y0, x1, y1)))
  {
    Metal m;
    if (e.hasPos)
    {
      x0 = x1 = e.x;
      y0 = y1 = e.y;
    }
    m.x0 = x0;
    m.y0 = y0;
    m.x1 = x1;
    m.y1 = y1;
    outDir(e.str("out", "d"), m.outDx, m.outDy);
    m.reveal = e.num("reveal", 0) != 0;
    w.metals.push_back(m);
    w.on = true;
    return true;
  }
  if (e.kind == "prize" && e.hasPos)
  {
    Prize p;
    p.x = e.x;
    p.y = e.y;
    p.kind = e.str("kind", "gold");
    p.score = e.num("score", 10000);
    w.prizes.push_back(p);
    w.on = true;
    return true;
  }
  if (e.kind == "vskin" && e.hasPos)
  {
    // Matched to the Virus on the same spot in linkWest.
    w.tonic = e.x * 1000 + e.y;
    w.tonicSkin = e.str("skin", "tonic_bottle");
    w.tonicBob = e.num("bob", 1) != 0;
    w.on = true;
    return true;
  }
  if (e.kind == "piano" && e.hasPos)
  {
    w.pianoX = e.x;
    w.pianoY = e.y;
    w.on = true;
    return true;
  }
  if (e.kind == "bell" && e.hasPos)
  {
    w.bellX = e.x;
    w.bellY = e.y;
    w.on = true;
    return true;
  }
  if (e.kind == "spawner" && e.str("kind") == "tumble_mine")
  {
    MineSpawner s;
    s.x = (e.hasPos ? e.x : e.num("x")) * kCellsPerTile;
    s.y = (e.hasPos ? e.y : e.num("y")) * kCellsPerTile + 1;
    s.every = std::max(1, e.num("every", 75));
    s.max = std::max(1, e.num("max", 3));
    w.spawners.push_back(s);
    w.on = true;
    return true;
  }
  if (e.kind == "highnoon")
  {
    auto& n = w.noon;
    n.on = true;
    n.markX = e.num("duelist", 40) * kCellsPerTile;
    n.markY = mLevel->startTy * kCellsPerTile + 1;
    n.runnerX = mLevel->startTx * kCellsPerTile;
    n.runnerY = n.markY;
    w.on = true;
    return true;
  }
  if (western && e.kind == "deco" && e.str("kind") != "42" && (e.hasPos || e.rect("rect", x0, y0, x1, y1)))
  {
    WestDeco d;
    d.kind = e.str("kind");
    d.text = e.str("text");
    if (e.hasPos)
    {
      x0 = x1 = e.x;
      y0 = y1 = e.y;
    }
    d.x0 = x0;
    d.y0 = y0;
    d.x1 = x1;
    d.y1 = y1;
    w.decos.push_back(d);
    w.on = true;
    return true;
  }
  if (western && e.kind == "deco" && e.str("kind") == "42" && e.hasPos)
  {
    w.posters.push_back({e.x, e.y, 0});
    w.on = true;
    return false; // the deco is still drawn as usual (and read for its text)
  }
  return false;
}

void World::setupWestEnemy(Enemy& en, const EntityDef& e)
{
  (void)e;
  if (en.kind == EnemyKind::Duelist)
  {
    mWest.duelists.push_back(int(&en - mEnemies.data()));
    en.attach = 0; // idle
    mWest.on = true;
  }
  if (en.kind == EnemyKind::WindowBandit)
  {
    en.hidden = true; // ducked below the sill
    en.timer = int(en.x) % kBanditDown;
    mWest.on = true;
  }
  if (en.kind == EnemyKind::TumbleMine)
    mWest.on = true;
}

void World::linkWest()
{
  auto& w = mWest;
  w.oneHit = mLevel->rules.find("onehit") != std::string::npos;
  if (!w.on && !w.oneHit)
    return;
  w.on = true;
  auto barrelIndex = [&](const std::string& id) {
    for (std::size_t i = 0; i < w.barrels.size(); ++i)
      if (w.barrels[i].id == id)
        return int(i);
    return -1;
  };
  for (auto& f : w.fuses)
    f.barrel = barrelIndex(f.toId);
  for (auto& d : w.bridges)
    d.barrel = barrelIndex(d.barrelId);
  if (w.tonic >= 0)
  {
    const int tx = w.tonic / 1000, ty = w.tonic % 1000;
    w.tonic = -1;
    for (std::size_t i = 0; i < mItems.size(); ++i)
    {
      auto& it = mItems[i];
      if (it.kind == ItemKind::Virus && it.x / kCellsPerTile == tx && (it.y - 1) / kCellsPerTile == ty)
      {
        w.tonic = int(i);
        it.variant = 3; // drawn as the skin (world_west_draw.cpp)
      }
    }
  }
  // The vault bell's bonus entrance waits until the bell is shot.
  for (const auto& m : w.metals)
    if (m.reveal)
      for (auto& pr : mProps)
        if (pr.kind == PropKind::BonusDoor)
          pr.dormant = true;
  if (w.noon.on)
  {
    // High Noon: the Six-Shooter in hand for good, the first duelist on
    // his way.
    if (mLevelProto >= 0)
    {
      mPlayer.weapon = Weapon::Proto;
      mPlayer.proto = mLevelProto;
      mPlayer.ammo = protoDef(mLevelProto).maxAmmo;
    }
    startDuel();
  }
  w.checkpoints = -1;
  snapWest();
}

// --- Fuses and barrels ---------------------------------------------------------------------

void World::lightFuseBlock(int bx, int by)
{
  bool caught = false;
  for (auto& f : mWest.fuses)
    for (auto& c : f.cells)
      if (c.x == bx && c.y == by && c.state == 0)
      {
        c.state = 1;
        c.t = int8_t(f.framesPerBlock);
        caught = true;
      }
  if (caught)
    playSound(Sfx::FuseLit);
}

void World::igniteBarrel(int i)
{
  Barrel& b = mWest.barrels[std::size_t(i)];
  if (b.blown || b.t >= 0)
    return;
  b.t = b.flash;
  playSound(Sfx::BarrelHiss);
}

bool World::lightWestAt(const CellBox& box)
{
  bool any = false;
  for (auto& f : mWest.fuses)
    for (auto& c : f.cells)
    {
      if (c.state != 0)
        continue;
      const CellBox cb{c.x * kCellsPerTile, c.y * kCellsPerTile, kCellsPerTile, kCellsPerTile};
      if (cb.intersects(box))
      {
        lightFuseBlock(c.x, c.y);
        any = true;
      }
    }
  for (std::size_t i = 0; i < mWest.barrels.size(); ++i)
  {
    const Barrel& b = mWest.barrels[i];
    const CellBox bb{b.x * kCellsPerTile, b.y * kCellsPerTile, kCellsPerTile, kCellsPerTile};
    if (!b.blown && b.t < 0 && bb.intersects(box))
    {
      igniteBarrel(int(i));
      any = true;
    }
  }
  return any;
}

void World::westBlast(int cx, int cy, int radius, int damage, int hurt)
{
  // radius: cells. Enemies (asleep off screen too), boxes, the bosses.
  explodeAt(cx, cy, radius, damage);
  const CellBox area{cx - radius, cy - radius, radius * 2 + 1, radius * 2 + 1};
  for (auto& e : mEnemies)
    if (e.alive && !e.active && e.box().intersects(area))
      damageEnemy(e, damage);
  // `by=explosion` blocks.
  for (const auto& b : mBreakables)
  {
    if (b.broken)
      continue;
    const CellBox ba{b.x0 * kCellsPerTile, b.y0 * kCellsPerTile, (b.x1 - b.x0 + 1) * kCellsPerTile,
      (b.y1 - b.y0 + 1) * kCellsPerTile};
    if (ba.intersects(area))
      hitBreakable(ba, damage, 1);
  }
  if (hurt > 0 && mPlayer.state != PlayerState::Dying && mPlayer.hitBox().intersects(area))
  {
    const int mercy = mPlayer.mercy;
    mPlayer.mercy = 0; // a blast is a blast
    hurtPlayer(hurt);
    if (mPlayer.state != PlayerState::Dying && mPlayer.mercy == 0)
      mPlayer.mercy = mercy;
  }
  // Fuses and barrels in the radius catch.
  for (auto& f : mWest.fuses)
    for (auto& c : f.cells)
    {
      if (c.state != 0)
        continue;
      const float dx = float(c.x * kCellsPerTile + 1 - cx), dy = float(c.y * kCellsPerTile + 1 - cy);
      if (dx * dx + dy * dy <= float((radius + 1) * (radius + 1)))
        lightFuseBlock(c.x, c.y);
    }
  for (std::size_t i = 0; i < mWest.barrels.size(); ++i)
  {
    const Barrel& b = mWest.barrels[i];
    const float dx = float(b.x * kCellsPerTile + 1 - cx), dy = float(b.y * kCellsPerTile + 1 - cy);
    if (!b.blown && dx * dx + dy * dy <= float((radius + 1) * (radius + 1)))
      igniteBarrel(int(i));
  }
}

void World::blowBarrel(int i)
{
  Barrel& b = mWest.barrels[std::size_t(i)];
  b.blown = true;
  b.t = -1;
  westBlast(b.x * kCellsPerTile + 1, b.y * kCellsPerTile + 1, b.radius * kCellsPerTile, b.damage, b.hurt);
  for (auto& d : mWest.bridges)
    if (d.barrel == i && !d.down && d.t < 0)
    {
      d.t = 0;
      playSound(Sfx::Creak);
    }
}

void World::updateFuses()
{
  auto& w = mWest;
  // Burn: each burning block burns out after framesPerBlock frames and
  // lights the blocks on either side (and any fuse sharing the block).
  std::vector<std::pair<int, int>> next;
  for (std::size_t fi = 0; fi < w.fuses.size(); ++fi)
  {
    auto& f = w.fuses[fi];
    for (std::size_t i = 0; i < f.cells.size(); ++i)
    {
      auto& c = f.cells[i];
      if (c.state != 1 || --c.t > 0)
        continue;
      c.state = 2;
      if (i > 0)
        next.push_back({f.cells[i - 1].x, f.cells[i - 1].y});
      if (i + 1 < f.cells.size())
        next.push_back({f.cells[i + 1].x, f.cells[i + 1].y});
      else if (f.barrel >= 0)
        igniteBarrel(f.barrel);
    }
  }
  for (const auto& p : next)
    for (auto& f : w.fuses)
      for (auto& c : f.cells)
        if (c.x == p.first && c.y == p.second && c.state == 0)
        {
          c.state = 1;
          c.t = int8_t(f.framesPerBlock);
        }
  for (std::size_t i = 0; i < w.barrels.size(); ++i)
  {
    Barrel& b = w.barrels[i];
    if (b.blown || b.t < 0)
      continue;
    if (--b.t <= 0)
      blowBarrel(int(i));
  }
  for (auto& d : w.bridges)
  {
    if (d.down || d.t < 0)
      continue;
    if (++d.t < kBridgeFall)
      continue;
    d.down = true;
    for (int k = 0; k < d.len; ++k)
      mMap.setBlock(d.hx, d.hy - k, Tile::Empty);
    for (int k = 1; k <= d.len; ++k)
      mMap.setBlock(d.hx + d.dir * k, d.hy + 1, Tile::Solid);
    mCamera.shake(8, 1.2f);
    playSound(Sfx::Crash);
  }
}

void World::snapWest()
{
  auto& w = mWest;
  w.snapFuses.clear();
  for (const auto& f : w.fuses)
  {
    std::vector<int8_t> s;
    for (const auto& c : f.cells)
      s.push_back(c.state == 1 ? 0 : c.state);
    w.snapFuses.push_back(s);
  }
  w.snapBarrels.clear();
  for (const auto& b : w.barrels)
    w.snapBarrels.push_back(b.blown ? 1 : 0);
}

// --- Enemies -------------------------------------------------------------------------------

void World::updateDuelist(Enemy& e, const EnemyDef& def)
{
  (void)def;
  const auto& p = mPlayer;
  e.dir = p.x + 1 < e.x + 1 ? -1 : 1;
  if (mWest.noon.on)
    return; // High Noon runs its duel itself (updateHighNoon)
  const bool inRange = std::abs((p.x + 1) - (e.x + 1)) <= kDuelRange * kCellsPerTile &&
    std::abs(p.y - e.y) <= 3 && p.state != PlayerState::Dying;
  const int gap = p.turbo > 0 ? kDuelGap * 2 : kDuelGap;
  // attach: 0 idle, 1 waiting for ring 1, 2 between the rings, 3 drawing.
  if (!inRange)
  {
    e.attach = 0;
    e.tell = 0;
    return;
  }
  if (e.attach == 0)
  {
    e.attach = 1;
    e.timer = 0;
    e.dive = 0; // ring 1 now
  }
  // (e.timer counts up in World::updateEnemies)
  if (e.attach == 1 && e.timer >= e.dive)
  {
    e.attach = 2;
    e.timer = 0;
    mWest.bellRing = 20;
    playSound(Sfx::Bell);
  }
  else if (e.attach == 2 && e.timer >= gap)
  {
    e.attach = 3;
    e.timer = 0;
    e.tell = kDuelDraw;
    mWest.bellRing = 20;
    playSound(Sfx::Bell);
    playSound(Sfx::DuelDraw);
  }
  else if (e.attach == 3)
  {
    if (e.tell > 0)
      --e.tell;
    if (e.timer >= kDuelDraw)
    {
      // Horizontal, at the runner's chest: jump it.
      spawnProjectile(ShotKind::Enemy, e.dir > 0 ? e.x + e.w : e.x - 2, e.y - 2, e.dir, 0);
      mProjectiles.back().speed = 2;
      playSound(Sfx::EnemyShot);
      e.attach = 1;
      e.timer = 0;
      e.dive = kDuelRearm;
    }
  }
}

void World::updateTumbleMine(Enemy& e, const EnemyDef& def)
{
  (void)def;
  // The wind it is in: its direction, and whether it is gusting.
  int dir = e.dir;
  bool gust = false;
  for (const auto& z : mZones)
    if (z.kind == ZoneKind::Wind && z.box.intersects(e.box()))
    {
      if (z.dx != 0)
        dir = z.dx;
      gust = z.period > 0 && (mStats.frames % z.period) < z.on;
    }
  e.dir = dir;
  // It rolls over gaps of two blocks or less: held up while there is
  // ground under it, or within 4 cells behind and ahead of it.
  auto groundAt = [&](int x) { return mMap.solidTop(x, e.y + 1); };
  bool under = false, behind = false, ahead = false;
  for (int x = e.x; x < e.x + e.w; ++x)
    under = under || groundAt(x);
  for (int k = 1; k <= 4; ++k)
  {
    behind = behind || groundAt(dir > 0 ? e.x - k : e.x + e.w - 1 + k);
    ahead = ahead || groundAt(dir > 0 ? e.x + e.w - 1 + k : e.x - k);
  }
  if (!under && !(behind && ahead))
  {
    if (!mMap.solid(e.x, e.y + 1) && !mMap.solid(e.x + e.w - 1, e.y + 1))
      ++e.y; // falls
  }
  else if (gust || mStats.frames % 2 == 0)
  {
    const int nx = e.x + dir;
    bool blocked = false;
    for (int y = e.y - e.h + 1; y <= e.y; ++y)
      blocked = blocked || mMap.solid(dir > 0 ? nx + e.w - 1 : nx, y);
    if (blocked)
    {
      killEnemy(e); // bursts on the wall, harmless
      return;
    }
    e.x = nx;
  }
  // Rattles and blinks red when you come within 4 blocks.
  const auto& p = mPlayer;
  const int dx = std::abs((p.x + 1) - (e.x + e.w / 2));
  if (dx <= 4 * kCellsPerTile && std::abs(p.y - e.y) <= 8)
  {
    if (e.tell == 0)
      playSound(Sfx::Rattle);
    e.tell = 8;
  }
  else if (e.tell > 0)
    --e.tell;
  // Contact: it blows up (its top three rows: a runner in a dip is safe).
  const CellBox top{e.x, e.y - e.h + 1, e.w, e.h - 1};
  if (p.state != PlayerState::Dying && top.intersects(p.hitBox()))
  {
    e.alive = false;
    westBlast(e.x + e.w / 2, e.y - e.h / 2, kCellsPerTile, 1, 1);
  }
}

void World::updateWindowBandit(Enemy& e, const EnemyDef& def)
{
  // Ducked kBanditDown frames, his hat showing for kBanditTell, then up for
  // kBanditUp: he fires as he comes up (e.timer counts up in
  // World::updateEnemies).
  const int cycle = kBanditDown + kBanditTell + kBanditUp;
  const int t = e.timer % cycle;
  e.hidden = t < kBanditDown + kBanditTell;
  e.tell = t >= kBanditDown && t < kBanditDown + kBanditTell ? kBanditDown + kBanditTell - t : 0;
  if (t == kBanditDown + kBanditTell + 2 && mPlayer.state != PlayerState::Dying)
  {
    const auto& p = mPlayer;
    if (std::abs((p.x + 1) - (e.x + 1)) <= 22 * kCellsPerTile)
      shootAt(e, e.x + 1, e.y - 2, 1, def.range > 0 ? def.range : -1);
  }
}

// --- Shots ---------------------------------------------------------------------------------

int World::shotAtWest(Projectile& pr, const CellBox& b)
{
  auto& w = mWest;
  // A Six-Shooter bullet glances off metal once.
  const bool six = pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::SixShooter);
  for (auto& m : w.metals)
  {
    const CellBox mb{m.x0 * kCellsPerTile, m.y0 * kCellsPerTile, (m.x1 - m.x0 + 1) * kCellsPerTile,
      (m.y1 - m.y0 + 1) * kCellsPerTile};
    if (!mb.intersects(b))
      continue;
    if (m.ring == 0)
      playSound(Sfx::Bell);
    m.ring = 30;
    if (m.reveal)
      for (auto& prop : mProps)
        if (prop.kind == PropKind::BonusDoor && prop.dormant)
        {
          prop.dormant = false;
          showMessage("THE VAULT BELL RINGS - A DOOR OPENS");
          playSound(Sfx::LettersComplete);
        }
    if (!six || pr.ricochet)
      return mMap.overlapsSolid(b) ? 1 : 0;
    pr.ricochet = true;
    const bool vertical = m.outDy != 0;
    const int len = std::max(pr.w, pr.h), thick = std::min(pr.w, pr.h);
    pr.w = vertical ? thick : len;
    pr.h = vertical ? len : thick;
    pr.dx = m.outDx;
    pr.dy = m.outDy;
    if (m.outDx > 0)
      pr.x = mb.right() + 1;
    else if (m.outDx < 0)
      pr.x = mb.left() - pr.w;
    if (m.outDy > 0)
      pr.y = mb.bottom() + 1;
    else if (m.outDy < 0)
      pr.y = mb.top() - pr.h;
    if (vertical)
      pr.x = std::clamp(b.x, mb.left(), mb.right());
    else
      pr.y = std::clamp(b.y, mb.top(), mb.bottom());
    pr.prevX = pr.x;
    pr.prevY = pr.y;
    burst(cellCenter(b), rgb(255, 255, 220), rgb(255, 200, 80), 6, 1.4f);
    playSound(Sfx::Ricochet);
    return 2;
  }
  // A barrel takes the bullet and lights.
  for (std::size_t i = 0; i < w.barrels.size(); ++i)
  {
    const Barrel& br = w.barrels[i];
    const CellBox bb{br.x * kCellsPerTile, br.y * kCellsPerTile, kCellsPerTile, kCellsPerTile};
    if (!br.blown && bb.intersects(b))
    {
      igniteBarrel(int(i));
      return 1;
    }
  }
  // A fuse catches where the shot crosses it (the shot goes on).
  for (auto& f : w.fuses)
    for (auto& c : f.cells)
    {
      if (c.state != 0)
        continue;
      const CellBox cb{c.x * kCellsPerTile, c.y * kCellsPerTile, kCellsPerTile, kCellsPerTile};
      if (cb.intersects(b))
        lightFuseBlock(c.x, c.y);
    }
  // A wanted poster spins on its nail.
  for (auto& po : w.posters)
  {
    const CellBox pb{po.x * kCellsPerTile, po.y * kCellsPerTile, 2 * kCellsPerTile, 2 * kCellsPerTile};
    if (pb.intersects(b) && po.spin == 0)
    {
      po.spin = 40;
      playSound(Sfx::PosterSpin);
    }
  }
  return 0;
}

bool World::sixShooterFire()
{
  auto& w = mWest;
  w.on = true; // the reload runs in updateWest (the Six-Shooter may turn up anywhere)
  if (w.reload > 0)
    return false;
  if (--w.cylinder <= 0)
  {
    w.cylinder = 0;
    w.reload = mPlayer.turbo > 0 ? kSixReloadTurbo : kSixReload;
    playSound(Sfx::Reload);
  }
  return true;
}

// --- High Noon -----------------------------------------------------------------------------

void World::startDuel()
{
  auto& n = mWest.noon;
  // The next duelist walks in from the right edge.
  if (n.duelist < 0 || !mEnemies[std::size_t(n.duelist)].alive)
  {
    const int def = enemyIndex("duelist");
    if (def < 0)
      return;
    spawnEnemy(def, (mLevel->width - 2) * kCellsPerTile, n.markY);
    n.duelist = int(mEnemies.size()) - 1;
    if (n.duel > 1)
      ++mStats.enemiesTotal; // the tally counts all twenty
    Enemy& e = mEnemies.back();
    e.active = true;
    e.dir = -1;
  }
  n.phase = DuelPhase::WalkIn;
  n.t = 0;
  // The Six-Shooter never runs dry here.
  if (mPlayer.weapon == Weapon::Proto)
    mPlayer.ammo = protoDef(mPlayer.proto).maxAmmo;
  n.gap = n.duel <= 10 ? kDuelGap : 15 + int(lcg(n.seed) % 16);
}

void World::restartDuel(const char* why)
{
  auto& n = mWest.noon;
  // Both back on their marks; the bell starts over.
  for (auto& pr : mProjectiles)
    pr.alive = false;
  auto& p = mPlayer;
  p.x = p.prevX = n.runnerX;
  p.y = p.prevY = n.runnerY;
  p.facing = 1;
  if (n.duelist >= 0)
  {
    Enemy& e = mEnemies[std::size_t(n.duelist)];
    e.x = e.prevX = n.markX;
    e.y = e.prevY = n.markY;
    e.tell = 0;
  }
  n.phase = DuelPhase::Ring1;
  n.t = 0;
  n.text = why;
  n.message = 45;
}

void World::updateHighNoon()
{
  auto& n = mWest.noon;
  static const bool debug = std::getenv("GR_WEST_DEBUG") != nullptr;
  static int lastPhase = -1;
  if (debug && !mSimulation && int(n.phase) != lastPhase)
  {
    lastPhase = int(n.phase);
    std::fprintf(stderr, "noon f%d duel %d phase %d text %s\n", mStats.frames, n.duel, lastPhase, n.text.c_str());
  }
  if (n.message > 0)
    --n.message;
  if (n.phase == DuelPhase::Done)
    return;
  ++n.t;
  Enemy* e = n.duelist >= 0 ? &mEnemies[std::size_t(n.duelist)] : nullptr;
  if (e && !e->alive && n.phase != DuelPhase::Won && n.phase != DuelPhase::WalkIn)
  {
    if (n.phase == DuelPhase::Ring1 || n.phase == DuelPhase::Gap)
    {
      // Shot before the second ring: a false start. He gets up again.
      e->alive = true;
      e->hp = enemyDef(e->def).hp;
      if (mStats.kills > 0)
        --mStats.kills;
      ++n.falseStarts;
      restartDuel("FALSE START");
      return;
    }
    for (auto& pr : mProjectiles)
      if (pr.kind == ShotKind::Enemy)
        pr.alive = false; // his shot dies with him
    dropGems(e->x + 1, e->y - 2, 1);
    n.phase = DuelPhase::Won;
    n.t = 0;
    return;
  }
  // A shot before the second ring is a false start too, even a miss.
  if ((n.phase == DuelPhase::Ring1 || n.phase == DuelPhase::Gap || n.phase == DuelPhase::WalkIn) && mWest.fired)
  {
    ++n.falseStarts;
    restartDuel("FALSE START");
    return;
  }
  switch (n.phase)
  {
    case DuelPhase::WalkIn:
      if (e)
      {
        const int from = (mLevel->width - 2) * kCellsPerTile;
        e->x = e->prevX = from + (n.markX - from) * std::min(n.t, kHighNoonWalk) / kHighNoonWalk;
        e->y = n.markY;
        e->dir = -1;
      }
      if (n.t >= kHighNoonWalk)
      {
        n.phase = DuelPhase::Ring1;
        n.t = 0;
      }
      break;
    case DuelPhase::Ring1:
      if (n.t >= 1)
      {
        mWest.bellRing = 20;
        playSound(Sfx::Bell);
        n.phase = DuelPhase::Gap;
        n.t = 0;
      }
      break;
    case DuelPhase::Gap:
      if (n.t >= n.gap)
      {
        mWest.bellRing = 20;
        playSound(Sfx::Bell);
        playSound(Sfx::DuelDraw);
        n.phase = DuelPhase::Draw;
        n.t = 0;
        if (e)
          e->tell = kDuelDraw;
        n.text = "DRAW!";
        n.message = 20;
      }
      break;
    case DuelPhase::Draw:
    case DuelPhase::Fired:
      if (e && e->tell > 0)
        --e->tell;
      if (e && ((n.phase == DuelPhase::Draw && n.t >= kDuelDraw) || (n.phase == DuelPhase::Fired && n.t >= 30)))
      {
        spawnProjectile(ShotKind::Enemy, e->x - 2, e->y - 2, -1, 0);
        mProjectiles.back().speed = 2;
        playSound(Sfx::EnemyShot);
        n.phase = DuelPhase::Fired;
        n.t = 0;
      }
      break;
    case DuelPhase::Won:
      if (n.t >= 2)
      {
        if (n.duel >= kHighNoonDuels)
        {
          n.phase = DuelPhase::Done;
          n.text = "TWENTY DUELS - THE STREET IS YOURS";
          n.message = 90;
          playSound(Sfx::LettersComplete);
          break;
        }
        ++n.duel;
        startDuel();
      }
      break;
    case DuelPhase::Done:
      break;
  }
}

bool World::westRunnerHit()
{
  // High Noon: a hit restarts the duel, nothing else.
  if (!mWest.noon.on || mWest.noon.phase == DuelPhase::Done)
    return false;
  restartDuel("SHOT - AGAIN");
  return true;
}

// --- Update --------------------------------------------------------------------------------

void World::updateWest(const PlayerInput& input)
{
  (void)input;
  auto& w = mWest;
  auto& p = mPlayer;
  if (w.reload > 0 && --w.reload == 0)
    w.cylinder = kSixCylinder;
  if (w.bellRing > 0)
    --w.bellRing;
  if (w.quickDraw > 0)
    --w.quickDraw;
  if (w.tune > 0)
    --w.tune;
  if (w.pianoFlash > 0)
    --w.pianoFlash;
  for (auto& m : w.metals)
    if (m.ring > 0)
      --m.ring;
  for (auto& po : w.posters)
    if (po.spin > 0)
      --po.spin;

  // A new checkpoint: what the fuses and barrels go back to.
  int active = 0;
  for (const auto& cp : mCheckpoints)
    active += cp.active ? 1 : 0;
  if (active != w.checkpoints)
  {
    w.checkpoints = active;
    snapWest();
  }

  updateFuses();

  // Duelists: one killed under Turbo before the second ring is a quick draw.
  for (int i : w.duelists)
  {
    Enemy& e = mEnemies[std::size_t(i)];
    if (!e.alive && e.attach > 0 && e.attach < 3)
    {
      if (p.turbo > 0)
      {
        w.quickDraw = 60;
        addScore(500, cellCenter(e.box()));
        showMessage("QUICK DRAW!");
      }
      e.attach = 0;
    }
  }

  // Tumble Mine spawners: one every `every` frames while the runner is near.
  const int mineDef = enemyIndex("tumble_mine");
  for (auto& s : w.spawners)
  {
    s.mines.erase(std::remove_if(s.mines.begin(), s.mines.end(),
                    [&](int i) { return !mEnemies[std::size_t(i)].alive; }),
      s.mines.end());
    if (mineDef < 0 || std::abs(p.x - s.x) > 48 * kCellsPerTile)
      continue;
    if (++s.t >= s.every && int(s.mines.size()) < s.max)
    {
      s.t = 0;
      spawnEnemy(mineDef, s.x, s.y);
      s.mines.push_back(int(mEnemies.size()) - 1);
    }
  }

  // Prizes.
  const CellBox pbox = p.box();
  for (auto& pz : w.prizes)
  {
    const CellBox zb{pz.x * kCellsPerTile, pz.y * kCellsPerTile, kCellsPerTile, kCellsPerTile};
    if (pz.taken || p.state == PlayerState::Dying || !zb.intersects(pbox))
      continue;
    pz.taken = true;
    addScore(pz.score, cellCenter(zb));
    playSound(Sfx::LettersComplete);
    showMessage(pz.kind == "gold_revolver" ? "THE GOLD-PLATED REVOLVER - SECRET FOUND" : "PRIZE");
  }

  // The tonic's label.
  w.tonicNear = false;
  if (w.tonic >= 0 && std::size_t(w.tonic) < mItems.size() && !mItems[std::size_t(w.tonic)].taken)
  {
    const Item& it = mItems[std::size_t(w.tonic)];
    w.tonicNear = std::abs(p.x + 1 - it.x) <= 2 * kCellsPerTile + 1 && std::abs(p.y - it.y) <= 2 * kCellsPerTile + 2;
  }

  // The piano: a shot fired with the muzzle in front of a key plays it.
  if (w.pianoT > 0 && --w.pianoT == 0)
    w.pianoStep = 0;
  if (w.pianoX >= 0 && w.fired)
    do
    {
      const int kx = w.firedX / kCellsPerTile - w.pianoX, ky = w.firedY / kCellsPerTile;
      if (kx < 0 || kx > 3 || ky < w.pianoY - 1 || ky > w.pianoY)
        break;
      w.pianoKey = kx;
      w.pianoFlash = 10;
      playSound(Sfx(int(Sfx::Piano1) + kx));
      if (kx == w.pianoStep)
      {
        ++w.pianoStep;
        w.pianoT = 60;
        if (w.pianoStep == 4)
        {
          w.pianoStep = 0;
          w.tune = 90;
          playSound(Sfx::HonkyTonk);
          showMessage("THE PIANO PLAYS ITSELF A BAR");
        }
      }
      else
      {
        w.pianoStep = kx == 0 ? 1 : 0;
        w.pianoT = 60;
      }
    } while (false);

  if (w.noon.on)
    updateHighNoon();
  w.fired = false;
}

void World::resetWest()
{
  auto& w = mWest;
  // Fuses and barrels as they were at the checkpoint (barrels that blew
  // since stay blown: what they broke is gone).
  for (std::size_t fi = 0; fi < w.fuses.size() && fi < w.snapFuses.size(); ++fi)
  {
    auto& f = w.fuses[fi];
    if (f.barrel >= 0 && w.barrels[std::size_t(f.barrel)].blown)
    {
      for (auto& c : f.cells)
        if (c.state == 1)
          c.state = 2;
      continue;
    }
    for (std::size_t i = 0; i < f.cells.size() && i < w.snapFuses[fi].size(); ++i)
    {
      f.cells[i].state = w.snapFuses[fi][i];
      f.cells[i].t = 0;
    }
  }
  for (auto& b : w.barrels)
    if (!b.blown)
      b.t = -1;
  w.reload = 0;
  w.cylinder = kSixCylinder;
  for (int i : w.duelists)
  {
    mEnemies[std::size_t(i)].attach = 0;
    mEnemies[std::size_t(i)].tell = 0;
  }
}

// --- Saves ---------------------------------------------------------------------------------

bool World::westCanSave() const
{
  return !mWest.anyBurning() && !mWest.noon.on;
}

std::vector<int> World::westSave() const
{
  // [fuse cells burnt (one per cell, in order)..., barrels blown..., bridges
  // down..., prizes taken..., bonus revealed]
  const auto& w = mWest;
  std::vector<int> s;
  for (const auto& f : w.fuses)
    for (const auto& c : f.cells)
      s.push_back(c.state == 2 ? 1 : 0);
  for (const auto& b : w.barrels)
    s.push_back(b.blown ? 1 : 0);
  for (const auto& d : w.bridges)
    s.push_back(d.down ? 1 : 0);
  for (const auto& p : w.prizes)
    s.push_back(p.taken ? 1 : 0);
  bool revealed = false;
  for (const auto& m : w.metals)
    if (m.reveal)
      for (const auto& pr : mProps)
        revealed = revealed || (pr.kind == PropKind::BonusDoor && !pr.dormant);
  s.push_back(revealed ? 1 : 0);
  return s;
}

bool World::validWestSave(const std::vector<int>& s) const
{
  std::size_t n = 1;
  for (const auto& f : mWest.fuses)
    n += f.cells.size();
  n += mWest.barrels.size() + mWest.bridges.size() + mWest.prizes.size();
  return s.size() == n;
}

void World::loadWest(const std::vector<int>& s)
{
  auto& w = mWest;
  std::size_t k = 0;
  for (auto& f : w.fuses)
    for (auto& c : f.cells)
    {
      c.state = s[k++] ? 2 : 0;
      c.t = 0;
    }
  for (auto& b : w.barrels)
  {
    b.blown = s[k++] != 0;
    b.t = -1;
  }
  for (auto& d : w.bridges)
  {
    const bool down = s[k++] != 0;
    if (down && !d.down)
    {
      d.down = true;
      d.t = kBridgeFall;
      for (int i = 0; i < d.len; ++i)
        mMap.setBlock(d.hx, d.hy - i, Tile::Empty);
      for (int i = 1; i <= d.len; ++i)
        mMap.setBlock(d.hx + d.dir * i, d.hy + 1, Tile::Solid);
    }
  }
  for (auto& p : w.prizes)
    p.taken = s[k++] != 0;
  if (s[k++])
    for (auto& pr : mProps)
      if (pr.kind == PropKind::BonusDoor)
        pr.dormant = false;
  w.checkpoints = -1;
}

} // namespace gr
