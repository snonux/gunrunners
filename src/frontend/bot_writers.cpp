// The bot in Level 38. The planner walks the writers' route
// (Planner::writersGoal): spots before each thing in the way, then the
// keyboard. These rules step in: strike any word in the way ahead (a wall,
// the spikes, the guard, a Writer's Block, the crusher, the door; never a
// load-bearing FLOOR or LADDER), shoot a Typo coming close, and type the
// bridge sentence by landing on the keys. Breakout is played by aiming the
// paddle under the ball.

#include "frontend/bot.hpp"
#include "frontend/planner.hpp"
#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace gr
{

namespace
{

int sgn(int v) { return (v > 0) - (v < 0); }

} // namespace

namespace
{

// Breakout's ball, stepped as the world steps it (walls, the ceiling, the
// bricks that are left, each as if it broke) until it comes down to `row`
// or hits a brick (when `stopAtBrick`). Returns the quarter steps taken.
struct Ball
{
  float x, y, vx, vy;
  bool brick = false;
  int hits = 0; // bricks hit
};

int flyBall(const World& w, Ball& b, std::vector<char>& gone, int row, bool stopAtBrick)
{
  const auto& s = w.writers();
  const auto& map = w.map();
  for (int k = 0; k < 4000; ++k)
  {
    const float nx = b.x + b.vx * 0.25f, ny = b.y + b.vy * 0.25f;
    const int cx = int(std::floor(nx)), cy = int(std::floor(ny));
    if (b.vy > 0 && cy >= row)
      return k;
    bool bounced = false;
    for (std::size_t i = 0; i < s.bricks.size() && !bounced; ++i)
    {
      const Brick& br = s.bricks[i];
      if (!br.alive || gone[i] || cx < br.x || cx >= br.x + 4 || cy < br.y || cy >= br.y + 2)
        continue;
      const int ox = int(std::floor(b.x)), oy = int(std::floor(b.y));
      if (oy < br.y || oy >= br.y + 2)
        b.vy = -b.vy;
      else if (ox < br.x || ox >= br.x + 4)
        b.vx = -b.vx;
      else
        b.vy = -b.vy;
      gone[i] = br.hp <= 1;
      bounced = true;
    }
    if (bounced)
    {
      b.brick = true;
      ++b.hits;
      if (stopAtBrick)
        return k;
      continue;
    }
    if (map.solid(cx, cy))
    {
      const int ox = int(std::floor(b.x)), oy = int(std::floor(b.y));
      const bool side = map.solid(cx, oy), top = map.solid(ox, cy);
      if (side || !top)
        b.vx = -b.vx;
      if (top || !side)
        b.vy = -b.vy;
      continue;
    }
    b.x = nx;
    b.y = ny;
  }
  return 4000;
}

} // namespace

Input Bot::breakout(const World& w)
{
  const auto& s = w.writers();
  const auto& p = w.player();
  Input in;
  if (!s.ball)
  {
    in.fire = !mPenPrev;
    mPenPrev = in.fire;
    return in;
  }
  mPenPrev = false;
  // Where the ball comes down to the paddle.
  const int row = p.y - 4;
  std::vector<char> gone(s.bricks.size(), 0);
  Ball ball{s.bx, s.by, s.bvx, s.bvy};
  const int steps = flyBall(w, ball, gone, row, false);
  const float landX = ball.x + ball.vx * 0.25f, landY = ball.y + ball.vy * 0.25f;
  const int land = int(std::floor(landX));
  // Each spot under it the paddle can reach in time: where the return goes
  // (the world's 30, 45 or 60 degrees from where it hits). Take the one
  // that hits the most bricks for its time before it comes back down.
  const int frames = steps / 4;
  int want = land - 1;
  float best = -1.0f;
  for (int px = land - 3; px <= land + 1; ++px)
  {
    const int far = std::abs(px - p.x);
    if (far > frames * 3 / 2)
      continue;
    const float rel = (landX - (float(px) + 1.5f)) / 2.5f;
    const float a = std::abs(rel) < 0.34f ? 1.047f : std::abs(rel) < 0.75f ? 0.785f : 0.524f;
    const float dir = rel < -0.05f ? -1.0f : rel > 0.05f ? 1.0f : (ball.vx < 0 ? -1.0f : 1.0f);
    Ball back{landX, landY, dir * 2.0f * std::cos(a), -2.0f * std::sin(a)};
    std::vector<char> g = gone;
    const int t = flyBall(w, back, g, row, false);
    const float score = float(back.hits) / float(t + 40) - 0.0001f * float(far);
    if (score > best)
    {
      best = score;
      want = px;
    }
  }
  // The paddle moves two cells at a time.
  if (p.x < want - 1 || (p.x < want && frames <= 2))
    in.right = true;
  else if (p.x > want + 1 || (p.x > want && frames <= 2))
    in.left = true;
  return in;
}

bool Bot::writers(const World& w, Input& in)
{
  const auto& s = w.writers();
  const auto& p = w.player();
  if (s.breakout)
  {
    in = breakout(w);
    return true;
  }
  if (p.state == PlayerState::Dying || p.state == PlayerState::Teleporting)
    return false;
  const int step = writersStep(w);
  const int mid = p.x + 1;
  int goalX = w.level().exitTx * kCellsPerTile;
  if (step >= 0)
    goalX = s.route[std::size_t(step)].x;
  int dir = sgn(goalX - mid);
  if (dir == 0)
    dir = p.facing;
  const int floor = w.writersFloor(p.y);
  const bool pen = p.weapon == Weapon::Proto && p.proto == int(ProtoId::RedPen) && p.ammo > 0;
  const bool ground = p.state == PlayerState::OnGround;
  auto shoot = [&](int at, bool crouch) {
    if (p.facing != at)
    {
      (at > 0 ? in.right : in.left) = true; // turning round costs a frame, no step
      mPenPrev = false;
      return;
    }
    in.down = crouch;
    in.fire = !mPenPrev;
    mPenPrev = in.fire;
  };
  // A word in the way ahead, at the height a standing shot flies.
  if (pen && ground)
    for (const auto& wd : s.words)
    {
      if (wd.load || wd.target == WordTarget::Egg || w.writersFloor(wd.y) != floor)
        continue;
      if (p.y - 2 < wd.y || p.y - 2 > wd.y + 1)
        continue;
      const int dx = wd.x + wd.w / 2 - mid;
      if (sgn(dx) != dir || std::abs(dx) > 24)
        continue;
      // Shots die off screen: walk on till the word's near end is in view.
      const int camX = int(w.camera().x());
      if (dir > 0 ? wd.x > camX + int(kViewCellsW) - 3 : wd.x + wd.w < camX + 3)
        continue;
      if (wd.state == WordState::Intact)
      {
        // One shot a word: wait while it flies (3 cells a frame).
        if (mStrikeWait > 0)
        {
          --mStrikeWait;
          return true;
        }
        shoot(dir, false);
        if (in.fire)
          mStrikeWait = std::abs(dx) / 3 + 4;
        return true;
      }
      if (wd.state == WordState::Striking)
        return true; // wait for it to go
    }
  const bool typing = step >= 0 && s.route[std::size_t(step)].act == "type" && floor == 0 && !s.keys.empty() &&
                      mid >= s.keys.front().x * kCellsPerTile - 8 &&
                      mid <= (s.keys.back().x + 2) * kCellsPerTile + 8 &&
                      s.bridge.typed < int(s.bridge.text.size());
  // A Typo coming close on this floor: a crouched shot (they are 2 cells
  // tall), when nothing (a key) is in the way of it.
  if (ground && !typing)
    for (const auto& t : s.typos)
    {
      const Enemy& e = w.enemies()[std::size_t(t.enemy)];
      if (!e.alive || std::abs(e.y - p.y) > 1)
        continue;
      const int dx = e.x + 1 - mid;
      if (std::abs(dx) > 14 || std::abs(dx) < 2)
        continue;
      bool clear = true;
      for (int x = std::min(mid, e.x + 1); x <= std::max(mid, e.x + 1) && clear; ++x)
        clear = !w.map().solid(x, p.y - 1);
      if (!clear)
        continue;
      shoot(sgn(dx), true);
      return true;
    }
  mPenPrev = false;
  mStrikeWait = 0;
  // The keyboard: land on each key the sentence wants next.
  if (typing)
  {
    const char want = s.bridge.text[std::size_t(s.bridge.typed)];
    const TypeKey* key = nullptr;
    for (const auto& k : s.keys)
      if (k.ch == want)
        key = &k;
    if (!key)
      return false;
    const int tx = key->x * kCellsPerTile; // the runner's x with its middle over the key
    if (!ground)
    {
      in.jump = p.state == PlayerState::Jumping;
      if (p.x < tx)
        in.right = true;
      else if (p.x > tx)
        in.left = true;
      mTypePrevJump = in.jump;
      return true;
    }
    const bool on = p.y + 1 == key->y * kCellsPerTile && mid >= tx && mid <= tx + 3;
    if (on || std::abs(p.x - tx) <= 12)
    {
      in.jump = !mTypePrevJump;
      if (!on && p.x < tx)
        in.right = true;
      else if (!on && p.x > tx)
        in.left = true;
    }
    else
    {
      // Hop along over the keys in between.
      in.jump = !mTypePrevJump;
      (tx > p.x ? in.right : in.left) = true;
    }
    mTypePrevJump = in.jump;
    return true;
  }
  mTypePrevJump = false;
  return false;
}

} // namespace gr
