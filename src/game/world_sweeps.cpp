// Level 41, Sweeps Week: the Audience Vote (ballots at the vote
// checkpoints, the style score, the rules they bring), the Applause
// Cannon's meter, Warm-up Comics and their rubber chickens, Audience
// Plants, Cue Card Drones, turntables and the crowd; the Paintball bonus
// (rules=paint). See sweeps.hpp.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace gr
{

namespace
{

int sgn(int v) { return (v > 0) - (v < 0); }

bool alive(const Player& p) { return p.state != PlayerState::Dying && p.state != PlayerState::Teleporting; }

constexpr unsigned kFriendly[] = {kSwBigHeads, kSwDoubleGems, kSwLowGrav, kSwQuack};
constexpr unsigned kNeutral[] = {kSwTurbo, kSwMirror};
constexpr unsigned kNasty[] = {kSwSlippery, kSwHats, kSwLightsOut, kSwVirus};
constexpr int kVirusVariant = 7; // the vote's V among the items
constexpr float kChickenFall = 0.25f; // cells a frame per frame
constexpr float kChickenUp = 1.5f;    // its throw's first climb

bool isSweepsEnemy(EnemyKind k)
{
  return k == EnemyKind::WarmupComic || k == EnemyKind::AudiencePlant || k == EnemyKind::CueDrone;
}

} // namespace

const char* sweepsRuleName(unsigned bit)
{
  switch (bit)
  {
    case kSwBigHeads:
      return "BIG HEADS";
    case kSwDoubleGems:
      return "DOUBLE GEMS";
    case kSwLowGrav:
      return "LOW GRAVITY";
    case kSwQuack:
      return "QUACK";
    case kSwTurbo:
      return "TURBO FOR EVERYONE";
    case kSwMirror:
      return "MIRROR MODE";
    case kSwConfetti:
      return "???";
    case kSwSlippery:
      return "SLIPPERY FLOORS";
    case kSwHats:
      return "PARTY HATS";
    case kSwLightsOut:
      return "LIGHTS OUT";
    case kSwVirus:
      return "VIRUS FOR THE RUNNER";
    default:
      return "";
  }
}

std::string sweepsOptionName(const SwOption& o)
{
  if (o.cat == SwCat::Fanfare)
  {
    static const char* const kFanfares[] = {"BRASS BAND", "DRUM ROLL", "STANDING OVATION"};
    return kFanfares[std::clamp(o.fanfare, 0, 2)];
  }
  std::string out;
  for (int i = 0; i < kSwRuleCount; ++i)
    if ((o.rules >> i & 1u) != 0)
    {
      if (!out.empty())
        out += " + ";
      out += sweepsRuleName(1u << i);
    }
  return out;
}

// --- Setup ---------------------------------------------------------------------------------

bool World::setupSweepsEntity(const EntityDef& e)
{
  const bool paint = mLevel->rules.find("paint") != std::string::npos;
  if (mLevel->themeKey != "sweeps_week" && !paint)
    return false;
  auto& s = mSweeps;
  s.on = true;
  s.paint = paint;
  if (e.kind == "duck")
    return true; // the duck's sequined bow tie is only drawn
  if (e.kind == "vote" && e.hasPos)
  {
    SwVote v;
    v.id = e.id;
    v.x = e.x * kCellsPerTile;
    v.y = e.y * kCellsPerTile + 1;
    const auto at = e.list("virus");
    if (at.size() == 2)
    {
      v.virusX = at[0] * kCellsPerTile;
      v.virusY = at[1] * kCellsPerTile + 1;
    }
    v.fanfare = e.num("fanfare", 0) != 0;
    s.votes.push_back(v);
    return true;
  }
  if (e.kind == "turntable" && e.hasPos)
  {
    SwTurntable t;
    t.x0 = e.x;
    t.x1 = e.x + std::max(1, e.num("w", 10)) - 1;
    t.row = e.y;
    t.dir = e.str("dir", "r") == "l" ? -1 : 1;
    s.turntables.push_back(t);
    return true;
  }
  if (e.kind == "crowd" && e.hasPos)
  {
    static const char* const kLooks[] = {"fan", "cop", "bouncer", "mannequin", "pilot"};
    SwCrowd c;
    c.x = e.x * kCellsPerTile;
    c.y = e.y * kCellsPerTile + 1;
    const std::string look = e.str("look", "fan");
    for (int i = 0; i < 5; ++i)
      if (look == kLooks[i])
        c.look = i;
    s.crowd.push_back(c);
    return true;
  }
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  if (e.kind == "arena" && e.rect("rect", x0, y0, x1, y1))
  {
    s.arenas.push_back({x0 * kCellsPerTile, y0 * kCellsPerTile, (x1 - x0 + 1) * kCellsPerTile, (y1 - y0 + 1) * kCellsPerTile});
    return true;
  }
  if (e.kind == "springpad" && e.hasPos)
  {
    SwPad pad;
    pad.x0 = e.x;
    pad.x1 = e.x + std::max(1, e.num("w", 2)) - 1;
    pad.row = e.y;
    s.pads.push_back(pad);
    return true;
  }
  if (e.kind == "seatladder" && e.hasPos)
  {
    s.seatX = e.x;
    s.seatY = e.y;
    return true;
  }
  return false;
}

void World::setupSweepsEnemy(Enemy& en, const EntityDef& e)
{
  if (!mSweeps.on || !isSweepsEnemy(en.kind))
    return;
  SwActor a;
  a.enemy = int(&en - mEnemies.data());
  a.homeY = en.y;
  a.card = int(mSweeps.actors.size()) % 3;
  for (const auto& [px, py] : e.path("path"))
    a.path.push_back({px * kCellsPerTile, py * kCellsPerTile + 1});
  if (a.path.empty())
    a.path.push_back({en.x, en.y});
  a.target = a.path.size() > 1 ? 1 : 0;
  mSweeps.actors.push_back(a);
}

int World::sweepsSegment(int cx) const
{
  int seg = 0;
  for (const auto& v : mSweeps.votes)
    if (cx >= v.x)
      ++seg;
  return seg;
}

void World::linkSweeps()
{
  auto& s = mSweeps;
  const bool paint = mLevel->rules.find("paint") != std::string::npos;
  if (mLevel->themeKey == "sweeps_week" || paint)
  {
    s.on = true;
    s.paint = paint;
  }
  if (!s.on)
    return;
  std::stable_sort(s.votes.begin(), s.votes.end(), [](const SwVote& a, const SwVote& b) { return a.x < b.x; });
  for (auto& a : s.actors)
    a.seg = sweepsSegment(mEnemies[std::size_t(a.enemy)].x + 1);
  s.lastHp = mPlayer.hp;
  s.lastGems = mStats.gems;
  s.rules = s.lastWin = s.paint ? 0u : unsigned(kSwBigHeads);
  for (auto& pr : mProps)
    if (pr.kind == PropKind::BonusDoor)
      pr.dormant = true; // until the letter G is taken
  if (!s.paint)
    return;
  // Paintball: every block top with room over it counts; walls only show.
  const int w = mLevel->width, h = mLevel->height;
  s.paints.assign(std::size_t(w * h), 0);
  s.tops = 0;
  auto open = [&](int bx, int by) {
    return bx >= 0 && by >= 0 && bx < w && by < h && mMap.block(bx, by) != Tile::Solid && mMap.block(bx, by) != Tile::Platform;
  };
  for (int by = 1; by < h; ++by)
    for (int bx = 0; bx < w; ++bx)
    {
      const Tile t = mMap.block(bx, by);
      if (t != Tile::Solid && t != Tile::Platform)
        continue;
      auto& c = s.paints[std::size_t(by * w + bx)];
      if (open(bx, by - 1))
      {
        c = 1;
        ++s.tops;
      }
      else if (t == Tile::Solid && (open(bx - 1, by) || open(bx + 1, by) || open(bx, by + 1)))
        c = 3;
    }
  if (mLevel->goal.rfind("paint:", 0) == 0)
    s.goal = std::clamp(std::atoi(mLevel->goal.c_str() + 6), 1, 100);
}

// --- Frames --------------------------------------------------------------------------------

bool World::sweepsMirrored() const
{
  return (mSweeps.rules & kSwMirror) != 0 && mPlayer.turbo == 0;
}

void World::updateSweepsFrames(const PlayerInput& input)
{
  PlayerInput in = input;
  // MIRROR MODE: left is right (a Turbo pick-up puts it back for its 225 frames).
  if (sweepsMirrored())
    std::swap(in.left, in.right);
  updateFrame(in);
  // The exit fanfare the last vote picked (its ballot settles at the exit).
  if (mState == WorldState::Exiting && mSweeps.ballot >= 0 && mSweeps.votes[std::size_t(mSweeps.ballot)].fanfare)
    closeBallot();
  if (mState == WorldState::Exiting && mSweeps.fanfare >= 0 && !mSweeps.played)
  {
    static const Sfx kSounds[] = {Sfx::BrassFanfare, Sfx::DrumRoll, Sfx::Ovation};
    mSweeps.played = true;
    playSound(kSounds[std::clamp(mSweeps.fanfare, 0, 2)]);
    showMessage(sweepsOptionName({0u, SwCat::Fanfare, mSweeps.fanfare, 100}) + "!");
  }
  // TURBO FOR EVERYONE: the whole show at 1.25x, every fourth frame twice.
  if ((mSweeps.rules & kSwTurbo) != 0 && mState == WorldState::Playing && ++mSweeps.boost % 4 == 0)
  {
    in.jump.triggered = false;
    in.fire.triggered = false;
    in.use.triggered = false;
    updateFrame(in);
  }
}

std::uint32_t World::sweepsRand()
{
  auto& r = mSweeps.rng;
  r ^= r << 13;
  r ^= r >> 17;
  r ^= r << 5;
  return r;
}

int World::sweepsStyle() const
{
  const auto& s = mSweeps;
  const int clean = std::max(0, 40 - 10 * s.lost);
  const int streak = 10 * (std::clamp(s.best, 1, kSwMeterMax) - 1);
  int total = 0, dead = 0;
  for (const auto& a : s.actors)
    if (a.seg == s.cur)
    {
      ++total;
      dead += mEnemies[std::size_t(a.enemy)].alive ? 0 : 1;
    }
  const int kills = total > 0 ? 30 * dead / total : 30;
  return std::clamp(clean + streak + kills, 0, 100);
}

// --- The vote --------------------------------------------------------------------------------

void World::openBallot(int k)
{
  auto& s = mSweeps;
  const SwVote& v = s.votes[std::size_t(k)];
  const int style = sweepsStyle();
  int drones = 0;
  for (const auto& a : s.actors)
    if (a.seg == s.cur && mEnemies[std::size_t(a.enemy)].alive && mEnemies[std::size_t(a.enemy)].kind == EnemyKind::CueDrone)
      ++drones;
  s.rng ^= std::uint32_t(mStats.frames + 1) * 0x9E3779B9u;
  sweepsRand();
  s.ballot = k;
  s.ballotT = 0;
  s.style = style;
  applySweepsRules(0); // the old rule ends at the checkpoint
  if (v.fanfare)
  {
    for (int i = 0; i < 3; ++i)
      s.options[i] = {0u, SwCat::Fanfare, i, 100};
    s.winner = int(sweepsRand() % 3u);
  }
  else
  {
    // One rule (two for a friendly double) from a pool, never last segment's winner.
    auto pick = [&](const unsigned* pool, int n, int count) {
      std::vector<unsigned> left;
      for (int i = 0; i < n; ++i)
        if ((pool[i] & s.lastWin) == 0 && (pool[i] != kSwQuack || s.quackOk))
          left.push_back(pool[i]);
      if (left.empty())
        for (int i = 0; i < n; ++i)
          if (pool[i] != kSwQuack || s.quackOk)
            left.push_back(pool[i]);
      unsigned out = 0;
      for (int c = 0; c < count && !left.empty(); ++c)
      {
        const std::size_t j = sweepsRand() % left.size();
        out |= left[j];
        left.erase(left.begin() + std::ptrdiff_t(j));
      }
      return out;
    };
    s.options[0] = {pick(kFriendly, 4, style >= 80 ? 2 : 1), SwCat::Friendly, -1, 100 + 3 * style};
    s.options[1] = {pick(kNeutral, 2, 1), SwCat::Neutral, -1, 100};
    s.options[2] = {pick(kNasty, 4, 1), SwCat::Nasty, -1, 100 + 2 * (100 - style) + 100 * std::min(2, drones)};
    if (sweepsRand() % 10u == 0u)
      s.options[2] = {kSwConfetti, SwCat::Secret, -1, 100}; // ???: counts as neutral
    if (s.options[2].cat == SwCat::Secret && s.meter >= kSwMeterMax)
      s.winner = 2; // the meter at x4: ??? wins outright
    else
    {
      const int sum = s.options[0].weight + s.options[1].weight + s.options[2].weight;
      int roll = int(sweepsRand() % std::uint32_t(sum));
      s.winner = 0;
      while (s.winner < 2 && roll >= s.options[s.winner].weight)
        roll -= s.options[s.winner++].weight;
    }
    if (s.force != 0)
    {
      // Tests: this rule wins, in the slot of its kind.
      const int slot = (s.force & (kSwBigHeads | kSwDoubleGems | kSwLowGrav | kSwQuack)) != 0 ? 0
        : (s.force & (kSwTurbo | kSwMirror)) != 0                                           ? 1
                                                                                            : 2;
      s.options[slot].rules = s.force;
      s.options[slot].cat = s.force == kSwConfetti ? SwCat::Secret : s.options[slot].cat;
      s.winner = slot;
    }
  }
  playSound(Sfx::BallotSting); // the HUD shows the ballot
}

void World::closeBallot()
{
  auto& s = mSweeps;
  auto& v = s.votes[std::size_t(s.ballot)];
  v.done = true;
  s.cur = s.ballot + 1;
  s.ballot = -1;
  const SwOption o = s.options[s.winner];
  s.lost = 0;
  s.best = s.meter;
  if (o.cat == SwCat::Fanfare)
  {
    s.fanfare = o.fanfare;
    s.lastWin = 0;
    return;
  }
  s.lastWin = o.rules;
  applySweepsRules(o.rules);
  for (auto& a : s.actors)
  {
    Enemy& e = mEnemies[std::size_t(a.enemy)];
    if (a.seg != s.cur || !e.alive)
      continue;
    if ((o.rules & kSwHats) != 0 && !a.hat)
    {
      a.hat = true;
      ++e.hp; // PARTY HATS: a hit more each
    }
    if ((o.rules & kSwConfetti) != 0)
      a.popper = true;
  }
  if ((o.rules & kSwVirus) != 0 && v.virusX >= 0)
  {
    Item it;
    it.kind = ItemKind::Virus;
    it.variant = kVirusVariant;
    it.x = it.prevX = v.virusX;
    it.y = it.prevY = v.virusY;
    it.floating = true;
    mItems.push_back(it);
    playSound(Sfx::VirusWarble);
  }
}

void World::applySweepsRules(unsigned rules)
{
  auto& s = mSweeps;
  s.rules = rules;
  mHull.lowgrav = (rules & kSwLowGrav) != 0 || mLevel->flags.count("lowgrav") != 0;
  if ((rules & kSwConfetti) == 0)
    for (auto& a : s.actors)
      a.popper = false;
  if ((rules & kSwVirus) == 0)
    for (auto& it : mItems)
      if (it.kind == ItemKind::Virus && it.variant == kVirusVariant)
        it.taken = true; // the V goes with its rule
}

// --- Every frame ---------------------------------------------------------------------------

SwActor* World::sweepsActor(const Enemy& e)
{
  for (auto& a : mSweeps.actors)
    if (a.enemy == int(&e - mEnemies.data()))
      return &a;
  return nullptr;
}

bool World::sweepsGround(int mvX, int mvY)
{
  auto& s = mSweeps;
  auto& p = mPlayer;
  if ((s.rules & kSwSlippery) == 0 || p.turbo > 0)
  {
    s.slide = s.slideCarry = 0;
    return false;
  }
  // SLIPPERY FLOORS: a quarter cell a frame faster a frame pushing, an
  // eighth slower letting go (the runner slides on about 4 cells).
  if (mvY != 0)
  {
    p.stance = mvY < 0 ? Stance::Up : Stance::Crouched;
    setVisual(mvY < 0 ? PlayerVisual::LookingUp : PlayerVisual::Crouching);
  }
  else
    setVisual(PlayerVisual::Standing);
  if (mvX != 0 && mvY == 0)
  {
    if (mvX != p.facing)
      switchOrientation();
    s.slide += std::clamp(mvX * 8 - s.slide, -2, 2);
  }
  else if (s.slide != 0)
    s.slide -= sgn(s.slide);
  s.slideCarry += s.slide;
  const int steps = s.slideCarry / 8;
  s.slideCarry -= steps * 8;
  for (int i = 0; i < std::abs(steps); ++i)
  {
    if (mMap.moveHorizontallyWithStairStepping(p.x, p.y, Player::kWidth, p.height(), sgn(steps)) != MoveResult::Completed)
    {
      s.slide = s.slideCarry = 0;
      break;
    }
    if (mvY == 0)
      setVisual(PlayerVisual::Walking);
    ++p.walkFrame;
  }
  return true;
}

void World::updateSweeps()
{
  auto& s = mSweeps;
  auto& p = mPlayer;
  ++s.clock;
  if (s.paint)
  {
    updatePaint();
    return;
  }
  // Damage resets the Applause Cannon's meter and costs style.
  if (p.hp < s.lastHp)
  {
    s.lost += s.lastHp - p.hp;
    if (s.meter > 1)
      showMessage("THE APPLAUSE DIES DOWN - x1");
    s.meter = 1;
    s.streak = 0;
  }
  s.lastHp = p.hp;
  s.best = std::max(s.best, s.meter);
  // DOUBLE GEMS: each one is worth two.
  if (mStats.gems > s.lastGems && (s.rules & kSwDoubleGems) != 0)
  {
    const int extra = mStats.gems - s.lastGems;
    addScore(500 * extra, {(float(p.x) + 1.5f) * kCellSize, (float(p.y) - 6.0f) * kCellSize});
  }
  s.lastGems = mStats.gems;
  // The vote: a ballot as the runner passes each vote checkpoint.
  if (s.ballot < 0 && s.cur < int(s.votes.size()) && alive(p) && p.x + 1 >= s.votes[std::size_t(s.cur)].x)
    openBallot(s.cur);
  if (s.ballot >= 0)
  {
    ++s.ballotT;
    if (s.ballotT % 2 == 0 && s.ballotT < kSwSwing)
      playSound(Sfx::MeterTick);
    if (s.ballotT == kSwSwing)
    {
      playSound(Sfx::AudienceRoar);
      showMessage(sweepsOptionName(s.options[s.winner]) + " WINS THE VOTE");
    }
    if (s.ballotT >= kSwSwing + kSwShow)
      closeBallot();
  }
  // Enemies past the runner's segment sleep until its ballot is over; ???'s
  // confetti cannons never move.
  for (const auto& a : s.actors)
  {
    Enemy& e = mEnemies[std::size_t(a.enemy)];
    if (e.alive && (a.seg > s.cur || a.popper))
      e.stun = std::max(e.stun, 2);
  }
  // Turntables carry what stands on them a cell every other frame.
  if (s.clock % 2 == 0)
    for (const auto& t : s.turntables)
    {
      const int dir = t.dirAt(s.clock);
      const int top = t.row * kCellsPerTile;
      auto onIt = [&](int x, int w, int y) { return y + 1 == top && x + w > t.x0 * kCellsPerTile && x < (t.x1 + 1) * kCellsPerTile; };
      if (alive(p) && p.state == PlayerState::OnGround && onIt(p.x, Player::kWidth, p.y))
        mMap.moveHorizontally(p.x, p.y, Player::kWidth, p.height(), dir);
      for (auto& e : mEnemies)
        if (e.alive && e.stun == 0 && e.kind == EnemyKind::WarmupComic && onIt(e.x, e.w, e.y))
          mMap.moveHorizontally(e.x, e.y, e.w, e.h, dir);
    }
  for (const auto& t : s.turntables)
    if (s.clock % kSwTurnFlip == 0 && isOnScreen({t.x0 * kCellsPerTile, t.row * kCellsPerTile, (t.x1 - t.x0 + 1) * kCellsPerTile, 2}, 2))
      playSound(Sfx::TurntableTurn);
  updateChickens();
  // VIRUS FOR THE RUNNER: the V drifts at the runner's chest.
  if (s.clock % kSwVirusEvery == 0 && alive(p))
    for (auto& it : mItems)
      if (!it.taken && it.kind == ItemKind::Virus && it.variant == kVirusVariant)
      {
        it.x += sgn(p.x - it.x);
        it.y += sgn(p.y - 1 - it.y);
      }
  // The seat's frame: once the seat is broken a ladder goes down into the hollow.
  if (s.seatX >= 0 && mMap.block(s.seatX, s.seatY) == Tile::Empty)
    mMap.setBlock(s.seatX, s.seatY, Tile::Ladder);
  // The bonus entrance shows once the letter G is taken.
  const bool g = mStats.letters.find('G') != std::string::npos;
  for (auto& pr : mProps)
    if (pr.kind == PropKind::BonusDoor)
      pr.dormant = !g;
}

void World::updateChickens()
{
  auto& s = mSweeps;
  auto& p = mPlayer;
  for (std::size_t i = 0; i < s.chickens.size();)
  {
    auto& c = s.chickens[i];
    bool gone = false;
    c.vy = std::min(2.0f, c.vy + kChickenFall);
    const float nx = c.x + c.vx;
    if (mMap.overlapsSolid({int(std::floor(nx)), int(std::floor(c.y)), 2, 2}))
      c.vx = -c.vx * 0.5f;
    else
      c.x = nx;
    const float ny = c.y + c.vy;
    if (mMap.overlapsSolid({int(std::floor(c.x)), int(std::floor(ny)), 2, 2}))
    {
      if (c.vy > 0.0f)
      {
        // A bounce (and a honk); the fourth time it lies still and is gone.
        if (++c.bounces > kSwChickenBounces)
          gone = true;
        if (isOnScreen(c.box(), 2))
          playSound(Sfx::ChickenSqueak);
      }
      c.vy = c.vy > 0.0f ? std::min(-0.75f, -c.vy * 0.6f) : 0.0f;
    }
    else
      c.y = ny;
    if (c.y > float(mMap.height()) || c.x < -4.0f || c.x > float(mMap.width()) + 4.0f)
      gone = true;
    if (!gone && alive(p))
    {
      CellBox hb = p.hitBox();
      if ((s.rules & kSwBigHeads) != 0)
      {
        --hb.y;
        ++hb.h;
      }
      if (c.box().intersects(hb))
      {
        const int hp = p.hp;
        hurtPlayer(1);
        if (p.hp < hp && alive(p))
        {
          // Knocked back the way the chicken flew (Rocco half as far; SLIPPERY 2.5x).
          int cells = mCharacterIndex == 1 ? kSwKnock / 2 : kSwKnock;
          if ((s.rules & kSwSlippery) != 0)
            cells = cells * 5 / 2;
          const int dir = c.vx != 0.0f ? (c.vx > 0.0f ? 1 : -1) : sgn(p.x + 1 - int(c.x) - 1);
          for (int k = 0; k < cells; ++k)
            mMap.moveHorizontally(p.x, p.y, Player::kWidth, p.height(), dir == 0 ? 1 : dir);
        }
        playSound(Sfx::ChickenSqueak);
        gone = true;
      }
    }
    if (gone)
      s.chickens.erase(s.chickens.begin() + std::ptrdiff_t(i));
    else
      ++i;
  }
}

// --- Paintball -----------------------------------------------------------------------------

void World::updatePaint()
{
  auto& s = mSweeps;
  auto& p = mPlayer;
  if (!alive(p) || mState != WorldState::Playing)
    return;
  const int w = mLevel->width;
  auto at = [&](int cx, int cy) -> std::uint8_t* {
    const int bx = cx / kCellsPerTile, by = cy / kCellsPerTile;
    if (cx < 0 || cy < 0 || bx >= w || by >= mLevel->height)
      return nullptr;
    return &s.paints[std::size_t(by * w + bx)];
  };
  const CellBox b = p.box();
  // The floor under the feet.
  if (p.state == PlayerState::OnGround)
    for (int cx = b.left(); cx <= b.right(); ++cx)
      if (auto* c = at(cx, b.bottom() + 1); c && *c == 1 && mMap.solidTop(cx, b.bottom() + 1))
      {
        *c = 2;
        ++s.painted;
      }
  // The walls (and the ceiling) the runner touches.
  for (int cy = b.top(); cy <= b.bottom(); ++cy)
    for (int cx : {b.left() - 1, b.right() + 1})
      if (auto* c = at(cx, cy); c && *c == 3 && mMap.solid(cx, cy))
        *c = 4;
  for (int cx = b.left(); cx <= b.right(); ++cx)
    if (auto* c = at(cx, b.top() - 1); c && *c == 3)
      *c = 4;
  // Spring pads.
  if (p.state == PlayerState::OnGround)
    for (const auto& pad : s.pads)
      if (b.bottom() + 1 == pad.row * kCellsPerTile && b.right() >= pad.x0 * kCellsPerTile &&
          b.left() <= pad.x1 * kCellsPerTile + 1)
      {
        startLaunch(kSwLaunch);
        playSound(Sfx::SpringBoing);
      }
  const int pct = s.tops > 0 ? s.painted * 100 / s.tops : 0;
  while (s.chant < 3 && pct >= 25 * (s.chant + 1))
  {
    ++s.chant;
    playSound(Sfx::CrowdChant);
    char buf[48];
    std::snprintf(buf, sizeof(buf), "THE CROWD CHANTS - %d%%", 25 * s.chant);
    showMessage(buf);
  }
  if (s.tops > 0 && s.painted * 100 >= s.goal * s.tops)
  {
    showMessage("PAINTED!");
    playSound(Sfx::Teleport);
    mState = WorldState::Exiting;
    mStateFrames = 0;
  }
}

// --- Shots ---------------------------------------------------------------------------------

bool World::shotAtSweeps(Projectile& pr, const CellBox& b)
{
  auto& s = mSweeps;
  auto& p = mPlayer;
  if (s.paint)
  {
    // A shot paints the first block it hits.
    if (pr.kind != ShotKind::Enemy && mMap.overlapsSolid(b))
      for (int cy = b.top(); cy <= b.bottom(); ++cy)
        for (int cx = b.left(); cx <= b.right(); ++cx)
        {
          const int bx = cx / kCellsPerTile, by = cy / kCellsPerTile;
          if (cx < 0 || cy < 0 || bx >= mLevel->width || by >= mLevel->height || !mMap.solid(cx, cy))
            continue;
          auto& c = s.paints[std::size_t(by * mLevel->width + bx)];
          if (c == 1)
          {
            c = 2;
            ++s.painted;
          }
          else if (c == 3)
            c = 4;
        }
    return false;
  }
  const bool big = (s.rules & kSwBigHeads) != 0;
  if (pr.kind == ShotKind::Enemy)
  {
    // BIG HEADS: the runner's head is a cell taller too.
    if (!big || !alive(p))
      return false;
    const CellBox hb = p.hitBox();
    if (CellBox{hb.x, hb.y - 1, hb.w, 1}.intersects(b) && !hb.intersects(b))
    {
      hurtPlayer(1);
      return true;
    }
    return false;
  }
  for (auto& e : mEnemies)
  {
    if (!e.alive || !e.active || e.hidden || e.trapped)
      continue;
    CellBox hb = e.box();
    if (big)
      hb = {hb.x - 1, hb.y - 2, hb.w + 2, hb.h + 2}; // BIG HEADS: 2 cells up, a cell each side
    if (!hb.intersects(b) || std::find(pr.hit.begin(), pr.hit.end(), e.id) != pr.hit.end())
      continue;
    SwActor* a = sweepsActor(e);
    const Vec2 c = cellCenter(e.box());
    if (a && a->popper)
    {
      // ???: a confetti cannon pops, its score and 500 more.
      damageEnemy(e, e.hp);
      addScore(500, {c.x, c.y - 30.0f});
      static const Color kBits[] = {rgb(255, 80, 80), rgb(80, 220, 255), rgb(255, 230, 60), rgb(160, 255, 120)};
      for (const Color bit : kBits)
        burst(c, bit, rgb(255, 255, 255), 8, 2.0f);
      playSound(Sfx::ConfettiBlast);
      return true;
    }
    const bool applause = pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::ApplauseCannon);
    const bool was = e.alive;
    shotHitsEnemy(e, pr.dx, pr.damage);
    if (was && !e.alive && pr.kind == ShotKind::Proto)
      ++mStats.protoKills;
    if (applause)
    {
      // Four hits in a row without a scratch: the meter goes up a step.
      ++s.streak;
      const int meter = std::min(kSwMeterMax, 1 + s.streak / kSwHitsPerStep);
      if (meter > s.meter)
      {
        s.meter = meter;
        playSound(Sfx::MeterUp);
        char buf[32];
        std::snprintf(buf, sizeof(buf), "APPLAUSE x%d", meter);
        showMessage(buf);
      }
      s.best = std::max(s.best, s.meter);
    }
    if ((s.rules & kSwQuack) != 0)
      playSound(Sfx::Quack);
    burst(c, rgb(255, 255, 255), mTheme.enemyLight, 5, 1.2f);
    if (!pr.pierce)
      return true;
    pr.hit.push_back(e.id);
  }
  return false;
}

// --- Enemies -------------------------------------------------------------------------------

namespace
{

bool inArena(const SweepsState& s, const CellBox& b)
{
  for (const auto& a : s.arenas)
    if (a.intersects(b))
      return true;
  return false;
}

} // namespace

void World::updateComic(Enemy& e, const EnemyDef& def)
{
  auto& s = mSweeps;
  const auto& p = mPlayer;
  if (!mMap.onSolidGround(e.box()))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, 1);
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return;
  }
  if (e.cool > 0)
    --e.cool;
  const int mid = p.x + 1, ex = e.x + e.w / 2;
  if (e.tell > 0)
  {
    // Ba-dum-tss: then the chicken flies.
    if (--e.tell > 0)
      return;
    SwChicken c;
    c.x = float(e.dir > 0 ? e.x + e.w : e.x - 2);
    c.y = float(e.y - e.h);
    c.vy = -kChickenUp;
    const float ty = float(p.y - 3);
    const float disc = kChickenUp * kChickenUp + 2.0f * kChickenFall * (ty - c.y);
    const float t = disc > 0.0f ? (kChickenUp + std::sqrt(disc)) / kChickenFall : kChickenUp / kChickenFall;
    c.vx = std::clamp((float(mid - 1) - c.x) / std::max(1.0f, t), -1.5f, 1.5f);
    s.chickens.push_back(c);
    e.cool = def.cooldown;
    playSound(Sfx::Throw);
    return;
  }
  const bool arena = inArena(s, e.box());
  if (alive(p) && e.cool == 0 && std::abs(mid - ex) <= def.range && p.y - e.y >= -8 && p.y - e.y <= 30 &&
      isOnScreen(e.box(), 0))
  {
    // One Comic throws at a time, but in a marked arena.
    bool other = false;
    for (const auto& o : mEnemies)
      if (&o != &e && o.alive && o.kind == EnemyKind::WarmupComic && o.tell > 0 && !inArena(s, o.box()))
        other = true;
    if (arena || !other)
    {
      e.dir = mid >= ex ? 1 : -1;
      e.tell = def.tell;
      playSound(Sfx::Rimshot);
      return;
    }
  }
  // Patrol, half a cell a frame.
  if (e.timer % std::max(1, def.stepEvery) != 0)
    return;
  const CellBox b = e.box();
  const int aheadX = e.dir > 0 ? b.right() + 1 : b.left() - 1;
  const bool wall = e.dir > 0 ? mMap.touchingRightWall(b) : mMap.touchingLeftWall(b);
  const bool ledge = !mMap.solidTop(aheadX, b.bottom() + 1);
  if (wall || ledge)
    e.dir = -e.dir;
  else
    e.x += e.dir;
}

void World::updatePlant(Enemy& e, const EnemyDef& def)
{
  SwActor* a = sweepsActor(e);
  if (!a)
    return;
  auto& s = mSweeps;
  const auto& p = mPlayer;
  if (!mMap.onSolidGround(e.box()))
  {
    // Its seat went: it falls.
    mMap.moveVertically(e.x, e.y, e.w, e.h, 1);
    a->stand = 0;
    e.tell = 0;
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return;
  }
  if (e.cool > 0)
    --e.cool;
  const int mid = p.x + 1, ex = e.x + e.w / 2;
  if (mid != ex)
    e.dir = sgn(mid - ex);
  const bool reach = alive(p) && std::abs(mid - ex) <= kSwPlantReach && std::abs(p.y - e.y) <= 12;
  if (a->stand == 0)
  {
    // Sitting (the only one not clapping): stands for a runner close by,
    // one at a time but in a marked arena.
    if (!reach || !isOnScreen(e.box(), 0))
      return;
    if (!inArena(s, e.box()))
      for (const auto& o : s.actors)
        if (&o != a && o.stand > 0 && mEnemies[std::size_t(o.enemy)].alive)
          return;
    a->stand = 1;
    a->away = 0;
    e.tell = def.tell;
    playSound(Sfx::SeatFlip);
    return;
  }
  if (a->stand < def.tell)
  {
    ++a->stand;
    e.tell = def.tell - a->stand + 1;
    return;
  }
  e.tell = 0;
  a->away = reach ? 0 : a->away + 1;
  if (a->away >= kSwPlantSit)
  {
    a->stand = 0;
    a->away = 0;
    return;
  }
  if (e.cool > 0 || !alive(p) || !isOnScreen(e.box(), 0) || std::abs(mid - ex) > 30)
    return;
  // An aimed shot, a cell a frame.
  e.cool = def.cooldown;
  const int fx = e.dir > 0 ? e.x + e.w : e.x - 1, fy = e.y - 3;
  Projectile pr;
  pr.kind = ShotKind::Enemy;
  pr.w = pr.h = 1;
  pr.speed = 1;
  pr.damage = 1;
  const float dx = float(mid - fx), dy = float(p.y - 2 - fy);
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
  playSound(Sfx::TomatoToss);
}

void World::updateCueDrone(Enemy& e, const EnemyDef& def)
{
  SwActor* a = sweepsActor(e);
  if (!a)
    return;
  const auto& p = mPlayer;
  const int mid = p.x + 1, ex = e.x + e.w / 2;
  if (a->swoop == 1)
  {
    // The swoop: straight down onto the runner, then back up.
    if (e.y >= p.y || e.y >= a->homeY + 20 || mMap.overlapsSolid(boxAt(e.x, e.y + 1, e.w, e.h)))
      a->swoop = 2;
    else
      ++e.y;
    return;
  }
  if (a->swoop == 2)
  {
    if (e.timer % 2 == 0)
      --e.y;
    if (e.y <= a->homeY)
    {
      e.y = a->homeY;
      a->swoop = 0;
      e.cool = def.cooldown;
    }
    return;
  }
  if (a->flip > 0)
  {
    e.tell = kSwDroneFlip - a->flip + 1;
    if (++a->flip > kSwDroneFlip)
    {
      a->flip = 0;
      e.tell = 0;
      a->swoop = 1;
      a->card = (a->card + 1) % 3;
      playSound(Sfx::DroneSwoop);
    }
    return;
  }
  if (e.cool > 0)
    --e.cool;
  // Along its path, half a cell a frame.
  if (e.timer % std::max(1, def.stepEvery) == 0 && !a->path.empty())
  {
    const auto& [tx, ty] = a->path[std::size_t(a->target) % a->path.size()];
    if (e.x != tx)
      e.dir = sgn(tx - e.x);
    e.x += sgn(tx - e.x);
    e.y += sgn(ty - e.y);
    if (e.x == tx && e.y == ty)
      a->target = (a->target + 1) % int(a->path.size());
    a->homeY = e.y;
  }
  // Right over the runner: the card flips, then the swoop.
  if (alive(p) && e.cool == 0 && std::abs(mid - ex) <= 2 && p.y > e.y && p.y - e.y <= 24 && isOnScreen(e.box(), 0))
  {
    a->flip = 1;
    e.tell = kSwDroneFlip;
    playSound(Sfx::CardSpin);
  }
}

// --- Lives and savegames -------------------------------------------------------------------

void World::resetSweeps()
{
  auto& s = mSweeps;
  s.chickens.clear();
  s.slide = s.slideCarry = 0;
  s.lastHp = mPlayer.hp;
  for (auto& a : s.actors)
  {
    Enemy& e = mEnemies[std::size_t(a.enemy)];
    a.stand = 0;
    a.away = 0;
    a.flip = 0;
    if (a.swoop != 0)
      e.y = a.homeY;
    a.swoop = 0;
    e.tell = 0;
  }
}

bool World::sweepsCanSave() const
{
  return mSweeps.chickens.empty() && mSweeps.ballot < 0 && !mSweeps.paint;
}

std::vector<int> World::sweepsSave() const
{
  const auto& s = mSweeps;
  std::vector<int> v{s.cur, s.ballot, s.ballotT, s.winner, s.style, int(s.rules), int(s.lastWin), s.fanfare,
    int(s.rng), s.lost, s.best, s.meter, s.streak, s.lastHp, s.lastGems, s.slide, s.slideCarry, s.clock, s.boost};
  for (const auto& o : s.options)
  {
    v.push_back(int(o.rules));
    v.push_back(int(o.cat));
    v.push_back(o.fanfare);
    v.push_back(o.weight);
  }
  for (const auto& vote : s.votes)
    v.push_back(vote.done ? 1 : 0);
  for (const auto& a : s.actors)
  {
    v.push_back(a.hat ? 1 : 0);
    v.push_back(a.popper ? 1 : 0);
    v.push_back(a.stand);
    v.push_back(a.away);
    v.push_back(a.target);
    v.push_back(a.flip);
    v.push_back(a.swoop);
    v.push_back(a.homeY);
    v.push_back(a.card);
  }
  return v;
}

World::SaveLayout World::sweepsLayout() const
{
  const auto& s = mSweeps;
  SaveLayout l;
  l.head = 19 + 12;
  l.lists.push_back({1, std::vector<int>(s.votes.size(), -1)});
  l.lists.push_back({9, {}});
  for (const auto& a : s.actors)
    l.lists.back().enemies.push_back(a.enemy);
  return l;
}

bool World::validSweepsSave(const std::vector<int>& v) const
{
  const auto& s = mSweeps;
  return v.size() == 19 + 12 + s.votes.size() + s.actors.size() * 9;
}

void World::loadSweeps(const std::vector<int>& v)
{
  auto& s = mSweeps;
  std::size_t i = 0;
  s.cur = v[i++];
  s.ballot = v[i++];
  s.ballotT = v[i++];
  s.winner = std::clamp(v[i++], 0, 2);
  s.style = v[i++];
  const unsigned rules = unsigned(v[i++]);
  s.lastWin = unsigned(v[i++]);
  s.fanfare = v[i++];
  s.rng = std::uint32_t(v[i++]);
  s.lost = v[i++];
  s.best = v[i++];
  s.meter = std::clamp(v[i++], 1, kSwMeterMax);
  s.streak = v[i++];
  s.lastHp = v[i++];
  s.lastGems = v[i++];
  s.slide = v[i++];
  s.slideCarry = v[i++];
  s.clock = v[i++];
  s.boost = v[i++];
  for (auto& o : s.options)
  {
    o.rules = unsigned(v[i++]);
    o.cat = SwCat(std::clamp(v[i++], 0, int(SwCat::Fanfare)));
    o.fanfare = v[i++];
    o.weight = v[i++];
  }
  for (auto& vote : s.votes)
    vote.done = v[i++] != 0;
  for (auto& a : s.actors)
  {
    a.hat = v[i++] != 0;
    a.popper = v[i++] != 0;
    a.stand = v[i++];
    a.away = v[i++];
    a.target = v[i++];
    a.flip = v[i++];
    a.swoop = v[i++];
    a.homeY = v[i++];
    a.card = v[i++];
  }
  s.chickens.clear();
  s.rules = rules;
  mHull.lowgrav = (rules & kSwLowGrav) != 0 || mLevel->flags.count("lowgrav") != 0;
  if (s.seatX >= 0 && mMap.block(s.seatX, s.seatY) == Tile::Empty)
    mMap.setBlock(s.seatX, s.seatY, Tile::Ladder);
  const bool g = mStats.letters.find('G') != std::string::npos;
  for (auto& pr : mProps)
    if (pr.kind == PropKind::BonusDoor)
      pr.dormant = !g;
}

} // namespace gr
