// The level map: opened from a level (M, the gamepad's Back, or the touch
// pad's map button), it freezes the game like the pause menu and shows the
// part of the level the runners have explored so far. It opens zoomed in
// on the runner; the zoom button shows the whole level at once, and the
// stick, d-pad or arrows pan around.

#include "frontend/game.hpp"

#include "data/campaign.hpp"

#include <SDL.h>

#include <algorithm>
#include <cmath>

namespace gr
{

namespace
{

constexpr Color kInk = rgb(16, 12, 26);
// The map's window on the screen.
constexpr float kAreaX = 40.0f, kAreaY = 92.0f, kAreaW = 1200.0f, kAreaH = 548.0f;
// Zoomed in: pixels a block, and the blocks per baked chunk.
constexpr float kCloseScale = 24.0f;
constexpr int kChunk = 48;

} // namespace

float Game::mapFitScale() const
{
  if (!mLevel || mLevel->width <= 0 || mLevel->height <= 0)
    return kCloseScale;
  return std::min(kAreaW / float(mLevel->width), kAreaH / float(mLevel->height));
}

float Game::mapScale() const
{
  // Small levels fit whole at (nearly) the close-up size: one zoom only.
  const float fit = mapFitScale();
  return mMapClose && fit < kCloseScale * 0.8f ? kCloseScale : fit;
}

void Game::freeMap()
{
  mMapWhole = Texture{};
  mMapChunks.clear();
  mMapWholeBaked = mMapChunksBaked = false;
  mMapWorld = nullptr;
}

void Game::openMap()
{
  if (!mWorld)
    return;
  openMenu(Menu::Map);
  freeMap();
  const auto& p = mWorld->player();
  mMapX = (float(p.x) + 1.5f) / float(kCellsPerTile);
  mMapY = (float(p.y) - 2.0f) / float(kCellsPerTile);
  mMapPanTicks = 0;
}

void Game::tickMap(const Input& in, bool close, bool ok)
{
  if (close)
  {
    sound(Sfx::MenuMove);
    closeMenu();
    return;
  }
  if (ok && mapFitScale() < kCloseScale * 0.8f)
  {
    sound(Sfx::MenuSelect);
    mMapClose = !mMapClose;
  }
  // Held directions pan, faster the longer they are held.
  const int dx = (in.right ? 1 : 0) - (in.left ? 1 : 0);
  const int dy = (in.down ? 1 : 0) - (in.up ? 1 : 0);
  mMapPanTicks = (dx || dy) ? mMapPanTicks + 1 : 0;
  const float s = mapScale();
  const float speed = (6.0f + float(std::min(mMapPanTicks, 40)) * 0.5f) / s;
  mMapX += float(dx) * speed;
  mMapY += float(dy) * speed;
  // Keep the level in view: centred when it fits, else up to its edges.
  auto clampAxis = [s](float& c, int blocks, float view) {
    const float half = view * 0.5f / s;
    c = float(blocks) * s <= view ? float(blocks) * 0.5f : std::clamp(c, half, float(blocks) - half);
  };
  if (mLevel)
  {
    clampAxis(mMapX, mLevel->width, kAreaW);
    clampAxis(mMapY, mLevel->height, kAreaH);
  }
}

void Game::renderMap()
{
  auto& r = mRenderer;
  const auto& t = theme();
  r.fillRect(0, 0, float(kScreenW), float(kScreenH), rgba(4, 3, 14, 232));
  if (!mWorld || !mLevel)
    return;

  // Bake what this zoom needs (once per opening, again after a theme switch).
  if (mMapWorld != mWorld.get() || mMapTheme != mThemeIndex)
  {
    freeMap();
    mMapWorld = mWorld.get();
    mMapTheme = mThemeIndex;
  }
  const bool close = mapScale() != mapFitScale();
  const float s = mapScale();
  if (!close && !mMapWholeBaked)
  {
    mMapWhole = mWorld->bakeMap(r, s, 0, 0, mLevel->width, mLevel->height);
    mMapWholeBaked = true;
  }
  if (close && !mMapChunksBaked)
  {
    for (int by = 0; by < mLevel->height; by += kChunk)
      for (int bx = 0; bx < mLevel->width; bx += kChunk)
        if (Texture tex = mWorld->bakeMap(r, s, bx, by, kChunk, kChunk))
          mMapChunks.push_back({bx, by, std::move(tex)});
    mMapChunksBaked = true;
  }

  // The window, with the map clipped to it.
  r.fillRect(kAreaX - 3, kAreaY - 3, kAreaW + 6, kAreaH + 6, withAlpha(t.accentA, 150));
  r.fillRect(kAreaX, kAreaY, kAreaW, kAreaH, rgb(8, 8, 18));
  const float cx = kAreaX + kAreaW * 0.5f, cy = kAreaY + kAreaH * 0.5f;
  float ox, oy;
  if (close)
  {
    ox = cx - mMapX * s;
    oy = cy - mMapY * s;
  }
  else
  {
    ox = cx - float(mLevel->width) * s * 0.5f;
    oy = cy - float(mLevel->height) * s * 0.5f;
  }
  ox = std::round(ox);
  oy = std::round(oy);
  const SDL_Rect clip{int(kAreaX), int(kAreaY), int(kAreaW), int(kAreaH)};
  SDL_RenderSetClipRect(r.sdl(), &clip);
  if (close)
  {
    for (const auto& c : mMapChunks)
      r.draw(c.tex, ox + float(c.bx) * s, oy + float(c.by) * s);
  }
  else
  {
    r.draw(mMapWhole, ox, oy);
  }
  mWorld->drawMapMarks(r, ox, oy, s, mFrame);
  SDL_RenderSetClipRect(r.sdl(), nullptr);

  // Title, zoom and help.
  std::string title = mLevel->name.empty() ? "STAGE 1" : mLevel->name;
  if (mLevelNumber > 0 && (mMainWorld || mBonusOnly))
    title = "BONUS LEVEL";
  else if (mLevelNumber > 0)
    title = "LEVEL " + std::to_string(mLevelNumber) + "  -  " + campaignLevel(mLevelNumber).title;
  r.drawText("MAP", kAreaX, 26, {44.0f, t.accentA, kInk, true});
  r.drawText(title, cx, 38, {24.0f, t.hudText, kInk, true}, Align::Center);
  if (mapFitScale() < kCloseScale * 0.8f)
    r.drawText(close ? "ZOOM: CLOSE" : "ZOOM: WHOLE LEVEL", kAreaX + kAreaW, 40, {18.0f, t.accentB, kInk, true},
      Align::Right);

  auto padKeys = [this](Act a) {
    std::string s;
    for (int c : mBindings.pad[std::size_t(a)])
      s += (s.empty() ? "" : "/") + padName(c);
    return s;
  };
  std::string help = mOptions.touch ? "D-PAD pan   OK zoom   BACK close" : "ARROWS / STICK pan   ENTER / A zoom   ";
  if (!mOptions.touch)
  {
    std::string shut = keyName(mBindings.keys[std::size_t(Act::Map)][0]);
    if (shut == "-")
      shut.clear();
    const std::string pad = padKeys(Act::Map);
    shut += (shut.empty() ? "" : " / ") + std::string("ESC") + (pad.empty() ? "" : " / " + pad);
    help += shut + " close";
  }
  r.drawText(help, cx, 664, {17.0f, rgb(200, 198, 222), kInk}, Align::Center);
}

} // namespace gr
