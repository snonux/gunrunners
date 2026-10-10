// Deep water: `@ sea rect=...` fills a rectangle with water whose surface is
// its top row. A runner in it swims (slowly, sinking a little when idle) and
// holds their breath for 20 seconds, then starts losing hearts; at the
// surface the air comes back and a jump climbs out. A submarine drives
// anywhere in it. Its residents: Piranhas that chase whatever is in the
// water with them, Jellyfish that pulse and sting, Sea Mines that blow up
// when something comes close, and the Anglerfish that lunges from behind
// its lure.

#include "game/world.hpp"

#include "assets/art.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace gr
{

namespace
{

constexpr float kCellPx = float(kCellSize) * kPixelScale; // 32
constexpr int kDrownEvery = 20; // frames per heart once the air is gone

int sgn(int v) { return (v > 0) - (v < 0); }

unsigned mix(unsigned a, unsigned b)
{
  unsigned h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u + (a << 6) + (a >> 2));
  h ^= h >> 15;
  h *= 0x2C1B3C6Du;
  h ^= h >> 12;
  return h;
}

} // namespace

bool World::setupSeaEntity(const EntityDef& e)
{
  if (e.kind != "sea")
    return false;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  if (!e.rect("rect", x0, y0, x1, y1))
    return true;
  Sea s;
  s.x0 = x0 * kCellsPerTile;
  s.y0 = y0 * kCellsPerTile;
  s.x1 = (x1 + 1) * kCellsPerTile - 1;
  s.y1 = (y1 + 1) * kCellsPerTile - 1;
  mSeas.push_back(s);
  return true;
}

bool World::inSea(int cx, int cy) const
{
  for (const auto& s : mSeas)
    if (s.contains(cx, cy))
      return true;
  return false;
}

int World::seaSurface(int cx) const
{
  int best = -1;
  for (const auto& s : mSeas)
    if (cx >= s.x0 && cx <= s.x1 && (best < 0 || s.y0 < best))
      best = s.y0;
  return best;
}

bool World::waterCell(int cx, int cy) const { return inSea(cx, cy) && !mMap.solid(cx, cy); }

// --- The runner in the water ------------------------------------------------------

bool World::updateSwim(int mvX, int mvY, const PlayerInput& input)
{
  auto& p = mPlayer;
  const int cx = p.x + 1;
  if (!inSea(cx, p.y - 2))
  {
    if (p.state == PlayerState::Swim)
    {
      p.state = PlayerState::Falling;
      p.frames = 0;
    }
    return false;
  }
  if (p.state != PlayerState::Swim)
  {
    p.state = PlayerState::Swim;
    p.frames = 0;
    p.fling = 0;
    p.vineArc = false;
    p.jumpRequested = false;
    playSound(Sfx::Splash);
    const int surface = seaSurface(cx);
    burst({(float(p.x) + 1.5f) * kCellSize, float(surface) * kCellSize}, rgb(200, 240, 255), rgb(80, 160, 230), 14,
      1.6f, false);
  }
  ++p.frames;
  p.stance = Stance::Regular;
  if (mvX != 0)
  {
    if (mvX != p.facing)
      p.facing = mvX;
    else if (p.frames % 2 == 0 || p.turbo > 0)
      mMap.moveHorizontally(p.x, p.y, Player::kWidth, p.height(), mvX);
  }
  const int surface = seaSurface(cx);
  const bool headOut = surface >= 0 && p.y - 4 < surface;
  // At the surface, a jump climbs out.
  if (input.jump.triggered && headOut && !mMap.touchingCeiling(p.box()))
  {
    jump();
    return true;
  }
  int dy = 0;
  if (mvY < 0 || input.jump.pressed)
    dy = p.frames % 2 == 0 ? -1 : 0; // a stroke up every other frame
  else if (mvY > 0)
    dy = 1;
  else
    dy = p.frames % 4 == 0 ? 1 : 0; // sinks slowly when idle
  if (dy < 0 && surface >= 0 && p.y - 4 <= surface - 2)
    dy = 0; // head and shoulders out is as high as swimming goes
  if (dy != 0)
    mMap.moveVertically(p.x, p.y, Player::kWidth, p.height(), dy);
  setVisual(dy < 0 || (mvX != 0 && p.frames % 8 < 4) ? PlayerVisual::Jumping : PlayerVisual::Falling);
  return true;
}

void World::updateSea()
{
  if (mSeas.empty())
    return;
  auto& p = mPlayer;
  const bool underwater = p.vehicle < 0 && p.state != PlayerState::Dying && inSea(p.x + 1, p.y - 4);
  if (!underwater)
  {
    mAir = std::min(kAirFrames, mAir + 8);
    mDrown = 0;
    return;
  }
  if (mAir > 0)
  {
    --mAir;
    if (mAir == kAirFrames / 3)
      showMessage("AIR RUNNING LOW - GET TO THE SURFACE");
    if (mAir == 0)
      showMessage("OUT OF AIR");
    if (mStats.frames % 9 == 0)
      burst({(float(p.x) + 1.5f + float(p.facing)) * kCellSize, float(p.y - 5) * kCellSize}, rgb(220, 245, 255),
        rgb(160, 210, 255), 2, 0.6f, false);
    return;
  }
  if (++mDrown >= kDrownEvery)
  {
    mDrown = 0;
    hurtPlayer(1);
  }
}

// --- Residents -----------------------------------------------------------------

namespace
{

// The runner or the vehicle they drive: its centre, and whether it is in the water.
struct SeaTarget
{
  int x, y;
  bool wet;
  CellBox box;
};

} // namespace

// (Members can't return the local struct, so these use a small helper.)
static SeaTarget seaTargetOf(const World& w)
{
  const Player& p = w.player();
  const CellBox b = w.riding() ? w.riding()->box() : p.box();
  const int x = b.x + b.w / 2, y = b.y + b.h / 2;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  return {x, y, alive && w.inSea(x, y), b};
}

void World::updateFish(Enemy& e, const EnemyDef& def)
{
  auto inWater = [&](const CellBox& b) {
    return waterCell(b.x, b.y) && waterCell(b.right(), b.y) && waterCell(b.x, b.bottom()) &&
      waterCell(b.right(), b.bottom());
  };
  const SeaTarget t = seaTargetOf(*this);
  const int ex = e.x + e.w / 2, ey = e.y - e.h / 2;
  const int dist = std::abs(t.x - ex) + std::abs(t.y - ey);
  if (t.wet && dist <= def.range)
    e.attach = 1;
  else if (!t.wet || dist > def.range * 2)
    e.attach = 0;
  if (e.attach == 0)
  {
    if (e.timer % std::max(1, def.stepEvery) != 0)
      return;
    CellBox nb = e.box();
    nb.x += e.dir;
    if (inWater(nb))
      e.x += e.dir;
    else
      e.dir = -e.dir;
    return;
  }
  // The chase: a cell a frame along, every other frame up or down.
  const int sx = sgn(t.x - ex), sy = sgn(t.y - ey);
  if (sx != 0)
  {
    e.dir = sx;
    CellBox nb = e.box();
    nb.x += sx;
    if (inWater(nb))
      e.x += sx;
  }
  if (sy != 0 && e.timer % 2 == 0)
  {
    CellBox nb = e.box();
    nb.y += sy;
    if (inWater(nb))
      e.y += sy;
  }
}

void World::updateJelly(Enemy& e, const EnemyDef& /*def*/)
{
  auto inWater = [&](const CellBox& b) {
    return waterCell(b.x, b.y) && waterCell(b.right(), b.y) && waterCell(b.x, b.bottom()) &&
      waterCell(b.right(), b.bottom());
  };
  // A pulse up, then a slow sink, and a lean toward whatever is close.
  const int ph = (e.timer + e.id * 7) % 48;
  CellBox nb = e.box();
  if (ph < 10)
  {
    nb.y -= 1;
    if (inWater(nb))
      --e.y;
  }
  else if (ph % 3 == 0)
  {
    nb.y += 1;
    if (inWater(nb))
      ++e.y;
  }
  e.tell = ph < 10 ? 1 : 0;
  const SeaTarget t = seaTargetOf(*this);
  if (t.wet && std::abs(t.x - (e.x + 1)) < 24 && e.timer % 4 == 0)
  {
    const int sx = sgn(t.x - (e.x + 1));
    CellBox sb = e.box();
    sb.x += sx;
    if (sx != 0 && inWater(sb))
      e.x += sx;
  }
}

void World::updateSeaMine(Enemy& e, const EnemyDef& def)
{
  if (e.attach == 0)
  {
    e.ox = e.x;
    e.oy = e.y;
    e.attach = 1;
  }
  if (e.attach == 9)
    return;
  // Bobs on its chain.
  e.y = e.oy + ((e.timer / 15) % 2);
  if (e.tell > 0)
  {
    if (--e.tell == 0)
      damageEnemy(e, e.hp);
    return;
  }
  const SeaTarget t = seaTargetOf(*this);
  const int r = std::max(1, def.range);
  const CellBox area{e.x - r, e.y - e.h + 1 - r, e.w + r * 2, e.h + r * 2};
  const bool alive = mPlayer.state != PlayerState::Dying && mPlayer.state != PlayerState::Teleporting;
  if (alive && area.intersects(t.box))
  {
    e.tell = std::max(1, def.tell);
    playSound(Sfx::Beep);
  }
}

void World::blowSeaMine(Enemy& e)
{
  if (e.attach == 9)
    return;
  e.attach = 9;
  e.alive = false;
  const int cx = e.x + e.w / 2, cy = e.y - e.h / 2;
  const CellBox area{cx - 5, cy - 5, 11, 11};
  const SeaTarget t = seaTargetOf(*this);
  if (area.intersects(t.box))
    hurtPlayer(2);
  explodeAt(cx, cy, 4, 3);
  burst({(float(cx) + 0.5f) * kCellSize, (float(cy) + 0.5f) * kCellSize}, rgb(220, 245, 255), rgb(120, 190, 255),
    24, 2.4f, false);
}

void World::updateAngler(Enemy& e, const EnemyDef& def)
{
  auto inWater = [&](const CellBox& b) {
    return waterCell(b.x, b.y) && waterCell(b.right(), b.y) && waterCell(b.x, b.bottom()) &&
      waterCell(b.right(), b.bottom());
  };
  auto stepToward = [&](int tx, int ty) {
    const int sx = sgn(tx - e.x), sy = sgn(ty - e.y);
    CellBox nb = e.box();
    nb.x += sx;
    if (sx != 0 && inWater(nb))
      e.x += sx;
    nb = e.box();
    nb.y += sy;
    if (sy != 0 && inWater(nb))
      e.y += sy;
    return sx != 0 || sy != 0;
  };
  if (e.attach == 0)
  {
    e.ox = e.x;
    e.oy = e.y;
    e.attach = 1;
  }
  const SeaTarget t = seaTargetOf(*this);
  const int ex = e.x + e.w / 2, ey = e.y - e.h / 2;
  switch (e.attach)
  {
    case 1: // waiting behind the lure
      if (t.wet && t.x != ex)
        e.dir = sgn(t.x - ex);
      if (e.cool > 0)
        --e.cool;
      else if (t.wet && std::abs(t.x - ex) + std::abs(t.y - ey) <= def.range)
      {
        e.tell = std::max(1, def.tell);
        e.attach = 2;
        playSound(Sfx::Rumble);
      }
      break;
    case 2: // the lure flares
      if (--e.tell <= 0)
      {
        e.aimX = t.x - e.w / 2;
        e.aimY = t.y + e.h / 2;
        e.attach = 3;
        e.dive = 10;
        playSound(Sfx::Snap);
      }
      break;
    case 3: // the lunge, two cells a frame
      for (int k = 0; k < 2; ++k)
        stepToward(e.aimX, e.aimY);
      if (--e.dive <= 0 || (e.x == e.aimX && e.y == e.aimY))
        e.attach = 4;
      break;
    default: // back to its spot
      if (!stepToward(e.ox, e.oy) || (e.x == e.ox && e.y == e.oy))
      {
        e.attach = 1;
        e.cool = def.cooldown * 2;
      }
      break;
  }
}

// --- Drawing -------------------------------------------------------------------

void World::drawSeaBack(Renderer& r, float camX, float camY, int /*frame*/) const
{
  // Behind the rocks: the deep, darker the further down.
  for (const auto& s : mSeas)
  {
    const float x0 = float(s.x0) * kCellPx - camX, x1 = float(s.x1 + 1) * kCellPx - camX;
    const float y0 = float(s.y0) * kCellPx - camY, y1 = float(s.y1 + 1) * kCellPx - camY;
    if (x1 < 0.0f || x0 > float(kScreenW) || y1 < 0.0f || y0 > float(kScreenH))
      continue;
    const float l = std::max(0.0f, x0), rr = std::min(float(kScreenW), x1);
    const int bands = 12;
    for (int k = 0; k < bands; ++k)
    {
      const float a = y0 + (y1 - y0) * float(k) / float(bands), b = y0 + (y1 - y0) * float(k + 1) / float(bands);
      if (b < 0.0f || a > float(kScreenH))
        continue;
      const float t = float(k) / float(bands - 1);
      const Color c = lerpColor(rgba(20, 90, 150, 170), rgba(4, 14, 40, 235), t);
      r.fillRect(l, std::max(0.0f, a), rr - l, std::min(float(kScreenH), b) - std::max(0.0f, a) + 1.0f, c);
    }
  }
}

void World::drawSeaFront(Renderer& r, float camX, float camY, int frame) const
{
  for (const auto& s : mSeas)
  {
    const float x0 = float(s.x0) * kCellPx - camX, x1 = float(s.x1 + 1) * kCellPx - camX;
    const float y0 = float(s.y0) * kCellPx - camY, y1 = float(s.y1 + 1) * kCellPx - camY;
    if (x1 < 0.0f || x0 > float(kScreenW) || y1 < 0.0f || y0 > float(kScreenH))
      continue;
    const float l = std::max(0.0f, x0), rr = std::min(float(kScreenW), x1);
    const float top = std::max(0.0f, y0), bottom = std::min(float(kScreenH), y1);
    // A blue-green tint over everything in the water.
    r.fillRect(l, top, rr - l, bottom - top, rgba(40, 130, 190, 54));
    // Light shafts from the surface, swaying.
    for (int k = 0; k < 8; ++k)
    {
      const float wx = std::fmod(float(k) * 263.0f - camX * 0.6f, float(kScreenW) + 400.0f);
      const float sx = (wx < 0.0f ? wx + float(kScreenW) + 400.0f : wx) - 200.0f;
      const float sway = std::sin(float(frame) * 0.013f + float(k)) * 40.0f;
      if (sx + 200.0f < l || sx - 200.0f > rr)
        continue;
      r.drawLine(sx, y0, sx + 120.0f + sway, std::min(y1, y0 + 520.0f), 46.0f, rgba(170, 230, 255, 16), Blend::Add);
    }
    // Rising bubbles.
    for (int k = 0; k < 40; ++k)
    {
      const unsigned h = mix(unsigned(k), 77u);
      const float span = std::max(1.0f, x1 - x0);
      const float bx = x0 + float(h % 10000u) / 10000.0f * span + std::sin(float(frame) * 0.05f + float(k)) * 6.0f;
      const float rise = float((frame * (1 + int(h % 3u)) + int(h >> 8)) % std::max(1, int(y1 - y0)));
      const float by = y1 - rise;
      if (bx < -8.0f || bx > float(kScreenW) + 8.0f || by < y0 + 4.0f || by < -8.0f || by > float(kScreenH) + 8.0f)
        continue;
      const float sz = 3.0f + float(h % 4u);
      r.fillRect(bx, by, sz, sz, rgba(220, 245, 255, 120), Blend::Add);
    }
    // The surface: a bright rippling line.
    if (y0 >= -10.0f && y0 <= float(kScreenH) + 10.0f)
    {
      for (float x = l; x < rr; x += 16.0f)
      {
        const float wave = std::sin((x + camX) * 0.03f + float(frame) * 0.12f) * 3.0f;
        r.fillRect(x, y0 + wave - 2.0f, 16.0f, 5.0f, rgba(200, 245, 255, 150));
        r.fillRect(x, y0 + wave + 3.0f, 16.0f, 10.0f, rgba(120, 210, 255, 40));
      }
    }
  }
}

void World::drawAirHud(Renderer& r, int frame) const
{
  if (mSeas.empty() || mState != WorldState::Playing)
    return;
  const auto& p = mPlayer;
  if (p.vehicle >= 0 || mAir >= kAirFrames)
    return;
  // Ten bubbles of breath above the runner's head in the HUD row.
  const float x = 24.0f, y = 140.0f; // under the HUD messages (y 92)
  r.fillRect(x - 10.0f, y - 8.0f, 360.0f, 46.0f, rgba(6, 20, 40, 170));
  r.drawText("AIR", x, y, {18.0f, rgb(170, 230, 255), rgb(6, 20, 40)});
  const int bubbles = (mAir * 10 + kAirFrames - 1) / kAirFrames;
  for (int i = 0; i < 10; ++i)
  {
    const float bx = x + 70.0f + float(i) * 27.0f, by = y + 15.0f;
    const bool full = i < bubbles;
    const bool blink = mAir < kAirFrames / 3 && full && (frame / 10) % 2 == 0;
    r.fillRect(bx - 9.0f, by - 9.0f, 18.0f, 18.0f, full ? (blink ? rgb(255, 255, 255) : rgb(150, 220, 255)) : rgba(60, 80, 100, 160));
    r.fillRect(bx - 5.0f, by - 6.0f, 5.0f, 5.0f, rgba(255, 255, 255, full ? 220 : 60));
  }
}

} // namespace gr
