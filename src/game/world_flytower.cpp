// Level 40, The Fly Tower: the Cue Sheet (cue lists, pyro, trusses,
// confetti, followspots, the towel), the Pyro Rig, Followspots, Riggers and
// Sandbag Drops; the Trigger Happy bonus (rules=jumpfire). See flytower.hpp.

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

int wrap(int v, int n) { return n <= 0 ? 0 : ((v % n) + n) % n; }

std::vector<std::string> splitIds(const std::string& s)
{
  std::vector<std::string> out;
  std::string cur;
  for (char c : s)
  {
    if (c == ',')
    {
      if (!cur.empty())
        out.push_back(cur);
      cur.clear();
    }
    else
      cur += c;
  }
  if (!cur.empty())
    out.push_back(cur);
  return out;
}

FtCueKind cueKind(const std::string& k)
{
  if (k == "wave")
    return FtCueKind::Wave;
  if (k == "truss")
    return FtCueKind::Truss;
  if (k == "confetti")
    return FtCueKind::Confetti;
  if (k == "spot")
    return FtCueKind::Spot;
  if (k == "towel")
    return FtCueKind::Towel;
  if (k == "cut")
    return FtCueKind::Cut;
  return FtCueKind::Pyro;
}

} // namespace

CellBox FtPyro::flame() const
{
  if (dir < 0)
    return {(x - h) * kCellsPerTile, (y - w + 1) * kCellsPerTile, h * kCellsPerTile, w * kCellsPerTile};
  if (dir > 0)
    return {(x + 1) * kCellsPerTile, (y - w + 1) * kCellsPerTile, h * kCellsPerTile, w * kCellsPerTile};
  return {x * kCellsPerTile, (y - h + 1) * kCellsPerTile, w * kCellsPerTile, h * kCellsPerTile};
}

CellBox FtPyro::fuseBox() const
{
  return {(x - dir) * kCellsPerTile, (y - 1) * kCellsPerTile, kCellsPerTile, 2 * kCellsPerTile};
}

CellBox FtConfetti::lane() const
{
  const int x0 = dir < 0 ? x - len + 1 : x;
  return {x0 * kCellsPerTile, (y - 2) * kCellsPerTile, len * kCellsPerTile, 3 * kCellsPerTile};
}

// --- Setup ---------------------------------------------------------------------------------

bool World::setupFlyTowerEntity(const EntityDef& e)
{
  const bool trigger = mLevel->rules.find("jumpfire") != std::string::npos;
  if (mLevel->themeKey != "fly_tower" && !trigger)
    return false;
  auto& s = mFly;
  s.on = true;
  s.trigger = trigger;
  if (e.kind == "duck")
    return true; // the duck's headset is only drawn
  if (e.kind == "cuelist")
  {
    FtCueList l;
    l.id = e.id;
    e.rect("zone", l.x0, l.y0, l.x1, l.y1);
    l.period = std::max(1, e.num("period", 300));
    s.lists.push_back(l);
    return true;
  }
  if (e.kind == "cue")
  {
    FtCue c;
    c.id = e.id;
    c.label = e.str("label");
    c.listId = e.str("list");
    c.targetIds = e.str("target");
    c.t = e.num("t", 0);
    c.kind = cueKind(e.str("kind"));
    c.stop = e.num("stop", 1);
    c.on = e.num("on", 1) != 0;
    c.dur = e.num("dur", kFtBurn);
    c.warn = e.num("warn", 0);
    c.once = e.num("once", 0) != 0;
    if (c.kind == FtCueKind::Wave)
    {
      c.waveOn = std::max(1, e.num("on", 20));
      c.waveOff = std::max(1, e.num("off", 40));
      c.offsets = e.list("offsets");
    }
    s.cues.push_back(c);
    return true;
  }
  if (e.kind == "pyro" && e.hasPos)
  {
    FtPyro py;
    py.id = e.id;
    py.x = e.x;
    py.y = e.y;
    py.h = std::max(1, e.num("h", 8));
    py.w = std::max(1, e.num("w", 2));
    const std::string d = e.str("dir", "up");
    py.dir = d == "l" ? -1 : d == "r" ? 1 : 0;
    py.fuse = e.num("fuse", 0);
    s.pyros.push_back(py);
    return true;
  }
  if (e.kind == "platform" && e.hasPos && e.str("mode") == "truss")
  {
    setupPlatform(e);
    FtTruss t;
    t.id = e.id;
    t.platform = int(mPlatforms.size()) - 1;
    for (int v : e.list("stops"))
      t.stops.push_back(v * kCellsPerTile);
    if (t.stops.empty())
      t.stops.push_back(e.y * kCellsPerTile);
    auto& pl = mPlatforms.back();
    pl.y = pl.prevY = pl.startY = t.stops.front();
    pl.target = 0; // no path of its own (a savegame reloads it as 0)
    s.trusses.push_back(t);
    return true;
  }
  if (e.kind == "confetti" && e.hasPos)
  {
    FtConfetti c;
    c.id = e.id;
    c.x = e.x;
    c.y = e.y;
    c.dir = e.str("dir", "l") == "r" ? 1 : -1;
    c.len = std::max(1, e.num("len", 10));
    s.confetti.push_back(c);
    return true;
  }
  if (e.kind == "towel" && e.hasPos)
  {
    s.towelX = e.x * kCellsPerTile;
    s.towelY0 = e.y * kCellsPerTile + 1;
    s.towelLand = e.num("land", e.y) * kCellsPerTile + 1;
    return true;
  }
  if (e.kind == "target" && e.hasPos)
  {
    FtTarget t;
    t.x = e.x;
    t.y = e.y;
    s.targets.push_back(t);
    return true;
  }
  if (e.kind == "step" && e.hasPos)
  {
    FtStep st;
    st.x = e.x * kCellsPerTile;
    st.y = e.y * kCellsPerTile + 1;
    st.act = e.str("act", "go");
    st.arg = e.str("arg");
    s.route.push_back(st);
    return true;
  }
  return false;
}

void World::setupFlyTowerEnemy(Enemy& en, const EntityDef& e)
{
  if (!mFly.on)
    return;
  if (en.kind != EnemyKind::Followspot && en.kind != EnemyKind::Rigger && en.kind != EnemyKind::SandbagDrop)
    return;
  FtActor a;
  a.id = e.id;
  a.enemy = int(&en - mEnemies.data());
  a.beamX = en.x + en.w / 2;
  mFly.actors.push_back(a);
}

void World::linkFlyTower()
{
  auto& s = mFly;
  if (mLevel->themeKey == "fly_tower" || mLevel->rules.find("jumpfire") != std::string::npos)
  {
    s.on = true;
    s.trigger = mLevel->rules.find("jumpfire") != std::string::npos;
  }
  if (!s.on)
    return;
  for (auto& c : s.cues)
  {
    for (std::size_t i = 0; i < s.lists.size(); ++i)
      if (s.lists[i].id == c.listId)
        c.list = int(i);
    for (const auto& id : splitIds(c.targetIds))
    {
      auto find = [&](const auto& v) {
        for (std::size_t i = 0; i < v.size(); ++i)
          if (v[i].id == id)
            return int(i);
        return -1;
      };
      int k = -1;
      switch (c.kind)
      {
        case FtCueKind::Pyro:
        case FtCueKind::Wave:
        case FtCueKind::Cut:
          k = find(s.pyros);
          break;
        case FtCueKind::Truss:
          k = find(s.trusses);
          break;
        case FtCueKind::Confetti:
          k = find(s.confetti);
          break;
        case FtCueKind::Spot:
          k = find(s.actors);
          break;
        case FtCueKind::Towel:
          break;
      }
      if (k >= 0)
        c.targets.push_back(k);
    }
  }
  for (auto& st : s.route)
    for (std::size_t i = 0; i < s.trusses.size(); ++i)
      if (st.act == "ride" && s.trusses[i].id == st.arg)
        st.truss = int(i);
  if (s.trigger)
  {
    // Trigger Happy fires the blaster.
    auto& p = mPlayer;
    p.weapon = Weapon::Normal;
    p.proto = -1;
    p.ammo = 0;
  }
  syncPlatformCollision();
}

// --- The cue sheet -------------------------------------------------------------------------

int World::cueIn(const FtCue& c) const
{
  if (c.kind == FtCueKind::Cut || c.t < 0 || c.list < 0 || (c.once && c.fired))
    return -1;
  const auto& l = mFly.lists[std::size_t(c.list)];
  if (!l.started)
    return -1;
  return wrap(c.t - l.t, l.period);
}

int World::pyroHeat(const FtPyro& py, int ahead) const
{
  const auto& s = mFly;
  const int k = int(&py - s.pyros.data());
  int heat = 0;
  if (py.fuseT >= 0)
  {
    if (py.fuseT >= kFtGlow)
      return 2;
    if (kFtGlow - py.fuseT <= ahead)
      heat = 1;
  }
  for (const auto& c : s.cues)
  {
    if ((c.kind != FtCueKind::Pyro && c.kind != FtCueKind::Wave) || c.list < 0)
      continue;
    const auto& l = s.lists[std::size_t(c.list)];
    if (!l.started)
      continue;
    for (std::size_t j = 0; j < c.targets.size(); ++j)
    {
      if (c.targets[j] != k)
        continue;
      if (c.kind == FtCueKind::Pyro)
      {
        if (l.t >= c.t && l.t - c.t < c.dur)
          return 2;
        if (wrap(c.t - l.t, l.period) <= ahead)
          heat = 1;
      }
      else
      {
        const int cycle = c.waveOn + c.waveOff;
        const int off = j < c.offsets.size() ? c.offsets[j] : 0;
        const int u = wrap(l.t - c.t - off, cycle);
        if (u < c.waveOn)
          return 2;
        if (cycle - u <= ahead)
          heat = 1;
      }
    }
  }
  return heat;
}

bool World::trussHome(const FtTruss& t) const
{
  return t.goal == 0 && mPlatforms[std::size_t(t.platform)].y == t.stops.front();
}

void World::fireFlyCue(FtCue& c)
{
  auto& s = mFly;
  c.fired = true;
  switch (c.kind)
  {
    case FtCueKind::Pyro:
    case FtCueKind::Wave:
      for (int k : c.targets)
        if (isOnScreen(s.pyros[std::size_t(k)].flame(), 4))
        {
          playSound(Sfx::FlameShot);
          break;
        }
      break;
    case FtCueKind::Truss:
      for (int k : c.targets)
      {
        auto& t = s.trusses[std::size_t(k)];
        t.goal = std::clamp(c.stop, 0, int(t.stops.size()) - 1);
        if (isOnScreen(mPlatforms[std::size_t(t.platform)].box(), 4))
          playSound(Sfx::Clunk);
      }
      break;
    case FtCueKind::Confetti:
      for (int k : c.targets)
        if (isOnScreen(s.confetti[std::size_t(k)].lane(), 4))
        {
          playSound(Sfx::Applause);
          break;
        }
      break;
    case FtCueKind::Spot:
      for (int k : c.targets)
      {
        s.actors[std::size_t(k)].lamp = c.on;
        if (!c.on)
          s.actors[std::size_t(k)].lock = 0;
        const Enemy& e = mEnemies[std::size_t(s.actors[std::size_t(k)].enemy)];
        if (e.alive && isOnScreen(e.box(), 8))
          playSound(Sfx::LightClunk);
      }
      break;
    case FtCueKind::Towel:
      if (s.towelX >= 0 && s.towelY < 0)
      {
        s.towelY = s.towelY0;
        showMessage("Q42 THE ANSWER");
      }
      break;
    case FtCueKind::Cut:
      break;
  }
}

void World::updateFlyCues()
{
  auto& s = mFly;
  const auto& p = mPlayer;
  for (auto& l : s.lists)
  {
    if (!l.started)
      continue;
    l.t = (l.t + 1) % l.period;
    for (auto& c : s.cues)
      if (c.list == int(&l - s.lists.data()) && c.t == l.t && !(c.once && c.fired))
        fireFlyCue(c);
  }
  // The zone the runner's feet are in (on an overlap, the later list).
  s.active = -1;
  if (!alive(p))
    return;
  const int bx = (p.x + 1) / kCellsPerTile, by = p.y / kCellsPerTile;
  for (std::size_t i = 0; i < s.lists.size(); ++i)
  {
    const auto& l = s.lists[i];
    if (bx >= l.x0 && bx <= l.x1 && by >= l.y0 && by <= l.y1)
      s.active = int(i);
  }
  if (s.active >= 0 && !s.lists[std::size_t(s.active)].started)
  {
    auto& l = s.lists[std::size_t(s.active)];
    l.started = true;
    l.t = 0;
    for (auto& c : s.cues)
      if (c.list == s.active && c.t == 0 && !(c.once && c.fired))
        fireFlyCue(c);
  }
}

// --- Pyro ----------------------------------------------------------------------------------

void World::updateFlyPyro()
{
  auto& s = mFly;
  auto& p = mPlayer;
  if (s.burnCool > 0)
    --s.burnCool;
  bool inFlame = false;
  for (auto& py : s.pyros)
  {
    if (py.fuseT >= 0 && ++py.fuseT >= kFtGlow + kFtBurn)
    {
      py.fuseT = -1;
      py.spent = true;
    }
    if (py.fuseT == kFtGlow)
      playSound(Sfx::FlameShot);
    // The Q99 cannon: the board's struck cue flickers when the runner faces it.
    if (py.dir != 0)
    {
      const int cx = py.x * kCellsPerTile + 1;
      const bool facing = alive(p) && std::abs(p.y - (py.y * kCellsPerTile + 1)) <= 2 &&
        sgn(cx - (p.x + 1)) == p.facing && std::abs(cx - (p.x + 1)) < 40;
      if (facing && !py.facing)
        py.faced = kFtCutFlicker;
      else if (py.faced > 0)
        --py.faced;
      py.facing = facing;
    }
    if (pyroHeat(py, 0) != 2)
      continue;
    const CellBox f = py.flame();
    hitBreakable(f, 99, 0);
    if (alive(p) && f.intersects(p.hitBox()))
      inFlame = true;
  }
  if (!inFlame)
    return;
  // Inside a burning column: an updraft, and a heart every few frames; the
  // jetpack (and Turbo) ride it unhurt.
  if (p.state == PlayerState::Jetpack)
  {
    mMap.moveVertically(p.x, p.y, Player::kWidth, p.height(), -1);
    return;
  }
  if (p.turbo > 0)
  {
    if (p.state == PlayerState::OnGround || p.state == PlayerState::Jumping || p.state == PlayerState::Falling)
    {
      p.state = PlayerState::Falling;
      p.frames = 0;
      mMap.moveVertically(p.x, p.y, Player::kWidth, p.height(), -3);
    }
    return;
  }
  if (p.state == PlayerState::Jumping || p.state == PlayerState::Falling)
    mMap.moveVertically(p.x, p.y, Player::kWidth, p.height(), -1);
  if (s.burnCool == 0)
  {
    hurtPlayer(1);
    s.burnCool = kFtBurnHurt;
  }
}

// --- Trusses -------------------------------------------------------------------------------

void World::updateTrusses()
{
  auto& s = mFly;
  auto& p = mPlayer;
  bool moved = false;
  for (auto& t : s.trusses)
  {
    auto& pl = mPlatforms[std::size_t(t.platform)];
    const int goal = t.stops[std::size_t(t.goal)];
    t.waiting = false;
    if (pl.y == goal)
      continue;
    const int dy = sgn(goal - pl.y);
    const bool riding = (p.state == PlayerState::OnGround || p.state == PlayerState::Recovering) && standsOn(p.box(), pl);
    // Never into the runner: it waits.
    const CellBox next{pl.x, pl.y + dy, pl.w, pl.h};
    if (!riding && alive(p) && next.intersects(p.box()))
    {
      t.waiting = true;
      continue;
    }
    // Boxes and items standing on it ride along.
    std::vector<ItemBox*> boxes;
    for (auto& b : mBoxes)
      if (b.alive && b.y + 1 == pl.y && b.x < pl.x + pl.w && pl.x < b.x + 2)
        boxes.push_back(&b);
    std::vector<Item*> items;
    for (auto& it : mItems)
      if (!it.taken && it.y + 1 == pl.y && it.x < pl.x + pl.w && pl.x < it.x + 2)
        items.push_back(&it);
    if (!movePlatform(pl, 0, dy))
    {
      t.waiting = true;
      continue;
    }
    for (auto* b : boxes)
      b->y += dy;
    for (auto* it : items)
      it->y += dy;
    moved = true;
  }
  if (moved)
    syncPlatformCollision();
}

// --- Confetti ------------------------------------------------------------------------------

void World::updateConfetti()
{
  auto& s = mFly;
  auto& p = mPlayer;
  if (!alive(p) || mCharacterIndex == 1 || p.state == PlayerState::Ladder)
    return; // Rocco isn't pushed
  for (const auto& c : s.cues)
  {
    if (c.kind != FtCueKind::Confetti || c.list < 0)
      continue;
    const auto& l = s.lists[std::size_t(c.list)];
    if (!l.started || l.t < c.t || l.t - c.t >= kFtConfettiPush)
      continue;
    for (int k : c.targets)
    {
      const auto& cf = s.confetti[std::size_t(k)];
      if (cf.lane().intersects(p.box()))
        mMap.moveHorizontally(p.x, p.y, Player::kWidth, p.height(), cf.dir);
    }
  }
}

// --- Sandbags ------------------------------------------------------------------------------

void World::updateBags()
{
  auto& s = mFly;
  auto& p = mPlayer;
  for (std::size_t i = 0; i < s.bags.size();)
  {
    auto& b = s.bags[i];
    bool gone = false;
    for (int step = 0; step < 2 && !gone; ++step)
    {
      const int under = b.y + 4;
      bool landed = false;
      for (int cx = b.x; cx < b.x + 4; ++cx)
        landed = landed || mMap.solidTop(cx, under) || mMap.solid(cx, under);
      if (!landed)
      {
        ++b.y;
        if (b.y > mMap.height())
          gone = true;
        continue;
      }
      // It lands: the catwalk blocks under it break.
      const int by = under / kCellsPerTile, bx = b.x / kCellsPerTile;
      int mask = 0;
      for (int k = 0; k < 2; ++k)
        if (mMap.block(bx + k, by) == Tile::Platform)
        {
          mMap.setBlock(bx + k, by, Tile::Empty);
          mask |= 1 << k;
        }
      if (mask != 0)
        s.gaps.push_back({bx, by, kFtRehang, mask});
      const Vec2 c = cellCenter(b.box());
      burst(c, rgb(200, 170, 120), rgb(140, 110, 70), 16, 1.6f);
      if (isOnScreen(b.box(), 4))
        playSound(Sfx::Thud);
      gone = true;
    }
    if (!gone && !b.hit && alive(p) && b.box().intersects(p.hitBox()))
    {
      b.hit = true;
      hurtPlayer(1);
    }
    if (gone)
      s.bags.erase(s.bags.begin() + std::ptrdiff_t(i));
    else
      ++i;
  }
  // Broken catwalks are hung again (never under the runner).
  for (std::size_t i = 0; i < s.gaps.size();)
  {
    auto& g = s.gaps[i];
    const CellBox area{g.bx * kCellsPerTile, g.by * kCellsPerTile - 1, 2 * kCellsPerTile, kCellsPerTile + 1};
    if (--g.t > 0 || area.intersects(p.box()))
    {
      g.t = std::max(g.t, 0);
      ++i;
      continue;
    }
    for (int k = 0; k < 2; ++k)
      if ((g.mask >> k & 1) != 0 && mMap.block(g.bx + k, g.by) == Tile::Empty)
        mMap.setBlock(g.bx + k, g.by, Tile::Platform);
    s.gaps.erase(s.gaps.begin() + std::ptrdiff_t(i));
  }
}

// --- Every frame ---------------------------------------------------------------------------

void World::updateFlyTower()
{
  auto& s = mFly;
  auto& p = mPlayer;
  // Trigger Happy: a press on the ground jumped; its shot comes now.
  if (s.groundPress)
  {
    s.groundPress = false;
    if (alive(p) && p.state != PlayerState::Ladder)
      fireShot();
  }
  updateFlyCues();
  updateFlyPyro();
  updateTrusses();
  updateConfetti();
  updateBags();
  // Lit: a followspot has had the runner long enough for its beam to turn amber.
  s.lit = false;
  for (const auto& a : s.actors)
  {
    const Enemy& e = mEnemies[std::size_t(a.enemy)];
    if (e.kind == EnemyKind::Followspot && e.alive && a.lamp && a.lock >= kFtSpotLock)
      s.lit = true;
  }
  if (s.lit)
  {
    s.litX = p.x + 1;
    s.litY = p.y;
  }
  // The towel: it falls to the deck and lies there; a touch is a full heal once a life.
  if (s.towelY >= 0)
  {
    s.towelY = std::min(s.towelLand, s.towelY + kFtTowelFall);
    const CellBox towel{s.towelX, s.towelY - 1, 2, 2};
    if (!s.towelUsed && alive(p) && towel.intersects(p.box()))
    {
      s.towelUsed = true;
      p.hp = p.maxHp;
      showMessage("DON'T PANIC - FULL HEALTH");
      playSound(Sfx::Health);
      burst(cellCenter(towel), rgb(255, 255, 255), rgb(120, 200, 255), 16, 1.4f);
    }
  }
  // The route: back to the step the runner's floor needs after a fall, then
  // past every step done.
  if (p.state == PlayerState::OnGround)
    while (s.routeAt > 0)
    {
      const auto& prev = s.route[std::size_t(s.routeAt - 1)];
      int level = prev.y;
      if (prev.truss >= 0)
        level = s.trusses[std::size_t(prev.truss)].stops.back() - 1;
      if (p.y <= level + 6)
        break;
      --s.routeAt;
    }
  while (s.routeAt < int(s.route.size()) && flyTowerStepDone(s.routeAt))
    ++s.routeAt;
}

bool World::flyTowerStepDone(int i) const
{
  const auto& s = mFly;
  const auto& st = s.route[std::size_t(i)];
  const auto& p = mPlayer;
  if (!alive(p))
    return false;
  if (st.truss >= 0)
  {
    // Up at its top stop's floor (or higher).
    const int top = s.trusses[std::size_t(st.truss)].stops.back() - 1;
    return p.y <= top && p.state == PlayerState::OnGround;
  }
  if (p.y < st.y - 8)
    return true;
  return std::abs(p.x + 1 - st.x) <= 4 && std::abs(p.y - st.y) <= 2 && p.state == PlayerState::OnGround;
}

// --- The Pyro Rig --------------------------------------------------------------------------

bool World::pyroJet() const
{
  const auto& p = mPlayer;
  return p.weapon == Weapon::Proto && p.proto == int(ProtoId::PyroRig) && p.ammo > 0;
}

CellBox World::pyroStream() const
{
  const auto& p = mPlayer;
  if (p.stance == Stance::Up)
  {
    int len = 0;
    const int x = p.x + 1, y0 = p.y - p.height();
    while (len < kFtStreamLen && !mMap.solid(x, y0 - len))
      ++len;
    if (len < kFtStreamLen)
      ++len; // licks the block it hits
    return {x - 1, y0 - len + 1, 2, std::max(1, len)};
  }
  const int gy = p.y + (p.stance == Stance::Crouched ? -1 : -2);
  const int x0 = p.facing > 0 ? p.x + 3 : p.x - 1;
  int len = 0;
  while (len < kFtStreamLen && !mMap.solid(x0 + p.facing * len, gy))
    ++len;
  if (len < kFtStreamLen)
    ++len; // licks the block it hits
  len = std::max(1, len);
  return {p.facing > 0 ? x0 : x0 - len + 1, gy - 1, len, 2};
}

void World::updatePyroRig(const Button& fire)
{
  auto& s = mFly;
  auto& p = mPlayer;
  auto outOfAmmo = [&] {
    p.ammo = 0;
    p.weapon = Weapon::Normal;
    p.proto = -1;
    s.stream = 0;
    showMessage("OUT OF AMMO - BACK TO THE BLASTER");
  };
  if (p.state == PlayerState::Jetpack)
  {
    // The jetpack: a unit of fuel every 2 frames, free in a burning column.
    s.stream = 0;
    bool free = p.turbo > 0;
    for (const auto& py : s.pyros)
      free = free || (pyroHeat(py, 0) == 2 && py.flame().intersects(p.hitBox()));
    if (!free && ++s.jetTick % 2 == 1 && --p.ammo <= 0)
      outOfAmmo();
    return;
  }
  s.jetTick = 0;
  if (!fire.pressed)
  {
    s.stream = 0;
    return;
  }
  ++s.stream;
  p.muzzleTicks = 4;
  p.muzzleStance = p.stance;
  if (s.stream == 1)
    playSound(Sfx::FlameShot);
  if ((s.stream - 1) % kFtStreamEvery == 0)
  {
    const CellBox cone = pyroStream();
    const int damage = p.turbo > 0 ? 2 : protoDef(ProtoId::PyroRig).damage;
    for (auto& e : mEnemies)
    {
      if (!e.alive || e.hidden || !e.box().intersects(cone))
        continue;
      e.active = true;
      const bool was = e.alive;
      shotHitsEnemy(e, p.facing, damage);
      if (was && !e.alive)
        ++mStats.protoKills;
    }
    hitBreakable(cone, damage, 0);
    for (auto& py : s.pyros)
      if (py.fuse > 0 && py.fuseT < 0 && !py.spent && py.fuseBox().intersects(cone) && --py.fuse == 0)
      {
        py.fuseT = 0;
        playSound(Sfx::FuseLit);
      }
  }
  // A unit of ammo every 2 frames of stream (Turbo: free).
  if (s.stream % 2 == 1 && p.turbo == 0 && --p.ammo <= 0)
    outOfAmmo();
}

// --- Shots ---------------------------------------------------------------------------------

bool World::shotAtFlyTower(Projectile& pr, const CellBox& b)
{
  auto& s = mFly;
  (void)pr;
  // The Q99 cannon's fuse box: two hits and it fires.
  for (auto& py : s.pyros)
    if (py.fuse > 0 && py.fuseT < 0 && !py.spent && py.fuseBox().intersects(b))
    {
      if (--py.fuse == 0)
      {
        py.fuseT = 0;
        playSound(Sfx::FuseLit);
        showMessage("Q99 - THE CUT CUE GOES OFF");
      }
      else
        playSound(Sfx::Hit);
      return true;
    }
  // Trigger Happy's discs.
  for (auto& t : s.targets)
    if (!t.hit && t.box().intersects(b))
    {
      t.hit = true;
      ++s.hits;
      const Vec2 c = cellCenter(t.box());
      addScore(300, c);
      burst(c, rgb(255, 230, 120), rgb(255, 80, 60), 14, 1.6f);
      playSound(Sfx::Hit);
      return true;
    }
  return false;
}

// --- Trigger Happy -------------------------------------------------------------------------

PlayerInput World::flyTowerInput(const PlayerInput& raw)
{
  auto& s = mFly;
  const auto& p = mPlayer;
  // Jump and fire are one button: on the ground a press jumps and fires,
  // in the air it only fires.
  PlayerInput in = raw;
  const bool press = raw.jump.triggered || raw.fire.triggered;
  const bool held = raw.jump.pressed || raw.fire.pressed;
  const bool ground = p.state == PlayerState::OnGround || p.state == PlayerState::Ladder;
  in.jump = {held && ground, press && ground};
  in.fire = {press && !ground, press && !ground};
  if (press && ground && alive(p))
    s.groundPress = true;
  return in;
}

// --- Enemies -------------------------------------------------------------------------------

FtActor* World::flyActor(const Enemy& e)
{
  for (auto& a : mFly.actors)
    if (a.enemy == int(&e - mEnemies.data()))
      return &a;
  return nullptr;
}

void World::updateFollowspot(Enemy& e, const EnemyDef& /*def*/)
{
  FtActor* a = flyActor(e);
  if (!a)
    return;
  const auto& p = mPlayer;
  const int mid = p.x + 1;
  const bool reach = alive(p) && std::abs(mid - (e.x + e.w / 2)) <= kFtSpotReachX && std::abs(p.y - e.y) <= kFtSpotReachY;
  if (!a->lamp || !reach)
  {
    a->lock = 0;
    return;
  }
  // The beam follows the runner at half a cell a frame.
  if (e.timer % 2 == 0 && a->beamX != mid)
    a->beamX += sgn(mid - a->beamX);
  const bool on = std::abs(a->beamX - mid) <= 3;
  a->lock = on ? a->lock + 1 : 0;
  if (a->lock == kFtSpotLock)
    playSound(Sfx::LightClunk);
}

void World::updateRigger(Enemy& e, const EnemyDef& def)
{
  FtActor* a = flyActor(e);
  if (!a)
    return;
  auto& s = mFly;
  const auto& p = mPlayer;
  const int mid = p.x + 1;
  if (mid != e.x + e.w / 2)
    e.dir = sgn(mid - (e.x + e.w / 2));
  if (e.cool > 0)
    --e.cool;
  if (!alive(p))
  {
    a->aim = 0;
    e.tell = 0;
    return;
  }
  if (a->aim == 0)
  {
    if (e.cool > 0 || !isOnScreen(e.box(), 0))
      return;
    // One awake Rigger a screen: none starts while another is aiming.
    for (const auto& o : s.actors)
      if (&o != a && o.aim > 0 && mEnemies[std::size_t(o.enemy)].alive)
        return;
    if (s.lit)
    {
      a->live = true;
      a->aimX = mid;
      a->aimY = p.y - 2;
    }
    else if (s.litX >= 0 && std::abs(s.litX - mid) <= 24 && std::abs(s.litY - p.y) <= 12)
    {
      // Blind: at the runner's last lit spot.
      a->live = false;
      a->aimX = s.litX;
      a->aimY = s.litY - 2;
    }
    else
    {
      a->live = false;
      a->aimX = mid;
      a->aimY = p.y - 2;
    }
    a->aim = 1;
  }
  else
    ++a->aim;
  // Lit, the laser sight follows the runner.
  if (a->live && s.lit)
  {
    a->aimX = mid;
    a->aimY = p.y - 2;
  }
  e.tell = std::max(1, kFtRiggerAim - a->aim + 1);
  if (a->aim == kFtRiggerAim - kFtRiggerBeep)
    playSound(Sfx::Beep);
  if (a->aim < kFtRiggerAim)
    return;
  a->aim = 0;
  e.tell = 0;
  e.cool = def.cooldown;
  const int fx = e.dir > 0 ? e.x + e.w : e.x - 1, fy = e.y - 3;
  Projectile pr;
  pr.kind = ShotKind::Enemy;
  pr.w = pr.h = 1;
  pr.speed = 1;
  pr.damage = 1;
  const float dx = float(a->aimX - fx), dy = float(a->aimY - fy);
  const float len = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
  pr.precise = true;
  pr.fx = float(fx);
  pr.fy = float(fy);
  pr.vx = dx / len;
  pr.vy = dy / len;
  pr.dx = sgn(int(std::lround(pr.vx * 2.0f)));
  pr.dy = sgn(int(std::lround(pr.vy * 2.0f)));
  pr.x = pr.prevX = fx;
  pr.y = pr.prevY = fy;
  mProjectiles.push_back(pr);
  playSound(Sfx::EnemyShot);
}

void World::updateSandbagDrop(Enemy& e, const EnemyDef& def)
{
  FtActor* a = flyActor(e);
  if (!a)
    return;
  auto& s = mFly;
  const auto& p = mPlayer;
  if (e.cool > 0)
    --e.cool;
  if (a->tell > 0)
  {
    // The bag's shadow grows; then the cut.
    ++a->tell;
    e.tell = std::max(1, kFtBagTell - a->tell + 1);
    if (a->tell > kFtBagTell)
    {
      s.bags.push_back({a->bagX, a->bagY, false});
      a->tell = 0;
      e.tell = 0;
      e.cool = def.cooldown;
      playSound(Sfx::Whoosh);
    }
    return;
  }
  const int mid = p.x + 1, ex = e.x + e.w / 2;
  if (e.cool > 0 || !alive(p) || std::abs(mid - ex) > kFtBagReach || p.state == PlayerState::Ladder)
    return;
  int top = 0;
  if (p.y > e.y + 4 && p.y < e.y + 40)
    top = (e.y + 1) / kCellsPerTile * kCellsPerTile + kCellsPerTile; // hung under its own floor
  else if (std::abs(p.y - e.y) <= 4)
    top = std::max(0, p.y - p.height() - 14); // from the flies, over the runner's head
  else
    return;
  const int x = std::max(0, mid / kCellsPerTile - 1) * kCellsPerTile;
  // Out of anything solid.
  int tries = 0;
  while (tries < 16 && mMap.overlapsSolid({x, top, 4, 4}))
  {
    ++top;
    ++tries;
  }
  if (tries >= 16)
    return;
  a->bagX = x;
  a->bagY = top;
  a->tell = 1;
  e.tell = kFtBagTell;
}

// --- Lives and savegames -------------------------------------------------------------------

void World::resetFlyTower()
{
  auto& s = mFly;
  s.bags.clear();
  s.burnCool = 0;
  s.stream = 0;
  s.jetTick = 0;
  s.groundPress = false;
  s.towelUsed = false;
  for (auto& c : s.cues)
    if (c.once)
      c.fired = false;
  for (auto& a : s.actors)
  {
    a.aim = 0;
    a.tell = 0;
    a.lock = 0;
  }
}

bool World::flyTowerCanSave() const
{
  return mFly.bags.empty() && !mFly.trigger;
}

std::vector<int> World::flyTowerSave() const
{
  const auto& s = mFly;
  std::vector<int> v{s.routeAt, s.towelY, s.towelUsed ? 1 : 0, s.hits, s.litX, s.litY, s.burnCool, s.lit ? 1 : 0};
  for (const auto& l : s.lists)
  {
    v.push_back(l.started ? 1 : 0);
    v.push_back(l.t);
  }
  for (const auto& c : s.cues)
    v.push_back(c.fired ? 1 : 0);
  for (const auto& t : s.trusses)
  {
    v.push_back(t.goal);
    v.push_back(t.waiting ? 1 : 0);
  }
  for (const auto& py : s.pyros)
  {
    v.push_back(py.fuse);
    v.push_back(py.fuseT);
    v.push_back(py.spent ? 1 : 0);
  }
  for (const auto& a : s.actors)
  {
    v.push_back(a.lamp ? 1 : 0);
    v.push_back(a.beamX);
    v.push_back(a.lock);
    v.push_back(a.aim);
    v.push_back(a.aimX);
    v.push_back(a.aimY);
    v.push_back(a.live ? 1 : 0);
    v.push_back(a.tell);
    v.push_back(a.bagX);
    v.push_back(a.bagY);
  }
  for (const auto& t : s.targets)
    v.push_back(t.hit ? 1 : 0);
  v.push_back(int(s.gaps.size()));
  for (const auto& g : s.gaps)
  {
    v.push_back(g.bx);
    v.push_back(g.by);
    v.push_back(g.t);
    v.push_back(g.mask);
  }
  return v;
}

bool World::validFlyTowerSave(const std::vector<int>& v) const
{
  const auto& s = mFly;
  const std::size_t fixed = 8 + s.lists.size() * 2 + s.cues.size() + s.trusses.size() * 2 + s.pyros.size() * 3 +
    s.actors.size() * 10 + s.targets.size();
  if (v.size() < fixed + 1 || v[fixed] < 0)
    return false;
  return v.size() == fixed + 1 + std::size_t(v[fixed]) * 4;
}

void World::loadFlyTower(const std::vector<int>& v)
{
  auto& s = mFly;
  std::size_t i = 0;
  s.routeAt = v[i++];
  s.towelY = v[i++];
  s.towelUsed = v[i++] != 0;
  s.hits = v[i++];
  s.litX = v[i++];
  s.litY = v[i++];
  s.burnCool = v[i++];
  s.lit = v[i++] != 0;
  for (auto& l : s.lists)
  {
    l.started = v[i++] != 0;
    l.t = v[i++];
  }
  for (auto& c : s.cues)
    c.fired = v[i++] != 0;
  for (auto& t : s.trusses)
  {
    t.goal = v[i++];
    t.waiting = v[i++] != 0;
  }
  for (auto& py : s.pyros)
  {
    py.fuse = v[i++];
    py.fuseT = v[i++];
    py.spent = v[i++] != 0;
  }
  for (auto& a : s.actors)
  {
    a.lamp = v[i++] != 0;
    a.beamX = v[i++];
    a.lock = v[i++];
    a.aim = v[i++];
    a.aimX = v[i++];
    a.aimY = v[i++];
    a.live = v[i++] != 0;
    a.tell = v[i++];
    a.bagX = v[i++];
    a.bagY = v[i++];
  }
  for (auto& t : s.targets)
    t.hit = v[i++] != 0;
  const int n = v[i++];
  s.gaps.clear();
  for (int k = 0; k < n; ++k)
  {
    FtGap g;
    g.bx = v[i++];
    g.by = v[i++];
    g.t = v[i++];
    g.mask = v[i++];
    s.gaps.push_back(g);
    for (int d = 0; d < 2; ++d)
      if ((g.mask >> d & 1) != 0 && mMap.block(g.bx + d, g.by) == Tile::Platform)
        mMap.setBlock(g.bx + d, g.by, Tile::Empty);
  }
  s.bags.clear();
  syncPlatformCollision();
}

} // namespace gr
