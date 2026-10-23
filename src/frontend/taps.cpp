// Direct touch in the menus. Each screen marks what it draws as tap
// targets; a tap picks the target under it, which sets the screen's cursor
// and, where a tap should act, presses OK for the next tick. So the menus
// keep a single path for the keyboard, the gamepad and a finger. A drag
// turns into up/down (or left/right) steps, and pans the map.

#include "frontend/game.hpp"

#include <algorithm>
#include <cmath>

namespace gr
{

namespace
{

// Drag distance per step: about a list row, or most of a carousel card.
constexpr float kRowDrag = 46.0f;
constexpr float kCardDrag = 150.0f;

} // namespace

void Game::clearTapTargets()
{
  mTapTargets.clear();
  mNavigationTargets.clear();
  // A drag left over from another screen does not step this one.
  if (mTapScreen != tapScreen())
  {
    mDragX = mDragY = 0.0f;
    mDragStepsX = mDragStepsY = 0;
  }
  mTapScreen = tapScreen();
}

void Game::tapTarget(float x, float y, float w, float h, std::function<void(float, float)> act)
{
  mTapTargets.push_back({x, y, w, h, std::move(act)});
}

std::vector<SDL_FRect> Game::touchNavigationTargets() const
{
  return mTapScreen == tapScreen() ? mNavigationTargets : std::vector<SDL_FRect>{};
}

void Game::menuStepButtons(std::function<void(int)> step, bool previous, bool next)
{
  for (int side : {-1, 1})
  {
    const bool enabled = side < 0 ? previous : next;
    const SDL_FRect rect{side < 0 ? 24.0f : 1104.0f, 300, 152, 88};
    const Color color = enabled ? theme().accentA : rgb(100, 98, 120);
    mRenderer.fillRect(rect.x, rect.y, rect.w, rect.h, withAlpha(color, 90));
    mRenderer.drawText(side < 0 ? "< PREV" : "NEXT >", rect.x + rect.w * 0.5f, 326,
      {24.0f, color, rgb(16, 12, 26), true}, Align::Center);
    // Disabled buttons consume taps too: they must never press the pad below.
    mNavigationTargets.push_back(rect);
    tapTarget(rect.x, rect.y, rect.w, rect.h, [step, side, enabled](float, float) {
      if (enabled)
        step(side);
    });
  }
}

int Game::menuPage(int count, int& cursor, int rows)
{
  if (!mOptions.touch || count <= rows)
    return 0;
  const int first = cursor / rows * rows;
  menuStepButtons([this, &cursor, first, rows](int side) {
    cursor = first + side * rows;
    sound(Sfx::MenuMove);
  }, first > 0, first + rows < count);
  mRenderer.drawText(std::to_string(first / rows + 1) + " / " + std::to_string((count + rows - 1) / rows),
    1180, 408, {22.0f, theme().hudText, rgb(16, 12, 26)}, Align::Center);
  return first;
}

void Game::tapPress(bool Input::*button)
{
  mTapInput.*button = true;
}

void Game::tapAt(float x, float y)
{
  // The targets are from the last frame drawn; a tap meant for a screen
  // that has gone since must not act on the next one.
  if (mTapScreen != tapScreen())
    return;
  for (auto it = mTapTargets.rbegin(); it != mTapTargets.rend(); ++it)
  {
    if (x >= it->x && x < it->x + it->w && y >= it->y && y < it->y + it->h)
    {
      const auto act = it->act;
      act(x, y);
      return;
    }
  }
}

void Game::dragBy(float dx, float dy)
{
  if (mMenu == Menu::Map)
  {
    // The map follows the finger.
    const float s = mapScale();
    mMapX -= dx / s;
    mMapY -= dy / s;
    return;
  }
  // Like scrolling a list on a phone: a finger moving up brings what is
  // below into view (the cursor steps down), one moving left the next card.
  const bool across = (mMenu == Menu::None && mMode == Mode::Select) || mMenu == Menu::Runner;
  const bool along = !across && !(mMenu == Menu::None && mMode == Mode::Editor && wantsText()) &&
    (mMenu != Menu::None || mMode == Mode::Title || mMode == Mode::List || mMode == Mode::Editor);
  auto steps = [](float& acc, float d, float per, int& out) {
    acc -= d;
    while (acc >= per)
    {
      acc -= per;
      ++out;
    }
    while (acc <= -per)
    {
      acc += per;
      --out;
    }
  };
  if (across)
    steps(mDragX, dx, mMenu == Menu::Runner ? kRowDrag * 2.0f : kCardDrag, mDragStepsX);
  if (along)
    steps(mDragY, dy, kRowDrag, mDragStepsY);
}

Input Game::takeTapInput()
{
  Input in = mTapInput;
  mTapInput = Input{};
  // One drag step a tick, so each one is a press of its own.
  if (mDragStepsY != 0)
  {
    (mDragStepsY > 0 ? in.down : in.up) = true;
    mDragStepsY += mDragStepsY > 0 ? -1 : 1;
  }
  if (mDragStepsX != 0)
  {
    (mDragStepsX > 0 ? in.right : in.left) = true;
    mDragStepsX += mDragStepsX > 0 ? -1 : 1;
  }
  // A tap is a fresh press even when the same button was down last tick.
  for (bool Input::*f : {&Input::left, &Input::right, &Input::up, &Input::down, &Input::confirm, &Input::back})
    if (in.*f)
      mPrev.*f = false;
  return in;
}

bool Game::runnerOnScreen(float& x, float& y) const
{
  if (!inLevel() || !mWorld)
    return false;
  const auto& p = mWorld->player();
  const auto& cam = mWorld->camera();
  const float cell = float(kCellSize) * kPixelScale;
  // The player's position is its bottom-left cell.
  x = (float(p.x) + float(Player::kWidth) * 0.5f) * cell - cam.renderX() * kPixelScale;
  y = (float(p.y) + 1.0f - float(p.height()) * 0.5f) * cell - cam.renderY() * kPixelScale;
  return x > -cell && x < float(kScreenW) + cell && y > -cell && y < float(kScreenH) + cell;
}

int Game::vehicleOnScreen(SDL_FRect& bounds) const
{
  if (!inLevel() || !mWorld || mWorld->state() != WorldState::Playing || !mWorld->riding())
    return -1;
  const auto& v = *mWorld->riding();
  const float alpha = renderAlpha();
  const float cell = float(kCellSize) * kPixelScale;
  const auto& cam = mWorld->camera();
  bounds = {(float(v.prevX) + float(v.x - v.prevX) * alpha) * cell - cam.renderX() * kPixelScale,
    (float(v.prevY) + float(v.y - v.prevY) * alpha + 1 - float(v.h)) * cell - cam.renderY() * kPixelScale,
    float(v.w) * cell, float(v.h) * cell};
  return bounds.x + bounds.w > 0 && bounds.x < kScreenW && bounds.y + bounds.h > 0 && bounds.y < kScreenH
    ? mWorld->player().vehicle : -1;
}

bool Game::menuPad() const
{
  auto it = mProfile.controls.find("touch.menupad");
  return it != mProfile.controls.end() && !it->second.empty() && it->second[0] != 0;
}

bool Game::touchThumbs() const
{
  auto it = mProfile.controls.find("touch.thumbs");
  return it == mProfile.controls.end() || it->second.empty() || it->second[0] != 0;
}

void Game::setMenuPad(bool on)
{
  if (menuPad() == on)
    return;
  mProfile.controls["touch.menupad"] = {on ? 1 : 0};
  saveProfile();
}

} // namespace gr
