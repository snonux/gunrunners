// Vehicles: a tank, a helicopter, a hoverbike, a submarine, a space ship
// and a mech walker. A vehicle stands parked in its level (`@ vehicle
// kind=...`) until a runner climbs in with USE (or up); then the runner's
// controls drive it, its armour takes every hit and its own guns fire. USE
// (or down + jump) climbs back out. A destroyed vehicle throws the runner
// clear and turns up again at its home a few seconds later; driving past a
// checkpoint moves its home there.

#include "game/world.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace gr
{

namespace
{

constexpr int kVehicleMercy = 20;  // frames without damage after a hit
constexpr int kWreckFrames = 150;  // a destroyed vehicle is back home after this
constexpr int kFuelLow = 150;

const std::array<VehicleDef, std::size_t(VehicleKind::Count)> kDefs{{
  {"tank", "TANK", 8, 5, 12, 0, rgb(120, 150, 80)},
  {"helicopter", "HELICOPTER", 10, 5, 8, 900, rgb(240, 180, 40)},
  {"hoverbike", "HOVERBIKE", 7, 3, 6, 0, rgb(255, 60, 150)},
  {"submarine", "SUBMARINE", 9, 4, 10, 0, rgb(250, 200, 40)},
  {"spaceship", "SPACE SHIP", 8, 4, 8, 0, rgb(150, 210, 255)},
  {"mech", "MECH WALKER", 6, 8, 14, 0, rgb(220, 100, 50)},
}};

// Jump arcs: cells risen per frame.
constexpr std::array<int, 7> kBikeArc{2, 2, 2, 1, 1, 1, 0};       // 9 cells
constexpr std::array<int, 9> kMechArc{3, 3, 2, 2, 2, 1, 1, 1, 0}; // 15 cells with jump held

// Speeds in sixteenths of a cell a frame.
constexpr int kHeliSpeed = 24, kHeliAccel = 4;
constexpr int kBikeSpeed = 32, kBikeAccel = 4;
constexpr int kShipSpeed = 40, kShipThrust = 3;

int approach(int v, int target, int step)
{
  if (v < target)
    return std::min(target, v + step);
  return std::max(target, v - step);
}

const char* vehicleTip(VehicleKind k)
{
  switch (k)
  {
    case VehicleKind::Tank: return "UP AIMS THE CANNON";
    case VehicleKind::Heli: return "DOWN + FIRE DROPS A BOMB - WATCH THE FUEL";
    case VehicleKind::Bike: return "JUMP CLEARS GAPS AT SPEED";
    case VehicleKind::Sub: return "FIRE LAUNCHES TORPEDOES";
    case VehicleKind::Ship: return "NO GRAVITY - IT KEEPS DRIFTING";
    case VehicleKind::Mech: return "HOLD JUMP FOR THE JETS - LAND HARD TO STOMP";
    default: return "";
  }
}

} // namespace

const VehicleDef& vehicleDef(VehicleKind k) { return kDefs[std::size_t(std::clamp(int(k), 0, int(VehicleKind::Count) - 1))]; }

VehicleKind vehicleKindForKey(const std::string& key, bool* ok)
{
  struct Alias
  {
    const char* key;
    VehicleKind kind;
  };
  static const Alias kAliases[] = {
    {"tank", VehicleKind::Tank},       {"heli", VehicleKind::Heli},        {"helicopter", VehicleKind::Heli},
    {"chopper", VehicleKind::Heli},    {"bike", VehicleKind::Bike},        {"hoverbike", VehicleKind::Bike},
    {"sub", VehicleKind::Sub},         {"submarine", VehicleKind::Sub},    {"ship", VehicleKind::Ship},
    {"spaceship", VehicleKind::Ship},  {"mech", VehicleKind::Mech},        {"walker", VehicleKind::Mech},
  };
  for (const auto& a : kAliases)
    if (key == a.key)
    {
      if (ok)
        *ok = true;
      return a.kind;
    }
  if (ok)
    *ok = false;
  return VehicleKind::Tank;
}

// --- Setup ---------------------------------------------------------------------

bool World::setupVehicleEntity(const EntityDef& e)
{
  bool ok = false;
  VehicleKind kind = VehicleKind::Tank;
  if (e.kind == "vehicle")
    kind = vehicleKindForKey(e.str("kind", "tank"), &ok);
  else
    kind = vehicleKindForKey(e.kind, &ok);
  if (!ok)
  {
    if (e.kind == "vehicle")
      std::fprintf(stderr, "level line %d: unknown vehicle kind '%s'\n", e.line, e.str("kind").c_str());
    return e.kind == "vehicle";
  }
  if (!e.hasPos)
    return true;
  const VehicleDef& d = vehicleDef(kind);
  Vehicle v;
  v.kind = kind;
  v.id = e.id;
  v.w = d.w;
  v.h = d.h;
  // Like everything else, it stands on the bottom row of its block.
  v.x = v.prevX = v.homeX = e.x * kCellsPerTile;
  v.y = v.prevY = v.homeY = e.y * kCellsPerTile + 1;
  v.facing = v.homeFacing = e.str("dir", e.str("facing", "r")) == "l" ? -1 : 1;
  v.hp = e.num("hp", d.hp);
  v.fuel = d.fuel;
  v.bot = e.num("bot", 0) != 0;
  v.pilot = e.num("pilot", 0) != 0;
  const auto drop = e.list("drop");
  if (drop.size() >= 2)
  {
    v.dropX = drop[0] * kCellsPerTile;
    v.dropY = drop[1] * kCellsPerTile + 1;
  }
  mVehicles.push_back(v);
  return true;
}

// --- Queries -------------------------------------------------------------------

bool World::vehicleFits(VehicleKind k, int x, int y) const
{
  const VehicleDef& d = vehicleDef(k);
  const CellBox b = boxAt(x, y, d.w, d.h);
  if (b.x < 0 || b.right() >= mMap.width() || b.y < 0 || mMap.overlapsSolid(b))
    return false;
  if (mSpace.starfall && b.bottom() >= mMap.height())
    return false; // open space: the map's floor is as much a wall as its top
  if (k == VehicleKind::Sub)
  {
    // In the water, with no more than the conning tower above the surface.
    const int cx = x + d.w / 2;
    const int surface = seaSurface(cx);
    return surface >= 0 && inSea(cx, y) && inSea(x + 1, y) && inSea(x + d.w - 2, y) && b.y >= surface - 1;
  }
  return true;
}

int World::vehicleInReach() const
{
  const auto& p = mPlayer;
  if (p.vehicle >= 0 || p.state == PlayerState::Dying || p.state == PlayerState::Teleporting)
    return -1;
  CellBox reach = p.box();
  reach.x -= 1;
  reach.y -= 1;
  reach.w += 2;
  reach.h += 2;
  int best = -1, bestD = 1 << 30;
  for (std::size_t i = 0; i < mVehicles.size(); ++i)
  {
    const Vehicle& v = mVehicles[i];
    if (v.occupied || v.wreck > 0 || !v.box().intersects(reach))
      continue;
    const int d = std::abs((v.x * 2 + v.w) - (p.x * 2 + Player::kWidth)) + std::abs((v.y * 2 - v.h) - (p.y * 2 - 5));
    if (d < bestD)
    {
      bestD = d;
      best = int(i);
    }
  }
  return best;
}

namespace
{

// Where the runner climbs out: beside the cockpit, else on the roof.
bool exitSpot(const CollisionMap& map, const Vehicle& v, int& x, int& y)
{
  const int cx = v.x + v.w / 2 - 1;
  const int spots[][2] = {{cx, v.y}, {cx, v.y - v.h}, {cx, v.y - 2}, {v.x - Player::kWidth, v.y},
    {v.x + v.w, v.y}};
  // A submarine lets you out through the hatch on top.
  const int first = v.kind == VehicleKind::Sub ? 1 : 0;
  for (int k = 0; k < 5; ++k)
  {
    const int i = (k + first) % 5;
    const CellBox b = boxAt(spots[i][0], spots[i][1], Player::kWidth, 5);
    if (b.x < 0 || b.right() >= map.width() || b.y < 0 || map.overlapsSolid(b))
      continue;
    x = spots[i][0];
    y = spots[i][1];
    return true;
  }
  return false;
}

} // namespace

bool World::canLeaveVehicle() const
{
  const Vehicle* v = riding();
  int x = 0, y = 0;
  if (v && v->pilot && !mMap.onSolidGround(v->box()))
    return false; // nothing to climb out onto in space
  return v && exitSpot(mMap, *v, x, y);
}

// --- Getting in and out ----------------------------------------------------------

bool World::tryBoard(const PlayerInput& input)
{
  auto& p = mPlayer;
  if (mVehicles.empty() || p.vehicle >= 0 || p.cart >= 0 || p.vine >= 0)
    return false;
  const bool upPress = mUpBoards && input.up && !mUpHeld &&
    (p.state == PlayerState::OnGround || p.state == PlayerState::Swim);
  if (!input.use.triggered && !upPress)
    return false;
  const int i = vehicleInReach();
  if (i < 0)
    return false;
  boardVehicle(i);
  return true;
}

void World::boardVehicle(int index)
{
  auto& p = mPlayer;
  Vehicle& v = mVehicles[std::size_t(index)];
  v.occupied = true;
  v.ax = v.ay = 0;
  p.vehicle = index;
  p.facing = v.facing;
  p.state = PlayerState::OnGround;
  p.frames = 0;
  p.fling = 0;
  p.vineArc = false;
  p.somersault = -1;
  p.jumpRequested = false;
  p.stance = Stance::Regular;
  mLaunch = mLaunchBump = 0;
  setVisual(PlayerVisual::Standing);
  placeDriver();
  playSound(Sfx::EngineOn);
  const VehicleDef& d = vehicleDef(v.kind);
  showMessage(std::string(d.name) + " - " + vehicleTip(v.kind) + " - " + mUseLabel + " GETS OUT");
  burst(cellCenter(v.box()), d.color, rgb(255, 255, 255), 12, 1.4f);
}

void World::leaveVehicle(bool thrown)
{
  auto& p = mPlayer;
  if (p.vehicle < 0)
    return;
  Vehicle& v = mVehicles[std::size_t(p.vehicle)];
  int x = 0, y = 0;
  if (!thrown && v.pilot && !mMap.onSolidGround(v.box()))
  {
    showMessage("NO AIR OUT THERE - LAND FIRST");
    return;
  }
  if (!exitSpot(mMap, v, x, y))
  {
    if (!thrown)
    {
      showMessage("NO ROOM TO CLIMB OUT HERE");
      return;
    }
    x = v.x + v.w / 2 - 1;
    y = v.y;
  }
  v.occupied = false;
  v.ax = v.ay = 0;
  if (v.kind == VehicleKind::Ship || v.kind == VehicleKind::Bike)
    v.vx = v.vy = 0;
  p.vehicle = -1;
  p.x = x;
  p.y = y;
  p.facing = v.facing;
  p.jumpRequested = false;
  if (thrown)
  {
    jump();
    p.mercy = 30;
  }
  else
  {
    p.state = PlayerState::Falling;
    p.frames = 0;
    setVisual(PlayerVisual::Falling);
  }
  playSound(Sfx::EngineOff);
}

void World::placeDriver()
{
  auto& p = mPlayer;
  if (p.vehicle < 0)
    return;
  const Vehicle& v = mVehicles[std::size_t(p.vehicle)];
  p.x = v.x + v.w / 2 - 1;
  p.y = v.y;
  p.facing = v.facing;
  p.state = PlayerState::OnGround;
  p.frames = 0;
  setVisual(PlayerVisual::Standing);
}

void World::resetVehicles()
{
  for (auto& v : mVehicles)
  {
    v.x = v.prevX = v.homeX;
    v.y = v.prevY = v.homeY;
    v.facing = v.homeFacing;
    v.hp = vehicleDef(v.kind).hp;
    v.fuel = vehicleDef(v.kind).fuel;
    v.vx = v.vy = v.ax = v.ay = 0;
    v.air = -1;
    v.fallen = 0;
    v.wreck = 0;
    v.mercy = 0;
    v.occupied = false;
  }
  mPlayer.vehicle = -1;
}

// --- Damage --------------------------------------------------------------------

void World::damageVehicle(Vehicle& v, int amount)
{
  if (v.wreck > 0 || v.mercy > 0 || mGod || mPlayer.turbo > 0 || amount <= 0)
    return;
  v.hp -= amount;
  v.mercy = kVehicleMercy;
  v.flash = 3;
  playSound(Sfx::Hit);
  burst(cellCenter(v.box()), rgb(255, 200, 120), rgb(255, 255, 255), 8, 1.4f);
  if (!mSimulation && std::getenv("GR_PLANNER_DEBUG"))
    std::fprintf(stderr, "vehicle hit at %d,%d frame %d hp %d\n", v.x, v.y, mStats.frames, v.hp);
  if (v.hp <= 0)
    wreckVehicle(v);
  else if (v.hp <= 2)
    showMessage(std::string(vehicleDef(v.kind).name) + " ARMOUR CRITICAL");
}

void World::wreckVehicle(Vehicle& v)
{
  v.hp = 0;
  const CellBox b = v.box();
  if (v.occupied && v.pilot)
  {
    // Level 43: no climbing out in space. The runner goes up with it.
    killPlayer();
    mPlayer.deathPhase = 2;
    mPlayer.frames = 0;
    mPlayer.hidden = true;
  }
  else if (v.occupied)
    leaveVehicle(true);
  v.occupied = false;
  v.wreck = kWreckFrames;
  explodeAt(b.x + b.w / 2, b.y + b.h / 2, std::max(b.w, b.h) / 2 + 1, 3);
  burst(cellCenter(b), rgb(90, 90, 100), rgb(40, 40, 50), 24, 2.0f, false);
  showMessage(std::string(vehicleDef(v.kind).name) + " DESTROYED - A NEW ONE IS ON ITS WAY");
}

bool World::shotAtVehicle(const CellBox& b)
{
  const Vehicle* r = riding();
  if (!r || !r->box().intersects(b))
    return false;
  Vehicle& v = mVehicles[std::size_t(mPlayer.vehicle)];
  burst(cellCenter(b), rgb(255, 255, 255), rgb(255, 200, 60), 6, 1.4f);
  damageVehicle(v, 1);
  return true;
}

// --- Driving -------------------------------------------------------------------

Projectile& World::vehicleShot(ShotKind kind, float x, float y, float vx, float vy, int speed, int damage)
{
  Projectile pr;
  pr.kind = kind;
  pr.precise = true;
  pr.fx = x;
  pr.fy = y;
  pr.vx = vx;
  pr.vy = vy;
  pr.x = pr.prevX = int(std::floor(x));
  pr.y = pr.prevY = int(std::floor(y));
  pr.dx = vx < 0.0f ? -1 : (vx > 0.0f ? 1 : 0);
  pr.dy = vy < -0.3f ? -1 : (vy > 0.3f ? 1 : 0);
  pr.speed = speed;
  pr.damage = damage;
  pr.w = 2;
  pr.h = 1;
  pr.vehicle = true;
  pr.pierce = kind == ShotKind::Laser;
  mProjectiles.push_back(pr);
  return mProjectiles.back();
}

bool World::vehicleFall(Vehicle& v, int cells)
{
  if (mMap.onSolidGround(v.box()))
    return false;
  mMap.moveVertically(v.x, v.y, v.w, v.h, cells);
  return true;
}

bool World::vehicleStep(Vehicle& v, int dx, int dy)
{
  if (dx == 0 && dy == 0)
    return false;
  if (vehicleFits(v.kind, v.x + dx, v.y + dy))
  {
    v.x += dx;
    v.y += dy;
    return true;
  }
  if (dx != 0 && dy != 0)
    return vehicleStep(v, dx, 0) || vehicleStep(v, 0, dy);
  return false;
}

void World::updateDrive(int mvX, int mvY, const PlayerInput& input)
{
  auto& p = mPlayer;
  Vehicle& v = mVehicles[std::size_t(p.vehicle)];
  if (input.use.triggered || (mvY > 0 && input.jump.triggered))
  {
    leaveVehicle(false);
    if (p.vehicle < 0)
      return;
  }
  switch (v.kind)
  {
    case VehicleKind::Tank:
      driveTank(v, mvX, mvY, input);
      break;
    case VehicleKind::Heli:
      driveHeli(v, mvX, mvY, input);
      break;
    case VehicleKind::Bike:
      driveBike(v, mvX, mvY, input);
      break;
    case VehicleKind::Sub:
      driveSub(v, mvX, mvY, input);
      break;
    case VehicleKind::Ship:
      driveShip(v, mvX, mvY, input);
      break;
    case VehicleKind::Mech:
      driveMech(v, mvX, mvY, input);
      break;
    case VehicleKind::Count:
      break;
  }
  placeDriver();
  if (v.y > mMap.height() + 3)
  {
    // Over the edge: the vehicle is gone and so is the runner.
    v.occupied = false;
    v.wreck = kWreckFrames;
    p.vehicle = -1;
    playSound(Sfx::Death);
    p.state = PlayerState::Dying;
    p.deathPhase = 3;
    p.frames = 0;
    p.hidden = true;
    ++mStats.deaths;
  }
}

void World::driveTank(Vehicle& v, int mvX, int mvY, const PlayerInput& input)
{
  v.aim = mvY < 0 ? 1 : 0;
  if (!vehicleFall(v, 2) && mvX != 0)
  {
    if (mvX != v.facing)
      v.facing = mvX; // turning the hull costs the frame
    else if (++v.step % 3 != 0)
      mMap.moveHorizontallyWithStairStepping(v.x, v.y, v.w, v.h, mvX);
  }
  if (input.fire.pressed && v.cool == 0)
  {
    v.cool = 9;
    const float px = float(v.x) + float(v.w) * 0.5f, py = float(v.y - 3);
    const float vx = v.aim ? 0.7f * float(v.facing) : float(v.facing), vy = v.aim ? -0.7f : 0.0f;
    Projectile& pr =
      vehicleShot(ShotKind::Rocket, px + vx * 4.5f - 1.0f, py + vy * 4.5f, vx, vy, 3, 5);
    pr.radius = 3;
    pr.gy = v.aim ? 0.04f : 0.015f;
    playSound(Sfx::Cannon);
    mCamera.shake(4, 1.0f);
    burst({(px + vx * 5.0f) * kCellSize, (py + vy * 5.0f) * kCellSize}, rgb(255, 240, 180), rgb(255, 140, 40), 8, 1.6f);
  }
}

void World::driveHeli(Vehicle& v, int mvX, int mvY, const PlayerInput& input)
{
  const VehicleDef& d = vehicleDef(v.kind);
  const bool grounded = mMap.onSolidGround(v.box());
  if (grounded)
    v.fuel = std::min(d.fuel, v.fuel + 6); // refuels on the ground
  else if (v.fuel > 0)
  {
    --v.fuel;
    if (v.fuel == kFuelLow)
      showMessage("HELICOPTER FUEL LOW - LAND TO REFUEL");
    else if (v.fuel == 0)
      showMessage("OUT OF FUEL");
  }
  ++v.step;
  if (mvX != 0)
    v.facing = mvX;
  v.vx = approach(v.vx, mvX * (v.fuel > 0 ? kHeliSpeed : kHeliSpeed / 2), kHeliAccel);
  v.ax += v.vx;
  while (std::abs(v.ax) >= 16)
  {
    const int s = v.ax > 0 ? 1 : -1;
    v.ax -= s * 16;
    if (!vehicleStep(v, s, 0))
    {
      v.vx = v.ax = 0;
      break;
    }
  }
  // Up climbs while there is fuel; with none it settles to the ground.
  int dy = mvY;
  if (v.fuel == 0)
    dy = 1;
  if (dy != 0)
    vehicleStep(v, 0, dy);
  if (v.cool == 0 && input.fire.pressed && !(mvY > 0 && v.cool2 == 0))
  {
    v.cool = 3;
    const float y = float(v.y - 1) + (v.barrel ? 0.0f : 0.5f);
    v.barrel = !v.barrel;
    vehicleShot(ShotKind::Normal, v.facing > 0 ? float(v.x + v.w) : float(v.x - 2), y, float(v.facing), 0.0f, 3, 1);
    playSound(Sfx::Shot);
  }
  if (input.fire.pressed && mvY > 0 && v.cool2 == 0)
  {
    v.cool2 = 12;
    Projectile& pr = vehicleShot(ShotKind::Rocket, float(v.x + v.w / 2 - 1), float(v.y + 1),
      0.3f * float(v.facing), 0.5f, 2, 5);
    pr.gy = 0.15f;
    pr.radius = 4;
    pr.w = 2;
    pr.h = 2;
    pr.dy = 1;
    playSound(Sfx::RocketShot);
  }
}

void World::driveBike(Vehicle& v, int mvX, int mvY, const PlayerInput& input)
{
  (void)mvY;
  const int arcLen = int(kBikeArc.size());
  const bool grounded = v.air < 0;
  if (mvX != 0)
    v.facing = mvX;
  const int target = mvX * kBikeSpeed;
  const bool reversing = mvX != 0 && v.vx != 0 && (mvX > 0) != (v.vx > 0);
  v.vx = approach(v.vx, target, mvX == 0 ? 3 : (reversing ? 6 : (grounded ? kBikeAccel : 2)));
  if (std::abs(v.vx) >= 8)
    ++v.step;
  v.ax += v.vx;
  while (std::abs(v.ax) >= 16)
  {
    const int s = v.ax > 0 ? 1 : -1;
    v.ax -= s * 16;
    const MoveResult r = grounded ? mMap.moveHorizontallyWithStairStepping(v.x, v.y, v.w, v.h, s)
                                  : mMap.moveHorizontally(v.x, v.y, v.w, v.h, s);
    if (r != MoveResult::Completed)
    {
      if (std::abs(v.vx) >= 24)
      {
        playSound(Sfx::Land);
        mCamera.shake(4, 1.0f);
      }
      v.vx = v.ax = 0;
      break;
    }
  }
  if (grounded)
  {
    if (input.jump.triggered && !mMap.touchingCeiling(v.box()))
    {
      v.air = 0;
      playSound(Sfx::Jump);
    }
    else if (!mMap.onSolidGround(v.box()))
      v.air = arcLen; // rolled off a ledge
  }
  if (v.air >= 0 && v.air < arcLen)
  {
    if (mMap.moveVertically(v.x, v.y, v.w, v.h, -kBikeArc[std::size_t(v.air)]) != MoveResult::Completed)
      v.air = arcLen;
    else
      ++v.air;
  }
  else if (v.air >= arcLen)
  {
    if (mMap.moveVertically(v.x, v.y, v.w, v.h, 2) != MoveResult::Completed)
    {
      v.air = -1;
      playSound(Sfx::Land);
      burst({(float(v.x) + float(v.w) * 0.5f) * kCellSize, float(v.y + 1) * kCellSize}, rgb(255, 120, 200),
        rgb(255, 255, 255), 8, 1.2f);
    }
  }
  if (input.fire.pressed && v.cool == 0)
  {
    v.cool = 4;
    vehicleShot(ShotKind::Normal, v.facing > 0 ? float(v.x + v.w) : float(v.x - 2), float(v.y - 1),
      float(v.facing), 0.0f, 4, 1);
    playSound(Sfx::Shot);
  }
}

void World::driveSub(Vehicle& v, int mvX, int mvY, const PlayerInput& input)
{
  if (!vehicleFits(v.kind, v.x, v.y))
  {
    // Out of the water: it can only drop back in.
    mMap.moveVertically(v.x, v.y, v.w, v.h, 2);
  }
  else
  {
    if (mvX != 0)
    {
      v.facing = mvX;
      vehicleStep(v, mvX, 0);
    }
    // A slow ballast tank: up and down every frame, but no faster.
    if (mvY != 0)
      vehicleStep(v, 0, mvY);
    if (mvX != 0 || mvY != 0)
      ++v.step;
  }
  if (input.fire.pressed && v.cool == 0)
  {
    v.cool = 12;
    Projectile& pr = vehicleShot(ShotKind::Rocket, v.facing > 0 ? float(v.x + v.w) : float(v.x - 2),
      float(v.y - 1), float(v.facing), 0.0f, 2, 5);
    pr.radius = 3;
    playSound(Sfx::Torpedo);
  }
}

void World::driveShip(Vehicle& v, int mvX, int mvY, const PlayerInput& input)
{
  if (mvX != 0)
    v.facing = mvX;
  // Thrust adds to the drift; letting go only slows it a little.
  v.vx = mvX != 0 ? std::clamp(v.vx + mvX * kShipThrust, -kShipSpeed, kShipSpeed) : approach(v.vx, 0, 1);
  v.vy = mvY != 0 ? std::clamp(v.vy + mvY * kShipThrust, -kShipSpeed, kShipSpeed) : approach(v.vy, 0, 1);
  if (mvX != 0 || mvY != 0)
    ++v.step;
  v.ax += v.vx;
  v.ay += v.vy;
  auto bump = [&](int& vel, int& acc) {
    // Bounces back off walls; hard knocks dent the hull.
    if (std::abs(vel) > 28)
      damageVehicle(v, 1);
    vel = -vel / 2;
    acc = 0;
    playSound(Sfx::Land);
  };
  while (std::abs(v.ax) >= 16)
  {
    const int s = v.ax > 0 ? 1 : -1;
    v.ax -= s * 16;
    if (!vehicleStep(v, s, 0))
    {
      bump(v.vx, v.ax);
      break;
    }
  }
  while (std::abs(v.ay) >= 16)
  {
    const int s = v.ay > 0 ? 1 : -1;
    v.ay -= s * 16;
    if (!vehicleStep(v, 0, s))
    {
      bump(v.vy, v.ay);
      break;
    }
  }
  if (input.fire.pressed && v.cool == 0)
  {
    v.cool = 4;
    const float y = float(v.y) - (v.barrel ? 0.5f : 2.5f);
    v.barrel = !v.barrel;
    vehicleShot(ShotKind::Laser, v.facing > 0 ? float(v.x + v.w) : float(v.x - 3), y, float(v.facing), 0.0f, 5, 2);
    mProjectiles.back().w = 3;
    playSound(Sfx::LaserShot);
  }
}

void World::driveMech(Vehicle& v, int mvX, int mvY, const PlayerInput& input)
{
  const int arcLen = int(kMechArc.size());
  v.aim = mvY < 0 ? 1 : 0;
  if (mvX != 0 && mvX != v.facing)
    v.facing = mvX;
  else if (mvX != 0 && (++v.step % 2 == 0))
  {
    if (v.air < 0)
      mMap.moveHorizontallyWithStairStepping(v.x, v.y, v.w, v.h, mvX);
    else
      mMap.moveHorizontally(v.x, v.y, v.w, v.h, mvX);
  }
  if (v.air < 0)
  {
    if (input.jump.triggered && !mMap.touchingCeiling(v.box()))
    {
      v.air = 0;
      v.fallen = 0;
      playSound(Sfx::RocketShot);
    }
    else if (!mMap.onSolidGround(v.box()))
    {
      v.air = arcLen;
      v.fallen = 0;
    }
  }
  if (v.air >= 0 && v.air < arcLen)
  {
    // The jets fire while jump is held; letting go cuts them.
    if (!input.jump.pressed && v.air > 1)
      v.air = arcLen;
    else if (mMap.moveVertically(v.x, v.y, v.w, v.h, -kMechArc[std::size_t(v.air)]) != MoveResult::Completed)
      v.air = arcLen;
    else
      ++v.air;
  }
  else if (v.air >= arcLen)
  {
    const int before = v.y;
    if (mMap.moveVertically(v.x, v.y, v.w, v.h, 2) != MoveResult::Completed)
    {
      v.fallen += v.y - before;
      v.air = -1;
      if (v.fallen >= 6)
        stomp(v);
      else
        playSound(Sfx::Land);
      v.fallen = 0;
    }
    else
      v.fallen += v.y - before;
  }
  if (input.fire.pressed && v.cool == 0)
  {
    v.cool = 5;
    Projectile* pr = nullptr;
    if (v.aim)
      pr = &vehicleShot(ShotKind::Normal, float(v.x + v.w / 2 + v.facing), float(v.y - v.h - 1), 0.0f, -1.0f, 3, 2);
    else
      pr = &vehicleShot(ShotKind::Normal, v.facing > 0 ? float(v.x + v.w) : float(v.x - 2), float(v.y - 5),
        float(v.facing), 0.0f, 3, 2);
    pr->strong = true;
    playSound(Sfx::Shot);
  }
}

void World::stomp(Vehicle& v)
{
  // Lands hard: the floor under it cracks if it can, and a shockwave runs
  // along the ground both ways.
  playSound(Sfx::Stomp);
  mCamera.shake(10, 2.0f);
  const CellBox under{v.x, v.y + 1, v.w, 2};
  for (int k = 0; k < 4; ++k)
    if (!hitBreakable(under, 9, 5))
      break;
  const CellBox wave{v.x - 8, v.y - 3, v.w + 16, 5};
  for (auto& e : mEnemies)
    if (e.alive && e.active && !e.hidden && e.box().intersects(wave))
      damageEnemy(e, 3);
  for (int k = -1; k <= 1; k += 2)
    burst({(float(v.x) + float(v.w) * 0.5f + float(k) * 6.0f) * kCellSize, float(v.y + 1) * kCellSize},
      rgb(220, 200, 170), rgb(120, 100, 80), 14, 2.4f, false);
}

// --- Per frame -----------------------------------------------------------------

void World::vehicleContacts(Vehicle& v)
{
  const CellBox vb = v.box();
  for (auto& e : mEnemies)
  {
    if (!e.alive || !e.active || e.hidden || e.trapped || e.y < 0 || !e.box().intersects(vb))
      continue;
    const EnemyDef& def = enemyDef(e.def);
    if (def.flags & kEnemyHarmless)
      continue;
    const bool small = def.hp <= 4 && e.h <= 5 && e.kind != EnemyKind::SeaMine;
    if ((v.kind == VehicleKind::Tank || v.kind == VehicleKind::Mech) && small)
    {
      // Crushed under the treads (or the feet).
      playSound(Sfx::Crunch);
      damageEnemy(e, e.hp);
      continue;
    }
    if (v.kind == VehicleKind::Bike && std::abs(v.vx) >= 24 && v.mercy == 0)
    {
      damageEnemy(e, 2);
      v.vx = -v.vx / 3;
      v.mercy = 8;
      continue;
    }
    damageVehicle(v, 1);
  }
  // Rotors, hulls and hulls in space don't like spikes; treads, hover
  // skirts and steel feet don't mind.
  if ((v.kind == VehicleKind::Heli || v.kind == VehicleKind::Sub || v.kind == VehicleKind::Ship) &&
      mMap.overlapsHazard(vb))
    damageVehicle(v, 1);
  if (!mLavas.empty() && inLava(vb))
    damageVehicle(v, 2);
}

void World::updateVehicles(const PlayerInput& /*input*/)
{
  if (mVehicles.empty())
    return;
  bool subs = false;
  for (std::size_t i = 0; i < mVehicles.size(); ++i)
  {
    Vehicle& v = mVehicles[i];
    subs = subs || v.kind == VehicleKind::Sub;
    if (v.wreck > 0)
    {
      if (--v.wreck == 0)
      {
        const int keepHomeX = v.homeX, keepHomeY = v.homeY;
        v.x = v.prevX = keepHomeX;
        v.y = v.prevY = keepHomeY;
        v.facing = v.homeFacing;
        v.hp = vehicleDef(v.kind).hp;
        v.fuel = vehicleDef(v.kind).fuel;
        v.vx = v.vy = v.ax = v.ay = 0;
        v.air = -1;
        burst(cellCenter(v.box()), vehicleDef(v.kind).color, rgb(255, 255, 255), 16, 1.6f);
      }
      continue;
    }
    if (v.mercy > 0)
      --v.mercy;
    if (v.flash > 0)
      --v.flash;
    if (v.cool > 0)
      --v.cool;
    if (v.cool2 > 0)
      --v.cool2;
    if (v.occupied)
    {
      vehicleContacts(v);
      continue;
    }
    // Parked: ground vehicles and empty helicopters settle onto the
    // ground; a submarine stays where it is in the water, a ship drifts to
    // a stop.
    switch (v.kind)
    {
      case VehicleKind::Tank:
      case VehicleKind::Bike:
      case VehicleKind::Mech:
        vehicleFall(v, 2);
        v.air = -1;
        break;
      case VehicleKind::Heli:
        v.vx = 0;
        vehicleFall(v, 1);
        if (mMap.onSolidGround(v.box()))
          v.fuel = std::min(vehicleDef(v.kind).fuel, v.fuel + 6);
        break;
      case VehicleKind::Sub:
        if (!vehicleFits(v.kind, v.x, v.y))
          vehicleFall(v, 2);
        break;
      case VehicleKind::Ship:
        v.vx = approach(v.vx, 0, 2);
        v.vy = approach(v.vy, 0, 2);
        break;
      case VehicleKind::Count:
        break;
    }
    if (v.y > mMap.height() + 3)
      v.wreck = kWreckFrames;
  }
  // A moored submarine's deck can be stood on.
  if (subs)
    syncPlatformCollision();
}

} // namespace gr
