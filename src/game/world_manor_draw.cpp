// Level 23, Fright Night Manor: drawing (the side-only walls and floors,
// mirrors, levers, portraits, the coffin, lightning, the shimmer of a step
// through a mirror) and the Both Sides split screen. See world_manor.cpp for
// the logic.

#include "game/world.hpp"

#include <SDL.h>

#include <algorithm>
#include <cmath>

namespace gr
{

namespace
{

constexpr float kCellPx = float(kCellSize) * kPixelScale; // 32
constexpr float kTilePx = float(kTileSize) * kPixelScale; // 64

float lerpCells(int prev, int cur, float alpha)
{
  return (float(prev) + float(cur - prev) * alpha) * kCellPx;
}

} // namespace

void World::drawManorBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)alpha;
  const auto& m = mManor;
  // Side-only walls and floors: solid on their side, a ghost outline on the
  // other.
  for (const auto& sl : m.layers)
  {
    const auto& l = mLayers[std::size_t(sl.layer)];
    const float x = float(l.x0) * kTilePx - camX, y = float(l.y0) * kTilePx - camY;
    const float w = float(l.x1 - l.x0 + 1) * kTilePx, h = float(l.y1 - l.y0 + 1) * kTilePx;
    if (x > float(kScreenW) + 64.0f || x + w < -64.0f || y > float(kScreenH) + 64.0f || y + h < -64.0f)
      continue;
    const Color c = l.tile == Tile::Platform ? mTheme.platform : mTheme.rock;
    if (l.solid)
      r.fillRect(x, y, w, l.tile == Tile::Platform ? 18.0f : h, c);
    else
    {
      const Color g = withAlpha(mTheme.trimGlow, 70);
      r.fillRect(x, y, w, 3.0f, g);
      r.fillRect(x, y + h - 3.0f, w, 3.0f, g);
      r.fillRect(x, y, 3.0f, h, g);
      r.fillRect(x + w - 3.0f, y, 3.0f, h, g);
    }
  }
  // Mirrors: a silver frame with the other side's shimmer.
  for (const auto& mi : m.mirrors)
  {
    const float x = float(mi.x) * kTilePx - camX, y = float(mi.y) * kTilePx - camY;
    if (x > float(kScreenW) + 64.0f || x + 128.0f < -64.0f)
      continue;
    const Color frameC = mi.kind == MirrorKind::Broken ? rgb(120, 120, 130) : rgb(210, 215, 230);
    r.fillRect(x + 8.0f, y + 4.0f, 112.0f, 188.0f, frameC);
    const float s = 0.5f + 0.5f * std::sin(float(frame) * 0.1f + float(mi.x));
    const Color glass = mi.third > 0 ? rgb(40, 255, 90)
      : (m.side == kSideReal ? rgb(60 + int(40 * s), 200, 120) : rgb(150 + int(40 * s), 80, 200));
    r.fillRect(x + 16.0f, y + 12.0f, 96.0f, 172.0f, withAlpha(glass, 160));
  }
  // Levers.
  for (const auto& l : m.levers)
  {
    const float x = float(l.x) * kTilePx - camX, y = float(l.y) * kTilePx - camY;
    r.fillRect(x + 24.0f, y + 16.0f, 16.0f, 48.0f, rgb(90, 80, 70));
    r.fillRect(l.state ? x + 32.0f : x + 8.0f, y + 4.0f, 24.0f, 12.0f, l.state ? rgb(80, 220, 120) : rgb(220, 80, 80));
  }
  // Portraits (the ghosts' frames).
  for (const auto& pt : m.portraits)
  {
    const float x = float(pt.x) * kTilePx - camX, y = float(pt.y) * kTilePx - camY - 64.0f;
    r.fillRect(x, y, 128.0f, 128.0f, rgb(120, 90, 40));
    r.fillRect(x + 12.0f, y + 12.0f, 104.0f, 104.0f, rgb(40, 30, 50));
  }
  // Lance's portrait (Secret 2), on its side.
  if (m.lanceX >= 0 && !offSide(m.lanceSide))
  {
    const float x = float(m.lanceX) * kTilePx - camX, y = float(m.lanceY) * kTilePx - camY;
    r.fillRect(x, y, 128.0f, 128.0f, rgb(160, 120, 50));
    r.fillRect(x + 12.0f, y + 12.0f, 104.0f, 104.0f, m.lanceShot ? rgb(120, 30, 40) : rgb(60, 50, 70));
  }
  // The coffin.
  if (m.coffinX >= 0)
  {
    const float x = float(m.coffinX) * kTilePx - camX, y = float(m.coffinY) * kTilePx - camY;
    const float lift = m.coffin > 0 ? std::min(1.0f, float(40 - m.coffin) / 6.0f) * 32.0f : 0.0f;
    r.fillRect(x, y + 16.0f, float(m.coffinW) * kTilePx, 112.0f, rgb(70, 40, 30));
    r.fillRect(x - 8.0f, y + 8.0f - lift, float(m.coffinW) * kTilePx + 16.0f, 16.0f, rgb(100, 60, 40));
  }
}

void World::drawManorFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)camX;
  (void)camY;
  (void)frame;
  (void)alpha;
  const auto& m = mManor;
  if (m.flip > 0)
  {
    const float t = 1.0f - std::abs(float(m.flip) - kMirrorShimmer / 2.0f) / (kMirrorShimmer / 2.0f);
    r.fillRect(0.0f, 0.0f, float(kScreenW), float(kScreenH), withAlpha(rgb(220, 240, 255), int(200 * t)));
  }
  if (m.flash > 0)
    r.fillRect(0.0f, 0.0f, float(kScreenW), float(kScreenH), withAlpha(rgb(255, 255, 255), 60 * m.flash));
}

void World::drawManorHud(Renderer& r, int frame) const
{
  (void)r;
  (void)frame;
}

// Both Sides: the top runner in the top half of the screen, the bottom one
// in the bottom half, each with its own camera; one HUD over both.
void World::drawSplit(Renderer& r, int frame, float alpha) const
{
  const auto& s = mManor.split;
  const float halfH = float(kScreenH) / 2.0f;
  auto camFor = [&](const Player& p, int rowTop, int rowBottom, float& cx, float& cy) {
    const float mapW = float(mMap.width()) * kCellPx;
    cx = std::clamp(lerpCells(p.prevX, p.x, alpha) + 48.0f - float(kScreenW) / 2.0f, 0.0f, std::max(0.0f, mapW - float(kScreenW)));
    const float top = float(rowTop) * kTilePx, bottom = float(rowBottom + 1) * kTilePx;
    cy = std::clamp(lerpCells(p.prevY, p.y, alpha) - halfH * 0.6f, top, std::max(top, bottom - halfH));
  };
  SDL_Renderer* sdl = r.sdl();
  // Top half.
  SDL_Rect vp{0, 0, kScreenW, int(halfH)};
  SDL_RenderSetViewport(sdl, &vp);
  mSplitPass = 1;
  camFor(mPlayer, 0, s.divider - 1, mSplitCamX, mSplitCamY);
  draw(r, frame, alpha);
  // Bottom half: the twin drawn as the runner.
  vp = {0, int(halfH), kScreenW, int(halfH)};
  SDL_RenderSetViewport(sdl, &vp);
  mSplitPass = 2;
  auto& self = const_cast<World&>(*this);
  std::swap(self.mPlayer, self.mTwin);
  camFor(mPlayer, s.divider + 2, mLevel->height - 1, mSplitCamX, mSplitCamY);
  draw(r, frame, alpha);
  std::swap(self.mPlayer, self.mTwin);
  mSplitPass = 0;
  SDL_RenderSetViewport(sdl, nullptr);
  r.fillRect(0.0f, halfH - 3.0f, float(kScreenW), 6.0f, rgb(200, 210, 240));
  drawHud(r, frame);
}

} // namespace gr
