// Level 15, Hangar Bay (SPEC 15): Breach and Vent. Hull panels that only a
// Breach Charge opens, the vents behind them that pull everything loose
// out into space for 60 frames, handrails to hold on to, loose crates,
// and the hangar's crew: Loader Mechs that throw crates, Weld Drones that
// leave hot seams, and Tether Pairs with a beam strung between them.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace gr
{

namespace
{

constexpr int kTell = 12;           // frames of red strobe before the shutter slams
constexpr float kChargeGravity = 0.15f;
constexpr int kBlast = 4;           // cells: a charge's blast reaches 2 blocks
constexpr int kChargeDamage = 8;
constexpr int kMaxCharges = 2;
constexpr int kDetonateHold = 8;    // frames fire is held to set the charges off
constexpr float kCrateGravity = 0.2f;
constexpr int kLegsHp = 4;
constexpr int kLiftReach = 8;       // cells: a crate this close to a Loader Mech can be thrown
constexpr int kRun = 30;            // a Weld Drone's run
constexpr int kSeamLife = 45;
constexpr int kBeamOn = 90;         // a Tether Pair's beam: on, then down, then flickering back
constexpr int kBeamDown = 16;
constexpr int kFlicker = 8;
constexpr int kRam = 24;

int sgn(int v) { return (v > 0) - (v < 0); }
float sgnf(float v) { return v > 0.0f ? 1.0f : (v < 0.0f ? -1.0f : 0.0f); }

CellBox panelBox(const HullPanel& p)
{
  return {p.bx * kCellsPerTile, p.by * kCellsPerTile, p.w * kCellsPerTile, p.h * kCellsPerTile};
}

CellBox reachBox(const HullPanel& p)
{
  const CellBox b = panelBox(p);
  return {b.x - 1, b.y - 1, b.w + 2, b.h + 2};
}

} // namespace

// --- Setup ---------------------------------------------------------------------------

bool World::setupStationEntity(const EntityDef& e)
{
  auto& st = mStation;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  if (e.kind == "panel" && e.hasPos)
  {
    HullPanel p;
    p.id = e.id;
    p.bx = e.x;
    p.by = e.y;
    p.w = std::max(1, e.num("w", 2));
    p.h = std::max(1, e.num("h", 2));
    p.ventId = e.str("vent");
    p.reseal = std::max(15, e.num("reseal", 90));
    p.bonus = e.num("bonus", 0) != 0;
    st.panels.push_back(p);
    return true;
  }
  if (e.kind == "vent" && e.rect("rect", x0, y0, x1, y1))
  {
    VentZone v;
    v.id = e.id;
    v.x0 = x0;
    v.y0 = y0;
    v.x1 = x1;
    v.y1 = y1;
    v.pull = std::clamp(e.num("pull", 1), 1, 4);
    v.frames = std::max(kTell + 1, e.num("frames", 60));
    // exclude=x0,y0,x1,y1;x0,y0,x1,y1 (the path parser splits on ';').
    const std::string ex = e.str("exclude");
    std::size_t at = 0;
    while (at < ex.size())
    {
      const std::size_t end = std::min(ex.find(';', at), ex.size());
      std::array<int, 4> r{};
      if (std::sscanf(ex.substr(at, end - at).c_str(), "%d,%d,%d,%d", &r[0], &r[1], &r[2], &r[3]) == 4)
        v.exclude.push_back(r);
      at = end + 1;
    }
    st.vents.push_back(v);
    return true;
  }
  if (e.kind == "handrail")
  {
    st.rails.push_back({e.num("x0"), e.num("x1"), e.num("y")});
    return true;
  }
  if (e.kind == "crate" && e.hasPos)
  {
    Crate c;
    c.bx = e.x;
    c.by = e.y;
    c.gems = e.num("gems", 0);
    st.crates.push_back(c);
    return true;
  }
  if (e.kind == "cratepile" && e.hasPos)
  {
    for (int i = 0; i < std::max(1, e.num("n", 3)); ++i)
    {
      Crate c;
      c.bx = e.x;
      c.by = e.y - i;
      st.crates.push_back(c);
    }
    return true;
  }
  if (e.kind == "setpiece")
  {
    st.setOn = true;
    st.setX0 = e.num("x0");
    st.setX1 = e.num("x1");
    st.setY = e.num("y");
    return true;
  }
  return false;
}

void World::setupStationEnemy(Enemy& en, const EntityDef& e)
{
  if (en.kind == EnemyKind::WeldDrone)
  {
    const auto path = e.list("path");
    en.railX0 = path.size() == 2 ? path[0] * kCellsPerTile : -1;
    en.railX1 = path.size() == 2 ? path[1] * kCellsPerTile + 1 : -1;
    en.attach = e.num("wall", 0) != 0 ? 1 : 0; // climbing the wall on its right
    en.dir = en.attach ? -1 : en.dir;
    en.hidden = e.num("sleep", 0) != 0;
  }
  else if (en.kind == EnemyKind::Loader)
  {
    en.ox = kLegsHp;
    en.aimX = -1; // no crate in its hands
    en.railX0 = en.x - 12;
    en.railX1 = en.x + 12;
  }
  else if (en.kind == EnemyKind::Tether)
  {
    // The level places the lower drone; linkStation strings the upper one
    // `gap` blocks above it.
    const auto patrol = e.list("patrol");
    en.railX0 = patrol.size() == 2 ? patrol[0] * kCellsPerTile : en.x - 10;
    en.railX1 = patrol.size() == 2 ? patrol[1] * kCellsPerTile : en.x + 10;
    en.attach = -2;
    en.aimY = std::max(2, e.num("gap", 5)) * kCellsPerTile;
    en.variant = 1;
    en.hidden = e.num("sleep", 0) != 0;
  }
}

void World::linkStation()
{
  auto& st = mStation;
  for (auto& p : st.panels)
    for (std::size_t i = 0; i < st.vents.size(); ++i)
      if (st.vents[i].id == p.ventId)
        p.vent = int(i);
  for (const auto& c : st.crates)
    mMap.setBlock(c.bx, c.by, Tile::Solid);
  // Tether Pairs: the upper drone of each.
  const std::size_t n = mEnemies.size();
  for (std::size_t i = 0; i < n; ++i)
  {
    if (mEnemies[i].kind != EnemyKind::Tether || mEnemies[i].attach != -2)
      continue;
    const Enemy low = mEnemies[i];
    spawnEnemy(low.def, low.x, low.y - low.aimY);
    Enemy& up = mEnemies.back();
    up.variant = 0;
    up.attach = int(i);
    up.railX0 = low.railX0;
    up.railX1 = low.railX1;
    up.aimY = low.aimY;
    up.hidden = low.hidden;
    up.dir = low.dir;
    mEnemies[i].attach = int(mEnemies.size()) - 1;
  }
  st.sleepers.clear();
  for (std::size_t i = 0; i < mEnemies.size(); ++i)
    if (mEnemies[i].hidden && (mEnemies[i].kind == EnemyKind::WeldDrone || mEnemies[i].kind == EnemyKind::Tether))
      st.sleepers.push_back(int(i));
  // The candid camera outside the bay doors shows while the panel over it
  // is open.
  for (std::size_t i = 0; i < mEnemies.size() && !st.panels.empty(); ++i)
  {
    if (mEnemies[i].kind != EnemyKind::Camera)
      continue;
    const CellBox cam = mEnemies[i].box();
    for (std::size_t k = 0; k < st.panels.size(); ++k)
    {
      const CellBox pb = panelBox(st.panels[k]);
      const CellBox near{pb.x - 4, pb.y - 4, pb.w + 8, pb.h + 8};
      if (near.intersects(cam))
      {
        st.camera = int(i);
        st.cameraPanel = int(k);
        mEnemies[i].hidden = true;
      }
    }
  }
  // The bonus entrance waits behind its panel.
  for (const auto& p : st.panels)
    if (p.bonus)
      for (auto& pr : mProps)
        if (pr.kind == PropKind::BonusDoor && reachBox(p).intersects(pr.box()))
          pr.dormant = true;
}

void World::resetStation()
{
  mStation.caught = -1;
  mStation.detonate = 0;
}

// --- Rails, panels, vents ------------------------------------------------------------

bool World::holdingRail() const
{
  const auto& p = mPlayer;
  if (p.state == PlayerState::Ladder)
    return true;
  if (p.state != PlayerState::OnGround && p.state != PlayerState::Recovering)
    return false;
  const int feet = p.y / kCellsPerTile;
  const int mid = (p.x + 1) / kCellsPerTile;
  for (const auto& r : mStation.rails)
    if (feet == r.y + 1 && mid >= r.x0 - 1 && mid <= r.x1 + 1)
      return true;
  return false;
}

int World::ventPulling(int cx, int cy, int& tx, int& ty) const
{
  const auto& st = mStation;
  for (std::size_t i = 0; i < st.panels.size(); ++i)
  {
    const HullPanel& p = st.panels[i];
    if (p.open == 0 || p.vent < 0)
      continue;
    const VentZone& v = st.vents[std::size_t(p.vent)];
    if (!v.inside(cx / kCellsPerTile, cy / kCellsPerTile))
      continue;
    const CellBox b = panelBox(p);
    tx = b.x + b.w / 2;
    ty = b.y + b.h / 2;
    return int(i);
  }
  return -1;
}

int World::panelAt(const CellBox& b) const
{
  for (std::size_t i = 0; i < mStation.panels.size(); ++i)
    if (mStation.panels[i].open > 0 && reachBox(mStation.panels[i]).intersects(b))
      return int(i);
  return -1;
}

void World::breachPanel(int index)
{
  auto& st = mStation;
  HullPanel& p = st.panels[std::size_t(index)];
  if (p.open > 0 || p.cool > 0)
    return;
  p.open = 1;
  p.dented = false;
  const CellBox b = panelBox(p);
  const Vec2 c = cellCenter(b);
  burst(c, rgb(255, 255, 255), rgb(255, 200, 40), 30, 3.0f);
  flashAt(c, 200.0f, rgb(255, 255, 255), 20);
  mCamera.shake(12, 2.0f);
  playSound(Sfx::Siren);
  showMessage(p.bonus ? "THE HULL OPENS... STATIC OUT THERE" : "HULL BREACH! HOLD ON TO SOMETHING");
  // Nothing loose in reach: a single sock floats out (the easter egg).
  if (p.vent >= 0)
  {
    const VentZone& v = st.vents[std::size_t(p.vent)];
    bool loose = false;
    for (const auto& e : mEnemies)
      loose = loose || (e.alive && !e.hidden && v.inside((e.x + 1) / kCellsPerTile, e.y / kCellsPerTile) &&
                        !(e.kind == EnemyKind::Loader && e.ox > 0) && e.kind != EnemyKind::Camera);
    for (const auto& it : mItems)
      loose = loose || (!it.taken && v.inside(it.x / kCellsPerTile, it.y / kCellsPerTile));
    for (const auto& cr : st.crates)
      loose = loose || (cr.alive && v.inside(cr.bx, cr.by));
    const auto& pl = mPlayer;
    loose = loose || v.inside((pl.x + 1) / kCellsPerTile, pl.y / kCellsPerTile);
    if (!loose)
    {
      Debris d;
      d.kind = 4;
      const float far = std::abs(v.x0 - p.bx) > std::abs(v.x1 - p.bx) ? float(v.x0) : float(v.x1);
      d.x = (far + 0.5f) * float(kCellsPerTile);
      d.y = (float(v.y0 + v.y1) * 0.5f) * float(kCellsPerTile);
      d.vx = (float(b.x) - d.x) / 60.0f;
      d.vy = (float(b.y) - d.y) / 60.0f;
      d.spin = 0.08f;
      d.life = 90;
      st.debris.push_back(d);
    }
  }
}

void World::ventOut(int kind, float x, float y, int variant)
{
  Debris d;
  d.kind = kind;
  d.x = x;
  d.y = y;
  const float a = float(mStats.frames % 17) * 0.37f;
  d.vx = std::cos(a) * 0.6f;
  d.vy = std::sin(a) * 0.6f;
  d.spin = 0.15f + float(mStats.frames % 5) * 0.05f;
  d.variant = variant;
  mStation.debris.push_back(d);
}

// Moves a box toward (tx, ty) by up to `cells`, one cell at a time, around
// nothing: walls stop it. Returns whether it moved.
bool World::pullBox(int& x, int& y, int w, int h, int tx, int ty, int cells) const
{
  bool moved = false;
  for (int s = 0; s < cells; ++s)
  {
    const int cx = x + w / 2, cy = y - h / 2;
    const int dx = sgn(tx - cx), dy = sgn(ty - cy);
    if (dx != 0 && !mMap.overlapsSolid(boxAt(x + dx, y, w, h)))
    {
      x += dx;
      moved = true;
    }
    if (dy != 0 && !mMap.overlapsSolid(boxAt(x, y + dy, w, h)))
    {
      y += dy;
      moved = true;
    }
  }
  return moved;
}

void World::updateVents()
{
  auto& st = mStation;
  auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  st.holding = alive && holdingRail();

  for (std::size_t i = 0; i < st.panels.size(); ++i)
  {
    HullPanel& hp = st.panels[i];
    if (hp.cool > 0)
      --hp.cool;
    if (hp.open == 0)
      continue;
    const int frames = hp.vent >= 0 ? st.vents[std::size_t(hp.vent)].frames : 60;
    if (hp.open == frames - kTell)
      playSound(Sfx::Klaxon);
    if (++hp.open > frames)
    {
      hp.open = 0;
      hp.cool = hp.reseal;
      hp.dented = true;
      mCamera.shake(6, 1.0f);
      playSound(Sfx::Clunk);
      if (st.caught == int(i))
        st.caught = -1;
    }
  }

  // The runner: held by a panel's frame, or pulled toward it.
  if (st.caught >= 0)
  {
    if (!alive || st.panels[std::size_t(st.caught)].open == 0)
      st.caught = -1;
    else
    {
      p.x = p.prevX = st.caughtX;
      p.y = p.prevY = st.caughtY;
      p.mercy = std::max(p.mercy, 2);
      if (p.state == PlayerState::Jumping)
        p.state = PlayerState::Falling;
    }
  }
  int tx = 0, ty = 0;
  const int pulling = alive && st.caught < 0 && p.turbo == 0 && p.vehicle < 0 ? ventPulling(p.x + 1, p.y - 2, tx, ty) : -1;
  if (pulling >= 0 && !st.holding)
  {
    const int pull = st.vents[std::size_t(st.panels[std::size_t(pulling)].vent)].pull;
    const int y0 = p.y;
    for (int s = 0; s < pull; ++s)
    {
      const int dx = sgn(tx - (p.x + 1)), dy = sgn(ty - (p.y - 2));
      if (dx != 0)
        mMap.moveHorizontally(p.x, p.y, Player::kWidth, p.height(), dx);
      if (dy != 0)
        mMap.moveVertically(p.x, p.y, Player::kWidth, p.height(), dy);
    }
    if (p.y < y0 && (p.state == PlayerState::OnGround || p.state == PlayerState::Recovering))
      startFalling();
    if (p.state == PlayerState::OnGround && !mMap.onSolidGround(p.box()))
      startFalling();
    const int at = panelAt(p.box());
    if (at >= 0)
    {
      HullPanel& hp = st.panels[std::size_t(at)];
      if (hp.bonus)
      {
        // The bonus panel lets the runner through: into the static.
        for (auto& pr : mProps)
          if (pr.kind == PropKind::BonusDoor && !pr.used)
          {
            pr.used = true;
            pr.dormant = false;
          }
        mBonusRequested = true;
        playSound(Sfx::Teleport);
      }
      else
      {
        st.caught = at;
        st.caughtX = p.x;
        st.caughtY = p.y;
        playSound(Sfx::Clunk);
        showMessage("CAUGHT BY THE FRAME!");
      }
    }
  }

  // Enemies: everything not clamped down.
  for (auto& e : mEnemies)
  {
    if (!e.alive || e.hidden || e.kind == EnemyKind::Camera || (e.kind == EnemyKind::Loader && e.ox > 0))
      continue;
    const CellBox b = e.box();
    const int k = ventPulling(b.x + b.w / 2, b.y + b.h / 2, tx, ty);
    if (k < 0)
      continue;
    pullBox(e.x, e.y, e.w, e.h, tx, ty, st.vents[std::size_t(st.panels[std::size_t(k)].vent)].pull);
    e.active = true;
    if (panelAt(e.box()) >= 0)
    {
      // Out it goes: it scores as if shot.
      const EnemyDef& def = enemyDef(e.def);
      e.alive = false;
      if (!(def.flags & kEnemyNoTally))
        ++mStats.kills;
      addScore(def.score, cellCenter(e.box()));
      ventOut(2, float(e.x + e.w / 2), float(e.y - e.h / 2), e.def);
    }
  }
  // Items and gems: lost to space.
  for (auto& it : mItems)
  {
    if (it.taken || it.heldBy >= 0)
      continue;
    const int k = ventPulling(it.x + 1, it.y - 1, tx, ty);
    if (k < 0)
      continue;
    it.floating = true;
    it.prevX = it.x;
    it.prevY = it.y;
    pullBox(it.x, it.y, 2, 2, tx, ty, st.vents[std::size_t(st.panels[std::size_t(k)].vent)].pull);
    if (panelAt(it.box()) >= 0)
    {
      it.taken = true;
      ventOut(it.kind == ItemKind::Gem ? 1 : 3, float(it.x + 1), float(it.y - 1), it.variant);
    }
  }
  // Crates: off the floor and out.
  for (auto& c : st.crates)
  {
    if (!c.alive || c.held)
      continue;
    const int cx = c.loose ? int(c.fx) + 1 : c.bx * kCellsPerTile + 1;
    const int cy = c.loose ? int(c.fy) + 1 : c.by * kCellsPerTile + 1;
    const int k = ventPulling(cx, cy, tx, ty);
    if (k < 0)
      continue;
    if (!c.loose)
    {
      c.loose = true;
      c.fx = float(c.bx * kCellsPerTile);
      c.fy = float(c.by * kCellsPerTile);
      c.vx = c.vy = 0.0f;
      mMap.setBlock(c.bx, c.by, Tile::Empty);
    }
    int x = int(c.fx), y = int(c.fy) + kCellsPerTile - 1;
    pullBox(x, y, kCellsPerTile, kCellsPerTile, tx, ty, st.vents[std::size_t(st.panels[std::size_t(k)].vent)].pull);
    c.fx = float(x);
    c.fy = float(y - kCellsPerTile + 1);
    c.vx = c.vy = 0.0f;
    if (panelAt(boxAt(x, y, kCellsPerTile, kCellsPerTile)) >= 0)
    {
      c.alive = false;
      ventOut(0, c.fx + 1.0f, c.fy + 1.0f, c.gems);
    }
  }
  // The candid camera outside the bay doors.
  if (st.camera >= 0 && st.cameraPanel >= 0 && mEnemies[std::size_t(st.camera)].alive)
    mEnemies[std::size_t(st.camera)].hidden = st.panels[std::size_t(st.cameraPanel)].open == 0;
}

// --- Crates --------------------------------------------------------------------------

void World::breakCrate(Crate& c)
{
  if (!c.alive)
    return;
  c.alive = false;
  const int x = c.loose ? int(c.fx) : c.bx * kCellsPerTile;
  const int y = c.loose ? int(c.fy) : c.by * kCellsPerTile;
  if (!c.loose && !c.held)
    mMap.setBlock(c.bx, c.by, Tile::Empty);
  const Vec2 at{(float(x) + 1.0f) * float(kCellSize), (float(y) + 1.0f) * float(kCellSize)};
  burst(at, rgb(150, 160, 175), rgb(255, 200, 60), 14, 1.8f, false);
  playSound(Sfx::BoxBreak);
  if (c.gems > 0)
  {
    dropGems(x, y + 1, c.gems);
    showMessage("A CRATE FULL OF GEMS!");
  }
}

bool World::shotAtStation(Projectile& pr, const CellBox& b)
{
  auto& st = mStation;
  for (auto& c : st.crates)
  {
    if (!c.alive || c.held)
      continue;
    const CellBox cb = c.loose ? CellBox{int(c.fx), int(c.fy), kCellsPerTile, kCellsPerTile}
                               : CellBox{c.bx * kCellsPerTile, c.by * kCellsPerTile, kCellsPerTile, kCellsPerTile};
    if (cb.intersects(b))
    {
      breakCrate(c);
      return !pr.pierce;
    }
  }
  // A Loader Mech's legs are their own target (the lower 3 cells).
  for (auto& e : mEnemies)
  {
    if (!e.alive || e.hidden || e.kind != EnemyKind::Loader || e.ox <= 0)
      continue;
    const CellBox legs{e.x, e.y - 2, e.w, 3};
    if (!legs.intersects(b) || b.bottom() < legs.top() || b.top() < legs.top())
      continue;
    e.ox -= std::max(1, pr.damage);
    e.flash = 8;
    if (e.ox <= 0)
    {
      e.ox = 0;
      const Vec2 c = cellCenter(legs);
      burst(c, rgb(255, 200, 60), rgb(120, 130, 150), 18, 2.0f);
      playSound(Sfx::SmallExplosion);
      showMessage("ITS LEGS ARE GONE: IT WILL VENT NOW");
    }
    else
      playSound(Sfx::Hit);
    return !pr.pierce;
  }
  return false;
}

void World::updateCrates()
{
  auto& st = mStation;
  auto& p = mPlayer;
  int tx = 0, ty = 0;
  for (auto& c : st.crates)
  {
    if (!c.alive || !c.loose || c.held)
      continue;
    if (ventPulling(int(c.fx) + 1, int(c.fy) + 1, tx, ty) >= 0)
      continue; // the vent has it
    // Falling or flying: gravity, then a landing (or a crash for a thrown one).
    c.vy = std::min(2.0f, c.vy + kCrateGravity);
    const int steps = std::max(1, int(std::ceil(std::max(std::fabs(c.vx), std::fabs(c.vy)) / 0.5f)));
    bool hit = false;
    for (int s = 0; s < steps && !hit; ++s)
    {
      const float nx = c.fx + c.vx / float(steps), ny = c.fy + c.vy / float(steps);
      const CellBox nb{int(std::floor(nx)), int(std::floor(ny)), kCellsPerTile, kCellsPerTile};
      if (c.thrown && p.state != PlayerState::Dying && nb.intersects(p.hitBox()))
      {
        hurtPlayer(1);
        hit = true;
        break;
      }
      if (mMap.overlapsSolid(nb))
      {
        hit = true;
        break;
      }
      c.fx = nx;
      c.fy = ny;
    }
    if (c.fy > float(mMap.height() + 4))
    {
      c.alive = false;
      continue;
    }
    if (!hit)
      continue;
    if (c.thrown)
    {
      breakCrate(c);
      continue;
    }
    // Lands on the block grid, if the block is free.
    const int bx = int(std::lround(c.fx / float(kCellsPerTile)));
    const int by = int(std::floor((c.fy + 1.0f) / float(kCellsPerTile)));
    if (!mMap.solid(bx * kCellsPerTile, by * kCellsPerTile) &&
        !boxAt(bx * kCellsPerTile, by * kCellsPerTile + 1, 2, 2).intersects(p.box()))
    {
      c.loose = false;
      c.bx = bx;
      c.by = by;
      mMap.setBlock(bx, by, Tile::Solid);
      playSound(Sfx::Land);
    }
    else
      breakCrate(c);
  }
  // A resting crate with nothing under it drops.
  for (auto& c : st.crates)
  {
    if (!c.alive || c.loose || c.held)
      continue;
    if (mMap.solid(c.bx * kCellsPerTile, (c.by + 1) * kCellsPerTile))
      continue;
    mMap.setBlock(c.bx, c.by, Tile::Empty);
    c.loose = true;
    c.fx = float(c.bx * kCellsPerTile);
    c.fy = float(c.by * kCellsPerTile);
    c.vx = c.vy = 0.0f;
  }
}

// --- Breach Charges ------------------------------------------------------------------

void World::placeCharge(int ox, int oy)
{
  const auto& p = mPlayer;
  BreachCharge c;
  c.fx = c.prevFx = float(ox) + 0.5f;
  c.fy = c.prevFy = float(oy) + 0.5f;
  const float f = float(p.facing);
  switch (p.stance)
  {
    case Stance::Up:
      c.vx = 0.2f * f;
      c.vy = -2.0f;
      break;
    case Stance::Crouched:
      c.vx = 0.8f * f;
      c.vy = -0.4f;
      break;
    case Stance::Down:
    case Stance::Jetpack:
      c.vy = 1.0f;
      break;
    default:
      c.vx = 1.6f * f;
      c.vy = -0.8f;
      break;
  }
  if (mMap.solid(int(std::floor(c.fx)), int(std::floor(c.fy))))
  {
    c.fx = float(p.x) + 1.5f;
    c.fy = float(p.y) - 2.5f;
  }
  mStation.charges.push_back(c);
  playSound(Sfx::Click);
}

void World::blastAt(int cx, int cy)
{
  explodeAt(cx, cy, kBlast, kChargeDamage);
  const CellBox area{cx - kBlast, cy - kBlast, kBlast * 2 + 1, kBlast * 2 + 1};
  auto& st = mStation;
  for (std::size_t i = 0; i < st.panels.size(); ++i)
    if (panelBox(st.panels[i]).intersects(area))
      breachPanel(int(i));
  for (auto& c : st.crates)
  {
    if (!c.alive)
      continue;
    const CellBox cb = c.loose ? CellBox{int(c.fx), int(c.fy), kCellsPerTile, kCellsPerTile}
                               : CellBox{c.bx * kCellsPerTile, c.by * kCellsPerTile, kCellsPerTile, kCellsPerTile};
    if (cb.intersects(area))
      breakCrate(c);
  }
  for (const auto& b : mBreakables)
  {
    const CellBox box{b.x0 * kCellsPerTile, b.y0 * kCellsPerTile, (b.x1 - b.x0 + 1) * kCellsPerTile,
      (b.y1 - b.y0 + 1) * kCellsPerTile};
    if (!b.broken && (b.by == 0 || b.by == 1 || b.by == 2) && box.intersects(area))
      hitBreakable(box, 99, 1);
  }
}

void World::detonateCharges()
{
  auto charges = std::move(mStation.charges);
  mStation.charges.clear();
  mStation.detonate = 0;
  for (const auto& c : charges)
    blastAt(int(std::floor(c.fx)), int(std::floor(c.fy)));
}

void World::updateCharges()
{
  auto& st = mStation;
  for (auto& c : st.charges)
  {
    c.prevFx = c.fx;
    c.prevFy = c.fy;
    ++c.age;
    if (c.enemy >= 0)
    {
      const Enemy& e = mEnemies[std::size_t(c.enemy)];
      if (e.alive && !e.hidden)
      {
        c.fx = float(e.x + c.ox) + 0.5f;
        c.fy = float(e.y + c.oy) + 0.5f;
        continue;
      }
      // Its enemy is gone: the charge drops.
      c.enemy = -1;
      c.stuck = false;
      c.vx = c.vy = 0.0f;
    }
    if (c.stuck)
    {
      // Stuck to a crate that has gone: it drops too.
      if (!mMap.solid(int(std::floor(c.fx + c.vx)), int(std::floor(c.fy + c.vy))))
      {
        c.stuck = false;
        c.vx = c.vy = 0.0f;
      }
      else
        continue;
    }
    c.vy = std::min(2.0f, c.vy + kChargeGravity);
    const int steps = std::max(1, int(std::ceil(std::max(std::fabs(c.vx), std::fabs(c.vy)) / 0.5f)));
    for (int s = 0; s < steps && !c.stuck; ++s)
    {
      const float nx = c.fx + c.vx / float(steps), ny = c.fy + c.vy / float(steps);
      const int cx = int(std::floor(nx)), cy = int(std::floor(ny));
      for (std::size_t i = 0; i < mEnemies.size(); ++i)
      {
        const Enemy& e = mEnemies[i];
        if (!e.alive || e.hidden || e.kind == EnemyKind::Camera || !e.box().intersects({cx, cy, 1, 1}))
          continue;
        c.enemy = int(i);
        c.ox = cx - e.x;
        c.oy = cy - e.y;
        c.stuck = true;
        break;
      }
      if (c.stuck)
        break;
      if (mMap.solid(cx, cy))
      {
        // Sticks where it hit; remember which way the surface is.
        c.vx = std::clamp(float(cx) + 0.5f - c.fx, -1.0f, 1.0f);
        c.vy = std::clamp(float(cy) + 0.5f - c.fy, -1.0f, 1.0f);
        c.stuck = true;
        playSound(Sfx::Click);
        break;
      }
      c.fx = nx;
      c.fy = ny;
    }
    if (c.stuck && c.enemy < 0)
    {
      // Rest against the surface: keep the direction to it in vx, vy.
      const float sx = c.vx, sy = c.vy;
      c.vx = std::fabs(sx) >= std::fabs(sy) ? sgnf(sx) : 0.0f;
      c.vy = std::fabs(sx) >= std::fabs(sy) ? 0.0f : sgnf(sy);
    }
  }
  st.charges.erase(std::remove_if(st.charges.begin(), st.charges.end(),
                     [&](const BreachCharge& c) { return c.fy > float(mMap.height() + 4); }),
    st.charges.end());
}

// The Breach Charge's trigger: a tap places one (two out at most); a tap
// with two out, or fire held for 8 frames, sets them all off.
bool World::breachTrigger(const Button& fire)
{
  auto& st = mStation;
  if (fire.pressed && !st.charges.empty())
  {
    if (++st.detonate == kDetonateHold)
    {
      detonateCharges();
      return true;
    }
  }
  else
    st.detonate = 0;
  if (fire.triggered && int(st.charges.size()) >= kMaxCharges)
  {
    detonateCharges();
    return true;
  }
  return false;
}

// --- Enemies -------------------------------------------------------------------------

void World::updateLoader(Enemy& e, const EnemyDef& def)
{
  auto& st = mStation;
  const auto& p = mPlayer;
  const CellBox pb = p.box();
  const bool vulnerable = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  if (e.cool > 0)
    --e.cool;
  const int mid = e.x + e.w / 2;
  if (e.tell > 0)
  {
    // The crate goes up over its head...
    if (--e.tell == 0 && e.aimX >= 0)
    {
      // ...and over at the runner.
      Crate& c = st.crates[std::size_t(e.aimX)];
      c.held = false;
      c.loose = true;
      c.thrown = true;
      c.fx = float(mid - 1);
      c.fy = float(e.y - e.h - 2);
      const float d = float((pb.x + 1) - mid);
      c.vx = std::clamp(d / 18.0f, -2.0f, 2.0f);
      c.vy = -1.4f;
      e.aimX = -1;
      e.cool = def.cooldown;
      playSound(Sfx::Whoosh);
    }
    return;
  }
  const bool inRange = vulnerable && std::abs((pb.x + 1) - mid) <= def.range && std::abs(p.y - e.y) <= 16 &&
    isOnScreen(e.box(), 0);
  if (inRange && e.cool == 0)
  {
    // A crate within reach: the top of the nearest pile.
    int best = -1;
    for (std::size_t i = 0; i < st.crates.size(); ++i)
    {
      const Crate& c = st.crates[i];
      if (!c.alive || c.loose || c.held || std::abs(c.bx * kCellsPerTile + 1 - mid) > kLiftReach)
        continue;
      if (c.by * kCellsPerTile + 1 < e.y - e.h || c.by * kCellsPerTile > e.y)
        continue;
      if (mMap.solid(c.bx * kCellsPerTile, (c.by - 1) * kCellsPerTile))
        continue; // not the top one
      if (best < 0 || std::abs(c.bx * kCellsPerTile - mid) < std::abs(st.crates[std::size_t(best)].bx * kCellsPerTile - mid))
        best = int(i);
    }
    if (best >= 0)
    {
      Crate& c = st.crates[std::size_t(best)];
      mMap.setBlock(c.bx, c.by, Tile::Empty);
      c.held = true;
      e.aimX = best;
      e.tell = def.tell;
      e.dir = (pb.x + 1) < mid ? -1 : 1;
      playSound(Sfx::Stomp);
      return;
    }
  }
  if (e.ox <= 0)
    return; // no legs: it sits where it fell
  // Patrol: a step every few frames, turning at walls and ledges.
  if (e.timer % std::max(1, def.stepEvery) != 0)
    return;
  const int nx = e.x + e.dir;
  const CellBox nb = boxAt(nx, e.y, e.w, e.h);
  const int footX = e.dir > 0 ? nb.right() : nb.left();
  if (nx < e.railX0 || nx > e.railX1 || mMap.overlapsSolid(nb) || !mMap.solidTop(footX, e.y + 1))
    e.dir = -e.dir;
  else
    e.x = nx;
  if (e.timer % (def.stepEvery * 4) == 0 && isOnScreen(e.box(), 2))
    playSound(Sfx::Stomp);
}

void World::updateWeldDrone(Enemy& e, const EnemyDef& def)
{
  auto& st = mStation;
  if (e.hidden)
    return; // asleep until the set piece
  if (e.cool > 0)
  {
    --e.cool;
    return;
  }
  if (e.dive == 0)
  {
    // The torch flares white, then it welds a run.
    if (e.tell == 0)
      e.tell = def.tell;
    if (--e.tell > 0)
      return;
    e.dive = kRun;
    if (isOnScreen(e.box(), 2))
      playSound(Sfx::Zap);
  }
  --e.dive;
  if (e.dive == 0)
    e.cool = def.cooldown;
  if (e.attach != 0)
  {
    // Climbing the wall on its right: up until the wall ends, then over
    // onto its top.
    const int wallX = e.x + e.w;
    if (mMap.solid(wallX, e.y - e.h))
    {
      if (!mMap.overlapsSolid(boxAt(e.x, e.y - 1, e.w, e.h)))
        --e.y;
    }
    else
    {
      int top = e.y;
      while (top > 0 && !mMap.solid(wallX, top))
        --top;
      e.x = wallX;
      e.y = top - 1;
      while (e.y > 0 && mMap.overlapsSolid(e.box()))
        --e.y;
      e.attach = 0;
      e.dir = 1;
    }
    st.seams.push_back({wallX - 1, e.y, kSeamLife, e.carrier});
    return;
  }
  // Along its floor, turning at its path's ends, walls and edges.
  const int nx = e.x + e.dir;
  const CellBox nb = boxAt(nx, e.y, e.w, e.h);
  const int footX = e.dir > 0 ? nb.right() : nb.left();
  const bool outside = e.railX0 >= 0 && (nx < e.railX0 || nx + e.w - 1 > e.railX1);
  if (outside || mMap.overlapsSolid(nb) || !mMap.solidTop(footX, e.y + 1))
    e.dir = -e.dir;
  else
    e.x = nx;
  for (int k = 0; k < e.w; ++k)
  {
    bool found = false;
    for (auto& s : st.seams)
      if (s.x == e.x + k && s.y == e.y)
      {
        s.life = kSeamLife;
        s.green = s.green || e.carrier;
        found = true;
      }
    if (!found)
      st.seams.push_back({e.x + k, e.y, kSeamLife, e.carrier});
  }
}

void World::updateTether(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  if (e.hidden)
    return;
  const bool paired = e.attach >= 0 && mEnemies[std::size_t(e.attach)].alive;
  if (paired)
  {
    // The lower drone leads; the upper one keeps station above it.
    if (e.variant == 0)
    {
      const Enemy& low = mEnemies[std::size_t(e.attach)];
      e.x = low.x;
      e.y = low.y - e.aimY;
      e.dir = low.dir;
      e.tell = low.tell;
      return;
    }
    // The beam: on, down for a moment, then flickering back.
    const int phase = e.timer % (kBeamOn + kBeamDown + kFlicker);
    e.tell = phase >= kBeamOn + kBeamDown ? kBeamOn + kBeamDown + kFlicker - phase : 0;
    e.dive = phase >= kBeamOn ? 1 : 0; // no beam while down or flickering
    if (e.timer % std::max(1, def.stepEvery) != 0)
      return;
    const int nx = e.x + e.dir;
    if (nx < e.railX0 || nx + e.w - 1 > e.railX1 || mMap.overlapsSolid(boxAt(nx, e.y, e.w, e.h)) ||
        mMap.overlapsSolid(boxAt(nx, e.y - e.aimY, e.w, e.h)))
      e.dir = -e.dir;
    else
      e.x = nx;
    return;
  }
  // Alone: a wobble, then a ram at the runner.
  e.attach = -1;
  e.dive = 1;
  if (e.cool > 0)
  {
    --e.cool;
    return;
  }
  if (e.tell == 0 && e.ox == 0)
  {
    e.tell = def.tell;
    return;
  }
  if (e.tell > 0)
  {
    if (--e.tell == 0)
      e.ox = kRam;
    return;
  }
  const int dx = sgn((p.x + 1) - (e.x + 1)), dy = sgn((p.y - 2) - (e.y - 1));
  if (dx != 0 && !mMap.overlapsSolid(boxAt(e.x + dx, e.y, e.w, e.h)))
    e.x += dx;
  if (dy != 0 && !mMap.overlapsSolid(boxAt(e.x, e.y + dy, e.w, e.h)))
    e.y += dy;
  if (dx != 0)
    e.dir = dx;
  if (--e.ox <= 0)
  {
    e.ox = 0;
    e.cool = 15;
  }
}

CellBox World::tetherBeam(const Enemy& low) const
{
  const Enemy& up = mEnemies[std::size_t(low.attach)];
  return {low.x, up.y + 1, 1, std::max(0, (low.y - low.h) - up.y)};
}

// --- Per frame -----------------------------------------------------------------------

void World::updateStation(const PlayerInput& input)
{
  (void)input;
  auto& st = mStation;
  if (!st.on() && st.charges.empty())
    return;
  auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  updateCharges();
  updateVents();
  updateCrates();

  // Weld seams cool down; a hot one hurts, a green one infects.
  for (auto& s : st.seams)
    --s.life;
  st.seams.erase(std::remove_if(st.seams.begin(), st.seams.end(), [](const Seam& s) { return s.life <= 0; }),
    st.seams.end());
  if (alive && p.mercy == 0)
  {
    const CellBox pb = p.box();
    for (const auto& s : st.seams)
    {
      if (s.life < 6 || !pb.intersects({s.x, s.y, 1, 1}))
        continue;
      if (s.green)
      {
        if (p.virus == 0)
        {
          infect();
          p.mercy = 20;
        }
      }
      else
        hurtPlayer(1);
      break;
    }
  }
  // Tether beams.
  if (alive)
    for (const auto& e : mEnemies)
    {
      if (!e.alive || e.hidden || e.kind != EnemyKind::Tether || e.variant != 1 || e.attach < 0 || e.dive != 0 ||
          !mEnemies[std::size_t(e.attach)].alive)
        continue;
      if (tetherBeam(e).intersects(p.hitBox()))
      {
        hurtPlayer(1);
        break;
      }
    }

  for (auto& d : st.debris)
  {
    d.x += d.vx;
    d.y += d.vy;
    d.angle += d.spin;
    --d.life;
  }
  st.debris.erase(std::remove_if(st.debris.begin(), st.debris.end(), [](const Debris& d) { return d.life <= 0; }),
    st.debris.end());

  // The cab set piece: standing on the cab's deck wakes the drones.
  if (st.setOn && !st.setFired && alive && p.state == PlayerState::OnGround)
  {
    const int bx = (p.x + 1) / kCellsPerTile, by = p.y / kCellsPerTile;
    if (by == st.setY && bx >= st.setX0 && bx <= st.setX1)
    {
      st.setFired = true;
      for (int i : st.sleepers)
      {
        mEnemies[std::size_t(i)].hidden = false;
        mEnemies[std::size_t(i)].active = true;
      }
      playSound(Sfx::Siren);
      showMessage("ZERO: MAINTENANCE CREW, PLEASE ESCORT OUR GUESTS OUT");
    }
  }
}

bool World::stationCanSave() const
{
  const auto& st = mStation;
  if (st.caught >= 0 || !st.charges.empty())
    return false;
  for (const auto& p : st.panels)
    if (p.open > 0)
      return false;
  for (const auto& c : st.crates)
    if (c.alive && (c.loose || c.held))
      return false;
  return true;
}

} // namespace gr
