// Level 34, Zeppelin Relay: the drifting fleet (ships that bob, drift and
// sink, and carry what sits on them), the Grapple Gun and its rings, Sky
// Pirates, Peck Parrots, Balloon Mines, the Looter and its captain's log,
// the clouds below, and the Human Cannonball bonus (rules=cannonball).
// Drawing is in world_fleet_draw.cpp.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace gr
{

namespace
{

int sgn(int v) { return (v > 0) - (v < 0); }

constexpr float kRad = 3.14159265f / 180.0f;

// "A/P" (blocks either way / frames) -> cells, frames.
void parseWave(const std::string& s, int& a, int& p)
{
  if (s.empty())
    return;
  const auto slash = s.find('/');
  a = std::max(0, std::atoi(s.substr(0, slash).c_str())) * kCellsPerTile;
  if (slash != std::string::npos)
    p = std::max(2, std::atoi(s.substr(slash + 1).c_str()));
}

int ringPeriod(int len) { return std::max(16, (12 + len) & ~1); }

bool lightEnemy(EnemyKind k)
{
  return k == EnemyKind::SkyPirate || k == EnemyKind::PeckParrot || k == EnemyKind::BalloonMine;
}

} // namespace

// --- Setup ---------------------------------------------------------------------------------

bool World::setupFleetEntity(const EntityDef& e)
{
  const bool cannon = mLevel->rules.find("cannonball") != std::string::npos;
  if (mLevel->themeKey != "sunset_airships" && !cannon)
    return false;
  auto& s = mFleet;
  s.on = true;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  const bool hasRect = e.rect("rect", x0, y0, x1, y1);
  if (e.kind == "ship" && e.hasPos)
  {
    Ship sh;
    sh.id = e.id;
    sh.name = e.str("name");
    sh.x0 = e.x;
    sh.g = e.y;
    sh.x1 = std::max(e.x, e.num("x1", e.x + 10));
    parseWave(e.str("bob"), sh.bobA, sh.bobP);
    parseWave(e.str("drift"), sh.driftA, sh.driftP);
    sh.phase = e.num("phase", 0);
    sh.balloon = e.num("balloon", 1) != 0;
    sh.sinkMax = std::max(0, e.num("sink", 0)) * kCellsPerTile;
    sh.looter = e.num("looter", 0) != 0;
    sh.tiles = e.num("tiles", 0) != 0;
    s.ships.push_back(sh);
    return true;
  }
  if (e.kind == "ring" && e.hasPos)
  {
    s.rings.push_back({e.x * kCellsPerTile + 1, e.y * kCellsPerTile + 1});
    return true;
  }
  if (e.kind == "spawn" && e.str("kind") == "sky_pirate")
  {
    const int def = enemyIndex("sky_pirate");
    if (def < 0)
      return true;
    // Out of sight (and of the way) until its trigger.
    const int tx = e.num("trigger", 0) * kCellsPerTile;
    spawnEnemy(def, tx, 10);
    auto& en = mEnemies.back();
    en.hidden = true;
    Pirate pi;
    pi.enemy = int(mEnemies.size()) - 1;
    pi.triggerX = tx;
    s.pirates.push_back(pi);
    return true;
  }
  if (e.kind == "spawn" && hasRect && e.str("kind") == "balloon_mine")
  {
    MineSpawn m;
    m.x0 = x0 * kCellsPerTile;
    m.x1 = (x1 + 1) * kCellsPerTile - 1;
    m.y = y1 * kCellsPerTile + 1;
    m.every = std::max(10, e.num("every", 90));
    m.max = std::max(1, e.num("max", 2));
    m.seed = 1 + x0 * 7 + y0;
    s.spawns.push_back(m);
    return true;
  }
  if (e.kind == "treasure" && e.hasPos && e.str("art") == "captains_log")
  {
    s.logX = e.x * kCellsPerTile;
    s.logY = e.y * kCellsPerTile + 1;
    s.logScore = e.num("score", 10000);
    return true;
  }
  if (e.kind == "bay" && hasRect)
  {
    // A ship's bomb bay: its hatch (hatch=x,y) opens from inside.
    s.bayX0 = x0 * kCellsPerTile;
    s.bayX1 = (x1 + 1) * kCellsPerTile - 1;
    s.bayY = (y1 + 1) * kCellsPerTile;
    const auto d = e.list("drop");
    if (d.size() == 2)
    {
      s.dropX = d[0] * kCellsPerTile;
      s.dropY = d[1] * kCellsPerTile - 1;
    }
    const auto h = e.list("hatch");
    if (h.size() == 2)
    {
      s.hatchTx = h[0];
      s.hatchTy = h[1];
    }
    return true;
  }
  if (e.kind == "chain")
  {
    // from=x,g: the near end's last block and its deck; to=x: the far deck.
    const auto f = e.list("from");
    if (f.size() == 2)
    {
      s.chainX0 = f[0] * kCellsPerTile - 2;
      s.chainY = f[1] * kCellsPerTile - 1;
    }
    s.chainX1 = e.num("to", 0) * kCellsPerTile;
    return true;
  }
  if (e.kind == "killzone" && hasRect)
  {
    s.killY = y0 * kCellsPerTile;
    return true;
  }
  if (e.kind == "cannon" && e.hasPos)
  {
    s.cannonX = e.x * kCellsPerTile;
    s.cannonY = e.y * kCellsPerTile + 1;
    return true;
  }
  if (e.kind == "net" && e.hasPos)
  {
    FleetState::Net n;
    n.x0 = e.x * kCellsPerTile;
    n.x1 = (std::max(e.x, e.num("x1", e.x + 2)) + 1) * kCellsPerTile - 1;
    n.y = e.y * kCellsPerTile;
    n.score = e.num("score", 1000);
    s.nets.push_back(n);
    return true;
  }
  if (e.kind == "duck")
    return true; // the duck's goggles are only drawn
  return false;
}

void World::setupFleetEnemy(Enemy& en, const EntityDef& e)
{
  auto& s = mFleet;
  if (!s.on || en.kind != EnemyKind::PeckParrot)
    return;
  Parrot pa;
  pa.enemy = int(mEnemies.size()) - 1;
  pa.perchX = en.x;
  pa.perchY = en.y;
  pa.carrier = en.carrier;
  int r0 = 0, r1 = 0, ry0 = 0, ry1 = 0;
  if (e.rect("rail", r0, ry0, r1, ry1))
  {
    pa.railX0 = r0 * kCellsPerTile;
    pa.railX1 = (r1 + 1) * kCellsPerTile - en.w;
    pa.railY = ry0 * kCellsPerTile - 1; // standing on that row
  }
  en.active = true; // sinks its ship from out of view too
  s.parrots.push_back(pa);
}

void World::linkFleet()
{
  auto& s = mFleet;
  s.cannonball = mLevel->rules.find("cannonball") != std::string::npos;
  s.on = s.on || s.cannonball || mLevel->themeKey == "sunset_airships";
  if (!s.on)
    return;
  for (std::size_t i = 0; i < s.ships.size(); ++i)
  {
    Ship& sh = s.ships[i];
    sh.baseX = sh.x0 * kCellsPerTile;
    sh.baseY = sh.g * kCellsPerTile;
    if (sh.tiles)
      continue; // a steady ship: its deck is in the map
    Platform pl;
    pl.id = sh.id.empty() ? "ship" : sh.id;
    pl.fleet = true;
    pl.x = pl.prevX = sh.baseX;
    pl.y = pl.prevY = pl.startY = pl.homeY = sh.baseY;
    pl.w = sh.deckW();
    pl.h = kCellsPerTile;
    // Its travel, for the bot's map of where there is a deck to stand on.
    pl.path.emplace_back(sh.baseX - sh.driftA, sh.baseY - sh.bobA);
    pl.path.emplace_back(sh.baseX + sh.driftA, sh.baseY + (sh.looter ? 0 : sh.bobA + sh.sinkMax));
    sh.platform = int(mPlatforms.size());
    mPlatforms.push_back(pl);
    // What rides it.
    const CellBox hold{sh.baseX - 2, sh.baseY - 24, sh.deckW() + 4, 38};
    for (auto& it : mItems)
      if (hold.intersects(it.box()))
      {
        it.ship = int(i); // items come and go: they carry their ship
        it.restX = it.x;
        it.restY = it.y;
      }
    for (std::size_t k = 0; k < mBoxes.size(); ++k)
      if (hold.intersects(mBoxes[k].box()))
        sh.cargo.push_back({Cargo::Box, int(k), mBoxes[k].x, mBoxes[k].y});
    for (std::size_t k = 0; k < mCheckpoints.size(); ++k)
      if (hold.intersects(mCheckpoints[k].box()))
        sh.cargo.push_back({Cargo::Checkpoint, int(k), mCheckpoints[k].x, mCheckpoints[k].y});
    for (std::size_t k = 0; k < mProps.size(); ++k)
      if (hold.intersects(mProps[k].box()))
        sh.cargo.push_back({Cargo::Prop, int(k), mProps[k].x, mProps[k].y});
    if (s.logX >= 0 && hold.contains(s.logX, s.logY))
      sh.cargo.push_back({Cargo::Treasure, 0, s.logX, s.logY});
  }
  // Each parrot sits on the ship under it.
  for (auto& pa : s.parrots)
  {
    const Enemy& e = mEnemies[std::size_t(pa.enemy)];
    for (std::size_t i = 0; i < s.ships.size(); ++i)
    {
      const Ship& sh = s.ships[i];
      if (e.x + 1 >= sh.baseX && e.x + 1 < sh.baseX + sh.deckW())
        pa.ship = int(i);
    }
    pa.hpSeen = e.hp;
  }
  syncPlatformCollision();
  if (s.cannonball)
    reloadCannon();
}

// --- Ships ---------------------------------------------------------------------------------

void World::shipStep(Ship& sh, int dx, int dy)
{
  Platform& pl = mPlatforms[std::size_t(sh.platform)];
  // Pirates on the deck and the carrier parrot hopping along it ride too.
  std::vector<Enemy*> riders;
  for (auto& e : mEnemies)
    if (e.alive && !e.hidden && (e.kind == EnemyKind::SkyPirate || e.kind == EnemyKind::PeckParrot) &&
        standsOn(e.box(), pl))
      riders.push_back(&e);
  if (!movePlatform(pl, dx, dy))
    return;
  for (Enemy* e : riders)
  {
    e->x += dx;
    e->y += dy;
  }
}

void World::placeShip(Ship& sh, bool carry)
{
  auto& s = mFleet;
  if (sh.platform < 0)
    return;
  const float tf = float(s.t + sh.phase);
  sh.ox = sh.driftA > 0 ? int(std::lround(float(sh.driftA) * std::sin(6.2831853f * tf / float(sh.driftP)))) : 0;
  sh.oy = sh.bobA > 0 ? int(std::lround(float(sh.bobA) * std::sin(6.2831853f * tf / float(sh.bobP)))) : 0;
  sh.oy += sh.sink + (sh.dip > 0 ? kCellsPerTile : 0);
  if (sh.looter && sh.looterT >= 0)
    sh.oy += std::min(sh.looterT, kLooterFrames) / kLooterEvery;
  Platform& pl = mPlatforms[std::size_t(sh.platform)];
  const int tx = sh.baseX + sh.ox, ty = sh.baseY + sh.oy;
  if (carry)
  {
    for (int guard = 0; guard < 16 && (pl.x != tx || pl.y != ty); ++guard)
      shipStep(sh, sgn(tx - pl.x), sgn(ty - pl.y));
  }
  pl.x = tx;
  pl.y = ty;
  if (!carry)
    pl.prevX = pl.x, pl.prevY = pl.y;
  pl.hidden = sh.looter && sh.looterT >= kLooterFrames;
  // Its cargo.
  const int index = int(&sh - s.ships.data());
  for (auto& it : mItems)
    if (it.ship == index && !it.taken)
    {
      const int x = it.restX + sh.ox, y = it.restY + sh.oy;
      it.prevX = carry ? it.x : x;
      it.prevY = carry ? it.y : y;
      it.x = x;
      it.y = y;
    }
  for (const auto& c : sh.cargo)
  {
    const int x = c.x + sh.ox, y = c.y + sh.oy;
    switch (c.kind)
    {
      case Cargo::Item:
        break;
      case Cargo::Box:
        mBoxes[std::size_t(c.index)].x = x;
        mBoxes[std::size_t(c.index)].y = y;
        break;
      case Cargo::Checkpoint:
        mCheckpoints[std::size_t(c.index)].x = x;
        mCheckpoints[std::size_t(c.index)].y = y;
        break;
      case Cargo::Prop:
        mProps[std::size_t(c.index)].x = x;
        mProps[std::size_t(c.index)].y = y;
        break;
      case Cargo::Treasure:
        s.logX = x;
        s.logY = y;
        break;
      case Cargo::Enemy:
        break;
    }
  }
}

void World::updateFleetShips()
{
  auto& s = mFleet;
  if (!s.on || s.ships.empty())
    return;
  ++s.t;
  for (std::size_t i = 0; i < s.ships.size(); ++i)
  {
    Ship& sh = s.ships[i];
    if (sh.platform < 0)
      continue;
    // A parrot on the balloon weighs it down; it comes back up without one.
    bool perched = false;
    for (const auto& pa : s.parrots)
      perched = perched || (pa.ship == int(i) && !pa.carrier && pa.state == 0 && mEnemies[std::size_t(pa.enemy)].alive);
    const int want = perched ? sh.sinkMax : 0;
    if (sh.sink != want)
    {
      if (++sh.sinkT >= kShipSinkEvery)
      {
        sh.sinkT = 0;
        sh.sink += sgn(want - sh.sink);
        if (perched && isOnScreen(mPlatforms[std::size_t(sh.platform)].box(), 4))
          playSound(Sfx::BalloonHiss);
      }
    }
    else
      sh.sinkT = 0;
    if (sh.dip > 0)
      --sh.dip;
    if (sh.looter && sh.looterT >= 0 && sh.looterT < kLooterFrames)
      ++sh.looterT;
    placeShip(sh, true);
  }
  // Now and then the engines of a ship in view (sound only).
  if (s.t % 240 == 0)
    for (const auto& sh : s.ships)
      if (sh.platform >= 0 && isOnScreen(mPlatforms[std::size_t(sh.platform)].box(), 0))
      {
        playSound(Sfx::EngineDrone);
        break;
      }
}

int World::shipUnder() const
{
  const auto& p = mPlayer;
  if (p.state != PlayerState::OnGround && p.state != PlayerState::Recovering)
    return -1;
  const CellBox b = p.box();
  for (std::size_t i = 0; i < mFleet.ships.size(); ++i)
  {
    const Ship& sh = mFleet.ships[i];
    if (sh.platform >= 0)
    {
      if (standsOn(b, mPlatforms[std::size_t(sh.platform)]))
        return int(i);
    }
    else if (b.bottom() + 1 == sh.baseY && b.right() >= sh.baseX && b.x < sh.baseX + sh.deckW())
      return int(i);
  }
  return -1;
}

// --- Update ----------------------------------------------------------------------------------

void World::updateFleet(const PlayerInput& input)
{
  auto& s = mFleet;
  auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  if (s.cannonball)
    return;
  // The swing's line goes with the runner letting go.
  if (s.ring >= 0 && !(p.state == PlayerState::Swing && p.vine >= 0))
  {
    s.ring = -1;
    mVines.clear();
    p.vine = -1;
  }
  // Below the clouds' top there is nothing to catch you.
  if (alive && p.y >= s.killY && p.state != PlayerState::Swing)
  {
    playSound(Sfx::Death);
    p.state = PlayerState::Dying;
    p.deathPhase = 3;
    p.frames = 0;
    p.hidden = true;
    ++mStats.deaths;
    if (!mSimulation && std::getenv("GR_PLANNER_DEBUG"))
      std::fprintf(stderr, "death (clouds) at %d,%d frame %d\n", p.x, p.y, mStats.frames);
    return;
  }
  const int under = alive ? shipUnder() : -1;
  // Rocco's hard landings dip a small ship.
  if (p.state == PlayerState::Jumping || p.state == PlayerState::Falling)
    s.airTop = s.airTop < 0 ? p.y : std::min(s.airTop, p.y);
  else
  {
    if (s.airTop >= 0 && under >= 0 && mCharacterIndex == 1 && p.y - s.airTop >= kDipFall)
    {
      Ship& sh = s.ships[std::size_t(under)];
      if (sh.platform >= 0 && sh.deckW() <= kDipDeck && !sh.looter)
      {
        sh.dip = kDipFrames;
        playSound(Sfx::DeckDip);
      }
    }
    s.airTop = -1;
  }
  // The bomb bay's hatch opens from inside.
  if (!s.hatchOpen && s.hatchTx >= 0 && alive && p.state == PlayerState::OnGround && p.y + 1 == s.bayY &&
      p.x >= s.bayX0 - 1 && p.x + 2 <= s.bayX1 + 1)
  {
    s.hatchOpen = true;
    mMap.setBlock(s.hatchTx, s.hatchTy, Tile::Ladder);
    showMessage("THE BOMB BAY - SECRET FOUND");
    playSound(Sfx::BayHatch);
  }
  // The Looter goes down once boarded.
  if (under >= 0)
  {
    Ship& sh = s.ships[std::size_t(under)];
    if (sh.looter && sh.looterT < 0)
    {
      sh.looterT = 0;
      showMessage("THE LOOTER IS GOING DOWN!");
      playSound(Sfx::LooterSink);
    }
  }
  // Easter egg: stand still by a hull's name for a moment.
  for (std::size_t i = 0; i < s.ships.size(); ++i)
  {
    Ship& sh = s.ships[i];
    const bool still = int(i) == under && !input.left && !input.right && !input.jump.pressed && !input.fire.pressed;
    sh.still = still ? sh.still + 1 : 0;
    if (sh.still == kNameStill && !sh.named && !sh.name.empty())
    {
      sh.named = true;
      showMessage(sh.name + " - EPISODE 1 SENDS ITS REGARDS");
      playSound(Sfx::Chime);
    }
  }
  // The captain's log.
  if (alive && s.logX >= 0 && !s.logTaken && boxAt(s.logX, s.logY, 2, 2).intersects(p.box()))
  {
    s.logTaken = true;
    addScore(s.logScore, cellCenter(boxAt(s.logX, s.logY, 2, 2)));
    showMessage("THE CAPTAIN'S LOG - SECRET FOUND");
    playSound(Sfx::Gem);
    playSound(Sfx::Chime);
  }
  // Sky Pirates jet in as the runner passes their marks.
  for (auto& pi : s.pirates)
  {
    Enemy& e = mEnemies[std::size_t(pi.enemy)];
    if (pi.state != 0 || !e.alive || !alive || p.x < pi.triggerX)
      continue;
    pi.state = 1;
    e.hidden = false;
    e.active = true;
    e.x = e.prevX = p.x + 44;
    e.y = e.prevY = p.y - 16;
    e.dir = -1;
    // Land a little ahead of the runner, where there is deck.
    pi.tx = p.x + 10;
    pi.ty = p.y;
    for (int dx = 10; dx >= 4; --dx)
      if (mMap.solidTop(p.x + dx + 1, p.y + 1))
      {
        pi.tx = p.x + dx;
        break;
      }
    playSound(Sfx::PirateJet);
    showMessage("SKY PIRATE!");
  }
  // Balloon Mines rise out of the clouds.
  for (auto& m : s.spawns)
  {
    m.enemies.erase(std::remove_if(m.enemies.begin(), m.enemies.end(),
                      [&](int k) { return !mEnemies[std::size_t(k)].alive; }),
      m.enemies.end());
    if (!alive || p.x + 40 < m.x0 || p.x > m.x1 + 20)
      continue;
    if (++m.t < m.every || int(m.enemies.size()) >= m.max)
      continue;
    m.t = 0;
    m.seed = (m.seed * 1103 + 12345) % 30011;
    const int lo = std::max(m.x0, p.x - 12), hi = std::min(m.x1, p.x + 30);
    if (hi <= lo)
      continue;
    const int def = enemyIndex("balloon_mine");
    if (def < 0)
      continue;
    spawnEnemy(def, lo + m.seed % (hi - lo), m.y);
    Enemy& en = mEnemies.back();
    en.active = true;
    m.enemies.push_back(int(mEnemies.size()) - 1);
    s.mines.push_back({int(mEnemies.size()) - 1, -1, 0});
  }
  s.mines.erase(std::remove_if(s.mines.begin(), s.mines.end(),
                  [&](const Mine& m) { return !mEnemies[std::size_t(m.enemy)].alive; }),
    s.mines.end());
  // Yanked enemies come in on the line.
  for (auto& y : s.yanks)
  {
    Enemy& e = mEnemies[std::size_t(y.enemy)];
    if (!e.alive)
    {
      y.frames = 0;
      continue;
    }
    for (int k = 0; k < kYankSpeed; ++k)
    {
      const int toward = sgn((p.x + 1) - (e.x + e.w / 2));
      if (toward == 0 || mMap.overlapsSolid(boxAt(e.x + toward, e.y, e.w, e.h)))
        break;
      e.x += toward;
    }
    --y.frames;
  }
  s.yanks.erase(std::remove_if(s.yanks.begin(), s.yanks.end(), [](const Yank& y) { return y.frames <= 0; }),
    s.yanks.end());
  // Shrapnel.
  for (auto& sh : s.shards)
  {
    sh.x += sh.vx;
    sh.y += sh.vy;
    sh.vy += 0.1f;
    --sh.life;
  }
  s.shards.erase(std::remove_if(s.shards.begin(), s.shards.end(), [](const Shard& h) { return h.life <= 0; }),
    s.shards.end());
}

bool World::fleetYanked(const Enemy& e) const
{
  for (const auto& y : mFleet.yanks)
    if (&mEnemies[std::size_t(y.enemy)] == &e)
      return true;
  return false;
}

void World::resetFleet()
{
  auto& s = mFleet;
  s.ring = -1;
  s.yanks.clear();
  s.airTop = -1;
  mVines.clear();
  for (auto& sh : s.ships)
  {
    sh.dip = 0;
    if (sh.looter)
      sh.looterT = -1;
    placeShip(sh, false);
  }
  syncPlatformCollision();
  // Past the third checkpoint the gun comes back with 20 shots.
  auto& p = mPlayer;
  int reached = 0;
  for (const auto& cp : mCheckpoints)
    reached += cp.active;
  const int gun = protoIndex("grapple_gun");
  if (reached >= 3 && gun >= 0 && !(p.weapon == Weapon::Proto && p.proto == gun && p.ammo >= 20))
  {
    equip(Weapon::Proto, gun); // a spare grapple comes back with its own shots
    p.ammo = std::max(p.ammo, 20);
    p.charge = 0;
  }
}

// --- The Grapple Gun ---------------------------------------------------------------------

void World::updateGrapple(const Button& fire)
{
  auto& p = mPlayer;
  auto& s = mFleet;
  if (!fire.triggered || p.ammo <= 0 || (p.shotCooldown > 0 && p.turbo == 0))
    return;
  for (const auto& pr : mProjectiles)
    if (pr.alive && pr.kind == ShotKind::Proto && pr.proto == p.proto)
      return; // one hook out at a time
  const ProtoDef& def = protoDef(p.proto);
  // Forward, up and forward (45 degrees), or straight up.
  float vx = float(p.facing), vy = 0.0f;
  if (s.up && s.mvX != 0)
  {
    vx = 0.7071f * float(s.mvX);
    vy = -0.7071f;
    p.facing = s.mvX;
  }
  else if (s.up)
  {
    vx = 0.0f;
    vy = -1.0f;
  }
  Projectile pr;
  pr.kind = ShotKind::Proto;
  pr.proto = p.proto;
  pr.precise = true;
  pr.w = pr.h = 1;
  pr.speed = int(kHookSpeed);
  pr.damage = def.damage;
  pr.range = kHookRange;
  pr.fx = float(p.x) + 1.0f;
  pr.fy = float(p.y - Player::kHeight + 1);
  pr.vx = vx;
  pr.vy = vy;
  pr.dx = vx < -0.1f ? -1 : (vx > 0.1f ? 1 : 0);
  pr.dy = vy < -0.1f ? -1 : 0;
  pr.x = pr.prevX = int(std::floor(pr.fx));
  pr.y = pr.prevY = int(std::floor(pr.fy));
  mProjectiles.push_back(pr);
  p.shotCooldown = def.cooldown;
  p.recoil = 1;
  p.muzzleTicks = 6;
  p.muzzleStance = p.stance;
  playSound(Sfx::GrappleFire);
  if (--p.ammo == 0)
  {
    p.weapon = Weapon::Normal;
    p.proto = -1;
    showMessage("OUT OF HOOKS - BACK TO THE BLASTER");
  }
}

bool World::grappleOut() const
{
  for (const auto& pr : mProjectiles)
    if (pr.alive && pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::GrappleGun))
      return true;
  return false;
}

void World::hookRing(int i)
{
  auto& s = mFleet;
  auto& p = mPlayer;
  const Ring& rg = s.rings[std::size_t(i)];
  const float hx = float(p.x) + 1.5f, hy = float(p.y - Player::kHeight + 1);
  const float dx = hx - float(rg.x), dy = hy - float(rg.y);
  const int len = std::clamp(int(std::lround(std::sqrt(dx * dx + dy * dy))), kLineMin, kLineMax);
  float a = dy > 0.5f ? std::atan2(dx, dy) / kRad : (dx >= 0.0f ? 80.0f : -80.0f);
  Vine v;
  v.id = "ring";
  v.ring = true;
  v.ax = rg.x;
  v.ay = rg.y;
  v.len = len;
  v.period = ringPeriod(len);
  v.amp = v.rest = 0;
  v.amp = std::clamp(int(std::lround(std::fabs(a))), 8, kRingAmpMax);
  v.rest = 8;
  v.t = a >= 0.0f ? 0 : v.period / 2;
  // Hanging must not put the runner in the deck under the ring: a shorter
  // line, or no catch at all.
  auto blocked = [&](int at) {
    float px = 0.0f, py = 0.0f;
    v.point(at, px, py);
    return mMap.overlapsSolid(boxAt(int(std::lround(px - 1.5f)), int(std::lround(py)) + 5, Player::kWidth, 6));
  };
  while (blocked(v.len) && v.len > kLineMin)
  {
    --v.len;
    v.period = ringPeriod(v.len);
  }
  if (blocked(v.len))
    return;
  mVines.clear();
  mVines.push_back(v);
  s.ring = i;
  p.state = PlayerState::Swing;
  p.vine = 0;
  p.vineAt = v.len;
  p.frames = 0;
  p.somersault = -1;
  p.fling = 0;
  p.vineArc = false;
  p.fromLadder = false;
  mLaunch = 0;
  setVisual(PlayerVisual::Hanging);
  playSound(Sfx::RopeCreak);
  placeOnVine();
}

void World::setRingLine(int len)
{
  auto& p = mPlayer;
  Vine& v = mVines[std::size_t(p.vine)];
  len = std::clamp(len, kLineMin, kLineMax);
  if (len == p.vineAt)
    return;
  const int period = ringPeriod(len);
  v.t = v.t * period / v.period;
  v.period = period;
  v.len = len;
  p.vineAt = len;
}

void World::updateRingSwing(int mvX, int mvY, const Button& jump)
{
  auto& p = mPlayer;
  Vine& v = mVines[std::size_t(p.vine)];
  setVisual(PlayerVisual::Hanging);
  if (jump.triggered)
  {
    releaseRing(true);
    return;
  }
  // The winch takes up a long line; up and down reel it in and out.
  if (p.vineAt > kLineWinch)
    setRingLine(std::max(kLineWinch, p.vineAt - 2));
  else if (mvY != 0 && p.oddFrame)
    setRingLine(p.vineAt + mvY);
  if (mvX != 0)
  {
    p.facing = mvX;
    if (mvX == v.swingDir())
      ++v.pump;
  }
  placeOnVine();
}

void World::releaseRing(bool launch)
{
  auto& p = mPlayer;
  auto& s = mFleet;
  Vine& v = mVines[std::size_t(p.vine)];
  float x0 = 0.0f, y0 = 0.0f;
  v.point(p.vineAt, x0, y0);
  const float a1 = v.amp * std::cos(6.2831853f * float(v.t + 1) / float(v.period)) * kRad;
  const float vx = float(v.ax) + float(p.vineAt) * std::sin(a1) - x0;
  const int dir = std::fabs(vx) < 0.4f ? 0 : (vx > 0.0f ? 1 : -1);
  p.vine = -1;
  s.ring = -1;
  mVines.clear();
  p.y -= 1;
  if (mMap.overlapsSolid(p.box()))
    ++p.y;
  p.somersault = -1;
  p.frames = 0;
  if (launch)
  {
    p.state = PlayerState::Jumping;
    p.fromLadder = true;
    p.vineArc = true;
    p.fling = (std::fabs(vx) >= 1.2f ? 2 : 1) * dir;
    if (dir != 0)
      p.facing = dir;
    p.jumpRequested = false;
    setVisual(PlayerVisual::Jumping);
    playSound(Sfx::Jump);
    playSound(Sfx::Whoosh);
    return;
  }
  p.state = PlayerState::Falling;
  p.vineArc = false;
  p.fling = dir;
  setVisual(PlayerVisual::Falling);
}

// 1: the shot is spent here.
bool World::shotAtFleet(Projectile& pr, const CellBox& hit)
{
  (void)hit;
  auto& s = mFleet;
  if (pr.kind != ShotKind::Proto || pr.proto != int(ProtoId::GrappleGun))
    return false;
  const auto& p = mPlayer;
  if (p.state == PlayerState::Dying || p.state == PlayerState::Teleporting || p.cart >= 0 || p.vehicle >= 0)
    return false;
  // The hook's path this step, against each ring.
  const float x1 = pr.fx + 0.5f, y1 = pr.fy + 0.5f;
  const float x0 = x1 - pr.vx, y0 = y1 - pr.vy;
  for (std::size_t i = 0; i < s.rings.size(); ++i)
  {
    if (int(i) == s.ring)
      continue;
    const float rx = float(s.rings[i].x) + 0.5f, ry = float(s.rings[i].y) + 0.5f;
    const float ex = x1 - x0, ey = y1 - y0;
    const float len2 = std::max(0.0001f, ex * ex + ey * ey);
    const float u = std::clamp(((rx - x0) * ex + (ry - y0) * ey) / len2, 0.0f, 1.0f);
    const float cx = x0 + ex * u - rx, cy = y0 + ey * u - ry;
    if (cx * cx + cy * cy > kHookCatch * kHookCatch)
      continue;
    pr.alive = false;
    if (p.state == PlayerState::Swing && s.ring >= 0)
    {
      // From one ring to the next: the old line goes.
      mVines.clear();
      mPlayer.vine = -1;
    }
    hookRing(int(i));
    playSound(Sfx::GrappleLatch);
    return true;
  }
  return false;
}

// 1: the shot is spent, 0: as usual.
int World::shotAtFleetEnemy(Projectile& pr, Enemy& e)
{
  // A Balloon Mine shot from range goes off at once.
  if (e.kind == EnemyKind::BalloonMine)
  {
    burstMine(e);
    return 1;
  }
  if (pr.kind != ShotKind::Proto || pr.proto != int(ProtoId::GrappleGun) || !lightEnemy(e.kind))
    return 0;
  // The hook yanks a light enemy toward the runner.
  damageEnemy(e, pr.damage);
  if (e.alive)
  {
    const auto& p = mPlayer;
    mFleet.yanks.push_back({int(&e - mEnemies.data()), kYankFrames, sgn((p.x + 1) - (e.x + e.w / 2))});
    for (auto& pa : mFleet.parrots)
      if (&mEnemies[std::size_t(pa.enemy)] == &e && pa.state != 4)
        pa.state = 3; // shaken off its perch
    playSound(Sfx::GrappleYank);
  }
  return 1;
}

// --- Enemies -------------------------------------------------------------------------------

void World::updateSkyPirate(Enemy& e, const EnemyDef& def)
{
  auto& s = mFleet;
  Pirate* pi = nullptr;
  for (auto& f : s.pirates)
    if (&mEnemies[std::size_t(f.enemy)] == &e)
      pi = &f;
  if (!pi || pi->state == 0)
    return;
  const auto& p = mPlayer;
  if (pi->state == 1)
  {
    // Jetting in: straight for its landing spot.
    for (int k = 0; k < kPirateFly; ++k)
    {
      e.x += sgn(pi->tx - e.x);
      e.y += sgn(pi->ty - e.y);
    }
    e.dir = pi->tx < e.x ? -1 : 1;
    if (std::abs(e.x - pi->tx) <= 1 && std::abs(e.y - pi->ty) <= 1)
      pi->state = 2;
    return;
  }
  // Gone over the side: into the clouds, for full score.
  if (e.y >= s.killY)
  {
    damageEnemy(e, e.hp);
    return;
  }
  if (fleetYanked(e))
    return;
  if (!mMap.onSolidGround(e.box()))
  {
    pi->fall = std::min(pi->fall + 1, 4);
    mMap.moveVertically(e.x, e.y, e.w, e.h, pi->fall >= 3 ? 2 : 1);
    return;
  }
  pi->fall = 0;
  if (e.cool > 0)
    --e.cool;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  const CellBox pb = p.hitBox(), b = e.box();
  const int dx = (pb.x + 1) - (b.x + 1);
  const bool level = std::abs(pb.bottom() - b.bottom()) <= 4;
  if (e.tell > 0)
  {
    if (--e.tell == 0)
    {
      if (pi->attack == 1)
      {
        const CellBox blade = e.dir > 0 ? CellBox{b.right() + 1, b.y, kCutlassReach, b.h}
                                        : CellBox{b.x - kCutlassReach, b.y, kCutlassReach, b.h};
        if (alive && blade.intersects(pb))
          hurtPlayer(1);
        playSound(Sfx::CutlassSlash);
      }
      else
      {
        Projectile pr;
        pr.kind = ShotKind::Enemy;
        pr.w = 2;
        pr.h = 1;
        pr.speed = 1;
        pr.damage = 1;
        pr.dx = e.dir;
        pr.dy = 0;
        pr.x = pr.prevX = e.dir > 0 ? b.right() + 1 : b.x - 2;
        pr.y = pr.prevY = b.y + 1;
        pr.range = kPistolRange + 10;
        mProjectiles.push_back(pr);
        playSound(Sfx::FlintShot);
      }
      e.cool = def.cooldown;
      pi->attack = 0;
    }
    return;
  }
  if (alive && level && e.cool == 0 && isOnScreen(b, 0))
  {
    const int gap = dx > 0 ? pb.x - b.right() - 1 : b.x - pb.right() - 1;
    if (gap <= kCutlassReach)
    {
      e.dir = dx < 0 ? -1 : 1;
      pi->attack = 1;
      e.tell = def.tell;
      playSound(Sfx::CutlassDraw);
      return;
    }
    if (std::abs(dx) <= kPistolRange)
    {
      e.dir = dx < 0 ? -1 : 1;
      pi->attack = 2;
      e.tell = 12;
      playSound(Sfx::FlintCock);
      return;
    }
  }
  // Chase along the deck, but not off it.
  if (!alive || std::abs(dx) <= 3 || e.timer % std::max(1, def.stepEvery) != 0)
    return;
  e.dir = dx < 0 ? -1 : 1;
  const int aheadX = e.dir > 0 ? b.right() + 1 : b.left() - 1;
  if (mMap.solidTop(aheadX, b.bottom() + 1) && !mMap.overlapsSolid(boxAt(e.x + e.dir, e.y, e.w, e.h)))
    e.x += e.dir;
}

void World::updatePeckParrot(Enemy& e, const EnemyDef& def)
{
  auto& s = mFleet;
  Parrot* pa = nullptr;
  for (auto& f : s.parrots)
    if (&mEnemies[std::size_t(f.enemy)] == &e)
      pa = &f;
  if (!pa)
    return;
  const auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  int ox = 0, oy = 0;
  if (pa->ship >= 0)
  {
    ox = s.ships[std::size_t(pa->ship)].ox;
    oy = s.ships[std::size_t(pa->ship)].oy;
  }
  const int homeX = pa->perchX + ox, homeY = pa->perchY + oy;
  // Shot but not down: shooed off its perch.
  if (e.hp < pa->hpSeen && (pa->state == 0 || pa->state == 4))
  {
    pa->state = 1, pa->t = def.tell;
    playSound(Sfx::ParrotSquawk);
  }
  pa->hpSeen = e.hp;
  if (fleetYanked(e))
    return;
  // The carrier: hops along the rail at a runner close by, and pecks.
  if (pa->carrier && pa->state == 0 && alive && pa->railX1 > pa->railX0 && p.x >= pa->railX0 - 2 &&
      p.x <= pa->railX1 + 2 && std::abs(p.y - pa->railY) <= 12)
  {
    pa->state = 4;
    pa->hopT = 0;
    e.y = pa->railY;
  }
  switch (pa->state)
  {
    case 0: // perched, pecking at the balloon
      e.x = homeX;
      e.y = homeY;
      e.dir = (p.x + 1 < e.x) ? -1 : 1;
      e.tell = 0;
      break;
    case 1: // squawk and bob, then a dive
      e.tell = pa->t;
      if (--pa->t <= 0)
      {
        pa->state = 2;
        pa->t = kParrotDive;
        pa->fx = float(e.x);
        pa->fy = float(e.y);
        const float tx = float(p.x), ty = float(p.y - 2);
        const float dx = tx - pa->fx, dy = ty - pa->fy, d = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
        pa->vx = 2.0f * dx / d;
        pa->vy = 2.0f * dy / d;
        e.dir = dx < 0.0f ? -1 : 1;
        playSound(Sfx::ParrotDive);
      }
      break;
    case 2: // the dive
      e.tell = 0;
      pa->fx += pa->vx;
      pa->fy += pa->vy;
      e.x = int(std::lround(pa->fx));
      e.y = int(std::lround(pa->fy));
      if (--pa->t <= 0 || mMap.overlapsSolid(e.box()) || e.y >= s.killY - 4)
        pa->state = 3;
      break;
    case 3: // back up to the perch
    {
      e.tell = 0;
      const int tx = pa->carrier && pa->railX1 > pa->railX0 ? std::clamp(e.x, pa->railX0, pa->railX1) : homeX;
      const int ty = pa->carrier && pa->railX1 > pa->railX0 ? pa->railY : homeY;
      e.x += sgn(tx - e.x);
      e.y += sgn(ty - e.y);
      e.dir = tx < e.x ? -1 : 1;
      if (e.x == tx && e.y == ty)
        pa->state = pa->carrier && pa->railX1 > pa->railX0 ? 4 : 0;
      break;
    }
    case 4: // the carrier on the rail
    {
      e.tell = 0;
      if (!alive || p.x < pa->railX0 - 8 || p.x > pa->railX1 + 8)
        break;
      const int dx = (p.x + 1) - (e.x + 1);
      e.dir = dx < 0 ? -1 : 1;
      if (++pa->hopT >= kParrotHop && std::abs(dx) > 1)
      {
        pa->hopT = 0;
        e.x = std::clamp(e.x + 4 * e.dir, pa->railX0, pa->railX1);
        if (isOnScreen(e.box(), 0))
          playSound(Sfx::ParrotHop);
      }
      e.dive = pa->hopT < 6 ? 1 : 0; // mid-hop
      break;
    }
    default:
      break;
  }
}

void World::burstMine(Enemy& e)
{
  auto& s = mFleet;
  if (!e.alive)
    return;
  const auto& p = mPlayer;
  const float cx = float(e.x) + float(e.w) / 2.0f, cy = float(e.y) - float(e.h) / 2.0f;
  for (int k = 0; k < 8; ++k)
  {
    const float a = float(k) * 0.785398f;
    s.shards.push_back({cx, cy, std::cos(a) * 1.0f, std::sin(a) * 1.0f, 8});
  }
  const float px = float(p.x) + 1.5f, py = float(p.y) - 2.0f;
  const bool vulnerable = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  if (vulnerable && std::fabs(px - cx) <= float(kMineRadius) + 1.5f && std::fabs(py - cy) <= float(kMineRadius) + 2.5f)
    hurtPlayer(1);
  damageEnemy(e, e.hp);
  e.alive = false;
  playSound(Sfx::MinePop);
  explodeVisual(cellCenter(e.box()));
}

void World::explodeVisual(Vec2 at)
{
  burst(at, rgb(255, 220, 120), rgb(255, 90, 40), 18, 2.0f);
  flashAt(at, 90.0f, rgb(255, 150, 60), 14);
  playSound(Sfx::Explosion);
}

void World::updateBalloonMine(Enemy& e, const EnemyDef& def)
{
  (void)def;
  Mine* m = nullptr;
  for (auto& f : mFleet.mines)
    if (&mEnemies[std::size_t(f.enemy)] == &e)
      m = &f;
  if (!m)
    return;
  if (m->fuse >= 0)
  {
    e.tell = m->fuse;
    if (--m->fuse < 0)
      burstMine(e);
    return;
  }
  if (++m->sub >= kMineRise)
  {
    m->sub = 0;
    --e.y;
  }
  if (e.y < kMineTop)
  {
    e.alive = false; // gone up out of the level (no score)
    return;
  }
  const auto& p = mPlayer;
  const CellBox near{e.x - kMineNear, e.y - e.h + 1 - kMineNear, e.w + 2 * kMineNear, e.h + 2 * kMineNear};
  if (p.state != PlayerState::Dying && near.intersects(p.hitBox()))
  {
    m->fuse = kMineFuse;
    playSound(Sfx::MineFuse);
  }
}

// --- Human Cannonball ----------------------------------------------------------------------

void World::reloadCannon()
{
  auto& s = mFleet;
  auto& p = mPlayer;
  s.flying = false;
  s.reload = 0;
  p.x = p.prevX = s.cannonX;
  p.y = p.prevY = s.cannonY;
  p.state = PlayerState::OnGround;
  p.hidden = true;
  p.facing = 1;
  setVisual(PlayerVisual::Standing);
}

void World::updateCannonball(int mvX, int mvY, const PlayerInput& in)
{
  auto& s = mFleet;
  auto& p = mPlayer;
  if (p.state == PlayerState::Dying || p.state == PlayerState::Teleporting)
    return;
  if (s.reload > 0)
  {
    if (--s.reload == 0)
      reloadCannon();
    return;
  }
  if (!s.flying)
  {
    // In the barrel: aim with up and down, fire with jump or fire.
    p.hidden = true;
    if (mvY != 0 && !s.aimHeld)
      s.angle = std::clamp(s.angle - 15 * mvY, 30, 60);
    s.aimHeld = mvY != 0;
    if (in.jump.triggered || in.fire.triggered)
    {
      s.flying = true;
      s.fx = float(p.x);
      s.fy = float(p.y);
      s.vx = kCannonSpeed * std::cos(float(s.angle) * kRad);
      s.vy = -kCannonSpeed * std::sin(float(s.angle) * kRad);
      p.hidden = false;
      p.state = PlayerState::Jumping;
      setVisual(PlayerVisual::Jumping);
      playSound(Sfx::CannonBoom);
      burst({(float(p.x) + 1.5f) * kCellSize, (float(p.y) - 2.0f) * kCellSize}, rgb(255, 230, 160), rgb(160, 150, 140), 14,
        2.0f, false);
    }
    return;
  }
  // In flight: gravity, a little steering.
  s.vy += kCannonGravity;
  s.vx += kCannonSteer * float(mvX);
  if (mvX != 0)
    p.facing = mvX;
  const float nx = s.fx + s.vx, ny = s.fy + s.vy;
  const int oldY = p.y;
  int x = p.x;
  const int stepX = int(std::lround(nx)) - p.x;
  if (stepX != 0 && mMap.moveHorizontally(x, p.y, Player::kWidth, p.height(), stepX) != MoveResult::Completed)
    s.vx = 0.0f;
  p.x = x;
  s.fx = s.vx == 0.0f ? float(p.x) : nx;
  const int stepY = int(std::lround(ny)) - p.y;
  bool landed = false;
  if (stepY != 0)
  {
    // A net catches you as you come down through it.
    for (const auto& n : s.nets)
      if (stepY > 0 && oldY < n.y && oldY + stepY >= n.y && p.x + 1 >= n.x0 && p.x + 1 <= n.x1)
      {
        p.y = n.y - 1;
        mStats.score += n.score;
        ++s.hits;
        showMessage(n.score >= 5000 ? "BULLSEYE!" : "IN THE NET!");
        playSound(Sfx::NetBoing);
        playSound(Sfx::Applause);
        landed = true;
        break;
      }
    if (!landed && mMap.moveVertically(p.x, p.y, Player::kWidth, p.height(), stepY) != MoveResult::Completed)
    {
      if (stepY > 0)
      {
        landed = true;
        playSound(Sfx::Thud);
      }
      else
        s.vy = 0.0f;
    }
  }
  s.fy = landed ? float(p.y) : ny;
  if (landed || p.y > mMap.height() - 2)
  {
    s.flying = false;
    s.reload = kCannonReload;
    p.state = PlayerState::OnGround;
    setVisual(PlayerVisual::Standing);
    return;
  }
  p.state = s.vy < 0.0f ? PlayerState::Jumping : PlayerState::Falling;
  p.frames = 0;
  setVisual(s.vy < 0.0f ? PlayerVisual::Jumping : PlayerVisual::Falling);
}

// --- Saves ---------------------------------------------------------------------------------

bool World::fleetCanSave() const
{
  const auto& s = mFleet;
  if (s.cannonball)
    return !s.flying && s.reload == 0;
  if (s.ring >= 0 || !s.yanks.empty() || !s.mines.empty() || grappleOut())
    return false;
  for (const auto& pi : s.pirates)
    if (pi.state == 1)
      return false;
  for (const auto& pa : s.parrots)
    if (pa.state == 1 || pa.state == 2)
      return false;
  for (const auto& sh : s.ships)
    if (sh.dip > 0)
      return false;
  return true;
}

std::vector<int> World::fleetSave() const
{
  const auto& s = mFleet;
  std::vector<int> v{s.t, s.logTaken, int(s.ships.size()), int(s.pirates.size()), int(s.parrots.size()), s.hatchOpen};
  for (const auto& sh : s.ships)
  {
    v.push_back(sh.sink);
    v.push_back(sh.looterT);
    v.push_back(sh.named);
  }
  for (const auto& pi : s.pirates)
    v.push_back(pi.state);
  for (const auto& pa : s.parrots)
    v.push_back(pa.state);
  v.push_back(s.angle);
  return v;
}

bool World::validFleetSave(const std::vector<int>& v) const
{
  const auto& s = mFleet;
  return v.size() == 7 + 3 * s.ships.size() + s.pirates.size() + s.parrots.size() &&
    v[2] == int(s.ships.size()) && v[3] == int(s.pirates.size()) && v[4] == int(s.parrots.size());
}

void World::loadFleet(const std::vector<int>& v)
{
  auto& s = mFleet;
  s.t = v[0];
  s.logTaken = v[1] != 0;
  if (v[5] != 0 && !s.hatchOpen && s.hatchTx >= 0)
    mMap.setBlock(s.hatchTx, s.hatchTy, Tile::Ladder);
  s.hatchOpen = v[5] != 0;
  std::size_t k = 6;
  for (auto& sh : s.ships)
  {
    sh.sink = v[k++];
    sh.looterT = v[k++];
    sh.named = v[k++] != 0;
    sh.dip = 0;
  }
  for (auto& pi : s.pirates)
    pi.state = v[k++];
  for (auto& pa : s.parrots)
    pa.state = v[k++];
  s.angle = v[k];
  s.ring = -1;
  s.yanks.clear();
  s.mines.clear();
  for (auto& m : s.spawns)
    m.enemies.clear();
  for (std::size_t i = 0; i < s.ships.size(); ++i)
  {
    Ship& sh = s.ships[i];
    if (sh.platform < 0)
      continue;
    placeShip(sh, false);
    // Items come back where they were saved: tag them to their ship again.
    const CellBox hold{sh.baseX + sh.ox - 2, sh.baseY + sh.oy - 24, sh.deckW() + 4, 38};
    for (auto& it : mItems)
      if (it.ship < 0 && hold.intersects(it.box()))
      {
        it.ship = int(i);
        it.restX = it.x - sh.ox;
        it.restY = it.y - sh.oy;
      }
  }
  syncPlatformCollision();
}

} // namespace gr
