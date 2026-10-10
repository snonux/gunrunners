// Level 27, The Long Rain: shadows, flashlights and the club spotlight, the
// "?" over a runner being seen, the whistle and the alarm's Goons, Crows
// that caw, loud shots heard, the Silencer's quiet triple hit, Clean Case
// and Case Closed, the safe behind the painting, the office alarm and its
// shutter, the jukebox, the drifting smoke cloud and the trench coat. Also
// the Silent Picture bonus (rules=silent_film): 1.5x logic, banana peels,
// pratfalls, the runaway pram, the ladder gag, title cards and hats.
// Drawing is in world_noir_draw.cpp.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace gr
{

namespace
{

constexpr float kPi = 3.14159265f;
constexpr int kWhistleCool = 45;
constexpr int kGoonFirst = 15;   // frames from the whistle to the first Goon
constexpr int kGoonLeave = 150;  // frames a Goon may take walking back to its door
constexpr int kCrowRest = 45;    // frames after a caw before a Crow caws again
constexpr int kPeelSlide = 8;    // cells
constexpr int kPratfall = 15;
constexpr int kPratDrop = 8;     // cells: a landing from higher than this is a pratfall
constexpr int kCard = 30;
constexpr int kTrip = 30;        // frames an extra lies on the floor
constexpr int kGagTip = 12;

int sgn(int v) { return (v > 0) - (v < 0); }

CellBox blockRect(int x0, int y0, int x1, int y1)
{
  return {std::min(x0, x1) * kCellsPerTile, std::min(y0, y1) * kCellsPerTile, (std::abs(x1 - x0) + 1) * kCellsPerTile,
    (std::abs(y1 - y0) + 1) * kCellsPerTile};
}

} // namespace

// --- Setup ---------------------------------------------------------------------------------

bool World::setupNoirEntity(const EntityDef& e)
{
  const bool film = mLevel->rules.find("silent_film") != std::string::npos;
  if (mLevel->themeKey != "noir_city" && !film)
    return false;
  auto& n = mNoir;
  auto& f = mFilm;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  const bool hasRect = e.rect("rect", x0, y0, x1, y1);
  if (film)
  {
    if (e.kind == "peel" && e.hasPos)
    {
      f.peels.push_back(boxAt(e.x * kCellsPerTile, e.y * kCellsPerTile + 1, 2, 1));
      return true;
    }
    if (e.kind == "pram" && e.hasPos)
    {
      // `@ pram x y to=x1 every=`: rolls from x down the street's steps to x1.
      f.pramX0 = e.x * kCellsPerTile;
      f.pramX1 = e.num("to", e.x + 20) * kCellsPerTile;
      f.pramY = e.y * kCellsPerTile + 1;
      f.pramEvery = std::max(30, e.num("every", 90));
      f.pramT = 0;
      return true;
    }
    if (e.kind == "laddergag" && e.hasPos)
    {
      // `@ laddergag x y h= len=`: a ladder standing on (x, y), h blocks tall,
      // that tips over to the right into a bridge len blocks long.
      f.gagX = e.x;
      f.gagY1 = e.y;
      f.gagY0 = e.y - std::max(2, e.num("h", 6)) + 1;
      f.gagLen = std::max(2, e.num("len", 6));
      for (int by = f.gagY0; by <= f.gagY1; ++by)
        mMap.setBlock(f.gagX, by, Tile::Ladder);
      return true;
    }
    if (e.kind == "room" && e.hasPos)
    {
      // `@ room x 0 text="..."`: a title card when the runner first gets to x.
      f.roomX.push_back(e.x * kCellsPerTile);
      f.cards.push_back(e.str("text"));
      return true;
    }
    return false;
  }
  if (e.kind == "shadow" && hasRect)
  {
    n.shadows.push_back(blockRect(x0, y0, x1, y1));
    n.on = true;
    return true;
  }
  if (e.kind == "dark" && hasRect)
  {
    n.darks.push_back(blockRect(x0, y0, x1, y1));
    n.on = true;
    return true;
  }
  if (e.kind == "rain" && hasRect)
  {
    n.rains.push_back(blockRect(x0, y0, x1, y1));
    n.on = true;
    return true;
  }
  if (e.kind == "light" && e.hasPos)
  {
    n.lights.push_back({e.x * kCellsPerTile + 1, e.y * kCellsPerTile + 1, e.num("r", 2) * kCellsPerTile});
    n.on = true;
    return true;
  }
  if (e.kind == "spotlight")
  {
    NoirSpot s;
    s.x = (e.hasPos ? e.x : e.num("x")) * kCellsPerTile + 1;
    s.y = (e.hasPos ? e.y : e.num("y")) * kCellsPerTile + 1;
    const auto sweep = e.list("sweep");
    if (sweep.size() == 2)
    {
      s.x0 = std::min(sweep[0], sweep[1]) * kCellsPerTile + 1;
      s.x1 = std::max(sweep[0], sweep[1]) * kCellsPerTile + 1;
    }
    s.floor = e.num("floor", 37) * kCellsPerTile + 1;
    s.period = std::max(2, e.num("period", 120));
    const auto cone = e.list("cone");
    if (cone.size() == 2)
    {
      s.half = std::max(1, cone[0] / 2);
      s.len = cone[1] * kCellsPerTile;
    }
    n.spots.push_back(s);
    n.on = true;
    return true;
  }
  if (e.kind == "door" && e.hasPos)
  {
    n.doors.push_back({e.x * kCellsPerTile, e.y * kCellsPerTile + 1});
    n.on = true;
    return true;
  }
  if (e.kind == "switch" && e.hasPos && e.id == "painting")
  {
    n.paintX = e.x;
    n.paintY = e.y;
    const auto lad = e.list("ladder"); // x, y0, y1 (blocks)
    if (lad.size() == 3)
    {
      n.ladderX = lad[0];
      n.ladderY0 = std::min(lad[1], lad[2]);
      n.ladderY1 = std::max(lad[1], lad[2]);
    }
    n.on = true;
    return true;
  }
  if (e.kind == "switch" && e.hasPos && e.id == "alarm")
  {
    n.alarmX = e.x;
    n.alarmY = e.y;
    const auto sh = e.list("shutter"); // x, y0, y1 (blocks)
    if (sh.size() == 3)
    {
      n.shutterX = sh[0];
      n.shutterY0 = std::min(sh[1], sh[2]);
      n.shutterY1 = std::max(sh[1], sh[2]);
      for (int by = n.shutterY0; by <= n.shutterY1; ++by)
        mMap.setBlock(n.shutterX, by, Tile::Solid);
    }
    n.on = true;
    return true;
  }
  if (e.kind == "jukebox" && e.hasPos)
  {
    n.jukeX = e.x;
    n.jukeY = e.y;
    n.on = true;
    return true;
  }
  if (e.kind == "note" && e.hasPos)
  {
    n.noteX = e.x;
    n.noteY = e.y;
    n.on = true;
    return true;
  }
  if (e.kind == "prize" && e.hasPos)
  {
    n.prizeX = e.x;
    n.prizeY = e.y;
    n.prizeKind = e.str("kind", "red_evidence");
    n.prizeScore = e.num("score", 2000);
    n.on = true;
    return true;
  }
  if (e.kind == "vskin" && e.hasPos)
  {
    n.smokeAt = e.x * 1000 + e.y;
    n.smokeSkin = e.str("skin", "smoke_cloud");
    const auto path = e.path("path");
    if (path.size() >= 2)
    {
      n.smokeX0 = std::min(path[0].first, path[1].first) * kCellsPerTile;
      n.smokeX1 = std::max(path[0].first, path[1].first) * kCellsPerTile;
    }
    const float speed = e.real("speed", 0.25f);
    n.smokeEvery = std::max(1, int(std::lround(1.0f / std::max(0.05f, speed))));
    n.on = true;
    return true;
  }
  return false;
}

void World::setupNoirEnemy(Enemy& en, const EntityDef& e)
{
  if (en.kind != EnemyKind::Flatfoot && en.kind != EnemyKind::Goon && en.kind != EnemyKind::Crow)
    return;
  if (mLevel->rules.find("silent_film") != std::string::npos)
  {
    mFilm.on = true;
    return;
  }
  mNoir.on = true;
  if (en.kind != EnemyKind::Flatfoot)
    return;
  // path=x0,y;x1,y: the patrol's ends (blocks); once=1 walks it once and is gone.
  const auto path = e.path("path");
  if (path.size() >= 2)
  {
    const int a = path[0].first * kCellsPerTile, b = path[1].first * kCellsPerTile;
    en.railX0 = std::min(a, b);
    en.railX1 = std::max(a, b);
    en.x = en.prevX = a;
    en.y = en.prevY = path[0].second * kCellsPerTile + 1;
    en.dir = b < a ? -1 : 1;
    if (e.num("once", 0) != 0)
    {
      mNoir.f0 = int(mEnemies.size()) - 1;
      mNoir.f0Trigger = e.num("trigger", 10) * kCellsPerTile;
      en.hidden = true;
      en.attach = 1; // walks its path once
    }
  }
  else
  {
    en.railX0 = en.x - 16;
    en.railX1 = en.x + 16;
  }
}

void World::linkNoir()
{
  auto& n = mNoir;
  auto& f = mFilm;
  f.on = f.on || mLevel->rules.find("silent_film") != std::string::npos;
  if (f.on)
  {
    // The film's clock runs 1.5x: 60 seconds on the screen are 90 on film.
    mBonusFramesLeft = mBonusFramesLeft * 3 / 2;
    f.room = -1;
    return;
  }
  n.on = n.on || mLevel->themeKey == "noir_city"; // the stealth rules hold in every noir level
  if (!n.on)
    return;
  if (n.smokeAt >= 0)
    for (std::size_t i = 0; i < mItems.size(); ++i)
    {
      Item& it = mItems[i];
      if (it.kind == ItemKind::Virus && (it.x / kCellsPerTile) * 1000 + (it.y - 1) / kCellsPerTile == n.smokeAt)
      {
        n.smoke = int(i);
        it.variant = 4; // a cloud of cigarette smoke
      }
    }
}

// --- Seeing and hearing ----------------------------------------------------------------

namespace
{

// Clear sight from (ax, ay) to (bx, by) in cells: no solid tile between.
bool clearSight(const CollisionMap& map, int ax, int ay, int bx, int by)
{
  const int steps = std::max(std::abs(bx - ax), std::abs(by - ay));
  for (int i = 1; i < steps; ++i)
  {
    const int x = ax + (bx - ax) * i / steps, y = ay + (by - ay) * i / steps;
    if (map.solid(x, y))
      return false;
  }
  return true;
}

} // namespace

bool World::noirHiddenAt(int cx, int cy) const
{
  const auto& n = mNoir;
  bool cover = false;
  for (const auto& b : n.shadows)
    cover = cover || b.contains(cx, cy);
  for (const auto& b : n.darks)
    cover = cover || b.contains(cx, cy);
  if (!cover)
    return false;
  for (const auto& l : n.lights)
  {
    const int dx = cx - l.x, dy = cy - l.y;
    if (dx * dx + dy * dy <= l.r * l.r)
      return false;
  }
  return true;
}

int World::noirSpotX(const NoirSpot& s) const
{
  const int half = s.period / 2;
  const int t = mStats.frames % s.period;
  const int u = t < half ? t : s.period - t; // 0..half and back
  return s.x0 + (s.x1 - s.x0) * u / std::max(1, half);
}

bool World::inSpotlight(int cx, int cy) const
{
  for (const auto& s : mNoir.spots)
  {
    const float ax = float(noirSpotX(s) - s.x), ay = float(s.floor - s.y);
    const float vx = float(cx - s.x), vy = float(cy - s.y);
    const float vl = std::sqrt(vx * vx + vy * vy), al = std::sqrt(ax * ax + ay * ay);
    if (vl > float(s.len) || vl < 1.0f || al < 1.0f)
      continue;
    const float cosA = (ax * vx + ay * vy) / (al * vl);
    if (cosA >= std::cos(float(s.half) * kPi / 180.0f) && clearSight(mMap, s.x, s.y, cx, cy))
      return true;
  }
  return false;
}

bool World::inFlashlight(const Enemy& e, int cx, int cy) const
{
  if (!e.alive || e.hidden || e.kind != EnemyKind::Flatfoot || e.dive > 0)
    return false;
  const CellBox b = e.box();
  const int ox = e.dir > 0 ? b.right() + 1 : b.left() - 1, oy = b.top() + 2;
  const int dx = (cx - ox) * e.dir, dy = cy - oy;
  if (dx <= 0 || dx * dx + dy * dy > kFlashLen * kFlashLen)
    return false;
  if (std::abs(dy) > dx * std::tan(float(kFlashHalf) * kPi / 180.0f))
    return false;
  return clearSight(mMap, ox, oy, cx, cy);
}

bool World::noirUnaware(const Enemy& e) const
{
  if (!mNoir.on || mNoir.alarm > 0 || e.kind == EnemyKind::Goon)
    return false;
  const CellBox pb = mPlayer.box();
  return !inFlashlight(e, pb.x + pb.w / 2, pb.y + pb.h / 2);
}

void World::raiseAlarm(const std::string& why)
{
  auto& n = mNoir;
  static const bool debug = std::getenv("GR_NOIR_DEBUG") != nullptr;
  if (debug && !mSimulation)
    std::fprintf(stderr, "f%d alarm at %d,%d: %s\n", mStats.frames, mPlayer.x, mPlayer.y, why.c_str());
  if (n.alarm == 0)
  {
    ++n.alarms;
    n.goonIn = kGoonFirst;
    showMessage(why);
    playSound(Sfx::Siren);
  }
  n.segmentAlarm = true;
  n.alarm = kAlarmFrames;
  n.suspicion = 0;
  n.alarmFlash = 30;
  if (n.shutterX >= 0 && !n.shutterOpen)
  {
    n.shutterOpen = true;
    for (int by = n.shutterY0; by <= n.shutterY1; ++by)
      mMap.setBlock(n.shutterX, by, Tile::Empty);
    playSound(Sfx::Shutter);
  }
}

void World::noirShotFired(int x, int y, bool loud)
{
  auto& n = mNoir;
  if (!n.on || !loud)
    return;
  n.noiseX = x;
  n.noiseY = y;
  n.noiseT = 0;
  for (auto& e : mEnemies)
  {
    if (!e.alive || e.hidden)
      continue;
    const CellBox b = e.box();
    const int dx = x - (b.x + b.w / 2), dy = y - (b.y + b.h / 2);
    if (dx * dx + dy * dy > kHearReach * kHearReach)
      continue;
    if (e.kind == EnemyKind::Flatfoot && e.dive <= 0)
    {
      e.aimX = kHearTurn; // looks toward the sound
      e.dir = dx < 0 ? -1 : 1;
    }
    else if (e.kind == EnemyKind::Crow && e.tell == 0 && e.cool == 0)
    {
      e.tell = enemyDef(e.def).tell; // turns its head: shot now, it never caws
      e.dir = dx < 0 ? -1 : 1;
    }
  }
}

void World::noirCheckpoint()
{
  auto& n = mNoir;
  if (!n.on)
    return;
  if (!n.segmentAlarm)
  {
    const auto& p = mPlayer;
    addScore(kCleanCase, {(float(p.x) + 1.5f) * kCellSize, float(p.y - 6) * kCellSize});
    showMessage("CLEAN CASE");
    playSound(Sfx::Chime);
  }
  n.segmentAlarm = false;
}

// --- Enemies -----------------------------------------------------------------------------------

namespace
{

// One step toward x along the floor; false at a wall or a ledge.
bool stepToward(const CollisionMap& map, Enemy& e, int dir)
{
  const CellBox b = e.box();
  const int aheadX = dir > 0 ? b.right() + 1 : b.left() - 1;
  const bool wall = dir > 0 ? map.touchingRightWall(b) : map.touchingLeftWall(b);
  if (wall || !map.solidTop(aheadX, b.bottom() + 1))
    return false;
  e.x += dir;
  return true;
}

} // namespace

void World::updateFlatfoot(Enemy& e, const EnemyDef& def)
{
  auto& n = mNoir;
  const auto& p = mPlayer;
  const CellBox b = e.box();
  if (mFilm.on)
  {
    updateFilmExtra(e, def);
    return;
  }
  // F0 waits out of sight until the runner gets to the alley's doorway.
  if (e.hidden)
  {
    if (int(&e - mEnemies.data()) == n.f0 && !n.f0Out && p.x + 1 >= n.f0Trigger)
    {
      n.f0Out = true;
      e.hidden = false;
      e.active = true;
      playSound(Sfx::Clunk); // the door
    }
    return;
  }
  if (!mMap.onSolidGround(b))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, 1);
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return;
  }
  if (e.aimX > 0)
  {
    --e.aimX; // listening, turned toward the sound
    return;
  }
  if (e.timer % std::max(1, def.stepEvery) != 0)
    return;
  if (n.alarm > 0)
  {
    // The whistle's blown: after the runner.
    const int dx = (p.x + 1) - (b.x + b.w / 2);
    if (std::abs(dx) > 2)
    {
      e.dir = sgn(dx);
      stepToward(mMap, e, e.dir);
    }
    return;
  }
  // Back to the patrol, then along it.
  if (e.x < e.railX0 || e.x > e.railX1)
  {
    e.dir = e.x < e.railX0 ? 1 : -1;
    if (!stepToward(mMap, e, e.dir) && e.attach == 0)
    {
      e.railX0 = std::min(e.railX0, e.x);
      e.railX1 = std::max(e.railX1, e.x);
    }
    return;
  }
  const bool end = e.dir > 0 ? e.x + 1 > e.railX1 : e.x - 1 < e.railX0;
  if (end && e.attach == 1)
  {
    // F0 goes in at the side door.
    e.alive = false;
    e.hidden = true;
    --mStats.enemiesTotal;
    playSound(Sfx::Clunk);
    return;
  }
  if (end || !stepToward(mMap, e, e.dir))
    e.dir = -e.dir;
}

void World::updateGoon(Enemy& e, const EnemyDef& def)
{
  auto& n = mNoir;
  const auto& p = mPlayer;
  const CellBox b = e.box();
  const int idx = int(&e - mEnemies.data());
  if (!mMap.onSolidGround(b))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, 1);
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return;
  }
  // The alarm is over: back in through the door it came out of.
  if (n.alarm == 0)
  {
    if (n.burstBy == idx)
      n.burstBy = -1;
    e.tell = 0;
    const NoirDoor& d = n.doors[std::size_t(std::clamp(e.attach, 0, int(n.doors.size()) - 1))];
    ++e.dive;
    const int dx = d.x - e.x;
    if (std::abs(dx) <= 1 || e.dive > kGoonLeave)
    {
      e.alive = false;
      e.hidden = true;
      return;
    }
    if (e.timer % std::max(1, def.stepEvery) == 0)
    {
      e.dir = sgn(dx);
      if (!stepToward(mMap, e, e.dir) && e.dive > 30)
        e.dive = kGoonLeave; // can't get there: slips away
    }
    return;
  }
  const int dx = (p.x + 1) - (b.x + b.w / 2), dy = p.y - b.bottom();
  // The shared burst: one Goon fires at a time.
  if (n.burstBy == idx)
  {
    e.dir = dx < 0 ? -1 : 1;
    if (e.tell > 0)
    {
      if (--e.tell == 0)
        playSound(Sfx::Click);
      return;
    }
    if (e.cool > 0)
    {
      --e.cool;
      return;
    }
    spawnProjectile(ShotKind::Enemy, e.dir > 0 ? b.right() + 1 : b.left() - 1, b.top() + 2, e.dir, 0);
    playSound(Sfx::EnemyShot);
    if (--n.burstShots <= 0)
    {
      n.burstBy = -1;
      n.burstCool = def.cooldown;
    }
    else
      e.cool = kBurstGap - 1;
    return;
  }
  const bool canFire = std::abs(dx) <= 24 && std::abs(dy) <= 4 && p.state != PlayerState::Dying && isOnScreen(b, 0);
  if (n.burstBy < 0 && n.burstCool == 0 && canFire)
  {
    n.burstBy = idx;
    n.burstShots = kBurstShots;
    e.tell = def.tell; // raises the tommy gun
    e.cool = 0;
    e.dir = dx < 0 ? -1 : 1;
    return;
  }
  if (e.timer % std::max(1, def.stepEvery) != 0)
    return;
  // Closes in to about 8 cells.
  if (std::abs(dx) > 8)
  {
    e.dir = sgn(dx);
    stepToward(mMap, e, e.dir);
  }
  else
    e.dir = dx < 0 ? -1 : 1;
}

void World::updateCrow(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  if (e.cool > 0)
    --e.cool;
  if (e.tell > 0)
  {
    if (--e.tell == 0)
    {
      raiseAlarm("A CROW CAWS - ALARM!");
      playSound(Sfx::Screech);
      e.cool = kCrowRest;
    }
    return;
  }
  if (e.cool > 0 || p.state == PlayerState::Dying || p.state == PlayerState::Teleporting)
    return;
  const CellBox pb = p.box(), b = e.box();
  const int cx = pb.x + pb.w / 2, cy = pb.y + pb.h / 2;
  const int dx = cx - (b.x + 1), dy = cy - (b.y + 1);
  if (!mNoir.hidden && dx * dx + dy * dy <= kCrowReach * kCrowReach)
  {
    e.tell = def.tell; // turns its head and ruffles
    e.dir = dx < 0 ? -1 : 1;
  }
}

// --- Each frame ----------------------------------------------------------------------------

void World::updateNoir(const PlayerInput& input)
{
  (void)input;
  auto& n = mNoir;
  auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  const CellBox pb = p.box();
  const int cx = pb.x + pb.w / 2, cy = pb.y + pb.h / 2;

  // Seen? Shadows and dark rooms hide; the spotlight lights what it falls on.
  const bool spot = inSpotlight(cx, cy);
  n.hidden = !spot && noirHiddenAt(cx, cy);
  n.inCone = false;
  if (alive && !n.hidden)
  {
    n.inCone = spot;
    for (const auto& e : mEnemies)
      if (e.active && inFlashlight(e, cx, cy))
        n.inCone = true;
  }
  if (n.inCone && p.turbo == 0 && n.whistleCool == 0)
  {
    if (++n.suspicion >= kSpotFill)
      raiseAlarm(spot ? "CAUGHT IN THE SPOTLIGHT - ALARM!" : "SPOTTED - THE WHISTLE BLOWS!");
    if (n.suspicion == 1 && n.alarm == 0)
      playSound(Sfx::Warn);
  }
  else if (n.suspicion > 0)
    --n.suspicion;

  // The alarm: Goons out of the nearest door, one every 30 frames, two at most.
  if (n.alarmFlash > 0)
    --n.alarmFlash;
  if (n.burstCool > 0)
    --n.burstCool;
  if (n.whistleCool > 0)
    --n.whistleCool;
  if (n.alarm > 0)
  {
    if (--n.alarm == 0)
    {
      n.whistleCool = kWhistleCool;
      n.goonIn = 0;
      showMessage("THE ALARM IS OVER");
    }
    else if (n.goonIn > 0)
      --n.goonIn;
    else if (alive)
    {
      int goons = 0;
      for (const auto& e : mEnemies)
        goons += e.alive && e.kind == EnemyKind::Goon;
      int best = -1, bestD = kDoorReach * kDoorReach + 1;
      for (std::size_t i = 0; i < n.doors.size(); ++i)
      {
        const int dx = n.doors[i].x + 1 - cx, dy = n.doors[i].y - 2 - cy;
        if (dx * dx + dy * dy < bestD)
        {
          bestD = dx * dx + dy * dy;
          best = int(i);
        }
      }
      if (goons < kGoonMax && best >= 0)
      {
        const int def = enemyIndex("goon");
        spawnEnemy(def, n.doors[std::size_t(best)].x, n.doors[std::size_t(best)].y);
        Enemy& g = mEnemies.back();
        g.attach = best;
        g.active = true;
        g.dir = cx < g.x ? -1 : 1;
        playSound(Sfx::Clunk);
      }
      n.goonIn = kGoonEvery;
    }
  }
  // The shutter rolls down again once the alarm is over and nothing is in it.
  if (n.alarm == 0 && n.shutterOpen)
  {
    const CellBox sb = blockRect(n.shutterX, n.shutterY0, n.shutterX, n.shutterY1);
    bool clear = !sb.intersects(pb);
    for (const auto& e : mEnemies)
      clear = clear && !(e.alive && e.box().intersects(sb));
    if (clear)
    {
      n.shutterOpen = false;
      for (int by = n.shutterY0; by <= n.shutterY1; ++by)
        mMap.setBlock(n.shutterX, by, Tile::Solid);
      playSound(Sfx::Shutter);
    }
  }
  if (n.noiseT >= 0 && ++n.noiseT > 20)
    n.noiseT = -1;

  // The lipstick note on the bar mirror.
  if (n.noteX >= 0 && !n.noteRead && alive && blockRect(n.noteX - 1, n.noteY, n.noteX + 1, n.noteY + 3).intersects(pb))
  {
    n.noteRead = true;
    showMessage("LIPSTICK ON THE MIRROR: \"BEHIND THE PAINTING\"");
  }
  // The red evidence.
  if (n.prizeX >= 0 && !n.prizeTaken && alive && blockRect(n.prizeX, n.prizeY, n.prizeX, n.prizeY).intersects(pb))
  {
    n.prizeTaken = true;
    addScore(n.prizeScore, {(float(n.prizeX) + 0.5f) * kTileSize, (float(n.prizeY) + 0.5f) * kTileSize});
    showMessage("EVIDENCE - THE RED LEDGER");
    playSound(Sfx::Letter);
  }
  // The jukebox plays in a minor key until the club is left.
  if (n.jukebox && n.jukeX >= 0 && std::abs(p.x / kCellsPerTile - n.jukeX) > 40)
  {
    n.jukebox = false;
    mMusicOverride = mLevel->music;
  }
  // The smoke cloud drifts on its path.
  if (n.smoke >= 0 && std::size_t(n.smoke) < mItems.size())
  {
    Item& it = mItems[std::size_t(n.smoke)];
    if (!it.taken && mStats.frames % n.smokeEvery == 0)
    {
      if ((n.smokeDir > 0 && it.x >= n.smokeX1) || (n.smokeDir < 0 && it.x <= n.smokeX0))
        n.smokeDir = -n.smokeDir;
      it.x += n.smokeDir;
    }
  }
  // Stand still in the rain for 20 seconds: a trench coat and a hat.
  bool wet = false;
  for (const auto& r : n.rains)
    wet = wet || r.contains(cx, cy);
  const bool still = p.state == PlayerState::OnGround && p.x == p.prevX && p.y == p.prevY;
  if (n.coat)
    ++n.coatT;
  else if (wet && still && alive)
  {
    if (++n.rainFrames >= kRainCoat)
    {
      n.coat = true;
      showMessage("A TRENCH COAT AND A HAT. IT ALWAYS RAINS IN THIS TOWN");
      playSound(Sfx::Chime);
    }
  }
  else
    n.rainFrames = 0;
}

bool World::shotAtNoir(Projectile& pr, const CellBox& b)
{
  auto& n = mNoir;
  if (n.paintX >= 0 && !n.paintOpen && blockRect(n.paintX - 1, n.paintY, n.paintX + 1, n.paintY + 1).intersects(b))
  {
    // The painting swings open; the safe opens; a ladder goes up to the booth.
    n.paintOpen = true;
    openSafe();
    showMessage("THE PAINTING SWINGS OPEN - THE SAFE TOO");
    playSound(Sfx::Creak);
    return true;
  }
  if (n.alarmX >= 0 && blockRect(n.alarmX, n.alarmY, n.alarmX, n.alarmY + 1).intersects(b))
  {
    raiseAlarm("THE OFFICE ALARM RINGS!");
    playSound(Sfx::Bell);
    return true;
  }
  if (n.jukeX >= 0 && !n.jukebox && blockRect(n.jukeX, n.jukeY - 1, n.jukeX, n.jukeY).intersects(b))
  {
    n.jukebox = true;
    mMusicOverride = mLevel->music + "_minor";
    showMessage("THE JUKEBOX PLAYS IT IN A MINOR KEY");
    playSound(Sfx::Clunk);
    return true;
  }
  (void)pr;
  return false;
}

void World::openSafe()
{
  auto& n = mNoir;
  if (n.ladderX < 0)
    return;
  for (int by = n.ladderY0; by <= n.ladderY1; ++by)
    mMap.setBlock(n.ladderX, by, Tile::Ladder);
  // The block beside the ladder's top opens as a hatch: a runner on the
  // ladder is a block and a half wide.
  if (mMap.block(n.ladderX - 1, n.ladderY0) == Tile::Solid)
    mMap.setBlock(n.ladderX - 1, n.ladderY0, Tile::Platform);
}

void World::resetNoir()
{
  auto& n = mNoir;
  n.alarm = 0;
  n.suspicion = 0;
  n.burstBy = -1;
  n.burstCool = 0;
  n.whistleCool = 0;
  n.goonIn = 0;
  n.rainFrames = 0;
  for (auto& e : mEnemies)
  {
    if (!e.alive)
      continue;
    if (e.kind == EnemyKind::Goon)
    {
      e.alive = false;
      e.hidden = true;
    }
    if (e.kind == EnemyKind::Flatfoot)
      e.aimX = 0;
    if (e.kind == EnemyKind::Crow)
    {
      e.tell = 0;
      e.cool = 0;
    }
  }
  if (n.shutterOpen)
  {
    n.shutterOpen = false;
    for (int by = n.shutterY0; by <= n.shutterY1; ++by)
      mMap.setBlock(n.shutterX, by, Tile::Solid);
  }
  auto& f = mFilm;
  f.slide = 0;
  f.pratfall = 0;
  f.fallTop = -1;
  if (f.gagState == 1)
    layLadderBridge();
}

// --- Saving ------------------------------------------------------------------------------------

bool World::noirCanSave() const
{
  return mNoir.alarm == 0 && mFilm.gagState != 1;
}

std::vector<int> World::noirSave() const
{
  // [alarms, segment alarm, painting open, note read, prize taken, F0 out, coat]
  const auto& n = mNoir;
  return {n.alarms, n.segmentAlarm ? 1 : 0, n.paintOpen ? 1 : 0, n.noteRead ? 1 : 0, n.prizeTaken ? 1 : 0,
    n.f0Out ? 1 : 0, n.coat ? 1 : 0};
}

bool World::validNoirSave(const std::vector<int>& s) const
{
  return s.size() == 7;
}

void World::loadNoir(const std::vector<int>& s)
{
  auto& n = mNoir;
  n.alarms = s[0];
  n.segmentAlarm = s[1] != 0;
  if (s[2] != 0 && !n.paintOpen)
  {
    n.paintOpen = true;
    openSafe();
  }
  n.noteRead = s[3] != 0;
  n.prizeTaken = s[4] != 0;
  if (s[5] != 0 && n.f0 >= 0 && std::size_t(n.f0) < mEnemies.size())
  {
    n.f0Out = true;
    mEnemies[std::size_t(n.f0)].hidden = false;
  }
  n.coat = s[6] != 0;
}

// --- Silent Picture ---------------------------------------------------------------------------

void World::layLadderBridge()
{
  auto& f = mFilm;
  f.gagState = 2;
  for (int bx = f.gagX + 1; bx <= f.gagX + f.gagLen; ++bx)
    if (mMap.block(bx, f.gagY1 + 1) == Tile::Empty)
      mMap.setBlock(bx, f.gagY1 + 1, Tile::Platform);
}

void World::updateFilmExtra(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  const CellBox b = e.box();
  if (!mMap.onSolidGround(b))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, 1);
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return;
  }
  if (e.dive > 0)
  {
    --e.dive; // flat on the floor, seeing stars
    return;
  }
  // A banana peel: down he goes.
  for (const auto& pl : mFilm.peels)
    if (pl.intersects(boxAt(b.x, b.bottom(), b.w, 1)) && e.aimY == 0)
    {
      e.dive = kTrip;
      e.aimY = 1;
      playSound(Sfx::Boing);
      return;
    }
  if (e.timer % std::max(1, def.stepEvery) != 0)
    return;
  const int dx = (p.x + 1) - (b.x + b.w / 2);
  // The runner got past him: he spins round and trips over his own feet.
  if (std::abs(dx) > 2 && sgn(dx) != e.dir && e.aimX == 0)
  {
    e.dir = sgn(dx);
    e.dive = kTrip;
    e.aimX = 20; // won't trip again straight away
    return;
  }
  if (e.aimX > 0)
    --e.aimX;
  if (std::abs(dx) > 1)
  {
    e.dir = sgn(dx);
    if (!stepToward(mMap, e, e.dir))
      e.aimY = 0;
    else if (e.aimY != 0 && e.timer % 16 == 0)
      e.aimY = 0; // off the peel: it can get him again
  }
}

void World::filmShotHit(Enemy& e)
{
  // The hat goes flying.
  if (!mFilm.on || e.variant != 0)
    return;
  e.variant = 1;
  FilmHat h;
  const CellBox b = e.box();
  h.x = float(b.x) + 1.0f;
  h.y = float(b.top()) - 1.0f;
  h.vx = float(-e.dir) * 0.6f;
  h.vy = -1.4f;
  mFilm.hats.push_back(h);
}

bool World::filmHolds() const
{
  return mFilm.on && (mFilm.pratfall > 0 || mFilm.slide > 0 || mFilm.gagState == 1);
}

void World::updateFilm(const PlayerInput& input)
{
  (void)input;
  auto& f = mFilm;
  auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  // Title cards between rooms.
  for (int i = int(f.roomX.size()) - 1; i > f.room; --i)
    if (p.x + 1 >= f.roomX[std::size_t(i)])
    {
      f.room = i;
      if (!f.cards[std::size_t(i)].empty())
      {
        f.card = kCard;
        f.cardText = f.cards[std::size_t(i)];
      }
      break;
    }
  // A banana peel slides the runner 4 blocks.
  if (f.slide > 0)
  {
    const auto r = mMap.moveHorizontally(p.x, p.y, Player::kWidth, p.height(), f.slideDir);
    (void)r;
    if (--f.slide == 0 || p.x == p.prevX)
      f.slide = 0;
  }
  else if (alive && p.state == PlayerState::OnGround)
    for (const auto& pl : f.peels)
      if (pl.intersects(boxAt(p.x, p.y, Player::kWidth, 1)) && p.x != p.prevX)
      {
        f.slide = kPeelSlide;
        f.slideDir = p.x > p.prevX ? 1 : -1;
        playSound(Sfx::Boing);
        break;
      }
  // Pratfalls: a landing from more than 4 blocks bounces the runner.
  if (f.pratfall > 0)
    --f.pratfall;
  if (p.state == PlayerState::OnGround || p.state == PlayerState::Ladder)
  {
    if (f.fallTop >= 0 && p.state == PlayerState::OnGround && p.y - f.fallTop > kPratDrop)
    {
      f.pratfall = kPratfall;
      showMessage("OOF!");
    }
    f.fallTop = -1;
  }
  else if (f.fallTop < 0)
    f.fallTop = p.prevY;
  // The pram: off down the street's steps, again and again.
  if (f.pramX0 >= 0)
  {
    if (!f.pramOn)
    {
      if (++f.pramT >= f.pramEvery && std::abs(p.x - f.pramX0) < 60)
      {
        f.pramOn = true;
        f.pramX = float(f.pramX0);
        f.pramV = 0.5f;
        f.pramT = 0;
      }
    }
    else
    {
      f.pramV = std::min(1.5f, f.pramV + 0.04f);
      f.pramX += f.pramV;
      // Its wheels follow the ground down the steps.
      const int px = int(f.pramX);
      int y = f.pramY;
      while (y < mMap.height() - 1 && !mMap.solidTop(px + 1, y + 1))
        ++y;
      while (y > 0 && mMap.solid(px + 1, y))
        --y;
      f.pramY = y;
      const CellBox pram = boxAt(px, f.pramY, 3, 3);
      if (alive && pram.intersects(p.hitBox()))
        hurtPlayer(1);
      if (px >= f.pramX1)
        f.pramOn = false;
    }
  }
  // The ladder gag: at the top it tips over the gap, runner and all.
  if (f.gagState == 0 && f.gagX >= 0 && p.state == PlayerState::Ladder && p.x / kCellsPerTile <= f.gagX &&
      (p.x + 2) / kCellsPerTile >= f.gagX && p.y - 4 <= f.gagY0 * kCellsPerTile + 2)
  {
    f.gagState = 1;
    f.gagT = 0;
    for (int by = f.gagY0; by <= f.gagY1; ++by)
      mMap.setBlock(f.gagX, by, Tile::Empty);
    playSound(Sfx::Creak);
    showMessage("TIMBER!");
  }
  if (f.gagState == 1)
  {
    ++f.gagT;
    const float a = std::min(1.0f, float(f.gagT) / float(kGagTip)) * kPi * 0.5f;
    const float len = float((f.gagY1 - f.gagY0 + 1) * kCellsPerTile);
    const float pivotX = float(f.gagX * kCellsPerTile), pivotY = float((f.gagY1 + 1) * kCellsPerTile);
    p.x = int(pivotX + len * std::sin(a)) - 1;
    p.y = int(pivotY - len * std::cos(a)) - 1;
    p.state = PlayerState::Falling;
    if (f.gagT >= kGagTip)
    {
      layLadderBridge();
      // Off the end onto the far roof, flat on the back.
      p.x = (f.gagX + f.gagLen + 1) * kCellsPerTile;
      p.y = (f.gagY1 + 1) * kCellsPerTile - 1;
      p.state = PlayerState::OnGround;
      f.pratfall = kPratfall;
      f.fallTop = -1;
      playSound(Sfx::Thud);
    }
  }
  // Hats in the air.
  for (auto& h : f.hats)
  {
    h.x += h.vx;
    h.y += h.vy;
    h.vy = std::min(1.5f, h.vy + 0.12f);
    ++h.t;
  }
  f.hats.erase(std::remove_if(f.hats.begin(), f.hats.end(), [](const FilmHat& h) { return h.t > 40; }), f.hats.end());
}

} // namespace gr
