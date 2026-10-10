// Episode 7, DEEP SPACE (docs/DEEP_SPACE.md). Level 48, Bounder Plains:
// the Tamer's Whip (a short crack that stuns aliens and calls the Bounders
// near you), climbing onto a Bounder's back by landing on it, the way a
// Bounder nobody rides trots home (or to the whip), and the plains' aliens:
// the Thorn Hog (paces, lowers its tusks and charges), the Sky Gulper (a
// floating mouth that swallows a runner on foot and spits them back) and
// the thornbushes in the grass. Riding a Bounder is in world_vehicle.cpp.

#include "game/world.hpp"

#include "assets/art.hpp"
#include "assets/enemy_art.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace gr
{

namespace
{

constexpr float kCellPx = float(kCellSize) * kPixelScale; // 32
constexpr int kWhipReach = 8;   // cells ahead the lash reaches
constexpr int kWhipStun = 45;   // frames an alien it hits stays dazed
constexpr int kCallReach = 32;  // cells: a Bounder this near comes when the whip cracks
constexpr int kCallFrames = 150;
constexpr int kSpit = 10;       // cells a Sky Gulper spits you back

int sgn(int v) { return (v > 0) - (v < 0); }

} // namespace

// --- The Tamer's Whip ----------------------------------------------------------------

void World::crackWhip(int ox, int oy, int dir)
{
  auto& p = mPlayer;
  dir = dir < 0 ? -1 : 1;
  int damage = protoDef(ProtoId::TamersWhip).damage;
  if (p.turbo > 0)
    damage *= 2;
  else if (p.virus > 0)
    damage = std::max(1, damage / 2);
  int len = 0;
  while (len < kWhipReach && !mMap.solid(ox + dir * (len + 1), oy))
    ++len;
  const CellBox reach{dir > 0 ? ox : ox - len, oy - 2, len + 1, 5};
  for (auto& e : mEnemies)
  {
    if (!e.alive || e.hidden || e.trapped || e.y < 0 || !e.box().intersects(reach))
      continue;
    burst(cellCenter(e.box()), rgb(255, 230, 160), rgb(255, 255, 255), 8, 1.4f);
    damageEnemy(e, damage);
    if (e.alive)
      e.stun = std::max(e.stun, kWhipStun);
  }
  for (auto& bx : mBoxes)
    if (bx.alive && bx.box().intersects(reach))
    {
      destroyBox(bx);
      break;
    }
  // Every Bounder in earshot that nobody rides comes trotting.
  bool called = false;
  for (auto& v : mVehicles)
  {
    if (v.kind != VehicleKind::Bounder || v.occupied || v.wreck > 0)
      continue;
    const int dx = std::abs(v.x + v.w / 2 - (p.x + 1)), dy = std::abs(v.y - p.y);
    if (dx > kCallReach || dy > kCallReach)
      continue;
    v.call = kCallFrames;
    called = true;
  }
  if (called)
    showMessage("HUP! HUP! - A BOUNDER IS COMING");
  mSpace.whipX = ox;
  mSpace.whipY = oy;
  mSpace.whipDir = dir;
  mSpace.whipLen = len;
  mSpace.whipShow = 5;
  playSound(Sfx::Crack);
}

void World::updatePlains()
{
  if (mSpace.whipShow > 0)
    --mSpace.whipShow;
}

// --- Bounders ------------------------------------------------------------------------

bool World::tryMountBounder()
{
  // Landing on a Bounder's back puts you in the saddle.
  auto& p = mPlayer;
  if (p.state != PlayerState::Falling || p.vehicle >= 0 || p.cart >= 0)
    return false;
  const CellBox pb = p.box();
  for (std::size_t i = 0; i < mVehicles.size(); ++i)
  {
    const Vehicle& v = mVehicles[i];
    if (v.kind != VehicleKind::Bounder || v.occupied || v.wreck > 0)
      continue;
    const CellBox vb = v.box();
    const int top = vb.top();
    if (pb.bottom() < top - 1 || pb.bottom() > top + 2)
      continue;
    if (pb.right() < vb.left() + 1 || pb.left() > vb.right() - 1)
      continue;
    boardVehicle(int(i));
    return true;
  }
  return false;
}

void World::updateParkedBounder(Vehicle& v)
{
  // Nobody on its back: it trots home to its pen, or to a whip's crack,
  // and hops what is in the way.
  const auto& p = mPlayer;
  int tx = v.homeX;
  if (v.call > 0)
  {
    --v.call;
    tx = p.x + 1 - v.w / 2;
  }
  const int dx = tx - v.x;
  const bool grounded = v.air < 0 && mMap.onSolidGround(v.box());
  bool blocked = false;
  if (std::abs(dx) > (v.call > 0 ? 3 : 1) && (v.air >= 0 || ++v.step % 2 == 0))
  {
    v.facing = sgn(dx);
    const MoveResult r = grounded ? mMap.moveHorizontallyWithStairStepping(v.x, v.y, v.w, v.h, v.facing)
                                  : mMap.moveHorizontally(v.x, v.y, v.w, v.h, v.facing);
    blocked = r != MoveResult::Completed;
  }
  else if (v.call > 0 && std::abs(dx) <= 3 && grounded)
    v.call = 0; // here
  if (grounded && blocked)
  {
    v.air = 0; // a hop over it
    playSound(Sfx::Boing);
  }
  bounderAir(v, true);
}

// --- Aliens --------------------------------------------------------------------------

void World::updateThornHog(Enemy& e, const EnemyDef& def)
{
  // attach 0 pacing, 1 pawing the ground (the tell), 2 charging.
  if (!mMap.onSolidGround(e.box()))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, 2);
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return;
  }
  if (e.cool > 0)
    --e.cool;
  const auto& p = mPlayer;
  const CellBox b = e.box();
  auto blockedAhead = [&](int dir) {
    const bool wall = dir > 0 ? mMap.touchingRightWall(b) : mMap.touchingLeftWall(b);
    const int aheadX = dir > 0 ? b.right() + 1 : b.left() - 1;
    return wall || !mMap.solidTop(aheadX, b.bottom() + 1);
  };
  switch (e.attach)
  {
    case 0:
    {
      const int dxp = (p.x + 1) - (e.x + e.w / 2);
      if (e.cool == 0 && std::abs(p.y - e.y) <= 2 && std::abs(dxp) <= def.range && p.state != PlayerState::Dying &&
          !p.hidden)
      {
        e.dir = dxp < 0 ? -1 : 1;
        e.attach = 1;
        e.tell = def.tell;
        playSound(Sfx::Hiss);
        break;
      }
      if (e.timer % std::max(1, def.stepEvery) != 0)
        break;
      if (blockedAhead(e.dir))
        e.dir = -e.dir;
      else
        e.x += e.dir;
      break;
    }
    case 1:
      if (--e.tell <= 0)
      {
        e.attach = 2;
        e.ox = e.x;
      }
      break;
    default:
      for (int k = 0; k < 2; ++k)
      {
        if (blockedAhead(e.dir) || std::abs(e.x - e.ox) >= def.range + 8)
        {
          e.attach = 0;
          e.cool = def.cooldown;
          break;
        }
        e.x += e.dir;
      }
      if (e.timer % 3 == 0)
        burst({(float(e.x) + float(e.w) * 0.5f) * kCellSize, float(e.y + 1) * kCellSize}, rgb(200, 160, 200),
          rgb(120, 90, 120), 3, 1.0f, false);
      break;
  }
}

void World::updateSkyGulper(Enemy& e, const EnemyDef& def)
{
  // Sways to and fro over where it started, bobbing. A runner on foot that
  // touches its mouth is swallowed and spat back the way they came; one on
  // a Bounder is too big a mouthful.
  if (e.attach == 0)
  {
    e.attach = 1;
    e.ox = e.x;
    e.oy = e.y;
  }
  if (e.cool > 0)
    --e.cool;
  const float t = float(e.timer);
  e.x = e.ox + int(std::lround(float(def.range) * std::sin(t * 0.04f)));
  e.y = e.oy + int(std::lround(2.0f * std::sin(t * 0.11f)));
  e.dir = std::cos(t * 0.04f) >= 0.0f ? 1 : -1;
  auto& p = mPlayer;
  if (e.cool > 0 || p.vehicle >= 0 || p.mercy > 0 || p.hidden || p.state == PlayerState::Dying ||
      p.state == PlayerState::Teleporting || !e.box().intersects(p.hitBox()))
    return;
  e.cool = def.cooldown;
  playSound(Sfx::Gulp);
  const int dir = p.facing > 0 ? -1 : 1;
  burst(cellCenter(p.box()), rgb(255, 140, 180), rgb(255, 230, 240), 12, 1.6f, false);
  mMap.moveHorizontally(p.x, p.y, Player::kWidth, p.height(), dir * kSpit);
  p.facing = -dir; // still facing the way you were going
  p.mercy = 30;
  jump();
  burst(cellCenter(p.box()), rgb(255, 140, 180), rgb(255, 230, 240), 12, 1.6f, false);
  showMessage("GULPED - AND SPAT OUT");
}

// --- Drawing -------------------------------------------------------------------------

void World::drawPlainsFront(Renderer& r, float camX, float camY, int frame, float /*alpha*/) const
{
  if (!mSpace.plains || mSpace.whipShow <= 0)
    return;
  // The lash: a curling line out from the hand, the crack at its tip.
  const float x0 = (float(mSpace.whipX) + 0.5f) * kCellPx - camX, y0 = (float(mSpace.whipY) + 0.5f) * kCellPx - camY;
  const float len = (float(mSpace.whipLen) + 0.5f) * kCellPx * float(mSpace.whipDir);
  const float k = float(mSpace.whipShow) / 5.0f;
  const int segs = 10;
  float px = x0, py = y0;
  for (int i = 1; i <= segs; ++i)
  {
    const float u = float(i) / float(segs);
    const float x = x0 + len * u;
    const float y = y0 - std::sin(u * 3.14159f) * 26.0f * k + std::sin(u * 9.0f + float(frame)) * 4.0f * u;
    r.drawLine(px, py, x, y, 7.0f - 4.0f * u, rgb(120, 70, 40));
    r.drawLine(px, py, x, y, 3.0f - 1.5f * u, rgb(230, 170, 90));
    px = x;
    py = y;
  }
  drawGlow(r, mArt, px, py, 44, rgb(255, 240, 200), 0.9f * k);
}

} // namespace gr
