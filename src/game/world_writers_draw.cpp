// Level 38, Writers' Room: drawing (handwritten words and their red
// strike-outs, the walls, doors and the filing cabinet they name, the red
// strings and their pins, the typewriter's keys and the paper with its
// sentence, the bridge, the crusher press, inkwells, Typos, Deadlines,
// Writer's Blocks, the sketch guard, the pitch cards, the egg and its
// chick, the original pitch, and the Breakout bonus's bricks and ball). See
// world_writers.cpp for the logic.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace gr
{

namespace
{

constexpr float kCellPx = float(kCellSize) * kPixelScale; // 32
constexpr float kTilePx = kCellPx * 2.0f;
constexpr Color kInk = rgb(30, 30, 50);
constexpr Color kRedInk = rgb(220, 30, 40);
constexpr Color kPaper = rgb(246, 240, 222);
constexpr Color kPencil = rgb(110, 110, 120);
constexpr Color kHudInk = rgb(10, 8, 20);

void box(Renderer& r, const CellBox& b, float camX, float camY, Color c)
{
  r.fillRect(float(b.x) * kCellPx - camX, float(b.y) * kCellPx - camY, float(b.w) * kCellPx, float(b.h) * kCellPx, c);
}

float px(int cx, float camX) { return float(cx) * kCellPx - camX; }

// A word in marker: `shown` letters of it (rewriting brings them back one by one).
void drawWord(Renderer& r, const Word& w, float camX, float camY, int shown, float alpha, Color ink)
{
  std::string text = w.text.substr(0, std::size_t(std::clamp(shown, 0, int(w.text.size()))));
  if (text.empty())
    return;
  const float cx = (float(w.x) + float(w.w) * 0.5f) * kCellPx - camX;
  r.drawText(text, cx - float(w.text.size() - text.size()) * 9.0f, float(w.y) * kCellPx - camY + 6.0f,
    {40.0f, ink, rgb(255, 250, 235), true}, Align::Center, alpha);
}

// Back of a writers' room tile wall: ruled paper.
void paperBlock(Renderer& r, int tx, int ty, float camX, float camY, Color base)
{
  const float x = float(tx) * kTilePx - camX, y = float(ty) * kTilePx - camY;
  r.fillRect(x, y, kTilePx, kTilePx, base);
  r.fillRect(x, y + kTilePx * 0.5f, kTilePx, 2.0f, rgb(150, 180, 220));
  r.fillRect(x, y, 2.0f, kTilePx, rgb(200, 190, 170));
}

} // namespace

bool World::writersDrawsBlock(int tx, int ty) const
{
  const auto& s = mWriters;
  for (const auto& k : s.keys)
    if (ty == k.y && (tx == k.x || tx == k.x + 1))
      return true;
  for (const auto& b : s.blocks)
    if (b.stampX >= 0 && tx >= b.stampX && tx < b.stampX + 2 && ty >= b.stampY && ty < b.stampY + 2)
      return true;
  for (const auto& w : s.words)
    if (w.target == WordTarget::Rect && tx >= w.x0 && tx <= w.x1 && ty >= w.y0 && ty <= w.y1 &&
        mMap.block(tx, ty) == Tile::Solid)
      return true;
  return false;
}

void World::drawWritersBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)alpha;
  const auto& s = mWriters;
  if (s.breakout)
  {
    // The bricks: letters in typewriter ink (red ink takes two hits).
    for (const auto& b : s.bricks)
    {
      if (!b.alive)
        continue;
      box(r, {b.x, b.y, 4, 2}, camX, camY, kPaper);
      box(r, {b.x, b.y + 1, 4, 1}, camX, camY, rgb(225, 218, 196));
      const Color ink = b.red ? (b.hp > 1 ? kRedInk : rgb(240, 130, 130)) : kInk;
      const char t[2] = {b.ch, 0};
      r.drawText(t, px(b.x + 2, camX), px(b.y, camY) + 4.0f, {44.0f, ink, 0}, Align::Center);
      if (b.gem)
        r.fillRect(px(b.x + 3, camX) + 10.0f, px(b.y, camY) + 6.0f, 8.0f, 8.0f, rgb(90, 220, 255));
    }
    return;
  }
  // Pitch cards on the corkboard (a shot spins them).
  for (const auto& c : s.cards)
  {
    const float sx = std::cos(float(c.spin) * 0.5f);
    const float w = 4.0f * kCellPx * std::max(0.1f, std::abs(sx));
    const float x = px(c.x + 2, camX) - w * 0.5f;
    r.fillRect(x, px(c.y, camY), w, 3.0f * kCellPx, kPaper);
    r.fillRect(px(c.x + 2, camX) - 4.0f, px(c.y, camY) - 4.0f, 8.0f, 8.0f, kRedInk);
    if (std::abs(sx) > 0.5f)
      r.drawText(c.text, px(c.x + 2, camX), px(c.y, camY) + 10.0f, {14.0f, kInk, 0}, Align::Center);
  }
  // The red strings and their pins.
  for (std::size_t i = 0; i + 3 < s.pins.size(); i += 2)
  {
    const float x0 = (float(s.pins[i]) + 1.0f) * kCellPx - camX, y0 = float(s.pins[i + 1]) * kCellPx - camY;
    const float x1 = (float(s.pins[i + 2]) + 1.0f) * kCellPx - camX, y1 = float(s.pins[i + 3]) * kCellPx - camY;
    r.drawLine(x0, y0, x1, y1, 6.0f, kRedInk);
  }
  for (std::size_t i = 0; i + 1 < s.pins.size(); i += 2)
  {
    const float x = (float(s.pins[i]) + 1.0f) * kCellPx - camX, y = float(s.pins[i + 1]) * kCellPx - camY;
    r.fillRect(x - 10.0f, y - 10.0f, 20.0f, 20.0f, rgb(240, 200, 40));
  }
  // The paper over the keyboard with the sentence, the letters typed so far in ink.
  if (!s.keys.empty() && !s.bridge.text.empty())
  {
    const int x0 = s.keys.front().x * kCellsPerTile, x1 = (s.keys.back().x + 2) * kCellsPerTile;
    const int y = (s.keys.front().y - 12) * kCellsPerTile;
    box(r, {x0, y, x1 - x0, 8}, camX, camY, kPaper);
    const float cx = px((x0 + x1) / 2, camX);
    r.drawText(s.bridge.text, cx, px(y + 2, camY), {56.0f, rgb(210, 205, 190), 0}, Align::Center);
    const std::string typed = s.bridge.text.substr(0, std::size_t(std::max(0, s.bridge.typed)));
    // Typed letters over the faint ones, left aligned with them.
    const float full = float(r.text(s.bridge.text, {56.0f, 0, 0}).w());
    r.drawText(typed, cx - full * 0.5f, px(y + 2, camY), {56.0f, kInk, 0}, Align::Left);
    if (s.bridge.wrong > 0)
      r.drawText("CLACK", cx, px(y + 6, camY), {24.0f, kRedInk, 0}, Align::Center);
  }
  // Inkwells.
  for (const auto& k : s.inkwells)
  {
    box(r, {k.x, k.y - 2, 4, 3}, camX, camY, rgb(40, 40, 70));
    box(r, {k.x + 1, k.y - 3, 2, 1}, camX, camY, rgb(20, 20, 40));
  }
  // The original pitch on the top shelf.
  if (s.pitchX >= 0 && !s.pitchTaken)
  {
    box(r, {s.pitchX, s.pitchY - 2, 4, 3}, camX, camY, rgb(255, 245, 200));
    r.drawText("PITCH", px(s.pitchX + 2, camX), px(s.pitchY - 2, camY) + 8.0f, {16.0f, kRedInk, 0}, Align::Center);
  }
  (void)frame;
}

void World::drawWritersFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)alpha;
  const auto& s = mWriters;
  if (s.breakout)
  {
    if (s.ball)
      r.fillRect((s.bx - 0.5f) * kCellPx - camX, (s.by - 0.5f) * kCellPx - camY, kCellPx, kCellPx, kRedInk);
    return;
  }
  // The walls, doors and the cabinet the words name, drawn over what hides
  // behind them; fading while struck.
  for (const auto& w : s.words)
  {
    if (w.target != WordTarget::Rect)
      continue;
    float a = 1.0f;
    if (w.state == WordState::Striking && w.t > kWrStrikeLine)
      a = 1.0f - float(w.t - kWrStrikeLine) / float(kWrStrikeFade);
    else if (w.gone())
      continue;
    for (int ty = w.y0; ty <= w.y1; ++ty)
      for (int tx = w.x0; tx <= w.x1; ++tx)
        if (w.tiles.size() > std::size_t((ty - w.y0) * (w.x1 - w.x0 + 1) + (tx - w.x0)) &&
            w.tiles[std::size_t((ty - w.y0) * (w.x1 - w.x0 + 1) + (tx - w.x0))] == Tile::Solid)
          paperBlock(r, tx, ty, camX, camY, withAlpha(rgb(236, 228, 205), int(a * 255.0f)));
  }
  // Writer's Blocks sitting (or sliding): a slab of stone with dust.
  for (const auto& b : s.blocks)
  {
    const Enemy& e = mEnemies[std::size_t(b.enemy)];
    if (!e.alive)
      continue;
    const float shake = e.tell > 0 ? float((frame / 2) % 2) * 3.0f : 0.0f;
    r.fillRect(px(e.x, camX) + shake, px(e.y - e.h + 1, camY), float(e.w) * kCellPx, float(e.h) * kCellPx, rgb(150, 145, 140));
    r.fillRect(px(e.x, camX) + shake, px(e.y - e.h + 1, camY), float(e.w) * kCellPx, 6.0f, rgb(190, 185, 180));
  }
  // Keys.
  for (const auto& k : s.keys)
  {
    const bool next = s.bridge.typed < int(s.bridge.text.size()) && s.bridge.text[std::size_t(s.bridge.typed)] == k.ch;
    const float down = k.press > 0 ? 6.0f : 0.0f;
    const float x = float(k.x) * kTilePx - camX, y = float(k.y) * kTilePx - camY + down;
    r.fillRect(x + 4.0f, y, kTilePx * 2.0f - 8.0f, kTilePx - down, rgb(40, 40, 46));
    r.fillRect(x + 8.0f, y + 4.0f, kTilePx * 2.0f - 16.0f, kTilePx * 0.6f, next ? rgb(255, 230, 120) : rgb(230, 225, 210));
    r.drawText(k.ch == ' ' ? "SPACE" : std::string(1, k.ch), x + kTilePx, y + 6.0f, {k.ch == ' ' ? 14.0f : 28.0f, kInk, 0},
      Align::Center);
  }
  // The bridge's blocks: paper strips.
  for (int i = 0; i < std::min(s.bridge.typed, s.bridge.x1 - s.bridge.x0 + 1); ++i)
    paperBlock(r, s.bridge.x0 + i, s.bridge.row, camX, camY, kPaper);
  if (s.bridge.typed >= int(s.bridge.text.size()))
    for (int tx = s.bridge.x0 + int(s.bridge.text.size()); tx <= s.bridge.x1; ++tx)
      paperBlock(r, tx, s.bridge.row, camX, camY, kPaper);
  // The crusher press.
  for (const auto& c : s.crushers)
  {
    const CellBox b = c.box();
    const float a = c.gone ? 0.0f : 1.0f;
    if (a <= 0.0f)
      continue;
    box(r, {b.x + 3, b.y, 2, b.h - 4}, camX, camY, rgb(120, 120, 130));
    box(r, {b.x, b.bottom() - 3, b.w, 4}, camX, camY, rgb(70, 70, 80));
    box(r, {b.x, b.bottom(), b.w, 1}, camX, camY, rgb(200, 60, 50));
  }
  // Typos, Deadlines, the sketch guard.
  for (const auto& t : s.typos)
  {
    const Enemy& e = mEnemies[std::size_t(t.enemy)];
    if (!e.alive)
      continue;
    const float x = px(e.x, camX), y = px(e.y - 1, camY);
    r.fillRect(x, y, 2.0f * kCellPx, 2.0f * kCellPx, rgb(40, 40, 70));
    const char* letters = "QZXJK";
    const char t2[2] = {letters[(e.timer / (t.tell > 0 ? 1 : 8)) % 5], 0};
    r.drawText(t2, x + kCellPx, y + 4.0f, {40.0f, rgb(120, 255, 120), 0}, Align::Center);
  }
  for (const auto& d : s.deadlines)
  {
    const Enemy& e = mEnemies[std::size_t(d.enemy)];
    if (!e.alive)
      continue;
    const float x = px(e.x, camX), y = px(e.y - e.h + 1, camY);
    r.fillRect(x, y, float(e.w) * kCellPx, float(e.h - 1) * kCellPx, e.tell > 0 && frame % 4 < 2 ? rgb(255, 220, 90) : rgb(240, 235, 220));
    const float cx = x + float(e.w) * kCellPx * 0.5f, cy = y + float(e.h - 1) * kCellPx * 0.5f;
    const float ang = float(d.lap) / float(kWrDeadlineLap) * 6.2832f;
    r.drawLine(cx, cy, cx + std::sin(ang) * 50.0f, cy - std::cos(ang) * 50.0f, 5.0f, kInk);
    r.drawLine(cx, cy, cx + std::sin(float(d.step) * 1.2f) * 30.0f, cy - std::cos(float(d.step) * 1.2f) * 30.0f, 7.0f, kRedInk);
    r.fillRect(x + 8.0f, y + float(e.h - 1) * kCellPx, 8.0f, kCellPx, kInk);
    r.fillRect(x + float(e.w) * kCellPx - 16.0f, y + float(e.h - 1) * kCellPx, 8.0f, kCellPx, kInk);
  }
  for (int g : s.guards)
  {
    const Enemy& e = mEnemies[std::size_t(g)];
    if (!e.alive)
      continue;
    const float x = px(e.x, camX), y = px(e.y - e.h + 1, camY);
    r.drawLine(x + kCellPx * 1.5f, y + kCellPx, x + kCellPx * 1.5f, y + float(e.h) * kCellPx, 6.0f, kPencil);
    r.fillRect(x + kCellPx * 0.5f, y, kCellPx * 2.0f, kCellPx * 1.5f, rgb(250, 248, 240));
    const float lift = e.tell > 0 ? -kCellPx * 0.5f : 0.0f;
    r.drawLine(x + kCellPx * 1.5f, y + kCellPx * 2.5f + lift, x + kCellPx * (1.5f + 2.0f * float(e.dir)),
      y + kCellPx * 2.5f + lift, 6.0f, kPencil);
  }
  // The words, on top of everything they name.
  for (const auto& w : s.words)
  {
    if (w.target == WordTarget::Enemy && w.enemy >= 0 && !mEnemies[std::size_t(w.enemy)].alive && w.state != WordState::Striking)
      continue;
    if (w.target == WordTarget::Egg && s.egg.state != 0)
    {
      if (w.state == WordState::Striking)
        drawWord(r, w, camX, camY, int(w.text.size()), 1.0f, kInk);
      continue;
    }
    const int n = int(w.text.size());
    switch (w.state)
    {
      case WordState::Intact:
      case WordState::Waiting:
        if (w.target == WordTarget::Egg)
          box(r, {w.x - 1, w.y - 1, w.w + 2, 4}, camX, camY, rgb(255, 240, 120));
        drawWord(r, w, camX, camY, n, 1.0f, w.load ? kPencil : kInk);
        break;
      case WordState::Striking:
      {
        drawWord(r, w, camX, camY, n, 1.0f, kInk);
        const float f = std::min(1.0f, float(w.t) / float(kWrStrikeLine));
        r.drawLine(px(w.x, camX), px(w.y + 1, camY), px(w.x, camX) + float(w.w) * kCellPx * f, px(w.y + 1, camY) - 4.0f, 6.0f,
          kRedInk);
        break;
      }
      case WordState::Struck:
        drawWord(r, w, camX, camY, n, 0.35f, kInk);
        r.drawLine(px(w.x, camX), px(w.y + 1, camY), px(w.x + w.w, camX), px(w.y + 1, camY) - 4.0f, 6.0f, kRedInk);
        break;
      case WordState::Rewriting:
      {
        // Letter by letter, all back kWrRewriteShow frames before the target.
        const int shown = std::min(n, n * w.t / std::max(1, kWrRewrite - kWrRewriteShow));
        drawWord(r, w, camX, camY, n, 0.35f, kInk);
        drawWord(r, w, camX, camY, shown, 1.0f, kInk);
        break;
      }
      case WordState::Sketching:
      {
        const int shown = std::min(n, n * w.t / std::max(1, kWrResketch - 4));
        drawWord(r, w, camX, camY, shown, 1.0f, kPencil);
        break;
      }
    }
  }
  // The egg and the chick.
  if (s.egg.state >= 1 && s.egg.state <= 3)
  {
    const float x = s.egg.x * kCellPx - camX, y = s.egg.y * kCellPx - camY;
    if (s.egg.state < 3)
    {
      const float wob = s.egg.state == 2 ? std::sin(float(s.egg.t) * 0.8f) * 4.0f : 0.0f;
      r.fillRect(x + wob, y - kCellPx * 1.5f, kCellPx * 1.2f, kCellPx * 1.5f, rgb(255, 250, 235));
    }
    else
    {
      r.fillRect(x, y - kCellPx, kCellPx, kCellPx, rgb(255, 230, 60));
      if (s.egg.cheep > 0)
        r.drawText("CHEEP", x + kCellPx * 0.5f, y - kCellPx * 2.0f, {16.0f, kInk, 0}, Align::Center);
    }
  }
}

void World::drawWritersHud(Renderer& r, int frame) const
{
  const auto& s = mWriters;
  (void)frame;
  if (s.breakout)
  {
    char buf[64];
    std::snprintf(buf, sizeof buf, "BALLS %d   BRICKS %d", s.balls, s.broken);
    r.drawText(buf, float(kScreenW) * 0.5f, 60.0f, {28.0f, rgb(255, 250, 235), kHudInk}, Align::Center);
    return;
  }
  if (s.scramble > 0)
    r.drawText("SCROE", 40.0f, 96.0f, {28.0f, rgb(120, 255, 120), kHudInk}, Align::Left);
}

} // namespace gr
