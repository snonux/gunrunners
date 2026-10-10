// Level 19, Gravity Lab (SPEC 19): Gravity Switches. Nine test chambers,
// each with its own down (`@ gravzone`); a wall switch turns a chamber over
// and everything in it falls the other way. A turned runner moves on a
// turned copy of the map (CollisionMap::makeView): the usual player code
// runs on it unchanged, and the runner, its shots and its grenades are
// turned back afterwards. The Grav Grenade hangs as a little vortex that
// pulls enemies in; Flip Walkers fall when their chamber turns, Gravity
// Probes pull the runner toward them, Test Subjects copy the runner's
// jumps. The Gun Gravity bonus (rules=gun_gravity) turns the runner toward
// wherever its last shot went.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace gr
{

namespace
{

constexpr int kSwitchRest = 10; // frames a switch rests after it is thrown
constexpr int kQuick = 15;      // two flips (or two hits) within this many frames
constexpr int kDizzy = 30;
constexpr int kVortexFlight = 12;
constexpr int kVortexLife = 45;
constexpr int kVortexReach = 12; // cells (6 blocks)
constexpr int kVortexBlast = 4;  // cells (2 blocks)
constexpr int kProbeKeep = 12;   // cells a Gravity Probe keeps from the runner
constexpr int kBigFall = 20;     // cells: a Flip Walker's fall that kills it (10 blocks)
constexpr int kFall = 10;        // cells: a fall that hurts it (5 blocks)

int sgn(int v) { return (v > 0) - (v < 0); }

Grav parseGrav(const std::string& s)
{
  return s == "up" ? Grav::Up : (s == "left" ? Grav::Left : (s == "right" ? Grav::Right : Grav::Down));
}

std::vector<std::string> names(const std::string& s)
{
  std::vector<std::string> out;
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, ','))
    if (!item.empty())
      out.push_back(item);
  return out;
}

// Boxes between the map and a turned runner's view of it (g: 1 up, 2 left,
// 3 right; see CollisionMap::makeView). Wc, Hc: the map in cells.
CellBox toView(int g, const CellBox& b, int Wc, int Hc)
{
  if (g == 1)
    return {b.x, Hc - b.y - b.h, b.w, b.h};
  if (g == 2)
    return {b.y, Wc - b.x - b.w, b.h, b.w};
  return {Hc - b.y - b.h, b.x, b.h, b.w};
}

CellBox fromView(int g, const CellBox& v, int Wc, int Hc)
{
  if (g == 1)
    return {v.x, Hc - v.y - v.h, v.w, v.h};
  if (g == 2)
    return {Wc - v.y - v.h, v.x, v.h, v.w};
  return {v.y, Hc - v.x - v.w, v.h, v.w};
}

// A direction seen by the turned runner, in the map.
void dirFromView(int g, int vdx, int vdy, int& dx, int& dy)
{
  if (g == 1)
  {
    dx = vdx;
    dy = -vdy;
  }
  else if (g == 2)
  {
    dx = -vdy;
    dy = vdx;
  }
  else
  {
    dx = vdy;
    dy = -vdx;
  }
}

void dirFromView(int g, float vdx, float vdy, float& dx, float& dy)
{
  if (g == 1)
  {
    dx = vdx;
    dy = -vdy;
  }
  else if (g == 2)
  {
    dx = -vdy;
    dy = vdx;
  }
  else
  {
    dx = vdy;
    dy = -vdx;
  }
}

// A point (cells, continuous) seen by the turned runner, in the map.
void pointFromView(int g, float vx, float vy, int Wc, int Hc, float& x, float& y)
{
  if (g == 1)
  {
    x = vx;
    y = float(Hc) - vy;
  }
  else if (g == 2)
  {
    x = float(Wc) - vy;
    y = vx;
  }
  else
  {
    x = vy;
    y = float(Hc) - vx;
  }
}

// Where the runner's x, y sit in its box for each down (Player::box()).
void placeRunner(Player& p, const CellBox& b)
{
  switch (p.grav)
  {
    case Grav::Up:
    case Grav::Left:
      p.x = b.x;
      p.y = b.y;
      break;
    case Grav::Right:
      p.x = b.x + b.w - 1;
      p.y = b.y;
      break;
    default:
      p.x = b.x;
      p.y = b.y + b.h - 1;
      break;
  }
}

// One cell of fall toward this down, per frame of falling (as the runner).
int fallStep(int frames) { return frames < 2 ? 1 : 2; }

CellBox switchBox(const GravSwitch& s) { return {s.bx * kCellsPerTile, s.by * kCellsPerTile, 2, 2}; }

} // namespace

// --- Setup ---------------------------------------------------------------------------------

bool World::setupGravEntity(const EntityDef& e)
{
  auto& g = mGrav;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  if (e.kind == "gravzone" && e.rect("rect", x0, y0, x1, y1))
  {
    GravZone z;
    z.id = e.id;
    z.x0 = x0;
    z.y0 = y0;
    z.x1 = x1;
    z.y1 = y1;
    z.dir = parseGrav(e.str("dir"));
    z.flip = 1000;
    g.zones.push_back(z);
    g.zoneSw.push_back(e.str("switch"));
    g.on = true;
    return true;
  }
  if (e.kind == "switch" && e.hasPos && e.str("kind") == "panel")
  {
    GravSwitch s;
    s.id = e.id;
    s.bx = e.x;
    s.by = e.y;
    s.flash = 1000;
    g.switches.push_back(s);
    g.on = true;
    return true;
  }
  if (e.kind == "testgate" && e.hasPos)
  {
    TestGate t;
    t.id = e.id;
    t.bx = e.x;
    t.by = e.y;
    t.h = std::max(1, e.num("h", 3));
    g.gates.push_back(t);
    g.gateOpens.push_back(names(e.str("opens")));
    g.gateSw.push_back(e.str("hits"));
    g.on = true;
    return true;
  }
  if (e.kind == "fakewall" && e.hasPos)
  {
    g.fakes.push_back({e.x, e.y, std::max(1, e.num("h", 1))});
    g.on = true;
    return true;
  }
  if (e.kind == "deco" && e.str("kind") == "arrow" && e.hasPos)
  {
    g.arrows.push_back({e.x, e.y, -1});
    g.on = true;
    return true;
  }
  if (e.kind == "deco" && mLevel->themeKey == "station_gravlab")
  {
    const std::string k = e.str("kind");
    if (k != "mug" && k != "desk" && k != "chain" && k != "window" && k != "42")
      return false;
    GravDeco d;
    d.kind = k;
    if (!e.rect("rect", d.x0, d.y0, d.x1, d.y1))
    {
      if (!e.hasPos)
        return false;
      d.x0 = d.x1 = e.x;
      d.y0 = d.y1 = e.y;
    }
    g.decos.push_back(d);
    return true;
  }
  return false;
}

void World::setupGravEnemy(Enemy& en, const EntityDef& e)
{
  if (!e.id.empty())
    mGrav.names.emplace_back(e.id, int(mEnemies.size()) - 1);
  if (en.kind == EnemyKind::FlipWalker || en.kind == EnemyKind::Probe || en.kind == EnemyKind::TestSubject)
    mGrav.on = true;
}

int World::gravZoneAt(int cx, int cy) const
{
  const int bx = cx >= 0 ? cx / kCellsPerTile : -1, by = cy >= 0 ? cy / kCellsPerTile : -1;
  int best = -1, area = 0;
  for (std::size_t i = 0; i < mGrav.zones.size(); ++i)
  {
    const auto& z = mGrav.zones[i];
    if (bx < z.x0 || bx > z.x1 || by < z.y0 || by > z.y1)
      continue;
    // A zone inside another (the closet in A1) wins.
    const int a = (z.x1 - z.x0 + 1) * (z.y1 - z.y0 + 1);
    if (best < 0 || a < area)
    {
      best = int(i);
      area = a;
    }
  }
  return best;
}

void World::linkGrav()
{
  auto& g = mGrav;
  g.gun = mLevel->rules.find("gun_gravity") != std::string::npos;
  g.on = g.on || g.gun || mLevel->weapon == "grav_grenade";
  if (!g.on)
  {
    g.names.clear();
    return;
  }
  auto switchIndex = [&](const std::string& id) {
    for (std::size_t i = 0; i < g.switches.size(); ++i)
      if (!id.empty() && g.switches[i].id == id)
        return int(i);
    return -1;
  };
  for (std::size_t i = 0; i < g.zones.size() && i < g.zoneSw.size(); ++i)
  {
    g.zones[i].sw = switchIndex(g.zoneSw[i]);
    if (g.zones[i].sw >= 0)
      g.switches[std::size_t(g.zones[i].sw)].zone = int(i);
  }
  // A switch nobody named works the chamber it sits in.
  for (auto& s : g.switches)
    if (s.zone < 0)
      s.zone = gravZoneAt(s.bx * kCellsPerTile, s.by * kCellsPerTile);
  for (auto& a : g.arrows)
    a.zone = gravZoneAt(a.bx * kCellsPerTile, a.by * kCellsPerTile);
  for (std::size_t i = 0; i < g.gates.size(); ++i)
  {
    auto& t = g.gates[i];
    if (i < g.gateSw.size())
      t.sw = switchIndex(g.gateSw[i]);
    if (i < g.gateOpens.size())
      for (const auto& n : g.gateOpens[i])
        for (const auto& [id, en] : g.names)
          if (id == n)
            t.enemies.push_back(en);
    for (int k = 0; k < t.h; ++k)
      mMap.setBlock(t.bx, t.by + k, Tile::Solid);
  }
  for (const auto& f : g.fakes)
    for (int k = 0; k < f.h; ++k)
      mMap.setBlock(f.bx, f.by + k, Tile::Empty);
  g.zoneSw.clear();
  g.gateSw.clear();
  g.gateOpens.clear();
  g.names.clear();
  resetGrav();
}

void World::resetGrav()
{
  auto& g = mGrav;
  auto& p = mPlayer;
  ++g.respawns;
  g.vortices.clear();
  g.dizzy = 0;
  g.turned = g.jumped = false;
  p.grav = Grav::Down;
  if (g.gun)
    return;
  // A checkpoint on a ceiling: the runner comes back hanging from it.
  const int z = gravZoneAt(p.x + 1, p.y);
  const int top = (p.y / kCellsPerTile) * kCellsPerTile;
  if (z >= 0 && g.zones[std::size_t(z)].dir == Grav::Up && mMap.solid(p.x + 1, top - 1))
  {
    p.grav = Grav::Up;
    p.y = p.prevY = top;
  }
}

bool World::gravCanSave() const
{
  // Not mid-fall after a flip, nor with a vortex out.
  return mGrav.vortices.empty() && !mGrav.turned;
}

// --- The runner ----------------------------------------------------------------------------

void World::turnRunner(Grav g)
{
  auto& p = mPlayer;
  if (p.grav == g || p.state == PlayerState::Dying || p.state == PlayerState::Teleporting)
    return;
  const CellBox old = p.box();
  const bool upright = p.grav == Grav::Down || p.grav == Grav::Up;
  const bool nowUpright = g == Grav::Down || g == Grav::Up;
  CellBox nb = old;
  if (upright != nowUpright)
  {
    // Onto a wall (or off one): the box turns about its middle, and steps
    // out of anything solid it now overlaps.
    nb.w = old.h;
    nb.h = old.w;
    nb.x = old.x + old.w / 2 - nb.w / 2;
    nb.y = old.y + old.h / 2 - nb.h / 2;
    if (mMap.overlapsSolid(nb))
    {
      bool found = false;
      for (int r = 1; r <= 3 && !found; ++r)
        for (int dy = -r; dy <= r && !found; ++dy)
          for (int dx = -r; dx <= r && !found; ++dx)
          {
            if (std::max(std::abs(dx), std::abs(dy)) != r)
              continue;
            const CellBox t{nb.x + dx, nb.y + dy, nb.w, nb.h};
            if (!mMap.overlapsSolid(t))
            {
              nb = t;
              found = true;
            }
          }
      if (!found)
        return; // no room to turn here
    }
  }
  p.grav = g;
  placeRunner(p, nb);
  p.prevX = p.x;
  p.prevY = p.y;
  p.state = PlayerState::Falling;
  p.frames = 0;
  p.somersault = -1;
  p.coyote = 0;
  setVisual(PlayerVisual::Falling);
  mLaunch = mLaunchBump = 0;
  mGrav.turned = true;
  const int now = clock();
  if (now - mGrav.lastFlip <= kQuick)
    mGrav.dizzy = kDizzy;
  mGrav.lastFlip = now;
}

void World::updateGravPlayer(const PlayerInput& input)
{
  auto& g = mGrav;
  auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  // A runner in a chamber takes its down at once.
  if (alive && !g.gun)
  {
    const CellBox b = p.box();
    const int z = gravZoneAt(b.x + b.w / 2, b.y + b.h / 2);
    if (z >= 0 && g.zones[std::size_t(z)].dir != p.grav)
      turnRunner(g.zones[std::size_t(z)].dir);
  }
  // Up on a switch throws it.
  if (alive && input.up && !g.upHeld)
    for (std::size_t i = 0; i < g.switches.size(); ++i)
      if (switchBox(g.switches[i]).intersects(p.box()))
      {
        hitGravSwitch(int(i));
        break;
      }
  g.upHeld = input.up;

  const PlayerState before = p.state;
  const std::size_t shots0 = mProjectiles.size(), vortices0 = g.vortices.size(), parts0 = mParticles.size();
  if (p.grav == Grav::Down)
    updatePlayer(input);
  else
  {
    // The turned runner moves on the turned map, as if down were down.
    const int gi = int(p.grav);
    const int Wc = mMap.width(), Hc = mMap.height();
    if (!mViewMap)
      mViewMap.emplace(mMap);
    mViewMap->makeView(mMap, gi);
    std::swap(mMap, *mViewMap);
    const Grav real = p.grav;
    const CellBox v = toView(gi, p.box(), Wc, Hc);
    p.grav = Grav::Down;
    p.x = v.x;
    p.y = v.y + v.h - 1;
    const int respawns = g.respawns;
    updatePlayer(input);
    std::swap(mMap, *mViewMap);
    if (g.respawns == respawns)
    {
      p.grav = real;
      placeRunner(p, fromView(gi, boxAt(p.x, p.y, Player::kWidth, p.height()), Wc, Hc));
    }
    // What it fired and kicked up, turned back.
    for (std::size_t i = shots0; i < mProjectiles.size(); ++i)
    {
      auto& pr = mProjectiles[i];
      const CellBox b = fromView(gi, pr.box(), Wc, Hc);
      pr.x = pr.prevX = b.x;
      pr.y = pr.prevY = b.y;
      pr.w = b.w;
      pr.h = b.h;
      int dx = 0, dy = 0;
      dirFromView(gi, pr.dx, pr.dy, dx, dy);
      pr.dx = dx;
      pr.dy = dy;
      if (pr.precise)
      {
        pointFromView(gi, pr.fx, pr.fy, Wc, Hc, pr.fx, pr.fy);
        dirFromView(gi, pr.vx, pr.vy, pr.vx, pr.vy);
      }
    }
    for (std::size_t i = vortices0; i < g.vortices.size(); ++i)
    {
      auto& vo = g.vortices[i];
      pointFromView(gi, vo.x, vo.y, Wc, Hc, vo.x, vo.y);
      dirFromView(gi, vo.vx, vo.vy, vo.vx, vo.vy);
      vo.down = real;
    }
    for (std::size_t i = parts0; i < mParticles.size(); ++i)
    {
      auto& pa = mParticles[i];
      float x = 0.0f, y = 0.0f;
      pointFromView(gi, pa.pos.x / kCellSize, pa.pos.y / kCellSize, Wc, Hc, x, y);
      pa.pos = {x * kCellSize, y * kCellSize};
      dirFromView(gi, pa.vel.x, pa.vel.y, pa.vel.x, pa.vel.y);
    }
  }
  g.jumped = before == PlayerState::OnGround && p.state == PlayerState::Jumping;
  // Gun Gravity: down is where the last shot went (straight shots only).
  if (g.gun)
    for (std::size_t i = shots0; i < mProjectiles.size(); ++i)
    {
      const auto& pr = mProjectiles[i];
      if (pr.kind == ShotKind::Enemy || (pr.dx != 0 && pr.dy != 0) || (pr.dx == 0 && pr.dy == 0))
        continue;
      const Grav want = pr.dx > 0 ? Grav::Right : (pr.dx < 0 ? Grav::Left : (pr.dy < 0 ? Grav::Up : Grav::Down));
      if (want != p.grav)
      {
        turnRunner(want);
        g.lastShot = 0;
      }
    }
}

// --- Switches, chambers, gates ---------------------------------------------------------------

void World::hitGravSwitch(int si)
{
  auto& g = mGrav;
  if (si < 0 || std::size_t(si) >= g.switches.size())
    return;
  auto& s = g.switches[std::size_t(si)];
  if (s.flash < kSwitchRest)
    return;
  const int now = clock();
  const bool quick = now - s.lastHit <= kQuick;
  s.lastHit = now;
  s.flash = 0;
  // The closet: two quick hits open its door.
  if (quick)
    for (auto& t : g.gates)
      if (!t.open && t.sw == si)
      {
        t.open = true;
        t.opened = 0;
        for (int k = 0; k < t.h; ++k)
          mMap.setBlock(t.bx, t.by + k, Tile::Empty);
        playSound(Sfx::ForceFieldOff);
        showMessage("A HIDDEN DOOR SLIDES OPEN");
      }
  if (s.zone >= 0)
    flipZone(s.zone);
}

void World::flipZone(int zi)
{
  auto& z = mGrav.zones[std::size_t(zi)];
  z.dir = z.dir == Grav::Up ? Grav::Down : Grav::Up;
  z.flip = 0;
  z.lastFlip = clock();
  playSound(Sfx::GravFlip);
  mCamera.shake(4, 0.6f);
}

bool World::shotAtGrav(Projectile& pr, const CellBox& b)
{
  auto& g = mGrav;
  for (std::size_t i = 0; i < g.switches.size(); ++i)
    if (switchBox(g.switches[i]).intersects(b))
    {
      hitGravSwitch(int(i));
      pr.alive = false;
      burst(cellCenter(b), rgb(255, 160, 60), rgb(255, 255, 255), 6, 1.0f);
      return true;
    }
  return false;
}

// --- The Grav Grenade ---------------------------------------------------------------------------

void World::throwVortex(int ox, int oy, int dx, int dy)
{
  GravVortex v;
  v.x = float(ox) + 0.5f;
  v.y = float(oy) + 0.5f;
  if (dx != 0)
  {
    v.vx = float(dx) * 2.0f;
    v.vy = -0.75f; // a lob, falling toward the thrower's down
  }
  else
    v.vy = float(dy) * 2.0f;
  v.down = Grav::Down; // turned with the runner (updateGravPlayer)
  mGrav.vortices.push_back(v);
}

void World::updateVortices()
{
  auto& g = mGrav;
  for (auto& v : g.vortices)
  {
    if (v.flight >= 0)
    {
      // Flying: falls toward its down, stops at the first thing it meets.
      float gx = 0.0f, gy = 0.0f;
      switch (v.down)
      {
        case Grav::Up:
          gy = -1.0f;
          break;
        case Grav::Left:
          gx = -1.0f;
          break;
        case Grav::Right:
          gx = 1.0f;
          break;
        default:
          gy = 1.0f;
          break;
      }
      v.vx += gx * 0.25f;
      v.vy += gy * 0.25f;
      ++v.flight;
      bool stop = v.flight >= kVortexFlight;
      const int steps = 4;
      for (int s = 0; s < steps && !stop; ++s)
      {
        const float nx = v.x + v.vx / float(steps), ny = v.y + v.vy / float(steps);
        const int cx = int(std::floor(nx)), cy = int(std::floor(ny));
        if (mMap.solid(cx, cy) || cx < 0 || cy < 0 || cx >= mMap.width() || cy >= mMap.height())
        {
          stop = true;
          break;
        }
        v.x = nx;
        v.y = ny;
        const CellBox at{cx, cy, 1, 1};
        for (std::size_t i = 0; i < g.switches.size() && !stop; ++i)
          if (switchBox(g.switches[i]).intersects(at))
          {
            hitGravSwitch(int(i));
            stop = true;
          }
        for (const auto& e : mEnemies)
          if (e.alive && !e.hidden && e.box().intersects(at))
            stop = true;
      }
      if (stop)
      {
        v.flight = -1;
        v.life = kVortexLife;
        v.vx = v.vy = 0.0f;
        playSound(Sfx::Vortex);
      }
      continue;
    }
    // Hanging: pulls enemies within reach to its middle, then pops.
    const int cx = int(std::floor(v.x)), cy = int(std::floor(v.y));
    for (auto& e : mEnemies)
    {
      if (!e.alive || e.hidden || !e.active)
        continue;
      const CellBox b = e.box();
      const int ex = b.x + b.w / 2, ey = b.y + b.h / 2;
      if (std::abs(ex - cx) > kVortexReach || std::abs(ey - cy) > kVortexReach)
        continue;
      if (ex != cx)
        mMap.moveHorizontally(e.x, e.y, e.w, e.h, sgn(cx - ex));
      if (ey != cy)
        mMap.moveVertically(e.x, e.y, e.w, e.h, sgn(cy - ey));
    }
    if (--v.life <= 0)
    {
      const auto& p = mPlayer;
      int damage = protoDef(int(ProtoId::GravGrenade)).damage;
      if (p.turbo > 0)
        damage *= 2;
      else if (p.virus > 0)
        damage = std::max(1, damage / 2);
      const CellBox blast{cx - kVortexBlast, cy - kVortexBlast, kVortexBlast * 2 + 1, kVortexBlast * 2 + 1};
      for (auto& e : mEnemies)
        if (e.alive && !e.hidden && e.box().intersects(blast))
          damageEnemy(e, damage);
      const Vec2 c{v.x * kCellSize, v.y * kCellSize};
      burst(c, rgb(190, 120, 255), rgb(40, 20, 60), 20, 2.2f);
      flashAt(c, 90.0f, rgb(170, 110, 255), 16);
      playSound(Sfx::SmallExplosion);
    }
  }
  g.vortices.erase(std::remove_if(g.vortices.begin(), g.vortices.end(),
                     [](const GravVortex& v) { return v.flight < 0 && v.life <= 0; }),
    g.vortices.end());
}

// --- Enemies -----------------------------------------------------------------------------------

namespace
{

// Which way is down for an enemy (its middle's chamber).
bool upsideDown(const World& w, const Enemy& e)
{
  const CellBox b = e.box();
  const int z = w.gravZoneAt(b.x + b.w / 2, b.y + b.h / 2);
  return z >= 0 && w.grav().zones[std::size_t(z)].dir == Grav::Up;
}

// Its feet's row: the box's bottom, or its top when it hangs from a ceiling.
int feetRow(const Enemy& e, bool up) { return up ? e.y - e.h + 1 : e.y; }

int runnerFeetRow(const Player& p)
{
  const CellBox b = p.box();
  return p.grav == Grav::Up ? b.top() : b.bottom();
}

} // namespace

// Falls toward its chamber's down; true while it is in the air. A landing
// after a long fall hurts (a Flip Walker's, `hurt`).
static bool enemyFall(World& w, const CollisionMap& map, Enemy& e, bool up, bool hurt, int& damage)
{
  (void)w;
  const CellBox b = e.box();
  const bool grounded = up ? map.touchingCeiling(b) : map.onSolidGround(b);
  damage = 0;
  if (grounded && e.dive == 0)
    return false;
  if (!grounded)
  {
    if (e.dive == 0)
      e.oy = e.y;
    const int step = fallStep(e.dive);
    ++e.dive;
    map.moveVertically(e.x, e.y, e.w, e.h, up ? -step : step);
    const CellBox nb = e.box();
    if (!(up ? map.touchingCeiling(nb) : map.onSolidGround(nb)))
    {
      if (e.y > map.height() + 4 || e.y < -4)
        e.alive = false;
      return true;
    }
  }
  // Landed.
  const int fell = std::abs(e.y - e.oy);
  if (hurt)
    damage = fell >= kBigFall ? 4 : (fell >= kFall ? 2 : 0);
  e.dive = 0;
  return false;
}

void World::updateFlipWalker(Enemy& e, const EnemyDef& def)
{
  const bool up = upsideDown(*this, e);
  e.attach = up ? 2 : 0;
  int damage = 0;
  const bool air = enemyFall(*this, mMap, e, up, true, damage);
  if (damage > 0)
  {
    playSound(Sfx::Thud);
    damageEnemy(e, damage);
  }
  if (air || !e.alive)
    return;
  const auto& p = mPlayer;
  // It turns to face a runner on its floor, then walks at it.
  const CellBox pb = p.box();
  const int toward = sgn(pb.x + pb.w / 2 - (e.x + e.w / 2));
  const bool sameFloor = std::abs(runnerFeetRow(p) - feetRow(e, up)) <= 2 && std::abs(pb.x - e.x) < 30;
  if (e.tell > 0)
  {
    if (--e.tell == 0)
      e.dir = e.aimX;
    return;
  }
  if (sameFloor && toward != 0 && toward != e.dir && p.state != PlayerState::Dying)
  {
    e.tell = def.tell;
    e.aimX = toward;
    return;
  }
  if (e.timer % std::max(1, def.stepEvery) != 0)
    return;
  // Patrol: turn at walls and at the edge of its floor.
  const int ahead = e.dir > 0 ? e.x + e.w : e.x - 1;
  const int floorRow = up ? e.y - e.h : e.y + 1;
  const bool edge = up ? !mMap.solid(ahead, floorRow) : !mMap.solidTop(ahead, floorRow);
  if (edge || mMap.moveHorizontally(e.x, e.y, e.w, e.h, e.dir) != MoveResult::Completed)
    e.dir = -e.dir;
}

void World::updateProbe(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  const CellBox b = e.box(), pb = p.box();
  const int dx = (pb.x + pb.w / 2) - (b.x + b.w / 2), dy = (pb.y + pb.h / 2) - (b.y + b.h / 2);
  const int dist = std::max(std::abs(dx), std::abs(dy));
  // Hover at its distance from the runner.
  if (e.timer % std::max(1, def.stepEvery) == 0)
  {
    const int way = dist < kProbeKeep ? -1 : (dist > kProbeKeep + 4 ? 1 : 0);
    if (way != 0)
    {
      if (dx != 0 && std::abs(dx) >= std::abs(dy) / 2)
        mMap.moveHorizontally(e.x, e.y, e.w, e.h, way * sgn(dx));
      if (dy != 0 && std::abs(dy) >= std::abs(dx) / 2)
        mMap.moveVertically(e.x, e.y, e.w, e.h, way * sgn(dy));
    }
  }
  if (e.cool > 0)
    --e.cool;
  if (e.tell > 0)
  {
    if (--e.tell == 0 && p.state != PlayerState::Dying)
    {
      // An aimed shot at the runner's middle.
      const float cx = float(b.x) + float(b.w) * 0.5f, cy = float(b.y) + float(b.h) * 0.5f;
      const float tx = float(pb.x) + float(pb.w) * 0.5f, ty = float(pb.y) + float(pb.h) * 0.5f;
      const float len = std::max(0.001f, std::hypot(tx - cx, ty - cy));
      Projectile pr;
      pr.kind = ShotKind::Enemy;
      pr.w = pr.h = 1;
      pr.speed = 1;
      pr.damage = 1;
      pr.range = 40;
      pr.carrier = e.carrier;
      pr.precise = true;
      pr.vx = (tx - cx) / len;
      pr.vy = (ty - cy) / len;
      pr.fx = cx + pr.vx * 2.5f;
      pr.fy = cy + pr.vy * 2.5f;
      pr.dx = sgn(int(std::lround(pr.vx * 2.0f)));
      pr.dy = sgn(int(std::lround(pr.vy * 2.0f)));
      pr.x = pr.prevX = int(std::floor(pr.fx));
      pr.y = pr.prevY = int(std::floor(pr.fy));
      if (!mMap.solid(pr.x, pr.y))
        mProjectiles.push_back(pr);
      playSound(Sfx::ProbeShot);
      e.cool = def.cooldown;
    }
    return;
  }
  if (e.cool == 0 && dist < 28 && isOnScreen(b, 0) && p.state != PlayerState::Dying)
    e.tell = def.tell;
}

void World::updateTestSubject(Enemy& e, const EnemyDef& def)
{
  const bool up = upsideDown(*this, e);
  e.attach = up ? 2 : 0;
  // Spikes kill it.
  if (mMap.overlapsHazard(e.box()))
  {
    damageEnemy(e, 4);
    if (!e.alive)
      return;
  }
  const auto& p = mPlayer;
  const CellBox pb = p.box();
  const int z = gravZoneAt(e.x + e.w / 2, e.y - e.h / 2);
  const int pz = gravZoneAt(pb.x + pb.w / 2, pb.y + pb.h / 2);
  // A copied jump: the runner's own arc, then a fall.
  if (e.cool > 0)
  {
    const auto& arc = runnerJumpArc();
    const int k = int(arc.size()) - e.cool;
    const int rise = k >= 0 && k < int(arc.size()) ? arc[std::size_t(k)] : 0;
    mMap.moveVertically(e.x, e.y, e.w, e.h, up ? rise : -rise);
    if (e.timer % std::max(1, def.stepEvery) == 0 && e.aimX != 0)
      mMap.moveHorizontally(e.x, e.y, e.w, e.h, e.aimX);
    --e.cool;
    if (mMap.overlapsHazard(e.box()))
      damageEnemy(e, 4);
    return;
  }
  int damage = 0;
  if (enemyFall(*this, mMap, e, up, false, damage))
    return;
  if (e.tell > 0)
  {
    // Crouched: then up it goes, the runner's way.
    if (--e.tell == 0)
      e.cool = int(runnerJumpArc().size());
    return;
  }
  if (mGrav.jumped && z == pz && z >= 0 && std::abs(pb.x - e.x) < 40)
  {
    e.tell = def.tell;
    e.aimX = sgn(pb.x + pb.w / 2 - (e.x + e.w / 2));
    return;
  }
  // Chase the runner along its floor.
  if (z != pz || z < 0 || p.state == PlayerState::Dying)
    return;
  const int toward = sgn(pb.x + pb.w / 2 - (e.x + e.w / 2));
  if (toward != 0)
    e.dir = toward;
  if (e.timer % std::max(1, def.stepEvery) == 0 && toward != 0)
    mMap.moveHorizontally(e.x, e.y, e.w, e.h, e.dir);
}

// --- Every frame -------------------------------------------------------------------------------

void World::updateGrav(const PlayerInput& input)
{
  (void)input;
  auto& g = mGrav;
  if (!g.on)
    return;
  for (auto& z : g.zones)
    z.flip = std::min(z.flip + 1, 1000);
  for (auto& s : g.switches)
    s.flash = std::min(s.flash + 1, 1000);
  if (g.dizzy > 0)
    --g.dizzy;
  ++g.lastShot;
  auto& p = mPlayer;
  // The thud of landing after a flip.
  if (g.turned && p.state != PlayerState::Falling && p.state != PlayerState::Jumping)
  {
    g.turned = false;
    if (p.state == PlayerState::OnGround || p.state == PlayerState::Recovering)
      playSound(Sfx::Thud);
  }
  // Gates open once their test subjects are down.
  for (auto& t : g.gates)
  {
    if (t.open)
    {
      t.opened = std::min(t.opened + 1, 1000);
      continue;
    }
    if (t.enemies.empty())
      continue;
    bool dead = true;
    for (int en : t.enemies)
      dead = dead && !(std::size_t(en) < mEnemies.size() && mEnemies[std::size_t(en)].alive);
    if (!dead)
      continue;
    t.open = true;
    t.opened = 0;
    for (int k = 0; k < t.h; ++k)
      mMap.setBlock(t.bx, t.by + k, Tile::Empty);
    playSound(Sfx::ForceFieldOff);
    showMessage("TEST COMPLETE - GATE OPEN");
  }
  // Gravity Probes pull the runner in (along its floor); Turbo shrugs it off.
  if (p.state != PlayerState::Dying && p.state != PlayerState::Teleporting && p.turbo == 0 && clock() % 4 == 0)
  {
    const CellBox pb = p.box();
    const int pcx = pb.x + pb.w / 2, pcy = pb.y + pb.h / 2;
    for (const auto& e : mEnemies)
    {
      if (!e.alive || !e.active || e.kind != EnemyKind::Probe)
        continue;
      const CellBox b = e.box();
      const int dx = b.x + b.w / 2 - pcx, dy = b.y + b.h / 2 - pcy;
      const int range = enemyDef(e.def).range;
      if (std::abs(dx) > range || std::abs(dy) > range)
        continue;
      CellBox nb = pb;
      const bool upright = p.grav == Grav::Down || p.grav == Grav::Up;
      if (upright && dx != 0)
        mMap.moveHorizontally(nb.x, nb.bottom(), nb.w, nb.h, sgn(dx));
      else if (!upright && dy != 0)
      {
        int bottom = nb.bottom();
        mMap.moveVertically(nb.x, bottom, nb.w, nb.h, sgn(dy));
        nb.y = bottom - nb.h + 1;
      }
      placeRunner(p, nb);
      break;
    }
  }
  updateVortices();
}

} // namespace gr
