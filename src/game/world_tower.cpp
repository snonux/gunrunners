// Level 35, The Transmitter: the turning tower (flags=wrapx), its doors,
// Mast Climbers, Relay Drones and Feedback Nodes, the Satellite Swarm, the
// Halcyon Titan on the top deck, the brochure and the eggs, and the Copycat
// bonus (rules=copycat). Drawing is in world_tower_draw.cpp.
//
// The map's x wraps. Rather than teach every system about the seam, the
// world turns: whenever the runner is more than kTowerKeep cells from the
// map's middle, every tile, actor and the camera move round by an even
// number of cells so that the runner is in the middle again. The seam is
// then always half a lap away, on the back of the tower, where only things
// that circle the mast (Relay Drones, the Titan in its orbit, their shots)
// ever cross it; wrapActors() brings those back into the map.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace gr
{

namespace
{

int sgn(int v) { return (v > 0) - (v < 0); }

int wrapIn(int v, int n) { return n > 0 ? ((v % n) + n) % n : v; }

template <typename T>
void rotateRows(std::vector<T>& v, int w, int h, int by)
{
  if (w <= 0 || by % w == 0 || int(v.size()) < w * h)
    return;
  std::vector<T> row(static_cast<std::size_t>(w));
  for (int y = 0; y < h; ++y)
  {
    for (int x = 0; x < w; ++x)
      row[std::size_t(wrapIn(x + by, w))] = v[std::size_t(y * w + x)];
    std::copy(row.begin(), row.end(), v.begin() + std::ptrdiff_t(y * w));
  }
}

} // namespace

int World::towerDx(int from, int to) const
{
  const int n = mMap.width();
  int d = to - from;
  if (!mTower.wrap || n <= 0)
    return d;
  d = wrapIn(d, n);
  return d >= n / 2 ? d - n : d;
}

bool World::titanFight() const
{
  const auto& t = mTower.titan;
  return t.on && t.phase != TitanPhase::Waiting && t.phase != TitanPhase::Done;
}

int World::titanHp() const { return mTower.titan.on ? mTower.titan.total() : 0; }

// --- Setup ---------------------------------------------------------------------------------

bool World::setupTowerEntity(const EntityDef& e)
{
  const bool copy = mLevel->rules.find("copycat") != std::string::npos;
  if (mLevel->themeKey != "broadcast_mast" && !copy)
    return false;
  auto& s = mTower;
  s.on = true;
  s.copycat = copy;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  const bool hasRect = e.rect("rect", x0, y0, x1, y1);
  if (e.kind == "door" && e.hasPos)
  {
    TowerDoor d;
    d.id = e.id;
    d.to = e.str("to");
    d.x = e.x * kCellsPerTile;
    d.y = e.y * kCellsPerTile + 1;
    s.doors.push_back(d);
    return true;
  }
  if (e.kind == "route")
  {
    for (const auto& [px, py] : e.path("path"))
      s.route.push_back({px * kCellsPerTile, py * kCellsPerTile - 1});
    return true;
  }
  if (e.kind == "spawn" && e.str("kind") == "mast_climber")
  {
    // Out of sight until the runner climbs the ladder at `ladder=x`.
    const int def = enemyIndex("mast_climber");
    if (def < 0 || !e.hasPos)
      return true;
    spawnEnemy(def, e.x * kCellsPerTile, e.y * kCellsPerTile + 1);
    mEnemies.back().hidden = true;
    s.waits.push_back({int(mEnemies.size()) - 1, e.num("ladder", e.x) * kCellsPerTile});
    return true;
  }
  if (e.kind == "treasure" && e.hasPos && e.str("art") == "titan_brochure")
  {
    s.brochureX = e.x * kCellsPerTile;
    s.brochureY = e.y * kCellsPerTile + 1;
    s.brochureScore = e.num("score", 10000);
    return true;
  }
  if (e.kind == "titan" && e.hasPos)
  {
    auto& t = s.titan;
    t.on = true;
    t.landX = e.x * kCellsPerTile;
    t.deckY = e.y * kCellsPerTile + 1;
    t.x = t.prevX = t.landX;
    t.y = t.prevY = t.deckY - 40;
    return true;
  }
  if (e.kind == "flag" && e.hasPos)
  {
    s.flagX = e.x * kCellsPerTile;
    s.flagY = e.y * kCellsPerTile + 1;
    return true;
  }
  if (e.kind == "pit" && hasRect)
  {
    s.pits.push_back({x0 * kCellsPerTile, y0 * kCellsPerTile, (x1 - x0 + 1) * kCellsPerTile, (y1 - y0 + 1) * kCellsPerTile});
    return true;
  }
  if (e.kind == "duck")
    return true; // the duck's hard hat is only drawn
  return false;
}

void World::setupTowerEnemy(Enemy& en, const EntityDef& e)
{
  (void)e;
  auto& s = mTower;
  if (!s.on)
    return;
  const int i = int(&en - mEnemies.data());
  if (en.kind == EnemyKind::RelayDrone)
  {
    en.active = true; // it circles the mast whether or not it is in view
    s.relays.push_back({i, en.y, 1});
  }
  else if (en.kind == EnemyKind::FeedbackNode)
    s.nodes.push_back({i, 0, 0});
  else if (en.kind == EnemyKind::MastClimber && !s.copycat)
    s.climbers.push_back({i, 0, 0, -1, 0});
}

void World::linkTower()
{
  auto& s = mTower;
  s.on = s.on || mLevel->themeKey == "broadcast_mast" || mLevel->flag("wrapx");
  if (!s.on)
    return;
  s.wrap = mLevel->flag("wrapx");
  if (s.wrap)
    mMap.setWrapX(true);
  for (auto& d : s.doors)
    for (std::size_t j = 0; j < s.doors.size(); ++j)
      if (s.doors[j].id == d.to)
        d.link = int(j);
  // The Mast Climbers that wait for a ladder.
  for (const auto& w : s.waits)
    s.climbers.push_back({w.enemy, 0, 0, -1, 0});
  if (s.copycat)
  {
    s.dummyFall.assign(mEnemies.size(), 0);
    s.dummyVy.assign(mEnemies.size(), 0);
  }
  if (s.wrap)
    towerFrameEnd(); // the runner starts in the middle
}

// --- The turning tower ---------------------------------------------------------------------

void World::turnTower(int d)
{
  auto& s = mTower;
  const int W = mMap.width();
  d = wrapIn(d, W);
  if (d > W / 2)
    d -= W; // the short way round: the runner moves by d
  if (d == 0 || d % kCellsPerTile != 0)
    return;
  const int db = d / kCellsPerTile;
  s.rot = wrapIn(s.rot + d, W);
  ++s.turns;
  auto mv = [&](int& x) { x += d; };
  // The level itself (the exit, decorations) and the map.
  {
    auto lv = std::make_shared<Level>(*mLevel);
    rotateRows(lv->tiles, lv->width, lv->height, db);
    lv->exitTx = wrapIn(lv->exitTx + db, lv->width);
    lv->startTx = wrapIn(lv->startTx + db, lv->width);
    for (auto& dec : lv->decorations)
      dec.first = wrapIn(dec.first + db, lv->width);
    for (auto& sp : lv->spawns)
      sp.tx = wrapIn(sp.tx + db, lv->width);
    mLevel = lv;
  }
  mMap.rotateX(db);
  rotateRows(mLayerMask, mLevel->width, mLevel->height, db);
  if (mExplored && !mSimulation)
    rotateRows(*mExplored, mLevel->width, mLevel->height, db);
  // The runner and everything else.
  auto& p = mPlayer;
  mv(p.x);
  mv(p.prevX);
  mv(mSafeX);
  mRespawnX = wrapIn(mRespawnX + d, W);
  for (auto& e : mEnemies)
  {
    mv(e.x);
    mv(e.prevX);
    mv(e.railX0);
    mv(e.railX1);
    mv(e.aimX);
    mv(e.ox);
    e.drawX += float(d);
  }
  // Things that stay put stay on the map, so they come round again.
  for (auto& it : mItems)
  {
    const int nx = wrapIn(it.x + d, W);
    it.prevX += nx - it.x;
    it.x = nx;
  }
  for (auto& b : mBoxes)
    b.x = wrapIn(b.x + d, W);
  for (auto& cp : mCheckpoints)
    cp.x = wrapIn(cp.x + d, W);
  for (auto& pr : mProps)
    pr.x = wrapIn(pr.x + d, W);
  for (auto& pr : mProjectiles)
  {
    mv(pr.x);
    mv(pr.prevX);
    pr.fx += float(d);
    if (pr.anchorX >= 0)
      mv(pr.anchorX);
  }
  const float px = float(d * kCellSize);
  for (auto& pa : mParticles)
    pa.pos.x += px;
  for (auto& f : mFlashes)
    f.pos.x += px;
  for (auto& t : mTexts)
    t.pos.x += px;
  mCamera.shiftX(d);
  // The tower's own things.
  for (auto& dr : s.doors)
    dr.x = wrapIn(dr.x + d, W);
  for (auto& w : s.route)
    w.x = wrapIn(w.x + d, W);
  for (auto& w : s.waits)
    w.ladderX = wrapIn(w.ladderX + d, W);
  for (auto& dr : s.swarm)
    dr.x += float(d);
  auto& t = s.titan;
  mv(t.x);
  mv(t.prevX);
  t.landX = wrapIn(t.landX + d, W);
  for (auto& c : t.cross)
    c = wrapIn(c + d, W);
  if (s.brochureX >= 0)
    s.brochureX = wrapIn(s.brochureX + d, W);
  if (s.flagX >= 0)
    s.flagX = wrapIn(s.flagX + d, W);
  wrapActors();
}

// Things that circle the mast come back in at the other edge.
void World::wrapActors()
{
  const int W = mMap.width();
  auto near = [&](int& x, int& prev) {
    const int c = mPlayer.x;
    int k = 0;
    while (x + k * W < c - W / 2)
      ++k;
    while (x + k * W >= c + W / 2)
      --k;
    x += k * W;
    prev += k * W;
  };
  for (auto& e : mEnemies)
  {
    if (!e.alive)
      continue;
    int before = e.x;
    near(e.x, e.prevX);
    e.drawX += float(e.x - before);
  }
  for (auto& pr : mProjectiles)
  {
    if (!pr.alive)
      continue;
    const int before = pr.x;
    near(pr.x, pr.prevX);
    pr.fx += float(pr.x - before);
  }
  auto& t = mTower.titan;
  if (t.on)
    near(t.x, t.prevX);
}

// After the camera has moved: turn the tower if the runner strayed from
// the middle, and keep the view on the runner.
void World::towerFrameEnd()
{
  if (!mTower.wrap)
    return;
  const int W = mMap.width();
  const int mid = W / 2 - 1;
  const int off = mPlayer.x - mid;
  if (std::abs(off) > kTowerKeep && !mSimulation) // (the bot's look-ahead copies stay put)
  {
    int d = -off;
    d -= d % kCellsPerTile; // whole blocks
    turnTower(d);
  }
  wrapActors();
  // The player stays centred horizontally: the tower turns, not the view.
  mCamera.lockX(cameraTarget().left - 18); // as Camera::centerOn puts it
}

// --- Per frame -----------------------------------------------------------------------------

void World::updateTower(const PlayerInput& input)
{
  auto& s = mTower;
  auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  const bool upNow = input.up && !mTowerUpHeld;
  mTowerUpHeld = input.up;
  if (s.copycat)
  {
    updateCopycat(input);
    return;
  }
  // Laps walked this life: 42 and the tower spins (and the band waltzes).
  if (alive && p.state != PlayerState::Teleporting)
  {
    s.walked += std::abs(p.x - p.prevX) <= 4 ? std::abs(p.x - p.prevX) : 0;
    if (s.dizzy == 0 && !s.waltz && s.walked >= kDizzyLaps * mMap.width())
    {
      s.dizzy = kDizzyFrames;
      s.waltz = true;
      mMusicOverride = "theme_waltz";
      showMessage("42 LAPS OF THE MAST - THE WHOLE TOWER SPINS");
      playSound(Sfx::Whoosh);
    }
  }
  if (s.dizzy > 0)
    --s.dizzy;
  // A door: up takes you through to its twin.
  if (alive && upNow && p.state == PlayerState::OnGround)
    for (const auto& d : s.doors)
      if (d.link >= 0 && std::abs(towerDx(d.x, p.x)) <= 1 && std::abs(p.y - d.y) <= 1)
      {
        const auto& to = s.doors[std::size_t(d.link)];
        p.x = p.prevX = to.x;
        p.y = p.prevY = to.y;
        p.state = PlayerState::OnGround;
        playSound(Sfx::Teleport);
        towerFrameEnd();
        mCamera.centerOn(cameraTarget(), mMap.width(), mMap.height());
        break;
      }
  // A Mast Climber waiting at the foot of a ladder sets off once you climb it.
  for (auto& w : s.waits)
  {
    if (w.enemy < 0)
      continue;
    Enemy& e = mEnemies[std::size_t(w.enemy)];
    if (alive && p.state == PlayerState::Ladder && std::abs(towerDx(w.ladderX, p.x)) <= 2)
    {
      e.hidden = false;
      e.active = true;
      w.enemy = -1;
      playSound(Sfx::ArmorClank);
    }
  }
  // The bot's way up: the furthest waypoint stood on.
  if (alive && p.state == PlayerState::OnGround)
  {
    for (int i = std::min(int(s.route.size()) - 1, s.routeAt + 3); i > s.routeAt; --i)
    {
      const auto& w = s.route[std::size_t(i)];
      if (std::abs(towerDx(w.x, p.x)) <= 3 && std::abs(p.y - w.y) <= 2)
      {
        s.routeAt = i;
        break;
      }
    }
    // Fallen: back to the last waypoint at or below the runner.
    if (s.routeAt >= 0 && p.y > s.route[std::size_t(s.routeAt)].y + 6)
    {
      int j = s.routeAt;
      while (j >= 0 && s.route[std::size_t(j)].y < p.y - 1)
        --j;
      s.routeAt = j;
    }
  }
  // A Feedback Node's ring pushes the runner along.
  if (s.pushT > 0)
  {
    --s.pushT;
    if (alive && p.state != PlayerState::Ladder)
    {
      int x = p.x;
      mMap.moveHorizontally(x, p.y, Player::kWidth, p.height(), s.pushDir);
      p.x = x;
      if (p.state == PlayerState::OnGround && !mMap.onSolidGround(p.box()))
        startFallingDelayed();
    }
  }
  updateSwarm();
  // The brochure.
  if (alive && s.brochureX >= 0 && !s.brochureTaken &&
      boxAt(s.brochureX, s.brochureY, 2, 2).intersects(p.box()))
  {
    s.brochureTaken = true;
    addScore(s.brochureScore, cellCenter(boxAt(s.brochureX, s.brochureY, 2, 2)));
    showMessage("THE HALCYON TITAN SALES BROCHURE - SECRET FOUND");
    playSound(Sfx::Chime);
  }
  // Under the alien's flag on the top: its anthem.
  if (s.flagX >= 0 && !s.anthem)
  {
    if (alive && p.state == PlayerState::OnGround && std::abs(towerDx(s.flagX, p.x)) <= 2 && p.y > s.flagY &&
        p.y - s.flagY < 16 && p.x == p.prevX)
    {
      if (++s.flagT >= kAnthemStill)
      {
        s.anthem = true;
        showMessage("AN ALIEN ANTHEM, FOUR NOTES LONG");
        playSound(Sfx::FlagUp);
      }
    }
    else
      s.flagT = 0;
  }
  updateTitan();
}

void World::resetTower()
{
  auto& s = mTower;
  if (!s.on)
    return;
  s.pushT = 0;
  s.walked = 0;
  s.dizzy = 0;
  if (s.waltz)
  {
    s.waltz = false;
    mMusicOverride = mLevel->music;
  }
  for (auto& d : s.swarm)
    d.offline = 0;
  // The Titan waits on the deck for you to come back up.
  auto& t = s.titan;
  if (t.on && t.phase != TitanPhase::Done && t.phase != TitanPhase::Topple && t.phase != TitanPhase::Waiting)
  {
    t.move = TitanMove::None;
    t.moveT = 0;
    t.wave = -1;
    t.open = -1;
    t.t = 0;
  }
}

// --- Satellite Swarm -------------------------------------------------------------------------

void World::updateSwarm()
{
  auto& s = mTower;
  const auto& p = mPlayer;
  ++s.swarmT;
  const bool on = p.weapon == Weapon::Proto && ProtoId(p.proto) == ProtoId::SatelliteSwarm;
  const float cx = float(p.x) + 1.5f, cy = float(p.y) - 2.0f;
  for (int i = 0; i < kSwarmDrones; ++i)
  {
    auto& d = s.swarm[std::size_t(i)];
    if (d.offline > 0)
      --d.offline;
    const float a = 6.2831853f * (float(s.swarmT % kSwarmLap) / float(kSwarmLap) + float(i) / float(kSwarmDrones));
    d.x = cx + float(kSwarmRadius) * std::cos(a);
    d.y = cy + float(kSwarmRadius) * 0.6f * std::sin(a);
    if (!on)
      d.offline = 0;
  }
}

void World::swarmVolley(int dx, int dy)
{
  auto& s = mTower;
  if (!s.on)
    return;
  for (const auto& d : s.swarm)
  {
    if (d.offline > 0)
      continue;
    Projectile pr;
    pr.kind = ShotKind::Proto;
    pr.proto = int(ProtoId::SatelliteSwarm);
    pr.dx = dx;
    pr.dy = dy;
    pr.speed = 2;
    pr.damage = 1;
    pr.w = dx != 0 ? 2 : 1;
    pr.h = dx != 0 ? 1 : 2;
    pr.x = pr.prevX = int(std::floor(d.x)) - (dx < 0 ? 1 : 0);
    pr.y = pr.prevY = int(std::floor(d.y));
    mProjectiles.push_back(pr); // (a drone inside the lattice fires into it)
  }
}

bool World::swarmBlocks(Projectile& pr)
{
  auto& s = mTower;
  const auto& p = mPlayer;
  if (pr.kind != ShotKind::Enemy || !(p.weapon == Weapon::Proto && ProtoId(p.proto) == ProtoId::SatelliteSwarm))
    return false;
  const Vec2 c = cellCenter(pr.box());
  for (auto& d : s.swarm)
  {
    if (d.offline > 0)
      continue;
    const float ex = c.x / float(kCellSize) - d.x, ey = c.y / float(kCellSize) - d.y;
    if (ex * ex + ey * ey > 2.0f * 2.0f)
      continue;
    d.offline = kDroneOffline;
    pr.alive = false;
    playSound(Sfx::Deflect);
    burst(c, rgb(160, 255, 255), rgb(255, 255, 255), 10, 1.4f);
    return true;
  }
  return false;
}

// --- Enemies -----------------------------------------------------------------------------------

void World::updateMastClimber(Enemy& e, const EnemyDef& def)
{
  auto& s = mTower;
  if (e.hidden || s.copycat)
    return;
  Climber* c = nullptr;
  for (auto& f : s.climbers)
    if (&mEnemies[std::size_t(f.enemy)] == &e)
      c = &f;
  if (!c)
    return;
  const auto& p = mPlayer;
  int reached = 0;
  for (const auto& cp : mCheckpoints)
    reached += cp.active;
  if (c->giveUp < 0)
    c->giveUp = reached;
  if (reached >= c->giveUp + 2)
    return; // you are two checkpoints ahead: it stops
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  if (e.cool > 0)
    --e.cool;
  const CellBox b = e.box(), pb = p.hitBox();
  const int dx = towerDx(b.x + 1, pb.x + 1);
  // The wrench: raised, then swung.
  if (e.tell > 0)
  {
    if (--e.tell == 0)
    {
      const CellBox reach = e.dir > 0 ? CellBox{b.right() + 1, b.y, kClimberReach, b.h}
                                      : CellBox{b.x - kClimberReach, b.y, kClimberReach, b.h};
      if (alive && reach.intersects(pb))
        hurtPlayer(1);
      playSound(Sfx::Swoosh);
      e.cool = def.cooldown;
    }
    return;
  }
  const int ladderCx = ((b.x + 1) / kCellsPerTile) * kCellsPerTile;
  auto ladderAt = [&](int row) { return mMap.ladder(ladderCx, row); };
  if (c->climb != 0)
  {
    // On a ladder: up (or down) toward the runner, half a cell a frame.
    if (e.timer % 2 != 0)
      return;
    if (c->climb < 0 && ladderAt(e.y - e.h + 1))
    {
      e.x = ladderCx - 1;
      --e.y;
      return;
    }
    if (c->climb > 0 && ladderAt(e.y + 1))
    {
      e.x = ladderCx - 1;
      ++e.y;
      return;
    }
    c->climb = 0;
    if (c->climb == 0 && !mMap.onSolidGround(e.box()))
    {
      // Off the top: onto the floor beside the ladder.
      int y = e.y;
      mMap.moveVertically(e.x, y, e.w, e.h, -1);
      e.y = y;
    }
    return;
  }
  if (!mMap.onSolidGround(b))
  {
    c->fall = std::min(c->fall + 1, 4);
    int y = e.y;
    mMap.moveVertically(e.x, y, e.w, e.h, c->fall >= 3 ? 2 : 1);
    e.y = y;
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return;
  }
  c->fall = 0;
  const bool level = std::abs(pb.bottom() - b.bottom()) <= 3;
  const int gap = dx > 0 ? towerDx(b.right(), pb.x) - 1 : towerDx(pb.right(), b.x) - 1;
  if (alive && level && gap <= kClimberReach && e.cool == 0)
  {
    e.dir = dx < 0 ? -1 : 1;
    e.tell = def.tell;
    return;
  }
  if (!alive || e.timer % std::max(1, def.stepEvery) != 0)
    return;
  // Up a ladder after the runner.
  if (pb.bottom() < b.bottom() - 3 && ladderAt(e.y - e.h + 1))
  {
    c->climb = -1;
    return;
  }
  if (pb.bottom() > b.bottom() + 3 && ladderAt(e.y + 1))
  {
    c->climb = 1;
    return;
  }
  // Along the floor: toward a ladder up if the runner is above, else at them.
  int want = dx;
  if (pb.bottom() < b.bottom() - 3)
    for (int k = 1; k <= 40; ++k)
    {
      bool found = false;
      for (int sd : {-1, 1})
        if (mMap.ladder(ladderCx + sd * k * kCellsPerTile, e.y - 1) && mMap.solidTop(ladderCx + sd * k * kCellsPerTile, e.y + 1))
        {
          want = sd;
          found = true;
          break;
        }
      if (found)
        break;
    }
  if (std::abs(want) <= 1 && want == dx)
    return; // right on them
  e.dir = want < 0 ? -1 : 1;
  int x = e.x;
  mMap.moveHorizontally(x, e.y, e.w, e.h, e.dir);
  e.x = x;
}

void World::updateRelayDrone(Enemy& e, const EnemyDef& def)
{
  auto& s = mTower;
  RelayDrone* r = nullptr;
  for (auto& f : s.relays)
    if (&mEnemies[std::size_t(f.enemy)] == &e)
      r = &f;
  if (!r)
    return;
  // Round and round the mast, a cell a frame.
  e.x += r->dir;
  e.dir = r->dir;
  if (e.cool > 0)
    --e.cool;
  const auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  const CellBox b = e.box(), pb = p.hitBox();
  const int dx = towerDx(b.x + 2, pb.x + 1);
  const int dy = (pb.y + 2) - (b.y + 1);
  if (e.tell > 0)
  {
    if (--e.tell == 0)
    {
      Projectile pr;
      pr.kind = ShotKind::Enemy;
      pr.speed = 1;
      pr.damage = 1;
      pr.w = pr.h = 1;
      pr.dx = sgn(dx);
      pr.dy = std::abs(dy) > std::abs(dx) / 2 + 2 ? sgn(dy) : 0;
      if (pr.dx == 0 && pr.dy == 0)
        pr.dy = 1;
      pr.x = pr.prevX = b.x + 2;
      pr.y = pr.prevY = b.y + 2;
      pr.range = 60;
      mProjectiles.push_back(pr);
      playSound(Sfx::EnemyShot);
      e.cool = def.cooldown;
    }
    return;
  }
  if (alive && e.cool == 0 && std::abs(dx) <= kRelayRange && std::abs(dy) <= kRelayRange)
    e.tell = kRelayTell;
}

void World::updateFeedbackNode(Enemy& e, const EnemyDef& def)
{
  (void)def;
  auto& s = mTower;
  FeedbackNode* n = nullptr;
  for (auto& f : s.nodes)
    if (&mEnemies[std::size_t(f.enemy)] == &e)
      n = &f;
  if (!n)
    return;
  const auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  const CellBox b = e.box(), pb = p.hitBox();
  const int dx = towerDx(b.x + 2, pb.x + 1);
  const int dy = (pb.y + 2) - (b.y + 2);
  const float dist = std::sqrt(float(dx * dx + dy * dy));
  ++n->t;
  switch (n->state)
  {
    case 0: // idle: wakes with the runner near its ring
      e.tell = 0;
      if (alive && dist <= float(kNodeRadius + 10))
      {
        n->state = 1;
        n->t = 0;
        playSound(Sfx::CoreHum);
      }
      break;
    case 1: // glowing white, the hum rising
      e.tell = kNodeTell - n->t;
      if (n->t >= kNodeTell)
      {
        n->state = 2;
        n->t = 0;
        e.tell = 0;
        if (alive && dist <= float(kNodeRadius))
        {
          s.pushT = mCharacterIndex == 1 ? kNodePush / 2 : kNodePush; // Rocco is heavy
          s.pushDir = dx < 0 ? -1 : 1;
        }
        playSound(Sfx::CorePulse);
      }
      break;
    case 2: // the ring goes out
      if (n->t >= 8)
      {
        n->state = 3;
        n->t = 0;
      }
      break;
    default:
      if (n->t >= kNodeCool)
      {
        n->state = 0;
        n->t = 0;
      }
      break;
  }
}

// --- The Halcyon Titan -----------------------------------------------------------------------

void World::titanHurtsRunner(int damage)
{
  hurtPlayer(damage);
}

void World::titanDrop(ItemKind kind)
{
  const auto& t = mTower.titan;
  Item it;
  it.kind = kind;
  it.x = it.prevX = t.x + kTitanW / 2 - 1 + (kind == ItemKind::Turbo ? -t.facing * 8 : t.facing * 8);
  it.y = it.prevY = t.deckY;
  it.pickupDelay = 4;
  mItems.push_back(it);
}

void World::updateTitan()
{
  auto& t = mTower.titan;
  if (!t.on)
    return;
  const auto& p = mPlayer;
  if (t.flash > 0)
    --t.flash;
  if (t.banner > 0)
    --t.banner;
  ++t.t;
  switch (t.phase)
  {
    case TitanPhase::Waiting:
      // On the top deck: the cargo airship brings it in.
      if (p.state == PlayerState::OnGround && p.y <= t.deckY && p.y >= t.deckY - 2)
      {
        t.phase = TitanPhase::Intro;
        t.t = 0;
        t.ship = 0;
        t.banner = kTitanIntro;
        t.bannerText = "BROUGHT TO YOU BY HALCYON ARMS!";
        t.x = t.prevX = t.landX;
        t.y = t.prevY = t.deckY - 40;
        t.facing = towerDx(t.x + kTitanW / 2, p.x + 1) < 0 ? -1 : 1;
        playSound(Sfx::Rumble);
        showMessage("BROUGHT TO YOU BY HALCYON ARMS!");
      }
      return;
    case TitanPhase::Intro:
      ++t.ship;
      t.y = t.deckY - 40 + (40 * std::min(t.t, kTitanIntro - 10)) / (kTitanIntro - 10);
      if (t.t >= kTitanIntro)
      {
        t.y = t.deckY;
        t.phase = TitanPhase::Demo;
        t.t = 0;
        t.move = TitanMove::None;
        mCamera.shake(12, 3.0f);
        playSound(Sfx::Stomp);
      }
      return;
    case TitanPhase::Demo:
      titanDemo();
      break;
    case TitanPhase::Orbit:
      titanOrbit();
      break;
    case TitanPhase::Overheat:
      titanOverheat();
      break;
    case TitanPhase::Topple:
      // Teeters, then over the side into the clouds.
      if (t.t > kTeeter)
      {
        ++t.fallT;
        t.y += 1 + t.fallT / 4;
        t.x += t.facing;
        if (t.fallT == 1)
          playSound(Sfx::Whistle);
      }
      if (t.fallT > 50)
      {
        t.phase = TitanPhase::Done;
        mCamera.shake(20, 4.0f);
        playSound(Sfx::Explosion);
        showMessage("THE HALCYON TITAN IS DOWN - RIDE THE SIGNAL");
      }
      return;
    default:
      return;
  }
  // Walking into it hurts.
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  if (alive && t.phase != TitanPhase::Topple && t.phase != TitanPhase::Done)
  {
    CellBox b = t.box();
    b.x = p.x + towerDx(p.x, b.x);
    if (b.intersects(p.hitBox()))
      titanHurtsRunner(1);
  }
}

void World::titanDemo()
{
  auto& t = mTower.titan;
  const auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  const int mid = t.x + kTitanW / 2;
  const int pc = p.x + 1;
  const int dx = towerDx(mid, pc); // + the runner is to the right
  const bool onDeck = p.y >= t.deckY - 2 && p.y <= t.deckY + 1;
  if (t.core > 0)
    --t.core;
  if (t.move == TitanMove::None)
  {
    // Between attacks: it turns to face you.
    t.facing = dx < 0 ? -1 : 1;
    if (t.t >= std::max(kTitanGap, kTitanCore))
    {
      t.move = TitanMove(1 + t.nextMove);
      t.nextMove = (t.nextMove + 1) % 3;
      t.moveT = 0;
      t.banner = 30;
      t.bannerText = t.move == TitanMove::Laser ? "LASER SWEEP!" : t.move == TitanMove::Missiles ? "MISSILE VOLLEY!" : "FLAMETHROWER!";
      playSound(Sfx::Siren);
    }
    return;
  }
  ++t.moveT;
  const int front = dx * t.facing; // cells in front of its middle
  switch (t.move)
  {
    case TitanMove::Laser:
      if (t.moveT == kLaserTell)
        playSound(Sfx::LaserShot);
      if (t.moveT > kLaserTell && t.moveT <= kLaserTell + kLaserFrames)
      {
        // At standing head height: crouch under it.
        if (alive && onDeck && front > 0 && front <= kLaserReach && p.height() >= Player::kHeight)
          titanHurtsRunner(1);
      }
      if (t.moveT > kLaserTell + kLaserFrames)
        t.move = TitanMove::None;
      break;
    case TitanMove::Missiles:
      if (t.moveT == 1)
      {
        // Crosshairs on the deck: one on you, the rest around.
        const int offs[4] = {0, -7, 7, 14 * (dx < 0 ? -1 : 1)};
        for (int i = 0; i < 4; ++i)
          t.cross[std::size_t(i)] = wrapIn(pc + offs[i], mMap.width());
        playSound(Sfx::Beep);
      }
      if (t.moveT == kMissileTell)
      {
        bool hit = false;
        for (int c : t.cross)
        {
          explodeVisual({(float(c) + 0.5f) * kCellSize, float(t.deckY - 1) * kCellSize});
          hit = hit || (std::abs(towerDx(c, pc)) <= kMissileRadius && onDeck);
        }
        if (alive && hit)
          titanHurtsRunner(1);
        playSound(Sfx::Explosion);
        t.move = TitanMove::None;
      }
      break;
    case TitanMove::Flame:
      if (t.moveT == kFlameTell)
        playSound(Sfx::FlameShot);
      if (t.moveT > kFlameTell && t.moveT <= kFlameTell + kFlameFrames)
      {
        if (alive && front > 0 && front <= kTitanW / 2 + kFlameReach && p.y > t.deckY - kTitanH &&
            (t.moveT - kFlameTell) % 15 == 1)
          titanHurtsRunner(1);
      }
      if (t.moveT > kFlameTell + kFlameFrames)
        t.move = TitanMove::None;
      break;
    default:
      t.move = TitanMove::None;
      break;
  }
  if (t.move == TitanMove::None)
  {
    // The power core opens after each attack.
    t.core = kTitanCore;
    t.t = 0;
  }
}

void World::titanOrbit()
{
  auto& t = mTower.titan;
  const auto& p = mPlayer;
  const int W = mMap.width();
  const int mid = t.x + kTitanW / 2;
  const int dx = towerDx(mid, p.x + 1);
  // Round to the far side and keep away, half a cell a frame.
  if (std::abs(dx) < W / 2 - 4 && t.t % 2 == 0)
  {
    t.facing = dx < 0 ? 1 : -1; // its back to you
    t.x += t.facing;
  }
  // Lattice shots come round the mast at you.
  if (++t.lattice >= kLatticeCool + kLatticeTell)
  {
    t.lattice = 0;
    Projectile pr;
    pr.kind = ShotKind::Enemy;
    pr.speed = 1;
    pr.damage = 1;
    pr.w = 2;
    pr.h = 1;
    pr.dx = dx < 0 ? -1 : 1;
    pr.dy = 0;
    pr.x = pr.prevX = pr.dx > 0 ? t.x + kTitanW : t.x - 2;
    pr.y = pr.prevY = t.deckY;
    pr.range = W;
    mProjectiles.push_back(pr);
    playSound(Sfx::EnemyShot);
  }
}

void World::titanOverheat()
{
  auto& t = mTower.titan;
  const auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  const int mid = t.x + kTitanW / 2;
  const int pc = p.x + 1;
  const int dx = towerDx(mid, pc);
  const bool onDeck = p.y >= t.deckY - 1 && p.y <= t.deckY + 1;
  // Stomps: a crouch, then a shockwave along the deck both ways.
  if (++t.stompT >= kTitanStompEvery)
  {
    t.stompT = 0;
    t.wave = 0;
    mCamera.shake(10, 2.5f);
    playSound(Sfx::Stomp);
  }
  if (t.wave >= 0)
  {
    const int before = t.wave;
    t.wave += kTitanWaveSpeed;
    const int ad = std::abs(dx);
    if (alive && onDeck && p.state == PlayerState::OnGround && ad > before && ad <= t.wave)
      titanHurtsRunner(1);
    if (t.wave > kTitanWaveReach)
      t.wave = -1;
  }
  // The vent panels, one at a time.
  if (t.open < 0)
  {
    if (++t.gapT >= kPanelGap)
      for (int k = 0; k < 4; ++k)
      {
        const int i = (t.nextMove + k) % 4;
        if (t.panel[std::size_t(i)] > 0)
        {
          t.open = i;
          t.openT = 0;
          t.nextMove = (i + 1) % 4;
          playSound(Sfx::ValveShut);
          break;
        }
      }
    return;
  }
  ++t.openT;
  const bool back = t.open >= 2;
  const int openFor = back ? kPanelOpenBack : kPanelOpen;
  if (t.openT > kPanelTell)
  {
    // Steam jets out of the open panel's side.
    const int side = back ? -t.facing : t.facing;
    const int out = dx * side - kTitanW / 2;
    if (alive && out >= -1 && out <= kSteamReach + 2 && p.y > t.deckY - kTitanH && (t.openT - kPanelTell) % 15 == 1)
      titanHurtsRunner(1);
  }
  if (t.openT >= kPanelTell + openFor)
  {
    t.open = -1;
    t.gapT = 0;
  }
}

bool World::shotAtTower(Projectile& pr, const CellBox& hit)
{
  (void)hit;
  auto& t = mTower.titan;
  if (!t.on || pr.kind == ShotKind::Enemy)
    return false;
  if (t.phase != TitanPhase::Demo && t.phase != TitanPhase::Orbit && t.phase != TitanPhase::Overheat)
    return false;
  CellBox b = t.box();
  b.x = pr.x + towerDx(pr.x, b.x);
  if (!b.intersects(pr.box()))
    return false;
  pr.alive = false;
  const Vec2 c = cellCenter(pr.box());
  const int dmg = std::max(1, pr.damage);
  const int fromDir = pr.dx; // the way the shot flies
  auto spark = [&]() {
    burst(c, rgb(255, 230, 160), rgb(255, 255, 255), 6, 1.0f);
    playSound(Sfx::Ricochet);
  };
  if (t.phase == TitanPhase::Demo)
  {
    // Only the open core in its chest.
    if (t.core <= 0 || fromDir == t.facing)
    {
      spark();
      return true;
    }
    t.hp[0] = std::max(0, t.hp[0] - dmg);
    t.flash = 4;
    playSound(Sfx::Hit);
    if (t.hp[0] == 0)
    {
      t.phase = TitanPhase::Orbit;
      t.t = 0;
      t.lattice = 0;
      t.move = TitanMove::None;
      t.banner = 45;
      t.bannerText = "NOW WITH ORBITAL MODE!";
      titanDrop(ItemKind::Health);
      showMessage("IT CLIMBS ROUND THE MAST - CATCH IT");
    }
    return true;
  }
  if (t.phase == TitanPhase::Orbit)
  {
    t.hp[1] = std::max(0, t.hp[1] - dmg);
    t.flash = 4;
    playSound(Sfx::Hit);
    if (t.hp[1] == 0)
    {
      t.phase = TitanPhase::Overheat;
      t.t = 0;
      t.stompT = 0;
      t.wave = -1;
      t.open = -1;
      t.gapT = 0;
      t.nextMove = 0;
      t.facing = towerDx(t.x + kTitanW / 2, mPlayer.x + 1) < 0 ? -1 : 1;
      t.banner = 45;
      t.bannerText = "WARNING: OVERHEATING";
      titanDrop(ItemKind::Health);
      titanDrop(ItemKind::Turbo);
      showMessage("ITS VENTS OPEN ONE BY ONE - THE BACK ONES FROM BEHIND");
    }
    return true;
  }
  // Overheat: only the open panel, from its side.
  const bool back = t.open >= 2;
  const bool right = t.open >= 0 && t.openT > kPanelTell && (back ? fromDir == t.facing : fromDir == -t.facing);
  if (!right)
  {
    spark();
    return true;
  }
  auto& hp = t.panel[std::size_t(t.open)];
  hp = std::max(0, hp - dmg);
  t.hp[2] = t.panel[0] + t.panel[1] + t.panel[2] + t.panel[3];
  t.flash = 4;
  playSound(Sfx::Hit);
  if (hp == 0)
  {
    t.open = -1;
    t.gapT = 0;
    burst(c, rgb(255, 120, 60), rgb(255, 255, 200), 20, 2.0f);
    playSound(Sfx::Explosion);
    if (t.hp[2] == 0)
    {
      t.phase = TitanPhase::Topple;
      t.t = 0;
      t.fallT = 0;
      t.wave = -1;
      t.facing = towerDx(t.x + kTitanW / 2, mPlayer.x + 1) < 0 ? 1 : -1; // over, away from you
      addScore(50000, cellCenter(t.box()));
      ++mStats.kills;
      t.banner = 60;
      t.bannerText = "PRODUCT RECALL";
    }
  }
  return true;
}

// --- Copycat ---------------------------------------------------------------------------------

void World::updateCopycat(const PlayerInput& input)
{
  auto& s = mTower;
  auto& p = mPlayer;
  const int mvX = (input.right ? 1 : 0) - (input.left ? 1 : 0);
  const auto& arc = runnerJumpArc();
  if (s.dummyFall.size() < mEnemies.size())
  {
    s.dummyFall.resize(mEnemies.size(), 0);
    s.dummyVy.resize(mEnemies.size(), 0);
  }
  for (std::size_t i = 0; i < mEnemies.size(); ++i)
  {
    Enemy& e = mEnemies[i];
    if (!e.alive)
      continue;
    e.active = true;
    int& fall = s.dummyFall[i];
    int& jumpAt = s.dummyVy[i]; // frames into a jump, 0 none
    const bool ground = mMap.onSolidGround(e.box());
    // The runner's moves: walk (at a Mast Climber's half speed), jump.
    if (mvX != 0 && mStats.frames % 2 == 0)
    {
      e.dir = mvX;
      int x = e.x;
      mMap.moveHorizontally(x, e.y, e.w, e.h, mvX);
      e.x = x;
    }
    if (ground && jumpAt == 0 && input.jump.triggered)
      jumpAt = 1;
    if (jumpAt > 0)
    {
      const int up = jumpAt <= int(arc.size()) ? arc[std::size_t(jumpAt - 1)] : 0;
      int y = e.y;
      if (up > 0 && mMap.moveVertically(e.x, y, e.w, e.h, -up) != MoveResult::Completed)
        jumpAt = int(arc.size());
      e.y = y;
      if (++jumpAt > int(arc.size()))
        jumpAt = 0;
      fall = 0;
    }
    else if (!ground)
    {
      fall = std::min(fall + 1, 4);
      int y = e.y;
      mMap.moveVertically(e.x, y, e.w, e.h, fall >= 3 ? 2 : 1);
      e.y = y;
    }
    else
      fall = 0;
    // Feet first into a pit: caught.
    for (const auto& pit : s.pits)
      if (pit.intersects({e.x, e.y, e.w, 1}))
      {
        e.alive = false;
        ++s.caught;
        if (!mSimulation && std::getenv("GR_TOWER_DEBUG"))
          std::fprintf(stderr, "copycat f%d: dummy %d caught at %d,%d\n", mStats.frames, int(i), e.x, e.y);
        addScore(1000, cellCenter(e.box()));
        playSound(Sfx::Gem);
        showMessage("CAUGHT ONE");
        break;
      }
    if (e.alive && e.y > mMap.height() + 4)
      e.alive = false;
  }
  // A dummy bumps the runner back two blocks.
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  if (alive)
    for (const auto& e : mEnemies)
      if (e.alive && e.box().intersects(p.hitBox()))
      {
        const int dir = (p.x + 1) < (e.x + e.w / 2) ? -1 : 1;
        int x = p.x;
        mMap.moveHorizontally(x, p.y, Player::kWidth, p.height(), dir * 2 * kCellsPerTile);
        p.x = x;
        playSound(Sfx::Bonk);
        break;
      }
}

// --- Saves -----------------------------------------------------------------------------------

bool World::towerCanSave() const
{
  const auto& t = mTower.titan;
  return !titanFight() || t.phase == TitanPhase::Waiting;
}

std::vector<int> World::towerSave() const
{
  const auto& s = mTower;
  std::vector<int> v{s.rot, s.routeAt, s.brochureTaken, s.walked, s.waltz, s.anthem, s.caught,
    int(s.titan.phase == TitanPhase::Done), int(s.waits.size())};
  for (const auto& w : s.waits)
    v.push_back(w.enemy >= 0);
  return v;
}

bool World::validTowerSave(const std::vector<int>& s) const
{
  return s.size() >= 9 && s[8] == int(mTower.waits.size()) && s.size() == 9 + mTower.waits.size() && s[0] >= 0 &&
    s[0] < std::max(1, mMap.width());
}

void World::loadTower(const std::vector<int>& v)
{
  auto& s = mTower;
  s.routeAt = std::min(v[1], int(s.route.size()) - 1);
  s.brochureTaken = v[2] != 0;
  s.walked = std::max(0, v[3]);
  s.waltz = v[4] != 0;
  if (s.waltz)
    mMusicOverride = "theme_waltz";
  s.anthem = v[5] != 0;
  s.caught = std::max(0, v[6]);
  if (v[7] != 0 && s.titan.on)
  {
    s.titan.phase = TitanPhase::Done;
    s.titan.hp = {0, 0, 0};
  }
  for (std::size_t i = 0; i < s.waits.size(); ++i)
    if (v[9 + i] == 0)
      s.waits[i].enemy = -1;
}

} // namespace gr
