// Level 29, Dead Air: the signal colour and its colour bars, RGB dials
// (shot, or timed), the change rule (the incoming bars flash, then the
// change waits until the runner is off the outgoing colour), the
// Tri-Tone Blaster's tint and pips (all three lit: black bars), Static
// Swarms, the Test Card and its arena, Ghost Images, tuning in, and the
// Wraparound bonus (rules=wrap_xy). Episode 5's treasure is here too.
// Drawing is in world_signal_draw.cpp.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>

namespace gr
{

namespace
{

constexpr Color kBarColors[4] = {rgb(235, 40, 50), rgb(40, 220, 90), rgb(50, 110, 255), rgb(24, 24, 28)};

int colorIndex(const std::string& name)
{
  if (name == "green")
    return kSignalGreen;
  if (name == "blue")
    return kSignalBlue;
  if (name == "black")
    return kSignalBlack;
  return kSignalRed;
}

CellBox blockRect(int x0, int y0, int x1, int y1)
{
  return {std::min(x0, x1) * kCellsPerTile, std::min(y0, y1) * kCellsPerTile, (std::abs(x1 - x0) + 1) * kCellsPerTile,
    (std::abs(y1 - y0) + 1) * kCellsPerTile};
}

// The Test Card's quadrants: 0 top left, 1 top right, 2 bottom right,
// 3 bottom left; which colour sits where after `turn` quarter turns.
int quadrantColor(int pos, int turn)
{
  return ((pos - turn) % 4 + 4) % 4;
}

} // namespace

Color signalColor(int c)
{
  return kBarColors[std::clamp(c, 0, 3)];
}

// --- Setup ---------------------------------------------------------------------------------

bool World::setupSignalEntity(const EntityDef& e)
{
  auto& s = mSignal;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  const bool hasRect = e.rect("rect", x0, y0, x1, y1);
  // Episode 5's 10,000-point treasures, in any theme.
  if (e.kind == "treasure" && e.hasPos)
  {
    Treasure t;
    t.x = e.x;
    t.y = e.y;
    t.score = e.num("score", 10000);
    t.art = e.str("art");
    s.treasures.push_back(t);
    return true;
  }
  if (mLevel->themeKey != "dead_air" && mLevel->rules.find("wrap_xy") == std::string::npos)
    return false;
  if (e.kind == "signal")
  {
    s.on = true;
    s.color = colorIndex(e.str("start", "red"));
    return true;
  }
  if (e.kind == "bar" && hasRect)
  {
    s.on = true;
    Layer l;
    l.id = e.id.empty() ? "bar" + std::to_string(s.bars.size()) : e.id;
    l.x0 = std::min(x0, x1);
    l.y0 = std::min(y0, y1);
    l.x1 = std::max(x0, x1);
    l.y1 = std::max(y0, y1);
    l.tile = Tile::Solid;
    l.driver = LayerDriver::Script;
    l.style = 7; // drawn by world_signal_draw.cpp
    const int c = colorIndex(e.str("color", "red"));
    l.color = kBarColors[c];
    l.scriptSolid = false;
    l.solid = true;
    for (int ty = l.y0; ty <= l.y1; ++ty)
      for (int tx = l.x0; tx <= l.x1; ++tx)
        if (tx >= 0 && ty >= 0 && tx < mLevel->width && ty < mLevel->height)
          mLayerMask[std::size_t(ty * mLevel->width + tx)] = 1;
    mLayers.push_back(l);
    s.bars.push_back({int(mLayers.size()) - 1, c});
    return true;
  }
  if (e.kind == "rgbswitch" && e.hasPos)
  {
    s.on = true;
    RgbDial d;
    d.id = e.id;
    d.x = e.x;
    d.y = e.y;
    d.timed = e.str("mode", "shoot") == "timed";
    d.period = std::max(1, e.num("period", 150));
    d.warn = e.num("warn", 22);
    d.phase = e.num("phase", 0);
    if (hasRect)
      d.rect = blockRect(x0, y0, x1, y1);
    s.dials.push_back(d);
    return true;
  }
  if (e.kind == "cardarena" && hasRect)
  {
    s.arena = blockRect(x0, y0, x1, y1);
    const auto wall = e.list("wall"); // x, y0, y1 (blocks)
    if (wall.size() == 3)
    {
      s.wallX = wall[0];
      s.wallY0 = std::min(wall[1], wall[2]);
      s.wallY1 = std::max(wall[1], wall[2]);
    }
    const auto out = e.list("exit"); // x, y0, y1 (blocks)
    if (out.size() == 3)
    {
      s.exitX = out[0];
      s.exitY0 = std::min(out[1], out[2]);
      s.exitY1 = std::max(out[1], out[2]);
    }
    return true;
  }
  if (e.kind == "tunein")
  {
    s.tuneX = e.num("x", e.hasPos ? e.x : -1);
    return true;
  }
  if (e.kind == "duck")
    return true; // the duck's costume is only drawn
  return false;
}

void World::setupSignalEnemy(Enemy& en, const EntityDef& e)
{
  if (en.kind != EnemyKind::StaticSwarm && en.kind != EnemyKind::TestCard && en.kind != EnemyKind::GhostImage)
    return;
  mSignal.on = true;
  if (en.kind == EnemyKind::StaticSwarm)
    en.carrier = e.num("carrier", 0) != 0;
  if (en.kind == EnemyKind::TestCard)
    mSignal.card = int(mEnemies.size()) - 1;
  if (en.kind == EnemyKind::GhostImage)
  {
    en.railX0 = en.x - 12;
    en.railX1 = en.x + 12;
  }
}

void World::linkSignal()
{
  auto& s = mSignal;
  s.wrap = mLevel->rules.find("wrap_xy") != std::string::npos;
  if (s.wrap)
  {
    s.on = true;
    mMap.setWrap(true);
    s.homeX = mPlayer.x;
    s.homeY = mPlayer.y;
  }
  s.on = s.on || mLevel->themeKey == "dead_air";
  if (!s.on)
    return;
  applySignal();
  for (const auto& b : s.bars)
    applyLayer(mLayers[std::size_t(b.layer)], mLayers[std::size_t(b.layer)].scriptSolid);
}

// --- The signal ----------------------------------------------------------------------------

bool World::blackLit() const
{
  const auto& s = mSignal;
  return s.pips[0] > 0 && s.pips[1] > 0 && s.pips[2] > 0;
}

// Which bars want to be solid now (the layers follow, never inside the runner).
void World::applySignal()
{
  auto& s = mSignal;
  const bool black = blackLit();
  for (const auto& b : s.bars)
    mLayers[std::size_t(b.layer)].scriptSolid = b.color == kSignalBlack ? black : b.color == s.color;
}

void World::requestSignal(int c)
{
  auto& s = mSignal;
  c = ((c % 3) + 3) % 3;
  if (c == s.color && s.pending < 0)
    return;
  if (c == s.pending)
    return;
  s.pending = c;
  s.flash = kSignalFlash;
  playSound(Sfx::Blink);
}

void World::stepSignal()
{
  const auto& s = mSignal;
  requestSignal((s.pending >= 0 ? s.pending : s.color) + 1);
}

// On a solid bar of colour c (the blocks under the runner's feet).
bool World::onBar(int c) const
{
  const auto& p = mPlayer;
  if (p.state != PlayerState::OnGround)
    return false;
  const int by = (p.y + 1) / kCellsPerTile;
  for (const auto& b : mSignal.bars)
  {
    if (b.color != c)
      continue;
    const auto& l = mLayers[std::size_t(b.layer)];
    if (!l.solid || by < l.y0 || by > l.y1)
      continue;
    for (int cx = p.x; cx < p.x + Player::kWidth; ++cx)
    {
      const int bx = cx / kCellsPerTile;
      if (bx >= l.x0 && bx <= l.x1)
        return true;
    }
  }
  return false;
}

int World::signalShown() const
{
  const auto& s = mSignal;
  return s.pending >= 0 ? s.pending : s.color;
}

void World::wrapRunner()
{
  auto& p = mPlayer;
  const int W = mMap.width(), H = mMap.height();
  if (p.x + 1 < 0)
    p.x += W, p.prevX += W;
  else if (p.x + 1 >= W)
    p.x -= W, p.prevX -= W;
  if (p.y >= H)
    p.y -= H, p.prevY -= H;
  else if (p.y < 0)
    p.y += H, p.prevY += H;
}

void World::updateSignal(const PlayerInput& input)
{
  (void)input;
  auto& s = mSignal;
  auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  if (s.tintCool > 0)
    --s.tintCool;
  if (s.snow > 0)
    --s.snow;
  if (s.shake > 0)
    --s.shake;
  if (s.cardStandBy > 0)
    --s.cardStandBy;
  // Tint pips fade; a black bar goes with the first one out.
  const bool wasBlack = blackLit();
  for (int& pip : s.pips)
    if (pip > 0)
      --pip;
  if (wasBlack != blackLit())
    applySignal();
  // Timed dials step the signal while the runner is inside their rect.
  for (auto& d : s.dials)
  {
    if (d.lock > 0)
      --d.lock;
    if (d.flash > 0)
      --d.flash;
    if (!d.timed || !alive || !d.rect.intersects(p.box()) || s.cardDead)
      continue;
    ++d.t;
    if ((d.t + d.phase) % d.period == 0)
    {
      d.flash = 6;
      stepSignal();
    }
  }
  // A change: the incoming bars flash, then it lands as soon as the runner
  // is off the outgoing colour.
  if (s.pending >= 0)
  {
    if (s.flash > 0)
      --s.flash;
    else if (!onBar(s.color))
    {
      s.color = s.pending;
      s.pending = -1;
      applySignal();
      playSound(Sfx::Zap);
    }
  }
  // The Test Card down by any means counts.
  if (!s.cardDead && s.card >= 0 && std::size_t(s.card) < mEnemies.size() && !mEnemies[std::size_t(s.card)].alive)
  {
    s.cardDead = true;
    s.beam = CardBeam{};
  }
  // The Test Card's arena: in, the wall shuts behind; the card down, it opens.
  if (s.wallX >= 0)
  {
    const bool in = alive && s.arena.intersects(p.box()) && p.x > (s.wallX + 1) * kCellsPerTile;
    if (!s.wallShut && in && !s.cardDead)
      shutCardWall(true);
    else if (s.wallShut && s.cardDead)
      shutCardWall(false);
  }
  // Tune in: past the mark with the card down.
  if (!s.tuned && s.tuneX >= 0 && s.cardDead && p.x >= s.tuneX * kCellsPerTile)
  {
    s.tuned = true;
    s.shake = kTuneShake;
    mCamera.shake(8, 2.0f);
    showMessage("SIGNAL LOCKED: TUNED IN");
    playSound(Sfx::Chime);
  }
  // Treasures.
  for (auto& t : s.treasures)
    if (!t.taken && alive && boxAt(t.x * kCellsPerTile, t.y * kCellsPerTile + 1, 2, 2).intersects(p.box()))
    {
      t.taken = true;
      const Vec2 at{(float(t.x) * kCellsPerTile + 1.0f) * kCellSize, (float(t.y) * kCellsPerTile) * kCellSize};
      addScore(t.score, at);
      showMessage(t.art == "maxtv_ident" ? "MAXTV: EVERYTHING, ALWAYS" : "TREASURE!");
      playSound(Sfx::Gem);
      playSound(Sfx::Chime);
    }
  // The bonus entrance shows (and counts) only while the black bars are solid.
  if (!s.wrap)
    for (auto& pr : mProps)
      if (pr.kind == PropKind::BonusDoor)
        pr.dormant = !blackLit();
  // Frame 4242: the backdrop static says thanks, for one frame.
  s.egg = mStats.frames == 4242 ? 1 : 0;
}

void World::shutCardWall(bool shut)
{
  auto& s = mSignal;
  s.wallShut = shut;
  for (int by = s.wallY0; by <= s.wallY1; ++by)
    mMap.setBlock(s.wallX, by, shut ? Tile::Solid : Tile::Empty);
  for (int by = s.exitY0; s.exitX >= 0 && by <= s.exitY1; ++by)
    mMap.setBlock(s.exitX, by, shut ? Tile::Solid : Tile::Empty);
  playSound(shut ? Sfx::Shutter : Sfx::Clunk);
}

// Up + fire with the Tri-Tone turns its tint instead of firing; true if it did.
bool World::turnTint(const Button& fire)
{
  auto& s = mSignal;
  const auto& p = mPlayer;
  if (!fire.triggered || p.stance != Stance::Up)
    return false;
  if (s.tintCool == 0)
  {
    s.tint = (s.tint + 1) % 3;
    s.tintCool = kTintCool;
    playSound(Sfx::Click);
  }
  return true;
}

void World::tintShot(Projectile& pr)
{
  auto& s = mSignal;
  pr.tint = s.tint;
  s.pips[s.tint] = kPipFrames;
  if (s.on && blackLit())
    applySignal();
}

bool World::shotAtSignal(Projectile& pr, const CellBox& hit)
{
  auto& s = mSignal;
  for (auto& d : s.dials)
  {
    if (d.timed || !boxAt(d.x * kCellsPerTile, d.y * kCellsPerTile + 1, 2, 2).intersects(hit))
      continue;
    if (d.lock == 0)
    {
      d.lock = kDialLock;
      d.flash = 6;
      if (pr.tint >= 0)
        requestSignal(pr.tint);
      else
        stepSignal();
    }
    burst(cellCenter(hit), signalColor(signalShown()), rgb(255, 255, 255), 6, 1.2f);
    return true;
  }
  return false;
}

// 0 as usual, 1 the shot is spent, 2 it passes through.
int World::shotAtSignalEnemy(Projectile& pr, Enemy& e)
{
  auto& s = mSignal;
  if (e.kind == EnemyKind::GhostImage)
  {
    const int halo = (mStats.frames / kImageCycle) % 3;
    const bool open = e.dive > 0; // no halo for a while after it fires
    if (open || pr.tint == halo)
    {
      if (pr.tint == halo)
        pr.damage = std::max(pr.damage, 2);
      return 0;
    }
    e.flash = 3;
    burst(cellCenter(e.box()), signalColor(halo), rgb(255, 255, 255), 5, 1.2f);
    playSound(Sfx::Tink);
    return 1;
  }
  if (e.kind == EnemyKind::TestCard)
  {
    s.cardStandBy = 12;
    const int c = s.color;
    if (s.cardHp[c] > 0)
    {
      s.cardHp[c] = std::max(0, s.cardHp[c] - (pr.tint == c ? 2 : 1));
      e.flash = 4;
      playSound(Sfx::Hit);
      if (s.cardHp[c] == 0)
      {
        showMessage("THAT QUARTER'S DOWN");
        burst(cellCenter(e.box()), signalColor(c), rgb(200, 200, 200), 14, 1.6f);
      }
      e.hp = s.cardHp[0] + s.cardHp[1] + s.cardHp[2];
      if (e.hp == 0)
      {
        s.cardDead = true;
        s.beam = CardBeam{};
        addScore(enemyDef(e.def).score, cellCenter(e.box()));
        killEnemy(e);
        showMessage("TEST CARD DOWN: TUNE IN ->");
      }
    }
    else
    {
      e.flash = 2;
      playSound(Sfx::Tink);
    }
    return 1;
  }
  return 0;
}

// --- Enemies ---------------------------------------------------------------------------------

void World::updateStaticSwarm(Enemy& e, const EnemyDef& def)
{
  (void)def;
  auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  const int W = mMap.width(), H = mMap.height();
  if (e.cool > 0)
    --e.cool;
  if (e.attach != 0)
  {
    // Latched: it rides on the runner's side.
    if (!alive)
    {
      e.attach = 0;
      e.cool = 30;
      return;
    }
    e.x = e.attach > 0 ? p.x + 2 : p.x - 3;
    e.y = p.y - 1;
    if (p.turbo > 0)
    {
      // Turbo shakes it off.
      e.attach = 0;
      e.cool = 45;
      e.x += e.dir * 10;
      burst(cellCenter(e.box()), rgb(220, 220, 220), rgb(120, 120, 120), 10, 1.6f);
      return;
    }
    if (!e.carrier && ++e.timer % kSwarmBite == 0)
      hurtPlayer(1);
    if (e.carrier)
    {
      mSignal.snow = std::max(mSignal.snow, 2);
      mSignal.snowQuarter = e.attach > 0 ? 1 : 0;
    }
    return;
  }
  if (!alive)
    return;
  // Drift at the runner half a cell a frame, through walls and bars (across
  // the edges in the Wraparound).
  const int tx = p.x, ty = p.y - 1;
  int dx = tx - e.x, dy = ty - e.y;
  if (mSignal.wrap)
  {
    if (std::abs(dx) > W / 2)
      dx -= (dx > 0 ? W : -W);
    if (std::abs(dy) > H / 2)
      dy -= (dy > 0 ? H : -H);
  }
  if (e.timer % (mSignal.wrap ? 3 : 2) == 0)
  {
    e.x += (dx > 0) - (dx < 0);
    e.y += (dy > 0) - (dy < 0);
    if (dx != 0)
      e.dir = dx > 0 ? 1 : -1;
  }
  if (mSignal.wrap)
  {
    e.x = ((e.x % W) + W) % W;
    e.y = ((e.y % H) + H) % H;
  }
  if (e.cool > 0 || !e.box().intersects(p.hitBox()))
  {
    e.tell = 0;
    return;
  }
  if (mSignal.wrap)
  {
    // The Wraparound: a touch puts the runner back in the middle.
    p.x = p.prevX = mSignal.homeX;
    p.y = p.prevY = mSignal.homeY;
    p.state = PlayerState::OnGround;
    setVisual(PlayerVisual::Standing);
    e.cool = 30;
    playSound(Sfx::Teleport);
    showMessage("STATIC! BACK TO THE MIDDLE");
    return;
  }
  // The hiss rises, then it latches.
  if (e.tell == 0)
  {
    e.tell = kSwarmHiss;
    playSound(Sfx::Hiss);
  }
  if (--e.tell > 0)
    return;
  e.attach = (e.x + 2 > p.x + 1) ? 1 : -1;
  e.timer = 0;
  if (e.carrier)
  {
    infect();
    mSignal.snow = kSnowFrames;
    mSignal.snowQuarter = e.attach > 0 ? 1 : 0;
  }
  playSound(Sfx::Hiss);
}

void World::updateTestCard(Enemy& e, const EnemyDef& def)
{
  (void)def;
  auto& s = mSignal;
  auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  if (!s.wallShut || s.cardDead)
    return;
  if (e.timer % kCardTurn == 0)
    ++s.cardTurn;
  auto& b = s.beam;
  const CellBox box = e.box();
  if (b.on)
  {
    ++b.t;
    if (!b.burning && b.t >= kCardTell)
    {
      b.burning = true;
      b.t = 0;
      playSound(Sfx::EyeBeam);
    }
    if (b.burning)
    {
      // Along the beam, two cells thick, to the first wall.
      const float dx = b.x1 - b.x0, dy = b.y1 - b.y0;
      const float len = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
      for (float k = 0.0f; k <= len; k += 1.0f)
      {
        const float x = b.x0 + dx * k / len, y = b.y0 + dy * k / len;
        if (k > 6.0f && mMap.solid(int(std::floor(x)), int(std::floor(y))))
          break;
        if (alive && CellBox{int(std::floor(x)) - 1, int(std::floor(y)) - 1, 2, 2}.intersects(p.hitBox()))
        {
          hurtPlayer(1);
          break;
        }
      }
      if (b.t >= kCardBeam)
      {
        b = CardBeam{};
        s.cardCool = kCardCool;
      }
    }
    return;
  }
  if (s.cardCool > 0)
  {
    --s.cardCool;
    return;
  }
  if (!alive)
    return;
  // The quadrant nearest the runner (still lit) aims at where they are.
  const float pcx = float(p.x) + 1.5f, pcy = float(p.y) - 2.0f;
  int best = -1;
  float bestD = 1e9f, bx = 0.0f, by = 0.0f;
  for (int pos = 0; pos < 4; ++pos)
  {
    const int c = quadrantColor(pos, s.cardTurn);
    if (c < 3 && s.cardHp[c] == 0)
      continue;
    const float qx = float(box.x) + (pos == 1 || pos == 2 ? 6.0f : 2.0f);
    const float qy = float(box.y) + (pos >= 2 ? 6.0f : 2.0f);
    const float d = (qx - pcx) * (qx - pcx) + (qy - pcy) * (qy - pcy);
    if (d < bestD)
    {
      bestD = d;
      best = c;
      bx = qx;
      by = qy;
    }
  }
  if (best < 0)
    return;
  const float dx = pcx - bx, dy = pcy - by;
  const float len = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
  b.on = true;
  b.burning = false;
  b.t = 0;
  b.quadrant = best;
  b.x0 = bx;
  b.y0 = by;
  b.x1 = bx + dx / len * float(kCardReach);
  b.y1 = by + dy / len * float(kCardReach);
  playSound(Sfx::Beep);
}

void World::updateGhostImage(Enemy& e, const EnemyDef& def)
{
  auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  if (e.dive > 0)
    --e.dive; // frames left with no halo after a shot
  if (e.cool > 0)
    --e.cool;
  const CellBox b = e.box();
  const int pcx = p.x + 1;
  if (e.tell > 0)
  {
    // Its after-images snap back into it, then it fires.
    if (--e.tell == 0)
    {
      spawnProjectile(ShotKind::Enemy, e.dir > 0 ? b.right() + 1 : b.left() - 1, b.top() + 2, e.dir, 0);
      mProjectiles.back().speed = 1;
      e.cool = def.cooldown;
      e.dive = kImageOpen;
      playSound(Sfx::EnemyShot);
    }
    return;
  }
  // Patrol half a cell a frame.
  if (e.timer % 2 == 0)
  {
    int nx = e.x + e.dir;
    const bool edge = !mMap.solidTop(e.dir > 0 ? b.right() + 1 : b.left() - 1, b.bottom() + 1);
    if (nx < e.railX0 || nx > e.railX1 || edge || mMap.solid(e.dir > 0 ? b.right() + 1 : b.left() - 1, b.bottom()))
    {
      e.dir = -e.dir;
      nx = e.x;
    }
    e.x = nx;
  }
  if (alive && e.cool == 0 && std::abs(pcx - (b.x + 1)) <= 20 && std::abs(p.y - b.bottom()) <= 6)
  {
    e.dir = pcx < b.x + 1 ? -1 : 1;
    e.tell = kImageTell;
  }
}

// --- Saving ------------------------------------------------------------------------------------

bool World::signalCanSave() const
{
  const auto& s = mSignal;
  return s.pending < 0 && !s.beam.on;
}

std::vector<int> World::signalSave() const
{
  const auto& s = mSignal;
  std::vector<int> v{s.color, s.tint, s.wallShut, s.cardHp[0], s.cardHp[1], s.cardHp[2], s.cardTurn, s.cardDead,
    s.tuned};
  for (const auto& t : s.treasures)
    v.push_back(t.taken);
  return v;
}

bool World::validSignalSave(const std::vector<int>& v) const
{
  return v.size() == 9 + mSignal.treasures.size() && v[0] >= 0 && v[0] < 3 && v[1] >= 0 && v[1] < 3;
}

void World::loadSignal(const std::vector<int>& v)
{
  auto& s = mSignal;
  s.color = v[0];
  s.pending = -1;
  s.flash = 0;
  s.tint = v[1];
  for (int k = 0; k < 3; ++k)
  {
    s.cardHp[k] = v[std::size_t(3 + k)];
    s.pips[k] = 0;
  }
  s.cardTurn = v[6];
  s.cardDead = v[7] != 0;
  s.tuned = v[8] != 0;
  s.beam = CardBeam{};
  for (std::size_t i = 0; i < s.treasures.size(); ++i)
    s.treasures[i].taken = v[9 + i] != 0;
  if (s.wallX >= 0 && (v[2] != 0) != s.wallShut)
    shutCardWall(v[2] != 0);
  applySignal();
  for (const auto& b : s.bars)
  {
    // As updateLayers: never solid on top of the runner.
    auto& l = mLayers[std::size_t(b.layer)];
    const bool inside = blockRect(l.x0, l.y0, l.x1, l.y1).intersects(mPlayer.box());
    applyLayer(l, l.scriptSolid && !inside);
  }
}

} // namespace gr
