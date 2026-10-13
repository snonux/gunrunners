// Level 33, Magma Mainframe: rising lava with coolant purges at the
// checkpoints, the Heat Sink, Firewalls, Lava Daemons, Fan Bots, the
// corrupted rack, the magma gutter, the RATINGS server and the DO NOT
// REBOOT rack, and the Overclock bonus (rules=overclock). Drawing is in
// world_magma_draw.cpp.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace gr
{

namespace
{

int sgn(int v) { return (v > 0) - (v < 0); }

CellBox blockRect(int x0, int y0, int x1, int y1)
{
  return {std::min(x0, x1) * kCellsPerTile, std::min(y0, y1) * kCellsPerTile, (std::abs(x1 - x0) + 1) * kCellsPerTile,
    (std::abs(y1 - y0) + 1) * kCellsPerTile};
}

// "1/20" -> 1, 20.
void parseRate(const std::string& s, int& num, int& den)
{
  const auto slash = s.find('/');
  num = std::max(1, std::atoi(s.substr(0, slash).c_str()));
  den = slash == std::string::npos ? 1 : std::max(1, std::atoi(s.substr(slash + 1).c_str()));
}

} // namespace

// --- Setup ---------------------------------------------------------------------------------

bool World::setupMagmaEntity(const EntityDef& e)
{
  if (mLevel->themeKey != "volcano_datacenter")
    return false;
  auto& s = mMagma;
  s.on = true;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  const bool hasRect = e.rect("rect", x0, y0, x1, y1);
  if (e.kind == "fluid" && hasRect && e.str("kind") == "rising")
  {
    const CellBox b = blockRect(x0, y0, x1, y1);
    s.rising = true;
    s.x0 = b.x;
    s.x1 = b.right();
    s.startY = s.surface = b.y;
    s.floorY = b.bottom();
    parseRate(e.str("rate", "1/20"), s.rateNum, s.rateDen);
    s.delay = std::max(0, e.num("delay", 150));
    s.capY = std::max(0, e.num("cap", 0)) * kCellsPerTile;
    return true;
  }
  if (e.kind == "fluid" && hasRect && e.str("kind") == "gutter")
  {
    s.gutter = blockRect(x0, y0, x1, y1);
    return true;
  }
  if (e.kind == "purge" && e.hasPos)
  {
    Purge p;
    p.id = e.id;
    p.x = e.x * kCellsPerTile;
    p.y = e.y * kCellsPerTile + 1;
    s.purges.push_back(p);
    return true;
  }
  if (e.kind == "virusrack" && e.hasPos)
  {
    VirusRack r;
    r.x = e.x * kCellsPerTile;
    r.y = e.y * kCellsPerTile + 1;
    r.hp = std::max(1, e.num("hp", 4));
    r.on = std::max(1, e.num("on", 60));
    r.off = std::max(kRackSpin, e.num("off", 45));
    r.dir = e.str("dir", "l") == "r" ? 1 : -1;
    r.dust = hasRect ? blockRect(x0, y0, x1, y1) : CellBox{r.x - 12, r.y - 5, 12, 6};
    s.racks.push_back(r);
    return true;
  }
  if (e.kind == "reboot" && e.hasPos)
  {
    s.rebootX = e.x * kCellsPerTile;
    s.rebootY = e.y * kCellsPerTile + 1;
    return true;
  }
  if (e.kind == "treasure" && e.hasPos && e.str("art") == "server_ratings")
  {
    s.serverX = e.x * kCellsPerTile;
    s.serverY = e.y * kCellsPerTile + 1;
    s.serverScore = e.num("score", 10000);
    return true;
  }
  if (e.kind == "duck")
    return true; // the duck's heat suit is only drawn
  return false;
}

void World::setupMagmaEnemy(Enemy& en, const EntityDef& e)
{
  auto& s = mMagma;
  if (!s.on && mLevel->rules.find("overclock") == std::string::npos)
    return;
  const int idx = int(mEnemies.size()) - 1;
  if (en.kind == EnemyKind::Firewall)
  {
    Firewall w;
    w.enemy = idx;
    w.x0 = e.num("x0", en.x / kCellsPerTile) * kCellsPerTile;
    w.x1 = (e.num("x1", en.x / kCellsPerTile) + 1) * kCellsPerTile - en.w;
    if (w.x1 < w.x0)
      w.x1 = w.x0;
    const std::string ports = e.str("ports");
    std::size_t at = 0;
    while (at < ports.size())
    {
      const auto comma = ports.find(',', at);
      const std::string item = ports.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
      if (!item.empty())
        w.ports.push_back(std::atoi(item.c_str()) * kCellsPerTile);
      if (comma == std::string::npos)
        break;
      at = comma + 1;
    }
    en.x = std::clamp(en.x, w.x0, w.x1);
    w.startX = en.x;
    w.startDir = e.str("dir") == "l" ? -1 : 1;
    en.dir = w.startDir;
    s.walls.push_back(w);
    return;
  }
  if (en.kind == EnemyKind::LavaDaemon)
  {
    Daemon d;
    d.enemy = idx;
    d.floorY = en.y;
    d.ax = en.x;
    d.bx = e.num("x2", -1) >= 0 ? e.num("x2", 0) * kCellsPerTile : en.x + 12;
    d.marshmallow = e.num("marshmallow", 0) != 0;
    d.roused = !d.marshmallow;
    s.daemons.push_back(d);
    return;
  }
  if (en.kind == EnemyKind::FanBot)
  {
    Fan f;
    f.enemy = idx;
    en.dir = e.str("dir", "r") == "l" ? -1 : 1;
    s.fans.push_back(f);
  }
}

void World::linkMagma()
{
  auto& s = mMagma;
  s.overclock = mLevel->rules.find("overclock") != std::string::npos;
  s.on = s.on || s.overclock || mLevel->themeKey == "volcano_datacenter";
  if (!s.on)
    return;
  if (s.overclock)
  {
    mBonusFramesLeft *= 3; // the timer counts real seconds
    for (auto& w : s.walls)
      w.harmless = true;
    // The music runs fast too (sound only: the frontend plays this track).
    if (!mLevel->music.empty() && mMusicOverride.empty())
      mMusicOverride = mLevel->music + "+fast";
  }
  // The Fan Bots' gusts: across the shaft from the blades to the far wall.
  for (auto& f : s.fans)
  {
    const Enemy& e = mEnemies[std::size_t(f.enemy)];
    const int mid = e.y - e.h / 2;
    int x = e.dir > 0 ? e.x + e.w : e.x - 1;
    const int start = x;
    while (x >= 0 && x < mMap.width() && !mMap.solid(x, mid) && std::abs(x - start) < 80)
      x += e.dir;
    Zone z;
    z.kind = ZoneKind::Wind;
    z.box = e.dir > 0 ? CellBox{start, e.y - kFanBand + 1, x - start, kFanBand}
                      : CellBox{x + 1, e.y - kFanBand + 1, start - x, kFanBand};
    z.dx = e.dir;
    z.num = 1;
    z.den = 2;
    z.period = kFanPeriod;
    z.on = kFanOn;
    f.zone = int(mZones.size());
    mZones.push_back(z);
  }
  // The bonus entrance waits for the RATINGS server to be shot.
  if (s.serverX >= 0 && !s.serverShot)
    for (auto& pr : mProps)
      if (pr.kind == PropKind::BonusDoor)
        pr.dormant = true;
}

int World::lavaGap() const
{
  if (!mMagma.rising)
    return 99;
  return std::max(0, (mMagma.surface - mPlayer.y) / kCellsPerTile);
}

// --- Rising lava ---------------------------------------------------------------------------

// The nearest standing spot at least kPopClear above the surface: up the
// runner's column first, then outward; never above the next checkpoint.
bool World::findLavaPop(int& outX, int& outY) const
{
  const auto& s = mMagma;
  const auto& p = mPlayer;
  int limit = 0;
  for (const auto& cp : mCheckpoints)
    if (!cp.active && cp.y < s.surface)
      limit = std::max(limit, cp.y);
  const int top = s.surface - kPopClear;
  for (int k = 0; k <= 80; ++k)
  {
    const int dx = (k % 2 == 0 ? 1 : -1) * ((k + 1) / 2);
    const int x = p.x + dx;
    if (x < s.x0 || x + Player::kWidth - 1 > s.x1)
      continue;
    for (int y = top; y >= std::max(limit, 5); --y)
    {
      const CellBox b = boxAt(x, y, Player::kWidth, Player::kHeight);
      if (!mMap.overlapsSolid(b) && mMap.onSolidGround(b))
      {
        outX = x;
        outY = y;
        return true;
      }
    }
  }
  return false;
}

void World::touchLava()
{
  auto& p = mPlayer;
  auto& s = mMagma;
  const Vec2 at{(float(p.x) + 1.5f) * kCellSize, float(p.y) * kCellSize};
  burst(at, rgb(255, 200, 80), rgb(255, 70, 20), 16, 1.8f);
  playSound(Sfx::LavaPop);
  ++s.pops;
  if (p.mercy == 0 && !mGod)
  {
    p.hp -= kLavaHearts;
    mStats.tookDamage = true;
    if (p.hp <= 0)
    {
      p.hp = 0;
      killPlayer();
      return;
    }
    playSound(Sfx::Hurt);
  }
  int x = p.x, y = p.y;
  if (!findLavaPop(x, y))
    return;
  // Drawing only: the pop's arc (world_magma_draw.cpp).
  s.popFrame = mStats.frames;
  s.popFromX = p.x;
  s.popFromY = p.y;
  s.popToX = x;
  s.popToY = y;
  p.x = p.prevX = x;
  p.y = p.prevY = y;
  p.state = PlayerState::OnGround;
  p.frames = 0;
  p.somersault = -1;
  p.fling = 0;
  mLaunch = 0;
  p.mercy = std::max(p.mercy, 30);
  setVisual(PlayerVisual::Standing);
  burst({(float(p.x) + 1.5f) * kCellSize, (float(p.y) - 2.0f) * kCellSize}, rgb(255, 255, 255), rgb(255, 120, 40), 10, 1.2f);
}

void World::updateMagma(const PlayerInput& input)
{
  (void)input;
  auto& s = mMagma;
  auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  const CellBox pb = p.box();

  // Coolant: a checkpoint touched vents its purge, once.
  for (auto& pu : s.purges)
  {
    if (pu.steam > 0)
      --pu.steam;
    if (!pu.fired && alive && pu.box().intersects(pb))
    {
      pu.fired = true;
      pu.steam = 30;
      s.pause = std::max(s.pause, kPurgeFrames);
      playSound(Sfx::SteamBurst);
      showMessage("COOLANT PURGE - THE LAVA HOLDS");
    }
  }
  if (s.reboot > 0)
    --s.reboot;

  // The lava rises.
  if (s.rising && mState == WorldState::Playing)
  {
    if (s.delay > 0)
    {
      --s.delay;
      if (s.delay == 0 && !mSimulation)
        playSound(Sfx::LavaRumble); // it starts to rise
    }
    else if (s.reboot > 0)
    {
    }
    else if (s.pause > 0)
    {
      if (--s.pause == kSirenFrames)
        playSound(Sfx::PurgeSiren);
      else if (s.pause == 0 && !mSimulation)
        playSound(Sfx::LavaRumble); // rising again
    }
    else
    {
      s.sub += s.rateNum;
      while (s.sub >= s.rateDen)
      {
        s.sub -= s.rateDen;
        if (s.surface > s.capY)
          --s.surface;
      }
    }
    // Turbo walks on it (and comes up with it).
    syncFloats();
    if (alive && p.turbo > 0 && p.y >= s.surface && p.y <= s.surface + 2 && pb.right() >= s.x0 && pb.left() <= s.x1)
      mMap.moveVertically(p.x, p.y, Player::kWidth, p.height(), s.surface - 1 - p.y);
    if (alive && p.turbo == 0 && p.y >= s.surface && pb.right() >= s.x0 && pb.left() <= s.x1)
      touchLava();
    // What it swallows dies (no score).
    for (auto& e : mEnemies)
      if (e.alive && e.y - e.h + 1 >= s.surface && e.x + e.w > s.x0 && e.x <= s.x1)
      {
        e.alive = false;
        burst(cellCenter(e.box()), rgb(255, 200, 80), rgb(255, 70, 20), 10, 1.4f);
      }
  }

  // The magma gutter: a heart every 15 frames, no pop.
  if (s.gutter.w > 0 && alive && p.turbo == 0 && !mGod && s.gutter.intersects(pb) && p.y >= s.gutter.y)
  {
    if (s.gutterT++ % kGutterEvery == 0)
    {
      p.hp -= 1;
      mStats.tookDamage = true;
      playSound(Sfx::GutterSizzle);
      burst({(float(p.x) + 1.5f) * kCellSize, float(p.y) * kCellSize}, rgb(255, 200, 80), rgb(255, 70, 20), 8, 1.2f);
      if (p.hp <= 0)
      {
        p.hp = 0;
        killPlayer();
      }
    }
  }
  else
    s.gutterT = 0;

  // The corrupted rack blows Virus dust.
  for (auto& r : s.racks)
  {
    if (r.hp <= 0)
      continue;
    r.t = (r.t + 1) % (r.on + r.off);
    if (r.t == r.on + r.off - kRackSpin && isOnScreen(r.box(), 4))
      playSound(Sfx::RackGlitch);
    if (r.blowing() && alive && p.virus == 0 && p.mercy == 0 && r.dust.intersects(p.hitBox()))
      infect();
  }

  // A Fan Bot shot down stops its gust.
  for (const auto& f : s.fans)
    if (f.zone >= 0 && !mEnemies[std::size_t(f.enemy)].alive)
      mZones[std::size_t(f.zone)].on = 0;
}

void World::resetMagma()
{
  auto& s = mMagma;
  bool atCheckpoint = false;
  for (const auto& cp : mCheckpoints)
    atCheckpoint = atCheckpoint || cp.active;
  if (s.rising)
  {
    if (atCheckpoint)
    {
      s.surface = std::min(s.startY, mRespawnY + kRespawnDepth);
      s.pause = kPurgeFrames; // the purge runs again
    }
    else
    {
      s.surface = s.startY;
      s.delay = 150;
      s.pause = 0;
    }
    s.sub = 0;
    syncFloats();
  }
  s.reboot = 0;
  s.rebooted = false;
  s.sucking = false;
  s.sinkT = 0;
  s.gutterT = 0;
  for (auto& w : s.walls)
  {
    Enemy& e = mEnemies[std::size_t(w.enemy)];
    if (!e.alive)
      continue;
    e.x = e.prevX = w.startX;
    e.dir = w.startDir;
    w.pause = 0;
    w.lastPort = -1;
  }
  for (auto& d : s.daemons)
  {
    Enemy& e = mEnemies[std::size_t(d.enemy)];
    if (!e.alive)
      continue;
    e.x = e.prevX = d.ax;
    e.y = e.prevY = d.floorY;
    d.leapT = 0;
    e.tell = 0;
  }
}

// --- The Heat Sink -------------------------------------------------------------------------

void World::updateHeatSink(const Button& fire)
{
  auto& p = mPlayer;
  auto& s = mMagma;
  const ProtoDef& def = protoDef(p.proto);
  const int row = p.stance == Stance::Crouched ? p.y - 1 : p.y - 2;
  const int mx = p.facing > 0 ? p.x + Player::kWidth : p.x - 1;
  if (fire.pressed)
  {
    if (!s.sucking)
      playSound(Sfx::SinkSuck);
    s.sucking = true;
    ++s.sinkT;
    // Fireballs and enemy shots in the cone come in.
    for (auto& pr : mProjectiles)
    {
      if (!pr.alive || pr.kind != ShotKind::Enemy)
        continue;
      const CellBox b = pr.box();
      const int d = (b.x + b.w / 2 - mx) * p.facing;
      const int dy = std::abs(b.y + b.h / 2 - row);
      if (d < -1 || d > kSinkReach || dy > 1 + d / 2)
        continue;
      pr.alive = false;
      const Vec2 c = cellCenter(b);
      burst(c, rgb(255, 200, 80), rgb(255, 120, 40), 6, 0.8f);
      if (p.ammo < kSinkMax)
      {
        ++p.ammo;
        playSound(Sfx::SinkGulp);
      }
    }
    return;
  }
  if (!s.sucking)
    return;
  s.sucking = false;
  s.sinkT = 0;
  if (p.ammo <= 0 || p.shotCooldown > 0)
    return;
  // Release: everything stored goes out as one slow fireball.
  Projectile pr;
  pr.kind = ShotKind::Rocket;
  pr.proto = int(ProtoId::HeatSink);
  pr.w = pr.h = 2;
  pr.speed = 1;
  pr.damage = p.ammo;
  pr.radius = kSinkRadius;
  pr.dx = p.facing;
  pr.range = 60;
  pr.x = pr.prevX = p.facing > 0 ? p.x + Player::kWidth : p.x - 2;
  pr.y = pr.prevY = row - 1;
  mProjectiles.push_back(pr);
  playSound(Sfx::SinkThrow);
  p.shotCooldown = def.cooldown;
  p.ammo = 0;
  p.weapon = Weapon::Normal;
  p.proto = -1;
  showMessage("HEAT SINK EMPTY - BACK TO THE BLASTER");
}

// A Heat Sink fireball goes off where it hits rock, an enemy or a rack;
// other shots hit the racks and the RATINGS server.
bool World::shotAtMagma(Projectile& pr, const CellBox& hit)
{
  auto& s = mMagma;
  if (pr.kind == ShotKind::Enemy)
    return false;
  const bool sink = pr.kind == ShotKind::Rocket && pr.proto == int(ProtoId::HeatSink);
  if (s.serverX >= 0 && !s.serverShot && s.serverBox().intersects(hit))
  {
    s.serverShot = true;
    addScore(s.serverScore, cellCenter(s.serverBox()));
    burst(cellCenter(s.serverBox()), rgb(255, 230, 120), rgb(120, 200, 255), 20, 2.0f);
    playSound(Sfx::RatingsDing);
    showMessage("RATINGS (DO NOT TOUCH) - SOMETHING OPENS");
    for (auto& d : mProps)
      if (d.kind == PropKind::BonusDoor)
        d.dormant = false;
    if (sink)
    {
      s.blast = true;
      explodeAt(hit.x + hit.w / 2, hit.y + hit.h / 2, pr.radius, pr.damage);
      s.blast = false;
    }
    return true;
  }
  if (sink)
  {
    bool boom = mMap.overlapsSolid(hit);
    for (const auto& e : mEnemies)
      boom = boom || (e.alive && e.active && !e.hidden && e.box().intersects(hit));
    for (const auto& r : s.racks)
      boom = boom || (r.hp > 0 && r.box().intersects(hit));
    if (!boom)
      return false;
    const int cx = hit.x + hit.w / 2, cy = hit.y + hit.h / 2;
    const CellBox area{cx - pr.radius, cy - pr.radius, pr.radius * 2 + 1, pr.radius * 2 + 1};
    for (auto& r : s.racks)
      if (r.hp > 0 && r.box().intersects(area))
      {
        r.hp = 0;
        addScore(500, cellCenter(r.box()));
      }
    s.blast = true;
    explodeAt(cx, cy, pr.radius, pr.damage);
    s.blast = false;
    return true;
  }
  for (auto& r : s.racks)
    if (r.hp > 0 && r.box().intersects(hit))
    {
      burst(cellCenter(hit), rgb(140, 255, 70), rgb(255, 255, 255), 6, 1.2f);
      if (--r.hp <= 0)
      {
        addScore(500, cellCenter(r.box()));
        playSound(Sfx::SmallExplosion);
        showMessage("CORRUPTED RACK DOWN");
      }
      else
        playSound(Sfx::Hit);
      return true;
    }
  if (s.rebootX >= 0 && !s.rebooted && s.rebootBox().intersects(hit))
  {
    s.rebooted = true;
    s.reboot = kRebootFrames;
    playSound(Sfx::RebootChime);
    showMessage("REBOOTING... UPTIME 0 DAYS");
    return true;
  }
  return false;
}

bool World::magmaBlocksDamage(const Enemy& e) const
{
  return e.kind == EnemyKind::Firewall && !mMagma.blast;
}

// 1: the shot is spent, 2: it passes, 0: as usual.
int World::shotAtMagmaEnemy(Projectile& pr, Enemy& e)
{
  if (e.kind == EnemyKind::Firewall)
  {
    burst(cellCenter(pr.box()), rgb(255, 230, 120), rgb(255, 120, 40), 4, 0.8f);
    return 1; // only a Heat Sink fireball hurts it
  }
  if (e.kind == EnemyKind::FanBot && pr.dy == 0)
  {
    // The blades turn shots from the front; above or below the fan it's open.
    const CellBox b = pr.box();
    const int dir = pr.precise ? (pr.vx < 0.0f ? -1 : 1) : pr.dx;
    const bool fanRows = b.bottom() >= e.y - e.h + 2 && b.top() <= e.y - 1;
    if (dir == -e.dir && fanRows)
    {
      e.flash = 2;
      burst(cellCenter(b), rgb(255, 255, 255), rgb(200, 200, 220), 5, 1.2f);
      playSound(Sfx::Tink);
      return 1;
    }
  }
  if (e.kind == EnemyKind::LavaDaemon)
    for (auto& d : mMagma.daemons)
      if (&mEnemies[std::size_t(d.enemy)] == &e)
        d.roused = true;
  return 0;
}

// --- Enemies -------------------------------------------------------------------------------

void World::updateFirewall(Enemy& e, const EnemyDef& def)
{
  (void)def;
  Firewall* w = nullptr;
  for (auto& f : mMagma.walls)
    if (&mEnemies[std::size_t(f.enemy)] == &e)
      w = &f;
  if (!w)
    return;
  if (w->pause > 0)
  {
    --w->pause;
    if (w->pause == 0 && !mSimulation && isOnScreen(e.box(), 2))
      playSound(Sfx::FirewallRoar); // the flames back up
  }
  else if (e.timer % 2 == 0)
  {
    e.x += e.dir;
    if (e.x <= w->x0 || e.x >= w->x1)
    {
      e.x = std::clamp(e.x, w->x0, w->x1);
      e.dir = -e.dir;
    }
    if (w->lastPort >= 0 && e.x != w->lastPort)
      w->lastPort = -1;
    for (const int port : w->ports)
      if (e.x == port && w->lastPort != port)
      {
        w->pause = kWallFlicker + kWallDip;
        w->lastPort = port;
        if (!mSimulation && isOnScreen(e.box(), 2))
          playSound(Sfx::FirewallSputter);
      }
  }
  const bool dipped = w->pause > 0 && w->pause <= kWallDip;
  e.tell = w->pause > kWallDip ? w->pause - kWallDip : 0;
  e.dive = dipped ? 1 : 0;
  // The flames: contact hurts a heart and knocks you back.
  auto& p = mPlayer;
  if (p.state == PlayerState::Dying || p.state == PlayerState::Teleporting)
    return;
  const int tall = dipped ? kWallLow : kWallTall;
  const CellBox flames{e.x, e.y - tall + 1, e.w, tall};
  if (!flames.intersects(p.hitBox()))
    return;
  if (!w->harmless)
    hurtPlayer(1);
  if (p.state == PlayerState::Dying)
    return;
  const int dir = sgn((p.x + 1) - (e.x + e.w / 2 - 1)) != 0 ? sgn((p.x + 1) - (e.x + e.w / 2 - 1)) : -e.dir;
  for (int i = 0; i < kWallKnock; ++i)
    mMap.moveHorizontally(p.x, p.y, Player::kWidth, p.height(), dir);
  if (w->harmless)
    playSound(Sfx::Boing);
}

void World::updateLavaDaemon(Enemy& e, const EnemyDef& def)
{
  Daemon* d = nullptr;
  for (auto& f : mMagma.daemons)
    if (&mEnemies[std::size_t(f.enemy)] == &e)
      d = &f;
  if (!d)
    return;
  // Leaping between its perches.
  if (d->leapT > 0)
  {
    const float k = float(d->leapT) / float(kDaemonArc);
    e.x = d->fromX + int(std::lround(float(d->toX - d->fromX) * k));
    e.y = d->floorY - int(std::lround(24.0f * k * (1.0f - k)));
    if (++d->leapT > kDaemonArc)
    {
      d->leapT = 0;
      e.x = d->toX;
      e.y = d->floorY;
      e.dir = d->toX > d->fromX ? 1 : -1;
    }
    return;
  }
  if (!d->roused)
  {
    e.tell = 0;
    return; // roasting its marshmallow
  }
  if (e.cool > 0)
    --e.cool;
  const auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  const CellBox pb = p.hitBox(), b = e.box();
  const int dx = (pb.x + 1) - (b.x + 1);
  const bool sees = alive && std::abs(dx) <= kDaemonSight && std::abs(pb.bottom() - b.bottom()) <= 16 && isOnScreen(b, 0);
  if (e.tell > 0)
  {
    if (--e.tell == 0)
    {
      // A fireball lobbed at you: lands where you stand now.
      const float g = 0.25f;
      const float sx = float(b.x + 1), sy = float(b.y - 1);
      const float tx = float(pb.x + 1), ty = float(pb.bottom() - 1);
      const float T = std::clamp(std::fabs(tx - sx) / 1.0f, 8.0f, 20.0f);
      Projectile pr;
      pr.kind = ShotKind::Enemy;
      pr.w = pr.h = 1;
      pr.speed = 1;
      pr.damage = 1;
      pr.precise = true;
      pr.range = 80;
      pr.gy = g;
      pr.fx = sx;
      pr.fy = sy;
      pr.vx = (tx - sx) / T;
      pr.vy = (ty - sy - 0.5f * g * T * T) / T;
      pr.dx = dx < 0 ? -1 : 1;
      pr.x = pr.prevX = int(sx);
      pr.y = pr.prevY = int(sy);
      mProjectiles.push_back(pr);
      if (!mSimulation)
        playSound(Sfx::DaemonSpit);
      e.cool = def.cooldown;
    }
    return;
  }
  if (sees && e.cool == 0)
  {
    e.dir = dx < 0 ? -1 : 1;
    e.tell = def.tell;
    if (!mSimulation)
      playSound(Sfx::DaemonWindup);
    return;
  }
  if (e.timer % kDaemonLeap == kDaemonLeap - 1 && d->bx != d->ax)
  {
    d->fromX = e.x;
    d->toX = e.x == d->ax ? d->bx : d->ax;
    d->leapT = 1;
    if (!mSimulation && isOnScreen(b, 0))
      playSound(Sfx::DaemonHop);
  }
}

void World::updateFanBot(Enemy& e, const EnemyDef& def)
{
  (void)def;
  const int t = mStats.frames % kFanPeriod;
  e.dive = t < kFanOn ? 1 : 0; // blowing
  e.tell = t >= kFanPeriod - kFanSpin ? kFanPeriod - t : 0;
  if (t == kFanPeriod - kFanSpin && !mSimulation && isOnScreen(e.box(), 2))
    playSound(Sfx::FanWhine);
  if (t == 0 && !mSimulation && isOnScreen(e.box(), 2))
    playSound(Sfx::FanGust);
}

// --- Saves ---------------------------------------------------------------------------------

bool World::magmaCanSave() const
{
  const auto& s = mMagma;
  if (s.sucking || s.reboot > 0)
    return false;
  for (const auto& w : s.walls)
    if (w.pause > 0)
      return false;
  for (const auto& d : s.daemons)
    if (d.leapT > 0)
      return false;
  return true;
}

std::vector<int> World::magmaSave() const
{
  const auto& s = mMagma;
  std::vector<int> v{s.surface, s.delay, s.pause, s.sub, s.rebooted, s.serverShot, int(s.purges.size()),
    int(s.racks.size()), int(s.daemons.size())};
  for (const auto& pu : s.purges)
    v.push_back(pu.fired);
  for (const auto& r : s.racks)
  {
    v.push_back(r.hp);
    v.push_back(r.t);
  }
  for (const auto& d : s.daemons)
    v.push_back(d.roused);
  return v;
}

bool World::validMagmaSave(const std::vector<int>& v) const
{
  const auto& s = mMagma;
  return v.size() == 9 + s.purges.size() + 2 * s.racks.size() + s.daemons.size() && v[6] == int(s.purges.size()) &&
    v[7] == int(s.racks.size()) && v[8] == int(s.daemons.size());
}

void World::loadMagma(const std::vector<int>& v)
{
  auto& s = mMagma;
  s.surface = v[0];
  s.delay = v[1];
  s.pause = v[2];
  s.sub = v[3];
  s.rebooted = v[4] != 0;
  s.serverShot = v[5] != 0;
  std::size_t at = 9;
  for (auto& pu : s.purges)
    pu.fired = v[at++] != 0;
  for (auto& r : s.racks)
  {
    r.hp = v[at++];
    r.t = v[at++];
  }
  for (auto& d : s.daemons)
    d.roused = v[at++] != 0;
  if (s.serverShot)
    for (auto& pr : mProps)
      if (pr.kind == PropKind::BonusDoor)
        pr.dormant = false;
  syncFloats();
}

} // namespace gr
