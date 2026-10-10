// Level 11, Idol Mines (SPEC 11): Mine Carts. Carts ride `@ rail` tracks
// and keep their speed: downhill they gather pace, uphill they lose it
// (between 1 and 3 cells a frame). Jump while riding to hop the cart over a
// gap (a second jump in the air bails out), hold down to duck. Shooting a
// lever throws its junction. A cart that rolls into a gap without hopping
// falls onto whatever is below; a lost cart comes back to its dock. Bumpers
// stop carts, and above 2 cells a frame that is a crash.
//
// The prototype is the Blasting Caps: a lobbed stick of dynamite that
// bounces twice, rolls (along rails at 3 cells a frame) and goes off after
// 30 frames, breaking `by=explosion` rock. Cart Bandits ride their own rail
// level with you, Bat Clouds swarm along their tunnels and Rock Moles
// surface out of the rock near you. Also the Pinball Mine bonus
// (rules=pinball), where the runner is the ball.

#include "game/world.hpp"

#include "assets/art.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace gr
{

namespace
{

constexpr Color kInk = rgb(10, 8, 20);
constexpr float kCellPx = float(kCellSize) * kPixelScale;
constexpr float kTilePx = float(kTileSize) * kPixelScale;
constexpr int kMinSpeed = 8, kMaxSpeed = 24; // eighths of a cell a frame
constexpr int kCrashSpeed = 16;              // a bumper hit above this throws you out
constexpr int kHopFrames = 12;
constexpr int kHop[kHopFrames] = {1, 2, 3, 3, 4, 4, 4, 4, 3, 3, 2, 1}; // cells up, frame by frame
constexpr int kLostFrames = 60;
constexpr int kCapFuse = 30;
constexpr float kCapGravity = 0.15f;
constexpr int kBlast = 6;          // cells: a cap's blast reaches 3 blocks
constexpr int kDaysEvery = 900;    // frames without a crash for another day on the sign
constexpr int kBatPeriod = 30;
constexpr int kMoleOut = 30, kMoleTell = 15;
const Color kRust = rgb(150, 74, 40);
const Color kIron = rgb(70, 66, 72);
const Color kTimber = rgb(132, 92, 52);
const Color kGold = rgb(255, 204, 70);

int sgn(int v) { return (v > 0) - (v < 0); }
float sgnf(float v) { return v > 0.0f ? 1.0f : (v < 0.0f ? -1.0f : 0.0f); }

int segLen(const std::pair<int, int>& a, const std::pair<int, int>& b)
{
  return std::max(1, std::max(std::abs(b.first - a.first), std::abs(b.second - a.second))) * 8;
}

// "34..66" -> 34, 66.
bool parseSpan(const std::string& s, int& a, int& b)
{
  const auto dots = s.find("..");
  if (dots == std::string::npos)
    return false;
  a = std::atoi(s.substr(0, dots).c_str());
  b = std::atoi(s.substr(dots + 2).c_str());
  if (a > b)
    std::swap(a, b);
  return true;
}

// Which way along x a rail runs at s (its segment's direction).
int railDx(const Rail& r, int s)
{
  int at = 0;
  for (std::size_t i = 1; i < r.pts.size(); ++i)
  {
    const int L = segLen(r.pts[i - 1], r.pts[i]);
    if (s <= at + L || i + 1 == r.pts.size())
      return sgn(r.pts[i].first - r.pts[i - 1].first);
    at += L;
  }
  return 1;
}

} // namespace

CellBox Cart::box() const
{
  return boxAt(int(std::lround(fx)) - 2, int(std::lround(fy)) - 1, kW, kH);
}

// --- Level entities ----------------------------------------------------------------

bool World::setupMineEntity(const EntityDef& e)
{
  if (e.kind == "rail")
  {
    Rail r;
    r.id = e.id;
    // Blocks to cells: x is the block's middle, y the top of the ground block.
    for (const auto& [x, y] : e.path("path"))
      r.pts.emplace_back(x * kCellsPerTile + 1, y * kCellsPerTile);
    // gaps=80..84;182..186
    const std::string gaps = e.str("gaps");
    std::size_t at = 0;
    while (at < gaps.size())
    {
      const auto semi = gaps.find(';', at);
      int a = 0, b = 0;
      if (parseSpan(gaps.substr(at, semi == std::string::npos ? std::string::npos : semi - at), a, b))
        r.gaps.emplace_back(a * kCellsPerTile, (b + 1) * kCellsPerTile - 1);
      if (semi == std::string::npos)
        break;
      at = semi + 1;
    }
    // branch=J1:1 - taken while lever J1 is in state 1.
    const std::string branch = e.str("branch");
    if (!branch.empty())
    {
      const auto colon = branch.find(':');
      r.leverId = branch.substr(0, colon);
      r.state = colon == std::string::npos ? 1 : std::atoi(branch.substr(colon + 1).c_str());
    }
    r.resets = e.num("reset", 0) != 0;
    for (std::size_t i = 1; i < r.pts.size(); ++i)
      r.len += segLen(r.pts[i - 1], r.pts[i]);
    if (r.pts.size() >= 2)
      mRails.push_back(r);
    return true;
  }
  if (e.kind == "cart" && e.hasPos)
  {
    // Given by the cart's left block, standing on its rail.
    Cart c;
    c.id = e.id;
    c.fx = float(e.x * kCellsPerTile + 2);
    c.fy = float((e.y + 1) * kCellsPerTile);
    c.painted = e.num("paint42", 0) != 0;
    c.dockRail = -1;
    // The rail is resolved in linkMine (rails can come later).
    c.dockS = e.num("dir", 1) < 0 ? -1 : 1;
    c.id += "|" + e.str("rail");
    mCarts.push_back(c);
    return true;
  }
  if (e.kind == "bumper" && e.hasPos)
  {
    mBumpers.push_back({e.x * kCellsPerTile, e.y * kCellsPerTile, kCellsPerTile, kCellsPerTile});
    return true;
  }
  if (e.kind == "lantern" && e.hasPos)
  {
    mLanterns.emplace_back(e.x * kCellsPerTile, e.y * kCellsPerTile + 1);
    return true;
  }
  if (e.kind == "lever" && e.hasPos)
  {
    Lever l;
    l.id = e.id;
    l.x = e.x * kCellsPerTile;
    l.y = e.y * kCellsPerTile - 2; // stands on the bottom of its block, 2 blocks tall
    l.states = std::max(2, e.num("states", 2));
    l.state = std::clamp(e.num("state", 0), 0, l.states - 1);
    mLevers.push_back(l);
    return true;
  }
  if (e.kind == "vein")
  {
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    if (e.rect("rect", x0, y0, x1, y1))
      mVeins.push_back(
        {x0 * kCellsPerTile, y0 * kCellsPerTile, (x1 - x0 + 1) * kCellsPerTile, (y1 - y0 + 1) * kCellsPerTile});
    return true;
  }
  if (e.kind == "respawn" && e.hasPos)
  {
    // The box at that block comes back `frames` after it was opened.
    const int bx = e.x * kCellsPerTile, by = e.y * kCellsPerTile + 1;
    for (std::size_t i = 0; i < mBoxes.size(); ++i)
      if (mBoxes[i].x == bx && mBoxes[i].y == by)
      {
        const int frames = std::max(15, e.num("frames", 150));
        mRespawns.push_back({int(i), frames, frames});
      }
    return true;
  }
  if (e.kind == "trapdoor" && e.hasPos)
  {
    Trapdoor t;
    t.x0 = e.x;
    t.x1 = e.x + std::max(1, e.num("w", 2)) - 1;
    t.y = e.y;
    mTrapdoors.push_back(t);
    return true;
  }
  if (e.kind == "rubble" && e.hasPos)
  {
    // rubble x y breakable=N: once that breakable (by its index among the
    // level's breakables) is blown, a block of rubble lands at x, y.
    mRubble.push_back({e.num("breakable", int(mBreakables.size()) - 1), e.x, e.y, 0});
    return true;
  }
  if (e.kind == "dayssign" && e.hasPos)
  {
    mDaysSign = {e.x * kCellsPerTile, e.y * kCellsPerTile, e.num("w", 4) * kCellsPerTile, e.num("h", 2) * kCellsPerTile};
    return true;
  }
  if (e.kind == "batcloud" && e.hasPos)
  {
    const int def = enemyIndex("bat");
    const int count = std::clamp(e.num("count", 6), 1, 10);
    int a = e.x - 8, b = e.x + 8;
    parseSpan(e.str("patrol"), a, b);
    for (int i = 0; i < count && def >= 0; ++i)
    {
      spawnEnemy(def, e.x * kCellsPerTile + i * 3, e.y * kCellsPerTile + 1);
      Enemy& bat = mEnemies.back();
      bat.railX0 = a * kCellsPerTile;
      bat.railX1 = (b + 1) * kCellsPerTile - bat.w;
      bat.aimY = e.y * kCellsPerTile; // the cloud's middle row
      bat.attach = e.num("amp", 2) * kCellsPerTile;
      bat.aimX = i * 3; // its place in the cloud
      bat.timer = 0;
    }
    return true;
  }
  if (e.kind == "flipper" || e.kind == "pbumper" || e.kind == "lamp" || e.kind == "pwall" || e.kind == "plunger" ||
      e.kind == "gate" || e.kind == "drain")
  {
    auto& pin = mPin;
    const auto pts = e.list("at");
    if (e.kind == "flipper" && e.hasPos)
    {
      // x y: the pivot, side=l|r.
      if (e.str("side") == "r")
      {
        pin.rx = float(e.x * kCellsPerTile);
        pin.ry = float(e.y * kCellsPerTile + 1);
      }
      else
      {
        pin.lx = float(e.x * kCellsPerTile + kCellsPerTile);
        pin.ly = float(e.y * kCellsPerTile + 1);
      }
      pin.flipLen = float(e.num("len", 4) * kCellsPerTile) - 1.0f;
    }
    else if (e.kind == "pbumper" && e.hasPos)
      pin.bumpers.push_back({float(e.x * kCellsPerTile + 1), float(e.y * kCellsPerTile + 1), 2.2f});
    else if (e.kind == "lamp" && e.hasPos)
      pin.lamps.push_back({float(e.x * kCellsPerTile + 1), float(e.y * kCellsPerTile + 1)});
    else if (e.kind == "pwall")
    {
      // path=x,y;x,y;... in blocks (their corners): a rail the ball bounces off.
      const auto path = e.path("path");
      for (std::size_t i = 1; i < path.size(); ++i)
        pin.segs.push_back({float(path[i - 1].first * kCellsPerTile), float(path[i - 1].second * kCellsPerTile),
          float(path[i].first * kCellsPerTile), float(path[i].second * kCellsPerTile)});
    }
    else if (e.kind == "plunger" && e.hasPos)
    {
      pin.plungerX = float(e.x * kCellsPerTile + 1);
      pin.plungerY = float(e.y * kCellsPerTile);
    }
    else if (e.kind == "gate" && e.hasPos)
    {
      pin.gateX = float(e.x * kCellsPerTile + 1);
      pin.gateY = float(e.y * kCellsPerTile + 1);
    }
    else if (e.kind == "drain")
    {
      int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
      if (e.rect("rect", x0, y0, x1, y1))
        pin.drain = {x0 * kCellsPerTile, y0 * kCellsPerTile, (x1 - x0 + 1) * kCellsPerTile,
          (y1 - y0 + 1) * kCellsPerTile};
    }
    (void)pts;
    return true;
  }
  return false;
}

void World::setupMineEnemy(Enemy& en, const EntityDef& e)
{
  if (en.kind == EnemyKind::Bandit)
  {
    // Waits off stage until it is its turn; rides rail= (resolved in linkMine).
    en.hidden = true;
    int order = 0;
    for (const auto& o : mEnemies)
      order += o.kind == EnemyKind::Bandit && &o != &en;
    en.attach = order;
    en.dive = 0;
    en.aimX = 0;
    en.aimY = -1;
    en.platform = -1;
    for (std::size_t i = 0; i < mRails.size(); ++i)
      if (mRails[i].id == e.str("rail"))
        en.platform = int(i);
    en.railX0 = en.x; // where it rolls in
  }
  if (en.kind == EnemyKind::Mole)
  {
    en.hidden = true;
    en.railX0 = en.x; // home
    en.railX1 = en.y;
    en.attach = 0;
    en.dive = 0;
    en.active = true;
  }
}

void World::linkMine()
{
  if (mCarts.empty() && mLevers.empty() && mRails.empty())
    return;
  for (auto& r : mRails)
    for (std::size_t i = 0; i < mLevers.size(); ++i)
      if (!r.leverId.empty() && mLevers[i].id == r.leverId)
        r.lever = int(i);
  for (auto& c : mCarts)
  {
    const auto bar = c.id.find('|');
    const std::string rail = bar == std::string::npos ? std::string() : c.id.substr(bar + 1);
    const int dir = c.dockS;
    c.id = c.id.substr(0, bar);
    for (std::size_t i = 0; i < mRails.size(); ++i)
      if (mRails[i].id == rail)
        c.dockRail = int(i);
    if (c.dockRail < 0)
    {
      std::fprintf(stderr, "cart %s: no rail %s\n", c.id.c_str(), rail.c_str());
      c.lost = -1;
      continue;
    }
    // Where on its rail the cart stands.
    const Rail& r = mRails[std::size_t(c.dockRail)];
    int best = 0;
    float bestD = 1e9f;
    for (int s = 0; s <= r.len; s += 2)
    {
      float x = 0, y = 0, a = 0;
      railPoint(r, s, x, y, a);
      const float d = std::fabs(x - c.fx) + std::fabs(y - c.fy);
      if (d < bestD)
      {
        bestD = d;
        best = s;
      }
    }
    c.dockS = best;
    dockCart(c);
    c.dir = dir;
  }
  // Bandits: their rail's ends and row.
  for (auto& e : mEnemies)
    if (e.kind == EnemyKind::Bandit && e.platform >= 0)
    {
      const Rail& r = mRails[std::size_t(e.platform)];
      int x0 = r.pts.front().first, x1 = r.pts.back().first;
      if (x0 > x1)
        std::swap(x0, x1);
      e.railX1 = x1 - e.w / 2;
      e.aimY = r.pts.front().second;
      e.y = e.prevY = e.aimY - 1;
      e.railX0 = std::max(x0 - e.w / 2, e.railX0);
      e.x = e.prevX = e.railX0;
    }
}

// --- Track geometry --------------------------------------------------------------

void World::railPoint(const Rail& r, int s, float& x, float& y, float& angle, int* seg) const
{
  s = std::clamp(s, 0, r.len);
  int at = 0;
  for (std::size_t i = 1; i < r.pts.size(); ++i)
  {
    const auto& a = r.pts[i - 1];
    const auto& b = r.pts[i];
    const int L = segLen(a, b);
    if (s <= at + L || i + 1 == r.pts.size())
    {
      const float t = std::clamp(float(s - at) / float(L), 0.0f, 1.0f);
      x = float(a.first) + float(b.first - a.first) * t;
      y = float(a.second) + float(b.second - a.second) * t;
      angle = std::atan2(float(b.second - a.second), float(b.first - a.first));
      if (seg)
        *seg = int(i) - 1;
      return;
    }
    at += L;
  }
  x = float(r.pts.front().first);
  y = float(r.pts.front().second);
  angle = 0.0f;
}

bool World::overGap(const Rail& r, float x) const
{
  for (const auto& [a, b] : r.gaps)
    if (x >= float(a) && x < float(b + 1))
      return true;
  return false;
}

bool World::railY(const Rail& r, float x, float& y) const
{
  if (r.resets || overGap(r, x))
    return false;
  for (std::size_t i = 1; i < r.pts.size(); ++i)
  {
    const auto& a = r.pts[i - 1];
    const auto& b = r.pts[i];
    const float x0 = float(std::min(a.first, b.first)), x1 = float(std::max(a.first, b.first));
    if (x < x0 - 0.5f || x > x1 + 0.5f || a.first == b.first)
      continue;
    const float t = std::clamp((x - float(a.first)) / float(b.first - a.first), 0.0f, 1.0f);
    y = float(a.second) + float(b.second - a.second) * t;
    return true;
  }
  return false;
}

// The rail (other than `skip`) passing within a cell of (x, y), and where on
// it; -1 if none. Loops and gaps do not count.
int World::railNear(float x, float y, int skip, int& s) const
{
  for (std::size_t i = 0; i < mRails.size(); ++i)
  {
    if (int(i) == skip)
      continue;
    const Rail& r = mRails[i];
    float ty = 0;
    if (!railY(r, x, ty) || std::fabs(ty - y) > 1.01f)
      continue;
    // Find s: walk the rail in half cells.
    float bestD = 1e9f;
    for (int k = 0; k <= r.len; k += 4)
    {
      float px = 0, py = 0, a = 0;
      railPoint(r, k, px, py, a);
      const float d = std::fabs(px - x) + std::fabs(py - y);
      if (d < bestD)
      {
        bestD = d;
        s = k;
      }
    }
    return int(i);
  }
  return -1;
}

// --- Carts ---------------------------------------------------------------------------

void World::placeCart(Cart& c)
{
  if (c.rail < 0 || c.falling)
    return;
  const Rail& r = mRails[std::size_t(c.rail)];
  float x = 0, y = 0, a = 0;
  railPoint(r, c.s, x, y, a);
  int lift = c.hop > 0 ? kHop[std::clamp(c.hop - 1, 0, kHopFrames - 1)] : 0;
  // Not through a tunnel's roof: the hop stops where it would hit rock.
  while (lift > 0)
  {
    const int top = int(std::lround(y)) - lift - (mPlayer.cart >= 0 && &mCarts[std::size_t(mPlayer.cart)] == &c ? 6 : 3);
    const CellBox b{int(std::lround(x)) - 2, top, Cart::kW, int(std::lround(y)) - lift - top};
    if (!mMap.overlapsSolid(b))
      break;
    --lift;
  }
  c.fx = x;
  c.fy = y - float(lift);
  c.angle = a;
}

void World::dockCart(Cart& c)
{
  c.rail = c.dockRail;
  c.s = c.dockS;
  c.speed = 0;
  c.hop = 0;
  c.falling = false;
  c.lost = 0;
  c.vx = c.vy = 0.0f;
  placeCart(c);
  c.prevFx = c.fx;
  c.prevFy = c.fy;
  c.prevAngle = c.angle;
}

void World::loseCart(Cart& c)
{
  const bool ridden = mPlayer.cart >= 0 && &mCarts[std::size_t(mPlayer.cart)] == &c;
  if (ridden)
    leaveCart(false, 0);
  const Vec2 at{c.fx * float(kCellSize), (c.fy - 1.5f) * float(kCellSize)};
  burst(at, rgb(170, 140, 100), rgb(90, 70, 50), 16, 1.8f);
  playSound(Sfx::Clunk);
  c.lost = kLostFrames;
  c.falling = false;
  c.speed = 0;
  c.hop = 0;
  crashed();
}

void World::crashed()
{
  // The sign goes back to 0.
  mDays = 0;
  mDaysFrames = 0;
}

void World::leaveCart(bool jumpOut, int fling)
{
  auto& p = mPlayer;
  if (p.cart < 0)
    return;
  p.cart = -1;
  p.cartDuck = false;
  p.stance = Stance::Regular;
  if (jumpOut)
  {
    jump();
    p.fling = fling;
  }
  else
  {
    p.state = PlayerState::Falling;
    p.frames = 0;
    setVisual(PlayerVisual::Falling);
  }
}

void World::placeRider()
{
  auto& p = mPlayer;
  if (p.cart < 0)
    return;
  const Cart& c = mCarts[std::size_t(p.cart)];
  const float ux = std::sin(c.angle), uy = -std::cos(c.angle);
  const float off = p.cartDuck ? 0.0f : 1.0f;
  const float px = c.fx + ux * off, py = c.fy + uy * off;
  p.x = int(std::lround(px - 1.5f));
  p.y = int(std::lround(py)) - 1;
}

void World::stepCart(Cart& c, bool ridden)
{
  c.prevFx = c.fx;
  c.prevFy = c.fy;
  c.prevAngle = c.angle;
  if (c.lost != 0)
  {
    if (c.lost > 0 && --c.lost == 0)
    {
      dockCart(c);
      burst({c.fx * float(kCellSize), (c.fy - 1.5f) * float(kCellSize)}, kGold, rgb(255, 255, 255), 10, 1.2f);
    }
    return;
  }
  if (c.rail < 0)
    return;
  if (c.falling)
  {
    c.vy = std::min(c.vy + 0.25f, 2.0f);
    const float nx = c.fx + c.vx, ny = c.fy + c.vy;
    for (std::size_t i = 0; i < mRails.size(); ++i)
    {
      float ty = 0;
      if (!railY(mRails[i], nx, ty) || c.fy > ty + 0.01f || ny < ty)
        continue;
      // Onto the track below, rolling on the way it was going.
      int s = 0;
      if (railNear(nx, ty, -1, s) != int(i))
        continue;
      c.rail = int(i);
      c.s = s;
      c.falling = false;
      c.dir = sgnf(c.vx) * float(railDx(mRails[i], s)) < 0.0f ? -1 : 1;
      c.speed = std::clamp(int(std::fabs(c.vx) * 8.0f), kMinSpeed, kMaxSpeed);
      placeCart(c);
      playSound(Sfx::Rail);
      burst({c.fx * float(kCellSize), c.fy * float(kCellSize)}, rgb(255, 220, 140), rgb(255, 120, 40), 10, 1.4f);
      return;
    }
    const CellBox b = boxAt(int(std::lround(nx)) - 2, int(std::lround(ny)) - 1, Cart::kW, Cart::kH);
    if (mMap.overlapsSolid(b) || ny > float(mMap.height() + 6))
    {
      loseCart(c);
      return;
    }
    c.fx = nx;
    c.fy = ny;
    c.angle *= 0.8f;
    return;
  }
  if (c.speed == 0)
  {
    placeCart(c);
    return;
  }
  const Rail* r = &mRails[std::size_t(c.rail)];
  // Downhill speeds it up, uphill slows it down; Turbo holds top speed.
  {
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0, a = 0;
    railPoint(*r, c.s, x0, y0, a);
    railPoint(*r, c.s + c.dir * 8, x1, y1, a);
    if (ridden && mPlayer.turbo > 0)
      c.speed = kMaxSpeed;
    else if (c.hop == 0 && y1 - y0 > 0.05f)
      ++c.speed;
    else if (c.hop == 0 && y1 - y0 < -0.05f)
      --c.speed;
    c.speed = std::clamp(c.speed, kMinSpeed, kMaxSpeed);
  }
  int ns = c.s + c.dir * c.speed;
  // A junction: a branch rail starting on this one, its lever set for it.
  for (std::size_t bi = 0; bi < mRails.size(); ++bi)
  {
    const Rail& br = mRails[bi];
    if (int(bi) == c.rail || br.lever < 0 || mLevers[std::size_t(br.lever)].state != br.state)
      continue;
    int sb = 0;
    float ty = 0;
    if (!railY(*r, float(br.pts.front().first), ty) || std::abs(ty - float(br.pts.front().second)) > 1.01f)
      continue;
    // Where the branch leaves this rail.
    float bestD = 1e9f;
    for (int k = 0; k <= r->len; k += 2)
    {
      float px = 0, py = 0, a = 0;
      railPoint(*r, k, px, py, a);
      const float d = std::fabs(px - float(br.pts.front().first)) + std::fabs(py - float(br.pts.front().second));
      if (d < bestD)
      {
        bestD = d;
        sb = k;
      }
    }
    const bool crosses = c.dir > 0 ? (c.s < sb && ns >= sb) : (c.s > sb && ns <= sb);
    const int bdx = sgn(br.pts[1].first - br.pts[0].first);
    if (!crosses || c.dir * railDx(*r, sb) != bdx)
      continue;
    c.rail = int(bi);
    c.dir = 1;
    ns = std::abs(ns - sb);
    r = &mRails[bi];
    playSound(Sfx::Clunk);
    break;
  }
  if (ns < 0 || ns > r->len)
  {
    const int endS = ns < 0 ? 0 : r->len;
    const int left = ns < 0 ? -ns : ns - r->len;
    float ex = 0, ey = 0, ea = 0;
    railPoint(*r, endS, ex, ey, ea);
    const float xdir = float(c.dir * railDx(*r, endS));
    // A bumper at the end of the line.
    for (const auto& bump : mBumpers)
    {
      const CellBox near{int(ex) - 3, int(ey) - 3, 6, 4};
      if (!near.intersects(bump))
        continue;
      c.s = endS;
      c.hop = 0;
      const bool crash = ridden && c.speed > kCrashSpeed && mPlayer.turbo == 0;
      c.speed = 0;
      placeCart(c);
      playSound(Sfx::Clunk);
      mCamera.shake(crash ? 8 : 3, crash ? 1.2f : 0.5f);
      if (crash)
      {
        placeRider();
        leaveCart(true, int(xdir));
        crashed();
        showMessage("CRASH! THROWN OUT OF THE CART");
      }
      return;
    }
    // Onto the rail that carries on from here (a branch joining back).
    int s2 = 0;
    const int next = railNear(ex, ey, c.rail, s2);
    if (next >= 0)
    {
      if (r->resets && r->lever >= 0)
        mLevers[std::size_t(r->lever)].state = 0;
      const Rail& nr = mRails[std::size_t(next)];
      c.rail = next;
      c.dir = xdir * float(railDx(nr, s2)) < 0.0f ? -1 : 1;
      c.s = std::clamp(s2 + c.dir * left, 0, nr.len);
      placeCart(c);
      return;
    }
    // Off the end: it falls.
    c.s = endS;
    placeCart(c);
    c.falling = true;
    c.hop = 0;
    c.vx = xdir * float(c.speed) / 8.0f;
    c.vy = 0.0f;
    return;
  }
  c.s = ns;
  if (c.hop > 0 && ++c.hop > kHopFrames)
  {
    c.hop = 0;
    playSound(Sfx::Rail);
  }
  placeCart(c);
  // Into a gap without hopping: it falls.
  if (c.hop == 0 && overGap(*r, c.fx))
  {
    c.falling = true;
    c.vx = float(c.dir * railDx(*r, c.s)) * float(c.speed) / 8.0f;
    c.vy = 0.0f;
  }
}

void World::updateRide(int mvX, int mvY, const PlayerInput& in)
{
  auto& p = mPlayer;
  Cart& c = mCarts[std::size_t(p.cart)];
  if (mvX != 0)
    p.facing = mvX;
  if (in.jump.triggered)
  {
    if (c.speed == 0 || c.falling)
    {
      leaveCart(true, 0); // climb out
      return;
    }
    if (c.hop == 0)
    {
      c.hop = 1;
      playSound(Sfx::Jump);
    }
    else
    {
      // Bail out in mid-air: a normal jump, the cart carries on.
      const Rail& r = mRails[std::size_t(c.rail)];
      leaveCart(true, c.dir * railDx(r, c.s));
      return;
    }
  }
  stepCart(c, true);
  if (p.cart < 0)
    return;
  p.cartDuck = mvY > 0;
  p.stance = p.cartDuck ? Stance::Crouched : (mvY < 0 ? Stance::Up : Stance::Regular);
  p.state = PlayerState::OnGround;
  p.frames = 0;
  setVisual(p.cartDuck ? PlayerVisual::Crouching : (mvY < 0 ? PlayerVisual::LookingUp : PlayerVisual::Standing));
  placeRider();
}

void World::boardCarts()
{
  auto& p = mPlayer;
  if (p.cart >= 0 || p.state == PlayerState::Dying)
    return;
  for (std::size_t i = 0; i < mCarts.size(); ++i)
  {
    Cart& c = mCarts[i];
    if (c.lost != 0 || c.rail < 0 || c.falling)
      continue;
    const CellBox b = c.box();
    const int overlap = std::min(p.x + Player::kWidth, b.x + b.w) - std::max(p.x, b.x);
    if (overlap < 2 || p.y < b.top() - 1 || p.y > b.bottom() + 1)
      continue;
    p.cart = int(i);
    if (c.speed == 0)
    {
      // A still cart sets off the way you face.
      const Rail& r = mRails[std::size_t(c.rail)];
      c.speed = kMinSpeed;
      c.dir = p.facing * railDx(r, c.s) < 0 ? -1 : 1;
    }
    p.state = PlayerState::OnGround;
    p.frames = 0;
    p.fling = 0;
    p.vineArc = false;
    setVisual(PlayerVisual::Standing);
    placeRider();
    playSound(Sfx::Land);
    return;
  }
}

void World::resetMine()
{
  mPlayer.cart = -1;
  mPlayer.cartDuck = false;
  for (auto& c : mCarts)
    if (c.dockRail >= 0)
      dockCart(c);
  mCaps.clear();
  for (auto& e : mEnemies)
    if (e.kind == EnemyKind::Mole && e.alive)
    {
      e.attach = 0;
      e.hidden = true;
      e.x = e.railX0;
      e.y = e.railX1;
      e.dive = 30;
    }
}

// --- Per frame ------------------------------------------------------------------------

void World::updateMine(const PlayerInput& /*input*/)
{
  if (mRails.empty() && mCaps.empty() && mRespawns.empty() && mTrapdoors.empty() && mVeins.empty() && mRubble.empty())
    return;
  auto& p = mPlayer;
  if (p.cart >= 0 && (p.state == PlayerState::Dying || p.state == PlayerState::Teleporting))
  {
    p.cart = -1;
    p.cartDuck = false;
  }
  for (std::size_t i = 0; i < mCarts.size(); ++i)
    if (int(i) != p.cart)
      stepCart(mCarts[i], false);
  for (auto& l : mLevers)
    if (l.cool > 0)
      --l.cool;
  updateCaps();

  // The cursed veins: touch them and you catch the virus.
  if (p.state != PlayerState::Dying && p.virus == 0)
  {
    CellBox reach = p.box();
    reach.y -= 1;
    reach.h += 1;
    for (const auto& v : mVeins)
      if (reach.intersects(v))
      {
        infect();
        burst({(float(p.x) + 1.5f) * kCellSize, float(v.bottom()) * kCellSize}, kGold, rgb(140, 255, 70), 12, 1.4f);
        break;
      }
  }
  // The W box that comes back.
  for (auto& rs : mRespawns)
  {
    ItemBox& box = mBoxes[std::size_t(rs[0])];
    if (box.alive)
    {
      rs[2] = rs[1];
      continue;
    }
    bool waiting = false;
    for (const auto& it : mItems)
      waiting = waiting || (!it.taken && it.kind == box.content);
    if (waiting || --rs[2] > 0)
      continue;
    box.alive = true;
    rs[2] = rs[1];
    burst({(float(box.x) + 1.0f) * kCellSize, (float(box.y) - 1.0f) * kCellSize}, rgb(120, 255, 120),
      rgb(255, 255, 255), 14, 1.4f);
  }
  // Rocco's trapdoor.
  if (mCharacterIndex == 1 && p.state == PlayerState::OnGround && p.cart < 0)
    for (auto& t : mTrapdoors)
      if (!t.open && p.y + 1 == t.y * kCellsPerTile && p.x + 2 >= t.x0 * kCellsPerTile &&
          p.x <= (t.x1 + 1) * kCellsPerTile - 1)
      {
        t.open = true;
        for (int tx = t.x0; tx <= t.x1; ++tx)
          mMap.setBlock(tx, t.y, Tile::Empty);
        burst({float((t.x0 + t.x1 + 1) * kCellsPerTile) * 0.5f * kCellSize, float(t.y * kCellsPerTile) * kCellSize},
          kTimber, rgb(90, 60, 30), 18, 1.8f);
        playSound(Sfx::Clunk);
        showMessage("THE TRAPDOOR GIVES WAY");
      }
  // The foreman's floor: blown out, it lands as a step of rubble.
  for (auto& rb : mRubble)
    if (!rb[3] && rb[0] >= 0 && std::size_t(rb[0]) < mBreakables.size() && mBreakables[std::size_t(rb[0])].broken)
    {
      rb[3] = 1;
      mMap.setBlock(rb[1], rb[2], Tile::Solid);
      burst({(float(rb[1]) + 0.5f) * kTilePx / kPixelScale, (float(rb[2]) + 0.5f) * kTilePx / kPixelScale},
        rgb(170, 140, 100), rgb(90, 70, 50), 14, 1.6f);
    }
  // DAYS WITHOUT ACCIDENT.
  if (mDaysSign.w > 0 && ++mDaysFrames >= kDaysEvery)
  {
    mDaysFrames = 0;
    if (mDays < 999)
      ++mDays;
  }
}

// --- Blasting Caps -----------------------------------------------------------------

void World::throwCap(int ox, int oy)
{
  const auto& p = mPlayer;
  Cap c;
  c.fx = c.prevFx = float(ox) + 0.5f;
  c.fy = c.prevFy = float(oy) + 1.0f;
  const float f = float(p.facing);
  switch (p.stance)
  {
    case Stance::Up:
      c.vx = 0.4f * f;
      c.vy = -2.2f;
      break;
    case Stance::Crouched:
      c.vx = 0.5f * f; // set down: it rolls a little way
      c.vy = -0.3f;
      break;
    case Stance::Down:
    case Stance::Jetpack:
      c.vx = 0.0f;
      c.vy = 0.5f;
      break;
    default:
      c.vx = 1.3f * f;
      c.vy = -1.1f;
      break;
  }
  // Thrown from a moving cart, it keeps the cart's speed.
  if (p.cart >= 0)
  {
    const Cart& cart = mCarts[std::size_t(p.cart)];
    if (cart.rail >= 0 && !cart.falling)
      c.vx += float(cart.dir * railDx(mRails[std::size_t(cart.rail)], cart.s)) * float(cart.speed) / 8.0f;
  }
  // Not inside a wall.
  if (mMap.solid(int(std::floor(c.fx)), int(std::floor(c.fy - 0.5f))))
    c.fx = float(p.x) + 1.5f;
  c.fuse = kCapFuse;
  mCaps.push_back(c);
  playSound(Sfx::Fuse);
}

void World::blowCap(Cap& cap)
{
  cap.alive = false;
  const int cx = int(std::floor(cap.fx)), cy = int(std::floor(cap.fy - 0.5f));
  explodeAt(cx, cy, kBlast, 8);
  const CellBox area{cx - kBlast, cy - kBlast, kBlast * 2 + 1, kBlast * 2 + 1};
  for (std::size_t i = 0; i < mBreakables.size(); ++i)
  {
    const auto& b = mBreakables[i];
    const CellBox box{b.x0 * kCellsPerTile, b.y0 * kCellsPerTile, (b.x1 - b.x0 + 1) * kCellsPerTile,
      (b.y1 - b.y0 + 1) * kCellsPerTile};
    if (!b.broken && (b.by == 0 || b.by == 1 || b.by == 2) && box.intersects(area))
      hitBreakable(box, 99, 1);
  }
  // Too close: it costs a heart (nothing in Turbo).
  auto& p = mPlayer;
  if (p.state != PlayerState::Dying && p.box().intersects(area))
    hurtPlayer(1);
}

void World::updateCaps()
{
  if (mCaps.empty())
    return;
  for (auto& c : mCaps)
  {
    if (!c.alive)
      continue;
    c.prevFx = c.fx;
    c.prevFy = c.fy;
    if (--c.fuse <= 0)
    {
      blowCap(c);
      continue;
    }
    // A cap landing in a Bandit's cart.
    for (auto& e : mEnemies)
    {
      if (!e.alive || e.hidden || e.kind != EnemyKind::Bandit || c.vy <= 0.0f)
        continue;
      const CellBox cartBox{e.x, e.y - 2, e.w, 3};
      if (cartBox.intersects({int(std::floor(c.fx)), int(std::floor(c.fy - 0.5f)), 1, 1}))
      {
        killEnemy(e);
        if (c.alive)
          blowCap(c);
        break;
      }
    }
    if (!c.alive)
      continue;
    if (c.rail >= 0)
    {
      // Rolling along a rail at 3 cells a frame.
      const Rail& r = mRails[std::size_t(c.rail)];
      c.rs += c.rdir * 24;
      float x = 0, y = 0, a = 0;
      railPoint(r, c.rs, x, y, a);
      if (c.rs < 0 || c.rs > r.len || overGap(r, x))
      {
        c.vx = float(c.rdir * railDx(r, std::clamp(c.rs, 0, r.len))) * 1.5f;
        c.vy = 0.0f;
        c.rail = -1;
        continue;
      }
      c.fx = x;
      c.fy = y;
      continue;
    }
    c.vy = std::min(2.5f, c.vy + kCapGravity);
    const int steps = std::max(1, int(std::ceil(std::max(std::fabs(c.vx), std::fabs(c.vy)) / 0.5f)));
    for (int k = 0; k < steps && c.alive; ++k)
    {
      // Sideways: walls send it back at half speed.
      const float nx = c.fx + c.vx / float(steps);
      if (mMap.solid(int(std::floor(nx)), int(std::floor(c.fy - 0.5f))))
        c.vx = -c.vx * 0.5f;
      else
        c.fx = nx;
      const float ny = c.fy + c.vy / float(steps);
      if (c.vy > 0.0f)
      {
        const int row = int(std::floor(ny));
        if (row >= int(std::floor(c.fy)) && mMap.solidTop(int(std::floor(c.fx)), row) &&
            !mMap.solid(int(std::floor(c.fx)), int(std::floor(c.fy)) - 1))
        {
          c.fy = float(row);
          // On a rail: it rolls along it, downhill or the way it was going.
          int s = 0;
          const int ri = railNear(c.fx, c.fy, -1, s);
          if (ri >= 0)
          {
            float x0 = 0, y0 = 0, x1 = 0, y1 = 0, a = 0;
            railPoint(mRails[std::size_t(ri)], s - 8, x0, y0, a);
            railPoint(mRails[std::size_t(ri)], s + 8, x1, y1, a);
            c.rail = ri;
            c.rs = s;
            const int dx = railDx(mRails[std::size_t(ri)], s);
            int way = std::fabs(y1 - y0) > 0.1f ? (y1 > y0 ? 1 : -1) : (int(sgnf(c.vx)) * dx);
            c.rdir = way == 0 ? 1 : way;
            c.vx = c.vy = 0.0f;
            break;
          }
          if (c.bounces < 2 && c.vy > 0.6f)
          {
            ++c.bounces;
            c.vy = -c.vy * 0.5f;
            c.vx *= 0.5f;
            playSound(Sfx::Land);
          }
          else
          {
            c.vy = 0.0f;
            c.vx *= 0.8f; // rolling to a stop
          }
          break;
        }
        c.fy = ny;
      }
      else
      {
        if (mMap.solid(int(std::floor(c.fx)), int(std::floor(ny - 0.5f))))
          c.vy = 0.0f;
        else
          c.fy = ny;
      }
    }
    if (c.fy > float(mMap.height() + 4))
      c.alive = false;
  }
  mCaps.erase(std::remove_if(mCaps.begin(), mCaps.end(), [](const Cap& c) { return !c.alive; }), mCaps.end());
}

bool World::shotAtMine(Projectile& /*pr*/, const CellBox& b)
{
  for (auto& l : mLevers)
  {
    if (!l.box().intersects(b))
      continue;
    if (l.cool == 0)
    {
      l.state = (l.state + 1) % l.states;
      l.cool = 8;
      playSound(Sfx::Clunk);
      burst(cellCenter(l.box()), rgb(255, 230, 160), kRust, 8, 1.2f);
    }
    return true;
  }
  return false;
}

// --- Enemies ---------------------------------------------------------------------------

void World::updateBandit(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  if (e.hidden)
  {
    // Rolls in when it is its turn (the one before it is gone) and the
    // runner is coming up to where it enters.
    if (e.dive == -2)
      return;
    for (const auto& o : mEnemies)
      if (o.kind == EnemyKind::Bandit && o.attach < e.attach && o.alive)
        return;
    if (p.x + 24 < e.railX0 || p.x > e.railX1)
      return;
    e.hidden = false;
    e.x = e.prevX = e.railX0;
    e.drawSnap = true;
    e.timer = 0;
    playSound(Sfx::Rail);
    return;
  }
  // Level with the runner (within 3 blocks), 1 to 3 cells a frame.
  const int target = p.x + 1 - e.w / 2;
  const int gap = target - e.x;
  int step = 0;
  if (std::abs(gap) > 6)
    step = std::clamp(gap / 3, -3, 3);
  else if (gap != 0)
    step = sgn(gap);
  e.x += step;
  e.dir = gap < 0 ? -1 : 1;
  if (e.x >= e.railX1)
  {
    // Off the end of its rail: gone, and the next one can come.
    e.alive = false;
    e.hidden = true;
    e.dive = -2;
    return;
  }
  e.x = std::max(e.x, e.railX0 - 8);
  // Pistol up for 10 frames, then a shot in one of eight directions.
  if (e.tell > 0)
  {
    if (--e.tell == 0)
    {
      const int fx = e.x + e.w / 2, fy = e.y - 4;
      const float dx = float(p.x + 1 - fx), dy = float(p.y - 2 - fy);
      const float a = std::round(std::atan2(dy, dx) / 0.785398f) * 0.785398f;
      Projectile pr;
      pr.kind = ShotKind::Enemy;
      pr.w = pr.h = 1;
      pr.speed = 1;
      pr.damage = 1;
      pr.range = 60;
      pr.precise = true;
      pr.fx = float(fx);
      pr.fy = float(fy);
      pr.vx = std::cos(a);
      pr.vy = std::sin(a);
      pr.dx = sgn(int(std::lround(pr.vx * 2.0f)));
      pr.dy = sgn(int(std::lround(pr.vy * 2.0f)));
      pr.x = pr.prevX = fx;
      pr.y = pr.prevY = fy;
      mProjectiles.push_back(pr);
      playSound(Sfx::EnemyShot);
      e.dive = def.cooldown;
    }
    return;
  }
  if (e.dive > 0)
    --e.dive;
  if (e.dive <= 0 && isOnScreen(e.box(), 0) && p.state != PlayerState::Dying)
    e.tell = def.tell;
}

void World::updateBat(Enemy& e, const EnemyDef& /*def*/)
{
  // The cloud flies to and fro along its tunnel, a sine up and down.
  const int span = std::max(1, e.railX1 - e.railX0);
  const int t = mStats.frames + e.aimX;
  const int k = t % (span * 2);
  e.x = e.railX0 + (k < span ? k : span * 2 - k);
  e.dir = k < span ? 1 : -1;
  const float ph = 6.2831853f * float(mStats.frames % kBatPeriod) / float(kBatPeriod);
  e.y = e.aimY + int(std::lround(float(e.attach) * std::sin(ph + float(e.aimX) * 0.12f)));
  if (e.timer == 1 && isOnScreen(e.box(), 0))
    playSound(Sfx::Flap);
}

void World::updateMole(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  // attach: 0 in the rock, 1 dust at the exit spot, 2 out.
  if (e.attach == 0)
  {
    e.hidden = true;
    if (e.dive > 0)
    {
      --e.dive;
      return;
    }
    const int hx = e.railX0, hy = e.railX1;
    if (std::abs(p.x - hx) > def.range || std::abs(p.y - hy) > def.range || p.state == PlayerState::Dying)
      return;
    // One mole out at a time, and never while a Bandit is about.
    for (const auto& o : mEnemies)
    {
      if (&o == &e || !o.alive)
        continue;
      if ((o.kind == EnemyKind::Mole && o.attach != 0) || (o.kind == EnemyKind::Bandit && !o.hidden && isOnScreen(o.box(), 0)))
        return;
    }
    // The spot in the rock face nearest the runner, within reach of home.
    int bestX = -1, bestY = -1, bestD = 1 << 30;
    for (int y = hy - def.range; y <= hy + def.range; ++y)
      for (int x = hx - def.range; x <= hx + def.range; ++x)
      {
        const CellBox b = boxAt(x, y, e.w, e.h);
        if (mMap.overlapsSolid(b) || !mMap.solid(x + 1, y + 1))
          continue;
        const bool face = mMap.solid(x - 1, y) || mMap.solid(x + e.w, y);
        if (!face)
          continue;
        const int d = std::abs(x - p.x) + std::abs(y - p.y);
        if (d < 6 || b.intersects(p.box()))
          continue;
        if (d < bestD)
        {
          bestD = d;
          bestX = x;
          bestY = y;
        }
      }
    if (bestX < 0)
      return;
    e.x = e.prevX = bestX;
    e.y = e.prevY = bestY;
    e.drawSnap = true;
    e.attach = 1;
    e.tell = kMoleTell;
    return;
  }
  if (e.attach == 1)
  {
    if (e.tell % 4 == 0)
      burst({(float(e.x) + 1.5f) * kCellSize, (float(e.y) - 2.0f) * kCellSize}, rgb(170, 140, 100), rgb(100, 80, 60), 3,
        0.6f, false);
    if (--e.tell > 0)
      return;
    e.attach = 2;
    e.hidden = false;
    e.timer = 0;
    playSound(Sfx::Dig);
    burst(cellCenter(e.box()), rgb(170, 140, 100), rgb(90, 70, 50), 14, 1.6f);
    return;
  }
  // Out: a rock lobbed at the runner, then back into the rock.
  e.dir = p.x < e.x ? -1 : 1;
  if (e.timer == 10 && p.state != PlayerState::Dying)
  {
    const CellBox pb = p.box(), b = e.box();
    const float g = 0.15f;
    const float sx = float(b.x + 1), sy = float(b.y);
    const float tx = float(pb.x + 1), ty = float(pb.bottom() - 1);
    const float T = std::clamp(std::fabs(tx - sx) / 1.2f, 8.0f, 18.0f);
    Projectile pr;
    pr.kind = ShotKind::Enemy;
    pr.w = pr.h = 1;
    pr.speed = 1;
    pr.damage = 1;
    pr.precise = true;
    pr.range = 80;
    pr.gy = g;
    pr.fx = sx;
    pr.fy = sy;
    pr.vx = (tx - sx) / T;
    pr.vy = (ty - sy - 0.5f * g * T * T) / T;
    pr.dx = e.dir;
    pr.x = pr.prevX = int(sx);
    pr.y = pr.prevY = int(sy);
    mProjectiles.push_back(pr);
    playSound(Sfx::EnemyShot);
  }
  if (e.timer >= kMoleOut)
  {
    e.attach = 0;
    e.hidden = true;
    e.dive = def.cooldown;
    burst(cellCenter(e.box()), rgb(170, 140, 100), rgb(90, 70, 50), 10, 1.2f);
    playSound(Sfx::Dig);
  }
}

} // namespace gr
