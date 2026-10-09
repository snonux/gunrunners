// Level 6, Maglev Express (SPEC 06): Clearance. The map never moves; the
// backdrop rushes past (trainscroll) while gantries sweep over the train
// from the front: crouch under the low ones, jump the tall ones, take a
// hatch for the rest. The tunnel's mouth and rings clear the whole roof;
// a passing train overhead carries the candid camera and the bonus patch.
// Track Hoppers leap car to car, Rail Drones drop caltrops and Decouplers
// unhook the rear cars. The Arc Caster's lightning jumps between targets.
// Also the Light Trail bonus's rule.

#include "game/world.hpp"

#include "assets/enemy_art.hpp"

#include <algorithm>
#include <cmath>

namespace gr
{

namespace
{

constexpr Color kInk = rgb(10, 8, 20);
constexpr float kCellPx = float(kCellSize) * kPixelScale;
constexpr float kTilePx = float(kTileSize) * kPixelScale;
constexpr int kBrakeFrames = 90;
constexpr int kPryFrames = 8;
constexpr int kHopHeight = 8;     // cells: a Track Hopper's arc
constexpr int kArcReach = 16;     // cells: the Arc Caster's first jump
constexpr int kArcChain = 10;     // cells: from target to target
constexpr int kArcStun = 4;
constexpr int kTrailLife = 45;

int sgn(int v) { return (v > 0) - (v < 0); }

Color gantryColor(GantryKind k)
{
  switch (k)
  {
    case GantryKind::Low:
      return rgb(80, 170, 255);
    case GantryKind::Tall:
      return rgb(255, 180, 40);
    case GantryKind::Tall4:
      return rgb(240, 240, 255);
    case GantryKind::Mouth:
    case GantryKind::Ring:
      return rgb(255, 60, 50);
  }
  return rgb(255, 255, 255);
}

const char* gantryAdvice(GantryKind k)
{
  switch (k)
  {
    case GantryKind::Low:
      return "LOW GANTRY - CROUCH!";
    case GantryKind::Tall:
      return "BARRIER - JUMP!";
    case GantryKind::Tall4:
      return "TALL GANTRY - JUMP HIGH OR TAKE A HATCH!";
    case GantryKind::Mouth:
      return "TUNNEL - GET INSIDE!";
    case GantryKind::Ring:
      return "RING - STAY INSIDE!";
  }
  return "";
}

// Frames until the band reaches box b: 0 while over it, -1 once past.
int etaFor(const Gantry& g, const CellBox& b)
{
  if (g.x < 0)
    return -1;
  if (g.x > b.right())
    return (g.x - b.right() + g.speed - 1) / g.speed;
  if (g.x + 1 >= b.left())
    return 0;
  return -1;
}

} // namespace

// --- Level entities -------------------------------------------------------------

bool World::setupMaglevEntity(const EntityDef& e)
{
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  const bool hasRect = e.rect("rect", x0, y0, x1, y1);
  auto platformIndex = [&](const std::string& id) {
    for (std::size_t i = 0; i < mPlatforms.size(); ++i)
      if (mPlatforms[i].id == id)
        return int(i);
    return -1;
  };
  if (e.kind == "trainscroll")
  {
    mTrain = true;
    const auto sp = e.list("speeds");
    for (std::size_t i = 0; i < sp.size() && i < mScrollSpeeds.size(); ++i)
      mScrollSpeeds[i] = sp[i];
    return true;
  }
  if (e.kind == "gantry")
  {
    Gantry g;
    g.id = e.id;
    const std::string k = e.str("kind", "low");
    g.kind = k == "tall"    ? GantryKind::Tall
           : k == "tall4" ? GantryKind::Tall4
           : k == "mouth" ? GantryKind::Mouth
           : k == "ring"  ? GantryKind::Ring
                          : GantryKind::Low;
    g.trigger = e.num("trigger", 0) * kCellsPerTile;
    g.speed = std::max(1, e.num("speed", 3));
    g.warn = std::max(8, e.num("warn", 30));
    mGantries.push_back(g);
    mLevelGantries = mGantries.size();
    return true;
  }
  if (e.kind == "tunnel" && hasRect)
  {
    mTunnelX0 = x0 * kCellsPerTile;
    mTunnelX1 = (x1 + 1) * kCellsPerTile - 1;
    mTunnelRing = std::max(30, e.num("ring", 90));
    return true;
  }
  if (e.kind == "passgap" && e.hasPos)
  {
    mGapX = e.x * kCellsPerTile;
    mGapFrames = std::max(1, e.num("frames", 160));
    mPasser = platformIndex(e.str("platform"));
    return true;
  }
  if (e.kind == "bonuspatch")
  {
    // A bonus entrance riding a platform; there once the camera is shot.
    Prop pr;
    pr.kind = PropKind::BonusDoor;
    pr.w = 4;
    pr.h = 6;
    pr.ride = platformIndex(e.str("on"));
    pr.rideDx = e.num("dx", 2) * kCellsPerTile;
    pr.rideDy = -pr.h;
    pr.dormant = true;
    if (pr.ride >= 0)
    {
      const Platform& pl = mPlatforms[std::size_t(pr.ride)];
      pr.x = pl.x + pr.rideDx;
      pr.y = pl.y + pr.rideDy;
    }
    mProps.push_back(pr);
    return true;
  }
  if (e.kind == "arrival" && e.hasPos)
  {
    mBrakeX = e.x * kCellsPerTile;
    const std::string layer = e.str("layer");
    for (std::size_t i = 0; i < mLayers.size(); ++i)
      if (mLayers[i].id == layer)
        mStationLayer = int(i);
    return true;
  }
  return false;
}

void World::setupMaglevEnemy(Enemy& en, const EntityDef& e)
{
  if (e.has("ride"))
  {
    // The camera on the passing train's roof: hidden with it.
    for (std::size_t i = 0; i < mPlatforms.size(); ++i)
      if (mPlatforms[i].id == e.str("ride"))
      {
        en.platform = int(i);
        en.aimX = en.x - mPlatforms[i].x;
        if (mPlatforms[i].hidden)
          en.y = en.prevY = -8;
      }
  }
  if (en.kind == EnemyKind::Decoupler)
  {
    // Asleep down in its coupling, under the roofs.
    en.railX0 = en.x;
    en.railX1 = en.y;     // where it stands once it has climbed out
    en.aimY = en.y + 8;   // where it hides
    en.y = en.prevY = en.aimY;
    en.attach = 0;
  }
  if (en.kind == EnemyKind::RailDrone)
    en.aimY = en.y; // its cruising height
}

// --- Queries ----------------------------------------------------------------------

float World::trainSpeed() const
{
  if (mBrakeAt < 0)
    return 1.0f;
  return std::max(0.0f, 1.0f - float(mStats.frames - mBrakeAt) / float(kBrakeFrames));
}

int World::roofTopBelow(int cx, int cy) const
{
  for (int y = std::max(0, cy); y < mMap.height(); ++y)
    if (mMap.solidTop(cx, y))
      return y;
  return -1;
}

bool World::sameRoof(const CellBox& a, const CellBox& b) const
{
  // Both standing on one unbroken roof: the rails conduct.
  if (a.bottom() != b.bottom())
    return false;
  const int y = a.bottom() + 1;
  for (int x = std::min(a.left(), b.left()); x <= std::max(a.right(), b.right()); ++x)
    if (!mMap.solid(x, y))
      return false;
  return true;
}

bool World::trainDanger() const
{
  const auto& p = mPlayer;
  if (p.state == PlayerState::Dying || p.state == PlayerState::Teleporting)
    return false;
  const CellBox stand = boxAt(p.x, p.y, Player::kWidth, Player::kHeight);
  for (const auto& g : mGantries)
  {
    const int eta = etaFor(g, stand);
    if (eta < 0)
      continue;
    const int window = (g.kind == GantryKind::Low || g.kind == GantryKind::Tall) ? 12 : 40;
    if (eta <= window && stand.top() <= g.bandBottom() && stand.bottom() >= g.bandTop())
      return true;
  }
  for (const auto& e : mEnemies)
    if (e.alive && e.kind == EnemyKind::Decoupler && (e.attach == 1 || e.attach == 2) &&
        mStats.frames - e.lastDive >= enemyDef(e.def).cooldown - 30 && p.x + 1 < e.x)
      return true;
  // In the tunnel, on the roof, with a ring about to come.
  if (mTunnelState == 1 && stand.top() < 24 && mStats.frames >= mGapUntil && mTunnelNext - mStats.frames < 12)
    return true;
  return false;
}

bool World::trainBusy() const
{
  if (mBrakeAt >= 0 && mStats.frames - mBrakeAt <= kBrakeFrames)
    return true;
  if (mTunnelState == 1)
    return true;
  for (const auto& g : mGantries)
    if (g.x >= 0)
      return true;
  for (const auto& e : mEnemies)
    if (e.alive && e.kind == EnemyKind::Decoupler && (e.attach == 1 || e.attach == 2))
      return true;
  return false;
}

// --- Update -----------------------------------------------------------------------

void World::fireGantry(Gantry& g)
{
  // It comes in from far enough ahead to give `warn` frames of lights.
  g.fired = true;
  g.x = g.prevX = mPlayer.x + Player::kWidth + g.speed * (g.warn + 14);
}

void World::updateGantries()
{
  auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  bool active = false;
  for (const auto& g : mGantries)
    active = active || g.x >= 0;
  // One on screen at a time: a gantry whose trigger is passed waits for
  // the one before it.
  for (std::size_t i = 0; i < mLevelGantries; ++i)
  {
    Gantry& g = mGantries[i];
    if (!g.fired && !active && alive && p.x >= g.trigger)
    {
      fireGantry(g);
      active = true;
    }
  }
  const CellBox pb = p.box();
  for (auto& g : mGantries)
  {
    if (g.x < 0)
      continue;
    g.prevX = g.x;
    g.x -= g.speed;
    const int eta = etaFor(g, pb);
    if (alive && (eta == g.warn || (eta > 0 && eta <= 8 && eta % 3 == 0)))
      playSound(Sfx::Warn);
    // Anything in the band as it sweeps past is knocked off the train.
    const int sx0 = g.x, sx1 = g.prevX + 1;
    if (alive && pb.right() >= sx0 && pb.left() <= sx1 && pb.top() <= g.bandBottom() && pb.bottom() >= g.bandTop())
    {
      playSound(Sfx::Hit);
      showMessage(g.kind == GantryKind::Mouth || g.kind == GantryKind::Ring ? "SMACK! THE TUNNEL WINS"
                                                                            : "KNOCKED OFF THE TRAIN");
      killPlayer();
    }
    if (g.x + 2 < mCamera.x() - 8 && g.x < p.x - 40)
      g.x = -1;
  }
  // Tunnel rings are made on the fly; drop them once they are past.
  for (std::size_t i = mGantries.size(); i-- > mLevelGantries;)
    if (mGantries[i].x < 0)
      mGantries.erase(mGantries.begin() + std::ptrdiff_t(i));
}

void World::updateTunnel()
{
  if (mTunnelX0 < 0)
    return;
  auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  const int clock = mStats.frames;
  auto spawn = [&](GantryKind kind) {
    Gantry g;
    g.kind = kind;
    fireGantry(g);
    mGantries.push_back(g);
  };
  if (mTunnelState == 0 && alive && p.x + 1 >= mTunnelX0)
  {
    mTunnelState = 1;
    spawn(GantryKind::Mouth);
    mTunnelNext = clock + mTunnelRing;
    showMessage("TUNNEL AHEAD - GET INSIDE A CAR!");
  }
  else if (mTunnelState == 1)
  {
    if (p.x > mTunnelX1 && alive)
    {
      mTunnelState = 2;
      mTunnelExitX = p.x + Player::kWidth + 3 * 44;
    }
    else
    {
      if (!mGapDone && mGapX >= 0 && alive && p.x >= mGapX && mPasser >= 0)
      {
        // A gap in the tunnel roof, and a train going the other way.
        mGapDone = true;
        mGapUntil = clock + mGapFrames;
        Platform& pl = mPlatforms[std::size_t(mPasser)];
        pl.running = true;
        pl.hidden = false;
        showMessage("A TRAIN PASSES OVERHEAD");
        playSound(Sfx::Chime);
      }
      if (clock < mGapUntil)
        mTunnelNext = std::max(mTunnelNext, mGapUntil + 20); // rings pause
      else if (clock >= mTunnelNext && alive)
      {
        spawn(GantryKind::Ring);
        mTunnelNext = clock + mTunnelRing;
      }
    }
  }
  for (const auto& g : mGantries)
    if (g.kind == GantryKind::Mouth && g.x >= 0)
      mTunnelMouthX = g.x;
  if (mTunnelState == 2 && mTunnelExitX > -1000)
    mTunnelExitX -= 3;
}

void World::updateCaltrops()
{
  auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  for (auto& c : mCaltrops)
  {
    c.prevX = c.x;
    c.prevY = c.y;
    if (!c.landed)
    {
      for (int s = 0; s < 2 && !c.landed; ++s)
      {
        if (mMap.solidTop(c.x, c.y + 1) || mMap.solidTop(c.x + 1, c.y + 1))
          c.landed = true;
        else
          ++c.y;
      }
      if (c.y > mMap.height() + 2)
        c.life = 0;
    }
    else
    {
      --c.life;
      // The wind of the train pushes the plain ones back; off a car's end
      // they are gone.
      if (!c.green && ++c.slide % 4 == 0)
        --c.x;
      if (!mMap.solidTop(c.x, c.y + 1) && !mMap.solidTop(c.x + 1, c.y + 1))
        c.landed = false;
    }
    if (alive && c.life > 0 && c.box().intersects(p.hitBox()))
    {
      if (c.green)
      {
        if (p.turbo == 0)
          infect();
      }
      else
      {
        hurtPlayer(1);
      }
      c.life = 0;
      burst(cellCenter(c.box()), c.green ? rgb(120, 255, 80) : rgb(220, 220, 230), rgb(255, 255, 255), 6, 1.2f);
    }
  }
  mCaltrops.erase(std::remove_if(mCaltrops.begin(), mCaltrops.end(), [](const Caltrop& c) { return c.life <= 0; }),
    mCaltrops.end());

  for (auto& w : mWaves)
  {
    for (int s = 0; s < 2 && w.left > 0; ++s)
    {
      w.x += w.dir;
      --w.left;
      if (!mMap.solidTop(w.x, w.y + 1))
        w.left = 0; // ran off the end of the roof
      if (alive && w.y == p.y && w.x >= p.x && w.x <= p.x + Player::kWidth - 1 &&
          (p.state == PlayerState::OnGround || p.state == PlayerState::Recovering))
        hurtPlayer(1);
    }
  }
  mWaves.erase(std::remove_if(mWaves.begin(), mWaves.end(), [](const Shockwave& w) { return w.left <= 0; }),
    mWaves.end());
}

void World::updateLightTrail(const PlayerInput& input)
{
  auto& p = mPlayer;
  for (auto& t : mTrailBlocks)
    --t.life;
  mTrailBlocks.erase(std::remove_if(mTrailBlocks.begin(), mTrailBlocks.end(), [](const TrailBlock& t) { return t.life <= 0; }),
    mTrailBlocks.end());
  // In the air, the runner's feet draw a one-way bridge; holding down
  // lets you sink through it.
  const bool air = p.state == PlayerState::Jumping || p.state == PlayerState::Falling;
  if (air && !input.down && p.y + 1 < mMap.height())
  {
    bool fresh = true;
    for (auto& t : mTrailBlocks)
      if (t.x == p.x && t.y == p.y + 1)
      {
        t.life = kTrailLife;
        fresh = false;
      }
    if (fresh)
      mTrailBlocks.push_back({p.x, p.y + 1, kTrailLife});
  }
  syncFloats();
}

void World::resetTrain()
{
  for (auto& g : mGantries)
    g.x = -1;
  mGantries.resize(mLevelGantries);
  mCaltrops.clear();
  mWaves.clear();
  for (auto& e : mEnemies)
  {
    if (e.kind != EnemyKind::Decoupler)
      continue;
    if (!e.alive && e.variant == 1)
    {
      // It took the rear cars with you on them: it is back in its coupling.
      e.alive = true;
      e.hp = enemyDef(e.def).hp;
      e.variant = 0;
      e.attach = 0;
    }
    if (e.alive && (e.attach == 1 || e.attach == 2))
      e.attach = 0;
    if (e.alive && e.attach == 0)
    {
      e.x = e.railX0;
      e.y = e.aimY;
      e.drawSnap = true;
    }
  }
  if (mTunnelState == 1)
    mTunnelNext = mStats.frames + mTunnelRing;
}

void World::updateMaglev(const PlayerInput& input)
{
  for (auto& a : mArcs)
    --a.life;
  mArcs.erase(std::remove_if(mArcs.begin(), mArcs.end(), [](const ArcBolt& a) { return a.life <= 0; }), mArcs.end());
  if (mLightTrail)
    updateLightTrail(input);
  if (!mTrain)
    return;
  auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  const int clock = mStats.frames;
  mScroll += trainSpeed();

  // Arrival: the train brakes and the station's platform slides in.
  if (mBrakeX >= 0 && mBrakeAt < 0 && alive && p.x >= mBrakeX)
  {
    mBrakeAt = clock;
    playSound(Sfx::Chime);
    showMessage("NOW ARRIVING: HALCYON CENTRAL");
  }
  if (mBrakeAt >= 0 && clock - mBrakeAt >= kBrakeFrames && mStationLayer >= 0)
  {
    Layer& l = mLayers[std::size_t(mStationLayer)];
    if (!l.scriptSolid)
    {
      l.scriptSolid = true;
      playSound(Sfx::Chime);
      showMessage("MIND THE GAP");
    }
  }

  updateGantries();
  updateTunnel();

  // What rides the passing train: the camera and the bonus patch.
  for (auto& e : mEnemies)
  {
    if (!e.alive || e.kind != EnemyKind::Camera || e.platform < 0 || e.platform >= int(mPlatforms.size()))
      continue;
    const Platform& pl = mPlatforms[std::size_t(e.platform)];
    const int ny = pl.hidden ? -8 : pl.y - 1;
    if (ny != e.y)
      e.drawSnap = true;
    e.x = pl.x + e.aimX;
    e.y = ny;
  }
  for (auto& pr : mProps)
  {
    if (pr.ride < 0 || pr.ride >= int(mPlatforms.size()))
      continue;
    const Platform& pl = mPlatforms[std::size_t(pr.ride)];
    pr.x = pl.x + pr.rideDx;
    pr.y = pl.y + pr.rideDy;
    pr.dormant = pl.hidden || !mStats.camera;
  }

  updateCaltrops();

  // The dining car's loose floor panel: hold down on it to pry it up.
  bool prying = false;
  if (alive && p.state == PlayerState::OnGround && input.down)
    for (const auto& b : mBreakables)
    {
      if (b.broken || b.by != 4)
        continue;
      if (p.y + 1 == b.y0 * kCellsPerTile && p.x + Player::kWidth - 1 >= b.x0 * kCellsPerTile &&
          p.x <= (b.x1 + 1) * kCellsPerTile - 1)
      {
        prying = true;
        if (++mPry >= kPryFrames)
        {
          const CellBox area{b.x0 * kCellsPerTile, b.y0 * kCellsPerTile, (b.x1 - b.x0 + 1) * kCellsPerTile, kCellsPerTile};
          hitBreakable(area, 99, 4);
          showMessage("A SERVICE HATCH! THERE IS A CRAWLSPACE DOWN THERE");
          mPry = 0;
        }
        break;
      }
    }
  if (!prying)
    mPry = 0;
}

// --- Enemies ----------------------------------------------------------------------

void World::updateHopper(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  switch (e.attach)
  {
    case 0: // on a roof, getting its breath back
    {
      if (!mMap.onSolidGround(e.box()))
      {
        mMap.moveVertically(e.x, e.y, e.w, e.h, 2);
        if (e.y > mMap.height() + 4)
          e.alive = false;
        break;
      }
      e.dir = p.x < e.x ? -1 : 1;
      const int dx = p.x - e.x;
      if (e.timer < def.cooldown || !alive || std::abs(dx) > 48 || std::abs(p.y - e.y) > 16 || std::abs(dx) < 4)
        break;
      // Toward the runner's car: at most a car's length, onto a roof.
      const int want = e.x + std::clamp(dx, -30, 30);
      int tx = -1, ty = -1;
      for (int k = 0; k < 40 && ty < 0; ++k)
      {
        const int cx = want - sgn(dx) * k;
        if (cx == e.x)
          break;
        const int top = roofTopBelow(cx + 1, std::max(0, e.y - 12));
        if (top < 0 || !mMap.solidTop(cx, top) || !mMap.solidTop(cx + e.w - 1, top))
          continue;
        if (mMap.overlapsSolid(boxAt(cx, top - 1, e.w, e.h)))
          continue;
        tx = cx;
        ty = top - 1;
      }
      if (ty < 0)
        break;
      e.aimX = tx;
      e.aimY = ty;
      e.railX0 = e.x;
      e.railX1 = e.y;
      e.dive = std::max(6, std::abs(tx - e.x) / def.stepEvery + 2); // frames in the air
      e.lastDive = 0;
      e.tell = def.tell; // crouches first
      e.attach = 1;
      break;
    }
    case 1:
      if (--e.tell <= 0)
      {
        e.attach = 2;
        e.lastDive = 0;
        if (isOnScreen(e.box(), 4))
          playSound(Sfx::Jump);
      }
      break;
    case 2: // in the air
    {
      const int T = std::max(1, e.dive);
      const int t = ++e.lastDive;
      const float u = std::min(1.0f, float(t) / float(T));
      e.x = e.railX0 + int(std::lround(float(e.aimX - e.railX0) * u));
      e.y = e.railX1 + int(std::lround(float(e.aimY - e.railX1) * u - 4.0f * float(kHopHeight) * u * (1.0f - u)));
      if (t >= T)
      {
        e.x = e.aimX;
        e.y = e.aimY;
        e.attach = 0;
        e.timer = 0;
        // The landing sends a ripple along the roof both ways.
        mWaves.push_back({e.x - 1, e.y, -1, def.range});
        mWaves.push_back({e.x + e.w, e.y, 1, def.range});
        if (isOnScreen(e.box(), 4))
        {
          playSound(Sfx::SmallExplosion);
          mCamera.shake(4, 1.0f);
        }
      }
      break;
    }
    default:
      e.attach = 0;
      break;
  }
}

void World::updateRailDrone(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  // Hovers alongside the train a little ahead of the runner, queueing up
  // behind any other drone already there instead of stacking on it.
  const int ahead = p.facing > 0 ? 1 : -1;
  int tx = p.x + 4 * ahead;
  for (const auto& o : mEnemies)
  {
    if (&o == &e)
      break;
    if (o.alive && o.kind == EnemyKind::RailDrone && std::abs(o.x - tx) < 6)
      tx = o.x - 6 * ahead;
  }
  if (e.timer % std::max(1, def.stepEvery) == 0 && tx != e.x)
    e.x += sgn(tx - e.x);
  e.dir = p.x < e.x ? -1 : 1;
  e.y = e.aimY + ((e.timer / 8) % 2);
  if (e.tell > 0)
  {
    if (--e.tell == 0)
    {
      Caltrop c;
      c.x = c.prevX = e.x + 1;
      c.y = c.prevY = e.y + 1;
      c.green = e.carrier;
      mCaltrops.push_back(c);
      if (isOnScreen(e.box(), 2))
        playSound(Sfx::Clunk);
    }
    return;
  }
  if (!alive || e.timer % std::max(1, def.cooldown) != 0 || std::abs(p.x - e.x) > 12)
    return;
  if (e.carrier)
  {
    // Green caltrops only over the middle of a car, well clear of the gaps.
    const int cx = e.x + e.w / 2;
    const int top = roofTopBelow(cx, e.y + 1);
    if (top < 0)
      return;
    int x0 = cx;
    while (x0 > 0 && mMap.solid(x0 - 1, top))
      --x0;
    const int block = (cx - x0) / kCellsPerTile;
    if (block < 10 || block > 16)
      return;
  }
  e.tell = def.tell; // the bay opens
}

void World::updateDecoupler(Enemy& e, const EnemyDef& def)
{
  auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  const int clock = mStats.frames;
  switch (e.attach)
  {
    case 0: // asleep in its coupling until a runner comes up behind it
      e.y = e.aimY;
      if (alive && p.x + Player::kWidth <= e.x && p.x + Player::kWidth > e.x - def.range && std::abs(p.y - e.railX1) < 20)
      {
        e.attach = 1;
        e.tell = def.tell;
        e.lastDive = clock;
        playSound(Sfx::Clunk);
        showMessage("A DECOUPLER! GET ACROSS BEFORE IT UNHOOKS THE CAR");
      }
      break;
    case 1: // climbing out
      --e.tell;
      e.y = e.railX1 + (e.aimY - e.railX1) * std::max(0, e.tell) / std::max(1, def.tell);
      if (e.tell <= 0)
        e.attach = 2;
      break;
    case 2: // working the coupling: the lamp counts down
      e.dir = p.x < e.x ? -1 : 1;
      if (clock - e.lastDive >= def.cooldown)
      {
        playSound(Sfx::Clunk);
        mCamera.shake(6, 1.5f);
        if (alive && p.x + 1 < e.x)
        {
          showMessage("UNCOUPLED! THE REAR CARS DRIFT AWAY...");
          killPlayer();
          e.variant = 1; // back in its coupling when you are
        }
        else
        {
          showMessage("THE REAR CARS DRIFT AWAY");
        }
        e.attach = 3;
        e.alive = false; // it rides off with them
      }
      break;
    default:
      break;
  }
}

// The Arc Caster: lightning to the nearest enemy ahead, then on to up to
// three more close by (or anywhere on the same roof: the rails conduct).
void World::fireArc(int ox, int oy, int dir, int damage)
{
  auto clearLine = [&](int x0, int y0, int x1, int y1) {
    const int steps = std::max(std::abs(x1 - x0), std::abs(y1 - y0));
    for (int i = 1; i < steps; ++i)
      if (mMap.solid(x0 + (x1 - x0) * i / steps, y0 + (y1 - y0) * i / steps))
        return false;
    return true;
  };
  auto hidden = [](const Enemy& e) { return e.kind == EnemyKind::Decoupler && e.attach == 0; };
  std::vector<std::size_t> hits;
  int best = -1, bestD = 1 << 30;
  for (std::size_t i = 0; i < mEnemies.size(); ++i)
  {
    const Enemy& e = mEnemies[i];
    if (!e.alive || e.trapped || (!e.active && !hidden(e)) || e.y < 0)
      continue;
    const CellBox b = e.box();
    const int d = dir > 0 ? b.left() - ox : ox - b.right();
    if (d < -2 || d > kArcReach)
      continue;
    const int slack = hidden(e) ? 12 : 4;
    if (oy < b.top() - slack || oy > b.bottom() + slack)
      continue;
    // Down into a coupling the lightning finds its way; elsewhere it needs
    // a clear line.
    if (!hidden(e) && !clearLine(ox, oy, b.x + b.w / 2, std::clamp(oy, b.top(), b.bottom())))
      continue;
    if (d < bestD)
    {
      bestD = d;
      best = int(i);
    }
  }
  Vec2 from{float(ox), float(oy)};
  auto bolt = [&](Vec2 a, Vec2 b) {
    ArcBolt bt;
    bt.x0 = a.x;
    bt.y0 = a.y;
    bt.x1 = b.x;
    bt.y1 = b.y;
    bt.seed = int(mLogicRng.next() % 997u);
    mArcs.push_back(bt);
  };
  auto mid = [](const CellBox& b) { return Vec2{float(b.x) + float(b.w) * 0.5f, float(b.y) + float(b.h) * 0.5f}; };
  if (best < 0)
  {
    // Nothing to jump to: a short crackle that still breaks boxes.
    int ex = ox;
    for (int i = 0; i < kArcReach / 2; ++i)
    {
      if (mMap.solid(ex + dir, oy))
        break;
      ex += dir;
    }
    bolt(from, {float(ex), float(oy)});
    for (auto& bx : mBoxes)
      if (bx.alive && bx.box().intersects({std::min(ox, ex), oy - 1, std::abs(ex - ox) + 1, 3}))
      {
        destroyBox(bx);
        break;
      }
    hitBreakable({ex + dir, oy, 1, 1}, damage, 0);
    playSound(Sfx::Zap);
    return;
  }
  hits.push_back(std::size_t(best));
  for (std::size_t k = 0; k < hits.size() && hits.size() < 4; ++k)
  {
    const CellBox hb = mEnemies[hits[k]].box();
    int next = -1, nextD = 1 << 30;
    for (std::size_t i = 0; i < mEnemies.size(); ++i)
    {
      const Enemy& e = mEnemies[i];
      if (!e.alive || e.trapped || (!e.active && !hidden(e)) || e.y < 0 ||
          std::find(hits.begin(), hits.end(), i) != hits.end())
        continue;
      const CellBox b = e.box();
      const Vec2 a = mid(hb), c = mid(b);
      const int d = int(std::max(std::abs(a.x - c.x), std::abs(a.y - c.y)));
      if (d > kArcChain && !sameRoof(hb, b))
        continue;
      if (d < nextD)
      {
        nextD = d;
        next = int(i);
      }
    }
    if (next >= 0)
      hits.push_back(std::size_t(next));
  }
  Vec2 at = from;
  for (std::size_t i : hits)
  {
    Enemy& e = mEnemies[i];
    const Vec2 c = mid(e.box());
    bolt(at, c);
    at = c;
    e.stun = std::max(e.stun, kArcStun);
    burst({c.x * kCellSize, c.y * kCellSize}, rgb(170, 210, 255), rgb(255, 255, 255), 8, 1.6f);
    damageEnemy(e, damage);
    if (!e.alive)
      ++mStats.protoKills;
  }
  playSound(Sfx::Zap);
}

// --- Drawing ----------------------------------------------------------------------

void World::drawTrainProp(Renderer& r, const Prop& pr, float x, float y, float w, float h, int frame, bool foreground) const
{
  switch (pr.kind)
  {
    case PropKind::CarInterior:
      if (!foreground)
      {
        // The cabin's back wall: panels, a lamp strip and windows with the
        // city's lights streaming past.
        r.fillRect(x, y, w, h, rgb(36, 40, 58));
        r.fillRect(x, y, w, 10.0f, rgb(220, 230, 255));
        r.fillRect(x, y + 10.0f, w, 6.0f, rgba(220, 230, 255, 60));
        const float shift = std::fmod(mScroll * float(mScrollSpeeds[2]) * 0.5f, 97.0f);
        for (float wx = x + 40.0f; wx + 120.0f < x + w; wx += 192.0f)
        {
          const float wy = y + 50.0f, ww = 120.0f, wh = std::min(110.0f, h - 120.0f);
          r.fillRect(wx - 6.0f, wy - 6.0f, ww + 12.0f, wh + 12.0f, rgb(70, 76, 100));
          r.fillRect(wx, wy, ww, wh, rgb(16, 14, 36));
          for (int i = 0; i < 6; ++i)
          {
            const float lx = std::fmod(float(hash2(i, int(wx)) % 400u) - shift * float(1 + i % 3) + 800.0f, 160.0f) - 20.0f;
            if (lx > 0.0f && lx < ww - 30.0f)
              r.fillRect(wx + lx, wy + 12.0f + float(i * 15 % 80), 30.0f, 3.0f,
                i % 2 ? rgba(255, 90, 200, 160) : rgba(90, 220, 255, 140));
          }
          r.fillRect(wx, wy, ww, 8.0f, rgba(255, 255, 255, 30));
        }
        // A red stripe along the bottom of the wall.
        r.fillRect(x, y + h - 26.0f, w, 6.0f, rgb(220, 40, 60));
      }
      break;
    case PropKind::Seat:
      if (foreground)
      {
        // A blue bench with a high back, over its solid block.
        r.fillRect(x + 6.0f, y - 30.0f, 14.0f, h + 30.0f, rgb(40, 70, 150));
        r.fillRect(x + 6.0f, y + 4.0f, w - 12.0f, h * 0.45f, rgb(60, 100, 190));
        r.fillRect(x + 6.0f, y + 4.0f, w - 12.0f, 6.0f, rgb(120, 160, 230));
        r.fillRect(x + 14.0f, y + h * 0.45f + 4.0f, 8.0f, h * 0.55f - 4.0f, rgb(90, 96, 110));
        r.fillRect(x + w - 22.0f, y + h * 0.45f + 4.0f, 8.0f, h * 0.55f - 4.0f, rgb(90, 96, 110));
      }
      break;
    case PropKind::Crate:
      if (foreground)
      {
        r.fillRect(x + 2.0f, y + 2.0f, w - 4.0f, h - 4.0f, rgb(150, 110, 60));
        r.fillRect(x + 2.0f, y + 2.0f, w - 4.0f, 8.0f, rgb(190, 150, 90));
        r.drawLine(x + 6.0f, y + 6.0f, x + w - 6.0f, y + h - 6.0f, 6.0f, rgb(110, 76, 40));
        r.drawLine(x + w - 6.0f, y + 6.0f, x + 6.0f, y + h - 6.0f, 6.0f, rgb(110, 76, 40));
        r.drawText("FRAGILE", x + w * 0.5f, y + h * 0.5f - 10.0f, {16.0f, rgb(230, 60, 50), rgb(40, 20, 10)}, Align::Center);
      }
      break;
    case PropKind::Sleeper:
      if (!foreground)
      {
        // A commuter asleep in his seat, newspaper over his face.
        const float bx = x + w * 0.5f, by = y + h;
        r.fillRect(bx - 34.0f, by - 70.0f, 68.0f, 70.0f, rgb(60, 66, 96));
        r.fillRect(bx - 24.0f, by - 104.0f, 48.0f, 38.0f, rgb(230, 190, 160));
        r.fillRect(bx - 30.0f, by - 96.0f, 60.0f, 30.0f, rgb(235, 232, 220));
        r.fillRect(bx - 24.0f, by - 88.0f, 40.0f, 3.0f, rgb(80, 80, 90));
        r.fillRect(bx - 24.0f, by - 80.0f, 34.0f, 3.0f, rgb(80, 80, 90));
        const float zt = std::fmod(float(frame) * 0.5f, 60.0f);
        r.drawText("z", bx + 30.0f + zt * 0.4f, by - 120.0f - zt, {22.0f, rgba(255, 255, 255, int(220 - zt * 3)), kInk},
          Align::Center);
      }
      break;
    case PropKind::HiScore:
      if (!foreground)
      {
        // Through the window: the Halcyon tower's billboard.
        r.fillRect(x, y, w, h, rgb(20, 10, 40));
        r.fillRect(x + 4.0f, y + 4.0f, w - 8.0f, h - 8.0f, rgb(40, 16, 70));
        r.drawText("HI-SCORE", x + w * 0.5f, y + 10.0f, {20.0f, rgb(255, 80, 200), kInk, true}, Align::Center);
        if (mHiScore >= 0)
          r.drawText(std::to_string(mHiScore), x + w * 0.5f, y + h * 0.5f, {24.0f, rgb(120, 240, 255), kInk, true},
            Align::Center);
      }
      break;
    default:
      break;
  }
}

void World::drawMaglevBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)frame;
  // The light trail under the runner's feet.
  for (const auto& t : mTrailBlocks)
  {
    const float x = float(t.x) * kCellPx - camX, y = float(t.y) * kCellPx - camY;
    const float a = std::min(1.0f, float(t.life) / 15.0f);
    drawGlow(r, mArt, x + 48.0f, y + 16.0f, 50, rgb(0, 240, 255), 0.25f * a);
    r.fillRect(x, y, 3.0f * kCellPx, kCellPx, withAlpha(rgb(0, 220, 255), int(150 * a)), Blend::Add);
    r.fillRect(x, y, 3.0f * kCellPx, 5.0f, withAlpha(rgb(220, 255, 255), int(230 * a)));
  }
  if (!mTrain)
    return;
  const float scroll = mScroll + trainSpeed() * alpha;
  // The guideway under the train, its pylons rushing past.
  const float gy = 21.0f * kTilePx - camY;
  if (gy < float(kScreenH))
  {
    r.fillRect(0.0f, gy + 40.0f, float(kScreenW), 60.0f, rgb(40, 44, 60));
    r.fillRect(0.0f, gy + 40.0f, float(kScreenW), 6.0f, rgb(150, 160, 190));
    r.fillRect(0.0f, gy + 100.0f, float(kScreenW), 400.0f, rgb(20, 22, 34));
    const float pitch = 420.0f;
    const float off = std::fmod(scroll * float(mScrollSpeeds[2]) + camX, pitch);
    for (float px = -off; px < float(kScreenW) + pitch; px += pitch)
    {
      r.fillRect(px, gy + 100.0f, 50.0f, 400.0f, rgb(52, 56, 74));
      r.fillRect(px, gy + 100.0f, 8.0f, 400.0f, rgb(80, 86, 110));
    }
    // Speed lines along the rail.
    for (int i = 0; i < 8; ++i)
    {
      const float lx = std::fmod(float(hash2(i, 77) % 2000u) - scroll * 40.0f + camX * 0.0f + 400000.0f, 1700.0f) - 200.0f;
      r.fillRect(lx, gy + 50.0f + float(i % 4) * 10.0f, 160.0f, 2.0f, rgba(200, 220, 255, 90));
    }
  }
  // Gantry struts (the posts behind the train).
  for (const auto& g : mGantries)
  {
    if (g.x < 0)
      continue;
    const float gx = (float(g.prevX) + float(g.x - g.prevX) * alpha) * kCellPx - camX;
    if (gx < -200.0f || gx > float(kScreenW) + 200.0f)
      continue;
    if (g.kind == GantryKind::Tall)
      r.fillRect(gx + 20.0f, float(g.bandBottom() + 1) * kCellPx - camY, 24.0f, 600.0f, rgb(60, 64, 80));
  }
  // The passing train: chrome cars on the track above ours.
  if (mPasser >= 0)
  {
    const Platform& pl = mPlatforms[std::size_t(mPasser)];
    if (!pl.hidden)
    {
      const float x = (float(pl.prevX) + float(pl.x - pl.prevX) * alpha) * kCellPx - camX;
      const float y = float(pl.y) * kCellPx - camY;
      const float w = float(pl.w) * kCellPx;
      r.fillRect(x, y, w, 150.0f, rgb(120, 128, 150));
      r.fillRect(x, y, w, 10.0f, rgb(220, 226, 240));
      r.fillRect(x, y + 60.0f, w, 40.0f, rgb(30, 30, 50));
      for (float wx = x + 20.0f; wx < x + w - 40.0f; wx += 70.0f)
        r.fillRect(wx, y + 66.0f, 44.0f, 28.0f, rgba(255, 230, 160, 200));
      r.fillRect(x, y + 120.0f, w, 6.0f, rgb(255, 40, 60));
      drawGlow(r, mArt, x + 10.0f, y + 110.0f, 40, rgb(255, 40, 60), 0.6f);
    }
  }
  // The station platform slides in as the train brakes.
  if (mStationLayer >= 0 && mBrakeAt >= 0)
  {
    const Layer& l = mLayers[std::size_t(mStationLayer)];
    const float t = std::min(float(kBrakeFrames), float(mStats.frames - mBrakeAt) + alpha);
    const float remaining = (float(kBrakeFrames) - t) * (float(kBrakeFrames) - t) / (2.0f * float(kBrakeFrames));
    const float x = float(l.x0) * kTilePx - camX + remaining * float(mScrollSpeeds[2]);
    const float y = float(l.y0) * kTilePx - camY;
    const float w = float(l.x1 - l.x0 + 1) * kTilePx + 400.0f, h = float(l.y1 - l.y0 + 1) * kTilePx;
    if (x < float(kScreenW))
    {
      r.fillRect(x, y, w, h, rgb(70, 72, 84));
      r.fillRect(x, y, w, 12.0f, rgb(255, 210, 40));
      for (float sx = x; sx < x + w; sx += 40.0f)
        r.fillRect(sx, y + 12.0f, 20.0f, 6.0f, rgb(30, 30, 30));
      r.fillRect(x, y + 40.0f, w, h - 40.0f, rgb(52, 54, 66));
      // Canopy and sign.
      r.fillRect(x + 60.0f, y - 340.0f, 16.0f, 340.0f, rgb(90, 96, 120));
      r.fillRect(x + 20.0f, y - 360.0f, w, 24.0f, rgb(110, 118, 140));
      r.fillRect(x + 140.0f, y - 300.0f, 360.0f, 56.0f, rgb(20, 30, 70));
      r.drawText("HALCYON CENTRAL", x + 320.0f, y - 288.0f, {28.0f, rgb(255, 255, 255), kInk, true}, Align::Center);
      drawGlow(r, mArt, x + 320.0f, y - 270.0f, 120, rgb(120, 180, 255), 0.25f);
    }
  }
}

void World::drawMaglevFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  if (!mTrain && mArcs.empty())
    return;
  const auto& p = mPlayer;
  // Caltrops and roof ripples.
  for (const auto& c : mCaltrops)
  {
    const float x = (float(c.prevX) + float(c.x - c.prevX) * alpha) * kCellPx - camX;
    const float y = (float(c.prevY) + float(c.y - c.prevY) * alpha) * kCellPx - camY;
    const Color body = c.green ? rgb(110, 240, 70) : rgb(200, 204, 214);
    const bool blink = c.life < 30 && (frame / 4) % 2 == 0;
    if (blink)
      continue;
    for (int i = 0; i < 4; ++i)
    {
      const float a = float(i) * 1.5708f + 0.4f;
      r.drawLine(x + 32.0f, y + 18.0f, x + 32.0f + std::cos(a) * 20.0f, y + 18.0f + std::sin(a) * 14.0f, 5.0f, body);
    }
    if (c.green)
      drawGlow(r, mArt, x + 32.0f, y + 16.0f, 30, rgb(120, 255, 80), 0.4f);
  }
  for (const auto& w : mWaves)
  {
    const float x = float(w.x) * kCellPx - camX, y = float(w.y) * kCellPx - camY;
    r.fillRect(x - 16.0f, y + 6.0f, 48.0f, 26.0f, rgba(255, 220, 140, 150), Blend::Add);
    drawGlow(r, mArt, x + 8.0f, y + 20.0f, 36, rgb(255, 180, 80), 0.5f);
  }
  // Track Hoppers' landing shadows.
  for (const auto& e : mEnemies)
    if (e.alive && e.kind == EnemyKind::Hopper && e.attach == 2 && e.dive - e.lastDive <= 10)
    {
      const float x = float(e.aimX) * kCellPx - camX, y = (float(e.aimY) + 1.0f) * kCellPx - camY;
      r.fillRect(x - 8.0f, y - 8.0f, float(e.w) * kCellPx + 16.0f, 10.0f, rgba(0, 0, 0, 130));
      r.fillRect(x, y - 6.0f, float(e.w) * kCellPx, 4.0f, rgba(255, 60, 40, 160));
    }
  // Decouplers' coupling lamps count down.
  for (const auto& e : mEnemies)
    if (e.alive && e.kind == EnemyKind::Decoupler && e.attach >= 1)
    {
      const int left = enemyDef(e.def).cooldown - (mStats.frames - e.lastDive);
      const float x = (float(e.x) + 1.5f) * kCellPx - camX, y = float(e.railX1 + 3) * kCellPx - camY;
      const bool on = left > 30 ? (frame / 8) % 2 == 0 : (frame / 3) % 2 == 0;
      drawGlow(r, mArt, x, y, 40, rgb(255, 40, 40), on ? 0.9f : 0.3f);
      r.drawText(std::to_string(std::max(0, left / 15 + 1)), x, y - 90.0f, {24.0f, rgb(255, 90, 80), kInk, true},
        Align::Center);
    }
  // Arc Caster lightning.
  for (const auto& a : mArcs)
  {
    const float x0 = a.x0 * kCellPx - camX, y0 = a.y0 * kCellPx - camY;
    const float x1 = a.x1 * kCellPx - camX, y1 = a.y1 * kCellPx - camY;
    const int segs = 7;
    float px = x0, py = y0;
    for (int i = 1; i <= segs; ++i)
    {
      const float u = float(i) / float(segs);
      const float j = i == segs ? 0.0f : (float(hash2(a.seed + i, frame / 2) % 60u) - 30.0f);
      const float nx = x0 + (x1 - x0) * u + j * 0.3f, ny = y0 + (y1 - y0) * u + j;
      r.drawLine(px, py, nx, ny, 7.0f, rgba(120, 170, 255, 160), Blend::Add);
      r.drawLine(px, py, nx, ny, 2.5f, rgb(240, 250, 255));
      px = nx;
      py = ny;
    }
    drawGlow(r, mArt, x1, y1, 50, rgb(150, 200, 255), 0.7f);
  }
  if (!mTrain)
    return;

  // Gantries: the band, its lamps, and the lights along the roof edges.
  const GantryKind* warnKind = nullptr;
  int warnEta = 1000;
  for (const auto& g : mGantries)
  {
    if (g.x < 0)
      continue;
    const CellBox pb = p.box();
    const int eta = etaFor(g, pb);
    const bool inBand = pb.top() - 1 <= g.bandBottom() && pb.bottom() >= g.bandTop() - 6;
    if (eta >= 0 && eta <= g.warn && eta < warnEta && inBand)
    {
      warnEta = eta;
      warnKind = &g.kind;
    }
    const float gx = (float(g.prevX) + float(g.x - g.prevX) * alpha) * kCellPx - camX;
    if (gx < -400.0f || gx > float(kScreenW) + 200.0f)
      continue;
    const float top = float(g.bandTop()) * kCellPx - camY, bot = float(g.bandBottom() + 1) * kCellPx - camY;
    const Color lamp = gantryColor(g.kind);
    if (g.kind == GantryKind::Mouth || g.kind == GantryKind::Ring)
    {
      // Concrete: the portal's face, or a ring of the tunnel lining.
      const float wide = g.kind == GantryKind::Mouth ? 180.0f : 64.0f;
      r.fillRect(gx, -200.0f, wide, bot + 200.0f, rgb(64, 64, 72));
      r.fillRect(gx, -200.0f, 10.0f, bot + 200.0f, rgb(110, 110, 120));
      r.fillRect(gx, bot - 18.0f, wide, 18.0f, rgb(40, 40, 46));
      for (float ly = top + 60.0f; ly < bot - 40.0f; ly += 160.0f)
        drawGlow(r, mArt, gx + 30.0f, ly, 30, lamp, 0.6f);
      if (g.kind == GantryKind::Mouth)
        for (float hy = top + 20.0f; hy < bot; hy += 60.0f)
          r.fillRect(gx + 20.0f, hy, wide - 40.0f, 14.0f, ((int(hy) / 60) % 2) ? rgb(240, 200, 40) : rgb(30, 30, 30));
      continue;
    }
    if (g.kind != GantryKind::Tall)
      r.fillRect(gx + 20.0f, -200.0f, 24.0f, top + 200.0f, rgb(80, 86, 104));
    r.fillRect(gx, top, 64.0f, bot - top, rgb(90, 96, 116));
    r.fillRect(gx, top, 64.0f, 8.0f, rgb(170, 176, 196));
    for (float hy = top + 14.0f; hy < bot - 8.0f; hy += 26.0f)
      r.fillRect(gx + 6.0f, hy, 52.0f, 12.0f, ((int(hy) / 26) % 2) ? rgb(240, 200, 40) : rgb(30, 30, 30));
    const bool on = (frame / 4) % 2 == 0;
    drawGlow(r, mArt, gx + 32.0f, top + 4.0f, 34, lamp, on ? 0.9f : 0.5f);
    drawGlow(r, mArt, gx + 32.0f, bot - 4.0f, 34, lamp, on ? 0.5f : 0.9f);
  }
  if (warnKind)
  {
    // The ring of lights along the roof edges, amber then red.
    const bool red = warnEta <= 8;
    const Color c = red ? rgb(255, 40, 40) : rgb(255, 170, 30);
    const bool on = (frame / (red ? 2 : 4)) % 2 == 0;
    const int rowTop = 24;
    const int cx0 = std::max(0, int(camX / kCellPx)), cx1 = std::min(mMap.width() - 1, int((camX + float(kScreenW)) / kCellPx) + 1);
    for (int cx = cx0 - cx0 % 4; cx <= cx1; cx += 4)
    {
      if (!mMap.solid(cx, rowTop) || mMap.solid(cx, rowTop - 1))
        continue;
      const float lx = (float(cx) + 0.5f) * kCellPx - camX, ly = float(rowTop) * kCellPx - camY + 4.0f;
      r.fillRect(lx - 5.0f, ly - 3.0f, 10.0f, 6.0f, on ? c : withAlpha(c, 90));
      if (on)
        drawGlow(r, mArt, lx, ly, 18, c, 0.5f);
    }
    r.drawText(gantryAdvice(*warnKind), float(kScreenW) * 0.5f, 120.0f, {30.0f, c, kInk, true}, Align::Center,
      on ? 1.0f : 0.6f);
  }

  // The tunnel: dark between its mouth and its end, sodium lamps rushing by.
  if (mTunnelState >= 1)
  {
    const float scroll = mScroll + trainSpeed() * alpha;
    float x0 = float(mTunnelMouthX) * kCellPx - camX;
    float x1 = mTunnelState == 2 ? float(mTunnelExitX) * kCellPx - camX : float(kScreenW) + 10.0f;
    x0 = std::max(0.0f, x0);
    x1 = std::min(float(kScreenW), x1);
    if (x1 > x0)
    {
      const bool gap = mStats.frames < mGapUntil;
      r.fillRect(x0, 0.0f, x1 - x0, float(kScreenH), rgba(4, 4, 12, gap ? 120 : 175));
      const float pitch = 20.0f * kTilePx * 0.5f;
      const float off = std::fmod(scroll * float(mScrollSpeeds[2]) + camX * 0.0f, pitch);
      for (float lx = x0 - off + pitch; lx < x1; lx += pitch)
      {
        drawGlow(r, mArt, lx, 60.0f, 110, rgb(255, 150, 40), 0.55f);
        r.fillRect(lx - 30.0f, 52.0f, 60.0f, 10.0f, rgb(255, 200, 120));
      }
      if (gap && mPasser >= 0 && !mPlatforms[std::size_t(mPasser)].hidden)
      {
        // Night sky through the gap in the tunnel roof.
        const Platform& pl = mPlatforms[std::size_t(mPasser)];
        const float gx = float(pl.x - 8) * kCellPx - camX;
        drawGlow(r, mArt, gx + float(pl.w + 16) * kCellPx * 0.5f, 0.0f, 128, rgb(120, 140, 200), 0.35f);
      }
    }
  }

  // Rain, everywhere but in the tunnel.
  if (mTunnelState != 1)
  {
    const float t = float(frame);
    for (int i = 0; i < 70; ++i)
    {
      const unsigned h = hash2(i, 991);
      const float speed = 26.0f + float(h % 10u);
      const float x = std::fmod(float(h % 1600u) - t * 9.0f + 160000.0f, 1500.0f) - 100.0f;
      const float y = std::fmod(float((h >> 8) % 800u) + t * speed, 820.0f) - 60.0f;
      r.drawLine(x, y, x - 10.0f, y + 34.0f, 2.0f, rgba(170, 190, 230, 90));
    }
  }
}

} // namespace gr
