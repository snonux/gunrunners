// Level 38, Writers' Room: handwritten words and the Red Pen that strikes
// them out, Typos that write them back, load-bearing words that sketch
// themselves back, the red strings (walkable slopes), the typewriter and
// its bridge, the crusher press, Deadlines, Writer's Blocks and the sketch
// guard, the pen boxes that come back, the easter eggs (the egg and its
// chick, the rejected pitches, the original pitch), and the Breakout bonus
// (rules=breakout). Drawing is in world_writers_draw.cpp.

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

// Cells across for a word: a block per two letters.
int wordCells(const std::string& text) { return std::max(1, int(text.size() + 1) / 2) * kCellsPerTile; }

bool redPen(const Projectile& pr) { return pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::RedPen); }

} // namespace

// --- Setup ---------------------------------------------------------------------------------

bool World::setupWritersEntity(const EntityDef& e)
{
  const bool breakout = mLevel->rules.find("breakout") != std::string::npos;
  if (mLevel->themeKey != "writers_room" && !breakout)
    return false;
  auto& s = mWriters;
  s.on = true;
  s.breakout = breakout;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  const bool hasRect = e.rect("rect", x0, y0, x1, y1);
  if (e.kind == "word" && e.hasPos)
  {
    Word w;
    w.id = e.id;
    w.text = e.str("text", "WORD");
    w.x = e.x * kCellsPerTile;
    w.y = e.y * kCellsPerTile;
    w.w = wordCells(w.text);
    w.load = e.num("load", 0) != 0;
    const std::string target = e.str("target");
    if (target == "crusher")
      w.target = WordTarget::Crusher;
    else if (target == "egg")
      w.target = WordTarget::Egg;
    else if (hasRect)
    {
      w.target = WordTarget::Rect;
      w.x0 = x0;
      w.y0 = y0;
      w.x1 = x1;
      w.y1 = y1;
    }
    s.words.push_back(w);
    return true;
  }
  if (e.kind == "inkwell" && e.hasPos)
  {
    Inkwell k;
    k.x = e.x * kCellsPerTile;
    k.y = e.y * kCellsPerTile + 1;
    k.timer = kWrTypoEvery - 60; // the first Typo comes out soon after you arrive
    s.inkwells.push_back(k);
    return true;
  }
  if (e.kind == "crusher" && e.hasPos)
  {
    Crusher c;
    c.x0 = e.x;
    c.w = e.num("w", 4);
    c.rest = e.y;
    c.to = e.num("to", e.y + 8);
    c.face = (c.rest + 1) * kCellsPerTile - 1;
    s.crushers.push_back(c);
    return true;
  }
  if (e.kind == "key" && e.hasPos)
  {
    TypeKey k;
    k.x = e.x;
    k.y = e.y;
    const std::string ch = e.str("ch", "A");
    k.ch = ch == "SPACE" ? ' ' : ch.empty() ? 'A' : ch[0];
    s.keys.push_back(k);
    return true;
  }
  if (e.kind == "bridge" && hasRect)
  {
    s.bridge.x0 = x0;
    s.bridge.x1 = x1;
    s.bridge.row = y0;
    s.bridge.text = e.str("text", "A BRIDGE APPEARS");
    return true;
  }
  if (e.kind == "card" && e.hasPos)
  {
    s.cards.push_back({e.x * kCellsPerTile, e.y * kCellsPerTile, e.str("text"), 0});
    return true;
  }
  if (e.kind == "string")
  {
    const auto pins = e.list("pins");
    for (std::size_t i = 0; i + 1 < pins.size(); i += 2)
    {
      s.pins.push_back(pins[i] * kCellsPerTile);
      s.pins.push_back(pins[i + 1] * kCellsPerTile);
    }
    return true;
  }
  if (e.kind == "penbox" && e.hasPos)
  {
    // Matched to its box in linkWriters (the box comes from the map's `W`).
    s.penBoxes.push_back(e.x * 1000 + e.y);
    return true;
  }
  if (e.kind == "pitch" && e.hasPos)
  {
    s.pitchX = e.x * kCellsPerTile;
    s.pitchY = e.y * kCellsPerTile + 1;
    return true;
  }
  if (e.kind == "step" && e.hasPos)
  {
    WritersStep st;
    st.x = e.x * kCellsPerTile;
    st.y = e.y * kCellsPerTile + 1;
    st.act = e.str("act", "go");
    st.arg = e.str("arg");
    st.dir = e.num("dir", 1);
    s.route.push_back(st);
    return true;
  }
  if (e.kind == "brick" && e.hasPos)
  {
    Brick b;
    b.x = e.x * kCellsPerTile;
    b.y = e.y * kCellsPerTile;
    const std::string ch = e.str("ch", " ");
    b.ch = ch.empty() ? ' ' : ch[0];
    b.red = e.num("red", 0) != 0;
    b.hp = b.red ? 2 : 1;
    b.gem = e.num("gem", 0) != 0;
    s.bricks.push_back(b);
    return true;
  }
  if (e.kind == "duck")
    return true; // the duck's reading glasses and pencil are only drawn
  return false;
}

void World::setupWritersEnemy(Enemy& en, const EntityDef& e)
{
  auto& s = mWriters;
  const int i = int(&en - mEnemies.data());
  auto enemyWord = [&](const char* text) {
    Word w;
    w.id = text;
    w.text = text;
    w.w = wordCells(w.text);
    w.target = WordTarget::Enemy;
    w.enemy = i;
    s.words.push_back(w);
    return int(s.words.size()) - 1;
  };
  if (en.kind == EnemyKind::Deadline)
  {
    Deadline d;
    d.enemy = i;
    const auto lane = e.list("lane");
    d.x0 = lane.size() == 2 ? lane[0] * kCellsPerTile : en.x - 20;
    d.x1 = lane.size() == 2 ? (lane[1] + 1) * kCellsPerTile : en.x + en.w + 20;
    s.deadlines.push_back(d);
  }
  else if (en.kind == EnemyKind::WritersBlock)
  {
    WritersBlock b;
    b.enemy = i;
    b.word = enemyWord("BLOCK");
    s.blocks.push_back(b);
  }
  else if (en.kind == EnemyKind::SketchGuard)
  {
    s.guards.push_back(i);
    enemyWord("GUARD");
  }
}

void World::linkWriters()
{
  auto& s = mWriters;
  if (!s.on)
    return;
  // The strings: a one-way slope between each pair of pins, a cell per column.
  if (s.pins.size() >= 4)
  {
    std::vector<int> tops(std::size_t(mMap.width()), -1);
    for (std::size_t i = 0; i + 3 < s.pins.size(); i += 2)
    {
      int ax = s.pins[i], ay = s.pins[i + 1], bx = s.pins[i + 2], by = s.pins[i + 3];
      if (ax > bx)
      {
        std::swap(ax, bx);
        std::swap(ay, by);
      }
      // The pins sit in the middle of their blocks.
      ax += 1;
      bx += 1;
      for (int x = ax; x <= bx; ++x)
        if (x >= 0 && x < int(tops.size()))
          tops[std::size_t(x)] = ay + int(std::lround(float(by - ay) * float(x - ax) / float(std::max(1, bx - ax))));
    }
    // Out to the floor at each end: the first and last pins' rows.
    const int lo = std::min(s.pins[0], s.pins[s.pins.size() - 2]), hi = std::max(s.pins[0], s.pins[s.pins.size() - 2]);
    for (int x = lo; x <= hi + 1; ++x)
      if (x >= 0 && x < int(tops.size()) && tops[std::size_t(x)] < 0)
        tops[std::size_t(x)] = x <= lo + 1 ? (s.pins[0] == lo ? s.pins[1] : s.pins.back()) : (s.pins[0] == lo ? s.pins.back() : s.pins[1]);
    mMap.setSlopes(std::move(tops));
  }
  // What each word's rect holds.
  for (auto& w : s.words)
    if (w.target == WordTarget::Rect)
    {
      w.tiles.clear();
      for (int ty = w.y0; ty <= w.y1; ++ty)
        for (int tx = w.x0; tx <= w.x1; ++tx)
          w.tiles.push_back(mMap.block(tx, ty));
    }
  // Writer's Blocks are solid while they sit.
  for (auto& b : s.blocks)
    stampBlock(b, true);
  syncWriterWords();
  // Two Typos per inkwell, waiting in the ink.
  const int typoDef = enemyIndex("typo");
  for (auto& k : s.inkwells)
    for (int& slot : k.typos)
    {
      spawnEnemy(typoDef, k.x + 1, k.y);
      if (mEnemies.empty() || typoDef < 0)
        break;
      mEnemies.back().alive = false;
      slot = int(mEnemies.size()) - 1;
      s.typos.push_back({slot, -1, 0, 1});
    }
  // The pen boxes that come back.
  std::vector<int> boxes;
  for (int code : s.penBoxes)
  {
    const int bx = code / 1000, by = code % 1000;
    for (std::size_t i = 0; i < mBoxes.size(); ++i)
      if (mBoxes[i].x / kCellsPerTile == bx && mBoxes[i].y / kCellsPerTile == by)
        boxes.push_back(int(i));
  }
  s.penBoxes = boxes;
  s.penTimers.assign(s.penBoxes.size(), 0);
  for (auto& c : s.crushers)
    c.face = (c.rest + 1) * kCellsPerTile - 1;
  s.bridge.typed = 0;
}

// --- Words ---------------------------------------------------------------------------------

// The floor a cell row is on (rows from `@ floors`): 0 the top one.
int World::writersFloor(int cy) const
{
  // F3 above row 24, F2 above row 42, F1 below (blocks).
  if (cy < 24 * kCellsPerTile)
    return 0;
  if (cy < 42 * kCellsPerTile)
    return 1;
  return 2;
}

void World::syncWriterWords()
{
  auto& s = mWriters;
  for (auto& w : s.words)
    if (w.target == WordTarget::Enemy && w.enemy >= 0 && std::size_t(w.enemy) < mEnemies.size())
    {
      // On its chest: the row a runner on its floor shoots at.
      const Enemy& e = mEnemies[std::size_t(w.enemy)];
      w.x = e.x + e.w / 2 - w.w / 2;
      w.y = e.y - 3;
    }
}

bool World::writersTargetOnRunner(const Word& w) const
{
  const CellBox pb = mPlayer.box();
  if (w.target == WordTarget::Rect)
    return CellBox{w.x0 * kCellsPerTile, w.y0 * kCellsPerTile, (w.x1 - w.x0 + 1) * kCellsPerTile,
      (w.y1 - w.y0 + 1) * kCellsPerTile}
      .intersects(pb);
  if (w.target == WordTarget::Crusher)
    for (const auto& c : mWriters.crushers)
      if (CellBox{c.x0 * kCellsPerTile, 4, c.w * kCellsPerTile, (c.rest + 1) * kCellsPerTile - 4}.intersects(pb))
        return true;
  return false;
}

void World::setWordTarget(Word& w, bool on)
{
  auto& s = mWriters;
  switch (w.target)
  {
    case WordTarget::Rect:
    {
      std::size_t i = 0;
      for (int ty = w.y0; ty <= w.y1; ++ty)
        for (int tx = w.x0; tx <= w.x1; ++tx, ++i)
          mMap.setBlock(tx, ty, on && i < w.tiles.size() ? w.tiles[i] : Tile::Empty);
      break;
    }
    case WordTarget::Enemy:
      if (!on && w.enemy >= 0 && std::size_t(w.enemy) < mEnemies.size())
      {
        Enemy& e = mEnemies[std::size_t(w.enemy)];
        for (auto& b : s.blocks)
          if (b.enemy == w.enemy)
            stampBlock(b, false);
        if (e.alive)
          killEnemy(e);
      }
      break;
    case WordTarget::Crusher:
      for (auto& c : s.crushers)
      {
        c.gone = !on;
        c.t = 0;
        c.face = (c.rest + 1) * kCellsPerTile - 1;
      }
      break;
    case WordTarget::Egg:
      if (!on && s.egg.state == 0)
      {
        s.egg.state = 1;
        s.egg.x = float(w.x + w.w / 2 - 1);
        s.egg.y = float(w.y + 2);
        s.egg.vy = 0.0f;
        showMessage("THE EASTER EGG HATCHES A PLAN");
      }
      break;
  }
}

void World::strikeWord(Word& w)
{
  if (w.state != WordState::Intact)
    return;
  w.state = WordState::Striking;
  w.t = 0;
  playSound(Sfx::PenStrike);
  const Vec2 c = cellCenter(w.box());
  burst(c, rgb(230, 30, 40), rgb(255, 200, 200), 10, 1.4f);
}

// A Typo or a pencil puts the word back; the target returns once the
// runner is out of its way.
void World::finishWord(Word& w)
{
  if (writersTargetOnRunner(w))
  {
    w.state = WordState::Waiting;
    return;
  }
  w.state = WordState::Intact;
  w.t = 0;
  w.typo = -1;
  setWordTarget(w, true);
  playSound(Sfx::WordBack);
}

void World::updateWords()
{
  auto& s = mWriters;
  const auto& p = mPlayer;
  syncWriterWords();
  for (auto& w : s.words)
  {
    ++w.t;
    switch (w.state)
    {
      case WordState::Intact:
        // A word on a dead enemy goes with it.
        if (w.target == WordTarget::Enemy && w.enemy >= 0 && !mEnemies[std::size_t(w.enemy)].alive)
          w.state = WordState::Struck;
        break;
      case WordState::Striking:
        if (w.t == kWrStrikeLine)
          playSound(Sfx::WordFade);
        if (w.t >= kWrStrikeLine + kWrStrikeFade)
        {
          setWordTarget(w, false);
          w.state = WordState::Struck;
          w.t = 0;
        }
        break;
      case WordState::Struck:
        if (w.load && w.t >= kWrResketchWait)
        {
          // Never while the runner is under it or beside it.
          const CellBox zone{(w.x0 - 2) * kCellsPerTile, (w.y0 - 3) * kCellsPerTile, (w.x1 - w.x0 + 5) * kCellsPerTile,
            (w.y1 - w.y0 + 10) * kCellsPerTile};
          if (!zone.intersects(p.box()))
          {
            w.state = WordState::Sketching;
            w.t = 0;
            playSound(Sfx::PencilSketch);
          }
        }
        break;
      case WordState::Rewriting:
      {
        const bool there = w.typo >= 0 && mEnemies[std::size_t(w.typo)].alive;
        if (!there)
        {
          w.state = WordState::Struck;
          w.typo = -1;
          w.t = 0;
        }
        else if (w.t >= kWrRewrite)
          finishWord(w);
        else if (w.t % 6 == 0)
          playSound(Sfx::Scribble);
        break;
      }
      case WordState::Sketching:
        if (w.t >= kWrResketch)
          finishWord(w);
        break;
      case WordState::Waiting:
        if (!writersTargetOnRunner(w))
          finishWord(w);
        break;
    }
  }
}

// --- Shots ---------------------------------------------------------------------------------

// Red Pen shots strike the first word they touch; any shot types a key, tinks
// off a Writer's Block and spins a pitch card.
bool World::shotAtWriters(Projectile& pr, const CellBox& b)
{
  auto& s = mWriters;
  if (pr.kind == ShotKind::Enemy)
    return false;
  if (s.breakout)
    return false;
  syncWriterWords();
  if (redPen(pr))
  {
    const CellBox wide{b.x - 1, b.y, b.w + 2, b.h};
    for (auto& w : s.words)
      if (w.state == WordState::Intact && wide.intersects(w.box()))
      {
        strikeWord(w);
        return true;
      }
  }
  for (auto& k : s.keys)
    if (b.intersects({k.x * kCellsPerTile, k.y * kCellsPerTile, 2 * kCellsPerTile, kCellsPerTile}))
    {
      typeKey(k);
      return true;
    }
  for (const auto& wb : s.blocks)
  {
    const Enemy& e = mEnemies[std::size_t(wb.enemy)];
    if (e.alive && b.intersects(e.box()))
    {
      burst(cellCenter(b), rgb(200, 200, 200), rgb(120, 120, 120), 4, 1.0f);
      playSound(Sfx::BlockTink);
      return true;
    }
  }
  for (auto& c : s.cards)
    if (c.spin == 0 && b.intersects({c.x, c.y, 4, 3}))
    {
      c.spin = 24;
      playSound(Sfx::CardSpin);
    }
  return false;
}

bool World::writersBlocksDamage(const Enemy& e) const
{
  return e.kind == EnemyKind::WritersBlock; // only its word kills it
}

// --- The typewriter --------------------------------------------------------------------------

void World::typeKey(TypeKey& k)
{
  auto& br = mWriters.bridge;
  k.press = 6;
  if (br.text.empty() || br.typed >= int(br.text.size()))
  {
    playSound(Sfx::KeyClack);
    return;
  }
  if (br.text[std::size_t(br.typed)] != k.ch)
  {
    br.wrong = 10;
    playSound(Sfx::KeyWrong);
    return;
  }
  const int x = br.x0 + br.typed;
  if (x <= br.x1)
    mMap.setBlock(x, br.row, Tile::Solid);
  ++br.typed;
  playSound(Sfx::KeyClack);
  if (br.typed >= int(br.text.size()))
  {
    // The rest of the gap, if the sentence is shorter than the bridge.
    for (int tx = x + 1; tx <= br.x1; ++tx)
      mMap.setBlock(tx, br.row, Tile::Solid);
    showMessage(br.text);
    playSound(Sfx::CarriageBell);
  }
}

// The key a runner landed on types; so does one a shot hits.
void World::updateTypeKeys()
{
  auto& s = mWriters;
  auto& p = mPlayer;
  for (auto& k : s.keys)
    if (k.press > 0)
      --k.press;
  if (s.bridge.wrong > 0)
    --s.bridge.wrong;
  const bool grounded = p.state == PlayerState::OnGround && mMap.onSolidGround(p.box());
  if (grounded && !s.wasGrounded && alive(p))
  {
    const int mid = p.x + 1;
    for (auto& k : s.keys)
      if (p.y + 1 == k.y * kCellsPerTile && mid >= k.x * kCellsPerTile - 1 && mid <= k.x * kCellsPerTile + 4)
      {
        typeKey(k);
        break;
      }
  }
  s.wasGrounded = grounded;
}

// --- The crusher ---------------------------------------------------------------------------

void World::updateCrushers()
{
  auto& s = mWriters;
  auto& p = mPlayer;
  for (auto& c : s.crushers)
  {
    const int up = (c.rest + 1) * kCellsPerTile - 1, down = (c.to + 1) * kCellsPerTile - 1;
    if (c.gone)
    {
      c.face = up;
      continue;
    }
    if (!isOnScreen(c.box(), 8))
      continue;
    // Rest, the tell (it lifts a cell and creaks), the slam, the hold, the rise.
    c.t = (c.t + 1) % kWrCrusherPeriod;
    const int tellAt = kWrCrusherPeriod - 30 - kWrCrusherTell;
    if (c.t < tellAt)
      c.face = up;
    else if (c.t < tellAt + kWrCrusherTell)
    {
      c.face = up - 1;
      if (c.t == tellAt)
        playSound(Sfx::CrusherCreak);
    }
    else if (c.t < tellAt + kWrCrusherTell + 4)
    {
      c.face = std::min(down, c.face + (down - up + 3) / 4 + 1);
      if (c.face == down)
      {
        playSound(Sfx::CrusherSlam);
        mCamera.shake(6, 1.0f);
      }
    }
    else if (c.t < tellAt + kWrCrusherTell + 12)
      c.face = down;
    else
      c.face = std::max(up, c.face - 2);
    if (alive(p) && c.box().intersects(p.box()) && c.face > up)
      killPlayer();
  }
}

// --- Inkwells and Typos -----------------------------------------------------------------------

void World::updateInkwells()
{
  auto& s = mWriters;
  for (auto& k : s.inkwells)
  {
    if (!isOnScreen(boxAt(k.x, k.y, 4, 2), 8))
      continue;
    int living = 0;
    for (int slot : k.typos)
      living += slot >= 0 && mEnemies[std::size_t(slot)].alive ? 1 : 0;
    if (living >= 2)
      continue;
    if (++k.timer < kWrTypoEvery)
      continue;
    k.timer = 0;
    for (int slot : k.typos)
      if (slot >= 0 && !mEnemies[std::size_t(slot)].alive)
      {
        Enemy& e = mEnemies[std::size_t(slot)];
        const EnemyDef& def = enemyDef(e.def);
        e.alive = true;
        e.active = true;
        e.hp = def.hp;
        e.x = e.prevX = k.x + 1;
        e.y = e.prevY = k.y;
        e.cool = 0;
        e.tell = 0;
        e.drawSnap = true;
        for (auto& t : s.typos)
          if (t.enemy == slot)
          {
            t.word = -1;
            t.tell = 0;
          }
        burst(cellCenter(e.box()), rgb(30, 30, 60), rgb(80, 80, 140), 10, 1.2f);
        playSound(Sfx::InkPlop);
        break;
      }
  }
}

// A Typo can stand here (its feet on a floor or a string).
bool World::typoGround(int x, int y) const
{
  for (int cx = x; cx < x + 2; ++cx)
    if (mMap.solidTop(cx, y + 1))
      return true;
  return false;
}

void World::updateTypo(Enemy& e, const EnemyDef& def)
{
  auto& s = mWriters;
  Typo* t = nullptr;
  for (auto& k : s.typos)
    if (k.enemy == int(&e - mEnemies.data()))
      t = &k;
  if (!t)
    return;
  auto& p = mPlayer;
  if (e.cool > 0)
    --e.cool;
  if (!mMap.onSolidGround(e.box()))
  {
    int y = e.y;
    mMap.moveVertically(e.x, y, e.w, e.h, 2);
    e.y = y;
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return;
  }
  const int floor = writersFloor(e.y);
  const int mid = e.x + 1;
  // Busy rewriting.
  for (const auto& w : s.words)
    if (w.state == WordState::Rewriting && w.typo == t->enemy)
      return;
  // The nearest struck word on its floor (never an enemy's, never the egg).
  if (t->word >= 0 && s.words[std::size_t(t->word)].state != WordState::Struck)
    t->word = -1;
  if (t->word < 0)
  {
    int best = kWrTypoReach + 1;
    for (std::size_t i = 0; i < s.words.size(); ++i)
    {
      const auto& w = s.words[i];
      if (w.state != WordState::Struck || w.target == WordTarget::Enemy || w.target == WordTarget::Egg ||
          writersFloor(w.y) != floor)
        continue;
      const int d = std::abs(w.x + w.w / 2 - mid);
      if (d < best)
      {
        best = d;
        t->word = int(i);
      }
    }
  }
  int tx = -1;
  if (t->word >= 0)
  {
    auto& w = s.words[std::size_t(t->word)];
    tx = w.x + w.w / 2;
    if (std::abs(tx - mid) <= 2)
    {
      // Turbo: they stand and scribble on nothing.
      if (p.turbo > 0)
      {
        if (e.timer % 8 == 0)
          playSound(Sfx::Scribble);
        return;
      }
      w.state = WordState::Rewriting;
      w.typo = t->enemy;
      w.t = 0;
      return;
    }
  }
  else if (alive(p) && writersFloor(p.y) == floor && std::abs(p.x + 1 - mid) <= kWrTypoSight)
  {
    // The runner: a squeak and a scramble of its letter first.
    const int dir = sgn(p.x + 1 - mid);
    if (dir != 0 && dir != e.dir && t->tell == 0)
    {
      t->tell = kWrTypoTell;
      playSound(Sfx::TypoSqueak);
    }
    if (t->tell > 0)
    {
      if (--t->tell == 0)
        e.dir = dir;
      return;
    }
    tx = p.x + 1;
  }
  if (e.timer % std::max(1, def.stepEvery) != 0)
    return;
  int dir = tx >= 0 ? sgn(tx - mid) : (t->wander != 0 ? t->wander : 1);
  if (dir == 0)
    return;
  // Never off the end of a floor or a string (and never onto a ladder).
  const int ahead = dir > 0 ? e.x + e.w : e.x - 1;
  const bool ground = mMap.solidTop(ahead, e.y + 1) || mMap.solidTop(ahead, e.y + 2) || mMap.solidTop(ahead, e.y);
  const bool wall = dir > 0 ? mMap.touchingRightWall(e.box()) : mMap.touchingLeftWall(e.box());
  if (!ground || wall || mMap.ladder(ahead & ~1, e.y + 1))
  {
    if (tx < 0)
      t->wander = -dir;
    return;
  }
  e.dir = dir;
  int x = e.x, y = e.y;
  mMap.moveHorizontallyWithStairStepping(x, y, e.w, e.h, dir);
  e.x = x;
  e.y = y;
}

// --- Deadline -------------------------------------------------------------------------------

void World::updateDeadline(Enemy& e, const EnemyDef& def)
{
  auto& s = mWriters;
  Deadline* d = nullptr;
  for (auto& k : s.deadlines)
    if (k.enemy == int(&e - mEnemies.data()))
      d = &k;
  if (!d)
    return;
  auto& p = mPlayer;
  if (e.cool > 0)
    --e.cool;
  if (!mMap.onSolidGround(e.box()))
  {
    int y = e.y;
    mMap.moveVertically(e.x, y, e.w, e.h, 2);
    e.y = y;
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return;
  }
  // Every lap of the minute hand it speeds up; the bell rings first.
  if (d->step < 4)
  {
    ++d->lap;
    e.tell = d->lap >= kWrDeadlineLap - kWrDeadlineBell ? kWrDeadlineLap - d->lap : 0;
    if (d->lap == kWrDeadlineLap - kWrDeadlineBell)
      playSound(Sfx::DeadlineBell);
    else if (d->lap % 15 == 0 && isOnScreen(e.box(), 0))
      playSound(Sfx::DeadlineTick); // the minute hand ticking round, a quarter lap at a time
    if (d->lap >= kWrDeadlineLap)
    {
      d->lap = 0;
      ++d->step;
    }
  }
  if (alive(p) && e.cool == 0 && p.mercy == 0 && e.box().intersects(p.hitBox()))
  {
    hurtPlayer(d->step >= 4 ? 2 : 1);
    e.cool = def.cooldown;
  }
  d->part += kWrDeadlineSpeeds[std::clamp(d->step, 0, 4)];
  while (d->part >= 4)
  {
    d->part -= 4;
    const CellBox b = e.box();
    const int ahead = e.dir > 0 ? b.right() + 1 : b.x - 1;
    const bool wall = e.dir > 0 ? mMap.touchingRightWall(b) : mMap.touchingLeftWall(b);
    if (wall || !mMap.solidTop(ahead, b.bottom() + 1) || (e.dir > 0 && b.right() + 1 >= d->x1) ||
        (e.dir < 0 && b.x - 1 < d->x0))
    {
      e.dir = -e.dir;
      break;
    }
    e.x += e.dir;
  }
}

// --- Writer's Block -----------------------------------------------------------------------------

// Its 2 x 2 blocks solid while it sits (never on top of the runner).
bool World::stampBlock(WritersBlock& b, bool on)
{
  if (b.enemy < 0 || std::size_t(b.enemy) >= mEnemies.size())
    return false;
  if (b.stampX >= 0)
  {
    for (int ty = b.stampY; ty < b.stampY + 2; ++ty)
      for (int tx = b.stampX; tx < b.stampX + 2; ++tx)
        mMap.setBlock(tx, ty, Tile::Empty);
    b.stampX = b.stampY = -1;
  }
  const Enemy& e = mEnemies[std::size_t(b.enemy)];
  if (!on || !e.alive || e.x % kCellsPerTile != 0)
    return false;
  if (e.box().intersects(mPlayer.box()))
    return false;
  b.stampX = e.x / kCellsPerTile;
  b.stampY = (e.y - e.h + 1) / kCellsPerTile;
  for (int ty = b.stampY; ty < b.stampY + 2; ++ty)
    for (int tx = b.stampX; tx < b.stampX + 2; ++tx)
      mMap.setBlock(tx, ty, Tile::Solid);
  return true;
}

void World::updateWritersBlock(Enemy& e, const EnemyDef& def)
{
  auto& s = mWriters;
  WritersBlock* b = nullptr;
  for (auto& k : s.blocks)
    if (k.enemy == int(&e - mEnemies.data()))
      b = &k;
  if (!b)
    return;
  auto& p = mPlayer;
  if (b->word >= 0 && s.words[std::size_t(b->word)].state != WordState::Intact)
    return; // struck: it fades where it is
  if (e.cool > 0)
    --e.cool;
  if (b->slide != 0)
  {
    const int dir = sgn(b->slide);
    for (int k = 0; k < 2 && b->slide != 0; ++k)
    {
      const CellBox nb = e.box();
      const int ahead = dir > 0 ? nb.right() + 1 : nb.x - 1;
      const bool wall = dir > 0 ? mMap.touchingRightWall(nb) : mMap.touchingLeftWall(nb);
      if (wall || !mMap.solidTop(ahead, nb.bottom() + 1))
      {
        b->slide = 0;
        break;
      }
      CellBox moved = nb;
      moved.x += dir;
      if (alive(p) && moved.intersects(p.hitBox()))
      {
        // It shoves the runner a block along; if they can't go, it stops.
        int x = p.x;
        if (mMap.moveHorizontally(x, p.y, Player::kWidth, p.height(), dir * kCellsPerTile) != MoveResult::Completed)
        {
          b->slide = 0;
          break;
        }
        p.x = x;
        playSound(Sfx::BlockThud);
      }
      e.x += dir;
      b->slide -= dir;
    }
    if (b->slide == 0)
    {
      // Settle on a whole block, then sit.
      if (e.x % kCellsPerTile != 0)
      {
        const int dir2 = sgn(e.dir) != 0 ? sgn(e.dir) : 1;
        CellBox nb = e.box();
        nb.x += dir2;
        if (!mMap.overlapsSolid(nb) && !nb.intersects(p.hitBox()))
          e.x += dir2;
        else
          e.x -= dir2;
      }
      e.cool = def.cooldown;
      playSound(Sfx::BlockThud);
      burst(cellCenter({e.x, e.y, e.w, 1}), rgb(200, 190, 170), rgb(150, 140, 120), 8, 1.0f);
    }
    return;
  }
  if (b->stampX < 0)
    stampBlock(*b, true); // sits as soon as the runner is clear
  if (e.tell > 0)
  {
    if (--e.tell == 0)
    {
      stampBlock(*b, false);
      b->slide = e.dir * kWrBlockSlide;
      // Stop a few blocks short of the runner, in their way.
      const int gap = std::abs(p.x + 1 - (e.x + e.w / 2)) - 8;
      b->slide = e.dir * std::clamp(gap, 0, kWrBlockSlide);
      if (b->slide == 0)
        stampBlock(*b, true);
    }
    return;
  }
  if (e.cool > 0 || !alive(p) || std::abs(p.y - e.y) > 2)
    return;
  // Slides into the path of a runner coming its way (one who has got past
  // it is left alone).
  const int dx = (e.x + e.w / 2) - (p.x + 1);
  if (std::abs(dx) > kWrBlockSight || std::abs(dx) <= 10 || sgn(dx) != p.facing)
    return;
  e.dir = -sgn(dx);
  e.tell = kWrBlockTell;
  playSound(Sfx::BlockScrape);
}

// --- The sketch guard -------------------------------------------------------------------------

void World::updateSketchGuard(Enemy& e, const EnemyDef& def)
{
  auto& p = mPlayer;
  if (e.cool > 0)
    --e.cool;
  const int dx = (p.x + 1) - (e.x + e.w / 2);
  if (dx != 0)
    e.dir = sgn(dx);
  if (e.tell > 0)
  {
    // It raises its pencil rifle, then fires.
    if (--e.tell == 0)
    {
      spawnProjectile(ShotKind::Enemy, e.dir > 0 ? e.x + e.w : e.x - 1, e.y - 2, e.dir, 0);
      playSound(Sfx::EnemyShot);
      e.cool = def.cooldown;
    }
    return;
  }
  if (e.cool == 0 && alive(p) && std::abs(dx) < kWrGuardRange && std::abs(p.y - e.y) < 8 && isOnScreen(e.box(), 0))
    e.tell = def.tell;
}

// --- Every frame ---------------------------------------------------------------------------

PlayerInput World::writersInput(const PlayerInput& raw)
{
  if (!mWriters.breakout)
    return raw;
  return breakoutInput(raw);
}

void World::updateWriters(const PlayerInput& input)
{
  (void)input;
  auto& s = mWriters;
  auto& p = mPlayer;
  if (s.breakout)
  {
    updateBreakout();
    return;
  }
  updateWords();
  updateTypeKeys();
  updateCrushers();
  updateInkwells();
  // A Typo's infection scrambles a word on the HUD for a while.
  if (p.virus > 0 && s.scramble == 0)
    for (const auto& t : s.typos)
      if (mEnemies[std::size_t(t.enemy)].alive && mEnemies[std::size_t(t.enemy)].box().intersects(p.hitBox()))
      {
        s.scramble = 120;
        showMessage("YUO'VE GOT A TYOP");
      }
  if (s.scramble > 0)
    --s.scramble;
  // Pen boxes come back for a runner nearly out of Red Pen.
  const int pen = p.weapon == Weapon::Proto && p.proto == int(ProtoId::RedPen) ? p.ammo : 0;
  for (std::size_t i = 0; i < s.penBoxes.size(); ++i)
  {
    auto& b = mBoxes[std::size_t(s.penBoxes[i])];
    if (b.alive)
    {
      s.penTimers[i] = 0;
      continue;
    }
    bool lying = false;
    for (const auto& it : mItems)
      lying = lying || (!it.taken && it.kind == ItemKind::Proto && std::abs(it.x - b.x) < 4 && std::abs(it.y - b.y) < 8);
    if (lying || pen >= kWrPenLow)
    {
      s.penTimers[i] = 0;
      continue;
    }
    if (++s.penTimers[i] >= kWrPenRespawn)
    {
      s.penTimers[i] = 0;
      b.alive = true;
      b.flash = 8;
      burst(cellCenter(b.box()), rgb(230, 30, 40), rgb(255, 255, 255), 10, 1.4f);
      playSound(Sfx::PencilSketch);
    }
  }
  // Pitch cards spin down.
  for (auto& c : s.cards)
    if (c.spin > 0)
      --c.spin;
  // The original pitch on the top shelf.
  if (s.pitchX >= 0 && !s.pitchTaken && alive(p) && p.box().intersects(boxAt(s.pitchX, s.pitchY, 4, 3)))
  {
    s.pitchTaken = true;
    addScore(10000, cellCenter(boxAt(s.pitchX, s.pitchY, 4, 3)));
    showMessage("THE ORIGINAL PITCH: WE SELL THE GUNS, THEY RUN THE GAUNTLET");
    playSound(Sfx::LettersComplete);
  }
  updateWritersEgg();
  // The bot's way through: past every step that is done.
  while (s.routeAt < int(s.route.size()) && writersStepDone(s.routeAt))
    ++s.routeAt;
}

void World::updateWritersEgg()
{
  auto& s = mWriters;
  auto& g = s.egg;
  auto& p = mPlayer;
  int active = 0;
  for (const auto& cp : mCheckpoints)
    active += cp.active ? 1 : 0;
  switch (g.state)
  {
    case 1: // falling to the nearest string or floor
      g.vy = std::min(2.0f, g.vy + 0.25f);
      for (float k = 0; k < g.vy; k += 1.0f)
      {
        if (mMap.solidTop(int(g.x), int(g.y) + 1) || mMap.solidTop(int(g.x) + 1, int(g.y) + 1))
        {
          g.state = 2;
          g.t = 0;
          playSound(Sfx::Thud);
          break;
        }
        g.y += 1.0f;
      }
      if (g.y > float(mMap.height()))
        g.state = 4;
      break;
    case 2: // hatching
      if (++g.t >= kWrEggHatch)
      {
        g.state = 3;
        g.checkpoints = active;
        showMessage("CHEEP!");
        playSound(Sfx::EggCrack);
      }
      break;
    case 3: // a chick at the runner's heels, home at the next checkpoint
    {
      const float want = float(p.x + 1 - p.facing * 4);
      g.x += std::clamp(want - g.x, -1.0f, 1.0f);
      g.y += std::clamp(float(p.y) - g.y, -2.0f, 2.0f);
      if (g.cheep > 0)
        --g.cheep;
      if (p.state == PlayerState::Jumping && p.frames == 1)
      {
        g.cheep = 10;
        playSound(Sfx::EggCheep);
      }
      if (active > g.checkpoints)
        g.state = 4;
      break;
    }
    default:
      break;
  }
}

bool World::writersStepDone(int i) const
{
  const auto& s = mWriters;
  const auto& st = s.route[std::size_t(i)];
  const auto& p = mPlayer;
  if (st.act == "type")
    return s.bridge.typed >= int(s.bridge.text.size());
  // A spot: reached.
  return std::abs(p.x + 1 - st.x) <= 4 && std::abs(p.y - st.y) <= 2 && p.state == PlayerState::OnGround;
}

void World::resetWriters()
{
  auto& s = mWriters;
  if (!s.on)
    return;
  for (auto& b : s.blocks)
  {
    b.slide = 0;
    const Enemy& e = mEnemies[std::size_t(b.enemy)];
    if (e.alive && e.x % kCellsPerTile != 0)
      mEnemies[std::size_t(b.enemy)].x -= e.x % kCellsPerTile;
    mEnemies[std::size_t(b.enemy)].tell = 0;
  }
  if (s.egg.state == 1 || s.egg.state == 2)
    s.egg.state = 4;
  s.scramble = 0;
  s.wasGrounded = true;
  s.ball = false;
  // The route goes back to the furthest step the runner is at or past:
  // the steps are in order along the way, which zigzags up the floors.
  const auto& p = mPlayer;
  while (s.routeAt > 0)
  {
    const auto& st = s.route[std::size_t(s.routeAt - 1)];
    const bool behind = writersFloor(st.y) < writersFloor(p.y) ||
      (writersFloor(st.y) == writersFloor(p.y) && (st.dir > 0 ? st.x > p.x + 8 : st.x < p.x - 8));
    if (!behind)
      break;
    --s.routeAt;
  }
}

// --- Breakout ---------------------------------------------------------------------------------

PlayerInput World::breakoutInput(const PlayerInput& raw)
{
  auto& s = mWriters;
  auto& p = mPlayer;
  PlayerInput in = raw;
  in.jump = Button{};
  in.up = in.down = false;
  if (in.fire.triggered && !s.ball && s.balls > 0 && alive(p))
  {
    s.ball = true;
    s.bx = float(p.x) + 1.0f;
    s.by = float(p.y - 6);
    const float d = 2.0f * 0.7071f;
    s.bvx = float(p.facing) * d;
    s.bvy = -d;
    playSound(Sfx::Shot);
  }
  in.fire = Button{};
  // Walking only, a cell and a half a frame.
  if (alive(p) && p.state == PlayerState::OnGround && (in.left != in.right) && mStats.frames % 2 == 0)
  {
    const int dir = in.right ? 1 : -1;
    int x = p.x;
    mMap.moveHorizontally(x, p.y, Player::kWidth, p.height(), dir);
    p.x = x;
  }
  return in;
}

void World::updateBreakout()
{
  auto& s = mWriters;
  auto& p = mPlayer;
  if (!s.ball)
    return;
  // Four quarter steps a frame (speed 2): walls, the ceiling, bricks, the runner.
  for (int k = 0; k < 4 && s.ball; ++k)
  {
    const float nx = s.bx + s.bvx * 0.25f, ny = s.by + s.bvy * 0.25f;
    const int cx = int(std::floor(nx)), cy = int(std::floor(ny));
    bool bounced = false;
    // Bricks.
    for (auto& b : s.bricks)
    {
      if (!b.alive || !CellBox{b.x, b.y, 4, 2}.contains(cx, cy))
        continue;
      const int ox = int(std::floor(s.bx)), oy = int(std::floor(s.by));
      if (oy < b.y || oy >= b.y + 2)
        s.bvy = -s.bvy;
      else if (ox < b.x || ox >= b.x + 4)
        s.bvx = -s.bvx;
      else
        s.bvy = -s.bvy;
      bounced = true;
      if (--b.hp <= 0)
      {
        b.alive = false;
        ++s.broken;
        const Vec2 c = cellCenter({b.x, b.y, 4, 2});
        burst(c, b.red ? rgb(220, 40, 50) : rgb(40, 40, 50), rgb(255, 250, 230), 10, 1.4f);
        addScore(100, c);
        playSound(Sfx::BrickBreak);
        if (b.gem)
        {
          Item it;
          it.kind = ItemKind::Gem;
          it.x = it.prevX = b.x + 1;
          it.y = it.prevY = b.y + 2;
          it.variant = s.broken % 4;
          it.pickupDelay = 2;
          mItems.push_back(it);
        }
      }
      else
        playSound(Sfx::BrickCrack);
      break;
    }
    if (bounced)
      continue;
    // The runner, the paddle: the angle comes from where it hits.
    const CellBox pb = p.box();
    if (s.bvy > 0 && alive(p) && cx >= pb.x - 1 && cx <= pb.right() + 1 && cy >= pb.y && cy <= pb.y + 1)
    {
      const float rel = (nx - (float(pb.x) + 1.5f)) / 2.5f;
      const float a = std::abs(rel) < 0.34f ? 1.047f : std::abs(rel) < 0.75f ? 0.785f : 0.524f; // 60, 45, 30
      const float dir = rel < -0.05f ? -1.0f : rel > 0.05f ? 1.0f : (s.bvx < 0 ? -1.0f : 1.0f);
      s.bvx = dir * 2.0f * std::cos(a);
      s.bvy = -2.0f * std::sin(a);
      playSound(Sfx::PaddleBoing);
      continue;
    }
    // Past the runner, down to the floor: lost.
    if (cy > pb.bottom())
    {
      s.ball = false;
      --s.balls;
      playSound(Sfx::BallLost);
      if (s.balls <= 0)
      {
        showMessage("THE SHOW IS OVER");
        mBonusFailed = true;
        mState = WorldState::Done;
        mStateFrames = 0;
      }
      else
        showMessage(s.balls == 1 ? "LAST BALL" : "BALL LOST");
      break;
    }
    // Walls and the ceiling.
    if (mMap.solid(cx, cy))
    {
      const int ox = int(std::floor(s.bx)), oy = int(std::floor(s.by));
      const bool side = mMap.solid(cx, oy), top = mMap.solid(ox, cy);
      if (side || !top)
        s.bvx = -s.bvx;
      if (top || !side)
        s.bvy = -s.bvy;
      playSound(Sfx::Bonk);
      continue;
    }
    s.bx = nx;
    s.by = ny;
  }
}

// --- Savegames ---------------------------------------------------------------------------------

bool World::writersCanSave() const
{
  const auto& s = mWriters;
  if (s.breakout || s.egg.state == 1 || s.egg.state == 2)
    return false;
  for (const auto& w : s.words)
    if (w.state == WordState::Striking || w.state == WordState::Rewriting || w.state == WordState::Sketching ||
        w.state == WordState::Waiting)
      return false;
  for (const auto& b : s.blocks)
    if (b.slide != 0 || mEnemies[std::size_t(b.enemy)].tell > 0)
      return false;
  return true;
}

// [route at, bridge typed, pitch taken, egg state] [word: state, t]
// [crusher: gone, t] [block: stamp x, stamp y] [inkwell timers] [pen timers]
std::vector<int> World::writersSave() const
{
  const auto& s = mWriters;
  std::vector<int> v{s.routeAt, s.bridge.typed, s.pitchTaken ? 1 : 0, s.egg.state == 3 ? 3 : s.egg.state == 0 ? 0 : 4};
  for (const auto& w : s.words)
  {
    v.push_back(w.state == WordState::Intact ? 0 : 1);
    v.push_back(w.state == WordState::Intact ? 0 : w.t);
  }
  for (const auto& c : s.crushers)
  {
    v.push_back(c.gone ? 1 : 0);
    v.push_back(c.t);
  }
  for (const auto& b : s.blocks)
  {
    v.push_back(b.stampX);
    v.push_back(b.stampY);
  }
  for (const auto& k : s.inkwells)
    v.push_back(k.timer);
  for (int t : s.penTimers)
    v.push_back(t);
  return v;
}

bool World::validWritersSave(const std::vector<int>& v) const
{
  const auto& s = mWriters;
  return v.size() == 4 + s.words.size() * 2 + s.crushers.size() * 2 + s.blocks.size() * 2 + s.inkwells.size() +
    s.penTimers.size();
}

void World::loadWriters(const std::vector<int>& v)
{
  auto& s = mWriters;
  if (!validWritersSave(v))
    return;
  std::size_t at = 0;
  s.routeAt = v[at++];
  // The bridge: as many blocks as letters typed.
  for (int tx = s.bridge.x0; tx <= s.bridge.x1; ++tx)
    mMap.setBlock(tx, s.bridge.row, Tile::Empty);
  s.bridge.typed = 0;
  const int typed = v[at++];
  for (int i = 0; i < typed && i <= s.bridge.x1 - s.bridge.x0; ++i)
    mMap.setBlock(s.bridge.x0 + i, s.bridge.row, Tile::Solid);
  s.bridge.typed = typed;
  if (typed >= int(s.bridge.text.size()))
    for (int tx = s.bridge.x0; tx <= s.bridge.x1; ++tx)
      mMap.setBlock(tx, s.bridge.row, Tile::Solid);
  s.pitchTaken = v[at++] != 0;
  s.egg.state = v[at++];
  for (auto& w : s.words)
  {
    const bool struck = v[at++] != 0;
    w.t = v[at++];
    w.typo = -1;
    w.state = struck ? WordState::Struck : WordState::Intact;
    if (w.target == WordTarget::Rect)
      setWordTarget(w, !struck);
  }
  for (auto& c : s.crushers)
  {
    c.gone = v[at++] != 0;
    c.t = v[at++];
    c.face = (c.rest + 1) * kCellsPerTile - 1;
  }
  for (auto& b : s.blocks)
  {
    stampBlock(b, false);
    b.slide = 0;
    const int sx = v[at++], sy = v[at++];
    if (sx >= 0)
    {
      b.stampX = sx;
      b.stampY = sy;
      for (int ty = sy; ty < sy + 2; ++ty)
        for (int tx = sx; tx < sx + 2; ++tx)
          mMap.setBlock(tx, ty, Tile::Solid);
    }
  }
  for (auto& k : s.inkwells)
    k.timer = v[at++];
  for (int& t : s.penTimers)
    t = v[at++];
  s.wasGrounded = true;
  syncWriterWords();
}

} // namespace gr
