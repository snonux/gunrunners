// Level 41, Sweeps Week: the crowd, the vote screens over the checkpoints,
// turntables and their arrows, rubber chickens, cue cards, party hats,
// beaks and big heads, LIGHTS OUT and MIRROR MODE, the ballot and the
// Applause Cannon's meter on the HUD; Paintball's paint. Placeholder
// shapes until the art pass.

#include "game/world.hpp"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace gr
{

namespace
{

constexpr float kCellPx = float(kCellSize) * kPixelScale; // 32

int sgn(int v) { return (v > 0) - (v < 0); }

void box(Renderer& r, const CellBox& b, float camX, float camY, Color c, Blend bl = Blend::Alpha)
{
  r.fillRect(float(b.x) * kCellPx - camX, float(b.y) * kCellPx - camY, float(b.w) * kCellPx, float(b.h) * kCellPx, c, bl);
}

Color catColor(SwCat c)
{
  switch (c)
  {
    case SwCat::Friendly:
      return rgb(90, 220, 120);
    case SwCat::Neutral:
      return rgb(120, 180, 255);
    case SwCat::Nasty:
      return rgb(255, 80, 80);
    case SwCat::Secret:
      return rgb(230, 120, 255);
    case SwCat::Fanfare:
      return rgb(255, 210, 60);
  }
  return rgb(255, 255, 255);
}

Color runnerPaint(int character)
{
  static const Color kPaints[] = {rgb(60, 140, 255), rgb(255, 140, 40), rgb(255, 90, 190),
    rgb(80, 220, 120), rgb(80, 230, 240), rgb(200, 200, 215)};
  return character >= 0 && character < 6 ? kPaints[character] : rgb(255, 230, 80);
}

// The needle's position over the three options (0..2) during the swing.
float needleAt(const SweepsState& s)
{
  if (s.ballotT >= kSwSwing)
    return float(s.winner);
  const float t = float(s.ballotT) / float(kSwSwing);
  const float wobble = std::sin(float(s.ballotT) * 0.35f) * (1.0f - t) * 1.2f;
  return std::clamp(1.0f + (float(s.winner) - 1.0f) * t + wobble, 0.0f, 2.0f);
}

// The screen grabbed, flipped left to right and put back (MIRROR MODE).
void mirrorView(Renderer& r)
{
  SDL_Renderer* sdl = r.sdl();
  SDL_Rect vp;
  SDL_RenderGetViewport(sdl, &vp);
  float sx = 1.0f, sy = 1.0f;
  SDL_RenderGetScale(sdl, &sx, &sy);
  const int w = int(std::lround(float(vp.w) * sx)), h = int(std::lround(float(vp.h) * sy));
  if (w <= 0 || h <= 0)
    return;
  std::vector<Uint32> pixels(std::size_t(w) * std::size_t(h));
  if (SDL_RenderReadPixels(sdl, nullptr, SDL_PIXELFORMAT_ARGB8888, pixels.data(), w * 4) != 0)
    return;
  SDL_Texture* tex = SDL_CreateTexture(sdl, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
  if (!tex)
    return;
  SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_NONE);
  SDL_UpdateTexture(tex, nullptr, pixels.data(), w * 4);
  const SDL_Rect dst{0, 0, vp.w, vp.h};
  SDL_RenderCopyEx(sdl, tex, nullptr, &dst, 0.0, nullptr, SDL_FLIP_HORIZONTAL);
  SDL_DestroyTexture(tex);
}

} // namespace

void World::drawSweepsBack(Renderer& r, float camX, float camY, int frame, float /*alpha*/) const
{
  const auto& s = mSweeps;
  const auto& p = mPlayer;
  if (s.paint)
    return;
  // The vote screens over the checkpoints: idle, the ballot, the winner.
  for (std::size_t i = 0; i < s.votes.size(); ++i)
  {
    const auto& v = s.votes[i];
    const CellBox scr{v.x - 14, v.y - 16, 32, 8};
    if (!isOnScreen(scr, 4))
      continue;
    box(r, {scr.x - 1, scr.y - 1, scr.w + 2, scr.h + 2}, camX, camY, rgb(40, 30, 20));
    box(r, scr, camX, camY, rgb(14, 16, 30));
    const float cx = (float(scr.x) + float(scr.w) * 0.5f) * kCellPx - camX, ty = float(scr.y) * kCellPx - camY;
    if (s.ballot == int(i))
    {
      // The options are on the HUD's ballot; the screen shows the meter.
      const bool settled = s.ballotT >= kSwSwing;
      r.drawText(settled ? "THE AUDIENCE HAS SPOKEN" : "VOTE NOW!", cx, ty + 60.0f,
                 {36.0f, settled ? catColor(s.options[s.winner].cat) : rgb(255, 230, 80), rgb(0, 0, 0)}, Align::Center);
      for (int k = 0; k < 3; ++k)
        r.fillRect(cx + float(k - 1) * 200.0f - 80.0f, ty + 120.0f, 160.0f, 24.0f,
                   withAlpha(catColor(s.options[k].cat), settled && k == s.winner ? 230 : 90));
      const float nx = cx + (needleAt(s) - 1.0f) * 200.0f;
      r.drawLine(cx, ty + 240.0f, nx, ty + 150.0f, 8.0f, rgb(255, 230, 80));
    }
    else
    {
      const bool done = v.done;
      r.drawText(done ? "THANK YOU!" : (v.fanfare ? "ENCORE VOTE" : "AUDIENCE VOTE"), cx, ty + 90.0f,
                 {40.0f, (frame / 8) % 2 || done ? rgb(255, 210, 60) : rgb(255, 120, 60), rgb(0, 0, 0)}, Align::Center);
    }
  }
  // The crowd: clapping, the cameos turning their heads after the runner.
  for (const auto& c : s.crowd)
  {
    const CellBox b{c.x, c.y - 4, 3, 5};
    if (!isOnScreen(b, 2))
      continue;
    static const Color kBody[] = {rgb(150, 60, 160), rgb(190, 200, 215), rgb(30, 30, 36), rgb(230, 205, 170),
      rgb(120, 80, 40)};
    const float x = float(c.x) * kCellPx - camX, y = float(c.y + 1) * kCellPx - camY;
    r.fillRect(x + 8.0f, y - 100.0f, 80.0f, 100.0f, kBody[c.look]);
    r.fillRect(x + 22.0f, y - 150.0f, 52.0f, 52.0f, c.look == 3 ? rgb(240, 225, 200) : rgb(230, 180, 140));
    const int look = sgn(p.x + 1 - (c.x + 1));
    if (c.look != 0)
      r.fillRect(x + 42.0f + float(look) * 12.0f, y - 132.0f, 10.0f, 10.0f, rgb(20, 20, 20));
    if (c.look != 3)
    {
      // Clapping (the Mannequin never claps).
      const float clap = (frame / 4 + c.x) % 2 ? 4.0f : 20.0f;
      r.fillRect(x + 48.0f - clap - 10.0f, y - 80.0f, 10.0f, 16.0f, rgb(230, 180, 140));
      r.fillRect(x + 48.0f + clap, y - 80.0f, 10.0f, 16.0f, rgb(230, 180, 140));
    }
    if (c.look == 1)
      r.fillRect(x + 70.0f, y - 70.0f, 24.0f, 30.0f, rgb(230, 40, 40)); // the popcorn
    if (c.look == 2)
      r.fillRect(x + 84.0f, y - 170.0f, 22.0f, 70.0f, rgb(60, 140, 255)); // the foam finger
  }
  // Turntables: chase arrows the way they turn, lit before each turn.
  for (const auto& t : s.turntables)
  {
    const CellBox b{t.x0 * kCellsPerTile, t.row * kCellsPerTile, (t.x1 - t.x0 + 1) * kCellsPerTile, 1};
    if (!isOnScreen(b, 2))
      continue;
    const int dir = t.dirAt(s.clock);
    const bool warn = kSwTurnFlip - s.clock % kSwTurnFlip <= kSwTurnWarn;
    box(r, b, camX, camY, rgb(60, 50, 70));
    for (int x = b.x; x < b.x + b.w; x += 4)
    {
      const int off = ((s.clock / 2) * dir % 4 + 4) % 4;
      const float ax = float(x + off) * kCellPx - camX, ay = float(b.y) * kCellPx - camY + 8.0f;
      const Color c = warn && (frame / 3) % 2 ? rgb(255, 80, 60) : rgb(255, 210, 80);
      r.drawLine(ax, ay, ax + float(dir) * 16.0f, ay + 8.0f, 4.0f, c);
      r.drawLine(ax, ay + 16.0f, ax + float(dir) * 16.0f, ay + 8.0f, 4.0f, c);
    }
  }
}

void World::drawSweepsFront(Renderer& r, float camX, float camY, int frame, float /*alpha*/) const
{
  const auto& s = mSweeps;
  const auto& p = mPlayer;
  if (s.paint)
  {
    // Paint on every block the runner (or a shot) touched.
    const int w = mLevel->width;
    const Color paint = runnerPaint(mCharacterIndex);
    const int bx0 = std::max(0, int(camX / (2.0f * kCellPx)) - 1), by0 = std::max(0, int(camY / (2.0f * kCellPx)) - 1);
    for (int by = by0; by < std::min(mLevel->height, by0 + 14); ++by)
      for (int bx = bx0; bx < std::min(w, bx0 + 23); ++bx)
      {
        const int c = s.paints[std::size_t(by * w + bx)];
        const CellBox b{bx * kCellsPerTile, by * kCellsPerTile, kCellsPerTile, kCellsPerTile};
        if (c == 2 || c == 4)
          box(r, b, camX, camY, withAlpha(paint, 170));
        else if (c == 1)
          box(r, {b.x, b.y, b.w, 1}, camX, camY, withAlpha(rgb(255, 255, 255), 90 + 60 * ((frame / 8) % 2)));
      }
    for (const auto& pad : s.pads)
      box(r, {pad.x0 * kCellsPerTile, pad.row * kCellsPerTile - 1, (pad.x1 - pad.x0 + 1) * kCellsPerTile, 1}, camX, camY,
          rgb(255, 230, 60));
    return;
  }
  const bool big = (s.rules & kSwBigHeads) != 0, quack = (s.rules & kSwQuack) != 0;
  for (const auto& a : s.actors)
  {
    const Enemy& e = mEnemies[std::size_t(a.enemy)];
    if (!e.alive || !isOnScreen(e.box(), 6))
      continue;
    const float ex = (float(e.x) + float(e.w) * 0.5f) * kCellPx - camX, top = float(e.y - e.h + 1) * kCellPx - camY;
    if (big)
      r.fillRect(ex - 60.0f, top - 70.0f, 120.0f, 90.0f, withAlpha(rgb(240, 200, 160), 200));
    if (a.hat)
    {
      const Color hat = (a.enemy % 2) ? rgb(255, 60, 160) : rgb(80, 220, 255);
      for (int k = 0; k < 5; ++k)
        r.fillRect(ex - 25.0f + float(k) * 5.0f, top - 20.0f - float(k) * 12.0f, 50.0f - float(k) * 10.0f, 12.0f, hat);
    }
    if (quack)
      r.fillRect(ex + float(e.dir) * 30.0f - 12.0f, top + 30.0f, 24.0f, 12.0f, rgb(255, 160, 30));
    if (a.popper)
      r.drawText("POP!", ex, top - 30.0f, {22.0f, rgb(255, 230, 80), rgb(0, 0, 0)}, Align::Center);
    if (e.kind == EnemyKind::CueDrone)
    {
      static const char* const kCards[] = {"BOO!", "THROW THINGS!", "VOTE NASTY"};
      const bool flipping = a.flip > 0 && (frame / 2) % 2;
      r.fillRect(ex - 70.0f, top + 96.0f, 140.0f, 34.0f, flipping ? rgb(200, 200, 200) : rgb(250, 250, 240));
      if (!flipping)
        r.drawText(kCards[a.card % 3], ex, top + 100.0f, {18.0f, rgb(20, 20, 20), 0}, Align::Center);
    }
    if (e.kind == EnemyKind::AudiencePlant)
    {
      if (a.stand == 0)
        r.fillRect(ex - 50.0f, top + 90.0f, 100.0f, 70.0f, rgb(120, 30, 40)); // the seat, in front of him
      else if (e.tell > 0)
        r.drawText("!", ex, top - 40.0f, {40.0f, rgb(255, 80, 60), rgb(0, 0, 0)}, Align::Center);
    }
    if (e.kind == EnemyKind::WarmupComic && e.tell > 0)
      r.drawText("BA-DUM-TSS", ex, top - 40.0f, {20.0f, rgb(255, 230, 120), rgb(0, 0, 0)}, Align::Center);
  }
  if (big && p.state != PlayerState::Dying)
  {
    const float px = (float(p.x) + 1.5f) * kCellPx - camX, py = float(p.y - p.height() + 1) * kCellPx - camY;
    r.fillRect(px - 30.0f, py - 34.0f, 60.0f, 50.0f, withAlpha(rgb(240, 200, 160), 170));
  }
  // Rubber chickens.
  for (const auto& c : s.chickens)
  {
    const float x = c.x * kCellPx - camX, y = c.y * kCellPx - camY;
    r.fillRect(x + 8.0f, y + 20.0f, 48.0f, 30.0f, rgb(250, 240, 200));
    r.fillRect(x + (c.vx >= 0.0f ? 44.0f : 4.0f), y + 4.0f, 16.0f, 20.0f, rgb(250, 240, 200));
    r.fillRect(x + (c.vx >= 0.0f ? 48.0f : 8.0f), y, 8.0f, 6.0f, rgb(230, 40, 40));
  }
  // The rubber duck's sequined bow tie.
  for (const auto& it : mItems)
    if (!it.taken && it.kind == ItemKind::Duck)
    {
      const float x = (float(it.x) + 1.0f) * kCellPx - camX, y = (float(it.y) - 0.3f) * kCellPx - camY;
      const Color tie = (frame / 6) % 2 == 0 ? rgb(255, 60, 120) : rgb(255, 210, 60);
      r.fillRect(x - 14.0f, y - 6.0f, 12.0f, 12.0f, tie);
      r.fillRect(x + 2.0f, y - 6.0f, 12.0f, 12.0f, tie);
      r.fillRect(x - 3.0f, y - 3.0f, 6.0f, 6.0f, rgb(255, 245, 220));
    }
  // The V of VIRUS FOR THE RUNNER glows.
  for (const auto& it : mItems)
    if (!it.taken && it.kind == ItemKind::Virus && it.variant == 7)
      r.fillRect((float(it.x) - 1.0f) * kCellPx - camX, (float(it.y) - 2.0f) * kCellPx - camY, 4.0f * kCellPx,
                 4.0f * kCellPx, withAlpha(rgb(120, 255, 60), 60 + 40 * ((frame / 4) % 2)), Blend::Add);
}

void World::drawSweepsTop(Renderer& r, float camX, float camY, int /*frame*/) const
{
  const auto& s = mSweeps;
  const auto& p = mPlayer;
  if ((s.rules & kSwLightsOut) != 0)
  {
    // LIGHTS OUT: dark but for the runner's own light.
    constexpr int cols = 41, rows = 24;
    std::vector<float> a(std::size_t(cols * rows));
    const float px = (float(p.x) + 1.5f) * kCellPx - camX, py = (float(p.y) - 2.0f) * kCellPx - camY;
    const float rad = float(kSwLight) * kCellPx;
    for (int y = 0; y < rows; ++y)
      for (int x = 0; x < cols; ++x)
      {
        const float dx = float(x) * kCellPx - px, dy = float(y) * kCellPx - py;
        const float d = std::sqrt(dx * dx + dy * dy) / rad;
        a[std::size_t(y * cols + x)] = std::clamp((d - 0.7f) / 0.3f, 0.0f, 1.0f) * 0.94f;
      }
    r.drawAlphaGrid(a, cols, rows, 0.0f, 0.0f, kCellPx, kCellPx, rgb(0, 0, 0));
  }
  if (sweepsMirrored())
    mirrorView(r);
}

void World::drawSweepsHud(Renderer& r, int frame) const
{
  const auto& s = mSweeps;
  char buf[96];
  if (s.paint)
  {
    const int pct = s.tops > 0 ? s.painted * 100 / s.tops : 0;
    std::snprintf(buf, sizeof(buf), "PAINTED %d%% OF %d%%", pct, s.goal);
    r.drawText(buf, float(kScreenW) - 24.0f, 110.0f, {30.0f, runnerPaint(mCharacterIndex), rgb(0, 0, 0)}, Align::Right);
    return;
  }
  // The Applause Cannon's meter.
  const float mx = float(kScreenW) - 260.0f, my = 96.0f;
  r.fillRect(mx - 10.0f, my - 6.0f, 250.0f, 44.0f, withAlpha(rgb(10, 10, 16), 190));
  std::snprintf(buf, sizeof(buf), "APPLAUSE x%d", s.meter);
  r.drawText(buf, mx, my, {22.0f, s.meter >= 4 && (frame / 4) % 2 ? rgb(255, 255, 255) : rgb(255, 210, 60), 0});
  for (int k = 0; k < kSwMeterMax; ++k)
    r.fillRect(mx + 160.0f + float(k) * 18.0f, my + 2.0f, 14.0f, 26.0f, k < s.meter ? rgb(255, 210, 60) : rgb(70, 60, 40));
  // The rule in force.
  if (s.ballot < 0 && s.rules != 0)
  {
    SwOption o;
    o.rules = s.rules;
    r.drawText("RULE: " + sweepsOptionName(o), float(kScreenW) * 0.5f, 140.0f, {22.0f, rgb(255, 230, 140), rgb(0, 0, 0)},
               Align::Center);
  }
  if (s.ballot < 0)
    return;
  // The ballot: three options, the applause meter's needle swinging over them.
  const float y = 130.0f, cx = float(kScreenW) * 0.5f;
  r.fillRect(cx - 520.0f, y - 10.0f, 1040.0f, 150.0f, withAlpha(rgb(10, 8, 20), 215));
  if (s.options[0].cat == SwCat::Fanfare)
    std::snprintf(buf, sizeof(buf), "ENCORE! THE AUDIENCE PICKS YOUR EXIT FANFARE");
  else
    std::snprintf(buf, sizeof(buf), "THE AUDIENCE VOTES - STYLE %d", s.style);
  r.drawText(buf, cx, y - 4.0f, {22.0f, rgb(255, 255, 255), 0}, Align::Center);
  for (int k = 0; k < 3; ++k)
  {
    const float ox = cx + float(k - 1) * 340.0f;
    const bool won = s.ballotT >= kSwSwing && k == s.winner;
    const Color c = catColor(s.options[k].cat);
    r.fillRect(ox - 160.0f, y + 26.0f, 320.0f, 56.0f, withAlpha(c, won && (frame / 3) % 2 ? 230 : 90));
    r.drawText(sweepsOptionName(s.options[k]), ox, y + 40.0f, {20.0f, rgb(255, 255, 255), rgb(0, 0, 0)}, Align::Center);
  }
  const float nx = cx + (needleAt(s) - 1.0f) * 340.0f;
  r.fillRect(nx - 6.0f, y + 88.0f, 12.0f, 40.0f, rgb(255, 230, 80));
  r.drawLine(cx, y + 136.0f, nx, y + 92.0f, 5.0f, rgb(255, 230, 80));
}

} // namespace gr
