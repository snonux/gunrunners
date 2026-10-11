// Level 31, Storm Relay: lightning zones that strike their highest metal,
// dead machines a strike powers, the Tesla Rod (a lightning rod you throw),
// Storm Gulls, Kite Drones and Foghorns, and the Bungee bonus (rules=bungee).
// Drawing is in world_storm_draw.cpp.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace gr
{

namespace
{

constexpr float kPi = 3.14159265f;

CellBox blockRect(int x0, int y0, int x1, int y1)
{
  return {std::min(x0, x1) * kCellsPerTile, std::min(y0, y1) * kCellsPerTile, (std::abs(x1 - x0) + 1) * kCellsPerTile,
    (std::abs(y1 - y0) + 1) * kCellsPerTile};
}

int sgn(int v) { return (v > 0) - (v < 0); }

// Standing on metal touches it too.
bool touches(const CellBox& metal, const CellBox& b) { return CellBox{metal.x, metal.y - 1, metal.w, metal.h + 1}.intersects(b); }

// A rod comes out: zones aimed at it, or at a later one, follow.
void removeRod(StormState& s, int i)
{
  s.rods.erase(s.rods.begin() + i);
  for (auto& z : s.zones)
    if (z.target.kind == TargetKind::Rod)
    {
      if (z.target.index == i)
        z.target = {};
      else if (z.target.index > i)
        --z.target.index;
    }
}

// The Kite Drone's figure eight about its anchor.
void figureEight(int phase, int& dx, int& dy)
{
  const float a = 2.0f * kPi * float(phase % kKiteLoop) / float(kKiteLoop);
  dx = int(std::lround(float(kKiteRadius) * std::sin(a)));
  dy = int(std::lround(float(kKiteRadius) * 0.5f * std::sin(2.0f * a)));
}

} // namespace

// G-U-N in Morse (--. ..- -.), 4-frame dots, 12-frame dashes, 4 frames between
// marks, 12 between letters and 40 before it starts again.
bool morseLight(int t)
{
  static const char* const kCode = "--. ..- -.";
  int at = 0;
  int loop = 0;
  for (const char* c = kCode; *c; ++c)
    loop += *c == ' ' ? 8 : (*c == '-' ? kMorseDash : kMorseDot) + 4;
  loop += 40;
  t %= loop;
  for (const char* c = kCode; *c; ++c)
  {
    if (*c == ' ')
    {
      at += 8; // with the mark's 4: 12 between letters
      continue;
    }
    const int len = *c == '-' ? kMorseDash : kMorseDot;
    if (t >= at && t < at + len)
      return true;
    at += len + 4;
  }
  return false;
}

// --- Setup ---------------------------------------------------------------------------------

bool World::setupStormEntity(const EntityDef& e)
{
  if (mLevel->themeKey != "storm_cliffs")
    return false;
  auto& s = mStorm;
  s.on = true;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  const bool hasRect = e.rect("rect", x0, y0, x1, y1);
  if (e.kind == "metal" && hasRect)
  {
    MetalGroup* g = nullptr;
    for (auto& m : s.metals)
      if (!e.id.empty() && m.id == e.id)
        g = &m;
    if (!g)
    {
      s.metals.push_back({e.id, {}, 0});
      g = &s.metals.back();
    }
    g->boxes.push_back(blockRect(x0, y0, x1, y1));
    return true;
  }
  if (e.kind == "machine" && hasRect)
  {
    Machine m;
    m.id = e.id;
    const std::string kind = e.str("kind", "door");
    m.kind = kind == "lift" ? MachineKind::Lift
      : kind == "generator" ? MachineKind::Generator
      : kind == "lamp"      ? MachineKind::Lamp
                            : MachineKind::Door;
    m.box = blockRect(x0, y0, x1, y1);
    m.liftId = e.str("lift");
    if (!e.rect("opens", m.ox0, m.oy0, m.ox1, m.oy1))
      m.ox1 = m.oy1 = -1;
    m.ladderX = e.num("ladder", -1);
    s.machines.push_back(m);
    return true;
  }
  if (e.kind == "lightning" && e.rect("zone", x0, y0, x1, y1))
  {
    StormZone z;
    z.id = e.id;
    z.rect = blockRect(x0, y0, x1, y1);
    z.period = std::max(kStrikeGlow + kStrikeHurt + 1, e.num("period", 90));
    z.glow = std::clamp(e.num("glow", kStrikeGlow), 1, z.period - 1);
    z.damage = std::max(1, e.num("damage", 2));
    z.t = e.num("phase", 0) % z.period;
    s.zones.push_back(z);
    // `cycle=` names metal pieces; they are looked up in linkStorm.
    mStormCycles.push_back(e.str("cycle"));
    return true;
  }
  if (e.kind == "spawn" && hasRect)
  {
    GullSpawn g;
    g.rect = blockRect(x0, y0, x1, y1);
    g.def = enemyIndex(e.str("kind", "storm_gull"));
    g.count = e.num("count", 3);
    g.max = e.num("max", 2);
    s.spawns.push_back(g);
    return true;
  }
  if (e.kind == "frygull" && e.hasPos)
  {
    s.fryX = e.x * kCellsPerTile;
    s.fryY = e.y * kCellsPerTile + 1;
    return true;
  }
  if (e.kind == "post" && e.hasPos)
  {
    s.postX = float(e.x * kCellsPerTile) + 1.0f;
    s.postY = float(e.y * kCellsPerTile) + 1.0f;
    return true;
  }
  if (e.kind == "lighthouse" && hasRect)
  {
    s.tower = blockRect(x0, y0, x1, y1);
    return true;
  }
  if (e.kind == "ring" && e.hasPos)
  {
    s.rings.emplace_back(e.x * kCellsPerTile + 1, e.y * kCellsPerTile + 1);
    s.ringTaken.push_back(false);
    return true;
  }
  if (e.kind == "duck")
    return true; // the duck's costume is only drawn
  return false;
}

void World::setupStormEnemy(Enemy& en, const EntityDef& e)
{
  (void)e;
  auto& s = mStorm;
  if (!s.on)
    return;
  if (en.kind == EnemyKind::KiteDrone)
  {
    Kite k;
    k.enemy = int(mEnemies.size()) - 1;
    k.ax = en.x + en.w / 2;
    k.ay = en.y - en.h / 2;
    s.kites.push_back(k);
  }
}

void World::linkStorm()
{
  auto& s = mStorm;
  s.on = s.on || mLevel->themeKey == "storm_cliffs";
  s.bungee = mLevel->rules.find("bungee") != std::string::npos;
  if (!s.on)
    return;
  for (auto& g : s.metals)
  {
    g.top = g.boxes.empty() ? 0 : g.boxes.front().y;
    for (const auto& b : g.boxes)
      g.top = std::min(g.top, b.y);
  }
  for (std::size_t i = 0; i < s.zones.size() && i < mStormCycles.size(); ++i)
  {
    const std::string& list = mStormCycles[i];
    std::size_t at = 0;
    while (at < list.size())
    {
      const std::size_t comma = std::min(list.find(',', at), list.size());
      const std::string id = list.substr(at, comma - at);
      for (std::size_t g = 0; g < s.metals.size(); ++g)
        if (s.metals[g].id == id)
          s.zones[i].cycle.push_back(int(g));
      at = comma + 1;
    }
  }
  mStormCycles.clear();
  for (std::size_t i = 0; i < s.machines.size(); ++i)
  {
    auto& m = s.machines[i];
    if (m.kind == MachineKind::Lamp)
      s.lamp = int(i);
    if (m.kind != MachineKind::Lift)
      continue;
    for (std::size_t j = 0; j < mPlatforms.size(); ++j)
      if (mPlatforms[j].id == m.liftId)
      {
        m.platform = int(j);
        mPlatforms[j].dead = true;
      }
  }
  if (s.bungee)
  {
    const auto& p = mPlayer;
    s.bx = s.by = 0.0f;
    s.flying = false;
    if (s.postX == 0.0f)
    {
      s.postX = float(p.x) + 1.5f;
      s.postY = float(p.y) - 12.0f;
    }
  }
}

// --- Lightning -----------------------------------------------------------------------------

// What a strike on this target touches: the struck metal (a Kite Drone's
// tether too), and the zone's piece in turn.
std::vector<CellBox> World::struckMetal(const StormZone& z, const StrikeTarget& t) const
{
  const auto& s = mStorm;
  std::vector<CellBox> out;
  switch (t.kind)
  {
    case TargetKind::Metal:
      if (std::size_t(t.index) < s.metals.size())
        out = s.metals[std::size_t(t.index)].boxes;
      break;
    case TargetKind::Machine:
      if (std::size_t(t.index) < s.machines.size())
        out.push_back(s.machines[std::size_t(t.index)].box);
      break;
    case TargetKind::Rod:
      if (std::size_t(t.index) < s.rods.size())
      {
        const Rod& r = s.rods[std::size_t(t.index)];
        out.push_back(r.box());
        if (r.machine >= 0)
          out.push_back(s.machines[std::size_t(r.machine)].box);
      }
      break;
    case TargetKind::Enemy:
      if (std::size_t(t.index) < mEnemies.size() && mEnemies[std::size_t(t.index)].alive)
      {
        const Enemy& e = mEnemies[std::size_t(t.index)];
        out.push_back(e.box());
        for (const auto& k : s.kites)
          if (k.enemy == t.index)
          {
            const int ex = e.x + e.w / 2, ey = e.y - e.h / 2;
            const int n = std::max(std::abs(ex - k.ax), std::abs(ey - k.ay));
            for (int i = 0; i <= n; i += 2)
              out.push_back({k.ax + (ex - k.ax) * i / std::max(1, n), k.ay + (ey - k.ay) * i / std::max(1, n), 1, 1});
          }
      }
      break;
    case TargetKind::None:
      break;
  }
  if (t.kind != TargetKind::None && z.extra >= 0 && std::size_t(z.extra) < s.metals.size())
    for (const auto& b : s.metals[std::size_t(z.extra)].boxes)
      out.push_back(b);
  return out;
}

// The glow starts on the zone's highest metal (ties: nearest the runner).
void World::chooseStrike(StormZone& z)
{
  auto& s = mStorm;
  const int px = mPlayer.x + 1;
  StrikeTarget best;
  int bestTop = 0, bestDist = 0;
  auto offer = [&](TargetKind kind, int index, const CellBox& b) {
    if (!b.intersects(z.rect))
      return;
    const int top = std::max(b.y, z.rect.y);
    const int dist = std::abs(b.x + b.w / 2 - px);
    if (best.kind == TargetKind::None || top < bestTop || (top == bestTop && dist < bestDist))
    {
      best = {kind, index};
      bestTop = top;
      bestDist = dist;
    }
  };
  for (std::size_t i = 0; i < s.metals.size(); ++i)
    for (const auto& b : s.metals[i].boxes)
      offer(TargetKind::Metal, int(i), b);
  for (std::size_t i = 0; i < s.machines.size(); ++i)
    offer(TargetKind::Machine, int(i), s.machines[i].box);
  for (const auto& k : s.kites)
    if (std::size_t(k.enemy) < mEnemies.size() && mEnemies[std::size_t(k.enemy)].alive)
      offer(TargetKind::Enemy, k.enemy, mEnemies[std::size_t(k.enemy)].box());
  // A stuck rod takes the strike, whatever is higher.
  for (std::size_t i = 0; i < s.rods.size(); ++i)
    if (z.rect.contains(s.rods[i].x + 1, s.rods[i].y))
      best = {TargetKind::Rod, int(i)};
  z.target = best;
  z.extra = z.cycle.empty() ? -1 : z.cycle[std::size_t(z.cycleAt) % z.cycle.size()];
}

void World::strike(StormZone& z)
{
  auto& s = mStorm;
  const StrikeTarget t = z.target;
  z.live = struckMetal(z, t);
  z.target = {};
  if (t.kind == TargetKind::None || z.live.empty())
  {
    z.live.clear();
    return;
  }
  z.hurt = kStrikeHurt;
  const CellBox& top = z.live.front();
  z.boltX = top.x + top.w / 2;
  z.boltY = top.y;
  if (!z.cycle.empty())
    z.cycleAt = (z.cycleAt + 1) % int(z.cycle.size());
  if (!mSimulation)
    playSound(Sfx::Thunder);
  if (!mSimulation)
    mCamera.shake(6, 1.5f);
  // Enemies touching the struck metal (the target itself included).
  for (std::size_t i = 0; i < mEnemies.size(); ++i)
  {
    Enemy& e = mEnemies[i];
    if (!e.alive || e.hidden)
      continue;
    bool hit = t.kind == TargetKind::Enemy && t.index == int(i);
    for (const auto& b : z.live)
      hit = hit || touches(b, e.box());
    if (hit)
      damageEnemy(e, z.damage);
  }
  switch (t.kind)
  {
    case TargetKind::Machine:
      powerMachine(t.index);
      break;
    case TargetKind::Rod:
    {
      // The rod arcs to every enemy close by, and powers what it is stuck in.
      const Rod r = s.rods[std::size_t(t.index)];
      for (auto& e : mEnemies)
      {
        if (!e.alive || e.hidden)
          continue;
        const int ex = e.x + e.w / 2, ey = e.y - e.h / 2;
        if (std::abs(ex - (r.x + 1)) <= kRodReach && std::abs(ey - r.y) <= kRodReach)
        {
          s.arcs.push_back({r.x + 1, r.y, ex, ey, kArcFrames});
          damageEnemy(e, kRodArc);
        }
      }
      if (r.machine >= 0)
        powerMachine(r.machine);
      removeRod(s, t.index);
      break;
    }
    case TargetKind::Enemy:
      for (auto& k : s.kites)
        if (k.enemy == t.index)
          k.spark = kSparkFrames;
      break;
    default:
      break;
  }
}

void World::powerMachine(int index)
{
  auto& s = mStorm;
  if (std::size_t(index) >= s.machines.size() || s.machines[std::size_t(index)].powered)
    return;
  Machine& m = s.machines[std::size_t(index)];
  m.powered = true;
  switch (m.kind)
  {
    case MachineKind::Lift:
      if (m.platform >= 0)
        mPlatforms[std::size_t(m.platform)].dead = false;
      showMessage("THE LIFT HAS POWER");
      break;
    case MachineKind::Door:
    case MachineKind::Generator:
      openMachine(m);
      showMessage(m.kind == MachineKind::Generator ? "THE GENERATOR HUMS: A HATCH OPENS" : "THE DOOR OPENS");
      break;
    case MachineKind::Lamp:
      s.morse = 0;
      showMessage("THE LAMP IS LIT");
      if (!mSimulation)
        playSound(Sfx::LampOn);
      break;
  }
  if (!mSimulation)
    playSound(Sfx::Clunk);
}

void World::openMachine(const Machine& m)
{
  for (int by = m.oy0; by <= m.oy1; ++by)
    for (int bx = m.ox0; bx <= m.ox1; ++bx)
      mMap.setBlock(bx, by, bx == m.ladderX ? Tile::Ladder : Tile::Empty);
}

// --- The Tesla Rod -------------------------------------------------------------------------

void World::stickRod(Projectile& pr, int machine)
{
  auto& s = mStorm;
  Rod r;
  r.dir = pr.dx != 0 ? pr.dx : 1;
  // Back out of the wall to the last free spot along the shot's way.
  r.x = pr.x;
  r.y = pr.y;
  for (int i = 0; i < 4 && machine < 0 && mMap.overlapsSolid(r.box()); ++i)
  {
    r.x -= pr.dx;
    r.y -= pr.dy;
  }
  r.machine = machine;
  if (int(s.rods.size()) >= kRodsMax)
    removeRod(s, 0);
  s.rods.push_back(r);
  if (!mSimulation)
    playSound(Sfx::BoltThunk);
  // Its zone's next strike comes to the rod, after the full glow.
  for (auto& z : s.zones)
    if (z.rect.contains(r.x + 1, r.y))
    {
      z.target = {TargetKind::Rod, int(s.rods.size()) - 1};
      z.extra = z.cycle.empty() ? -1 : z.cycle[std::size_t(z.cycleAt) % z.cycle.size()];
      z.t = std::max(0, z.period - z.glow);
      break;
    }
}

bool World::shotAtStorm(Projectile& pr, const CellBox& hit)
{
  auto& s = mStorm;
  // Any shot close to the fry gull sends it off with its fry.
  if (s.fryX >= 0 && s.fryFlown == 0 && pr.kind != ShotKind::Enemy &&
      std::abs(hit.x - (s.fryX + 1)) <= 12 && std::abs(hit.y - (s.fryY - 2)) <= 8)
  {
    s.fryFlown = 1;
    if (!mSimulation)
      playSound(Sfx::Flap);
  }
  if (!(pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::TeslaRod)))
    return false;
  for (std::size_t i = 0; i < s.machines.size(); ++i)
    if (s.machines[i].box.intersects(hit))
    {
      stickRod(pr, int(i));
      return true;
    }
  return false;
}

int World::shotAtStormEnemy(Projectile& pr, Enemy& e)
{
  (void)pr;
  if (e.kind == EnemyKind::KiteDrone)
    for (auto& k : mStorm.kites)
      if (k.enemy == int(&e - mEnemies.data()))
        sparkKite(k);
  return 0;
}

// A hit sends a spark down the tether: it hurts what touches the tether.
void World::sparkKite(Kite& k)
{
  if (k.spark > 0)
    return;
  k.spark = kSparkFrames;
  for (auto& e : mEnemies)
    if (e.alive && &e != &mEnemies[std::size_t(k.enemy)] && touchesTether(k, e.box()))
      damageEnemy(e, 2);
}

bool World::touchesTether(const Kite& k, const CellBox& b) const
{
  if (std::size_t(k.enemy) >= mEnemies.size())
    return false;
  const Enemy& e = mEnemies[std::size_t(k.enemy)];
  if (!e.alive)
    return false;
  const int ex = e.x + e.w / 2, ey = e.y - e.h / 2;
  const int n = std::max(1, std::max(std::abs(ex - k.ax), std::abs(ey - k.ay)));
  for (int i = 0; i < n; ++i)
    if (b.contains(k.ax + (ex - k.ax) * i / n, k.ay + (ey - k.ay) * i / n))
      return true;
  return false;
}

// --- Update --------------------------------------------------------------------------------

void World::updateStorm(const PlayerInput& input)
{
  (void)input;
  auto& s = mStorm;
  auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  for (auto& a : s.arcs)
    --a.frames;
  s.arcs.erase(std::remove_if(s.arcs.begin(), s.arcs.end(), [](const Arc& a) { return a.frames <= 0; }), s.arcs.end());
  if (s.fryFlown > 0 && s.fryFlown < 200)
    ++s.fryFlown;
  if (s.morse >= 0)
    ++s.morse;
  if (s.bungee)
  {
    // Storm rings, 200 points each.
    const CellBox pb = p.box();
    for (std::size_t i = 0; i < s.rings.size(); ++i)
      if (!s.ringTaken[i] && CellBox{s.rings[i].first - 2, s.rings[i].second - 2, 4, 4}.intersects(pb))
      {
        s.ringTaken[i] = true;
        mStats.score += kBungeeRing;
        if (!mSimulation)
          playSound(Sfx::Chime);
      }
    return;
  }
  // Rods unstruck fall out.
  for (int i = int(s.rods.size()) - 1; i >= 0; --i)
    if (--s.rods[std::size_t(i)].life <= 0)
      removeRod(s, i);
  // Lightning.
  for (auto& z : s.zones)
  {
    ++z.t;
    if (z.t == z.period - z.glow && z.target.kind == TargetKind::None)
      chooseStrike(z);
    if (z.t >= z.period)
    {
      z.t = 0;
      strike(z);
    }
    if (z.hurt > 0)
    {
      --z.hurt;
      if (alive)
        for (const auto& b : z.live)
          if (touches(b, p.box()))
          {
            hurtPlayer(z.damage);
            break;
          }
      if (z.hurt == 0)
        z.live.clear();
    }
  }
  // Sparks down the tethers.
  for (auto& k : s.kites)
    if (k.spark > 0)
    {
      --k.spark;
      if (alive && touchesTether(k, p.box()))
        hurtPlayer(1);
    }
  // Storm Gulls come in while the runner is in their rect.
  for (auto& g : s.spawns)
  {
    g.enemies.erase(std::remove_if(g.enemies.begin(), g.enemies.end(),
                      [&](int i) { return std::size_t(i) >= mEnemies.size() || !mEnemies[std::size_t(i)].alive; }),
      g.enemies.end());
    if (g.cool > 0)
      --g.cool;
    if (!alive || g.def < 0 || g.cool > 0 || g.spawned >= g.count || int(g.enemies.size()) >= g.max ||
        !g.rect.contains(p.x + 1, p.y))
      continue;
    const int side = g.spawned % 2 == 0 ? 1 : -1;
    const int x = std::clamp(p.x + side * 20, 0, mMap.width() - 5);
    spawnEnemy(g.def, x, std::max(4, p.y - 24));
    Enemy& e = mEnemies.back();
    e.active = true;
    e.dir = -side;
    e.cool = 20;
    g.enemies.push_back(int(mEnemies.size()) - 1);
    ++g.spawned;
    g.cool = kGullEvery;
  }
}

// --- Enemies -------------------------------------------------------------------------------

void World::updateStormGull(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  const int pcx = p.x + 1, pcy = p.y - 2;
  const int cx = e.x + e.w / 2, cy = e.y - 1;
  auto toward = [&](int tx, int ty, int speed) {
    const int dx = std::clamp(tx - cx, -speed, speed), dy = std::clamp(ty - cy, -speed, speed);
    e.x += dx;
    e.y += dy;
    if (dx != 0)
      e.dir = sgn(dx);
    return std::abs(tx - cx) <= speed && std::abs(ty - cy) <= speed;
  };
  // Out of its stretch of cliff, a gull leaves (up and away, no score).
  if (e.dive != 4)
    for (const auto& g : mStorm.spawns)
      if (std::find(g.enemies.begin(), g.enemies.end(), int(&e - mEnemies.data())) != g.enemies.end() &&
          !CellBox{g.rect.x - 16, g.rect.y - 16, g.rect.w + 32, g.rect.h + 32}.contains(pcx, pcy))
        e.dive = 4;
  switch (e.dive)
  {
    case 4: // leaving
      e.y -= kGullDive;
      if (e.y < 0 || !isOnScreen(e.box(), 4))
        e.alive = false;
      break;
    case 0: // wheeling high over the runner
    {
      const int tx = pcx + int(std::lround(10.0f * std::sin(2.0f * kPi * float(e.timer % 60) / 60.0f)));
      const int ty = pcy - kGullWheel;
      toward(tx, ty, e.timer % 2 == 0 ? 1 : 2);
      if (e.cool > 0)
        --e.cool;
      else if (alive && std::abs(cy - ty) <= 3 && std::abs(cx - pcx) <= 12)
      {
        e.dive = 1;
        e.tell = def.tell;
        if (!mSimulation)
          playSound(Sfx::Screech);
      }
      break;
    }
    case 1: // the scream, wings folded
      if (--e.tell <= 0)
      {
        e.dive = 2;
        e.tell = 0;
        e.ox = cx; // where the dive started
        e.oy = cy;
        e.aimX = pcx; // through the runner's spot
        e.aimY = pcy;
      }
      break;
    case 2: // down the V
      if (toward(e.aimX, e.aimY, kGullDive))
        e.dive = 3;
      break;
    case 3: // and up its other side
      if (toward(e.aimX + (e.aimX - e.ox), e.oy, kGullDive))
      {
        e.dive = 0;
        e.cool = def.cooldown;
      }
      break;
    default:
      e.dive = 0;
      break;
  }
}

void World::updateKiteDrone(Enemy& e, const EnemyDef& def)
{
  auto& s = mStorm;
  Kite* k = nullptr;
  for (auto& kk : s.kites)
    if (kk.enemy == int(&e - mEnemies.data()))
      k = &kk;
  if (!k)
    return;
  const auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  const int pcx = p.x + 1, pcy = p.y - 2;
  const int cx = e.x + e.w / 2, cy = e.y - e.h / 2;
  auto toward = [&](int tx, int ty) {
    e.x += sgn(tx - cx);
    e.y += sgn(ty - cy);
    return std::abs(tx - cx) <= 1 && std::abs(ty - cy) <= 1;
  };
  int fx = 0, fy = 0;
  figureEight(k->phase, fx, fy);
  ++k->t;
  if (e.cool > 0)
    --e.cool;
  switch (k->state)
  {
    case 0: // the figure eight
      ++k->phase;
      toward(k->ax + fx, k->ay + fy);
      if (e.cool == 0 && alive && std::abs(pcx - k->ax) <= kKiteReach + 8 && std::abs(pcy - k->ay) <= kKiteReach + 4)
      {
        k->state = 1;
        k->t = 0;
        e.tell = def.tell;
      }
      break;
    case 1: // the rotor's pitch rises
      e.tell = std::max(0, def.tell - k->t);
      if (k->t >= def.tell)
      {
        k->state = 2;
        k->t = 0;
        if (!mSimulation)
          playSound(Sfx::Whine);
      }
      break;
    case 2: // at the runner, as far as the tether lets it
    {
      const int tx = std::clamp(pcx, k->ax - kKiteReach, k->ax + kKiteReach);
      const int ty = std::clamp(pcy, k->ay - kKiteReach, k->ay + kKiteReach);
      toward(tx, ty);
      if (k->t >= kKiteChase)
      {
        k->state = 3;
        k->t = 0;
      }
      break;
    }
    default: // back to the loop
      if (toward(k->ax + fx, k->ay + fy) || k->t > 40)
      {
        k->state = 0;
        e.cool = def.cooldown;
      }
      break;
  }
}

void World::updateFoghorn(Enemy& e, const EnemyDef& def)
{
  auto& s = mStorm;
  auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  const int dir = e.dir < 0 ? -1 : 1;
  const CellBox cone{dir < 0 ? e.x - kFogLength : e.x + e.w, e.y - kFogHeight + 1, kFogLength, kFogHeight};
  if (e.dive > 0)
  {
    // The blast: a cell a frame away from the horn (crouched, every other frame).
    --e.dive;
    if (alive && cone.intersects(p.box()) && p.vehicle < 0)
    {
      const bool crouched = p.stance == Stance::Crouched;
      if (!crouched || ++s.pushTick % 2 == 0)
      {
        mMap.moveHorizontally(p.x, p.y, Player::kWidth, p.height(), dir);
        if (p.state == PlayerState::OnGround && !mMap.onSolidGround(p.box()))
          startFalling();
      }
      // The carrier's blast carries the green salt spray.
      if (e.carrier && p.virus == 0 && p.mercy == 0)
        infect();
    }
    if (e.dive == 0)
      e.cool = def.cooldown;
    return;
  }
  if (e.tell > 0)
  {
    if (--e.tell == 0)
    {
      e.dive = kFogPush;
      if (!mSimulation)
        playSound(Sfx::Honk);
    }
    return;
  }
  if (e.cool > 0)
  {
    --e.cool;
    return;
  }
  const CellBox wake{cone.x - 8, cone.y - 4, cone.w + 16, cone.h + 8};
  if (alive && wake.intersects(p.box()))
  {
    e.tell = def.tell;
    if (!mSimulation)
      playSound(Sfx::Inhale);
  }
}

// --- Bungee --------------------------------------------------------------------------------

// The runner is tied to the post by an elastic rope. On the ground, walking
// away stretches it (up to its max); letting go flings them toward and past
// the post. In the air they swing on it like a pendulum (and can fire).
void World::updateBungeeRide(int mvX, const PlayerInput& in)
{
  auto& s = mStorm;
  auto& p = mPlayer;
  if (p.state == PlayerState::Dying || p.state == PlayerState::Teleporting)
    return;
  const float mx = float(p.x) + 1.5f, my = float(p.y) - 2.0f;
  float dx = s.postX - mx, dy = s.postY - my;
  float dist = std::max(0.01f, std::sqrt(dx * dx + dy * dy));
  if (!s.flying)
  {
    if (!mMap.onSolidGround(p.box()))
    {
      s.flying = true;
      s.vx = s.vy = 0.0f;
    }
    else if (in.jump.triggered)
    {
      s.flying = true; // a hop
      s.vx = 0.0f;
      s.vy = -1.5f;
    }
    else if (mvX != 0)
    {
      p.facing = mvX;
      const float ndx = s.postX - (mx + float(mvX));
      if (std::sqrt(ndx * ndx + dy * dy) <= kBungeeMax)
        mMap.moveHorizontally(p.x, p.y, Player::kWidth, p.height(), mvX);
      s.stretching = true;
      p.state = PlayerState::OnGround;
      setVisual(PlayerVisual::Walking);
      return;
    }
    else if (s.stretching && dist > kBungeeRest + 1.0f)
    {
      // Let go: flung toward the post.
      const float speed = std::min(kBungeeFling, (dist - kBungeeRest) / 4.0f);
      s.vx = dx / dist * speed;
      s.vy = dy / dist * speed;
      s.flying = true;
      if (!mSimulation)
        playSound(Sfx::RubberBoing);
    }
    else
    {
      s.stretching = false;
      p.state = PlayerState::OnGround;
      setVisual(PlayerVisual::Standing);
      return;
    }
    s.stretching = false;
    s.bx = s.by = 0.0f;
  }
  // In the air: gravity, the rope, a little pumping.
  s.vy += kBungeeFall;
  if (dist > kBungeeRest)
  {
    const float a = kBungeePull * (dist - kBungeeRest);
    s.vx += dx / dist * a;
    s.vy += dy / dist * a;
  }
  s.vx += 0.03f * float(mvX);
  s.vx = std::clamp(s.vx * 0.995f, -kBungeeFling, kBungeeFling);
  s.vy = std::clamp(s.vy * 0.995f, -kBungeeFling, kBungeeFling);
  if (mvX != 0)
    p.facing = mvX;
  s.bx += s.vx;
  s.by += s.vy;
  const int stepX = int(s.bx), stepY = int(s.by);
  s.bx -= float(stepX);
  s.by -= float(stepY);
  if (stepX != 0 && mMap.moveHorizontally(p.x, p.y, Player::kWidth, p.height(), stepX) != MoveResult::Completed)
  {
    s.vx = 0.0f;
    s.bx = 0.0f;
  }
  if (stepY != 0 && mMap.moveVertically(p.x, p.y, Player::kWidth, p.height(), stepY) != MoveResult::Completed)
  {
    if (stepY > 0)
    {
      s.flying = false; // landed
      s.vx = s.vy = 0.0f;
      p.state = PlayerState::OnGround;
      setVisual(PlayerVisual::Standing);
      if (!mSimulation)
        playSound(Sfx::Land);
      return;
    }
    s.vy = 0.0f;
    s.by = 0.0f;
  }
  // The rope stretches no further: no speed away from the post.
  dx = s.postX - (float(p.x) + 1.5f);
  dy = s.postY - (float(p.y) - 2.0f);
  dist = std::max(0.01f, std::sqrt(dx * dx + dy * dy));
  if (dist > kBungeeMax)
  {
    const float out = -(s.vx * dx + s.vy * dy) / dist;
    if (out > 0.0f)
    {
      s.vx += dx / dist * out;
      s.vy += dy / dist * out;
    }
  }
  p.state = s.vy < 0.0f ? PlayerState::Jumping : PlayerState::Falling;
  p.frames = 0;
  setVisual(s.vy < 0.0f ? PlayerVisual::Jumping : PlayerVisual::Falling);
}

// --- Saving --------------------------------------------------------------------------------

bool World::stormCanSave() const
{
  const auto& s = mStorm;
  for (const auto& z : s.zones)
    if (z.hurt > 0 || z.glowing())
      return false;
  for (const auto& g : s.spawns)
    if (!g.enemies.empty())
      return false;
  return s.rods.empty() && !s.flying;
}

std::vector<int> World::stormSave() const
{
  const auto& s = mStorm;
  std::vector<int> v{s.fryFlown > 0, s.morse >= 0, int(s.machines.size()), int(s.spawns.size())};
  for (const auto& m : s.machines)
    v.push_back(m.powered);
  for (const auto& g : s.spawns)
    v.push_back(g.spawned);
  for (const auto& z : s.zones)
    v.push_back(z.cycleAt);
  return v;
}

bool World::validStormSave(const std::vector<int>& v) const
{
  const auto& s = mStorm;
  return v.size() == 4 + s.machines.size() + s.spawns.size() + s.zones.size() && v[2] == int(s.machines.size()) &&
    v[3] == int(s.spawns.size());
}

void World::loadStorm(const std::vector<int>& v)
{
  auto& s = mStorm;
  s.fryFlown = v[0] != 0 ? 200 : 0;
  s.morse = v[1] != 0 ? 0 : -1;
  std::size_t at = 4;
  for (std::size_t i = 0; i < s.machines.size(); ++i)
  {
    auto& m = s.machines[i];
    m.powered = v[at++] != 0;
    if (!m.powered)
      continue;
    if (m.kind == MachineKind::Lift && m.platform >= 0)
      mPlatforms[std::size_t(m.platform)].dead = false;
    if (m.kind == MachineKind::Door || m.kind == MachineKind::Generator)
      openMachine(m);
  }
  for (auto& g : s.spawns)
    g.spawned = v[at++];
  for (auto& z : s.zones)
  {
    z.cycleAt = v[at++];
    if (z.cycle.empty() || z.cycleAt < 0 || z.cycleAt >= int(z.cycle.size()))
      z.cycleAt = 0;
  }
}

} // namespace gr
