// Level 39, The Gallery: monitors, panning cameras, the Echo Mic, Floor
// Managers, Cable Wranglers and Boom Ops; the Rewind bonus. See gallery.hpp.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace gr
{

namespace
{

int sgn(int v) { return (v > 0) - (v < 0); }

bool alive(const Player& p) { return p.state != PlayerState::Dying && p.state != PlayerState::Teleporting; }

} // namespace

// --- Setup ---------------------------------------------------------------------------------

bool World::setupGalleryEntity(const EntityDef& e)
{
  const bool rewind = mLevel->rules.find("rewind") != std::string::npos;
  if (mLevel->themeKey != "gallery" && !rewind)
    return false;
  auto& s = mGallery;
  s.on = true;
  s.rewind = rewind;
  if (e.kind == "duck")
    return true; // the duck's studio headphones are only drawn
  if (e.kind == "portal" && e.hasPos)
  {
    Monitor m;
    m.id = e.id;
    m.x = e.x;
    m.y = e.y;
    m.label = e.str("label");
    m.to = e.str("to");
    m.oneway = e.num("oneway", 0) != 0;
    m.virus = e.num("virus", 0) != 0;
    m.dead = m.to.empty();
    s.monitors.push_back(m);
    return true;
  }
  if (e.kind == "camspot")
  {
    CamSpot c;
    c.id = e.id;
    const auto pts = e.path("path");
    if (pts.empty())
      return true;
    c.x0 = pts.front().first * kCellsPerTile + 1;
    c.x1 = pts.back().first * kCellsPerTile + 1;
    c.row = pts.front().second * kCellsPerTile + 1;
    const std::string sp = e.str("speed", "1/2");
    c.n = std::max(1, std::atoi(sp.c_str()));
    const auto slash = sp.find('/');
    c.d = slash == std::string::npos ? 1 : std::max(1, std::atoi(sp.c_str() + slash + 1));
    s.cams.push_back(c);
    return true;
  }
  if (e.kind == "step" && e.hasPos)
  {
    GalleryStep st;
    st.x = e.x * kCellsPerTile;
    st.y = e.y * kCellsPerTile + 1;
    st.act = e.str("act", "go");
    st.arg = e.str("arg");
    st.lo = e.num("lo", 0) * kCellsPerTile;
    st.hi = e.num("hi", 0) * kCellsPerTile + 1;
    s.route.push_back(st);
    return true;
  }
  if (e.kind == "crusher" && e.hasPos && rewind)
  {
    GlCrusher c;
    c.x0 = e.x;
    c.w = e.num("w", 2);
    c.rest = e.y;
    c.to = e.num("to", e.y + 4);
    c.period = std::max(16, e.num("period", 30));
    c.tell = e.num("tell", 8);
    c.face = (c.rest + 1) * kCellsPerTile - 1;
    s.crushers.push_back(c);
    return true;
  }
  return false;
}

void World::setupGalleryEnemy(Enemy& en, const EntityDef& /*e*/)
{
  if (!mGallery.on)
    return;
  if (en.kind != EnemyKind::FloorManager && en.kind != EnemyKind::CableWrangler && en.kind != EnemyKind::BoomOp)
    return;
  GalleryActor a;
  a.enemy = int(&en - mEnemies.data());
  mGallery.actors.push_back(a);
}

void World::linkGallery()
{
  auto& s = mGallery;
  if (mLevel->themeKey == "gallery" || mLevel->rules.find("rewind") != std::string::npos)
  {
    s.on = true;
    s.rewind = mLevel->rules.find("rewind") != std::string::npos;
  }
  if (!s.on)
    return;
  for (auto& m : s.monitors)
  {
    if (m.to.empty())
      continue;
    for (std::size_t i = 0; i < s.monitors.size(); ++i)
      if (s.monitors[i].id == m.to)
        m.target = int(i);
    for (std::size_t i = 0; i < s.cams.size(); ++i)
      if (s.cams[i].id == m.to)
        m.cam = int(i);
    const auto comma = m.to.find(',');
    if (m.target < 0 && m.cam < 0 && comma != std::string::npos)
    {
      m.toX = std::atoi(m.to.c_str()) * kCellsPerTile + 1;
      m.toY = std::atoi(m.to.c_str() + comma + 1) * kCellsPerTile + 1;
    }
  }
  for (auto& a : s.actors)
  {
    const Enemy& e = mEnemies[std::size_t(a.enemy)];
    a.cableY = e.y;
    a.cableX0 = a.cableX1 = e.x + e.w / 2;
  }
}

// --- Monitors ------------------------------------------------------------------------------

int World::camX(const CamSpot& c) const
{
  const int len = std::abs(c.x1 - c.x0);
  if (len == 0)
    return c.x0;
  const int u = (mGallery.t * c.n / c.d) % (2 * len);
  return c.x0 + sgn(c.x1 - c.x0) * (u < len ? u : 2 * len - u);
}

bool World::monitorExit(const Monitor& m, int& x, int& y) const
{
  const auto& s = mGallery;
  if (m.target >= 0)
  {
    const Monitor& t = s.monitors[std::size_t(m.target)];
    x = t.x * kCellsPerTile + 2;
    y = t.feet();
    return true;
  }
  if (m.cam >= 0)
  {
    const CamSpot& c = s.cams[std::size_t(m.cam)];
    x = camX(c);
    y = c.row;
    // Snapped to floor on the side the camera is on: out of any wall.
    const int dir = sgn(c.x1 - c.x0) != 0 ? sgn(c.x1 - c.x0) : 1;
    for (int k = 0; k < 12; ++k)
      for (int side : {-dir, dir})
      {
        const int tx = x + side * k;
        if (!mMap.overlapsSolid({tx - 1, y - Player::kHeight + 1, Player::kWidth, Player::kHeight}) &&
            mMap.solidTop(tx, y + 1))
        {
          x = tx;
          return true;
        }
      }
    return true;
  }
  if (m.toX >= 0)
  {
    x = m.toX;
    y = m.toY;
    return true;
  }
  return false;
}

bool World::atMonitor(const Monitor& m) const
{
  const auto& p = mPlayer;
  const int mid = p.x + 1;
  return !m.dead && p.y >= m.feet() - 1 && p.y <= m.feet() && mid >= m.x * kCellsPerTile &&
    mid < m.x * kCellsPerTile + 4;
}

PlayerInput World::galleryInput(const PlayerInput& raw)
{
  auto& s = mGallery;
  auto& p = mPlayer;
  if (s.hop > 0 || s.trip > 0)
    return PlayerInput{};
  if (raw.up && p.state == PlayerState::OnGround && alive(p) && s.arrived < 0)
    for (std::size_t i = 0; i < s.monitors.size(); ++i)
      if (atMonitor(s.monitors[i]))
      {
        s.hop = kGlHop;
        s.hopTo = int(i);
        playSound(Sfx::EchoIn);
        return PlayerInput{};
      }
  // Up held from the last hop doesn't hop back.
  if (!raw.up)
    s.arrived = -1;
  return raw;
}

void World::finishHop()
{
  auto& s = mGallery;
  auto& p = mPlayer;
  const Monitor& m = s.monitors[std::size_t(s.hopTo)];
  int x = 0, y = 0;
  if (monitorExit(m, x, y))
  {
    p.x = x - 1;
    p.y = y;
    p.prevX = p.x;
    p.prevY = p.y;
    p.state = PlayerState::OnGround;
    mCamera.centerOn(cameraTarget(), mMap.width(), mMap.height());
    if (m.virus)
      infect();
  }
  s.arrived = s.hopTo;
  s.hopTo = -1;
}

// --- Shots ---------------------------------------------------------------------------------

bool World::shotAtGallery(Projectile& pr, const CellBox& b)
{
  auto& s = mGallery;
  // The Echo Mic: every shot comes again a second later from where it was fired.
  if (pr.age == 1 && pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::EchoMic) && !pr.echo)
    s.echoes.push_back({kGlEcho, pr.x, pr.y, pr.dx, pr.dy, pr.damage});
  if (pr.hopCool != 0)
  {
    // Just out of a monitor: it wakes what it hits in the room it shows.
    for (auto& e : mEnemies)
      if (e.alive && !e.active && e.box().intersects(b))
        e.active = true;
    return false;
  }
  // Into a monitor, out of the one it shows.
  for (const auto& m : s.monitors)
  {
    if (m.dead || !b.intersects(m.box()))
      continue;
    int x = 0, y = 0;
    if (!monitorExit(m, x, y))
      continue;
    const int up = m.feet() - pr.y; // as high over the floor as it went in
    pr.x = x + (pr.dx > 0 ? 3 : -4);
    pr.y = y - up;
    pr.prevX = pr.x;
    pr.prevY = pr.y;
    pr.hopCool = -30; // flies on off screen in the room it shows (World::updateProjectiles)
    if (pr.precise)
    {
      pr.fx = float(pr.x);
      pr.fy = float(pr.y);
    }
    return false;
  }
  return false;
}

// --- Enemies -------------------------------------------------------------------------------

GalleryActor* World::galleryActor(const Enemy& e)
{
  for (auto& a : mGallery.actors)
    if (a.enemy == int(&e - mEnemies.data()))
      return &a;
  return nullptr;
}

void World::galleryPatrol(Enemy& e, const EnemyDef& def)
{
  if (def.stepEvery <= 0 || mStats.frames % def.stepEvery != 0)
    return;
  const CellBox b = e.box();
  const int ahead = e.dir > 0 ? b.right() + 1 : b.x - 1;
  const bool wall = e.dir > 0 ? mMap.touchingRightWall(b) : mMap.touchingLeftWall(b);
  if (wall || !mMap.solidTop(ahead, b.bottom() + 1))
  {
    e.dir = -e.dir;
    if (GalleryActor* a = galleryActor(e))
      if (e.kind == EnemyKind::CableWrangler && !a->dropped)
      {
        a->whip = kGlWhip;
        playSound(Sfx::Whoosh);
      }
    return;
  }
  e.x += e.dir;
}

void World::updateCableWrangler(Enemy& e, const EnemyDef& def)
{
  GalleryActor* a = galleryActor(e);
  if (!a)
    return;
  auto& p = mPlayer;
  if (e.cool > 0)
    --e.cool;
  if (a->whip > 0)
    --a->whip;
  if (e.hp < def.hp)
    a->dropped = true; // shot: it lets go of the cable
  galleryPatrol(e, def);
  if (!a->dropped)
  {
    // The cable trails behind it at floor height, kGlCable cells long.
    const int tail = e.x + e.w / 2 - e.dir * kGlCable;
    a->cableX0 = std::min(tail, e.x + e.w / 2);
    a->cableX1 = std::max(tail, e.x + e.w / 2);
    a->cableY = e.y;
    // Trimmed where it would go through a wall.
    const int mid = e.x + e.w / 2;
    for (int x = mid; x >= a->cableX0; --x)
      if (mMap.solid(x, a->cableY))
      {
        a->cableX0 = x + 1;
        break;
      }
    for (int x = mid; x <= a->cableX1; ++x)
      if (mMap.solid(x, a->cableY))
      {
        a->cableX1 = x - 1;
        break;
      }
    const CellBox cable{a->cableX0, a->cableY - (a->whip > 0 ? 2 : 0), a->cableX1 - a->cableX0 + 1, a->whip > 0 ? 3 : 1};
    if (alive(p) && e.cool == 0 && p.mercy == 0 && mGallery.trip == 0 && cable.intersects(p.hitBox()))
    {
      hurtPlayer(1);
      mGallery.trip = kGlTrip;
      e.cool = def.cooldown;
      playSound(Sfx::Thud);
    }
  }
  if (alive(p) && e.cool == 0 && p.mercy == 0 && e.box().intersects(p.hitBox()))
  {
    hurtPlayer(1);
    e.cool = def.cooldown;
  }
}

CellBox World::boomArc(const Enemy& e) const
{
  // The mic passes 6-8 cells over the floor, over a standing runner's head
  // and into a jumping one; the arc is 5 blocks either side.
  return {e.x + e.w / 2 - 10, e.y - 8, 20, 3};
}

void World::updateBoomOp(Enemy& e, const EnemyDef& def)
{
  GalleryActor* a = galleryActor(e);
  if (!a)
    return;
  auto& p = mPlayer;
  const int dx = (p.x + 1) - (e.x + e.w / 2);
  if (dx != 0)
    e.dir = sgn(dx);
  if (e.cool > 0)
    --e.cool;
  // A pass every cooldown frames while the runner is near: the whoosh and
  // the mic's shadow (tell), then the sweep.
  if (a->pass == 0 && e.cool == 0 && alive(p) && std::abs(dx) < 24 && std::abs(p.y - e.y) < 8)
  {
    a->pass = 1;
    playSound(Sfx::Whoosh);
  }
  if (a->pass > 0)
  {
    ++a->pass;
    e.tell = a->pass <= kGlBoomPass ? kGlBoomPass - a->pass + 1 : 0;
    if (a->pass > kGlBoomPass && alive(p) && p.mercy == 0 && boomArc(e).intersects(p.hitBox()))
      hurtPlayer(1);
    if (a->pass > kGlBoomPass + kGlBoomSweep)
    {
      a->pass = 0;
      e.cool = def.cooldown;
    }
  }
  if (alive(p) && p.mercy == 0 && e.box().intersects(p.hitBox()))
    hurtPlayer(1);
}

bool World::lineOfSight(int x0, int y0, int x1, int y1) const
{
  const int n = std::max(std::abs(x1 - x0), std::abs(y1 - y0));
  for (int i = 1; i < n; ++i)
    if (mMap.solid(x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n))
      return false;
  return true;
}

void World::updateFloorManager(Enemy& e, const EnemyDef& def)
{
  GalleryActor* a = galleryActor(e);
  if (!a)
    return;
  auto& p = mPlayer;
  const int dx = (p.x + 1) - (e.x + e.w / 2);
  if (e.cool > 0)
    --e.cool;
  const bool sees = alive(p) && std::abs(dx) <= kGlManagerSight && std::abs(p.y - e.y) < 8 &&
    lineOfSight(e.x + e.w / 2, e.y - 4, p.x + 1, p.y - 3) && mGallery.hop == 0;
  if (a->count > 0)
  {
    if (!sees)
    {
      a->count = 0; // out of sight: the count is off
      e.tell = 0;
      return;
    }
    e.dir = sgn(dx) != 0 ? sgn(dx) : e.dir;
    const int total = p.turbo > 0 ? kGlCountTurbo : kGlCount;
    ++a->count;
    e.tell = total - a->count + 1;
    const int point = total * 7 / 22;
    if (a->count > point && (a->count - point) % std::max(1, (total - point) / 3) == 1)
      playSound(Sfx::Beep);
    if (a->count >= total)
    {
      // "One": every awake enemy in his room fires at the runner.
      a->count = 0;
      e.tell = 0;
      e.cool = def.cooldown;
      for (auto& o : mEnemies)
      {
        if (!o.alive || !o.active || std::abs(o.y - e.y) > 8 || !lineOfSight(o.x + o.w / 2, o.y - 3, e.x + e.w / 2, e.y - 3))
          continue;
        const int d = sgn((p.x + 1) - (o.x + o.w / 2));
        spawnProjectile(ShotKind::Enemy, d > 0 ? o.x + o.w : o.x - 2, o.y - 3, d != 0 ? d : 1, 0);
      }
      playSound(Sfx::EnemyShot);
    }
    return;
  }
  galleryPatrol(e, def);
  if (sees && e.cool == 0)
  {
    a->count = 1;
    e.dir = sgn(dx) != 0 ? sgn(dx) : e.dir;
  }
  if (alive(p) && p.mercy == 0 && e.box().intersects(p.hitBox()))
    hurtPlayer(1);
}

// --- Every frame ---------------------------------------------------------------------------

void World::updateGallery()
{
  auto& s = mGallery;
  auto& p = mPlayer;
  ++s.t;
  if (s.trip > 0)
    --s.trip;
  if (s.hop > 0 && --s.hop == 0)
    finishHop();
  // Echo Mic repeats.
  for (std::size_t i = 0; i < s.echoes.size();)
  {
    auto& ec = s.echoes[i];
    if (--ec.t > 0)
    {
      ++i;
      continue;
    }
    spawnProjectile(ShotKind::Proto, ec.x, ec.y, ec.dx, ec.dy);
    Projectile& pr = mProjectiles.back();
    pr.proto = int(ProtoId::EchoMic);
    pr.damage = ec.damage;
    pr.echo = true;
    playSound(Sfx::EchoIn);
    s.echoes.erase(s.echoes.begin() + std::ptrdiff_t(i));
  }
  // The route.
  while (s.routeAt < int(s.route.size()) && galleryStepDone(s.routeAt))
    ++s.routeAt;
  // Rewind: crushers and the history.
  for (auto& c : s.crushers)
  {
    const int up = (c.rest + 1) * kCellsPerTile - 1, down = (c.to + 1) * kCellsPerTile - 1;
    c.t = (c.t + 1) % c.period;
    const int slam = c.period - 10; // the slam starts here, holds, rises
    if (c.t < slam - c.tell)
      c.face = up;
    else if (c.t < slam)
    {
      c.face = up - 1;
      if (c.t == slam - c.tell)
        playSound(Sfx::Creak);
    }
    else if (c.t < slam + 3)
    {
      c.face = std::min(down, c.face + (down - up + 2) / 3 + 1);
      if (c.face == down)
        playSound(Sfx::Stomp);
    }
    else if (c.t < slam + 6)
      c.face = down;
    else
      c.face = std::max(up, c.face - (down - up + 3) / 4);
    if (alive(p) && c.face > up && c.box().intersects(p.hitBox()) && p.y - Player::kHeight < c.face)
      killPlayer();
  }
  if (s.rewind && alive(p) && s.rewinding == 0)
  {
    mRewind.push_back({p, mEnemies, mItems, mPlatforms, s.crushers, mStats.gems, mStats.score});
    while (int(mRewind.size()) > kGlRewindKeep)
      mRewind.pop_front();
  }
}

bool World::galleryRewindDeath()
{
  auto& s = mGallery;
  if (!s.rewind || s.rewinding > 0 || mRewind.empty())
    return false;
  s.rewinding = kGlRewindPlay;
  ++s.rewinds;
  playSound(Sfx::Rewind);
  return true;
}

void World::finishRewind()
{
  // Back to the oldest moment kept with the runner on their feet.
  std::size_t at = 0;
  while (at + 1 < mRewind.size() && mRewind[at].player.state != PlayerState::OnGround)
    ++at;
  const auto& f = mRewind[at];
  mPlayer = f.player;
  mEnemies = f.enemies;
  mItems = f.items;
  mPlatforms = f.platforms;
  mGallery.crushers = f.crushers;
  mStats.gems = f.gems;
  mStats.score = f.score;
  mProjectiles.clear();
  mRewind.clear();
  mCamera.centerOn(cameraTarget(), mMap.width(), mMap.height());
}

bool World::galleryStepDone(int i) const
{
  const auto& s = mGallery;
  const auto& st = s.route[std::size_t(i)];
  const auto& p = mPlayer;
  if (st.act == "pan")
  {
    // Out on the camera's side: arg is the floor's row (blocks).
    const int row = std::atoi(st.arg.c_str()) * kCellsPerTile + 1;
    return std::abs(p.y - row) <= 2 && p.x + 1 >= st.lo - 3 && p.x + 1 <= st.hi + 3;
  }
  if (st.act == "hop")
  {
    // Out in front of the monitor it leads to: arg is that spot (x,y blocks).
    const auto comma = st.arg.find(',');
    if (comma == std::string::npos)
      return false;
    const int ax = std::atoi(st.arg.c_str()) * kCellsPerTile, ay = std::atoi(st.arg.c_str() + comma + 1) * kCellsPerTile + 1;
    return std::abs(p.x + 1 - ax) <= 4 && std::abs(p.y - ay) <= 2;
  }
  return std::abs(p.x + 1 - st.x) <= 4 && std::abs(p.y - st.y) <= 2 && p.state == PlayerState::OnGround;
}

void World::resetGallery()
{
  auto& s = mGallery;
  s.hop = 0;
  s.hopTo = -1;
  s.trip = 0;
  s.arrived = -1;
  s.echoes.clear();
  for (auto& a : s.actors)
    a.count = a.pass = 0;
}

// --- Savegames -----------------------------------------------------------------------------

bool World::galleryCanSave() const
{
  const auto& s = mGallery;
  return !s.rewind && s.hop == 0 && s.echoes.empty() && s.trip == 0;
}

std::vector<int> World::gallerySave() const
{
  const auto& s = mGallery;
  std::vector<int> v{s.t, s.routeAt};
  for (const auto& a : s.actors)
  {
    v.push_back(a.dropped ? 1 : 0);
    v.push_back(a.cableX0);
    v.push_back(a.cableX1);
  }
  return v;
}

bool World::validGallerySave(const std::vector<int>& v) const
{
  return v.size() == 2 + mGallery.actors.size() * 3;
}

void World::loadGallery(const std::vector<int>& v)
{
  auto& s = mGallery;
  s.t = v[0];
  s.routeAt = v[1];
  for (std::size_t i = 0; i < s.actors.size(); ++i)
  {
    s.actors[i].dropped = v[2 + i * 3] != 0;
    s.actors[i].cableX0 = v[3 + i * 3];
    s.actors[i].cableX1 = v[4 + i * 3];
  }
  resetGallery();
}

} // namespace gr
