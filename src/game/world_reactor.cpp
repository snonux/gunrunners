// Level 20, Reactor Core (SPEC 20): the Core Pulse. Every ten seconds a
// ring races out from the core and costs two hearts to every runner it
// passes outside a lead booth (`@ shield rect=`). The Deflector Bracer
// raises a shield that bounces shots back and soaks a pulse, throwing it
// back as an arc. Conduit Sparks ride their wires, Shield Drones keep their
// group from harm inside a bubble, Isotope Imps get faster with every pulse
// they live through. Three valves stop the pulses and open the lift cage.
// The Stop Motion bonus (rules=stop_motion) only moves the world on frames
// when the runner moves (World::update).

#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace gr
{

namespace
{

constexpr float kSparkSpeed = 1.5f; // cells a frame along its wire
constexpr int kDroneHover = 6;      // cells above its group (3 blocks)
constexpr int kImpLunge = 4;        // cells (2 blocks)
constexpr int kArcHalf = 6;         // cells: half the arc's height (6 blocks in all)
constexpr int kArcSpeed = 3;        // cells a frame
constexpr int kArcLife = 16;        // frames
constexpr int kArcDamage = 3;

int sgn(int v) { return (v > 0) - (v < 0); }

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

// gaps=a-b,c-d (degrees).
std::vector<std::pair<int, int>> gaps(const std::string& s)
{
  std::vector<std::pair<int, int>> out;
  for (const auto& item : names(s))
  {
    const auto dash = item.find('-', 1);
    if (dash == std::string::npos)
      continue;
    out.emplace_back(std::atoi(item.substr(0, dash).c_str()), std::atoi(item.substr(dash + 1).c_str()));
  }
  return out;
}

bool inGap(const CorePulse& c, float dx, float dy)
{
  if (c.gaps.empty())
    return false;
  float a = std::atan2(dy, dx) * 57.29578f;
  if (a < 0.0f)
    a += 360.0f;
  for (const auto& [a0, a1] : c.gaps)
  {
    if (a0 <= a1 ? (a >= float(a0) && a <= float(a1)) : (a >= float(a0) || a <= float(a1)))
      return true;
  }
  return false;
}

float centreX(const CellBox& b) { return float(b.x) + float(b.w) * 0.5f; }
float centreY(const CellBox& b) { return float(b.y) + float(b.h) * 0.5f; }

CellBox blockBox(int bx, int by) { return {bx * kCellsPerTile, by * kCellsPerTile, 2, 2}; }

} // namespace

// --- Setup ---------------------------------------------------------------------------------

bool World::setupReactorEntity(const EntityDef& e)
{
  auto& rs = mReactor;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  const bool core = mLevel->themeKey == "station_reactor";
  if (e.kind == "pulse" && e.hasPos)
  {
    CorePulse c;
    c.x = float(e.x * kCellsPerTile + 1);
    c.y = float(e.y * kCellsPerTile + 1);
    c.period = std::max(10, e.num("period", 150));
    c.damage = std::max(1, e.num("damage", 2));
    c.speed = e.real("speed", 8.0f) * float(kCellsPerTile);
    c.phase = std::clamp(e.num("phase", 0), 0, c.period - 1);
    c.clock = c.phase;
    c.gaps = gaps(e.str("gaps"));
    c.reach = float(e.num("reach", 0) * kCellsPerTile); // blocks; 0: past the map's corners
    rs.pulses.push_back(c);
    rs.on = true;
    return true;
  }
  if (e.kind == "shield" && e.rect("rect", x0, y0, x1, y1))
  {
    rs.booths.push_back({x0, y0, x1, y1});
    rs.on = true;
    return true;
  }
  if (e.kind == "wire" && e.has("path"))
  {
    CoreWire w;
    w.id = e.id;
    for (const auto& [px, py] : e.path("path"))
      w.path.emplace_back(px * kCellsPerTile, py * kCellsPerTile + 1); // a spark's bottom-left, on the block
    w.loop = w.path.size() > 2 && w.path.front() == w.path.back();
    for (std::size_t i = 1; i < w.path.size(); ++i)
      w.len += float(std::abs(w.path[i].first - w.path[i - 1].first) + std::abs(w.path[i].second - w.path[i - 1].second));
    rs.wires.push_back(w);
    rs.on = true;
    return true;
  }
  if (e.kind == "liftcage" && e.rect("rect", x0, y0, x1, y1))
  {
    rs.cageX0 = x0;
    rs.cageY0 = y0;
    rs.cageX1 = x1;
    rs.cageY1 = y1;
    rs.on = true;
    return true;
  }
  if (e.kind == "unlit" && e.rect("rect", x0, y0, x1, y1))
  {
    rs.unlitX0 = x0;
    rs.unlitY0 = y0;
    rs.unlitX1 = x1;
    rs.unlitY1 = y1;
    rs.on = true;
    return true;
  }
  if (e.kind == "console" && e.hasPos)
  {
    rs.consoleX = e.x;
    rs.consoleY = e.y;
    rs.on = true;
    return true;
  }
  if (core && e.kind == "deco")
  {
    ReactorDeco d;
    d.kind = e.str("kind");
    d.text = e.str("text");
    if (!e.rect("rect", d.x0, d.y0, d.x1, d.y1))
    {
      d.x0 = d.x1 = e.x;
      d.y0 = d.y1 = e.y;
    }
    rs.decos.push_back(d);
    rs.on = true;
    return true;
  }
  // Levels 5 and 45 have their own drips and valves.
  if (core && e.kind == "drip" && e.hasPos)
  {
    CoolantDrip d;
    d.x = e.x * kCellsPerTile;
    d.y = e.y * kCellsPerTile + 2; // under the pipe
    d.period = std::max(10, e.num("period", 20));
    rs.drips.push_back(d);
    rs.on = true;
    return true;
  }
  if (core && e.kind == "valve" && e.hasPos)
  {
    CoreValve v;
    v.id = e.id;
    v.x = e.x;
    v.y = e.y;
    rs.valves.push_back(v);
    rs.on = true;
    return true;
  }
  return false;
}

void World::setupReactorEnemy(Enemy& en, const EntityDef& e)
{
  auto& rs = mReactor;
  const std::size_t i = std::size_t(&en - mEnemies.data());
  if (rs.names.size() <= i)
    rs.names.resize(i + 1);
  rs.names[i] = e.id;
  if (en.kind == EnemyKind::Spark)
  {
    SparkRide sr;
    sr.enemy = int(i);
    sr.wireId = e.str("wire");
    rs.sparks.push_back(sr);
    rs.on = true;
  }
  if (en.kind == EnemyKind::ShieldDrone)
  {
    DroneGroup g;
    g.enemy = int(i);
    g.names = names(e.str("guard"));
    rs.drones.push_back(g);
    rs.on = true;
  }
  if (en.kind == EnemyKind::Imp)
  {
    en.variant = e.num("hat", 0) != 0 ? 1 : 0;
    rs.on = true;
  }
}

void World::linkReactor()
{
  auto& rs = mReactor;
  if (!rs.on)
    return;
  const float Wc = float(mMap.width()), Hc = float(mMap.height());
  for (auto& c : rs.pulses)
  {
    const float fx = std::max(c.x, Wc - c.x), fy = std::max(c.y, Hc - c.y);
    if (c.reach <= 0.0f)
      c.reach = std::hypot(fx, fy) + 8.0f;
  }
  auto byName = [&](const std::string& n) {
    for (std::size_t k = 0; k < rs.names.size(); ++k)
      if (!n.empty() && rs.names[k] == n)
        return int(k);
    return -1;
  };
  for (auto& sr : rs.sparks)
  {
    for (std::size_t w = 0; w < rs.wires.size(); ++w)
      if (rs.wires[w].id == sr.wireId || (sr.wireId.empty() && sr.wire < 0))
        sr.wire = int(w);
    if (sr.wire < 0)
      continue;
    // Onto its wire where it is closest.
    Enemy& e = mEnemies[std::size_t(sr.enemy)];
    sr.s = closestOnWire(rs.wires[std::size_t(sr.wire)], e.x, e.y);
    e.attach = sr.wire;
    placeSpark(sr);
  }
  for (auto& g : rs.drones)
    for (const auto& n : g.names)
    {
      const int k = byName(n);
      if (k >= 0)
        g.group.push_back(k);
    }
  setCage(rs.cageOpen);
  if (rs.unlitX0 >= 0)
    for (int ty = rs.unlitY0; ty <= rs.unlitY1; ++ty)
      for (int tx = rs.unlitX0; tx <= rs.unlitX1; ++tx)
        if (tx >= 0 && ty >= 0 && tx < mLevel->width && ty < mLevel->height)
          mLayerMask[std::size_t(ty * mLevel->width + tx)] = 1; // drawn only in a flash (drawReactorBack)
}

void World::setCage(bool open)
{
  auto& rs = mReactor;
  rs.cageOpen = open;
  if (rs.cageX0 < 0)
    return;
  for (int ty = rs.cageY0; ty <= rs.cageY1; ++ty)
    mMap.setBlock(rs.cageX1, ty, open ? Tile::Empty : Tile::Solid);
}

// --- Wires -----------------------------------------------------------------------------------

float World::closestOnWire(const CoreWire& w, int x, int y) const
{
  float best = 0.0f, bestD = 1e9f, s = 0.0f;
  for (std::size_t i = 1; i < w.path.size(); ++i)
  {
    const auto [ax, ay] = w.path[i - 1];
    const auto [bx, by] = w.path[i];
    const int len = std::abs(bx - ax) + std::abs(by - ay);
    for (int k = 0; k <= len; ++k)
    {
      const float t = len > 0 ? float(k) / float(len) : 0.0f;
      const float px = float(ax) + float(bx - ax) * t, py = float(ay) + float(by - ay) * t;
      const float d = std::abs(px - float(x)) + std::abs(py - float(y));
      if (d < bestD)
      {
        bestD = d;
        best = s + float(k);
      }
    }
    s += float(len);
  }
  return best;
}

void World::wirePoint(const CoreWire& w, float s, float& x, float& y) const
{
  if (w.path.empty())
  {
    x = y = 0.0f;
    return;
  }
  if (w.loop && w.len > 0.0f)
  {
    s = std::fmod(s, w.len);
    if (s < 0.0f)
      s += w.len;
  }
  s = std::clamp(s, 0.0f, w.len);
  for (std::size_t i = 1; i < w.path.size(); ++i)
  {
    const auto [ax, ay] = w.path[i - 1];
    const auto [bx, by] = w.path[i];
    const float len = float(std::abs(bx - ax) + std::abs(by - ay));
    if (s <= len || i + 1 == w.path.size())
    {
      const float t = len > 0.0f ? std::min(1.0f, s / len) : 0.0f;
      x = float(ax) + float(bx - ax) * t;
      y = float(ay) + float(by - ay) * t;
      return;
    }
    s -= len;
  }
  x = float(w.path.back().first);
  y = float(w.path.back().second);
}

void World::placeSpark(const SparkRide& sr)
{
  Enemy& e = mEnemies[std::size_t(sr.enemy)];
  float x = 0.0f, y = 0.0f;
  wirePoint(mReactor.wires[std::size_t(sr.wire)], sr.s, x, y);
  e.x = int(std::lround(x));
  e.y = int(std::lround(y));
  e.dir = sr.dir;
}

// --- Booths, the Bracer --------------------------------------------------------------------------

bool World::inBooth() const
{
  const CellBox b = mPlayer.box();
  const int cx = b.x + b.w / 2, feet = b.y + b.h - 1;
  for (const auto& bo : mReactor.booths)
    if (cx >= bo.x0 * kCellsPerTile && cx < (bo.x1 + 1) * kCellsPerTile && feet >= bo.y0 * kCellsPerTile &&
        feet < (bo.y1 + 1) * kCellsPerTile)
      return true;
  return false;
}

bool World::atValve(int i) const
{
  if (i < 0 || std::size_t(i) >= mReactor.valves.size())
    return false;
  const CoreValve& v = mReactor.valves[std::size_t(i)];
  return blockBox(v.x, v.y).intersects(mPlayer.box()) && mPlayer.state == PlayerState::OnGround;
}

bool World::bracerUp() const { return mReactor.raise > 0 && mPlayer.state != PlayerState::Dying; }

CellBox World::bracerBox() const
{
  const CellBox b = mPlayer.box();
  return {mPlayer.facing > 0 ? b.x + b.w : b.x - 2, b.y - 1, 2, b.h + 1};
}

bool World::updateBracer(const Button& fire)
{
  auto& p = mPlayer;
  auto& rs = mReactor;
  if (p.weapon != Weapon::Proto || ProtoId(p.proto) != ProtoId::DeflectorBracer)
  {
    rs.raise = 0;
    return false;
  }
  const bool canRaise = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  if (fire.pressed && canRaise)
  {
    ++p.charge;
    if (rs.raise > 0)
    {
      if (++rs.raise > kBracerRaise)
      {
        rs.raise = 0;
        rs.spent = true; // let go of fire to raise it again
        dropBracer();
      }
    }
    else if (!rs.spent && p.charge >= kBracerTap && p.ammo > 0)
    {
      rs.raise = 1;
      --p.ammo;
      playSound(Sfx::BracerUp);
    }
    return true;
  }
  // Let go: a tap fires a pulse shot.
  if (p.charge > 0 && p.charge < kBracerTap && canFire() && (p.shotCooldown == 0 || p.turbo > 0))
  {
    fireShot();
    p.shotCooldown = protoDef(int(ProtoId::DeflectorBracer)).cooldown;
  }
  p.charge = 0;
  rs.spent = false;
  if (rs.raise > 0)
  {
    rs.raise = 0;
    dropBracer();
  }
  return true;
}

void World::dropBracer()
{
  auto& p = mPlayer;
  if (p.weapon == Weapon::Proto && p.ammo <= 0)
  {
    p.ammo = 0;
    p.weapon = Weapon::Normal;
    p.proto = -1;
    showMessage("OUT OF AMMO - BACK TO THE BLASTER");
  }
}

bool World::shotAtBracer(Projectile& pr)
{
  if (!bracerUp() || pr.kind != ShotKind::Enemy || !pr.box().intersects(bracerBox()))
    return false;
  // Back along its path, and now it hurts enemies.
  pr.kind = ShotKind::Normal;
  pr.dx = -pr.dx;
  pr.dy = -pr.dy;
  pr.vx = -pr.vx;
  pr.vy = -pr.vy;
  pr.carrier = false;
  pr.strong = true;
  pr.speed = std::max(2, pr.speed);
  pr.range = -1;
  mReactor.reflect = 6;
  playSound(Sfx::Deflect);
  burst(cellCenter(pr.box()), rgb(200, 240, 255), rgb(90, 200, 255), 8, 1.4f);
  return true;
}

void World::bracerPush(Enemy& e, int dx)
{
  if (!e.alive || dx == 0 || e.kind == EnemyKind::Spark)
    return;
  mMap.moveHorizontally(e.x, e.y, e.w, e.h, dx * kCellsPerTile);
}

bool World::droneShields(const Enemy& e) const
{
  for (const auto& g : mReactor.drones)
  {
    const Enemy& d = mEnemies[std::size_t(g.enemy)];
    if (!d.alive || &d == &e)
      continue;
    const CellBox db = d.box(), eb = e.box();
    if (std::hypot(centreX(eb) - centreX(db), centreY(eb) - centreY(db)) <= float(kBubble))
      return true;
  }
  return false;
}

bool World::reactorBlocksDamage(Enemy& e)
{
  if (!mReactor.on || mReactor.drones.empty() || !droneShields(e))
    return false;
  for (auto& g : mReactor.drones)
  {
    const Enemy& d = mEnemies[std::size_t(g.enemy)];
    if (d.alive && std::hypot(centreX(e.box()) - centreX(d.box()), centreY(e.box()) - centreY(d.box())) <= float(kBubble))
      g.shimmer = 8;
  }
  e.flash = 2;
  playSound(Sfx::Deflect);
  return true;
}

// --- Update ----------------------------------------------------------------------------------

bool World::runnerMoving(const PlayerInput& input) const
{
  const auto& p = mPlayer;
  return input.left || input.right || input.up || input.down || input.jump.pressed || p.state == PlayerState::Jumping ||
    p.state == PlayerState::Falling;
}

int World::framesToRing() const
{
  // How long until the next ring (or one already out) gets to the runner.
  const auto& rs = mReactor;
  if (rs.stopped >= 0 || rs.pulses.empty())
    return 1 << 20;
  const CellBox b = mPlayer.box();
  int best = 1 << 20;
  for (const auto& c : rs.pulses)
  {
    const float d = std::hypot(centreX(b) - c.x, centreY(b) - c.y);
    for (const auto& ring : c.rings)
      if (ring.r < d)
        best = std::min(best, int(std::ceil((d - ring.r) / c.speed)));
    best = std::min(best, c.period - c.clock + int(std::ceil(d / c.speed)));
  }
  return best;
}

int World::pulseCountdown() const
{
  const auto& rs = mReactor;
  if (rs.stopped >= 0 || rs.pulses.empty())
    return -1;
  const CorePulse& c = rs.pulses.front();
  return std::clamp((c.period - c.clock) * 10 / c.period, 0, 10);
}

float World::pulseHum() const
{
  const auto& rs = mReactor;
  if (rs.pulses.empty())
    return 0.0f;
  if (rs.stopped >= 0)
    return std::max(0.0f, 1.0f - float(rs.stopped) / float(kPulseWindDown)) * 0.5f;
  const CorePulse& c = rs.pulses.front();
  const int left = c.period - c.clock;
  return left > kPulseHum ? 0.0f : 1.0f - float(left) / float(kPulseHum);
}

void World::pulseHit(CorePulse& c, CoreRing& ring)
{
  auto& p = mPlayer;
  auto& rs = mReactor;
  if (p.state == PlayerState::Dying || p.state == PlayerState::Teleporting || inBooth())
    return;
  const CellBox b = p.box();
  if (inGap(c, centreX(b) - c.x, centreY(b) - c.y))
    return;
  if (bracerUp() && rs.soakCool == 0 && !ring.soaked)
  {
    // Soaked: thrown back as an arc the way the runner faces.
    ring.soaked = true;
    rs.soakCool = kBracerSoak;
    rs.soakFlash = 10;
    BracerArc a;
    a.x = centreX(b) + float(p.facing * 2);
    a.y = centreY(b);
    a.dir = p.facing;
    rs.arcs.push_back(a);
    playSound(Sfx::Deflect);
    flashAt({a.x * kCellSize, a.y * kCellSize}, 120.0f, rgb(120, 210, 255), 14);
    return;
  }
  // The planner's look-ahead leaves the pulses to the bot (it waits in a
  // booth or raises the Bracer; frontend/bot.cpp). In Stop Motion the rings
  // are terrain like any other.
  if (mSimulation && !rs.stopMotion)
    return;
  hurtPlayer(c.damage);
}

void World::updateReactor(const PlayerInput& input)
{
  auto& rs = mReactor;
  if (!rs.on)
    return;
  auto& p = mPlayer;
  if (rs.soakCool > 0)
    --rs.soakCool;
  if (rs.reflect > 0)
    --rs.reflect;
  if (rs.soakFlash > 0)
    --rs.soakFlash;
  if (rs.flash > 0)
    --rs.flash;
  if (rs.fanfare > 0)
    --rs.fanfare;
  for (auto& g : rs.drones)
    if (g.shimmer > 0)
      --g.shimmer;
  if (rs.stopped >= 0 && rs.stopped < 1000)
    ++rs.stopped;

  // The core: the hum, then a ring.
  const CellBox pb = p.box();
  const float px = centreX(pb), py = centreY(pb);
  for (auto& c : rs.pulses)
  {
    if (rs.stopped < 0)
    {
      ++c.clock;
      if (c.clock == c.period - kPulseHum && !mSimulation)
        playSound(Sfx::CoreHum);
      if (c.clock >= c.period)
      {
        c.clock = 0;
        CoreRing ring;
        ring.side = std::hypot(px - c.x, py - c.y) < 0.5f ? -1 : 1;
        c.rings.push_back(ring);
        rs.flash = kPulseFlash;
        playSound(Sfx::CorePulse);
        if (!rs.stopMotion)
          mCamera.shake(6, 1.0f);
      }
    }
    for (auto& ring : c.rings)
    {
      const float r0 = ring.r;
      ring.r += c.speed;
      const int side = std::hypot(px - c.x, py - c.y) < ring.r ? -1 : 1;
      if (side != ring.side)
        pulseHit(c, ring);
      ring.side = side;
      // Imps that live through it speed up; the one in the hard hat salutes.
      for (auto& e : mEnemies)
      {
        if (!e.alive || !e.active || e.kind != EnemyKind::Imp)
          continue;
        const float d = std::hypot(centreX(e.box()) - c.x, centreY(e.box()) - c.y);
        if (d >= r0 && d < ring.r)
        {
          e.attach = std::min(4, e.attach + 1);
          if (e.variant == 1)
            e.aimX = 15;
        }
      }
    }
    c.rings.erase(std::remove_if(c.rings.begin(), c.rings.end(), [&](const CoreRing& r) { return r.r > c.reach; }),
      c.rings.end());
  }

  // The candid camera on the crane only shows in a flash.
  for (auto& e : mEnemies)
    if (e.kind == EnemyKind::Camera && e.alive && !rs.pulses.empty() && !rs.stopMotion)
      e.hidden = rs.flash == 0;

  // Thrown-back arcs.
  for (auto& a : rs.arcs)
  {
    ++a.life;
    for (int step = 0; step < kArcSpeed && a.life <= kArcLife; ++step)
    {
      a.x += float(a.dir);
      if (mMap.solid(int(a.x), int(a.y)))
      {
        a.life = kArcLife + 1;
        break;
      }
      const CellBox ab{int(a.x) - 1, int(a.y) - kArcHalf, 2, kArcHalf * 2};
      // A Shield Drone's bubble stops it: it breaks on the bubble, and the
      // jolt goes to the drone.
      for (auto& g : rs.drones)
      {
        Enemy& d = mEnemies[std::size_t(g.enemy)];
        if (!d.alive || a.life > kArcLife)
          continue;
        const float dx = std::abs(a.x - centreX(d.box()));
        const float dy = std::max(0.0f, std::abs(a.y - centreY(d.box())) - float(kArcHalf));
        if (std::hypot(dx, dy) <= float(kBubble))
        {
          a.life = kArcLife + 1;
          g.shimmer = 10;
          damageEnemy(d, kArcDamage);
        }
      }
      if (a.life > kArcLife)
        break;
      for (auto& e : mEnemies)
      {
        if (!e.alive || !e.active || e.hidden || !e.box().intersects(ab) ||
            std::find(a.hit.begin(), a.hit.end(), e.id) != a.hit.end())
          continue;
        a.hit.push_back(e.id);
        damageEnemy(e, kArcDamage);
      }
    }
  }
  rs.arcs.erase(std::remove_if(rs.arcs.begin(), rs.arcs.end(), [](const BracerArc& a) { return a.life > kArcLife; }),
    rs.arcs.end());

  // The leaking coolant pipe.
  for (auto& d : rs.drips)
  {
    if (++d.t >= d.period)
      d.t = 0;
    if (d.t == 8)
      d.drops.push_back(float(d.y));
    for (auto& y : d.drops)
    {
      y += 1.0f;
      const CellBox db{d.x, int(y) - 1, 1, 2};
      if (mMap.solid(d.x, int(y)))
      {
        burst({(float(d.x) + 0.5f) * kCellSize, y * kCellSize}, rgb(150, 255, 90), rgb(60, 140, 40), 4, 0.8f);
        y = -1.0f;
      }
      else if (bracerUp() && (db.intersects(bracerBox()) || db.intersects(p.box())))
      {
        burst({(float(d.x) + 0.5f) * kCellSize, y * kCellSize}, rgb(150, 255, 90), rgb(90, 200, 255), 5, 1.0f);
        y = -1.0f; // the shield takes it
      }
      else if (db.intersects(p.hitBox()) && p.state != PlayerState::Dying)
      {
        if (p.virus == 0 && p.mercy == 0)
          infect();
        y = -1.0f;
      }
    }
    d.drops.erase(std::remove_if(d.drops.begin(), d.drops.end(), [](float y) { return y < 0.0f; }), d.drops.end());
  }

  // Valves: hold up at one to shut it.
  const bool upNow = input.up && !rs.upHeld;
  rs.upHeld = input.up;
  for (auto& v : rs.valves)
  {
    if (v.shut)
      continue;
    const bool at = input.up && atValve(int(&v - rs.valves.data()));
    if (!at)
    {
      v.held = 0;
      continue;
    }
    if (v.held % 8 == 0)
      playSound(Sfx::ValveTurn);
    if (++v.held >= kValveHold)
    {
      v.shut = true;
      int left = 0;
      for (const auto& o : rs.valves)
        left += !o.shut;
      addScore(500, cellCenter(blockBox(v.x, v.y)));
      playSound(Sfx::ValveShut);
      if (left > 0)
        showMessage("VALVE SHUT - " + std::to_string(left) + " TO GO");
      else
      {
        rs.stopped = 0;
        for (auto& c : rs.pulses)
          c.clock = 0;
        setCage(true);
        showMessage("THE CORE IS POWERING DOWN - THE LIFT IS OPEN");
      }
    }
  }

  // DO NOT PRESS.
  if (rs.consoleX >= 0 && upNow && rs.fanfare == 0 &&
      CellBox{rs.consoleX * kCellsPerTile - 1, rs.consoleY * kCellsPerTile - 1, 4, 4}.intersects(pb))
  {
    rs.fanfare = 90;
    playSound(Sfx::Rewind);
    showMessage("YOU PRESSED IT");
  }
}

// --- Enemies ---------------------------------------------------------------------------------

void World::updateSpark(Enemy& e, const EnemyDef& /*def*/)
{
  auto& rs = mReactor;
  for (auto& sr : rs.sparks)
  {
    if (sr.enemy != int(&e - mEnemies.data()) || sr.wire < 0)
      continue;
    const CoreWire& w = rs.wires[std::size_t(sr.wire)];
    sr.s += kSparkSpeed * float(sr.dir);
    if (!w.loop)
    {
      if (sr.s >= w.len)
      {
        sr.s = w.len;
        sr.dir = -1;
      }
      else if (sr.s <= 0.0f)
      {
        sr.s = 0.0f;
        sr.dir = 1;
      }
    }
    else if (sr.s >= w.len)
      sr.s -= w.len;
    placeSpark(sr);
    // Which way it is going along the map (drawn).
    float ax = 0.0f, ay = 0.0f, bx = 0.0f, by = 0.0f;
    wirePoint(w, sr.s, ax, ay);
    wirePoint(w, sr.s + float(sr.dir), bx, by);
    if (std::abs(bx - ax) > 0.1f)
      e.dir = bx > ax ? 1 : -1;
    return;
  }
}

void World::updateShieldDrone(Enemy& e, const EnemyDef& def)
{
  const DroneGroup* g = nullptr;
  for (const auto& dg : mReactor.drones)
    if (dg.enemy == int(&e - mEnemies.data()))
      g = &dg;
  if (!g)
    return;
  float sx = 0.0f;
  int n = 0, top = 1 << 20;
  for (int k : g->group)
  {
    const Enemy& m = mEnemies[std::size_t(k)];
    if (!m.alive)
      continue;
    sx += centreX(m.box());
    top = std::min(top, m.box().y);
    ++n;
  }
  if (n == 0)
    return; // its group is gone: it hangs where it is
  if (e.timer % std::max(1, def.stepEvery) != 0)
    return;
  const int tx = int(std::lround(sx / float(n) - float(e.w) * 0.5f));
  const int ty = top - kDroneHover; // its bottom row
  if (tx != e.x)
  {
    e.dir = sgn(tx - e.x);
    mMap.moveHorizontally(e.x, e.y, e.w, e.h, sgn(tx - e.x));
  }
  if (ty != e.y)
    mMap.moveVertically(e.x, e.y, e.w, e.h, sgn(ty - e.y));
}

void World::updateImp(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  // Falls when it has no floor (pushed off a ledge).
  if (!mMap.onSolidGround(e.box()))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, 2);
    return;
  }
  if (e.aimX > 0)
  {
    --e.aimX; // saluting the pulse
    return;
  }
  if (e.cool > 0)
    --e.cool;
  const CellBox pb = p.box();
  const int dx = (pb.x + pb.w / 2) - (e.x + e.w / 2);
  const bool sameFloor = std::abs((pb.y + pb.h - 1) - e.y) <= 3;
  if (e.dive > 0)
  {
    // The lunge: two blocks in two frames.
    --e.dive;
    if (mMap.moveHorizontally(e.x, e.y, e.w, e.h, e.dir * kImpLunge / 2) != MoveResult::Completed)
      e.dive = 0;
    if (e.dive == 0)
      e.cool = def.cooldown;
    return;
  }
  if (e.tell > 0)
  {
    if (--e.tell == 0)
      e.dive = 2;
    return;
  }
  if (p.state == PlayerState::Dying)
    return;
  if (dx != 0)
    e.dir = sgn(dx);
  if (sameFloor && std::abs(dx) <= kImpLunge + pb.w && e.cool == 0)
  {
    e.tell = def.tell;
    playSound(Sfx::Click);
    return;
  }
  // Scuttle: 1/2 a cell a frame, 1/4 faster for every pulse it has lived through.
  e.fx += 0.5f + 0.25f * float(std::clamp(e.attach, 0, 4));
  while (e.fx >= 1.0f)
  {
    e.fx -= 1.0f;
    const int ahead = e.dir > 0 ? e.x + e.w : e.x - 1;
    if (!mMap.solidTop(ahead, e.y + 1) || mMap.moveHorizontally(e.x, e.y, e.w, e.h, e.dir) != MoveResult::Completed)
    {
      e.fx = 0.0f;
      break; // the edge of its floor: it waits there
    }
  }
}

// --- Saves -----------------------------------------------------------------------------------

bool World::reactorCanSave() const
{
  for (const auto& c : mReactor.pulses)
    if (!c.rings.empty())
      return false;
  return mReactor.arcs.empty() && mReactor.raise == 0;
}

std::vector<int> World::reactorSave() const
{
  const auto& rs = mReactor;
  std::vector<int> v{rs.stopped, rs.cageOpen ? 1 : 0, rs.soakCool};
  for (const auto& c : rs.pulses)
    v.push_back(c.clock);
  for (const auto& d : rs.valves)
    v.push_back(d.shut ? 1 : 0);
  for (const auto& sr : rs.sparks)
    v.push_back(int(sr.s * 2.0f) * 2 + (sr.dir > 0 ? 1 : 0));
  return v;
}

bool World::validReactorSave(const std::vector<int>& v) const
{
  const auto& rs = mReactor;
  if (v.size() != 3 + rs.pulses.size() + rs.valves.size() + rs.sparks.size())
    return false;
  for (std::size_t i = 0; i < rs.pulses.size(); ++i)
    if (v[3 + i] < 0 || v[3 + i] >= rs.pulses[i].period)
      return false;
  return true;
}

void World::loadReactor(const std::vector<int>& v)
{
  auto& rs = mReactor;
  std::size_t at = 0;
  rs.stopped = v[at++];
  const bool open = v[at++] != 0;
  rs.soakCool = v[at++];
  for (auto& c : rs.pulses)
  {
    c.clock = v[at++];
    c.rings.clear();
  }
  for (auto& d : rs.valves)
  {
    d.shut = v[at++] != 0;
    d.held = 0;
  }
  for (auto& sr : rs.sparks)
  {
    const int k = v[at++];
    sr.s = float(k / 2) * 0.5f;
    sr.dir = (k & 1) ? 1 : -1;
    if (sr.wire >= 0 && mEnemies[std::size_t(sr.enemy)].alive)
      placeSpark(sr);
  }
  for (auto& d : rs.drips)
    d.drops.clear();
  rs.arcs.clear();
  rs.raise = 0;
  rs.flash = 0;
  setCage(open);
}

} // namespace gr
