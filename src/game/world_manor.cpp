// Level 23, Fright Night Manor (SPEC 23): the Mirror World. The level is on
// one of two sides at a time, the real manor or its reflection. Layers,
// enemies, boxes and items tagged `side=` exist only on their side (layers
// drawn as a ghost outline on the other). Standing in front of a mirror and
// pressing Up steps through it: 12 frames of shimmer, then the other side,
// at the same spot. Portrait Ghosts leave their portraits and chase through
// walls (seen only in the reflection, or as silhouettes in the lightning);
// a Silver Crossbow bolt pins whatever it hits for 75 frames, and a pinned
// ghost is a 2-block platform. Haunted Armors wake when you pass and swing
// their halberds; Poltergeists throw books, plates and candlesticks. Two
// levers open the ballroom. The Both Sides bonus (rules=split_mirror) runs
// a second runner on the same input with left and right swapped.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace gr
{

namespace
{

std::uint32_t lcg(std::uint32_t& s)
{
  s = s * 1664525u + 1013904223u;
  return s >> 8;
}

int sideOf(const std::string& s)
{
  return s == "real" ? kSideReal : (s == "mirror" ? kSideMirror : -1);
}

bool standing(const Player& p)
{
  return p.state == PlayerState::OnGround || p.state == PlayerState::Recovering;
}

} // namespace

// --- Setup ---------------------------------------------------------------------------------

bool World::setupManorEntity(const EntityDef& e)
{
  auto& m = mManor;
  if (e.kind == "mirror" && e.hasPos && !e.has("angle")) // (with an angle: Level 10's sun mirrors)
  {
    ManorMirror mi;
    mi.id = e.id;
    mi.x = e.x;
    mi.y = e.y;
    const std::string kind = e.str("kind");
    mi.kind = kind == "broken" ? MirrorKind::Broken : (kind == "crypt" ? MirrorKind::Crypt : MirrorKind::Normal);
    const auto land = e.list("land");
    if (land.size() == 2)
    {
      mi.toX = land[0] * kCellsPerTile;
      mi.toY = land[1] * kCellsPerTile + 1;
    }
    mManorLinks.push_back({int(m.mirrors.size()), e.str("to")});
    m.mirrors.push_back(mi);
    m.on = true;
    return true;
  }
  if (e.kind == "switch" && e.str("kind") == "lever")
  {
    ManorLever l;
    l.id = e.id.empty() ? e.str("id") : e.id;
    l.x = e.hasPos ? e.x : e.num("x");
    l.y = e.hasPos ? e.y : e.num("y");
    m.levers.push_back(l);
    m.on = true;
    return true;
  }
  if (e.kind == "sideitem" && e.hasPos)
  {
    mManorSideItems.push_back({e.x, e.y, sideOf(e.str("side"))});
    m.on = true;
    return true;
  }
  if (e.kind == "exitside")
  {
    m.exitSide = sideOf(e.str("side"));
    m.on = true;
    return true;
  }
  if (e.kind == "boltbox" && e.hasPos)
  {
    ItemBox b;
    b.content = ItemKind::Proto;
    b.x = e.x * kCellsPerTile;
    b.y = e.y * kCellsPerTile + 1;
    m.boltBox = int(mBoxes.size());
    mBoxes.push_back(b);
    m.on = true;
    return true;
  }
  if (e.kind == "prize" && e.str("kind") == "lance_vampire" && e.hasPos)
  {
    m.lanceX = e.x;
    m.lanceY = e.y;
    m.lanceScore = e.num("score", 10000);
    m.lanceSide = sideOf(e.str("side"));
    m.on = true;
    return true;
  }
  if (e.kind == "coffin")
  {
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    if (e.rect("rect", x0, y0, x1, y1))
    {
      m.coffinX = x0;
      m.coffinY = y0;
      m.coffinW = x1 - x0 + 1;
    }
    const auto sign = e.list("sign");
    if (sign.size() == 2)
    {
      m.signX = sign[0];
      m.signY = sign[1];
    }
    m.on = true;
    return true;
  }
  if (e.kind == "botroute")
  {
    std::stringstream ss(e.str("steps"));
    std::string tok;
    while (std::getline(ss, tok, ','))
      mManorRoute.push_back(tok);
    m.on = true;
    return true;
  }
  if (e.kind == "ghostbridge")
  {
    m.bridgeEdge = e.num("edge", -1);
    m.bridgeLanding = e.num("landing", -1);
    m.bridgeRow = e.num("row", -1);
    m.on = true;
    return true;
  }
  if (e.kind == "manordeco")
  {
    ManorDeco d;
    d.kind = e.str("kind");
    d.text = e.str("text");
    d.side = sideOf(e.str("side"));
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    if (e.rect("rect", x0, y0, x1, y1))
    {
      d.x0 = x0;
      d.y0 = y0;
      d.x1 = x1;
      d.y1 = y1;
    }
    else if (e.hasPos)
      d.x0 = d.x1 = e.x, d.y0 = d.y1 = e.y;
    m.decos.push_back(d);
    m.on = true;
    return true;
  }
  if (e.kind == "twin" && e.hasPos)
  {
    m.split.on = true;
    m.split.twinX = e.x * kCellsPerTile;
    m.split.twinY = e.y * kCellsPerTile + 1;
    const auto top = e.list("top"), bottom = e.list("bottom");
    if (top.size() == 2)
    {
      m.split.topExitX = top[0];
      m.split.topExitY = top[1];
    }
    if (bottom.size() == 2)
    {
      m.split.botExitX = bottom[0];
      m.split.botExitY = bottom[1];
    }
    m.split.divider = e.num("divider", 11);
    m.on = true;
    return true;
  }
  return false;
}

void World::setupManorLayer(const EntityDef& e, int index)
{
  const int side = sideOf(e.str("side"));
  if (side >= 0)
  {
    mManor.layers.push_back({index, side});
    mManor.on = true;
  }
  if (e.str("driver") == "switch" && e.has("switch"))
  {
    LeverDoor d;
    d.layer = index;
    mManorDoorLevers.push_back({int(mManor.doors.size()), e.str("switch")});
    mManor.doors.push_back(d); // kept in linkManor if its switches are levers
  }
}

void World::setupManorEnemy(Enemy& en, const EntityDef& e)
{
  const int index = int(&en - mEnemies.data());
  const int side = sideOf(e.str("side"));
  if (side >= 0)
  {
    if (mManor.enemySide.size() <= std::size_t(index))
      mManor.enemySide.resize(std::size_t(index) + 1, -1);
    mManor.enemySide[std::size_t(index)] = side;
    mManor.on = true;
  }
  if (en.kind == EnemyKind::PortraitGhost)
  {
    ManorState::Portrait p;
    p.x = e.x;
    p.y = e.y;
    p.ghost = index;
    mManor.portraits.push_back(p);
    en.attach = 0; // in its portrait
    en.hidden = true;
    mManor.on = true;
  }
  if (en.kind == EnemyKind::HauntedArmor)
  {
    en.attach = 0; // standing still until you pass
    mManor.on = true;
  }
  if (en.kind == EnemyKind::Poltergeist)
  {
    const std::string obj = e.str("object", "book");
    en.variant = obj == "tureen" ? 1 : (obj == "candelabra" ? 2 : 0);
    mManor.on = true;
  }
}

void World::linkManor()
{
  auto& m = mManor;
  if (mLevel->rules.find("split_mirror") != std::string::npos)
    m.split.on = m.on = true;
  if (!m.on)
  {
    m.doors.clear(); // other levels' switch layers
    return;
  }
  m.enemySide.resize(mEnemies.size(), -1);
  m.boxSide.assign(mBoxes.size(), -1);
  for (auto& s : mManorSideItems)
  {
    for (std::size_t i = 0; i < mBoxes.size(); ++i)
      if (mBoxes[i].x / kCellsPerTile == s.x && mBoxes[i].y / kCellsPerTile == s.y)
        m.boxSide[i] = s.side;
    for (auto& it : mItems)
      if (it.x / kCellsPerTile == s.x && it.y / kCellsPerTile == s.y)
      {
        it.side = s.side;
        s.item = 1;
        s.copy = it;
      }
  }
  for (const auto& [mi, to] : mManorLinks)
    for (std::size_t j = 0; j < m.mirrors.size(); ++j)
      if (!to.empty() && m.mirrors[j].id == to)
        m.mirrors[std::size_t(mi)].to = int(j);
  for (const auto& [di, names] : mManorDoorLevers)
  {
    std::stringstream ss(names);
    std::string tok;
    while (std::getline(ss, tok, ','))
      for (std::size_t j = 0; j < m.levers.size(); ++j)
        if (m.levers[j].id == tok)
          m.doors[std::size_t(di)].levers.push_back(int(j));
  }
  m.doors.erase(std::remove_if(m.doors.begin(), m.doors.end(), [](const LeverDoor& d) { return d.levers.empty(); }),
    m.doors.end());
  for (const auto& d : m.doors)
    mLayers[std::size_t(d.layer)].scriptSolid = true;
  for (const auto& tok : mManorRoute)
  {
    ManorStep st;
    const auto colon = tok.find(':');
    const std::string name = tok.substr(0, colon);
    for (std::size_t j = 0; j < m.mirrors.size(); ++j)
      if (m.mirrors[j].id == name)
        st.mirror = int(j);
    for (std::size_t j = 0; j < m.levers.size(); ++j)
      if (m.levers[j].id == name)
        st.lever = int(j);
    if (colon != std::string::npos)
      st.side = sideOf(tok.substr(colon + 1));
    if (st.mirror >= 0 || st.lever >= 0)
      m.route.push_back(st);
  }
  m.step = 0;
  m.stepFresh = true;
  // The level starts on the side the runner stands on: the real manor.
  m.side = kSideReal;
  for (const auto& sl : m.layers)
  {
    auto& l = mLayers[std::size_t(sl.layer)];
    l.style = 6;
    l.scriptSolid = sl.side == m.side;
  }
  parkManor();
  if (m.split.on)
  {
    mTwin = mPlayer;
    mTwin.x = mTwin.prevX = m.split.twinX;
    mTwin.y = mTwin.prevY = m.split.twinY;
    mTwin.facing = -mPlayer.facing;
    m.split.lastTopX = mPlayer.x;
    m.split.lastTopY = mPlayer.y;
    m.split.lastBotX = mTwin.x;
    m.split.lastBotY = mTwin.y;
  }
}

// --- The side ------------------------------------------------------------------------------

bool World::offSide(int side) const
{
  return mManor.on && side >= 0 && side != mManor.side;
}

void World::parkManor()
{
  auto& m = mManor;
  for (std::size_t i = 0; i < m.enemySide.size() && i < mEnemies.size(); ++i)
    if (m.enemySide[i] >= 0)
      mEnemies[i].hidden = offSide(m.enemySide[i]);
  for (std::size_t i = 0; i < m.boxSide.size() && i < mBoxes.size(); ++i)
  {
    const bool parked = std::find(m.parkedBoxes.begin(), m.parkedBoxes.end(), int(i)) != m.parkedBoxes.end();
    if (offSide(m.boxSide[i]) && mBoxes[i].alive)
    {
      mBoxes[i].alive = false;
      m.parkedBoxes.push_back(int(i));
    }
    else if (!offSide(m.boxSide[i]) && parked)
    {
      mBoxes[i].alive = true;
      m.parkedBoxes.erase(std::find(m.parkedBoxes.begin(), m.parkedBoxes.end(), int(i)));
    }
  }
  // Parked items stay in the list as taken (updateItems keeps them).
  for (auto& it : mItems)
    if (it.side >= 0 && offSide(it.side) && !it.taken)
      it.taken = it.parked = true;
    else if (it.side >= 0 && !offSide(it.side) && it.parked)
      it.taken = it.parked = false;
}

void World::setManorSide(int side)
{
  auto& m = mManor;
  m.side = side;
  for (const auto& sl : m.layers)
  {
    auto& l = mLayers[std::size_t(sl.layer)];
    l.scriptSolid = sl.side == side;
    applyLayer(l, l.scriptSolid);
  }
  // Ghost platforms stay (the ghosts are on both sides); shots in flight go.
  for (auto& pr : mProjectiles)
    pr.alive = false;
  parkManor();
}

void World::flipManor(int mirror, bool backwards)
{
  auto& m = mManor;
  m.flip = kMirrorShimmer;
  m.flipTo = mirror;
  m.backwards = backwards;
  playSound(Sfx::MirrorFlip);
}

// --- Levers, mirrors, the route ------------------------------------------------------------

void World::throwLever(int i)
{
  auto& m = mManor;
  ManorLever& l = m.levers[std::size_t(i)];
  if (l.state == 1)
    return;
  l.state = 1;
  l.swing = 20;
  playSound(Sfx::Lever);
  for (const auto& d : m.doors)
  {
    bool all = !d.levers.empty();
    for (int k : d.levers)
      all = all && m.levers[std::size_t(k)].state == 1;
    auto& layer = mLayers[std::size_t(d.layer)];
    if (all && layer.scriptSolid)
    {
      layer.scriptSolid = false;
      applyLayer(layer, false);
      showMessage("THE BALLROOM DOORS CREAK OPEN");
      playSound(Sfx::Creak);
    }
    else if (!all)
      showMessage("A LEVER CLUNKS - SOMEWHERE A LOCK TURNS");
  }
}

int World::manorMirrorAt(const Player& p) const
{
  const int cx = p.x + 1;
  for (std::size_t i = 0; i < mManor.mirrors.size(); ++i)
  {
    const ManorMirror& mi = mManor.mirrors[i];
    if (cx >= mi.x * kCellsPerTile && cx < (mi.x + 2) * kCellsPerTile && p.y >= mi.y * kCellsPerTile &&
        p.y < (mi.y + 3) * kCellsPerTile)
      return int(i);
  }
  return -1;
}

int World::manorLeverAt(const Player& p) const
{
  const CellBox pb = p.box();
  for (std::size_t i = 0; i < mManor.levers.size(); ++i)
  {
    const ManorLever& l = mManor.levers[i];
    const CellBox lb{l.x * kCellsPerTile - 1, l.y * kCellsPerTile - 2, kCellsPerTile + 2, kCellsPerTile + 2};
    if (lb.intersects(pb))
      return int(i);
  }
  return -1;
}

void World::advanceManorRoute()
{
  auto& m = mManor;
  while (m.step < int(m.route.size()))
  {
    const ManorStep& s = m.route[std::size_t(m.step)];
    bool done = false;
    if (s.lever >= 0)
      done = m.levers[std::size_t(s.lever)].state == 1;
    else
      done = m.side == s.side && (m.stepFresh || m.lastFlip == s.mirror);
    m.stepFresh = false;
    if (!done)
      break;
    ++m.step;
    m.stepFresh = true;
  }
}

// --- Enemies -------------------------------------------------------------------------------

void World::updatePortraitGhost(Enemy& e, const EnemyDef& def)
{
  (void)def;
  const auto& p = mPlayer;
  if (e.attach == 0)
  {
    e.hidden = true; // in its portrait (updateManor brings it out)
    return;
  }
  e.hidden = false;
  if (e.attach == 4)
    return; // drifting home to its portrait (updateManor moves it)
  if (e.cool > 0)
    --e.cool;
  // It hovers at the runner's feet (pinned there, its top is a step up).
  const int tx = p.x + 1 - e.w / 2, ty = p.y + 1;
  const int dx = tx - e.x, dy = ty - e.y;
  e.dir = dx < 0 ? -1 : 1;
  if (e.attach == 1)
  {
    // Hover after the runner through walls: down or up to their height
    // first, then across at half a cell a frame.
    e.y += dy > 0 ? 1 : (dy < 0 ? -1 : 0);
    if (e.timer % 2 == 0)
      e.x += dx > 0 ? 1 : (dx < 0 ? -1 : 0);
    if (e.cool == 0 && std::abs(dx) <= kGhostReach && std::abs(dy) <= 6 && p.state != PlayerState::Dying)
    {
      e.attach = 2;
      e.tell = kGhostTell;
      e.aimX = dx > 0 ? 1 : (dx < 0 ? -1 : 0);
      e.aimY = dy > 0 ? 1 : (dy < 0 ? -1 : 0);
    }
  }
  else if (e.attach == 2)
  {
    if (--e.tell <= 0)
    {
      e.attach = 3;
      e.dive = kGhostLunge;
      playSound(Sfx::GhostHiss);
    }
  }
  else if (e.attach == 3)
  {
    e.x += e.aimX;
    if (e.timer % 2 == 0)
      e.y += e.aimY;
    if (--e.dive <= 0)
    {
      e.attach = 1;
      e.cool = 40;
    }
  }
}

void World::updateHauntedArmor(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  const CellBox b = e.box();
  const int pcx = p.x + 1, ecx = b.x + b.w / 2;
  const bool sameFloor = std::abs(p.y - e.y) <= 2 && p.state != PlayerState::Dying;
  if (e.attach == 0)
  {
    // Still as a suit of armour until a runner on its floor passes close.
    if (sameFloor && std::abs(pcx - ecx) <= 3 * kCellsPerTile + 1)
    {
      e.attach = 1;
      playSound(Sfx::ArmorClank);
    }
    return;
  }
  if (e.cool > 0)
    --e.cool;
  if (e.attach == 2)
  {
    // The visor glows, then the halberd comes down in front of it.
    if (--e.tell > 0)
      return;
    const CellBox swing{e.dir > 0 ? b.right() + 1 : b.left() - 3 * kCellsPerTile, b.top(), 3 * kCellsPerTile, b.h};
    if (swing.intersects(p.hitBox()) && p.state != PlayerState::Dying)
      hurtPlayer(1);
    playSound(Sfx::Swoosh);
    e.attach = 1;
    e.cool = def.cooldown;
    e.dive = 6; // the swing's frames (drawn)
    return;
  }
  if (e.dive > 0)
    --e.dive;
  if (!mMap.onSolidGround(b))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, 1);
    return;
  }
  e.dir = pcx < ecx ? -1 : 1;
  const int gap = e.dir > 0 ? p.x - b.right() - 1 : b.left() - (p.x + 3);
  if (sameFloor && gap <= 3 * kCellsPerTile - 1 && gap >= -2 && e.cool == 0)
  {
    e.attach = 2;
    e.tell = kArmorTell;
    return;
  }
  if (!sameFloor || e.timer % std::max(1, def.stepEvery) != 0 || gap <= 1)
    return;
  const int aheadX = e.dir > 0 ? b.right() + 1 : b.left() - 1;
  const bool wall = e.dir > 0 ? mMap.touchingRightWall(b) : mMap.touchingLeftWall(b);
  if (!wall && mMap.solidTop(aheadX, b.bottom() + 1))
    e.x += e.dir;
}

void World::updatePoltergeist(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  e.hidden = false;
  if (e.cool > 0)
    --e.cool;
  const int dx = (p.x + 1) - (e.x + 1);
  const bool inRange = std::abs(dx) <= 8 * kCellsPerTile && std::abs(p.y - e.y) <= 10 && p.state != PlayerState::Dying;
  if (e.tell > 0)
  {
    if (--e.tell > 0)
      return;
    // Throw: a lob that lands where the runner stands now.
    const float g = 0.15f;
    const float sx = float(e.x + 1), sy = float(e.y - 2);
    const float tx = float(p.x + 1), ty = float(p.y - 2);
    const float T = std::clamp(std::fabs(tx - sx) / 1.2f, 8.0f, 18.0f);
    Projectile pr;
    pr.kind = ShotKind::Enemy;
    pr.w = pr.h = 2;
    pr.speed = 1;
    pr.damage = 1;
    pr.precise = true;
    pr.range = 90;
    pr.gy = g;
    pr.fx = sx;
    pr.fy = sy;
    pr.vx = (tx - sx) / T;
    pr.vy = (ty - sy - 0.5f * g * T * T) / T;
    pr.dx = dx < 0 ? -1 : 1;
    pr.x = pr.prevX = int(sx);
    pr.y = pr.prevY = int(sy);
    pr.thrown = 1 + e.variant; // drawn as a book, a plate or a candlestick
    if (e.variant == 1 && (++mManor.plates % 3) == 0)
      pr.carrier = true; // every third plate glows green
    mProjectiles.push_back(pr);
    playSound(Sfx::Throw);
    e.cool = def.cooldown;
    return;
  }
  if (inRange && e.cool == 0)
  {
    e.tell = kPolterTell; // the next thing rattles and lifts
    playSound(Sfx::Rattle);
  }
}

// A Silver Crossbow bolt pins what it hits; a pinned ghost is a platform.
// A Turbo hit knocks a Haunted Armor over. 0 go on, 1 the shot is spent,
// 2 it passes this enemy.
int World::shotAtManorEnemy(Projectile& pr, Enemy& e)
{
  if (e.kind == EnemyKind::HauntedArmor && mPlayer.turbo > 0)
  {
    damageEnemy(e, e.hp);
    return 1;
  }
  // A pinned ghost is a platform: shots pass it.
  if (e.kind == EnemyKind::PortraitGhost && e.stun > 0)
    return 2;
  const bool bolt = pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::SilverCrossbow);
  if (!bolt)
    return 0;
  damageEnemy(e, pr.damage);
  if (!e.alive)
    return 1;
  e.stun = kPinFrames;
  e.tell = 0;
  if (e.kind == EnemyKind::PortraitGhost)
  {
    e.attach = 1;
    pinGhost(int(&e - mEnemies.data()));
  }
  playSound(Sfx::BoltThunk);
  return 1;
}

void World::pinGhost(int index)
{
  auto& m = mManor;
  const Enemy& e = mEnemies[std::size_t(index)];
  for (const auto& pl : m.platforms)
    if (pl.enemy == index)
      return;
  ManorState::GhostPlatform pl;
  pl.enemy = index;
  pl.bx = (e.x + 1) / kCellsPerTile;
  pl.by = (e.y - e.h + 1) / kCellsPerTile;
  for (int k = 0; k < 2; ++k)
    if (mMap.block(pl.bx + k, pl.by) == Tile::Empty)
    {
      mMap.setBlock(pl.bx + k, pl.by, Tile::Platform);
      pl.set |= 1 << k;
    }
  m.platforms.push_back(pl);
}

void World::unpinGhosts(bool all)
{
  auto& m = mManor;
  for (std::size_t i = 0; i < m.platforms.size();)
  {
    const auto& pl = m.platforms[i];
    const Enemy& e = mEnemies[std::size_t(pl.enemy)];
    if (!all && e.alive && e.stun > 0)
    {
      ++i;
      continue;
    }
    for (int k = 0; k < 2; ++k)
      if (pl.set & (1 << k))
        mMap.setBlock(pl.bx + k, pl.by, Tile::Empty);
    m.platforms.erase(m.platforms.begin() + std::ptrdiff_t(i));
  }
}

// --- Shots ---------------------------------------------------------------------------------

// Levers, the broken mirror, the coffin and Lance's portrait take shots.
bool World::shotAtManor(Projectile& pr, const CellBox& b)
{
  auto& m = mManor;
  for (std::size_t i = 0; i < m.levers.size(); ++i)
  {
    const ManorLever& l = m.levers[i];
    const CellBox lb{l.x * kCellsPerTile, l.y * kCellsPerTile, kCellsPerTile, kCellsPerTile};
    if (l.state == 0 && lb.intersects(b))
    {
      throwLever(int(i));
      return true;
    }
  }
  for (auto& mi : m.mirrors)
  {
    const CellBox mb{mi.x * kCellsPerTile, mi.y * kCellsPerTile, 2 * kCellsPerTile, 3 * kCellsPerTile};
    if (mi.kind == MirrorKind::Broken && mb.intersects(b))
    {
      if (mi.third == 0)
        playSound(Sfx::GlassChime);
      mi.third = kThirdWorld;
      return true;
    }
  }
  if (m.coffinX >= 0)
  {
    const CellBox cb{m.coffinX * kCellsPerTile, m.coffinY * kCellsPerTile, m.coffinW * kCellsPerTile, 2 * kCellsPerTile};
    if (cb.intersects(b))
    {
      if (m.coffin == 0)
      {
        m.coffin = 40;
        m.coffinWoke = true;
        playSound(Sfx::CoffinSlam);
      }
      return true;
    }
  }
  if (m.lanceX >= 0 && !m.lanceShot && !offSide(m.lanceSide))
  {
    const CellBox lb{m.lanceX * kCellsPerTile, m.lanceY * kCellsPerTile, 2 * kCellsPerTile, 2 * kCellsPerTile};
    if (lb.intersects(b))
    {
      m.lanceShot = true;
      m.lanceWink = 60;
      addScore(m.lanceScore, cellCenter(lb));
      showMessage("LANCE MARQUEE, PRINCE OF DARKNESS - SECRET FOUND");
      playSound(Sfx::LettersComplete);
      return true;
    }
  }
  return false;
}

// --- Update --------------------------------------------------------------------------------

void World::updateManor(const PlayerInput& input)
{
  auto& m = mManor;
  auto& p = mPlayer;
  const bool upTap = input.up && !m.prevUp;
  const bool downTap = input.down && !m.prevDown;
  m.prevUp = input.up;
  m.prevDown = input.down;

  // A new checkpoint keeps the side you are on.
  int active = 0;
  for (const auto& cp : mCheckpoints)
    active += cp.active ? 1 : 0;
  if (active != m.checkpoints)
  {
    if (m.checkpoints >= 0 && active > m.checkpoints)
      m.cpSide = m.side;
    m.checkpoints = active;
  }

  for (auto& l : m.levers)
    if (l.swing > 0)
      --l.swing;
  for (auto& mi : m.mirrors)
    if (mi.third > 0)
      --mi.third;
  if (m.coffin > 0 && --m.coffin == 0)
    showMessage("I SAID NIGHT SHIFT");
  if (m.lanceWink > 0)
    --m.lanceWink;
  if (m.reflection > 0)
    --m.reflection;

  // Stepping through: the side flips halfway through the shimmer.
  if (m.flip > 0)
  {
    --m.flip;
    if (m.flip == kMirrorShimmer / 2)
    {
      const ManorMirror& mi = m.mirrors[std::size_t(m.flipTo)];
      if (mi.kind == MirrorKind::Normal)
      {
        setManorSide(1 - m.side);
        m.lastFlip = m.flipTo;
        if (m.backwards)
        {
          m.reflection = kBackwardsHold;
          m.reflX = p.x;
          m.reflY = p.y;
        }
      }
      else if (mi.to >= 0)
      {
        // The broken mirror and the crypt's: to the other one's spot.
        const ManorMirror& to = m.mirrors[std::size_t(mi.to)];
        p.x = p.prevX = mi.toX >= 0 ? mi.toX : to.x * kCellsPerTile;
        p.y = p.prevY = mi.toY >= 0 ? mi.toY : (to.y + 3) * kCellsPerTile - 1;
        mCamera.centerOn(cameraTarget(), mMap.width(), mMap.height());
      }
    }
    advanceManorRoute();
    return;
  }

  if (p.state != PlayerState::Dying && p.state != PlayerState::Teleporting && standing(p) && !input.fire.pressed)
  {
    const int mi = manorMirrorAt(p);
    if (mi >= 0)
    {
      const ManorMirror& mr = m.mirrors[std::size_t(mi)];
      const bool open = mr.kind == MirrorKind::Normal || (mr.kind == MirrorKind::Broken && mr.third > 0) ||
        mr.kind == MirrorKind::Crypt;
      if (open && (upTap || (downTap && mr.kind == MirrorKind::Normal)))
        flipManor(mi, downTap && !upTap);
    }
    const int lv = manorLeverAt(p);
    if (lv >= 0 && upTap)
      throwLever(lv);
  }

  // Lightning.
  if (m.flash > 0)
    --m.flash;
  if (--m.lightning <= 0)
  {
    m.flash = 4;
    m.lightning = kLightningEvery - 60 + int(lcg(m.seed) % 121);
    playSound(Sfx::Thunder);
  }

  // Portrait Ghosts: out of the portrait when the runner comes within 10
  // blocks; back 90 frames after one is shot down (two out at most).
  int out = 0;
  for (auto& pt : m.portraits)
  {
    const Enemy& g = mEnemies[std::size_t(pt.ghost)];
    out += g.alive && g.attach != 0 ? 1 : 0;
  }
  for (auto& pt : m.portraits)
  {
    Enemy& g = mEnemies[std::size_t(pt.ghost)];
    const int d = std::max(std::abs(p.x / kCellsPerTile - pt.x), std::abs(p.y / kCellsPerTile - pt.y));
    if (!g.alive)
    {
      if (pt.back == 0)
        pt.back = kGhostReturn;
      else if (--pt.back == 0)
      {
        // Back in its frame, waiting.
        g.alive = true;
        g.hp = enemyDef(g.def).hp;
        g.x = g.prevX = pt.x * kCellsPerTile;
        g.y = g.prevY = pt.y * kCellsPerTile + g.h - 1;
        g.attach = 0;
        g.stun = 0;
        g.hidden = true;
        g.drawSnap = true;
      }
      continue;
    }
    // Far from its portrait (the runner gone elsewhere), it goes back in.
    if (g.attach == 1 && g.stun == 0 && d > kGhostLeash)
    {
      const int hx = pt.x * kCellsPerTile - g.x, hy = pt.y * kCellsPerTile + g.h - 1 - g.y;
      if (std::abs(hx) <= 1 && std::abs(hy) <= 1)
      {
        g.attach = 0;
        g.hidden = true;
        --out;
      }
      else
      {
        g.x += hx > 0 ? 1 : (hx < 0 ? -1 : 0);
        g.y += hy > 0 ? 1 : (hy < 0 ? -1 : 0);
        g.attach = 4; // drifting home (updatePortraitGhost leaves it be)
      }
      continue;
    }
    if (g.attach == 4)
      g.attach = 1;
    if (g.attach == 0 && d <= kGhostWake && out < 2 && p.state != PlayerState::Dying)
    {
      g.attach = 1;
      g.active = true;
      g.hidden = false;
      g.cool = 20;
      ++out;
      playSound(Sfx::GhostHiss);
    }
  }
  unpinGhosts(false);

  // The refill box comes back while the runner is short of bolts.
  if (m.boltBox >= 0)
  {
    ItemBox& b = mBoxes[std::size_t(m.boltBox)];
    const bool low = !(p.weapon == Weapon::Proto && p.proto == int(ProtoId::SilverCrossbow) && p.ammo >= 4);
    if (b.alive || !low)
      m.boltT = 0;
    else if (++m.boltT >= kBoltRespawn)
    {
      b.alive = true;
      m.boltT = 0;
      burst(cellCenter(b.box()), rgb(220, 230, 255), rgb(140, 120, 220), 10, 1.2f);
    }
  }

  // Off-side enemies stay frozen and out of sight.
  for (std::size_t i = 0; i < m.enemySide.size() && i < mEnemies.size(); ++i)
    if (m.enemySide[i] >= 0)
      mEnemies[i].hidden = offSide(m.enemySide[i]);

  advanceManorRoute();
}

bool World::manorRunnerHit()
{
  return mManor.flip > 0; // stepping through a mirror: nothing touches you
}

void World::resetManor()
{
  auto& m = mManor;
  m.flip = 0;
  m.reflection = 0;
  unpinGhosts(true);
  for (auto& pt : m.portraits)
  {
    Enemy& g = mEnemies[std::size_t(pt.ghost)];
    if (g.alive)
    {
      g.x = g.prevX = pt.x * kCellsPerTile;
      g.y = g.prevY = pt.y * kCellsPerTile + g.h - 1;
      g.attach = 0;
      g.stun = 0;
      g.hidden = true;
      g.drawSnap = true;
    }
  }
  if (m.side != m.cpSide)
    setManorSide(m.cpSide);
  m.stepFresh = true;
  advanceManorRoute();
}

// --- Both Sides ----------------------------------------------------------------------------

void World::updateTwin(const PlayerInput& input)
{
  auto& s = mManor.split;
  PlayerInput mirrored = input;
  mirrored.left = input.right;
  mirrored.right = input.left;
  // The top runner first (already moved), then the bottom one in its place.
  if (standing(mPlayer))
  {
    s.lastTopX = mPlayer.x;
    s.lastTopY = mPlayer.y;
  }
  std::swap(mPlayer, mTwin);
  updatePlayer(mirrored);
  if (standing(mPlayer))
  {
    s.lastBotX = mPlayer.x;
    s.lastBotY = mPlayer.y;
  }
  const CellBox pb = mPlayer.box();
  for (auto& it : mItems)
    if (!it.taken && it.heldBy < 0 && it.pickupDelay <= 0 && it.box().intersects(pb))
      collectItem(it);
  std::swap(mPlayer, mTwin);
  // A runner that falls off its half goes back to its last floor.
  const int topLimit = s.divider * kCellsPerTile, botLimit = mMap.height() - 1;
  auto back = [&](Player& r, int x, int y) {
    r.x = r.prevX = x;
    r.y = r.prevY = y;
    r.state = PlayerState::OnGround;
    r.frames = 0;
  };
  if (mPlayer.y >= topLimit)
    back(mPlayer, s.lastTopX, s.lastTopY);
  if (mTwin.y >= botLimit)
    back(mTwin, s.lastBotX, s.lastBotY);
  // Both on their exits at once: out.
  auto onExit = [&](const Player& r, int ex, int ey) {
    const CellBox zone{ex * kCellsPerTile - 1, (ey + 1) * kCellsPerTile - 6, 4, 6};
    return standing(r) && zone.intersects(r.box());
  };
  s.topOn = onExit(mPlayer, s.topExitX, s.topExitY);
  s.botOn = onExit(mTwin, s.botExitX, s.botExitY);
  if (s.topOn && s.botOn && mState == WorldState::Playing)
  {
    mPlayer.state = PlayerState::Teleporting;
    mTwin.state = PlayerState::Teleporting;
    mState = WorldState::Exiting;
    mStateFrames = 0;
    playSound(Sfx::Teleport);
  }
}

// --- Saves ---------------------------------------------------------------------------------

bool World::manorCanSave() const
{
  return mManor.flip == 0 && mManor.platforms.empty() && !mManor.split.on;
}

std::vector<int> World::manorSave() const
{
  // [side, route step, checkpoint side, levers..., parked boxes (one flag a
  // box)..., side items still there (one flag an @ sideitem item)...,
  // lance shot]
  const auto& m = mManor;
  std::vector<int> s{m.side, m.step, m.cpSide};
  for (const auto& l : m.levers)
    s.push_back(l.state);
  for (std::size_t i = 0; i < m.boxSide.size(); ++i)
    s.push_back(std::find(m.parkedBoxes.begin(), m.parkedBoxes.end(), int(i)) != m.parkedBoxes.end() ? 1 : 0);
  for (const auto& si : mManorSideItems)
    if (si.item >= 0)
    {
      bool there = false;
      for (const auto& it : mItems)
        there = there || (it.side == si.side && it.x == si.copy.x && it.y == si.copy.y && (!it.taken || it.parked));
      s.push_back(there ? 1 : 0);
    }
  s.push_back(m.lanceShot ? 1 : 0);
  return s;
}

bool World::validManorSave(const std::vector<int>& s) const
{
  const auto& m = mManor;
  std::size_t items = 0;
  for (const auto& si : mManorSideItems)
    items += si.item >= 0 ? 1 : 0;
  return s.size() == 4 + m.levers.size() + m.boxSide.size() + items && (s[0] == 0 || s[0] == 1);
}

void World::loadManor(const std::vector<int>& s)
{
  auto& m = mManor;
  std::size_t k = 0;
  const int side = s[k++];
  m.step = s[k++];
  m.cpSide = s[k++];
  for (auto& l : m.levers)
    l.state = s[k++];
  m.parkedBoxes.clear();
  for (std::size_t i = 0; i < m.boxSide.size(); ++i)
    if (s[k++])
      m.parkedBoxes.push_back(int(i));
  // The side items: the save's item list has the ones that were out (with
  // no side); put back each one that is still there, side and all.
  for (const auto& si : mManorSideItems)
  {
    if (si.item < 0)
      continue;
    mItems.erase(std::remove_if(mItems.begin(), mItems.end(),
                   [&](const Item& it) { return it.x == si.copy.x && it.y == si.copy.y && it.kind == si.copy.kind; }),
      mItems.end());
    if (s[k++])
    {
      Item it = si.copy;
      it.taken = it.parked = false;
      mItems.push_back(it);
    }
  }
  m.lanceShot = s[k++] != 0;
  m.flip = 0;
  m.stepFresh = false;
  m.checkpoints = -1;
  setManorSide(side);
  for (const auto& d : m.doors)
  {
    bool all = !d.levers.empty();
    for (int i : d.levers)
      all = all && m.levers[std::size_t(i)].state == 1;
    auto& layer = mLayers[std::size_t(d.layer)];
    layer.scriptSolid = !all;
    applyLayer(layer, !all);
  }
}

} // namespace gr
