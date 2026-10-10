// Level 26, Kaiju Tonight: the model city that breaks (buildings, towers
// that topple into bridges, the station roof over the subway), power lines
// and their dark sectors, the Atomic Breath, Model Tank squads, Toy Jet
// formations, Maser Trucks, the damage meter, and Gorgazilla on the harbor
// piers. Drawing is in world_city_draw.cpp; the Top-Down Rampage bonus in
// world_topdown.cpp.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>

namespace gr
{

namespace
{

constexpr int kSwipeShow = 4;
constexpr int kSwipeReach = 8; // cells to both sides
constexpr int kDiveWade = 20;  // most frames spent wading into the bay
constexpr int kRipple = 20;    // ripples show where he surfaces this long before
constexpr int kSplash = 4;     // cells around his surfacing spot
constexpr int kStagger = 30;
constexpr int kSit = 45;
constexpr int kLeave = 120;
constexpr int kLizardScore = 25000;

CellBox blockRect(int x0, int y0, int x1, int y1)
{
  return {x0 * kCellsPerTile, y0 * kCellsPerTile, (x1 - x0 + 1) * kCellsPerTile, (y1 - y0 + 1) * kCellsPerTile};
}

CellBox buildingBox(const Building& b)
{
  return blockRect(b.x0, b.y0, b.x1, b.y1);
}

CellBox bridgeBox(const Building& b)
{
  return blockRect(b.bx0, b.by0, b.bx1, b.by1);
}

} // namespace

// --- Setup ---------------------------------------------------------------------------------

bool World::setupCityEntity(const EntityDef& e)
{
  auto& c = mCity;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  if (e.kind == "building" && e.rect("rect", x0, y0, x1, y1))
  {
    Building b;
    b.id = e.id;
    const std::string kind = e.str("kind", "house");
    b.kind = kind == "block" ? BuildingKind::Block
      : kind == "roof"       ? BuildingKind::Roof
      : kind == "tower"      ? BuildingKind::Tower
      : kind == "prize"      ? BuildingKind::Prize
                             : BuildingKind::House;
    b.x0 = std::min(x0, x1);
    b.x1 = std::max(x0, x1);
    b.y0 = std::min(y0, y1);
    b.y1 = std::max(y0, y1);
    const int def = b.kind == BuildingKind::House ? 100
      : b.kind == BuildingKind::Tower             ? 500
      : b.kind == BuildingKind::Prize             ? 10000
                                                  : 250;
    b.rating = e.num("rating", def);
    int u0 = 0, v0 = 0, u1 = 0, v1 = 0;
    if (e.rect("under", u0, v0, u1, v1))
    {
      b.ux0 = std::min(u0, u1);
      b.ux1 = std::max(u0, u1);
      b.uy0 = std::min(v0, v1);
      b.uy1 = std::max(v0, v1);
    }
    if (b.kind == BuildingKind::Tower)
    {
      // It lies 2 rows thick on the ground it stood on, as long as it was
      // tall, starting beside its foot.
      b.fall = e.str("fall", "r") == "l" ? -1 : 1;
      const int len = b.y1 - b.y0 + 1;
      b.by0 = b.y1 - 1;
      b.by1 = b.y1;
      b.bx0 = b.fall > 0 ? b.x1 + 1 : b.x0 - len;
      b.bx1 = b.fall > 0 ? b.x1 + len : b.x0 - 1;
    }
    for (int by = b.y0; by <= b.y1; ++by)
      for (int bx = b.x0; bx <= b.x1; ++bx)
        mMap.setBlock(bx, by, Tile::Solid);
    if (b.id == "diner")
    {
      c.diner = int(c.buildings.size());
      c.dinerX = (b.x1 + 1) * kCellsPerTile;
    }
    c.buildings.push_back(b);
    c.on = true;
    return true;
  }
  if (e.kind == "powerline")
  {
    const auto path = e.path("path");
    if (path.size() < 2)
      return false;
    PowerLine l;
    l.id = e.id;
    l.sector = e.str("sector");
    l.x0 = std::min(path[0].first, path[1].first) * kCellsPerTile;
    l.x1 = (std::max(path[0].first, path[1].first) + 1) * kCellsPerTile - 1;
    l.y = path[0].second * kCellsPerTile + 1;
    c.lines.push_back(l);
    c.on = true;
    return true;
  }
  if (e.kind == "sector" && e.rect("rect", x0, y0, x1, y1))
  {
    c.sectors.push_back({e.id, std::min(x0, x1), std::max(x0, x1), false});
    c.on = true;
    return true;
  }
  if (e.kind == "squad" && e.rect("rect", x0, y0, x1, y1))
  {
    const int def = enemyIndex(e.str("kind", "model_tank"));
    if (def < 0)
      return false;
    TankSquad s;
    s.x0 = std::min(x0, x1) * kCellsPerTile;
    s.x1 = (std::max(x0, x1) + 1) * kCellsPerTile - 1;
    const int y = std::max(y0, y1) * kCellsPerTile + 1;
    const int w = enemyDef(def).w, h = enemyDef(def).h;
    // Every spot along the rect a tank fits (buildings stand in the way),
    // then `count` of them spread out over those spots.
    std::vector<int> spots;
    for (int x = s.x0; x + w - 1 <= s.x1; ++x)
      if (!mMap.overlapsSolid(boxAt(x, y, w, h)) && (spots.empty() || x >= spots.back() + w + 1))
        spots.push_back(x);
    const int count = std::min(int(spots.size()), std::max(1, e.num("count", 4)));
    for (int i = 0; i < count; ++i)
    {
      const int spot = count > 1 ? spots[std::size_t(i * (int(spots.size()) - 1) / (count - 1))] : spots[0];
      spawnEnemy(def, spot, y);
      Enemy& en = mEnemies.back();
      en.attach = int(c.squads.size()); // its squad
      en.dir = i % 2 ? 1 : -1;
      en.railX0 = s.x0;
      en.railX1 = s.x1;
      s.tanks.push_back(int(mEnemies.size()) - 1);
    }
    c.squads.push_back(s);
    c.on = true;
    return true;
  }
  if (e.kind == "formation")
  {
    const int def = enemyIndex(e.str("kind", "toy_jet"));
    if (def < 0)
      return false;
    JetFormation f;
    f.trigger = e.num("trigger") * kCellsPerTile;
    f.row = e.num("row", 24) * kCellsPerTile + 1;
    const int count = std::max(1, e.num("count", 3));
    for (int i = 0; i < count; ++i)
    {
      spawnEnemy(def, f.trigger, f.row);
      Enemy& en = mEnemies.back();
      en.hidden = true;
      en.attach = int(c.formations.size());
      en.variant = i;
      f.jets.push_back(int(mEnemies.size()) - 1);
    }
    c.formations.push_back(f);
    c.on = true;
    return true;
  }
  if (e.kind == "lizard" && e.hasPos)
  {
    auto& z = c.lizard;
    z.on = true;
    z.x = z.prevX = float(e.x * kCellsPerTile);
    z.startX = e.x * kCellsPerTile;
    z.floor = e.y * kCellsPerTile + 1;
    if (e.rect("arena", x0, y0, x1, y1))
    {
      z.x0 = x0;
      z.y0 = y0;
      z.x1 = x1;
      z.y1 = y1;
    }
    if (e.rect("door", x0, y0, x1, y1))
    {
      z.doorX = x0;
      z.doorY0 = std::min(y0, y1);
      z.doorY1 = std::max(y0, y1);
    }
    const auto piers = e.list("piers"); // west x0,x1, east x0,x1 (blocks)
    if (piers.size() == 4)
      for (int i = 0; i < 2; ++i)
      {
        z.piers[i][0] = piers[std::size_t(i * 2)] * kCellsPerTile;
        z.piers[i][1] = (piers[std::size_t(i * 2 + 1)] + 1) * kCellsPerTile - Lizard::kW;
      }
    const auto drops = e.path("drops");
    for (std::size_t i = 0; i < drops.size() && i < 2; ++i)
    {
      z.dropX[i] = drops[i].first;
      z.dropY = drops[i].second;
    }
    const auto turbo = e.list("turbo");
    if (turbo.size() == 2)
    {
      z.turboX = turbo[0];
      z.turboY = turbo[1];
    }
    const auto dish = e.list("dish");
    if (dish.size() == 2)
    {
      z.dishX = dish[0] * kCellsPerTile;
      z.dishY = dish[1] * kCellsPerTile + 1;
    }
    c.on = true;
    return true;
  }
  if (e.kind == "fullheal" && e.hasPos)
  {
    c.fullhealX = e.x;
    c.fullhealY = e.y;
    c.on = true;
    return true;
  }
  if (e.kind == "photo" && e.hasPos)
  {
    c.photoX = e.x * kCellsPerTile;
    c.photoY = e.y * kCellsPerTile + 1;
    return true;
  }
  if (e.kind == "fluid" && e.str("kind") == "pond" && mLevel->themeKey == "model_city" &&
      e.rect("rect", x0, y0, x1, y1))
  {
    c.ponds.push_back(blockRect(std::min(x0, x1), std::min(y0, y1), std::max(x0, x1), std::max(y0, y1)));
    c.on = true;
    return true;
  }
  return false;
}

void World::setupCityEnemy(Enemy& en, const EntityDef& e)
{
  (void)e;
  if (en.kind == EnemyKind::ModelTank || en.kind == EnemyKind::ToyJet || en.kind == EnemyKind::MaserTruck)
  {
    en.attach = -1; // no squad, no formation
    en.railX0 = -1;
    mCity.on = true;
  }
}

void World::linkCity()
{
  auto& c = mCity;
  if (!c.on)
    return;
  // A Maser Truck belongs to the sector it starts in (variant: the sector).
  for (auto& e : mEnemies)
    if (e.kind == EnemyKind::MaserTruck)
    {
      e.variant = -1;
      for (std::size_t s = 0; s < c.sectors.size(); ++s)
        if (e.x / kCellsPerTile >= c.sectors[s].x0 && e.x / kCellsPerTile <= c.sectors[s].x1)
          e.variant = int(s);
    }
}

// --- Breaking things -----------------------------------------------------------------------

void World::breakBuilding(int index)
{
  auto& c = mCity;
  Building& b = c.buildings[std::size_t(index)];
  if (!b.standing())
    return;
  b.cracks = 0;
  b.burn = 0;
  if (b.kind == BuildingKind::Tower)
  {
    b.state = BuildingState::Leaning;
    b.t = 0;
    playSound(Sfx::Creak);
    return;
  }
  b.state = BuildingState::Crumbling;
  b.t = 0;
  for (int by = b.y0; by <= b.y1; ++by)
    for (int bx = b.x0; bx <= b.x1; ++bx)
      mMap.setBlock(bx, by, Tile::Empty);
  if (b.ux0 >= 0)
    for (int by = b.uy0; by <= b.uy1; ++by)
      for (int bx = b.ux0; bx <= b.ux1; ++bx)
        mMap.setBlock(bx, by, Tile::Empty);
  rateBreak(b);
  burst(cellCenter(buildingBox(b)), mTheme.rockLight, mTheme.rock, 14 + 4 * (b.x1 - b.x0 + b.y1 - b.y0), 1.6f);
  playSound(b.kind == BuildingKind::House ? Sfx::BoxBreak : Sfx::BuildingCrumble);
  if (b.kind == BuildingKind::Prize)
    showMessage("THE HALCYON TOWER! +" + std::to_string(b.rating));
  else if (b.kind == BuildingKind::Roof)
    showMessage("THE STATION ROOF GIVES WAY!");
  // What stood on it falls (the runner falls by itself).
  for (auto& e : mEnemies)
    if (e.alive && !e.hidden && e.kind != EnemyKind::ToyJet && e.box().bottom() + 1 == buildingBox(b).top() &&
        e.box().intersects({buildingBox(b).x, buildingBox(b).y - 1, buildingBox(b).w, 1}))
      e.y += 0; // walkers fall on their own next frame
}

void World::rateBreak(const Building& b)
{
  auto& c = mCity;
  const int r = b.rating * (mPlayer.turbo > 0 ? 2 : 1);
  c.rating += r;
  c.ratingFlash = 20;
  addScore(r, cellCenter(b.state == BuildingState::Lying ? bridgeBox(b) : buildingBox(b)));
}

int World::buildingAt(int cx, int cy) const
{
  const int bx = cx / kCellsPerTile, by = cy / kCellsPerTile;
  if (cx < 0 || cy < 0)
    return -1;
  for (std::size_t i = 0; i < mCity.buildings.size(); ++i)
    if (mCity.buildings[i].standing() && mCity.buildings[i].covers(bx, by))
      return int(i);
  return -1;
}

void World::snapLine(PowerLine& l)
{
  auto& c = mCity;
  if (l.snapped)
    return;
  l.snapped = true;
  l.sparks = kLineSparks;
  playSound(Sfx::Zap);
  burst({(float(l.x0 + l.x1) * 0.5f) * kCellSize, float(l.y) * kCellSize}, rgb(255, 255, 200), rgb(120, 200, 255), 24, 2.0f);
  for (auto& s : c.sectors)
    if (s.id == l.sector && !s.dark)
    {
      s.dark = true;
      showMessage("BLACKOUT IN SECTOR " + s.id + "!");
    }
}

bool World::sectorDark(int sector) const
{
  return sector >= 0 && sector < int(mCity.sectors.size()) && mCity.sectors[std::size_t(sector)].dark;
}

void World::layTower(Building& b)
{
  b.state = BuildingState::Lying;
  b.t = 0;
  for (int by = b.y0; by <= b.y1; ++by)
    for (int bx = b.x0; bx <= b.x1; ++bx)
      mMap.setBlock(bx, by, Tile::Empty);
  for (int by = b.by0; by <= b.by1; ++by)
    for (int bx = b.bx0; bx <= b.bx1; ++bx)
      mMap.setBlock(bx, by, Tile::Solid);
}

void World::updateBuildings()
{
  auto& c = mCity;
  auto& p = mPlayer;
  const bool rocco = mCharacter.id == "rocco";
  const bool airborne = p.state == PlayerState::Jumping || p.state == PlayerState::Falling;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  const CellBox pb = p.box();
  const CellBox sides{pb.x - 1, pb.y, pb.w + 2, pb.h};
  const CellBox head{pb.x, pb.y - 1, pb.w, 1};
  const CellBox feet{pb.x, pb.bottom() + 1, pb.w, 1};
  for (std::size_t i = 0; i < c.buildings.size(); ++i)
  {
    Building& b = c.buildings[i];
    const CellBox box = buildingBox(b);
    if (!b.burnNow)
      b.burn = 0;
    b.burnNow = false;
    switch (b.state)
    {
      case BuildingState::Standing:
      {
        if (!alive || mTop.on)
          break;
        // Jumped into (any face but its roof, which is landed on), or
        // touched at all by Rocco.
        const bool face = sides.intersects(box) || head.intersects(box);
        const bool onRoof = feet.intersects(box);
        if ((airborne && face) || (rocco && (face || onRoof)))
        {
          breakBuilding(int(i));
          break;
        }
        // Stood on: cracks spread for 20 frames, then it gives.
        b.cracks = onRoof && p.state == PlayerState::OnGround ? b.cracks + 1 : 0;
        if (b.cracks >= kRoofCrack)
        {
          breakBuilding(int(i));
          break;
        }
        if (b.burn >= kBurnFrames)
          breakBuilding(int(i));
        break;
      }
      case BuildingState::Crumbling:
        if (++b.t >= kCrumbleFrames)
          b.state = BuildingState::Down;
        break;
      case BuildingState::Leaning:
        if (++b.t >= kTowerLean)
        {
          b.state = BuildingState::Falling;
          b.t = 0;
          for (int by = b.y0; by <= b.y1; ++by)
            for (int bx = b.x0; bx <= b.x1; ++bx)
              mMap.setBlock(bx, by, Tile::Empty);
          playSound(Sfx::Whistle);
        }
        break;
      case BuildingState::Falling:
        if (++b.t >= kTowerFall)
        {
          layTower(b);
          const CellBox bridge = bridgeBox(b);
          // It crushes what is under it.
          for (auto& e : mEnemies)
            if (e.alive && !e.hidden && e.kind != EnemyKind::ToyJet && e.box().intersects(bridge))
              killEnemy(e);
          if (alive && p.box().intersects(bridge))
          {
            hurtPlayer(2);
            if (p.state != PlayerState::Dying)
            {
              // Out from under it, onto it.
              p.y = p.prevY = bridge.top() - 1;
              p.state = PlayerState::Falling;
            }
          }
          rateBreak(b);
          burst(cellCenter(bridge), mTheme.rockLight, mTheme.rock, 30, 2.2f);
          playSound(Sfx::Crash);
          mCamera.shake(8, 2.0f);
        }
        break;
      default:
        break;
    }
  }
}

// --- The Atomic Breath ---------------------------------------------------------------------

void World::updateBreath(const Button& fire)
{
  auto& p = mPlayer;
  auto& c = mCity;
  c.on = true;
  if (!fire.pressed || p.ammo <= 0)
  {
    c.breath = 0;
    c.breathAng = 0;
    return;
  }
  ++c.breath;
  if (c.breath == 1)
    playSound(Sfx::BreathBeam);
  // Up or Down while breathing sweeps it, a 15-degree step every 3 frames.
  if (c.breath % kBreathStepEvery == 0)
  {
    if (p.stance == Stance::Up)
      c.breathAng = std::max(-3, c.breathAng - 1);
    else if (p.stance == Stance::Crouched || p.stance == Stance::Down)
      c.breathAng = std::min(3, c.breathAng + 1);
  }
  p.muzzleTicks = 4;
  p.muzzleStance = p.stance;
  breathBeam();
  if (c.breath % kBreathAmmoEvery == 0 && p.turbo == 0 && --p.ammo <= 0)
  {
    p.ammo = 0;
    p.weapon = Weapon::Normal;
    p.proto = -1;
    c.breath = 0;
    showMessage("OUT OF AMMO - BACK TO THE BLASTER");
  }
}

void World::breathBeam()
{
  auto& c = mCity;
  const auto& p = mPlayer;
  const int dir = p.facing;
  const float ox = dir > 0 ? float(p.x + Player::kWidth) : float(p.x) - 0.01f;
  const float oy = float(p.y - (p.stance == Stance::Crouched ? 2 : 3)) + 0.5f;
  const float a = float(c.breathAng) * 0.2618f;
  const float vx = std::cos(a) * float(dir), vy = std::sin(a);
  c.beamX0 = ox;
  c.beamY0 = oy;
  c.beamDir = dir;
  const bool hit = (c.breath - 1) % kBreathDamageEvery == 0;
  int burnt = -1;
  std::vector<int> struck;
  bool lizard = false;
  float x = ox, y = oy;
  for (float d = 0.0f; d < float(kBreathLen); d += 0.5f)
  {
    x = ox + vx * d;
    y = oy + vy * d;
    const int cx = int(std::floor(x)), cy = int(std::floor(y));
    burnt = buildingAt(cx, cy);
    if (burnt >= 0 || mMap.solid(cx, cy))
      break;
    for (auto& l : c.lines)
      if (!l.snapped && cx >= l.x0 && cx <= l.x1 && std::abs(cy - l.y) <= 1)
        snapLine(l);
    for (std::size_t i = 0; i < mEnemies.size(); ++i)
    {
      const Enemy& e = mEnemies[i];
      if (e.alive && !e.hidden && e.box().contains(cx, cy) &&
          std::find(struck.begin(), struck.end(), int(i)) == struck.end())
        struck.push_back(int(i));
    }
    if (lizardHittable() && lizardBox().contains(cx, cy))
    {
      lizard = true;
      break;
    }
  }
  c.beamX1 = x;
  c.beamY1 = y;
  if (burnt >= 0)
  {
    Building& b = c.buildings[std::size_t(burnt)];
    ++b.burn;
    b.burnNow = true;
  }
  if (lizard && hit)
    damageLizard(1);
  for (int i : struck)
  {
    Enemy& e = mEnemies[std::size_t(i)];
    if (e.kind == EnemyKind::ToyJet)
    {
      if (e.tell == 0)
        e.tell = 1; // spins out of its formation
      continue;
    }
    if (hit)
    {
      const bool was = e.alive;
      shotHitsEnemy(e, dir, protoDef(ProtoId::AtomicBreath).damage);
      if (was && !e.alive)
        ++mStats.protoKills;
    }
  }
}

// --- Enemies -------------------------------------------------------------------------------

void World::updateModelTank(Enemy& e, const EnemyDef& def)
{
  auto& p = mPlayer;
  if (mTop.on)
  {
    updateTopTank(e, def);
    return;
  }
  const CellBox b = e.box();
  // Landed on: squashed.
  if (p.state != PlayerState::Dying && p.y > p.prevY && p.prevY < b.top() && p.y >= b.top() - 1 &&
      p.x + Player::kWidth > b.x && p.x < b.x + b.w)
  {
    killEnemy(e);
    p.y = b.top() - 1;
    startLaunch(6);
    playSound(Sfx::Stomp);
    return;
  }
  if (!mMap.onSolidGround(b))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, 1);
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return;
  }
  if (e.tell > 0)
  {
    // The turret lifts and flashes; then a shot up at 45 degrees toward the runner.
    if (--e.tell == 0 && p.state != PlayerState::Dying)
    {
      e.dir = p.x + 1 < b.x + 1 ? -1 : 1;
      spawnProjectile(ShotKind::Enemy, e.dir > 0 ? b.right() + 1 : b.left() - 1, b.top() - 1, e.dir, -1);
      playSound(Sfx::Cannon);
    }
    return;
  }
  if (e.timer % std::max(1, def.stepEvery) != 0)
    return;
  const int aheadX = e.dir > 0 ? b.right() + 1 : b.left() - 1;
  const bool wall = e.dir > 0 ? mMap.touchingRightWall(b) : mMap.touchingLeftWall(b);
  const bool end = e.railX0 >= 0 && (e.dir > 0 ? b.right() + 1 > e.railX1 : b.left() - 1 < e.railX0);
  if (wall || end || !mMap.solidTop(aheadX, b.bottom() + 1))
    e.dir = -e.dir;
  else
    e.x += e.dir;
}

void World::updateToyJet(Enemy& e, const EnemyDef& def)
{
  (void)def;
  // Formations fly in updateCity; a jet the breath touched spins down.
  if (e.tell == 0 || e.hidden)
    return;
  ++e.tell;
  e.x += e.dir;
  if (e.tell % 2 == 0)
    ++e.y;
  if (e.tell >= 24 || mMap.overlapsSolid(e.box()))
  {
    burst(cellCenter(e.box()), rgb(255, 230, 160), rgb(160, 160, 170), 12, 1.4f);
    killEnemy(e);
  }
}

void World::updateMaserTruck(Enemy& e, const EnemyDef& def)
{
  auto& p = mPlayer;
  const CellBox b = e.box();
  if (e.dive > 0)
    --e.dive; // the beam shows
  if (e.cool > 0)
    --e.cool;
  if (!mMap.onSolidGround(b))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, 1);
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return;
  }
  const bool dark = sectorDark(e.variant);
  const int row = b.top() + 1; // the dish
  if (e.tell > 0)
  {
    if (dark)
    {
      e.tell = 0; // the ring fizzles out
      return;
    }
    if (--e.tell == 0)
    {
      // The beam: along the dish's row up to 20 blocks, stopped by anything solid.
      const int x0 = e.dir > 0 ? b.right() + 1 : b.left() - 1;
      int x = x0;
      for (int n = 0; n < kMaserBeam && !mMap.solid(x, row); ++n)
        x += e.dir;
      e.aimX = x;
      e.aimY = row;
      e.dive = kMaserShow;
      e.cool = def.cooldown;
      playSound(Sfx::EyeBeam);
      const CellBox beam{std::min(x0, x), row, std::abs(x - x0) + 1, 1};
      if (p.state != PlayerState::Dying && beam.intersects(p.hitBox()))
        hurtPlayer(2);
    }
    return;
  }
  // The runner in line within the beam's reach: turn, and the ring grows.
  const int dx = (p.x + 1) - (b.x + b.w / 2);
  const bool inLine = p.y >= row && p.y - 4 <= row && std::abs(dx) <= kMaserBeam;
  if (!dark && e.cool == 0 && inLine && p.state != PlayerState::Dying && isOnScreen(b, 0))
  {
    e.dir = dx < 0 ? -1 : 1;
    e.tell = def.tell;
    playSound(Sfx::MaserHum);
    return;
  }
  if (e.timer % std::max(1, def.stepEvery) != 0)
    return;
  const int aheadX = e.dir > 0 ? b.right() + 1 : b.left() - 1;
  const bool wall = e.dir > 0 ? mMap.touchingRightWall(b) : mMap.touchingLeftWall(b);
  if (wall || !mMap.solidTop(aheadX, b.bottom() + 1))
    e.dir = -e.dir;
  else
    e.x += e.dir;
}

void World::startJetRun(JetFormation& f)
{
  f.state = 2;
  f.t = 0;
  int i = 0;
  for (int j : f.jets)
  {
    Enemy& e = mEnemies[std::size_t(j)];
    if (!e.alive)
      continue;
    e.hidden = false;
    e.active = true;
    e.dive = 0;
    e.tell = 0;
    e.dir = f.dir;
    e.x = f.dir > 0 ? mCamera.x() - e.w - 2 - i * 5 : mCamera.x() + int(kViewCellsW) + 2 + i * 5;
    e.y = f.row - (i % 2) * 2;
    e.prevX = e.x;
    e.prevY = e.y;
    e.drawSnap = true;
    ++i;
  }
  playSound(Sfx::Whine);
}

void World::updateJets()
{
  auto& c = mCity;
  const auto& p = mPlayer;
  for (auto& f : c.formations)
    switch (f.state)
    {
      case 0:
        if (p.x >= f.trigger && p.state != PlayerState::Dying)
        {
          f.state = 1;
          f.t = 0;
          f.dir = 1;
          playSound(Sfx::Whine);
        }
        break;
      case 1: // small in the backdrop, growing
        if (++f.t >= kJetIntro)
          startJetRun(f);
        break;
      case 2:
      {
        bool flying = false;
        for (int j : f.jets)
        {
          Enemy& e = mEnemies[std::size_t(j)];
          if (!e.alive || e.hidden || e.tell > 0)
            continue;
          e.x += 2 * e.dir;
          const int cx = e.x + e.w / 2;
          if (e.dive == 0 && std::abs(cx - (p.x + 1)) <= 1 && e.y < p.y - 4 && p.state != PlayerState::Dying)
          {
            spawnProjectile(ShotKind::Enemy, cx, e.y + 1, 0, 1);
            playSound(Sfx::EnemyShot);
            e.dive = 1;
          }
          const bool gone = e.dir > 0 ? e.x > mCamera.x() + int(kViewCellsW) + 2 : e.x + e.w < mCamera.x() - 2;
          if (gone)
            e.hidden = true;
          else
            flying = true;
        }
        if (!flying)
        {
          ++f.run;
          f.t = 0;
          f.state = f.run >= 2 ? 4 : 3;
          if (f.state == 4)
            for (int j : f.jets)
              if (mEnemies[std::size_t(j)].tell == 0)
              {
                mEnemies[std::size_t(j)].hidden = true;
                mEnemies[std::size_t(j)].alive = false; // they fly home
              }
        }
        break;
      }
      case 3:
        if (++f.t >= kJetGap)
        {
          f.dir = -f.dir;
          startJetRun(f);
        }
        break;
      default:
        break;
    }
  // The squads: one tank at a time lifts its turret.
  for (auto& s : c.squads)
  {
    if (s.cool > 0)
    {
      --s.cool;
      continue;
    }
    const int n = int(s.tanks.size());
    for (int k = 0; k < n; ++k)
    {
      const int i = (s.next + k) % n;
      Enemy& e = mEnemies[std::size_t(s.tanks[std::size_t(i)])];
      if (!e.alive || !e.active || e.tell > 0 || !isOnScreen(e.box(), 0) || std::abs(e.x - p.x) > 24 ||
          p.state == PlayerState::Dying)
        continue;
      e.tell = enemyDef(e.def).tell;
      s.next = i + 1;
      s.cool = kSquadCool;
      break;
    }
  }
}

// --- Gorgazilla ----------------------------------------------------------------------------

CellBox World::lizardBox() const
{
  const auto& z = mCity.lizard;
  return boxAt(int(std::lround(z.x)), z.floor, Lizard::kW, Lizard::kH);
}

bool World::lizardHittable() const
{
  const auto& z = mCity.lizard;
  return z.on && (z.state == LizardState::Idle || z.state == LizardState::Roar || z.state == LizardState::Swipe ||
                   z.state == LizardState::Dive);
}

bool World::lizardFight() const
{
  const auto& z = mCity.lizard;
  return z.on && z.doorShut && z.state != LizardState::Waiting && z.state < LizardState::Sit;
}

int World::lizardHp() const
{
  const auto& z = mCity.lizard;
  if (!z.on || z.state >= LizardState::Sit)
    return 0;
  return (3 - z.phase) * Lizard::kHp + std::max(0, z.hp);
}

int World::lizardPier() const
{
  const auto& z = mCity.lizard;
  return z.x + Lizard::kW * 0.5f < float(z.piers[0][1] + Lizard::kW + z.piers[1][0]) * 0.5f ? 0 : 1;
}

void World::damageLizard(int amount)
{
  auto& z = mCity.lizard;
  if (!lizardHittable())
    return;
  z.hp -= amount;
  z.flash = 4;
  if (z.hp > 0)
    return;
  const Vec2 at = cellCenter(lizardBox());
  if (z.phase < 3)
  {
    // He staggers and drops a health box on the pier he stands on.
    ++z.phase;
    z.hp = Lizard::kHp;
    z.state = LizardState::Stagger;
    z.t = 0;
    z.attacks = 0;
    playSound(Sfx::Roar);
    const int k = lizardPier();
    Item it;
    it.kind = ItemKind::Health;
    it.x = it.prevX = z.dropX[k] * kCellsPerTile;
    it.y = it.prevY = z.dropY * kCellsPerTile + 1 - 10;
    it.pickupDelay = 4;
    mItems.push_back(it);
    showMessage(z.phase == 2 ? "GORGAZILLA STAGGERS!" : "GORGAZILLA IS FURIOUS!");
    burst(at, rgb(120, 230, 120), rgb(255, 255, 255), 20, 1.8f);
    return;
  }
  z.hp = 0;
  z.state = LizardState::Sit;
  z.t = 0;
  z.beam = 0;
  addScore(kLizardScore, at);
  showMessage("CUT! THAT'S A WRAP");
  playSound(Sfx::Applause);
}

void World::shutArenaDoor(bool shut)
{
  auto& z = mCity.lizard;
  z.doorShut = shut;
  for (int by = z.doorY0; by <= z.doorY1; ++by)
    mMap.setBlock(z.doorX, by, shut ? Tile::Solid : Tile::Empty);
}

void World::moveLizard(float dx)
{
  auto& z = mCity.lizard;
  const int k = lizardPier();
  const float nx = std::clamp(z.x + dx, float(z.piers[k][0]), float(z.piers[k][1]));
  if (!mMap.overlapsSolid(boxAt(int(std::lround(nx)), z.floor, Lizard::kW, Lizard::kH)))
    z.x = nx;
}

void World::updateLizard()
{
  auto& z = mCity.lizard;
  auto& p = mPlayer;
  if (!z.on)
    return;
  z.prevX = z.x;
  if (z.flash > 0)
    --z.flash;
  if (z.beam > 0)
    --z.beam;
  if (z.surfaced > 0)
    --z.surfaced;
  const bool vulnerable = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  const float pcx = float(p.x) + 1.5f;
  const float zcx = z.x + Lizard::kW * 0.5f;
  if (z.state == LizardState::Waiting)
  {
    if (vulnerable && p.x > (z.doorX + 1) * kCellsPerTile && p.x < (z.x1 + 1) * kCellsPerTile &&
        p.y >= z.y0 * kCellsPerTile && p.y <= (z.y1 + 1) * kCellsPerTile)
    {
      shutArenaDoor(true);
      z.state = LizardState::Idle;
      z.cool = 30;
      z.dir = pcx < zcx ? -1 : 1;
      showMessage("GORGAZILLA!");
      playSound(Sfx::Screech);
      mCamera.shake(10, 2.0f);
    }
    return;
  }
  ++z.t;
  switch (z.state)
  {
    case LizardState::Idle:
    {
      z.dir = pcx < zcx ? -1 : 1;
      if (std::abs(pcx - zcx) > 10.0f)
        moveLizard(0.25f * float(z.dir));
      if (--z.cool > 0 || !vulnerable)
        break;
      ++z.attacks;
      z.t = 0;
      const bool near = std::abs(pcx - zcx) <= float(kSwipeReach + Lizard::kW) && std::abs(p.y - z.floor) <= 4;
      if (z.phase >= 2 && z.attacks % 3 == 0)
      {
        z.state = LizardState::Dive;
        playSound(Sfx::Splash);
      }
      else if (near)
      {
        z.state = LizardState::Swipe;
        playSound(Sfx::Rattle);
      }
      else
      {
        z.state = LizardState::Roar;
        z.beamRow = -1;
        playSound(Sfx::Roar);
      }
      break;
    }
    case LizardState::Roar:
      if (z.t == kRoarLen - kRoarLock)
        playSound(Sfx::MaserHum);
      if (z.t == kRoarLock)
        z.beamRow = p.y - 1; // the dish locks onto the runner's row
      if (z.t >= kRoarLen)
      {
        z.beam = kMaserShow;
        z.beamHit = false;
        playSound(Sfx::EyeBeam);
        const int x0 = z.x0 * kCellsPerTile, x1 = z.dishX;
        if (vulnerable && z.beamRow >= 0 && CellBox{x0, z.beamRow, x1 - x0 + 1, 1}.intersects(p.hitBox()))
        {
          hurtPlayer(2);
          z.beamHit = true;
        }
        z.state = LizardState::Idle;
        z.cool = 45;
      }
      break;
    case LizardState::Swipe:
      if (z.t == kSwipeTell)
      {
        const CellBox sweep{int(z.x) - kSwipeReach, z.floor - 1, Lizard::kW + 2 * kSwipeReach, 2};
        playSound(Sfx::Swoosh);
        if (vulnerable && sweep.intersects(p.hitBox()))
          hurtPlayer(1);
        if (z.phase == 3 && !z.turboDropped && z.turboX >= 0)
        {
          // The tail knocks a Turbo box loose from the crate.
          z.turboDropped = true;
          Item it;
          it.kind = ItemKind::Turbo;
          it.x = it.prevX = z.turboX * kCellsPerTile;
          it.y = it.prevY = z.turboY * kCellsPerTile + 1 - 6;
          it.pickupDelay = 4;
          mItems.push_back(it);
          showMessage("A TURBO BOX FALLS OFF THE CRATE");
        }
      }
      if (z.t >= kSwipeTell + kSwipeShow)
      {
        z.state = LizardState::Idle;
        z.cool = z.phase == 3 ? 30 : 45;
      }
      break;
    case LizardState::Dive:
    {
      // Wades into the bay.
      const float bay = float(z.piers[0][1] + Lizard::kW + z.piers[1][0]) * 0.5f - Lizard::kW * 0.5f;
      z.x += std::clamp(bay - z.x, -0.5f, 0.5f);
      if (std::abs(bay - z.x) < 0.5f || z.t >= kDiveWade)
      {
        z.x = bay;
        z.state = LizardState::Under;
        z.t = 0;
        playSound(Sfx::Splash);
        burst({(z.x + 2.0f) * kCellSize, float(z.floor) * kCellSize}, rgb(150, 210, 255), rgb(255, 255, 255), 18, 1.8f);
      }
      break;
    }
    case LizardState::Under:
      if (z.t == kUnder - kRipple)
      {
        // He surfaces on the runner's pier, a little off from the runner.
        const int k = pcx < float(z.piers[0][1] + Lizard::kW + z.piers[1][0]) * 0.5f ? 0 : 1;
        const int a = z.piers[k][0], b = z.piers[k][1];
        int x = int(pcx) + (pcx - float(a) > float(b) - pcx ? -10 : 6);
        x = std::clamp(x, a, b);
        while (x > a && mMap.overlapsSolid(boxAt(x, z.floor, Lizard::kW, Lizard::kH)))
          --x;
        z.splashX = x;
        playSound(Sfx::Bubble);
      }
      if (z.t >= kUnder)
      {
        z.x = float(z.splashX);
        z.surfaced = 10;
        z.state = LizardState::Idle;
        z.cool = 45;
        playSound(Sfx::Splash);
        const CellBox splash{z.splashX - kSplash, z.floor - Lizard::kH + 1, Lizard::kW + 2 * kSplash, Lizard::kH};
        burst(cellCenter(splash), rgb(150, 210, 255), rgb(255, 255, 255), 30, 2.2f);
        if (vulnerable && splash.intersects(p.hitBox()))
          hurtPlayer(1);
      }
      break;
    case LizardState::Stagger:
      if (z.t >= kStagger)
      {
        z.state = LizardState::Idle;
        z.cool = 30;
      }
      break;
    case LizardState::Sit:
      if (z.t >= kSit)
      {
        z.state = LizardState::Leave;
        z.t = 0;
        z.manX = z.x;
      }
      break;
    case LizardState::Leave:
      z.manX -= 0.5f; // a tired man walks off set to the west
      if (z.t >= kLeave)
      {
        z.state = LizardState::Done;
        shutArenaDoor(false);
      }
      break;
    default:
      break;
  }
}

// --- Update --------------------------------------------------------------------------------

void World::updateCity(const PlayerInput& input)
{
  (void)input;
  auto& c = mCity;
  auto& p = mPlayer;
  if (c.ratingFlash > 0)
    --c.ratingFlash;
  if (c.wave > 0)
    --c.wave;
  if (c.photoSwing > 0)
    --c.photoSwing;
  updateBuildings();
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  // Power lines: touched, they snap.
  for (auto& l : c.lines)
  {
    if (l.sparks > 0)
      --l.sparks;
    if (alive && !l.snapped && CellBox{l.x0, l.y, l.x1 - l.x0 + 1, 1}.intersects(p.hitBox()))
      snapLine(l);
  }
  // The diner, jumped clear over.
  if (c.diner >= 0 && !c.dinerSaid && alive && p.x >= c.dinerX)
  {
    c.dinerSaid = true;
    if (c.buildings[std::size_t(c.diner)].standing())
    {
      c.wave = 90;
      addScore(10000, cellCenter(buildingBox(c.buildings[std::size_t(c.diner)])));
      showMessage("SECRET: THE DINER STILL STANDS! +10000");
      playSound(Sfx::Cheer);
    }
  }
  // The full heal inside the arena door.
  if (c.fullhealX >= 0 && !c.fullhealUsed && alive && p.hp < p.maxHp &&
      blockRect(c.fullhealX, c.fullhealY - 1, c.fullhealX, c.fullhealY).intersects(p.box()))
  {
    c.fullhealUsed = true;
    p.hp = p.maxHp;
    playSound(Sfx::Health);
    showMessage("FULL HEAL");
    burst(cellCenter(blockRect(c.fullhealX, c.fullhealY - 1, c.fullhealX, c.fullhealY)), rgb(255, 120, 140), rgb(255, 255, 255), 20, 1.6f);
  }
  // Water: a heart, and back on dry land.
  for (const auto& w : c.ponds)
    if (alive && w.intersects(p.box()) && p.box().bottom() > w.top())
    {
      burst(cellCenter(p.box()), rgb(150, 210, 255), rgb(255, 255, 255), 16, 1.6f);
      playSound(Sfx::Splash);
      hurtPlayer(1);
      if (p.state != PlayerState::Dying)
      {
        p.x = p.prevX = mSafeX;
        p.y = p.prevY = mSafeY;
        p.state = PlayerState::OnGround;
        setVisual(PlayerVisual::Standing);
      }
      break;
    }
  updateJets();
  updateLizard();
}

bool World::shotAtCity(Projectile& pr, const CellBox& b)
{
  auto& c = mCity;
  if (lizardHittable() && lizardBox().intersects(b))
  {
    damageLizard(pr.damage);
    burst(cellCenter(b), rgb(255, 255, 255), rgb(120, 230, 120), 5, 1.2f);
    return true;
  }
  if (c.photoX >= 0 && CellBox{c.photoX, c.photoY - 2, 2, 3}.intersects(b))
  {
    c.photoSwing = 40;
    playSound(Sfx::PosterSpin);
    return true;
  }
  return false;
}

void World::resetCity()
{
  auto& c = mCity;
  auto& z = c.lizard;
  c.breath = 0;
  c.breathAng = 0;
  for (auto& b : c.buildings)
  {
    b.cracks = 0;
    b.burn = 0;
    // A tower caught mid-fall lands.
    if (b.state == BuildingState::Leaning || b.state == BuildingState::Falling)
      layTower(b);
    if (b.state == BuildingState::Crumbling)
      b.state = BuildingState::Down;
  }
  for (auto& f : c.formations)
    if (f.state >= 1 && f.state <= 3)
    {
      f.state = 0;
      f.run = 0;
      for (int j : f.jets)
      {
        Enemy& e = mEnemies[std::size_t(j)];
        e.hidden = true;
        e.tell = 0;
      }
    }
  for (int s = 0; s < int(c.squads.size()); ++s)
    c.squads[std::size_t(s)].cool = 30;
  if (z.on && z.state < LizardState::Sit)
  {
    z.state = LizardState::Waiting;
    z.phase = 1;
    z.hp = Lizard::kHp;
    z.x = z.prevX = float(z.startX);
    z.t = 0;
    z.cool = 30;
    z.attacks = 0;
    z.beam = 0;
    z.beamRow = -1;
    z.flash = 0;
    if (z.doorShut)
      shutArenaDoor(false);
  }
}

// --- Saves ---------------------------------------------------------------------------------

bool World::cityCanSave() const
{
  const auto& c = mCity;
  for (const auto& b : c.buildings)
    if (b.state == BuildingState::Leaning || b.state == BuildingState::Falling)
      return false;
  for (const auto& f : c.formations)
    if (f.state >= 1 && f.state <= 3)
      return false;
  return !c.lizard.on || c.lizard.state == LizardState::Waiting || c.lizard.state == LizardState::Done;
}

std::vector<int> World::citySave() const
{
  // [rating, diner said, full heal used, lizard done, then per building its
  //  state, per line snapped, per formation its state]
  const auto& c = mCity;
  std::vector<int> s{c.rating, c.dinerSaid ? 1 : 0, c.fullhealUsed ? 1 : 0,
    c.lizard.state == LizardState::Done ? 1 : 0};
  for (const auto& b : c.buildings)
    s.push_back(int(b.state == BuildingState::Crumbling ? BuildingState::Down : b.state)); // as it loads
  for (const auto& l : c.lines)
    s.push_back(l.snapped ? 1 : 0);
  for (const auto& f : c.formations)
    s.push_back(f.state);
  return s;
}

bool World::validCitySave(const std::vector<int>& s) const
{
  const auto& c = mCity;
  return s.size() == 4 + c.buildings.size() + c.lines.size() + c.formations.size();
}

void World::loadCity(const std::vector<int>& s)
{
  auto& c = mCity;
  std::size_t i = 0;
  c.rating = s[i++];
  c.dinerSaid = s[i++] != 0;
  c.fullhealUsed = s[i++] != 0;
  if (s[i++] != 0 && c.lizard.on)
  {
    c.lizard.state = LizardState::Done;
    c.lizard.hp = 0;
    c.lizard.phase = 3;
  }
  for (auto& b : c.buildings)
  {
    const auto st = BuildingState(std::clamp(s[i++], 0, int(BuildingState::Lying)));
    if (st == BuildingState::Standing)
      continue;
    if (b.kind == BuildingKind::Tower)
    {
      layTower(b);
      continue;
    }
    b.state = BuildingState::Down;
    for (int by = b.y0; by <= b.y1; ++by)
      for (int bx = b.x0; bx <= b.x1; ++bx)
        mMap.setBlock(bx, by, Tile::Empty);
    if (b.ux0 >= 0)
      for (int by = b.uy0; by <= b.uy1; ++by)
        for (int bx = b.ux0; bx <= b.ux1; ++bx)
          mMap.setBlock(bx, by, Tile::Empty);
  }
  for (auto& l : c.lines)
  {
    l.snapped = s[i++] != 0;
    l.sparks = 0;
    if (l.snapped)
      for (auto& sc : c.sectors)
        if (sc.id == l.sector)
          sc.dark = true;
  }
  for (auto& f : c.formations)
  {
    f.state = s[i++] == 4 ? 4 : 0;
    if (f.state == 4)
      for (int j : f.jets)
      {
        mEnemies[std::size_t(j)].hidden = true;
        mEnemies[std::size_t(j)].alive = false;
      }
  }
}

} // namespace gr
