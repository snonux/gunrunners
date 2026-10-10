// Level 21, ZERO (SPEC 21): Live Rewiring. The level script moves the
// bulkheads between named states (`@ shift`), each shown on the monitors 45
// frames before it happens; a bulkhead never shuts on the runner or within
// two blocks of them. The Phase Rifle's shots go on through one wall (or
// one rack, door or bulkhead) and make what is hidden glint. Lattice
// Turrets ride their ceiling rails above the runner and fire straight down,
// Repair Swarms build back what was broken, Echoes replay the runner's own
// moves and shots from 75 frames ago. ZERO itself is the red eye in a ring
// of servers, fought in three phases; then the wall falls. The Wireframe
// bonus (rules=wireframe) turns every `#` into a passable wireframe and
// makes `@ hitbox rect=` the only solid ground.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace gr
{

namespace
{

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

} // namespace

// --- Setup ---------------------------------------------------------------------------------

bool World::setupZeroEntity(const EntityDef& e)
{
  auto& z = mZero;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  const bool servers = mLevel->themeKey == "station_servers";
  if (e.kind == "shift")
  {
    ZeroShift s;
    s.id = e.id;
    const std::string trig = e.str("trigger");
    if (trig.rfind("x:", 0) == 0)
      s.triggerX = std::atoi(trig.c_str() + 2) * kCellsPerTile;
    else if (trig.rfind("switch:", 0) == 0)
      s.triggerSwitch = trig.substr(7);
    s.openIds = names(e.str("open"));
    s.closeIds = names(e.str("close"));
    s.slide = std::max(1, e.num("slide", kShiftSlide));
    z.shifts.push_back(s);
    z.on = true;
    return true;
  }
  if (e.kind == "monitor" && e.hasPos)
  {
    z.monitors.push_back({e.x, e.y});
    z.on = true;
    return true;
  }
  if (e.kind == "lasergrid" && e.rect("rect", x0, y0, x1, y1))
  {
    ZeroGrid g;
    g.x0 = x0;
    g.y0 = y0;
    g.x1 = x1;
    g.y1 = y1;
    g.dmg = std::max(1, e.num("dmg", 1));
    g.every = std::max(2, e.num("every", 6));
    z.grids.push_back(g);
    z.on = true;
    return true;
  }
  if (e.kind == "switch" && e.hasPos && e.str("kind") == "shootable" && servers)
  {
    z.switches.push_back({e.id, e.x, e.y, false});
    z.on = true;
    return true;
  }
  if (e.kind == "echopad" && e.hasPos)
  {
    EchoPad p;
    p.id = e.id;
    p.x = e.x;
    p.y = e.y;
    p.wake = e.str("wake");
    p.armed = p.wake.empty();
    z.pads.push_back(p);
    z.on = true;
    return true;
  }
  if (e.kind == "terminal" && e.hasPos)
  {
    ZeroTerminal t;
    t.x = e.x;
    t.y = e.y;
    t.text = e.str("text");
    t.code = e.num("code", 0) != 0;
    z.terminals.push_back(t);
    z.on = true;
    return true;
  }
  if (e.kind == "hitbox" && e.rect("rect", x0, y0, x1, y1))
  {
    z.hitboxes.push_back({x0, y0, x1, y1});
    z.on = true;
    return true;
  }
  if (servers && (e.kind == "fakewall" || e.kind == "deco") && e.rect("rect", x0, y0, x1, y1))
  {
    ZeroDeco d;
    d.kind = e.kind == "fakewall" ? "fakewall" : e.str("kind");
    d.text = e.str("text");
    d.x0 = x0;
    d.y0 = y0;
    d.x1 = x1;
    d.y1 = y1;
    z.decos.push_back(d);
    z.on = true;
    return true;
  }
  if (e.kind == "boss" && e.id == "ZERO" && e.hasPos)
  {
    auto& b = z.boss;
    b.on = true;
    b.x = e.x;
    b.y = e.y;
    b.w = std::max(1, e.num("w", 6));
    b.h = std::max(1, e.num("h", 6));
    b.cx = float(b.x * kCellsPerTile) + float(b.w * kCellsPerTile) * 0.5f;
    b.cy = float(b.y * kCellsPerTile) + float(b.h * kCellsPerTile) * 0.5f;
    if (e.rect("arena", x0, y0, x1, y1))
    {
      b.ax0 = x0;
      b.ay0 = y0;
      b.ax1 = x1;
      b.ay1 = y1;
      b.floorRow = y1 + 1;
    }
    if (e.rect("door", x0, y0, x1, y1))
    {
      b.doorX0 = x0;
      b.doorY0 = y0;
      b.doorX1 = x1;
      b.doorY1 = y1;
    }
    if (e.rect("wall", x0, y0, x1, y1))
    {
      b.wallX0 = x0;
      b.wallY0 = y0;
      b.wallX1 = x1;
      b.wallY1 = y1;
    }
    b.turretIds = names(e.str("turrets"));
    b.hp = 30;
    z.on = true;
    return true;
  }
  return false;
}

void World::setupZeroEnemy(Enemy& en, const EntityDef& e)
{
  auto& z = mZero;
  const std::size_t i = std::size_t(&en - mEnemies.data());
  if (z.names.size() <= i)
    z.names.resize(i + 1);
  z.names[i] = e.id;
  if (en.kind == EnemyKind::LatticeTurret)
  {
    // It hangs from the rail's row: its top on the block's top.
    const auto rail = e.list("rail");
    const int r0 = rail.size() == 2 ? rail[0] : e.x - 4, r1 = rail.size() == 2 ? rail[1] : e.x + 4;
    en.railX0 = r0 * kCellsPerTile;
    en.railX1 = std::max(en.railX0, (r1 + 1) * kCellsPerTile - en.w);
    en.y = en.prevY = e.y * kCellsPerTile + en.h - 1;
    en.aimX = en.x; // where it was put (rebuilt there)
    en.aimY = en.y;
    z.on = true;
  }
  if (en.kind == EnemyKind::RepairSwarm)
  {
    en.aimX = en.x; // home
    en.aimY = en.y;
    z.on = true;
  }
}

void World::linkZero()
{
  auto& z = mZero;
  if (!z.on)
    return;
  // Bulkheads and floor segments: the script layers drawn by world_zero_draw.cpp.
  for (std::size_t i = 0; i < mLayers.size(); ++i)
  {
    const Layer& l = mLayers[i];
    if (l.style != 5)
      continue;
    ZeroDoor d;
    d.layer = int(i);
    d.pos = d.from = d.to = l.scriptSolid ? 1.0f : 0.0f;
    d.segment = l.id.rfind("SEG", 0) == 0;
    // Bulkheads hanging from the corridor's ceiling slide down; the rest
    // come up out of the floor.
    d.fromCeiling = !d.segment && mMap.block(l.x0, l.y0 - 1) == Tile::Solid;
    z.doors.push_back(d);
  }
  auto layerIndex = [&](const std::string& id) {
    for (std::size_t i = 0; i < mLayers.size(); ++i)
      if (mLayers[i].id == id)
        return int(i);
    return -1;
  };
  for (auto& s : z.shifts)
  {
    for (const auto& id : s.openIds)
      if (const int li = layerIndex(id); li >= 0)
        s.open.push_back(li);
    for (const auto& id : s.closeIds)
      if (const int li = layerIndex(id); li >= 0)
        s.close.push_back(li);
  }
  // Doors that come back (`rebuild=1`).
  for (const auto& e : mLevel->entities)
    if (e.kind == "breakable" && e.num("rebuild", 0) != 0)
    {
      int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
      if (!e.rect("rect", x0, y0, x1, y1))
        continue;
      for (std::size_t i = 0; i < mBreakables.size(); ++i)
        if (mBreakables[i].x0 == x0 && mBreakables[i].y0 == y0 && mBreakables[i].x1 == x1 && mBreakables[i].y1 == y1)
        {
          z.rebuild.push_back(int(i));
          z.rebuildHp.push_back(mBreakables[i].hp);
        }
    }
  // A grating you stomp through hides a pocket: a phase shot over it glints.
  for (const auto& br : mBreakables)
    if (br.by == 7)
    {
      ZeroDeco d;
      d.kind = "pocket";
      d.x0 = br.x0;
      d.y0 = br.y0;
      d.x1 = br.x1;
      d.y1 = br.y1;
      z.decos.push_back(d);
    }
  for (auto& p : z.pads)
    if (!p.wake.empty())
      for (const auto& s : z.switches)
        if (s.id == p.wake && s.hit)
          p.armed = true;
  auto& b = z.boss;
  for (const auto& id : b.turretIds)
    for (std::size_t i = 0; i < z.names.size(); ++i)
      if (z.names[i] == id)
        b.turrets.push_back(int(i));
  // The Wireframe bonus: every solid block inside the frame turns into a
  // wireframe you can pass through; the hitboxes are the solid ground.
  if (z.wireframe)
  {
    for (int ty = 1; ty < mMap.height() / kCellsPerTile - 1; ++ty)
      for (int tx = 1; tx < mMap.width() / kCellsPerTile - 1; ++tx)
        if (mMap.block(tx, ty) == Tile::Solid)
        {
          z.wires.emplace_back(tx, ty);
          mMap.setBlock(tx, ty, Tile::Empty);
        }
    for (const auto& h : z.hitboxes)
      for (int ty = h[1]; ty <= h[3]; ++ty)
        for (int tx = h[0]; tx <= h[2]; ++tx)
          mMap.setBlock(tx, ty, Tile::Solid);
  }
}

// --- Per frame -------------------------------------------------------------------------------

namespace
{

constexpr float kRackRadius = 9.0f;   // cells: the racks' ring around the eye
constexpr float kRackHalf = 2.0f;     // cells: half a rack's depth across the ring
constexpr float kOrbitSpeed = 0.25f;  // cells a frame along the ring
constexpr int kRackSlots = 10;        // eight racks and two gaps (slots 0 and 5)
constexpr int kFanCycle = 40;         // frames between the eye's fans
constexpr int kIrisTell = 12;
constexpr int kOpenFrames = 150;      // the shutter open after a wave of Echoes
constexpr int kAimedEvery = 30;
constexpr int kRetract = 30;          // frames the racks take to pull back
constexpr int kWaveDelay = 20;        // frames between the shutter closing and the next wave
constexpr int kPadEchoLife = 450;     // frames a pad's Echo lasts
constexpr int kThingLayer = 1000000, kThingBreakable = 2000000, kThingRack = 3000000;
constexpr float kPi = 3.14159265f;

CellBox layerBox(const Layer& l)
{
  return {l.x0 * kCellsPerTile, l.y0 * kCellsPerTile, (l.x1 - l.x0 + 1) * kCellsPerTile,
    (l.y1 - l.y0 + 1) * kCellsPerTile};
}

CellBox grow(const CellBox& b, int by) { return {b.x - by, b.y - by, b.w + 2 * by, b.h + 2 * by}; }

// The rack slot at this point (cells), -1 if it is not on the ring or in a gap.
int rackSlot(const ZeroBoss& b, float sx, float sy)
{
  if (b.racks <= 0.05f)
    return -1;
  const float dx = sx - b.cx, dy = sy - b.cy;
  const float r = std::sqrt(dx * dx + dy * dy);
  if (std::abs(r - kRackRadius * b.racks) > kRackHalf)
    return -1;
  float a = std::atan2(dy, dx) - b.orbit;
  a = std::fmod(a, 2.0f * kPi);
  if (a < 0.0f)
    a += 2.0f * kPi;
  const int slot = std::min(kRackSlots - 1, int(a / (2.0f * kPi / float(kRackSlots))));
  return slot == 0 || slot == 5 ? -1 : slot;
}

} // namespace

bool ZeroBoss::racksBlock(float sx, float sy) const { return rackSlot(*this, sx, sy) >= 0; }

bool ZeroBoss::eyeOpen() const
{
  switch (phase)
  {
    case ZeroPhase::Racks: return true; // through the gaps
    case ZeroPhase::Echoes: return !shutter && open > 0;
    case ZeroPhase::Overload: return cycle >= 60 && cycle < 82;
    default: return false;
  }
}

int ZeroBoss::total() const
{
  if (!on)
    return 0;
  switch (phase)
  {
    case ZeroPhase::Idle: return 90;
    case ZeroPhase::Racks: return hp + 60;
    case ZeroPhase::Retract: return 60;
    case ZeroPhase::Echoes: return hp + 30;
    case ZeroPhase::Overload: return hp;
    default: return 0;
  }
}

bool World::zeroFight() const
{
  const auto& b = mZero.boss;
  return b.on && !b.away &&
    (b.phase == ZeroPhase::Racks || b.phase == ZeroPhase::Retract || b.phase == ZeroPhase::Echoes ||
      b.phase == ZeroPhase::Overload);
}

int World::zeroHp() const { return mZero.boss.total(); }

bool World::overSafeSegment() const
{
  const auto& b = mZero.boss;
  if (!b.on || b.phase != ZeroPhase::Overload)
    return true;
  const int k = (mPlayer.x + 1) / kCellsPerTile - b.ax0;
  return k < 0 || k / 3 >= kZeroSegments || (b.safe >> (k / 3)) & 1u;
}

// --- The level script ---------------------------------------------------------------------------

void World::startShift(ZeroShift& s)
{
  auto& z = mZero;
  s.t = 0;
  z.preview = int(&s - z.shifts.data());
  z.previewT = 0;
  playSound(Sfx::Monitor);
}

void World::slideDoor(ZeroDoor& d, float to, int frames)
{
  d.from = d.pos;
  d.to = to;
  d.slide = std::max(1, frames);
  d.t = 0;
}

void World::updateShifts()
{
  auto& z = mZero;
  const auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  for (auto& s : z.shifts)
  {
    if (s.done)
      continue;
    if (s.t < 0)
    {
      bool go = alive && s.triggerX >= 0 && p.x >= s.triggerX;
      for (const auto& sw : z.switches)
        go = go || (!s.triggerSwitch.empty() && sw.id == s.triggerSwitch && sw.hit);
      if (go)
        startShift(s);
      continue;
    }
    ++s.t;
    if (s.t == kShiftPreview)
    {
      for (auto& d : z.doors)
      {
        if (std::find(s.open.begin(), s.open.end(), d.layer) != s.open.end())
          slideDoor(d, 0.0f, s.slide);
        if (std::find(s.close.begin(), s.close.end(), d.layer) != s.close.end())
          slideDoor(d, 1.0f, s.slide);
      }
      playSound(Sfx::Bulkhead);
    }
    if (s.t >= kShiftPreview + s.slide)
    {
      s.done = true;
      ++z.state;
      if (z.preview == int(&s - z.shifts.data()))
        z.preview = -1;
    }
  }
  if (z.preview >= 0)
    ++z.previewT;
}

void World::updateDoors()
{
  auto& z = mZero;
  const CellBox pbox = mPlayer.box();
  const bool alive = mPlayer.state != PlayerState::Dying;
  for (auto& d : z.doors)
  {
    Layer& l = mLayers[std::size_t(d.layer)];
    if (d.shake > 0)
      --d.shake;
    const bool closing = d.to > d.from;
    if (d.t < d.slide)
    {
      // A bulkhead holds where it is while the runner is within two blocks
      // of it; a floor segment only while the runner is in its way.
      const CellBox area = layerBox(l);
      const bool inWay = alive && (d.segment ? area.intersects(pbox) : grow(area, 4).intersects(pbox));
      if (!(closing && inWay))
      {
        ++d.t;
        d.pos = d.t >= d.slide ? d.to : d.from + (d.to - d.from) * float(d.t) / float(d.slide);
      }
    }
    // Shut: solid once all the way down. Opening: open once half way up.
    l.scriptSolid = closing ? d.pos >= 0.999f : d.pos > 0.5f;
  }
}

// --- The Phase Rifle -------------------------------------------------------------------------------

// What a solid cell belongs to, so a shot passes exactly one of them: a
// bulkhead, a door or a block.
int World::phaseThing(int cx, int cy) const
{
  const int tx = cx / kCellsPerTile, ty = cy / kCellsPerTile;
  for (std::size_t i = 0; i < mLayers.size(); ++i)
  {
    const Layer& l = mLayers[i];
    if (l.solid && tx >= l.x0 && tx <= l.x1 && ty >= l.y0 && ty <= l.y1)
      return kThingLayer + int(i);
  }
  for (std::size_t i = 0; i < mBreakables.size(); ++i)
  {
    const Breakable& b = mBreakables[i];
    if (!b.broken && tx >= b.x0 && tx <= b.x1 && ty >= b.y0 && ty <= b.y1)
      return kThingBreakable + int(i);
  }
  return ty * (mMap.width() / kCellsPerTile) + tx;
}

bool World::phaseThrough(Projectile& pr, const CellBox& b)
{
  if (pr.kind != ShotKind::Proto || pr.proto != int(ProtoId::PhaseRifle) || pr.phase == -2)
    return false;
  int thing = -1;
  for (int cy = b.y; cy < b.y + b.h; ++cy)
    for (int cx = b.x; cx < b.x + b.w; ++cx)
    {
      if (!mMap.solid(cx, cy))
        continue;
      const int t = phaseThing(cx, cy);
      if (thing >= 0 && t != thing)
        return false; // two walls at once
      thing = t;
    }
  if (thing < 0)
    return false;
  if (pr.phase >= 0 && pr.phase != thing)
    return false; // the second wall stops it
  if (pr.phase < 0)
  {
    pr.phase = thing;
    // A door it passes still takes the hit.
    if (thing >= kThingBreakable && thing < kThingRack)
      hitBreakable(b, pr.damage, 0);
  }
  return true;
}

void World::phaseGlint(const CellBox& b)
{
  for (auto& d : mZero.decos)
  {
    if (d.kind != "fakewall" && d.kind != "pocket")
      continue;
    // A pocket under the floor glints for a shot passing over it.
    const int up = d.kind == "pocket" ? 6 : 0;
    const CellBox area{d.x0 * kCellsPerTile, d.y0 * kCellsPerTile - up, (d.x1 - d.x0 + 1) * kCellsPerTile,
      (d.y1 - d.y0 + 1) * kCellsPerTile + up};
    if (!area.intersects(b))
      continue;
    if (d.glint == 0)
      playSound(Sfx::Glint);
    d.glint = kGlintFrames;
  }
}

bool World::shotAtZero(Projectile& pr, const CellBox& b)
{
  auto& z = mZero;
  const bool phase = pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::PhaseRifle);
  if (phase)
    phaseGlint(b);
  for (auto& sw : z.switches)
  {
    const CellBox box{sw.x * kCellsPerTile, sw.y * kCellsPerTile, kCellsPerTile, 2 * kCellsPerTile};
    if (sw.hit || !box.intersects(b))
      continue;
    sw.hit = true;
    for (auto& pad : z.pads)
      if (pad.wake == sw.id)
        pad.armed = true;
    const Vec2 c = cellCenter(box);
    burst(c, rgb(255, 255, 255), rgb(255, 60, 60), 14, 1.8f);
    flashAt(c, 90.0f, rgb(255, 80, 80), 16);
    playSound(Sfx::ForceFieldOff);
    showMessage("KILL SWITCH " + sw.id + " - REWIRING");
    return true;
  }
  auto& bo = z.boss;
  if (!bo.on || bo.away || !(bo.phase == ZeroPhase::Racks || bo.phase == ZeroPhase::Retract ||
                               bo.phase == ZeroPhase::Echoes || bo.phase == ZeroPhase::Overload))
    return false;
  const float sx = float(b.x) + float(b.w) * 0.5f, sy = float(b.y) + float(b.h) * 0.5f;
  const int slot = rackSlot(bo, sx, sy);
  if (slot >= 0)
  {
    // A rack: the Phase Rifle goes through one.
    if (phase && (pr.phase == -1 || pr.phase == kThingRack + slot))
      pr.phase = kThingRack + slot;
    else
    {
      bo.glance = 4;
      burst(cellCenter(b), rgb(255, 230, 230), rgb(200, 40, 40), 4, 1.0f);
      playSound(Sfx::Land);
      return true;
    }
  }
  const CellBox eye{bo.x * kCellsPerTile, bo.y * kCellsPerTile, bo.w * kCellsPerTile, bo.h * kCellsPerTile};
  if (!eye.intersects(b))
    return false;
  if (bo.eyeOpen())
    hurtZero(pr.damage, b);
  else
  {
    bo.glance = 4;
    burst(cellCenter(b), rgb(255, 255, 255), rgb(150, 150, 170), 4, 1.0f);
    playSound(Sfx::Land);
  }
  return true;
}

// --- Echoes ------------------------------------------------------------------------------------

void World::recordEcho()
{
  auto& z = mZero;
  const auto& p = mPlayer;
  EchoFrame f;
  f.x = int16_t(p.x);
  f.y = int16_t(p.y);
  f.facing = int8_t(p.facing);
  f.visual = uint8_t(p.visual);
  if (z.fired)
  {
    f.shotDx = z.firedDx;
    f.shotDy = z.firedDy;
    f.shotOx = z.firedOx;
    f.shotOy = z.firedOy;
  }
  z.fired = false;
  z.history[std::size_t(z.historyAt)] = f;
  z.historyAt = (z.historyAt + 1) % kEchoHistory;
  z.historyLen = std::min(kEchoHistory, z.historyLen + 1);
}

int World::spawnEcho(int x, int y, int delay, int look, int hp)
{
  spawnEnemy(enemyIndex("echo"), x, y);
  Enemy& e = mEnemies.back();
  e.attach = delay;
  e.aimX = look;
  e.hp = hp;
  e.active = true;
  e.timer = 0;
  e.dir = mZero.ago(delay).facing;
  burst({(float(x) + 1.5f) * kCellSize, (float(y) - 2.0f) * kCellSize}, rgb(120, 255, 255), rgb(255, 255, 255), 14,
    1.6f);
  playSound(Sfx::EchoIn);
  return int(mEnemies.size()) - 1;
}

void World::updateEcho(Enemy& e, const EnemyDef& /*def*/)
{
  const auto& z = mZero;
  // Boss Echoes (aimX 0-2) last until shot; a pad's fades after a while.
  if (e.aimX < 0 && e.timer > kPadEchoLife)
  {
    e.alive = false;
    burst(cellCenter(e.box()), rgb(120, 255, 255), rgb(40, 120, 160), 12, 1.4f);
    return;
  }
  const EchoFrame& f = z.ago(e.attach);
  e.x = f.x;
  e.y = f.y;
  e.dir = f.facing;
  if (f.shotDx != 0 || f.shotDy != 0)
  {
    spawnProjectile(ShotKind::Enemy, f.x + f.shotOx, f.y + f.shotOy, f.shotDx, f.shotDy);
    Projectile& pr = mProjectiles.back();
    pr.speed = 2;
    pr.echo = true;
  }
}

// --- Lattice Turrets and Repair Swarms -------------------------------------------------------------

void World::updateLatticeTurret(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  // Ride the rail to stay over the runner; a turret sharing a rail keeps
  // out of the other's way.
  int target = std::clamp(p.x + 1 - e.w / 2, e.railX0, e.railX1);
  if (e.timer % std::max(1, def.stepEvery) == 0 && target != e.x)
  {
    const int nx = e.x + (target > e.x ? 1 : -1);
    bool clear = true;
    for (const auto& o : mEnemies)
      if (&o != &e && o.alive && o.kind == EnemyKind::LatticeTurret && o.y == e.y && std::abs(o.x - nx) < e.w + 1 &&
          std::abs(o.x - nx) < std::abs(o.x - e.x))
        clear = false;
    if (clear)
      e.x = nx;
  }
  if (e.dive > 0)
    --e.dive; // cooling down
  if (e.tell > 0)
  {
    if (--e.tell == 0)
    {
      spawnProjectile(ShotKind::Enemy, e.x + e.w / 2 - 1, e.y + 1, 0, 1);
      mProjectiles.back().speed = 1;
      e.dive = def.cooldown;
      playSound(Sfx::ProbeShot);
    }
    return;
  }
  const int over = (p.x + 1) - (e.x + e.w / 2);
  if (e.dive == 0 && std::abs(over) <= 3 && p.y > e.y && p.state != PlayerState::Dying)
    e.tell = def.tell;
}

void World::updateRepairSwarm(Enemy& e, const EnemyDef& def)
{
  const auto& z = mZero;
  // Hover to the nearest wreck in its patch (24 blocks round home), else home.
  int gx = e.aimX, gy = e.aimY;
  int best = 1 << 30;
  for (const auto& w : z.wrecks)
  {
    if (std::abs(w.x - e.aimX) > 48)
      continue;
    const int d = std::abs(w.x - (e.x + 1)) + std::abs(w.y - (e.y - 1));
    if (d < best)
    {
      best = d;
      gx = w.x - 1;
      gy = w.y + 1;
    }
  }
  if (e.timer % std::max(1, def.stepEvery) != 0)
    return;
  if (gx != e.x)
    e.x += gx > e.x ? 1 : -1;
  if (gy != e.y)
    e.y += gy > e.y ? 1 : -1;
  e.dir = gx >= e.x ? 1 : -1;
}

// --- ZERO ------------------------------------------------------------------------------------------

void World::shutZeroDoor(bool shut)
{
  const auto& b = mZero.boss;
  if (b.doorX0 < 0)
    return;
  for (int ty = b.doorY0; ty <= b.doorY1; ++ty)
    for (int tx = b.doorX0; tx <= b.doorX1; ++tx)
      mMap.setBlock(tx, ty, shut ? Tile::Solid : Tile::Empty);
  if (shut)
  {
    playSound(Sfx::Bulkhead);
    mCamera.shake(8, 2.0f);
  }
}

void World::pickSafeSegments()
{
  auto& b = mZero.boss;
  b.prevSafe = b.safe;
  uint16_t prev = b.prevSafe;
  if (b.cycles == 0)
  {
    // The first cycle: near where the runner stands, and the Turbo's segment.
    const int k = std::clamp(((mPlayer.x + 1) / kCellsPerTile - b.ax0) / 3, 0, kZeroSegments - 1);
    prev = uint16_t((1u << k) | (1u << 4));
  }
  auto near = [](uint16_t set, int k) {
    for (int j = std::max(0, k - 2); j <= std::min(kZeroSegments - 1, k + 2); ++j)
      if ((set >> j) & 1u)
        return true;
    return false;
  };
  for (int tries = 0; tries < 400; ++tries)
  {
    b.seed = b.seed * 1103515245u + 12345u;
    uint16_t pick = 0;
    uint32_t r = b.seed >> 4;
    int n = 0;
    for (int guard = 0; n < 4 && guard < 64; ++guard)
    {
      const int k = int(r % uint32_t(kZeroSegments));
      r = r / 7u + uint32_t(guard) * 2654435761u;
      if ((pick >> k) & 1u)
        continue;
      pick = uint16_t(pick | (1u << k));
      ++n;
    }
    if (n < 4)
      continue;
    if (b.cycles == 0 && !((pick >> 4) & 1u))
      continue; // the Turbo lands on segment 5
    bool ok = true;
    for (int k = 0; k < kZeroSegments && ok; ++k)
    {
      if (((pick >> k) & 1u) && !near(prev, k))
        ok = false; // every new safe segment within 6 blocks of an old one
      if (((prev >> k) & 1u) && !near(pick, k))
        ok = false; // and every old one within 6 blocks of a new one
    }
    if (ok)
    {
      b.safe = pick;
      return;
    }
  }
  b.safe = prev; // (never happens with four of twelve)
}

void World::zeroPhase(ZeroPhase phase)
{
  auto& z = mZero;
  auto& b = z.boss;
  b.phase = phase;
  b.t = 0;
  b.iris = 0;
  b.fan = 0;
  const int floorY = b.floorRow * kCellsPerTile - 1;
  auto drop = [&](ItemKind kind, int bx) {
    Item it;
    it.kind = kind;
    it.x = it.prevX = bx * kCellsPerTile;
    it.y = it.prevY = floorY - 10;
    it.pickupDelay = 4;
    mItems.push_back(it);
  };
  switch (phase)
  {
    case ZeroPhase::Racks:
      b.hp = 30;
      b.racks = 1.0f;
      playSound(Sfx::Rumble);
      showMessage("ZERO - SHOOT THE EYE THROUGH THE GAPS");
      break;
    case ZeroPhase::Retract:
      drop(ItemKind::Health, 160);
      playSound(Sfx::Bulkhead);
      showMessage("THE RACKS PULL BACK");
      break;
    case ZeroPhase::Echoes:
      b.hp = 30;
      b.shutter = true;
      b.open = 0;
      b.wave = 0;
      b.echoes = {-1, -1, -1};
      playSound(Sfx::Shutter);
      showMessage("ZERO SHUTS ITS EYE - SHOOT YOUR ECHOES");
      break;
    case ZeroPhase::Overload:
      b.hp = 30;
      b.cycle = 0;
      b.cycles = 0;
      b.shutter = false;
      for (int i : b.echoes)
        if (i >= 0 && mEnemies[std::size_t(i)].alive)
          mEnemies[std::size_t(i)].alive = false;
      b.echoes = {-1, -1, -1};
      drop(ItemKind::Health, 178);
      drop(ItemKind::Turbo, b.ax0 + 13); // onto segment 5
      pickSafeSegments();
      mCamera.shake(14, 2.5f);
      playSound(Sfx::Rumble);
      showMessage("OVERLOAD - STAND ON THE LIT SEGMENTS");
      break;
    case ZeroPhase::Reveal:
    {
      b.reveal = 0;
      b.lights = 0;
      for (auto& d : z.doors)
        if (d.segment && d.pos < 1.0f)
          slideDoor(d, 1.0f, 6);
      for (auto& e : mEnemies)
        if (e.alive && (e.kind == EnemyKind::Echo || std::find(b.turrets.begin(), b.turrets.end(),
                                                         int(&e - mEnemies.data())) != b.turrets.end()))
        {
          e.alive = false;
          burst(cellCenter(e.box()), rgb(255, 120, 120), rgb(255, 255, 255), 12, 1.6f);
        }
      const Vec2 c{b.cx * kCellSize, b.cy * kCellSize};
      addScore(50000, c);
      flashAt(c, 300.0f, rgb(255, 60, 60), 40);
      playSound(Sfx::LettersComplete);
      showMessage("ZERO: THANK YOU. I HATED THAT SCRIPT.");
      break;
    }
    case ZeroPhase::Done:
      b.exitOpen = true;
      break;
    default:
      break;
  }
}

void World::hurtZero(int damage, const CellBox& at)
{
  auto& b = mZero.boss;
  b.hp -= std::max(1, damage);
  b.flash = 6;
  burst(cellCenter(at), rgb(255, 80, 80), rgb(255, 255, 255), 8, 1.4f);
  playSound(Sfx::Hit);
  if (b.hp > 0)
    return;
  b.hp = 0;
  switch (b.phase)
  {
    case ZeroPhase::Racks: zeroPhase(ZeroPhase::Retract); break;
    case ZeroPhase::Echoes: zeroPhase(ZeroPhase::Overload); break;
    case ZeroPhase::Overload: zeroPhase(ZeroPhase::Reveal); break;
    default: break;
  }
}

void World::updateZeroBoss()
{
  auto& z = mZero;
  auto& b = z.boss;
  const auto& p = mPlayer;
  if (b.flash > 0)
    --b.flash;
  if (b.glance > 0)
    --b.glance;
  if (b.beam > 0)
    --b.beam;
  const CellBox arena{b.ax0 * kCellsPerTile, b.ay0 * kCellsPerTile, (b.ax1 - b.ax0 + 1) * kCellsPerTile,
    (b.ay1 - b.ay0 + 2) * kCellsPerTile};
  const CellBox pb = p.box();
  const bool inArena = p.state == PlayerState::OnGround && pb.x >= arena.x && pb.x + pb.w <= arena.x + arena.w &&
    pb.y >= arena.y && pb.y + pb.h <= arena.y + arena.h;
  if (b.phase == ZeroPhase::Idle)
  {
    if (inArena)
    {
      shutZeroDoor(true);
      zeroPhase(ZeroPhase::Racks);
    }
    return;
  }
  if (b.away)
  {
    if (!inArena)
      return;
    b.away = false; // back in: the door shuts again and it goes on
    shutZeroDoor(true);
  }
  ++b.t;
  if (b.iris > 0)
    --b.iris;
  const float ex = b.cx, ey = float((b.y + b.h) * kCellsPerTile); // the eye's underside, where shots come from
  auto shoot = [&](float ang) {
    spawnProjectile(ShotKind::Enemy, int(ex), int(ey), 0, 1);
    Projectile& pr = mProjectiles.back();
    pr.precise = true;
    pr.fx = ex;
    pr.fy = ey;
    pr.vx = std::cos(ang);
    pr.vy = std::sin(ang);
    pr.speed = 1;
  };
  const float aim = std::atan2(float(p.y - 2) - ey, float(p.x) + 1.5f - ex);
  switch (b.phase)
  {
    case ZeroPhase::Racks:
      b.orbit = std::fmod(b.orbit + kOrbitSpeed / kRackRadius, 2.0f * kPi);
      if (++b.fan == kFanCycle - kIrisTell)
        b.iris = kIrisTell;
      if (b.fan >= kFanCycle)
      {
        b.fan = 0;
        for (int k = -1; k <= 1; ++k)
          shoot(aim + float(k) * 0.3f);
        playSound(Sfx::ProbeShot);
      }
      break;
    case ZeroPhase::Retract:
      b.orbit = std::fmod(b.orbit + kOrbitSpeed / kRackRadius, 2.0f * kPi);
      b.racks = std::max(0.0f, 1.0f - float(b.t) / float(kRetract));
      if (b.t >= kRetract)
        zeroPhase(ZeroPhase::Echoes);
      break;
    case ZeroPhase::Echoes:
    {
      if (b.shutter)
      {
        bool any = false, alive = false;
        for (int i : b.echoes)
          if (i >= 0)
          {
            any = true;
            alive = alive || mEnemies[std::size_t(i)].alive;
          }
        if (!any && b.t >= kWaveDelay)
        {
          // Three of the runner's own moves, from 75, 105 and 135 frames ago.
          static constexpr int kDelays[3] = {75, 105, 135};
          for (int k = 0; k < 3; ++k)
          {
            const EchoFrame& f = z.ago(kDelays[k]);
            b.echoes[std::size_t(k)] = spawnEcho(f.x, f.y, kDelays[k], k, 3);
          }
          ++b.wave;
        }
        else if (any && !alive)
        {
          b.shutter = false;
          b.open = kOpenFrames;
          b.fan = 0;
          playSound(Sfx::Shutter);
          showMessage("THE EYE IS OPEN");
        }
      }
      else
      {
        if (++b.fan == kAimedEvery - kIrisTell)
          b.iris = kIrisTell;
        if (b.fan >= kAimedEvery)
        {
          b.fan = 0;
          shoot(aim);
          playSound(Sfx::ProbeShot);
        }
        if (--b.open <= 0)
        {
          b.open = 0;
          b.shutter = true;
          b.echoes = {-1, -1, -1};
          b.t = 0;
          playSound(Sfx::Shutter);
        }
      }
      break;
    }
    case ZeroPhase::Overload:
    {
      ++b.cycle;
      if (b.cycle >= kOverloadCycle)
      {
        // The segments come back (never on the runner), new safe ones.
        b.cycle = 0;
        ++b.cycles;
        for (auto& d : z.doors)
          if (d.segment && d.to < 1.0f)
            slideDoor(d, 1.0f, 6);
        pickSafeSegments();
        playSound(Sfx::Monitor);
      }
      for (std::size_t di = 0; di < z.doors.size(); ++di)
      {
        auto& d = z.doors[di];
        if (!d.segment)
          continue;
        const int k = (mLayers[std::size_t(d.layer)].x0 - b.ax0) / 3;
        if ((b.safe >> k) & 1u)
          continue;
        if (b.cycle == 45)
          d.shake = 15;
        if (b.cycle == 60)
          slideDoor(d, 0.0f, 1);
      }
      if (b.cycle == 45)
        playSound(Sfx::Rumble);
      if (b.cycle == 60)
      {
        playSound(Sfx::EyeCharge);
        mCamera.shake(6, 1.5f);
      }
      if (b.cycle == 82)
      {
        b.beam = 23;
        playSound(Sfx::EyeBeam);
      }
      break;
    }
    case ZeroPhase::Reveal:
    {
      const int t = ++b.reveal;
      if (t == 45)
        playSound(Sfx::Crash);
      if (t == 90)
      {
        playSound(Sfx::WallFall);
        mCamera.shake(8, 4.0f);
      }
      if (t == 120 || t == 135 || t == 150)
      {
        ++b.lights;
        playSound(Sfx::LightClunk);
      }
      if (t == 150)
        playSound(Sfx::Applause);
      if (t == 160)
        showMessage("LANCE: AND THAT'S A WRAP ON SEASON ONE!");
      if (t >= kRevealFrames)
      {
        zeroPhase(ZeroPhase::Done);
        showMessage("A STAGE DOOR: TO SET B");
      }
      break;
    }
    default:
      break;
  }
  // Where a fall into the grille sends the runner back: a segment that is
  // still there.
  if (b.phase == ZeroPhase::Overload && mLavaSafeX >= b.ax0 * kCellsPerTile)
  {
    auto standing = [&](int k) {
      for (const auto& d : z.doors)
        if (d.segment && (mLayers[std::size_t(d.layer)].x0 - b.ax0) / 3 == k)
          return mLayers[std::size_t(d.layer)].solid && d.to >= 1.0f;
      return false;
    };
    const int k0 = std::clamp((mLavaSafeX + 1) / kCellsPerTile - b.ax0, 0, kZeroSegments * 3 - 1) / 3;
    if (!standing(k0))
      for (int dk = 1; dk < kZeroSegments; ++dk)
      {
        const int k = standing(k0 - dk) ? k0 - dk : (standing(k0 + dk) ? k0 + dk : -1);
        if (k < 0)
          continue;
        mLavaSafeX = (b.ax0 + 3 * k) * kCellsPerTile + 1;
        mLavaSafeY = b.floorRow * kCellsPerTile - 1;
        break;
      }
  }
}

// --- Per frame -------------------------------------------------------------------------------------

void World::updateZero(const PlayerInput& input)
{
  auto& z = mZero;
  if (z.wireframe)
    return;
  auto& p = mPlayer;
  const CellBox pbox = p.box();
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  recordEcho();
  updateShifts();
  updateDoors();

  // Laser grids: on for the first half of every beat of `every` frames.
  for (const auto& g : z.grids)
  {
    const CellBox area{g.x0 * kCellsPerTile, g.y0 * kCellsPerTile, (g.x1 - g.x0 + 1) * kCellsPerTile,
      (g.y1 - g.y0 + 1) * kCellsPerTile};
    if (alive && mStats.frames % g.every < g.every / 2 && area.intersects(p.hitBox()))
      hurtPlayer(g.dmg);
  }

  // Echo pads: passing one starts an Echo that steps out where you stood.
  for (auto& pad : z.pads)
  {
    if (!pad.armed)
      continue;
    if (pad.echo >= 0 && !mEnemies[std::size_t(pad.echo)].alive)
      pad.echo = -1;
    const CellBox spot{pad.x * kCellsPerTile, pad.y * kCellsPerTile - 4, kCellsPerTile, 6};
    if (alive && pad.passedAt < 0 && pad.echo < 0 && spot.intersects(pbox))
      pad.passedAt = mStats.frames;
    if (pad.passedAt >= 0 && mStats.frames - pad.passedAt >= kEchoDelay)
    {
      const EchoFrame& f = z.ago(kEchoDelay);
      pad.echo = spawnEcho(f.x, f.y, kEchoDelay, -1, 4);
      pad.passedAt = -1;
    }
  }

  // Wrecks: what broke, and Repair Swarms building it back.
  for (std::size_t i = 0; i < mEnemies.size() && i < mLevelEnemyCount; ++i)
  {
    const Enemy& e = mEnemies[i];
    if (e.alive || e.kind != EnemyKind::LatticeTurret ||
        std::find(z.boss.turrets.begin(), z.boss.turrets.end(), int(i)) != z.boss.turrets.end())
      continue;
    bool known = false;
    for (const auto& w : z.wrecks)
      known = known || (w.turret && w.index == int(i));
    if (!known)
      z.wrecks.push_back({true, int(i), e.x + e.w / 2, e.y - e.h / 2, 0, 0});
  }
  for (int bi : z.rebuild)
  {
    const Breakable& br = mBreakables[std::size_t(bi)];
    if (!br.broken)
      continue;
    bool known = false;
    for (const auto& w : z.wrecks)
      known = known || (!w.turret && w.index == bi);
    if (!known)
      z.wrecks.push_back({false, bi, (br.x0 + br.x1 + 1) * kCellsPerTile / 2, (br.y0 + br.y1 + 1) * kCellsPerTile / 2,
        0, 0});
  }
  for (std::size_t wi = 0; wi < z.wrecks.size();)
  {
    auto& w = z.wrecks[wi];
    ++w.t;
    bool tended = false;
    for (const auto& e : mEnemies)
      tended = tended || (e.alive && e.kind == EnemyKind::RepairSwarm && std::abs(e.x + 1 - w.x) <= 3 &&
                           std::abs(e.y - 1 - w.y) <= 3);
    w.tended = tended ? w.tended + 1 : 0;
    bool done = false;
    if (w.tended > 0 && w.t >= kRebuildFrames)
    {
      if (w.turret)
      {
        Enemy& e = mEnemies[std::size_t(w.index)];
        if (!e.box().intersects(pbox))
        {
          e.alive = true;
          e.hp = enemyDef(e.def).hp;
          e.active = true;
          e.tell = 0;
          e.dive = enemyDef(e.def).cooldown;
          e.flash = 8;
          if (!(enemyDef(e.def).flags & kEnemyNoTally) && mStats.kills > 0)
            --mStats.kills; // it can be counted again
          done = true;
        }
      }
      else
      {
        Breakable& br = mBreakables[std::size_t(w.index)];
        const CellBox area{br.x0 * kCellsPerTile, br.y0 * kCellsPerTile, (br.x1 - br.x0 + 1) * kCellsPerTile,
          (br.y1 - br.y0 + 1) * kCellsPerTile};
        if (!area.intersects(pbox))
        {
          br.broken = false;
          for (std::size_t k = 0; k < z.rebuild.size(); ++k)
            if (z.rebuild[k] == w.index)
              br.hp = z.rebuildHp[k];
          for (int ty = br.y0; ty <= br.y1; ++ty)
            for (int tx = br.x0; tx <= br.x1; ++tx)
              mMap.setBlock(tx, ty, Tile::Solid);
          done = true;
        }
      }
      if (done)
      {
        const Vec2 c{float(w.x) * kCellSize, float(w.y) * kCellSize};
        burst(c, rgb(120, 255, 200), rgb(255, 255, 255), 16, 1.6f);
        flashAt(c, 80.0f, rgb(120, 255, 200), 14);
        playSound(Sfx::Rebuild);
      }
    }
    if (done)
      z.wrecks.erase(z.wrecks.begin() + std::ptrdiff_t(wi));
    else
      ++wi;
  }

  // Terminals: up at one lights its screen; the public one listens for up,
  // up, down, down.
  const bool upNow = input.up && !z.upHeld, downNow = input.down && !z.downHeld;
  z.upHeld = input.up;
  z.downHeld = input.down;
  if (z.codeT > 0 && --z.codeT == 0)
    z.codeStep = 0;
  for (auto& t : z.terminals)
  {
    if (t.shown > 0)
      --t.shown;
    const CellBox at{t.x * kCellsPerTile - 1, t.y * kCellsPerTile - 2, kCellsPerTile + 2, 4};
    if (!alive || !at.intersects(pbox))
      continue;
    if (upNow && t.shown == 0 && z.codeStep == 0)
    {
      t.shown = 90;
      showMessage(t.text);
      playSound(Sfx::Monitor);
    }
    if (!t.code)
      continue;
    static constexpr bool kUp[4] = {true, true, false, false};
    if (upNow || downNow)
    {
      if (upNow == kUp[z.codeStep])
      {
        if (z.codeStep == 0)
          z.codeT = 60;
        if (++z.codeStep == 4)
        {
          z.codeStep = 0;
          z.codeT = 0;
          z.joke = 150;
          showMessage("ZERO: WHY DON'T AIS WATCH TELEVISION? TOO MANY RERUNS OF THE SAME LOOP.");
          playSound(Sfx::Cheer);
        }
      }
      else
        z.codeStep = upNow ? 1 : 0;
    }
  }
  if (z.joke > 0)
    --z.joke;

  // A landing on a grating stomps it.
  const bool air = p.state == PlayerState::Jumping || p.state == PlayerState::Falling;
  if (alive && z.inAir && (p.state == PlayerState::OnGround || p.state == PlayerState::Recovering))
    hitBreakable({p.x, p.y + 1, Player::kWidth, 1}, 1, 7);
  z.inAir = air;

  for (auto& d : z.decos)
    if (d.glint > 0)
      --d.glint;

  if (z.boss.on)
    updateZeroBoss();
}

// --- Respawns and saves ----------------------------------------------------------------------------

void World::resetZero()
{
  auto& z = mZero;
  // Echoes go; the pads wait for the runner to pass again.
  for (auto& e : mEnemies)
    if (e.alive && e.kind == EnemyKind::Echo)
      e.alive = false;
  for (auto& pad : z.pads)
  {
    pad.passedAt = -1;
    pad.echo = -1;
  }
  z.historyLen = 0;
  z.fired = false;
  z.inAir = false;
  auto& b = z.boss;
  if (!b.on || b.phase == ZeroPhase::Idle || b.phase == ZeroPhase::Reveal || b.phase == ZeroPhase::Done)
    return;
  // Back at the checkpoint: the door opens and ZERO waits for the runner.
  b.away = true;
  shutZeroDoor(false);
  for (auto& d : z.doors)
    if (d.segment)
    {
      d.pos = d.from = d.to = 1.0f;
      d.t = d.slide = 0;
      d.shake = 0;
      mLayers[std::size_t(d.layer)].scriptSolid = true;
    }
  b.cycle = 0;
  b.beam = 0;
  b.iris = 0;
  b.fan = 0;
  if (b.phase == ZeroPhase::Echoes)
  {
    b.shutter = true;
    b.open = 0;
    b.echoes = {-1, -1, -1};
    b.t = 0;
  }
  if (b.phase == ZeroPhase::Overload)
    pickSafeSegments();
}

bool World::zeroCanSave() const
{
  const auto& z = mZero;
  for (const auto& s : z.shifts)
    if (s.t >= 0 && !s.done)
      return false;
  for (const auto& d : z.doors)
    if (d.t < d.slide)
      return false;
  for (const auto& e : mEnemies)
    if (e.alive && e.kind == EnemyKind::Echo)
      return false;
  for (const auto& pad : z.pads)
    if (pad.passedAt >= 0)
      return false;
  const auto& b = z.boss;
  return !b.on || b.phase == ZeroPhase::Idle || b.phase == ZeroPhase::Done;
}

std::vector<int> World::zeroSave() const
{
  const auto& z = mZero;
  std::vector<int> v{z.state};
  for (const auto& s : z.shifts)
    v.push_back(s.done ? 1 : 0);
  for (const auto& sw : z.switches)
    v.push_back(sw.hit ? 1 : 0);
  for (const auto& pad : z.pads)
    v.push_back(pad.armed ? 1 : 0);
  v.push_back(z.boss.phase == ZeroPhase::Done ? 1 : 0);
  return v;
}

bool World::validZeroSave(const std::vector<int>& v) const
{
  const auto& z = mZero;
  return v.size() == 2 + z.shifts.size() + z.switches.size() + z.pads.size();
}

void World::loadZero(const std::vector<int>& v)
{
  auto& z = mZero;
  std::size_t at = 0;
  z.state = v[at++];
  for (auto& s : z.shifts)
  {
    s.done = v[at++] != 0;
    s.t = s.done ? kShiftPreview + s.slide : -1;
    if (!s.done)
      continue;
    for (auto& d : z.doors)
    {
      if (std::find(s.open.begin(), s.open.end(), d.layer) != s.open.end())
        d.pos = d.from = d.to = 0.0f;
      if (std::find(s.close.begin(), s.close.end(), d.layer) != s.close.end())
        d.pos = d.from = d.to = 1.0f;
    }
  }
  for (auto& d : z.doors)
  {
    d.t = d.slide = 0;
    Layer& l = mLayers[std::size_t(d.layer)];
    l.scriptSolid = d.pos >= 0.5f;
    applyLayer(l, l.scriptSolid);
  }
  for (auto& sw : z.switches)
    sw.hit = v[at++] != 0;
  for (auto& pad : z.pads)
  {
    pad.armed = v[at++] != 0;
    pad.passedAt = -1;
    pad.echo = -1;
  }
  if (v[at++] != 0 && z.boss.on)
  {
    z.boss.phase = ZeroPhase::Done;
    z.boss.exitOpen = true;
    z.boss.racks = 0.0f;
    z.boss.reveal = kRevealFrames;
    z.boss.lights = 3;
  }
  z.preview = -1;
  z.historyLen = 0;
  z.wrecks.clear();
}

} // namespace gr
